/************************************************************************************
 * Name            : ns_pop3_parse.c 
 * Purpose         : This file contains all the pop3 parsing related function of netstorm
 * Initial Version : Wednesday, January 27 2010 
 * Modification    : -
 ***********************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
 // #include <netinet/in.h>  //not needed on IRIX
#include <netdb.h>
#include <sys/times.h>
#include <sys/types.h>
#include <unistd.h>
 // #include <sys/select.h>
 // #include <netdb.h>
#include <ctype.h>
 // #include <dlfcn.h>
 // #include <sys/ipc.h>
 // #include <sys/shm.h>
 // #include <sys/wait.h>
 // #include <assert.h>
#include <errno.h>
 



#include <regex.h>
#include <libgen.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "nslb_util.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"

#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "user_tables.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "url.h"
#include "util.h"
#include "timing.h"
#include "tmr.h"
#include "amf.h"
#include "ns_trans_parse.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "runlogic.h"
#include "ns_gdf.h"
#include "ns_vars.h"
#include "ns_log.h"
#include "ns_cookie.h"
#include "ns_user_monitor.h"
#include "ns_alloc.h"
#include "ns_percentile.h"
#include "ns_parse_scen_conf.h"
#include "ns_server_admin_utils.h"
#include "ns_error_codes.h"
#include "ns_page.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "ns_smtp_parse.h"
#include "ns_smtp.h"
#include "ns_pop3.h"
#include "ns_smtp_parse.h"
#include "ns_script_parse.h" 
#include "ns_exit.h"

static int parse_pop3(FILE *cap_fp, int sess_idx, int *line_num, char *cap_fname, 
               int pop3_action_type) 
{
  int ii;
  int function_ends = 0;
  int pop3_server_flag, user_flag, passwd_flag;
  char *line_ptr;
  char line[MAX_LINE_LENGTH];

  NSDL2_POP3(NULL, NULL, "Method Called. File: %s", cap_fname);

  pop3_server_flag = user_flag = passwd_flag = 0;

  NSDL2_SCHEDULE(NULL, NULL, "file:%s", cap_fname);

  if (create_requests_table_entry(&ii) != SUCCESS) {   // Fill request type inside create table entry
      NS_EXIT(-1, "get_url_requets(): Could not create pop3 request entry while parsing line %d in file %s", *line_num, cap_fname);
  }
  proto_based_init(ii, POP3_REQUEST);

  NSDL2_POP3(NULL, NULL, "ii = %d, total_request_entries = %d, total_pop3_request_entries = %d",
                          ii, total_request_entries, total_pop3_request_entries);

  while (nslb_fgets(line, MAX_LINE_LENGTH, cap_fp, 1)) {
    NSDL3_POP3(NULL, NULL, "line = %s", line);

    (*line_num)++;
    line_ptr = line;

    CLEAR_WHITE_SPACE(line_ptr);
    IGNORE_COMMENTS(line_ptr);
   
    if (*line_ptr == '\n')
     continue;

    /* remove the newline character from end of line. */
    if (strchr(line_ptr, '\n')) {
      if (strlen(line_ptr) > 0)
        line_ptr[strlen(line_ptr) - 1] = '\0';
    }

    if(search_comma_as_last_char(line_ptr, &function_ends))
      NS_EXIT(-1, "search_comma_as_last_char() failed");

    if(function_ends && line_ptr[0] == ',')
      break;

    if (!strncmp(line_ptr, "POP_SERVER", strlen("POP_SERVER"))) {  // Parametrization = No
      if(pop3_server_flag) {
        NS_EXIT(-1, "POP_SERVER can be given once.");
      }
      pop3_server_flag = 1;
        
      line_ptr += strlen("POP_SERVER");

      CLEAR_WHITE_SPACE(line_ptr);
      if (line_ptr[0] == '=') {
        line_ptr++;
      }
      else {
         NS_EXIT(-1,  "= expected after POP3_SERVER at line %d.");
      }
      CLEAR_WHITE_SPACE(line_ptr);
 
      line_ptr[strlen(line_ptr) - 1 ] = '\0';
      requests[ii].index.svr_idx = get_server_idx(line_ptr, requests[ii].request_type, *line_num);

      /*Added for filling all server in gSessionTable*/
      CREATE_AND_FILL_SESS_HOST_TABLE_ENTRY(ii, "Method called from parse_pop3");

    } else if (!strncmp(line_ptr, "USER_ID", strlen("USER_ID"))) {  // Parametrization = Yes
      if(user_flag) {
        NS_EXIT(-1, "USER_ID can be given once.");
      }
      user_flag = 1;
      line_ptr += strlen("USER_ID");

      CLEAR_WHITE_SPACE(line_ptr);
      if (line_ptr[0] == '=') {
        line_ptr++;
      }
      else {
         NS_EXIT(-1, "= expected after USER_ID at line %d.");
      }
      CLEAR_WHITE_SPACE(line_ptr);
 
      line_ptr[strlen(line_ptr) - 1 ] = '\0';
      segment_line(&(requests[ii].proto.pop3.user_id), line_ptr, 0, *line_num, sess_idx, cap_fname);

    } else if (!strncmp(line_ptr, "PASSWORD", strlen("PASSWORD"))) {  // Parametrization = Yes
      if(passwd_flag) {
        NS_EXIT(-1, "PASSWORD can be given once.");
      }
      passwd_flag = 1;
      line_ptr += strlen("PASSWORD");

      CLEAR_WHITE_SPACE(line_ptr);
      if (line_ptr[0] == '=') {
        line_ptr++;
      }
      else {
         NS_EXIT(-1, "= expected after PASSWORD at line %d.");
      }
      CLEAR_WHITE_SPACE(line_ptr);
 
      line_ptr[strlen(line_ptr) - 1 ] = '\0';
      segment_line(&(requests[ii].proto.pop3.passwd), line_ptr, 0, *line_num, sess_idx, cap_fname);

    } else {
      fprintf(stderr, "Line %d not expected\n", *line_num);
      return -1;
    }

    if(function_ends)
     break;
  }

  requests[ii].proto.pop3.pop3_action_type = pop3_action_type;
  if(!pop3_server_flag) {
     NS_EXIT(-1, "POP3_SERVER must be given for POP3 SESSION for page %s!", RETRIEVE_BUFFER_DATA(gPageTable[g_cur_page].page_name));
  }

  if (!user_flag) {
     NS_EXIT(-1, "USER_ID must be given for POP3 for page %s!", RETRIEVE_BUFFER_DATA(gPageTable[g_cur_page].page_name));
  }
 
  if (!passwd_flag) {
     NS_EXIT(-1, "PASSWORD must be given for POP3 for page %s!", RETRIEVE_BUFFER_DATA(gPageTable[g_cur_page].page_name));
  }

  if(!function_ends) {
    fprintf(stderr, "End of function pop3 not found\n");
    return -1;
  }

  
  gPageTable[g_cur_page].first_eurl = ii;

  return 0;
}

