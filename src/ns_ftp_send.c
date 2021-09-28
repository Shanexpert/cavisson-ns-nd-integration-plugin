/*  */
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
#include "ns_ftp.h"
#include "ns_ftp_send.h"
//#include "ns_handle_read.h"
#include "ns_ftp_parse.h"
#include "nslb_cav_conf.h"
#include "ns_auto_fetch_embd.h"
#include "ns_pop3.h"
#include "ns_sock_listen.h"
#include "nslb_sock.h"
#include "ns_log_req_rep.h"
#include "ns_dns.h"
#include "ns_group_data.h"

/*Below two function come from HPD.
 * We can move these function in lib*/
/* Example format: 227 Entering Passive Mode (127,0,0,1,167,148). */
void fill_ip_port(char *data, char *ip, int port)
{
  char *ptr = data;
  int i = 0;
  int fport,sport;
  char p[20];

  NSDL3_FTP(NULL, NULL, "Method called, Ip = %s, port = %d", ip, port);
  strcat(data, ip);
  while(ptr[i] != '\0')
  {
    if(ptr[i] == '.')
      ptr[i] = ',';
    i++;
  }
  strcat(data, ",");
  fport = (port >> 8) & 0xff; //first part of port
  sport = (port & 0xff); //second part of port
  sprintf(p,"%d", fport);
  strcat(data, p);
  strcat(data, ",");
  sprintf(p,"%d", sport);
  strcat(data, p);
  NSDL3_FTP(NULL, NULL, "Method called, Data = %s", data);
}

/*This function is to get port
 *  *Because DATA connection should be made on same IP*/
  //cptr is contrl conn
void get_ip_port(connection* cptr, char *my_ip, int *my_port)
{
  char our_ip_tmp[1024];
  char *ptr;
  char *port;

  NSDL3_FTP(NULL, cptr, "Method called");

  strcpy(our_ip_tmp, nslb_get_src_addr(cptr->conn_fd));
  /* Our_ip_tmp =>  IPV4:192.168.1.70.31337 */
  port = ptr = rindex(our_ip_tmp, '.');
  port++;//Pointing to port
  *my_port = atoi(port); 

  *ptr = '\0';
  ptr = index(our_ip_tmp, ':');
  ptr++;// pointing to IP
  strcpy(my_ip, ptr);
  NSDL3_FTP(NULL, cptr, "my_ip  = %s, port = %d", my_ip, *my_port);
}

void check_and_fill(char *ip)
{
  NSDL3_FTP(NULL, NULL, "Method called, IP = %s", ip);

  if(!strcmp(ip, "0.0.0.0")) 
    strcpy(ip, g_cavinfo.NSAdminIP);
  else if (!strcmp(ip, "127.0.0.1"))
    strcpy(ip, g_cavinfo.NSAdminIP);

  NSDL3_FTP(NULL, NULL, "Method called, IP = %s", ip);
}

/*This function will be called only for
 * Active transfer type*/
void open_new_data_con_and_fill_ip_port(connection *cptr, u_ns_ts_t now, 
                        char *ip, int *port) {
  action_request_Shr *new_url_num;
  connection *new_cptr;
  
  MY_MALLOC(new_url_num, sizeof(action_request_Shr), "redirect_url_num", -1);
  memcpy(new_url_num, cptr->url_num, sizeof (action_request_Shr));
  /* required */
  new_url_num->request_type = FTP_DATA_REQUEST;

  /*In first call we will use IP only.
 * we will not use port*/ 
  get_ip_port(cptr, ip, port); 
  check_and_fill(ip);
  new_cptr = (connection *)get_new_data_connection(cptr, now, ip, 0, new_url_num);
  new_cptr->conn_state = CNST_LISTENING;
  start_listen_socket(new_cptr, now, cptr);
   /*Here we will use port also*/ 
  get_ip_port(new_cptr, ip, port); 
  NSDL3_FTP(NULL, cptr, "starting listen socket with new cptr = %p on port = %d", new_cptr, *port);
}

