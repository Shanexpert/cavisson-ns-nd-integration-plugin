/************************************************************************************
 * Name      : ns_xmpp.c 
 * Purpose   : This file contains functions related to XMPP Protocol 
 * Author(s) : Atul Kumar Sharma
 * Date      : 14 June 2018 
 * Copyright : (c) Cavisson Systems
 * Modification History :
 ***********************************************************************************/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <openssl/sha.h>

#include "url.h"
#include "util.h"
#include "netstorm.h"
#include "ns_common.h"
#include "ns_global_settings.h"
#include "ns_log.h"
#include "ns_script_parse.h"
#include "ns_http_process_resp.h"
#include "ns_log_req_rep.h"
#include "nslb_util.h"
#include "ns_group_data.h"
#include "ns_exit.h"
#include "ns_trace_level.h"
#include "ns_xmpp.h"

/***********************Global Variable ******************/

#ifndef CAV_MAIN
int cur_post_buf_len;
#else
extern __thread int cur_post_buf_len;
#endif



/*******************************************************/

static int init_xmpp_uri(int *url_idx, char *flow_file)
{

  NSDL2_XMPP(NULL, NULL, "Method Called url_idx = %d, flow_file = %s", *url_idx, flow_file); 

  //creating request table
  if(create_requests_table_entry(url_idx) != SUCCESS) // Fill request type inside create table entry
   {
     SCRIPT_PARSE_ERROR(script_line, "get_url_requets(): Could not create xmpp request entry while parsing line %d in file %s\n", 
                                                                                                    script_ln_no, flow_filename);
   }

   gPageTable[g_cur_page].first_eurl = *url_idx;
    
  return NS_PARSE_SCRIPT_SUCCESS;
}

/*
//extract buffer
static int extract_buffer(FILE *flow_fp, char *start_ptr, char **end_ptr, char *flow_file)
{
  int buffer_over = 0;

  NSDL1_XMPP(NULL, NULL, "Method called, start_ptr = [%s]", start_ptr);

  if((start_ptr = strstr(start_ptr, "=")) == NULL)
    SCRIPT_PARSE_ERROR(script_line, "Buffer Should Start on a new line");

  start_ptr++;

  if(strrchr(script_line, '"')) // Find the last quote
    *end_ptr = strrchr(script_line, '"');

  NSDL1_XMPP(NULL, NULL, "end_ptr = %s", end_ptr);

  copy_to_post_buf(start_ptr, strlen(start_ptr));

  if(*end_ptr != NULL)
  {
    NSDL2_PARSING(NULL, NULL, "Buffer is over");
    buffer_over = 1;
  }

  if(buffer_over == 0)
    SCRIPT_PARSE_ERROR(script_line, "END_INLINE keyword is missing");

  return NS_PARSE_SCRIPT_SUCCESS;
}
*/

static int xmpp_set_post_body(int send_tbl_idx, int sess_idx, int *script_ln_no, char *cap_fname)
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
          sprintf (fbuf, "%s/scripts/%s/%s", g_ns_wdir,
                   get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name), sess_idx, "/"), fname);
                   //Previously taking with only script name
                   //get_sess_name_with_proj_subproj(RETRIEVE_BUFFER_DATA(gSessionTable[sess_idx].sess_name)), fname);
          fname = fbuf;
      }

      ffd = open (fname, O_RDONLY);
      if (!ffd) {
          NSDL4_XMPP("%s() : Unable to open $CAVINCLUDE$ file %s\n", (char*)__FUNCTION__, fname);
          return NS_PARSE_SCRIPT_ERROR;
      }
      cur_post_buf_len = 0;
      while (1) {
          rlen = read (ffd, fbuf, 8192);
          if (rlen > 0) {
            if (copy_to_post_buf(fbuf, rlen)) {
              NSDL4_XMPP(NULL, NULL,"%s(): Request BODY could not alloccate mem for %s\n", (char*)__FUNCTION__, fname);
              return NS_PARSE_SCRIPT_ERROR;
            }
            continue;
          } else if (rlen == 0) {
              break;
          } else {
              perror("reading CAVINCLUDE BODY");
              NSDL4_XMPP(NULL, NULL, "%s(): Request BODY could not read %s\n", (char*)__FUNCTION__, fname);
              return NS_PARSE_SCRIPT_ERROR;
          }
      }
      close (ffd);
  }
  if (cur_post_buf_len) 
  {
    if (noparam_flag) {
      segment_line_noparam(&(requests[send_tbl_idx].proto.xmpp.message), g_post_buf, sess_idx);
    } else {
      segment_line(&(requests[send_tbl_idx].proto.xmpp.message), g_post_buf, 0, *script_ln_no, sess_idx, cap_fname);
    }
  }
  return NS_PARSE_SCRIPT_SUCCESS; 
}


#if 0
static int xmpp_set_domain(char *domain, char *flow_file, int sess_idx, int url_idx)
{

  char hostname[MAX_LINE_LENGTH + 1];
  int  request_type;
  char request_line[MAX_LINE_LENGTH + 1];
  //int get_no_inlined_obj_set_for_all = 1;

  NSDL2_PARSING(NULL, NULL, "Method Called Uri=%s", domain);
  //Parses Absolute/Relative URLs
  //TODO
  //if(parse_domain(domain, "{/?#", &request_type, hostname, request_line) != RET_PARSE_OK)
  //  SCRIPT_PARSE_ERROR(script_line, "Invalid URL");

  //Request type should be from xmpp
  if(request_type == REQUEST_TYPE_NOT_FOUND)
    SCRIPT_PARSE_ERROR(script_line, "Invalid DOMAIN");
  

  //proto_based_init(url_idx, request_type);
  request_type = XMPP_REQUEST;
  requests[url_idx].request_type = request_type;

  //Setting url type to Main/Embedded
  gPageTable[g_cur_page].num_eurls++; // Increment urls

  if (g_max_num_embed < gPageTable[g_cur_page].num_eurls) g_max_num_embed = gPageTable[g_cur_page].num_eurls; //Get high water mark

  // check if the hostname exists in the server table, if not add it
  requests[url_idx].index.svr_idx = get_server_idx(hostname, requests[url_idx].request_type, script_ln_no);

  if(requests[url_idx].index.svr_idx != -1)
  {
    if(gServerTable[requests[url_idx].index.svr_idx].main_url_host == -1)
    {
      gServerTable[requests[url_idx].index.svr_idx].main_url_host = 1; // For main url
    }
  }
  else
  {
    SCRIPT_PARSE_ERROR(script_line, "Could not Add hostname to server table");
  }
   //StrEnt* segtable = &(requests[url_idx].proto.ws.uri);
  segment_line(&(requests[url_idx].proto.xmpp.domain), request_line, 0, script_ln_no, sess_idx, flow_filename);

  /*Added for filling all server in gSessionTable*/
  CREATE_AND_FILL_SESS_HOST_TABLE_ENTRY(url_idx, "Method called from xmpp_set_domain");

  NSDL3_XMPP(NULL, NULL, "Exitting Method ");
  return NS_PARSE_SCRIPT_SUCCESS;

}
#endif
/*
Below is the xmpp opening stream 
*/
/*
void conn_open_stream(xmpp_conn_t * const conn)
{
    xmpp_send_raw_string(conn, "<?xml version=\"1.0\"?>"               \
                         "<stream:stream to=\"%s\" "                   \
                         "xml:lang=\"%s\" "                            \
                         "version=\"1.0\" "                            \
                         "xmlns=\"%s\" "                               \
                         "xmlns:stream=\"%s\">",
                         conn->domain,
                         conn->lang,
                         conn->type == XMPP_CLIENT ? XMPP_NS_CLIENT :
                                                     XMPP_NS_COMPONENT,
                         XMPP_NS_STREAMS);
}

*/
/*--------------------------------------------------------------------------------------------- 
 * Purpose   : This function will parse ns_xmpp_login() API and do follwing things 
 *              
 *             (1) Create and fill following tables -
 *                 (i) Add dummy page name like xmpp_<id> into gPageTable
 *                 (ii) Create request table and fill data  
 *                 (iii) create requst Table and fill its members 
 *
 * Input     : flow_fp - pointer to input flow file 
 *                                      ns_xmpp_login("PageName",  //dummy not given by user
 *                                      "user_name=xmpp,
 *                                      "Domain=cavisson.com",
 *                                      "password = your xmpp pasword",
 *             outfp   - pointer to output flow file (made in $NS_WDIR/.tmp/ns-inst<nvm_id>/)
 *             flow_filename - flow file name 
 *             sess_idx- pointing to session index in gSessionTable 
 *
 * Output    : On success -  0
 *             On Failure - -1  
 *--------------------------------------------------------------------------------------------*/
