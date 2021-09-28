#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "url.h"
#include "ns_common.h"
#include "ns_global_settings.h"
#include "ns_log.h"
#include "ns_script_parse.h"
#include "ns_http_process_resp.h"
//#include "ns_page_dump.h"
#include "ns_ldap.h"
#include "ns_log_req_rep.h"
#include "nslb_util.h" 
#include "ns_group_data.h"
#include "ns_alloc.h"

#define LDAP_RESP_BUF (100 * 1024)

#define CHECK_MAND_ARGS\
          if(operation == LDAP_SEARCH && (!b_flag || !f_flag)){\
             SCRIPT_PARSE_ERROR(script_line, "Mandatory field BASE/FILTER is missing in LDAP SEARCH operation.");}\
          else if(operation == LDAP_RENAME && !b_flag){\
             SCRIPT_PARSE_ERROR(script_line, "Mandatory field NEW_RDN is missing in LDAP RENAME operation.");}\
          else if((operation != LDAP_SEARCH && operation != LDAP_LOGIN && operation != LDAP_LOGOUT) && !d_flag){\
             SCRIPT_PARSE_ERROR(script_line, "Mandatory field DN is missing in LDAP operation.");}
              
#define IS_HEADER_PARSED(cptr) (cptr->req_code_filled & 0x80000000)

#define SET_HEADER_PARSED(cptr) cptr->req_code_filled = (cptr->content_length | 0x80000000);

#define UNSET_HEADER_PARSED(cptr) cptr->req_code_filled = (cptr->req_code_filled & 0x7FFFFFFF);

#define LAST_FRAME_OFFSET(cptr) (cptr->req_code_filled & 0x7FFFFFFF)

/*
#define LOG_LDAP_REQ(my_cptr, bytes_to_log)\
  VUser *vptr; \
  vptr = my_cptr->vptr; \
  if(NS_IF_PAGE_DUMP_ENABLE && (runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_ALL_SESS \
                                || runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_PAGE_ON_HEADER_BASED) \
                            && (runprof_table_shr_mem[vptr->group_num].gset.trace_level > TRACE_URL_DETAIL)) \
  { \
    log_ldap_req(my_cptr, vptr, bytes_to_log); \
  }   

#define LOG_LDAP_RES(my_cptr, local_resp_buf)\
  VUser *vptr; \
  vptr = my_cptr->vptr; \
  if(NS_IF_PAGE_DUMP_ENABLE && (runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_ALL_SESS \
                                || runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_PAGE_ON_HEADER_BASED) \
                            && (runprof_table_shr_mem[vptr->group_num].gset.trace_level > TRACE_URL_DETAIL)) \
  { \
    log_ldap_res(my_cptr, vptr, local_resp_buf); \
  }   
*/

extern int msg_len;

int total_ldap_request_entries = 0;
int ldap_operation;
LDAPCAvgTime *ldap_cavgtime;
int g_ldap_cavgtime_idx = -1;

#ifndef CAV_MAIN
action_request* requests;
int g_ldap_avgtime_idx = -1;
LDAPAvgTime *ldap_avgtime = NULL;
#else
extern __thread action_request* requests;
__thread int g_ldap_avgtime_idx = -1;
__thread LDAPAvgTime *ldap_avgtime = NULL;
#endif

int kw_set_ldap_timeout(char *buf, int *to_change, char *err_msg)
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

int ldap_proto_init(int *row_num, int proto, int operation){

  NSDL2_HTTP(NULL, NULL, "Method called, proto = [%d], operation = [%d]", proto, operation);

  requests[*row_num].request_type = proto;

  if (proto == LDAP_REQUEST || proto == LDAPS_REQUEST) { // LDAP
    if(!(global_settings->protocol_enabled & LDAP_PROTOCOL_ENABLED)) {
        global_settings->protocol_enabled |= LDAP_PROTOCOL_ENABLED;
    }
    total_ldap_request_entries++;

    requests[*row_num].proto.ldap.username.seg_start = -1;
    requests[*row_num].proto.ldap.username.num_entries = 0;
  
    requests[*row_num].proto.ldap.passwd.seg_start = -1;
    requests[*row_num].proto.ldap.passwd.num_entries = 0;

    requests[*row_num].proto.ldap.operation = operation;
   }

  NSDL2_HTTP(NULL, NULL, "Exitting method");
  return (SUCCESS);
}

int ldap_set_url(char *url, char *flow_file, int sess_idx, int *url_idx, char embedded_url, int operation)
{
  int  request_type;
  //int localhost_flag = 0;
  //int ret;

  NSDL2_PARSING(NULL, NULL, "Method Called Url=%s", url);

  //parsing of url
  if (url[0] == '\0')
  {
    NSEL_MAJ(NULL, NULL, ERROR_ID, ERROR_ATTR, "Error: Url is empty. url = %s", url);
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012330_ID, CAV_ERR_1012330_MSG, "URL");
  }
  if(!strncasecmp(url, "ldap://", 7)){
    request_type = LDAP_REQUEST;
    url += 7;
    NSDL2_HTTP(NULL, NULL, "request_type = %d", request_type);
  }else if(!strncasecmp(url, "ldap:///", 8)){
    //localhost_flag = 1;
    request_type = LDAP_REQUEST;
    url += 8;
    NSDL2_HTTP(NULL, NULL, "request_type = %d", request_type); 
  }else if(!strncasecmp(url, "ldaps://", 8)){
    request_type = LDAPS_REQUEST;
    url += 8;
    NSDL2_HTTP(NULL, NULL, "request_type = %d", request_type);
  }else{
     SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012223_ID, CAV_ERR_1012223_MSG, url);
  }

  ldap_proto_init(url_idx, request_type, operation);
  NSDL2_HTTP(NULL, NULL, "url=[%s] url_idx=[%d]", url, *url_idx);
  //extract username, password, hostname, port
  char *ptr = strchr(url, '@');
  char *tmp = NULL;
  
  if(ptr != NULL){
    *ptr = '\0';
    ptr++;
   
    NSDL2_HTTP(NULL, NULL, "url=[%s] url_idx=[%d]", url, *url_idx);
    if((tmp = strchr(url, ':')) != NULL){
       strncpy(requests[*url_idx].proto.ldap.user, url, tmp-url);
       requests[*url_idx].proto.ldap.user[tmp-url] = '\0';
       strcpy(requests[*url_idx].proto.ldap.password, ++tmp);
    }else{
       strcpy(requests[*url_idx].proto.ldap.user, url);
    }
 
    url = ptr;

    NSDL2_HTTP(NULL, NULL, "url=[%s] ptr=[%s] user=[%s] pass=[%s]", url, ptr, requests[*url_idx].proto.ldap.user, requests[*url_idx].proto.ldap.password); 

    segment_line(&(requests[*url_idx].proto.ldap.username), requests[*url_idx].proto.ldap.user, 0, script_ln_no, sess_idx, flow_file);
    segment_line(&(requests[*url_idx].proto.ldap.passwd), requests[*url_idx].proto.ldap.password, 0, script_ln_no, sess_idx, flow_file);
  }else{  
  //what to do in case we dont have username and password : TODO
  }

  //Setting url type to Main/Embedded
  if(!embedded_url)
    requests[*url_idx].proto.ldap.type = MAIN_URL;
  else
    requests[*url_idx].proto.ldap.type = EMBEDDED_URL;
  
  gPageTable[g_cur_page].num_eurls++; // Increment urls

  // check if the hostname exists in the server table, if not add it
  requests[*url_idx].index.svr_idx = get_server_idx(url, requests[*url_idx].request_type, script_ln_no);

  if(requests[*url_idx].index.svr_idx != -1)
  {
    if(gServerTable[requests[*url_idx].index.svr_idx].main_url_host == -1)
    {
      if(embedded_url == 0) // Main url
        gServerTable[requests[*url_idx].index.svr_idx].main_url_host = 1; // For main url
      else
        gServerTable[requests[*url_idx].index.svr_idx].main_url_host = 0;
    }
  }
  else
  { 
    SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012073_ID, CAV_ERR_1012073_MSG);
  }
  /*Added for filling all server in gSessionTable*/
  CREATE_AND_FILL_SESS_HOST_TABLE_ENTRY(*url_idx, "Method called from ldap_set_url");

  return NS_PARSE_SCRIPT_SUCCESS;
}

