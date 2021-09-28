#ifndef ns_rbu_api_h__ 
#define ns_rbu_api_h__ 


#define RBU_MAX_KEYWORD_LENGTH              512
#define RBU_MAX_MODE_LENGTH                 512 
#define RBU_MAX_PROFILE_NAME_LENGTH         512 
#define RBU_MAX_HAR_FILE_LENGTH             512 
#define RBU_MAX_HAR_DIR_NAME_LENGTH         512 
#define RBU_MAX_PATH_LENGTH                 1024
#define RBU_MAX_USAGE_LENGTH                2 * 1024 
#define RBU_MAX_CMD_LENGTH                  10 * 1024
#define RBU_MAX_FIREFOX_CMD_LENGTH          1024
#define RBU_MAX_BUF_LENGTH                  1024
#define RBU_MAX_FILE_LENGTH                 1024
#define RBU_MAX_TRACE_FILE_LENGTH           1048
#define RBU_MAX_WAN_ARGS_LENGTH             512
#define RBU_MAX_WAN_ACCESS_LENGTH           512
#define RBU_USER_AGENT_LENGTH               1024
#define RBU_MAX_DEFAULT_PAGE_LENGTH         1024
#define RBU_MAX_NAME_LENGTH                 256 
#define RBU_MAX_VALUE_LENGTH                512 
#define RBU_MAX_TTI_FILE_LENGTH             4 * 1024
#define RBU_MAX_COLUMNS                     2
#define RBU_MAX_PAGE_NAME_LENGTH            512
#define RBU_MAX_URL_COUNT                   3500

#define RBU_MAX_URL_LENGTH                  5 * 1024
#define RBU_IOVECTOR_SIZE                   1024 
#define RBU_MAX_REQ_BODY_LENGTH             1024 * 1024
#define RBU_MAX_URL_REQ_DUMP_BUF_LENGTH     20 * 1024
#define RBU_MAX_256BYTE_LENGTH              256 
#define RBU_MAX_8BYTE_LENGTH                8 
#define RBU_MAX_16BYTE_LENGTH               16 
#define RBU_MAX_32BYTE_LENGTH               32 
#define RBU_MAX_64BYTE_LENGTH               64
#define RBU_MAX_128BYTE_LENGTH              128 
#define RBU_MAX_2K_LENGTH                   2048

#define RBU_MAX_ACC_LOG_LENGTH              1024
#define MAX_SCRIPT_EXECUTION_LOG_LENGTH     4096

#define RBU_HAR_FILE_NAME_SIZE              1024

#define RBU_HTTP_REQUEST                   1
#define RBU_HTTPS_REQUEST                  2
#define RBU_HTTP_METHOD_GET                1  
#define RBU_HTTP_METHOD_POST               2 
#define RBU_HTTP_REQUEST_STR               "http://"
#define RBU_HTTPS_REQUEST_STR              "https://"

#define RBU_FWRITE_EACH_ELEMENT_SIZE       1

#define RBU_SEL_MODE_ID                    0
#define RBU_SEL_MODE_XPATH                 1
#define RBU_SEL_MODE_DOMSTRING             2

#define DUMMY_PAGE                         1
#define NON_DUMMY_PAGE                     0

#define TCP_CLIENT_CON_TIMEOUT             10
#define ELEMENT_ID_MODE 0
#define ELEMENT_XPATH_MODE 1

//For mimeType Size Calculation
#define MIMETYPE_JS                        0
#define MIMETYPE_CSS                       1
#define MIMETYPE_IMG                       2
#define MIMETYPE_HTML                      3
#define MIMETYPE_OTHER                     4

#define PG_STATUS_1xx                      1
#define PG_STATUS_2xx                      2
#define PG_STATUS_3xx                      3
#define PG_STATUS_4xx                      4
#define PG_STATUS_5xx                      5

#define RBU_MAX_DOMAIN_NAME_LENGTH         256

#define RBU_SET_WRITE_PTR(write_ptr, write_idx, free_space) \
{ \
  NSDL3_API(vptr, vptr->last_cptr, "write_ptr = %p, write_idx = %d, free_space = %d", write_ptr, write_idx, free_space); \
  write_ptr += write_idx; \
  free_space -= write_idx; \
}

#define CREATE_RBU_RESP_ATTR \
{ \
  if(vptr->httpData->rbu_resp_attr == NULL) \
  { \
    NSDL2_API(vptr, NULL, "Allocating RBU_RESP_ATTR, vptr->httpData->rbu_resp_attr = %p", vptr->httpData->rbu_resp_attr); \
    MY_MALLOC(vptr->httpData->rbu_resp_attr, sizeof(RBU_RespAttr),  "vptr->httpData->rbu_resp_attr", -1); \
    memset(vptr->httpData->rbu_resp_attr, 0, sizeof(RBU_RespAttr)); \
    NSDL2_API(vptr, NULL, "After allocation of RBU_RESP_ATTR, vptr->httpData->rbu_resp_attr = %p", vptr->httpData->rbu_resp_attr); \
  } \
}

#define CREATE_RBU_HARTIME \
{\
  if(rbu_resp_attr->rbu_hartime == NULL) \
  {\
    NSDL2_API(vptr, NULL, "Allocating RBU_HARTIME, rbu_har_time = %p", rbu_resp_attr->rbu_hartime); \
    MY_MALLOC(rbu_resp_attr->rbu_hartime, sizeof(RBU_HARTime) * RBU_MAX_URL_COUNT,  "rbu_resp_attr->rbu_hartime", -1); \
    memset(rbu_resp_attr->rbu_hartime, 0, sizeof(RBU_HARTime) * RBU_MAX_URL_COUNT); \
    NSDL2_API(vptr, NULL, "After allocation of RBU_HARTime, rbu_hartime = %p", rbu_resp_attr->rbu_hartime); \
  } \
}

