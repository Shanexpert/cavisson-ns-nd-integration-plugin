/**
 * FILE: ns_ftp.c
 * PURPOSE: contains all FTP reading and state switching mechanism
 * AUTHOR: bhav
 */


#include <stdio.h>
#include <string.h>
#include <regex.h>
#include <ctype.h>
#include <sys/sendfile.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"

#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_msg_com_util.h"
#include "output.h"
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
#include "ns_test_gdf.h"

#include "netstorm.h"
#include "ns_log.h"
#include "ns_sock_list.h"
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
#include "ns_common.h"
#include "nslb_util.h"
#include "ns_ftp_send.h"
#include "ns_url_resp.h"
#include "ns_pop3.h"
#include "ns_auto_fetch_embd.h"
#include "ns_log.h"
#include "nslb_sock.h"
#include "ns_parallel_fetch.h"
#include "ns_http_process_resp.h"
#include "ns_js.h"
#include "ns_ftp.h"
#include "ns_connection_pool.h"
#include "ns_dns.h"
#include "ns_alloc.h"
#include "ns_log_req_rep.h"
#include "ns_group_data.h"
#include "ns_exit.h"

char g_ftp_st_str[][0xff] = {
  "FTP_INITIALIZATION",
  "FTP_CONNECTED",
  "FTP_USER",
  "FTP_PASS",
  "FTP_PASV",
  "FTP_RETR",
  "FTP_RETR_INTR",
  "FTP_RETR_INTR_RECV",
  "FTP_QUIT"
};

/* Common error code mapping */
static void set_ftp_error_code(connection *cptr, int err_code) {

  NSDL2_FTP(NULL, cptr, "Method called, err_code = %d", err_code);
  
  switch(err_code) {
  case 530:  // 530 Authentication failed 
    cptr->req_ok = NS_REQUEST_AUTH_FAIL;
    break;
  default:
    cptr->req_ok = NS_REQUEST_BAD_RESP;
    break;
  }
}

void debug_log_ftp_res(connection *cptr, char *buf, int size)
{
  VUser *vptr;
  vptr = (VUser *)cptr->vptr;
  int request_type = cptr->url_num->request_type;

  if (!((runprof_table_shr_mem[vptr->group_num].gset.debug & DM_LOGIC4) && 
        (request_type == FTP_REQUEST &&
         (runprof_table_shr_mem[vptr->group_num].gset.module_mask & MM_FTP))))
    return;

/*   if ((runprof_table_shr_mem[vptr->group_num].gset.debug & DM_LOGIC4)  &&  */
/*       (runprof_table_shr_mem[vptr->group_num].gset.module_mask & MM_HTTP)) */
  {
    char log_file[1024];
    int log_fd;

    // Log file name format is url_rep_<nvm_id>_<user_id>_<sess_inst>_<pg_inst>_<url_inst>_<sess_id>_<page_id>_<url_id>
    // url_id is not yet implemented (always 0)
    //sprintf(log_file, "%s/logs/TR%d/url_rep_%d_%ld_%ld_%d_0.dat", g_ns_wdir, testidx, my_port_index, vptr->user_index, cur_vptr->sess_inst, vptr->page_instance);
  

  SAVE_REQ_REP_FILES
   sprintf(log_file, "%s/logs/%s/ftp_session_%hd_%u_%u_%d_0_%d_%d_%d_0.dat",
          g_ns_wdir, req_rep_file_path, child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,
          vptr->group_num, GET_SESS_ID_BY_NAME(vptr),
          GET_PAGE_ID_BY_NAME(vptr));

    
    // Do not change the debug trace message as it is parsed by GUI
    if(cptr->tcp_bytes_recv == 0)  //This check to print debug trace only once
      NS_DT4(vptr, cptr, DM_L1, MM_FTP, "Response is in file '%s'", log_file);
    
    //Since response can come partialy so this will print debug trace many time
    //cptr->tcp_bytes_recv = 0, means this response comes first time

    if((log_fd = open(log_file, O_CREAT|O_CLOEXEC|O_WRONLY|O_APPEND, 00666)) < 0)
        fprintf(stderr, "Error: Error in opening file for logging URL request\n"
);
    else
    {
      write(log_fd, buf, size);
      close(log_fd);
    }
  }
}

void debug_log_ftp_data_res(connection *cptr, char *buf, int size)
{
  VUser *vptr;
  vptr = (VUser *)cptr->vptr;
  int request_type = cptr->url_num->request_type;

  NSDL2_FTP(NULL, cptr, "Method called, request_type = [%d]", request_type);
  
  if (!((runprof_table_shr_mem[vptr->group_num].gset.debug & DM_LOGIC4) && 
        (request_type == FTP_DATA_REQUEST &&
         (runprof_table_shr_mem[vptr->group_num].gset.module_mask & MM_FTP))))
    return;

/*   if ((runprof_table_shr_mem[vptr->group_num].gset.debug & DM_LOGIC4)  &&  */
/*       (runprof_table_shr_mem[vptr->group_num].gset.module_mask & MM_HTTP)) */
  {
    char log_file[1024];
    int log_fd;

    // Log file name format is url_rep_<nvm_id>_<user_id>_<sess_inst>_<pg_inst>_<url_inst>_<sess_id>_<page_id>_<url_id>
    // url_id is not yet implemented (always 0)
    //sprintf(log_file, "%s/logs/TR%d/url_rep_%d_%ld_%ld_%d_0.dat", g_ns_wdir, testidx, my_port_index, vptr->user_index, cur_vptr->sess_inst, vptr->page_instance);
  
   SAVE_REQ_REP_FILES
   sprintf(log_file, "%s/logs/%s/ftp_data_session_%hd_%u_%u_%d_0_%d_%d_%d_0.dat",
          g_ns_wdir, req_rep_file_path, child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,
          vptr->group_num, GET_SESS_ID_BY_NAME(vptr),
          GET_PAGE_ID_BY_NAME(vptr));
    
    // Do not change the debug trace message as it is parsed by GUI
    if(cptr->tcp_bytes_recv == 0)  //This check to print debug trace only once
      NS_DT4(vptr, cptr, DM_L1, MM_FTP, "Response is in file '%s'", log_file);
    
    //Since response can come partialy so this will print debug trace many time
    //cptr->tcp_bytes_recv = 0, means this response comes first time

    if((log_fd = open(log_file, O_CREAT|O_CLOEXEC|O_WRONLY|O_APPEND, 00666)) < 0)
        fprintf(stderr, "Error: Error in opening file for logging URL request\n"
);
    else
    {
      write(log_fd, buf, size);
      close(log_fd);
    }
  }
}