static int parse_ns_pop3(char *api_name, char *api_to_run, FILE *flow_fp, FILE *outfp, char *flow_filename, char *flowout_filename, int sess_idx, 
               int pop3_action_type) 
{
  int url_idx;
  int pop3_server_flag, user_flag, passwd_flag, starttls_flag;
  char *start_quotes;
  char *close_quotes;
  char pagename[MAX_LINE_LENGTH + 1];
  char attribute_name[128 + 1];
  char attribute_value[MAX_LINE_LENGTH + 1];
  int ret;
  char *page_end_ptr;

  NSDL2_POP3(NULL, NULL, "Method Called. File: %s", flow_filename);

  pop3_server_flag = user_flag = passwd_flag = starttls_flag = 0;

  NSDL2_SCHEDULE(NULL, NULL, "file:%s", flow_filename);

  create_requests_table_entry(&url_idx); // Fill request type inside create table entry

  NSDL2_POP3(NULL, NULL, "url_idx = %d, total_request_entries = %d, total_pop3_request_entries = %d",
                          url_idx, total_request_entries, total_pop3_request_entries);

  //We will be checking for (" & white spaces in-between in extract_pagename 
  ret = extract_pagename(flow_fp, flow_filename, script_line, pagename, &page_end_ptr);
  if(ret == NS_PARSE_SCRIPT_ERROR) return NS_PARSE_SCRIPT_ERROR;

  // For POP3, we are internally using ns_web_url API
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
    NSDL3_POP3(NULL, NULL, "line = %s", script_line);

    ret = get_next_argument(flow_fp, start_quotes, attribute_name, attribute_value, &close_quotes, 0);

    if(ret == NS_PARSE_SCRIPT_ERROR) return NS_PARSE_SCRIPT_ERROR;

    if (!strcmp(attribute_name, "POP_SERVER") || !strcmp(attribute_name, "SPOP_SERVER")) // Parametrization is not allowed for this argument
    {
      if (!strcmp(attribute_name, "POP_SERVER")){
        proto_based_init(url_idx, POP3_REQUEST);
      }else{
        proto_based_init(url_idx, SPOP3_REQUEST);
      }
      if(pop3_server_flag)
      {
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012218_ID, CAV_ERR_1012218_MSG, "(S)POP_SERVER");
      }
      requests[url_idx].index.svr_idx = get_server_idx(attribute_value, requests[url_idx].request_type, script_ln_no);
      pop3_server_flag = 1;
      NSDL2_POP3(NULL, NULL,"Value of  %s = %s, svr_idx = %d", attribute_value, attribute_name, requests[url_idx].index.svr_idx);

      /*Added for filling all server in gSessionTable*/
      CREATE_AND_FILL_SESS_HOST_TABLE_ENTRY(url_idx, "Method called from parse_ns_pop3");
    }
    else if (!strcmp(attribute_name, "USER_ID")) // Parametrization is allowed for this argument
    { 
      if(user_flag)
      {
        SCRIPT_PARSE_ERROR(NULL, "USER_ID can be given once.");
      }
      segment_line(&(requests[url_idx].proto.pop3.user_id), attribute_value, 0, script_ln_no, sess_idx, flow_filename);
      user_flag = 1;
      NSDL2_POP3(NULL, NULL,"Value of  %s = %s, user_idx = %d", attribute_value, attribute_name, requests[url_idx].proto.pop3.user_id);
     } 
    else if (!strcmp(attribute_name, "PASSWORD")) // Parametrization is allowed for this argument
    { 
      if(passwd_flag)
      {
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012218_ID, CAV_ERR_1012218_MSG, "PASSWORD");
      }
      segment_line(&(requests[url_idx].proto.pop3.passwd), attribute_value, 0, script_ln_no, sess_idx, flow_filename);
      passwd_flag = 1;
      NSDL2_POP3(NULL, NULL,"Value of  %s = %s, password_idx = %d", attribute_value, attribute_name, requests[url_idx].proto.pop3.passwd);
    } else if (!strcmp(attribute_name, "STARTTLS")){ // Parametrization is  not allowed for this argument
      if(starttls_flag){
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012218_ID, CAV_ERR_1012218_MSG, "STARTTLS");
      }
      if(!strcasecmp(attribute_value, "YES")){
        requests[url_idx].proto.pop3.authentication_type = 1;
      }else if(!strcasecmp(attribute_value, "NO")){
        requests[url_idx].proto.pop3.authentication_type = 0;
      }else{
       SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012220_ID, CAV_ERR_1012220_MSG, attribute_value, "STARTTLS", "YES", "NO");
      }
      starttls_flag = 1;
      NSDL2_POP3(NULL, NULL,"Value of  %s = %s", attribute_value, attribute_name);
    }else 
    {
      SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012219_ID, CAV_ERR_1012219_MSG, attribute_name);
    }
    ret = read_till_start_of_next_quotes(flow_fp, flow_filename, close_quotes, &start_quotes, 0, outfp);
    //In case next comma not found between quotes or end_of_file found
    if(ret == NS_PARSE_SCRIPT_ERROR)
    {
      NSDL2_POP3(NULL, NULL,"Next attribute is not found");
      break;
    }
  }
  if(start_quotes == NULL)
  {
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012209_ID, CAV_ERR_1012209_MSG, "POP3");
  }  
  else
 {
    if(!strncmp(start_quotes, ");", 2))
    {
      NSDL2_POP3(NULL, NULL,"End of function ns_pop_get found %s", start_quotes);
    }
    else 
    {
      SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012210_ID, CAV_ERR_1012210_MSG, start_quotes);
    } 
  }

  requests[url_idx].proto.pop3.pop3_action_type = pop3_action_type;
   
  // Validate all mandatory arguments are given
  if(!pop3_server_flag) 
  {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012217_ID, CAV_ERR_1012217_MSG, "POP3_SERVER", RETRIEVE_BUFFER_DATA(gPageTable[g_cur_page].page_name));
  }

  if (!user_flag) 
  {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012217_ID, CAV_ERR_1012217_MSG, "USER_ID", RETRIEVE_BUFFER_DATA(gPageTable[g_cur_page].page_name));
  }
 
  if (!passwd_flag) 
  {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012217_ID, CAV_ERR_1012217_MSG, "PASSWORD", RETRIEVE_BUFFER_DATA(gPageTable[g_cur_page].page_name));
  }
  return NS_PARSE_SCRIPT_SUCCESS;
}

