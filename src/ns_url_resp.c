/**
 * ns_url_resp.c
 *
 * This file contains methods for processing of URL response
 */
#include <ctype.h>
#include <regex.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "nslb_time_stamp.h"
#include "url.h"
#include "ns_tag_vars.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "nslb_util.h"
#include "nslb_sock.h"
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

#include "netstorm.h"
#include "ns_log.h"
#include "decomp.h"
#include "divide_users.h"
#include "divide_values.h"
#include "child_init.h"
#include "runlogic.h"
#include "ns_vars.h"
#include "ns_alloc.h"
#include "ns_debug_trace.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "ns_url_req.h"
#include "ns_page.h"

#include "ns_trace_log.h"
#include "ns_pop3.h"
#include "ns_smtp.h"
#include "ns_ftp.h"
#include "ns_dns.h"
#include "ns_http_cache_hdr.h"
#include "ns_data_types.h"
#include "ns_http_cache_reporting.h"
#include "ns_log_req_rep.h"
#include "ns_auto_redirect.h"
#include "ns_http_cache.h"
#include "ns_js.h"
#include "ns_vuser_trace.h"
#include "ns_page_dump.h"
#include "nslb_hessian.h"
#include "protobuf/nslb_protobuf_adapter.h"
#include "comp_decomp/nslb_comp_decomp.h"

#include "amf.h"
#include "ns_ldap.h"
#include "ns_java_obj_mgr.h"
#include "ns_jrmi.h"
#include "ns_click_script.h"
#include "ns_group_data.h"
#include "ns_exit.h"
#include "comp_decomp/nslb_comp_decomp.h"

char *url_resp_buff;
int url_resp_size;
extern connection *cur_cptr;
extern char interactive_buf[4096];
char *full_buffer = NULL;
static int full_buffer_len;
char *hessian_buffer = NULL;
char *resp_body = NULL;
int resp_body_len = 0;

char *uncompress_reply_body(char *inp_buffer_body, int body_size, VUser *vptr, int *blen, connection *cptr);

char *proto_to_str(int request_type)
{
  if(request_type < 0 || request_type > g_proto_str_max_idx)
    return "Invalid Request Type";
  else
    return g_proto_str[request_type];
}

char *get_request_string(connection *cptr)
{
  /*bug 52092: get request_type direct from cptr, instead of url_num*/
  int request_type = cptr->request_type;
  char request_buf[MAX_LINE_LENGTH + 1];
  int request_buf_len;

  switch (request_type) {
  case HTTP_REQUEST:
  case HTTPS_REQUEST:
    return get_url_req_line(cptr, request_buf, &request_buf_len, MAX_LINE_LENGTH);

  case SMTP_REQUEST:
  case SMTPS_REQUEST:
    return smtp_state_to_str(cptr->proto_state);

  case POP3_REQUEST:
  case SPOP3_REQUEST:
    return pop3_state_to_str(cptr->proto_state);

  case FTP_REQUEST:
    return ftp_state_to_str(cptr->proto_state);

  case DNS_REQUEST:
    return dns_state_to_str(cptr->proto_state);

//  case LDAP_REQUEST:
  //  return ldap_state_to_str(cptr->proto_state);

  /*case IMAP_REQUEST:
  case IMAPS_REQUEST:
    return imap_state_to_str(cptr->proto_state);
  */
  default:
    return "UNKNOWN";
  }
}

char *cptr_to_string(connection *cptr, char *str, int size)
{
  int request_type = cptr->request_type;

  snprintf(str, size, 
      "%s = %s, location_url = %s, redirect_count = %d, conn_fd = %d, "
      "conn_state = %d, header_state = %d, chunked_state = %d, cookie_hash_code = %lu, "
      "cookie_idx = %d, gServerTable_idx = %d, started_at = %llu, connect_time = %d, "
      "ssl_handshake_time = %d, write_complete_time = %d, "
      "first_byte_rcv_time = %d, request_complete_time = %d, http_payload_sent = %d, tcp_bytes_sent = %d, "
      "req_ok = %d, completion_code = %d, num_retries = %d, resp_code = %d, resp_code_filled = %d, "
      "con_init_time = %llu, end_time = %llu, content_length = %d, bytes = %u, tcp_bytes_recv = %u, "
      "total_bytes = %u, body_offset = %d, checksum = %d, uid = %d, connection_type = %d, "
      "compression_type = %d, num_ka = %d, ssl = %p, chunk_size = %d, old_svr_entry = %p, buf_head = %p, "
      "cur_buf = %p, free_array = %p, send_vector = %p, bytes_left_to_send = %d, num_send_vectors = %d, "
      "first_vector_offset = %d, last_iov_base = %p ", 
      proto_to_str(request_type), get_request_string(cptr),
        cptr->location_url,
        cptr->redirect_count,
        cptr->conn_fd,
        cptr->conn_state,
        cptr->header_state, 
        cptr->chunked_state,
        cptr->cookie_hash_code, 
        cptr->cookie_idx,
        cptr->gServerTable_idx,
        cptr->started_at, 
        cptr->connect_time,
        cptr->ssl_handshake_time,
        cptr->write_complete_time,
        cptr->first_byte_rcv_time,
        cptr->request_complete_time,
        cptr->http_payload_sent,
        cptr->tcp_bytes_sent,
        cptr->req_ok,
        cptr->completion_code,
        cptr->num_retries,
        cptr->req_code,
        cptr->req_code_filled,
        cptr->con_init_time,
        cptr->end_time,
        cptr->content_length,
        cptr->bytes,
        cptr->tcp_bytes_recv,
        cptr->total_bytes, 
        cptr->body_offset,
        cptr->checksum,
        cptr->uid,
        cptr->connection_type, 
        cptr->compression_type,
        cptr->num_ka,
        cptr->ssl,
        cptr->chunk_size,
        cptr->old_svr_entry, 
        cptr->buf_head,
        cptr->cur_buf,
        cptr->free_array,
        cptr->send_vector,
        cptr->bytes_left_to_send,
        cptr->num_send_vectors,
        cptr->first_vector_offset,
        cptr->last_iov_base
        );
  return str;
}

char *vptr_to_string(VUser *vptr, char *str, int size)
{
  snprintf(str, size,
          "Page name = %s, next_page = %p, first_page_url = %p,"// start_url_ridx = %d, end_url_ridx = %d, "
          "urls_left = %d, urls_awaited = %d, pg_begin_at = %llu, "
          "is_cur_pg_bad = %d, is_embd_autofetch = %d, http_req_free = %p, num_http_req_free = %d, "
          "sess_status = %d, vuser_state = %d, sess_think_duration = %u, head_hlist = %p, tail_hlist = %p, "
          "hptr = %p, started_at = %llu, modem_id = %d, sess_ptr = %p, rp_ptr = %p, up_ptr = %p, location = %p, "
          "access = %p, browser = %p, cnum_parallel = %d, cmax_parallel = %d, "
          "per_svr_max_parallel = %d, compression_type = %d, head_creuse = %p, tail_creuse = %p, "
          "freeConnHead = %p, *ugtable = %p, *uctable = %p, udvtable = %p, ustable = %p, "
          "uvtable = %p, order_table = %p, server_entry_idx = %d, page_status = %d, tx_ok = %d, free_next = %p, "
          "busy_next = %p, busy_prev = %p, sess_inst = %u, timer_ptr = %p, taghashptr = %p, hashcode = %d, "
          "referer = %s, referer_size = %d, clust_id = %d, group_num = %d, user_index = %u, user_ip = %p, "
          "buf_head = %p, cur_buf = %p, url_num = %p, bytes = %u, page_instance = %d, tx_instance = %d, "
          "pg_think_time = %d, flags = %lld, body_offset = %d, ssl_mode = %d, *tx_info_ptr = %p, "
          "*pcRunLogic = %p, *chunk_size_head = %p",
          
          //vptr->cur_page->page_name,
          vptr->cur_page ? vptr->cur_page->page_name : "(nil)",
          vptr->next_page, 
          vptr->first_page_url, 
          //vptr->start_url_ridx, 
          //vptr->end_url_ridx, 
          vptr->urls_left, 
          vptr->urls_awaited, 
          vptr->pg_begin_at, 
          vptr->is_cur_pg_bad, 
          vptr->is_embd_autofetch, 
          vptr->http_req_free, 
          vptr->num_http_req_free, 
          vptr->sess_status, 
          vptr->vuser_state, 
          vptr->sess_think_duration, 
          vptr->head_hlist, 
          vptr->tail_hlist, 
          vptr->hptr, 
          vptr->started_at, 
          vptr->modem_id, 
          vptr->sess_ptr, 
          vptr->rp_ptr, 
          vptr->up_ptr, 
          vptr->location, 
          vptr->access, 
          vptr->browser, 
          // vptr->machine, 
          // vptr->freq, 
          vptr->cnum_parallel, 
          vptr->cmax_parallel, 
          vptr->per_svr_max_parallel, 
          vptr->compression_type, 
          vptr->head_creuse, 
          vptr->tail_creuse, 
          vptr->freeConnHead, 
          vptr->ugtable, 
          vptr->uctable, 
          vptr->udvtable, 
          vptr->ustable, 
          vptr->uvtable, 
          vptr->order_table, 
          vptr->server_entry_idx, 
          vptr->page_status, 
          vptr->tx_ok, 
          vptr->free_next, 
          vptr->busy_next, 
          vptr->busy_prev, 
          vptr->sess_inst, 
          vptr->timer_ptr, 
          vptr->taghashptr, 
          vptr->hashcode, 
          vptr->referer, 
          vptr->referer_size, 
          vptr->clust_id, 
          vptr->group_num, 
          vptr->user_index, 
          vptr->user_ip, 
          vptr->buf_head, 
          vptr->cur_buf, 
          vptr->url_num, 
          vptr->bytes, 
          vptr->page_instance, 
          vptr->tx_instance, 
          vptr->pg_think_time, 
          vptr->flags, 
          vptr->body_offset, 
          vptr->ssl_mode,
          vptr->tx_info_ptr, 
          vptr->pcRunLogic,
          vptr->chunk_size_head
          );
  return str;
}

