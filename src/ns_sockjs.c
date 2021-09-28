/************************************************************************************
 * Name	     : ns_sockjs.c 
 * Purpose   : This file contains functions related to SockJs 
 * Author(s) : Vikas Kumar Verma
 * Date      : 18 July 2018
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
#include "ns_common.h"
#include "ns_global_settings.h"
#include "ns_log.h"
#include "ns_script_parse.h"
#include "ns_http_process_resp.h"
#include "ns_log_req_rep.h"
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
#include "ns_group_data.h"
#include "ns_sockjs.h"

/*********Global Variables****************/
int total_sockjs_connect_entries = 0;
int total_sockjs_close_entries = 0;
int max_sockjs_connect_entries = 0;
int max_sockjs_close_entries = 0;
int max_sockjs_conn = 0;     //It is used for malloc sockjs_conn
unsigned short int sockjs_idx_list[65535];

sockjs_connect_table *g_sockjs_connect;
sockjs_close_table *g_sockjs_close;
sockjs_close_table_shr *g_sockjs_close_shr;

/* 1) ID should be numeric
   2) ID should be unique */
static int validate_sockjs_id(int sockjs_id, int url_idx)
{
  int i;
  NSDL2_PARSING(NULL, NULL, "Method Called, sockjs_id = %d", sockjs_id);

  if(sockjs_id < 1)
    SCRIPT_PARSE_ERROR(script_line, "ID should be greater than 0, ID=%d", sockjs_id);
  //checking uniquness of id
  for(i = url_idx - 2 ; i >= 0 ; i--) {
    if(requests[i].proto.http.sockjs.conn_id == sockjs_id)
      return -2; 
  } 
  return 0;
}

//ID should be unique
static int is_sockjs_conid_exist(char *sockjs_id)
{
  int i;
  int num_sockjs_id = atoi(sockjs_id);

  NSDL2_PARSING(NULL, NULL, "Method Called, sockjs_id = %s ", sockjs_id);

  if(!num_sockjs_id)
  {
    SCRIPT_PARSE_ERROR(script_line, "ID should be greater than 0, ID = %s ", num_sockjs_id);
    return -1;
  }

  //checking uniquness of id
  for(i = 0; i < total_request_entries; i++) 
  {
    if(requests[i].proto.http.sockjs.conn_id == num_sockjs_id)
    return i; //conid exist 
  }
  return -2;
}

int create_sockjs_connect_table_entry(int *row_num)
{
  NSDL2_PARSING(NULL, NULL, "Method called");

  if (total_sockjs_connect_entries == max_sockjs_connect_entries)
  {
    MY_REALLOC_EX(g_sockjs_connect, (max_sockjs_connect_entries + SOCKJS_CONNECT_ENTRIES) * sizeof(sockjs_connect_table), max_sockjs_connect_entries * sizeof(sockjs_connect_table), "g_sockjs_connect", -1);
    if (!g_sockjs_connect)
    {
      NSTL1_OUT(NULL, NULL,"create_sockjs_connect_table_entry(): Error allocating more memory for connect entries\n");
      return(FAILURE);
    }
    else
      max_sockjs_connect_entries += SOCKJS_CONNECT_ENTRIES;
  }

  *row_num =  total_sockjs_connect_entries++; //Increment it when connect api called

  g_sockjs_connect[*row_num].conn_id = -1;

  return(SUCCESS);
}

int create_sockjs_close_table_entry(int *row_num)
{
  NSDL2_PARSING(NULL, NULL, "Method called");

  if (total_sockjs_close_entries == max_sockjs_close_entries)
  {
    MY_REALLOC_EX(g_sockjs_close, (max_sockjs_close_entries + SOCKJS_CLOSE_ENTRIES) * sizeof(sockjs_close_table), max_sockjs_close_entries * sizeof(sockjs_close_table), "g_sockjs_close", -1);
    if (!g_sockjs_close)
    {
      NSTL1_OUT(NULL, NULL,"create_sockjs_close_table_entry(): Error allocating more memory for close entries\n");
      return(FAILURE);
    }
    else
      max_sockjs_close_entries += SOCKJS_CLOSE_ENTRIES;
  }

  *row_num =  total_sockjs_close_entries++; //Increment it when close api called

  g_sockjs_close[*row_num].conn_id = -1;

  return(SUCCESS);
}