int parse_pop3_stat(FILE *cap_fp, int sess_idx, int *line_num, char *cap_fname)
{
  return parse_pop3(cap_fp, sess_idx, line_num, cap_fname, POP3_ACTION_STAT);
}

int parse_pop3_list(FILE *cap_fp, int sess_idx, int *line_num, char *cap_fname)
{
  return parse_pop3(cap_fp, sess_idx, line_num, cap_fname, POP3_ACTION_LIST);
}

int parse_pop3_get (FILE *cap_fp, int sess_idx, int *line_num, char *cap_fname)
{
  return parse_pop3(cap_fp, sess_idx, line_num, cap_fname, POP3_ACTION_GET);
}


int parse_ns_pop3_stat(FILE *flow_fp, FILE *outfp, char *flow_filename, char *flowout_filename, int sess_idx)
{
  return parse_ns_pop3("ns_pop_stat", "ns_web_url", flow_fp, outfp, flow_filename, flowout_filename, sess_idx, POP3_ACTION_STAT);
}

int parse_ns_pop3_list(FILE *flow_fp, FILE *outfp, char *flow_filename, char *flowout_filename, int sess_idx)
{
  return parse_ns_pop3("ns_pop_list", "ns_web_url", flow_fp, outfp, flow_filename, flowout_filename, sess_idx, POP3_ACTION_LIST);
}

int parse_ns_pop3_get(FILE *flow_fp, FILE *outfp, char *flow_filename, char *flowout_filename, int sess_idx)
{
  return parse_ns_pop3("ns_pop_get", "ns_web_url", flow_fp, outfp, flow_filename, flowout_filename, sess_idx, POP3_ACTION_GET);
}


// KEYWORD GROUP VALUE
int kw_set_pop3_timeout(char *buf, int *to_change, char *err_msg)
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
