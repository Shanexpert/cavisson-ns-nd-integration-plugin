#include <stddef.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "ns_log.h"
#include "ns_data_types.h"
#include "ns_rdp_api.h"


#define NS_RDP_BUFF_SIZE 128
GlobalRdpData g_rdp_vars;

char *rdp_api_arr[RDP_API_COUNT] = {NS_RDP_CONNECT_STR, NS_RDP_DISCONNECT_STR, NS_KEY_STR, NS_KEY_DOWN_STR, NS_KEY_UP_STR,
					NS_TYPE_STR, NS_MOUSE_DOWN_STR, NS_MOUSE_UP_STR, NS_MOUSE_CLICK_STR,
					NS_MOUSE_DOUBLE_CLICK_STR, NS_MOUSE_MOVE_STR, NS_MOUSE_DRAG_STR, NS_SYNC_STR} ;



int total_rdp_request_entries; /*bug 79149*/
void segment_line_ex(StrEnt* segtable, char* line, int line_number, int sess_idx, char *fname, int id_flag, int *curr_flag)
{

  NSDL1_RDP(NULL, NULL, "Method called. line = %s", line);
  if(id_flag == -1)                                                          
   SCRIPT_PARSE_ERROR(script_line, "ID is a mandatory argument and must be provided in the API");
   
  if(*curr_flag != -1) {
      NS_EXIT(-1, "%s can be given once.", line);
  }
  *curr_flag = 1;
  CLEAR_WHITE_SPACE(line);
  CLEAR_WHITE_SPACE_FROM_END(line);
  //ToDo: check for = before key or values
  segment_line(segtable, line, 0, line_number, sess_idx, fname);
  NSDL2_RDP(NULL, NULL, "line = %s", line);
}
 
void rdp_init(int row_num, int proto, int api_type)
{
  NSDL2_MISC(NULL, NULL, "total_rdp_req_entries = %d", total_rdp_request_entries);
  requests[row_num].request_type = proto;
  requests[row_num].proto.rdp.operation = api_type;
  switch(api_type)
  {
    case NS_RDP_CONNECT:
    {
      requests[row_num].proto.rdp.connect.host.seg_start = -1;
      requests[row_num].proto.rdp.connect.host.num_entries= 0;
 
      requests[row_num].proto.rdp.connect.user.seg_start = -1;
      requests[row_num].proto.rdp.connect.user.num_entries= 0;
      
      requests[row_num].proto.rdp.connect.password.seg_start = -1;
      requests[row_num].proto.rdp.connect.password.num_entries= 0;
      
      requests[row_num].proto.rdp.connect.domain.seg_start = -1;
      requests[row_num].proto.rdp.connect.domain.num_entries= 0;
     
      total_rdp_request_entries++;
    }
    break;

    case NS_RDP_DISCONNECT:
    break;
  }
}


int nsi_rdp_connect(char* host, char* user, char* pwd, char* domain)
{
 
  VUser *vptr = (VUser*)TLS_GET_VPTR();
  NSDL2_RTE(vptr,NULL, "Method called. host = %p user = %p pwd = %p domain = %p", host, user, pwd, domain);

  if(!host || !host[0] || !user || !pwd)
  {
    char err_buff[1024];
    sprintf(err_buff, "Error!!! Invalid I/Ps");
    NSDL3_RTE(vptr, NULL, "%s", err_buff);
    NSTL1(vptr, NULL, "%s", err_buff);
    return RDP_CONN_FAIL;
  }
 
  NSDL2_RTE(vptr,NULL, "host = %s user = %s pwd = %s domain = %s", host, user, pwd, domain);
  char cmd[500];
  if(domain && domain[0])
    sprintf(cmd, "xfreerdp +clipboard /cert:ignore /grab-keyboard /u:%s /p:%s /d:%s /v:%s >/dev/null  2>&1 < /dev/null & echo -n $! ", user, pwd, domain, host);
  else
    sprintf(cmd, "xfreerdp +clipboard /cert:ignore /grab-keyboard /u:%s /p:%s  /v:%s >/dev/null  2>&1 < /dev/null & echo -n $! ", user, pwd, host);
  
  RTE_THINK_TIME(vptr, NULL, 5*1000)
  NSDL2_RTE(vptr,NULL, "cmd = %s", cmd);

  if(ns_desktop_open(cmd) != RDP_SUCCESS)
  {
    NSDL1_RTE(vptr, NULL, "ns_desktop_open failed!!!!");
    NSTL1(vptr, NULL, "ns_desktop_open failed!!!!");
    return  RDP_CONN_FAIL;
  }
  NSDL2_RTE(vptr,NULL, "returning. vptr[%p]->xwp = %p", vptr, vptr->xwp);
  return RDP_SUCCESS;
}