static int parse_ldap_url(FILE *flow_fp, char *flow_file, char *starting_quotes, int sess_idx, char embedded_urls, int *url_idx, char **closing_quotes, int operation)
{
  char attribute_value[MAX_LINE_LENGTH];
  char attribute_name[128 + 1];
  // Need to keep copy as attribute_value gets overriten
  char url[MAX_LINE_LENGTH] = ""; // Copy URL

  NSDL2_PARSING(NULL, NULL, "Method Called starting_quotes=%s closing_quotes=%s", starting_quotes, *closing_quotes);

  //starting_quotes will point to the starting of the argument quotes
  //In case ret is NS_PARSE_SCRIPT_SUCCESS, continue reading next argument
  //else if starting quotes is NULL, return

  if(get_next_argument(flow_fp, starting_quotes, attribute_name, attribute_value, closing_quotes, 0) == NS_PARSE_SCRIPT_ERROR)
    return NS_PARSE_SCRIPT_ERROR;

  NSDL2_PARSING(NULL, NULL, "After get_next_argument starting_quotes=%s closing_quotes=%s", starting_quotes, *closing_quotes);
  if (strcasecmp(attribute_name, "URL") == 0)
  {
    NSDL2_PARSING(NULL, NULL, "URL [%s] found at script_line %d", attribute_value, script_ln_no);

    if(ldap_set_url(attribute_value, flow_file, sess_idx, url_idx, embedded_urls, operation) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;
      strcpy(url, attribute_value); // TODO : we have a need to keep url or not
  } else {
     SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012224_ID, CAV_ERR_1012224_MSG, attribute_name);
  }
  return 0;
}

int parse_and_extract_pagename(FILE* flow_fp, char *flow_file, char *script_line, char **page_end_ptr, char *pagename){
  int ret;
  
  NSDL2_PARSING(NULL, NULL, "Method Called");

  ret = extract_pagename(flow_fp, flow_file, script_line, pagename, page_end_ptr);

  if(ret == NS_PARSE_SCRIPT_ERROR)
    return NS_PARSE_SCRIPT_ERROR;
  
  return 0;
}

int parse_ldap_login(FILE *flow_fp, FILE *outfp,  char *flow_file, char *flow_outfile, int sess_idx, int *url_idx, int operation)
{
 // int  url_idx = 0;
  int ret;
  char *page_end_ptr = NULL;
  char embedded_urls = 0;
  char pagename[MAX_LINE_LENGTH + 1];
  char *closing_quotes = NULL;
  char *starting_quotes = NULL;
  
  NSDL2_PARSING(NULL, NULL, "Method Called");
  
  //Extract PageName from script_line
  //Page name would always be the first mandatory argument in ns_web_url

  ret = parse_and_extract_pagename(flow_fp, flow_file, script_line, &page_end_ptr, pagename);
  if(ret == NS_PARSE_SCRIPT_ERROR) return NS_PARSE_SCRIPT_ERROR;

  if((parse_and_set_pagename((operation==LDAP_LOGIN)?"ns_ldap_login":"ns_ldap_logout", "ns_web_url", flow_fp, flow_file, script_line, outfp, flow_outfile, sess_idx, &page_end_ptr, pagename)) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;

  if (create_requests_table_entry(url_idx) != SUCCESS) // Fill request type inside create table entry
  { 
    SCRIPT_PARSE_ERROR(NULL, "get_url_requets(): Could not create ldap request entry while parsing line %d in file %s\n", script_ln_no, flow_filename);
  }

  gPageTable[g_cur_page].first_eurl = *url_idx;
 // gPageTable[g_cur_page].num_eurls++; // Increment urls

  starting_quotes = page_end_ptr;
  closing_quotes = starting_quotes;

  NSDL2_PARSING(NULL, NULL, " Starting Quotes=%s, Starting quotes=%p, Closing_quotes=%s, Closing_quotes=%p, after read_till_start_of_next_quotes()", starting_quotes, starting_quotes, closing_quotes, closing_quotes);

  ret = read_till_start_of_next_quotes(flow_fp, flow_file, closing_quotes, &starting_quotes, embedded_urls, outfp);

  NSDL2_PARSING(NULL, NULL, "Starting Quotes=%s, closing_quotes = [%s]", starting_quotes, closing_quotes);

  if(ret == NS_PARSE_SCRIPT_ERROR)
  {
    if(starting_quotes == NULL)
      return NS_PARSE_SCRIPT_ERROR;
    else  //Starting quotes not found and some text found
      SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012160_ID, CAV_ERR_1012160_MSG);
  }
  if(parse_ldap_url(flow_fp, flow_file, starting_quotes, sess_idx, embedded_urls, url_idx, &closing_quotes, operation) == NS_PARSE_SCRIPT_ERROR)
    return NS_PARSE_SCRIPT_ERROR;

  // To check ); after URL argument in the same line 
  closing_quotes++;
  CLEAR_WHITE_SPACE(closing_quotes);
  if(strncmp(closing_quotes, ");" ,2) == 0){
    return NS_PARSE_SCRIPT_SUCCESS;
  }

  ret = read_till_start_of_next_quotes(flow_fp, flow_file, closing_quotes,  &starting_quotes, embedded_urls, outfp);

  NSDL2_PARSING(NULL, NULL, "Starting Quotes=%s, Starting quotes=%p, Closing_quotes=%s, Closing_quotes=%p, after read_till_start_of_next_quotes()", 
                  starting_quotes, starting_quotes, closing_quotes, closing_quotes);
  if(ret == NS_PARSE_SCRIPT_ERROR)
  {
    if(starting_quotes == NULL)
      return NS_PARSE_SCRIPT_ERROR;
  }
  if (strncmp(starting_quotes, ");", 2) == 0)
  {
    NSDL2_PARSING(NULL, NULL, "End of function found at script_line %d in file %s",
                        script_ln_no, flow_file);
    return NS_PARSE_SCRIPT_SUCCESS;
  }
  return NS_PARSE_SCRIPT_SUCCESS;
}

static int check_for_special_char(char *ptr, int len, char only_char)
{
  NSDL2_PARSING(NULL, NULL, "Method Called");
NSDL2_PARSING(NULL, NULL, "harshit ptr = [%s], len=[%d]",ptr,len);
  while(len)
  {
     if(!only_char)
     {
       if(*ptr == '|' ||  *ptr == '\\' || *ptr == '/' || *ptr == '=' || *ptr == '*')
         return 1;
     }
     else
     {
        if(*ptr == only_char)
        {
          return 1;
        } 
     }
     len-- ;
	ptr++;
  }
  return 0;
}


