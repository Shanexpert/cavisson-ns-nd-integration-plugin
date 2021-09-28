/******************************************************************************************************
 * Name                :  ns_vuser_runtime_control.c 
 * Purpose             :  Code for Vuser Management
 * Author              :  Devendar/Atul/Anubhav/Nisha
 * Intial version date :  28/11/18
 * Bug Id              :  54384
 * CVS Doc path 
   Req                 :  cavisson\docs\Products\NetStorm\TechDocs\NetStormCore\Req\VUserManagementReq
   Design              :  cavisson\docs\Products\NetStorm\TechDocs\NetStormCore\Design\VUserManagement
 * Last modification date: 
*******************************************************************************************************/
#include<stdio.h>

#include "nslb_util.h"
#include "nslb_big_buf.h"
#include "nslb_static_var_use_once.h"

#include "netomni/src/core/ni_script_parse.h"
#include "netomni/src/core/ni_scenario_distribution.h"

#include "ns_static_use_once.h"
#include "nslb_time_stamp.h"
#include "ns_common.h"
#include "ns_log.h"
#include "ns_alloc.h"
#include "ns_event_id.h"
#include "ns_event_log.h"
#include "ns_string.h"

#include "util.h"
#include "ns_test_gdf.h"
#include "divide_users.h"
#include "divide_values.h"
#include "ns_static_vars.h"
#include "wait_forever.h"
#include "ns_msg_com_util.h"
#include "ns_runtime_changes.h"
#include "ns_static_vars_rtc.h"
#include "nslb_alloc.h"

#include "netstorm.h"
#include "child_init.h"
#include "unique_vals.h"
#include "ns_child_msg_com.h"
#include "ns_master_agent.h"
#include "ns_trace_level.h"
#include "ns_schedule_fcs.h"
#include "ns_group_data.h"
#include "ns_gdf.h"
#include "ns_vuser.h"
#include "ns_vuser_tasks.h"
#include "ns_vuser_ctx.h"
#include "ns_msg_def.h"
#include "ns_trans.h"
#include "ns_vuser_runtime_control.h"
#include "ns_runtime.h"

static RTC_VUser *rtc_send_recv_msg = NULL;
int rtc_send_recv_msg_size=0;

GroupVUserSummaryTable *gVUserSummaryTable = NULL; 
static int gVUserSummaryTableSize = 0;

GroupVUserSummaryTable *clientVUserSummaryTable = NULL;
static int clientVUserSummaryTableSize = 0;

VUserInfoTable *gVUserInfoTable = NULL;
int maxVUserInfoTableEntries = 0;
int totalVUserInfoTableEntries = 0;

VUserQueryInfo *gVUserQueryInfo = NULL;
int maxVUserQueryInfoEntries = 0;
int totalVUserQueryInfoEntries = 0;

VUserQueryInfo *clientVUserQueryInfo = NULL;
int maxClientVUserQueryInfoEntries = 0;

int perClientQuantityInfoSize = 0;

VUserPRSInfo *gVUserPRSInfo = NULL;
int maxVUserPRSInfoEntries = 0;
int totalVUserPRSInfoEntries = 0;

VUserPRSInfo *clientVUserPRSInfo = NULL;
int maxClientVUserPRSInfoEntries = 0;

VUserPauseList *vuser_pause_list;

int *gVUserQueryGrpIdx = NULL;
int *gVUserQueryStatusIdx = NULL;
int *perClientQuantity = NULL;
int *perClientIndex = NULL;

static Msg_com_con *vuser_rtc_mccptr = NULL;

char *rtc_vuser_msg_buf;
int rtc_vuser_msg_buf_size = 0;
void send_rtc_msg_to_parent_or_master(int opcode, char *msg, int size);
int send_rtc_msg_to_client(Msg_com_con *mccptr, int opcode, char *msg, int size);

unsigned long vuser_client_mask[4]; //resetting it at CHECK_AND_SET_INVOKER_MCCPTR 
int g_vuser_msg_seq_num=0;
char vuser_status_msg[][64] = {
  "VUser is Down", 
  "VUser is Running", 
  "VUser is Waiting for Sync Point", 
  "Vuser is Paused", 
  "VUser is Exiting", 
  "VUser is Gradually Exiting", 
  "VUser is Stopped" 
};

char vuser_status_str[][32] = {
  "Down",
  "Running",
  "SP Waiting",
  "Paused",
  "Exiting",
  "G Exiting",
  "Stopped"
};

#define VUSER_ERROR_CTRL_INUSE "[VUser Manager is Busy]"
#define VUSER_ERROR_NO_SUMMARY "[VUser summary table not created]"
#define VUSER_ERROR_VUSER_LIST "[Invalid VUser List command arguments]"
#define VUSER_ERROR_PAUSE_VUSER "[Invalid Pause VUser command arguments]"
#define VUSER_ERROR_RESUME_VUSER "[Invalid Resume VUser command arguments]"
#define VUSER_ERROR_STOP_VUSER "[Invalid Stop VUser command arguments]"
#define VUSER_ERROR_GROUP_INFO "[Invalid Group Information]"
#define VUSER_ERROR_GEN_INFO "[Invalid Generator Information]"
#define VUSER_ERROR_STATUS_INFO "[Invalid Status Information]"
#define VUSER_ERROR_PAUSED_LIST_INFO "[Invalid Paused List command arguments]"
#define VUSER_PAUSE_DONE_MSG "[Pause Request Processed]"
#define VUSER_RESUME_DONE_MSG "[Resume Request Processed]"
#define CAV_MEM_MAP_DONE_MSG "[Memory Map Dump Request Processed]"

#define VUSER_DEBUG_AND_RETURN(X, Y) \
{ \
  NSDL2_MESSAGES(NULL, NULL, "%s", Y); \
  return X; \
}

#define VUSER_ERROR_AND_RETURN(Y) \
{ \
  char send_msg[64]; \
  strcpy(&send_msg[4], "Error:"); \
  strcpy(&send_msg[4 + 6], Y); \
  int send_msg_size = strlen(&send_msg[4]); \
  memcpy(send_msg, &send_msg_size, 4); \
  send_msg_size += 4; \
  write_msg(mccptr, send_msg, send_msg_size, 0, CONTROL_MODE); \
  NSTL1(NULL, NULL, "%s", Y); \
  return; \
}

#define CHECK_AND_SET_INVOKER_MCCPTR \
{ \
  if(loader_opcode != CLIENT_LOADER && vuser_rtc_mccptr != NULL) \
  { \
    VUSER_ERROR_AND_RETURN(VUSER_ERROR_CTRL_INUSE) \
  } \
  if(clientVUserSummaryTable == NULL) \
  { \
    VUSER_ERROR_AND_RETURN(VUSER_ERROR_NO_SUMMARY) \
  } \
  vuser_rtc_mccptr = mccptr; \
  memset(vuser_client_mask, 0, 4 * sizeof(unsigned long));\
  g_vuser_msg_seq_num++;\
}

inline void check_send_rtc_msg_done(int opcode, char *msg, int size)
{
  NSDL2_MESSAGES(NULL, NULL, "Method called opcode = %d, size = %d", opcode, size);

  if(CHECK_ALL_VUSER_CLIENT_DONE)
  {
    NSDL2_MESSAGES(NULL, NULL, "All client done, send msg to master/invoker.");
    if(opcode == -1)  //Bcos rtc_send_recv_msg->opcode is static and we need to access it from wait_forever.c
    {
      opcode = rtc_send_recv_msg->opcode;
    }
    NSDL2_MESSAGES(NULL, NULL, "opcode = %d", opcode);
    if(loader_opcode == CLIENT_LOADER)
    {
      SEND_MSG_TO_MASTER(opcode, msg, size);
    }
    else
    {
      send_rtc_msg_to_invoker(vuser_rtc_mccptr, opcode, msg, size);
      vuser_rtc_mccptr = NULL;
    }
  }
}

void ns_reduce_vuser_summary_counters_on_nvm_failure(int nvm_idx){

  int i;
  GroupVUserSummaryTable *clientVUserSummaryTablePtr;
  NSDL2_MESSAGES(NULL, NULL, "Method Called. client_idx = %d", nvm_idx); 
  //clientVUserSummaryTable gets init , only when  process_get_vuser_summary() is called as response for UI 
  if(!clientVUserSummaryTable)
   return;

  clientVUserSummaryTablePtr = &clientVUserSummaryTable[ nvm_idx * (total_runprof_entries + 1)];
 
  for(i = 0 ; i < total_runprof_entries; i++){

    gVUserSummaryTable[i].num_down_vuser -= clientVUserSummaryTablePtr[i].num_down_vuser;
    gVUserSummaryTable[i].num_running_vuser -= clientVUserSummaryTablePtr[i].num_running_vuser;
    gVUserSummaryTable[i].num_spwaiting_vuser -= clientVUserSummaryTablePtr[i].num_spwaiting_vuser;
    gVUserSummaryTable[i].num_paused_vuser -= clientVUserSummaryTablePtr[i].num_paused_vuser;
    gVUserSummaryTable[i].num_exiting_vuser -= clientVUserSummaryTablePtr[i].num_exiting_vuser;
    gVUserSummaryTable[i].num_gradual_exiting_vuser -= clientVUserSummaryTablePtr[i].num_gradual_exiting_vuser;
    gVUserSummaryTable[i].num_stopped_vuser -= clientVUserSummaryTablePtr[i].num_stopped_vuser;
    gVUserSummaryTable[i].total_vusers -= clientVUserSummaryTablePtr[i].total_vusers;

    clientVUserSummaryTablePtr[total_runprof_entries].num_down_vuser -= clientVUserSummaryTablePtr[i].num_down_vuser;
    clientVUserSummaryTablePtr[total_runprof_entries].num_running_vuser -= clientVUserSummaryTablePtr[i].num_running_vuser;
    clientVUserSummaryTablePtr[total_runprof_entries].num_spwaiting_vuser -= clientVUserSummaryTablePtr[i].num_spwaiting_vuser;
    clientVUserSummaryTablePtr[total_runprof_entries].num_paused_vuser -= clientVUserSummaryTablePtr[i].num_paused_vuser;
    clientVUserSummaryTablePtr[total_runprof_entries].num_exiting_vuser -= clientVUserSummaryTablePtr[i].num_exiting_vuser;
    clientVUserSummaryTablePtr[total_runprof_entries].num_gradual_exiting_vuser -= clientVUserSummaryTablePtr[i].num_gradual_exiting_vuser;
    clientVUserSummaryTablePtr[total_runprof_entries].num_stopped_vuser -= clientVUserSummaryTablePtr[i].num_stopped_vuser;
    clientVUserSummaryTablePtr[total_runprof_entries].total_vusers -= clientVUserSummaryTablePtr[i].total_vusers;

  }
}

void inline ns_vuser_client_failed(int client_idx)
{
  NSDL2_MESSAGES(NULL, NULL, "Method Called. client_idx = %d", client_idx);
 
  if(rtc_send_recv_msg == NULL) 
    return;

  switch(rtc_send_recv_msg->opcode)
  {
    case GET_VUSER_SUMMARY:
    if(loader_opcode == MASTER_LOADER) 
      check_send_rtc_msg_done(GET_VUSER_SUMMARY_ACK, (char *)clientVUserSummaryTable, clientVUserSummaryTableSize);
    else
      check_send_rtc_msg_done(GET_VUSER_SUMMARY_ACK, (char *)gVUserSummaryTable, gVUserSummaryTableSize);
    break;
    case GET_VUSER_LIST:
      check_send_rtc_msg_done(GET_VUSER_LIST_ACK, (char *)gVUserInfoTable, totalVUserInfoTableEntries * sizeof(VUserInfoTable)); 
    break;
    case PAUSE_VUSER:
      check_send_rtc_msg_done(PAUSE_VUSER_ACK, NULL, 0);
    break;
    case RESUME_VUSER:
      check_send_rtc_msg_done(RESUME_VUSER_ACK, NULL, 0);
    break;
    case STOP_VUSER:
      check_send_rtc_msg_done(STOP_VUSER_ACK, NULL, 0);
    break;
  }
}

void init_vuser_pause_list ()
{
  NSDL1_MESSAGES(NULL, NULL, "Method Called");
  MY_MALLOC_AND_MEMSET(vuser_pause_list, sizeof(VUserPauseList) * total_runprof_entries, "vuser pause list", -1);
}

void add_to_pause_list(VUser *vptr)
{
  NSDL1_SCHEDULE(vptr, NULL, "Method Called");

  if(!vuser_pause_list)
    init_vuser_pause_list();

  if(vuser_pause_list[vptr->group_num].vptr_head == NULL)//Linked list is empty
  {
    vuser_pause_list[vptr->group_num].vptr_head = vptr;
    vptr->pause_vuser_next = NULL;
    vptr->pause_vuser_prev = NULL;
    NSDL1_SCHEDULE(vptr, NULL, "vuser_pause_list[%d].vptr_head = %p", vuser_pause_list[vptr->group_num].vptr_head);

  }
  else
  {
    VUser *cur_vuser = vuser_pause_list[vptr->group_num].vptr_head;
    cur_vuser->pause_vuser_prev = vptr;
    vptr->pause_vuser_next = cur_vuser;
    vptr->pause_vuser_prev = NULL;
    vuser_pause_list[vptr->group_num].vptr_head = vptr;
    NSDL1_SCHEDULE(vptr, NULL, "cur_vuser = %p vuser_pause_list[%d].vptr_head = %p",
                                cur_vuser, vptr->group_num, vuser_pause_list[vptr->group_num].vptr_head);
  }
}