void copy_url_resp(connection *cptr) {
  int blen = cptr->bytes;
  int i;
  int complete_buffers = blen / COPY_BUFFER_LENGTH;
  int incomplete_buf_size = blen % COPY_BUFFER_LENGTH;
  struct copy_buffer *buffer;
  char *copy_cursor;
  int no_validation = runprof_table_shr_mem[((VUser *)(cptr->vptr))->group_num].gset.no_validation;
  CacheTable_t* cache_ptr = NULL;
  VUser *vptr = cptr->vptr;
  int url_resp_len = 0;
  int sess_id = -1;
  char save_url_rsp = -1;
  char *outbuff = NULL;
  int outbuff_len = 0;
  int url_resp_buff_size = 0;
  int hdr_size = 0;
  NSDL2_HTTP(NULL, cptr, "Method called, cptr = %p, complete_buffers = %d, incomplete_buf_size = %d", 
                          cptr, complete_buffers, incomplete_buf_size);

  /*bug id  78144 : check if cptr->cptr_data is available, in order to avoid crash if cptr->cptr_data is NULL */
  if( (cptr->cptr_data) && NS_IF_CACHING_ENABLE_FOR_USER)
  {
    cache_ptr = (CacheTable_t*)cptr->cptr_data->cache_data;
    if(NS_IF_CACHING_ENABLE_FOR_USER && (cache_ptr->cache_flags & NS_CACHE_ENTRY_VALIDATE) && cptr->req_code == 304)
    {
      NSDL2_HTTP(NULL, cptr, "Got 304 in resp code setting response from cache");
      cache_set_resp_frm_cache(cptr, vptr);
      blen = cptr->bytes;
      complete_buffers = blen / COPY_BUFFER_LENGTH;
      incomplete_buf_size = blen % COPY_BUFFER_LENGTH;
    }
  }

  if (cptr->buf_head) {
    buffer = cptr->buf_head;
  } else {
  //ANIL - for HTTP redirects  with body, size may be be non-zero but no data saved.
    NSDL2_HTTP(NULL, cptr, "Returning... Since buffer size is 0");

    /* In case RESP STATUS=302 and buffer size is zero but buffer is not empty
     *(i.e buffer is having resp of previous url). Then making buffer NULL */
    if(url_resp_buff != NULL)
      url_resp_buff[0] = '\0';

    return;
  }

  NSDL2_HTTP(NULL, cptr, "cptr = %p, url_resp_buff = %p, url_resp_size = %d,"
                         " cptr->buff_head = %p, complete_buffers = %d,"
                         " incomplete_buf_size = %d, blen = %d",
                         cptr, url_resp_buff, url_resp_size, buffer, complete_buffers,
                         incomplete_buf_size, blen);

  //In case of FTP protocol for PASV command o/p, we need data sent by Server
  //So if connections protocol state is PASV then no validation feature
  //will not apply
  if (no_validation && (cptr->proto_state != ST_FTP_PASV))
    return;

  // Abhay :- (BUGID - 52101) In case of Search=ALL we need to increase the url_resp_buff with header length also, so need to increase 
  //                          url_resp_len with header size
  if(vptr->response_hdr && (vptr->cur_page->save_headers & SEARCH_IN_HEADER))
    url_resp_len = vptr->response_hdr->hdr_buf_len + blen;
  else
    url_resp_len = blen;

  if((url_resp_buff == NULL) || (url_resp_len > url_resp_size))
  {
    MY_REALLOC_EX(url_resp_buff, url_resp_len + 1, url_resp_size + 1, "url_resp_buff", -1);
    url_resp_size = url_resp_len; // does not include null temination
  }

  copy_cursor = url_resp_buff;

  for (i = 0; i < complete_buffers; i++)
  {
    if (buffer && buffer->buffer) {
      memcpy(copy_cursor, buffer->buffer, COPY_BUFFER_LENGTH);
      copy_cursor += COPY_BUFFER_LENGTH;
      buffer = buffer->next;
    } else {
      char cptr_to_str[MAX_LINE_LENGTH + 1];
      char vptr_to_str[MAX_LINE_LENGTH + 1];
      cptr_to_string(cptr, cptr_to_str, MAX_LINE_LENGTH);
      vptr_to_string(cptr->vptr, vptr_to_str, MAX_LINE_LENGTH);
      
      error_log("%s|%d|buffer/buffer->buffer is null. cptr => %s, vptr => %s ", 
                __FUNCTION__, __LINE__, cptr_to_str, vptr_to_str);
      break;
    }
  }

  if (incomplete_buf_size) {
    if (buffer && buffer->buffer) {
      memcpy(copy_cursor, buffer->buffer, incomplete_buf_size);
    } else {
      char cptr_to_str[MAX_LINE_LENGTH + 1];
      char vptr_to_str[MAX_LINE_LENGTH + 1];
      cptr_to_string(cptr, cptr_to_str, MAX_LINE_LENGTH);
      vptr_to_string(cptr->vptr, vptr_to_str, MAX_LINE_LENGTH);
      
  NSDL2_HTTP(NULL, cptr, "url_resp_buff = %s", url_resp_buff);
      error_log("%s|%d|buffer/buffer->buffer is null. cptr => %s, vptr => %s ", 
                __FUNCTION__, __LINE__, cptr_to_str, vptr_to_str);
    }
  }

  //Prachi: Fixed bug - 4746
  url_resp_buff[blen] = 0;
  //url_resp_buff[url_resp_size] = 0;
  NSDL2_HTTP(NULL, cptr, "blen = %d, url_resp_buff %s", blen, url_resp_buff);
 
  /* Getting Session ID and get the URL response */ 
 
  sess_id = runprof_table_shr_mem[vptr->group_num].sess_ptr->sess_id;
  save_url_rsp = session_table_shr_mem[sess_id].save_url_body_head_resp;
  
  if((save_url_rsp && cptr->url_num->post_url_func_ptr) || (cptr->url_num->request_type == WS_REQUEST) || (cptr->url_num->request_type == WSS_REQUEST) || (cptr->url_num->request_type == SOCKET_REQUEST) || (cptr->url_num->request_type == SSL_SOCKET_REQUEST))
  {
    if(cptr->compression_type)
    {
      //decompress url_resp_buff, blen
      vptr->compression_type = cptr->compression_type;
      outbuff = uncompress_reply_body(url_resp_buff, blen, vptr, &outbuff_len, cptr);
    }
    else
    {
      outbuff = url_resp_buff;
      outbuff_len = blen;
    }
    
    if(vptr->response_hdr)
    {
      if(cptr->url_num->proto.http.type != MAIN_URL)
        vptr->response_hdr->used_hdr_buf_len = 0; //No Header Info for Inline 
      hdr_size = vptr->response_hdr->used_hdr_buf_len;
    }
    url_resp_buff_size = outbuff_len + hdr_size;
    if (url_resp_buff_size)
    {
      if(vptr->url_resp_buff_size < url_resp_buff_size)
      {
        NSLB_FREE_AND_MAKE_NOT_NULL(vptr->url_resp_buff, "url resp buffer", -1 , NULL);
        vptr->url_resp_buff_size = url_resp_buff_size;
        NSLB_MALLOC(vptr->url_resp_buff,  vptr->url_resp_buff_size + 1, "vptr->url_resp_buff", -1, NULL);
      }
      if(hdr_size)
      {
        memcpy(vptr->url_resp_buff, vptr->response_hdr->hdr_buffer, hdr_size);
      }
      memcpy(vptr->url_resp_buff + hdr_size, outbuff, outbuff_len);
      vptr->url_resp_buff[url_resp_buff_size] = '\0';
    }
    vptr->bytes = outbuff_len;
  }
#if 0

  memcpy(url_resp_hdr_buff, url_resp_buff, hlen);

  //Remove last one or two char ('\n' or '\r\n\' or '\r') and replace by NULL.
  if((url_resp_hdr_buff[hlen - 1] == '\r') || (url_resp_hdr_buff[hlen - 1] == '\n'))
  {
    if((url_resp_hdr_buff[hlen - 2] == '\r') || (url_resp_hdr_buff[hlen - 2] == '\n'))
      url_resp_hdr_buff[hlen - 2] = '\0';
    else
      url_resp_hdr_buff[hlen - 1] = '\0';
  }
#endif
}


//Add end---------------------------------------------
// 02/01/07 - Achint - New function added for support of Pre URL Callback
inline void on_url_start(connection *cptr, u_ns_ts_t now)
{
  NSDL2_HTTP(NULL, cptr, "Method called");
  VUser *vptr = cptr->vptr;
#ifdef NS_DEBUG_ON
  char request_buf[MAX_LINE_LENGTH];
  int request_buf_len;
#endif

  //#Shilpa 16Feb2011 - Commented average_time->fetches_started++ as wrongly done here. 
  //because was incremented even in case response is taken from cache. Moved to set_cptr_for_new_req()
  switch(cptr->url_num->request_type) {
    case HTTP_REQUEST:
    case HTTPS_REQUEST:
      //average_time->fetches_started++;
      if (cptr->url_num->pre_url_func_ptr != NULL) {
        NSDL3_HTTP(NULL, cptr, "Calling PRE_CB function (%s) for URL= %s",
                                cptr->url_num->pre_url_fname, 
                                get_url_req_line(cptr, request_buf,
                                                 &request_buf_len,
                                                 MAX_LINE_LENGTH));
        cur_cptr = cptr;
        TLS_SET_VPTR(vptr);
        cptr->url_num->pre_url_func_ptr();
      }
      if(cptr->url_num->proto.http.tx_hash_idx != -1)
      {
        tx_add_node(cptr->url_num->proto.http.tx_hash_idx, vptr, NS_TX_IS_INLINE, now);
        cptr->tx_instance = vptr->tx_instance;
      }
      break;
    case SMTP_REQUEST:
    case SMTPS_REQUEST:
      //average_time->smtp_fetches_started++;
      if (cptr->url_num->pre_url_func_ptr != NULL) {
        NSDL3_HTTP(NULL, cptr, "Calling PRE_CB function (%s)",
                                cptr->url_num->pre_url_fname);
        cptr->url_num->pre_url_func_ptr();
      }
      break;
    case POP3_REQUEST:
    case SPOP3_REQUEST:
      //average_time->pop3_fetches_started++;
      if (cptr->url_num->pre_url_func_ptr != NULL) {
        NSDL3_HTTP(NULL, cptr, "Calling PRE_CB function (%s)",
                                cptr->url_num->pre_url_fname);
        cptr->url_num->pre_url_func_ptr();
      }
      break;
    case FTP_REQUEST:
      //average_time->ftp_fetches_started++;
      if (cptr->url_num->pre_url_func_ptr != NULL) {
        NSDL3_HTTP(NULL, cptr, "Calling PRE_CB function (%s)",
                                cptr->url_num->pre_url_fname);
        cptr->url_num->pre_url_func_ptr();
      }
      break;
    case DNS_REQUEST:
      //average_time->dns_fetches_started++;
      if (cptr->url_num->pre_url_func_ptr != NULL) {
        NSDL3_HTTP(NULL, cptr, "Calling PRE_CB function (%s)",
                                cptr->url_num->pre_url_fname);
        cptr->url_num->pre_url_func_ptr();
      }
      break;
   /* case LDAP_REQUEST:
      if (cptr->url_num->pre_url_func_ptr != NULL) {
        NSDL3_HTTP(NULL, cptr, "Calling PRE_CB function (%s)",
                                cptr->url_num->pre_url_fname);
        cptr->url_num->pre_url_func_ptr();
      }
      break;
   */
   /* case IMAP_REQUEST:
      if (cptr->url_num->pre_url_func_ptr != NULL) {
        NSDL3_HTTP(NULL, cptr, "Calling PRE_CB function (%s)",
                                cptr->url_num->pre_url_fname);
        cptr->url_num->pre_url_func_ptr();
      }
      break;
   */
  }
}

