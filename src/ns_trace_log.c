/**
 * Name             : ns_trace_log.c
 * Author           : Shri Chandra
 * Purpose          : This file contains method releated to
                        - page snap shot
 * Initial Version  : 03/11/09
 * Modification Date:

 *
**/

#include <ctype.h>
#include <regex.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <math.h>

#include "url.h"
#include "ns_search_vars.h"
#include "ns_json_vars.h"
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
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"
#include "ns_http_version.h"
#include "netstorm.h"
#include "ns_string.h"
#include "ns_cookie.h"
#include "ns_trace_log.h"
#include "nslb_util.h"
#include "ns_url_resp.h"
#include "ns_page_dump.h"
#include "ns_event_log.h"
#include "ns_url_req.h"
#include "ns_alloc.h"
#include "ns_vuser_tasks.h"
#include "ns_log_req_rep.h"
#include "nslb_time_stamp.h"
#include "divide_values.h"
#include "ns_exit.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"
#include "nslb_cav_conf.h"

//Usage for Tracing
#define trace_level_usage(err, buf, err_msg) { \
  sprintf(err_msg, "Error: Invalid value for G_TRACING keyword: %s\n %s\n", buf, err); \
  strcat(err_msg, "Usage:G_TRACING <group-name> <trace-level> <trace-destination> <trace-session> <max-trace-message-size> <trace-inline-url> <trace-session-limit> <trace-session-limit-value> (optional)\n"); \
  strcat(err_msg, "  Here trace-level is:\n"); \
  strcat(err_msg, "  0 : No trace enabled (default)\n"); \
  strcat(err_msg, "  1 : Only URL name is enabled\n"); \
  strcat(err_msg, "  2 : Only URL request response files are created\n"); \
  strcat(err_msg, "  3 : URL request, response and parameter substitution are enabled\n"); \
  strcat(err_msg, "  4 : URL request, response, parameter substitution and page dump created\n"); \
  fprintf(stderr, "%s", err_msg);\
}   

#define trace_dest_usage(err, buf, err_msg) { \
  sprintf(err_msg, "Error: Invalid value for G_TRACING keyword: %s\n %s\n", buf, err); \
  strcat(err_msg, "Usage:G_TRACING <group-name> <trace-level> <trace-destination> <trace-session> <max-trace-message-size> <trace-inline-url> <trace-session-limit> <trace-session-limit-value> (optional)\n"); \
  strcat(err_msg, "  Here trace-destination is:\n"); \
  strcat(err_msg, "  0 : Log only to standard output (screen) (default)\n"); \
  strcat(err_msg, "  1 : Log to file only\n"); \
  strcat(err_msg, "  2 : Log to both standard output and log file\n"); \
  strcat(err_msg, "  Trace destination is applicable for trace level one, for higher levels it should be option 1\n"); \
  fprintf(stderr, "%s", err_msg);\
}

#define trace_on_failure_usage(err, buf, err_msg) { \
  sprintf(err_msg, "Error: Invalid value for G_TRACING keyword: %s\n %s\n", buf, err); \
  strcat(err_msg, "Usage:G_TRACING <group-name> <trace-level> <trace-destination> <trace-session> <max-trace-message-size> <trace-inline-url> <trace-session-limit> <trace-session-limit-value> (optional)\n"); \
  strcat(err_msg, "  Here trace-session is:\n"); \
  strcat(err_msg, "  0 : Trace complete sessions\n"); \
  strcat(err_msg, "  1 : Trace only fail pages (default)\n"); \
  strcat(err_msg, "  2 : Trace complete session, if any page or transaction failing\n"); \
  fprintf(stderr, "%s", err_msg);\
}

#define trace_inline_url_usage(err, buf, err_msg) { \
  sprintf(err_msg, "Error: Invalid value for G_TRACING keyword: %s\n %s\n", buf, err); \
  strcat(err_msg, "Usage:G_TRACING <group-name> <trace-level> <trace-destination> <trace-session> <max-trace-message-size> <trace-inline-url> <trace-session-limit> <trace-session-limit-value> (optional)\n"); \
  strcat(err_msg, "  Here trace-inline-url is:\n"); \
  strcat(err_msg, "  0 : Disable inline url(default)\n"); \
  strcat(err_msg, "  1 : Enable inline url\n"); \
  fprintf(stderr, "%s", err_msg);\
}

#define trace_msg_size_usage(err, buf, err_msg) { \
  sprintf(err_msg, "Error: Invalid value for G_TRACING keyword: %s\n %s\n", buf, err); \
  strcat(err_msg, "Usage:G_TRACING <group-name> <trace-level> <trace-destination> <trace-session> <max-trace-message-size> <trace-inline-url> <trace-session-limit> <trace-session-limit-value> (optional)\n"); \
  fprintf(stderr, "%s", err_msg);\
}
/*  fprintf(stderr, "  Here max-trace-message-size is maximum size of the trace message that should be logged in case of trace level greater than 2."); 
  fprintf(stderr, "  The valid values are (0-100,000,000), default value is 4096 bytes\n"); 
*/


#define limit_mode_usage(err, buf, err_msg) \
{ \
  sprintf(err_msg, "Error: Invalid value for G_TRACING keyword: %s\n %s\n", buf, err); \
  strcat(err_msg, "Usage:G_TRACING <group-name> <trace-level> <trace-destination> <trace-session> <max-trace-message-size> <trace-inline-url> <trace-session-limit> <trace-session-limit-value> (optional)\n"); \
  strcat(err_msg, "  Here trace-session-limit is used to define how many sessions, one need to be dump\n"); \
  strcat(err_msg, "      0 : unlimited(default)\n"); \
  strcat(err_msg, "      1 : Limit defined in percentage\n");\
  strcat(err_msg, "      2 : Limit defined in number\n"); \
  strcat(err_msg, "  Where trace-session-limit-value is used to define pct in case of trace-session-limit 1 and number of sessions for trace-session-limit value is 2\n"); \
  fprintf(stderr, "%s", err_msg);\
} 

#define trace_usage(err_msg) \
{ \
  sprintf(err_msg, "Error: Invalid value for G_TRACING keyword:Invalid number of arguments\n");\
  strcat(err_msg, "Usage:G_TRACING <group-name> <trace-level> <trace-destination> <trace-session> <max-trace-message-size> <trace-inline-url> <trace-session-limit> <trace-session-limit-value> (optional)\n");\
  fprintf(stderr, "%s", err_msg);\
}


#define MAKE_RTC_MSG_FOR_TRACE_LOG(num_bytes_written) \
{\
    num_bytes_written += sprintf(err_msg, "Runtime changes applied to 'G_TRACING' for scenario group '%s'. ", grp);\
    if(prev_trace_log_level != log_level)\
    {\
      num_bytes_written += sprintf(err_msg + num_bytes_written, "New setting for trace level = %d. ", log_level);\
    }\
    if(prev_trace_log_area != log_area)\
    {\
      num_bytes_written += sprintf(err_msg + num_bytes_written, "New setting for trace area = %d. ", log_area);\
    }\
    if((prev_trace_log_limit_mode != limit_mode))\
    {\
      if((prev_trace_log_limit_mode == 1) && (limit_mode == 2)){\
        num_bytes_written += sprintf(err_msg + num_bytes_written, "New setting for trace session limit = next %.0lf sessions.", limit_mode_value);\
      }\
      else if((prev_trace_log_limit_mode == 2) && (limit_mode == 1)){\
        num_bytes_written += sprintf(err_msg + num_bytes_written, "New setting for trace session limit = %.2lf percent of remaining sessions.", pct_limit_mode_value);\
      }\
      else if((prev_trace_log_limit_mode == 1) && (limit_mode == 0)){\
        num_bytes_written += sprintf(err_msg + num_bytes_written, "New setting for trace session limit = all remaing sessions.");\
      }\
      else if((prev_trace_log_limit_mode == 2) && (limit_mode == 0)){\
        num_bytes_written += sprintf(err_msg + num_bytes_written, "New setting for trace session limit = all remaining sessions.");\
      }\
      else if((prev_trace_log_limit_mode == 0) && (limit_mode == 1)){\
        num_bytes_written += sprintf(err_msg + num_bytes_written, "New setting for trace session limit = %.2lf percent of remaing sessions.", pct_limit_mode_value);\
      }\
      else if((prev_trace_log_limit_mode == 0) && (limit_mode == 2)){\
        num_bytes_written += sprintf(err_msg + num_bytes_written, "New setting for trace session limit = next %.0lf sessions.", limit_mode_value);\
      }\
    }\
    else if(prev_limit_mode_val != limit_mode_value)\
    {\
     if(limit_mode == 1)\
     {\
       num_bytes_written += sprintf(err_msg + num_bytes_written, "New setting for trace session limit = %.2lf percent of remaing sessions.", pct_limit_mode_value);\
     }\
     else if(limit_mode == 2)\
     {\
       num_bytes_written += sprintf(err_msg + num_bytes_written, "New setting for trace session limit = next %.0lf sessions.", limit_mode_value);\
     }\
   }\
} 