#define CREATE_RBU_DOMAIN \
{\
  if(rbu_domains == NULL) \
  {\
    NSDL2_API(vptr, NULL, "Before allocating rbu_domain_info_t, rbu_domain_info = %p", rbu_domains); \
    MY_MALLOC_AND_MEMSET(rbu_domains, sizeof(Rbu_domain_info) * max_rbu_domain_entries, "rbu_domain_info", -1); \
    NSDL2_API(vptr, NULL, "After allocation of rbu_domain_info_t, rbu_domain_info = %p, dname_big_buf.buffer = %p", rbu_domain_info, dname_big_buf.buffer); \
  }\
}

#define CREATE_RBU_LIGHTHOUSE_STRUCT \
{\
  if(rbu_resp_attr->rbu_light_house == NULL) \
  {\
    NSDL2_API(vptr, NULL, "Allocating RBU_LightHouse, rbu_light_house = %p", rbu_resp_attr->rbu_light_house); \
    MY_MALLOC(rbu_resp_attr->rbu_light_house, sizeof(RBU_LightHouse),  "rbu_resp_attr->rbu_light_house", -1); \
    memset(rbu_resp_attr->rbu_light_house, 0, sizeof(RBU_LightHouse)); \
    MY_MALLOC(rbu_resp_attr->rbu_light_house->lighthouse_filename, RBU_MAX_NAME_LENGTH + 6, \
                       "rbu_resp_attr->rbu_light_house->lighthouse_filename", -1); \
    NSDL2_API(vptr, NULL, "After allocation of RBU_LightHouse, rbu_light_house = %p", rbu_resp_attr->rbu_light_house); \
  } \
}