int parse_attr_val(int url_idx, int sess_idx, FILE *flow_fp, char *start_ptr, char **end_ptr, char *embedded_urls, char *flow_file, char **starting_quotes, int operation, FILE *outfp)
{
  char *ptr, *ptr1;
  char final_buf[1024*10];
  int ret;
  int multiline_comment = 0;

  NSDL2_PARSING(NULL, NULL, "Method called, start_ptr = %s", start_ptr);

  CLEAR_WHITE_SPACE(start_ptr);
  if(strncasecmp(start_ptr, "ATTR_LIST_BEGIN,", 16))
    SCRIPT_PARSE_ERROR(script_line, "passed tag is not ldap ATTR_LIST_BEGIN");

  //read next line from script file
  while(read_line_and_comment(flow_fp, outfp))
  {
    CLEAR_WHITE_SPACE(script_line);
    CLEAR_WHITE_SPACE_FROM_END(script_line);
    if(*script_line =='\0'||*script_line =='\n' )
	continue;
    if(multiline_comment)
    {
       if(strstr(script_line, "*/") != NULL )
       {
          NSDL3_PARSING(NULL, NULL, "End of multi-line comment");
          multiline_comment = 0;
	  if(strstr(script_line, "/*") != NULL )
          {
            NSDL3_PARSING(NULL, NULL, "End of multi-line comment");
            multiline_comment = 1;
            continue;
          }
       }
       continue;
    }
    else
    {
   //Handling starting of multi-line comments
      if(strncmp(script_line, "/*", 2) == 0)
      {
         NSDL3_PARSING(NULL, NULL, "Start of multi-line comment");
         multiline_comment = 1;
       	 if(strstr(script_line, "*/") != NULL )
         {
	   NSDL3_PARSING(NULL, NULL, "End of multi-line comment");
           multiline_comment = 0;
           continue;
         }
         continue;
       }
     //Handling of single-line comments
       else if(strncmp(script_line, "//", 2) == 0)
       {
         NSDL3_PARSING(NULL, NULL, "single-line comment found");
         *script_line = '\0';
         continue;
       }
    }
    //read first quote, it can't be an escaped quote
    start_ptr = strchr(script_line, '"');

    if(!start_ptr)
    {
      if((*end_ptr = strstr(script_line, "ATTR_LIST_END);")) != NULL) // brecket is in the same line
      {
        final_buf[strlen(final_buf) - 1] = '\0';
        segment_line(&(requests[url_idx].proto.ldap.attributes), final_buf, 0, script_ln_no, sess_idx, flow_file); 
        NSDL2_PARSING(NULL, NULL, "ldap attr_list block is over, *end_ptr = %s final_buf = [%s]", *end_ptr, final_buf);
        return NS_PARSE_SCRIPT_SUCCESS;
      } else if((*end_ptr = strstr(script_line, "ATTR_LIST_END")) != NULL) // bracket is in new line, consume one extra line
      {
        final_buf[strlen(final_buf) - 1] = '\0';
        segment_line(&(requests[url_idx].proto.ldap.attributes), final_buf, 0, script_ln_no, sess_idx, flow_file); 
        NSDL2_PARSING(NULL, NULL, "ldap attr_list block is over, *end_ptr = %s final_buf = [%s]", *end_ptr, final_buf);
        read_line_and_comment(flow_fp, outfp);
        *starting_quotes = script_line;
        return NS_PARSE_SCRIPT_SUCCESS;
      }
      SCRIPT_PARSE_ERROR(start_ptr, "Starting quote not found");
    }
    start_ptr++; //skip to next char after quote

    //getting operation, name and values....

    if(operation == LDAP_MODIFY){
      if((ptr = strstr(start_ptr, "OPERATION")) != NULL){
        ptr += 9;
        CLEAR_WHITE_SPACE(ptr);
        if(*ptr != '='){
          NSDL2_PARSING(NULL, NULL," Error: '=' is missing after OPERATION, ptr = %s", ptr);
          return NS_PARSE_SCRIPT_ERROR;
        }else{
          ptr++;
          CLEAR_WHITE_SPACE(ptr);
          if((ptr1 = strchr(ptr, ',')) != NULL){
            if(!strncasecmp(ptr, "ADD", ptr1-ptr) || !strncasecmp(ptr, "DELETE", ptr1-ptr) || !strncasecmp(ptr, "MODIFY", ptr1-ptr)){
              strncat(final_buf, ptr, ptr1-ptr); 
              strcat(final_buf, ","); 
            }
            else{
               //error operation other than add, delete, modify
              NSDL2_PARSING(NULL, NULL,"Invalid  operation, ptr = %s", ptr);
              return NS_PARSE_SCRIPT_ERROR;
            }
          }else{
            NSDL2_PARSING(NULL, NULL," Error: ',' is missing after OPERATION in ldap modify operation, ptr = %s", ptr);
            return NS_PARSE_SCRIPT_ERROR;
          }
        }
      }else{
        NSDL2_PARSING(NULL, NULL,"OPERATION attribute is not present in LDAP modify operation, ptr = %s", ptr);
        return NS_PARSE_SCRIPT_ERROR;
      }
    }

    if((ptr = strstr(start_ptr, "NAME")) != NULL){
      ptr += 4;
    }else{
      NSDL2_PARSING(NULL, NULL,"Name attribute is not present, ptr = %s", ptr);
      return NS_PARSE_SCRIPT_ERROR;
    }

    NSDL2_PARSING(NULL, NULL,"after name, ptr = [%s]", ptr);
    CLEAR_WHITE_SPACE(ptr);

    if(*ptr != '='){
      NSDL2_PARSING(NULL, NULL, " '=' is missing after NAME attribute, ptr = %s", ptr);
      return NS_PARSE_SCRIPT_ERROR;
    }
     
    ptr++;

   // CLEAR_WHITE_SPACE(ptr);
   // TODO: escape ',' if it is coming in value

    if((ptr1 = strchr(ptr, ',')) == NULL){
       NSDL2_PARSING(NULL, NULL, " ',' is missing after NAME attribute value, ptr = %s", ptr);
       return NS_PARSE_SCRIPT_ERROR; 
    }

    ret = check_for_special_char(ptr, ptr1 - ptr, 0);
    if(ret){
      NSDL2_PARSING(NULL, NULL, " special char '|' '*' ''' '/' not allowed in NAME or VALUE, ptr = %s", ptr);
      SCRIPT_PARSE_ERROR(script_line, "special char '|' ''' '/' not allowed in NAME or VALUE, NAME/VALUE = %s", ptr);
    }

    strncat(final_buf, ptr, ptr1-ptr); 
    strcat(final_buf, "="); 
    if((ptr = strstr(ptr1, "VALUE")) == NULL){
      NSDL2_PARSING(NULL, NULL, " VALUE is missing in ldap attr name value pair, ptr = %s", ptr);
       return NS_PARSE_SCRIPT_ERROR;
    }

    ptr += 5;
    CLEAR_WHITE_SPACE(ptr);
    if(*ptr != '='){
     NSDL2_PARSING(NULL, NULL, " VALUE is missing in ldap attr name value pair, ptr = %s", ptr);
       return NS_PARSE_SCRIPT_ERROR;
    }

    ptr++;
    if((ptr1 = strchr(ptr, '"')) == NULL){
      NSDL2_PARSING(NULL, NULL, " double quotes is missing, ptr = %s", ptr);
       return NS_PARSE_SCRIPT_ERROR;
    }
    
    ret = check_for_special_char(ptr, ptr1 - ptr, '|');
    if(ret){
      NSDL2_PARSING(NULL, NULL, " special char '|' '*' ''' '/' not allowed in NAME or VALUE, ptr = %s", ptr);
      SCRIPT_PARSE_ERROR(script_line, "special char '|' ''' '/' not allowed in NAME or VALUE, NAME/VALUE = %s", ptr);
    }
    strncat(final_buf, ptr, ptr1-ptr);
    strcat(final_buf, "|");
  }
  return 0;
}