int nsi_rdp_disconnect()
{
 
  VUser *vptr = (VUser*)TLS_GET_VPTR();
  NSDL2_RTE(vptr,NULL, "Method called");

  ns_desktop_key_type(KEY_DOWN, "<Alt_L>");
  ns_desktop_key_type(KEY_DOWN, "<F4>");
  ns_desktop_wait_sync(1);
  ns_desktop_key_type(KEY_UP, "<Alt_L>");
  ns_desktop_key_type(KEY_UP, "<F4>");
  ns_desktop_wait_sync(6);
  ns_desktop_key_type(KEY_PRESS_AND_RELEASE, "<Return>");
  //1st: RDP log out
  /***********************************************************************************/
 /* ns_desktop_key_type(KEY_DOWN, "<Control_L>");
  ns_desktop_wait_sync(1);

  ns_desktop_key_type(KEY_DOWN, "<Escape>");
  ns_desktop_wait_sync(1);
 
  ns_desktop_key_type(KEY_UP, "<Control_L>");
  ns_desktop_key_type(KEY_UP, "<Escape>");
  ns_desktop_wait_sync(1);
 

  ns_desktop_key_type(KEY_PRESS_AND_RELEASE, "<Tab>");
  ns_desktop_wait_sync(1);
 
  ns_desktop_key_type(KEY_PRESS_AND_RELEASE, "<Down>");
  ns_desktop_wait_sync(1);
 
  ns_desktop_key_type(KEY_PRESS_AND_RELEASE, "<Return>");
  ns_desktop_wait_sync(2);
 
  ns_desktop_key_type(KEY_PRESS_AND_RELEASE, "<Down>");
  ns_desktop_wait_sync(1);
 
  ns_desktop_key_type(KEY_PRESS_AND_RELEASE, "<Down>");
  ns_desktop_wait_sync(1);
 
  ns_desktop_key_type(KEY_PRESS_AND_RELEASE, "<Return>");
  ns_desktop_wait_sync(4);*/

  /***********************************************************************************/
  ns_desktop_wait_sync(2);


  //2nd: Close Rmeote Desktop Client
  /***********************************************************************************/
  //ns_desktop_key_type(KEY_DOWN, "<Alt_L>");
  //ns_desktop_key_type(KEY_DOWN, "<F4>");
  //ns_desktop_wait_sync(1);
  //ns_desktop_key_type(KEY_UP, "<Alt_L>");
  //ns_desktop_key_type(KEY_UP, "<F4>");
  //ns_desktop_wait_sync(1);
  /***********************************************************************************/

  if(ns_desktop_close() != RDP_SUCCESS)
  {
     NSDL1_RTE(vptr, NULL, "ns_desktop_close failed!!!!");
     NSTL1(vptr, NULL, "ns_desktop_close failed!!!!");
     return  RDP_ERROR;
  }
  NSDL2_RTE(vptr,NULL, "returning");
  return RDP_SUCCESS;
}


int nsi_rdp_key(int action, char *input)
{
   char key_str[256];
   sprintf(key_str, "<%s>", input);
   NSDL2_RTE(NULL,NULL, "key_str=%d", key_str);
   return ns_desktop_key_type(action, key_str);
}


int nsi_rdp_type(int action_type, char *input)
{ 
   return ns_desktop_key_type(action_type, input);     
}                                    
   
//ns_rdp_sync_on_mouse_click(int msec)


int nsi_rdp_mouse_move(int x, int y, int origin)
{
  return ns_desktop_mouse_move(origin, x, y);
}

int nsi_rdp_mouse_double_click(int mouseX, int mouseY, int button_type, int origin)
{
   if(RDP_ERROR == ns_desktop_mouse_move(origin, mouseX, mouseY))
     return RDP_ERROR;
   
   if(RDP_ERROR == ns_desktop_wait_sync(1))
     return RDP_ERROR;

   return ns_desktop_mouse_double_click(button_type);
}


int nsi_rdp_mouse_click(int mouseX, int mouseY, int button_type, int origin)
{
   if(RDP_ERROR == ns_desktop_mouse_move(origin, mouseX, mouseY))
     return RDP_ERROR;
   
   if(RDP_ERROR == ns_desktop_wait_sync(2))
     return RDP_ERROR;
   
   return ns_desktop_mouse_click(button_type);
}

int nsi_rdp_sync(int sec)
{
  return ns_desktop_wait_sync(sec);
}


