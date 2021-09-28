#include "ns_tr069_includes.h"

inline void tr069_parse_add_obj_req(VUser *vptr, char *req_ptr) {

  char *obj_name = NULL;
  char *param_key = NULL;
  char *tmp = NULL;
  int name_len, key_len;
  int ret = TR069_ERROR;
  
  NSDL2_TR069(vptr, NULL, "Method called");
  while(1) {
    name_len = 0;
    ret = tr069_get_data_value(req_ptr, &tmp, &name_len, "<ObjectName", ">");
    if(ret == TR069_ERROR)
      break;
    ret = tr069_get_data_value(tmp, &obj_name, &name_len, ">", "</ObjectName>");
    if(ret == TR069_ERROR)
      break;
    req_ptr = obj_name + name_len; 
    NSDL2_TR069(NULL, NULL, "req_ptr = %s", req_ptr);

    key_len = 0;
    ret =  tr069_get_data_value(req_ptr, &tmp, &key_len, "<ParamemeterKey", ">");
    if(ret == TR069_ERROR)
      break;
    ret =  tr069_get_data_value(tmp, &param_key, &key_len, ">", "</ParameterKey>");
    if(ret == TR069_ERROR)
      break;
    req_ptr =  param_key + key_len; 
    tr069_set_param_values(vptr, "InternetGatewayDevice.ManagementServer.ParameterKey", -1, param_key, key_len);
    NSDL2_TR069(NULL, NULL, "req_ptr = %s", req_ptr);

    if(key_len == 0)
     break;
  }
  NSDL2_TR069(NULL, NULL, "Method exiting");
}

int tr069_cpe_add_object_ex(VUser *vptr, int page_id) {

  int ret = -1;
  NSDL2_TR069(vptr, NULL, "Method Called");

  ret = ns_web_url_ext(vptr, page_id);
  ret = tr069_parse_req_and_get_rpc_method_name(vptr, ret);

  NSDL2_TR069(vptr, NULL, "tr069_parse_req_and_get_rpc_method_name returned = %d", ret);
  return ret;
}

void tr069_parse_delete_obj_req(VUser *vptr, char *req_ptr) {

  char *obj_name = NULL;
  char *param_key = NULL;
  char *tmp = NULL;
  int name_len, key_len;
  int ret = TR069_ERROR;
  
  NSDL2_TR069(NULL, NULL, "Method called");
  while(1)
  {
    name_len = 0;
    ret = tr069_get_data_value(req_ptr, &tmp, &name_len, "<ObjectName", ">");
    if(ret == TR069_ERROR)
      break;
    ret = tr069_get_data_value(tmp, &obj_name, &name_len, ">", "</ObjectName>");
    if(ret == TR069_ERROR)
      break;
    req_ptr = obj_name + name_len; 
    NSDL2_TR069(NULL, NULL, "req_ptr = %s", req_ptr);

    key_len = 0;
    ret = tr069_get_data_value(req_ptr, &tmp, &key_len, "<ParameterKey", ">");
    if(ret == TR069_ERROR)
      break;
    ret = tr069_get_data_value(tmp, &param_key, &key_len, ">", "</ParameterKey>");
    if(ret == TR069_ERROR)
      break;
    req_ptr = param_key + key_len; 
    tr069_set_param_values(vptr, "InternetGatewayDevice.ManagementServer.ParameterKey", -1, param_key, key_len);
    NSDL2_TR069(NULL, NULL, "req_ptr = %s", req_ptr);

    if(key_len == 0)
     break;
  }
  NSDL2_TR069(NULL, NULL, "Method exiting");
}

int tr069_cpe_delete_object_ex(VUser *vptr, int page_id) {

  int ret = -1;
  NSDL2_TR069(vptr, NULL, "Method Called");

  ret = ns_web_url_ext(vptr, page_id);
  ret = tr069_parse_req_and_get_rpc_method_name(vptr, ret);

  NSDL2_TR069(vptr, NULL, "tr069_parse_req_and_get_rpc_method_name returned = %d", ret);
  return ret;
}