// Note: these memory must be free at the end of session , clean it at user cleanup
#define MALLOC_RBU_RESP_ATTR_MEMBERS \
{ \
  if(vptr->httpData->rbu_resp_attr != NULL) \
  { \
    NSDL2_API(vptr, NULL, "Allocating har_log_path, vptr->httpData->rbu_resp_attr->har_log_path = %p", vptr->httpData->rbu_resp_attr->har_log_path); \
    MY_MALLOC(vptr->httpData->rbu_resp_attr->har_log_path, RBU_MAX_PATH_LENGTH + 1,  "vptr->httpData->rbu_resp_attr->har_log_path", -1); \
    memset(vptr->httpData->rbu_resp_attr->har_log_path, 0, RBU_MAX_PATH_LENGTH + 1); \
    NSDL2_API(vptr, NULL, "After allocation of har_log_path, vptr->httpData->rbu_resp_attr->har_log_path = %p", vptr->httpData->rbu_resp_attr->har_log_path); \
 \
    NSDL2_API(vptr, NULL, "Allocating har_log_dir, vptr->httpData->rbu_resp_attr->har_log_dir = %p", vptr->httpData->rbu_resp_attr->har_log_dir); \
    MY_MALLOC(vptr->httpData->rbu_resp_attr->har_log_dir, RBU_MAX_HAR_DIR_NAME_LENGTH + 1,  "vptr->httpData->rbu_resp_attr->har_log_dir", -1); \
    memset(vptr->httpData->rbu_resp_attr->har_log_dir, 0, RBU_MAX_HAR_DIR_NAME_LENGTH + 1); \
    NSDL2_API(vptr, NULL, "After allocation of har_log_dir, vptr->httpData->rbu_resp_attr->har_log_dir = %p", vptr->httpData->rbu_resp_attr->har_log_dir); \
 \
    NSDL2_API(vptr, NULL, "Allocating profile, vptr->httpData->rbu_resp_attr->profile = %p", vptr->httpData->rbu_resp_attr->profile); \
    MY_MALLOC(vptr->httpData->rbu_resp_attr->profile, RBU_MAX_PROFILE_NAME_LENGTH + 1,  "vptr->httpData->rbu_resp_attr->profile", -1); \
    memset(vptr->httpData->rbu_resp_attr->profile, 0, RBU_MAX_PROFILE_NAME_LENGTH + 1); \
    NSDL2_API(vptr, NULL, "After allocation of profile, vptr->httpData->rbu_resp_attr->profile = %p", vptr->httpData->rbu_resp_attr->profile); \
 \
    NSDL2_API(vptr, NULL, "Allocating POST req file name, vptr->httpData->rbu_resp_attr->post_req_filename = %p", vptr->httpData->rbu_resp_attr->post_req_filename); \
    MY_MALLOC(vptr->httpData->rbu_resp_attr->post_req_filename, RBU_MAX_FILE_LENGTH + 1, "vptr->httpData->rbu_resp_attr->post_req_filename", -1); \
    memset(vptr->httpData->rbu_resp_attr->post_req_filename, 0, RBU_MAX_FILE_LENGTH + 1); \
    NSDL2_API(vptr, NULL, "After allocation of POST req file name, vptr->httpData->rbu_resp_attr->post_req_filename = %p", vptr->httpData->rbu_resp_attr->post_req_filename); \
 \
    NSDL2_API(vptr, NULL, "Allocating firefox_cmd_buf, vptr->httpData->rbu_resp_attr->firefox_cmd_buf = %p", vptr->httpData->rbu_resp_attr->firefox_cmd_buf); \
    MY_MALLOC(vptr->httpData->rbu_resp_attr->firefox_cmd_buf, RBU_MAX_CMD_LENGTH + 1,  "vptr->httpData->rbu_resp_attr->firefox_cmd_buf", -1); \
    memset(vptr->httpData->rbu_resp_attr->firefox_cmd_buf, 0, RBU_MAX_CMD_LENGTH + 1); \
    NSDL2_API(vptr, NULL, "After allocation of firefox_cmd_buf, vptr->httpData->rbu_resp_attr->firefox_cmd_buf = %p", vptr->httpData->rbu_resp_attr->firefox_cmd_buf); \
 \
    NSDL2_API(vptr, NULL, "Allocating url, vptr->httpData->rbu_resp_attr->url = %p", vptr->httpData->rbu_resp_attr->url); \
    MY_MALLOC(vptr->httpData->rbu_resp_attr->url, RBU_MAX_URL_LENGTH + 1,  "vptr->httpData->rbu_resp_attr->url", -1); \
    memset(vptr->httpData->rbu_resp_attr->url, 0, RBU_MAX_URL_LENGTH + 1); \
    NSDL2_API(vptr, NULL, "After allocation of url, vptr->httpData->rbu_resp_attr->url = %p", vptr->httpData->rbu_resp_attr->url); \
 \
    NSDL2_API(vptr, NULL, "Allocating date_time_str, vptr->httpData->rbu_resp_attr->date_time_str = %p", vptr->httpData->rbu_resp_attr->date_time_str); \
    MY_MALLOC(vptr->httpData->rbu_resp_attr->date_time_str, RBU_MAX_64BYTE_LENGTH + 1,  "vptr->httpData->rbu_resp_attr->date_time_str", -1); \
    memset(vptr->httpData->rbu_resp_attr->date_time_str, 0, RBU_MAX_64BYTE_LENGTH + 1); \
    NSDL2_API(vptr, NULL, "After allocation of url, vptr->httpData->rbu_resp_attr->date_time_str = %p", vptr->httpData->rbu_resp_attr->date_time_str); \
    NSDL2_API(vptr, NULL, "Allocating brwsr_vrsn, vptr->httpData->rbu_resp_attr->brwsr_vrsn = %p", \
                           vptr->httpData->rbu_resp_attr->brwsr_vrsn); \
    MY_MALLOC(vptr->httpData->rbu_resp_attr->brwsr_vrsn, RBU_MAX_8BYTE_LENGTH + 1,\
                                      "vptr->httpData->rbu_resp_attr->brwsr_vrsn", -1); \
    memset(vptr->httpData->rbu_resp_attr->brwsr_vrsn, 0, RBU_MAX_8BYTE_LENGTH + 1); \
    NSDL2_API(vptr, NULL, "After allocation of brwsr_vrsn, vptr->httpData->rbu_resp_attr->brwsr_vrsn = %p" \
                                             , vptr->httpData->rbu_resp_attr->brwsr_vrsn); \
    NSDL2_API(vptr, NULL, "Allocating req_method, vptr->httpData->rbu_resp_attr->req_method = %p", \
                           vptr->httpData->rbu_resp_attr->req_method); \
    MY_MALLOC(vptr->httpData->rbu_resp_attr->req_method, RBU_MAX_8BYTE_LENGTH + 1,\
                                      "vptr->httpData->rbu_resp_attr->req_method", -1); \
    memset(vptr->httpData->rbu_resp_attr->req_method, 0, RBU_MAX_8BYTE_LENGTH + 1); \
    NSDL2_API(vptr, NULL, "After allocation of req_method, vptr->httpData->rbu_resp_attr->req_method = %p" \
                                             , vptr->httpData->rbu_resp_attr->req_method); \
    NSDL2_API(vptr, NULL, "Allocating server_ip_add, vptr->httpData->rbu_resp_attr->server_ip_add= %p", \
                           vptr->httpData->rbu_resp_attr->server_ip_add); \
    MY_MALLOC(vptr->httpData->rbu_resp_attr->server_ip_add, RBU_MAX_32BYTE_LENGTH + 1,\
                                      "vptr->httpData->rbu_resp_attr->server_ip_add", -1); \
    memset(vptr->httpData->rbu_resp_attr->server_ip_add, 0, RBU_MAX_32BYTE_LENGTH + 1); \
    NSDL2_API(vptr, NULL, "After allocation of server_ip_add, vptr->httpData->rbu_resp_attr->server_ip_add= %p" \
                                             , vptr->httpData->rbu_resp_attr->server_ip_add); \
    NSDL2_API(vptr, NULL, "Allocating access_log_msg, vptr->httpData->rbu_resp_attr->access_log_msg= %p", \
                           vptr->httpData->rbu_resp_attr->access_log_msg); \
    MY_MALLOC(vptr->httpData->rbu_resp_attr->access_log_msg, RBU_MAX_ACC_LOG_LENGTH + 1,\
                                      "vptr->httpData->rbu_resp_attr->access_log_msg", -1); \
    memset(vptr->httpData->rbu_resp_attr->access_log_msg, 0, RBU_MAX_ACC_LOG_LENGTH + 1); \
    NSDL2_API(vptr, NULL, "After allocation of access_log_msg, vptr->httpData->rbu_resp_attr->access_log_msg= %p" \
                                             , vptr->httpData->rbu_resp_attr->access_log_msg); \
    NSDL2_API(vptr, NULL, "Allocating date, vptr->httpData->rbu_resp_attr->date= %p", \
                           vptr->httpData->rbu_resp_attr->date); \
    MY_MALLOC(vptr->httpData->rbu_resp_attr->date, RBU_MAX_32BYTE_LENGTH + 1,\
                                      "vptr->httpData->rbu_resp_attr->date", -1); \
    memset(vptr->httpData->rbu_resp_attr->date, 0, RBU_MAX_32BYTE_LENGTH + 1); \
    NSDL2_API(vptr, NULL, "After allocation of date, vptr->httpData->rbu_resp_attr->date= %p" \
                                             , vptr->httpData->rbu_resp_attr->date); \
    NSDL2_API(vptr, NULL, "Allocating time, vptr->httpData->rbu_resp_attr->time= %p", \
                           vptr->httpData->rbu_resp_attr->time); \
    MY_MALLOC(vptr->httpData->rbu_resp_attr->time, RBU_MAX_32BYTE_LENGTH + 1,\
                                      "vptr->httpData->rbu_resp_attr->time", -1); \
    memset(vptr->httpData->rbu_resp_attr->time, 0, RBU_MAX_32BYTE_LENGTH + 1); \
    NSDL2_API(vptr, NULL, "After allocation of time, vptr->httpData->rbu_resp_attr->time= %p" \
                                             , vptr->httpData->rbu_resp_attr->time); \
    NSDL2_API(vptr, NULL, "Allocating inline_url, vptr->httpData->rbu_resp_attr->inline_url= %p", \
                           vptr->httpData->rbu_resp_attr->inline_url); \
    MY_MALLOC(vptr->httpData->rbu_resp_attr->inline_url, RBU_MAX_128BYTE_LENGTH + 1,\
                                      "vptr->httpData->rbu_resp_attr->inline_url", -1); \
    memset(vptr->httpData->rbu_resp_attr->inline_url, 0, RBU_MAX_128BYTE_LENGTH + 1); \
    NSDL2_API(vptr, NULL, "After allocation of inline_url, vptr->httpData->rbu_resp_attr->inline_url= %p" \
                                             , vptr->httpData->rbu_resp_attr->inline_url); \
    NSDL2_API(vptr, NULL, "Allocating status_text, vptr->httpData->rbu_resp_attr->status_text= %p", \
                           vptr->httpData->rbu_resp_attr->status_text); \
    MY_MALLOC(vptr->httpData->rbu_resp_attr->status_text, RBU_MAX_64BYTE_LENGTH + 1,\
                                      "vptr->httpData->rbu_resp_attr->status_text", -1); \
    memset(vptr->httpData->rbu_resp_attr->status_text, 0, RBU_MAX_64BYTE_LENGTH + 1); \
    NSDL2_API(vptr, NULL, "After allocation of status_text, vptr->httpData->rbu_resp_attr->status_text= %p" \
                                             , vptr->httpData->rbu_resp_attr->status_text); \
    NSDL2_API(vptr, NULL, "Allocating user_agent_str, vptr->httpData->rbu_resp_attr->user_agent_str= %p", \
                           vptr->httpData->rbu_resp_attr->user_agent_str); \
    MY_MALLOC(vptr->httpData->rbu_resp_attr->user_agent_str, RBU_USER_AGENT_LENGTH + 1,\
                                      "vptr->httpData->rbu_resp_attr->user_agent_str", -1); \
    memset(vptr->httpData->rbu_resp_attr->user_agent_str, 0, RBU_USER_AGENT_LENGTH + 1); \
    NSDL2_API(vptr, NULL, "After allocation of user_agent_str, vptr->httpData->rbu_resp_attr->user_agent_str= %p" \
                                             , vptr->httpData->rbu_resp_attr->user_agent_str); \
    NSDL2_API(vptr, NULL, "Allocating param_string, vptr->httpData->rbu_resp_attr->param_string= %p", \
                           vptr->httpData->rbu_resp_attr->param_string); \
    MY_MALLOC(vptr->httpData->rbu_resp_attr->param_string, RBU_MAX_128BYTE_LENGTH + 1,\
                                      "vptr->httpData->rbu_resp_attr->param_string", -1); \
    memset(vptr->httpData->rbu_resp_attr->param_string, 0, RBU_MAX_128BYTE_LENGTH + 1); \
    NSDL2_API(vptr, NULL, "After allocation of param_string, vptr->httpData->rbu_resp_attr->param_string= %p" \
                                             , vptr->httpData->rbu_resp_attr->param_string); \
    NSDL2_API(vptr, NULL, "Allocating http_vrsn, vptr->httpData->rbu_resp_attr->http_vrsn = %p", \
                           vptr->httpData->rbu_resp_attr->http_vrsn); \
    MY_MALLOC(vptr->httpData->rbu_resp_attr->http_vrsn, RBU_MAX_16BYTE_LENGTH + 1,\
                                      "vptr->httpData->rbu_resp_attr->http_vrsn", -1); \
    memset(vptr->httpData->rbu_resp_attr->http_vrsn, 0, RBU_MAX_16BYTE_LENGTH + 1); \
    NSDL2_API(vptr, NULL, "After allocation of http_vrsn, vptr->httpData->rbu_resp_attr->http_vrsn = %p" \
                                             , vptr->httpData->rbu_resp_attr->http_vrsn); \
    NSDL2_API(vptr, NULL, "Allocating har_name, vptr->httpData->rbu_resp_attr->har_name = %p", \
                           vptr->httpData->rbu_resp_attr->har_name); \
    MY_MALLOC(vptr->httpData->rbu_resp_attr->har_name, RBU_HAR_FILE_NAME_SIZE + 1,\
                                      "vptr->httpData->rbu_resp_attr->har_name", -1); \
    memset(vptr->httpData->rbu_resp_attr->har_name, 0, RBU_HAR_FILE_NAME_SIZE + 1); \
    NSDL2_API(vptr, NULL, "After allocation of har_name, vptr->httpData->rbu_resp_attr->har_name = %p" \
                                             , vptr->httpData->rbu_resp_attr->har_name); \
    NSDL2_API(vptr, NULL, "Allocating dvc_info, vptr->httpData->rbu_resp_attr->dvc_info = %p", \
                           vptr->httpData->rbu_resp_attr->dvc_info); \
    MY_MALLOC(vptr->httpData->rbu_resp_attr->dvc_info, RBU_HAR_FILE_NAME_SIZE + 1,\
                                      "vptr->httpData->rbu_resp_attr->dvc_info", -1); \
    memset(vptr->httpData->rbu_resp_attr->dvc_info, 0, RBU_HAR_FILE_NAME_SIZE + 1); \
    NSDL2_API(vptr, NULL, "After allocation of dvc_info, vptr->httpData->rbu_resp_attr->dvc_info = %p", \
                                             vptr->httpData->rbu_resp_attr->dvc_info); \
  } \
}

