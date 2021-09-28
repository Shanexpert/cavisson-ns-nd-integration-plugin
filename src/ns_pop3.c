/**
 * File: ns_pop3.c
 * Purpose: POP3 processing and state machine functions
 * Author: Bhavpreet
 * 
 */

#include <stdio.h>
#include <string.h>
#include <regex.h>
#include <stdlib.h>

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
#include "ns_pop3.h"
#include "ns_common.h"
#include "ns_pop3_parse.h"
#include "ns_pop3_send.h"
#include "ns_vars.h"
#include "ns_url_resp.h"
#include "ns_auto_fetch_embd.h"
#include "ns_alloc.h"
#include "nslb_util.h"
#include "ns_http_process_resp.h"
#include "ns_log_req_rep.h"
#include "ns_group_data.h"
#include "ns_exit.h"
#include "ns_ftp.h"
#include "nslb_cav_conf.h"

int do_not_delete = 0;

char g_pop3_st_str[][0xff] = {  /* The length of each string can not be >= 0xff */
  "POP3_INITIALIZATION",
  "POP3_CONNECTED",
  "POP3_USER",
  "POP3_PASS",
  "POP3_STAT",
  "POP3_LIST",
  "POP3_RETR",
  "POP3_DELE",
  "POP3_QUIT"
};

void pop3_timeout_handle( ClientData client_data, u_ns_ts_t now ) {

  connection* cptr;
  cptr = (connection *)client_data.p;
  VUser* vptr = cptr->vptr;
  
  NSDL2_POP3(vptr, cptr, "Method Called, vptr=%p cptr=%p conn state=%d", vptr, cptr, cptr->conn_state);

  if(vptr->vuser_state == NS_VUSER_PAUSED){
    vptr->flags |= NS_VPTR_FLAGS_TIMER_PENDING;
    return;
  }

  Close_connection( cptr , 0, now, NS_REQUEST_TIMEOUT, NS_COMPLETION_TIMEOUT);
}

void delete_pop3_timeout_timer(connection *cptr) {

  NSDL2_POP3(NULL, cptr, "Method called, timer type = %d", cptr->timer_ptr->timer_type);

  if ( cptr->timer_ptr->timer_type == AB_TIMEOUT_IDLE ) {
    NSDL2_POP3(NULL, cptr, "Deleting Idle timer.");
    dis_timer_del(cptr->timer_ptr);
  }
}

/* This function converts the int state to STR */
char *pop3_state_to_str(int state)
{
  /* TODO bounds checking */
  return g_pop3_st_str[state];
}

/* Function calls retry_connection() which will ensure normal http like retries for pop3 also. */
static inline void
handle_pop3_bad_read (connection *cptr, u_ns_ts_t now)
{
  NSDL2_POP3(NULL, cptr, "cptr=%p conn state=%d, now = %u", cptr, cptr->conn_state, now);
 
  retry_connection(cptr, now, NS_REQUEST_BAD_HDR);
}

#define DELTA_POP3_SCAN_LIST_ENTRIES 20

//This method to create table entry
//On success row num contains the newly created row-index of table
static int create_pop3_scan_list_table_entry(int *row_num, int *total, int *max, POP3_scan_listing **ptr)
{
  int size = sizeof(POP3_scan_listing);

  NSDL2_MON(NULL, NULL, "Method called");
  if (*total == *max)
  {
    MY_REALLOC_EX(*ptr, (*max + DELTA_POP3_SCAN_LIST_ENTRIES) * size, (*max) * size,__FUNCTION__, -1);
    *max += DELTA_POP3_SCAN_LIST_ENTRIES;
  }
  *row_num = (*total)++;
  //if(global_settings->debug) 
    NSDL(NULL, NULL, DM_EXECUTION, MM_MISC, "row_num = %d, total = %d, max = %d, ptr = %p, size = %d, name = %s", *row_num, *total, *max, &ptr, size, __FUNCTION__);
  return 0;
}

/* Following functions are used to manage scan listing resulting from POP3 LIST cmd */
#define POP3_SLST_NEW    1
#define POP3_SLST_IDX    2
#define POP3_SLST_SPACE  3
#define POP3_SLST_OCT    4
#define POP3_SLST_TXT    5
#define POP3_SLST_CR     6
#define POP3_SLST_CRLF   7

