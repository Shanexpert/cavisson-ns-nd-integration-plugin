/******************************************************************
 * Name    :    ns_log.c
 * Author  :    Anuj Dhiman
 * Purpose :    This file contains error and debug log methods
 * Modification History:
 * Note:
 *
 *
 * 04/02/08:  Anuj - Initial Version
*****************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <regex.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "nslb_util.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"

#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "user_tables.h"
#include "ns_error_codes.h"
#include <string.h>
#include <time.h>
#include <ctype.h>

#include "ns_server.h"
#include "util.h"
#include "tmr.h"
#include "timing.h"
#include "ns_gdf.h"
#include "ns_log.h"
#include "ns_event_log.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "nslb_hessian.h"
#include "netstorm.h"
#include "nslb_time_stamp.h"
#include "nslb_alloc.h"
#include "nslb_log.h"
#include "nslb_sock.h"
#include "ns_exit.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"
#include "ns_handle_alert.h"
#include "ns_file_upload.h"
#include "nslb_cav_conf.h"

#define DEFAULT_MAX_DEBUG_FILE_SIZE 100000000
#define DEFAULT_MAX_ERROR_FILE_SIZE 10000000

#define DEFAULT_DEBUG_TEST_SETTINGS_VUERS 10
#define DEFAULT_DEBUG_TEST_SETTINGS_SESSION_RATE 60 
#define DEFAULT_DEBUG_TEST_SETTINGS_DURATION 300
#define DEFAULT_DEBUG_TEST_SETTINGS_SESSION 100

static unsigned int max_debug_log_file_size = DEFAULT_MAX_DEBUG_FILE_SIZE;  // Approx 100MB
unsigned int max_error_log_file_size        = DEFAULT_MAX_ERROR_FILE_SIZE;  // Approx 10 MB // used in netstorm.c

int error_fd = -1;
int debug_fd = -1;
int debug_log_value = 0;// This is to keep integer value of debug level
int sess_warning_fd = -1;
// This mthd will create the error.log file in the append_mode

/********************** Never Use Trace_log OR Debug_log from here **************************/
void open_log(char *name, int *fd, unsigned int max_size, char *header)
{
  struct stat stat_buf;
  char log_file[1024], log_file_prev[1024];
  int ret = 0;

  if(!strcmp(name, "debug.log")) //TODO Krishna: must to it while calling method 
    sprintf(log_file, "%s/logs/TR%d/%s", g_ns_wdir, testidx, name);
  else if(!strcmp(name, "ns_trace.log")) //for continuous monitoring
    sprintf(log_file, "%s/logs/%s/ns_logs/%s", g_ns_wdir, global_settings->tr_or_partition, name);
  else //for continuous monitoring
    sprintf(log_file, "%s/logs/%s/%s", g_ns_wdir, global_settings->tr_or_partition, name);
    
  //If above log file opened by this NVM then check whether file size is greater than provided max_size or not? 
  if((ret = nslb_open_log(fd, log_file, max_size, header)) < 0)
    NS_EXIT(-1, "Error: Error in opening file '%s', Error = '%s'", log_file, nslb_get_error());

  //new log file opened
  if (ret == 1)
  {
    if(!strcmp(name, "ns_trace.log")) {
      snprintf(log_file_prev, 1024, "%s/logs/TR%d/ns_logs", g_ns_wdir, testidx);
      if(stat(log_file_prev, &stat_buf)) // if ns_logs not present
        mkdir(log_file_prev, 0755);
      else if(S_ISREG(stat_buf.st_mode)) // if ns_logs file present
      {
        unlink(log_file_prev);           // remove ns_logs file first
        mkdir(log_file_prev, 0755);      //create ns_logs directory
      }
      if(global_settings->alert_info)
        ns_alert_config();
/*    File upload configuration has been done already so no need.
      if(global_settings->monitor_type)
      {
        ns_config_file_upload(global_settings->file_upload_info->server_ip, global_settings->file_upload_info->server_port,
                              global_settings->file_upload_info->protocol, global_settings->file_upload_info->url,
                              global_settings->file_upload_info->max_conn_retry, global_settings->file_upload_info->retry_timer, ns_event_fd);
      } */
    }
  }
}
//--------------------------------------------//
//void ns_debug_log_ex(int log_level, unsigned int mask, VUser *vptr, connection *cptr, char *file, int line, char *fname, char *format, ...)
//void ns_debug_log_ex(int log_level, unsigned int mask, char *file, int line, char *fname, VUser *vptr, connection *cptr, char *format, ...)
void ns_debug_log_ex(int log_level, unsigned long long mask, char *file, int line, char *fname, void *void_vptr, void *void_cptr, char *format, ...)
{
  VUser *vptr = (VUser *)void_vptr;
  connection *cptr = (connection *)void_cptr;
  va_list ap;
  char grp_name[MAX_GRP_NAME_LEN + 1]="\0";
  int amt_written = 0, amt_written1=0;
  int user_index = -1, sess_inst = -1, page_instance = -1;
  int cptr_fd = -1;
  VUser *vp;
  connection *cp;

  GroupSettings *gset;
  char curr_time_buffer[100];

  cp = cptr;
  vp = vptr;

  if (cp)  {                    /* cptr always has correct vptr so we take vptr from cptr */
    vp = (VUser *)cp->vptr;
    cptr_fd = cptr->conn_fd;
  }

  if (vp && (vp->group_num > -1 && vp->group_num < total_runprof_entries)) {
    gset = &runprof_table_shr_mem[vp->group_num].gset;
    strcpy(grp_name, runprof_table_shr_mem[vp->group_num].scen_group_name);
    user_index = vp->user_index;
    sess_inst = vp->sess_inst;
    page_instance = vp->page_instance;
  } else {
    gset = group_default_settings;
    strcpy(grp_name, "-");
  }

  //Checking for gset. Because in one case(when we are freeing group_default_settings pointer, then we are agi
  //logging debug message at that time we will get get NULL. So checking for gset)
  if(((log_level & gset->debug) == 0) || ((gset->module_mask & mask) == 0))
    return;
  
  // my_port_index will be 255 for parent and 1,2..... for chields
  // 05/28/09 11:16:33|00:00:09|netstorm.c|1812|Close_connection|1|TestGroup|0|11|6|Debug|This is a test 99
  //"Absolute Time Stamp|Relative Time Stamp|File|Line|Function|Group|Child/Parent|User Index|Session Instance|Page|Instance|Logs"
  amt_written1 = snprintf(g_tls.log_buffer, g_tls.log_buffer_size, "\n%s|%s|%s|%d|%s|%s|%d|%d|%d|%d|%d|",
                                 nslb_get_cur_date_time(curr_time_buffer, 1), get_relative_time(), file, line, fname, grp_name,
                                 my_child_index, user_index, sess_inst, page_instance, cptr_fd);

  va_start (ap, format);
  amt_written = vsnprintf(g_tls.log_buffer + amt_written1 , g_tls.log_buffer_size - amt_written1, format, ap);
  va_end(ap);

  /**
   *  Upon  successful return, vsnprintf return the number of characters printed (not including the trailing ’\0’ used
   *  to end output to strings).  The functions vsnprintf do not write more  than  size  bytes  (including  the
   *  trailing  ’\0’).  If the output was truncated due to this limit then the return value is the number of characters (not
   *  including the trailing ’\0’) which would have been written to the final string if enough  space  had  been  available.
   *  Thus,  a return value of size or more means that the output was truncated. (See also below under NOTES.)  If an output
   *  error is encountered, a negative value is returned.
   */

  g_tls.log_buffer[g_tls.log_buffer_size-1] = 0;

  // In some cases, vsnprintf return -1 but data is copied in buffer
  // This is a quick fix to handle this. need to find the root cause
  if(amt_written < 0)
  {
    amt_written = strlen(g_tls.log_buffer) - amt_written1;
  }

  if(amt_written > (g_tls.log_buffer_size - amt_written1))
  {
    amt_written = (g_tls.log_buffer_size - amt_written1);
  }

  // If testidx is not there than it will write to the terminal else to the file
  if(testidx < 0)
    printf("%s", g_tls.log_buffer);
  else
  {
    open_log("debug.log", &debug_fd, max_debug_log_file_size, DEBUG_HEADER);
    write(debug_fd, g_tls.log_buffer, amt_written + amt_written1);
  }
}

