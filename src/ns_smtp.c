/**
 * FILE: ns_smtp.c
 * PURPOSE: contains all SMTP reading and state switching mechanism
 * AUTHOR: bhav
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
#include "ns_trace_level.h"

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
#include "ns_common.h"
#include "nslb_util.h"
#include "ns_log_req_rep.h"
#include "ns_group_data.h"
#include "ns_exit.h"

/* This table maps string equivalents for SMTP states */
char g_smtp_st_str[][0xff] = {  /* The length of each string can not be >= 0xff */
  "SMTP_INITIALIZATION",
  "SMTP_CONNECTED",
  "SMTP_HELO",
  "SMTP_EHLO",
  "SMTP_STARTTLS",
  "SMTP_AUTH_LOGIN",
  "SMTP_AUTH_LOGIN_USER_ID",
  "SMTP_AUTH_LOGIN_PASSWD",
  "SMTP_MAIL",
  "SMTP_RCPT",
  "SMTP_RCPT_CC",
  "SMTP_RCPT_BCC",
  "SMTP_DATA",
  "SMTP_DATA_BODY",
  "SMTP_RSET",
  "SMTP_QUIT"
};

/* Common error code mapping */
static void set_smtp_error_code(connection *cptr, int err_code) {

  NSDL2_SMTP(NULL, cptr, "Method called, err_code = %d", err_code) ;
  
  switch(err_code) {
  case 535:  // 535 Authentication failed 
    cptr->req_ok = NS_REQUEST_AUTH_FAIL;
    break;
  case 550:  // Requested action not taken: mailbox unavailable
  case 553:  // Requested action not taken: mailbox name not allowed
    cptr->req_ok = NS_REQUEST_MBOX_ERR;
    break;
  case 552:  // Requested mail action aborted: exceeded storage allocation 
    cptr->req_ok = NS_REQUEST_MBOX_STORAGE_ERR;
    break;
  case 554:  // Transaction failed (Or, in the case of a connection-opening response, "No SMTP service here") 
    cptr->req_ok = NS_REQUEST_5xx;
    break;
  case 551:  // User not local; please try <forward-path>
  case 555:  // MAIL FROM/RCPT TO parameters not recognized or not implemented
    cptr->req_ok = NS_REQUEST_BAD_RESP;
    break;
  default:
    cptr->req_ok = NS_REQUEST_BAD_RESP;
    break;
  }
}

void delete_smtp_timeout_timer(connection *cptr) {

  NSDL2_SMTP(NULL, cptr, "Method called, timer type = %d", cptr->timer_ptr->timer_type);

  if ( cptr->timer_ptr->timer_type == AB_TIMEOUT_IDLE ) {
    NSDL2_SMTP(NULL, cptr, "Deleting Idle timer.");
    dis_timer_del(cptr->timer_ptr);
  }
}

/* Function calls retry_connection() which will ensure normal http like retries for SMTP also. */
static inline void
handle_smtp_bad_read (connection *cptr, u_ns_ts_t now)
{
  NSDL2_SMTP(NULL, cptr, "cptr=%p conn state=%d, now = %u", cptr, cptr->conn_state, now);
 
  retry_connection(cptr, now, NS_REQUEST_BAD_HDR); /* TODO:BHAV use same code NS_REQUEST_BAD_READ */
}


/**
 * Below are the processing functions for each type of command. The functions
 * process responses from commands issued to the SMTP server.
 */

/**
 * smtp_handle_handshake(): expects +ve initial greeting from the server
 * and sends EHLO.
 */
void smtp_process_handshake(connection *cptr, u_ns_ts_t now, char *buf, int bytes_read) 
{
  int req_code = cptr->req_code;

  NSDL2_SMTP(NULL, cptr, "conn state=%d, req_code = %d, buf = %s, bytes_read = %d, now = %u",
                          cptr->conn_state, req_code, buf, bytes_read, now);

  /* we should prob. check ESMTP ready and supported AUTH type ? */
  if (req_code == 220) {
    smtp_send_ehlo(cptr, now);
  } else {
    Close_connection(cptr, 0, now, NS_REQUEST_BAD_RESP, NS_COMPLETION_SMTP_ERROR);
  }
}