void remove_from_pause_list(VUser *vptr)
{
  VUser *cur_vuser = NULL;
  VUser *prev_vuser= NULL;
  VUser *next_vuser= NULL;

  NSDL1_SCHEDULE(vptr, NULL, "Method Called");

  cur_vuser = vptr;
  prev_vuser = vptr->pause_vuser_prev;
  next_vuser = vptr->pause_vuser_next; 

  NSDL1_SCHEDULE(vptr, NULL, "cur_vuser = %p, prev_vuser = %p next_vuser = %p", cur_vuser, prev_vuser, next_vuser);  

  if(cur_vuser == vuser_pause_list [vptr->group_num].vptr_head)
    vuser_pause_list [vptr->group_num].vptr_head = next_vuser;
  
  if(prev_vuser)
    prev_vuser->pause_vuser_next = next_vuser;
  if(next_vuser)
    next_vuser->pause_vuser_prev = prev_vuser;
  
  /* This approach removes the node in O(n) while the above is in O(1)
 
  cur_vuser_head = vuser_pause_list [vptr->group_num].vptr_head;
  prev_vuser_head = vuser_pause_list[vptr->group_num].vptr_head;

  for(; cur_vuser_head != NULL; cur_vuser_head = cur_vuser_head->pause_vuser_next)
  {
    NSDL1_SCHEDULE(NULL, NULL, "vptr = %p, cur_vuser_head = %p, prev_vuser_head = %p, next_vuser_head = %p",
                                vptr, cur_vuser_head, prev_vuser_head, next_vuser_head);

    next_vuser_head = cur_vuser_head->pause_vuser_next;

    if(cur_vuser_head == vptr)
    {
      NSDL2_SCHEDULE(NULL, NULL, "prev_vuser_head = %p, next_vuser_head = %p, cur_vuser_head = %p, vptr = %p",                                                                   prev_vuser_head, next_vuser_head, cur_vuser_head, vptr);

      if(prev_vuser_head)
        prev_vuser_head->pause_vuser_next = next_vuser_head;
    }

    prev_vuser_head = cur_vuser_head;
  }
*/
}

void remove_from_blocked_list(VUser *vptr){

  NSDL1_SCHEDULE(vptr, NULL, "Method Called");

  Pool_vptr *vptr_pool_head = (Pool_vptr *)sess_pool->busy_head;
  while(vptr_pool_head)
  {
    if(vptr == vptr_pool_head->sav_vptr){
      nslb_mp_free_slot(sess_pool, (void *)vptr_pool_head);
    }
    vptr_pool_head = nslb_next(vptr_pool_head);
  }
}

void remove_all_cptr_from_epoll(VUser *vptr){

  NSDL1_SCHEDULE(NULL, NULL, "Method Called vptr = %p", vptr);
  //Removing all in_use cptrs from epoll
  connection *local_cptr = vptr->head_cinuse;
  while(local_cptr){
    NSDL3_SCHEDULE(vptr, NULL, "Going to remove_select cptr->conn_fd = %d cptr = %p", local_cptr->conn_fd, local_cptr);
    if(remove_select(local_cptr->conn_fd) < 0){  
      NSTL1(vptr, NULL, "remove_select failed for cptr->conn_fd = %d cptr = %p", local_cptr->conn_fd, local_cptr);
    }
    else
      vptr->operation = VUT_EPOLL_WAIT;  
    local_cptr = (connection *)local_cptr->next_inuse;
  }
}

void pause_vuser(VUser *vptr)
{
  NSDL1_SCHEDULE(vptr, NULL, "Method Called");

  //Change state
  switch(vptr->vuser_state){

    case NS_VUSER_ACTIVE:
      VUSER_ACTIVE_TO_PAUSED(vptr);
      remove_all_cptr_from_epoll(vptr);
      break;

    case NS_VUSER_BLOCKED:
      //remove this vptr from from blocked list an
      remove_from_blocked_list(vptr); 
      VUSER_BLOCKED_TO_PAUSED(vptr);  
      vptr->operation = VUT_START_SESSION;  
      break;

    case NS_VUSER_THINKING:
      VUSER_THINKING_TO_PAUSED(vptr);
      vptr->operation = VUT_PAGE_THINK_TIME;
      break;

    case NS_VUSER_SESSION_THINK:
      VUSER_WAITING_TO_PAUSED(vptr);
      vptr->operation = VUT_SESSION_PACING;
      break;

    default:
      NSTL1(NULL, NULL, "Error occured while pausing the user, vptr->vuser_state = %d", vptr->vuser_state); 
     //fprintf(stderr, "Error occured while pausing the user");//ERROR TRACE vptr->vuser_state should not reach here
  } 

  if(vptr->flags & NS_VUSER_PAUSE) //Pausing User from VUser context
    vptr->flags &= ~NS_VUSER_PAUSE;
  //vptr->pause_start_time = now;
  add_to_pause_list(vptr); 
}

void add_all_cptr_to_epoll(VUser *vptr, u_ns_ts_t now){
  
  NSDL1_SCHEDULE(NULL, NULL, "Method Called vptr = %p", vptr);  

  connection *local_cptr = vptr->head_cinuse;

  while(local_cptr){
    NSDL3_SCHEDULE(vptr, NULL, "Going to add_select cptr->conn_fd = %d cptr = %p", local_cptr->conn_fd, local_cptr);

    if (add_select(local_cptr, local_cptr->conn_fd, EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLET) < 0) {
      NSDL4_SCHEDULE(vptr, NULL, "add_select failed for cptr->conn_fd = %d cptr = %p", local_cptr->conn_fd, local_cptr);
    }

//    if(!(local_cptr->url_num->request_type == XMPP_REQUEST || local_cptr->url_num->request_type == XMPPS_REQUEST) && 
    if(vptr->flags & NS_VPTR_FLAGS_TIMER_PENDING)
    {
      (local_cptr->timer_ptr->timer_proc)( local_cptr->timer_ptr->client_data, now );
    }
    local_cptr = (connection *)local_cptr->next_inuse;
  }
}

int resume_vuser(VUser *vptr, u_ns_ts_t now)
{
  NSDL1_SCHEDULE(NULL, NULL, "Method Called");

  int operation;

  if(vptr->vuser_state != NS_VUSER_PAUSED){
    NSTL1(NULL, NULL, "Resuming User which not in paused state, vptr->vuser_state = %d", vptr->vuser_state);
    //fprintf(stderr, "User is not in paused state\n");
    return -1;
  }
    
  remove_from_pause_list(vptr);
  //vptr->pause_end_time = now;
  operation = vptr->operation;
  vptr->operation = VUT_NO_TASK;   
  switch(operation)
  {
    case VUT_START_SESSION:
    VUSER_PAUSED_TO_ACTIVE(vptr);
    on_new_session_start(vptr, now);
    break;  
     
    case VUT_USER_CONTEXT:
    VUSER_PAUSED_TO_ACTIVE(vptr);
    switch_to_vuser_ctx(vptr, "Resumed from pause");
    break;
     
    case VUT_EPOLL_WAIT:
    VUSER_PAUSED_TO_ACTIVE(vptr);
    add_all_cptr_to_epoll(vptr, now);
    break;
    
    case VUT_PAGE_THINK_TIME:
    VUSER_PAUSED_TO_THINKING(vptr);
    break;
    
    case VUT_SESSION_PACING:   
    VUSER_PAUSED_TO_WAITING(vptr);
    break;
   
    case VUT_WEB_URL:
    case VUT_CLICK_ACTION:
    case VUT_RBU_WEB_URL_END: 
    case VUT_END_SESSION:
    case VUT_WS_SEND: 
    case VUT_WS_CLOSE: 
    case VUT_SOCKJS_CLOSE: 
    case VUT_XMPP_SEND: 
    case VUT_XMPP_LOGOUT: 
      VUSER_PAUSED_TO_ACTIVE(vptr);
      vut_add_task(vptr,operation);
    break; 

    default :                   
    NSDL2_SCHEDULE(NULL, NULL, "Unknown operation %d",operation);
    VUSER_PAUSED_TO_ACTIVE(vptr);
  }  
  return 0;
}

int divide_vuser_pause_resume_stop_msg_list(int num_vuser){
  
  int i, num_client;
  num_client = global_settings->num_process;
  VUserPRSInfo *gVUserPRSInfoPtr;
  VUserPRSInfo *clientVUserPRSInfoPtr;

  NSDL2_MESSAGES(NULL, NULL, "Method called, num_vuser= %d", num_vuser);
  
  if(maxClientVUserPRSInfoEntries < num_vuser)
  {
    maxClientVUserPRSInfoEntries = num_vuser;
    MY_REALLOC(clientVUserPRSInfo, maxClientVUserPRSInfoEntries * sizeof(VUserPRSInfo), "clientVUserPRSInfo", -1);
  }

  /*Converting count into index table
    Suppose there are two nvms with counts
    0 1 2 3 4 5
    0|0|1|0|3|0 
   
    This table is converted to index table as
    0 1 2 3 4 5
    0|0|0|0|1|0 
  */

  int count = 0, index;
  for(i = 0; i < num_client; i++)
  {
    index = count;
    if(perClientQuantity[i])
    {
      count += perClientQuantity[i];
      perClientIndex[i] = index;
    }
  }
    
  gVUserPRSInfoPtr = gVUserPRSInfo; 
  for(i = 0; i < num_vuser; i++, gVUserPRSInfoPtr++){
    index = (loader_opcode == MASTER_LOADER)? gVUserPRSInfoPtr->gen_idx: gVUserPRSInfoPtr->nvm_id;
    clientVUserPRSInfoPtr = &clientVUserPRSInfo[perClientIndex[index]]; 
    clientVUserPRSInfoPtr->nvm_id = gVUserPRSInfoPtr->nvm_id;
    clientVUserPRSInfoPtr->vuser_idx = gVUserPRSInfoPtr->vuser_idx;
    clientVUserPRSInfoPtr->vptr = gVUserPRSInfoPtr->vptr;
    clientVUserPRSInfoPtr->grp_idx = gVUserPRSInfoPtr->grp_idx;
    clientVUserPRSInfoPtr->quantity = 0;
    clientVUserPRSInfoPtr->gen_idx = gVUserPRSInfoPtr->gen_idx;
    perClientIndex[index]++;
  }

  count = 0;
  for(i = 0; i < num_client; i++)
  {
    index = count;
    if(perClientQuantity[i])
    {
      count += perClientQuantity[i];
      perClientIndex[i] = index;
    }
  }
  return 0;
}  

int divide_vuser_pause_resume_stop_msg_quantity(int opcode, int in_grp_idx, int total_users, int gen_idx, int num_group, int num_gen)
{
  int available_users, mapped_grp_idx, num_client, i, grp_idx;
  static int num_users;
  static int grp_completed = 0;
  NSDL2_MESSAGES(NULL, NULL, "Method called, opcode = %d, in_grp_idx = %d, num_users = %d, gen_idx = %d,"
                                            " num_group = %d, num_gen = %d, total_users = %d",
                                             opcode, in_grp_idx, num_users, gen_idx, num_group, num_gen, total_users);

  perClientQuantityInfoSize = num_group;

  if(maxClientVUserPRSInfoEntries < (global_settings->num_process * perClientQuantityInfoSize))
  {
    maxClientVUserPRSInfoEntries = global_settings->num_process * perClientQuantityInfoSize;
    MY_REALLOC(clientVUserPRSInfo, maxClientVUserPRSInfoEntries * sizeof(VUserPRSInfo), "clientVUserPRSInfo", -1);
  }

  if(total_users >= 0)
  {
    //In case of NC data division is done at Controller. So, if total_users > 0 , then only division will be done for NVM's else quantity 0        will be filled.
    if((loader_opcode == CLIENT_LOADER) && !total_users)
    {
      NSDL2_MESSAGES(NULL, NULL, "No Data is available for division.");
      grp_completed = 1;
    }
    else
    {
      grp_completed = 0;
    }
    num_users = total_users;
  }

  NSDL2_MESSAGES(NULL, NULL, "num_users = %d, grp_completed = %d", num_users, grp_completed);
  //for(g = 0; g < num_group; g++)
  {
    if(in_grp_idx != -1) {
      grp_idx = in_grp_idx;
      mapped_grp_idx = gVUserQueryGrpIdx[grp_idx];
    }
    else {
      grp_idx = total_runprof_entries;
      mapped_grp_idx = 0;
    }

    NSDL2_MESSAGES(NULL, NULL, "mapped_grp_idx = %d", mapped_grp_idx);
    num_client = ((loader_opcode == MASTER_LOADER) && gen_idx != -1)? 1: global_settings->num_process;
    for(i = 0; i < num_client; i++)
    {
      //TODO check client active or not
      int client_id = (loader_opcode == MASTER_LOADER && num_gen)? gen_idx: g_msg_com_con[i].nvm_index;
      NSDL2_MESSAGES(NULL, NULL, "Control Connection: client_id = %d , for i =  %d", client_id, i);
      if(client_id == -1)
      {
        NSDL2_MESSAGES(NULL, NULL, "Control Connection: client will not participate in vuser division");
        continue;
      } 
      GroupVUserSummaryTable *clientVUserSummaryTablePtr = &clientVUserSummaryTable[(client_id * (total_runprof_entries + 1)) + grp_idx];
      //VUserPRSInfo *clientVUserQueryInfoPtr = &clientVUserQueryInfo[client_id * perClientQuantityInfoSize];
      VUserPRSInfo *clientVUserPRSInfoPtr = &clientVUserPRSInfo[client_id * perClientQuantityInfoSize];

      if(grp_completed)
      {
        clientVUserPRSInfoPtr[mapped_grp_idx].grp_idx = in_grp_idx;
        clientVUserPRSInfoPtr[mapped_grp_idx].quantity = 0;
        clientVUserPRSInfoPtr[mapped_grp_idx].gen_idx = gen_idx;
        clientVUserPRSInfoPtr[mapped_grp_idx].vuser_idx = -1;
        clientVUserPRSInfoPtr[mapped_grp_idx].vptr = 0;
        clientVUserPRSInfoPtr[mapped_grp_idx].nvm_id = g_msg_com_con[i].nvm_index;
        continue;
      }
      if(opcode == PAUSE_VUSER)
        available_users = clientVUserSummaryTablePtr->num_running_vuser;
      else if(opcode == RESUME_VUSER)
        available_users = clientVUserSummaryTablePtr->num_paused_vuser;
      else
        available_users = clientVUserSummaryTablePtr->num_running_vuser + clientVUserSummaryTablePtr->num_spwaiting_vuser + clientVUserSummaryTablePtr->num_paused_vuser + clientVUserSummaryTablePtr->num_exiting_vuser + clientVUserSummaryTablePtr->num_gradual_exiting_vuser;
      NSDL2_MESSAGES(NULL, NULL, "available_users = %d", available_users);

      if(num_users)
      {
        if(available_users > num_users)
          available_users = num_users;
        num_users -= available_users;
        if(num_users <= 0)
          grp_completed = 1;
      }

      if(available_users > 0)
        perClientQuantity[client_id] = 1; //set 

      clientVUserPRSInfoPtr[mapped_grp_idx].grp_idx = in_grp_idx;
      clientVUserPRSInfoPtr[mapped_grp_idx].quantity = available_users;
      clientVUserPRSInfoPtr[mapped_grp_idx].gen_idx = gen_idx;
      clientVUserPRSInfoPtr[mapped_grp_idx].vuser_idx = -1;
      clientVUserPRSInfoPtr[mapped_grp_idx].vptr = 0;
      clientVUserPRSInfoPtr[mapped_grp_idx].nvm_id = g_msg_com_con[i].nvm_index;
      NSDL2_MESSAGES(NULL, NULL, "grp_idx = %d, available users = %d, gen_idx = %d", in_grp_idx, available_users, gen_idx);
    }
  }
  return 0;
}

