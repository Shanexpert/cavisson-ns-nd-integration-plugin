/************************************************************************************
 * Name      : ns_desktop.c 
 * Purpose   : This file contains functions related to rte desktop protocol 
 * Author(s) : Devendra Jain/Anup Singh
 * Date      : 03 Nov 2020
 * Copyright : (c) Cavisson Systems
 * Modification History : .
 ***********************************************************************************/

#include "ns_desktop.h"
#include "ns_rdp_api.h"
#include "ns_tls_utils.h"
#include "ns_string.h"

#define MICRO_TO_SECOND	1000000

KeyApiCount gKeyApi;
MouseApiCount gMouseApi;

//StrEnt* key_segtable_arr[];
static void set_segment_value(char* line, int id_flag, int *curr_flag, int *out_seg_value)
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
  int value = atoi(line);
  if(value < 0)
    SCRIPT_PARSE_ERROR(script_line, "%s value can not be < 0 ", line);
  //ToDo: check for = before key or values
  *out_seg_value = value;
  NSDL2_RDP(NULL, NULL, "line = %s value = %d", line, *out_seg_value);
}

void key_init(int row_num, int proto, int api_type)
{
  NSDL2_MISC(NULL, NULL, "Method called. row_num = %d proto = %d api_type = %d", row_num, proto, api_type);
  requests[row_num].request_type = proto;
  requests[row_num].proto.rdp.operation = api_type;
  switch(api_type)
  {
    case NS_KEY:
    {
      requests[row_num].proto.rdp.key.key_value.seg_start = -1;
      requests[row_num].proto.rdp.key.key_value.num_entries= 0;
    }
    break;
 
    case NS_KEY_DOWN:
    {
      requests[row_num].proto.rdp.key_down.key_value.seg_start = -1;
      requests[row_num].proto.rdp.key_down.key_value.num_entries= 0;
    }
    break;

    case NS_KEY_UP:
    {
      requests[row_num].proto.rdp.key_up.key_value.seg_start = -1;
      requests[row_num].proto.rdp.key_up.key_value.num_entries= 0;
    }
    break;

    case NS_TYPE:
    {
      requests[row_num].proto.rdp.type.key_value.seg_start = -1;
      requests[row_num].proto.rdp.type.key_value.num_entries= 0;
    }
    break;

    default:
    break;
    //error handling
  }
}

void sync_init(int row_num, int proto, int api_type)
{
  NSDL2_MISC(NULL, NULL, "Method called. row_num = %d proto = %d api_type = %d", row_num, proto, api_type);
  requests[row_num].request_type = proto;
  requests[row_num].proto.rdp.operation = api_type;

  requests[row_num].proto.rdp.sync.timeout = 0;
} 
void mouse_init(int row_num, int proto, int api_type)
{
  NSDL2_MISC(NULL, NULL, "Method called. row_num = %d proto = %d api_type = %d", row_num, proto, api_type);
  requests[row_num].request_type = proto;
  requests[row_num].proto.rdp.operation = api_type;
  switch(api_type)
  {
    case NS_MOUSE_DOWN:
    {
      requests[row_num].proto.rdp.mouse_down.x_pos = -1;
      requests[row_num].proto.rdp.mouse_down.y_pos = -1;
      requests[row_num].proto.rdp.mouse_down.button_type = LEFT_BUTTON;
      requests[row_num].proto.rdp.mouse_down.origin = MOUSEMOVE_ABSOLUTE;
    }
    break;
 
    case NS_MOUSE_UP:
    {
      requests[row_num].proto.rdp.mouse_up.x_pos = -1;
      requests[row_num].proto.rdp.mouse_up.y_pos = -1;
      requests[row_num].proto.rdp.mouse_up.button_type = LEFT_BUTTON;
      requests[row_num].proto.rdp.mouse_up.origin = MOUSEMOVE_ABSOLUTE;
    }
    break;
    
    case NS_MOUSE_CLICK:
    {
      requests[row_num].proto.rdp.mouse_click.x_pos = -1;
      requests[row_num].proto.rdp.mouse_click.y_pos = -1;
      requests[row_num].proto.rdp.mouse_click.button_type = LEFT_BUTTON;
      requests[row_num].proto.rdp.mouse_click.origin = MOUSEMOVE_ABSOLUTE;
    }
    break;
  
    case NS_MOUSE_DOUBLE_CLICK:
    {
      requests[row_num].proto.rdp.mouse_double_click.x_pos = -1;
      requests[row_num].proto.rdp.mouse_double_click.y_pos = -1;
      requests[row_num].proto.rdp.mouse_double_click.button_type = LEFT_BUTTON;
      requests[row_num].proto.rdp.mouse_double_click.origin = MOUSEMOVE_ABSOLUTE;
    }
    break;
    
    case NS_MOUSE_MOVE:
    {
      requests[row_num].proto.rdp.mouse_move.x_pos = -1;
      requests[row_num].proto.rdp.mouse_move.y_pos = -1;
      requests[row_num].proto.rdp.mouse_move.button_type = LEFT_BUTTON;
      requests[row_num].proto.rdp.mouse_move.origin = MOUSEMOVE_ABSOLUTE;
    }
    break;

    case NS_MOUSE_DRAG:
    {
      requests[row_num].proto.rdp.mouse_drag.x_pos = -1;
      requests[row_num].proto.rdp.mouse_drag.y_pos = -1;
      requests[row_num].proto.rdp.mouse_drag.x1_pos = -1;
      requests[row_num].proto.rdp.mouse_drag.y1_pos = -1;
      requests[row_num].proto.rdp.mouse_drag.button_type = LEFT_BUTTON;
      requests[row_num].proto.rdp.mouse_drag.origin = MOUSEMOVE_RELATIVE;
    }
    break;


    default:
    break;
    //error handling
  }
}


