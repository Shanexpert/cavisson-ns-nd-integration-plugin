#include <regex.h>

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

#include "netstorm.h"
#include "ns_log.h"
#include "ns_sock_com.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "ns_parallel_fetch.h"
#include "divide_users.h"
#include "divide_values.h"
#include "child_init.h"
#include "unique_vals.h"
#include "ns_sock_list.h"
#include "src_ip.h"
#include "ns_pop3.h"
#include "ns_js.h"
#include "ns_event_log.h"
#include "ns_connection_pool.h"
#include "ns_event_id.h"
#include "ns_string.h"
#include "ns_alloc.h"
#include "ns_page_based_stats.h"
#include "ns_inline_delay.h"
#include "ns_h2_req.h"
#include "ns_group_data.h"
#include "ns_gdf.h"
#include "ns_script_parse.h"
/*bug 54315: method declared*/
static connection* get_inuse_connection(VUser *vptr, int host_idx);     
//next host from host list of hosts containing yet-to-be-executed URL's
inline HostSvrEntry*
next_from_hlist(VUser* vptr, HostSvrEntry* hptr)
{
  NSDL2_SCHEDULE(vptr, NULL, "Method called");
  HostSvrEntry *return_hptr;

  //if (global_settings->debug &&  (global_settings->module_mask & FUNCTION_CALL_OUTPUT))
    NSDL1_MISC(vptr, NULL, "Entering next_from_hlist: hptr=%p", hptr);

  return_hptr = (HostSvrEntry *)hptr->next_hlist;

  //if (global_settings->debug &&  (global_settings->module_mask & FUNCTION_CALL_OUTPUT))
    NSDL1_MISC(vptr, NULL, "Exiting next_from_hlist. %p", return_hptr);

  return return_hptr;
}

//next host from host list of hosts containing yet-to-be-executed URL's
inline HostSvrEntry*
next_remove_from_hlist(VUser* vptr, HostSvrEntry* hptr)
{
  NSDL2_SCHEDULE(vptr, NULL, "Method called");
  HostSvrEntry* cur_next, *cur_prev;

  //if (global_settings->debug &&  (global_settings->module_mask & FUNCTION_CALL_OUTPUT))
    NSDL1_MISC(vptr, NULL, "Entering next_remove_from_hlist: hptr=%p", hptr);

  cur_next = (HostSvrEntry *)hptr->next_hlist;
  cur_prev = (HostSvrEntry *)hptr->prev_hlist;

  if (cur_next)
    cur_next->prev_hlist = (struct HostSvrEntry *)cur_prev;
  else
    vptr->tail_hlist = (HostSvrEntry *)cur_prev;

  if (cur_prev)
    cur_prev->next_hlist = (struct HostSvrEntry *)cur_next;
  else
    vptr->head_hlist = (HostSvrEntry *)cur_next;

  //if (global_settings->debug &&  (global_settings->module_mask & FUNCTION_CALL_OUTPUT))
    NSDL1_MISC(vptr, NULL, "Exiting next_remove_from_hlist.  vptr[%p]->head_hlist=%p",  vptr,  vptr->head_hlist);

  return (HostSvrEntry*)cur_next;
}

