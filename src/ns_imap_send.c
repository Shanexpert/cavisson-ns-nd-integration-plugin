#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
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
#include "ns_imap.h"
#include "nslb_util.h"
#include "ns_auto_fetch_embd.h"
#include "ns_dns.h"
#include "ns_url_resp.h"
#include "ns_string.h"
#include "ns_log_req_rep.h"
#include "nslb_search_vars.h"
#include "nslb_cav_conf.h"

int g_imap_cavgtime_idx = -1;
#ifndef CAV_MAIN
int g_imap_avgtime_idx = -1;
IMAPAvgTime *imap_avgtime = NULL;
#else
__thread int g_imap_avgtime_idx = -1;
__thread IMAPAvgTime *imap_avgtime = NULL;
#endif
IMAPCAvgTime *imap_cavgtime;

void debug_log_imap_req(connection *cptr, char *buf, int bytes_to_log, int complete_data, int first_trace_write_flag)
{

  //TODO: handle new ns_logs location in case of partition and non partition case ......................................................
  VUser* vptr = cptr->vptr;
  int request_type = cptr->url_num->request_type;

  if (!((runprof_table_shr_mem[vptr->group_num].gset.debug & DM_LOGIC4) && 
        (((request_type == IMAP_REQUEST) || (request_type == IMAPS_REQUEST)) &&
         (runprof_table_shr_mem[vptr->group_num].gset.module_mask & MM_IMAP))))
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

    // Log file name format is url_req_<nvm_id>_<user_id>_<sess_inst>_<pg_inst>_<url_inst>_<sess_id>_<page_id>_<url_id>
    // url_id is not yet implemented (always 0)

   SAVE_REQ_REP_FILES
   sprintf(log_file, "%s/logs/%s/imap_session_%hd_%u_%u_%d_0_%d_%d_%d_0.dat",
          g_ns_wdir, req_rep_file_path, child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,
          vptr->group_num, GET_SESS_ID_BY_NAME(vptr),
          GET_PAGE_ID_BY_NAME(vptr));
    // Do not change the debug trace message as it is parsed by GUI
    if(first_trace_write_flag)
      NS_DT4(vptr, cptr, DM_L1, MM_IMAP, "Request is in file '%s'", log_file);

    log_fp = fopen(log_file, "a+");
    if (log_fp == NULL)
    {
      fprintf(stderr, "Unable to open file %s. err = %s\n", log_file, strerror(errno));
      return;
    }

    //write for both ssl and non ssl url
    if(fwrite(buf, bytes_to_log, 1, log_fp) != 1)
    {
      fprintf(stderr, "Error: Can not write to url request file. err = %s, bytes_to_log = %d, buf = %s\n", strerror(errno), bytes_to_log, buf);
      return;
    }
    if (complete_data) fwrite(line_break, strlen(line_break), 1, log_fp);

    if(fclose(log_fp) != 0)
    {
      fprintf(stderr, "Unable to close url request file. err = %s\n", strerror(errno));
      return;
    }
  }
}

void debug_log_imap_res(connection *cptr, char *buf, int size) //TODO:HANDLE for log directories in case of partition and non partition case
{
  VUser *vptr;
  vptr = (VUser *)cptr->vptr;
  int request_type = cptr->url_num->request_type;

  if (!((runprof_table_shr_mem[vptr->group_num].gset.debug & DM_LOGIC4) && 
        (((request_type == IMAP_REQUEST) || (request_type == IMAPS_REQUEST)) &&
         (runprof_table_shr_mem[vptr->group_num].gset.module_mask & MM_IMAP))))
    return;

  {
    char log_file[1024];
    int log_fd;

    // Log file name format is url_rep_<nvm_id>_<user_id>_<sess_inst>_<pg_inst>_<url_inst>_<sess_id>_<page_id>_<url_id>
    // url_id is not yet implemented (always 0)
    //sprintf(log_file, "%s/logs/TR%d/url_rep_%d_%ld_%ld_%d_0.dat", g_ns_wdir, testidx, my_port_index, vptr->user_index, cur_vptr->sess_inst, vptr->page_instance);
  

   SAVE_REQ_REP_FILES
   sprintf(log_file, "%s/logs/%s/imap_session_%hd_%u_%u_%d_0_%d_%d_%d_0.dat",
          g_ns_wdir, req_rep_file_path, child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,
          vptr->group_num, GET_SESS_ID_BY_NAME(vptr),
          GET_PAGE_ID_BY_NAME(vptr));
    // Do not change the debug trace message as it is parsed by GUI
    if(cptr->tcp_bytes_recv == 0)  //This check to print debug trace only once
      NS_DT4(vptr, cptr, DM_L1, MM_IMAP, "Response is in file '%s'", log_file);

    if((log_fd = open(log_file, O_CREAT|O_CLOEXEC|O_WRONLY|O_APPEND, 00666)) < 0)
        fprintf(stderr, "Error: Error in opening file for logging URL request\n");
    else
    {
      write(log_fd, buf, size);
      close(log_fd);
    }
  }
}

