/******************************************************************
 * Name    : ns_debug_trace.c
 * Author  : Archana
 * Purpose : This file contains methods related to
             parsing keyword and to create debug_trace.log file when run NetStorm in Debug trace mode 
 * Note:
 Format of debug_trace.log file:
 Header will be:
  TimeStamp|IndentLevel|ScriptName|PageName|UrlId|ScriptFileName|ScriptLineNumber|Message
  Where
  TimeStamp is relative time in HH:MM:SS
  IndentLevel is 1, 2, 3, 4. This will be used by GUI to indent the message
  PageName is the page name which is being executed
  UrlId is the id of the URL being executed. (Not available now. Put NA in the file)
  ScriptName is the name of script being executed e.g. hpd_tours
  ScriptFileName is the script.capture if main URL, embedded URL script.detail else script.c 
  ScriptLineNumber is the line number in the script file at which execution is in progress. (Not available now. Put -1 in the file)
  Message is the debug trace message
IndentLevel:
1 - Script
2 - Page or Transaction
3 - Page pre/check and URL
4 - Connect/Req/Res message

 * Modification History:
 * 21/11/08 - Initial Version
*****************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>

#include <string.h>
#include <time.h>
#include <ctype.h>
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
#include "ns_server.h"
#include "util.h"
#include "timing.h"
#include "tmr.h"

#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"
#include "ns_http_version.h"
#include "netstorm.h"

#include "ns_debug_trace.h"
#include "ns_log.h"
#include "ns_vuser_ctx.h"
#include "ns_script_parse.h"
#include "ns_exit.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"
#define MAX_TRACE_LOG_BUF_SIZE 64000
#define MAX_NAME_SIZE 6400

#ifndef CAV_MAIN
int debug_trace_log_value = 0; //default
#else
__thread int debug_trace_log_value = 0;
#endif

int trace_log_fd = -1;  /* bug 78684  removed static keyword in order to make scope global and can be closed in ns_parent.c */
int script_execution_log_fd = -1;


/*This method used to parse DEBUG_TRACE keyword.
  Format:
    DEBUG_TRACE <value>
    Where value is 0 (default) to disable and 1 is to enable debug trace messages
*/
int kw_set_debug_trace(char *text, char *keyword, char *buf)
{
  char err_msg[MAX_DATA_LENGTH];
  debug_trace_log_value = atoi(text);
  NSDL1_HTTP(NULL, NULL, "Method called.debug_trace_log_value = %d\n", debug_trace_log_value);
  if(debug_trace_log_value > 0)
  {
    if (give_debug_error(0, err_msg) == -1)
      NS_KW_PARSING_ERR(buf, 0, err_msg, DEBUG_TRACE_USAGE, CAV_ERR_1011022, keyword);
  }
  return 0;
}

//To get time in HH:MM:SS
static char *get_cur_time()
{
  time_t    tloc;
  struct  tm *lt, tm_struct;
  static  char cur_time[100];

  (void)time(&tloc);
  if((lt = nslb_localtime(&tloc, &tm_struct, 1)) == (struct tm *)NULL)
    strcpy(cur_time, "Error");
  else
    sprintf(cur_time, "%02d:%02d:%02d", lt->tm_hour, lt->tm_min, lt->tm_sec);
  return(cur_time);
}

static void open_debug_trace_file(int *fd)
{
  char log_file[MAX_DATA_LENGTH];

  sprintf(log_file, "%s/logs/TR%d/debug_trace.log", g_ns_wdir, testidx);

  if (*fd <= 0 ) //if fd is not open then open it
  {
    *fd = open (log_file, O_CREAT|O_WRONLY|O_APPEND|O_CLOEXEC, 00666);
    if (!*fd)
    {
      NS_EXIT(-1, CAV_ERR_1000006, log_file, errno, nslb_strerror(errno));
    }
  }
}

void open_script_execution_log_file(int *fd)
{
  char log_file[MAX_DATA_LENGTH];

  sprintf(log_file, "%s/logs/%s/execution_log/script_execution.log", g_ns_wdir, global_settings->tr_or_partition);

  if (*fd <= 0 ) //if fd is not open then open it
  {
    *fd = open (log_file, O_CREAT|O_WRONLY|O_APPEND|O_CLOEXEC, 00666);
    if (!*fd)
    {
      NS_EXIT(-1, "Error: unable to open file '%s', due to = '%s'", log_file, nslb_strerror(errno));
    }
  }
}

