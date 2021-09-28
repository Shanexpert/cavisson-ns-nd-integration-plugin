/************************************************************************************
 * Name            : ns_dns_parse.c 
 * Purpose         : This file contains all the dns parsing related function of netstorm
 * Initial Version : Wednesday, January 27 2010 
 * Modification    : -
 ***********************************************************************************/


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

#include <arpa/nameser.h>
#include <arpa/nameser_compat.h>
#include "nslb_dns.h"
#include "ns_smtp_parse.h"
#include "ns_dns.h"
#include "ns_script_parse.h"
#include "ns_exit.h"

int kw_set_dns_timeout(char *buf, int *to_change, char *err_msg)
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


int parse_dns_query(FILE *cap_fp, int sess_idx, int *line_num, char *cap_fname) 
{
  int ii;
  int function_ends = 0;
  int dns_server_flag, type_flag = 0, name_flag, recursive_flag, proto_flag;
  int assert_rr_type_flag = 0, assert_rr_data_flag = 0; 
  char *line_ptr;
  char line[MAX_LINE_LENGTH];

  NSDL2_DNS(NULL, NULL, "Method Called. File: %s", cap_fname);

  dns_server_flag = name_flag = recursive_flag = proto_flag = 0;

  NSDL2_SCHEDULE(NULL, NULL, "file:%s", cap_fname);

  if (create_requests_table_entry(&ii) != SUCCESS) {   // Fill request type inside create table entry
      NS_EXIT(-1, "get_url_requets(): Could not create DNS request entry while parsing line %d in file %s", *line_num, cap_fname);
  }
  proto_based_init(ii, DNS_REQUEST);

  NSDL2_DNS(NULL, NULL, "ii = %d, total_request_entries = %d, total_dns_request_entries = %d",
                          ii, total_request_entries, total_dns_request_entries);

  while (nslb_fgets(line, MAX_LINE_LENGTH, cap_fp, 1)) {
    NSDL3_DNS(NULL, NULL, "line = %s", line);

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
      NS_EXIT(-1, "search_comma_as_last_char() failed.");

    if(function_ends && line_ptr[0] == ',')
      break;

    if (!strncmp(line_ptr, "DNS_SERVER_IP", strlen("DNS_SERVER_IP"))) {  // Parametrization = No
      if(dns_server_flag) {
        NS_EXIT(-1, "DNS_SERVER_IP can be given once");
      }
      dns_server_flag = 1;
        
      line_ptr += strlen("DNS_SERVER_IP");

      CLEAR_WHITE_SPACE(line_ptr);
      if (line_ptr[0] == '=') {
        line_ptr++;
      }
      else {
         NS_EXIT(-1, "= expected after DNS_SERVER_IP at line %d.", *line_num);
      }
      CLEAR_WHITE_SPACE(line_ptr);
 
      line_ptr[strlen(line_ptr) - 1 ] = '\0';
      requests[ii].index.svr_idx = get_server_idx(line_ptr, requests[ii].request_type, *line_num);
      /*Added for filling all server in gSessionTable*/
      CREATE_AND_FILL_SESS_HOST_TABLE_ENTRY(ii, "Method called from parse_dns_query");

    } else if (!strncmp(line_ptr, "PROTO", strlen("PROTO"))) {  // Parametrization = NO
      if(proto_flag) {
        NS_EXIT(-1, "PROTO can be given once for %s at line %d.", cap_fname, *line_num);
      }
      proto_flag = 1;
      line_ptr += strlen("PROTO");

      CLEAR_WHITE_SPACE(line_ptr);
      if (line_ptr[0] == '=') {
        line_ptr++;
      }
      else {
         NS_EXIT(-1, "= expected after PROTO for %s at line %d.", cap_fname, *line_num);
      }
      CLEAR_WHITE_SPACE(line_ptr);
 
      line_ptr[strlen(line_ptr) - 1 ] = '\0';
      if(strcmp(line_ptr, "UDP") == 0) {
        requests[ii].proto.dns.proto = USE_DNS_ON_UDP;
      } else if (strcmp(line_ptr, "TCP") == 0) {
        requests[ii].proto.dns.proto = USE_DNS_ON_TCP; 
      } else {
         NS_EXIT(-1, "PROTO can have only TCP/UDP for  %s at line %d.", cap_fname, *line_num);
      }
    } else if (!strncmp(line_ptr, "NAME", strlen("NAME"))) {  // Parametrization = Yes
      if(name_flag) {
        NS_EXIT(-1, "NAME can be given once.");
      }
      name_flag = 1;
      line_ptr += strlen("NAME");

      CLEAR_WHITE_SPACE(line_ptr);
      if (line_ptr[0] == '=') {
        line_ptr++;
      }
      else {
         NS_EXIT(-1, "= expected after NAME at line %d.", *line_num);
      }
      CLEAR_WHITE_SPACE(line_ptr);
 
      line_ptr[strlen(line_ptr) - 1 ] = '\0';
      segment_line(&(requests[ii].proto.dns.name), line_ptr, 0, *line_num, sess_idx, cap_fname);

    } else if (!strncmp(line_ptr, "RECURSIVE", strlen("RECURSIVE"))) {  // Parametrization = NO
      if(recursive_flag) {
        NS_EXIT(-1, "RECURSIVE can be given once.");
      }
      recursive_flag = 1;
      line_ptr += strlen("RECURSIVE");

      CLEAR_WHITE_SPACE(line_ptr);
      if (line_ptr[0] == '=') {
        line_ptr++;
      }
      else {
         NS_EXIT(-1, "= expected after RECURSIVE at line %d.", *line_num);
      }
      CLEAR_WHITE_SPACE(line_ptr);
 
      line_ptr[strlen(line_ptr) - 1 ] = '\0';
      requests[ii].proto.dns.recursive = atoi(line_ptr);

    } else if (!strncmp(line_ptr, "TYPE", strlen("TYPE"))) {  // Parametrization = NO
      if(type_flag) {
        NS_EXIT(-1, "TYPE can be given once.");
      }
      type_flag = 1;
      line_ptr += strlen("TYPE");

      CLEAR_WHITE_SPACE(line_ptr);
      if (line_ptr[0] == '=') {
        line_ptr++;
      }
      else {
         NS_EXIT(-1, "= expected after TYPE at line %d.", *line_num);
      }
      CLEAR_WHITE_SPACE(line_ptr);
 
      line_ptr[strlen(line_ptr) - 1 ] = '\0';
      int qtype = dns_qtype_to_int(line_ptr); 
      if(qtype != -1)
        requests[ii].proto.dns.qtype = qtype;
      else {
        NS_EXIT(-1, "Invalid query type '%s' given at line %d.", line_ptr, *line_num); 
      }
    } else if (!strncmp(line_ptr, "ASSERT_RR_TYPE", strlen("ASSERT_RR_TYPE"))) {  // Parametrization = Yes
      if(assert_rr_type_flag) {
        NS_EXIT(-1, "ASSERT_RR_TYPE can be given once.");
      }
      assert_rr_type_flag = 1;
      line_ptr += strlen("ASSERT_RR_TYPE");

      CLEAR_WHITE_SPACE(line_ptr);
      if (line_ptr[0] == '=') {
        line_ptr++;
      }
      else {
         NS_EXIT(-1, "= expected after ASSERT_RR_TYPE at line %d.", *line_num);
      }
      CLEAR_WHITE_SPACE(line_ptr);
 
      line_ptr[strlen(line_ptr) - 1 ] = '\0';
      segment_line(&(requests[ii].proto.dns.assert_rr_type), line_ptr, 0, *line_num, sess_idx, cap_fname);

    } else if (!strncmp(line_ptr, "ASSERT_RR_DATA", strlen("ASSERT_RR_DATA"))) {  // Parametrization = Yes
      if(assert_rr_data_flag) {
        NS_EXIT(-1, "ASSERT_RR_DATA can be given once.");
      }
      assert_rr_data_flag = 1;
      line_ptr += strlen("ASSERT_RR_DATA");

      CLEAR_WHITE_SPACE(line_ptr);
      if (line_ptr[0] == '=') {
        line_ptr++;
      }
      else {
         NS_EXIT(-1, "= expected after ASSERT_RR_DATA at line %d.", *line_num);
      }
      CLEAR_WHITE_SPACE(line_ptr);
 
      line_ptr[strlen(line_ptr) - 1 ] = '\0';
      segment_line(&(requests[ii].proto.dns.assert_rr_data), line_ptr, 0, *line_num, sess_idx, cap_fname);
    } else {
      fprintf(stderr, "Line %d not expected\n", *line_num);
      return -1;
    }

    if(function_ends)
     break;
  }

  if(!dns_server_flag) {
     NS_EXIT(-1, "DNS_SERVER_IP must be given for DNS_SERVER_IP SESSION for page %s!", RETRIEVE_BUFFER_DATA(gPageTable[g_cur_page].page_name));
  }

  if (!name_flag) {
     NS_EXIT(-1, "USER_ID must be given for DNS for page %s!", RETRIEVE_BUFFER_DATA(gPageTable[g_cur_page].page_name));
  }
 
  if(!function_ends) {
    fprintf(stderr, "End of function dns not found\n");
    return -1;
  }

  
  gPageTable[g_cur_page].first_eurl = ii;

  return 0;
}