// Add keyword function   
// move  kw_set_tracing() from ns_parse_scen_conf.c
int kw_set_tracing (char *buf, short *to_change_trace_level, short *to_change_max_trace_level, short *to_change_trace_dest, short *to_change_max_trace_dest, short *to_change_trace_on_fail, int *to_change_max_log_space, short *to_change_trace_inline_url, short *to_change_trace_limit_mode, double *to_change_trace_limit_mode_val, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char grp[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH];
  /*Tracing option*/
  char log_level_buf[MAX_DATA_LINE_LENGTH];
  char log_dest_buf[MAX_DATA_LINE_LENGTH];
  char log_area_buf[MAX_DATA_LINE_LENGTH];
  char log_size_buf[MAX_DATA_LINE_LENGTH];
  char log_inline_url_buf[MAX_DATA_LINE_LENGTH];
  char limit_mode_buf[MAX_DATA_LINE_LENGTH];
  char limit_mode_val_buf[MAX_DATA_LINE_LENGTH];

  int num, log_level, log_dest, log_area, log_size, log_inline_url, limit_mode;
  double limit_mode_value, pct_limit_mode_value = 0;
  int prev_trace_log_level, prev_trace_log_area, prev_trace_log_limit_mode;
  double prev_limit_mode_val;
 
  /*Setting default value: G_TRACING ALL 0 0 0 0 0 0*/
  log_level_buf[0] = '0';
  log_level_buf[1] = '\0';
  log_dest_buf[0] = '0';
  log_dest_buf[1] = '\0';
  log_area_buf[0] = '0';
  log_area_buf[1] = '\0';
  log_size_buf[0] = '0';
  log_size_buf[1] = '\0';
  log_inline_url_buf[0] = '0';
  log_inline_url_buf[1] = '\0';
  limit_mode_buf[0] = '0'; 
  limit_mode_buf[1] = '\0'; 

  NSDL4_PARSING(NULL, NULL, "Method called, buf = %s, runtime_flag = %d", buf, runtime_flag);

  num = sscanf(buf, "%s %s %s %s %s %s %s %s %s %s", keyword, grp, log_level_buf, log_dest_buf, 
               log_area_buf, log_size_buf, log_inline_url_buf, limit_mode_buf, limit_mode_val_buf, tmp);  

  NSDL2_PARSING(NULL, NULL, "Number of arguments, num = %d", num);

  if(runtime_flag)
  {
    prev_trace_log_level = *to_change_trace_level;
    prev_trace_log_area = *to_change_trace_on_fail;
    prev_trace_log_limit_mode = *to_change_trace_limit_mode;
    prev_limit_mode_val = *to_change_trace_limit_mode_val; 
    
    NSDL2_PARSING(NULL, NULL, "prev_trace_log_level = %d, prev_trace_log_area = %d, prev_trace_log_limit_mode = %d, prev_limit_mode_val = %lf", prev_trace_log_level, prev_trace_log_area, prev_trace_log_limit_mode, prev_limit_mode_val);    
  }

  /*Keyword Migration*/
  if (num < 5) {
    /*Errornous case*/
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_TRACING_USAGE, CAV_ERR_1011017, CAV_ERR_MSG_1);
  } else if ((num == 5) || (num == 6)) {
    /*Old format*/
    NSDL2_PARSING(NULL, NULL, "Old tracing format hence setting default values");
  } else if ((num == 8) || (num == 9)) {
    /*New format*/
    NSDL2_PARSING(NULL, NULL, "New tracing format");
  } else if(num > 9) {
    /*Errornous case*/
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_TRACING_USAGE, CAV_ERR_1011017, CAV_ERR_MSG_1);
  }
  
  /*Trace Log Level*/
  if (ns_is_numeric(log_level_buf) == 0) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_TRACING_USAGE, CAV_ERR_1011017, CAV_ERR_MSG_2);
  }  
  /*Converting trace log level*/
  log_level = atoi(log_level_buf);     
  /*Validate trace log level option*/
  //if ((log_level < 0) || (log_level > 5))  //future trace level
  if ((log_level < 0) || (log_level > 4)) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_TRACING_USAGE, CAV_ERR_1011017, CAV_ERR_MSG_3);
  }
#if 0
  /* In case of runtime changes we need to retain earlier value
   * Hence no need to force all fields to 0 
   * Trace level equal to 0 then return*/
  if (log_level == 0) {
    NSDL2_PARSING(NULL, NULL, "Trace log level is equal to 0 hence disabling keyword");
    *to_change_trace_level = log_level;
    *to_change_trace_dest = 0;
    *to_change_trace_on_fail = 0;
    *to_change_max_log_space = 0;
    *to_change_trace_inline_url = 0;  
    *to_change_trace_limit_mode = 0;

    NSDL2_PARSING(NULL, NULL, "log_level = %d, trace_dest = %d, log_area = %d, log_size = %d,"
                  " inline_url =%d, limit_mode = %d", log_level, *to_change_trace_dest, 
                  *to_change_trace_on_fail, *to_change_max_log_space, 
                  *to_change_trace_inline_url, *to_change_trace_limit_mode);
    return 0;
  }
#endif
  /*Destination*/
  if (ns_is_numeric(log_dest_buf) == 0) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_TRACING_USAGE, CAV_ERR_1011017, CAV_ERR_MSG_2);
  }
  /*Converting destination*/
  log_dest = atoi(log_dest_buf);

  /*Validate destination*/
  if ((log_dest < 0) || (log_dest > 2)) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_TRACING_USAGE, CAV_ERR_1011017, CAV_ERR_MSG_3);
  }
  /*Trace destination should be 1 for tracing level greater than 1*/
  if ((log_level > 1) && (log_dest != 1)) {
    NSDL2_PARSING(NULL, NULL, "Trace destination should be 1 for tracing level greater than 1");
    fprintf(stderr, "Trace destination should be 1 for tracing level greater than 1\n");
    NS_DUMP_WARNING("Tracing should be in file only for tracing URL request response or parameter substitution or page dump created");
    log_dest = 1;
  }

  /*Trace on failure*/
  if (ns_is_numeric(log_area_buf) == 0) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_TRACING_USAGE, CAV_ERR_1011017, CAV_ERR_MSG_2);
  }
  /*Converting trace on failure*/
  log_area = atoi(log_area_buf);  
  /*Validate trace on failure*/
  if ((log_area < 0) || (log_area > 3)) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_TRACING_USAGE, CAV_ERR_1011017, CAV_ERR_MSG_3);
  } 

  /*In case of trace level 1 only trace-on-failure 0,1 are applicable*/
  if ((log_level == 1) && (log_area == 2)) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_TRACING_USAGE, CAV_ERR_1011017, CAV_ERR_MSG_3);
  }
  
  /* Max trace msg size*/
  if (ns_is_numeric(log_size_buf) == 0) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_TRACING_USAGE, CAV_ERR_1011017, CAV_ERR_MSG_2);
  }
  /*Converting max trace msg size*/
  log_size = atoi(log_size_buf);
  /*Validate max trace msg size*/
  if (log_size != 0) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_TRACING_USAGE, CAV_ERR_1011017, "Value should be 0");
  }

  /*Inline URL*/
  if (ns_is_numeric(log_inline_url_buf) == 0) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_TRACING_USAGE, CAV_ERR_1011017, CAV_ERR_MSG_2);
  }
  /*Converting Inline URL*/
  log_inline_url = atoi(log_inline_url_buf);
  /*Validation for Inline URL*/
  if ((log_inline_url < 0) || (log_inline_url > 1)) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_TRACING_USAGE, CAV_ERR_1011017, CAV_ERR_MSG_3);
  }
  /*In case of trace level 1, inline url should be 0*/
  if((log_level == 1) && (log_inline_url == 1)) {
    NSDL2_PARSING(NULL, NULL, "For trace level 1, inline url should be zero, hence making inline url 0");
    fprintf(stderr, "For trace level 1, inline url should be zero, hence making inline url 0\n");
    NS_DUMP_WARNING("Inline url should be disabled for tracing only url names");
    log_inline_url = 0;
  }

  /*Validation for limit-mode*/
  if (ns_is_numeric(limit_mode_buf) == 0) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_TRACING_USAGE, CAV_ERR_1011017, CAV_ERR_MSG_2);
  }
  /*Converting limit mode*/
  limit_mode = atoi(limit_mode_buf);        
  /*Validation for limit-mode*/   
  if ((limit_mode < 0) || (limit_mode > 2)) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_TRACING_USAGE, CAV_ERR_1011017, CAV_ERR_MSG_3);
  } 
  /* limit-mode value not given*/
  if ((limit_mode != 0) && (num != 9)) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_TRACING_USAGE, CAV_ERR_1011017, CAV_ERR_MSG_1);
  }     
  /* limit-mode value given*/
  if ((limit_mode == 0) && (num == 9)) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_TRACING_USAGE, CAV_ERR_1011017, CAV_ERR_MSG_1);
  }     

  /*If log level is greater than 0 */ 
  if (log_level > 0) 
  {
    /*Force logsize to unlimited, if tracing level is greater than or equal to 3.
    if (log_level >= 3) 
      log_size = 0;*/

    /*For max-trace-message-size 0 */
    if (!log_size) log_size = 100000000;	

    if (limit_mode == 1) 
    {
      /*Validation for limit-mode value; pct*/
      if (ns_is_float(limit_mode_val_buf) == 0) {
        NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_TRACING_USAGE, CAV_ERR_1011017, CAV_ERR_MSG_2);
      }
      /*Converting pct*/
      limit_mode_value = pct_limit_mode_value = atof(limit_mode_val_buf);
      
      /*Pct allowed upto 2 decimal*/
      double pct = limit_mode_value;
      int count = 0;
      for (; pct != floor(pct); pct *= 10)  {
        NSDL2_PARSING(NULL, NULL, "pct = %lf", pct); 
        count++;
      }
      if (count > 2) {
        NSDL2_PARSING(NULL, NULL, "Invalid pct value, count = %d", count);
        NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_TRACING_USAGE, CAV_ERR_1011018, "");
      }    

      /*Validate pct for range (0.01-100)*/
      if ((limit_mode_value < -1) || (limit_mode_value > 100)) {
        NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_TRACING_USAGE, CAV_ERR_1011017, CAV_ERR_MSG_6);
      } 
      limit_mode_value = limit_mode_value * PCT_MULTIPLIER;
      if (limit_mode_value == 0) {
        NSDL2_PARSING(NULL, NULL, "Invalid pct value");
        NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_TRACING_USAGE, CAV_ERR_1011017, CAV_ERR_MSG_4);
      }   
      *to_change_trace_limit_mode_val = limit_mode_value;
    } 
    if (limit_mode == 2) {
      if (ns_is_numeric(limit_mode_val_buf) == 0) {
        NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_TRACING_USAGE, CAV_ERR_1011017, CAV_ERR_MSG_2);
      }
      limit_mode_value = atof(limit_mode_val_buf);
      
      *to_change_trace_limit_mode_val = (int)limit_mode_value;
    } 
  } 
  //Update struct 
  *to_change_trace_level = log_level;
  /* For runtime changes, we need to retain max trace level*/
  if(runtime_flag) 
  {
    if(*to_change_max_trace_level < log_level) 
    {
      NSDL2_PARSING(NULL, NULL, "Runtime changes, tracing log_level %d greater than max level %d", log_level, *to_change_max_trace_level);
      *to_change_max_trace_level = log_level;
    }
    if(*to_change_max_trace_dest < log_dest) 
    {
      NSDL2_PARSING(NULL, NULL, "Runtime changes, tracing log_dest %d greater than max dest %d", log_dest, *to_change_max_trace_dest);
      *to_change_max_trace_dest = log_dest; 
    }
    num = 0;
    MAKE_RTC_MSG_FOR_TRACE_LOG(num);
  } else {
     NSDL2_PARSING(NULL, NULL, "Init settings tracing log_level %d, log_dest %d ", log_level, log_dest);
     *to_change_max_trace_level = log_level;
     *to_change_max_trace_dest = log_dest; 
  }
  *to_change_trace_dest = log_dest;
  *to_change_trace_on_fail = log_area;
  *to_change_max_log_space = log_size;
  *to_change_trace_inline_url = log_inline_url;
  *to_change_trace_limit_mode = limit_mode;

  NSDL2_PARSING(NULL, NULL, "log_level = %d, trace_dest = %d, log_area = %d, log_size = %d,"
                " inline_url = %d, limit_mode = %d, max_trace_level = %d, max_trace_dest = %d", log_level, *to_change_trace_dest, 
                log_area, log_size, log_inline_url, limit_mode, *to_change_max_trace_level, *to_change_max_trace_dest);
  return 0;
}

