/* Name    : ns_sync_point.c
 * Purpose :
 *
 */  

#include "ns_cache_include.h"
#include "ns_http_script_parse.h"
#include "ns_vuser_tasks.h"
#include "ns_sync_point.h"
#include "ns_session.h"
#include "tmr.h"
#include "ns_trans_parse.h"
#include "ns_trans.h"
#include "ns_error_codes.h"
#include "ns_string.h"
#include "ns_msg_def.h"
#include "ns_child_thread_util.h"
#include "ns_vuser_ctx.h"
#include "ns_vuser.h"
#include "ns_schedule_phases_parse.h"
#include "netomni/src/core/ni_scenario_distribution.h"
#include "ns_group_data.h"
#include "ns_nvm_njvm_msg_com.h"
#include "ns_script_parse.h"

#define SLOT_OF_VUSERS_TO_FREE 1

SP_user *sp_user_tbl = NULL;
SPGroupTable_shr *spGroupTable_shr = NULL;
SPTableEntry_shr *syncPntTable_shr = NULL;
SPApiTableLogEntry_shr *spApiTable_shr = NULL;
int *all_grp_entries_shr = NULL;
void sync_point_remove_users_frm_linked_list(VUser *vptr, u_ns_ts_t now)
{
  int sp_tbl_id, k;
  int sp_flag = 0;
  VUser *cur_vuser_head = NULL;
  VUser *prev_vuser_head = NULL;
  VUser *next_vuser_head = NULL;

  NSDL1_SP(NULL, NULL, "Method Called");

  if(spGroupTable_shr[vptr->sync_point_id].sync_group_name_as_int == -1)
    sp_tbl_id = all_grp_entries_shr[(vptr->sync_point_id * (total_runprof_entries + 1)) + (spGroupTable_shr[vptr->sync_point_id].sync_group_name_as_int + 1)];
  else
    sp_tbl_id = all_grp_entries_shr[(vptr->sync_point_id * (total_runprof_entries + 1)) + (vptr->group_num + 1)];

  cur_vuser_head =  sp_user_tbl[sp_tbl_id].vptr_head;
  prev_vuser_head = sp_user_tbl[sp_tbl_id].vptr_head;

  /*Here incrementing pointers: cur_vuser_head & next_vuser_head according to for loop, but ptr prev_vuser_head will be incremented acc. to sp_flag because sp_flag         gives an idea of index where vptr & cur_vuser_head matched so that we can get index of previous node.*/

  for(; cur_vuser_head != NULL; cur_vuser_head = cur_vuser_head->sp_vuser_next)
  {
    NSDL1_SP(NULL, NULL, "vptr = %p, cur_vuser_head = %p, prev_vuser_head = %p, next_vuser_head = %p, sp_flag = %d \n", vptr, cur_vuser_head, prev_vuser_head, next_vuser_head, sp_flag);

    next_vuser_head = cur_vuser_head->sp_vuser_next;

    if(cur_vuser_head == vptr)
    {
      for(k = 0; k < sp_flag - 1; k++)
      {
        prev_vuser_head = prev_vuser_head->sp_vuser_next;
      }

      NSDL2_SP(NULL, NULL, "sp_flag = %d, prev_vuser_head = %p, next_vuser_head = %p, cur_vuser_head = %p, vptr = %p",                                                                              sp_flag, prev_vuser_head, next_vuser_head, cur_vuser_head, vptr);

      prev_vuser_head->sp_vuser_next = next_vuser_head;
    }
    sp_flag++;
  }
}

static int sync_point_chk_for_ramp_down(VUser *vptr, u_ns_ts_t now)
{
  //int ret;

  if (vptr->flags & NS_VUSER_RAMPING_DOWN) 
  {
    NSDL1_SP(NULL, NULL, "User is marked for ramp down for new session. So doing cleanup. gset.rampdown_method.mode = %d",                                                            runprof_table_shr_mem[vptr->group_num].gset.rampdown_method.mode);

     /*Free user data and user slot by calling user_cleanup
       as this user cannot do any more sessions
       user_cleanup(vptr, now);
       VUSER_DEC_ACTIVE(vptr);*/

    if(runprof_table_shr_mem[vptr->group_num].gset.rampdown_method.mode == RDM_MODE_ALLOW_CURRENT_SESSION_COMPLETE)  //Allow curent session to complete
      //ret = stop_user_and_allow_cur_sess_complete(vptr, now);
      stop_user_and_allow_cur_sess_complete(vptr, now);
    else if(runprof_table_shr_mem[vptr->group_num].gset.rampdown_method.mode == RDM_MODE_ALLOW_CURRENT_PAGE_COMPLETE)  //Allow curent page to complete
      //ret = stop_user_and_allow_current_page_to_complete(vptr, now);
      stop_user_and_allow_current_page_to_complete(vptr, now);
    else if(runprof_table_shr_mem[vptr->group_num].gset.rampdown_method.mode == RDM_MODE_STOP_IMMEDIATELY)  // Stop user immediate with status
    {
      //ret = stop_user_immediately(vptr,  now);
      stop_user_immediately(vptr,  now);
      return 1;
    }
  }

  return 0;
}

//Function for creating sync point release message
SP_msg* create_sync_point_release_msg(SPTableEntry_shr *syncPntTbl) 
{
  int step_size = (sizeof(int) * syncPntTbl->release_schedule_step);
  int msg_size = sizeof(SP_msg) + (2 * step_size); 
  int *step_duration = syncPntTbl->release_schedule_step_duration;
  static SP_msg *my_msg = NULL;
  static int my_msg_size = 0;
  int duration;

  NSDL4_SP(NULL, NULL, "Method Called, step_size = %d, msg_size = %d", step_size, msg_size); 

  if(msg_size > my_msg_size)
  {
    MY_REALLOC(my_msg, msg_size, "SyncPoint Release Message", -1);
    my_msg_size = msg_size;
  }

  my_msg->sp_rel_step = syncPntTbl->release_schedule_step;
  my_msg->sync_point_id = syncPntTbl->sp_grp_tbl_idx;
  my_msg->grp_idx = syncPntTbl->sync_group_name_as_int;
  my_msg->sync_point_type = syncPntTbl->sp_type;
  
  if(step_size)  
  {
    if(syncPntTbl->release_schedule == SP_RELEASE_SCH_RATE)
    {
      duration = convert_rate_to_duration(syncPntTbl->total_accu_usrs, syncPntTbl->release_schedule_step_duration[0]);  
      // There can be the case:: Vusers are less, rate is high so (Vusers * 60/Rate) computes to 0
      if(duration == 0)
         duration = 1; // Atleast 1 sec to provide for NVM to release all Vusers
      step_duration = &duration;
    }
    my_msg->sp_rel_step_duration = (int*)((char *)my_msg + sizeof(SP_msg));
    my_msg->sp_rel_step_quantity = (int*)((char *)my_msg + sizeof(SP_msg) + step_size);
    memcpy(my_msg->sp_rel_step_duration, step_duration, step_size);
    memcpy(my_msg->sp_rel_step_quantity, syncPntTbl->release_schedule_step_quantity, step_size);
  }
  return my_msg;
}

static void release_sync_point_frm_time_out ( ClientData client_data, u_ns_ts_t now, char *to_reason, int timeout_type)
{
  char grp_name[SP_MAX_DATA_LINE_LENGTH];
  SPTableEntry_shr *syncPntTbl; 
  syncPntTbl = (SPTableEntry_shr *)client_data.p; 
  int fd = 0;
  SP_msg *my_msg;

  my_msg = create_sync_point_release_msg(syncPntTbl);
    
  if(syncPntTbl->sync_group_name_as_int == -1)    // group name
    strcpy(grp_name, "ALL");
  else
    strcpy(grp_name, runprof_table_shr_mem[syncPntTbl->sync_group_name_as_int].scen_group_name);

  release_sync_point(fd, syncPntTbl->self_idx, my_msg, syncPntTbl->sync_pt_name, grp_name, to_reason, timeout_type);
}


//Make a rapper for releasing sync poin on time out
//  Need two - One for inter and one for overall
//   In inter -> u need to delete overal all
//   In overall -> u need to delete inter 
static void sync_point_release_iato_cb(ClientData client_data, u_ns_ts_t now)
{ 
  NSDL4_SP(NULL, NULL, "Method Called: Callback function due to IATO"); 
  release_sync_point_frm_time_out (client_data, now, "Timeout [IATO]", REL_IATO);
}

static void sync_point_release_oato_cb(ClientData client_data, u_ns_ts_t now)
{ 
   NSDL4_SP(NULL, NULL, "Method Called: Callback function due to OATO"); 
   release_sync_point_frm_time_out (client_data, now, " Timeout [OATO]", REL_OATO);
}

void sync_point_release_release_type_cb(ClientData client_data, u_ns_ts_t now)
{
   SPTableEntry_shr *syncPntTbl;
   syncPntTbl = (SPTableEntry_shr *)client_data.p;
   NSDL4_SP(NULL, NULL, "Method Called: Callback function due to REL_RTTO"); 
   if(syncPntTbl->release_type == SP_RELEASE_TYPE_TIME) 
      release_sync_point_frm_time_out (client_data, now, "Absolute Time Completed", REL_RTTO);
   else if(syncPntTbl->release_type == SP_RELEASE_TYPE_PERIOD)
      release_sync_point_frm_time_out (client_data, now, "Period Completed", REL_RTTO);
}

static void sync_point_release_release_done_cb(ClientData client_data, u_ns_ts_t now)
{
   
   SPTableEntry_shr *SPTE = (SPTableEntry_shr *)client_data.p;
   NSDL4_SP(NULL, NULL, "Method Called");
   if (loader_opcode != CLIENT_LOADER)  // donot write in event.log in case of generator
   {
      char grp_name[128]={0};
      char msg_buffer[128]={0};
      char release_count[128]={0}; 
      sprintf(msg_buffer, "Syncpoint Released Successfully");

      if(SPTE->sync_group_name_as_int == -1)    // group name
         strcpy(grp_name, "ALL");
      else
         strcpy(grp_name, runprof_table_shr_mem[SPTE->scen_grp_idx].scen_group_name);

      // Write in event.log  
      sprintf(release_count,"%d",SPTE->release_count);
      NS_EL_4_ATTR(EID_SP_RELEASE, -1, -1, EVENT_SYNC_POINT, EVENT_INFORMATION,
            grp_name, ret_sp_name(SPTE->sp_type), SPTE->sync_pt_name,
            release_count, msg_buffer);
   }
   if(SPTE->next_release == 1)
      SPTE->sp_actv_inactv = SP_ACTIVE;
   else
      SPTE->sp_actv_inactv = SP_INACTIVE;

   create_sync_point_summary_file();
}