#define ADD_USER_AGENT_IN_FIREFOX_CMD \
{ \
  CLEAR_WHITE_SLASH_R_SLASH_N_END(user_agent_str); \
  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "--cav_hdr \"User-Agent: %s\" ", user_agent_str); \
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len); \
}

#define ADD_USER_AGENT_IN_CHROME_MSG \
{ \
  CLEAR_WHITE_SLASH_R_SLASH_N_END(user_agent_str); \
  ADD_HEADERS_IN_CHROME_MSG("User-Agent", user_agent_str); \
}

#define ADD_HEADERS_IN_CHROME_MSG(hname, hvalue) \
{ \
  if(header_count)/*add seperator(,)*/  \
  {	  \
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, ", ");   \
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len); \
  }       \
  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "{\"name\": \"%s\", \"value\": \"%s\"}", hname, hvalue); \
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len); \
  header_count += 1;\
}

#define ADD_COOKIES_IN_CHROME_MSG(cname, cvalue, cdomain) \
{ \
  if(cookie_count) /*add separator(,)*/ \
  { \
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, ", ");   \
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len); \
  }       \
  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "{\"name\": \"%s\", \"value\": \"%s\" , \"domain\": \"%s\"}", cname?cname:"NULL", cvalue?cvalue:"NULL", cdomain?cdomain:"NULL"); \
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len); \
  cookie_count +=1; \
} 

