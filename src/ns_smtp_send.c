/*  */
#include <stdio.h>
#include <stdlib.h>
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
#include "ns_smtp.h"
#include "ns_smtp_send.h"
//#include "ns_handle_read.h"
#include "ns_smtp_parse.h"
#include "init_cav.h"
#include "poi.h"
#include "ns_auto_fetch_embd.h"
#include "nslb_util.h"
#include "ns_log_req_rep.h"
#include "ns_group_data.h"
#include "nslb_cav_conf.h"

void smtp_timeout_handle( ClientData client_data, u_ns_ts_t now ) {

  connection* cptr;
  cptr = (connection *)client_data.p;
  VUser* vptr = cptr->vptr;
  
  NSDL2_SMTP(vptr, cptr, "Method Called, vptr=%p cptr=%p conn state=%d", vptr, cptr, cptr->conn_state);

  if(vptr->vuser_state == NS_VUSER_PAUSED){
    vptr->flags |= NS_VPTR_FLAGS_TIMER_PENDING;
    return;
  }

  Close_connection( cptr , 0, now, NS_REQUEST_TIMEOUT, NS_COMPLETION_TIMEOUT);
}

void add_smtp_timeout_timer(connection *cptr, u_ns_ts_t now) {
 
  int timeout_val;
  ClientData client_data;
  VUser *vptr;

  client_data.p = cptr; 
  vptr = cptr->vptr;

  NSDL2_SMTP(NULL, cptr, "Method Called, cptr=%p conn state=%d", cptr, cptr->conn_state);
  
  switch(cptr->proto_state) {
/*
  case ST_SMTP_CONNECTED:
    break;
*/
  case ST_SMTP_HELO:
    timeout_val = runprof_table_shr_mem[vptr->group_num].gset.smtp_timeout; 
    break;
  case ST_SMTP_EHLO:
    timeout_val = runprof_table_shr_mem[vptr->group_num].gset.smtp_timeout; 
    break;
  case ST_SMTP_AUTH_LOGIN:
    timeout_val = runprof_table_shr_mem[vptr->group_num].gset.smtp_timeout; 
    break;
  case ST_SMTP_AUTH_LOGIN_USER_ID:
    timeout_val = runprof_table_shr_mem[vptr->group_num].gset.smtp_timeout; 
    break;
  case ST_SMTP_AUTH_LOGIN_PASSWD:
    timeout_val = runprof_table_shr_mem[vptr->group_num].gset.smtp_timeout; 
    break;
  case ST_SMTP_MAIL:
    timeout_val = runprof_table_shr_mem[vptr->group_num].gset.smtp_timeout_mail; 
    break;
  case ST_SMTP_RCPT:
    timeout_val = runprof_table_shr_mem[vptr->group_num].gset.smtp_timeout_rcpt; 
    break;
  case ST_SMTP_RCPT_CC:
    timeout_val = runprof_table_shr_mem[vptr->group_num].gset.smtp_timeout_rcpt; 
    break;
  case ST_SMTP_RCPT_BCC:
    timeout_val = runprof_table_shr_mem[vptr->group_num].gset.smtp_timeout_rcpt; 
    break;
  case ST_SMTP_DATA:
    timeout_val = runprof_table_shr_mem[vptr->group_num].gset.smtp_timeout_data_init; 
    break;
  case ST_SMTP_DATA_BODY:
    timeout_val = runprof_table_shr_mem[vptr->group_num].gset.smtp_timeout_data_term; 
    break;

  case ST_SMTP_QUIT:
    timeout_val = runprof_table_shr_mem[vptr->group_num].gset.smtp_timeout; 
    break;

  default:
    return;  // Return from Here do not add timer
  }

  NSDL3_SMTP(NULL, cptr, "timeout_val = %d", timeout_val);

  cptr->timer_ptr->actual_timeout = timeout_val;
  dis_timer_add(AB_TIMEOUT_IDLE, cptr->timer_ptr, now, smtp_timeout_handle, client_data, 0 );
}