static void send_imap_req(connection *cptr, int imap_size, NSIOVector *ns_iovec, u_ns_ts_t now)
{
  int bytes_sent;

  NSDL2_IMAP(NULL, cptr, "cptr=%p conn state=%d, proto_state = %d, imap_size = %d, now = %u", cptr, cptr->conn_state, cptr->proto_state, imap_size, now);

  if ((bytes_sent = writev(cptr->conn_fd, ns_iovec->vector, NS_GET_IOVEC_CUR_IDX(*ns_iovec))) < 0)
  {
    perror( "writev");
    printf("fd=%d num_vectors=%d \n", cptr->conn_fd, NS_GET_IOVEC_CUR_IDX(*ns_iovec));
    NS_FREE_RESET_IOVEC(*ns_iovec);
    
    fprintf(stderr, "sending imap request failed\n");
    perror("IMAP send error");

    retry_connection(cptr, now, NS_REQUEST_WRITE_FAIL);
    return;
  }

  if(set_socketopt_for_quickack(cptr->conn_fd, 1) < 0)
  {
    retry_connection(cptr, now, NS_REQUEST_CONFAIL);
    return;
  }

  if (bytes_sent < imap_size )
  {
    handle_incomplete_write(cptr, ns_iovec, NS_GET_IOVEC_CUR_IDX(*ns_iovec), imap_size, bytes_sent);
    return;
  }


#ifdef NS_DEBUG_ON
  // Complete data send, so log all vectors in req file
  int i;
  for(i = 0; i < NS_GET_IOVEC_CUR_IDX(*ns_iovec); i++)
  {
    if(i == NS_GET_IOVEC_CUR_IDX(*ns_iovec) - 1) debug_log_imap_req(cptr, NS_GET_IOVEC_VAL(*ns_iovec, i), NS_GET_IOVEC_LEN(*ns_iovec, i), 0, 1);
    else debug_log_imap_req(cptr, NS_GET_IOVEC_VAL(*ns_iovec, i), NS_GET_IOVEC_LEN(*ns_iovec, i), 0, 0);
  }
#endif
  if (cptr->conn_state == CNST_REUSE_CON)
    cptr->req_code_filled = -2;

  NS_FREE_RESET_IOVEC(*ns_iovec);

  if (cptr->url_num->proto.http.exact)
    SetSockMinBytes(cptr->conn_fd, cptr->url_num->proto.http.bytes_to_recv);

  cptr->tcp_bytes_sent = bytes_sent;
  imap_avgtime->imap_tx_bytes += bytes_sent;
  on_request_write_done (cptr);
}

