/******************************************************************
 * Name    :    ns_trace_level.c
 * Author  :    
 * Purpose :    This file contains trace level methods
 * Modification History:
 * Note:
 *
 *****************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <regex.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <v1/topolib_log.h>
#include "ns_log.h"
#include "ns_trace_level.h"
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

#include "ns_server.h"
#include "util.h"
#include "tmr.h"
#include "timing.h"
#include "ns_gdf.h"
#include "ns_event_log.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "nslb_hessian.h"
#include "netstorm.h"
#include "nslb_alloc.h"

#define MAX_BUF_SIZE 32
#define DEFAULT_MON_LOG_LEVEL 0x000000FF
#define DEFAULT_TRACE_LEVEL_FILE_SIZE 10000000
int ns_event_fd = -1;
unsigned int max_trace_level_file_size = DEFAULT_TRACE_LEVEL_FILE_SIZE;  // Approx 10 MB
unsigned int mon_log_tracing_level;
/*************************************************************************
 * Description       : ns_trace_level_usage() macro used to print usage for
 *                     NS_TRACE_LEVEL keyword and exit.
 *                     Called from kw_set_ns_trace_leve().
 * Input Parameters
 *       err         : Print error message.
 *       buf         : To print keyword
 *       runtime_flag: For RTC flag is set 1 otherwise 0
 *       err_msg     : Use to store error message for runtime changes 
 * Output Parameters : None
 * Return            : None
 *************************************************************************/

static int ns_trace_level_usage(char *err, char *buf, int runtime_flag, char *err_msg) 
{
  sprintf(err_msg, "Error: Invalid value of NS_TRACE_LEVEL keyword: %s\nLine: %s\n ", err, buf);
  strcat(err_msg, "  Usage: NS_TRACE_LEVEL <trace level> <trace level file size\n");
  strcat(err_msg, "  Where trace level:\n");
  strcat(err_msg, "                    Used for debugging purpose, to trace netstorm messages, here 1 is default value\n");
  strcat(err_msg, "        trace level file size:\n");
  strcat(err_msg, "      Used to provide trace level file size. This is an optional field\n");
  if(runtime_flag == 0)
    {NS_EXIT(-1, "%s", err_msg);}
  else{
    NSTL1_OUT(NULL, NULL, "%s", err_msg);
    return -1;
  }
  return 0;
}

/* Function used to parse NS_TRACE_LEVEL keyword
 * Syntax: NS_TRACE_LEVEL <trace level> <trace log file size>
 * */
int kw_set_ns_trace_level(char *buf, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char level[MAX_DATA_LINE_LENGTH];
  char size[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH];//This used to check if some extra field is given
  unsigned int trace_file_size;
  int trace_level = 1, num;

  //Setting default value
  strcpy(level, "1");
  strcpy(size, "10"); //10 MB default

  NSDL1_PARSING(NULL, NULL, "Method called.");
  NSDL4_PARSING(NULL, NULL, "Buffer received, buf = %s", buf);

  num = sscanf(buf, "%s %s %s %s", keyword, level, size, tmp); // This is used to check number of arguments

  if ((num < 2) || (num > 3)) {
    ns_trace_level_usage("Invalid number of arguments", buf, runtime_flag, err_msg);
  }

  if(ns_is_numeric(level) == 0) {
    ns_trace_level_usage("Trace level should be numeric value", buf, runtime_flag, err_msg);
  }
  trace_level = atoi(level);

  if((trace_level < 0) || (trace_level > 4)) {
    ns_trace_level_usage("Invalid value for trace level", buf, runtime_flag, err_msg);  
  }

  if(ns_is_numeric(size) == 0) {
    ns_trace_level_usage("Trace level file size should be numeric value", buf, runtime_flag, err_msg);
  }

  trace_file_size = atoi(size);

  if ((trace_file_size < 1 ) || (trace_file_size > 2048)) {
    ns_trace_level_usage("Trace level File Size cannot be less than 1 MB or greater then 2048 MB", buf, runtime_flag, err_msg);
  }

  if(trace_level == 0) global_settings->ns_trace_level = 0x00000000;
  if(trace_level == 1) global_settings->ns_trace_level = 0x000000FF;
  if(trace_level == 2) global_settings->ns_trace_level = 0x0000FFFF;
  if(trace_level == 3) global_settings->ns_trace_level = 0x00FFFFFF;
  if(trace_level == 4) global_settings->ns_trace_level = 0xFFFFFFFF;

  //max_trace_event_file_size = trace_file_size * 1024 * 1024; //In MB

  max_trace_level_file_size = trace_file_size * 1024 * 1024; //In MB

  NSDL2_PARSING(NULL, NULL, "Trace level = %d, Trace log size = %d", global_settings->ns_trace_level, max_trace_level_file_size);
  return 0;
}