#define REMOVE_COOKIES_IN_CHROME_MSG(cname, cdomain) \
{ \
  if(cookie_count) /*add separator(,)*/ \
  { \
    cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, ", ");   \
    RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len); \
  }       \
  cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "{\"name\": \"%s\", \"domain\": \"%s\"}", cname?cname:"NULL", cdomain?cdomain:"NULL"); \
  RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len); \
  cookie_count +=1; \
} \

#define ADD_COOKIES_PAIR_IN_CHROME_MSG(buf, delimeter)\
{  \
  char *cookies_fields[128]; \
  char *sep_cookie_fields[3]; \
  int i, num_cookies =0, tot_fields = 0; \
  \
  num_cookies = get_tokens_ex2(buf, cookies_fields, ";", 128); \
  NSDL2_RBU(NULL, NULL, "num_cookies = %d", num_cookies); \
  \
  if(num_cookies == 0) \
  { \
    NSDL2_RBU(NULL, NULL, "num_cookies = %d", num_cookies); \
    tot_fields = get_tokens_ex2(buf, sep_cookie_fields, delimeter, 3); \
    if (tot_fields < 2)\
       NSDL2_RBU(NULL,NULL,"Atleast two fields are mandatory in cookie");\
    else if(tot_fields == 2) \
    { \
      BRU_CLEAR_WHITE_SPACE(sep_cookie_fields[0]); \
      BRU_CLEAR_WHITE_SPACE(sep_cookie_fields[1]); \
      ADD_COOKIES_IN_CHROME_MSG(sep_cookie_fields[0], sep_cookie_fields[1], NULL);\
    }else if (tot_fields == 3) { \
      BRU_CLEAR_WHITE_SPACE(sep_cookie_fields[0]); \
      BRU_CLEAR_WHITE_SPACE(sep_cookie_fields[1]);\
      BRU_CLEAR_WHITE_SPACE(sep_cookie_fields[2]); \
      ADD_COOKIES_IN_CHROME_MSG(sep_cookie_fields[0], sep_cookie_fields[1], sep_cookie_fields[2]); \
    }else {\
       NSDL2_RBU(NULL,NULL,"More then three fields are not supported yet in cookie");\
    }\
  } \
  else \
  { \
    for(i = 0; i < num_cookies; i++) \
    { \
       NSDL2_RBU(NULL, NULL, "cookies_fields[%d] = [%s]", i, cookies_fields[i]); \
       tot_fields = get_tokens_ex2(cookies_fields[i], sep_cookie_fields, delimeter, 3); \
       if (tot_fields < 2)\
        NSDL2_RBU(NULL,NULL,"Atleast two fields are mandatory in cookie");\
       else if(tot_fields == 2) \
       { \
         BRU_CLEAR_WHITE_SPACE(sep_cookie_fields[0]); \
         BRU_CLEAR_WHITE_SPACE(sep_cookie_fields[1]); \
         ADD_COOKIES_IN_CHROME_MSG(sep_cookie_fields[0], sep_cookie_fields[1], NULL);\
       }else if (tot_fields == 3) { \
         BRU_CLEAR_WHITE_SPACE(sep_cookie_fields[0]); \
         BRU_CLEAR_WHITE_SPACE(sep_cookie_fields[1]);\
         BRU_CLEAR_WHITE_SPACE(sep_cookie_fields[2]); \
         ADD_COOKIES_IN_CHROME_MSG(sep_cookie_fields[0], sep_cookie_fields[1], sep_cookie_fields[2]); \
       }else {\
          NSDL2_RBU(NULL,NULL,"Atleast two fields are mandatory in cookie");\
       }\
     } \
  } \
}

#define RETURN(ret) \
{ \
  if(json) nslb_json_free(json); \
  if(req_fp) fclose(req_fp); \
  if(rep_fp) fclose(rep_fp); \
  if(rep_body_fp) fclose(rep_body_fp); \
  return ret; \
}

#define OPEN_ELEMENT(json) \
{ \
  int ret = 0; \
  if(json->c_element_type == NSLB_JSON_OBJECT) \
    ret = nslb_json_open_obj(json); \
  else if(json->c_element_type == NSLB_JSON_ARRAY) \
    ret = nslb_json_open_array(json); \
  if(ret != 0) { \
    fprintf(stderr, "Error: ns_rbu_process_har_file() - failed to open json element, error = %s, line = %d\n", nslb_json_strerror(json), __LINE__); \
    RETURN(-1);	\
  } \
}

#define CLOSE_ELEMENT_OBJ(json)	\
{ \
  if(nslb_json_close_obj(json) != 0){ \
    fprintf(stderr, "Error: ns_rbu_process_har_file() - failed to close json element object, error = %s, line = %d\n", nslb_json_strerror(json), __LINE__); \
    RETURN(-1);	 \
  } \
}

#define CLOSE_ELEMENT_ARR(json)	\
{ \
  if(nslb_json_close_array(json) != 0){ \
    fprintf(stderr, "Error: ns_rbu_process_har_file() - failed to close json element array, error = %s, line = %d\n", nslb_json_strerror(json), __LINE__); \
    RETURN(-1);	\
  } \
}

#define GOTO_ELEMENT(json, ele_name) \
{ \
  if(nslb_json_goto_element(json, ele_name) != 0) { \
    fprintf(stderr, "Error: ns_rbu_process_har_file() - failed to move to element \'%s\', error = %s, line = %d", ele_name, nslb_json_strerror(json), __LINE__); \
    RETURN(-1);	\
  } \
}

//This macro will not return if error.
#define GOTO_ELEMENT2(json_ele_name) nslb_json_goto_element(json, ele_name)