void imap_send_starttls(connection *cptr, u_ns_ts_t now)
{
  int i;
  int imap_size = 0;
  char tmp_buf[10] = "0";

  NSDL2_IMAP(NULL, cptr, "cptr=%p conn state=%d, now = %u", cptr, cptr->conn_state, now);

  int cp_bytes = sprintf(tmp_buf, "%s ", (char*)ns_get_random_str(2, 3, "a-z0-9")); 
  tmp_buf[cp_bytes] = '\0';
     
  NS_RESET_IOVEC(g_scratch_io_vector);

  NS_FILL_IOVEC(g_scratch_io_vector, tmp_buf, strlen(tmp_buf));
  /* make request */
  NS_FILL_IOVEC(g_scratch_io_vector, IMAP_CMD_STARTTLS, strlen(IMAP_CMD_STARTTLS));

  for (i=0; i < NS_GET_IOVEC_CUR_IDX(g_scratch_io_vector); i++) {
    imap_size += NS_GET_IOVEC_LEN(g_scratch_io_vector, i);
  }

  //cptr->proto_state = cptr->url_num->proto.imap.imap_action_type; /* Next reading state */
  cptr->proto_state = ST_IMAP_STARTTLS; /* Next reading state */
  if(cptr->request_type == IMAPS_REQUEST){
    copy_request_into_buffer(cptr, imap_size, &g_scratch_io_vector);
    if(set_socketopt_for_quickack(cptr->conn_fd, 1) < 0)
    {
       retry_connection(cptr, now, NS_REQUEST_CONFAIL);
       return;
    }
    handle_imap_ssl_write (cptr, now);
  }else{
    send_imap_req(cptr, imap_size, &g_scratch_io_vector, now);
  }
}

void imap_send_login(connection *cptr, u_ns_ts_t now)
{
  int i, next_idx = 0;
  int imap_size = 0;
  char tmp_buf[10] = "0";

  NSDL2_IMAP(NULL, cptr, "cptr=%p conn state=%d, now = %u", cptr, cptr->conn_state, now);

  int cp_bytes = sprintf(tmp_buf, "%s ", (char*)ns_get_random_str(2, 3, "a-z0-9")); 
  tmp_buf[cp_bytes] = '\0';

  NS_RESET_IOVEC(g_scratch_io_vector);

  NS_FILL_IOVEC(g_scratch_io_vector, tmp_buf, strlen(tmp_buf));    
  /* make request */
  NS_FILL_IOVEC(g_scratch_io_vector, IMAP_CMD_LOGIN, strlen(IMAP_CMD_LOGIN));

  /* put user name */

  next_idx = insert_segments(cptr->vptr, cptr, &(cptr->url_num->proto.imap.user_id), &g_scratch_io_vector, NULL, 0, 1, 1, cptr->url_num, SEG_IS_NOT_REPEAT_BLOCK);

  if(next_idx == -2)
    return;

  NS_FILL_IOVEC(g_scratch_io_vector, " ", strlen(" "));
  /* put password */

  next_idx = insert_segments(cptr->vptr, cptr, &(cptr->url_num->proto.imap.passwd), &g_scratch_io_vector, NULL, 0, 1, 1, cptr->url_num, SEG_IS_NOT_REPEAT_BLOCK);

  if(next_idx == -2)
    return;

  /* append CRLF */
  NS_FILL_IOVEC(g_scratch_io_vector, IMAP_CMD_CRLF, strlen(IMAP_CMD_CRLF));

  for (i = 0; i < NS_GET_IOVEC_CUR_IDX(g_scratch_io_vector); i++) {
    imap_size += NS_GET_IOVEC_LEN(g_scratch_io_vector, i);
  }

  cptr->proto_state = ST_IMAP_LOGIN; /* Next reading state */
  if(cptr->request_type == IMAPS_REQUEST){
    copy_request_into_buffer(cptr, imap_size, &g_scratch_io_vector);
    if(set_socketopt_for_quickack(cptr->conn_fd, 1) < 0)
    {
       retry_connection(cptr, now, NS_REQUEST_CONFAIL);
       return;
    }
    handle_imap_ssl_write (cptr, now);
  }else{
    send_imap_req(cptr, imap_size, &g_scratch_io_vector, now);
  }
}

void imap_process_logout(connection *cptr, u_ns_ts_t now, char *buf, int bytes_read)
{
  int status = cptr->req_ok;
  
  NSDL2_IMAP(NULL, cptr, "conn state=%d, req_code = %s, buf = %s, bytes_read = %d, now = %u",
             cptr->conn_state, 
             cptr->req_code == IMAP_ERR ? "BAD" : "OK", 
             buf, bytes_read, now);
    
  // Confail means req_ok is not filled by any error earliar, so its a success case
  if(status == NS_REQUEST_CONFAIL)
    status = NS_REQUEST_OK;

  Close_connection(cptr, 0, now, status, NS_COMPLETION_CLOSE);
}

