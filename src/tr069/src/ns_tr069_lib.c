#include "ns_tr069_includes.h"

char tr069_block[TR069_MAX_BLOCK_LEN + 1];

HashCodeToIndex tr069_hash_code_to_index;
StrToIndex      tr069_path_name_to_index;
HashCodeToStr   tr069_hash_code_to_path_name;
IndexToHashCode tr069_index_to_hash_code;
IndexToStr      tr069_index_to_path_name;

static char *tr069EventName[] = 
{
  "0 BOOTSTRAP", "1 BOOT", "2 PERIODIC", "3 SCHEDULED", "4 VALUE CHANGE", "5 KICKED",
  "6 CONNECTION REQUEST", "7 TRANSFER COMPLETE", "8 DIAGNOSTIC COMPLETE", "9 REQUEST DOWNLOAD",
  "10 AUTONOMOUS TRANSFER COMPLETE", 
  "M REBOOT", "M SCHEDULE INFORM", "M DOWNLOAD", "M UPLOAD"
//  "M VENDOR SPECIFIC METHOD", "X OUI EVENT"
}; 

inline char *tr069_get_event_name(int event_id)
{
  NSDL2_TR069(NULL, NULL, "Method Called, event_id = %d", event_id);
  return tr069EventName[event_id];
}

// This method will fill parameter used in the repsonse template
// This is for phase 1 only
// We will have all paramter valures from a file paramter using value in file option
inline void tr069_fill_response(char *forWhich, char *ParamName, char *ParamValue) {

   NSDL2_TR069(NULL, NULL, "Method Called, forWhich = [%s], ParamName = [%s], ParamValue = [%s]",
			    forWhich, ParamName, ParamValue);

   ns_save_string(ParamValue, ParamName);
}

inline void tr069_clear_vars() {

   NSDL2_TR069(NULL, NULL, "Method Called");

   //ns_save_string("", "TR069ReqHeadersDP", 0);
   ns_save_string("", "TR069RPCMethodSP");
   ns_save_string("", "TR069SOAPBodySP");
}

// TODO: IPv6 Not supported
void  tr069_switch_acs_to_acs_main_url(VUser *vptr, char *new_url) {

  int port, request_type;
  char hostname[1024], request_line[1024];

  NSDL2_TR069(vptr, NULL, "Method Called");

  if(new_url && !extract_hostname_and_request(new_url, hostname, request_line, &port, &request_type,
                                   NULL, vptr->first_page_url->request_type)) {
    if(port == -1)  {
      if (request_type == HTTPS_REQUEST) {// https
         port = 443;
      } else if(request_type == HTTP_REQUEST) { /* IPV4 */
          port = 80;
      } else {
        return;
      }
    }
    remap_all_urls_to_other_host(vptr, hostname, port);
  }
}

/* Function whcih close the fd only as close_connection is already done*/
static inline void tr069_close_fd(VUser *vptr, u_ns_ts_t now) {

  connection *cptr = vptr->last_cptr;
 
  NSDL2_SCHEDULE(vptr, NULL, "Method Called, "
                             "End of TR069 session Forcefully closing fd."
                             "last_cptr = [%p]", vptr->last_cptr);

  /*This can be done as follwoing"
   *  last_cptr = (connection *)vptr->first_cptr + global_settings->max_con_per_vuser;
   *  for (cptr = vptr->first_cptr; cptr < last_cptr; cptr++) 
   */  
  if(cptr) {
    if(cptr->conn_state != CNST_LISTENING) {
       close_fd(cptr, 1, now);
    } else {
      NSDL2_SCHEDULE(vptr, NULL, "Can not close fd as its in a CNST_LISTENING state");
    }
  }

  if(vptr->httpData->flags & NS_TR069_REMAP_SERVER) {
    char* url_ptr = tr069_is_cpe_new(vptr); 
    if(url_ptr) {
      tr069_switch_acs_to_acs_main_url(vptr, url_ptr);
    }
    vptr->httpData->flags &= ~NS_TR069_REMAP_SERVER;
  }
}
  