void init_sp_user_table ()
{
  NSDL1_SP(NULL, NULL, "Method Called");
  MY_MALLOC_AND_MEMSET(sp_user_tbl, sizeof(SP_user) * total_malloced_syncpoint_entries, "sync point user table", -1);
}

void copy_sp_grp_tbl_into_shr_memory()
{
  NSDL1_SP(NULL, NULL, "Method Called");
  spGroupTable_shr = (SPGroupTable_shr*) do_shmget(sizeof(SPGroupTable_shr) * total_sp_grp_entries, "Group sync point shared"); 
  memcpy(spGroupTable_shr, spGroupTable, (sizeof(SPGroupTable_shr) * total_sp_grp_entries));
}

void copy_sp_tbl_into_shr_memory()
{
  NSDL1_SP(NULL, NULL, "Method Called");
  syncPntTable_shr = (SPTableEntry_shr*) do_shmget(sizeof(SPTableEntry_shr) * (max_syncpoint_entries + SP_DELTA_ENTRIES), "Group sync point shared");
  memcpy(syncPntTable_shr, syncPntTable, (sizeof(SPTableEntry_shr) * max_syncpoint_entries));
  g_sync_point_table = syncPntTable_shr;
}

void copy_all_grp_tbl_into_shr_memory()
{
  NSDL1_SP(NULL, NULL, "Method Called");
  all_grp_entries_shr = (int *)do_shmget(sizeof(int) * (total_sp_grp_entries * (total_runprof_entries + 1)), "Group sync point shared");
  memcpy(all_grp_entries_shr, all_grp_entries, (sizeof(int) * (total_sp_grp_entries * (total_runprof_entries + 1))));
}

void copy_sp_api_tbl_into_shr_memory()
{
  NSDL1_SP(NULL, NULL, "Method Called");
  spApiTable_shr = (SPApiTableLogEntry_shr*) do_shmget(sizeof(SPApiTableLogEntry_shr) * total_sp_api_found, "sync point api shared"); 
  memcpy(spApiTable_shr, spApiTable, (sizeof(SPApiTableLogEntry_shr) * total_sp_api_found));
}

static void send_msg(SP_msg *rcv_msg, int send_to_all, int fd)
{
  int i = 0;

  for(i = 0; i < global_settings->num_process; i++)
  {
    NSDL2_SP(NULL, NULL, "Standalone netstorm: writing msg at g_msg_com_con[%d].ip = %s, fd = %d", 
                          i, g_msg_com_con[i].ip, g_msg_com_con[i].fd);

    if(rcv_msg->opcode == SP_RELEASE)
      rcv_msg->msg_len = sizeof(SP_msg) + (2 * sizeof(int) * rcv_msg->sp_rel_step) - sizeof(int);
    else
      rcv_msg->msg_len = sizeof(SP_msg) - sizeof(int);

    if(send_to_all)
    {
      //Send to all NVMs
      write_msg(&g_msg_com_con[i], (char *)rcv_msg, rcv_msg->msg_len + sizeof(int), 0, CONTROL_MODE);
    }  
    else
    {
      //Send to a particular NVM
      if(g_msg_com_con[i].fd == fd){
	write_msg(&g_msg_com_con[i], (char *)rcv_msg, rcv_msg->msg_len + sizeof(int), 0, CONTROL_MODE);
        break;
      }
    }
  }
}

void process_sp_wait_msg_frm_parent(SP_msg *rcv_msg)
{

  if (loader_opcode == CLIENT_LOADER) 
  {  //if generator then send msg to nvm and return
    NSDL1_SP(NULL, NULL, "Got message by generator parent, forwarding to genartor NVM");
    return(send_msg(rcv_msg, 0, rcv_msg->nvm_fd));  
  }

  NSDL1_SP(NULL, NULL, "Got message from NVM");
  int sp_id = rcv_msg->sync_point_id;
  
  //int sp_type = rcv_msg->sync_point_type;
  VUser *my_vuser = (VUser*)rcv_msg->vuser;

  //Got wait msg from parent, this user will go in NVM's user list  

  INCREMENTING_VUSER_SYNCPOINT_WAITING_COUNTERS_WITHOUT_STATE_CHANGE(my_vuser);

  if(sp_user_tbl[sp_id].vptr_head == NULL)//Linked list is empty
  {
    sp_user_tbl[sp_id].vptr_head = my_vuser;
    //Need to put NULL here only coz second time node will be added 
    //at head so no need to fill next with NULL
    my_vuser->sp_vuser_next = NULL; 
  }
  else
  {
    my_vuser->sp_vuser_next = sp_user_tbl[sp_id].vptr_head;
    sp_user_tbl[sp_id].vptr_head = my_vuser;
  }
  /* To maintain count on each NVM of how many Vusers taking part in syncpoint */
  sp_user_tbl[sp_id].nvm_sync_point_users++;
}

void process_sp_continue_msg_frm_parent(SP_msg *rcv_msg){

  if (loader_opcode == CLIENT_LOADER) 
  {  //if generator then send msg to nvm and return
    return(send_msg(rcv_msg, 0, rcv_msg->nvm_fd));  
  }

  int sp_type = rcv_msg->sync_point_type;
  VUser *vptr = ((VUser *)rcv_msg->vuser);

  NSDL1_SP(NULL, NULL, "Method Called, rcv_msg = %p, vptr = %p, sp_type = %d", rcv_msg, rcv_msg->vuser, sp_type);

  //Here no need to do any thing as on continue
  //NVM will continue with vuser and will makae vuser to wait.
  u_ns_ts_t now = get_ms_stamp();

  //We are incrementing counter here because function sync_point_chk_for_ramp_down ()
  //is called fot relase sync point also. in release sysncopoint we are decrementing 
  //syncpoint waiting users. but in continue message we dont need to decrement syncpoint waitin counter, so incremeting before calling function
  INCREMENTING_VUSER_SYNCPOINT_WAITING_COUNTERS_WITHOUT_STATE_CHANGE(vptr);

  // Check if user was marked ramped down while waiting for reply from parent
  if(sync_point_chk_for_ramp_down(vptr, now) == 1)
    return; // Return as user is cleaned up

  // Change user state to active as we set to sync point state before sending message to parent
  VUSER_SYNCPOINT_WAITING_TO_ACTIVE(vptr);
  

  if(sp_type == SP_TYPE_START_PAGE)
    nsi_web_url_int(vptr, now);
  else if(sp_type == SP_TYPE_START_SCRIPT)
  {
    if(on_new_session_start (vptr, now) == NS_SESSION_FAILURE){
       NSDL1_SP(NULL, NULL, "Start new session failed from sync point");
     }
  }
  else if(sp_type == SP_TYPE_START_SYNCPOINT)
  {
    NSDL1_SP(NULL, NULL, "vptr->mcctptr = %p", ((VUser*)rcv_msg->vuser)->mcctptr);

    if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_SEPARATE_THREAD)
    {
      //If mcctptr not NULL, it means it is running in thread mode. hence need not to switch ctx just send msg to Vuser thread.
      send_msg_nvm_to_vutd(vptr, NS_API_SYNC_POINT_REP, 0);   
    }
    else if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT)
    {
      //If mcctptr NULL, it means it is running in NVM ctx. hence switching to Vuser ctx.
      switch_to_vuser_ctx(vptr, "sync point api switching to user context");
    }
    else if(vptr->sess_ptr->script_type == NS_SCRIPT_TYPE_JAVA)
    {
      NSDL1_SP(NULL, NULL, "Sending msg to njvm for TX");
      send_msg_to_njvm(vptr->mcctptr, NS_NJVM_API_SYNC_POINT_REP, 0);
    }
    else
    {
      NSTL1(NULL, NULL, "Error: Invalid case for SP_TYPE_START_SYNCPOINT");
    }
  }
  else if(sp_type == SP_TYPE_START_TRANSACTION)
  {
    NSDL1_SP(NULL, NULL, "vptr->mcctptr = %p", vptr->mcctptr);

    if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_SEPARATE_THREAD)
    {
      //If mcctptr not NULL, it means it is running in thread mode. hence need not to switch ctx just send msg to Vuser thread.
      send_msg_nvm_to_vutd(vptr, NS_API_START_TX_REP, tx_start_with_name(g_sync_point_table[rcv_msg->sync_point_id].sync_pt_name, vptr));   
    }
    else if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT)
    {
      //If mcctptr NULL, it means it is running in NVM ctx. hence switching to Vuser ctx.
      switch_to_vuser_ctx(vptr, "sync point transaction switching to user context");
    }
    else if(vptr->sess_ptr->script_type == NS_SCRIPT_TYPE_JAVA)
    {
      NSDL1_SP(NULL, NULL, "Sending msg to njvm for TX");
      send_msg_to_njvm(vptr->mcctptr, NS_NJVM_API_START_TRANSACTION_REP, 0);
    }
    else
    {
      NSTL1(NULL, NULL, "Error: Invalid case for SP_TYPE_START_TRANSACTION");
    }
  }
  else
    //TODO: Log event
    fprintf(stderr, "Invalid sync point %d got in message. It should not happen.\n", sp_type);
}

