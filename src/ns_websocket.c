/************************************************************************************
 * Name	     : ns_websocket.c 
 * Purpose   : This file contains functions related to WebSocket Protocol 
 * Author(s) : Manish Kumar Mishra 
 * Date      : 18 August 2015 
 * Copyright : (c) Cavisson Systems
 * Modification History :
 ***********************************************************************************/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>
#include <strings.h>
#include <openssl/sha.h>

#include "url.h"
#include "util.h"
#include "ns_common.h"
#include "ns_global_settings.h"
#include "ns_log.h"
#include "ns_script_parse.h"
#include "ns_http_process_resp.h"
#include "ns_log_req_rep.h"
#include "ns_websocket.h"
#include "ns_string.h"
#include "ns_url_req.h"
#include "ns_vuser_tasks.h"
#include "ns_http_script_parse.h"
#include "ns_page_dump.h"
#include "ns_vuser_thread.h" 
#include "nslb_encode_decode.h"
#include "netstorm.h"
#include "ns_sock_com.h"
#include "ns_debug_trace.h"
#include "ns_trace_level.h"
#include "ns_websocket_reporting.h"
#include "ns_group_data.h"
#include "ns_exit.h"

/*********Global Variables****************/
int max_ws_send_entries =0;
int total_ws_send_entries =0;
int total_ws_close_entries =0;
int max_ws_close_entries =0;
int max_ws_callback_entries =0;
int total_ws_callback_entries =0;
int uran_fd = -1;
int max_ws_conn = 0;     //It is used for malloc ws_conn which is used at send API
unsigned short int ws_idx_list[65535];
char *str;    //contain raw string which is encoded and generate key
char *ws_send_buffer = NULL;
int  ws_send_buff_len = 0;
char *ws_send_frame = NULL;
unsigned int g_max_frame_size = 0;
char rand_string_data[20]; 

ws_callback *g_ws_callback;
ws_send_table *g_ws_send;
ws_close_table *g_ws_close;
ws_callback_shr *g_ws_callback_shr;
ws_send_table_shr *g_ws_send_shr;
ws_close_table_shr *g_ws_close_shr;
#ifndef CAV_MAIN
action_request* requests;
int cur_post_buf_len;
#else
extern __thread action_request* requests;
extern __thread int cur_post_buf_len;
#endif
char *ws_http_version_buffers = " HTTP/1.1\r\n";
char WS_CRLFString[3] = "\r\n";
int WS_CRLFString_Length = 2;

char *ws_Host_header_buf = "Host: ";
char *method_name = "GET ";
char *upgrade_buf = "Upgrade: websocket\r\n";
char *ws_connection_buf = "Connection: Upgrade\r\n";
char *sec_ws_key_buf = "Sec-WebSocket-Key: ";
char *sec_ws_protocol_buf = "Sec-WebSocket-Protocol: chat, superchat\r\n";
char *sec_ws_protocol_ext_buf = "Sec-WebSocket-Extensions:permessage-deflate; client_max_window_bits\r\n";
char *sec_ws_version_buf = "Sec-WebSocket-Version: 13\r\n";
char *ws_agent_buf = "User-Agent: ";
char *ws_accept_enc_buf = "Accept-Encoding: gzip, deflate, sdch\r\n";
struct iovec *vector;


void ws_resp_timeout(ClientData client_data, u_ns_ts_t now) 
{
  connection *cptr;
  cptr = (connection *)client_data.p;
  VUser *vptr = cptr->vptr;
  timer_type* tmr = cptr->timer_ptr;

  NSDL2_WS(NULL, cptr, "Method Called, cptr = %p conn state = %d now = %llu", 
                            cptr, cptr->conn_state, now);
 
  if(vptr->vuser_state == NS_VUSER_PAUSED){
    vptr->flags |= NS_VPTR_FLAGS_TIMER_PENDING;
    return;
  }

  vptr->ws_status = NS_REQUEST_TIMEOUT;
  ws_avgtime->ws_error_codes[NS_REQUEST_TIMEOUT]++;

  NSDL2_WS(NULL, NULL, "ws_avgtime->ws_error_codes[%d] = %llu, "
                       "timer: [timer_type, timeout, actual_timeout, timer_status] = [%d, %llu, %d, %d]", 
                        vptr->ws_status, ws_avgtime->ws_error_codes[NS_REQUEST_TIMEOUT],
                        tmr->timer_type, tmr->timeout, tmr->actual_timeout, tmr->timer_status);


  if(tmr->timer_type == AB_TIMEOUT_IDLE) 
  {
    NSDL2_WS(NULL, cptr, "Deleting Idle timer of Frame reading.");
    dis_timer_del(tmr);
  }   
  else
  {
    NSDL2_WS(NULL, cptr, "Code should not come in this lag");
  }

  cptr->conn_state = CNST_WS_IDLE;

  if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT)
    switch_to_vuser_ctx(vptr, "Switching to VUser context: ws_resp_timeout()"); 
  else if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_SEPARATE_THREAD)
    send_msg_nvm_to_vutd(vptr, NS_API_WEBSOCKET_READ_REP, 0); 
  else if(vptr->sess_ptr->script_type == NS_SCRIPT_TYPE_JAVA)
    send_msg_to_njvm(vptr->mcctptr, NS_NJVM_API_WEB_WEBSOCKET_READ_REP, 0);
}

void websocket_close_connection(connection* cptr, int done, u_ns_ts_t now, int req_ok, int completion) 
{
  int url_ok = 0;
  int status;
  int redirect_flag = 0;
  int request_type;
  char taken_from_cache = 0; // No
  VUser *vptr = cptr->vptr;

  NSDL2_WS(vptr, cptr, "Method called. cptr = %p, done = %d, req_ok = %d, completion = %d", cptr, done, req_ok, completion);

  //action_request_Shr *url_num = get_top_url_num(cptr);
  request_type = cptr->request_type;
  
  if(!cptr->request_complete_time)
    cptr->request_complete_time = now;

  //req_ok manually because in ws no page concept
  status = cptr->req_ok = req_ok;
  url_ok = !status;

  NSDL2_WS(vptr, cptr, "request_type %d, urls_awaited = %d, done = %d", request_type, vptr->urls_awaited, done);

  if(done)
  {
    cptr->num_retries = 0;
    if(vptr->urls_awaited)
      vptr->urls_awaited--;
    NS_DT3(vptr, cptr, DM_L1, MM_CONN, "Completed %s session with server %s",
         get_req_type_by_name(request_type),
         nslb_sock_ntop((struct sockaddr *)&cptr->cur_server));

    FREE_AND_MAKE_NULL(cptr->data, "Freeing cptr data", -1);

    //Updating WebSocket response status and error codes graph
    UPDATE_WS_RESP_STATUS_AND_ERR_GRPH
 }

  /* Do not close fd in WebSocket Handshake */
  //if(completion == NS_COMPLETION_CLOSE)
  if((cptr->conn_fd > 0) && ((cptr->conn_state != CNST_WS_READING) || (status != NS_REQUEST_OK)))
    close_fd(cptr, done, now);
 
  if (status != NS_REQUEST_OK) { //if page not success
     NSDL2_WS(vptr, cptr, "aborting on main status=%d", status);
     abort_bad_page(cptr, status, redirect_flag);
     vptr->ws_status = status;
     if(status == NS_REQUEST_BAD_HDR) 
     {
       vptr->ws_status = NS_REQUEST_BAD_HDR;
       ws_avgtime->ws_error_codes[NS_REQUEST_BAD_HDR]++;     //WSFailureStats Graph 
       NSDL2_WS(NULL, NULL, "ws_avgtime->ws_error_codes[%d] = %llu", vptr->ws_status, ws_avgtime->ws_error_codes[status]);
     }
  }

  /* TODO: Need to proper handle this setting 1 just to do_data_processing */
  if(completion != NS_COMPLETION_CLOSE)
  {
    vptr->redirect_count = 1;

    handle_url_complete(cptr, request_type, now, url_ok,
		        redirect_flag, status, taken_from_cache); 
  }

  if(cptr->conn_state == CNST_WS_READING)
  {
    NSDL2_WS(NULL, NULL, "Setting cptr->conn_state from CNST_WS_READING to CNST_WS_IDLE as WS handshake done");
    cptr->conn_state = CNST_WS_IDLE;
  }

  //TODO: Handle Page complete
  //if(!done)
    //do_data_processing(cptr, now, VAR_IGNORE_REDIRECTION_DEPTH, 1);

  //#if 0
  /* Only Last will be handled here */
  if (!vptr->urls_awaited && (completion != NS_COMPLETION_CLOSE)) {
    handle_page_complete(cptr, vptr, done, now, request_type);
  } else {
    NSDL2_CONN(NULL, cptr, "Handle handle_page_complete() not called as completion code is %d", completion);  // Error LOG
  }
  //#endif

  if ((cptr->conn_state == CNST_FREE) && (completion == NS_COMPLETION_CLOSE)) {
    if (cptr->list_member & NS_ON_FREE_LIST) {
      NSTL1(vptr, cptr, "Connection slot is already in free connection list");
    } else {
      /* free_connection_slot remove connection from either or both reuse or inuse link list*/
      NSDL2_WS(vptr, cptr, "Connection state is free, need to call free_connection_slot");
      free_connection_slot(cptr, now);
   }
   /* Change the context accordingly and send the response */
   if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT)
     switch_to_vuser_ctx(vptr, "WebSocketClose: websocket_close_connection()");
   else if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_SEPARATE_THREAD)
     send_msg_nvm_to_vutd(vptr, NS_API_WEBSOCKET_CLOSE_REP, 0);
   else if(vptr->sess_ptr->script_type == NS_SCRIPT_TYPE_JAVA)
     send_msg_to_njvm(vptr->mcctptr, NS_NJVM_API_WEB_WEBSOCKET_CLOSE_REP, 0);
  }
}

int ws_set_post_body(int send_tbl_idx, int sess_idx, int *script_ln_no, char *cap_fname)
{
  char *fname, fbuf[8192];
  int ffd, rlen, noparam_flag = 0;

  NSDL2_PARSING(NULL, NULL, "Method called, send_tbl_idx = %d, sess_idx = %d", send_tbl_idx, sess_idx);

  if (cur_post_buf_len <= 0) return NS_PARSE_SCRIPT_SUCCESS; //No BODY, exit

  //Removing traing ,\n from post buf.

  if(gSessionTable[sess_idx].script_type == NS_SCRIPT_TYPE_LEGACY)
  {
    validate_body_and_ignore_last_spaces();
  }
  else
  {
    if(validate_body_and_ignore_last_spaces_c_type(sess_idx) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;
  }

  //Check if BODY is provided using $CAVINCLUDE$= directive
  if((strncasecmp (g_post_buf, "$CAVINCLUDE$=", 13) == 0) || (strncasecmp (g_post_buf, "$CAVINCLUDE_NOPARAM$=", 21) == 0)) {

   if(strncasecmp (g_post_buf, "$CAVINCLUDE_NOPARAM$=", 21) == 0)
   {
      fname = g_post_buf + 21;
      noparam_flag = 1;
   }
   else
     fname = g_post_buf + 13;

      if (fname[0] != '/') {
          /*bug id: 101320: using g_ns_ta_dir instead of g_ns_wdir, avoid using hardcoded scripts dir*/
          sprintf (fbuf, "%s/%s/%s", GET_NS_TA_DIR(),
                   get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), sess_idx, "/"), fname);
                   //Previously taking with only script name
                   //get_sess_name_with_proj_subproj(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name)), fname);
          fname = fbuf;
      }

      NSDL2_WS(NULL, NULL, "fbuf = %s", fbuf);
      ffd = open (fname, O_RDONLY|O_CLOEXEC);
      if (!ffd) {
          NSDL4_WS("%s() : Unable to open $CAVINCLUDE$ file %s\n", (char*)__FUNCTION__, fname);
          return NS_PARSE_SCRIPT_ERROR;
      }
      cur_post_buf_len = 0;
      while (1) {
          rlen = read (ffd, fbuf, 8192);
          if (rlen > 0) {
            if (copy_to_post_buf(fbuf, rlen)) {
              NSDL4_WS(NULL, NULL,"%s(): Request BODY could not alloccate mem for %s\n", (char*)__FUNCTION__, fname);
              return NS_PARSE_SCRIPT_ERROR;
            }
            continue;
          } else if (rlen == 0) {
              break;
          } else {
              perror("reading CAVINCLUDE BODY");
              NSDL4_WS(NULL, NULL, "%s(): Request BODY could not read %s\n", (char*)__FUNCTION__, fname);
              return NS_PARSE_SCRIPT_ERROR;
          }
      }
      close (ffd);
  }
  if (cur_post_buf_len) 
  {
    save_and_segment_ws_body(send_tbl_idx,
                        g_post_buf, noparam_flag, script_ln_no,
                        sess_idx, cap_fname);
    NSDL2_WS(NULL, NULL, "Send Table data at ws_send_id = %d, conn_id = %d, send_buf.seg_start = %lu, send_buf.num_ernties = %d, "
                         "isbinary = %d, ws_idx = %d", 
                                       send_tbl_idx, g_ws_send[send_tbl_idx].id, 
                                       g_ws_send[send_tbl_idx].send_buf.seg_start, g_ws_send[send_tbl_idx].send_buf.num_entries, 
                                       g_ws_send[send_tbl_idx].isbinary, g_ws_send[send_tbl_idx].ws_idx); 
  }
  return NS_PARSE_SCRIPT_SUCCESS; 
}

//ws_api_flag = 0 means send api, 1 means close api
int ns_websocket_ext(VUser *vptr, int ws_api_id, int ws_api_flag)
{
  char msg[512];
  NSDL2_API(vptr, NULL, "Method called. api_id = %d, flag = %d", ws_api_id, ws_api_flag);
  
   /* Below code should executed for USER CONTEXT MODE and JAVA Type scripts */

    NSDL2_API(vptr, NULL, "ws_api_flag = %d, vptr->ws_status = %d", ws_api_flag, vptr->ws_status);
   if(ws_api_flag == 0) //send api
      vut_add_task(vptr, VUT_WS_SEND);
    else if(ws_api_flag == 1) //close api 
      vut_add_task(vptr, VUT_WS_CLOSE);

  if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT)
  {
  /*  if(ws_api_flag == 0) //send api
      vut_add_task(vptr, VUT_WS_SEND);
    else if(ws_api_flag == 1) //close api 
      vut_add_task(vptr, VUT_WS_CLOSE);*/

    sprintf(msg, "%s", (ws_api_flag == 0)?"ns_websocket_ext(): waiting for VUT_WS_SEND to complete":
                                          "ns_websocket_ext(): waiting for VUT_WS_CLOSE to complete");
    switch_to_nvm_ctx(vptr, msg);
    
//    NSDL2_API(vptr, NULL, "ws_api_flag = %d, vptr->ws_status = %d", ws_api_flag, vptr->ws_status);
    return vptr->ws_status;
  }
  #if 0
  else if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_SEPARATE_THREAD)
  {
    NSDL2_API(vptr, NULL, "Sending message to NVM and waiting for reply, send_id = %d", send_id);
    /*static int tlt_no_url = 0; NSTL1_OUT(NULL, NULL, "total no url = %d\n", ++tlt_no_url);*/
    Ns_web_url_req web_url;
    web_url.opcode = NS_API_WEB_URL_REQ;
    //web_url.page_id = send_id;
    int ret = vutd_send_msg_to_nvm(VUT_WS_SEND, (char *)(&web_url), sizeof(Ns_web_url_req));
    NSDL2_API(vptr, NULL, "take the response and take response send_id = %d", send_id);
    return (ret);
  }
  #endif
  return 0;
}