int parse_ldap_operation(FILE *flow_fp, FILE *outfp, char *flow_file, char *flowout_file, int sess_idx, int operation, int *url_exist)
{
  int ret;
  int url_idx;
  int wrtn_bytes = 0;
  char attr_name[1024*10];
  char pagename[1024 +1];
  char attribute_value[MAX_LINE_LENGTH];
  char attribute_name[128 + 1];
  char *page_end_ptr = NULL;
  char *closing_quotes = NULL;
  char *starting_quotes = NULL;
  char *tmp_attr_ptr;
  char *attr_name_ptr = tmp_attr_ptr = attr_name;
  int d_flag=0;int s_flag=0;int b_flag=0;int f_flag=0;int t_flag=0;int m_flag=0; 

  NSDL2_PARSING(NULL, NULL, "Method Called");

  if(parse_and_extract_pagename(flow_fp, flow_file, script_line, &page_end_ptr, pagename) == NS_PARSE_SCRIPT_ERROR)
    return NS_PARSE_SCRIPT_ERROR;

  NSDL2_PARSING(NULL, NULL, "ldap_pagename = [%s]", pagename);
  if((parse_and_set_pagename("ns_ldap_", "ns_web_url", flow_fp, flow_file, script_line, outfp, flowout_file, sess_idx, &page_end_ptr, pagename)) == NS_PARSE_SCRIPT_ERROR)
     return NS_PARSE_SCRIPT_ERROR;

  create_requests_table_entry(&url_idx); // Fill request type inside create table entry

  gPageTable[g_cur_page].first_eurl = url_idx;
  //gPageTable[g_cur_page].num_eurls++; // Increment urls

  starting_quotes = page_end_ptr;
  closing_quotes = starting_quotes;

  NSDL2_PARSING(NULL, NULL, "page_end_ptr = [%s] closing= [%s]", page_end_ptr, closing_quotes);
  ret = read_till_start_of_next_quotes(flow_fp, flow_file, closing_quotes, &starting_quotes, 0, outfp);
  if(ret == NS_PARSE_SCRIPT_ERROR)
    return NS_PARSE_SCRIPT_ERROR;

  NSDL2_PARSING(NULL, NULL, "before while loop: starting_quotes=%p closing_quotes=%s page_end_ptr = %s", starting_quotes, closing_quotes, page_end_ptr); 
  while(1)
  {
    if(get_next_argument(flow_fp, starting_quotes, attribute_name, attribute_value, &closing_quotes, 0) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;

    NSDL2_PARSING(NULL, NULL, "in while first call: starting_quotes=%p closing_quotes=%s", starting_quotes, closing_quotes); 
    if(!strncasecmp(attribute_name, "URL", 3)){
      if(*url_exist){
        SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012218_ID, CAV_ERR_1012218_MSG, "URL");
      }
      *url_exist = 1;
      if(parse_ldap_url(flow_fp, flow_file, starting_quotes, sess_idx, 0, &url_idx, &closing_quotes, operation) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
    }else if(!strncasecmp(attribute_name, "ATTR_LIST_BEGIN", 15)){
      if(parse_attr_val(url_idx, sess_idx, flow_fp, script_line, &closing_quotes, 0, flow_file, &starting_quotes, operation, outfp) == NS_PARSE_SCRIPT_ERROR)
      {
        NSDL2_PARSING(NULL, NULL, "error in ldap attribute parse");
        return NS_PARSE_SCRIPT_ERROR;
      }else{
        CHECK_MAND_ARGS
        return NS_PARSE_SCRIPT_SUCCESS;
      }
    }else if(!strncasecmp(attribute_name, "DN", 2)){
      if(d_flag){
         SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012218_ID, CAV_ERR_1012218_MSG, "DN");
      }
       d_flag++;
      NSDL2_PARSING(NULL, NULL, "dn name=[%s]", attribute_value); 
      segment_line(&(requests[url_idx].proto.ldap.dn), attribute_value, 0, script_ln_no, sess_idx, flow_file);
    }else if(!strncasecmp(attribute_name, "BASE", 4) || !strncasecmp(attribute_name, "NEW_RDN", 7)){
      if(b_flag){
         SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012218_ID, CAV_ERR_1012218_MSG, "BASE");
      }
      b_flag++;
      NSDL2_PARSING(NULL, NULL, "base name=[%s]", attribute_value); 
      segment_line(&(requests[url_idx].proto.ldap.base), attribute_value, 0, script_ln_no, sess_idx, flow_file);
    }else if(!strncasecmp(attribute_name, "SCOPE", 5) || !strncasecmp(attribute_name, "DELETE_OLD", 10)){
      if(s_flag){
         SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012218_ID, CAV_ERR_1012218_MSG, "SCOPE");
      }
       s_flag++;
      NSDL2_PARSING(NULL, NULL, "scope=[%s]", attribute_value); 
      if(!strncasecmp(attribute_value, "Base", 4)){
        segment_line(&(requests[url_idx].proto.ldap.scope), "0", 0, script_ln_no, sess_idx, flow_file);
      }
      else if(!strncasecmp(attribute_value, "OneLevel", 8)){
        segment_line(&(requests[url_idx].proto.ldap.scope), "1", 0, script_ln_no, sess_idx, flow_file);
      }
      else if(!strncasecmp(attribute_value, "Subtree", 7)){
        segment_line(&(requests[url_idx].proto.ldap.scope), "2", 0, script_ln_no, sess_idx, flow_file);
      }
      else{
        fprintf(stderr, "Invalid Base Level for Base attribute in ldap search operation\n");
        return NS_PARSE_SCRIPT_ERROR;
      }
    }else if(!strncasecmp(attribute_name, "FILTER", 6)){

      if(f_flag){
        SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012218_ID, CAV_ERR_1012218_MSG, "FILTER");
      }
      f_flag++;
      NSDL2_PARSING(NULL, NULL, "filter=[%s]", attribute_value); 
      segment_line(&(requests[url_idx].proto.ldap.filter), attribute_value, 0, script_ln_no, sess_idx, flow_file);
    }else if(!strncasecmp(attribute_name, "TIME_OUT", 8)){
      if(t_flag){
        SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012218_ID, CAV_ERR_1012218_MSG, "TIME_OUT");
      }
      t_flag++;
      NSDL2_PARSING(NULL, NULL, "time out=[%s]", attribute_value); 
      segment_line(&(requests[url_idx].proto.ldap.time_limit), attribute_value, 0, script_ln_no, sess_idx, flow_file);
    }else if(!strncasecmp(attribute_name, "MODE", 4)){
      if(m_flag){
        SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012218_ID, CAV_ERR_1012218_MSG, "MODE");
      }
      m_flag++;
      NSDL2_PARSING(NULL, NULL, "mode=[%s]", attribute_value);

      if(!strncasecmp(attribute_value, "SYNC", 4)){
        segment_line(&(requests[url_idx].proto.ldap.mode), "0", 0, script_ln_no, sess_idx, flow_file);
      }
      else if(!strncasecmp(attribute_value, "ASYNC", 5)){
        segment_line(&(requests[url_idx].proto.ldap.mode), "1", 0, script_ln_no, sess_idx, flow_file);
      }
      else{
        fprintf(stderr, "Invalid value for Mode attribute in ldap search operation\n");
        return NS_PARSE_SCRIPT_ERROR;
      }
    }else if (!strncasecmp(attribute_name, "ATTR_VALUE", 10)){
      NSDL2_PARSING(NULL, NULL, "attr_val=[%s]", attribute_value); 
      segment_line(&(requests[url_idx].proto.ldap.attr_value), attribute_value, 0, script_ln_no, sess_idx, flow_file);
    }else if(!strncasecmp(attribute_name, "ATTR_NAME", 9)){
      NSDL2_PARSING(NULL, NULL, "attr_name=[%s]", attribute_value); 
      wrtn_bytes = sprintf(attr_name_ptr, "%s|", attribute_value); 
      attr_name_ptr += wrtn_bytes; 
    }else if(!strncasecmp(attribute_name, "DEL_OLD", 7)){
      NSDL2_PARSING(NULL, NULL, "attr_val=[%s]", attribute_value);
      if (ns_is_numeric(attribute_value) == 0) {
        SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012452_ID, CAV_ERR_1012452_MSG, "DEL_OLD");
      }
      int val = atoi(attribute_value);
      if(val > 1 || val < 0){
        SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012452_ID, CAV_ERR_1012452_MSG, "DEL_OLD");
      }
      segment_line(&(requests[url_idx].proto.ldap.scope), attribute_value, 0, script_ln_no, sess_idx, flow_file);//using scope field for rename del_old field, take same from segment in ldap make request
    }

    // To check ); after URL argument in the same line 
    char *tmp = closing_quotes;
    tmp++;
    CLEAR_WHITE_SPACE(tmp);
    if(strncmp(tmp, ");" ,2) == 0){
      *(attr_name_ptr - 1) = '\0';
      segment_line(&(requests[url_idx].proto.ldap.attr_name), tmp_attr_ptr, 0, script_ln_no, sess_idx, flow_file);
      CHECK_MAND_ARGS
      return NS_PARSE_SCRIPT_SUCCESS;
    }

    ret = read_till_start_of_next_quotes(flow_fp, flow_file, closing_quotes,  &starting_quotes, 0, outfp);
    NSDL2_PARSING(NULL, NULL, "Starting Quotes=%s, Starting quotes=%p, Closing_quotes=%s, Closing_quotes=%p, after read_till_start_of_next_quotes()", starting_quotes, starting_quotes, closing_quotes, closing_quotes);
  
    if(ret == NS_PARSE_SCRIPT_ERROR)
    {
      if(starting_quotes == NULL)
        return NS_PARSE_SCRIPT_ERROR;
    }
    if (strncmp(starting_quotes, ");", 2) == 0){
      *(attr_name_ptr - 1) = '\0';
      segment_line(&(requests[url_idx].proto.ldap.attr_name), tmp_attr_ptr, 0, script_ln_no, sess_idx, flow_file);
  
      NSDL2_PARSING(NULL, NULL, "End of function found at script_line %d in file %s",
                        script_ln_no, flow_file);
      CHECK_MAND_ARGS
      return NS_PARSE_SCRIPT_SUCCESS;
    }
  }//end of while loop
}

int ns_parse_ldap(FILE *flow_fp, FILE *outfp,  char *flow_file, char *flow_outfile, int sess_idx, int operation){

  int ret;
  int url_idx;
  int url_exist = 0;

  NSDL2_PARSING(NULL, NULL, "Method Called");

  switch(operation){
    case LDAP_LOGIN:
    case LDAP_LOGOUT:
        ret = parse_ldap_login(flow_fp, outfp, flow_file, flow_outfile, sess_idx, &url_idx, operation);
        break;
    
    case LDAP_ADD:
    case LDAP_MODIFY:
    case LDAP_RENAME:
    case LDAP_SEARCH:
    case LDAP_UPDATE:
    case LDAP_DELETE:{
        ret = parse_ldap_operation(flow_fp, outfp, flow_file, flow_outfile, sess_idx, operation, &url_exist);
        if(!url_exist){
          SCRIPT_PARSE_ERROR_EXIT_EX(script_line, CAV_ERR_1012225_ID, CAV_ERR_1012225_MSG);
        }
      }
      break;

    default:
       NSDL2_PARSING(NULL, NULL, "Operation not supported.");
       return -1;
  }
  return ret;
}