void pop3_save_scan_listing(connection *cptr, char *buf, int buf_len)
{
  int rnum = 0;
  char *ptr = NULL;
  //int len = 0;
  int state = POP3_SLST_NEW;
  int icount = 0;
  char local_oct[0xff];
  int oct_count = 0;
  
  /* Example response in buf will be:
     +OK 3 message (4914 octets)\r\n
     1 1920\r\n
     2 2453\r\n
     3 541\r\n
     .\r\n

   */
  ptr = strstr(buf, "\r\n");

  if (ptr == NULL) {       /* Something is wrong */
    fprintf(stderr, "%s: pop3 invalid response\n", __FUNCTION__);
    return;
  }
  ptr += 2; /* + \r\n */
  //len = buf_len - (ptr - buf);
  
  /* Assuming null terminated string */
  while (ptr[0]) {
    if (ptr[0] == '.') /* Finished processing; we dont check for remaining \r\n */
      return;

    switch(state) {
    case POP3_SLST_NEW:
      create_pop3_scan_list_table_entry(&rnum, &(cptr->total_pop3_scan_list), 
                                        &(cptr->max_pop3_scan_list), 
                                        &(cptr->pop3_scan_list_head));
      state = POP3_SLST_IDX;    /* no need to set */
      /* Fallthrough */
    case POP3_SLST_IDX:
      if (ptr[0] == ' ') {
        state = POP3_SLST_SPACE;
        cptr->pop3_scan_list_head[rnum].maildrop_idx[icount] = 0;
        icount = 0;             /* reset */
      } else { /* copy */
        NSDL3_POP3(NULL, cptr, "rnum = %d", rnum);
        cptr->pop3_scan_list_head[rnum].maildrop_idx[icount] = ptr[0];
        icount++; /* TODO bounds check; 
                   * right now assuming it should not be more than 9 digits. */
      }
      
      break;

    case POP3_SLST_SPACE:
      if (ptr[0] != ' ') {
        state = POP3_SLST_OCT;
        oct_count = 0;
        NSDL3_POP3(NULL, cptr, "oct_count = %d, ptr[0] = %c\n", oct_count, ptr[0]);
        local_oct[oct_count] = ptr[0];
        oct_count++; /* TODO bounds check; 
                      * right now assuming it should not be more than 0xff digits. */
      } /* else remain in same state */
      break;

    case POP3_SLST_OCT:
      NSDL3_POP3(NULL, cptr, "oct_count = %d, ptr[0] = %c\n", oct_count, ptr[0]);
      if (ptr[0] == ' ') {
        state = POP3_SLST_TXT;  /* There might be some extra text */
        local_oct[oct_count] = 0;
        oct_count = 0;             /* reset */
        cptr->pop3_scan_list_head[rnum].octets = atoi((const char *)local_oct);
        NSDL3_POP3(NULL, cptr, "rnum = %d, octets extacted = %d, local_oct = %s\n", rnum, 
                   cptr->pop3_scan_list_head[rnum].octets, local_oct);
      } else if (ptr[0] == '\r') {
        state = POP3_SLST_CR;
        local_oct[oct_count] = 0;
        oct_count = 0;             /* reset */
        cptr->pop3_scan_list_head[rnum].octets = atoi((const char *)local_oct);
        NSDL3_POP3(NULL, cptr, "rnum = %d, octets extacted = %d, local_oct = %s\n", rnum, 
                   cptr->pop3_scan_list_head[rnum].octets, local_oct);
      } else { /* copy */
        local_oct[oct_count] = ptr[0];
        oct_count++; /* TODO bounds check; 
                      * right now assuming it should not be more than 0xff digits. */
      }
      break;
    case POP3_SLST_TXT:
      if (ptr[0] == '\r') {
        state = POP3_SLST_CR;
      }
      break;
    case POP3_SLST_CR:
      if (ptr[0] == '\n') {
        state = POP3_SLST_NEW;
      } else /* switch back to TXT state */
        state = POP3_SLST_TXT;
      break;
    }
    
    //printf("XXXXXXXXX state = %d [%c]\n", state, ptr[0]);
    ptr++;
    //printf("XXXXXXXXX new ptr[0] = [%c]\n", ptr[0]);
  }

#ifdef NS_DEBUG_ON
  {
    int i;
    for (i = 0; i < cptr->total_pop3_scan_list; i++) {
      NSDL3_POP3(NULL, cptr, "idx = %s, octets = %d\n", 
                 cptr->pop3_scan_list_head[i].maildrop_idx,
                 cptr->pop3_scan_list_head[i].octets);
    }
  }
#endif  /*  NS_DEBUG_ON */
}

