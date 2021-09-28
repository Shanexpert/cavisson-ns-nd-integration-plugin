#include "ns_tr069_includes.h"

#include "ns_tr069_cpe_rpc_param.h"

/************************** GET RPC Methods Start**************************/
static inline void 
tr069_gen_get_rpc_methods_block_CB(int param_len, char *param_start, 
                                   char **cur_ptr, int *num_params) {

  //int buf_len = 0;
  int len; 
  char *ptr;
  char indent_new_line[100] = "";

  NSDL2_TR069(NULL, NULL, "Method Called");

  if(*num_params > 0)   // For nth time
       sprintf(indent_new_line, "\n        ");
 
  ptr = *cur_ptr;
  len = strlen(indent_new_line);
  memcpy(ptr, indent_new_line, len);
  ptr += len;
  
  memcpy(ptr, "<string>", sizeof("<string>"));
  ptr += (sizeof("<string>")) -1;
  
  memcpy(ptr, param_start, param_len);
  ptr += param_len;
  
  memcpy(ptr, "</string>", sizeof("</string>"));
  ptr += (sizeof("</string>")) -1;

  *ptr = '\0';

  (*num_params)++;

  NSDL2_TR069(NULL, NULL, "num_params = %d, get_parameters_names_block = [%s]",
                           *num_params, *cur_ptr);
  //*cur_ptr += buf_len;
  *cur_ptr = ptr;
}

static inline void 
tr069_gen_get_rpc_methods_block(TR069GetRPCMethodsblockCB callback_fn,
                                char **cur_ptr, int *num_params) {

  int len;
  char *ptr;
  char new_line[] = "\n";
  char *parameter_structs = ns_eval_string("{TR069RPCSupportedFP}");
  char *param_start = parameter_structs;
  
  NSDL2_TR069(NULL, NULL, "Method Called, parameter_structs = [%s]", parameter_structs);
  if(!param_start) param_start= new_line;

  while((ptr = index(param_start, '\n'))) {
    len = ptr - param_start;
    NSDL2_TR069(NULL, NULL, "len = [%d]", len);
    callback_fn(len, param_start, cur_ptr, num_params);
    param_start = ptr + 1;
  }
}

// Create response for Get RPC Method request
inline void tr069_parse_get_rpc_method_req(VUser *vptr, char *rpc_method) {

  char count_str[16];
  char *cur_ptr = tr069_block;
  int num_params = 0;

  NSDL2_TR069(vptr, NULL, "Method Called");

  // Currently this is is coming from file parameter
  tr069_gen_get_rpc_methods_block(tr069_gen_get_rpc_methods_block_CB, &cur_ptr, &num_params);

  if(num_params <= 0) {
    NSDL2_TR069(vptr, NULL, "num_params ( %d )  must be > 0.", num_params);
    return; 
  }

  sprintf(count_str, "%d", num_params);
  tr069_fill_response("TR069GetRPCMethods", "TR069NumRPCMethodsDP", count_str);
  tr069_fill_response("TR069GetRPCMethods", "TR069GetRPCMethodsResponseDP", tr069_block);

  NSDL2_TR069(NULL, NULL, "Method exiting");
}

int tr069_cpe_execute_get_rpc_methods_ex(VUser *vptr, int page_id) {

  int ret = -1;

  NSDL2_TR069(vptr, NULL, "Method Called");

  ret = ns_web_url_ext(vptr, page_id);
  ret = tr069_parse_req_and_get_rpc_method_name(vptr, ret);

  NSDL2_TR069(vptr, NULL, "tr069_parse_req_and_get_rpc_method_name returned = %d", ret);
  return ret;
}

/************************** GET RPC Methods End**************************/