int kw_set_nlm_trace_level(char *buf, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH];
  int  level;
  int size = 0; 
  int num;
  
  NSDL2_PARSING(NULL, NULL, "Method called, buf = %s", buf);
  
  num = sscanf(buf, "%s %d %d %s", keyword, &level, &size, tmp);
 
  if (num < 2){
    NS_EXIT(-1, "Invaid number of arguments in line %s", buf); 
  }
  
  if (level < 0 || level > 4){
    fprintf(stderr, "Invaid nsa_log_manager level %d, hence setting to default level 1\n", level);
    level = 1;
  }

  if (size <= 0){
    if(num >= 3)  //checking if size if provided by user or not; if yes then show warning
      fprintf(stderr, "Invaid nsa_log_manager file size %d,  hence setting to default size 10 MB\n", size);
    size = 10;
  }

  global_settings->nlm_trace_file_sz = size*1024*1024;
  global_settings->nlm_trace_level = level;

  return 0;
}

int kw_set_nlr_trace_level(char *buf, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH];
  int level;
  int size = 0; 
  int num;
  
  NSDL2_PARSING(NULL, NULL, "Method called, buf = %s", buf);
  
  num = sscanf(buf, "%s %d  %d %s", keyword, &level, &size, tmp);
 
  if (num < 2){
    NS_EXIT(-1, "Invaid number of arguments %s", buf);
  }
  
  if (level < 0 || level > 4)
  {
    fprintf(stderr, "Invaid logging reader level %d, hence setting to default level 1\n", level);
    level = 1;
  }


  if (size <= 0){
    if(num >= 3) //checking if size is provided by user or not, if yes then show warning
      fprintf(stderr, "Invalid logging reader file size %d, hence setting to default size 10 MB\n", size);
    size = 10;
  }

  global_settings->nlr_trace_file_sz = size*1024*1024;
  global_settings->nlr_trace_level = level;

  return 0;
}

int kw_set_nlw_trace_level(char *buf, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH];
  int level;
  int size = 0; 
  int num;
  
  NSDL2_PARSING(NULL, NULL, "Method called, buf = %s", buf);
  
  num = sscanf(buf, "%s %d %d %s", keyword, &level, &size, tmp);
 
  if (num < 2){
    NS_EXIT(-1, "Invaid number of arguments in line %s.", buf);
  }
  
  if (level < 0 || level > 4)
  {
    fprintf(stderr, "Invaid logging writer level %d, hence setting to default level 1\n", level);
    level = 1;
  }

  if (size <= 0){
    if(num >= 3) //checking if size is provided by user or not, if yes then show warning
    fprintf(stderr, "Invalid logging writer file size %d, hence setting to default size 10 MB;\n", size);
    size = 10;
  }

  global_settings->nlw_trace_file_sz = size*1024*1024;
  global_settings->nlw_trace_level = level;

  return 0;  
}