#define MAX_TRACE_PARAM_VALUE_LENGTH     65536   /* 64*1024 = 64K  */
#define MAX_TRACE_PARAM_ENTRIES          64000   

static inline void tracing_limit_usage(char *err_msg)
{
  fprintf(stderr, "Error: Invalid \'G_TRACING_LIMIT\' keyword(%s)\n", err_msg);
  fprintf(stderr, "Usages:G_TRACING_LIMIT <group name> <tracing scratch buffer size> <max trace param entries> <max trace param value length>\n");
}

/*To set limits related to trace logging */
int kw_set_traceing_limit(char *buf, int *to_chagnge_max_trace_param_entries,
                                int *to_change_max_trace_param_value_size, char *err_msg)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char grp[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH];

  char max_trace_param_entries_buf[MAX_DATA_LINE_LENGTH];
  char max_trace_param_value_size_buf[MAX_DATA_LINE_LENGTH];
 
  int max_trace_param_entries;
  int max_trace_param_value_size;
   
	int num = sscanf(buf, "%s %s %s %s %s", keyword, grp, max_trace_param_entries_buf, max_trace_param_value_size_buf, tmp);  

  NSDL2_PARSING(NULL, NULL, "Number of arguments, num = %d", num);

  /*Keyword Migration*/
  if (num < 4) {
    tracing_limit_usage("Number of arguments are less than required");
    return -1;
  }
  
  if(ns_is_numeric(max_trace_param_entries_buf) == 0) {
    tracing_limit_usage("max trace param entries is non-numeric");
    return -1;
  } 
  max_trace_param_entries = atoi(max_trace_param_entries_buf);
  if(max_trace_param_entries <= 0){
    tracing_limit_usage("max trace param entries should be greater than 0");
    return -1;
  }
  if(max_trace_param_entries > MAX_TRACE_PARAM_ENTRIES) {
    fprintf(stderr, "Warning: G_TRACING_LIMIT keyword, max trace param entries(field 4) is more than max value %d, setting max value\n", MAX_TRACE_PARAM_ENTRIES);
    NS_DUMP_WARNING("Keyword G_TRACING_LIMIT has max size '%d' for trace scratch buffer, setting trace scratch buffer size to '%d'", max_trace_param_entries, MAX_TRACE_PARAM_ENTRIES);
    *to_chagnge_max_trace_param_entries = MAX_TRACE_PARAM_ENTRIES;
  } else
    *to_chagnge_max_trace_param_entries = max_trace_param_entries;

  if(ns_is_numeric(max_trace_param_value_size_buf) == 0) {
    tracing_limit_usage("max trace param value length is non-numeric");
    return -1;
  }
  max_trace_param_value_size = atoi(max_trace_param_value_size_buf);
  if(max_trace_param_value_size <= 0){
    tracing_limit_usage("max trace param value length should be greater than 0");
    return -1;
  }
  if(max_trace_param_value_size > MAX_TRACE_PARAM_VALUE_LENGTH) {
    fprintf(stderr, "Warning: G_TRACING_LIMIT keyword, max trace param value length(field 5) is more than max value %d(64 k), setting max value\n", MAX_TRACE_PARAM_VALUE_LENGTH);
    NS_DUMP_WARNING("Keyword G_TRACING_LIMIT has trace param value length '%d' that is more than max value, So setting its value to 64k", max_trace_param_value_size, MAX_TRACE_PARAM_VALUE_LENGTH);
    *to_change_max_trace_param_value_size = MAX_TRACE_PARAM_VALUE_LENGTH;
  } else
    *to_change_max_trace_param_value_size = max_trace_param_value_size;

  NSDL2_PARSING(NULL, NULL, "max_trace_param_entries = %d, max_trace_param_value_size = %d",
	*to_chagnge_max_trace_param_entries, *to_change_max_trace_param_value_size);
  return 0;
} 

// Move these two functions from netstorm.c
int get_max_tracing_level()
{
  int i;
  int max = 0;

  for (i = 0; i < total_runprof_entries; i++) {
    max = max > runprof_table_shr_mem[i].gset.max_trace_level ? max : runprof_table_shr_mem[i].gset.max_trace_level;
  }
 //printf("XXXXX trace level = %d XXXXX\n", max);
  return max;
}

int get_max_trace_dest()
{
  int i;
  int max = 0;

  for (i = 0; i < total_runprof_entries; i++) {
    max = max > runprof_table_shr_mem[i].gset.max_trace_dest ? max : runprof_table_shr_mem[i].gset.max_trace_dest;
  }

  return max;
}