int ns_parse_xmpp_login(FILE *flow_fp, FILE *outfp, char *flow_filename, char *flowout_filename, int sess_idx)
{

  int url_idx;
  char pagename[XMPP_MAX_ATTR_LEN + 1];
  int ret;
  char *close_quotes = NULL;
  char *start_quotes = NULL;
  char *page_end_ptr = NULL;
  char attribute_name[XMPP_MAX_ATTR_LEN + 1];
  char attribute_value[XMPP_MAX_ATTR_LEN + 1];
  char user_flag , password_flag, domain_flag, sasl_auth_type_flag, starttls_flag, accept_contact;
  char file_upload_service, group_chat_service; 
  user_flag = password_flag = domain_flag = sasl_auth_type_flag =starttls_flag = accept_contact = 0;
  file_upload_service = group_chat_service = 0;

  NSDL2_XMPP(NULL, NULL, "Method Called, sess_idx = %d", sess_idx);  


  //Adding Dummy page name as in ns_xmpp_connect() API page name is not given 
  sprintf(pagename, "xmpp_%d", web_url_page_id);

  page_end_ptr = strchr(script_line, '"');

  NSDL2_XMPP(NULL, NULL, "pagename - [%s], page_end_ptr = [%s]", pagename, page_end_ptr);

  if((parse_and_set_pagename("ns_xmpp_login", "ns_web_url", flow_fp, flow_filename, 
              script_line, outfp, flowout_filename, sess_idx, &page_end_ptr, pagename)) == NS_PARSE_SCRIPT_ERROR)
  return NS_PARSE_SCRIPT_ERROR;

  close_quotes = page_end_ptr;
  start_quotes = page_end_ptr;

  if(init_xmpp_uri(&url_idx, flow_filename) == NS_PARSE_SCRIPT_ERROR)  //request table is created
    return NS_PARSE_SCRIPT_ERROR;

  //Set default values here because in below loop all the members reset and creating  problem
  proto_based_init(url_idx, XMPP_REQUEST); 
  requests[url_idx].proto.xmpp.action = NS_XMPP_LOGIN;  

  while(1)
  {
    NSDL3_XMPP(NULL, NULL, "line = %s", script_line);
    ret = get_next_argument(flow_fp, start_quotes, attribute_name, attribute_value, &close_quotes, 0);
    if(ret == NS_PARSE_SCRIPT_ERROR) return NS_PARSE_SCRIPT_ERROR;
    
    if(!strcasecmp(attribute_name, "UserName"))
    {
      NSDL2_XMPP(NULL, NULL, "User_name [%s] ", attribute_value);
      if(user_flag) {
        SCRIPT_PARSE_ERROR(script_line, "UserName can be given once.\n");
      }
      user_flag = 1;

      segment_line(&(requests[url_idx].proto.xmpp.user), attribute_value, 0, script_ln_no, sess_idx, flow_filename);
      //if(xmpp_set_user(attribute_value, flow_filename, sess_idx, url_idx) == NS_PARSE_SCRIPT_ERROR)
      //  return NS_PARSE_SCRIPT_ERROR;
      NSDL2_XMPP(NULL, NULL, "XMPP: Value of %s = %s ,segment offset = %d", 
                              attribute_name, attribute_value, requests[url_idx].proto.xmpp.user);


    } else if(!strcasecmp(attribute_name, "Domain")) {

      if(domain_flag) {
        SCRIPT_PARSE_ERROR(script_line, "domain can be given once.\n");
      }
      domain_flag = 1;
      //proto_based_init(url_idx, XMPP_REQUEST);

      requests[url_idx].index.svr_idx = get_server_idx(attribute_value, requests[url_idx].request_type, script_ln_no);

      CREATE_AND_FILL_SESS_HOST_TABLE_ENTRY(url_idx, "Method called from ns_xmpp_login");
      NSDL2_XMPP(NULL, NULL, "url_idx = %d, attribute_value = %s", url_idx, attribute_value);
      //if(xmpp_set_domain(attribute_value, flow_filename, sess_idx, url_idx) == NS_PARSE_SCRIPT_ERROR)
      segment_line(&(requests[url_idx].proto.xmpp.domain), attribute_value, 0, script_ln_no, sess_idx, flow_filename);
      NSDL2_XMPP(NULL, NULL, "origin = %d", requests[url_idx].proto.xmpp.domain); 

    } else if(!strcasecmp(attribute_name, "Password")) {
      if(password_flag) {
        SCRIPT_PARSE_ERROR(script_line, "PASSWORD can be given once.\n");
      }
      password_flag = 1;

      NSDL2_XMPP(NULL, NULL, "url_idx = %d, attribute_value = %s", url_idx, attribute_value);
      segment_line(&(requests[url_idx].proto.xmpp.password), attribute_value, 0, script_ln_no, sess_idx, flow_filename);
      NSDL2_XMPP(NULL, NULL, "password offset = %d", requests[url_idx].proto.xmpp.domain); 

    } else if(!strcasecmp(attribute_name, "SASL_AUTH_TYPE")) {
      if(sasl_auth_type_flag) {
        SCRIPT_PARSE_ERROR(script_line, "SASL_AUTH_TYPE can be given once.\n");
      }
      sasl_auth_type_flag = 1;

      NSDL2_XMPP(NULL, NULL, "url_idx = %d, attribute_value = %s", url_idx, attribute_value);

      segment_line(&(requests[url_idx].proto.xmpp.sasl_auth_type), attribute_value, 0, script_ln_no, sess_idx, flow_filename);
      NSDL2_XMPP(NULL, NULL, "password offset = %d", requests[url_idx].proto.xmpp.sasl_auth_type); 

    } else if(!strcasecmp(attribute_name, "STARTTLS")) {  //starttls yes/no Parameterization is not allowed in this parameter.

      if(starttls_flag) {
        SCRIPT_PARSE_ERROR(script_line, "STARTTLS can be given once.\n");
      }
      starttls_flag = 1;
      NSDL2_XMPP(NULL, NULL, "url_idx = %d, attribute_value = %s", url_idx, attribute_value);

      if(!strcasecmp(attribute_value, "NO"))
        requests[url_idx].proto.xmpp.starttls = STARTTLS_DISSABLE;
      else if(!strcasecmp(attribute_value, "YES"))
        requests[url_idx].proto.xmpp.starttls = STARTTLS_ENABLE;
      else{
        SCRIPT_PARSE_ERROR(script_line, "Unexpected option for STARTTLS %s, option can be either YES or NO", attribute_value);
      }

      NSDL2_XMPP(NULL, NULL, "starttls = %d", requests[url_idx].proto.xmpp.starttls); 

    } else if(!strcasecmp(attribute_name, "accept_contact")) {  //accept_contact yes/no Parameterization is not allowed in this parameter.

      if(accept_contact) {
        SCRIPT_PARSE_ERROR(script_line, "accept_contact can be given once.\n");
      }
      accept_contact = 1;
      NSDL2_XMPP(NULL, NULL, "url_idx = %d, attribute_value = %s", url_idx, attribute_value);

      if(!strcasecmp(attribute_value, "NO"))
        requests[url_idx].proto.xmpp.accept_contact = NOT_ACCEPT_CONTACT;
      else if(!strcasecmp(attribute_value, "YES"))
        requests[url_idx].proto.xmpp.accept_contact = ACCEPT_CONTACT;
      else{
        SCRIPT_PARSE_ERROR(script_line, "Unexpected option for accept_contact %s, option can be either YES or NO", attribute_value);
      }

      NSDL2_XMPP(NULL, NULL, "accept_contact = %d", requests[url_idx].proto.xmpp.accept_contact); 
    }else if(!strcasecmp(attribute_name, "group_chat_service")) {  

      if(group_chat_service) {
        SCRIPT_PARSE_ERROR(script_line, "group_chat_service can be given once.\n");
      }
      group_chat_service = 1;
      NSDL2_XMPP(NULL, NULL, "url_idx = %d, attribute_value = %s", url_idx, attribute_value);
      
      segment_line(&(requests[url_idx].proto.xmpp.group), attribute_value, 0, script_ln_no, sess_idx, flow_filename);
      NSDL2_XMPP(NULL, NULL, "group offset = %d", requests[url_idx].proto.xmpp.group); 

    }else if(!strcasecmp(attribute_name, "file_upload_service")) {  

      if(file_upload_service) {
        SCRIPT_PARSE_ERROR(script_line, "file_upload_service can be given once.\n");
      }
      file_upload_service = 1;
      NSDL2_XMPP(NULL, NULL, "url_idx = %d, attribute_value = %s", url_idx, attribute_value);
      
      segment_line(&(requests[url_idx].proto.xmpp.file), attribute_value, 0, script_ln_no, sess_idx, flow_filename);
      NSDL2_XMPP(NULL, NULL, "file offset = %d", requests[url_idx].proto.xmpp.file); 

    }
    else
    {
      SCRIPT_PARSE_ERROR(script_line,"Unhandled argument found [%s]", attribute_name);
    } 

    ret = read_till_start_of_next_quotes(flow_fp, flow_filename, close_quotes, &start_quotes, 0, outfp);
    if(ret == NS_PARSE_SCRIPT_ERROR)
    {
      NSDL2_XMPP(NULL, NULL, "Next attribute is not found");
      break;
    }
  } //End while loop here

  if(!user_flag && !domain_flag && !password_flag && !sasl_auth_type_flag)
    SCRIPT_PARSE_ERROR(script_line,"User, Domain, Password, Sala Auth Type all are mandatory arguments"); 

  NSDL2_XMPP(NULL, NULL, "Exiting Method");
  return NS_PARSE_SCRIPT_SUCCESS;
}