void open_sess_file(int *fd)
{
  char filename[1024 + 1];
  struct stat s;

  snprintf(filename, 1024, "%s/logs/TR%d/ns_logs/", g_ns_wdir, testidx);
  if(stat(filename, &s) || !S_ISDIR(s.st_mode)) //if ns_logs not found
    mkdir(filename, 0755);

  strcat(filename, "sess_warning.log");
  *fd = open(filename, O_CREAT|O_WRONLY|O_APPEND|O_TRUNC|O_CLOEXEC, 00666);
  if(*fd <= 0)
  {
    NS_EXIT(-1, CAV_ERR_1000006, filename, errno, nslb_strerror(errno));
  }
}

void ns_sess_warning_log(char *format, ...)
{
  va_list ap;
  int amt_written = 0;
  char curr_time_buffer[100];
  int buf_len = 0;
  
  if(testidx < 0)
    return;

  amt_written = snprintf(g_tls.log_buffer, g_tls.log_buffer_size, "%s|", nslb_get_cur_date_time(curr_time_buffer, 1)); 
  va_start(ap,format);
  vsnprintf(g_tls.log_buffer + amt_written, g_tls.log_buffer_size - amt_written, format, ap);
  va_end(ap);
  
  buf_len = strlen(g_tls.log_buffer);

  if(buf_len >= VUSER_THREAD_BUFFER_SIZE-2)
    buf_len = VUSER_THREAD_BUFFER_SIZE -2;
  
  g_tls.log_buffer[buf_len] = '\n';
  g_tls.log_buffer[buf_len + 1] = '\0';
  
  if(sess_warning_fd <= 0)
    open_sess_file(&sess_warning_fd);

  //file will have date time and warning
  write(sess_warning_fd, g_tls.log_buffer, buf_len+1);
}

