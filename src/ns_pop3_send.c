/**
 * File: ns_pop3_send.c
 * Purpose: POP3 sending data functions
 * Author: Bhavpreet
 * 
 */

#include <stdio.h>
#include <string.h>
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
#include "ns_pop3.h"
//#include "ns_handle_read.h"
#include "ns_pop3_parse.h"
#include "ns_pop3_send.h"
#include "nslb_util.h"
#include "ns_log_req_rep.h"
#include "ns_ssl.h"
#include "ns_group_data.h"
#include "nslb_cav_conf.h"

/* Purpose: This method is to write debug url request file
   Arguments:
     cptr         - connection pointer
     buf          - data(ssl/non ssl)
     bytes_to_log - how many bytes to write
*/
void debug_log_pop3_req(connection *cptr, char *buf, int bytes_to_log, int complete_data, int first_trace_write_flag)
{
  VUser* vptr = cptr->vptr;
  int request_type = cptr->url_num->request_type;

  if (!((runprof_table_shr_mem[vptr->group_num].gset.debug & DM_LOGIC4) && 
        ((request_type == POP3_REQUEST || request_type == SPOP3_REQUEST) &&
         (runprof_table_shr_mem[vptr->group_num].gset.module_mask & MM_POP3))))
    return;

  if((global_settings->replay_mode)) return;

  {
    NSDL3_HTTP(vptr, cptr, "Method called. bytes_size = %d", bytes_to_log);
    char log_file[4096] = "\0";
    FILE *log_fp;
    char line_break[] = "\n------------------------------------------------------------\n";
    //Need to check if buf is null since following error is coming when try to write null
    //Error: Can not write to url request file. err = Operation now in progress, bytes_to_log = 0, buf = (null)
    //also check if bytes_to_log is 0, it possible when buf = ""
    if((buf == NULL) || (bytes_to_log == 0)) return;  
    
    SAVE_REQ_REP_FILES
    sprintf(log_file, "%s/logs/%s/pop3_session_%hd_%u_%u_%d_0_%d_%d_%d_0.dat",
          g_ns_wdir, req_rep_file_path, child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,
          vptr->group_num, GET_SESS_ID_BY_NAME(vptr),
          GET_PAGE_ID_BY_NAME(vptr));

    // Do not change the debug trace message as it is parsed by GUI
    if(first_trace_write_flag)
      NS_DT4(vptr, cptr, DM_L1, MM_POP3, "Request is in file '%s'", log_file);

    log_fp = fopen(log_file, "a+");
    if (log_fp == NULL)
    {
      fprintf(stderr, "Unable to open file %s. err = %s\n", log_file, nslb_strerror(errno));
      return;
    }

    //write for both ssl and non ssl url
    if(fwrite(buf, bytes_to_log, 1, log_fp) != 1)
    {
      fprintf(stderr, "Error: Can not write to url request file. err = %s, bytes_to_log = %d, buf = %s\n", nslb_strerror(errno), bytes_to_log, buf);
      return;
    }
    if (complete_data) fwrite(line_break, strlen(line_break), 1, log_fp);

    if(fclose(log_fp) != 0)
    {
      fprintf(stderr, "Unable to close url request file. err = %s\n", nslb_strerror(errno));
      return;
    }
  }
}

/* Functions to send messages to server */
//This method to send pop3 request

