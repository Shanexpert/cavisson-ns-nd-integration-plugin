#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "ns_log.h"
#include "ns_cavmain_child.h"
#include "ns_child.h"
#include "ns_cavmain.h"
#include "util.h"
#include "ns_cavmain_child_thread.h"
#include "nslb_alert.h"
#include "ns_file_upload.h"
#include "ns_schedule_ramp_down_fcu.h"
#include "ns_child_thread_util.h"
#include "ns_vuser.h"
#include "nslb_time_stamp.h"

extern int run_mode;
extern unsigned char my_port_index;
extern unsigned char my_child_index;
extern Msg_com_con g_child_msg_com_con;
extern Msg_com_con g_dh_child_msg_com_con;
extern NetstormAlert *g_ns_file_upload;
extern FileUploadInfo g_file_upload_info;
Msg_com_con sm_nvm_listen_msg_con;
GroupSettings *p_group_default_settings;
Global_data *p_global_settings;
// To control new monitor request
NSLBMap *sm_mon_map;

#define CM_SEND_DATA_TO_PARENT(th_flag, send_msg, data_size) \
{ \
  if(write_msg((th_flag == DATA_MODE)?&g_dh_child_msg_com_con:&g_child_msg_com_con, (char*)send_msg, data_size, 0, th_flag) !=  CM_SUCCESS) \
  {\
    NSDL2_MESSAGES(NULL, NULL, "ERROR while sending data to parent");\
    return;\
  }\
}
 
int cm_start_child()
{
   NSDL2_MESSAGES(NULL, NULL, "Method called pid=%d", getpid());
  
   NSDL2_MESSAGES(NULL, NULL, "parent_port_num=%d ", CM_GET_PARENT_PORT_NUM());
   run_mode = NORMAL_RUN; //Test will always run in normal phase
   sm_init_thread_manager(debug_fd);
  
   // Initialize MAP for SM Mon Request 
   sm_mon_map = nslb_map_init(NSLB_MAP_INIT_SIZE, NSLB_MAP_DELATE_SIZE);
    
   // Init and config file upload url info
   g_ns_file_upload = NULL;
   ns_init_file_upload(global_settings->file_upload_info->tp_init_size, global_settings->file_upload_info->tp_max_size,
                       global_settings->file_upload_info->mq_init_size, global_settings->file_upload_info->mq_max_size);

   strcpy(global_settings->file_upload_info->server_ip, g_file_upload_info.server_ip);
   global_settings->file_upload_info->server_port = g_file_upload_info.server_port;
   global_settings->file_upload_info->protocol = g_file_upload_info.protocol;
   strcpy(global_settings->file_upload_info->url, g_file_upload_info.url);

   NSDL2_MESSAGES(NULL, NULL, "Method called Server IP=%s, Server Port=%d, Protocol=%d, URL=%s", global_settings->file_upload_info->server_ip, global_settings->file_upload_info->server_port, global_settings->file_upload_info->protocol, global_settings->file_upload_info->url);

   ns_config_file_upload(global_settings->file_upload_info->server_ip, global_settings->file_upload_info->server_port,
                        global_settings->file_upload_info->protocol, global_settings->file_upload_info->url,
                        global_settings->file_upload_info->max_conn_retry, global_settings->file_upload_info->retry_timer, ns_event_fd);

   // After mon request gets completed even then we need to have debug and trace logs
   p_global_settings = global_settings;
   p_group_default_settings = group_default_settings;

   netstorm_child(); 

  return CM_SUCCESS;
}


/*pack sm status msg and return the msg size*/
int cm_pack_sm_status_msg(int opcode, char* mon_id, char* msg, char **out_buf)
{

  NSDL2_MESSAGES(NULL, NULL, "Method called. opcode=%d mo_id=%s msg=%s out_buf=%p", opcode, mon_id, msg, *out_buf);

  //char* scratch_buff = *out_buf;
  CM_MON_REQ *send_msg = (CM_MON_REQ*)*out_buf;
  send_msg->data = (char*)*out_buf + sizeof(CM_MON_REQ);
  send_msg->opcode = opcode;
  send_msg->child_id = my_child_index;

  CM_CHILD_STATUS *cm_res = (CM_CHILD_STATUS*)send_msg->data;
  int addr_offset = sizeof(CM_CHILD_STATUS);

  ASSIGN_ADDRESS_AND_VALUE_LEN(&cm_res->mon_id.value, &cm_res->mon_id.len, (char*)cm_res + addr_offset, strlen(mon_id));
  FILL_SM_REQ_FIELDS(cm_res->mon_id.value, mon_id, cm_res->mon_id.len)
  addr_offset += (cm_res->mon_id.len + 1); //+1 for null '\0' character

  ASSIGN_ADDRESS_AND_VALUE_LEN(&cm_res->msg.value, &cm_res->msg.len, (char*)cm_res + addr_offset, strlen(msg));
  NSDL2_MESSAGES(NULL, NULL, "msg_len=%d", cm_res->msg.len);
  FILL_SM_REQ_FIELDS(cm_res->msg.value, msg, cm_res->msg.len)
  addr_offset += (cm_res->msg.len + 1); //+1 for null '\0' character

  int  data_size = addr_offset + sizeof(CM_MON_REQ);
  send_msg->msg_len =  data_size - sizeof(int);
  NSDL2_MESSAGES(NULL, NULL, "send_msg->msg_len=%d data_size=%d", send_msg->msg_len, data_size);
  return data_size;
}