int kw_set_nsdbu_trace_level(char *buf, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH];
  int  level; 
  int size = 0;
  int num;
  
  NSDL2_PARSING(NULL, NULL, "Method called, buf = %s", buf);
  
  num = sscanf(buf, "%s %d %d %s", keyword, &level, &size, tmp);
 
  if (num < 2){
    NS_EXIT(-1, "Invaid number of arguments in line %s", buf);
  }
  
  if (level < 0 || level > 4)
  {
    fprintf(stderr, "Invaid logging writer level %d, hence setting to default level 1\n", level);
    level = 1;
  }


  if (size <= 0){
    if(num >= 3)    //checking if size is provided by user or not, if yes then show warning
      fprintf(stderr, "Invalid ndbu trace level file size %d, hence setting to default size 10 MB\n", size);
    size = 10;
  }

  global_settings->nsdbu_trace_file_sz = size*1024*1024;
  global_settings->nsdbu_trace_level = level;

  return 0;
}

void kw_set_mon_log_trace_level(char *text1, char *msg)
{
  char level[MAX_BUF_SIZE];
  int ret;
  char keywd[MAX_BUF_SIZE];

  ret = sscanf(text1, "%s %s",keywd , level);
  if (ret != 2)
  {
    sprintf(msg, "Wrong Usage. Expected usage of %s keyword is '<Keyword> <value>. Hence setting it to default value %d", keywd,DEFAULT_MON_LOG_LEVEL );
    mon_log_tracing_level =DEFAULT_MON_LOG_LEVEL; 
  } 
  else 
  {
    if(!ns_is_numeric(level))
    {
      sprintf(msg, "Value for MONITOR_TRACE_LEVEL is not numeric. Given value is %s. Hence setting it to default value %d", level,DEFAULT_MON_LOG_LEVEL);
      mon_log_tracing_level =DEFAULT_MON_LOG_LEVEL;
    }
    else
    {
      mon_log_tracing_level = atoi(level);
      sprintf(msg, "tracing_level = %d\n", mon_log_tracing_level);

      if(mon_log_tracing_level == 0) mon_log_tracing_level = 0x00000000;
      else if(mon_log_tracing_level == 1) mon_log_tracing_level = 0x000000FF;
      else if(mon_log_tracing_level == 2) mon_log_tracing_level = 0x0000FFFF;
      else if(mon_log_tracing_level == 3) mon_log_tracing_level = 0x00FFFFFF;
      else if(mon_log_tracing_level == 4) mon_log_tracing_level = 0xFFFFFFFF;
    }
  }

}

void ns_trace_level_ex_out(int trace_level, char *file, int line, char *fname, void *void_vptr, void *void_cptr, char *format, ...)
{
  VUser *vptr = (VUser *)void_vptr;
  connection *cptr = (connection *)void_cptr;
  va_list ap;
  //char buffer[MAX_TRACE_LEVEL_BUF_SIZE + 1];
  char grp_name[MAX_GRP_NAME_LEN + 1]="\0";
  int amt_written = 0, amt_written1=0;
  int user_index = -1, sess_inst = -1, page_instance = -1;
  int cptr_fd = -1;
  VUser *vp;
  connection *cp;
  char curr_time_buffer[100];

  cp = cptr;
  vp = vptr;

  if (cp)  {                    /* cptr always has correct vptr so we take vptr from cptr */
    vp = (VUser *)cp->vptr;
    cptr_fd = cptr->conn_fd;
    }

  if (vp && (vp->group_num > -1 && vp->group_num < total_runprof_entries)) {
    strcpy(grp_name, runprof_table_shr_mem[vp->group_num].scen_group_name);
    user_index = vp->user_index;
    sess_inst = vp->sess_inst;
    page_instance = vp->page_instance;
  } else {
    strcpy(grp_name, "-");
  }

  if(((trace_level & global_settings->ns_trace_level) == 0))
    return;

  if(!g_tls.log_buffer)
     ns_tls_init(8001);

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
    //fprintf(stderr, "%s\n", buffer + amt_written1); /*bug 78764 - avoid priting trace unnecessary on stderr */
    open_log("ns_trace.log", &ns_event_fd, max_trace_level_file_size, TRACE_HEADER);
    //fprintf(stderr, "########## FD = %d, buffer = [%s], amt_written = %d, amt_written1 = %d\n", ns_event_fd, buffer, amt_written, amt_written1);
    write(ns_event_fd, g_tls.log_buffer, amt_written + amt_written1);
    /* This is code is moved upward
    va_start (ap, format);
    amt_written = vsnprintf(buffer, MAX_TRACE_LEVEL_BUF_SIZE, format, ap);
    va_end(ap);
    buffer[MAX_TRACE_LEVEL_BUF_SIZE] = 0;
    fprintf(stdout,"%s\n",buffer); */
  }
}

