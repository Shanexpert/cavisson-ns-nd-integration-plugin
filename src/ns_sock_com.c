/*********************************************************************
* Name: ns_sock_com.c
* Purpose: Socket Communication related  functions
* Author: Archana
* Intial version date: 14/12/07
* Last modification date: 28/08/17
*********************************************************************/

#include <regex.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <arpa/inet.h>
#include "decomp.h"
#include "nslb_time_stamp.h"
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

#include "netstorm.h"
#include "ns_sock_list.h"
#include "ns_msg_com_util.h"
#include "ns_string.h"
#include "nslb_sock.h"
#include "poi.h"
#include "src_ip.h"
#include "unique_vals.h"
#include "divide_users.h"
#include "divide_values.h"
#include "child_init.h"
#include "amf.h"
#include "deliver_report.h"
#include "wait_forever.h"
#include "ns_sock_com.h"
#include "netstorm_rmi.h"
#include "ns_child_msg_com.h"
#include "ns_log.h"
#include "ns_log_req_rep.h"
//#include "ns_handle_read.h"
#include "ns_ssl.h"
#include "ns_wan_env.h"
#include "ns_url_req.h"
#include "ns_debug_trace.h"
#include "ns_alloc.h"
#include "ns_auto_redirect.h"
#include "ns_replay_access_logs.h"
#include "ns_vuser.h"
#include "ns_schedule_phases_parse.h"
#include "ns_gdf.h"
#include "ns_schedule_pause_and_resume.h"
#include "ns_page.h"
#include "ns_smtp_send.h"
#include "ns_smtp.h"
#include "ns_pop3.h"
#include "ns_ftp.h"
#include "ns_ldap.h"
#include "ns_imap.h"
#include "ns_ftp_send.h"
#include "ns_ftp_parse.h"
#include <arpa/nameser.h>
#include <arpa/nameser_compat.h>
#include "nslb_dns.h"
#include "ns_dns.h"
#include "ns_event_log.h"
#include "ns_keep_alive.h"
#include "ns_event_id.h"
#include "ns_http_process_resp.h"
#include "ns_http_cache.h"
#include "ns_http_cache_store.h"
#include "ns_http_pipelining.h"
#include "ns_vuser_trace.h"
#include "ns_page_dump.h"

#include "ns_http_hdr_states.h"
#include "ns_socket_api_int.h"        //for defn of debug_log_user_socket_data
#include "ns_proxy_server.h"
#include "ns_connection_pool.h"
#include "ns_http_auth.h"
#include "ns_proxy_server_reporting.h"
#include "ns_dns_reporting.h"
#include  "ns_parent.h"
#include "ns_group_data.h"
#include "nslb_util.h"
#include "comp_decomp/nslb_comp_decomp.h"
#include "ns_jrmi.h"
#include "ns_websocket.h"
#include "ns_h2_req.h"
#include "ns_websocket_reporting.h"
#include "ns_exit.h"
#include "util.h"
#include "output.h"
#include "ns_xmpp.h"
#include "ns_embd_objects.h"
#include "ns_auto_fetch_embd.h"
#include "ns_dynamic_hosts.h"
#include "ns_h2_stream.h"
#include "ns_h2_header_files.h"
#include "ns_data_handler_thread.h"
#include "ns_url_resp.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"
#include "ns_vuser_ctx.h"
#include "ns_socket.h"
#include "ns_child_thread_util.h"
#include "ns_socket.h"

#include "nslb_date.h"

extern char g_ns_wdir[];
char line_break[] = "\n------------------------------------------------------------\n";
int line_break_length = sizeof(line_break) - 1; //sizeof gives one extra

unsigned int bind_fail_count = 0;
#define USEONCE_VALUE_OVER -10
#define IS_FILE_ZERO(x)  (x < 5)
#define IS_FILE_ONE(x)  ((x > 4) && (x < 11))
#define IS_FILE_TWO(x)  ((x > 10) && (x < 20))
#define IS_FILE_THREE(x)  ((x > 19) && (x < 38))
#define IS_FILE_FOUR(x)  ((x > 37) && (x < 73))
#define IS_FILE_FIVE(x)  ((x > 72) && (x < 85))
#define IS_FILE_SIX(x)  ((x > 84) && (x < 92))
#define IS_FILE_SEVEN(x)  ((x > 91) && (x < 97))
#define IS_FILE_EIGHT(x)  ((x > 96) && (x < 100))

#define LDAP_BUFFER_SIZE (1024*1024)

extern stream *get_stream(connection *cptr, unsigned int strm_id);
action_request_Shr* get_url_svr_ptr(VUser *vptr, char *url, char *request, action_request_Shr *url_num)
{
  char hostname[256 + 1];
  int request_type, port, gserver_shr_idx;
  int hostname_len;
  unsigned short rec_server_port; //Sending Dummy

  action_request_Shr *cur_url;
  NSDL1_HTTP(vptr, NULL, "Method called. vptr = %p, url = %s,  request_type = %d " , vptr, url, url_num->request_type);

  if(extract_hostname_and_request(url, hostname, request, &port, &request_type, NULL, url_num->request_type) < 0) 
  {
    NSTL1(vptr, NULL, "Got malformed url = %s", url);
    return NULL;
  }
                                               
  NSDL3_HTTP(vptr, NULL, "url = %s, Extracted: hostname = %s, port = %d, request_type = %d", url, hostname, port, request_type);

  if (hostname[0] == '\0') { 
    NSTL1(vptr, NULL, "Error: Invalid hostname = %s", url);
    return NULL;
  }
  /* Case 2) Host name was given but port was missing, hence updating port with respect to request type of main URL*/
  if (port == -1) {
    if (request_type == HTTPS_REQUEST || request_type == WSS_REQUEST)
      port = 443;
    else
      port = 80;
  }

  hostname_len = find_host_name_length_without_port(hostname, &rec_server_port);
  gserver_shr_idx = find_gserver_shr_idx(hostname, port, hostname_len);

  if (gserver_shr_idx == -1)
  {
    gserver_shr_idx = add_dynamic_hosts (vptr, hostname, port, 2, 0, request_type, url, vptr->sess_ptr->sess_name, vptr->cur_page->page_name, vptr->user_index, runprof_table_shr_mem[vptr->group_num].scen_group_name);
    if (gserver_shr_idx < 0)
    {
      NSTL1(vptr, NULL, "Error: Invalid dynamic idx = %d", gserver_shr_idx);
      return NULL;
    }
  }
  
  MY_MALLOC(cur_url, sizeof(action_request_Shr), "url_num.param_url",-1);
  memcpy(cur_url, url_num, sizeof(action_request_Shr));
  cur_url->request_type = request_type; 
  cur_url->index.svr_ptr = &gserver_table_shr_mem[gserver_shr_idx];
  cur_url->is_url_parameterized = NS_URL_PARAM_VAL; //Need to free after use
  if(!url_num->parent_url_num)
    cur_url->parent_url_num = url_num;
  NSDL3_HTTP(vptr, NULL, "gserver_shr_idx = %d", gserver_shr_idx);
  return cur_url;
}

//New Function
action_request_Shr *process_segmented_url(VUser *vptr, action_request_Shr *url_num, char **url, int *url_len)
{
  int consumed_vector = 0;
  int len = 0, i;
  static char request[MAX_LINE_LENGTH];

  NSDL2_SOCKETS(vptr, NULL, "Method called, url_num = %p, vptr = %p", url_num, vptr);
 
  NS_RESET_IOVEC(g_scratch_io_vector);

  if (url_num->request_type == HTTP_REQUEST || url_num->request_type == HTTPS_REQUEST)
  {
    consumed_vector = insert_segments(vptr, NULL, &(url_num->proto.http.url_without_path), &g_scratch_io_vector,  
                                      &len, 0, 1, REQ_PART_REQ_LINE, url_num, SEG_IS_NOT_REPEAT_BLOCK);

    NSDL2_SOCKETS(vptr, NULL, "consumed_vector = %d", consumed_vector);
    if(consumed_vector < 1)
    {
      NSTL1(vptr, NULL, "Error in insert_segments of url");
      return NULL;
    }

    consumed_vector = insert_segments(vptr, NULL, &(url_num->proto.http.url), &g_scratch_io_vector, 
                                      &len, 0, 1, REQ_PART_REQ_LINE, url_num, SEG_IS_NOT_REPEAT_BLOCK); 

    NSDL2_SOCKETS(vptr, NULL, "consumed_vector = %d", consumed_vector);

    for(i = 0; i < consumed_vector; i++)
    {
      len += g_scratch_io_vector.vector[i].iov_len; 
    }
 
    if(len > ns_nvm_scratch_buf_size)
    {
      ns_nvm_scratch_buf_size = len ; 
      MY_REALLOC(ns_nvm_scratch_buf, ns_nvm_scratch_buf_size + 1, "ns_nvm_scratch_buf", -1);
    } 
    get_key_ivec_buf(consumed_vector, &g_scratch_io_vector, &(url_num->proto.http.url_without_path), ns_nvm_scratch_buf, ns_nvm_scratch_buf_size);
  }
  else if (url_num->request_type == WS_REQUEST || url_num->request_type == WSS_REQUEST)
  {
    consumed_vector = insert_segments(vptr, NULL, &(url_num->proto.ws.uri_without_path), &g_scratch_io_vector, 
                                      &len, 0, 1, REQ_PART_REQ_LINE, url_num, SEG_IS_NOT_REPEAT_BLOCK);

    NSDL2_SOCKETS(vptr, NULL, "consumed_vector = %d", consumed_vector);
    if(consumed_vector < 1)
    {
      NSTL1(vptr, NULL, "Error in insert_segments of url");
      return NULL;
    }

    consumed_vector = insert_segments(vptr, NULL, &(url_num->proto.ws.uri), &g_scratch_io_vector, 
                                      &len, 0, 1, REQ_PART_REQ_LINE, url_num, SEG_IS_NOT_REPEAT_BLOCK);

    NSDL2_SOCKETS(vptr, NULL, "consumed_vector = %d", consumed_vector);

    for(i = 0; i < consumed_vector; i++)
    {
      len += g_scratch_io_vector.vector[i].iov_len;
    }

    if(len > ns_nvm_scratch_buf_size)
    {
      ns_nvm_scratch_buf_size = len ;
      MY_REALLOC(ns_nvm_scratch_buf, ns_nvm_scratch_buf_size + 1, "ns_nvm_scratch_buf", -1);
    }
    get_key_ivec_buf(consumed_vector, &g_scratch_io_vector, &(url_num->proto.ws.uri_without_path), ns_nvm_scratch_buf, ns_nvm_scratch_buf_size);
  }
  else if((url_num->request_type == SOCKET_REQUEST) || (url_num->request_type == SSL_SOCKET_REQUEST))
  {
    consumed_vector = insert_segments(vptr, NULL, &(url_num->proto.socket.open.remote_host), &g_scratch_io_vector,
                                      &len, 0, 1, REQ_PART_REQ_LINE, url_num, SEG_IS_NOT_REPEAT_BLOCK);

    NSDL2_SOCKETS(vptr, NULL, "consumed_vector = %d", consumed_vector);
    if(consumed_vector < 1)
    {
      NSTL1(vptr, NULL, "Error in insert_segments of url");
      return NULL;
    }

    for(i = 0; i < consumed_vector; i++)
    {
      len += g_scratch_io_vector.vector[i].iov_len;
    }

    if(len > ns_nvm_scratch_buf_size)
    {
      ns_nvm_scratch_buf_size = len ;
      MY_REALLOC(ns_nvm_scratch_buf, ns_nvm_scratch_buf_size + 1, "ns_nvm_scratch_buf", -1);
    }
    get_key_ivec_buf(consumed_vector, &g_scratch_io_vector, &(url_num->proto.socket.open.remote_host), ns_nvm_scratch_buf, ns_nvm_scratch_buf_size);
  }

  url_num = get_url_svr_ptr(vptr, ns_nvm_scratch_buf, request, url_num);

  if(!url_num)
  {
    NSTL1(vptr, NULL, "Error invalid url = %s", ns_nvm_scratch_buf);
    return NULL;
  }

  SvrTableEntry_Shr* svr_ptr = url_num->index.svr_ptr;
  NSDL2_SOCKETS(vptr, NULL, "svr_ptr = %p, idx = %d, server_hostname = %s", svr_ptr, svr_ptr->idx, svr_ptr->server_hostname);
  if(!svr_ptr)
  {
    NSTL1(vptr, NULL, "Error svr_ptr is NULL, for url = %s", ns_nvm_scratch_buf);
    return NULL;
  }
  *url_len = strlen(request);
  NSDL2_SOCKETS(vptr, NULL, "url_len = %d, svr_ptr = %p", *url_len, svr_ptr);

  char *loc_url;
  MY_MALLOC(loc_url, *url_len + 1, "cptr->url", -1); 

  *url = loc_url;
  char *p, *query = NULL;

  NSDL2_SOCKETS(vptr, NULL, "request = [%s]", request);
  if((p = strchr(request,'/')))
  {
    p++;
    if((*p != '?') && (*p != '#'))
    {
      if((query = strstr(p,"/?")) || (query = strstr(p,"/#")))
      {
        *query = '\0'; 
        query++;
        (*url_len)--;
      }
    }
  }
  strcpy(*url, request);
  if(query)
  {
    strcat(*url, query);
  }
  
  NS_FREE_RESET_IOVEC(g_scratch_io_vector);
  NSDL3_HTTP(vptr, NULL, "parameterized_url, url = %s", *url);
  return url_num; 
}

inline void free_dns_cptr(connection *cptr){
  NSDL2_SOCKETS(NULL, NULL, "Method called");
  u_ns_ts_t now = get_ms_stamp(); 
  remove_select(cptr->conn_fd);
  close(cptr->conn_fd);
  FREE_AND_MAKE_NULL(cptr->url_num, "free'ng dns nonblock url_num", -1);
  free_connection_slot(cptr, now);
}

 

int start_udp_socket( connection* cptr, u_ns_ts_t now );

/* ------ Start: Code related to Optimize Ether Flow ------ */

/*
   This function is to set socket option so that connection will be
   closed by sending RST instead of FIN.
   It returns value but will not be used as it is doing end_test_run in
   case of error
*/
int inline set_socketopt_for_close_by_rst(int fd)
{
  NSDL2_SOCKETS(NULL, NULL, "Method called");
  if(global_settings->optimize_ether_flow & OPTIMIZE_CLOSE_BY_RST)
  {
    //printf("set_socketopt_for_close_by_rst() - Setting socket option for closing by RST using SO_LINGER, fd = %d\n", fd);
    struct linger lgr;
    lgr.l_onoff = 1;
    lgr.l_linger = 0;
    if(setsockopt(fd, SOL_SOCKET, SO_LINGER, (char *) &lgr, sizeof(lgr)) < 0)
    {
      fprintf(stderr, "Error: set_socketopt_for_close_by_rst() - Error in setting SO_LINGER option. Socket fd = %d, errno = %d\n", fd, errno);
      perror("set_socketopt_for_close_by_rst");
      return -1;
    }
    /*int len = sizeof(int);
    if (getsockopt(fd, SOL_SOCKET, SO_LINGER, (void *) &lgr, &len ) < 0)
    {
      fprintf(stderr, "Error: set_socketopt_for_close_by_rst() - Error in getting SO_LINGER option. Socket fd = %d, errno = %d\n", fd, errno);
      return -1;
    }
    printf("set_socketopt_for_close_by_rst() - Getting socket option for closing by RST using SO_LINGER, lgr.l_onoff = %d, lgr.l_linger = %d, length = %d\n", lgr.l_onoff, lgr.l_linger, len);*/
  }
  return 0;
}

/*
  This function is to clear/Unclear Quick Ack.
  If flag = 0 then it will set socket option to clear and if flag = 1
  then it will set socket option to unclear for Quick Ack
*/
static int inline set_socketopt_quickack(int fd, int flag)
{
  NSDL1_SOCKETS(NULL, NULL, "Setting socket option for quickack using TCP_QUICKACK, flag = %d, fd = %d", flag, fd);

  if(setsockopt(fd, IPPROTO_TCP, TCP_QUICKACK, (char *)&flag, sizeof(flag) ) < 0)
  {
    fprintf(stderr, "Error: set_socketopt_quickack() - Error in setting TCP_QUICKACK option. Socket fd = %d, errno = %d\n", fd, errno);
    perror("set_socketopt_quickack");
    return -1;
  }
  /*int len = sizeof(int);
  if(getsockopt(fd, IPPROTO_TCP, TCP_QUICKACK, (char *)&flag, &len ) < 0)
  {
    fprintf(stderr, "Error: set_socketopt_quickack() - Error in getting TCP_QUICKACK option. Socket fd = %d, errno = %d\n", fd, errno);
    return -1;
  }
  printf("set_socketopt_quickack() - Getting socket option for quickack using TCP_QUICKACK, flag = %d, length = %d\n", flag, len);*/
  return 0;
}

/*
  This function will set socket option to clear or unclear quick ack.
  after_connect is 0 when called before connect. It is 1 when called
  after connect
*/
int inline set_socketopt_for_quickack(int fd, int after_connect)
{
  NSDL2_SOCKETS(NULL, NULL, "Method called");
  if(after_connect == 0) // Called before connection is done
  {
    if(global_settings->optimize_ether_flow & OPTIMIZE_HANDSHAKE_MERGE_ACK)
    {
      //printf("set_socketopt_for_quickack() - Setting socket option to clear quickack before connection, fd = %d\n", fd);
      return(set_socketopt_quickack(fd, 0));  // Clear Quick Ack
    }
  }
  else // Called after connect is done
  {
    if((global_settings->optimize_ether_flow & OPTIMIZE_HANDSHAKE_MERGE_ACK) && !(global_settings->optimize_ether_flow & OPTIMIZE_DATA_MERGE_ACK))
    {
      //printf("set_socketopt_for_quickack() - Setting socket option to unclear quickack after connection, fd = %d\n", fd);
      return(set_socketopt_quickack(fd, 1));  // Unclear Quick Ack
    }
    if(!(global_settings->optimize_ether_flow & OPTIMIZE_HANDSHAKE_MERGE_ACK) && (global_settings->optimize_ether_flow & OPTIMIZE_DATA_MERGE_ACK))
    {
      //printf("set_socketopt_for_quickack() - Setting socket option to clear quickack after connection, fd = %d\n", fd);
      return(set_socketopt_quickack(fd, 0));  // Clear Quick Ack
    }
  }
  return 0;
}

int set_optimize_ether_flow (char *buf, int runtime_flag, char *err_msg)
{
  char keyword[100];
  int num, value1, value2, value3;

  NSDL2_SOCKETS(NULL, NULL, "Method called");
  if((num = sscanf(buf, "%s %d %d %d", keyword, &value1, &value2, &value3)) != 4)
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, OPTIMIZE_ETHER_FLOW_USAGE, CAV_ERR_1011070, CAV_ERR_MSG_1);

  if(((value1 < 0) || (value1 > 1)) || ((value2 < 0) || (value2 > 1)) || ((value3 < 0) || (value3 > 1)))
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, OPTIMIZE_ETHER_FLOW_USAGE, CAV_ERR_1011070, CAV_ERR_MSG_3);

  if(value1 == 1)
    global_settings->optimize_ether_flow |= OPTIMIZE_HANDSHAKE_MERGE_ACK;
  if(value2 == 1)
    global_settings->optimize_ether_flow |= OPTIMIZE_DATA_MERGE_ACK;
  if(value3 == 1)
    global_settings->optimize_ether_flow |= OPTIMIZE_CLOSE_BY_RST;
  
  return 0;
}

/* ------ End: Code related to Optimize Ether Flow ------ */
inline void free_cptr_vector_idx(connection *cptr, int idx)
{
  char *free_array = cptr->free_array;
  char *iov_base_ptr;

  NSDL2_HTTP(NULL, NULL, "Method called vector idx=%d ",idx);
  if(free_array[idx] & (NS_IOVEC_HTTP2_FRAME_FLAG | NS_IOVEC_FREE_FLAG))
  {
    if (idx == cptr->first_vector_offset)
    {
      iov_base_ptr = cptr->last_iov_base;
      cptr->last_iov_base = NULL;
      NSDL2_HTTP(NULL, NULL, "idx[%d] == cptr->last_iov_base[%d] iov_base_ptr=%p", idx, cptr->first_vector_offset,iov_base_ptr);
    }
    else
    {
      iov_base_ptr = cptr->send_vector[idx].iov_base;
      NSDL2_HTTP(NULL, NULL, "idx[%d] != cptr->last_iov_base[%d] iov_base_ptr=%p ", idx, cptr->first_vector_offset,iov_base_ptr);
    }
 
    if(free_array[idx] & NS_IOVEC_FREE_FLAG)
    {
      NSDL2_HTTP(NULL, NULL, "calling FREE_AND_MAKE_NOT_NULL_EX for vector idx=%d iov_base_ptr=%p", idx,iov_base_ptr);
      FREE_AND_MAKE_NOT_NULL_EX(iov_base_ptr, cptr->send_vector[idx].iov_len, "cptr->send_vector[idx].iov_base", idx);
    }
    else {
      NSDL2_HTTP(NULL, NULL, "calling release_frame for vector idx=%d iov_base_ptr=%p", idx,iov_base_ptr);
      release_frame((data_frame_hdr *)iov_base_ptr);
    }
  }
}
// This method will free iov_base of cptr->send_vectors, cptr->free_array and cptr->send_vector
void free_cptr_send_vector(connection *cptr, int num_vectors)
{
  int j;

  NSDL2_HTTP(NULL, NULL, "Method called");
  for (j = cptr->first_vector_offset; j < num_vectors; j++) 
  {
    free_cptr_vector_idx(cptr, j);
    NSDL3_HTTP(NULL, cptr, "num = %d size = %d", j, cptr->send_vector[j].iov_len);
  }
  FREE_AND_MAKE_NULL(cptr->free_array, "cptr->free_array", -1);
  FREE_AND_MAKE_NULL(cptr->send_vector, "cptr->send_vector", -1);
}

void idle_connection( ClientData client_data, u_ns_ts_t now )
{
  char buf[128];
  int sts;
  connection* cptr;
  cptr = client_data.p;
  VUser* vptr = cptr->vptr;

  NSDL2_SOCKETS(vptr, cptr, "Method called");

  if(vptr->vuser_state == NS_VUSER_PAUSED){
    vptr->flags |= NS_VPTR_FLAGS_TIMER_PENDING; 
    return;
  }

  if(cptr->conn_state == CNST_FREE){
    NS_EL_2_ATTR(EID_CONN_HANDLING,  -1, -1, EVENT_CORE, EVENT_WARNING, __FILE__, (char*)__FUNCTION__,
                   "idle_connection called for connection (%p) which is already in connection free list. Ignored ", cptr);
    return;
  }

  sts = read( cptr->conn_fd, buf, 128 );
  if (sts > 0) buf[sts]=0;
  else buf[0]=0;
  NSDL1_CONN(NULL, cptr, "TIMEOUT: read fd=%d, sts=%d err=%s buf=%s", 
                         cptr->conn_fd, sts, nslb_strerror(errno), buf);

  //Decrementing connect_failure in case of connection timeout as its getting incremented in close_fd
  if (cptr->conn_state < CNST_MIN_CONFAIL) {
    Close_connection( cptr , 0, now, NS_REQUEST_CONFAIL, NS_COMPLETION_TIMEOUT);
  } else {
    if (cptr->http2_state == HTTP2_CON_PREFACE_CNST) 
      cptr->free_array = NULL; 
    
    if(cptr->free_array){
      // In case if time out if request is not completely written, then cptr->send_vector will have allocated vector, we need to free it
      if(cptr->url_num->request_type == HTTP_REQUEST){
        free_cptr_send_vector(cptr, cptr->num_send_vectors); 
      } else if(cptr->url_num->request_type == HTTPS_REQUEST){
        ssl_free_send_buf(cptr);
      }
    }
    
    NSDL1_CONN(NULL, cptr, "TIMEOUT:3 content_length=%d, req_code_filled=%d"
                           " req_code=%d bytes=%d, tcp_in=%u cur=%u"
                           " first=%d start=%llu connect_time=%d"
                           " write=%d",
                           cptr->content_length, cptr->req_code_filled,
                           cptr->req_code, cptr->bytes, cptr->tcp_bytes_recv,
                           get_ms_stamp(), cptr->first_byte_rcv_time,
                           cptr->started_at, cptr->connect_time, cptr->write_complete_time);

    // Following code is to retry on timeout 
    if(cptr->url_num->request_type == HTTP_REQUEST || cptr->url_num->request_type == HTTPS_REQUEST){
      if(runprof_table_shr_mem[((VUser* )(cptr->vptr))->group_num].gset.retry_on_timeout == RETRY_ON_TIMEOUT_OFF){
        NSDL3_CONN(NULL, cptr, "RETRY_ON_TIMEOUT is OFF, going to close connection"); 
        if(cptr->http_protocol == HTTP_MODE_HTTP2 && cptr->http2 && cptr->url_num->proto.http.type == EMBEDDED_URL){
            stream *stream_ptr = NULL;
            stream_ptr = get_stream(cptr, cptr->http2->last_stream_id);
            if(stream_ptr){
              if (cptr->http2->last_stream_id > 1)
                copy_stream_to_cptr(cptr, stream_ptr);
              int loc_state = cptr->conn_state;
              pack_and_send_reset_frame(cptr, stream_ptr, CANCEL_STREAM, now);
              cptr->conn_state = loc_state;
             // RELEASE_STREAM(cptr, stream_ptr);
              cptr->http2->last_stream_id += 2;
              if(cptr->http2->total_open_streams)
                cptr->conn_state = CNST_HTTP2_WRITING;
                cptr->http2->donot_release_cptr = 1;
              Close_connection( cptr , 1, now, NS_REQUEST_TIMEOUT, NS_COMPLETION_TIMEOUT); 
              //if(cptr->http2->total_open_streams == 0)
                //cptr->http2->donot_release_cptr = 0;
              return;
            }
        }
        else
          Close_connection( cptr , 0, now, NS_REQUEST_TIMEOUT, NS_COMPLETION_TIMEOUT); 
      } else if (runprof_table_shr_mem[((VUser* )(cptr->vptr))->group_num].gset.retry_on_timeout == RETRY_ON_TIMEOUT_SAFE){
        if(cptr->url_num->proto.http.http_method == HTTP_METHOD_GET || cptr->url_num->proto.http.http_method == HTTP_METHOD_HEAD 
           || cptr->url_num->proto.http.http_method == HTTP_METHOD_CONNECT){
          NSDL3_CONN(NULL, cptr, "RETRY_ON_TIMEOUT is ON for only SAFE Methods, going to retry connection"); 
          retry_connection(cptr, now, NS_REQUEST_TIMEOUT); 
        }else {
          Close_connection( cptr , 0, now, NS_REQUEST_TIMEOUT, NS_COMPLETION_TIMEOUT);
        } 
      }else {
         NSDL3_CONN(NULL, cptr, "RETRY_ON_TIMEOUT is ON for ALL Methods, going to retry connection"); 
         retry_connection(cptr, now, NS_REQUEST_TIMEOUT); 
      }

      NSDL3_CONN(NULL, cptr, "actual_timeout = %d", cptr->timer_ptr->actual_timeout); 
      if(cptr->timer_ptr->actual_timeout < runprof_table_shr_mem[((VUser* )(cptr->vptr))->group_num].gset.idle_secs){
        NSDL3_CONN(NULL, cptr, "At timeout trace"); 
        NSTL1(NULL, cptr, "Response Timeout Occurs");
      }

      return;
    }

    Close_connection( cptr , 0, now, NS_REQUEST_TIMEOUT, NS_COMPLETION_TIMEOUT);
  }
}

static int inline
ns_strcpy(char* dest, const char* source) {
  int i;

  NSDL2_SOCKETS(NULL, NULL, "Method called");
  if (!source)
    return 0;

  for (i = 0; *source != '\0'; i++, dest++, source++) {
    *dest = *source;
  }
  *dest = '\0';

  return i;
}

/***
 *** class_conv converts a random number from 0 to 99 to a class number
 ***/

static
int class_conv(int num)  {
  NSDL2_SOCKETS(NULL, NULL, "Method called");
  if (IN_CLASS_ZERO(num))
    return 0;
  else {
    if (IN_CLASS_ONE(num))
      return 1;
    else {
      if (IN_CLASS_TWO(num))
        return 2;
      else {
        if (IN_CLASS_THREE(num))
          return 3;
      }
    }
  }
  return 0;
}

/***
 *** file_conv converts a random number from 0 to 99 to a file number
 ***/
static
int file_conv(int num) {
  NSDL2_SOCKETS(NULL, NULL, "Method called");
  if (IS_FILE_ZERO(num))
    return 0;
  else {
    if (IS_FILE_ONE(num))
      return 1;
    else {
      if (IS_FILE_TWO(num))
        return 2;
      else {
        if (IS_FILE_THREE(num))
          return 3;
        else {
          if (IS_FILE_FOUR(num))
            return 4;
          else {
            if (IS_FILE_FIVE(num))
              return 5;
            else {
              if (IS_FILE_SIX(num))
                return 6;
              else {
                if (IS_FILE_SEVEN(num))
                  return 7;
                else
                  if (IS_FILE_EIGHT(num))
                    return 8;
              }
            }
          }
        }
      }
    }
  }
  return 0;
}

void free_all_vectors(connection *cptr) 
{
  int j;

  NSDL1_HTTP(NULL, cptr, "Method Called, num_vectors = %d", cptr->num_send_vectors);
  
  if(cptr->send_vector == NULL)
  {
    NSDL1_HTTP(NULL, cptr, "send_vector is NULL, returning.");
    return;
  }
  
  for(j = cptr->first_vector_offset; j < cptr->num_send_vectors; j++)
  {
    NSDL3_HTTP(NULL, cptr, "num = %d size = %d", j, cptr->send_vector[j].iov_len);
    free_cptr_vector_idx(cptr,j);
  }
}

inline void save_referer(connection* cptr) {
  VUser* vptr = cptr->vptr;
  char* refer_ptr = NULL;
  int proto_len;

  NSDL2_SOCKETS(vptr, cptr, "Method called");
  if ((cptr->url_num->proto.http.type != MAIN_URL) || 
      (!(runprof_table_shr_mem[vptr->group_num].gset.enable_referer & REFERER_ENABLED)) ||
      ((status_code[cptr->req_code].status_settings & STATUS_CODE_REDIRECT) && (!(runprof_table_shr_mem[vptr->group_num].gset.enable_referer & CHANGE_REFERER_ON_REDIRECT)))||
      (!(status_code[cptr->req_code].status_settings & STATUS_CODE_REDIRECT) && (status_code[cptr->req_code].status_settings & STATUS_CODE_DONT_CHANGE_REFERER)))  /* save the old url request for the referer */
  return;

  if (cptr->url != NULL) { // cptr_url should be present always, this check is just for safety 

    // Now make new one
    if (cptr->url_num->request_type == HTTP_REQUEST) {
      proto_len = 7;
    } else {
      proto_len = 8;
    }

    /* Format of Referer is  http://10.10.70.2:9014/tours/index.html 
       where http://          tells the protocol type
             10.10.70.2:9014  is the server name
             tours/index.html is the cptr->url
             at last we add CRLF \r\n to mark the end of the referer header.  */
    // Calculate complete referer size so that vptr->referer can be allocated
    vptr->referer_size = proto_len + cptr->old_svr_entry->server_name_len + cptr->url_len + 2;

    if (vptr->referer_size > vptr->referer_buf_size)
    {
      MY_REALLOC(vptr->referer, vptr->referer_size + 1, "vptr->referer", -1); //Adding 1 for NULL
      vptr->referer_buf_size = vptr->referer_size;
    }

    // Allocate new referer  
    refer_ptr = vptr->referer;

    // Copy protocol of referer 
    if (cptr->url_num->request_type == HTTP_REQUEST) {
      memcpy(refer_ptr, "http://", 7);
      refer_ptr += 7;
    } else {
      memcpy(refer_ptr, "https://", 8);
      refer_ptr += 8;
    }
    // Copy host of referer
    memcpy(refer_ptr, cptr->old_svr_entry->server_name, cptr->old_svr_entry->server_name_len);
    refer_ptr += cptr->old_svr_entry->server_name_len;

    // Copy url of referer
    memcpy(refer_ptr, cptr->url, cptr->url_len);
    refer_ptr += cptr->url_len;

    // Copy CRLF at the end of referer 
    memcpy(refer_ptr, "\r\n", 2);
    
    vptr->referer[vptr->referer_size] = '\0';
  
    NSDL2_SOCKETS(vptr, cptr, "vptr->referer_size = %d, vptr->referer = %s", vptr->referer_size, vptr->referer);
  }
  else {
    NSEL_MAJ(NULL, cptr, ERROR_ID, ERROR_ATTR, "Error: cptr->url == %s", cptr->url);
  }
}