inline int get_ldap_value_from_segments(connection *cptr, StrEnt_Shr* seg_tab_ptr, char **buf) 
{
  int i, ret, total_len = 0;
  char *to_fill;
  VUser *vptr = cptr->vptr;

  NSDL4_LDAP(vptr, NULL, "Method Called");
 
  // Get all segment values in a vector
  // Note that some segment may be parameterized

  NS_RESET_IOVEC(g_scratch_io_vector);

  if((ret = insert_segments(vptr, cptr, seg_tab_ptr, &g_scratch_io_vector, NULL, 0, 1, 1, cptr->url_num, SEG_IS_NOT_REPEAT_BLOCK)) < 0)
  { 
    NSEL_MAJ(NULL, NULL, ERROR_ID, ERROR_ATTR, 
        "Error in insert_segments(), return value = %d\n", g_scratch_io_vector.cur_idx);
     
    if(ret == MR_USE_ONCE_ABORT)
      return ret;
    return(-1);
  }

  // Calculate total lenght of all components which are in vector
  for (i = 0; i < g_scratch_io_vector.cur_idx; i++) {
    total_len += g_scratch_io_vector.vector[i].iov_len;
  }

  MY_MALLOC(*buf, total_len + 1, "buf", -1);
  to_fill = *buf;
  
  NSDL4_LDAP(vptr, NULL, "total_len = %d", total_len);

  for (i = 0; i < g_scratch_io_vector.cur_idx; i++) {
    bcopy(g_scratch_io_vector.vector[i].iov_base, to_fill, g_scratch_io_vector.vector[i].iov_len);
    to_fill += g_scratch_io_vector.vector[i].iov_len;
  }
  *to_fill = 0; // NULL terminate

  NSDL4_LDAP(vptr, NULL, "Concated value = %s", to_fill);

  NS_FREE_RESET_IOVEC(g_scratch_io_vector);
  return 0;
}


/* This method will get the values from segments and fill that value to corresponding structure for diffrent operations 
 * These filled structre will passed to their request making methods. 
*/
void make_ldap_request(connection *cptr, unsigned char *buffer, int *length){

  NSDL2_LDAP(NULL, cptr, "Method called");

  switch(cptr->url_num->proto.ldap.operation){

   case LDAP_LOGIN:
    {
      NSDL2_LDAP(NULL, cptr, "Going to get segments for login");
      Authentication ptr;
      memset(&ptr, 0, sizeof(Authentication));
      // get_ldap_value_from_segments(cptr, &cptr->url_num->proto.ldap.dn, &ptr.dn_name);
      // TODO : deside type simple/sasl here based on ldp and ldaps
      //seg_tab_ptr = cptr->url_num->proto->ldap->dn;
      //get_ldap_value_from_segments(cptr, cptr->url_num->proto.ldap.dn, &ptr->auth->_name);
      get_ldap_value_from_segments(cptr, &cptr->url_num->proto.ldap.username, &ptr.dn_name);
      get_ldap_value_from_segments(cptr, &cptr->url_num->proto.ldap.passwd, &ptr.auth_name);
      make_bind_request(cptr, &buffer, length, ptr);
      break;
    }
    case LDAP_LOGOUT:
      make_unbind_request(cptr, &buffer, length);
      break;

    case LDAP_ADD:
    case LDAP_MODIFY:
    {
      NSDL2_LDAP(NULL, cptr, "Going to get segments for add");
      Add ptr;
      memset(&ptr, 0, sizeof(Add));
      // Set dn here
      get_ldap_value_from_segments(cptr, &cptr->url_num->proto.ldap.dn, &ptr.dn);
      // set message buf for attribute name and value
      get_ldap_value_from_segments(cptr, &cptr->url_num->proto.ldap.attributes, &ptr.msg_buf);
      NSDL2_LDAP(NULL, cptr, "msg_buf = [%s]", ptr.msg_buf);

      make_add_request(cptr, &buffer, length, ptr, cptr->url_num->proto.ldap.operation);
      break;
    }
    case LDAP_RENAME:
    {
      NSDL2_LDAP(NULL, cptr, "Going to get segments for rename");
      Rename ptr;
      memset(&ptr, 0, sizeof(Rename));
      // Set dn here
      get_ldap_value_from_segments(cptr, &cptr->url_num->proto.ldap.dn, &ptr.dn);
      NSDL2_LDAP(NULL, cptr, "old_dn = [%s]", ptr.dn);
      // set message buf for attribute name and value
      get_ldap_value_from_segments(cptr, &cptr->url_num->proto.ldap.base, &ptr.new_dn);
      NSDL2_LDAP(NULL, cptr, "new_dn = [%s]", ptr.new_dn);
      get_ldap_value_from_segments(cptr, &cptr->url_num->proto.ldap.scope, &ptr.del_old);

      make_rename_request(cptr, &buffer, length, ptr);
      break;
    }
    case LDAP_SEARCH:
    {
      NSDL2_LDAP(NULL, cptr, "Going to get segments for search");
      Search ptr;
      memset(&ptr, 0, sizeof(Search));
      // Set base here
      get_ldap_value_from_segments(cptr, &cptr->url_num->proto.ldap.base, &ptr.base);
      // set scope here
      get_ldap_value_from_segments(cptr, &cptr->url_num->proto.ldap.scope, &ptr.scope);
      // set filter here
      get_ldap_value_from_segments(cptr, &cptr->url_num->proto.ldap.filter, &ptr.filter);
      // set time limit here
      get_ldap_value_from_segments(cptr, &cptr->url_num->proto.ldap.time_limit, &ptr.timelimit);
      // set deref aliasses here
      get_ldap_value_from_segments(cptr, &cptr->url_num->proto.ldap.deref_aliases, &ptr.deref);
      // set types only here
      get_ldap_value_from_segments(cptr, &cptr->url_num->proto.ldap.types_only, &ptr.typesonly);
      // set size only here
      get_ldap_value_from_segments(cptr, &cptr->url_num->proto.ldap.size_limit, &ptr.sizelimit);
      // set message buf for attribute name and value
      get_ldap_value_from_segments(cptr, &cptr->url_num->proto.ldap.attr_name, &ptr.attribute);

      make_search_request(cptr, &buffer, length, ptr);
      break;

    }
    case LDAP_DELETE:
    {
      NSDL2_LDAP(NULL, cptr, "Going to get segments for delete");
      Delete ptr;
      memset(&ptr, 0, sizeof(Delete));
      // Set dn here
      get_ldap_value_from_segments(cptr, &cptr->url_num->proto.ldap.dn, &ptr.dn);
      make_delete_request(cptr, &buffer, length, ptr);
      break;

    }
    default: 
    //TODO: Put evet log
    END_TEST_RUN
  }
}

//log ldap request
void log_ldap_req(connection *cptr, VUser *vptr, int size)
{
  /*
  VUser *vptr;
  vptr = (VUser *)cptr->vptr;
  int request_type = cptr->url_num->request_type;

  NSDL2_LDAP(NULL, cptr, "Method called");

  if (!((runprof_table_shr_mem[vptr->group_num].gset.debug & DM_LOGIC4) && 
        (request_type == LDAP_REQUEST &&
         (runprof_table_shr_mem[vptr->group_num].gset.module_mask & MM_LDAP))))
    return;
  */

  char log_file[1024];
  char log_req_file[1024];
  int log_fd;
  int log_data_fd;
  int ret;

  // Log file name format is url_rep_<nvm_id>_<user_id>_<sess_inst>_<pg_inst>_<url_inst>_<sess_id>_<page_id>_<url_id>
  // url_id is not yet implemented (always 0)
  SAVE_REQ_REP_FILES
  sprintf(log_req_file, "%s/logs/%s/ldap_req_session_%hd_%u_%u_%d_0_%d_%d_%d_0.dat", 
          g_ns_wdir, req_rep_file_path, child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,
          vptr->group_num, GET_SESS_ID_BY_NAME(vptr),
          GET_PAGE_ID_BY_NAME(vptr));
 
  if((log_data_fd = open(log_req_file, O_CREAT|O_WRONLY|O_APPEND|O_CLOEXEC, 00666)) < 0)
    fprintf(stderr, "Error: Error in opening file for logging URL request\n");
  else
  {
    NSDL2_LDAP(NULL, cptr, "Logging ldap request binary data");
    ret = write(log_data_fd, cptr->free_array, cptr->content_length);
    if(ret == -1)
      fprintf(stderr, "Error: Error in dumping data to file for logging URL request\n"); 
    close(log_data_fd);
  }

  sprintf(log_file, "%s/logs/%s/ldap_req_session_%hd_%u_%u_%d_0_%d_%d_%d_0.xml", 
          g_ns_wdir, req_rep_file_path, child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,
          vptr->group_num, GET_SESS_ID_BY_NAME(vptr),
          GET_PAGE_ID_BY_NAME(vptr));
    
  // Do not change the debug trace message as it is parsed by GUI
  NS_DT4(vptr, cptr, DM_L1, MM_LDAP, "Request is in file '%s'", log_file);
    
  NSDL2_LDAP(NULL, cptr, "Going to open file %s", log_file);

  if((log_fd = open(log_file, O_CREAT|O_CLOEXEC|O_WRONLY|O_APPEND, 00666)) < 0)
    fprintf(stderr, "Error: Error in opening file for logging URL request\n");
  else
  {
    char buf[LDAP_RESP_BUF];
    NSDL2_LDAP(NULL, cptr, "Going to call ldap_decode for request");
    ldap_decode(cptr, cptr->free_array, buf, cptr->content_length, log_fd); 
    close(log_fd);
  }
}

