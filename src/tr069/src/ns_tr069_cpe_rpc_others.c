#include "ns_tr069_includes.h"
#include "ns_tr069_acs_rpc.h" 

inline void tr069_parse_reboot_req(VUser *vptr, char *req_ptr) {

  char *cmd_key = NULL;
  int key_len;
  int ret = TR069_ERROR;
  //int status = 0;
  char *tmp = NULL;
  
  NSDL2_TR069(NULL, NULL, "Method called");
  while(1)
  {
    key_len = 0;
    ret = tr069_get_data_value(req_ptr, &tmp, &key_len, "<CommandKey", ">");
    if(ret == TR069_ERROR)
      break;
    ret = tr069_get_data_value(tmp, &cmd_key, &key_len, ">", "</CommandKey>");
    if(ret == TR069_ERROR)
      break;
    tr069_set_reboot_cmd_key (vptr, cmd_key, key_len);
    req_ptr = cmd_key + key_len; 
    NSDL2_TR069(NULL, NULL, "req_ptr = %s", req_ptr);
    //status = tr069_reboot(vptr, value, value_len);
    //if(status){
      //NSDL2_TR069(NULL, NULL, "Value of commandkey is not found.");
      //break;
    //}
    if(key_len == 0)
     break;
  }
  NSDL2_TR069(NULL, NULL, "Method exiting");
}
int tr069_cpe_reboot_ex(VUser *vptr, int page_id) {

  int ret = -1;
  NSDL2_TR069(vptr, NULL, "Method Called");

  ret = ns_web_url_ext(vptr, page_id);

  
  // Check it should call or not
  ret = tr069_parse_req_and_get_rpc_method_name(vptr, ret);

  NSDL2_TR069(vptr, NULL, "tr069_parse_req_and_get_rpc_method_name returned = %d", ret);
  return ret;
}

/*
 Download has following flow
   CPE -> Inform with Periodic event
   ACS -> Inform Response
   CPE -> Empty Request
   ACS -> Download req from ACS with cmd_key and delay time
   CPE -> Download rep with Status 1
   ACS -> Empty Request
   <End of Session>
 
   <Now CPE will start dowloading of a file. Download delay is simulated using keyword value>

   CPE -> Inform with 7 Transfer complete and M Download (with cmd_key) event
   ACS -> Inform response
   CPE -> TransferComplete with cmd_key
   ACS -> TransferCompleteResponse
   CPE -> Empty Request
   ACS -> Empty Request
   <End of Session>

   <Now CPE will reboot itself. Reboot time is simulated using keyword value>

   CPE -> Inform with 1 Boot, 4 Value Change and M Download (with cmd_key) event
   ACS -> Inform response
   CPE -> Empty Request
   ACS -> Empty Request
   <End of Session>
*/