static int set_api(char *api_name, char *api_to_run, FILE *flow_fp, char *flow_file, char *line_ptr,
                                  FILE *outfp,  char *flow_outfile, int send_tb_idx)
{
  char *start_idx;
  char *end_idx;
  char str[MAX_LINE_LENGTH + 1];
  int len ;

  NSDL1_XMPP(NULL, NULL ,"Method Called. send_tb_idx = %d", send_tb_idx);
  start_idx = line_ptr;
  NSDL2_PARSING(NULL, NULL, "start_idx = [%s]", start_idx);
  end_idx = strstr(line_ptr, api_name);
  if(end_idx == NULL)
    SCRIPT_PARSE_ERROR(script_line, "Invalid Format or API Name not found in Line");

  len = end_idx - start_idx;
  strncpy(str, start_idx, len); //Copying the return value first
  str[len] = '\0';

  // Write like this. ret = may not be there
  //         int ret = ns_web_url(0) // HomePage

  NSDL2_XMPP(NULL, NULL,"Before sprintf str is = %s ,send_tb_idx = %d", str, send_tb_idx);
  sprintf(str, "%s %s(%d); ", str, api_to_run, send_tb_idx);
  NSDL2_XMPP(NULL, NULL," final str is = %s ", str);
  if(write_line(outfp, str, 1) == NS_PARSE_SCRIPT_ERROR)
    SCRIPT_PARSE_ERROR(script_line, "Error Writing in File ");

  return NS_PARSE_SCRIPT_SUCCESS;
}