int tr069_get_data_value(char *data_buf, char **value, int *len, char *str_to_search_start, char *str_to_search_end){
  
  char *tmp_ptr2 = NULL; 
  char *ptr = NULL;

  NSDL2_TR069(NULL, NULL, "Method Called, data_buf = %s, str_to_search_start = %s"
                          ", str_to_search_end = %s",
                          data_buf, str_to_search_start, str_to_search_end);

  if(data_buf == NULL)
  {
    NSDL2_TR069(NULL, NULL, "Error: Buffer to be parsed is NULL");
    *value = NULL;
    *len = 0; 
    return TR069_ERROR;
  }

  if(str_to_search_start == NULL)
  {
    NSDL2_TR069(NULL, NULL, "Error: String to search is NULL.");
    *value = NULL;
    *len = 0; 
    return TR069_ERROR;
  }

  ptr = strstr(data_buf, str_to_search_start);
  if(ptr == NULL)
  { 
    NSDL2_TR069(NULL, NULL, "Error: String [%s] not found in [%s]", str_to_search_start, data_buf);
    *value = NULL;
    *len = 0; 
    return TR069_ERROR;
  }

  ptr += strlen(str_to_search_start);
  
  tmp_ptr2 = strstr(ptr, str_to_search_end);
  if(tmp_ptr2 == NULL)
  {
    NSDL2_TR069(NULL, NULL, "Error: String [%s] not found in [%s]", str_to_search_start, data_buf);
    *value = NULL;
    *len = 0; 
    return TR069_ERROR;
  }

  *len = tmp_ptr2 - ptr;
  
  *value = ptr;
  
  NSDL2_TR069(NULL, NULL, "Value = %*.*s, length = %d", *len, *len, *value, *len);
  return TR069_SUCCESS;
}

static int tr069_extract_id(char *data_buf, char **id, int *id_len)
{
  int ret = TR069_ERROR;
  char *tmp = NULL;
   
  NSDL2_TR069(NULL, NULL, "Method called, data_buf = [%s]", data_buf);
  
  ret = tr069_get_data_value(data_buf, &tmp, id_len, ":ID", ">");
    if(ret == TR069_ERROR)
    { 
      *id = NULL;
      *id_len = 0;   
      NSDL2_TR069(NULL, NULL, "ID not found");
      return -1;
    }
  //data_buf = id + *id_len;
  ret = tr069_get_data_value(tmp, id, id_len, ">", "</");
    if(ret == TR069_ERROR)
    {
      *id = NULL;
      *id_len = 0;
      NSDL2_TR069(NULL, NULL, "ID not found");
      return -1;
     }
  
  NSDL2_TR069(NULL, NULL, "id = %*.*s, id_len = %d", *id_len, *id_len, *id, *id_len);
  return 0;
}

inline int extract_and_set_param_key(VUser *vptr, char *cur_ptr) {

  char *param_key;
  int key_len;
  int ret = TR069_ERROR;

  NSDL2_TR069(vptr, NULL, "Method called, cur_ptr = [%s]", cur_ptr);

  ret = tr069_get_data_value(cur_ptr, &param_key, &key_len, "<ParameterKey>", "</ParameterKey>");

  if(ret == TR069_ERROR) {
    NSDL2_TR069(NULL, NULL, "ParameterKey not found, cur_ptr = [%s]", cur_ptr);
    return TR069_ERROR;
  }

  NSDL2_TR069(vptr, NULL, "parameter key = %*.*s, key_len = %d\n", key_len, key_len, param_key, key_len);

  tr069_set_param_values(vptr, "InternetGatewayDevice.ManagementServer.ParameterKey", -1, param_key, key_len);

  return 0;
}