inline void tr069_parse_download_req(VUser *vptr, char *req_ptr) {

  char *cmdkey = NULL;
  char *file_type = NULL;
  char *url = NULL;
  char *user_name = NULL;
  char *password = NULL;
  char *file_size = NULL;
  char *target_file = NULL;
  char *delay_sec = NULL;
  char *success_url = NULL;
  char *failure_url = NULL;
  char *tmp = NULL; 
  int cmdkey_len, file_type_len, url_len, user_name_len, password_len, file_size_len, target_file_len, delay_sec_len, success_url_len, failure_url_len;

  int ret = TR069_ERROR;
  
  NSDL2_TR069(NULL, NULL, "Method called");
  while(1)
  {
    cmdkey_len = 0;
    ret = tr069_get_data_value(req_ptr, &tmp, &cmdkey_len, "<CommandKey", ">");
    if(ret == TR069_ERROR)
      break;
    ret = tr069_get_data_value(tmp, &cmdkey, &cmdkey_len, ">", "</CommandKey>");
    if(ret == TR069_ERROR)
      break;
    req_ptr = cmdkey + cmdkey_len;
    tr069_set_download_cmd_key (vptr, cmdkey, cmdkey_len);
    NSDL2_TR069(NULL, NULL, "req_ptr = %s", req_ptr);

    file_type_len = 0;
    ret = tr069_get_data_value(req_ptr, &tmp, &file_type_len, "<FileType", ">");
    if(ret == TR069_ERROR)
      break;
    ret = tr069_get_data_value(tmp, &file_type, &file_type_len, ">", "</FileType>");
    if(ret == TR069_ERROR)
      break;
    req_ptr = file_type + file_type_len; 
    NSDL2_TR069(NULL, NULL, "req_ptr = %s", req_ptr);

    url_len = 0;
    ret = tr069_get_data_value(req_ptr, &tmp, &url_len, "<URL", ">");
    if(ret == TR069_ERROR)
      break;
    ret = tr069_get_data_value(tmp, &url, &url_len, ">", "</URL>");
    if(ret == TR069_ERROR)
      break;
    req_ptr = url + url_len; 
    NSDL2_TR069(NULL, NULL, "req_ptr = %s", req_ptr);

    user_name_len = 0;
    ret = tr069_get_data_value(req_ptr, &tmp, &user_name_len, "<Username", ">");
    if(ret == TR069_ERROR)
      break;
    ret = tr069_get_data_value(tmp, &user_name, &user_name_len, ">", "</Username>");
    if(ret == TR069_ERROR)
      break; 
    req_ptr = user_name + user_name_len; 
    NSDL2_TR069(NULL, NULL, "req_ptr = %s", req_ptr);

    password_len = 0;
    ret = tr069_get_data_value(req_ptr, &tmp, &password_len, "<Password", ">");
    if(ret == TR069_ERROR)
      break;
    ret = tr069_get_data_value(tmp, &password, &password_len, ">", "</Password>");
    if(ret == TR069_ERROR)
      break;
    req_ptr = password + password_len; 
    NSDL2_TR069(NULL, NULL, "req_ptr = %s", req_ptr);

    file_size_len = 0;
    ret = tr069_get_data_value(req_ptr, &tmp, &file_size_len, "<FileSize", ">");
    if(ret == TR069_ERROR)
      break;
    ret = tr069_get_data_value(tmp, &file_size, &file_size_len, ">", "</FileSize>");
    if(ret == TR069_ERROR)
      break;
    req_ptr = file_size + file_size_len; 
    NSDL2_TR069(NULL, NULL, "req_ptr = %s", req_ptr);

    target_file_len = 0;
    ret = tr069_get_data_value(req_ptr, &tmp, &target_file_len, "<TargetFileName", ">");
    if(ret == TR069_ERROR)
      break;
    ret = tr069_get_data_value(tmp, &target_file, &target_file_len, ">", "</TargetFileName>");
    if(ret == TR069_ERROR)
      break;
    req_ptr = target_file + target_file_len; 
    NSDL2_TR069(NULL, NULL, "req_ptr = %s", req_ptr);

    delay_sec_len = 0;
    ret = tr069_get_data_value(req_ptr, &tmp, &delay_sec_len, "<DelaySeconds", ">");
    if(ret == TR069_ERROR)
      break;
    ret = tr069_get_data_value(tmp, &delay_sec, &delay_sec_len, ">", "</DelaySeconds>");
    if(ret == TR069_ERROR)
      break;
    req_ptr = delay_sec + delay_sec_len; 
    NSDL2_TR069(NULL, NULL, "req_ptr = %s", req_ptr);
    
    success_url_len = 0;
    ret = tr069_get_data_value(req_ptr, &tmp, &success_url_len, "<SuccessURL", ">");
    if(ret == TR069_ERROR)
      break;
    ret = tr069_get_data_value(tmp, &success_url, &success_url_len, ">", "</SuccessURL>");
    if(ret == TR069_ERROR)
      break;
    req_ptr = success_url + success_url_len;
    NSDL2_TR069(NULL, NULL, "req_ptr = %s", req_ptr);

    failure_url_len = 0;
    ret = tr069_get_data_value(req_ptr, &tmp, &failure_url_len, "<FailureURL", ">");
    if(ret == TR069_ERROR)
      break;
    ret = tr069_get_data_value(tmp, &failure_url, &failure_url_len, ">", "</FailureURL>");
    if(ret == TR069_ERROR)
      break;
    req_ptr = failure_url + failure_url_len;
    NSDL2_TR069(NULL, NULL, "req_ptr = %s", req_ptr);
  }
  NSDL2_TR069(NULL, NULL, "Method exiting");
}

int tr069_cpe_download_ex(VUser *vptr, int page_id) {

  int ret = -1;
  NSDL2_TR069(vptr, NULL, "Method Called");

  ret = ns_web_url_ext(vptr, page_id);

  // Check it should call or not
  ret = tr069_parse_req_and_get_rpc_method_name(vptr, ret);

  NSDL2_TR069(vptr, NULL, "tr069_parse_req_and_get_rpc_method_name returned = %d", ret);
  return ret;
}

int tr069_cpe_transfer_complete_ex(VUser *vptr, int page_id) {

  int ret = -1;
  NSDL2_TR069(vptr, NULL, "Method Called");

  ret = ns_web_url_ext(vptr, page_id);

  tr069_gen_req_id(tr069_block, "TR069TransferComplete");
  tr069_fill_response("TR069TransferComplete", "TR069CPEReqIDDP", tr069_block);

  // Check it should call or not
  //ret = tr069_parse_req_and_get_rpc_method_name(vptr, ret);

  NSDL2_TR069(vptr, NULL, "ns_web_url_ext returned = %d", ret);
  return ret;
}

int ns_tr069_get_periodic_inform_time_ext(VUser *vptr) {

  int is_inform_enable = 0;
  int time = -1;
  TR069ParamPath_t *param_path_table_ptr = &tr069_param_path_table[(vptr->user_index * total_param_path_entries)];
  NSDL2_TR069(vptr, NULL, "Method Called");

  if(IGDPeriodicInformEnableidx >= 0)
    is_inform_enable =  param_path_table_ptr[IGDPeriodicInformEnableidx].data.value->value_idx;

  if(is_inform_enable && IGDPeriodicInformIntervalidx >= 0) {
    time = param_path_table_ptr[IGDPeriodicInformIntervalidx].data.value->value_idx;
  }

  NSDL2_TR069(vptr, NULL, "is_inform_enable = %d, time = %d",
                           is_inform_enable, time);
  return time;
} 