static int ns_desktop_send(char *buffer)
{
  VUser *vptr = (VUser*)TLS_GET_VPTR();
  NSDL2_RTE(vptr, NULL,"Method called. buffer = %s vptr[%p]->xwp = %p", buffer, vptr, vptr->xwp);
  if(!vptr->xwp)
  {
     NSDL4_RTE(vptr, NULL, "vptr->xwp is NULL");
     NSTL1(vptr, NULL, "vptr->xwp is NULL");
     return DESKTOP_ERROR;
  }

  char buff[1024];
  sprintf(buff, "%s\n", buffer);
  NSDL2_RTE(vptr, NULL," now buffer to send = %s ", buff);
  if (fputs(buff, vptr->xwp) == EOF)
  {
    char err_buff[1024];
    sprintf(err_buff, "Error in Write fd = %p, buffer = %s", vptr->xwp, buffer);
    NSDL4_RTE(vptr, NULL, "%s", err_buff);
    NSTL1(vptr, NULL, "%s", err_buff);
    return DESKTOP_ERROR;
  }
  fflush(vptr->xwp);
  NSDL2_RTE(vptr, NULL,"Returning");
  return DESKTOP_SUCCESS; 
}
//
static int inline ns_desktop_run_rdp(char *cmd)
{

  NSDL2_RTE(NULL, NULL,"Method called. cmd = %s", cmd);
  FILE *fp;
  if(!(fp = nslb_popen(cmd, "r")))
     return DESKTOP_ERROR;
  //wait for 2 second
  sleep(2);
  char read_buf[1024 + 1];
  nslb_fgets(read_buf, 1024, fp, 0);
  pclose(fp);

  NSDL2_RTE(NULL, NULL,"xfreerdp proc id = %s", read_buf);
  int xfreerdp_pid;
  if(!(xfreerdp_pid = atoi(read_buf)))
     return DESKTOP_ERROR;
  
  NSDL2_RTE(NULL, NULL,"check xfreerdp proc availability");
 //check if process is running
  if(kill(xfreerdp_pid, 0) != 0)
    return DESKTOP_ERROR;   

  NSDL2_RTE(NULL, NULL,"returning xfreerdp_pid = %d", xfreerdp_pid);
  return xfreerdp_pid;
}

/*--------------------------------------------------------------------------------------------- 
 * Name        : ns_desktop_open 
 * Discription : This function will open a seperate process to execute xte  
 *
 * Input     :  NA 
 *
 * Output    : On error    DESKTOP_ERROR
 *             On success   0 
 *--------------------------------------------------------------------------------------------*/
int ns_desktop_open(char* cmd)
{
  VUser *vptr = (VUser*)TLS_GET_VPTR();
  NSDL2_RTE(vptr, NULL,"Method called. vptr[%p]->xwp= %p cmd = %s", vptr, vptr->xwp, cmd);

  char display[8];
  ns_advance_param("ns_rte_display");
  snprintf(display, 8, ":%s", ns_eval_string("{ns_rte_display}"));
  setenv("DISPLAY", display, 1);
  NSDL2_RTE(vptr, NULL,"display = %s", display);
  
  if(ns_desktop_run_rdp(cmd) <= 0)
  {
     NSTL1(vptr, NULL, "unable to run rdp command");
     NSDL2_RTE(vptr, NULL,"unable to run rdp command");
     return DESKTOP_ERROR;
  }
  char xte_cmd[50];
  sprintf(xte_cmd, "xte -x %s", display);
  if(NULL == (vptr->xwp = popen(xte_cmd, "w")))
  {
    char err_buff[1024];
    sprintf(err_buff, "Error during popen xte");
    NSDL4_RTE(vptr, NULL, "%s", err_buff);
    NSTL1(vptr, NULL, "%s", err_buff);
    return DESKTOP_ERROR;
  } 
  NSDL2_RTE(vptr, NULL,"Now vptr[%p]->xwp = %p", vptr, vptr->xwp);
  return DESKTOP_SUCCESS;
}