//this function checks for numeric decimal ip vs char stream hostname
//TODO: right now basic check is performed.....need to perform more checks and validate it
//need to validate for ipv6 also...currently for ipv4 only
/*
int check_hostname_vs_ip(char *svr_name){
  int ret;
  char *ptr;
  char c1[128],c2[128], c3[128], c4[128], c5[128];
  int num; 

  int num = sscanf(svr_name, "%s.%s.%s.%s.%s", c1, c2, c3, c4, c5);
  NSDL2_SOCKETS(NULL, NULL, "Method called, nummm = [%d] val1 = [%s] val2 = [%s] val3 = [%s] val4 = [%s]", num, c1, c2, c3, c4);

  if(num != 4)
   return 2; //assume that it's not in dotted decimal format ....but in ipv6 it may be
 
  if((num == 4) && ns_is_numeric(c1) && ns_is_numeric(c2) && ns_is_numeric(c3) && ns_is_numeric(c4)) //i.e 10.10.70.3
    return 1; 
  
  if((ptr = strrchr(svr_name, '.')) != NULL)
    ptr++; 

  ret = ns_is_numeric(ptr);
  if(ret)
    return 1;  //last octet must not be in decimal format
  else
    return 2; //hostname(string)

}*/
static int fill_sockaddr_if_resolve(connection *cptr, VUser *vptr, PerHostSvrTableEntry_Shr* svr_entry)
{
  char *cur_server_name = svr_entry->server_name; // mapped server
  int cur_server_len = svr_entry->server_name_len; // mapped server len
  char *tmp;
  int len;
  int i;
  unsigned short port;
  u_ns_ts_t remaining_ttl = 0;

  NSDL2_SOCKETS(NULL, NULL, "Method called. Host to resolve(actual server)= %s,host name length= %d", svr_entry->server_name, cur_server_len);

  //Memset socket address
  memset(&(cptr->cur_server.sin6_addr), 0, sizeof(struct sockaddr_in6));

  if(runprof_table_shr_mem[vptr->group_num].gset.dns_caching_mode == USE_DNS_CACHE_FOR_TTL)
  {
    remaining_ttl = get_ms_stamp() - svr_entry->last_resolved_time;  /*finding remaining time-to-live for a host*/
    NSDL2_SOCKETS(NULL, NULL, "CACHE TTL Mode remaining_ttl = %lld, last_resolved_time = %lld, svr_entry addr = %p", remaining_ttl, svr_entry->last_resolved_time, svr_entry);
    if(remaining_ttl < runprof_table_shr_mem[vptr->group_num].gset.dns_cache_ttl)
    {
      INCREMENT_DNS_LOOKUP_CUM_SAMPLE_COUNTER(vptr);
      INCREMENT_DNS_LOOKUP_FROM_CACHE_COUNTER(vptr);
      NSDL2_SOCKETS(NULL, NULL, "dns_lookup_from_cache = %u, dns_lookup_per_sec = %u", dns_lookup_stats_avgtime->dns_from_cache_per_sec,
                                 dns_lookup_stats_avgtime->dns_lookup_per_sec);
      memcpy(&(cptr->cur_server), &(svr_entry->saddr), sizeof(struct sockaddr_in6));
      if (runprof_table_shr_mem[vptr->group_num].gset.dns_debug_mode == 1)
        dns_resolve_log_write(g_partition_idx, "C", svr_entry->server_name, 0, &cptr->cur_server);
      return 1;
    }
    else
    {
      return 0;
    }
  }


  // Adjust lenght if server name has port as we need to check server name without port
  if ((tmp = strrchr(cur_server_name, ':')) != NULL)
  {
    // take len for server name only
    cur_server_len = tmp - cur_server_name;
    tmp++; 
    port = atoi(tmp);
    NSDL2_SOCKETS(NULL, NULL, "New length of host removing port = %d, extracted port = %hu", cur_server_len, port);
  } else {
    if (cptr->url_num->request_type == HTTP_REQUEST)
      port = 80; 
    else if (cptr->url_num->request_type == HTTPS_REQUEST)
      port = 443;
    NSDL2_SOCKETS(NULL, NULL, "For request type = %d setting default port = %hu", cptr->url_num->request_type, port); 
  }
  
  // Now search in ustable is server is already resolved or not
  for(i = 0; i < total_svr_entries; i++) 
  {
    if(vptr->ustable[i].svr_ptr == NULL) {
      NSDL2_SOCKETS(NULL, NULL, "vptr->ustable[%d].svr_ptr is NULL. It is not mapped, hence continue here", i);
      continue;
    }

    if(vptr->usr_entry[i].resolve_saddr.sin6_port == 0) {
      NSDL2_SOCKETS(NULL, NULL, " Host at index %d is not resolved", i);
      continue;
    }

    len = vptr->ustable[i].svr_ptr->server_name_len;

    if((tmp = strrchr(vptr->ustable[i].svr_ptr->server_name, ':')) != NULL)  
      len = (tmp - vptr->ustable[i].svr_ptr->server_name);

    if(len == cur_server_len && !strncmp(vptr->ustable[i].svr_ptr->server_name, cur_server_name, len)){
      NSDL2_SOCKETS(NULL, NULL, "Found host resolved host at index %d for server %s with address = %p", i, vptr->ustable[i].svr_ptr->server_name, &vptr->usr_entry[i].resolve_saddr);
      memcpy(&(cptr->cur_server), &vptr->usr_entry[i].resolve_saddr, sizeof(struct sockaddr_in6));
      NSDL2_SOCKETS(vptr, cptr, "Server is already resolved with address = %p. Setting port to %hu", &cptr->cur_server, port);
      cptr->cur_server.sin6_port = ntohs(port);  // Must set port as resolved address has port used on intial resolution of server
      if (runprof_table_shr_mem[vptr->group_num].gset.dns_debug_mode == 1)
        dns_resolve_log_write(g_partition_idx, "C", vptr->ustable[i].svr_ptr->server_name, 0, &cptr->cur_server);
      return 1;
    }
  } 
  return 0;

}

#if 0
static int get_nameserver(char *str)
{
  int i;
  NSDL2_SOCKETS(NULL, NULL, "Method called, server = %s", str);

  for(i=0; i < _res.nscount; i++){
    if (inet_ntop(AF_INET, &_res.nsaddr_list[i].sin_addr, str, 50) == NULL) {
      fprintf(stderr, "Error: res_init failed to initialize state structure\n");
      end_test_run();
    } else{
      NSDL2_SOCKETS(NULL, NULL, "nameserver = %s", str);
      return 0;
    }
  }
  return -1;
}
#endif

int do_dns_lookup(connection *cptr, u_ns_ts_t now){
  int select_port;
  char *ptr = NULL;
  PerHostSvrTableEntry_Shr* svr_entry;
  VUser *vptr = ((connection*)(cptr->conn_link))->vptr;
  
  NSDL1_SOCKETS(NULL, cptr, "Method called");

  svr_entry = ((connection*)(cptr->conn_link))->old_svr_entry;

  /* Set timouts for all protocals*/
  //set_cptr_for_new_req(cptr, vptr, now);

  if (runprof_table_shr_mem[vptr->group_num].gset.use_same_netid_src) {
    if (svr_entry->net_idx != vptr->user_ip->net_idx) {
      vptr->user_ip = get_src_ip(vptr, svr_entry->net_idx);
    }
  }

  //Commented NULL from debug as need to pass vptr and cptr
  //NS_DT4(NULL, NULL, DM_L1, MM_CONN,  "allocated src ip = [%s]", nslb_sock_ntop((struct sockaddr *)&(vptr->user_ip->ip_addr)));
  NS_DT4(vptr, cptr, DM_L1, MM_CONN,  "allocated src ip = [%s]", nslb_sock_ntop((struct sockaddr *)&(vptr->user_ip->ip_addr)));

  NSDL3_HTTP(NULL, cptr, "port_index = [%d] min_port=[%d], max_port=[%d] total_ip_entries = [%d] src_ip=[%s]", my_port_index, v_port_table[my_port_index].min_port, v_port_table[my_port_index].max_port, total_ip_entries, nslb_sock_ntop((struct sockaddr *)&(vptr->user_ip->ip_addr)));

  if(cptr->url_num->proto.dns.proto == USE_DNS_ON_TCP )
  {
    cptr->conn_fd = get_socket(AF_INET, vptr->group_num);

    BIND_SOCKET((char*)&(vptr->user_ip->ip_addr), 
                    v_port_table[my_port_index].min_port,
                    v_port_table[my_port_index].max_port);

    int flag = 1;
    if (setsockopt( cptr->conn_fd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag) ) < 0) {
      fprintf(stderr, "Error: Setting fd to TCP no-delay failed\n");
      perror( get_url_req_url(cptr) );
      end_test_run();
    }

#ifdef NS_USE_MODEM
    if (global_settings->wan_env) // WAN is Enabled then set 
      set_socket_for_wan(cptr, svr_entry);
#endif
  }else{
    cptr->conn_fd = get_udp_socket(AF_INET, vptr->group_num);
    BIND_SOCKET((char*)&(vptr->user_ip->ip_addr), 
                    v_port_table[my_port_index].min_port,
                    v_port_table[my_port_index].max_port);
    NSDL3_HTTP(NULL, cptr, "udp_socket fd = [%d]", cptr->conn_fd);
  }

  if (fcntl(cptr->conn_fd, F_SETFL, O_NONBLOCK) < 0){
    fprintf(stderr, "Error: Setting fd to nonblock failed\n"); 
    perror( get_url_req_url(cptr->conn_link) ); 
    end_test_run(); 
  }

  if (add_select(cptr, cptr->conn_fd, EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLET) < 0) {
    fprintf(stderr, "Error: Set Select failed on WRITE EVENT\n");
    end_test_run();
  }
#if 0
  char svr_name[64 + 1];
  if(get_nameserver(svr_name) == -1){   //this method will get the source ip(nameserver) from /etc/resolv.conf??we will fill default port 
    fprintf(stderr, "Error: Unable to get the server name to send DNS resolve request\n");
    end_test_run();  
  }

  if (inet_ntop(AF_INET, &_res.nsaddr_list[i].sin_addr, str, 50) == NULL) {
    fprintf(stderr, "Error: res_init failed to initialize state structure\n");
    end_test_run();
#endif

  char svr_name[64 + 1];
  //TODO: need to check for number of DNS servers
  memset((char *)&cptr->conn_server, 0, sizeof(struct sockaddr_in6));
  struct sockaddr_in *sin = (struct sockaddr_in *) &cptr->conn_server;
  sin->sin_family = AF_INET;
  sin->sin_port = htons(53);
  //sin->sin_addr.s_addr = _res.nsaddr_list[0].sin_addr;
  sin->sin_addr = _res.nsaddr_list[0].sin_addr;

  if( cptr->url_num->proto.dns.proto == USE_DNS_ON_TCP ){
TRY_CONNECT:
  if (connect( cptr->conn_fd, (SA *) &(cptr->conn_server), sizeof(struct sockaddr_in6)) < 0 ) {
      if ( errno == EINPROGRESS ) {
        cptr->conn_state = CNST_CONNECTING;
        NSDL3_HTTP(NULL, cptr, "setting conn_state to CNST_CONNECTING fd = [%d]", cptr->conn_fd);
#ifndef USE_EPOLL
        FD_SET( cptr->conn_fd, &g_wfdset );
#endif
        return 1;
    }
    else if ( errno == EALREADY) {
       // cptr->conn_state = CNST_CONNECTING;
        NSDL3_HTTP(NULL, cptr, "setting conn_state to CNST_ALREADY. It should not come here.");
#ifndef USE_EPOLL
        FD_SET( cptr->conn_fd, &g_wfdset );
#endif
        goto TRY_CONNECT;
        //return -1;
      } else if (errno == ECONNREFUSED) {
        fprintf(stderr, "Error: start_socket(): Connection refused from server %s\n", nslb_get_src_addr(cptr->conn_fd));
        FREE_AND_MAKE_NULL(cptr->url_num, "free'ng dns nonblock url_num", -1);
        connection *http_cptr = cptr->conn_link; 
        free_dns_cptr(cptr);
        FREE_AND_MAKE_NULL(cptr->data, "Freeing cptr data", -1);
        http_cptr->conn_link = NULL;
        dns_http_close_connection(http_cptr, 0, now, 1, NS_COMPLETION_NOT_DONE);
        return -1;
      } else {
        char srcip[128];
        average_time->num_con_fail++; // Increament Number of TCP Connection failed
        strcpy (srcip, nslb_get_src_addr(cptr->conn_fd));
        printf("Connection failed at %llu (nvm=%d sess_inst=%u user_index=%u src_ip=%s) to %s   error=[%s]\n", 
                now, my_child_index, vptr->sess_inst, vptr->user_index, srcip, nslb_sock_ntop((SA *)&cptr->cur_server), nslb_strerror(errno));
        //Earlier ips[cur_ip_entry].port
        perror( get_url_req_url(cptr) );
        retry_connection(cptr, now, NS_REQUEST_CONFAIL); 
        return -1;
      }
    }
  }

 
  if((ptr = strchr(svr_entry->server_name, ':')) != NULL){
    strncpy(svr_name, svr_entry->server_name, ptr-svr_entry->server_name); 
    svr_name[ptr-svr_entry->server_name] = '\0';
  }
  else
    strcpy(svr_name, svr_entry->server_name);

  dns_make_request(cptr, now, 1, svr_name);
  return 0;
}

inline void
start_new_socket(connection *cptr, u_ns_ts_t now) {
  NSDL2_SOCKETS(NULL, cptr, "Method called, cptr = %p", cptr);
  //Increment fetches started only for first try
  VUser* vptr = cptr->vptr;
  PerHostSvrTableEntry_Shr* svr_entry;
  u_ns_ts_t local_end_time_stamp;
  int ret = 0;

  // on_url_start must be called before start_socket() as it may update variables used in the request
  if(cptr->num_retries == 0)
    on_url_start(cptr, now);

  NSDL2_SOCKETS(vptr, cptr, "REQUEST TYPE= [%d], mode = [%d]", cptr->request_type, runprof_table_shr_mem[vptr->group_num].gset.use_dns);

 
  NSDL2_SOCKETS(vptr, cptr, "request_type = %d, svr_ptr = %p", cptr->url_num->request_type, cptr->url_num->index.svr_ptr); 
 
  //call NONBLOCKING DNS function
  //Need to check in light of HTTP caching. In caching we make url and check if that url is in 
  //cached buffer. Here we are making DNS connection before the freshness checking of cached url.
  //Need to bypass this DNS code
  if((cptr->request_type == HTTP_REQUEST || cptr->request_type == HTTPS_REQUEST) 
      && (runprof_table_shr_mem[vptr->group_num].gset.use_dns == USE_DNS_NONBLOCK)){
 
    if ((svr_entry = get_svr_entry(vptr, cptr->url_num->index.svr_ptr)) == NULL) {
      fprintf(stderr, "Start Socket: Unknown host\n");
      end_test_run();
    } else {
      cptr->old_svr_entry = svr_entry;
    }

    if(runprof_table_shr_mem[vptr->group_num].gset.dns_caching_mode == USE_DNS_CACHE_FOR_SESSION){
          NSDL2_SOCKETS(vptr, cptr, "Use DNS caching mode enabled. Checking if server is already resolved or not");
          ret = fill_sockaddr_if_resolve(cptr, vptr, svr_entry);
    }

    //DNS is not resolved or DNS CACHE is disabled
    if(!ret)
    {   //end
      NSDL2_SOCKETS(vptr, cptr, "Dns is not resolved..........."); 
      //get free cptr for new dns request
      connection *dns_cptr = get_free_connection_slot(vptr);    
      //point DNS and HTTP connection to each other
      dns_cptr->conn_link = cptr;                               
      cptr->conn_link= dns_cptr;

      MY_MALLOC_AND_MEMSET(dns_cptr->url_num, sizeof(action_request_Shr), "Allocate memory for nonblock dns share memory", -1);
      dns_cptr->request_type = dns_cptr->url_num->request_type = DNS_REQUEST;

      if(runprof_table_shr_mem[vptr->group_num].gset.dns_conn_type == 0){
        dns_cptr->url_num->proto.dns.proto = USE_DNS_ON_UDP;
      }
      else{
        dns_cptr->url_num->proto.dns.proto = USE_DNS_ON_TCP;
      }

     //check if address is already resolved or not
     
      cptr->ns_component_start_time_stamp = get_ms_stamp();//Set NS component start time  
      NSDL2_SOCKETS(vptr, cptr, "Time before resolving host, start_time = %u", cptr->ns_component_start_time_stamp);
      INCREMENT_DNS_LOOKUP_CUM_SAMPLE_COUNTER(vptr);

      int ok = do_dns_lookup(dns_cptr, now);
      if(ok == 1)
        return;

      if(!ok){
       /******* moved this code after dns response come*******/
        //local_end_time_stamp = get_ms_stamp();//set end time
        //cptr->dns_lookup_time = local_end_time_stamp - cptr->ns_component_start_time_stamp; //time taken while resolving host
        //cptr->ns_component_start_time_stamp = local_end_time_stamp;//Update component time. 
        //UPDATE_DNS_LOOKUP_TIME_COUNTERS(cptr->dns_lookup_time);
        //cache resolved address if cache is enabled for session     
       }else{
         INCREMENT_DNS_LOOKUP_FAILURE_COUNTER(vptr);
         local_end_time_stamp = get_ms_stamp();//time taken to resolve host
         cptr->dns_lookup_time = local_end_time_stamp - cptr->ns_component_start_time_stamp;//DNS lookup time diff
         cptr->ns_component_start_time_stamp = local_end_time_stamp; //Update ns_component_start_time_stamp
         
         SET_MIN (average_time->url_dns_min_time, cptr->dns_lookup_time);
         SET_MAX (average_time->url_dns_max_time, cptr->dns_lookup_time);
         average_time->url_dns_tot_time += cptr->dns_lookup_time;
         average_time->url_dns_count++;

         UPDATE_GROUP_BASED_NETWORK_TIME(dns_lookup_time, url_dns_tot_time, url_dns_min_time, url_dns_max_time, url_dns_count);

         UPDATE_DNS_LOOKUP_TIME_COUNTERS(vptr, cptr->dns_lookup_time);

         if(IS_TCP_CLIENT_API_EXIST)
           fill_tcp_client_avg(vptr, DNS_TIME, cptr->dns_lookup_time);

         if(IS_UDP_CLIENT_API_EXIST)
           fill_udp_client_avg(vptr, DNS_TIME, cptr->dns_lookup_time);

         if (runprof_table_shr_mem[vptr->group_num].gset.dns_debug_mode == 1)
           dns_resolve_log_write(g_partition_idx, "NR", svr_entry->server_name, cptr->dns_lookup_time, &cptr->cur_server);
      }
      return;
    }
  }
 
  if(!(cptr->request_type == DNS_REQUEST &&  cptr->url_num->proto.dns.proto == USE_DNS_ON_UDP) &&
     !((cptr->request_type == SOCKET_REQUEST || cptr->request_type == SSL_SOCKET_REQUEST) &&
        cptr->url_num->proto.socket.open.protocol == UDP_PROTO))
     start_socket(cptr, now );
  else
     start_udp_socket(cptr, now );

#if 0
/** We need to increament num_connections when succsefule - To be Discussed with Anil */
  if ( cptr->conn_state != CNST_FREE ) {
    // ++num_connections;
    //if ( num_connections > max_parallel ) max_parallel = num_connections;
  }
#endif
}

int inline get_udp_socket(int family, int grp_idx) {
  int fd;
  int SO_on;

  NSDL2_SOCKETS(NULL, NULL, "Method called, family = %d", family);
  fd = socket(family, SOCK_DGRAM, 0 );

  if ( fd < 0 ) {
    fprintf(stderr, "UDP SOCK Error: %s : NVM/Parent %d was failed to get new socket, error message %s\n", (char*)__FUNCTION__, my_child_index, nslb_strerror(errno));
    END_TEST_RUN
  }

  if (!(runprof_table_shr_mem[grp_idx].gset.disable_reuseaddr))
  {
    SO_on = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *) &SO_on, sizeof(int)) < 0) {
      fprintf(stderr, "Error: %s failed to set REUSEADDR.\n", (char*)__FUNCTION__);
      END_TEST_RUN
    }
  }
  return fd;
}

int inline get_socket(int family, int grp_idx)
{
  int fd;
  int SO_on;

  NSDL2_SOCKETS(NULL, NULL, "Method called, family = %d, grp_idx = %d", family, grp_idx);

  fd = socket( family, SOCK_STREAM, 0);

  if(fd < 0)
  {
    fprintf(stderr, "TCP SOCK Error: NVM/Parent %d was failed to create socket, error message %s\n", my_child_index, nslb_strerror(errno));
    end_test_run();
  }
  if (!(runprof_table_shr_mem[grp_idx].gset.disable_reuseaddr))
  {
    SO_on = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *) &SO_on, sizeof(int)) < 0)
    {
      fprintf(stderr, "Setting REUSE AADRR\n");
      end_test_run();
    }
  }
  /*
  SO_on = 0;
  slen = sizeof(int);
  if (getsockopt(cptr->conn_fd, SOL_SOCKET, SO_REUSEADDR, (char *) &SO_on, &slen) < 0)
  {
    fprintf(stderr, "Getting REUSE AADRR\n");
    retry_connection(cptr, now, NS_REQUEST_CONFAIL);
    return;
  }
  if (!SO_on)
  {
    printf("Reuse not set\n");
  }
  */
  if(set_socketopt_for_close_by_rst(fd) < 0) end_test_run();
  if(set_socketopt_for_quickack(fd, 0) < 0) end_test_run();
  //fd = g_original_fd;
  /*fd = dup (g_original_fd);
  if (fd < 0)
  {
    fprintf(stderr, "socket() dup failed!!\n");
    end_test_run();
  }*/
  return fd;
}

/***
 *** get_file_request will create a random file_request
***/
static void get_file_request(char * buffer, int type, int dir_num, int* class, int* file)
{
  int class_rand;
  int file_rand;
  int class_num, file_num;

  NSDL2_SOCKETS(NULL, NULL, "Method called");
  class_rand = rand() % 100;
  class_num = class_conv(class_rand);
  file_rand = rand() % 100;
  file_num = file_conv(file_rand);
  /*memset(buffer, 0, SEND_BUFFER_SIZE);*/

  sprintf(buffer, "GET %s/file_set/dir%05d/class%d_%d%s", global_settings->spec_url_prefix, dir_num, class_num, file_num, global_settings->spec_url_suffix);

  if (type == NKA)
    strcat(buffer, " HTTP/1.0\r\n\r\n");
  else
    strcat(buffer, " HTTP/1.0\r\nConnection: Keep-Alive\r\n\r\n");

  *class = class_num;
  *file = file_num;
}

#ifdef USE_EPOLL
int v_epoll_fd;
int el_epoll_fd;

inline int
ns_epoll_init(int *fd)
{
  NSDL2_SOCKETS(NULL, NULL, "Method called");
  if ((*fd = epoll_create(NS_EPOLL_MAXFD)) == -1) {
    perror("epoll_create");
    NSTL1(NULL, NULL, "netstorm: failed to create epoll. Err[%d]: %s", errno, nslb_strerror(errno));
    return -1;
  }
  NSDL2_SOCKETS(NULL, NULL, "Method exit, fd = %d", *fd);
  return 0;
}
// Changing cptr to data As add_select is called from differnt places with different arguments as cptr was causing ambiquity
int
add_select(void* data, int fd, int event)
{
	  struct epoll_event pfd;

  	  NSDL2_SOCKETS(NULL, NULL, "Method called. fd = %d, event = %0x", 
                          fd, event);
          bzero(&pfd, sizeof(struct epoll_event)); //Added after valgrind reported bug
	  //pfd.events = EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLET;
	  pfd.events = event;
	  pfd.data.ptr = (void*) data;
	  if (epoll_ctl(v_epoll_fd, EPOLL_CTL_ADD, fd, &pfd) == -1) {
	    perror("epoll add");
	    printf("netstorm: add epoll failed\n");
	    return -1;
	  } else
	    return 0;
}

inline int remove_select(int fd)
{
  struct epoll_event pfd;

  NSDL2_SOCKETS(NULL, NULL, "Method called, Removing %d from select.", fd);
  if (fd < 0) {
    /* This check added as remove select was getting called for fd = -1. Following is the error: */
    /* unknown socket active: cptr=0x890c5c0 fd=-1 state=0 */
    /* epoll del: Bad file descriptor */
    /* netstorm: del epoll failed */
    /* reset Select failed on READ */
    /* FATAL ERROR: TEST RUN CANCELLED */
    /* kill_all_children() called */

    /* Below code statements is commented due to the bug 63050 */
   // fprintf(stderr, "%s, fd is = %d\n", __FUNCTION__, fd);
    NSTL1(NULL, NULL,"fd is = %d", fd);
    return 0;
  }

  bzero(&pfd, sizeof(struct epoll_event)); //Added after valgrind reported bug
  if (epoll_ctl(v_epoll_fd, EPOLL_CTL_DEL, fd, &pfd) == -1) {

    /*bug 79803 added fd and epoll_fd in tarce*/
    NSTL1(NULL, NULL,"netstorm: Epoll delete failed [%d] = [%s] for fd=%d from v_epoll_fd=%d ",errno, nslb_strerror(errno), fd, v_epoll_fd);

   /* Ignore only ENOENT error, for rest return -1 */
   if(errno != ENOENT)
    return -1;  
  }
   
  return 0;
}

int
mod_select(int sfd, connection* cptr, int fd, int event)
{
	  struct epoll_event pfd;

  	  NSDL2_SOCKETS(NULL, cptr, "Method called. sfd = %d, cptr = %p, fd = %d, event = %d", 
                         sfd, cptr, fd, event);
          bzero(&pfd, sizeof(struct epoll_event)); //Added after valgrind reported bug
	  //pfd.events = EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLET;
	  pfd.events = event;
	  if(global_settings->high_perf_mode)
	  	pfd.data.fd = sfd;
	  else
	  	pfd.data.ptr = (void*) cptr;
	  if (epoll_ctl(v_epoll_fd, EPOLL_CTL_MOD, fd, &pfd) == -1) {
	    perror("epoll mod");
	    printf("netstorm: add epoll failed\n");
	    return -1;
	  } else
	    return 0;
}

#else


fd_set g_rfdset;
fd_set g_wfdset;

static inline void
ns_fd_set( fd_set *rfd,  fd_set *wfd)
{
int cnum;

      NSDL2_SOCKETS(vptr, cptr, "Method called");
      FD_ZERO( rfd );
      FD_ZERO( wfd );
      for ( cnum = 0; cnum < max_connections*ns_factor; ++cnum )
      //for ( cnum = 0; cnum < max_connections; ++cnum ) {
	if (ns_fd_isset(&ns_fdset, connections[cnum].conn_fd))
	  continue;
	switch ( connections[cnum].conn_state ) {
	case CNST_CONNECTING:
	case CNST_WRITING:
	case CNST_SSL_WRITING:
	  FD_SET( connections[cnum].conn_fd, wfd );
	  break;
	case CNST_HEADERS:
	case CNST_READING:
	  FD_SET( connections[cnum].conn_fd, rfd );
	  break;
	}
      //}
}

static inline void
ns_fd_check( fd_set *rfd,  fd_set *wfd, u_ns_ts_t now)
{
int cnum;
      NSDL2_SOCKETS(vptr, cptr, "Method called");

      //for ( cnum = 0; cnum < max_connections*ns_factor; ++cnum )
      for ( cnum = 0; cnum < ultimate_max_connections*ns_factor; ++cnum )
      //for ( cnum = 0; cnum < ultimate_max_connections; ++cnum )
      {
	switch ( connections[cnum].conn_state ) {
	case CNST_CONNECTING:
	  if ( FD_ISSET( connections[cnum].conn_fd, wfd ) ) {
      	    FD_CLR( connections[cnum].conn_fd, &g_wfdset );
	    handle_connect( cnum, now, 1 );
	  }
	  break;
	case CNST_WRITING:
	  if ( FD_ISSET( connections[cnum].conn_fd, wfd ) ) {
      	    FD_CLR( connections[cnum].conn_fd, &g_wfdset );
             if(global_settings->replay_mode)
               send_replay_req_after_partial_write(cnum, now)
             else
	      handle_write( cnum, now ); // Neeraj is cnum same cptr?
	  }
	  break;
        case CNST_LDAP_WRITING:
	  if ( FD_ISSET( connections[cnum].conn_fd, wfd ) ) {
      	    FD_CLR( connections[cnum].conn_fd, &g_wfdset );
            handle_ldap_write(cnum, now);
	  }
	  break;
#ifdef ENABLE_SSL
	case CNST_SSL_WRITING:
	  if ( FD_ISSET( connections[cnum].conn_fd, wfd ) ) {
      	    FD_CLR( connections[cnum].conn_fd, &g_wfdset );
             if(global_settings->replay_mode)
               send_replay_req_after_partial_write(cnum, now)
             else{
              if(IS_HTTP2_INLINE_REQUEST){
	        handle_ssl_write( cnum, now );
                if(cptr->http_protocol == HTTP_MODE_HTTP2 && cptr->url_num->proto.http.type == EMBEDDED_URL){
                  HTTP2_SET_INLINE_QUEUE
                }
              }else
	        handle_ssl_write_ex( cnum, now );
             }
	  }
	  break;
	case CNST_SSLCONNECTING:
	  if ( FD_ISSET( connections[cnum].conn_fd, rfd ) ) {
	    FD_CLR( connections[cnum].conn_fd, &g_rfdset );
	    handle_connect( cnum, now, 0 );
	  }
	  break;
#endif
	case CNST_CHUNKED_READING:
	case CNST_HEADERS:
	case CNST_READING:
	  if ( FD_ISSET( connections[cnum].conn_fd, rfd ) ) {
	    handle_read( cnum, now );
	  }
	  break;
	case CNST_REUSE_CON:
	  if ( FD_ISSET( connections[cnum].conn_fd, rfd) || FD_ISSET( connections[cnum].conn_fd, wfd))
	    if (vusers[connections[cnum].vnum].vuser_state != NS_VUSER_CLEANUP)   {
	      close_fd_and_release_cptr(cnum, NS_FD_CLOSE_REMOVE_RESP, now);
	    }
	  break;
	default: /* case CNST_FREE */
	  if ((connections[cnum].conn_fd != -1) && /* Added check as we are making fd -1 in close_fd - Oct 16 2008 */
              (FD_ISSET( connections[cnum].conn_fd, rfd) || FD_ISSET( connections[cnum].conn_fd, wfd))) {
	    printf("unknown socket active: cnum=%d fd=%d state=%d\n", cnum, connections[cnum].conn_fd, connections[cnum].conn_state);
	    if (vusers[connections[cnum].vnum].vuser_state != NS_VUSER_CLEANUP)   {
              NSDL2_SOCKETS(NULL, NULL, "CNST_FREE case");
	      close_fd_and_release_cptr(cnum, NS_FD_CLOSE_REMOVE_RESP, now);
	    }
	  }
	}
      }
}
#endif

void inline reset_after_100_continue_hdr_sent(connection *cptr)
{
  NSDL3_HTTP(NULL, cptr, "Method called");
  cptr->content_length = -1;
  cptr->conn_state = CNST_HEADERS;
  cptr->header_state = HDST_BOL;
}

/*bug 84661 method: get_vecotr_to_send*/
/*Return ***********************
** > 0 : Number of vecctor to write/send
** = 0 : means not write is required
** < 1 : error
*/
static inline unsigned int h2_get_vecotr_to_send( connection *cptr, stream * sptr, int num_vectors, int *ptr_i)
{
   int sptr_data_to_send = 0;
   NSDL1_HTTP(NULL, cptr,"Method called. num_vectors=%d",num_vectors);
   NSDL3_HTTP(NULL, cptr, " stream[%d] remote_window_size=%d" , sptr->stream_id,sptr->flow_control.remote_window_size);
   /*bug 84661 check if we are not sending data more than defined window*/
   /*updated Handling of flow control*/ 
   if(sptr->flow_control.remote_window_size < 0)
     return HTTP2_ERROR;
   int k = 0;
   int vectors_to_send = 0; 
   for( *ptr_i = cptr->first_vector_offset; *ptr_i < num_vectors; ++(*ptr_i), k++){
     sptr_data_to_send += cptr->send_vector[*ptr_i].iov_len;
     vectors_to_send = k+1;
     NSDL3_HTTP(NULL, cptr, "sptr_data_to_send=%d sptr->flow_control.remote_window_size=%d vectors_to_send=%d i_count=%d",
							sptr_data_to_send,sptr->flow_control.remote_window_size,vectors_to_send,*ptr_i);
     //if(sptr_data_to_send > sptr->flow_control.remote_window_size) {
       /*bug 84661 check if we are not sending data more than defined window*/		 
       if((sptr->tcp_bytes_sent + sptr_data_to_send) >  sptr->window_update_from_server) {
         NSDL3_HTTP(NULL, cptr, "sptr->tcp_bytes_sent[%d] + sptr_data_to_send[%d] > sptr->window_update_from_server=%d vectors_to_send=%d ",
				sptr->tcp_bytes_sent, sptr_data_to_send,sptr->window_update_from_server,vectors_to_send ); 
        sptr_data_to_send -= cptr->send_vector[*ptr_i].iov_len;
        --vectors_to_send;
        --(*ptr_i);
        /*return in case vectors_to_send = 0*/
        if(!vectors_to_send) {
          /*bug 84661 */
          copy_cptr_to_stream(cptr, sptr);
          /*reset state to CNST_HTTP2_READING so that next event can be read for WINDOW_UPDATE*/
          cptr->conn_state = CNST_HTTP2_READING;
        }
        break;
       } 
       //break;
     //}
   }
   
  NSDL3_HTTP(NULL, cptr, "sptr_data_to_send = %d sptr->flow_control.remote_window_size = %d icount = %d vectors_to_send = %d",
                             sptr_data_to_send, sptr->flow_control.remote_window_size, *ptr_i, vectors_to_send);    
  return vectors_to_send;
}
 