inline void
add_to_hlist(VUser *vptr, HostTableEntry_Shr *hel)
{
  HostSvrEntry *hptr;
  short hnum;
  HostSvrEntry* cur_tail;

  NSDL2_HTTP(vptr, NULL, "Method called");
  //if (global_settings->debug &&  (global_settings->module_mask & FUNCTION_CALL_OUTPUT))
    NSDL1_HTTP(vptr, NULL, "Entering add_to_hlist: hnum=%d, hel->num_url = %d, hel->first_url = %p", 
                            hel->svr_ptr->idx, hel->num_url, hel->first_url);

  hnum = hel->svr_ptr->idx;

  NSDL2_HTTP(vptr, NULL, "hnum = %d, hel->svr_ptr = %p, vptr->hptr = %p", hnum, hel->svr_ptr, vptr->hptr);
  hptr = vptr->hptr + hnum;
  hptr->hurl_left = hel->num_url;
  NSDL2_HTTP(vptr, NULL, "hptr = %p, hptr->hurl_left = %d, hel->first_url = %p", hptr, hptr->hurl_left, hel->first_url);
  /* This cur_url_head should not point to the shared data structure, it should point to the allocated 
   * data structure which is freed later in case of repeat inline urls.
   */
  //hptr->cur_url = hptr->cur_url_head = hel->first_url;
  hptr->cur_url = hel->first_url;

  //add to global reuse list of the vuser
  cur_tail = vptr->tail_hlist;
  if (cur_tail)
    cur_tail->next_hlist = (struct HostSvrEntry *)hptr;
  else
    vptr->head_hlist = hptr;

  hptr->prev_hlist = (struct HostSvrEntry *)cur_tail;
  hptr->next_hlist = NULL;
  vptr->tail_hlist = hptr;
  hptr->http_mode =  runprof_table_shr_mem[vptr->group_num].gset.http_settings.http_mode;
  hptr->num_parallel = 0; /*bug 54315 init num_parallel with zero*/
  //if (global_settings->debug && (global_settings->module_mask & FUNCTION_CALL_OUTPUT))
  NSDL1_MISC(vptr, NULL, "Exiting add_to_hlist ,vptr[%p]->head_hlist=%p", vptr, vptr->head_hlist);
}

void
dump_parallel(VUser *vptr)
{
  connection* next;
  int ii;
  HostSvrEntry *hptr;
  
  NSDL2_CONN(vptr, NULL, "Method Called, cnum=%d cmax = %d ps_cmax=%d",  vptr->cnum_parallel, vptr->cmax_parallel, vptr->per_svr_max_parallel);

  next = vptr->head_creuse;
  NSDL3_CONN(vptr, NULL, "Global reuse list:  vptr->head_creuse=%p",  vptr->head_creuse);
  while (next) {
    NSDL3_CONN(vptr, NULL, "cptr=%p fd=%d state=%d started=%u connected=%d",
                            next, next->conn_fd,
                            next->conn_state, next->started_at, next->connect_time);
    next = (connection*)next->next_reuse;
  }
  for (ii=0; ii<= g_cur_server; ii++) {
    hptr = vptr->hptr + ii;
    if (hptr->num_parallel) {
      NSDL3_CONN(vptr, NULL, "Server#%d num_parallel=%d: ", ii, hptr->num_parallel);
      next = hptr->svr_con_head;
      while (next) {
	NSDL3_CONN(vptr, NULL, "%p",next);
	next = (connection*)next->next_svr;
     }
      // NSDL3_CONN(vptr, NULL, "\n");
    }
  }
}

//Only used for executing first URL of first page
//in case of REUSE USER
int
try_url_on_cur_con (connection* cptr, action_request_Shr* cur_url, u_ns_ts_t now)
{
  int cur_host, last_host;
  VUser *vptr;
  int done = 0;

  //if (global_settings->debug && (global_settings->module_mask & FUNCTION_CALL_OUTPUT))
  NSDL2_CONN(NULL, cptr, "Method called: cptr = %p cur_url=%p at %u", cptr,cur_url, now);

  vptr = cptr->vptr;

  cur_host = get_svr_ptr(cur_url, vptr)->idx;
  //  last_host = get_svr_ptr(cptr->url_num, vptr)->idx;
  last_host = cptr->gServerTable_idx; /* taking from saved idx since we might have freed cptr->url_num in case of redirecion */

  NSDL2_CONN(NULL, cptr, "cptr->conn_state = %d, cur_host = %d, last_host = %d", cptr->conn_state, cur_host, last_host);
  if (cptr->conn_state == CNST_REUSE_CON) {
    if (cur_host == last_host) {
      SEND_URL_REQ(cptr, cur_url, vptr, now, done);
      //cptr->url_num = cur_url;
    } else {
      add_to_reuse_list(cptr);
    }
  } else if (cptr->conn_state == CNST_FREE) {   /* This might have been allocated for reuse and think timer */
    free_connection_slot(cptr, now);
  } else if (cptr->conn_state == CNST_WS_IDLE) {
      NSDL3_CONN(NULL, cptr, "try_url_on_cur_con(): Connection is in ws_idle state");
  } else {
    fprintf(stderr, "try_url_on_cur_con(): Connection is in wrong state %d\n", cptr->conn_state);
    NSDL2_SCHEDULE(NULL, cptr, "try_url_on_cur_con(): Connection is in wrong state %d", cptr->conn_state);
    //cptr->redirect_url_num = redirect_url_num;
    retry_connection(cptr, now, NS_REQUEST_CONFAIL);
    done = 1;
  }

  NSDL2_CONN(NULL, cptr, "Exiting : try_url_on_cur_con: done =%d ", done);

  return done;
}