void ftp_timeout_handle( ClientData client_data, u_ns_ts_t now ) {

  connection* cptr;
  cptr = (connection *)client_data.p;
  VUser* vptr = cptr->vptr;
  
  NSDL2_FTP(vptr, cptr, "Method Called, vptr=%p cptr=%p conn state=%d, request_type = %d", 
            vptr, cptr, cptr->conn_state,
            cptr->url_num->request_type);

  if(vptr->vuser_state == NS_VUSER_PAUSED){
    vptr->flags |= NS_VPTR_FLAGS_TIMER_PENDING;
    return;
  }

  if (cptr->url_num->request_type == FTP_DATA_REQUEST) {
    Close_connection(cptr->conn_link , 0, now, NS_REQUEST_TIMEOUT, NS_COMPLETION_TIMEOUT);
  }
  
  Close_connection( cptr , 0, now, NS_REQUEST_TIMEOUT, NS_COMPLETION_TIMEOUT);

}

void add_ftp_timeout_timer(connection *cptr, u_ns_ts_t now) {
 
  int timeout_val;
  ClientData client_data;
  VUser *vptr;

  client_data.p = cptr; 
  vptr = cptr->vptr;

  NSDL2_FTP(NULL, cptr, "Method Called, cptr=%p conn state=%d", cptr, cptr->conn_state);
  
  timeout_val = runprof_table_shr_mem[vptr->group_num].gset.ftp_timeout; 

  NSDL3_FTP(NULL, cptr, "timeout_val = %d", timeout_val);

  cptr->timer_ptr->actual_timeout = timeout_val;
  dis_timer_add(AB_TIMEOUT_IDLE, cptr->timer_ptr, now, ftp_timeout_handle, client_data, 0 );
}