//This will free val if not null
#define GET_ELEMENT_VALUE(json, val, len_ptr, malloc_flag, decode_flag)	\
{ \
  val = nslb_json_element_value(json, len_ptr, malloc_flag, decode_flag); \
	if(!ptr) { \
	  fprintf(stderr, "Error: ns_rbu_process_har_file() - failed to get value of element, error = %s, line = %d\n", nslb_json_strerror(json), __LINE__); \
    RETURN(-1);	\
  } \
}

#define DUMP_TO_LOG_FILE(fp, ...) if(fp) fprintf(fp,  __VA_ARGS__);

#ifdef CAV_MAIN
#define RBU_NS_LOGS_PATH \
  char ns_logs_file_path[256 + 1]; \
  char rbu_logs_file_path[256 + 1]; \
  sprintf(ns_logs_file_path, "TR%d/%s/ns_logs", testidx, vptr->sess_ptr->sess_name); \
  sprintf(rbu_logs_file_path, "TR%d/%s/rbu_logs", testidx, vptr->sess_ptr->sess_name);
#else
#define RBU_NS_LOGS_PATH \
  char ns_logs_file_path[256 + 1]; \
  char rbu_logs_file_path[256 + 1]; \
  if(vptr->partition_idx <= 0) { \
    sprintf(ns_logs_file_path, "TR%d/ns_logs", testidx); \
    sprintf(rbu_logs_file_path, "TR%d/rbu_logs", testidx); \
  } \
  else { \
    sprintf(ns_logs_file_path, "TR%d/%lld/ns_logs", testidx, vptr->partition_idx); \
    sprintf(rbu_logs_file_path, "TR%d/%lld/rbu_logs", testidx, vptr->partition_idx); \
  }
#endif

#define ADD_USER_IDENTITY_IN_HAR(har_file) \
    if(*har_file != '\0') { \
      char har_file_buf[1024+1]; \
      char *ch; \
      strcpy(har_file_buf, har_file); \
      ch=strrchr(har_file_buf,'.'); \
      int z=ch-har_file_buf; \
      har_file_buf[z]='\0'; \
      sprintf(har_file, "%s+%hd_%u_%u_%d_0_%d_%d_%d_0.har", har_file_buf, child_idx, vptr->user_index, vptr->sess_inst,                                                 vptr->page_instance, vptr->group_num, GET_SESS_ID_BY_NAME(vptr), GET_PAGE_ID_BY_NAME(vptr));\
    }

#define CLEAR_WHITE_SLASH_R_SLASH_N_END(ptr) { int end_len = strlen(ptr); \
                                          while((ptr[end_len - 1] == '\n') || ptr[end_len - 1] == '\r') { \
                                            ptr[end_len - 1] = '\0';\
                                            end_len = strlen(ptr);\
                                          }\
                                        }

#define BRU_CLEAR_WHITE_SPACE(ptr) {while ((*ptr == ' ') || (*ptr == '\t')) ptr++;}


//Here interval is in millisecond
#define VUSER_SLEEP(vptr, interval) \
{\
  if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT) \
  { \
    vptr->flags |= NS_USER_PTT_AS_SLEEP; \
    ns_rbu_page_think_time_as_sleep(vptr, interval);\
    vptr->flags &= ~NS_USER_PTT_AS_SLEEP; \
  } \
  else \
    usleep(1000 * interval); \
}

#define ADD_ND_FPI_HEADER_IN_CHROME_MSG() \
{\
  if(header_count) /*add seperator(,)*/ \
    {\
      cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, ", "); \
      RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len); \
    } \
    if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_nd_fpi_mode == 1)\
    { \
        cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "{\"name\": \"CavNDFPInstance\", \"value\":\"f\"}");\
        RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);\
    }\
    else if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.rbu_nd_fpi_mode == 2)\
    {\
        cmd_write_idx = snprintf(cmd_buf_ptr, free_cmd_buf_len, "{\"name\": \"CavNDFPInstance\", \"value\":\"F\"}");\
        RBU_SET_WRITE_PTR(cmd_buf_ptr, cmd_write_idx, free_cmd_buf_len);\
    }\
    header_count += 1;\
}\

#define RBU_SET_PAGE_STATUS_ON_ERR(vptr, rbu_resp_attr, err_type, ptr) \
{ \
  rbu_resp_attr->is_incomplete_har_file = 1; \
  if(err_type == NS_REQUEST_CV_FAILURE) \
    vptr->sess_status = NS_REQUEST_ERRMISC; \
  else \
    vptr->sess_status = err_type; \
  rbu_resp_attr->cv_status = err_type; \
  vptr->page_status = err_type; \
  vptr->last_cptr->req_ok = err_type; \
  rbu_resp_attr->url_date_time = (time(NULL)  * 1000); \
  rbu_fill_page_status(vptr, rbu_resp_attr); \
  if (err_type == NS_REQUEST_TIMEOUT) {\
    strncpy(rbu_resp_attr->access_log_msg, "TimeOut Occurs", 512); \
  } \
  if(ptr->continue_error_value == 0) {  \
    NSDL2_RBU(vptr, NULL, "G_CONTINUE_ON_PAGE_ERR is 0, hence next_pg_id is setting to NS_NEXT_PG_STOP_SESSION"); \
    vptr->next_pg_id = NS_NEXT_PG_STOP_SESSION; \
    vptr->pg_think_time = 0; \
  } \
} \