int parse_and_fill_pause_resume_stop_vuser(int opcode, char *msg, int size)
{
  int num_token = 0, num_group = 0, num_gen = 0, gen = 0, quantity = 0, num_vptr = 0, num_vuser = 0;
  int i, grp_idx, gen_idx = 0;
  char *token_fields[MAX_TOKEN];
  char *group_fields[MAX_TOKEN];
  char *vuser_fields[MAX_TOKEN];
  char *vptr_fields[MAX_TOKEN];
  char *gen_fields[MAX_TOKEN];
  char *ptr = NULL;
  VUserPRSInfo *gVUserPRSInfoPtr;
  
  NSDL2_MESSAGES(NULL, NULL, "Method called, opcode = %d, size = %d", opcode, size);
  
  if(loader_opcode != CLIENT_LOADER)
  {
    NSTL3(NULL, NULL, "Method called, opcode = %d, msg = %s, size = %d", opcode, msg + 8, size);
    num_token = get_tokens(msg + 8, token_fields, ";", MAX_TOKEN);  //+8 = 4(opcode), 4(msg_len)
    NSDL2_MESSAGES(NULL, NULL, "num_token = %d", num_token);

    for(i = 0; i < num_token; i++)
    {
      if((ptr = strchr(token_fields[i], '=')) != NULL)
      {
        switch(*(ptr-1))
        {
          case 'g':
            num_group = get_tokens(ptr+1, group_fields, ",", MAX_TOKEN);
          break;
          case 'q':
            quantity = atoi(ptr+1);
          break;
          case 'l':
            num_vuser = get_tokens(ptr+1, vuser_fields, ",", MAX_TOKEN); 
          break;
          case 'v':
            num_vptr = get_tokens(ptr+1, vptr_fields, ",", MAX_TOKEN); 
          break;
          case 'G':
            if(loader_opcode == MASTER_LOADER)
              num_gen = get_tokens(ptr+1, gen_fields, ",", MAX_TOKEN);
            else
              num_gen = 0;
          break;
          default:
            NSDL2_MESSAGES(NULL, NULL, "Unknown input");
        }
      }
      else
        NSDL2_MESSAGES(NULL, NULL, "Wrong Input Pattern");
    }
  
    NSDL2_MESSAGES(NULL, NULL, "num_group = %d, quantity = %d, num_gen = %d, num_vuser = %d, num_vptr = %d", 
                              num_group, quantity, num_gen, num_vuser, num_vptr);
    if(!num_group)
    {
      VUSER_DEBUG_AND_RETURN(-1, VUSER_ERROR_GROUP_INFO);
    }
    for(i = 0; i < num_group; i++)
    {
      if(strstr(group_fields[i], "-1"))
      {
        if(num_group != 1)
          VUSER_DEBUG_AND_RETURN(-1, VUSER_ERROR_GROUP_INFO);
        grp_idx = -1; 
      }
      else
      {
        grp_idx = (ns_is_numeric(group_fields[i]))? atoi(group_fields[i]): -1;
        if(grp_idx < 0 || grp_idx >= total_runprof_entries)
        {
          VUSER_DEBUG_AND_RETURN(-1, VUSER_ERROR_GROUP_INFO);
        }
        gVUserQueryGrpIdx[grp_idx] = i;
      }
    }
    totalVUserPRSInfoEntries = num_group;
    if(num_gen)
    {
      for(i = 0; i< num_gen; i++)
      {
        if(strstr(gen_fields[i], "-1"))
        {
          if(num_gen != 1)
            VUSER_DEBUG_AND_RETURN(-1, VUSER_ERROR_GEN_INFO);
          gen_idx = -1;
        }
        else
        {
          gen_idx = (ns_is_numeric(gen_fields[i]))? atoi(gen_fields[i]): -1;
          if(gen_idx < 0 || gen_idx >= global_settings->num_process)
          {
            VUSER_DEBUG_AND_RETURN(-1, VUSER_ERROR_GEN_INFO);
          }
        }
      }
      totalVUserPRSInfoEntries *= num_gen;
    }

    // Parsing of VUser list
    if(num_vuser)
    {
      if(num_vuser != num_vptr || num_group > 1 || num_gen || quantity)
      {
        VUSER_DEBUG_AND_RETURN(-1, VUSER_ERROR_PAUSED_LIST_INFO);
      }
      totalVUserPRSInfoEntries *= num_vuser; //Assuming only one group and one gen, 
    }
    
    if(maxVUserPRSInfoEntries < totalVUserPRSInfoEntries)
    {
      maxVUserPRSInfoEntries = totalVUserPRSInfoEntries;
      MY_REALLOC(gVUserPRSInfo, maxVUserPRSInfoEntries * sizeof(VUserPRSInfo), "gVUserPRSInfo", -1);
    }
    VUserPRSInfo *gVUserPRSInfoPtr = gVUserPRSInfo;
    if(!num_vuser)
    {
      int g;
      for(g = 0 ; g < num_group; g++)
      {
        grp_idx = atoi(group_fields[g]);
        int gen_quantity = quantity;
        for(gen = 0; gen < ((num_gen == 0)? 1: num_gen); gen++, gVUserPRSInfoPtr++)
        {
          gen_idx = (num_gen==0)? -1: atoi(gen_fields[gen]);
          gVUserPRSInfoPtr->grp_idx = grp_idx;
          gVUserPRSInfoPtr->quantity = gen_quantity;
          gVUserPRSInfoPtr->gen_idx = gen_idx;
          gVUserPRSInfoPtr->vuser_idx = 0;
          gVUserPRSInfoPtr->vptr = 0;
          //quantity has to be overall for all generator
          gen_quantity = -1;
        } 
      }
    }
    else
    { 
      // Parsing of VUser list
      int num_token_loc; 
      char *token_fields_loc[MAX_TOKEN];
      //num_vuser = num_vptr 
      for(i = 0; i < num_vuser; i++, gVUserPRSInfoPtr++)
      { //vuser_fields will be like nvm:user_id    0:1 1:2
        num_token_loc = get_tokens(vuser_fields[i], token_fields_loc, ":", MAX_TOKEN);
        if(loader_opcode == MASTER_LOADER)
        {
          if(num_token_loc != 3)
            VUSER_DEBUG_AND_RETURN(-1, VUSER_ERROR_PAUSED_LIST_INFO);
             
          gVUserPRSInfoPtr->gen_idx = atoi(token_fields_loc[0]);
          gVUserPRSInfoPtr->nvm_id = atoi(token_fields_loc[1]);
          gVUserPRSInfoPtr->vuser_idx = atoi(token_fields_loc[2]);
          perClientQuantity[gVUserPRSInfoPtr->gen_idx]++;
        }
        else
        {
          if(num_token_loc != 2)
            VUSER_DEBUG_AND_RETURN(-1, VUSER_ERROR_PAUSED_LIST_INFO);
          gVUserPRSInfoPtr->gen_idx = -1;
          gVUserPRSInfoPtr->nvm_id = atoi(token_fields_loc[0]);
          gVUserPRSInfoPtr->vuser_idx = atoi(token_fields_loc[1]);
          perClientQuantity[gVUserPRSInfoPtr->nvm_id]++;
        }
        gVUserPRSInfoPtr->vptr = atol(vptr_fields[i]); 
        gVUserPRSInfoPtr->grp_idx = grp_idx;
        gVUserPRSInfoPtr->quantity = 0;
        NSDL2_MESSAGES(NULL, NULL, "gVUserPRSInfoPtr->gen_idx = %d, gVUserPRSInfoPtr->nvm_id = %d, gVUserPRSInfoPtr->vuser_idx = %d", 
                                      gVUserPRSInfoPtr->gen_idx, gVUserPRSInfoPtr->nvm_id, gVUserPRSInfoPtr->vuser_idx);
      }
    }
  }
  else
  {
    totalVUserPRSInfoEntries = size/sizeof(VUserPRSInfo);
    NSDL2_MESSAGES(NULL, NULL, "totalVUserQueryInfoEntries = %d", totalVUserPRSInfoEntries);
    gVUserPRSInfo = (VUserPRSInfo *)((char *)msg + sizeof(RTC_VUser));
    gVUserPRSInfoPtr = gVUserPRSInfo;
    NSDL2_MESSAGES(NULL, NULL, "gVUserPRSInfoPtr->grp_idx = %d", gVUserPRSInfoPtr->grp_idx);
    {
      int cur_g = -1;
      for(i = 0 ; i < totalVUserPRSInfoEntries; i++, gVUserPRSInfoPtr++)
      {
        if(gVUserPRSInfoPtr->vuser_idx >= 0)
        {
          perClientQuantity[gVUserPRSInfoPtr->nvm_id]++;
          num_vuser++;
        }
        else if(gVUserPRSInfoPtr->grp_idx == -1)
        {
          num_group = 1;
          break;
        }
        else if(gVUserPRSInfoPtr->grp_idx != cur_g)
        {
           cur_g = gVUserPRSInfoPtr->grp_idx;
           gVUserQueryGrpIdx[cur_g] = num_group;
           num_group++;
        }
      }
    } 
    num_gen = 1;   
  }
  NSDL2_MESSAGES(NULL, NULL, "num_group = %d, num_gen = %d, num_process = %d", num_group, num_gen, global_settings->num_process);

  //Resetting gVUserPRSInfoPtr
  gVUserPRSInfoPtr = gVUserPRSInfo;

  NSDL2_MESSAGES(NULL, NULL, "totalVUserPRSInfoEntries = %d", totalVUserPRSInfoEntries);
  if(num_vuser)
  {
    divide_vuser_pause_resume_stop_msg_list(num_vuser);
  }
  else
  {
    for(i = 0; i < totalVUserPRSInfoEntries; i++, gVUserPRSInfoPtr++)
    {
      divide_vuser_pause_resume_stop_msg_quantity(opcode, gVUserPRSInfoPtr->grp_idx, gVUserPRSInfoPtr->quantity, gVUserPRSInfoPtr->gen_idx, num_group, num_gen); 
    } 
  }

  //num_client = (loader_opcode == MASTER_LOADER && num_gen)? num_gen: global_settings->num_process;
  NSDL2_MESSAGES(NULL, NULL, "num_process = %d, num_client = %d", global_settings->num_process, ((loader_opcode == MASTER_LOADER && num_gen)? num_gen: global_settings->num_process));

  int idx, nvm_idx = 0; 
  for(i = 0; i < global_settings->num_process; i++)
  {
    VUserPRSInfo *clientVUserPRSInfoPtr; 
    nvm_idx = g_msg_com_con[i].nvm_index;
    NSDL2_MESSAGES(NULL, NULL, "nvm_idx = %d, perClientQuantity[nvm_idx] = %d", nvm_idx, perClientQuantity[nvm_idx]);
    if(nvm_idx == -1)
    {
      NSDL2_MESSAGES(NULL, NULL, "Control Connection: client will not participate in vuser division");
      continue;
    } 
    if(!perClientQuantity[nvm_idx])
    {
      continue;
    }
    if(num_vuser)
    {
      perClientQuantityInfoSize = perClientQuantity[nvm_idx];
      idx = perClientIndex[nvm_idx];
      clientVUserPRSInfoPtr = &clientVUserPRSInfo[idx];
    }
    else
    {
      clientVUserPRSInfoPtr = &clientVUserPRSInfo[nvm_idx * perClientQuantityInfoSize];
    }
    NSDL2_MESSAGES(NULL, NULL, "perClientQuantityInfoSize = %d, nvm_idx = %d, clientVUserPRSInfoPtr->quantity = %d", 
                                perClientQuantityInfoSize, nvm_idx, clientVUserPRSInfoPtr->quantity);
    send_rtc_msg_to_client(&g_msg_com_con[i], opcode, (char *)clientVUserPRSInfoPtr, perClientQuantityInfoSize * sizeof(VUserPRSInfo));
  }
  if(opcode == PAUSE_VUSER)
    check_send_rtc_msg_done(PAUSE_VUSER_ACK, NULL, 0);
  else if(opcode == RESUME_VUSER)
    check_send_rtc_msg_done(RESUME_VUSER_ACK, NULL, 0);
  else
    check_send_rtc_msg_done(STOP_VUSER_ACK, NULL, 0);

  return 0;
}