// 02/01/07 - Achint - New function added for support of Post URL Callback
/* Curently uncompress is not done for body returned by ns_url_get_body_msg */
void on_url_done(connection *cptr, int redirect_flag, int taken_from_cache, u_ns_ts_t now)
{
  int blen = cptr->bytes;
  int complete_buffers = blen / COPY_BUFFER_LENGTH;
  int incomplete_buf_size = blen % COPY_BUFFER_LENGTH;
  struct copy_buffer *buffer = cptr->buf_head;
  struct copy_buffer *old_buffer;
  int i;
  VUser *vptr = cptr->vptr;

#ifdef NS_DEBUG_ON
  char request_buf[MAX_LINE_LENGTH + 1];
  int request_buf_len;
#endif

  NSDL2_HTTP(NULL, cptr, "Method called, type = %d, redirect_flag = 0x%x, post_url_func_ptr = %p, cptr->buf_head = %p, cptr->flags = 0x%x",
			 cptr->url_num->proto.http.type, redirect_flag, cptr->url_num->post_url_func_ptr, cptr->buf_head, cptr->flags);



  if ((cptr->url_num->post_url_func_ptr != NULL) ||
      ((cptr->url_num->proto.http.type == MAIN_URL) && 
       ((!(redirect_flag & NS_HTTP_REDIRECT_URL)) ||
        ((redirect_flag & NS_HTTP_REDIRECT_URL) && (vptr->cur_page->redirection_depth_bitmask & (1 << (vptr->redirect_count -1))))
       )) || 
       ((cptr->flags & NS_CPTR_CONTENT_TYPE_MPEGURL) || (cptr->flags & NS_CPTR_CONTENT_TYPE_MEDIA)) || 
      (cptr->url_num->request_type == WS_REQUEST || cptr->url_num->request_type == WSS_REQUEST) 
     )
  {
    copy_url_resp(cptr);
  }
  NSDL4_HTTP(NULL, cptr, "Method called, cptr->url_num->postcallback_rdepth_bitmask = %04x , (1 << (vptr->redirect_count -1)) = %04x ",
                                         cptr->url_num->postcallback_rdepth_bitmask, (1 << (vptr->redirect_count -1)));

  if ((cptr->url_num->post_url_func_ptr != NULL ) && (cptr->url_num->postcallback_rdepth_bitmask & (1 << (vptr->redirect_count -1))))
  {
    cur_cptr = cptr;
    // We are setting cur_vptr here because if vptr is not set, and other user started then cur_vptr may be
    // set to another users vptr Bug: 4877
    TLS_SET_VPTR(vptr);
    NSDL3_HTTP(NULL, cptr, "After returned from copy_url_resp, calling post_url_func_ptr(), cptr = %p, cur_cptr = %p",cptr, cur_cptr);
    cptr->url_num->post_url_func_ptr();
    NSDL3_HTTP(NULL, cptr, "Post CB function is called");
    cur_cptr = NULL;
  }

  
  if(cptr->url_num->proto.http.tx_hash_idx != -1)
    tx_end_inline_tx("InlineTx", cptr->req_ok, cptr->tx_instance, vptr, now, cptr->url_num->proto.http.tx_hash_idx);

  // We are freeing link header value here, as we are not using it after post url callback
  FREE_AND_MAKE_NULL(cptr->link_hdr_val, "cptr->link_hdr_val", -1);

  //if ((cptr->url_num->proto.http.type == MAIN_URL) && NS_IF_PAGE_DUMP_ENABLE && (runprof_table_shr_mem[vptr->group_num].gset.trace_level == TRACE_LEVEL_1) && ((runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == 0) || ((runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == 1) && (vptr->page_status != NS_REQUEST_OK))))

  /* In case of trace level 1, we will be logging url details from seperate flow other
   * than vuser trace structure or one through make_page_dump_buff*/
  if (
   ((runprof_table_shr_mem[vptr->group_num].gset.trace_level == TRACE_URL_DETAIL) && (cptr->url_num->proto.http.type == MAIN_URL))&&   ((runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_ALL_SESS || runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_PAGE_ON_HEADER_BASED) || ((runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_FAILED_PG) && (cptr->req_ok != NS_REQUEST_OK))))
  {
    /*Need to call inside this because if trace_on_failure is 1 and URL is not failing then */
    if(need_to_dump_session(vptr, runprof_table_shr_mem[vptr->group_num].gset.trace_limit_mode, vptr->group_num)){
      NSDL3_HTTP(NULL, cptr, "Dumping Url infomation ");
      dump_url_details(cptr, now);
    }
  }

/*
Logic for freeing response:
				In Cache	Not In cache
Main is not redirected Main 	   N                 N -> This will get freed in do_data_processing
Main is redircted (MainR1)
  Not Used in search               N                 Y
  Used in search                   N                 N -> This will get freed in do_data_processing
Final response of Main redirection
  Not Used in search               N                 Y
  Used in search                   N                 N -> This will get freed in do_data_processing
Embedded                           N                 Y

*/

  int free_response = 1;
  if(!taken_from_cache)  // Entry is not in cache
  {
    if(NS_IF_CACHING_ENABLE_FOR_USER && cptr->cptr_data != NULL 
                          && cptr->cptr_data->cache_data != NULL) {
      // If entry is in cache, it will always return 0
      // If entry is not in cache, it will always return 1 
      free_response = cache_update_table_entries(cptr);

      NSDL3_HTTP(NULL, cptr, "free_response = %d", free_response);
    }
    else // User is not enabled for caching
    {
      // To be or not will be decided below
    }
  } 
  else  // Entry is in cache
  {
    NSDL3_HTTP(NULL, cptr, "Response is taken from cache do not free it");
    free_response = 0; // As entry is in cache, do not free it
  }

  if(cptr->flags & NS_CPTR_RESP_FRM_CACHING)
  { 
    //Just to check. It should not happen
    NSDL3_HTTP(NULL, cptr, "cptr NS_CPTR_RESP_FRM_CACHING bit is already setted.");
  }
 
  // At this point, only if entry is in cache, free_response will be 0
  if(free_response == 0) 
  {
    NSDL3_HTTP(NULL, cptr, "Response is taken from cache. Setting cptr NS_CPTR_RESP_FRM_CACHING bit.");
    //TODO : Wat if max conn is 1 and main url is cacheable and emd is not cacheble 
    cptr->flags |= NS_CPTR_RESP_FRM_CACHING;
    return;
  }
  
  //Assiging buf_head here bcoz resp can come from cache.
  //In that it acn be not NULL
  buffer = cptr->buf_head;
  blen = cptr->bytes;
  complete_buffers = blen / COPY_BUFFER_LENGTH;
  incomplete_buf_size = blen % COPY_BUFFER_LENGTH;

  /* For embedded URL with no URL callback, response is not saved buffer will be NULL */
  if (!buffer) 
  {
    NSDL3_HTTP(NULL, cptr, "Buffer is NULL, returning..");
    return;
  }
  // At this point, entry is not cached. So we need to find out if Main url is to be freed here or not?
  // We should not free Main URL in these cases:
  //   Main not redirected Or
  //   Main redirected and has search param
  
  if((cptr->url_num->request_type == WS_REQUEST || cptr->url_num->request_type == WSS_REQUEST) || cptr->url_num->proto.http.type == MAIN_URL)
  {
    if((!(redirect_flag & NS_HTTP_REDIRECT_URL)) || 
                ((redirect_flag & NS_HTTP_REDIRECT_URL) &&
                 (vptr->cur_page->redirection_depth_bitmask & (1 << (vptr->redirect_count -1)))))
    {
      NSDL3_HTTP(NULL, cptr, "Not freeing the respone buffer, since variables are going to apply on it, Returning... ");
      return;
    }
  }
  // Free in case of not Main URL or (Main URL and is redirected)
  // Final URL of Main redirection chain is freed in do_data_processing()
  // Free response if emb & free_response
  // we need to free
  {

    if (cptr->url_num->request_type == HTTP_REQUEST ||
        cptr->url_num->request_type == HTTPS_REQUEST) {
      NSDL3_HTTP(NULL, cptr, "freeeing buffers for URL request line = %s,"
                             " blen = %d, complete_buffers = %d,"
                             " incomplete_buf_size = %d",
                             get_url_req_line(cptr, request_buf, &request_buf_len,
                                                                 MAX_LINE_LENGTH),
                             blen, complete_buffers, incomplete_buf_size);
    } else {
      NSDL3_HTTP(NULL, cptr, "freeeing buffers, blen = %d, "
                             "complete_buffers = %d, incomplete_buf_size = %d",
                             blen, complete_buffers, incomplete_buf_size);
    }

    buffer = cptr->buf_head;
    for (i = 0; i < complete_buffers; i++)
    {
      if (buffer) {
        old_buffer = buffer;
        buffer = buffer->next;

        NSDL3_HTTP(NULL, cptr, "Freeing buffer = %p", old_buffer);
        FREE_AND_MAKE_NOT_NULL(old_buffer, "Freeing buffer", i);
      } else {
        char cptr_to_str[MAX_LINE_LENGTH + 1];
        char vptr_to_str[MAX_LINE_LENGTH + 1];
        cptr_to_string(cptr, cptr_to_str, MAX_LINE_LENGTH);
        vptr_to_string(cptr->vptr, vptr_to_str, MAX_LINE_LENGTH);
          
        error_log("%s|%d|buffer is null. cptr => %s, vptr => %s ", 
                  __FUNCTION__, __LINE__, cptr_to_str, vptr_to_str);
        break;
      }
    }
    if (incomplete_buf_size) {
      if (buffer) {
        NSDL3_HTTP(NULL, cptr, "Freeing buffer = %p", buffer);
        FREE_AND_MAKE_NOT_NULL(buffer, "Freeing last incomplete buffer", -1);
      } else {
        char cptr_to_str[MAX_LINE_LENGTH + 1];
        char vptr_to_str[MAX_LINE_LENGTH + 1];
        cptr_to_string(cptr, cptr_to_str, MAX_LINE_LENGTH);
        vptr_to_string(cptr->vptr, vptr_to_str, MAX_LINE_LENGTH);
      
        error_log("%s|%d|buffer/buffer->buffer is null. cptr => %s, vptr => %s ", 
                  __FUNCTION__, __LINE__, cptr_to_str, vptr_to_str);
      }
    }
    cptr->buf_head = cptr->cur_buf = NULL;
  }
}  