/* This method is will make and send inline requests on http2 connection. In case of http2, NS will not wait for the response after sending 
 * request, it will send multiple inline request on single connection
*/
void http2_send_inline_req(VUser *vptr, connection *cptr, HostSvrEntry *hptr, u_ns_ts_t now){

  action_request_Shr* cur_url = NULL;

  NSDL2_HTTP2(vptr, cptr, "Method Called, hptr->hurl_left = %d", hptr->hurl_left);
  if(cptr->conn_state == CNST_CONNECTING){
    NSDL2_HTTP2(vptr, cptr, "Http2 connection with state CNST_CONNECTING");
    return;
  }
  // Send all the requests that are left   
  while(hptr->hurl_left > 0){
    /*When total_open_streams reaches max_concurrent_stream we will return and will make request again when streams get freed*/
    if(cptr->http2 && (cptr->http2->total_open_streams == cptr->http2->settings_frame.settings_max_concurrent_streams)){
      NSDL2_HTTP2(vptr, cptr, "total_open_streams reached max_concurrent_stream ,total_open_streams = %d max_concurrent_stream = %d, hence returning", cptr->http2->total_open_streams, cptr->http2->settings_frame.settings_max_concurrent_streams);
      return;
    }
    NSDL2_HTTP2(vptr, cptr, "Http2 connection, going to send request on new streams for pending lines of this host");
    // Done to allocate it new for new request
    cur_url = hptr->cur_url;
    cptr->cptr_data = NULL;
    cptr->cur_buf = NULL;
    cptr->buf_head = NULL;
    cptr->bytes = 0;
    cptr->total_bytes = 0;
    SET_URL_NUM_IN_CPTR(cptr, cur_url);
    CHECK_AND_SET_INLINE_BLOCK_TIME
    hurl_done(vptr, cur_url, hptr, now);
    // Set as is needed instart socket, it may not set while this method is called frorm try_url_on_any_con
    cptr->conn_state = CNST_REUSE_CON;
    renew_connection(cptr, now);
  }
}

