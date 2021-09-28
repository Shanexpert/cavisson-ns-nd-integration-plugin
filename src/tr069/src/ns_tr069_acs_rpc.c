#include "ns_tr069_includes.h"
#include "../../ns_tls_utils.h"

#define NS_TWO_SPACES	"  "
#define NS_FOUR_SPACES  "    "


/* Generate req id and set in TR069ReqId variable
   Generate a uniq req Id across NVMs in following format
   TR<testrun>NVM<nvmid><counter>
   e.g  TR1234.NVM001.11234
*/

void tr069_gen_req_id(char *req_id, char *forWhich) {
   static unsigned int tr069_req_id_counter = 0;
  
   NSDL2_TR069(NULL, NULL, "Method Called, forWhich = [%s]", forWhich);

   sprintf(req_id, "TR%d.NVM%d.%d", testidx, my_port_index, tr069_req_id_counter++);

   NSDL2_TR069(NULL, NULL, "Generated req_id = [%s]", req_id);
}

static void tr069_get_inform_device_info_parameters(VUser *vptr, char *cur_ptr) {

  int value_len;
  TR069ParamPath_t *param_path_table_ptr = NULL; 
  char *Manufacturer, Manufacturer_str[256];
  char *OUI, OUI_str[256];
  char *ProductClass, ProductClass_str[256];
  char *SerialNumber, SerialNumber_str[256];
  char *ptr;
  int len;

  NSDL2_TR069(vptr, NULL, "Method Called");

  if(global_settings->tr069_options & NS_TR069_OPTIONS_TREE_DATA)  {
     param_path_table_ptr = &tr069_param_path_table[vptr->user_index * total_param_path_entries];

     if(IGDDeviceInfoManufactureridx >= 0)
       Manufacturer = tr069_get_value_from_big_buf(param_path_table_ptr[IGDDeviceInfoManufactureridx].data.value, &value_len);
     else 
       Manufacturer = "";

     if(IGDDeviceInfoManufacturerOUIidx >= 0)
       OUI = tr069_get_value_from_big_buf(param_path_table_ptr[IGDDeviceInfoManufacturerOUIidx].data.value, &value_len);
     else 
       OUI = "";

     if(IGDDeviceInfoProductClassidx >= 0)
       ProductClass = tr069_get_value_from_big_buf(param_path_table_ptr[IGDDeviceInfoProductClassidx].data.value, &value_len);
     else 
       ProductClass = "";

     if(IGDDeviceInfoSerialNumberidx >= 0)
       SerialNumber = tr069_get_value_from_big_buf(param_path_table_ptr[IGDDeviceInfoSerialNumberidx].data.value, &value_len);
     else 
       SerialNumber = "";

  } else {  //TODO need to be advanced
    Manufacturer = Manufacturer_str;
    strcpy(Manufacturer, ns_eval_string("{TR069ManufacturerFP}"));
    OUI = OUI_str;
    strcpy(OUI, ns_eval_string("{TR069OUIFP}"));
    ProductClass = ProductClass_str;
    strcpy(ProductClass, ns_eval_string("{TR069ProductClassFP}"));
    SerialNumber = SerialNumber_str;
    strcpy(SerialNumber, ns_eval_string("{TR069SerialNumberFP}"));
  }
/*
  <DeviceId xmlns="urn:dslforum-org:cwmp-1-0">
     <Manufacturer xsi:type="xsd:string">{TR069ManufacturerFP}</Manufacturer>
     <OUI xsi:type="xsd:string">{TR069OUIFP}</OUI>
     <ProductClass xsi:type="xsd:string">{TR069ProductClassFP}</ProductClass>
     <SerialNumber xsi:type="xsd:string">{TR069SerialNumberFP}</SerialNumber>
  </DeviceId>
*/
  ptr = cur_ptr;
  memcpy(ptr,  "<Manufacturer xsi:type=\"xsd:string\">", sizeof("<Manufacturer xsi:type=\"xsd:string\">"));
  ptr += (sizeof("<Manufacturer xsi:type=\"xsd:string\">"))- 1;
  len = strlen(Manufacturer);
  memcpy(ptr, Manufacturer, len);
  ptr += len;
  
  memcpy(ptr, "</Manufacturer>\n        <OUI xsi:type=\"xsd:string\">", sizeof( "</Manufacturer>\n        <OUI xsi:type=\"xsd:string\">"))
; 
  ptr += (sizeof("</Manufacturer>\n        <OUI xsi:type=\"xsd:string\">")) - 1;
  len = strlen(OUI);
  memcpy(ptr, OUI, len);
  ptr += len;

  memcpy(ptr, "</OUI>\n        <ProductClass xsi:type=\"xsd:string\">", sizeof("</OUI>\n        <ProductClass xsi:type=\"xsd:string\">"));
  ptr += (sizeof( "</OUI>\n        <ProductClass xsi:type=\"xsd:string\">")) - 1;
  len = strlen(ProductClass);
  memcpy(ptr, ProductClass, len);
  ptr += len;

  memcpy(ptr, "</ProductClass>\n        <SerialNumber xsi:type=\"xsd:string\">", sizeof( "</ProductClass>\n        <SerialNumber xsi:type=\"xsd:string\">"));
  ptr += (sizeof( "</ProductClass>\n        <SerialNumber xsi:type=\"xsd:string\">")) - 1;
  len = strlen(SerialNumber);
  memcpy(ptr, SerialNumber, len);
  ptr += len;
  
  memcpy(ptr, "</SerialNumber>", sizeof("</SerialNumber>"));
  ptr  += (sizeof("</SerialNumber>")) - 1;
  
  *ptr ='\0'; 
  
}