/* This method to lopg debug trace on complete writen */
//Only called for http request
int handle_write( connection* cptr, u_ns_ts_t now ) 
{
  struct iovec *vector_ptr;
  int num_vectors;
  int i;
  int bytes_to_send;
  stream *sptr;
  int vectors_to_send ;
  VUser *vptr = (VUser *)cptr->vptr;

  NSDL2_HTTP(vptr, cptr, 
             "Method called, [PartialWrite] vptr = %p, cptr=%p, "
             "cptr{state=%d, http_protocol=%d, "
             "send_vector = %d, first_vector_offset = %d, num_send_vectors = "
             "%d, bytes_left_to_send = %d, content_length = %d}", 
             vptr, cptr, cptr->conn_state,cptr->http_protocol, cptr->send_vector, 
             cptr->first_vector_offset, cptr->num_send_vectors, 
             cptr->bytes_left_to_send, cptr->content_length);

  if(cptr->http_protocol == HTTP_MODE_HTTP2){
    sptr = cptr->http2->http2_cur_stream;
    copy_stream_to_cptr(cptr, sptr);
  }

#ifndef NS_DEBUG_ON
#endif
  struct iovec *start_vector = cptr->send_vector + cptr->first_vector_offset;

  if(!(cptr->flags & NS_HTTP_EXPECT_100_CONTINUE_HDR)) {
     // Num vectors to be written
     NSDL3_HTTP(NULL, cptr, "Setting num vectors, num_send_vectors = %d, bytes_left_to_send = %d",
                             cptr->num_send_vectors, cptr->bytes_left_to_send);
     bytes_to_send = cptr->bytes_left_to_send;
     num_vectors = cptr->num_send_vectors;
  } else { // we have not written header yet in case of 100 Continue
     vector_ptr = start_vector; 
     // Num vectors to be written till body index
     num_vectors = cptr->body_index + 1;  // its an index so to get count added 1
     /* if header is not send than we need to set*/
     bytes_to_send = cptr->bytes_left_to_send - cptr->content_length;
     NSDL3_HTTP(NULL, cptr, "Setting num vectors (as 100 contine response has came)"
			    "body_index = %d, num_send_vectors = %d,"
			    "bytes_left_to_send = %d, content_length = %d",
			    cptr->body_index, num_vectors, cptr->bytes_left_to_send,
			    cptr->content_length);
  }

 if(cptr->http_protocol == HTTP_MODE_HTTP2){
  /*bug 84661 moved code block to get_vecotr_to_send()*/
  if ((vectors_to_send =  h2_get_vecotr_to_send(cptr, sptr, num_vectors, &i) ) <= HTTP2_SUCCESS)
   return HTTP2_ERROR;
 }
 else{
   vectors_to_send = num_vectors - cptr->first_vector_offset;
 }

  NSDL3_HTTP(NULL, cptr, "vectors_to_send = %d", vectors_to_send);
  char *free_array = cptr->free_array;

  if (cptr->bytes_left_to_send == 0) /* nothing to send */
  {
    NSDL3_HTTP(NULL, cptr, "Handle write called with bytes_left_to_send 0. Should not come here");
    return 0;
  }
  /*bug 84661 errno reset to 0*/
  errno = 0;
  // bytes_sent = writev(cptr->conn_fd, start_vector, num_vectors - cptr->first_vector_offset);
  int bytes_sent = writev(cptr->conn_fd, start_vector, vectors_to_send);
  if (bytes_sent < 0) 
  {
    NSEL_MIN(NULL, cptr, ERROR_ID, ERROR_ATTR, "Error[%d]: (%s) in writing vector,"
                                                "sending HTTP request failed fd = %d,"
                                                "num_vectors = %d, %s",
                                                errno,nslb_strerror(errno), cptr->conn_fd, num_vectors,
                                                get_url_req_url(cptr));

    NSDL3_HTTP(NULL, cptr, "Error[%d]: (%s) in writing vector,"
                                                "sending HTTP request failed fd = %d,"
                                                "num_vectors = %d, %s",
                                                errno,nslb_strerror(errno), cptr->conn_fd, num_vectors,
                                                get_url_req_url(cptr));
    free_cptr_send_vector(cptr, cptr->num_send_vectors);

    //TODO: need to rethink again, How other protocol handle this situation
    if (IS_TCP_CLIENT_API_EXIST)
      fill_tcp_client_avg(vptr, NUM_SEND_FAILED, 0);

    //Close_connection(cptr, 0, now, NS_REQUEST_WRITE_FAIL, NS_COMPLETION_NOT_DONE); //ERR_ERR
    retry_connection(cptr, now, NS_REQUEST_WRITE_FAIL); //ERR_ERR
    NSDL2_HTTP(vptr, cptr, "[PartialWrite] Connection retried"); 
    return -2;
  }
  //Reducing the size of connection and stream flow control window
  if(cptr->http_protocol == HTTP_MODE_HTTP2){
    NSDL3_HTTP(NULL, cptr, "Before sptr->flow_control.remote_window_size = %d sptr->http_size_to_send = %d "
                           "cptr->http2->flow_control.remote_window_size = %d",
                            sptr->flow_control.remote_window_size, sptr->http_size_to_send, cptr->http2->flow_control.remote_window_size);
    sptr->flow_control.remote_window_size -= bytes_sent;
    sptr->http_size_to_send -= bytes_sent;
    cptr->http2->flow_control.remote_window_size -= bytes_sent;
    NSDL3_HTTP(NULL, cptr, " After sptr->flow_control.remote_window_size = %d sptr->http_size_to_send = %d "
                           "cptr->http2->flow_control.remote_window_size = %d",
                            sptr->flow_control.remote_window_size, sptr->http_size_to_send, cptr->http2->flow_control.remote_window_size);
  }

  cptr->tcp_bytes_sent += bytes_sent;
  average_time->tx_bytes += bytes_sent;
  NSDL3_HTTP(vptr, cptr, "[PartialWrite] bytes_sent = %d, bytes_to_send = %d", bytes_sent, bytes_to_send);

  if(SHOW_GRP_DATA) {
    avgtime *lol_average_time;
    lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
    lol_average_time->tx_bytes += bytes_sent;
  }

  if (bytes_sent < bytes_to_send) 
  {
    if (IS_TCP_CLIENT_API_EXIST)
      fill_tcp_client_avg(vptr, SEND_THROUGHPUT, cptr->tcp_bytes_sent);

    int amt_writ = 0;
    NSDL4_HTTP(NULL, cptr, "cptr->first_vector_offset = %d, num_vectors = %d", cptr->first_vector_offset ,num_vectors);
    for (i =  cptr->first_vector_offset, vector_ptr = start_vector; i < num_vectors; i++, vector_ptr++) 
    {
      amt_writ += vector_ptr->iov_len;
      if (bytes_sent >= amt_writ) 
      {
        //This vector completely written
        NSDL3_HTTP(NULL, cptr, "This vector completely written");
#ifdef NS_DEBUG_ON
        debug_log_http_req(cptr, vector_ptr->iov_base, vector_ptr->iov_len, 1, 0);
#else
        if(cptr->http_protocol != HTTP_MODE_HTTP2)
          LOG_HTTP_REQ(cptr, vector_ptr->iov_base, vector_ptr->iov_len, 1, 0);
#endif
        free_cptr_vector_idx(cptr, i);
      } 
      else 
      {
        NSDL3_HTTP(NULL, cptr, "This vector partilally written for i = %d", i);
#ifdef NS_DEBUG_ON

       debug_log_http_req(cptr, vector_ptr->iov_base, vector_ptr->iov_len - (amt_writ-bytes_sent), 0, 0);
#else
      if(cptr->http_protocol != HTTP_MODE_HTTP2)
        LOG_HTTP_REQ(cptr, vector_ptr->iov_base, vector_ptr->iov_len - (amt_writ-bytes_sent), 0, 0);
#endif
        //This vector only partilally written
        NSDL4_HTTP(NULL, cptr, "before cptr->last_iov_base=%p,vector_ptr->iov_base=%p",cptr->last_iov_base, vector_ptr->iov_base);
        // Since one vector element can be partial many times, we need to keep it's pointer in 
        // last_iov_based only once which the buffer start address as iov_based keep changing
        // We need to save in last_iov_based in two conditions:
        //   1. If it is not set (it is NULL)
        //   2. If was set but not that vector element is completely writen and another element is paritail
        //   Note condiiton is 2 is also handled by checking NULL as we are freeing last_iov_base and making NULL above
        /* Shalu: we will set last_iov_base only in case if vector need to be free. if we are setting it without checking it will not be null*/
       if((cptr->last_iov_base == NULL) && (free_array[i] & NS_IOVEC_FREE_FLAG))
          cptr->last_iov_base = vector_ptr->iov_base;

        NSDL4_HTTP(NULL, cptr, "after cptr->last_iov_base=%p",cptr->last_iov_base);
        cptr->first_vector_offset = i;
        vector_ptr->iov_base = vector_ptr->iov_base + vector_ptr->iov_len - (amt_writ-bytes_sent);
        vector_ptr->iov_len = amt_writ-bytes_sent;
        cptr->bytes_left_to_send -= bytes_sent;        
        if(cptr->http_protocol == HTTP_MODE_HTTP2){
          copy_cptr_to_stream(cptr, sptr);
          NSDL3_HTTP(NULL, cptr,"sptr->tcp_bytes_sent[%d] +  cptr->http2->settings_frame.settings_max_frame_size[%d] > sptr->window_update_from_server[%d]",sptr->tcp_bytes_sent, cptr->http2->settings_frame.settings_max_frame_size, sptr->window_update_from_server);
          //bug 51330 : reset cptr->conn_state to CNST_HTTP2_READING
          if((sptr->tcp_bytes_sent + cptr->http2->settings_frame.settings_max_frame_size) > sptr->window_update_from_server)
             cptr->conn_state = CNST_HTTP2_READING;
         }
         #ifndef USE_EPOLL
          FD_SET( cptr->conn_fd, &g_rfdset );
#endif
          return -1;
      }
    }
  }
  else
  {
    /* We need to close conn & reload/click away a page if forcefully reloaded/click away i.e. time is 0(reload/clickaway)
     * chk and force reload click away is called twice. Partial write is done then it will be called from handle write else form here
     */
    if (cptr->url_num->proto.http.type == MAIN_URL)
      chk_and_force_reload_click_away(cptr, now);
      int j = cptr->first_vector_offset;
#ifdef NS_DEBUG_ON
    // All vectors written
      for (vector_ptr = start_vector; j < num_vectors; j++, vector_ptr++)
      { 
        if(j == num_vectors - 1) {
          debug_log_http_req(cptr, vector_ptr->iov_base, vector_ptr->iov_len, 1, 1);
        }
        else {
          debug_log_http_req(cptr, vector_ptr->iov_base, vector_ptr->iov_len, 0, 0);
        }
      }
#else
    // All vectors written
      for (vector_ptr = start_vector; j < num_vectors; j++, vector_ptr++)
      { 
        if(j == num_vectors - 1) {
          if(cptr->http_protocol != HTTP_MODE_HTTP2)
            LOG_HTTP_REQ(cptr, vector_ptr->iov_base, vector_ptr->iov_len, 1, 1);
        }
        else {
          if(cptr->http_protocol != HTTP_MODE_HTTP2)
            LOG_HTTP_REQ(cptr, vector_ptr->iov_base, vector_ptr->iov_len, 0, 0);
        }
      }
#endif
  }

  /* We are done with writing in case of Expect: 100-continue,
   * so we need to return as we can not free free_array & some
   * other things done after follwoing block  
   */
  if((cptr->flags & NS_HTTP_EXPECT_100_CONTINUE_HDR) == NS_HTTP_EXPECT_100_CONTINUE_HDR) {
     NSDL3_HTTP(NULL, cptr, "Headers for Expect: 100-continue completely written, returning");
     reset_after_100_continue_hdr_sent(cptr);
     return 0;
  }

  if (cptr->conn_state == CNST_REUSE_CON)
    cptr->req_code_filled = -2;

  free_cptr_send_vector(cptr, num_vectors);
  
  if (cptr->url_num->proto.http.exact)
    SetSockMinBytes(cptr->conn_fd, cptr->url_num->proto.http.bytes_to_recv);

  if (cptr->http_protocol == HTTP_MODE_HTTP2){
    cptr->conn_state = CNST_HTTP2_READING; /*bug 51330: changed _WRITING to READING*/
    sptr->bytes_left_to_send = 0;
  }
  else
    cptr->conn_state = CNST_HEADERS;

  on_request_write_done (cptr);

  //TODO: Move code to on_request_write_done
  if((cptr->request_type == SOCKET_REQUEST) || (cptr->request_type == SOCKET_REQUEST))
  {
    NSDL2_SOCKETS(vptr, cptr, "Socket_Write: Switching to user context, Writing complete");
    if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT)
      switch_to_vuser_ctx(vptr, "Socket Send API Success, switching");
  }
  return 0;
}

//Only called for hhtp request
// This is called only once for the first time parital write
void
handle_incomplete_write(connection* cptr, NSIOVector *ns_iovec, int num_vectors, int bytes_to_send, int bytes_sent)
{
  struct iovec *vector_ptr = ns_iovec->vector; 
  
  struct iovec *start_vector;
  char *start_array;
  int left_vectors;
  int amt_writ;
  int i,j;

  stream *sptr;
  VUser* vptr = cptr->vptr;
#ifndef NS_DEBUG_ON
#endif

  if(cptr->http_protocol == HTTP_MODE_HTTP2){
    sptr = cptr->http2->http2_cur_stream; 
    NSDL3_HTTP(NULL, cptr, "Before sptr->flow_control.remote_window_size = %d sptr->http_size_to_send = %d "
                           "cptr->http2->flow_control.remote_window_size = %d", 
                            sptr->flow_control.remote_window_size, sptr->http_size_to_send, cptr->http2->flow_control.remote_window_size); 
    sptr->flow_control.remote_window_size -= bytes_sent;
    sptr->http_size_to_send -= bytes_sent;
    cptr->http2->flow_control.remote_window_size -= bytes_sent;
    NSDL3_HTTP(NULL, cptr, "After sptr->flow_control.remote_window_size = %d sptr->http_size_to_send = %d "
                           "cptr->http2->flow_control.remote_window_size = %d", 
                            sptr->flow_control.remote_window_size, sptr->http_size_to_send, cptr->http2->flow_control.remote_window_size); 
  }

  NSDL3_HTTP(NULL, cptr, "handle_incomplete_write:cptr=%p, state=%d , num_vectors=%d, bytes_to_send=%d, bytes_sent=%d", cptr, cptr->conn_state, num_vectors, bytes_to_send, bytes_sent);

  amt_writ = 0;
  cptr->bytes_left_to_send = bytes_to_send - bytes_sent;
  for (i = 0; i < num_vectors; i++, vector_ptr++) 
  {
    amt_writ += vector_ptr->iov_len;
    NSDL3_HTTP(NULL, cptr, "i=%d, bytes_sent[%d] > amt_writ[%d], vector_ptr %p vector_ptr->iov_len=%d",
									i, bytes_sent,amt_writ, vector_ptr, vector_ptr->iov_len);
    if (bytes_sent >= amt_writ) 
    {
      //This vector completely written
      NSDL3_HTTP(NULL, cptr, "This vector completely written, cptr->request_type = %d vector idx=%d", cptr->request_type,i);
      switch(cptr->request_type) {
        case HTTP_REQUEST:
        case HTTPS_REQUEST:
        case WS_REQUEST:
        case WSS_REQUEST:
        case SOCKET_REQUEST:
        case SSL_SOCKET_REQUEST:
#ifdef NS_DEBUG_ON
            debug_log_http_req(cptr, vector_ptr->iov_base, vector_ptr->iov_len, 0, 0);
#else
            if(cptr->http_protocol != HTTP_MODE_HTTP2)
              LOG_HTTP_REQ(cptr, vector_ptr->iov_base, vector_ptr->iov_len, 0, 0);
#endif
          break;
#ifdef NS_DEBUG_ON
        case SMTP_REQUEST:
        case SMTPS_REQUEST:
          debug_log_smtp_req(cptr, vector_ptr->iov_base, vector_ptr->iov_len, 0, 0);
          break;
        case POP3_REQUEST:
        case SPOP3_REQUEST:
          debug_log_pop3_req(cptr, vector_ptr->iov_base, vector_ptr->iov_len, 0, 0);
          break;
        case FTP_REQUEST:
          debug_log_ftp_req(cptr, vector_ptr->iov_base, vector_ptr->iov_len, 0, 0);
          break;
        case DNS_REQUEST:
          debug_log_dns_req(cptr, vector_ptr->iov_base, vector_ptr->iov_len, 0, 0);
          break;
        case USER_SOCKET_REQUEST:
          debug_log_user_socket_data(cptr, vector_ptr->iov_base, vector_ptr->iov_len, 0, 0);
          break;
        case IMAP_REQUEST:
        case IMAPS_REQUEST:
          debug_log_imap_req(cptr, vector_ptr->iov_base, vector_ptr->iov_len, 0, 0);
          break;
        case JRMI_REQUEST:
        //  debug_log_jrmi_req(cptr, vector_ptr->iov_base, vector_ptr->iov_len, 0, 0);
          break;
#endif
      }
      NS_FREE_IOVEC(*ns_iovec, i);
    } 
    else
    {
      NSDL3_HTTP(NULL, cptr, "This vector (%d) partilally written, amt_writ = %d, bytes_sent = %d, iov_len = %d, bytes_to_send = %d",
			      i, amt_writ, bytes_sent, vector_ptr->iov_len, bytes_to_send);
      switch(cptr->request_type) {
        case HTTP_REQUEST:
        case HTTPS_REQUEST:
        case WS_REQUEST:
        case WSS_REQUEST:
        case SOCKET_REQUEST:
        case SSL_SOCKET_REQUEST:
#ifdef NS_DEBUG_ON
            debug_log_http_req(cptr, vector_ptr->iov_base, vector_ptr->iov_len - (amt_writ-bytes_sent), 0, 0);
#else
            if(cptr->http_protocol != HTTP_MODE_HTTP2)
              LOG_HTTP_REQ(cptr, vector_ptr->iov_base, vector_ptr->iov_len - (amt_writ-bytes_sent), 0, 0);
#endif
          break;
#ifdef NS_DEBUG_ON
        case SMTP_REQUEST:
        case SMTPS_REQUEST:
          debug_log_smtp_req(cptr, vector_ptr->iov_base, vector_ptr->iov_len - (amt_writ-bytes_sent), 0, 0);
          break;
        case POP3_REQUEST:
        case SPOP3_REQUEST:
          debug_log_pop3_req(cptr, vector_ptr->iov_base, vector_ptr->iov_len - (amt_writ-bytes_sent), 0, 0);
          break;
        case FTP_REQUEST:
          debug_log_ftp_req(cptr, vector_ptr->iov_base, vector_ptr->iov_len - (amt_writ-bytes_sent), 0, 0);
          break;
        case DNS_REQUEST:
          debug_log_dns_req(cptr, vector_ptr->iov_base, vector_ptr->iov_len - (amt_writ-bytes_sent), 0, 0);
          break;
        case USER_SOCKET_REQUEST:
          debug_log_user_socket_data(cptr, vector_ptr->iov_base, vector_ptr->iov_len - (amt_writ-bytes_sent), 0, 0);
          break;
        case IMAP_REQUEST:
        case IMAPS_REQUEST:
          debug_log_imap_req(cptr, vector_ptr->iov_base, vector_ptr->iov_len - (amt_writ-bytes_sent), 0, 0);
          break;
        case JRMI_REQUEST:
        //  debug_log_jrmi_req(cptr, vector_ptr->iov_base, vector_ptr->iov_len - (amt_writ-bytes_sent), 0, 0);
          break;
#endif
      }
      //This vector only partilally written
      /* reenabling this check for NULL below. 
       * if the same (first) vector is incomplete, its iov_base is changed each time and 
       * the old  iov_base is saved in last_iov_base here. Therefore, we  lose the original
       * iov_base the 2nd time we're here.
       * for socket api, handle_incomplete_ is called repeatedly as the 1st and only vector
       * is usually not sent in one go. later, we free the last_iov_base - this causes a 
       * core dump as it is not the addr that was originally malloced
       * Should nt this be true for all cases where handle_incomplete is called ?
       * -Jai
       */
      
      /* Shalu: we will set last_iov_base only in case if vector need to be free. if we are setting it without checking it will not be null*/
      if((cptr->last_iov_base == NULL) && (ns_iovec->flags[i] & NS_IOVEC_FREE_FLAG)){
        NSDL3_HTTP(NULL, cptr, "copying %p to last_iov_base",vector_ptr->iov_base);
        cptr->last_iov_base = vector_ptr->iov_base;
      }
      vector_ptr->iov_base = vector_ptr->iov_base + vector_ptr->iov_len - (amt_writ-bytes_sent);
      vector_ptr->iov_len = amt_writ-bytes_sent;
      cptr->bytes_left_to_send = bytes_to_send - bytes_sent;
      break;
    }
  }

  cptr->num_send_vectors = left_vectors = num_vectors - i;
  
  if((cptr->flags & NS_HTTP_EXPECT_100_CONTINUE_HDR) == NS_HTTP_EXPECT_100_CONTINUE_HDR) {
    cptr->body_index -= i; // Rearrange BODY vector index in case of 100 Continue
    NSDL3_HTTP(NULL, cptr,  "Rearranging body index, as we have free's written vector,"
			    " new body index = %d",
			    cptr->body_index);
  }
 
  cptr->first_vector_offset = 0;
  MY_MALLOC(start_array, left_vectors, "Allocate Left Vector to strt array", -1);
  MY_MALLOC(start_vector, left_vectors * sizeof (struct iovec), "Allocate Left Vector to strt vector", -1);
  for (j=0; i < num_vectors; i++, j++, vector_ptr++)
  {
    start_array[j] = ns_iovec->flags[i];
    start_vector[j].iov_base = vector_ptr->iov_base;
    start_vector[j].iov_len = vector_ptr->iov_len;
  }
  cptr->free_array = start_array;
  cptr->send_vector = start_vector;

  // Commn3wnte  
  //cptr->conn_state = CNST_WRITING;
  if (cptr->url_num->proto.http.exact)
    SetSockMinBytes(cptr->conn_fd, cptr->url_num->proto.http.bytes_to_recv);
  //SetSockMinBytes(cptr->conn_fd, 10306);

  cptr->tcp_bytes_sent += bytes_sent;

  if (cptr->url_num->request_type == SMTP_REQUEST || cptr->url_num->request_type == SMTPS_REQUEST) {
    INC_SMTP_TX_BYTES_COUNTER(vptr, bytes_sent);
  } else if ((cptr->url_num->request_type == POP3_REQUEST) || (cptr->url_num->request_type == SPOP3_REQUEST)) {
    INC_POP3_SPOP3_TX_BYTES_COUNTER(vptr, bytes_sent);
  } else if (cptr->url_num->request_type == FTP_REQUEST  ||
             cptr->url_num->request_type == FTP_DATA_REQUEST) {
    INC_FTP_DATA_TX_BYTES_COUNTER(vptr, bytes_sent);
  } else if (cptr->url_num->request_type == DNS_REQUEST) {
    INC_DNS_TX_BYTES_COUNTER(vptr, bytes_sent);
  } else if ((cptr->url_num->request_type == LDAP_REQUEST) || (cptr->url_num->request_type == LDAPS_REQUEST)) {
    INC_LDAP_TX_BYTES_COUNTER(vptr, bytes_sent);
  } else if (cptr->url_num->request_type == IMAP_REQUEST) {
    INC_IMAP_TX_BYTES_COUNTER(vptr, bytes_sent);
  } else if (cptr->url_num->request_type == JRMI_REQUEST) {
    INC_JRMI_TX_BYTES_COUNTER(vptr, bytes_sent);
  } else if ((cptr->url_num->request_type == WS_REQUEST) || (cptr->url_num->request_type == WSS_REQUEST)) {
    INC_WS_TX_BYTES_COUNTER(vptr, bytes_sent);
  } else if ((cptr->url_num->request_type == SOCKET_REQUEST) || (cptr->url_num->request_type == SSL_SOCKET_REQUEST)) {
    if (IS_TCP_CLIENT_API_EXIST)
      fill_tcp_client_avg(vptr, SEND_THROUGHPUT, bytes_sent);
  }else {
    INC_OTHER_TYPE_TX_BYTES_COUNTER(vptr, bytes_sent);
  }

  NS_RESET_IOVEC(*ns_iovec);
  NSDL3_HTTP(NULL, cptr,  "cptr->num_send_vectors = %d, cptr->tcp_bytes_sent = %d", cptr->num_send_vectors, cptr->tcp_bytes_sent);
  NSDL1_HTTP(NULL, cptr, "[cptr=%p]: Succfully Sent Bytes = %d, Left to Send Bytes = %d", 
                          cptr, cptr->tcp_bytes_sent, cptr->bytes_left_to_send);

  if(cptr->http_protocol == HTTP_MODE_HTTP2){
    NSDL3_HTTP(NULL, cptr,  "num_vectors = %d sptr->num_vectors = %d bytes_sent=%d + 16384 >sptr->window_update_from_server=%d", num_vectors, sptr->num_vectors,bytes_sent, sptr->window_update_from_server);
    if((num_vectors != sptr->num_vectors) || ((bytes_sent +  cptr->http2->settings_frame.settings_max_frame_size) > sptr->window_update_from_server)){
      cptr->conn_state = CNST_HTTP2_READING; /*bug 51330: changed WRITING to READING*/
      NSDL3_HTTP(NULL, cptr,  "Setting connection state to CNST_HTTP2_READING to read window update frame");
    }
    copy_cptr_to_stream(cptr, sptr); 
  }

#ifndef USE_EPOLL
  FD_SET( cptr->conn_fd, &g_wfdset );
#endif
}

void handle_ssl_write_ex(connection *cptr, u_ns_ts_t now){
  int ret;
  NSDL1_HTTP2(NULL, cptr, "Method called");
  while(cptr->http2->front){
    copy_stream_to_cptr(cptr, cptr->http2->front); 
    ret = handle_ssl_write (cptr, now);
    if(ret == 0){
      NSDL1_HTTP2(NULL, cptr, "Going to write next request");
      cptr->http2->front = cptr->http2->front->q_next; 
    }else if(ret == -1){
      NSDL1_HTTP2(NULL, cptr, "Eagain copy all the counters to scptr");
      copy_cptr_to_stream(cptr, cptr->http2->front);
      break; 
    }else {
      NSDL1_HTTP2(NULL, cptr, "Error in writing request");
      break;
    } 
  }
}

void http2_handle_ssl_write_ex(VUser *vptr, connection *cptr, u_ns_ts_t now, int http_size, int num_vectors){

  int ret;
  NSDL1_HTTP2(NULL, cptr, "Method called cptr->http2->front=%p cptr->http2_rear=%p cptr->http2->http2_cur_stream=%p",
                       cptr->http2->front, cptr->http2->rear,cptr->http2->http2_cur_stream);
  /* bug 79062 check if http2_cur_stream is equal to front */
  // Copy current cptr data to current stream, and add that node to queue
  if(cptr->http2->http2_cur_stream != cptr->http2->front ){
    copy_request_into_buffer(cptr, http_size, &g_req_rep_io_vector);
    copy_cptr_to_stream(cptr, cptr->http2->http2_cur_stream);
    /*add node to the Stream queue*/
    add_node_to_queue(cptr, cptr->http2->http2_cur_stream); 
  }
  while(cptr->http2->front){
    NSDL1_HTTP2(NULL, cptr, "cptr->http2->front=%p",cptr->http2->front); 
    // Copy front data into cptr  
    copy_stream_to_cptr(cptr, cptr->http2->front);
    /*bug 79062 copy Q->front to http2_cur_stream, as handle_ssl_write use it*/
    cptr->http2->http2_cur_stream = cptr->http2->front;
    if(vptr->flags & NS_SOAP_WSSE_ENABLE)
    {
      NSDL4_HTTP(NULL, cptr, "Apply soap ws security in outgoing soap xml");
      char *outbuf;
      int outbuf_len;
      nsSoapWSSecurityInfo *ns_ws_info = vptr->ns_ws_info;
      if(ns_ws_info && (ns_apply_soap_ws_security(ns_ws_info, cptr->free_array, http_size, &outbuf , &outbuf_len) == 0))
      {
        NSDL4_HTTP(NULL, cptr, "Successfully applied ws security xml, free previous memory");
        FREE_AND_MAKE_NOT_NULL(cptr->free_array, "cptr->free_array", -1);
        cptr->free_array = outbuf;
        //cptr->content_length +=  outbuf_len - http_size; 
        cptr->bytes_left_to_send += outbuf_len - http_size; 
      }
    } 
    if(set_socketopt_for_quickack(cptr->conn_fd, 1) < 0)
    {
      retry_connection(cptr, now, NS_REQUEST_CONFAIL);
      return;
    }
    ret = handle_ssl_write (cptr, now);
    if(ret == 0){
      NSDL1_HTTP2(NULL, cptr, "Going to write next request");
      cptr->http2->front = cptr->http2->front->q_next; 
    } else if(ret == -1){
      NSDL1_HTTP2(NULL, cptr, "Eagain copy all the counters to scptr");
      copy_cptr_to_stream(cptr, cptr->http2->front);
      break;     
    } else {
      NSDL1_HTTP2(NULL, cptr, "Error in writing request");
      break;
    } 
  }
}

void copy_iovec_to_iovec_local(connection *cptr, int http_size, int num_vectors, struct iovec *vector, u_ns_ts_t now) {

  struct iovec *local_vec_ptr;
  int amt_writ, i;

  NSDL2_HTTP2(NULL, cptr, "Method called, http_size = %d num_vectors = %d", http_size, num_vectors);

  // Allocate memory to vector 
  cptr->tcp_bytes_sent = 0;

  MY_MALLOC(local_vec_ptr, (num_vectors * sizeof (struct iovec)), "Allocate num Vector to local vector", -1);
  for (i = 0; i < num_vectors; i++) {
    MY_MALLOC (local_vec_ptr[i].iov_base, vector[i].iov_len, "local_vec_ptr", -1);
    memcpy(local_vec_ptr[i].iov_base, vector[i].iov_base, vector[i].iov_len);
    local_vec_ptr[i].iov_len = vector[i].iov_len;
    amt_writ += vector[i].iov_len;
  }
 
  NSDL2_HTTP2(NULL, NULL, "amt_writ is [%d] ", amt_writ);
  //local_vec_ptr->iov_len = amt_writ; 
  cptr->send_vector = local_vec_ptr;
  cptr->bytes_left_to_send = amt_writ;
  cptr->num_send_vectors = num_vectors;
}
/*bug 79062 method renamed*/
void h2_send_http_req_ex(connection *cptr, int http_size, NSIOVector *ns_iovec, u_ns_ts_t now){
  int ret;
  NSDL1_HTTP2(NULL, cptr, "Method called");
  /*bug 79062 check if node exists in queue */
  if(cptr->http2->http2_cur_stream != cptr->http2->front) { 
    // Copy current cptr data to current stream, and add that node to queue
    copy_iovec_to_iovec_local(cptr, http_size, ns_iovec->cur_idx, ns_iovec->vector, now); // TODO  write copy iovec into local iov
    NS_FREE_RESET_IOVEC(*ns_iovec);
    copy_cptr_to_stream(cptr, cptr->http2->http2_cur_stream); 
    add_node_to_queue(cptr, cptr->http2->http2_cur_stream); 
  }
  while(cptr->http2->front){
    NSDL1_HTTP2(NULL, cptr, "Queue is avaliable");
    copy_stream_to_cptr(cptr, cptr->http2->front);
    /*bug 79062 assign Q->front to cur stream*/
    cptr->http2->http2_cur_stream = cptr->http2->front; 
    ret = send_http_req(cptr, http_size, ns_iovec, now);
    if(ret == 0){
      NSDL1_HTTP2(NULL, cptr, "Going to write next request");
      cptr->http2->front = cptr->http2->front->q_next; 
    }else if(ret == -1){
      NSDL1_HTTP2(NULL, cptr, "Eagain copy all the counters to scptr");
      copy_cptr_to_stream(cptr, cptr->http2->front);
      break; 
    }else {
      NSDL1_HTTP2(NULL, cptr, "Error in writing requests");
      break;
    } 
  }
  NSDL1_HTTP2(NULL, cptr, "Method exit");
}