void imap_send_fetch(connection *cptr, u_ns_ts_t now)
{
  int i,next_idx = 0;
  int imap_size = 0;
  int tmp_len = 0;
  char part_buf[50] = "";
  char tmp_buf[10] = "";

  NSDL2_IMAP(NULL, cptr, "cptr=%p conn state=%d, now = %u", cptr, cptr->conn_state, now);

  int cp_bytes = sprintf(tmp_buf, "%s ", (char*)ns_get_random_str(2, 3, "a-z0-9")); 
  tmp_buf[cp_bytes] = '\0';

  NS_RESET_IOVEC(g_req_rep_io_vector);

  NS_FILL_IOVEC(g_req_rep_io_vector, tmp_buf, strlen(tmp_buf)); 
  
  /* make request */
  NS_FILL_IOVEC(g_req_rep_io_vector, IMAP_CMD_FETCH, strlen(IMAP_CMD_FETCH));

  next_idx = insert_segments(cptr->vptr, cptr, &(cptr->url_num->proto.imap.mail_seq), &g_req_rep_io_vector, NULL, 0, 1, 1, cptr->url_num, SEG_IS_NOT_REPEAT_BLOCK);

  if(next_idx == -2)
    return;
  
  // if given 
  if(cptr->url_num->proto.imap.fetch_part.seg_start) {
     tmp_len = get_value_from_segments(cptr, part_buf, &cptr->url_num->proto.imap.fetch_part, "FETCH", NS_MAXCDNAME);
    if(tmp_len > 0){
      if(!strcasecmp(part_buf, "FULL")){
        tmp_len = sprintf(part_buf, "%s", " full\r\n");
      }else if(!strcasecmp(part_buf, "HEADER")){
        tmp_len = sprintf(part_buf, "%s", " body[header]\r\n");
      }else if(!strcasecmp(part_buf, "TEXT")){
        tmp_len = sprintf(part_buf, "%s", " body[text]\r\n");
      }else if(!strcasecmp(part_buf, "ATTACH")){
        tmp_len = sprintf(part_buf, "%s", " body[]\r\n"); //TODO: fill for attachment
      }else if(!strcasecmp(part_buf, "BODY")){
        tmp_len = sprintf(part_buf, "%s", " body\r\n");
      }else{
        //Error: we have handled this case in parsing, should not happen here
      }
    }
  }
  part_buf[tmp_len] = '\0';
  NS_FILL_IOVEC(g_req_rep_io_vector, part_buf, strlen(part_buf));

  for (i = 0; i < NS_GET_IOVEC_CUR_IDX(g_req_rep_io_vector); i++) {
    imap_size += NS_GET_IOVEC_LEN(g_req_rep_io_vector, i);
  }

  NS_DT2(NULL, cptr, DM_L1, MM_IMAP, "Sending 'FETCH' to Server.");

  cptr->proto_state = ST_IMAP_FETCH; /* Next reading state */

  if(cptr->request_type == IMAPS_REQUEST){
    copy_request_into_buffer(cptr, imap_size, &g_req_rep_io_vector);
    if(set_socketopt_for_quickack(cptr->conn_fd, 1) < 0)
    {
       retry_connection(cptr, now, NS_REQUEST_CONFAIL);
       return;
    }
    handle_imap_ssl_write (cptr, now);
  }else{
    send_imap_req(cptr, imap_size, &g_req_rep_io_vector, now);
  }
}