// This will be replace by tree method later
static int tr069_get_inform_parameters(TR069GetInformParamCB callback_fn, VUser *vptr,
                                char **cur_ptr, int *num_params) {
  int idx;
  int len;
  char *ptr;
//  char *parameter_structs; 
  char *param_start;
  char tmp_buf[1024];
  char *fields[4]; 
  //int name_len;
  char name[512], type[128], value[512];
  char *name_ptr, *value_ptr;

  TR069ParamPath_t *param_path_ptr;
  int param_path_ptr_idx;

  NSDL2_TR069(vptr, NULL, "Method Called");

  if(global_settings->tr069_options & NS_TR069_OPTIONS_TREE_DATA)  {
    // Get Data from tree data
    param_path_ptr = &tr069_param_path_table[vptr->user_index * total_param_path_entries];
    NSDL2_TR069(vptr, NULL, "Getting inform parameters from tree data.");
    if(!(vptr->httpData->flags & (NS_TR069_EVENT_VALUE_CHANGE_PASSIVE || NS_TR069_EVENT_VALUE_CHANGE_ACTIVE))) {
      NSDL2_TR069(vptr, NULL, "Traversing tr069_inform_paramters_indexes,"
                              " total_inform_parameters = %d",
                              total_inform_parameters);
      for(idx = 0; idx < total_inform_parameters; idx++) {
        param_path_ptr_idx = tr069_inform_paramters_indexes[idx]; 
        //name_ptr = tr069_get_name_from_index(param_path_ptr_idx, &name_len);
        name_ptr = tr069_index_to_path_name(param_path_ptr_idx);
        if(param_path_ptr[param_path_ptr_idx].path_type_flags & NS_TR069_PARAM_STRING) {
          value_ptr = tr069_get_value_from_big_buf(param_path_ptr[param_path_ptr_idx].data.value, &len);
          strcpy(type, "string");
        } else {
          value_ptr = value;
          sprintf(value_ptr, "%lu", param_path_ptr[param_path_ptr_idx].data.value->value_idx);
          strcpy(type, "unsignedInt");
        }
        callback_fn(name_ptr, type, value_ptr, cur_ptr, num_params);
      }

    } else {
      NSDL2_TR069(vptr, NULL, "Traversing tr069 data tree, total_full_param_path_entries = %d",
                               total_full_param_path_entries);
      for(idx = 0; idx < total_full_param_path_entries; idx++) {
        param_path_ptr_idx = tr069_full_param_path_table[idx].path_table_index;
        //name_ptr = tr069_get_name_from_index(param_path_ptr_idx, &name_len);
        name_ptr = tr069_index_to_path_name(param_path_ptr_idx);
        if(param_path_ptr[param_path_ptr_idx].path_type_flags & 
           (NS_TR069_INFORM_PARAMETER || NS_TR069_INFORM_PARAMETER_RUN_TIME)) {
          if(param_path_ptr[param_path_ptr_idx].path_type_flags & NS_TR069_PARAM_STRING) {
            value_ptr = tr069_get_value_from_big_buf(param_path_ptr[param_path_ptr_idx].data.value, &len);
            strcpy(type, "string");
          } else {
            value_ptr = value;
            sprintf(value_ptr, "%lu", param_path_ptr[param_path_ptr_idx].data.value->value_idx);
            strcpy(type, "unsignedInt");
          }
          callback_fn(name_ptr, type, value_ptr, cur_ptr, num_params);
          param_path_ptr[param_path_ptr_idx].path_type_flags &= ~NS_TR069_INFORM_PARAMETER_RUN_TIME;
        }
      }
    }
  } else {
    NSDL2_TR069(vptr, NULL, "Getting inform parameters from file parameter.");
    // Get Data from file parameter
    //parameter_structs = ns_eval_string("{TR069IPFP}");
    while((ptr = index(param_start, '\n'))) {
      len = ptr - param_start;

      // TODO OPtimized follwing block of code
      strncpy(tmp_buf, param_start, len);
      tmp_buf[len] = '\0';
      get_tokens(tmp_buf, fields, "|", 3); 
      if(fields[0]) {
         strcpy(name, fields[0]);
      } else {
        name[0] = '\0';
      }
      if(fields[1]) {
         strcpy(type, fields[1]);
      } else {
        type[0] = '\0';
      }
      if(fields[2]) {
         strcpy(value, fields[2]);
      } else {
        value[0] = '\0';
      }
      NSDL2_TR069(NULL, NULL, "tmp_buf = [%s], name = [%s], type = [%s], value = [%s]", tmp_buf, name, type, value);

      callback_fn(name, type, value, cur_ptr, num_params);
      //param_start += len;
      param_start = ptr + 1;
    }
  }
  return 0;
}
/**
 *
 */