int get_parameter_name_value(VUser* vptr, int used_param_id, char **name, int *name_len, int *vector_var_idx, char **value, int *value_len)
{
  usedParamTable *used_param = ((usedParamTable *)vptr->httpData->up_t.used_param) + used_param_id;
  
  SegTableEntry_Shr *seg_ptr = used_param->seg_ptr;
  VarTableEntry_Shr *var = NULL;
  *vector_var_idx = 0;
  static char other_var_name[64] = "";  //for other var which have fixed name

  NSDL2_LOGGING(vptr, NULL, "Method called seg_ptr->type = %d", seg_ptr->type);

  switch (seg_ptr->type) {
    case VAR:
    {
      NSDL2_LOGGING(vptr, NULL, "case: VAR called");
      var = get_fparam_var(vptr, -1, seg_ptr->seg_ptr.fparam_hash_code);
      *name = var->name_pointer; 
      *name_len = strlen(*name);
      break;
    }
    case INDEX_VAR:
    {
      NSDL2_LOGGING(vptr, NULL, "case: INDEX_VAR called");
      *name = seg_ptr->seg_ptr.var_ptr->name_pointer; 
      *name_len = strlen(*name);
      break;
    }
    case COOKIE_VAR:
    {
      NSDL2_LOGGING(vptr, NULL, "case: COOKIE_VAR called");
      *name = (char *)cookie_get_key(seg_ptr->seg_ptr.cookie_hash_code);
      *name_len = *name?strlen(*name):0;
      break;
    }
    case TAG_VAR:
    case NSL_VAR:
    case JSON_VAR:
    case SEARCH_VAR:
    {
      NSDL2_LOGGING(vptr, NULL, "case: SEARCH VAR called");
	    int hash_code_idx = vptr->sess_ptr->vars_rev_trans_table_shr_mem[seg_ptr->seg_ptr.var_idx];

      *name = (char *)vptr->sess_ptr->var_get_key(hash_code_idx);
      *name_len = strlen(*name);

      int val_index = (seg_ptr->data)?*(int *)seg_ptr->data:0; // Postfix of variable(i.e.how variable was used? with index, count)
  
      *vector_var_idx = val_index;
      //if no index set then check case of cur_seq
      if(!*vector_var_idx && used_param->cur_seq)
        *vector_var_idx = used_param->cur_seq;     
	    break;
    }
    case CLUST_VAR:
    {
      NSDL2_LOGGING(vptr, NULL, "case: CLUST_VAR called");
      *name = clust_name_table_shr_mem[seg_ptr->seg_ptr.var_idx];
      *name_len = strlen(*name); 
      break;
    }     
    case GROUP_VAR:
    {
      NSDL2_LOGGING(vptr, NULL, "case: GROUP_VAR called");
      *name = rungroup_name_table_shr_mem[seg_ptr->seg_ptr.var_idx];
      *name_len = strlen(*name);
      break;
    }
    case RANDOM_VAR:
    {
      NSDL2_LOGGING(vptr, NULL, "case: RANDOM_VAR called");
      *name = seg_ptr->seg_ptr.random_ptr->var_name;
      *name_len = strlen(*name);
      break;
    }
    case UNIQUE_VAR:
    {
      NSDL2_LOGGING(vptr, NULL, "case: UNIQUE_VAR called");
      *name = seg_ptr->seg_ptr.unique_ptr->var_name;
      *name_len = strlen(*name);
      break;
    }
    case RANDOM_STRING:
    {
      NSDL2_LOGGING(vptr, NULL, "case: RANDOM_STRING called");
      *name = seg_ptr->seg_ptr.random_str->var_name;
      *name_len = strlen(*name);
      break;
    }
    case DATE_VAR:
    {
      NSDL2_LOGGING(vptr, NULL, "case: DATE_VAR called");
      *name = seg_ptr->seg_ptr.date_ptr->var_name;
      *name_len = strlen(*name);
      break;
    }
    case GROUP_NAME_VAR:
    {
      NSDL2_LOGGING(vptr, NULL, "case: GROUP_NAME_VAR called");
      strcpy(other_var_name, "cav_sgroup_name");
      *name = other_var_name;
      *name_len = strlen(*name);
      break; 
    }     
    case CLUST_NAME_VAR:
    {
      NSDL2_LOGGING(vptr, NULL, "case: CLUST_NAME_VAR called");
      strcpy(other_var_name, "cav_scluster_name");
      *name = other_var_name;
      *name_len = strlen(*name);
      break; 
    }     
    case USERPROF_NAME_VAR:
    {
      NSDL2_LOGGING(vptr, NULL, "case: USERPROF_NAME_VAR called");
      strcpy(other_var_name, "cav_user_profile");
      *name = other_var_name;
      *name_len = strlen(*name);
      break; 
    }     
    case HTTP_VERSION_VAR:
    {
      NSDL2_LOGGING(vptr, NULL, "case: HTTP_VERSION_VAR called");
      strcpy(other_var_name, "cav_http_ver_var");
      *name = other_var_name;
      *name_len = strlen(*name);
      break; 
    }     
    default: 
    {
      NSDL2_LOGGING(vptr, NULL, "Default case. Tpe = %d", seg_ptr->type);
      //return seg_ptr->type;
      return -1;
    }
  }
 
  //set value 
  *value = used_param->value;
  *value_len = used_param->length;
  
  NSDL2_LOGGING(vptr, NULL, "name = %*.*s, name_len = %d, vector var index = %d, value = %*.*s, value_len = %d", *name_len, *name_len, *name, *name_len, *vector_var_idx, *value_len, *value_len, *value, *value_len);
  return seg_ptr->type;
}

#if 0
/*define characters that need to be escaped from parameter value */
#define PAGE_DUMP_ESCAPE_CHAR {'\n', '\r', '\\', '\'', ',', ';'}

//define encoder method
static inline void get_encode_str(char in, char *out_str, int *out_len)
{
  if(in == '\n')
    { strcpy(out_str, "$Cav_%09"); *out_len = 8; }
  else if(in == '\r')
    { strcpy(out_str, "$Cav_%0D"); *out_len = 8; }
  else if(in == '\\')
    { strcpy(out_str, "\\\\"); *out_len = 4; }
  else if(in == '\'')
    { strcpy(out_str, "\\'"); *out_len = 3; }
  else if(in == ',')
    { strcpy(out_str, "$Cav_%2C"); *out_len = 8; }
  else if(in == ';')
    { strcpy(out_str, "$Cav_%3B"); *out_len = 8; }
  else 
    { sprintf(out_str, "%c", in); *out_len = 1; }

}
#endif 

#define COPY_ENCODE_STR(encode_str, encode_str_len, out_loc, in_loc, out_max){\
  if((encode_str_len + out_loc) < out_max) {\
    memcpy(&out[out_loc], encode_str, encode_str_len);\
    NSDL4_LOGGING(NULL, NULL, "In = %s, Out = %*.*s, copied at = %d, encode_str_len = %d", encode_str, out_loc, out_loc, out, out_loc, encode_str_len);\
    out_loc += encode_str_len;\
    in_loc++;  \
  }\
  else \
    end = 1;\
}

/* this is like memcpy will not fill null pointer at the end 
 * this will copy 'in' buffer to 'out' with escaping special character upto either end of 'in' buffer or out_max(whatever comes first).
 * return - total character copied in out buffer
 * escaped character array can be pass in this function*/

int get_utf_8_char_size(char c)
{
   if ((c & 0x80) == 0)
     return 1;
   if ((c & 0xE0) == 0xC0)
     return 2;
   if ((c & 0xF0) == 0xE0)
     return 3;
   if ((c & 0xF8) == 0xF0)
     return 4;
   return -1;
}

int copy_and_escape(char *in, int in_len, char *out, int out_max)
{
  char binary_data_string[] = "This is binary data";
  int binary_value_len = 19;
  int out_loc = 0;
  int in_loc = 0;
  
  int encode_str_len;
  char end = 0;
  int csize;

  NSDL4_LOGGING(NULL, NULL, "Input String = %*.*s", in_len, in_len, in);

  //no check for null pointer because it is like memcpy
  while(in_loc < in_len) 
  {
    if ((csize = get_utf_8_char_size(in[in_loc])) < 0)
    {
      int max_avl_space;
      max_avl_space = (out_max<binary_value_len)?out_max:binary_value_len;
      memcpy(out, binary_data_string, max_avl_space);
      NSDL1_LOGGING(NULL, NULL, "Given data is binary data. Copied data is [%*.*s] and length is [%d]", max_avl_space, max_avl_space, out, max_avl_space);
      return max_avl_space;
    }
    NSDL4_LOGGING(NULL, NULL, "csize = %d", csize);

    if (csize == 1) //ASCII
    {
      switch(in[in_loc])
      {
        case '\n':
          encode_str_len = 8;
          COPY_ENCODE_STR("$Cav_%09", encode_str_len, out_loc, in_loc, out_max);
          break;
        case '\r':
          encode_str_len = 8;
          COPY_ENCODE_STR("$Cav_%0D", encode_str_len, out_loc, in_loc, out_max);
          break;
        case '\\':
          encode_str_len = 2;
          COPY_ENCODE_STR("\\\\", encode_str_len, out_loc, in_loc, out_max);
          break;
        case '\'':
          encode_str_len = 2;
          COPY_ENCODE_STR("\\'", encode_str_len, out_loc, in_loc, out_max);
          break;
        case '"':
          encode_str_len = 8;
          COPY_ENCODE_STR("$Cav_%22", encode_str_len, out_loc, in_loc, out_max);
          break;
        case ',':
          encode_str_len = 8;
          COPY_ENCODE_STR("$Cav_%2C", encode_str_len, out_loc, in_loc, out_max);
          break;
        case ';':
          encode_str_len = 8;
          COPY_ENCODE_STR("$Cav_%3B", encode_str_len, out_loc, in_loc, out_max);
          break;
        case '|':
          encode_str_len = 8;
          COPY_ENCODE_STR("$Cav_%7C", encode_str_len, out_loc, in_loc, out_max);
          break;
        case '&':
          encode_str_len = 8;
          COPY_ENCODE_STR("$Cav_%26", encode_str_len, out_loc, in_loc, out_max);
          break;
        case '<':
          encode_str_len = 8;
          COPY_ENCODE_STR("$Cav_%3C", encode_str_len, out_loc, in_loc, out_max);
          break;
        case '>':
          encode_str_len = 8;
          COPY_ENCODE_STR("$Cav_%3E", encode_str_len, out_loc, in_loc, out_max);
          break;
        default:
        {
          char buff[2];
          encode_str_len = 1;
          sprintf(buff, "%c", in[in_loc]);
          COPY_ENCODE_STR(buff, encode_str_len, out_loc, in_loc, out_max);
          break;
        }
      }
    }
    else //UTF-8
    {
       char *buff = &in[in_loc];
       encode_str_len = csize;
       in_loc += (csize - 1);
       COPY_ENCODE_STR(buff, encode_str_len, out_loc, in_loc, out_max);
    }

    if(out_loc >= out_max || end)
      break;    
  }
  NSDL4_LOGGING(NULL, NULL, "Output String = %*.*s, max out = %d, total copied = %d", out_loc, out_loc, out, out_max, out_loc);
  return out_loc; 
}