#if 0

//void ns_debug_log(int log_level, unsigned int mask, VUser *vptr, connection *cptr, char *file, int line, char *fname, char *format, ...)
void ns_debug_log(int log_level, unsigned int mask, char *file, int line, char *fname, char *format, ...)
{
  va_list ap;
  char buffer[MAX_DEBUG_ERR_LOG_BUF_SIZE + 1];
  int amt_written = 0, amt_written1=0;

  if(((log_level & group_default_settings->debug) == 0) || ((group_default_settings->module_mask & mask) == 0))
  //if(((globals.module_mask & mask) == 0))
    return;

  // my_port_index will be 255 for parent and 1,2..... for chields
  amt_written1 = sprintf(buffer, "\n%s|%s|%d|%s|%d|Debug|", get_cur_date_time(), file, line, fname, my_port_index);

  va_start (ap, format);
  amt_written = vsnprintf(buffer + amt_written1 , MAX_DEBUG_ERR_LOG_BUF_SIZE - amt_written1, format, ap);
  va_end(ap);

  /**
   *  Upon  successful return, vsnprintf return the number of characters printed (not including the trailing ’\0’ used
   *  to end output to strings).  The functions vsnprintf do not write more  than  size  bytes  (including  the
   *  trailing  ’\0’).  If the output was truncated due to this limit then the return value is the number of characters (not
   *  including the trailing ’\0’) which would have been written to the final string if enough  space  had  been  available.
   *  Thus,  a return value of size or more means that the output was truncated. (See also below under NOTES.)  If an output
   *  error is encountered, a negative value is returned.
   */
  buffer[MAX_DEBUG_ERR_LOG_BUF_SIZE] = 0;

  // If testidx is not there than it will write to the terminal else to the file
  if(testidx < 0)
    printf("%s", buffer);
  else
  {
    open_log("debug.log", &debug_fd, max_debug_log_file_size, DEBUG_HEADER);
    write(debug_fd, buffer, (amt_written + amt_written1));
  }
}

#endif

char *log_level_to_str(int log_level)
{
  switch(log_level) {
  case ERROR_LOG_CRITICAL:
    return "Critical";
  case ERROR_LOG_MAJOR:
    return "Major";
  case ERROR_LOG_MINOR:
    return "Minor";
  case ERROR_LOG_WARNING:
    return "Warning";
  default:
    return "Unknown Log Level";
  }

  /* Can not reach */
  return NULL;
}