void process_resume_vuser(Msg_com_con *mccptr, char *msg, int size)
{
  int ret;
  char runtime_log[1024 + 1];

  NSDL2_MESSAGES(NULL, NULL, "Method called");

  /* open log file */
  sprintf(runtime_log, "%s/logs/TR%d/runtime_changes/runtime_changes.log", g_ns_wdir, testidx);
  if ((rtc_log_fp = fopen(runtime_log, "a+")) == NULL) {
    NSDL2_MESSAGES(NULL, NULL, "Error in opening file %s. Error=%s", runtime_log, nslb_strerror(errno));
    NSTL3(NULL, NULL, "Error in opening file %s. Error=%s", runtime_log, nslb_strerror(errno));
    return;
  }

  if(loader_opcode != CLIENT_LOADER && vuser_rtc_mccptr != NULL){
    fprintf(rtc_log_fp, "ERROR: %s\n", VUSER_ERROR_CTRL_INUSE); 
    fclose(rtc_log_fp); 
    rtc_log_fp = NULL;
  }

  CHECK_AND_SET_INVOKER_MCCPTR

  memset(perClientQuantity, 0, global_settings->num_process * sizeof(int)); 
  memset(perClientIndex, 0, global_settings->num_process * sizeof(int)); 

  if(rtc_log_fp && (loader_opcode != CLIENT_LOADER))
    fprintf(rtc_log_fp, "Command: %s", msg + 8); //+8 to skip opcode + msg_len

  if((ret = parse_and_fill_pause_resume_stop_vuser(RESUME_VUSER, msg, size)) < 0)
  {
    if(rtc_log_fp){
      fprintf(rtc_log_fp, "Error: %s\n", VUSER_ERROR_RESUME_VUSER);
      fclose(rtc_log_fp);
      rtc_log_fp = NULL;
    }
    vuser_rtc_mccptr = NULL;
    VUSER_ERROR_AND_RETURN(VUSER_ERROR_RESUME_VUSER);
  }
}

void process_pause_vuser(Msg_com_con *mccptr, char *msg, int size)
{
  int ret;
  char runtime_log[1024 + 1];

  NSDL2_MESSAGES(NULL, NULL, "Method called");

  /* open log file */
  sprintf(runtime_log, "%s/logs/TR%d/runtime_changes/runtime_changes.log", g_ns_wdir, testidx);
  if ((rtc_log_fp = fopen(runtime_log, "a+")) == NULL) {
    NSDL2_MESSAGES(NULL, NULL, "Error in opening file %s. Error=%s", runtime_log, nslb_strerror(errno));
    NSTL3(NULL, NULL, "Error in opening file %s. Error=%s", runtime_log, nslb_strerror(errno));
    return;
  }

  if(loader_opcode != CLIENT_LOADER && vuser_rtc_mccptr != NULL){
    fprintf(rtc_log_fp, "ERROR: %s\n", VUSER_ERROR_CTRL_INUSE); 
    fclose(rtc_log_fp); 
    rtc_log_fp = NULL;
  }

  CHECK_AND_SET_INVOKER_MCCPTR

  memset(perClientQuantity, 0, global_settings->num_process * sizeof(int)); 
  memset(perClientIndex, 0, global_settings->num_process * sizeof(int)); 

  if(rtc_log_fp && (loader_opcode != CLIENT_LOADER))
    fprintf(rtc_log_fp, "Command: %s", msg + 8); //+8 to skip opcode + msg_len

  if((ret = parse_and_fill_pause_resume_stop_vuser(PAUSE_VUSER, msg, size)) < 0)
  {
    if(rtc_log_fp){
      fprintf(rtc_log_fp, "Error: %s\n", VUSER_ERROR_PAUSE_VUSER);
      fclose(rtc_log_fp); 
      rtc_log_fp = NULL;
    }

    vuser_rtc_mccptr = NULL;
    VUSER_ERROR_AND_RETURN(VUSER_ERROR_PAUSE_VUSER);
  }
}

void process_stop_vuser(Msg_com_con *mccptr, char *msg, int size)
{
  int ret;
    
  NSDL2_MESSAGES(NULL, NULL, "Method called");

  CHECK_AND_SET_INVOKER_MCCPTR
  
  memset(perClientQuantity, 0, global_settings->num_process * sizeof(int)); 
  memset(perClientIndex, 0, global_settings->num_process * sizeof(int)); 
  if(rtc_log_fp && (loader_opcode != CLIENT_LOADER))
    fprintf(rtc_log_fp, "%s\n", msg + 8); //+8 to skip opcode + msg_len

  if((ret = parse_and_fill_pause_resume_stop_vuser(STOP_VUSER, msg, size)) < 0)
  {
    if(rtc_log_fp)
      fprintf(rtc_log_fp, "Error: control connection %s\n", VUSER_ERROR_PAUSE_VUSER);
    vuser_rtc_mccptr = NULL;
    VUSER_ERROR_AND_RETURN(VUSER_ERROR_STOP_VUSER);
  }
}

/******************************************************************
 * Name    :    init_vuser_summary_table
 * Purpose :    This function will malloc VUserSummaryTable of size
                gVUserSummaryTableSize to keep data of all VUsers.
 * Note    :
 * Author  :    
 * Intial version date:    28/11/18
 * Last modification date: 
******************************************************************/
void init_vuser_summary_table(int parent_only)
{
  gVUserSummaryTableSize = total_runprof_entries * sizeof(GroupVUserSummaryTable);
  
  NSDL2_MESSAGES(NULL, NULL, "Method called, parent_only = %d, gVUserSummaryTableSize = %d", parent_only, gVUserSummaryTableSize);

  if(!parent_only)
  {
    MY_MALLOC_AND_MEMSET(gVUserSummaryTable, gVUserSummaryTableSize, "VUserSummaryTable", 0);
  }
  else
  {
    clientVUserSummaryTableSize =  (global_settings->num_process * (gVUserSummaryTableSize + sizeof(GroupVUserSummaryTable))); // 1 extra row in clientSummaryTable for client overall data
    MY_MALLOC_AND_MEMSET(clientVUserSummaryTable, clientVUserSummaryTableSize, "clientVUserSummaryTable", 0);
    MY_MALLOC_AND_MEMSET(gVUserQueryGrpIdx, total_runprof_entries * sizeof(int), "gVUserQueryGrpIdx", -1); 
    MY_MALLOC_AND_MEMSET(gVUserQueryStatusIdx, (NS_VUSER_RTC_STOPPED + 1) * sizeof(int), "gVUserQueryStatusIdx", -1);
    MY_MALLOC_AND_MEMSET(perClientQuantity, global_settings->num_process * sizeof(int), "perClientQuantity", -1);
    MY_MALLOC_AND_MEMSET(perClientIndex, global_settings->num_process * sizeof(int), "perClientIndex", -1);
  }
}

/********************************************************************
 * Name    :    send_rtc_msg_to_client
 * Purpose :    This function will write msg to all its clients/NVMS.
                Here "vuser_client_mask" is taken to keep track
                of messages sent, to which NVMS/clients
 * Note    :
 * Author  :    
 * Intial version date:    28/11/18
 * Last modification date: 
*********************************************************************/
int send_rtc_msg_to_client(Msg_com_con *mccptr, int opcode, char *msg, int size)
{
  int data_size;
  NSDL2_MESSAGES(NULL, NULL, "Method called, opcode = %d, size = %d", opcode, size);
  if((loader_opcode == MASTER_LOADER) && (generator_entry[mccptr->nvm_index].flags & IS_GEN_INACTIVE))
  {
    NSTL1(NULL, NULL, "Vuser Manager :Generator id %d already marked as killed", mccptr->nvm_index);
    return -1; 
  }
  if(mccptr->fd == -1)  //Check if nvm/generator is dead or not
  {
    if(mccptr->ip)
    {
      NSDL2_MESSAGES(NULL, NULL, "Connection with the NVM/Generator is already closed so not sending the msg to %s",
                                  msg_com_con_to_str(mccptr));
    }
    return -1;
  }
  
  data_size = size + sizeof(RTC_VUser);
  if(data_size > rtc_send_recv_msg_size)
  {
    rtc_send_recv_msg_size = data_size;
    MY_REALLOC(rtc_send_recv_msg, rtc_send_recv_msg_size, "RTC_VUser",-1);
    rtc_send_recv_msg->data = (char*)rtc_send_recv_msg + sizeof(RTC_VUser);
  }
  rtc_send_recv_msg->opcode = opcode;
  rtc_send_recv_msg->gen_rtc_idx = g_vuser_msg_seq_num;
  if(size > 0)
    memcpy(rtc_send_recv_msg->data, msg, size);
  rtc_send_recv_msg->msg_len =  data_size - sizeof(int);
  NSDL2_MESSAGES(NULL, NULL, "data_size = %d", data_size);
  if((write_msg(mccptr, (char *)rtc_send_recv_msg, data_size, 0, ISCALLER_DATA_HANDLER?DATA_MODE:CONTROL_MODE)) < 0)
  {
    NSDL2_MESSAGES(NULL, NULL, "Failed to write message");
    return -1;
  }
  if(!CHECK_RTC_FLAG(RUNTIME_ALERT_FLAG))
    INC_VUSER_MSG_COUNT(mccptr->nvm_index); 

  return 0;
}

/******************************************************************
 * Name    :    send_rtc_msg_to_all_clients
 * Purpose :    This function will check whether it is called from
                Controller or NS Parent and then send message to all
                clients/NVMS using send_rtc_msg_to_client() 
******************************************************************/
int send_rtc_msg_to_all_clients(int opcode, char *msg, int size)
{
  int i;
  NSDL2_MESSAGES(NULL, NULL, "Method called. opcode = %d, size = %d", opcode, size);
  for(i = 0; i <  global_settings->num_process; i++)
  { 
    //TODO: if nvm has send finished report and is in pause
    NSDL3_MESSAGES(NULL, NULL, "Sending msg to Client id = %d, opcode = %d, msg = %p", i, opcode, msg);
    NSTL1(NULL, NULL, "Sending msg to client '%d' for opcode '%d'", i, opcode);
    send_rtc_msg_to_client(&g_msg_com_con[i], opcode, msg, size);
  }
  return 0;  
}

/******************************************************************
 * Name    :    process_get_vuser_summary
 * Purpose :    This function is invoked from wait_forever.c when 
                opcode is "GET_VUSER_SUMMARY"
******************************************************************/
void process_get_vuser_summary(Msg_com_con *mccptr)
{
  NSDL2_MESSAGES(NULL, NULL, "Method called");

  if(!clientVUserSummaryTable)
    init_vuser_summary_table(1);

  CHECK_AND_SET_INVOKER_MCCPTR

  /*Commenting these as in case NVM is finished its work, its data is not sent. Hence not memset() these so that we can get its previous dataFor other nvms instead of memset() this is handled in update_vuser_summary_counters()*/
  //memset(gVUserSummaryTable, 0, gVUserSummaryTableSize); //Comments: block
  //memset(clientVUserSummaryTable, 0, clientVUserSummaryTableSize); //Comments: block
  send_rtc_msg_to_all_clients(GET_VUSER_SUMMARY, NULL, 0);

  if(loader_opcode == MASTER_LOADER) 
    check_send_rtc_msg_done(GET_VUSER_SUMMARY_ACK, (char *)clientVUserSummaryTable, clientVUserSummaryTableSize);
  else
    check_send_rtc_msg_done(GET_VUSER_SUMMARY_ACK, (char *)gVUserSummaryTable, gVUserSummaryTableSize);
}