void delete_ftp_timeout_timer(connection *cptr) {

  NSDL2_FTP(NULL, cptr, "Method called, timer type = %d", cptr->timer_ptr->timer_type);

  if ( cptr->timer_ptr->timer_type == AB_TIMEOUT_IDLE ) {
    NSDL2_FTP(NULL, cptr, "Deleting Idle timer.");
    dis_timer_del(cptr->timer_ptr);
  }
}

/* Function calls retry_connection() which will ensure normal http like retries for ftp also. */
static inline void
handle_ftp_bad_read (connection *cptr, u_ns_ts_t now)
{
  NSDL2_FTP(NULL, cptr, "cptr=%p conn state=%d, now = %u", cptr, cptr->conn_state, now);
 
  if(cptr->conn_link)		/* data con */
    Close_connection(cptr->conn_link, 0, now, NS_REQUEST_OK, NS_COMPLETION_CLOSE); 
  
  retry_connection(cptr, now, NS_REQUEST_BAD_HDR);
}

static inline void
handle_ftp_data_write_complete (connection *cptr, u_ns_ts_t now)
{
  NSDL2_FTP(NULL, cptr, "cptr=%p conn state=%d, now = %u", cptr, cptr->conn_state, now);
  connection *ctrl_cptr;
  ctrl_cptr = ((connection *)(cptr->conn_link));

  Close_connection(cptr, 0, now, NS_REQUEST_OK, NS_COMPLETION_CLOSE);

  if(!ctrl_cptr)
    return;

  /* Set timer on conn_link if its in INTRIM retr state so we can wait for response 2xx */
  if (ctrl_cptr->proto_state == ST_FTP_RETR_INTRM) {
    //printf("Control con is still in intrm state so adding timer\n");
    delete_ftp_timeout_timer(ctrl_cptr);
    add_ftp_timeout_timer(ctrl_cptr, now);
  } else if (ctrl_cptr->proto_state == ST_FTP_RETR_INTRM_RECEIVED) {
    delete_ftp_timeout_timer(ctrl_cptr);
    if (ctrl_cptr->redirect_count < ctrl_cptr->url_num->proto.ftp.num_get_files)
      ftp_send_pasv(ctrl_cptr, now);
    else
      ftp_send_quit(ctrl_cptr, now);
  }
}

static inline void
handle_ftp_data_read_complete (connection *cptr, u_ns_ts_t now)
{
  NSDL2_FTP(NULL, cptr, "cptr=%p conn state=%d, now = %u", cptr, cptr->conn_state, now);
  connection *ctrl_cptr;

  ctrl_cptr = ((connection *)(cptr->conn_link));

  Close_connection(cptr, 0, now, NS_REQUEST_OK, NS_COMPLETION_CLOSE);

  /* Set timer on conn_link if its in INTRIM retr state so we can wait for response 2xx */
  if (ctrl_cptr->proto_state == ST_FTP_RETR_INTRM) {
    //printf("Control con is still in intrm state so adding timer\n");
    delete_ftp_timeout_timer(ctrl_cptr);
    add_ftp_timeout_timer(ctrl_cptr, now);
  } else if (ctrl_cptr->proto_state == ST_FTP_RETR_INTRM_RECEIVED) {
    delete_ftp_timeout_timer(ctrl_cptr);
    if (ctrl_cptr->redirect_count < ctrl_cptr->url_num->proto.ftp.num_get_files)
      ftp_send_pasv(ctrl_cptr, now);
    else
      ftp_send_quit(ctrl_cptr, now);
  }
}

char *ftp_state_to_str(int state) 
{
  /* TODO bounds checking */
  return g_ftp_st_str[state];
}

void ftp_process_handshake(connection *cptr, u_ns_ts_t now, char *buf, int bytes_read)
{
  int req_code = cptr->req_code;

  NSDL2_FTP(NULL, cptr, "conn state=%d, req_code = %d, buf = %s, bytes_read = %d, now = %u",
                          cptr->conn_state, req_code, buf, bytes_read, now);
  
  if (req_code == 220) {
    ftp_send_user(cptr, now);
  } else {
    Close_connection(cptr, 0, now, NS_REQUEST_BAD_RESP, NS_COMPLETION_FTP_ERROR);
  }
}

void ftp_process_user(connection *cptr, u_ns_ts_t now, char *buf, int bytes_read)
{
  int req_code = cptr->req_code;
  
  int first_digit  = req_code / 100;
  //int second_digit = (req_code / 10) % 10;

  NSDL2_FTP(NULL, cptr, "conn state=%d, req_code = %d, buf = %s, bytes_read = %d, now = %u",
                          cptr->conn_state, req_code, buf, bytes_read, now);

  switch (first_digit) {
  case 3:   /* user name accepted we send the passwd */
    ftp_send_pass(cptr, now);
    break;

  default:
    set_ftp_error_code(cptr, req_code);
    ftp_send_quit(cptr, now);
    break;
  }
}