static void tr069_gen_parameter_value_struct_block(char *name, char *type, char *value, char **cur_ptr, int *num_params)
{
  char indent_new_line[100] = "";
  char *ptr;
  int len;

  NSDL2_TR069(NULL, NULL, "Method called. name = [%s], type = [%s], value = [%s]", name, type, value);

  /*  <ParameterValueStruct xsi:type="cwmp:ParameterValueStruct">
          <Name xsi:type="xsd:string">InternetGatewayDevice.WANDevice.2.WANEthernetInterfaceConfig.Status</Name>
          <Value xsi:type="xsd:string">Up</Value>
        </ParameterValueStruct>*/


  if(*num_params > 0)
    strcpy(indent_new_line, "\n        "); // indent and New line is to be added for all events except the first one

    ptr = *cur_ptr;

    len = strlen(indent_new_line);
    memcpy(ptr, indent_new_line, len);
    ptr += len;
   
    memcpy(ptr, "<ParameterValueStruct>\n          <Name>", sizeof("<ParameterValueStruct>\n          <Name>"));
    NSDL2_TR069(NULL, NULL, "num_params = %d, parameter_struct_block = [%s]", *num_params, *cur_ptr);
    ptr += (sizeof("<ParameterValueStruct>\n          <Name>")) - 1;

    len = strlen(name);
    memcpy(ptr, name, len);  
    ptr += len ;
    
    memcpy(ptr, "</Name>\n          <Value xsi:type=\"xsd:", sizeof("</Name>\n          <Value xsi:type=\"xsd:"));
    ptr += (sizeof("</Name>\n          <Value xsi:type=\"xsd:")) - 1 ;

    len = strlen(type);
    memcpy(ptr, type, len);
    ptr += len ;

    memcpy(ptr, "\">", sizeof("\">"));
    NSDL2_TR069(NULL, NULL, "num_params = %d, parameter_struct_block = [%s]", *num_params, *cur_ptr);
    ptr += (sizeof("\">")) - 1 ;

    len = strlen(value);
    memcpy(ptr, value, len);
    ptr += len;
    memcpy(ptr, "</Value>\n        </ParameterValueStruct>", sizeof("</Value>\n        </ParameterValueStruct>"));
    NSDL2_TR069(NULL, NULL, "num_params = %d, parameter_struct_block = [%s]", *num_params, *cur_ptr);
    ptr += (sizeof("</Value>\n        </ParameterValueStruct>")) - 1;

   /* len = strlen(value);
    memcpy(ptr, value, len);
    ptr += len ;*/
    *ptr = '\0';

  (*num_params)++;
  NSDL2_TR069(NULL, NULL, "num_params = %d, parameter_struct_block = [%s]", *num_params, *cur_ptr);

  *cur_ptr = ptr;
}