/************************** Set Parameter Values Start**************************/
void tr069_parse_spv_req(VUser *vptr, char *req_ptr) {

  char *cur_ptr = tr069_block;
  char *name, *value;
  char *tmp = NULL;
  int name_len, value_len;
  int ret = TR069_ERROR;
  int array_size;
  int status = 0;
   
  NSDL2_TR069(NULL, NULL, "Method called, req_ptr = [%s]", req_ptr);
  
  array_size = tr069_extract_num_param_frm_req(req_ptr);

  if(global_settings->tr069_options & NS_TR069_OPTIONS_TREE_DATA) {
    NSDL2_TR069(NULL, NULL, "Making request from run time response data.");
    while(array_size) {
      name_len = value_len = 0;
      ret = tr069_get_data_value(req_ptr, &tmp, &name_len, "<Name", ">");
      NSDL2_TR069(NULL, NULL, "string extracted = %*.*s, len = %d", name_len, name_len, tmp, name_len);
      //NSDL2_TR069(NULL, NULL, "Value = %*.*s, length = %d", *len, *len, *value, *len);
      if(ret == TR069_ERROR)
        break;
      ret = tr069_get_data_value(tmp, &name, &name_len, ">", "</Name>");
       if(ret == TR069_ERROR)
        break;
 
      //call_back_func(value, value_len);
      req_ptr = name + name_len; 
      NSDL2_TR069(NULL, NULL, "req_ptr = %s", req_ptr);

      //value_len = 0;
      ret = tr069_get_data_value(req_ptr, &tmp, &value_len, "<Value", ">");
      if(ret == TR069_ERROR)
        break;
      ret = tr069_get_data_value(tmp, &value, &value_len, ">", "</Value>");
      if(ret == TR069_ERROR)
        break;
      req_ptr = value + value_len; 
      NSDL2_TR069(NULL, NULL, "req_ptr = %s", req_ptr);

      status = tr069_set_param_values(vptr, name, name_len, value, value_len);
      if(status) {
        NSDL2_TR069(NULL, NULL, "Can not set the value of parameter  = [%*.*s]", name_len, name_len, name);
        break;
      }
      array_size--;
      //if(value_len == 0)
      // break;
    } 
  } else {
    status = 0;
  }

  if(global_settings->tr069_options & NS_TR069_OPTIONS_TREE_DATA) {
    extract_and_set_param_key(vptr, req_ptr);
  }

  sprintf(cur_ptr, "%d", status?1:0);
  tr069_fill_response("TR069SetParameterValues", "TR069SPVStatusDP", cur_ptr);

  NSDL2_TR069(NULL, NULL, "Method exiting");
}

int tr069_cpe_execute_set_parameter_values_ex(VUser *vptr, int page_id) {

  int ret = -1;

  NSDL2_TR069(vptr, NULL, "Method Called");

  ret = ns_web_url_ext(vptr, page_id);
  ret = tr069_parse_req_and_get_rpc_method_name(vptr, ret);

  NSDL2_TR069(vptr, NULL, "tr069_parse_req_and_get_rpc_method_name returned = %d", ret);
  return ret;
}

/************************** Set Parameter Values End**************************/

/************************** Get Parameter Values Start**************************/
static inline void tr069_gen_get_parameters_values_block_CB(char *name, char *type, char *value,
                                                     char **cur_ptr, int *num_params) {
  char indent_new_line[16] = "";
  char *ptr;
  int len;

  NSDL2_TR069(NULL, NULL, "Method Called, name = [%s], type = [%s], value = [%s]", name, type, value);

  if(*num_params > 0)   // For nth time
    sprintf(indent_new_line, "\n        ");

  // For 0th time
  ptr = *cur_ptr;
  len = strlen(indent_new_line);
  memcpy(ptr, indent_new_line, len);
  ptr += len;

  memcpy(ptr, "<ParameterValueStruct xsi:type=\"cwmp:ParameterValueStruct\">\n          <Name xsi:type=\"xsd:string\">",
      sizeof("<ParameterValueStruct xsi:type=\"cwmp:ParameterValueStruct\">\n          <Name xsi:type=\"xsd:string\">"));
  ptr += (sizeof("<ParameterValueStruct xsi:type=\"cwmp:ParameterValueStruct\">\n          <Name xsi:type=\"xsd:string\">")) -1;

  len = strlen(name);
  memcpy(ptr, name, len);
  ptr += len;
  
  memcpy(ptr, "</Name>\n          <Value xsi:type=\"xsd:>", sizeof("</Name>\n          <Value xsi:type=\"xsd:>"));
  ptr +=(sizeof("</Name>\n          <Value xsi:type=\"xsd:")) -1;
  

  len = strlen(type);
  memcpy(ptr, type, len);
  ptr += len;
 

  memcpy(ptr, "\">", sizeof("\">"));
  ptr += sizeof(("\">")) -1;
  len = strlen(value);
  memcpy(ptr, value, len);
  ptr += len;

   
  memcpy(ptr, "</Value>\n        </ParameterValueStruct>", sizeof("</Value>\n        </ParameterValueStruct>"));
  ptr += (sizeof("</Value>\n        </ParameterValueStruct>")) -1;

  *ptr = '\0';
   
  (*num_params)++;
  NSDL2_TR069(NULL, NULL, "num_params = %d, parameter_struct_block = [%s]", *num_params, *cur_ptr);
  *cur_ptr = ptr;
}

