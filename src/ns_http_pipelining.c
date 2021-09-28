/**
 * HTTP Pipelining specific code
 */

#include <stdio.h>
#include <string.h>
#include <regex.h>
#include <ctype.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
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
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"
#include "ns_http_version.h"

#include "netstorm.h"
#include "ns_log.h"
#include "ns_auto_cookie.h"
#include "ns_cookie.h"
#include "amf.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "ns_url_req.h"
#include "ns_alloc.h"
#include "ns_sock_com.h"
#include "ns_debug_trace.h"
#include "ns_smtp.h"
#include "ns_smtp_send.h"
//#include "ns_handle_read.h"
#include "init_cav.h"
#include "ns_ftp_send.h"
#include "ns_url_resp.h"
#include "ns_ftp.h"
#include "ns_pop3.h"
#include "ns_auto_fetch_embd.h"
#include "ns_log.h"
#include "nslb_sock.h"
#include "ns_parallel_fetch.h"
#include "ns_vuser.h"
#include "ns_js.h"
#include "ns_connection_pool.h"
#include "ns_exit.h"
//Usage for G_ENABLE_PIPELINIG keyword
static void ns_g_enable_pipelining_usage(char *msg, char *err_msg){
  sprintf(err_msg, "Error: %s\n", msg);
  strcat(err_msg, "Usage:\n");
  strcat(err_msg, "G_ENABLE_PIPELINIG <GROUP/ALL> <MODE> <PIPELINE DEPTH>\n");
  strcat(err_msg, "Mode values can be 0(Disable)-Default, 1(Enable)\n");
  strcat(err_msg, "Pipeline depth cannot be greater than 32\n");
  NS_EXIT(-1, "%s", err_msg);
}


/* send_request_on_pipe() sends cur_url on the pipe and puts old hurl into
 * queue. This method is very similar to try_hurl_on_cur_con() */
static void send_request_on_pipe(connection *cptr, 
                                 HostSvrEntry *cur_host,
                                 action_request_Shr *cur_url, 
                                 u_ns_ts_t now)
{
  VUser *vptr = (VUser *)cptr->vptr;
  NSDL3_CONN(vptr, cptr, "Method Called");

  if (cptr->num_pipe > 1 &&
      (cur_url->proto.http.http_method == HTTP_METHOD_POST || 
       cur_url->proto.http.http_method == HTTP_METHOD_PUT  ||
       cur_url->proto.http.http_method == HTTP_METHOD_PATCH)) {
    NSEL_CRI(vptr, cptr, ERROR_ID, ERROR_ATTR, "not sending PUT/POST/PATCH request on pipe");
    return;
  }

  if (cptr->num_pipe > 1) {
    pipeline_page_list *pp_list;
    pipeline_page_list *pp_travel;

    MY_MALLOC(pp_list, sizeof(pipeline_page_list), "pipeline_page_list", -1);
    pp_list->next = NULL;

    /* Save */
    pp_list->url_num = cur_url;

    if (!cptr->data) {
      cptr->data = (void *)pp_list;
    } else {
      pp_travel = (pipeline_page_list *)(cptr->data);
      /* Go to end */
      while (pp_travel->next)  pp_travel = pp_travel->next;
      
      /* Add in the end. */
      pp_travel->next = pp_list;
    }
  }

  SET_URL_NUM_IN_CPTR(cptr, cur_url);
  cptr->url_num = cur_url;
  hurl_done(vptr, cur_url, cur_host, now);
  if (cur_host->hurl_left == 0)
    next_remove_from_hlist(vptr, vptr->hptr + cptr->gServerTable_idx);

  NSDL3_CONN(vptr, cptr, "trying to send more num_pipe = %d, cptr->conn_state = %d\n",
             cptr->num_pipe, cptr->conn_state);
  

/* Changed to start_new_socket() on Feb 16th, 2011 for cleanup and to make sure  on_url_start is called before start_socket()
  //renew_connection(cptr, now);
  start_socket(cptr, now );
  // printf("Second Calling function on_url_start\n");
  on_url_start(cptr);
  //(average_time->fetches_started)++;
*/
  start_new_socket(cptr, now );
}


/**
 * This method is called by :
 * on_request_write_done() or close_connection()
 *
 * if called from on_request_write_done() we can be sure that it is not
 * in the middle of writing a request. In this case we can proceed with
 * sending the pending request on the pipeline 
 *
 * if called from close_connection() we might be in the middle of writing
 * another request. In this case its conn_state will be writing. Otherwise
 * it can be either already closed or already in reuse list. It might also
 * so happen that the connection it is not in writing state; this means 
 * that the we have completely sent the requests in the pipe. Here we can 
 * queue the next request and send it directly (if there is space in the 
 * qeuue).
 *
 * The function is re-entrant.
 *
 * This function also handles parallel connections logic.
 *
 * NOTE: Residue of POST request should not reach here.
 *
 */