/*bug 51330: method h2_vector_to_write*/
int h2_vector_to_write(stream* sptr,  NSIOVector *ns_iovec)
{
  int index;
  int iov_len = 0;
  NSDL2_HTTP(NULL,NULL,"sptr->window_update_from_server=%d num_vectors=%d", sptr->window_update_from_server, NS_GET_IOVEC_CUR_IDX(*ns_iovec) );
  for(index = 0; index < NS_GET_IOVEC_CUR_IDX(*ns_iovec); ++index)
  {
    NSDL2_HTTP(NULL,NULL, "NS_GET_IOVEC_LEN(%d)=%d", index, NS_GET_IOVEC_LEN(*ns_iovec, index)); 
    iov_len += NS_GET_IOVEC_LEN(*ns_iovec, index);
    NSDL2_HTTP(NULL,NULL,"iov_len=%d",iov_len);
    if(iov_len > sptr->window_update_from_server)
      break;
  }
  return index;
}
//This method to send http request
int send_http_req(connection *cptr, int http_size, NSIOVector *ns_iovec, u_ns_ts_t now)
{
  int bytes_sent;
  int vectors_to_send;
  int vectors_to_write;
  stream *sptr = NULL;

  VUser* vptr = cptr->vptr;
  /* This will have the header size in case of Expect: 100-Continue
   * else it will have the http_size as passed */
  int http_size_tmp;
  NSDL2_HTTP(NULL, cptr, "Method Called, cptr=%p state=%d, http_size = %d, sending request on pipe = %d, proto_state = %d",
			 cptr, cptr->conn_state, http_size, cptr->num_pipe, cptr->proto_state);


  if(!(cptr->flags & NS_HTTP_EXPECT_100_CONTINUE_HDR)) {
     NSDL3_HTTP(NULL, cptr, "Sending http request");
     vectors_to_send = ns_iovec->cur_idx;
     http_size_tmp = http_size;
  } else  { // Send all Headers ONLY rest (BODY will be send when 100 Continue will come)
     NSDL3_HTTP(NULL, cptr, "Sending only header for expect 100 continue. body_index = %d",
					 cptr->body_index);
     vectors_to_send = cptr->body_index;
     http_size_tmp = http_size - cptr->content_length;
  }

  errno = 0;
  
  if(cptr->http_protocol == HTTP_MODE_HTTP2){
    sptr = cptr->http2->http2_cur_stream;
    /*bug 51330: call h2_vector_to_write()*/
    vectors_to_write = h2_vector_to_write(sptr, ns_iovec);
    NSDL2_HTTP(NULL, cptr, "vectors_to_write=%d",vectors_to_write);
  }
  else
    vectors_to_write = vectors_to_send;
  
  NSDL2_HTTP(NULL, cptr, "vectors_to_write = %d vectors_to_send = %d cptr->conn_fd=%d", vectors_to_write, vectors_to_send,cptr->conn_fd);
 
  if ((bytes_sent = writev(cptr->conn_fd, ns_iovec->vector, vectors_to_write)) < 0) {
    int merr = errno;
    NSEL_MIN(NULL, cptr, ERROR_ID, ERROR_ATTR, "Error: (%s) in writing vector,"
						"sending HTTP request failed fd = %d,"
						"vectors_to_write = %d, %s",
						nslb_strerror(errno), cptr->conn_fd, vectors_to_write,
						get_url_req_url(cptr));

    if(merr != EAGAIN)
    {
      NS_FREE_RESET_IOVEC(*ns_iovec);
      retry_connection(cptr, now, NS_REQUEST_WRITE_FAIL);
      return -2;
    }
  }

  if(set_socketopt_for_quickack(cptr->conn_fd, 1) < 0) {
    NS_FREE_RESET_IOVEC(*ns_iovec);
    retry_connection(cptr, now, NS_REQUEST_CONFAIL);
    return -2;
  }

  //save_referer(cptr, vector, referer_idx, 0);
  /* There are two possibleities here
   * Case: 1) HTTP - non 100-continue
   *     	o- http_size_tmp is same as http_size where http_size is
   *		   complete request size, if we have written less than total
   *		   request size than we will prepare for partial write & return.
   * Case: 2) HTTP - 100-continue
   *		o- In this case http_size_tmp will have the header size of http
   *		   request, so if header is not completely send we will prepare
   *		   for partial (header) write & return.
   *		o- If header is written completely than we will we will prepare
   *		   for partial write & return.  
   */ 	     
  if (bytes_sent < http_size_tmp) {
    NSDL3_HTTP(NULL, cptr, "Complete size not send hence returning. "
		    "bytes_sent = %d, http_size_tmp = %d",
		     bytes_sent, http_size_tmp);
    handle_incomplete_write( cptr, ns_iovec, ns_iovec->cur_idx, http_size, bytes_sent);
    return -1;    
  }

  /* it will come in follwoing block only for Expect: 100-continue*/ 
  if ((cptr->flags & NS_HTTP_EXPECT_100_CONTINUE_HDR) == NS_HTTP_EXPECT_100_CONTINUE_HDR) {
    NSDL3_HTTP(NULL, cptr, "Header for Expect: 100-continue completely written, "
			    "bytes_sent = %d, http_size_tmp = %d",
			     bytes_sent, http_size_tmp);
    handle_incomplete_write( cptr, ns_iovec, ns_iovec->cur_idx, http_size, bytes_sent);
    /* handle_incomplete_write set the conn_state to CNST_WRITING but
     * as we are expecting for 100-continue we will set to CNST HEADERS*/
    reset_after_100_continue_hdr_sent(cptr);
    return 0;
  }

#ifdef NS_DEBUG_ON
  // Complete data send, so log all vectors in req file
  int i;
  for(i = 0; i < ns_iovec->cur_idx; i++)
  {
    if(i == ns_iovec->cur_idx - 1) {
      NSDL3_HTTP(NULL, cptr, "Log req idx = %d", i);
      debug_log_http_req(cptr, ns_iovec->vector[i].iov_base, ns_iovec->vector[i].iov_len, 1, 1);
    }
    else {
      NSDL3_HTTP(NULL, cptr, "Log req idx = %d", i); 
      debug_log_http_req(cptr, ns_iovec->vector[i].iov_base, ns_iovec->vector[i].iov_len, 0, 0);
    }
  }
#else
  // Complete data send, so log all vectors in req file
  if(cptr->http_protocol != HTTP_MODE_HTTP2) {
    int i;
    for(i = 0; i < ns_iovec->cur_idx; i++)
    {
      if(i == ns_iovec->cur_idx - 1) {
        LOG_HTTP_REQ(cptr, ns_iovec->vector[i].iov_base, ns_iovec->vector[i].iov_len, 1, 1);
      }
      else {
        LOG_HTTP_REQ(cptr, ns_iovec->vector[i].iov_base, ns_iovec->vector[i].iov_len, 0, 0);
      }
    }
  }
#endif

  if (cptr->conn_state == CNST_REUSE_CON)
    cptr->req_code_filled = -2;

  NS_FREE_RESET_IOVEC(*ns_iovec);
  
  if (cptr->bytes == 0)  {      /* we should not set it if in the middle of reading 
                                 * this check is related to pipelining */
    // This check is for http2 protocol, in case protocol version is http2, CNST_HEADERS will not be used
    if(cptr->http_protocol != HTTP_MODE_HTTP2) // TODO replace this check with cptr->http_protocol != 2
      cptr->conn_state = CNST_HEADERS;
    else 
      cptr->conn_state = CNST_HTTP2_READING; /*bug 51330: changed _WRITING to _READING */
  } 
  if (cptr->url_num->proto.http.exact)
    SetSockMinBytes(cptr->conn_fd, cptr->url_num->proto.http.bytes_to_recv);
  //SetSockMinBytes(cptr->conn_fd, 10306);
  cptr->tcp_bytes_sent = bytes_sent;

  average_time->tx_bytes += bytes_sent;

  if(SHOW_GRP_DATA) {
    avgtime *lol_average_time;
    lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
    lol_average_time->tx_bytes += bytes_sent;
  }

  on_request_write_done (cptr);

  /* We need to close conn & reload/click away a page if forcefully reloaded/click away i.e. time is 0(reload/clickaway)
   * For http chk and force reload click away is called twice. Partial write is done then it will be called from handle write else form here
   */
  if (cptr->url_num->proto.http.type == MAIN_URL)
    chk_and_force_reload_click_away(cptr, now);

  return 0;
}

/* This method is to free memmory from kernel.
 * (As know for concurrency test case we need more memory to make more connection)
 * For each connection made by NS, kernel allocates 4KB memory (total mem allocated is approx 2.5 + 4 KB)& until the conn is closed it retains this memory 
 * At max 4 GB mem is supported for FC9-32 Bit so to reuse that memory we need to free that 4KB memory explicitely, on the 
 * basis of connection is KEEP ALIVE & It was not the last request.  */
inline void free_sock_mem(int fd) 
{
  int SO_len, SO_on=1;
  SO_len = sizeof(int);
  NSDL2_SOCKETS(NULL, NULL, "Method called. fd = %d", fd);

  if(setsockopt(fd, SOL_TCP, FREE_SOCKET_MEM, (char *) &SO_on, SO_len) < 0 )
    fprintf(stderr, "Error: Freeing Kernel memory for socket.\n");
}

inline void update_parallel_connection_counters(connection *cptr, u_ns_ts_t now) {

  VUser *vptr = cptr->vptr;

  NSDL2_SOCKETS(vptr, cptr, "Method called. conn_fd=%d, conn_state = %d, now=%u, cptr->timer_ptr->timer_type = %d",
			     cptr->conn_fd, cptr->conn_state, now, cptr->timer_ptr->timer_type);

  HostSvrEntry *hptr;

  if(cptr->request_type != CPE_RFC_REQUEST &&
      cptr->request_type != USER_SOCKET_REQUEST) {
    hptr = vptr->hptr + cptr->gServerTable_idx;
    int timer_type = cptr->timer_ptr->timer_type;

    // This condition is added to handle a case where delay is scheduled and server closes the connection, in this case we are closing
    // fd but not freeing cptr, hence nnot updating cnum_parallel as it is used to manage 
    if (!((cptr->conn_state == CNST_REUSE_CON) && (timer_type == AB_TIMEOUT_IDLE))){
      vptr->cnum_parallel--;
      /*bug 54315: reduce count only for non zero value, inorder to avoid -ive count */
      if(hptr->num_parallel)
        hptr->num_parallel--;
      NSDL2_SOCKETS(NULL, NULL, " Decrementing parallel counters vptr->cnum_parallel = %d, hptr->num_parallel = %d", vptr->cnum_parallel, hptr->num_parallel);
    }
    NSDL2_SOCKETS(NULL, NULL, "vptr->cnum_parallel = %d, hptr->num_parallel = %d", vptr->cnum_parallel, hptr->num_parallel);
  }
}

/* Must be called ONLY & ONLY IF FRESH response is coming from cache*/
inline void cache_close_fd (connection* cptr, u_ns_ts_t now) {
  VUser *vptr = cptr->vptr;
  NSDL2_SOCKETS(NULL, cptr, "Method called. conn_fd=%d, conn_state = %d, now=%u",
			     cptr->conn_fd, cptr->conn_state, now);

#ifdef NS_DEBUG_ON
  /* This is called to append line break at the end of the http response. */
  if(vptr->flags & NS_CACHING_ON)
    if(!(((CacheTable_t*)(cptr->cptr_data->cache_data))->cache_flags & NS_CACHE_ENTRY_IN_CACHE))
    debug_log_http_res(cptr, line_break, line_break_length);
#else
   if(vptr->flags & NS_CACHING_ON){
     if(!(((CacheTable_t*)(cptr->cptr_data->cache_data))->cache_flags & NS_CACHE_ENTRY_IN_CACHE)){
       LOG_HTTP_RES(cptr, line_break, line_break_length);
     }
   }
#endif

  if(cptr->conn_state == CNST_REUSE_CON) {
    NSDL2_SOCKETS(NULL, cptr, "cptr conn state is CNST_REUSE_CON returning");
    vptr->last_cptr = cptr;
    return;
  } else {
    NSDL2_SOCKETS(NULL, cptr, "Setting connection state to CNST_FREE");
    cptr->conn_state = CNST_FREE;
    cptr->conn_fd = -1;
    update_parallel_connection_counters(cptr, now);
    vptr->last_cptr = NULL;
  }
}

inline void connection_closed_dump(VUser *vptr, connection *cptr)
{  															
  char srcip[128];

  NSDL2_SOCKETS(NULL, cptr, "Method called, vptr = %p, cptr = %p", vptr, cptr);

  strcpy(srcip, nslb_get_src_addr(cptr->conn_fd));

  if((cptr->flags & NS_CPTR_FLAGS_CON_USING_PROXY) && (runprof_table_shr_mem[vptr->group_num].proxy_ptr->http_proxy_server != NULL))	
  {
    char origin_server_buf[1024 + 1];
    char proxy_server_buf[1024 + 1];

    strcpy(proxy_server_buf, nslb_sock_ntop((struct sockaddr *)&cptr->conn_server));
    strcpy(origin_server_buf, nslb_sock_ntop((struct sockaddr *)&cptr->cur_server));

    NSDL3_SOCKETS(vptr, cptr, "http_proxy_server = %s, https_proxy_server = %s", 
                               runprof_table_shr_mem[vptr->group_num].proxy_ptr->http_proxy_server,
                               runprof_table_shr_mem[vptr->group_num].proxy_ptr->https_proxy_server);

    if((cptr->url_num->request_type == HTTP_REQUEST) || (cptr->url_num->request_type == WS_REQUEST))  			
    { 														
      NS_DT4(vptr, cptr, DM_L1, MM_SOCKETS, "Closing for HTTP request and for group %s to proxy server '%s:%hd' (%s) "
                                            "for origin server '%s' (%s) with fd %d (Recorded Server: %s), Client IP: %s",	
             				     runprof_table_shr_mem[vptr->group_num].scen_group_name,
           				     runprof_table_shr_mem[vptr->group_num].proxy_ptr->http_proxy_server, 
          				     runprof_table_shr_mem[vptr->group_num].proxy_ptr->http_port, 
                                             proxy_server_buf, gserver_table_shr_mem[cptr->gServerTable_idx].server_hostname,
                                             origin_server_buf, cptr->conn_fd, gserver_table_shr_mem[cptr->gServerTable_idx].server_hostname, 
                                             srcip);					

    }  														
       															
    if((cptr->url_num->request_type == HTTPS_REQUEST) || (cptr->url_num->request_type == WSS_REQUEST))                
    {  														
        NS_DT4(vptr, cptr, DM_L1, MM_SOCKETS, "Closing for HTTPS request and for group %s "			
                             		      "to proxy server '%s:%hd' (%s) "								
                                              "for origin server '%s' (%s) with fd %d (Recorded Server: %s), Client IP: %s",		
                      			       runprof_table_shr_mem[vptr->group_num].scen_group_name,
                      			       runprof_table_shr_mem[vptr->group_num].proxy_ptr->https_proxy_server,
                      			       runprof_table_shr_mem[vptr->group_num].proxy_ptr->https_port,
                      			       proxy_server_buf, gserver_table_shr_mem[cptr->gServerTable_idx].server_hostname, 
                                               origin_server_buf, cptr->conn_fd, gserver_table_shr_mem[cptr->gServerTable_idx].server_hostname, 					      srcip);						
    }
  }														
  else														
  {															
    NS_DT4(vptr, cptr, DM_L1, MM_SOCKETS, "Closing for HTTPS request and for group %s to server '%s' (%s) with fd %d"			
                                          " (Recorded Server: %s), Client IP: %s",					
                                           runprof_table_shr_mem[vptr->group_num].scen_group_name,			
                                           gserver_table_shr_mem[cptr->gServerTable_idx].server_hostname,
                                           nslb_sock_ntop((struct sockaddr *)&cptr->cur_server),				
                                           cptr->conn_fd,							
                                           gserver_table_shr_mem[cptr->gServerTable_idx].server_hostname, srcip);			
  }															
}
/*bug 54315: updated for error vs normal close*/
#define update_h2_urlsCount(cptr,vptr)\
{ \
  NSDL2_CONN(vptr, cptr,"before closing fd, cptr[%p]->req_ok=%d, cptr->gServerTable_idx=%d cptr->http_protocol=%d", cptr, cptr->req_ok, cptr->gServerTable_idx, cptr->http_protocol);         \
  NSDL2_CONN(vptr, cptr, "urls_left=%d urls_awaited=%d",  \
             vptr->urls_left,vptr->urls_awaited); \
  if(cptr->http2)                                                             \
  {                                                                                                      \
      NSDL2_CONN(vptr, cptr, "cptr->http2->total_open_streams=%d", cptr->http2->total_open_streams); \
      if((cptr->req_ok >= NS_REQUEST_BADBYTES) && cptr->http2->total_open_streams) \
        vptr->urls_awaited++; \
      vptr->urls_awaited -= (cptr->http2->total_open_streams );                                               \
  }\
  NSDL2_CONN(vptr, cptr, "urls_left=%d urls_awaited=%d",  \
    vptr->urls_left,vptr->urls_awaited);             \
}
inline void close_fd (connection* cptr, int done, u_ns_ts_t now)
{
  VUser *vptr = cptr->vptr;
  unsigned char request_type;

  int timer_type;
  timer_type = cptr->timer_ptr->timer_type;

  NSDL2_SOCKETS(NULL, cptr, "Method called. cptr = %p, conn_fd=%d, done=%d, now=%u, request_type = %d, done = %d, timer_type = %s"
 			     ", cptr->conn_state = %d", 
			    cptr, cptr->conn_fd, done, now, cptr->request_type, done, get_timer_type_by_name(timer_type), cptr->conn_state);

#ifndef USE_EPOLL
  FD_CLR( cptr->conn_fd, &g_rfdset );
  FD_CLR( cptr->conn_fd, &g_wfdset );
#endif

/* This is called to append line break at the end of the http response. */
  /* In case of con fail, line break should not go in file */

  if((done <= NS_FD_CLOSE_AS_PER_RESP && cptr->request_type != LDAP_REQUEST && cptr->request_type != LDAPS_REQUEST) && 
     ((cptr->request_type != WS_REQUEST && cptr->request_type != WSS_REQUEST &&
       cptr->request_type != FC2_REQUEST && cptr->request_type != SOCKET_REQUEST &&
       cptr->request_type != SSL_SOCKET_REQUEST)/* && (done == NS_DO_NOT_CLOSE_FD)*/))
  {
    if(cptr->req_ok != NS_REQUEST_CONFAIL)
    {
#ifdef NS_DEBUG_ON
      /*bug 68963: added line break at the end of response file for HTTP2 as well*/
      if(cptr->http_protocol == HTTP_MODE_HTTP1)
        debug_log_http_res(cptr, line_break, line_break_length);
      else
        debug_log_http_res(cptr, line_break, line_break_length);
#else
      if(cptr->http_protocol != HTTP_MODE_HTTP2){
        LOG_HTTP_RES(cptr, line_break, line_break_length);
      }
#endif
    }
  }

  /* Following are the cases for handling connection close and timers
  Case	   			done	conn_state		KA Timer	IdleTimer(For TO) IdleTimer(For Schedule)	Action
  --------------------------------------------------------------------------------------------------------------------------
  FirstReqOnAConnection    	Any	Not Reuse		No		Yes		No			Delete Timer
  RepeatReqOnKAConnection    	Any	Not Reuse		No		Yes		No			Delete Timer
  (KA Timeout will never be applicable in above cases as this is already deleted when new Req is send)

  (done will always be > 0 in Server Close Cases)
  ServerClose		    	> 0	Reuse			No		No		No			Close Fd
  ServerClose		    	> 0	Reuse			No		No		Yes			Not not delete timer. Close Fd
  ServerClose		   	> 0 	Reuse			Yes		No		No			Delete timer. Close Fd
  */

  if(((timer_type == AB_TIMEOUT_IDLE) && (cptr->conn_state != CNST_REUSE_CON))  || // Timer is for Timepout for response (Not for inline schedule due to inline delay)
  (timer_type == AB_TIMEOUT_KA)) // Timer is for Keep Alive and connection is now closed from server
  {
    /*bug 68086 delete timer only if :
      1- HTTP 1 protocol
      2- Or  HTTP2 AND http2->total_open_streams == 0, means if close_fd has been called for last stream, i.e.
      timer should not be deleted in case there is any stream is open, means if t.o occur on any stream other than last   */
   if(!(cptr->http_protocol == HTTP_MODE_HTTP2 && ( (cptr->http2) && cptr->http2->total_open_streams ) ))
      dis_timer_del(cptr->timer_ptr);

  }
  
  //cache_free_data(cptr->cptr_data->cache_data, 1);
  
  if (done) {
#ifdef ENABLE_SSL
    ssl_struct_free(cptr);
#endif

    //Reset proxy flag and connect flags because next time whenever connection is made then on that time Proxy flag should be unset
    cptr->proxy_auth_proto = 0;
    cptr->flags &= ~NS_CPTR_FLAGS_CON_USING_PROXY;
    cptr->flags &= ~NS_CPTR_FLAGS_CON_USING_PROXY_CONNECT;
    NSDL2_PROXY(NULL, cptr, "Unsetting NS_CPTR_FLAGS_CON_USING_PROXY & NS_CPTR_FLAGS_CON_USING_PROXY_CONNECT flag");
 
    // TODO: Free cptr_data and all other mallocs in this if entry is not in the cache
    // Freeing cptr_data in handle_url_complete ()

    /*HPM is only for TCP requests (Except ftp active mode)*/ 
    if(global_settings->high_perf_mode && 
        (cptr->request_type == HTTP_REQUEST || 
         cptr->request_type == HTTPS_REQUEST ||
       //  (cptr->request_type == LDAP_REQUEST) ||
        (cptr->request_type == DNS_REQUEST && cptr->url_num->proto.dns.proto == USE_DNS_ON_TCP) || 
        (cptr->request_type == FTP_DATA_REQUEST && cptr->url_num->proto.ftp.passive_or_active == FTP_TRANSFER_TYPE_PASSIVE)||
        (cptr->request_type == FTP_REQUEST )||
        (cptr->request_type == SMTP_REQUEST ))) {
    	add_sock_to_list(cptr->conn_fd, 1, &runprof_table_shr_mem[vptr->group_num].gset);
    } else {
#ifdef USE_EPOLL
             if(vptr->vuser_state != NS_VUSER_PAUSED) //cptr->conn_fd already removed during VUser PAUSE 
             {
		num_reset_select++; /*TST */
                if(cptr->req_ok != NS_SOCKET_BIND_FAIL) {     //If Bind failed remove select is not of use
		  if (remove_select(cptr->conn_fd) < 0) {
                    /* Below statement is commented due to Bug 63050 */
		   // printf("reset Select failed on READ \n");
                    NSTL1(NULL, NULL,"remove_select failed for cptr->conn_fd =%d", cptr->conn_fd);
		   /*bug 79803 - do not perform end test, instead dump cptr for analysis*/
		   /*we are commenting end_test_run() for time being as a workaround, call must not hit this scope */
		   /*we will keep analysing this issue*/
 		   char cptr_to_str[MAX_LINE_LENGTH + 1] = "\0";
      		   cptr_to_string(cptr, cptr_to_str, MAX_LINE_LENGTH); 
		   NSTL1(NULL, NULL,"cptr info =%s", cptr_to_str);
		   /*AS its not a valid fd, so reset as -1 */
		   cptr->conn_fd = -1;
		   //end_test_run();
		  }
                }
              }
#endif
        //Commented NULL from debug as need to pass vptr and cptr
        //NS_DT4(NULL, NULL, DM_L1, MM_CONN,  "closing conection fd = %d", cptr->conn_fd);
        //NS_DT4(vptr, cptr, DM_L1, MM_CONN,  "closing conection fd = %d", cptr->conn_fd);
        NSDL2_CONN(vptr, cptr,"Closing conection fd = %d cptr->http_protocol=%d", cptr->conn_fd,cptr->http_protocol);
#ifdef NS_DEBUG_ON
        connection_closed_dump(vptr, cptr);
#endif
    	/*bug 54315 get hptr and update vptr+hptr accordingly before closing fd for the same*/
	update_h2_urlsCount(cptr,vptr)
	close(cptr->conn_fd);
    }

    ns_fd_setbit(&ns_fdset, cptr->conn_fd);

    request_type = get_request_type(cptr);
    NSDL2_CONN(vptr, cptr, "request_type = %d, conn_state = %d", request_type, cptr->conn_state);

    switch(request_type) {
      case SOCKET_REQUEST:
      case SSL_SOCKET_REQUEST:
      {
        if ((cptr->conn_state > CNST_MIN_CONFAIL) || (cptr->conn_state == CNST_REUSE_CON) 
            || (cptr->conn_state == CNST_SSLCONNECTING)) 
        {
          if (IS_TCP_CLIENT_API_EXIST)
            fill_tcp_client_avg(vptr, CON_CLOSED, 0);

          if (IS_UDP_CLIENT_API_EXIST)
            fill_udp_client_avg(vptr, CON_CLOSED, 0);

          INC_HTTP_HTTPS_NUM_CONN_BREAK_COUNTERS(vptr);
        }
        else
        {
          NSDL2_CONN(vptr, cptr, "[SocketStats] Connection was not succesful. "
            "So not decreamenting num_connections. "
            "conn_state = %d, num_connections = %d",
             cptr->conn_state, num_connections); 
        }
        break;            //Fall through to HTTP as it has Overall counters
      }
      case HTTP_REQUEST:
      case HTTPS_REQUEST:
      //Changes done for bug#5106: In case of SSL failure we need to decrement variable "num_connections"
        if ((cptr->conn_state > CNST_MIN_CONFAIL) || (cptr->conn_state == CNST_REUSE_CON) || (cptr->conn_state == CNST_SSLCONNECTING)) {
          DEC_HTTP_HTTPS_CONN_COUNTERS(vptr);
     //changes done for bug #32642 and #24075 : Closed connection where more than the open connections
          INC_HTTP_HTTPS_NUM_CONN_BREAK_COUNTERS(vptr);
        } else  {
          NSDL2_CONN(vptr, cptr, "Connection was not succesful."
                                " So not decreamenting num_connections."
                                " conn_state = %d, num_connections = %d",
                                cptr->conn_state, num_connections); 
        }
        break;
      case WS_REQUEST:
      case WSS_REQUEST:
        if ((cptr->conn_state > CNST_MIN_CONFAIL) || (cptr->conn_state == CNST_REUSE_CON) || (cptr->conn_state == CNST_SSLCONNECTING)) {
          //Bug 35394 - WebSocket Stats : We are getting data in graph "websocket Connections Close/Sec" while getting 100% ConFail (zero connection is opened).
          INC_WS_WSS_NUM_CONN_BREAK_COUNTERS(vptr);
        } else  {
          NSDL2_CONN(vptr, cptr, "Connection was not succesful. So not decreamenting num_connections, conn_state = %d, num_connections = %d",
                                  cptr->conn_state, num_connections);
        }
        break; 
      case SMTP_REQUEST:
      case SMTPS_REQUEST:
        DEC_SMTP_SMTPS_CONN_COUNTERS(vptr);
        break;
      case POP3_REQUEST:
      case SPOP3_REQUEST:
        DEC_POP3_SPOP3_CONN_COUNTERS(vptr);
        break;
      case FTP_REQUEST:
        INC_FTP_CONN_COUNTERS(vptr);
        break;
      case FTP_DATA_REQUEST:
        DEC_FTP_DATA_CONN_COUNTERS(vptr);
        break;
      case LDAP_REQUEST:
      case LDAPS_REQUEST:
        DEC_LDAP_CONN_COUNTERS(vptr);
        break;
      case DNS_REQUEST:
        if(cptr->url_num->proto.dns.proto == USE_DNS_ON_TCP) {  // Do only for TCP Conns
          DEC_DNS_CONN_COUNTERS(vptr);             
        }
        break;
      case IMAP_REQUEST:
      case IMAPS_REQUEST:
        DEC_IMAP_IMAPS_CONN_COUNTERS(vptr);
        break;
      case JRMI_REQUEST:
        DEC_JRMI_CONN_COUNTERS(vptr);
        break;
      case FC2_REQUEST:
        break;
    }

    // Moving this before  setting state to free as we are using it inside update_parallel_connection_counters to check for decrementing 
    // vptr->cnum_parallel and hptr->num_parallel
    /*Bug  54315 : resetting cptr->conn_state before calling update_parallel_connection_counters(), 
     so that it can reduce vptr->cnum_parallel--*/ 
    cptr->conn_state = CNST_FREE;
    update_parallel_connection_counters(cptr, now);
    cptr->conn_fd = -1;

    //Releasing the connection, hence making last_cptr NULL in new script design
    vptr->last_cptr = NULL;
  } 
  else if(cptr->request_type == WS_REQUEST || cptr->request_type == WSS_REQUEST) {
    NSDL2_WS(vptr, cptr, "Don't set cptr->conn_state == CNST_REUSE_CON, as request is WS");
  } 
  else {
    //Till the response of all the multiplexed inline requests is received will not set conn_state to CNST_REUSE_CON
    if(cptr->http_protocol == HTTP_MODE_HTTP2 && cptr->http2->total_open_streams) {
      /*bug 78108 set last_cptr, so that cptr can be reuse on next execute_page call from same host */
      vptr->last_cptr = cptr;
      cptr->conn_state = CNST_REUSE_CON;
      
      NSDL2_SOCKETS(NULL, NULL, "setting connection state to reuse for HTTP2 ");

      //NSDL2_SOCKETS(NULL, NULL, "Not setting connection state to reuse, as it is a http2 connection and streams are opned on this cptr");
      return;
    }
    NSDL2_SOCKETS(NULL, NULL, "Setting connection state to reuse");
    //Storing the cptr used in the last_cptr for reuse in new script design
    vptr->last_cptr = cptr;
    cptr->conn_state = CNST_REUSE_CON;
    cptr->conn_link = NULL;
    check_and_add_ka_timer(cptr, vptr, now);
  }
  NSDL2_SOCKETS(NULL, NULL, "cptr = %p, conn_state = %d", cptr, cptr->conn_state);
}

inline void close_fd_and_release_cptr (connection* cptr, int done, u_ns_ts_t now)
{
  close_fd(cptr, done, now);
  NSDL1_CONN(NULL, NULL, "Need to call free_connection_slot cptr = %p ", cptr);
  NSDL1_CONN(NULL, NULL, "cptr->timer_ptr->timer_type  = [%d]", cptr->timer_ptr->timer_type);
  if(cptr->timer_ptr->timer_type != AB_TIMEOUT_IDLE) // Dont free cptr here if idle timer is set, this is used to schedule inline delay
    free_connection_slot(cptr, now);
}

#if 0
void
init_parent_listner_socket()
{
//int opt;
//struct sockaddr_in udp_servaddr;
int value;

    NSDL2_SOCKETS(NULL, NULL, "Method called");
     /* Open UDP listening port to collect stats from children */
    if ((udp_fd = nslb_udp_server(0, 0)) < 0) {
      perror("netstorm:  Error in creating Parent UDP listen socket.  Aborting...\n");
      exit(1);
    }

    //Get the Parent port number
    {
	  struct sockaddr_in sockname;
	  socklen_t socksize = sizeof (sockname);
	  getsockname(udp_fd, (struct sockaddr *)&sockname, &socksize);
	  parent_port_number = ntohs (sockname.sin_port);
	  //printf("parent port number is %hd\n", parent_port_number);
    }

     /*{
      int size2 = sizeof(int);
      int value2;
      getsockopt(udp_fd, SOL_SOCKET, SO_RCVBUF, (char*) &value2, &size2);
      printf("before getting, rcv value is %d, size is %d\n", value2, size2);
      getsockopt(udp_fd, SOL_SOCKET, SO_SNDBUF, (char*) &value2, &size2);
      printf("before getting, snd value is %d, size is %d\n", value2, size2);
      }*/

    value = 3 * g_avgtime_size * global_settings->num_process + 1;  /* The two is because the child may send a progress report and a finish report within a very small time frame and the parent may not be fast enough to wake up and read them seperately */

    //printf("valus is %d num_proc=%d avg-size=%d g_avgtime_size=%d\n", value, global_settings->num_process, sizeof(avgtime), g_avgtime_size);
    if (setsockopt(udp_fd, SOL_SOCKET, SO_RCVBUF, (char*) &value, sizeof(int)) == -1) {
      printf("error in setting udp_fd recv buffer size\n");
      close(udp_fd);
      exit(-1);
    }

#if 0
   printf("new val is %d\n", value);
    value = 3 * sizeof(avgtime) * global_settings->num_process + 1;  /* The two is because the child may send a progress report and a finish report within a very small time frame and the parent may not be fast enough to wake up and read them seperately */
    if (setsockopt(udp_fd, SOL_SOCKET, SO_SNDBUF, (char*) &value, sizeof(int)) == -1) {
      printf("error in setting udp_fd recv buffer size\n");
      close(udp_fd);
      exit(-1);
    }
   printf("new val2 is %d\n", value);
     {
      int size2 = sizeof(int);
      int value2;
      getsockopt(udp_fd, SOL_SOCKET, SO_RCVBUF, (char*) &value2, &size2);
      printf("after getting, value is %d, size is %d\n", value2, size2);
      }
#endif
}
#endif

inline void
wait_sockets_clear() {
  char line[MAX_LINE_LENGTH];
  char* stat_ptr, *stat2_ptr;
  int sock_tw;
  FILE* sock_stat_file;

  NSDL2_SOCKETS(NULL, NULL, "Method called");
  while (1) {
    if ((sock_stat_file = fopen("/proc/net/sockstat", "r")) == NULL) {
      perror("./netstorm");
      NS_EXIT(-1, CAV_ERR_1000006, "/proc/net/sockstat", errno, nslb_strerror(errno));
    }
    while (nslb_fgets(line, MAX_LINE_LENGTH, sock_stat_file, 0)) {
      if (!strncasecmp(line, "TCP:", 4))
	break;
    }

    stat_ptr = strstr(line, "tw ");
    stat_ptr += 3;
    stat2_ptr = strchr(stat_ptr, ' ');
    *stat2_ptr = '\0';
    sock_tw = atoi(stat_ptr);
    //if (sock_tw < 20)
    if (sock_tw < global_settings->tw_sockets_limit)
      break;

    fclose(sock_stat_file);
    printf("waiting for sockets to clear ...\n");
    sleep(3);
  }
}