void pop3_free_scan_listing(connection *cptr)
{
  int i;
  for (i = 0; i < cptr->total_pop3_scan_list; i++) {
    NSDL3_POP3(NULL, cptr, "Freeing idx = %s, octets = %d\n", 
               cptr->pop3_scan_list_head[i].maildrop_idx,
               cptr->pop3_scan_list_head[i].octets);
  }

  FREE_AND_MAKE_NULL_EX(cptr->pop3_scan_list_head, sizeof(POP3_scan_listing), __FUNCTION__, -1);
  cptr->total_pop3_scan_list = 0;
  cptr->max_pop3_scan_list = 0;
  cptr->cur_pop3_in_scan_list = 0;
}

int pop3_msg_left_in_scan_listing(connection *cptr)
{
  if (cptr->cur_pop3_in_scan_list == cptr->total_pop3_scan_list)
    return 0;
  else
    return 1;
}

char *pop3_get_next_from_scan_listing(connection *cptr)
{
  int cur = cptr->cur_pop3_in_scan_list;
  cptr->cur_pop3_in_scan_list++;
  return cptr->pop3_scan_list_head[cur].maildrop_idx;
}

char *pop3_get_last_fetched_from_scan_listing(connection *cptr)
{
  int cur = cptr->cur_pop3_in_scan_list - 1;
  return cptr->pop3_scan_list_head[cur].maildrop_idx;
}

void pop3_process_handshake(connection *cptr, u_ns_ts_t now, char *buf, int bytes_read)
{
  int req_code   = cptr->req_code;
  
  NSDL2_POP3(NULL, cptr, "conn state=%d, req_code = %s, buf = %s, bytes_read = %d, now = %u",
             cptr->conn_state, 
             cptr->req_code == POP3_ERR ? "-ERR" : "+OK", 
             buf, bytes_read, now);

  if (req_code == POP3_OK){
    if(cptr->url_num->proto.pop3.authentication_type)
      pop3_send_stls(cptr, now);
    else
      pop3_send_user(cptr, now);
  }
  else 
    Close_connection(cptr, 0, now, NS_REQUEST_BAD_RESP, NS_COMPLETION_POP3_ERROR);
    
}

void pop3_process_user(connection *cptr, u_ns_ts_t now, char *buf, int bytes_read)
{
  int req_code   = cptr->req_code;
  
  NSDL2_POP3(NULL, cptr, "conn state=%d, req_code = %s, buf = %s, bytes_read = %d, now = %u",
             cptr->conn_state, 
             cptr->req_code == POP3_ERR ? "-ERR" : "+OK", 
             buf, bytes_read, now);

  if (req_code == POP3_OK) {
    pop3_send_pass(cptr, now);
  } else if (req_code == POP3_ERR) {
    cptr->req_ok = NS_REQUEST_AUTH_FAIL;
    pop3_send_quit(cptr, now);
  }
}

void pop3_process_stls(connection *cptr, u_ns_ts_t now, char *buf, int bytes_read)
{
  int req_code   = cptr->req_code;
  
  NSDL2_POP3(NULL, cptr, "conn state=%d, req_code = %s, buf = %s, bytes_read = %d, now = %u",
             cptr->conn_state, 
             cptr->req_code == POP3_ERR ? "-ERR" : "+OK", 
             buf, bytes_read, now);

  if (req_code == POP3_OK) {
    cptr->request_type = SPOP3_REQUEST;
    cptr->proto_state = ST_POP3_STLS_LOGIN;
    cptr->conn_state = CNST_CONNECTING;
    
    handle_connect(cptr, now, 0);
  } 
  else {
    Close_connection(cptr, 0, now, NS_REQUEST_BAD_RESP, NS_COMPLETION_IMAP_ERROR);
  } 

}