void ftp_process_pass(connection *cptr, u_ns_ts_t now, char *buf, int bytes_read)
{
  int req_code = cptr->req_code;

  int first_digit  = req_code / 100;
  int second_digit = (req_code / 10) % 10;
  //int third_digit  = (req_code % 10);

  NSDL2_FTP(NULL, cptr, "conn state=%d, req_code = %d, buf = %s, bytes_read = %d, now = %u",
                          cptr->conn_state, req_code, buf, bytes_read, now);

  switch (first_digit) {
  case 2:   /* passwd  accepted  we send mail */
    if (1)  /* we assume that we have at least one file to fetch. */
    {
      if(cptr->url_num->proto.ftp.ftp_cmd.seg_start != NULL)
        ftp_send_cmd(cptr,now);
      else if(cptr->url_num->proto.ftp.passive_or_active == FTP_TRANSFER_TYPE_ACTIVE)
        ftp_send_port(cptr, now);
      else
        ftp_send_pasv(cptr, now);
    }
    else    /* QUIT */
      ftp_send_quit(cptr, now);
    break;

    /* all other cases are errors */
  case 5:     /* This basically means auth failed, we should try to send just MAIL */
    switch(second_digit) {
/*    case 0:   // auth failed
      //  not supported we will now try sending MAIL
      ftp_send_mail(cptr, now);
      break;
*/
    default:
      set_ftp_error_code(cptr, req_code);
      ftp_send_quit(cptr, now);
      break;
    }
    break;

    /* All other cases we quit */
 default: /* Error */
    set_ftp_error_code(cptr, req_code);
    ftp_send_quit(cptr, now);
    break;
  }
}

void ftp_process_cmd(connection *cptr, u_ns_ts_t now, char *buf, int bytes_read)
{
  int req_code = cptr->req_code;

  int first_digit  = req_code / 100;
  int second_digit = (req_code / 10) % 10;
  //int third_digit  = (req_code % 10);

  NSDL2_FTP(NULL, cptr, "conn state=%d, req_code = %d, buf = %s, bytes_read = %d, now = %u",
                          cptr->conn_state, req_code, buf, bytes_read, now);

  switch (first_digit) {
  case 2:   /* passwd  accepted  we send mail */
    if (1)  /* we assume that we have at least one file to fetch. */
    {
       if(cptr->url_num->proto.ftp.passive_or_active == FTP_TRANSFER_TYPE_ACTIVE)
        ftp_send_port(cptr, now);
       else
        ftp_send_pasv(cptr, now);
    }
    else    /* QUIT */
      ftp_send_quit(cptr, now);
    break;

    /* all other cases are errors */
  case 5:     /* This basically means auth failed, we should try to send just MAIL */
    switch(second_digit) {
/*    case 0:   // auth failed
      //  not supported we will now try sending MAIL
      ftp_send_mail(cptr, now);
      break;
*/
    default:
      set_ftp_error_code(cptr, req_code);
       ftp_send_quit(cptr, now);
      break;
    }
    break;

    /* All other cases we quit */
 default: /* Error */
    set_ftp_error_code(cptr, req_code);
    ftp_send_quit(cptr, now);
    break;
  }
}

 
void ftp_extract_pasv_ip_port(char *full_buffer, int fb_len, char *ip, int *port) 
{
#define ST_FTP_PASV_BEGIN       0
#define ST_FTP_PASV_IP_A        1
#define ST_FTP_PASV_IP_B        2
#define ST_FTP_PASV_IP_C        3
#define ST_FTP_PASV_IP_D        4
#define ST_FTP_PASV_PORT_A      5
#define ST_FTP_PASV_PORT_B      6
#define ST_FTP_PASV_END         7


  char *ptr;
  int state = ST_FTP_PASV_BEGIN;
  //char ip[0xff];   /* Enought for IPV6 */
  char *ip_ptr = ip;
  char port_a[4];
  char port_b[4];
  char *port_ptr;

  NSDL2_FTP(NULL, NULL, "fb_len = %d, full_buffer = %s", fb_len, full_buffer);
  /* Example string: 
   * 227 Entering Passive Mode (127,0,0,1,51,115). 
   * 227 Entering Passive Mode (127,0,0,1,51,115)
   * 227 Entering Passive Mode 127,0,0,1,51,115
   */
  
  ptr = full_buffer + 3;

  while (ptr[0]) {

    if (state == ST_FTP_PASV_END)
      break;

    switch(state) {

    case ST_FTP_PASV_BEGIN:
      if (isdigit(ptr[0])) {
        state = ST_FTP_PASV_IP_A;
        ip_ptr = ip;
        ip_ptr[0] = ptr[0];
        ip_ptr++;
      }
      break;

    case ST_FTP_PASV_IP_A:
      if (ptr[0] == ',') {
        ip_ptr[0] = '.';
        state = ST_FTP_PASV_IP_B;
      } else
        ip_ptr[0] = ptr[0];
      ip_ptr++;
      break;

    case ST_FTP_PASV_IP_B:
      if (ptr[0] == ',') {
        ip_ptr[0] = '.';
        state = ST_FTP_PASV_IP_C;
      } else
        ip_ptr[0] = ptr[0];
      ip_ptr++;
      break;

    case ST_FTP_PASV_IP_C:
      if (ptr[0] == ',') {
        ip_ptr[0] = '.';
        state = ST_FTP_PASV_IP_D;
      } else
        ip_ptr[0] = ptr[0];
      ip_ptr++;
      break;

    case ST_FTP_PASV_IP_D:
      if (ptr[0] == ',') {
        ip_ptr[0] = '\0';
        state = ST_FTP_PASV_PORT_A;
        port_ptr = port_a;
      } else
        ip_ptr[0] = ptr[0];
      ip_ptr++;
      break;

    case ST_FTP_PASV_PORT_A:
      if (ptr[0] == ',') {
        port_ptr[0] = '\0';
        state = ST_FTP_PASV_PORT_B;
        port_ptr = port_b;
      } else {
        port_ptr[0] = ptr[0];
        port_ptr++;
      }
      break;

    case ST_FTP_PASV_PORT_B:
      if (isdigit(ptr[0])) {
        port_ptr[0] = ptr[0];
        port_ptr++;
        port_ptr[0] = '\0';
      } else {
        state = ST_FTP_PASV_END;
      }
      break;
    }
    //printf("XXXXXXXXX state = %d [%c]\n", state, ptr[0]);
    ptr++;
  }

  *port = (atoi(port_a) << 8) + atoi(port_b);

  NSDL3_FTP(NULL, NULL, "ip = %s, port = %d (%s, %s)", ip, *port, port_a, port_b);
}