//Make sure the con_state is REUSE prior to calling this function
int
try_hurl_on_cur_con (connection* cptr, u_ns_ts_t now)
{
  int cur_host;
  VUser *vptr;
  HostSvrEntry *hptr;
  action_request_Shr* cur_url=NULL;
  int done = 0;

  NSDL2_CONN(NULL, cptr, "Method called: cptr = %p at %u gServerTable_idx=%d", cptr, now, cptr->gServerTable_idx);

  vptr = cptr->vptr;

  //cur_host = get_svr_ptr(cptr->url_num, vptr)->idx;
  cur_host = cptr->gServerTable_idx; /* taking from saved idx since we might have freed cptr->url_num in case of redirecion */

  hptr = vptr->hptr + cur_host;

  NSDL2_CONN(NULL, cptr, "hptr->hurl_left = %d, cur_host = %d, hptr->cur_url = %p, hptr = %p, "
                         "cptr->http_protocol = %d, cur_host = %d, is_url_parameterized = %d", 
                          hptr->hurl_left, cur_host, hptr->cur_url, hptr, cptr->http_protocol, cur_host, cptr->url_num->is_url_parameterized);
  //Bug: In AutoFetch url
  if ((hptr->hurl_left > 0) && (!cptr->url_num->is_url_parameterized)) {
    cur_url = hptr->cur_url;
    SET_URL_NUM_IN_CPTR(cptr, cur_url);
    //cptr->url_num = cur_url;
    CHECK_AND_SET_INLINE_BLOCK_TIME
    hurl_done(vptr, cur_url, hptr, now);
    if (hptr->hurl_left == 0)
      next_remove_from_hlist(vptr, vptr->hptr+cur_host);
    /*If a request is served from HTTP2 server push conn_state is set to CNST_HTTP2_WRITING and close_connection() is called, so it comes           here again for sending inline request. Here conn_state must be set to CNST_REUSE_CON as done in http2_send_inline_req()*/
    if(cptr->http2 && cptr->conn_state == CNST_HTTP2_WRITING)
      cptr->conn_state = CNST_REUSE_CON;

    renew_connection(cptr, now);
    done = 1;
    // This method will send multiple inline requests on same connection for http2 
    if(cptr->http_protocol == HTTP_MODE_HTTP2){
      http2_send_inline_req(vptr, cptr, hptr, now);
    }
  } else {
    if(cptr->http2)
      NSDL1_CONN(NULL, cptr, "cptr->http_protocol = %d cptr->http2->total_open_streams = %d", cptr->http_protocol, cptr->http2->total_open_streams);
    if(!(cptr->http_protocol == HTTP_MODE_HTTP2 && cptr->http2 && cptr->http2->total_open_streams))
      add_to_reuse_list(cptr);
  }

  NSDL1_CONN(NULL, cptr, "Exiting : try_hurl_on_cur_con: done=%d cur_url=%p", done, cur_url);

  return done;
}