void pop3_process_pass(connection *cptr, u_ns_ts_t now, char *buf, int bytes_read)
{
  int req_code   = cptr->req_code;
  
  NSDL2_POP3(NULL, cptr, "conn state=%d, req_code = %s, buf = %s, bytes_read = %d, now = %u",
             cptr->conn_state, 
             cptr->req_code == POP3_ERR ? "-ERR" : "+OK", 
             buf, bytes_read, now);

  if (req_code == POP3_OK) {
    switch (cptr->url_num->proto.pop3.pop3_action_type) {
    case POP3_ACTION_STAT:
      pop3_send_stat(cptr, now);
      break;
    case POP3_ACTION_LIST:
    case POP3_ACTION_GET:
      pop3_send_list(cptr, now);
      break;
    }
  } else if (req_code == POP3_ERR) {
    cptr->req_ok = NS_REQUEST_AUTH_FAIL;
    pop3_send_quit(cptr, now);
  }
}

void pop3_process_stat(connection *cptr, u_ns_ts_t now, char *buf, int bytes_read)
{
  int req_code   = cptr->req_code;
  VUser *vptr = (VUser *)cptr->vptr;
  
  NSDL2_POP3(NULL, cptr, "conn state=%d, req_code = %s, buf = %s, bytes_read = %d, now = %u",
             cptr->conn_state, 
             cptr->req_code == POP3_ERR ? "-ERR" : "+OK", 
             buf, bytes_read, now);

  if (req_code == POP3_OK) {

    copy_url_resp(cptr);

    /* we have to copy it here since we are going to use do_data_processing
     * in STAT/RETR/LIST cases in POP3 */
    copy_cptr_to_vptr(vptr, cptr); // Save buffers from cptr to vptr etc 

    /* Process vars on it. (checkpoint and others) */
    do_data_processing(cptr, now, VAR_IGNORE_REDIRECTION_DEPTH, 1);
    cptr->bytes = vptr->bytes = 0;
    url_resp_buff[0] = 0;
    
    NSDL2_POP3(NULL, cptr, "Going to send quit, cptr->bytes = %d, vptr->bytes = %d\n", 
               cptr->bytes, vptr->bytes);

    pop3_send_quit(cptr, now);
  } else if (req_code == POP3_ERR) {
    cptr->req_ok = NS_REQUEST_BAD_RESP;
    pop3_send_quit(cptr, now);
  }
}

void pop3_process_list(connection *cptr, u_ns_ts_t now, char *buf, int bytes_read)
{
  int req_code   = cptr->req_code;
  VUser *vptr = (VUser *)cptr->vptr;
  int blen = cptr->bytes;
  char *full_buffer;
  
  NSDL2_POP3(NULL, cptr, "conn state=%d, req_code = %s, buf = %s, bytes_read = %d, now = %u",
             cptr->conn_state, 
             cptr->req_code == POP3_ERR ? "-ERR" : "+OK", 
             buf, bytes_read, now);


  if (req_code == POP3_OK) {

    copy_url_resp(cptr);

    /* we have to copy it here since we are going to use do_data_processing
     * in STAT/RETR/LIST cases in POP3 */
    copy_cptr_to_vptr(vptr, cptr); // Save buffers from cptr to vptr etc 

    switch (cptr->url_num->proto.pop3.pop3_action_type) {
    case POP3_ACTION_LIST:

      do_data_processing(cptr, now, VAR_IGNORE_REDIRECTION_DEPTH, 1);

      RESET_URL_RESP_AND_CPTR_VPTR_BYTES;
    
      NSDL2_POP3(NULL, cptr, "Going to send quit, cptr->bytes = %d, vptr->bytes = %d\n", 
                 cptr->bytes, vptr->bytes);

      pop3_send_quit(cptr, now);
      break;
    case POP3_ACTION_GET:
      
      full_buffer = get_reply_buffer(cptr, &blen, 0, 1);

      /* store "scan listing" for messages */
      pop3_save_scan_listing(cptr, full_buffer, blen);

      RESET_URL_RESP_AND_CPTR_VPTR_BYTES;

      if (pop3_msg_left_in_scan_listing(cptr)) { /* means there are more to be retrieved. */
        pop3_send_retr(cptr, now);
      } else /* QUIT: nothing to retrieve */
        pop3_send_quit(cptr, now);
      break;
    }
  } else if (req_code == POP3_ERR) {
    cptr->req_ok = NS_REQUEST_BAD_RESP;
    pop3_send_quit(cptr, now);
  }
}