//Reset all those values which are changed on every sessions
#define RESET_RBU_RESP_ATTR_TIME(a) \
{ \
  NSDL3_API(vptr, vptr->last_cptr, "RESET_RBU_RESP_ATTR_TIME called"); \
  a->is_incomplete_har_file = 0; \
  a->on_content_load_time = -1; \
  a->on_load_time = -1; \
  a->page_load_time = -1; \
  a->_tti_time = -1; \
  a->_cav_start_render_time = -1; \
  a->_cav_end_render_time = -1; \
  a->first_paint = -1; \
  a->first_contentful_paint = -1; \
  a->largest_contentful_paint = -1; \
  a->total_blocking_time = -1; \
  a->cum_layout_shift = -1; \
  a->request_without_cache = -1; \
  a->request_from_cache = -1; \
  a->byte_rcvd = -1; \
  a->byte_send = -1; \
  a->byte_rcvd_bfr_DOM = -1; \
  a->byte_rcvd_bfr_OnLoad = -1; \
  a->status_code = -1; \
  a->resp_body_size = -1; \
  a->req_body_size = -1;  \
  if(a->resp_body != NULL) \
    memset(a->resp_body, 0, a->last_malloced_size); \
  a->pg_wgt = -1; \
  a->resp_js_size = -1; \
  a->resp_css_size = -1; \
  a->resp_img_size = -1; \
  a->dom_element = -1; \
  a->pg_speed = -1; \
  a->akamai_cache = -1; \
  a->main_url_resp_time = -1; \
  a->pg_avail = 1; \
  a->sess_completed = 0; \
  a->sess_success = 0; \
  a->resp_html_size = -1; \
  a->resp_other_size = -1; \
  a->resp_js_count = -1; \
  a->resp_css_count = -1; \
  a->resp_img_count = -1; \
  a->resp_html_count = -1; \
  a->resp_other_count = -1; \
  a->main_url_start_date_time = 0; \
  a->req_frm_browser_cache_bfr_DOM = -1; \
  a->req_frm_browser_cache_bfr_OnLoad = -1; \
  a->req_frm_browser_cache_bfr_Start_render = -1; \
  a->req_bfr_DOM = -1; \
  a->req_bfr_OnLoad = -1; \
  a->req_bfr_Start_render = -1; \
  a->rbu_hartime = NULL;\
  a->pg_status = 0 ; \
  if(a->access_log_msg) strcpy(a->access_log_msg, ""); \
  if(a->req_method) strcpy(a->req_method, ""); \
  if(a->server_ip_add) strcpy(a->server_ip_add, ""); \
  if(a->date) strcpy(a->date, ""); \
  if(a->time) strcpy(a->time, ""); \
  if(a->inline_url) strcpy(a->inline_url, ""); \
  if(a->status_text) strcpy(a->status_text, ""); \
  a->all_url_resp_time = -1; \
  if(a->user_agent_str) strcpy(a->user_agent_str, ""); \
  a->url_date_time = -1; \
  if(a->param_string) strcpy(a->param_string, ""); \
  a->dns_time = 0; \
  a->tcp_time = 0; \
  a->ssl_time = 0; \
  a->har_date_and_time = 0; \
  a->speed_index = -1; \
  a->connect_time = 0; \
  a->wait_time = 0; \
  a->rcv_time = 0; \
  a->blckd_time = 0; \
  a->url_resp_time = 0; \
  a->total_blocking_time = 0;\
  a->largest_contentful_paint = 0;\
  a->cum_layout_shift = 0;\  
  a->cv_status = 0 ; \
  if(a->date_time_str) strcpy(a->date_time_str, ""); \
  if(a->har_name) strcpy(a->har_name, ""); \
  if(a->dvc_info) strcpy(a->dvc_info, ""); \
  a->executed_js_result = 0; \
  a->mark_and_measures = NULL; \
  a->total_mark_and_measures = 0 ; \
}

#define RESET_RBU_DOMAIN_STRUCTS \
{\
 for(int i = 0; i< total_rbu_domain_entries; i++ ) \
 { \
   rbu_domains[i].dns_time = 0; \
   rbu_domains[i].tcp_time = 0; \
   rbu_domains[i].ssl_time = 0; \
   rbu_domains[i].connect_time = 0; \
   rbu_domains[i].wait_time = 0; \
   rbu_domains[i].rcv_time = 0; \
   rbu_domains[i].blckd_time = 0; \
   rbu_domains[i].url_resp_time = 0; \
   rbu_domains[i].num_request = 0; \
 } \
 total_rbu_domain_entries = 0;\
}

typedef struct RBU_HARTime
{
  unsigned long started_date_time;  //will store started date time of each request 
  unsigned long end_date_time;      //will store end end time of each request 
} RBU_HARTime; 

typedef struct RBU_LightHouse
{
  char *lighthouse_filename;
  u_ns_ts_t lh_file_date_time;
  int performance_score;  
  int pwa_score;  
  int accessibility_score;  
  int best_practice_score;  
  int seo_score;  
  int first_contentful_paint_time;  
  int first_meaningful_paint_time;  
  int speed_index;  
  int first_CPU_idle;  
  int time_to_interact;  
  int input_latency;
  int largest_contentful_paint;
  int total_blocking_time;
  float cum_layout_shift;  
} RBU_LightHouse;

#define RBU_MARK_TYPE 0 
#define RBU_MEASURE_TYPE 1

typedef struct RBU_MarkMeasure {
  char name[128];
  char type; 
  int startTime;
  int duration;
}RBU_MarkMeasure;