int pipeline_connection(VUser *vptr, connection* cptr, u_ns_ts_t now)
{

  HostSvrEntry* host_head;

  NSDL2_CONN(vptr, cptr, "Method Called, cptr=%p and urls left is %d"
                         " and urls_awaited is %d num_pipe = %d",
                          cptr, vptr->urls_left, vptr->urls_awaited, cptr->num_pipe);

  if (cptr->bytes_left_to_send != 0 /* Ensure last write was not partial */) {
    NSDL3_HTTP(vptr, cptr, "There was a partial write Returning. "
               "(cptr->bytes_left_to_send = %d)",
               cptr->bytes_left_to_send);
    return 0;
  }

  if (cptr != NULL) {
    
    if (/* (cptr->num_pipe > 0) &&  */ /* POST request will no reach this func() */
        (cptr->num_pipe < runprof_table_shr_mem[vptr->group_num].gset.max_pipeline)) {

      /* Check if we have more to send on same host. */
      host_head = vptr->hptr + cptr->gServerTable_idx;

      if (host_head->hurl_left > 0) {
        //if (cptr->conn_state != CNST_WRITING) { /* should never be the case of writing */
        if (host_head->cur_url->proto.http.http_method != HTTP_METHOD_POST && 
	    host_head->cur_url->proto.http.http_method != HTTP_METHOD_PUT &&
	    host_head->cur_url->proto.http.http_method != HTTP_METHOD_PATCH)
          send_request_on_pipe(cptr, host_head, host_head->cur_url, now);
        else /* PUT/POST SPECIAL */ {
          NSDL2_CONN(vptr, cptr, "Req is either POST or PUT or PATCH. Request will not be pipelined.");
          return 1;  // Returning cptr as this cptr in not used, so we can do parallel fetch on this cptr
        }
          //} /* else if writing we let it write first */
      } else if (cptr->num_pipe == 0) {
        add_to_reuse_list(cptr);
      }
    } else {
      NSDL2_CONN(vptr, cptr, "Connection queue is full. Request will not be pipelined.");
      return 1;
    }
  } else {
    NSDL2_CONN(vptr, cptr, "cptr is NULL. Request will not be pipelined.");
    return 1;
  }

  return 0;
}


int kw_set_g_enable_pipelining(char *text, GroupSettings *gset, char *err_msg)
{
  int enable;
  int num_value = 8; // Default Pipeline depth -- Arun Nishad
  int tmp_value;
  char keyword[1024];
  char sgrp_name[1024];
  int num_fields = sscanf(text, "%s %s %d %d %d", keyword, sgrp_name, &enable, &num_value, &tmp_value);
  
  if(num_fields < 3 || num_fields > 4) {
     ns_g_enable_pipelining_usage("Too many arguments given for keyword G_ENABLE_PIPELING", err_msg);
  }

  if(enable != 0 && enable != 1) {
    ns_g_enable_pipelining_usage("<mode> can have only two values 0 or 1 for keyword G_ENABLE_PIPELING", err_msg); 
  }

  gset->enable_pipelining = enable;
  if (enable) {
    gset->max_pipeline = num_value;
    if (num_value <= 0) {
      ns_g_enable_pipelining_usage("Pipelining depth must be > 0 for keyword G_ENABLE_PIPELING", err_msg);
    } else if (num_value > 32) {
      ns_g_enable_pipelining_usage("Maximum allowed pipeline depth is 32", err_msg);
    }
  }
  if(gset->enable_pipelining == 1 && global_settings->replay_mode != 0){
    ns_g_enable_pipelining_usage("Pipelining can not be enabled with Replay Access Logs scenario", err_msg);
  }
  return 0;
}

//To check Pipeline enabled in any of the group. 
//Used to check 100-Continue at the time of script parsing
inline int get_any_pipeline_enabled()
{
  static int pipeline_enabled = -1;
  int i;
  NSDL2_HTTP(NULL, NULL, "Method Called");
  if(pipeline_enabled == -1)
  {
    for (i = 0; i < total_runprof_entries; i++)
    {
      if(runProfTable[i].gset.enable_pipelining)
      {
        NSDL2_HTTP(NULL, NULL, "Pipeline Enabled in group=%d", i);
        pipeline_enabled = 1;
        return pipeline_enabled;
      }
    }
    pipeline_enabled = 0;
    NSDL2_HTTP(NULL, NULL, "Pipeline not enabled in any group");
  }
  NSDL2_HTTP(NULL, NULL, "Pipeline is %s", (pipeline_enabled)?"enabled":"disabled");
  return pipeline_enabled;
}

void validate_pipeline_keyword()
{
  if((get_any_pipeline_enabled() == 1) && (global_settings->g_follow_redirects > 0)){
    NS_EXIT(-1, "Pipelining can not be enabled with auto redirect.");
  }
}

/* The function returns the url_num whose response is awaited/received. */
action_request_Shr *get_top_url_num(connection *cptr)
{
  NSDL2_CONN(NULL, cptr, "Method Called");

  // for DNS, we use cptr->data for DNS specific data
  if (!(cptr->data) || (cptr->url_num->request_type == DNS_REQUEST) 
    || (cptr->url_num->request_type == USER_SOCKET_REQUEST) || (cptr->request_type == JRMI_REQUEST)) {
    NSDL2_CONN(NULL, cptr, "Returning url_num from cptr");
    return cptr->url_num;
  }else{
    NSDL2_CONN(NULL, cptr, "Returning url_num from cptr->data");
    return ((pipeline_page_list *)(cptr->data))->url_num;
  }
}

/* Start filling in the connection slot. */
inline void setup_cptr_for_pipelining(connection *cptr) {

  NSDL2_HTTP(NULL, cptr, "Method Called, enable_pipelining = %d, num pipe = %d",
			  group_default_settings->enable_pipelining, cptr->num_pipe);

  if (cptr->url_num->request_type == HTTP_REQUEST ||
      cptr->url_num->request_type == HTTPS_REQUEST) {
    if (cptr->url_num->proto.http.http_method == HTTP_METHOD_POST ||
        cptr->url_num->proto.http.http_method == HTTP_METHOD_PUT ||
        cptr->url_num->proto.http.http_method == HTTP_METHOD_PATCH) {
    
      cptr->num_pipe = -1;
    
    } else {
      if (cptr->num_pipe == -1)
        cptr->num_pipe = 0;
      cptr->num_pipe++;
      NSDL4_HTTP(NULL, cptr, "incrementing pipe count = %d", cptr->num_pipe);
    }
  }
}