static inline void tr069_gen_get_parameters_values_block(TR069GPVblockCB callback_fn,
                                                 char **cur_ptr, int *num_params) {
  int len;
  char *ptr;
  char * parameter_structs = ns_eval_string("{TR069GPVFP}");
  char tmp_buf[TR069_NAME_LEN + TR069_TYPE_LEN + TR069_VALUE_LEN + 1];
  char *fields[10];
  char new_line[] = "\n";
  char name[TR069_NAME_LEN + 1];
  char type[TR069_TYPE_LEN + 1];
  char value[TR069_VALUE_LEN + 1];

  NSDL2_TR069(NULL, NULL, "Method Called, parameter_structs = [%s]", parameter_structs);
  
  char *param_start = parameter_structs; 
  if(!param_start) param_start = new_line;

  while((ptr = index(param_start, '\n'))) {
    len = ptr - param_start;
    strncpy(tmp_buf, param_start, len);
    tmp_buf[len] = '\0';

    get_tokens(tmp_buf, fields, ",", 3);
    if(fields[0]) {
      strncpy(name, fields[0], TR069_NAME_LEN);
    } else {
      strncpy(name, "NotAvailable", TR069_NAME_LEN);
    }
    name[TR069_NAME_LEN] = '\0';

    if(fields[1]) {
      strncpy(type, fields[1], TR069_TYPE_LEN);
    } else {
      strncpy(type, "NotAvailable", TR069_TYPE_LEN);
    }
    type[TR069_TYPE_LEN] = '\0';

    if(fields[2]) {
      strncpy(value, fields[2], TR069_VALUE_LEN);
    } else {
      strncpy(value, "NotAvailable", TR069_VALUE_LEN);
    }
    value[TR069_VALUE_LEN] = '\0';
    NSDL2_TR069(NULL, NULL, "tmp_buf = [%s], name = [%s], type = [%s], value = [%s]", tmp_buf, name, type, value);
    callback_fn(name, type, value, cur_ptr, num_params);
    param_start = ptr + 1;
  }
}

void tr069_parse_gpv_req(VUser *vptr, char *req_ptr){

  char count_str[16];
  int num_params = 0;
  char *cur_ptr = tr069_block;
  char *value = NULL;
  int value_len;
  int ret = TR069_ERROR;
  int array_size = 0;
  int status = 0; 

  tr069_block[0] = '\0';
  
  NSDL2_TR069(NULL, NULL, "Method called");

  if(global_settings->tr069_options & NS_TR069_OPTIONS_TREE_DATA) {
    array_size = tr069_extract_num_param_frm_req(req_ptr);
    NSDL2_TR069(NULL, NULL, "Making request from run time response data, array_size = %d", array_size);
    while(array_size) {
      value_len = 0;
      ret = tr069_get_data_value(req_ptr, &value, &value_len, "<string", ">");
      if(ret == TR069_ERROR)
        break;
      req_ptr = value + value_len; 
      ret = tr069_get_data_value(req_ptr, &value, &value_len, ">", "</");
      if(ret == TR069_ERROR)
        break;
      req_ptr = value + value_len; 
      array_size--;
      NSDL2_TR069(NULL, NULL, "req_ptr = %s, Remainning values = %d", req_ptr, array_size);
      status = tr069_get_param_values_with_cb(vptr, value, value_len, 
                                              tr069_gen_get_parameters_values_block_CB, &cur_ptr,
                                              &num_params);
      if(status) {
        NSDL2_TR069(NULL, NULL, "Can not get the value = [%*.*s]", value_len, value_len, value);
        break;
      }
    }
  } else {
    NSDL2_TR069(NULL, NULL, "Making request from static data file.");
    tr069_gen_get_parameters_values_block(tr069_gen_get_parameters_values_block_CB, &cur_ptr, &num_params);
  }

  if(num_params <= 0) {
    NSDL2_TR069(vptr, NULL, "num_params ( %d )  must be > 0.", num_params);
    return; 
  }

  sprintf(count_str, "%d", num_params);
  tr069_fill_response("TR069GetParameterValueResponse", "TR060NumGPVDP", count_str);
  tr069_fill_response("TR069GetParameterValueResponse", "TR069GPVStructDP", tr069_block);

  NSDL2_TR069(NULL, NULL, "Method exiting");
}