void imap_send_logout(connection *cptr, u_ns_ts_t now)
{
  int i;
  int imap_size = 0;
  char tmp_buf[10] = "";

  NSDL2_IMAP(NULL, cptr, "cptr=%p conn state=%d, now = %u", cptr, cptr->conn_state, now);

  int cp_bytes = sprintf(tmp_buf, "%s ", (char*)ns_get_random_str(2, 3, "a-z0-9")); 
  tmp_buf[cp_bytes] = '\0';

  NS_RESET_IOVEC(g_scratch_io_vector);

  NS_FILL_IOVEC(g_scratch_io_vector, tmp_buf, strlen(tmp_buf));
  /* make request */
  NS_FILL_IOVEC(g_scratch_io_vector, IMAP_CMD_LOGOUT, strlen(IMAP_CMD_LOGOUT));

  for (i=0; i < NS_GET_IOVEC_CUR_IDX(g_scratch_io_vector); i++) {
    imap_size += NS_GET_IOVEC_LEN(g_scratch_io_vector, i);
  }

  NS_DT2(NULL, cptr, DM_L1, MM_IMAP, "Sending 'LOGOUT' to Server.");

  cptr->proto_state = ST_IMAP_LOGOUT; /* Next reading state */

  if(cptr->request_type == IMAPS_REQUEST){
    copy_request_into_buffer(cptr, imap_size, &g_scratch_io_vector);
    if(set_socketopt_for_quickack(cptr->conn_fd, 1) < 0)
    {
       retry_connection(cptr, now, NS_REQUEST_CONFAIL);
       return;
    }
    handle_imap_ssl_write (cptr, now);
  }else{
    send_imap_req(cptr, imap_size, &g_scratch_io_vector, now);
  }
}

void imap_send_list(connection *cptr, u_ns_ts_t now)
{
  int i;
  //VUser *vptr = cptr->vptr;
  int imap_size = 0;
  char tmp_buf[10] = "0";

  NSDL2_IMAP(NULL, cptr, "cptr=%p conn state=%d, now = %u", cptr, cptr->conn_state, now);

  int cp_bytes = sprintf(tmp_buf, "%s ", (char*)ns_get_random_str(2, 3, "a-z0-9")); 
  tmp_buf[cp_bytes] = '\0';

  NS_RESET_IOVEC(g_scratch_io_vector);

  NS_FILL_IOVEC(g_scratch_io_vector, tmp_buf, strlen(tmp_buf));
  /* make request */
  NS_FILL_IOVEC(g_scratch_io_vector, IMAP_CMD_LIST, strlen(IMAP_CMD_LIST));

  for (i = 0; i < NS_GET_IOVEC_CUR_IDX(g_scratch_io_vector); i++) {
    imap_size += NS_GET_IOVEC_LEN(g_scratch_io_vector, i);
  }

  NS_DT2(NULL, cptr, DM_L1, MM_IMAP, "Sending 'LIST' to Server.");

  cptr->proto_state = ST_IMAP_LIST; /* Next reading state */

  if(cptr->request_type == IMAPS_REQUEST){
    copy_request_into_buffer(cptr, imap_size, &g_req_rep_io_vector);
    if(set_socketopt_for_quickack(cptr->conn_fd, 1) < 0)
    {
       retry_connection(cptr, now, NS_REQUEST_CONFAIL);
       return;
    }
    handle_imap_ssl_write (cptr, now);
  }else{
    send_imap_req(cptr, imap_size, &g_scratch_io_vector, now);
  }
}

void imap_send_select(connection *cptr, u_ns_ts_t now)
{
  int i;
  int imap_size = 0;
  char tmp_buf[10] = "0";
  NSDL2_IMAP(NULL, cptr, "cptr=%p conn state=%d, now = %u", cptr, cptr->conn_state, now);

  int cp_bytes = sprintf(tmp_buf, "%s ", (char*)ns_get_random_str(2, 3, "a-z0-9")); 
  tmp_buf[cp_bytes] = '\0';

  NS_RESET_IOVEC(g_scratch_io_vector);

  NS_FILL_IOVEC(g_scratch_io_vector, tmp_buf, strlen(tmp_buf));
  /* make request */
  NS_FILL_IOVEC(g_scratch_io_vector, IMAP_CMD_SELECT, strlen(IMAP_CMD_SELECT));

  for (i = 0; i < NS_GET_IOVEC_CUR_IDX(g_scratch_io_vector); i++) {
    imap_size += NS_GET_IOVEC_LEN(g_scratch_io_vector, i);
  }

  NS_DT2(NULL, cptr, DM_L1, MM_IMAP, "Sending 'SELECT' to Server.");

  cptr->proto_state = ST_IMAP_SELECT; /* Next reading state */

  if(cptr->request_type == IMAPS_REQUEST){
    copy_request_into_buffer(cptr, imap_size, &g_scratch_io_vector);
    if(set_socketopt_for_quickack(cptr->conn_fd, 1) < 0)
    {
       retry_connection(cptr, now, NS_REQUEST_CONFAIL);
       return;
    }
    handle_imap_ssl_write (cptr, now);
  }else{
    send_imap_req(cptr, imap_size, &g_scratch_io_vector, now);
  }
}