int nsi_rdp_execute(VUser *vptr)
{
   TLS_SET_VPTR(vptr)
  //VUser *vptr = TLS_GET_VPTR();
  NSDL2_RTE(vptr,NULL, "Method called. vptr[%p]->first_page_url = %p page_id = %d vptr->xwp = %p", vptr, vptr->first_page_url, vptr->next_pg_id, vptr->xwp);
  action_request_Shr* request =   &request_table_shr_mem[vptr->next_pg_id]; /*vptr->first_page_url*/;
  
  char buff1[NS_RDP_BUFF_SIZE]; //get value from segment requtes->rdp.connect.host
  char buff2[NS_RDP_BUFF_SIZE]; //get value from segment requtes->rdp.connect.user
  char buff3[NS_RDP_BUFF_SIZE]; //get value from segment requtes->rdp.connect.password
  char buff4[NS_RDP_BUFF_SIZE]; //get value from segment requtes->rdp.connect.domain
  
  NSDL2_RTE(vptr,NULL, "request[%p]->proto.rdp.operation = %d", request, request->proto.rdp.operation);
  switch(request->proto.rdp.operation)
  {
     case NS_RDP_CONNECT:
     NSDL2_RTE(vptr,NULL, "calling nsi_rdp_connect");
     ns_get_values_from_segments(vptr, NULL, &request->proto.rdp.connect.host, buff1, NS_RDP_BUFF_SIZE);
     ns_get_values_from_segments(vptr, NULL,  &request->proto.rdp.connect.user, buff2, NS_RDP_BUFF_SIZE);
     ns_get_values_from_segments(vptr, NULL, &request->proto.rdp.connect.password, buff3, NS_RDP_BUFF_SIZE);
     if(request->proto.rdp.connect.domain.seg_start)
       ns_get_values_from_segments(vptr, NULL,  &request->proto.rdp.connect.domain, buff4, NS_RDP_BUFF_SIZE);
     NSDL2_RTE(vptr,NULL, "host = %s user = %s pwd = %s domain = %s vptr[%p]->xwp = %p", /*host*/buff1, /*user*/buff2, /*pwd*/buff3, /*domain*/buff4, vptr, vptr->xwp);
     return (vptr->page_status = nsi_rdp_connect(/*host, user, pwd, domain*/buff1, buff2, buff3, buff4)); 
     break;

     case NS_RDP_DISCONNECT:
     NSDL2_RTE(vptr,NULL, "calling nsi_rdp_disconnect");
     return nsi_rdp_disconnect();

     case NS_KEY:
     ns_get_values_from_segments(vptr, NULL, &request->proto.rdp.key.key_value, buff1, NS_RDP_BUFF_SIZE);
     NSDL2_RTE(vptr,NULL, "calling nsi_rdp_key. key_value = %s vptr[%p]->xwp = %p", buff1, vptr, vptr->xwp);
     return nsi_rdp_key(KEY_PRESS_AND_RELEASE, buff1);

     case NS_KEY_DOWN:
     ns_get_values_from_segments(vptr, NULL, &request->proto.rdp.key_down.key_value, buff1, NS_RDP_BUFF_SIZE);
     NSDL2_RTE(vptr,NULL, "calling nsi_rdp_key. key_value = %s", buff1 );
     return nsi_rdp_key(KEY_DOWN, buff1);

     case NS_KEY_UP:
     ns_get_values_from_segments(vptr, NULL, &request->proto.rdp.key_up.key_value, buff1, NS_RDP_BUFF_SIZE);
     NSDL2_RTE(vptr,NULL, "calling nsi_rdp_key. key_value = %s", buff1);
     return nsi_rdp_key(KEY_UP, buff1);

     case NS_TYPE:
     ns_get_values_from_segments(vptr, NULL, &request->proto.rdp.type.key_value, buff1, NS_RDP_BUFF_SIZE);
     NSDL2_RTE(vptr,NULL, "calling nsi_rdp_type. key_value = %s", buff1);
     return nsi_rdp_type(KEY_PRESS_AND_RELEASE, buff1);

     case NS_MOUSE_DOWN:
     NSDL2_RTE(vptr,NULL, "calling nsi_rdp_mouse_click. x = %d y = %d button = %d origin = %d", request->proto.rdp.mouse_down.x_pos, request->proto.rdp.mouse_down.y_pos, request->proto.rdp.mouse_down.button_type, request->proto.rdp.mouse_down.origin);
     return nsi_rdp_mouse_click(request->proto.rdp.mouse_down.x_pos, request->proto.rdp.mouse_down.y_pos, request->proto.rdp.mouse_down.button_type, request->proto.rdp.mouse_down.origin);

     case NS_MOUSE_UP:
     NSDL2_RTE(vptr,NULL, "calling nsi_rdp_mouse_click. x = %d y = %d button = %d origin = %d", request->proto.rdp.mouse_up.x_pos, request->proto.rdp.mouse_up.y_pos, request->proto.rdp.mouse_up.button_type, request->proto.rdp.mouse_up.origin);
     return nsi_rdp_mouse_click(request->proto.rdp.mouse_up.x_pos, request->proto.rdp.mouse_up.y_pos, request->proto.rdp.mouse_up.button_type, request->proto.rdp.mouse_up.origin);

     case NS_MOUSE_CLICK:
     NSDL2_RTE(vptr,NULL, "calling nsi_rdp_mouse_click. x = %d y = %d button = %d origin = %d", request->proto.rdp.mouse_click.x_pos, request->proto.rdp.mouse_click.y_pos, request->proto.rdp.mouse_click.button_type, request->proto.rdp.mouse_click.origin);
     return nsi_rdp_mouse_click(request->proto.rdp.mouse_click.x_pos, request->proto.rdp.mouse_click.y_pos, request->proto.rdp.mouse_click.button_type, request->proto.rdp.mouse_click.origin);

     case NS_MOUSE_DOUBLE_CLICK:
     NSDL2_RTE(vptr,NULL, "calling nsi_rdp_mouse_double_click. x = %d y = %d button = %d origin = %d", request->proto.rdp.mouse_double_click.x_pos, request->proto.rdp.mouse_double_click.y_pos, request->proto.rdp.mouse_double_click.button_type, request->proto.rdp.mouse_double_click.origin);
     return nsi_rdp_mouse_double_click(request->proto.rdp.mouse_double_click.x_pos, request->proto.rdp.mouse_double_click.y_pos, request->proto.rdp.mouse_double_click.button_type, request->proto.rdp.mouse_double_click.origin);

     case NS_MOUSE_MOVE:
     NSDL2_RTE(vptr,NULL, "calling nsi_rdp_mouse_move. x = %d y = %d origin = %d", request->proto.rdp.mouse_move.x_pos, request->proto.rdp.mouse_move.y_pos, request->proto.rdp.mouse_move.origin);
     return nsi_rdp_mouse_move(request->proto.rdp.mouse_move.x_pos, request->proto.rdp.mouse_move.y_pos, request->proto.rdp.mouse_move.origin);

     /*case NS_MOUSE_DRAG:
     return nsi_rdp_mouse_click(request->proto.rdp.mouse_drag.x_pos, request->proto.rdp.mouse_drag.y_pos, request->proto.rdp.mouse_drag.x1_pos, request->proto.rdp.mouse_drag.y1_pos, request->proto.rdp.mouse_drag.button_type, request->proto.rdp.mouse_drag.origin);*/

     case NS_SYNC:
     NSDL2_RTE(vptr,NULL, "calling nsi_rdp_sync. timeout = %d", request->proto.rdp.sync.timeout);
     return nsi_rdp_sync(request->proto.rdp.sync.timeout);

  }
  return RDP_ERROR;
}