/*This function is to get new data connection
 * This function will return connection. This 
 * connection does not have any FD. We need to fill 
 * this connection with FD and other data*/
connection *get_new_data_connection(connection *cptr, u_ns_ts_t now, char *hostname, 
                               int port, action_request_Shr *new_url_num)
{
  connection *new_cptr = NULL;
  VUser *vptr = (VUser *)cptr->vptr;
  int cur_host;
  HostSvrEntry *hptr;

  NSDL1_FTP(NULL, NULL, "Method called hostname = %s", hostname);

  new_cptr = get_free_connection_slot(vptr);
  if (new_cptr == cptr)
    assert(0);
  if (new_cptr == NULL) {
    //Create a free slot by closing an idling RESUE_CON connection, if available
    //new_cptr = remove_head_glb_reuse_list(vptr);
    new_cptr = vptr->head_creuse;
    
    if (new_cptr) {
      while (new_cptr == cptr) {
        new_cptr = (connection *)new_cptr->next_reuse;
      }
    }

    if (new_cptr == NULL) {
      NSDL1_FTP(vptr, NULL, "There is no element to remove from head of glb reuse list");
      NS_EXIT(-1, "There is no element to remove from head of glb reuse list");
      //return -1;
    }
    /*Connection pool design used to remove connection from reuse list*/ 
    remove_from_all_reuse_list(new_cptr);
    if (new_cptr == NULL) {
      fprintf(stderr, "try_hurl_on_any_con:free_connection not available\n");
      end_test_run();
    }
    //Reuse connection for unrealted host available: Kill the connection
    //So that we can use this slot for current host
    close_fd_and_release_cptr (new_cptr, NS_FD_CLOSE_REMOVE_RESP, now);
 
    //Start new connection
    new_cptr = get_free_connection_slot(vptr);
    if (new_cptr == NULL) {
      fprintf(stderr, "try_hurl_on_any_con:free_connection not available\n");
      end_test_run();
    }
  }

  /* this will be same as ftp control con */
  cur_host = get_svr_ptr(cptr->url_num, vptr)->idx;
  hptr = vptr->hptr + cur_host;
  
  hptr->num_parallel++;
  vptr->cnum_parallel++;

  SET_URL_NUM_IN_CPTR(new_cptr, new_url_num);

  new_cptr->num_retries = 0;

  /* Data server info will be used for connect */
  if (!nslb_fill_sockaddr(&(new_cptr->cur_server), hostname, port)) {
    fprintf(stderr, "Error: Host <%s> specified by Host header is not a valid hostname. Exiting \n",
            hostname);
    end_test_run();
  }

  new_cptr->conn_link = cptr;
  cptr->conn_link = new_cptr;
  return new_cptr;
}

/* Returns -1 on fail */
int ftp_open_data_con(connection *cptr, char *full_buffer, int fb_len, u_ns_ts_t now)
{
  char hostname[MAX_LINE_LENGTH];
  int port;
  action_request_Shr *new_url_num;
  connection *new_cptr;
  //VUser *vptr = (VUser *)cptr->vptr;

  ftp_extract_pasv_ip_port(full_buffer, fb_len, hostname, &port);

  MY_MALLOC(new_url_num, sizeof(action_request_Shr), "redirect_url_num", -1);
  memcpy(new_url_num, cptr->url_num, sizeof (action_request_Shr));
  /* required */
  new_url_num->request_type = FTP_DATA_REQUEST;
  
  new_cptr = get_new_data_connection(cptr, now, hostname, port, new_url_num);

  NSDL3_FTP(NULL, cptr, "starting socket with new cptr");
  // Here we should not call start_new_socket() as on_url_start should not be called for data connection
  start_socket(new_cptr, now);
  return 0;
}

void ftp_process_pasv(connection *cptr, u_ns_ts_t now, char *buf, int bytes_read)
{
  int req_code = cptr->req_code;
  
  int first_digit  = req_code / 100;
  //int second_digit = (req_code / 10) % 10;
  int blen = cptr->bytes;
  VUser *vptr = (VUser *)cptr->vptr;

  //int third_digit  = (req_code % 10);
  
  NSDL2_FTP(NULL, cptr, "conn state=%d, req_code = %d, buf = %s, bytes_read = %d, now = %u",
                          cptr->conn_state, req_code, buf, bytes_read, now);

  copy_url_resp(cptr);
  copy_cptr_to_vptr(vptr, cptr); // Save buffers from cptr to vptr etc 
  
  full_buffer = get_reply_buffer(cptr, &blen, 0, 1); 

  switch (first_digit) {
  case 2:   /* passwd  accepted  we send mail */
    if (ftp_open_data_con(cptr, full_buffer, blen, now) == -1) {
      /* we were not able to open data con */
      cptr->req_ok = NS_REQUEST_ERRMISC;
      ftp_send_quit(cptr, now);
    } else {
      RESET_URL_RESP_AND_CPTR_VPTR_BYTES;
      if(cptr->url_num->proto.ftp.file_type == FTP_TRANSFER_TYPE_BINARY)
        ftp_send_type(cptr,now);
      else if(cptr->url_num->proto.ftp.ftp_action_type == FTP_ACTION_RETR) 
        ftp_send_retr(cptr, now);
      else if(cptr->url_num->proto.ftp.ftp_action_type == FTP_ACTION_STOR) 
        ftp_send_stor(cptr, now);
    }
    break;

    /* all other cases are errors */
  case 5:     /* This basically means auth failed, we should try to send just MAIL */
    set_ftp_error_code(cptr, req_code);
    ftp_send_quit(cptr, now);
    break;

  //  case 4: /* Retry */

    /* All other cases we quit */
  default: /* Error */
    set_ftp_error_code(cptr, req_code);
    ftp_send_quit(cptr, now);
    break;
  }
}