// Atul - 08/16/2007 Now its handle using conf file with keyword default is 0
//#define MAX_URL_RETRIES 3
void inline retry_connection(connection* cptr, u_ns_ts_t now, int req_status)
{
  VUser* vptr = cptr->vptr;
  int max_url_retries = runprof_table_shr_mem[vptr->group_num].gset.max_url_retries; /* per Group */
  /*bug 52092: get request_type direct from cptr, instead of url_num*/
  int request_type = cptr->request_type;
  int num_retries;
  HostSvrEntry *hptr;
  int cur_host;

  NSDL1_SOCKETS(vptr, cptr, "Method called. req_status = %d, max_url_retries = %d, cptr->num_retries = %d", 
                             req_status, max_url_retries, cptr->num_retries);
  if((req_status == NS_REQUEST_SSL_HSHAKE_FAIL) && (cptr->request_type == WSS_REQUEST))
  {
    vptr->ws_status = NS_REQUEST_SSL_HSHAKE_FAIL;
    ws_avgtime->ws_error_codes[NS_REQUEST_SSL_HSHAKE_FAIL]++;
    NSDL2_WS(vptr, NULL, "ws_avgtime->ws_error_codes[%d] = %llu", vptr->ws_status, ws_avgtime->ws_error_codes[NS_REQUEST_SSL_HSHAKE_FAIL]);
  }
  if((req_status == NS_REQUEST_SSL_HSHAKE_FAIL) && (cptr->request_type == SSL_SOCKET_REQUEST))
  {
    vptr->page_status = NS_REQUEST_SSL_HSHAKE_FAIL;

    if (IS_TCP_CLIENT_API_EXIST)
      fill_tcp_client_failure_avg(vptr, NS_REQUEST_SSL_HSHAKE_FAIL);

    if (IS_UDP_CLIENT_API_EXIST)
      fill_udp_client_failure_avg(vptr, NS_REQUEST_SSL_HSHAKE_FAIL);
   
    NSDL2_HTTP(vptr, NULL, "vptr->page_status = %d", vptr->page_status);
  }


  /* We are deleting timer as it can come here with idle_timer in case connection is reused and it is closed by server while writing request, 
   * in this case cptr->conn_state will be CNST_REUSE_CON and timer will not be deleted in close fd. So we need to delete timer here, as it 
   * should not trigerred after freeing this cptr. #BugId: 22851 
   */
  if (cptr->timer_ptr->timer_type >= 0)
  {

    NSDL1_SOCKETS(vptr, cptr, "Retry connection called with "
                  "cptr->timer_ptr->timer_type = [%d]. Going to delete timer",
  	           cptr->timer_ptr->timer_type); 

    dis_timer_del(cptr->timer_ptr);
  }

  cptr->num_retries++;
  //Atul 08/16/2007 - max_url_retries set using conf file
  if ((cptr->num_retries <= max_url_retries && 
      /* Skip retry for pipelining */
       !(runprof_table_shr_mem[vptr->group_num].gset.enable_pipelining && (cptr->num_pipe > 0))) || ((cptr->http_protocol == HTTP_MODE_HTTP2) && (req_status == NS_HTTP2_REQUEST_ERROR))){
       NSDL1_SOCKETS(vptr, cptr, "Retrying connection for url = %s, cptr->num_retries = %d, max_url_retries = %d, "
                                 "runprof_table_shr_mem[%d].gset.enable_pipelining = %d, cptr->num_pipe = %d", 
                                  get_url_req_url(cptr), cptr->num_retries, max_url_retries, vptr->group_num, 
                                  runprof_table_shr_mem[vptr->group_num].gset.enable_pipelining, cptr->num_pipe);

       NSDL2_SOCKETS(vptr, cptr, "cptr->flags=0x%x, cptr->proto_state=%d", cptr->flags, cptr->proto_state);
       if (cptr->flags & NS_CPTR_AUTH_MASK)  
       {
 	  NS_EL_2_ATTR(EID_CONN_HANDLING, vptr->user_index, vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL,
			      nslb_sockaddr_to_ip((struct sockaddr *)&cptr->cur_server, 1), nslb_get_src_addr(cptr->conn_fd), 
                              "Connection closed by server while in authentication.\nEvent Data:\n"
	                      "Script Name = %s, Page Name = %s  Request = %s Url = %s",
			       vptr->sess_ptr->sess_name, vptr->cur_page->page_name, proto_to_str(request_type), get_request_string(cptr));

         //Handling Connection brk after NTLM T0
         if(cptr->proto_state == ST_AUTH_NTLM_RCVD)
         {
           NSDL1_SOCKETS(vptr, cptr, "Reusing the connection cptr in case of NTLM..cptr=%p", cptr);
           close_fd(cptr, 0, now);
           close(cptr->conn_fd);
           cptr->conn_state = CNST_FREE;
           start_new_socket(cptr, now);
           return;
         }
         else
         {
           NSDL1_SOCKETS(vptr, cptr, "Connection break received from server. Authentication can be done on the same connection only");
           if(cptr->request_type == DNS_REQUEST && cptr->conn_link){
             connection *http_cptr = cptr->conn_link; 
             free_dns_cptr(cptr);
             FREE_AND_MAKE_NULL(cptr->data, "Freeing cptr data", -1);
             http_cptr->conn_link = NULL;
             NSDL2_SOCKETS(NULL, NULL, "IF BLOCK");
             dns_http_close_connection(http_cptr, 0, now, 1, NS_COMPLETION_NOT_DONE);
           }else{
             Close_connection(cptr, 0, now, req_status, NS_COMPLETION_NOT_DONE);
           }
           return;
         }
       }

        /* Connection Pool Design: While fetching connection from connection pool we cannot 
         * ensure both connections are same hence saving url in local var and will update 
         * connection pointer with url_num*/
        action_request_Shr* local_url_num = cptr->url_num;
        num_retries = cptr->num_retries;
        NSDL2_SOCKETS(NULL, NULL, "Calling close fd");
        close_fd_and_release_cptr(cptr, NS_FD_CLOSE_REMOVE_RESP, now);
        cptr = get_free_connection_slot(vptr);
        /* Updating connection pointer with local url_num*/
        // Check here if Protocol is HTTP. We will also set protocol type (either HTTP1 or 2) in cptr.
        //SET_HTTP_VERSION()
        //NSDL2_HTTP2(NULL, NULL, "HTTP mode for in cptr is [%d] ", cptr->http_protocol);
        cptr->url_num = local_url_num; 
        cur_host = get_svr_ptr(local_url_num, vptr)->idx;
        // Check_for_HTTP2
        /* While fetching new connection from pool to serve HTTP request, 
         * its server index was not set. Therefore while adding the connection in global server list 
         * it got added into HTTPS server list. Hence setting server index in cptr*/
        cptr->gServerTable_idx = cur_host; //updated gServerTable_idx of new cptr
        hptr = vptr->hptr + cur_host;
        hptr->num_parallel++;

        //assert(cptr == conn_ptr); // Anil - Why to check this?
        // Since we are assuming both  conn_ptr and cptr are same, we need not copy
        // conn_ptr->redirect_url_num =  cptr->redirect_url_num;
        vptr->cnum_parallel++;
        //dump_con(vptr, "retry con", cptr);
        /* This is done to specify which type of request is this. To be reviewed in
         * in light of the new protocol; check if we really need retry_connection
         * in case of new protocols. TODO:BHAV */
        cptr->request_type = request_type;
        cptr->num_retries = num_retries;
        if(req_status == NS_HTTP2_REQUEST_ERROR)                                                                                           
          cptr->http_protocol = HTTP_MODE_HTTP2;                     
        NSDL1_SOCKETS(vptr, cptr, "Making New connection: Request type = %d cptr->gServerTable_idx=%d", request_type,cptr->gServerTable_idx);  
        start_new_socket(cptr, now);
  } 
  else 
  {
    //printf ("retries over\n");
    if(cptr->request_type == DNS_REQUEST && cptr->conn_link)
    {
      connection *http_cptr = cptr->conn_link;
      FREE_AND_MAKE_NULL(cptr->data, "Freeing cptr data", -1);
      free_dns_cptr(cptr);
      http_cptr->conn_link = NULL;
      NSDL2_SOCKETS(NULL, NULL, "DNS failure close connection");
      dns_http_close_connection(http_cptr, 0, now, 1, NS_COMPLETION_NOT_DONE);
    }else{
      Close_connection(cptr, 0, now, req_status, NS_COMPLETION_NOT_DONE);
    }
  }
}

inline void
inc_con_num_and_succ(connection *cptr)
{
  NSDL2_SOCKETS(NULL, NULL, "Method called");
  // Increament current number of connections
  VUser *vptr = cptr->vptr;
  switch(cptr->request_type) {
    case SOCKET_REQUEST:
    case SSL_SOCKET_REQUEST:
    {
      if (IS_TCP_CLIENT_API_EXIST)
        fill_tcp_client_avg(vptr, CON_OPENED, 0);

      if (IS_UDP_CLIENT_API_EXIST)
        fill_udp_client_avg(vptr, CON_OPENED, 0);

      //break;                    //Fall though in HTTP case as it has Overall counters
    }
    case HTTP_REQUEST:
    case HTTPS_REQUEST:
      NSDL2_SOCKETS(NULL, NULL, "Updating Connection counters");
      INC_HTTP_HTTPS_CONN_COUNTERS(vptr);
      break;
    case WS_REQUEST:
    case WSS_REQUEST:
      INC_WS_WSS_CONN_COUNTERS(vptr);
      break;
    case SMTP_REQUEST:
    case SMTPS_REQUEST:
      INC_SMTP_SMTPS_CONN_COUNTERS(vptr);
      break;
    case POP3_REQUEST:
    case SPOP3_REQUEST:
      INC_POP3_SPOP3_CONN_COUNTERS(vptr);
      break;
    case FTP_REQUEST:
    case FTP_DATA_REQUEST:
      INC_FTP_DATA_CONN_COUNTERS(vptr);
      break;
    case LDAP_REQUEST:
    case LDAPS_REQUEST:
      INC_LDAP_CONN_COUNTERS(vptr);
      break;
    case DNS_REQUEST:
      INC_DNS_CONN_COUNTERS(vptr);
      break;
    case USER_SOCKET_REQUEST:
      break;
    case IMAP_REQUEST:
    case IMAPS_REQUEST:
      INC_IMAP_IMAPS_CONN_COUNTERS(vptr);
      break;
    case JRMI_REQUEST:
      INC_JRMI_CONN_COUNTERS(vptr);
  }
}

static inline int
double_check_connect(connection *cptr, u_ns_ts_t now)
{
  /* Check to make sure the non-blocking connect succeeded. */
  int err;
  socklen_t errlen;
  VUser* vptr = cptr->vptr;
  struct sockaddr* sa;
  int request_type = cptr->url_num->request_type;

  if(cptr->url_num->request_type == DNS_REQUEST && cptr->conn_link)
    sa = (struct sockaddr*)&cptr->conn_server; 
  else
    sa = (struct sockaddr*)&cptr->cur_server;
  

  NSDL2_SOCKETS(NULL, cptr, "Method called, cptr = %p, conn_fd = %d", cptr, cptr->conn_fd);
  if ( connect( cptr->conn_fd, sa, sizeof(struct sockaddr_in6) ) < 0 )
  {
    fprintf(stderr, "Error: num: %d error: %s\n", errno, nslb_strerror(errno)); 
    NSDL2_SOCKETS(NULL, cptr, "Error: num: %d error: %s\n", errno, nslb_strerror(errno)); 
    switch ( errno )
    {
      case EISCONN: // Connecton is made
        break;
      case EINVAL: // Invalid arguments. Something wrong with socket
        errlen = sizeof(err);
        if ( getsockopt( cptr->conn_fd, SOL_SOCKET, SO_ERROR, (void*) &err, &errlen ) < 0 )
	  (void) fprintf(stderr, "Error: %s: unknown connect error\n", get_url_req_url(cptr));
        else
	  (void) fprintf( stderr, "Error: %s: %s\n", get_url_req_url(cptr), nslb_strerror( err ) );
        if (cptr->url_num->request_type == SMTP_REQUEST || cptr->url_num->request_type == SMTPS_REQUEST)
          //Achint: Need to do
          average_time->smtp_num_con_fail++;
        else if ((cptr->url_num->request_type == POP3_REQUEST) || (cptr->url_num->request_type == SPOP3_REQUEST))
          average_time->pop3_num_con_fail++;
        else if (cptr->url_num->request_type == FTP_REQUEST  ||
             cptr->url_num->request_type == FTP_DATA_REQUEST)
          ftp_avgtime->ftp_num_con_fail++;
        else if (cptr->url_num->request_type == DNS_REQUEST)
          average_time->dns_num_con_fail++;
        else if (cptr->url_num->request_type == LDAP_REQUEST || cptr->url_num->request_type == LDAPS_REQUEST)
          ldap_avgtime->ldap_num_con_fail++;
        else if (cptr->url_num->request_type == IMAP_REQUEST || cptr->url_num->request_type == IMAPS_REQUEST)
          imap_avgtime->imap_num_con_fail++;
        else if (cptr->url_num->request_type == JRMI_REQUEST)//TODO: JRMI
          jrmi_avgtime->jrmi_num_con_fail++;
        else if ((cptr->url_num->request_type == WS_REQUEST) || (cptr->url_num->request_type == WSS_REQUEST))
          ws_avgtime->ws_num_con_fail++;  
        else if ((cptr->url_num->request_type == SOCKET_REQUEST) || (cptr->url_num->request_type == SSL_SOCKET_REQUEST))
        {
          if (IS_TCP_CLIENT_API_EXIST)
            fill_tcp_client_avg(vptr, CON_FAILED, 0); 

          if (IS_UDP_CLIENT_API_EXIST)
            fill_udp_client_avg(vptr, CON_FAILED, 0); 
        }
        else
          average_time->num_con_fail++;

        if(cptr->request_type == DNS_REQUEST && cptr->conn_link){
          connection *http_cptr = cptr->conn_link;
          FREE_AND_MAKE_NULL(cptr->data, "Freeing cptr data", -1);
          free_dns_cptr(cptr);
          http_cptr->conn_link = NULL;
          NSDL2_SOCKETS(NULL, NULL, "Close connection as we cannot recover from this error on retry");
          dns_http_close_connection(http_cptr, 0, now, 1, NS_COMPLETION_NOT_DONE);
         }else{
           Close_connection(cptr, 0, now, NS_REQUEST_CONFAIL, NS_COMPLETION_NOT_DONE);
         }
        return -1;
      case EAGAIN:
      case EALREADY :
      case EINPROGRESS :
        if (cptr->url_num->request_type == SMTP_REQUEST || cptr->url_num->request_type == SMTPS_REQUEST)
          average_time->smtp_num_con_fail++;
        else if ((cptr->url_num->request_type == POP3_REQUEST) || (cptr->url_num->request_type == SPOP3_REQUEST))
          average_time->pop3_num_con_fail++;
        else if (cptr->url_num->request_type == FTP_REQUEST  ||
             cptr->url_num->request_type == FTP_DATA_REQUEST)
          ftp_avgtime->ftp_num_con_fail++;
        else if (cptr->url_num->request_type == DNS_REQUEST)
          average_time->dns_num_con_fail++;
        else if (cptr->url_num->request_type == LDAP_REQUEST || cptr->url_num->request_type == LDAPS_REQUEST)
          ldap_avgtime->ldap_num_con_fail++;
        else if (cptr->url_num->request_type == IMAP_REQUEST || cptr->url_num->request_type == IMAPS_REQUEST)
          imap_avgtime->imap_num_con_fail++;
        else if (cptr->url_num->request_type == JRMI_REQUEST )
          jrmi_avgtime->jrmi_num_con_fail++; //JRMI
        else if ((cptr->url_num->request_type == WS_REQUEST) || (cptr->url_num->request_type == WSS_REQUEST))
          ws_avgtime->ws_num_con_fail++; 
        else if ((cptr->url_num->request_type == SOCKET_REQUEST) || (cptr->url_num->request_type == SSL_SOCKET_REQUEST))
        {
          if (IS_TCP_CLIENT_API_EXIST)
            fill_tcp_client_avg(vptr, CON_FAILED, 0);

          if (IS_UDP_CLIENT_API_EXIST)
            fill_udp_client_avg(vptr, CON_FAILED, 0);
        }
        else
          average_time->num_con_fail++;
        {
          char srcip[128];
          // char request_buf[MAX_LINE_LENGTH];
          strcpy(srcip, nslb_get_src_addr_ex(cptr->conn_fd, 0)); 
	  NS_EL_3_ATTR(EID_CONN_FAILURE, vptr->user_index,
                              vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL,
			      nslb_sockaddr_to_ip((struct sockaddr *)&cptr->cur_server, 1),
			      srcip, 
			      nslb_strerror(errno),
                              "Error in making connection.\nEvent Data:\n"
	                      "Script Name = %s, Page Name = %s  Request = %s Url = %s",
			       vptr->sess_ptr->sess_name,
			       vptr->cur_page->page_name,
			       proto_to_str(request_type),
			       get_request_string(cptr));
        }

        retry_connection(cptr, now, NS_REQUEST_CONFAIL);
        return -1;
      default:
        if (cptr->url_num->request_type == SMTP_REQUEST || cptr->url_num->request_type == SMTPS_REQUEST)
          average_time->smtp_num_con_fail++; 
        else if ((cptr->url_num->request_type == POP3_REQUEST) || (cptr->url_num->request_type == SPOP3_REQUEST))
          average_time->pop3_num_con_fail++; 
        else if (cptr->url_num->request_type == FTP_REQUEST  ||
             cptr->url_num->request_type == FTP_DATA_REQUEST)
          ftp_avgtime->ftp_num_con_fail++; 
        else if (cptr->url_num->request_type == DNS_REQUEST)
          average_time->dns_num_con_fail++; 
        else if (cptr->url_num->request_type == LDAP_REQUEST || cptr->url_num->request_type == LDAPS_REQUEST)
          ldap_avgtime->ldap_num_con_fail++; 
        else if (cptr->url_num->request_type == IMAP_REQUEST || cptr->url_num->request_type == IMAPS_REQUEST)
          imap_avgtime->imap_num_con_fail++; 
        else if (cptr->url_num->request_type == JRMI_REQUEST) 
          jrmi_avgtime->jrmi_num_con_fail++; //JRMI
        else if ((cptr->url_num->request_type == WS_REQUEST) || (cptr->url_num->request_type == WSS_REQUEST))
          ws_avgtime->ws_num_con_fail++;
        else if ((cptr->url_num->request_type == SOCKET_REQUEST) || (cptr->url_num->request_type == SSL_SOCKET_REQUEST))
        {
          if (IS_TCP_CLIENT_API_EXIST)
            fill_tcp_client_avg(vptr, CON_FAILED, 0);

          if (IS_UDP_CLIENT_API_EXIST)
            fill_udp_client_avg(vptr, CON_FAILED, 0);
        }
        else
          average_time->num_con_fail++; 
        {
          char srcip[128];
          // char request_buf[MAX_LINE_LENGTH];
          strcpy(srcip, nslb_get_src_addr_ex(cptr->conn_fd, 0)); 
	  NS_EL_3_ATTR(EID_CONN_FAILURE, vptr->user_index,
                              vptr->sess_inst, EVENT_CORE, EVENT_CRITICAL,
			      nslb_sockaddr_to_ip((struct sockaddr *)&cptr->cur_server, 1),
			      srcip, 
			      nslb_strerror(errno),
                              "Error in making connection.\nEvent Data:\n"
	                      "Script Name = %s, Page Name = %s  Request = %s Url = %s",
			       vptr->sess_ptr->sess_name,
			       vptr->cur_page->page_name,
			       proto_to_str(request_type),
			       get_request_string(cptr));
        }
        // Close connection as we cannot recover from this error on retry
        if(cptr->request_type == DNS_REQUEST && cptr->conn_link){
          connection *http_cptr = cptr->conn_link;
          FREE_AND_MAKE_NULL(cptr->data, "Freeing cptr data", -1);
          free_dns_cptr(cptr);
          http_cptr->conn_link = NULL;
          NSDL2_SOCKETS(NULL, NULL, "Close connection as we cannot recover from this error on retry");
          dns_http_close_connection(http_cptr, 0, now, 1, NS_COMPLETION_NOT_DONE);
         }else{
           Close_connection(cptr, 0, now, NS_REQUEST_CONFAIL, NS_COMPLETION_NOT_DONE);
         }
        return -1;
    }
  }
  inc_con_num_and_succ(cptr);
  return 0;
}

void
SetSockMinBytes(int fd, int minbytes)
{
  NSDL2_SOCKETS(NULL, NULL, "Method called");
  int value=minbytes;
  //int size=sizeof(int);

    return;
    if (setsockopt(fd, SOL_SOCKET, SO_RCVLOWAT, (char*) &value, sizeof(int)) == -1) {
      close(fd);
      NS_EXIT(-1, "Failed to set socket opetion. MIN Bytes [%s]", nslb_strerror(errno));
    }
#if 0
    value = 0;
    if (getsockopt(fd, SOL_SOCKET, SO_RCVLOWAT, (char*) &value, &size) == -1) {
      printf("error in getting  MIN Bytes [%s]\n", nslb_strerror(errno));
      close(fd);
      exit(-1);
    }
    printf("MIN Bytes [%d]\n", value);
#endif
}

inline void
handle_server_close (connection *cptr, u_ns_ts_t now) {

  int timer_type = cptr->timer_ptr->timer_type;

  NSDL2_SOCKETS(NULL, cptr, "Method called");

  //if((cptr->request_type == SOCKET_REQUEST) || (cptr->request_type == SSL_SOCKET_REQUEST))
  if (IS_REQUEST_SSL_OR_NONSSL_SOCKET)
  {
    if (IS_TCP_CLIENT_API_EXIST)
      fill_tcp_client_avg(cptr->vptr, CON_CLOSED_BY_PEER, 0);

    if (IS_UDP_CLIENT_API_EXIST)
      fill_udp_client_avg(cptr->vptr, CON_CLOSED_BY_PEER, 0);
  }

  /*bug 54315 : as url_num is zero when conn_state either CNST FREE or REUSE, so just call close_fd and release cptr,
              instead of calling close_connection as it may use url_num and lread to crasah*/
  if( (cptr->http_protocol == HTTP_MODE_HTTP2) && (cptr->conn_state <= CNST_REUSE_CON) )
   {
     close_fd_and_release_cptr(cptr, NS_FD_CLOSE_REMOVE_RESP, now);
     return;
   } 

  if (cptr->req_code_filled < 0) {
    if(timer_type == AB_TIMEOUT_KA)
       close_fd_and_release_cptr(cptr, NS_FD_CLOSE_REMOVE_RESP, now);
       /* means that this connection is a keep alive or new and the server closed it without sending any dat*/
    retry_connection(cptr, now, NS_REQUEST_BAD_HDR);
    } else {
      if ((cptr->content_length == -1) && (cptr->conn_state == CNST_READING)) {
        /* Means that we are expecting a close from server when document finished */
        Close_connection(cptr, 0, now, get_req_status(cptr), NS_COMPLETION_CLOSE);
      } else {
        NSDL1_HTTP(NULL, cptr, "BADBYTE:3 content_length=%u req_code_filled=%d"
                               " req_code=%d bytes=%u, tcp_in=%u cur=%u"
                               " first=%d start=%llu connect_time=%d"
                               " write=%d",
                               cptr->content_length, cptr->req_code_filled,
                               cptr->req_code, cptr->bytes,
                               cptr->tcp_bytes_recv, get_ms_stamp(),
                               cptr->first_byte_rcv_time, cptr->started_at,
                               cptr->connect_time,
                               cptr->write_complete_time);

         Close_connection(cptr, 0, now, NS_REQUEST_BADBYTES, NS_COMPLETION_CLOSE);
      }
   }
}

// Note - For high per mode, we do not do double_check_connect(),
//        so need to inc counters in this macro
#define DOUBLE_CHECK_CONNECT() \
  if(global_settings->high_perf_mode == 0) \
  { \
    if ( double_check ) \
    { \
      if (double_check_connect(cptr, now) == -1) \
        return; \
    } \
  } \
  else \
  { \
    if(double_check) \
      inc_con_num_and_succ(cptr); \
  }\
  now = get_ms_stamp(); \
  cptr->connect_time = now - cptr->ns_component_start_time_stamp; \
  cptr->ns_component_start_time_stamp = now; \
  SET_MIN (average_time->url_conn_min_time, cptr->connect_time); \
  SET_MAX (average_time->url_conn_max_time, cptr->connect_time); \
  average_time->url_conn_tot_time += cptr->connect_time; \
  average_time->url_conn_count++; \
  UPDATE_GROUP_BASED_NETWORK_TIME(connect_time, url_conn_tot_time, url_conn_min_time, url_conn_max_time, url_conn_count);

/*make_request() is returned with MR_IO_VECTOR_MAX_LIMIT,when the iovectors reaches the maximum value.Earlier we were ending the test run but now instead of stopping the test we will fail the page. Also conn_state was set to CNST_CONNECTING (for 1U and sessions after 8), hence no connection was available, as in close_fd() conn counters were not decremented, so it is set to CNST_WRITING*/ 
#define MAKE_REQUEST() \
  if((cptr->http_protocol == HTTP_MODE_HTTP2)) {\
    NSDL2_HTTP2(NULL, NULL, "HTTP2 : num_vector = [%d] \n", num_vectors);\
    ret = http2_make_request (cptr, &num_vectors, now); \
  }\
  else \
   ret = make_request(cptr, &num_vectors, now);\
  \
  if (ret < 0) { \
    NS_FREE_RESET_IOVEC(g_req_rep_io_vector)\
    if(ret == MR_USE_ONCE_ABORT) \
      return; \
    if (ret == NS_STREAM_ERROR) \
      return;\
    if(ret == MR_IO_VECTOR_MAX_LIMIT){ \
      Close_connection(cptr, 0, now, NS_REQUEST_ERRMISC, NS_COMPLETION_NOT_DONE);\
    }\
    else{\
      Close_connection(cptr, 0, now, NS_REQUEST_ERRMISC, NS_COMPLETION_NOT_DONE);\
    }\
    return; \
  }\
  \
  stream *sptr;\
  int sptr_data_to_send = 0;\
  if(cptr->http_protocol == HTTP_MODE_HTTP2 && cptr->request_type == HTTP_REQUEST)\
    sptr = cptr->http2->http2_cur_stream;\
  for (i=0; i < num_vectors; i++) { \
    NSDL2_HTTP2(NULL, NULL, "Request: iov len = %d", g_req_rep_io_vector.vector[i].iov_len);\
    if(cptr->http_protocol == HTTP_MODE_HTTP2 && cptr->request_type == HTTP_REQUEST){\
      if(sptr_data_to_send + g_req_rep_io_vector.vector[i].iov_len <= sptr->flow_control.remote_window_size){\
        sptr_data_to_send += g_req_rep_io_vector.vector[i].iov_len;\
        sptr->num_vectors = i + 1;\
      }\
    }\
     http_size += g_req_rep_io_vector.vector[i].iov_len;  /*Here we are calculating Header size */ \
  }\
  if(cptr->http_protocol == HTTP_MODE_HTTP2 && cptr->request_type == HTTP_REQUEST){\
     sptr->http_size_to_send = http_size;\
  }