static int inline
get_parameter_values(VUser* vptr, char* log_space, int max_bytes , int *complete_flag) {
  int i;
  int copied_value_len;
  int max_avl_space;
  int written;
  int total_written = 0;
  int amt_left = max_bytes;
  usedParamTable *used_param = (usedParamTable *)vptr->httpData->up_t.used_param;
  int num_used_param = vptr->httpData->up_t.total_entries;
  SegTableEntry_Shr *seg_ptr;
  VarTableEntry_Shr *var = NULL;
  int max_param_value_len = runprof_table_shr_mem[vptr->group_num].gset.max_trace_param_value_size;
  char *param = NULL;

  NSDL2_LOGGING(vptr, NULL, "Method called");
#define CHANGE_AMOUNT(amount) {total_written += amount; log_space += amount; amt_left -= amount;}

  for (i = 0; i < num_used_param; i++) {
    seg_ptr = used_param[i].seg_ptr;    

    switch (seg_ptr->type) 
    {
      case VAR:
      case INDEX_VAR:
      {
        if(seg_ptr->type == VAR)
        {
          var = get_fparam_var(vptr, -1, seg_ptr->seg_ptr.fparam_hash_code);
          param = var->name_pointer;
        }
        else
        {
          param = seg_ptr->seg_ptr.var_ptr->name_pointer; 
        }

        int get_it=0;
	if (get_it) {
	  ns_log_msg(1, "sess_inst=%u, user_index=%u i= %d amt_left=%d, name=0%x\n",
	                  vptr->sess_inst, vptr->user_index, i, amt_left, param); 
        }

        // First put name of variable
       	written = snprintf(log_space, amt_left, "%s=", param); 
        log_space[amt_left] = '\0';
        NSDL4_LOGGING(vptr, NULL, "amt_left = %d, written = %d, log_space = [%s]",
                                   amt_left, written, log_space);
       	if(written < 0 || written >= amt_left) { 
          HANDLE_SNPRINT_ERRCASE(log_space, written, total_written, amt_left);
	  CHANGE_AMOUNT(written);
   	  if (get_it) {
	    ns_log_msg(1, "Exiting: sess_inst=%u, user_index=%u amt_left=%d, total_written=%d\n",
	                   vptr->sess_inst, vptr->user_index, amt_left, written);
          }
	  return total_written;
    	  } else {
	    CHANGE_AMOUNT(written);
	}
        break;
      }
      case COOKIE_VAR:
      // For Auto Cookie Mode, we are not supporting COOKIE Vars for now. So code will come
      // here only for Manual Cookies
      {
	      written = snprintf(log_space, amt_left, "%s=", cookie_get_key(seg_ptr->seg_ptr.cookie_hash_code));
        log_space[amt_left] = '\0';
        NSDL4_LOGGING(vptr, NULL, "amt_left = %d, written = %d, log_space = [%s]",
                                   amt_left, written, log_space);
	      if (written < 0 || written >= amt_left) {
          HANDLE_SNPRINT_ERRCASE(log_space, written, total_written, amt_left);
	        CHANGE_AMOUNT(written);
	        return total_written;
      	} else {
	        CHANGE_AMOUNT(written);
	      }

	      break;
      }
      case TAG_VAR:
      case NSL_VAR:
      case JSON_VAR:
      case SEARCH_VAR:
      {
        int val_index = (seg_ptr->data)?*(int *)seg_ptr->data:0;
        char val_index_buf[32] = "";
      	int hash_code_idx = vptr->sess_ptr->vars_rev_trans_table_shr_mem[seg_ptr->seg_ptr.var_idx];
        
        // to give index in case of vector parameter        
        if(val_index > 0)
          sprintf(val_index_buf, "_%d", val_index);
        else if(val_index == -1)
          strcpy(val_index_buf, "_count");
        else if(used_param[i].cur_seq > 0)
          sprintf(val_index_buf, "_%d", used_param[i].cur_seq);

      	written = snprintf(log_space, amt_left, "%s%s=", vptr->sess_ptr->var_get_key(hash_code_idx), val_index_buf);
        log_space[amt_left] = '\0';
        NSDL4_LOGGING(vptr, NULL, "amt_left = %d, written = %d, log_space = [%s]",
																		                           amt_left, written, log_space);
      	if (written < 0 || written >= amt_left) {
          HANDLE_SNPRINT_ERRCASE(log_space, written, total_written, amt_left);
      	  CHANGE_AMOUNT(written);
      	  return total_written;
      	} else {
      	  CHANGE_AMOUNT(written);
      	}
	      break;
      }
      case CLUST_VAR:
      {
	      written = snprintf(log_space, amt_left, "%s=", clust_name_table_shr_mem[seg_ptr->seg_ptr.var_idx]);
        log_space[amt_left] = '\0';
        NSDL4_LOGGING(vptr, NULL, "amt_left = %d, written = %d, log_space = [%s]",
                                   amt_left, written, log_space);
       	if (written < 0 || written >= amt_left) {
          HANDLE_SNPRINT_ERRCASE(log_space, written, total_written, amt_left);
	        CHANGE_AMOUNT(written);
	        return total_written;
	      } else {
	        CHANGE_AMOUNT(written);
        }
        break;
      }

      case GROUP_VAR:
      {
	      written = snprintf(log_space, amt_left, "%s=", rungroup_name_table_shr_mem[seg_ptr->seg_ptr.var_idx]);
        log_space[amt_left] = '\0';
        NSDL4_LOGGING(vptr, NULL, "amt_left = %d, written = %d, log_space = [%s]",
                                   amt_left, written, log_space);
	      if (written < 0 || written >= amt_left) {
          HANDLE_SNPRINT_ERRCASE(log_space, written, total_written, amt_left);
	        CHANGE_AMOUNT(written);
	        return total_written;
	      } else {
	        CHANGE_AMOUNT(written);
	      }
	      break;
      }

      case GROUP_NAME_VAR:
      {
      	written = snprintf(log_space, amt_left, "cav_sgroup_name=");
        log_space[amt_left] = '\0';
        NSDL4_LOGGING(vptr, NULL, "amt_left = %d, written = %d, log_space = [%s]",
                                   amt_left, written, log_space);
      	if (written < 0 || written >= amt_left) {
          HANDLE_SNPRINT_ERRCASE(log_space, written, total_written, amt_left);
	        CHANGE_AMOUNT(written);
	        return total_written;
	      } else {
	        CHANGE_AMOUNT(written);
	      }
	      break;
      }

      case CLUST_NAME_VAR:
      {
	      written = snprintf(log_space, amt_left, "cav_scluster_name=");
        log_space[amt_left] = '\0';
        NSDL4_LOGGING(vptr, NULL, "amt_left = %d, written = %d, log_space = [%s]",
                                   amt_left, written, log_space);
      	if (written < 0 || written >= amt_left) {
          HANDLE_SNPRINT_ERRCASE(log_space, written, total_written, amt_left);
	        CHANGE_AMOUNT(written);
	        return total_written;
	      } else {
	        CHANGE_AMOUNT(written);
      	}
	      break;
      }

      case USERPROF_NAME_VAR:
      {
	      written = snprintf(log_space, amt_left, "cav_user_profile=");
        log_space[amt_left] = '\0';
        NSDL4_LOGGING(vptr, NULL, "amt_left = %d, written = %d, log_space = [%s]",
                                   amt_left, written, log_space);
	      if (written < 0 || written >= amt_left) {
          HANDLE_SNPRINT_ERRCASE(log_space, written, total_written, amt_left);
	        CHANGE_AMOUNT(written);
	        return total_written;
	      } else {
	      CHANGE_AMOUNT(written);
     	}
	      break;
      }

      case HTTP_VERSION_VAR:
      {
        written = snprintf(log_space, amt_left, "cav_http_ver_var=");
        log_space[amt_left] = '\0';
        NSDL4_LOGGING(vptr, NULL, "amt_left = %d, written = %d, log_space = [%s]",
                                   amt_left, written, log_space);
        if (written < 0 || written >= amt_left) {
          HANDLE_SNPRINT_ERRCASE(log_space, written, total_written, amt_left);
          CHANGE_AMOUNT(written);
          return total_written;
        } else {
          CHANGE_AMOUNT(written);
        }
        break;
      }
      case RANDOM_VAR:
      {
        written = snprintf(log_space, amt_left, "%s=", seg_ptr->seg_ptr.random_ptr->var_name);
        log_space[amt_left] = '\0';
        NSDL4_LOGGING(vptr, NULL, "amt_left = %d, written = %d, log_space = [%s]",
                                   amt_left, written, log_space);
        if (written < 0 || written >= amt_left) {
          HANDLE_SNPRINT_ERRCASE(log_space, written, total_written, amt_left);
          CHANGE_AMOUNT(written);
          return total_written;
        } else {
          CHANGE_AMOUNT(written);
        }
        break;
      }
      case UNIQUE_VAR:
      {
        written = snprintf(log_space, amt_left, "%s=", seg_ptr->seg_ptr.unique_ptr->var_name);
        log_space[amt_left] = '\0';
        NSDL4_LOGGING(vptr, NULL, "amt_left = %d, written = %d, log_space = [%s]",
                                   amt_left, written, log_space);
        if (written < 0 || written >= amt_left) {
          HANDLE_SNPRINT_ERRCASE(log_space, written, total_written, amt_left);
          CHANGE_AMOUNT(written);
          return total_written;
        } else {
          CHANGE_AMOUNT(written);
        }
        break;
      }
      case RANDOM_STRING:
      {
        written = snprintf(log_space, amt_left, "%s=", seg_ptr->seg_ptr.random_str->var_name);
        log_space[amt_left] = '\0';
        NSDL4_LOGGING(vptr, NULL, "amt_left = %d, written = %d, log_space = [%s]",
                                   amt_left, written, log_space);
        if (written < 0 || written >= amt_left) {
          HANDLE_SNPRINT_ERRCASE(log_space, written, total_written, amt_left);
          CHANGE_AMOUNT(written);
          return total_written;
        } else {
          CHANGE_AMOUNT(written);
        }
        break;
      }
      case DATE_VAR:
      {
        written = snprintf(log_space, amt_left, "%s=", seg_ptr->seg_ptr.date_ptr->var_name);
        log_space[amt_left] = '\0';
        NSDL4_LOGGING(vptr, NULL, "amt_left = %d, written = %d, log_space = [%s]",
                                   amt_left, written, log_space);
        if (written < 0 || written >= amt_left) {
          HANDLE_SNPRINT_ERRCASE(log_space, written, total_written, amt_left);
          CHANGE_AMOUNT(written);
          return total_written;
        } else {
          CHANGE_AMOUNT(written);
        }
        break;
      }
  
      default:
        continue;
    }  // End Switch
    
    //write value
    //max limit to copy
    max_avl_space = (amt_left<max_param_value_len)?amt_left:max_param_value_len;
    //function will return value copied by this function
    copied_value_len = copy_and_escape(used_param[i].value, used_param[i].length, log_space, max_avl_space);
    CHANGE_AMOUNT(copied_value_len);

    if(amt_left <= 0)
      return total_written; 
    
    // Now Put ';'
    // Do not add semicolon after last parameter
    written = 0; 
    if (i < (num_used_param -1))
      written = snprintf(log_space, amt_left, ";");

    log_space[amt_left] = '\0';
    NSDL4_LOGGING(vptr, NULL, "amt_left = %d, written = %d, log_space = [%s]",
                               amt_left, written, log_space);

    if (written < 0 || written >= amt_left) {
      HANDLE_SNPRINT_ERRCASE(log_space, written, total_written, amt_left);
      CHANGE_AMOUNT(written);
      return total_written;
    } else {
      CHANGE_AMOUNT(written);
    }
  }  // End For LOOP
  *complete_flag = 1;
  return total_written;
}