char *uncompress_reply_body(char *inp_buffer_body, int body_size, VUser *vptr, int *blen, connection *cptr)
{
#if 0
  char *body_chunk_extract;
  chunk_size_node *chunk_size_list, *old;
  int chunk_sz;
  int cur_size = 0;
  int vptr_bytes;
#endif
  char err_msg[1024];

/*   FILE *foo; */

  uncomp_cur_len = 0;

  NSDL1_HTTP(vptr, NULL, "Method called");

  NSDL3_HTTP(vptr, NULL, "Method called. body size = %d", body_size);


  // 16-oct-2013: In case of URL failures we need do decompress response because it will be used in page dump and debugging. Bug Id: mentis 192 
  /* if (vptr->page_status != NS_REQUEST_OK) {
    NSDL3_HTTP(vptr, NULL, "uncompression was not done since page was not successful");
    //printf("uncompression was not done since page was not successful\n");
    return inp_buffer_body;
  }*/


  // If un/docompress fails, set url status to UncompFail (TBD)
    
  if ( 1 /* vptr->chunk_size_head == NULL */) { /* Case compression = yes, chunk = no */
    //if (ns_decomp_do_new (inp_buffer_body, body_size, vptr->compression_type, err_msg)) 
    NSDL2_HTTP(vptr, NULL, "Before Decompressing: uncomp_buf = %p, uncomp_max_len = %d, uncomp_cur_len = %d, "
                           "compression_type = %d, body_size = %d",
                            uncomp_buf, uncomp_max_len, uncomp_cur_len, vptr->compression_type, body_size); 
    if (nslb_decompress(inp_buffer_body, body_size, &uncomp_buf, (size_t *)&uncomp_max_len, (size_t *)&uncomp_cur_len, vptr->compression_type,
          err_msg, 1024)) {
      char cptr_to_str[MAX_LINE_LENGTH + 1];
      cptr_to_string(cptr, cptr_to_str, MAX_LINE_LENGTH);
      NSEL_MIN(NULL, cptr, ERROR_ID, ERROR_ATTR,
                  "Error Decompressing: %s, Received code = %d for host=%s"
                  "page=%s, sess=%s, cptr => %s",
                  err_msg,
                  cptr->req_code,
                  nslb_sock_ntop((struct sockaddr *)&cptr->cur_server),
                  ((VUser *)cptr->vptr)->cur_page->page_name,
                  ((VUser *)cptr->vptr)->sess_ptr->sess_name,
                  cptr_to_str);

      NSDL3_HTTP(vptr, NULL, "Error Decompressing: %s", err_msg);
      //fprintf (stderr, "Error decompressing non-chunked body: %s\n", err_msg);  /*bug 78764 : commented unwanted trace*/
      //full_buffer = inp_buffer;
      return inp_buffer_body;
    } else {
      *blen = vptr->bytes = uncomp_cur_len;
       NSDL2_HTTP(vptr, NULL, "After Decompressing: uncomp_buf = %p, uncomp_max_len = %d, uncomp_cur_len = %d, "
                              "compression_type = %d",
                               uncomp_buf, uncomp_max_len, uncomp_cur_len, vptr->compression_type); 
      return uncomp_buf;
    }
  }


// Commented as above block is always true
#if 0
  chunk_size_list = vptr->chunk_size_head;
  body_chunk_extract = inp_buffer_body;
  vptr_bytes = vptr->bytes;
  vptr->bytes = 0;
  if (init_ns_decomp_do_continue(vptr->compression_type) < 0) {
    printf("Unable to initialize decompression\n");
    return inp_buffer_body;
  }

  for (chunk_sz = chunk_size_list->chunk_sz; chunk_sz != 0;) {
    int ret;
    //printf("XXX %s chunk_size = 0x%x\n", __FUNCTION__, chunk_sz);
    NSDL3_HTTP(NULL, NULL, "Decompressing chunk %d. chunk_sz = 0x%x", chunk_sz, chunk_sz);

    ret = ns_decomp_do_continue (body_chunk_extract, chunk_sz);

    if (ret == -1) {
      NSDL3_HTTP(NULL, NULL, "Error in decompressing for chunk_sz = 0x%x", chunk_sz);
      printf ("Error decompressing chunked body\n");
      //full_buffer = inp_buffer;
      while (chunk_size_list) {
        old = chunk_size_list;
        chunk_size_list = chunk_size_list->next;
        FREE_AND_MAKE_NOT_NULL(old, "old", -1);
      }
      vptr->bytes = vptr_bytes;
      vptr->chunk_size_head = NULL;
      return  inp_buffer_body;
    } 

    // Here either ret is 0 (stread is over) or 1 (expecting more data)

    { /* lib expecting more data or the case of Z_STREAM_END */
      if (total_size < cur_size + uncomp_cur_len) {
        //        printf("XXX Rellocing..\n");
        MY_REALLOC(full_buffer, total_size + uncomp_cur_len, "full_buffer", -1); /* Request failed - what do we do ??: BHAV */
        total_size += uncomp_cur_len;
      }
      body_chunk_extract += chunk_sz;
      memcpy(full_buffer + cur_size, uncomp_buf, uncomp_cur_len);
      cur_size += uncomp_cur_len;
      vptr->bytes += uncomp_cur_len;
      *blen = vptr->bytes;
    }
    
    old = chunk_size_list;
    chunk_size_list = chunk_size_list->next;
    if (chunk_size_list)
      chunk_sz = chunk_size_list->chunk_sz;
    else
      chunk_sz = 0;
    FREE_AND_MAKE_NOT_NULL(old, "old", -1);

    if (ret == 0 && chunk_size_list) { /* we already got a Z_STREAM_END but chunks
                                        * are still left
                                        */ 
      NSDL3_HTTP(NULL, NULL, "Body with bad data, containing more than one compressed streams");
      printf("Body with bad data, containing more than one compressed streams\n");
      while (chunk_size_list) {
        old = chunk_size_list;
        chunk_size_list = chunk_size_list->next;
        FREE_AND_MAKE_NOT_NULL(old, "old", -1);
      }
      vptr->bytes = vptr_bytes;
      vptr->chunk_size_head = NULL;
      return  inp_buffer_body;
    }
  }

  vptr->chunk_size_head = NULL;
  NSDL3_HTTP(NULL, NULL, "Method done");
  
/*   foo = fopen("/tmp/foobar", "w"); */
/*   fwrite(full_buffer, vptr->bytes, 1, foo); */
/*   fclose(foo); */
  return full_buffer;
#endif

}

//Serach for absolute path images
//return NULL, if not found
//returns ptr to / of abs path, if found and replaces / in <img src="/ with NULL char
static char * get_abs_image(char *contents, regex_t *ppreg)
{
char *ptr, *ptr1, *eptr;
regmatch_t match[1];

    NSDL2_HTTP(NULL, NULL, "Method called");
    ptr = contents;
    while (1) {
    	ptr = index (ptr, '<');
    	if (!ptr) return NULL;
    	ptr++;
	eptr = index (ptr, '<');
    	if (!eptr) return NULL;
    	while (isspace((int)(*ptr))) ptr++;
#if 0
	if (strncasecmp (ptr, "img", 3)) continue;
	ptr += 3;
    	while (isspace((int)(*ptr))) ptr++;
#endif
        if (regexec(ppreg, ptr, 1, match, 0)) continue;
        if (match[0].rm_so == -1) continue;
	ptr1 = ptr + match[0].rm_so;
	//ptr1 = strstr (ptr, "src");
	//if (!ptr1) continue;
	if (eptr < ptr1) continue;
	ptr = ptr1+3;
	//if (strncasecmp (ptr, "src", 3)) continue;
	//ptr += 3;
    	while (isspace((int)(*ptr))) ptr++;
	if (*ptr != '=') continue;
	ptr++;
    	while (isspace((int)(*ptr))) ptr++;
	if (*ptr == '"') ptr++;
	if (*ptr == '\'') ptr++;
    	while (isspace((int)(*ptr))) ptr++;
	if (*ptr != '/') continue;
	//*ptr = '\0';
	return ptr;
    }
}

//Writes the whole contenets
static int inline
write_all(int fd, char *buf, int len) {
int done =0;
int ret;

  	NSDL2_HTTP(NULL, NULL, "Method called");
	while (len > done) {
	    ret = write (fd, buf+done, len - done);
	    if (ret == -1)
		return -1;
	    done =+ ret;
	}
	return 0;
}