/*process SM_START message from Parent*/
void cm_process_sm_start_msg_frm_parent(CM_MON_REQ *rcv_msg)
{

  CM_MON_INIT* rcv_data = (CM_MON_INIT*)((char*)rcv_msg + sizeof(CM_MON_REQ));
  NSDL2_MESSAGES(NULL, NULL, " Method Called Recieved Msg=>%p", rcv_msg);

  char mon_id[SM_MON_ID_MAX_LEN + 1];
  char mon_name[SM_MON_ID_MAX_LEN + 1];
  char tier_name[SM_MON_ID_MAX_LEN + 1];
  char srv_name[SM_MON_ID_MAX_LEN + 1];
  int  mon_type = rcv_msg->ns_version;
  int  mon_index = rcv_msg->gen_rtc_idx; 
  long part_id = rcv_msg->partition_idx;
  int  testnum = rcv_msg->testidx; 
  // Unpack the message to get the parameters
  cm_unpack_init_msg(rcv_data, mon_id, mon_name, tier_name, srv_name);
  
  void* out_ptr;
  NSLBMap *nslb_map_ptr = sm_mon_map;

  /* find key in MAP*/
  int norm_id = nslb_map_find(nslb_map_ptr, mon_id, &out_ptr);
  switch(norm_id)
  {
     case SM_ERROR:
          NSDL2_MESSAGES(NULL, NULL,"Internal error. Mon Type %s requests can not be processed", mon_id);
          cm_send_message_to_parent(NS_TEST_ERROR, mon_id, CM_MON_STOPPED_ERR);
          return;
 
     case NORM_ID_NOT_FOUND:
          NSDL2_MESSAGES(NULL, NULL,"Norm Id not found. Mon Type %s is new request. Hence processed it..", mon_id);    
          break;
     
     default:
          NSDL2_MESSAGES(NULL, NULL,"Norm Id found. Mon Type %s is already in processing. Discard this...", mon_id);
          cm_send_message_to_parent(NS_TEST_ERROR, mon_id, CM_MON_STOPPED_ERR);
          return;
  }
   
  g_partition_idx = part_id;
  sprintf(g_controller_testrun, "%d", testnum); 
  
  // Its time to spawn thread and pass it to thread
  // and start Scenario and Script parsing 
  sm_run_command_in_thread(mon_id, mon_type, mon_name, mon_index, tier_name, srv_name); 
  
}

/* Process SM_STOP request from parent */
void cm_process_sm_stop_msg_frm_parent(CM_MON_REQ *rcv_msg)
{
   void *out_ptr;
   u_ns_ts_t now;

   char mon_id[SM_MON_ID_MAX_LEN + 1];
  
   CM_MON_STOP* rcv_data = (CM_MON_STOP*)((char*)rcv_msg + sizeof(CM_MON_REQ));
  
   // Unpack message to get mon id
   cm_unpack_stop_msg(rcv_data, mon_id);
  
   NSDL2_MESSAGES(NULL, NULL, " Method Called Recieved Msg=>%p, rcv_data=%p, mon_id= %s", rcv_msg, rcv_data, mon_id); 
  
   NSLBMap *nslb_map_ptr = sm_mon_map;
   // Unpack the msg here to get mon_id here
   if(nslb_map_find(nslb_map_ptr, mon_id, &out_ptr) < 0) {
      NSDL2_MESSAGES(NULL, NULL, "Unable to find mon type %s in MAP as this request is not in processed state", mon_id);
      cm_send_message_to_parent(NS_TEST_ERROR, mon_id, CM_MON_INVALID_REQ);
      return; /*avoid crash*/
   }
   // Delete this map
   nslb_map_delete(sm_mon_map, mon_id);

   SMMonSessionInfo *sm_mon_info_child = (SMMonSessionInfo *)out_ptr;
   sm_mon_info_child->status = SM_STOP;
  
   // Set the global vars
   cav_main_set_global_vars(sm_mon_info_child);
   // We have to rampdown all user immediatley without completion of their session 
   // TODO : Right now we are stopping all session forcefully. Need to discuss whether session has to be executed or stopped.
   group_default_settings->rampdown_method.mode = 2; 
   now = get_ms_stamp();
   remove_all_user_stop_immediately(1, now);
}