static void send_pop3_req(connection *cptr, int pop3_size, u_ns_ts_t now)
{
  int bytes_sent;
  VUser *vptr = cptr->vptr;

  NSDL2_POP3(NULL, cptr, "cptr=%p conn state=%d, proto_state = %d, pop3_size = %d, now = %u", cptr, cptr->conn_state, cptr->proto_state, pop3_size, now);

  if((bytes_sent = writev(cptr->conn_fd, g_scratch_io_vector.vector, g_scratch_io_vector.cur_idx)) < 0)
  {
    perror( "writev");
    printf("fd=%d num_vectors=%d \n", cptr->conn_fd, g_scratch_io_vector.cur_idx);
    NS_FREE_RESET_IOVEC(g_scratch_io_vector);

    fprintf(stderr, "sending pop3 request failed\n");
    perror("POP3 send error");

    retry_connection(cptr, now, NS_REQUEST_WRITE_FAIL);
    return;
  }
  //printf ("byte send = %d\n", bytes_sent);
  if(set_socketopt_for_quickack(cptr->conn_fd, 1) < 0)
  {
    retry_connection(cptr, now, NS_REQUEST_CONFAIL);
    return;
  }
  if (bytes_sent < pop3_size )
  {
    handle_incomplete_write(cptr, &g_scratch_io_vector, g_scratch_io_vector.cur_idx, pop3_size, bytes_sent);
    return;
  }

#ifdef NS_DEBUG_ON
  // Complete data send, so log all vectors in req file
  int i;
  for(i = 0; i < g_scratch_io_vector.cur_idx; i++)
  {
    if(i == g_scratch_io_vector.cur_idx - 1)
      debug_log_pop3_req(cptr, g_scratch_io_vector.vector[i].iov_base, g_scratch_io_vector.vector[i].iov_len, 0, 1);
    else debug_log_pop3_req(cptr, g_scratch_io_vector.vector[i].iov_base, g_scratch_io_vector.vector[i].iov_len, 0, 0);
     
  }
#endif
  if (cptr->conn_state == CNST_REUSE_CON)
    cptr->req_code_filled = -2;

  NS_FREE_RESET_IOVEC(g_scratch_io_vector);

  /* cptr->conn_state for pop3 will be set to reading in on_request_write_done() */
  if (cptr->url_num->proto.http.exact)
    SetSockMinBytes(cptr->conn_fd, cptr->url_num->proto.http.bytes_to_recv);
  cptr->tcp_bytes_sent = bytes_sent;
  INC_POP3_SPOP3_TX_BYTES_COUNTER(vptr, bytes_sent);
  on_request_write_done (cptr);
}

void pop3_send_user(connection *cptr, u_ns_ts_t now)
{
  int i, ret;
  int pop3_size = 0;

  NSDL2_POP3(NULL, cptr, "cptr=%p conn state=%d, now = %u", cptr, cptr->conn_state, now);

  /* make request */
  NS_FILL_IOVEC(g_scratch_io_vector, POP3_CMD_USER, strlen(POP3_CMD_USER));

  /* put user name */
  /*Bug :: 18710  A coreDump was generated due to this debug, reason being that seg_ptr will no longer point to str_ptr in case of file
   parameter .  The structure as per new design is using fparam_hash_code  instead of Pointer table . Hence commenting debug here */

  ret = insert_segments(cptr->vptr, cptr, &(cptr->url_num->proto.pop3.user_id), &g_scratch_io_vector, NULL, 0, 1, 1, cptr->url_num, SEG_IS_NOT_REPEAT_BLOCK);

   if(ret == -2)
     return;

  /* append CRLF */
  NS_FILL_IOVEC(g_scratch_io_vector, POP3_CMD_CRLF, strlen(POP3_CMD_CRLF));

  for(i = 0; i < g_scratch_io_vector.cur_idx; i++) {
    pop3_size += g_scratch_io_vector.vector[i].iov_len;
  }

  cptr->proto_state = ST_POP3_USER; /* Next reading state */

  if(cptr->request_type == SPOP3_REQUEST){
    copy_request_into_buffer(cptr, pop3_size, &g_scratch_io_vector);
    if(set_socketopt_for_quickack(cptr->conn_fd, 1) < 0)
    {
       retry_connection(cptr, now, NS_REQUEST_CONFAIL);
       return;
    }
    handle_pop3_ssl_write (cptr, now);
  }else{
    send_pop3_req(cptr, pop3_size, now);
  }
}

void pop3_send_stls(connection *cptr, u_ns_ts_t now)
{
  int i;
  int pop3_size = 0;

  NSDL2_POP3(NULL, cptr, "cptr=%p conn state=%d, now = %u", cptr, cptr->conn_state, now);

  /* make request */
  NS_FILL_IOVEC(g_scratch_io_vector, POP3_CMD_STLS, strlen(POP3_CMD_STLS));

  for(i = 0; i < g_scratch_io_vector.cur_idx; i++) {
    pop3_size += g_scratch_io_vector.vector[i].iov_len;
  }

  cptr->proto_state = ST_POP3_STLS; /* Next reading state */

  if(cptr->request_type == SPOP3_REQUEST){
    copy_request_into_buffer(cptr, pop3_size, &g_scratch_io_vector);
    if(set_socketopt_for_quickack(cptr->conn_fd, 1) < 0)
    {
       retry_connection(cptr, now, NS_REQUEST_CONFAIL);
       return;
    }
    handle_pop3_ssl_write(cptr, now);
  }else{
    send_pop3_req(cptr, pop3_size, now);
  }
}