void pop3_process_retr(connection *cptr, u_ns_ts_t now, char *buf, int bytes_read)
{
  int req_code   = cptr->req_code;
  VUser *vptr = (VUser *)cptr->vptr;
  
  NSDL2_POP3(NULL, cptr, "conn state=%d, req_code = %s, buf = %s, bytes_read = %d, now = %u",
             cptr->conn_state, 
             cptr->req_code == POP3_ERR ? "-ERR" : "+OK", 
             buf, bytes_read, now);

  if (req_code == POP3_OK) {

    copy_url_resp(cptr);

    /* we have to copy it here since we are going to use do_data_processing
     * in STAT/RETR/LIST cases in POP3 */
    copy_cptr_to_vptr(vptr, cptr); // Save buffers from cptr to vptr etc 

    /* do vars processing on each mail retrieved. */
    do_data_processing(cptr, now, VAR_IGNORE_REDIRECTION_DEPTH, 1);
    cptr->bytes = vptr->bytes = 0;
    url_resp_buff[0] = 0;

    NSDL2_POP3(NULL, cptr, "Going to send dele, cptr->bytes = %d, vptr->bytes = %d\n", 
               cptr->bytes, vptr->bytes);

    /* Delete the retrieved msg on maildrop */
    if (do_not_delete) {
      if (pop3_msg_left_in_scan_listing(cptr)) { /* means there are more to be retrieved. */
        pop3_send_retr(cptr, now);
      } else
        pop3_send_quit(cptr, now);
    } else {
      pop3_send_dele(cptr, now);
    }

  } else if (req_code == POP3_ERR) {
    cptr->req_ok = NS_REQUEST_BAD_RESP;
    pop3_send_quit(cptr, now);
  }
}

void pop3_process_dele(connection *cptr, u_ns_ts_t now, char *buf, int bytes_read)
{
  int req_code   = cptr->req_code;
  
  NSDL2_POP3(NULL, cptr, "conn state=%d, req_code = %s, buf = %s, bytes_read = %d, now = %u",
             cptr->conn_state, 
             cptr->req_code == POP3_ERR ? "-ERR" : "+OK", 
             buf, bytes_read, now);

  if (req_code == POP3_OK) {
    if (pop3_msg_left_in_scan_listing(cptr)) { /* means there are more to be retrieved. */
      pop3_send_retr(cptr, now);
    } else
      pop3_send_quit(cptr, now);
  } else if (req_code == POP3_ERR) {
    cptr->req_ok = NS_REQUEST_BAD_RESP;
    pop3_send_quit(cptr, now);
  }
}

void pop3_process_quit(connection *cptr, u_ns_ts_t now, char *buf, int bytes_read)
{
  int status   = cptr->req_ok;
  
  NSDL2_POP3(NULL, cptr, "conn state=%d, req_code = %s, buf = %s, bytes_read = %d, now = %u",
             cptr->conn_state, 
             cptr->req_code == POP3_ERR ? "-ERR" : "+OK", 
             buf, bytes_read, now);
    
  // Confail means req_ok is not filled by any error earliar, so its a success case
  if(status == NS_REQUEST_CONFAIL)
    status = NS_REQUEST_OK;

  Close_connection(cptr, 0, now, status, NS_COMPLETION_CLOSE);
}

void debug_log_pop3_res(connection *cptr, char *buf, int size)
{
  VUser *vptr;
  vptr = (VUser *)cptr->vptr;
  int request_type = cptr->url_num->request_type;

  if (!((runprof_table_shr_mem[vptr->group_num].gset.debug & DM_LOGIC4) && 
        ((request_type == POP3_REQUEST) || (request_type == SPOP3_REQUEST)) &&
         (runprof_table_shr_mem[vptr->group_num].gset.module_mask & MM_POP3)))
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
   sprintf(log_file, "%s/logs/%s/pop3_session_%hd_%u_%u_%d_0_%d_%d_%d_0.dat",
          g_ns_wdir, req_rep_file_path, child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,
          vptr->group_num, GET_SESS_ID_BY_NAME(vptr),
          GET_PAGE_ID_BY_NAME(vptr));
    
    // Do not change the debug trace message as it is parsed by GUI
    if(cptr->tcp_bytes_recv == 0)  //This check to print debug trace only once
      NS_DT4(vptr, cptr, DM_L1, MM_POP3, "Response is in file '%s'", log_file);

    if((log_fd = open(log_file, O_CREAT|O_CLOEXEC|O_WRONLY|O_APPEND, 00666)) < 0)
        fprintf(stderr, "Error: Error in opening file for logging URL request\n");
    else
    {
      write(log_fd, buf, size);
      close(log_fd);
    }
  }
}