static inline int get_values_from_segments(connection *cptr, char **buffer, int *buffer_len, StrEnt_Shr* seg_tab_ptr, char *for_which, int max, int req_part)
{
  int i, total_len = 0;
  int ret, buf_len = 0;
  char *to_fill = NULL;
  VUser *vptr = cptr->vptr;

  NSDL2_WS(vptr, cptr, "Method Called, for_which = %s, max = %d, req_part = %d", for_which, max, req_part);

  NS_RESET_IOVEC(g_scratch_io_vector);

  // Get all segment values in a vector
  // Note that some segment may be parameterized
  if ((ret = insert_segments(vptr, cptr, seg_tab_ptr, &g_scratch_io_vector, NULL, 0, 1, req_part, cptr->url_num, SEG_IS_NOT_REPEAT_BLOCK)) < 0){
    NSEL_MAJ(NULL, NULL, ERROR_ID, ERROR_ATTR,
        "Error in insert_segments() for %s, return value = %d\n", for_which, NS_GET_IOVEC_CUR_IDX(g_scratch_io_vector));

     if(ret == MR_USE_ONCE_ABORT)
       return ret;

     return(-1);
  }

  // Calculate total lenght of all components which are in vector
  for (i = 0; i < NS_GET_IOVEC_CUR_IDX(g_scratch_io_vector); i++) {
    total_len += g_scratch_io_vector.vector[i].iov_len;
  }

  NSDL2_WS(vptr, cptr, "total_len = %d, next_idx = %d", total_len, g_scratch_io_vector.cur_idx);

  if(g_scratch_io_vector.cur_idx <= 0 || g_scratch_io_vector.cur_idx > max) {
    NSEL_MAJ(NULL, NULL, ERROR_ID, ERROR_ATTR,
        "Total length (%d) of %s is either 0 or > than max (%d) value",
        total_len, for_which, max);
    NS_FREE_RESET_IOVEC(g_scratch_io_vector); 
    return -1;
  }

  for (i = 0; i < g_scratch_io_vector.cur_idx; i++) {
    buf_len += g_scratch_io_vector.vector[i].iov_len;
  }

  NSDL2_WS(vptr, cptr, "buffer_len = %d , buf_len = %d", *buffer_len, buf_len);
  if(*buffer == NULL || (*buffer_len <= buf_len))
  {
    //+128 bytes extra memory just to maintain minium memory allocation
    *buffer_len += buf_len;
    MY_REALLOC(*buffer, *buffer_len, "Realocate memory to ws_send_buffer/ws_uri_last_part", -1);
  }

  //Memset buffer before reuse
  memset(*buffer, 0, *buffer_len);

  to_fill = *buffer;
  for (i = 0; i < g_scratch_io_vector.cur_idx; i++) {
    bcopy(g_scratch_io_vector.vector[i].iov_base, to_fill, g_scratch_io_vector.vector[i].iov_len);
    to_fill += g_scratch_io_vector.vector[i].iov_len;
  }
  *to_fill = 0; // NULL terminate

  NSDL2_WS(vptr, cptr, "Concated value = [%s], [%p]", *buffer, *buffer);

  NS_FREE_RESET_IOVEC(g_scratch_io_vector); 
  NSDL2_WS(vptr, cptr, "Returning from get_values_from_segments, buffer = %p", *buffer);
  return total_len;
}
#if 0
int Base64Encode1(const char *in, int in_len, char *out, int out_size)
{
  char encode[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                               "abcdefghijklmnopqrstuvwxyz0123456789+/";
  unsigned char triple[3];
  int i;
  int len;
  int line = 0;
  int done = 0;

  while (in_len) {
    len = 0;
    for (i = 0; i < 3; i++) {
      if (in_len) {
        triple[i] = *in++;
        len++;
        in_len--;
      } else
        triple[i] = 0;
    }

    if (done + 4 >= out_size)
      return -1;

    *out++ = encode[triple[0] >> 2];
    *out++ = encode[((triple[0] & 0x03) << 4) |
                               ((triple[1] & 0xf0) >> 4)];
    *out++ = (len > 1 ? encode[((triple[1] & 0x0f) << 2) |
                               ((triple[2] & 0xc0) >> 6)] : '=');
    *out++ = (len > 2 ? encode[triple[2] & 0x3f] : '=');
    done += 4;
    line += 4;
  }

  if (done + 1 >= out_size)
    return -1;
  *out++ = '\0';
  return done;
}
#endif
void debug_log_ws_req(connection *cptr, char *buf, int bytes_to_log, int complete_data, int first_trace_write_flag)
{
  VUser* vptr = cptr->vptr;
  int request_type = cptr->url_num->request_type;

  if (!((runprof_table_shr_mem[vptr->group_num].gset.debug & DM_LOGIC4) &&
        (request_type == WS_REQUEST &&
         (runprof_table_shr_mem[vptr->group_num].gset.module_mask & MM_HTTP))))
    return;

  if((global_settings->replay_mode)) return;

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
  sprintf(log_file, "%s/logs/%s/ws_req_%hd_%u_%u_%d_0_%d_%d_%d_0.dat",
                    g_ns_wdir, req_rep_file_path, child_idx, vptr->user_index,
                    vptr->sess_inst, vptr->page_instance, vptr->group_num,
                    GET_SESS_ID_BY_NAME(vptr),
                    GET_PAGE_ID_BY_NAME(vptr));

  // Do not change the debug trace message as it is parsed by GUI
  if(first_trace_write_flag)
    NS_DT4(vptr, cptr, DM_L1, MM_SMTP, "Request is in file '%s'", log_file);

  log_fp = fopen(log_file, "a+");
  if (log_fp == NULL)
  {
    NSTL1_OUT(NULL, NULL, "Unable to open file %s. err = %s\n", log_file, nslb_strerror(errno));
    return;
  }

  //write for both ssl and non ssl url
  if(fwrite(buf, bytes_to_log, 1, log_fp) != 1)
  {
    NSTL1_OUT(NULL, NULL, "Error: Can not write to url request file. err = %s, bytes_to_log = %d, buf = %s\n", nslb_strerror(errno), bytes_to_log, buf);
    return;
  }
  if (complete_data) fwrite(line_break, strlen(line_break), 1, log_fp);

  if(fclose(log_fp) != 0)
  {
    NSTL1_OUT(NULL, NULL, "Unable to close url request file. err = %s\n", nslb_strerror(errno));
    return;
  }
}

void log_ws_res(connection *cptr, VUser *vptr, char *buf, int size)
{ 
  char log_file[1024];
  int log_fd;

  NSDL3_HTTP(NULL, NULL, "Method Called, buf = [%s], size = [%d]", buf, size);  
  SAVE_REQ_REP_FILES

  sprintf(log_file, "%s/logs/%s/ws_rep_%hd_%u_%u_%d_0_%d_%d_%d_0.dat",
          g_ns_wdir, req_rep_file_path, child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,
          vptr->group_num, GET_SESS_ID_BY_NAME(vptr),
          GET_PAGE_ID_BY_NAME(vptr));

  NSDL3_HTTP(NULL, NULL, "GET_SESS_ID_BY_NAME(vptr) = %u, GET_PAGE_ID_BY_NAME(vptr) = %u", GET_SESS_ID_BY_NAME(vptr), GET_PAGE_ID_BY_NAME(vptr));
  // Do not change the debug trace message as it is parsed by GUI
  if(cptr->tcp_bytes_recv == 0)  //This check to print debug trace only once
    NS_DT4(vptr, cptr, DM_L1, MM_HTTP, "Response is in file '%s'", log_file);

  if((log_fd = open(log_file, O_CREAT|O_CLOEXEC|O_WRONLY|O_APPEND, 00666)) < 0)
  {
    NSTL1_OUT(NULL, NULL, "Error: Error in opening file for logging URL request\n");
  }
  else
  {
    if(cptr->tcp_bytes_recv == 0)  //This check to print debug trace only once
    {
      write(log_fd, cptr->url, cptr->url_len);
      write(log_fd, "\n", 1);
    }
    write(log_fd, buf, size);
    close(log_fd);
  }
}

void send_ws_connect_req(connection *cptr, int ws_size, u_ns_ts_t now)
{
  IW_UNUSED(VUser *vptr = cptr->vptr);

  NSDL2_WS(vptr, cptr, "Method Called, ws_size = %d, num_vectors = %d, cptr->conn_fd = %d", 
                        ws_size, g_req_rep_io_vector.cur_idx, cptr->conn_fd);
  send_http_req(cptr, ws_size, &g_req_rep_io_vector, now);
  NSDL2_WS(vptr, cptr, "WS Connect request send succfully");
}

/**************************************************************************************
Function      : make_ws_request
Purpose       : 
Request Format: GET tours/index.html HTTP/1.1\r\n
     		HOST: 10.10.70.5:8002\r\n
		Upgrade: websocket\r\n
		Connection: Upgrade\r\n
		Sec-WebSocket-Key: CfxdFfe23932vDSrec==\r\n		 
  		Sec-WebSocket-Protocol: chat, superchat\r\n
	        Sec-WebSocket-Version: 13\r\n
		Origin: http://google.com\r\n 
                User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 
			    (KHTML, like Gecko) Chrome/41.0.2272.118 Safari/537.36\r\n
                Accept-Encoding: gzip, deflate, sdch\r\n		
                \r\n	
***************************************************************************************/
int make_ws_request(connection* cptr, int *ws_req_size, u_ns_ts_t now)
{
  action_request_Shr* request = cptr->url_num;
  PerHostSvrTableEntry_Shr* svr_entry;
  VUser* vptr = cptr->vptr;
  char buf[128];
  char *str;
  int n, ret;
  int use_rec_host = runprof_table_shr_mem[vptr->group_num].gset.use_rec_host;
  int disable_headers = runprof_table_shr_mem[vptr->group_num].gset.disable_headers;
  int len;

  //Fill Method name
  NSDL2_WS(vptr, cptr, "Method Called, cptr = %p, vptr = %p, user_index = %d",  cptr, vptr, vptr->user_index);

  NS_RESET_IOVEC(g_req_rep_io_vector);
  NS_FILL_IOVEC(g_req_rep_io_vector, method_name, METHOD_LENGTH);
  *ws_req_size += METHOD_LENGTH;

  //get server name 
  svr_entry = get_svr_entry(vptr, cptr->url_num->index.svr_ptr);

  NSDL2_WS(vptr, cptr, "cptr->url_num->proto.ws.uri = %p, vptr->httpData->ws_uri_last_part = %p", 
                        cptr->url_num->is_url_parameterized? cptr->url_num->proto.ws.uri_without_path: cptr->url_num->proto.ws.uri, vptr->httpData->ws_uri_last_part);
  if((vptr->httpData->ws_uri_last_part == NULL) || 
     (vptr->httpData->ws_client_base64_encoded_key == NULL) || 
     (vptr->httpData->ws_expected_srever_base64_encoded_key == NULL))
  {
    NSDL2_WS(vptr, cptr, "Allocating memory for websocket request members ..");
    MY_MALLOC_AND_MEMSET(vptr->httpData->ws_client_base64_encoded_key, 40, "vptr->httpData->ws_client_base64_encoded_key", -1);
    MY_MALLOC_AND_MEMSET(vptr->httpData->ws_expected_srever_base64_encoded_key, 30, "vptr->httpData->ws_expected_srever_base64_encoded_key", -1);
  }
  else
  {
    memset(vptr->httpData->ws_client_base64_encoded_key, 0, 40);
    memset(vptr->httpData->ws_expected_srever_base64_encoded_key, 0, 30);
  }
  if(!cptr->url_num->is_url_parameterized)
  {
    if((len = get_values_from_segments(cptr, &vptr->httpData->ws_uri_last_part, &vptr->httpData->ws_uri_last_part_len, &cptr->url_num->proto.ws.uri, "uri", 1024, REQ_PART_REQ_LINE)) <= 0){
      NSTL1_OUT(NULL, NULL, "Error in getting value from segment for object id.\n");
      end_test_run();
    }

    NSDL2_WS(vptr, cptr, "cptr->url_num->proto.ws.uri.seg_start = %p, vptr->httpData->ws_uri_last_part = %s", 
                          cptr->url_num->proto.ws.uri.seg_start, vptr->httpData->ws_uri_last_part);
    //Bug 34151 - WebSocket | Getting core dump when AUTO_COOKIE keyword is using in scenario.
    cptr->url_len = strlen(vptr->httpData->ws_uri_last_part);
    MY_REALLOC(cptr->url, cptr->url_len + 1, "cptr->url_len", -1);
    strcpy(cptr->url, vptr->httpData->ws_uri_last_part);
  }
  NSDL2_WS(vptr, cptr, "cptr->url = %s, cptr->url_len = %d, cur_idx = %d", cptr->url, cptr->url_len, g_req_rep_io_vector.cur_idx);

  if(cptr->url != NULL) {

    if(cptr->flags & NS_CPTR_FLAGS_CON_USING_PROXY)
    {
      NSDL3_HTTP(vptr, cptr, "Filling url (proxy case)");
      //We will make fully qualified url if we are using Proxy and -
      // 1) Request is ws and G_PROXY_PROTO_MODE is 1 OR  
      // 2) Request is wss and G_PROXY_PROTO_MODE is 1 
      // In case of proxy, we need to send fully qualified URL e.g. http://www.cavisson.com:8080/index.html
      // Here host:port is the host/port of the Origin Server (Not proxy server)

      // Step1b.1: Fill schema part of URL (ws:// or wss://)
      if(cptr->url_num->request_type == WS_REQUEST)
      {
        NS_FILL_IOVEC(g_req_rep_io_vector, WS_REQUEST_STR, WS_REQUEST_STR_LEN);
        *ws_req_size += WS_REQUEST_STR_LEN;
      }
      else if(cptr->url_num->request_type == WSS_REQUEST)
      {
        NS_FILL_IOVEC(g_req_rep_io_vector, WSS_REQUEST_STR, WSS_REQUEST_STR_LEN);
        *ws_req_size += WSS_REQUEST_STR_LEN;
      }

      // Step1b.2: Fill host part of URL Eg: 192.168.1.66:8080. Host is actual host (Origin Server)
      // Port will be filled if it not default
      NS_FILL_IOVEC(g_req_rep_io_vector, svr_entry->server_name, svr_entry->server_name_len); 
      *ws_req_size += svr_entry->server_name_len;
    } 
    // Step1b.3: Fill url path of URL e.g. /test/index.html?a=b&c=d
    NS_FILL_IOVEC(g_req_rep_io_vector, cptr->url, cptr->url_len);
    *ws_req_size += cptr->url_len;
  }
  else {
    NSTL1_OUT(NULL, NULL, "make_ws_request(): URL is null..");
    return -1;
  }

  //HTTP version 
  NS_FILL_IOVEC(g_req_rep_io_vector, ws_http_version_buffers, WS_HTTP_VERSION_STRING_LENGTH);
  *ws_req_size += WS_HTTP_VERSION_STRING_LENGTH;

  //insert host header
  if (!(disable_headers & NS_HOST_HEADER)) {
    NS_FILL_IOVEC(g_req_rep_io_vector, ws_Host_header_buf, HOST_HEADER_STRING_LENGTH);
    *ws_req_size += HOST_HEADER_STRING_LENGTH;

    if (use_rec_host == 0) //Send actual host (mapped)
    {
      NS_FILL_IOVEC(g_req_rep_io_vector, svr_entry->server_name, svr_entry->server_name_len);
      *ws_req_size += svr_entry->server_name_len;
    }
    else
    {
      //recorded host
      NS_FILL_IOVEC(g_req_rep_io_vector, cptr->url_num->index.svr_ptr->server_hostname, cptr->url_num->index.svr_ptr->server_hostname_len);
      *ws_req_size += cptr->url_num->index.svr_ptr->server_hostname_len;
    }
    NS_FILL_IOVEC(g_req_rep_io_vector, WS_CRLFString, WS_CRLFString_Length);
    *ws_req_size += WS_CRLFString_Length;
  }

  //insert upgrade buffer
  NS_FILL_IOVEC(g_req_rep_io_vector, upgrade_buf, UPGRADE_BUF_STRING_LENGTH);
  *ws_req_size += UPGRADE_BUF_STRING_LENGTH;

  //insert connection buffer
  NS_FILL_IOVEC(g_req_rep_io_vector, ws_connection_buf, CONN_BUF_STRING_LENGTH);
  *ws_req_size += CONN_BUF_STRING_LENGTH;

  //insert sec-websocket-key
  NS_FILL_IOVEC(g_req_rep_io_vector, sec_ws_key_buf, SEC_BUF_STRING_LENGTH);
  *ws_req_size += SEC_BUF_STRING_LENGTH;

  str = ns_get_random_str(16, 16, "a-z0-9");
  strcpy(rand_string_data, str);
  nslb_binary_base64encode(str, 16, vptr->httpData->ws_client_base64_encoded_key, BUFFER_LENGTH);
  NSDL2_WS(vptr, cptr, "Base64 Encoded value: %s\n", vptr->httpData->ws_client_base64_encoded_key);

  NS_FILL_IOVEC(g_req_rep_io_vector, vptr->httpData->ws_client_base64_encoded_key, strlen(vptr->httpData->ws_client_base64_encoded_key));
  *ws_req_size += strlen(vptr->httpData->ws_client_base64_encoded_key);

  NS_FILL_IOVEC(g_req_rep_io_vector, WS_CRLFString, WS_CRLFString_Length);
  *ws_req_size += WS_CRLFString_Length;

  //insert Sec-WebSocket-Version 
  NS_FILL_IOVEC(g_req_rep_io_vector, sec_ws_version_buf, SEC_WS_VERSION_STRING_LENGTH);
  *ws_req_size += SEC_WS_VERSION_STRING_LENGTH;

  /* insert the User-Agent header */
  if (!(disable_headers & NS_UA_HEADER)) {
    NS_FILL_IOVEC(g_req_rep_io_vector, ws_agent_buf, WS_USER_AGENT_STRING_LENGTH);
    *ws_req_size += WS_USER_AGENT_STRING_LENGTH;

    if (!(vptr->httpData && (vptr->httpData->ua_handler_ptr != NULL)
        && (vptr->httpData->ua_handler_ptr->ua_string != NULL)))
    {
      NS_FILL_IOVEC(g_req_rep_io_vector, vptr->browser->UA, strlen(vptr->browser->UA));
      *ws_req_size += strlen(vptr->browser->UA);
    } 
    else 
    {
      NS_FILL_IOVEC(g_req_rep_io_vector, vptr->httpData->ua_handler_ptr->ua_string, vptr->httpData->ua_handler_ptr->ua_len);
      *ws_req_size += vptr->httpData->ua_handler_ptr->ua_len;
    }
  }

  NSDL2_WS(vptr, cptr, "Insert Headers in WS, cptr = %p, num_seg = %d", 
                        cptr, cptr->url_num->proto.ws.hdrs.num_entries);

  int content_length;
  if((ret = insert_segments(vptr, cptr, &request->proto.ws.hdrs, &g_req_rep_io_vector, 
                                 &content_length, 0, 1, REQ_PART_HEADERS, request, SEG_IS_NOT_REPEAT_BLOCK)) < 0) 
  {

    NSDL2_WS(vptr, cptr, "Error: failed to fill Header, cur_idx = %d", ret);
    return -1;
  }
  *ws_req_size += content_length;

  /*Last Part of WS Req:  Add \r\n at last of WS Request */
  NS_FILL_IOVEC(g_req_rep_io_vector, WS_CRLFString, WS_CRLFString_Length);
  *ws_req_size += WS_CRLFString_Length;

  NSDL2_WS(vptr, cptr, "Total fill vectors - %d", NS_GET_IOVEC_CUR_IDX(g_req_rep_io_vector));

  /* prepare the expected server accept response */
  n = sprintf(buf, "%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", vptr->httpData->ws_client_base64_encoded_key);
  SHA1((unsigned char *)buf, n, (unsigned char *)str);
  nslb_binary_base64encode(str, 20, vptr->httpData->ws_expected_srever_base64_encoded_key, 30);
  NSDL2_WS(vptr, cptr, "vptr->httpData->ws_expected_srever_base64_encoded_key = %s", vptr->httpData->ws_expected_srever_base64_encoded_key);
  FREE_AND_MAKE_NULL(str, "str", -1);

  return 0;
}

int proc_ws_hdr_upgrade(connection *cptr, char *buf, int byte_left, int *bytes_consumed, u_ns_ts_t now)
{
  VUser *vptr = cptr->vptr;
  char upgrade_hdr[1024] = "";

  NSDL2_WS(vptr, cptr, "Method Called, buf = %s, byte_left = %d, bytes_consumed = %d, request_type = %d", 
                        buf, byte_left, *bytes_consumed, cptr->request_type);
  char cur_byte;

  for(*bytes_consumed = 0; *bytes_consumed < byte_left; (*bytes_consumed)++) 
  {
    cur_byte = buf[*bytes_consumed];
    NSDL2_HTTP(vptr, cptr, "cur byte = %c", cur_byte);
     
    if(cur_byte == '\r') {
      cptr->header_state = HDST_CR;
      break;
    }
    else {
      NSDL2_HTTP(vptr, cptr, "Setting proc_ws_hdr_upgrade(): header state to HDST_TEXT");
      cptr->header_state = HDST_TEXT;
      upgrade_hdr[*bytes_consumed] = cur_byte;
    }
  }

  /*********************************************
   Check Upgrade Header, It can be -
   1. websocket
   2. h2/h2c
  *********************************************/
  if(cptr->req_code == 101)
  {
    NSDL2_HTTP(NULL, cptr, "Check for Upgrade: h2/h2c/websocket, upgrade_hdr = %s", upgrade_hdr);
    char *proto = NULL;

    NSDL2_HTTP(NULL, cptr, "request_type = %d", cptr->request_type);
    if((cptr->request_type == WS_REQUEST) || (cptr->request_type == WSS_REQUEST))
    {
      /* If request is of type Websocket and server does not support wesocket protocol then close connection */
      proto = strcasestr(upgrade_hdr, "WebSocket");
      if(!proto)
      {
        NSTL1_OUT(NULL, cptr, "Server doesn't support WebSocket Request, hence closing connection, Upgrade header is %s", upgrade_hdr);
        Close_connection(cptr, 1, now, NS_REQ_PROTO_NOT_SUPPORTED_BY_SERVER, NS_COMPLETION_NOT_DONE); //ERR_ERR
        return -1;
      }
      else
      {
        NSDL2_WS(vptr, cptr, "Server support WebSocket Request. Upgrading to websocket");
        /* Set vptr->last_cptr since in API page_think_time() we need cptr */
        vptr->last_cptr = cptr;
        cptr->flags |= NS_WEBSOCKET_UPGRADE_DONE;
      }
    }
    else
    {
      /* If HTTP 1.1 request is send to a server supporting HTTP/2. 
         In this case server asks user to force upgrade to http2 by sending                   
         upgrade header in resp for request.
         Upgrade: h2,https/1.1,h2c^M
         Connection: Upgrade
         In this case netstorm will close the connection
      */
      if(cptr->http_protocol == HTTP_MODE_HTTP1)
      {
        NSDL2_HTTP(NULL, cptr, "HTTP1 request has been sent to server which supports HTTP2. Therefore closing the connection");
        Close_connection(cptr, 0, now, NS_REQUEST_BADBYTES, NS_COMPLETION_NOT_DONE); //ERR_ERR
        return HTTP2_ERROR;
      }
     
      proto = strstr(upgrade_hdr,"h2");
      if(proto) 
      {
        cptr->http_protocol = HTTP_MODE_HTTP2;
        vptr->hptr->http_mode = HTTP_MODE_HTTP2;
        cptr->flags |= NS_HTTP2_UPGRADE_DONE;
        NSDL2_HTTP(NULL, cptr, "Protocol type Negotiated to HTTP2. cptr->http_protocol [%d] vptr->hptr->http_mode = %d", 
                                cptr->http_protocol, vptr->hptr->http_mode);
      }
      else
      {
        cptr->http_protocol = HTTP_MODE_HTTP1;
        NSDL2_HTTP(NULL, cptr, "Set protocol type HTTP1");
      }
    }
  }
  else if((cptr->req_code != 101) && ((cptr->request_type == WS_REQUEST) && (cptr->request_type == WSS_REQUEST)))
  {
    NSTL1_OUT(NULL, cptr, "Server doesn't support WebSocket Request, hence closing connection");
    Close_connection(cptr, 0, now, NS_REQ_PROTO_NOT_SUPPORTED_BY_SERVER, NS_COMPLETION_NOT_DONE); //ERR_ERR
    return -1;
  }

  NSDL2_WS(vptr, cptr, "Upgrade header: = %s, bytes_consumed = %d", upgrade_hdr, *bytes_consumed);
  return 0;
}

int proc_ws_hdr_connection(connection *cptr, char *buf, int byte_left, int *bytes_consumed, u_ns_ts_t now)
{
  NSDL2_WS(NULL, cptr, "Method Called, cptr = %p, byte_left = %d, bytes_consumed = %d", cptr, byte_left, *bytes_consumed);
  //char ws_conn_hdr[1024] = "";
  char cur_byte;

  IW_UNUSED(char ws_conn_hdr[1024] = "");
  for(*bytes_consumed = 0; *bytes_consumed < byte_left; (*bytes_consumed)++)
  {
    cur_byte = buf[*bytes_consumed];
    NSDL2_HTTP(NULL, cptr, "cur byte = %c", cur_byte);

    if(cur_byte == '\r') {
      cptr->header_state = HDST_CR;
      break;
    }
    else {
      NSDL2_HTTP(NULL, cptr, "Setting proc_ws_hdr_connection(): header state to HDST_TEXT");
      cptr->header_state = HDST_TEXT;
      IW_UNUSED(ws_conn_hdr[*bytes_consumed] = cur_byte);
    }
  }
  NSDL2_HTTP(NULL, cptr, "value of ws_conn_hdr = [%s]", ws_conn_hdr);

  return 0;
}

int proc_ws_hdr_sec_websocket_accept(connection *cptr, char *buf, int byte_left, int *bytes_consumed, u_ns_ts_t now)
{
  VUser *vptr = cptr->vptr;

  NSDL2_WS(vptr, cptr, "Method Called, where buf = %s, byte_left = %d, bytes_consumed = %d", buf, byte_left, *bytes_consumed);
  
  char ws_key[1024] = "";
  char cur_byte;

  for(*bytes_consumed = 0; *bytes_consumed < byte_left; (*bytes_consumed)++)
  {
    cur_byte = buf[*bytes_consumed];
    NSDL2_HTTP(vptr, cptr, "cur byte = %c", cur_byte);
     
    if(cur_byte == '\r') {
      cptr->header_state = HDST_CR;
      break;
    }
    else {
      NSDL2_HTTP(vptr, cptr, "Setting proc_ws_hdr_sec_websocket_accept(): header state to HDST_TEXT");
      cptr->header_state = HDST_TEXT;
      ws_key[*bytes_consumed] = cur_byte;
    }
  }

  NSDL2_WS(vptr, cptr, "proc_ws_hdr_sec_websocket_accept(): bytes_consumed = %d, byte_left = %d", *bytes_consumed, byte_left);
  if(!strcmp(vptr->httpData->ws_expected_srever_base64_encoded_key, ws_key))
    NSDL2_WS(vptr, cptr, "Websocket key match with client key = [%s] where server key = [%s]", 
                          vptr->httpData->ws_expected_srever_base64_encoded_key, ws_key);
  else {
    NSDL2_WS(vptr, cptr, "Websocket key not match with client key = [%s] where server key = [%s]", 
                          vptr->httpData->ws_expected_srever_base64_encoded_key, ws_key);
    //TODO: what shoud do???
  }

  return 0;
}

int set_api(char *api_to_run, FILE *flow_fp, char *flow_file, char *line_ptr,
                                  FILE *outfp,  char *flow_outfile, int send_tb_idx)
{
  char *start_idx;
  char *end_idx;
  char str[MAX_LINE_LENGTH + 1];
  int len ;

  NSDL1_WS(NULL, NULL ,"Method Called. send_tb_idx = %d", send_tb_idx);
  start_idx = line_ptr;
  NSDL2_PARSING(NULL, NULL, "start_idx = [%s]", start_idx);
  end_idx = strstr(line_ptr, api_to_run);
  if(end_idx == NULL)
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012070_ID, CAV_ERR_1012070_MSG);

  len = end_idx - start_idx;
  strncpy(str, start_idx, len); //Copying the return value first
  str[len] = '\0';

  // Write like this. ret = may not be there
  //         int ret = ns_web_url(0) // HomePage

  NSDL2_WS(NULL, NULL,"Before sprintf str is = %s ", str);
  NSDL2_WS(NULL, NULL, "Add api  ns_web_websocket_send hidden file, send_tb_idx = %d", send_tb_idx);
  sprintf(str, "%s %s(%d); ", str, api_to_run, send_tb_idx);
  NSDL2_WS(NULL, NULL," final str is = %s ", str);
  if(write_line(outfp, str, 1) == NS_PARSE_SCRIPT_ERROR)
    SCRIPT_PARSE_ERROR(api_to_run, "Error Writing in File ");

  return NS_PARSE_SCRIPT_SUCCESS;
}

