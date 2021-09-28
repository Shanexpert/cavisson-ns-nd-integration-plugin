#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "url.h"
#include "util.h"
#include "ns_common.h"
#include "ns_global_settings.h"
#include "ns_log.h"
#include "ns_script_parse.h"
#include "ns_http_process_resp.h"
#include "ns_imap.h"
#include "ns_log_req_rep.h"
#include "nslb_util.h" 
#include "ns_group_data.h"
#include "ns_exit.h"
#include "ns_trace_level.h"

/* Function to parse imap apis
  e.g
     ns_imap_list("imap_operation",
              "IMAP_SERVER=websrv.cavisson.com",
              "USER_ID=johnydepp@cavisson.com",
              "PASSWORD=john!",
   );
*/

char g_imap_st_str[][0xff] = {  /* The length of each string can not be >= 0xff */
  "IMAP_INITIALIZATION",
  "IMAP_CONNECTED",
  "IMAP_HANDSHAKE",
  "IMAP_SELECT",
  "IMAP_FETCH",
  "IMAP_LIST",
  "IMAP_STORE",
  "IMAP_SEARCH",
  "IMAP_LOGOUT"
  "IMAP_LOGIN"
  "IMAP_DELETE"
};

#define CHECK_MAND_FIELDS\
  if(!imap_server_flag) {\
    SCRIPT_PARSE_ERROR(NULL, "IMAP_SERVER must be given for IMAP SESSION for page %s!\n", RETRIEVE_BUFFER_DATA(gPageTable[g_cur_page].page_name));\
  }\
  if (!user_flag) {\
    SCRIPT_PARSE_ERROR(NULL, "USER_ID must be given for IMAP for page %s!\n", RETRIEVE_BUFFER_DATA(gPageTable[g_cur_page].page_name));\
  }\
  if (!passwd_flag){\
    SCRIPT_PARSE_ERROR(NULL, "PASSWORD must be given for IMAP for page %s!\n", RETRIEVE_BUFFER_DATA(gPageTable[g_cur_page].page_name));\
  }\
  if(imap_action_type == IMAP_FETCH && !mail_seq_flag){\
    SCRIPT_PARSE_ERROR(NULL, "MAIL_SEQ must be given for IMAP for page %s!\n", RETRIEVE_BUFFER_DATA(gPageTable[g_cur_page].page_name));\
  }\
  if(imap_action_type == IMAP_FETCH && !fetch_part_flag){\
    SCRIPT_PARSE_ERROR(NULL, "FETCH must be given for IMAP for page %s!\n", RETRIEVE_BUFFER_DATA(gPageTable[g_cur_page].page_name));\
  }\
  if(((imap_action_type != IMAP_FETCH) && (imap_action_type != IMAP_SEARCH)) && mail_seq_flag){\
    SCRIPT_PARSE_ERROR(NULL, "MAIL_SEQ/FETCH can be given for only for FETCH IMAP command for page %s!\n", RETRIEVE_BUFFER_DATA(gPageTable[g_cur_page].page_name));\
  }\
  if(imap_action_type != IMAP_FETCH && fetch_part_flag){\
    SCRIPT_PARSE_ERROR(NULL, "FETCH can be given for only for FETCH IMAP command for page %s!\n", RETRIEVE_BUFFER_DATA(gPageTable[g_cur_page].page_name));\
  }

/* This function converts the int state to STR */
char *imap_state_to_str(int state)
{
  /* TODO bounds checking */
  return g_imap_st_str[state];
}

int kw_set_imap_timeout(char *buf, int *to_change, char *err_msg)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char grp[MAX_DATA_LINE_LENGTH];
  int num_value;
  int num_args;

  num_args = sscanf(buf, "%s %s %d", keyword, grp, &num_value);

  if(num_args != 3) {
    fprintf(stderr, "Two arguments expected for %s\n", keyword);
    return 1;
  }

  if (num_value <= 0) {
    fprintf(stderr, "Keyword (%s) value must be greater than 0.\n", keyword);
    return 1;
  }

  *to_change = num_value;
  return 0;
}