void pop3_send_pass(connection *cptr, u_ns_ts_t now)
{
  int i, ret;
  int pop3_size = 0;

  NSDL2_POP3(NULL, cptr, "cptr=%p conn state=%d, now = %u", cptr, cptr->conn_state, now);

  /* make request */
  NS_FILL_IOVEC(g_scratch_io_vector, POP3_CMD_PASS, strlen(POP3_CMD_PASS));

  /* put user name */
  ret = insert_segments(cptr->vptr, cptr, &(cptr->url_num->proto.pop3.passwd), &g_scratch_io_vector, NULL, 0, 1, 1, cptr->url_num, SEG_IS_NOT_REPEAT_BLOCK);

   if(ret == -2)
     return;

  /* append CRLF */
  NS_FILL_IOVEC(g_scratch_io_vector, POP3_CMD_CRLF, strlen(POP3_CMD_CRLF));

  for(i = 0; i < g_scratch_io_vector.cur_idx; i++) {
    pop3_size += g_scratch_io_vector.vector[i].iov_len;
  }

  cptr->proto_state = ST_POP3_PASS; /* Next reading state */

  if(cptr->request_type == SPOP3_REQUEST){
    copy_request_into_buffer(cptr, pop3_size, &g_scratch_io_vector);
    if(set_socketopt_for_quickack(cptr->conn_fd, 1) < 0)
    {
       retry_connection(cptr, now, NS_REQUEST_CONFAIL);
       return;
    }
    handle_pop3_ssl_write (cptr, now);
  }else{
    send_pop3_req(cptr, pop3_size, now);
  }
}

void pop3_send_stat(connection *cptr, u_ns_ts_t now)
{
  int i;
  int pop3_size = 0;

  NSDL2_POP3(NULL, cptr, "cptr=%p conn state=%d, now = %u", cptr, cptr->conn_state, now);

  /* make request */
  NS_FILL_IOVEC(g_scratch_io_vector, POP3_CMD_STAT, strlen(POP3_CMD_STAT));

  for(i = 0; i < g_scratch_io_vector.cur_idx; i++) {
    pop3_size += g_scratch_io_vector.vector[i].iov_len;
  }

  NS_DT2(NULL, cptr, DM_L1, MM_POP3, "Sending 'STAT' to Server.");

  cptr->proto_state = ST_POP3_STAT; /* Next reading state */

  if(cptr->request_type == SPOP3_REQUEST){
    copy_request_into_buffer(cptr, pop3_size, &g_scratch_io_vector);
    if(set_socketopt_for_quickack(cptr->conn_fd, 1) < 0)
    {
       retry_connection(cptr, now, NS_REQUEST_CONFAIL);
       return;
    }
    handle_pop3_ssl_write (cptr, now);
  }else{
    send_pop3_req(cptr, pop3_size, now);
  }
}

void pop3_send_list(connection *cptr, u_ns_ts_t now)
{
  int i;
  int pop3_size = 0;

  NSDL2_POP3(NULL, cptr, "cptr=%p conn state=%d, now = %u", cptr, cptr->conn_state, now);

  /* make request */
  NS_FILL_IOVEC(g_scratch_io_vector, POP3_CMD_LIST, strlen(POP3_CMD_LIST));

  for(i = 0; i < g_scratch_io_vector.cur_idx; i++) {
    pop3_size += g_scratch_io_vector.vector[i].iov_len;
  }

  NS_DT2(NULL, cptr, DM_L1, MM_POP3, "Sending 'LIST' to Server.");

  cptr->proto_state = ST_POP3_LIST; /* Next reading state */

  if(cptr->request_type == SPOP3_REQUEST){
    copy_request_into_buffer(cptr, pop3_size, &g_scratch_io_vector);
    if(set_socketopt_for_quickack(cptr->conn_fd, 1) < 0)
    {
       retry_connection(cptr, now, NS_REQUEST_CONFAIL);
       return;
    }
    handle_pop3_ssl_write (cptr, now);
  }else{
    send_pop3_req(cptr, pop3_size, now);
  }
}