//To print header of debug_trace.log file
void trace_log_init()
{
  if(debug_trace_log_value == 0)
    return;

  if(loader_opcode == MASTER_LOADER) {
    if((debug_trace_log_value == 1) && !(global_settings->enable_event_logger))
      NS_EXIT(-1, CAV_ERR_1031016);
  }

  write_log_file(NS_SCENARIO_PARSING, "Opening debug trace");
  char buffer[MAX_HD_FT_SIZE + 1];
  //strcpy(buffer, "TimeStamp|IndentLevel|ScriptName|PageName|UrlId|ScriptFileName|ScriptLineNumber|Message");
  strcpy(buffer, 
  "TimeStamp|IndentLevel|ScriptName|PageName|UrlId|ScriptFileName|ScriptLineNumber|GenId|nvmId|UserIndex|SessIns|PageIns|GrpNo|SessId|PgId|Message");
  open_debug_trace_file(&trace_log_fd);
  write(trace_log_fd, buffer, strlen(buffer));
}

//To print header of script_execution.log file
void script_execution_log_init()
{
  if(debug_trace_log_value == 0)
    return;

  char *write_ptr = NULL;
  int rnum;
  int amt_written = 0;
  int write_idx = 0;
  int free_buf_len = ns_nvm_scratch_buf_size;

  NSDL4_RBU(NULL, NULL, "ns_nvm_scratch_buf_size = %d", ns_nvm_scratch_buf_size);

  write_ptr = ns_nvm_scratch_buf;

  write_idx = snprintf(write_ptr, free_buf_len, "Total Test Case=%d; ", global_settings->num_fetches);
  NS_SET_WRITE_PTR(write_ptr, write_idx, free_buf_len);

  for(rnum=0; rnum < total_runprof_entries; rnum++)
  {
    if(free_buf_len <= 0)
    {
      NSDL1_RBU(NULL, NULL, "Need to realloc ns_nvm_scratch_buf");
      //store written amount as after realloc address of ns_nvm_scratch_buf may change
      amt_written = ns_nvm_scratch_buf_size - 1;

      MY_REALLOC(ns_nvm_scratch_buf, ns_nvm_scratch_buf_size + MAX_DATA_LENGTH, "reallocating for script_execution_log", -1);
      ns_nvm_scratch_buf_size = ns_nvm_scratch_buf_size + MAX_DATA_LENGTH;  //Update length of the buffer

      //Update write_ptr as after realloc address of ns_nvm_scratch_buf may change
      write_ptr = ns_nvm_scratch_buf + amt_written;
      free_buf_len = ns_nvm_scratch_buf_size - amt_written;  //Update free_buf_len as buffer size is increased
    }

    write_idx = snprintf(write_ptr, free_buf_len, "%s=%d; ",
                     runprof_table_shr_mem[rnum].sess_ptr->sess_name, runprof_table_shr_mem[rnum].quantity);
    NS_SET_WRITE_PTR(write_ptr, write_idx, free_buf_len);
  }
  
  write_idx = snprintf(write_ptr, free_buf_len, "Total Pages=%d\n", g_rbu_num_pages);
  NS_SET_WRITE_PTR(write_ptr, write_idx, free_buf_len);

  write_idx = snprintf(write_ptr, free_buf_len,
                          "#TimeStamp|IndentLevel|ScriptName|PageName|UrlId|ScriptFileName|ScriptLineNumber|nvmId|UserIndex|SessIns|PageIns|GrpNo|SessId|PgId|Message");
  open_script_execution_log_file(&script_execution_log_fd);
  write(script_execution_log_fd, ns_nvm_scratch_buf, strlen(ns_nvm_scratch_buf));

  ns_nvm_scratch_buf[0] = 0;  //reset the buffer
}

//To print footer of debug_trace.log file
void trace_log_end()
{
  if(debug_trace_log_value == 0)
    return;

  char buffer[MAX_HD_FT_SIZE + 1];
  snprintf(buffer, MAX_HD_FT_SIZE, "\n**************************************END of Debug Trace**************************************");
  open_debug_trace_file(&trace_log_fd);
  write(trace_log_fd, buffer, strlen(buffer));
}

//To print footer of script_execution.log file
void script_execution_log_end()
{
  if(debug_trace_log_value == 0)
    return;

  char buffer[MAX_HD_FT_SIZE + 1];
  snprintf(buffer, MAX_HD_FT_SIZE, "\n**************************************END of Script Execution Log**************************************");
  open_script_execution_log_file(&script_execution_log_fd);
  write(script_execution_log_fd, buffer, strlen(buffer));
}