void smtp_process_ehlo(connection *cptr, u_ns_ts_t now, char *buf, int bytes_read, int starttls) 
{
  int req_code = cptr->req_code;
  
  int first_digit  = req_code / 100;
  int second_digit = (req_code / 10) % 10;
  //int third_digit  = (req_code % 10);
  
  NSDL2_SMTP(NULL, cptr, "conn state=%d, req_code = %d, buf = %s, bytes_read = %d, now = %u",
                          cptr->conn_state, req_code, buf, bytes_read, now);

  switch (first_digit) {
  case 2:         /* EHLO successful */
    if (starttls){
      smtp_send_starttls(cptr, now);
    }
    else { 
    if (cptr->url_num->proto.smtp.user_id.seg_start != NULL) { /* we have to send auth */
      smtp_send_auth_login(cptr, now);  /* supporting AUTH LOGIN only for now */
    } else {     /* send MAIL directly */
      smtp_send_mail(cptr, now);
    }
    }
   
    break;

  case 4:    /* Retry? */
    smtp_send_quit(cptr, now);
    break;

  case 5:
    switch(second_digit) {
    case 0:   /* not supported command */
      /* EHLO not supported we will now try HELO */
      smtp_send_helo(cptr, now);
      break;
    default:
      set_smtp_error_code(cptr, req_code);
      smtp_send_quit(cptr, now);
    }
    break;

  default: /* Error */
    set_smtp_error_code(cptr, req_code);
    smtp_send_quit(cptr, now);
    break;
  }
}

void smtp_process_helo(connection *cptr, u_ns_ts_t now, char *buf, int bytes_read) 
{
  int req_code = cptr->req_code;
  
  int first_digit  = req_code / 100;
  int second_digit = (req_code / 10) % 10;
  //int third_digit  = (req_code % 10);
  
  NSDL2_SMTP(NULL, cptr, "conn state=%d, req_code = %d, buf = %s, bytes_read = %d, now = %u",
                          cptr->conn_state, req_code, buf, bytes_read, now);

  switch (first_digit) {
  case 2:         /* HELO successful */
    /* we dont send AUTH here since the server does not supports ESMTP */
    smtp_send_mail(cptr, now);
    break;
  case 4:
    smtp_send_quit(cptr, now);
    break;
  case 5:
    switch(second_digit) {
    case 0:   /* not supported command */
      /* EHLO not supported we will now call HELO */
      smtp_send_helo(cptr, now);
    default:
      set_smtp_error_code(cptr, req_code);
      smtp_send_quit(cptr, now);
      break;
    }
    break;
  default: /* Error */
    set_smtp_error_code(cptr, req_code);
    smtp_send_quit(cptr, now);
    break;
  }
}

/*------------------------------------------------------------------------------
Function to process response of STARTTLS
command from the server.

--------------------------------------------------------------------------------*/

void smtp_process_starttls(connection *cptr, u_ns_ts_t now, char *buf, int bytes_read) 
{
  int req_code = cptr->req_code;
  
  int first_digit  = req_code / 100;
  //int second_digit = (req_code / 10) % 10;
  //int third_digit  = (req_code % 10);
  
  NSDL2_SMTP(NULL, cptr, "conn state=%d, req_code = %d, buf = %s, bytes_read = %d, now = %u",
                          cptr->conn_state, req_code, buf, bytes_read, now);

  switch (first_digit) {
  case 2:         /* STARTTLS successful */
    /* we have to make connection secure */
    cptr->request_type = SMTPS_REQUEST;
    cptr->proto_state = ST_SMTP_STARTTLS_LOGIN;
    cptr->conn_state = CNST_CONNECTING;
    
    handle_connect(cptr, now, 0);

    break;
  case 4:
    smtp_send_quit(cptr, now);
    break;
  case 5:
   // switch(second_digit) {
   // case 0:   /* not supported command */
   //   /* EHLO not supported we will now call HELO */
   //   smtp_send_helo(cptr, now);
   // default:
      set_smtp_error_code(cptr, req_code);
      smtp_send_quit(cptr, now);
   //   break;
   // }
    break;
  default: /* Error */
    set_smtp_error_code(cptr, req_code);
    smtp_send_quit(cptr, now);
    break;
  }
}


/* Process auth Intrim state */
void smtp_process_auth_login(connection *cptr, u_ns_ts_t now, char *buf, int bytes_read)
{
  int req_code = cptr->req_code;
  
  int first_digit  = req_code / 100;
  int second_digit = (req_code / 10) % 10;
  //int third_digit  = (req_code % 10);
  
  NSDL2_SMTP(NULL, cptr, "conn state=%d, req_code = %d, buf = %s, bytes_read = %d, now = %u",
                          cptr->conn_state, req_code, buf, bytes_read, now);

  switch (first_digit) {
  case 3:   /* AUTH LOGIN supported we send the user id */
    smtp_send_auth_login_user_id(cptr, now);
    break;

  case 5:     
    switch(second_digit) {
    case 0:   /* not supported command */
      /*  not supported we will now try sending MAIL */
      smtp_send_mail(cptr, now);
      break;
    default:
      set_smtp_error_code(cptr, req_code);
      smtp_send_quit(cptr, now);
      break;
    }
    break;

    /* All other cases we quit */
  default: /* Error */
    set_smtp_error_code(cptr, req_code);
    smtp_send_quit(cptr, now);
    break;
  } 
}