void 
//save_contents(char *lbuf, connection *cptr)
save_contents(char *lbuf, VUser *vptr, connection *cptr)
{
char *addr, *ustr, *ptr, *ptr1;
char dname[MAX_FILE_NAME];
char base[MAX_FILE_NAME];
char fname[MAX_FILE_NAME];
char buf[MAX_FILE_NAME];
char cbuf[MAX_FILE_NAME];
int fd, len;
static int first=1;
static regex_t srcreg;
int return_value;
char err_msg[1000];
char *ptr2;

    NSDL2_HTTP(NULL, NULL, "Method called, bytes=%d Contents=%s\n", (int)vptr->bytes, lbuf);

    if (!getcwd(buf, MAX_FILE_NAME)) {
        NS_EXIT(-1, "error in getting pwd");
    }
    //addr = inet_ntoa (vptr->sin_addr);
    addr = nslb_sock_ntop((struct sockaddr *) &(vptr->sin_addr));
    if (!addr) {
	printf("Current address conversion fails\n");
	return ;
    }
    sprintf(base, "%s/logs/TR%d/data/%s/",buf, testidx, addr);

    //if (globals.debug)
    NSDL1_MISC(NULL, NULL, "address is %s\n", addr);

#if 0
    sprintf(dname,"%s/logs/TR%d/data/%s",buf, testidx, addr);
    if (mkdir(dname, 0777) != 0) {
	perror("mkdir");
	printf("Unable to Create addr directory %s \n", dname);
	//return;
    }
#endif

    ustr = get_url_req_url(cptr);
    ptr = index (ustr, ' ');
    if (!ptr) {
	printf("url '%s' string format not OK (expected like 'GET URL HTTP/1.x')\n", ustr);
	return ;
    }
    ptr++;
    if (*ptr == '/') ptr++;

#if 0
    strncpy (ubuf, ptr, MAX_FILE_NAME-1);
    ubuf[MAX_FILE_NAME-1] = 0;
#endif

    ptr1 = index (ptr, ' ');
    if (!ptr1) {
	printf("url '%s' string format not OK (Expected, something like 'GET URL HTTP/1.x')\n", ptr);
	return ;
    }
    *ptr1 = 0;

    //ptr now points to whole URL, take just the base URL.upto ?
    ptr1 = index (ptr, '?');
    if (ptr1)  *ptr1 = '\0';

    //Setup dirname and filename to save
    ptr1 = rindex (ptr, '/');
    if (ptr1) {
	*ptr1 = '\0';
	ptr1++;
	sprintf(dname,"%s/logs/TR%d/data/%s/%s",buf, testidx, addr, ptr);
	sprintf(fname,"%s",ptr1);
    } else {
	sprintf(dname,"%s/logs/TR%d/data/%s",buf, testidx, addr);
	sprintf(fname,"%s",ptr);
    }
    if (fname[0] == '\0')
	strcpy(fname, "index.html");

    sprintf (buf, "mkdir -p %s", dname);

    //if (globals.debug)
    NSDL1_MISC(NULL, NULL, "making dir using cmd '%s'\n", buf);

    system(buf);
    sprintf(buf, "%s/%s", dname, fname);

    //if (globals.debug)
    NSDL1_MISC(NULL, NULL, "creating file '%s'\n", buf);


    //Start mozilla to display the contents.  After first time, content would be shown
    //in same window.Also, chnage the embedded object's URL path to saved data
    //Important for absolute paths. like img src = /tours/images/abc.gif
    if (first) {
        sprintf (cbuf, "mozilla &");
	system (cbuf);
	sleep(5);
	first = 0;
  	return_value = regcomp(&srcreg, "src", REG_EXTENDED|REG_ICASE);
  	if (return_value != 0) {
    	    regerror(return_value, &srcreg, err_msg, 1000);
    	    NS_EXIT(-1, "regcomp failed:%s", err_msg);
  	}
    }

    //if (globals.debug)
    NSDL1_MISC(NULL, NULL, "writing to  file '%s'\n", buf);

    fd = open (buf, O_CREAT|O_WRONLY|O_CLOEXEC, 00666);
    if (!fd) {
	printf("open for '%s' failed\n", buf);
	return ;
    }

    if (vptr->bytes)  {
	len = strlen (lbuf);
	//printf("lbuf len is %d\n", len);
	ptr = lbuf;
	while ((ptr2 = get_abs_image(ptr, &srcreg))) {
	    len = ptr2 - ptr;
	    //printf("writing  len is %d\n", len);
            write_all (fd, ptr, len);
	    ptr += len;
	    ptr++;
	    //printf("writing  base %s\n", base);
            write_all (fd, base, strlen(base));
  	}
	len = strlen(ptr);
	//printf("writing  final len is %d\n", len);
        write_all (fd, ptr, len);
        //ret =  write (fd, lbuf, strlen(lbuf));
    } else {
	sprintf(cbuf, "Page Status is = %s\n",  get_error_code_name(vptr->page_status));
        write (fd, cbuf, strlen(cbuf));
    }

    close(fd);

#if 0
    sprintf (interactive_buf, "%s %s >/tmp/nstmp; mv /tmp/nstmp %s", base, buf, buf);
    printf("Executing '%s'\n", interactive_buf);
    system (interactive_buf);
#endif
    //sprintf (interactive_buf, "sleep 5; mozilla -remote \"openFile(%s)\"", buf);

    sprintf (interactive_buf, "mozilla -remote \"openFile(%s)\"", buf);

    //if (globals.debug)
    NSDL1_MISC(NULL, NULL, "exiting save_contents\n");
}


char *url_get_body_msg(int *size)
{
  *size = resp_body_len;
  return resp_body;
}