void nsi_rdp_execute_ex(VUser* vptr)
{
  //VUser *vptr = TLS_GET_VPTR();
  NSDL2_RDP(vptr, NULL, "Method called. vptr = %p page_id = %d  vptr->xwp = %p", vptr, vptr->next_pg_id, vptr->xwp);
  char buff[250];
  vptr->rdp_status = nsi_rdp_execute(vptr);
  NSDL2_RDP(vptr, NULL, "runprof_table_shr_mem[vptr->group_num].gset.script_mode = %d vptr->page_status=%d", runprof_table_shr_mem[vptr->group_num].gset.script_mode, vptr->page_status);
  if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT) {
    switch(vptr->rdp_status/*(vptr->rdp_status = nsi_rdp_execute(vptr))*/)
    {
      case RDP_SUCCESS:
      sprintf(buff, "%s", "RDP Success");
      break;

      default:
      sprintf(buff,"%s", "RDP Failed");
      break;
    }
    NSDL2_RDP(vptr, NULL,"buff = %s vptr[%p]->xwp = %p", buff, vptr, vptr->xwp);
    switch_to_vuser_ctx(vptr, buff);
  }
}

//ToDo: move to utils.c common file for usage from RDP as well
int ns_get_values_from_segments(VUser* vptr, connection *cptr, StrEnt_Shr* seg_tab_ptr, char *buffer, int buf_size)
{
  NSDL2_RDP(vptr, cptr, "Method Called");
  int ret;
  action_request_Shr *url_num;
  if(!cptr)
    url_num = vptr->first_page_url;
  else
   url_num = cptr->url_num;
  NS_RESET_IOVEC(g_scratch_io_vector)
  // Get all segment values in a vector
  // Note that some segment may be parameterized
  if ((ret = insert_segments(vptr, cptr, seg_tab_ptr, &g_scratch_io_vector, NULL, 0, 1, 1,url_num, SEG_IS_NOT_REPEAT_BLOCK)) < 0)
  {
     NSDL2_RDP(NULL, NULL, ERROR_ID, ERROR_ATTR, "Error in insert_segments()");
     if(ret == -2)
       return ret;
     return(-1);
  }
  int filled_len=0;
  int avail_size;
  for(int i = 0; i < g_scratch_io_vector.cur_idx; i++) {
    avail_size = buf_size - filled_len;
    if (g_scratch_io_vector.vector[i].iov_len < avail_size)
    {
      strncpy(buffer + filled_len, g_scratch_io_vector.vector[i].iov_base, g_scratch_io_vector.vector[i].iov_len);
      filled_len += g_scratch_io_vector.vector[i].iov_len;
    }
    else
    {
       strncpy(buffer + filled_len, g_scratch_io_vector.vector[i].iov_base, avail_size);
       filled_len += avail_size - 1;
       break;
    }
  }
  buffer[filled_len] = 0;
  NS_FREE_RESET_IOVEC(g_scratch_io_vector);
  NSDL2_RDP(vptr, cptr, "segment value = [%s]", buffer);
  return  filled_len;
}