/* b64 encoding for vector */
unsigned char *base64_encode_vector()
{
  int vlen = 0;
  int len = 0;
  int i, j;
  unsigned char *b64_str;
  unsigned char *out_ptr; /* ptr  */
  unsigned char in[3];
  int in_cnt = 0;

  NSDL2_SMTP(NULL, NULL, "Method Called");

  for (i = 0; i < g_scratch_io_vector.cur_idx; i++) { vlen += g_scratch_io_vector.vector[i].iov_len; }
  if (vlen == 0) return NULL;
  memset(in, 0, 3);
  
  /* Estimate the len of b64 string */
  len = ((vlen % 3) ? 4 : 0) + ((vlen / 3) * 4);

  len++;    /* to store \0 */
  // we will copy \r\n after the calling of this method
  //len += 2; /* for \r\n */

  MY_MALLOC(b64_str, len, "b64 encoding new buf", -1);
  out_ptr = b64_str;
  for (i = 0; i < g_scratch_io_vector.cur_idx; i++) {
    for (j = 0; j < g_scratch_io_vector.vector[i].iov_len; j++) {
      in[in_cnt] = ((char *)(g_scratch_io_vector.vector[i].iov_base))[j];
      in_cnt++;
      
      if (in_cnt == 3) {
        encodeblock(in, out_ptr, in_cnt);
        in_cnt = 0;
        memset(in, 0, 3);
        out_ptr += 4; /* advance 4 since now its b64 */
      }
    }
  }

  if (in_cnt) { /* remaining */
    encodeblock(in, out_ptr, in_cnt);
    in_cnt = 0;
    out_ptr += 4;
  }
  out_ptr[0] = '\0';

  NS_FREE_RESET_IOVEC(g_scratch_io_vector);
  
  return b64_str;
}