void ftp_process_type(connection *cptr, u_ns_ts_t now, char *buf, int bytes_read)
{
  int req_code = cptr->req_code;
  
  int first_digit  = req_code / 100;

  NSDL2_FTP(NULL, cptr, "conn state=%d, req_code = %d, buf = %s, bytes_read = %d, now = %u",
                          cptr->conn_state, req_code, buf, bytes_read, now);

  switch (first_digit) {
  case 2:
      if(cptr->url_num->proto.ftp.ftp_action_type == FTP_ACTION_RETR)
        ftp_send_retr(cptr, now);
      else if(cptr->url_num->proto.ftp.ftp_action_type == FTP_ACTION_STOR)
        ftp_send_stor(cptr, now);
 
    break;

    /* all other cases are errors */
  case 5:     /* This basically means auth failed, we should try to send just MAIL */
    set_ftp_error_code(cptr, req_code);
    ftp_send_quit(cptr, now);
    break;

    /* All other cases we quit */
  default: /* Error */
    set_ftp_error_code(cptr, req_code);
    ftp_send_quit(cptr, now);
    break;
  }
}

void ftp_process_port(connection *cptr, u_ns_ts_t now, char *buf, int bytes_read)
{
  int req_code = cptr->req_code;
  
  int first_digit  = req_code / 100;

  NSDL2_FTP(NULL, cptr, "conn state=%d, req_code = %d, buf = %s, bytes_read = %d, now = %u",
                          cptr->conn_state, req_code, buf, bytes_read, now);

  switch (first_digit) {
  case 2: 
     if(cptr->url_num->proto.ftp.file_type == FTP_TRANSFER_TYPE_BINARY)
        ftp_send_type(cptr,now);
     else if(cptr->url_num->proto.ftp.ftp_action_type == FTP_ACTION_RETR) 
       ftp_send_retr(cptr, now);
     else if(cptr->url_num->proto.ftp.ftp_action_type == FTP_ACTION_STOR) 
       ftp_send_stor(cptr, now);
    break;

    /* all other cases are errors */
  case 5:     /* This basically means auth failed, we should try to send just MAIL */
    set_ftp_error_code(cptr, req_code);
    ftp_send_quit(cptr, now);
    break;

    /* All other cases we quit */
  default: /* Error */
    set_ftp_error_code(cptr, req_code);
    ftp_send_quit(cptr, now);
    break;
  }
}

void ftp_process_retr_stor(connection *cptr, u_ns_ts_t now, char *buf, int bytes_read)
{
  int req_code = cptr->req_code;
  
  int first_digit  = req_code / 100;
  int second_digit = (req_code / 10) % 10;
  //int third_digit  = (req_code % 10);
  
  NSDL2_FTP(NULL, cptr, "conn state=%d, req_code = %d, buf = %s, bytes_read = %d, now = %u",
                          cptr->conn_state, req_code, buf, bytes_read, now);

  switch (first_digit) {
  case 1: 
    switch(second_digit) {
    case 5:   /* fall through for now */
    default:
      /* Just set the state and send nothing since the server will respond 
       * back once again after sending the complete file */
      if(cptr->url_num->proto.ftp.ftp_action_type == FTP_ACTION_RETR) 
        cptr->proto_state = ST_FTP_RETR_INTRM;
      else if(cptr->url_num->proto.ftp.ftp_action_type == FTP_ACTION_STOR){
        handle_ftp_data_write(cptr, now); 
        cptr->proto_state = ST_FTP_STOR_INTRM;
      }
      /* we dont set the timer here, the timer is set on data connection.
       * if timeout occurs on data connection it also closes the control
       * connection with T.O. Error.
       */
      //add_ftp_timeout_timer(cptr, now);

      /* we read since data might be available right now */
      //handle_ftp_read(cptr, now);
      break;
    }
    break;

    /* all other cases are errors */
  case 5:     /* This basically means auth failed, we should try to send just MAIL */
    switch(second_digit) {
/*    case 0:   // auth failed
      //  not supported we will now try sending MAIL
      ftp_send_mail(cptr, now);
      break;
*/
    default:
      set_ftp_error_code(cptr, req_code);
      ftp_send_quit(cptr, now);
      break;
    }
    break;

    /* All other cases we quit */
  default: /* Error */
    set_ftp_error_code(cptr, req_code);
    ftp_send_quit(cptr, now);
    break;
  }
}

void ftp_process_retr_stor_intrm(connection *cptr, u_ns_ts_t now, char *buf, int bytes_read)
{
  int req_code = cptr->req_code;
  
  int first_digit  = req_code / 100;
  int second_digit = (req_code / 10) % 10;
  //int third_digit  = (req_code % 10);
  
  NSDL2_FTP(NULL, cptr, "conn state=%d, req_code = %d, buf = %s, bytes_read = %d, now = %u",
            cptr->conn_state, req_code, buf, bytes_read, now);

  switch (first_digit) {
  case 2: 
    switch(second_digit) {
    case 2:
       if(cptr->url_num->proto.ftp.ftp_action_type == FTP_ACTION_RETR)
          cptr->redirect_count++;
      if (cptr->conn_link == NULL) {/* i.e. the data connection line is broken
                                     * (file completely read) */
        if (cptr->redirect_count < cptr->url_num->proto.ftp.num_get_files)
          ftp_send_pasv(cptr, now);
        else
          ftp_send_quit(cptr, now);
      } else {
         if(cptr->url_num->proto.ftp.ftp_action_type == FTP_ACTION_RETR) 
           cptr->proto_state = ST_FTP_RETR_INTRM_RECEIVED;
         else if(cptr->url_num->proto.ftp.ftp_action_type == FTP_ACTION_STOR) 
           cptr->proto_state = ST_FTP_STOR_INTRM_RECEIVED;
      }
    break;
    }
    break;
  /* all other cases are errors */
  case 5:  
    switch(second_digit) {
/*    case 0:   // auth failed
      //  not supported we will now try sending MAIL
      ftp_send_mail(cptr, now);
      break;
*/
    default:
      set_ftp_error_code(cptr, req_code);
      ftp_send_quit(cptr, now);
      break;
    }
    break;

    /* All other cases we quit */
  default: /* Error */
    set_ftp_error_code(cptr, req_code);
    ftp_send_quit(cptr, now);
    break;
  }
}