// This mthd will be called from the get_req_status() in the netstorm.c
void error_log_ex(int log_level, char *file, int line, char *fname,
                  //VUser *vptr, connection *cptr, char *error_id,
                  void *void_vptr, void *void_cptr, char *error_id,
                  char *error_attr, char *format, ...)
{
  VUser *vptr = (VUser *)void_vptr;
  connection *cptr = (connection *)void_cptr;
  va_list ap;
  int amt_written = 0, amt_written1 = 0;
  int user_index = -1, sess_inst = -1, page_instance = -1;
  int cptr_fd = -1;
  VUser *vp;
  connection *cp;
  //GroupSettings *gset;
  char grp_name[MAX_GRP_NAME_LEN + 1]="\0";
  char curr_time_buffer[100];

  if (!(global_settings->error_log & log_level))
    return;

  cp = cptr;
  vp = vptr;

  if (cp)  {                    /* cptr always has correct vptr so we take vptr from cptr */
    vp = (VUser *)cp->vptr;
    cptr_fd = cptr->conn_fd;
  }

  if (vp && (vp->group_num > -1 && vp->group_num < total_runprof_entries)) {
    //gset = &runprof_table_shr_mem[vp->group_num].gset;
    strcpy(grp_name, runprof_table_shr_mem[vp->group_num].scen_group_name);
    user_index = vp->user_index;
    sess_inst = vp->sess_inst;
    page_instance = vp->page_instance;
  } else {
    //gset = group_default_settings;
    strcpy(grp_name, "-");
  }

  amt_written1 = snprintf(g_tls.log_buffer,g_tls.log_buffer_size, "\n%s|%s|%s|%s|%s|%s|%d|%s|%s|%d|%d|%d|%d|%d|Error|",
                         nslb_get_cur_date_time(curr_time_buffer, 1), get_relative_time(), error_id, log_level_to_str(log_level),
                         error_attr, file, line, fname, grp_name, my_child_index,
                         user_index, sess_inst, page_instance, cptr_fd);

  va_start(ap, format);
  amt_written = vsnprintf(g_tls.log_buffer + amt_written1, g_tls.log_buffer_size - amt_written1, format, ap);
  va_end(ap);
  /**
   *  Upon  successful return, vsnprintf return the number of characters printed (not including the trailing ’\0’ used
   *  to end output to strings).  The functions vsnprintf do not write more  than  size  bytes  (including  the
   *  trailing  ’\0’).  If the output was truncated due to this limit then the return value is the number of characters (not
   *  including the trailing ’\0’) which would have been written to the final string if enough  space  had  been  available.
   *  Thus,  a return value of size or more means that the output was truncated. (See also below under NOTES.)  If an output
   *  error is encountered, a negative value is returned.
   */
  g_tls.log_buffer[g_tls.log_buffer_size-1] = 0;

  // In some cases, vsnprintf return -1 but data is copied in buffer
  // This is a quick fix to handle this. need to find the root cause
  if(amt_written < 0)
  {
    amt_written = strlen(g_tls.log_buffer) - amt_written1;
  }

  if(amt_written > (g_tls.log_buffer_size - amt_written1))
  {
    amt_written = (g_tls.log_buffer_size - amt_written1);
  }
  // If testidx is not there than it will write to the terminal else to the file
  if(testidx < 0)
    fprintf(stderr, "%s", g_tls.log_buffer);
  else
  {
    open_log("error.log", &error_fd, max_error_log_file_size,
             "Absolute Time Stamp|Relative Time Stamp|Error Id|Log Level|Error Attribute|File|Line|Function|Group|Parent/Child Idx|User Index|Session Instance|Page Instance|fd|Type|User Data");
    write(error_fd, g_tls.log_buffer, amt_written + amt_written1);
  }
}

static char *module_mask_name[] =
{
  "ALL",
  "HTTP",        "POLL",          "CONN",          "TESTCASE",
  "VARS",        "TRANS",         "RUNLOGIC",      "MESSAGES",
  "REPORTING",   "GDF",           "OAAM",          "COOKIES",
  "IPMGMT",      "SOCKETS",       "LOGGING",       "API",
  "SCHEDULE",    "ETHERNET",      "HASHCODE",      "SSL",
  "MON",         "WAN",           "CHILD",         "PARENT",
  "TIMER",       "MISC",          "MEMORY",        "REPLAY",
  "SCRIPT",      "SMTP",          "POP3",          "FTP",
  "DNS",         "CACHE",         "JAVA_SCRIPT",   "PARSING",
  "TR069",       "USER_TRACE",    "RUNTIME",       "AUTH",
  "PROXY",       "SP",            "NJVM",          "RBU",
  "LDAP",        "IMAP",           "JRMI",         "WS",
  "MM_PERCENTILE","HTTP2",         "RTE",          "DB_AGG",
  "SVRIP",        "HLS" ,          "XMPP",         "JMS",    
  "NH"
};

static unsigned long long module_mask_bit[] =
{
  MM_ALL,
  MM_HTTP,       MM_POLL,         MM_CONN,         MM_TESTCASE,
  MM_VARS,       MM_TRANS,        MM_RUNLOGIC,     MM_MESSAGES,
  MM_REPORTING,  MM_GDF,          MM_OAAM,         MM_COOKIES,
  MM_IPMGMT,     MM_SOCKETS,      MM_LOGGING,      MM_API,
  MM_SCHEDULE,   MM_ETHERNET,     MM_HASHCODE,     MM_SSL,
  MM_MON,        MM_WAN,          MM_CHILD,        MM_PARENT,
  MM_TIMER,      MM_MISC,         MM_MEMORY,       MM_REPLAY,
  MM_SCRIPT,     MM_SMTP,         MM_POP3,         MM_FTP,
  MM_DNS,        MM_CACHE,        MM_JAVA_SCRIPT,  MM_PARSING,
  MM_TR069,      MM_USER_TRACE,   MM_RUNTIME,      MM_AUTH,
  MM_PROXY,      MM_SP,           MM_NJVM,         MM_RBU,
  MM_LDAP,       MM_IMAP,         MM_JRMI,         MM_WS,
  MM_PERCENTILE, MM_HTTP2,        MM_RTE,          MM_DB_AGG,
  MM_SVRIP ,     MM_HLS,          MM_XMPP,         MM_JMS,
  MM_NH	,	 MM_RDP
}; /*bug 79149: MM_RDP added*/

