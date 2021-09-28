/************************************************************************************
 * Name	     : ns_rbu_api.c (old name - ns_browser_api.c)
 * Purpose   : This file contains all the functions related to brower based testing which    
 *             called form User Context and other function which involved in NVM context are 
 *             defined in ns_rbu.c 
 * Code Flow : 
 * Author(s) : Manish Kumar Mishra
 * Date      : 14 Oct. 2013
 * Copyright : (c) Cavisson Systems
 * Modification History :
 *   SM, 27 Dec 13      : Provided SIGSTOP support to firefox to first stop loading the unloaded page and then kill it. Ref: bug 6430
 *   Manish, 7 Jan 14   : (1) Design change - removed all old APIs and RBU API will same as WEB Url API
			  (2) Add function ns_rbu_execute_page() to execute rbu pages in User Context  
 *                        (3) Renaming file name from ns_browser_api.c to ns_rbu_api.c
 *  SM, 10 Jan 14       : Changes for supporting POST body data.
 *  SM, 11-02-2014: Screen resolution and HAR file rename changes.
 *  SM, 04-04-2014: Handling for deleting POST request files from ns_logs directory.
 *  SM, 12-05-2014: Added handling for PAGELOADWAITTIME
 *  DP, 25 July 2014    : Adding support of Chrome Browser  
 ***********************************************************************************/

/* Headers */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <curl/curl.h>

#include "ns_log.h"
#include "nslb_util.h"
#include "nslb_json_parser.h"
#include "nslb_alloc.h"
#include "nslb_log.h"
#include "ns_cache_include.h"
#include "ns_vuser_thread.h"
#include "ns_url_req.h"
#include "ns_page_dump.h"
#include "ns_script_parse.h"
#include "ns_http_script_parse.h"
#include "ns_vuser_tasks.h"
#include "ns_error_codes.h"
#include "ns_http_process_resp.h" 
#include "ns_vuser_ctx.h" 
#include "ns_alloc.h"
#include "ns_rbu.h"
#include "nslb_json_parser.h"
#include "ns_rbu_api.h"
#include "url.h"
#include "ns_click_script.h"
#include "ns_network_cache_reporting.h"
#include "ns_nvm_njvm_msg_com.h"
#include "ns_child_thread_util.h"
#include "ns_vuser.h"
#include "ns_session.h"
#include "ns_rbu_page_stat.h"
#include "netstorm.h"
#include "ns_trace_level.h"
#include "ns_data_types.h"
#include "util.h"
#include "math.h"
#include "ns_exit.h"
#include "ns_rbu_domain_stat.h"
#include "ns_dynamic_avg_time.h"
#include "ns_test_monitor.h"
#include "ns_file_upload.h"

//#include "ns_rbu_access_log.h"

// During callback we need in some case few values to be saved so that it can be used later
// We will use these struct at multiple places for different purpose
typedef struct
{
  VUser *vptr;
  char renamed_har_file_with_abs_path[1024];
  char prev_har_file[RBU_MAX_HAR_FILE_LENGTH];
  int  page_status;
  time_t performance_trace_start_time;
  time_t start_time;
}web_url_end_cb;

// This struct will be used at many places not only for lighthouse
typedef struct
{
  VUser *vptr;
  time_t start_time;
}wait_for_lgh_cb;

typedef struct
{
  VUser *vptr;
  time_t start_time;
  int page_load_wait_time;
  int ret;
}wait_for_move_snap_html;

#define RBU_CREATE_FILL_WEB_END_DATA \
{\
  MY_MALLOC(web_url_end, sizeof(web_url_end_cb),  "web_url_end_callback_data", -1); \
  memset(web_url_end, 0, sizeof(web_url_end_cb)); \
  strcpy(web_url_end->renamed_har_file_with_abs_path, har_file); \
  strcpy(web_url_end->prev_har_file, prev_har_file_name); \
  web_url_end->vptr = vptr; \
  web_url_end->page_status = req_ok; \
  web_url_end->performance_trace_start_time = start_time; \
  web_url_end->start_time = 0;\
}

#define VUSER_SLEEP_WEB_URL_END(vptr, interval) \
{\
  ns_sleep(vptr->httpData->rbu_resp_attr->timer_ptr, interval, web_rbu_end_callback, (void *)web_url_end);\
  return 0; \
}

#define VUSER_SLEEP_MAKE_RBU_CONNECTION(vptr, interval)\
{\
  ns_sleep(vptr->httpData->rbu_resp_attr->timer_ptr, interval, make_rbu_connection_callback, (void *)vptr);\
}

#define VUSER_SLEEP_RBU_START_BROWSER(vptr, interval, mode) \
{\
  if(!mode) \
    ns_sleep(vptr->httpData->rbu_resp_attr->timer_ptr, interval, ns_rbu_action_after_browser_start_callback, (void *)vptr);\
  else \
    ns_sleep(vptr->httpData->rbu_resp_attr->timer_ptr, interval, make_rbu_connection_callback, (void *)vptr);\
}

#define VUSER_SLEEP_WAIT_FOR_LIGHT_HOUSE_REPORT(vptr, interval) \
{\
  wait_for_lgh_cb *wait_for_lgh; \
  MY_MALLOC(wait_for_lgh, sizeof(wait_for_lgh_cb),  "lighthousereportcallback", -1); \
  wait_for_lgh->vptr = vptr; \
  wait_for_lgh->start_time = start_time; \
  ns_sleep(vptr->httpData->rbu_resp_attr->timer_ptr, interval, wait_for_lighthouse_callback, (void *)wait_for_lgh);\
  return 0; \
}

#define VUSER_SLEEP_FIREFOX_COMMAND_AND_RUN(vptr, interval) \
{\
  ns_sleep(vptr->httpData->rbu_resp_attr->timer_ptr, interval, wait_firefox_make_run_cmd_callback, (void *)vptr);\
}

#define VUSER_SLEEP_MAKE_CLICK_CON_TO_BROWSER(vptr, interval) \
{\
  wait_for_lgh_cb *make_click_con;\
  MY_MALLOC(make_click_con, sizeof(wait_for_lgh_cb),  "click_execute_page_callback", -1); \
  make_click_con->vptr = vptr; \
  make_click_con->start_time = start_time; \
  ns_sleep(vptr->httpData->rbu_resp_attr->timer_ptr, interval, make_click_con_to_browser_callback, (void *)make_click_con);\
  return 0; \
}

#define VUSER_SLEEP_MAKE_CLICK_CON_TO_BROWSER_V2(vptr, interval) \
{\
  wait_for_lgh_cb *make_click_con;\
  MY_MALLOC(make_click_con, sizeof(wait_for_lgh_cb),  "click_execute_page_callback", -1); \
  make_click_con->vptr = vptr; \
  make_click_con->start_time = start_time; \
  ns_sleep(vptr->httpData->rbu_resp_attr->timer_ptr, interval, make_click_con_to_browser_v2_callback, (void *)make_click_con);\
  return 0; \
}

#define VUSER_SLEEP_MOVE_SNAPSHOT_TO_TR(vptr, interval) \
{\
  wait_for_move_snap_html *wait_for_move;\
  MY_MALLOC(wait_for_move, sizeof(wait_for_move_snap_html),  "move_html_snapshot", -1); \
  wait_for_move->vptr = vptr; \
  wait_for_move->start_time = start_time; \
  wait_for_move->ret = ret; \
  wait_for_move->page_load_wait_time = wait_time; \
  ns_sleep(vptr->httpData->rbu_resp_attr->timer_ptr, interval, move_snapshot_to_tr_callback, (void *)wait_for_move); \
  return 0; \
}

#define VUSER_SLEEP_MOVE_SNAPSHOT_TO_TR_V2(vptr, interval) \
{\
  web_url_end_cb *move_snap_shot_v2;\
  MY_MALLOC(move_snap_shot_v2, sizeof(web_url_end_cb),  "move_snapshot_v2", -1); \
  move_snap_shot_v2->vptr = vptr; \
  move_snap_shot_v2->start_time = start_time; \
  strcpy(move_snap_shot_v2->renamed_har_file_with_abs_path, renamed_har_file); \
  strcpy(move_snap_shot_v2->prev_har_file, prev_har_file); \
  move_snap_shot_v2->page_status = page_status; \
  move_snap_shot_v2->performance_trace_start_time = performance_trace_start_time; \
  ns_sleep(vptr->httpData->rbu_resp_attr->timer_ptr, interval, move_snapshot_to_tr_v2_callback, (void *)move_snap_shot_v2);\
  return 0; \
}

#define VUSER_SLEEP_MOVE_HTML_SNAPSHOT_TO_TR(vptr, interval) \
{\
  wait_for_move_snap_html *wait_for_snap;\
  MY_MALLOC(wait_for_snap, sizeof(wait_for_move_snap_html),  "move_html_snapshot", -1); \
  wait_for_snap->vptr = vptr; \
  wait_for_snap->start_time = start_time; \
  wait_for_snap->ret = ret;\
  wait_for_snap->page_load_wait_time = wait_time; \
  ns_sleep(vptr->httpData->rbu_resp_attr->timer_ptr, interval, move_html_snapshot_to_tr_callback, (void *)wait_for_snap);\
  return; \
}

#define HANDLE_RBU_PAGE_FAILURE(retVal) \
{\
  ContinueOnPageErrorTableEntry_Shr *ptr; \
  ptr = (ContinueOnPageErrorTableEntry_Shr *)runprof_table_shr_mem[vptr->group_num].continue_onpage_error_table[vptr->cur_page->page_number]; \
  nsi_rbu_handle_page_failure(vptr, ptr, retVal); \
}

#define HANDLE_RBU_PAGE_FAILURE_WITH_CLICK(retVal) \
{\
  ClickActionTableEntry_Shr *ca1; \
  int fret = retVal;\
  ca1 = &(clickaction_table_shr_mem[vptr->httpData->clickaction_id]); \
  char *att1[NUM_ATTRIBUTE_TYPES]; \
  memset(att1, 0, sizeof(att1)); \
  read_attributes_array_from_ca_table(vptr, att1, ca1); \
  if (!strcmp(att1[APINAME], "ns_get_num_domelement")) { \
    fret = vptr->httpData->rbu_resp_attr->num_domelement; \
  } \
  else if (!strcmp(att1[APINAME], "ns_execute_js")) { \
    fret = vptr->httpData->rbu_resp_attr->executed_js_result; \
  }\
  free_attributes_array(att1); \
  ContinueOnPageErrorTableEntry_Shr *ptr; \
  ptr = (ContinueOnPageErrorTableEntry_Shr *)runprof_table_shr_mem[vptr->group_num].continue_onpage_error_table[vptr->cur_page->page_number]; \
  nsi_rbu_handle_page_failure(vptr, ptr, fret); \
}


char *rbu_http_version_buffers[] = {"HTTP/1.0", "HTTP/1.1"};
static char filter_str[1024];

char script_execution_fail_msg[MAX_SCRIPT_EXECUTION_LOG_LENGTH + 1] = "Internal Error";
int g_rbu_lighthouse_csv_fd = -1;

/* This function only used if script is running in RBU and in user context mode */
int ns_rbu_page_think_time_as_sleep(VUser *vptr, int page_think_time)
{
  NSDL2_RBU(vptr, NULL, "Method called. vptr = %p, cptr = %p, page_think_time = %d milli-secs", vptr, vptr->last_cptr, page_think_time);

  if(page_think_time <= 0)
  {
    NSDL2_RBU(vptr, NULL, "Returning as page_think_time is 0 (page_think_time = %d)", page_think_time);
    return 0;
  }

  vptr->pg_think_time = page_think_time;
  vut_add_task(vptr, VUT_PAGE_THINK_TIME);
  NSDL2_RBU(vptr, NULL, "Waiting for think timer as sleep to be over");
  switch_to_nvm_ctx(vptr, "PageThinkTimeStart");

  return 0;
}

/*------------------------------------------------------------------- 
 * Purpose   : This function will dump RBU Http request into url_req_.. file 
 *
 * Input     : prof_name - provide profile name or ALL
 *
 * Output    : On success - 0 
 *-------------------------------------------------------------------*/
  /* Dump Url Response 
     Response File Data : 
      -----------------------------------------------------------------------
      /tiny
      HTTP/1.1 200 OK^M
      Content-Length: 2^M
      Connection: Keep-Alive^M
      ^M
      Body .....  
      -----------------------------------------------------------------------
   */
inline static void ns_rbu_debug_log_http_resp(VUser *vptr)
{
  char url_resp_file[RBU_MAX_FILE_LENGTH + 1];        /* name of file to store url response */
  char url_resp_body_file[RBU_MAX_FILE_LENGTH + 1];   /* name of file to store url rep body */
  char line_terminater[] = "\r\n";                    /* End of line */ 
  char line_break[] = "\n-----------------------------------------------------------\n"; 
  char rep_dump_buf[2014 + 1];                         /* This buffer store whole url req and at last dump into file and free it*/
  char *rep_dump_buf_ptr = NULL;                     
  FILE *url_resp_fp = NULL;
  FILE *url_resp_body_fp = NULL;
  int copyied_len = 0;
  int total_copyied_len = 0;
  int left_len = RBU_MAX_URL_REQ_DUMP_BUF_LENGTH;

  connection *cptr = vptr->last_cptr;
  RBU_RespAttr *rbu_resp_attr = vptr->httpData->rbu_resp_attr; 

  NSDL2_RBU(vptr, NULL, "vptr = %p, rbu_resp_attr = %p", vptr, rbu_resp_attr);

  RBU_NS_LOGS_PATH

  sprintf(url_resp_file, "%s/logs/%s/req_rep/url_rep_%hd_%u_%u_%d_0_%d_%d_%d_0.dat",
		      g_ns_wdir, ns_logs_file_path, child_idx, vptr->user_index,
		      vptr->sess_inst, vptr->page_instance, vptr->group_num,
		      GET_SESS_ID_BY_NAME(vptr),
		      GET_PAGE_ID_BY_NAME(vptr));

  sprintf(url_resp_body_file, "%s/logs/%s/req_rep/url_rep_body_%hd_%u_%u_%d_0_%d_%d_%d_0.dat",
		      g_ns_wdir, ns_logs_file_path, child_idx, vptr->user_index,
		      vptr->sess_inst, vptr->page_instance, vptr->group_num,
		      GET_SESS_ID_BY_NAME(vptr),
		      GET_PAGE_ID_BY_NAME(vptr));

  NSDL2_RBU(vptr, NULL, "url_resp_file = [%s], url_resp_body_file = [%s]", url_resp_file, url_resp_body_file);

  if((url_resp_fp = fopen(url_resp_file, "w+")) == NULL)
  {
    NSTL1_OUT(NULL, NULL, "Error: failed in opening file '%s'.\n", url_resp_file); 
    return ;
  }

  if((url_resp_body_fp = fopen(url_resp_body_file, "w+")) == NULL)
  {
    NSTL1_OUT(NULL, NULL, "Error: failed in opening file '%s'.\n", url_resp_body_file); 
    return ;
  }
 
  /* dump body in resp body file*/
  NSDL2_RBU(vptr, NULL, "resp_body_size = %d", rbu_resp_attr->resp_body_size);
  if(rbu_resp_attr->resp_body != NULL && rbu_resp_attr->resp_body_size > 0)
  {
    if(fwrite(rbu_resp_attr->resp_body, RBU_FWRITE_EACH_ELEMENT_SIZE, rbu_resp_attr->resp_body_size, url_resp_body_fp) <= 0)
    {
      NSTL1_OUT(NULL, NULL, "Error: writing into file '%s' failed.\n", url_resp_body_file);
      fclose(url_resp_body_fp);
      return;
    }

    fclose(url_resp_body_fp);
  }
  
  /* url resp file */
  rep_dump_buf_ptr = rep_dump_buf;
 
  //url
  copyied_len = snprintf(rep_dump_buf_ptr, left_len, "%s%s", cptr->url, line_terminater);  
  total_copyied_len += copyied_len; 
  NSDL2_RBU(vptr, NULL, "total_copyied_len = %d, rep_dump_buf = %s", total_copyied_len, rep_dump_buf);
  RBU_SET_WRITE_PTR(rep_dump_buf_ptr, copyied_len, left_len);

  //HTTP/version 200 OK
  copyied_len = snprintf(rep_dump_buf_ptr, left_len, "%s %d%s", (cptr->url_num->request_type == RBU_HTTP_REQUEST)?
			 rbu_http_version_buffers[(int)(cptr->url_num->proto.http.http_version)]:"HTTPS", rbu_resp_attr->status_code, 
			 line_terminater);  
  total_copyied_len += copyied_len; 
  NSDL2_RBU(vptr, NULL, "total_copyied_len = %d, rep_dump_buf = %s", total_copyied_len, rep_dump_buf);
  RBU_SET_WRITE_PTR(rep_dump_buf_ptr, copyied_len, left_len);

  //Content Length
  copyied_len = snprintf(rep_dump_buf_ptr, left_len, "Content-Length: %d%s", rbu_resp_attr->resp_body_size, line_terminater);  
  total_copyied_len += copyied_len; 
  NSDL2_RBU(vptr, NULL, "total_copyied_len = %d, rep_dump_buf = %s", total_copyied_len, rep_dump_buf);
  RBU_SET_WRITE_PTR(rep_dump_buf_ptr, copyied_len, left_len);
  
  copyied_len = snprintf(rep_dump_buf_ptr, left_len, "%s", line_break);  
  total_copyied_len += copyied_len; 
  NSDL2_RBU(vptr, NULL, "total_copyied_len = %d, rep_dump_buf = %s", total_copyied_len, rep_dump_buf);
  RBU_SET_WRITE_PTR(rep_dump_buf_ptr, copyied_len, left_len);

  *rep_dump_buf_ptr = '\0';

  if(fwrite(rep_dump_buf, RBU_FWRITE_EACH_ELEMENT_SIZE, total_copyied_len, url_resp_fp) <= 0)
  {
    NSTL1_OUT(NULL, NULL, "Error: writing into file '%s' failed.\n", url_resp_file);
    fclose(url_resp_fp);
    return;
  }
  
  //Body
  if(rbu_resp_attr->resp_body != NULL && rbu_resp_attr->resp_body_size > 0)
  {
    if(fwrite(rbu_resp_attr->resp_body, RBU_FWRITE_EACH_ELEMENT_SIZE, rbu_resp_attr->resp_body_size, url_resp_fp) <= 0)
    {
      NSTL1_OUT(NULL, NULL, "Error: writing into file '%s' failed.\n", url_resp_file);
      fclose(url_resp_fp);
      return;
    }
  }

  fclose(url_resp_fp);
}

/* This method will set req and rep file name to dump requests */
static inline void set_req_rep_log_filename(VUser *vptr, char *url_req_file, char *url_resp_file, char *url_resp_body_file)
{
  NSDL2_RBU(vptr, NULL, "Method called");
  int set_flag = 0;
  

//??:Where are we checking for response code(if successfull or failed).
  
#ifdef NS_DEBUG_ON
  set_flag = 1; 
#else
  connection *cptr = vptr->last_cptr;
  if((NS_IF_PAGE_DUMP_ENABLE && (runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_ALL_SESS) && 
	    (runprof_table_shr_mem[vptr->group_num].gset.trace_level > TRACE_URL_DETAIL) && 
	    ((cptr->url_num->proto.http.type == EMBEDDED_URL && runprof_table_shr_mem[vptr->group_num].gset.trace_inline_url) || 
	     (cptr->url_num->proto.http.type != EMBEDDED_URL)))) {
    set_flag = 1;
  }
#endif 

  if(set_flag == 0)
    return;

  RBU_NS_LOGS_PATH

  sprintf(url_req_file, "%s/logs/%s/req_rep/url_req_%hd_%u_%u_%d_0_%d_%d_%d_0.dat",
		      g_ns_wdir, ns_logs_file_path, child_idx, vptr->user_index,
		      vptr->sess_inst, vptr->page_instance, vptr->group_num,
		      GET_SESS_ID_BY_NAME(vptr),
		      GET_PAGE_ID_BY_NAME(vptr));

  sprintf(url_resp_file, "%s/logs/%s/req_rep/url_rep_%hd_%u_%u_%d_0_%d_%d_%d_0.dat",
		      g_ns_wdir, ns_logs_file_path, child_idx, vptr->user_index,
		      vptr->sess_inst, vptr->page_instance, vptr->group_num,
		      GET_SESS_ID_BY_NAME(vptr),
		      GET_PAGE_ID_BY_NAME(vptr));

  sprintf(url_resp_body_file, "%s/logs/%s/req_rep/url_rep_body_%hd_%u_%u_%d_0_%d_%d_%d_0.dat",
		      g_ns_wdir, ns_logs_file_path, child_idx, vptr->user_index,
		      vptr->sess_inst, vptr->page_instance, vptr->group_num,
		      GET_SESS_ID_BY_NAME(vptr),
		      GET_PAGE_ID_BY_NAME(vptr));


  NSDL3_RBU(vptr, NULL, "url_req_file = %s, url_resp_file = %s, url_resp_body_file = %s", url_req_file, url_resp_file, url_resp_body_file);
}

// "startedDateTime": "2014-06-05T14:35:38.383+05:30"
// TODO: support zone time stamp calculate (+05:30) 
long get_start_time_msec(VUser *vptr , char *time)
{
  char *start_tm_ptr = NULL; 
  char *end_tm_ptr = NULL;
  char buf[RBU_MAX_64BYTE_LENGTH + 1];
  long time_msec = 0;
  int len = 0;
  struct tm time_st;  //intermediate datastructure

  NSDL3_RBU(vptr, NULL, "Method Called, time = [%s]", time);

  //Here we are filling tm structure.see man page of strptime for more details
  if(strptime(time, "%Y-%m-%dT%T", &time_st) == NULL)
  {
    //TODO: move into error logs
    NSDL3_RBU(vptr, NULL, "Error: failed to fill structure time_st by time %s.Hence ignoring this element object.", time);
    return -1; 
  }

  time_st.tm_isdst = -1;  //for day light saving

  //mktime will convert date format into seconds 
  //TODO what if mktime fails
  time_msec = mktime(&time_st) * 1000;

  // getting millisecond part from provide time eg: 383 from 2014-06-05T14:35:38.383+05:30 
  // TODO: Handle error case if millisecond not in Time   
  start_tm_ptr = strchr(time, '.'); 
  if((end_tm_ptr = strchr(start_tm_ptr, '+')) == NULL)
    end_tm_ptr = strchr(start_tm_ptr, '-');
  
  if(start_tm_ptr == NULL || end_tm_ptr == NULL)
  {
    NSDL3_RBU(vptr, NULL, "Unable to convert Time [%s] into milliseconds ", time);
    return -1;
  }

  start_tm_ptr++;
  len = end_tm_ptr - start_tm_ptr;
  strncpy(buf, start_tm_ptr, len);  
  buf[len] = '\0';
  NSDL3_RBU(vptr, NULL, "msec = %s", buf);

  time_msec += atoi(buf);  // adding millisecond into total time.
  NSDL3_RBU(vptr, NULL, "time [%s] succefully converted into millisecond = [%ld]", time, time_msec);

  return time_msec;

}

int create_csv_data(VUser *vptr, RBU_RespAttr *rbu_resp_attr)
{
  char csv_buff[1024]="";
  float on_content_load_time_sec;
  float on_load_time_sec;
  float page_load_time_sec;
  char *csv_file_name = vptr->cur_page->first_eurl->proto.http.rbu_param.csv_file_name;
  int csv_fd = vptr->cur_page->first_eurl->proto.http.rbu_param.csv_fd;
  
  //RBU_RespAttr *rbu_resp_attr = vptr->httpData->rbu_resp_attr;

  NSDL3_RBU(vptr, NULL, "Method Called. file_name = %s, fd = %d", csv_file_name, csv_fd);
 
  //Date time calculation moved to funtion ns_rbu_get_date_time()
  //ns_rbu_get_date_time(vptr, rbu_resp_attr); 
  if (!((global_settings->protocol_enabled & RBU_API_USED ) && (vptr->sess_ptr->script_type == NS_SCRIPT_TYPE_JAVA)))
    ns_rbu_get_date_time(vptr, rbu_resp_attr); 
  else
    ns_rbu_get_date_time(vptr, NULL); 

  on_content_load_time_sec = (float) (rbu_resp_attr->on_content_load_time)/1000;
  on_load_time_sec = (float) (rbu_resp_attr->on_load_time)/1000;
  page_load_time_sec = (float) (rbu_resp_attr->page_load_time)/1000;

  NSDL3_RBU(vptr, NULL, "Date = %s, time = %s, on_content_load_time_sec = %f, on_load_time_sec = %f, page_load_time_sec =%f", 
						 rbu_resp_attr->date, rbu_resp_attr->time, on_content_load_time_sec, 
						 on_load_time_sec, page_load_time_sec);

  sprintf(csv_buff, "%s,%s,%.3f,%.3f,%.3f,%d,%d,%.3f,%.3f\n",
						 rbu_resp_attr->date, rbu_resp_attr->time, on_content_load_time_sec, 
						 on_load_time_sec, page_load_time_sec, 
						 rbu_resp_attr->request_without_cache, rbu_resp_attr->request_from_cache, 
						 rbu_resp_attr->byte_rcvd, rbu_resp_attr->byte_send);

  NSDL1_RBU(vptr, NULL,"dump csv_buffer %s", csv_buff);

  if(write(csv_fd, csv_buff, strlen(csv_buff)) == -1)
  {
    NSTL1_OUT(NULL, NULL, "Error in writing file %s, fd = %d. Error = %s", csv_file_name, csv_fd, nslb_strerror(errno));
    NS_DT1(vptr, NULL, DM_L1, MM_VARS, "Error: failed to write %s file on fd = %d.Error = %s", csv_file_name, csv_fd, nslb_strerror(errno));
    
    snprintf(rbu_resp_attr->access_log_msg, RBU_MAX_ACC_LOG_LENGTH, "Error in write %s file on fd = %d.Error = %s", 
               csv_file_name, csv_fd, nslb_strerror(errno));
    snprintf(script_execution_fail_msg, MAX_SCRIPT_EXECUTION_LOG_LENGTH, "Internal Error:Failed to dump data into csv file '%s'. Error: %s",
               csv_file_name, nslb_strerror(errno));
    return -1;
  }

  return 0;
}

static int sort_hartime_array(const void *P1, const void *P2)
{
  RBU_HARTime *p1, *p2;
  p1 = (RBU_HARTime *)P1;
  p2 = (RBU_HARTime *)P2;

  //Here we are sorting on basis of started_date_time in increasing order
  if (p1->started_date_time < p2->started_date_time)
    return -1;
  if (p1->started_date_time == p2->started_date_time)
    return 0;
  if (p1->started_date_time > p2->started_date_time)
    return 1;

  /*should not reach here*/
  return 0;
}

//This function will calculate page load time
static long get_page_load_time(RBU_RespAttr *rbu_resp_attr,  VUser *vptr, int count, int phase_interval)
{
  int i;
  long total_time = 0;

  //Commented below code. Bug-73558: Make phase_interval configurable.
  /*int phase_interval = 4000; Assuming 4000 ms is default phase intervel time, Means if two consecutive url has diff more than
			       4000 then we assume starting of new phase */
  char new_phase = 0;        //Assuming new phase 0 first time
  long start_time_msec = 0;
  long phase_last_start_time = 0;
  long on_load_time = 0;
  long min_start_time = 0;
  long max_end_time = 0;
  long end_time_msec = 0;



  NSDL2_RBU(vptr, NULL, "Method called, rbu_resp_attr->rbu_hartime = [%p], count = %d, phase_interval = %d",
                  rbu_resp_attr->rbu_hartime, count, phase_interval);

  //sort the rbu_hartime structure on start_date_time basis
  qsort(rbu_resp_attr->rbu_hartime, count, sizeof(RBU_HARTime), sort_hartime_array);

  //rbu_hartime structure is sorted now 
  //open this code only if you doubt on qsort
 
#if 0
  for(i=0; i < count; i++)
  {
    NSDL2_RBU(vptr, NULL, "sorted rbu_resp_attr->rbu_hartime[i].started_date_time = %lu, rbu_resp_attr->rbu_hartime[i].end_date_time = %lu", 
								       rbu_resp_attr->rbu_hartime[i].started_date_time, rbu_resp_attr->rbu_hartime[i].end_date_time); 
  }
#endif

   //Need to initilize onload += startedDateTime for first time ...webpagetest convention used in breaking phase
   //for making the time absolute
   if(rbu_resp_attr->on_load_time > 0)
     on_load_time = rbu_resp_attr->on_load_time +  rbu_resp_attr->rbu_hartime[0].started_date_time; 

  //resetting i for further use
  i = 0;
  //loop for traverse each startdatetime 
  //count contains number of request so we have to process each request one by one 
  while(i < count)
  {
    
    start_time_msec = rbu_resp_attr->rbu_hartime[i].started_date_time;
    end_time_msec = rbu_resp_attr->rbu_hartime[i].end_date_time;

    // New phase is started if:
    // 1) There is no phase yet.
    // 2) There is a gap between this request and the last one.
    // 3) The new request is not started during the page load.
    if(phase_interval >= 0)
      new_phase = (start_time_msec > on_load_time ) && ((start_time_msec - phase_last_start_time) >= phase_interval);

    //NSDL3_RBU(vptr, NULL, "new_phase = %d, start_time_msec =%ld, on_load_time = %ld, phase_last_start_time = %ld, end_time_msec = %ld", 
    //                                                     new_phase, start_time_msec, on_load_time, phase_last_start_time, end_time_msec);
    if(new_phase)
    {
      total_time += (max_end_time - min_start_time);
      max_end_time = 0;
      min_start_time = 0;
    }

    // Minimum of all Start times in Har file entries is set
    if(min_start_time == 0 || min_start_time > start_time_msec)
      min_start_time = start_time_msec;

    // Maximum of all End times in Har file entries is set
    if(max_end_time < end_time_msec)
      max_end_time = end_time_msec;

    phase_last_start_time = start_time_msec;
    NSDL3_RBU(vptr, NULL, "start_time_msec = %ld, end_time_msec = %ld, phase_last_start_time = %ld, total_time = %ld", 
								start_time_msec, end_time_msec, phase_last_start_time, total_time);
    i++; //increasing counter
  }
  //add last pageload time 
  //total time is page load time
  total_time += (max_end_time - min_start_time);
  NSDL3_RBU(vptr, NULL, "total_time = %ld, max_end_time = %ld, min_start_time = %ld", total_time, max_end_time, min_start_time); 

  return total_time;
}

char *domain_fields[50 + 1];
char *code_fields[30+1];
int domain_num_tokens = 0;
int status_num_tekens = 0;

/* This function that calculate cache domain */
static inline void ns_rbu_calculate_cache_domain(VUser *vptr, char *url, char *domain_cache, char *domain_status, int *total_domain_req, int *total_cache_req)
{
  char url_domain[256 + 1] = "";
  char *start_ptr, *end_ptr;
  int len = 0, i;

  NSDL3_RBU(NULL, NULL, "Method called, url = %s, domain_cache = %s, domain_status = %s, total_domain_req = %d, total_cache_req = %d", 
			 url, domain_cache, domain_status, *total_domain_req, *total_cache_req);
 
  if((start_ptr = strchr(url, '/')) == NULL)
    start_ptr = url;
  else
    start_ptr+=2;                  //eg: https://www.jcpenney.com/ww/dd/ff

  end_ptr = strchr(start_ptr, '/');
  if(end_ptr == NULL)
    end_ptr = url + strlen(url);

  len = end_ptr - start_ptr;

  strncpy(url_domain, start_ptr, len);    
  url_domain[len] = '\0';
  NSDL3_RBU(NULL, NULL, "url_domain = %s", url_domain); 

  //Resolved Bug 16950 - If G_RBU_CACHE_DOMAIN is 1 but domain_list is not provided, in that case it will take all request from domain.
  if(domain_num_tokens == 0)
  {
    *total_domain_req = *total_domain_req + 1;
    if(strcmp(domain_cache, "Origin") && (strstr(domain_status, "Hit")))
      *total_cache_req = *total_cache_req + 1;
    NSDL3_RBU(NULL, NULL, "domain_num_tokens = 0, total_domain_req = %d, total_cache_req = %d", *total_domain_req, *total_cache_req);
  }

  for(i = 0; i < domain_num_tokens; i++)
  {
    NSDL3_RBU(NULL, NULL, "url_domain = %s, Domain fields[%d] = %s, domain_cache = %s",
			   url_domain, i, domain_fields[i], domain_cache); 
    //Check whether user domain and url's domain matched or not.
    if(!strcmp(url_domain, domain_fields[i]))
    {
      *total_domain_req = *total_domain_req + 1;

      //Check the response is not served from "Origin" and it status is "Hit"
      if(strcmp(domain_cache, "Origin") && (strstr(domain_status, "Hit")))
	*total_cache_req = *total_cache_req + 1;
    }
  }
}

/* Set cache related elements */
static int set_cache_elements(VUser *vptr, RBU_RespAttr *rbu_resp_attr, nslb_json_t *json, FILE *req_fp, FILE *rep_fp, FILE *rep_body_fp, int rbu_cache_domain_enable, int total_request_counter, int *total_domain_req, int *total_cache_req, int *request_from_browser_cache, int *is_cache, int url_time, int *req_frm_browser_cache_bfr_DOM, int *req_frm_browser_cache_bfr_OnLoad, int *req_frm_browser_cache_bfr_Start_render, int *req_bfr_DOM, int *req_bfr_OnLoad, int *req_bfr_Start_render, int *temp_req_body_size, int *temp_req_hdr_size, int *tot_req_body_size, int *tot_req_hdr_size, char *temp_url)
{
  char cav_cache_provider[30 + 1] = "";
  char cav_cache_state[30 + 1] = "";
  char *ptr = NULL;
  int len;

  NSDL2_RBU(NULL, NULL, "Method called, rbu_cache_domain_enable = %d, total_request_counter = %d, total_domain_req = %d, "
			"total_cache_req = %d, request_from_browser_cache = %d, is_cache = %d, url_time = %d, "
			"req_frm_browser_cache_bfr_DOM = %d, req_frm_browser_cache_bfr_OnLoad = %d, "
			"req_frm_browser_cache_bfr_Start_render = %d, req_bfr_DOM = %d, req_bfr_OnLoad = %d, "
			"req_bfr_Start_render = %d, temp_req_body_size = %d, temp_req_hdr_size = %d, tot_req_body_size = %d, "
			"tot_req_hdr_size = %d, temp_url = %s",
			 rbu_cache_domain_enable, total_request_counter, *total_domain_req, *total_cache_req, *request_from_browser_cache, 
			 *is_cache, url_time, *req_frm_browser_cache_bfr_DOM, *req_frm_browser_cache_bfr_OnLoad, 
			 *req_frm_browser_cache_bfr_Start_render, *req_bfr_DOM, *req_bfr_OnLoad, *req_bfr_Start_render, 
			 *temp_req_body_size, *temp_req_hdr_size, *tot_req_body_size, *tot_req_hdr_size, temp_url);

  //goto & get "_cav_cache_provider" element
  GOTO_ELEMENT(json, "_cav_cache_provider");
  GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);
  strcpy(cav_cache_provider, ptr);

  //goto & get "_cav_cache_state" element
  GOTO_ELEMENT(json, "_cav_cache_state");
  GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);
  strcpy(cav_cache_state, ptr);

  NSDL2_RBU(NULL, NULL, "cav_cache_provider = %s, cav_cache_state = %s", cav_cache_provider, cav_cache_state);

  //this will set req_frm_browser_cache_bfr_DOM, req_frm_browser_cache_bfr_OnLoad & req_frm_browser_cache_bfr_Start_render
  if(!strcmp("Browser", cav_cache_provider))
  {
    if(!strcmp("Hit", cav_cache_state))
    { 
      (*request_from_browser_cache)++;
      *is_cache = 1;

      if(!rbu_cache_domain_enable)
      {
	(*total_cache_req)++;
	NSDL2_RBU(NULL, NULL, "Browser-Hit total_cache_req = %d, cav_cache_provider = %s, cav_cache_state = %s", 
			       total_cache_req, cav_cache_provider, cav_cache_state);
      }
    }

    NSDL2_RBU(NULL, NULL, "For request count = %d, url_time = %d, DOMLoad time = %d, OnLoad Time = %d", 
			   total_request_counter, url_time , rbu_resp_attr->on_content_load_time, rbu_resp_attr->on_load_time);

    //To set no. of request served from browser cache, coming before Dom event and OnLoad event are fired
    ns_rbu_req_stat(url_time, rbu_resp_attr->on_content_load_time, req_frm_browser_cache_bfr_DOM);
    ns_rbu_req_stat(url_time, rbu_resp_attr->on_load_time, req_frm_browser_cache_bfr_OnLoad);
    ns_rbu_req_stat(url_time, rbu_resp_attr->_cav_start_render_time, req_frm_browser_cache_bfr_Start_render);
  }
  else
  { 
    // If cache_provider is not "Origin" and cache_state is "HIT", counted as cache.
    if((strcmp(cav_cache_provider, "Origin")) && (strstr(cav_cache_state, "Hit")))
    {
      if(!rbu_cache_domain_enable)
      {
	(*total_cache_req)++;
	NSDL2_RBU(NULL, NULL, "total_cache_req = %d, cav_cache_provider = %s, cav_cache_state = %s",
			       *total_cache_req, cav_cache_provider, cav_cache_state);
      }
    }
    //To set no. of request, coming before Dom and onload events are fired
    ns_rbu_req_stat(url_time, rbu_resp_attr->on_content_load_time, req_bfr_DOM);
    ns_rbu_req_stat(url_time, rbu_resp_attr->on_load_time, req_bfr_OnLoad);
    ns_rbu_req_stat(url_time, rbu_resp_attr->_cav_start_render_time, req_bfr_Start_render);
  }

  //calculate req_body_size & req_hdr_size which came from cache
  if(!(*is_cache))
  {
    *tot_req_body_size += *temp_req_body_size;
    *tot_req_hdr_size += *temp_req_hdr_size;
  }

  NSDL2_RBU(NULL, NULL, "RequestStat: total_request_counter = %d, req_bfr_DOM = %d, req_frm_browser_cache_bfr_DOM = %d, "
			"req_bfr_OnLoad = %d, req_frm_browser_cache_bfr_OnLoad = %d, req_bfr_Start_render = %d, "
			"req_frm_browser_cache_bfr_Start_render = %d, tot_req_body_size = %d, tot_req_hdr_size = %d",
			 total_request_counter, *req_bfr_DOM, *req_frm_browser_cache_bfr_DOM, *req_bfr_OnLoad, 
			 *req_frm_browser_cache_bfr_OnLoad, *req_bfr_Start_render, *req_frm_browser_cache_bfr_Start_render,
			 *tot_req_body_size, *tot_req_hdr_size);

  //check whether request is served from domain or not 
  if(rbu_cache_domain_enable)
  {
    ns_rbu_calculate_cache_domain(vptr, temp_url, cav_cache_provider, cav_cache_state, total_domain_req, total_cache_req);
    NSDL2_RBU(NULL, NULL, "total_domain_req = %d, total_cache_req = %d", *total_domain_req, *total_cache_req);
  }
  return 0;
}

/* Set timings elements */
static int set_timings_elements(VUser *vptr, RBU_RespAttr *rbu_resp_attr, nslb_json_t *json, FILE *req_fp, FILE *rep_fp, FILE *rep_body_fp, char *main_url_resp_time_done, int main_url_done, int norm_id)
{
  char *ptr = NULL;
  int len = 0;
  float dns_time = 0.0;                   //time catured by entries.timings.dns
  float connect_time = 0.0;               //time catured by entries.timings.connect
  float wait_time = 0.0;                  //time catured by entries.timings.wait
  float receive_time = 0.0;               //time catured by entries.timings.receive
  float blckd_time = 0.0;                 //time catured by entries.timings.blocked
  float ssl_time = 0.0;                   //time catured by entries.timings.ssl

  //goto "timings" element
  GOTO_ELEMENT(json, "timings");

  OPEN_ELEMENT(json);

  //goto & get "dns" element
  GOTO_ELEMENT(json, "dns");
  GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);
  dns_time = (atof(ptr) > 0 ? atof(ptr) : 0);
  NSDL2_RBU(NULL, NULL, "dns_time = %f", dns_time); 

  //goto & get "connect" element
  GOTO_ELEMENT(json, "connect");
  GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);
  connect_time = (atof(ptr) > 0 ? atof(ptr) : 0);

  //goto & get "wait" element
  GOTO_ELEMENT(json, "wait");
  GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);
  wait_time = (atof(ptr) > 0 ? atof(ptr) : 0); 

  //goto & get "receive" element
  GOTO_ELEMENT(json, "receive");
  GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);
  receive_time = (atof(ptr) > 0 ? atof(ptr) : 0 );

  //goto & get "blocked" element
  GOTO_ELEMENT(json, "blocked");
  GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);
  blckd_time = (atof(ptr) > 0 ? atof(ptr) : 0);

  //goto & get "blocked" element
  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.browser_mode == 1) {
    GOTO_ELEMENT(json, "ssl");
    GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);
    ssl_time = (atof(ptr) > 0 ? atof(ptr) : 0);
  }

  NSDL2_RBU(NULL, NULL, "dns_time = %f, connect_time = %f, wait_time = %f, receive_time = %f", 
			 dns_time, connect_time, wait_time, receive_time);

  if(norm_id != -1)
  {
    rbu_domains[norm_id].dns_time += dns_time;
    rbu_domains[norm_id].connect_time += connect_time;
    rbu_domains[norm_id].wait_time += wait_time;
    rbu_domains[norm_id].rcv_time += receive_time;
    rbu_domains[norm_id].blckd_time += blckd_time;
    rbu_domains[norm_id].ssl_time += ssl_time;

    NSDL2_RBU(NULL, NULL, "Domain Stat | for norm_id = %d, dns_time = %f, tcp_time = %f, ssl_time = %f, "
                                  "connect_time = %f, wait_time = %f, receive_time = %f, blckd_time = %f", 
                                  norm_id, rbu_domains[norm_id].dns_time, rbu_domains[norm_id].tcp_time, 
                                  rbu_domains[norm_id].ssl_time, rbu_domains[norm_id].connect_time, 
                                  rbu_domains[norm_id].wait_time, rbu_domains[norm_id].rcv_time, 
                                  rbu_domains[norm_id].blckd_time);
  }

  NSDL2_RBU(NULL, NULL, "main_url_resp_time_done = %d", *main_url_resp_time_done);

  rbu_resp_attr->all_url_resp_time = (dns_time + connect_time + wait_time + receive_time);
  if((*main_url_resp_time_done) == 0 && main_url_done)
  {
    rbu_resp_attr->dns_time = dns_time;
    rbu_resp_attr->connect_time = connect_time;
    rbu_resp_attr->wait_time = wait_time ;
    rbu_resp_attr->rcv_time = receive_time;
    rbu_resp_attr->blckd_time = blckd_time;
    rbu_resp_attr->ssl_time = ssl_time;
    rbu_resp_attr->tcp_time = (connect_time - ssl_time);
    rbu_resp_attr->main_url_resp_time = rbu_resp_attr->all_url_resp_time;
    *main_url_resp_time_done = 1;
  }

  NSDL2_RBU(NULL, NULL, "URL timings | "
                        "dns_time = %f, connect_time = %f, wait_time = %f, receive_time = %f,"
                        " blocked_time = %f, ssl_time = %f, tcp_time = %f",
                         rbu_resp_attr->dns_time, rbu_resp_attr->connect_time, 
                         rbu_resp_attr->wait_time, rbu_resp_attr->rcv_time,
                         rbu_resp_attr->blckd_time, rbu_resp_attr->ssl_time, 
                         rbu_resp_attr->tcp_time); 

  NSDL2_RBU(NULL, NULL, "Main url Resp time = %f, main_url_resp_time_done = %d, all_url_resp_time = %f", 
                        rbu_resp_attr->main_url_resp_time, *main_url_resp_time_done, rbu_resp_attr->all_url_resp_time);

  //closing entries.timings{}
  CLOSE_ELEMENT_OBJ(json);
  
  return 0;
}

/* 
  This function will process har file and fill requested attributes to rbu structure.
  It will also dump url request and response.
*/
//Request file and response file are to dump headers and body.
static int ns_rbu_process_har_file(char *har_file_name, RBU_RespAttr *rbu_resp_attr, VUser *vptr, char *prof_name, char *prev_har_file_name)
{
  static short redirect_status_code[] = {301, 302, 307, 401, 407};
  nslb_json_t *json = NULL;
  char *ptr = NULL;
  int len;
  int ret;

  char score_cmd[RBU_MAX_256BYTE_LENGTH +1] = "";
  char score_buf[RBU_MAX_64BYTE_LENGTH +1] = "";
  char body_found = 0;
  FILE *cmd_fp = NULL;
  int score = 0;
  int cav_loadTime = -1;
  int phase_interval = 4000; /*Assuming 4000 ms is default phase intervel time, Means if two consecutive url has diff more than
			       4000 then we assume starting of new phase */

  /*These are to dump request and resposne to log files */
  char req_file_name[RBU_MAX_128BYTE_LENGTH + 1] = "";
  char rep_file_name[RBU_MAX_128BYTE_LENGTH + 1] = "";
  char rep_body_file_name[RBU_MAX_128BYTE_LENGTH + 1] = "";
  FILE *req_fp = NULL;
  FILE *rep_fp = NULL;
  FILE *rep_body_fp = NULL;
  long ret_date_time = 0;
  int rbu_cache_domain_enable = runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_cache_domain_mode;  

  char cavtest_req_buf[FILE_MAX_UPLOAD_SIZE + 1];
  char cavtest_res_buf[FILE_MAX_UPLOAD_SIZE + 1];
  int cav_res_written = 0;
  int cav_req_written = 0;
  char cavtest_req_file_name[512];
  char cavtest_res_file_name[512];
  char cavtest_res_body_file_name[512];

  NSDL2_RBU(vptr, NULL, "Method called, har_file_name = %s, prof_name = %s, prev_har_file_name = %s", 
			  *har_file_name?har_file_name:NULL, *prof_name?prof_name:NULL, *prev_har_file_name?prev_har_file_name:NULL);  

  if(global_settings->monitor_type == WEB_PAGE_AUDIT)
  {
    snprintf(cavtest_req_file_name, 512, "TR%s/%lld/ns_logs/req_rep/%s/url_req_%hd_%u_%u_%d_0_%d_%d_%d_0_%lld.txt",
                                          g_controller_testrun, global_settings->cavtest_partition_idx, vptr->sess_ptr->sess_name,
                                          child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance, vptr->group_num,
                                          GET_SESS_ID_BY_NAME(vptr), GET_PAGE_ID_BY_NAME(vptr),
                                          g_time_diff_bw_cav_epoch_and_ns_start_milisec + vptr->started_at);

    snprintf(cavtest_res_file_name, 512, "TR%s/%lld/ns_logs/req_rep/%s/url_rep_%hd_%u_%u_%d_0_%d_%d_%d_0_%lld.txt",
                                          g_controller_testrun, global_settings->cavtest_partition_idx, vptr->sess_ptr->sess_name,
                                          child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance, vptr->group_num,
                                          GET_SESS_ID_BY_NAME(vptr), GET_PAGE_ID_BY_NAME(vptr),
                                          g_time_diff_bw_cav_epoch_and_ns_start_milisec + vptr->started_at);

    snprintf(cavtest_res_body_file_name, 512, "TR%s/%lld/ns_logs/req_rep/%s/url_rep_body_%hd_%u_%u_%d_0_%d_%d_%d_0_%lld.txt",
                                               g_controller_testrun, global_settings->cavtest_partition_idx, vptr->sess_ptr->sess_name,
                                               child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance, vptr->group_num,
                                               GET_SESS_ID_BY_NAME(vptr), GET_PAGE_ID_BY_NAME(vptr),
                                               g_time_diff_bw_cav_epoch_and_ns_start_milisec + vptr->started_at);
    /* declare a variables */
    int fd;
    char *buffer;
    long numbytes;
    struct stat info;
 
    /* open an existing file for reading */
    if(stat(har_file_name, &info) == 0)
    { 
      /* Get the number of bytes */
      numbytes = info.st_size; 
 
      /* grab sufficient memory for the buffer to hold the text */
      NSLB_MALLOC(buffer, numbytes + 1, "local buffer", -1, NULL);
 
      /* copy all the text into the buffer */
      fd = open(har_file_name, O_RDONLY|O_CLOEXEC);
      if(read(fd, buffer, numbytes) > 0)
      { 
        /*Upload file */
        char cavtest_har_file_name[512];
        snprintf(cavtest_har_file_name, 512, "TR%s/%lld/ns_logs/req_rep/%s/P_%hd_%u_%u_%d_0_%d_%d_%d_0_%lld.har",
                                              g_controller_testrun, global_settings->cavtest_partition_idx, vptr->sess_ptr->sess_name,
                                              child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance, vptr->group_num,
                                              GET_SESS_ID_BY_NAME(vptr), GET_PAGE_ID_BY_NAME(vptr),
                                              g_time_diff_bw_cav_epoch_and_ns_start_milisec + vptr->started_at);
        ns_file_upload(cavtest_har_file_name, buffer, numbytes);
      } 
      else
      {
        NSTL1(NULL, NULL, "Failed to read, har_file = %s", har_file_name);
      }
      /* free the memory we used for the buffer */
      free(buffer);
      close(fd); 
    }
  }

  #ifndef CAV_MAIN
  //Initialize request and response log files.
  set_req_rep_log_filename(vptr, req_file_name, rep_file_name, rep_body_file_name);
  #endif

  if(*req_file_name && ((req_fp = fopen(req_file_name, "a")) == NULL)) 
  {
    NSDL2_RBU(NULL, NULL, "Error: ns_rbu_process_har_file() - failed to open request log file \'%s\'\n", req_file_name);
    NSTL1_OUT(NULL, NULL, "Error: ns_rbu_process_har_file() - failed to open request log file \'%s\'\n", req_file_name);
    strncpy(rbu_resp_attr->access_log_msg, "Error: Failed to open request log file", RBU_MAX_ACC_LOG_LENGTH);
    snprintf(script_execution_fail_msg, MAX_SCRIPT_EXECUTION_LOG_LENGTH, "Internal Error:Failed to open request log file \'%s\'", req_file_name);
    RETURN(-1);
  }

  if(*rep_file_name && ((rep_fp = fopen(rep_file_name, "a")) == NULL)) 
  {
    NSDL2_RBU(NULL, NULL, "Error: ns_rbu_process_har_file() - failed to open response log file \'%s\'\n", rep_file_name);
    NSTL1_OUT(NULL, NULL, "Error: ns_rbu_process_har_file() - failed to open response log file \'%s\'\n", rep_file_name);
    strncpy(rbu_resp_attr->access_log_msg, "Error: Failed to open response log file", RBU_MAX_ACC_LOG_LENGTH);
    snprintf(script_execution_fail_msg, MAX_SCRIPT_EXECUTION_LOG_LENGTH, "Internal Error:Failed to open response log file \'%s\'", rep_file_name);
    RETURN(-1);
  }

  if(!har_file_name || *har_file_name == '\0')
  {
    NSDL2_RBU(vptr, NULL, "Error: ns_rbu_process_har_file() - HAR file name NOT supplied\n");
    NSTL1_OUT(NULL, NULL, "Error: ns_rbu_process_har_file() - HAR file name NOT supplied\n");
    strncpy(rbu_resp_attr->access_log_msg, "Error: HAR file name NOT supplied", RBU_MAX_ACC_LOG_LENGTH);
    strncpy(script_execution_fail_msg, "Internal Error:HAR file name NOT supplied.", MAX_SCRIPT_EXECUTION_LOG_LENGTH);
    RETURN(-1);
  }

  if(access(har_file_name, F_OK) == -1)
  {
    NSDL2_RBU(vptr, NULL, "Error: ns_rbu_process_har_file() - HAR file \'%s\' does not exist, error: %s\n", har_file_name, nslb_strerror(errno));
    NSTL1_OUT(NULL, NULL, "Error: ns_rbu_process_har_file() - HAR file \'%s\' does not exist, error: %s\n", har_file_name, nslb_strerror(errno));
    strncpy(rbu_resp_attr->access_log_msg, "Error: HAR file does not exist", RBU_MAX_ACC_LOG_LENGTH);
    snprintf(script_execution_fail_msg, MAX_SCRIPT_EXECUTION_LOG_LENGTH, "Internal Error:Page is not loaded completely in browser.");
    RETURN(-1);
  }

  nslb_json_error err;

  //Load har file
  json = nslb_json_init(har_file_name, 0, 0, &err);
  if(!json) {
    NSTL1_OUT(NULL, NULL, "Error: ns_rbu_process_har_file() - failed to open file %s, error = %s\n", har_file_name, err.str);
    NSDL2_RBU(NULL, NULL, "Error: ns_rbu_process_har_file() - failed to open file %s, error = %s\n", har_file_name, err.str);
    strncpy(rbu_resp_attr->access_log_msg, "Error: Failed to open HAR file", RBU_MAX_ACC_LOG_LENGTH);
    snprintf(script_execution_fail_msg, MAX_SCRIPT_EXECUTION_LOG_LENGTH, "Internal Error:Failed to open HAR file, Error: %s", err.str);
    RETURN(-1);
  }

  //Count domain list, provided in G_RBU_CACHE_DOMAIN
  NSDL2_RBU(vptr, NULL, "rbu_cache_domain_mode = %d", rbu_cache_domain_enable);
  if(rbu_cache_domain_enable)
  {
    //Tokenise domain list 
    domain_num_tokens = get_tokens(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.domain_list, domain_fields, ";",  51);
    NSDL3_RBU(vptr, NULL, "domain_num_tokens = %d", domain_num_tokens);
  }


  NSDL2_RBU(vptr, NULL, "Calculation for Page Score, retry_count = %d", rbu_resp_attr->retry_count);
  sprintf(score_cmd, "nsu_rbu_page_speed \"%s\" 2>&1 </dev/null", har_file_name);
  NSDL3_RBU(vptr, NULL, "score_cmd = %s", score_cmd);
  // Since we are in callback - so there is no need to call popen command again to get page score
  if(!rbu_resp_attr->retry_count)
  {
    rbu_resp_attr->retry_count = 1;
    cmd_fp = nslb_popen(score_cmd, "re");
    if(cmd_fp != NULL)
    {
      if(fgets(score_buf, RBU_MAX_64BYTE_LENGTH, cmd_fp) != NULL)
      {
        score = atoi(score_buf);
        NSDL2_RBU(vptr, NULL, "page score = %d", score);
        rbu_resp_attr->pg_speed = score;
      }
      else
        NSDL2_RBU(vptr, NULL, "fgets() return status is NULL for Page Score Calculation");

      //Closing cmd_fp here 
      pclose(cmd_fp);
    }
    else
      NSDL2_RBU(vptr, NULL, "popen() fails while processing 'nsu_rbu_page_speed' and error is = %s", nslb_strerror(errno));
  }
  //open the root object.
  OPEN_ELEMENT(json);
  
  //log is root element, looking for log
  GOTO_ELEMENT(json, "log");

  //go inside log
  OPEN_ELEMENT(json);

  //pages is an element inside log, look for log-->pages
  GOTO_ELEMENT(json, "pages");

  //go inside pages
  OPEN_ELEMENT(json);

  //as pages is an array, go inside array
  if(nslb_json_next_array_item(json) == -1) 
  {
    rbu_resp_attr->is_incomplete_har_file = 1;
    
    //Case of incomplete HAR i.e., only format we have
    //Handling T.O. Err, url end and context at ns_rbu_execute_page()
    NSDL2_RBU(vptr, NULL, "num_retry_on_page_failure - %d and retry_count_on_abort - %d", 
                            runprof_table_shr_mem[vptr->group_num].gset.num_retry_on_page_failure, vptr->retry_count_on_abort);

    if(runprof_table_shr_mem[vptr->group_num].gset.num_retry_on_page_failure > vptr->retry_count_on_abort) {
      snprintf(rbu_resp_attr->access_log_msg, RBU_MAX_ACC_LOG_LENGTH, "Error: har file '%s' is not loaded completely downloaded. "
                                             "Hence Session Aborted and going to Retry", har_file_name);
      RETURN(-2);
    }

    NSTL1_OUT(NULL, NULL, "Error: ns_rbu_process_har_file() - failed to array open \'pages\', error = %s", nslb_json_strerror(json));
    NSDL2_RBU(vptr, NULL, "Error: ns_rbu_process_har_file() - failed to array open \'pages\', error = %s", nslb_json_strerror(json));
    strncpy(rbu_resp_attr->access_log_msg, "Error: Failed to open array of HAR file, HAR is incomplete", RBU_MAX_ACC_LOG_LENGTH);
    strncpy(script_execution_fail_msg, "Error:Page is not loaded completely in browser.",MAX_SCRIPT_EXECUTION_LOG_LENGTH);
    RETURN(-1);
  }

  //go inside array
  OPEN_ELEMENT(json);

  //goto & get "startedDateTime" element
  GOTO_ELEMENT(json, "startedDateTime");
  GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);
  snprintf(rbu_resp_attr->date_time_str, RBU_MAX_64BYTE_LENGTH, "%s", ptr);

  if((ret_date_time = get_start_time_msec(vptr, rbu_resp_attr->date_time_str)) == -1)
  {
    NSTL1_OUT(NULL, NULL, "Error: ns_rbu_process_har_file() -failed to compute main_url_start_date_time\n");
    NSDL2_RBU(NULL, NULL, "Error: ns_rbu_process_har_file() -failed to compute main_url_start_date_time");
    NS_EL_2_ATTR(EID_FOR_API, vptr->user_index, vptr->sess_inst, EVENT_API, 4,
			      vptr->sess_ptr->sess_name, vptr->cur_page->page_name, 
			      "Error in computing main_url_start_date_time %s", rbu_resp_attr->date_time_str);
    strncpy(rbu_resp_attr->access_log_msg, "Error: Failed to calculate start date time of Main URL", RBU_MAX_ACC_LOG_LENGTH);
    strncpy(script_execution_fail_msg, "Internal Error:Failed to calculate start date time of Main URL", MAX_SCRIPT_EXECUTION_LOG_LENGTH);
    RETURN(-1);
  }
  //rbu_resp_attr->main_url_start_date_time = (unsigned long long) ret_date_time;

  //calculating epoch time for main_url_start_date_time
  rbu_resp_attr->main_url_start_date_time = (((unsigned long long) ret_date_time) / 1000) - global_settings->unix_cav_epoch_diff;

  NSDL2_RBU(vptr, NULL, "main_url_start_date_time = %lld in epoch and %lld in milisecond ",
                           rbu_resp_attr->main_url_start_date_time, ret_date_time);

  //goto & get "_cav_dom_element" 
  GOTO_ELEMENT(json, "_cav_dom_element");
  GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);
  rbu_resp_attr->dom_element = atoi(ptr);
  
  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.browser_mode != RBU_BM_FIREFOX)
  {
    //goto & get "_cav_loadTime"
    GOTO_ELEMENT(json, "_cav_loadTime");
    GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);
    cav_loadTime = atoi(ptr);
 
    //goto & get "_cav_ignore_har" 
    GOTO_ELEMENT(json, "_cav_ignore_har");
    GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);

    if(cav_loadTime > 0)
    {
      rbu_resp_attr->page_load_time = cav_loadTime;
      rbu_resp_attr->status_code = 200;
      
      NSDL2_RBU(vptr, NULL, "_cav_loadTime found, Setting page_load_time = %d.", rbu_resp_attr->page_load_time);
    }
    else if(atoi(ptr) == 1)
    {
      rbu_resp_attr->is_incomplete_har_file = 1;
      /* _cav_ignore_har will set as 1 in case of click action to ignore HAR if no request is send.
         Mark this page as Success as click action is done successfully. */
      rbu_resp_attr->status_code = 200;
 
      strncpy(rbu_resp_attr->access_log_msg, "Ignoring HAR as no request is send.", RBU_MAX_ACC_LOG_LENGTH);
      NSDL2_RBU(vptr, NULL, "Ignoring HAR as no request is send.");
 
      RETURN(1);
    }
    
    //goto & get "_cav_phase_interval" 
    GOTO_ELEMENT(json, "_cav_phase_interval");
    GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);
    phase_interval = atoi(ptr);

    //goto & get "_cav_page_status"
    GOTO_ELEMENT(json, "_cav_page_status");
    GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);
    rbu_resp_attr->pg_status = atoi(ptr);
  }

  //goto pageTimings block
  GOTO_ELEMENT(json, "pageTimings");

  //open pageTimings block
  OPEN_ELEMENT(json);

  //goto & get "onContentLoad" element
  GOTO_ELEMENT(json, "onContentLoad");
  GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);
  rbu_resp_attr->on_content_load_time = atoi(ptr);

  //goto & get "onLoad" element
  GOTO_ELEMENT(json, "onLoad");
  GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);
  rbu_resp_attr->on_load_time = atoi(ptr);
  
  //goto & get "_cav_startRender" element
  GOTO_ELEMENT(json, "_cav_startRender");
  GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);
  rbu_resp_attr->_cav_start_render_time = atoi(ptr);

  //goto & get "_cav_endRender" element
  GOTO_ELEMENT(json, "_cav_endRender");
  GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);
  rbu_resp_attr->_cav_end_render_time = atoi(ptr);

  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.browser_mode == RBU_BM_CHROME)
  {
    //goto & get "_firstPaint" element
    GOTO_ELEMENT(json, "_firstPaint");
    GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);
    rbu_resp_attr->first_paint = atoi(ptr);

    //goto & get "_firstContentfulPaint" element
    GOTO_ELEMENT(json, "_firstContentfulPaint");
    GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);
    rbu_resp_attr->first_contentful_paint = atoi(ptr);

    //goto & get "_largestContentfulPaint" element
    GOTO_ELEMENT(json, "_largestContentfulPaint");
    GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);
    rbu_resp_attr->largest_contentful_paint = atoi(ptr);

    //goto & get "_cumulativeLayoutShift" element
    GOTO_ELEMENT(json, "_cumulativeLayoutShift");
    GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);
    rbu_resp_attr->cum_layout_shift = atof(ptr);

    //goto & get "_totalBlockingTime" element
    GOTO_ELEMENT(json, "_totalBlockingTime");
    GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);
    rbu_resp_attr->total_blocking_time = atoi(ptr);

    //goto & get "_tti" element
    GOTO_ELEMENT(json, "_tti");
    GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);
    rbu_resp_attr->_tti_time = atoi(ptr);

    NSDL3_RBU(vptr, NULL, "first_paint = %d, first_contentful_paint = %d, largest_contentful_paint = %d, cum_layout_shift = %f, "
                          "total_blocking_time = %d, tti_time = %d", rbu_resp_attr->first_paint, rbu_resp_attr->first_contentful_paint,
                           rbu_resp_attr->largest_contentful_paint, rbu_resp_attr->cum_layout_shift, rbu_resp_attr->total_blocking_time,
                           rbu_resp_attr->_tti_time);
  }

  //goto & get "_tti" element
  if((runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.browser_mode != RBU_BM_CHROME) &&
     (runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.tti_mode == 1))
  {
    GOTO_ELEMENT(json, "_tti");
    GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);
    rbu_resp_attr->_tti_time = atoi(ptr);
  }

  //close pages[]
  CLOSE_ELEMENT_OBJ(json);  //close pageTimings '}' object

  // Handling for makr and measure. 
  int mark_and_measure_count = 0;
  int mm_count = 0, j;  
  RBU_MarkMeasure markMeasure;
  RBU_MarkMeasure *rbu_mark_measures = NULL; 
  char *elementName;

  for (j = 0; j < 2; j++)
  {
    elementName = j == 0 ? "_marks": "_measures";
    //first get count so data can be malloced. 
    if (nslb_json_goto_element(json, elementName) == 0) {
      //open array and check for length. 
      OPEN_ELEMENT(json);

      // check number of marks.
      while (nslb_json_next_array_item(json) != -1) {
        mark_and_measure_count++; 
      } 

      if (json->error != NSLB_JSON_ARR_END) {
        fprintf(stderr, "Error: ns_rbu_process_har_file() - failed to parse marks in HAR, error = %s, line = %d\n", nslb_json_strerror(json), __LINE__); 
        RETURN(-1);	 
      }

      CLOSE_ELEMENT_ARR(json); // close _marks array.
    }
  }

  NSDL3_RBU(vptr, NULL, "Total mark and measures - %d", mark_and_measure_count);
  
  if (mark_and_measure_count) {
  MY_MALLOC(rbu_mark_measures, mark_and_measure_count * sizeof(RBU_MarkMeasure), "rbu_mark_measures", -1); 

  for (j = 0; j < 2; j++) {
    elementName = j == 0 ? "_marks": "_measures";
    // again open the array and fill the value. 
    //first get count so data can be malloced. 
    if (nslb_json_goto_element(json, elementName) == 0) {
      //open array and check for length. 
      OPEN_ELEMENT(json);

      // check number of marks.
      while (nslb_json_next_array_item(json) != -1) {
         OPEN_ELEMENT(json); // open mark entry 

         GOTO_ELEMENT(json, "name"); 

         GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);
 
         //copy mark name. 
         len = len > 127 ? 127: len;
         strncpy(markMeasure.name, ptr, len);
         markMeasure.name[len] = 0;

         markMeasure.type = j == 0? RBU_MARK_TYPE: RBU_MEASURE_TYPE;
       
         GOTO_ELEMENT(json, "startTime");
         GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);

         markMeasure.startTime = atoi(ptr); 

         GOTO_ELEMENT(json, "duration");
         GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);
         markMeasure.duration = atoi(ptr);

         CLOSE_ELEMENT_OBJ(json); // close mark/measure object

         //copy to array.
         memcpy(&rbu_mark_measures[mm_count++], &markMeasure, sizeof(markMeasure)); 
      } 

      if (json->error != NSLB_JSON_ARR_END) {
        fprintf(stderr, "Error: ns_rbu_process_har_file() - failed to parse marks in HAR, error = %s, line = %d\n", nslb_json_strerror(json), __LINE__); 
        RETURN(-1);	 
      }

      CLOSE_ELEMENT_ARR(json); // close _marks/measure array.
    }
  }

  rbu_resp_attr->mark_and_measures = rbu_mark_measures; 
  rbu_resp_attr->total_mark_and_measures = (short) mm_count;
  }


  CLOSE_ELEMENT_OBJ(json);  //close pages '}' object
  CLOSE_ELEMENT_ARR(json);  //close pages ']' array

  NSDL3_RBU(vptr, NULL, "_cav_dom_element = %d, onContentLoad = %d, onLoad = %d, _cav_startRender = %d, _cav_endRender = %d, _tti = %d", 
			 rbu_resp_attr->dom_element, rbu_resp_attr->on_content_load_time, rbu_resp_attr->on_load_time, 
			 rbu_resp_attr->_cav_start_render_time, rbu_resp_attr->_cav_end_render_time, rbu_resp_attr->_tti_time);
  //entries
  char start_time_str[RBU_MAX_64BYTE_LENGTH + 1]={0};               //"startedDateTime" of requests
  long elapse_time = 0;                           //Load time of request
  long total_time = 0;                            //pageload time 
  //int first_time = 0;                           //TODO How to handle this as it is used in Akamai cache setting
  int url_time = 0;                               //Time to compare if DOM or Onload or Start Render event is fired till now or not?
  int total_request_counter = 0;                  //Total Number of Requests
  int count = -1;                                 //This counter will keep track of initializing values
  //static __thread RBU_HARTime rbu_hartime[3500]; Moved to struct RBU_RespAttr (ns_rbu_api.h) to resolve Bug Id: 29739 
  //Request
  int tot_req_hdr_size = 0;                       //Sum of all headersSize of request
  char header_buffer_name_cav[32];                //request header name buffer 
  int tot_req_body_size = 0;                      //Sum of all bodySize of request
  //Response
  int req_bfr_DOM = 0;                            //Number Of Requests before DOM Content Event is fired (except Browser Cache)
  int req_bfr_OnLoad = 0;                         //Number of Requests before Onload Event is fired (except Browser cache)
  int req_bfr_Start_render = 0;                   //Number of Requests before Start Render (except Browser cache)
  int req_frm_browser_cache_bfr_DOM = 0;          //Number Of Requests, served from cache, before DOM Content Event is fired
  int req_frm_browser_cache_bfr_OnLoad = 0;       //Number Of Requests, served from cache, before OnLoad Event is fired
  int req_frm_browser_cache_bfr_Start_render = 0; //Number Of Requests, served from cache, before Start Render
  int resp_other_count = 0;                       //Count for resp.content.mimeType != (html/js/css/image)
  float resp_other_size = 0;                      //Size of resp.content.mimeType != (html/js/css/image)  
  int request_from_browser_cache = 0;             //Total Number of Requests served from browser cache
  int main_url_done = 0;                          //Enable when main(first) request found which not includes redirect_status_code
  int main_url_flag = 1;                          //Enable when request found which not includes redirect_status_code
  int dump_body_flag = 1;                         //Dump body of those main url's response which is non-redirect
  char main_url_resp_time_done = 0;               //Flag for Main URL done (dns + connect + wait+ receive)
  float tot_response_body_size = 0;               //Sum of all response which are not coming from cache.
  int tot_tcp_bytes_received = 0;                 //Sum of headerSize + bodySize.
  int total_domain_req = 0;                       //Domain provided by user "G_RBU_CACHE_DOMAIN"
  int total_cache_req = 0;                        //Sum of request served from Akamai, Instart, Cloudfront, Strangeloop, Browser, Browser(304).
  char cavnv_instrumented = 1;                    //Assumed cookie is always instrumented.
  char cav_nv_done = 0;                           //Parse headername "X-CavNV"
  char parse_cav_nv_done = 0;                     //Parse cookie cavNV
  int resp_type = -1;                             //Flag for Response Type (mimeTye etc)
  int resp_js_count = 0;                          //count for resp.content.mimeType = JavaScript
  float resp_js_size = 0;                         //size of resp.content.mimeType = JavaScript

  int resp_css_count = 0;                         //count for resp.content.mimeType = CSS
  float resp_css_size = 0;                        //size of resp.content.mimeType = CSS
  int resp_img_count = 0;                         //count for resp.content.mimeType = Image
  float resp_img_size = 0;                        //size of resp.content.mimeType = Image
  int resp_html_count = 0;                        //count for resp.content.mimeType = html
  float resp_html_size = 0;                       //size of resp.content.mimeType =  html
  char browser[20] = "Unknown";
  int rbu_max_url_count = RBU_MAX_URL_COUNT;
 
  int network_cache_stats_enable = runprof_table_shr_mem[vptr->group_num].gset.enable_network_cache_stats;

  int main_url_dump_acc_log = 0; // main_url_done get set in mid of processing har, so to dump access log taking another flag

  int norm_id = -1;
  char main_url_status_text[RBU_MAX_64BYTE_LENGTH + 1];
   
  int tot_res_hdr_size = 0;         //Response headerSize
  float byte_rcvd_bfr_DOM = 0;
  float byte_rcvd_bfr_OnLoad = 0;
 
  //Browser Mode For HAR, doing it in single run
  //Before, for each entries we are doing
  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.browser_mode == RBU_BM_FIREFOX)
  {
    if(global_settings->enable_ns_firefox)
      strcpy(browser, "Netstorm Firefox");
    else
      strcpy(browser, "System Firefox");
  }
  else
  {
    if(global_settings->enable_ns_chrome)
      strcpy(browser, "Netstorm Chrome");
    else
      strcpy(browser, "System Chrome");
  }

  //Handle "entries":[{"request":{}, "response":{}}]
  //entries is an element inside log, look for log-->entries
  GOTO_ELEMENT(json, "entries");

  //go inside entries
  OPEN_ELEMENT(json);

  //This will traverse elements inside "entries", if it is not open then return
  while(1)
  {
    //Checking "entries" is not empty
    if(nslb_json_next_array_item(json) == -1) 
    {
      if(json->error == NSLB_JSON_ARR_END)
	NSDL2_RBU(vptr, NULL, "No more entries remained in entries array");
      else
      {
	NSDL2_RBU(vptr, NULL, "Failed to get next item of array \'entries\', error = %s", nslb_json_strerror(json));
	NSTL1_OUT(NULL, NULL, "Failed to get next item of array \'entries\', error = %s", nslb_json_strerror(json));
        snprintf(script_execution_fail_msg, MAX_SCRIPT_EXECUTION_LOG_LENGTH, "Internal Error: Failed to get item of array \'entries\' in HAR, error = %s", nslb_json_strerror(json));
	//RETURN(-1);
	break;
      }
      break;
    }

    //open this element
    OPEN_ELEMENT(json);

    total_request_counter++;   //Total count of request in entries
    count++;                   //increment for next rbu_hartime
   
    CREATE_RBU_HARTIME       //Assuming maximum requests/response in a har file will be 3500, malloc the same if required
			     //realloc 1000 again.  
    //TODO Do we need a maximum value when realloc will stop??
    if(count >= rbu_max_url_count){ //Means we reached threshold
      NSDL4_RBU(NULL,NULL, "threshold exceed reallocing memory for rbu_hartime");
      rbu_max_url_count = rbu_max_url_count + 1000;
      MY_REALLOC(rbu_resp_attr->rbu_hartime, sizeof(RBU_HARTime) * (rbu_max_url_count), "rbu_resp_attr->rbu_hartime", -1);
    }

    //goto & get "startedDateTime" element
    GOTO_ELEMENT(json, "startedDateTime"); 
    GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);
    snprintf(start_time_str, RBU_MAX_64BYTE_LENGTH, "%s", ptr);

    ns_rbu_get_date_time(vptr, rbu_resp_attr);
    NSDL2_RBU(NULL, NULL, "date - [%s] ", rbu_resp_attr->date);

    if((rbu_resp_attr->rbu_hartime[count].started_date_time = get_start_time_msec(vptr, start_time_str)) == -1)
    {
      NSTL1_OUT(NULL, NULL, "Error: Unable to convert startedDateTime into millisecond.Ignoring Sample %s", start_time_str);
      CLOSE_ELEMENT_OBJ(json);
      continue; //Ignoring corrupted sample
    }

    //For Access log, dump each request timestamp
    rbu_resp_attr->url_date_time = rbu_resp_attr->rbu_hartime[count].started_date_time;

    //goto & get "time" element
    GOTO_ELEMENT(json, "time");
    GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);
    elapse_time = atoi(ptr);    
    if(main_url_done == 0)
      rbu_resp_attr->url_resp_time = elapse_time;

    rbu_resp_attr->rbu_hartime[count].end_date_time = rbu_resp_attr->rbu_hartime[count].started_date_time + elapse_time;

    NSDL2_RBU(NULL, NULL, "URL elapse time = %f", rbu_resp_attr->url_resp_time);
    NSDL2_RBU(NULL, NULL, "rbu_resp_attr->rbu_hartime[%d].started_date_time = %lld, "
                          "elapse_time = %d, rbu_resp_attr->rbu_hartime[%d].end_date_time = %lld, " 
			  "rbu_resp_attr->rbu_hartime[0].started_date_time = %lld", 
			   count, rbu_resp_attr->rbu_hartime[count].started_date_time, elapse_time, 
			   count, rbu_resp_attr->rbu_hartime[count].end_date_time, 
			   rbu_resp_attr->rbu_hartime[0].started_date_time); 

    url_time = (rbu_resp_attr->rbu_hartime[count].started_date_time - rbu_resp_attr->rbu_hartime[0].started_date_time);
    NSDL2_RBU(NULL, NULL, "Url time = %d for request no. = %d", url_time, total_request_counter);

    //Bug-55519: Main url should be decided according to document.location.href
    if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.browser_mode != RBU_BM_FIREFOX)
    {
      //goto & get "_cav_main_url" element
      GOTO_ELEMENT(json, "_cav_main_url");
      GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);

      if(main_url_done == 1 && atoi(ptr) == 1)
      {
        main_url_done = 0; //reset main_url_done, if main_url_done is set before getting _cav_main_url flag as '1'
        body_found = 0;    //reset body_found, so that body of main url also dumped.
        dump_body_flag = 1; //reset dump_body_flag
        rbu_resp_attr->url_resp_time = elapse_time;
      }
      NSDL2_RBU(NULL, NULL, "main_url_done = %d", main_url_done);
    }
 
    /***************************** Parse Request Data **************************/
    
    int temp_req_hdr_size = 0;    //headerSize of request
    int temp_req_body_size = 0;   //bodySize of request

    NSDL3_RBU(vptr, NULL, "Processing request detail");
    //goto request element.
    GOTO_ELEMENT(json, "request");
  
    //open request element object.
    OPEN_ELEMENT(json);
  
    //goto & get "method" element
    GOTO_ELEMENT(json, "method");
    GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);
    DUMP_TO_LOG_FILE(req_fp, "%s ", ptr);  
    cav_req_written += snprintf(cavtest_req_buf + cav_req_written, FILE_MAX_UPLOAD_SIZE - cav_req_written, "%s ", ptr);

    strcpy(rbu_resp_attr->req_method, ptr);
    NSDL3_RBU(vptr, NULL, "Request Method is %s", rbu_resp_attr->req_method);

    //goto & get "url" element 
    GOTO_ELEMENT(json, "url");
    GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);

    snprintf(rbu_resp_attr->inline_url, RBU_MAX_128BYTE_LENGTH, "%s", ptr);
    DUMP_TO_LOG_FILE(req_fp, "%s ", ptr);  
    cav_req_written += snprintf(cavtest_req_buf + cav_req_written, FILE_MAX_UPLOAD_SIZE - cav_req_written, "%s ", ptr);
 
    NSDL3_RBU(vptr, NULL, "Inline URL %s", rbu_resp_attr->inline_url);

    /* Create Rbu_domains and fill its members */
    if(global_settings->rbu_domain_stats_mode == RBU_DOMAIN_STAT_ENABLED)
    {
      norm_id = ns_rbu_handle_domain_discovery(vptr, rbu_resp_attr->inline_url, elapse_time);
      NSDL3_RBU(vptr, NULL, "norm_id = %d", norm_id);
    }

    //Take query parameter 
    ns_rbu_url_parameter_fetch(vptr, rbu_resp_attr);
    NSDL4_RBU(vptr, NULL, "Access Log : param_string = %s", rbu_resp_attr->param_string);

    //goto & get "httpVersion" element 
    GOTO_ELEMENT(json, "httpVersion");
    GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);
    strcpy(rbu_resp_attr->http_vrsn, ptr);
    DUMP_TO_LOG_FILE(req_fp, "%s\r\n", ptr);
    cav_req_written += snprintf(cavtest_req_buf + cav_req_written, FILE_MAX_UPLOAD_SIZE - cav_req_written, "%s\r\n", ptr);

    NSDL3_RBU(vptr, NULL, "HTTPVersion = %s", rbu_resp_attr->http_vrsn);

    //now move to Headers.
    GOTO_ELEMENT(json, "headers");  
     
    //open header array
    OPEN_ELEMENT(json);
  
    while(!nslb_json_next_array_item(json)) 
    {     
      //open this element
      OPEN_ELEMENT(json);
     
      //go & get "name" element 
      GOTO_ELEMENT(json, "name"); 
      GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);
      DUMP_TO_LOG_FILE(req_fp, "%s: ", ptr);
      cav_req_written += snprintf(cavtest_req_buf + cav_req_written, FILE_MAX_UPLOAD_SIZE - cav_req_written, "%s: ", ptr);
      snprintf(header_buffer_name_cav, 32, "%s", ptr);

      if(!strcasecmp(ptr, "User-Agent"))
      {
        NSDL3_RBU(vptr, NULL, "At header : User-agent found!");
        GOTO_ELEMENT(json, "value");
        GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);
        NSDL3_RBU(vptr, NULL, "USER-AGENT string - %s", ptr);
        strcpy(rbu_resp_attr->user_agent_str, ptr);
      } 
      //go & get "value" element 
      GOTO_ELEMENT(json, "value");
      GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);
      DUMP_TO_LOG_FILE(req_fp, "%s\r\n", ptr);
      cav_req_written += snprintf(cavtest_req_buf + cav_req_written, FILE_MAX_UPLOAD_SIZE - cav_req_written, "%s\r\n", ptr);
      //Atul: We will parse cookie CavNV if it will appear in request header.

      //NSDL3_RBU(NULL, NULL, "header_buffer_name_cav %s", header_buffer_name_cav);

      if( (parse_cav_nv_done != 1) && (!strcmp("Cookie", header_buffer_name_cav)))
      {
	char *start_cav_nv = NULL;
	char counter = 0; 
	NSDL3_RBU(NULL, NULL, "Cookie found in request header");
	//Now search for CavNV cookie is present or not.
	if( (start_cav_nv = strstr(ptr, "CavNV")) != NULL)
	{
	   NSDL3_RBU(NULL, NULL, "CavNV Cookie found in request header");
	  //Now cav_nv is pointing to starting of CavNV Cookie
	  while(start_cav_nv)
	  {
	    start_cav_nv = strchr(start_cav_nv,'-');
	    if(start_cav_nv)
	    {
	      counter++;
	      start_cav_nv++;
	      //We need value between 3 and 4th '-'
	      if(counter < 3)
	      start_cav_nv = strchr(start_cav_nv,'-');
	    else
	      break;
	    }
	  }
	  if(start_cav_nv)
	  {
	    cavnv_instrumented = atoi(start_cav_nv);
	    parse_cav_nv_done = 1;
	    NSDL3_RBU(NULL, NULL, "Request header cavnv_instrumented = [%d], parse_cav_nv_done = %d" ,cavnv_instrumented, parse_cav_nv_done);
	  } 
	}
      }
  
      //close element of header array so we can move to next element. 
      CLOSE_ELEMENT_OBJ(json);
    } 

    //close headers array
    CLOSE_ELEMENT_ARR(json);

    //Dump some Headers related to NS.
    DUMP_TO_LOG_FILE(req_fp, "\nVia: %s\r\n", browser);
    DUMP_TO_LOG_FILE(req_fp, "Browser-Profile: %s\r\n", prof_name);
    DUMP_TO_LOG_FILE(req_fp, "Har-File: %s\r\n", prev_har_file_name);
  
    //Headers End. 
    DUMP_TO_LOG_FILE(req_fp, "\r\n");
    cav_req_written += snprintf(cavtest_req_buf + cav_req_written, FILE_MAX_UPLOAD_SIZE - cav_req_written, "\r\n");

    //Here we will parse request cookie  

    GOTO_ELEMENT(json, "cookies");

    //open cookies array
    OPEN_ELEMENT(json);
 
    while(!nslb_json_next_array_item(json)) 
    {
      //open this element
      OPEN_ELEMENT(json);

      GOTO_ELEMENT(json, "name");
      GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);
      if(!strcmp("CavNVC", ptr))
      {
	GOTO_ELEMENT(json, "value");
	GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);
	if(!strcmp(rbu_resp_attr->netTest_CavNVC_cookie_val, ""))
          strcpy(rbu_resp_attr->netTest_CavNVC_cookie_val, ptr);
	NSDL3_RBU(NULL, NULL, "CavNVC cookie value = [%s]", ptr);
	//Eg:- CavNVC=000595678381740130307-2-138692191-2
      }
      //close element of cookies array so we can move to next element. 
      CLOSE_ELEMENT_OBJ(json);
    }
    //Close Cookies
    CLOSE_ELEMENT_ARR(json);


    //goto & get "headersSize" element
    GOTO_ELEMENT(json, "headersSize");
    GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);
    temp_req_hdr_size = atoi(ptr);

    //goto & get "bodySize" element, if body not present then bodySize value will be -1. 
    GOTO_ELEMENT(json, "bodySize");
    GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);
    temp_req_body_size = atoi(ptr);
  
    //This is an optional element.
    ret = nslb_json_goto_element(json, "postData");
    if(ret != 0 && json->error != NSLB_JSON_NOT_FOUND)
    {
      NSDL2_RBU(NULL, NULL, "Error: ns_rbu_process_har_file() - failed to get content element, error = %s\n", nslb_json_strerror(json));
      NSTL1_OUT(NULL, NULL, "Error: ns_rbu_process_har_file() - failed to get content element, error = %s\n", nslb_json_strerror(json));
      strncpy(rbu_resp_attr->access_log_msg, "Error: Failed to get elements of HAR, HAR is incomplete", RBU_MAX_ACC_LOG_LENGTH);
      strncpy(script_execution_fail_msg, "Error:Page is not loaded completely in browser", MAX_SCRIPT_EXECUTION_LOG_LENGTH);
      //RETURN(-1);
      break;
    }
    if(ret == 0) 
    {
      NSDL3_RBU(vptr, NULL, "Request found with body");   
    
      OPEN_ELEMENT(json);
		 
      //goto & get "text" element.
      GOTO_ELEMENT(json, "text");
      GET_ELEMENT_VALUE(json, ptr, &len, 0, 1);
      DUMP_TO_LOG_FILE(req_fp, "%s", ptr);
      cav_req_written += snprintf(cavtest_req_buf + cav_req_written, FILE_MAX_UPLOAD_SIZE - cav_req_written, "%s", ptr);
  
      //close content
      CLOSE_ELEMENT_OBJ(json);
    }

    //This is to separate two request 
    DUMP_TO_LOG_FILE(req_fp, "\n---------------------------------------------------------------------\n");
    if(global_settings->monitor_type == WEB_PAGE_AUDIT)
    {
      cav_req_written += snprintf(cavtest_req_buf + cav_req_written, FILE_MAX_UPLOAD_SIZE - cav_req_written,
                             "\n---------------------------------------------------------------------\n");
      ns_file_upload(cavtest_req_file_name, cavtest_req_buf, cav_req_written);
    }
    cavtest_req_buf[0] = 0;
    cav_req_written = 0;
  
    //close request element
    CLOSE_ELEMENT_OBJ(json);

    /************************** Parse Response Data ****************************/
   
    int tcp_bytes_received = 0;         //Response headerSize
    int body_received_size = 0;         //for a single response 
    int is_cache = 0;                   //Request is served from cache

    NSDL3_RBU(vptr, NULL, "Processing response detail"); 
    //goto response element.
    GOTO_ELEMENT(json, "response");
  
    //open request element object.
    OPEN_ELEMENT(json);
 
    //goto & get "status" element
    GOTO_ELEMENT(json, "status");
    GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);
    vptr->last_cptr->req_code = atoi(ptr);    //set on cptr to get req_ok for network_cache_stats.

    NSDL3_RBU(vptr, NULL, "status_code of request[%d] = %d", total_request_counter, atoi(ptr)); 

    if(main_url_done == 0)                    //Just set for main url. should not be set for other inline urls. 
      rbu_resp_attr->status_code = atoi(ptr);  

    //goto & get "httpVersion" element 
    GOTO_ELEMENT(json, "httpVersion");
    GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);
    if(main_url_done == 0)
    {
      DUMP_TO_LOG_FILE(rep_fp, "%s %d ", ptr, vptr->last_cptr->req_code);   //dump httpVersion
      cav_res_written += snprintf(cavtest_res_buf + cav_res_written, FILE_MAX_UPLOAD_SIZE - cav_res_written, "%s %d ", 
                                                                     ptr, vptr->last_cptr->req_code);
    }

    //goto & get "statusText" element 
    GOTO_ELEMENT(json, "statusText");
    GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);
    strncpy(rbu_resp_attr->status_text, ptr, 64);
    if(main_url_done == 0)
    {
      DUMP_TO_LOG_FILE(rep_fp, "%s\r\n", ptr);    //dump statusText
      cav_res_written += snprintf(cavtest_res_buf + cav_res_written, FILE_MAX_UPLOAD_SIZE - cav_res_written, "%s\r\n", ptr);
      strncpy(main_url_status_text, rbu_resp_attr->status_text, RBU_MAX_64BYTE_LENGTH);
 
      if (rbu_resp_attr->status_code >= 400 && rbu_resp_attr->status_code < 500)
        snprintf(script_execution_fail_msg, MAX_SCRIPT_EXECUTION_LOG_LENGTH, "Client Side Error: Status Code = %d, Status Text = '%s', "
                   "URL=%s", rbu_resp_attr->status_code, rbu_resp_attr->status_text, rbu_resp_attr->inline_url);
      else if (rbu_resp_attr->status_code >= 500 && rbu_resp_attr->status_code < 600)
        snprintf(script_execution_fail_msg, MAX_SCRIPT_EXECUTION_LOG_LENGTH, "Server Side Error: Status Code = %d, Status Text = '%s', "
                   "URL=%s", rbu_resp_attr->status_code, rbu_resp_attr->status_text, rbu_resp_attr->inline_url);

      else if(!memcmp(rbu_resp_attr->status_text, "Pending", 7))
      {
        snprintf(script_execution_fail_msg, MAX_SCRIPT_EXECUTION_LOG_LENGTH, "Main URL is not loaded in browser, URL=%s",
                  rbu_resp_attr->inline_url);
      }
      else if(!memcmp(rbu_resp_attr->status_text, "Failed", 6))
      {
        snprintf(script_execution_fail_msg, MAX_SCRIPT_EXECUTION_LOG_LENGTH, "Main URL(Application) is not accessible, URL=%s",
                  rbu_resp_attr->inline_url);
      }
    }
 
    //now move to Headers.
    GOTO_ELEMENT(json, "headers");  
     
    //open header array
    OPEN_ELEMENT(json);

    //save header in case of network_cache_stats_enable;
    char header_buffer_name[128 + 1];
    char header_buffer_val[512 + 1];
    int header_buffer_val_len;
    int consumed_bytes = 0;
    while(!nslb_json_next_array_item(json)) 
    {
      //open this element
      OPEN_ELEMENT(json);
   
      //goto & get header "name" element  
      GOTO_ELEMENT(json, "name"); 
      GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);
  
      if(main_url_done == 0)  
      {
	DUMP_TO_LOG_FILE(rep_fp, "%s: ", ptr);
        cav_res_written += snprintf(cavtest_res_buf + cav_res_written, FILE_MAX_UPLOAD_SIZE - cav_res_written, "%s: ", ptr);
      }
      snprintf(header_buffer_name, 128, "%s", ptr);
 
      //goto & get header "value" element  
      GOTO_ELEMENT(json, "value");
      GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);
     
      if(main_url_done == 0)  
      {
	DUMP_TO_LOG_FILE(rep_fp, "%s\r\n", ptr);
        cav_res_written += snprintf(cavtest_res_buf + cav_res_written, FILE_MAX_UPLOAD_SIZE - cav_res_written, "%s\r\n", ptr);
      }
      snprintf(header_buffer_val, 512, "%s\r\n", ptr);
      header_buffer_val_len = strlen(header_buffer_val);
 
      //Atul Sharma need to parse cav_nv header  and check cookie CavNV is present in header 
      //If we already parsed the cookie then ignore it.
      char counter = 0;
      if( (parse_cav_nv_done != 1) && (!strcasecmp("Set-Cookie", header_buffer_name)))
      {
	char *start_cav_nv = NULL;
	counter = 0; 
	NSDL3_RBU(NULL, NULL, "Set-Cookie found in response header");
	//Now search for CavNV cookie is present or not.
	if( (start_cav_nv = strstr(ptr, "CavNV")) != NULL)
	{
	   NSDL3_RBU(NULL, NULL, "CavNV Cookie found in response header");
	  //Now cav_nv is pointing to starting of CavNV Cookie
	  while(start_cav_nv)
	  {
	    start_cav_nv = strchr(start_cav_nv,'-');
	    if(start_cav_nv)
	    {
	      counter++;
	      start_cav_nv++;
	      //We need value between 3 and 4th '-'
	      if(counter < 3)
	      start_cav_nv = strchr(start_cav_nv,'-');
	    else
	      break;
	    }
	  }
	  if(start_cav_nv)
	  {
	    //start_cav_nv string will contains '_' so atoi will return integer comes before '_'. see atoi behaviour for details.
	    cavnv_instrumented = atoi(start_cav_nv);
	    parse_cav_nv_done = 1;
	    NSDL3_RBU(NULL, NULL, "cavnv_instrumented = [%d], parse_cav_nv_done = %d" ,cavnv_instrumented, parse_cav_nv_done);
	  } 
	}
	NSDL3_RBU(NULL, NULL, "CavNV Cookie not found in Set-Cookie response header");
      }

      //CavNV  <FlowPathInstance>_<TRNum>_<Category>_<Instrumented>
      /*
	Category can be - 
	       9 - unknown
	      10 - normal
	      11 - slow
	      12 - v. slow
	      13 - error 
	 Eg- X-CavNV : 4640209689524032449_50000_9_0 
       */
      //if cookie is not instrumented then 
      if(cav_nv_done == 0 && cavnv_instrumented != 0)
      {
	if(!strcasecmp("X-CavNV", header_buffer_name))
	{
	  char *cavnv_str = NULL;
	  strcpy(rbu_resp_attr->cav_nv_val, ptr); 
	  //parse category
	  cavnv_str = rbu_resp_attr->cav_nv_val; 
	  cavnv_str = strchr(cavnv_str,'_');
	  cavnv_str++;
	  cavnv_str = strchr(cavnv_str,'_');
	  cavnv_str++;
	  NSDL3_RBU(NULL, NULL, "cavnv string = [%s]", cavnv_str);
	  int category = atoi(cavnv_str);
	  char *cavnv_inst = NULL;
	  int cavnv_instrumented_header = -1;
	  if((cavnv_inst = strchr(cavnv_str, '_'))  != NULL)
	  {
	    cavnv_inst++;
	    cavnv_instrumented_header = atoi(cavnv_inst);
	  }
	  NSDL3_RBU(NULL, NULL, "category = [%d], cavnv_instrumented_header = [%d]", category, cavnv_instrumented_header);

	  if(category == 10 || cavnv_instrumented_header != 1 ) //Means we don't want to logged cav_nv_val into database if category is 10
	    *rbu_resp_attr->cav_nv_val = '\0';
	  cav_nv_done = 1;
	  NSDL3_RBU(NULL, NULL, "X-CavNV header = [%s]", rbu_resp_attr->cav_nv_val);
	}
      }

      //process network cache headers. 
      if(network_cache_stats_enable)
      {
	NSDL2_RBU(vptr, NULL, "header_buffer_name = %s, header_buffer_val = %s", header_buffer_name, header_buffer_val);

	//vptr->last_cptr->cptr_data will be allocated and vptr->last_cptr->cptr_data->nw_cache_state will
	//be set by below netcache function calls
	//reset vptr->last_cptr->cptr_data->nw_cache_state
	if(vptr->last_cptr->cptr_data) vptr->last_cptr->cptr_data->nw_cache_state = 0;
	
	//passing now - the last parameter as 0 as it is not being used while header processing.
	if(!strcmp(header_buffer_name, "X-Cache"))
	  proc_http_hdr_x_cache(vptr->last_cptr, header_buffer_val, header_buffer_val_len, &consumed_bytes, 0);
	else if(!strcmp(header_buffer_name, "X-Cache-Remote"))  
	  proc_http_hdr_x_cache_remote(vptr->last_cptr, header_buffer_val, header_buffer_val_len, &consumed_bytes, 0);
	else if(!strcmp(header_buffer_name, "X-Check-Cacheable"))
	  proc_http_hdr_x_check_cacheable(vptr->last_cptr, header_buffer_val, header_buffer_val_len, &consumed_bytes, 0);
      }
      //close element of header array so we can move to next element. 
      CLOSE_ELEMENT_OBJ(json);
    } 

    if(main_url_done == 0) 
    {
      DUMP_TO_LOG_FILE(rep_fp, "\r\n");
      cav_res_written += snprintf(cavtest_res_buf + cav_res_written, FILE_MAX_UPLOAD_SIZE - cav_res_written, "\r\n");
    }
  
    //close headers array
    CLOSE_ELEMENT_ARR(json);
    
    //Atul: Here we will parse response cookie  

    GOTO_ELEMENT(json, "cookies");

    //open cookies array
    OPEN_ELEMENT(json);

    char *start_ptr = NULL;
    char *end_ptr = NULL;
    char instrument_Str[4] = "";
    char counter = 0;
 
    //char *out_buf_ptr = rbu_resp_attr->cav_nv_val;
    while(!nslb_json_next_array_item(json)) 
    {
      //open this element
      OPEN_ELEMENT(json);

      GOTO_ELEMENT(json, "name");
      GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);
      if(!strcmp("CavNV", ptr))
      {
	GOTO_ELEMENT(json, "value");
	GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);
	if(!strcmp(rbu_resp_attr->netTest_CavNV_cookie_val, ""))
          strcpy(rbu_resp_attr->netTest_CavNV_cookie_val, ptr);
	start_ptr = ptr;
	NSDL3_RBU(NULL, NULL, "CavNv cookie value = [%s]", ptr);
	//cav_nv_val will be in form of 
	// [<ndSessionId>-<TestRun>-<prev_fp_start_time>-<Instrumented(0/1)>-<category)>-<ExceptionCount(0forNow)>-<TotalFPCount>-<TierId>-<ServerId>-<InsstanceId>]
	//Eg:- CavNV=4612202928777447781-50000-105264919121-0-9-0-7480-10-16-29
	//and we need to store Instrument value
	if(parse_cav_nv_done == 0) 
	{
	  while(start_ptr)
	  {
	    start_ptr = strchr(start_ptr,'-');
	    if(start_ptr)
	    {
	      counter++;
	      start_ptr++;
	      //We need value between 3 and 4th '-'
	      if(counter < 3)
	      start_ptr = strchr(start_ptr,'-');
	    else
	      break;
	    }
	  }
	  if(start_ptr)
	  {
	    end_ptr = strchr(start_ptr,'-');
	    int len = end_ptr - start_ptr;
	    strncpy(instrument_Str, start_ptr, len);
	    instrument_Str[len] = 0;
	    cavnv_instrumented = atoi(instrument_Str);
	    parse_cav_nv_done = 1;
	  } 
	   NSDL3_RBU(NULL, NULL, "cavnv_instrumented = [%d]", cavnv_instrumented);
	} 
      }
      //close element of header array so we can move to next element. 
      CLOSE_ELEMENT_OBJ(json);
    }
    //Close Cookie
    CLOSE_ELEMENT_ARR(json);


    //Note: we are checking status code after processing header, to differentiate main_url_done and network_cache_stats_enable.
    int j;
    for(j = 0; j < sizeof(redirect_status_code)/sizeof(short); j++) {

      if(redirect_status_code[j] == rbu_resp_attr->status_code)
	break;
    }
    if(j == sizeof(redirect_status_code)/sizeof(short))
    {
      NSDL2_RBU(vptr, NULL, "Found first non redirect url, status code = %d", rbu_resp_attr->status_code);
      //This is the final url to dump request/response.
      main_url_done = 1;
      main_url_flag = 1;
    }
    else
      NSDL2_RBU(vptr, NULL, "Status code found = %d(redirected url), moving to next url", rbu_resp_attr->status_code);
  
    //goto & get "headersSize" element
    GOTO_ELEMENT(json, "headersSize");
    GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);
    tcp_bytes_received = atoi(ptr);
    tot_res_hdr_size += tcp_bytes_received;

    //goto & get "bodySize" element
    GOTO_ELEMENT(json, "bodySize");
    GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);
    body_received_size = atoi(ptr);   //if body not present then bodySize value will be -1 
    rbu_resp_attr->in_url_pg_wgt = (float)(body_received_size)/1024;
    NSDL2_RBU(vptr, NULL, "rbu_resp_attr->in_url_resp_size = %f", rbu_resp_attr->in_url_pg_wgt);
 
    //This element is optional so need to check is present or not.
    //goto "content" element, for response body
    ret = nslb_json_goto_element(json, "content");
    if(ret != 0 && json->error != NSLB_JSON_NOT_FOUND)
    {
      NSTL1_OUT(NULL, NULL, "Error: ns_rbu_process_har_file() - failed to get content element, error = %s\n", nslb_json_strerror(json));
      //RETURN(-1);
      break;
    }
    if(ret == 0) 
    {
      NSDL3_RBU(vptr, NULL, "Response body found");
      OPEN_ELEMENT(json);
  
      if(body_found == 0) 
      {
	if(rbu_resp_attr->resp_body != NULL) 
	{
	  FREE_AND_MAKE_NULL(rbu_resp_attr->resp_body, "rbu_resp_attr->resp_body", -1);
	  NSDL3_RBU(vptr, NULL, "Previous resp_body pointer freed");
	}
	//get response body text.
	GOTO_ELEMENT(json, "text");
	GET_ELEMENT_VALUE(json, ptr, &len, 1, 1);
	NSDL2_RBU(NULL, NULL, "Dumping body = '%s'", ptr);
	DUMP_TO_LOG_FILE(rep_fp, "%s", ptr);

        if(global_settings->monitor_type == WEB_PAGE_AUDIT)
        {
          //Dump Headers
          ns_file_upload(cavtest_res_file_name, cavtest_res_buf, cav_res_written);
          //Dump Body
          ns_file_upload(cavtest_res_file_name, ptr, len);

          ns_file_upload(cavtest_res_body_file_name, ptr, len);
          ns_file_upload(cavtest_res_body_file_name, "\n---------------------------------------------------------------------\n", 71);
        }
        cavtest_res_buf[0] = 0;
        cav_res_written = 0;

	//Currently we are using save buffer that is malloced by json parser we will not free this buffer.
	//Now setting last malloced size to len(TODO: it should be handled properly because malloced size can be more than this)
	rbu_resp_attr->resp_body = ptr;
	rbu_resp_attr->resp_body_size = len; 
	rbu_resp_attr->last_malloced_size = len;
  
	//we need to dump in rep_body file also.
   
	if(*rep_body_file_name && ((rep_body_fp = fopen(rep_body_file_name, "a")) == NULL)) 
	{
	  NSTL1_OUT(NULL, NULL, "Error: ns_rbu_process_har_file() - failed to open response log file \'%s\'\n", rep_body_file_name);
	  //RETURN(-1);
	  break; 
	}
	if(rep_body_fp)
	{
	  if(fwrite(rbu_resp_attr->resp_body, 1, rbu_resp_attr->resp_body_size, rep_body_fp) != rbu_resp_attr->resp_body_size) 
	  {
	    NSTL1_OUT(NULL, NULL, "Error: ns_rbu_process_har_file() - failed to write resp body to log file, error = %s", nslb_strerror(errno));
	    //RETURN(-1);
	    break;
	  }
	  DUMP_TO_LOG_FILE(rep_body_fp, "\n---------------------------------------------------------------------\n");
	}
       
	if((dump_body_flag) && (main_url_done))    //this check is for dump body only for non-rediect request.
	{
	  body_found = 1;
	  dump_body_flag = 0;
	}
      }

      //Read response.content.mime-type. And collect Data from har for GDF.
      // in HAR it will be like : "mimeType": "image/jpeg",
      GOTO_ELEMENT(json, "mimeType");
      GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);
      NSDL3_RBU(vptr, NULL, "Checking response type mimeType = %s", ptr);

      //Calculating size of different Response types.
      if(strstr(ptr, "javascript")) 
      {
	resp_type = MIMETYPE_JS;
	resp_js_count++;
	resp_js_size += ((body_received_size > 0) ? body_received_size : 0);
	NSDL3_RBU(vptr, NULL, "resp_js_count = %d, resp_js_size = %f", resp_js_count, resp_js_size);
      }
      else if(strstr(ptr, "css"))
      { 
	resp_type = MIMETYPE_CSS;
	resp_css_count++;
	resp_css_size += ((body_received_size > 0) ? body_received_size : 0);
	NSDL3_RBU(vptr, NULL, "resp_css_count = %d, resp_css_size = %f", resp_css_count, resp_css_size);
      }
      else if(strstr(ptr, "image"))
      { 
	resp_type = MIMETYPE_IMG;
	resp_img_count++;
	resp_img_size += ((body_received_size > 0) ? body_received_size : 0);
	NSDL3_RBU(vptr, NULL, "resp_img_count = %d, resp_img_size = %f", resp_img_count, resp_img_size);
      }
      else if(strstr(ptr, "html")) 
      {
	resp_type = MIMETYPE_HTML;
	resp_html_count++;
	resp_html_size += ((body_received_size > 0) ? body_received_size : 0);
	NSDL3_RBU(vptr, NULL, "resp_html_count = %d, resp_html_size = %f", resp_html_count, resp_html_size);
      }
      else
      {
	resp_type = MIMETYPE_OTHER;
	resp_other_count++;
	resp_other_size += ((body_received_size > 0) ? body_received_size : 0);
	NSDL3_RBU(vptr, NULL, "resp_other_count = %d, resp_other_size = %f", resp_other_count, resp_other_size);
	NSDL3_RBU(vptr, NULL, "mimeType is OTHER, resp_type = %d", resp_type);
      }

      //close content
      CLOSE_ELEMENT_OBJ(json);
    }
    else
    {
      rbu_resp_attr->resp_body = NULL;
      rbu_resp_attr->resp_body_size = 0;      
      rbu_resp_attr->last_malloced_size = 0;
    }
   
    //This is to separate two response
    DUMP_TO_LOG_FILE(rep_fp, "\n---------------------------------------------------------------------\n");
    if(global_settings->monitor_type == WEB_PAGE_AUDIT)
    {
      cav_res_written += snprintf(cavtest_res_buf + cav_res_written, FILE_MAX_UPLOAD_SIZE - cav_res_written,
                             "\n---------------------------------------------------------------------\n");
      ns_file_upload(cavtest_res_file_name, cavtest_res_buf, cav_res_written);
    }
    cavtest_res_buf[0] = 0;
    cav_res_written = 0;

    //close "response" element
    CLOSE_ELEMENT_OBJ(json);

    //goto and set "timings" elements
    set_timings_elements(vptr, rbu_resp_attr, json, req_fp, rep_fp, rep_body_fp, &main_url_resp_time_done, main_url_done, norm_id);

    if(temp_req_body_size < 0)
      temp_req_body_size = 0;

    //set cache provider & req coming from domLoad, onLoad, startRender
    set_cache_elements(vptr, rbu_resp_attr, json, req_fp, rep_fp, rep_body_fp, rbu_cache_domain_enable, total_request_counter, &total_domain_req, &total_cache_req, &request_from_browser_cache, &is_cache, url_time, &req_frm_browser_cache_bfr_DOM, &req_frm_browser_cache_bfr_OnLoad, &req_frm_browser_cache_bfr_Start_render, &req_bfr_DOM, &req_bfr_OnLoad, &req_bfr_Start_render, &temp_req_body_size, &temp_req_hdr_size, &tot_req_body_size, &tot_req_hdr_size, rbu_resp_attr->inline_url);

    //Comes like IP:PORT (104.108.194.29:80)
    GOTO_ELEMENT(json, "serverIPAddress");
    GET_ELEMENT_VALUE(json, ptr, &len, 0, 0);
    strcpy(rbu_resp_attr->server_ip_add, ptr);
    NSDL3_RBU(vptr, NULL, "Access Log : serverIPAddress - %s", rbu_resp_attr->server_ip_add);

    if(body_received_size > 0)
    {
      if(!is_cache)
      {
	tot_response_body_size += body_received_size;
        //Bug-72584
        if(url_time < rbu_resp_attr->on_content_load_time)
          byte_rcvd_bfr_DOM += body_received_size;
 
        if(url_time < rbu_resp_attr->on_load_time)
          byte_rcvd_bfr_OnLoad += body_received_size;
      }
      tot_tcp_bytes_received += tcp_bytes_received + body_received_size;   //headerSize + bodySize
    }

    //set to cptr.    
    vptr->last_cptr->tcp_bytes_recv = tot_tcp_bytes_received;

    //Calculation for byte send and byte received
    rbu_resp_attr->byte_send = (tot_req_hdr_size + tot_req_body_size)/1024;
    rbu_resp_attr->byte_rcvd = (tot_response_body_size)/1024;
  
    NSDL3_RBU(vptr, NULL, "tcp_bytes_recv = %d, byte_send = %f, byte_rcvd = %f", vptr->last_cptr->tcp_bytes_recv, 
			   rbu_resp_attr->byte_send, rbu_resp_attr->byte_rcvd);

    //DUMP INTO RBUResp to fetch for gdf
    if(resp_type != -1)
    {
      NSDL3_RBU(vptr, NULL, "Filling Page resource weights - resp_js_size = %f, resp_css_size = %f, resp_img_size = %f, resp_html_size = %f",
			     resp_js_size, resp_css_size, resp_img_size, resp_html_size);

      //Structure Filling for Size of different mimeType
      rbu_resp_attr->resp_js_size = resp_js_size/1024;
      rbu_resp_attr->resp_css_size = resp_css_size/1024;
      rbu_resp_attr->resp_img_size = resp_img_size/1024;
      rbu_resp_attr->resp_html_size = resp_html_size/1024;
      rbu_resp_attr->resp_other_size = resp_other_size/1024;
      rbu_resp_attr->pg_wgt = (resp_js_size + resp_css_size + resp_img_size + resp_html_size)/1024;

      //Structure Filling for Count of different mimeType
      rbu_resp_attr->resp_js_count = resp_js_count;
      rbu_resp_attr->resp_css_count = resp_css_count;
      rbu_resp_attr->resp_img_count = resp_img_count;
      rbu_resp_attr->resp_html_count = resp_html_count;
      rbu_resp_attr->resp_other_count = resp_other_count;

      //DeBug for mimeType wgts
      NSDL3_RBU(vptr, NULL, "Filled Page resource weights - "
			    "Size : resp_js_size = %f, resp_css_size = %f, resp_img_size = %f, "
			    "resp_html_size = %f, resp_other_size = %f, pg_wgt = %f, in_url_pg_wgt = %f\n"
			    "Count : resp_js_count = %d, resp_css_count = %d, resp_img_count = %d, "
			    "resp_html_count = %d, resp_other_count = %d",
			    rbu_resp_attr->resp_js_size, rbu_resp_attr->resp_css_size, rbu_resp_attr->resp_img_size,
			    rbu_resp_attr->resp_html_size, rbu_resp_attr->resp_other_size, rbu_resp_attr->pg_wgt,
			    rbu_resp_attr->in_url_pg_wgt, rbu_resp_attr->resp_js_count, rbu_resp_attr->resp_css_count, 
			    rbu_resp_attr->resp_img_count, rbu_resp_attr->resp_html_count, rbu_resp_attr->resp_other_count);
    }

    /* Get number of request server from Browser Cache and Server */
    rbu_resp_attr->request_without_cache = total_request_counter - request_from_browser_cache;
    rbu_resp_attr->request_from_cache = request_from_browser_cache;
    rbu_resp_attr->req_frm_browser_cache_bfr_DOM = req_frm_browser_cache_bfr_DOM;
    rbu_resp_attr->req_frm_browser_cache_bfr_OnLoad = req_frm_browser_cache_bfr_OnLoad;
    rbu_resp_attr->req_frm_browser_cache_bfr_Start_render = req_frm_browser_cache_bfr_Start_render;
    rbu_resp_attr->req_bfr_DOM = req_bfr_DOM;
    rbu_resp_attr->req_bfr_OnLoad = req_bfr_OnLoad;
    rbu_resp_attr->req_bfr_Start_render = req_bfr_Start_render;

    NSDL3_RBU(vptr, NULL, "Different RequestStats: total_request_counter = %d, request_from_browser_cache = %d, "
			  "req_frm_browser_cache_bfr_DOM = %d, req_frm_browser_cache_bfr_OnLoad = %d, "
			  "req_frm_browser_cache_bfr_Start_render = %d, "
			  "req_bfr_DOM = %d, req_bfr_OnLoad = %d, req_bfr_Start_render = %d",
			   total_request_counter, request_from_browser_cache,
			   rbu_resp_attr->req_frm_browser_cache_bfr_DOM, rbu_resp_attr->req_frm_browser_cache_bfr_OnLoad, 
			   rbu_resp_attr->req_frm_browser_cache_bfr_Start_render,
			   rbu_resp_attr->req_bfr_DOM, rbu_resp_attr->req_bfr_OnLoad, rbu_resp_attr->req_bfr_Start_render);
    
    //Checks if need to dump access log or not
    ns_rbu_check_acc_log_dump(vptr, rbu_resp_attr, main_url_dump_acc_log);
    
    if((network_cache_stats_enable == 1) && main_url_done) //not for redirected URL
    {
      int status = 0; 
      //set NS_RESP_NETCACHE only for main url.
      //if(main_url_flag && first_time && vptr->last_cptr->cptr_data)
      if(main_url_flag && vptr->last_cptr->cptr_data)
      {
	if(vptr->last_cptr->cptr_data->nw_cache_state & TCP_HIT_MASK)
	{
	  NSDL3_RBU(vptr, NULL, "url is of main url type");
	  vptr->flags |= NS_RESP_NETCACHE;
	} else {
	  vptr->flags &= ~NS_RESP_NETCACHE;
	} 
	main_url_flag = 0;      //This will on for main which is non-redirected url.
      }
      status = !get_req_status(vptr->last_cptr);
      NSDL3_RBU(vptr, NULL, "updating nw_cache_stats, elapse_time = %d, url_ok = %d, total_tcp_bytes_received = %d",  
			     elapse_time, status, vptr->last_cptr->tcp_bytes_recv);
      nw_cache_stats_update_counter(vptr->last_cptr, elapse_time, status);
    }
/*
    //Currently we can't block inline urls.
    //If inline_object are disabled then no need to process further requests.
    int fetch_inline_obj = !(runprof_table_shr_mem[vptr->group_num].gset.get_no_inlined_obj);
    NSDL3_RBU(vptr, NULL, "network_cache_stats_enable = %d, fetch_inline_obj = %d", network_cache_stats_enable, fetch_inline_obj);
*/
    //if main url done and network_cache_stats not enabled then every thing done.
    //if(main_url_done && (network_cache_stats_enable == 0))   //We want data for each request so we will not break.
    // break;

    resp_type = -1;
 
    //For access log for mode 1, data need to dump for only main url  
    //Cannot use main_url_done, as data fetching is restricted with this flag, and we want each data for main URL.
    main_url_dump_acc_log = main_url_done;   

    //we have to check that cavnv_instrumented is not 1 then we do not logged the X-CavNV header in CSV.
    //Below check will also help us if cookie and header will not be in same reqest
    if(cav_nv_done == 0 || cavnv_instrumented != 1 )
    {
      strcpy(rbu_resp_attr->cav_nv_val, "");
    }
    NSDL3_RBU(vptr, NULL, "Finally X-CavNV header value = %s", rbu_resp_attr->cav_nv_val);
    //close array element of entries array(need to move to another element).
    CLOSE_ELEMENT_OBJ(json);

  } //End of entries while loop.

  CLOSE_ELEMENT_ARR(json);  //close entries array ']'
  CLOSE_ELEMENT_OBJ(json);  //close log '}'
  CLOSE_ELEMENT_OBJ(json);  //close json '}'

  //If "entries" element is empty, then pageLoad & cache is 0 
  if(count != -1)
  {
    //calling getpageload function 
    if(cav_loadTime <= 0)
    { 
      total_time = get_page_load_time(rbu_resp_attr, vptr, ++count, phase_interval);   
      rbu_resp_attr->page_load_time = (int)total_time; 
    }
    NSDL2_RBU(NULL, NULL, "rbu_resp_attr->page_load_time = %d, count = %d, total_request_counter = %d", 
			   rbu_resp_attr->page_load_time, count, total_request_counter);

    //calculate CDN Offload %
    if(rbu_cache_domain_enable)
    {
      rbu_resp_attr->akamai_cache = (total_cache_req / (total_domain_req * 1.0)) * 100;
      NSDL3_RBU(vptr, NULL, "total_cache_req = %d, total_domain_req = %d", total_cache_req, total_domain_req);
    }
    else
    {
      rbu_resp_attr->akamai_cache = (total_cache_req / (total_request_counter * 1.0)) * 100;
      NSDL3_RBU(NULL, NULL, "total_cache_req = %d, total_request_counter = %d", total_cache_req, total_request_counter);
    }
    NSDL3_RBU(NULL, NULL, "CDN Offload % = %f", rbu_resp_attr->akamai_cache);

    if(byte_rcvd_bfr_DOM > 0)
      rbu_resp_attr->byte_rcvd_bfr_DOM = byte_rcvd_bfr_DOM/1024;
    if(byte_rcvd_bfr_OnLoad > 0)
      rbu_resp_attr->byte_rcvd_bfr_OnLoad = byte_rcvd_bfr_OnLoad/1024;
 
    NSDL4_RBU(vptr, NULL, "rbu_resp_attr->byte_rcvd_bfr_DOM = %f, rbu_resp_attr->byte_rcvd_bfr_OnLoad = %f",
              rbu_resp_attr->byte_rcvd_bfr_DOM, rbu_resp_attr->byte_rcvd_bfr_OnLoad);

    strncpy(rbu_resp_attr->status_text, main_url_status_text, RBU_MAX_64BYTE_LENGTH);
  }

  rbu_resp_attr->req_body_size = tot_req_body_size / 1024;

  //Cavisson Test Web Page Audit
  if(global_settings->monitor_type == WEB_PAGE_AUDIT)
  {
    cavtest_web_avg->status_code = rbu_resp_attr->status_code;
    cavtest_web_avg->req_header_size = tot_req_hdr_size / 1024;
    cavtest_web_avg->res_header_size = tot_res_hdr_size / 1024;
    NSDL4_RBU(vptr, NULL, "rbu_resp_attr->status_code = %d, tot_req_hdr_size = %d, tot_res_hdr_size = %d",
                           rbu_resp_attr->status_code, tot_req_hdr_size, tot_res_hdr_size);
  }

  RETURN(0);
}

static int filter_file(const struct dirent *a)
{
  char *ptr = (char*)a->d_name;

  //if(strstr(ptr, filter_str))
  char *q = NULL;
  q = strstr(ptr, "_last"); //No Need to count _last clip bugid 63286
  if((strstr(ptr, filter_str)) && !q)  
    return 1;
  else
    return 0;
}
//12 '_' Number
int get_clip_id(char *clip)
{
 if(!clip)
  return 0;
 int count = 0;
 int clipid;
 char clipStr[16];
 char *ptr , *ptr2;
 ptr = clip;
 while(ptr)
 {
   ptr = strchr(ptr,'_');
   if(ptr)
   {
     count++;
     ptr++;
     if(count < 12)
      ptr = strchr(ptr,'_');
     else
      break;
   }
 }
 if(ptr)
 {
 ptr2 = strchr(ptr,'_');
 int len = ptr2 - ptr;
 strncpy(clipStr,ptr,len);
 clipStr[len] = 0;
 clipid = atoi(clipStr);
 }
 return clipid;
}

//Moved below function to nslb_speed_index.cpp file
#if 0
//13 '_' Number
int get_clip_time_index(char *clip)
{
 if(!clip)
   return 0;
 int count = 0;
 int clipid;
 char clipStr[16];
 char *ptr , *ptr2;
 ptr = clip;
 while(ptr)
 {
   ptr = strchr(ptr,'_');
   if(ptr)
   {
     count++;
     ptr++;
     if(count < 13)
      ptr = strchr(ptr,'_');
     else
      break;
   }
 }
 if(ptr)
 {
 ptr2 = strchr(ptr,'_');
 int len = ptr2 - ptr;
 strncpy(clipStr,ptr,len);
 clipStr[len] = 0;
 clipid = atoi(clipStr);
 }
 clipid = clipid/100;
 return clipid;
}
#endif
int my_alpha_sort(const struct dirent **aa, const struct dirent **bb)
{
 int data1 , data2;
 NSDL1_RBU(NULL, NULL, "first string = %s, second string = %s", (*aa)->d_name, (*bb)->d_name);
 data1 = get_clip_id((char*)(*aa)->d_name);
 data2 = get_clip_id((char*)(*bb)->d_name);
 return(data1 - data2);
}

void get_prof_name(char *har_name, char *prof_name)
{
  NSDL3_RBU(NULL, NULL, "get_prof_name() called");
  char *har_name_ptr = NULL;
  char *prof_name_ptr = NULL;
  char local_har_name[1024] = "";
  strcpy(local_har_name, har_name);
  if((har_name_ptr = index(local_har_name, '+')) != NULL) {
    *har_name_ptr = '\0';
    har_name_ptr++;

    if((prof_name_ptr = index(har_name_ptr, '+')) != NULL)
    {
      *prof_name_ptr = '\0';
      strcpy(prof_name, har_name_ptr);
      NSDL3_RBU(NULL, NULL, "profile name : %s", prof_name);
    } 
  }
  NSDL3_RBU(NULL, NULL, "get_prof_name() complete");
}

int last_malloc_size_for_speed_index_arr = 0;
double *speed_index_val = NULL;
static int get_speed_index(VUser *vptr, char *har_sfx, int *speed_index)
{
  NSDL3_RBU(vptr, vptr, "get_speed_index() called");
  char path_snap_shot[512] = {0};
  int i, num_snap_shots = 0,  max_time = 0; 
  struct dirent **snap_shot_dirent;
  unsigned long long start_time;
  int visual_complete_tm = 0;

  RBU_NS_LOGS_PATH
  sprintf(path_snap_shot, "%s/logs/%s/snap_shots/", g_ns_wdir, rbu_logs_file_path);
  sprintf(filter_str, "video_clip_%s", har_sfx);
  num_snap_shots = scandir(path_snap_shot, &snap_shot_dirent, filter_file,my_alpha_sort);
  NSDL1_RBU(vptr, NULL, "Method Called, path_snap_shot = %s, num_snap_shots = %d, har_sfx = %s", 
			  path_snap_shot, num_snap_shots, har_sfx); 
  if(num_snap_shots < 0 )
  {
    NSTL1_OUT(NULL, NULL, "Error: Failed to open snap_shots dir");
    strncpy(vptr->httpData->rbu_resp_attr->access_log_msg, "Error: Failed to open snap-shot directory", RBU_MAX_ACC_LOG_LENGTH);
    strncpy(script_execution_fail_msg, "Internal Error:Failed to open snap-shot directory.", MAX_SCRIPT_EXECUTION_LOG_LENGTH); 
    return -1;
  }

  if(num_snap_shots)
  { 
    if(num_snap_shots == 1)
      *speed_index = -1;
    else 
    {
      max_time = ns_get_clip_time_index(snap_shot_dirent[num_snap_shots-1]->d_name) + 1;

      NSDL1_RBU(NULL, NULL, "max_time = %d, last_malloc_size_for_speed_index_arr = %d" , max_time, last_malloc_size_for_speed_index_arr);
      if (last_malloc_size_for_speed_index_arr < max_time)
      {
	speed_index_val = (double*) realloc(speed_index_val,sizeof(double) * max_time);
	if ( speed_index_val == NULL) {
	  NSTL1_OUT(NULL, NULL, "failed to realloc speed_index_val for har_sfx = %s\n", har_sfx);
	  goto error;
	}
	last_malloc_size_for_speed_index_arr = max_time;
      }
      memset(speed_index_val, 0, ((max_time)* sizeof(double)));

      start_time = get_ms_stamp(); //for analyze the time taken by the ns_get_speed_index function.
      /*
       * Bug 50092 - Use same Algorithm for Speed Index and Visual Complete time.
       * Visual Complete time will be calculated using Histogram same as Speed Index.
       */
      *speed_index = ns_get_speed_index(path_snap_shot, snap_shot_dirent, num_snap_shots,
                                        &visual_complete_tm, speed_index_val);
      start_time = get_ms_stamp() - start_time;

      if((vptr->httpData->rbu_resp_attr->_cav_start_render_time > 0) &&
         (visual_complete_tm > vptr->httpData->rbu_resp_attr->_cav_start_render_time))
      {
        NSDL3_RBU(NULL, NULL, "HISTOGRAM: visual_complete_tm = %d, CAVSERICE: cav_end_render_time = %d",
                  visual_complete_tm, vptr->httpData->rbu_resp_attr->_cav_end_render_time);
        vptr->httpData->rbu_resp_attr->_cav_end_render_time = visual_complete_tm;
      }

      NSDL1_RBU(NULL, NULL, "*speed_index = %d for har_sfx = %s, time_taken = %llu ms, cav_end_render_time = %d", 
                 *speed_index, har_sfx, start_time, vptr->httpData->rbu_resp_attr->_cav_end_render_time);
    }
  }
  else
  {
    NSTL1_OUT(NULL, NULL, "G_RBU_CAPTURE_CLIPS is enabled, but did not get any snapshot for har_sfx = %s\n", har_sfx);
  }

error:
  for(i = 0; i < num_snap_shots; i++) {
     free(snap_shot_dirent[i]);
     snap_shot_dirent[i] = NULL;
  }
  free(snap_shot_dirent);
  snap_shot_dirent = NULL;
  return 0; 
}

inline int get_har_name_and_date(VUser *vptr, char *har_file_with_path, char *har_name, u_ns_ts_t *har_date_and_time, int *speed_index, char *prof_name)
{
  NSDL1_RBU(vptr, NULL, "get_har_name_and_date() called");
  char *har_name_ptr = NULL, *start_ptr = NULL;
  char buf[128] = {0};
  char cmd[512] = {0};

  if((har_name_ptr = rindex(har_file_with_path, '/')) != NULL) {
    har_name_ptr++; //Pointing to first character of HAR file name i.e P_
    if(strstr(har_name_ptr, ".har")) {
      strcpy(har_name, har_name_ptr);
      sprintf(cmd, "echo \"%s\" | awk -F '+' '{print $4 \"-\"  $5}'", har_name);
      if(nslb_run_cmd_and_get_last_line(cmd, 128, buf) != 0) {           //buf will have date, which is extracted from HAR name
	NSTL1_OUT(NULL, NULL, "Error in running cmd = %s, exiting !\n", cmd); 
	return -1;
      }
      /*struct tm tm;
      NSDL1_RBU(vptr, NULL, "buf = %s", buf);
      if (strptime(buf, "%Y-%m-%d %H-%M-%S", &tm) == NULL)
      {
	NSTL1_OUT(NULL, NULL, "Error in parsing timestamp from = '%s'.\n", buf); 
	return -1 ;
      }
      *har_date_and_time = mktime(&tm) - global_settings->unix_cav_epoch_diff; */

      //Resolved Bug 27740 - SM-DDR | Page detail report is showing wrong time of har file, with difference of 1 hr.
      char *tm_field[6];

      get_tokens(buf, tm_field, "-", 6);
      NSDL1_RBU(vptr, NULL, "Y = %s, m = %s, d = %s, H = %s, M = %s, S = %s",
		  tm_field[0], tm_field[1], tm_field[2], tm_field[3], tm_field[4], tm_field[5]);

      struct tm t;
      long gmt_t;
      //2017-05-15+05-38-57
      t.tm_isdst = -1;  //for day light saving
      t.tm_mon = atoi(tm_field[1]) - 1;
      t.tm_mday = atoi(tm_field[2]);
      t.tm_hour = atoi(tm_field[3]);
      t.tm_min = atoi(tm_field[4]);
      t.tm_sec = atoi(tm_field[5]);

      t.tm_year = atoi(tm_field[0]) - 1900;
      gmt_t = (long)mktime(&t);

      NSDL1_RBU(vptr, NULL, "gmt_t = %ld", gmt_t);

      *har_date_and_time = gmt_t - global_settings->unix_cav_epoch_diff;
      if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_capture_clip) 
      {
	sprintf(cmd, "echo \"%s\" | awk -F '+' '{print $6}'", har_name);
	if(nslb_run_cmd_and_get_last_line(cmd, 128, buf) != 0) {
	  NSTL1_OUT(NULL, NULL, "Error in running cmd = %s, exiting !\n", cmd); 
	  return -1;
	}
	if((start_ptr = strchr(buf, '.')) != NULL)
	{
	  *start_ptr = '\0';
	} else {
	  NSDL1_RBU(vptr, NULL, "Not found [.] in %s", har_name); 
	  return -1;
	}
	//If providing threshold value for clips, i.e., mode 2, then speed_index will not be calculated
	if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_capture_clip != 2) 
	  get_speed_index(vptr, buf, speed_index);
	else
	  *speed_index = -1;

	NSDL1_RBU(vptr, NULL, "speed_index %d", *speed_index);
      }
      get_prof_name(har_name, prof_name);
    } else {
      NSDL4_RBU(vptr, NULL, "Error to get har name");
    }
  } else {
    NSDL4_RBU(vptr, NULL, "Cannot get / form = %s", har_file_with_path);
  }

  return 0;
}
/*--------------------------------------------------------------------------------------------- 
 * Purpose   : This function will do following - 
 *             (1) Process Url Resp and fill the structure RBU_RespAttr
 *             (2) Make Like list for response body 
 *             (3) Set cptr variables - req_ok, req_type, redirection_depth, bytes 
 *
 * Input     : vptr     - provide pointer to current vertual user
 *             url      - provide fully qualyfied URL
 *             har_file - provide generated har file name with absolute path 
 *
 * Output    : On error    -1
 *             On success   0 
 *--------------------------------------------------------------------------------------------*/

void web_rbu_end_callback(ClientData client_data)
{
  web_url_end_cb *web_url_end_rbu = (web_url_end_cb *)client_data.p;
  VUser *vptr = web_url_end_rbu->vptr;

  NSDL1_RBU(vptr, NULL, "Method called, Timer Expired");
 
  int ret = ns_rbu_web_url_end(vptr, web_url_end_rbu->vptr->httpData->rbu_resp_attr->url, web_url_end_rbu->renamed_har_file_with_abs_path, web_url_end_rbu->vptr->httpData->rbu_resp_attr->har_file_prev_size, web_url_end_rbu->vptr->httpData->rbu_resp_attr->profile, web_url_end_rbu->prev_har_file, web_url_end_rbu->page_status, web_url_end_rbu->performance_trace_start_time);
 
 if(ret < 0)
 {
   HANDLE_RBU_PAGE_FAILURE(ret)
 }
   
 FREE_AND_MAKE_NULL(web_url_end_rbu, "web_url_end_callback", -1); 
 
}

int ns_rbu_web_url_end(VUser *vptr, char *url, char *har_file, int har_file_size, char *prof_name, char *prev_har_file_name, int req_ok,
                              time_t start_time)
{
  connection *cptr = vptr->last_cptr;
  RBU_RespAttr *rbu_resp_attr;

  char har_name[1024] = {0};
  char prof_nme[128] = {0};
  u_ns_ts_t har_date_and_time = 0;
  int speed_index = -1;
  int ret = 0;
  time_t cur_time;
  long long elaps_time = 0;
  int wait_time = 0;
  char *user_agent_buff;
  int found = 0;

  NSDL1_RBU(vptr, NULL, "Method called, vptr = %p, har_file = %s, har_file_size = %d, vptr->httpData->rbu_resp_attr = %p", 
					vptr, har_file?har_file:NULL, har_file_size, vptr->httpData->rbu_resp_attr);

  /* Allocate memory for RBU_RespAttr if not not allocated before and restet status_code, resp_body_size, etc..*/
  CREATE_RBU_RESP_ATTR // This is not required WE have already done malloc of rbu_resp_attr in rbu_init
  rbu_resp_attr = vptr->httpData->rbu_resp_attr;

  NSDL2_RBU(vptr, NULL, "rbu_resp_attr = %p, rbu_resp_attr->retry_count = %d", rbu_resp_attr, rbu_resp_attr->retry_count); 

  //Reset page stats member of structure RBU_RESP_ATTR_TIME to avoid overlapping
  RESET_RBU_RESP_ATTR_TIME(rbu_resp_attr);

  // (1) Process Resp 
  /* Enhancement-8591: If HAR is not made successfully and G_CONTINUE_ON_PAGE_ERR is 0, then session will be aborted. */
  ContinueOnPageErrorTableEntry_Shr *ptr;
  ptr = (ContinueOnPageErrorTableEntry_Shr *)runprof_table_shr_mem[vptr->group_num].continue_onpage_error_table[vptr->cur_page->page_number];
  NSDL3_RBU(vptr, NULL, "har_file_size = %d, continue_on_pg_err = %d", 
			 har_file_size, ptr->continue_error_value);
 
  if(har_file_size != 0)
  {
    ret = ns_rbu_process_har_file(har_file, rbu_resp_attr, vptr, prof_name, prev_har_file_name);
    if(ret < 0)
    {
      //Case of Abort when HAR file is not complete and just format get dumped
      if(ret == -2)
      {
        HANDLE_RBU_PAGE_FAILURE_WITH_CLICK(ret)
        return ret;
      }

      NSDL4_RBU(vptr, NULL, "Har file is found but some elements are missing, hence setting page_status, sess_status, req_ok to Misc Err");
      strncpy(rbu_resp_attr->access_log_msg, "Har file is found but some elements are missing, "
                                           "hence setting page_status, sess_status, req_ok to Misc Err", RBU_MAX_ACC_LOG_LENGTH);
      ns_rbu_log_access_log(vptr, rbu_resp_attr, RBU_ACC_LOG_DUMP_LOG);

      RBU_SET_PAGE_STATUS_ON_ERR(vptr, rbu_resp_attr, NS_REQUEST_ERRMISC, ptr);
    }
    else if(ret == 1) // Below code has been addded as when we are returning 1 no action was getting initiated for NVM.
    {
      //Ignore HAR as no request is send over the network
      NSDL1_RBU(vptr, NULL, "HAR file processing done. ret = %d, is_incomplete_har_file = %d", ret, rbu_resp_attr->is_incomplete_har_file);
      if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT)
      {
        // This vptr->page_status is required in ns_click_action
        vptr->page_status = 0;
        NSDL1_RBU(vptr, NULL, "Switching to user context");
        switch_to_vuser_ctx(vptr, "ClickExecutePageOver");
	return ret;
      }
      else if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_SEPARATE_THREAD)
      {
        // Throughout the code we are sending WEB_URL_REP to thread
        NSDL1_RBU(vptr, NULL, "Sending reply message of CLICK_API to thread");
        send_msg_nvm_to_vutd(vptr, NS_API_WEB_URL_REP, 0);
      }
      else if(vptr->sess_ptr->script_type == NS_SCRIPT_TYPE_JAVA)
      {
        NSDL1_RBU(vptr, NULL, "Sending reply message of CLICK_API to JAVA");
        send_msg_to_njvm(vptr->mcctptr, NS_NJVM_API_CLICK_API_REP, 0);
      }
      return ret;
    }
    NSDL4_RBU(vptr, NULL, "HAR file processing done. is_incomplete_har_file = %d", rbu_resp_attr->is_incomplete_har_file);
    NSDL4_RBU(vptr, NULL, "Resp Attr Dump: status_code = %d, malloced_len = %d, resp_body_size = %d, resp_body = [%s]", 
			   rbu_resp_attr->status_code, rbu_resp_attr->last_malloced_size, 
			   rbu_resp_attr->resp_body_size, rbu_resp_attr->resp_body);
    //Bug-59603: Performance Trace Dump
    if(rbu_resp_attr->performance_trace_flag)
    {
      char performance_trace_file_buf[RBU_MAX_PATH_LENGTH + 1];
      char buf[RBU_MAX_2K_LENGTH + 1];
      char renamed_page_name[RBU_MAX_VALUE_LENGTH + 1];
      int ret = 0;
      wait_time = (rbu_resp_attr->performance_trace_timeout/1000);

      if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.screen_size_sim == 0)
        snprintf(renamed_page_name, RBU_MAX_VALUE_LENGTH, "%s", vptr->cur_page->page_name);
      else
        snprintf(renamed_page_name, RBU_MAX_VALUE_LENGTH, "%s'(%dx%d)'", vptr->cur_page->page_name,
                   vptr->screen_size->width, vptr->screen_size->height); 
 
      RBU_NS_LOGS_PATH
      //path of TR, where gzip will copy
      snprintf(performance_trace_file_buf, RBU_MAX_PATH_LENGTH, "%s/logs/%s/performance_trace/P_%s+%s.gz",
                      g_ns_wdir, rbu_logs_file_path, renamed_page_name, rbu_resp_attr->performance_trace_filename);

      //gzip command buf
      snprintf(buf, RBU_MAX_2K_LENGTH, "gzip -c %s/%s/performance_trace/%s > %s",
                      rbu_resp_attr->har_log_path, rbu_resp_attr->profile, rbu_resp_attr->performance_trace_filename,
                      performance_trace_file_buf);  

      performance_trace_file_buf[0] = '\0';

      //path of profile log directory
      snprintf(performance_trace_file_buf, RBU_MAX_PATH_LENGTH, "%s/%s/performance_trace/%s",
                      rbu_resp_attr->har_log_path, rbu_resp_attr->profile, rbu_resp_attr->performance_trace_filename);
  
      NSDL4_RBU(vptr, NULL, "Checking performance trace file made or not at [%s]", performance_trace_file_buf);
       
      while(1)
      {
        //Case : Accessing performance trace file at path 
        if((access(performance_trace_file_buf, F_OK) != -1))
        {
          NSDL4_RBU(vptr, NULL, "Performance trace file %s made succefully.", performance_trace_file_buf);
 
          ret =  nslb_system2(buf);
          if (WEXITSTATUS(ret) != 1)
          {       
            NSDL2_RBU(vptr, NULL, "Performance trace file for rbu_logs_file_path = %s moved sucessfully", rbu_logs_file_path);
          }
          else  
          {
            NSDL2_RBU(vptr, NULL, "Unable to move Performance trace file for path  = %s", rbu_logs_file_path);
            NS_DT1(vptr, NULL, DM_L1, MM_VARS, "Unable to move Performance trace file for path  = %s", rbu_logs_file_path);
            rbu_resp_attr->performance_trace_flag = 0;
          }

          break;
        }
        else
        {
          NSDL4_RBU(vptr, NULL, "Performance trace file %s not made.", performance_trace_file_buf);
 
          cur_time = time(NULL);
          elaps_time = cur_time - start_time;
          NSDL4_RBU(vptr, NULL, "elaps_time = %lld, wait_time = %d\n", elaps_time, wait_time);
          if(elaps_time > wait_time)
          {
            NSDL1_RBU(vptr, NULL, "Timeout - Performance trace file not generated within %lld sec.", elaps_time);
            NS_DT1(vptr, NULL, DM_L1, MM_VARS, "Timeout - Performance trace file not generated within %lld sec.", elaps_time);
            NSTL1_OUT(vptr, NULL, "Timeout - Performance trace file not generated within %lld sec.", elaps_time);
      
            rbu_resp_attr->performance_trace_flag = 0;
            break;
          }
          NSDL4_RBU(vptr, NULL, "Performance trace file not made sleep for 600 ms.");
          web_url_end_cb *web_url_end;
          RBU_CREATE_FILL_WEB_END_DATA 
          VUSER_SLEEP_WEB_URL_END(vptr, 600);
        }
      }
    }
    rbu_resp_attr->retry_count = 0; // Initializing it to 0 so that it can be used later
    //This function will create csv data and dump it to csv file
    //Call only call if har file found and RBU_ENABLE_CSV is enable i.e 1
    if(rbu_resp_attr->is_incomplete_har_file == 0)  //only call if har file found
    { 
      if(global_settings->rbu_enable_csv)
      {
        if((create_csv_data(vptr, rbu_resp_attr )) == -1 )
	  NSDL2_RBU(vptr,NULL, "Error while created csv file");
      }

      if((get_har_name_and_date(vptr, har_file, har_name, &har_date_and_time, &speed_index, prof_nme)) == -1)
      {
        //If we dont found date and time in har file name then we consider that har as bad har.
        NSDL2_RBU(vptr, NULL, "Unable to calculate 'har_date_and_time' hence considering har [%s] as bad har file"
          		    " and not Dumping data into CSV file", har_file);
        
        snprintf(rbu_resp_attr->access_log_msg, RBU_MAX_ACC_LOG_LENGTH, 
                    "Unable to calculate 'har_date_and_time' hence considering har[%s] as bad har file and not Dumping data into CSV file", 
                    har_file);
        strncat(rbu_resp_attr->access_log_msg, " Status set as Misc", RBU_MAX_ACC_LOG_LENGTH);
        strncpy(script_execution_fail_msg, "Internal Error:Failed to calculate 'har_date_and_time'.", MAX_SCRIPT_EXECUTION_LOG_LENGTH);
        RBU_SET_PAGE_STATUS_ON_ERR(vptr, rbu_resp_attr, NS_REQUEST_ERRMISC, ptr);

      }
      else
      {
        //Moving this calling to ns_rbu_handle_web_url_end(), as need to dump cv_status to csv.
        //Only dump data into csv if har_file is not bad
        //if(rbu_resp_attr->is_incomplete_har_file == 0 && har_name[0])
        //log_page_rbu_detail_record(vptr,har_name, har_date_and_time, speed_index, vptr->httpData->rbu_resp_attr->cav_nv_val, NULL, prof_nme);
 
        strcpy(vptr->httpData->rbu_resp_attr->har_name, har_name);
        vptr->httpData->rbu_resp_attr->har_date_and_time = har_date_and_time;
        vptr->httpData->rbu_resp_attr->speed_index = speed_index;

        //Adding Device Info in DB and CSV
        //Bug : 59539 Device info should be differentiated on the basis of user agent. 
        //From 4.1.14 DeviceInfo will have value according to UA string 
        if((runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.user_agent_mode == 1) &&
            (vptr->httpData && (vptr->httpData->ua_handler_ptr != NULL) && (vptr->httpData->ua_handler_ptr->ua_string != NULL)))
        {
          user_agent_buff = vptr->httpData->ua_handler_ptr->ua_string;
        }
        else if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.user_agent_mode == 2)
          user_agent_buff = runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.user_agent_name;
        else
          user_agent_buff = vptr->browser->UA;

        while(*user_agent_buff)
        {
          if(*user_agent_buff == 'm' || *user_agent_buff == 'M')
          {
            user_agent_buff++;
            if(*user_agent_buff == 'o' || *user_agent_buff == 'O')
            {
              user_agent_buff++;
              if(*user_agent_buff == 'b' || *user_agent_buff == 'B')
              {
                user_agent_buff++;
                if(*user_agent_buff == 'i' || *user_agent_buff == 'I')
                {
                  found = 1;
                  sprintf(rbu_resp_attr->dvc_info, "Mobile:%s:%s",
                     (runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.browser_mode == RBU_BM_CHROME ? "Chrome": "Firefox"),
                      runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.brwsr_vrsn);
                }
                else
                  user_agent_buff-=3;
              }
              else
                user_agent_buff-=2;
            }
            else
              user_agent_buff--;
          }
          user_agent_buff++;
        }

        if(!found)
        {
          sprintf(rbu_resp_attr->dvc_info, "Desktop:%s:%s", 
                   (runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.browser_mode == RBU_BM_CHROME ? "Chrome": "Firefox"), 
                    runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.brwsr_vrsn);
        }
      /*******  End of UA string based DeviceInfo Code **************/
      }
    }
  }
      
  //Bug 83293
  cptr->req_ok = req_ok;
  if(rbu_resp_attr->status_code >=100)
  {
    cptr->req_code = rbu_resp_attr->status_code;
    cptr->req_ok = get_req_status(cptr);
    rbu_resp_attr->pg_avail = (cptr->req_ok == 0)?1:0;
  }
  else if(rbu_resp_attr->pg_status == NS_REQUEST_TIMEOUT)
  {
    RBU_SET_PAGE_STATUS_ON_ERR(vptr, rbu_resp_attr, NS_REQUEST_TIMEOUT, ptr);
    rbu_resp_attr->is_incomplete_har_file = 0; //Dump data into CSV
  }
  else if(rbu_resp_attr->pg_status == NS_REQUEST_CONFAIL)
  {
    RBU_SET_PAGE_STATUS_ON_ERR(vptr, rbu_resp_attr, NS_REQUEST_CONFAIL, ptr);
    rbu_resp_attr->is_incomplete_har_file = 0; //Dump data into CSV
    strncpy(rbu_resp_attr->access_log_msg, "Main URL(Application) is not accessible.", 512);
  }
  else
  {
    //cptr->req_ok = req_ok; //Moved above
    snprintf(script_execution_fail_msg, MAX_SCRIPT_EXECUTION_LOG_LENGTH, "HAR file is not generated.");
  }

  rbu_fill_page_status(vptr, rbu_resp_attr);

  NSDL2_RBU(vptr, NULL, "Fill pg_avail = %d, rbu_resp_attr = %p", rbu_resp_attr->pg_avail, rbu_resp_attr);
  NSDL2_RBU(NULL, NULL, "After HAR process - RBU_resp_attr data - "
				"on_content_load_time= %d, on_load_time = %d, page_load_time = %d, _tti_time = %d, _cav_startRender_time = %d, "
				"visually_complete_time = %d, request_without_cache = %d, request_from_cache = %d, byte_received =%f, byte_send = %f," 
				"js_size = %f, css_size = %f, img_wgt = %f, pg_wgt = %f, pg_avail = %d",
				 rbu_resp_attr->on_content_load_time, 
				 rbu_resp_attr->on_load_time, 
				 rbu_resp_attr->page_load_time, 
				 rbu_resp_attr->_tti_time, 
				 rbu_resp_attr->_cav_start_render_time, 
				 rbu_resp_attr->_cav_end_render_time,
				 rbu_resp_attr->request_without_cache,
				 rbu_resp_attr->request_from_cache,
				 rbu_resp_attr->byte_rcvd,
				 rbu_resp_attr->byte_send,
				 rbu_resp_attr->resp_js_size,
				 rbu_resp_attr->resp_css_size,
				 rbu_resp_attr->resp_img_size,
				 rbu_resp_attr->pg_wgt,
                         rbu_resp_attr->pg_avail);

  cptr->bytes = rbu_resp_attr->resp_body_size;

  NSDL2_RBU(vptr, NULL, "Waiting for handle page complete done, page_name = %s, page_id = %d",
                           vptr->cur_page->page_name, vptr->cur_page->page_id);

  vut_add_task(vptr, VUT_RBU_WEB_URL_END);
  return 0;
}

/*------------------------------------------------------------------- 
 * Purpose   : This function will dump RBU Http request into url_req_.. file 
 *
 * Input     : prof_name - provide profile name or ALL
 *                         
 *
 * Output    : On success - 0 
 *-------------------------------------------------------------------*/
  /* Dump host into url request file 
     Request File Data : 
      -----------------------------------------------------------------------
      GET /tiny HTTP/1.1^M
      Host: 127.0.0.1:81^M
      User-Agent: Mozilla/5.0 (Windows; U; Windows NT 5.2; en-US; rv:1.8.1.4) Gecko/20070515 Firefox/11.0.0.4^M
      Accept: text/xml,application/xml,application/xhtml+xml,text/html;q=0.9,text/plain;q=0.8,video/x-mng,image/png,image/jpeg,image/gif;q=0.2,text/css,;q=0.^M
      Accept-Encoding: gzip, deflate, compress;q=0.9^M
      Keep-Alive: 300^M
      Connection: keep-alive^M
      Content-Type: text/xml; charset=UTF-8^M
      
      Borwser-User-Profile <profile name>
      Vnc-Display-Id <id>
      
      Body .....  
      -----------------------------------------------------------------------
   */
inline static void ns_rbu_debug_log_http_req(VUser *vptr, char *url, char *prof_name, char **headers, int num_headers, char *req_body, int body_size)
{
  char url_req_file[RBU_MAX_FILE_LENGTH + 1];        /* name of file to store url req */
  char line_terminater[] = "\r\n";                   /* End of line */ 
  char line_break[] = "\n-----------------------------------------------------------\n"; 
  char *req_dump_buf = NULL;                         /* This buffer store whole url req and at last dump into file and free it*/
  char *req_dump_buf_ptr = NULL;                     
  FILE *url_req_fp = NULL;
  int copyied_len = 0;
  int total_copyied_len = 0;
  int left_len = RBU_MAX_URL_REQ_DUMP_BUF_LENGTH;
  int i = 0;

  PerHostSvrTableEntry_Shr* svr_entry;

  NSDL2_RBU(vptr, NULL, "vptr = %p, url = %s, prof_name = %s, num_headers = %d, req_body = %s", 
                         vptr, url, prof_name, num_headers, req_body?req_body:NULL);

  RBU_NS_LOGS_PATH

  /* Url Req file in format - url_req_<nvm_id>_<user_id>_<sess_inst>_<pg_inst>_<url_inst>_<sess_id>_<page_id>_<url_id> 
     Open file in append mode and dump as got Firefox ARGS
   */
  sprintf(url_req_file, "%s/logs/%s/req_rep/url_req_%hd_%u_%u_%d_0_%d_%d_%d_0.dat",
                      g_ns_wdir, ns_logs_file_path, child_idx, vptr->user_index,
                      vptr->sess_inst, vptr->page_instance, vptr->group_num,
                      GET_SESS_ID_BY_NAME(vptr),
                      GET_PAGE_ID_BY_NAME(vptr));

  NSDL2_RBU(vptr, NULL, "url_req_file = [%s]", url_req_file);

  if((url_req_fp = fopen(url_req_file, "w+")) == NULL)
  {
    NSTL1_OUT(NULL, NULL, "Error: failed in opening file '%s'.\n", url_req_file); 
    return ;
  }
 
  MY_MALLOC(req_dump_buf, RBU_MAX_URL_REQ_DUMP_BUF_LENGTH + 1, "RBU Req Dump buf", -1);
  req_dump_buf_ptr = req_dump_buf;

  /* GET /tiny HTTP/1.1^M */
  if(vptr->last_cptr->url_num->proto.http.http_method == RBU_HTTP_METHOD_GET)
  {
    copyied_len = snprintf(req_dump_buf_ptr, left_len, "GET %s%s", url, line_terminater);  
    total_copyied_len += copyied_len; 
    NSDL2_RBU(vptr, NULL, "total_copyied_len = %d, req_dump_buf_ptr = %s", total_copyied_len, req_dump_buf_ptr);
    RBU_SET_WRITE_PTR(req_dump_buf_ptr, copyied_len, left_len);
  }
  else
  {
    copyied_len = snprintf(req_dump_buf_ptr, left_len, "POST %s%s", url, line_terminater);  
    total_copyied_len += copyied_len; 
    NSDL2_RBU(vptr, NULL, "total_copyied_len = %d, req_dump_buf_ptr = %s", total_copyied_len, req_dump_buf_ptr);
    RBU_SET_WRITE_PTR(req_dump_buf_ptr, copyied_len, left_len);
  }

  /* Host: 127.0.0.1:81^M */
   svr_entry = get_svr_entry(vptr, vptr->last_cptr->url_num->index.svr_ptr);

   copyied_len = snprintf(req_dump_buf_ptr, left_len, "HOST: %s%s", svr_entry->server_name, line_terminater);  
   total_copyied_len += copyied_len; 
   NSDL2_RBU(vptr, NULL, "total_copyied_len = %d, req_dump_buf_ptr = %s", total_copyied_len, req_dump_buf_ptr);
   RBU_SET_WRITE_PTR(req_dump_buf_ptr, copyied_len, left_len);
  
  /* User-Agent: Mozilla/5.0 (Windows; U; Windows NT 5.2; en-US; rv:1.8.1.4) Gecko/20070515 Firefox/11.0.0.4^M */

  /* Headers */ 
  for(i = 0; i < num_headers; i++)
  {
    NSDL2_RBU(vptr, NULL, "headers[%d] = %s", i, headers[i]);
    copyied_len = snprintf(req_dump_buf_ptr, left_len, "%s%s", headers[i], line_terminater);  
    total_copyied_len += copyied_len; 
    NSDL2_RBU(vptr, NULL, "total_copyied_len = %d, req_dump_buf_ptr = %s", total_copyied_len, req_dump_buf_ptr);
    RBU_SET_WRITE_PTR(req_dump_buf_ptr, copyied_len, left_len);
  }
  
  //Addind \r\n for Header end
  //if(num_headers)
  //{
    copyied_len = snprintf(req_dump_buf_ptr, left_len, "%s", line_terminater);  
    total_copyied_len += copyied_len;  
    NSDL2_RBU(vptr, NULL, "total_copyied_len = %d, req_dump_buf_ptr = %s", total_copyied_len, req_dump_buf_ptr);
    RBU_SET_WRITE_PTR(req_dump_buf_ptr, copyied_len, left_len);
  //}

  /* Firefox Parametrise args */
  copyied_len = snprintf(req_dump_buf_ptr, left_len, "Browser-User-Profile %s%s", prof_name, line_terminater);  
  total_copyied_len += copyied_len;  
  NSDL2_RBU(vptr, NULL, "total_copyied_len = %d, req_dump_buf_ptr = %s", total_copyied_len, req_dump_buf_ptr);
  RBU_SET_WRITE_PTR(req_dump_buf_ptr, copyied_len, left_len);

  copyied_len = snprintf(req_dump_buf_ptr, left_len, "%s", line_break);  
  total_copyied_len += copyied_len;  
  NSDL2_RBU(vptr, NULL, "total_copyied_len = %d, req_dump_buf_ptr = %s", total_copyied_len, req_dump_buf_ptr);
  RBU_SET_WRITE_PTR(req_dump_buf_ptr, copyied_len, left_len);

  *req_dump_buf_ptr = '\0';

  NSDL2_RBU(vptr, NULL, "total_copyied_len = %d, req_dump_buf = %s", total_copyied_len, req_dump_buf);
   
  if(fwrite(req_dump_buf, RBU_FWRITE_EACH_ELEMENT_SIZE, total_copyied_len, url_req_fp) <= 0)
  {
    NSTL1_OUT(NULL, NULL, "Error: writing into file '%s' failed.", url_req_file);
    fclose(url_req_fp);
    return;
  }

  /* Dumping Body*/
  if(req_body != NULL)
  {
    if(fwrite(req_body, RBU_FWRITE_EACH_ELEMENT_SIZE, body_size, url_req_fp) <= 0)
    {
      NSTL1_OUT(NULL, NULL, "Error: writing into file '%s' failed.", url_req_file);
      fclose(url_req_fp);
      return;
    }
  }

  fclose(url_req_fp);
  FREE_AND_MAKE_NULL(req_dump_buf, "req_dump_buf", -1);
}
//Atul- making it global since we are calling it from rbu.c
int ns_rbu_validate_profile(VUser *vptr, char *browser_base_log_path, char *prof_name)
{
  char p_file[1024 + 1];
  char line_buf[1024 + 1];

  FILE *fp = NULL;
  int found = 0;
  char *p_name;
  struct stat st;
  if (prof_name == NULL)
    p_name = vptr->httpData->rbu_resp_attr->profile; 
  else
    p_name = prof_name;

  NSDL1_RBU(vptr, NULL,"Method called, browser_base_log_path = %s, p_name = %s", browser_base_log_path, p_name?p_name:"NULL");

  if(p_name[0] == 0)
  {
    NSTL1_OUT(NULL, NULL, "Error: ns_rbu_validate_profile() - profile name is empty.\n");
    return -1;
  }

  /* In the case of Chrome browser we have only profile directory hence check only exitance of profile directory */
  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.browser_mode == RBU_BM_CHROME)
    goto check_prof_dir;

  snprintf(p_file, 1024, "%s/profiles.ini", browser_base_log_path);
  
  fp = fopen(p_file, "r");
  if(fp == NULL)
  {
    NSTL1_OUT(NULL, NULL, "Error: ns_rbu_validate_profile() - failed in opening file '%s', error(%d) = %s\n", p_file, errno, nslb_strerror(errno));
    return -1;
  }
 
  while(fgets(line_buf, 1024, fp))
  {
    NSDL4_RBU(vptr, NULL,"line_buf = [%s]", line_buf);
    if(strstr(line_buf, p_name)) //If pattren found
    {
      found = 1;
      break;
    }
  }

  fclose(fp);

  if(!found) // If profile not found in profile.ini 
  {
    NSTL1_OUT(NULL, NULL, "Error: ns_rbu_validate_profile() - profile '%s' is not valid profile, "
                    "as their entry not present in profile.ini. Provide a valid profile.\n", p_name);
  
    return -1;
  }

  check_prof_dir:
    snprintf(p_file, 1024, "%s/profiles/%s", browser_base_log_path, p_name);
    if((stat(p_file, &st) == -1) || !S_ISDIR(st.st_mode)) //If profile directory not found  
    {
      NSTL1_OUT(NULL, NULL, "Error: ns_rbu_execute_page() - profile '%s' is not valid profile, "
                      "as their directory '%s' not present. Provide a valid profile.\n", p_name, p_file);
      return -1;
    }
    
    //check whether logs dir exists or not.
    snprintf(p_file, 1024, "%s/logs/%s", browser_base_log_path, p_name);
    if((stat(p_file, &st) == -1) || !S_ISDIR(st.st_mode)) //If logs directory not found
    {
      NSTL1_OUT(NULL, NULL, "Error: ns_rbu_execute_page() -logs '%s' is not valid profile, "
                      "as their directory '%s' not present. Provide a valid profile.\n", p_name, p_file);
      return -1;
    }

    return 0;
}

/*------------------------------------------------------------------- 
 * Purpose   : This function will close firefox instace either by profile name or ALL 
 *             It can parametrise
 *
 * Output    : On success - 0 
 * Since deprecated removed calling from ns_stop_browser()
 *-------------------------------------------------------------------*/
int ns_stop_browser_internal(VUser *vptr)
{
  char cmd[RBU_MAX_BUF_LENGTH], cmd1[RBU_MAX_BUF_LENGTH];

  /* Manish: This API will not used from 4.0.0 Because we will stop browser on the end od session if stop browser flag on in G_RBU keyword*/
  return 0;


  // Shakti: bug 6621: Stop firefox instance only if script is RBU
  if(!runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_rbu)
  {
    NSDL2_RBU(vptr, NULL, "Script is not rbu so returning.");
    return 0;
  }

  NSDL2_RBU(vptr, NULL, "Method Called, profile_name = %s, default_svr_location = [%s]", vptr->httpData->rbu_resp_attr->profile, 
                         default_svr_location?default_svr_location:NULL);  

  // Validating Profile name.
  if(ns_rbu_validate_profile(vptr, "/home/cavisson/.rbu/.mozilla/firefox/logs", NULL) == -1)
    return -1;

  // Manish: Why we are sending signal 19 to firefox command ???? How is stop firefox in proper way??? 
  // I think we don't need to send signal 9 to firefox instead of this we should check is firefox is in crash condition by reading its crash 
  // Report if yes then remove all crash memory of firfox (files generated due to crash)
  //snprintf(cmd, RBU_MAX_BUF_LENGTH - 1, "kill -19 `ps -ef | grep -w \"firefox.*-P %s\" | grep -v grep | awk -F' ' '{print $2}' 2>/dev/null`", vptr->httpData->rbu_resp_attr->profile);
  snprintf(cmd, RBU_MAX_BUF_LENGTH - 1, "kill -19 `ps -ef | grep -w \"firefox.*-profile %s\" | grep -v grep | awk -F' ' '{print $2}' 2>/dev/null`", vptr->httpData->rbu_resp_attr->profile);

  //snprintf(cmd1, RBU_MAX_BUF_LENGTH - 1, "kill -9 `ps -ef | grep -w \"firefox.*-P %s\" | grep -v grep | awk -F' ' '{print $2}' 2>/dev/null`", vptr->httpData->rbu_resp_attr->profile);
  snprintf(cmd1, RBU_MAX_BUF_LENGTH - 1, "kill -9 `ps -ef | grep -w \"firefox.*-profile %s\" | grep -v grep | awk -F' ' '{print $2}' 2>/dev/null`", vptr->httpData->rbu_resp_attr->profile);

  NSDL2_RBU(vptr, NULL, "ns_stop_browser: cmd = [%s]", cmd);
  nslb_system2(cmd);

  VUSER_SLEEP(vptr, 1000);// This APi we are not using anymore so not changing this SLEEP

  NSDL2_RBU(vptr, NULL, "ns_stop_browser: cmd1 = [%s]", cmd1);
  nslb_system2(cmd1);
	
  return 0;
}


// We are switching to NVM conext to allow NVM to perform it's tasks. This must be done when using user context mode (Not in thread mode)
//if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT)
//  switch_to_nvm_ctx_ext(vptr, "SwitchToNVM");

//We have removed switch_to_nvm_ctx_ext, instead that now we are using page_think_timer.
//add page think timer as sleep.
#define IS_TIMEOUT \
{ \
  cur_time = time(NULL); \
  elaps_time = cur_time - start_time; \
  NSDL4_RBU(vptr, NULL, "elaps_time = %lld, wait_time = %d\n", elaps_time, wait_time); \
  if(elaps_time > wait_time) \
  { \
    IW_UNUSED(timeout_flag = 1); \
    page_status = NS_REQUEST_TIMEOUT; \
    rbu_fill_page_status(vptr, NULL); \
    if(vptr->first_page_url->proto.http.rbu_param.merge_har_file == 0) \
    { \
      NSTL1_OUT(NULL, NULL, "Timeout - Har file for url '%s' not generated within %lld sec.\n", url, elaps_time); \
    } \
    else if(vptr->first_page_url->proto.http.rbu_param.merge_har_file == 1) \
    { \
      if(merge_first_size == 0) \
      { \
        NSTL1_OUT(NULL, NULL, "Timeout - MergeHarFile flag ON, first Har file for url '%s' not generated within %lld sec.\n", url, elaps_time); \
      } \
      else \
      { \
        NSTL1_OUT(NULL, NULL, "Timeout - MergeHarFile flag ON, second Har file for url '%s' not generated within %lld sec.\n", url, elaps_time); \
      } \
    } \
    NSDL1_RBU(vptr, NULL, "Timeout - Har file for url '%s' not generated within %lld sec.", url, elaps_time); \
    break; \
  } \
  else \
  { \
    wait_for_lgh_cb *wait_for_lgh;\
    MY_MALLOC(wait_for_lgh, sizeof(wait_for_lgh_cb),  "lighthousereportcallback", -1); \
    wait_for_lgh->vptr = vptr; \
    wait_for_lgh->start_time = start_time; \
    ns_sleep(rbu_resp_attr->timer_ptr, 600, ns_rbu_wait_for_har_file_callback, (void *)wait_for_lgh);\
    return 0; \
  } \
}

void ns_rbu_wait_for_har_file_callback(ClientData client_data)
{
  wait_for_lgh_cb *tmp_wait_for_lgh =  (wait_for_lgh_cb *) client_data.p;
  VUser *vptr = tmp_wait_for_lgh->vptr;

  unsigned short har_timeout;
  HAR_TIMEOUT

  NSDL3_RBU(vptr, NULL, "Method called, Timer Expired::, vptr = %p", vptr);

  // Calling call back function
  int ret = ns_rbu_wait_for_har_file(vptr->cur_page->page_name, vptr->httpData->rbu_resp_attr->url, vptr->httpData->rbu_resp_attr->profile, har_timeout, vptr->httpData-> rbu_resp_attr->har_log_path, vptr->httpData->rbu_resp_attr->har_log_dir, vptr->httpData->rbu_resp_attr->firefox_cmd_buf, vptr->first_page_url->proto.http.rbu_param.har_rename_flag, vptr, tmp_wait_for_lgh->start_time);
 
 FREE_AND_MAKE_NULL(tmp_wait_for_lgh, "har_file_callback", -1);
 
  if((ret != 1) && (ret != 0))
    HANDLE_RBU_PAGE_FAILURE(ret)
}

static  char dir[512];
//Only allow files and file name should have har.
int file_filter(const struct dirent *a)
{
  int len = strlen(a->d_name) - 4;
  if(len < 0) len = 0;
  //if((a->d_type == DT_REG) && strstr(a->d_name, ".har"))
  //if((nslb_get_file_type(".", a) == DT_REG) && (!strcmp(a->d_name + len, ".har")))
  if((nslb_get_file_type(dir, a) == DT_REG) && (!strcmp(a->d_name + len, ".har")))
  {
    return 1;
  }
  return 0;  
}


//Return 
// -1 : error
// -2 : not found.
//  >= 0: if success. 
// This function will get latest har file 
static int get_har_file(VUser *vptr, char *log_path, char *har_dir, char *har_file)
{  
  struct dirent **file_dirent;
  DIR *dirp;
  int num_files;
  NSDL3_RBU(vptr, NULL, "Method called, log path = %s, har_dir = %s", log_path, har_dir);  
  
  //char dir[512];
  har_file[0] = 0;
  //directory path.
  sprintf(dir, "%s/%s", log_path, har_dir);
  dirp = opendir(dir);
  num_files = scandir(dir, &file_dirent, file_filter, NULL);
  closedir(dirp);
  NSDL3_RBU(vptr, NULL, "num_files = %d", num_files);
  //Error.
  if(num_files == -1)
  {
    NSTL1_OUT(NULL, NULL, "Error: get_har_file() - scandir failed for dir = %s, error = %s\n", dir, nslb_strerror(errno));
    strncpy(vptr->httpData->rbu_resp_attr->access_log_msg, "Error: Unable to get HAR file", RBU_MAX_ACC_LOG_LENGTH);
    snprintf(script_execution_fail_msg, MAX_SCRIPT_EXECUTION_LOG_LENGTH, "Internal Error:Unable to get HAR file. Error: %s", nslb_strerror(errno));
    return -1;
  }   
 
  //No file found. 
  if(num_files == 0)
  {
    free(file_dirent);
    return -2;
  }
  
  //we have some files, find file with max modified time.
  int i, latest_file_idx = 0;
  time_t max_mtime = 0;
  long long latest_file_size = 0;
  char file_name_with_path[1024];
  struct stat har_stat;
  for(i = 0; i < num_files; i++)
  {
    sprintf(file_name_with_path, "%s/%s", dir, file_dirent[i]->d_name);
    if(stat(file_name_with_path, &har_stat))
    {
      NSTL1_OUT(NULL, NULL, "Error: get_har_file() - stat failed for file %s, error = %s\n", file_name_with_path, nslb_strerror(errno));
      //free all files name.
      while(num_files--)  free(file_dirent[num_files]);
      free(file_dirent); 
      return -1;
    }
    if(har_stat.st_mtime > max_mtime)
    {
      max_mtime = har_stat.st_mtime;
      latest_file_size = har_stat.st_size;
      latest_file_idx = i; 
    }
  }

  //save latest file name.
  strcpy(har_file, file_dirent[latest_file_idx]->d_name);
  NSDL3_RBU(vptr, NULL, "Har File \'%s\' Found, file size = %lld", har_file, latest_file_size);
  //free all files name.
  while(num_files--)  free(file_dirent[num_files]);
  free(file_dirent);
  return latest_file_size;
}

//Commented: Not in use
/* This function will make harp file from har file
*  @har_file - HAR file with absolute path
*  HARP file format - onInputData( HAR File DATA );
*  Return status - -1 on error
*                -  0 sucess   */
/*
static int make_harp(VUser *vptr, char *har_file)
{
  FILE *harp_fp = NULL;
  FILE *har_fp = NULL;
  char harp_file[1024 + 1];
  char read_buff[1024] = "";

  //HARP file name
  sprintf(harp_file, "%sp", har_file);

  NSDL2_RBU(vptr, NULL, "Method called, har_file = %s and harp_file is = %s", har_file, harp_file);

  if((harp_fp = fopen(harp_file, "w")) == NULL)
  {
    NSTL1_OUT(NULL, NULL, "Error: Failed in opening file: '%s'. Error (errno %d): [%s]\n", harp_file, errno, nslb_strerror(errno));
    return -1 ;
  }


  if((har_fp = fopen(har_file, "r")) == NULL)
  {
    NSTL1_OUT(NULL, NULL, "Error: Failed in opening file: '%s'.Error (errno %d): [%s]\n", har_file, errno, nslb_strerror(errno));
    return -1;
  }

  fputs("onInputData(\n", harp_fp);

  while((fgets(read_buff,1024,har_fp)) != NULL)
  {
    fputs(read_buff,harp_fp);
  }
  fputs(");", harp_fp);
  fclose(har_fp);
  fclose(harp_fp);
  return 0;
}
*/
  
/* This function  Rename HAR file and move it to TRxxx or .TRxxx accordingly
 *  Rename HAR file has following naming convention :
 *  Before Rename - PageName(widthXheight)+profile_name+Host_name+Date+Time.har
 *  After Rename  - P_<Before Rename>+<child_idx>_<user_index>_<sess_inst>_<page_instance>_0_<grup_num>_<sess_id>_<page_id>_0.har
 *  Return status - -1 on error
 *                   0 on sucess  */
static int rename_har_and_move_to_tr_dir(VUser *vptr, int rename_flag, char *har_file, char *page_name, char *firefox_log_path, char *har_log_dir, char *profile_name, char *renamed_har)
{
  char orig_har[1024 + 1];
  struct stat st;
  int ret = 0;
  //char renamed_har[1024 + 1];

  NSDL2_RBU(vptr, NULL, "Method Called, rename_flag = %d, har_file = %s, page_name = %s, firefox_log_path = %s, har_log_dir = %s,"
                        "profile_name = %s", rename_flag, har_file, page_name, firefox_log_path, har_log_dir, profile_name);

  sprintf(orig_har, "%s/%s/%s", firefox_log_path, har_log_dir, har_file);
  //Confirm that the har file is present in the har log path or not.
  //stat returns zero on sucess
  NSDL3_RBU(vptr, NULL, "Orig har file = %s", orig_har);
  if(stat(orig_har, &st) == -1 )
  {
    NSDL3_RBU(vptr, NULL, "Stat fails on file %s.Error = %s", orig_har, nslb_strerror(errno));
    snprintf(vptr->httpData->rbu_resp_attr->access_log_msg, RBU_MAX_ACC_LOG_LENGTH, "Error: Stat fails on HAR file");
    snprintf(script_execution_fail_msg, MAX_SCRIPT_EXECUTION_LOG_LENGTH, "Internal Error:Stat fails on file %s", orig_har);
    return -1;
  }
  //This macro returns non-zero if the file is a regular file.
  if(S_ISREG(st.st_mode) == 0)
  {
    NSDL3_RBU(vptr, NULL, "Unable to locate orig har file %s. Error = %s", orig_har, nslb_strerror(errno));
    return -1;
  }

  // Add User Identity in HAR file renaming convention i.e. child_idx, user_index, sess_idx,page_instance, group_num, session_id, page_id.
  ADD_USER_IDENTITY_IN_HAR(har_file);

  NSDL3_RBU(vptr, NULL, "After adding User index in HAR File = %s", har_file);
  //Atul set log paths
  RBU_NS_LOGS_PATH

  //If RBU_POST_PROC keyword is present then Rename and move HAR into /home/cavisson/.rbu/<.mozilla/.chrome>/logs/profile_name/.TRxx
  if (global_settings->rbu_har_rename_info_file) {
    if (rename_flag )
      sprintf(renamed_har, "%s/%s/.TR%d/P_%s+%s+%s", firefox_log_path, har_log_dir, testidx, page_name, profile_name, har_file);
    else
      sprintf(renamed_har, "%s/%s/.TR%d/%s", firefox_log_path, har_log_dir, testidx, har_file);
  } else { //If RBU_POST_PROC keyword is not present then Rename and move HAR file into $NS_WDIR/logs/<TRxx/Partition>/rbu_logs/harp_files
    if (rename_flag)
      sprintf(renamed_har, "%s/logs/%s/harp_files/P_%s+%s+%s" , g_ns_wdir, rbu_logs_file_path, page_name, profile_name, har_file);
    else
      sprintf(renamed_har, "%s/logs/%s/harp_files/%s" , g_ns_wdir, rbu_logs_file_path, har_file);
  }

  //Atul: in Case of Multidisk env we need to use mv command to move the har_file to disk 
  if (global_settings->multidisk_ns_rbu_logs_path && global_settings->multidisk_ns_rbu_logs_path[0])
  {
    char command[RBU_MAX_2K_LENGTH];
    NSDL1_RBU(vptr, NULL, "orig har is = %s, renamed har is = %s", orig_har, renamed_har);
    sprintf(command, "mv %s %s", orig_har, renamed_har );
    ret = nslb_system2(command);
    if (WEXITSTATUS(ret) == 1)
    {
      NSDL2_RBU(vptr, NULL, "Unable to move har file to rbu_logs  = %s", orig_har);
    }
    else
    {
      NSDL2_RBU(vptr, NULL, "Har file [%s] moved to rbu_logs sucessfully", renamed_har);
    }

  }
  else
  {
    if(rename(orig_har, renamed_har) < 0) 
    {
      // Moving HAR file failed
      NSTL1_OUT(NULL, NULL, "failed to move file '%s' to '%s'. Error (errno %d): [%s]\n", orig_har, renamed_har, errno, nslb_strerror(errno));
      return -1;
    }
    //Moved sucessfully 
    NSDL3_RBU(vptr, NULL, "Har file '%s' moved to '%s' successfully", orig_har, renamed_har);
  }

  #if 0
  //Remove this we don't want harp files.
  //Make HARP file from HAR FILE
  if (make_harp(vptr, renamed_har) == 0)
    NSDL3_RBU(vptr, NULL, "Harp file of %s is sucessfully made", renamed_har);
  else
  {
    NSDL3_RBU(vptr,NULL, "Unable to make harp file %s", renamed_har);
    return -1;
  }
  #endif

  return 0;
}

// This the callback function for lighthouse report wait
void wait_for_lighthouse_callback(ClientData client_data)
{
  NSDL2_RBU(NULL, NULL, "Timer Expired:: Method Called - Callback Function");
  
  wait_for_lgh_cb *tmp_wait_for_lgh = (wait_for_lgh_cb *) client_data.p;
  VUser *vptr = tmp_wait_for_lgh->vptr;
  int ret;

  unsigned short har_timeout;
  HAR_TIMEOUT

  ret = ns_rbu_wait_for_lighthouse_report(vptr, har_timeout, tmp_wait_for_lgh->start_time);
  FREE_AND_MAKE_NULL(tmp_wait_for_lgh, "wait_for_lgh", -1); 

  if(ret != 0)
    HANDLE_RBU_PAGE_FAILURE(ret)
}
/*------------------------------------------------------------------------------------------------------
 *Purpose   : This function will do following -
 *              1. Move lighthouse report as soon as found on desired path
 *              2. Create CSV(RBULightHouseRecord.csv) file
 *
 * Input    : vptr      - provide point to current virtual user 
 *            wait_time - provide timeout
 *
 * Output   : On success -  0
 *            On Failure - -1 
 *-----------------------------------------------------------------------------------------------------*/

int ns_rbu_wait_for_lighthouse_report(VUser *vptr, int wait_time, time_t start_time)
{
  char lh_rpt_file_with_ext[RBU_MAX_HAR_FILE_LENGTH + 1] = {0};
  char lh_csv_file_with_ext[RBU_MAX_HAR_FILE_LENGTH + 1] = {0};
  char tr_lh_file_path[RBU_MAX_HAR_FILE_LENGTH + 1] = {0};
  char lighthouse_filename[RBU_MAX_NAME_LENGTH + 1];
  char rbu_lh_record[RBU_MAX_HAR_FILE_LENGTH + 1];
  char *rbu_lh_buf;
  char lighthouse_report_moved = 0;
  //int csv_buf_write_idx = 0;
  long long elaps_time = 0;

  time_t cur_time;
  FILE *csv_fp = NULL;
  connection *cptr = vptr->last_cptr;
  RBU_RespAttr *rbu_resp_attr = vptr->httpData->rbu_resp_attr;
  ContinueOnPageErrorTableEntry_Shr *ptr;

  ptr = (ContinueOnPageErrorTableEntry_Shr *)runprof_table_shr_mem[vptr->group_num].continue_onpage_error_table[vptr->cur_page->page_number];

  /*Reset page stats member of structure RBU_RESP_ATTR_TIME to avoid overlapping*/
  RESET_RBU_RESP_ATTR_TIME(rbu_resp_attr);

  RBU_LightHouse *rbu_lh = rbu_resp_attr->rbu_light_house;

  //Bug 59807: Setting is_incomplete_har_file = 1, So that no data dump into RTG for lighthouse.
  rbu_resp_attr->is_incomplete_har_file = 1;

  NSDL1_RBU(vptr, NULL, "Method called, vptr = [%p], wait_time = [%d], pg_name = [%s], pg_id = [%d], filename = [%s]", 
                         vptr, wait_time, vptr->cur_page->page_name, vptr->cur_page->page_norm_id, rbu_lh->lighthouse_filename);

  RBU_NS_LOGS_PATH

  char lgh_filename[RBU_MAX_NAME_LENGTH + 6];
  strncpy(lgh_filename, rbu_lh->lighthouse_filename, RBU_MAX_NAME_LENGTH + 6);
  char *lptr;
  // Due to callback we were getting .html appended multiple times in lighthouse report file
  if((lptr = strstr(lgh_filename, ".html")) == NULL)
  {
    snprintf(lh_rpt_file_with_ext, RBU_MAX_HAR_FILE_LENGTH,
                        "%s/%s/%s.html",
                        rbu_resp_attr->har_log_path, rbu_resp_attr->profile, rbu_lh->lighthouse_filename); 
    snprintf(lh_csv_file_with_ext, RBU_MAX_HAR_FILE_LENGTH,
                        "%s/%s/csv/%s.csv",
                        rbu_resp_attr->har_log_path, rbu_resp_attr->profile, rbu_lh->lighthouse_filename); 
    snprintf(lighthouse_filename, RBU_MAX_NAME_LENGTH, "%s", rbu_lh->lighthouse_filename);
    snprintf(rbu_lh->lighthouse_filename, RBU_MAX_NAME_LENGTH + 6, "%s.html", lighthouse_filename);
  }
  else
  {
    snprintf(lh_rpt_file_with_ext, RBU_MAX_HAR_FILE_LENGTH,"%s/%s/%s",
                        rbu_resp_attr->har_log_path, rbu_resp_attr->profile, rbu_lh->lighthouse_filename);
    *lptr = '\0';
    snprintf(lh_csv_file_with_ext, RBU_MAX_HAR_FILE_LENGTH, "%s/%s/csv/%s.csv",
                        rbu_resp_attr->har_log_path, rbu_resp_attr->profile, lgh_filename);
  }
  
  NSDL4_RBU(vptr, NULL, "lighthouse_filename = %s csv = %s", rbu_lh->lighthouse_filename, lh_csv_file_with_ext);

  rbu_lh->lh_file_date_time = start_time - global_settings->unix_cav_epoch_diff;
 
  NSDL4_RBU(vptr, NULL, "Checking lighthouse report file made or not at [%s]", lh_rpt_file_with_ext);
  while(1)
  {
    cur_time = time(NULL);
    elaps_time = cur_time - start_time;
    //Case : Accessing lighthouse file at path 
    if((access(lh_rpt_file_with_ext, F_OK) != -1) && (access(lh_csv_file_with_ext, F_OK) != -1))
    {
      NSDL4_RBU(vptr, NULL, "Lighthouse report file %s made succefully.", lh_rpt_file_with_ext);
      NSDL4_RBU(vptr, NULL, "Lighthouse csv file %s made succefully.", lh_csv_file_with_ext);
      
      sprintf(tr_lh_file_path ,"%s/logs/%s/lighthouse/%s", 
                                g_ns_wdir, rbu_logs_file_path, rbu_lh->lighthouse_filename); 

      if(rename(lh_rpt_file_with_ext, tr_lh_file_path))
      {
        //Case : Report successfully made but not moved
        NSTL1_OUT(NULL, NULL, "failed to move lighthouse report \'%s\' to \'%s\'", lh_rpt_file_with_ext, tr_lh_file_path);
        snprintf(rbu_resp_attr->access_log_msg, RBU_MAX_ACC_LOG_LENGTH, "Error: failed to move lighthouse report '%s'.", lh_rpt_file_with_ext);
        snprintf(script_execution_fail_msg, MAX_SCRIPT_EXECUTION_LOG_LENGTH,
                    "Error: failed to move lighthouse report '%s'.", lh_rpt_file_with_ext);
        return -1;
      }
   
      //Case : successfully made and moved
      NSDL3_RBU(vptr, NULL, "lighthouse report file \'%s\' moved to \'%s\' successfully", lh_rpt_file_with_ext, tr_lh_file_path);

      //If Report moved successfully, fopen csv, and work accordingly
      if ((csv_fp = fopen(lh_csv_file_with_ext, "r")) == NULL)
      {
        fprintf(stderr,"Failed to read file '%s.'\n", lh_csv_file_with_ext);
        snprintf(rbu_resp_attr->access_log_msg, RBU_MAX_ACC_LOG_LENGTH, "Error: Failed to read file '%s'.", lh_csv_file_with_ext);
        snprintf(script_execution_fail_msg, MAX_SCRIPT_EXECUTION_LOG_LENGTH, "Error: Failed to read file '%s'.", lh_csv_file_with_ext);
        return -1;
      } 

      /* Read csv made by extension and dump into lightHouse structure.*/
      //We have 11 comma separated value in RBULightHouseRecord
      // We will have one line in csv at once.
      if(fgets(rbu_lh_record, RBU_MAX_VALUE_LENGTH, csv_fp))
      {
        rbu_lh_buf = strtok(rbu_lh_record, ",");   //First value of csv file
        rbu_lh->performance_score = atoi(rbu_lh_buf); 
        NSDL3_RBU(vptr, NULL, "LHR |performance_score = %d ", rbu_lh->performance_score);
 
        rbu_lh_buf = strtok(NULL, ",");                  //Second value of csv file
        rbu_lh->pwa_score = atoi(rbu_lh_buf); 
        NSDL3_RBU(vptr, NULL, "LHR | PWAScore= %d ", rbu_lh->pwa_score);
 
        rbu_lh_buf = strtok(NULL, ",");                  //Third value of csv file
        rbu_lh->accessibility_score = atoi(rbu_lh_buf); 
        NSDL3_RBU(vptr, NULL, "LHR | accessibility_score = %d ", rbu_lh->accessibility_score);
 
        rbu_lh_buf = strtok(NULL, ",");                  //Fourth value of csv file
        rbu_lh->best_practice_score = atoi(rbu_lh_buf); 
        NSDL3_RBU(vptr, NULL, "LHR | best_practice_score = %d ", rbu_lh->best_practice_score);
        
        rbu_lh_buf = strtok(NULL, ",");                  //Fifth value of csv file
        rbu_lh->seo_score = atoi(rbu_lh_buf); 
        NSDL3_RBU(vptr, NULL, "LHR | seo_score = %d ", rbu_lh->seo_score);
 
        rbu_lh_buf = strtok(NULL, ",");                  //Sixth value of csv file
        rbu_lh->first_contentful_paint_time = atoi(rbu_lh_buf); 
        NSDL3_RBU(vptr, NULL, "LHR | first_contentful_paint_time = %d ", rbu_lh->first_contentful_paint_time);
 
        rbu_lh_buf = strtok(NULL, ",");                  //Seventh value of csv file
        rbu_lh->first_meaningful_paint_time = atoi(rbu_lh_buf); 
        NSDL3_RBU(vptr, NULL, "LHR | first_meaningful_paint_time = %d ", rbu_lh->first_meaningful_paint_time);
 
        rbu_lh_buf = strtok(NULL, ",");                  //Eighth value of csv file
        rbu_lh->speed_index = atoi(rbu_lh_buf); 
        NSDL3_RBU(vptr, NULL, "LHR | speed_index = %d ", rbu_lh->speed_index);
 
        rbu_lh_buf = strtok(NULL, ",");                  //Ninth value of csv file
        rbu_lh->first_CPU_idle = atoi(rbu_lh_buf); 
        NSDL3_RBU(vptr, NULL, "LHR | first_CPU_idle = %d ", rbu_lh->first_CPU_idle);
 
        rbu_lh_buf = strtok(NULL, ",");                  //Tenth value of csv file
        rbu_lh->time_to_interact = atoi(rbu_lh_buf); 
        NSDL3_RBU(vptr, NULL, "LHR | time_to_interact = %d ", rbu_lh->time_to_interact);
 
        rbu_lh_buf = strtok(NULL, ",");                  //Elevanth value of csv file
        rbu_lh->input_latency = atoi(rbu_lh_buf); 
        NSDL3_RBU(vptr, NULL, "LHR | input_latency = %d ", rbu_lh->input_latency);

        rbu_lh_buf = strtok(NULL, ",");
	rbu_lh->largest_contentful_paint = rbu_lh_buf ? atoi(rbu_lh_buf): -1;

        rbu_lh_buf = strtok(NULL, ",");
        rbu_lh->total_blocking_time = rbu_lh_buf ? atoi(rbu_lh_buf): -1;

        rbu_lh_buf = strtok(NULL, ",");
        rbu_lh->cum_layout_shift = rbu_lh_buf ? atof(rbu_lh_buf): -1.0;
                  
        NSDL3_RBU(vptr, NULL, "LHR | largest_contentful_paint = %d", rbu_lh->largest_contentful_paint);
        NSDL3_RBU(vptr, NULL, "LHR | total_blocking_time = %d", rbu_lh->total_blocking_time);
        NSDL3_RBU(vptr, NULL, "LHR | cum_layout_shift = %f", rbu_lh->cum_layout_shift);
      }

      fclose(csv_fp);
       
      lighthouse_report_moved = 1;
      break;
    }
    else         //Case : Not Found
    {
      NSDL4_RBU(vptr, NULL, "elaps_time = %lld, wait_time = %d\n", elaps_time, wait_time);
      if(elaps_time > wait_time)
      {
        NSDL1_RBU(vptr, NULL, "Timeout - lighthouse report or csv file not generated within %lld sec.", elaps_time);
        snprintf(script_execution_fail_msg, MAX_SCRIPT_EXECUTION_LOG_LENGTH,
                    "Timeout - lighthouse report or csv file not generated within %lld sec.", elaps_time);

        RBU_SET_PAGE_STATUS_ON_ERR(vptr, rbu_resp_attr, NS_REQUEST_TIMEOUT, ptr);
        strcat(rbu_resp_attr->access_log_msg, " - lighthouse report or csv file not generated.");

        rbu_fill_page_status(vptr, NULL);
        break;
      }
      NSDL4_RBU(vptr, NULL, "lighthouse report or csv file not made sleep for 600 ms. Error: %d:(%s)", errno, nslb_strerror(errno));
      VUSER_SLEEP_WAIT_FOR_LIGHT_HOUSE_REPORT(vptr, 600);
    }
  }

  //For phase 1:  we are assuming each page with status 200 if lighthouse report is made successfully
  if(lighthouse_report_moved == 1)
  {
    NSDL4_RBU(vptr, NULL, "Setting request status. page_load_time = %lld sec", elaps_time);
    NS_DT1(vptr, NULL, DM_L1, MM_VARS, "Lighthouse Report File : \'%s\'", rbu_lh->lighthouse_filename);
    rbu_resp_attr->page_load_time = elaps_time * 1000;
    rbu_resp_attr->status_code = 200;
    cptr->req_code = 200;
    cptr->req_ok = NS_REQUEST_OK;
  }
  else
  {
    printf("\nTimeout - Lighthouse report file for '%s' not generated within %lld sec.\n", rbu_lh->lighthouse_filename, elaps_time);
    NS_DT3(vptr, NULL, DM_L1, MM_VARS, "Timeout - lighthouse report file for '%s' not generated within %lld sec.",
                 rbu_lh->lighthouse_filename, elaps_time);
  }

  NSDL2_RBU(vptr, NULL, "Waiting for handle page complete done, page_name = %s, page_id = %d",
                           vptr->cur_page->page_name, vptr->cur_page->page_id);
  vut_add_task(vptr, VUT_RBU_WEB_URL_END);

  return 0;
}
/*------------------------------------------------------------------------------------------------------
 *Purpose   : In this function we will move html file as soon as it is found on desired path
 *
 *-----------------------------------------------------------------------------------------------------*/
void move_html_snapshot_to_tr_callback(ClientData client_data)
{
  wait_for_move_snap_html *tmp_wait_for_snap = (wait_for_move_snap_html *)client_data.p;
  NSDL1_RBU(tmp_wait_for_snap->vptr, NULL, "Method Called:: Timer Expired"); 
  move_html_snapshot_to_tr(tmp_wait_for_snap->vptr, tmp_wait_for_snap->page_load_wait_time, tmp_wait_for_snap->start_time, tmp_wait_for_snap->ret);
  FREE_AND_MAKE_NULL(tmp_wait_for_snap, "move_html_snapshot_to_tr", -1);
}

void move_html_snapshot_to_tr(VUser *vptr, int wait_time, time_t start_time, int ret)
{
  char ss_file_with_ext[RBU_MAX_HAR_FILE_LENGTH + 1];
  char tr_ss_file_path[RBU_MAX_HAR_FILE_LENGTH + 1];
  long long elaps_time = 0;
  time_t cur_time;

  NSDL1_RBU(vptr, NULL, "Method called, vptr = [%p], wait_time = [%d]", vptr, wait_time);

  RBU_NS_LOGS_PATH

  snprintf(ss_file_with_ext, RBU_MAX_FILE_LENGTH,
                        "%s/%s/page_screen_shot_%hd_%u_%u_%d_0_%d_%d_%d_0.html",
                        vptr->httpData->rbu_resp_attr->har_log_path, vptr->httpData->rbu_resp_attr->profile, child_idx, vptr->user_index,
                        vptr->sess_inst, vptr->page_instance, vptr->group_num, GET_SESS_ID_BY_NAME(vptr), GET_PAGE_ID_BY_NAME(vptr));
  snprintf(tr_ss_file_path, RBU_MAX_HAR_FILE_LENGTH, "%s/logs/%s/screen_shot/page_screen_shot_%hd_%u_%u_%d_0_%d_%d_%d_0.html", 
                            g_ns_wdir, rbu_logs_file_path, child_idx, vptr->user_index, vptr->sess_inst,
                            vptr->page_instance, vptr->group_num, GET_SESS_ID_BY_NAME(vptr), GET_PAGE_ID_BY_NAME(vptr));
  NSDL4_RBU(vptr, NULL, "Checking html screenshot file made or not ss_file_with_ext = [%s]", ss_file_with_ext);
  while(1)
  {
    if(access(ss_file_with_ext, F_OK) != -1)
    {
      NSDL4_RBU(vptr, NULL, "Screen shot file %s made succefully.", ss_file_with_ext);
      if(rename(ss_file_with_ext, tr_ss_file_path))
      {
        NSTL1(NULL, NULL, "failed to move html screenshot \'%s\' to \'%s\'\n", ss_file_with_ext,tr_ss_file_path);
        NSDL3_RBU(vptr, NULL, "failed to move html screenshot \'%s\' to \'%s\'\n", ss_file_with_ext,tr_ss_file_path);
      }
      else 
      {
        NSDL3_RBU(vptr, NULL, "HTML screenshot file \'%s\' moved to \'%s\' successfully", ss_file_with_ext, tr_ss_file_path);
      }
      break;
    }
    else
    {
      cur_time = time(NULL);
      elaps_time = cur_time - start_time;
      NSDL4_RBU(vptr, NULL, "elaps_time = %lld, wait_time = %d\n", elaps_time, wait_time);
      if(elaps_time > wait_time)
      {
        NSTL1(vptr, NULL, "Timeout - HTML screenshot file for '%s' not generated within %lld sec.\n", ss_file_with_ext, elaps_time);
        NSDL1_RBU(vptr, NULL, "Timeout - HTML screenshot file for '%s' not generated within %lld sec.\n", ss_file_with_ext, elaps_time);
        break;
      }
      NSDL4_RBU(vptr, NULL, "HTML screenshot file not made, sleep for 600 ms. Error: %d:(%s)", errno, nslb_strerror(errno));
      VUSER_SLEEP_MOVE_HTML_SNAPSHOT_TO_TR(vptr, 600);
    }
  }
  perform_task_after_rbu_failure(ret, vptr);
}


void move_snapshot_to_tr_v2_callback(ClientData client_data)
{
  // We need here complete structure
  web_url_end_cb *tmp_move_snap_shot_v2 = (web_url_end_cb *)client_data.p;
  VUser *vptr = tmp_move_snap_shot_v2->vptr; 
  NSDL2_RBU(NULL, NULL, "Method Called:: Timer Expired:: %p", tmp_move_snap_shot_v2);
 
  unsigned short har_timeout;
  HAR_TIMEOUT
  
  move_snapshot_to_tr_v2(tmp_move_snap_shot_v2->vptr, har_timeout, tmp_move_snap_shot_v2->start_time, tmp_move_snap_shot_v2->renamed_har_file_with_abs_path, tmp_move_snap_shot_v2->prev_har_file, tmp_move_snap_shot_v2->page_status, tmp_move_snap_shot_v2->performance_trace_start_time);

  FREE_AND_MAKE_NULL(tmp_move_snap_shot_v2, "move_snapshot_to_v2", -1);
}

/*------------------------------------------------------------------------------------------------------
 *Purpose   : In this function we will move snapshot as soon as snapshot found on desired path
 *
 *-----------------------------------------------------------------------------------------------------*/
/* THIS FUNCTION IS DUPLICATE OF move_snapshot_to_tr with some changes. PLEASE CORRECT THIS ***********/

int move_snapshot_to_tr_v2(VUser *vptr, int wait_time, time_t start_time, char *renamed_har_file, char *prev_har_file, int page_status, time_t performance_trace_start_time)
{
  char ss_file_with_ext[RBU_MAX_HAR_FILE_LENGTH] = {0};
  char tr_ss_file_path[RBU_MAX_HAR_FILE_LENGTH] = {0};
  long long elaps_time = 0;
  time_t  cur_time;
  int ret;
  NSDL1_RBU(vptr, NULL, "Method called, vptr = [%p], wait_time = [%d]", vptr, wait_time);

  RBU_NS_LOGS_PATH
  sprintf(ss_file_with_ext, "%s.jpeg", vptr->httpData->rbu_resp_attr->page_screen_shot_file);
  NSDL4_RBU(vptr, NULL, "Checking screen shot file made or not ss_file_with_ext = [%s]", ss_file_with_ext);
  while(1)
  {
    if(access(ss_file_with_ext, F_OK) != -1)
    {
      NSDL4_RBU(vptr, NULL, "Screen shot file %s made succefully.", ss_file_with_ext);
      if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.browser_mode == RBU_BM_CHROME)
      {
        sprintf(tr_ss_file_path ,"%s/logs/%s/screen_shot/page_screen_shot_%hd_%u_%u_%d_0_%d_%d_%d_0.jpeg", 
                                  g_ns_wdir, rbu_logs_file_path, 
                                  child_idx, vptr->user_index, vptr->sess_inst,
                                  vptr->page_instance, vptr->group_num, GET_SESS_ID_BY_NAME(vptr), GET_PAGE_ID_BY_NAME(vptr));
        if(rename(ss_file_with_ext, tr_ss_file_path))
        {
          NSTL1_OUT(NULL, NULL, "failed to move screen shot \'%s\' to \'%s\'\n", ss_file_with_ext,tr_ss_file_path);
          NSDL3_RBU(vptr, NULL, "failed to move screen shot \'%s\' to \'%s\'\n", ss_file_with_ext,tr_ss_file_path);
        }
        else 
        {
          NSDL3_RBU(vptr, NULL, "screen shot file \'%s\' moved to \'%s\' successfully", ss_file_with_ext, tr_ss_file_path);
        }
      }
      else
        NSDL3_RBU(vptr, NULL, "screen shot file \'%s\' moved successfully", ss_file_with_ext);
      break;
    }
    else
    {
      cur_time = time(NULL);
      elaps_time = cur_time - start_time;
      NSDL4_RBU(vptr, NULL, "elaps_time = %lld, wait_time = %d\n", elaps_time, wait_time);
      if(elaps_time > wait_time)
      {
        printf("Timeout - Screen shot file for '%s' not generated within %lld sec.\n", ss_file_with_ext, elaps_time);
        NSDL1_RBU(vptr, NULL, "Timeout..................\n");
        break;
      }
      NSDL4_RBU(vptr, NULL, "screen shot file not made sleep for 300 ms. Error: %d:(%s)", errno, nslb_strerror(errno));
      VUSER_SLEEP_MOVE_SNAPSHOT_TO_TR_V2(vptr, 600);
    }
  }
  vptr->httpData->rbu_resp_attr->retry_count = 0; 
  ret = ns_rbu_web_url_end(vptr, vptr->httpData->rbu_resp_attr->url, renamed_har_file, vptr->httpData->rbu_resp_attr->har_file_prev_size, vptr->httpData->rbu_resp_attr->profile, prev_har_file, page_status, performance_trace_start_time);
  if(ret < 0)
  {
    HANDLE_RBU_PAGE_FAILURE(ret)
  }
 
  return ret;
}

void move_snapshot_to_tr_callback(ClientData client_data)
{
  wait_for_move_snap_html *tmp_wait_for_move = (wait_for_move_snap_html *)client_data.p; 
  NSDL1_RBU(NULL, NULL, "Method called, Timer Expired");  
  move_snapshot_to_tr(tmp_wait_for_move->vptr, tmp_wait_for_move->page_load_wait_time, tmp_wait_for_move->start_time, tmp_wait_for_move->ret);
  FREE_AND_MAKE_NULL(tmp_wait_for_move, "tmp_wait_for_move", -1); 

}
/*------------------------------------------------------------------------------------------------------
 *Purpose   : In this function we will move snapshot as soon as snapshot found on desired path
 *
 *-----------------------------------------------------------------------------------------------------*/
int move_snapshot_to_tr(VUser *vptr, int wait_time, time_t start_time, int ret)
{
  char ss_file_with_ext[RBU_MAX_HAR_FILE_LENGTH] = {0};
  char tr_ss_file_path[RBU_MAX_HAR_FILE_LENGTH] = {0};
  long long elaps_time = 0;
  time_t  cur_time;

  NSDL1_RBU(vptr, NULL, "Method called, vptr = [%p], wait_time = [%d]", vptr, wait_time);

  RBU_NS_LOGS_PATH
  sprintf(ss_file_with_ext, "%s.jpeg", vptr->httpData->rbu_resp_attr->page_screen_shot_file);
  NSDL4_RBU(vptr, NULL, "Checking screen shot file made or not ss_file_with_ext = [%s]", ss_file_with_ext);
  while(1)
  {
    if(access(ss_file_with_ext, F_OK) != -1)
    {
      NSDL4_RBU(vptr, NULL, "Screen shot file %s made succefully.", ss_file_with_ext);
      if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.browser_mode == RBU_BM_CHROME)
      {
        sprintf(tr_ss_file_path ,"%s/logs/%s/screen_shot/page_screen_shot_%hd_%u_%u_%d_0_%d_%d_%d_0.jpeg", 
                                  g_ns_wdir, rbu_logs_file_path, 
                                  child_idx, vptr->user_index, vptr->sess_inst,
                                  vptr->page_instance, vptr->group_num, GET_SESS_ID_BY_NAME(vptr), GET_PAGE_ID_BY_NAME(vptr));
        if(rename(ss_file_with_ext, tr_ss_file_path))
        {
          NSTL1_OUT(NULL, NULL, "failed to move screen shot \'%s\' to \'%s\'\n", ss_file_with_ext,tr_ss_file_path);
          NSDL3_RBU(vptr, NULL, "failed to move screen shot \'%s\' to \'%s\'\n", ss_file_with_ext,tr_ss_file_path);
        }
        else 
        {
          NSDL3_RBU(vptr, NULL, "screen shot file \'%s\' moved to \'%s\' successfully", ss_file_with_ext, tr_ss_file_path);
        }
      }
      else
        NSDL3_RBU(vptr, NULL, "screen shot file \'%s\' moved successfully", ss_file_with_ext);
      break;
    }
    else
    {
      cur_time = time(NULL);
      elaps_time = cur_time - start_time;
      NSDL4_RBU(vptr, NULL, "elaps_time = %lld, wait_time = %d\n", elaps_time, wait_time);
      if(elaps_time > wait_time)
      {
        printf("Timeout - Screen shot file for '%s' not generated within %lld sec.\n", ss_file_with_ext, elaps_time);
        NSDL1_RBU(vptr, NULL, "Timeout..................\n");
        break;
      }
      NSDL4_RBU(vptr, NULL, "screen shot file not made sleep for 300 ms. Error: %d:(%s)", errno, nslb_strerror(errno));
      VUSER_SLEEP_MOVE_SNAPSHOT_TO_TR(vptr, 600);
    }
  }

  if((ret == -1) && (runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.browser_mode == RBU_BM_CHROME))
  {
    time_t start_time1 = time(NULL);
    move_html_snapshot_to_tr(vptr, wait_time, start_time1, ret);
    return 0;
  }
  perform_task_after_rbu_failure(ret, vptr);
  return 0;
}

/*------------------------------------------------------------------- 
 * Purpose   : In this function we wait for har file generation as soon as har file found on desird path
 *             we return from this function   
 *
 * Input     : 
 *             page_name - Name to which we have to rename the har file. 
 *                         This is mandatory option because bydefault rename har file option is enable
 *
 *             url       - Url which have to be hit. 
 *                         This is also mandatory option
 *                         For single Profile it will used like-
 *                         http://www.yahoo.com/
 *
 *                         For multiprofile url should be given like-
 *                        -P <profile name> http://www.yahoo.com/ 
 *
 *             wait_time  - Time taken to load the page which is given by url. default is 30 sec.
 *
 *             har_file_flag - This will indicate har file have to be renamed or not?
 *                             1 - rename the har file (default)
 *                             0 - not rename the har file
 *
 *             vnc_display   - Implies vnc server display number. default is 1
 *
 *             log_file      - Name of the log file where har file is generated. default name is logs  
 *
 * Output    : On success - 0 
 *             On failure - -1 
 *             On Abort   - -2 
 *-------------------------------------------------------------------*/
int ns_rbu_wait_for_har_file(char *page_name, char *url, char *prof_name, int wait_time, char *firefox_log_path, char *har_log_dir, char *cmd_buf, int har_file_flag, VUser *vptr, time_t start_time)
{
  RBU_RespAttr *rbu_resp_attr;
  char cur_har_file[RBU_MAX_HAR_FILE_LENGTH];
  //char prev_har_file[RBU_MAX_HAR_FILE_LENGTH]="";
  char merge_first_har[RBU_MAX_HAR_FILE_LENGTH];
  char file_list[1024 + 1];
 // unsigned int prev_size = 0;
  unsigned int cur_size = 0;
  unsigned int merge_first_size = 0;
  IW_UNUSED(int timeout_flag = 0);
  long long elaps_time = 0;
  time_t  cur_time;
  int found = 0;
  int merge_done = 0;
  int page_status = NS_REQUEST_ERRMISC;
  int ret;
  char renamed_page_name[512];
  char renamed_har_file_with_abs_path[1024];
  time_t performance_trace_start_time;

  /* Initialize */
  cur_har_file[0] = 0;
  //prev_har_file[0] = 0;
  cmd_buf[0] = 0;
  merge_first_har[0] = 0;
  file_list[0] = 0;
  renamed_har_file_with_abs_path[0] = 0;

  rbu_resp_attr = vptr->httpData->rbu_resp_attr;

  ContinueOnPageErrorTableEntry_Shr *ptr;
  ptr = (ContinueOnPageErrorTableEntry_Shr *)runprof_table_shr_mem[vptr->group_num].continue_onpage_error_table[vptr->cur_page->page_number];

  NSDL1_RBU(vptr, NULL, "Method called, page_name = [%s], url = [%s], prof_name = [%s], wait_time = %d, firefox_log_path = [%s], "
                         "har_log_dir = [%s], har_file_flag = %d",
                         page_name, url, prof_name, wait_time, firefox_log_path, har_log_dir, har_file_flag);


  /* Wait till har file is not generated */
  NSDL2_RBU(vptr, NULL, "Waiting for browser to load the page for %d seconds", wait_time);
  //removed make hidden TR code from here Atul.
  while(1)
  {
    ret = get_har_file(vptr, firefox_log_path, har_log_dir, cur_har_file);
    NSDL2_RBU(vptr, NULL, "get_har_file() return status = %d", ret);
    /* Enhancement-8591: If HAR not made successfully and G_CONTINUE_ON_PAGE_ERR is 0, then session will be aborted. 
       If HAR is not made, then ret could be -1 or -2. 
       Here ret -1 => Unable to open profile log dir (Eg: /home/cavisson/.rbu/.mozilla/firfox/logs/<profile_name>) 
            ret -2 => HAR file is not made

      on ret -1 n -2 we should set is_incomplete_har_file flag as 1
    */
    if(ret == -1)
    {
      NSDL3_RBU(vptr, NULL, "Unable to open profile log dir, hence setting page_status, sess_status & req_ok to MiscErr.");
      strncpy(rbu_resp_attr->access_log_msg, 
              "Unable to open profile log dir, hence setting page_status, sess_status & req_ok to MiscErr.", RBU_MAX_ACC_LOG_LENGTH);
      strncpy(script_execution_fail_msg, "Internal Error:Unable to open profile log dir.", MAX_SCRIPT_EXECUTION_LOG_LENGTH); 
      RBU_SET_PAGE_STATUS_ON_ERR(vptr, rbu_resp_attr, NS_REQUEST_ERRMISC, ptr);

      cur_size = 0;
      break;
    }
    //File not found.
    else if(ret == -2) 
      cur_size = 0;

    //File found.
    else 
      cur_size = ret; 
    
    if(cur_size != 0 && rbu_resp_attr->har_file_prev_size == 0) //Har file found
    {
      rbu_resp_attr->har_file_prev_size = cur_size;
      //strcpy(prev_har_file, cur_har_file);
      strcpy(rbu_resp_attr->prev_har_file_name, cur_har_file);
      NSDL1_RBU(vptr, NULL, "Har file '%s' for url %s found. Size = %d\n", cur_har_file, url, cur_size);
      //start timer for performance trace dump
      //performance_trace_start_time = time(NULL);
      IS_TIMEOUT
    }
    if(cur_size != 0 && rbu_resp_attr->har_file_prev_size != 0) //Checking har file is in the appending mode or not
    {
      NSDL4_RBU(vptr, NULL, "cur_size = %d, prev_size = %d, cur_har_file = [%s], prev_har_file = [%s]\n", 
                                     cur_size, rbu_resp_attr->har_file_prev_size, cur_har_file, rbu_resp_attr->prev_har_file_name);
      if((cur_size > rbu_resp_attr->har_file_prev_size) && (!strcmp(cur_har_file, rbu_resp_attr->prev_har_file_name)))  
      {
        NSDL2_RBU(vptr, NULL, "Har file '%s' is in appending mode.\n", rbu_resp_attr->prev_har_file_name);
        rbu_resp_attr->har_file_prev_size = cur_size;
        IS_TIMEOUT
      }
      
      //Case: cur_size = prev_size
      // Handling case when one url made more than one har file as in Khols issue
      NSDL4_RBU(vptr, NULL, "merge_har_file = %d", vptr->first_page_url->proto.http.rbu_param.merge_har_file);
      if(vptr->first_page_url->proto.http.rbu_param.merge_har_file && !merge_done)
      {
        NSDL4_RBU(vptr, NULL, "found = %d, merge_done = %d", found, merge_done);
        if(found)
        {
          sprintf(file_list, "%s,%s", merge_first_har, cur_har_file);
          #ifdef NS_DEBUG_ON 
            sprintf(cmd_buf, "chm_har_file -o mergeGetHarstats -p %s -l %s -t %d -F %s -P %s -i %lld -D 1", 
                                                           firefox_log_path, har_log_dir, testidx, file_list, page_name, vptr->partition_idx);
            NSDL4_RBU(vptr, NULL, "Running command to merge har files - [%s]", cmd_buf);
          #else
            sprintf(cmd_buf, "chm_har_file -o mergeGetHarstats -p %s -l %s -t %d -F %s -P %s -i %lld",
                                                           firefox_log_path, har_log_dir, testidx, file_list, page_name, vptr->partition_idx);
            NSDL4_RBU(vptr, NULL, "Running command to merge har files - [%s]", cmd_buf);
          #endif
          nslb_system2(cmd_buf);

          rbu_resp_attr->har_file_prev_size = cur_size = 0;
          merge_done=1;
          continue;
        }

        //Check if har made and if merge flag on then wait for other har file
        if(!merge_first_size && !strcmp(cur_har_file, rbu_resp_attr->prev_har_file_name) && !found) //Found first Merge file
        {
          NSDL4_RBU(vptr, NULL, "Found first merge har file %s, size = %u", cur_har_file, cur_size);
          strcpy(merge_first_har, cur_har_file);
          merge_first_size = cur_size;
          //prev_size = cur_size = 0;
          continue;
        }
        else if(strcmp(cur_har_file, rbu_resp_attr->prev_har_file_name) && !found) //comes only one
        {
          NSDL4_RBU(vptr, NULL, "Found second merge har file %s, size = %u", cur_har_file, cur_size);
          rbu_resp_attr->har_file_prev_size = cur_size;
          strcpy(rbu_resp_attr->prev_har_file_name, cur_har_file);
          found=1;
          continue;
        }
        else
          IS_TIMEOUT
      }

      NSDL2_RBU(vptr, NULL, "*** Har file '%s' of url '%s' completely made of size %lu.****\n", rbu_resp_attr->prev_har_file_name, url, cur_size);
      #ifdef NS_DEBUG_ON
        printf("Har file '%s' of url '%s' completely made of size %u in %lld sec.\n", rbu_resp_attr->prev_har_file_name, url, cur_size, elaps_time);
      #endif
      break;
    }
    else //cur_size = 0 , Har file not found
      IS_TIMEOUT
  }
  
  performance_trace_start_time = time(NULL);

  if(cur_size == 0)
  {
    rbu_resp_attr->is_incomplete_har_file = 1;
    //Handling T.O. Err, url end and context at ns_rbu_execute_page()
    NSDL2_RBU(vptr, NULL, "num_retry_on_page_failure - %d and retry_count_on_abort - %d", 
                            runprof_table_shr_mem[vptr->group_num].gset.num_retry_on_page_failure, vptr->retry_count_on_abort);
    if(runprof_table_shr_mem[vptr->group_num].gset.num_retry_on_page_failure > vptr->retry_count_on_abort) {
      snprintf(rbu_resp_attr->access_log_msg, RBU_MAX_ACC_LOG_LENGTH, "Error: Page '%s' is not loaded completely in the browser. "
                                             "Hence Session Aborted and going to Retry", page_name);
     rbu_resp_attr->har_file_prev_size = 0;
     return -2;
    }

    RBU_SET_PAGE_STATUS_ON_ERR(vptr, rbu_resp_attr, NS_REQUEST_TIMEOUT, ptr);
    ns_rbu_log_access_log(vptr, rbu_resp_attr, RBU_ACC_LOG_DUMP_LOG);
  }

  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.screen_size_sim == 0) 
    snprintf(renamed_page_name, 512, "%s", page_name);
  else
    snprintf(renamed_page_name, 512, "%s(%dx%d)", page_name, vptr->screen_size->width, vptr->screen_size->height);

  /* Resolve Bug 17123: */
  if(rbu_resp_attr->har_file_prev_size && cur_har_file[0])
  {
    if(rename_har_and_move_to_tr_dir(vptr, har_file_flag, cur_har_file, renamed_page_name, firefox_log_path, har_log_dir, prof_name, renamed_har_file_with_abs_path) == 0)
      NSDL2_RBU(vptr, NULL, "Sucessfully renamed and  moved har file");
    else
    {
      NSDL2_RBU(vptr, NULL, "Unable to renamed and moved har file");
      //return -1;
    }
  }

  char time_taken[256];
  char timeout[256];
  sprintf(time_taken, "%lld sec", elaps_time);
  sprintf(timeout, "timeout in %d sec", wait_time);
  /* BBT Report */
  NSDL2_RBU(vptr, NULL, "Page = [%s], Profile = [%s], Har File = [%s], Har File size = %lu, Time taken = %lld",
                                 page_name, prof_name, rbu_resp_attr->prev_har_file_name[0]?rbu_resp_attr->prev_har_file_name:"Not Found", rbu_resp_attr->har_file_prev_size, timeout_flag?timeout:time_taken);
  #ifdef NS_DEBUG_ON
  printf("Browser Based Report -------------\n"
         "  Group          : %s\n"
         "  Page           : %s\n"
         "  Profile        : %s\n"
         "  Vnc Display id : %d\n"
         "  Har File       : %s\n"
         "  Har File size  : %u bytes\n"
         "  Time taken     : %s\n",
         runprof_table_shr_mem[vptr->group_num].scen_group_name, page_name, prof_name, vptr->httpData->rbu_resp_attr->vnc_display,
         rbu_resp_attr->prev_har_file_name[0]?rbu_resp_attr->prev_har_file_name:"Not Found",rbu_resp_attr->har_file_prev_size, (vptr->first_page_url->proto.http.rbu_param.merge_har_file == 0)?(timeout_flag?timeout:time_taken):(found?time_taken:"Timeout for second Har file"));
  #endif

  /* Enhancement-8591: If Har file not found and G_CONTINUE_ON_PAGE_ERR 0 then session will be aborted */
  NSDL3_RBU(vptr, NULL, "prev_size = %u, continue_on_pg_err = %d", rbu_resp_attr->har_file_prev_size, ptr->continue_error_value);
  if(rbu_resp_attr->har_file_prev_size == 0)
  {
    NSDL3_RBU(vptr, NULL, "Har file not found, hence setting page_status, sess_status & req_ok to MiscErr.");
    strncpy(script_execution_fail_msg, "Error: Page is not loaded completely in browser.", MAX_SCRIPT_EXECUTION_LOG_LENGTH);

    RBU_SET_PAGE_STATUS_ON_ERR(vptr, rbu_resp_attr, NS_REQUEST_TIMEOUT, ptr);
  }

  //AKS: Moving snap shot to TRxxx from .rbu/.chrome/logs here
  RBU_NS_LOGS_PATH

  sprintf(cmd_buf, "mv %s/%s/clips/*.jpeg %s/logs/%s/snap_shots >/dev/null 2>&1", 
              firefox_log_path, har_log_dir, g_ns_wdir, rbu_logs_file_path);
  NSDL2_RBU(vptr, NULL, "Running command to move snap shot file = %s", cmd_buf);
  ret =  nslb_system2(cmd_buf);
  if (WEXITSTATUS(ret) != 1)
  {
    NSDL2_RBU(vptr, NULL, "snap shot for rbu_logs_file_path = %s moved sucessfully", rbu_logs_file_path);
  }
  else 
  {
    NSDL2_RBU(vptr, NULL, "Unable to move snap shot for path  = %s", rbu_logs_file_path);
  }

  if(!renamed_har_file_with_abs_path[0])
  {
    NS_DT3(vptr, NULL, DM_L1, MM_VARS, "Error: HAR file for '%s' not found.", page_name);
    snprintf(script_execution_fail_msg, MAX_SCRIPT_EXECUTION_LOG_LENGTH, "Internal Error: Page '%s' is not loaded completely in browser.", page_name);
  }
  NS_DT3(vptr, NULL, DM_L1, MM_VARS, "HAR File : \'%s\'", renamed_har_file_with_abs_path);

  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_screen_shot &&
        NS_IF_PAGE_DUMP_ENABLE && runprof_table_shr_mem[vptr->group_num].gset.trace_level > TRACE_ONLY_REQ_RESP)
  {
    time_t start_time = time(NULL);
    /* Made two version of move_snap_shot_to_tr.
       This is done due to the reason that move_snapshot_to_tr function is called at two places.
       1st in case of click_execute_page - where due to callback it has some dependency.
       PLEASE CORRECT THIS */     
    move_snapshot_to_tr_v2(vptr, wait_time, start_time, renamed_har_file_with_abs_path, rbu_resp_attr->prev_har_file_name, page_status, performance_trace_start_time);
    return 0;
  }
  vptr->httpData->rbu_resp_attr->retry_count = 0;
  ret = ns_rbu_web_url_end(vptr, url, renamed_har_file_with_abs_path, rbu_resp_attr->har_file_prev_size, prof_name, rbu_resp_attr->prev_har_file_name, page_status, performance_trace_start_time);
  return ret;
}

/*--------------------------------------------------------------------------------------------- 
 * Purpose   : This function will make headers from insert segment and malloc memory for that and 
 *             set adderes into provide request header buf 
 *             Eg: 
 *
 * Input     : vptr              - provide point to current virtual user 
 *             req_headers_buf   - provide pointer to point malloced memory for headers 
 *             req_headers_size  - provide variable to store request header size 
 *
 * Output    : On success -  0
 *             On Failure - -1  
 *--------------------------------------------------------------------------------------------*/
inline int ns_rbu_make_req_headers(VUser *vptr, char **req_headers_buf, int *req_headers_size)
{
  int i = 0;
  int ret;
  char *req_headers_buf_ptr;

  action_request_Shr* request = vptr->last_cptr->url_num;

  NSDL1_RBU(vptr, NULL, "Method Called, vptr = %p", vptr); 

  NS_RESET_IOVEC(g_scratch_io_vector);
  
  ret = insert_segments(vptr, vptr->last_cptr, &request->proto.http.hdrs, &g_scratch_io_vector, req_headers_size, 0, 1, REQ_PART_HEADERS, request, SEG_IS_NOT_REPEAT_BLOCK);

  if(ret == MR_USE_ONCE_ABORT)
    return ret;

  NSDL2_RBU(vptr, NULL, "num_vectors = %d, req_headers_size = %d", g_scratch_io_vector.cur_idx, *req_headers_size); 

  MY_MALLOC(*req_headers_buf, (*req_headers_size) + 1, "RBU Request Headers", -1);
  req_headers_buf_ptr = *req_headers_buf;
 
  // Copy request body into buffer
  for(i = 0; i < g_scratch_io_vector.cur_idx; i++)
  {
    memcpy(req_headers_buf_ptr, g_scratch_io_vector.vector[i].iov_base, g_scratch_io_vector.vector[i].iov_len);
    req_headers_buf_ptr += g_scratch_io_vector.vector[i].iov_len;
  }
  NS_FREE_RESET_IOVEC(g_scratch_io_vector);
  
  *req_headers_buf_ptr = '\0';
  return 0;
}

inline void ns_rbu_make_req_cookies(VUser *vptr, char **req_cookies_buf, int *req_cookies_size)
{
  int i = 0;
  int num_vectors = 0;
  char *req_cookies_buf_ptr;

  action_request_Shr* request = vptr->last_cptr->url_num;

  NSDL1_RBU(vptr, NULL, "Method Called, vptr = %p", vptr); 

  NS_RESET_IOVEC(g_scratch_io_vector);
  
  make_cookie_segments(vptr->last_cptr, &(request->proto.http), &g_scratch_io_vector);
  num_vectors = g_scratch_io_vector.cur_idx;

  if(num_vectors == 0)
  {
    NSDL2_RBU(vptr, NULL, "num_vectors = 0, hence returning...");
    return;
  }

  for(i = 0; i < num_vectors; i++)
    *req_cookies_size += g_scratch_io_vector.vector[i].iov_len;

  NSDL2_RBU(vptr, NULL, "num_vectors = %d, req_cookies_size = %d", num_vectors, *req_cookies_size); 

  MY_MALLOC(*req_cookies_buf, (*req_cookies_size) + 1, "RBU Request Cookies", -1);
  req_cookies_buf_ptr = *req_cookies_buf;
 
  // Copy request body into buffer
  for(i = 0; i < num_vectors; i++)
  {
    memcpy(req_cookies_buf_ptr, g_scratch_io_vector.vector[i].iov_base, g_scratch_io_vector.vector[i].iov_len);
    req_cookies_buf_ptr += g_scratch_io_vector.vector[i].iov_len;
  }
  NS_FREE_RESET_IOVEC(g_scratch_io_vector);
  
  *req_cookies_buf_ptr = '\0';
}

/*--------------------------------------------------------------------------------------------- 
 * Purpose   : This function will make body from insert segment and malloc memory for that and 
 *             set adderes into provide request body pointer
 *             Eg: 
 *
 * Input     : vptr              - provide point to current virtual user 
 *             req_body_buf_ptr  - provide pointer to point malloced request body 
 *             req_body_size     - provide variable to store request body size 
 *
 * Output    : On success -  0
 *             On Failure - -1  
 *--------------------------------------------------------------------------------------------*/
inline static int ns_rbu_make_req_body(VUser *vptr, char **req_body_buf, int *req_body_size)
{
  int i = 0;
  int ret;
  char *req_body_buf_ptr;

  action_request_Shr* request = vptr->last_cptr->url_num;

  NSDL1_RBU(vptr, NULL, "Method Called, vptr = %p", vptr); 
 
  NS_RESET_IOVEC(g_scratch_io_vector);
  
  ret = insert_segments(vptr, vptr->last_cptr, request->proto.http.post_ptr, &g_scratch_io_vector, req_body_size, 
                                request->proto.http.body_encoding_flag, 1, REQ_PART_BODY, request, SEG_IS_NOT_REPEAT_BLOCK);

  //TODO: IOVEC_CHANGE - whether to return or not
  if(ret == MR_USE_ONCE_ABORT)
    return ret;

  NSDL2_RBU(vptr, NULL, "num_vectors = %d, *req_body_size = %d", g_scratch_io_vector.cur_idx, *req_body_size); 

  MY_MALLOC(*req_body_buf, ((*req_body_size) + 1), "RBU Request Body Buf", -1);
  req_body_buf_ptr = *req_body_buf;
 
  // Copy request body into buffer
  for(i = 0; i < g_scratch_io_vector.cur_idx; i++)
  {
    memcpy(req_body_buf_ptr, g_scratch_io_vector.vector[i].iov_base, g_scratch_io_vector.vector[i].iov_len);
    req_body_buf_ptr += g_scratch_io_vector.vector[i].iov_len;
  }
  NS_FREE_RESET_IOVEC(g_scratch_io_vector);
 
  *req_body_buf_ptr = '\0';
  return 0;
}

/*--------------------------------------------------------------------------------------------- 
 * Purpose   : This function will make fully quallyfied url and store that into provided buffer 
 *             Eg: http://www.google.com/index.html?name=Manish&emp_id=M101
 *
 * Input     : vptr    - provide point to current virtual user 
 *             url_buf - provide buffer to fill fully qualified url 
 *             len     - provide MAX url buffer size
 *
 * Output    : On success -  0
 *             On Failure - -1  
 *--------------------------------------------------------------------------------------------*/
//inline static void ns_rbu_make_full_qal_url(VUser *vptr, char *url_buf, int len)
inline void ns_rbu_make_full_qal_url(VUser *vptr)
{
  char *url_buf_ptr;
  connection *cptr = vptr->last_cptr; 
  
  int write_idx = 0;
  int free_url_buf_len = RBU_MAX_URL_LENGTH;
 
  NSDL1_RBU(vptr, cptr, "Method Called, vptr = %p, vptr->httpData->rbu_resp_attr->url = %p, free_url_buf_len = %d", 
                         vptr, vptr->httpData->rbu_resp_attr->url, free_url_buf_len);
  if(vptr->httpData->rbu_resp_attr->url == NULL)
    MY_MALLOC(vptr->httpData->rbu_resp_attr->url, RBU_MAX_URL_LENGTH + 1,  "vptr->httpData->rbu_resp_attr", 1);
  
  url_buf_ptr = vptr->httpData->rbu_resp_attr->url;

  // Step1: Fill schema part of URL (http:// or https://) 
  NSDL3_RBU(vptr, cptr, "Fill schema part of URL - %s, free_url_buf_len = %d", 
                         (cptr->url_num->request_type == RBU_HTTP_REQUEST)?RBU_HTTP_REQUEST_STR:RBU_HTTPS_REQUEST_STR, free_url_buf_len);
  if(cptr->url_num->request_type == RBU_HTTP_REQUEST)
  {
    write_idx = snprintf(url_buf_ptr, free_url_buf_len, RBU_HTTP_REQUEST_STR); 
    RBU_SET_WRITE_PTR(url_buf_ptr, write_idx, free_url_buf_len);
  }
  else if(cptr->url_num->request_type == RBU_HTTPS_REQUEST)
  {
    write_idx = snprintf(url_buf_ptr, free_url_buf_len, RBU_HTTPS_REQUEST_STR); 
    RBU_SET_WRITE_PTR(url_buf_ptr, write_idx, free_url_buf_len);
  }
  
  // Step.2: Fill host part of URL Eg: 192.168.1.66:8080. Host is actual host (Origin Server)
  //write_idx = snprintf(url_buf_ptr, svr_entry->server_name_len, svr_entry->server_name); 
  NSDL3_RBU(vptr, cptr, "Fill host part of URL - %s, free_url_buf_len = %d", cptr->old_svr_entry->server_name, free_url_buf_len);
  write_idx = snprintf(url_buf_ptr, free_url_buf_len, "%s", cptr->old_svr_entry->server_name); 
  RBU_SET_WRITE_PTR(url_buf_ptr, write_idx, free_url_buf_len);

  // Step.3: Fill url path of URL e.g. /test/index.html?a=b&c=d
  NSDL3_RBU(vptr, cptr, "Fill url path of URL - %s, free_url_buf_len = %d", cptr->url, free_url_buf_len);
  write_idx = snprintf(url_buf_ptr, free_url_buf_len, "%s", cptr->url); 
  RBU_SET_WRITE_PTR(url_buf_ptr, write_idx, free_url_buf_len);

  NSDL3_RBU(vptr, cptr, "write_idx = %d, free_url_buf_len = %d", write_idx, free_url_buf_len);
  *url_buf_ptr = '\0';

  NSDL4_RBU(vptr, cptr, "ns_rbu_make_full_qal_url() End: vptr->httpData->rbu_resp_attr->url = [%s]", vptr->httpData->rbu_resp_attr->url);
}


/*--------------------------------------------------------------------------------------------- 
 * Purpose   : This function get parameter value by fun ns_eval_string()  
 *             (1) If user not provide - profile name, har log dir, and vnc display then NS will
 *                 use their own by function /etc/rbu_param.cvs 
 *
 * Input     : vptr          - provide point to current virtual user
 *
 * Output    : set value of following member of structure of RBU_RespAttr 
 *             profile       - provide buffer to fill profile name 
 *             har_log_dir   - provide buffer to fill har log directory name
 *             vnc_display   - provide buffer to fill VNC display id
 *
 *--------------------------------------------------------------------------------------------*/
static int ns_rbu_get_param(VUser *vptr)
{
  char buf[512]; 
  RBU_Param_Shr *rbu_param = &vptr->first_page_url->proto.http.rbu_param;  
  RBU_RespAttr *rbu_resp_attr = vptr->httpData->rbu_resp_attr;

  buf[0] = 0;

  NSDL1_RBU(vptr, NULL, "Method Called, vptr = %p, rbu_param = %p, rbu_resp_attr = %p", 
                                        vptr, rbu_param, rbu_resp_attr);

  //(a) Profile Name 
  NSDL2_RBU(vptr, NULL, "browser_user_profile = %s, enable_auto_param = %d, rbu_enable_auto_param = %d", 
                         rbu_param->browser_user_profile?rbu_param->browser_user_profile:NULL, 
                         runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_auto_param, 
                         global_settings->rbu_enable_auto_param); 
  
  //Here we are checking whether RBU_ENABLE_AUTO_PARAM (Automation of profiles and vncs) is off or on. 
  if(global_settings->rbu_enable_auto_param)  
    strcpy(rbu_resp_attr->profile, ns_eval_string("{cav_browser_user_profile}"));
  else if(rbu_param->browser_user_profile != NULL)
    strcpy(rbu_resp_attr->profile, ns_eval_string(rbu_param->browser_user_profile));
  else
    return -1;

  //(b) Log Dir  
  NSDL2_RBU(vptr, NULL, "har_log_dir = %s", rbu_param->har_log_dir?rbu_param->har_log_dir:NULL);  
  if(global_settings->rbu_enable_auto_param)
    strcpy(rbu_resp_attr->har_log_dir, ns_eval_string("{cav_har_log_dir}"));
  else if(rbu_param->har_log_dir != NULL)
    strcpy(rbu_resp_attr->har_log_dir, ns_eval_string(rbu_param->har_log_dir));
  else 
    return -1;
   
  //(c) Vnc Display Id 
  NSDL2_RBU(vptr, NULL, "har_log_dir = %s", rbu_param->vnc_display_id?rbu_param->vnc_display_id:NULL);  
  if(global_settings->rbu_enable_auto_param)
  {
    strcpy(buf, ns_eval_string("{cav_vnc_display_id}"));
    rbu_resp_attr->vnc_display = atoi(buf);
  }
  else if(rbu_param->vnc_display_id != NULL)
  {
    strcpy(buf, ns_eval_string(rbu_param->vnc_display_id));
    rbu_resp_attr->vnc_display = atoi(buf);
  }
  else
    return -1;
  
  return 0;
}

/* This function will initialise RBU_RespAttr */
static int ns_rbu_init(VUser *vptr)
{
  char ffx_part[] = ".rbu/.mozilla/firefox";
  char chrome_part[] = ".rbu/.chrome";
  char b_path[512 + 1] = "";

  RBU_RespAttr *rbu_resp_attr = NULL;

  NSDL1_RBU(vptr, NULL, "Method called, Before Init RBU_RespAttr: vptr = %p, rbu_resp_attr = %p", vptr, vptr->httpData->rbu_resp_attr);

  // Allocate memory to RBU_RespAttr only if it is not malloced before but reset it on every start of the session
  if(vptr->httpData->rbu_resp_attr == NULL)
  {
    /* Create RBU_RespAttr becuase we need firefox command buf*/
    CREATE_RBU_RESP_ATTR

    /* Allocating members of RBU_RespAttr*/
    MALLOC_RBU_RESP_ATTR_MEMBERS
  }

  rbu_resp_attr = vptr->httpData->rbu_resp_attr;

  //Create RBU Light house Structure
  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.lighthouse_mode)
    CREATE_RBU_LIGHTHOUSE_STRUCT

  NSDL2_RBU(vptr, NULL, "After Init RBU_RespAttr: rbu_resp_attr =  %p", rbu_resp_attr);
  if(rbu_resp_attr == NULL)
  {
    NSTL1_OUT(vptr, NULL, "Error: ns_rbu_init() - Allocation of memory to RBU_RespAttr failed.\n");
    return -1;
  }

  if(rbu_resp_attr->timer_ptr == NULL)
  {
    NSDL2_API(vptr, NULL, "Allocating rbu_resp_attr->timer_ptr = %p", rbu_resp_attr->timer_ptr);
    MY_MALLOC(rbu_resp_attr->timer_ptr, sizeof(timer_type),  "rbu_resp_attr->timer_ptr", -1);
    NSDL2_API(vptr, NULL, "After allocation of timer_ptr = %p", rbu_resp_attr->timer_ptr);
    rbu_resp_attr->timer_ptr->timer_type = -1;
  }

  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.browser_mode == RBU_BM_FIREFOX)
  {
    snprintf(rbu_resp_attr->har_log_path, RBU_MAX_PATH_LENGTH, "%s/%s/logs", g_home_env, ffx_part);
    snprintf(b_path, 512, "%s/%s", g_home_env, ffx_part);
  }
  else if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.browser_mode == RBU_BM_CHROME)
  {
    snprintf(rbu_resp_attr->har_log_path, RBU_MAX_PATH_LENGTH, "%s/%s/logs", g_home_env, chrome_part);
    snprintf(b_path, 512, "%s/%s", g_home_env, chrome_part);
  }

  /* Parametrise args*/
  if(ns_rbu_get_param(vptr) == -1)
  {
    NSTL1_OUT(vptr, NULL, "Error: ns_rbu_get_param(): fetching of parameter failed. Check If Auto Paramter is off and in script there is no "
                    "custom parameter then On Auto Parameter, by keyword G_RBU. \n"); 

    strncpy(rbu_resp_attr->access_log_msg, "Error: Fetching of parameter failed", RBU_MAX_ACC_LOG_LENGTH);
    strncpy(script_execution_fail_msg, "Internal Error:Failed to fetch file parameters i.e. BrowserUserProfile, HarLogDir and VncDisplayId.", MAX_SCRIPT_EXECUTION_LOG_LENGTH);

    return -1;
  }

  NSDL2_RBU(vptr, NULL, "After parametrization: prof_name = [%s], har_log_dir = [%s], vnc_display = [%d], url = [%s]", 
                         rbu_resp_attr->profile, rbu_resp_attr->har_log_dir, rbu_resp_attr->vnc_display, vptr->last_cptr->url);

  // Validating Profile name.
  if(ns_rbu_validate_profile(vptr, b_path, NULL) == -1)
    return -1;

  // Copy profile files into logs/TRxxx/rbu_logs/profiles/<profile_name>
   if(global_settings->rbu_enable_auto_param)
   {
     if(ns_rbu_copy_profiles_to_tr(vptr, b_path, rbu_resp_attr->profile) == -1)
       NSDL2_RBU(vptr, NULL, "Unable to copy profiles to TRxxx");
   }

   // Below are new members of rbu_resp_attr used to control callback
   rbu_resp_attr->har_file_prev_size = 0;
   rbu_resp_attr->retry_count = 0;
   rbu_resp_attr->prev_har_file_name[0] = 0;

  return 0;
}

static int ns_rbu_make_firefox_command(VUser *vptr, int page_id, int dummy_flag)
{
  char file_name[RBU_MAX_FILE_LENGTH];                /* To reduce stack overhead taking only one buf to store file name tmp bacis , 
                                                         resuse this buffere */ 
  char wan_args[RBU_MAX_WAN_ARGS_LENGTH + 1];         /* contain wan setting Eg: for 3G_UMTS3G - 100000:200000:0:0:0:0:0 */

  char wan_access[RBU_MAX_WAN_ACCESS_LENGTH + 1];
  
  char user_agent_str[RBU_USER_AGENT_LENGTH + 1];

  char *cmd_buf_ptr = NULL;                            /* This pointer is responsilbe to fill firefox command buffer */

  int cmd_write_idx = 0;
  int free_cmd_buf_len = RBU_MAX_CMD_LENGTH;
  int timeout_for_next_req = 0;   //setting default valur of variable 'timeout_for_next_req' and 'G_RBU_PAGE_LOADED_TIMEOUT'
  
  connection *cptr = vptr->last_cptr;
  RBU_RespAttr *rbu_resp_attr = vptr->httpData->rbu_resp_attr;

  NSDL1_RBU(vptr, NULL, "Method Called: vptr = %p, page_id = %d, rbu_resp_attr = %p, enable_ns_firefox = %d, "
                        "g_ns_firefox_binpath = [%s], dummy_flag = %d", 
                         vptr, page_id, rbu_resp_attr, global_settings->enable_ns_firefox, 
                         g_ns_firefox_binpath[0]?g_ns_firefox_binpath:NULL, dummy_flag);
  
  /* Set path to store request response files */
  RBU_NS_LOGS_PATH

  /* Allocate memory(10 K) to store firefox whole command */
  /* /path/firefox <url> -P <profile_name> --display=:<display_id> --cav_wan_setting <fw_badwidth:rv_band_width:compression:fw_late:rv_late:fw_loss:rv_loss> --cav_hdr <header_name: header_value> --cav_hdr <header_name: header_value> --cav_post_hdr <header_name: header_value> --cav_post_body <req_body_file> --cav_enable_ss --cav_ss_file <screen_shot_file> */

  cmd_buf_ptr = rbu_resp_attr->firefox_cmd_buf;

  /* Move all files from har log dir before executing new Page so that if due to some reason 
     har file left then not distrub Renaming of others. 
     Here using same buffer as we are using in firefox command i.e. rbu_resp_attr->firefox_cmd_buf so that stack size not increases*/
  if(global_settings->rbu_har_rename_info_file)
  {
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "mv %s/%s/*.har %s/%s/.TR%d >/dev/null 2>&1", 
                              rbu_resp_attr->har_log_path, rbu_resp_attr->har_log_dir, 
                              rbu_resp_attr->har_log_path, rbu_resp_attr->har_log_dir, testidx);

    NSDL2_RBU(vptr, NULL, "Running command to move old Har file in .TRxx: cmd_buf = [%s]", rbu_resp_attr->firefox_cmd_buf);
  }
  else
  {
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "mv %s/%s/*.har %s/logs/%s/harp_files/ >/dev/null 2>&1",
                              rbu_resp_attr->har_log_path, rbu_resp_attr->har_log_dir,
                              g_ns_wdir, rbu_logs_file_path);

    NSDL2_RBU(vptr, NULL, "Running command to move old Har file in rbu_logs dir: cmd_buf = [%s]", rbu_resp_attr->firefox_cmd_buf);
  }

  nslb_system2(rbu_resp_attr->firefox_cmd_buf);

  // clear buffer so that there is no junck data in memory
  memset(rbu_resp_attr->firefox_cmd_buf, 0, 2*cmd_write_idx + 1);  
  cmd_write_idx = 0;

  /* Firfox Args(1): fully qualified url */
  // Make fully qualified url - http://127.0.0.1:81/url_service/url_test.xml?RespSameAsReq=Y
  // and store it into vptr->httpData->rbu_resp_attr->url
  if(!dummy_flag) ns_rbu_make_full_qal_url(vptr);

  /* Adding controlle name just identify that form which controller firefox command envoked so that we can clean firefox command before
     start of the test */
  char controller_name[1024  + 1] = "";
  char *controller_name_ptr = NULL;
  
  if((controller_name_ptr = strrchr(g_ns_wdir, '/')) != NULL)
    strcpy(controller_name, controller_name_ptr + 1);


  NSDL2_RBU(vptr, NULL, "Firefox Args (1): url = [%s], prof_name = [%s], vnc_display = %d, controller_name = [%s]", 
                          rbu_resp_attr->url, rbu_resp_attr->profile, rbu_resp_attr->vnc_display, controller_name);

  /* Making firefox command , Note add one space on every option adding*/
  if(global_settings->enable_ns_firefox) /* NS Firefox */
  {
    #ifndef CAV_MAIN
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "nohup %s/firefox \"about:blank\" --cav_url \"%s\" "
    //                                  "-height 720 -width 1020 -P %s --display=:%d "
                                        "-height 720 -width 1020 -profile %s/.rbu/.mozilla/firefox/profiles/%s --display=:%d "
                                        "--cav_testrun %d --controller_name %s --cav_partition %lld --cav_close_prev_tab 0 "
                                        "--cav_debug_level %d ", 
    //                                  g_ns_firefox_binpath, dummy_flag?g_rbu_dummy_url:rbu_resp_attr->url, rbu_resp_attr->profile, 
                                        g_ns_firefox_binpath, dummy_flag?g_rbu_dummy_url:rbu_resp_attr->url,
                                        g_home_env, rbu_resp_attr->profile, 
                                        rbu_resp_attr->vnc_display, testidx, controller_name, vptr->partition_idx, get_debug_level(vptr));  
    #else
       cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "nohup %s/firefox \"about:blank\" --cav_url \"%s\" "
                                        "-height 720 -width 1020 -profile %s/.rbu/.mozilla/firefox/profiles/%s --display=:%d "
                                        "--cav_testrun %d --controller_name %s --cav_partition %s --cav_close_prev_tab 0 "
                                        "--cav_debug_level %d ",
                                        g_ns_firefox_binpath, dummy_flag?g_rbu_dummy_url:rbu_resp_attr->url,
                                        g_home_env, rbu_resp_attr->profile, rbu_resp_attr->vnc_display,
                                        testidx, controller_name, vptr->sess_ptr->sess_name, get_debug_level(vptr));
    #endif
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  }
  else /* System firefox */
  {
    //cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "nohup firefox \"about:blank\" --cav_url \"%s\" -P %s --display=:%d "
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "nohup firefox \"about:blank\" --cav_url \"%s\" "
                                          "-profile %s/.rbu/.mozilla/firefox/profiles/%s --display=:%d "
                                          "--cav_testrun %d --controller_name %s --cav_partition %lld --cav_close_prev_tab 0 "
                                          "--cav_debug_level %d ", 
                                          dummy_flag?g_rbu_dummy_url:rbu_resp_attr->url, g_home_env, rbu_resp_attr->profile, 
                                          rbu_resp_attr->vnc_display, testidx, controller_name, vptr->partition_idx, get_debug_level(vptr));  
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  }
 
  /* Firfox Args(2): --cav_wan_setting <wan_args> */
  // WAN option is works only NS firefox not with system
  NSDL2_RBU(vptr, NULL, "wan_env = %d, enable_ns_firefox = %d", global_settings->wan_env, global_settings->enable_ns_firefox);
  if(global_settings->wan_env && global_settings->enable_ns_firefox) //If WAN_ENV is ON and nestorm firefox used
  {
    get_wan_args_for_browser(vptr, wan_args, wan_access);
    NSDL2_RBU(vptr, NULL, "Firefox Args (2): wan_args = %s", wan_args);
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "--cav_wan_setting %s ", wan_args); 
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
    
    NSDL2_RBU(vptr, NULL, "Firefox Args (2): wan_access = %s", wan_access);
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "--cav_wan_access \"%s\" ", wan_access);
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  }

  // In case of dummy url we no need of rest option so return from here
  if(dummy_flag)
    goto at_last;

  /* Firfox Args: --cav_resolution <screen_width> x <screen_height>" */
  NSDL2_RBU(vptr, NULL, "rbu_screen_sim_mode = [%d]", runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.screen_size_sim);
  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.screen_size_sim == 0)
  {
    NSDL2_RBU(vptr, NULL, "Setting firefox default screen resolution");
  }
  else if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.screen_size_sim == 1)
  {
    NSDL2_RBU(vptr, NULL, "Setting firefox screen resolution from user");
    // cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "--cav_resolution %dx%d ", global_settings->rbu_screen_width, global_settings->rbu_screen_height);
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "--cav_resolution %dx%d ", vptr->screen_size->width, vptr->screen_size->height); 
    // TODO - replace with screen prt
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  }

  /* Firfox Args(3): --cav_enabless --cav_ssfile <file_name>*/
  NSDL2_RBU(vptr, NULL, "trace_on_fail = %d, trace_level = %d", runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail, 
                         runprof_table_shr_mem[vptr->group_num].gset.trace_level);
  //if((NS_IF_PAGE_DUMP_ENABLE && (runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_ALL_SESS) &&
    //        (runprof_table_shr_mem[vptr->group_num].gset.trace_level > TRACE_CREATE_PG_DUMP)))
  //enable_screen_shot
  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_screen_shot && 
        NS_IF_PAGE_DUMP_ENABLE && runprof_table_shr_mem[vptr->group_num].gset.trace_level > TRACE_ONLY_REQ_RESP)
  {
    if(rbu_resp_attr->page_screen_shot_file == NULL)
      MY_MALLOC(rbu_resp_attr->page_screen_shot_file, RBU_MAX_FILE_LENGTH + 1,  "vptr->httpData->rbu_resp_attr->page_screen_shot_file", 1);
    
    //changes file nameing convention%hd_%u_%u_%d_0_%d_%d_%d_0.dat 
    snprintf(rbu_resp_attr->page_screen_shot_file, RBU_MAX_FILE_LENGTH - 1, 
                    "%s/logs/%s/screen_shot/page_screen_shot_%hd_%u_%u_%d_0_%d_%d_%d_0", 
                    g_ns_wdir, rbu_logs_file_path,  child_idx, vptr->user_index, vptr->sess_inst, 
                    vptr->page_instance, vptr->group_num, GET_SESS_ID_BY_NAME(vptr), GET_PAGE_ID_BY_NAME(vptr));

    NSDL2_RBU(vptr, NULL, "Firefox Args (3): screen_shot_file_name = %s", rbu_resp_attr->page_screen_shot_file);
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "--cav_enable_ss --cav_ssfile %s ", rbu_resp_attr->page_screen_shot_file); 
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  }

  //enable_capture_clip
  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_capture_clip)
  {
    if(rbu_resp_attr->page_capture_clip_file == NULL)
      MY_MALLOC(rbu_resp_attr->page_capture_clip_file, RBU_MAX_FILE_LENGTH + 1, 
                                   "vptr->httpData->rbu_resp_attr->page_capture_clip_file", 1);

    snprintf(rbu_resp_attr->page_capture_clip_file, RBU_MAX_FILE_LENGTH - 1,
                    "%s/%s/clips/video_clip_%hd_%u_%u_%d_0_%d_%d_%d_0_", 
                    rbu_resp_attr->har_log_path, rbu_resp_attr->profile, child_idx, vptr->user_index, vptr->sess_inst,
                    vptr->page_instance, vptr->group_num, GET_SESS_ID_BY_NAME(vptr), GET_PAGE_ID_BY_NAME(vptr));
    
    NSDL2_RBU(vptr, NULL, "Firefox Args (): capture_clip_file_name = %s", rbu_resp_attr->page_capture_clip_file);
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, 
                                 "--cav_enable_cc --cav_ccfile %s --cav_clips_freq %d --cav_clips_qual %d ", 
                                 rbu_resp_attr->page_capture_clip_file, 
                                 runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.clip_frequency, 
                                 runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.clip_quality);
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  }

  //enable_cav_render-- if enable then its value should be >=100
  //if rbu_setting and capture clips both are inable the capture_clip may take prefrence
  if ((runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_settings >=100) && !(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_capture_clip))
  {
    NSDL2_RBU(vptr, NULL, "Firefox Args (): cav_render_freq = %d", runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_settings);
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "--cav_render_freq %d ", runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_settings);
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  }
  else 
  {
    NSDL2_RBU(vptr, NULL, "Firefox Args (): cav_render_freq = %d", runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_settings);
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "--cav_render_freq 0 ");
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  }
    //--cav_tti - if it is enable then _tti field appear in HAR file 
  NSDL2_RBU(vptr, NULL, "Firefox Args (): cav_tti = %d", runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.tti_mode);
  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "--cav_tti %d ", runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.tti_mode);
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);


  /* Firfox Args(4): --cav_header "User-Agent: <string>" */
  //enable_ns_firefox 0 -> don't set firefox set its default
  //enable_ns_firefox 1 -> take from User profile 
  //enable_ns_firefox 2 -> take from user provided string
  int disable_headers = runprof_table_shr_mem[vptr->group_num].gset.disable_headers;
  NSDL2_RBU(vptr, NULL, "global_settings->rbu_user_agent = [%d]", runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.user_agent_mode);
  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.user_agent_mode == 0)
  {
    NSDL2_RBU(vptr, NULL, "Setting firefox default user agent");
  }
  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.user_agent_mode == 1)
  {
    NSDL2_RBU(vptr, NULL, "Setting user agent from user profile");
    if (!(disable_headers & NS_UA_HEADER)) 
    {
      if (!(vptr->httpData && (vptr->httpData->ua_handler_ptr != NULL)
          && (vptr->httpData->ua_handler_ptr->ua_string != NULL)))
      {
        NSDL2_RBU(vptr, NULL, "Firefox Args (4): User-Agent = %s", vptr->browser->UA);
        strcpy(user_agent_str, vptr->browser->UA);
        ADD_USER_AGENT_IN_FIREFOX_CMD
      }  
      else 
      {
        NSDL2_RBU(vptr, NULL, "Firefox Args (4): User-Agent = %s", vptr->httpData->ua_handler_ptr->ua_string);
        strcpy(user_agent_str, vptr->httpData->ua_handler_ptr->ua_string);
        ADD_USER_AGENT_IN_FIREFOX_CMD
      }
    }
  }
  else if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.user_agent_mode == 2)
  {
    NSDL2_RBU(vptr, NULL, "Setting user agent provide by user");
    NSDL2_RBU(vptr, NULL, "Firefox Args (4): User-Agent = %s", 
                           runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.user_agent_name);
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "--cav_hdr \"User-Agent: %s\" ", 
                                                            runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.user_agent_name); 
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  }

  //Bug Id-30543 
  //G_RBU_PAGE_LOADED_TIMEOUT > 500ms, So WaitForNextReq > 500ms
  //By Default, G_RBU_PAGE_LOADED_TIMEOUT = 5000ms
  timeout_for_next_req  = ((vptr->first_page_url->proto.http.rbu_param.timeout_for_next_req > 500)?
                              vptr->first_page_url->proto.http.rbu_param.timeout_for_next_req :
                              runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.page_loaded_timeout);
  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "--cav_pageLoadedTimeout %d ", timeout_for_next_req);
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  NSDL2_RBU(vptr, NULL, "Setting pageLoadedTimeout = %d ", timeout_for_next_req);
 
  unsigned short har_timeout;
  int timeout; 
  HAR_TIMEOUT 
  //Setting  page_load_wait_time 5 second less because we are taking this time diff to be in sync of extension and netstorm 
  timeout = (1000 * har_timeout) - 5000;
  NSDL2_RBU(vptr, NULL, "Setting page load wait time out provide by user");
  NSDL2_RBU(vptr, NULL, "Firefox Args (): cav_pageLoadWaitTime %d", timeout);
  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "--cav_pageLoadWaitTime %d ", timeout);
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);

  char *req_body_buf = NULL;
  char *req_headers_buf = NULL;
  char *req_cookies_buf = NULL;
  int req_body_len = 0;
  int req_headers_size = 0;
  int req_cookies_size = 0;
  FILE *req_fp = NULL;
  char *headers_fields[128];
  int num_headers = 0;
  int i = 0;

  // Add cookies
  NSDL2_RBU(vptr, NULL, "Make Req Cookie form cookie table");
  ns_rbu_make_req_cookies(vptr, &req_cookies_buf, &req_cookies_size);
  NSDL2_RBU(vptr, NULL, "req_cookies_buf = [%s], req_cookies_size = %d", req_cookies_buf, req_cookies_size);
  if(req_cookies_buf != NULL)
  {
    //trim \r\n from end
    CLEAR_WHITE_SLASH_R_SLASH_N_END(req_cookies_buf);
    NSDL2_RBU(vptr, NULL, "After clear , req_cookies_buf = [%s]", req_cookies_buf);
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "--cav_hdr \"%s\" ", req_cookies_buf); 
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  }

  /* Insert Network Cache Stats header - Begin - */
  if(IS_NETWORK_CACHE_STATS_ENABLED_FOR_GRP(vptr->group_num))
  {
    NSDL3_RBU(vptr, cptr, "Network Cache Stats Headers = [%s], len =[%d]", 
                           network_cache_stats_header_buf_ptr, network_cache_stats_header_buf_len);
    network_cache_stats_header_buf_ptr[network_cache_stats_header_buf_len - 2] = '\0';
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "--cav_hdr \"%s\" ", network_cache_stats_header_buf_ptr); 
    //cmd_write_idx -= 3;
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  }  /* Network Cache Stats header - End - */

  // ADD CavTxName or customized header, if G_SEND_NS_TX_HTTP_HEADER is on
  if(runprof_table_shr_mem[vptr->group_num].gset.ns_tx_http_header_s.mode)
  {
    NSDL3_RBU(vptr, cptr, "G_SEND_NS_TX_HTTP_HEADER is enabled. Going to prepare transaction header"); 
    TxInfo *node_ptr = (TxInfo *) vptr->tx_info_ptr;
    if(node_ptr == NULL) {
      NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_WARNING,
                  __FILE__, (char*)__FUNCTION__, "No transaction is running for this user, so netcache transaction header will not be send.");
    } else {
     char *tx_name = nslb_get_norm_table_data(&normRuntimeTXTable, node_ptr->hash_code);
    // Adding HTTP Header name is the name of HTTP header. Default value of http header name is "CavTxName".
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "--cav_hdr \"%s", runprof_table_shr_mem[vptr->group_num].gset.ns_tx_http_header_s.header_name); 
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);

  
    // Adding Transaction name, http req will have this header with the last tx started before this URL was send.
    if(runprof_table_shr_mem[vptr->group_num].gset.ns_tx_http_header_s.tx_variable[0] == '\0')
    {
      NSDL3_RBU(vptr, cptr, "tx var is null..., tx name = %s", tx_name); 
      // We are sending head node of link list, that is the last transaction started before ns_web_url  
      cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "%s\" ", tx_name); 
      RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
    }
    else{
       NSDL3_RBU(vptr, cptr, "tx var is not null..."); 
       cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "%s\" ", ns_eval_string(runprof_table_shr_mem[vptr->group_num].gset.ns_tx_http_header_s.tx_variable)); 
       RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
        }
      }
  } // End of CavTxName hdr

  /* Added Custom headers */
  NSDL2_RBU(vptr, NULL, "Make Req Headers from segment table");
  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_header_flag)
  {
    ns_rbu_make_req_headers(vptr, &req_headers_buf, &req_headers_size);
    NSDL2_RBU(vptr, NULL, "req_headers_buf = [%s], req_headers_size = %d", req_headers_buf, req_headers_size);
    //trim \r\n from end
    CLEAR_WHITE_SLASH_R_SLASH_N_END(req_headers_buf);
    NSDL2_RBU(vptr, NULL, "After clear, req_headers_buf = [%s]", req_headers_buf);
  } 
  num_headers = get_tokens_ex2(req_headers_buf, headers_fields, "\r\n", 128);
  NSDL2_RBU(vptr, NULL, "num_headers = %d", num_headers);

  //MM: In case of post body Why we are not adding header in all inline urls? 
  NSDL2_RBU(vptr, NULL, "cptr->url_num->proto.http.post_ptr = %p, http_method = %d", 
                         cptr->url_num->proto.http.post_ptr, cptr->url_num->proto.http.http_method);
  if(cptr->url_num->proto.http.post_ptr != NULL && cptr->url_num->proto.http.http_method == HTTP_METHOD_POST)  //If body exist  
  {
    snprintf(file_name, RBU_MAX_FILE_LENGTH - 1, "%s/logs/%s/ns_rbu_req_body_post_%hd_%u_%u_%d_%d_%d_%d.dat", g_ns_wdir, 
                    ns_logs_file_path, child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance, 
                    vptr->group_num, GET_SESS_ID_BY_NAME(vptr), GET_PAGE_ID_BY_NAME(vptr));

    /* Firfox Args(5): --cav_header <header_name: header_value> --cav_header <header_name: header_value> */
    /* cav_post_hdr set header only in main url not in embeded urls*/
    for(i = 0; i < num_headers; i++)
    {
      NSDL2_RBU(vptr, NULL, "Firefox Args (5): headers_fields[%d] = [%s]", i, headers_fields[i]);
      if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_header_flag == 1)
        cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "--cav_post_hdr \"%s\" ", headers_fields[i]); 
      else
        cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "--cav_hdr \"%s\" ", headers_fields[i]);
      RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
    }

    NSDL2_RBU(vptr, NULL, "Make Req Body file '%s'", file_name);
    ns_rbu_make_req_body(vptr, &req_body_buf, &req_body_len);

    NSDL2_RBU(vptr, NULL, "req_body_buf = %p, req_body_len = %d, req_body_buf = [%s]", req_body_buf, req_body_len, req_body_buf?req_body_buf:NULL);
    //Dump Body into body file 
    if(req_body_buf != NULL)
    {
      if((req_fp = fopen(file_name, "w+")) != NULL)
      {
        //SM, handling for POST request file delete, have to store file name so that it can later be removed
        memset(vptr->httpData->rbu_resp_attr->post_req_filename, 0, RBU_MAX_FILE_LENGTH + 1);
        snprintf(vptr->httpData->rbu_resp_attr->post_req_filename, RBU_MAX_FILE_LENGTH, "%s", file_name);

        if(fwrite(req_body_buf, RBU_FWRITE_EACH_ELEMENT_SIZE, req_body_len, req_fp) <= 0)
        {
          NSTL1_OUT(NULL, NULL, "Error: in opening url request body file '%s' failed.\n", file_name);
        }

        fclose(req_fp);
      }    


      /* Firfox Args(6): --cav_reqbody <req_body_file> */
      NSDL2_RBU(vptr, NULL, "Firefox Args (6): req_body_buf = %s", req_body_buf);
      cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "--cav_post_body %s ", file_name); 
      RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
    }
  }
  else if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_header_flag)
  {
    /* cav_hdr set header in main url as well as in embeded urls*/
    for(i = 0; i < num_headers; i++)
    {
      NSDL2_RBU(vptr, NULL, "Firefox Args (5): headers_fields[%d] = [%s]", i, headers_fields[i]);
      if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_header_flag == 1)
       cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "--cav_post_hdr \"%s\" ", headers_fields[i]);
      else if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_header_flag == 2)
       cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "--cav_hdr \"%s\" ", headers_fields[i]);
      RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
    }
  }    
  //NVSM and ND Integration Changes in header.
  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_nd_fpi_send_mode == 0) //we have to send header in main url only
  {
    if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_nd_fpi_mode == 1)
    { 
        cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "--cav_post_hdr \"CavNDFPInstance: f\" "); 
        RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
    }
    else if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_nd_fpi_mode == 2) 
    {
        cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "--cav_post_hdr \"CavNDFPInstance: F\" "); 
        RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
    }
    NSDL2_RBU(vptr, NULL,"rbu_gset.rbu_nd_fpi_mode = %d, Added cav_post_hdr CavNDFPInstance: sucessfully", 
                                                       runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_nd_fpi_mode);
  }
  else if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_nd_fpi_send_mode == 1) //We want to send hdr in main as well as inline url
  {
  
    if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_nd_fpi_mode == 1)
    { 
        cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "--cav_hdr \"CavNDFPInstance: f\" "); 
        RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
    }
    else if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_nd_fpi_mode == 2) 
    {
        cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "--cav_hdr \"CavNDFPInstance: F\" "); 
        RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
    }

    NSDL2_RBU(vptr, NULL,"rbu_gset.rbu_nd_fpi_mode = %d, Added cav_hdr CavNDFPInstance: sucessfully", 
                                                       runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_nd_fpi_mode);
  }
  //rbu_har_setting  
  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_har_setting_mode)
  {
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "--cav_har_setting %d --cav_har_compression  %d "
                             "--cav_har_request %d --cav_har_response %d ",
                             runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_har_setting_mode,
                             runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_har_setting_compression,
                             runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_har_setting_request,
                             runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_har_setting_response);
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  }
  else
  {
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "--cav_har_setting 0 ");
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  }
 
  /* Free memory malloced for body and header */

  FREE_AND_MAKE_NULL(req_body_buf, "req_body_buf", -1);
  FREE_AND_MAKE_NULL(req_headers_buf, "req_headers_buf", -1);
  FREE_AND_MAKE_NULL(req_cookies_buf, "req_cookies_buf", -1);

  at_last: 
  /* Firfox Args(7): fireofox out file */
  // In firefox out file name add profile name because for each profile one firefox instance created
  snprintf(file_name, RBU_MAX_FILE_LENGTH, "%s/logs/%s/ns_rbu_firefox_out_prof_%s.log", g_ns_wdir, rbu_logs_file_path, rbu_resp_attr->profile); 
  NSDL2_RBU(vptr, NULL, "Firefox Args (7): firefox_out_file = %s", file_name);
  #ifdef NS_DEBUG_ON
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "--cav_listener >> %s 2>&1 < /dev/null & echo -n $! ", file_name); 
  #else 
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "--cav_listener >/dev/null  2>&1 < /dev/null & echo -n $! "); 
  #endif
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);

  // Null terminate cmd buf 
  *cmd_buf_ptr = '\0';
  NSDL2_RBU(vptr, NULL, "Firefox MSG = %s", rbu_resp_attr->firefox_cmd_buf);

  return 0;
}

void handler_sigchild_ignore(int data)
{
  NSDL2_RBU(NULL, NULL, "Ignoring signal %d to open chrome browser", data);  
}

int wait_firefox_make_run_cmd_callback(ClientData client_data)
{
  VUser *vptr = (VUser *)client_data.p;
  NSDL2_RBU(vptr, NULL, "Method Called:: Timer Expired");

  vptr->httpData->rbu_resp_attr->sess_start_flag = 1;
  
  // This function never return other than 0
  if(ns_rbu_make_firefox_command(vptr, vptr->next_pg_id, NON_DUMMY_PAGE) != 0)
    return -1;
  
  if(ns_rbu_run_firefox(vptr, NON_DUMMY_PAGE) != 0)
    return -1;

  return 0;
}

int ns_rbu_run_firefox(VUser *vptr, int dummy_flag)
{
  char read_buf[1024 + 1];
  int is_ffx_running = 0;    /* This falg check is firefox process running or not if running then set it to 1 */
  typedef void (*sighandler_t)(int);  

  NSDL2_RBU(vptr, NULL, "Running command to start Browser: firefox_cmd_buf = [%s]", vptr->httpData->rbu_resp_attr->firefox_cmd_buf);
  
  #ifdef NS_DEBUG_ON
  printf("Hitting url '%s' via %s.\n", dummy_flag?g_rbu_dummy_url:vptr->httpData->rbu_resp_attr->url, 
                                       global_settings->enable_ns_firefox?"Netstorm Firefox":"System Firefox");
  #endif
 
  /* If firefox is not running then store their pid to kill on session end */
  if((vptr->httpData->rbu_resp_attr->browser_pid != 0) && (kill(vptr->httpData->rbu_resp_attr->browser_pid, 0) == 0)) //Firefox running
    is_ffx_running = 1;

  sighandler_t prev_handler;
  //prev_handler = signal(SIGCHLD, SIG_IGN);
  prev_handler = signal(SIGCHLD, handler_sigchild_ignore);

  FILE *fp = popen(vptr->httpData->rbu_resp_attr->firefox_cmd_buf, "r");
  if(fp == NULL)
  {
    NS_EL_2_ATTR(EID_FOR_API, vptr->user_index, vptr->sess_inst, EVENT_API, 4, 
                            vptr->sess_ptr->sess_name, vptr->cur_page->page_name, "Error in starting browser. Error = %s", nslb_strerror(errno));
    strncpy(vptr->httpData->rbu_resp_attr->access_log_msg, "Error in Starting Browser", RBU_MAX_ACC_LOG_LENGTH);
    snprintf(script_execution_fail_msg, MAX_SCRIPT_EXECUTION_LOG_LENGTH, "Internal Error:Failed to start browser. Error: %s", nslb_strerror(errno));
    HANDLE_RBU_PAGE_FAILURE(-1)
    return -1;
  }

  //Need to check if browser was started successfully or not.(It may through some error messages.)
  NSDL2_RBU(vptr, NULL, "Browser started succesfully, is_ffx_running = %d", is_ffx_running);

  // Read firefox pid if firefox is not running
  if(!is_ffx_running)
  {
    fgets(read_buf, 1024, fp);
    NSDL4_RBU(vptr, NULL, "Firefox proc id - ret_buf = [%s]", read_buf);

    vptr->httpData->rbu_resp_attr->browser_pid = atoi(read_buf);
  }
    

  //Manish:Tue Feb 19 01:52:17 EST 2013: commenting the below event handling because till now
  //        i am getting any proof-full solution to handle the error "No child processes"
  //        TODO - this has to be handle in feature 
  //        Related bug id #5269 
  #if 0  
  if(pclose(fp) < 0)
  {
    NS_EL_2_ATTR(EID_FOR_RBU, vptr->user_index, vptr->sess_inst, EVENT_RBU, 4, 
                            vptr->sess_ptr->sess_name, page_name, "Error in closing file pointer. Error = %s", nslb_strerror(errno));
  }
  (void) signal( SIGCHLD, prev_handler);
  #endif

  pclose(fp);
  (void) signal( SIGCHLD, prev_handler);

  if(dummy_flag)
    VUSER_SLEEP_FIREFOX_COMMAND_AND_RUN(vptr, 10000);
  
  return 0;
}

/*------------------------------------------------------------------------------------------------------------- 
 * Purpose   : This function will add a line of setting screen size when the browser is Mozilla Firefox 42.0
 *           : On start of each session this will set screen size and simulate the browser screen on VNC.
 *    
 * Input     : vptr and name of profile  
 *    
 * Output    : On error    -1
 *             On success   0
 *
 * Rslvd_Bug : Bug 15420 - In Walgreens , we upgraded Firefox latest version(42) , on executing Test for 
 *             Firefox Desktop on the VNC we see the Mobile screen.
 *    
 * Build_ver : 4.1.5#31 
 *------------------------------------------------------------------------------------------------------------*/
int ns_rbu_set_screen_size(VUser *vptr, char *profile)
{
  FILE *file_fp;
  char buf[512 + 1] = "";
  char file_name[128 + 1] = ""; 
  char line[1024 + 1] = "";
  int flag = 0;
  int len = 0;

  NSDL2_RBU(NULL, NULL, "Method called with profile = %s", profile);
 
  // Add line 'user_pref("devtools.responsiveUI.presets", "[{\"key\":\"100x200\",\"name\":\"custom\",\"width\":100,\"height\":200}]");'
  // in profile's ~/.rbu/.mozilla/firefox/profiles/ffx1/prefs.js
  sprintf(file_name, "%s/.rbu/.mozilla/firefox/profiles/%s/prefs.js", g_home_env, profile);
  len = snprintf(buf, 512, "user_pref(\"devtools.responsiveUI.presets\", \"[{\\\"key\\\":\\\"%dx%d\\\",\\\"name\\\":\\\"custom\\\",\\\"width\\\":%d,\\\"height\\\":%d}]\");\n",
                vptr->screen_size->width, vptr->screen_size->height, vptr->screen_size->width, vptr->screen_size->height);

  buf[len] = '\0';

  NSDL4_RBU(NULL, NULL, "file_name = %s, buf = %s with length = %d", file_name, buf, len);

  if((file_fp = fopen(file_name, "r+")) != NULL)
  {
    // Check "user_pref("devtools.responsiveUI.presets", "[{"key": 100X200, "name": "custom", "width": 100, "height": 200}];" 
    // same entry in prefs.js exists or not. 
    while(fgets(line, 1024, file_fp))
    {
      NSDL4_RBU(NULL, NULL, "line = %s, str = %s\n", line, buf);
      if(!strcmp(line, buf))
      {
        NSDL4_RBU(NULL, NULL, "Found str = '%s' in line '%s'", buf, line);
        flag = 1;   //screen size is already present in prefs.js
        break;
      }
      else
       NSDL4_RBU(NULL, NULL, "Not Found str = '%s' in line '%s'", buf, line);
    }

    if(flag == 0)
    {
      if(fwrite(buf, len, 1, file_fp) <= 0)
      {
        NSDL4_RBU(NULL, NULL, "Error: Unable to write in file '%s'.", file_name);
        return -1;
      }
      else
        NSDL4_RBU(NULL, NULL, "str = '%s' is write in file = '%s'.", buf, file_name);
    }
    else
      NSDL4_RBU(NULL, NULL, "str = '%s' is already present in file '%s'.", buf, file_name);

    fclose(file_fp);
  }
  else
  {
    NSDL4_RBU(NULL, NULL, "Error: in  opening file = %s", file_name);
    return -1;
  }
  return 0;
}

static int ns_rbu_execute_page_via_firefox( VUser *vptr, int page_id )
{
  NSDL1_RBU(vptr, NULL, "Method Called: rbu_resp_attr = %p, g_rbu_dummy_url = [%s], rbu_enable_dummy_page = %d, sess_start_flag = %d", 
                           vptr->httpData->rbu_resp_attr, *g_rbu_dummy_url?g_rbu_dummy_url:"NULL",  
                           global_settings->rbu_enable_dummy_page, 
                           vptr->httpData->rbu_resp_attr?vptr->httpData->rbu_resp_attr->sess_start_flag:0);

  /* If RBU_ENABLE_DUMMY_PAGE 1 and this is starting page of session then excute DUMMY PAGE to activate netexport */
  //if(global_settings->rbu_enable_dummy_page && vptr->httpData->rbu_resp_attr == NULL)
  if(global_settings->rbu_enable_dummy_page && vptr->httpData->rbu_resp_attr->sess_start_flag == 0)
  {
    NSDL2_RBU(vptr, NULL, "Making Firefox command for DUMMY PAGE");
    if(ns_rbu_make_firefox_command(vptr, page_id, DUMMY_PAGE) != 0)
    {
      HANDLE_RBU_PAGE_FAILURE(-1)
      return -1;
    }

    if(ns_rbu_run_firefox(vptr, DUMMY_PAGE) != 0)
      return -1;
    else 
      return 0;

    // In case of DUMMY_PAGE have added a timer consiting of vptr so in case when timer expired below line will be executed 
    // SHJ:: This is earlier comment => In case of dummy page this will never happen
    vptr->httpData->rbu_resp_attr->sess_start_flag = 1;
  }

   // This function never return other than 0
  if(ns_rbu_make_firefox_command(vptr, page_id, NON_DUMMY_PAGE) != 0)
    return -1;

  if(ns_rbu_run_firefox(vptr, NON_DUMMY_PAGE) != 0)
    return -1;

  return 0;
}

/*--------------------------------------------------------------------------------------------- 
 * Purpose   : This function will called form API ns_web_url(). Here following task will be done ... 
 *              
 *             (1) Check whether the RBU_BROWSER_MODE is Firefox or Chrome Browser
 *                 if 0 = Firefox Browser (Default)
 *                    1 = Chrome Browser
 *
 *             (2) Parametise following - (a) Profile Name (b) Log Dir (c) Vnc Display Id
 *                 If these argument are not given by user then it will be taken from internal variables
 *
 *             (3) Make Firefox command 
 *                 (2.1) Add url , profile , display
 *                 (2.2) Add WAN args, if WAN is ON
 *                 (2.3) Add POST body if method is POST
 *                 (2.4) Add Header if Header is provided in script
 *
 *             (4) Run firefox command  
 *
 *             (5) Wait till har file not made. 
 *
 *             (6) After getting har file add a task VUT_RBU_WEB_URL_END, Switch contex: User --> NVM and
 *                 handle_page_complete() fun will call
 *
 * Input     : vptr    - to point current vptr 
 *             page_id - to provide page id of that PAGE
 *
 * Output    : On success -  0
 *             On Failure - -1  
 *--------------------------------------------------------------------------------------------*/
int ns_rbu_execute_page(VUser *vptr, int page_id)
{
  NSDL2_RBU(vptr, NULL, "Method called, vptr = %p, page_id = %d, rbu_resp_attr = %p", vptr, page_id, vptr->httpData->rbu_resp_attr);

  int ret = -1;

  /* 1. Allocate memory, 2. Get parameters */
  if(ns_rbu_init(vptr) == -1)
  {
    NSTL1_OUT(vptr, NULL, "Failed to start the session");
    strncpy(script_execution_fail_msg, "Internal Error: Failed to start the session.", MAX_SCRIPT_EXECUTION_LOG_LENGTH);
    HANDLE_RBU_PAGE_FAILURE(ret)
    return ret;
  }
  // Execute page via firefox or chrome browser according to browser mode
  if((runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_rbu == 2) &&
     (runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.browser_mode == RBU_BM_CHROME))
  {
    ret = ns_rbu_execute_page_via_node(vptr, page_id);
  }
  else if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.browser_mode == RBU_BM_FIREFOX)
  {
    ret = ns_rbu_execute_page_via_firefox(vptr, page_id);
  }
  else if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.browser_mode == RBU_BM_CHROME)
  {
    ret = ns_rbu_execute_page_via_chrome(vptr, page_id);
  }
  else if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.browser_mode == RBU_BM_VENDOR)
  { 
    //TODO:
    // To support Browser from VendorData.default
    return ret;
  }
  else
  {
    NSTL1_OUT(NULL, NULL, "Error: ns_rbu_execute_page(): provided browser mode not aplicable.\n");
    return ret;
  }
 
  if(ret != 0)
    return ret;
 
  return 0;
}

/*---------------------------------------------------------------------------------------------------------------- 
 * Fun Name  : ns_rbu_make_firefox_ca_req 
 *
 * Purpose   : This function will make msg of click action request for firefox browser 
 *
 * Input     : vptr, req_buf (This is the buffer where msg stored), attr (This will decide whether the 
 *             input is iframe or normal click), snapshot_file (absolute path of file in TRxxx/rbu_logs/snapshot), 
 *             timeout (pageLoadTimeout is in milliseconds), harflag (whether HAR is made or not),
 *             clip_file (absolute path of file in TRxxx/rbu_logs/clips)
 *
 * Format    : Format of msg will be
 *             reload_har_flag=<0/1>|snapshot=<file/NA>|timeout=<time in milliseconds>|sel_mode=<selection_mode>|
 *             clips=<clips_file/NA>|frequency=<freq in milliseconds>|quality=<0-100>|render_enable=<0/1>|
 *             id_or_xpath=<id/xpath>|opcode=<1001/1002>|attr_value=<used for edit field> 
 *
 * Output    : On error     -1
 *             On success   amt_written(length of msg)
 *        
 * Build_v   : 4.1.5 #5 
 *------------------------------------------------------------------------------------------------------------------*/
static int ns_rbu_make_firefox_ca_req(VUser *vptr, char *out_buf, char *attr[], char *snapshot_file, int timeout, int harflag, char *clip_file)
{
  int req_buf_write_idx = 0;
  int free_req_buf_len = RBU_MAX_CMD_LENGTH; 
  char *req_buf = out_buf;
  int timeout_for_next_req = 0;
  RBU_RespAttr *rbu_resp_attr = vptr->httpData->rbu_resp_attr;
  char escape_buf[2048 + 1] = "";
  int flag = 0;

  NSDL2_RBU(vptr, NULL, "Method called, req_buf = %p", req_buf);

  //create message.
  //first check for type.
 
  //Currently we are taking har file and snapshot on each message.
  int iframe_flag = 0;
  int render_enable=0; 
 
  if(attr[IFRAMEID]) 
    iframe_flag = 101;
  else if(attr[IFRAMEXPATH] || attr[IFRAMEXPATH1] || attr[IFRAMEXPATH2] || attr[IFRAMEXPATH3] || attr[IFRAMEXPATH4] || attr[IFRAMEXPATH5])
    iframe_flag = 102;

  //Bug Id-30543
  //G_RBU_PAGE_LOADED_TIMEOUT > 500ms , So WaitForNextReq > 500ms
  //By Default, G_RBU_PAGE_LOADED_TIMEOUT = 5000ms
  timeout_for_next_req  = ((vptr->first_page_url->proto.http.rbu_param.timeout_for_next_req > 500)?
                              vptr->first_page_url->proto.http.rbu_param.timeout_for_next_req :
                              runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.page_loaded_timeout);

  //Adding render_enable settings
  if ((runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_settings >= 100) && 
       !(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_capture_clip)) 
    render_enable = runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_settings;
  else 
    render_enable = 0;
  
  NSDL3_RBU(vptr, NULL, "Adding render enable = %d", render_enable);
  //Note: DomString is not available for iframe.

  req_buf_write_idx = sprintf(req_buf, "reload_har_flag=%d|snapshot=%s|pageLoadedTimeout=%d|timeout=%d|"
                                       "clips=%s|frequency=%d|quality=%d|render_enable=%d|",
                                       harflag, snapshot_file, timeout_for_next_req, timeout, clip_file,
                                       runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.clip_frequency,
                                       runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.clip_quality, render_enable);
  RBU_SET_WRITE_PTR(req_buf, req_buf_write_idx, free_req_buf_len);

  if(iframe_flag) {
    req_buf_write_idx = sprintf(req_buf, "iframe_flag=%d|", iframe_flag); 

    RBU_SET_WRITE_PTR(req_buf, req_buf_write_idx, free_req_buf_len);
  }
  //Below code is commented according to New Design
  //In new design we make a json message to handle sel_mode and id_or_xpath, below is the sample json message for the same.
  //selectionId=[{"idType": 102, "id": "/path/"}, {"idType": 103, "id": "/path/"}]
  //Refer to bug id 29970 
  /*
  if(attr[ID])
    req_buf_write_idx = sprintf(req_buf, "sel_mode=101|id_or_xpath=%s|", attr[ID]);

  else if(attr[XPATH] || attr[XPATH1] || attr[XPATH2] || attr[XPATH3] || attr[XPATH4] || attr[XPATH5]){
    req_buf_write_idx = sprintf(req_buf, "sel_mode=102|");

    if(attr[XPATH]){        
      RBU_SET_WRITE_PTR(req_buf, req_buf_write_idx, free_req_buf_len);
      req_buf_write_idx = sprintf(req_buf, "id_or_xpath=%s|", attr[XPATH]);
    }
    if(attr[XPATH1]){
      RBU_SET_WRITE_PTR(req_buf, req_buf_write_idx, free_req_buf_len);
      req_buf_write_idx = sprintf(req_buf, "id_or_xpath1=%s|", attr[XPATH1]);
    }
    if(attr[XPATH2]){
      RBU_SET_WRITE_PTR(req_buf, req_buf_write_idx, free_req_buf_len);
      req_buf_write_idx = sprintf(req_buf, "id_or_xpath2=%s|", attr[XPATH2]);
    }
    if(attr[XPATH3]){
      RBU_SET_WRITE_PTR(req_buf, req_buf_write_idx, free_req_buf_len);
      req_buf_write_idx = sprintf(req_buf, "id_or_xpath3=%s|", attr[XPATH3]);
    }
    if(attr[XPATH4]){
      RBU_SET_WRITE_PTR(req_buf, req_buf_write_idx, free_req_buf_len);
      req_buf_write_idx = sprintf(req_buf, "id_or_xpath4=%s|", attr[XPATH4]);
    }
    if(attr[XPATH5]){
      RBU_SET_WRITE_PTR(req_buf, req_buf_write_idx, free_req_buf_len);
      req_buf_write_idx = sprintf(req_buf, "id_or_xpath5=%s|", attr[XPATH5]);
    }
  }
  else if(attr[CLASS])
    req_buf_write_idx = sprintf(req_buf, "sel_mode=106|id_or_xpath=%s|", attr[CLASS]);

  else if(attr[DOMSTRING])
    req_buf_write_idx = sprintf(req_buf, "sel_mode=103|id_or_xpath=%s|", attr[DOMSTRING]);

  else if(attr[CSSPATH])
    req_buf_write_idx = sprintf(req_buf, "sel_mode=105|id_or_xpath=%s|", attr[CSSPATH]);

  else {
    NSTL1_OUT(NULL, NULL, "Error: (ns_rbu_send_msg_to_browser), Both ID and XPATH are missing.\n");
    strncpy(rbu_resp_attr->access_log_msg, "Error: Both ID and XPATH are missing.", RBU_MAX_ACC_LOG_LENGTH);
    return -1;    
  }
  RBU_SET_WRITE_PTR(req_buf, req_buf_write_idx, free_req_buf_len);
  */

  /* New Design -> In script ID, XPATH, CLASS is given, it will check first ID then XPATH and then CLASS */
  /* Msg: idType -> selectionId=[{"idType": 102, "id": "/path/"}] */

  req_buf_write_idx = sprintf(req_buf, "selectionId=[");
  RBU_SET_WRITE_PTR(req_buf, req_buf_write_idx, free_req_buf_len); 
 
  char attr_found = 0;
  if(attr[ID])
  {
    attr_found = 1;
    fill_selection_attr(vptr, attr[ID], 101, escape_buf, &req_buf, &free_req_buf_len, &flag);
    NSDL2_RBU(vptr, NULL, "selection id_type(101), free_req_buf_len = %d", free_req_buf_len);
  }

  if(attr[XPATH])
  { 
    attr_found = 1;
    fill_selection_attr(vptr, attr[XPATH], 102, escape_buf, &req_buf, &free_req_buf_len, &flag);
    NSDL2_RBU(vptr, NULL, "selection id_type(102), free_req_buf_len = %d, req_buf = %p", free_req_buf_len, req_buf);
  }

  if(attr[XPATH1])
  {
    attr_found = 1;
    fill_selection_attr(vptr, attr[XPATH1], 102, escape_buf, &req_buf, &free_req_buf_len, &flag);
    NSDL2_RBU(vptr, NULL, "selection id_type(102), free_req_buf_len = %d, req_buf = %p", free_req_buf_len, req_buf);
  }
  
  if(attr[XPATH2])
  {
    attr_found = 1;
    fill_selection_attr(vptr, attr[XPATH2], 102, escape_buf, &req_buf, &free_req_buf_len, &flag);
    NSDL2_RBU(vptr, NULL, "selection id_type(102), free_req_buf_len = %d, req_buf = %p", free_req_buf_len, req_buf);
  }

  if(attr[XPATH3])
  {
    attr_found = 1;
    fill_selection_attr(vptr, attr[XPATH3], 102, escape_buf, &req_buf, &free_req_buf_len, &flag);
    NSDL2_RBU(vptr, NULL, "selection id_type(102), free_req_buf_len = %d, req_buf = %p", free_req_buf_len, req_buf);
  }

  if(attr[XPATH4])
  {
    attr_found = 1;
    fill_selection_attr(vptr, attr[XPATH4], 102, escape_buf, &req_buf, &free_req_buf_len, &flag);
    NSDL2_RBU(vptr, NULL, "selection id_type(102), free_req_buf_len = %d, req_buf = %p", free_req_buf_len, req_buf);
  }

  if(attr[XPATH5])
  {
    attr_found = 1;
    fill_selection_attr(vptr, attr[XPATH5], 102, escape_buf, &req_buf, &free_req_buf_len, &flag);
    NSDL2_RBU(vptr, NULL, "selection id_type(102), free_req_buf_len = %d, req_buf = %p", free_req_buf_len, req_buf);
  }

  if(attr[CLASS])
  {
    attr_found = 1;
    fill_selection_attr(vptr, attr[CLASS], 106, escape_buf, &req_buf, &free_req_buf_len, &flag);
    NSDL2_RBU(vptr, NULL, "selection id_type(106), free_req_buf_len = %d", free_req_buf_len);
  }

  if(attr[DOMSTRING])
  {
    attr_found = 1;
    fill_selection_attr(vptr, attr[DOMSTRING], 103, escape_buf, &req_buf, &free_req_buf_len, &flag);
    NSDL2_RBU(vptr, NULL, "selection id_type(103), free_req_buf_len = %d", free_req_buf_len);
  }
   
  if(attr[CSSPATH])
  {
    attr_found = 1;
    fill_selection_attr(vptr, attr[CSSPATH], 105, escape_buf, &req_buf, &free_req_buf_len, &flag);
    NSDL2_RBU(vptr, NULL, "selection id_type(105), free_req_buf_len = %d", free_req_buf_len);
  } 
  
  else if(attr_found == 0)
  {
    NSTL1_OUT(NULL, NULL, "Error: (ns_rbu_make_firefox_ca_req), ID_TYPE: ID, XPATH and DOMSTRING missing.\n");
    strncpy(rbu_resp_attr->access_log_msg, "Error: ID_TYPE: ID, XPATH and DOMSTRING missing.", RBU_MAX_ACC_LOG_LENGTH);
    strncpy(script_execution_fail_msg, "ID_TYPE: ID, XPATH and DOMSTRING missing in script.", MAX_SCRIPT_EXECUTION_LOG_LENGTH);
    return -1;
  }

  req_buf_write_idx = sprintf(req_buf, "]|");
  RBU_SET_WRITE_PTR(req_buf, req_buf_write_idx, free_req_buf_len);

  //set opcode as per apiname.
  
  //Below element will be called for click.
  if(iframe_flag == 101){
    req_buf_write_idx = sprintf(req_buf, "iframe_selector=%s|", attr[IFRAMEID]);
    RBU_SET_WRITE_PTR(req_buf, req_buf_write_idx, free_req_buf_len);
  }
  else if(iframe_flag == 102){
    if(attr[IFRAMEXPATH]){
      req_buf_write_idx = sprintf(req_buf, "iframe_selector=%s|", attr[IFRAMEXPATH]);
      RBU_SET_WRITE_PTR(req_buf, req_buf_write_idx, free_req_buf_len);
    }
    if(attr[IFRAMEXPATH1]){
      req_buf_write_idx = sprintf(req_buf, "iframe_selector1=%s|", attr[IFRAMEXPATH1]);
      RBU_SET_WRITE_PTR(req_buf, req_buf_write_idx, free_req_buf_len);
    }
    if(attr[IFRAMEXPATH2]){
      req_buf_write_idx = sprintf(req_buf, "iframe_selector2=%s|", attr[IFRAMEXPATH2]);
      RBU_SET_WRITE_PTR(req_buf, req_buf_write_idx, free_req_buf_len);
    }
    if(attr[IFRAMEXPATH3]){
      req_buf_write_idx = sprintf(req_buf, "iframe_selector3=%s|", attr[IFRAMEXPATH3]);
      RBU_SET_WRITE_PTR(req_buf, req_buf_write_idx, free_req_buf_len);
    }
    if(attr[IFRAMEXPATH4]){
      req_buf_write_idx = sprintf(req_buf, "iframe_selector4=%s|", attr[IFRAMEXPATH4]);
      RBU_SET_WRITE_PTR(req_buf, req_buf_write_idx, free_req_buf_len);
    } 
    if(attr[IFRAMEXPATH5]){
      req_buf_write_idx = sprintf(req_buf, "iframe_selector5=%s|", attr[IFRAMEXPATH5]);
      RBU_SET_WRITE_PTR(req_buf, req_buf_write_idx, free_req_buf_len);
    }
  }
 
  if(!strncasecmp(attr[APINAME], "ns_button", 9) || 
     !strncasecmp(attr[APINAME], "ns_span", 7) ||
     !strncasecmp(attr[APINAME], "ns_link", 7) ||
     !strncasecmp(attr[APINAME], "ns_map_area", 11)) {
     int focus_only = attr[FOCUSONLY]?atoi(attr[FOCUSONLY]):0;

     if(!focus_only)
       req_buf_write_idx = sprintf(req_buf, "opcode=1001|attr_value=NA\n");
     else{
       NSDL3_RBU(vptr, NULL, "Found ns_link, with focus only event");
       req_buf_write_idx = sprintf(req_buf, "opcode=1011|attr_value=NA\n");
     }
  } 
  else if(!strncasecmp(attr[APINAME], "ns_edit_field", 13) ||
          !strncasecmp(attr[APINAME], "ns_text_area", 12))
    req_buf_write_idx = sprintf(req_buf, "opcode=1002|attr_value=%s\n", attr[3]);
  
  else if(!strncasecmp(attr[APINAME], "ns_check_box", 12)) 
    //In this case we are sending value(on/off).
    req_buf_write_idx = sprintf(req_buf, "opcode=1003|attr_value=%s\n", attr[3]);
 
  else if(!strncasecmp(attr[APINAME], "ns_radio_group", 14)) 
    req_buf_write_idx = sprintf(req_buf, "opcode=1004|attr_value=%s\n", attr[3]);
  
  else if(!strncasecmp(attr[APINAME], "ns_list", 7))
    req_buf_write_idx = sprintf(req_buf, "opcode=1005|attr_value=%s\n", attr[CONTENT]);
  
  else if(!strncasecmp(attr[APINAME], "ns_mouse_hover", 14))
    req_buf_write_idx = sprintf(req_buf, "opcode=1011|attr_value=NA");

  else if(!strncasecmp(attr[APINAME], "ns_mouse_out", 12))
    req_buf_write_idx = sprintf(req_buf, "opcode=1012|attr_value=NA");

  else if(!strncasecmp(attr[APINAME], "ns_mouse_move", 13))
    req_buf_write_idx = sprintf(req_buf, "opcode=1013|attr_value=NA");
 
  RBU_SET_WRITE_PTR(req_buf, req_buf_write_idx, free_req_buf_len); 
  return (req_buf - out_buf);
}

static int escape_quotes_and_copy(char *in, char *out)
{
  int i = 0;
  int j = 0;

  if(in == NULL || *in == '\0')
  {
    strcpy(out, "NA");
    return -1;
  }
  
  while(in[i]) 
  {
    if(in[i] == '"')
    {
      out[j++] = '\\';
      out[j++] = '"';
    }
    else 
      out[j++] = in[i];
    i++;
  }
  out[j] = 0;
  return j;
}

inline void fill_selection_attr(VUser *vptr, char *value, int id_type, char *escape_buf, char **cmd_buf_ptr, int *free_cmd_buf_len, int *flag) 
{
  int cmd_write_idx = 0;
  char *selctionId = value?value:"NA";

  NSDL3_RBU(NULL, NULL, "Method called, attr = %s, id = %d, cmd_buf_ptr = %p, free_cmd_buf_len = %d", 
                         value, id_type, cmd_buf_ptr, *free_cmd_buf_len); 

  escape_quotes_and_copy(selctionId, escape_buf); 
  selctionId = escape_buf;

  if(*flag == 0) 
  {
    cmd_write_idx = snprintf(*cmd_buf_ptr, *free_cmd_buf_len, "{\"idType\": %d, \"id\": \"%s\"}", id_type, selctionId); 
    *flag = 1;
  }
  else
    cmd_write_idx = snprintf(*cmd_buf_ptr, *free_cmd_buf_len, ", {\"idType\": %d, \"id\": \"%s\"}", id_type, selctionId);

  NSDL2_RBU(NULL, NULL, "Chrome/Firefox CA Message: selectionID = %s", *cmd_buf_ptr); 

  RBU_SET_WRITE_PTR(*cmd_buf_ptr, cmd_write_idx, *free_cmd_buf_len);
}

inline void fill_selectioniframe_attr(VUser *vptr, char *value, int id_type, char *escape_buf, char **cmd_buf_ptr, int *free_cmd_buf_len, int *flag) 
{
  int cmd_write_idx = 0;
  char *selctionId = value?value:"NA"; 

  NSDL3_RBU(NULL, NULL, "Method called, attr = %s, id = %d, cmd_buf_ptr = %p, free_cmd_buf_len = %d, selctionId = %s",
                         value, id_type, cmd_buf_ptr, *free_cmd_buf_len, selctionId);
 
  escape_quotes_and_copy(selctionId, escape_buf); 
  selctionId = escape_buf;

  if(*flag == 0)
  {  
    cmd_write_idx = snprintf(*cmd_buf_ptr, *free_cmd_buf_len, "{\"idType\": %d, \"id\": \"%s\"}", id_type, selctionId);
    *flag = 1;
  }
  else
    cmd_write_idx = snprintf(*cmd_buf_ptr, *free_cmd_buf_len, ", {\"idType\": %d, \"id\": \"%s\"}", id_type, selctionId);

  NSDL2_RBU(NULL, NULL, "Chrome CA Message: selectionIframeID = %s", *cmd_buf_ptr); 
  
  RBU_SET_WRITE_PTR(*cmd_buf_ptr, cmd_write_idx, *free_cmd_buf_len); 
} 

/* This method will send message to chrome browser */
/*
 {"opcode": "1002", "params": {"id": "", idType: 101, action: 1001, value: "****", iframe: {"id": "", "idType": 101}, "captureHar": {"reloadHar": 0, "path": ""}, "snapshot": {"enable": true, "file": ""}, "captureClips": {"enable": true, "clipDir": "", "frequency": 100, "quality": 0}}}
*/
static int ns_rbu_make_chrome_ca_req(VUser *vptr, char *req_buf, char *attr[], char *snapshot_file, int timeout, int harflag, char *clip_file, 
long long scroll_page_x, long  long scroll_page_y)
{
  //TODO: handle it properly
  char escape_buf[2048 + 1] = "";
  char *cmd_buf_ptr = req_buf;
  char *scroll_enable = "false"; // for page scroll 
  int timeout_for_next_req = 0;
  int phase_interval_for_page_load = 0;
  int cmd_write_idx = 0;
  int free_cmd_buf_len = RBU_MAX_CMD_LENGTH;
  int id_type = 0;
  int action = 0;
  int flag = 0; 

  RBU_RespAttr *rbu_resp_attr = vptr->httpData->rbu_resp_attr;
  NSDL2_RBU(vptr, NULL, "Method called, req_buf = %p, snapshot_file = %s, clip_file = %s, timeout = %d, harflag = %d", 
                         req_buf, snapshot_file?snapshot_file:"NULL", clip_file, timeout, harflag);

  /* Msg: start - {"opcode": "1002", "params": { */
  NSDL2_RBU(vptr, NULL, "Chrome CA Message: (Start): opcode and param");
  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len,
                              "{\"opcode\": \"1002\", "
                              " \"params\": "
                                   "{\"selectionId\": [");
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);

  /* New Design -> In script ID, XPATH, CLASS is given, it will check first ID then XPATH and then CLASS */

  /* Msg: idType - "idType": 102, "id": "/path/" */
  
  char attr_found = 0;
  if(attr[ID])
  { 
    attr_found = 1;
    fill_selection_attr(vptr, attr[ID], 101, escape_buf, &cmd_buf_ptr, &free_cmd_buf_len, &flag); 
    NSDL2_RBU(vptr, NULL, "selection id_type(101), free_cmd_buf_len = %d", free_cmd_buf_len);
  }

  if(attr[XPATH])
  {
    attr_found = 1;
    fill_selection_attr(vptr, attr[XPATH], 102, escape_buf, &cmd_buf_ptr, &free_cmd_buf_len, &flag);
    NSDL2_RBU(vptr, NULL, "selection id_type(102), free_cmd_buf_len = %d, cmd_buf_ptr = %p", free_cmd_buf_len, cmd_buf_ptr);
  }

  /*Resolved bug 24535 - NS RBU Enhancement*/
  if(attr[XPATH1])
  {
    attr_found = 1;
    fill_selection_attr(vptr, attr[XPATH1], 102, escape_buf, &cmd_buf_ptr, &free_cmd_buf_len, &flag);
    NSDL2_RBU(vptr, NULL, "selection id_type(102), free_cmd_buf_len = %d, cmd_buf_ptr = %p", free_cmd_buf_len, cmd_buf_ptr);
  }

  if(attr[XPATH2])
  {
    attr_found = 1;
    fill_selection_attr(vptr, attr[XPATH2], 102, escape_buf, &cmd_buf_ptr, &free_cmd_buf_len, &flag);
    NSDL2_RBU(vptr, NULL, "selection id_type(102), free_cmd_buf_len = %d, cmd_buf_ptr = %p", free_cmd_buf_len, cmd_buf_ptr);
  }

  if(attr[XPATH3])
  {
    attr_found = 1;
    fill_selection_attr(vptr, attr[XPATH3], 102, escape_buf, &cmd_buf_ptr, &free_cmd_buf_len, &flag);
    NSDL2_RBU(vptr, NULL, "selection id_type(102), free_cmd_buf_len = %d, cmd_buf_ptr = %p", free_cmd_buf_len, cmd_buf_ptr);
  }

  if(attr[XPATH4])
  {
    attr_found = 1;
    fill_selection_attr(vptr, attr[XPATH4], 102, escape_buf, &cmd_buf_ptr, &free_cmd_buf_len, &flag);
    NSDL2_RBU(vptr, NULL, "selection id_type(102), free_cmd_buf_len = %d, cmd_buf_ptr = %p", free_cmd_buf_len, cmd_buf_ptr);
  }

  if(attr[XPATH5])
  {
    attr_found = 1;
    fill_selection_attr(vptr, attr[XPATH5], 102, escape_buf, &cmd_buf_ptr, &free_cmd_buf_len, &flag);
    NSDL2_RBU(vptr, NULL, "selection id_type(102), free_cmd_buf_len = %d, cmd_buf_ptr = %p", free_cmd_buf_len, cmd_buf_ptr);
  }
   
  if(attr[CLASS])
  { 
    attr_found = 1;
    fill_selection_attr(vptr, attr[CLASS], 106, escape_buf, &cmd_buf_ptr, &free_cmd_buf_len, &flag);
    NSDL2_RBU(vptr, NULL, "selection id_type(106), free_cmd_buf_len = %d", free_cmd_buf_len);
  }

  if(attr[DOMSTRING])
  {
    attr_found = 1;
    fill_selection_attr(vptr, attr[DOMSTRING], 103, escape_buf, &cmd_buf_ptr, &free_cmd_buf_len, &flag);
    NSDL2_RBU(vptr, NULL, "selection id_type(103), free_cmd_buf_len = %d", free_cmd_buf_len);
  }

  if(attr[CSSPATH])
  {
    attr_found = 1;
    fill_selection_attr(vptr, attr[CSSPATH], 105, escape_buf, &cmd_buf_ptr, &free_cmd_buf_len, &flag);
    NSDL2_RBU(vptr, NULL, "selection id_type(105), free_cmd_buf_len = %d", free_cmd_buf_len);
  }

  if(attr[NAME] || attr[SRC] || attr[HREF] || attr[ALT] || attr[TITLE] || attr[TEXT])
  {
    attr_found = 1;
    if(flag == 0)
      cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "{\"idType\": 107, \"id\": {");
    else
      cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, ", {\"idType\": 107, \"id\": {");
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);

    if(attr[NAME])
    {
      cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"name\": \"%s\", ", attr[NAME]);
      RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
    }

    if(attr[SRC])
    {
      cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"src\": \"%s\", ", attr[SRC]);
      RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
    }

    if(attr[HREF])
    {
      cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"href\": \"%s\", ", attr[HREF]);
      RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
    }

    if(attr[ALT])
    {
      cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"alt\": \"%s\", ", attr[ALT]);
      RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
    }

    if(attr[TITLE])
    {
      cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"title\": \"%s\", ", attr[TITLE]);
      RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
    }

    if(attr[TEXT])
    {
      cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"text\": \"%s\", ", attr[TEXT]);
      RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
    }

    //-2: overwrite last comma & space(, )
    cmd_buf_ptr -= 2;
    free_cmd_buf_len += 2;
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "}}");
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  }

  if(attr[COORDINATES])
    attr_found = 1;

  id_type = attr[SCROLLPAGEX]?104:0;

  if(id_type == 104)
  {
    attr_found = 1;
    fill_selection_attr(vptr, "NA", 104, escape_buf, &cmd_buf_ptr, &free_cmd_buf_len, &flag);
    NSDL2_RBU(vptr, NULL, "selection id_type(104), free_cmd_buf_len = %d", free_cmd_buf_len);
  }
  else if((id_type == 0) && (attr_found == 0) && strncasecmp(attr[APINAME], "ns_execute_js", 13))
  {
    NSTL1_OUT(NULL, NULL, "Error: (ns_rbu_make_chrome_ca_req), ID_TYPE: ID, XPATH and DOMSTRING missing.\n");
    strncpy(rbu_resp_attr->access_log_msg, "Error: ID_TYPE: ID, XPATH and DOMSTRING missing.", RBU_MAX_ACC_LOG_LENGTH);
    strncpy(script_execution_fail_msg, "ID_TYPE: ID, XPATH and DOMSTRING missing in script.", MAX_SCRIPT_EXECUTION_LOG_LENGTH);
    return -1;
  }

  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "], ");
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);

  if(attr[TAG])
  {
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"tag\": \"%s\", ", attr[TAG]);
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  }

  if(attr[AUTOSELECTOR])
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"autoSelector\": \"%s\", ", attr[AUTOSELECTOR]);
  else
  {
    if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_auto_selector_mode == 1)
      cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"autoSelector\": \"true[Last]\", ");
    else if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_auto_selector_mode == 2)
      cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"autoSelector\": \"true[First]\", ");
    else
      cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"autoSelector\": false, ");
  }
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);

  if(!strncasecmp(attr[APINAME], "ns_js_checkpoint", 16))
  {
    int valueType_int;

    if(attr[valueType])
    {
      if(!strncasecmp(attr[valueType], "static", 6))
        valueType_int = 0;
      else if(!strncasecmp(attr[valueType], "JS String", 9))
        valueType_int = 1;
      else if(!strncasecmp(attr[valueType], "cookie", 6))
        valueType_int = 2;
      else if(!strncasecmp(attr[valueType], "dom element", 11))
        valueType_int = 3;
      else 
      {
        NSTL1_OUT(NULL, NULL, "Error: (ns_rbu_make_chrome_ca_req), Invalid argument '%s' passed with attribute 'valueType'.\n", 
                        attr[valueType]);
        strncpy(rbu_resp_attr->access_log_msg, "Error: Invalid argument passed with Attribute 'valueType'.", RBU_MAX_ACC_LOG_LENGTH);
        snprintf(script_execution_fail_msg, MAX_SCRIPT_EXECUTION_LOG_LENGTH, "Invalid argument '%s' passed with attribute 'valueType'.", 
                        attr[valueType]);
        return -1;
      }
    }
    else
    {
      NSTL1_OUT(NULL, NULL, "Error: (ns_rbu_make_chrome_ca_req), Attribute 'valueType' is missing with ns_js_checkpoint() API.\n");
      strncpy(rbu_resp_attr->access_log_msg, "Error: Attribute 'valueType' missing.", RBU_MAX_ACC_LOG_LENGTH);
      strncpy(script_execution_fail_msg, "Attribute 'valueType' is missing with ns_js_checkpoint() API.", MAX_SCRIPT_EXECUTION_LOG_LENGTH);
      return -1;
    }

    if(valueType_int == 3)
    {
      if(attr[CSSPATH1])
        escape_quotes_and_copy(attr[CSSPATH1], escape_buf);
      else
      {
        NSTL1_OUT(NULL, NULL, "Error: (ns_rbu_make_chrome_ca_req), 'csspath1'(css selector of 2nd Element) is missing in ns_js_checkpoint() API.\n");
        strncpy(rbu_resp_attr->access_log_msg, "Error: 'csspath1'(css selector of 2nd Element) is missing in ns_js_checkpoint() API.",
          RBU_MAX_ACC_LOG_LENGTH);
        snprintf(script_execution_fail_msg, MAX_SCRIPT_EXECUTION_LOG_LENGTH, "'csspath1'(css selector of 2nd Element) is missing in ns_js_checkpoint() API.");
        return -1; 
      }
      cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"checkpoint\":{\"name\": \"%s\", \"valueType\": \"%d\", "
                                                                            "\"propertyName\": \"%s\", \"propertyName1\": \"%s\", "
                                                                            "\"css_selector1\": \"%s\", "
                                                                            "\"operator\": \"%s\", \"abortTest\": \"%s\"}, ",
                      vptr->cur_page->page_name, valueType_int, attr[propertyName], attr[propertyName1], escape_buf,
                      attr[OPERATOR], attr[abortTest]); 
    }
    else
    {
      cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"checkpoint\":{\"name\": \"%s\", \"value\": %s, \"valueType\": \"%d\", "
                                                                            "\"propertyName\": \"%s\", \"operator\": \"%s\", "
                                                                            "\"abortTest\": \"%s\"}, ",
                      vptr->cur_page->page_name, attr[3], valueType_int, attr[propertyName], attr[OPERATOR], attr[abortTest]);
    }
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  }

  if(attr[COORDINATES])
  {
    NSDL4_RBU(vptr, NULL, "attr[COORDINATES] = %s", attr[COORDINATES]);
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"coordinates\": %s, ", attr[COORDINATES]); 
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
    
  }

  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.selector_mapping_mode == 1)
  {
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"selectorMapping\": true, ");
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  }

  /* Msg: "action": 1001, "value": */
  if(!strncasecmp(attr[APINAME], "ns_button", 9) || 
     !strncasecmp(attr[APINAME], "ns_span", 7) ||
     !strncasecmp(attr[APINAME], "ns_link", 7) ||
     !strncasecmp(attr[APINAME], "ns_map_area", 11))
    action = 1001; 
  else if(!strncasecmp(attr[APINAME], "ns_mouse_hover", 14))
    action = 1011;
  else if(!strncasecmp(attr[APINAME], "ns_mouse_out", 12))
    action = 1012;
  else if(!strncasecmp(attr[APINAME], "ns_mouse_move", 13))
    action = 1013;
  else if(!strncasecmp(attr[APINAME], "ns_edit_field", 13) ||
          !strncasecmp(attr[APINAME], "ns_text_area", 12))
    action = 1002; 
  else if(!strncasecmp(attr[APINAME], "ns_check_box", 12)) 
    action = 1003; 
  else if(!strncasecmp(attr[APINAME], "ns_radio_group", 14)) 
    action = 1004; 
  else if(!strncasecmp(attr[APINAME], "ns_list", 7))
    action = 1005;
  else if(!strncasecmp(attr[APINAME], "ns_scroll", 9))
    action = 1006;
  else if(!strncasecmp(attr[APINAME], "ns_key_event", 12))
    action = 1021;
  else if(!strncasecmp(attr[APINAME], "ns_get_num_domelement", strlen("ns_get_num_domelement")))
    action = 1007;
  else if(!strncasecmp(attr[APINAME], "ns_browse_file", 14)) {
    action = 1008;
    if( access( attr[3], F_OK ) != -1 )
      NSDL2_RBU(vptr, NULL, "File '%s' exists.", attr[3]);
    else {
      NS_DT1(vptr, NULL, DM_L1, MM_VARS, "Error: File '%s' provided in ns_browse_file() API does not exist.", attr[3]);
      NSTL1_OUT(NULL, NULL, "Error: File '%s' provided in ns_browse_file() API does not exist.", attr[3]);
      snprintf(rbu_resp_attr->access_log_msg, RBU_MAX_ACC_LOG_LENGTH,
                "Error: File '%s' provided in ns_browse_file() API does not exist.", attr[3]);
      snprintf(script_execution_fail_msg, MAX_SCRIPT_EXECUTION_LOG_LENGTH,
                "File '%s' provided in ns_browse_file() API does not exist.", attr[3]);
      return -1;
    }
  }
  else if(!strncasecmp(attr[APINAME], "ns_js_checkpoint", 16))
    action = 1009;
  else if(!strncasecmp(attr[APINAME], "ns_execute_js", 13))
    action = 2001;

  char *value_ptr = (action != 1005)?attr[3]:attr[CONTENT]?attr[CONTENT]:"";
  escape_quotes_and_copy(value_ptr, escape_buf);
  value_ptr = escape_buf;

  // in ns_vars.h VALUE macro is 0 so take 3 insted of macro
  NSDL2_RBU(vptr, NULL, "Chrome CA Message: action = %d, value = %s, value_ptr = %s", action, attr[3], value_ptr);
  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"action\": %d, \"value\": \"%s\", ", action, value_ptr?value_ptr:"NA"); 
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);

  //Bug - 59393: clickAction i.e., ACTION
  if(action == 1001)
  {
    int clickAction = 0;                                      //"ACTION=click"
    if(attr[ACTION])
    {
      if(!strncasecmp(attr[ACTION], "double-click", 12))      //"ACTION=double-click"
        clickAction = 1;
      else if(!strncasecmp(attr[ACTION], "right-click", 11))  //"ACTION=right-click"
        clickAction = 2;
      NSDL2_RBU(vptr, NULL, "Chrome CA Message: ACTION = %s, clickAction = %d", attr[ACTION], clickAction);
    }
  
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"clickAction\": %d, ", clickAction); 
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  }

  /* Msg: iframe: {"id": "", "idType": 101} */

  flag = 0;
  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"selectionIframeId\": [");
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);

  if(attr[IFRAMEID])
  {
    fill_selectioniframe_attr(vptr, attr[IFRAMEID], 101, escape_buf, &cmd_buf_ptr, &free_cmd_buf_len, &flag);
    NSDL2_RBU(vptr, NULL, "selectionIframe id_type = %d", id_type);
  }

  if(attr[IFRAMEXPATH])
  {
    fill_selectioniframe_attr(vptr, attr[IFRAMEXPATH], 102, escape_buf, &cmd_buf_ptr, &free_cmd_buf_len, &flag);
    NSDL2_RBU(vptr, NULL, "selectionIframe id_type = %d", id_type);
  }

  if(attr[IFRAMEXPATH1])
  {
    fill_selectioniframe_attr(vptr, attr[IFRAMEXPATH1], 102, escape_buf, &cmd_buf_ptr, &free_cmd_buf_len, &flag);
    NSDL2_RBU(vptr, NULL, "selectionIframe id_type = %d", id_type);
  }

  if(attr[IFRAMEXPATH2])
  {
    fill_selectioniframe_attr(vptr, attr[IFRAMEXPATH2], 102, escape_buf, &cmd_buf_ptr, &free_cmd_buf_len, &flag);
    NSDL2_RBU(vptr, NULL, "selectionIframe id_type = %d", id_type);
  }

  if(attr[IFRAMEXPATH3])
  {
    fill_selectioniframe_attr(vptr, attr[IFRAMEXPATH3], 102, escape_buf, &cmd_buf_ptr, &free_cmd_buf_len, &flag);
    NSDL2_RBU(vptr, NULL, "selectionIframe id_type = %d", id_type);
  }

  if(attr[IFRAMEXPATH4])
  {
    fill_selectioniframe_attr(vptr, attr[IFRAMEXPATH4], 102, escape_buf, &cmd_buf_ptr, &free_cmd_buf_len, &flag);
    NSDL2_RBU(vptr, NULL, "selectionIframe id_type = %d", id_type);
  }

  if(attr[IFRAMEXPATH5])
  {
    fill_selectioniframe_attr(vptr, attr[IFRAMEXPATH5], 102, escape_buf, &cmd_buf_ptr, &free_cmd_buf_len, &flag);
    NSDL2_RBU(vptr, NULL, "selectionIframe id_type = %d", id_type);
  }

  if(attr[IFRAMECLASS])
  {
    fill_selectioniframe_attr(vptr, attr[IFRAMECLASS], 106, escape_buf, &cmd_buf_ptr, &free_cmd_buf_len, &flag);
    NSDL2_RBU(vptr, NULL, "selectionIframe id_type = %d", id_type);
  }

  if(attr[IFRAMEDOMSTRING])
  {
    fill_selectioniframe_attr(vptr, attr[IFRAMEDOMSTRING], 103, escape_buf, &cmd_buf_ptr, &free_cmd_buf_len, &flag);
    NSDL2_RBU(vptr, NULL, "selectionIframe id_type = 103");
  }

  if(attr[IFRAMECSSPATH])
  {
    fill_selectioniframe_attr(vptr, attr[IFRAMECSSPATH], 105, escape_buf, &cmd_buf_ptr, &free_cmd_buf_len, &flag);
    NSDL2_RBU(vptr, NULL, "selectionIframe id_type = 105");
  }

  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "], ");
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);

  /* Msg: "captureHar": {"reloadHar": 0, "path": ""} */
  /* Msg: "captureHar": {"reloadHar": 0, "path": "", "threshold": 0} */
  NSDL2_RBU(vptr, NULL, "Chrome CA Message: captureHar - harflag = %d, path = %s", harflag, vptr->httpData->rbu_resp_attr->har_log_path);
  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"captureHar\": {\"reloadHar\": %d, \"path\": \"\", \"threshold\": %d}, ", 
                             harflag, runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.har_threshold);
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);

  /* Msg: "snapshot": {"enable": true, "file": ""} */
  NSDL2_RBU(vptr, NULL, "Chrome CA Message: snapShot - file = %s", snapshot_file);
  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"snapshot\": {\"enable\": %s, \"file\": \"%s\"}, ", 
                                         (runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_screen_shot && harflag )?"true":"false", 
                                          !strcmp(snapshot_file, "NA")?"NA":(snapshot_file + strlen(vptr->httpData->rbu_resp_attr->har_log_path) + strlen( vptr->httpData->rbu_resp_attr->profile) + 2));
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);

  /* Msg: "captureClips": {"enable": true, "file": ""} */
  NSDL2_RBU(vptr, NULL, "Chrome CA Message: clip_file = %s ", clip_file);
  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len,
                          "\"captureClips\": {\"enable\": %d, \"clipDir\": \"%s\", \"frequency\": %d, \"quality\": %d, \"domload_th\": %d, \"onload_th\": %d}, ",
                          runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_capture_clip, !strcmp(clip_file, "NA")?"NA":(clip_file + strlen(vptr->httpData->rbu_resp_attr->har_log_path) + strlen(vptr->httpData->rbu_resp_attr->profile) + 2),
                          attr[CLIPINTERVAL]?atoi(attr[CLIPINTERVAL]):runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.clip_frequency,
                          runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.clip_quality, 
                          runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.clip_domload_th, 
                          runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.clip_onload_th);
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  
  //G_RBU_PAGE_LOADED_TIMEOUT > 500ms , So WaitForNextReq > 500ms
  //By Default, G_RBU_PAGE_LOADED_TIMEOUT = 5000ms
  /* Msg: Add pageLoadedTimeout : "pageLoadedTimeout": 1200 */
  timeout_for_next_req  = ((vptr->first_page_url->proto.http.rbu_param.timeout_for_next_req > 500)?
                              vptr->first_page_url->proto.http.rbu_param.timeout_for_next_req :
                              runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.page_loaded_timeout);

  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"pageLoadedTimeout\": \"%d\", ",
                                         timeout_for_next_req);
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);

  NSDL2_RBU(NULL, NULL, "Setting pageLoadedTimeout = %d ", timeout_for_next_req);

  /*Bug-73558: G_RBU_PAGE_LOADED_TIMEOUT(phase_interval) > 2000ms , So PhaseInterval > 2000ms
   *By Default, G_RBU_PAGE_LOADED_TIMEOUT - phase_interval = 4000ms
   *Msg: Add pageLoadPhaseInterval : "pageLoadPhaseInterval": 4000 */
  phase_interval_for_page_load = ((vptr->first_page_url->proto.http.rbu_param.phase_interval_for_page_load > 2000)?
                                     vptr->first_page_url->proto.http.rbu_param.phase_interval_for_page_load :
                                     runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.pg_load_phase_interval);

  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"pageLoadPhaseInterval\": \"%d\", ",
                                         phase_interval_for_page_load);
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);

  NSDL2_RBU(NULL, NULL, "Setting pageLoadPhaseInterval = %d ", phase_interval_for_page_load);


  /* Msg: Add timeout : "timeout": 60000 */
  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"timeout\": \"%d\", ", timeout);
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);

  /* Bug 58849 - We are not getting the page element time of a text box while passing the value in parent text box
   * Msg: Add waitForActionDone
   * waitForActionDone : {"mark": "myMark1"} 
   * waitForActionDone : {"measure": "myMeasure"}
   */
  if(attr[WaitForActionDone])
  {
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"waitForActionDone\": %s, ", attr[WaitForActionDone]);
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  }

  /*Bug 59603: Performance Trace Dump
   *Message Format::
   *"performanceTrace": {"enable": true, "timeout": 10000, "memoryTrace": 1, "screenshot": 0, "durationLevel": 0, "filename": "performance_trace/P_index+NSAppliance-work-cavisson-TR5627-1-1+kohls.com+0_0_0_1_0_0_0_1_0.json"}
   */
  if(attr[PerformanceTraceLog] || runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.performance_trace_mode == 1)
  {
    char mode = 0;
    int perf_timeout = RBU_DEFAULT_PERFORMANCE_TRACE_TIMEOUT;
    char memoryTrace = 1;
    char screenshot = 0;
    char duration_level = 0;
    char host[RBU_MAX_BUF_LENGTH]; 
    char *ptr = NULL;

    if(rbu_resp_attr->performance_trace_filename == NULL)
      MY_MALLOC(rbu_resp_attr->performance_trace_filename, RBU_MAX_TRACE_FILE_LENGTH + 1,
                              "vptr->httpData->rbu_resp_attr->performance_trace_filename", 1);

    PerHostSvrTableEntry_Shr* svr_entry = get_svr_entry(vptr, vptr->cur_page->first_eurl->index.svr_ptr);

    snprintf(host, RBU_MAX_BUF_LENGTH, "%s", svr_entry->server_name);

    ptr = host;
    while(*ptr)
    {
      if(*ptr == ':') 
      {
        *ptr = '-';
        break;
      }
      ptr++;
    }

    /*Performance Trace Dump file name format:
     *P_<page_name>+<Chrome-Profile-Name>+<Host>+<child_idx>_<user_index>_<sess_inst>_<page_instance>_0_<group_num>_<sess_id>_<page_id>_0.json
     */

    snprintf(rbu_resp_attr->performance_trace_filename, RBU_MAX_TRACE_FILE_LENGTH, "%s+%s+%hd_%u_%u_%d_0_%d_%d_%d_0.json",
               rbu_resp_attr->profile, host, child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,
               vptr->group_num, GET_SESS_ID_BY_NAME(vptr), GET_PAGE_ID_BY_NAME(vptr));

    if(attr[PerformanceTraceLog])
    {
      int num_fields = 0;
      char *fields[5];
      //Input Format: <mode>:<timeout>:<memory_flag>:<screenshot_flag>:<duartion_level>
      num_fields = get_tokens_ex2(attr[PerformanceTraceLog], fields, ":", 5);
 
      switch(num_fields)
      {
        case 5:
          if((ns_is_numeric(fields[4]) == 0) || (match_pattern(fields[4], "^[0-1]$") == 0))
          {
            NS_DT1(vptr, NULL, DM_L1, MM_VARS, "Warning: PerformanceTraceLog-Duration-Level (%s) is not valid. "
                                               "Using default configuration", fields[4]);
            NSTL1_OUT(NULL, NULL, "Warning: PerformanceTraceLog-Duration-Level (%s) is not valid. "
                                  "Using default configuration", fields[4]);
          }
          else
            duration_level = atoi(fields[4]);
        case 4:
          if((ns_is_numeric(fields[3]) == 0) || (match_pattern(fields[3], "^[0-1]$") == 0))
          {
            NS_DT1(vptr, NULL, DM_L1, MM_VARS, "Warning: PerformanceTraceLog-Screenshot Mode (%s) is not valid. "
                                               "Using default configuration", fields[3]);
            NSTL1(NULL, NULL, "Warning: PerformanceTraceLog-Screenshot Mode (%s) is not valid. "
                              "Using default configuration", fields[3]);
            NS_DUMP_WARNING("PerformanceTraceLog-Screenshot Mode (%s) is not valid. "
                                  "Using default configuration", fields[3]);
          }
          else
            screenshot = atoi(fields[3]);
        case 3:
          if((ns_is_numeric(fields[2]) == 0) || (match_pattern(fields[2], "^[0-1]$") == 0))
          {
            NS_DT1(vptr, NULL, DM_L1, MM_VARS, "Warning: PerformanceTraceLog-Memory-Trace Mode (%s) is not valid. "
                                               "Using default configuration", fields[2]);
            NSTL1(NULL, NULL, "Warning: PerformanceTraceLog-Memory-Trace Mode (%s) is not valid. "
                              "Using default configuration", fields[2]);
            NS_DUMP_WARNING("PerformanceTraceLog-Memory-Trace Mode (%s) is not valid. "
                                  "Using default configuration", fields[2]);
          }
          else
            memoryTrace = atoi(fields[2]);
        case 2:
          if(ns_is_numeric(fields[1]) == 0)
          {
            NS_DT1(vptr, NULL, DM_L1, MM_VARS, "Warning: PerformanceTraceLog-Timeout value (%s) is not numeric. "
                                               "Using default configuration.", fields[1]);
            NSTL1(NULL, NULL, "Warning: PerformanceTraceLog-Timeout value (%s) is not numeric. "
                              "Using default configuration.", fields[1]);
            NS_DUMP_WARNING("PerformanceTraceLog-Timeout value (%s) is not numeric. "
                                  "Using default configuration.", fields[1]);
          }
          else
          {
            perf_timeout = atoi(fields[1]);
            if(perf_timeout < 10000 || perf_timeout > 120000)
            {
              NSTL1(NULL, NULL, "Warning: Timeout value (%d) should be between 10000 msec and 60000 msec. "
                                "Using default configuration.", perf_timeout);
              NSTL1_OUT(NULL, NULL, "Warning: Timeout value (%d) should be between 10000 msec and 120000 msec. "
                                    "Using default configuration.", perf_timeout);
              NS_DT1(vptr, NULL, DM_L1, MM_VARS, "Warning: Timeout value (%d) should be between 10000 msec and 120000 msec. "
                                                 "Using default configuration.", perf_timeout);
              NS_DUMP_WARNING("Timeout value (%d) should be between 10000 msec and 60000 msec. "
                                "Using default configuration.", perf_timeout);
              perf_timeout = RBU_DEFAULT_PERFORMANCE_TRACE_TIMEOUT;
            }
          }
        case 1:
          if((ns_is_numeric(fields[0]) == 0) || (match_pattern(fields[0], "^[0-1]$") == 0))
          {
            NS_DT1(vptr, NULL, DM_L1, MM_VARS, "Warning: PerformanceTraceLog-Mode value (%s) is not valid. "
                                               "Using default configuration.", fields[0]);
            NSTL1(NULL, NULL, "Warning: PerformanceTraceLog-Mode value (%s) is not valid. "
                              "Using default configuration.", fields[0]);
            NS_DUMP_WARNING("PerformanceTraceLog-Mode value (%s) is not valid. "
                              "Using default configuration.", fields[0]);
          }
          else
            mode = atoi(fields[0]);
          break; 
        default:
          NS_DT1(vptr, NULL, DM_L1, MM_VARS, "Warning: Number of arguments provided with 'PerformanceTraceLog' is incorrect. "
                                             "Using default configuration.");
          NSTL1(NULL, NULL, "Warning: Number of arguments provided with 'PerformanceTraceLog' is incorrect. "
                                             "Using default configuration.");
          NS_DUMP_WARNING("Number of arguments provided with 'PerformanceTraceLog' is incorrect. "
                                             "Using default configuration.");
      }
    }
    else
    {
       mode = runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.performance_trace_mode;
       perf_timeout = runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.performance_trace_timeout;
       memoryTrace = runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.performance_trace_memory_flag;
       screenshot = runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.performance_trace_screenshot_flag; 
       duration_level = runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.performance_trace_duration_level; 
    } 

    rbu_resp_attr->performance_trace_flag = mode;
    rbu_resp_attr->performance_trace_timeout = perf_timeout;

    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"performanceTrace\": {\"enable\": %s, \"timeout\": %d, \"memoryTrace\": %d, "
                                                                "\"screenshot\": %d, \"durationLevel\": %d, "
                                                                "\"filename\": \"performance_trace/%s\"}, ",
                                                                  (mode == 1) ? "true" : "false", perf_timeout, memoryTrace, screenshot,
                                                                  duration_level, rbu_resp_attr->performance_trace_filename);
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  }
  else
    rbu_resp_attr->performance_trace_flag = 0;

  /* Msg: Add Headers
      "headers": [{"name": "FirstHeader", "value": "FirstHeaderValue"}, {"name": "secondHeader", "value": "secondHeaderValue"}, ...],*/

  // Start of Headers -
  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"headers\": [");
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);

  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_header_flag != 0)
  {
    char *req_headers_buf = NULL;
    char *headers_fields[128];
    char *sep_fields[128];
    int req_headers_size = 0;
    int num_headers = 0;
    int header_count = 0;
    int count_fields = 0, i;

    //Send Header in Main as Well Inline url  
    NSDL2_RBU(vptr, NULL, "Make Req Headers from segment table");
    if(ns_rbu_make_req_headers(vptr, &req_headers_buf, &req_headers_size) < 0)
      return -1;
    NSDL2_RBU(vptr, NULL, "req_headers_buf = [%s], req_headers_size = %d", req_headers_buf, req_headers_size);

    //trim \r\n from end
    CLEAR_WHITE_SLASH_R_SLASH_N_END(req_headers_buf);
    NSDL2_RBU(vptr, NULL, "After clear, req_headers_buf = [%s]", req_headers_buf);

    num_headers = get_tokens_ex2(req_headers_buf, headers_fields, "\r\n", 128);
    NSDL2_RBU(vptr, NULL, "num_headers = %d", num_headers);

    for(i = 0; i < num_headers; i++)
    {
      NSDL2_RBU(vptr, NULL, "headers_fields[%d] = [%s]", i, headers_fields[i]);
      count_fields = get_tokens_ex2(headers_fields[i], sep_fields, ":", 128);
      if(count_fields == 2)
        ADD_HEADERS_IN_CHROME_MSG(sep_fields[0], sep_fields[1]);
    }
  } 

  /* End of Headers: Headers end - }], */
  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "], ");
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);

  /* Msg: Add "scrollPage": {"enable": true, "x": "1200", "y": "5500"} */
  if((scroll_page_x != 0) && (scroll_page_y != 0))
    scroll_enable = "true";

  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"scrollPage\": {\"enable\": \"%s\", \"x\": %lld, \"y\": %lld}, ",
                           scroll_enable, scroll_page_x, scroll_page_y);
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len); 

  /*Msg: Add Render Enable  */
  NSDL3_RBU(vptr, NULL, "Adding render enable = %d", runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_settings);
  if ((runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_settings >= 100) && !(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_capture_clip) ) 
  {
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"renderEnable\" : {\"value\": %d}" , runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_settings);
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  }  
  else 
  {
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "\"renderEnable\" : {\"value\": 0}");
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  }
  
  /*Msg: Add rbu_har_setting */
  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_har_setting_mode)
  {
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, ", \"rbuHARSetting\" :{\"enable\": %d, \"compression\": %d, "
                             "\"request\": %d, \"response\": %d, \"jsProcAndUri\": %d}",
                             runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_har_setting_mode, 
                             runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_har_setting_compression,
                             runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_har_setting_request, 
                             runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_har_setting_response,
                             runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_js_proc_tm_and_dt_uri);
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  }

  NSDL4_RBU(NULL, NULL, "Adding FRAMEWORK = %s in chrome click and script request.", attr[SPAFRAMEWORK]);
  /* Msg:  Added SPA Frame Work in Chrome MSG*/
  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, ", \"spaFrameWork\" :{\"frame\": \"%s\"}", 
                                        (attr[SPAFRAMEWORK] && !(strncasecmp(attr[SPAFRAMEWORK], "angular", 7)))?
                                        attr[SPAFRAMEWORK]:"NULL");
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);

  /*Msg : Domain List which will be ignored or not dump in HAR file */
  NSDL3_RBU(NULL, NULL, "Adding Domain Name = %s", runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_domain_ignore_list);
  if( strlen(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_domain_ignore_list) != 0)
  { 
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, ", \"domainIgnoreList\" : \"%s\""
                                                             , runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_domain_ignore_list);
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  }
   /*Msg : URls List which will be blocked  before going over Network */
  NSDL3_RBU(NULL, NULL, "Adding Urls to block = %s", runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_block_url_list);
  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_block_url_list != NULL)
  {
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, ", \"blockUrlList\" : \"%s\""
                                                             , runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_block_url_list);
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  }


  NSDL4_RBU(NULL, NULL, "Adding cookie msg = %s in chrome click and script request.", attr[COOKIES]);
  /* Msg: Cookie added in click & script in Chrome MSG */
  // "cookies" : [{"flag": 1, "pairs": [ {"name": "HPD", "value": "HPD1"}}]
  // Here flag: 1 is to add_cookie.
  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, ", \"cookies\" :[");
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);

  int cookie_count;
  int attr_flag = 0;
  if(attr[COOKIES])
  {
    attr_flag = 1;
    cookie_count = 0;
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "{\"flag\": 1, \"pairs\" :[");
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);

    ADD_COOKIES_PAIR_IN_CHROME_MSG(attr[COOKIES], ":");

    // ']}' is for "pairs and flag" end in cookies msg 
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "]}");
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  } 

  /* Msg: Cookie added in ns_web_url in Chrome MSG */
  /* Msg: Set Cookie header - "Cookie": "<string>", */
  char *req_cookies_buf = NULL;
  int req_cookies_size = 0;
  char *ptr = NULL;
 
  NSDL2_RBU(vptr, NULL, "Make Req Cookie form cookie table");
  ns_rbu_make_req_cookies(vptr, &req_cookies_buf, &req_cookies_size);
  NSDL2_RBU(vptr, NULL, "req_cookies_buf = [%s], req_cookies_size = %d", req_cookies_buf, req_cookies_size);
  if(req_cookies_buf != NULL)
  {
    cookie_count = 0;

    //trim \r\n from end
    CLEAR_WHITE_SLASH_R_SLASH_N_END(req_cookies_buf);
    ptr = strstr(req_cookies_buf, "Cookie:"); 
    if(ptr != NULL) 
    {
      ptr = req_cookies_buf + strlen("Cookie:");
      BRU_CLEAR_WHITE_SPACE(ptr);
    }
    NSDL2_RBU(vptr, NULL, "After clear , req_cookies_buf = [%s]", ptr);

    if(attr_flag == 1)
    {
      cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, ",");
      RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
    }

    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "{\"flag\": 1, \"pairs\" :[");
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);

    ADD_COOKIES_PAIR_IN_CHROME_MSG(ptr, "=");

    // ']}' is for "pairs and flag" end in cookies msg 
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "]}");
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len); 
  }
  // ']' is for "cookies" end in cookies msg
  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "]");
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);
  
  /* End MSG */
  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "}}\n");
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);

  // Null terminate cmd buf
  *cmd_buf_ptr = '\0';

  return (cmd_buf_ptr - req_buf); //MSG size
}
inline int check_resp_for_redirection(char *alert_resp, char *header_buf) 
{ 
  char *start_ptr = NULL;  
  char *end_ptr = NULL;  
  int len = 0; 
  
  NSDL1_RBU(NULL, NULL, "Method called"); 
  if((strstr(alert_resp, "302 Found")) != NULL) 
  { 
    if((start_ptr = strstr(alert_resp, "Location:")) != NULL) 
    { 
      start_ptr +=10;    
      NSDL1_RBU(NULL, NULL, "start_ptr = [%s]", start_ptr); 
      if((end_ptr = strstr(start_ptr, "\r\n")) != NULL) 
      { 
        NSDL1_RBU(NULL, NULL, "end_ptr = [%s]", end_ptr); 
        len = end_ptr - start_ptr; 
        start_ptr[len] = '\0'; 
        NSDL1_RBU(NULL, NULL, "Redirected resp which is going to hit. = [%s]", start_ptr); 
      } 
      strcpy(header_buf, start_ptr);  
    } 
    return 0; 
  } 
  else 
    return -1; 
}
/*---------------------------------------------------------------------*/
/*--- OpenConnection - create socket and connect to server.         ---*/
/*---------------------------------------------------------------------*/
int OpenConnection(const char *hostname, int port)
{   
  int sockfd = -1;
  struct hostent *host;
  struct sockaddr_in addr;
  int herr;

  NSDL2_RBU(NULL, NULL, "Method called, hostname = [%s], port = [%d]", hostname, port);
  host = nslb_gethostbyname2(hostname, AF_INET, &herr);
  if(!host)
  {
    perror(hostname);
    abort();
  }
  if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
  {
    NSDL2_RBU(NULL, NULL, "Error: Unable to create socket");
    return -1;
  } 

  bzero(&addr, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = *(long*)(host->h_addr);
  if(connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
  {
    NSDL2_RBU(NULL, NULL, "Error: Unable to connect, while creating ssl connection on sockfd = [%d]", sockfd);
    close(sockfd);
    return -1;
  }
  return sockfd;
}

/*---------------------------------------------------------------------*/
/*--- InitCTX - initialize the SSL engine.                          ---*/
/*---------------------------------------------------------------------*/
#if OPENSSL_VERSION_NUMBER < 0x10100000L
SSL_CTX* InitCTX(void)
{   
  const SSL_METHOD *method;
  SSL_CTX *ctx;

  OpenSSL_add_all_algorithms();               /* Load cryptos, et.al. */
  SSL_load_error_strings();                   /* Bring in and register error messages */
  SSL_library_init();
  method = SSLv23_client_method();            /* Create new client-method instance */
  ctx = SSL_CTX_new(method);                  /* Create new context */
  if(ctx == NULL)
  {
    NSDL1_RBU(NULL, NULL, "Error: Unable create new context");
    abort();
  }
  return ctx;
}
#endif

#if OPENSSL_VERSION_NUMBER >= 0x10100000L
SSL_CTX *InitCTX(void)
{
  //SSL_METHOD *meth;
  SSL_CTX *ctx;

  //Global system initialization
  //SSL_library_init();
  //OPENSSL_init_ssl();
  //SSL_load_error_strings();

  // An error write context
  //bio_err=BIO_new_fp(stderr,BIO_NOCLOSE);

  // Create our method
  //meth=(SSL_METHOD *)TLS_method();

  // Create our context
  ctx=SSL_CTX_new(TLS_method());

  // Load our keys and certificates
  //if(!(SSL_CTX_use_certificate_file(ctx,keyfile,SSL_FILETYPE_PEM)))
  //berr_exit("Couldn't read certificate file");

  //if(!(SSL_CTX_use_PrivateKey_file(ctx,keyfile,SSL_FILETYPE_PEM)))
  //berr_exit("Couldn't read key file");

  return ctx;
}
#endif



/*---------------------------------------------------------------------*/
/*--- ShowCerts - print out the certificates.                       ---*/
/*---------------------------------------------------------------------*/
void ShowCerts(SSL* ssl)
{   
  X509 *cert;
  char *line;

  cert = SSL_get_peer_certificate(ssl);       /* get the server's certificate */
  if(cert != NULL)
  {
    NSDL1_RBU(NULL, NULL, "Server certificates:");
    line = X509_NAME_oneline(X509_get_subject_name(cert), 0, 0);
    NSDL1_RBU(NULL, NULL, "Subject: %s", line);
    free(line);                                                     /* free the malloc'ed string */
    line = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0);
    NSDL1_RBU(NULL, NULL, "Issuer: %s", line);
    free(line);                                                     /* free the malloc'ed string */
    X509_free(cert);                                        /* free the malloc'ed certificate copy */
  }
  else
    NSDL1_RBU(NULL, NULL, "No certificates.");
}

inline void make_ssl_connection(char *msg, char *host) 
{ 
  SSL_CTX *ctx; 
  int server; 
  SSL *ssl; 
  char buf[1024]; 
  int bytes; 
  char *hostname = NULL, *portnum = NULL; 
  char *fields[125 + 1]; 
  int total_fields = 0; 
  int i; 
  
  NSDL1_RBU(NULL, NULL, "Method called, msg = [%s], host = [%s]", msg, host); 
  total_fields = get_tokens(host, fields, ":", 5); 
  
  NSDL1_RBU(NULL, NULL, "total_fields = [%d]", total_fields); 
  for(i=0;i<total_fields;i++) 
  { 
    NSDL1_RBU(NULL, NULL, "fields = [%s]", fields[i]); 
  } 
  
  hostname=fields[0]; 
  portnum=fields[1];  
 
  NSDL1_RBU(NULL, NULL, "hostname = [%s], portnum = [%s]", hostname, portnum); 

  if(portnum == NULL)
    portnum = "443";
  ctx = InitCTX(); 
  if((server = OpenConnection(hostname, atoi(portnum))) == -1)
  {
    NSDL2_RBU(NULL, NULL, "Error: In OpenConnection");
    return;
  }
  ssl = SSL_new(ctx);            
  SSL_set_fd(ssl, server);       
  if (SSL_connect(ssl) == -1 )  
  { 
    NSDL1_RBU(NULL, NULL, "Error: Unable to connect ssl"); 
    return;
  }
  else 
  { 
    NSDL1_RBU(NULL, NULL, "Connected with %s encryption\n", SSL_get_cipher(ssl)); 
    ShowCerts(ssl);                          
    ERR_clear_error();
    SSL_write(ssl, msg, strlen(msg));       
    bytes = SSL_read(ssl, buf, sizeof(buf)); 
    buf[bytes] = 0; 
    NSDL1_RBU(NULL, NULL, "Received: \"%s\"\n", buf); 
    SSL_free(ssl);                          
  } 
  close(server);                         
  SSL_CTX_free(ctx);                    
}
static inline void rbu_send_alerts(VUser *vptr, char *tomcat_ip, char *msg) 
{ 
  char alert_buf[RBU_MAX_USAGE_LENGTH] = "";
  char alert_resp[1024 + 1] = "";
  int alert_fd = -1;
  char err_msg[1024 + 1] = "";

  NSDL1_RBU(vptr, NULL, "Method called, tomcat_ip = [%s], msg = [%s]", tomcat_ip, msg);  

  if(!strcmp(master_ip, "")) 
  { 
    snprintf(alert_buf, RBU_MAX_USAGE_LENGTH, "GET /DashboardServer/web/AlertDataService/generateCustomAlert?testRun=%d&message=%s HTTP/1.1\r\nHost: %s:%d\r\n\r\n", \
                        testidx, msg, tomcat_ip, g_tomcat_port); 
  } 
  else 
  { 
    snprintf(alert_buf, RBU_MAX_USAGE_LENGTH, "GET /DashboardServer/web/AlertDataService/generateCustomAlert?testRun=%s&message=%s HTTP/1.1\r\nHost: %s:%d\r\n\r\n", \
                        getenv("NS_CONTROLLER_TEST_RUN"), msg, tomcat_ip, g_tomcat_port); 
  } 
  
  NSDL1_RBU(vptr, NULL, "alert_buf = %s", alert_buf); 
  NSTL1(NULL, NULL, "alert_buf = %s", alert_buf);  
  
  alert_fd = nslb_tcp_client_ex(tomcat_ip, g_tomcat_port, TCP_CLIENT_CON_TIMEOUT, err_msg);
  NSDL1_RBU(vptr, NULL, "alert_fd = %d", alert_fd); 
  if(alert_fd > 0) 
  { 
    if(send(alert_fd, alert_buf, sizeof(alert_buf), 0) != sizeof(alert_buf))  
    { 
      NSDL1_RBU(vptr, NULL, "Failed in sending message, error = %s", nslb_strerror(errno)); 
      NSTL1(NULL, NULL, "Failed in sending message, error = %s", nslb_strerror(errno)); 
      close(alert_fd); 
      return; 
    } 
    
    if(read(alert_fd, alert_resp, sizeof(alert_resp)) <= 0)  
    { 
      NSDL1_RBU(vptr, NULL, "Failed alert_resp = %s, error = %s", alert_resp, nslb_strerror(errno)); 
      NSTL1(NULL, NULL, "Failed alert_resp = %s, error = %s", alert_resp, nslb_strerror(errno)); 
      close(alert_fd);
      return; 
    }  
    else 
    { 
      NSDL1_RBU(vptr, NULL, "alert_resp = %s", alert_resp); 
      NSTL1(NULL, NULL, "alert_resp = %s", alert_resp); 

      char header_buf[1024 + 1]; 
      int ret = -1;
 
      ret = check_resp_for_redirection(alert_resp, header_buf); 
      if(ret == 0) 
      { 
        char *fields[1024]; 
        char is_ssl = 0; 
        int i; 
        
        if(strstr(header_buf, "https") != NULL) 
          is_ssl= 1; 
        
        int count = get_tokens(header_buf, fields, "/", 3); 
        
        for(i=0; i < count; i++) 
          NSDL1_RBU(vptr, NULL, "fields[%d] = %s", i, fields[i]); 
      
        //Resolved Bug 29993 - ScriptFailure-Alert | Alert mail is not generating on script failure as in alert mail request, 
        //                     TR number is passing as null.  
        if(!strcmp(master_ip, ""))
        {
          snprintf(alert_buf, RBU_MAX_USAGE_LENGTH, "GET /DashboardServer/web/AlertDataService/generateCustomAlert?testRun=%d&message=%s HTTP/1.1\r\nHost: %s\r\n\r\n",
                              testidx, msg, fields[1]);
        }
        else
        {
          snprintf(alert_buf, RBU_MAX_USAGE_LENGTH, "GET /DashboardServer/web/AlertDataService/generateCustomAlert?testRun=%s&message=%s HTTP/1.1\r\nHost: %s\r\n\r\n",                              getenv("NS_CONTROLLER_TEST_RUN"), msg, fields[1]);
        } 
        
        NSDL1_RBU(NULL, NULL, "alert_buf = [%s]", alert_buf); 
       
        //If the connection is not https.
        if(!is_ssl) 
        { 
          if(send(alert_fd, alert_buf, sizeof(alert_buf), 0) != sizeof(alert_buf)) 
          { 
            NSDL1_RBU(vptr, NULL, "Failed in sending message, error =.\n"); 
            close(alert_fd); 
            return; 
          } 
          
          if(read(alert_fd, alert_resp, sizeof(alert_resp)) <= 0) 
          { 
            NSDL1_RBU(vptr, NULL, "Failed alert_resp = %s, error.\n", alert_resp); 
            close(alert_fd); 
            return; 
          } 
          NSDL1_RBU(vptr, NULL, "alert_resp = \n%s", alert_resp);
        } 
        else 
        {
          make_ssl_connection(alert_buf, fields[1]); 
        }
      } 
      else 
      { 
        NSDL1_RBU(vptr, NULL, "No redirection found"); 
        return; 
      }
    }
    close(alert_fd); 
  } 
  else 
  { 
    NSDL1_RBU(vptr, NULL, "Error in connecting alert_fd = %d", alert_fd); 
    NSTL1(NULL, NULL, "Error in connecting alert_fd = %d", alert_fd); 
  } 
}

//Bug-85245
void make_msg_and_send_alert(VUser *vptr, char *msg)
{
  NSDL2_RBU(vptr, NULL, "Method called, msg = %s", msg);
  NSDL1_RBU(vptr, NULL, "vptr->sess_ptr->rbu_alert_policy = [%s]", vptr->sess_ptr->rbu_alert_policy?vptr->sess_ptr->rbu_alert_policy:"NULL");

  if(global_settings->rbu_alert_policy_mode)
    ns_send_alert2(ALERT_MAJOR, vptr->sess_ptr->rbu_alert_policy, msg);
  else
    ns_send_alert2(ALERT_MAJOR, NULL, msg);

  return;
}

//JS Checkpoint status handling
//checkpoint_resp format: name=checkpoint_color, value=#f1f2f1, status=Fail, abortTest=1, errorMessage=Content Validation Fail
#define PARSE_CHECKPOINT_RESP(checkpoint_resp, element) \
  ptr1 = strstr(checkpoint_resp, element); \
  if(ptr1){ \
    ptr1 = ptr1 + strlen(element) + 1; \
    ptr2 = strstr(ptr1, ", "); \
    checkpoint_resp = ptr2; \
    len = ptr2 - ptr1; \
    memset(value, 0, strlen(value));; \
    strncpy(value, ptr1, len); \
  }

int process_js_checkpoint(VUser *vptr, char *checkpoint_resp)
{
  RBU_RespAttr *rbu_resp_attr = vptr->httpData->rbu_resp_attr;
  char cp_name[256] = "";
  char cp_status[512] = "";
  char cp_abortTest[512] = "";
  char cp_msg[512 + 1] = "";
  char *ptr1 = checkpoint_resp;
  char *ptr2 = NULL;
  int len;
  char value[1024] = "";
  char element[32] = "name";
 
  NSDL2_RBU(vptr, NULL, "Method called, vptr = %p, checkpoint_resp = %s", vptr, checkpoint_resp);

  PARSE_CHECKPOINT_RESP(checkpoint_resp, element);
  strcpy(cp_name, value);

  strcpy(element, "status");
  PARSE_CHECKPOINT_RESP(checkpoint_resp, element);
  strcpy(cp_status, value);
  
  if(!strncmp(cp_status, "Fail", 4))
  {
    ContinueOnPageErrorTableEntry_Shr *ptr;
    ptr = (ContinueOnPageErrorTableEntry_Shr *)runprof_table_shr_mem[vptr->group_num].continue_onpage_error_table[vptr->cur_page->page_number];

    strcpy(element, "abortTest");
    PARSE_CHECKPOINT_RESP(checkpoint_resp, element);
    strcpy(cp_abortTest, value);

    checkpoint_resp = checkpoint_resp + 2;          //+2 for comma(,) and space
    strncpy(cp_msg, checkpoint_resp, 512);

    NSDL2_RBU(vptr, NULL, "JS Checkpoint '%s' failed, %s.", cp_name, cp_msg);

    NS_DT1(vptr, NULL, DM_L1, MM_VARS, "Error:JS Checkpoint Failed, Page = %s, Checkpoint = '%s', Status = Fail.",
                                       page_table_shr_mem[vptr->next_pg_id - 1].page_name, cp_name);

    NS_SEL(vptr, NULL, DM_L1, MM_SCHEDULE, "SCRIPT_EXECUTION_LOG: Script=%s; Status=Warning; flowName=%s; Page=%s; Session=%s; "
                 "MSG=JS Checkpoint '%s' failed, Reason: %s",
                 get_sess_name_with_proj_subproj_int(vptr->sess_ptr->sess_name, vptr->sess_ptr->sess_id, "/"), vptr->cur_page->flow_name,
                 page_table_shr_mem[vptr->next_pg_id - 1].page_name, vptr->sess_ptr->sess_name, cp_name, cp_msg);

    snprintf(script_execution_fail_msg, MAX_SCRIPT_EXECUTION_LOG_LENGTH, "JS Checkpoint Failed, Page = %s, Checkpoint = '%s', "
                                        "Status = Fail, %s.", page_table_shr_mem[vptr->next_pg_id - 1].page_name, cp_name, cp_msg);

    snprintf(rbu_resp_attr->access_log_msg, RBU_MAX_ACC_LOG_LENGTH, "Error : JS Checkpoint '%s' failed, %s.", cp_name, cp_msg);

    char alert_msg[RBU_MAX_2K_LENGTH] = "";

    if(send_events_to_master == 1)
      snprintf(alert_msg, RBU_MAX_2K_LENGTH, "ContentValidationError: JS Checkpoint '%s' of Script '%s' from Generator '%s' is failed, %s",
               cp_name, vptr->sess_ptr->sess_name, global_settings->event_generating_host, cp_msg);
    else
      snprintf(alert_msg, RBU_MAX_2K_LENGTH, "ContentValidationError: JS Checkpoint '%s' of Script '%s' is failed, %s",
               cp_name, vptr->sess_ptr->sess_name, cp_msg);
      
    make_msg_and_send_alert(vptr, alert_msg);
    
    if(!(strcmp(cp_abortTest, "1")))
    {
      ptr->continue_error_value = 0;
    }
    else
    {
      ptr->continue_error_value = 1;
      NSTL1(vptr, NULL, "JS Checkpoint %s failed, but abortTest is 0, so continuing...", cp_name);
    }
    return -1;
  }
  else
  {
    NSDL2_RBU(vptr, NULL, "JS Checkpoint '%s' Passed.", cp_name);
    NS_DT1(vptr, NULL, DM_L1, MM_VARS, "JS Checkpoint '%s' Pass.", cp_name);
  }
  return 0;
}

int perform_task_after_rbu_failure(int ret, VUser *vptr)
{
  RBU_RespAttr *rbu_resp_attr = vptr->httpData->rbu_resp_attr;

  ContinueOnPageErrorTableEntry_Shr *ptr;
  ptr = (ContinueOnPageErrorTableEntry_Shr *)runprof_table_shr_mem[vptr->group_num].continue_onpage_error_table[vptr->cur_page->page_number];

  if(ret == -1)
  { 
    NSDL2_RBU(vptr, NULL, "CA: Command execution failed, hence setting page_status, sess_status and req_ok to MiscErr.");
    RBU_SET_PAGE_STATUS_ON_ERR(vptr, rbu_resp_attr, NS_REQUEST_ERRMISC, ptr);

    //Bug 88813
    NSDL2_RBU(vptr, NULL, "num_retry_on_page_failure - %d and retry_count_on_abort - %d",
                             runprof_table_shr_mem[vptr->group_num].gset.num_retry_on_page_failure, vptr->retry_count_on_abort);

    if(runprof_table_shr_mem[vptr->group_num].gset.num_retry_on_page_failure > vptr->retry_count_on_abort)
    { 
      strcat(rbu_resp_attr->access_log_msg, ". Hence, Session Aborted and going to Retry");
      return -2;
    }
  }
  else
  { 
    NSDL2_RBU(vptr, NULL, "CA: JS Checkpoint failed, hence setting page_status, req_ok to CVFail.");
    RBU_SET_PAGE_STATUS_ON_ERR(vptr, rbu_resp_attr, NS_REQUEST_CV_FAILURE, ptr);
  }

  //On Failure: Access Log
  ns_rbu_log_access_log(vptr, rbu_resp_attr, RBU_ACC_LOG_DUMP_LOG);

  //Add vptr->operation to VUT_RBU_WEB_URL_END and then switch to ns_rbu_handle_web_url_end()
  vut_add_task(vptr, VUT_RBU_WEB_URL_END);
  return 0;

}

int handle_rbu_failure_con_to_brower(int ret, VUser *vptr, int page_load_wait_time)
{
  time_t start_time = time(NULL);
  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_screen_shot &&
       NS_IF_PAGE_DUMP_ENABLE && runprof_table_shr_mem[vptr->group_num].gset.trace_level > TRACE_ONLY_REQ_RESP)
  { 
    move_snapshot_to_tr(vptr, page_load_wait_time, start_time, ret);
    return 0;
  }
  perform_task_after_rbu_failure(ret, vptr);
  return 0;
}

int rbu_make_click_con_to_browser_v2(VUser *vptr, int cav_fd, time_t start_time, time_t con_start_time)
{

  RBU_RespAttr *rbu_resp_attr = vptr->httpData->rbu_resp_attr;

  //Setting ns_logs_file_path, and rbu_logs_file_path 
  RBU_NS_LOGS_PATH
 
  //Get Buffer for processing.
  //Currently we are using same buffer which was allocated for ns_web_url call. This buffer will be of 10K.
  //Check if this much is sufficient or not.
  //If this is not allocated then we need to malloc it.
  if(!vptr->httpData->rbu_resp_attr->firefox_cmd_buf) {
    
    MY_MALLOC(vptr->httpData->rbu_resp_attr->firefox_cmd_buf, RBU_MAX_CMD_LENGTH + 1,  "vptr->httpData->rbu_resp_attr->firefox_cmd_buf", 1);
  }

  char *cav_request = vptr->httpData->rbu_resp_attr->firefox_cmd_buf;
  int cav_request_size = 0;
 
  ClickActionTableEntry_Shr *ca;
  char *att[NUM_ATTRIBUTE_TYPES];
  /* Find the click action table entry for ca_idx */
  ca = &(clickaction_table_shr_mem[vptr->httpData->clickaction_id]);
  memset(att, 0, sizeof(att));
  if(read_attributes_array_from_ca_table(vptr, att, ca) < 0)
    return -1; // SHJ: Do error handling here

  //screen_shot name
  if(rbu_resp_attr->page_screen_shot_file == NULL)
  {
    MY_MALLOC(rbu_resp_attr->page_screen_shot_file, RBU_MAX_FILE_LENGTH + 1,
                               "vptr->httpData->rbu_resp_attr->page_screen_shot_file", 1);
  }
  else
    memset(rbu_resp_attr->page_screen_shot_file, 0, RBU_MAX_FILE_LENGTH + 1);

  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_screen_shot &&
        NS_IF_PAGE_DUMP_ENABLE && runprof_table_shr_mem[vptr->group_num].gset.trace_level > TRACE_ONLY_REQ_RESP)
  {
    if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.browser_mode == RBU_BM_FIREFOX)
    {
      snprintf(rbu_resp_attr->page_screen_shot_file, RBU_MAX_FILE_LENGTH - 1,
                    "%s/logs/%s/screen_shot/page_screen_shot_%hd_%u_%u_%d_0_%d_%d_%d_0",
                    g_ns_wdir, rbu_logs_file_path, child_idx, vptr->user_index, vptr->sess_inst,
                    vptr->page_instance, vptr->group_num, GET_SESS_ID_BY_NAME(vptr), GET_PAGE_ID_BY_NAME(vptr));
    }
    else if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.browser_mode == RBU_BM_CHROME)
    {
      snprintf(rbu_resp_attr->page_screen_shot_file, RBU_MAX_FILE_LENGTH,
                        "%s/%s/screen_shot/page_screen_shot_%hd_%u_%u_%d_0_%d_%d_%d_0",
                        rbu_resp_attr->har_log_path, rbu_resp_attr->profile, child_idx, vptr->user_index, vptr->sess_inst,
                        vptr->page_instance, vptr->group_num, GET_SESS_ID_BY_NAME(vptr), GET_PAGE_ID_BY_NAME(vptr));
    }

    NSDL2_RBU(vptr, NULL, "Click & Script screen_shot_file_name = %s", rbu_resp_attr->page_screen_shot_file);
  }
  else
  {
    strcpy(rbu_resp_attr->page_screen_shot_file, "NA");
  }

  unsigned short har_timeout;
  HAR_TIMEOUT
  // We need to send message to listner before 5 sec to timeout because this time out is commn to wait for har file and listener
  int page_load_wait_time = att[PAGELOADWAITTIME] ? atoi(att[PAGELOADWAITTIME]) : har_timeout;
  int timeout = (page_load_wait_time * 1000) - 5000;

  int browser_version = 0;
  nslb_atoi(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.brwsr_vrsn, &browser_version);
  if(browser_version >= 78)
    timeout = (page_load_wait_time * 1000) - 10000;

  NSDL2_RBU(vptr, NULL, "profile = [%s], page_name = [%s], page_load_wait_time = %d",
                       rbu_resp_attr->profile[0]?rbu_resp_attr->profile:"NULL", vptr->cur_page->page_name[0]?vptr->cur_page->page_name:"NULL",
                       page_load_wait_time);

  //SCROLLPAGE
  vptr->first_page_url->proto.http.rbu_param.scroll_page_x = att[SCROLLPAGEX]?atoi(att[SCROLLPAGEX]):0;
  vptr->first_page_url->proto.http.rbu_param.scroll_page_y = att[SCROLLPAGEY]?atoi(att[SCROLLPAGEY]):0;

  vptr->first_page_url->proto.http.rbu_param.timeout_for_next_req = att[WaitForNextReq]?atoi(att[WaitForNextReq]):0;
  vptr->first_page_url->proto.http.rbu_param.phase_interval_for_page_load = att[PhaseInterval]?atoi(att[PhaseInterval]):0;

  int scroll_page_x = att[SCROLLPAGEX] ? atoi(att[SCROLLPAGEX]) : vptr->first_page_url->proto.http.rbu_param.scroll_page_x;
  int scroll_page_y = att[SCROLLPAGEY] ? atoi(att[SCROLLPAGEY]) : vptr->first_page_url->proto.http.rbu_param.scroll_page_y;

  NSDL2_RBU(vptr, NULL, "scroll_page_x = %d, scroll_page_y = %d",scroll_page_x, scroll_page_y);

  //for ns_click har flag will be on.
  //When har file not enable for graph then is_incomplete_har_file will be two, 
  //means no data will get dumped in Avg Structure of RBU Page Stat
  int har_flag = 0;
  if(!strncasecmp(att[APINAME], "ns_link", 7) || !strncasecmp(att[APINAME], "ns_scroll", 9) ||
     !strncasecmp(att[APINAME], "ns_button", 9) || !strncasecmp(att[APINAME], "ns_span", 7) || !strncasecmp(att[APINAME], "ns_map_area", 11)||
     !strncasecmp(att[APINAME], "ns_mouse_hover", 14) || !strncasecmp(att[APINAME], "ns_mouse_out", 12) ||
     !strncasecmp(att[APINAME], "ns_key_event", 12))
    har_flag = 1;

  /*APIs such as ns_edit() for which HAR is disabled by default, will not be enabled through Keyword G_RBU_RELOAD_HAR. 
    HAR will be enabled for them through only attribute ReloadHar/HARFLAG(i.e., "ReloadHar=1"). */
  if(har_flag == 1)
    har_flag = runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.reload_har;

  NSDL3_RBU(vptr, NULL, "reload_har_reload_flag for group[%d] = [%d]", vptr->group_num,
            runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.reload_har);

  if(att[HARFLAG]) {
    if(!strcmp(att[HARFLAG], "1")) har_flag = 1;
    else if(!strcmp(att[HARFLAG], "101")) har_flag = 101; //for har_flag ON and merge 3 or more har files is also ON
    else if(!strcmp(att[HARFLAG], "100")) har_flag = 100; //for har_flag OFF and merge 3 or more har files ON
    else har_flag = 0;
  }

  //Add capture clip name

  if(rbu_resp_attr->page_capture_clip_file == NULL)
  {
    MY_MALLOC(rbu_resp_attr->page_capture_clip_file, RBU_MAX_FILE_LENGTH + 1,
                             "vptr->httpData->rbu_resp_attr->page_capture_clip_file", 1);
  }
  else
    memset(rbu_resp_attr->page_capture_clip_file, 0, RBU_MAX_FILE_LENGTH + 1);

  if((har_flag == 1 || har_flag == 101) && (runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_capture_clip))
  {
    //TODO: we have to remove clip link and make clips diretly on log/TRxx path
    if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.browser_mode == RBU_BM_FIREFOX ||
        runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.browser_mode == RBU_BM_CHROME)
    {
      snprintf(rbu_resp_attr->page_capture_clip_file, RBU_MAX_FILE_LENGTH - 1,
                    "%s/%s/clips/video_clip_%hd_%u_%u_%d_0_%d_%d_%d_0_",
                    rbu_resp_attr->har_log_path, rbu_resp_attr->profile, child_idx, vptr->user_index, vptr->sess_inst,
                    vptr->page_instance, vptr->group_num, GET_SESS_ID_BY_NAME(vptr), GET_PAGE_ID_BY_NAME(vptr));

    }
    else
      strcpy(rbu_resp_attr->page_capture_clip_file, "NA");
  }
  else
  {
    strcpy(rbu_resp_attr->page_capture_clip_file, "NA");
  }

  NSDL2_RBU(vptr, NULL, "BrowserMode = %d, CaptureClip = %d, har_flag = %d, "
                        "page_capture_clip_file = %p, snapshot = %p",
                        runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.browser_mode,
                        runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_capture_clip, har_flag,
                        rbu_resp_attr->page_capture_clip_file, rbu_resp_attr->page_screen_shot_file);

  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.browser_mode == RBU_BM_FIREFOX)
    cav_request_size = ns_rbu_make_firefox_ca_req(vptr, cav_request, att, rbu_resp_attr->page_screen_shot_file, timeout, har_flag, rbu_resp_attr->page_capture_clip_file);
  else if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.browser_mode == RBU_BM_CHROME)
    cav_request_size = ns_rbu_make_chrome_ca_req(vptr, cav_request, att, rbu_resp_attr->page_screen_shot_file, timeout, har_flag, rbu_resp_attr->page_capture_clip_file, scroll_page_x, scroll_page_y);

  if(cav_request_size == -1)
  {
    handle_rbu_failure_con_to_brower(-1, vptr, page_load_wait_time);
    HANDLE_RBU_PAGE_FAILURE_WITH_CLICK(-1)
    return -1;
  }

  char *tok, *len, *checkpoint_resp = NULL;
  int max_wait = 0;
  char write_read_fail;
  long long elaps_time, con_elaps_time;
  time_t end_time, con_end_time;
  char cav_resp[RBU_MAX_BUF_LENGTH + 1] = "";
  int interval = 0;
  if(att[WaitUntil])
  {
    if((tok = index(att[WaitUntil], ':')) != NULL)
    {
      tok++;
      if(ns_is_numeric(tok))
        nslb_atoi(tok, &max_wait);
    }
    NSDL2_RBU(vptr, NULL, "attr[WaitUntil] = %s, max_wait = %d", att[WaitUntil], max_wait);
  }
  else if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.wait_until_timeout > 0)
  {
    max_wait = runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.wait_until_timeout;
    NSDL2_RBU(vptr, NULL, "max_wait = %d", max_wait);
  }
  while(1)
  {
    write_read_fail = 0;
    //Send message to cavisson service.
    if(write(cav_fd, cav_request, cav_request_size) != cav_request_size)
    {
      NSDL2_RBU(vptr, NULL, "Error: (ns_rbu_send_msg_to_browser), Failed to send complete request to cav service.\n");
      write_read_fail = 1; //fail to write
    }
    else
    {
      //Wait for response.
      if(read(cav_fd, cav_resp, RBU_MAX_BUF_LENGTH) <= 0)
      {
        NSDL2_RBU(vptr, NULL, "Error: (ns_rbu_send_msg_to_browser), Failed to read response from cav service. error = %s\n", nslb_strerror(errno));
        write_read_fail = 2; //fail to read
      }
    }
   
    con_end_time = time(NULL);
    end_time = time(NULL);
    close(cav_fd);
    if(write_read_fail == 0)
    {
      if(!strncasecmp(cav_resp, "success", 7))
        break;
    }
    elaps_time = end_time - start_time;
    if(elaps_time >= max_wait)  //Main Timer
    {
      NSDL1_RBU(vptr, NULL, "start_time = %lld, end_time = %lld, elaps_time = %d", start_time, end_time, elaps_time);
      break;
    }

    con_elaps_time = con_end_time - con_start_time;
    NSDL4_RBU(vptr, NULL, "con_start_time = %lld, con_end_time = %lld, con_elaps_time = %d",
                           con_start_time, con_end_time, con_elaps_time);
    if(con_elaps_time < TCP_CLIENT_CON_TIMEOUT)  //Connection/Internal Timer
    {
      interval = (TCP_CLIENT_CON_TIMEOUT - con_elaps_time) * 1000;  //convert into ms
      interval = (interval > 1000) ? 1000 : interval;
      VUSER_SLEEP_MAKE_CLICK_CON_TO_BROWSER_V2(vptr, interval);
    }
  }
  char *acc_log_resp_msg = "";
  int flag = 1; 
  if(write_read_fail == 1)
  {
    NSTL1_OUT(NULL, NULL, "Error: (ns_rbu_send_msg_to_browser), Failed to send complete request to cav service.\n");
    strncpy(rbu_resp_attr->access_log_msg, "Error: Failed to send complete request to extension", RBU_MAX_ACC_LOG_LENGTH);
    strncpy(script_execution_fail_msg, "Internal Error:Netstorm failed to send complete request to browser.", MAX_SCRIPT_EXECUTION_LOG_LENGTH);
    handle_rbu_failure_con_to_brower(-1, vptr, page_load_wait_time);
    HANDLE_RBU_PAGE_FAILURE_WITH_CLICK(-1)
    return -1;
  }
  else if(write_read_fail == 2)
  {
    NSTL1_OUT(NULL, NULL, "Error: (ns_rbu_send_msg_to_browser), Failed to read response from cav service. error = %s\n", nslb_strerror(errno));
    strncpy(rbu_resp_attr->access_log_msg, "Error: Failed to read response from extension", RBU_MAX_ACC_LOG_LENGTH);
    snprintf(script_execution_fail_msg, MAX_SCRIPT_EXECUTION_LOG_LENGTH, "Internal Error:Netstorm failed to read response from browser. Error: %s", nslb_strerror(errno));
    handle_rbu_failure_con_to_brower(-1, vptr, page_load_wait_time);
    HANDLE_RBU_PAGE_FAILURE_WITH_CLICK(-1)
    return -1;
  }

  if((len = strstr(cav_resp, ";1007:"))){
    len = len + strlen(";1007:");
    NSDL3_RBU(vptr, NULL, "len = %s ", len);

    if((tok = strtok(len, ";")) != NULL)
      vptr->httpData->rbu_resp_attr->num_domelement = atoi(len);

    NSDL3_RBU(vptr, NULL, "num_domelement = %d ", vptr->httpData->rbu_resp_attr->num_domelement);
  }
  else if((checkpoint_resp = strstr(cav_resp, ";1009:"))){
    checkpoint_resp = checkpoint_resp + strlen(";1009:");
    checkpoint_resp = ns_strtok(checkpoint_resp, ";");

    if(process_js_checkpoint(vptr, checkpoint_resp) == -1) {
      handle_rbu_failure_con_to_brower(-3, vptr, page_load_wait_time);
      HANDLE_RBU_PAGE_FAILURE_WITH_CLICK(-1)
      return -3; //For CVFail returning -3
    }
    else
      //return 0;
      flag = 0;
  }
  else if((len = strstr(cav_resp, ";2001:"))){
    len = len + strlen(";2001:");
    if((tok = strtok(len, ";")) != NULL){
      NSDL2_RBU(vptr, NULL, "JS code executed successfully. Output = %s", tok);
      if(ns_is_numeric(tok))
        vptr->httpData->rbu_resp_attr->executed_js_result = atoi(tok);
      else
        vptr->httpData->rbu_resp_attr->executed_js_result = 1;
    }
  }
  if(flag == 1)
  {
    //Now check the response.
    if(!strncasecmp(cav_resp, "success", 7))  
    {
      NSDL3_RBU(vptr, NULL, "Method exiting Successfully");
    }
    else 
    {
      char *ptr;
      char alert_msg[RBU_MAX_USAGE_LENGTH] = "";
      int write_idx = 0;

      if((ptr = strstr(cav_resp, "MSG_END")) != NULL)
        *ptr='\0';
      NSDL2_RBU(vptr, NULL, "Command execution failed, msg = %s", cav_resp);
      NSTL1(vptr, NULL, "Command execution failed, msg = %s, Script = %s, Flow = %s, Page = %s",
                cav_resp, vptr->sess_ptr->sess_name, vptr->cur_page->flow_name, vptr->cur_page->page_name);

      acc_log_resp_msg = cav_resp + 6; //point to error msg, so incerese by lenth of 'error_'
      NS_DT1(vptr, NULL, DM_L1, MM_VARS, "Error: %s, Page '%s' execution failed, Script = %s, FlowFile = %s", acc_log_resp_msg,
                 vptr->cur_page->page_name, vptr->sess_ptr->sess_name, vptr->cur_page->flow_name);

      snprintf(script_execution_fail_msg, MAX_SCRIPT_EXECUTION_LOG_LENGTH, "%s, URL=%s, ElementType=%s", acc_log_resp_msg,
              (att[URL]) ? att[URL] : rbu_resp_attr->url,
              (att[TAG]) ? att[TAG] : "NA");
      snprintf(rbu_resp_attr->access_log_msg, RBU_MAX_ACC_LOG_LENGTH, "Error : %s", acc_log_resp_msg);

      if((ptr = strstr(cav_resp, "error_Failed to find element on page with")) != NULL)
      { 
        write_idx = snprintf(alert_msg, RBU_MAX_USAGE_LENGTH, "Failed to find element on Page '%s' of Script '%s' ",
                     vptr->cur_page->page_name, vptr->sess_ptr->sess_name);
        if(send_events_to_master == 1)
          write_idx += snprintf(alert_msg + write_idx, RBU_MAX_USAGE_LENGTH - write_idx, "on Generator '%s' ",
                              global_settings->event_generating_host);
        snprintf(alert_msg + write_idx, RBU_MAX_USAGE_LENGTH - write_idx, "with element %s",
               cav_resp + strlen("error_Failed to find element on page with "));

        make_msg_and_send_alert(vptr, alert_msg);
      }
      else if((ptr = strstr(cav_resp, "error_Failed to find iframe on page with")) != NULL)
      {
        write_idx = snprintf(alert_msg, RBU_MAX_USAGE_LENGTH, "Failed to find iframe on Page '%s' of Script '%s' ",
                     vptr->cur_page->page_name, vptr->sess_ptr->sess_name);
        if(send_events_to_master == 1)
          write_idx += snprintf(alert_msg + write_idx, RBU_MAX_USAGE_LENGTH - write_idx, "on Generator '%s' ",
                     vptr->cur_page->page_name, vptr->sess_ptr->sess_name);
        snprintf(alert_msg + write_idx, RBU_MAX_USAGE_LENGTH - write_idx, "with iframe %s",
               cav_resp + strlen("error_Failed to find iframe on page with "));

        make_msg_and_send_alert(vptr, alert_msg);
      }
      handle_rbu_failure_con_to_brower(-1, vptr, page_load_wait_time);
      HANDLE_RBU_PAGE_FAILURE_WITH_CLICK(-1)
       return -1; // SHJ: Do Error Handling here
    }

    //Set MerHarFiles fleg
    vptr->first_page_url->proto.http.rbu_param.merge_har_file = att[MERGEHARFILES]?atoi(att[MERGEHARFILES]):0;

    if(har_flag == 1 || har_flag == 101) 
    {
      //Only for msg
      ns_rbu_make_full_qal_url(vptr);
      time_t start_time = time(NULL);

      //TODO: Wat happen when ns_rbu_wait_for_har_file, retrun '-1'. If handled?
      /* Wait till either har file not generated or timeout */
      ns_rbu_wait_for_har_file(vptr->cur_page->page_name, rbu_resp_attr->url, rbu_resp_attr->profile, page_load_wait_time, rbu_resp_attr->har_log_path, rbu_resp_attr->har_log_dir, rbu_resp_attr->firefox_cmd_buf, vptr->first_page_url->proto.http.rbu_param.har_rename_flag, vptr, start_time); 
    }
    else 
    {
      NSDL2_RBU(vptr, NULL, "status set to OK for page = %s", vptr->cur_page->page_name);
      vptr->last_cptr->req_ok = NS_REQUEST_OK;
      //For NetTest
      vptr->sess_ptr->netTest_page_executed++;
      global_settings->netTest_total_executed_page++;

      NS_SEL(vptr, NULL, DM_L1, MM_SCHEDULE, "SCRIPT_EXECUTION_LOG: Script=%s; Status=Progress; Total Page=%d; Executed Page=%d",
              get_sess_name_with_proj_subproj_int(vptr->sess_ptr->sess_name, vptr->sess_ptr->sess_id, "/"),
              vptr->sess_ptr->num_pages, vptr->sess_ptr->netTest_page_executed); 
      NS_SEL(vptr, NULL, DM_L1, MM_SCHEDULE, "SCRIPT_EXECUTION_LOG: Script=%s; Status=OverallExecutedPage; Total Executed Page=%d",
              get_sess_name_with_proj_subproj_int(vptr->sess_ptr->sess_name, vptr->sess_ptr->sess_id, "/"),
              global_settings->netTest_total_executed_page);
      // This is the case when no web_url_end will occur. So, we need to switch user, send message to thread and java
      if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT)
      {
        // Since no action has been required for these api. So for user_context we are setting vptr->page_status to 0
        // This vptr->page_status is required in ns_click_action
        NSDL1_RBU(vptr, NULL, "Switching to user context");
        switch_to_vuser_ctx(vptr, "ClickExecutePageOver");
      }
      else if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_SEPARATE_THREAD)
      {
        // Throughout the code we are sending WEB_URL_REP to thread
        NSDL1_RBU(vptr, NULL, "Sending reply message of CLICK_API to thread");
        send_msg_nvm_to_vutd(vptr, NS_API_WEB_URL_REP, 0);
      }
      else if(vptr->sess_ptr->script_type == NS_SCRIPT_TYPE_JAVA)
      {
        NSDL1_RBU(vptr, NULL, "Sending reply message of CLICK_API to JAVA");
        send_msg_to_njvm(vptr->mcctptr, NS_NJVM_API_CLICK_API_REP, 0);
      }
    }
  }
  return 0;
}

void make_click_con_to_browser_v2_callback(ClientData client_data)
{

  wait_for_lgh_cb *tmp_click_con = (wait_for_lgh_cb *)client_data.p;
  VUser *vptr = tmp_click_con->vptr;
  NSDL4_RBU(vptr, NULL, "Method called, Timer Expired");
  ns_rbu_make_click_con_to_browser(vptr, vptr->httpData->rbu_resp_attr->profile, tmp_click_con->start_time);
 
}

// This is the callback function for click_con_to_browser
void make_click_con_to_browser_callback(ClientData client_data)
{
  wait_for_lgh_cb *tmp_click_con = (wait_for_lgh_cb *)client_data.p;
  VUser *vptr = tmp_click_con->vptr;
  NSDL4_RBU(vptr, NULL, "Method called, Timer Expired");
  ns_rbu_make_click_con_to_browser(vptr, vptr->httpData->rbu_resp_attr->profile, tmp_click_con->start_time);
  
}
//This method will make click connection to browser.
//Return fd : Success, -1: Failure
int ns_rbu_make_click_con_to_browser(VUser *vptr, char *prof_name, time_t start_time)
{
  char port_file[RBU_MAX_VALUE_LENGTH];
  struct stat f_stat;
  char port_str[RBU_MAX_8BYTE_LENGTH];
  FILE *port_fp;
  int cav_fd, cav_port; 
  char err_msg[RBU_MAX_VALUE_LENGTH];

  RBU_RespAttr *rbu_resp_attr = vptr->httpData->rbu_resp_attr;

  int retry = vptr->httpData->rbu_resp_attr->retry_count;

  //Check for cav service listener port.
  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.browser_mode == RBU_BM_FIREFOX)
  {
    snprintf(port_file, RBU_MAX_VALUE_LENGTH, "%s/.rbu/.mozilla/firefox/profiles/%s/cav_listener.port", g_home_env, prof_name);
  }
  else if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.browser_mode == RBU_BM_CHROME)
  {
    snprintf(port_file, RBU_MAX_VALUE_LENGTH, "%s/.rbu/.chrome/profiles/%s/port_info.txt", g_home_env, prof_name);
  }

  do
  { 
    if(lstat(port_file, &f_stat)) {
      NSTL1_OUT(NULL, NULL, "Error:(ns_rbu_send_msg_to_browser), port file \'%s\' doesn't exist, Cav Service not listening.\n", port_file);
      snprintf(rbu_resp_attr->access_log_msg, RBU_MAX_ACC_LOG_LENGTH, "Error: port file \'%s\' doesn't exist, Cav Service not listening.", 
                    port_file);
      strncpy(script_execution_fail_msg, "Internal Error:Port file doesn't exist, Netstorm Browser Extension(CavService) not listening.", MAX_SCRIPT_EXECUTION_LOG_LENGTH);
      HANDLE_RBU_PAGE_FAILURE_WITH_CLICK(-1)
      return -1;
    }

    if(!f_stat.st_size) {
      NSTL1_OUT(NULL, NULL, "Error: (ns_rbu_send_msg_to_browser), port file \'%s\' empty, Cav Service not Listening.\n", port_file);
      snprintf(rbu_resp_attr->access_log_msg, RBU_MAX_ACC_LOG_LENGTH, "Error: port file \'%s\' empty, Cav Service not listening.", port_file);
      strncpy(script_execution_fail_msg, "Internal Error:Port file is empty, Netstorm Browser Extension(CavService) not listening.", MAX_SCRIPT_EXECUTION_LOG_LENGTH);
      HANDLE_RBU_PAGE_FAILURE_WITH_CLICK(-1)
      return -1;
    }

    if((port_fp = fopen(port_file, "r")) == NULL) {
      NSTL1_OUT(NULL, NULL, "Error: (ns_rbu_send_msg_to_browser), failed to open port file \'%s\'\n", port_file);
      snprintf(rbu_resp_attr->access_log_msg, 
                 RBU_MAX_ACC_LOG_LENGTH, "Error: failed to open port file \'%s\'", 
                port_file);
      strncpy(script_execution_fail_msg, "Internal Error:Failed to open Netstorm Browser Extension(CavService) port file.", MAX_SCRIPT_EXECUTION_LOG_LENGTH);
      HANDLE_RBU_PAGE_FAILURE_WITH_CLICK(-1)
      return -1;
    }

    if(fread(port_str, 1, f_stat.st_size, port_fp) != f_stat.st_size) {
      NSTL1_OUT(NULL, NULL, "Error: (ns_rbu_send_msg_to_browser), failed to read from port file \'%s\'\n", port_file);
      strncpy(rbu_resp_attr->access_log_msg, "Error: failed to read from port file", 
                     RBU_MAX_ACC_LOG_LENGTH);
      strncpy(script_execution_fail_msg, "Internal Error:Failed to read from Netstorm Browser Extension(CavService) port file.", MAX_SCRIPT_EXECUTION_LOG_LENGTH);
      HANDLE_RBU_PAGE_FAILURE_WITH_CLICK(-1)
      return -1;
    }
  
    port_str[f_stat.st_size] = 0;

    fclose(port_fp);
  
    cav_port = atoi(port_str);

    NSDL3_RBU(vptr, NULL, "Making click connection to cav-listner at Port = %d, max_url_retries = %d, retry_count = %d",
                                                   cav_port, global_settings->rbu_com_setting_max_retry, retry);
    NSTL1(vptr, NULL, "Making click connection to cav listner at Port = %d, max_url_retries = %d, retry_count = %d",
                                                   cav_port, global_settings->rbu_com_setting_max_retry, retry);
    // Now connect to this port.
    // Earlier we were setting timeout to 60 sec. In a macys machine some times connect was blocking up to 60 sec. Although it should not
    // happen generally. Due to this reason our NVM was blocked in connect, and we was getting **(stars) in progress report. So setting this
    // timeout to 10 second
    // TODO: make this socket nonblocking

    cav_fd = nslb_tcp_client_ex("127.0.0.1", cav_port, TCP_CLIENT_CON_TIMEOUT, err_msg);

    //If connection is made then break the loop   
    if(cav_fd > 0)
    {
      NSDL1_RBU(vptr, NULL, "Click Connection made sucessfully on port = %d, cav_fd = %d, con_retry_count = %d", cav_port, cav_fd, retry);
      NSTL1(vptr, NULL, "Click Connection made sucessfully on port = %d, cav_fd = %d, con_retry_count = %d", cav_port, cav_fd, retry);
      break;
    }
 
    retry++;
    //If retry count is greater then rbu_com_setting_max_retry then return -1
    if((retry > global_settings->rbu_com_setting_max_retry) && (cav_fd == -1))
    {
      NS_DT1(vptr, NULL, DM_L1, MM_VARS, "Error: Failed to create connection with NetStorm Browser Extension (i.e CavService) due to = %s", err_msg);

      NSDL1_RBU(vptr, NULL, "Error: Click Connection is not made as retry count %d exceed to max %d, cav_fd = %d, cav_port = %d, err_msg = %s",
                                                        retry, global_settings->rbu_com_setting_max_retry, cav_fd, cav_port, err_msg);

      NSTL1(vptr, NULL, "Error: Click Connection is not made as retry count %d exceed to max %d, cav_fd = %d, cav_port = %d, err_msg = %s",
                                                        retry, global_settings->rbu_com_setting_max_retry, cav_fd, cav_port, err_msg);
      strncpy(rbu_resp_attr->access_log_msg, "Error: Click Connection is not made", RBU_MAX_ACC_LOG_LENGTH);
      snprintf(script_execution_fail_msg, MAX_SCRIPT_EXECUTION_LOG_LENGTH, "Internal Error:Failed to create connection with NetStorm Browser Extension (i.e CavService). Error: %s", err_msg); 
      HANDLE_RBU_PAGE_FAILURE_WITH_CLICK(-1)
      return -1;
    }
    NSDL4_RBU(vptr, NULL, "Unable to connect going to sleep for %u ms and tried to connect %d times", 
                                                             global_settings->rbu_com_setting_interval, retry);
    vptr->httpData->rbu_resp_attr->retry_count = retry;
    VUSER_SLEEP_MAKE_CLICK_CON_TO_BROWSER(vptr, global_settings->rbu_com_setting_interval);

  } while(global_settings->rbu_com_setting_mode);

  // Making retry count to 0.. So that it can be used later
  vptr->httpData->rbu_resp_attr->retry_count = 0;

  if(cav_fd < 0)
  {
    HANDLE_RBU_PAGE_FAILURE_WITH_CLICK(-1)
    return -1;
  }
  else
  {
    time_t con_start_time = time(NULL);
    rbu_make_click_con_to_browser_v2(vptr, cav_fd, start_time, con_start_time);
  }
  return 0;
}

/* Click APIs
     ns_link
     ns_element 
     ns_span
     ns_button
     ns_form
     ns_map_area

     ns_edit_field
     ns_check_box
     ns_list

     ns_mouse_hover
     ns_mouse_out

     ns_key_event

     Other Mouse Events: ns_mouse_down, ns_mouse_up, ns_mouse_move, ns_mouse_enter, ns_mouse_leave
     Other Mouse Events are not implemented in API, as not able to understant the Real Usage

     har_flag may be 0 - OFF, 1 - ON, 100 - OFF but merge 3 or more har files ON, 101 - ON and merge 3 or more har files ON
*/

int ns_rbu_click_execute_page(VUser *vptr, int page_id, int ca_idx)
{

  ClickActionTableEntry_Shr *ca;
  char *att[NUM_ATTRIBUTE_TYPES];

  RBU_RespAttr *rbu_resp_attr;

  #ifdef NS_DEBUG_ON
    char debug_msg[64*1024];
  #endif

  NSDL1_RBU(vptr, NULL, "Method Called, vptr = %p, page_id = %d, ca_id = %d", vptr, page_id, ca_idx);

  /* Find the click action table entry for ca_idx */
  ca = &(clickaction_table_shr_mem[ca_idx]);

  memset(att, 0, sizeof(att));
  if(read_attributes_array_from_ca_table(vptr, att, ca) < 0)
  {
    HANDLE_RBU_PAGE_FAILURE(-1)
    return -1;
  }
  NSDL2_RBU(vptr, NULL, "Attributes Array Read, ATTRIBUTES: %s", attributes2str(att, debug_msg));

  rbu_resp_attr = vptr->httpData->rbu_resp_attr;
  if(rbu_resp_attr == NULL)
  {
    NSTL1_OUT(NULL, NULL, "ns_rbu_click_execute_page(): rbu_resp_attr is NULL\n");
    HANDLE_RBU_PAGE_FAILURE(-1)
    return -1;
  }

  // Assigning it to 0 so that it can be used in make_conn_to_browser
  rbu_resp_attr->retry_count = 0;

  // We will not take any action on nsi_form api.
  // These API's are not used but we are not deleting them
  if(!strncasecmp(att[APINAME], "ns_form", 7) || !strncasecmp(att[APINAME], "ns_element", 10)) 
  {
    NSDL2_RBU(vptr, NULL, "%s api found, no action taken", att[APINAME]);
    if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT)
    {
      // Since no action has been required for these api. So for user_context we are setting vptr->page_status to 0
      // This vptr->page_status is required in ns_click_action
      vptr->page_status = 0;
      NSDL1_RBU(vptr, NULL, "Switching to user context");
      switch_to_vuser_ctx(vptr, "ClickExecutePageOver");
      return 0;
    }
    else if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_SEPARATE_THREAD)
    {
      // Throughout the code we are sending WEB_URL_REP to thread
      NSDL1_RBU(vptr, NULL, "Sending reply message of CLICK_API to thread");
      send_msg_nvm_to_vutd(vptr, NS_API_WEB_URL_REP, 0);
    }
    else if(vptr->sess_ptr->script_type == NS_SCRIPT_TYPE_JAVA)
    {
      NSDL1_RBU(vptr, NULL, "Sending reply message of CLICK_API to JAVA");
      send_msg_to_njvm(vptr->mcctptr, NS_NJVM_API_CLICK_API_REP, 0);
    }
    return 0;
  }

  // Use of att is done here.. Deallocating it
  free_attributes_array(att);

  time_t start_time;
  start_time = time(NULL); // This start time we need in callback

  int ret = ns_rbu_make_click_con_to_browser(vptr, vptr->httpData->rbu_resp_attr->profile, start_time);
  if(ret != 0) // We need to take action on this return
    return ret;
  else
   return 0;
}//Function ends here


/*--------------------------------------------------------------------------------------------------------------- 
 * Fuction name   : ns_rbu_copy_profiles_to_tr
 * 
 * Purpose        : In case of RBU Automation : This will copy profiles to 
 *                                              /home/<user>/<controller>/logs/TRxxx/rbu_logs/profiles
 *
 * Input          : vptr, browser_base_log_path = /home/<user>/.rbu/.mozilla/firefox/logs or /home/<user>/.rbu/.chrome/logs
 *                  profile_name
 *             
 * Output         : On success =  0
 *                : On fail    = -1
 *--------------------------------------------------------------------------------------------------------------*/
int ns_rbu_copy_profiles_to_tr(VUser *vptr, char *browser_base_log_path, char *profile_name)
{
  char tr_profiles_path [RBU_MAX_PROFILE_NAME_LENGTH + 1];
  char tr_profile_name [RBU_MAX_PROFILE_NAME_LENGTH + 8];
  //Bug 64225: command length was more 1024. Hence, increasing cmd_buf length from 1024 to 2 * 1024.
  char cmd_buf [RBU_MAX_USAGE_LENGTH];  

  NSDL2_RBU(vptr, NULL, "Method called, vptr = %p, browser_base_log_path = %s, profile_name = %s", vptr, browser_base_log_path, profile_name);

  snprintf(tr_profiles_path, RBU_MAX_PROFILE_NAME_LENGTH, "%s/logs/%s/rbu_logs/profiles", getenv("NS_WDIR"), global_settings->tr_or_partition);
  if(make_dir(tr_profiles_path) != 0)
    return -1;

  snprintf(tr_profile_name, RBU_MAX_PROFILE_NAME_LENGTH + 8, "%s/%s", tr_profiles_path, profile_name);
  if(make_dir(tr_profile_name) != 0)
    return -1;

  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.browser_mode == RBU_BM_FIREFOX)
    sprintf(cmd_buf, "cp -r %s/profiles/%s/extensions/ %s/ >/dev/null 2>&1",
                      browser_base_log_path, profile_name, tr_profile_name);
  else if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.browser_mode == RBU_BM_CHROME)
  {
    snprintf(cmd_buf, RBU_MAX_USAGE_LENGTH, "cp -r %s/profiles/%s/\"Local State\" "
                     "%s/profiles/%s/port_info.txt "
                     "%s/profiles/%s/Default/Preferences "
                     "%s/profiles/%s/Default/Extensions/banplgldaabehoilbechndpcjfhacgjb/ %s/ >/dev/null 2>&1",
                      browser_base_log_path, profile_name,
                      browser_base_log_path, profile_name,
                      browser_base_log_path, profile_name,
                      browser_base_log_path, profile_name, tr_profile_name);
  }

  NSDL2_RBU(NULL, NULL, "cmd_buf = %s", cmd_buf);
  nslb_system2(cmd_buf);

 return 0;
}

/*--------------------------------------------------------------------------------------------------
 * Function name    : ns_rbu_start_vnc_and_create_profiles 
 *
 * Purpose 	    : In case of RBU Automation: This function will create profile, invoke vnc and 
 *                    stored in rbu_parameter.csv in each script 
 *                    Profile Name convention - 
 *                    <Generator_Name>-<Controller_Name>-<User_Name>-<Browser_Name>-<Vnc_Id>
 *
 * Output   	    : On success =  0
 *                  : On failure = -1
 * Changing rbu_parameter.csv to .rbu_parameter.csv at script path
 *--------------------------------------------------------------------------------------------------*/
int ns_rbu_start_vnc_and_create_profiles(char *controller_name)
{
  char cmd_buf[2048 + 1];
  char script_path [1024 + 1];
  char rbu_parameter_path [1024 + 1];
  int gp_idx; 

  cmd_buf [0] = 0;
  script_path [0] = 0;
  rbu_parameter_path [0] = 0;

  //struct stat st;
  FILE *rbu_param_fp;

  //First delete rbu_parameter.csv file from the script
  NSDL2_RBU(NULL, NULL, "Method called");

  for(gp_idx = 0; gp_idx < total_runprof_entries; gp_idx++)
  {
    if(!runProfTable[gp_idx].gset.rbu_gset.enable_rbu)
      continue;
/*
    sprintf(rbu_parameter_path, "./scripts/%s/.rbu_parameter.csv", 
             get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[runProfTable[gp_idx].sessprof_idx].sess_name),
                                                 runProfTable[gp_idx].sessprof_idx, "/"));
             //get_sess_name_with_proj_subproj(RETRIEVE_BUFFER_DATA(gSessionTable[runProfTable[gp_idx].sessprof_idx].sess_name)));
*/
    sprintf(rbu_parameter_path, "%s/%s/%s/.rbu_parameter.csv", g_ns_wdir, GET_NS_RTA_DIR(),
             get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[runProfTable[gp_idx].sessprof_idx].sess_name),
                                                 runProfTable[gp_idx].sessprof_idx, "/"));
    NSDL2_RBU(NULL, NULL, "rbu_parameter_path = %s, enable_rbu = %d", rbu_parameter_path, runProfTable[gp_idx].gset.rbu_gset.enable_rbu);

    if((rbu_param_fp = fopen(rbu_parameter_path, "w")) == NULL)
    {
      NSTL1_OUT(NULL, NULL, "Error in opening the file '%s'.error = %s\n", rbu_parameter_path, nslb_strerror(errno));
    }
 
    if(rbu_param_fp != NULL)
      fclose(rbu_param_fp);  
  }

  /* TODO: Its hard to find system chrome or firefox as we cannot proivde sample profile for all version of firefox and chrome 
       In Case of Chrome we always support chrome 34 as system chrome. */
  NSDL2_RBU(NULL, NULL, "total_runprof_entries = %d, controller_name = %s, genarator_name = %s", 
                         total_runprof_entries, controller_name, global_settings->event_generating_host);

  for(gp_idx = 0; gp_idx < total_runprof_entries; gp_idx++)
  {
    NSDL2_RBU(NULL, NULL, "scen_group_name = %s, sess_name = %s, enable_rbu = %d", 
                           RETRIEVE_BUFFER_DATA(runProfTable[gp_idx].scen_group_name),
                           RETRIEVE_BUFFER_DATA(gSessionTable[runProfTable[gp_idx].sessprof_idx].sess_name), 
                           runProfTable[gp_idx].gset.rbu_gset.enable_rbu);

    /*Resolved bug 27336 -->> RBU- Users and vnc are creating on the basis of users provided to total group in scenario in case of RBU_ENABLE_AUTO_PARAM 1 */
    if(!runProfTable[gp_idx].gset.rbu_gset.enable_rbu)
      continue;
 
    if(runProfTable[gp_idx].gset.rbu_gset.browser_mode == RBU_BM_FIREFOX)
    {
      /* According to bug 23122, version option is supported in nsu_firefox_profile_manager */
      if(global_settings->enable_ns_firefox == 1)
      {
        char buff[256] = {0};
        char *firefox_version = runProfTable[gp_idx].gset.rbu_gset.brwsr_vrsn ;
        char *ptr1 = NULL;
        char *ptr2 = NULL;
        FILE *fp = NULL;
        struct stat s;
         
        sprintf(buff, "%s/firefox", g_ns_firefox_binpath);
        if((stat(buff, &s)) != 0)
        {
          NSDL2_RBU(NULL, NULL, "Unable to find path %s, Error: %s", buff, nslb_strerror(errno)); 
          NS_EXIT(-1, CAV_ERR_1031023);
        }
        sprintf(cmd_buf, "%s --version", buff);
        NSDL2_RBU(NULL, NULL, "Command to findout Firefox version = %s", cmd_buf); 
        
        fp = nslb_popen(cmd_buf, "re");
        if(fp)
        {
          buff[0] = '\0';
          while(fgets(buff, 256, fp) != NULL)
          {
            if(buff[0] != '\0')
            {
              ptr1 = strstr(buff, "Mozilla Firefox");
              if(ptr1)
              {
                ptr1 = ptr1 + 16;
                ptr2 = strchr(ptr1, '.');
                if(ptr2)
                {
                  *ptr2 = '\0';
                  strcpy(firefox_version, ptr1);
                  NSDL2_RBU(NULL, NULL, "Firefox version obtained from path %s = %s", g_ns_firefox_binpath, firefox_version);
                }
              } 
            }
          }
          pclose(fp);

          //Firefox version check is needed because we need test directory to run RBU test, 
          //test directory for different browser version is different
          //for example : test_42 at test path will be created for ffx v42
          if(firefox_version[0] == '\0')
          {
            NSDL2_RBU(NULL, NULL, "Unable to find firefox version");
            NS_EXIT(-1, CAV_ERR_1031020, "Firefox");
          } 
          else if(strcmp(firefox_version, "43"))
          {
            NSDL2_RBU(NULL, NULL, "Unable to create profile for given firefox %s, as this version is not supported by RBU.", firefox_version);
            NS_EXIT(-1, CAV_ERR_1031021, "firefox", firefox_version); 
          }
          
          #ifndef CAV_MAIN
          sprintf(cmd_buf, "nsu_auto_gen_prof_and_vnc -o start -n %d -P %s-%s-%s-TR%d-0- -w -B 0 -v %s -f /tmp/rbu_parameter_%d.csv -t %d >/dev/null 2>&1", runProfTable[gp_idx].quantity, global_settings->event_generating_host, controller_name, g_ns_login_user,
              testidx, firefox_version, testidx, testidx);
          #else
          sprintf(cmd_buf, "nsu_auto_gen_prof_and_vnc -o start -n %d -P CavTest-%s-cavisson-TR%d-0- -w -B 0 -v %s -f /tmp/rbu_parameter_%d.csv -t %d >/dev/null 2>&1", runProfTable[gp_idx].quantity, controller_name, testidx, firefox_version, testidx, testidx);
          #endif
        } 
      }
      else
        sprintf(cmd_buf, "nsu_auto_gen_prof_and_vnc -o start -n %d -P %s-%s-%s-TR%d-0- -w -B 0 -v 32 -f /tmp/rbu_parameter_%d.csv -t %d >/dev/null 2>&1", runProfTable[gp_idx].quantity, global_settings->event_generating_host, controller_name, g_ns_login_user, 
              testidx, testidx, testidx);
    }
    else if(runProfTable[gp_idx].gset.rbu_gset.browser_mode == RBU_BM_CHROME)
    {
      if(global_settings->enable_ns_chrome == 1)
      {
        char buff[256] = {0};
        char *chrome_version = runProfTable[gp_idx].gset.rbu_gset.brwsr_vrsn;
        char *ptr1 = NULL;
        char *ptr2 = NULL;
        FILE *fp = NULL;
        struct stat s;

        sprintf(buff, "%s/chromium-browser", g_ns_chrome_binpath);
        if((stat(buff, &s)) != 0)
        {
          fprintf(stderr, "Error: You have enabled the RBU feature but RBU is not installed/configured");
          NSDL2_RBU(NULL, NULL, "Unable to find path %s, Error: %s", buff, nslb_strerror(errno));
          NS_EXIT(-1, CAV_ERR_1000034, buff, errno, nslb_strerror(errno)); 
        }
        sprintf(cmd_buf, "%s/chromium-browser --version", g_ns_chrome_binpath);
        NSDL2_RBU(NULL, NULL, "Command to findout chromium version = %s", cmd_buf);
                
        fp = nslb_popen(cmd_buf, "re");
        if(fp)
        {
          buff[0] = '\0';
          while(fgets(buff, 256, fp) != NULL)
          {
            if(buff[0] != '\0')
            {
              ptr1 = strstr(buff, "Chromium");
              if(ptr1)
              {
                 ptr1 = ptr1 + 9;
                 ptr2 = strchr(ptr1, '.');
                 if(ptr2)
                 {
                   *ptr2 = '\0';
                    strcpy(chrome_version, ptr1);
                    NSDL2_RBU(NULL, NULL, "Chromium version obtained from path %s = %s", g_ns_chrome_binpath, chrome_version);
                 } 
              }
            } 
          }
          pclose(fp);

          //Chrome version check is needed because we need test directory to run RBU test, 
          //test directory for different browser version is different
          //for example : test_40 at test path will be created for chrome v40
          if(chrome_version[0] == '\0')
          {
            NSDL2_RBU(NULL, NULL, "Unable to find chrome version");
            NS_EXIT(-1, CAV_ERR_1031020, "Chrome");
          }
          else if((strcmp(chrome_version, "60")) && (strcmp(chrome_version, "68")) &&
                 (strcmp(chrome_version, "78")) && (strcmp(chrome_version, "93")))
          {
            NSDL2_RBU(NULL, NULL, "Unable to create profile for given chrome %s, as this version is not supported by RBU.\n", chrome_version); 
            NS_EXIT(-1, CAV_ERR_1031021, "chrome", chrome_version); 
          } 

          /* Currently we have supported chromium-browser 40 and 51 as NS chrome browser */
          #ifndef CAV_MAIN
          sprintf(cmd_buf, "nsu_auto_gen_prof_and_vnc -o start -n %d -P %s-%s-%s-TR%d-1- -w -B 1 -v %s -f /tmp/rbu_parameter_%d.csv -t %d >/dev/null 2>&1",
                          runProfTable[gp_idx].quantity, global_settings->event_generating_host, controller_name,
                          g_ns_login_user, testidx, chrome_version, testidx, testidx);
          #else
          sprintf(cmd_buf, "nsu_auto_gen_prof_and_vnc -o start -n %d -P CavTest-%s-cavisson-TR%d-1- -w -B 1 -v %s -f /tmp/rbu_parameter_%d.csv -t %d >/dev/null 2>&1",
                          runProfTable[gp_idx].quantity, controller_name, testidx, chrome_version, testidx, testidx);
          #endif
        }
      }
      else 
        sprintf(cmd_buf, "nsu_auto_gen_prof_and_vnc -o start -n %d -P %s-%s-%s-TR%d-1- -w -B 1 -v 34 -f /tmp/rbu_parameter_%d.csv -t %d >/dev/null 2>&1",
                          runProfTable[gp_idx].quantity, global_settings->event_generating_host, controller_name,
                          g_ns_login_user, testidx, testidx, testidx);
    }
    else  
    { 
      NSDL3_RBU(NULL, NULL, "Invalid Browser mode");
      NS_EXIT(-1, CAV_ERR_1031022); 
    }                                 
  
    NSDL2_RBU(NULL, NULL, "Creating Profiles and vncs: cmd_buf = %s", cmd_buf);
  
    nslb_system2(cmd_buf);
/*
    sprintf(script_path, "./scripts/%s",
                          get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[runProfTable[gp_idx].sessprof_idx].sess_name),
	                  runProfTable[gp_idx].sessprof_idx, "/"));
                          //get_sess_name_with_proj_subproj(RETRIEVE_BUFFER_DATA(gSessionTable[runProfTable[gp_idx].sessprof_idx].sess_name)));
*/ 
    sprintf(script_path, "%s/%s", GET_NS_RTA_DIR(),
                          get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[runProfTable[gp_idx].sessprof_idx].sess_name),
	                  runProfTable[gp_idx].sessprof_idx, "/"));

    NSDL2_RBU(NULL, NULL, " script_path = %s", script_path);
    sprintf(cmd_buf, "cat /tmp/rbu_parameter_%d.csv >> %s/%s/.rbu_parameter.csv; rm /tmp/rbu_parameter_%d.csv 2>/dev/null", 
                      testidx, g_ns_wdir, script_path, testidx);

    NSDL2_RBU(NULL, NULL, " Update .rbu_parameter.csv file in script, %s", cmd_buf);

    nslb_system2(cmd_buf);

  }
  return 0;
}

/*--------------------------------------------------------------------------------------------------
 * Function name    : ns_rbu_chk_sess_duality
 *
 * Purpose          : Need to check wheather same script is used in both RBU and Non-RBU mode.
 *                    Same script can not be used in both mode.
 *
 * Modificaton Date :
 * Build_v          : 4.3.0 #67
 *--------------------------------------------------------------------------------------------------*/
void ns_rbu_chk_sess_duality()
{
  int sess_idx, runprof_idx;
  char rbu_enable = 0;
  char rbu_disable = 0;

  NSDL2_RBU(NULL, NULL, "Method called.");
  for(sess_idx = 0; sess_idx < total_sess_entries; sess_idx++) //Iterate for Script
  {
    //reset flags.
    rbu_enable = 0;
    rbu_disable = 0;
    for(runprof_idx = 0; runprof_idx < total_runprof_entries; runprof_idx++) //Iterate for Group
    {
      if(sess_idx == runProfTable[runprof_idx].sessprof_idx) //Script used in the group
      {
        if(runProfTable[runprof_idx].gset.rbu_gset.enable_rbu) //RBU is enabled
          rbu_enable = 1;
        else
          rbu_disable = 1;
      }
    }
    if(rbu_enable && rbu_disable) //Same script is used in both RBU and Non-RBU mode
    {
      NS_EXIT(-1, CAV_ERR_1011290,
                    get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), sess_idx, "/"));
    }
  }
}

/*--------------------------------------------------------------------------------------------------
 * Function name    : ns_rbu_on_test_start 
 *
 * Purpose          : In case of RBU Automation: This function firstly, stop vnc and delete profiles 
 *		      on that controller and then call ns_rbu_start_vnc_and_create_profiles()
 *
 * Output           : On success =  0
 *                  : On failure = -1
 *         
 * Modificaton Date : 
 * Build_v          : 4.1.5 #12
 *--------------------------------------------------------------------------------------------------*/
int ns_rbu_on_test_start()
{
  char cmd_buf [2048 + 1];
  char controller_name[512 + 1] = "";
  char *controller_name_ptr = NULL;

  cmd_buf [0] = 0;

  NSDL2_RBU(NULL, NULL, "Method called."); 

  //Is script used in both rbu and non-rbu mode?
  ns_rbu_chk_sess_duality();

  if((controller_name_ptr = strrchr(g_ns_wdir, '/')) != NULL)
    strcpy(controller_name, controller_name_ptr + 1);

  NSDL4_RBU(NULL, NULL, "controller_name = %s, generator_name = %s", controller_name, global_settings->event_generating_host);

  //This will stop vnc and delete profiles
  #ifdef NS_DEBUG_ON
    sprintf(cmd_buf, "nsu_auto_gen_prof_and_vnc -o init -P %s-%s -t %d -D >/dev/null 2>&1", 
                      controller_name, g_ns_login_user, testidx); 
  #else
    sprintf(cmd_buf, "nsu_auto_gen_prof_and_vnc -o init -P %s-%s -t %d >/dev/null 2>&1", 
                      controller_name, g_ns_login_user, testidx);
  #endif

  NSDL2_RBU(NULL, NULL, "cmd_buf = %s", cmd_buf);

  nslb_system2(cmd_buf);

  if(ns_rbu_start_vnc_and_create_profiles(controller_name) == -1)
  {
    NSDL3_RBU(NULL, NULL, "Unable to start vnc and create profiles, hence returning -1");
    return -1;
  }

  return 0;
}

/*--------------------------------------------------------------------------------------------------
 * Function name    : ns_rbu_req_stat
 *
 * Purpose          : This function will compare each url time with DOM_Loaded_time, OnLoad_Time and Start_Render_Time
 *                    If url time is less then any of these time then request will be set as request coming before that event  
 *
 * Output           : If Condition pass: Increment the counter 
 *                  : If Condition fails: No changes in counter
 *         
 * Modificaton Date : 
 *--------------------------------------------------------------------------------------------------*/
int ns_rbu_req_stat(int url_time, int rbu_attr, int *req_count)
{
  NSDL2_RBU(NULL, NULL, "Method called.");
  NSDL2_RBU(NULL, NULL, "url_time = [%d], RBU_RespAttr = [%d]", url_time, rbu_attr);
  
  if(url_time <= rbu_attr)
  {
    (*req_count)++;
    NSDL2_RBU(NULL, NULL, "Number of request = [%d]", *req_count);
  }

  return *req_count;
}

/*--------------------------------------------------------------------------------------------------
 * Function name    : rbu_fill_page_status 
 *
 * Purpose          : This function will fill page status data to the RBU_RespAttr structure
 *                    These will be used by RBUPageStatAvgTime structure to fill page status graphs.
 *
 *         
 * Author           : Shikha 
 * Date             : 20 July 2017
 *--------------------------------------------------------------------------------------------------*/
int rbu_fill_page_status(VUser *vptr, RBU_RespAttr *rbu_resp_attr)
{
  int hash_status_code = 0;

  NSDL1_RBU(vptr, NULL, "Method called");

  if(rbu_resp_attr == NULL){
    if (vptr->httpData->rbu_resp_attr != NULL)
    {
      rbu_resp_attr = vptr->httpData->rbu_resp_attr;
    }
    else
    { 
      NSDL4_RBU(vptr, NULL, "RBU_RespAttr not accessible");
      return -1;
    }
  }

  //In CA: On first click fail, rbu_resp_attr->page_status sets as previuos web url rbu_resp_attr->page_status
  //Now taking as vptr->page_status
  if(vptr->page_status == NS_REQUEST_ERRMISC || vptr->page_status == NS_REQUEST_TIMEOUT || vptr->page_status == NS_REQUEST_CV_FAILURE)
    rbu_resp_attr->status_code = 0;

  NSDL1_RBU(vptr, NULL, "HAR File status_code = [%d]", rbu_resp_attr->status_code);

  if((rbu_resp_attr->status_code >=100) && (rbu_resp_attr->status_code < 600)){
    hash_status_code = (rbu_resp_attr->status_code/100);
  }

  if(vptr->page_status == NS_REQUEST_ERRMISC || vptr->page_status == NS_REQUEST_TIMEOUT || vptr->page_status == NS_REQUEST_CV_FAILURE)
    hash_status_code = 0;

  NSDL1_RBU(vptr, NULL, "hash_status_code = [%d]", hash_status_code);

  switch(hash_status_code)
  {
    case 1: 
          rbu_resp_attr->pg_status = PG_STATUS_1xx;
          break;
    case 2: 
          rbu_resp_attr->pg_status = PG_STATUS_2xx;
          break;
    case 3: 
          rbu_resp_attr->pg_status = PG_STATUS_3xx;
          break;
    case 4: 
          rbu_resp_attr->pg_status = PG_STATUS_4xx;
          break;
    case 5:
          rbu_resp_attr->pg_status = PG_STATUS_5xx;
          break;
    default:
          rbu_resp_attr->pg_status = 0;
          break;
  }

  NSDL1_RBU(NULL, NULL, "Page Status Set as = [%d], ",
                           rbu_resp_attr->pg_status);

  return 0;
}

int ns_rbu_get_date_time(VUser *vptr, RBU_RespAttr *rbu_resp_attr)
{
  char *start_ptr = NULL;
  char *end_ptr = NULL;
  int len = 0;

  if(rbu_resp_attr == NULL)
    return -1;
  else
    rbu_resp_attr = vptr->httpData->rbu_resp_attr;

  //Get date
  NSDL3_RBU(vptr, NULL, "rbu_resp_attr->date_time_str is = %s", rbu_resp_attr->date_time_str);
  start_ptr = rbu_resp_attr->date_time_str;
  end_ptr = strchr(rbu_resp_attr->date_time_str, 'T');
  len = end_ptr - start_ptr;
  strncpy(rbu_resp_attr->date, start_ptr, len);
  rbu_resp_attr->date[len] = '\0';

  NSDL3_RBU(vptr, NULL, "start_ptr is = %s, end_ptr = %s, len = %d", start_ptr, end_ptr, len);
  //Get time
  end_ptr = end_ptr + 1; //moving one location ahead so end_ptr now pointing to starting time
  start_ptr= end_ptr;
  end_ptr = strchr(start_ptr, '.');
  len = end_ptr - start_ptr;
  strncpy(rbu_resp_attr->time, start_ptr, len);
  rbu_resp_attr->time[len] = '\0';

  NSDL3_RBU(NULL, NULL, "Date - %s and time - %s", rbu_resp_attr->date, rbu_resp_attr->time);
  
  return 0;
}

/*--------------------------------------------------------------------------------------------------
 * Function name    : ns_rbu_check_acc_log_dump 
 *
 * Purpose          : This function will check if access log need to dump or not 
 *         
 * Author           : Shikha 
 * Date             : 2017
 * Example          : If G_RBU_ACCESS_LOG ALL 1 5xx,  then only data for 5xx resquest code will be dumped 
 *--------------------------------------------------------------------------------------------------*/
void ns_rbu_check_acc_log_dump(VUser *vptr, RBU_RespAttr *rbu_resp_attr, int main_url_done)
{
  int h_pg_status = 0;
  char acc_log_status[32] = {0};
  char acc_log_status_local[32] = {0};
  strcpy(acc_log_status, runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_acc_log_status);

  char *code_fields[50 + 1] = {0};
  int code_num_tokens;
  int acc_log_counter = 0;

  NSDL3_RBU(vptr, NULL, " Method Called");

  // If rbu_acc_log_status is set valid and G_RBU_ACCESS_LOG keyword has mode '2', then inline URL ll also dumped
  // If rbu_acc_log_status is set valid and G_RBU_ACCESS_LOG keyword has mode '1', then Only main URL with redirections will be dumped
  NSDL4_RBU(vptr, NULL, "rbu_acc_log_status - [%s]", acc_log_status);
  if(((runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_access_log_mode == 2) || 
     ((runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_access_log_mode == 1) && (main_url_done == 0)))){

    //Calculating hash code for req status code using vptr->last_cptr->req_code, as dumping all URL status code
    h_pg_status = (vptr->last_cptr->req_code/100);
    NSDL4_RBU(vptr, NULL, "Access Log : Hash Code - [%d]", h_pg_status);

    strcpy(acc_log_status_local, acc_log_status);
    code_num_tokens = get_tokens(acc_log_status_local, code_fields, ";",  10);

    NSDL4_RBU(vptr, NULL, "Access Log : code_num_tokens = %d", code_num_tokens);
    for(acc_log_counter = 0 ; acc_log_counter < code_num_tokens ; acc_log_counter++)
    {
      strcpy(rbu_resp_attr->access_log_msg, "");

      NSDL4_RBU(NULL, NULL, "Access Log : code_fields[%d] - %s", 
                            acc_log_counter, code_fields[acc_log_counter]);
      //TODO set flag bit according to 
      if((!strcmp(code_fields[acc_log_counter], "1xx") && (h_pg_status == PG_STATUS_1xx)) ||
         (!strcmp(code_fields[acc_log_counter], "2xx") && (h_pg_status == PG_STATUS_2xx)) ||
         (!strcmp(code_fields[acc_log_counter], "3xx") && (h_pg_status == PG_STATUS_3xx)) ||
         (!strcmp(code_fields[acc_log_counter], "4xx") && (h_pg_status == PG_STATUS_4xx)) ||
         (!strcmp(code_fields[acc_log_counter], "5xx") && (h_pg_status == PG_STATUS_5xx))){
        snprintf(rbu_resp_attr->access_log_msg, RBU_MAX_ACC_LOG_LENGTH, "%s", rbu_resp_attr->status_text);
        NSDL4_RBU(vptr, NULL, "Access Log : Status Text - %s", rbu_resp_attr->access_log_msg);
        ns_rbu_log_access_log(vptr, rbu_resp_attr, RBU_ACC_LOG_DUMP_LOG);
        NSDL4_RBU(vptr, NULL, "Access Log : Dumping access_log");
      }
    }
  }
}

/*--------------------------------------------------------------------------------------------------
 * Function name    : ns_rbu_url_parameter_fetch 
 *
 * Purpose          : This function will fetch parameter from URL and save into param_string of rbu_resp_attr
 *         
 * Author           : Shikha 
 * Date             : 2 Sept 2017
 * Example          : rbu_resp_attr->inline_url : /catalog/navigation.jsp?_DARGS=/catalog/v2/fragments/pdp_addToBag_Form.jsp
 *                  : after strpbrk, param will hold : ?_DARGS=/catalog/v2/fragments/pdp_addToBag_Form.jsp
 *                  : So, rbu_resp_attr->param_string wil hold : _DARGS=/catalog/v2/fragments/pdp_addToBag_Form.jsp
 *--------------------------------------------------------------------------------------------------*/
void ns_rbu_url_parameter_fetch(VUser *vptr, RBU_RespAttr *rbu_resp_attr)
{
  char *param;                       //Store pointer location for query parameter of a url

  param = strpbrk(rbu_resp_attr->inline_url, ";#?");

  if(param == NULL)
    strcpy(rbu_resp_attr->param_string, "");
  else
    strncpy(rbu_resp_attr->param_string, param+1, 128);

  param = '\0';

    NSDL4_RBU(vptr, NULL, "Access Log : param_string = %s", rbu_resp_attr->param_string);
}

void create_rbu_domains_table()
{
  int old_max_domains = 0; 
  int new_max_domains = 0;

  NSDL3_RBU(NULL, NULL, "Method called, total_rbu_domains = %d, max_rbu_domains = %d", 
                         total_rbu_domain_entries, max_rbu_domain_entries);

  if(total_rbu_domain_entries >= max_rbu_domain_entries)
  {
    old_max_domains = max_rbu_domain_entries;
    new_max_domains = old_max_domains + DELTA_RBU_DOMAIN_ENTRIES;

    NSDL3_RBU(NULL, NULL, "old_rbu_domains = %d, total_rbu_domains = %d",
                           old_max_domains, new_max_domains);

    MY_REALLOC_AND_MEMSET(rbu_domains, sizeof(Rbu_domains) * new_max_domains, sizeof(Rbu_domains) * old_max_domains, "rbu_domains", -1);
    
    /* Note: total_rbu_domain_entries and max_rbu_domain_entries will not be increased here as these variables also used
             to increase avgtime. */
  }
}

int ns_rbu_handle_domain_discovery(VUser *vptr, char *inline_url, int resp_time)
{
  static char domain_vector[RBU_MAX_DOMAIN_NAME_LENGTH + 1];
  char *st_ptr = NULL;
  char *ed_ptr = NULL;
  char tmp;
  int norm_id = -1;
  int domain_len = 0;
  int is_new_domain = 0;
  int row_num = -1;

  NSDL4_RBU(vptr, NULL, "Method Called, with inline_url as - %s, elapse_time = %d", inline_url, resp_time);

  domain_vector[0] = 0;

  //Skipping protocol schema i.e. http:// or https:// or ftp://
  if((st_ptr = (strstr(inline_url, "//"))) == NULL)
  {
    NSDL4_RBU(vptr, NULL, "URL is not in correct format, '//' missing in protocol schema (eg: http://). Url is %s", inline_url);
    return -1;
  }

  st_ptr += 2;                  //skip '//'

  if((ed_ptr = strchr(st_ptr, '/')) != NULL)
  {
    tmp = *ed_ptr;
    *ed_ptr = '\0'; //Terminating domian
  }
  
  NSDL4_RBU(vptr, NULL, "st_ptr = [%s]", st_ptr);

  domain_len = sprintf(domain_vector, "%s>%s>%s", runprof_table_shr_mem[vptr->group_num].scen_group_name, 
                                     vptr->cur_page->page_name, st_ptr);

  //domain_len = strlen(domain_vector);
  NSDL4_RBU(vptr, NULL, "domain_len = %d, domain_vector = %s", domain_len, domain_vector);

  //In last: Fill ed_ptr to make inline url string  
  *ed_ptr = tmp;

  //Fetch norm_id acording to the vector
  norm_id = nslb_get_or_set_norm_id(&rbu_domian_normtbl, domain_vector, domain_len, &is_new_domain);

  if(is_new_domain)
  {
    NSDL4_RBU(vptr, NULL, "New domain discovered, domain_len = %d, domain_vector = %s, norm_id = %d", 
                           domain_len, domain_vector, norm_id);

    create_rbu_domains_table();

    //send local_norm_id to parent
    send_new_object_discovery_record_to_parent(vptr, domain_len, domain_vector, NEW_OBJECT_DISCOVERY_RBU_DOMAIN, norm_id);

    create_dynamic_data_avg(&g_child_msg_com_con, &row_num, my_port_index, NEW_OBJECT_DISCOVERY_RBU_DOMAIN);
  }

  rbu_domains[norm_id].is_filled = 1;

  //Aggregated response time of urls of same domain
  rbu_domains[norm_id].url_resp_time += resp_time;

  //increment num_request to keep track on number of request served from that domain
  rbu_domains[norm_id].num_request++;

  return norm_id;
}