/**
 * function handle_pop3_read() is the main function handling pop3 protocol
 * All state changes take place in this function. 
 * Most of the code is borrowed from http's handle_read()
 *
 */
int
handle_pop3_read( connection *cptr, u_ns_ts_t now )
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
  int err = 0;
  char *err_buff = NULL;
  //char err_msg[65545 + 1];

  NSDL2_POP3(NULL, cptr, "conn state=%d, now = %u", cptr->conn_state, now);

  request_type = cptr->request_type;
  if (request_type != POP3_REQUEST && request_type != SPOP3_REQUEST) {
    /* Something is very wrong we should not be here. */
    NS_EXIT(-1, "Request type is not (s)pop3 but still we are in an pop3 state.");
  }

  cptr->body_offset = 0;     /* Offset will always be from 0; used in get_reply_buf */

  while (1) {
    // printf ("0 Req code=%d\n", cptr->req_code);
    if ( do_throttle )
      bytes_read = THROTTLE / 2;
    else
      bytes_read = sizeof(buf) - 1;

#ifdef ENABLE_SSL
  if (request_type == SPOP3_REQUEST) {
    bytes_read = SSL_read(cptr->ssl, buf, bytes_read);
    
        if (bytes_read > 0) buf[bytes_read] = '\0'; // NULL terminate for printing/logging
        NSDL3_SSL(NULL, cptr, "Read %d bytes. msg=%s", bytes_read, (bytes_read>0)?buf:"-");

    if (bytes_read <= 0) {
      err = SSL_get_error(cptr->ssl, bytes_read);
      switch (err) {
      case SSL_ERROR_ZERO_RETURN:  /* means that the connection closed from the server */
	handle_pop3_bad_read (cptr, now);
	return -1;
      case SSL_ERROR_WANT_READ:
	return -1;
	/* It can but isn't supposed to happen */
      case SSL_ERROR_WANT_WRITE:
	fprintf(stderr, "SSL_read error: SSL_ERROR_WANT_WRITE\n");
	handle_pop3_bad_read (cptr, now);
	return -1;
      case SSL_ERROR_SYSCALL: //Some I/O error occurred
        if (errno == EAGAIN) // no more data available, return (it is like SSL_ERROR_WANT_READ)
        {
          NSDL1_SSL(NULL, cptr, "POP3 SSL_read: No more data available, return");
          handle_pop3_bad_read (cptr, now);
          return -1;
        }

        if (errno == EINTR)
        {
          NSDL3_SSL(NULL, cptr, "POP3 SSL_read interrupted. Continuing...");
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
	    handle_pop3_bad_read (cptr, now);
	else
          handle_pop3_bad_read (cptr, now);
	return -1;
      }
    }
  } else {
    bytes_read = read(cptr->conn_fd, buf, bytes_read);
    NSDL2_POP3(NULL, cptr, "bytes_read = %d, buf = %*.*s\n", bytes_read, bytes_read, bytes_read, buf);
#ifdef NS_DEBUG_ON
    if (bytes_read > 0) buf[bytes_read] = '\0'; // NULL terminate for printing/logging
    NSDL3_POP3(NULL, cptr, "Read %d bytes. msg=%s", bytes_read, bytes_read>0?buf:"-");
#endif

    if ( bytes_read < 0 ) {
      if (errno == EAGAIN) {
#ifndef USE_EPOLL
        FD_SET( cptr->conn_fd, &g_rfdset );
#endif
        return 1;
      } else {
        NSDL3_POP3(NULL, cptr, "read failed (%s) for main: host = %s [%d]", nslb_strerror(errno),
                   cptr->url_num->index.svr_ptr->server_hostname,
                   cptr->url_num->index.svr_ptr->server_port);
        handle_pop3_bad_read (cptr, now);
        return -1;
      }
    } else if (bytes_read == 0) {
      handle_pop3_bad_read (cptr, now);
      return -1;
    }
  }
#else
 
    bytes_read = read(cptr->conn_fd, buf, bytes_read);
    NSDL2_POP3(NULL, cptr, "bytes_read = %d, buf = %*.*s\n", bytes_read, bytes_read, bytes_read, buf);

#ifdef NS_DEBUG_ON
    if (bytes_read > 0) buf[bytes_read] = '\0'; // NULL terminate for printing/logging
      NSDL3_POP3(NULL, cptr, "Read %d bytes. msg=%s", bytes_read, bytes_read>0?buf:"-");
#endif

    if ( bytes_read < 0 ) {
      if (errno == EAGAIN) {
#ifndef USE_EPOLL
        FD_SET( cptr->conn_fd, &g_rfdset );
#endif
        return 1;
      } else {
        NSDL3_POP3(NULL, cptr, "read failed (%s) for main: host = %s [%d]", nslb_strerror(errno),
                   cptr->url_num->index.svr_ptr->server_hostname,
                   cptr->url_num->index.svr_ptr->server_port);
        handle_pop3_bad_read (cptr, now);
        return -1;
      }
    } else if (bytes_read == 0) {
      handle_pop3_bad_read (cptr, now);
      return -1;
    }
#endif
    bytes_handled = 0;

    /* Put checks for no validation ?? TODO:BHAV:XXXXXXXXX ??? */
    /* we copy the data to be parsed in future. */
    if (cptr->proto_state == ST_POP3_STAT ||
        cptr->proto_state == ST_POP3_LIST ||
        cptr->proto_state == ST_POP3_RETR) {
      copy_retrieve_data(cptr, buf, bytes_read, cptr->bytes);
      cptr->bytes += bytes_read;
    }
  
    /* POP3 response parsing */
    for( ; bytes_handled < bytes_read; bytes_handled++) {
      
      if (cptr->header_state == POP3_HDST_END) {
         /* We have read the end and still there are some bytes in the buffer */
        NSDL1_POP3(NULL, NULL, "Extra bytes in buffer");
        break;                  /* break from for */
      }
      
      switch ( cptr->header_state ) {
      case POP3_HDST_NEW:
        if (buf[bytes_handled] == '+')
          cptr->header_state = POP3_HDST_PLUS;
        else if (buf[bytes_handled] == '-')
          cptr->header_state = POP3_HDST_MINUS;
        else { /* Bad state exit? */
          //NSEL_MAJ(NULL, cptr, "Invalid POP3 header state (%d)", cptr->header_state);
        }

        break;
      case POP3_HDST_PLUS:
        if (buf[bytes_handled] == 'O')
          cptr->header_state = POP3_HDST_PLUS_O;
        /* ELSE error TODO */

        break;

      case POP3_HDST_PLUS_O:
        if (buf[bytes_handled] == 'K') {
          cptr->req_code = POP3_OK;
          cptr->header_state = POP3_HDST_TEXT;
        } 
        /* ELSE error TODO */

        break;

      case POP3_HDST_MINUS:
        if (buf[bytes_handled] == 'E')
          cptr->header_state = POP3_HDST_MINUS_E;

        break;

      case POP3_HDST_MINUS_E:
        if (buf[bytes_handled] == 'R') {
          cptr->header_state = POP3_HDST_MINUS_ER;
        }
        break;

      case POP3_HDST_MINUS_ER:
        if (buf[bytes_handled] == 'R') {
          cptr->req_code = POP3_ERR;
          cptr->header_state = POP3_HDST_TEXT;
        }
        break;

      case POP3_HDST_TEXT:
        if (buf[bytes_handled] == '\r')
          cptr->header_state = POP3_HDST_TEXT_CR;
        /* else state remains the same */
        break;

      case POP3_HDST_TEXT_CR:
        if (buf[bytes_handled] == '\n') {
          if ((cptr->req_code != POP3_ERR) &&
                   ((cptr->proto_state == ST_POP3_LIST) ||
                    (cptr->proto_state == ST_POP3_RETR))) { /* this case we need to process 
                                                      * further for CRLF.CRLF*/
            cptr->header_state = POP3_HDST_TEXT_CRLF;
          } else
            cptr->header_state = POP3_HDST_END;

        } else if (buf[bytes_handled] == '\r') { /* there might be another \r */
          cptr->header_state = POP3_HDST_TEXT_CR;
        } else /* reset to text state */
          cptr->header_state = POP3_HDST_TEXT;

        break;

      case POP3_HDST_TEXT_CRLF:
        if (buf[bytes_handled] == '.')
          cptr->header_state = POP3_HDST_TEXT_CRLF_DOT;
        else if (buf[bytes_handled] == '\r')  /* there might be another \r */
          cptr->header_state = POP3_HDST_TEXT_CR;
        else /* reset to text state */
          cptr->header_state = POP3_HDST_TEXT;
        break;

      case POP3_HDST_TEXT_CRLF_DOT:
        if (buf[bytes_handled] == '\r')
          cptr->header_state = POP3_HDST_TEXT_CRLF_DOT_CR;
        else /* reset to text state */
          cptr->header_state = POP3_HDST_TEXT;
        break;

      case POP3_HDST_TEXT_CRLF_DOT_CR:
        if (buf[bytes_handled] == '\n') /* End now */
          cptr->header_state = POP3_HDST_END;
        else if (buf[bytes_handled] == '\r') /* there might be another \r\n */
          cptr->header_state = POP3_HDST_TEXT_CR;
        else /* reset to text state */
          cptr->header_state = POP3_HDST_TEXT;
        break;

      default:
        break;
      }
    }
    
    /* Here it will be pop3 specific so we need to add new fields in avg_time */
    cptr->tcp_bytes_recv += bytes_handled;
    INC_POP3_RX_BYTES(vptr, bytes_handled); 
    //cptr->bytes += bytes_handled;
    average_time->pop3_total_bytes += bytes_handled;
    if(SHOW_GRP_DATA)
    {
      avgtime *lol_average_time;
      lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
      lol_average_time->pop3_total_bytes += bytes_handled;
    }
 
#ifdef NS_DEBUG_ON
    debug_log_pop3_res(cptr, buf, bytes_read); /* TODO:BHAV */
#endif

    if (cptr->header_state == POP3_HDST_END) {
      /* Reset the state for next time */
      cptr->header_state = POP3_HDST_NEW;
      break;  /* From while */
    }
  }

  // delete pop3 timeout timers
  delete_pop3_timeout_timer(cptr);

  NSDL2_POP3(NULL, cptr, "conn state=%d, proto_state = %d, now = %u", cptr->conn_state, cptr->proto_state, now);
  
  /* POP3 state machine */
  switch(cptr->proto_state) {
  case ST_POP3_CONNECTED:
    NS_DT2(NULL, cptr, DM_L1, MM_POP3, "Handshake done.");
    //NS_DT2(NULL, cptr, DM_L1, MM_POP3, "Received initial response from POP3 server.");
    pop3_process_handshake(cptr, now, buf, bytes_read);
    break;
  case ST_POP3_STLS:
    NS_DT2(NULL, cptr, DM_L1, MM_POP3, "Received response for 'STLS' command.");
    pop3_process_stls(cptr, now, buf, bytes_read);
    break;
  case ST_POP3_USER:
    NS_DT2(NULL, cptr, DM_L1, MM_POP3, "Received response for 'USER' command.");
    pop3_process_user(cptr, now, buf, bytes_read);
    break;
  case ST_POP3_PASS:
    NS_DT2(NULL, cptr, DM_L1, MM_POP3, "Received response for 'PASS' command.");
    pop3_process_pass(cptr, now, buf, bytes_read);
    break;
  case ST_POP3_STAT:
    NS_DT2(NULL, cptr, DM_L1, MM_POP3, "Received response for 'LIST' command.");
    pop3_process_stat(cptr, now, buf, bytes_read);
    break;
  case ST_POP3_LIST:
    NS_DT2(NULL, cptr, DM_L1, MM_POP3, "Received response for 'LIST' command.");
    pop3_process_list(cptr, now, buf, bytes_read);
    break;
  case ST_POP3_RETR:
    NS_DT2(NULL, cptr, DM_L1, MM_POP3, "Received response for 'RETR' command.");
    pop3_process_retr(cptr, now, buf, bytes_read);
    break;
  case ST_POP3_DELE:
    NS_DT2(NULL, cptr, DM_L1, MM_POP3, "Received response for 'DELE' command.");
    pop3_process_dele(cptr, now, buf, bytes_read);
    break;
/*   case ST_POP3_RSET:  */
/*     pop3_process_rset(cptr, now, buf, bytes_read); */
/*     break; */
  case ST_POP3_QUIT:
    NS_DT2(NULL, cptr, DM_L1, MM_POP3, "Received response for 'QUIT' command.");
    pop3_process_quit(cptr, now, buf, bytes_read);
    break;
  default:
    //NSEL_MAJ(NULL, cptr, "Invalid POP3 protocol state (%d)", cptr->proto_state);
    break;
  }
  return 0;
}