int divide_vuser_list_msg(int in_grp_idx, int start_offset, int total_users, int status, int gen_idx, int num_group, int num_status, int num_gen)
{
  int available_users, mapped_grp_idx, mapped_status_idx, num_client, i, grp_idx;
  static int num_users, offset;
  static  int grp_completed = 0;

  NSDL2_MESSAGES(NULL, NULL, "Method called, control connection in_grp_idx = %d, start_offset = %d, total_users = %d, status = %d,"
                              " gen_idx = %d, num_group = %d, num_status = %d, num_gen = %d", 
                               in_grp_idx, start_offset, total_users, status, gen_idx, num_group, num_status, num_gen);

  num_status = num_status ? num_status: 1;  
  perClientQuantityInfoSize = num_group * num_status; 

  if(total_users >= 0)
  {
    //In case of NC data division is done at Controller. So, if total_users > 0 , then only division will be done for NVM's else quantity 0        will be filled.
    if((loader_opcode == CLIENT_LOADER) && !total_users)
    {
      NSDL2_MESSAGES(NULL, NULL, "Control Connection: No Data is available for division.");
      grp_completed = 1;
    }
    else
    {
      grp_completed = 0;
    }
    num_users = total_users;
  }

  NSDL2_MESSAGES(NULL, NULL, "Control Connection: num_users = %d, grp_completed = %d", num_users, grp_completed);

  if(start_offset >= 0)
    offset = start_offset;

  NSDL2_MESSAGES(NULL, NULL, "Control connection perClientQuantityInfoSize = %d", perClientQuantityInfoSize);

  if(maxClientVUserQueryInfoEntries < (global_settings->num_process * perClientQuantityInfoSize))
  {
    maxClientVUserQueryInfoEntries = global_settings->num_process * perClientQuantityInfoSize;
    MY_REALLOC(clientVUserQueryInfo, maxClientVUserQueryInfoEntries * sizeof(VUserQueryInfo), "clientVUserQueryInfo", -1);
  }
 
  //for(g = 0; g < num_group; g++)
  {
    if(in_grp_idx != -1) {
      grp_idx = in_grp_idx;
      mapped_grp_idx = gVUserQueryGrpIdx[grp_idx];
      NSDL2_MESSAGES(NULL, NULL, "Control connection mapped_grp_idx = %d", mapped_grp_idx);
    }
    else {
      grp_idx = total_runprof_entries;
      mapped_grp_idx = 0;
    }
    if(status != -1)
      mapped_status_idx = gVUserQueryStatusIdx[status]; 
    else 
      mapped_status_idx = 0;
    num_client = ((loader_opcode == MASTER_LOADER) && gen_idx != -1)? 1: global_settings->num_process;
    for(i = 0; i < num_client; i++)
    {
      //TODO check client active or not
      int client_id = (loader_opcode == MASTER_LOADER && num_gen)? gen_idx: g_msg_com_con[i].nvm_index;
      NSDL2_MESSAGES(NULL, NULL, "Control Connection: client_id = %d , for i =  %d, grp_completed = %d", client_id, i, grp_completed);
      if(client_id == -1)
      {
        //bug 63959: when child failed to connect then unable to fetch Vuser list, so skipping client
        NSDL2_MESSAGES(NULL, NULL, "Control Connection: client will not participate in vuser division");
        continue;
      }
      GroupVUserSummaryTable *clientVUserSummaryTablePtr = &clientVUserSummaryTable[client_id * (total_runprof_entries + 1) + grp_idx];
      VUserQueryInfo *clientVUserQueryInfoPtr = &clientVUserQueryInfo[client_id * perClientQuantityInfoSize];
      if(grp_completed)
      {
        clientVUserQueryInfoPtr[(mapped_grp_idx * num_status) + mapped_status_idx].grp_idx = in_grp_idx;
        clientVUserQueryInfoPtr[(mapped_grp_idx * num_status) + mapped_status_idx].offset = offset;
        clientVUserQueryInfoPtr[(mapped_grp_idx * num_status) + mapped_status_idx].limit = 0;
        clientVUserQueryInfoPtr[(mapped_grp_idx * num_status) + mapped_status_idx].status = status;
        clientVUserQueryInfoPtr[(mapped_grp_idx * num_status) + mapped_status_idx].gen_idx = gen_idx;
        continue;
      }
      switch(status)
      { 
        case NS_VUSER_RTC_DOWN:
        available_users = clientVUserSummaryTablePtr->num_down_vuser;
        break;
      
        case NS_VUSER_RTC_RUNNING:
        available_users = clientVUserSummaryTablePtr->num_running_vuser;
        break;
      
        case NS_VUSER_RTC_GRADUAL_EXITING:
        available_users = clientVUserSummaryTablePtr->num_gradual_exiting_vuser;
        break;
      
        case NS_VUSER_RTC_EXITING:
        available_users = clientVUserSummaryTablePtr->num_exiting_vuser;
        break;
      
        case NS_VUSER_RTC_SYNC_POINT:
        available_users = clientVUserSummaryTablePtr->num_spwaiting_vuser;
        break;
      
        case NS_VUSER_RTC_PAUSED:
        available_users = clientVUserSummaryTablePtr->num_paused_vuser;
        break;
      
        case NS_VUSER_RTC_STOPPED:
        available_users = clientVUserSummaryTablePtr->num_stopped_vuser;
        break;
      
        default:
        available_users = clientVUserSummaryTablePtr->num_running_vuser + clientVUserSummaryTablePtr->num_spwaiting_vuser + clientVUserSummaryTablePtr->num_paused_vuser + clientVUserSummaryTablePtr->num_exiting_vuser + clientVUserSummaryTablePtr->num_gradual_exiting_vuser;
      }
    
      NSDL2_MESSAGES(NULL, NULL, "Control connection, available_users = %d, num_users = %d, offset = %d", 
                                  available_users, num_users, offset);
      NSTL3(NULL, NULL, "Control connection, available_users = %d, num_users = %d, offset = %d", available_users, num_users, offset);
      if(num_users)
      {      
        if(available_users <= offset)
        {
          //skip the client
          offset -= available_users;
          perClientQuantity[client_id] = 0; //Reset 
          continue;
        }
        else
        {
          available_users -= offset;
        }
        if(available_users > num_users)
          available_users = num_users;
        num_users -= available_users;
        if(num_users <= 0)
          grp_completed = 1;
      }
     
      perClientQuantity[client_id] = 1; //Set  
      clientVUserQueryInfoPtr[(mapped_grp_idx * num_status) + mapped_status_idx].grp_idx = in_grp_idx;
      clientVUserQueryInfoPtr[(mapped_grp_idx * num_status) + mapped_status_idx].offset = offset;
      clientVUserQueryInfoPtr[(mapped_grp_idx * num_status) + mapped_status_idx].limit = available_users;
      clientVUserQueryInfoPtr[(mapped_grp_idx * num_status) + mapped_status_idx].status = status;
      clientVUserQueryInfoPtr[(mapped_grp_idx * num_status) + mapped_status_idx].gen_idx = gen_idx;
   
      NSDL2_MESSAGES(NULL, NULL, "Control connection, filling vuser list, grp_idx = %d, offset = %d, limit = %d, status = %d, gen_idx = %d", 
                                  in_grp_idx, offset, available_users, status, gen_idx);
   
      offset = 0;
    }
  }
  return 0;
}

int parse_and_fill_vuser_list(char *msg, int size)
{
  int num_token, gen;
  //int num_client;
  int limit = 0;
  int num_group = 0;
  int num_status = 0;
  int offset = 0;
  int num_gen = 0;
  int i, g, s, grp_idx, status, gen_idx;
  char *token_fields[MAX_TOKEN];
  char *group_fields[MAX_TOKEN];
  char *status_fields[MAX_TOKEN];
  char *gen_fields[MAX_TOKEN];
  char *ptr = NULL;
  VUserQueryInfo *gVUserQueryInfoPtr;

  NSDL2_MESSAGES(NULL, NULL, "Method called, size = %d", size);
  
  if(loader_opcode != CLIENT_LOADER)
  {
    num_token = get_tokens(msg+8, token_fields, ";", MAX_TOKEN);  //+8 = 4(opcode), 4(msg_len)
    NSDL2_MESSAGES(NULL, NULL, "num_token = %d", num_token);
    NSTL2(NULL, NULL, "num_token = %d", num_token);
 
    for(i = 0; i < num_token; i++)
    {
      if((ptr = strchr(token_fields[i], '=')) != NULL)
      {
        switch(*(ptr-1))
        {
          case 'g':
            num_group = get_tokens(ptr+1, group_fields, ",", MAX_TOKEN);
          break;
          case 'O':
            offset = atoi(ptr+1);
          break;
          case 'L':
            limit = atoi(ptr+1);
          break;
          case 's':
            num_status = get_tokens(ptr+1, status_fields, ",", MAX_TOKEN);
          break;
          case 'G':
            if(loader_opcode == MASTER_LOADER)
              num_gen = get_tokens(ptr+1, gen_fields, ",", MAX_TOKEN);
            else
              num_gen = 0;
          break;
          default:
            NSDL2_MESSAGES(NULL, NULL, "Unknown input");
        }
      }
      else
      {
        NSDL2_MESSAGES(NULL, NULL, "Wrong Input Pattern");
        NSTL2(NULL, NULL, "Wrong Input pattern");
      }
    }
    if(!num_group)
    {
      VUSER_DEBUG_AND_RETURN(-1, VUSER_ERROR_GROUP_INFO);
    }
    for(i = 0; i < num_group; i++)
    {
      if(strstr(group_fields[i],"-1"))
      {
        if(num_group != 1)
          VUSER_DEBUG_AND_RETURN(-1, VUSER_ERROR_GROUP_INFO);
        grp_idx = -1; 
      }
      else
      {
        grp_idx = (ns_is_numeric(group_fields[i]))? atoi(group_fields[i]): -1;
        if(grp_idx < 0 || grp_idx >= total_runprof_entries)
        {
          VUSER_DEBUG_AND_RETURN(-1, VUSER_ERROR_GROUP_INFO);
        }
        gVUserQueryGrpIdx[grp_idx] = i;
      }
    }
    totalVUserQueryInfoEntries = num_group;
    if(num_gen)
    {
      for(i = 0; i < num_gen; i++)
      {
        if(strstr(gen_fields[i],"-1"))
        {
          if(num_gen != 1)
            VUSER_DEBUG_AND_RETURN(-1, VUSER_ERROR_GEN_INFO);
          gen_idx = -1;
        }
        else
        {
          gen_idx = (ns_is_numeric(gen_fields[i]))? atoi(gen_fields[i]): -1;
          if(gen_idx < 0 || gen_idx >= global_settings->num_process)
          {
            VUSER_DEBUG_AND_RETURN(-1, VUSER_ERROR_GEN_INFO);
          }
        }
      }
      totalVUserQueryInfoEntries *= num_gen;
    }
    if(num_status)
    {
      for(i = 0; i < num_status; i++)
      {
        if(strstr(status_fields[i],"-1"))
        {
          if(num_status != 1)
            VUSER_DEBUG_AND_RETURN(-1, VUSER_ERROR_STATUS_INFO);
          status = -1;
        }
        else
        {
          status = (ns_is_numeric(status_fields[i]))? atoi(status_fields[i]): -1;
          if(status < 0 || status > NS_VUSER_RTC_STOPPED)
          {
            VUSER_DEBUG_AND_RETURN(-1, VUSER_ERROR_STATUS_INFO);
          }
          gVUserQueryStatusIdx[status] = i;
        }
      }
      totalVUserQueryInfoEntries *= num_status;
    }
  
    NSDL2_MESSAGES(NULL, NULL, "totalVUserQueryInfoEntries = %d", totalVUserQueryInfoEntries);
    
    if(maxVUserQueryInfoEntries < totalVUserQueryInfoEntries)
    {
      maxVUserQueryInfoEntries = totalVUserQueryInfoEntries;
      MY_REALLOC(gVUserQueryInfo, maxVUserQueryInfoEntries * sizeof(VUserQueryInfo), "gVUserQueryInfo", -1);
    }
    gVUserQueryInfoPtr = gVUserQueryInfo;
    //for(i = 0 ; i < totalVUserQueryInfoEntries; i++)
    {
      for(g = 0 ; g < num_group; g++)
      {
        grp_idx = atoi(group_fields[g]);
        for(s = 0; s < ((num_status == 0)? 1: num_status); s++) 
        {
          status = (num_status == 0)? -1: atoi(status_fields[s]);
          int gen_offset = offset;
          int gen_limit = limit; 
          for(gen = 0; gen < ((num_gen == 0)? 1: num_gen); gen++, gVUserQueryInfoPtr++)
          {
            gen_idx = (num_gen == 0)? -1: atoi(gen_fields[gen]);
            gVUserQueryInfoPtr->grp_idx = grp_idx;
            gVUserQueryInfoPtr->offset = gen_offset;
            gVUserQueryInfoPtr->limit = gen_limit;  
            gVUserQueryInfoPtr->status =  status;
            gVUserQueryInfoPtr->gen_idx = gen_idx;
            //limit and offset has to be overall for all generator
            gen_limit = -1;
            gen_offset = -1;
          } 
        }
      } 
    }
  }
  else
  {
    totalVUserQueryInfoEntries = size/sizeof(VUserQueryInfo);
    NSDL2_MESSAGES(NULL, NULL, "totalVUserQueryInfoEntries = %d", totalVUserQueryInfoEntries);  
    gVUserQueryInfo = (VUserQueryInfo *)((char *)msg + sizeof(RTC_VUser));
    gVUserQueryInfoPtr = gVUserQueryInfo;
    NSDL2_MESSAGES(NULL, NULL, "gVUserQueryInfoPtr->grp_idx = %d, gVUserQueryInfoPtr->status = %d, gVUserQueryInfoPtr->limit = %d", 
                                gVUserQueryInfoPtr->grp_idx, gVUserQueryInfoPtr->status, gVUserQueryInfoPtr->limit);

    if(gVUserQueryInfoPtr->grp_idx == -1 && gVUserQueryInfoPtr->status == -1)
    {
      num_group = 1;
      num_status = 1;
    }
    else if(gVUserQueryInfoPtr->status == -1)
    {
      int cur_g = -1, j = 0;
      num_status = 1;
      num_group = totalVUserQueryInfoEntries;
      for(i = 0 ; i < totalVUserQueryInfoEntries; i++,gVUserQueryInfoPtr++)
      {
        if(gVUserQueryInfoPtr->grp_idx != cur_g)
        {
           cur_g = gVUserQueryInfoPtr->grp_idx; 
           gVUserQueryGrpIdx[cur_g] = j;
           j++;
        }
      }
    }
    else if(gVUserQueryInfoPtr->grp_idx == -1)
    {
      num_group = 1;
      num_status = totalVUserQueryInfoEntries;
    }
    else
    {
      int cur_g = -1, cur_s = -1, sflag = 0;
      for(i = 0 ; i < totalVUserQueryInfoEntries; i++,gVUserQueryInfoPtr++)
      {
        if(gVUserQueryInfoPtr->grp_idx != cur_g)
        {
           if(cur_g != -1)
             sflag = 1;
           cur_g = gVUserQueryInfoPtr->grp_idx;  
           gVUserQueryGrpIdx[cur_g] = num_group;
           num_group++;
        }
        if(!sflag && (gVUserQueryInfoPtr->status != cur_s))
        {
           num_status++;
           cur_s = gVUserQueryInfoPtr->status;  
        }
      }
    }
    num_gen = 1;
  }
  NSDL2_MESSAGES(NULL, NULL, "num_group =  %d, num_status = %d, num_gen = %d", num_group, num_status, num_gen);

  //Reset gVUserQueryInfoPtr to gVUserQueryInfo
  gVUserQueryInfoPtr = gVUserQueryInfo;

  for(i = 0; i < totalVUserQueryInfoEntries; i++, gVUserQueryInfoPtr++)
  {
    divide_vuser_list_msg(gVUserQueryInfoPtr->grp_idx, gVUserQueryInfoPtr->offset, gVUserQueryInfoPtr->limit, gVUserQueryInfoPtr->status, gVUserQueryInfoPtr->gen_idx, num_group, num_status, num_gen);  
  }

  //num_client = (loader_opcode == MASTER_LOADER && num_gen)? num_gen: global_settings->num_process;

  NSDL2_MESSAGES(NULL, NULL, "perClientQuantityInfoSize = %d", perClientQuantityInfoSize);
  for(i = 0; i < global_settings->num_process; i++)
  {
    int nvm_idx = g_msg_com_con[i].nvm_index;
    NSDL2_MESSAGES(NULL, NULL, "nvm_idx = %d, perClientQuantity[nvm_idx] = %d", nvm_idx, perClientQuantity[nvm_idx]);
    if(nvm_idx == -1)
    {
      NSDL2_MESSAGES(NULL, NULL, "Control Connection: client will not participate in vuser division");
      continue;
    } 
    if(!perClientQuantity[nvm_idx])
    {
      continue;
    }
    VUserQueryInfo *clientVUserQueryInfoPtr = &clientVUserQueryInfo[nvm_idx * perClientQuantityInfoSize];
    send_rtc_msg_to_client(&g_msg_com_con[i], GET_VUSER_LIST, (char *)clientVUserQueryInfoPtr, perClientQuantityInfoSize * sizeof(VUserQueryInfo));
  }
  check_send_rtc_msg_done(GET_VUSER_LIST_ACK, (char *)gVUserInfoTable, 0); 
  return 0;
}