void debug_log_ldap_req(connection *cptr, int size)
{
  VUser *vptr;
  vptr = (VUser *)cptr->vptr;
  int request_type = cptr->url_num->request_type;

  NSDL2_LDAP(NULL, cptr, "Method called");

  if (!(((runprof_table_shr_mem[vptr->group_num].gset.debug & DM_LOGIC4) && 
      (request_type == LDAP_REQUEST || request_type == LDAPS_REQUEST) && 
      (runprof_table_shr_mem[vptr->group_num].gset.module_mask & MM_LDAP))||
      ((NS_IF_PAGE_DUMP_ENABLE && (runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_ALL_SESS || runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_PAGE_ON_HEADER_BASED) && (runprof_table_shr_mem[vptr->group_num].gset.trace_level > TRACE_URL_DETAIL)))))
  {
     NSDL2_LDAP(NULL, cptr, "Either debug is disable or page dump feature is disable.hence returning");
     return;
  }

  log_ldap_req(cptr, vptr, size);
}


void handle_ldap_write(connection *cptr,  u_ns_ts_t now){
  int bytes_sent;
  VUser *vptr = cptr->vptr; 
  char *buf = cptr->free_array + cptr->tcp_bytes_sent;
  int bytes_left_to_send = cptr->bytes_left_to_send;

  NSDL2_DNS(NULL, cptr, "Method called cptr=%p conn state=%d, proto_state = %d, now = %u", 
							                   cptr, cptr->conn_state, cptr->proto_state, now);

  if(cptr->request_type == LDAPS_REQUEST)
  {
    ERR_clear_error();
    bytes_sent = SSL_write(cptr->ssl, buf, bytes_left_to_send);
    switch (SSL_get_error(cptr->ssl, bytes_sent))
    {
      case SSL_ERROR_NONE:
        cptr->tcp_bytes_sent += bytes_sent;
        average_time->tx_bytes += bytes_sent;
        if(SHOW_GRP_DATA) {
          avgtime *lol_average_time;
          lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
          lol_average_time->tx_bytes += bytes_sent;
        }
        if (bytes_sent >= bytes_left_to_send)
        {
          cptr->bytes_left_to_send -= bytes_sent;
          //all sent
          break;
        }
        else
        {
          cptr->bytes_left_to_send -= bytes_sent;
        }
      case SSL_ERROR_WANT_WRITE:
        NSDL3_HTTP(NULL, cptr, "SSL_ERROR_WANT_WRITE occurred", cptr->bytes_left_to_send);
        cptr->conn_state = CNST_SSL_WRITING;
        return;
      case SSL_ERROR_WANT_READ:
        NSDL3_HTTP(NULL, cptr, "SSL_ERROR_WANT_READ occurred", cptr->bytes_left_to_send);
        cptr->conn_state = CNST_SSL_WRITING;
        return;
      case SSL_ERROR_ZERO_RETURN:
        NSDL3_HTTP(NULL, cptr, "SSL_Write ERROR occurred");
        ssl_free_send_buf(cptr);
        retry_connection(cptr, now, NS_REQUEST_SSLWRITE_FAIL);
        return;
      case SSL_ERROR_SSL:
        ERR_print_errors_fp(stderr);
        ssl_free_send_buf(cptr);
        retry_connection(cptr, now, NS_REQUEST_SSLWRITE_FAIL);
        return;
      default:
        NSDL3_HTTP(NULL, cptr, "SSL_Write ERROR %d",errno);
        ssl_free_send_buf(cptr);
        retry_connection(cptr, now, NS_REQUEST_SSLWRITE_FAIL);
        return;
    }
  }
  else
  {
    if((bytes_sent = write(cptr->conn_fd, buf, cptr->bytes_left_to_send)) < 0){
      NSEL_CRI(NULL, cptr, ERROR_ID, ERROR_ATTR,"Sending LDAP request failed, fd = %d, Error=%",
                                                                cptr->conn_fd, nslb_strerror(errno));

      retry_connection(cptr, now, NS_REQUEST_WRITE_FAIL);
      return;
    }

    if(set_socketopt_for_quickack(cptr->conn_fd, 1) < 0) {
      retry_connection(cptr, now, NS_REQUEST_CONFAIL);
      return;
    }

    if (bytes_sent < cptr->bytes_left_to_send) {
      cptr->bytes_left_to_send -= bytes_sent;
      ldap_avgtime->ldap_tx_bytes += bytes_sent;
      cptr->tcp_bytes_sent += bytes_sent;
      cptr->conn_state = CNST_LDAP_WRITING;
      return;
    }
  }

  //log ldap request
#ifdef NS_DEBUG_ON
  debug_log_ldap_req(cptr, cptr->content_length);
#else
  LOG_LDAP_REQ(cptr, cptr->content_length);
#endif

  FREE_AND_MAKE_NOT_NULL(cptr->free_array,"cptr->free_array from ldap", -1);

  // Increase both the counters for ldap and tcp
  ldap_avgtime->ldap_tx_bytes += bytes_sent;
  cptr->tcp_bytes_sent += bytes_sent;
  cptr->bytes_left_to_send -= bytes_sent;

  on_request_write_done (cptr);
  //TODO: logs
  cptr->content_length = 0;
  cptr->req_code_filled = 0;
}

/* Function calls retry_connection() which will ensure normal http like retries for ftp also. */
static inline void
handle_ldap_bad_read (connection *cptr, u_ns_ts_t now)
{
  NSDL2_LDAP(NULL, cptr, "cptr=%p conn state=%d, now = %u", cptr, cptr->conn_state, now);
 
  int timer_type = cptr->timer_ptr->timer_type;

  if (cptr->req_code_filled == 0) {
    if(timer_type == AB_TIMEOUT_KA)
       close_fd_and_release_cptr(cptr, NS_FD_CLOSE_REMOVE_RESP, now);
       /* means that this connection is a keep alive or new and the server closed it without sending any dat*/
       retry_connection(cptr, now, NS_REQUEST_BAD_HDR);
  } else { // TODO check if complete length is recived or not
    Close_connection(cptr, 0, now, NS_REQUEST_OK, NS_COMPLETION_CLOSE); 
  }
}

static inline void
handle_ldap_data_read_complete (connection *cptr, u_ns_ts_t now)
{
  NSDL2_LDAP(NULL, cptr, "cptr=%p conn state=%d, now = %u", cptr, cptr->conn_state, now);

  Close_connection(cptr, 0, now, NS_REQUEST_OK, NS_COMPLETION_CLOSE);
}

// THis method will return ptr of link list for a given offset. 
char *get_ptr_from_cptr_buf(connection *cptr, int offset)
{
  int skip_buffer = offset / COPY_BUFFER_LENGTH; 
  struct copy_buffer *buffer = cptr->buf_head;
  
  while (skip_buffer && buffer) {
    buffer = buffer->next;    
    skip_buffer--;
  }

  // TODO: put logs. 
  if (skip_buffer || !buffer) return NULL;

  return &buffer->buffer[offset % COPY_BUFFER_LENGTH];
}


//this function will copy from cptr->buf_head to buffer
int copy_from_cptr_to_buf(connection *cptr, char *buf)
{
  int total = cptr->total_bytes;
  int len = 0; 
  int copied_len = 0;

  struct copy_buffer *tmp_buf = cptr->buf_head; 
  NSDL2_LDAP(NULL, cptr, "Method called, cptr->total_bytes = %d", cptr->total_bytes);

/*
  if(total > LDAP_RESP_BUF) {
    fprintf(stderr, "Ldap response size is bigger then LDAP_RESP_BUF, so not decoding it for logging\n");
    return -1;
  }
*/

  while(tmp_buf){
    if(tmp_buf->next == NULL)
      len = total - copied_len;
    else
      len = COPY_BUFFER_LENGTH; 
    memcpy(buf + copied_len, tmp_buf->buffer, len);
    copied_len += len;   
    NSDL2_LDAP(NULL, cptr, "copied_len = [%d], buf = %*.*s", copied_len, copied_len, copied_len, buf);
    tmp_buf = tmp_buf->next;
  }
  return 0;
}

char *ldap_buffer = NULL;
int ldap_buffer_len;
	