/*
**Purpose: To create debug_trace.log file 
**Arguments:
  Arg1 - indent_level will be 1, 2, 3, 4 
  Arg2 - vptr, that used to get ScriptName and PageName, if vptr is NULL then ScriptName and PageName would be NA
  Arg3 - cptr, that used to get ScriptFileName, if cptr is NULL then ScriptFileName will be NA
  Arg4 - file is for future use
  Arg5 - line is for future use
  Arg6 - fname is for future use
  Arg7 - format is rest part of arguments
*/
void ns_debug_trace_log(int indent_level, VUser *vptr, connection *cptr, int debug_mask, u_ns_8B_t module_mask, char *file, int line, char *fname, char *format, ...)
{
  int amt_written = 0, amt_written1=0;
  va_list ap;
  char urlId[MAX_NAME_SIZE];
  char scriptFileName[MAX_FILE_NAME_LEN];
  short scriptLineNumber;
  char *cur_page_name = "NA";
  unsigned int user_index_dt, sess_inst_dt, tot_dt_buf_size;
  unsigned short page_inst_dt, child_id_dt, opcode = DEBUG_TRACE_MSG;
  int max_dt_buf_size = g_tls.buffer_size - (UNSIGNED_INT_SIZE + SHORT_SIZE);
  char *dt_buf = g_tls.buffer;

  if(g_parent_idx != -1)
    dt_buf += UNSIGNED_INT_SIZE + SHORT_SIZE;

  strcpy(urlId, "NA");  //Not available now
  
  if(cptr != NULL)
  {
    if(vptr == NULL)
      vptr = cptr->vptr;

    if ((vptr != NULL) && (vptr->cur_page != NULL))
    {
      strcpy(scriptFileName, vptr->cur_page->flow_name);
    }
    else
      strcpy(scriptFileName, "NA");
  } 
  else            //Bug 25238: cptr is coming NULL, so added "NA" as scriptFileName
    strcpy(scriptFileName, "NA");

  scriptLineNumber = -1; //Not available now.

  child_id_dt = ((child_idx < 0) ? 1 : (child_idx + 1));                         //Local argument for NVM Id

  if(vptr != NULL)
  {
    if(vptr->cur_page != NULL) // To handle script without any pages
      cur_page_name = vptr->cur_page->page_name;

    user_index_dt = ((vptr->user_index < 0) ? 1 : (vptr->user_index + 1));         //Local argument for User Index
    sess_inst_dt = ((vptr->sess_inst < 0) ? 1 : (vptr->sess_inst + 1));            //Local argument for Session Instance
    page_inst_dt = ((vptr->page_instance < 0 ) ? 1 : (vptr->page_instance + 1));   //Local argument for Page Instance

    //Commented : Sending more arguments in Debug trace
    //amt_written1 = sprintf(buffer, "\n%s|%d|%s|%s|%s|%s|%d|", get_cur_time(), indent_level, vptr->sess_ptr->sess_name, cur_page_name, urlId, scriptFileName, scriptLineNumber);
    amt_written1 = snprintf(dt_buf, max_dt_buf_size, "\n%s|%d|%s|%s|%s|%s|%hi|%d|%hu|%u|%u|%hu|%d|%u|%u|", 
                                   get_cur_time(), indent_level, vptr->sess_ptr->sess_name, 
                                   cur_page_name, urlId, scriptFileName, scriptLineNumber, g_parent_idx, child_id_dt, 
                                   user_index_dt, sess_inst_dt, page_inst_dt, vptr->group_num,
                                   GET_SESS_ID_BY_NAME(vptr), GET_PAGE_ID_BY_NAME(vptr));
  }
  else
    amt_written1 = snprintf(dt_buf, max_dt_buf_size, "\n%s|%d|NA|NA|%s|%s|%hi|%d|%hu|-|-|NA|NA|NA|NA|", 
                                   get_cur_time(), indent_level, urlId, scriptFileName, scriptLineNumber, g_parent_idx, child_id_dt);

   //Commented : Sending more arguments in Debug trace
   //amt_written1 = sprintf(buffer, "\n%s|%d|NA|NA|%s|%s|%d|", get_cur_time(), indent_level, urlId, scriptFileName, scriptLineNumber);

  va_start (ap, format);
  amt_written = vsnprintf(dt_buf + amt_written1 , max_dt_buf_size - amt_written1, format, ap);
  va_end(ap);

  dt_buf[max_dt_buf_size-1] = 0;

  if(g_debug_script) // If script debugging ...
  {
    printf("\nDS_MSG_START:\n");
    printf("%s", dt_buf + amt_written1); // Show only message
    printf("\nDS_MSG_END:\n");
  }

  // If testidx is not there than it will write to the terminal else to the file
  if(testidx < 0)
    fprintf(stderr, "%s\n", dt_buf);
  else
  {
    if(debug_trace_log_value != 0)
    {
      if(g_parent_idx == -1) // In case of Netstorm
      {
        //we will write to debug trace only when debug trace is enable, but we will write in debug log.
          open_debug_trace_file(&trace_log_fd);
          write(trace_log_fd, dt_buf, (amt_written + amt_written1));
      }
      else // In case of generator parent
      {
        // This is the message coming from nvm/users
        tot_dt_buf_size = amt_written + amt_written1;
        // If message is truncated 
        if(tot_dt_buf_size > max_dt_buf_size)
          tot_dt_buf_size = max_dt_buf_size;
        // Add opcode in dt_buf
        memcpy(g_tls.buffer + UNSIGNED_INT_SIZE, &opcode, SHORT_SIZE);
        // Add size of opcode
        tot_dt_buf_size += SHORT_SIZE;
        // Add tot_dt_buf_size in dt_buf
        memcpy(g_tls.buffer, &tot_dt_buf_size, UNSIGNED_INT_SIZE);
        // Add size of tot_dt_buf_size
        tot_dt_buf_size += UNSIGNED_INT_SIZE;
        
        write_msg(&g_el_subproc_msg_com_con, g_tls.buffer, tot_dt_buf_size, 1, CONTROL_MODE); // Write at event logger fd
      }
    }
  }
}