void smtp_process_auth_login_user_id(connection *cptr, u_ns_ts_t now, char *buf, int bytes_read)
{
  int req_code = cptr->req_code;
  
  int first_digit  = req_code / 100;
  int second_digit = (req_code / 10) % 10;
  //int third_digit  = (req_code % 10);
  
  NSDL2_SMTP(NULL, cptr, "conn state=%d, req_code = %d, buf = %s, bytes_read = %d, now = %u",
                          cptr->conn_state, req_code, buf, bytes_read, now);

  switch (first_digit) {
  case 3:   /* user name accepted  we send the passwd */
    smtp_send_auth_login_passwd(cptr, now);
    break;

  case 5:     /* This basically user id  auth failed, we should try to send just MAIL */
    switch(second_digit) {
/*    case 0:   // auth failed
      //  not supported we will now try sending MAIL 
      smtp_send_mail(cptr, now);
      break;
*/
    default:
      set_smtp_error_code(cptr, req_code);
      smtp_send_quit(cptr, now);
      break;
    }
    break;

    /* All other cases we quit */
  default: /* Error */
    set_smtp_error_code(cptr, req_code);
    smtp_send_quit(cptr, now);
    break;
  } 
}

void smtp_process_auth_login_passwd(connection *cptr, u_ns_ts_t now, char *buf, int bytes_read)
{
  int req_code = cptr->req_code;
  
  int first_digit  = req_code / 100;
  int second_digit = (req_code / 10) % 10;
  //int third_digit  = (req_code % 10);
  
  NSDL2_SMTP(NULL, cptr, "conn state=%d, req_code = %d, buf = %s, bytes_read = %d, now = %u",
                          cptr->conn_state, req_code, buf, bytes_read, now);

  switch (first_digit) {
  case 2:   /* passwd  accepted  we send mail */
      smtp_send_mail(cptr, now);
    break;

    /* all other cases are errors */
  case 5:     /* This basically means auth failed, we should try to send just MAIL */
    switch(second_digit) {
/*    case 0:   // auth failed
      //  not supported we will now try sending MAIL
      smtp_send_mail(cptr, now);
      break;
*/
    default:
      set_smtp_error_code(cptr, req_code);
      smtp_send_quit(cptr, now);
      break;
    }
    break;

    /* All other cases we quit */
  default: /* Error */
    set_smtp_error_code(cptr, req_code);
    smtp_send_quit(cptr, now);
    break;
  }
}

void smtp_process_mail(connection *cptr, u_ns_ts_t now, char *buf, int bytes_read)
{
  int req_code = cptr->req_code;
  
  int first_digit  = req_code / 100;
  //int second_digit = (req_code / 10) % 10;
  //int third_digit  = (req_code % 10);
  
  NSDL2_SMTP(NULL, cptr, "conn state=%d, req_code = %d, buf = %s, bytes_read = %d, now = %u",
                          cptr->conn_state, req_code, buf, bytes_read, now);

  switch (first_digit) {
  case 2:   /* MAIL FROM:  accepted we send mail */
    smtp_send_rcpt(cptr, now);
    break;

    /* all other cases are errors */
    /* All other cases we quit */
  default: /* Error */
    set_smtp_error_code(cptr, req_code);
    smtp_send_quit(cptr, now);
    break;
  }
}

void smtp_process_rcpt(connection *cptr, u_ns_ts_t now, char *buf, int bytes_read)
{
  int req_code = cptr->req_code;
  
  int first_digit  = req_code / 100;
  //int second_digit = (req_code / 10) % 10;
  //int third_digit  = (req_code % 10);
  
  NSDL2_SMTP(NULL, cptr, "conn state=%d, req_code = %d, buf = %s, bytes_read = %d, now = %u, first_digit = %d",
                          cptr->conn_state, req_code, buf, bytes_read, now, first_digit);

  switch (first_digit) {
  case 2:   /* RCPT TO:  accepted */
    smtp_send_rcpt(cptr, now);
    break;

  case 5:   /* RCPT TO was rejected; as Long as we have atleast one success RCPT we are ok. */
    smtp_send_rcpt(cptr, now);

    break;
    /* all other cases are errors */
    /* All other cases we quit */
  default: /* Error */
    set_smtp_error_code(cptr, req_code);
    smtp_send_quit(cptr, now);
    break;
  }
}