void handle_connect( connection* cptr, u_ns_ts_t now, int double_check)
{
  char send_buf[SEND_BUFFER_SIZE];
  int i, class, file;
  int num_vectors = 0, http_size = 0;
  int bytes_sent = 0;
  int request_type;
  int ret;
  int ws_req_size = 0;
  int update_ssl_settings = 0; 
   
  VUser* vptr = cptr->vptr;

  ClientData client_data;

  client_data.p = cptr;
 
  NSDL2_SOCKETS(NULL, cptr, "Method called, cptr=%p dc=%d state=%d.", cptr, double_check, cptr->conn_state);

  //request_type = cptr->url_num->request_type;
  request_type = cptr->request_type;
  NSDL2_SOCKETS(NULL, cptr, "request_type=%d, conn_state=%d", request_type, cptr->conn_state);

  NSDL2_SOCKETS(NULL, cptr, "cptr->cflags=0x%x", cptr->flags);
  /* num_pipe < 2 since if there area already requests in pipe we do not reopen
     * the connections. */
  if (( cptr->conn_state != CNST_REUSE_CON &&
       (!runprof_table_shr_mem[vptr->group_num].gset.enable_pipelining || cptr->num_pipe < 2))) { /* CNST_REUSE_CON is only set in close_fd; 
                                               * so new connection will always fall here */
    switch (request_type)
    {
      case HTTP_REQUEST:
      case WS_REQUEST:
      case SOCKET_REQUEST:
        DOUBLE_CHECK_CONNECT();
        //In case of HTTP SSL handshake diff should be zero
        cptr->ssl_handshake_time = now - cptr->ns_component_start_time_stamp;
        cptr->ns_component_start_time_stamp = get_ms_stamp();//Update component time

        SET_MIN (average_time->url_ssl_min_time, cptr->ssl_handshake_time);
        SET_MAX (average_time->url_ssl_max_time, cptr->ssl_handshake_time);
        average_time->url_ssl_tot_time += cptr->ssl_handshake_time;
        average_time->url_ssl_count++;

        UPDATE_GROUP_BASED_NETWORK_TIME(ssl_handshake_time, url_ssl_tot_time, url_ssl_min_time, url_ssl_max_time, url_ssl_count);

        if (IS_TCP_CLIENT_API_EXIST)
          fill_tcp_client_avg(vptr, CON_TIME, cptr->connect_time);

        if (IS_UDP_CLIENT_API_EXIST)
          fill_udp_client_avg(vptr, CON_TIME, cptr->connect_time);

        NSDL2_SOCKETS(NULL, cptr, "HTTP SSL handshake diff time stamp = %d, "
            "updated component time stamp = %u", cptr->ssl_handshake_time, cptr->ns_component_start_time_stamp);

        break;
#ifdef ENABLE_SSL
      /* Do SSL connect */
      case HTTPS_REQUEST:
      case WSS_REQUEST:
      case SSL_SOCKET_REQUEST:
      case IMAPS_REQUEST:
      case SPOP3_REQUEST:
      case SMTPS_REQUEST:
      case XMPPS_REQUEST:
      case LDAPS_REQUEST:
        if (cptr->http2_state == HTTP2_SETTINGS_ACK || cptr->http2_state == HTTP2_SETTINGS_DONE) {
          break;   
        }
        if (cptr->conn_state == CNST_CONNECTING)  // Must used curly braces as macro has if
        { 
           DOUBLE_CHECK_CONNECT();
        }
      
         /*Manish: here we are making proxy connection if proxy is enable
          * and if bit NS_CPTR_FLAGS_CON_USING_PROXY_CONNECT not set*/
        //No need to check for PROXY_ENABLE bit as PROXY_ENABLE and PROXY_CONNECT bits will be 
        //set togeather
        if(cptr->flags & NS_CPTR_FLAGS_CON_USING_PROXY_CONNECT)
        {
          /*bug 52092 : added calling HTTP and HTTP2 PROXY accordingly*/
          make_proxy_and_send_connect(cptr, vptr, now);
          return;
        }
   
        if ( cptr->conn_state != CNST_SSLCONNECTING)
        {
          if (set_ssl_con (cptr))
          {
            retry_connection(cptr, now, NS_REQUEST_SSL_HSHAKE_FAIL); //ERR_ERR
            return;
          }
          cptr->conn_state = CNST_SSLCONNECTING;
          NSDL2_SOCKETS(NULL, cptr, "request_type=%d, conn_state=%d", request_type, cptr->conn_state);
        }
        NSDL1_SSL(NULL, cptr, "handle_connect:about to do SSL_connect");

        // Using get_svr_entry for knowing the server_name for a particular url. svr_entry has the server_name along with port so we remove the port
        // and pass it as a hostname to the openssl method SSL_set_tlsext_host_name which uses the TLS SNI extension to set the hostname
        if (runprof_table_shr_mem[vptr->group_num].gset.ssl_settings & SSL_SNI_ENABLED) {
          PerHostSvrTableEntry_Shr* svr_entry = NULL;
          svr_entry = get_svr_entry(vptr, cptr->url_num->index.svr_ptr);

  	  char host_name[2*1024]; //Hostname should not be greater than 2k
          char *port = strchr(svr_entry->server_name, ':');
          NSDL1_SSL(NULL, cptr, "server port is = %s", port);

          if (port != NULL) {
	    strncpy(host_name, svr_entry->server_name, port - svr_entry->server_name);
          }
 	  else {
	    strcpy(host_name, svr_entry->server_name);
  	  }
          NSDL1_SSL(NULL, cptr, "server host_name= %s", host_name);

          if (!SSL_set_tlsext_host_name(cptr->ssl, host_name)) {
            NSDL1_SSL(NULL, cptr, "Unable to send SNI for hostname = %s", host_name);
          }
        }

        /*Bug-84169: Changes for getting secret keys with tls1.3 (openssl1.1.1b) */
        #if OPENSSL_VERSION_NUMBER >= 0x10100000L
        if(global_settings->ssl_key_log)
        {
          SSL_CTX *ssl_ctx = SSL_get_SSL_CTX(cptr->ssl);
          if (set_keylog_file(ssl_ctx, global_settings->ssl_key_log_file))
            fprintf(stderr,"UNABLE TO USE KEYLOGGING\n");
        }
        #endif

        i = SSL_connect(cptr->ssl);
        NSDL1_SSL(NULL, cptr, "handle_connect:SSL_connect done");
        switch (SSL_get_error(cptr->ssl, i))
        {
          case SSL_ERROR_NONE:
            NSDL1_CONN(NULL, cptr, "handle_connect:case no err ");
            break;
          case SSL_ERROR_WANT_READ:
            NSDL1_CONN(NULL, cptr, "handle_connect:case want read err ");
#ifndef USE_EPOLL
            FD_SET( cptr->conn_fd, &g_rfdset );
#endif
            return;
          case SSL_ERROR_WANT_WRITE:
            NSDL1_CONN(NULL, cptr, "handle_connect:case want write err ");
            /* It can but isn't supposed to happen */
            /*bug 54315: trace moved to log file*/
            NSTL1(vptr, NULL, "SSL_connect error: SSL_ERROR_WANT_WRITE");
	    NSDL1_CONN(NULL, cptr, "SSL_connect error: SSL_ERROR_WANT_WRITE ");
            //Close_connection(cptr, 0, now, NS_REQUEST_SSL_HSHAKE_FAIL, NS_COMPLETION_NOT_DONE); //ERR_ERR
            retry_connection(cptr, now, NS_REQUEST_SSL_HSHAKE_FAIL); //ERR_ERR
            return;
          case SSL_ERROR_SYSCALL:
            NSDL1_CONN(NULL, cptr, "handle_connect:case err syscall ");
            /*bug 54315: trace moved to log file*/
            NSTL1(vptr, NULL, "SSL_connect error: errno=%d", errno);
            NSDL1_CONN(NULL, cptr, "SSL_connect error: errno=%d", errno);

            retry_connection(cptr, now, NS_REQUEST_SSL_HSHAKE_FAIL); //ERR_ERR
            return;
          case SSL_ERROR_ZERO_RETURN:
            NSDL1_CONN(NULL, cptr, "handle_connect:case err zero ");
            /*bug 54315: trace moved to log file*/
            NSTL1(vptr, NULL, "SSL_connect error: aborted");
            NSDL1_CONN(NULL, cptr, "SSL_connect error: aborted");
            retry_connection(cptr, now, NS_REQUEST_SSL_HSHAKE_FAIL); //ERR_ERR
            return;
          case SSL_ERROR_SSL:
            NSDL1_CONN(NULL, cptr, "handle_connect:case err ssl ");
            //ERR_print_errors_fp(stderr);
            /*bug 54315: trace moved to log file*/
            char *err_buff = ERR_error_string(ERR_get_error(), NULL);
            NSTL1(vptr, NULL, "Cipher(s) %s  may not be suported by server error=%d, text=%s", group_default_settings->ssl_ciphers, SSL_ERROR_SSL, err_buff);
            NSDL1_CONN(NULL, cptr, "Cipher(s) %s  may not be suported by server error=%d, text=%s", group_default_settings->ssl_ciphers, SSL_ERROR_SSL, err_buff);
            retry_connection(cptr, now, NS_REQUEST_SSL_HSHAKE_FAIL); //ERR_ERR
            return;
          default:
            NSDL1_CONN(NULL, cptr, "handle_connect:case default ");
            /*bug 54315: trace moved to log file*/
            NSTL1(vptr, NULL, "SSL_connect error: unknown");
            NSDL1_CONN(NULL, cptr, "SSL_connect error: unknown");
            retry_connection(cptr, now, NS_REQUEST_SSL_HSHAKE_FAIL); //ERR_ERR
            return;
        }
        
        ssl_sess_save(cptr);

        #if OPENSSL_VERSION_NUMBER >= 0x10100000L
        /* Unsetting flag gset.ssl_regenotiation, because as per the rfc ssl renegotiation is forbidden in TLSv1.3.
           0x0304 - 772 (TLSv1.3), 0x0303 - 771 (TLSv1.2), etc.  */
        if (SSL_get_min_proto_version(cptr->ssl) == 772)
        {
          runprof_table_shr_mem[vptr->group_num].gset.ssl_regenotiation = 0;
          NSTL1(vptr, NULL, "TLSv1.3 does not support SSL_RENEGOTIATION"); 
        }
        #endif

        if (runprof_table_shr_mem[vptr->group_num].gset.ssl_regenotiation == 1){
          NSDL1_CONN(NULL, cptr, "SSL_RENEGOTIATION STARTED ");
          SSL_renegotiate(cptr->ssl);       //in case of renegotiation this is openssl api which starts renegotiation 
        }

        now = get_ms_stamp();//Get current time stamp
        //Calculate ssl handshake time stamp diff
        cptr->ssl_handshake_time = now - cptr->ns_component_start_time_stamp;
        cptr->ns_component_start_time_stamp = now;//Update component time stamp
        SET_MIN (average_time->url_ssl_min_time, cptr->ssl_handshake_time);
        SET_MAX (average_time->url_ssl_max_time, cptr->ssl_handshake_time);
        average_time->url_ssl_tot_time += cptr->ssl_handshake_time;
        average_time->url_ssl_count++;

        UPDATE_GROUP_BASED_NETWORK_TIME(ssl_handshake_time, url_ssl_tot_time, url_ssl_min_time, url_ssl_max_time, url_ssl_count);

        if(IS_TCP_CLIENT_API_EXIST)
        {
          ns_socket_modify_epoll(cptr, -1, EPOLLOUT);
          fill_tcp_client_avg(vptr, SSL_TIME, cptr->ssl_handshake_time);
        }

        if (IS_UDP_CLIENT_API_EXIST)
        {
          ns_socket_modify_epoll(cptr, -1, EPOLLOUT);
          fill_udp_client_avg(vptr, SSL_TIME, cptr->ssl_handshake_time);
        }

        NSDL1_CONN(NULL, cptr, "Calculate ssl handshake time stamp diff = %d, update component time stamp = %u", cptr->ssl_handshake_time, cptr->ns_component_start_time_stamp);
        break;
#endif
      case SMTP_REQUEST:
        DOUBLE_CHECK_CONNECT();
        /* Its a timer for Greeting time out.
         * If greeting comes before timer expires we delete this timer & add timer for other command.
         */
        delete_smtp_timeout_timer(cptr);
        cptr->timer_ptr->actual_timeout = runprof_table_shr_mem[vptr->group_num].gset.smtp_timeout_greeting;
        dis_timer_add(AB_TIMEOUT_IDLE, cptr->timer_ptr, now, smtp_timeout_handle, client_data, 0 );
        break;
      case POP3_REQUEST:
        DOUBLE_CHECK_CONNECT();
        /* Its a timer for Greeting time out.
         * If greeting comes before timer expires we delete this timer & add timer for other command.
         */
        delete_pop3_timeout_timer(cptr);
        cptr->timer_ptr->actual_timeout = runprof_table_shr_mem[vptr->group_num].gset.pop3_timeout;
        dis_timer_add(AB_TIMEOUT_IDLE, cptr->timer_ptr, now, pop3_timeout_handle, client_data, 0 );

        break;
      case FTP_REQUEST:
        DOUBLE_CHECK_CONNECT();
        /* Its a timer for Greeting time out.
         * If greeting comes before timer expires we delete this timer & add timer for other command.
         */
        delete_ftp_timeout_timer(cptr);

        dis_timer_add(AB_TIMEOUT_IDLE, cptr->timer_ptr, now, ftp_timeout_handle, client_data, 0 );

        break;

      case FTP_DATA_REQUEST:
        DOUBLE_CHECK_CONNECT();
        delete_ftp_timeout_timer(cptr);
        cptr->timer_ptr->actual_timeout =
        runprof_table_shr_mem[vptr->group_num].gset.ftp_timeout;

        dis_timer_add(AB_TIMEOUT_IDLE, cptr->timer_ptr,
        now, ftp_timeout_handle, client_data, 0 );

        /* Its a timer for Greeting time out.
         * If greeting comes before timer expires we delete this timer & add timer for other command.
         */

        break;

      case LDAP_REQUEST:
        DOUBLE_CHECK_CONNECT();
        delete_ldap_timeout_timer(cptr);
        cptr->timer_ptr->actual_timeout = runprof_table_shr_mem[vptr->group_num].gset.ldap_timeout;
        dis_timer_add(AB_TIMEOUT_IDLE, cptr->timer_ptr, now, ldap_timeout_handle, client_data, 0 );

        break;
      case DNS_REQUEST:
        DOUBLE_CHECK_CONNECT();
        
        cptr->timer_ptr->actual_timeout = runprof_table_shr_mem[vptr->group_num].gset.dns_timeout;
        dis_timer_add(AB_TIMEOUT_IDLE, cptr->timer_ptr, now, dns_timeout_handle, client_data, 0 );

        break;

      case IMAP_REQUEST:
        DOUBLE_CHECK_CONNECT();
        delete_imap_timeout_timer(cptr);
        cptr->timer_ptr->actual_timeout = runprof_table_shr_mem[vptr->group_num].gset.imap_timeout;
        dis_timer_add(AB_TIMEOUT_IDLE, cptr->timer_ptr, now, imap_timeout_handle, client_data, 0 );

        break;

      case JRMI_REQUEST:
        DOUBLE_CHECK_CONNECT();
        delete_jrmi_timeout_timer(cptr);
        cptr->timer_ptr->actual_timeout = runprof_table_shr_mem[vptr->group_num].gset.jrmi_timeout;
        dis_timer_add(AB_TIMEOUT_IDLE, cptr->timer_ptr, now, jrmi_timeout_handle, client_data, 0 );
        break;

      case XMPP_REQUEST:
        DOUBLE_CHECK_CONNECT();
        //Need to implement seprate timer for xmpp
        break;

      case FC2_REQUEST:
       DOUBLE_CHECK_CONNECT();

#ifdef RMI_MODE
      case JBOSS_CONNECT_REQUEST:
        DOUBLE_CHECK_CONNECT();
        cptr->conn_state = CNST_JBOSS_CONN_READ_FIRST;
        cptr->total_bytes = cptr->url_num->proto.http.first_mesg_len; /* amt of bytes to expect from the first read */
        handle_jboss_read(cptr, now);
        return;

      case RMI_CONNECT_REQUEST:
        DOUBLE_CHECK_CONNECT();
        cptr->conn_state = CNST_RMI_CONN_READ_VERIFY;
        handle_rmi_connect(cptr, now);
        return;

      case RMI_REQUEST:
        DOUBLE_CHECK_CONNECT();
        break;

      case PING_ACK_REQUEST:
        DOUBLE_CHECK_CONNECT();
        break;

#endif
    }
    /*Fix BUG#21149, where core dump came when using G_USE_DNS ALL 2 1 1 1, because in case of DNS we cannot fill server entry struct 
     * at start_new_socket() and we go for accessing.*/
    if(cptr->url_num->index.svr_ptr != NULL) {
      // Fixes for bug #20153
      NS_DT3(vptr, cptr, DM_LOGIC3, MM_CONN, "Connected for host (%s) on fd = %d." , get_svr_entry(vptr, cptr->url_num->index.svr_ptr)->server_name, cptr->conn_fd);
    } else {
      NS_DT3(vptr, cptr, DM_LOGIC3, MM_CONN, "svr_ptr is NULL for request_type = %d", request_type);
    }
  }   
  /* Resending Connect request in case of proxy authentication 
   * In case of auth connection state is set to REUSE
   */
  else if(cptr->flags & NS_CPTR_FLAGS_CON_USING_PROXY_CONNECT)
  {
    NSDL2_CONN(NULL, cptr, "handle_connect:case want write err ");
    /*bug 52092 : added calling HTTP and  HTTP2 PROXY accordingly*/
    if ( make_proxy_and_send_connect(cptr, vptr, now) != PROXY_SUCCESS )
         NS_FREE_RESET_IOVEC(g_req_rep_io_vector); //Currently allocating in case of auth only
    return;
  }

  NSDL2_SSL(NULL, cptr, "VPTR : SSL Setting are cert_idx = %d, and key_idx = %d", vptr->httpData->ssl_cert_id, vptr->httpData->ssl_key_id);
  NSDL2_SSL(NULL, cptr, "CPTR : SSL Setting are cert_idx = %d, and key_idx = %d", cptr->ssl_cert_id, cptr->ssl_key_id);

  //When not in SSL connecting state or new SSL certificate or Key is applied 
  //Example : cptr have cert id as 3, and user applied API() for new ssl setting, then vptr->httpData have cert_id as 4, 
  //          then will do ssl setting again with new configuration
  update_ssl_settings = ( request_type == HTTPS_REQUEST) && (cptr->ssl_cert_id != vptr->httpData->ssl_cert_id || cptr->ssl_key_id != vptr->httpData->ssl_key_id);
  NSDL3_SSL(NULL, cptr, "update_ssl_settings = %d", update_ssl_settings);
  if(update_ssl_settings)
  {
     NSDL3_SSL(NULL, cptr, "SSL setting are updated, going to reconnect SSL");
     set_ssl_recon(cptr);
  }

  /* Change cptr->proto_state to HTTP2_HANDSHAKE_DONE.  This means that  HTTP2 Settings are
   * negotiated between Client and server .
   */
  if (cptr->http2_state == HTTP2_SETTINGS_ACK) { 
    NSDL2_CONN(NULL, NULL, "setting state to HTTP2_HANDSHAKE_DONE");
    cptr->http2_state = HTTP2_HANDSHAKE_DONE;
  }

  // Support for Http2. On sucessfull connection in case of Http2 (mode 2) we send connection preface followed by setting frame
  NSDL2_CONN(NULL, cptr, "HTTP mode is [%d] hptr->http_mode is [%hd]  ", cptr->http_protocol, vptr->hptr->http_mode);
  if (cptr->conn_state != CNST_REUSE_CON){
    if ((request_type == HTTP_REQUEST || request_type == HTTPS_REQUEST) && (cptr->http_protocol== HTTP_MODE_HTTP2)){
      if (cptr->http2_state != HTTP2_HANDSHAKE_DONE) {
        cptr->tcp_bytes_sent = 0;  // Required in HTTP2 ssl writing
        init_cptr_for_http2(cptr);
        /*bug 93672: gRPC - send ping frame after sending SETTING FRAME*/
        h2_make_handshake_and_ping_frames(cptr, now);
        return; 
      }     
    }
  }
 
  // Support for FC2. 
  if (request_type == FC2_REQUEST){
    if(cptr->fc2_state == FC2_SEND_HANDSHAKE)
      cptr->tcp_bytes_sent = 0;  
    init_fc2(cptr,vptr);
    fc2_make_and_send_frames(vptr, now);
    return;
  }

  //Setting cur_vptr for ns_eval_string 
  TLS_SET_VPTR(vptr);

  /*Socket API Support: Now Socket is conected, Set cptr state to connected and switch context,
    as we don't have to do anything else
    TODO: Need to handle fot thread and Java mode*/
  if((request_type == SOCKET_REQUEST) || (request_type == SSL_SOCKET_REQUEST))
  {
    NSDL2_CONN(NULL, NULL, "Socket connected, switch to VUser context");

    cptr->conn_state = CNST_IDLE;

    if(cptr->timer_ptr->timer_type == AB_TIMEOUT_IDLE)
      dis_timer_del(cptr->timer_ptr);

    if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT)
      switch_to_vuser_ctx(vptr, "Socket connected");
    else if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_SEPARATE_THREAD)
      send_msg_nvm_to_vutd(vptr, NS_API_WEB_URL_REP, 0);

    NSDL2_CONN(NULL, NULL, "socket connected, cptr->conn_state = %d, cptr->req_ok = %d", cptr->conn_state, cptr->req_ok);
    return;
  }

  if (global_settings->num_dirs)
  {
    cptr->dir = rand()%(global_settings->num_dirs);
    get_file_request(send_buf, KA, cptr->dir, &class, &file);
    cptr->file = file;
    cptr->class = class;
  }
  else
  {
    NSDL2_CONN(NULL, cptr, "Making Requst according to protocol, request_type = %d", request_type);
    switch (request_type)
    {
#ifdef RMI_MODE
      case RMI_REQUEST:
        if ((ret = make_rmi_request(cptr, &g_req_rep_io_vector)) == -1)
        {
          fprintf(stderr, "Handle_Connent(): Failed in creating the request to send\n");
          end_test_run();  /* If the make_rmi_request fails, it means there is a bug in the program */
        }
        
        break;
      case PING_ACK_REQUEST:
        break;
#endif
       case WS_REQUEST: 
       case WSS_REQUEST: 
         {
           int ret;
           NSDL2_CONN(NULL, cptr, "Making Websocket Connect Request");
           if((ret = make_ws_request(cptr, &ws_req_size, now)) == -1)
           {
             fprintf(stderr, "Failed in creating the request to send\n");
             end_test_run();
           }
           NSDL2_CONN(NULL, cptr, "ws_req_size= %d", ws_req_size);
         }
         break;
      default:
        if((!global_settings->replay_mode) && ((request_type == HTTP_REQUEST) || (request_type == HTTPS_REQUEST))) 
        {
          NSDL2_CONN(NULL, cptr, "Going to Make Request. cptr = %p, cptr->http_protocol = [%d]", cptr, cptr->http_protocol);
          MAKE_REQUEST();
        }
        else // Replay mode or other protocol. Only replay case to be handled here if NS code to be used
        {
          if(IS_REPLAY_TO_USE_NS_CODE())
          {
            MAKE_REQUEST();
          }
        }
        break;
    }
  }

#ifdef NS_DEBUG_ON
  if(!(global_settings->replay_mode))
  {
    // This must be called with complete data as it needs complete body
    amf_debug_log_http_req(cptr, g_req_rep_io_vector.vector, g_req_rep_io_vector.cur_idx);
    hessian_debug_log_http_req(cptr, g_req_rep_io_vector.vector, g_req_rep_io_vector.cur_idx);
    java_obj_debug_log_http_req(cptr, g_req_rep_io_vector.vector, g_req_rep_io_vector.cur_idx);
  }
#endif

   NSDL3_HTTP(NULL, cptr, "current flag value = 0x%X", vptr->flags);
   /* ReplayAccessLog: In release 3.9.0 changes are done for replay access log,
    * if replay_mode is enable and url is non redirecting then we need to process replay request.
    */
   if(IS_REPLAY_TO_USE_REPLAY_CODE()) 
   {
     process_replay_req(cptr, now);
     return;
   }
   
   NSDL3_HTTP(NULL, cptr, "cptr->url_num->proto.http.type = %d, runprof_table_shr_mem[vptr->group_num].gset.trace_inline_url = %d", cptr->url_num->proto.http.type, runprof_table_shr_mem[vptr->group_num].gset.trace_inline_url);

   if((NS_IF_TRACING_ENABLE_FOR_USER && cptr->url_num->proto.http.type != EMBEDDED_URL) ||
       /*For page dump, if inline is enabled then only dump the inline urls in page dump 
        *Otherwise dump only main url*/
       ((NS_IF_PAGE_DUMP_ENABLE_WITH_TRACE_ON_FAIL) && 
         (((cptr->url_num->proto.http.type == EMBEDDED_URL) && (runprof_table_shr_mem[vptr->group_num].gset.trace_inline_url == 1)) || (cptr->url_num->proto.http.type != EMBEDDED_URL))))
     
   {
     NSDL3_HTTP(NULL, cptr, "http_size = %d num_vectors = %d, ws_req_size = %d", http_size, g_req_rep_io_vector.cur_idx, ws_req_size);
     if(request_type == WS_REQUEST || request_type == WSS_REQUEST)
       ut_update_req_file(vptr, ws_req_size, g_req_rep_io_vector.cur_idx, g_req_rep_io_vector.vector);
    else if (cptr->http_protocol != HTTP_MODE_HTTP2) //Temporary fix for G_TRACING ALL 4 1 2 0 0 0 for HTTP2(Need to understand detail design) 
       ut_update_req_file(vptr, http_size, g_req_rep_io_vector.cur_idx, g_req_rep_io_vector.vector);
   }
   
  /* For HTTP/HTTPS request if Expect: 100 Continue & POST BODY is there then we need
   * to send only header part, rest (BODY part) we will send when 100 Continue response will come
   * 
   */
  switch (request_type)
  {
#ifdef ENABLE_SSL
    case HTTPS_REQUEST:
    case WSS_REQUEST:
      if(IS_HTTP2_INLINE_REQUEST) { 
        http2_handle_ssl_write_ex(vptr, cptr, now, http_size, g_req_rep_io_vector.cur_idx);
        //In case of HTTPs/H2, increment num of req. sent in case of inline
        average_time->fetches_sent++; 
        if(SHOW_GRP_DATA) { 
          avgtime *lol_average_time;
          lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
          lol_average_time->fetches_sent++;
        }
        return;
      }
      if(request_type == HTTPS_REQUEST)
        copy_request_into_buffer(cptr, http_size, &g_req_rep_io_vector);
      else if(request_type == WSS_REQUEST)
      {
        NSDL4_HTTP(NULL, cptr, "Sending WSS Connect request ws_req_size = %d, num_vectors = %d", ws_req_size, g_req_rep_io_vector.cur_idx);
        copy_request_into_buffer(cptr, ws_req_size, &g_req_rep_io_vector);
        cptr->header_state = HDST_BOL;
      }
      if(vptr->flags & NS_SOAP_WSSE_ENABLE)
      {
        NSDL4_HTTP(NULL, cptr, "Apply soap ws security in outgoing soap xml");
        char *outbuf;
        int outbuf_len;
        nsSoapWSSecurityInfo *ns_ws_info = vptr->ns_ws_info;
        if(ns_ws_info && (ns_apply_soap_ws_security(ns_ws_info, cptr->free_array, http_size, &outbuf , &outbuf_len) == 0))
        {
          NSDL4_HTTP(NULL, cptr, "Successfully applied ws security xml, free previous memory");
          FREE_AND_MAKE_NOT_NULL(cptr->free_array, "cptr->free_array", -1);
          cptr->free_array = outbuf;
          //cptr->content_length +=  outbuf_len - http_size; 
          cptr->bytes_left_to_send += outbuf_len - http_size; 
        }
      } 
      if(set_socketopt_for_quickack(cptr->conn_fd, 1) < 0)
      {
        retry_connection(cptr, now, NS_REQUEST_CONFAIL);
        return;
      }
      handle_ssl_write (cptr, now);
      // Copy current cptr data to current stream, and add that node to queue
      if(cptr->http_protocol == HTTP_MODE_HTTP2 && cptr->url_num->proto.http.type == EMBEDDED_URL){
        HTTP2_SET_INLINE_QUEUE
      }
      //In case of HTTPs, increment num of req. sent
      average_time->fetches_sent++; 
      if(SHOW_GRP_DATA) { 
        avgtime *lol_average_time;
        lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
        lol_average_time->fetches_sent++;
      }
      return;
#endif
    case HTTP_REQUEST:
      // If num vectors is greater the IOV_MAX(kernel limit of writev to send iovectors) or WS Security flag is set , then copy all the vectors in to buffer just like we do in https and send it using https
      //TODO: IOVEC_CHANGE
      if(g_req_rep_io_vector.cur_idx > IOV_MAX || vptr->flags & NS_SOAP_WSSE_ENABLE)
      {
        copy_request_into_buffer(cptr, http_size, &g_req_rep_io_vector);
        if(vptr->flags & NS_SOAP_WSSE_ENABLE)
        {
          NSDL4_HTTP(NULL, cptr, "Apply soap ws security in outgoing soap xml");
          char *outbuf;
          int outbuf_len;
          nsSoapWSSecurityInfo *ns_ws_info = vptr->ns_ws_info;
          if(ns_ws_info && (ns_apply_soap_ws_security(ns_ws_info, cptr->free_array, http_size, &outbuf , &outbuf_len) == 0))
          {
            NSDL4_HTTP(NULL, cptr, "Successfully applied ws security xml, free previous memory");
            FREE_AND_MAKE_NOT_NULL(cptr->free_array, "cptr->free_array", -1);
            cptr->free_array = outbuf;
            http_size = outbuf_len;
          }
        }
        else
        {
          NSDL4_HTTP(NULL, cptr, "num_vectors exceeds IOV_MAX, copying vectors to single buffer");
        }
        NS_RESET_IOVEC(g_req_rep_io_vector);
        NS_FILL_IOVEC_AND_MARK_FREE(g_req_rep_io_vector, cptr->free_array, http_size);
        cptr->free_array = NULL; // Make it null as we are checking it in idle_connection in case of TIMEOUT
      }

      if(IS_HTTP2_INLINE_REQUEST) {
        h2_send_http_req_ex(cptr, http_size, &g_req_rep_io_vector, now);
      } else { 
        send_http_req(cptr, http_size, &g_req_rep_io_vector, now);
        // Copy current cptr data to current stream, and add that node to queue
        if(cptr->http_protocol == HTTP_MODE_HTTP2 && cptr->url_num->proto.http.type == EMBEDDED_URL){
          HTTP2_SET_INLINE_QUEUE
        }
      }
      //In case of HTTP, increment num of req. sent
      average_time->fetches_sent++; 
      if(SHOW_GRP_DATA) { 
        avgtime *lol_average_time;
        lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
        lol_average_time->fetches_sent++;
      }
      return;

    case WS_REQUEST: 
    {
      NSDL4_HTTP(NULL, cptr, "Sending WS Connect request ws_req_size = %d, num_vectors = %d", ws_req_size, g_req_rep_io_vector.cur_idx);
      //TODO: Need To Discuss IOVEC_CHANGE
      if(g_req_rep_io_vector.cur_idx > IOV_MAX){
        NSDL4_HTTP(NULL, cptr, "num_vectors exceeds IOV_MAX, copying vetors to single buffer");
        copy_request_into_buffer(cptr, ws_req_size, &g_req_rep_io_vector);
        NS_FILL_IOVEC_AND_MARK_FREE(g_req_rep_io_vector, cptr->free_array, http_size);
        cptr->free_array = NULL; // Make it null as we are checking it in idle_connection in case of TIMEOUT
        num_vectors = 1;  
      }
      send_ws_connect_req(cptr, ws_req_size, now);
      cptr->header_state = HDST_BOL;
      return;
    }
    case SMTP_REQUEST: 
    case SMTPS_REQUEST: {
      int retval;
      /* Set initial states for SMTP */
      if(cptr->proto_state != ST_SMTP_STARTTLS_LOGIN){
        NSDL4_SMTP(NULL, cptr, "Setting smtp_state = ST_SMTP_CONNECTED and conn_state = CNST_READING\n");
        cptr->proto_state = ST_SMTP_CONNECTED;
        cptr->header_state = SMTP_HDST_RCODE_X;
        cptr->conn_state = CNST_READING; /* we will be reading now for +ve response from server. */
      }
       
      if(cptr->proto_state == ST_SMTP_STARTTLS_LOGIN){
        NSDL4_SMTP(NULL, cptr, "Sending EHLO again over SSL connection");
        smtp_send_ehlo(cptr, now); 
      }

      /* we have to call handle_smtp_read() here since we might get
       * 220 immidiately after connecting from server. Maybe not use EPOLLET ?? */
      retval = handle_smtp_read(cptr, now);
      if (retval == 0) {
        NSDL3_SMTP(NULL, cptr, "returned after processing now in state = %s", 
                   smtp_state_to_str(cptr->proto_state));
      }
      return;
    }
    case POP3_REQUEST: 
    case SPOP3_REQUEST:{ 
      int retval;
      /* Set initial states for pop3 */
      if(cptr->proto_state != ST_POP3_STLS_LOGIN){
        NSDL4_POP3(NULL, cptr, "Setting pop3_state = ST_pop3_CONNECTED and conn_state = CNST_READING\n");
        cptr->proto_state = ST_POP3_CONNECTED;
        cptr->header_state = POP3_HDST_NEW;
        cptr->conn_state = CNST_READING;
      }

      if(cptr->proto_state == ST_POP3_STLS_LOGIN){
         pop3_send_user(cptr, now); 
      }
      /* we will be reading now for +ve response from server. */
      /* we have to call handle_pop3_read() here since we might get
       * +OK immidiately after connecting from server. Maybe not use EPOLLET ?? */
      retval = handle_pop3_read(cptr, now);
      if (retval == 0) {
        NSDL3_POP3(NULL, cptr, "returned after processing now in state = %s", 
                   pop3_state_to_str(cptr->proto_state));
      }
      return;
    }
    case FTP_REQUEST: {
      int retval;
      /* Set initial states for pop3 */
      NSDL4_FTP(NULL, cptr, "Setting FTP_state = ST_FTP_CONNECTED and conn_state = CNST_READING\n");
      cptr->proto_state = ST_FTP_CONNECTED;
      cptr->header_state = FTP_HDST_RCODE_X;
      cptr->conn_state = CNST_READING; /* we will be reading now for +ve response from server. */
      /* we have to call handle_ftp_read() here since we might get
       * 220 immidiately after connecting from server. Maybe not use EPOLLET ?? */
      retval = handle_ftp_read(cptr, now);
      if (retval == 0) {
        NSDL3_FTP(NULL, cptr, "returned after processing now in state = %s", 
                   ftp_state_to_str(cptr->proto_state));
      }
      return;
    }
    case FTP_DATA_REQUEST: {
      NSDL3_FTP(NULL, cptr, "we have connected to the ftp server's data port"); 

      /* Set initial states for ftp data con */

      /* Add timer on data connection. Similar timer will exist on cotrol link */
      add_ftp_timeout_timer(cptr, now);

      /* Data might be available immediately  */
      if(cptr->url_num->proto.ftp.ftp_action_type == FTP_ACTION_RETR){
        cptr->conn_state = CNST_READING;
        handle_ftp_data_read(cptr, now);
      }
      return;
    }
    case DNS_REQUEST: 
    {
      NSDL4_DNS(NULL, cptr, "Calling dns_make_request");
      if(cptr->conn_link)
      {
        char *ptr = NULL;
        char svr_name[1024 + 1];
        PerHostSvrTableEntry_Shr* svr_entry = ((connection*)(cptr->conn_link))->old_svr_entry;

        if((ptr = strchr(svr_entry->server_name, ':')) != NULL){
          strncpy(svr_name, svr_entry->server_name, ptr-svr_entry->server_name);
          svr_name[ptr-svr_entry->server_name] = '\0';
        }
        else
          strcpy(svr_name, svr_entry->server_name);
         
        dns_make_request(cptr, now, 1, svr_name);
      }
      else
      {
        dns_make_request(cptr, now, 0, NULL);
        NSDL4_DNS(NULL, cptr, "Returned from dns_make_request");
      }
      return;
    }
    case LDAP_REQUEST: 
    case LDAPS_REQUEST: {
      unsigned char *ldap_req_buffer;
      MY_MALLOC(ldap_req_buffer, LDAP_BUFFER_SIZE, "ldap_req_buffer", -1);
      int length = 0;  
      NSDL4_LDAP(NULL, cptr, "Calling ldap_make_request");
      make_ldap_request(cptr, ldap_req_buffer, &length);
      cptr->tcp_bytes_sent = 0;
      cptr->free_array = (char *)ldap_req_buffer;
      cptr->content_length = length;
      cptr->bytes_left_to_send = length;
      NSDL4_LDAP(NULL, cptr, "Returned from ldap_make_request");
      handle_ldap_write(cptr, now);
      return;
    }
    case JRMI_REQUEST: {
      NSDL4_JRMI(NULL, cptr, "Calling jrmi_make_request");
      cptr->proto_state = ST_JRMI_HANDSHAKE;
       make_jrmi_msg(cptr, now);
      NSDL4_JRMI(NULL, cptr, "Returned from jrmi_make_request");
      return;
    }
    #ifdef RMI_MODE
    case RMI_REQUEST:
      if ((bytes_sent = writev(cptr->conn_fd, g_req_rep_io_vector.vector, g_req_rep_io_vector.cur_idx)) < 0)
      {
        NS_FREE_RESET_IOVEC(g_req_rep_io_vector);
        fprintf(stderr, "sending RMI request failed\n");
        perror("rmi_request");
        retry_connection(cptr, now, NS_REQUEST_CONFAIL);
        return;
      }
      else
      {
        NS_FREE_RESET_IOVEC(g_req_rep_io_vector);
      }
      cptr->conn_state = CNST_RMI_READ;
      break;
    case PING_ACK_REQUEST:
    {
      char ping_packet = 0x52;
      VUser* vptr = cptr->vptr;
      if ((bytes_sent = write(cptr->conn_fd, &ping_packet, sizeof(char))) < 0)
      {
        fprintf(stderr, "sending PING ACK request: write() failed\n");
        if (cptr->conn_state == CNST_REUSE_CON)
          retry_connection(cptr, now, NS_REQUEST_CONFAIL);
        else
        {
          cptr->conn_state = CNST_RMI_READ;
          Close_connection(cptr, 0, now, NS_REQUEST_WRITE_FAIL, NS_COMPLETION_NOT_DONE);
        }
        return;
      }
      cptr->conn_state = CNST_PING_ACK_READ;
      break;
    }
#endif
    case IMAPS_REQUEST:
    case IMAP_REQUEST: {
      int retval;
     if(cptr->proto_state != ST_IMAP_TLS_LOGIN){ 
        NSDL4_IMAP(NULL, cptr, "Setting imap_state = ST_IMAP_CONNECTED and conn_state = CNST_READING, cptr->bytes = [%d]", cptr->bytes);
        cptr->proto_state = ST_IMAP_CONNECTED;
        cptr->header_state = IMAP_H_NEW;
        cptr->conn_state = CNST_READING;
      }
     
      if(cptr->proto_state == ST_IMAP_TLS_LOGIN){
         imap_send_login(cptr, now); 
      }

      /* we will be reading now for +ve response from server. */
      /* we have to call handle_imap_read() here since we might get server response immediately */
      retval = handle_imap_read(cptr, now);
      if (retval == 0) {
        NSDL3_IMAP(NULL, cptr, "returned after processing now in state = %s", 
                   imap_state_to_str(cptr->proto_state));
      }
      return;
    }

    case XMPPS_REQUEST:
    case XMPP_REQUEST: {
        NSDL4_XMPP(NULL, cptr, "Setting xmpp conn_state = CNST_READING, cptr->bytes = [%d]", cptr->bytes);
        cptr->conn_state = CNST_WRITING;
        xmpp_start_stream(cptr, now); 
        NSDL4_XMPP(NULL, cptr, "Returned from xmpp_start_stream");
        return;

    }

    default:
      NSDL4_HTTP(NULL, cptr, "handle_connect: request of type %d sending a url request", request_type);
      fprintf(stderr, "handle_connect: request of type %d sending a url request\n", request_type);
      break;
  }

  cptr->tcp_bytes_sent = bytes_sent;
  if (cptr->url_num->request_type == SMTP_REQUEST || cptr->url_num->request_type == SMTPS_REQUEST) {
    INC_SMTP_TX_BYTES_COUNTER(vptr, bytes_sent);
  } else if ((cptr->url_num->request_type == POP3_REQUEST) || (cptr->url_num->request_type == SPOP3_REQUEST)) {
    INC_POP3_SPOP3_TX_BYTES_COUNTER(vptr, bytes_sent);
  } else if (cptr->url_num->request_type == FTP_REQUEST  ||
             cptr->url_num->request_type == FTP_DATA_REQUEST) {

    INC_FTP_DATA_TX_BYTES_COUNTER(vptr, bytes_sent);
  } else if (cptr->url_num->request_type == DNS_REQUEST) {
    INC_DNS_TX_BYTES_COUNTER(vptr, bytes_sent);
  } else if (cptr->url_num->request_type == FTP_DATA_REQUEST) {
    /* nothing to do for now */
  } else if (cptr->url_num->request_type == LDAP_REQUEST || cptr->url_num->request_type == LDAPS_REQUEST) {
    INC_LDAP_TX_BYTES_COUNTER(vptr, bytes_sent); 
  } else if (cptr->url_num->request_type == IMAP_REQUEST || cptr->url_num->request_type == IMAPS_REQUEST) {
    INC_IMAP_TX_BYTES_COUNTER(vptr, bytes_sent); 
  }else if (cptr->url_num->request_type == JRMI_REQUEST ) {
    INC_JRMI_TX_BYTES_COUNTER(vptr, bytes_sent);
  }else if ((cptr->url_num->request_type == WS_REQUEST) || (cptr->url_num->request_type == WSS_REQUEST)) {
    INC_WS_TX_BYTES_COUNTER(vptr, bytes_sent);
  } else {                    /* HTTP/HTTPS */
    INC_OTHER_TYPE_TX_BYTES_COUNTER(vptr, bytes_sent);
  }
 
  on_request_write_done (cptr);
  NSDL4_DNS(NULL, cptr, "Returning  from handle_connect");
}
 