int kw_set_modulemask(char *buf, unsigned long long *to_change, char *err_msg, int runtime_flag)
{
  char *mask_tokens[100];
  int  num_module_mask;
  int  i,j,wrt_bytes = 0;
  unsigned long long token_module_mask = 0;
  unsigned long long module_mask = 0;

  if (buf[strlen(buf) - 1] == '\n')
    buf[strlen(buf) - 1] = '\0'; // Replace new line by Null
  num_module_mask = get_tokens(buf, mask_tokens, " ", 100);

  if(runtime_flag)
     wrt_bytes += sprintf(err_msg, "Runtime changes applied to scenario group '%s'. Module selected by users are ", mask_tokens[1]);

  for (i = 2; i < num_module_mask; i++) // Oth token will be module_mask
  {
    if (isalpha(mask_tokens[i][0])) // String format
    {
      //fprintf(stderr, "The name of Module mask supplied by user is '%s' is a string\n", mask_tokens[i]);
      for (j = 0; j < (sizeof(module_mask_name) / sizeof(long)); j++) // Increase the loop count if there is new module masks is to be added
      {
        if (!strcasecmp(mask_tokens[i], module_mask_name[j]))
        {
          token_module_mask = module_mask_bit[j];
          if(runtime_flag)
           wrt_bytes += sprintf(err_msg + wrt_bytes, "%s ", module_mask_name[j]);
          break;
        }
        else if (j == (sizeof(module_mask_name) - 1)) // Increase the loop count if there is new module masks is to be added
        {
          NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_MODULEMASK_USAGE, CAV_ERR_1011089, mask_tokens[i], "");
        }
        else
          continue;
      }
      if(j == sizeof(module_mask_name)/sizeof(long)) 
      {
        NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_MODULEMASK_USAGE, CAV_ERR_1011089, mask_tokens[i], "");
      }
    }
    else if (!strncmp(mask_tokens[i], "0x", 2)) // Hex format
    {
      //fprintf(stderr, "The name of Module mask supplied by user is '%s' is a Hex\n", mask_tokens[i]);
      sscanf(mask_tokens[i], "%llX", &token_module_mask);
    }
    else // Decimal format
    {
      //fprintf(stderr, "The name of Module mask supplied by user is '%s' is a Decimal\n", mask_tokens[i]);
      token_module_mask = atoll(mask_tokens[i]);
    }

    //fprintf(stderr, "The token_module_mask is = %u\n", token_module_mask);
    module_mask = module_mask + token_module_mask;
  }

  *to_change = module_mask;

  return 0;
}

int kw_set_max_debug_log_file_size(char*buf, char *err_msg, int runtime_flag)
{
  //fprintf(stderr, "kw_set_max_debug_log_file_size() - Method called\n");
  char keyword[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH];
  char text[MAX_DATA_LINE_LENGTH];
  int debug_log_file_size;
  int num;

  NSDL2_SCHEDULE(NULL, NULL, "Method called");

  num = sscanf(buf, "%s %s %s", keyword, text, tmp);
  if(num != 2) { //Check for extra arguments.
    sprintf(err_msg, "Error: Invalid number of arguments for Keyword MAX_DEBUG_LOG_FILE_SIZE");
    if(!runtime_flag)
    {
      NS_EXIT(-1, "%s", err_msg);
    }
    else
    {
      return(-1);
    }
  }

  if(ns_is_numeric(text))
  {
    debug_log_file_size = (atoi(text));
  }
  else
  {
    sprintf(err_msg, "Error: Value of MAX_DEBUG_LOG_FILE_SIZE is not numeric");
    if(!runtime_flag)
    {
      NS_EXIT(-1,  "%s", err_msg);
    }
    else
    {
      return(-1);
    }
  }

  if ((debug_log_file_size < 1 ) || (debug_log_file_size > 2048))
  {
    sprintf(err_msg, "Error: Maximum Debug Log File Size cannot be less than 1 MB or greater then 2048 MB");
    if(!runtime_flag)
    {
      NS_EXIT(-1, "%s", err_msg);
    }
    else
      return(-1);
  }
  max_debug_log_file_size = debug_log_file_size * 1024 * 1024;
  // Bug - 77372
  nslb_util_set_debug_log_size(max_debug_log_file_size);
  NSDL2_SCHEDULE(NULL, NULL, "max_debug_log_file_size = %d", max_debug_log_file_size);
  return 0;
}