void ns_trace_level_ex(int trace_level, char *file, int line, char *fname, void *void_vptr, void *void_cptr, char *format, ...)
{
  VUser *vptr = (VUser *)void_vptr;
  connection *cptr = (connection *)void_cptr;
  va_list ap;
  //char buffer[MAX_TRACE_LEVEL_BUF_SIZE + 1];
  char grp_name[MAX_GRP_NAME_LEN + 1]="\0";
  int amt_written = 0, amt_written1=0;
  int user_index = -1, sess_inst = -1, page_instance = -1;
  int cptr_fd = -1;
  VUser *vp;
  connection *cp;
  char curr_time_buffer[100];

  cp = cptr;
  vp = vptr;

  if (cp)  {                    /* cptr always has correct vptr so we take vptr from cptr */
    vp = (VUser *)cp->vptr;
    cptr_fd = cptr->conn_fd;
  }

  if (vp && (vp->group_num > -1 && vp->group_num < total_runprof_entries)) {
    strcpy(grp_name, runprof_table_shr_mem[vp->group_num].scen_group_name);
    user_index = vp->user_index;
    sess_inst = vp->sess_inst;
    page_instance = vp->page_instance;
  } else {
    strcpy(grp_name, "-");
  }

  if(((trace_level & global_settings->ns_trace_level) == 0))
    return;

  // my_port_index will be 255 for parent and 1,2..... for chields
  // 05/28/09 11:16:33|00:00:09|netstorm.c|1812|Close_connection|1|TestGroup|0|11|6|Debug|This is a test 99
  //"Absolute Time Stamp|Relative Time Stamp|File|Line|Function|Group|Child/Parent|User Index|Session Instance|Page|Instance|Logs"
  /*amt_written1 = sprintf(buffer, "\n%s|%s|%s|%d|%s|%s|%d|%d|%d|%d|%d|",
                                 nslb_get_cur_date_time(curr_time_buffer, 1), get_relative_time(), file, line, fname, grp_name,
                                 my_port_index, user_index, sess_inst, page_instance, cptr_fd); */
   if(!g_tls.log_buffer)
      ns_tls_init(8001);
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
    open_log("ns_trace.log", &ns_event_fd, max_trace_level_file_size, TRACE_HEADER);
    //fprintf(stderr, "########## FD = %d, buffer = [%s], amt_written = %d, amt_written1 = %d\n", ns_event_fd, buffer, amt_written, amt_written1);
    write(ns_event_fd, g_tls.log_buffer, amt_written + amt_written1);
  }
}

/* Add keyword to LOG VUser data */
int kw_set_log_vuser_data_interval(char *buf, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH];
  int interval = 15 * 1000;   // In mili sec (Default is 15 min)
  int count = 4; //Default 
  int mode = 0;
  int num;
  
  NSDL2_PARSING(NULL, NULL, "Method called, buf = %s", buf);
  
  num = sscanf(buf, "%s %d %d %d %s", keyword, &mode, &interval, &count, tmp);
  NSDL2_PARSING(NULL, NULL, "mode = %d, keyword = %s, interval = %d", mode, keyword, interval);
 
  if (num < 2){
    NS_EXIT(-1, "Invaid number of arguments in line %s", buf); 
  }
 
  global_settings->log_vuser_mode = mode;// Will be use to enable and disable it 
  global_settings->log_vuser_data_interval = interval * 1000; 
  global_settings->log_vuser_data_count = count; 

  return 0;
}