static int set_api(char *api_to_run, FILE *flow_fp, char *flow_file, char *line_ptr, FILE *outfp,  char *flow_outfile, int tb_idx)
{
  char *start_idx;
  char *end_idx;
  char str[MAX_LINE_LENGTH + 1];
  int len ;

  NSDL1_PARSING(NULL, NULL ,"Method Called. tb_idx = %d", tb_idx);
  start_idx = line_ptr;
  NSDL2_PARSING(NULL, NULL, "start_idx = [%s]", start_idx);
  end_idx = strstr(line_ptr, api_to_run);
  if(end_idx == NULL)
    SCRIPT_PARSE_ERROR(script_line, "Invalid Format or API Name not found in Line");

  len = end_idx - start_idx;
  strncpy(str, start_idx, len); //Copying the return value first
  str[len] = '\0';

  NSDL2_PARSING(NULL, NULL,"Before sprintf str is = %s ", str);
  sprintf(str, "%s %s(%d); ", str, api_to_run, tb_idx);
  NSDL2_PARSING(NULL, NULL," final str is = %s ", str);
  if(write_line(outfp, str, 1) == NS_PARSE_SCRIPT_ERROR)
    SCRIPT_PARSE_ERROR(api_to_run, "Error Writing in File ");

  return NS_PARSE_SCRIPT_SUCCESS;
}