/*ns_desktop_close*/
int ns_desktop_close()
{
  VUser *vptr = (VUser*)TLS_GET_VPTR();
  NSDL3_RTE(vptr,NULL, "Method called. xwp = %p", vptr->xwp);
  if(vptr->xwp) {
   pclose(vptr->xwp);
   vptr->xwp = NULL;
  }
  return DESKTOP_SUCCESS;
}

/*ns_desktop_key_inputs_inputs*/
int ns_desktop_key_type(int type, char *input)
{
  VUser *vptr = (VUser*)TLS_GET_VPTR();
  char* key_action[] = {"key", "keydown", "keyup"}; 
  NSDL2_RTE(vptr, NULL, "Method called. input = %p vptr[%p]->xwp = %p", input, vptr, vptr->xwp);
  if(!input)
  {
    char err_buff[1024];
    sprintf(err_buff, "Error!!! Invalid I/P");
    NSDL4_RTE(vptr, NULL, "%s", err_buff);
    NSTL1(vptr, NULL, "%s", err_buff);
    return DESKTOP_ERROR;
  } 
  char cmd[MAX_RTE_CMD_LENGTH];
  char *textPtr;
  int key; 

  NSDL2_RTE(vptr, NULL, "input = %s", input);
  //Check for paramterers
  if(input[0] == '{')
  {
    textPtr = ns_eval_string(input);
  }
  else
  {
    textPtr = input;
  }
  NSDL4_RTE(vptr,NULL, "textPtr = %s", textPtr);
  char key_buff[32+1];
  char* last_ptr;
  int len;
  
  /*ToDo:  Handle mix mode example: "abc<Return>" **/
  while(*textPtr)
  {
    key = 0;
   
    //Check for <key>
    if (*textPtr == '<')
    {
      if((last_ptr = strchr(textPtr,'>')))
      {
        ++textPtr;
        len = (last_ptr - textPtr); 
        strncpy(key_buff, textPtr, len);
        NSDL2_RTE(vptr, NULL, "len = %d  key_buff = %s", len, key_buff);
        key_buff[len] = '\0';
        NSDL2_RTE(vptr, NULL, "now  key_buff = %s", key_buff);
        key = 1;
        textPtr = last_ptr + 1;
        NSDL2_RTE(vptr, NULL, "input = %s", input);
      }
    }
    //Input is string
    if(!key)
    {
      //sprintf(cmd,"str %c",*textPtr);
      sprintf(cmd,"str %s", textPtr);
      if (ns_desktop_send(cmd) < 0)
      {
        NSTL1(vptr,NULL,"ns_rte_x3270_write failed");
        return DESKTOP_ERROR;
      }
      break;
    }
    //Input is key
    else
    {
      NSDL2_RTE(vptr, NULL, "now  key_buff = %s", key_buff);
      sprintf(cmd,"%s %s", key_action[type], key_buff);
      if (ns_desktop_send(cmd) < 0)
      {
        NSTL1(vptr,NULL,"ns_rte_x3270_write failed");
        return DESKTOP_ERROR;
      }
      break;
    }
  }
  return DESKTOP_SUCCESS;
}


/*ns_desktop_sleep*/
int ns_desktop_wait_sync(int sec)
{
  

  VUser *vptr = (VUser*)TLS_GET_VPTR();
  RTE_THINK_TIME(vptr, NULL, sec*1000)
  //ToDo: UT Error or default 1 sec
  if(!sec)
    sec = 1;

  char cmd[MAX_RTE_CMD_LENGTH];
  NSDL3_RTE(vptr,NULL, "Method called. sec = %d", sec);
  //Wait to syncronize the input 
  sprintf(cmd,"usleep %d", sec*MICRO_TO_SECOND);
  NSDL3_RTE(vptr,NULL, "cmd = %s", cmd);
  if (ns_desktop_send(cmd) < 0)
  {
    NSTL1(vptr,NULL,"ns_desktop_send failed");
    return DESKTOP_ERROR;
  }
  return DESKTOP_SUCCESS;
}

/*ns_desktop_mouse_move*/
int ns_desktop_mouse_move(int type, int mouseX, int mouseY)
{
  char* mousemove_type[] = {"mousemove", "mousermove"};
  char cmd[MAX_RTE_CMD_LENGTH];
  VUser *vptr = (VUser*)TLS_GET_VPTR();
  NSDL3_RTE(vptr,NULL, "Method called, mousemove type = %d, mouseX = %d mouseY = %d", type, mouseX, mouseY);
  //Expect Command to wait for a text for given duration
  sprintf(cmd,"%s %d %d",  mousemove_type[type], mouseX, mouseY);
  if (ns_desktop_send(cmd) < 0)
  {
    NSTL1(vptr,NULL,"ns_rte_x3270_write failed");
    return DESKTOP_ERROR;
  }
  return DESKTOP_SUCCESS;
}