char *get_reply_buffer(connection *cptr, int *blen, int present_depth, int free_buffer_flag)
{ 
  VUser *vptr = cptr->vptr;
  int i;
  int complete_buffers = *blen / COPY_BUFFER_LENGTH;
  int incomplete_buf_size = *blen % COPY_BUFFER_LENGTH;
  struct copy_buffer* buffer = vptr->buf_head;
  struct copy_buffer* old_buffer;
  static char inp_buffer[1]={'\0'};
  full_buffer= inp_buffer; //Just to keep null terminator
  //char* copy_cursor = inp_buffer;
  char* copy_cursor = url_resp_buff;
  int used_hdr_buf_len = 0; 
  int sess_id = -1;
  char save_url_rsp = 0;
  char err_msg[1024];
 
  NSDL2_HTTP(vptr, NULL, "Method called. vptr = %p, vptr->buff_head = %p, complete_buffers = %d, incomplete_buf_size = %d, blen = %d vptr->page_status=%d", vptr, buffer, complete_buffers, incomplete_buf_size, *blen, vptr->page_status);

  //ANIL - for HTTP redirects  with body, size me be non-zero but no dat asaved.
  if (!buffer) {
    //Bug fixed - 4167
    //For a URL resp code is 302 and not giving AUTO_REDIRECTION in scenario.
    //then blen was getting set but we did not has buffer as we dont save 302 response
    //so resetting blen to 0
    *blen = 0;
    /* In release 3.9.7 we are creating empty response body file, if reply buffer is empty or Confail
     * Purpose: In page dump we need empty response body file*/
#ifdef NS_DEBUG_ON
   NSDL2_HTTP(vptr, NULL, "cptr->url_num->request_type = %d", cptr->url_num->request_type);
   if(cptr->url_num->request_type == HTTP_REQUEST || 
       cptr->url_num->request_type == HTTPS_REQUEST || cptr->url_num->request_type == WS_REQUEST || cptr->url_num->request_type == WSS_REQUEST)
   if (present_depth == -1) //Manish: fix bug 2663
     debug_log_http_res_body(vptr, full_buffer, *blen);
#else
   /*Page dump, added for trace-on-failure 0*/
   LOG_HTTP_RES_BODY(cptr, vptr, full_buffer, *blen);
#endif

    /* Point header buffer to full_buffer */
    if(vptr->response_hdr && (vptr->cur_page->save_headers & SEARCH_IN_HEADER))
    {
      full_buffer = vptr->response_hdr->hdr_buffer;
      *blen = vptr->response_hdr->used_hdr_buf_len;
    }

    return full_buffer;
  }

  /* In Response we have body as well as headers, Copy header in url_resp_buff at start if save_headers flag is set for HEADER */
  if(url_resp_buff && (vptr->cur_page->save_headers & SEARCH_IN_HEADER))
  {
    NSDL3_HTTP(vptr, NULL, "copy header to url_resp_buff");
    memcpy(copy_cursor, vptr->response_hdr->hdr_buffer, vptr->response_hdr->used_hdr_buf_len);
    copy_cursor += vptr->response_hdr->used_hdr_buf_len;
    *blen += vptr->response_hdr->used_hdr_buf_len;
    used_hdr_buf_len = vptr->response_hdr->used_hdr_buf_len;
  }

  for (i = 0; i < complete_buffers; i++) {
    if (buffer && buffer->buffer) {
      memcpy(copy_cursor, buffer->buffer, COPY_BUFFER_LENGTH);
      copy_cursor += COPY_BUFFER_LENGTH;
      old_buffer = buffer;
      buffer = buffer->next;
      /* In case depth is given for the last URL, we enter this function twice.
       * Once when do_data_processing is called explicitly before
       * handle_page_complete and 2nd time when do_data_processing is called
       * from handle_page_complete. In this case, we must not free
       * the data in the first call since we re-calculate from cptr->buffers 
      */
     // if (!(present_depth != VAR_IGNORE_REDIRECTION_DEPTH && !vptr->urls_awaited))
      if(free_buffer_flag){   
        NSDL3_HTTP(vptr, NULL, "Freeing buffer = %p", old_buffer);
        FREE_AND_MAKE_NOT_NULL_EX (old_buffer, COPY_BUFFER_LENGTH, "Freeing old_buffer", i);
      }
    } else {
      //char cptr_to_str[MAX_LINE_LENGTH + 1];
      char vptr_to_str[MAX_LINE_LENGTH + 1];
      //cptr_to_string(cptr, cptr_to_str, MAX_LINE_LENGTH);
      vptr_to_string(vptr, vptr_to_str, MAX_LINE_LENGTH);
      
      error_log("%s|%d|buffer/buffer->buffer is null. vptr => %s ", 
                __FUNCTION__, __LINE__, vptr_to_str);
      break;
    }
  }

  if (incomplete_buf_size) {
    if (buffer && buffer->buffer) {
      memcpy(copy_cursor, buffer->buffer, incomplete_buf_size);
      /* In case depth is given for the last URL, we enter this function twice.
       * Once when do_data_processing is called explicitly before
       * handle_page_complete and 2nd time when do_data_processing is called
       * from handle_page_complete. In this case, we must not free
       * the data in the first call since we re-calculate from cptr->buffers 
      */
     // if (!(present_depth != VAR_IGNORE_REDIRECTION_DEPTH && !vptr->urls_awaited))
      if(free_buffer_flag)
        FREE_AND_MAKE_NOT_NULL_EX (buffer, incomplete_buf_size, "Freeing last incomplete buffer", -1);
    } else {
      //char cptr_to_str[MAX_LINE_LENGTH + 1];
      char vptr_to_str[MAX_LINE_LENGTH + 1];
      //cptr_to_string(cptr, cptr_to_str, MAX_LINE_LENGTH);
      vptr_to_string(vptr, vptr_to_str, MAX_LINE_LENGTH);
      
      error_log("%s|%d|buffer/buffer->buffer is null. vptr => %s ", 
                __FUNCTION__, __LINE__, vptr_to_str);
    }
  }

  // 02/08/07 Atul Add Code for Body
  char *inp_buffer_body = url_resp_buff + used_hdr_buf_len + vptr->body_offset; // Point to body part
  int body_size = *blen - used_hdr_buf_len - vptr->body_offset;
  NSDL3_HTTP(vptr, NULL, "offset = [%d] java mgr flag = [%d]  body_size = [%d] heax1= [%2x] hex2 = [%2x]",vptr->body_offset, global_settings->use_java_obj_mgr, body_size, (unsigned char)inp_buffer_body[0], (unsigned char)inp_buffer_body[1]);


  //For now, body_size and blen are same
  // we need to do decompression only for body
  if (*blen > 0) {
    if (vptr->compression_type && !(GRPC_REQ)){
       /* bug 93672: gRPC
       When per-RPC compression configuration isn't present for a message, the
       channel compression configuration MUST be used. Otherewise grpc-encoding 
       MUST be used.
      */
      if(vptr->compression_type && !(vptr->flags & NS_VPTR_GRPC_ENCODING)) {
        NSDL3_HTTP(vptr, NULL, "Compression type = %d, body_size = %d, *blen = %d, vptr->body_offset = %d", vptr->compression_type, body_size, *blen, vptr->body_offset);
        full_buffer = uncompress_reply_body(inp_buffer_body, body_size, vptr, blen, cptr);
        int url_resp_len = *blen + used_hdr_buf_len;
        if(url_resp_len > url_resp_size)
        {
          url_resp_size = url_resp_len;
          MY_REALLOC(url_resp_buff, url_resp_size, "url_resp_buff reallocation of memory",-1);
          inp_buffer_body =  url_resp_buff + used_hdr_buf_len + vptr->body_offset;
        }
        NSDL3_HTTP(vptr,NULL,"Full Buffer %s",full_buffer);
        memcpy(inp_buffer_body, full_buffer, *blen);
     
        full_buffer = url_resp_buff;
        *blen = url_resp_len; 
      }
    }
    else{
      full_buffer = url_resp_buff; 
    }

    //check for magic number 'aced'(network byte order) for java object
    if( (global_settings->use_java_obj_mgr) && (body_size >= 2) && ( ((unsigned char)full_buffer[0] == (unsigned char) 0xac) && ((unsigned char)full_buffer[1] == (unsigned char)0xed))){
      NSDL3_HTTP(vptr, NULL, "Setting flag NS_RESP_JAVA_OBJ");
      vptr->flags |= NS_RESP_JAVA_OBJ ;
      inp_buffer_body = full_buffer;
    } 
 
    if (vptr->flags & NS_RESP_AMF) {
      // If AMF, then convert to AMF XML 
      // TODO: It is assumed that AMF req will have AMF response.
      //       We need to check Content-Type: application/x-amf in response
      int inlen = *blen;
      NSDL3_HTTP(vptr, NULL, "Converting binary AMF to XML, inlen = %d", inlen);
      *blen = ns_amf_binary_to_xml(inp_buffer_body, &inlen);
      full_buffer = amf_asc_ptr; // This is a global variable which is used by ns_amf_binary_to_xml
    } else if (vptr->flags & NS_RESP_HESSIAN){
      if(hessian_buffer == NULL)
      {
        MY_MALLOC(hessian_buffer, HESSIAN_MAX_BUF_SIZE, "hessian_buffer", -1);
      }
      hessian_set_version(2);
      if((hessian2_decode(HESSIAN_MAX_BUF_SIZE, hessian_buffer, 0, inp_buffer_body, blen))){
        *blen = strlen(hessian_buffer);
        full_buffer = hessian_buffer;
      } else {
        fprintf(stderr, "Error in decoding hessian buffer, ignored. Original buffer will be used. Error = %s\n", nslb_strerror(errno));
        full_buffer = inp_buffer_body;
        hessian_buffer[0] = '\0';
      }
    }else if (vptr->flags & NS_RESP_JAVA_OBJ){ //case: JAVA object 
      int out_len = 0;
      int total_len = 0;
      u_ns_ts_t start_timestamp, end_timestamp, time_taken;
      NSDL3_HTTP(vptr, NULL, "Got Java Object in response");
      if(hessian_buffer == NULL)
      {
        MY_MALLOC(hessian_buffer, HESSIAN_MAX_BUF_SIZE, "hessian_buffer", -1);
      }
       
      if((total_len = create_java_obj_msg(1, inp_buffer_body, hessian_buffer, blen, &out_len, 2)) > 0) {
        start_timestamp = get_ms_stamp();
        if(send_java_obj_mgr_data(hessian_buffer, out_len, 1) != 0){
	  fprintf(stderr, "Error in sending data to java object manager.\n");
	  end_test_run(); 
        } 
 
        memset(hessian_buffer, 0, *blen);
 
        if(read_java_obj_msg(hessian_buffer, blen, 1) != 0){
	  fprintf(stderr, "Error in receiving data to java object manager.\n");
	  end_test_run(); 
        }
        //*blen = strlen(hessian_buffer);
        full_buffer = hessian_buffer;
        NSDL3_HTTP(vptr, NULL, "full_buffer = %*.*s", 1024, 1024, full_buffer);
      }else{
        fprintf(stderr, "Error in crating message data for java object manager.\n");
        end_test_run(); 
      }
      end_timestamp = get_ms_stamp();
      time_taken = end_timestamp - start_timestamp;
      NSDL3_HTTP(vptr, NULL, "Time taken = [%d]ms, Threshold = [%lld]ms", time_taken, global_settings->java_object_mgr_threshold);
      if (time_taken > global_settings->java_object_mgr_threshold)
        NS_DT1(vptr, NULL, DM_L1, MM_HTTP, "Time taken by Java object manager is exceeding threshold value. Time taken = [%d]ms, Threshold = [%lld]ms", time_taken, global_settings->java_object_mgr_threshold);

    } 
    else if((vptr->flags & NS_RESP_PROTOBUF) && (vptr->first_page_url->proto.http.protobuf_urlattr_shr.resp_message))
    {
      
     //TODO: Manish: need to make code comman as signle url can be decoded only by one type of decoding at a time
      if(hessian_buffer == NULL)
      {
        MY_MALLOC(hessian_buffer, HESSIAN_MAX_BUF_SIZE, "hessian_buffer", -1);
      }

      NSDL3_HTTP(vptr, NULL, "Decode protobuf: before decoding, resp_message = %p, body_size = %d",
                       vptr->first_page_url->proto.http.protobuf_urlattr_shr.resp_message, body_size);

      memcpy(hessian_buffer, url_resp_buff, used_hdr_buf_len);

      if((*blen = nslb_decode_protobuf(vptr->first_page_url->proto.http.protobuf_urlattr_shr.resp_message,
                              inp_buffer_body, body_size, hessian_buffer + used_hdr_buf_len, HESSIAN_MAX_BUF_SIZE)))
      {
        *blen += used_hdr_buf_len;
        full_buffer = hessian_buffer;
      }
      else
      {
        fprintf(stderr, "Error in decoding protobuf buffer, ignored. Original buffer will be used. Error = %s\n", nslb_strerror(errno));
        full_buffer = inp_buffer_body;
        hessian_buffer[0] = '\0';
      }
      NSDL3_HTTP(vptr, NULL, "After protobuf decoding: len = %d", *blen);
    }
    else if(GRPC_REQ && (vptr->flags & NS_VPTR_CONTENT_TYPE_GRPC)) 
    {
      NSDL3_HTTP(vptr, NULL, "Processs GRPC Response");
      char comp_flag; 
      int msg_len;
      
      comp_flag = inp_buffer_body[0];  //Copy Compression Flag 
      memcpy(&msg_len, &inp_buffer_body[1], 4); //Copy Message Len
      
      msg_len = ntohl(msg_len); //Convert Network to Host Byte Order
      inp_buffer_body += 5; //Shift Body buffer pointer by 5 (1 for Compression Flag, 4 for Message Len)
      body_size -= 5; //Decrese body size
      
      NSDL3_HTTP(vptr, NULL, "comp_flag = %c , msg_len = %d",comp_flag, msg_len);
      if(comp_flag && (vptr->flags & NS_VPTR_GRPC_ENCODING))    //check for compression flag 
      {
        NSDL3_HTTP(vptr, NULL, "decompress grpc body");
        nslb_decompress(inp_buffer_body, body_size, &ns_nvm_scratch_buf, (size_t *)&ns_nvm_scratch_buf_size, (size_t *)&body_size, vptr->compression_type, err_msg, 1024);
       
        //Set Buffer pointer to inp_buffer_body
        inp_buffer_body = ns_nvm_scratch_buf;
      }
 
      if(hessian_buffer == NULL)
      {
        MY_MALLOC(hessian_buffer, HESSIAN_MAX_BUF_SIZE, "hessian_buffer", -1);
      }
      memcpy(hessian_buffer, url_resp_buff, used_hdr_buf_len);

      //Process Body if it is encoded in protobuf format
      if(GRPC_PROTO_RESPONSE && (vptr->first_page_url->proto.http.protobuf_urlattr_shr.resp_message))
      {
        
        NSDL3_HTTP(vptr, NULL, "Decode protobuf: before decoding, resp_message = %p, body_size = %d",
                       vptr->first_page_url->proto.http.protobuf_urlattr_shr.resp_message, body_size);
 
        if((*blen = nslb_decode_protobuf(vptr->first_page_url->proto.http.protobuf_urlattr_shr.resp_message,
                              inp_buffer_body, body_size, hessian_buffer + used_hdr_buf_len, HESSIAN_MAX_BUF_SIZE)))
        {
          *blen += used_hdr_buf_len;
          full_buffer = hessian_buffer;
        }
        else
        {
          fprintf(stderr, "Error in decoding protobuf buffer, ignored. Original buffer will be used. Error = %s\n", nslb_strerror(errno));
          full_buffer = inp_buffer_body;
          hessian_buffer[0] = '\0';
        }

        NSDL3_HTTP(vptr, NULL, "After protobuf decoding: len = %d", *blen);
      }
      //Process other body format 
      else
      {
         memcpy(&hessian_buffer[used_hdr_buf_len], inp_buffer_body, body_size);
         *blen = used_hdr_buf_len + body_size;
         full_buffer = hessian_buffer;
      }
    }
    else if((IS_REQUEST_SSL_OR_NONSSL_SOCKET) && (vptr->cur_page->first_eurl->proto.socket.recv.msg_fmt.msg_enc_dec))
    {
      hessian_buffer = vptr->cur_page->first_eurl->proto.socket.enc_dec_cb(inp_buffer_body, body_size, blen);
      if(hessian_buffer && hessian_buffer[0])
      {
        full_buffer = hessian_buffer;
      }
      else
      {
        NSTL1(NULL, NULL, "Error in decoding Socket msg, ignored. Original buffer will be used");
        full_buffer = inp_buffer_body;
        if(hessian_buffer)
          hessian_buffer[0] = '\0';
      }
    }

    if(!*blen) *blen = used_hdr_buf_len;
    full_buffer[*blen] = '\0';
  }

  resp_body = full_buffer + used_hdr_buf_len;
  resp_body_len = *blen - used_hdr_buf_len; 

  NSDL3_HTTP(vptr, NULL, "resp_body_len = %d, resp_body = %s", resp_body_len, resp_body);

  #ifdef NS_DEBUG_ON
  if(cptr->url_num->request_type == HTTP_REQUEST || 
     cptr->url_num->request_type == HTTPS_REQUEST ||
     cptr->request_type == SOCKET_REQUEST || cptr->request_type == SSL_SOCKET_REQUEST)
     /*Excluding headers while dumpung in url_resp_body file, so we increase the full_buffer to vptr->response_hdr->used_hdr_buf_len*/
     if (present_depth == -1) {//Manish: fix bug 2663
       debug_log_http_res_body(vptr, resp_body, resp_body_len);
     }
  #else
   /*Page dump, added for trace-on-failure 0*/
   LOG_HTTP_RES_BODY(cptr, vptr, resp_body, resp_body_len)
  #endif

  /* Bug Fixed 1094: we should NULL following only if we have to free buffer Tuesday, September 14 2010 */
  if(free_buffer_flag)
  { 
    vptr->buf_head = NULL;
    vptr->cur_buf = NULL;
    vptr->chunk_size_head = NULL;
  }

  /* Getting SESSION ID to retrieve URL response */
  sess_id = runprof_table_shr_mem[vptr->group_num].sess_ptr->sess_id;
  save_url_rsp = session_table_shr_mem[sess_id].save_url_body_head_resp;
  if(save_url_rsp || ((IS_REQUEST_SSL_OR_NONSSL_SOCKET) && vptr->cur_page->first_eurl->proto.socket.recv.msg_fmt.msg_enc_dec))
  {
    int hdr_size = 0;
    if(!(vptr->cur_page->save_headers & SEARCH_IN_HEADER) && (save_url_rsp & SAVE_URL_HEADER)) 
    {
        hdr_size = vptr->response_hdr->used_hdr_buf_len;
    }
    if(vptr->url_resp_buff_size < (*blen + hdr_size))
    {
      NSLB_FREE_AND_MAKE_NOT_NULL(vptr->url_resp_buff, "url resp buffer", -1 , NULL);
      vptr->url_resp_buff_size = *blen + hdr_size;
      NSLB_MALLOC(vptr->url_resp_buff,  vptr->url_resp_buff_size + 1, "vptr->url_resp_buff", -1, NULL);
    }
    if(hdr_size)
      memcpy(vptr->url_resp_buff, vptr->response_hdr->hdr_buffer, hdr_size);
     
    memcpy(vptr->url_resp_buff + hdr_size, full_buffer, *blen);
    vptr->url_resp_buff[*blen + hdr_size] = '\0';
    vptr->bytes = resp_body_len;
  }

  return full_buffer;
}