void imap_process_fetch(connection *cptr, u_ns_ts_t now, char *buf, int bytes_read)
{
  int req_code   = cptr->req_code;
  VUser *vptr = (VUser *)cptr->vptr;
  
  NSDL2_IMAP(NULL, cptr, "conn state=%d, req_code = %s, buf = %s, bytes_read = %d, now = %u",
             cptr->conn_state, 
             cptr->req_code == IMAP_ERR ? "BAD" : "OK", 
             buf, bytes_read, now);

  if (req_code == IMAP_OK) {
    copy_url_resp(cptr);
    /* we have to copy it here since we are going to use do_data_processing
     * in SEARCH/FETCH/LIST cases in IMAP */
    copy_cptr_to_vptr(vptr, cptr); // Save buffers from cptr to vptr etc 

    /* Process vars on it. (checkpoint and others) */
    do_data_processing(cptr, now, VAR_IGNORE_REDIRECTION_DEPTH, 1);
    cptr->bytes = vptr->bytes = 0;
    url_resp_buff[0] = 0;
    
    NSDL2_IMAP(NULL, cptr, "Going to send logout, cptr->bytes = %d, vptr->bytes = %d\n", 
               cptr->bytes, vptr->bytes);

      imap_send_logout(cptr, now);
  } else if (req_code == IMAP_ERR) {
    cptr->req_ok = NS_REQUEST_BAD_RESP;
    imap_send_logout(cptr, now);
  }
}

void imap_process_select(connection *cptr, u_ns_ts_t now, char *buf, int bytes_read)
{
  int req_code   = cptr->req_code;
  VUser *vptr = (VUser *)cptr->vptr;
  
  NSDL2_IMAP(NULL, cptr, "conn state=%d, req_code = %s, buf = %s, bytes_read = %d, now = %u",
             cptr->conn_state, 
             cptr->req_code == IMAP_ERR ? "BAD" : "OK", 
             buf, bytes_read, now);

  if (req_code == IMAP_OK) {
    copy_url_resp(cptr);
    /* we have to copy it here since we are going to use do_data_processing
     * in SEARCH/FETCH/LIST cases in IMAP */
    copy_cptr_to_vptr(vptr, cptr); // Save buffers from cptr to vptr etc 

    /* Process vars on it. (checkpoint and others) */
    do_data_processing(cptr, now, VAR_IGNORE_REDIRECTION_DEPTH, 1);
    cptr->bytes = vptr->bytes = 0;
    url_resp_buff[0] = 0;
    
    NSDL2_IMAP(NULL, cptr, "Going to send quit, cptr->bytes = %d, vptr->bytes = %d\n", 
               cptr->bytes, vptr->bytes);

    if(cptr->url_num->proto.imap.imap_action_type == IMAP_SELECT){
      imap_send_logout(cptr, now);
    }
    else if(cptr->url_num->proto.imap.imap_action_type == IMAP_FETCH){
      imap_send_fetch(cptr, now);
    }
    else if(cptr->url_num->proto.imap.imap_action_type == IMAP_SEARCH){
     // imap_send_search(cptr, now);
    }

  } else if (req_code == IMAP_ERR) {
    cptr->req_ok = NS_REQUEST_BAD_RESP;
    imap_send_logout(cptr, now);
  }
}