void smtp_process_rcpt_cc(connection *cptr, u_ns_ts_t now, char *buf, int bytes_read)
{
  int req_code = cptr->req_code;
  
  int first_digit  = req_code / 100;
  //int second_digit = (req_code / 10) % 10;
  //int third_digit  = (req_code % 10);
  
  NSDL2_SMTP(NULL, cptr, "conn state=%d, req_code = %d, buf = %s, bytes_read = %d, now = %u",
                          cptr->conn_state, req_code, buf, bytes_read, now);

  switch (first_digit) {
  case 2:   /* RCPT TO:  accepted */
    smtp_send_rcpt_cc(cptr, now);
    break;

  case 5:   /* RCPT TO was rejected; as Long as we have atleast one success RCPT we are ok. */
    smtp_send_rcpt_cc(cptr, now);

    break;
    /* all other cases are errors */
    /* All other cases we quit */
  default: /* Error */
    set_smtp_error_code(cptr, req_code);
    smtp_send_quit(cptr, now);
    break;
  }
}

void smtp_process_rcpt_bcc(connection *cptr, u_ns_ts_t now, char *buf, int bytes_read)
{
  int req_code = cptr->req_code;
  
  int first_digit  = req_code / 100;
  //int second_digit = (req_code / 10) % 10;
  //int third_digit  = (req_code % 10);
  
  NSDL2_SMTP(NULL, cptr, "conn state=%d, req_code = %d, buf = %s, bytes_read = %d, now = %u",
                          cptr->conn_state, req_code, buf, bytes_read, now);

  switch (first_digit) {
  case 2:   /* RCPT TO:  accepted */
    smtp_send_rcpt_bcc(cptr, now);
    break;

  case 5:   /* RCPT TO was rejected; as Long as we have atleast one success RCPT we are ok. */
    smtp_send_rcpt_bcc(cptr, now);

    break;
    /* all other cases are errors */
    /* All other cases we quit */
  default: /* Error */
    set_smtp_error_code(cptr, req_code);
    smtp_send_quit(cptr, now);
    break;
  }
}

void smtp_process_data(connection *cptr, u_ns_ts_t now, char *buf, int bytes_read)
{
  int req_code = cptr->req_code;
  
  int first_digit  = req_code / 100;
  //int second_digit = (req_code / 10) % 10;
  //int third_digit  = (req_code % 10);
  
  NSDL2_SMTP(NULL, cptr, "conn state=%d, req_code = %d, buf = %s, bytes_read = %d, now = %u",
                          cptr->conn_state, req_code, buf, bytes_read, now);

  switch (first_digit) {
  case 3:
    smtp_send_data_body(cptr, now); 
    break;

    /* All other cases we quit */
  default: /* Error */
    set_smtp_error_code(cptr, req_code);
    smtp_send_quit(cptr, now);
    break;
  }
}

void smtp_process_data_body(connection *cptr, u_ns_ts_t now, char *buf, int bytes_read)
{
  int req_code = cptr->req_code;
  
  int first_digit  = req_code / 100;
  //int second_digit = (req_code / 10) % 10;
  //int third_digit  = (req_code % 10);
  
  NSDL2_SMTP(NULL, cptr, "conn state=%d, req_code = %d, buf = %s, bytes_read = %d, now = %u, Mail Sent Count = %d",
                          cptr->conn_state, req_code, buf, bytes_read, now, cptr->mail_sent_count);
  
  switch (first_digit) {
  case 2:                       /* Mail accepted. */
    /* check if we need to send more mails */
    //if (cptr->mail_sent_count < cptr->url_num->proto.smtp.msg_count) { /* Thats it we quit */
    if (cptr->mail_sent_count > 0) { /* we have to send more mails */
      smtp_send_mail(cptr, now);
    } else {                    /* Thats it we quit */
      smtp_send_quit(cptr, now);
      cptr->mail_sent_count = 0;
    }
    break;

    /* All other cases we quit */
  default: /* Error */
    set_smtp_error_code(cptr, req_code);
    smtp_send_quit(cptr, now);
    break;
  }
}

void smtp_process_quit(connection *cptr, u_ns_ts_t now, char *buf, int bytes_read)
{
  int status   = cptr->req_ok;
  
  NSDL2_SMTP(NULL, cptr, "conn state=%d, req_code = %d, buf = %s, bytes_read = %d, now = %u",
                          cptr->conn_state, cptr->req_code, buf, bytes_read, now);
    
  // Confail means req_ok is not filled by any error earliar, so its a success case
  if(status == NS_REQUEST_CONFAIL)
    status = NS_REQUEST_OK;

  Close_connection(cptr, 0, now, status, NS_COMPLETION_CLOSE);
}

/* This function converts the int state to STR */
char *smtp_state_to_str(int state) 
{
  /* TODO bounds checking */
  return g_smtp_st_str[state];
}