int ns_parse_imap(char *api_name, char *api_to_run, FILE *flow_fp, FILE *outfp, char *flow_filename, char *flowout_filename, int sess_idx, 
               int imap_action_type) 
{
  int url_idx;
  int imap_server_flag, user_flag, passwd_flag, mail_seq_flag, fetch_part_flag, starttls_flag;
  char *start_quotes;
  char *close_quotes;
  char pagename[MAX_LINE_LENGTH + 1];
  char attribute_name[128 + 1];
  char attribute_value[MAX_LINE_LENGTH + 1];
  int ret;
  char *page_end_ptr;

  NSDL2_IMAP(NULL, NULL, "Method Called. File: %s", flow_filename);

  imap_server_flag = user_flag = passwd_flag = mail_seq_flag = fetch_part_flag = starttls_flag = 0;

  NSDL2_SCHEDULE(NULL, NULL, "file:%s", flow_filename);

  create_requests_table_entry(&url_idx); // Fill request type inside create table entry

  //proto_based_init(url_idx, IMAP_REQUEST);

  NSDL2_IMAP(NULL, NULL, "url_idx = %d, total_request_entries = %d, total_imap_request_entries = %d",
                          url_idx, total_request_entries, total_imap_request_entries);

  //We will be checking for (" & white spaces in-between in extract_pagename 
  ret = extract_pagename(flow_fp, flow_filename, script_line, pagename, &page_end_ptr);
  if(ret == NS_PARSE_SCRIPT_ERROR) return NS_PARSE_SCRIPT_ERROR;

  // For IMAP, we are internally using ns_web_url API
    if((parse_and_set_pagename(api_name, api_to_run, flow_fp, flow_filename, script_line, outfp, flowout_filename, sess_idx, &page_end_ptr, pagename)) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;

  gPageTable[g_cur_page].first_eurl = url_idx;
  gPageTable[g_cur_page].num_eurls++; // Increment urls

  close_quotes = page_end_ptr;
  start_quotes = NULL;

  // Point to next argument
  ret = read_till_start_of_next_quotes(flow_fp, flow_filename, close_quotes, &start_quotes, 0, outfp);
  //This will return if start quotes of next argument is not found or some other printable
  //is found including );
  if(ret == NS_PARSE_SCRIPT_ERROR)
  {  
    SCRIPT_PARSE_ERROR(script_line, "Syntax error");
    return NS_PARSE_SCRIPT_ERROR;
  }
 
  while(1) 
  {
    NSDL3_IMAP(NULL, NULL, "line = %s", script_line);
    ret = get_next_argument(flow_fp, start_quotes, attribute_name, attribute_value, &close_quotes, 0);
    if(ret == NS_PARSE_SCRIPT_ERROR) return NS_PARSE_SCRIPT_ERROR;
    
    if (!strcmp(attribute_name, "IMAP_SERVER") || !strcmp(attribute_name, "IMAPS_SERVER")) // Parametrization is not allowed for this argument
    {
      if (!strcmp(attribute_name, "IMAP_SERVER")){
        proto_based_init(url_idx, IMAP_REQUEST);
      }else{
        proto_based_init(url_idx, IMAPS_REQUEST);
      }
      if(imap_server_flag)
      { 
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012218_ID, CAV_ERR_1012218_MSG, "IMAP(S) Server");
      }
      requests[url_idx].index.svr_idx = get_server_idx(attribute_value, requests[url_idx].request_type, script_ln_no);
      imap_server_flag = 1;
      NSDL2_IMAP(NULL, NULL,"Value of  %s = %s, svr_idx = %d", attribute_value, attribute_name, requests[url_idx].index.svr_idx);
      /*Added for filling all server in gSessionTable*/
      CREATE_AND_FILL_SESS_HOST_TABLE_ENTRY(url_idx, "Method called from ns_parse_imap");

    } else if (!strcmp(attribute_name, "STARTTLS")){ // Parametrization is  not allowed for this argument
      if(starttls_flag){
         SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012218_ID, CAV_ERR_1012218_MSG, "STARTTLS");
      }
      if(!strcasecmp(attribute_value, "YES")){
        requests[url_idx].proto.imap.authentication_type = 1;
      }else if(!strcasecmp(attribute_value, "NO")){
        requests[url_idx].proto.imap.authentication_type = 0;
      }else{
         SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012220_ID, CAV_ERR_1012220_MSG, attribute_value, "STARTTLS", "YES", "NO");
      }
      starttls_flag = 1;
      NSDL2_IMAP(NULL, NULL,"Value of  %s = %s", attribute_value, attribute_name);
    } else if (!strcmp(attribute_name, "USER_ID")){ // Parametrization is allowed for this argument
      if(user_flag){
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012218_ID, CAV_ERR_1012218_MSG, "USER_ID");
      }
      segment_line(&(requests[url_idx].proto.imap.user_id), attribute_value, 0, script_ln_no, sess_idx, flow_filename);
      user_flag = 1;
      NSDL2_IMAP(NULL, NULL,"Value of  %s = %s, svr_idx = %d", attribute_value, attribute_name, requests[url_idx].proto.imap.user_id);
    } 
    else if (!strcmp(attribute_name, "PASSWORD")){ // Parametrization is allowed for this argument
      if(passwd_flag){
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012218_ID, CAV_ERR_1012218_MSG, "PASSWORD");
      }
      segment_line(&(requests[url_idx].proto.imap.passwd), attribute_value, 0, script_ln_no, sess_idx, flow_filename);
      passwd_flag = 1;
      NSDL2_IMAP(NULL, NULL,"Value of  %s = %s, svr_idx = %d", attribute_value, attribute_name, requests[url_idx].proto.imap.passwd);
    }else if(!strcmp(attribute_name, "MAIL_SEQ")){
      if(mail_seq_flag){
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012218_ID, CAV_ERR_1012218_MSG, "MAIL_SEQ");
      }
      segment_line(&(requests[url_idx].proto.imap.mail_seq), attribute_value, 0, script_ln_no, sess_idx, flow_filename);
      mail_seq_flag = 1;
      NSDL2_IMAP(NULL, NULL,"Value of  %s = %s, svr_idx = %d", attribute_value, attribute_name, requests[url_idx].proto.imap.mail_seq);
    }else if(!strcmp(attribute_name, "FETCH")){
      if(fetch_part_flag){
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012218_ID, CAV_ERR_1012218_MSG, "FETCH");
      }

      if(!(strcasecmp(attribute_value, "TEXT")) || !(strcasecmp(attribute_value, "HEADER")) || !(strcasecmp(attribute_value, "BODY")) || !(strcasecmp(attribute_value, "ATTACH")) || !(strcasecmp(attribute_value, "FULL"))){
        segment_line(&(requests[url_idx].proto.imap.fetch_part), attribute_value, 0, script_ln_no, sess_idx, flow_filename);
        fetch_part_flag = 1;
        NSDL2_IMAP(NULL, NULL,"Value of  %s = %s, svr_idx = %d", attribute_value, attribute_name, requests[url_idx].proto.imap.fetch_part);
      }else{
         SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012215_ID, CAV_ERR_1012215_MSG, attribute_value, "FETCH");
      }