#if 0
// Name: tr069_get_next_event
// Purpose: Get next event mask to be filled in inform message
// Input
//   events = Event mask with bits set for events which are to be sent
//   event_id = Pointer to event id which is filled by this method
// Return
//   Bit mask of next event or  0 if no more events are there
//   event_id is updated with event id of the event
//   
static int tr069_get_next_event(int events, int *event_id)
{
int event_id_mask = 0x00000001; // Set with bit 0 on so that we can shift this bit

  NSDL2_TR069(NULL, NULL, "Method called. events = 0x%x, event_id = %d ", events, *event_id);

  while(*event_id < TR069_MAX_EVENTS)
  {
    event_id_mask = 1 << (*event_id);
    NSDL2_TR069(NULL, NULL, "event_id_mask = 0x%x, event_id = %d", event_id_mask, *event_id);
    if(event_id_mask & events)
      return event_id_mask;

    (*event_id)++;
  }
  return 0;
}
#endif

static inline void tr069_fill_event_blk(char **cur_ptr, char *cmd_key, int event_id, int *num_events){
  int len = 0;
  char indent_new_line[100] = "";
  char *ptr;
  NSDL2_TR069(NULL, NULL, "Method called. cm_key = %s, event_id = %d, num_events = %d", cmd_key, event_id, *num_events);

  if(*num_events > 0)
    strcpy(indent_new_line, "\n        "); // indent and New line is to be added for all events except the first one

    ptr = *cur_ptr;  
    len = strlen(indent_new_line);
    memcpy(ptr, indent_new_line, len);
    ptr += len;

    memcpy(ptr, "<EventStruct xsi:type=\"cwmp:EventStruct\">\n          <EventCode xsi:type=\"xsd:string\">", sizeof("<EventStruct xsi:type=\"cwmp:EventStruct\">\n          <EventCode xsi:type=\"xsd:string\">"));
    ptr += (sizeof("<EventStruct xsi:type=\"cwmp:EventStruct\">\n          <EventCode xsi:type=\"xsd:string\">")) - 1;
    len = strlen( tr069_get_event_name(event_id));
    memcpy(ptr, tr069_get_event_name(event_id), len);
    ptr += len;

    memcpy(ptr, "</EventCode>\n          <CommandKey xsi:type=\"xsd:string\">", sizeof("</EventCode>\n          <CommandKey xsi:type=\"xsd:string\">"));
    ptr += (sizeof("</EventCode>\n          <CommandKey xsi:type=\"xsd:string\">")) - 1;
    len =strlen(cmd_key);
    memcpy(ptr, cmd_key, len);
    ptr += len;
    
    memcpy(ptr, "</CommandKey>\n        </EventStruct>", sizeof("</CommandKey>\n        </EventStruct>"));
    ptr += (sizeof("</CommandKey>\n        </EventStruct>")) - 1;
    *ptr = '\0';    
   
    *cur_ptr = ptr;
   (*num_events)++;
   

  NSDL2_TR069(NULL, NULL, "Method exiting, num_events = %d", *num_events);
}

//
//This function is to set cmd key
void tr069_set_reboot_cmd_key (VUser *vptr, char *cmd_key, int cmd_key_len)
{
  NSDL2_TR069(NULL, NULL, "Method called");
  MY_MALLOC(vptr->httpData->reboot_cmd_key, cmd_key_len + 1, "cmd key", 1);

  memcpy(vptr->httpData->reboot_cmd_key, cmd_key, cmd_key_len);
  vptr->httpData->reboot_cmd_key[cmd_key_len] = '\0';

  vptr->httpData->flags |=  NS_TR069_EVENT_BOOT;
  vptr->httpData->flags |=  NS_TR069_EVENT_M_REBOOT;

  NSDL2_TR069(NULL, NULL, "Method exiting, Cmd Key = %s", vptr->httpData->reboot_cmd_key);
}

//
//This function is to set cmd key
void tr069_set_download_cmd_key (VUser *vptr, char *cmd_key, int cmd_key_len)
{
  NSDL2_TR069(NULL, NULL, "Method called");
  MY_MALLOC(vptr->httpData->download_cmd_key, cmd_key_len + 1, "cmd key", 1);

  memcpy(vptr->httpData->download_cmd_key, cmd_key, cmd_key_len);
  vptr->httpData->download_cmd_key[cmd_key_len] = '\0';

  vptr->httpData->flags |=  NS_TR069_EVENT_M_DOWNLOAD;
  vptr->httpData->flags |=  NS_TR069_EVENT_TRANSFER_COMPLETE;

  NSDL2_TR069(NULL, NULL, "Method exiting, Cmd Key = %s", vptr->httpData->download_cmd_key);
}