int tr069_cpe_execute_get_parameter_values_ex(VUser *vptr, int page_id) {

  int ret = -1;

  NSDL2_TR069(vptr, NULL, "Method Called");

  ret = ns_web_url_ext(vptr, page_id);
  ret = tr069_parse_req_and_get_rpc_method_name(vptr, ret);

  NSDL2_TR069(vptr, NULL, "tr069_parse_req_and_get_rpc_method_name returned = %d", ret);
  return ret;
}

/************************** Get Parameter Values End**************************/

/************************** Get Parameter Names Start**************************/
static inline void
tr069_gen_get_parameters_names_block_CB(char *name, char *writable,
                                        char **cur_ptr, int *num_params) {
  char indent_new_line[16] = "";
  char *ptr;
  int len;
    
  NSDL2_TR069(NULL, NULL, "Method called. name = [%s], writable = [%s], num_params = [%d]", 
                           name, writable, *num_params);
    
  if(*num_params > 0) 
    sprintf(indent_new_line, "\n        "); // indent and New line is to be added for all events except the first one
     
  ptr = *cur_ptr;
  len = strlen(indent_new_line);
  memcpy(ptr, indent_new_line, len);
  ptr += len;


  memcpy(ptr, "<ParameterInfoStruct xsi:type=\"cwmp:ParameterInfoStruct\">\n          <Name xsi:type=\"xsd:string\">",
       sizeof("<ParameterInfoStruct xsi:type=\"cwmp:ParameterInfoStruct\">\n          <Name xsi:type=\"xsd:string\">"));
  ptr += (sizeof("<ParameterInfoStruct xsi:type=\"cwmp:ParameterInfoStruct\">\n          <Name xsi:type=\"xsd:string\">" )) -1;
  len = strlen(name);
  memcpy(ptr, name, len);
  ptr += len;

  memcpy(ptr, "</Name>\n          <Writable xsi:type=\"xsd:boolean\">", sizeof("</Name>\n          <Writable xsi:type=\"xsd:boolean\">"));
  ptr += sizeof("</Name>\n          <Writable xsi:type=\"xsd:boolean\">") -1;


  len = strlen(writable);
  memcpy(ptr, writable, len);
  ptr += len;


   memcpy(ptr, "</Writable>\n        </ParameterInfoStruct>", sizeof("</Writable>\n        </ParameterInfoStruct>"));
   ptr += (sizeof("</Writable>\n        </ParameterInfoStruct>")) -1;
   *ptr = '\0';

  (*num_params)++;
  
  NSDL2_TR069(NULL, NULL, "num_params = %d, get_parameters_names_block = [%s]",  *num_params, *cur_ptr);
  *cur_ptr += len;
}

static inline void
tr069_gen_get_parameters_names_block(TR069GPNblockCB callback_fn, 
                                     char **cur_ptr, int *num_params) {
  int len;
  char tmp_buf[TR069_NAME_LEN + TR069_TYPE_LEN + 1];
  char name[TR069_NAME_LEN + 1];
  char *parameter_structs = ns_eval_string("{TR069GPNFP}");
  char writable[TR069_TYPE_LEN + 1];
  char *ptr;
  char *fields[20];
  char new_line[] = "\n";

  NSDL2_TR069(NULL, NULL, "Method Called, parameter_structs = [%s]", parameter_structs);
  char *param_start = parameter_structs;

  if(!param_start) param_start = new_line; 

  while((ptr = index(param_start, '\n'))) {
    len = ptr - param_start;
    strncpy(tmp_buf, param_start, len);
    tmp_buf[len] = '\0';

    get_tokens(tmp_buf, fields, ",", 3);
    if(fields[0]) {
      strncpy(name, fields[0], TR069_NAME_LEN);
    } else {
      strncpy(name, "NotAvailable", TR069_NAME_LEN);
    }
    name[TR069_NAME_LEN] = '\0';

    if(fields[1]) {
      strncpy(writable, fields[1], TR069_TYPE_LEN);
    } else {
      strncpy(writable, "NotAvailable", TR069_TYPE_LEN);
    }
    writable[TR069_TYPE_LEN] = '\0';
      
    NSDL2_TR069(NULL, NULL, "tmp_buf = [%s], name = [%s], writable = [%s]", tmp_buf, name, writable);
    
    callback_fn(name, writable, cur_ptr, num_params);
    param_start = ptr + 1;
  }
}