//TODO: handle for all cases of 'search' command 
    }else{ 
       SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012219_ID, CAV_ERR_1012219_MSG, attribute_name);
    }
    ret = read_till_start_of_next_quotes(flow_fp, flow_filename, close_quotes, &start_quotes, 0, outfp);
    //In case next comma not found between quotes or end_of_file found
    if(ret == NS_PARSE_SCRIPT_ERROR){
      NSDL2_IMAP(NULL, NULL,"Next attribute is not found");
      break;
    }
  }
  if(start_quotes == NULL){
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012209_ID, CAV_ERR_1012209_MSG, "IMAP");
  }  
  else{
    if(!strncmp(start_quotes, ");", 2)){
      NSDL2_IMAP(NULL, NULL,"End of function found %s", start_quotes);
    }
    else {
      SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012210_ID, CAV_ERR_1012210_MSG, start_quotes);
    } 
  }

  requests[url_idx].proto.imap.imap_action_type = imap_action_type;
  
  // Validate all mandatory arguments are given
  CHECK_MAND_FIELDS 
  return NS_PARSE_SCRIPT_SUCCESS;
}


/* Function calls retry_connection() which will ensure normal http like retries for imap also. */
static inline void
handle_imap_bad_read (connection *cptr, u_ns_ts_t now)
{
  NSDL2_IMAP(NULL, cptr, "cptr=%p conn state=%d, now = %u", cptr, cptr->conn_state, now);
 
  retry_connection(cptr, now, NS_REQUEST_BAD_HDR);
}