inline int tr069_get_rpc_method(VUser *vptr, char *ptr, int ptr_len, int *req_type, int *bytes_eaten) {

  char *start, *end, *ptr1;
  //int left = 0;
  char *rpc_method = NULL;
  u_ns_ts_t now = get_ms_stamp();
 
  NSDL2_TR069(NULL, NULL, "Method called");
  NSDL4_TR069(NULL, NULL, "Body = %s", ptr);

  start = index(ptr, '<');
  if(!start) {
    fprintf(stderr, "Error in parsing body\n"); 
    *req_type = -1;
    return TR069_ERROR;
  }

  NSDL2_TR069(NULL, NULL, "%s: %d, start = %s\n", (char*)__FUNCTION__, __LINE__, start);
  start++;
  NSDL2_TR069(NULL, NULL, "%s: %d, start = %s\n", (char*)__FUNCTION__, __LINE__, start);

  end = index(start, '>');
  //printf("%s: %d\n", (char*)__FUNCTION__, __LINE__);
  if(!end) {
    fprintf(stderr, "Error in parsing body\n"); 
    *req_type = -1;
    return TR069_ERROR;
  }

  NSDL2_TR069(NULL, NULL, "%s: %d, end = %s\n", (char*)__FUNCTION__, __LINE__, end);
  int len = end - start;
  if(len < 0 || len > ptr_len) return TR069_ERROR;

  ptr = end + 1; // Till here it must contain RPC Method
  NSDL2_TR069(NULL, NULL, "ptr = %s", ptr); 
  ptr1 = index(start, ':'); 

  NSDL2_TR069(NULL, NULL, "start = %s", start);
 
  NSDL2_TR069(NULL, NULL, "start = %p, ptr1 = %p, end = %p\n", start, ptr1, end);

  if(ptr1 && (ptr1 < end)) {
    rpc_method = ptr1 + 1;
  } else {
    rpc_method = start;
  }

  CLEAR_WHITE_SPACE(rpc_method);

  NSDL2_TR069(NULL, NULL, "rpc_method = %s", rpc_method);

  if(!strncmp(rpc_method, "GetRPCMethods", GET_RPC_METHODS_LEN)) {
    *req_type = NS_TR069_GET_RPC_METHODS;
    *bytes_eaten = GET_RPC_METHODS_LEN;
    tr069_parse_get_rpc_method_req(vptr, rpc_method); 
  } else if (!strncmp(rpc_method, "SetParameterValues", SET_PARAMETER_VALUES_LEN)) {
    *req_type = NS_TR069_SET_PARAMETER_VALUES;
    *bytes_eaten = SET_PARAMETER_VALUES_LEN;
    rpc_method = rpc_method + *bytes_eaten;
    tr069_parse_spv_req(vptr, rpc_method); 
  } else if (!strncmp(rpc_method, "GetParameterValues", GET_PARAMETER_VALUES_LEN)) {
    *req_type = NS_TR069_GET_PARAMETER_VALUES;
    *bytes_eaten = GET_PARAMETER_VALUES_LEN;
    rpc_method = rpc_method + *bytes_eaten;
    tr069_parse_gpv_req(vptr, rpc_method); 
  } else if (!strncmp(rpc_method, "GetParameterNames", GET_PARAMETER_NAMES_LEN)) {
    *req_type = NS_TR069_GET_PARAMETER_NAMES;
    *bytes_eaten = GET_PARAMETER_NAMES_LEN;
    rpc_method = rpc_method + *bytes_eaten;
    tr069_parse_gpn_req(vptr, rpc_method);
  } else if (!strncmp(rpc_method, "SetParameterAttributes", SET_PARAMETER_ATTRIBUTES_LEN)) {
    *req_type = NS_TR069_SET_PARAMETER_ATTRIBUTES;
    *bytes_eaten = SET_PARAMETER_ATTRIBUTES_LEN;
    rpc_method = rpc_method + *bytes_eaten;
    tr069_parse_spa_req(vptr, rpc_method);
  } else if (!strncmp(rpc_method, "GetParameterAttributes", GET_PARAMETER_ATTRIBUTES_LEN)) {
    *req_type = NS_TR069_GET_PARAMETER_ATTRIBUTES;
    *bytes_eaten = GET_PARAMETER_ATTRIBUTES_LEN; 
    rpc_method = rpc_method + *bytes_eaten;
    tr069_parse_gpa_req(vptr, rpc_method);
  } else if (!strncmp(rpc_method, "AddObject", ADD_OBJECT_LEN)) {
    *req_type = NS_TR069_ADD_OBJECT;
    *bytes_eaten = ADD_OBJECT_LEN; 
    rpc_method = rpc_method + *bytes_eaten;
    tr069_parse_add_obj_req(vptr, rpc_method);
  } else if (!strncmp(rpc_method, "DeleteObject", DELETE_OBJECT_LEN)) {
    *req_type = NS_TR069_DELETE_OBJECT;
    *bytes_eaten = DELETE_OBJECT_LEN; 
    rpc_method = rpc_method + *bytes_eaten;
    tr069_parse_delete_obj_req(vptr, rpc_method);
  } else if (!strncmp(rpc_method, "Reboot", REBOOT_LEN)) {
    *req_type = NS_TR069_REBOOT;
    *bytes_eaten = REBOOT_LEN; 
    rpc_method = rpc_method + *bytes_eaten;
    tr069_parse_reboot_req(vptr, rpc_method);
  } else if (!strncmp(rpc_method, "Download", DOWNLOAD_LEN)) {
    *req_type = NS_TR069_DOWNLOAD;
    *bytes_eaten = DOWNLOAD_LEN; 
    rpc_method = rpc_method + *bytes_eaten;
    tr069_parse_download_req(vptr, rpc_method);
  } else if (rpc_method[0] == '\0') { // Empty post
    *req_type =  NS_TR069_END_OF_SESSION;  // TODO - Need to close connection
    *bytes_eaten = 0; 
    tr069_close_fd(vptr, now);
    return NS_TR069_END_OF_SESSION;
  } else {
    NSDL2_TR069(NULL, NULL, "Error: Invalid RPC method name [%s].", rpc_method);
    fprintf(stderr, "Error: Invalid RPC method name (%s).\n", rpc_method);
    *req_type =  NS_TR069_END_OF_SESSION; 
    *bytes_eaten = 0;
    tr069_close_fd(vptr, now); 
    return NS_TR069_END_OF_SESSION;
  }

  NSDL2_TR069(NULL, NULL, "req_type = %d, bytes_eaten = %d", *req_type, *bytes_eaten);
  return 0;
}