inline void tr069_parse_gpn_req (VUser *vptr, char *req_ptr) {

  char *cur_ptr = tr069_block; 
  char *param_path = NULL;
  char *next_level = NULL;
  char *tmp = NULL;
  int path_len, level_len, num_params;
  IW_UNUSED(int array_size);
  IW_UNUSED(int status);
  int ret = TR069_ERROR;
  char count_str[16];
  num_params = 0;
  
  NSDL2_TR069(vptr, NULL, "Method Called");
 
  if(global_settings->tr069_options & NS_TR069_OPTIONS_TREE_DATA) {
    #ifdef NS_DEBUG_ON
      array_size = tr069_extract_num_param_frm_req(req_ptr);
    #else
      tr069_extract_num_param_frm_req(req_ptr);
    #endif
    NSDL2_TR069(NULL, NULL, "Making request from run time response data, array_size = %d", array_size);
    while(1) {
      path_len = 0;
      ret = tr069_get_data_value(req_ptr, &tmp, &path_len, "<ParameterPath", ">");
      if(ret == TR069_ERROR)
        break;
      ret = tr069_get_data_value(tmp, &param_path, &path_len, ">", "</ParameterPath>");
      if(ret == TR069_ERROR)
        break;
      req_ptr = param_path + path_len; 
      NSDL2_TR069(NULL, NULL, "req_ptr = %s", req_ptr);

      level_len = 0;
      ret = tr069_get_data_value(req_ptr, &tmp, &level_len, "<NextLevel", ">");
      if(ret == TR069_ERROR)
        break;
      ret = tr069_get_data_value(tmp, &next_level, &level_len, ">", "</NextLevel>");
      if(ret == TR069_ERROR)
        break;
      req_ptr = next_level + level_len; 
      NSDL2_TR069(NULL, NULL, "req_ptr = %s", req_ptr);

      if(level_len == 0)
       break;

      #ifdef NS_DEBUG_ON
        status = tr069_get_param_names_with_cb(vptr, param_path, path_len, 
                                              tr069_gen_get_parameters_names_block_CB, &cur_ptr,
                                              &num_params);
      #else
        tr069_get_param_names_with_cb(vptr, param_path, path_len, 
                                              tr069_gen_get_parameters_names_block_CB, &cur_ptr,
                                              &num_params);
 
      #endif
      NSDL2_TR069(NULL, NULL, "status = %d", status);
    }
  } else {
    tr069_gen_get_parameters_names_block(tr069_gen_get_parameters_names_block_CB, &cur_ptr, &num_params);
    NSDL2_TR069(NULL, NULL, "Making request from static data file.");
  }

  sprintf(count_str, "%d", num_params);
  tr069_fill_response("TR069GetParameterNamesResponse", "TR069NumGPNDP", count_str);
  tr069_fill_response("TR069GetParameterNamesResponse", "TR069GPNInfoStructDP", tr069_block);
}

// GetParameterNames
int tr069_cpe_execute_get_parameter_names_ex(VUser *vptr, int page_id) {

  int ret = -1;

  NSDL2_TR069(vptr, NULL, "Method Called");

  ret = ns_web_url_ext(vptr, page_id);
  ret = tr069_parse_req_and_get_rpc_method_name(vptr, ret);

  NSDL2_TR069(vptr, NULL, "tr069_parse_req_and_get_rpc_method_name returned = %d", ret);
  return ret;
}
/************************** Get Parameter Names Start**************************/