void sp_free_vusers(SP_user *SPU, int num_vusers_to_free)
{

  int ret_val;

  int sp_type = SPU->sp_type;
  int sp_id = SPU->sp_id;
  u_ns_ts_t now = get_ms_stamp();

  VUser *my_vuser_head =  SPU->vptr_head;

  NSDL1_SP(my_vuser_head, NULL, "Method Called, num_vusers_to_free=%d, sp_id = %d, sp_type = %d ",num_vusers_to_free, sp_id, sp_type);

  SPU->nvm_sync_point_users -= num_vusers_to_free;

  for(; ((my_vuser_head != NULL) && (num_vusers_to_free > 0)); my_vuser_head = my_vuser_head->sp_vuser_next)
  {
    NSDL1_SP(NULL, NULL, "my_vuser_head = %p, mcctptr = %p my_vuser_head->sp_vuser_next = %p\n", my_vuser_head, my_vuser_head->mcctptr, my_vuser_head->sp_vuser_next);
    num_vusers_to_free--;

    ret_val = sync_point_chk_for_ramp_down(my_vuser_head, now);
    if (ret_val == 1)
    {
      continue;
    }

    VUSER_SYNCPOINT_WAITING_TO_ACTIVE(my_vuser_head);

    if(sp_type == SP_TYPE_START_PAGE)
      nsi_web_url_int(my_vuser_head, now);
    else if(sp_type == SP_TYPE_START_SCRIPT)
    {
       if(on_new_session_start (my_vuser_head, now) == NS_SESSION_FAILURE){
          NSDL1_SP(NULL, NULL, "Start new session failed from sync point");
       }
    }
    else if(sp_type == SP_TYPE_START_SYNCPOINT)
    {
      NSDL1_SP(NULL, NULL, "my_vuser_head->mcctptr = %p", my_vuser_head->mcctptr);

      if(runprof_table_shr_mem[my_vuser_head->group_num].gset.script_mode == NS_SCRIPT_MODE_SEPARATE_THREAD)
      {
        //This is in thread mode
        send_msg_nvm_to_vutd(my_vuser_head, NS_API_SYNC_POINT_REP, 0);   
      }
      else if(runprof_table_shr_mem[my_vuser_head->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT)
      { 
        //This is in NVM ctx
        switch_to_vuser_ctx(my_vuser_head, "sync point api switching to user context");
      }
      else if(my_vuser_head->sess_ptr->script_type == NS_SCRIPT_TYPE_JAVA)
      {
        NSDL1_SP(NULL, NULL, "Sending msg to njvm for TX");
        send_msg_to_njvm(my_vuser_head->mcctptr, NS_NJVM_API_SYNC_POINT_REP, 0);
      }
      else
      {
        NSTL1(NULL, NULL, "Error: Invalid case for SP_TYPE_START_SYNCPOINT");
      }
    }
    else if(sp_type == SP_TYPE_START_TRANSACTION)
    {
      NSDL1_SP(NULL, NULL, "TX: my_vuser_head->mcctptr = %p", my_vuser_head->mcctptr);

      if(runprof_table_shr_mem[my_vuser_head->group_num].gset.script_mode == NS_SCRIPT_MODE_SEPARATE_THREAD)
      {
        send_msg_nvm_to_vutd(my_vuser_head, NS_API_START_TX_REP, tx_start_with_name(g_sync_point_table[sp_id].sync_pt_name, my_vuser_head));   
      }
      else if(runprof_table_shr_mem[my_vuser_head->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT)
      {
        //This is in NVM ctx
        switch_to_vuser_ctx(my_vuser_head, "sync point transaction switching to user context"); 
      }
      else if(my_vuser_head->sess_ptr->script_type == NS_SCRIPT_TYPE_JAVA)
      {   
        NSDL1_SP(NULL, NULL, "Sending msg to njvm for TX");
        send_msg_to_njvm(my_vuser_head->mcctptr, NS_NJVM_API_START_TRANSACTION_REP, tx_start_with_name(g_sync_point_table[sp_id].sync_pt_name, my_vuser_head));
      }
      else
      {
        NSTL1(NULL, NULL, "Error: Invalid case for SP_TYPE_START_TRANSACTION");
      }
    }
    else
      //TODO: Log event
      fprintf(stderr, "Invalid sync point %d got in message. It should not happen.\n", sp_type);

  }
  SPU->vptr_head = my_vuser_head;
}

void release_next_step(SP_user *SPU);

void sp_release_vusers_schedule(ClientData client_data, u_ns_ts_t now)
{
  SP_user *SPU = (SP_user *)client_data.p;
  // Get number of vusers to be free 
  int num_vusers_to_free = SPU->num_vusers_to_free; 
  NSDL4_SP(NULL, NULL, "Method Called, SPU->num_vusers_to_free=%d, SPU->extra_users_to_add=%d, SPU = %p",
           SPU->num_vusers_to_free, SPU->extra_users_to_add, SPU);
  NSTL4(NULL, NULL, "Method Called, SPU->num_vusers_to_free=%d, SPU->extra_users_to_add=%d, SPU = %p",
           SPU->num_vusers_to_free, SPU->extra_users_to_add, SPU);
  
  // Case: When number of users are greater than duration in which they have to released 
  if(SPU->extra_users_to_add != 0)
  {
      num_vusers_to_free = SPU->num_vusers_to_free + SLOT_OF_VUSERS_TO_FREE;
      SPU->extra_users_to_add -= SLOT_OF_VUSERS_TO_FREE;
  }

  // Free users here
  sp_free_vusers(SPU, num_vusers_to_free);

  // Take the last position of vuser in syncpoint list
  if(SPU->nvm_sync_point_users == 0)
  {
     NSDL1_SP(NULL, NULL, "All users removed from sync point");
     SPU->vptr_head = NULL;
     //Stop Timer as all users are released from syncpoint
     dis_timer_del(SPU->timer_ptr_sche_timeout);
     return;
  }
 
  int rel_step = SPU->next_step;
  SPU->step_quantity[rel_step] -= num_vusers_to_free;
  NSDL4_SP(NULL, NULL, "Vuser to be removed from Sync-Point - %d", SPU->step_quantity[rel_step]);
  if(SPU->step_quantity[rel_step] == 0)
  {
    NSDL1_SP(NULL, NULL, "All users removed from sync point current step, release next step");
    dis_timer_del(SPU->timer_ptr_sche_timeout);
    SPU->next_step++;
    release_next_step(SPU);
    return;
  }

  int extra_time = 0;
  NSDL4_SP(NULL, NULL, "mod_of_release = %d", SPU->mod_of_release);
  if(SPU->mod_of_release == 0)// when duration is greater than syncpoint users
  {
     if(SPU->duration_to_add > 0)
     {
        extra_time = 1; 
        SPU->duration_to_add--;
     }
  }
  else // when users are greater than duration
  {
     NSDL1_SP(NULL, NULL, "SPU->num_time_frame = %d",SPU->num_time_frame);
     if(SPU->num_time_frame > 0)
     {
       if(SPU->num_time_frame == 1) // this is the last frame add all users in this frame and release them
       {
         SPU->num_vusers_to_free = SPU->num_vusers_to_free + SPU->remaining_users;
       }
       SPU->num_time_frame--;
     }
  }
  SPU->timer_ptr_sche_timeout->actual_timeout =  (SPU->rate + extra_time) * 1000;
  NSDL4_SP(NULL, NULL, "nvm_sync_point_users = %d , num_vusers_to_free = %d , timeout = %d",
           SPU->nvm_sync_point_users, SPU->num_vusers_to_free, SPU->timer_ptr_sche_timeout->actual_timeout);
  NSTL4(NULL, NULL, "nvm_sync_point_users = %d , num_vusers_to_free = %d , timeout = %d",
           SPU->nvm_sync_point_users, SPU->num_vusers_to_free, SPU->timer_ptr_sche_timeout->actual_timeout);
} 

void process_sp_release_msg_frm_parent(SP_msg *rcv_msg)
{
  if (loader_opcode == CLIENT_LOADER) 
  {  //if generator then send msg to nvm and return
    return(send_msg(rcv_msg, 1, 0));
  }
  
  int sp_id = rcv_msg->sync_point_id;
  // Filling type of sync point
  sp_user_tbl[sp_id].sp_type = rcv_msg->sync_point_type;

  //Got release message from Parent. NVM need to release all vusers 
  //from their linked list either immediately/within duration/with rate

  NSDL1_SP(NULL, NULL, "Method Called, rcv_msg = %p", rcv_msg);
  NSDL1_SP(NULL, NULL, "sp_rel_step = %d, sp_id = %d, sp_type = %d, sp_user_tbl[sp_id].nvm_sync_point_users = %d", rcv_msg->sp_rel_step, sp_id, sp_user_tbl[sp_id].sp_type, sp_user_tbl[sp_id].nvm_sync_point_users);
  
  NSTL1(NULL, NULL, "Method Called, rcv_msg = %p", rcv_msg);
  NSTL1(NULL, NULL, "sp_rel_step = %d, sp_id = %d, sp_type = %d, sp_user_tbl[sp_id].nvm_sync_point_users = %d", rcv_msg->sp_rel_step, sp_id, sp_user_tbl[sp_id].sp_type, sp_user_tbl[sp_id].nvm_sync_point_users);
 
 
  // If this NVM doesn't have any user just ignore
  if(sp_user_tbl[sp_id].nvm_sync_point_users == 0)
     return;

  // Save Syncpoint ID
  sp_user_tbl[sp_id].sp_id = sp_id;

  if(rcv_msg->sp_rel_step)
  {   
    int i, size;
    size = sizeof(int) * rcv_msg->sp_rel_step;
    MY_REALLOC(sp_user_tbl[sp_id].step_quantity, size, "Step Duration Quantity", -1);
    MY_REALLOC(sp_user_tbl[sp_id].step_duration, size, "Step Duration Duration", -1);
    int qty_leftover = sp_user_tbl[sp_id].nvm_sync_point_users;
    int step_size = (sizeof(int) * rcv_msg->sp_rel_step);
   
    int *l_sp_rel_step_duration = (int *)((char *)rcv_msg + sizeof(SP_msg));
    int *l_sp_rel_step_quantity = (int *)((char *)rcv_msg + sizeof(SP_msg) + step_size);
    NSDL1_SP(NULL, NULL, "l_sp_rel_step_duration = %d, l_sp_rel_step_quantity = %d, rcv_msg->sp_rel_step = %d", *l_sp_rel_step_duration, *l_sp_rel_step_quantity, rcv_msg->sp_rel_step);
    NSTL1(NULL, NULL, "l_sp_rel_step_duration = %d, l_sp_rel_step_quantity = %d, rcv_msg->sp_rel_step = %d", *l_sp_rel_step_duration, *l_sp_rel_step_quantity, rcv_msg->sp_rel_step);
    for(i = 0; i < rcv_msg->sp_rel_step; i++)
    {  
       sp_user_tbl[sp_id].step_quantity[i] = (sp_user_tbl[sp_id].nvm_sync_point_users * l_sp_rel_step_quantity[i])/100;
       NSDL1_SP(NULL, NULL, "sp_user_tbl[sp_id].step_quantity[i] = %d", sp_user_tbl[sp_id].step_quantity[i]);
       NSTL1(NULL, NULL, "sp_user_tbl[sp_id].step_quantity[i] = %d", sp_user_tbl[sp_id].step_quantity[i]);
       sp_user_tbl[sp_id].step_duration[i] = l_sp_rel_step_duration[i];
       qty_leftover -= sp_user_tbl[sp_id].step_quantity[i];
    } 
    sp_user_tbl[sp_id].step_quantity[i-1] += qty_leftover; //Add all remaining quantity to last step
    sp_user_tbl[sp_id].next_step = 0;
    release_next_step(&sp_user_tbl[sp_id]);
  }
  else
  { 
    // Default case Release User - Immediate 
    sp_free_vusers(&sp_user_tbl[sp_id], sp_user_tbl[sp_id].nvm_sync_point_users);
    sp_user_tbl[sp_id].vptr_head = NULL;
  }
  return;
}

//Function for releasing multiple step
void release_next_step(SP_user *SPU)
{
  int rel_step = SPU->next_step;
  int extra_time = 0;
  int rel_duration = SPU->step_duration[rel_step];
  int nvm_sync_point_users = SPU->step_quantity[rel_step];

  NSDL1_SP(NULL, NULL, "rel_duration = %d, nvm_sync_point_users = %d",rel_duration, nvm_sync_point_users);
  NSTL1(NULL, NULL, "rel_duration = %d, nvm_sync_point_users = %d",rel_duration, nvm_sync_point_users);
  
  // compare whether seconds are greater or smaller than number of Vusers to release
  // This is the case when not more than 1 user can get free from syncpoint because duration is greater than num_vusers
  if(rel_duration >= nvm_sync_point_users)
  {
    // In case of multi step there can be the case when first step doesn't contain any Vuser's to be free 
    if(nvm_sync_point_users == 0)
    {
       SPU->rate = rel_duration;
       SPU->duration_to_add = 0;
       SPU->num_vusers_to_free = 0;
    }
    else
    {     
       SPU->rate = rel_duration/nvm_sync_point_users;
       SPU->duration_to_add = rel_duration % nvm_sync_point_users;
       SPU->num_vusers_to_free = 1;
    }
        
    SPU->remaining_users = 0;
    SPU->mod_of_release = 0;
    
    NSDL4_SP(NULL, NULL, "rate = %d, duration_to_add = %d, num_vusers_to_free = %d ", SPU->rate, SPU->duration_to_add, SPU->num_vusers_to_free);  
    NSTL4(NULL, NULL, "rate = %d, duration_to_add = %d, num_vusers_to_free = %d ", SPU->rate, SPU->duration_to_add, SPU->num_vusers_to_free);  
   
    if(SPU->duration_to_add > 0)
    {  
       extra_time = 1; 
       SPU->duration_to_add--;
    }
  }
  else // this is the case when number of Vusers are greater than timeout
  {
    // Here are we are taking slot of 1 sec to release vusers so first check how many secs are there
    // In this case we are considering a time frame of 1 sec to release as much user
    //   as we can, so getting number of frames

    SPU->num_time_frame = rel_duration/SLOT_OF_VUSERS_TO_FREE; //it will give number of times in which users will get free
    SPU->rate = SLOT_OF_VUSERS_TO_FREE;
    SPU->duration_to_add = rel_duration % SLOT_OF_VUSERS_TO_FREE;
    // vusers_to_release provides in 1 sec how many vusers get releases in 1 sec and
    int vusers_to_release = nvm_sync_point_users/rel_duration;
    SPU->num_vusers_to_free = vusers_to_release * SLOT_OF_VUSERS_TO_FREE;
    SPU->remaining_users = nvm_sync_point_users % rel_duration;
    SPU->mod_of_release = 1;
    SPU->extra_users_to_add = 0;
    if((SPU->remaining_users / SLOT_OF_VUSERS_TO_FREE) != 0)
    {
      SPU->extra_users_to_add = SPU->remaining_users / SLOT_OF_VUSERS_TO_FREE;
      SPU->remaining_users = SPU->remaining_users % SLOT_OF_VUSERS_TO_FREE;
        
    }
    /*if(SPU->extra_users_to_add != 0)
    {
       SPU->num_vusers_to_free = SPU->num_vusers_to_free + SLOT_OF_VUSERS_TO_FREE;
       SPU->extra_users_to_add -= SLOT_OF_VUSERS_TO_FREE;
    } */
    SPU->num_time_frame--;

    NSDL4_SP(NULL, NULL, "rate = %d, duration_to_add = %d, vusers_to_release = %d, num_vusers_to_free = %d extra_users_to_add = %d remaining_users = %d", SPU->rate, SPU->duration_to_add, vusers_to_release, SPU->num_vusers_to_free, SPU->extra_users_to_add, SPU->remaining_users);

    NSTL4(NULL, NULL, "rate = %d, duration_to_add = %d, vusers_to_release = %d, num_vusers_to_free = %d extra_users_to_add = %d remaining_users = %d", SPU->rate, SPU->duration_to_add, vusers_to_release, SPU->num_vusers_to_free, SPU->extra_users_to_add, SPU->remaining_users);

    NSDL4_SP(NULL, NULL, " num_time_frame = %d, mod_of_release = %d", SPU->num_time_frame, SPU->mod_of_release);
    NSTL4(NULL, NULL, " num_time_frame = %d, mod_of_release = %d", SPU->num_time_frame, SPU->mod_of_release);
  }
  
  ClientData client_data;
  client_data.p = SPU;
  if(!SPU->timer_ptr_sche_timeout)
  {
    MY_MALLOC(SPU->timer_ptr_sche_timeout, sizeof(timer_type), "timer_ptr_schetimeout", -1);
    SPU->timer_ptr_sche_timeout->timer_type = -1;
  }
  
  SPU->timer_ptr_sche_timeout->actual_timeout = (SPU->rate + extra_time) * 1000;
  dis_timer_add_ex(AB_TIMEOUT_RAMP, SPU->timer_ptr_sche_timeout,
            get_ms_stamp(), sp_release_vusers_schedule, client_data, 1, 0);
}

/* Purpose: This function will make request and send to particular NVM
 * Return : 0 - on success
 *          1 - on failure
 */
int make_msg_and_send_to_nvm(SP_msg *rcv_msg, int sp_tbl_id, int opcode, int fd, int send_to_all)
{
  NSDL1_SP(NULL, NULL, "Method Called, rcv_msg = %p, opcode = %d, loader_opcode = %d", rcv_msg, opcode, loader_opcode);
  //NSTL1(NULL, NULL, "Method Called, rcv_msg = %p, opcode = %d, loader_opcode = %d, sp_tbl_id= %d", rcv_msg, opcode, loader_opcode, sp_tbl_id);
  rcv_msg->opcode = opcode;
  rcv_msg->sync_point_id = sp_tbl_id;
  //send msg to nvm from parent
  send_msg(rcv_msg, send_to_all, fd);
  return 0;
}

//Following function will return sync point name on giving sync point type.
//
char *ret_sp_name(int sp_type)
{
  NSDL2_SP(NULL, NULL, "Method Called, tye = %d", sp_type);
  if(sp_type == SP_TYPE_START_TRANSACTION)
    return "Start Transaction";
  else if(sp_type == SP_TYPE_START_PAGE)
    return "Start Page";
  else if(sp_type == SP_TYPE_START_SCRIPT)
    return "Start Script";
  else if(sp_type == SP_TYPE_START_SYNCPOINT)
    return "Start SyncPoint";
  else
  {
    fprintf(stderr,"Given sync point type %d is not correct\n", sp_type);
    return "Invalid sync point type";
  }
}

void release_sync_point(int fd, int sp_tbl_id, SP_msg *rcv_msg, char *syn_name, char *grp_name, char *reason_for_release ,int timeout_type) {

  int sp_grp_tbl_id = rcv_msg->sync_point_id;
  int grp_idx = rcv_msg->grp_idx;
  char release_count[256];
  char msg_buffer[256];
  int release_target_user;
  char cur_time[100];

  g_sync_point_table[sp_tbl_id].next_release = 0;

  NSDL1_SP(NULL, NULL, "Method called, Releasing SynPoint at index %d of policy %d", sp_tbl_id, 
     g_sync_point_table[sp_tbl_id].release_target_usr_policy[g_sync_point_table[sp_tbl_id].cur_policy_idx]);

  NSTL1(NULL, NULL, "Method called, Releasing SynPoint at index %d of policy %d", sp_tbl_id,
     g_sync_point_table[sp_tbl_id].release_target_usr_policy[g_sync_point_table[sp_tbl_id].cur_policy_idx]);
 
  //Delete old timer and Add new one
  if (g_sync_point_table[sp_tbl_id].timer_ptr_iato->timer_type >= 0) 
  {
    NSDL2_SP(NULL, NULL, "Deleting user arrival timer for sync point %d, timer_ptr = %p", 
                          sp_tbl_id, g_sync_point_table[sp_tbl_id].timer_ptr_iato);
    NSTL2(NULL, NULL, "Deleting user arrival timer for sync point %d, timer_ptr = %p",
                          sp_tbl_id, g_sync_point_table[sp_tbl_id].timer_ptr_iato);
    dis_timer_del(g_sync_point_table[sp_tbl_id].timer_ptr_iato);
  }

  // Add Over all timer
  if (g_sync_point_table[sp_tbl_id].timer_ptr_oato->timer_type >= 0) 
  {
    NSDL2_SP(NULL, NULL, "Deleting overall timer for sync point %d, timer_ptr = %p", 
                          sp_tbl_id, g_sync_point_table[sp_tbl_id].timer_ptr_oato);
    NSTL2(NULL, NULL, "Deleting overall timer for sync point %d, timer_ptr = %p",
                          sp_tbl_id, g_sync_point_table[sp_tbl_id].timer_ptr_oato);
    dis_timer_del(g_sync_point_table[sp_tbl_id].timer_ptr_oato);
  }
  
  // Remove Release Type Time timer
  if (g_sync_point_table[sp_tbl_id].timer_ptr_reltype->timer_type >= 0)
  {
    NSDL2_SP(NULL, NULL, "Deleting Release Type timer for sync point %d, timer_ptr = %p",
                          sp_tbl_id, g_sync_point_table[sp_tbl_id].timer_ptr_reltype);
    NSTL2(NULL, NULL, "Deleting Release Type timer for sync point %d, timer_ptr = %p",
                          sp_tbl_id, g_sync_point_table[sp_tbl_id].timer_ptr_reltype);
    dis_timer_del(g_sync_point_table[sp_tbl_id].timer_ptr_reltype);
  }

  //Send msg to NVM to release SynPoint
  make_msg_and_send_to_nvm(rcv_msg, sp_tbl_id, SP_RELEASE, fd, 1);

  release_target_user = g_sync_point_table[sp_tbl_id].release_target_usr_policy[g_sync_point_table[sp_tbl_id].cur_policy_idx];

  //Update policy idnex and also make users to zero
  g_sync_point_table[sp_tbl_id].cur_policy_idx++;
  g_sync_point_table[sp_tbl_id].total_accu_usrs = 0;
  g_sync_point_table[sp_tbl_id].release_count++; // how much time release has been done 

  sprintf(release_count, "%d", g_sync_point_table[sp_tbl_id].release_count);

  //Update last release time and reason
  strcpy(g_sync_point_table[sp_tbl_id].last_release_reason, reason_for_release);
  strcpy(g_sync_point_table[sp_tbl_id].last_release_time, get_relative_time());

  if (loader_opcode != CLIENT_LOADER)
  {
     if(timeout_type == REL_IATO)
     {
        sprintf(cur_time, "%02d:%02d:%02d", (g_sync_point_table[sp_tbl_id].inter_arrival_timeout/1000) / 3600, ((g_sync_point_table[sp_tbl_id].inter_arrival_timeout/1000) % 3600) / 60, ((g_sync_point_table[sp_tbl_id].inter_arrival_timeout/1000) % 3600) % 60);
        sprintf(msg_buffer, "Releasing SyncPoint due to inter Vuser arrival timeout of %s (HH:MM:SS). Scenario Group = %s, Participating Vusers = %.0f%%, Release Target Vusers = %d.", cur_time, grp_name, g_sync_point_table[sp_tbl_id].sync_pt_usr_pct,release_target_user);
     }
     else if(timeout_type == REL_OATO)
     {
        sprintf(cur_time, "%02d:%02d:%02d", (g_sync_point_table[sp_tbl_id].overall_timeout/1000) / 3600, ((g_sync_point_table[sp_tbl_id].overall_timeout/1000) % 3600) / 60, ((g_sync_point_table[sp_tbl_id].overall_timeout/1000) % 3600) % 60);
       sprintf(msg_buffer, "Releasing SyncPoint due to overall timeout of %s (HH:MM:SS). Scenario Group = %s, Participating Vusers = %.0f%%, Release Target Vusers = %d.", cur_time, grp_name, g_sync_point_table[sp_tbl_id].sync_pt_usr_pct,release_target_user );
     }
     else if(timeout_type == REL_TARGET_REACHED)
        sprintf(msg_buffer, "Releasing SyncPoint due to release target reached. Scenario Group = %s, Participating Vusers = %.0f%% , Release Target Vusers = %d", grp_name, g_sync_point_table[sp_tbl_id].sync_pt_usr_pct, release_target_user);
     else if(timeout_type == REL_MANUAL)
        sprintf(msg_buffer, "Releasing SyncPoint manually on request from user '%s'. Scenario Group = %s, Participating Vusers = %.2f%%, Release Target Vusers = %d", g_rtc_owner, grp_name, g_sync_point_table[sp_tbl_id].sync_pt_usr_pct, release_target_user);
     else if((timeout_type == REL_RTTO) && (g_sync_point_table[sp_tbl_id].release_type == SP_RELEASE_TYPE_PERIOD)) 
        sprintf(msg_buffer, "Releasing SyncPoint due to release period(%s) completed. Scenario Group = %s, Participating Vusers = %.2f%%, Release Target Vusers = %d", g_sync_point_table[sp_tbl_id].s_release_tval, grp_name, g_sync_point_table[sp_tbl_id].sync_pt_usr_pct, release_target_user);
    else if((timeout_type == REL_RTTO) && (g_sync_point_table[sp_tbl_id].release_type == SP_RELEASE_TYPE_TIME))
        sprintf(msg_buffer, "Releasing SyncPoint due to absolute time (%s) completed. Scenario Group = %s, Participating Vusers = %.2f%%, Release Target Vusers = %d", g_sync_point_table[sp_tbl_id].s_release_tval, grp_name, g_sync_point_table[sp_tbl_id].sync_pt_usr_pct, release_target_user);
     
    NS_EL_4_ATTR(EID_SP_RELEASE, -1, -1, EVENT_SYNC_POINT, EVENT_INFORMATION,
       grp_name, ret_sp_name(rcv_msg->sync_point_type), syn_name, 
       release_count, msg_buffer);
 }
   NSDL2_SP(NULL, NULL, "g_sync_point_table[sp_tbl_id].total_release_policies = %d, (g_sync_point_table[sp_tbl_id].cur_policy_idx + 1) = %d", g_sync_point_table[sp_tbl_id].total_release_policies, (g_sync_point_table[sp_tbl_id].cur_policy_idx + 1));

   NSTL2(NULL, NULL, "g_sync_point_table[sp_tbl_id].total_release_policies = %d, (g_sync_point_table[sp_tbl_id].cur_policy_idx + 1) = %d", g_sync_point_table[sp_tbl_id].total_release_policies, (g_sync_point_table[sp_tbl_id].cur_policy_idx + 1));

  //This case will handle following cases:
  // 10,15,*
  // 10,*
  // 10,15,20 - in case of 20 "IF" will get fail and elseif will be executed 

  g_sync_point_table[sp_tbl_id].next_release =  0;
  if(g_sync_point_table[sp_tbl_id].total_release_policies >= (g_sync_point_table[sp_tbl_id].cur_policy_idx + 1))
  {
    NSDL2_SP(NULL, NULL, "cur_policy_idx = %d", g_sync_point_table[sp_tbl_id].release_target_usr_policy[g_sync_point_table[sp_tbl_id].cur_policy_idx]);
   NSTL2(NULL, NULL, "cur_policy_idx = %d", g_sync_point_table[sp_tbl_id].release_target_usr_policy[g_sync_point_table[sp_tbl_id].cur_policy_idx]);
    
     g_sync_point_table[sp_tbl_id].next_release =  1;
  
    //If last sync policy is * then make curr_policy_idx to previous policy
    if(g_sync_point_table[sp_tbl_id].release_target_usr_policy[g_sync_point_table[sp_tbl_id].cur_policy_idx] == SP_RELEASE_PREV_POLICY)
    {
      g_sync_point_table[sp_tbl_id].cur_policy_idx = g_sync_point_table[sp_tbl_id].cur_policy_idx - 1;
    }    

  }
  else if(g_sync_point_table[sp_tbl_id].total_release_policies < (g_sync_point_table[sp_tbl_id].cur_policy_idx + 1))
  {
     // Clear syncpoint FD
     MY_CLR_FD_SHR(grp_idx, sp_grp_tbl_id);
  }

  g_sync_point_table[sp_tbl_id].sp_actv_inactv = SP_RELEASING;
  // SP_RELEASING status has to be shown on UI so need to write here in sync point summary file
  //create_sync_point_summary_file();
  // Previously we were making Syncpoint Inactive immediately on UI as Vusers were releasing immediately,
  // but now users has to be released with In duration/Rate so we have to wait till the time all 
  // users are released and then syncpoint status has to be updated on UI. 
  // To implement this, parent has to now wait for the same timeperiod for which child is releasing Vusers
  // from sync point.

  if(rcv_msg->sp_rel_step)
  {
     ClientData client_data;
     g_sync_point_table[sp_tbl_id].scen_grp_idx = grp_idx;
     client_data.p = &g_sync_point_table[sp_tbl_id];
     // Converting time into milliseconds to add into timer
     int timeout = 0;
     for(int i = 0; i < rcv_msg->sp_rel_step; i++)
        timeout += rcv_msg->sp_rel_step_duration[i];
     
     g_sync_point_table[sp_tbl_id].timer_ptr_reltype->actual_timeout =  timeout * 1000;
     if(g_sync_point_table[sp_tbl_id].next_release)
        NSDL2_SP(NULL, NULL, "Going to release syncpoint with multiple release policy after timeout of %d sec", timeout);
     else
        NSDL2_SP(NULL, NULL, "ALL policies over, going to make syncpoint inactive after timeout of %d sec", timeout);

     dis_timer_add_ex(AB_PARENT_SP_RELEASE_TYPE_TIMEOUT, g_sync_point_table[sp_tbl_id].timer_ptr_reltype,
       get_ms_stamp(), sync_point_release_release_done_cb, client_data, 0, 0);
  }
  else
  {
     NSDL2_SP(NULL, NULL, "g_sync_point_table[sp_tbl_id].sp_actv_inactv = %d", g_sync_point_table[sp_tbl_id].sp_actv_inactv);
     if (loader_opcode != CLIENT_LOADER) // donot log in case of generator
     {
        sprintf(msg_buffer, "Syncpoint Released Successfully");
        NS_EL_4_ATTR(EID_SP_RELEASE, -1, -1, EVENT_SYNC_POINT, EVENT_INFORMATION,
           grp_name, ret_sp_name(rcv_msg->sync_point_type), syn_name,
           release_count, msg_buffer);
     }
          
     g_sync_point_table[sp_tbl_id].sp_actv_inactv = g_sync_point_table[sp_tbl_id].next_release;
  }
  create_sync_point_summary_file();
}

/*Parent code.*/
void process_test_msg_from_nvm (int fd, SP_msg *my_rcv_msg)
{
  char syn_name[SP_MAX_DATA_LINE_LENGTH];
  char grp_name[SP_MAX_DATA_LINE_LENGTH];  //for event logging
  int sp_grp_tbl_id;
  int sp_tbl_id;
  //VUser *my_vuser = NULL;
  int grp_idx;
  char release_count[256];
  SP_msg rcv_msg;
  int user_index;
  ClientData client_data;

  memcpy(&rcv_msg, my_rcv_msg, sizeof(SP_msg));

  NSDL1_SP(NULL, NULL, "loader_opcode = [%d] ", loader_opcode);
  if (loader_opcode == CLIENT_LOADER) //generator
  {
    rcv_msg.nvm_fd = fd;
    rcv_msg.msg_len = sizeof(SP_msg) - sizeof(int);
    write_msg(g_master_msg_com_con, (char *)(&rcv_msg), sizeof(SP_msg), 0, CONTROL_MODE);
    return;
  }

  sp_grp_tbl_id = rcv_msg.sync_point_id;
  //my_vuser = (VUser*)rcv_msg.vuser;
  grp_idx = rcv_msg.grp_idx;
  user_index = rcv_msg.vuser_id;

  if(spGroupTable_shr[sp_grp_tbl_id].sync_group_name_as_int == -1)
    sp_tbl_id = all_grp_entries_shr[(sp_grp_tbl_id * (total_runprof_entries + 1)) + (spGroupTable_shr[sp_grp_tbl_id].sync_group_name_as_int + 1)];
  else
    sp_tbl_id = all_grp_entries_shr[(sp_grp_tbl_id * (total_runprof_entries + 1)) + (grp_idx + 1)];

  NSDL1_SP(NULL, NULL, "Method called, sp_grp_tbl_id = %d, total_runprof_entries = %d, grp_idx = %d, sp_tbl_id = %d\n", sp_grp_tbl_id, total_runprof_entries, grp_idx, sp_tbl_id);

  NSTL2(NULL, NULL, "Method called, sp_grp_tbl_id = %d, total_runprof_entries = %d, grp_idx = %d, sp_tbl_id = %d\n", sp_grp_tbl_id, total_runprof_entries, grp_idx, sp_tbl_id);

  strcpy(syn_name, g_sync_point_table[sp_tbl_id].sync_pt_name);     // sync point name 
  if(g_sync_point_table[sp_tbl_id].sync_group_name_as_int == -1)    // group name
    strcpy(grp_name, "ALL");
  else
    strcpy(grp_name, runprof_table_shr_mem[grp_idx].scen_group_name);
  
  if(g_sync_point_table[sp_tbl_id].total_accu_usrs == g_sync_point_table[sp_tbl_id].release_target_usr_policy[g_sync_point_table[sp_tbl_id].cur_policy_idx])
  {
     // send NVM message to CONTINUE with this vuser as we have already reached to the target
     // and we are waiting for Release type timeout/manual release
     NSDL2_SP(NULL, NULL, "Continue as sync point target reached");
     NSTL2(NULL, NULL, "Continue as sync point target reached");
     make_msg_and_send_to_nvm(&rcv_msg, sp_tbl_id, SP_VUSER_CONTINUE, fd, 0);
     return;
  }

 /* To avoid total_accu_usrs > release_target_usr_policy,
    as now we have to release users based on TIME, PERIOD, TARGET */
  if((g_sync_point_table[sp_tbl_id].total_accu_usrs + 1) == g_sync_point_table[sp_tbl_id].release_target_usr_policy[g_sync_point_table[sp_tbl_id].cur_policy_idx])
  {
     //Delete old timer
     if (g_sync_point_table[sp_tbl_id].timer_ptr_iato->timer_type >= 0)
     {
        NSDL2_SP(NULL, NULL, "Deleting user arrival timer for sync point %d, timer_ptr = %p",
                          sp_tbl_id, g_sync_point_table[sp_tbl_id].timer_ptr_iato);
        NSTL2(NULL, NULL, "Deleting user arrival timer for sync point %d, timer_ptr = %p",
                          sp_tbl_id, g_sync_point_table[sp_tbl_id].timer_ptr_iato);
        dis_timer_del(g_sync_point_table[sp_tbl_id].timer_ptr_iato);
     }

     //Delete old timer
     if (g_sync_point_table[sp_tbl_id].timer_ptr_oato->timer_type >= 0)
     {
        NSDL2_SP(NULL, NULL, "Deleting overall arrival timer for sync point %d, timer_ptr = %p",
                          sp_tbl_id, g_sync_point_table[sp_tbl_id].timer_ptr_oato);
        NSTL2(NULL, NULL, "Deleting overall arrival timer for sync point %d, timer_ptr = %p",
                          sp_tbl_id, g_sync_point_table[sp_tbl_id].timer_ptr_oato);
        dis_timer_del(g_sync_point_table[sp_tbl_id].timer_ptr_oato);
     }
  }

  //Now we got msg from NVM to test if this vuser will go in SyncPoint or not.
  //Parent will do following:
  //Get the sync_point_id, go into Sync Point table and check sync_point policy at sync_point_id
  //Case1: Total users are not reached at sync point
  //Send WAIT message to the NVM
  //Case2: Total users reached at sync point
  //Send RELEASE message to the NVM

  sprintf(release_count, "%d", g_sync_point_table[sp_tbl_id].release_count);

  /* This is case when all policies are over and parent got VUSER_TEST mesage from NVM.
   * In this case parent is already done with making syncpoint inactive but this messages was sent 
   * before parent made the syncpoint inactive.So, if this condition hits then it means 
   * this user needs to contiue*/
  if(g_sync_point_table[sp_tbl_id].total_release_policies < (g_sync_point_table[sp_tbl_id].cur_policy_idx + 1)){
    NSDL2_SP(NULL, NULL, "Continue as sync point policy over");
    NSTL2(NULL, NULL, "Continue as sync point policy over");
    make_msg_and_send_to_nvm(&rcv_msg, sp_tbl_id, SP_VUSER_CONTINUE, fd, 0);
    return;
  }

  NSDL2_SP(NULL, NULL, "g_sync_point_table[%d].total_accu_usrs = %d,release_target_usr_policy[%d] = %d", 
       sp_tbl_id, g_sync_point_table[sp_tbl_id].total_accu_usrs, 
       g_sync_point_table[sp_tbl_id].cur_policy_idx,
       g_sync_point_table[sp_tbl_id].release_target_usr_policy[g_sync_point_table[sp_tbl_id].cur_policy_idx]);
  NSTL2(NULL, NULL, "g_sync_point_table[%d].total_accu_usrs = %d,release_target_usr_policy[%d] = %d",
       sp_tbl_id, g_sync_point_table[sp_tbl_id].total_accu_usrs,
       g_sync_point_table[sp_tbl_id].cur_policy_idx,
       g_sync_point_table[sp_tbl_id].release_target_usr_policy[g_sync_point_table[sp_tbl_id].cur_policy_idx]);

  NSDL2_SP(NULL, NULL, "state of syncpoint is %d",g_sync_point_table[sp_tbl_id].sp_actv_inactv);
 
  // Case when one syncpoint release is in progress, till that time no other can come in syncpoint
  if(g_sync_point_table[sp_tbl_id].sp_actv_inactv != SP_ACTIVE)
  {
     //Send msg to NVM to wait for to release synpoint
     NSDL2_SP(NULL, NULL, "Continue as sync point is progress");
     NSTL2(NULL, NULL, "Continue as sync point is in progress");
     make_msg_and_send_to_nvm(&rcv_msg, sp_tbl_id, SP_VUSER_CONTINUE, fd, 0);
     return;
 }

  make_msg_and_send_to_nvm(&rcv_msg, sp_tbl_id, SP_VUSER_WAIT, fd, 0);

  //List is empty, it means first time its coming on this ysncpoint
  //start Overa ll time out and Inter arrival time out
  //
  if(g_sync_point_table[sp_tbl_id].total_accu_usrs == 0)
  {
    //Filling Client data, client data will contain particular sync point table
    client_data.p = &g_sync_point_table[sp_tbl_id];

    //Adding inetr arrival time out
    g_sync_point_table[sp_tbl_id].timer_ptr_iato->actual_timeout = g_sync_point_table[sp_tbl_id].inter_arrival_timeout;
    NSDL2_SP(NULL, NULL, "Adding Inter Arrival Timeout, sp_tbl_id = %d, actual_timeout = %d", 
                           sp_tbl_id, g_sync_point_table[sp_tbl_id].timer_ptr_iato->actual_timeout);  

    dis_timer_add_ex(AB_PARENT_SP_VUSER_ARRIVAL_TIMEOUT, g_sync_point_table[sp_tbl_id].timer_ptr_iato, 
                  get_ms_stamp(), sync_point_release_iato_cb, client_data, 0, 0);

    //Adding over all time out
    g_sync_point_table[sp_tbl_id].timer_ptr_oato->actual_timeout = g_sync_point_table[sp_tbl_id].overall_timeout;
    NSDL2_SP(NULL, NULL, "Adding Overall Timeout, sp_tbl_id = %d, actual_timeout = %d", 
                           sp_tbl_id, g_sync_point_table[sp_tbl_id].timer_ptr_oato->actual_timeout);  

    dis_timer_add_ex(AB_PARENT_SP_OVERALL_TIMEOUT, g_sync_point_table[sp_tbl_id].timer_ptr_oato, 
              get_ms_stamp(), sync_point_release_oato_cb, client_data, 0, 0);
 
    /* Add this timer only when Release Mode is Auto Release Type is  TIME/PERIOD/PERIODIC */
    if(g_sync_point_table[sp_tbl_id].release_mode == SP_RELEASE_AUTO) 
    {
       if ((g_sync_point_table[sp_tbl_id].release_type == SP_RELEASE_TYPE_TIME) || (g_sync_point_table[sp_tbl_id].release_type == SP_RELEASE_TYPE_PERIOD))    
       {
         int cur_policy_idx = g_sync_point_table[sp_tbl_id].cur_policy_idx;
         NSDL2_SP(NULL, NULL, "cur_policy_idx = %d, g_sync_point_table[sp_tbl_id].release_type = %d, release_type_timeout = %d, release_type_frequency = %d", cur_policy_idx, g_sync_point_table[sp_tbl_id].release_type, g_sync_point_table[sp_tbl_id].release_type_timeout, g_sync_point_table[sp_tbl_id].release_type_frequency);

         if((g_sync_point_table[sp_tbl_id].release_type == SP_RELEASE_TYPE_PERIOD) || (cur_policy_idx == 0))
           g_sync_point_table[sp_tbl_id].timer_ptr_reltype->actual_timeout = g_sync_point_table[sp_tbl_id].release_type_timeout;
         else
           g_sync_point_table[sp_tbl_id].timer_ptr_reltype->actual_timeout = g_sync_point_table[sp_tbl_id].release_type_frequency;

         NSDL2_SP(NULL, NULL, "Adding Release Type(%d) Timeout, sp_tbl_id = %d, actual_timeout = %d",
                           g_sync_point_table[sp_tbl_id].release_type, sp_tbl_id, g_sync_point_table[sp_tbl_id].timer_ptr_reltype->actual_timeout);

         dis_timer_add_ex(AB_PARENT_SP_RELEASE_TYPE_TIMEOUT, g_sync_point_table[sp_tbl_id].timer_ptr_reltype,
              get_ms_stamp(), sync_point_release_release_type_cb, client_data, 0, 0);
       }
    }   
  }
  else
  {
    //Not coming first time on this sync point
    //so just reset the inter arrival timeout
    if((g_sync_point_table[sp_tbl_id].total_accu_usrs + 1) < g_sync_point_table[sp_tbl_id].release_target_usr_policy[g_sync_point_table[sp_tbl_id].cur_policy_idx])
    {
        // RTC: When configured amount of Vusers reached to the syncpoint in that case we delete inter-arrival and 
        // overall timeout. Now if an RTC to increase Vuser to be participated in syncpoint is applied we get a core during
        // resetting inter-arrival timeout as we already deleted it earlier. Therefore we have to check if timer has been
        // deleted and Vusers are still coming in syncpoint, we need to add timers again.

        // TODO:: As oato reflects the overall timeout allocated to Vusers to reach syncpoint. But if above
        // case is encountered where Vusers are increased before releasing them, then below we are starting overall timeout again
        // which is not correct as per its feature.

        // NEED TO DISCUSS:: HOW TO GET CURRENT OVERALL TIMEOUT EXPIRED ??????
        if (syncPntTable_shr[sp_tbl_id].timer_ptr_iato->timer_type < 0)
        {
           //Filling Client data, client data will contain particular sync point table
           client_data.p = &syncPntTable_shr[sp_tbl_id];

           //Adding inter arrival time out
           syncPntTable_shr[sp_tbl_id].timer_ptr_iato->actual_timeout = syncPntTable_shr[sp_tbl_id].inter_arrival_timeout;
           NSDL2_SP(NULL, NULL, "Adding Inter Arrival Timeout after Vuser has increased, sp_tbl_id = %d, actual_timeout = %d",
                           sp_tbl_id, syncPntTable_shr[sp_tbl_id].timer_ptr_iato->actual_timeout);
           NSTL2(NULL, NULL, "Adding Inter Arrival Timeout after Vuser has increased, sp_tbl_id = %d, actual_timeout = %d",
                           sp_tbl_id, syncPntTable_shr[sp_tbl_id].timer_ptr_iato->actual_timeout);
           
           dis_timer_add_ex(AB_PARENT_SP_VUSER_ARRIVAL_TIMEOUT, syncPntTable_shr[sp_tbl_id].timer_ptr_iato,
                  get_ms_stamp(), sync_point_release_iato_cb, client_data, 0, 0);
        }
        

        if (syncPntTable_shr[sp_tbl_id].timer_ptr_oato->timer_type < 0)
        {

           //Filling Client data, client data will contain particular sync point table
           client_data.p = &syncPntTable_shr[sp_tbl_id];
           //Adding over all time out
           syncPntTable_shr[sp_tbl_id].timer_ptr_oato->actual_timeout = syncPntTable_shr[sp_tbl_id].overall_timeout;
           NSDL2_SP(NULL, NULL, "Adding Overall Timeout after Vuser has increased, sp_tbl_id = %d, actual_timeout = %d",
                           sp_tbl_id, syncPntTable_shr[sp_tbl_id].timer_ptr_oato->actual_timeout);
           NSTL2(NULL, NULL, "Adding Overall Timeout after Vuser has increased, sp_tbl_id = %d, actual_timeout = %d",
                           sp_tbl_id, syncPntTable_shr[sp_tbl_id].timer_ptr_oato->actual_timeout);

           dis_timer_add_ex(AB_PARENT_SP_OVERALL_TIMEOUT, syncPntTable_shr[sp_tbl_id].timer_ptr_oato,
              get_ms_stamp(), sync_point_release_oato_cb, client_data, 0, 0);
        }

       NSDL4_SP(NULL, NULL, "Resetting inter arrival Timeout, sp_tbl_id = %d, actual_timeout = %d", 
                           sp_tbl_id, g_sync_point_table[sp_tbl_id].timer_ptr_iato->actual_timeout);  
       NSTL4(NULL, NULL, "Resetting inter arrival Timeout, sp_tbl_id = %d, actual_timeout = %d", 
                           sp_tbl_id, g_sync_point_table[sp_tbl_id].timer_ptr_iato->actual_timeout);  
       dis_timer_reset(get_ms_stamp(), g_sync_point_table[sp_tbl_id].timer_ptr_iato);
    }
  }
 
   // Incrementing total number of Vusers reached syncpoint till now
   g_sync_point_table[sp_tbl_id].total_accu_usrs++;
   NSDL4_SP(NULL, NULL, "Total user are now  %d",g_sync_point_table[sp_tbl_id].total_accu_usrs);
   NSTL4(NULL, NULL, "Total user are now  %d",g_sync_point_table[sp_tbl_id].total_accu_usrs);
   
  //Adding log for last user in event log
  if((g_sync_point_table[sp_tbl_id].total_accu_usrs == 1) || (g_sync_point_table[sp_tbl_id].total_accu_usrs == g_sync_point_table[sp_tbl_id].release_target_usr_policy[g_sync_point_table[sp_tbl_id].cur_policy_idx]))
  {
    NS_EL_4_ATTR(EID_SP_ADD_FIRST_OR_LAST_USER, user_index,
        -1, EVENT_SYNC_POINT, EVENT_INFORMATION, grp_name,
        ret_sp_name(rcv_msg.sync_point_type), syn_name,
        release_count, "Adding Vuser %d to SyncPoint. Scenario group = %s, Participating Vuser = %.0f%%, Release target Vusers = %d", g_sync_point_table[sp_tbl_id].total_accu_usrs, grp_name, g_sync_point_table[sp_tbl_id].sync_pt_usr_pct, g_sync_point_table[sp_tbl_id].release_target_usr_policy[g_sync_point_table[sp_tbl_id].cur_policy_idx]);
  }
  else
  {
    //Adding user No. of scenario group .Static1KGrp. to Sync Point .Statik1KWithTx. of type .Start Script. 
    NS_EL_4_ATTR(EID_SP_ADD_USER, user_index,
        -1, EVENT_SYNC_POINT, EVENT_INFORMATION, grp_name,
        ret_sp_name(rcv_msg.sync_point_type), syn_name,
        release_count, "Adding Vuser %d to SyncPoint. Scenario group = %s, Participating Vuser = %.0f%%, Release target Vusers = %d", g_sync_point_table[sp_tbl_id].total_accu_usrs, grp_name, g_sync_point_table[sp_tbl_id].sync_pt_usr_pct, g_sync_point_table[sp_tbl_id].release_target_usr_policy[g_sync_point_table[sp_tbl_id].cur_policy_idx]);
  }

  /* In case of Release Type Target only we have to release user from here */
  if(g_sync_point_table[sp_tbl_id].total_accu_usrs == g_sync_point_table[sp_tbl_id].release_target_usr_policy[g_sync_point_table[sp_tbl_id].cur_policy_idx])
  {
     //Check for total users. If target reached then release users
     if((g_sync_point_table[sp_tbl_id].release_mode == SP_RELEASE_AUTO) && 
       ((g_sync_point_table[sp_tbl_id].release_type == SP_RELEASE_TYPE_TARGET) || (g_sync_point_table[sp_tbl_id].release_forcefully)))
     {
        SP_msg *my_msg = create_sync_point_release_msg(&g_sync_point_table[sp_tbl_id]); 
        release_sync_point(fd, sp_tbl_id, my_msg, syn_name, grp_name, "Target Reached", REL_TARGET_REACHED);
     }
  }
}

inline void nsi_send_sync_point_msg(VUser *vptr)
{
  SP_msg test_SP_msg;

  NSDL2_SP(NULL, NULL, "Method called. vptr = %p, user_index = %d. sync_point_id = %d, sp_type = %d, group_num = %d", vptr, vptr->user_index, vptr->sync_point_id, spGroupTable_shr[vptr->sync_point_id].sp_type, vptr->group_num);

  //This user is going to take part in syncpoint
  //Send msg to parent to check 
  
  //Makeing message to test whether Sync Point will has to release or not 
  //and send this message to Parent 
  test_SP_msg.opcode = SP_VUSER_TEST;
  test_SP_msg.child_id = my_port_index;
  
  test_SP_msg.vuser_id = vptr->user_index;
  test_SP_msg.sync_point_id = vptr->sync_point_id; //Here this is syncgrptblidx
  test_SP_msg.vuser = vptr;
  test_SP_msg.sync_point_type = spGroupTable_shr[vptr->sync_point_id].sp_type;     
  test_SP_msg.grp_idx = vptr->group_num;

  //strcpy(test_SP_msg.sp_name, spGroupTable_shr[syncgrptblidx].sp_name);

  // We are setting state of user here so that is user is selected for ramped down while waiting for reply from parent,
  // then it will know that user is in sync point
  //
  //VUSER_ACTIVE_TO_SYNCPOINT_WAITING(vptr);
  VUSER_ACTIVE_TO_SYNCPOINT_WAITING_WITHOUT_INCREMENTING_COUNTERS(vptr);
  send_child_to_parent_msg("vuser test", (char*)&test_SP_msg, sizeof(SP_msg), CONTROL_MODE);
}


int chk_and_make_test_msg_and_send_to_parent(VUser *vptr, int syncgrptblidx)
{
  int ran_num = -1;
  int sp_tbl_id;

  NSDL1_SP(NULL, NULL, "Method Called, vptr = %p, syncgrptblidx = %d", vptr, syncgrptblidx);

  //First check for active sync point
  //After that check for all group if ALL group is set then no need to check for particular grp
  //IfALL is not set then go for particualr group

  if((FD_ISSET(total_runprof_entries, &(spGroupTable_shr[syncgrptblidx].grpset)) || FD_ISSET(vptr->group_num, &(spGroupTable_shr[syncgrptblidx].grpset))))
  {
    sp_tbl_id = all_grp_entries_shr[(syncgrptblidx * (total_runprof_entries + 1)) + (vptr->group_num + 1)];
    //If this user will take part in syncpoint or not
    //Get random number and check with pct given
    //if fall in range then send msg to parent otherwise return and conitinue with vuser
    ran_num = ns_get_random_number_int(1, 10000); 
    
    NSDL2_SP(NULL, NULL, "ran_num = %d, spGroupTable_shr[%d].pct_usr = %d", ran_num, syncgrptblidx, spGroupTable_shr[syncgrptblidx].pct_usr);

    if(ran_num > g_sync_point_table[sp_tbl_id].sync_pt_usr_pct_as_int)
    { 
      return SP_CONTINUE_WITH_VUSER; 
    } 
  }
  else{
    return SP_CONTINUE_WITH_VUSER; 
  }
 
  vptr->sync_point_id = syncgrptblidx;
  nsi_send_sync_point_msg(vptr);
  return SP_VUSER_GOING_TO_PARTICIPATE_IN_SP;
}

int ns_sync_point_ext(char *sync_name, VUser* vptr)
{
  int hash_code;
  int ran_num; 
  int sp_tbl_id, i;
  SPApiTableLogEntry_shr cur_api;

  NSDL1_SP(NULL, NULL, "Method called. sync_name  = %s ", sync_name);
  //Get hash code of this sync point
  //if hash code found then check for enable disable
  //If enable then go to syncpoint otherwise return
 
  //len = sprintf(sync_buf, "syncpoint_%s", sync_name);
  hash_code = get_sp_api_hash_code(sync_name);
  if(hash_code < 0)
  {
    fprintf(stderr, "Hash code not found for syncpoint %s. It should not happened\n", sync_name);
    return 0;
  }
  
  for (i = 0; i < total_sp_api_found; i++)
  {
    NSDL2_SP(NULL, NULL, " spApiTable_shr[i].api_hash_idx = %d, hash_code = %d \n", spApiTable_shr[i].api_hash_idx, hash_code);
    if(hash_code == spApiTable_shr[i].api_hash_idx)
    {
      cur_api = spApiTable_shr[i];
    }
  }   

  //Got hash code now check if syncpoint is enable for given group

  NSDL1_SP(NULL, NULL, "hash_code = %d, cur_api.sp_grp_tbl_idx = %d \n", hash_code, cur_api.sp_grp_tbl_idx);

  if(cur_api.sp_grp_tbl_idx > -1) 
  {
    NSDL2_SP(NULL, NULL, "api... FD_ISSET(total_runprof_entries, &(spGroupTable_shr[cur_api.sp_grp_tbl_idx].grpset)) = %d, FD_ISSET(vptr->group_num, &(spGroupTable_shr[cur_api.sp_grp_tbl_idx].grpset)) = %d \n", FD_ISSET(total_runprof_entries, &(spGroupTable_shr[cur_api.sp_grp_tbl_idx].grpset)), FD_ISSET(vptr->group_num, &(spGroupTable_shr[cur_api.sp_grp_tbl_idx].grpset)));
   
    if((FD_ISSET(total_runprof_entries, &(spGroupTable_shr[cur_api.sp_grp_tbl_idx].grpset)) || FD_ISSET(vptr->group_num, &(spGroupTable_shr[cur_api.sp_grp_tbl_idx].grpset))))
    { 
      sp_tbl_id = all_grp_entries_shr[(cur_api.sp_grp_tbl_idx * (total_runprof_entries + 1)) + (vptr->group_num + 1)];
      ran_num = ns_get_random_number_int(1, 10000);

      NSDL2_SP(NULL, NULL, "ran_num = %d, spGroupTable_shr[%d].pct_usr = %d", ran_num, cur_api.sp_grp_tbl_idx, spGroupTable_shr[cur_api.sp_grp_tbl_idx].pct_usr);

      if(ran_num > g_sync_point_table[sp_tbl_id].sync_pt_usr_pct_as_int)
      { 
        //if(runprof_table_shr_mem[vptr->group_num].gset.script_mode != NS_SCRIPT_MODE_SEPARATE_THREAD)
        return -1;
      }
    }
    else{ 
      //if(runprof_table_shr_mem[vptr->group_num].gset.script_mode != NS_SCRIPT_MODE_SEPARATE_THREAD)
      return -1;
    }

    vptr->sync_point_id = cur_api.sp_grp_tbl_idx;
    vut_add_task(vptr, VUT_SYNC_POINT);

    //Need to check Context
    //In Thread mode we are already in NVM context so no need to switch b/w context
    if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT) 
    {
      switch_to_nvm_ctx(vptr, "SyncPointApiStart");
      NSDL1_SP(vptr, NULL, "Waiting for sync_point to be over"); 
      return 0;
    }
    else   
    {
      NSDL1_SP(vptr, NULL, "no ctx switch required");   
      return 0;
    }
  }
  return -1;
}

// sync_name is same as transaction name
/*
  This func is called only when sync point is enabled for start_tx API.
  Since SP is not currently supported for dync Tx, it will always return -1.
  It will check whether the vuser will take part in sp or noti based on following:
  1 - Sp is enable for that tx in Vuser Scenario grp
  2 - % of participating users

  If sp is enable
    check grp
    % of partcipating users
    and return 0

  else
    return -1
 
  Return:  0 - If users will take part in sp
          -1 - If users will not take part in sp and will execute tx.
*/
int ns_trans_chk_for_sp(char *sync_name, VUser* vptr)
{
  int hash_code;
  int ran_num; 
  int sp_tbl_id;
  //TxTableEntry txPtr;
  TxTableEntry_Shr txPtr;


  int sync_name_len = strlen(sync_name);

  //Checking for dynamic transaction
  if((sync_name[0] == '{') && (sync_name[sync_name_len - 1] ==  '}'))
  {
    NSDL1_SP(NULL, NULL, "Dynamic SyncPoint discovered..Hence returning");
    return -1;
  }

  NSDL1_SP(NULL, NULL, "Method called, sync_name = %s, sync_name_len = %d", sync_name, sync_name_len);

  //Get hash code of this sync point
  //if hash code found then check for enable disable
  //If enable then go to syncpoint otherwise return

  if(vptr->page_status == NS_REQUEST_RELOAD)
  {
    NSDL2_SP(vptr, NULL, "Returning without starting transaction (%s) as page is reloading.", sync_name);
    return -1;
  }

  if(tx_hash_func == NULL) // Static TX are used and gperf is enable
  {  
    NSDL2_SP(NULL, NULL, "Transaction \"%s\" is not a static transaction..Hence retruning", sync_name);
    return -1;
  } 

  hash_code = tx_hash_func(sync_name, sync_name_len);
  if(hash_code < 0) //case of dyn tx discovered for 1st time
  {
    NSDL2_SP(NULL, NULL, "Hash code not found for syncpoint %s. It should not happened.\n", sync_name);
    return -1;
  }

/*  
  for (i = 0; i < total_tx_entries; i++)
  {
    NSDL2_SP(NULL, NULL, " txTable[i].tx_hash_idx = %d, hash_code = %d \n", tx_table_shr_mem[i].tx_hash_idx, hash_code);
    if(hash_code == tx_table_shr_mem[i].tx_hash_idx)
    {
      vptr->hash_index = hash_code;
      txPtr = tx_table_shr_mem[i];
    }
  }   
*/
  
  vptr->hash_index = hash_code;
  txPtr = tx_table_shr_mem[tx_hash_to_index_table_shr_mem[hash_code]];

  NSDL2_SP(NULL, NULL, "hash_code = %d, txPtr.sp_grp_tbl_idx = %d \n", hash_code, txPtr.sp_grp_tbl_idx);

  //Got hash code now check if syncpoint is enable for given group
  if(txPtr.sp_grp_tbl_idx > -1) 
  {
    NSDL2_SP(NULL, NULL, "FD_ISSET(total_runprof_entries, &(spGroupTable_shr[txPtr.sp_grp_tbl_idx].grpset)) = %d, FD_ISSET(vptr->group_num, &(spGroupTable_shr[txPtr.sp_grp_tbl_idx].grpset)) = %d \n", FD_ISSET(total_runprof_entries, &(spGroupTable_shr[txPtr.sp_grp_tbl_idx].grpset)), FD_ISSET(vptr->group_num, &(spGroupTable_shr[txPtr.sp_grp_tbl_idx].grpset))); 

    if((FD_ISSET(total_runprof_entries, &(spGroupTable_shr[txPtr.sp_grp_tbl_idx].grpset)) || FD_ISSET(vptr->group_num, &(spGroupTable_shr[txPtr.sp_grp_tbl_idx].grpset))))
    {
      sp_tbl_id = all_grp_entries_shr[(txPtr.sp_grp_tbl_idx * (total_runprof_entries + 1)) + (vptr->group_num + 1)];
      ran_num = ns_get_random_number_int(1, 10000);

      NSDL2_SP(vptr, NULL, "ran_num = %d, spGroupTable_shr[%d].pct_usr = %d", ran_num, txPtr.sp_grp_tbl_idx, spGroupTable_shr[txPtr.sp_grp_tbl_idx].pct_usr);

      if(ran_num > g_sync_point_table[sp_tbl_id].sync_pt_usr_pct_as_int)
      {
        return -1; // return -1  means this user will run the transaction
      }
    }
    else {
      return -1;  // return -1  means this user will run the transaction
    }
    NSDL2_SP(vptr, NULL, "Vuser %d will take part in sync-point", vptr->user_index);
    vptr->sync_point_id = txPtr.sp_grp_tbl_idx;
    
    vut_add_task(vptr, VUT_SYNC_POINT);
    NSDL2_SP(vptr, NULL, "Waiting for sync_point to be over"); 

   //Need to check Context
   //In Thread mode we are already in NVM context so no need to switch b/w context
    if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT)
    {
      switch_to_nvm_ctx(vptr, "Transaction SyncPointStart");
      tx_start_with_hash_code(g_sync_point_table[sp_tbl_id].sync_pt_name, 
                         vptr->hash_index, vptr, NS_TX_IS_API_BASED);
    }
    return 0;
  }
  return -1;  // return -1  means this user will run the transaction
}

int convert_rate_to_duration(int num_users, int release_schedule_step)
{
   return (num_users * 60)/release_schedule_step;
}