/* Function used to form parameter name=value and encode parameter value
   Here we insert semicolon within name value pairs*/
int inline encode_parameter_value(char *name, char *value, char* log_space, int max_bytes , int *complete_flag, int first_parameter) 
{
  //int copied_value_len;
  //int max_avl_space;
  int written;
  int total_written = 0;
  int amt_left = max_bytes;

  NSDL1_LOGGING(NULL, NULL, "Method called, max_bytes = %d, complete_flag = %d, first_parameter = %d", max_bytes, *complete_flag, first_parameter);

  #define CHANGE_AMOUNT(amount) {total_written += amount; log_space += amount; amt_left -= amount;}
  //First put name of variable, here we need to insert semicolon as separator 
  //but do not add semicolon after last parameter
  if (!first_parameter)
    written = snprintf(log_space, amt_left, "%s=%s", name, value);
  else
    written = snprintf(log_space, amt_left, ";%s=%s", name, value);  
  log_space[amt_left] = '\0';
  NSDL4_LOGGING(NULL, NULL, "amt_left = %d, written = %d, log_space = [%s]", amt_left, written, log_space);
  if (written < 0 || written >= amt_left) {
    HANDLE_SNPRINT_ERRCASE(log_space, written, total_written, amt_left);
    CHANGE_AMOUNT(written);
    return total_written;
  }
  CHANGE_AMOUNT(written);
  //Encode variable value 
  //max limit to copy
  //max_avl_space = (amt_left < max_bytes)?amt_left:max_bytes;
  //function will return value copied by this function
  //copied_value_len = copy_and_escape(value, strlen(value), log_space, max_avl_space);
  //CHANGE_AMOUNT(copied_value_len);

  if (amt_left <= 0)
    return total_written;

  *complete_flag = 1; 
  return total_written;
}

#define FILE_SUFFIX \
sprintf(log_file, "%hd_%u_%u_%d_0_%d_%d_%d_0.dat",\
            child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,\
            vptr->group_num, GET_SESS_ID_BY_NAME(vptr),\
            GET_PAGE_ID_BY_NAME(vptr));\

//static int get_parameters( char* log_space, VUser* vptr, int max_bytes)
int get_parameters( connection *cptr, char* log_space, VUser* vptr, int max_bytes, int log_size, int *page_status_offset)
{
  //VUser* vptr = cptr->vptr; get the url_num from vptr
  int amt_written;
  int amt_left = max_bytes;
  int total_written = 0;

  NSDL1_HTTP(vptr, NULL, "Method called max_buf_len = %d log_size = %d", max_bytes, log_size);
#if 0
  /*Added for new LOG header*/
  char log_file[1024];
  int trace_url_detail = 0;
  if (amt_left) {
    /*In the page_dump.txt, Req, RepBody, and Rep fields added. So command needs PageInstance, SessId, PageId, GroupNum,
      MyPortIndex. Now these fields are write in dlog, slog and log file*/

    // Added flow name in Script field for C Type script in the format <ScriptName:FlowName>

    /* Changes done in trace log header, Added new fields, 
     * SIZE: To avoid readline call for GUI
     * 
     * Required LOG Header:
     * SessionInstance=%d; UserId=%d; Group=%s; Script=%s:%s; Page=%s; page_status=%s;PageInstance=0; Size=%d, Level=%d, FileSuffix=%s; URLStatus=%s; SessionStatus=%s              */

    FILE_SUFFIX
    if (runprof_table_shr_mem[vptr->group_num].gset.trace_level == TRACE_URL_DETAIL) {
      trace_url_detail = 1;
    }

    *page_status_offset = amt_written = snprintf(log_space, amt_left,
                "SessionInstance=%u; UserId=%u; Group=%s; Script=%s:%s; Page=%s;" 
                " PageStatus=",
                vptr->sess_inst, vptr->user_index, 
                runprof_table_shr_mem[vptr->group_num].scen_group_name,
                vptr->sess_ptr->sess_name, vptr->cur_page->flow_name?vptr->cur_page->flow_name:"NA",
                vptr->cur_page->page_name);

    HANDLE_SNPRINT_ERRCASE(log_space, amt_written, total_written, amt_left);
    amt_left -= amt_written;
    total_written = amt_written;

    amt_written = snprintf(log_space + total_written, amt_left,
                "%2.2d; PageInstance=%d; "
                "Size=%d; Level=%d; FileSuffix=%s; URLStatus=%s; SessionStatus=%s; ",
                vptr->page_status, vptr->page_instance, log_size, 
                runprof_table_shr_mem[vptr->group_num].gset.trace_level, log_file, 
                get_error_code_name(cptr->req_ok), trace_url_detail?"NA":"NF");

    HANDLE_SNPRINT_ERRCASE(log_space, amt_written, total_written, amt_left);
    amt_left -= amt_written;
    total_written += amt_written;
    NSDL3_LOGGING(vptr, NULL, "log_space = %s", log_space);
  }
  else
  {
    NSDL1_LOGGING(NULL, NULL, "No data availabe to read");
  }
#endif
  /* PageDump: In case of trace level 2 we need to dump request-response only, hence returning*/
  if (runprof_table_shr_mem[vptr->group_num].gset.trace_level <= TRACE_ONLY_REQ_RESP) {
    NSDL2_LOGGING(NULL, NULL, "Given trace-level is %d,hence returning total_written = %d", 
                  runprof_table_shr_mem[vptr->group_num].gset.trace_level, total_written);
    return total_written;
  }

  //If page failed because of connection making, parametrization substitution might not have been done
  NSDL1_HTTP(vptr, NULL, "vptr->page_status = %d, vptr->url_num = %p", vptr->page_status, vptr->url_num);
  //if (!(vptr->url_num) || (vptr->page_status == NS_REQUEST_CONFAIL ))
  if ((vptr->page_status == NS_REQUEST_CONFAIL ))
  {
      NSDL1_LOGGING(vptr, NULL, "Connection Failed");
      return total_written;
  }

  if (amt_left) {
    int complete_flag = 0;
    amt_written = get_parameter_values(vptr, log_space+total_written, amt_left, &complete_flag);
    //if complete parameter list not copied then add ... at the end
    if(!complete_flag) {
      char num_dotts = (amt_left - 3)>=0?3:amt_left;
      memset(log_space+total_written+(amt_written - num_dotts), '.', num_dotts); 
    }
    amt_left -= amt_written;
    total_written += amt_written;
    NSDL3_LOGGING(vptr, NULL, "at url: log_space = %s", log_space);
  }
  else
  {
    NSDL1_LOGGING(NULL, NULL, "No space availabe in buffer to write");
  }

  return total_written;
}

/* This function is used to do trace log which is used for
     Page snap shots
     trace logs
   This function code moved from ns_url_resp.c function do_data_processing() 
*/
void make_page_dump_buff(connection *cptr, VUser* vptr, u_ns_ts_t now, int blen, int *page_status_offset, int *total_bytes_copied)
{
  if (run_mode != NORMAL_RUN) return;
  char *log_space = ns_nvm_scratch_buf_trace;
  int max_buf_len = runprof_table_shr_mem[vptr->group_num].gset.max_trace_param_value_size; //Sctratch buff will be atleast of this size
  int bytes_to_copy;

  NSDL4_LOGGING(vptr, NULL, "Method called, vptr = %p, blen = %d, enable_rbu = %d, enable_screen_shot = %d", 
                             vptr, blen, runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_rbu, 
                             runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_screen_shot);
  
  // Since in new page dump design we don't need to read screen shot file and dump into buffer hence commaneting
  #if 0 
  /* RBU: Read screen shot file and store into buffer vptr->httpData->rbu_resp_attr->page_dump_buf 
          If screen shot file not present then make page dump by url response */

  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_rbu) 
  {
    NSDL4_LOGGING(vptr, NULL, "vptr->httpData->rbu_resp_attr = %p", vptr->httpData->rbu_resp_attr);
    vptr->httpData->rbu_resp_attr->screen_shot_file_flag = 0; // Must set to 0 for case of enable_screen_shot is not enabled 

    if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_screen_shot) 
    {
      ns_rbu_read_ss_file(vptr); 
      NSDL4_LOGGING(vptr, NULL, "screen_shot_file_flag = %d", vptr->httpData->rbu_resp_attr->screen_shot_file_flag);
      if(vptr->httpData->rbu_resp_attr->screen_shot_file_flag == 1) // File is present and read OK
        blen =  vptr->httpData->rbu_resp_attr->page_dump_buf_len; // Update blen with the size of screen shot
    }
  }
  #endif

  // Check if size is more than max limit
  if (runprof_table_shr_mem[vptr->group_num].gset.max_log_space <= blen)
    bytes_to_copy = runprof_table_shr_mem[vptr->group_num].gset.max_log_space;
  else
    bytes_to_copy = blen;

  if (runprof_table_shr_mem[vptr->group_num].gset.trace_level > TRACE_ONLY_REQ_RESP) { 
    *total_bytes_copied = get_parameters(cptr, log_space, vptr, max_buf_len, bytes_to_copy, page_status_offset);
  }
  else
  {
    *total_bytes_copied = get_parameters(cptr, log_space, vptr, max_buf_len, 0, page_status_offset);
  }
  NSDL1_LOGGING(vptr, NULL, "total_bytes_copied = %d", *total_bytes_copied);
  //if (*total_bytes_copied != 0) 
    //log_space[(*total_bytes_copied)++] = '\n';
  log_space[*total_bytes_copied] = '\0';
  NSDL4_LOGGING(vptr, NULL, "total_bytes_copied = %d, log_space = %s", *total_bytes_copied, log_space);
}