void pop3_send_retr(connection *cptr, u_ns_ts_t now)
{
  int i;
  int pop3_size = 0;

  NSDL2_POP3(NULL, cptr, "cptr=%p conn state=%d, now = %u", cptr, cptr->conn_state, now);

  /* make request */
  NS_FILL_IOVEC(g_scratch_io_vector, POP3_CMD_RETR, strlen(POP3_CMD_RETR));

  /* get next to fetch from "scan listing" */
  char *ptr = pop3_get_next_from_scan_listing(cptr);
  int len = strlen(ptr);
  NS_FILL_IOVEC(g_scratch_io_vector, ptr, len);

  NS_FILL_IOVEC(g_scratch_io_vector, POP3_CMD_CRLF, strlen(POP3_CMD_CRLF));

  for(i = 0; i < g_scratch_io_vector.cur_idx; i++) {
    pop3_size += g_scratch_io_vector.vector[i].iov_len;
  }

  cptr->proto_state = ST_POP3_RETR; /* Next reading state */

  if(cptr->request_type == SPOP3_REQUEST){
    copy_request_into_buffer(cptr, pop3_size, &g_scratch_io_vector);
    if(set_socketopt_for_quickack(cptr->conn_fd, 1) < 0)
    {
       retry_connection(cptr, now, NS_REQUEST_CONFAIL);
       return;
    }
    handle_pop3_ssl_write (cptr, now);
  }else{
    send_pop3_req(cptr, pop3_size, now);
  }
}

void pop3_send_dele(connection *cptr, u_ns_ts_t now)
{
  int i;
  int pop3_size = 0;

  NSDL2_POP3(NULL, cptr, "cptr=%p conn state=%d, now = %u", cptr, cptr->conn_state, now);

  /* make request */
  NS_FILL_IOVEC(g_scratch_io_vector, POP3_CMD_DELE, strlen(POP3_CMD_DELE));

  /* get next to fetch from "scan listing" */
  char *ptr = pop3_get_last_fetched_from_scan_listing(cptr);
  int len =  strlen(ptr);
  NS_FILL_IOVEC(g_scratch_io_vector, ptr, len);

  NS_FILL_IOVEC(g_scratch_io_vector, POP3_CMD_CRLF, strlen(POP3_CMD_CRLF));

  for(i = 0; i < g_scratch_io_vector.cur_idx; i++) {
    pop3_size += g_scratch_io_vector.vector[i].iov_len;
  }

  cptr->proto_state = ST_POP3_DELE; /* Next reading state */

  if(cptr->request_type == SPOP3_REQUEST){
    copy_request_into_buffer(cptr, pop3_size, &g_scratch_io_vector);
    if(set_socketopt_for_quickack(cptr->conn_fd, 1) < 0)
    {
       retry_connection(cptr, now, NS_REQUEST_CONFAIL);
       return;
    }
    handle_pop3_ssl_write (cptr, now);
  }else{
    send_pop3_req(cptr, pop3_size, now);
  }
}

void pop3_send_quit(connection *cptr, u_ns_ts_t now)
{
  int i;
  int pop3_size = 0;

  NSDL2_POP3(NULL, cptr, "cptr=%p conn state=%d, now = %u", cptr, cptr->conn_state, now);

  /* make request */
  NS_FILL_IOVEC(g_scratch_io_vector, POP3_CMD_QUIT, strlen(POP3_CMD_QUIT));

  for(i = 0; i < g_scratch_io_vector.cur_idx; i++) {
    pop3_size += g_scratch_io_vector.vector[i].iov_len;
  }

  NS_DT2(NULL, cptr, DM_L1, MM_POP3, "Sending 'QUIT' to Server.");

  cptr->proto_state = ST_POP3_QUIT; /* Next reading state */

  /* send EHLO */
  if(cptr->request_type == SPOP3_REQUEST){
    copy_request_into_buffer(cptr, pop3_size, &g_scratch_io_vector);
    if(set_socketopt_for_quickack(cptr->conn_fd, 1) < 0)
    {
       retry_connection(cptr, now, NS_REQUEST_CONFAIL);
       return;
    }
    handle_pop3_ssl_write (cptr, now);
  }else{
    send_pop3_req(cptr, pop3_size, now);
  }
}

void add_pop3_timeout_timer(connection *cptr, u_ns_ts_t now) {
 
  int timeout_val;
  ClientData client_data;
  VUser *vptr;

  client_data.p = cptr; 
  vptr = cptr->vptr;

  NSDL2_POP3(NULL, cptr, "Method Called, cptr=%p conn state=%d", cptr, cptr->conn_state);
  
  timeout_val = runprof_table_shr_mem[vptr->group_num].gset.pop3_timeout; 

  NSDL3_POP3(NULL, cptr, "timeout_val = %d", timeout_val);

  cptr->timer_ptr->actual_timeout = timeout_val;
  dis_timer_add(AB_TIMEOUT_IDLE, cptr->timer_ptr, now, pop3_timeout_handle, client_data, 0 );
}