// script_ln_no is a global varable
int parse_ns_dns_query(FILE *flow_fp, FILE *outfp, char *flow_filename, char *flowout_filename, int sess_idx)
{
  int index;
  char *page_end_ptr;
  char *start_quotes;
  char *close_quotes;
  char pagename[MAX_LINE_LENGTH];
  char attribute_name[128 + 1];
  char attribute_value[MAX_LINE_LENGTH];
  int dns_server_flag=0, type_flag = 0, name_flag=0, recursive_flag=0, proto_flag=0;
  int assert_rr_type_flag = 0, assert_rr_data_flag = 0;

  create_requests_table_entry(&index);  // Fill request type inside create table entry
   
  proto_based_init(index, DNS_REQUEST);
  NSDL2_DNS(NULL, NULL, "index = %d, total_request_entries = %d, total_dns_request_entries = %d",
                                                     index, total_request_entries, total_dns_request_entries);

  if(extract_pagename(flow_fp, flow_filename, script_line, pagename, &page_end_ptr))
    return NS_PARSE_SCRIPT_ERROR;

  // For DNS, we are internally using ns_web_url API
  if((parse_and_set_pagename("ns_dns_query", "ns_web_url", flow_fp, flow_filename, script_line, outfp, flowout_filename, sess_idx, &page_end_ptr, pagename)) == NS_PARSE_SCRIPT_ERROR)
    return NS_PARSE_SCRIPT_ERROR;

  //We will be checking for (" & white spaces in-between in extract_pagename
  gPageTable[g_cur_page].first_eurl = index;
  gPageTable[g_cur_page].num_eurls++; // Increment urls

  close_quotes = page_end_ptr;
  start_quotes = NULL;
  // Point to next argument
  //This will return if start quotes of next argument is not found or some other printable
  //is found including );
                
  if(read_till_start_of_next_quotes(flow_fp, flow_filename, close_quotes, &start_quotes, 0, outfp)) {
    SCRIPT_PARSE_ERROR(script_line, "Syntax Error");
    return NS_PARSE_SCRIPT_ERROR;
  }

  while(1)
  {
    NSDL2_DNS(NULL, NULL, "line = %s", script_line);

    //searching for attribute name and attribute value
    if(get_next_argument(flow_fp, start_quotes, attribute_name, attribute_value, &close_quotes, 0)){
      NSDL2_DNS(NULL, NULL, " error in get_next_argument %d line = %s",script_ln_no, script_line);
      return NS_PARSE_SCRIPT_ERROR;
    }
    if (!strcmp(attribute_name,"DNS_SERVER_IP")) 
    {  // Parametrization is not allowed for this argument
      if(dns_server_flag) 
      {
        NSDL2_DNS(NULL, NULL, "error in dns_parse_flag line = %s", script_line);
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012218_ID, CAV_ERR_1012218_MSG, "DNS_SERVER");
      }
      NSDL2_DNS(NULL, NULL, " dns_server_flag set line = %s", script_line);
      dns_server_flag = 1;
      requests[index].index.svr_idx = get_server_idx(attribute_value, requests[index].request_type,script_ln_no);
      /*Added for filling all server in gSessionTable*/
      CREATE_AND_FILL_SESS_HOST_TABLE_ENTRY(index, "Method called from parse_ns_dns_query");
    } 
    else if (!strcmp(attribute_name,"PROTO")) 
    {  // Parametrization is not allowed for this argument
      if(proto_flag)
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012218_ID, CAV_ERR_1012218_MSG, "PROTO"); 

      proto_flag = 1;
      NSDL2_DNS(NULL, NULL, " proto_flag set line = %s", script_line);

      if(strcmp(attribute_value, "UDP") == 0) 
        requests[index].proto.dns.proto = USE_DNS_ON_UDP;
      else if (strcmp(attribute_value, "TCP") == 0) 
        requests[index].proto.dns.proto = USE_DNS_ON_TCP; 
      else 
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012220_ID, CAV_ERR_1012220_MSG, attribute_value, "PROTO", "TCP", "UDP");
    }
    else if (!strcmp(attribute_name, "NAME")) 
    {  // Parametrization is allowed for this argument
      if(name_flag) 
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012218_ID, CAV_ERR_1012218_MSG, "NAME");

      name_flag = 1;
      NSDL2_DNS(NULL, NULL, " name_flag set line = %s", script_line);

      segment_line(&(requests[index].proto.dns.name), attribute_value, 0, script_ln_no, sess_idx,flow_filename);
    }
    else if (!strcmp(attribute_name,"RECURSIVE")) 
    {  // Parametrization is not allowed for this argument
      if(recursive_flag) 
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012218_ID, CAV_ERR_1012218_MSG, "RECURSIVE");

      recursive_flag = 1;
      NSDL2_DNS(NULL, NULL, " recursive_flag set line = %s", script_line);
      requests[index].proto.dns.recursive = atoi(attribute_value);
    }
    else if (!strcmp(attribute_name, "TYPE")) 
    {  // Parametrization is not allowed for this argument
      if(type_flag) 
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012218_ID, CAV_ERR_1012218_MSG, "TYPE");

      type_flag = 1;
      NSDL2_DNS(NULL, NULL, " type_flag set line = %s", script_line);

      int qtype = dns_qtype_to_int(attribute_value); 
      if(qtype != -1)
        requests[index].proto.dns.qtype = qtype;
      else
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012215_ID, CAV_ERR_1012215_MSG, attribute_value, "TYPE"); 
    }
    else if (!strcmp(attribute_name, "ASSERT_RR_TYPE")) 
    {  // Parametrization is allowed for this argument
      if(assert_rr_type_flag)
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012218_ID, CAV_ERR_1012218_MSG, "ASSERT_RR_TYPE"); 

      assert_rr_type_flag = 1;
      NSDL2_DNS(NULL, NULL, " assert_rr_type_flag set line = %s", script_line);

      segment_line(&(requests[index].proto.dns.assert_rr_type), attribute_value, 0, script_ln_no, sess_idx, flow_filename);

    }
    else if (!strcmp(attribute_name, "ASSERT_RR_DATA")) 
    {  // Parametrization is allowed for this argument
      if(assert_rr_data_flag) 
        SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012218_ID, CAV_ERR_1012218_MSG, "ASSERT_RR_DATA");

      assert_rr_data_flag = 1;
      NSDL2_DNS(NULL, NULL, " assert_rr_data_flag set line = %s", script_line);

      segment_line(&(requests[index].proto.dns.assert_rr_data),attribute_value, 0, script_ln_no, sess_idx, flow_filename);
    }
    else  
    {
      SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012219_ID, CAV_ERR_1012219_MSG, attribute_name);
    }

    if(read_till_start_of_next_quotes(flow_fp, flow_filename, close_quotes, &start_quotes, 0, outfp))
    {
      NSDL2_DNS(NULL, NULL, " assert_rr_data_flag set line = %s", script_line);
      break;
    }
  }

  if(start_quotes == NULL)
  {
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012209_ID, CAV_ERR_1012209_MSG, "DNS");
  }
  else
  {
    if(!strncmp(start_quotes, ");", 2))
    {
      gPageTable[g_cur_page].first_eurl = index;
      NSDL2_DNS(NULL, NULL, " ); quotes found ");
    }
    else{ 
      NSDL2_DNS(NULL, NULL, " ); not found "); //for debug logs
      SCRIPT_PARSE_ERROR(script_line, " \n  ); is missing in API");
      return NS_PARSE_SCRIPT_ERROR;
    }
  }

      
  NSDL2_DNS(NULL, NULL, "check for flag setting = %s", script_line);

// Validate all mandatory arguments are given

  if(!dns_server_flag)
  { 
    NSDL2_DNS(NULL, NULL, "Mandatory field DNS server not found");
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012211_ID, CAV_ERR_1012211_MSG, "DNS server");
  }
  if (!name_flag)
  {
    NSDL2_DNS(NULL, NULL, "Mandatory field domain name not found");
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012211_ID, CAV_ERR_1012211_MSG, "Domain name");
  }
  if (!type_flag)
  {
    NSDL2_DNS(NULL, NULL, "Mandatory field Type not found");
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012211_ID, CAV_ERR_1012211_MSG, "TYPE");
  }
  NSDL2_DNS(NULL, NULL, "All mandatory argument flag found parsing success");
  
  return NS_PARSE_SCRIPT_SUCCESS;
}