void cm_unpack_init_msg(CM_MON_INIT *rcv_data, char *out_mon_id, char* out_mon_name, char *out_tier_name, char *out_srv_name)
{
  NSDL2_MESSAGES(NULL, NULL, "Method called");
  int addr_offset = sizeof(CM_MON_INIT);
  ASSIGN_ADDRESS_AND_VALUE_LEN(&rcv_data->mon_id.value, &rcv_data->mon_id.len, (char*)rcv_data+ addr_offset, rcv_data->mon_id.len);
  NSDL2_MESSAGES(NULL, NULL, "now  rcv_data->mon_id.value=%s", rcv_data->mon_id.value);
  addr_offset += rcv_data->mon_id.len + 1;//+1 for null char
  ASSIGN_ADDRESS_AND_VALUE_LEN(&rcv_data->mon_name.value, &rcv_data->mon_name.len, (char*)rcv_data+ addr_offset, rcv_data->mon_name.len);
  NSDL2_MESSAGES(NULL, NULL, "now  rcv_data->mon_id.value=%s", rcv_data->mon_name.value);

  addr_offset += rcv_data->mon_name.len + 1;//+1 for null char
  /*unpact tier_name*/
  ASSIGN_ADDRESS_AND_VALUE_LEN(&rcv_data->tier_name.value, &rcv_data->tier_name.len, (char*)rcv_data+ addr_offset, rcv_data->tier_name.len);
  NSDL2_MESSAGES(NULL, NULL, "now  rcv_data->tier_name.value=%s", rcv_data->tier_name.value);
  addr_offset += rcv_data->tier_name.len + 1;//+1 for null char
  
  /*unpact server_name*/
  ASSIGN_ADDRESS_AND_VALUE_LEN(&rcv_data->server_name.value, &rcv_data->server_name.len, (char*)rcv_data+ addr_offset, rcv_data->server_name.len);
  NSDL2_MESSAGES(NULL, NULL, "now  rcv_data->server.value=%s", rcv_data->server_name.value);
  addr_offset += rcv_data->server_name.len + 1;//+1 for null char

  strncpy(out_tier_name, rcv_data->tier_name.value, rcv_data->tier_name.len);
  strncpy(out_srv_name, rcv_data->server_name.value, rcv_data->server_name.len);
  strncpy(out_mon_id, rcv_data->mon_id.value, rcv_data->mon_id.len);
  strncpy(out_mon_name, rcv_data->mon_name.value, rcv_data->mon_name.len);
  NSDL2_MESSAGES(NULL, NULL, "recieved mon_id=%s mon_name=%s, out_tier_name=%s, out_srv_name=%s ", out_mon_id, out_mon_name);

}

void cm_unpack_stop_msg(CM_MON_STOP *rcv_data, char *out_mon_id)
{
  NSDL2_MESSAGES(NULL, NULL, "Method called");
  int addr_offset = sizeof(CM_MON_STOP);
  ASSIGN_ADDRESS_AND_VALUE_LEN(&rcv_data->mon_id.value, &rcv_data->mon_id.len, (char*)rcv_data+ addr_offset, rcv_data->mon_id.len);
  NSDL2_MESSAGES(NULL, NULL, "now  rcv_data->mon_id.value=%s", rcv_data->mon_id.value);
/*  addr_offset += rcv_data->mon_id.len + 1;//+1 for null char
  ASSIGN_ADDRESS_AND_VALUE_LEN(&rcv_data->mon_name.value, &rcv_data->mon_name.len, (char*)rcv_data+ addr_offset, rcv_data->mon_name.len);
  NSDL2_MESSAGES(NULL, NULL, "now  rcv_data->mon_id.value=%s", rcv_data->mon_name.value);

  addr_offset += rcv_data->mon_name.len + 1;//+1 for null char
  //unpact tier_name 
  ASSIGN_ADDRESS_AND_VALUE_LEN(&rcv_data->tier_name.value, &rcv_data->tier_name.len, (char*)rcv_data+ addr_offset, rcv_data->tier_name.len);
  NSDL2_MESSAGES(NULL, NULL, "now  rcv_data->tier_name.value=%s", rcv_data->tier_name.value);
  addr_offset += rcv_data->tier_name.len + 1;//+1 for null char
  
  // unpact server_name
  ASSIGN_ADDRESS_AND_VALUE_LEN(&rcv_data->server_name.value, &rcv_data->server_name.len, (char*)rcv_data+ addr_offset, rcv_data->server_name.len);
  NSDL2_MESSAGES(NULL, NULL, "now  rcv_data->server.value=%s", rcv_data->server_name.value);
  addr_offset += rcv_data->server_name.len + 1;//+1 for null char

  strncpy(out_tier_name, rcv_data->tier_name.value, rcv_data->tier_name.len);
  strncpy(out_srv_name, rcv_data->server_name.value, rcv_data->server_name.len);/
  strncpy(out_mon_name, rcv_data->mon_name.value, rcv_data->mon_name.len); */
  strncpy(out_mon_id, rcv_data->mon_id.value, rcv_data->mon_id.len);
  NSDL2_MESSAGES(NULL, NULL, "recieved mon_id=%s", out_mon_id);

}