// Name: tr069_gen_inform_event_block
// Purpose: Fill EventStruct part of inform request (one or more) based on pending events
// Input:
//   vptr: Virtual user data pointer
//   event_block: Buffer in which EventStruct is filled. This must have sufficient space
// Return:
//   None. 
//   event_block is filled

static int tr069_gen_inform_event_block(VUser *vptr, char* event_block) {
  char *cur_ptr;
  int num_events = 0;
  int ret = 0;

  NSDL2_TR069(NULL, NULL, "Method Called. flags = [0x%x]", vptr->httpData->flags);

  if(vptr->httpData->flags == 0)
  {
    //fprintf(stderr, "Error: There are no pending events");
    strcpy(event_block, "Error: There are no pending events");
    return ret;
  }

  cur_ptr = event_block;

  if(vptr->httpData->flags & NS_TR069_EVENT_BOOTSTRAP)
    tr069_fill_event_blk(&cur_ptr, "", NS_TR069_BOOTSTRAP_ID, &num_events);
   
  if(vptr->httpData->flags & NS_TR069_EVENT_BOOT)
    tr069_fill_event_blk(&cur_ptr, "", NS_TR069_BOOT_ID, &num_events);

  if(vptr->httpData->flags & NS_TR069_EVENT_M_REBOOT)
    tr069_fill_event_blk(&cur_ptr, vptr->httpData->reboot_cmd_key, NS_TR069_M_REBOOT_ID, &num_events);

  if((vptr->httpData->flags & NS_TR069_EVENT_VALUE_CHANGE_PASSIVE) 
           || (vptr->httpData->flags & NS_TR069_EVENT_VALUE_CHANGE_ACTIVE))
    tr069_fill_event_blk(&cur_ptr, "", NS_TR069_VALUE_CHANGE_ID, &num_events);

  if(vptr->httpData->flags & NS_TR069_EVENT_PERIODIC)
    tr069_fill_event_blk(&cur_ptr, "", NS_TR069_PERIODIC_ID, &num_events);

  if(vptr->httpData->flags & NS_TR069_EVENT_GOT_RFC)
  {
    // Must sent 6 CONNECTION REQUEST as we got connection from ACS
    NSDL2_TR069(NULL, NULL, "Since we got connection from ACS, setting event bit for 6 CONNECTION REQUEST");
    tr069_fill_event_blk(&cur_ptr, "", NS_TR069_GOT_RFC_ID, &num_events);
  }

  if(vptr->httpData->flags & NS_TR069_EVENT_M_DOWNLOAD)
  {
    NSDL2_TR069(NULL, NULL, "Setting M Download event");
    tr069_fill_event_blk(&cur_ptr, vptr->httpData->download_cmd_key, NS_TR069_M_DOWNLOAD_ID, &num_events);
  }

  if(vptr->httpData->flags & NS_TR069_EVENT_TRANSFER_COMPLETE)
  {
    NSDL2_TR069(NULL, NULL, "Setting Transfer complete event");
    tr069_fill_event_blk(&cur_ptr, "", NS_TR069_TRANSFER_COMPLETE_ID, &num_events);
    //Fill variable with command key 
    tr069_fill_response("TR069TransferComplete", "TR069CmdKeyDP", vptr->httpData->download_cmd_key);
    ret = NS_TR069_TRANSFER_COMPLETE;
  }

  if((vptr->httpData->flags & NS_TR069_EVENT_M_DOWNLOAD) && (vptr->httpData->flags & NS_TR069_EVENT_TRANSFER_COMPLETE))
  {
    NSDL2_TR069(NULL, NULL, "Setting bits for download and transfer complete");
    //Reset all bits first and then set what is needed
    vptr->httpData->flags = 0;
    // Set bits to send events for these events (boot, download, value change)
    vptr->httpData->flags = (NS_TR069_EVENT_BOOT | NS_TR069_EVENT_M_DOWNLOAD | NS_TR069_EVENT_VALUE_CHANGE_PASSIVE);
  }
  else 
  {
    NSDL2_TR069(NULL, NULL, "Bits are not set for download and transfer complete");
    //Reset all bits
    vptr->httpData->flags = 0;
    FREE_AND_MAKE_NULL(vptr->httpData->reboot_cmd_key, "reboot_cmd_key buffer", 1);
    FREE_AND_MAKE_NULL(vptr->httpData->download_cmd_key, "download_cmd_key buffer", 1);
  }

  // Set this variable with the number of events
  ns_set_int_val("TR069NumEventsDP", num_events); // TODO - make a method

  NSDL2_TR069(NULL, NULL, "num_events = %d, event_block = [%s], ret = %d", num_events, event_block, ret);
  return ret;
}