void ftp_process_quit(connection *cptr, u_ns_ts_t now, char *buf, int bytes_read)
{
  int status   = cptr->req_ok;
  
  NSDL2_FTP(NULL, cptr, "conn state=%d, req_code = %d, buf = %s, bytes_read = %d, now = %u",
                          cptr->conn_state, cptr->req_code, buf, bytes_read, now);
    
  // Confail means req_ok is not filled by any error earliar, so its a success case
  if(status == NS_REQUEST_CONFAIL)
    status = NS_REQUEST_OK;

  Close_connection(cptr, 0, now, status, NS_COMPLETION_CLOSE);
}



/**
 * function handle_ftp_read() is the main function handling ftp protocol
 * All state changes take place in this function. 
 * Most of the code is borrowed from http's handle_read()
 *
 */
int
handle_ftp_read( connection *cptr, u_ns_ts_t now ) 
{
  VUser *vptr = cptr->vptr;

#ifdef ENABLE_SSL
  /* See the comment below */ // size changed from 32768 to 65536 for optimization : Anuj 28/11/07
  char buf[65536 + 1];    /* must be larger than THROTTLE / 2 */ /* +1 to append '\0' for debugging/logging */
#else
  char buf[4096 + 1];     /* must be larger than THROTTLE / 2 */ /* +1 to append '\0' for debugging/logging */
#endif
  int bytes_read, bytes_handled = 0;
  //register Long checksum;
  //action_request_Shr* url_num;
  int request_type;
  //char err_msg[65545 + 1];

  NSDL2_FTP(NULL, cptr, "conn state=%d, now = %u", cptr->conn_state, now);

  request_type = cptr->url_num->request_type;
  if (request_type != FTP_REQUEST) { 
    /* Something is very wrong we should not be here. */
    NS_EXIT(-1, "Request type is not ftp but still we are in an ftp state.");
  }

  cptr->body_offset = 0;     /* Offset will always be from 0; used in get_reply_buf */

  while (1) {
    // printf ("0 Req code=%d\n", cptr->req_code);
    if ( do_throttle )
      bytes_read = THROTTLE / 2;
    else
      bytes_read = sizeof(buf) - 1;

    bytes_read = read(cptr->conn_fd, buf, bytes_read);

    NSDL2_FTP(NULL, cptr, "bytes_read = %d, buf = %*.*s\n", bytes_read, bytes_read, bytes_read, buf);

#ifdef NS_DEBUG_ON
    //if (bytes_read != 10306) printf("rcd only %d bytes\n", bytes_read);
    if (bytes_read > 0) buf[bytes_read] = '\0'; // NULL terminate for printing/logging
    NSDL3_FTP(NULL, cptr, "Read %d bytes. msg=%s", bytes_read, bytes_read>0?buf:"-");
#endif

    if ( bytes_read < 0 ) {
      if (errno == EAGAIN) {
#ifndef USE_EPOLL
        FD_SET( cptr->conn_fd, &g_rfdset );
#endif
        return 1;
      } else {
        //char request_buf[MAX_LINE_LENGTH];
        //request_buf[0] = '\0';
        NSDL3_FTP(NULL, cptr, "read failed (%s) for main: host = %s [%d]", nslb_strerror(errno),
                   cptr->url_num->index.svr_ptr->server_hostname,
                   cptr->url_num->index.svr_ptr->server_port);
        handle_ftp_bad_read (cptr, now);
        return -1;
      }
    } else if (bytes_read == 0) {
      handle_ftp_bad_read (cptr, now);
      //handle_server_close (cptr, now);
      return -1;
    }

    bytes_handled = 0;

    if (cptr->proto_state == ST_FTP_PASV) {
      copy_retrieve_data(cptr, buf, bytes_read, cptr->bytes);
      cptr->bytes += bytes_read;
    }
  
    for( ; bytes_handled < bytes_read; bytes_handled++) {
      
      if (cptr->header_state == FTP_HDST_END) {
         /* We have red the end and still there are some bytes in the buffer */
        NSDL1_FTP(NULL, NULL, "Extra bytes in buffer");

        /* We might have got 226 ALong with 150 in case we are expecting
         * response to retr. This might happen when we have not read the data
         * in the read buffers while sender has already sent both responses (intrim
         * 150 and 226) */
        if (cptr->proto_state == ST_FTP_RETR || cptr->proto_state == ST_FTP_STOR) {
          NS_DT2(NULL, cptr, DM_L1, MM_FTP, "Received response for 'RETR/STOR' command.");
          ftp_process_retr_stor(cptr, now, buf, bytes_read);
          /* Reset the hdr state to read the next msg. */
          cptr->header_state = FTP_HDST_RCODE_X;
        } else
          break;                  /* break from for */
      }
      
      switch ( cptr->header_state ) {
      case FTP_HDST_RCODE_X:
        /* if (cptr->req_code_filled < 0)  */{
          cptr->req_code = (buf[bytes_handled] - 48) * 100;
        }
        cptr->header_state = FTP_HDST_RCODE_Y;
        break;
      case FTP_HDST_RCODE_Y:
        /* if (cptr->req_code_filled < 0)  */{
          cptr->req_code += (buf[bytes_handled] - 48) * 10;
        }
        cptr->header_state = FTP_HDST_RCODE_Z;
        break;
      case FTP_HDST_RCODE_Z:   /* Save code here if not already saved */
        /* if (cptr->req_code_filled < 0)  */{
          cptr->req_code += (buf[bytes_handled] - 48);
          //cptr->req_code_filled = 1; /* True */
        }
        cptr->header_state = FTP_HDST_TEXT;
        break;
      case FTP_HDST_TEXT:
        if (buf[bytes_handled] == '\r')
          cptr->header_state = FTP_HDST_TEXT_CR;
        break;
      case FTP_HDST_TEXT_CR:
        if (buf[bytes_handled] == '\n')
          cptr->header_state = FTP_HDST_END;
        else {
          /* Something is wrong  */
        }
        break;

      default:
        break;
      }
    }
    
    /* Here it will be ftp specific so we need to add new fields in avg_time */
    cptr->tcp_bytes_recv += bytes_handled;
    INC_FTP_RX_BYTES(vptr, bytes_handled); 

#ifdef NS_DEBUG_ON
    debug_log_ftp_res(cptr, buf, bytes_read);
#endif

    if (cptr->header_state == FTP_HDST_END) {
      /* Reset the state for next time */
      cptr->header_state = FTP_HDST_RCODE_X;
      break;  /* From while */
    }
  }

  // delete ftp timeout timers
  delete_ftp_timeout_timer(cptr);

  NSDL2_FTP(NULL, cptr, "conn state=%d, proto_state = %d, now = %u", cptr->conn_state, cptr->proto_state, now);
  
  switch(cptr->proto_state) {
  case ST_FTP_CONNECTED:
    NS_DT2(NULL, cptr, DM_L1, MM_FTP, "Handshake done.");
    ftp_process_handshake(cptr, now, buf, bytes_read);
    break;
  case ST_FTP_USER:
    NS_DT2(NULL, cptr, DM_L1, MM_FTP, "Received response for 'USER' command.");
    ftp_process_user(cptr, now, buf, bytes_read);
    break;
  case ST_FTP_PASS:
    NS_DT2(NULL, cptr, DM_L1, MM_FTP, "Received response for 'PASS' command.");
    ftp_process_pass(cptr, now, buf, bytes_read);
    break;
  case ST_FTP_CMD:
   NS_DT2(NULL, cptr, DM_L1, MM_FTP, "Received response for 'CMD'.");
   ftp_process_cmd(cptr, now, buf, bytes_read);
   break;
  case ST_FTP_TYPE:
    NS_DT2(NULL, cptr, DM_L1, MM_FTP, "Received response for 'TYPE' command.");
    ftp_process_type(cptr, now, buf, bytes_read);
    break;
  case ST_FTP_PASV:
    NS_DT2(NULL, cptr, DM_L1, MM_FTP, "Received response for 'PASV' command.");
    ftp_process_pasv(cptr, now, buf, bytes_read);
    break;
  case ST_FTP_PORT:
    NS_DT2(NULL, cptr, DM_L1, MM_FTP, "Received response for 'PORT' command.");
    ftp_process_port(cptr, now, buf, bytes_read);
    break;
  case ST_FTP_RETR:
    NS_DT2(NULL, cptr, DM_L1, MM_FTP, "Received response for 'RETR' command.");
    //ftp_process_retr(cptr, now, buf, bytes_read);
    ftp_process_retr_stor(cptr, now, buf, bytes_read);   //made it generic for both retrive and store
    break;
  case ST_FTP_RETR_INTRM:
    NS_DT2(NULL, cptr, DM_L1, MM_FTP, "Received response for 'RETR' command.");
    ftp_process_retr_stor_intrm(cptr, now, buf, bytes_read);
    break;
  case ST_FTP_RETR_INTRM_RECEIVED:
    fprintf(stderr, "received interrupt on intrm_Received state.\n");
    break;
  case ST_FTP_STOR:
    NS_DT2(NULL, cptr, DM_L1, MM_FTP, "Received response for 'STOR' command.");
    ftp_process_retr_stor(cptr, now, buf, bytes_read);
    break;
  case ST_FTP_STOR_INTRM:
    NS_DT2(NULL, cptr, DM_L1, MM_FTP, "Received response for 'STOR' command.");
    ftp_process_retr_stor_intrm(cptr, now, buf, bytes_read);
    break;
  case ST_FTP_STOR_INTRM_RECEIVED:
    fprintf(stderr, "received interrupt on intrm_Received state.\n");
    break;
  case ST_FTP_QUIT:
    NS_DT2(NULL, cptr, DM_L1, MM_FTP, "Received response for 'QUIT' command.");
    ftp_process_quit(cptr, now, buf, bytes_read);
    break;
  default:
    break;
  }
  return 0;
}