int create_websocket_close_table_entry(int *row_num)
{
  NSDL2_WS(NULL, NULL, "Method called");

  if (total_ws_close_entries == max_ws_close_entries)
  {
    MY_REALLOC_EX(g_ws_close, (max_ws_close_entries + WS_CLOSE_ENTRIES) * sizeof(ws_close_table), max_ws_close_entries * sizeof(ws_close_table), "g_ws_close", -1);
    if (!g_ws_close)
    {
      NSTL1_OUT(NULL, NULL,"create_websocket_close_table_entry(): Error allocating more memory for close entries\n");
      return(FAILURE);
    }
    else
      max_ws_close_entries += WS_CLOSE_ENTRIES;
  }

  *row_num =  total_ws_close_entries++; //Increment it when close api called

  g_ws_close[*row_num].conn_id = -1;
  g_ws_close[*row_num].status_code = 0;

  return(SUCCESS);
}

/*int create_websocket_callback_table_entry(int *row_num) 
{

//g_ws_close_shr

}*/

int create_websocket_callback_table_entry(int *row_num)
{
  NSDL1_WS(NULL, NULL, "Method called");
  if (total_ws_callback_entries == max_ws_callback_entries)
  {
    MY_REALLOC_EX(g_ws_callback, (max_ws_callback_entries + WS_CB_ENTRIES) * sizeof(ws_callback), max_ws_callback_entries * sizeof(ws_callback), "g_ws_callback", -1);
    if (!g_ws_callback)
    {
      NSTL1_OUT(NULL, NULL,"create_websocket_callback_table_entry(): Error allocating more memory for callback entries\n");
      return(FAILURE);
    }
    else
      max_ws_callback_entries += WS_CB_ENTRIES;
  }

  *row_num = total_ws_callback_entries++; 
  //memset(g_ws_callback, 0, sizeof(g_ws_callback));
 
  g_ws_callback[*row_num].cb_name = -1;
  g_ws_callback[*row_num].opencb_ptr = NULL;
  g_ws_callback[*row_num].msgcb_ptr = NULL;
  g_ws_callback[*row_num].errorcb_ptr = NULL;
  g_ws_callback[*row_num].closecb_ptr = NULL;

  return(SUCCESS);
}

int create_webSocket_send_table_entry(int *row_num)
{
  //action_request* requests;
  NSDL2_WS(NULL, NULL, "Method called");

  if (total_ws_send_entries == max_ws_send_entries)
  {
    MY_REALLOC_EX(g_ws_send, (max_ws_send_entries + WS_SEND_ENTRIES) * sizeof(ws_send_table), max_ws_send_entries * sizeof(ws_send_table), "g_ws_send", -1);
    if (!g_ws_send)
    {
      NSTL1_OUT(NULL, NULL,"create_websocket_send_table_entry(): Error allocating more memory for send entries\n");
      return(FAILURE);
    }
    else
      max_ws_send_entries += WS_SEND_ENTRIES;
  }

  *row_num =  total_ws_send_entries++; //Increment it when send api called

  g_ws_send[*row_num].send_buf.seg_start = -1;
  g_ws_send[*row_num].send_buf.num_entries = 0;
  g_ws_send[*row_num].id = -1;
  g_ws_send[*row_num].ws_idx = -1;
  
  NSDL2_WS(NULL, NULL, "send table *row_num = %d", *row_num);
  return(SUCCESS);
}


//copy websocket_send table in shared memory
void copy_websocket_send_table_to_shr(void)
{
  int send_tb_idx;

  NSDL2_WS(NULL, NULL, "Method called. total_websocket_send_entries = %d", total_ws_send_entries);
  if(!total_ws_send_entries)
  {
    NSDL2_WS(NULL, NULL, "No websocket_send entries", total_ws_send_entries);
    return;
  }
  
  g_ws_send_shr = (ws_send_table_shr*) do_shmget(sizeof (ws_send_table_shr) * total_ws_send_entries, "ws_send_table_shr");  
  
  for(send_tb_idx = 0; send_tb_idx < total_ws_send_entries ; send_tb_idx++)
  {
    NSDL2_WS(NULL, NULL, "Send Table data at ws_send_id = %d, conn_id = %d, send_buf.seg_start = %lu, send_buf.num_ernties = %d, "
                       "isbinary = %d, ws_idx = %d", 
                                       send_tb_idx, g_ws_send[send_tb_idx].id, 
                                       g_ws_send[send_tb_idx].send_buf.seg_start, g_ws_send[send_tb_idx].send_buf.num_entries, 
                                       g_ws_send[send_tb_idx].isbinary, g_ws_send[send_tb_idx].ws_idx); 

    g_ws_send_shr[send_tb_idx].id = g_ws_send[send_tb_idx].id;

    if (g_ws_send[send_tb_idx].send_buf.seg_start == -1)
      g_ws_send_shr[send_tb_idx].send_buf.seg_start = NULL;
    else
      g_ws_send_shr[send_tb_idx].send_buf.seg_start = SEG_TABLE_MEMORY_CONVERSION(g_ws_send[send_tb_idx].send_buf.seg_start);

    g_ws_send_shr[send_tb_idx].send_buf.num_entries = g_ws_send[send_tb_idx].send_buf.num_entries;

    NSDL2_WS(NULL, NULL, "JAGUUU: seg_start = %p", g_ws_send_shr[send_tb_idx].send_buf.seg_start);

    g_ws_send_shr[send_tb_idx].isbinary = g_ws_send[send_tb_idx].isbinary;

    g_ws_send_shr[send_tb_idx].ws_idx = g_ws_send[send_tb_idx].ws_idx;

    NSDL2_WS(NULL, NULL, "Send Table Shar data at ws_send_id = %d, conn_id = %d, send_buf.seg_start = %p, send_buf.num_ernties = %d, "
                       "isbinary = %d, ws_idx = %d", 
                                       send_tb_idx, g_ws_send_shr[send_tb_idx].id, 
                                       g_ws_send_shr[send_tb_idx].send_buf.seg_start, g_ws_send_shr[send_tb_idx].send_buf.num_entries, 
                                       g_ws_send_shr[send_tb_idx].isbinary, g_ws_send_shr[send_tb_idx].ws_idx); 
  }  
}

//copy_websocket_callback_table_to_shr();
void copy_websocket_callback_to_shr(void)
{
  int callback_idx = 0;

  NSDL2_WS(NULL, NULL, "Method called. total_websocket_callback_entries = %d, total_ws_callback_entries = %d",
                        total_ws_callback_entries, total_ws_callback_entries);
  if(!total_ws_callback_entries)
  {
    NSDL2_WS(NULL, NULL, "No websocket_callback_entries");
    return;
  }

  g_ws_callback_shr = (ws_callback_shr*) do_shmget(sizeof (ws_callback_shr) * total_ws_callback_entries, "ws_callback_shr");

  for (callback_idx = 0; callback_idx < total_ws_callback_entries; callback_idx++)
  {
    memset(g_ws_callback_shr, -1, (sizeof (ws_callback_shr) * total_ws_callback_entries));

    if (g_ws_callback[callback_idx].cb_name == -1)
      g_ws_callback_shr[callback_idx].cb_name = NULL;
    else
    {
      NSDL2_WS(NULL, NULL, "Method called, g_ws_callback[callback_idx].cb_name = %d, callback_idx = %d", 
                            g_ws_callback[callback_idx].cb_name, callback_idx);
      g_ws_callback_shr[callback_idx].cb_name = BIG_BUF_MEMORY_CONVERSION(g_ws_callback[callback_idx].cb_name);
    }
  }
}

// extract buffer
int extract_buffer(FILE *flow_fp, char *start_ptr, char **end_ptr, char *flow_file)
{
  int buffer_over = 0;

  NSDL1_WS(NULL, NULL, "Method called, start_ptr = [%s]", start_ptr);

  if((start_ptr = strstr(start_ptr, "=")) == NULL)
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012312_ID, CAV_ERR_1012312_MSG);

  start_ptr++;

  if(strrchr(script_line, '"')) // Find the last quote
    *end_ptr = strrchr(script_line, '"');

  NSDL1_WS(NULL, NULL, "end_ptr = %s", end_ptr);

  copy_to_post_buf(start_ptr, strlen(start_ptr));

  if(*end_ptr != NULL)
  {
    NSDL2_PARSING(NULL, NULL, "Buffer is over");
    buffer_over = 1;
  }

  if(buffer_over == 0)
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012313_ID, CAV_ERR_1012313_MSG);

  return NS_PARSE_SCRIPT_SUCCESS;
}

//1) ID should be unique
static int is_ws_conid_exist(char *ws_id, int url_idx)
{

  int i;
  int len = strlen(ws_id);
  int num_ws_id = atoi(ws_id);

  NSDL2_WS(NULL, NULL, "Method Called, ws_id = %s ", ws_id);

  for(i = 0 ; i < len ; i++) {
    if(!isdigit(ws_id[i]))
      return -1;
  }
  //checking uniquness of id
  for(i = 0; i < total_request_entries; i++) 
  {
    if(requests[i].request_type == WS_REQUEST || requests[i].request_type == WSS_REQUEST)
    {
      if( requests[i].proto.ws.conn_id == num_ws_id)
      return 1; //conid exist 
    }
  }
  return 0;
}