//log ldap response
void log_ldap_res(connection *cptr, VUser *vptr, char *buf)
{
  NSDL2_LDAP(NULL, cptr, "Method called");
/*
  VUser *vptr;
  vptr = (VUser *)cptr->vptr;
  int request_type = cptr->url_num->request_type;


  if (!((runprof_table_shr_mem[vptr->group_num].gset.debug & DM_LOGIC4) && 
        (request_type == LDAP_REQUEST &&
         (runprof_table_shr_mem[vptr->group_num].gset.module_mask & MM_LDAP))))
    return;
*/
  NSDL2_LDAP(NULL, cptr, "After calling copy_from_cptr_to_buf, buf = %*.*s", cptr->total_bytes, cptr->total_bytes, buf);

  char log_file[1024];
  char log_res_file[1024];
  int log_fd;
  int log_data_fd;
  int ret;

  // Log file name format is url_rep_<nvm_id>_<user_id>_<sess_inst>_<pg_inst>_<url_inst>_<sess_id>_<page_id>_<url_id>
  // url_id is not yet implemented (always 0)
  SAVE_REQ_REP_FILES

  //TODO:
  if(buf)
  {
    sprintf(log_res_file, "%s/logs/%s/ldap_resp_session_%hd_%u_%u_%d_0_%d_%d_%d_0.dat", 
            g_ns_wdir, req_rep_file_path, child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,
            vptr->group_num, GET_SESS_ID_BY_NAME(vptr),
            GET_PAGE_ID_BY_NAME(vptr));
  
    if((log_data_fd = open(log_res_file, O_CREAT|O_CLOEXEC|O_WRONLY|O_APPEND, 00666)) < 0)
      fprintf(stderr, "Error: Error in opening file for logging URL request\n");
    else
    {
      NSDL2_LDAP(NULL, cptr, "Logging ldap response binary data");
      ret = write(log_data_fd, buf, cptr->total_bytes);
      if(ret == -1)
        fprintf(stderr, "Error: Error in dumping data to file for logging URL request\n"); 
      close(log_data_fd);
    }
  }

  sprintf(log_file, "%s/logs/%s/ldap_resp_session_%hd_%u_%u_%d_0_%d_%d_%d_0.xml", 
          g_ns_wdir, req_rep_file_path, child_idx, vptr->user_index, vptr->sess_inst, vptr->page_instance,
          vptr->group_num, GET_SESS_ID_BY_NAME(vptr),
          GET_PAGE_ID_BY_NAME(vptr));
    
  // Do not change the debug trace message as it is parsed by GUI
  if(cptr->tcp_bytes_recv == 0)  //This check to print debug trace only once
    NS_DT4(vptr, cptr, DM_L1, MM_LDAP, "Response is in file '%s'", log_file);
    
  //Since response can come partialy so this will print debug trace many time
  //cptr->tcp_bytes_recv = 0, means this response comes first time
  NSDL2_LDAP(NULL, cptr, "Going to open file %s", log_file);

  if((log_fd = open(log_file, O_CREAT|O_CLOEXEC|O_WRONLY|O_APPEND, 00666)) < 0)
    fprintf(stderr, "Error: Error in opening file for logging URL request\n");
  else
  {
    ret = write(log_fd, ldap_buffer, ldap_buffer_len);
    if(ret != ldap_buffer_len){
      if(ret == -1){
        fprintf(stderr, "Error in logging decoding response for ldap");
      } else {
         fprintf(stderr, "Partial write while writing decoded response for ldap");
      }
    } 
    NSDL2_LDAP(NULL, cptr, "Going to call ldap_decode");
    close(log_fd);
  }
}

void debug_log_ldap_res(connection *cptr, char *buf)
{
  VUser *vptr;
  vptr = (VUser *)cptr->vptr;
  int request_type = cptr->url_num->request_type;

  NSDL2_LDAP(NULL, cptr, "Method called");

  if (!(((runprof_table_shr_mem[vptr->group_num].gset.debug & DM_LOGIC4) &&
      (request_type == LDAP_REQUEST || request_type ==  LDAPS_REQUEST) && 
      (runprof_table_shr_mem[vptr->group_num].gset.module_mask & MM_LDAP))||
      ((NS_IF_PAGE_DUMP_ENABLE && (runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_ALL_SESS || runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_PAGE_ON_HEADER_BASED) && (runprof_table_shr_mem[vptr->group_num].gset.trace_level > TRACE_URL_DETAIL)))))
  {
     NSDL2_LDAP(NULL, cptr, "Either debug is disable or page dump feature is disable.hence returning");
     return;
  }

  log_ldap_res(cptr, vptr, buf);
}

void process_ldap_res(connection *cptr){

  char buf[LDAP_RESP_BUF];
  char *buff_LDAP_resp = buf;

  NSDL2_LDAP(NULL, cptr, "Method called");

  if(cptr->total_bytes > LDAP_RESP_BUF) 
    MY_MALLOC(buff_LDAP_resp, cptr->total_bytes + 1, "new buffer for LDAP response", -1);
  
  // Copy response from link list to a buffer
  if(copy_from_cptr_to_buf(cptr, buff_LDAP_resp) == -1) //buf is of 100k, we may need more than this 
    return;
  
  // Allocate if buffer is null, this buffer is not frees anywhere, single buffer will be used to save response  
  if(ldap_buffer == NULL) {
    MY_MALLOC(ldap_buffer, LDAP_RESP_BUF, "ldap_buffer", -1);
  }

  // Decode ldap in xml format, decoded xml will be used in dubug logging and do_data_processing
  ldap_buffer_len = ldap_decode(cptr, buff_LDAP_resp, ldap_buffer, cptr->total_bytes, -1);
  cptr->bytes = ldap_buffer_len; 
  ldap_buffer_len = msg_len;

#ifdef NS_DEBUG_ON
  debug_log_ldap_res(cptr, buff_LDAP_resp);  
#else
  LOG_LDAP_RES(cptr, buff_LDAP_resp);
#endif

  if(buff_LDAP_resp != buf)
    FREE_AND_MAKE_NULL(buff_LDAP_resp, "LDAP response buffer", -1);
}

/*It will parse length and will move the pointer.*/
#define GET_MSG_LEN(ptr, len) {	\
  /*Check if multi byte*/	\
  int num_len_bytes = 0;	\
  if (ptr[0] & 0x80) {		\
    num_len_bytes = ptr[0];	\
    ptr ++;			\
    len = ldap_read_int(ptr, num_len_bytes); 	\
    ptr += num_len_bytes;	\
  } else {			\
    len = ptr[0];		\
    ptr ++;			\
  }				\
}

inline void get_ldap_msg_opcode(char *buffer, int *opcode) {
  char *ptr = buffer; 
  //int num_len_bytes = 0;
  int len;

  // skip first byte for sequence type which is 0x30. 
  ptr++;

  // check length.
  GET_MSG_LEN(ptr, len);

  // skip msg id type which is integer i.e. 0x2
  ptr++;

  // Check for msg id length..
  GET_MSG_LEN(ptr, len);

  // skip msg id. 
  ptr += len;

  // reached to request. 
  *opcode = ptr[0] & 0x1f;
}