/************************** Set Parameter Attributes Start**************************/
inline void tr069_parse_spa_req(VUser *vptr, char *req_ptr) {

  //char *cur_ptr = tr069_block; 
  char *name = NULL;
  char *Noti_change = NULL;
  char *Notification = NULL;
  char *Access_change = NULL;
  char *Access = NULL;
  char *value = NULL;
  int name_len, Notichange_len, Notification_len, Access_change_len, value_len, Access_len;
  int ret = TR069_ERROR;
  char *tmp = NULL;
  int array_size;

  NSDL2_TR069(NULL, NULL , "Method called");

  if(global_settings->tr069_options & NS_TR069_OPTIONS_TREE_DATA) {
    NSDL2_TR069(NULL, NULL, "Making request from run time response data.");
    array_size = tr069_extract_num_param_frm_req(req_ptr);
    while(array_size) {
      name_len = 0;
      ret = tr069_get_data_value(req_ptr,  &tmp, &name_len, "<Name" ,">");
      NSDL2_TR069(NULL, NULL, "string extracted = %*.*s, len = %d", name_len, name_len, tmp, name_len);
      if(ret == TR069_ERROR)
        break;
      ret = tr069_get_data_value(tmp, &name, &name_len, ">", "</Name>");
      if(ret == TR069_ERROR)
        break;
      req_ptr = name + name_len;
      NSDL2_TR069(NULL, NULL, "req_ptr = %s", req_ptr);
     
      Notichange_len = 0;
      ret = tr069_get_data_value(req_ptr,  &tmp, &Notichange_len, "<NotificationChange" , ">");
      NSDL2_TR069(NULL, NULL, "string extracted = %*.*s, len = %d", Notichange_len, Notichange_len, tmp, Notichange_len);
      if(ret == TR069_ERROR)
        break;
      ret = tr069_get_data_value(tmp, &Noti_change, &Notichange_len, ">", "</NotificationChange>");
      if(ret == TR069_ERROR)
        break;
      req_ptr = Noti_change + Notichange_len;
      NSDL2_TR069(NULL, NULL, "req_ptr = %s", req_ptr);

      Notification_len = 0;
      ret = tr069_get_data_value(req_ptr,  &tmp, &Notification_len, "<Notification", ">");
      NSDL2_TR069(NULL, NULL, "string extracted = %*.*s, len = %d", Notification_len, Notification_len, tmp, Notification_len); 
      if(ret == TR069_ERROR)
        break;
      ret = tr069_get_data_value(tmp, &Notification, &Notification_len, ">", "</Notification>");
      if(ret == TR069_ERROR)
        break; 
      req_ptr = Notification + Notification_len;
      NSDL2_TR069(NULL, NULL, "req_ptr = %s", req_ptr);
   
      Access_change_len = 0;
      ret = tr069_get_data_value(req_ptr, &tmp, &Access_change_len, "<AccessListChange", ">");
      NSDL2_TR069(NULL, NULL, "string extracted = %*.*s, len = %d", Access_change_len, Access_change_len, tmp, Access_change_len);
      if(ret == TR069_ERROR)
        break;
      ret = tr069_get_data_value(tmp, &Access_change, &Access_change_len, ">", "</AccessListChange>"); 
      if(ret == TR069_ERROR)
        break;
      req_ptr = Access_change + Access_change_len;
      NSDL2_TR069(NULL, NULL, "req_ptr = %s", req_ptr);

      Access_len = 0;
      ret = tr069_get_data_value(req_ptr, &Access, &Access_len, "<cwmp:AccessList>", "</cwmp:AccessList>");
      if(ret == TR069_ERROR)
        break;
      req_ptr = Access;
      NSDL2_TR069(NULL, NULL, "req_ptr = %s", req_ptr);
    
      value_len = 0;
      ret=tr069_get_data_value(req_ptr, &tmp, &value_len, "<string", ">");
      if(ret == TR069_ERROR)
        break;
      ret=tr069_get_data_value(tmp, &value, &value_len, ">", "</string>");
      if(ret ==TR069_ERROR)
        break;

      req_ptr = value + value_len;
      NSDL2_TR069(NULL, NULL, "req_ptr= %s", req_ptr);
      array_size--;
      tr069_set_param_attributes_with_cb(vptr, name, name_len, value, value_len);
    }
  } else {

  }
  NSDL2_TR069(NULL, NULL, "Method exiting");
}  

// No arguments
int tr069_cpe_execute_set_parameter_attributes_ex(VUser *vptr, int page_id) {

  int ret = -1;
  NSDL2_TR069(vptr, NULL, "Method Called");

  ret = ns_web_url_ext(vptr, page_id);
  ret = tr069_parse_req_and_get_rpc_method_name(vptr, ret);

  NSDL2_TR069(vptr, NULL, "tr069_parse_req_and_get_rpc_method_name returned = %d", ret);
  return ret;
}

/************************** Set Parameter Attributes Ends**************************/