//Execute as many URL's as possible for the cur_host
//on any avilable connections. Firstly try to reuse
//idling connections, then try to find free connection
//and than try to free up exiting idle connections
//and create new connections in their place for
//cur_host
int
try_hurl_on_any_con (VUser *vptr, HostSvrEntry* cur_host, u_ns_ts_t now)
{
  action_request_Shr* cur_url;
  connection* cptr = NULL;
  IW_UNUSED(int done = 0);

  NSDL2_CONN(vptr, NULL, "Method started: cur_host=%p at %u cur_host->hurl_left = [%d], is_url_parameterized = %d",
                          cur_host, now, cur_host->hurl_left, cur_host->cur_url->is_url_parameterized);

#ifdef NS_DEBUG_ON
#ifndef RMI_MODE
    dump_parallel(vptr);
#endif
#endif

  char *loc_url;
  int  loc_url_len;
  action_request_Shr *loc_url_num;
  SvrTableEntry_Shr *svrptr = NULL;
    // Use the REUSE connection, if one for this host is available
  while (cur_host->hurl_left > 0) 
  {
    if(cur_host->cur_url->is_url_parameterized)
    {
      NSDL2_CONN(vptr, NULL, "Host is parameterized");
      if((loc_url_num = process_segmented_url(vptr, cur_host->cur_url, &loc_url, &loc_url_len)) == NULL)
      { 
        NSTL2(NULL, NULL, "Start Socket: Unknown host.");
        vptr->urls_awaited--;
        vptr->urls_left--;
        NSDL2_CONN(vptr, cptr, "vptr->urls_awaited = %d, vptr->urls_left = %d ",
                                                        vptr->urls_awaited, vptr->urls_left,cur_host);
        if(!vptr->urls_awaited)
        {
          int status = NS_REQUEST_ERRMISC;
          INC_HTTP_FETCHES_STARTED_COUNTER(vptr);
          INC_HTTP_HTTPS_NUM_TRIES_COUNTER(vptr); 
          calc_pg_time(vptr, now);
          vptr->last_cptr = NULL;
          HANDLE_URL_PARAM_FAILURE(vptr);
        }
        return 0;
      }
      svrptr = loc_url_num->index.svr_ptr;
      NSDL2_CONN(vptr, NULL, "svrptr->idx = %d", svrptr->idx);
      cptr = remove_head_svr_reuse_list(vptr, vptr->hptr + svrptr->idx);
      if(cptr)
      {
        NSDL2_CONN(vptr, NULL, "loc_url = %s, loc_url_len = %d", loc_url, loc_url_len);
        cptr->url = loc_url;
        cptr->url_len = loc_url_len;
        cur_url = loc_url_num;
        cptr->flags |= NS_CPTR_FLAGS_FREE_URL;
      }
      else
      {
        cur_host->num_parallel--;
        (vptr->hptr + svrptr->idx)->num_parallel++; 
      }
    }
    else
    {
      cptr = remove_head_svr_reuse_list(vptr, cur_host);
    }
    if (cptr == NULL) { //Unused reuse entry not available
      NSDL2_CONN(vptr, cptr, "cptr is NULL.");
      break;
    }  
    SEND_HURL_REQ(cptr, cur_url, cur_host, vptr, now, done);
    // This method will send multiple inline requests on same connection for http2 
    if(cptr->http_protocol == HTTP_MODE_HTTP2){
      http2_send_inline_req(vptr, cptr, cur_host, now);
    }
  }

  // Find a free slot and & start a new connection
  // Firstly check if the num_paralle for this server is not at max
  while (cur_host->hurl_left > 0) {
    NSDL2_CONN(vptr, cptr, "num_parallel = %d, per_svr_max_parallel = %d, hurl_left = %d vptr->cnum_parallel=%d  vptr->cmax_parallel=%d",
			    cur_host->num_parallel, vptr->per_svr_max_parallel, cur_host->hurl_left, vptr->cnum_parallel, vptr->cmax_parallel);
    if (cur_host->num_parallel >= vptr->per_svr_max_parallel) {
      break;
    }
    NSDL2_CONN(vptr, cptr, " cur_host->http_mode = %d, cur_host->num_parallel = %d", cur_host->http_mode, cur_host->num_parallel);
    // In http2 there will be single connection on a host, so restriction is added to for more than one connection   
    if((cur_host->http_mode == HTTP_MODE_HTTP2) && (cur_host->num_parallel > 0)){
      NSDL2_CONN(vptr, cptr, "Current host is marked as http2, and it already has a connection, hence not starting a new connection"
                                      ". cur_host->http_mode = %d, cur_host->num_parallel = %d", cur_host->http_mode, cur_host->num_parallel);

      break;
    }
    if((cur_host->num_parallel > 0) && cur_host->http_mode == HTTP_MODE_AUTO){
      NSDL2_HTTP2(vptr, cptr, "cur_host->http_mode is 0, Do not send further requests on it until it is set to 1 or 2");
      break; 
    }
    if (vptr->cnum_parallel < vptr->cmax_parallel) {
      cptr = get_free_connection_slot(vptr);

      /* Connection pool design: get_free_connection_slot will never return null, this will not happen 
       * in connection pool design unless there is no memory in which case ns is exiting*/
      // Free connection is available
	    cur_host->num_parallel++;
	    vptr->cnum_parallel++;
      if(cur_host->cur_url->is_url_parameterized)
      {  
        NSDL2_CONN(vptr, NULL, "loc_url = %s, loc_url_len = %d", loc_url, loc_url_len);
        cptr->url = loc_url;
        cptr->url_len = loc_url_len;
        cur_url = loc_url_num;
        cptr->flags |= NS_CPTR_FLAGS_FREE_URL;
      }
      SEND_HURL_REQ(cptr, cur_url, cur_host, vptr, now, done);
 
      // This method will send multiple inline requests on same connection for http2 
      if(cptr->http_protocol == HTTP_MODE_HTTP2){
        http2_send_inline_req(vptr, cptr, cur_host, now);
      }
      
    } else { // overall parallel connection limit reached
      // We cannot create any new connection, we must kill some
      // connection for creating a new one
      cptr = remove_head_glb_reuse_list(vptr);
      
      if (cptr == NULL) break; //No idling connection

      //Kill the connection
      close_fd_and_release_cptr (cptr, NS_FD_CLOSE_REMOVE_RESP, now);
      //Start new connection
      cptr = get_free_connection_slot(vptr);
      /* Connection pool design: get_free_connection_slot will never return null, this will not happen 
       * in connection pool design unless there is no memory in which case ns is exiting*/
      cur_host->num_parallel++;
      vptr->cnum_parallel++;

      if(cur_host->cur_url->is_url_parameterized)
      {
        NSDL2_CONN(vptr, NULL, "loc_url = %s, loc_url_len = %d", loc_url, loc_url_len);
        cptr->url = loc_url;
        cptr->url_len = loc_url_len;
        cur_url = loc_url_num;
        cptr->flags |= NS_CPTR_FLAGS_FREE_URL;
      }
      SEND_HURL_REQ(cptr, cur_url, cur_host, vptr, now, done);
      if(cptr->http_protocol == HTTP_MODE_HTTP2){
        http2_send_inline_req(vptr, cptr, cur_host, now);
      }
    }
  }

  //if (global_settings->debug &&  (global_settings->module_mask & FUNCTION_CALL_OUTPUT))
  NSDL2_CONN(vptr, NULL, "Exiting : try_hurl_on_any_con: done =%d ", done);

  return (cur_host->hurl_left);
}