int kw_set_max_error_log_file_size(char*buf, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH];
  char text[MAX_DATA_LINE_LENGTH];
  int error_log_file_size;
  int num;

  num = sscanf(buf, "%s %s %s", keyword, text, tmp);
  if(num != 2)
  { //Check for extra arguments.
    sprintf(err_msg, "Invalid number of arguments for Keyword MAX_ERROR_LOG_FILE_SIZE");
    if(!runtime_flag)
    {
      NS_EXIT(-1, "%s", err_msg);
    }
    else
    {
      return(-1);
    }
  }

  if(ns_is_numeric(text))
  {
    error_log_file_size = (atoi(text));
  }
  else
  {
    sprintf(err_msg, "Error: Value of MAX_ERROR_LOG_FILE_SIZE is not numeric");
    if(!runtime_flag)
    {
      NS_EXIT(-1, "%s\n", err_msg);
    }
    else
    {
      return(-1);
    }
  }

  if ((error_log_file_size < 1 ) || (error_log_file_size > 2048))
  {
    sprintf(err_msg, "Error: Maximum Error Log File Size cannot be less than 1 MB or greater then 2048 MB");
    if(!runtime_flag)
    {
      NS_EXIT(-1, "%s", err_msg);
    }
    else
      return(-1);
  }

  max_error_log_file_size = error_log_file_size * 1024 * 1024;
  return 0;
}

extern char *argv0;
int give_debug_error(int runtime_flag, char *err_msg)
{
  if (strstr(argv0, ".debug") == NULL) { /* Not running with netstorm.debug */
    return -1;
  }
  return 0;
}


/*********************************************************
   summary: This funtion is used to set the value of debug mode test.
   funtion_name: kw_set_debug_test_settings
   Keyword: DEBUG_TEST_SETTINGS vusers session_rate duration session
   example: DEBUG_TEST_SETTINGS 10 60 300 100
**********************************************************/
void kw_set_debug_test_settings(char *buf)
{
  int num;
  char keyword[MAX_DATA_LINE_LENGTH];
  char vusers[MAX_DATA_LINE_LENGTH];
  char session_rate[MAX_DATA_LINE_LENGTH];
  char duration[MAX_DATA_LINE_LENGTH];
  char session[MAX_DATA_LINE_LENGTH];
  int vusers_mode, session_rate_mode, duration_mode, session_mode; 
 
  if ((num = sscanf(buf, "%s %s %s %s %s", keyword, vusers, session_rate, duration, session)) < 5) {
     NS_EXIT(-1, "Invalid format of the keyword %s", keyword);
  }
 
  // Below checks has been commented - as per customer needs. 
  // We need to configure this keyword as per the test configuration to make this as a debug test.
  //if(vusers_value <= 0 || vusers_value > 100) {
  if((nslb_atoi(vusers, &vusers_mode) < 0) || (vusers_mode <= 0)) {
    NSTL1(NULL, NULL, "Warning: Debug test settings virtual users value is not between 1 and 100. Setting to default value = %d.", DEFAULT_DEBUG_TEST_SETTINGS_VUERS);
    vusers_mode = DEFAULT_DEBUG_TEST_SETTINGS_VUERS;
  }

  //if(session_rate_value <= 0 || session_rate_value > 100) {
  if((nslb_atoi(session_rate, &session_rate_mode) < 0) || (session_rate_mode <= 0) ) {
    NSTL1(NULL, NULL, "Warning: Debug test settings session rate value is not between 1 and 100. Setting to default value = %d.", DEFAULT_DEBUG_TEST_SETTINGS_SESSION_RATE);
    session_rate_mode = DEFAULT_DEBUG_TEST_SETTINGS_SESSION_RATE;
  }

  //if(duration_value <= 0 || duration_value > 60*15) {
  if((nslb_atoi(duration, &duration_mode) < 0) || (duration_mode <= 0)) {
    NSTL1(NULL, NULL, "Warning: Debug test settings duration value is not between 1 and 900. Setting to default value = %d.", DEFAULT_DEBUG_TEST_SETTINGS_DURATION);
    duration_mode = DEFAULT_DEBUG_TEST_SETTINGS_DURATION;
  }

  //if(session_value <= 0 || session_value > 1000) {
  if((nslb_atoi(session, &session_mode) < 0) || (session_mode <= 0)) {
    NSTL1(NULL, NULL, "Warning: Debug test settings session value is not between 1 and 1000. Setting to default value = %d.", DEFAULT_DEBUG_TEST_SETTINGS_SESSION);
    session_mode = DEFAULT_DEBUG_TEST_SETTINGS_SESSION;
  }

  //Storing the value of keyword argument in data structure
  global_settings->debug_setting.debug_test_value_vuser = vusers_mode;
  global_settings->debug_setting.debug_test_value_session_rate = session_rate_mode;
  global_settings->debug_setting.debug_test_value_duration = duration_mode;
  global_settings->debug_setting.debug_test_value_session = session_mode;

  NSDL2_PARSING(NULL, NULL, "debug_test_value_vuser = %d, debug_test_value_session_rate = %d, debug_test_value_duration = %d,"
                            "debug_test_value_session = %d", global_settings->debug_setting.debug_test_value_vuser, 
                             global_settings->debug_setting.debug_test_value_session_rate, 
                             global_settings->debug_setting.debug_test_value_duration,
                             global_settings->debug_setting.debug_test_value_session);

}