int ns_desktop_mouse_click(int type)
{
  char* click_type[] = {"mouseclick 1", "mousedown 1", "mouseup 1", "mouseclick 3", "mousedown 3", "mouseup 3"};
  VUser *vptr = (VUser*)TLS_GET_VPTR();
  NSDL3_RTE(vptr,NULL, "Method called, type  = %d", type);
  //Expect Command to wait for a text for given duration
  if (ns_desktop_send(click_type[type]) < 0)
  {
    NSTL1(vptr,NULL,"ns_desktop_mouse_click failed");
    return DESKTOP_ERROR;
  }
  return DESKTOP_SUCCESS;
}

int ns_desktop_mouse_double_click(int button_type)
{

  NSDL3_RTE(NULL,NULL, "Method called. button_type = %d", button_type);
  if(ns_desktop_mouse_click(button_type) != DESKTOP_SUCCESS)
    return DESKTOP_ERROR;
 
  if(ns_desktop_mouse_click(button_type) != DESKTOP_SUCCESS)
    return DESKTOP_ERROR;
   
  return ns_desktop_wait_sync(1);
}

/*void ns_parse_id()
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
}*/
int get_key_api_and_segtable(int url_idx, int api_type, StrEnt **segtable, char buff[]/*, int *out_key_type*/)
{
   NSDL3_RDP(NULL, NULL, "Method called. segtable = %p, buff = %p url_idx = %d api_type = %d",  segtable, buff, url_idx, api_type);
   switch(api_type)
   { 
      case NS_KEY:
      {
         sprintf(buff, "%s", rdp_api_arr[NS_KEY]);
         *segtable = &(requests[url_idx].proto.rdp.key.key_value);
         //*out_key_type = KEY_PRESS_AND_RELEASE;
         return gKeyApi.key++;
      }
      break;
 
      case NS_KEY_DOWN:
      {
         sprintf(buff, "%s", rdp_api_arr[NS_KEY_DOWN]);
         *segtable = &(requests[url_idx].proto.rdp.key_down.key_value);
         //*out_key_type = KEY_DOWN;
         return gKeyApi.down++;
      }
      break;

      case NS_KEY_UP:
      {
         sprintf(buff, "%s", rdp_api_arr[NS_KEY_UP]);
         *segtable = &(requests[url_idx].proto.rdp.key_up.key_value);
         //*out_key_type = KEY_UP;
         return gKeyApi.up++;
      }
      break;

      case NS_TYPE:
      {
         sprintf(buff, "%s", rdp_api_arr[NS_TYPE]);
         *segtable = &(requests[url_idx].proto.rdp.type.key_value);
         //*out_key_type = KEY_PRESS_AND_RELEASE;
         return gKeyApi.type++;
      }
      break;
    }
   NSDL3_RDP(NULL, NULL, "segtable = %p, buff = %s",  *segtable, buff);
   return -1;
}


int  get_mouse_action_and_api(int url_idx, int api_type, ns_mouse **out_mouse_action, char buff[])
{
   NSDL3_RDP(NULL, NULL, "Method called. mouse_type = %p, buff = %p url_idx = %d",  out_mouse_action, buff, url_idx);
 
   switch(api_type)
   { 
      case NS_MOUSE_DOWN:
      {
         sprintf(buff, "%s", rdp_api_arr[NS_MOUSE_DOWN]);
         *out_mouse_action = &(requests[url_idx].proto.rdp.mouse_down);
         return gMouseApi.down++;
      }
      break;
 
      case NS_MOUSE_UP:
      {
         sprintf(buff, "%s", rdp_api_arr[NS_MOUSE_UP]);
         *out_mouse_action = &(requests[url_idx].proto.rdp.mouse_up);
	 return gMouseApi.up++;
      }
      break;

      case NS_MOUSE_CLICK:
      {
         sprintf(buff, "%s", rdp_api_arr[NS_MOUSE_CLICK]);
         *out_mouse_action = &(requests[url_idx].proto.rdp.mouse_click);
	 return gMouseApi.click;
      }
      break;

      case NS_MOUSE_DOUBLE_CLICK:
      {
         sprintf(buff, "%s", rdp_api_arr[NS_MOUSE_DOUBLE_CLICK]);
         *out_mouse_action = &(requests[url_idx].proto.rdp.mouse_double_click);
	 return gMouseApi.double_click++;
      }
      break;

      case NS_MOUSE_MOVE:
      {
         sprintf(buff, "%s", rdp_api_arr[NS_MOUSE_MOVE]);
         *out_mouse_action = &(requests[url_idx].proto.rdp.mouse_move);
	 return gMouseApi.mouse_move++;
      }
      break;
      case NS_MOUSE_DRAG:
      {
         sprintf(buff, "%s", rdp_api_arr[NS_MOUSE_DRAG]);
         *out_mouse_action = &(requests[url_idx].proto.rdp.mouse_drag);
	 return gMouseApi.mouse_drag++;
      }
      break;
    }
    NSDL3_RDP(NULL, NULL, "buff = %s", buff);
    return -1;
}
 