void debug_log_smtp_res(connection *cptr, char *buf, int size)
{
  VUser *vptr;
  vptr = (VUser *)cptr->vptr;
  int request_type = cptr->url_num->request_type;

  if (!((runprof_table_shr_mem[vptr->group_num].gset.debug & DM_LOGIC4) && 
       ((request_type == SMTP_REQUEST || request_type == SMTPS_REQUEST) &&
         (runprof_table_shr_mem[vptr->group_num].gset.module_mask & MM_SMTP))))
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
   sprintf(log_file, "%s/logs/%s/smtp_session_%hd_%u_%u_%d_0_%d_%d_%d_0.dat",
          g_ns_wdir, req_rep_file_path, child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,
          vptr->group_num, GET_SESS_ID_BY_NAME(vptr),
          GET_PAGE_ID_BY_NAME(vptr));
 
   /* sprintf(log_file, "%s/logs/TR%d/smtp_session_%hd_%u_%u_%d_0_%d_%d_%d_0.dat", 
            g_ns_wdir, testidx, child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,
            vptr->group_num, GET_SESS_ID_BY_NAME(vptr),
            GET_PAGE_ID_BY_NAME(vptr));*/
    
    // Do not change the debug trace message as it is parsed by GUI
    if(cptr->tcp_bytes_recv == 0)  //This check to print debug trace only once
      NS_DT4(vptr, cptr, DM_L1, MM_SMTP, "Response is in file '%s'", log_file);
    
    //Since response can come partialy so this will print debug trace many time
    //cptr->tcp_bytes_recv = 0, means this response comes first time

    if((log_fd = open(log_file, O_CREAT|O_WRONLY|O_APPEND|O_CLOEXEC, 00666)) < 0)
        fprintf(stderr, "Error: Error in opening file for logging URL request\n"
);
    else
    {
      write(log_fd, buf, size);
      close(log_fd);
    }
  }
}

/**
 * function handle_smtp_read() is the main function handling SMTP protocol
 * All state changes take place in this function. 
 * Most of the code is borrowed from http's handle_read()
 *
 */
int
handle_smtp_read( connection *cptr, u_ns_ts_t now ) 
{
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
  int err = 0;
  char *err_buff = NULL;
  //char err_msg[65545 + 1];
  int starttls = 0;
  VUser *vptr = cptr->vptr;
  NSDL2_SMTP(NULL, cptr, "conn state=%d, now = %u", cptr->conn_state, now);

  request_type = cptr->request_type;
  if (request_type != SMTP_REQUEST && request_type != SMTPS_REQUEST) { 
    /* Something is very wrong we should not be here. */
    NS_EXIT(-1, "Request type is not SMTP but still we are in an SMTP state.");
  }

  cptr->body_offset = 0;     /* Offset will always be from 0; used in get_reply_buf */

  while (1) {
    // printf ("0 Req code=%d\n", cptr->req_code);
    if ( do_throttle )
      bytes_read = THROTTLE / 2;
    else
      bytes_read = sizeof(buf) - 1;
#ifdef ENABLE_SSL
  if (request_type == SMTPS_REQUEST) {
    bytes_read = SSL_read(cptr->ssl, buf, bytes_read);
    if (bytes_read > 0) buf[bytes_read] = '\0'; //NULL terminated for printing/logging
      NSDL3_SSL(NULL, cptr, "Read %d bytes. msg=%s", bytes_read, (bytes_read>0)?buf:"-");
    if (bytes_read <= 0) {
      err = SSL_get_error(cptr->ssl, bytes_read);
      switch(err) {
        case SSL_ERROR_ZERO_RETURN:  /* means that the connection closed from the server */
          handle_smtp_bad_read (cptr, now);
          return -1;
        case SSL_ERROR_WANT_READ:
          return -1;   /* It can but isn't supposed to happen */
        case SSL_ERROR_WANT_WRITE:
          fprintf(stderr, "SSL_read error: SSL_ERROR_WANT_WRITE\n");
          handle_smtp_bad_read (cptr, now);
          return -1;
        case SSL_ERROR_SYSCALL: //Some I/O error occurred
          if (errno == EAGAIN) // no more data available, return (it is like SSL_ERROR_WANT_READ)
          {
            NSDL1_SSL(NULL, cptr, "SMTP SSL_read: No more data available, return");
            handle_smtp_bad_read (cptr, now);
            return -1;
          }

          if (errno == EINTR)
          {
            NSDL3_SSL(NULL, cptr, "SMTP SSL_read interrupted. Continuing...");
            continue;
          }
          /* FALLTHRU */
        case SSL_ERROR_SSL: //A failure in the SSL library occurred, usually a protocol error
          /* FALLTHRU */
        default:
          err_buff = ERR_error_string(err, NULL);
          NSTL1(NULL, NULL, "SSl library error %s ", err_buff);
          //ERR_print_errors_fp(ssl_logs);
          if ((bytes_read == 0) && (!runprof_table_shr_mem[((VUser *)(cptr->vptr))->group_num].gset.ssl_clean_close_only))
            //handle_server_close (cptr, now);
            handle_smtp_bad_read (cptr, now);
          else
            handle_smtp_bad_read (cptr, now);
          return -1;
      }
    }
  } 
  else {
    bytes_read = read(cptr->conn_fd, buf, bytes_read);

    NSDL2_SMTP(NULL, cptr, "bytes_read = %d, buf = %*.*s\n", bytes_read, bytes_read, bytes_read, buf);
   
#ifdef NS_DEBUG_ON
    //if (bytes_read != 10306) printf("rcd only %d bytes\n", bytes_read);
    if (bytes_read > 0) buf[bytes_read] = '\0'; // NULL terminate for printing/logging
    NSDL3_SMTP(NULL, cptr, "Read %d bytes. msg=%s", bytes_read, bytes_read>0?buf:"-");
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
        NSDL3_SMTP(NULL, cptr, "read failed (%s) for main: host = %s [%d]", nslb_strerror(errno),
                   cptr->url_num->index.svr_ptr->server_hostname,
                   cptr->url_num->index.svr_ptr->server_port);
        handle_smtp_bad_read (cptr, now);
        return -1;
      }
    } else if (bytes_read == 0) {
      handle_smtp_bad_read (cptr, now);
      //handle_server_close (cptr, now);
      return -1;
    }
  }