// This function is used when we send request after we got CONNECT resposse for https proxy connection
// In this case we need to reset these cptr attributes except connection related as connection is alredy made
inline void reset_cptr_attributes_after_proxy_connect(connection *cptr)
{
  cptr->request_complete_time = 0;
  cptr->req_ok = NS_REQUEST_CONFAIL; //was REQ_NEW
  cptr->req_code = 0;
  cptr->req_code_filled = -2;
  cptr->content_length = -1;
  cptr->bytes = 0;
  cptr->total_bytes = 0;
  cptr->tcp_bytes_recv = 0;
  cptr->checksum = 0;
  cptr->chunked_state = CHK_NO_CHUNKS;
  cptr->ctxt = NULL;
  cptr->completion_code = NS_COMPLETION_NOT_DONE;
  cptr->compression_type = NSLB_COMP_NONE;
  //Initializing the variables with -1.
  cptr->ssl_handshake_time = -1;
  cptr->write_complete_time = -1;
  cptr->first_byte_rcv_time = -1;
  //cptr->header_state = HDST_BOL;
  cptr->bytes_left_to_send = 0;
  cptr->nd_fp_instance = -1; // Must reset as cptr is reused and this is not filled if connection gets failed
  //Initializing the dns lookup time with -1.
  cptr->dns_lookup_time = -1;
}

inline void reset_cptr_attributes(connection *cptr)
{
  reset_cptr_attributes_after_proxy_connect(cptr);
  cptr->connect_time = -1;
}

/*I have used this one because of ns_get_random do some string operation we need 2 optimized that fn*/
int calculate_rand_number(double min, double max) {
  int out;
  NSDL4_SOCKETS(NULL, NULL, "Method called, min = %lf, max = %lf", min, max);
  out = min + (double)((max - (min - 1)) * (rand()/(RAND_MAX + max)));
  NSDL4_SOCKETS(NULL, NULL, "out = %d", out);
  return out;
}
/*
This method checks whether response was pushed for this url, by creating its hash_code and checking in hash_array, in case found, logs request/response and calls Close_connection() instead of making the request again
*/ 
inline void ns_check_pushed_response(connection* cptr, u_ns_ts_t now, int *ret)
{
  int promised_stream_id; 
  int idx;
  VUser* vptr = (VUser*)cptr->vptr; /*bug 86575: to check for caching*/  
  NSDL2_SOCKETS(NULL, cptr, "Method called cptr = %p", cptr);
  /*bug 86575: cache request time updated*/
  NS_SAVE_REQUEST_TIME_IN_CACHE() 
  if(cptr->url == NULL){
    NSDL2_SOCKETS(NULL, cptr, "cptr->url is null hence not going to check in pushed resources");
    return;
  }
  //Checking whether request was pushed, this will return 0 in case not found
  if(cptr->url_len != 1)   
    promised_stream_id = get_id_from_hash_table(cptr->http2->hash_arr,cptr->url,cptr->url_len); 
  else
    promised_stream_id = 0; 
 
  NSDL2_SOCKETS(NULL, cptr, "Promised_stream_id found = %d ", promised_stream_id);  
  
  if(promised_stream_id)
  {
    idx = (promised_stream_id / 2) - 1;
    /*bug 86575: idx updated according to the promise_count*/ 
    NS_H2_ALIGN_IDX(promised_stream_id)
    
   // log HTTP2 request here in its format , then log HTTP2 response and its data.
    #ifdef NS_DEBUG_ON  
      debug_log_http2_dump_pushed_hdr(cptr, promised_stream_id); 
    #else
    if(runprof_table_shr_mem[vptr->group_num].gset.trace_level)
      debug_log_http2_dump_pushed_hdr(cptr, promised_stream_id); 
    #endif

    if(cptr->http2->promise_buf[idx].free_flag){  
      char *new_line = "\n";
      char *header_start_char = ": ";
      char *push_resp = "PUSHED RESPONSE";
      
      #ifdef NS_DEBUG_ON
        debug_log_http2_res(cptr, (unsigned char *)new_line, 1);
        debug_log_http2_res(cptr, (unsigned char *)push_resp, 15);
        debug_log_http2_res(cptr, (unsigned char *)new_line, 1);
        debug_log_http2_res(cptr,(unsigned char *)cptr->url, cptr->url_len);
        debug_log_http2_res(cptr, (unsigned char *)new_line, 1);
      #else
        if(runprof_table_shr_mem[vptr->group_num].gset.trace_level)
        {
          log_http2_res(cptr, vptr, (unsigned char *)new_line, 1);
          log_http2_res(cptr, vptr, (unsigned char *)push_resp, 15);
          log_http2_res(cptr, vptr, (unsigned char *)new_line, 1);
          log_http2_res(cptr, vptr, (unsigned char *)cptr->url, cptr->url_len);
          log_http2_res(cptr, vptr, (unsigned char *)new_line, 1);
        }
      #endif

    int  num_header_entries = (int )cptr->http2->promise_buf[idx].header_count;
    NSDL2_SOCKETS(NULL, cptr, "num_header_entries = %d", num_header_entries);
    for(int i=0 ; i< num_header_entries ; i++)
    {
      #ifdef NS_DEBUG_ON
        debug_log_http2_res(cptr, (unsigned char *)cptr->http2->promise_buf[idx].response_headers[i].name, (int )cptr->http2->promise_buf[idx].response_headers[i].namelen);
        debug_log_http2_res(cptr, (unsigned char *)header_start_char, 2);
        debug_log_http2_res(cptr, (unsigned char *)cptr->http2->promise_buf[idx].response_headers[i].value, (int )cptr->http2->promise_buf[idx].response_headers[i].valuelen);
        debug_log_http2_res(cptr, (unsigned char *)new_line, 1);
      #else
        if(runprof_table_shr_mem[vptr->group_num].gset.trace_level)
        {
          log_http2_res(cptr, vptr, (unsigned char *)cptr->http2->promise_buf[idx].response_headers[i].name, (int )cptr->http2->promise_buf[idx].response_headers[i].namelen);
          log_http2_res(cptr, vptr, (unsigned char *)header_start_char, 2);
          log_http2_res(cptr, vptr, (unsigned char *)cptr->http2->promise_buf[idx].response_headers[i].value, (int )cptr->http2->promise_buf[idx].response_headers[i].valuelen);
          log_http2_res(cptr, vptr, (unsigned char *)new_line, 1);
          
        }
      #endif
      process_response_headers(cptr, cptr->http2->promise_buf[idx].response_headers[i], now);
    }
   
    //logging data;
    debug_log_http2_res(cptr, (unsigned char *)cptr->http2->promise_buf[idx].response_data, cptr->http2->promise_buf[idx].data_offset); 
    *ret = 1;
    INC_HTTP_FETCHES_STARTED_COUNTER(vptr);//incrementing req/sec count  
    if(cptr->http2->total_open_streams) /*bug 51330: changed _WRITING to _READING */
      cptr->conn_state = CNST_HTTP2_READING; // This state was getting set to CONN_REUSE , hence were not able to read in http2_handle_read   
    //Request response is complete here without going on to the network, going to call Close_connection
    cptr->flags |= NS_CPTR_HTTP2_PUSH; 
    Close_connection(cptr, 1, now, NS_REQUEST_OK, NS_COMPLETION_CONTENT_LENGTH);
   }
 }
}



// Sets timeout (e.g. idle) for response for different protocol
// #Shilpa 16Feb2011 Renamed function set_cptr_timeout() to set_cptr_for_new_req()
inline void set_cptr_for_new_req(connection* cptr, VUser *vptr,
                                u_ns_ts_t now) {


  ClientData client_data;
  client_data.p = cptr;

  int request_type = cptr->url_num->request_type;
  NSDL2_SOCKETS(NULL, NULL, "Method called, now = %u, request_type = %d",
						       now, request_type);

  //#Shilpa 16Feb2011 - Added average_time->fetches_started++ here as was wrongly done in on_url_done()
  //because was getting incremented even in case response is taken from cache
  switch(request_type) {
    case HTTP_REQUEST:
    case HTTPS_REQUEST:
      INC_HTTP_FETCHES_STARTED_COUNTER(vptr);
      // If user is ramping down, then set idle secs based on the ramp down method
      // Search all places for idle_secs
     if(!(((vptr->flags & NS_VUSER_RAMPING_DOWN) == NS_VUSER_RAMPING_DOWN) && 
         ((runprof_table_shr_mem[vptr->group_num].gset.rampdown_method.mode == RDM_MODE_ALLOW_CURRENT_SESSION_COMPLETE &&
         runprof_table_shr_mem[vptr->group_num].gset.rampdown_method.option == RDM_OPTION_HASTEN_COMPLETION_DISREGARDING_THINK_TIMES_USE_IDLE_TIME) ||
         (runprof_table_shr_mem[vptr->group_num].gset.rampdown_method.mode == RDM_MODE_ALLOW_CURRENT_PAGE_COMPLETE &&
         runprof_table_shr_mem[vptr->group_num].gset.rampdown_method.option == RDM_OPTION_HASTEN_COMPLETION_USING_IDLE_TIME))
       )) {
       NSDL2_SOCKETS(vptr, cptr, "grp id = %d, idle_secs = %d",
		     vptr->group_num, runprof_table_shr_mem[vptr->group_num].gset.idle_secs);

       PageReloadProfTableEntry_Shr *pagereload_ptr = NULL;
       PageClickAwayProfTableEntry_Shr *pageclickaway_ptr = NULL;

       if(cptr->url_num->proto.http.type == MAIN_URL) {
         if(runprof_table_shr_mem[vptr->group_num].page_reload_table) {
          pagereload_ptr = (PageReloadProfTableEntry_Shr*)runprof_table_shr_mem[vptr->group_num].page_reload_table[vptr->cur_page->page_number];
         }

         if(runprof_table_shr_mem[vptr->group_num].page_clickaway_table) {
           pageclickaway_ptr = (PageClickAwayProfTableEntry_Shr*)runprof_table_shr_mem[vptr->group_num].page_clickaway_table[vptr->cur_page->page_number];
         }
       }

       // Reload or Click Away timer;
       // Reload is prior to Click Away;
       // If Reload is done; then it will re-connect & add timer for Click Away if it exist
       if(pagereload_ptr && pagereload_ptr->reload_timeout > 0) {
         NSDL4_SOCKETS(vptr, NULL, "Adding reload timer for page %s.", vptr->cur_page->page_name);
         cptr->timer_ptr->actual_timeout = pagereload_ptr->reload_timeout; 
         dis_timer_add(AB_TIMEOUT_IDLE, cptr->timer_ptr, now, reload_connection, client_data, 0 );
       } else if(pageclickaway_ptr && pageclickaway_ptr->clickaway_timeout > 0) {
         NSDL4_SOCKETS(vptr, NULL, "Adding click away timer for page %s.", vptr->cur_page->page_name);
         cptr->timer_ptr->actual_timeout = pageclickaway_ptr->clickaway_timeout; 
         dis_timer_add(AB_TIMEOUT_IDLE, cptr->timer_ptr, now, click_away_connection, client_data, 0 );
       } else if(runprof_table_shr_mem[vptr->group_num].gset.idle_secs > 0) {
         NSDL4_SOCKETS(vptr, NULL, "Adding T.O. timer for page %s.", vptr->cur_page->page_name);
         // Add timer for T.O.
         cptr->timer_ptr->actual_timeout = runprof_table_shr_mem[vptr->group_num].gset.idle_secs;
         dis_timer_add_ex(AB_TIMEOUT_IDLE, cptr->timer_ptr, now, idle_connection, client_data, 0, global_settings->idle_timeout_all_flag);
       } else {
         NSDL2_SOCKETS(NULL, cptr, "No cptr timer added");
       }
     } else {
       // Add timer for T.O. in Ramp Down
       NSDL2_SOCKETS(vptr, cptr, "grp id = %d, User is marked as ramp down, idle_secs = %d, rampdown_method.time = %d",
                         vptr->group_num, runprof_table_shr_mem[vptr->group_num].gset.idle_secs,
                         runprof_table_shr_mem[vptr->group_num].gset.rampdown_method.time);
       cptr->timer_ptr->actual_timeout = runprof_table_shr_mem[vptr->group_num].gset.rampdown_method.time*1000; 
       //ab_timers[AB_TIMEOUT_IDLE].timeout_val = global_settings->rampdown_method.time*1000;
       NSDL2_SOCKETS(vptr, cptr, "New Idle time is (*1000) = %d", cptr->timer_ptr->actual_timeout);
       dis_timer_add_ex(AB_TIMEOUT_IDLE, cptr->timer_ptr, now, idle_connection, client_data, 0, global_settings->idle_timeout_all_flag);
     }
     break;  // Break for HTTP/HTTPS
  case WS_REQUEST:
  case WSS_REQUEST:
      INC_WS_WSS_FETCHES_STARTED_COUNTER(vptr);
      NSDL2_SOCKETS(NULL, NULL, "Inside ws request."); 
      cptr->timer_ptr->actual_timeout = runprof_table_shr_mem[vptr->group_num].gset.idle_secs; 
      dis_timer_add_ex(AB_TIMEOUT_IDLE, cptr->timer_ptr, now, idle_connection, client_data, 0, global_settings->idle_timeout_all_flag);
    break;
  case SOCKET_REQUEST:
  case SSL_SOCKET_REQUEST:
       NSDL2_SOCKETS(NULL, NULL, "Inside Socket request. Setting connect timeout, conn_timeout = %d",
                                  vptr->first_page_url->proto.socket.timeout_msec);
       cptr->timer_ptr->actual_timeout = (vptr->first_page_url->proto.socket.timeout_msec != -1)? 
                vptr->first_page_url->proto.socket.timeout_msec: 
                ((g_socket_vars.socket_settings.conn_to != -1)? g_socket_vars.socket_settings.conn_to: 
                runprof_table_shr_mem[vptr->group_num].gset.connect_timeout);
       dis_timer_add_ex(AB_TIMEOUT_IDLE, cptr->timer_ptr, now, idle_connection, client_data, 0, global_settings->idle_timeout_all_flag);
       break;
  case SMTP_REQUEST:
  case SMTPS_REQUEST:
    /* SMTP */

    /* Its a timer for Server (Connection) time out.
     * If connection is done we delete this timer & add timer for Greeting in handle connect. 
     */
    INC_SMTP_SMTPS_FETCHES_STARTED_COUNTER(vptr);
    cptr->mail_sent_count = cptr->url_num->proto.smtp.msg_count_min +
                           (int)(((float)cptr->url_num->proto.smtp.msg_count_max -
                            ((float)cptr->url_num->proto.smtp.msg_count_min - 1)) * 
                            (rand() / (RAND_MAX + (float)cptr->url_num->proto.smtp.msg_count_max)));
    cptr->timer_ptr->actual_timeout = runprof_table_shr_mem[vptr->group_num].gset.smtp_timeout;
    NSDL2_SOCKETS(vptr, cptr, "Smtp timout for connection = %d, mail_sent_count = %d",
                       cptr->timer_ptr->actual_timeout, cptr->mail_sent_count);
    dis_timer_add_ex(AB_TIMEOUT_IDLE, cptr->timer_ptr, now, smtp_timeout_handle, client_data, 0, global_settings->idle_timeout_all_flag);
    break; // Break For SMTP
  case POP3_REQUEST:
  case SPOP3_REQUEST:
    INC_POP3_SPOP3_FETCHES_STARTED_COUNTER(vptr);
    cptr->timer_ptr->actual_timeout = runprof_table_shr_mem[vptr->group_num].gset.pop3_timeout;
    NSDL2_SOCKETS(vptr, cptr, "pop3 timout for connection = %d",
                  cptr->timer_ptr->actual_timeout);
    dis_timer_add_ex(AB_TIMEOUT_IDLE, cptr->timer_ptr, now, pop3_timeout_handle, client_data, 0, global_settings->idle_timeout_all_flag);
    break; // POP3_REQUEST
  case FTP_REQUEST:
    INC_FTP_FETCHES_STARTED_COUNTER(vptr);
    cptr->timer_ptr->actual_timeout = runprof_table_shr_mem[vptr->group_num].gset.ftp_timeout;
    NSDL2_SOCKETS(vptr, cptr, "ftp timout for connection = %d",
                  cptr->timer_ptr->actual_timeout);
    dis_timer_add_ex(AB_TIMEOUT_IDLE, cptr->timer_ptr, now, ftp_timeout_handle, client_data, 0, global_settings->idle_timeout_all_flag);
    break;  // Break For FTP Request
  case FTP_DATA_REQUEST:
    break;
  case DNS_REQUEST:
    INC_DNS_FETCHES_STARTED_COUNTER(vptr);
    cptr->timer_ptr->actual_timeout = runprof_table_shr_mem[vptr->group_num].gset.dns_timeout;
    NSDL2_SOCKETS(vptr, cptr, "dns timout for connection = %d",
                               cptr->timer_ptr->actual_timeout);
    dis_timer_add_ex(AB_TIMEOUT_IDLE, cptr->timer_ptr, now, dns_timeout_handle, client_data, 0, global_settings->idle_timeout_all_flag);

    break; // Break For DNS Request
  case LDAP_REQUEST:
  case LDAPS_REQUEST:
    INC_LDAP_FETCHES_STARTED_COUNTER(vptr);
    cptr->timer_ptr->actual_timeout = runprof_table_shr_mem[vptr->group_num].gset.ldap_timeout;
    NSDL2_SOCKETS(vptr, cptr, "ldap timout for connection = %d",
                  cptr->timer_ptr->actual_timeout);
    dis_timer_add_ex(AB_TIMEOUT_IDLE, cptr->timer_ptr, now, ldap_timeout_handle, client_data, 0, global_settings->idle_timeout_all_flag);
    break;  // Break For ldap Request
  case IMAP_REQUEST:
  case IMAPS_REQUEST:
    INC_IMAP_IMAPS_FETCHES_STARTED_COUNTER(vptr);
    cptr->timer_ptr->actual_timeout = runprof_table_shr_mem[vptr->group_num].gset.imap_timeout;
    NSDL2_SOCKETS(vptr, cptr, "imap(s) timout for connection = %d",
                  cptr->timer_ptr->actual_timeout);
    dis_timer_add_ex(AB_TIMEOUT_IDLE, cptr->timer_ptr, now, imap_timeout_handle, client_data, 0, global_settings->idle_timeout_all_flag);
    break;  // Break For imap Request
  case XMPP_REQUEST:
  case XMPPS_REQUEST:
    XMPP_DATA_INTO_AVG(xmpp_login_attempted);
    //Increase Login Attempted Counters Here 
    //INC_XMPP_XMPPS_FETCHES_STARTED_COUNTER(vptr);
    //cptr->timer_ptr->actual_timeout = runprof_table_shr_mem[vptr->group_num].gset.imap_timeout;
    //NSDL2_SOCKETS(vptr, cptr, "imap(s) timout for connection = %d",
     //             cptr->timer_ptr->actual_timeout);
    //dis_timer_add_ex(AB_TIMEOUT_IDLE, cptr->timer_ptr, now, imap_timeout_handle, client_data, 0, global_settings->idle_timeout_all_flag);
    break;  // Break For imap Request
  case FC2_REQUEST:
    INC_FC2_FETCHES_STARTED_COUNTER(vptr);
    break;

  case JRMI_REQUEST:
    INC_JRMI_FETCHES_STARTED_COUNTER(vptr);
   /* cptr->timer_ptr->actual_timeout = runprof_table_shr_mem[vptr->group_num].gset.jrmi_timeout;
    NSDL2_SOCKETS(vptr, cptr, "jrmi timout for connection = %d",
                  cptr->timer_ptr->actual_timeout);
    dis_timer_add_ex(AB_TIMEOUT_IDLE, cptr->timer_ptr, now, jrmi_timeout_handle, client_data, 0, global_settings->idle_timeout_all_flag);*/
    break;
  default:
    NSDL2_SOCKETS(vptr, cptr, "Invalid Request type (%d)", request_type);
  }
}

static void cache_serve_callback( ClientData client_data, u_ns_ts_t now ) {
  connection* cptr;
  cptr = client_data.p;

  NSDL2_SOCKETS(NULL, cptr, "Method called, now = %d", now);
  Close_connection(cptr, 0, now, NS_REQUEST_OK, NS_COMPLETION_FRESH_RESPONSE_FROM_CACHE);
}

static inline void set_cache_serve_callback(connection *cptr, u_ns_ts_t now) {

  VUser *vptr = cptr->vptr;
  NSDL2_SOCKETS(NULL, cptr, "Method called, now = %d", now);

  if(runprof_table_shr_mem[vptr->group_num].gset.cache_delay_resp == 0) {
    Close_connection(cptr, 0, now, NS_REQUEST_OK, NS_COMPLETION_FRESH_RESPONSE_FROM_CACHE);
  } else {
    ClientData client_data;
    client_data.p = cptr;
    cptr->timer_ptr->actual_timeout = runprof_table_shr_mem[vptr->group_num].gset.cache_delay_resp;
    NSDL2_SOCKETS(NULL, cptr, "Adding '%d ms' delay to serve cache response.",
                               cptr->timer_ptr->actual_timeout);
    dis_timer_add_ex(AB_TIMEOUT_IDLE, cptr->timer_ptr, now, cache_serve_callback, client_data, 0, 1);
  }
}