void do_trace_log(connection *cptr, VUser *vptr, int blen, char *log_space, int total_bytes_copied, int page_status_offset, u_ns_ts_t now)
{
  if (run_mode != NORMAL_RUN) return;

  int bytes_to_copy;
  char orig_file_name[1024 +1];//Added to create file name of response body 
  char docs_path[1024 +1]; 
  char url_resp_body_path[4096 + 1];
  char log_file[1024];
  char *res_body_without_orig;  
  int orig_file_name_len = 0;
  u_ns_ts_t page_end_time = 0;//Calculate page end time with respect to RBU and NON-RBU case
  u_ns_ts_t page_response_time = 0;//To calculate page respose time (page download time)

  NSDL2_LOGGING(vptr, NULL, "log_space = %*.*s, total_bytes_copied = %d", total_bytes_copied, total_bytes_copied, log_space, total_bytes_copied);
  if (runprof_table_shr_mem[vptr->group_num].gset.max_log_space <= blen)
    bytes_to_copy = runprof_table_shr_mem[vptr->group_num].gset.max_log_space;
  else
    bytes_to_copy = blen;
  
  if(full_buffer)
    full_buffer[bytes_to_copy]='\0';
  if(cptr->request_type == JRMI_REQUEST){ 
    sprintf(log_file, "jrmi_call_%hd_%u_%u_%d_0_%d_%d_%d_0",
            child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,
            vptr->group_num, vptr->sess_ptr->sess_id, vptr->cur_page->page_id);
  }else{
    sprintf(log_file, "%hd_%u_%u_%d_0_%d_%d_%d_0",
            child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,
            vptr->group_num, vptr->sess_ptr->sess_id, vptr->cur_page->page_id);
  }
#if 0
  char tmp_buf[3];
  sprintf(tmp_buf, "%2.2d", vptr->page_status);
  bcopy(tmp_buf, log_space + page_status_offset, 2);
#endif
  /*For trace level > 2 need to log response body in docs folder*/
  if (runprof_table_shr_mem[vptr->group_num].gset.trace_level > TRACE_ONLY_REQ_RESP) {
    /* RBU: Check if screen shot file is read OK. If yes, use it */
    NSDL2_LOGGING(vptr, NULL, "Page Dumping: enable_rbu = %d, enable_screen_shot = %d," 
                     "screen_shot_file_flag = %d", 
                     runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_rbu, 
                     runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_screen_shot,
                     (vptr->httpData->rbu_resp_attr != NULL)?vptr->httpData->rbu_resp_attr->screen_shot_file_flag:0);

    /* Make response body file name and pass as an argument in log_message_record2
     * nvm_id:sess_instance:script_name:page_name:page_instance*/
    sprintf(orig_file_name, "%hd:%d:%s:%s:%d.orig", child_idx, vptr->sess_inst, get_sess_name_with_proj_subproj_int(vptr->sess_ptr->sess_name, vptr->sess_ptr->sess_id, "-"), vptr->cur_page->page_name, vptr->page_instance);
    /* Write page response body into file which is stored in docs directory*/
    if (vptr->partition_idx <= 0)
      sprintf(docs_path, "logs/TR%d", testidx);
    else
      sprintf(docs_path, "logs/TR%d/%lld", testidx, vptr->partition_idx);

    NSDL2_LOGGING(NULL, NULL, "Writing response body into file = %s at docs folder = %s, ", orig_file_name, docs_path);

    if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_rbu && 
         runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_screen_shot)
    {
      NSDL2_LOGGING(vptr, NULL, "Logging page dump using RBU screen shot, page_dump_buf = %p," 
         " Page_dump_buf_len= %d", vptr->httpData->rbu_resp_attr->page_dump_buf, vptr->httpData->rbu_resp_attr->page_dump_buf_len);

      //sprintf(url_resp_body_path, "%s.jpeg", vptr->httpData->rbu_resp_attr->page_screen_shot_file);
      //Earlier we was moving screen shot to logs/TRXX at post processing. In current design we move screen shot at session end 
      //thats why we are changing url_resp_body_path here

      sprintf(url_resp_body_path, "%s/%s/rbu_logs/screen_shot/page_screen_shot_%hd_%u_%u_%d_0_%d_%d_%d_0.jpeg", 
                                   g_ns_wdir, docs_path, child_idx, vptr->user_index, vptr->sess_inst,
                                   vptr->page_instance, vptr->group_num, GET_SESS_ID_BY_NAME(vptr), GET_PAGE_ID_BY_NAME(vptr));
      NSDL2_LOGGING(vptr, NULL, "url_resp_body_path = %s", url_resp_body_path);
    }
    else if(cptr->request_type == LDAP_REQUEST || cptr->request_type == LDAPS_REQUEST)
    {
      NSDL2_LOGGING(vptr, NULL, "Logging LDAP into page dump, resp_body = %p, resp_body_size = %d", full_buffer, bytes_to_copy);
      sprintf(url_resp_body_path, "%s/ns_logs/req_rep/ldap_resp_session_%hd_%u_%u_%d_0_%d_%d_%d_0.xml",
                  docs_path, child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,
                  vptr->group_num, GET_SESS_ID_BY_NAME(vptr), GET_PAGE_ID_BY_NAME(vptr));
    } else if(cptr->request_type == JRMI_REQUEST){
      NSDL2_LOGGING(vptr, NULL, "Logging JRMI into page dump, resp_body = %p, resp_body_size = %d", full_buffer, bytes_to_copy);

      sprintf(url_resp_body_path, "%s/ns_logs/req_rep/url_rep_body_jrmi_call_%hd_%u_%u_%d_0_%d_%d_%d_0.dat",
                  docs_path, child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,
                  vptr->group_num, GET_SESS_ID_BY_NAME(vptr), GET_PAGE_ID_BY_NAME(vptr));
    }
    else
    {
      NSDL2_LOGGING(vptr, NULL, "Logging non RBU into page dump, resp_body = %p, resp_body_size = %d", full_buffer, bytes_to_copy);

      sprintf(url_resp_body_path, "%s/ns_logs/req_rep/url_rep_body_%hd_%u_%u_%d_0_%d_%d_%d_0.dat",
                  docs_path, child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,
                  vptr->group_num, GET_SESS_ID_BY_NAME(vptr), GET_PAGE_ID_BY_NAME(vptr));
    }

    //Create orig file path 
    sprintf(docs_path, "%s/page_dump/docs/%s", docs_path, orig_file_name);
    //Hard link response body with orig file
    NSDL2_LOGGING(vptr, NULL, "Making hard link from %s to %s", url_resp_body_path, docs_path);
    if ((link(url_resp_body_path, docs_path)) == -1) 
    {
      //fprintf(stderr, "Error: do_trace_log() - Unable to create hard link for orig file '%s'. %s\n", docs_path, nslb_strerror(errno));
      if((symlink(url_resp_body_path, docs_path)) == -1)
        NSDL2_LOGGING(vptr, NULL, "Error: Unable to create link %s, err =%s", docs_path, nslb_strerror(errno));
    } 
    NSDL2_LOGGING(vptr, NULL, "Created link of %s in %s", url_resp_body_path, docs_path);
    // removing the .orig file with NULL
    NSDL2_LOGGING(vptr, NULL, "orig_file_name = %s", orig_file_name);
    if ((res_body_without_orig = strrchr(orig_file_name, '.')) != NULL)
    {
      *res_body_without_orig = '\0';
      NSDL2_LOGGING(vptr, NULL, "After removing orig extension orig_file_name = %s", orig_file_name);
      orig_file_name_len = strlen(orig_file_name); 
    }
  }
  /*Calculate page response time, units should be milliseconds*/
  if (!runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_rbu) { //NON RBU
    if (vptr->pg_begin_at > now) 
      page_end_time = g_time_diff_bw_cav_epoch_and_ns_start_milisec + vptr->pg_begin_at;
    else 
      page_end_time = g_time_diff_bw_cav_epoch_and_ns_start_milisec + now;
  } else {
    page_end_time = (g_time_diff_bw_cav_epoch_and_ns_start_milisec + vptr->pg_begin_at +
                    ((vptr->httpData->rbu_resp_attr->on_load_time != -1)?vptr->httpData->rbu_resp_attr->on_load_time:0));
  }
  NSDL2_LOGGING(vptr, NULL, "Calculated page end time in milliseconds = %u", page_end_time);
  /*Response time = Page end time - Page start time*/
  page_response_time = page_end_time - (g_time_diff_bw_cav_epoch_and_ns_start_milisec + vptr->pg_begin_at);

  log_page_dump_record(vptr, child_idx, vptr->pg_begin_at, cptr->ns_component_start_time_stamp, vptr->sess_inst, vptr->sess_ptr->sess_id, vptr->cur_page->page_id, vptr->page_instance, (total_bytes_copied != 0)?log_space:NULL, total_bytes_copied, vptr->page_status, vptr->cur_page->flow_name?vptr->cur_page->flow_name:"NA", strlen(log_file), log_file, orig_file_name_len, (orig_file_name_len == 0)?NULL:orig_file_name, vptr->cur_page->page_name?vptr->cur_page->page_name:"NA", page_response_time, -1, 0, NULL, 0, NULL); //Add three future fields

}