#else
   bytes_read = read(cptr->conn_fd, buf, bytes_read);
   NSDL2_SMTP(NULL, cptr, "bytes_read = %d, buf = %*.*s\n", bytes_read, bytes_read, bytes_read, buf);

#ifdef NS_DEBUG_ON
   if (bytes_read > 0) buf[bytes_read] = '\0'; // NULL terminate for printing/logging
     NSDL3_SMTP(NULL, cptr, "Read %d bytes. msg=%s", bytes_read, bytes_read>0?buf:"-");
#endif

   if ( bytes_read < 0 ) {
     if (errno == EAGAIN) {
#ifndef USE_EPOLL
   FD_SET( cptr->conn_fd, &g_rfdset );
#endif
   return 1;
     } else {
        NSDL3_SMTP(NULL, cptr, "read failed (%s) for main: host = %s [%d]", nslb_strerror(errno),
                   cptr->url_num->index.svr_ptr->server_hostname,
                   cptr->url_num->index.svr_ptr->server_port);
        handle_smtp_bad_read (cptr, now);
        return -1;
      }
    } else if (bytes_read == 0) {
      handle_smtp_bad_read (cptr, now);
      return -1;
    }
#endif

    bytes_handled = 0;

    /* we are not doing copy_retrieve data since we are not parsing it yet. will have
     * to do it in the future in case of partial read */
    //    copy_retrieve_data(cptr, &buf[bytes_handled], bytes_read - bytes_handled, cptr->bytes);
  
    for( ; bytes_handled < bytes_read; bytes_handled++) {

      if (cptr->header_state == SMTP_HDST_END) {
         /* We have red the end and still there are some bytes in the buffer */
        NSDL1_SMTP(NULL, NULL, "Extra bytes in buffer");
        break;                  /* break from for */
      }
      
      switch ( cptr->header_state ) {
      case SMTP_HDST_RCODE_X:
        /* if (cptr->req_code_filled < 0)  */{
          cptr->req_code = (buf[bytes_handled] - 48) * 100;
        }
        cptr->header_state = SMTP_HDST_RCODE_Y;
        break;
      case SMTP_HDST_RCODE_Y:
        /* if (cptr->req_code_filled < 0)  */{
          cptr->req_code += (buf[bytes_handled] - 48) * 10;
        }
        cptr->header_state = SMTP_HDST_RCODE_Z;
        break;
      case SMTP_HDST_RCODE_Z:   /* Save code here if not already saved */
        /* if (cptr->req_code_filled < 0)  */{
          cptr->req_code += (buf[bytes_handled] - 48);
          //cptr->req_code_filled = 1; /* True */
        }
        cptr->header_state = SMTP_HDST_SPACE_HYPHEN;
        break;
      case SMTP_HDST_SPACE_HYPHEN:
        if (buf[bytes_handled] == ' ')
          cptr->header_state = SMTP_HDST_SPACE_TEXT;
        else if (buf[bytes_handled] == '-')
          cptr->header_state = SMTP_HDST_HYPHEN_TEXT;
        break;
      case SMTP_HDST_SPACE_TEXT:
        if (buf[bytes_handled] == '\r')
          cptr->header_state = SMTP_HDST_SPACE_CFLR;
        break;
      case SMTP_HDST_SPACE_CFLR:
        if (buf[bytes_handled] == '\n')
          cptr->header_state = SMTP_HDST_END;
        else {
          /* Something is wrong  */
        }
        break;
      case SMTP_HDST_HYPHEN_TEXT:
        if (buf[bytes_handled] == '\r')
          cptr->header_state = SMTP_HDST_HYPHEN_CFLR;
        else {
          if (buf[bytes_handled] == 'S') {
            cptr->header_state = SMTP_HDST_HYPHEN_TEXT_S;
            NSDL2_SMTP(NULL, cptr, "SMTP_HDST_HYPHEN_TEXT_S");
          }
        }
        break;
      case SMTP_HDST_HYPHEN_CFLR:
        if (buf[bytes_handled] == '\n')
          cptr->header_state = SMTP_HDST_RCODE_X; /* we should expect a line again */
        else {
          /* Something is wrong  */
        }
        break;
      case SMTP_HDST_HYPHEN_TEXT_S:
        if (buf[bytes_handled] == 'T'){
          cptr->header_state = SMTP_HDST_HYPHEN_TEXT_ST;
          NSDL2_SMTP(NULL, cptr, "SMTP_HDST_HYPHEN_TEXT_ST");
        }
        else {
          cptr->header_state = SMTP_HDST_HYPHEN_TEXT;  
          NSDL2_SMTP(NULL, cptr, "SMTP_HDST_HYPHEN_TEXT");
        }
        break;
      case SMTP_HDST_HYPHEN_TEXT_ST:
        if (buf[bytes_handled] == 'A'){
          cptr->header_state = SMTP_HDST_HYPHEN_TEXT_STA;
          NSDL2_SMTP(NULL, cptr, "SMTP_HDST_HYPHEN_TEXT_STA");
        }
        else {
          cptr->header_state = SMTP_HDST_HYPHEN_TEXT;  
          NSDL2_SMTP(NULL, cptr, "SMTP_HDST_HYPHEN_TEXT");
        }
        break;
      case SMTP_HDST_HYPHEN_TEXT_STA:
        if (buf[bytes_handled] == 'R'){
          cptr->header_state = SMTP_HDST_HYPHEN_TEXT_STAR;
          NSDL2_SMTP(NULL, cptr, "SMTP_HDST_HYPHEN_TEXT_STAR");
        }
        else {
          cptr->header_state = SMTP_HDST_HYPHEN_TEXT;  
          NSDL2_SMTP(NULL, cptr, "SMTP_HDST_HYPHEN_TEXT");
        }
        break;
      case SMTP_HDST_HYPHEN_TEXT_STAR:
        if (buf[bytes_handled] == 'T'){
          cptr->header_state = SMTP_HDST_HYPHEN_TEXT_START;
          NSDL2_SMTP(NULL, cptr, "SMTP_HDST_HYPHEN_TEXT_START");
        }
        else {
          cptr->header_state = SMTP_HDST_HYPHEN_TEXT;  
          NSDL2_SMTP(NULL, cptr, "SMTP_HDST_HYPHEN_TEXT");
        }
        break;
      case SMTP_HDST_HYPHEN_TEXT_START:
        if (buf[bytes_handled] == 'T'){
          cptr->header_state = SMTP_HDST_HYPHEN_TEXT_STARTT;
          NSDL2_SMTP(NULL, cptr, "SMTP_HDST_HYPHEN_TEXT_STARTT");
        }
        else {
          cptr->header_state = SMTP_HDST_HYPHEN_TEXT;  
          NSDL2_SMTP(NULL, cptr, "SMTP_HDST_HYPHEN_TEXT");
        }
        break;
      case SMTP_HDST_HYPHEN_TEXT_STARTT:
        if (buf[bytes_handled] == 'L'){
          cptr->header_state = SMTP_HDST_HYPHEN_TEXT_STARTTL;
          NSDL2_SMTP(NULL, cptr, "SMTP_HDST_HYPHEN_TEXT_STARTTL");
        }
        else {
          cptr->header_state = SMTP_HDST_HYPHEN_TEXT;  
          NSDL2_SMTP(NULL, cptr, "SMTP_HDST_HYPHEN_TEXT");
        }
        break;
      case SMTP_HDST_HYPHEN_TEXT_STARTTL:
        if ((buf[bytes_handled] == 'S') && (cptr->proto_state == ST_SMTP_EHLO) && (request_type != SMTPS_REQUEST) && (cptr->url_num->proto.smtp.authentication_type)){
          starttls = 1;
          cptr->header_state = SMTP_HDST_HYPHEN_TEXT;  
          NSDL2_SMTP(NULL, cptr, "SMTP_HDST_HYPHEN_TEXT_STARTTLS");
        }
        else {
          cptr->header_state = SMTP_HDST_HYPHEN_TEXT;  
          NSDL2_SMTP(NULL, cptr, "SMTP_HDST_HYPHEN_TEXT");
        }
        break;

      default:
        break;
      }
    }

    /* Here it will be SMTP specific so we need to add new fields in avg_time */
    cptr->tcp_bytes_recv += bytes_handled;
    average_time->smtp_rx_bytes += bytes_handled;
    cptr->bytes += bytes_handled;
    average_time->smtp_total_bytes += bytes_handled;
    if(SHOW_GRP_DATA)
    {
      avgtime *lol_average_time;
      lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
      lol_average_time->smtp_rx_bytes += bytes_handled;
      lol_average_time->smtp_total_bytes += bytes_handled;
    } 

    