void process_get_vuser_list(Msg_com_con *mccptr, char *msg, int size)
{
  int ret;

  NSDL2_MESSAGES(NULL, NULL, "Method called, control connection size = %d", size);

  CHECK_AND_SET_INVOKER_MCCPTR
 
  totalVUserInfoTableEntries = 0;
  memset(perClientQuantity, 0, global_settings->num_process * sizeof(int)); 
  
  if((ret = parse_and_fill_vuser_list(msg, size)) < 0)
  {
    vuser_rtc_mccptr = NULL;
    VUSER_ERROR_AND_RETURN(VUSER_ERROR_VUSER_LIST);
  }
}

void send_rtc_msg_to_invoker(Msg_com_con *mccptr, int opcode, void *data, int size)
{
   char *send_msg_buf;
   int send_msg_size = 0, grp_idx, i;

   NSDL2_MESSAGES(NULL, NULL, "Method called. opcode = %d, size = %d", opcode, size);
   if(!rtc_vuser_msg_buf_size)
   {
     rtc_vuser_msg_buf_size = RTC_MSG_BUF_BLOCK_SIZE;
     MY_REALLOC(rtc_vuser_msg_buf, rtc_vuser_msg_buf_size, "rtc_vuser_msg_buf", -1);
   }
   send_msg_buf = rtc_vuser_msg_buf + 4;

   switch(opcode)   
   {
     case GET_VUSER_SUMMARY_ACK:
     { 
       GroupVUserSummaryTable *vusummary = data; 
       char *sess_name;
       char *script_name;
       char *grp_name;
       int gen_idx;
       int ramp_down_method;
       int sess_id;
       GroupVUserSummaryTable *vugrp;
       for(grp_idx = 0; grp_idx < total_runprof_entries; grp_idx++)
       {
         sess_name = runprof_table_shr_mem[grp_idx].sess_ptr->sess_name;
         sess_id = runprof_table_shr_mem[grp_idx].sess_ptr->sess_id;
         script_name = get_sess_name_with_proj_subproj_int(sess_name, sess_id, "/");
         grp_name = runprof_table_shr_mem[grp_idx].scen_group_name;
         ramp_down_method = runprof_table_shr_mem[grp_idx].gset.rampdown_method.option; 
         if(loader_opcode == MASTER_LOADER)
         {
            for(gen_idx = 0 ; gen_idx < global_settings->num_process; gen_idx++)
            {
              //Bug 70100 - Discarded generator should also be removed from Manage Vuser(s) UI. 
              if(generator_entry[gen_idx].flags & IS_GEN_INACTIVE)
              { 
                continue;
              }
              vugrp = &vusummary[((gen_idx * (total_runprof_entries + 1))) + grp_idx];
              if(send_msg_size + RTC_MSG_BUF_MAX_LINE_SIZE >= rtc_vuser_msg_buf_size)
              {
		rtc_vuser_msg_buf_size += RTC_MSG_BUF_BLOCK_SIZE;
		MY_REALLOC(rtc_vuser_msg_buf, rtc_vuser_msg_buf_size, "rtc_vuser_msg_buf", -1);
              }
              send_msg_buf = rtc_vuser_msg_buf + 4;
              send_msg_size += sprintf(send_msg_buf + send_msg_size, "%s|%d|%s|%d|%s|%d|%d|%d|%d|%d|%d|%d|%d|%d\n", grp_name, grp_idx, generator_entry[gen_idx].gen_name, gen_idx, script_name, vugrp->num_down_vuser, vugrp->num_running_vuser, vugrp->num_spwaiting_vuser, vugrp->num_paused_vuser, vugrp->num_exiting_vuser, vugrp->num_gradual_exiting_vuser, vugrp->num_stopped_vuser, vugrp->total_vusers, ramp_down_method);
	    }	
         }
         else
         {  
           vugrp = &vusummary[grp_idx]; 
           if(send_msg_size + RTC_MSG_BUF_MAX_LINE_SIZE >= rtc_vuser_msg_buf_size)
           {
	     rtc_vuser_msg_buf_size += RTC_MSG_BUF_BLOCK_SIZE;
	     MY_REALLOC(rtc_vuser_msg_buf, rtc_vuser_msg_buf_size, "rtc_vuser_msg_buf", -1);
           }
           send_msg_buf = rtc_vuser_msg_buf + 4;
           send_msg_size += sprintf(send_msg_buf + send_msg_size, "%s|%d|%s|%d|%s|%d|%d|%d|%d|%d|%d|%d|%d|%d\n", grp_name, grp_idx, "NA", -1, script_name, vugrp->num_down_vuser, vugrp->num_running_vuser, vugrp->num_spwaiting_vuser, vugrp->num_paused_vuser, vugrp->num_exiting_vuser, vugrp->num_gradual_exiting_vuser, vugrp->num_stopped_vuser, vugrp->total_vusers, ramp_down_method);
         }
       } 
       //Remove \n from last line
       if (send_msg_size)
       {
         send_msg_size--;
         send_msg_buf[send_msg_size] = '\0';
       }
       break;
     }

     case GET_VUSER_LIST_ACK:
     {
       if(!size) 
         break;
       VUserInfoTable *vuinfo = data; 
       for(i = 0; i < totalVUserInfoTableEntries; i++)
       {
         if(send_msg_size + RTC_MSG_BUF_MAX_LINE_SIZE >= rtc_vuser_msg_buf_size)
         {
           rtc_vuser_msg_buf_size += RTC_MSG_BUF_BLOCK_SIZE;
           MY_REALLOC(rtc_vuser_msg_buf, rtc_vuser_msg_buf_size, "rtc_vuser_msg_buf", -1);
         }
         send_msg_buf = rtc_vuser_msg_buf + 4;
         if(loader_opcode == MASTER_LOADER)
           send_msg_size += sprintf(send_msg_buf + send_msg_size, "%s|%ld|%s|%s|%d|%s|%d|%s|%s|%d|%d|%d|%s|%s|%d\n", vuinfo[i].vuser_id, vuinfo[i].vptr, vuser_status_str[vuinfo[i].vuser_status], runprof_table_shr_mem[vuinfo[i].grp_idx].scen_group_name, vuinfo[i].grp_idx, generator_entry[vuinfo[i].gen_idx].gen_name, vuinfo[i].gen_idx, vuinfo[i].message, vuinfo[i].vuser_location, vuinfo[i].vuser_elapsed_time, vuinfo[i].session_elapsed_time, vuinfo[i].script_status, vuinfo[i].page_name, vuinfo[i].tx_name, vuinfo[i].msg_opcode);
         else
           send_msg_size += sprintf(send_msg_buf + send_msg_size, "%s|%ld|%s|%s|%d|%s|%d|%s|%s|%d|%d|%d|%s|%s|%d\n", vuinfo[i].vuser_id, vuinfo[i].vptr, vuser_status_str[vuinfo[i].vuser_status], runprof_table_shr_mem[vuinfo[i].grp_idx].scen_group_name, vuinfo[i].grp_idx, "NA", -1, vuinfo[i].message, vuinfo[i].vuser_location, vuinfo[i].vuser_elapsed_time, vuinfo[i].session_elapsed_time, vuinfo[i].script_status, vuinfo[i].page_name, vuinfo[i].tx_name,vuinfo[i].msg_opcode);
       } 
       //Remove \n from last line
       if(totalVUserInfoTableEntries)
       {
         send_msg_size--;
         send_msg_buf[send_msg_size] = '\0';
       }
       break;
     }

     case PAUSE_VUSER_ACK:
     {
       send_msg_size = sprintf(send_msg_buf, "%s", VUSER_PAUSE_DONE_MSG);
       //Log buffer in runtime_changes.log
       if(rtc_log_fp) {
         fprintf(rtc_log_fp, " %s \nSuccessful\n", VUSER_PAUSE_DONE_MSG);
         fclose(rtc_log_fp);
         rtc_log_fp = NULL;
       }

       break;
     }
    
     case RESUME_VUSER_ACK:
     {
       send_msg_size = sprintf(send_msg_buf, "%s", VUSER_RESUME_DONE_MSG);
       //Log buffer in runtime_changes.log
       if(rtc_log_fp) {
         fprintf(rtc_log_fp, " %s \nSuccessful\n", VUSER_RESUME_DONE_MSG);
         fclose(rtc_log_fp);
         rtc_log_fp = NULL;
       }

       break;
     }     
     
     case CAV_MEMORY_MAP:
     {
       send_msg_size = sprintf(send_msg_buf, "%s\n", CAV_MEM_MAP_DONE_MSG);
       NSDL2_MESSAGES(NULL, NULL, "send_msg_size = %d", send_msg_size);

       memcpy(rtc_vuser_msg_buf, &send_msg_size, 4); 
       send_msg_size += 4;
       write_msg(mccptr, rtc_vuser_msg_buf, send_msg_size, 0, ISCALLER_DATA_HANDLER?DATA_MODE:CONTROL_MODE);
       mccptr = NULL;
       return;
       //break;
     }
     case APPLY_ALERT_RTC:
     {
       send_msg_size = sprintf(send_msg_buf, "%s\n", "[Alert RTC Request Processed]");
       NSDL2_MESSAGES(NULL, NULL, "send_msg_size = %d", send_msg_size);
       
       memcpy(rtc_vuser_msg_buf, &send_msg_size, 4);
       send_msg_size += 4;
       write_msg(mccptr, rtc_vuser_msg_buf, send_msg_size, 0, ISCALLER_DATA_HANDLER?DATA_MODE:CONTROL_MODE);
       mccptr = NULL;
       return;
  
     }
 
   }
   NSDL2_MESSAGES(NULL, NULL, "send_msg_size = %d", send_msg_size);

   memcpy(rtc_vuser_msg_buf, &send_msg_size, 4); //TODO:C 
   send_msg_size += 4;
   write_msg(vuser_rtc_mccptr, rtc_vuser_msg_buf, send_msg_size, 0, CONTROL_MODE); //TODO: E
   vuser_rtc_mccptr = NULL;
}