int parse_websocket_send_parameters(FILE *flow_fp, FILE *outfp, char *flow_file, char * flow_outfile, char *starting_quotes, int sess_idx)
{
  char attribute_name[128 + 1];
  char attribute_value[MAX_LINE_LENGTH];
  char *closing_quotes = '\0';
  int id_count = 0; //track of id count
  int buffer_count = 0; //track of buffer count
  int isbinary_count = 0; //track of isbinary count
  int id_exists = 0;
  int ret;
  int send_tb_idx;
 
  NSDL2_WS(NULL, NULL, "Method Called, sess_idx = %d", sess_idx);

  // Allocating record for websocket_send_table 
  if(create_webSocket_send_table_entry(&send_tb_idx) != SUCCESS)
    SCRIPT_PARSE_ERROR(script_line, "Unable to create webSocket_send_table");
  //Setting values for new page in page structure
  /*if (gSessionTable[sess_idx].num_ws_send == 0)
  {
    gSessionTable[sess_idx].ws_first_send = send_tb_idx;
    NSDL2_WS(NULL, NULL, "sess_idx = %d, ws_first_sen = %d", sess_idx, send_tb_idx);
    //gSessionTable[sess_idx].num_pages++;
  }*/

  //setting api ns_web_websocket_send here
  if(( set_api("ns_web_websocket_send", flow_fp, flow_filename, script_line, outfp, flow_outfile, send_tb_idx)) == NS_PARSE_SCRIPT_ERROR)
  NSDL2_WS(NULL, NULL, "Method Called starting_quotes = %p, starting_quotes = %s", starting_quotes, starting_quotes);

  while(1)
  {
    NSDL2_WS(NULL, NULL, "script_line = %s, send_tb_idx = %d", script_line, send_tb_idx);
    // It will parse from , to next starting quote.
    if(get_next_argument(flow_fp, starting_quotes, attribute_name, attribute_value, &closing_quotes, 0) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;

    NSDL2_WS(NULL, NULL, "attribute_name = %s, attribute_value = %s", attribute_name, attribute_value);
    if(strcasecmp(attribute_name, "ID") == 0)
    {
      NSDL2_WS(NULL, NULL, "starting_quotes = [%s], closing_quotes = [%s]", starting_quotes, closing_quotes);
      NSDL2_WS(NULL, NULL, "id=[%s] & id_count=[%d], found at script_line %d", attribute_value, id_count, script_ln_no);

      // In websocket_send id should be passed once
      id_count++;
      
      int ret = is_ws_conid_exist(attribute_value, send_tb_idx);
      if(ret != 1)
      {
        SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012324_ID, CAV_ERR_1012324_MSG, attribute_value);
      }
 
      g_ws_send[send_tb_idx].id = atoi(attribute_value);
      g_ws_send[send_tb_idx].ws_idx = ws_idx_list[g_ws_send[send_tb_idx].id]; 

      if(id_count > 1)
      {
        NSDL2_WS(NULL, NULL, "ID = [%d] passed in ns_web_websocket_send is more than one", attribute_value);
        return NS_PARSE_SCRIPT_ERROR;
      }
      
      //Due to design issue we can't parametrise ID
      //segment_line(&(g_ws_send[send_tb_idx].id), attribute_value, 1, script_ln_no, sess_idx, flow_file);

      id_exists = 1; // This is for checking whether id exists or not.

      NSDL2_WS(NULL, NULL, "WS_SEND: ID = %d, ws_idx = %d", g_ws_send[send_tb_idx].id, g_ws_send[send_tb_idx].ws_idx);
    }
    else if(strcasecmp(attribute_name, "BUFFER") == 0)
    {
      NSDL2_WS(NULL, NULL, "starting_quotes = [%s], closing_quotes = [%s]", starting_quotes, closing_quotes);
      NSDL2_WS(NULL, NULL, "buffer = [%s]", attribute_value);
   
      // In websocket_send buffer should be passed once
      buffer_count++;

      // buffer can be parameterized.
      if(extract_buffer(flow_fp, script_line, &closing_quotes, flow_file) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;

      // this is for set buffer
      //if(set_body(g_ws_send[send_tb_idx].ws_idx, sess_idx, starting_quotes, closing_quotes, flow_file) == NS_PARSE_SCRIPT_ERROR)
      if(ws_set_post_body(send_tb_idx, sess_idx, &script_ln_no, flow_file) == NS_PARSE_SCRIPT_ERROR)
      {
        NSDL2_WS(NULL, NULL, "Send Table data at ws_send_id = %d, conn_id = %d, send_buf.seg_start = %lu, send_buf.num_ernties = %d, "
                             "isbinary = %d, ws_idx = %d", 
                                       send_tb_idx, g_ws_send[send_tb_idx].id, 
                                       g_ws_send[send_tb_idx].send_buf.seg_start, g_ws_send[send_tb_idx].send_buf.num_entries, 
                                       g_ws_send[send_tb_idx].isbinary, g_ws_send[send_tb_idx].ws_idx); 
        return NS_PARSE_SCRIPT_ERROR;
      }
      if(buffer_count > 1)
      {
        NSDL2_WS(NULL, NULL, "BUFFER passed in ns_web_websocket_send is more than one");
        return NS_PARSE_SCRIPT_ERROR;
      }

      NSDL2_WS(NULL, NULL, "WS_SEND: BUFFER = %s", g_post_buf);
    }
    else if(strcasecmp(attribute_name, "ISBINARY") == 0)
    {
      NSDL2_WS(NULL, NULL, "starting_quotes = [%s], closing_quotes = [%s]", starting_quotes, closing_quotes);
      g_ws_send[send_tb_idx].isbinary = atoi(attribute_value);
      
      // In websocket_send id should be passed once
      isbinary_count++;
      if(isbinary_count > 1)
      {
        NSDL2_WS(NULL, NULL, "ISBINARY = [%d] passed in ns_web_websocket_send is more than one");
        return NS_PARSE_SCRIPT_ERROR;
      }

      //Validate isbinary
      if((g_ws_send[send_tb_idx].isbinary != 1) && (g_ws_send[send_tb_idx].isbinary != 0))
      {
        NSTL1_OUT(NULL, NULL, "In ns_web_websocket_send ISBINARY = [%d], should be not greater than 1 and less than 0. \n", 
                         g_ws_send[send_tb_idx].isbinary);
        return NS_PARSE_SCRIPT_ERROR;
      }
      NSDL2_WS(NULL, NULL, "WS_SEND: ISBINARY = %d", g_ws_send[send_tb_idx].isbinary);
    }
    else
    {
       SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012219_ID, CAV_ERR_1012219_MSG, attribute_name);
    }
    *starting_quotes = '\0';
    ret = read_till_start_of_next_quotes(flow_fp, flow_file, closing_quotes, &starting_quotes, 0, outfp);
    NSDL2_WS(NULL, NULL, "Starting Quotes=[%s], Starting quotes=[%p], Closing_quotes=[%s], Closing_quotes=[%p], after read_till_start_of_next_quotes()",
                          starting_quotes, starting_quotes, closing_quotes, closing_quotes);
    if(ret == NS_PARSE_SCRIPT_ERROR)
    {
      if(starting_quotes == NULL)
        return NS_PARSE_SCRIPT_ERROR;
      else // Means current parameters are processed, so break
        break;
    }
  } //End of while

  NSDL2_WS(NULL, NULL, "PARSEING: Send Table data at ws_send_id = %d, conn_id = %d, send_buf.seg_start = %lu, send_buf.num_ernties = %d, "
                             "isbinary = %d, ws_idx = %d", 
                                       send_tb_idx, g_ws_send[send_tb_idx].id, 
                                       g_ws_send[send_tb_idx].send_buf.seg_start, g_ws_send[send_tb_idx].send_buf.num_entries, 
                                       g_ws_send[send_tb_idx].isbinary, g_ws_send[send_tb_idx].ws_idx); 
  if(!id_exists)
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012315_ID, CAV_ERR_1012315_MSG);

  if(!buffer_count)
  {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012336_ID, CAV_ERR_1012336_MSG);
  }


  return NS_PARSE_SCRIPT_SUCCESS;
}

/*--------------------------------------------------------------------------------------------- 
 * Purpose   : This function will parse ns_web_websocket_send() API and do follwing things 
 *              
 *                 (i) create_webSocket_send_table_entry
 *                 (ii) parse_websocket_send_parameters 
 *
 * Input     : flow_fp - pointer to input flow file 
 *             ns_web_websocket_send("ID=22",
 *                                   "BUFFER={WebSocketSend0}",
 *                                   "ISBINARY=0")
 *
 * Output    : On success -  0
 *             On Failure - -1  
 *--------------------------------------------------------------------------------------------*/

int ns_parse_websocket_send(FILE *flow_fp, FILE *outfp,  char *flow_file, char *flow_outfile, int sess_idx)
{
  char *id_end_ptr = NULL;

  NSDL1_WS(NULL, NULL, "Method Called, sess_idx = %d, script_line = [%s]", sess_idx, script_line);

  //To store send msg into global buffer g_post_buf
  init_post_buf();

  // This will point to the starting quote '"'.
  id_end_ptr = strchr(script_line, '"');
  NSDL4_PARSING(NULL, NULL, "id_end_ptr = [%s], script_line = [%s]", id_end_ptr, script_line);

  // This will parse websocket_send api parameters - id, buffer, isbinary.
  NSDL2_WS(NULL, NULL, "**********flow_outfile = %s", flow_outfile);
  if(parse_websocket_send_parameters(flow_fp, outfp, flow_file, flow_outfile, id_end_ptr, sess_idx) == NS_PARSE_SCRIPT_ERROR)
    return NS_PARSE_SCRIPT_ERROR;

  NSDL2_WS(NULL, NULL, "Exiting Method");

  return NS_PARSE_SCRIPT_SUCCESS;
}

//1) ID should be unique
//2) ID should be numeric
static int validate_id(char *ws_id, int url_idx)
{
  int i;
  NSDL2_WS(NULL, NULL, "Method Called, ws_id = %s ", ws_id);
  int len = strlen(ws_id);
  for(i = 0 ; i < len ; i++) {
    if(!isdigit(ws_id[i]))
      return -1;
  }
  //checking uniquness of id
  for(i = url_idx - 1 ; i >= 0 ; i--) {
    if( requests[i].proto.ws.conn_id == atoi(ws_id))
      return -2; 
  }
  return 0;
}

int parse_uri(char *in_uri, char *host_end_markers, int *request_type, char *hostname, char *path)
{
  char *host_end = NULL;
  char *ws_str = "ws://";
  char *wss_str = "wss://";
  char *uri = in_uri;

  char param_stflag = 0;
  char param_edflag = 0;
  char path_flag = 0;
  char *uri_path_ptr;
  char is_param_scheme = 0;
  char is_param_host = 0;
  char is_param_port = 0;

  NSDL1_WS(NULL, NULL, "Method called. Parse URI = %s, host_end_markers = %s", uri, host_end_markers);

  if (in_uri[0] == '\0')
  {
    NSTL1_OUT(NULL, NULL, "Error: Url is empty. uri = %s\n", in_uri);
    NSEL_MAJ(NULL, NULL, ERROR_ID, ERROR_ATTR, "Error: Uri is empty. uri = %s", in_uri);
    return RET_PARSE_NOK;
  }


  //Check whether URL is parameterized or not. i.e URL="{scheme}://{host:port}/path?queryString"
  if(tolower(in_uri[0]) < 97 || tolower(in_uri[0]) > 123)
  {
    NSTL1_OUT(NULL, NULL, "Error: URL must be starts with either '{' or any character, invalid uri = %s", in_uri);
    return RET_PARSE_NOK;
  }
  //Scheme is parameterized
  if(in_uri[0] == '{')
  {
    is_param_scheme = 1;
    uri++;
    param_stflag++;
    NSDL2_HTTP(NULL, NULL, "Scheme is parameterized");
  }
  while(*uri)
  {

    NSDL2_HTTP(NULL, NULL, "path_flag = %d, *uri = %c, uri = %s", path_flag, *uri, uri);
    if((*uri == ':') && (*(uri + 1) == '/') && (*(uri + 2) == '/'))
    {
      if(is_param_scheme)
      {
        if((*(uri - 1)) != '}')
        {
          NSTL1_OUT(NULL, NULL, "Error: Scheme cannot be half parameterized, invalid uri = %s", in_uri);
          return RET_PARSE_NOK;
        }
        if(param_stflag != param_edflag)
        {
          NSTL1_OUT(NULL, NULL, "Error: start bracket and end brackets are not same, uri = %s", in_uri);
          return RET_PARSE_NOK;
        }
        param_stflag = param_edflag = 0;
      }
      uri += 3;  //Skipping ://
      if (*uri == '{')
      {
        param_stflag++;
        is_param_host = 1;
        NSDL2_HTTP(NULL, NULL, "Host is parameterized"); 
        uri++; 
      }
      continue;
    }
    else if (*uri == ':')
    {
      if(is_param_host)
      {
        if((*(uri - 1)) != '}')
        { 
          NSTL1_OUT(NULL, NULL, "Error: Host cannot be half parameterized, invalid uri = %s", in_uri);
          return RET_PARSE_NOK;
        }
        if(param_stflag != param_edflag)
        { 
          NSTL1_OUT(NULL, NULL, "Error: start bracket and end brackets are not same, uri = %s", in_uri);
          return RET_PARSE_NOK;
        }
        param_stflag = param_edflag = 0;
      }
      uri++; //Skiping :
      if (*uri == '{')
      {
        param_stflag++; 
        is_param_port = 1;
        NSDL2_HTTP(NULL, NULL, "Port is parameterized"); 
        uri++;
      }
      continue; 
    }
    else if((*uri == '/') || (*uri == '?') || (*uri == '#'))
    {
      path_flag = 1;
      NSDL2_HTTP(NULL, NULL, "path_flag = %d, *uri = %c, uri = %s", path_flag, *uri, uri);
      uri_path_ptr = uri;
      if(param_stflag  != param_edflag)
      { 
        NSTL1_OUT(NULL, NULL, "Error: start bracket and end brackets are not same, uri = %s", in_uri);
        return RET_PARSE_NOK;
      }
      break;
    }
    else if(*uri == '{')
    {
      param_stflag++;
    }
    else if(*uri == '}')
    {
      param_edflag++;
      if(param_edflag > param_stflag)
      {
        NSTL1_OUT(NULL, NULL, "Error: end brackets should be less than start brackets, uri = %s", in_uri);
        return RET_PARSE_NOK;
      }
    }
    uri++;
  }
  if(param_stflag  != param_edflag)
  { 
    NSTL1_OUT(NULL, NULL, "Error: start bracket and end brackets are not same, uri = %s", in_uri);
    return RET_PARSE_NOK;
  }

  if(is_param_scheme || is_param_host || is_param_port)
  {
    *request_type = PARAMETERIZED_URL;
    uri = in_uri;
    NSDL2_HTTP(NULL, NULL, "path_flag = %d, path = %s", path_flag, path);
    if(path_flag)
    {
      if(uri_path_ptr[0] == '/')
        strcpy(path, uri_path_ptr);
      else
      {
        path[0] = '/';
        strcpy(path + 1, uri_path_ptr);
      }
      uri_path_ptr[0] = '\0';
    }
  }
  else 
  {
    uri = in_uri;

    if (!strncasecmp(in_uri, ws_str, strlen(ws_str))) {
      *request_type = WS_REQUEST;
      uri += strlen(ws_str);
      NSDL2_WS(NULL, NULL, "request_type = %d", *request_type);
    } else if (!strncasecmp(in_uri, wss_str, strlen(wss_str))) {
      *request_type = WSS_REQUEST;
      uri += strlen(wss_str);
      NSDL2_WS(NULL, NULL, "request_type = %d", *request_type);
    } else if (!strncmp(in_uri, "//", 2)) {
      *request_type = REQUEST_TYPE_NOT_FOUND;
      uri += 2;
    } else {
      *request_type = REQUEST_TYPE_NOT_FOUND;

      //http:{host}:port   /cgi?sdjsj        
      /* 
        Case 1: when Request type is not found
        hostname will be empty 
        path will be uri
        eg - 
        img/test.gif     (relative)
        /img/test.gif    (absolute)
      */
      hostname[0] = '\0';
      strcpy(path, uri); /* if (path[0] == '/') then absolute otherwise relative */
      NSDL2_HTTP(NULL, NULL, "request_type = %d", *request_type);
  
      return RET_PARSE_OK;
    }
  
    // Check if it is empty after schema (e.g. ws://)
    if (uri[0] == '\0')
    {
      NSDL2_HTTP(NULL, NULL, "Error: Url host is empty. uri = %s\n", in_uri);
      NSTL1_OUT(NULL, NULL, "Error: Url host is empty. uri = %s\n", in_uri);
      return RET_PARSE_NOK;
    }
      
    if(host_end_markers)
       host_end = strpbrk(uri, host_end_markers);
 
    if (!host_end) {
      /* 
        Case 2: when Request type is found and path is not there
        hostname will be uri 
        path will be /
        eg -
        http://www.test.com
      */
      strcpy(hostname, uri);
      strcpy(path, "/");
      return RET_PARSE_OK;
    }
    
    if (host_end == uri) // E.g. http://?
    {
      NSDL2_HTTP(NULL, NULL, "Error: Url host is empty with query paramters. uri = %s\n", in_uri);
      NSTL1_OUT(NULL, NULL, "Error: Url host is empty with query paramters. uri = %s\n", in_uri);
      return RET_PARSE_NOK;
    }
      
    /* 
      Case 3: when Request type is found and path is there
      hostname will be extracted 
      path will be extracted
      eg -
      http://www.test.com/abc.html    (path - /abc.html )
      http://www.test.com?x=2         (path - /?x=2     )
      http://www.test.com#hello       (path - /#hello   )
      http://www.test.com{path}       (path - path is parametrise   )
    */
    strncpy(hostname, uri, host_end - uri);
    hostname[host_end - uri] = '\0';
      
    if(*host_end == '?' || *host_end == '#')
    {
      path[0] = '/';
      strcpy(path+1, host_end);
    }
    else
    {
      strcpy(path, host_end);
    }
  }
  NSDL2_MISC(NULL, NULL, "path of uri = %s", path);
 
  //Here all ns_decrypt API in query parameter will be parsed.
  if(path[0])
    ns_parse_decrypt_api(path);
   
  return RET_PARSE_OK;
}

#if 0
static int parse_uri(char *in_uri, char *host_end_markers, int *request_type, char *hostname, char *path)
{
  char *host_end = NULL;
  char *ws_str = "ws://";
  char *wss_str = "wss://";
  char *uri = in_uri;

  NSDL1_WS(NULL, NULL, "Method called. Parse URI = %s, host_end_markers = %s", uri, host_end_markers);

  if (in_uri[0] == '\0')
  {
    NSTL1_OUT(NULL, NULL, "Error: Url is empty. url = %s\n", in_uri);
    NSEL_MAJ(NULL, NULL, ERROR_ID, ERROR_ATTR, "Error: Uri is empty. uri = %s",
                                                in_uri);
    return RET_PARSE_NOK;
  }

  if (!strncasecmp(in_uri, ws_str, strlen(ws_str))) {
    *request_type = WS_REQUEST;
    uri += strlen(ws_str);
    NSDL2_WS(NULL, NULL, "request_type = %d", *request_type);
  } else if (!strncasecmp(in_uri, wss_str, strlen(wss_str))) {
    *request_type = WSS_REQUEST;
    uri += strlen(wss_str);
    NSDL2_WS(NULL, NULL, "request_type = %d", *request_type);
  } else if (!strncmp(in_uri, "//", 2)) {
    *request_type = REQUEST_TYPE_NOT_FOUND;
    uri += 2;
  } else {
    *request_type = REQUEST_TYPE_NOT_FOUND;

    /* 
      Case 1: when Request type is not found
      hostname will be empty 
      path will be uri
      eg - 
      img/test.gif     (relative)
      /img/test.gif    (absolute)
    */
    hostname[0] = '\0';
    strcpy(path, uri); /* if (path[0] == '/') then absolute otherwise relative */
    NSDL2_WS(NULL, NULL, "request_type = %d", *request_type);

    return RET_PARSE_OK;
  }

  // Check if it is empty after schema (e.g. http://)
  if (uri[0] == '\0')
  {
    NSDL2_WS(NULL, NULL, "Error: Url host is empty. url = %s\n", in_uri);
    NSTL1_OUT(NULL, NULL, "Error: Uri host is empty. uri = %s\n", in_uri);
    return RET_PARSE_NOK;
  }

  if (host_end_markers)
     host_end = strpbrk(uri, host_end_markers);

  if (!host_end) {
    /* 
      Case 2: when Request type is found and path is not there
      hostname will be url 
      path will be /
      eg -
      http://www.test.com
    */
    strcpy(hostname, uri);
    strcpy(path, "/");
    return RET_PARSE_OK;
  }

  if (host_end == uri) // E.g. ws://?
  {
    NSDL2_WS(NULL, NULL, "Error: Url host is empty with query paramters. uri = %s\n", in_uri);
    NSTL1_OUT(NULL, NULL, "Error: Url host is empty with query paramters. uri = %s\n", in_uri);
    return RET_PARSE_NOK;
  }

  /* 
    Case 3: when Request type is found and path is there
    hostname will be extracted 
    path will be extracted
    eg -
    http://www.test.com/abc.html    (path - /abc.html )
    http://www.test.com?x=2         (path - /?x=2     )
    http://www.test.com#hello       (path - /#hello   )
  */
  strncpy(hostname, uri, host_end - uri);
  hostname[host_end - uri] = '\0';

  if(*host_end == '?' || *host_end == '#')
  {
    path[0] = '/';
    strcpy(path+1, host_end);
  }
  else
  {
    strcpy(path, host_end);
  }
  return RET_PARSE_OK;
}
#endif