//Parse ns_sockjs_connect API
int parse_ns_sockjs_connect(FILE *flow_fp, FILE *outfp,  char *flow_file, char *flow_outfile, int sess_idx)
{
  int ret;
  char pagename[MAX_LINE_LENGTH + 1];
  char attribute_value[MAX_LINE_LENGTH];
  char attribute_name[128 + 1];
  char serverId[WORD_SIZE] = "000";
  char sessionId[WORD_SIZE] = "04f243afa8654c969f841f8604283f21";
  char header_buf[MAX_REQUEST_BUF_LENGTH];
  char *closing_quotes;
  char *starting_quotes;
  char url_exists = 0;
  char id_exists = 0;
  int id = 0;
  int connect_tb_idx = 0;
  int url_idx;
  char url[MAX_URL_LENGTH] = "";             // Copy URL
  static int cur_page_index = -1;             //For keeping track of multiple main urls
  static int duplicate_id_flag = -1;          //For keeping track of multiple ids

  NSDL2_PARSING(NULL, NULL, "Method Called, sess_idx = %d", sess_idx);

  //Adding Dummy page name as page name is not given in ns_sockjs_connect() 
  sprintf(pagename, "sockJsConnect_%d", web_url_page_id);
  starting_quotes = strchr(script_line, '"');

  if(starting_quotes == NULL)
    SCRIPT_PARSE_ERROR(script_line, "Invalid Syntax. \n E.g., ns_sockjs_connect(\"ID=1\"");

  NSDL4_PARSING(NULL, NULL, "pagename - [%s], starting_quotes = [%s]", pagename, starting_quotes);
  //Write function script_line read_till_start_of_next_quotes for checking ( and whitespaces
  if((parse_and_set_pagename("ns_sockjs_connect", "ns_web_url", flow_fp, flow_file, script_line, outfp, flow_outfile, sess_idx, &starting_quotes, pagename)) == NS_PARSE_SCRIPT_ERROR)
    return NS_PARSE_SCRIPT_ERROR;

  closing_quotes = starting_quotes;
  header_buf[0] = '\0';
 
  if(init_url(&url_idx, 0, flow_file, HTTP_NO_INLINE) == NS_PARSE_SCRIPT_ERROR)
    return NS_PARSE_SCRIPT_ERROR;
    
  //line_ptr will point to the starting of the argument quotes
  while(1)
  {
    if(get_next_argument(flow_fp, starting_quotes, attribute_name, attribute_value, &closing_quotes, 0) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;

    if (strcasecmp(attribute_name, "URL") == 0)
    {
      NSDL2_PARSING(NULL, NULL, "URL [%s] found at script_line %d", attribute_value, script_ln_no);
      NSDL2_PARSING(NULL, NULL, "cur_page_index=%d, g_cur_page=%d", cur_page_index, g_cur_page);
      if(cur_page_index != -1)
      {
        if(cur_page_index == g_cur_page)
          SCRIPT_PARSE_ERROR(script_line, "Cannot have two main url in a page");
      }
      cur_page_index = g_cur_page;
      strncpy(url, attribute_value, MAX_URL_LENGTH);
      //TODO what is info any why we are adding it here. can we add it in script?
      strncat(attribute_value, "/info", MAX_LINE_LENGTH);
 
      if(set_url(attribute_value, flow_file, sess_idx, url_idx, 0, 0, "ns_sockjs_connect") == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
      url_exists = 1;
     
      requests[url_idx].proto.http.sockjs.conn_id = id;
    }
    else if (!strcasecmp(attribute_name, "ID"))
    {
      NSDL2_PARSING(NULL, NULL, "ID =  [%s] ", attribute_value);
      if(duplicate_id_flag != -1)
      {
        if(duplicate_id_flag == g_cur_page)
          SCRIPT_PARSE_ERROR(script_line, "Cannot have two sockjs id same in a page");
      }
      duplicate_id_flag = g_cur_page;
      id = atoi(attribute_value);
      id_exists = 1;
    }
    else if(!strcasecmp(attribute_name, "serverId"))
    {
      NSDL2_PARSING(NULL, NULL, "url_idx = %d, attribute_name = %s, attribute_value = %s", url_idx, attribute_name, attribute_value);
      snprintf(serverId, WORD_SIZE, "%s", attribute_value);
    }
    else if(!strcasecmp(attribute_name, "sessionId"))
    {
      NSDL2_PARSING(NULL, NULL, "url_idx = %d, attribute_name = %s, attribute_value = %s", url_idx, attribute_name, attribute_value);
      snprintf(sessionId, WORD_SIZE, "%s", attribute_value);
    }
    //Extract Header
    else if (strcasecmp(attribute_name, "HEADER") == 0)
    {
      NSDL2_PARSING(NULL, NULL, "Header %s found at script_line %d\n", attribute_value, script_ln_no);
      if(set_headers(flow_fp, flow_file, attribute_value, header_buf, sess_idx, url_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }
    ret = read_till_start_of_next_quotes(flow_fp, flow_file, closing_quotes, &starting_quotes, 0, outfp);
    if (ret == NS_PARSE_SCRIPT_ERROR)
    {
      NSDL2_PARSING(NULL, NULL, "Next attribute is not found");
      break;
    } 
 
  }

  if(!url_exists)
    SCRIPT_PARSE_ERROR(script_line, "URL doesn't exists");

  if(!id_exists)
    SCRIPT_PARSE_ERROR(script_line, "ID doesn't exists");

  strcat(header_buf, "\r\n");
  NSDL2_PARSING(NULL, NULL, "Segmenting header_buf = [%s]", header_buf);
  segment_line(&(requests[url_idx].proto.http.hdrs), header_buf, 0, script_ln_no, sess_idx, flow_file);

  //xhr_streaming request
  if(init_url(&url_idx, 1, flow_file, HTTP_NO_INLINE) == NS_PARSE_SCRIPT_ERROR)
    return NS_PARSE_SCRIPT_ERROR;

  snprintf(attribute_value, MAX_LINE_LENGTH, "%s/%s/%s/xhr_streaming", url, serverId, sessionId);
  strncpy(url, attribute_value, MAX_URL_LENGTH);
  if(set_url(attribute_value, flow_file, sess_idx, url_idx, 1, 1, "ns_sockjs_connect") == NS_PARSE_SCRIPT_ERROR)
    return NS_PARSE_SCRIPT_ERROR; 

  requests[url_idx].proto.http.http_method = HTTP_METHOD_POST; // Set HTTP Method
  requests[url_idx].proto.http.http_method_idx = find_http_method_idx("POST");
 
  //ID validation  
  ret = validate_sockjs_id(id, url_idx);
  if(ret == -1){
    SCRIPT_PARSE_ERROR(script_line, "Invalid ID");
  }
  else if(ret == -2){
    SCRIPT_PARSE_ERROR(script_line, "ID should be unique in script");
  }

  if(create_sockjs_connect_table_entry(&connect_tb_idx) != SUCCESS)
    SCRIPT_PARSE_ERROR(script_line, "Unable to create sockjs_connect_table");

  requests[url_idx].proto.http.sockjs.conn_id = id;
  sockjs_idx_list[requests[url_idx].proto.http.sockjs.conn_id] = max_sockjs_conn;

  g_sockjs_connect[connect_tb_idx].conn_id = id;
  strncpy(g_sockjs_connect[connect_tb_idx].url, url, MAX_URL_LENGTH);
  NSDL2_PARSING(NULL, NULL, "SockJs Id = %d, max_sockjs_conn = %d", requests[url_idx].proto.http.sockjs.conn_id, max_sockjs_conn);

  max_sockjs_conn++;

  NSDL2_PARSING(NULL, NULL, "Segmenting header_buf = [%s]", header_buf);
  segment_line(&(requests[url_idx].proto.http.hdrs), header_buf, 0, script_ln_no, sess_idx, flow_file);

  NSDL2_PARSING(NULL, NULL, "Exiting Method");
  return NS_PARSE_SCRIPT_SUCCESS;
}

//Parse ns_sockjs_send API
int parse_ns_sockjs_send(FILE *flow_fp, FILE *outfp,  char *flow_file, char *flow_outfile, int sess_idx)
{
  int ret, i;
  char *ptr = NULL;
  char pagename[MAX_LINE_LENGTH + 1];
  char attribute_value[MAX_LINE_LENGTH];
  char attribute_name[128 + 1];
  char header_buf[MAX_REQUEST_BUF_LENGTH];
  char *closing_quotes;
  char *starting_quotes;
  char id_exists = 0;
  char body_exists = 0;
  int post_idx = 0;
  int url_idx;
  int id = 0;
  char url[MAX_LINE_LENGTH] = "";             // Copy URL

  NSDL2_PARSING(NULL, NULL, "Method Called, sess_idx = %d", sess_idx);

  //Adding Dummy page name as page name is not given in ns_sockjs_connect() 
  sprintf(pagename, "sockJsSend_%d", web_url_page_id);
  starting_quotes = strchr(script_line, '"');

  if(starting_quotes == NULL)
    SCRIPT_PARSE_ERROR(script_line, "Invalid Syntax. \n E.g., ns_sockjs_send(\"ID=1\"");

  NSDL4_PARSING(NULL, NULL, "pagename - [%s], starting_quotes = [%s]", pagename, starting_quotes);
  //Write function script_line read_till_start_of_next_quotes for checking ( and whitespaces
  if((parse_and_set_pagename("ns_sockjs_send", "ns_web_url", flow_fp, flow_file, script_line, outfp, flow_outfile, sess_idx, &starting_quotes, pagename)) == NS_PARSE_SCRIPT_ERROR)
    return NS_PARSE_SCRIPT_ERROR;

  closing_quotes = starting_quotes;

  header_buf[0] = '\0';
 
  if(init_url(&url_idx, 0, flow_file, HTTP_NO_INLINE) == NS_PARSE_SCRIPT_ERROR)
    return NS_PARSE_SCRIPT_ERROR;

  // line_ptr will point to the starting of the argument quotes
  while(1)
  {
    if(get_next_argument(flow_fp, starting_quotes, attribute_name, attribute_value, &closing_quotes, 0) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;

    if(!strcasecmp(attribute_name, "ID"))
    {
      NSDL2_PARSING(NULL, NULL, "ID [%s]", attribute_value);
      //validating id
      ret = is_sockjs_conid_exist(attribute_value);
      if(ret < 0)
      {
        SCRIPT_PARSE_ERROR(script_line, "ID [%s] is invalid as there is no sockJS connect for provided ID", attribute_value);
      }

      id = atoi(attribute_value); 
      id_exists = 1;
    }
    // Extract Body
    else if ((strcasecmp(attribute_name, "BODY") == 0))
    {
      if(extract_body(flow_fp, script_line, &closing_quotes, 0, flow_file, 0, outfp) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
      if(set_body(url_idx, sess_idx, starting_quotes, closing_quotes, flow_file) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
      CLEAR_WHITE_SPACE(script_line);
      starting_quotes = script_line;

      body_exists = 1;
      post_idx = requests[url_idx].proto.http.post_idx;
      break;
    }
    //Extract Header
    else if (strcasecmp(attribute_name, "HEADER") == 0)
    {
      NSDL2_PARSING(NULL, NULL, "Header %s found at script_line %d\n", attribute_value, script_ln_no);
      if(set_headers(flow_fp, flow_file, attribute_value, header_buf, sess_idx, url_idx) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }

    ret = read_till_start_of_next_quotes(flow_fp, flow_file, closing_quotes, &starting_quotes, 0, outfp);
    if (ret == NS_PARSE_SCRIPT_ERROR)
    {
      NSDL2_PARSING(NULL, NULL, "Next attribute is not found");
      break;
    } 
  }

  if(!id_exists)
    SCRIPT_PARSE_ERROR(script_line, "ID doesn't exists");
  if(!body_exists)
    SCRIPT_PARSE_ERROR(script_line, "BODY doesn't exists");

  for(i = 0; i < total_sockjs_connect_entries; i++)
  {
    if(g_sockjs_connect[i].conn_id == id)
    {
     strncpy(url, g_sockjs_connect[i].url, MAX_URL_LENGTH);
    }
  }

  //xhr_send request
  if((ptr = strstr(url, "xhr_streaming")) != NULL)
  {
    snprintf(ptr, MAX_LINE_LENGTH, "xhr_send"); 
  }
  else
    SCRIPT_PARSE_ERROR(script_line, "Main URL Not Found.");
 
  NSDL2_PARSING(NULL, NULL, "URL = [%s]", url);

  if(set_url(url, flow_file, sess_idx, url_idx, 0, 0, "ns_sockjs_send") == NS_PARSE_SCRIPT_ERROR)
    return NS_PARSE_SCRIPT_ERROR; 

  requests[url_idx].proto.http.http_method = HTTP_METHOD_POST; // Set HTTP Method
  requests[url_idx].proto.http.http_method_idx = find_http_method_idx("POST");
  requests[url_idx].proto.http.post_idx = post_idx;
  requests[url_idx].proto.http.sockjs.conn_id = id;

  strcat(header_buf, "\r\n");
  NSDL2_PARSING(NULL, NULL, "Segmenting header_buf = [%s]", header_buf);
  segment_line(&(requests[url_idx].proto.http.hdrs), header_buf, 0, script_ln_no, sess_idx, flow_file);

  NSDL2_PARSING(NULL, NULL, "Exiting Method");
  return NS_PARSE_SCRIPT_SUCCESS;
}

//Parse ns_sockjs_close API
int parse_ns_sockjs_close(FILE *flow_fp, FILE *outfp, char *flow_filename, char *flowout_filename, int sess_idx)
{
  char attribute_name[128 + 1];
  char attribute_value[MAX_LINE_LENGTH + 1];
  char *close_quotes = NULL;
  char *start_quotes = NULL;
  int close_tb_idx = 0;
  char id_exists = 0;
  int ret;

  NSDL2_PARSING(NULL, NULL, "Method Called, sess_idx = %d", sess_idx);
  if(create_sockjs_close_table_entry(&close_tb_idx) != SUCCESS)
    SCRIPT_PARSE_ERROR(script_line, "Unable to create sockjs_close_table");
    
  start_quotes = strchr(script_line, '"'); //This will point to the starting quote '"'.

  if(start_quotes == NULL)
    SCRIPT_PARSE_ERROR(script_line, "Invalid Syntax. \n E.g., ns_sockjs_close(\"ID=1\")");

  //setting api ns_sockjs_close here 
  if((set_api("ns_sockjs_close", flow_fp, flow_filename, script_line, outfp, flowout_filename, close_tb_idx)) == NS_PARSE_SCRIPT_ERROR)
     NSDL2_PARSING(NULL, NULL, "Method Called starting_quotes = %p, starting_quotes = %s", start_quotes, start_quotes);

  while (1)
  {
    NSDL3_PARSING(NULL, NULL, "line = %s, close_tb_idx = %d, start_quotes = %s", script_line, close_tb_idx, start_quotes);
    ret = get_next_argument(flow_fp, start_quotes, attribute_name, attribute_value, &close_quotes, 0);
    if(ret == NS_PARSE_SCRIPT_ERROR) return NS_PARSE_SCRIPT_ERROR;

    if(!strcasecmp(attribute_name, "ID"))
    {
      NSDL2_PARSING(NULL, NULL, "ID [%s]", attribute_value);
      //validating id
      ret = is_sockjs_conid_exist(attribute_value);
      if(ret < 0)
      {
        SCRIPT_PARSE_ERROR(script_line, "ID [%s] is invalid as there is no sockJS connect for provided ID", attribute_value);
      }

      g_sockjs_close[close_tb_idx].conn_id = sockjs_idx_list[atoi(attribute_value)];
      id_exists = 1;
    }

    ret = read_till_start_of_next_quotes(flow_fp, flow_filename, close_quotes, &start_quotes, 0, outfp);
    if (ret == NS_PARSE_SCRIPT_ERROR)
    {
      NSDL2_PARSING(NULL, NULL, "Next attribute is not found");
      break;
    }
  }
  
  if(!id_exists)
    SCRIPT_PARSE_ERROR(script_line, "ID doesn't exists.")

  NSDL2_PARSING(NULL, NULL, "Exiting Method");
  return NS_PARSE_SCRIPT_SUCCESS;
}

void copy_sockjs_close_table_to_shr(void)
{
  int close_tb_idx;
  NSDL2_HTTP(NULL, NULL, "Method called. total_sockjs_close_entries = %d", total_sockjs_close_entries);
  if(!total_sockjs_close_entries)
  {
    NSDL2_HTTP(NULL, NULL, "No sockjs_close entries", total_sockjs_close_entries);
    return;
  }

  g_sockjs_close_shr = (sockjs_close_table_shr*) do_shmget(sizeof (sockjs_close_table_shr) * total_sockjs_close_entries, "sockjs_close_table_shr");

  for(close_tb_idx = 0; close_tb_idx < total_sockjs_close_entries ; close_tb_idx++)
  {
    NSDL2_HTTP(NULL, NULL, "Close Table data at sockjs_close_id = %d, conn_id = %d",  close_tb_idx, g_sockjs_close[close_tb_idx].conn_id);
    g_sockjs_close_shr[close_tb_idx].conn_id = g_sockjs_close[close_tb_idx].conn_id;
    
   NSDL2_HTTP(NULL, NULL, "Close Table Shar data at sockjs_close_id = %d", close_tb_idx, g_sockjs_close_shr[close_tb_idx].conn_id);
  }
}

void sockjs_close_connection(connection* cptr) 
{
  int url_ok = 0;
  int status;
  int redirect_flag = 0;
  int request_type;
  int done = 1;
  char taken_from_cache = 0; // No
  VUser *vptr = cptr->vptr;

  NSDL2_HTTP(vptr, cptr, "Method called. cptr = %p", cptr);

  u_ns_ts_t now = get_ms_stamp();
  //action_request_Shr *url_num = get_top_url_num(cptr);
  request_type = cptr->request_type;
  
  if(!cptr->request_complete_time)
    cptr->request_complete_time = now;

  status = cptr->req_ok = NS_REQUEST_OK;
  url_ok = !status;

  NSDL2_HTTP(vptr, cptr, "request_type %d, urls_awaited = %d", request_type, vptr->urls_awaited);

  cptr->num_retries = 0;
  if(vptr->urls_awaited)
    vptr->urls_awaited--;
  NS_DT3(vptr, cptr, DM_L1, MM_CONN, "Completed %s session with server %s",
       get_req_type_by_name(request_type),
       nslb_sock_ntop((struct sockaddr *)&cptr->cur_server));

  FREE_AND_MAKE_NULL(cptr->data, "Freeing cptr data", -1);

  if((cptr->conn_fd > 0))
    close_fd(cptr, done, now);

  if(vptr->sockjs_status == NS_REQUEST_OK )
  { 
    handle_url_complete(cptr, request_type, now, url_ok, redirect_flag, status, taken_from_cache); 
    handle_page_complete(cptr, vptr, done, now, request_type);
  }

  if (cptr->list_member & NS_ON_FREE_LIST) {
    NSTL1(vptr, cptr, "Connection slot is already in free connection list");
  } else {
    /* free_connection_slot remove connection from either or both reuse or inuse link list*/
    NSDL2_HTTP(vptr, cptr, "Connection state is free, need to call free_connection_slot");
    free_connection_slot(cptr, now);
  }
}

void sockjs_close_connection_ex(VUser *vptr) 
{
  NSDL2_HTTP(vptr, NULL, "Method Called, vptr = %p", vptr);
 
  connection *last_cptr = vptr->last_cptr; 
  int i; 

  /*Bug 54299: -ve Active user count was coming incase of ConFail*/
  if(vptr->vuser_state == NS_VUSER_THINKING)
    VUSER_THINKING_TO_ACTIVE(vptr);  //changing the state of vuser thinking to active

  if(last_cptr && (last_cptr->conn_state == CNST_REUSE_CON))
  {
    add_to_reuse_list(last_cptr);
  }
  
  for (i = 0; i < max_sockjs_conn; i++)
  {
    if(vptr->sockjs_cptr[i])
    {
      vptr->sockjs_status = NS_REQUEST_ERRMISC; 
      sockjs_close_connection(vptr->sockjs_cptr[i]);
      vptr->sockjs_cptr[i] = NULL;
    }
  }
}

int nsi_sockjs_close(VUser *vptr) 
{ 
  connection *cptr; 
  connection *last_cptr = vptr->last_cptr;
 
  int close_idx = vptr->sockjs_close_id; 
 
  NSDL2_HTTP(vptr, NULL, "Method Called, close_idx = %d, conn_id = %d", close_idx, g_sockjs_close_shr[close_idx].conn_id); 

  if (last_cptr && (last_cptr->conn_state == CNST_REUSE_CON))
  {   
    add_to_reuse_list(last_cptr);
  }       
 
  cptr = vptr->sockjs_cptr[g_sockjs_close_shr[close_idx].conn_id]; 
  vptr->sockjs_cptr[g_sockjs_close_shr[close_idx].conn_id] = NULL;
 
  if(cptr == NULL) 
  { 
    NSTL2(NULL, NULL, "Error: cptr is NULL");  
    NSDL2_HTTP(NULL, NULL, "Error: cptr is NULL"); 
    vptr->sockjs_status = NS_REQUEST_ERRMISC; 
    return -1; 
  } 
   
  if(cptr->conn_fd < 0) 
  { 
    NSDL2_HTTP(vptr, cptr, "SockJS: cptr->conn_fd is -1"); 
    NSTL2(vptr, cptr, "SockJS: cptr->conn_fd is -1"); 
    return -1;  
  } 
  else 
  { 
    vptr->sockjs_status = NS_REQUEST_OK;  
    sockjs_close_connection(cptr); 
    NSDL2_HTTP(vptr, cptr, "nsi_sockjs_close(): is successfully closed."); 
  } 
 
  return 0; 
}