static inline void update_vuser_summary_counters(int child_id, GroupVUserSummaryTable *msg)
{
  int i;

  NSDL2_MESSAGES(NULL, NULL, "Method called, generator/child id = %d", child_id);

  if(child_id < 0 || child_id >= global_settings->num_process)
  {
    NSTL1(NULL, NULL, "Invalid Child id");
    return;
  }

  GroupVUserSummaryTable *clientVUserSummaryTablePtr = &clientVUserSummaryTable[child_id * (total_runprof_entries + 1)];
  for(i = 0 ; i < total_runprof_entries; i++)
  {
    int total_vusers;
    //TODO For debugging purposr only

    NSDL2_MESSAGES(NULL, NULL, "total_vusers = %d, msg->num_down_users = %d, msg->num_running_vuser = %d, msg->num_spwaiting_vuser = %d,"
                               " msg->num_paused_vuser = %d, msg->num_exiting_vuser = %d,  msg->num_gradual_exiting_vuser = %d,"
                               " msg->num_stopped_vuser = %d", 
                               total_vusers, msg->num_down_vuser, msg->num_running_vuser, msg->num_spwaiting_vuser, msg->num_paused_vuser, 
                               msg->num_exiting_vuser, msg->num_gradual_exiting_vuser, msg->num_stopped_vuser);

    gVUserSummaryTable[i].num_down_vuser -= clientVUserSummaryTablePtr[i].num_down_vuser;
    gVUserSummaryTable[i].num_running_vuser -= clientVUserSummaryTablePtr[i].num_running_vuser;
    gVUserSummaryTable[i].num_spwaiting_vuser -= clientVUserSummaryTablePtr[i].num_spwaiting_vuser;
    gVUserSummaryTable[i].num_paused_vuser -= clientVUserSummaryTablePtr[i].num_paused_vuser;
    gVUserSummaryTable[i].num_exiting_vuser -= clientVUserSummaryTablePtr[i].num_exiting_vuser;
    gVUserSummaryTable[i].num_gradual_exiting_vuser -= clientVUserSummaryTablePtr[i].num_gradual_exiting_vuser;
    gVUserSummaryTable[i].num_stopped_vuser -= clientVUserSummaryTablePtr[i].num_stopped_vuser;
    gVUserSummaryTable[i].total_vusers -= clientVUserSummaryTablePtr[i].total_vusers; 
    clientVUserSummaryTablePtr[total_runprof_entries].num_down_vuser -= clientVUserSummaryTablePtr[i].num_down_vuser;    
    clientVUserSummaryTablePtr[total_runprof_entries].num_running_vuser -= clientVUserSummaryTablePtr[i].num_running_vuser;
    clientVUserSummaryTablePtr[total_runprof_entries].num_spwaiting_vuser -= clientVUserSummaryTablePtr[i].num_spwaiting_vuser;
    clientVUserSummaryTablePtr[total_runprof_entries].num_paused_vuser -= clientVUserSummaryTablePtr[i].num_paused_vuser;
    clientVUserSummaryTablePtr[total_runprof_entries].num_exiting_vuser -= clientVUserSummaryTablePtr[i].num_exiting_vuser;
    clientVUserSummaryTablePtr[total_runprof_entries].num_gradual_exiting_vuser -= clientVUserSummaryTablePtr[i].num_gradual_exiting_vuser;
    clientVUserSummaryTablePtr[total_runprof_entries].num_stopped_vuser -= clientVUserSummaryTablePtr[i].num_stopped_vuser;

    if((get_group_mode(i)) == TC_FIX_CONCURRENT_USERS){ 
      total_vusers = msg->num_down_vuser + msg->num_running_vuser + msg->num_spwaiting_vuser + msg->num_paused_vuser + msg->num_exiting_vuser + msg->num_gradual_exiting_vuser + msg->num_stopped_vuser;
      gVUserSummaryTable[i].num_down_vuser +=  msg->num_down_vuser;
      clientVUserSummaryTablePtr[total_runprof_entries].num_down_vuser +=  msg->num_down_vuser;
      clientVUserSummaryTablePtr[i].num_down_vuser =  msg->num_down_vuser;
    }
    else
    {
      total_vusers = msg->num_running_vuser + msg->num_spwaiting_vuser + msg->num_paused_vuser + msg->num_exiting_vuser + msg->num_gradual_exiting_vuser + msg->num_stopped_vuser;
      gVUserSummaryTable[i].num_down_vuser = -1;
      clientVUserSummaryTablePtr[i].num_down_vuser =  -1;

    }

    clientVUserSummaryTablePtr[total_runprof_entries].total_vusers -= total_vusers;

    //Update increment over all counters
    gVUserSummaryTable[i].num_running_vuser += msg->num_running_vuser;
    gVUserSummaryTable[i].num_spwaiting_vuser += msg->num_spwaiting_vuser;
    gVUserSummaryTable[i].num_paused_vuser += msg->num_paused_vuser;
    gVUserSummaryTable[i].num_exiting_vuser += msg->num_exiting_vuser;
    gVUserSummaryTable[i].num_gradual_exiting_vuser += msg->num_gradual_exiting_vuser;
    gVUserSummaryTable[i].num_stopped_vuser += msg->num_stopped_vuser;
    gVUserSummaryTable[i].total_vusers += total_vusers; 
    
    //Update set per Client counters
    clientVUserSummaryTablePtr[i].num_running_vuser = msg->num_running_vuser;
    clientVUserSummaryTablePtr[i].num_spwaiting_vuser = msg->num_spwaiting_vuser;
    clientVUserSummaryTablePtr[i].num_paused_vuser = msg->num_paused_vuser;
    clientVUserSummaryTablePtr[i].num_exiting_vuser = msg->num_exiting_vuser;
    clientVUserSummaryTablePtr[i].num_gradual_exiting_vuser = msg->num_gradual_exiting_vuser;
    clientVUserSummaryTablePtr[i].num_stopped_vuser = msg->num_stopped_vuser;    
    clientVUserSummaryTablePtr[i].total_vusers = total_vusers;    
   
    //update per client overall
    clientVUserSummaryTablePtr[total_runprof_entries].num_running_vuser += msg->num_running_vuser;
    clientVUserSummaryTablePtr[total_runprof_entries].num_spwaiting_vuser += msg->num_spwaiting_vuser;
    clientVUserSummaryTablePtr[total_runprof_entries].num_paused_vuser += msg->num_paused_vuser;
    clientVUserSummaryTablePtr[total_runprof_entries].num_exiting_vuser += msg->num_exiting_vuser;
    clientVUserSummaryTablePtr[total_runprof_entries].num_gradual_exiting_vuser += msg->num_gradual_exiting_vuser;
    clientVUserSummaryTablePtr[total_runprof_entries].num_stopped_vuser += msg->num_stopped_vuser;    
    clientVUserSummaryTablePtr[total_runprof_entries].total_vusers += total_vusers;    

        msg++;
  }
}

/******************************************************************
 * Name    :    process_get_vuser_summary_ack
 * Purpose :    This function is called from wait_forever.c when
                opcode "GET_VUSER_SUMMARY_ACK" will be set from 
                child after sending complete VUser summary.
******************************************************************/
int process_get_vuser_summary_ack(int child_id, GroupVUserSummaryTable *msg)
{

  NSDL2_MESSAGES(NULL, NULL, "Method called, In control connection, received GET_VUSER_SUMMARY_ACK from generator/child id = %d", child_id);

  DEC_VUSER_MSG_COUNT(child_id, 0);

  update_vuser_summary_counters(child_id, msg);

  if(loader_opcode == MASTER_LOADER) 
    check_send_rtc_msg_done(GET_VUSER_SUMMARY_ACK, (char *)clientVUserSummaryTable, clientVUserSummaryTableSize);
  else
    check_send_rtc_msg_done(GET_VUSER_SUMMARY_ACK, (char *)gVUserSummaryTable, gVUserSummaryTableSize);
  return 0;
}

int process_get_vuser_list_ack(int child_id, VUserInfoTable *msg, int size)
{ 
  int row_size;
  NSDL2_MESSAGES(NULL, NULL, "Method called, In control connection, received GET_VUSER_LIST_ACK from generator/child id = %d, size = %d", child_id, size);

  DEC_VUSER_MSG_COUNT(child_id, 0);

  row_size = size / sizeof(VUserInfoTable);
  
  NSDL2_MESSAGES(NULL, NULL, "row_size = %d", row_size);

  if(maxVUserInfoTableEntries < (totalVUserInfoTableEntries + row_size))
  {
    maxVUserInfoTableEntries = totalVUserInfoTableEntries + row_size;
    MY_REALLOC(gVUserInfoTable, maxVUserInfoTableEntries * sizeof(VUserInfoTable), "gVUserInfoTable", -1); 
  }
  if(size)
  { 
    memcpy(&gVUserInfoTable[totalVUserInfoTableEntries], msg, size);
    totalVUserInfoTableEntries += row_size;
  }
  NSDL2_MESSAGES(NULL, NULL, "Control connection totalVUserInfoTableEntries = %d", totalVUserInfoTableEntries);

  check_send_rtc_msg_done(GET_VUSER_LIST_ACK, (char *)gVUserInfoTable, totalVUserInfoTableEntries * sizeof(VUserInfoTable)); 
  return 0;
}

int process_pause_vuser_ack(int child_id, char *msg)
{
  NSDL2_MESSAGES(NULL, NULL, "Method called, In control connection, received PAUSE_VUSER_ACK from generator/child id = %d, msg = %s", child_id, msg);

  DEC_VUSER_MSG_COUNT(child_id, 0);

  update_vuser_summary_counters(child_id, (GroupVUserSummaryTable *)msg);

  check_send_rtc_msg_done(PAUSE_VUSER_ACK, NULL, 0);
  return 0;
}

int process_resume_vuser_ack(int child_id, char *msg)
{
  NSDL2_MESSAGES(NULL, NULL, "Method called, In control connection, received RESUME_VUSER_ACK from generator/child id = %d, msg = %s", child_id, msg);

  DEC_VUSER_MSG_COUNT(child_id, 0);

  update_vuser_summary_counters(child_id, (GroupVUserSummaryTable *)msg);

  check_send_rtc_msg_done(RESUME_VUSER_ACK, NULL, 0);
  return 0;
}

void process_stop_vuser_ack(int child_id, char *msg)
{
  NSDL2_MESSAGES(NULL, NULL, "Method called, In control connection, received STOP_VUSER_ACK from generator/child id = %d, msg = %s", child_id, msg);

  update_vuser_summary_counters(child_id, (GroupVUserSummaryTable *)msg);

  check_send_rtc_msg_done(STOP_VUSER_ACK, NULL, 0);
}
/******************************************************************
 * Name    :    send_rtc_msg_to_parent
 * Purpose :     
******************************************************************/
void send_rtc_msg_to_parent_or_master(int opcode, char *data, int size) //TODO: Simplify logic
{
  Msg_com_con *msg_com_ptr;
  int data_size;

  NSDL1_MESSAGES(NULL, NULL, "Method called, opcode = %d, size = %d", opcode, size);

  data_size = size + sizeof(RTC_VUser);

  if(data_size > rtc_send_recv_msg_size)
  {
    rtc_send_recv_msg_size = data_size;
    MY_REALLOC(rtc_send_recv_msg, rtc_send_recv_msg_size, "RTC_VUser", -1);
    rtc_send_recv_msg->data = (char*)rtc_send_recv_msg + sizeof(RTC_VUser);
  }
  rtc_send_recv_msg->opcode = opcode;
  rtc_send_recv_msg->gen_rtc_idx = g_vuser_msg_seq_num;
  if(loader_opcode == CLIENT_LOADER)
  {
    rtc_send_recv_msg->child_id = g_parent_idx;
    msg_com_ptr = g_master_msg_com_con;
  }
  else
  {
    rtc_send_recv_msg->child_id = my_port_index;
    msg_com_ptr = &g_child_msg_com_con;
  }  
  if(size)
    memcpy(rtc_send_recv_msg->data, data, size);

  rtc_send_recv_msg->msg_len =  data_size - sizeof(int);
  write_msg(msg_com_ptr, (char *)rtc_send_recv_msg, data_size, 0, CONTROL_MODE); //TODO: handle error
}

/******************************************************************
 * Name    :    ns_process_vuser_summary
 * Purpose :    This function will send acknowledgement to parent 
                and set opcode to "GET_VUSER_SUMMARY_ACK" 
******************************************************************/
void ns_process_vuser_summary()
{
  NSDL2_MESSAGES(NULL, NULL, "Method called");
  SEND_MSG_TO_PARENT(GET_VUSER_SUMMARY_ACK, (char*)gVUserSummaryTable, gVUserSummaryTableSize);
}

int get_vuser_status(VUser* vptr)
{
  if(vptr->flags & NS_VUSER_GRADUAL_EXITING)
    return NS_VUSER_RTC_GRADUAL_EXITING;

  if(vptr->flags & NS_VUSER_RAMPING_DOWN)
    return NS_VUSER_RTC_EXITING;

  if((vptr->vuser_state == NS_VUSER_ACTIVE) || (vptr->vuser_state == NS_VUSER_THINKING) || 
                     (vptr->vuser_state == NS_VUSER_SESSION_THINK) || (vptr->vuser_state == NS_VUSER_BLOCKED))
    return NS_VUSER_RTC_RUNNING;
  
  if((vptr->vuser_state == NS_VUSER_SYNCPOINT_WAITING))
    return NS_VUSER_RTC_SYNC_POINT;
  
  if((vptr->vuser_state == NS_VUSER_PAUSED))
    return NS_VUSER_RTC_PAUSED;
     
  return NS_VUSER_RTC_STOPPED;
}

static inline void ns_pause_vuser_quantity(int grp_idx, int quantity)
{
  VUser *vptr = gBusyVuserHead;
  int vuser_state;

  NSDL2_MESSAGES(NULL, NULL, "Method called, grp_idx = %d, quantity = %d", grp_idx, quantity); 

  while(quantity && vptr)
  {
    if(grp_idx != -1)
    {
      if(vptr->group_num != grp_idx)
      {
        vptr = vptr->busy_next;
        continue;
      }
    }/*
    if(vptr->flags & NS_VUSER_RAMPING_DOWN)
    {
      vptr = vptr->busy_next;
      continue;
    }*/ 
    vuser_state = get_vuser_status(vptr);
    if(vuser_state != NS_VUSER_RTC_RUNNING)
    {
      vptr = vptr->busy_next;
      continue;
    }
    if(vptr->flags & NS_VUSER_USER_CTX)
    {
      vptr->flags |= NS_VUSER_PAUSE;
      vptr = vptr->busy_next;
      continue;
    }
    pause_vuser(vptr);
    vptr = vptr->busy_next;
    quantity--;
  }
  NSDL2_MESSAGES(NULL, NULL, "After pause quantity = %d", quantity); 
}

void ns_process_pause_vuser(void *msg, int size)
{
  int num_row,i; 
  int vuser_state;
  VUserPRSInfo *vupause;

  NSDL2_MESSAGES(NULL, NULL, "Method called, size = %d", size);
  vupause = (VUserPRSInfo  *)msg;
  num_row = (size/sizeof(VUserPRSInfo));

  NSDL2_MESSAGES(NULL, NULL, "num_row = %d", num_row);
  for(i = 0; i < num_row; i++)
  {
    NSDL2_MESSAGES(NULL, NULL, "grp_idx = %d, quantity = %d, gen_idx = %d", vupause[i].grp_idx, vupause[i].quantity, vupause[i].gen_idx);

    if(vupause[i].quantity)
      ns_pause_vuser_quantity(vupause[i].grp_idx, vupause[i].quantity);
    else if(vupause[i].vuser_idx >= 0)
    {
      if(((VUser *)(vupause[i].vptr))->flags == NS_VUSER_USER_CTX)
        ((VUser *)(vupause[i].vptr))->flags |= NS_VUSER_PAUSE;
      else{
        VUser *vptr = (VUser *)(vupause[i].vptr);
        if(vptr->group_num != vupause[i].grp_idx)
          continue;
        vuser_state = get_vuser_status(vptr);
        if(vuser_state != NS_VUSER_RTC_RUNNING)
          continue;
        pause_vuser(vptr);
      }      
    }
  }
  SEND_MSG_TO_PARENT(PAUSE_VUSER_ACK, (char*)gVUserSummaryTable, gVUserSummaryTableSize);
}