/* This function checks if we need to execute next registeration API
   If one API fails the page, then
     if G_CONTINUE_ON_PAGE_ERROR is 1, then next API is executed (irrespectice of ActionOnFail)
     if G_CONTINUE_ON_PAGE_ERROR is 0, then next API is 
        - Executed if the Action of API is set as Continue
        - Not executed if the Action of API is set as Stop
   Assumptions:
     1. If continue_on_pg_err is set, then we execute all registration APIs.
     2. If one registration API fails and action is to continue, then it will execute next API.
   Returns Value:
     returns 0 on fail [next api will not be called].
     retruns 1 on success [next api will be called].
*/
static int inline
chk_if_next_reg_api_to_exec(VUser *vptr) 
{
   ContinueOnPageErrorTableEntry_Shr *ptr;
   ptr = (ContinueOnPageErrorTableEntry_Shr *)runprof_table_shr_mem[vptr->group_num].continue_onpage_error_table[vptr->cur_page->page_number];
   if (vptr->page_status != NS_REQUEST_OK) /* Page failed by previous API */ 
  { 
    if ((ptr->continue_error_value) 
       && (!(vptr->flags & NS_ACTION_ON_FAIL_CONTINUE))) 
    { 
      NSDL2_VARS(NULL, NULL, "vptr->flags is not set and continue on page error is 0, therefore not processing the check reply size var"); 
      return 0 ; 
    } 
  } 
  return 1;
}