static int ws_set_uri(char *uri, char *flow_file, int sess_idx, int url_idx)
{
  //TODO reset this function 
  char hostname[MAX_LINE_LENGTH + 1];
  int  request_type;
  char request_line[MAX_LINE_LENGTH + 1];
  //int get_no_inlined_obj_set_for_all = 1;

  NSDL2_PARSING(NULL, NULL, "Method Called Uri=%s", uri);
  //Parses Absolute/Relative URLs
  if(parse_uri(uri, "{/?#", &request_type, hostname, request_line) != RET_PARSE_OK)
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012069_ID, CAV_ERR_1012069_MSG, uri);

  //Request type should be from ws, wss
  if(request_type == REQUEST_TYPE_NOT_FOUND)
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012317_ID, CAV_ERR_1012317_MSG);
  
  NSDL3_PARSING(NULL, NULL, "request_type = %d", request_type); 
  if(request_type == PARAMETERIZED_URL)
  {
    requests[url_idx].is_url_parameterized = NS_URL_PARAM_VAR;
    request_type = WS_REQUEST;
  } 
 
  //TODO: we have consider proto type is WS_REQUEST and set it already
  //proto_based_init(url_idx, request_type);
  requests[url_idx].request_type = request_type;

  //Setting url type to Main/Embedded
  gPageTable[g_cur_page].num_eurls++; // Increment urls

  if (g_max_num_embed < gPageTable[g_cur_page].num_eurls) g_max_num_embed = gPageTable[g_cur_page].num_eurls; //Get high water mark

  if(requests[url_idx].is_url_parameterized)
  {
    segment_line(&(requests[url_idx].proto.ws.uri_without_path), uri, 0, script_ln_no, sess_idx, flow_filename);
    requests[url_idx].index.svr_idx = get_parameterized_server_idx(hostname, requests[url_idx].request_type, script_ln_no);
  }
  else
  {
    // check if the hostname exists in the server table, if not add it
    requests[url_idx].index.svr_idx = get_server_idx(hostname, requests[url_idx].request_type, script_ln_no);

    /*Added for filling all server in gSessionTable*/
    CREATE_AND_FILL_SESS_HOST_TABLE_ENTRY(url_idx, "Method called from ws_set_uri");
  }

  if(requests[url_idx].index.svr_idx != -1)
  {
    if(gServerTable[requests[url_idx].index.svr_idx].main_url_host == -1)
    {
      gServerTable[requests[url_idx].index.svr_idx].main_url_host = 1; // For main url
    }
  }
  else
  {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012073_ID, CAV_ERR_1012073_MSG);
  }
  
  //URI contains whole string with path eg: uri = wss://echo.websocket.org/123, request_line = /123 
  NSDL2_PARSING(NULL, NULL, "uri = %s, request_line = %s", uri, request_line);
  segment_line(&(requests[url_idx].proto.ws.uri), request_line, 0, script_ln_no, sess_idx, flow_filename);
  
  /*Added for filling all server in gSessionTable*/
  //CREATE_AND_FILL_SESS_HOST_TABLE_ENTRY(url_idx, "Method called from ws_set_uri");

  NSDL3_WS(NULL, NULL, "Exitting Method ");
  return NS_PARSE_SCRIPT_SUCCESS;
}

static int init_ws_uri(int *url_idx, char *flow_file)
{
  NSDL2_WS(NULL, NULL, "Method Called url_idx = %d, flow_file = %s", *url_idx, flow_file); 

  //creating request table
  create_requests_table_entry(url_idx); // Fill request type inside create table entry

   gPageTable[g_cur_page].first_eurl = *url_idx;
    
  return NS_PARSE_SCRIPT_SUCCESS;
}


/*--------------------------------------------------------------------------------------------- 
 * Purpose   : This function will parse ns_we_websocket_connet() API and do follwing things 
 *              
 *             (1) Create and fill following tables -
 *                 (i) Add dummy page name like wsp_<id> into gPageTable
 *                 (ii) Create request table and fill data  
 *                 (iii) create ws_request Table and fill its members 
 *                 (iv) create ws_callback and fill its members 
 *
 * Input     : flow_fp - pointer to input flow file 
 *             ns_web_websocket_connect("PageName",  //dummy not given by user
 *                                      "ID=22",
 *                                      "URI=ws://rumpetroll.motherfrog.com:8180/",
 *                                      "Origin=http://your_server",
 *                                      "OnOpenCB=OnOpenCB0",
 *                                      "OnMessageCB=OnMessageCB0",
 *                                      "OnErrorCB=OnErrorCB0",
 *                                      "OnCloseCB=OnCloseCB0");
 *             outfp   - pointer to output flow file (made in $NS_WDIR/.tmp/ns-inst<nvm_id>/)
 *             flow_filename - flow file name 
 *             sess_idx- pointing to session index in gSessionTable 
 *
 * Output    : On success -  0
 *             On Failure - -1  
 *--------------------------------------------------------------------------------------------*/
int ns_parse_websocket_connect(FILE *flow_fp, FILE *outfp, char *flow_filename, char *flowout_filename, int sess_idx)
{ 
  int  url_idx = 0;
  int ws_cb_idx = 0;
  int ret;
  char header_buf[WS_MAX_HRD_LEN + 1] = "\0";
  char attribute_name[WS_MAX_ATTR_LEN + 1];
  char attribute_value[WS_MAX_ATTR_LEN + 1];
  char pagename[WS_MAX_ATTR_LEN + 1];
  char *page_end_ptr = NULL;
  static int cur_page_index = -1; //For keeping track of multiple main urls
  static int duplicate_id_flag = -1; //For keeping track of multiple ids
  char *close_quotes = NULL;
  char *start_quotes = NULL;
  char uri_exists = 0;

  NSDL2_WS(NULL, NULL, "Method Called, sess_idx = %d", sess_idx);

  //Adding Dummy page name as in ns_web_websocket_connect() API page name is not given 
  sprintf(pagename, "wsp_%d", web_url_page_id); 

  page_end_ptr = strchr(script_line, '"');

  NSDL2_WS(NULL, NULL, "pagename - [%s], page_end_ptr = [%s]", pagename, page_end_ptr);
  //TODO should we set default value of ws_call_back entry
  
  if((parse_and_set_pagename("ns_web_websocket_connect", "ns_web_url", flow_fp, flow_filename, 
              script_line, outfp, flowout_filename, sess_idx, &page_end_ptr, pagename)) == NS_PARSE_SCRIPT_ERROR)
    return NS_PARSE_SCRIPT_ERROR;

  close_quotes = page_end_ptr;
  start_quotes = page_end_ptr;

  if(init_ws_uri(&url_idx, flow_filename) == NS_PARSE_SCRIPT_ERROR)
    return NS_PARSE_SCRIPT_ERROR;

  //Set default values here because in below loop all the members reset and creating  problem
  proto_based_init(url_idx, WS_REQUEST); 

  while(1)
  {
    NSDL3_WS(NULL, NULL, "line = %s", script_line);
    ret = get_next_argument(flow_fp, start_quotes, attribute_name, attribute_value, &close_quotes, 0);
    if(ret == NS_PARSE_SCRIPT_ERROR) return NS_PARSE_SCRIPT_ERROR;

    if(!strcasecmp(attribute_name, "URI"))
    {
      NSDL2_WS(NULL, NULL, "URI [%s] ", attribute_value);
      if(cur_page_index != -1)
      {
        if(cur_page_index == g_cur_page)
          SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012218_ID, CAV_ERR_1012218_MSG, "URI");
      }
      cur_page_index = g_cur_page;
      if(ws_set_uri(attribute_value, flow_filename, sess_idx, url_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;

      uri_exists = 1;
      NSDL2_WS(NULL, NULL, "WEBSOCKET: Value of %s = %s , segment offset = %d", 
                                                               attribute_name, attribute_value, requests[url_idx].proto.ws.uri);
   }
    else if (!strcasecmp(attribute_name, "ID"))
    {
      NSDL2_WS(NULL, NULL, "ID =  [%s] ", attribute_value);
      if(duplicate_id_flag != -1)
      {
        if(duplicate_id_flag == g_cur_page)
          SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012218_ID, CAV_ERR_1012218_MSG, "ID");
      }
      duplicate_id_flag = g_cur_page;

      ret = validate_id(attribute_value, url_idx);
      if(ret == -1){
        SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012321_ID, CAV_ERR_1012321_MSG);
      }
      else if(ret == -2){
        SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012326_ID, CAV_ERR_1012326_MSG);
      }
   
      requests[url_idx].proto.ws.conn_id = atoi(attribute_value);
      //TODO: Use hash 
      //ws_idx_list[requests[url_idx].proto.ws.conn_id] = url_idx;
      ws_idx_list[requests[url_idx].proto.ws.conn_id] = max_ws_conn;

      NSDL2_WS(NULL, NULL, "WebSocket Id = %d, max_ws_conn = %d", requests[url_idx].proto.ws.conn_id, max_ws_conn);

      max_ws_conn++;
      //if(max_ws_conn < requests[url_idx].proto.ws.conn_id)
        //max_ws_conn = requests[url_idx].proto.ws.conn_id;
      // Due to design issue we are unable to parmetrise ID.
      //segment_line(&(requests[url_idx].proto.ws.conn_id), attribute_value, 1, script_ln_no, sess_idx, flow_filename); 
      //NSDL2_WS(NULL, NULL, "WEBSOCKET: Value of %s = %s , segment offset = %d", 
      //                                                         attribute_name, attribute_value, requests[url_idx].proto.ws.conn_id);
    } 
    else if(!strcasecmp(attribute_name, "Origin"))
    {
      NSDL2_WS(NULL, NULL, "url_idx = %d, attribute_value = %s", url_idx, attribute_value);
      if((requests[url_idx].proto.ws.origin = copy_into_big_buf(attribute_value, 0)) == -1)
      {
        NSDL2_WS(NULL, NULL, "Error: failed copying data '%s' into big buffer", requests[url_idx].proto.ws.origin);
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, "CavErr[1000018]: ", CAV_ERR_1000018 + CAV_ERR_HDR_LEN, attribute_value);
      }
      NSDL2_WS(NULL, NULL, "origin = %d", requests[url_idx].proto.ws.origin); 
    }
    else if(!strcasecmp(attribute_name, "Header"))
    {
      NSDL2_WS(NULL, NULL, "Header %s found at script_line %d\n", attribute_value, script_ln_no);
      //set_ws_headers(attribute_value, headers_buf);
      if(set_headers(flow_fp, flow_filename, attribute_value, header_buf, sess_idx, url_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    /*else if (strcasecmp(attribute_name, "COOKIE") == 0)
    {
      NSDL2_PARSING(NULL, NULL, "Cookie found at script_line %d\n", script_ln_no);
      set_cookie(attribute_value, flow_filename, url_idx, sess_idx);
    }*/
    else if(!strcasecmp(attribute_name, "OnOpenCB") || !strcasecmp(attribute_name, "OnMessageCB") || 
            !strcasecmp(attribute_name, "OnErrorCB") || !strcasecmp(attribute_name, "OnCloseCB"))
    {
      /* Create and fill websocket_callback table */
      if(create_websocket_callback_table_entry(&ws_cb_idx) != SUCCESS)
      { 
        NSTL1_OUT(NULL, NULL, "Unable to create websocket callback table.\n");
      }
    
      NSDL2_WS(NULL, NULL, "attribute_value = %s, ws_cb_idx = %d", attribute_value, ws_cb_idx); 

      if((g_ws_callback[ws_cb_idx].cb_name = copy_into_big_buf(attribute_value, 0)) == -1)
      {
        NSDL2_WS(NULL, NULL, "Error: failed copying data '%s' into big buffer", g_ws_callback[ws_cb_idx].cb_name);
        return NS_PARSE_SCRIPT_ERROR;
      }
      NSDL2_WS(NULL, NULL, "after bigbuff g_ws_callback[ws_cb_idx].cb_name = %s", RETRIEVE_BUFFER_DATA(g_ws_callback[ws_cb_idx].cb_name));
    }

    ret = read_till_start_of_next_quotes(flow_fp, flow_filename, close_quotes, &start_quotes, 0, outfp);
    if (ret == NS_PARSE_SCRIPT_ERROR)
    {
      NSDL2_WS(NULL, NULL, "Next attribute is not found");
      break;
    }
  } //End while loop here

  if(!uri_exists)
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012211_ID, CAV_ERR_1012211_MSG, "URI");

  /* Adding \r\n at last header 
     Don't add \r\n here as \r\n is already added in make_ws_req */
  //strcat(header_buf, "\r\n");
	
  NSDL2_WS(NULL, NULL, "Segmenting header_buf = [%s]", header_buf);
  segment_line(&(requests[url_idx].proto.ws.hdrs), header_buf, 0, script_ln_no, sess_idx, flow_filename);
  NSDL2_WS(NULL, NULL, "NUM ENTERIES  = %d, requests = %p", requests[url_idx].proto.ws.hdrs.num_entries, requests);

  NSDL2_WS(NULL, NULL, "Exiting Method");
  return NS_PARSE_SCRIPT_SUCCESS;
}

/*--------------------------------------------------------------------------------------------- 
 * Purpose   : This function will parse ns_web_websocket_close() API and do follwing things 
 *             close the connection
 * 
 * Input     : flow_fp - pointer to input flow file
 *              ns_web_websocket_close("ID=0",
 *                                      "CODE=1000",
 *                                      "REASON=Any reson");
 *             outfp   - pointer to output flow file (made in $NS_WDIR/.tmp/ns-inst<nvm_id>/)
 *             flow_filename - flow file name 
 *             sess_idx- pointing to session index in gSessionTable 
 *
 * Output    : On success -  0
 *             On Failure - -1  
 *--------------------------------------------------------------------------------------------*/
int ns_parse_websocket_close(FILE *flow_fp, FILE *outfp, char *flow_filename, char *flowout_filename, int sess_idx)
{
  char attribute_name[128 + 1];
  char attribute_value[MAX_LINE_LENGTH + 1];
  char *close_quotes = NULL;
  char *start_quotes = NULL;
  int close_tb_idx = 0;
  int ret;

  NSDL2_WS(NULL, NULL, "Method Called, sess_idx = %d", sess_idx);
  if(create_websocket_close_table_entry(&close_tb_idx) != SUCCESS)
    SCRIPT_PARSE_ERROR(script_line, "Unable to create webSocket_close_table");

    start_quotes = strchr(script_line, '"'); //This will point to the starting quote '"'.
 
  //setting api ns_web_websocket_close here 
  if((set_api("ns_web_websocket_close", flow_fp, flow_filename, script_line, outfp, flowout_filename, close_tb_idx)) == NS_PARSE_SCRIPT_ERROR)
     NSDL2_WS(NULL, NULL, "Method Called starting_quotes = %p, starting_quotes = %s", start_quotes, start_quotes);
 
  while (1)
  {
    NSDL3_WS(NULL, NULL, "line = %s, close_tb_idx = %d, start_quotes = %s", script_line, close_tb_idx, start_quotes);
    ret = get_next_argument(flow_fp, start_quotes, attribute_name, attribute_value, &close_quotes, 0);
    if(ret == NS_PARSE_SCRIPT_ERROR) return NS_PARSE_SCRIPT_ERROR;
    
    if(!strcasecmp(attribute_name, "ID"))
    {
      NSDL2_WS(NULL, NULL, "ID [%s]", attribute_value);
      //validating id
      ret = is_ws_conid_exist(attribute_value, close_tb_idx);
      if(ret != 1)
      {
        SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012324_ID, CAV_ERR_1012324_MSG, attribute_value);
      }

      g_ws_close[close_tb_idx].conn_id = ws_idx_list[atoi(attribute_value)]; 
      
    }
    else if(!strcasecmp(attribute_name, "CODE"))
    {
      NSDL2_WS(NULL, NULL, "CODE [%s] ", attribute_value);
     //TODO validate code code should be b/w 1000-1012 
     //validate_code()
     g_ws_close[close_tb_idx].status_code = atoi(attribute_value);
    }
    else if(!strcasecmp(attribute_name, "REASON"))
    {
      NSDL2_WS(NULL, NULL, "URI [%s] ", attribute_value);
      strcpy(g_ws_close[close_tb_idx].reason, attribute_value);
    }
   
    ret = read_till_start_of_next_quotes(flow_fp, flow_filename, close_quotes, &start_quotes, 0, outfp);
    if (ret == NS_PARSE_SCRIPT_ERROR)
    {
      NSDL2_WS(NULL, NULL, "Next attribute is not found");
      break;
    }
  } //End while loop here

  NSDL2_WS(NULL, NULL, "Exiting Method");
  return NS_PARSE_SCRIPT_SUCCESS;
}

void copy_websocket_close_table_to_shr(void)
{
  int close_tb_idx;
  NSDL2_WS(NULL, NULL, "Method called. total_websocket_close_entries = %d", total_ws_close_entries);
  if(!total_ws_close_entries)
  {
    NSDL2_WS(NULL, NULL, "No websocket_close entries", total_ws_close_entries);
    return;
  }
  g_ws_close_shr = (ws_close_table_shr*) do_shmget(sizeof (ws_close_table_shr) * total_ws_close_entries, "ws_close_table_shr");
  for(close_tb_idx = 0; close_tb_idx < total_ws_close_entries ; close_tb_idx++)
  {
    NSDL2_WS(NULL, NULL, "Send Table data at ws_close_id = %d, conn_id = %d, status = %d, reason = %s",
                                       close_tb_idx, g_ws_close[close_tb_idx].conn_id,
                                       g_ws_close[close_tb_idx].status_code, g_ws_close[close_tb_idx].reason);
    g_ws_close_shr[close_tb_idx].conn_id = g_ws_close[close_tb_idx].conn_id;
    g_ws_close_shr[close_tb_idx].status_code = g_ws_close[close_tb_idx].status_code;
    strcpy(g_ws_close_shr[close_tb_idx].reason, g_ws_close[close_tb_idx].reason);
    NSDL2_WS(NULL, NULL, "Send Table Shar data at ws_close_id = %d, conn_id = %d, status_code = %p, reason = %s",
                                       close_tb_idx, g_ws_close_shr[close_tb_idx].conn_id, g_ws_close_shr[close_tb_idx].status_code,
                                       g_ws_close_shr[close_tb_idx].reason);
  }
}