static inline void ns_resume_vuser_quantity(int in_grp_idx, int quantity)
{
  u_ns_ts_t now;
  now = get_ms_stamp();  
  int grp_count,g;
  VUser *vptr;
  NSDL2_MESSAGES(NULL, NULL, "Method called, grp_idx = %d, quantity = %d", in_grp_idx, quantity);

  if(!vuser_pause_list) 
    return;

  if(in_grp_idx == -1)
    grp_count = total_runprof_entries;
  else
    grp_count = 1;

  for(g = 0; g < grp_count; g++)
  {
    int grp_idx = (in_grp_idx == -1)?g:in_grp_idx;
    vptr = vuser_pause_list[grp_idx].vptr_head;
    while(quantity && vptr)
    {
      if(grp_idx != -1)
      {
        if(vptr->group_num != grp_idx)
        {
          vptr = vptr->pause_vuser_next;
          continue;
        }
      }
      resume_vuser(vptr, now); //This vptr will remove from pause list and next will become head
      vptr = vuser_pause_list[grp_idx].vptr_head;
      quantity--;
    }
  }
  NSDL2_MESSAGES(NULL, NULL, "quantity = %d", quantity);
}

//This function will return remaining quantity i.e non-paused users
int ns_rampdown_vuser_quantity(int in_grp_idx, int quantity, u_ns_ts_t now, int runtime_flag)
{
  int grp_count,g;
  VUser *vptr;
  NSDL2_MESSAGES(NULL, NULL, "Method called, grp_idx = %d, quantity = %d", in_grp_idx, quantity);

  if(!vuser_pause_list)
  {
    NSDL2_MESSAGES(NULL, NULL, "paused list is null");
    return quantity;
  }
 
  if(in_grp_idx == -1)
    grp_count = total_runprof_entries;
  else
    grp_count = 1;

  Schedule *schedule;
  int phase_idx;
  Phases *ph;
  for(g = 0; g < grp_count; g++)
  {
    int grp_idx = (in_grp_idx == -1)?g:in_grp_idx;
    vptr = vuser_pause_list[grp_idx].vptr_head;
    while(quantity && vptr)
    {
      if(in_grp_idx != -1)
      {
        if(vptr->group_num != in_grp_idx)
        {
          vptr = vptr->pause_vuser_next;
          continue;
        }
      }
      vptr->flags |= NS_VUSER_RAMPING_DOWN; 
      VUSER_INC_EXIT(vptr);
      //vptr->operation = VUT_NO_TASK;
      stop_user_immediately(vptr, now);
      vptr = vuser_pause_list[grp_idx].vptr_head;
      quantity--;
      if(global_settings->schedule_by == SCHEDULE_BY_SCENARIO) {
        schedule = v_port_entry->scenario_schedule;
        phase_idx = schedule->phase_idx;
        ph = &(schedule->phase_array[phase_idx]);
        int *per_grp_qty = ph->phase_cmd.ramp_up_phase.per_grp_qty;
        if(!runtime_flag)
          (per_grp_qty[grp_idx])--; 
      }
      else {
        schedule = &v_port_entry->group_schedule[grp_idx];
      }
      schedule->cur_vusers_or_sess--;
    }
  }
  NSDL2_MESSAGES(NULL, NULL, "quantity = %d", quantity);
  return quantity;
}


static inline void ns_stop_vuser_quantity(int grp_idx, int quantity, int g_exit)
{
  VUser *vptr;
  int vuser_state;

  NSDL2_MESSAGES(NULL, NULL, "Method called, grp_idx = %d, quantity = %d", grp_idx, quantity);
  //First remove from pause list, then from busy list 
  vptr = vuser_pause_list->vptr_head;
  while(quantity && vptr)
  { 
    if(grp_idx != -1)
    {
      if(vptr->group_num != grp_idx)
      {
        vptr = vptr->pause_vuser_next;
        continue;
      }
    }
    //Stopping this user
    vptr->flags |= NS_VUSER_RAMPING_DOWN;
    if((g_exit) && runprof_table_shr_mem[grp_idx].gset.rampdown_method.option != 2) //0=SessionComplete,1=PageComplete,2=StopImmediately
      vptr->flags |= NS_VUSER_GRADUAL_EXITING; //This will be based on RampDown setting 
    vptr = vuser_pause_list->vptr_head;
    quantity--;
  }
  
  vptr = gBusyVuserHead;
  while(quantity && vptr)
  {
    if(grp_idx != -1)
    {
      if(vptr->group_num != grp_idx)
      {
        vptr = vptr->busy_next;
        continue;
      }
    }
    vuser_state = get_vuser_status(vptr);
    if(vuser_state == NS_VUSER_RTC_EXITING || vuser_state == NS_VUSER_RTC_GRADUAL_EXITING)
    {
      vptr = vptr->busy_next;
      continue;
    }

    //Stopping this user
    vptr->flags |= NS_VUSER_RAMPING_DOWN;
    if((g_exit) && runprof_table_shr_mem[grp_idx].gset.rampdown_method.option != 2) //0=SessionComplete,1=PageComplete,2=StopImmediately
      vptr->flags |= NS_VUSER_GRADUAL_EXITING; //This will be based on RampDown setting 
    vptr = vptr->busy_next;
    quantity--;
  }
}

void ns_process_resume_vuser(void *msg, int size)
{
  int num_row,i;
  VUserPRSInfo *vuresume;

  NSDL2_MESSAGES(NULL, NULL, "Method called, size = %d", size);
  vuresume = (VUserPRSInfo *)msg;
  num_row = (size/sizeof(VUserPRSInfo));
  u_ns_ts_t now;
  now = get_ms_stamp();

  NSDL2_MESSAGES(NULL, NULL, "num_row = %d", num_row);
  for(i = 0; i < num_row; i++)
  {
    NSDL2_MESSAGES(NULL, NULL, "grp_idx = %d, quantity = %d, gen_idx = %d", vuresume[i].grp_idx, vuresume[i].quantity, vuresume[i].gen_idx);

    if(vuresume[i].quantity)
      ns_resume_vuser_quantity(vuresume[i].grp_idx, vuresume[i].quantity);
    else if(vuresume[i].vuser_idx >= 0)
      resume_vuser((VUser *)(vuresume[i].vptr), now);
    
  }
  SEND_MSG_TO_PARENT(RESUME_VUSER_ACK, (char*)gVUserSummaryTable, gVUserSummaryTableSize);
}

void ns_process_stop_vuser(void *msg, int size)
{   
  int num_row,i; 
  VUserPRSInfo *vustop;
      
  NSDL2_MESSAGES(NULL, NULL, "Method called, size = %d", size);
  vustop = (VUserPRSInfo *)msg;
  num_row = (size/sizeof(VUserPRSInfo));

  NSDL2_MESSAGES(NULL, NULL, "num_row = %d", num_row);
  for(i = 0; i < num_row; i++)
  { 
    NSDL2_MESSAGES(NULL, NULL, "grp_idx = %d, quantity = %d, gen_idx = %d", vustop[i].grp_idx, vustop[i].quantity, vustop[i].gen_idx);
    
    if(vustop[i].quantity)
      ns_stop_vuser_quantity(vustop[i].grp_idx, vustop[i].quantity, 0); //TODO g_exit
    else{
      VUser *vptr = (VUser *)vustop[i].vptr;
      //Stopping this user
      vptr->flags |= NS_VUSER_RAMPING_DOWN;
      if(runprof_table_shr_mem[vustop[i].grp_idx].gset.rampdown_method.option != 2) //0=SessionComplete,1=PageComplete,2=StopImmediately
        vptr->flags |= NS_VUSER_GRADUAL_EXITING; //This will be based on RampDown setting 
     }
  }
  SEND_MSG_TO_PARENT(STOP_VUSER_ACK, (char*)gVUserSummaryTable, gVUserSummaryTableSize);
}

static inline void fill_vuser_list(int grp_idx, int offset, int limit, int status)
{
  VUser *vptr = gBusyVuserHead;
  u_ns_ts_t now;
  now = get_ms_stamp();  
  int vuser_state;

  NSDL1_MESSAGES(NULL, NULL, "Method called, grp_idx = %d, offset = %d, limit = %d, status = %d", grp_idx, offset, limit, status); 

  if(maxVUserInfoTableEntries < (totalVUserInfoTableEntries + limit))
  {
    maxVUserInfoTableEntries = totalVUserInfoTableEntries + limit;
    MY_REALLOC(gVUserInfoTable, maxVUserInfoTableEntries * sizeof(VUserInfoTable), "gVUserInfoTable", -1);
  }

  while(limit && vptr)
  {
    if(grp_idx != -1)
    {
      if(vptr->group_num != grp_idx)
      {
        vptr = vptr->busy_next;
        continue;
      }
    }
    vuser_state = get_vuser_status(vptr);
    if(status != -1)
    {
      if(vuser_state != status)
      {
        vptr = vptr->busy_next;
        continue;
      }
    }
    if(offset > 0)
    {
       offset--;
       vptr = vptr->busy_next;
       continue;
    }
    if(g_parent_idx != -1)
      snprintf(gVUserInfoTable[totalVUserInfoTableEntries].vuser_id, MAX_VUSER_ID_LEN,"%d:%d:%d", g_parent_idx, my_port_index, vptr->user_index);
    else
      snprintf(gVUserInfoTable[totalVUserInfoTableEntries].vuser_id, MAX_VUSER_ID_LEN,"%d:%d", my_port_index, vptr->user_index);
    gVUserInfoTable[totalVUserInfoTableEntries].gen_idx = g_parent_idx;
    gVUserInfoTable[totalVUserInfoTableEntries].session_elapsed_time = (now - vptr->started_at);
    gVUserInfoTable[totalVUserInfoTableEntries].vuser_elapsed_time = 0; 
    gVUserInfoTable[totalVUserInfoTableEntries].vuser_status = vuser_state;
    gVUserInfoTable[totalVUserInfoTableEntries].script_status = vptr->sess_status;  
    gVUserInfoTable[totalVUserInfoTableEntries].grp_idx = vptr->group_num; 
    gVUserInfoTable[totalVUserInfoTableEntries].msg_opcode = vptr->vuser_state; 
    gVUserInfoTable[totalVUserInfoTableEntries].vptr = (long)vptr;
    if(vptr->flags & NS_VUSER_PAUSE) 
      strncpy(gVUserInfoTable[totalVUserInfoTableEntries].message, "Pause in progress", MAX_MSG_LEN);
    else  
      strncpy(gVUserInfoTable[totalVUserInfoTableEntries].message, "-", MAX_MSG_LEN);
    gVUserInfoTable[totalVUserInfoTableEntries].message[MAX_MSG_LEN] = '\0';
    strncpy(gVUserInfoTable[totalVUserInfoTableEntries].page_name, vptr->cur_page?vptr->cur_page->page_name:"-",MAX_MSG_LEN);
    gVUserInfoTable[totalVUserInfoTableEntries].page_name[MAX_MSG_LEN] = '\0';
    strncpy(gVUserInfoTable[totalVUserInfoTableEntries].vuser_location, vptr->location?vptr->location->name:"-",MAX_MSG_LEN); 
    gVUserInfoTable[totalVUserInfoTableEntries].vuser_location[MAX_MSG_LEN] = '\0';
    TxInfo *tx_ptr = (TxInfo *) vptr->tx_info_ptr;
    char *tx_name = tx_ptr?nslb_get_norm_table_data(&normRuntimeTXTable, tx_ptr->hash_code):"-"; 
    strncpy(gVUserInfoTable[totalVUserInfoTableEntries].tx_name, tx_name,MAX_MSG_LEN);
    gVUserInfoTable[totalVUserInfoTableEntries].tx_name[MAX_MSG_LEN] = '\0';
    
    vptr = vptr->busy_next;
    limit--;
    totalVUserInfoTableEntries++;
  }
  NSDL2_MESSAGES(NULL, NULL, "After list limit = %d", limit); 
}

void ns_process_vuser_list(void *msg, int size)
{
  int num_row,i;
  VUserQueryInfo *vuquery;

  NSDL1_MESSAGES(NULL, NULL, "Method called, size = %d", size);
  vuquery = (VUserQueryInfo  *)msg;
  num_row = (size/sizeof(VUserQueryInfo));
  
  totalVUserInfoTableEntries = 0;

  NSDL2_MESSAGES(NULL, NULL, "num_row = %d", num_row);
  for(i = 0; i < num_row; i++)
  {
    NSDL2_MESSAGES(NULL, NULL, "grp_idx = %d, offset = %d, limit = %d, status = %d, gen_idx = %d", vuquery[i].grp_idx, vuquery[i].offset, vuquery[i].limit, vuquery[i].status, vuquery[i].gen_idx);
 
    fill_vuser_list(vuquery[i].grp_idx, vuquery[i].offset, vuquery[i].limit, vuquery[i].status);
  }
  SEND_MSG_TO_PARENT(GET_VUSER_LIST_ACK, (char*)gVUserInfoTable, (totalVUserInfoTableEntries * sizeof(VUserInfoTable)));
}