static void tr069_gen_auth_header(VUser *vptr, char* headers) {

  char *ptr;
  //char u_name[128];
  //char u_passwd[128];
  char t_buf[1024];

  NSDL2_TR069(NULL, NULL, "Method Called");

  if(global_settings->tr069_options & NS_TR069_OPTIONS_TREE_DATA)  {
    ptr = tr069_get_full_param_values_str(vptr, "InternetGatewayDevice.ManagementServer.Username", -1); 
  } else {
    ptr = ns_eval_string("{TR069UsernameFP}");
  }

  // Get User name
  if(ptr) {
    sprintf(t_buf, "%s:", ptr);
  } else {
    headers[0] = '\0';
    return;
  }

  // Get Password
  if(global_settings->tr069_options & NS_TR069_OPTIONS_TREE_DATA)  {
    ptr = tr069_get_full_param_values_str(vptr, "InternetGatewayDevice.ManagementServer.Password", -1); 
  } else {
    ptr = ns_eval_string("{TR069PasswordFP}");
  }

  if(ptr) {
    strcat(t_buf, ptr);
  } else {
    headers[0] = '\0';
    return;
  }
 
  //int offset = sprintf(headers, "Authorization: Basic ");
  int offset = sprintf(headers, "Content-Type: text/xml; charset=utf-8\r\nAuthorization: Basic ");
  
  ns_base64_encode(t_buf, headers + offset);
  strcat(headers, "\r\n");
}

int tr069_cpe_auth(VUser *vptr, int page_id) { 

  NSDL2_TR069(vptr, NULL, "Method Called");
  
  tr069_gen_auth_header(vptr, tr069_block); 
  tr069_fill_response("TR069InformHeaders", "TR069ReqHeadersDP", tr069_block);

  return 0;
}

// Invoke Inform
int tr069_cpe_invoke_inform_ex(VUser *vptr, int page_id) { 
  int ret = -1;
  char *cur_ptr = tr069_block;
  int num_params = 0;
  int invoke_ret;

  NSDL2_TR069(vptr, NULL, "Method Called");
  TLS_SET_VPTR(vptr);


  tr069_gen_req_id(tr069_block, "TR069Inform");
  tr069_fill_response("TR069Inform", "TR069CPEReqIDDP", tr069_block);

  invoke_ret = tr069_gen_inform_event_block(vptr, tr069_block);
  
  tr069_fill_response("TR069InformEventStruct", "TR069IEStructDP", tr069_block);

  tr069_get_inform_device_info_parameters(vptr, tr069_block);
  tr069_fill_response("TR069InformParameterValueStruct", "TR069DeviceInfoDP", tr069_block);

  tr069_get_inform_parameters(tr069_gen_parameter_value_struct_block, vptr, &cur_ptr, &num_params);  
  tr069_fill_response("TR069InformParameterValueStruct", "TR069IPVStructDP", tr069_block);
  // Set this variable with the number of paramters
  ns_set_int_val("TR069NumIPVDP", num_params); // TODO - make a method

  if(global_settings->tr069_options & NS_TR069_OPTIONS_AUTH) {   // Auth in first attempt
    tr069_cpe_auth(vptr, page_id);
    ret = ns_web_url_ext(vptr, page_id);
    ret = invoke_ret;
    tr069_clear_vars();
    return ret;
  } else {
    ret = ns_web_url_ext(vptr, page_id);
  }

  // TODO: Use 401 req_code in cptr and also make is pass. Issue is we do not have cptr at this point
  if(ret == NS_REQUEST_OK) {
    tr069_cpe_auth(vptr, page_id);
    ret = ns_web_url_ext(vptr, page_id);
  }
  
  // Clean Body & Method
  tr069_clear_vars();
  ret = invoke_ret;
  return ret;
}

// Invite RPC (Its Just an empty post)
int tr069_cpe_invite_rpc_ex(VUser *vptr, int page_id) {

  int ret = -1;
  NSDL2_TR069(vptr, NULL, "Method Called");

  ret = ns_web_url_ext(vptr, page_id);
  ret = tr069_parse_req_and_get_rpc_method_name(vptr, ret);

  NSDL2_TR069(vptr, NULL, "tr069_parse_req_and_get_rpc_method_name returned = %d", ret);
  return ret;
}