//code to handle imap read data and process response
int
handle_imap_read( connection *cptr, u_ns_ts_t now )
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
  int start_tls = 0;
  int err = 0;
  char *err_buff = NULL;
  //char err_msg[65545 + 1];

  NSDL2_IMAP(NULL, cptr, "conn state=%d, now = %u", cptr->conn_state, now);

  request_type = cptr->request_type;
  if (request_type != IMAP_REQUEST && request_type != IMAPS_REQUEST) {
    /* Something is very wrong we should not be here. */
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012459_ID, CAV_ERR_1012459_MSG);
  }

  cptr->body_offset = 0;     /* Offset will always be from 0; used in get_reply_buf */

  while (1) {
    // printf ("0 Req code=%d\n", cptr->req_code);
    if ( do_throttle )
      bytes_read = THROTTLE / 2;
    else
      bytes_read = sizeof(buf) - 1;

#ifdef ENABLE_SSL
  if (request_type == IMAPS_REQUEST) {
    bytes_read = SSL_read(cptr->ssl, buf, bytes_read);
    
        if (bytes_read > 0) buf[bytes_read] = '\0'; // NULL terminate for printing/logging
        NSDL3_SSL(NULL, cptr, "Read %d bytes. msg=%s", bytes_read, (bytes_read>0)?buf:"-");

    if (bytes_read <= 0) {
      err = SSL_get_error(cptr->ssl, bytes_read);
      switch (err) {
      case SSL_ERROR_ZERO_RETURN:  /* means that the connection closed from the server */
	handle_imap_bad_read (cptr, now);
	return -1;
      case SSL_ERROR_WANT_READ:
	return -1;
	/* It can but isn't supposed to happen */
      case SSL_ERROR_WANT_WRITE:
	fprintf(stderr, "SSL_read error: SSL_ERROR_WANT_WRITE\n");
	handle_imap_bad_read (cptr, now);
	return -1;
      case SSL_ERROR_SYSCALL: //Some I/O error occurred
        if (errno == EAGAIN) // no more data available, return (it is like SSL_ERROR_WANT_READ)
        {
 /*          if (runprof_table_shr_mem[vptr->group_num].gset.enable_pipelining &&
              url_num->proto.http.type != MAIN_URL &&
              (cptr->num_pipe != -1) &&
              cptr->num_pipe < runprof_table_shr_mem[vptr->group_num].gset.max_pipeline) {
            pipeline_connection((VUser *)cptr->vptr, cptr, now);
          }
*/
          NSDL1_SSL(NULL, cptr, "IMAP SSL_read: No more data available, return");
            handle_imap_bad_read (cptr, now);
          return -1;
        }

        if (errno == EINTR)
        {
          NSDL3_SSL(NULL, cptr, "IMAP SSL_read interrupted. Continuing...");
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
	    handle_imap_bad_read (cptr, now);
	else
          handle_imap_bad_read (cptr, now);
	return -1;
      }
    }
  } else {
    bytes_read = read(cptr->conn_fd, buf, bytes_read);
    NSDL2_IMAP(NULL, cptr, "bytes_read = %d, buf = %*.*s\n", bytes_read, bytes_read, bytes_read, buf);
#ifdef NS_DEBUG_ON
    if (bytes_read > 0) buf[bytes_read] = '\0'; // NULL terminate for printing/logging
    NSDL3_IMAP(NULL, cptr, "Read %d bytes. msg=%s", bytes_read, bytes_read>0?buf:"-");
#endif

    if ( bytes_read < 0 ) {
      if (errno == EAGAIN) {
#ifndef USE_EPOLL
        FD_SET( cptr->conn_fd, &g_rfdset );
#endif
        return 1;
      } else {
        NSDL3_IMAP(NULL, cptr, "read failed (%s) for main: host = %s [%d]", nslb_strerror(errno),
                   cptr->url_num->index.svr_ptr->server_hostname,
                   cptr->url_num->index.svr_ptr->server_port);
        handle_imap_bad_read (cptr, now);
        return -1;
      }
    } else if (bytes_read == 0) {
      handle_imap_bad_read (cptr, now);
      return -1;
    }
  }
#else
 
    bytes_read = read(cptr->conn_fd, buf, bytes_read);
    NSDL2_IMAP(NULL, cptr, "bytes_read = %d, buf = %*.*s\n", bytes_read, bytes_read, bytes_read, buf);

#ifdef NS_DEBUG_ON
    if (bytes_read > 0) buf[bytes_read] = '\0'; // NULL terminate for printing/logging
      NSDL3_IMAP(NULL, cptr, "Read %d bytes. msg=%s", bytes_read, bytes_read>0?buf:"-");
#endif

    if ( bytes_read < 0 ) {
      if (errno == EAGAIN) {
#ifndef USE_EPOLL
        FD_SET( cptr->conn_fd, &g_rfdset );
#endif
        return 1;
      } else {
        NSDL3_IMAP(NULL, cptr, "read failed (%s) for main: host = %s [%d]", nslb_strerror(errno),
                   cptr->url_num->index.svr_ptr->server_hostname,
                   cptr->url_num->index.svr_ptr->server_port);
        handle_imap_bad_read (cptr, now);
        return -1;
      }
    } else if (bytes_read == 0) {
      handle_imap_bad_read (cptr, now);
      return -1;
    }
#endif

    bytes_handled = 0;

    /* we copy the data to be parsed in future. */
    //TODO: change to imap state
    if (cptr->proto_state == ST_IMAP_FETCH ||
        cptr->proto_state == ST_IMAP_LIST ||
        cptr->proto_state == ST_IMAP_SELECT ||
        cptr->proto_state == ST_IMAP_STORE ||
        cptr->proto_state == ST_IMAP_SEARCH) {
      copy_retrieve_data(cptr, buf, bytes_read, cptr->bytes);
      cptr->bytes += bytes_read;
    }
  
    /* IMAP response parsing */
    for( ; bytes_handled < bytes_read; bytes_handled++) {
      
      if (cptr->header_state == IMAP_H_END) {
        NSDL1_IMAP(NULL, NULL, "IMAP_H_END");
         /* We have read the end and still there are some bytes in the buffer */
        NSDL1_IMAP(NULL, NULL, "Extra bytes in buffer");
        break;                  /* break from for */
      }
      
      switch ( cptr->header_state ) {
      case IMAP_H_NEW:
        if (buf[bytes_handled] == '*'){
          cptr->header_state = IMAP_H_STR;
          NSDL1_IMAP(NULL, NULL, "IMAP_H_STR");
        }else { /* Bad state exit? */
            NSDL1_IMAP(NULL, NULL, "IMAP_H_TXT");
            cptr->header_state = IMAP_H_TXT;
        }
        break;

      case IMAP_H_STR:
        if(buf[bytes_handled] == ' '){
          NSDL1_IMAP(NULL, NULL, "IMAP_H_STR_SP");
          cptr->header_state = IMAP_H_STR_SP;
        }else{
          NSDL1_IMAP(NULL, NULL, "IMAP_H_STR_TXT");
          cptr->header_state = IMAP_H_STR_TXT;
        }

        break;
  
      case IMAP_H_STR_SP:
        if(buf[bytes_handled] == 'B'){
          cptr->header_state = IMAP_H_STR_SP_B;
          NSDL1_IMAP(NULL, NULL, "IMAP_H_STR_SP_B");
        }
        else if(buf[bytes_handled] == 'O'){
          NSDL1_IMAP(NULL, NULL, "IMAP_H_STR_SP_O");
          cptr->header_state = IMAP_H_STR_SP_O;
        }
        else if(buf[bytes_handled] == 'N'){
          NSDL1_IMAP(NULL, NULL, "IMAP_H_STR_SP_N");
          cptr->header_state = IMAP_H_STR_SP_N;
        }
        else{
          NSDL1_IMAP(NULL, NULL, "IMAP_H_STR_TXT");
          cptr->header_state = IMAP_H_STR_TXT;
        }

        break; 

      case IMAP_H_STR_SP_B:
        if(buf[bytes_handled] == 'A'){
          NSDL1_IMAP(NULL, NULL, "IMAP_H_STR_SP_BA");
          cptr->header_state = IMAP_H_STR_SP_BA;
        }
        else{
          NSDL1_IMAP(NULL, NULL, "IMAP_H_STR_TXT");
          cptr->header_state = IMAP_H_STR_TXT;
        }

        break;

     case IMAP_H_STR_SP_BA:
        if(buf[bytes_handled] == 'D'){
          NSDL1_IMAP(NULL, NULL, "IMAP_H_STR_TXT");
          cptr->header_state = IMAP_H_STR_TXT;
          cptr->req_code = IMAP_ERR;
        }
        else{
          NSDL1_IMAP(NULL, NULL, "IMAP_H_STR_TXT");
          cptr->header_state = IMAP_H_STR_TXT;
        }

        break;

     case IMAP_H_STR_SP_N:
        if(buf[bytes_handled] == 'O'){
          NSDL1_IMAP(NULL, NULL, "IMAP_H_STR_TXT");
          cptr->header_state = IMAP_H_STR_TXT;
          cptr->req_code = IMAP_ERR;
        }
        else{
          NSDL1_IMAP(NULL, NULL, "IMAP_H_STR_TXT");
          cptr->header_state = IMAP_H_STR_TXT;
        }

        break;
         
     case IMAP_H_STR_SP_O:
        if(buf[bytes_handled] == 'K'){
          NSDL1_IMAP(NULL, NULL, "IMAP_H_STR_TXT");
          cptr->header_state = IMAP_H_STR_TXT;
          cptr->req_code = IMAP_OK;
        }
        else{
          NSDL1_IMAP(NULL, NULL, "IMAP_H_STR_TXT");
          cptr->header_state = IMAP_H_STR_TXT;
        }

        break;
   
     case IMAP_H_STR_TXT:
        if(buf[bytes_handled] == '.'){
          NSDL1_IMAP(NULL, NULL, "IMAP_H_STR_DOT");
          cptr->header_state = IMAP_H_STR_DOT;
        }
        else if(buf[bytes_handled] == '\r'){  //Do we need to keep the same state or in case of '\r' need to change it
          NSDL1_IMAP(NULL, NULL, "IMAP_H_STR_TXT_CR");
          cptr->header_state = IMAP_H_STR_TXT_CR;
        }else if(buf[bytes_handled] == 'S'){
          NSDL1_IMAP(NULL, NULL, "IMAP_H_STR_TXT_CR");
          cptr->header_state = IMAP_H_STR_TXT_S;
        }

        break;

     case IMAP_H_STR_TXT_S:
        if(buf[bytes_handled] == 'T'){
          cptr->header_state = IMAP_H_STR_TXT_ST;
          NSDL1_IMAP(NULL, NULL, "IMAP_H_STR_TXT_ST");
        }
        else{
          NSDL1_IMAP(NULL, NULL, "IMAP_H_STR_TXT");
          cptr->header_state = IMAP_H_STR_TXT;
        }

        break;

     case IMAP_H_STR_TXT_ST:
        if(buf[bytes_handled] == 'A'){
          cptr->header_state = IMAP_H_STR_TXT_STA;
          NSDL1_IMAP(NULL, NULL, "IMAP_H_STR_TXT_STA");
        }
        else{
          NSDL1_IMAP(NULL, NULL, "IMAP_H_STR_TXT");
          cptr->header_state = IMAP_H_STR_TXT;
        }

        break;

     case IMAP_H_STR_TXT_STA:
        if(buf[bytes_handled] == 'R'){
          cptr->header_state = IMAP_H_STR_TXT_STAR;
          NSDL1_IMAP(NULL, NULL, "IMAP_H_STR_TXT_STAR");
        }
        else{
          NSDL1_IMAP(NULL, NULL, "IMAP_H_STR_TXT");
          cptr->header_state = IMAP_H_STR_TXT;
        }

        break;

     case IMAP_H_STR_TXT_STAR:
        if(buf[bytes_handled] == 'T'){
          cptr->header_state = IMAP_H_STR_TXT_START;
          NSDL1_IMAP(NULL, NULL, "IMAP_H_STR_TXT_START");
        }
        else{
          NSDL1_IMAP(NULL, NULL, "IMAP_H_STR_TXT");
          cptr->header_state = IMAP_H_STR_TXT;
        }

        break;

     case IMAP_H_STR_TXT_START:
        if(buf[bytes_handled] == 'T'){
          cptr->header_state = IMAP_H_STR_TXT_STARTT;
          NSDL1_IMAP(NULL, NULL, "IMAP_H_STR_TXT_STARTT");
        }
        else{
          NSDL1_IMAP(NULL, NULL, "IMAP_H_STR_TXT");
          cptr->header_state = IMAP_H_STR_TXT;
        }

        break;

     case IMAP_H_STR_TXT_STARTT:
        if(buf[bytes_handled] == 'L'){
          cptr->header_state = IMAP_H_STR_TXT_STARTTL;
          NSDL1_IMAP(NULL, NULL, "IMAP_H_STR_TXT_STARTTL");
        }
        else{
          NSDL1_IMAP(NULL, NULL, "IMAP_H_STR_TXT");
          cptr->header_state = IMAP_H_STR_TXT;
        }

        break;

     case IMAP_H_STR_TXT_STARTTL:
        if((buf[bytes_handled] == 'S') && (cptr->proto_state == ST_IMAP_CONNECTED) && (cptr->url_num->proto.imap.authentication_type)){
          start_tls = 1;
          NSDL1_IMAP(NULL, NULL, "IMAP_H_STR_TXT_STARTTLS");
        }
        else{
          NSDL1_IMAP(NULL, NULL, "IMAP_H_STR_TXT");
          cptr->header_state = IMAP_H_STR_TXT;
        }

        break;
     
     case IMAP_H_STR_TXT_CR:
        if(buf[bytes_handled] == '\n'){
          NSDL1_IMAP(NULL, NULL, "IMAP_H_NEW");
          cptr->header_state = IMAP_H_NEW;
        }
        else{
          NSDL1_IMAP(NULL, NULL, "IMAP_H_STR_TXT");
          cptr->header_state = IMAP_H_STR_TXT;
        }

        break;

     case IMAP_H_STR_DOT:
        if(buf[bytes_handled] == '\r'){
          NSDL1_IMAP(NULL, NULL, "IMAP_H_STR_DOT_CR");
          cptr->header_state = IMAP_H_STR_DOT_CR;
        }
        else{
          NSDL1_IMAP(NULL, NULL, "IMAP_H_STR_TXT");
          cptr->header_state = IMAP_H_STR_TXT;
        }

        break;
         
     case IMAP_H_STR_DOT_CR:
        if(buf[bytes_handled] == '\n'){
          if(cptr->proto_state == ST_IMAP_CONNECTED || cptr->proto_state == ST_IMAP_LOGOUT){
            NSDL1_IMAP(NULL, NULL, "HANDSHAKE and IMAP_H_END");
            cptr->header_state = IMAP_H_END;
          }
          else{
            NSDL1_IMAP(NULL, NULL, "IMAP_H_NEW");
            cptr->header_state = IMAP_H_NEW;
          }
        }
        else{
          NSDL1_IMAP(NULL, NULL, "IMAP_H_STR_TXT");
          cptr->header_state = IMAP_H_STR_TXT;
        }

        break;

     case IMAP_H_TXT:
        if(buf[bytes_handled] == ' '){ //may occcur in middle of line and BAD will come , ??need to handle that situation????????????????????
          NSDL1_IMAP(NULL, NULL, "IMAP_H_TXT_SP");
          cptr->header_state = IMAP_H_TXT_SP;
        }
        else if(buf[bytes_handled] == '.'){
          NSDL1_IMAP(NULL, NULL, "IMAP_H_TXT_DOT");
          cptr->header_state = IMAP_H_TXT_DOT;
        }
        else if(buf[bytes_handled] == '\r'){
          NSDL1_IMAP(NULL, NULL, "IMAP_H_TXT_CR");
          cptr->header_state = IMAP_H_TXT_CR;
       }

        break;

     case IMAP_H_TXT_DOT:
       if(buf[bytes_handled] == '\r'){
          NSDL1_IMAP(NULL, NULL, "IMAP_H_TXT_DOT_CR");
          cptr->header_state = IMAP_H_TXT_DOT_CR;
       }
       else {
          NSDL1_IMAP(NULL, NULL, "IMAP_H_TXT");
          cptr->header_state = IMAP_H_TXT;
       }
       break;

     case IMAP_H_TXT_DOT_CR:
       if(buf[bytes_handled] == '\n'){
          NSDL1_IMAP(NULL, NULL, "IMAP_H_END");
          cptr->header_state = IMAP_H_END;
       }
       else{
          NSDL1_IMAP(NULL, NULL, "IMAP_H_TXT ");
          cptr->header_state = IMAP_H_TXT;
       }
       break;
 
     case IMAP_H_TXT_CR:
       if(buf[bytes_handled] == '\n'){
          if((cptr->proto_state == ST_IMAP_LOGIN) || (cptr->req_code == IMAP_ERR)){
            NSDL1_IMAP(NULL, NULL, "LOGIN/ IMAP_ERR  &&  IMAP_H_END");
            cptr->header_state = IMAP_H_END;
          }
          else{
            NSDL1_IMAP(NULL, NULL, "IMAP_H_NEW");
            cptr->header_state = IMAP_H_NEW;
          }
       }
       else{
          NSDL1_IMAP(NULL, NULL, "IMAP_H_TXT");
          cptr->header_state = IMAP_H_TXT;
       }

       break;
    
      case IMAP_H_TXT_SP:
        if(buf[bytes_handled] == 'B'){
          NSDL1_IMAP(NULL, NULL, "IMAP_H_TXT_SP_B");
          cptr->header_state = IMAP_H_TXT_SP_B;
        }
        else if(buf[bytes_handled] == 'O'){
          NSDL1_IMAP(NULL, NULL, "IMAP_H_TXT_SP_O ");
          cptr->header_state = IMAP_H_TXT_SP_O;
        }
        else if(buf[bytes_handled] == 'N'){
          NSDL1_IMAP(NULL, NULL, "IMAP_H_TXT_SP_N");
          cptr->header_state = IMAP_H_TXT_SP_N;
        }
        else{
          NSDL1_IMAP(NULL, NULL, "IMAP_H_TXT");
          cptr->header_state = IMAP_H_TXT;
        }

        break; 

      case IMAP_H_TXT_SP_B:
        if(buf[bytes_handled] == 'A'){
          NSDL1_IMAP(NULL, NULL, "IMAP_H_TXT_SP_BA");
          cptr->header_state = IMAP_H_TXT_SP_BA;
        }
        else{
          NSDL1_IMAP(NULL, NULL, "IMAP_H_TXT");
          cptr->header_state = IMAP_H_TXT;
        }

        break;

     case IMAP_H_TXT_SP_BA:
        if(buf[bytes_handled] == 'D'){
          NSDL1_IMAP(NULL, NULL, "ERR && IMAP_H_TXT");
          cptr->header_state = IMAP_H_TXT;
          cptr->req_code = IMAP_ERR;
        }
        else{
          NSDL1_IMAP(NULL, NULL, "IMAP_H_TXT");
          cptr->header_state = IMAP_H_TXT;
        }

        break;

     case IMAP_H_TXT_SP_N:
        if(buf[bytes_handled] == 'O'){
          NSDL1_IMAP(NULL, NULL, "ERR   && IMAP_H_TXT");
          cptr->header_state = IMAP_H_TXT;
          cptr->req_code = IMAP_ERR;
        }
        else{
          NSDL1_IMAP(NULL, NULL, "IMAP_H_TXT");
          cptr->header_state = IMAP_H_TXT;
        }

        break;
         
     case IMAP_H_TXT_SP_O:
        if(buf[bytes_handled] == 'K'){
          NSDL1_IMAP(NULL, NULL, "OK && IMAP_H_TXT ");
          cptr->header_state = IMAP_H_TXT;
          cptr->req_code = IMAP_OK;
        }
        else{
          NSDL1_IMAP(NULL, NULL, "IMAP_H_TXT");
          cptr->header_state = IMAP_H_TXT;
        }

        break;

     case IMAP_H_END:                
        break;

     default:
        break;
      }
    }
    
    /* Here it will be imap specific so we need to add new fields in avg_time */
    cptr->tcp_bytes_recv += bytes_handled;
    imap_avgtime->imap_rx_bytes += bytes_handled;
    
    cptr->bytes += bytes_handled;
    imap_avgtime->imap_total_bytes += bytes_handled;
    
    INC_IMAP_RX_BYTES(vptr, bytes_handled); 
#ifdef NS_DEBUG_ON
    debug_log_imap_res(cptr, buf, bytes_read);
#endif

    if (cptr->header_state == IMAP_H_END) {
        NSDL2_IMAP(NULL, cptr, "conn state=%d, proto_state = %d, now = %u", cptr->conn_state, cptr->proto_state, now);
      /* Reset the state for next time */
       cptr->header_state = IMAP_H_NEW;
       //return from here as now we are going to start TLS negotiation
/*       if((cptr->proto_state == ST_IMAP_CONNECTED) && (cptr->url_num->proto.imap.authentication_type) && (cptr->req_code == IMAP_OK) && (start_tls)){
         return 0; 
       }*/
       break;  /* From while */
     }
  }

  // delete imap timeout timers
   delete_imap_timeout_timer(cptr);

   NSDL2_IMAP(NULL, cptr, "conn state=%d, proto_state = %d, now = %u", cptr->conn_state, cptr->proto_state, now);
 
  /* IMAP state machine */
  switch(cptr->proto_state) {
  case ST_IMAP_CONNECTED:
    NS_DT2(NULL, cptr, DM_L1, MM_IMAP, "Handshake done.");
    imap_process_handshake(cptr, now, buf, bytes_read, start_tls);
    break;
  case ST_IMAP_STARTTLS:
    NS_DT2(NULL, cptr, DM_L1, MM_IMAP, "Received response for 'STARTTLS' command.");
    imap_process_starttls(cptr, now, buf, bytes_read);
    break;
  case ST_IMAP_LOGIN:
    NS_DT2(NULL, cptr, DM_L1, MM_IMAP, "Received response for 'LOGIN' command.");
    imap_process_login(cptr, now, buf, bytes_read);
    break;
  case ST_IMAP_LIST:
    NS_DT2(NULL, cptr, DM_L1, MM_IMAP, "Received response for 'LIST' command.");
    imap_process_list(cptr, now, buf, bytes_read);
    break;
  case ST_IMAP_SELECT:
    NS_DT2(NULL, cptr, DM_L1, MM_IMAP, "Received response for 'SELECT' command.");
    imap_process_select(cptr, now, buf, bytes_read);
    break;
  case ST_IMAP_FETCH:
    NS_DT2(NULL, cptr, DM_L1, MM_IMAP, "Received response for 'FETCH' command.");
    imap_process_fetch(cptr, now, buf, bytes_read);
    break;
  case ST_IMAP_LOGOUT:
    NS_DT2(NULL, cptr, DM_L1, MM_IMAP, "Received response for 'LOGOUT' command.");
    imap_process_logout(cptr, now, buf, bytes_read);
    break;
 /* case ST_IMAP_SEARCH:
    NS_DT2(NULL, cptr, DM_L1, MM_IMAP, "Received response for 'SEARCH' command.");
    imap_process_search(cptr, now, buf, bytes_read);
    break;*/
 /* case ST_IMAP_STORE:
    NS_DT2(NULL, cptr, DM_L1, MM_IMAP, "Received response for 'STORE' command.");
    imap_process_store(cptr, now, buf, bytes_read);
    break;*/
  default:
    NS_DT2(NULL, cptr, DM_L1, MM_IMAP, "Invalid IMAP protocol state");
    //NSEL_MAJ(NULL, cptr, "Invalid IMAP protocol state (%d)", cptr->proto_state);
    break;
  }
  return 0;
}