int tr069_extract_num_param_frm_req (char *data_buf) {
  char *str_to_search_start = "[";
  char *str_to_search_end = "]";
  char buf_lol[256];
  int len = 0;
  char *ptr;
  char *tmp_ptr2;
  int int_val = 0;

  NSDL2_TR069(NULL, NULL, "Method called");

  if(data_buf == NULL)
  {
    NSDL2_TR069(NULL, NULL, "Error: Buffer to be parsed is NULL");
    return TR069_ERROR;
  }

  if(str_to_search_start == NULL)
  {
    NSDL2_TR069(NULL, NULL, "Error: String to search is NULL.");
    return TR069_ERROR;
  }

  ptr = strstr(data_buf, str_to_search_start);
  if(ptr == NULL)
  { 
    NSDL2_TR069(NULL, NULL, "Error: String [%s] not found in [%s]", str_to_search_start, data_buf);
    return TR069_ERROR;
  }

  ptr += strlen(str_to_search_start);
  
  tmp_ptr2 = strstr(ptr, str_to_search_end);
  if(tmp_ptr2 == NULL)
  {
    NSDL2_TR069(NULL, NULL, "Error: String [%s] not found in [%s]", str_to_search_start, data_buf);
    return TR069_ERROR;
  }

  len = tmp_ptr2 - ptr;
  memset(buf_lol, 0, 256);
  strncpy(buf_lol, ptr, len);   

  /*if(!ns_is_numeric(buf_lol))
  {
    //NSDL2_TR069(NULL, NULL, "Value = %s is not integer", buf_lol);
    fprintf(stderr, "Value = %s is not integer", buf_lol);
    return TR069_ERROR;
  }*/

  int_val = atoi(buf_lol); 
  data_buf = tmp_ptr2;
  NSDL2_TR069(NULL, NULL, "int Value = %d ", int_val);
  return int_val;  // Success
}