void imap_process_list(connection *cptr, u_ns_ts_t now, char *buf, int bytes_read)
{
  int req_code   = cptr->req_code;
  VUser *vptr = (VUser *)cptr->vptr;

  NSDL2_IMAP(NULL, cptr, "conn state=%d, req_code = %s, buf = %s, bytes_read = %d, now = %u",
             cptr->conn_state, 
             cptr->req_code == IMAP_ERR ? "BAD" : "OK", 
             buf, bytes_read, now);

  //TODO: based on imap action type send request, for now only handling select type
  if (req_code == IMAP_OK){
    copy_url_resp(cptr);
    /* we have to copy it here since we are going to use do_data_processing
     * in SEARCH/FETCH/LIST cases in IMAP */
    copy_cptr_to_vptr(vptr, cptr); // Save buffers from cptr to vptr etc 

    /* Process vars on it. (checkpoint and others) */
    do_data_processing(cptr, now, VAR_IGNORE_REDIRECTION_DEPTH, 1);
    cptr->bytes = vptr->bytes = 0;
    url_resp_buff[0] = 0;
    
    NSDL2_IMAP(NULL, cptr, "Going to send logout, cptr->bytes = %d, vptr->bytes = %d\n", 
               cptr->bytes, vptr->bytes);
    imap_send_logout(cptr, now);
  }else if(req_code == IMAP_ERR) {
    cptr->req_ok = NS_REQUEST_BAD_RESP;
    imap_send_logout(cptr, now);
  }
}

void imap_process_login(connection *cptr, u_ns_ts_t now, char *buf, int bytes_read)
{
  int req_code   = cptr->req_code;
  VUser *vptr = (VUser*)cptr->vptr;
  
  NSDL2_IMAP(NULL, cptr, "conn state=%d, req_code = %s, buf = %s, bytes_read = %d, now = %u",
             cptr->conn_state, 
             cptr->req_code == IMAP_ERR ? "BAD" : "OK", 
             buf, bytes_read, now);

  cptr->bytes = vptr->bytes = 0;
  //TODO: based on imap action type send request, for now only handling select type
  if (req_code == IMAP_OK){
    if(cptr->url_num->proto.imap.imap_action_type == IMAP_SELECT || cptr->url_num->proto.imap.imap_action_type == IMAP_SEARCH || cptr->url_num->proto.imap.imap_action_type == IMAP_FETCH){
      imap_send_select(cptr, now);
    }
    else if(cptr->url_num->proto.imap.imap_action_type == IMAP_LIST){
      imap_send_list(cptr, now);
    }
    else if(cptr->url_num->proto.imap.imap_action_type == IMAP_DELETE){
    }
    else{
      imap_send_logout(cptr, now);
    }
  }
  else{ 
    Close_connection(cptr, 0, now, NS_REQUEST_BAD_RESP, NS_COMPLETION_IMAP_ERROR);
  }
    
}
void imap_process_starttls(connection *cptr, u_ns_ts_t now, char *buf, int bytes_read)
{
  int req_code   = cptr->req_code;
  
  NSDL2_IMAP(NULL, cptr, "conn state=%d, req_code = %s, buf = %s, bytes_read = %d, now = %u",
             cptr->conn_state, 
             cptr->req_code == IMAP_ERR ? "BAD" : "OK", 
             buf, bytes_read, now);

  if (req_code == IMAP_OK){
    cptr->request_type = IMAPS_REQUEST;
    cptr->proto_state = ST_IMAP_TLS_LOGIN;
    cptr->conn_state = CNST_CONNECTING;
    
    handle_connect(cptr, now, 0);
  }
  else {
    Close_connection(cptr, 0, now, NS_REQUEST_BAD_RESP, NS_COMPLETION_IMAP_ERROR);
  }
}

void imap_process_handshake(connection *cptr, u_ns_ts_t now, char *buf, int bytes_read, int start_tls)
{
  int req_code   = cptr->req_code;
  
  NSDL2_IMAP(NULL, cptr, "conn state=%d, req_code = %s, buf = %s, bytes_read = %d, now = %u",
             cptr->conn_state, 
             cptr->req_code == IMAP_ERR ? "BAD" : "OK", 
             buf, bytes_read, now);

  if (req_code == IMAP_OK){
    if(start_tls){
      imap_send_starttls(cptr, now);
    }else{
      imap_send_login(cptr, now);
    }
  }
  else {
    Close_connection(cptr, 0, now, NS_REQUEST_BAD_RESP, NS_COMPLETION_IMAP_ERROR);
  }
}