/* initialize used parameter table for user trace and paga dump */
void init_trace_up_t(VUser *vptr) 
{
   NSDL2_HTTP(vptr, NULL,  "Method called");
 
  //check if pagedump is enabled and trace level is more than TRACE_URL_DETAIL
  if(!(((vptr->flags & NS_PAGE_DUMP_ENABLE) && runprof_table_shr_mem[vptr->group_num].gset.trace_level > TRACE_ONLY_REQ_RESP)
      ||
    (vptr->flags & NS_VUSER_TRACE_ENABLE)))
    return;
 
  MY_MALLOC(vptr->httpData->up_t.used_param, (sizeof(usedParamTable) * DELTA_USED_PARAM_TABLE_ENTRY), "vptr->httpData->up_t.used_param", -1);

  vptr->httpData->up_t.total_entries = 0;
  vptr->httpData->up_t.max_entries = DELTA_USED_PARAM_TABLE_ENTRY; 
}

void free_trace_up_t(VUser *vptr)
{
  NSDL2_HTTP(vptr, NULL, "Method called");
  
  if(!vptr->httpData->up_t.used_param)
    return;

  int i;
  usedParamTable *used_param = (usedParamTable *)vptr->httpData->up_t.used_param;
  for(i = 0; i < vptr->httpData->up_t.total_entries; i++)
  {
    if(used_param[i].flag & FREE_USED_PARAM_ENTRY) 
      FREE_AND_MAKE_NULL(used_param[i].value, "vptr->httData->up_t.used_param[i].value", -1);
  }
  FREE_AND_MAKE_NULL(vptr->httpData->up_t.used_param, "vptr->httpData->up_t.used_param", -1);
  vptr->httpData->up_t.total_entries = 0;  //Bug 33759: vptr->httpData->up_t.used_param is freed and total entries are used in 
  vptr->httpData->up_t.max_entries = 0;    //get_parameters_value function 
}


/*to compare data pointer in case of tag, search and nsl var */
#define COMPARE_VECTOR_INDEX(a,b) ((!a && !b) || (a && b && (*(int *)a == *(int *)b)))

/* This method will save used parameter's details in used_param table 
 * cur_seq is used for repeatable block will start from 1 and if 0 that means cur_seq not set */
void ns_save_used_param(VUser *vptr, SegTableEntry_Shr *seg_ptr, char *value, int value_len, char type, int malloc_flag, unsigned short cur_seq)
{
  VarTableEntry_Shr *var = NULL;
  /*In case if trace or page dump not enabled then used_param table wil not be allocated so it can be use as flag */
  if(!vptr->httpData->up_t.used_param) 
    return;

  NSDL3_VARS(vptr, NULL, "Method called");
  
  usedParamTable *used_param =  (usedParamTable *)vptr->httpData->up_t.used_param;
  char break_loop;
  
  //check if maximum entries reached in used_param
  if(vptr->httpData->up_t.total_entries == runprof_table_shr_mem[vptr->group_num].gset.max_trace_param_entries)
  {
    NSDL2_VARS(vptr, NULL, "Maximum entries (%d) reached in used_param table, Ignoring", vptr->httpData->up_t.total_entries);
    return;
  }

#define COMPARE_PARAM_WITH_REFRESH(param_ptr) {  \
  if(seg_ptr->seg_ptr.param_ptr == used_param[i].seg_ptr->seg_ptr.param_ptr) {  \
    if(seg_ptr->seg_ptr.param_ptr->refresh == SESSION)  \
      return;  \
    else      \
      break_loop = 1; \
  }   \
}
  
  //check for duplicate entries.
  int i;
  for(i = 0; i < vptr->httpData->up_t.total_entries; i++){
    break_loop = 0;
    /* compare type */
    if(seg_ptr->type != used_param[i].seg_ptr->type)
      continue;

    /* if type is same then compare index */
    switch (seg_ptr->type) {
      case VAR:    /* need to check mode */
        if(seg_ptr->seg_ptr.fparam_hash_code == used_param[i].seg_ptr->seg_ptr.fparam_hash_code) {
          var = get_fparam_var(vptr, -1, seg_ptr->seg_ptr.fparam_hash_code);
          if(var->group_ptr->type == SESSION)
            return;
           else
            break_loop = 1; 
        }  
        break;
      case INDEX_VAR:
        if(seg_ptr->seg_ptr.var_ptr == used_param[i].seg_ptr->seg_ptr.var_ptr)
          return;
        break;
      case COOKIE_VAR:
        if(seg_ptr->seg_ptr.cookie_hash_code == used_param[i].seg_ptr->seg_ptr.cookie_hash_code)
          return;
        break;
      case TAG_VAR:
      case SEARCH_VAR:
      case JSON_VAR:
      case UNIQUE_RANGE_VAR:
      case NSL_VAR:
        NSDL2_VARS(vptr, NULL, "seg_ptr->seg_ptr.var_idx = %d, saved var_idx = %d, seg_ptr->data = %d, used_param[i].seg_ptr->data = %d",
          seg_ptr->seg_ptr.var_idx, used_param[i].seg_ptr->seg_ptr.var_idx, seg_ptr->data?*(int *)seg_ptr->data:-2,  
                       used_param[i].seg_ptr->data?*(int *)used_param[i].seg_ptr->data:-2);
        if((seg_ptr->seg_ptr.var_idx == used_param[i].seg_ptr->seg_ptr.var_idx)
            &&
          (COMPARE_VECTOR_INDEX(seg_ptr->data, used_param[i].seg_ptr->data))){
          //check if index is not set and it's repeatabale block case then compare cur_seq also
          char vector_flag = vptr->sess_ptr->var_type_table_shr_mem[seg_ptr->seg_ptr.var_idx];
          int max_length = vptr->uvtable[seg_ptr->seg_ptr.var_idx].length;  //in case of vector flag 
          if(!vector_flag) return;
          else if(seg_ptr->data) return;
          //this will be case of repeatable block (if cur seq is exceeded from max length then no need to save) 
          else if((cur_seq > max_length) || (used_param[i].cur_seq == cur_seq)) return;
        }
        break;
      case RANDOM_VAR:
        COMPARE_PARAM_WITH_REFRESH(random_ptr);
        break;
      case UNIQUE_VAR:
        COMPARE_PARAM_WITH_REFRESH(unique_ptr);
        break;
      case RANDOM_STRING:
        COMPARE_PARAM_WITH_REFRESH(random_str);
        break;
      case DATE_VAR:
        COMPARE_PARAM_WITH_REFRESH(date_ptr);
        break;
      case CLUST_VAR:
      case GROUP_VAR:
        if(seg_ptr->seg_ptr.var_idx == used_param[i].seg_ptr->seg_ptr.var_idx)
          return;
        break;
      default:  //In case of other parameter no need to check seg_ptr, if type matched then return.
        return;
    }
    if(break_loop)
      break;
  }
  
  //if all test pass that means parameter is not duplicate so save in used_param table
  NSDL3_VARS(vptr, NULL, "new parameter found, saving in used_param table");
   
  /* create entry in used_param table */
	if(vptr->httpData->up_t.max_entries == vptr->httpData->up_t.total_entries) {	
		MY_REALLOC(vptr->httpData->up_t.used_param, sizeof(usedParamTable) * (vptr->httpData->up_t.max_entries + DELTA_USED_PARAM_TABLE_ENTRY), 
                                           "used_param", -1); 
		vptr->httpData->up_t.max_entries += DELTA_USED_PARAM_TABLE_ENTRY; 
    used_param = (usedParamTable *)vptr->httpData->up_t.used_param; //update used_param
	} 
 
  int seq = vptr->httpData->up_t.total_entries;
	used_param[seq].type = type;	
	used_param[seq].seg_ptr = seg_ptr;
  if(!malloc_flag) {  
	  used_param[seq].value = value;	
    used_param[seq].flag &= ~FREE_USED_PARAM_ENTRY; 
  } else  {  
    char *tmp;  /* need to malloc for these variables */
    MY_MALLOC(tmp, value_len+1, "vptr->httpData->up_t.used_param[seq].value", -1);
    memcpy(tmp, value, value_len);
    tmp[value_len] = 0;
    used_param[seq].value = tmp;
    used_param[seq].flag |= FREE_USED_PARAM_ENTRY;
  }
	used_param[seq].length = value_len;
  	used_param[seq].cur_seq = cur_seq;	
	vptr->httpData->up_t.total_entries++;	
	NSDL3_VARS(vptr, NULL, "Parameter value saved to used_param table, total entries = %d", vptr->httpData->up_t.total_entries);	
}