//Parse xmpp_send api
int ns_parse_xmpp_send(FILE *flow_fp, FILE *outfp,  char *flow_file, char *flow_outfile, int sess_idx)
{

  char attribute_name[128 + 1];
  char attribute_value[MAX_LINE_LENGTH];
  char *starting_quotes = NULL;
  int send_tb_idx = 0;
  int ret;
  char *closing_quotes = '\0';
  char user_flag;
  char message_flag;
  char file_flag;
  message_flag = file_flag = user_flag = 0;

  NSDL1_XMPP(NULL, NULL, "Method Called, sess_idx = %d, script_line = [%s]", sess_idx, script_line);

  //To store send msg into global buffer g_post_buf
  init_post_buf();
  if (create_requests_table_entry(&send_tb_idx) != SUCCESS) {   // Fill request type inside create table entry
      SCRIPT_PARSE_ERROR(script_line, "Could not create request entry while parsing line ");
  }
  starting_quotes = strchr(script_line, '"');
  NSDL4_PARSING(NULL, NULL, "starting_quotes = [%s], script_line = [%s]", starting_quotes, script_line);


  if(( set_api("ns_xmpp_send", "ns_xmpp_send", flow_fp, flow_filename, script_line, outfp, flow_outfile, send_tb_idx)) == NS_PARSE_SCRIPT_ERROR)
    NSDL2_XMPP(NULL, NULL, "Method Called starting_quotes = %p, starting_quotes = %s", starting_quotes, starting_quotes);

  //Set default values here because in below loop all the members reset and creating  problem
  proto_based_init(send_tb_idx, XMPP_REQUEST); 
  requests[send_tb_idx].proto.xmpp.action = NS_XMPP_SEND_MESSAGE;  

  while(1)
  {
    NSDL2_XMPP(NULL, NULL, "script_line = %s, send_tb_idx = %d", script_line, send_tb_idx);
    if(get_next_argument(flow_fp, starting_quotes, attribute_name, attribute_value, &closing_quotes, 0) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;
    NSDL2_XMPP(NULL, NULL, "attribute_name = %s, attribute_value = %s", attribute_name, attribute_value);
  
    if(!strcasecmp(attribute_name, "USER"))
    {
      NSDL2_XMPP(NULL, NULL, "user_name [%s] ", attribute_value);
      if(user_flag) {
         SCRIPT_PARSE_ERROR(script_line, "uid & group can be given once or can not apply together.\n");
      }
      user_flag = 1;

      segment_line(&(requests[send_tb_idx].proto.xmpp.user), attribute_value, 0, script_ln_no, sess_idx, flow_filename);
      NSDL2_XMPP(NULL, NULL, "xmpp: value of %s = %s ,segment offset = %d", 
                              attribute_name, attribute_value, requests[send_tb_idx].proto.xmpp.user);
      

    }
    else if(!strcasecmp(attribute_name, "GROUP"))
    {
      NSDL2_XMPP(NULL, NULL, "group_name [%s] ", attribute_value);
      if(user_flag) {
        SCRIPT_PARSE_ERROR(script_line, "uid & group can be given once or can not apply together.\n");
      }
      user_flag = 1;
      segment_line(&(requests[send_tb_idx].proto.xmpp.group), attribute_value, 0, script_ln_no, sess_idx, flow_filename);
      NSDL2_XMPP(NULL, NULL, "xmpp: value of %s = %s ,segment offset = %d", 
                              attribute_name, attribute_value, requests[send_tb_idx].proto.xmpp.group);

    }
    else if(!strcasecmp(attribute_name, "MESSAGE"))
    {
      //Buffer should be sent one time only
      if(message_flag){
        SCRIPT_PARSE_ERROR(script_line, "Message can be given once.\n");
      }
      message_flag++;

      if(extract_buffer(flow_fp, script_line, &closing_quotes, flow_file) == NS_PARSE_SCRIPT_ERROR)
         return NS_PARSE_SCRIPT_ERROR;
      
       if(xmpp_set_post_body(send_tb_idx, sess_idx, &script_ln_no, flow_file) == NS_PARSE_SCRIPT_ERROR)
       {
         NSDL2_XMPP(NULL, NULL, "Send data at message.seg_start = %lu, message.num_ernties = %d ",   
                               requests[send_tb_idx].proto.xmpp.message.seg_start, requests[send_tb_idx].proto.xmpp.message.num_entries);
         return NS_PARSE_SCRIPT_ERROR;
       }
       NSDL2_XMPP(NULL, NULL, "XMPP_SEND: BUFFER = %s", g_post_buf);
    }
    else if(!strcasecmp(attribute_name, "FILE"))
    {
      //Buffer should be sent one time only
      if(file_flag){
        SCRIPT_PARSE_ERROR(script_line, "Flag can be given once.\n");
      }
      file_flag++;

      segment_line(&(requests[send_tb_idx].proto.xmpp.file), attribute_value, 0, script_ln_no, sess_idx, flow_filename);
      NSDL2_XMPP(NULL, NULL, "XMPP: Value of %s = %s ,segment offset = %d", 
                              attribute_name, attribute_value, requests[send_tb_idx].proto.xmpp.file);

      if(segTable[requests[send_tb_idx].proto.xmpp.file.seg_start].type == STR)
      {
        if(add_static_file_entry(attribute_value, sess_idx) < 0)
          SCRIPT_PARSE_ERROR(script_line, "Can't load file %s\n",attribute_value);
      }
      else //in case of ns variable
      {
         add_all_static_files(sess_idx);
      }
    }
    //we need FILE attribute to send a file to server
    else
    {
      SCRIPT_PARSE_ERROR(script_line,"Unhandled argument found [%s]", attribute_name);
    } 

    *starting_quotes = '\0';
    ret = read_till_start_of_next_quotes(flow_fp, flow_file, closing_quotes, &starting_quotes, 0, outfp);
    NSDL2_PARSING(NULL, NULL, "Starting Quotes=[%s], Starting quotes=[%p], Closing_quotes=[%s], Closing_quotes=[%p]," 
                               "after read_till_start_of_next_quotes()",starting_quotes, starting_quotes, closing_quotes, closing_quotes);
    if(ret == NS_PARSE_SCRIPT_ERROR)
    {
      if(starting_quotes == NULL)
        return NS_PARSE_SCRIPT_ERROR;
      else // Means current parameters are processed, so break
        break;
    }

  } //End of while
  
  if(!user_flag && !(message_flag || file_flag))
    SCRIPT_PARSE_ERROR(script_line,"USER and Message/File are mandatory attributes", attribute_name);


  return NS_PARSE_SCRIPT_SUCCESS;
} 

int ns_parse_xmpp_add_contact(FILE *flow_fp, FILE *outfp,  char *flow_file, char *flow_outfile, int sess_idx)
{
  char attribute_name[128 + 1];
  char attribute_value[MAX_LINE_LENGTH];
  char pagename[XMPP_MAX_ATTR_LEN + 1];
  char *starting_quotes = NULL;
  int send_tb_idx = 0;
  int ret;
  char *closing_quotes = '\0';
  char group_flag = 0;
  char user_flag = 0;

  NSDL1_XMPP(NULL, NULL, "Method Called, sess_idx = %d, script_line = [%s]", sess_idx, script_line);

  if (create_requests_table_entry(&send_tb_idx) != SUCCESS) {   // Fill request type inside create table entry
      SCRIPT_PARSE_ERROR(script_line, "Could not create request entry while parsing line ");
  }
  
  //Adding Dummy page name as in ns_xmpp_send() API page name is not given 
  sprintf(pagename, "xmpp_%d", send_tb_idx);
  starting_quotes = strchr(script_line, '"');
  NSDL4_PARSING(NULL, NULL, "starting_quotes = [%s], script_line = [%s]", starting_quotes, script_line);


  if(( set_api("ns_xmpp_add_contact","ns_xmpp_send", flow_fp, flow_filename, script_line, outfp, flow_outfile, send_tb_idx)) == NS_PARSE_SCRIPT_ERROR)
    NSDL2_XMPP(NULL, NULL, "Method Called starting_quotes = %p, starting_quotes = %s", starting_quotes, starting_quotes);

  //Set default values here because in below loop all the members reset and creating  problem
  proto_based_init(send_tb_idx, XMPP_REQUEST); 
  requests[send_tb_idx].proto.xmpp.action = NS_XMPP_ADD_CONTACT;  
  while(1)
  {
    NSDL2_XMPP(NULL, NULL, "script_line = %s, send_tb_idx = %d", script_line, send_tb_idx);
    if(get_next_argument(flow_fp, starting_quotes, attribute_name, attribute_value, &closing_quotes, 0) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;
    NSDL2_XMPP(NULL, NULL, "attribute_name = %s, attribute_value = %s", attribute_name, attribute_value);

    if(!strcasecmp(attribute_name, "USER"))
    {
      NSDL2_XMPP(NULL, NULL, "USER [%s] ", attribute_value);
      if(user_flag) {
        SCRIPT_PARSE_ERROR(script_line, "USER can be given once.\n");
      }
      user_flag = 1;

      segment_line(&(requests[send_tb_idx].proto.xmpp.user), attribute_value, 0, script_ln_no, sess_idx, flow_filename);
      NSDL2_XMPP(NULL, NULL, "XMPP: Value of %s = %s ,segment offset = %d", 
                              attribute_name, attribute_value, requests[send_tb_idx].proto.xmpp.group);
    }
    else if(!strcasecmp(attribute_name, "GROUP"))
    {
      NSDL2_XMPP(NULL, NULL, "GROUP [%s] ", attribute_value);
      if(group_flag) {
        SCRIPT_PARSE_ERROR(script_line, "GROUP can be given once.\n");
      }
      group_flag = 1;

      segment_line(&(requests[send_tb_idx].proto.xmpp.group), attribute_value, 0, script_ln_no, sess_idx, flow_filename);
      NSDL2_XMPP(NULL, NULL, "XMPP: Value of %s = %s ,segment offset = %d", 
                              attribute_name, attribute_value, requests[send_tb_idx].proto.xmpp.group);
    }
    else
    {
      SCRIPT_PARSE_ERROR(script_line,"Unhandled argument found [%s]", attribute_name);
    } 

    *starting_quotes = '\0';
    ret = read_till_start_of_next_quotes(flow_fp, flow_file, closing_quotes, &starting_quotes, 0, outfp);
    NSDL2_PARSING(NULL, NULL, "Starting Quotes=[%s], Starting quotes=[%p], Closing_quotes=[%s], Closing_quotes=[%p]," 
                               "after read_till_start_of_next_quotes()",starting_quotes, starting_quotes, closing_quotes, closing_quotes);
    if(ret == NS_PARSE_SCRIPT_ERROR)
    {
      if(starting_quotes == NULL)
        return NS_PARSE_SCRIPT_ERROR;
      else // Means current parameters are processed, so break
        break;
    }

  } //End of while

  if(!user_flag)
    SCRIPT_PARSE_ERROR(script_line,"USER is mandatory attribute", attribute_name);

  return NS_PARSE_SCRIPT_SUCCESS;
}

int ns_parse_xmpp_delete_contact(FILE *flow_fp, FILE *outfp,  char *flow_file, char *flow_outfile, int sess_idx)
{
  char attribute_name[128 + 1];
  char attribute_value[MAX_LINE_LENGTH];
  char pagename[XMPP_MAX_ATTR_LEN + 1];
  char *starting_quotes = NULL;
  int send_tb_idx = 0;
  int ret;
  char *closing_quotes = '\0';
  char user_flag = 0;

  NSDL1_XMPP(NULL, NULL, "Method Called, sess_idx = %d, script_line = [%s]", sess_idx, script_line);

  if (create_requests_table_entry(&send_tb_idx) != SUCCESS) {   // Fill request type inside create table entry
      SCRIPT_PARSE_ERROR(script_line, "Could not create request entry while parsing line ");
  }
  
  //Adding Dummy page name as in ns_xmpp_send() API page name is not given 
  sprintf(pagename, "xmpp_%d", send_tb_idx);
  starting_quotes = strchr(script_line, '"');
  NSDL4_PARSING(NULL, NULL, "starting_quotes = [%s], script_line = [%s]", starting_quotes, script_line);


  if(( set_api("ns_xmpp_delete_contact", "ns_xmpp_send", flow_fp, flow_filename, script_line, outfp, flow_outfile, send_tb_idx)) == NS_PARSE_SCRIPT_ERROR)
    NSDL2_XMPP(NULL, NULL, "Method Called starting_quotes = %p, starting_quotes = %s", starting_quotes, starting_quotes);

  //Set default values here because in below loop all the members reset and creating  problem
  proto_based_init(send_tb_idx, XMPP_REQUEST); 
  requests[send_tb_idx].proto.xmpp.action = NS_XMPP_DELETE_CONTACT;  
  while(1)
  {
    NSDL2_XMPP(NULL, NULL, "script_line = %s, send_tb_idx = %d", script_line, send_tb_idx);
    if(get_next_argument(flow_fp, starting_quotes, attribute_name, attribute_value, &closing_quotes, 0) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;
    NSDL2_XMPP(NULL, NULL, "attribute_name = %s, attribute_value = %s", attribute_name, attribute_value);

    if(!strcasecmp(attribute_name, "USER"))
    {
      NSDL2_XMPP(NULL, NULL, "USER [%s] ", attribute_value);
      if(user_flag) {
        SCRIPT_PARSE_ERROR(script_line, "USER can be given once.\n");
      }
      user_flag = 1;

      segment_line(&(requests[send_tb_idx].proto.xmpp.user), attribute_value, 0, script_ln_no, sess_idx, flow_filename);
      NSDL2_XMPP(NULL, NULL, "XMPP: Value of %s = %s ,segment offset = %d", 
                              attribute_name, attribute_value, requests[send_tb_idx].proto.xmpp.group);
    }
    else
    {
      SCRIPT_PARSE_ERROR(script_line,"Unhandled argument found [%s]", attribute_name);
    } 

    *starting_quotes = '\0';
    ret = read_till_start_of_next_quotes(flow_fp, flow_file, closing_quotes, &starting_quotes, 0, outfp);
    NSDL2_PARSING(NULL, NULL, "Starting Quotes=[%s], Starting quotes=[%p], Closing_quotes=[%s], Closing_quotes=[%p]," 
                               "after read_till_start_of_next_quotes()",starting_quotes, starting_quotes, closing_quotes, closing_quotes);
    if(ret == NS_PARSE_SCRIPT_ERROR)
    {
      if(starting_quotes == NULL)
        return NS_PARSE_SCRIPT_ERROR;
      else // Means current parameters are processed, so break
        break;
    }

  } //End of while

  if(!user_flag)
    SCRIPT_PARSE_ERROR(script_line,"USER is mandatory attribute", attribute_name);

  return NS_PARSE_SCRIPT_SUCCESS;
}

int ns_parse_xmpp_create_group(FILE *flow_fp, FILE *outfp,  char *flow_file, char *flow_outfile, int sess_idx)
{
  char attribute_name[128 + 1];
  char attribute_value[MAX_LINE_LENGTH];
  char pagename[XMPP_MAX_ATTR_LEN + 1];
  char *starting_quotes = NULL;
  int send_tb_idx = 0;
  int ret;
  char *closing_quotes = '\0';
  char group_flag = 0;

  NSDL1_XMPP(NULL, NULL, "Method Called, sess_idx = %d, script_line = [%s]", sess_idx, script_line);

  if (create_requests_table_entry(&send_tb_idx) != SUCCESS) {   // Fill request type inside create table entry
    SCRIPT_PARSE_ERROR(script_line, "Could not create request entry while parsing line ");
  }
  
  //Adding Dummy page name as in ns_xmpp_send() API page name is not given 
  sprintf(pagename, "xmpp_%d", send_tb_idx);
  starting_quotes = strchr(script_line, '"');
  NSDL4_PARSING(NULL, NULL, "starting_quotes = [%s], script_line = [%s]", starting_quotes, script_line);


  if(( set_api("ns_xmpp_create_group","ns_xmpp_send", flow_fp, flow_filename, script_line, outfp, flow_outfile, send_tb_idx)) == NS_PARSE_SCRIPT_ERROR)
    NSDL2_XMPP(NULL, NULL, "Method Called starting_quotes = %p, starting_quotes = %s", starting_quotes, starting_quotes);

  //Set default values here because in below loop all the members reset and creating  problem
  proto_based_init(send_tb_idx, XMPP_REQUEST); 
  requests[send_tb_idx].proto.xmpp.action = NS_XMPP_CREATE_GROUP;  
  while(1)
  {
    NSDL2_XMPP(NULL, NULL, "script_line = %s, send_tb_idx = %d", script_line, send_tb_idx);
    if(get_next_argument(flow_fp, starting_quotes, attribute_name, attribute_value, &closing_quotes, 0) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;
    NSDL2_XMPP(NULL, NULL, "attribute_name = %s, attribute_value = %s", attribute_name, attribute_value);

    if(!strcasecmp(attribute_name, "GROUP"))
    {
      NSDL2_XMPP(NULL, NULL, "GROUP [%s] ", attribute_value);
      if(group_flag) {
        SCRIPT_PARSE_ERROR(script_line, "GROUP can be given once.\n");
      }
      group_flag = 1;

      segment_line(&(requests[send_tb_idx].proto.xmpp.group), attribute_value, 0, script_ln_no, sess_idx, flow_filename);
      NSDL2_XMPP(NULL, NULL, "XMPP: Value of %s = %s ,segment offset = %d", 
                              attribute_name, attribute_value, requests[send_tb_idx].proto.xmpp.group);
    }
    else
    {
      SCRIPT_PARSE_ERROR(script_line,"Unhandled argument found [%s]", attribute_name);
    } 

    *starting_quotes = '\0';
    ret = read_till_start_of_next_quotes(flow_fp, flow_file, closing_quotes, &starting_quotes, 0, outfp);
    NSDL2_PARSING(NULL, NULL, "Starting Quotes=[%s], Starting quotes=[%p], Closing_quotes=[%s], Closing_quotes=[%p]," 
                               "after read_till_start_of_next_quotes()",starting_quotes, starting_quotes, closing_quotes, closing_quotes);
    if(ret == NS_PARSE_SCRIPT_ERROR)
    {
      if(starting_quotes == NULL)
        return NS_PARSE_SCRIPT_ERROR;
      else // Means current parameters are processed, so break
        break;
    }

  } //End of while

  if(!group_flag)
    SCRIPT_PARSE_ERROR(script_line,"GROUP is mandatory attribute", attribute_name);

  return NS_PARSE_SCRIPT_SUCCESS;
}

int ns_parse_xmpp_delete_group(FILE *flow_fp, FILE *outfp,  char *flow_file, char *flow_outfile, int sess_idx)
{
  char attribute_name[128 + 1];
  char attribute_value[MAX_LINE_LENGTH];
  char pagename[XMPP_MAX_ATTR_LEN + 1];
  char *starting_quotes = NULL;
  int send_tb_idx = 0;
  int ret;
  char *closing_quotes = '\0';
  char group_flag = 0;

  NSDL1_XMPP(NULL, NULL, "Method Called, sess_idx = %d, script_line = [%s]", sess_idx, script_line);

  if (create_requests_table_entry(&send_tb_idx) != SUCCESS) {   // Fill request type inside create table entry
    SCRIPT_PARSE_ERROR(script_line, "Could not create request entry while parsing line ");
  }
  
  //Adding Dummy page name as in ns_xmpp_send() API page name is not given 
  sprintf(pagename, "xmpp_%d", send_tb_idx);
  starting_quotes = strchr(script_line, '"');
  NSDL4_PARSING(NULL, NULL, "starting_quotes = [%s], script_line = [%s]", starting_quotes, script_line);


  if(( set_api("ns_xmpp_delete_group","ns_xmpp_send", flow_fp, flow_filename, script_line, outfp, flow_outfile, send_tb_idx)) == NS_PARSE_SCRIPT_ERROR)
    NSDL2_XMPP(NULL, NULL, "Method Called starting_quotes = %p, starting_quotes = %s", starting_quotes, starting_quotes);

  //Set default values here because in below loop all the members reset and creating  problem
  proto_based_init(send_tb_idx, XMPP_REQUEST); 
  requests[send_tb_idx].proto.xmpp.action = NS_XMPP_DELETE_GROUP;  
  while(1)
  {
    NSDL2_XMPP(NULL, NULL, "script_line = %s, send_tb_idx = %d", script_line, send_tb_idx);
    if(get_next_argument(flow_fp, starting_quotes, attribute_name, attribute_value, &closing_quotes, 0) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;
    NSDL2_XMPP(NULL, NULL, "attribute_name = %s, attribute_value = %s", attribute_name, attribute_value);

    if(!strcasecmp(attribute_name, "GROUP"))
    {
      NSDL2_XMPP(NULL, NULL, "GROUP [%s] ", attribute_value);
      if(group_flag) {
        SCRIPT_PARSE_ERROR(script_line, "GROUP can be given once.\n");
      }
      group_flag = 1;

      segment_line(&(requests[send_tb_idx].proto.xmpp.group), attribute_value, 0, script_ln_no, sess_idx, flow_filename);
      NSDL2_XMPP(NULL, NULL, "XMPP: Value of %s = %s ,segment offset = %d", 
                              attribute_name, attribute_value, requests[send_tb_idx].proto.xmpp.group);
    }
    else
    {
      SCRIPT_PARSE_ERROR(script_line,"Unhandled argument found [%s]", attribute_name);
    } 

    *starting_quotes = '\0';
    ret = read_till_start_of_next_quotes(flow_fp, flow_file, closing_quotes, &starting_quotes, 0, outfp);
    NSDL2_PARSING(NULL, NULL, "Starting Quotes=[%s], Starting quotes=[%p], Closing_quotes=[%s], Closing_quotes=[%p]," 
                               "after read_till_start_of_next_quotes()",starting_quotes, starting_quotes, closing_quotes, closing_quotes);
    if(ret == NS_PARSE_SCRIPT_ERROR)
    {
      if(starting_quotes == NULL)
        return NS_PARSE_SCRIPT_ERROR;
      else // Means current parameters are processed, so break
        break;
    }

  } //End of while

  if(!group_flag)
    SCRIPT_PARSE_ERROR(script_line,"GROUP is mandatory attribute", attribute_name);

  return NS_PARSE_SCRIPT_SUCCESS;
}

int ns_parse_xmpp_join_group(FILE *flow_fp, FILE *outfp,  char *flow_file, char *flow_outfile, int sess_idx)
{
  char attribute_name[128 + 1];
  char attribute_value[MAX_LINE_LENGTH];
  char pagename[XMPP_MAX_ATTR_LEN + 1];
  char *starting_quotes = NULL;
  int send_tb_idx = 0;
  int ret;
  char *closing_quotes = '\0';
  char group_flag = 0;

  NSDL1_XMPP(NULL, NULL, "Method Called, sess_idx = %d, script_line = [%s]", sess_idx, script_line);

  if (create_requests_table_entry(&send_tb_idx) != SUCCESS) {   // Fill request type inside create table entry
    SCRIPT_PARSE_ERROR(script_line, "Could not create request entry while parsing line ");
  }
  
  //Adding Dummy page name as in ns_xmpp_send() API page name is not given 
  sprintf(pagename, "xmpp_%d", send_tb_idx);
  starting_quotes = strchr(script_line, '"');
  NSDL4_PARSING(NULL, NULL, "starting_quotes = [%s], script_line = [%s]", starting_quotes, script_line);


  if(( set_api("ns_xmpp_join_group","ns_xmpp_send", flow_fp, flow_filename, script_line, outfp, flow_outfile, send_tb_idx)) == NS_PARSE_SCRIPT_ERROR)
    NSDL2_XMPP(NULL, NULL, "Method Called starting_quotes = %p, starting_quotes = %s", starting_quotes, starting_quotes);

  //Set default values here because in below loop all the members reset and creating  problem
  proto_based_init(send_tb_idx, XMPP_REQUEST); 
  requests[send_tb_idx].proto.xmpp.action = NS_XMPP_JOIN_GROUP;  
  while(1)
  {
    NSDL2_XMPP(NULL, NULL, "script_line = %s, send_tb_idx = %d", script_line, send_tb_idx);
    if(get_next_argument(flow_fp, starting_quotes, attribute_name, attribute_value, &closing_quotes, 0) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;
    NSDL2_XMPP(NULL, NULL, "attribute_name = %s, attribute_value = %s", attribute_name, attribute_value);

    if(!strcasecmp(attribute_name, "GROUP"))
    {
      NSDL2_XMPP(NULL, NULL, "GROUP [%s] ", attribute_value);
      if(group_flag) {
        SCRIPT_PARSE_ERROR(script_line, "GROUP can be given once.\n");
      }
      group_flag = 1;

      segment_line(&(requests[send_tb_idx].proto.xmpp.group), attribute_value, 0, script_ln_no, sess_idx, flow_filename);
      NSDL2_XMPP(NULL, NULL, "XMPP: Value of %s = %s ,segment offset = %d", 
                              attribute_name, attribute_value, requests[send_tb_idx].proto.xmpp.group);
    }
    else
    {
      SCRIPT_PARSE_ERROR(script_line,"Unhandled argument found [%s]", attribute_name);
    } 

    *starting_quotes = '\0';
    ret = read_till_start_of_next_quotes(flow_fp, flow_file, closing_quotes, &starting_quotes, 0, outfp);
    NSDL2_PARSING(NULL, NULL, "Starting Quotes=[%s], Starting quotes=[%p], Closing_quotes=[%s], Closing_quotes=[%p]," 
                               "after read_till_start_of_next_quotes()",starting_quotes, starting_quotes, closing_quotes, closing_quotes);
    if(ret == NS_PARSE_SCRIPT_ERROR)
    {
      if(starting_quotes == NULL)
        return NS_PARSE_SCRIPT_ERROR;
      else // Means current parameters are processed, so break
        break;
    }

  } //End of while

  if(!group_flag)
    SCRIPT_PARSE_ERROR(script_line,"GROUP is mandatory attribute", attribute_name);

  return NS_PARSE_SCRIPT_SUCCESS;
}

int ns_parse_xmpp_leave_group(FILE *flow_fp, FILE *outfp,  char *flow_file, char *flow_outfile, int sess_idx)
{
  char attribute_name[128 + 1];
  char attribute_value[MAX_LINE_LENGTH];
  char pagename[XMPP_MAX_ATTR_LEN + 1];
  char *starting_quotes = NULL;
  int send_tb_idx = 0;
  int ret;
  char *closing_quotes = '\0';
  char group_flag = 0;

  NSDL1_XMPP(NULL, NULL, "Method Called, sess_idx = %d, script_line = [%s]", sess_idx, script_line);

  if (create_requests_table_entry(&send_tb_idx) != SUCCESS) {   // Fill request type inside create table entry
    SCRIPT_PARSE_ERROR(script_line, "Could not create request entry while parsing line ");
  }
  
  //Adding Dummy page name as in ns_xmpp_send() API page name is not given 
  sprintf(pagename, "xmpp_%d", send_tb_idx);
  starting_quotes = strchr(script_line, '"');
  NSDL4_PARSING(NULL, NULL, "starting_quotes = [%s], script_line = [%s]", starting_quotes, script_line);


  if(( set_api("ns_xmpp_leave_group","ns_xmpp_send", flow_fp, flow_filename, script_line, outfp, flow_outfile, send_tb_idx)) == NS_PARSE_SCRIPT_ERROR)
    NSDL2_XMPP(NULL, NULL, "Method Called starting_quotes = %p, starting_quotes = %s", starting_quotes, starting_quotes);

  //Set default values here because in below loop all the members reset and creating  problem
  proto_based_init(send_tb_idx, XMPP_REQUEST); 
  requests[send_tb_idx].proto.xmpp.action = NS_XMPP_LEAVE_GROUP;  
  while(1)
  {
    NSDL2_XMPP(NULL, NULL, "script_line = %s, send_tb_idx = %d", script_line, send_tb_idx);
    if(get_next_argument(flow_fp, starting_quotes, attribute_name, attribute_value, &closing_quotes, 0) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;
    NSDL2_XMPP(NULL, NULL, "attribute_name = %s, attribute_value = %s", attribute_name, attribute_value);

    if(!strcasecmp(attribute_name, "GROUP"))
    {
      NSDL2_XMPP(NULL, NULL, "GROUP [%s] ", attribute_value);
      if(group_flag) {
        SCRIPT_PARSE_ERROR(script_line, "GROUP can be given once.\n");
      }
      group_flag = 1;

      segment_line(&(requests[send_tb_idx].proto.xmpp.group), attribute_value, 0, script_ln_no, sess_idx, flow_filename);
      NSDL2_XMPP(NULL, NULL, "XMPP: Value of %s = %s ,segment offset = %d", 
                              attribute_name, attribute_value, requests[send_tb_idx].proto.xmpp.group);
    }
    else
    {
      SCRIPT_PARSE_ERROR(script_line,"Unhandled argument found [%s]", attribute_name);
    } 

    *starting_quotes = '\0';
    ret = read_till_start_of_next_quotes(flow_fp, flow_file, closing_quotes, &starting_quotes, 0, outfp);
    NSDL2_PARSING(NULL, NULL, "Starting Quotes=[%s], Starting quotes=[%p], Closing_quotes=[%s], Closing_quotes=[%p]," 
                               "after read_till_start_of_next_quotes()",starting_quotes, starting_quotes, closing_quotes, closing_quotes);
    if(ret == NS_PARSE_SCRIPT_ERROR)
    {
      if(starting_quotes == NULL)
        return NS_PARSE_SCRIPT_ERROR;
      else // Means current parameters are processed, so break
        break;
    }

  } //End of while

  if(!group_flag)
    SCRIPT_PARSE_ERROR(script_line,"GROUP is mandatory attribute", attribute_name);

  return NS_PARSE_SCRIPT_SUCCESS;
}

int ns_parse_xmpp_add_member(FILE *flow_fp, FILE *outfp,  char *flow_file, char *flow_outfile, int sess_idx)
{
  char attribute_name[128 + 1];
  char attribute_value[MAX_LINE_LENGTH];
  char pagename[XMPP_MAX_ATTR_LEN + 1];
  char *starting_quotes = NULL;
  int send_tb_idx = 0;
  int ret;
  char *closing_quotes = '\0';
  char group_flag = 0;
  char user_flag = 0;

  NSDL1_XMPP(NULL, NULL, "Method Called, sess_idx = %d, script_line = [%s]", sess_idx, script_line);

  if (create_requests_table_entry(&send_tb_idx) != SUCCESS) {   // Fill request type inside create table entry
    SCRIPT_PARSE_ERROR(script_line, "Could not create request entry while parsing line ");
  }
  
  //Adding Dummy page name as in ns_xmpp_send() API page name is not given 
  sprintf(pagename, "xmpp_%d", send_tb_idx);
  starting_quotes = strchr(script_line, '"');
  NSDL4_PARSING(NULL, NULL, "starting_quotes = [%s], script_line = [%s]", starting_quotes, script_line);


  if(( set_api("ns_xmpp_add_member","ns_xmpp_send", flow_fp, flow_filename, script_line, outfp, flow_outfile, send_tb_idx)) == NS_PARSE_SCRIPT_ERROR)
    NSDL2_XMPP(NULL, NULL, "Method Called starting_quotes = %p, starting_quotes = %s", starting_quotes, starting_quotes);

  //Set default values here because in below loop all the members reset and creating  problem
  proto_based_init(send_tb_idx, XMPP_REQUEST); 
  requests[send_tb_idx].proto.xmpp.action = NS_XMPP_ADD_MEMBER;  
  while(1)
  {
    NSDL2_XMPP(NULL, NULL, "script_line = %s, send_tb_idx = %d", script_line, send_tb_idx);
    if(get_next_argument(flow_fp, starting_quotes, attribute_name, attribute_value, &closing_quotes, 0) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;
    NSDL2_XMPP(NULL, NULL, "attribute_name = %s, attribute_value = %s", attribute_name, attribute_value);

    if(!strcasecmp(attribute_name, "USER"))
    {
      NSDL2_XMPP(NULL, NULL, "USER [%s] ", attribute_value);
      if(user_flag) {
        SCRIPT_PARSE_ERROR(script_line, "USER can be given once.\n");
      }
      user_flag = 1;

      segment_line(&(requests[send_tb_idx].proto.xmpp.user), attribute_value, 0, script_ln_no, sess_idx, flow_filename);
      NSDL2_XMPP(NULL, NULL, "XMPP: Value of %s = %s ,segment offset = %d", 
                              attribute_name, attribute_value, requests[send_tb_idx].proto.xmpp.group);
    }
    else if(!strcasecmp(attribute_name, "GROUP"))
    {
      NSDL2_XMPP(NULL, NULL, "GROUP [%s] ", attribute_value);
      if(group_flag) {
        SCRIPT_PARSE_ERROR(script_line, "GROUP can be given once.\n");
      }
      group_flag = 1;

      segment_line(&(requests[send_tb_idx].proto.xmpp.group), attribute_value, 0, script_ln_no, sess_idx, flow_filename);
      NSDL2_XMPP(NULL, NULL, "XMPP: Value of %s = %s ,segment offset = %d", 
                              attribute_name, attribute_value, requests[send_tb_idx].proto.xmpp.group);
    }
    else
    {
      SCRIPT_PARSE_ERROR(script_line,"Unhandled argument found [%s]", attribute_name);
    } 

    *starting_quotes = '\0';
    ret = read_till_start_of_next_quotes(flow_fp, flow_file, closing_quotes, &starting_quotes, 0, outfp);
    NSDL2_PARSING(NULL, NULL, "Starting Quotes=[%s], Starting quotes=[%p], Closing_quotes=[%s], Closing_quotes=[%p]," 
                               "after read_till_start_of_next_quotes()",starting_quotes, starting_quotes, closing_quotes, closing_quotes);
    if(ret == NS_PARSE_SCRIPT_ERROR)
    {
      if(starting_quotes == NULL)
        return NS_PARSE_SCRIPT_ERROR;
      else // Means current parameters are processed, so break
        break;
    }

  } //End of while

  if(!user_flag)
    SCRIPT_PARSE_ERROR(script_line,"GROUP is mandatory attribute", attribute_name);

  return NS_PARSE_SCRIPT_SUCCESS;
}

int ns_parse_xmpp_delete_member(FILE *flow_fp, FILE *outfp,  char *flow_file, char *flow_outfile, int sess_idx)
{
  char attribute_name[128 + 1];
  char attribute_value[MAX_LINE_LENGTH];
  char pagename[XMPP_MAX_ATTR_LEN + 1];
  char *starting_quotes = NULL;
  int send_tb_idx = 0;
  int ret;
  char *closing_quotes = '\0';
  char group_flag = 0;
  char user_flag = 0;

  NSDL1_XMPP(NULL, NULL, "Method Called, sess_idx = %d, script_line = [%s]", sess_idx, script_line);

  if (create_requests_table_entry(&send_tb_idx) != SUCCESS) {   // Fill request type inside create table entry
    SCRIPT_PARSE_ERROR(script_line, "Could not create request entry while parsing line ");
  }
  
  //Adding Dummy page name as in ns_xmpp_send() API page name is not given 
  sprintf(pagename, "xmpp_%d", send_tb_idx);
  starting_quotes = strchr(script_line, '"');
  NSDL4_PARSING(NULL, NULL, "starting_quotes = [%s], script_line = [%s]", starting_quotes, script_line);


  if(( set_api("ns_xmpp_delete_member","ns_xmpp_send", flow_fp, flow_filename, script_line, outfp, flow_outfile, send_tb_idx)) == NS_PARSE_SCRIPT_ERROR)
    NSDL2_XMPP(NULL, NULL, "Method Called starting_quotes = %p, starting_quotes = %s", starting_quotes, starting_quotes);

  //Set default values here because in below loop all the members reset and creating  problem
  proto_based_init(send_tb_idx, XMPP_REQUEST); 
  requests[send_tb_idx].proto.xmpp.action = NS_XMPP_DELETE_MEMBER;  
  while(1)
  {
    NSDL2_XMPP(NULL, NULL, "script_line = %s, send_tb_idx = %d", script_line, send_tb_idx);
    if(get_next_argument(flow_fp, starting_quotes, attribute_name, attribute_value, &closing_quotes, 0) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;
    NSDL2_XMPP(NULL, NULL, "attribute_name = %s, attribute_value = %s", attribute_name, attribute_value);

    if(!strcasecmp(attribute_name, "USER"))
    {
      NSDL2_XMPP(NULL, NULL, "USER [%s] ", attribute_value);
      if(user_flag) {
        SCRIPT_PARSE_ERROR(script_line, "USER can be given once.\n");
      }
      user_flag = 1;

      segment_line(&(requests[send_tb_idx].proto.xmpp.user), attribute_value, 0, script_ln_no, sess_idx, flow_filename);
      NSDL2_XMPP(NULL, NULL, "XMPP: Value of %s = %s ,segment offset = %d", 
                              attribute_name, attribute_value, requests[send_tb_idx].proto.xmpp.group);
    }
    else if(!strcasecmp(attribute_name, "GROUP"))
    {
      NSDL2_XMPP(NULL, NULL, "GROUP [%s] ", attribute_value);
      if(group_flag) {
        SCRIPT_PARSE_ERROR(script_line, "GROUP can be given once.\n");
      }
      group_flag = 1;

      segment_line(&(requests[send_tb_idx].proto.xmpp.group), attribute_value, 0, script_ln_no, sess_idx, flow_filename);
      NSDL2_XMPP(NULL, NULL, "XMPP: Value of %s = %s ,segment offset = %d", 
                              attribute_name, attribute_value, requests[send_tb_idx].proto.xmpp.group);
    }
    else
    {
      SCRIPT_PARSE_ERROR(script_line,"Unhandled argument found [%s]", attribute_name);
    } 

    *starting_quotes = '\0';
    ret = read_till_start_of_next_quotes(flow_fp, flow_file, closing_quotes, &starting_quotes, 0, outfp);
    NSDL2_PARSING(NULL, NULL, "Starting Quotes=[%s], Starting quotes=[%p], Closing_quotes=[%s], Closing_quotes=[%p]," 
                               "after read_till_start_of_next_quotes()",starting_quotes, starting_quotes, closing_quotes, closing_quotes);
    if(ret == NS_PARSE_SCRIPT_ERROR)
    {
      if(starting_quotes == NULL)
        return NS_PARSE_SCRIPT_ERROR;
      else // Means current parameters are processed, so break
        break;
    }

  } //End of while

  if(!user_flag)
    SCRIPT_PARSE_ERROR(script_line,"GROUP is mandatory attribute", attribute_name);

  return NS_PARSE_SCRIPT_SUCCESS;
}