//This function will print value of shared structure ws
//for testing only
//TODO Atul: take dump from shared memory .
/*
int ns_ws_data_structure_dump()
{
  NSDL2_WS(NULL, NULL, "Method called");

  int i = 0;
  int cb_idx = 0;

  if(!(global_settings->protocol_enabled && WS_PROTOCOL_ENABLED))
    return -1;

  NSDL1_WS(NULL, NULL, "Method called, total_ws_callback_entries = %d, total_ws_callback_entries = %d", 
                        total_ws_callback_entries, total_ws_callback_entries);

  NSDL2_PARSING(NULL, NULL, "WEBSOCKET Shared Memory Data Dump: start :-");
  for (i=0; i < total_ws_callback_entries; i++)
  {
    NSDL2_WS(NULL, NULL, "i = %d, origin = %p", i, request_table_shr_mem[i].proto.ws.origin);
    NSDL2_WS(NULL, NULL, "CONNECT DATA DUMP : request_table_shr_mem[i].proto.ws.origin = %s , request_table_shr_mem[i].proto.ws.conn_id = %d",
                          request_table_shr_mem[i].proto.ws.origin,
                          request_table_shr_mem[i].proto.ws.conn_id);

    NSDL2_WS(NULL,NULL, "SEND DATA DUMP :g_ws_send_shr[i].id  = %d, g_ws_send_shr[i].ws_idx = %d, buffer = %s, g_ws_send_shr[i].isbinary = %d",
                         g_ws_send_shr[i].id, g_ws_send_shr[i].ws_idx, g_post_buf, g_ws_send_shr[i].isbinary);
 
    for(cb_idx=0; cb_idx< total_ws_callback_entries; cb_idx++)
      NSDL2_WS(NULL, NULL, "g_ws_callback[cb_idx].cb_name = %s, cb_idx = %d", RETRIEVE_BUFFER_DATA(g_ws_callback[cb_idx].cb_name), cb_idx);
  }

  return 0;
}*/

int open_dev_urandom()
{
  uran_fd = open(SYSTEM_RANDOM_FILEPATH, O_RDONLY|O_CLOEXEC);
  if(uran_fd < 0)
  {
    NSTL1_OUT(NULL, NULL, "Unable to open file /dev/urandom, errno = %d (Error: %s)\n", errno, nslb_strerror(errno));
    return -1;
  }
  return 0;
}

int close_dev_urandom()
{
  close(uran_fd);
  return 0;
}

static void nslb_hexdump(void *vframe, size_t len)
{
  #define MAX_DUMP_SIZE           64*1024 //64 K
  int n;
  int m;
  int start;
  unsigned char *frame = (unsigned char *)vframe;
  char line[80];
  char *p;
  char *dump_buf=NULL; 
  int write_bytes = 0;
  int tot_write_bytes = 0;
  int available_bytes = MAX_DUMP_SIZE;

  /*if(dump_buf == NULL)
  {
    NSDL2_WS(NULL, NULL, "Memory allocation for dump_buf failed.");
    return;
  }
  */
  MY_MALLOC(dump_buf, MAX_DUMP_SIZE, "Allocate memory for nslb hexdump", -1);
  NSDL2_WS(NULL, NULL, "HexDump of frame pointing on %p of lenght %zu:  \n", vframe, len);
  for (n = 0; n < len;) {
    start = n;
    p = line;

    p += sprintf(p, "%04X: ", start);

    for (m = 0; m < 16 && n < len; m++)
      p += sprintf(p, "%02X ", frame[n++]);
    while (m++ < 16)
      p += sprintf(p, "   ");

    p += sprintf(p, "   ");

    for (m = 0; m < 16 && (start + m) < len; m++) {
      if (frame[start + m] >= ' ' && frame[start + m] < 127)
        *p++ = frame[start + m];
      else
        *p++ = '.';
    }
    while (m++ < 16)
      *p++ = ' ';

    *p++ = '\n';
    *p = '\0';
    write_bytes = snprintf(dump_buf + write_bytes, available_bytes, "%s", line);
    if(write_bytes >= available_bytes)
    {
      NSTL1(NULL, NULL, "Truncating buffer as its size is greater than %d\n", MAX_DUMP_SIZE);
      dump_buf[MAX_DUMP_SIZE] = 0; 
    }
    else
    {
      available_bytes -= write_bytes;
      tot_write_bytes += write_bytes;
    }
  }
  NSDL2_WS(NULL, NULL, " %d, %s", write_bytes, dump_buf);
  
  FREE_AND_MAKE_NOT_NULL(dump_buf, "dump_buf", -1);
  /*
  if(dump_buf) 
  {
    free(dump_buf);
    dump_buf = NULL;
  }
  */
}

/*--------------------------------------------------------------------------------------------- 
 * Name      : make_ws_frame()
 *
 * Purpose   : This function will make websocket frame according to RFC6455. Frame foramt - 
               
                   0                   1                   2                   3
                   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
                  +-+-+-+-+-------+-+-------------+-------------------------------+
                  |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
                  |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
                  |N|V|V|V|       |S|             |   (if payload len==126/127)   |
                  | |1|2|3|       |K|             |                               |
                  +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
                  |     Extended payload length continued, if payload len == 127  |
                  + - - - - - - - - - - - - - - - +-------------------------------+
                  |                               |Masking-key, if MASK set to 1  |
                  +-------------------------------+-------------------------------+
                  | Masking-key (continued)       |          Payload Data         |
                  +-------------------------------- - - - - - - - - - - - - - - - +
                  :                     Payload Data continued ...                :
                  + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
                  |                     Payload Data continued ...                |
                  +---------------------------------------------------------------+

                  Where:
                     FIN:  1 bit
                        Indicates that this is the final fragment in a message.  The first
                        fragment MAY also be the final fragment.
      
                     RSV1, RSV2, RSV3:  1 bit each
                        MUST be 0 unless an extension is negotiated that defines meanings
                        for non-zero values.  If a nonzero value is received and none of
                        the negotiated extensions defines the meaning of such a nonzero
                        value, the receiving endpoint MUST _Fail the WebSocket
      
                     Opcode:  4 bits
                        Defines the interpretation of the "Payload data".  If an unknown
                        opcode is received, the receiving endpoint MUST _Fail the
                        WebSocket Connection_.  The following values are defined.
      
                        *  %x0 denotes a continuation frame
                        *  %x1 denotes a text frame
                        *  %x2 denotes a binary frame
                        *  %x3-7 are reserved for further non-control frames
                        *  %x8 denotes a connection close
                        *  %x9 denotes a ping
                        *  %xA denotes a pong
                        *  %xB-F are reserved for further control frames
      
                     Mask:  1 bit
                        Defines whether the "Payload data" is masked.  If set to 1, a
                        masking key is present in masking-key, and this is used to unmask
                        the "Payload data" as per Section 5.3.  All frames sent from
                        client to server have this bit set to 1.
      
                     Payload length:  7 bits, 7+16 bits, or 7+64 bits
                        The length of the "Payload data", in bytes: if 0-125, that is the
                        payload length.  If 126, the following 2 bytes interpreted as a
                        16-bit unsigned integer are the payload length.  If 127, the
                        following 8 bytes interpreted as a 64-bit unsigned integer (the
                        most significant bit MUST be 0) are the payload length.  Multibyte
                        length quantities are expressed in network byte order.  Note that
                        in all cases, the minimal number of bytes MUST be used to encode
                        the length, for example, the length of a 124-byte-long string
                        can't be encoded as the sequence 126, 0, 124.  The payload length
                        is the length of the "Extension data" + the length of the
                        "Application data".  The length of the "Extension data" may be
                        zero, in which case the payload length is the length of the
                        "Application data".
      
                     Masking-key:  0 or 4 bytes
                        All frames sent from the client to the server are masked by a
                        32-bit value that is contained within the frame.  This field is
                        present if the mask bit is set to 1 and is absent if the mask bit
                        is set to 0.  See Section 5.3 for further information on client-
                        to-server masking.
      
                     Payload data:  (x+y) bytes
                        The "Payload data" is defined as "Extension data" concatenated
                        with "Application data".
      
                     Extension data:  x bytes
                        The "Extension data" is 0 bytes unless an extension has been
                        negotiated.  Any extension MUST specify the length of the
                        "Extension data", or how that length may be calculated, and how
                        the extension use MUST be negotiated during the opening handshake.
                        If present, the "Extension data" is included in the total payload
                        length.
      
                     Application data:  y bytes
                        Arbitrary "Application data", taking up the remainder of the frame
                        after any "Extension data".  The length of the "Application data"
                        is equal to the payload length minus the length of the "Extension
                        data".


 *              
 * Input Args: frame           - output framefer to store Frame  
 *             payload         - message which has to be send to server 
 *             payload_len     - message lenght 
 *             opcode          - operation code, this will tell message is text or binary 
 *
 * Output    : On success -  0
 *             On Failure - -1  
 *--------------------------------------------------------------------------------------------*/
int nslb_make_ws_frame(int write_fin, int masked7, int opcode, size_t payload_len, char *payload)
{
  int n;
  //int pre = 0;
  //int post = 0;    
  int write_idx = 0;
  char *frame = NULL;
  //int masked7 = 1;
  //unsigned char *dropmask = NULL;
  unsigned char is_masked_bit = 0;
  unsigned int max_frame_size = WS_SEND_BUFFER_PRE_PADDING + payload_len + WS_SEND_BUFFER_POST_PADDING;
  unsigned char frame_masking_key[4];
  unsigned char frame_mask_index = 0;
  int tot_frame_len = 0;
  //int idx;

  NSDL2_WS(NULL, NULL, "Method Called, write_fin = %d, masked7 = %d, ws_send_frame = %p, payload = [%s], payload_len = %zu, opcode = %d\n",
                         write_fin, masked7, ws_send_frame, payload, payload_len, opcode);
  if(ws_send_frame == NULL || g_max_frame_size < max_frame_size)
  {
    NSDL2_WS(NULL, NULL, "ws_send_frame is null going to allocate max_frame_size = %d", max_frame_size);
    MY_REALLOC(ws_send_frame, max_frame_size, "Allocate memory for ws frame", -1);
    g_max_frame_size += (max_frame_size - g_max_frame_size);
  }

  frame = ws_send_frame;
  /* Fill first byte of frame - 
            +-+-+-+-+-+-+-+-+           
            |F|R|R|R|-|-|-|-| 
            +-+-+-+-+-+-+-+-+
                    <-opcode->
  */
  if(masked7)
  {
    is_masked_bit = 1 << 7;
  }

  //n = opcode;

  switch(opcode & 0xf)
  {
    case LWS_WRITE_TEXT:
      n = LWS_WS_OPCODE_07__TEXT_FRAME;
      break;
    case LWS_WRITE_BINARY:
      n = LWS_WS_OPCODE_07__BINARY_FRAME;
      break;
    case LWS_WRITE_CONTINUATION:
      n = LWS_WS_OPCODE_07__CONTINUATION;
      break;
    case LWS_WRITE_CLOSE:
      n = LWS_WS_OPCODE_07__CLOSE;
      
      /*
       * 06+ has a 2-byte status code in network order
       * we can do this because we demand post-frame
       */

      break;
    case LWS_WRITE_PING:
      n = LWS_WS_OPCODE_07__PING;
      break;
    case LWS_WRITE_PONG:
      n = LWS_WS_OPCODE_07__PONG;
      break;
    default:
      NSTL1_OUT(NULL, NULL, "nslb_make_ws_frame(): unknow write opcode \n");
      return -1;
  }

  NSDL2_WS(NULL, NULL, "value of n is = %d", n);
  if(write_fin)
    n |= 1 << 7;

  /* Fill mask and payload len in frame (i.e. next 1 or 3 or 9 byte according to payload len)*/
  NSDL2_WS(NULL, NULL, "Write Fin bit, opcode, mask bit and payload len in frame from idx = %d\n", write_idx);
  if(payload_len < 126)
  {
    frame[write_idx++] = n;
    frame[write_idx++] = payload_len | is_masked_bit;
  }
  else
  {
    if(payload_len < 65536)
    {
      frame[write_idx++] = n;
      frame[write_idx++] = 126 | is_masked_bit;
      frame[write_idx++] = payload_len >> 8;
      frame[write_idx++] = payload_len;
    }
    else
    {
      //TODO: Check what is the meaning of getting __LP64__
      frame[write_idx++] = n;
      frame[write_idx++] = 127 | is_masked_bit;

      frame[write_idx++] = (payload_len >> 56) & 0x7f;
      frame[write_idx++] = payload_len >> 48;
      frame[write_idx++] = payload_len >> 40;
      frame[write_idx++] = payload_len >> 32;
      frame[write_idx++] = payload_len >> 24;
      frame[write_idx++] = payload_len >> 16;
      frame[write_idx++] = payload_len >> 8;
      frame[write_idx++] = payload_len;
    }
  }

  /* Fill masking key in the frame */
  NSDL2_WS(NULL, NULL, "Write masking key of len 4 in frame on idx = %d\n", write_idx);
  if(masked7)
  {
    //generate random key - Read /dev/urandom
    if((n = read(uran_fd, frame_masking_key, 4)) != 4)
    {
      NSTL1_OUT(NULL, NULL, "Error: Unable to read from random device %s %d\n", SYSTEM_RANDOM_FILEPATH, n);
      return -1;
    }

    //memcpy(frame_masking_key, "1234", 4);
    frame_mask_index = 0;

    NSDL2_WS(NULL, NULL, "Masking key: \n");
    nslb_hexdump(frame_masking_key, 4);


    memcpy(&frame[write_idx], frame_masking_key, 4);
    write_idx += 4;  //skip mask bytes

    NSDL2_WS(NULL, NULL, "Write payload of len %zu in frame on idx = %d\n", payload_len, write_idx);
    memcpy(&frame[write_idx], payload, payload_len);

    //idx = write_idx;
    //Mask payload with this maske key 
    for(n = 0; n < payload_len; n++)
    {
      //NSDL4_WS(NULL, NULL, "write_idx = %d, id = %d, Payload character = %c, maske key character = %0x\n",
      //                 write_idx, (frame_mask_index & 3), frame[write_idx], (unsigned char)frame_masking_key[frame_mask_index & 3]);
      frame[write_idx] = frame[write_idx] ^ frame_masking_key[frame_mask_index++ & 3];
      write_idx++;
    }

    //Below code is just for testing purpose only
    /*unsigned char buffer[1024];
      unsigned char frame_msk_idx = 0;
    //unmasking the payload 
    for(n = 0; n < payload_len; n++)
    {
      NSDL4_WS(NULL, NULL, "mask id = %d, frame = %0x, unmakin key = %02x\n", (frame_msk_idx & 3), (unsigned char)frame[idx], 
                                          (unsigned char)frame_masking_key[frame_msk_idx & 3]);
      buffer[n] = frame[idx] ^ frame_masking_key[frame_msk_idx++ & 3];
      idx++;
    }
    //printf("buffer = %s\n", buffer);
    NSDL2_WS(NULL, NULL, "After unmasking buffer is: %s", buffer);
    nslb_hexdump(buffer, payload_len);  */
   
  }
  else
  {
    NSDL2_WS(NULL, NULL, "Write payload of len %zu in frame on idx = %d\n", payload_len, write_idx);
    memcpy(&frame[write_idx], payload, payload_len);
    write_idx += payload_len;
  }
  tot_frame_len = write_idx;

  NSDL2_WS(NULL, NULL, "Frame Dump: tot_frame_len = %d\n", tot_frame_len);
  nslb_hexdump(frame, tot_frame_len);

  return tot_frame_len;
}

void inline on_sending_ws_frame_done(connection* cptr)
{
  IW_UNUSED(VUser *vptr = cptr->vptr);

  u_ns_ts_t now;

  NSDL2_WS(vptr, cptr, "Method called, cptr=[%p]", cptr);

  now = get_ms_stamp();
  //Calculate write complete time diff
  cptr->write_complete_time = now - cptr->ns_component_start_time_stamp;
  cptr->ns_component_start_time_stamp = now;//Update NS component start time stamp
  NSDL2_WS(vptr, cptr, "ws frame write complete done at = %d(sec), Update NS component start time stamp = %u", 
                             cptr->write_complete_time, cptr->ns_component_start_time_stamp);
  
  /* This has to be reset so we know there was no partial write */
  cptr->bytes_left_to_send = 0;
  
  NSDL2_WS(vptr, cptr, "Set cptr->conn_state: before set cptr = %p, cptr->conn_state = %d, request_type = %d", 
                        cptr, cptr->conn_state, cptr->url_num->request_type); 
  switch(cptr->url_num->request_type) {
    case WS_REQUEST:
    case WSS_REQUEST:
      //cptr->conn_state = CNST_WS_FRAME_READING;  // TODO: do we need to add timeout timer
      cptr->conn_state = CNST_WS_IDLE;  // TODO: do we need to add timeout timer
      return;
    break;
  }
#ifndef USE_EPOLL
  FD_SET( cptr->conn_fd, &g_rfdset );
#endif
} 