static int write_ftp_msg(connection *cptr, u_ns_ts_t now, char *buf, int len)
{
  int bytes_remain = len;
  int offset = 0;
  int bytes_sent;
  int fd = ((connection *)cptr->conn_link)->conn_fd;
  VUser *vptr = cptr->vptr;

  while(bytes_remain > 0)
  {
    bytes_sent = write(fd, buf + offset, bytes_remain);
    NSDL2_FTP(NULL, cptr, "Method called,  bytes_sent = [%d]  fd = [%d]", bytes_sent, fd);
    
    if(bytes_sent < 0)  {
      if(errno == EAGAIN)
        continue;
      fprintf(stderr, "Failed to write to ftp data connection, error = %s\n", nslb_strerror(errno));
      Close_connection(cptr->conn_link, 0, now, NS_REQUEST_BAD_HDR, NS_COMPLETION_BAD_READ); //TODO: change error 
      return -1;
    }
    bytes_remain -= bytes_sent;
    offset += bytes_sent;  
    cptr->tcp_bytes_sent += bytes_sent;
    INC_FTP_DATA_TX_BYTES_COUNTER(vptr, bytes_sent);
  }
  Close_connection(cptr->conn_link, 0, now, NS_REQUEST_OK, NS_COMPLETION_CLOSE); //TODO: change error 
  return 0;
}