int ns_parse_key_apis_ex(char *line, FILE *flow_fp, FILE *outfp,  char *flow_filename, char *flowout_filename, int sess_idx)
{
  CLEAR_WHITE_SPACE(line);
  CLEAR_WHITE_SPACE_FROM_END(line);
  char str[RDP_MAX_ATTR_LEN + 1];
  strcpy(str, line);
  str[strlen(line)] = '\0';
  NSDL2_RDP(NULL, NULL, "Method Called, line = %s str = %s", line, str);
  char* token = strtok(str, "("); 
  int api_type;
  NSDL2_RDP(NULL, NULL, "token = %s", token);
  //get api_type 
  if(!strcmp(token, NS_KEY_STR))
    api_type = NS_KEY;
  else if(!strcmp(token, NS_KEY_DOWN_STR))
    api_type = NS_KEY_DOWN;
  else if(!strcmp(token, NS_KEY_UP_STR))
    api_type = NS_KEY_UP;
  else
   return DESKTOP_ERROR;
  
  NSDL2_RDP(NULL, NULL, "token = %s api_type = %d", token, api_type);

  return ns_parse_key_apis(flow_fp, outfp, flow_filename, flowout_filename, sess_idx, api_type);
}

int ns_parse_key_apis(FILE *flow_fp, FILE *outfp,  char *flow_filename, char *flowout_filename, int sess_idx, int api_type)
{
  char attribute_name[128 + 1];
  char attribute_value[RDP_MAX_ATTR_LEN + 1];
  int url_idx;
  int id_flag = 0;
  int key_flag = -1;
  int str_flag = -1;

  NSDL2_RDP(NULL, NULL, "Method Called, sess_idx = %d api_type = %d script_line = %s", sess_idx, api_type, script_line);

  char *page_end_ptr;
  if(!(page_end_ptr = strchr(script_line, '"')))
     SCRIPT_PARSE_ERROR(script_line, "Argument must be provied in double quotes");

  char *close_quotes = page_end_ptr;
  char *start_quotes = page_end_ptr;

  create_requests_table_entry(&url_idx); // Fill request type inside create table entry

  NSDL2_RDP(NULL, NULL, "row num = %d api_type = %d", url_idx, api_type);
  key_init(url_idx, RDP_REQUEST, api_type);

  //int key_type;
  StrEnt* segtable;
  char buff[RDP_MAX_ATTR_LEN + 1];

  // Process other attribute one by one
  while(1)
  {
     NSDL3_RDP(NULL, NULL, "line = %s, start_quotes = %s", script_line, start_quotes);
     if(get_next_argument(flow_fp, start_quotes, attribute_name, attribute_value, &close_quotes, 0) == NS_PARSE_SCRIPT_ERROR)
       return NS_PARSE_SCRIPT_ERROR;

    /* if(!strcasecmp(attribute_name, "ID"))
     {
       ns_parse_id();
    }
    else*/ if(!strcasecmp(attribute_name, "Key"))
    {
      NSDL2_RDP(NULL, NULL, "Key =  [%s] ", attribute_value);
      if(key_flag != -1)
        SCRIPT_PARSE_ERROR(script_line, "Key should not be provided  more than once");
      int count = get_key_api_and_segtable(url_idx, api_type, &segtable, buff/*, &key_type*/);
      char pagename[RDP_MAX_ATTR_LEN + 1];
      sprintf(pagename, "%s_%s_%d", buff, attribute_value, count);
      NSDL2_RDP(NULL, NULL, "pagename =  [%s] ", pagename);
      if((parse_and_set_pagename(buff, "ns_rdp", flow_fp, flow_filename,
                 script_line, outfp, flowout_filename, sess_idx, &page_end_ptr, pagename)) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;
      segment_line_ex(segtable, attribute_value, script_ln_no, sess_idx, flow_filename, id_flag, &key_flag);
      NSDL2_RDP(NULL, NULL, "url_idx = %d, attribute_value = %s buff = %s", url_idx, attribute_value, buff);
    }
    else if(!strcasecmp(attribute_name, "String"))
    {
      NSDL2_RDP(NULL, NULL, "String =  [%s] ", attribute_value);
      if(str_flag != -1)
        SCRIPT_PARSE_ERROR(script_line, "String should not be provided  more than once");
      int count = get_key_api_and_segtable(url_idx, api_type, &segtable, buff/*, &key_type*/);
      char pagename[RDP_MAX_ATTR_LEN + 1];
      sprintf(pagename, "%s_%s_%d", buff, attribute_value, count);
      NSDL2_RDP(NULL, NULL, "pagename =  [%s] ", pagename);
      if((parse_and_set_pagename(buff, "ns_rdp", flow_fp, flow_filename,
                 script_line, outfp, flowout_filename, sess_idx, &page_end_ptr, pagename)) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;
      segment_line_ex(segtable, attribute_value, script_ln_no, sess_idx, flow_filename, id_flag, &str_flag);
      NSDL2_RDP(NULL, NULL, "url_idx = %d, attribute_value = %s buff = %s", url_idx, attribute_value, buff);
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

  if((key_flag == -1) && (str_flag == -1)) 
     SCRIPT_PARSE_ERROR(script_line, "KeyValue MUST be provided, inorder to perform any key action");

  NSDL2_RDP(NULL, NULL, "Exiting Method, protocol_enabled = 0x%x", global_settings->protocol_enabled);
  return NS_PARSE_SCRIPT_SUCCESS;
}


int ns_parse_mouse_apis_ex(char *line, FILE *flow_fp, FILE *outfp,  char *flow_filename, char *flowout_filename, int sess_idx)
{
  CLEAR_WHITE_SPACE(line);
  CLEAR_WHITE_SPACE_FROM_END(line);
 
  char str[RDP_MAX_ATTR_LEN + 1];
  strcpy(str, line);
  str[strlen(line)] = '\0';
  NSDL2_RDP(NULL, NULL, "Method Called, line = %s str = %s", line, str);
  char* token = strtok(str, "("); 
  int api_type;

  //get api_type 
  if(!strcmp(token, NS_MOUSE_DOWN_STR))
    api_type = NS_MOUSE_DOWN;
  else if(!strcmp(token, NS_MOUSE_UP_STR))
    api_type = NS_MOUSE_UP;
  else if(!strcmp(token, NS_MOUSE_CLICK_STR))
    api_type = NS_MOUSE_CLICK;
  else if(!strcmp(token, NS_MOUSE_DOUBLE_CLICK_STR))
    api_type = NS_MOUSE_DOUBLE_CLICK;
  else if(!strcmp(token, NS_MOUSE_MOVE_STR))
    api_type = NS_MOUSE_MOVE;
  else if(!strcmp(token, NS_MOUSE_DRAG_STR))
    api_type = NS_MOUSE_DRAG;
  else
   return DESKTOP_ERROR;
  
  NSDL2_RDP(NULL, NULL, "line = %s token = %s api_type = %d", line, token, api_type);

  return ns_parse_mouse_apis(flow_fp, outfp, flow_filename, flowout_filename, sess_idx, api_type);
}

int ns_get_mouse_button_type(char *button_str, int api_type)
{
  NSDL2_RDP(NULL, NULL, "Method called. butto_str = [%s] api_type = %d ", button_str, api_type);
  CLEAR_WHITE_SPACE(button_str);
  CLEAR_WHITE_SPACE_FROM_END(button_str);
  NSDL2_RDP(NULL, NULL, "now butto_str = [%s]", button_str);
  int button_type;  
  if(!strcasecmp(button_str, "LEFT"))
  {
     switch(api_type)
     {
        case NS_MOUSE_DOWN: 
        button_type = LEFT_BUTTON_DOWN;
        break;
    
        case NS_MOUSE_UP:
        button_type = LEFT_BUTTON_UP;
        break;
 
        case NS_MOUSE_CLICK:
        case NS_MOUSE_DOUBLE_CLICK:
        button_type = LEFT_BUTTON;
        break;
      }
  }
  else if(!strcasecmp(button_str, "RIGHT"))
  {
    switch(api_type)
    {
      case NS_MOUSE_DOWN: 
      button_type = RIGHT_BUTTON_DOWN;
      break;
    
      case NS_MOUSE_UP:
      button_type = RIGHT_BUTTON_UP;
      break;
         
      case NS_MOUSE_CLICK:
      case NS_MOUSE_DOUBLE_CLICK:
      button_type = RIGHT_BUTTON;
      break;
    }
  }
  else {
        SCRIPT_PARSE_ERROR(script_line, "Button type should be either LEFT or RIGHT. Invalid  argument value found for [%s] ", attribute_name);
  }
  NSDL2_RDP(NULL, NULL, "returning...button_type = %d ", button_type);
  return button_type;
}

int ns_parse_mouse_apis(FILE *flow_fp, FILE *outfp,  char *flow_filename, char *flowout_filename, int sess_idx, int api_type)
{
  char attribute_name[128 + 1];
  char attribute_value[RDP_MAX_ATTR_LEN + 1];
  int id_flag = 0;
  int x_flag, y_flag, x1_flag, y1_flag, button_flag, origin_flag;
  x_flag = y_flag = x1_flag = y1_flag = button_flag = origin_flag = -1;

  char out_buff[RDP_MAX_ATTR_LEN + 1];
  int url_idx;
  NSDL2_RDP(NULL, NULL, "Method Called, sess_idx = %d api_type = %d  script_line = %s", sess_idx, api_type, script_line);

  char *page_end_ptr;
  if(!(page_end_ptr = strchr(script_line, '"')))
     SCRIPT_PARSE_ERROR(script_line, "Argument must be provied in double quotes");

  char *close_quotes = page_end_ptr;
  char *start_quotes = page_end_ptr;

  create_requests_table_entry(&url_idx); // Fill request type inside create table entry

  mouse_init(url_idx, RDP_REQUEST, api_type);

  ns_mouse *mouse_action;
  //int type;
  //char buff[RDP_MAX_ATTR_LEN + 1];
  char pagename[RDP_MAX_ATTR_LEN + 1];
  // Process other attribute one by one
  while(1)
  {
     NSDL3_RDP(NULL, NULL, "line = %s, start_quotes = %s", script_line, start_quotes);
     if(get_next_argument(flow_fp, start_quotes, attribute_name, attribute_value, &close_quotes, 0) == NS_PARSE_SCRIPT_ERROR)
       return NS_PARSE_SCRIPT_ERROR;
 
    /* if(!strcasecmp(attribute_name, "ID"))
     {
        ns_parse_id();
    }
    else*/ if(!strcasecmp(attribute_name, "X") || !strcasecmp(attribute_name, "StartX"))
    {
       NSDL2_RDP(NULL, NULL, "X =  [%s] ", attribute_value);
       //ToDo: out_buff not required
       int count = get_mouse_action_and_api(url_idx, api_type, &mouse_action, out_buff); 
       if(y_flag == -1) { 
         sprintf(pagename, "%s_%s_%d", rdp_api_arr[api_type], attribute_value, count);
         NSDL2_RDP(NULL, NULL, "pagename =  [%s] ", pagename);
         if((parse_and_set_pagename(rdp_api_arr[api_type], "ns_rdp", flow_fp, flow_filename,
                   script_line, outfp, flowout_filename, sess_idx, &page_end_ptr, pagename)) == NS_PARSE_SCRIPT_ERROR)
         return NS_PARSE_SCRIPT_ERROR;
       }

       set_segment_value(attribute_value, id_flag, &x_flag, &(mouse_action->x_pos)); 
       NSDL2_RDP(NULL, NULL, "url_idx = %d, attribute_value = %s x_pos = %d", url_idx, attribute_value, mouse_action->x_pos);
    }
    else if(!strcasecmp(attribute_name, "Y") || !strcasecmp(attribute_name, "StartY"))
    {
       NSDL2_RDP(NULL, NULL, "Y =  [%s] ", attribute_value);
       //ToDo: out_buff not required
       int count = get_mouse_action_and_api(url_idx, api_type, &mouse_action, out_buff); 
       if(x_flag == -1) { 
         sprintf(pagename, "%s_%s_%d", rdp_api_arr[api_type], attribute_value, count);
         NSDL2_RDP(NULL, NULL, "pagename =  [%s] ", pagename);
         if((parse_and_set_pagename(rdp_api_arr[api_type], "ns_rdp", flow_fp, flow_filename,
                   script_line, outfp, flowout_filename, sess_idx, &page_end_ptr, pagename)) == NS_PARSE_SCRIPT_ERROR)
         return NS_PARSE_SCRIPT_ERROR;
       }

       set_segment_value(attribute_value, id_flag, &y_flag, &(mouse_action->y_pos)); 
       NSDL2_RDP(NULL, NULL, "url_idx = %d, attribute_value = %s y_pos = %d", url_idx, attribute_value, mouse_action->y_pos);
    }
    else if(!strcasecmp(attribute_name, "EndX"))
    {
       NSDL2_RDP(NULL, NULL, "EndX =  [%s] ", attribute_value);
       //ToDo: out_buff not required
       get_mouse_action_and_api(url_idx, api_type, &mouse_action, out_buff); 
       set_segment_value(attribute_value, id_flag, &x1_flag, &(mouse_action->x1_pos)); 
       NSDL2_RDP(NULL, NULL, "url_idx = %d, attribute_value = %s x1_pos = %d", url_idx, attribute_value, mouse_action->x1_pos);
    }
    else if(!strcasecmp(attribute_name, "EndY"))
    {
       NSDL2_RDP(NULL, NULL, "EndY =  [%s] ", attribute_value);
       //ToDo: out_buff not required
       get_mouse_action_and_api(url_idx, api_type, &mouse_action, out_buff); 

       set_segment_value(attribute_value, id_flag, &y1_flag, &(mouse_action->y1_pos)); 
       NSDL2_RDP(NULL, NULL, "url_idx = %d, attribute_value = %s y1_pos = %d", url_idx, attribute_value, mouse_action->y1_pos);
    }
   else if(!strcasecmp(attribute_name, "Button"))
   {
       NSDL2_RDP(NULL, NULL, "Button =  [%s] ", attribute_value);
       sprintf(attribute_value, "%d", ns_get_mouse_button_type(attribute_value, api_type));
       //ToDo: out_buff not required
       get_mouse_action_and_api(url_idx, api_type, &mouse_action, out_buff/*, &type*/);
       //sprintf(attribute_value, "%d", type);
       set_segment_value(attribute_value, id_flag, &button_flag, &(mouse_action->button_type)); 
       NSDL2_RDP(NULL, NULL, "url_idx = %d, attribute_value = %s button_type = %d", url_idx, attribute_value, mouse_action->button_type);
    }
    else if(!strcasecmp(attribute_name, "Origin"))
    {

       NSDL2_RDP(NULL, NULL, "Origin =  [%s] ", attribute_value);
    
      //ToDo: clear whitespace from attribute_value
      if(!strcmp(attribute_value, "Default"))
         sprintf(attribute_value, "%d", MOUSEMOVE_ABSOLUTE);
       else
         sprintf(attribute_value, "%d", MOUSEMOVE_RELATIVE);
       NSDL2_RDP(NULL, NULL, "now Origin =  [%s] ", attribute_value);
       //ToDo: out_buff not required
       get_mouse_action_and_api(url_idx, api_type, &mouse_action, out_buff); 
       set_segment_value(attribute_value, id_flag, &origin_flag, &(mouse_action->origin)); 
       NSDL2_RDP(NULL, NULL, "url_idx = %d, attribute_value = %s origin = %d", url_idx, attribute_value, mouse_action->origin);
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

  if((x_flag == -1) || (y_flag == -1) )
     SCRIPT_PARSE_ERROR(script_line, "X AND Y  MUST be provided, inorder to execute any mouse action");

  if( (NS_MOUSE_DRAG == api_type) && ((x_flag == -1) || (y_flag == -1)) )
     SCRIPT_PARSE_ERROR(script_line, "EndX AND EndY  MUST be provided, inorder to execute mouse drag action");

  NSDL2_RDP(NULL, NULL, "Exiting Method, protocol_enabled = 0x%x", global_settings->protocol_enabled);
  return NS_PARSE_SCRIPT_SUCCESS;
}

int ns_parse_sync_api(FILE *flow_fp, FILE *outfp,  char *flow_filename, char *flowout_filename, int sess_idx, int api_type)
{
  char attribute_name[128 + 1];
  char attribute_value[RDP_MAX_ATTR_LEN + 1];
  int url_idx;
  int id_flag = 0;
  int timeout_flag = -1;

  NSDL2_RDP(NULL, NULL, "Method Called, sess_idx = %d api_type = %d", sess_idx, api_type);

  char *page_end_ptr;
  if(!(page_end_ptr = strchr(script_line, '"')))
     SCRIPT_PARSE_ERROR(script_line, "Argument must be provied in double quotes");

  char *close_quotes = page_end_ptr;
  char *start_quotes = page_end_ptr;

  create_requests_table_entry(&url_idx); // Fill request type inside create table entry

  sync_init(url_idx, RDP_REQUEST, api_type);

  // Process other attribute one by one
  while(1)
  {
     NSDL3_RDP(NULL, NULL, "line = %s, start_quotes = %s", script_line, start_quotes);
     if(get_next_argument(flow_fp, start_quotes, attribute_name, attribute_value, &close_quotes, 0) == NS_PARSE_SCRIPT_ERROR)
       return NS_PARSE_SCRIPT_ERROR;

    /* if(!strcasecmp(attribute_name, "ID"))
     {
       ns_parse_id();
    }
    else*/ if(!strcasecmp(attribute_name, "Timeout"))
    {
      NSDL2_RDP(NULL, NULL, "Timeout =  [%s] ", attribute_value);
      char pagename[RDP_MAX_ATTR_LEN + 1];
      sprintf(pagename, "%s_%s_%d", "ns_sync", attribute_value, gKeyApi.sync++);
      NSDL2_RDP(NULL, NULL, "pagename =  [%s] ", pagename);
      if((parse_and_set_pagename(rdp_api_arr[api_type], "ns_rdp", flow_fp, flow_filename,
                 script_line, outfp, flowout_filename, sess_idx, &page_end_ptr, pagename)) == NS_PARSE_SCRIPT_ERROR)
      return NS_PARSE_SCRIPT_ERROR;
      set_segment_value(attribute_value, id_flag, &timeout_flag, &(requests[url_idx].proto.rdp.sync.timeout));
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

  if(timeout_flag == -1)
     SCRIPT_PARSE_ERROR(script_line, "Timeout MUST be provided, inorder to perform any sync action");

  NSDL2_RDP(NULL, NULL, "Exiting Method, protocol_enabled = 0x%x", global_settings->protocol_enabled);
  return NS_PARSE_SCRIPT_SUCCESS;
}