void debug_log_smtp_req(connection *cptr, char *buf, int bytes_to_log, int complete_data, int first_trace_write_flag)
{
  VUser* vptr = cptr->vptr;
  int request_type = cptr->url_num->request_type;

  if (!((runprof_table_shr_mem[vptr->group_num].gset.debug & DM_LOGIC4) && 
        ((request_type == SMTP_REQUEST || request_type == SMTPS_REQUEST) &&
         (runprof_table_shr_mem[vptr->group_num].gset.module_mask & MM_SMTP))))
    return;

  if((global_settings->replay_mode)) return;

  if (cptr->url_num->proto.http.body_encoding_flag != 2) // AMF - TBD
  {
    NSDL3_HTTP(vptr, cptr, "Method called. bytes_size = %d", bytes_to_log);
    char log_file[4096] = "\0";
    FILE *log_fp;
    char line_break[] = "\n------------------------------------------------------------\n";

    //Need to check if buf is null since following error is coming when try to write null
    //Error: Can not write to url request file. err = Operation now in progress, bytes_to_log = 0, buf = (null)
    //also check if bytes_to_log is 0, it possible when buf = ""
    if((buf == NULL) || (bytes_to_log == 0)) return;  

    // Log file name format is url_req_<nvm_id>_<user_id>_<sess_inst>_<pg_inst>_<url_inst>_<sess_id>_<page_id>_<url_id>
    // url_id is not yet implemented (always 0)
   SAVE_REQ_REP_FILES
   sprintf(log_file, "%s/logs/%s/smtp_session_%hd_%u_%u_%d_0_%d_%d_%d_0.dat",
          g_ns_wdir, req_rep_file_path, child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,
          vptr->group_num, GET_SESS_ID_BY_NAME(vptr),
          GET_PAGE_ID_BY_NAME(vptr));

   /* sprintf(log_file, "%s/logs/TR%d/smtp_session_%d_%u_%u_%d_0_%d_%d_%d_0.dat",
                       g_ns_wdir, testidx, my_port_index, vptr->user_index,
                       vptr->sess_inst, vptr->page_instance, vptr->group_num,
                       GET_SESS_ID_BY_NAME(vptr),
                       GET_PAGE_ID_BY_NAME(vptr));*/

    // Do not change the debug trace message as it is parsed by GUI
    if(first_trace_write_flag)
      NS_DT4(vptr, cptr, DM_L1, MM_SMTP, "Request is in file '%s'", log_file);

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
//This method to send smtp request
static void send_smtp_req(connection *cptr, int smtp_size, NSIOVector *ns_iovec, u_ns_ts_t now)
{
  int bytes_sent;
  VUser *vptr = cptr->vptr;

  NSDL2_SMTP(NULL, cptr, "cptr=%p conn state=%d, proto_state = %d, smtp_size = %d, now = %u", cptr, cptr->conn_state, cptr->proto_state, smtp_size, now);

  if ((bytes_sent = writev(cptr->conn_fd, ns_iovec->vector, ns_iovec->cur_idx)) < 0)
  {
    perror( "writev");
    printf("fd=%d num_vectors=%d \n", cptr->conn_fd, ns_iovec->cur_idx);

    NS_FREE_RESET_IOVEC(*ns_iovec);
    fprintf(stderr, "sending SMTP request failed\n");
    perror("SMTP send error");

    retry_connection(cptr, now, NS_REQUEST_WRITE_FAIL);
    return;
  }
  if(set_socketopt_for_quickack(cptr->conn_fd, 1) < 0)
  {
    NS_FREE_RESET_IOVEC(*ns_iovec);
    retry_connection(cptr, now, NS_REQUEST_CONFAIL);
    return;
  }
  if (bytes_sent < smtp_size )
  {
    handle_incomplete_write( cptr, ns_iovec, ns_iovec->cur_idx, smtp_size, bytes_sent);
    return;
  }


#ifdef NS_DEBUG_ON
  // Complete data send, so log all vectors in req file
  int i;
  for(i = 0; i < ns_iovec->cur_idx; i++)
  {
    if(i == ns_iovec->cur_idx - 1) debug_log_smtp_req(cptr, ns_iovec->vector[i].iov_base, ns_iovec->vector[i].iov_len, 0, 1);
    else debug_log_smtp_req(cptr, ns_iovec->vector[i].iov_base, ns_iovec->vector[i].iov_len, 0, 0);
     
  }
#endif
  if (cptr->conn_state == CNST_REUSE_CON)
    cptr->req_code_filled = -2;

  NS_FREE_RESET_IOVEC(*ns_iovec);

  /* cptr->conn_state for SMTP will be set to reading in on_request_write_done() */
  if (cptr->url_num->proto.http.exact)
    SetSockMinBytes(cptr->conn_fd, cptr->url_num->proto.http.bytes_to_recv);

  cptr->tcp_bytes_sent = bytes_sent;
  INC_SMTP_TX_BYTES_COUNTER(vptr, bytes_sent);
  on_request_write_done (cptr);
}

void smtp_send_ehlo(connection *cptr, u_ns_ts_t now)
{
  int i;
  int smtp_size = 0;
 
  NSDL2_SMTP(NULL, cptr, "cptr=%p conn state=%d, now = %u", cptr, cptr->conn_state, now);

  /* make request */
  NS_FILL_IOVEC(g_scratch_io_vector, SMTP_CMD_EHLO, strlen(SMTP_CMD_EHLO));

  for (i = 0; i < g_scratch_io_vector.cur_idx; i++) {
    smtp_size += g_scratch_io_vector.vector[i].iov_len;
  }

  cptr->proto_state = ST_SMTP_EHLO; /* Next reading state */

  /* send HELO */
  if(cptr->request_type == SMTPS_REQUEST){
    copy_request_into_buffer(cptr, smtp_size, &g_scratch_io_vector);
    if(set_socketopt_for_quickack(cptr->conn_fd, 1) < 0)
    {
      retry_connection(cptr, now, NS_REQUEST_CONFAIL);
      return;
    }
    handle_smtp_ssl_write(cptr, now);
  }else{
     send_smtp_req(cptr, smtp_size, &g_scratch_io_vector, now);
  }
}

void smtp_send_helo(connection *cptr, u_ns_ts_t now) 
{
  int i;
  int smtp_size = 0;

  NSDL2_SMTP(NULL, cptr, "cptr=%p conn state=%d, now = %u", cptr, cptr->conn_state, now);

  /* make request */
  NS_FILL_IOVEC(g_scratch_io_vector, SMTP_CMD_HELO, strlen(SMTP_CMD_HELO));

  for (i = 0; i < g_scratch_io_vector.cur_idx; i++) {
    smtp_size += g_scratch_io_vector.vector[i].iov_len;
  }

  cptr->proto_state = ST_SMTP_HELO; /* Next reading state */
  /* send EHLO */
  if(cptr->request_type == SMTPS_REQUEST){
    copy_request_into_buffer(cptr, smtp_size, &g_scratch_io_vector);
    if(set_socketopt_for_quickack(cptr->conn_fd, 1) < 0)
    {
      retry_connection(cptr, now, NS_REQUEST_CONFAIL);
      return;
    }
    handle_smtp_ssl_write(cptr, now);
  }else{
     send_smtp_req(cptr, smtp_size, &g_scratch_io_vector, now);
  }
}

/*-------------------------------------------------------------
Function to send STARTTLS command to server

--------------------------------------------------------------*/
void smtp_send_starttls(connection *cptr, u_ns_ts_t now) 
{
  int i;
  int smtp_size = 0;
  NSDL2_SMTP(NULL, cptr, "cptr=%p conn state=%d, now = %u", cptr, cptr->conn_state, now);

  /* make request */
  NS_FILL_IOVEC(g_scratch_io_vector, SMTP_CMD_STARTTLS, strlen(SMTP_CMD_STARTTLS));

  for (i = 0; i < g_scratch_io_vector.cur_idx; i++) {
    smtp_size += g_scratch_io_vector.vector[i].iov_len;
  }

  cptr->proto_state = ST_SMTP_STARTTLS; /* Next reading state */

  /* send STARTTLS */
  if(cptr->request_type == SMTPS_REQUEST){
    copy_request_into_buffer(cptr, smtp_size, &g_scratch_io_vector);
    if(set_socketopt_for_quickack(cptr->conn_fd, 1) < 0)
    {
      retry_connection(cptr, now, NS_REQUEST_CONFAIL);
      return;
    }
    handle_smtp_ssl_write(cptr, now);
  }else{
     send_smtp_req(cptr, smtp_size, &g_scratch_io_vector, now);
  }
}


void smtp_send_auth_login(connection *cptr, u_ns_ts_t now) 
{
  int i;
  int smtp_size = 0;

  NSDL2_SMTP(NULL, cptr, "cptr=%p conn state=%d, now = %u", cptr, cptr->conn_state, now);

  /* make request */
  NS_FILL_IOVEC(g_scratch_io_vector, SMTP_CMD_AUTH_LOGIN, strlen(SMTP_CMD_AUTH_LOGIN));

  for (i = 0; i < g_scratch_io_vector.cur_idx; i++) {
    smtp_size += g_scratch_io_vector.vector[i].iov_len;
  }

  cptr->proto_state = ST_SMTP_AUTH_LOGIN; /* Next reading state */

  /* send EHLO */
  if(cptr->request_type == SMTPS_REQUEST){
    copy_request_into_buffer(cptr, smtp_size, &g_scratch_io_vector);
    if(set_socketopt_for_quickack(cptr->conn_fd, 1) < 0)
    {
      retry_connection(cptr, now, NS_REQUEST_CONFAIL);
      return;
    }
    handle_smtp_ssl_write(cptr, now);
  }else{
     send_smtp_req(cptr, smtp_size, &g_scratch_io_vector, now);
  }
}

void smtp_send_auth_login_user_id(connection *cptr, u_ns_ts_t now)
{
  char *b64_str;
  int i, ret;
  int smtp_size = 0;

  NSDL2_SMTP(NULL, cptr, "cptr=%p conn state=%d, now = %u", cptr, cptr->conn_state, now);

  /* insert USER ID and send */
  ret = insert_segments(cptr->vptr, cptr, &(cptr->url_num->proto.smtp.user_id), &g_scratch_io_vector, NULL, 0, 1, 1, cptr->url_num, SEG_IS_NOT_REPEAT_BLOCK);
 
  if(ret == -2)
    return;
 
  b64_str = (char *)base64_encode_vector();

  if(b64_str) {
    NS_FILL_IOVEC_AND_MARK_FREE(g_scratch_io_vector, b64_str, strlen(b64_str)); 
  }

  NS_FILL_IOVEC(g_scratch_io_vector, SMTP_CMD_CRLF, strlen(SMTP_CMD_CRLF));

  for (i = 0; i < g_scratch_io_vector.cur_idx; i++) {
    smtp_size += g_scratch_io_vector.vector[i].iov_len;
  }

  cptr->proto_state = ST_SMTP_AUTH_LOGIN_USER_ID; /* Next reading state */
 
  if(cptr->request_type == SMTPS_REQUEST){
    copy_request_into_buffer(cptr, smtp_size, &g_scratch_io_vector);
    if(set_socketopt_for_quickack(cptr->conn_fd, 1) < 0)
    {
      retry_connection(cptr, now, NS_REQUEST_CONFAIL);
      return;
    }
    handle_smtp_ssl_write(cptr, now);
  }else{
     send_smtp_req(cptr, smtp_size, &g_scratch_io_vector, now);
  }
}

void smtp_send_auth_login_passwd(connection *cptr, u_ns_ts_t now)
{
  char *b64_str;
  int i, ret;
  int smtp_size = 0;

  NSDL2_SMTP(NULL, cptr, "cptr=%p conn state=%d, now = %u", cptr, cptr->conn_state, now);
  /* insert USER ID and send */
  ret = insert_segments(cptr->vptr, cptr, &(cptr->url_num->proto.smtp.passwd), &g_scratch_io_vector, NULL, 0, 1, 1, cptr->url_num, SEG_IS_NOT_REPEAT_BLOCK);
  
  if(ret == -2)
    return;

  b64_str = (char*)base64_encode_vector();

  if(b64_str) {
    NS_FILL_IOVEC_AND_MARK_FREE(g_scratch_io_vector, b64_str, strlen(b64_str));
  }

  NS_FILL_IOVEC(g_scratch_io_vector, SMTP_CMD_CRLF, strlen(SMTP_CMD_CRLF));

  for (i = 0; i < g_scratch_io_vector.cur_idx; i++) {
    smtp_size += g_scratch_io_vector.vector[i].iov_len;
  }

  cptr->proto_state = ST_SMTP_AUTH_LOGIN_PASSWD; /* Next reading state */
  
  if(cptr->request_type == SMTPS_REQUEST){
    copy_request_into_buffer(cptr, smtp_size, &g_scratch_io_vector);
    if(set_socketopt_for_quickack(cptr->conn_fd, 1) < 0)
    {
      retry_connection(cptr, now, NS_REQUEST_CONFAIL);
      return;
    }
    handle_smtp_ssl_write(cptr, now);
  }else{
     send_smtp_req(cptr, smtp_size, &g_scratch_io_vector, now);
  }
}

void smtp_send_mail(connection *cptr, u_ns_ts_t now)
{
  int i, smtp_size = 0;
  int ret;

  NSDL2_SMTP(NULL, cptr, "cptr=%p conn state=%d, now = %u", cptr, cptr->conn_state, now);

  /* insert MAIL FROM: and send */
  ret = insert_segments(cptr->vptr, cptr, &(cptr->url_num->proto.smtp.from_email), &g_scratch_io_vector, NULL, 0, 1, 1, cptr->url_num, SEG_IS_NOT_REPEAT_BLOCK);
  
  if(ret == -2)
    return;

  for (i = 0; i < g_scratch_io_vector.cur_idx; i++) {
    smtp_size += g_scratch_io_vector.vector[i].iov_len;
  }

  NS_DT2(NULL, cptr, DM_L1, MM_SMTP, "Sending 'MAIL FROM:' command to Server.");
  cptr->proto_state = ST_SMTP_MAIL; /* Next reading state */

  if(cptr->request_type == SMTPS_REQUEST){
    copy_request_into_buffer(cptr, smtp_size, &g_scratch_io_vector);
    if(set_socketopt_for_quickack(cptr->conn_fd, 1) < 0)
    {
      retry_connection(cptr, now, NS_REQUEST_CONFAIL);
      return;
    }
    handle_smtp_ssl_write(cptr, now);
  }else{
     send_smtp_req(cptr, smtp_size, &g_scratch_io_vector, now);
  }
}

void smtp_send_quit(connection *cptr, u_ns_ts_t now) 
{
  int i, smtp_size = 0;
  
  NSDL2_SMTP(NULL, cptr, "cptr=%p conn state=%d, now = %u", cptr, cptr->conn_state, now);

  /* make request */
  NS_FILL_IOVEC(g_scratch_io_vector, SMTP_CMD_QUIT, strlen(SMTP_CMD_QUIT));  

  for (i = 0; i < g_scratch_io_vector.cur_idx; i++) {
    smtp_size += g_scratch_io_vector.vector[i].iov_len;
  }

  cptr->proto_state = ST_SMTP_QUIT; /* Next reading state */

  /* send QUIT */
  if(cptr->request_type == SMTPS_REQUEST){
    copy_request_into_buffer(cptr, smtp_size, &g_scratch_io_vector);
    if(set_socketopt_for_quickack(cptr->conn_fd, 1) < 0)
    {
      retry_connection(cptr, now, NS_REQUEST_CONFAIL);
      return;
    }
    handle_smtp_ssl_write(cptr, now);
  }else{
     send_smtp_req(cptr, smtp_size, &g_scratch_io_vector, now);
  }
}

/**
 * smtp_send_rcpt() sends multiple RCPT. So we keep track of the numbers through
 * cptr->redirect_count; Since this variable is useless for SMTP type request, we
 * can override it.  
 */
void smtp_send_rcpt(connection *cptr, u_ns_ts_t now)
{
  int smtp_size = 0, i;
  int ret;
  
  NSDL2_SMTP(NULL, cptr, "cptr=%p conn state=%d, redirect_count = %d, num_to_emails = %d, location_url = %p, now = %u",
                          cptr, cptr->conn_state, cptr->redirect_count, cptr->url_num->proto.smtp.num_to_emails, cptr->location_url, now);

  /* if we have sent all RCPTs we reset the counter and return while setting the state */
  if (cptr->redirect_count == cptr->url_num->proto.smtp.num_to_emails) {
    cptr->redirect_count = 0;
    smtp_send_rcpt_cc(cptr, now);
    return;
  }
  
  //Narendra: 19Nov2013 , Changes to support multiple to_emails using parameters.
  //First we will fill to_emails in vectors, then we will check if there are multiple emails(if there are , in to_emails), then
  //save other emails in a buffer. modify vectors to save first email , and free remaining vectors. save remaining email buffer
  //to cptr->location_url(because it is not being used in case of smtp). And send request for first email. 
  //next time it will take to_email from cptr->location_url. 
  //cptr->redirect_count will only be incremented if email is last one.
  /* make request */
  if(!cptr->location_url) {
    ret = insert_segments(cptr->vptr, cptr, &(cptr->url_num->proto.smtp.to_emails[cptr->redirect_count]),
                                &g_scratch_io_vector, NULL, 0, 1, 1, cptr->url_num, SEG_IS_NOT_REPEAT_BLOCK);
  if(ret == -2)
    return;
  }
  else{
    NSDL2_SMTP(NULL, cptr, "Taking rcpt list from last partial rcpt_list = %s", cptr->location_url);
    NS_FILL_IOVEC(g_scratch_io_vector, cptr->location_url, strlen(cptr->location_url));
  }
 
  /* search , in these vector and keep remain string for further use */
  char *ptr = NULL;
  int brk_idx = -1;
  char multiple_rcpt_flag = 0;
  char rcpt_list[2048] = "";
  int rcpt_len = 0;
  int num_vectors = g_scratch_io_vector.cur_idx; 
  for (i = 0; i < num_vectors; i++) {
    if (!multiple_rcpt_flag && (NULL != ( ptr = strchr(g_scratch_io_vector.vector[i].iov_base, ',')))) {
      brk_idx = i;
      smtp_size += (ptr - (char *)g_scratch_io_vector.vector[i].iov_base);
      sprintf(rcpt_list, "RCPT TO: %s", ptr + 1);
      multiple_rcpt_flag = 1;
    }
    else{
      if(multiple_rcpt_flag)
        strcat(rcpt_list, g_scratch_io_vector.vector[i].iov_base);
      else
        smtp_size += g_scratch_io_vector.vector[i].iov_len;
    }
  }
  NS_DT2(NULL, cptr, DM_L1, MM_SMTP, "Remaining RCPT_LIST = %s", rcpt_list);
  /*Update next vecotor to brk_idx by \r\n*/
  if(multiple_rcpt_flag)
  {
    for(i = brk_idx + 1; i < num_vectors; i++);
      NS_FREE_IOVEC(g_scratch_io_vector, brk_idx + 1);

    NS_FILL_IOVEC_IDX(g_scratch_io_vector, CRLFString, CRLFString_Length, brk_idx + 1);
    g_scratch_io_vector.cur_idx = num_vectors = brk_idx + 2;
    rcpt_len = strlen(rcpt_list);
  } 
  cptr->proto_state = ST_SMTP_RCPT; /* Next reading state */  

  NS_DT2(NULL, cptr, DM_L1, MM_SMTP, "Sending 'RCPT TO:' command to Server.");

  if(cptr->request_type == SMTPS_REQUEST){
    copy_request_into_buffer(cptr, smtp_size, &g_req_rep_io_vector);
    if(set_socketopt_for_quickack(cptr->conn_fd, 1) < 0)
    {
      retry_connection(cptr, now, NS_REQUEST_CONFAIL);
      return;
    }
    handle_smtp_ssl_write(cptr, now);
  }else{
     send_smtp_req(cptr, smtp_size, &g_scratch_io_vector, now);
  }
  if(multiple_rcpt_flag)
  {
    if(!cptr->location_url){
      cptr->location_url = malloc(rcpt_len + 1);
      if(!cptr->location_url)
      {
        //If failed to allocate to big buffer then just skip to next one.
        NS_DT2(NULL, cptr, DM_L1, MM_SMTP, "Failed to allocate memory for remaing recipients, skipping.");
        cptr->redirect_count ++;
      }
      else {
        strncpy(cptr->location_url, rcpt_list, rcpt_len);
        cptr->location_url[rcpt_len] = 0; 
      }
    }
    /*If it is already allocated then no need to allocate it again*/
    else{
      strncpy(cptr->location_url, rcpt_list, rcpt_len);
      cptr->location_url[rcpt_len] = 0;      
    }
  }
  else{
    cptr->redirect_count++;
    FREE_AND_MAKE_NULL_EX(cptr->location_url, 0, "cptr->location_url", -1);
  }
}

void smtp_send_rcpt_cc(connection *cptr, u_ns_ts_t now)
{
  int smtp_size = 0, i;
  int ret;
  
  NSDL2_SMTP(NULL, cptr, "cptr=%p conn state=%d, redirect_count = %d, num_cc_emails = %d, now = %u",
                          cptr, cptr->conn_state, cptr->redirect_count, cptr->url_num->proto.smtp.num_cc_emails, now);

  /* if we have sent all RCPTs we reset the counter and return while setting the state */
  if (cptr->redirect_count == cptr->url_num->proto.smtp.num_cc_emails) {
    cptr->redirect_count = 0;
    smtp_send_rcpt_bcc(cptr, now);
    return;
  }
    
  /* make request */
  ret = insert_segments(cptr->vptr, cptr, &(cptr->url_num->proto.smtp.cc_emails[cptr->redirect_count]),
                                &g_scratch_io_vector, NULL, 0, 1, 1, cptr->url_num, SEG_IS_NOT_REPEAT_BLOCK);
  
  if(ret == -2)
    return;

  for (i = 0; i < g_scratch_io_vector.cur_idx; i++) {
    smtp_size += g_scratch_io_vector.vector[i].iov_len;
  }

  cptr->proto_state = ST_SMTP_RCPT_CC; /* Next reading state */  

  NS_DT2(NULL, cptr, DM_L1, MM_SMTP, "Sending Cc 'RCPT TO:' command to Server.");

  if(cptr->request_type == SMTPS_REQUEST){
    copy_request_into_buffer(cptr, smtp_size, &g_scratch_io_vector);
    if(set_socketopt_for_quickack(cptr->conn_fd, 1) < 0)
    {
      retry_connection(cptr, now, NS_REQUEST_CONFAIL);
      return;
    }
    handle_smtp_ssl_write(cptr, now);
  }else{
     send_smtp_req(cptr, smtp_size, &g_scratch_io_vector, now);
  }
  cptr->redirect_count++;
}

void smtp_send_rcpt_bcc(connection *cptr, u_ns_ts_t now)
{
  int smtp_size = 0, i;
  int ret;
  
  NSDL2_SMTP(NULL, cptr, "cptr=%p conn state=%d, redirect_count = %d, num_bcc_emails = %d, now = %u",
                          cptr, cptr->conn_state, cptr->redirect_count, cptr->url_num->proto.smtp.num_bcc_emails, now);

  /* if we have sent all RCPTs we reset the counter and return while setting the state */
  if (cptr->redirect_count == cptr->url_num->proto.smtp.num_bcc_emails) {
    cptr->redirect_count = 0;
    cptr->proto_state = ST_SMTP_DATA; /* Next reading state */  
    smtp_send_data(cptr, now);
    return;
  }
    
  /* make request */
  ret = insert_segments(cptr->vptr, cptr, &(cptr->url_num->proto.smtp.bcc_emails[cptr->redirect_count]),
                                &g_scratch_io_vector, NULL, 0, 1, 1, cptr->url_num, SEG_IS_NOT_REPEAT_BLOCK);
  
  if(ret == -2)
    return;

  for (i = 0; i < g_scratch_io_vector.cur_idx; i++) {
    smtp_size += g_scratch_io_vector.vector[i].iov_len;
  }

  cptr->proto_state = ST_SMTP_RCPT_BCC; /* Next reading state */  

  NS_DT2(NULL, cptr, DM_L1, MM_SMTP, "Sending Bcc 'RCPT TO:' command to Server.");

  if(cptr->request_type == SMTPS_REQUEST){
    copy_request_into_buffer(cptr, smtp_size, &g_req_rep_io_vector);
    if(set_socketopt_for_quickack(cptr->conn_fd, 1) < 0)
    {
      retry_connection(cptr, now, NS_REQUEST_CONFAIL);
      return;
    }
    handle_smtp_ssl_write(cptr, now);
  }else{
     send_smtp_req(cptr, smtp_size, &g_scratch_io_vector, now);
  }
  cptr->redirect_count++;
}


void smtp_send_data(connection *cptr, u_ns_ts_t now)
{
  int i;
  int smtp_size = 0;
 
  NSDL2_SMTP(NULL, cptr, "cptr=%p conn state=%d, now = %u", cptr, cptr->conn_state, now);

  /* make request */
  NS_FILL_IOVEC(g_scratch_io_vector, SMTP_CMD_DATA, strlen(SMTP_CMD_DATA));

  for (i = 0; i < g_scratch_io_vector.cur_idx; i++) {
    smtp_size += g_scratch_io_vector.vector[i].iov_len;
  }

  cptr->proto_state = ST_SMTP_DATA; /* Next reading state */
  /* send EHLO */
  if(cptr->request_type == SMTPS_REQUEST){
    copy_request_into_buffer(cptr, smtp_size, &g_scratch_io_vector);
    if(set_socketopt_for_quickack(cptr->conn_fd, 1) < 0)
    {
      retry_connection(cptr, now, NS_REQUEST_CONFAIL);
      return;
    }
    handle_smtp_ssl_write(cptr, now);
  }else{
     send_smtp_req(cptr, smtp_size, &g_scratch_io_vector, now);
  }
}

/* In smtp_send_data_body() we send Headers + Body + Attachments */
/* There is an issue here; vector limit is only 255 */
void smtp_send_data_body(connection *cptr, u_ns_ts_t now)
{
  int ret;
  int smtp_size = 0, i;
  VUser *vptr = cptr->vptr;
  ClientData client_data;

  client_data.p = cptr; 

  NSDL2_SMTP(NULL, cptr, "cptr=%p conn state=%d, , num_attachment = %hd, now = %u",
                          cptr, cptr->conn_state, cptr->url_num->proto.smtp.num_attachments, now);

  /* Fill hdrs */
  ret = insert_segments(vptr, cptr, &(cptr->url_num->proto.smtp.hdrs),
                             &g_scratch_io_vector, NULL, 0, 1, 1, cptr->url_num, SEG_IS_NOT_REPEAT_BLOCK);
  
  if(ret == -2)
    return;
  /* put new line */
  NS_FILL_IOVEC(g_scratch_io_vector, SMTP_CMD_CRLF, strlen(SMTP_CMD_CRLF));

  /* Fill Body */
  if (cptr->url_num->proto.smtp.enable_rand_bytes) {
    int a = cptr->url_num->proto.smtp.rand_bytes_min;
    int b = cptr->url_num->proto.smtp.rand_bytes_max;
    int size;

    size = a + (int) (((float)b - ((float)a - 1)) * (rand() / (RAND_MAX + (float)b)));

    NSDL2_SMTP(NULL, NULL, "rand_min = %f, rand_max = %f, size = %d", (float)a, (float)b, size);

    /* reuseing url_resp_buff for writing also */
    if( (url_resp_buff == NULL) || (size > url_resp_size)) {
      MY_REALLOC(url_resp_buff, size + 1, "url_resp_buff", -1);
      url_resp_size = size; // does not include null temination
    }

    NS_FILL_IOVEC(g_scratch_io_vector, smtp_body_hdr_begin, smtp_body_hdr_begin_len);

    generate_body_content(size, url_resp_buff);
    NS_FILL_IOVEC(g_scratch_io_vector, url_resp_buff, size); 

    NS_FILL_IOVEC(g_scratch_io_vector, SMTP_CMD_CRLF, strlen(SMTP_CMD_CRLF));    

    /* Add tail */
    if (cptr->url_num->proto.smtp.num_attachments == 0) {
      NS_FILL_IOVEC(g_scratch_io_vector, attachment_end_boundary, strlen(attachment_end_boundary));
    }
    NS_FILL_IOVEC(g_scratch_io_vector, SMTP_CMD_CRLF, strlen(SMTP_CMD_CRLF));
  } else
    ret = insert_segments(vptr, cptr, cptr->url_num->proto.smtp.body_ptr,
                               &g_scratch_io_vector, NULL, 0, 1, 1, cptr->url_num, SEG_IS_NOT_REPEAT_BLOCK);

  if(ret == -2)
    return;

  if (cptr->url_num->proto.smtp.num_attachments > 0) {
    /* put new line */
    NS_FILL_IOVEC(g_scratch_io_vector, SMTP_CMD_CRLF, strlen(SMTP_CMD_CRLF));
  }

  /* Fill attachments */
  for (i = 0; i < cptr->url_num->proto.smtp.num_attachments; i++) {
    ret = insert_segments(vptr, cptr, &(cptr->url_num->proto.smtp.attachment_ptr[i]),
                               &g_scratch_io_vector, NULL, 0, 1, 1, cptr->url_num, SEG_IS_NOT_REPEAT_BLOCK);
    if(ret == -2)
      return;
  }

  // Apending --Attachment End boundry--
  NS_FILL_IOVEC(g_scratch_io_vector, attachment_end_boundary, strlen(attachment_end_boundary));
  
  /* Append \r\n.\r\n */
  NS_FILL_IOVEC(g_scratch_io_vector, SMTP_CMD_BODY_TERM, strlen(SMTP_CMD_BODY_TERM));
  
  for (i=0; i < g_scratch_io_vector.cur_idx; i++) {
    smtp_size += g_scratch_io_vector.vector[i].iov_len;
  }

  cptr->proto_state = ST_SMTP_DATA_BODY; /* Next reading state */

/* Data Block: 3 Minutes (default)

   This is while awaiting the completion of each TCP SEND call
   transmitting a chunk of data.

   This timer is explicitly deleted in on_url_write_done()
*/
  cptr->timer_ptr->actual_timeout = runprof_table_shr_mem[vptr->group_num].gset.smtp_timeout_data_block; 
  dis_timer_add(AB_TIMEOUT_IDLE, cptr->timer_ptr, now, smtp_timeout_handle, client_data, 0 );

  NS_DT2(NULL, cptr, DM_L1, MM_SMTP, "Sending Body content to Server.");
 
  if(cptr->request_type == SMTPS_REQUEST){
    copy_request_into_buffer(cptr, smtp_size, &g_req_rep_io_vector);
    if(set_socketopt_for_quickack(cptr->conn_fd, 1) < 0)
    {
      retry_connection(cptr, now, NS_REQUEST_CONFAIL);
      return;
    }
    handle_smtp_ssl_write(cptr, now);
  }else{
     send_smtp_req(cptr, smtp_size, &g_scratch_io_vector, now);
  }
  cptr->mail_sent_count--;
}