int handle_ftp_data_write(connection *cptr, u_ns_ts_t now){
  /*get the file from file list
    open it and write data on ftp data connection fd 
  */
 
  int tmp_len;
  char file_name[2048];
  int fd = -1;
  int size = 0;
  char *file_buf = NULL;
  struct stat st;
   
  tmp_len = get_value_from_segments(cptr, file_name, &(cptr->url_num->proto.ftp.get_files_idx[cptr->redirect_count]), "PUT", NS_MAXCDNAME);

  NSDL2_FTP(NULL, cptr, "conn state=%d, now = %u, file_name = [%s]", cptr->conn_state, now, file_name);

  if((stat(file_name, &st)) == -1){
      fprintf(stderr, "Unable to stat file [%s], error = [%s]\n", file_name, nslb_strerror(errno));
      return -1;
  }

  size = st.st_size; 

  if((fd = open(file_name, O_RDWR|O_CLOEXEC, 0666)) == -1){
    fprintf(stderr, "Error: Unable to open file %s, error=[%s]\n", file_name, nslb_strerror(errno));
    return -1;
  }

  MY_MALLOC(file_buf, size + 1, "PUT BUFF", -1);
  tmp_len = read(fd, file_buf, size);

  tmp_len = write_ftp_msg(cptr, now, file_buf, size);
  if(tmp_len == -1){
    fprintf(stderr, "Error: Failed to write ftp put data\n");
    return -1;
  }
  cptr->redirect_count++;
#ifdef NS_DEBUG_ON
   debug_log_ftp_data_res(cptr, file_buf, size);
#endif
 
  NSDL2_FTP(NULL, cptr, "redirect_count=[%d], file_count=[%d]", cptr->redirect_count, cptr->url_num->proto.ftp.num_get_files);
  FREE_AND_MAKE_NOT_NULL(file_buf, "PUT BUFF", -1)

 /* if (cptr->redirect_count == cptr->url_num->proto.ftp.num_get_files){
     handle_ftp_data_write_complete (cptr, now);
  } */
  return 0;
}

/* in handle_ftp_data_read we read till be get a close interrupt */
/* no state machines for now */
int
handle_ftp_data_read( connection *cptr, u_ns_ts_t now ) 
{
#ifdef ENABLE_SSL
  /* See the comment below */ // size changed from 32768 to 65536 for optimization : Anuj 28/11/07
  char buf[65536 + 1];    /* must be larger than THROTTLE / 2 */ /* +1 to append '\0' for debugging/logging */
#else
  char buf[4096 + 1];     /* must be larger than THROTTLE / 2 */ /* +1 to append '\0' for debugging/logging */
#endif
  int bytes_read;//, bytes_handled = 0;
  //register Long checksum;
  //action_request_Shr* url_num;
  int request_type;
  //char err_msg[65545 + 1];

  NSDL2_FTP(NULL, cptr, "conn state=%d, now = %u", cptr->conn_state, now);

  request_type = cptr->url_num->request_type;
  if (request_type != FTP_DATA_REQUEST) { 
    /* Something is very wrong we should not be here. */
    NS_EXIT(-1, "Request type is not ftp but still we are in an ftp state.");
  }

  cptr->body_offset = 0;     /* Offset will always be from 0; used in get_reply_buf */

  while (1) {
    // printf ("0 Req code=%d\n", cptr->req_code);
    if ( do_throttle )
      bytes_read = THROTTLE / 2;
    else
      bytes_read = sizeof(buf) - 1;

    bytes_read = read(cptr->conn_fd, buf, bytes_read);

    NSDL2_FTP(NULL, cptr, "bytes_read = %d", bytes_read);

#ifdef NS_DEBUG_ON
    //if (bytes_read != 10306) printf("rcd only %d bytes\n", bytes_read);
    if (bytes_read > 0) buf[bytes_read] = '\0'; // NULL terminate for printing/logging
    NSDL3_FTP(NULL, cptr, "Read %d bytes. msg=%s", bytes_read, bytes_read>0?buf:"-");
#endif

    if ( bytes_read < 0 ) {
      if (errno == EINTR) 
        continue;
      if (errno == EAGAIN) {
#ifndef USE_EPOLL
        FD_SET( cptr->conn_fd, &g_rfdset );
#endif
        return 1;
      } else {
        //char request_buf[MAX_LINE_LENGTH];
        //request_buf[0] = '\0';
        NSDL3_FTP(NULL, cptr, "read failed (%s) for main: host = %s [%d]", nslb_strerror(errno),
                   cptr->url_num->index.svr_ptr->server_hostname,
                   cptr->url_num->index.svr_ptr->server_port);
        //handle_ftp_data_bad_read (cptr, now);
        //handle_ftp_bad_read (cptr, now);
        Close_connection(cptr->conn_link, 0, now, NS_REQUEST_BAD_HDR, NS_COMPLETION_BAD_READ);
        Close_connection(cptr, 0, now, NS_REQUEST_BAD_HDR, NS_COMPLETION_BAD_READ);
        return -1;
      }
    } else if (bytes_read == 0) {
      handle_ftp_data_read_complete (cptr, now);
      NSDL2_FTP(NULL, cptr, "ftp_avgtime->ftp_rx_bytes = %ld", ftp_avgtime->ftp_rx_bytes);
      //handle_server_close (cptr, now);
      return -1;
    }

    ftp_avgtime->ftp_rx_bytes += bytes_read;
    NSDL2_FTP(NULL, cptr, "ftp_avgtime->ftp_rx_bytes = %ld, bytes_read = %d", ftp_avgtime->ftp_rx_bytes, bytes_read);
    // delete ftp timeout timers
    delete_ftp_timeout_timer(cptr);
    add_ftp_timeout_timer(cptr, now);

    //bytes_handled = 0;
    
#ifdef NS_DEBUG_ON
    debug_log_ftp_data_res(cptr, buf, bytes_read);
#endif
  }  

  return 0;
}