int 
start_socket( connection* cptr, u_ns_ts_t now ) {
  VUser *vptr = cptr->vptr;
  PerHostSvrTableEntry_Shr* svr_entry;
  int timer_type;
  int select_port;
  char err_msg[1024];
  u_ns_ts_t local_end_time_stamp;

  timer_type = cptr->timer_ptr->timer_type;

  NSDL2_SOCKETS(NULL, cptr, "Method called, total_ip_entries = %d, cptr = %p", total_ip_entries, cptr);

  /*If timer is Keep-Alive time out then need to delete the timer.
   * as we are sending new request*/
  if ((timer_type >= 0) && (timer_type == AB_TIMEOUT_KA)) {
    dis_timer_del(cptr->timer_ptr);
  }

  NSDL1_SOCKETS(vptr, cptr, "start_socket:cptr=%p, request_type = %hd, cur_page = %s, sess_id = %d",
                cptr, cptr->url_num->request_type, vptr->cur_page->page_name, vptr->sess_ptr->sess_id);

  /* we reset them only in case of new pipe. */
  if (!runprof_table_shr_mem[vptr->group_num].gset.enable_pipelining || cptr->num_pipe < 1) {
    reset_cptr_attributes(cptr);
  }

  /* Start filling in the connection slot. */
  if (runprof_table_shr_mem[vptr->group_num].gset.enable_pipelining) setup_cptr_for_pipelining(cptr);

#ifdef RMI_MODE
  if (cptr->url_num->proto.http.bytevars.bytevar_start && cptr->url_num->request_type == RMI_REQUEST) {
    vptr->cur_bytevar = cptr->url_num->proto.http.bytevars.bytevar_start;
    vptr->end_bytevar = cptr->url_num->proto.http.bytevars.bytevar_start + cptr->url_num->bytevars.num_bytevars;
  } else {
    vptr->cur_bytevar = NULL;
  }
#endif

  NSDL1_SOCKETS(vptr, cptr, "conn_state = %d, num_pipe = %d", cptr->conn_state, cptr->num_pipe);
  if (cptr->conn_state != CNST_REUSE_CON &&
      (!runprof_table_shr_mem[vptr->group_num].gset.enable_pipelining || cptr->num_pipe < 2)) {
    if (cptr->num_retries == 0){ //This is the first try
      cptr->started_at = cptr->con_init_time = now;
    }

    /* KA has no relation with SMTP or POP3 or FTP or IMAP*/
    if (cptr->connection_type == KA && 
        ((cptr->url_num->request_type != SMTP_REQUEST) &&
         (cptr->url_num->request_type != SMTPS_REQUEST) &&
         (cptr->url_num->request_type != POP3_REQUEST) &&
         (cptr->url_num->request_type != SPOP3_REQUEST) &&
         (cptr->url_num->request_type != FTP_REQUEST) &&
         (cptr->url_num->request_type != FTP_DATA_REQUEST) &&
         (cptr->url_num->request_type != LDAP_REQUEST) &&
         (cptr->url_num->request_type != LDAPS_REQUEST) &&
         (cptr->url_num->request_type != IMAP_REQUEST) &&
         (cptr->url_num->request_type != IMAPS_REQUEST) &&
         (cptr->url_num->request_type != JRMI_REQUEST) &&
         (cptr->url_num->request_type != DNS_REQUEST))) {

      cptr->num_ka = 
        runprof_table_shr_mem[vptr->group_num].gset.num_ka_min + 
        (runprof_table_shr_mem[vptr->group_num].gset.num_ka_range > 0 ? 
         (rand() % runprof_table_shr_mem[vptr->group_num].gset.num_ka_range):0);

      if (cptr->num_ka <= 0) {
        cptr->num_ka = runprof_table_shr_mem[vptr->group_num].gset.num_ka_min + 
          (runprof_table_shr_mem[vptr->group_num].gset.num_ka_range)/2;
      }
      NSDL1_SOCKETS(NULL, NULL, "connection_type = %d", cptr->connection_type);
    }
    NSDL2_SCHEDULE(NULL, cptr, "grp id = %d, cptr->connection_type = %d, cptr->num_ka = %d "
                   "grp ka_pct = %d", 
                   vptr->group_num, cptr->connection_type, cptr->num_ka,
                   runprof_table_shr_mem[vptr->group_num].gset.ka_pct);

#ifdef RMI_MODE
    if (cptr->url_num->index.svr_ptr) {
      if ((svr_entry = get_svr_entry(vptr, cptr->url_num->index.svr_ptr)) == NULL) {
        fprintf(stderr, "Start Socket: Unknown host\n");
        end_test_run();
      }
      cptr->old_svr_entry = svr_entry;
    } else {
      svr_entry = cptr->old_svr_entry;
    }
#else
    if ((svr_entry = get_svr_entry(vptr, cptr->url_num->index.svr_ptr)) == NULL) {
      fprintf(stderr, "Start Socket: Unknown host\n");
      end_test_run();
    } else {
      cptr->old_svr_entry = svr_entry;
    }
#endif

    // AN-TODO Need optimization as code is duplicate in else block of reuse conn
    // http_make_url_and_check_cache() must be called after we have the svr_entry
    // and before any idle timer is started
    // CHECK  -Keep Alive timeout is started after repsone is recieved and next req is not to be send immeditaley
    if((cptr->url_num->request_type == HTTP_REQUEST ||
       cptr->url_num->request_type == HTTPS_REQUEST)) {
      int ret = 0;
      http_make_url_and_check_cache(cptr, now, &ret);
      if(ret == USEONCE_VALUE_OVER){
      /*  if(cptr->url_num->proto.http.type == EMBEDDED_URL)
          vptr->urls_awaited--;*/
        return ret;
      }
      if(NS_IF_TRACING_ENABLE_FOR_USER && cptr->url_num->proto.http.type != EMBEDDED_URL) {
        NSDL2_USER_TRACE(vptr, cptr, "Method called, User tracing enabled");
        ut_update_url_values(cptr, ret); // It will save the first URL of page as there is check in the function
      }

      if(ret == CACHE_RESP_IS_FRESH) {
        NSDL2_CACHE(NULL, cptr, "Serving Response for Request [%s] From Cache with fresh connection. "
	  			" conn_fd = %d, conn_state = %d",
			        cptr->url, cptr->conn_fd, cptr->conn_state); 
         /*To avoid stack overflow we are putting Close_connections into timer with a 10 ms.*/
        set_cache_serve_callback(cptr, now);
        return ret;
      }
         /* In case of click script, the recorded host could be different from the host to be hit.*/
      /* hence refresh the svr_entry from totsvr_table_ptr */
      if (cptr->url_num->proto.http.header_flags & NS_URL_CLICK_TYPE){
        if ((svr_entry = get_svr_entry(vptr, cptr->url_num->index.svr_ptr)) == NULL) {
          fprintf(stderr, "Start Socket: Unknown host\n");
          end_test_run();
        } else {
          cptr->old_svr_entry = svr_entry;
        }
      }
    
    }
    /* Handling of server push, in case of resources were pushed , we will stop the request from going on to the network, will serve it from
    stored response.
    Will be checked only when Server Push enabled. In case of get_no_inline server push should be disabled
    */
    if(cptr->http_protocol == HTTP_MODE_HTTP2 && runprof_table_shr_mem[vptr->group_num].gset.http2_settings.enable_push && cptr->http2){
      int ret_val = 0;
      ns_check_pushed_response(cptr, now, &ret_val);
      if(ret_val)
      return ret_val;
    }
   
  
    NSDL1_SOCKETS(NULL, NULL, "global_settings->high_perf_mode = %d", global_settings->high_perf_mode); 
    /* Set timouts for all protocals*/
    set_cptr_for_new_req(cptr, vptr, now);

    if (runprof_table_shr_mem[vptr->group_num].gset.use_same_netid_src) {
      if (svr_entry->net_idx != vptr->user_ip->net_idx)
        vptr->user_ip = get_src_ip(vptr, svr_entry->net_idx);
    }

    if(global_settings->high_perf_mode) {
      cptr->conn_fd = get_sock_from_list(vptr->user_ip->net_idx, cptr, runprof_table_shr_mem[vptr->group_num].gset);
#if 0
      //printf ("Using fd = %d\n", cptr->conn_fd);
      // Remove previos association of this socket by connecting to UnSpecified family
      struct sockaddr_in c_addr;
      bzero((char *) &c_addr, sizeof(c_addr));
      c_addr.sin_family = AF_UNSPEC;
      if (connect(cptr->conn_fd, (struct sockaddr*) &c_addr, sizeof(c_addr)) < 0)
      {
        perror("ERROR unbind");
        exit(1);
      }
#endif
    } else {
      NSDL1_SOCKETS(NULL, NULL, "coming in else block..");
      cptr->conn_fd = get_socket(vptr->user_ip->ip_addr.sin6_family, vptr->group_num);
      BIND_SOCKET((char*)&(vptr->user_ip->ip_addr), 
                  v_port_table[my_port_index].min_port,
                  v_port_table[my_port_index].max_port);
    }

    if((cptr->url_num->request_type == HTTPS_REQUEST) || (cptr->url_num->request_type == WSS_REQUEST)) {
      int flag = 1;
      if (setsockopt( cptr->conn_fd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag) ) < 0) {
        fprintf(stderr, "Error: Setting fd to TCP no-delay failed\n");
        perror( get_url_req_url(cptr) );
        end_test_run();
      }
    }

    if((cptr->url_num->request_type == WS_REQUEST) || (cptr->url_num->request_type == WSS_REQUEST))
    {
      vptr->ws_cptr[ws_idx_list[vptr->first_page_url->proto.ws.conn_id]] = cptr;
      NSDL2_WS(vptr, cptr, "CNST_FREE: vptr->first_page_url->proto.ws.conn_id = %d, ws_con id = %d, "
                          "vptr->ws_cptr[vptr->first_page_url->proto.ws.conn_id] = %p", 
                           vptr->first_page_url->proto.ws.conn_id, ws_idx_list[vptr->first_page_url->proto.ws.conn_id], 
                           vptr->ws_cptr[ws_idx_list[vptr->first_page_url->proto.ws.conn_id]]);
    }

    if((cptr->url_num->request_type == SOCKET_REQUEST) || (cptr->url_num->request_type == SSL_SOCKET_REQUEST))
    {
      //Registering function for handling receive data
      cptr->proc_recv_data = process_socket_recv_data;

      vptr->conn_array[vptr->first_page_url->proto.socket.norm_id] = cptr;
      NSDL2_WS(vptr, cptr, "CNST_FREE: vptr->first_page_url->proto.socket.norm_id = %d,"
                           "vptr->conn_array[vptr->first_page_url->proto.socket.norm_id] = %p",
                            vptr->first_page_url->proto.socket.norm_id,
                            vptr->conn_array[vptr->first_page_url->proto.socket.norm_id]);
    }
    
    /* Connect to the host. */
#ifdef USE_EPOLL
    if(global_settings->high_perf_mode == 0) {
      num_set_select++; /*TST */
      int event = EPOLLIN|EPOLLOUT|EPOLLERR|EPOLLHUP|EPOLLET;
     
      // Socket API is behave like chat application so read and write must be only when read or write api called by user
      if(cptr->request_type == SOCKET_REQUEST)
        event = EPOLLOUT|EPOLLERR|EPOLLHUP|EPOLLET;
      else if(cptr->request_type == SSL_SOCKET_REQUEST)
        event = EPOLLIN|EPOLLOUT|EPOLLERR|EPOLLHUP|EPOLLET;
        
      NSDL1_SOCKETS(vptr, cptr, "Adding cptr->conn_fd '%d' in epoll num_set_select = %d", cptr->conn_fd, num_set_select);
      //if (add_select(cptr, cptr->conn_fd, EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLET) < 0) 
      if (add_select(cptr, cptr->conn_fd, event) < 0) {
        fprintf(stderr, "Error: Set Select failed on WRITE EVENT\n");
        end_test_run();
      }
    }
#endif

#ifdef NS_USE_MODEM
    if (global_settings->wan_env) // WAN is Enabled then set 
      set_socket_for_wan(cptr, svr_entry);
#endif
  if(runprof_table_shr_mem[vptr->group_num].gset.use_dns != USE_DNS_NONBLOCK){
    if (runprof_table_shr_mem[vptr->group_num].gset.use_dns == USE_DNS_DYNAMIC) 
    {
       NSDL2_SOCKETS(vptr, cptr, "use_dns is active. Making DNS lookup");

      /* for FTP_DATA_REQUEST; we already have right cptr->cur_server */
      if (cptr->url_num->request_type != FTP_DATA_REQUEST) {
        int ret = 0;
        //Current server address, index,    
        if((runprof_table_shr_mem[vptr->group_num].gset.dns_caching_mode == USE_DNS_CACHE_FOR_SESSION) || (runprof_table_shr_mem[vptr->group_num].gset.dns_caching_mode == USE_DNS_CACHE_FOR_TTL)) {
          NSDL2_SOCKETS(vptr, cptr, "Use DNS caching mode enabled. Checking if server is already resolved or not");
          ret = fill_sockaddr_if_resolve(cptr, vptr, svr_entry);
        }
        if(!ret)
        {
          //Bug 55524 - Host is going to resolve from dns server first rather than the ip provided in G_STATIC_HOST
          char *ptr = NULL;
          char svr_name[1024 + 1];
          if((ptr = strchr(svr_entry->server_name, ':')) != NULL){
            strncpy(svr_name, svr_entry->server_name, ptr-svr_entry->server_name);
            svr_name[ptr-svr_entry->server_name] = '\0';
          }
          else
            strcpy(svr_name, svr_entry->server_name);

          cptr->ns_component_start_time_stamp = get_ms_stamp();//Set NS component start time  
          NSDL2_SOCKETS(vptr, cptr, "Time before resolving host, start_time = %u, server_name = %s", cptr->ns_component_start_time_stamp, svr_entry->server_name);
          INCREMENT_DNS_LOOKUP_CUM_SAMPLE_COUNTER(vptr); 
          //if (!nslb_fill_sockaddr(&(cptr->cur_server), svr_entry->server_name, ntohs(svr_entry->saddr.sin6_port))) 
          if (ns_get_host(vptr->group_num, &(cptr->cur_server), svr_name, ntohs(svr_entry->saddr.sin6_port), err_msg) == -1) {
            INCREMENT_DNS_LOOKUP_FAILURE_COUNTER(vptr);
            local_end_time_stamp = get_ms_stamp();//set end time
            cptr->dns_lookup_time = local_end_time_stamp - cptr->ns_component_start_time_stamp; //time taken while resolving host
            cptr->ns_component_start_time_stamp = local_end_time_stamp;//Update component time. 

            SET_MIN (average_time->url_dns_min_time, cptr->dns_lookup_time);
            SET_MAX (average_time->url_dns_max_time, cptr->dns_lookup_time);
            average_time->url_dns_tot_time += cptr->dns_lookup_time;
            average_time->url_dns_count++;

            UPDATE_GROUP_BASED_NETWORK_TIME(dns_lookup_time, url_dns_tot_time, url_dns_min_time, url_dns_max_time, url_dns_count);
 
            UPDATE_DNS_LOOKUP_TIME_COUNTERS(vptr, cptr->dns_lookup_time);

            if (IS_TCP_CLIENT_API_EXIST)
              fill_tcp_client_avg(vptr, DNS_TIME, cptr->dns_lookup_time);

            NSDL2_SOCKETS(vptr, cptr, "Error in resolving host %s:%s , threshold time given by user %d ms whereas total time taken %d ms. "
                                      "HostErr '%s'", svr_entry->server_name, proto_to_str(cptr->url_num->request_type), 
                            global_settings->dns_threshold_time_reporting, cptr->dns_lookup_time, err_msg);
            NS_EL_3_ATTR(EID_DYNAMIC_HOST,  -1, -1, EVENT_CORE, EVENT_WARNING,
                         vptr->sess_ptr->sess_name, vptr->cur_page->page_name, svr_entry->server_name,
                           "Error in resolving host %s:%s, total time taken %d ms. HostErr '%s'",
                            svr_entry->server_name, proto_to_str(cptr->url_num->request_type), cptr->dns_lookup_time, err_msg);
            //Host not resolve, log details in case dns debugging enable
            if (runprof_table_shr_mem[vptr->group_num].gset.dns_debug_mode == 1)
              dns_resolve_log_write(g_partition_idx, "NR", svr_entry->server_name, cptr->dns_lookup_time, &cptr->cur_server);

            //Need to reset 'svr_entry->server_flags', as not able to resolve the DNS
            svr_entry->server_flags &= ~NS_SVR_FLAG_SVR_ALREADY_RESOLVED;
            goto below;
/*            print_core_events((char*)__FUNCTION__, __FILE__,
                              "Error: Host <%s> specified by Host header"
                              " is not a valid hostname. Exiting\n",
                              svr_entry->server_name);
            end_test_run();
*/
          }

          // DNS lookup was succesful. Copy resolved address into ustable if caching enabled
          if ((runprof_table_shr_mem[vptr->group_num].gset.dns_caching_mode == USE_DNS_CACHE_FOR_SESSION) || (runprof_table_shr_mem[vptr->group_num].gset.dns_caching_mode == USE_DNS_CACHE_FOR_TTL)) {
            NSDL2_SOCKETS(vptr, cptr, "DNS resolve host cache enable hence saving resolved ip address for server name = %s at host index = %d", svr_entry->server_name, cptr->url_num->index.svr_ptr->idx);
            if((runprof_table_shr_mem[vptr->group_num].gset.dns_caching_mode == USE_DNS_CACHE_FOR_SESSION))
              memcpy(&(vptr->usr_entry[cptr->url_num->index.svr_ptr->idx].resolve_saddr), &(cptr->cur_server), 
                            sizeof(struct sockaddr_in6));
            else
              memcpy(&(svr_entry->saddr), &(cptr->cur_server), sizeof(struct sockaddr_in6));
          }
          local_end_time_stamp = get_ms_stamp();//time taken to resolve host
          svr_entry->last_resolved_time = local_end_time_stamp; //updating resolve time
          cptr->dns_lookup_time = local_end_time_stamp - cptr->ns_component_start_time_stamp;//DNS lookup time diff
          cptr->ns_component_start_time_stamp = local_end_time_stamp; //Update ns_component_start_time_stamp

          SET_MIN (average_time->url_dns_min_time, cptr->dns_lookup_time);
          SET_MAX (average_time->url_dns_max_time, cptr->dns_lookup_time);
          average_time->url_dns_tot_time += cptr->dns_lookup_time;
          average_time->url_dns_count++;

          UPDATE_GROUP_BASED_NETWORK_TIME(dns_lookup_time, url_dns_tot_time, url_dns_min_time, url_dns_max_time, url_dns_count);

          UPDATE_DNS_LOOKUP_TIME_COUNTERS(vptr, cptr->dns_lookup_time);
          if (IS_TCP_CLIENT_API_EXIST)
            fill_tcp_client_avg(vptr, DNS_TIME, cptr->dns_lookup_time);

          NSDL2_SOCKETS(vptr, cptr, "Time taken after resolving host, end_time = %u, diff_timestamp = %d, threshold_time = %d", 
                          local_end_time_stamp, cptr->dns_lookup_time, global_settings->dns_threshold_time_reporting);
          //Report event if time taken to resolve host is greater than threshold time set for DNS lookup
          if (cptr->dns_lookup_time > global_settings->dns_threshold_time_reporting)
          {
            NSDL2_SOCKETS(vptr, cptr, "Total time taken to resolve host %s:%s was %d ms whereas threshold time given by user was %d ms", 
                            svr_entry->server_name, proto_to_str(cptr->url_num->request_type), cptr->dns_lookup_time, global_settings->dns_threshold_time_reporting);
            NS_EL_3_ATTR(EID_DYNAMIC_HOST,  -1, -1, EVENT_CORE, EVENT_WARNING,
                            vptr->sess_ptr->sess_name, vptr->cur_page->page_name, svr_entry->server_name,
                             "Total time taken to resolve host %s:%s is %d ms.",
                                svr_entry->server_name, proto_to_str(cptr->url_num->request_type), cptr->dns_lookup_time);
  
          }
          //Log resolved host in case dns debugging enable
         if (runprof_table_shr_mem[vptr->group_num].gset.dns_debug_mode == 1)
           dns_resolve_log_write(g_partition_idx, "R", svr_entry->server_name, cptr->dns_lookup_time, &cptr->cur_server);
        }
      }
   }
   else{ //_NO || SIMULATE
      below: ;
#ifdef RMI_MODE
      if (cptr->url_num->index.svr_ptr)
#endif
       /* we already have right cur_server so do only for non FTP_DATA_REQUEST */
      if (cptr->url_num->request_type != FTP_DATA_REQUEST) {
        /*Try to resolve server if already not resolved*/
        //if(!svr_entry->server_already_resolved) 
        if(!(svr_entry->server_flags & NS_SVR_FLAG_SVR_ALREADY_RESOLVED)) {
          if (!nslb_fill_sockaddr(&(cptr->cur_server), svr_entry->server_name,
                                ntohs(svr_entry->saddr.sin6_port))) {
              print_core_events((char*)__FUNCTION__, __FILE__,
                                "Error: Host <%s> specified by Host header"
                                " is not a valid hostname. Exiting\n",
                                svr_entry->server_name);

              cptr->req_ok = NS_DNS_LOOKUP_FAIL;
              Close_connection(cptr, 0, now, NS_DNS_LOOKUP_FAIL, NS_COMPLETION_BIND_ERROR); 
              return -1;
              //end_test_run();
          }
        }
        memcpy (&(cptr->cur_server), &(svr_entry->saddr), sizeof(struct sockaddr_in6));
      }
    }
  }
    /*Manish: set Proxy connection */
       //First check for proxy. If proxy is given for this group then only
       //go to check for proxy
    if((runprof_table_shr_mem[vptr->group_num].proxy_ptr != NULL) &&
      ((((cptr->url_num->request_type == HTTP_REQUEST) || (cptr->url_num->request_type == WS_REQUEST) ||
         (cptr->url_num->request_type == SOCKET_REQUEST)) &&
            runprof_table_shr_mem[vptr->group_num].proxy_ptr->http_proxy_server != NULL) ||
      (((cptr->url_num->request_type == HTTPS_REQUEST) || (cptr->url_num->request_type == WSS_REQUEST) ||
        (cptr->url_num->request_type == SSL_SOCKET_REQUEST)) &&
           runprof_table_shr_mem[vptr->group_num].proxy_ptr->https_proxy_server != NULL)))
      {
         proxy_check_and_set_conn_server(vptr, cptr);
      }
      else
      {
       //Proxy is not given for this group
       NSDL2_SOCKETS(vptr, cptr, "Proxy is not using so setting host (i.e origin server)"
                          "'%s' for connection", 
                           nslb_sockaddr_to_ip((struct sockaddr *)&cptr->cur_server, 1));
       memcpy(&(cptr->conn_server), &(cptr->cur_server), sizeof(struct sockaddr_in6));
       //cptr->flags &= ~NS_CPTR_FLAGS_CON_USING_PROXY; //TODO: Do we need this??
      }
/*
     if(runprof_table_shr_mem[vptr->group_num].proxy_ptr == NULL) 
     (runprof_table_shr_mem[vptr->group_num].proxy_ptr->http_proxy_server == NULL && cptr->url_num->request_type == HTTP_REQUEST) 
     (runprof_table_shr_mem[vptr->group_num].proxy_ptr->https_proxy_server == NULL && cptr->url_num->request_type == HTTPS_REQUEST) 
     {
       //Proxy is not given for this group
       NSDL2_SOCKETS(vptr, cptr, "Proxy is not using so setting host (i.e origin server)"
                          "'%s' for connection", 
                           nslb_sockaddr_to_ip((struct sockaddr *)&cptr->cur_server, 1));
       memcpy(&(cptr->conn_server), &(cptr->cur_server), sizeof(struct sockaddr_in6));
       //cptr->flags &= ~NS_CPTR_FLAGS_CON_USING_PROXY; //TODO: Do we need this??
     }
     else{
       proxy_check_and_set_conn_server(vptr, cptr);
     }
*/
    now = cptr->ns_component_start_time_stamp = get_ms_stamp();//Update NS component start time stamp for connect

    switch(cptr->url_num->request_type) { 
      case SOCKET_REQUEST:
      case SSL_SOCKET_REQUEST:
      {
        if (IS_TCP_CLIENT_API_EXIST)
          fill_tcp_client_avg(vptr, CON_INIT, 0);

        if (IS_UDP_CLIENT_API_EXIST)
          fill_udp_client_avg(vptr, CON_INIT, 0);
        //break;               //Fall througth into HTTP as it has Overall counters
      }
      case HTTP_REQUEST:
      case HTTPS_REQUEST:
         average_time->num_con_initiated++; // Increament Number of TCP Connection initiated
         break;
      case SMTP_REQUEST:
      case SMTPS_REQUEST:
         average_time->smtp_num_con_initiated++; // Increament Number of TCP Connection initiated
         break;
      case POP3_REQUEST:
      case SPOP3_REQUEST:
         average_time->pop3_num_con_initiated++; // Increament Number of TCP Connection initiated
         break;
      case FTP_REQUEST:
      case FTP_DATA_REQUEST:
         ftp_avgtime->ftp_num_con_initiated++; // Increament Number of TCP Connection initiated
         break;
      case DNS_REQUEST:
         average_time->dns_num_con_initiated++; // Increament Number of TCP Connection initiated
         break;
      case LDAP_REQUEST:
      case LDAPS_REQUEST:
         ldap_avgtime->ldap_num_con_initiated++; // Increament Number of TCP Connection initiated
         break;
      case IMAP_REQUEST:
      case IMAPS_REQUEST:
         imap_avgtime->imap_num_con_initiated++; // Increament Number of TCP Connection initiated
         break;
      case JRMI_REQUEST:
         jrmi_avgtime->jrmi_num_con_initiated++; // Increament Number of TCP Connection initiated
         break;
      case WS_REQUEST:
      case WSS_REQUEST:
         INC_WS_WSS_INITIATED_CONN_COUNTERS(vptr); // Increment Number of TCP Connection initiated
         break;
    }

    char srcip[128];
    strcpy(srcip, nslb_get_src_addr(cptr->conn_fd)); 
    
    if(cptr->flags & NS_CPTR_FLAGS_CON_USING_PROXY)
    { 
      char origin_server_buf[1024 + 1]; 
      char proxy_server_buf[1024 + 1]; 

      strcpy(proxy_server_buf, nslb_sock_ntop((struct sockaddr *)&cptr->conn_server));
      strcpy(origin_server_buf, nslb_sock_ntop((struct sockaddr *)&cptr->cur_server));

      NSDL3_SOCKETS(vptr, cptr, "http_proxy_server = %s, https_proxy_server = %s", 
                                 runprof_table_shr_mem[vptr->group_num].proxy_ptr->http_proxy_server,
                                 runprof_table_shr_mem[vptr->group_num].proxy_ptr->https_proxy_server);

      if((cptr->url_num->request_type == HTTP_REQUEST) || (cptr->url_num->request_type == WS_REQUEST) || (cptr->url_num->request_type == SOCKET_REQUEST))
        if(runprof_table_shr_mem[vptr->group_num].proxy_ptr->http_proxy_server != NULL)
        {
          // Connecting to proxy server 'myprox.com' (ip:port) for orgin server name (ip)  with fd 1 "Recor
          NS_DT4(vptr, cptr, DM_L1, MM_SOCKETS, "Connecting for HTTP request and for group %s "
                                    "to proxy server '%s:%hd' (%s) "
                                    "for origin server '%s' (%s) with fd %d (Recorded Server: %s), Client IP: %s",
                        runprof_table_shr_mem[vptr->group_num].scen_group_name,
                        runprof_table_shr_mem[vptr->group_num].proxy_ptr->http_proxy_server,
                        runprof_table_shr_mem[vptr->group_num].proxy_ptr->http_port,
                        proxy_server_buf,
                        svr_entry->server_name,
                        origin_server_buf,
                        cptr->conn_fd,
                        cptr->url_num->index.svr_ptr->server_hostname, srcip);
          #if 0
          /*Manish: For Testing only*/
          printf("HTTP: Group = %s, Connecting to proxy server '%s:%hi' (%s) for origin server '%s' (%s)"
              "with fd %d (Recorded Server: %s), Client IP: %s\n",
                  runprof_table_shr_mem[vptr->group_num].scen_group_name,
                  runprof_table_shr_mem[vptr->group_num].proxy_ptr->http_proxy_server,
                  runprof_table_shr_mem[vptr->group_num].proxy_ptr->http_port,
                  nslb_sock_ntop((struct sockaddr *)&cptr->conn_server),
                  svr_entry->server_name,
                  nslb_sock_ntop((struct sockaddr *)&cptr->cur_server),
                  cptr->conn_fd,
                  cptr->url_num->index.svr_ptr->server_hostname, srcip);
          #endif
       }

      if((cptr->url_num->request_type == HTTPS_REQUEST) || (cptr->url_num->request_type == WSS_REQUEST) ||
         (cptr->url_num->request_type == SSL_SOCKET_REQUEST))
        if(runprof_table_shr_mem[vptr->group_num].proxy_ptr->https_proxy_server != NULL)
        {
          NS_DT4(vptr, cptr, DM_L1, MM_SOCKETS, "Connecting for HTTPS request and for group %s "
                               "to proxy server '%s:%hd' (%s) "
                               "for origin server '%s' (%s) with fd %d (Recorded Server: %s), Client IP: %s",
                        runprof_table_shr_mem[vptr->group_num].scen_group_name,
                        runprof_table_shr_mem[vptr->group_num].proxy_ptr->https_proxy_server,
                        runprof_table_shr_mem[vptr->group_num].proxy_ptr->https_port,
                        proxy_server_buf,
                        svr_entry->server_name,
                        origin_server_buf,
                        cptr->conn_fd,
                        cptr->url_num->index.svr_ptr->server_hostname, srcip);
     
          #if 0 
          /*Manish: For Testing only*/
          printf("HTTPS: Group = %s, Connecting to proxy server '%s:%hi' (%s) for origin server '%s' (%s)"
              "with fd %d (Recorded Server: %s), Client IP: %s\n",
                  runprof_table_shr_mem[vptr->group_num].scen_group_name,
                  runprof_table_shr_mem[vptr->group_num].proxy_ptr->https_proxy_server,
                  runprof_table_shr_mem[vptr->group_num].proxy_ptr->https_port,
                  nslb_sock_ntop((struct sockaddr *)&cptr->conn_server),
                  svr_entry->server_name,
                  nslb_sock_ntop((struct sockaddr *)&cptr->cur_server),
                  cptr->conn_fd,
                  cptr->url_num->index.svr_ptr->server_hostname, srcip);
          #endif
       }
    }
    else
    {
      NS_DT4(vptr, cptr, DM_L1, MM_SOCKETS, "Connecting for group %s to server '%s' (%s) with fd %d"
                                          " (Recorded Server: %s), Client IP: %s",
                                          runprof_table_shr_mem[vptr->group_num].scen_group_name,
                                          svr_entry->server_name,
                                          nslb_sock_ntop((struct sockaddr *)&cptr->cur_server),
                                          cptr->conn_fd,
                                          cptr->url_num->index.svr_ptr->server_hostname, srcip);
    } 
    
#ifdef NS_DEBUG_ON
    NSDL3_SOCKETS(vptr, cptr, "cptr->flags = %x", cptr->flags);
    if(cptr->flags & NS_CPTR_FLAGS_CON_USING_PROXY)
    {
      NSDL3_SOCKETS(vptr, cptr, "Connecting to proxy server = %s, with fd %d", 
                            nslb_sockaddr_to_ip((struct sockaddr *)&cptr->conn_server, 1), cptr->conn_fd);
    }
    else
    {
      NSDL3_SOCKETS(vptr, cptr, "Connecting to serve = %s, with fd %d", svr_entry->server_name, cptr->conn_fd);
    }
#endif
/****************/
   if((cptr->request_type == JRMI_REQUEST) && !(cptr->flags & NS_CPTR_JRMI_REG_CON)){
    char s_n[2048];
    char *ptr;
    struct sockaddr_in *sin;
     memset((char *)&cptr->conn_server, 0, sizeof(struct sockaddr_in6));
     sin = (struct sockaddr_in *) &cptr->conn_server;
     sin->sin_family = AF_INET;
     sin->sin_port = htons(cptr->url_num->proto.jrmi.port);

     strcpy(s_n, svr_entry->server_name);
     if((ptr = strchr(s_n, ':')) != NULL)
       *ptr = '\0';
     sin->sin_addr.s_addr = inet_addr(s_n);
    }
/****************/

    if (connect( cptr->conn_fd, (SA *) &(cptr->conn_server), sizeof(struct sockaddr_in6)) < 0 ) {
      if ( errno == EINPROGRESS ) {
        /*{
        struct sockaddr_in sockname;
        int socksize;
        getsockname(cptr->conn_fd, (struct sockaddr *)&sockname, &socksize);
        printf("after first connect, port: %hd\n", ntohs(sockname.sin_port));
        }*/
        cptr->conn_state = CNST_CONNECTING;
        NSDL3_HTTP(NULL, cptr, "Error: connect failed, setting conn_state to CNST_CONNECTING");
#ifndef USE_EPOLL
        FD_SET( cptr->conn_fd, &g_wfdset );
#endif
        return -1;
      } else if (errno == ECONNREFUSED) {
        switch(cptr->url_num->request_type) {
          case SSL_SOCKET_REQUEST:
          {
            if (IS_TCP_CLIENT_API_EXIST)
            {
              fill_tcp_client_avg(vptr, CON_FAILED, 0);
              fill_tcp_client_failure_avg(vptr, NS_REQUEST_CONFAIL);
            }

            if (IS_UDP_CLIENT_API_EXIST)
            {
              fill_udp_client_avg(vptr, CON_FAILED, 0);
              fill_udp_client_failure_avg(vptr, NS_REQUEST_CONFAIL);
            }
            break;
          }
          case HTTP_REQUEST:
          case HTTPS_REQUEST:
          case SOCKET_REQUEST:   //Increment avgtime for http
            average_time->num_con_fail++; // Increament Number of TCP Connection failed
            break;
          case SMTP_REQUEST:
          case SMTPS_REQUEST:
            average_time->smtp_num_con_fail++; // Increament Number of TCP Connection failed
            break;
          case POP3_REQUEST:
          case SPOP3_REQUEST:
            average_time->pop3_num_con_fail++; // Increament Number of TCP Connection failed
            break;
          case FTP_REQUEST:
          case FTP_DATA_REQUEST:
            ftp_avgtime->ftp_num_con_fail++; // Increament Number of TCP Connection failed
            break;
          case DNS_REQUEST:
            average_time->dns_num_con_fail++; // Increament Number of TCP Connection failed
            break;
          case LDAP_REQUEST:
          case LDAPS_REQUEST:
            ldap_avgtime->ldap_num_con_fail++; // Increament Number of TCP Connection failed
            break;
          case IMAP_REQUEST:
          case IMAPS_REQUEST:
            imap_avgtime->imap_num_con_fail++; // Increament Number of TCP Connection failed
            break;
          case JRMI_REQUEST:
            jrmi_avgtime->jrmi_num_con_fail++; // Increament Number of TCP Connection failed
            break;
          case WS_REQUEST:
          case WSS_REQUEST:
            ws_avgtime->ws_num_con_fail++;     // Increment Number of TCP Connection failed
            break;
       }

#ifdef RMI_MODE
        rmi_connection_retry();
#else
        fprintf(stderr, "Error: start_socket(): Connection refused from server %s\n", nslb_get_src_addr(cptr->conn_fd));
        Close_connection(cptr, 0, now, NS_REQUEST_CONFAIL, NS_COMPLETION_NOT_DONE);
#endif
        return 0;
      } else {
        char srcip[128];
        average_time->num_con_fail++; // Increament Number of TCP Connection failed
        strcpy (srcip, nslb_get_src_addr(cptr->conn_fd));
        printf("Connection failed at %llu (nvm=%d sess_inst=%u user_index=%u src_ip=%s) to %s\n", 
                now, my_child_index, vptr->sess_inst, vptr->user_index, srcip, nslb_sock_ntop((SA *)&cptr->cur_server));
        //Earlier ips[cur_ip_entry].port
        perror( get_url_req_url(cptr) );
        retry_connection(cptr, now, NS_REQUEST_CONFAIL);
        return 0;
      }
    }

    inc_con_num_and_succ(cptr); // Increament Number of TCP Connection success

    now = get_ms_stamp();//Need to calculate connect time
    cptr->connect_time = now - cptr->ns_component_start_time_stamp; //connection time diff
    cptr->ns_component_start_time_stamp = now;//Update NS component start time

    SET_MIN (average_time->url_conn_min_time, cptr->connect_time);
    SET_MAX (average_time->url_conn_max_time, cptr->connect_time);
    average_time->url_conn_tot_time += cptr->connect_time;
    average_time->url_conn_count++;

    UPDATE_GROUP_BASED_NETWORK_TIME(connect_time, url_conn_tot_time, url_conn_min_time, url_conn_max_time, url_conn_count);

    if (IS_TCP_CLIENT_API_EXIST)
      fill_tcp_client_avg(vptr, CON_TIME, cptr->connect_time);

    if (IS_UDP_CLIENT_API_EXIST)
      fill_udp_client_avg(vptr, CON_TIME, cptr->connect_time);

#ifdef RMI_MODE
    if (cptr->url_num->request_type == JBOSS_CONNECT_REQUEST) {
      cptr->conn_state = CNST_JBOSS_CONN_READ_FIRST;
      cptr->total_bytes = cptr->url_num->proto.http.first_mesg_len; /* amt of bytes to expect for the first reading */
      handle_jboss_read(cptr, now);
      return 0;
    }
    if (cptr->url_num->request_type == RMI_CONNECT_REQUEST) {
      handle_rmi_connect(cptr, now);
      return 0;
    }
#endif
  } else { /* if conn.state == CONST_REUSE_CON, reset stat times */
    NS_DT4(vptr, cptr, DM_L1, MM_SOCKETS,
		 "Reusing connection with fd %d to server '%s' (%s)"
		 " (Recorded Server: %s)",
		 cptr->conn_fd,
	         cptr->old_svr_entry->server_name,
		 nslb_sock_ntop((struct sockaddr *)&cptr->cur_server),
		 cptr->url_num->index.svr_ptr->server_hostname); 
     cptr->started_at = cptr->ns_component_start_time_stamp = now;

    //set ws_cptr
    if((cptr->url_num->request_type == WS_REQUEST) || (cptr->url_num->request_type == WSS_REQUEST))
    {
      vptr->ws_cptr[ws_idx_list[vptr->first_page_url->proto.ws.conn_id]] = cptr;
      NSDL2_WS(vptr, cptr, "CONST_REUSE_CON: vptr->first_page_url->proto.ws.conn_id = %d, ws_conn id = %d, "
                          "vptr->ws_cptr[vptr->first_page_url->proto.ws.conn_id] = %p", 
                           vptr->first_page_url->proto.ws.conn_id, ws_idx_list[vptr->first_page_url->proto.ws.conn_id], 
                           vptr->ws_cptr[vptr->first_page_url->proto.ws.conn_id]);
    }
    else if((cptr->request_type == SOCKET_REQUEST) || (cptr->request_type == SSL_SOCKET_REQUEST))
    {
      vptr->conn_array[vptr->first_page_url->proto.socket.norm_id] = cptr;
      NSDL2_WS(vptr, cptr, "CONST_REUSE_CON: vptr->first_page_url->proto.socket.norm_id = %d, "
                           "vptr->conn_array[vptr->first_page_url->proto.socket.norm_id] = %p", 
                            vptr->first_page_url->proto.socket.norm_id, 
                            vptr->conn_array[vptr->first_page_url->proto.socket.norm_id]);
    }


    // AN-TODO Need optimization as code is duplicate in if block of reuse also
    // http_make_url_and_check_cache() musst be called after we have the svr_entry
    // and before any idle timer is started
    // CHECK  -Keep Alive timeout is started after repsone is recieved and next req is not to be send immeditaley
    if(cptr->url_num->request_type == HTTP_REQUEST ||
       cptr->url_num->request_type == HTTPS_REQUEST) {
      int ret = 0;
      http_make_url_and_check_cache(cptr, now, &ret);
      if(ret == -10)
        return ret;

      if(NS_IF_TRACING_ENABLE_FOR_USER && cptr->url_num->proto.http.type != EMBEDDED_URL) {
        NSDL2_USER_TRACE(vptr, cptr, "Method called, User tracing enabled");
        ut_update_url_values(cptr, ret); // It will save the first URL of page as there is check in the function
      }

      if(ret == CACHE_RESP_IS_FRESH) {
         NSDL2_CACHE(NULL, cptr, "Serving Response for Request [%s] From Cache with reuse connection. "
				" conn_fd = %d, conn_state = %d",
			        cptr->url, cptr->conn_fd, cptr->conn_state); 
         /*To avoid stack overflow we are putting Close_connections into timer with a 10 ms.*/
         set_cache_serve_callback(cptr, now);
         return ret;
      }
    }
    
    /* Handling of server push, in case of resources were pushed , we will stop the request from going on to the network, will serve it from
    stored response.
    Will be checked only when Server Push enabled. In case of get_no_inline server push should be disabled
    */
    if(cptr->http_protocol == HTTP_MODE_HTTP2 && runprof_table_shr_mem[vptr->group_num].gset.http2_settings.enable_push && cptr->http2){
      int ret_val = 0;
      ns_check_pushed_response(cptr, now, &ret_val);
      if(ret_val)
      return ret_val;
    }
    /* Set timouts for all protocals*/
      set_cptr_for_new_req(cptr, vptr, now);
  }

  /* Connect succeeded instantly, so handle it now. */
  handle_connect( cptr, now, 0 );
  return 0;
}

/* 1. Create UDP Socket
 * 2. Bind it.
 * 3. Add select  
 * 4. Do not set_socket_for_wan as WAN is only for TCP.
 * 5. No HIGH PERFORMANCE MODE for UDP Socket
 */
int
start_udp_socket(connection* cptr, u_ns_ts_t now ) {
  VUser *vptr = cptr->vptr;
  PerHostSvrTableEntry_Shr* svr_entry;
  int select_port;

  /* In TCP version, If timer is Keep-Alive time out then need to delete the timer.
   * as we are sending new request*/
  /* Not needed in UDP as KA is only fot connections. For UDP we do not make any connections */

  NSDL2_SOCKETS(NULL, cptr, "Method called, total_ip_entries = %d,"
                            " cptr=%p, request_type = %hd",
                            total_ip_entries, cptr,
                            cptr->url_num->request_type);

  reset_cptr_attributes(cptr);

  if (cptr->num_retries == 0) //This is the first try
  {
    cptr->started_at = cptr->con_init_time = now;
  }

  if ((svr_entry = get_svr_entry(vptr, cptr->url_num->index.svr_ptr)) == NULL) {
    fprintf(stderr, "Start Udp Socket: Unknown host.\n");
    end_test_run();
  } else {
    cptr->old_svr_entry = svr_entry;
  }

  /* Set timouts for all protocals*/
  set_cptr_for_new_req(cptr, vptr, now);

  if (runprof_table_shr_mem[vptr->group_num].gset.use_same_netid_src) {
    if (svr_entry->net_idx != vptr->user_ip->net_idx) {
      vptr->user_ip = get_src_ip(vptr, svr_entry->net_idx);
    }
  }

  if (!nslb_fill_sockaddr(&(cptr->cur_server), svr_entry->server_name, ntohs(svr_entry->saddr.sin6_port))) {
       fprintf(stderr, "Error: Host <%s> specified by Host header is not a valid hostname. Exiting \n",
       svr_entry->server_name);
       end_test_run();
  }

  // We need to keep the reuse address in UDP also (SO_REUSEADDR)
  cptr->conn_fd = get_udp_socket(vptr->user_ip->ip_addr.sin6_family, vptr->group_num);
  BIND_SOCKET((char*)&(vptr->user_ip->ip_addr),
              v_port_table[my_port_index].min_port,
              v_port_table[my_port_index].max_port);

#ifdef USE_EPOLL
  num_set_select++; /*TST */
  if (add_select(cptr, cptr->conn_fd, EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLET) < 0) {
    fprintf(stderr, "Error: Set Select failed on WRITE EVENT\n");
    end_test_run();
  }
#endif

  /* For UDP, there is no connection made. So do not increament connection counters.
   * Also set connection related time to -1 */
  cptr->connect_time = -1;
  now = get_ms_stamp();

  if((cptr->url_num->request_type == SOCKET_REQUEST) || (cptr->url_num->request_type == SSL_SOCKET_REQUEST))
  {
    //Registering function for handling receive data
    cptr->proc_recv_data = process_socket_recv_data;

    vptr->conn_array[vptr->first_page_url->proto.socket.norm_id] = cptr;
    NSDL2_WS(vptr, cptr, "CNST_FREE: vptr->first_page_url->proto.socket.norm_id = %d,"
                         "vptr->conn_array[vptr->first_page_url->proto.socket.norm_id] = %p",
                          vptr->first_page_url->proto.socket.norm_id,
                          vptr->conn_array[vptr->first_page_url->proto.socket.norm_id]);
  }

  /* In UDP, there is no connection made. Calling handle_connect() with 0 so that it does not check for connection
   *   After the code will be same as used in TCP */
  handle_connect(cptr, now, 0);
  return 0;
}