//Only used for executing first URL of first page
//in case of REUSE USER
//firstly  try_url_on_cur_con() is tried, faling
//which this function is used.
int
try_url_on_any_con (VUser *vptr, action_request_Shr* cur_url, u_ns_ts_t now, int need_to_honour_req)
{
  HostSvrEntry *hptr;
  int cur_host;
  int done = 0;
  int loc_url_len;
  char *loc_url;
  action_request_Shr *loc_url_num; 
  connection* cptr;
  SvrTableEntry_Shr *svrptr = NULL; 

  NSDL2_CONN(vptr, NULL, "Method called: cur_url=%p at %u, need_to_honour_req=%d",cur_url, now, need_to_honour_req);
#ifdef NS_DEBUG_ON
    dump_parallel(vptr);
#endif

  if(cur_url->is_url_parameterized)
  {
    NSDL2_CONN(vptr, NULL, "Host is parameterized");
    if((loc_url_num = process_segmented_url(vptr, cur_url, &loc_url, &loc_url_len)) == NULL)
    {
      NSTL2(NULL, NULL, "Start Socket: Unknown host.");
      NSDL2_CONN(vptr, NULL, "Start Socket: Unknown host.");
      int status = NS_REQUEST_ERRMISC;
      INC_HTTP_FETCHES_STARTED_COUNTER(vptr);
      INC_HTTP_HTTPS_NUM_TRIES_COUNTER(vptr); 
      vptr->page_status = NS_REQUEST_URL_FAILURE;
      vptr->sess_status = NS_REQUEST_ERRMISC;
      calc_pg_time(vptr, now);
      tx_logging_on_page_completion(vptr, now);
      vptr->last_cptr = NULL;
      HANDLE_URL_PARAM_FAILURE(vptr);
      return -1;
    }
    svrptr = loc_url_num->index.svr_ptr;
    cur_url = loc_url_num;
  } 
  else
  {
    svrptr = get_svr_ptr(cur_url, vptr); 
  }
  cur_host = svrptr->idx;
  hptr = vptr->hptr + cur_host;
  NSDL2_CONN(vptr, NULL, "svrptr->idx = %d, hptr = %p", svrptr->idx, hptr);
 
  /*bug 54315: in case of redirect, check for the existing cptr and send request*/
 if((cptr = get_inuse_connection(vptr, cur_host)))
 {
   NSDL2_CONN(vptr, NULL, "cptr->conn_state=%d > [%d]", cptr->conn_state,CNST_SSLCONNECTING);
   if(cptr->conn_state > HTTP2_SETTINGS_DONE)
   {
     cptr->conn_state = CNST_REUSE_CON;
     NSDL2_CONN(vptr, NULL,"cptr[%p]->conn_state=%d", cptr, cptr->conn_state);
     return try_url_on_cur_con(cptr, cur_url, now);
   }
 }
  // Use the REUSE connection, if one for this host is available
  cptr = remove_head_svr_reuse_list(vptr, vptr->hptr + cur_host);

  NSDL2_CONN(vptr, NULL, "cptr = %p", cptr);
  if (cptr) { //Unused reuse entry available
    NSDL1_HTTP(vptr, NULL, "get_free_connection_slot cptr=%p, request_type = %d, header_state = %d", 
                              cptr, cur_url->request_type, cptr->header_state);
    NSDL2_CONN(vptr, NULL, "Using REUSE connection from server reuse list cptr=%p as host hptr=%p was available", cptr, hptr);
    if(cur_url->is_url_parameterized)
    {  
      NSDL2_CONN(vptr, NULL, "loc_url = %s, loc_url_len = %d", loc_url, loc_url_len);
      cptr->url = loc_url;
      cptr->url_len = loc_url_len;
      cptr->flags |= NS_CPTR_FLAGS_FREE_URL;
    }

    SEND_URL_REQ(cptr, cur_url, vptr, now, done);
  } else { // Find a free slot and & start a new connection
    // Firstly check if the num_paralle for this server is not at max
      NSDL3_CONN(vptr, NULL, "Connection Limit Details: Current number of connection per host=[%d],"
                     " Max parallel connection per server=[%d], Current number of parallel connection" 
                     "per vuser=[%d], Max connection per vuser=[%d])", 
                     hptr->num_parallel, vptr->per_svr_max_parallel, vptr->cnum_parallel, vptr->cmax_parallel);
    if (hptr->num_parallel < vptr->per_svr_max_parallel) {
      if (vptr->cnum_parallel < vptr->cmax_parallel) {
	      cptr = get_free_connection_slot(vptr);
        /* Connection pool design: get_free_connection_slot will never return NULL, this will not happen 
         * in connection pool design unless there is no memory in which case NS is exiting*/
	      hptr->num_parallel++;
	      vptr->cnum_parallel++;
        NSDL1_CONN(vptr, NULL, "Number of parallel connection on server %s is %d and "
                               "number of parallel connection on VUser %p is %d,  current cptr = %p", 
                                svrptr->server_hostname, hptr->num_parallel, vptr, vptr->cnum_parallel, cptr);
        NSDL1_CONN(vptr, NULL, "is_url_parameterized = %d", cur_url->is_url_parameterized);
        if(cur_url->is_url_parameterized)
        {  
          NSDL2_CONN(vptr, NULL, "loc_url = %s, loc_url_len = %d", loc_url, loc_url_len);
          cptr->url = loc_url;
          cptr->url_len = loc_url_len;
          cptr->flags |= NS_CPTR_FLAGS_FREE_URL;
        }
 
        SEND_URL_REQ(cptr, cur_url, vptr, now, done);
        NSDL1_HTTP(vptr, NULL, "Number of parallel get_free_connection_slot cptr=%p, request_type = %d, header_state = %d", 
                              cptr, cptr->request_type, cptr->header_state);
      } 
      else 
      { // overall parallel connection limit reached
        NSDL2_CONN(vptr, NULL, "Overall parallel connection limit reached");
	cptr = remove_head_glb_reuse_list(vptr);
	if (cptr) 
        {
	  //Kill the connection
	  close_fd_and_release_cptr (cptr, NS_FD_CLOSE_REMOVE_RESP, now);
	  //Start new connection
	  cptr = get_free_connection_slot(vptr);
          /* connection pool design: get_free_connection_slot will never return null, this will not happen 
           * in connection pool design unless there is no memory in which case ns is exiting*/
          hptr->num_parallel++;
	  vptr->cnum_parallel++;
          NSDL1_CONN(vptr, NULL, "Number of parallel connection on server %d and number of parallel connection %d at cptr = %p", 
                                  hptr->num_parallel, vptr->cnum_parallel, cptr);
          if(cur_url->is_url_parameterized)
          {  
            NSDL2_CONN(vptr, NULL, "loc_url = %s, loc_url_len = %d", loc_url, loc_url_len);
            cptr->url = loc_url;
            cptr->url_len = loc_url_len;
            cptr->flags |= NS_CPTR_FLAGS_FREE_URL;
          }
          
          SEND_URL_REQ(cptr, cur_url, vptr, now, done);
	}
      }
    }
    /* Bug#5170: While testing browser setting for walgreens following issue occured,  
     * Selected browser: InternetExplore6.0 with max connection per server was 4 and 
     *                   max connection per vuser was 16
     * 
     * Here embd URL was redirecting to X (some host)and which further redirected to Y (main host), 
     * whereas NS was simultaneously fetching URLs of main host which result in 
     * incrementing number of parallel connections per host.
     * 
     * Above condition failed, number of parallel connection per host was greater than per_svr_max_parallel  
     * Therefore while fetching redirected url using any connection was unable to get new connection slot
     * And NS terminated the test.
     * 
     * In auto redirection browsers honor such redirected urls, even if there connection limit exceed. 
     * Simulated and verified in firefox
     *
     * Hence added check if its auto redirection case and cptr is NULL then we need to get free connection slot 
     * and reset connection type to non keep-alive as it wont be reused
     * 
     * */
    if((done == 0) && (need_to_honour_req == NS_HONOR_REQUEST))
    {
      NS_EL_2_ATTR(EID_CONN_HANDLING,  -1, -1, EVENT_CORE, EVENT_INFORMATION, __FILE__, (char*)__FUNCTION__, 
                   "Maximum parallel connection limit reached and request need to be processed. Extra connection will be made for this request."
                   "Connection Limit Details: " 
                   "Current number of connection per server=%d, Maximum parallel connection per server=%d, "
                   "Current number of parallel connection per vuser=%d, Maximum connection per vuser=%d. ",
                   hptr->num_parallel, vptr->per_svr_max_parallel, vptr->cnum_parallel, vptr->cmax_parallel);

      NSDL3_CONN(vptr, NULL, "Maximum parallel connection limit reached and request need to be processed. Extra connection will be made for this request."
                    "Connection Limit Details: " 
                    "Current number of connection per host=[%d], Max parallel connection per server=[%d], " 
                    "Current number of parallel connection per vuser=[%d], Max connection per vuser=[%d]). ", hptr->num_parallel, vptr->per_svr_max_parallel, vptr->cnum_parallel, vptr->cmax_parallel);

	    cptr = get_free_connection_slot(vptr);
      // Here we must increment these so that more new connections are made only when some are closed
      // and these do not become -ve when decreamented after close connection
	    hptr->num_parallel++;
	    vptr->cnum_parallel++;
      cptr->connection_type = NKA;
      cptr->num_ka = 0;
      SEND_URL_REQ(cptr, cur_url, vptr, now, done);
      NSDL3_CONN(vptr, NULL, "cptr=%p, url=%s", cptr, cptr->url);
    }
  }

  NSDL1_CONN(vptr, NULL, "Exiting : try_url_on_any_con: done =%d at cptr = %p", done, cptr);

  return done;
}
/*bug 54315: The method retrieved cptr of HTTP2 type
 from cinuse list for a particular host*/
static connection* get_inuse_connection(VUser *vptr, int host_idx)
{
  connection *cptr = vptr->head_cinuse;
  NSDL1_CONN(vptr, NULL, "Method called. host_idx=%d cptr=%p", host_idx, cptr);
  for (; cptr != NULL; )
  {
    NSDL2_CONN(vptr, NULL,"cptr[%p]->gServerTable_idx=%d cptr->http_protocol=%d cptr->conn_state=%d", cptr, cptr->gServerTable_idx, cptr->http_protocol,cptr->conn_state);
    if((cptr->gServerTable_idx == host_idx) && (cptr->http_protocol == HTTP_MODE_HTTP2) )
      break;

    cptr = (connection *)cptr->next_inuse;
  }
  NSDL1_CONN(vptr, NULL, "Exit..cptr=%p", cptr);
  return cptr;
}