/* This structure will store two types of values -
    1) Data getting from Har file Eg: status_code, resp_body etc. 
    2) Data which will not changed throughout the session Eg: profile, har_log_path etc. 
    3) Buffer to reduce stact size in User Context (i.e. memory optimization) Eg: url, firefox_cmd_buf 
Pointer 'body' will be malloc'd, therefore, it is to be free'd by client
*/
typedef struct RBU_RespAttr 
{
  int status_code;
  int resp_body_size;
  int req_body_size;
  size_t last_malloced_size;
  char *resp_body;
  char *page_dump_buf;
  size_t last_page_dump_buf_malloced;
  int page_dump_buf_len;
  char screen_shot_file_flag;         /* 0 - Not present/error in reading, 1 - File is read in resp_body and size is in resp_body_size */
  char *url;
  char *firefox_cmd_buf;
  char *page_screen_shot_file;
  char *page_screen_shot_name;
  char *page_capture_clip_file;       /* capture clips of requested page */
  char *date_time_str;                    /*Date and time for csv */
  int on_content_load_time;
  int on_load_time;
  int page_load_time;		      /* This must be int & default value is -1, Resolved Bug : 17357 */
  int _cav_start_render_time;
  int _cav_end_render_time;
  int _tti_time;
  int first_paint;
  int first_contentful_paint;
  int largest_contentful_paint;
  int total_blocking_time;
  float cum_layout_shift;
  int request_from_cache;
  int request_without_cache;
  float byte_send;
  float byte_rcvd;
  float byte_rcvd_bfr_DOM;              //Byte received before DOM event fired
  float byte_rcvd_bfr_OnLoad;           //Byte received before OnLoad event fired
  int is_incomplete_har_file;           /* 0: when HAR made complete*/
                                        /* 1: when HAR is incomplete or not made successfully*/
  /* START: Following members will not change thorughout the session */
  char *profile;                         /* Store browser profile name */ 
  char *har_log_path;                    /* Store har log path Eg: /home/cavisson/.rbu/.mozilla/firefox/logs */
  char *har_log_dir;                     /* Store har log directory name Eg: nsDProf*/
  int vnc_display;
  int sess_start_flag;                   /* This flag will tell whether user is in running session or not? 
                                            0 - Default means session is not running or start of the start of the session 
                                            1 - means session is running */
  char *post_req_filename; /*store POST req file name*/
  int browser_pid;
  int first_pg;  //This flag will tell whether we have to send clean cache to true or not?

  int resp_js_count;           //Number of Java Script Present in HAR
  float resp_js_size;            //Size of Java Script Present in HAR
  int resp_css_count;          //Number of CSS Present in HAR
  float resp_css_size;           //Size of CSS Present in HAR
  int resp_img_count;          //Number of Image Present in HAR
  float resp_img_size;           //Size of Image Present in HAR
  int dom_element;               //Total Number of DOM elements 
  int pg_speed;                  //Page score
  float akamai_cache;            //Number of Request from Akamai Cache
  float main_url_resp_time;        //Main URL response time calculation (dns+connect+wait+receive)
  float pg_wgt;                  //Total weight of a page (resp_js_size + resp_css_size + resp_img_size)
  int pg_avail;                  // Page Availability - it can be 0 or 1 only
  int sess_completed;            // Keep tracke of total session completed in which these pages have participate
  int sess_success;              // Keep tracke of total success session in which these pages have been participate
  char pg_status;                  // Page Status will depend on main url status code.
  char *brwsr_vrsn;
 
  unsigned long long main_url_start_date_time;   //Main URL Start Date Time in ms.
  int req_frm_browser_cache_bfr_DOM;        //Request from browser cache before DOM event fired
  int req_frm_browser_cache_bfr_OnLoad;     //Request from browser cache before OnLoad event fired
  int req_frm_browser_cache_bfr_Start_render;     //Request from browser cache before Start Render
  int req_bfr_DOM;                          //Request before DOM event fired
  int req_bfr_OnLoad;                       //Request before OnLoad event fired
  int req_bfr_Start_render;                       //Request before Start Render
  int resp_html_count;                  //Number of HTML Present in HAR
  float resp_html_size;                 //Size of HTML Present in HAR
  int resp_other_count;                  //Number of other mimeType present(HTML, JS, CSS, Image) in HAR
  float resp_other_size;                 //Size of other mimeType present(HTML, JS, CSS, Image) in HAR
  char cav_nv_val[128];                 //Value of CavNV cookie
  RBU_HARTime *rbu_hartime;             //hartime structure to calculate pageload  

  char *req_method;                     //Store Request method value
  char *server_ip_add;                  //Store serverIpAddress of URl
  char *access_log_msg;                 //Store Message for access log
  char *date;
  char *time;
  char *inline_url;                     //Store Inline Url of Page
  char *status_text;                    //Store Status text of each URL
  float all_url_resp_time;              //for all url response time
  char *user_agent_str;                 //Stores User-Agent from HAR to access log

  u_ns_ts_t url_date_time;              //will store started date time of each request 
  char *param_string;                   //Stores query parameter

  float in_url_pg_wgt;                  //Response Size of URLs(main+inline)

  int num_domelement;                   //Stores number of domelement coming from extension

  float dns_time;                       //will store DNS time
  float tcp_time;                       //will store TCP time

  float ssl_time;                       //will store SSL time
  float connect_time;                   //will store connect time
  float wait_time;                      //will store wait time
  float rcv_time;                       //will store receive time
  float blckd_time;                     //will store blocked time
  float url_resp_time;                  //stores overall URL elapse time
  char *http_vrsn;                      //stores HTTPVersion of response                        

  char cv_status;                       //Check Point Status
  char *har_name;
  u_ns_ts_t har_date_and_time;
  int speed_index;  

  char *dvc_info;                       //Stores Device Information  example
                                        // JTS - 'Mobile:Android:6.0:Samsung Galaxy'
                                        // C -'Desktop:Chrome:54'
  char *performance_trace_filename;
  char performance_trace_flag;
  int performance_trace_timeout;
  
  char netTest_CavNV_cookie_val[128];       //Value of CavNV cookie for netTest
  char netTest_CavNVC_cookie_val[128];      //Value of CavNVC cookie for netTest
  int executed_js_result;                   //To store output obtained by executing js

  RBU_LightHouse *rbu_light_house;

  unsigned int page_norm_id;                         //Store page_norm_id in case of Java-type script

  RBU_MarkMeasure *mark_and_measures;
  short total_mark_and_measures;

  timer_type *timer_ptr;
  int har_file_prev_size;
  int retry_count;
  char prev_har_file_name[RBU_MAX_HAR_FILE_LENGTH];

  /* END */
} RBU_RespAttr;

extern int g_rbu_lighthouse_csv_fd;

extern void handler_sigchild_ignore(int data);
extern int ns_rbu_req_stat(int url_time, int rbu_attr, int *req_count);
extern int get_har_name_and_date();
extern int ns_get_speed_index(char *path_snap_shot , struct dirent **snap_shot_dirent , int num_snap_shots, int *visual_complete_tm, double *speed_index_val);
extern int rbu_fill_page_status();
extern void ns_rbu_log_access_log();
extern int ns_rbu_get_date_time();
extern void ns_rbu_check_acc_log_dump();
extern void ns_rbu_url_parameter_fetch();
extern SSL_CTX* InitCTX(void);
extern inline void fill_selection_attr();
int ns_rbu_handle_domain_discovery();
extern int ns_get_clip_time_index(char *clip);
#endif /* ns_rbu_api_h__ */