// This method is called after complete page is retrieved (mail and all embedded URLs).
// But this method only processes the body of main URL.
void
do_data_processing( connection *cptr, u_ns_ts_t now, int present_depth, int free_buffer_flag) {

  VUser *vptr = cptr->vptr;
  int blen = vptr->bytes;      //Only body size
  int outlen=0; //for amf

  char *js_buffer = NULL;
  int js_buffer_len = 0;
  char *body_ptr = NULL;
  //int buffer_len_to_process = 0;
   /*JS related time stamp*/ 
   u_ns_ts_t lol_js_proc_time_start;
   u_ns_4B_t lol_js_proc_time;
   

  int page_status_offset = 0;
  int total_bytes_copied = 0;

  /*Total do data processing related time stamp*/
  u_ns_ts_t lol_proc_time_start;
  u_ns_4B_t lol_proc_time;
 
  NSDL2_HTTP(vptr, NULL, "Method called. present depth = %d, free_buffer_flag = %d, page status = %d request_type =[%d]", 
                          present_depth, free_buffer_flag, vptr->page_status, cptr->request_type);
  
  lol_proc_time_start = get_ms_stamp();
  /*Dont free linked list-
   *1. If resp from cache no free
   *2. First time and is to be cached
  */
  if(vptr->flags & NS_RESP_FRM_CACHING) 
  {
    free_buffer_flag = 0;
    NSDL2_HTTP(vptr, NULL, "This response is from cache, so not freeing it. response length = %d", blen);
  }

  NSDL2_HTTP(vptr, NULL, "Do data Processing: enable_rb = %d", runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_rbu);
  if(!runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_rbu) //For Normal script
  {
    // In case of ldap request we have decoded request in ldap_buffer
    if(cptr->request_type == LDAP_REQUEST || cptr->request_type == LDAPS_REQUEST){
      body_ptr = full_buffer = ldap_buffer;
      blen = full_buffer_len = ldap_buffer_len;
    } else if (cptr->request_type == JRMI_REQUEST){
      body_ptr = full_buffer = jrmi_buff;
      blen = full_buffer_len = jrmi_content_size;
    }else if (cptr->request_type == JNVM_JRMI_REQUEST){
      body_ptr = full_buffer = jrmiRepPointer;
      vptr->bytes = blen = full_buffer_len = jrmiRepLength;
    }else { 
      full_buffer = get_reply_buffer(cptr, &blen, present_depth, free_buffer_flag); //Just to keep null terminator
      full_buffer_len  = blen; //For Search Parameter
      NSDL2_HTTP(NULL, NULL, "save_headers = %d, full_buffer_len = %d, full_buffer = %s, used_hdr_buf_len = %d",
                              vptr->cur_page->save_headers, full_buffer_len, full_buffer, 
                              vptr->response_hdr?vptr->response_hdr->used_hdr_buf_len:0);
      /*If SEARCH=ALL in search parameter then buffer_to_proc points to the response body, as full_buffer 
        contains header as well as body*/
      if(vptr->response_hdr && (vptr->cur_page->save_headers & SEARCH_IN_HEADER)){
        body_ptr = full_buffer + vptr->response_hdr->used_hdr_buf_len; 
        blen -= vptr->response_hdr->used_hdr_buf_len;
      }
      else
      {
        body_ptr = full_buffer;
      }
    }
  }
  else if (vptr->httpData->rbu_resp_attr->resp_body) // For RBU script
  {
    NSDL2_HTTP(vptr, NULL, "Fill full_buffer for Page Dump: full_buffer = %p, blen = %d", 
                            vptr->httpData->rbu_resp_attr->resp_body, vptr->httpData->rbu_resp_attr->resp_body_size);
    //Call har file parese and set full_buffer
    full_buffer = vptr->httpData->rbu_resp_attr->resp_body;
    blen = full_buffer_len = vptr->httpData->rbu_resp_attr->resp_body_size;
    body_ptr = full_buffer;
  }
  else
  {
    NSDL2_HTTP(vptr, NULL, "full_buffer Not set so returing from here");
    return ;
  }
  
  if(NS_IF_TRACING_ENABLE_FOR_USER || NS_IF_PAGE_DUMP_ENABLE_WITH_TRACE_ON_FAIL)
    ut_update_rep_body_file(vptr, blen, body_ptr);

  NSDL2_HTTP(vptr, NULL, "full_buffer = %s, length of full_buffer = %d", full_buffer, blen);
  //KNQ: if main page url is bad, why are you parsig the response?
  //if ((cptr->req_ok == REQ_OK) || ((globals.continue_on_pg_err == 1)))
  //TODO: Should check for 200 HTTP code too
  //if (cptr->req_ok == REQ_OK)
  //Either the page status is success or error code is more than 8.
  //Page Error code is either embedded object failure or some Content Verification failure

  /* Fix done for bug 5547, we will be dumping page dump data before processing variables.
   * Issue: Parameter substitution for search parameter was incorrect in page dump.
   * Consider case of a script having 2 pages where search parameter was applied on both the pages and used in URL/header/body. 
   * Page dump of first page should not show parameter substitution of variables whereas in second page 
   * parameter value of vars should be of first page */

  /* This function is used in page snap shot . 
     Old code has been removed from here and put in to make_page_dump_buff() function 
     get_parameters() and log_segment() function removed from ns_url_resp.c
     and moved to ns_trace_log.c
  */
  if(cptr->url_num->request_type == HTTP_REQUEST ||
     cptr->url_num->request_type == HTTPS_REQUEST ||
     cptr->url_num->request_type == JRMI_REQUEST ||
     cptr->url_num->request_type == JNVM_JRMI_REQUEST ||
     cptr->url_num->request_type == LDAP_REQUEST || cptr->url_num->request_type == LDAPS_REQUEST ||
     (cptr->url_num->request_type == WS_REQUEST || cptr->url_num->request_type == WSS_REQUEST) || 
     (cptr->url_num->request_type == SOCKET_REQUEST || cptr->url_num->request_type == SSL_SOCKET_REQUEST))
  {
    if(present_depth == VAR_IGNORE_REDIRECTION_DEPTH){  // Whole page is complete the call
     /* RTC Changes: For long session, if runtime change applied then following cases need to be handle.
      * In case of runtime change from (higher lvl (2,3,4)) to trace level 0 and 1, or
      * in case of trace-on-failure, all sessions
      * Then we need to free vptr nodes and reset flag in case of page dump enable*/
      if ((NS_IF_PAGE_DUMP_ENABLE && ((runprof_table_shr_mem[vptr->group_num].gset.trace_level == TRACE_URL_DETAIL) || (runprof_table_shr_mem[vptr->group_num].gset.trace_level == TRACE_DISABLE))) || (runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_ALL_SESS || 
          runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_PAGE_ON_HEADER_BASED))
      {
         NSDL2_LOGGING (vptr, NULL, "Need to reset flags");  
         free_nodes (vptr);  
         if (runprof_table_shr_mem[vptr->group_num].gset.trace_level == TRACE_URL_DETAIL)
           vptr->flags |= NS_PAGE_DUMP_ENABLE;
      }
     /* Need to verify whether tracing enable or not, trace_on_failure is 0 need to dump all interactions
      * For trace level 1, url information will be logged from funct on_url_done, hence we dont need to 
      * follow below path */ 
       NSDL2_LOGGING (vptr, NULL, "------ NS_IF_PAGE_DUMP_ENABLE = %0x, gset.trace_on_fail = %d, gset.trace_level = %d\n", 
                         vptr->flags & NS_PAGE_DUMP_ENABLE, runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail, 
                         runprof_table_shr_mem[vptr->group_num].gset.trace_level);
      if(NS_IF_PAGE_DUMP_ENABLE && ((runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_ALL_SESS) || 
                                    (runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_PAGE_ON_HEADER_BASED)) && 
                                  (runprof_table_shr_mem[vptr->group_num].gset.trace_level > TRACE_URL_DETAIL))
        make_page_dump_buff(cptr, vptr, now, blen, &page_status_offset, &total_bytes_copied);
      if(NS_IF_TRACING_ENABLE_FOR_USER || NS_IF_PAGE_DUMP_ENABLE_WITH_TRACE_ON_FAIL){
        ut_add_param_used_node (vptr);
      }
      free_trace_up_t(vptr); 
    }
  }


  if (((!(vptr->page_status)) || (vptr->page_status >= NS_REQUEST_MIN_VALID))  && 
                                 (vptr->page_status != NS_REQUEST_RELOAD))
  {
    /*JAVA Script Engine --*/
    // Process JavaScript only for the final response after complete page is loaded
    if(present_depth == VAR_IGNORE_REDIRECTION_DEPTH) 
    { 
   
      lol_js_proc_time_start = get_ms_stamp();
      if(runprof_table_shr_mem[vptr->group_num].gset.js_mode != NS_JS_DISABLE && 
                        (!(vptr->urls_awaited) && (cptr->url_num->request_type == HTTP_REQUEST || 
                           cptr->url_num->request_type == HTTPS_REQUEST))) {
         
        js_buffer = process_buffer_in_js(cptr, body_ptr, blen, &js_buffer_len);
      }

      NSDL2_HTTP(vptr, NULL, "js_buffer = %s", js_buffer);

      lol_js_proc_time =  (u_ns_4B_t )(get_ms_stamp() - lol_js_proc_time_start);
      average_time->page_js_proc_time_tot += lol_js_proc_time;

      if(lol_js_proc_time < average_time->page_js_proc_time_min)
        average_time->page_js_proc_time_min = lol_js_proc_time;
      if(lol_js_proc_time > average_time->page_js_proc_time_max) 
        average_time->page_js_proc_time_max = lol_js_proc_time;
      if(SHOW_GRP_DATA) {
        avgtime *lol_average_time;
        lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
        lol_average_time->page_js_proc_time_tot += lol_js_proc_time;
        if(lol_js_proc_time < lol_average_time->page_js_proc_time_min)
          lol_average_time->page_js_proc_time_min = lol_js_proc_time;
        if(lol_js_proc_time > lol_average_time->page_js_proc_time_max)
          lol_average_time->page_js_proc_time_max = lol_js_proc_time;
      }
      // We need to min and max and tot_time
      NSDL1_HTTP(vptr, NULL, "Time taken in JavaScript execution = %'.3f seconds, min = %'.3f, max = %'.3f, Total time =  %'.3f", (double )lol_js_proc_time/1000.0, (double )average_time->page_js_proc_time_min/1000.0, (double )average_time->page_js_proc_time_max/1000.0, (double)average_time->page_js_proc_time_tot/1000.0);
    }

    if (js_buffer && runprof_table_shr_mem[vptr->group_num].gset.js_mode != NS_JS_DO_NOT_CHECK_POINT) {
      NSDL2_HTTP(vptr, NULL, "Processing will be done on java script"
			     " response buffer, js_buffer = %p",
                             js_buffer);
      body_ptr = js_buffer;
      blen = js_buffer_len;
    }
#ifndef RMI_MODE
    memset(vptr->order_table, 0, user_order_table_size);
#endif

    // outlen is set by one of following functions if any Vars/API defined for that

    /*if depth is not equal to last[-1] then call only the searchvar and return
    * else do all the processing 
    * here we are processing the RedirectionDepth=ALL and RedirectionDepth=1,2,3,..n
    */
    if(present_depth != VAR_IGNORE_REDIRECTION_DEPTH) 
    {
      process_search_vars_from_url_resp(cptr, vptr, full_buffer, full_buffer_len, present_depth);
      if(chk_if_next_reg_api_to_exec(vptr))
        process_tag_vars_from_url_resp(vptr, body_ptr, blen, present_depth);

      //For JSON Var
      process_json_vars_from_url_resp(cptr, vptr, body_ptr, blen, present_depth);
        
      return;
    }
 
    process_tag_vars_from_url_resp(vptr, body_ptr, blen, present_depth);

    // Anil - Can we move this to common place after if as we are also doing in else case
    // Also process_tag_vars_from_url_resp() modifies full_buffer. Do it need to saved after it.
    if (global_settings->interactive)
      save_contents(body_ptr, vptr, cptr);  // Can not DO 

    /*Here we are processing only RedirectionDepth=Last */
    process_search_vars_from_url_resp(cptr, vptr, full_buffer, full_buffer_len, present_depth);
    //For JSON Var
    process_json_vars_from_url_resp(cptr, vptr, body_ptr, blen, present_depth);

    //set env for ns_eval_string
    TLS_SET_VPTR(vptr);

    if(chk_if_next_reg_api_to_exec(vptr))
      process_checkpoint_vars_from_url_resp(cptr, vptr, full_buffer, full_buffer_len, &outlen);

    if(chk_if_next_reg_api_to_exec(vptr))
      process_check_replysize_vars_from_url_resp(cptr, vptr, body_ptr, &outlen);

  } else if (global_settings->interactive) {
      NSDL2_HTTP(vptr, NULL, "Not verifying/checking and processing the vars due to bad page error");
      save_contents(body_ptr, vptr, cptr);   // Can not DO
  }

  if(cptr->url_num->request_type == HTTP_REQUEST ||
     cptr->url_num->request_type == HTTPS_REQUEST ||
     cptr->url_num->request_type == JRMI_REQUEST ||
     cptr->url_num->request_type == JNVM_JRMI_REQUEST ||
     cptr->url_num->request_type == LDAP_REQUEST || cptr->url_num->request_type == LDAPS_REQUEST || 
     (cptr->url_num->request_type == WS_REQUEST || cptr->url_num->request_type == WSS_REQUEST) || 
     (cptr->url_num->request_type == SOCKET_REQUEST || cptr->url_num->request_type == SSL_SOCKET_REQUEST))
  {
    if(present_depth == VAR_IGNORE_REDIRECTION_DEPTH){  // Whole page is complete the call
     /* Need to verify whether tracing enable or not, trace_on_failure is 0 need to dump all interactions
      * For trace level 1, url information will be logged from funct on_url_done, hence we dont need to 
      * follow below path */ 
      if(NS_IF_PAGE_DUMP_ENABLE && (runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_ALL_SESS || runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_PAGE_ON_HEADER_BASED) && (runprof_table_shr_mem[vptr->group_num].gset.trace_level > TRACE_URL_DETAIL))
        do_trace_log(cptr, vptr, blen, ns_nvm_scratch_buf_trace, total_bytes_copied, page_status_offset, now);
    }
  }

  if(present_depth == VAR_IGNORE_REDIRECTION_DEPTH) 
  {
    XML_FREE_JS_DOM_BUFFER(cptr, js_buffer);
    /*We will take time only for last response*/
    lol_proc_time = (u_ns_4B_t)(get_ms_stamp() - lol_proc_time_start);
    average_time->page_proc_time_tot += lol_proc_time;
    if(lol_js_proc_time < average_time->page_proc_time_min)
      average_time->page_proc_time_min = lol_proc_time;
    if(lol_proc_time > average_time->page_proc_time_max)
      average_time->page_proc_time_max = lol_proc_time;
    if(SHOW_GRP_DATA) {
      avgtime *lol_average_time;
      lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
      lol_average_time->page_proc_time_tot += lol_proc_time;
      if(lol_js_proc_time < lol_average_time->page_proc_time_min)
        lol_average_time->page_proc_time_min = lol_proc_time;
      if(lol_js_proc_time > lol_average_time->page_proc_time_max)
        lol_average_time->page_proc_time_max = lol_proc_time;
    }
    // We need to min and max and tot_time
    NSDL1_HTTP(vptr, NULL, "Time taken in processing of page response = %'.3f seconds, min = %'.3f, max= %'.3f, total = %'.3f,", (double )lol_proc_time/1000.0, (double )average_time->page_proc_time_min/1000.0, (double )average_time->page_proc_time_max/1000.0, (double )average_time->page_proc_time_tot/1000.0);
  }

}