void handle_send_ws_frame(connection* cptr, u_ns_ts_t now) 
{
  struct iovec *vector_ptr, *start_vector;
  char *free_array;
  int num_vectors;
  int amt_writ;
  int i,j;
  int bytes_sent, bytes_to_send;

#ifndef NS_DEBUG_ON
  VUser *vptr;
#endif

  if(cptr->conn_fd < 0)
  {
    NSTL1_OUT(NULL, NULL, "Error: cannot send frame because of conn_fd = %d", cptr->conn_fd);
    return;
  }

  NSDL2_WS(NULL, cptr, "Method called: cptr=%p state=%d ", cptr, cptr->conn_state);

  start_vector = cptr->send_vector + cptr->first_vector_offset;

  // we have not written header yet in case of 100 Continue
  vector_ptr = start_vector; 
  // Num vectors to be written till body index
  num_vectors = cptr->body_index + 1;  // its an index so to get count added 1
  /* if header is not send than we need to set*/
  bytes_to_send = cptr->bytes_left_to_send - cptr->content_length;
  NSDL2_WS(NULL, cptr, "Setting num vectors (as 100 contine response has came)"
			    "body_index = %d, num_send_vectors = %d,"
			    "bytes_left_to_send = %d, content_length = %d",
			    cptr->body_index, num_vectors, cptr->bytes_left_to_send,
			    cptr->content_length);

  free_array = cptr->free_array;

  if (cptr->bytes_left_to_send == 0) /* nothing to send */
  {
    NSDL2_WS(NULL, cptr, "Handle write called with bytes_left_to_send 0. Should not come here");
    return;
  }

  bytes_sent = writev(cptr->conn_fd, start_vector, num_vectors - cptr->first_vector_offset);
  if (bytes_sent < 0) 
  {
    NSEL_MIN(NULL, cptr, ERROR_ID, ERROR_ATTR, "Error: (%s) in writing vector,"
                                                "sending HTTP request failed fd = %d,"
                                                "num_vectors = %d, %s",
                                                nslb_strerror(errno), cptr->conn_fd, num_vectors,
                                                get_url_req_url(cptr));

    retry_connection(cptr, now, NS_REQUEST_WRITE_FAIL); //ERR_ERR
    return;
  }
  cptr->tcp_bytes_sent += bytes_sent;

  NSDL3_HTTP(NULL, cptr, "byte sent = %d, bytes_to_send = %d", bytes_sent, bytes_to_send);

  if (bytes_sent < bytes_to_send) 
  {
    amt_writ = 0;
    NSDL2_WS(NULL, cptr, "cptr->first_vector_offset = %d, num_vectors = %d", cptr->first_vector_offset ,num_vectors);
    for (i =  cptr->first_vector_offset, vector_ptr = start_vector; i < num_vectors; i++, vector_ptr++) 
    {
      amt_writ += vector_ptr->iov_len;
      if (bytes_sent >= amt_writ) 
      {
        //This vector completely written
        NSDL2_WS(NULL, cptr, "This vector completely written");
#ifdef NS_DEBUG_ON
        debug_log_http_req(cptr, vector_ptr->iov_base, vector_ptr->iov_len, 1, 0);
#else
        LOG_HTTP_REQ(cptr, vector_ptr->iov_base, vector_ptr->iov_len, 1, 0);
#endif
        free_cptr_vector_idx(cptr, i);
      } 
      else 
      {
        NSDL2_WS(NULL, cptr, "This vector partilally written for i = %d", i);
#ifdef NS_DEBUG_ON

       debug_log_http_req(cptr, vector_ptr->iov_base, vector_ptr->iov_len - (amt_writ-bytes_sent), 0, 0);
#else
       LOG_HTTP_REQ(cptr, vector_ptr->iov_base, vector_ptr->iov_len - (amt_writ-bytes_sent), 0, 0);
#endif
        //This vector only partilally written
        NSDL2_WS(NULL, cptr, "before cptr->last_iov_base=%p,vector_ptr->iov_base=%p",cptr->last_iov_base, vector_ptr->iov_base);
        // Since one vector element can be partial many times, we need to keep it's pointer in 
        // last_iov_based only once which the buffer start address as iov_based keep changing
        // We need to save in last_iov_based in two conditions:
        //   1. If it is not set (it is NULL)
        //   2. If was set but not that vector element is completely writen and another element is paritail
        //   Note condiiton is 2 is also handled by checking NULL as we are freeing last_iov_base and making NULL above
        /* Shalu: we will set last_iov_base only in case if vector need to be free. if we are setting it without checking it will not be null*/
        if((cptr->last_iov_base == NULL) && (free_array[i] & NS_IOVEC_FREE_FLAG))
          cptr->last_iov_base = vector_ptr->iov_base;

        NSDL2_WS(NULL, cptr, "after cptr->last_iov_base=%p",cptr->last_iov_base);
        cptr->first_vector_offset = i;
        vector_ptr->iov_base = vector_ptr->iov_base + vector_ptr->iov_len - (amt_writ-bytes_sent);
        vector_ptr->iov_len = amt_writ-bytes_sent;
        cptr->bytes_left_to_send -= bytes_sent;
#ifndef USE_EPOLL
        FD_SET( cptr->conn_fd, &g_rfdset );
#endif
        return;
      }
    }
  }
  else
  {
#ifdef NS_DEBUG_ON
    // All vectors written
      for (j = cptr->first_vector_offset, vector_ptr = start_vector; j < num_vectors; j++, vector_ptr++)
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
      for (j = cptr->first_vector_offset, vector_ptr = start_vector; j < num_vectors; j++, vector_ptr++)
      { 
        if(j == num_vectors - 1) {
          LOG_HTTP_REQ(cptr, vector_ptr->iov_base, vector_ptr->iov_len, 1, 1);
        }
        else {
          LOG_HTTP_REQ(cptr, vector_ptr->iov_base, vector_ptr->iov_len, 0, 0);
        }
      }
#endif
  }

  /* We are done with writing in case of Expect: 100-continue,
   * so we need to return as we can not free free_array & some
   * other things done after follwoing block  
   */

  if (cptr->conn_state == CNST_REUSE_CON)
    cptr->req_code_filled = -2;

  free_cptr_send_vector(cptr, num_vectors);
 
  on_sending_ws_frame_done(cptr);
}

void handle_incomplete_send_ws_frame(connection* cptr,  NSIOVector *ns_iovec, int num_vectors, int bytes_to_send, int bytes_sent)
{
  struct iovec *start_vector;
  char *start_array;
  int left_vectors;
  int amt_writ;
  int i,j;

  struct iovec *vector_ptr = ns_iovec->vector;
#ifndef NS_DEBUG_ON
  VUser* vptr = cptr->vptr;
#endif

  NSDL2_WS(NULL, cptr, "handle_incomplete_send_ws_frame(): cptr=%p, state=%d , num_vectors=%d, bytes_to_send=%d, bytes_sent=%d", 
                                                           cptr, cptr->conn_state, num_vectors, bytes_to_send, bytes_sent);

  amt_writ = 0;
  cptr->bytes_left_to_send = bytes_to_send - bytes_sent;
  for (i = 0; i < num_vectors; i++, vector_ptr++) 
  {
    amt_writ += vector_ptr->iov_len;
    NSDL3_HTTP(NULL, cptr, "i=%d, amt_writ=%d, vector_ptr %p vector_ptr->iov_len=%d", i, amt_writ, vector_ptr, vector_ptr->iov_len);
    if (bytes_sent >= amt_writ) 
    {
      //This vector completely written
      NSDL3_HTTP(NULL, cptr, "This vector completely written, cptr->request_type = %d", cptr->request_type);
      switch(cptr->request_type) {
        case WS_REQUEST:
#ifdef NS_DEBUG_ON
            debug_log_http_req(cptr, vector_ptr->iov_base, vector_ptr->iov_len, 0, 0);
#else
            LOG_HTTP_REQ(cptr, vector_ptr->iov_base, vector_ptr->iov_len, 0, 0);
#endif
          break;
      }
      NS_FREE_IOVEC(*ns_iovec, i)
    } 
    else
    {
      NSDL3_HTTP(NULL, cptr, "This vector (%d) partilally written, amt_writ = %d, bytes_sent = %d, iov_len = %d, bytes_to_send = %d",
			      i, amt_writ, bytes_sent, vector_ptr->iov_len, bytes_to_send);
      switch(cptr->request_type) {
        case WS_REQUEST:
#ifdef NS_DEBUG_ON
            debug_log_http_req(cptr, vector_ptr->iov_base, vector_ptr->iov_len - (amt_writ-bytes_sent), 0, 0);
#else
            LOG_HTTP_REQ(cptr, vector_ptr->iov_base, vector_ptr->iov_len - (amt_writ-bytes_sent), 0, 0);
#endif
          break;
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
      if((cptr->last_iov_base == NULL) && (NS_IS_IOVEC_FREE(*ns_iovec, i))){
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

  cptr->conn_state = CNST_WS_FRAME_WRITING;
  cptr->tcp_bytes_sent = bytes_sent;

  NSDL3_HTTP(NULL, cptr,  "[cptr=%p]: Request successfully sent, cptr->num_send_vectors = %d, cptr->tcp_bytes_sent = %d", 
                                      cptr, cptr->num_send_vectors, cptr->tcp_bytes_sent);

  NS_RESET_IOVEC(*ns_iovec);
  #ifndef USE_EPOLL
    FD_SET( cptr->conn_fd, &g_wfdset );
  #endif
}

/* Return Status:
   Partial Write   -2 
   Failure         -1
   Success          0
*/

static int send_ws_frame(connection *cptr, int frame_len, u_ns_ts_t now)
{
  VUser *vptr = cptr->vptr;
  int bytes_sent;

  NSDL2_WS(vptr, cptr, "send websocket msg: cptr = %p, state = %d, frame_len = %d, num_vectors = %d",
                                            cptr, cptr->conn_state, frame_len, NS_GET_IOVEC_CUR_IDX(g_req_rep_io_vector));

  if((bytes_sent = writev(cptr->conn_fd, g_req_rep_io_vector.vector, NS_GET_IOVEC_CUR_IDX(g_req_rep_io_vector))) < 0) {
    NSTL1_OUT(NULL, NULL, "Sending websocket buffer failed fd = [%d], Error = [%s]\n", cptr->conn_fd, nslb_strerror(errno));
    NS_FREE_RESET_IOVEC(g_req_rep_io_vector);
    //TODO: retry do or not if yes then we should callback or not???
    return -1;
  }

  if(set_socketopt_for_quickack(cptr->conn_fd, 1) < 0) {
    NS_FREE_RESET_IOVEC(g_req_rep_io_vector);
    return -1;
  }

  if (bytes_sent < frame_len) {
    NSDL2_WS(vptr, cptr, "Complete ws frame not send: bytes_sent = %d, frame_len = %d",
                                                      bytes_sent, frame_len);

    handle_incomplete_send_ws_frame(cptr, &g_req_rep_io_vector, NS_GET_IOVEC_CUR_IDX(g_req_rep_io_vector), frame_len, bytes_sent);
    return -2;
  }

#ifdef NS_DEBUG_ON
  // Complete data send, so log all vectors in req file
  int i;
  for(i = 0; i < NS_GET_IOVEC_CUR_IDX(g_req_rep_io_vector); i++)
  {
    if(i == (NS_GET_IOVEC_CUR_IDX(g_req_rep_io_vector) - 1)) {
      NSDL4_WS(vptr, cptr, "Log req idx = %d", i);
      debug_log_http_req(cptr, g_req_rep_io_vector.vector[i].iov_base, g_req_rep_io_vector.vector[i].iov_len, 1, 1);
    }
    else {
      NSDL4_WS(vptr, cptr, "Log req idx = %d", i);
      debug_log_http_req(cptr, g_req_rep_io_vector.vector[i].iov_base, g_req_rep_io_vector.vector[i].iov_len, 0, 0);
    }
  }
#else
  // Complete data send, so log all vectors in req file
  int i;
  for(i = 0; i < NS_GET_IOVEC_CUR_IDX(g_req_rep_io_vector); i++)
  {
    if(i == (NS_GET_IOVEC_CUR_IDX(g_req_rep_io_vector) - 1)) {
      LOG_HTTP_REQ(cptr, g_req_rep_io_vector.vector[i].iov_base, g_req_rep_io_vector.vector[i].iov_len, 1, 1);
    }
    else {
      LOG_HTTP_REQ(cptr, g_req_rep_io_vector.vector[i].iov_base, g_req_rep_io_vector.vector[i].iov_len, 0, 0);
    }
  }
#endif

  NS_FREE_RESET_IOVEC(g_req_rep_io_vector);
  cptr->tcp_bytes_sent += bytes_sent;
  average_time->tx_bytes += bytes_sent;

  //On_request_write_done 
  on_sending_ws_frame_done(cptr);

  return 0;
}

int ws_send_pong(connection *cptr)
{
  int total_frame_len = 0;
  int ret = 0;

  u_ns_ts_t now = get_ms_stamp();

  NSDL2_WS(NULL, NULL, "Method Called");

  NSDL2_WS(NULL, cptr, "cptr = %p, request type = %d, cptr->conn_fd = %d, cptr->req_ok = %d, cptr->conn_state = %d", 
                        cptr, cptr->request_type, cptr->conn_fd, cptr->req_ok, cptr->conn_state);

  if(cptr->conn_fd < 0)
  {
    NSTL1(NULL, NULL, "Error: cannot send WS message because conn_fd = %d, cptr->conn_state = %d, cptr = %p", 
                       cptr->conn_fd, cptr->conn_state, cptr);
    NSDL2_WS(NULL, NULL, "Error: cannot send WS message because conn_fd = %d, cptr->conn_state = %d, cptr = %p", 
                         cptr->conn_fd, cptr->conn_state, cptr);
    return -1;
  }

  total_frame_len = nslb_make_ws_frame(1, 0, LWS_WRITE_PONG, 0, NULL);
  
  NS_RESET_IOVEC(g_req_rep_io_vector);
  //stored the frame in vector list
  NS_FILL_IOVEC(g_req_rep_io_vector, ws_send_frame, total_frame_len);

  cptr->send_vector = g_req_rep_io_vector.vector;

  NSDL4_HTTP(NULL, cptr, "total_frame_len = %d", total_frame_len);
  //Update avg count of msg send bytes
  cptr->conn_state = CNST_WS_FRAME_WRITING;

  #ifdef ENABLE_SSL
  /* Handle SSL write message */
  if(cptr->request_type == WSS_REQUEST)
  {
    NSDL4_HTTP(NULL, cptr, "Sending WSS Connect request ws_req_size = %d, num_vectors = %d", total_frame_len, NS_GET_IOVEC_CUR_IDX(g_req_rep_io_vector));
    copy_request_into_buffer(cptr, total_frame_len, &g_req_rep_io_vector);

    if(set_socketopt_for_quickack(cptr->conn_fd, 1) < 0)
    {
      return -1;
    }

    ret = handle_ssl_write(cptr, now);
    if(ret == WS_SSL_ERROR)
    {
      NSDL4_HTTP(NULL, cptr, "WSS: send partial data for cptr = %p, conn_fd = %d", cptr, cptr->conn_fd);
      ret = -2;
    }
  }
  #endif
  else
  {
    ret = send_ws_frame(cptr, total_frame_len, now);
  }
  
  return ret;
}

int nsi_websocket_send(VUser *vptr)
{
  connection *cptr;
  int len = 0, total_frame_len = 0;
  int masked7 = 1, write_fin = 1;
  int send_idx = vptr->ws_send_id;
  int opt = 0;
  int ret = 0;

  u_ns_ts_t now = get_ms_stamp();

  NSDL2_WS(vptr, NULL, "Method Called, Send Table data at ws_send_id = %d, conn_id = %d, "
                       "send_buf.seg_statr = %p, send_buf.num_enties = %d, g_ws_send_shr[send_idx].ws_id = %d, "
                       "isbinary = %d, ws_conn id = %d, now = %u", 
                                       send_idx, g_ws_send_shr[send_idx].id, 
                                       g_ws_send_shr[send_idx].send_buf.seg_start, g_ws_send_shr[send_idx].send_buf.num_entries, 
                                       g_ws_send_shr[send_idx].ws_idx,
                                       g_ws_send_shr[send_idx].isbinary, ws_idx_list[send_idx], now); 

  cptr = vptr->ws_cptr[g_ws_send_shr[send_idx].ws_idx];

  if(cptr == NULL)
  {
    NSTL1(NULL, NULL, "Error: cptr is NULL for conn_id = %d , ws_conn id = %d ",
                      g_ws_send_shr[send_idx].id , ws_idx_list[send_idx]);
    INC_WS_MSG_SEND_FAIL_COUNTER(vptr);
    return -1;
  }

  NSDL2_WS(vptr, cptr, "cptr = %p, request type = %d, cptr->conn_fd = %d, cptr->req_ok = %d, cptr->conn_state = %d", 
                        cptr, cptr->request_type, cptr->conn_fd, cptr->req_ok, cptr->conn_state);

  if(cptr->conn_fd < 0)
  {
    NSTL1(NULL, NULL, "Error: cannot send WS message because conn_fd = %d, cptr->conn_state = %d, cptr = %p", 
                       cptr->conn_fd, cptr->conn_state, cptr);
    NSDL2_WS(NULL, NULL, "Error: cannot send WS message because conn_fd = %d, cptr->conn_state = %d, cptr = %p", 
                         cptr->conn_fd, cptr->conn_state, cptr);
    //TODO: 
    //retry_connection(cptr, now, NS_REQUEST_WRITE_FAIL);
    INC_WS_MSG_SEND_FAIL_COUNTER(vptr);
    return -1;
  }

  if((len = get_values_from_segments(cptr, &ws_send_buffer, &ws_send_buff_len, &g_ws_send_shr[send_idx].send_buf, "send_buf", 1024, REQ_PART_BODY)) <= 0){
    NSDL2_WS(NULL, NULL, "Error: in getting value from segment for send api. len = %d", len);
    NSTL1_OUT(NULL, NULL, "Error in getting value from segment for send api object id.\n");
    end_test_run();
  }

  //len = strlen(ws_send_buffer);
  //NSDL2_WS(vptr, cptr, "Send msg after parametrisation - %s, len = %d, address = %p", ws_send_buffer, len, ws_send_buffer);

  if(g_ws_send_shr[send_idx].isbinary == 0) {
    opt |= LWS_WRITE_TEXT;
  }
  else if(g_ws_send_shr[send_idx].isbinary == 1) {
    opt |= LWS_WRITE_BINARY;
  }
  else {
    opt |= LWS_WRITE_CLOSE;
  }
 
  NSDL2_WS(vptr, cptr, "Set the opcode for WS: opcode = %d", opt);
  open_dev_urandom();
  total_frame_len = nslb_make_ws_frame(write_fin, masked7, opt, len, ws_send_buffer);
  
  NS_RESET_IOVEC(g_req_rep_io_vector);
  //stored the frame in vector list
  NS_FILL_IOVEC(g_req_rep_io_vector, ws_send_frame, total_frame_len);
  close_dev_urandom();

  cptr->send_vector = g_req_rep_io_vector.vector;

  NSDL4_HTTP(NULL, cptr, "total_frame_len = %d", total_frame_len);
  //Update avg count of msg send bytes
  INC_WS_TX_BYTES_COUNTER(vptr, total_frame_len);

  cptr->conn_state = CNST_WS_FRAME_WRITING;

  #ifdef ENABLE_SSL
  /* Handle SSL write message */
  if(cptr->request_type == WSS_REQUEST)
  {
    NSDL4_HTTP(NULL, cptr, "Sending WSS Connect request ws_req_size = %d, num_vectors = %d", total_frame_len, NS_GET_IOVEC_CUR_IDX(g_req_rep_io_vector));
    copy_request_into_buffer(cptr, total_frame_len, &g_req_rep_io_vector);

    if(set_socketopt_for_quickack(cptr->conn_fd, 1) < 0)
    {
      //TODO: Handle retry connction 
      //retry_connection(cptr, now, NS_REQUEST_CONFAIL);
      //websocket_close_connection(cptr, 1, now, NS_REQUEST_CONFAIL, NS_COMPLETION_WS_ERROR);
      INC_WS_MSG_SEND_FAIL_COUNTER(vptr);
      return -1;
    }

    ret = handle_ssl_write(cptr, now);
    //if(cptr->conn_state == CNST_SSL_WRITING)
    if(ret == WS_SSL_ERROR)
    {
      NSDL4_HTTP(NULL, cptr, "WSS: send partial data for cptr = %p, conn_fd = %d", cptr, cptr->conn_fd);
      ret = -2;
    }
  }
  #endif
  else
  {
    ret = send_ws_frame(cptr, total_frame_len, now);
  }
  
  if(ret && (ret != WS_SSL_PARTIAL_WRITE)) //In case of ssl partial send we will not increase send fail counter
  {
    INC_WS_MSG_SEND_FAIL_COUNTER(vptr);   //Updated avg counter for failed msg
  }


  /********************************************************
    Don't switch to VUser contex in case of partial send
   ********************************************************/
  if(ret != WS_SSL_PARTIAL_WRITE) 
  {
    cptr->conn_state = CNST_WS_IDLE;
    if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT)
      switch_to_vuser_ctx(vptr, "SwitcToVUser: nsi_websocket_send(): Websocket send frame done.");
    else if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_SEPARATE_THREAD)
      send_msg_nvm_to_vutd(vptr, NS_API_WEBSOCKET_SEND_REP, 0);   
    else if(vptr->sess_ptr->script_type == NS_SCRIPT_TYPE_JAVA)
     {
      send_msg_to_njvm(vptr->mcctptr, NS_NJVM_API_WEB_WEBSOCKET_SEND_REP, 0);
      return 0;
     }
  }
  //TODO Free frame here
  return ret;
}

int nsi_web_websocket_read(VUser *vptr, int con_id, int timeout)
{
  connection *cptr = NULL;
  ClientData client_data;
  u_ns_ts_t now = 0;

  NSDL2_WS(vptr, NULL, "Method called, con_id = %d, timeout = %d, vptr = %p, ws_con id = %d, node_ptr = %p", 
                            con_id, timeout, vptr, ws_idx_list[con_id], (TxInfo *) vptr->tx_info_ptr); 

  if(!vptr)
  {
    NSDL2_WS(vptr, NULL, "vpt is not set = %p", vptr);
    return -1;
  }

  cptr = vptr->ws_cptr[ws_idx_list[con_id]];
  if(!cptr)
  {
    NSDL2_WS(vptr, NULL, "cptr is not set = %p", cptr);
    return -1;
  }

  NSDL2_WS(vptr, NULL, "cptr->conn_state = %d, cptr->req_ok = %d", cptr->conn_state, cptr->req_ok);

  //Bug 36316 - WebSocket Protocol : Getting wrong data in "WebSocket TCP connections" stats in case of ConFail.
  if(cptr->conn_fd < 0)
  {
    NSTL1(NULL, NULL, "Error: cannot read WebSocket message because conn_fd = %d, cptr->conn_state = %d, cptr = %p",
                       cptr->conn_fd, cptr->conn_state, cptr);
    NSDL2_WS(vptr, NULL, "Error: cannot read WebSocket message because conn_fd = %d, cptr->conn_state = %d, cptr = %p",
                          cptr->conn_fd, cptr->conn_state, cptr);

    return -1;
  }

  cptr->conn_state = CNST_WS_FRAME_READING;

  NSDL2_WS(vptr, NULL, "Read frames on cptr = %p", cptr);

  /* Adding timer to read response */
  memset(&client_data, 0, sizeof(client_data));
  client_data.p = cptr;
  now = get_ms_stamp();

  cptr->timer_ptr->actual_timeout = timeout;
  dis_timer_add_ex(AB_TIMEOUT_IDLE, cptr->timer_ptr, now, ws_resp_timeout, client_data, 0, 0);
 
  return 0;
}

//Manish - TODO: On websocket close API calling send a close frame to server not just close the connection
int nsi_websocket_close(VUser *vptr)
{
  connection *cptr;
  int close_idx = vptr->ws_close_id;

  NSDL2_WS(vptr, NULL, "Method Called, close_idx = %d, conn_id = %d", close_idx, g_ws_close_shr[close_idx].conn_id);

  u_ns_ts_t now = get_ms_stamp();
  cptr = vptr->ws_cptr[g_ws_close_shr[close_idx].conn_id];
   
  if(cptr == NULL)
  {
    NSTL2(NULL, NULL, "Error: cptr is NULL"); 
    NSDL2_WS(NULL, NULL, "Error: cptr is NULL");
    //vptr->ws_status = NS_REQUEST_ERRMISC;
    //return -1;
    goto err;  
  }
  
  if(cptr->conn_fd < 0)
  {
    NSDL2_WS(vptr, cptr, "WS: cptr->conn_fd is -1");
    NSTL2(vptr, cptr, "WS: cptr->conn_fd is -1");
    //return -1; 
    goto err;
  }

  websocket_close_connection(cptr, 1, now, cptr->req_ok, NS_COMPLETION_CLOSE);
  vptr->ws_status = NS_REQUEST_OK; 
  NSDL2_WS(vptr, cptr, "WS nsi_web_websocket_close(): is successfully closed..");
  return 0;

err:

  vptr->ws_status = NS_REQUEST_ERRMISC;
  ws_avgtime->ws_error_codes[NS_REQUEST_ERRMISC]++;

  NSDL2_WS(NULL, NULL, "WS: Error Case: ws_avgtime->ws_error_codes[%d] = %llu", 
                 vptr->ws_status, ws_avgtime->ws_error_codes[NS_REQUEST_ERRMISC]);

  if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT) 
    switch_to_vuser_ctx(vptr, "WebSocketCloseError: nsi_websocket_close()");
  else if(vptr->sess_ptr->script_type == NS_SCRIPT_TYPE_JAVA)
    send_msg_to_njvm(vptr->mcctptr,NS_NJVM_API_WEB_WEBSOCKET_CLOSE_REP, -1);
  else if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_SEPARATE_THREAD)
    send_msg_nvm_to_vutd(vptr, NS_API_WEBSOCKET_CLOSE_REP, -1);
  
  return -1;
}