int ns_parse_rdp_disconnect_api(FILE *flow_fp, FILE *outfp,  char *flow_filename, char *flowout_filename, int sess_idx, int api_type)
{
  int url_idx;
  NSDL2_RDP(NULL, NULL, "Method Called, sess_idx = %d api_type = %d", sess_idx, api_type);

  char *page_end_ptr;
  
  if((page_end_ptr = strchr(script_line, '"')))
     SCRIPT_PARSE_ERROR(script_line, "Arguments Not required");

  create_requests_table_entry(&url_idx); // Fill request type inside create table entry

  rdp_init(url_idx, RDP_REQUEST, api_type);

  char pagename[RDP_MAX_ATTR_LEN + 1];

  NSDL3_RDP(NULL, NULL, "line = %s, api = %s", script_line, rdp_api_arr[NS_RDP_DISCONNECT]);
  sprintf(pagename, "%s", rdp_api_arr[NS_RDP_DISCONNECT]);
  if((parse_and_set_pagename(rdp_api_arr[NS_RDP_DISCONNECT], "ns_rdp", flow_fp, flow_filename, script_line, outfp, flowout_filename, sess_idx, &page_end_ptr, pagename)) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;
  NSDL2_RDP(NULL, NULL, "Exiting Method");
  return NS_PARSE_SCRIPT_SUCCESS;
}


int ns_parse_rdp_connection_apis(FILE *flow_fp, FILE *outfp,  char *flow_filename, char *flowout_filename, int sess_idx, int api_type)
{
  char attribute_name[128 + 1];
  char attribute_value[RDP_MAX_ATTR_LEN + 1];
  int url_idx;
  int id_flag = 0;
  int host_flag, user_flag, pwd_flag, domain_flag;
  host_flag = user_flag = pwd_flag = domain_flag = -1;

  NSDL2_RDP(NULL, NULL, "Method Called, sess_idx = %d api_type = %d", sess_idx, api_type);

  char *page_end_ptr;
  if(!(page_end_ptr = strchr(script_line, '"')))
     SCRIPT_PARSE_ERROR(script_line, "Argument must be provied in double quotes");
  char *close_quotes = page_end_ptr;
  char *start_quotes = page_end_ptr;

  create_requests_table_entry(&url_idx); // Fill request type inside create table entry

  rdp_init(url_idx, RDP_REQUEST, api_type);

  char pagename[RDP_MAX_ATTR_LEN + 1];
  //ToDo: What is some one prvided argument with ns_rdp_disconnect() API???
 
  // Process other attribute one by one
  while(1)
  {
     NSDL3_RDP(NULL, NULL, "line = %s, start_quotes = %s", script_line, start_quotes);
     if(get_next_argument(flow_fp, start_quotes, attribute_name, attribute_value, &close_quotes, 0) == NS_PARSE_SCRIPT_ERROR)
       return NS_PARSE_SCRIPT_ERROR;

    /* if(!strcasecmp(attribute_name, "ID"))
     {
        NSDL2_RDP(NULL, NULL, "ID =  [%s] ", attribute_value);

        if(strlen(attribute_value) > MAX_ID_LEN)
         SCRIPT_PARSE_ERROR(script_line, "ID value can be max of 32 character long");

        //Check for special characters found in ID value
        if(strchr(attribute_value, '@') || strchr(attribute_value, ':') || strchr(attribute_value, '|'))
          SCRIPT_PARSE_ERROR(script_line, "ID cannot have @, :, | character in it. Provide ID value without this special characters");

        if(id_flag != -1)
        {
          if(id_flag == g_cur_page)
            SCRIPT_PARSE_ERROR(script_line, "Can not have more than one ID parameter");
        }

        int is_new_id_flag;
        //ID can be alphanumeric, so getting norm id for the ID
        int norm_id;
        // = nslb_get_or_gen_norm_id(&(g_rdp_vars.key), attribute_value, strlen(attribute_value), &is_new_id_flag);
        if((norm_id = nslb_get_norm_id(&(g_rdp_vars.key), attribute_value, strlen(attribute_value))) >= 0) {
          NSDL2_RDP(NULL, NULL, "norm_id =  [%d ", norm_id);
          SCRIPT_PARSE_ERROR(script_line, "ID should be unique in script");
        }
        if((norm_id = nslb_get_or_set_norm_id(&(g_rdp_vars.key), attribute_value, strlen(attribute_value), &is_new_id_flag)) < 0) {
          NSDL2_RDP(NULL, NULL, "norm_id =  [%d ", norm_id);
          return NS_PARSE_SCRIPT_ERROR;
        }

        char pagename[RDP_MAX_ATTR_LEN + 1];
        sprintf(pagename, "RdpConnect_%s", attribute_value);

        if((parse_and_set_pagename("ns_rdp_connect", "ns_rdp", flow_fp, flow_filename,
                    script_line, outfp, flowout_filename, sess_idx, &page_end_ptr, pagename)) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;

        id_flag = g_cur_page;

        //Setting first_eurl for current_page
        gPageTable[g_cur_page].first_eurl = url_idx;
        requests[url_idx].proto.rdp.norm_id = norm_id;

        if(norm_id == g_rdp_vars.max_api_id_entries)
        {
          MY_REALLOC(g_rdp_vars.proto_norm_id_mapping_2_action_tbl, (g_rdp_vars.max_api_id_entries + DELTA_API_ID_ENTRIES) * sizeof(int),
                         "RDP API ID's", -1);
          g_rdp_vars.max_api_id_entries += DELTA_API_ID_ENTRIES;
        }
        g_rdp_vars.proto_norm_id_mapping_2_action_tbl[norm_id] = url_idx;

        NSDL2_RDP(NULL, NULL, "id_flag = %d max_rdp_conn = %d, norm_id = %d",  id_flag, g_rdp_vars.max_rdp_conn, norm_id);
        g_rdp_vars.max_rdp_conn++;
    }
    else*/ if(!strcasecmp(attribute_name, "Host"))
    {
      NSDL2_RDP(NULL, NULL, "Host =  [%s] ", attribute_value);
      sprintf(pagename, "RdpConnect_%s", attribute_value);
      if((parse_and_set_pagename("ns_rdp_connect", "ns_rdp", flow_fp, flow_filename,
                 script_line, outfp, flowout_filename, sess_idx, &page_end_ptr, pagename)) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;

      segment_line_ex(&(requests[url_idx].proto.rdp.connect.host), attribute_value, script_ln_no, sess_idx, flow_filename, id_flag, &host_flag);
      NSDL2_RDP(NULL, NULL, "url_idx = %d, attribute_value = %s", url_idx, attribute_value);
    }
    else if(!strcasecmp(attribute_name, "UserName"))
    {
      NSDL2_RDP(NULL, NULL, "UserName =  [%s] ", attribute_value);
      segment_line_ex(&(requests[url_idx].proto.rdp.connect.user), attribute_value, script_ln_no, sess_idx, flow_filename, id_flag, &user_flag);
      NSDL2_RDP(NULL, NULL, "url_idx = %d, attribute_value = %s", url_idx, attribute_value);
    }

    else if(!strcasecmp(attribute_name, "Password"))
    {
      NSDL2_RDP(NULL, NULL, "Pwd =  [%s] ", attribute_value);
      //ToDo: check for encrypted password
      segment_line_ex(&(requests[url_idx].proto.rdp.connect.password), attribute_value, script_ln_no, sess_idx, flow_filename, id_flag, &pwd_flag);
      NSDL2_RDP(NULL, NULL, "url_idx = %d, attribute_value = %s", url_idx, attribute_value);
    }
    else if(!strcasecmp(attribute_name, "Domain"))
    {
      NSDL2_RDP(NULL, NULL, "Domain =  [%s] ", attribute_value);
      segment_line_ex(&(requests[url_idx].proto.rdp.connect.domain), attribute_value, script_ln_no, sess_idx, flow_filename, id_flag, &domain_flag);
      NSDL2_RDP(NULL, NULL, "url_idx = %d, attribute_value = %s", url_idx, attribute_value);
    }
    else
    {
      SCRIPT_PARSE_ERROR(script_line, "Unknown argument found [%s] for ns_rdp_connect API", attribute_name);
    }

    int ret = read_till_start_of_next_quotes(flow_fp, flow_filename, close_quotes, &start_quotes, 0, outfp);
    if (ret == NS_PARSE_SCRIPT_ERROR)
    {
      NSDL2_RDP(NULL, NULL, "Next attribute is not found");
      break;
    }

  }

  if((host_flag == -1) || (user_flag == -1) || (pwd_flag == -1))
     SCRIPT_PARSE_ERROR(script_line, "Host, UserName AND Password are MUST provided, inorder to make RDP connection");


  NSDL2_RDP(NULL, NULL, "Exiting Method, protocol_enabled = 0x%x", global_settings->protocol_enabled);
  return NS_PARSE_SCRIPT_SUCCESS;
}

/*int ns_parse_key_apis(FILE *flow_fp, FILE *outfp,  char *flow_filename, char *flowout_filename, int sess_idx, int api_type)
{
  char attribute_name[128 + 1];
  char attribute_value[RDP_MAX_ATTR_LEN + 1];
  int url_idx;
  int id_flag = 0;
  int host_flag, user_flag, pwd_flag, domain_flag;
  host_flag = user_flag = pwd_flag = domain_flag = -1;

  NSDL2_RDP(NULL, NULL, "Method Called, sess_idx = %d api_type = %d", sess_idx, api_type);

  char *page_end_ptr = strchr(script_line, '"');

  char *close_quotes = page_end_ptr;
  char *start_quotes = page_end_ptr;

  create_requests_table_entry(&url_idx); // Fill request type inside create table entry

  rdp_init(url_idx, RDP_REQUEST, api_type);

  if(RDP_DISCONNECT == api_type)
    return NS_PARSE_SCRIPT_SUCCESS;

  // Process other attribute one by one
  while(1)
  {
     NSDL3_RDP(NULL, NULL, "line = %s, start_quotes = %s", script_line, start_quotes);
     if(get_next_argument(flow_fp, start_quotes, attribute_name, attribute_value, &close_quotes, 0) == NS_PARSE_SCRIPT_ERROR)
       return NS_PARSE_SCRIPT_ERROR;*/

    /* if(!strcasecmp(attribute_name, "ID"))
     {
        NSDL2_RDP(NULL, NULL, "ID =  [%s] ", attribute_value);

        if(strlen(attribute_value) > MAX_ID_LEN)
         SCRIPT_PARSE_ERROR(script_line, "ID value can be max of 32 character long");

        //Check for special characters found in ID value
        if(strchr(attribute_value, '@') || strchr(attribute_value, ':') || strchr(attribute_value, '|'))
          SCRIPT_PARSE_ERROR(script_line, "ID cannot have @, :, | character in it. Provide ID value without this special characters");

        if(id_flag != -1)
        {
          if(id_flag == g_cur_page)
            SCRIPT_PARSE_ERROR(script_line, "Can not have more than one ID parameter");
        }

        int is_new_id_flag;
        //ID can be alphanumeric, so getting norm id for the ID
        int norm_id;
        // = nslb_get_or_gen_norm_id(&(g_rdp_vars.key), attribute_value, strlen(attribute_value), &is_new_id_flag);
        if((norm_id = nslb_get_norm_id(&(g_rdp_vars.key), attribute_value, strlen(attribute_value))) >= 0) {
          NSDL2_RDP(NULL, NULL, "norm_id =  [%d ", norm_id);
          SCRIPT_PARSE_ERROR(script_line, "ID should be unique in script");
        }
        if((norm_id = nslb_get_or_set_norm_id(&(g_rdp_vars.key), attribute_value, strlen(attribute_value), &is_new_id_flag)) < 0) {
          NSDL2_RDP(NULL, NULL, "norm_id =  [%d ", norm_id);
          return NS_PARSE_SCRIPT_ERROR;
        }

        char pagename[RDP_MAX_ATTR_LEN + 1];
        sprintf(pagename, "RdpConnect_%s", attribute_value);

        if((parse_and_set_pagename("ns_rdp_connect", "ns_rdp", flow_fp, flow_filename,
                    script_line, outfp, flowout_filename, sess_idx, &page_end_ptr, pagename)) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;

        id_flag = g_cur_page;

        //Setting first_eurl for current_page
        gPageTable[g_cur_page].first_eurl = url_idx;
        requests[url_idx].proto.rdp.norm_id = norm_id;

        if(norm_id == g_rdp_vars.max_api_id_entries)
        {
          MY_REALLOC(g_rdp_vars.proto_norm_id_mapping_2_action_tbl, (g_rdp_vars.max_api_id_entries + DELTA_API_ID_ENTRIES) * sizeof(int),
                         "RDP API ID's", -1);
          g_rdp_vars.max_api_id_entries += DELTA_API_ID_ENTRIES;
        }
        g_rdp_vars.proto_norm_id_mapping_2_action_tbl[norm_id] = url_idx;

        NSDL2_RDP(NULL, NULL, "id_flag = %d max_rdp_conn = %d, norm_id = %d",  id_flag, g_rdp_vars.max_rdp_conn, norm_id);
        g_rdp_vars.max_rdp_conn++;
    }
    else*/ /*if(!strcasecmp(attribute_name, "Host"))
    {
      NSDL2_RDP(NULL, NULL, "Host =  [%s] ", attribute_value);
      char pagename[RDP_MAX_ATTR_LEN + 1];
      sprintf(pagename, "RdpConnect_%s", attribute_value);
      if((parse_and_set_pagename("ns_rdp_connect", "ns_rdp", flow_fp, flow_filename,
                 script_line, outfp, flowout_filename, sess_idx, &page_end_ptr, pagename)) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;

      segment_line_ex(&(requests[url_idx].proto.rdp.connect.host), attribute_value, script_ln_no, sess_idx, flow_filename, id_flag, &host_flag);
      NSDL2_RDP(NULL, NULL, "url_idx = %d, attribute_value = %s", url_idx, attribute_value);
    }
    else if(!strcasecmp(attribute_name, "UserName"))
    {
      NSDL2_RDP(NULL, NULL, "UserName =  [%s] ", attribute_value);
      segment_line_ex(&(requests[url_idx].proto.rdp.connect.user), attribute_value, script_ln_no, sess_idx, flow_filename, id_flag, &user_flag);
      NSDL2_RDP(NULL, NULL, "url_idx = %d, attribute_value = %s", url_idx, attribute_value);
    }

    else if(!strcasecmp(attribute_name, "Password"))
    {
      NSDL2_RDP(NULL, NULL, "Pwd =  [%s] ", attribute_value);
      //ToDo: check for encrypted password
      segment_line_ex(&(requests[url_idx].proto.rdp.connect.password), attribute_value, script_ln_no, sess_idx, flow_filename, id_flag, &pwd_flag);
      NSDL2_RDP(NULL, NULL, "url_idx = %d, attribute_value = %s", url_idx, attribute_value);
    }
    else if(!strcasecmp(attribute_name, "Domain"))
    {
      NSDL2_RDP(NULL, NULL, "Domain =  [%s] ", attribute_value);
      segment_line_ex(&(requests[url_idx].proto.rdp.connect.domain), attribute_value, script_ln_no, sess_idx, flow_filename, id_flag, &domain_flag);
      NSDL2_RDP(NULL, NULL, "url_idx = %d, attribute_value = %s", url_idx, attribute_value);
    }
    else
    {
      SCRIPT_PARSE_ERROR(script_line, "Unknown argument found [%s] for ns_rdp_connect API", attribute_name);
    }

    int ret = read_till_start_of_next_quotes(flow_fp, flow_filename, close_quotes, &start_quotes, 0, outfp);
    if (ret == NS_PARSE_SCRIPT_ERROR)
    {
      NSDL2_RDP(NULL, NULL, "Next attribute is not found");
      break;
    }

  }

  if(!host_flag || !user_flag || !pwd_flag)
     SCRIPT_PARSE_ERROR(script_line, "Host, UserName and Password are MUST provided, inorder to make RDP connection");


  NSDL2_RDP(NULL, NULL, "Exiting Method, protocol_enabled = 0x%x", global_settings->protocol_enabled);
  return NS_PARSE_SCRIPT_SUCCESS;
}


int ns_parse_mouse_apis(FILE *flow_fp, FILE *outfp,  char *flow_filename, char *flowout_filename, int sess_idx, int api_type)
{
  char attribute_name[128 + 1];
  char attribute_value[RDP_MAX_ATTR_LEN + 1];
  int url_idx;
  int id_flag = 0;
  int host_flag, user_flag, pwd_flag, domain_flag;
  host_flag = user_flag = pwd_flag = domain_flag = -1;

  NSDL2_RDP(NULL, NULL, "Method Called, sess_idx = %d api_type = %d", sess_idx, api_type);

  char *page_end_ptr = strchr(script_line, '"');

  char *close_quotes = page_end_ptr;
  char *start_quotes = page_end_ptr;

  create_requests_table_entry(&url_idx); // Fill request type inside create table entry

  rdp_init(url_idx, RDP_REQUEST, api_type);

  if(RDP_DISCONNECT == api_type)
    return NS_PARSE_SCRIPT_SUCCESS;

  // Process other attribute one by one
  while(1)
  {
     NSDL3_RDP(NULL, NULL, "line = %s, start_quotes = %s", script_line, start_quotes);
     if(get_next_argument(flow_fp, start_quotes, attribute_name, attribute_value, &close_quotes, 0) == NS_PARSE_SCRIPT_ERROR)
       return NS_PARSE_SCRIPT_ERROR;
*/
    /* if(!strcasecmp(attribute_name, "ID"))
     {
        NSDL2_RDP(NULL, NULL, "ID =  [%s] ", attribute_value);

        if(strlen(attribute_value) > MAX_ID_LEN)
         SCRIPT_PARSE_ERROR(script_line, "ID value can be max of 32 character long");

        //Check for special characters found in ID value
        if(strchr(attribute_value, '@') || strchr(attribute_value, ':') || strchr(attribute_value, '|'))
          SCRIPT_PARSE_ERROR(script_line, "ID cannot have @, :, | character in it. Provide ID value without this special characters");

        if(id_flag != -1)
        {
          if(id_flag == g_cur_page)
            SCRIPT_PARSE_ERROR(script_line, "Can not have more than one ID parameter");
        }

        int is_new_id_flag;
        //ID can be alphanumeric, so getting norm id for the ID
        int norm_id;
        // = nslb_get_or_gen_norm_id(&(g_rdp_vars.key), attribute_value, strlen(attribute_value), &is_new_id_flag);
        if((norm_id = nslb_get_norm_id(&(g_rdp_vars.key), attribute_value, strlen(attribute_value))) >= 0) {
          NSDL2_RDP(NULL, NULL, "norm_id =  [%d ", norm_id);
          SCRIPT_PARSE_ERROR(script_line, "ID should be unique in script");
        }
        if((norm_id = nslb_get_or_set_norm_id(&(g_rdp_vars.key), attribute_value, strlen(attribute_value), &is_new_id_flag)) < 0) {
          NSDL2_RDP(NULL, NULL, "norm_id =  [%d ", norm_id);
          return NS_PARSE_SCRIPT_ERROR;
        }

        char pagename[RDP_MAX_ATTR_LEN + 1];
        sprintf(pagename, "RdpConnect_%s", attribute_value);

        if((parse_and_set_pagename("ns_rdp_connect", "ns_rdp", flow_fp, flow_filename,
                    script_line, outfp, flowout_filename, sess_idx, &page_end_ptr, pagename)) == NS_PARSE_SCRIPT_ERROR)
        return NS_PARSE_SCRIPT_ERROR;

        id_flag = g_cur_page;

        //Setting first_eurl for current_page
        gPageTable[g_cur_page].first_eurl = url_idx;
        requests[url_idx].proto.rdp.norm_id = norm_id;

        if(norm_id == g_rdp_vars.max_api_id_entries)
        {
          MY_REALLOC(g_rdp_vars.proto_norm_id_mapping_2_action_tbl, (g_rdp_vars.max_api_id_entries + DELTA_API_ID_ENTRIES) * sizeof(int),
                         "RDP API ID's", -1);
          g_rdp_vars.max_api_id_entries += DELTA_API_ID_ENTRIES;
        }
        g_rdp_vars.proto_norm_id_mapping_2_action_tbl[norm_id] = url_idx;

        NSDL2_RDP(NULL, NULL, "id_flag = %d max_rdp_conn = %d, norm_id = %d",  id_flag, g_rdp_vars.max_rdp_conn, norm_id);
        g_rdp_vars.max_rdp_conn++;
    }
    else*/  /*if(!strcasecmp(attribute_name, "Host"))
    {
      NSDL2_RDP(NULL, NULL, "Host =  [%s] ", attribute_value);
      char pagename[RDP_MAX_ATTR_LEN + 1];
      sprintf(pagename, "RdpConnect_%s", attribute_value);
      if((parse_and_set_pagename("ns_rdp_connect", "ns_rdp", flow_fp, flow_filename,
                 script_line, outfp, flowout_filename, sess_idx, &page_end_ptr, pagename)) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;

      segment_line_ex(&(requests[url_idx].proto.rdp.connect.host), attribute_value, script_ln_no, sess_idx, flow_filename, id_flag, &host_flag);
      NSDL2_RDP(NULL, NULL, "url_idx = %d, attribute_value = %s", url_idx, attribute_value);
    }
    else if(!strcasecmp(attribute_name, "UserName"))
    {
      NSDL2_RDP(NULL, NULL, "UserName =  [%s] ", attribute_value);
      segment_line_ex(&(requests[url_idx].proto.rdp.connect.user), attribute_value, script_ln_no, sess_idx, flow_filename, id_flag, &user_flag);
      NSDL2_RDP(NULL, NULL, "url_idx = %d, attribute_value = %s", url_idx, attribute_value);
    }

    else if(!strcasecmp(attribute_name, "Password"))
    {
      NSDL2_RDP(NULL, NULL, "Pwd =  [%s] ", attribute_value);
      //ToDo: check for encrypted password
      segment_line_ex(&(requests[url_idx].proto.rdp.connect.password), attribute_value, script_ln_no, sess_idx, flow_filename, id_flag, &pwd_flag);
      NSDL2_RDP(NULL, NULL, "url_idx = %d, attribute_value = %s", url_idx, attribute_value);
    }
    else if(!strcasecmp(attribute_name, "Domain"))
    {
      NSDL2_RDP(NULL, NULL, "Domain =  [%s] ", attribute_value);
      segment_line_ex(&(requests[url_idx].proto.rdp.connect.domain), attribute_value, script_ln_no, sess_idx, flow_filename, id_flag, &domain_flag);
      NSDL2_RDP(NULL, NULL, "url_idx = %d, attribute_value = %s", url_idx, attribute_value);
    }
    else
    {
      SCRIPT_PARSE_ERROR(script_line, "Unknown argument found [%s] for ns_rdp_connect API", attribute_name);
    }

    int ret = read_till_start_of_next_quotes(flow_fp, flow_filename, close_quotes, &start_quotes, 0, outfp);
    if (ret == NS_PARSE_SCRIPT_ERROR)
    {
      NSDL2_RDP(NULL, NULL, "Next attribute is not found");
      break;
    }

  }

  if(!host_flag || !user_flag || !pwd_flag)
     SCRIPT_PARSE_ERROR(script_line, "Host, UserName and Password are MUST provided, inorder to make RDP connection");


  NSDL2_RDP(NULL, NULL, "Exiting Method, protocol_enabled = 0x%x", global_settings->protocol_enabled);
  return NS_PARSE_SCRIPT_SUCCESS;
}*/