void kw_set_debug_mask(char *text, int *to_change)
{
  unsigned int debug_mask  = 0;
  char err_msg[0xff];

  //fprintf(stderr, "kw_set_debug_mask() mthd called\n");

  if (!strncmp(text, "0x", 2)) // Hex format
  {
    //fprintf(stderr, "The name of debug mask supplied by user is '%s' is a Hex\n", text);
    sscanf(text, "%x", &debug_mask);
  }
  else // Decimal format
  {
    //fprintf(stderr, "The name of debug mask supplied by user is '%s' is a decimal\n", text);
    debug_mask = atol(text);
  }

  *to_change = debug_mask;

  if (*to_change != 0)
  {
    if(give_debug_error(0, err_msg) == -1)
      NS_EXIT(-1, "G_DEBUG_MASK configuration is supported only for scenario in which debug logging is enabled.");
  }
}

int kw_set_debug(char *buf, int *to_change, char *err_msg, int runtime_flag)
{
  //fprintf(stderr, "kw_set_debug() mthd called\n");
  int ret, debug_val = 0, num;
  char text[MAX_DATA_LINE_LENGTH];
  char keyword[MAX_DATA_LINE_LENGTH];
  char grp[MAX_DATA_LINE_LENGTH];

  if ((num = sscanf(buf, "%s %s %s", keyword, grp, text)) < 3) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_DEBUG_USAGE, CAV_ERR_1011087, CAV_ERR_MSG_1);
  }
  // Do validations on text
  if(ns_is_numeric(text))
  {
    debug_val = (atoi(text));
  }
  else
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_DEBUG_USAGE, CAV_ERR_1011087, CAV_ERR_MSG_2);
  }
  if (debug_val != 0)
  {
    if ((ret = give_debug_error(runtime_flag, err_msg)) == -1)
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_DEBUG_USAGE, CAV_ERR_1011022, keyword);
  }
  *to_change = debug_val;
  debug_log_value = *to_change;
  if(*to_change == 1) *to_change = 0x000000FF;
  if(*to_change == 2) *to_change = 0x0000FFFF;
  if(*to_change == 3) *to_change = 0x00FFFFFF;
  if(*to_change == 4) *to_change = 0xFFFFFFFF;
  // debug_log_value = *to_change;
     
  //Set memory_map
  if(debug_log_value)
  {
    global_settings->enable_memory_map = 1;   
    nslb_mem_map_init();
  }

  if(runtime_flag)
    sprintf(err_msg, "Runtime changes applied to 'G_DEBUG' for scenario group '%s'. New setting for debug level = %d.", grp, debug_log_value);
  if(!runtime_flag && ((debug_log_value <= 4) && (debug_log_value > 0)))
    NS_DUMP_WARNING("Test is running in debug mode for scenario group '%s' with debug level = %d. If you are running load test that can affect your performance", grp, debug_log_value);
 
  return 0;
}


//Move to ns_log.c
#define MAX_DNS_BUFFER_SIZE 256 * 1024    //size of the buffer
#define NINETY_FIVE_PCT_OF_MAX_DNS_BUFFER_SIZE 249030  /* 95% of 256K buffer*/
#define DNS_RESOLVE_LOG_DUMP_INTERVAL_MILLI 300000  // 5 minutes

static char partition_name[2048], dns_resolve_log_buf[MAX_DNS_BUFFER_SIZE+1];
static unsigned int dns_log_file_size = 0;
static long long partition_idx = -1;
static int max_dns_log_file_size = 2 * MAX_DNS_BUFFER_SIZE;
static u_ns_ts_t dns_resolve_log_last_write_ts = 0;
static char header[] = "#TimeStamp|Host Name|IP Address|Lookup Time|Status\n";

// Average record size is 100 bytes. So in 1 MB, we can log 10K records
// If Session rate is 10K per minute, then we will fill this buffer in 100 minutes 
int g_dns_file_fd = -1;
unsigned int dns_resolve_log_buf_size = 0;
char dns_log_file[2048], log_file_prev[2048];
int local_dns_now;

/* Child need to call this on from child init if dns resolve log is enabled */
void dns_resolve_log_open_file()
{
  // Make file name
  sprintf(dns_log_file, "%s/logs/TR%d/%s/ns_logs/dns_lookup_details_%d.dat", g_ns_wdir, testidx, partition_name, my_child_index);
  
  // Open file
  //if fd is not open then open it
  g_dns_file_fd = open (dns_log_file, O_CREAT|O_CLOEXEC|O_WRONLY|O_APPEND, 00666);
  if (g_dns_file_fd == -1)
  {
    NS_EXIT(-1, "Error: Error in opening file '%s', Error = '%s'", dns_log_file, nslb_strerror(errno));
  }

  write(g_dns_file_fd, header, strlen(header));
  dns_log_file_size += strlen(header);
}

/* called from creating_ns_files_for_new_partition() */