/************************** Get Parameter Attributes Starts**************************/
static inline void 
tr069_gen_get_parameters_attributes_block_CB(char *name, int notification,
                                             char *accessList, char **cur_ptr,
                                             int *num_params) {
  char accessListblock[2048 + 1];
  char indent_new_line[16] = "";
  char *fields[21];
  int num_tokens;
  int access_idx;
  char *ptr;
  int len;

  accessListblock[0] = '\0';
  ptr = *cur_ptr;
  num_tokens = get_tokens(accessList, fields, "|", 20);

  if(*num_params > 0) 
    sprintf(indent_new_line, "\n        "); 
  
 /* int len = sprintf(*cur_ptr, "%s"
                              "<ParameterAttributeStruct xsi:type=\"cwmp:ParameterAttributeStruct\">\n"
                              "          <Name xsi:type=\"xsd:string\">%s</Name>\n"  
                              "          <Notification xsi:type=\"xsd:int\">%d</Notification>\n"
                              "%s\n"
                              "        </ParameterAttributeStruct>\n",
                              indent_new_line, name, notification, accessListblock);*/
   
  ptr = *cur_ptr;
  len = strlen(indent_new_line);
  memcpy(ptr, indent_new_line, len);
  ptr += len;
   
  memcpy(ptr,"<ParameterAttributeStruct xsi:type=\"cwmp:ParameterAttributeStruct\">\n          <Name xsi:type=\"xsd:string\">", sizeof("<ParameterAttributeStruct xsi:type=\"cwmp:ParameterAttributeStruct\">\n          <Name xsi:type=\"xsd:string\">"));
  ptr += (sizeof("<ParameterAttributeStruct xsi:type=\"cwmp:ParameterAttributeStruct\">\n          <Name xsi:type=\"xsd:string\">")) -1;

  len = strlen(name);
  memcpy(ptr, name, len);
  ptr +=len;
    
  memcpy(ptr, "</Name>\n          <Notification xsi:type=\"xsd:int\">", sizeof("</Name>\n          <Notification xsi:type=\"xsd:int\">"));
  ptr += (sizeof("</Name>\n          <Notificaton xsi:type=\"xsd:int\">"));


  sprintf(ptr, "%d", notification);
  ptr += 1;  // notification has only 3 possible value 0,1,2
  
  memcpy(ptr,"</Notification>\n",sizeof("</Notification>\n"));
  ptr += (sizeof("</Notification>\n")) -1;
  
  // Now create Access Block List 

  len = strlen(accessListblock);
  memcpy(ptr, accessListblock, len);
  ptr += len;
  
  for(access_idx = 0; access_idx < num_tokens; access_idx++) {
    if(accessList[access_idx]) {
      if(strcmp(fields[access_idx], "NA")) {
        if(accessListblock[0] != '\0') {
        /*  sprintf(accessListblock, "%s\n"
                                  "          <AccessList>\n"
                                  "            <xsd:string xsi:type=\"xsd:string\">%s</xsd:string>\n"
                                  "          </AccessList>",
                                  accessListblock, fields[access_idx]);
        */
          memcpy(ptr,   "\n          <AccessList>\n            <xsd:string xsi:type=\"xsd:string\">",
                 sizeof("\n          <AccessList>\n            <xsd:string xsi:type=\"xsd:string\">"));
          ptr += sizeof("\n          <AccessList>\n            <xsd:string xsi:type=\"xsd:string\">") -1;
  

          len = strlen(fields[access_idx]);
          memcpy(ptr, fields[access_idx], len);
          ptr += len;

          memcpy(ptr,   "</xsd:string>\n          </AccessList>",
                 sizeof("</xsd:string>\n          </AccessList>"));
          ptr += sizeof("</xsd:string>\n          </AccessList>") -1;
        } else {
          /*sprintf(accessListblock, "          <AccessList>\n"
                                  "            <xsd:string xsi:type=\"xsd:string\">%s</xsd:string>\n"
                                  "          </AccessList>",
                                  fields[access_idx]); */
          memcpy(ptr,"          <AccessList>\n            <xsd:string xsi:type=\"xsd:string\">", 
              sizeof("          <AccessList>\n            <xsd:string xsi:type=\"xsd:string\">"));
          ptr += (sizeof("          <AccessList>\n            <xsd:string xsi:type=\"xsd:string\">")) -1;
  

          len = strlen(fields[access_idx]);
          memcpy(ptr, fields[access_idx], len);
          ptr += len;

          memcpy(ptr, "</xsd:string>\n          </AccessList>", sizeof( "</xsd:string>\n          </AccessList>"));
          ptr += (sizeof( "</xsd:string>\n          </AccessList>")) -1;
          *ptr = '\0';
        }
      }
    }
  }

  memcpy(ptr, "\n        </ParameterAttributeStruct>\n", sizeof("\n        </ParameterAttributeStruct>\n"));
  ptr += (sizeof("\n        </ParameterAttributeStruct>\n")) -1;
  *ptr ='\0';  
    
  (*num_params)++;

  NSDL2_TR069(NULL, NULL, "num_params = %d, get_parameters_names_block = [%s]",  *num_params, *cur_ptr);
  //*cur_ptr += len;
  *cur_ptr = ptr; 
}