void debug_log_ftp_req(connection *cptr, char *buf, int bytes_to_log, int complete_data, int first_trace_write_flag)
{
  VUser* vptr = cptr->vptr;
  int request_type = cptr->url_num->request_type;

  if (!((runprof_table_shr_mem[vptr->group_num].gset.debug & DM_LOGIC4) && 
        (request_type == FTP_REQUEST &&
         (runprof_table_shr_mem[vptr->group_num].gset.module_mask & MM_FTP))))
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
   sprintf(log_file, "%s/logs/%s/ftp_session_%hd_%u_%u_%d_0_%d_%d_%d_0.dat",
          g_ns_wdir, req_rep_file_path, child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,
          vptr->group_num, GET_SESS_ID_BY_NAME(vptr),
          GET_PAGE_ID_BY_NAME(vptr));


    // Do not change the debug trace message as it is parsed by GUI
    if(first_trace_write_flag)
      NS_DT4(vptr, cptr, DM_L1, MM_FTP, "Request is in file '%s'", log_file);

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
//This method to send ftp request
static void send_ftp_req(connection *cptr, int ftp_size, u_ns_ts_t now, NSIOVector *ns_iovec)
{
  int bytes_sent;
  VUser *vptr = cptr->vptr;

  NSDL2_FTP(NULL, cptr, "cptr=%p conn state=%d, proto_state = %d, ftp_size = %d, now = %u", cptr, cptr->conn_state, cptr->proto_state, ftp_size, now);

  if ((bytes_sent = writev(cptr->conn_fd, ns_iovec->vector, NS_GET_IOVEC_CUR_IDX(*ns_iovec))) < 0)
  {
    perror( "writev");
    printf("fd=%d num_vectors=%d \n", cptr->conn_fd, NS_GET_IOVEC_CUR_IDX(*ns_iovec));
    NS_FREE_RESET_IOVEC(*ns_iovec);
    fprintf(stderr, "sending FTP request failed\n");

    perror("FTP send error");

    retry_connection(cptr, now, NS_REQUEST_WRITE_FAIL);
    return;
  }
  //printf ("byte send = %d\n", bytes_sent);
  if(set_socketopt_for_quickack(cptr->conn_fd, 1) < 0)
  {
    retry_connection(cptr, now, NS_REQUEST_CONFAIL);
    return;
  }
  if (bytes_sent < ftp_size )
  {
    handle_incomplete_write(cptr, ns_iovec, NS_GET_IOVEC_CUR_IDX(*ns_iovec), ftp_size, bytes_sent);
    return;
  }
  //In case of partial these counters are incremented in handle_incomplete_write
  cptr->tcp_bytes_sent += bytes_sent;
  INC_FTP_DATA_TX_BYTES_COUNTER(vptr, bytes_sent);


#ifdef NS_DEBUG_ON
  // Complete data send, so log all vectors in req file
  int i;
  for(i = 0; i < NS_GET_IOVEC_CUR_IDX(*ns_iovec); i++)
  {
    if(i == NS_GET_IOVEC_CUR_IDX(*ns_iovec) - 1)
      debug_log_ftp_req(cptr, NS_GET_IOVEC_VAL(*ns_iovec, i), NS_GET_IOVEC_LEN(*ns_iovec, i), 0, 1);
    else
      debug_log_ftp_req(cptr, NS_GET_IOVEC_VAL(*ns_iovec, i), NS_GET_IOVEC_LEN(*ns_iovec, i), 0, 0);
  }
#endif
  if (cptr->conn_state == CNST_REUSE_CON)
    cptr->req_code_filled = -2;

  NS_FREE_RESET_IOVEC(*ns_iovec);

  /* cptr->conn_state for FTP will be set to reading in on_request_write_done() */
  if (cptr->url_num->proto.http.exact)
    SetSockMinBytes(cptr->conn_fd, cptr->url_num->proto.http.bytes_to_recv);
  on_request_write_done (cptr);
}

void ftp_send_user(connection *cptr, u_ns_ts_t now)
{
  int i, ret;
  int ftp_size = 0;

  NSDL2_FTP(NULL, cptr, "cptr=%p conn state=%d, now = %u", cptr, cptr->conn_state, now);
  NS_RESET_IOVEC(g_scratch_io_vector);

  /* make request */
  NS_FILL_IOVEC(g_scratch_io_vector, FTP_CMD_USER, strlen(FTP_CMD_USER));

  /* put user name */
  
  ret = insert_segments(cptr->vptr, cptr, &(cptr->url_num->proto.ftp.user_id), &g_scratch_io_vector, NULL, 0, 1, 1, cptr->url_num, SEG_IS_NOT_REPEAT_BLOCK);
  NSDL2_FTP(NULL,cptr, "user id is [%s]", cptr, cptr->url_num->proto.ftp.user_id);

  if(ret == MR_USE_ONCE_ABORT)
    return;

  /* append CRLF */
  NS_FILL_IOVEC(g_scratch_io_vector, FTP_CMD_CRLF, strlen(FTP_CMD_CRLF));

  for (i = 0; i < NS_GET_IOVEC_CUR_IDX(g_scratch_io_vector); i++) {
    ftp_size += NS_GET_IOVEC_LEN(g_scratch_io_vector, i);
  }

  cptr->proto_state = ST_FTP_USER; /* Next reading state */

  send_ftp_req(cptr, ftp_size, now, &g_scratch_io_vector);
}

void ftp_send_pass(connection *cptr, u_ns_ts_t now)
{
  int i, ret;
  int ftp_size = 0;

  NSDL2_FTP(NULL, cptr, "cptr=%p conn state=%d, now = %u", cptr, cptr->conn_state, now);

  NS_RESET_IOVEC(g_scratch_io_vector);

  /* make request */
  NS_FILL_IOVEC(g_scratch_io_vector, FTP_CMD_PASS, strlen(FTP_CMD_PASS));

  ret = insert_segments(cptr->vptr, cptr, &(cptr->url_num->proto.ftp.passwd), &g_scratch_io_vector, NULL, 0, 1, 1, cptr->url_num, SEG_IS_NOT_REPEAT_BLOCK);

  if(ret == MR_USE_ONCE_ABORT)
    return;

  /* append CRLF */
  NS_FILL_IOVEC(g_scratch_io_vector, FTP_CMD_CRLF, strlen(FTP_CMD_CRLF));

  for(i = 0; i < NS_GET_IOVEC_CUR_IDX(g_scratch_io_vector); i++) {
    ftp_size += NS_GET_IOVEC_LEN(g_scratch_io_vector, i);
  }

  cptr->proto_state = ST_FTP_PASS; /* Next reading state */

  send_ftp_req(cptr, ftp_size, now, &g_scratch_io_vector);
}

void ftp_send_cmd(connection *cptr, u_ns_ts_t now)
{ 
  int i, ret;
  int ftp_size = 0;
  
  NSDL2_FTP(NULL, cptr, "cptr=%p conn state=%d, now = %u", cptr, cptr->conn_state, now);
  NS_RESET_IOVEC(g_scratch_io_vector);

  ret = insert_segments(cptr->vptr, cptr, &(cptr->url_num->proto.ftp.ftp_cmd), &g_scratch_io_vector, NULL, 0, 1, 1, cptr->url_num, SEG_IS_NOT_REPEAT_BLOCK);

  if(ret == MR_USE_ONCE_ABORT)
    return;

  /* make request */
  NS_FILL_IOVEC(g_scratch_io_vector, FTP_CMD_CRLF, strlen(FTP_CMD_CRLF));
        
  for(i = 0; i < NS_GET_IOVEC_CUR_IDX(g_scratch_io_vector); i++) {
    ftp_size += NS_GET_IOVEC_LEN(g_scratch_io_vector, i);
  }

  cptr->proto_state = ST_FTP_CMD; /* Next reading state */

  send_ftp_req(cptr, ftp_size, now, &g_scratch_io_vector);
}

void ftp_send_port(connection *cptr, u_ns_ts_t now)
{
  int i;
  int ftp_size = 0;
  char ip_port_data[2048] = "\0";
  int ip_port_data_len;
  char ip[1024];
  int port;
  char *ptr = NULL;

  NSDL2_FTP(NULL, cptr, "cptr=%p conn state=%d, now = %u", cptr, cptr->conn_state, now);

  NS_RESET_IOVEC(g_scratch_io_vector);

  /* make request */
  NS_FILL_IOVEC(g_scratch_io_vector, FTP_CMD_PORT, strlen(FTP_CMD_PORT));

  /*This function will create listing port*/
  open_new_data_con_and_fill_ip_port(cptr, now, ip, &port);
  fill_ip_port(ip_port_data, ip, port);
  strcat(ip_port_data, "\r\n");
  ip_port_data_len = strlen(ip_port_data);
  MY_MALLOC(ptr, ip_port_data_len + 1, "IP Port", 1);
  strcpy(ptr, ip_port_data); 

  NS_FILL_IOVEC_AND_MARK_FREE(g_scratch_io_vector, ptr, ip_port_data_len);

  for(i = 0; i < NS_GET_IOVEC_CUR_IDX(g_scratch_io_vector); i++) {
    ftp_size += NS_GET_IOVEC_LEN(g_scratch_io_vector, i);
  }

  NS_DT2(NULL, cptr, DM_L1, MM_FTP, "Sending 'PORT' to Server.");

  cptr->proto_state = ST_FTP_PORT; /* Next reading state */

  //RESET_URL_RESP_AND_CPTR_VPTR_BYTES;

  send_ftp_req(cptr, ftp_size, now, &g_scratch_io_vector);
}

void ftp_send_type(connection *cptr, u_ns_ts_t now)
{   
  int i;
  int ftp_size = 0;

  NSDL2_FTP(NULL, cptr, "cptr=%p conn state=%d, now = %u", cptr, cptr->conn_state, now);

  NS_RESET_IOVEC(g_scratch_io_vector);

  /* make request */
  NS_FILL_IOVEC(g_scratch_io_vector, FTP_CMD_TYPE, strlen(FTP_CMD_TYPE));

  NS_FILL_IOVEC(g_scratch_io_vector, FTP_CMD_CRLF, strlen(FTP_CMD_CRLF));

  for(i = 0; i < NS_GET_IOVEC_CUR_IDX(g_scratch_io_vector); i++) {
    ftp_size += NS_GET_IOVEC_LEN(g_scratch_io_vector, i);
  }

  cptr->proto_state = ST_FTP_TYPE; /* Next reading state */

  send_ftp_req(cptr, ftp_size, now, &g_scratch_io_vector);
}
                  
void ftp_send_pasv(connection *cptr, u_ns_ts_t now)
{
  int i;
  int ftp_size = 0;
  NSDL2_FTP(NULL, cptr, "cptr=%p conn state=%d, now = %u", cptr, cptr->conn_state, now);
 
  NS_RESET_IOVEC(g_scratch_io_vector);
  /* make request */
  NS_FILL_IOVEC(g_scratch_io_vector, FTP_CMD_PASV, strlen(FTP_CMD_PASV));
  
  for(i = 0; i < NS_GET_IOVEC_CUR_IDX(g_scratch_io_vector); i++) {
    ftp_size += NS_GET_IOVEC_LEN(g_scratch_io_vector, i);
  }

  NS_DT2(NULL, cptr, DM_L1, MM_FTP, "Sending 'PASV' to Server.");

  cptr->proto_state = ST_FTP_PASV; /* Next reading state */

  send_ftp_req(cptr, ftp_size, now, &g_scratch_io_vector);
}

void ftp_send_retr(connection *cptr, u_ns_ts_t now)
{
  int i, ret;
  int ftp_size = 0;

  NSDL2_FTP(NULL, cptr, "cptr=%p conn state=%d, now = %u", cptr, cptr->conn_state, now);

  NS_RESET_IOVEC(g_scratch_io_vector);
  /* make request */
  NS_FILL_IOVEC(g_scratch_io_vector, FTP_CMD_RETR, strlen(FTP_CMD_RETR));

  ret = insert_segments(cptr->vptr, cptr, &(cptr->url_num->proto.ftp.get_files_idx[cptr->redirect_count]),
                             &g_scratch_io_vector, NULL, 0, 1, 1, cptr->url_num, SEG_IS_NOT_REPEAT_BLOCK);

  if(ret == MR_USE_ONCE_ABORT)
    return;

  NS_FILL_IOVEC(g_scratch_io_vector, FTP_CMD_CRLF, strlen(FTP_CMD_CRLF));

  for(i = 0; i < NS_GET_IOVEC_CUR_IDX(g_scratch_io_vector); i++) {
    ftp_size += NS_GET_IOVEC_LEN(g_scratch_io_vector, i);
  }

  cptr->proto_state = ST_FTP_RETR; /* Next reading state */

  send_ftp_req(cptr, ftp_size, now, &g_scratch_io_vector);
}

void ftp_send_stor(connection *cptr, u_ns_ts_t now)
{
  int i;
  int ftp_size = 0;
  char file_name[2048];
  char *ptr = NULL;

  NSDL2_FTP(NULL, cptr, "cptr=%p conn state=%d, now = %u", cptr, cptr->conn_state, now);

  NS_RESET_IOVEC(g_req_rep_io_vector);
  /* make request */
  NS_FILL_IOVEC(g_req_rep_io_vector, FTP_CMD_STOR, strlen(FTP_CMD_STOR));

  get_value_from_segments(cptr, file_name, &(cptr->url_num->proto.ftp.get_files_idx[cptr->redirect_count]), "PUT", NS_MAXCDNAME);
  if(file_name[0] != '\0')
  {
    ptr = rindex(file_name, '/');
    if (ptr != NULL)
    {
      ptr++;
      NS_FILL_IOVEC(g_req_rep_io_vector, ptr, strlen(ptr));
    }
    else   
    {
      NS_FILL_IOVEC(g_req_rep_io_vector, file_name, strlen(file_name));
    }
  } 
     
  NS_FILL_IOVEC(g_req_rep_io_vector, FTP_CMD_CRLF, strlen(FTP_CMD_CRLF));

  for(i = 0; i < NS_GET_IOVEC_CUR_IDX(g_req_rep_io_vector); i++) {
    ftp_size += NS_GET_IOVEC_LEN(g_req_rep_io_vector, i);
  }

  cptr->proto_state = ST_FTP_STOR; /* Next reading state */

  send_ftp_req(cptr, ftp_size, now, &g_req_rep_io_vector);
}

void ftp_send_quit(connection *cptr, u_ns_ts_t now)
{
  int i;
  int ftp_size = 0;

  NSDL2_FTP(NULL, cptr, "cptr=%p conn state=%d, now = %u", cptr, cptr->conn_state, now);

  NS_RESET_IOVEC(g_scratch_io_vector);
  /* make request */
  NS_FILL_IOVEC(g_scratch_io_vector, FTP_CMD_QUIT, strlen(FTP_CMD_QUIT));

  for(i = 0; i < NS_GET_IOVEC_CUR_IDX(g_scratch_io_vector); i++) {
    ftp_size += NS_GET_IOVEC_LEN(g_scratch_io_vector, i);
  }

  NS_DT2(NULL, cptr, DM_L1, MM_FTP, "Sending 'QUIT' to Server.");

  cptr->proto_state = ST_FTP_QUIT; /* Next reading state */

  /* We also need to close data connection if its open  */
  if(cptr->conn_link)
    Close_connection(cptr->conn_link, 0, now, NS_REQUEST_OK, NS_COMPLETION_CLOSE);

  send_ftp_req(cptr, ftp_size, now, &g_scratch_io_vector);
}