//Purpose: To create script_execution.log file 
void ns_script_execution_log(int indent_level, VUser *vptr, connection *cptr, int debug_mask, u_ns_8B_t module_mask, char *file, int line, char *fname, char *format, ...)
{
  int amt_written = 0, amt_written1=0;
  va_list ap;
  char urlId[MAX_NAME_SIZE];
  char scriptFileName[MAX_FILE_NAME_LEN];
  int scriptLineNumber;
  char *cur_page_name = "NA";
  int child_id_dt, user_index_dt, sess_inst_dt, page_inst_dt;
  char curr_time_buffer[100];

  strcpy(urlId, "NA");  //Not available now
  
  if(cptr != NULL)
  {
    if(vptr == NULL)
      vptr = cptr->vptr;

    if ((vptr != NULL) && (vptr->cur_page != NULL))
    {
      strcpy(scriptFileName, vptr->cur_page->flow_name);
    }
    else
      strcpy(scriptFileName, "NA");
  } 
  else
    strcpy(scriptFileName, "NA");

  scriptLineNumber = -1; //Not available now.

  child_id_dt = ((child_idx < 0) ? 1 : (child_idx + 1));                         //Local argument for NVM Id

  if(vptr != NULL)
  {
    if(vptr->cur_page != NULL) // To handle script without any pages
      cur_page_name = vptr->cur_page->page_name;

    user_index_dt = ((vptr->user_index < 0) ? 1 : (vptr->user_index + 1));         //Local argument for User Index
    sess_inst_dt = ((vptr->sess_inst < 0) ? 1 : (vptr->sess_inst + 1));            //Local argument for Session Instance
    page_inst_dt = ((vptr->page_instance < 0 ) ? 1 : (vptr->page_instance + 1));   //Local argument for Page Instance

    amt_written1 = snprintf(g_tls.buffer,g_tls.buffer_size, "\n%s|%d|%s|%s|%s|%s|%d|%hd|%u|%u|%hu|%d|%u|%u|", 
                                   nslb_get_cur_date_time(curr_time_buffer, 1), indent_level, vptr->sess_ptr->sess_name, 
                                   cur_page_name, urlId, scriptFileName, scriptLineNumber, child_id_dt, 
                                   user_index_dt, sess_inst_dt, page_inst_dt, vptr->group_num,
                                   GET_SESS_ID_BY_NAME(vptr), GET_PAGE_ID_BY_NAME(vptr));
  }
  else
    amt_written1 = snprintf(g_tls.buffer,g_tls.buffer_size, "\n%s|%d|NA|NA|%s|%s|%d|%hd| | |NA|NA|NA|NA|", 
                                   nslb_get_cur_date_time(curr_time_buffer, 1), indent_level, urlId, scriptFileName, scriptLineNumber, 
                                   child_id_dt);

  va_start (ap, format);
  amt_written = vsnprintf(g_tls.buffer + amt_written1 , MAX_TRACE_LOG_BUF_SIZE - amt_written1, format, ap);
  va_end(ap);

  g_tls.buffer[g_tls.buffer_size-1] = 0;

  // If testidx is not there than it will write to the terminal else to the file
  if(testidx < 0)
    fprintf(stderr, "%s\n", g_tls.buffer);
  else
  {
    //we will write to script execution log only when debug trace is enable, but we will write in debug log.
    if(debug_trace_log_value != 0)
    {
      open_script_execution_log_file(&script_execution_log_fd);
      write(script_execution_log_fd, g_tls.buffer, (amt_written + amt_written1));
    }
  }
}