void sm_create_nvm_listener()
{
   int sm_nvm_listen_fd = -1;

   sm_nvm_listen_fd = vutd_create_listen_fd(&sm_nvm_listen_msg_con, NS_STRUCT_TYPE_SM_THREAD_LISTEN, &sm_nvm_listen_port);
   if(sm_nvm_listen_fd == -1)
   { 
      // Kill this nvm
      NS_EXIT(-1, "FD ERROR occured in child process[%d], sm_create_nvm_listener - with fd %d "
                  "EPOLL_CTL_ADD: err = %s", my_child_index);
   }
   add_select_msg_com_con((char*)&sm_nvm_listen_msg_con, sm_nvm_listen_fd, EPOLLIN);
}


void accept_connection_from_sm_thread(int fd)
{

  int accept_fd  = -1;
  NSDL3_SCHEDULE(NULL, NULL, "Method Called, NVM listen fd = %d", fd);
  
  if((accept_fd = accept(fd, NULL, 0)) == -1)
  {
    // What need to DO here
  } 
 
  Msg_com_con *sm_accept_msg_com_con;
  MY_MALLOC_AND_MEMSET(sm_accept_msg_com_con, sizeof (Msg_com_con), "sm_msg_com_con", -1);
  MY_MALLOC_AND_MEMSET(sm_accept_msg_com_con->read_buf, 1024, "sm_msg_com_con", -1);
  //MY_MALLOC_AND_MEMSET(sm_accept_msg_com_con->write_buf, 1024, "sm_msg_com_con", -1);
  sm_accept_msg_com_con->con_type = NS_STRUCT_TYPE_SM_THREAD_COM ;
  sm_accept_msg_com_con->fd = accept_fd;

  add_select_msg_com_con((char*)sm_accept_msg_com_con, accept_fd, EPOLLIN);


}

void handle_msg_from_sm_thread(Msg_com_con *mcctptr)
{
   NSDL3_SCHEDULE(NULL, NULL,"Method Called mcctptr=>%p", mcctptr);
   if(vutd_read_msg(mcctptr->fd, mcctptr->read_buf) < 0) 
   {       
      NSDL3_SCHEDULE(mcctptr->vptr, NULL, "NVM: Error in getting msg from thread on fd = %d", (mcctptr->fd));
      NSTL1(mcctptr->vptr,NULL, "NVM: Error in gettting msg from thread on fd = %d\n", (mcctptr->fd));
      return;
   }
        
   SMMonSessionInfo *sm_mon_info_child;        
   Vutd_info *sm_req = (Vutd_info *)mcctptr->read_buf;
   sm_mon_info_child = (SMMonSessionInfo *) sm_req->thread_info;
   NSDL3_SCHEDULE(NULL, NULL, "Mon Info PTR %p", sm_mon_info_child);
   sm_process_monitor_thread_msg(sm_mon_info_child);
   NSDL3_SCHEDULE(NULL, NULL,"Method Exit");
}

void cm_send_message_to_parent(int status, char *mon_id, char *msg)
{
   char msg_to_parent[MAX_CMD_LEN];
   char *status_msg = msg_to_parent;
   int data_size = cm_pack_sm_status_msg(status, mon_id, msg, &status_msg);
   NSDL3_SCHEDULE(NULL, NULL,"Status = %d, mon_id = %s, msg = %s, data_size = %d", status, mon_id, msg, data_size);  
   CM_SEND_DATA_TO_PARENT(CONTROL_MODE, status_msg, data_size)
}