int dns_resolve_log_switch_partition()
{ 
  dns_resolve_log_close_file();
  dns_resolve_log_open_file();
  return 0;
}


int dns_resolve_log_flush_buf()
{  

  if(g_dns_file_fd > 0)
  {
    if(write(g_dns_file_fd, dns_resolve_log_buf, dns_resolve_log_buf_size) < 0) 
    {
      NS_EXIT(-1, "Error: Error in writing file '%s', Error = '%s'", dns_log_file, nslb_strerror(errno));
    }
  }
  else
    return 0;
  
  dns_log_file_size += dns_resolve_log_buf_size;
  dns_resolve_log_buf_size = 0;
  dns_resolve_log_last_write_ts = get_ms_stamp(); 
  //dns_resolve_log_buf[MAX_DNS_BUFFER_SIZE+1] = 0;
  
  return 0;
}

//need to call this function at the end of test so that data still in the buffer will be flushed to files 
/* Called in two cases - Switch partition and at the end */
int dns_resolve_log_close_file()
{
  dns_resolve_log_flush_buf();
  //reset()
  //close()
  if(g_dns_file_fd > 0)
  {
    close(g_dns_file_fd);
    g_dns_file_fd = -1;

  }
  return 0;
}

/* change name - dns_resolve_log_write() */
//char *dns_status, char *server_name, int dns_lookup_time, struct sockaddr_in6 *resolved_ip;

void dns_resolve_log_write(long long cur_partition_idx, char *dns_status, char *server_name, int dns_lookup_time, struct sockaddr_in6 *resolved_ip)
{
  int status, port = 0;
  u_ns_ts_t now;

  if(cur_partition_idx == 0)   //if NS test is there
    strcpy(partition_name, ""); //Non partition case fill empty string
  
  NSDL1_SOCKETS(NULL, NULL, "Method called, cur_partition_idx = %lld, partition_idx = %lld", cur_partition_idx, partition_idx);
  // check for partition name
  if((cur_partition_idx > 0) && (cur_partition_idx > partition_idx)) 
  {
    //save partition name
    partition_idx = cur_partition_idx;
    snprintf(partition_name, 16, "%lld", partition_idx);
    partition_name[15] = '\0';
 
    dns_resolve_log_switch_partition();
  }

  now = get_ms_stamp();
  unsigned int local_now_sec = (now - global_settings->test_start_time)/1000;
  unsigned int local_now = (local_now_sec % 3600);

  /*Dump DNS lookup detail into buffer*/
  dns_resolve_log_buf_size += sprintf(dns_resolve_log_buf + dns_resolve_log_buf_size, "%02d:%02d:%02d|%s|%s|%d|%s\n", (local_now_sec / 3600), (local_now / 60), (local_now % 60), server_name, nslb_sockaddr_to_ip((struct sockaddr *)resolved_ip, port), dns_lookup_time, dns_status);
  
  NSDL1_SOCKETS(NULL, NULL, "Log file = %s, current size = [%d] max_dns_log_file_size = [%d].", dns_log_file, dns_resolve_log_buf_size, max_dns_log_file_size);

  NSDL1_SOCKETS(NULL, NULL, "now = [%d], dns_resolve_log_last_write_ts = [%d].", now, dns_resolve_log_last_write_ts);

  //when buffer fills 95% dump it to file of size 256K
  //OR 5 mins has been lapsed when last dumped
  if((dns_resolve_log_buf_size > NINETY_FIVE_PCT_OF_MAX_DNS_BUFFER_SIZE) || 
                     ((now - dns_resolve_log_last_write_ts) > DNS_RESOLVE_LOG_DUMP_INTERVAL_MILLI))
  {
    //TODO: What if buffer has some data and test is stopped then data will be lost
    dns_resolve_log_flush_buf();
  }  

  //when file get filled above 512K, a new file created with file.prev 
  if(dns_log_file_size > max_dns_log_file_size )
  { 
    dns_resolve_log_close_file();
    sprintf(log_file_prev, "%s.prev", dns_log_file);
    
    NSDL1_SOCKETS(NULL, NULL, "log_file_prev = %s dns_log_file = %s\n", log_file_prev, dns_log_file);
    status = rename(dns_log_file, log_file_prev);
    if(status < 0)
      // Never use debug_log from here
      fprintf(stderr, "Error in moving '%s' file, err = %s\n", dns_log_file, nslb_strerror(errno));  
    
    fprintf(stdout, "Moving DNS Resolve Log file %s file with size %u to %s file, max size = %u\n", 
                                    dns_log_file, dns_log_file_size, log_file_prev, max_dns_log_file_size);
    dns_resolve_log_open_file();
    dns_log_file_size = 0;
  }

  NSDL1_SOCKETS(NULL, NULL, "dns_log_file_size = [%d]", dns_log_file_size);
}


// End of file