#ifdef NS_DEBUG_ON
    debug_log_smtp_res(cptr, buf, bytes_read);
#endif

    if (cptr->header_state == SMTP_HDST_END) {
      /* Reset the state for next time */
      cptr->header_state = SMTP_HDST_RCODE_X;
      break;  /* From while */
    }
  }

  // delete smtp timeout timers
  delete_smtp_timeout_timer(cptr);

  NSDL2_SMTP(NULL, cptr, "conn state=%d, proto_state = %d, now = %u", cptr->conn_state, cptr->proto_state, now);
  
  switch(cptr->proto_state) {
  case ST_SMTP_CONNECTED:
    NS_DT2(NULL, cptr, DM_L1, MM_SMTP, "Handshake done.");
    smtp_process_handshake(cptr, now, buf, bytes_read);
    break;
  case ST_SMTP_HELO:
    NS_DT2(NULL, cptr, DM_L1, MM_SMTP, "Received response for 'HELO' command.");
    smtp_process_helo(cptr, now, buf, bytes_read);
    break;
  case ST_SMTP_EHLO:
    NS_DT2(NULL, cptr, DM_L1, MM_SMTP, "Received response for 'EHLO' command.");
    smtp_process_ehlo(cptr, now, buf, bytes_read, starttls);
    break;
  case ST_SMTP_STARTTLS:
    NS_DT2(NULL, cptr, DM_L1, MM_SMTP, "Received response for 'STARTTLS' command.");
    smtp_process_starttls(cptr, now, buf, bytes_read);
    break;
  case ST_SMTP_AUTH_LOGIN:
    NS_DT2(NULL, cptr, DM_L1, MM_SMTP, "Received response for 'AUTH_LOGIN' command.");
    smtp_process_auth_login(cptr, now, buf, bytes_read);
    break;
  case ST_SMTP_AUTH_LOGIN_USER_ID:
    NS_DT2(NULL, cptr, DM_L1, MM_SMTP, "Received response for 'AUTH_LOGIN_USER_ID' command.");
    smtp_process_auth_login_user_id(cptr, now, buf, bytes_read);
    break;
  case ST_SMTP_AUTH_LOGIN_PASSWD:
    NS_DT2(NULL, cptr, DM_L1, MM_SMTP, "Received response for 'AUTH_LOGIN_PASSWORD' command.");
    smtp_process_auth_login_passwd(cptr, now, buf, bytes_read);
    break;
  case ST_SMTP_MAIL:
    NS_DT2(NULL, cptr, DM_L1, MM_SMTP, "Received response for 'MAIL FROM:' command.");
    smtp_process_mail(cptr, now, buf, bytes_read);
    break;
  case ST_SMTP_RCPT:
    NS_DT2(NULL, cptr, DM_L1, MM_SMTP, "Received response for 'RCPT TO:' command.");
    smtp_process_rcpt(cptr, now, buf, bytes_read);
    break;
  case ST_SMTP_RCPT_CC:
    NS_DT2(NULL, cptr, DM_L1, MM_SMTP, "Received response for Cc 'RCPT TO:' command.");
    smtp_process_rcpt_cc(cptr, now, buf, bytes_read);
    break;
  case ST_SMTP_RCPT_BCC:
    NS_DT2(NULL, cptr, DM_L1, MM_SMTP, "Received response for Bcc 'RCPT TO:' command.");
    smtp_process_rcpt_bcc(cptr, now, buf, bytes_read);
    break;
  case ST_SMTP_DATA:
    NS_DT2(NULL, cptr, DM_L1, MM_SMTP, "Received response for 'DATA' command.");
    smtp_process_data(cptr, now, buf, bytes_read);
    break;
  case ST_SMTP_DATA_BODY:
    NS_DT2(NULL, cptr, DM_L1, MM_SMTP, "Received response for 'DATA' command on body completion.");
    smtp_process_data_body(cptr, now, buf, bytes_read);
    break;
/*   case ST_SMTP_RSET: */
/*     smtp_process_rset(cptr, now, buf, bytes_read); */
/*     break; */
  case ST_SMTP_QUIT:
    NS_DT2(NULL, cptr, DM_L1, MM_SMTP, "Received response for 'QUIT' command.");
    smtp_process_quit(cptr, now, buf, bytes_read);
    break;
  default:
    break;
  }
  return 0;
}