/*
 * Check if page status is OK or not. If not, return -1.
 * Based on RPCMethod, return #define value
 */
inline int tr069_parse_req_and_get_rpc_method_name(VUser *vptr, int ret) {

   char *body;
   u_ns_ts_t now = get_ms_stamp();
   char *resp_cur_ptr;
   int bytes_eaten = 0;
   int body_len = vptr->bytes - vptr->body_offset;
   int tr069_req_type;
   char *tmp = NULL;
   char *id;
   char id_str[256];
   int id_len, ret_val;

   NSDL2_TR069(vptr, NULL, "Method Called, ret = %d, Body length = %d", ret, body_len);

   body = full_buffer;
  
   if(body[0] == '\0' || body_len == 0){
     NSDL2_TR069(vptr, NULL, "Got nothing in body. Closing connection.");
     tr069_close_fd(vptr, now);
     return NS_TR069_END_OF_SESSION;
   }

   ret_val = tr069_extract_id(body, &id, &id_len);
   if(ret_val != -1) {
     NSDL2_TR069(vptr, NULL, "ID = [%*.*s], id_len = [%d]", id_len, id_len, id, id_len);
     strncpy(id_str, id, id_len);
     id_str[id_len] = '\0';
     tr069_fill_response("ACSReqID", "TR069ACSReqIDSP", id_str);
   }

   //This will make buffer with soap body only
   tmp = strstr(body, "<soapenv:Body>");
   if(!tmp) {
     NSDL2_TR069(vptr, NULL, "<soapenv:Body> not found in body. Closing connection.");
     tr069_close_fd(vptr, now);
     return NS_TR069_END_OF_SESSION;
   }
   resp_cur_ptr = tmp + strlen("<soapenv:Body>");
   
   tr069_get_rpc_method(vptr, resp_cur_ptr, body_len, &tr069_req_type, &bytes_eaten);
   
   return tr069_req_type;
}

inline void tr069_vuser_data_init(VUser *vptr){

  char *url_ptr;
  NSDL2_TR069(vptr, NULL, "Method called");

  if(!(global_settings->protocol_enabled & TR069_PROTOCOL_ENABLED)) {
    NSDL2_TR069(vptr, NULL, "TR069 protocol is not enabled, returning.");
    return;
  }
   
  // In case same vptr is used by another user
  vptr->httpData->flags = 0; 
  FREE_AND_MAKE_NULL(vptr->httpData->reboot_cmd_key, "reboot_cmd_key buffer", 1);
  FREE_AND_MAKE_NULL(vptr->httpData->download_cmd_key, "download_cmd_key buffer", 1);

  //Set Boot and Bootstrap bits
  vptr->httpData->flags |= NS_TR069_EVENT_BOOT; // This must be always set

  // If CPE is starting first time only
  url_ptr = tr069_is_cpe_new(vptr);
  if(!url_ptr) {
    NSDL2_TR069(vptr, NULL, "User is starting for the first time with this data");
    vptr->httpData->flags |= NS_TR069_EVENT_BOOTSTRAP;
  } else {
    vptr->httpData->flags |= NS_TR069_REMAP_SERVER;
    tr069_switch_acs_to_acs_main_url(vptr, url_ptr);
    vptr->httpData->flags &= ~NS_TR069_REMAP_SERVER;
  }
}