int handle_frame_read(connection *cptr, unsigned char *frame, size_t frame_len, char **payload_start)
{
  int read_idx = 0;
  int frame_read_done = 0;
  IW_UNUSED(unsigned int final);
  IW_UNUSED(unsigned char rsv);
  unsigned char opcode;
  unsigned int masked;
  //unsigned int all_zero_nonce = 1;
  //unsigned char frame_masking_key[4];
  size_t payload_len = 0;
  size_t partial_payload_len = 0;

  IW_UNUSED(VUser *vptr = cptr->vptr);

  NSDL2_WS(vptr, cptr, "handle_frame_read(): Method Called, frame_len = %zu, state = %d, frame = %s",
                        frame_len, cptr->ws_reading_state, frame);

  if(!frame || *frame == '\0')
  {
    NSTL1_OUT(NULL, NULL, "Error: WS Frame should be given\n");
    return -1;
  }

  if(frame_len <= 0)
  {
    NSTL1_OUT(NULL, NULL, "Error: WS Frame len should be a positive integer\n");
    return -1;
  }

  if((cptr->content_length != -1) && (cptr->ws_reading_state < FRAME_MASK_KEY1))
  {
    NSDL2_WS(vptr, cptr, "Frame is partially read: content lenght is not read completely., cptr->content_length = %d", cptr->content_length);
    payload_len = cptr->content_length;
  }

  for(read_idx = 0; read_idx < frame_len; read_idx++)
  {
    NSDL2_WS(vptr, cptr, "cptr->ws_reading_state = %d", cptr->ws_reading_state);
    switch(cptr->ws_reading_state)
    {
      case NEW_FRAME:
        NSDL2_WS(vptr, cptr, "NEW_FRAME");
        cptr->bytes = 0;
        IW_UNUSED(final = (frame[read_idx] >> 7) & 1);  /* read 1 bit from MSB */
        IW_UNUSED(rsv = frame[read_idx] & 0x70);        /* read 3 bits after 1 bit from MSB */
        opcode = frame[read_idx] & 0xf;      /* read 4 bits from LSB */
  
        cptr->ws_reading_state = FRAME_HDR_LEN;
        cptr->content_length = -1;
        *payload_start = NULL;
        NSDL2_WS(vptr, cptr, "final = %d, rsv = %d, opcode = %0x", final, rsv, opcode);
        break;

      case FRAME_HDR_LEN:
        NSDL2_WS(vptr, cptr, "FRAME_HDR_LEN");
        masked = frame[read_idx] & 0x80;    /* read 1 bit from*/

        NSDL2_WS(vptr, cptr, "Frame 2nd byte: maske bit = %d, payload lenght bits = %d", masked, (frame[read_idx] & 0x7f));
        switch (frame[read_idx] & 0x7f)
        {
          case 126:
            NSDL2_WS(vptr, cptr, "case 126");
            /* control frames are not allowed to have big lengths */
            if (opcode & 8) {
              NSTL1_OUT(NULL, NULL, "Control frame asking for extended length is illegal\n");
              NSDL2_WS(vptr, cptr, "Control frame asking for extended length is illegal");
              return -1;
            }
            cptr->ws_reading_state = FRAME_HDR_LEN16_2;
            break;

          case 127:
            NSDL2_WS(vptr, cptr, "case 127");
            /* control frames are not allowed to have big lengths */
            if (opcode & 8) {
              NSTL1_OUT(NULL, NULL, "Control frame asking for extended length is illegal\n");
              NSDL2_WS(vptr, cptr, "Control frame asking for extended length is illegal");
                      return -1;
            }
            cptr->ws_reading_state = FRAME_HDR_LEN64_8;
            break;

          default:
            NSDL2_WS(vptr, cptr, "case default");
            cptr->content_length = payload_len = frame[read_idx];
            NSDL2_WS(vptr, cptr, "payload_len = %zu", payload_len);
            if (masked) {
              cptr->ws_reading_state = FRAME_MASK_KEY1;
              NSDL2_WS(vptr, cptr, "comes in default case");
            }
            else {
              *payload_start = (char *)&frame[read_idx + 1];
              frame_read_done = 1;
              //cptr->ws_reading_state =  NEW_FRAME; 
              NSDL2_WS(vptr, cptr, "comes in this block where payload_start = %s and ptr = %p", *payload_start, payload_start);
            }
            break;
        }
        break;

      case FRAME_HDR_LEN16_2:
        NSDL2_WS(vptr, cptr, "Comes in FRAME_HDR_LEN16_2 case..");
        payload_len = frame[read_idx] << 8;
        cptr->content_length = payload_len;
        cptr->ws_reading_state = FRAME_HDR_LEN16_1;
        break;

      case FRAME_HDR_LEN16_1:
        NSDL2_WS(vptr, cptr, "Comes in FRAME_HDR_LEN16_1 case..");
        payload_len |= frame[read_idx];
        cptr->content_length = payload_len;
        *payload_start = (char *)&frame[read_idx + 1];
        frame_read_done = 1;
        NSDL2_WS(vptr, cptr, "FRAME_HDR_LEN16_1: payload_start = %s and ptr = %p, payload_len = %zu", *payload_start, payload_start, payload_len);
        break;
      case FRAME_HDR_LEN64_8:
        NSDL2_WS(vptr, cptr, "Comes in FRAME_HDR_LEN16_8 case..");
        payload_len = (size_t)frame[read_idx] << 56;
        cptr->content_length = payload_len;
        cptr->ws_reading_state = FRAME_HDR_LEN64_7;
        break;

      case FRAME_HDR_LEN64_7:
        payload_len |= (size_t)frame[read_idx] << 48;
        cptr->content_length = payload_len;
        cptr->ws_reading_state = FRAME_HDR_LEN64_6;
        break;

      case FRAME_HDR_LEN64_6:
        payload_len |= (size_t)frame[read_idx] << 40;
        cptr->content_length = payload_len;
        cptr->ws_reading_state = FRAME_HDR_LEN64_5;
        break;

      case FRAME_HDR_LEN64_5:
        payload_len |= (size_t)frame[read_idx] << 32;
        cptr->content_length = payload_len;
        cptr->ws_reading_state = FRAME_HDR_LEN64_4;
        break;

      case FRAME_HDR_LEN64_4:
        payload_len |= (size_t)frame[read_idx] << 24;
        cptr->content_length = payload_len;
        cptr->ws_reading_state = FRAME_HDR_LEN64_3;
        break;

      case FRAME_HDR_LEN64_3:
        payload_len |= (size_t)frame[read_idx] << 16;
        cptr->content_length = payload_len;
        cptr->ws_reading_state = FRAME_HDR_LEN64_2;
        break;

      case FRAME_HDR_LEN64_2:
        payload_len |= (size_t)frame[read_idx] << 8;
        cptr->content_length = payload_len;
        cptr->ws_reading_state = FRAME_HDR_LEN64_1;
        break;

     case FRAME_HDR_LEN64_1:
        if (masked) {
          cptr->ws_reading_state = FRAME_MASK_KEY1;
          NSDL2_WS(vptr, cptr, "FRAME_HDR_LEN64_1: cptr->ws_reading_state = %d", cptr->ws_reading_state);
        }
        else {
          *payload_start = (char *)&frame[read_idx + 1];
           frame_read_done = 1;
          NSDL2_WS(vptr, cptr, "FRAME_HDR_LEN16_8: payload_start = %s and ptr = %p", *payload_start, payload_start);
        }
        break;

      case FRAME_MASK_KEY1:
        //frame_masking_key[0] = frame[read_idx];
        //if(frame[read_idx])
          //all_zero_nonce = 0;
        cptr->ws_reading_state = FRAME_MASK_KEY2;
        break;

      case FRAME_MASK_KEY2:
        //frame_masking_key[1] = frame[read_idx];
        //if(frame[read_idx])
          //all_zero_nonce = 0;
        cptr->ws_reading_state = FRAME_MASK_KEY3;
        break;

      case FRAME_MASK_KEY3:
        //frame_masking_key[2] = frame[read_idx];
        //if(frame[read_idx])
          //all_zero_nonce = 0;
        cptr->ws_reading_state = FRAME_MASK_KEY4;
        break;
      case FRAME_MASK_KEY4:
        //frame_masking_key[3] = frame[read_idx];
        //if(frame[read_idx])
          //all_zero_nonce = 0;
        if(payload_len) {
          *payload_start = (char *)&frame[read_idx + 1];
          NSDL2_WS(vptr, cptr, "FRAME_MASK_KEY4: where payload_len = [%zu], payload data = [%s]", payload_len, *payload_start);
        }
        else {
          NSDL2_WS(vptr, cptr, "FRAME_MASK_KEY4: where payload len not found and set NEW_FRAME");
        }
        frame_read_done = 1;
        break;

      case FRAME_PAYLOAD:
        NSDL2_WS(vptr, cptr, "FRAME_PAYLOAD: Partial Payload handling, cptr->bytes = %d, read len = %d, content lenght = %d",
                              cptr->bytes, frame_len, cptr->content_length);
        *payload_start = (char *)&frame[read_idx];
        frame_read_done = 1;
        break;

      default:
        NSDL2_WS(vptr, cptr, "Frame Reading in default case");
    }//End switch

    /* if opcode is other than - 
           %x0 denotes a continuation frame, 
           %x1 denotes a text frame, 
           %x2 denotes a binary frame, 
           %x8 denotes a connection close 
       then just ignore the Frame */

    //only if reading state is new frame 
    if((cptr->ws_reading_state == NEW_FRAME) && opcode != 0 && opcode != 1 && opcode != 2 && opcode != 8)
    {
      NSDL2_WS(vptr, cptr, "Ignoring Frame as opcode is = %d", opcode);
      cptr->ws_reading_state =  NEW_FRAME;
      continue;
    }

    if(frame_read_done)
    {
      NSDL2_WS(vptr, cptr, "ws_reading_state = %d, read_idx = %d, cptr->content_length = %d, cptr->bytes = %d, frame_len = %d", 
                            cptr->ws_reading_state, read_idx, cptr->content_length, cptr->bytes, frame_len);
      if (opcode == 9) //ping from websocket server 
      {
        //ws_send_pong_frame to server
        ws_send_pong(cptr);
        break; 
      }
      if(cptr->ws_reading_state != FRAME_PAYLOAD)
      {
        int left_bytes = frame_len - read_idx;

        cptr->content_length = payload_len;

        //if(left_bytes >= cptr->content_length)
          //cptr->bytes += (read_idx + 1) + payload_len;
        //else
        if(left_bytes < cptr->content_length)
        {
          payload_len = frame_len - (read_idx + 1);
          cptr->bytes += payload_len; 
          //cptr->bytes += frame_len - (read_idx + 1);
        }
        else
          cptr->bytes += payload_len;
      }
      else
      {
        if(frame_len > (cptr->content_length - cptr->bytes))
        {
          payload_len = cptr->content_length - cptr->bytes;
          cptr->bytes = cptr->content_length;
        }
        else
        {
          cptr->bytes += frame_len;
          payload_len = frame_len;
        }
      }

      if((cptr->content_length > 0) && (cptr->bytes >= cptr->content_length))
      {
        cptr->ws_reading_state = NEW_FRAME;
        NSDL2_WS(vptr, cptr, "Frame reading done, completly processed!, cptr->bytes = %d, cptr->content_length = %d, ws_reading_state = %d",
                              cptr->bytes, cptr->content_length, cptr->ws_reading_state);
      }
      else
      {
        cptr->ws_reading_state = FRAME_PAYLOAD;
        NSDL2_WS(vptr, cptr, "Frame partialy read, Header processing done!, "
                             "cptr->bytes = %d, cptr->content_length = %d, ws_reading_state = %d",
                              cptr->bytes, cptr->content_length, cptr->ws_reading_state);
      }

      break;
    }
    else
    {
      partial_payload_len = frame_len;
    }
  }

  if(!frame_read_done)
    payload_len = partial_payload_len;
 
  NSDL2_WS(vptr, cptr, "Method is exiting, ws_reading_state = %d, frame_read_done = %d, "
                       "cptr->bytes = %d, payload_len = %zd, cptr->content_length = %d",
                        cptr->ws_reading_state, frame_read_done, cptr->bytes, payload_len, cptr->content_length);

  return payload_len;
}