int
handle_ldap_read( connection *cptr, u_ns_ts_t now ) 
{
  VUser *vptr = cptr->vptr;

#ifdef ENABLE_SSL
  /* See the comment below */ // size changed from 32768 to 65536 for optimization : Anuj 28/11/07
  char buf[65536 + 1];    /* must be larger than THROTTLE / 2 */ /* +1 to append '\0' for debugging/logging */
#else
  char buf[4096 + 1];     /* must be larger than THROTTLE / 2 */ /* +1 to append '\0' for debugging/logging */
#endif
  int bytes_read; 
  int request_type;
  char handle = 0;
  int req_len = 0;
  int val;
  short is_partial = 0;
  char *frame_start_ptr;
  //int msg_id_len = 0;
  char *err_buff;
  int err;

  NSDL2_LDAP(NULL, cptr, "conn state=%d, now = %u", cptr->conn_state, now);

  request_type = cptr->url_num->request_type;
  if (request_type != LDAP_REQUEST && request_type != LDAPS_REQUEST) { 
    /* Something is very wrong we should not be here. */
    fprintf(stderr, "Request type is not ldap but still we are in an ldap state. We must not come here. Something is seriusly wrong\n");
    END_TEST_RUN
  }

  cptr->body_offset = 0;     /* Offset will always be from 0; used in get_reply_buf */

  while (1) {
    // printf ("0 Req code=%d\n", cptr->req_code);
    if ( do_throttle )
      bytes_read = THROTTLE / 2;
    else
      bytes_read = sizeof(buf) - 1;
    
    if (request_type == LDAPS_REQUEST) {
      bytes_read = SSL_read(cptr->ssl, buf, bytes_read);

      if (bytes_read > 0) buf[bytes_read] = '\0'; // NULL terminate for printing/logging
      NSDL3_SSL(NULL, cptr, "Read %d bytes. msg=%s", bytes_read, (bytes_read>0)?buf:"-");

      if (bytes_read < 0) {
        err = SSL_get_error(cptr->ssl, bytes_read);
        switch (err) {
        case SSL_ERROR_ZERO_RETURN:  /* means that the connection closed from the server */
          handle_ldap_bad_read(cptr, now);
          return -1;
        case SSL_ERROR_WANT_READ:
          return -1;
          /* It can but isn't supposed to happen */
        case SSL_ERROR_WANT_WRITE:
          fprintf(stderr, "SSL_read error: SSL_ERROR_WANT_WRITE\n");
          handle_ldap_bad_read (cptr, now);
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
              handle_ldap_bad_read (cptr, now);
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
              handle_ldap_bad_read (cptr, now);
          else
            handle_ldap_bad_read (cptr, now);
          return -1;
      }
    } else if (bytes_read == 0) {
      if(cptr->url_num->proto.ldap.operation == LDAP_LOGOUT)
      {
        // delete ldap timeout timers
        delete_ldap_timeout_timer(cptr);

        // This will call close connection
        handle_ldap_data_read_complete(cptr, now);
        cptr->content_length = 0;
        return 0;
      }
      handle_ldap_bad_read (cptr, now);
      //handle_server_close (cptr, now);
      return -1;
    }
  } else {
    bytes_read = read(cptr->conn_fd, buf, bytes_read);

    NSDL2_LDAP(NULL, cptr, "bytes_read = %d", bytes_read);

#ifdef NS_DEBUG_ON
    //if (bytes_read != 10306) printf("rcd only %d bytes\n", bytes_read);
    if (bytes_read > 0) buf[bytes_read] = '\0'; // NULL terminate for printing/logging
    //NSDL3_FTP(NULL, cptr, "Read %d bytes. msg=%s", bytes_read, bytes_read>0?buf:"-");
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
        NSDL3_LDAP(NULL, cptr, "read failed (%s) for main: host = %s [%d]", nslb_strerror(errno),
                   cptr->url_num->index.svr_ptr->server_hostname,
                   cptr->url_num->index.svr_ptr->server_port);
        handle_ldap_bad_read (cptr, now);
        return -1;
      }
    } else if (bytes_read == 0) {
      if(cptr->url_num->proto.ldap.operation == LDAP_LOGOUT)
      {
        // delete ldap timeout timers
        delete_ldap_timeout_timer(cptr);
        
        // This will call close connection
        handle_ldap_data_read_complete(cptr, now);
        cptr->content_length = 0;
        return 0;
      }    
      handle_ldap_bad_read (cptr, now);
      //handle_server_close (cptr, now);
      return -1;
    }
  } 
 
    NSDL2_LDAP(NULL, cptr, "req_code_filled = %d", cptr->req_code_filled);

    // If head of frame is not parsed yet then parse that first.
    // if (cptr->req_code_filled <= cptr->content_length)/*if(cptr->req_code_filled < 0 )*/ 
    {
      /*if(((cptr->total_bytes - cptr->content_length) + bytes_read) < 2){ // length is read partial
        copy_retrieve_data(cptr, buf, bytes_read, cptr->total_bytes);
        cptr->total_bytes +=  bytes_read;
        continue;
      }else */ { // Partial read may done before, in case pratial can be only in one byte 
        copy_retrieve_data(cptr, buf, bytes_read, cptr->total_bytes);
        cptr->total_bytes +=  bytes_read;
        //cptr->cptr->buf_head->buffer; 
        // Take second byte of buffer as it tells about the length of the complete message
        is_partial = 0;
        NSDL2_LDAP(NULL, cptr, "handle=[%d] cptr->total_bytes=[%d], cptr->content_length=[%d]",handle, cptr->total_bytes, cptr->content_length);
        while(cptr->content_length < cptr->total_bytes)
        {
          if (!IS_HEADER_PARSED(cptr)) { 
 
          // If doesn't have enough data then continue.
          if (cptr->total_bytes - cptr->content_length < 2) 
          {
            is_partial = 1;
            break;
          }   

          // get from link list.
          frame_start_ptr = get_ptr_from_cptr_buf(cptr, cptr->content_length);
          // handle = cptr->buf_head->buffer[start_offset + 1]; 
          handle = frame_start_ptr[1];
          NSDL2_LDAP(NULL, cptr, "handle=[%d] ,start_offset =[%d], cptr->total_bytes=[%d], cptr->content_length=[%d]",handle, cptr->req_code_filled, cptr->total_bytes, cptr->content_length);
          // Check for the first bit, if it is set, that means length is in long format and this byte tells the number of bytes to be consumed 
          // for the length
          if(handle & 0x80){
            req_len = (handle & 0x7f);
            NSDL2_LDAP(NULL, cptr, "req_len=[%d]", req_len);
            if(cptr->content_length + 2 + req_len <= cptr->total_bytes){ // Length is in long format, we have got the length byte         
              //val = ldap_read_int(cptr->buf_head->buffer + 2, req_len); 
              val = ldap_read_int(frame_start_ptr + 2, req_len);
            } else { // length is in long format but partail
              is_partial = 1;
              break;        
            }    
          } else {
            val = handle;  // save length 
            // TODO: review
            req_len = 0;
          }     
          // It will store both. offset to last frame and flag if header is parsed. 
          SET_HEADER_PARSED(cptr);
          cptr->content_length = 2 + req_len + val + cptr->content_length;    
          //msg_id_len = frame_start_ptr[2 + req_len + 1];
          //NSDL2_LDAP(NULL, cptr, "After calc: handle=[%d] ,start_offset =[%d], cptr->total_bytes=[%d], cptr->content_length=[%d], opcode = %d",handle, cptr->req_code_filled, cptr->total_bytes, cptr->content_length, (frame_start_ptr[2 + req_len + 1 + 1 + msg_id_len] & 0x1f));
          } else {
            UNSET_HEADER_PARSED(cptr);
          }
        }
        if(is_partial)
          continue;
      } 
    } /*else {
      copy_retrieve_data(cptr, buf, bytes_read, cptr->total_bytes);
      cptr->total_bytes +=  bytes_read;
    } */

    // Check if it is search request then we have to process till done. 
    if (cptr->url_num->proto.ldap.operation == LDAP_SEARCH) {
      int last_frame_offset = LAST_FRAME_OFFSET(cptr); 
      frame_start_ptr = get_ptr_from_cptr_buf(cptr, last_frame_offset);
/*
 
      handle = frame_start_ptr[1];
      if(handle & 0x80){
            req_len = (handle & 0x7f);
            NSDL2_LDAP(NULL, cptr, "req_len=[%d]", req_len);
          } else {
            req_len = 0;
          }

      msg_id_len = frame_start_ptr[2 + req_len + 1];
*/
      int msg_opcode = -1;
      get_ldap_msg_opcode(frame_start_ptr, &msg_opcode);
      // NSDL2_LDAP(NULL, cptr, "operation = %d, frame_start_ptr = %p", (frame_start_ptr[2 + req_len + 1 + 1 + msg_id_len] & 0x1f), frame_start_ptr);
      NSDL2_LDAP(NULL, cptr, "frame_start_ptr = %p, opcode = %d", frame_start_ptr, msg_opcode);
      //if (!frame_start_ptr || ((frame_start_ptr[0] & 0x1f) != SEARCHRESULTDONE))
      if(msg_opcode != SEARCHRESULTDONE)
      {
        // TODO: put logs.
        continue;
      } 
    }
 
    // Complete response is read, break the loop of read
    if(cptr->total_bytes == cptr->content_length){
      break;
    } 
  }
   
  /* Here it will be ldap specific so we need to add new fields in avg_time */
  cptr->tcp_bytes_recv += cptr->content_length;
  ldap_avgtime->ldap_rx_bytes += cptr->content_length;
  ldap_avgtime->ldap_total_bytes += cptr->content_length;
  
  if(SHOW_GRP_DATA) { 
    LDAPAvgTime *local_ldap_avg = NULL;
    GET_LDAP_AVG(vptr, local_ldap_avg);
    local_ldap_avg->ldap_rx_bytes += cptr->content_length;
    local_ldap_avg->ldap_total_bytes += cptr->content_length; 
  }
  process_ldap_res(cptr); 

  // delete ldap timeout timers
  delete_ldap_timeout_timer(cptr);
  
  // This will call close connection
  handle_ldap_data_read_complete(cptr, now);
  cptr->content_length = 0;

  NSDL2_LDAP(NULL, cptr, "conn state=%d, proto_state = %d, now = %u", cptr->conn_state, cptr->proto_state, now);
  
  return 0;
}