static inline void
tr069_gen_get_parameters_attributes_block(TR069GPAblockCB callback_fn,
                                          char **cur_ptr, int *num_params) {

  char tmp_buf[TR069_NAME_LEN + TR069_TYPE_LEN + TR069_VALUE_LEN + 1];
  char name[TR069_NAME_LEN + 1];
  char notification[TR069_TYPE_LEN];
  char *parameter_structs = ns_eval_string("{TR069GPAListFP}");
  char accessList[TR069_VALUE_LEN + 1];
  char *fields[21];
  int len;
  char new_line[] = "\n";
  char *ptr;
  char *param_start = parameter_structs;
  NSDL2_TR069(NULL, NULL, "parameter_structs = [%s]", parameter_structs);

  if(!param_start) param_start = new_line; 

  while((ptr = index(param_start, '\n'))) {
    len = ptr - param_start;
    strncpy(tmp_buf, param_start, len);
    tmp_buf[len] = '\0';

    get_tokens(tmp_buf, fields, ",", 3);
    if(fields[0]) {
      strncpy(name, fields[0], TR069_NAME_LEN);
    } else {
      name[0] = '\0';
    }

    if(fields[1]) {
      strncpy(notification, fields[1], TR069_NAME_LEN);
    } else {
      notification[0] = '\0';
    }

    if(fields[2]) {
      strncpy(accessList, fields[1], TR069_NAME_LEN);
    } else {
      accessList[0] = '\0';
    }

    NSDL2_TR069(NULL, NULL, "tmp_buf = [%s], name = [%s], notification = [%s], accessList = [%s]",
                             tmp_buf, name, notification, accessList);
    callback_fn(name, atoi(notification), accessList, cur_ptr, num_params);
    param_start = ptr + 1;
  }
}

inline void tr069_parse_gpa_req(VUser *vptr, char *req_ptr) {

  int num_params = 0;
  char *value = NULL;
  int value_len;
  int ret = TR069_ERROR;
  char *tmp = NULL;
  char count_str[16];
  int array_size = tr069_extract_num_param_frm_req(req_ptr);
  char *cur_ptr = tr069_block; 

  NSDL2_TR069(NULL, NULL, "Method called");

  if(global_settings->tr069_options & NS_TR069_OPTIONS_TREE_DATA) {
    array_size = tr069_extract_num_param_frm_req(req_ptr);
    NSDL2_TR069(NULL, NULL, "Making request from run time response data, array_size = %d", array_size);
    while(array_size) {
      value_len = 0;
      ret = tr069_get_data_value(req_ptr, &tmp, &value_len, "<string", ">");
      if(ret == TR069_ERROR)
        break;
      ret = tr069_get_data_value(tmp, &value, &value_len, ">", "</string>");
      if(ret == TR069_ERROR)
        break;

      req_ptr = value + value_len;
      NSDL2_TR069(NULL, NULL, "req_ptr= %s", req_ptr);
      NSDL2_TR069(NULL, NULL, "Extracted value = %*.*s, value_len = %d", value_len, value_len, value, value_len);
      tr069_get_param_attributes_with_cb(vptr, value, value_len, 
                                         tr069_gen_get_parameters_attributes_block_CB,
                                         &cur_ptr, &num_params);

      array_size--;
    }
  } else {
    tr069_gen_get_parameters_attributes_block(tr069_gen_get_parameters_attributes_block_CB, &cur_ptr, &num_params);
  }

  sprintf(count_str, "%d", num_params);
  tr069_fill_response("TR069GetParameterAttribuetesResponse", "TR069NumGPAStructsDP", count_str);
  tr069_fill_response("TR069GetParameterAttribuetesResponse", "TR069GPAStructDP", tr069_block);

  NSDL2_TR069(NULL, NULL, "Method exiting");
}
// GetParameterAttributes
// Only ParameterNames comes as an array
int tr069_cpe_execute_get_parameter_attributes_ex(VUser *vptr, int page_id) {

  int ret = -1;
  NSDL2_TR069(vptr, NULL, "Method Called");

  ret = ns_web_url_ext(vptr, page_id);
  ret = tr069_parse_req_and_get_rpc_method_name(vptr, ret);

  NSDL2_TR069(vptr, NULL, "tr069_parse_req_and_get_rpc_method_name returned = %d", ret);
  return ret;
}

/************************** Get Parameter Attributes Ends**************************/
