/************************************************************************************************
 * File Name            : ns_runtime_changes_quantity.c
 * Author(s)            : Shilpa Sethi
 * Date                 : 23 November 2010
 * Copyright            : (c) Cavisson Systems
 * Purpose              : Parent operations for runtime changes in increase/decrease users/session
 * Modification History :
 *              <Author(s)>, <Date>, <Change Description/Location>
 ***********************************************************************************************/

//---------------- INCLUDE SECTION BEGINS ----------------------------------------
//
#include "ns_cache_include.h"
#include "ns_runtime_changes_quantity.h"
#include "ns_schedule_phases_parse.h" 
#include "ns_parse_scen_conf.h" 
#include "ns_trace_level.h"
#include "ns_runtime_changes.h"
#include "ns_schedule_phases_parse_validations.h"
#include "ns_schedule_pause_and_resume.h"
#include "netomni/src/core/ni_scenario_distribution.h"
#include "nsi_rtc_invoker.h"
#include "ns_data_handler_thread.h"
#include "divide_users.h"
#include "ns_runtime.h"
#include "../../base/libnscore/nslb_sock.h"
//---------------- INCLUDE SECTION ENDS -------------------------------------------


//static u_ns_ts_t test_paused_at;
static u_ns_ts_t total_pause_time;
//u_ns_8B_t rtc_child_bit_mask[4];
extern int sgrp_used_genrator_entries;
extern int first_time;
extern int runtime_id;
extern int nc_flag_set;
//extern int make_data_for_gen(char *err_msg);
//extern char qty_msg_buff[RTC_QTY_BUFFER_SIZE];
extern int sess_inst_num;
extern int g_rtc_msg_seq_num;
//extern int g_rtc_msg_seq_num;
extern int g_quantity_flag;
int phase_end_msg_flag; 
void process_rtc_ph_complete(parent_msg *msg)
{
  Phases *ph;
  parent_child *pc_msg = &(msg->top.internal);
  int grp_idx = pc_msg->grp_idx;
  int rtc_index = pc_msg->rtc_idx;
  char time[0xff];
  int start_idx;
  int num_rtc_for_this_id;
  int rtc_start_idx;
  int i;
  char phase_name[50];

  NSDL1_RUNTIME(NULL, NULL, "Method Called, grp idx = %d, phase idx = %d, child id = %d, RTC idx = %d, pc_msg->rtc_id = %d for control connection", 
                 pc_msg->grp_idx, pc_msg->phase_idx, pc_msg->child_id, rtc_index, pc_msg->rtc_id);

  if(loader_opcode == MASTER_LOADER) { 
    //in case of MASTER loader we need to loop through all rtcs and find the index which has given rtc_id
    for(i = 0; i < (global_settings->num_qty_rtc * total_runprof_entries) ; i++)
    {
      NSDL2_RUNTIME(NULL, NULL, "runtime_schedule[i].rtc_id = %d, pc_msg->rtc_id = %d for control connection", runtime_schedule[i].rtc_id, pc_msg->rtc_id);
      if(runtime_schedule[i].rtc_id == pc_msg->rtc_id)
      {
        rtc_index = i;
        break;
      }
    }
  }

  NSTL1(NULL, NULL, "Received phase complete message for grp idx = %d" 
         " phase completed phase idx = %d, child/generator id = %d, RTC idx = %d" 
         " num children/generator = %d", pc_msg->grp_idx, pc_msg->phase_idx, pc_msg->child_id, rtc_index, 
                                         global_settings->num_process);

  ph = &runtime_schedule[rtc_index].phase_array[pc_msg->phase_idx];
 
  start_idx = runtime_schedule[rtc_index].start_idx;
  NSDL1_RUNTIME(NULL, NULL, "Before total_done_msgs_need_to_come = %d for control connection", runtime_schedule[start_idx].total_done_msgs_need_to_come);
  runtime_schedule[start_idx].total_done_msgs_need_to_come--;
  num_rtc_for_this_id = runtime_schedule[start_idx].total_rtcs_for_this_id;
  rtc_start_idx = start_idx;


  NSDL1_RUNTIME(NULL, NULL, "ph = %p, start_idx = %d, total_done_msgs_need_to_come = %d, num_rtc_for_this_id = %d for control connection", ph, start_idx, runtime_schedule[start_idx].total_done_msgs_need_to_come, num_rtc_for_this_id);

  NSTL1(NULL, NULL, "ph = %p, start_idx = %d, total_done_msgs_need_to_come = %d, num_rtc_for_this_id = %d, rtc_id = %d", ph, start_idx, runtime_schedule[start_idx].total_done_msgs_need_to_come, num_rtc_for_this_id, pc_msg->rtc_id);

  if(loader_opcode == MASTER_LOADER) { //In case of master loader we compare num_process with total_killed_gen variable
    if (pc_msg->grp_idx == -1) {
      if ((runtime_schedule[start_idx].total_done_msgs_need_to_come - g_data_control_var.total_killed_gen) != 0) 
        return;
    } else { //In case of schedule by group, need to verify number of killed generator per group
      if ((runtime_schedule[start_idx].total_done_msgs_need_to_come - runprof_table_shr_mem[pc_msg->grp_idx].num_generator_kill_per_grp) != 0)
        return;
    }
  } else { //In case of standalone and client loader we will be using num processes
      if (runtime_schedule[start_idx].total_done_msgs_need_to_come != 0)
        return;
  }

  if (loader_opcode == CLIENT_LOADER) {
    NSTL1(NULL, NULL, "'%s' phase completed, sending message to controller", ph->phase_name);
    forward_msg_to_master(master_fd, msg, sizeof(parent_child));
    /* Added Phase complete message for generators in global.dat file 
     * as there was issue in gui while viewing phase duration */
    if(!phase_end_msg_flag)
    {
      convert_to_hh_mm_ss(get_ms_stamp() - global_settings->test_start_time, time);
      log_phase_time(PHASE_IS_COMPLETED, 6, ph->phase_name, time);
      phase_end_msg_flag = 1;
    }
    if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO) {
      print2f(rfp, "Phase '%s' (phase %d) was complete at %s\n", 
               ph->phase_name, pc_msg->phase_idx, time);
    } else if (global_settings->schedule_by == SCHEDULE_BY_GROUP) {
      print2f(rfp, "Group '%s' (group %d) phase '%s' (phase %d) was complete at %s\n", 
              runprof_table_shr_mem[grp_idx].scen_group_name, grp_idx, 
              ph->phase_name, pc_msg->phase_idx, time);
    }
    for(i = 0; i < num_rtc_for_this_id; i++)
    {
      runtime_schedule[rtc_start_idx].total_rtcs_for_this_id = 0;
      runtime_schedule[rtc_start_idx].start_idx = -1;
      runtime_schedule[rtc_start_idx].rtc_state = RTC_FREE;
      runtime_schedule[rtc_start_idx].phase_array[0].phase_status = PHASE_NOT_STARTED;
      rtc_start_idx++;
    }
    return;
  } 

  //RTC phase completed
  //Put phase complete in global.dat file 
  //reset RTC structure for new RTC  
  /* Phase Commentary */
  //TODO:Achint check if reset need for shared RTC also
  
  for(i = 0; i < num_rtc_for_this_id; i++)
  {
    runtime_schedule[rtc_start_idx].total_rtcs_for_this_id = 0;
    runtime_schedule[rtc_start_idx].start_idx = -1;
    runtime_schedule[rtc_start_idx].rtc_state = RTC_FREE;
    runtime_schedule[rtc_start_idx].phase_array[0].phase_status = PHASE_NOT_STARTED;
    rtc_start_idx++;
  }

  convert_to_hh_mm_ss(get_ms_stamp() - global_settings->test_start_time, time);
  if(loader_opcode == MASTER_LOADER)  
  {
    NSTL1(NULL, NULL, "rtc_id = %d", pc_msg->rtc_id);
    sprintf(phase_name, "RTC_PHASE_%d", pc_msg->rtc_id);
    log_phase_time(PHASE_IS_COMPLETED, 6, phase_name, time);
  }
  else
    log_phase_time(PHASE_IS_COMPLETED, 6, ph->phase_name, time);

  if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO) {
    print2f(rfp, "Phase '%s' (phase %d) was complete at %s\n", 
            (loader_opcode == MASTER_LOADER)?phase_name:ph->phase_name, pc_msg->phase_idx, time);
  } else if (global_settings->schedule_by == SCHEDULE_BY_GROUP) {
    print2f(rfp, "Group '%s' (group %d) phase '%s' (phase %d) was complete at %s\n", 
            runprof_table_shr_mem[grp_idx].scen_group_name, grp_idx, 
            (loader_opcode == MASTER_LOADER)?phase_name:ph->phase_name, pc_msg->phase_idx, time);
  }
}

//------------------MESSAGING/LOGGING SECTION BEGINS------------------------------

//Purpose: Used for settng output message...when RTC is successful

inline static void fill_output_msg(int grp_idx, int quantity, 
                           int quantity_left_to_remove,  char *err_msg, int session_rtc)
{
  //s_child_ports *v_port_entry_ptr;
  Schedule *cur_schedule;
  int grp_type = get_group_mode(grp_idx);
  int proc_index = global_settings->num_process - 1;
  //v_port_entry_ptr = &v_port_table[proc_index];

  if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO) 
    cur_schedule = v_port_table[proc_index].scenario_schedule;
  else
    cur_schedule = &(v_port_table[proc_index].group_schedule[grp_idx]);

  int phase_idx = cur_schedule->phase_idx;
  //Applying RTC in start phase in case group is not yet started
  if (phase_idx < 0)
    phase_idx = 0;

  NSDL3_RUNTIME(NULL, NULL, "Applying RTC in Phase Name = %s, phase_idx=%d", cur_schedule->phase_array[cur_schedule->phase_idx].phase_name, cur_schedule->phase_idx);
  if (grp_type == TC_FIX_USER_RATE) 
    sprintf(err_msg,"%.3f Session(s) rate per minute %s group '%s' in phase '%s'", 
                 abs(quantity - quantity_left_to_remove)/THOUSAND,   //No. of Sessions
                 ((quantity > 0)? "Increased in":"Decreased from"),
                 (runprof_table_shr_mem[grp_idx].scen_group_name),
                 cur_schedule->phase_array[phase_idx].phase_name);   //Phase Name      
  else if (!session_rtc)
    sprintf(err_msg,"%d User(s) %s group '%s' in phase '%s'", 
                 abs(quantity - quantity_left_to_remove),   //No. of Users
                 ((quantity > 0)? "Added in":"Removed from"),
                 (runprof_table_shr_mem[grp_idx].scen_group_name),
                 cur_schedule->phase_array[phase_idx].phase_name);   //Phase Name                
  else 
    sprintf(err_msg,"%d Session(s) %s group '%s' in phase '%s'", 
                 abs(quantity - quantity_left_to_remove),   //No. of Users
                 ((quantity > 0)? "Added in":"Removed from"),
                 (runprof_table_shr_mem[grp_idx].scen_group_name),
                 cur_schedule->phase_array[phase_idx].phase_name);   //Phase Name                
 
}

static void runtime_dump_qty_distribution_among_nvm(int grp_idx, int quantity, int runtime_index)
{
  char *type;
  int proc_index;
  float change_quantity[2];
  //s_child_ports *v_port_entry_ptr;
  Schedule *schedule_ptr, *rtc_schedule_ptr;
  float cur_vusers_or_sess = 0;

  type = get_grptype(grp_idx);
  
  NSDL2_RUNTIME(NULL, NULL, "Method Called. grp_idx=%d, quantity=%d", grp_idx, quantity);

  printf("Runtime User/Session Rate Distribution among NVMs:\n");

  int grp_type = get_group_mode(grp_idx);
 
  for (proc_index = 0; proc_index < global_settings->num_process; proc_index++)
  {
    //v_port_entry_ptr = &v_port_table[proc_index];
    if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO) 
      schedule_ptr = v_port_table[proc_index].scenario_schedule;
    else
      schedule_ptr = &(v_port_table[proc_index].group_schedule[grp_idx]);

    if (grp_type == TC_FIX_USER_RATE) 
      cur_vusers_or_sess += schedule_ptr->cur_vusers_or_sess / THOUSAND;
    else
      if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO) 
        cur_vusers_or_sess += schedule_ptr->cur_users[grp_idx];
      else
        cur_vusers_or_sess += schedule_ptr->cur_vusers_or_sess;
  }

  if (grp_type == TC_FIX_USER_RATE) 
    printf("Group=%s, Scenario Type=%s, Existing Sessions=%.03f, Change in Sessions at runtime=%.03f\n", 
                               runprof_table_shr_mem[grp_idx].scen_group_name, type, 
                               cur_vusers_or_sess, quantity/THOUSAND);
  else
    printf("Group=%s, Scenario Type=%s, Existing Users=%d, Change in Users at runtime=%d\n", 
           runprof_table_shr_mem[grp_idx].scen_group_name, type, (int)cur_vusers_or_sess, quantity);

  cur_vusers_or_sess = 0;
  for (proc_index = 0; proc_index < global_settings->num_process; proc_index++)
  {
    //v_port_entry_ptr = &v_port_table[proc_index];
    s_child_ports *v_port_entry_ptr = &v_port_table[proc_index];
    void *shr_mem_ptr = v_port_entry_ptr->runtime_schedule;
    rtc_schedule_ptr = shr_mem_ptr + (runtime_index * find_runtime_qty_mem_size());
    
    if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO)
    {
      schedule_ptr = v_port_table[proc_index].scenario_schedule;
    }
    else
    {
      schedule_ptr = &(v_port_table[proc_index].group_schedule[grp_idx]);
    }

    if(schedule_ptr->runtime_qty_change[0] > 0)
    {
      change_quantity[0] = rtc_schedule_ptr->runtime_qty_change[0];
    }
    else
    {
      change_quantity[0] = rtc_schedule_ptr->runtime_qty_change[0];
      change_quantity[1] = rtc_schedule_ptr->runtime_qty_change[1];
    }
    
    NSDL1_RUNTIME(NULL, NULL,"Change Users/Sessions=%.3f for NVM=%d", change_quantity[0], proc_index);

    if(grp_type == TC_FIX_CONCURRENT_USERS) {
      if(change_quantity[0] > 0) {
        per_proc_runprof_table[(proc_index * total_runprof_entries) + grp_idx] += change_quantity[0];
        NSDL1_RUNTIME(NULL, NULL, "Quantity on NVM %d is %d", proc_index, 
                                   per_proc_runprof_table[(proc_index * total_runprof_entries) + grp_idx]);
      } else {
        per_proc_runprof_table[(proc_index * total_runprof_entries) + grp_idx] += change_quantity[0]; 
        NSDL1_RUNTIME(NULL, NULL, "Quantity on NVM %d is %d", proc_index, 
                                   per_proc_runprof_table[(proc_index * total_runprof_entries) + grp_idx]);
      }
    } else {
      if(change_quantity[0] > 0) {
        per_proc_runprof_table[(proc_index * total_runprof_entries) + grp_idx] += change_quantity[0];
        NSDL1_RUNTIME(NULL, NULL, "Quantity on NVM %d is %d", proc_index, 
                                   per_proc_runprof_table[(proc_index * total_runprof_entries) + grp_idx]);
      } else {
        per_proc_runprof_table[(proc_index * total_runprof_entries) + grp_idx] += change_quantity[0];
        NSDL1_RUNTIME(NULL, NULL, "Quantity on NVM %d is %d", proc_index, 
                                   per_proc_runprof_table[(proc_index * total_runprof_entries) + grp_idx]);
      }
    }



    if (grp_type == TC_FIX_USER_RATE) 
    {
      change_quantity[0] /= THOUSAND;
      change_quantity[1] /= THOUSAND;
      cur_vusers_or_sess = schedule_ptr->cur_vusers_or_sess / THOUSAND;
    }
    else
      cur_vusers_or_sess = schedule_ptr->cur_vusers_or_sess;


    if (grp_type == TC_FIX_CONCURRENT_USERS) 
    {
      if (global_settings->schedule_by == SCHEDULE_BY_GROUP) 
      {
        printf("\tExisting Users in NVM%d: %d", proc_index, (int)cur_vusers_or_sess);
        NSDL1_RUNTIME(NULL, NULL, "Existing Users in NVM%d: %d", proc_index, (int)cur_vusers_or_sess);
      }
      else
      {
        printf("\tExisting Users in NVM%d: %d", proc_index, schedule_ptr->cur_users[grp_idx]);
        NSDL1_RUNTIME(NULL, NULL, "Existing Users in NVM%d: %d", proc_index, schedule_ptr->cur_users[grp_idx]);
      }

      if(change_quantity[0] > 0)
      {
        printf("\tUsers Added=%d\n", (int)change_quantity[0]);
        NSDL1_RUNTIME(NULL, NULL,"Users Added=%d", (int)change_quantity[0]);
      }
      else
      {
        printf("\tRamped-up Users Removed=%d\tNon-Ramped-up Users Removed=%d\n", (int)change_quantity[0], (int)change_quantity[1]);
        NSDL1_RUNTIME(NULL, NULL, "Ramped-up Users Removed=%d    Non-Ramped-up Users Removed=%d\n", (int)change_quantity[0], (int)change_quantity[1]);
      }
    }
    else  //FSR
    {
      printf("\tExisting Sessions in NVM%d: %.3f", proc_index, cur_vusers_or_sess);
      NSDL1_RUNTIME(NULL, NULL, "Existing Sessions in NVM%d: %.3f", proc_index, cur_vusers_or_sess);

      if(change_quantity[0] > 0)
      {
        printf("\tSessions Added=%.3f\n", change_quantity[0]);
        NSDL1_RUNTIME(NULL, NULL, "Sessions Added=%.3f\n", change_quantity[0]);
      }
      else
      {

        printf("\tRamped-up Sessions Removed=%.3f\tNon-Ramped-up Session Removed=%.3f\n", change_quantity[0], change_quantity[1]);
        NSDL1_RUNTIME(NULL, NULL, "\tRamped-up Sessions Removed=%.3f\tNon-Ramped-up Session Removed=%.3f\n", change_quantity[0], change_quantity[1]);
      }
    }
  }
}

//------------------MESSAGING/LOGGING SECTION BEGINS------------------------------


//------------------NVM'S INFO RELATED SECTION BEGINS ------------------------------

/*Purpose: *To find all NVM's which are still running
           *Set the index for NVMs serving the group in nvm_ids array. 
*/
int is_process_still_active(int grp_idx, int proc_index)
{
  s_child_ports *v_port_entry_ptr;
  Schedule *cur_schedule;
  int process_active = 1; 
  int i;
 
  NSDL2_RUNTIME(NULL, NULL, "Method Called grp_idx=%d", grp_idx);
  for(i = 0; i < global_settings->num_process; i++)
  {
    if(g_msg_com_con[i].nvm_index == proc_index)
      if(g_msg_com_con[i].fd == -1)
      {
        process_active = 0;
        NSDL3_RUNTIME(NULL, NULL, "Process %d has stopped running", proc_index);
        break;
      }
  }
  if(!process_active) 
    return 0;

  NSDL3_RUNTIME(NULL, NULL, "Process %d is still active", proc_index);

  //Bug#3965 This is used in case of Schedule by Group when on an NVM, one group is over however NVM is not over (hasen't send its finish report)
  //And we are adding users at that point of time, users were getting added and causing the test to keep on running.
  //Need to check if all the phases for that group (in which we need to add users) are done, mark the nvm for not distributing the runtime quantity 
  //Algo: Check if the NVM is in last phase & its bit in the bitmask is set for phase complete for the effective group

  v_port_entry_ptr = &v_port_table[proc_index];
  if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO)
    cur_schedule = v_port_entry_ptr->scenario_schedule;
  else
    cur_schedule = &(v_port_entry_ptr->group_schedule[grp_idx]);

  NSDL3_RUNTIME(NULL, NULL, "Process %d, phase_idx=%d, num_phases=%d", proc_index, cur_schedule->phase_idx, cur_schedule->num_phases);
  if((cur_schedule->phase_idx == (cur_schedule->num_phases -1)) && 
     (cur_schedule->phase_array[cur_schedule->phase_idx].phase_type == SCHEDULE_PHASE_RAMP_DOWN))
  {
    NSDL3_RUNTIME(NULL, NULL, "Process %d is in its last phase", proc_index);
#ifdef NS_DEBUG_ON
    NSDL2_MESSAGES(NULL, NULL, "%s", nslb_show_bitflag(cur_schedule->bitmask));
#endif
    /* Bug 81793: Restricted Qty rtc on rampdown phase
    if(nslb_check_bit_reset(cur_schedule->bitmask, proc_index)) 
    {
      //Phase complete done
      NSDL3_RUNTIME(NULL, NULL, "Process %d has completed processing all the phases of group %d,"
                                " hence will not be counted for distributing quantity at runtime", 
                                proc_index, grp_idx);
    }*/
    return 0;
  }
  /*This check is due to following reason:-
    In case some group has multiple NVMs(say 8) and out of 8 NVMs suppose (2) NVMs has completed duration phase and change their status to 
    PHASE_IS_COMPLETED but waiting for other NVMs to change their status to PHASE_IS_COMPLETED. 
    This will cause problem as data will distribute in NVMs which has change their status to PHASE_IS_COMPLETED.
  */
  if((global_settings->schedule_type == SCHEDULE_TYPE_SIMPLE ) && 
         ((cur_schedule->phase_array[cur_schedule->phase_idx].phase_type == SCHEDULE_PHASE_DURATION) && 
          (cur_schedule->phase_array[cur_schedule->phase_idx].phase_status == PHASE_IS_COMPLETED)))
  {
    NSDL3_RUNTIME(NULL, NULL, "Process %d has compelted processing the DURATION phases of group %d "
                              "hence will not be counted for distribtuting quantity at runtime", 
                               proc_index, grp_idx);
      return 0;
  }
  return 1;
}

/*Purpose: *To find all NVM's which are handling that scenario group using 
            per_proc_runprof_table[]. 
           *Set the index for NVMs serving the group in nvm_ids array. 
  Output Parameter: nvm_count-Returns number of NVMs serving the group for which 
                    runtime change quantity request came 
*/
void runtime_get_process_cnt_serving_grp(char nvm_ids[], int *nvm_count, int grp_idx)
{
  int proc_index, proc_grp_index;
  int mark_nvm;
  char nvm_idx[MAX_NVM_NUM];  
 
  NSDL2_RUNTIME(NULL, NULL, "Method Called grp_idx=%d", grp_idx);

  memcpy(nvm_idx, nvm_ids, MAX_NVM_NUM);

  for(proc_index = 0, mark_nvm = 0; proc_index < global_settings->num_process; proc_index++)
  {
    mark_nvm = 0;
    proc_grp_index = proc_index * total_runprof_entries + grp_idx;
    NSDL4_RUNTIME(NULL, NULL, "Checking for process %d serving group %d, per_proc_runprof_table[%d]=%d", 
                 proc_index, grp_idx, proc_grp_index, per_proc_runprof_table[proc_grp_index]);

    if(per_proc_runprof_table[proc_grp_index] <= 0)
    {
      NSDL3_RUNTIME(NULL, NULL, "Process %d is not serving group %d", proc_index, grp_idx);
    }
    else if(is_process_still_active(grp_idx, proc_index))
    {
      //Check if group is over
      if(is_group_over(grp_idx, proc_index))
        NSDL3_RUNTIME(NULL, NULL, "Group %d is over on process %d", grp_idx, proc_index);
      else
      {
        mark_nvm = 1;
        NSDL3_RUNTIME(NULL, NULL, "Process %d is active & running group %d", proc_index, grp_idx);
      }
    }  
    else
    {
      NSDL3_RUNTIME(NULL, NULL, "Process %d is not active or not running group %d or group is over", proc_index, grp_idx);
    }

    if(mark_nvm)
    {
      nvm_ids[proc_index] = 1;
      *nvm_count += 1;
    }
  } 

  NSDL2_RUNTIME(NULL, NULL, "Exiting Method, Total Nvm's serving group=%d is %d", grp_idx, *nvm_count);
}

void runtime_get_nvm_quantity(int grp_idx, int ramped_up_users_or_sess, int available_qty_to_remove[])
{
 
  int proc_index;
  s_child_ports *v_port_entry_ptr;
  Schedule *cur_schedule; 
  int grp_type, phase_id, not_ramped_up_qty;
  Ramp_up_schedule_phase *ramp_up_phase_ptr;

  grp_type = get_group_mode(grp_idx);

  NSDL3_RUNTIME(NULL, NULL, "Method Called. grp_idx=%d = ramped_up_users_or_sess=%d", grp_idx, ramped_up_users_or_sess);
  for (proc_index = 0; proc_index < global_settings->num_process; proc_index++)
  {
    v_port_entry_ptr = &v_port_table[proc_index];

    if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO) 
      cur_schedule = v_port_entry_ptr->scenario_schedule;
    else
      cur_schedule = &(v_port_entry_ptr->group_schedule[grp_idx]);
  
    if(ramped_up_users_or_sess == REMOVE_RAMPED_UP_VUSERS)
    {
      if(grp_type == TC_FIX_CONCURRENT_USERS)
      { 
        if(global_settings->schedule_by == SCHEDULE_BY_SCENARIO)
          available_qty_to_remove[proc_index] = cur_schedule->cur_users[grp_idx];
        else  //SCHEDULE_BY_GROUP
          available_qty_to_remove[proc_index] = cur_schedule->cur_vusers_or_sess;
      }
      else  //FSR
      {
        if(global_settings->schedule_by == SCHEDULE_BY_SCENARIO)
          available_qty_to_remove[proc_index] = (cur_schedule->cur_vusers_or_sess * get_fsr_grp_ratio(cur_schedule, grp_idx));
        else
          available_qty_to_remove[proc_index] = cur_schedule->cur_vusers_or_sess;
      }
    }
    else  //REMOVE_NOT_RAMPED_UP_VUSERS
    {
      not_ramped_up_qty = 0;
      for(phase_id = cur_schedule->phase_idx; phase_id < cur_schedule->num_phases; phase_id++)
      {
        if(cur_schedule->phase_array[phase_id].phase_type == SCHEDULE_PHASE_RAMP_UP)
        {
          ramp_up_phase_ptr = &cur_schedule->phase_array[phase_id].phase_cmd.ramp_up_phase;
          NSDL3_RUNTIME(NULL, NULL, "Ramp-up phase found at phase_id = %d moving in forward direction", phase_id);
        
          if(grp_type == TC_FIX_CONCURRENT_USERS)
          {
            NSDL3_RUNTIME(NULL, NULL, "Existing not RU users=%d, Not RU users found at phase %d=%d, ramp_up_phase_ptr->num_vusers_or_sess=%d, ramp_up_phase_ptr->ramped_up_vusers=%d", 
                        not_ramped_up_qty, phase_id, ramp_up_phase_ptr->num_vusers_or_sess - ramp_up_phase_ptr->ramped_up_vusers, ramp_up_phase_ptr->num_vusers_or_sess, ramp_up_phase_ptr->ramped_up_vusers);
            if(global_settings->schedule_by == SCHEDULE_BY_SCENARIO)
              not_ramped_up_qty += ramp_up_phase_ptr->per_grp_qty[grp_idx];
            else
              not_ramped_up_qty += (ramp_up_phase_ptr->num_vusers_or_sess - ramp_up_phase_ptr->ramped_up_vusers);
          }
          else //FSR
          {
            //NSDL3_RUNTIME(NULL, NULL, "Ramp-up phase found at phase_id = %d moving in forward direction", phase_id);
            if(global_settings->schedule_by == SCHEDULE_BY_SCENARIO)
            {
              //not_ramped_up_qty += cur_schedule->cumulative_runprof_table[grp_idx] - cur_schedule->cumulative_runprof_table[grp_idx - 1];
              not_ramped_up_qty += ((ramp_up_phase_ptr->num_vusers_or_sess - ramp_up_phase_ptr->max_ramp_up_vuser_or_sess) 
                                             * get_fsr_grp_ratio(cur_schedule, grp_idx));
            }
            else
            {
              not_ramped_up_qty += (ramp_up_phase_ptr->num_vusers_or_sess - ramp_up_phase_ptr->max_ramp_up_vuser_or_sess);
            }
          }
        }  
      }  //for - phase loop
      available_qty_to_remove[proc_index] = not_ramped_up_qty;
    }
    NSDL3_RUNTIME(NULL, NULL, "NVM %d available_qty_to_remove = %d", proc_index, available_qty_to_remove[proc_index]);
  }  //for - proc loop
  
}

//------------------NVM'S INFO RELATED SECTION ENDS ------------------------------


//------------------DISTRIBUTE QUANTITY SECTION BEGINS------------------------------
//

//ramped_up_users_or_sess - Used as second index in runtime_qty_change
//Added quantity is stored at runtime_qty_change[grp_idx][0]
//Removed quantity - Ramped up users/sessions is stored at runtime_qty_change[grp_idx][0]
//Removed quantity - Not Ramped up users/sessions is stored at runtime_qty_change[grp_idx][1]
static void set_distributed_quantity(int grp_idx, int nvm_quantity[], int ramped_up_users_or_sess, int running_idx)
{
  int proc_index;
  s_child_ports *v_port_entry;
  Schedule *cur_schedule;
  void *shr_mem_ptr;

  NSDL3_RUNTIME(NULL, NULL, "Method Called. grp_idx=%d, ramped_up_users_or_sess=%d", grp_idx, ramped_up_users_or_sess);

  NSDL3_RUNTIME(NULL, NULL, "Runtime Distribution:");
  for (proc_index = 0; proc_index < global_settings->num_process; proc_index++)
  {
    v_port_entry = &v_port_table[proc_index];
    shr_mem_ptr = v_port_entry->runtime_schedule;
    cur_schedule = shr_mem_ptr + (running_idx * find_runtime_qty_mem_size()); 
    cur_schedule->runtime_qty_change[ramped_up_users_or_sess] = nvm_quantity[proc_index];
    NSDL4_RUNTIME(NULL, NULL, "Current Schedule: cur_schedule = %p, running_phase_idx=%d," 
                  "NVM%d, cur_schedule->runtime_qty_change[%d]=%d", cur_schedule, running_idx, proc_index, 
                  ramped_up_users_or_sess, cur_schedule->runtime_qty_change[ramped_up_users_or_sess]);
  }
  NSDL3_RUNTIME(NULL, NULL, "Exitting Method.");
}

int runtime_distribute_fetches(char nvm_ids[], int num_fetches, char *err_msg)
{

  NSDL2_RUNTIME(NULL, NULL, "Method Called. num_fetches=%d", num_fetches);
  int i, num_val, used_val = 0, leftover; 
  int total_vals = 0;
  int total_users = 0;
  int is_dec_quantity = 0;

  if(num_fetches > 0)
    total_vals = num_fetches;
  else
  {
    //In case of RunTime Decrease sessions num_fetches is negative we makes it positive and enable dec_quantity flag
    total_vals = -num_fetches;
    is_dec_quantity = 1;
  }
  
  //Find total users 
  for (i = 0; i < global_settings->num_process; i++){
    //Skip if nvm is not active and active nvm does not have users.
    if(!(nvm_ids[i] && v_port_table[i].num_vusers)){
      NSDL2_SCHEDULE(NULL, NULL, "skipping nvm as nvm_ids[%d] = %d, v_port_table[%d].num_vusers tal_users = %d", 
                                               i, nvm_ids[i], i, v_port_table[i].num_vusers);
      continue;
    } 
    total_users += v_port_table[i].num_vusers;
  }

  if(total_users == 0)
  {
    sprintf(err_msg, "Error: No running VUser is availabe in system or all schedule phases are completed");
    return RUNTIME_ERROR;
  }
  
  NSDL2_SCHEDULE(NULL, NULL, "RTC: total_vals = %d, is_dec_quantity = %d, total_users = %d", total_vals, is_dec_quantity, total_users);
  
  for (i = 0; i < global_settings->num_process; i++)
  {
    //Skip if nvm is not active and active nvm does not have users.
    if(!(nvm_ids[i] && v_port_table[i].num_vusers))
      continue;

    num_val = (total_vals * v_port_table[i].num_vusers)/total_users;

    if(!is_dec_quantity)
      v_port_table[i].num_fetches += num_val;
    else
      v_port_table[i].num_fetches -= num_val;

    used_val += num_val;
  }

  leftover = total_vals - used_val;
  //Now fill leftover
  i = 0;
  while(leftover)
  {
    //check if nvm is alive or not.
    if((nvm_ids[i] && v_port_table[i].num_vusers))
    {  
      if(!is_dec_quantity)
        v_port_table[i].num_fetches++;
      else
        v_port_table[i].num_fetches--;
      leftover--;
    }
    i++;
    if (i == global_settings->num_process) i = 0;
  }
  //if num_fetches is set -ve at the time of decreasing then we set it to -1 forcefully.
  for (i = 0; i < global_settings->num_process; i++){
    if(v_port_table[i].num_fetches <= 0)
      v_port_table[i].num_fetches = -1;
    NSDL3_SCHEDULE(NULL, NULL, "RTC: NVM=%d  users=%d fetches=%d\n", i, v_port_table[i].num_vusers, v_port_table[i].num_fetches);
  }

  return RUNTIME_SUCCESS;
}
 
int runtime_distribute_fetchs_over_nvms_and_grps(char nvm_ids[], int fetches, int nvm_count, int grp_idx, char *err_msg)
{


  int num_fetches, cur_proc;
  int for_all, leftover;
  int j; 
  int is_dec_quantity = 0;

  NSDL2_RUNTIME(NULL, NULL, "Method Called. grp_idx=%d, fetches=%d, nvm_count = %d", grp_idx, fetches, nvm_count);
  
  if(fetches < 0)
  {
    num_fetches = -fetches;
    is_dec_quantity = 1;
  }
  else
    num_fetches = fetches; 

  /* Step 2: distribute fetches as if for load key == 1 */
  if(global_settings->nvm_distribution == 0 ) { //Default. all groups are distributed equally over NVM's
    NSDL2_SCHEDULE(NULL, NULL, "NVM Distribution Mode=0");


    //Handle per user fetches
    for_all = num_fetches / nvm_count;
    leftover = num_fetches % nvm_count;

    NSDL2_SCHEDULE(NULL, NULL, "Group Id = %d, num_fetches =  %d, for_all = %d, leftover = %d, is_dec_quantity = %d", 
                         grp_idx, num_fetches, for_all, leftover, is_dec_quantity);
    for (j = 0; j < global_settings->num_process; j++) {
      //check if nvm is alive or not and that nvm must have users.
      if(!(nvm_ids[j] && per_proc_runprof_table[(j * total_runprof_entries) + grp_idx]))
        continue;

      if(!is_dec_quantity) 
      {
        per_proc_per_grp_fetches[(j * total_runprof_entries) + grp_idx] += for_all;
        v_port_table[j].num_fetches += for_all;
      }
      else
      {
        per_proc_per_grp_fetches[(j * total_runprof_entries) + grp_idx] -= for_all;
        v_port_table[j].num_fetches -= for_all;
      }
    }
    if(leftover) 
    { 
      NSDL2_SCHEDULE(NULL, NULL, "leftover = %d", leftover);
      cur_proc = 0;
      for (j = 0; j < leftover; ) {
        //check if nvm is alive or not 
        if((nvm_ids[cur_proc] && per_proc_runprof_table[(cur_proc * total_runprof_entries) + grp_idx]))
        {
          if(!is_dec_quantity)
          {
            per_proc_per_grp_fetches[(cur_proc * total_runprof_entries) + grp_idx]++;
            v_port_table[cur_proc].num_fetches++;
          }
          else
          {
            per_proc_per_grp_fetches[(cur_proc * total_runprof_entries) + grp_idx]--;
            v_port_table[cur_proc].num_fetches--;
          }
          j++;
        }
        NSDL2_SCHEDULE(NULL, NULL, "cur_proc = %d", cur_proc);
        cur_proc++;
        if(cur_proc == global_settings->num_process) 
          cur_proc = 0;           /* This ensures that we fill left over for new group to the next NVM */
      }
    }
    //Atul: If num_fetches is 0 then set it to -1 forcefully.
    //In case if there are 'n' fetches and decreased 'n' fetches at runtime then fetches reduced to '0' and test will go in 
    //infinite duration mode as num_fetches are also '0' in case of FSR mode. 
    //Hence setting it to '-1' forcefully to distinguised from FSR mode.
    for (j = 0; j < global_settings->num_process; j++) {
      if(per_proc_per_grp_fetches[(j * total_runprof_entries) + grp_idx] <= 0)
        per_proc_per_grp_fetches[(j * total_runprof_entries) + grp_idx] = -1;
      if (v_port_table[j].num_fetches <= 0) 
        v_port_table[j].num_fetches = -1; 
    }
  } else if ( global_settings->nvm_distribution == 1 ) { //All groups are distrbuted in a way to putc one script in one NVM
    NSDL2_SCHEDULE(NULL, NULL, "NVM Distribution Mode = 1");
    NSDL2_RUNTIME(NULL, NULL, "Schedule by group does not support NVM Distribution Mode 1 in group based sessions"); 
    sprintf(err_msg, "Schedule by group does not support NVM Distribution Mode 1 in group based sessions");
    return RUNTIME_ERROR;
  } else {//User defined distribution
      NSDL2_SCHEDULE(NULL, NULL, "NVM Distribution Mode=2");
      RunProfTableEntry_Shr* rstart = runprof_table_shr_mem;
      rstart = &runprof_table_shr_mem[grp_idx];
      if(!(nvm_ids[rstart->cluster_id] && per_proc_runprof_table[(rstart->cluster_id * total_runprof_entries) + grp_idx])){
        NSDL2_RUNTIME(NULL, NULL, "Cannot apply runtime changes to group=%s as user running on this group are over OR group is over", 
                                                              runprof_table_shr_mem[grp_idx].scen_group_name);
        sprintf(err_msg, "Cannot apply runtime changes to group=%s as user running on this group are over OR group is over", 
                                                              runprof_table_shr_mem[grp_idx].scen_group_name);
        return RUNTIME_ERROR;
      }
      if(!is_dec_quantity){
        per_proc_per_grp_fetches[(rstart->cluster_id * total_runprof_entries) + grp_idx] += num_fetches;
        v_port_table[rstart->cluster_id].num_fetches += num_fetches;
      }
      else
      {
        per_proc_per_grp_fetches[(rstart->cluster_id * total_runprof_entries) + grp_idx] -= num_fetches;
        v_port_table[rstart->cluster_id].num_fetches -= num_fetches;
      }
      //Atul: If num_fetches is 0 then set it to -1 forcefully.
      //In case if there are 'n' fetches and decreased 'n' fetches at runtime then fetches reduced to '0' and test will go in 
      //infinite duration mode as num_fetches are also '0' in case of FSR mode. 
      //Hence setting it to '-1' forcefully to distinguised from FSR mode.
      if(per_proc_per_grp_fetches[(rstart->cluster_id * total_runprof_entries) + grp_idx] <= 0)
        per_proc_per_grp_fetches[(rstart->cluster_id * total_runprof_entries) + grp_idx] = -1;
      if (v_port_table[rstart->cluster_id].num_fetches <= 0) 
        v_port_table[rstart->cluster_id].num_fetches = -1; 
  }

#ifdef NS_DEBUG_ON
  int i;
  for (i = 0; i < total_runprof_entries; i++) {
    for (j = 0; j < global_settings->num_process; j++) {
      NSDL4_SCHEDULE(NULL, NULL, "NVM Distribution: Group id = %d, NVM = %d, QTY = %d, Session = %d", i, j, 
                     per_proc_runprof_table[(j * total_runprof_entries) + i], per_proc_per_grp_fetches[(j * total_runprof_entries) + i]);
    }
  }

  for (j = 0; j < global_settings->num_process; j++) {
    NSDL4_SCHEDULE(NULL, NULL, "nvm id = %d, total fetches = %d", j, v_port_table[j].num_fetches);
  }
#endif

  return RUNTIME_SUCCESS;
}

static int runtime_distribute_added_quantity(char nvm_idx[], int nvm_count, int grp_idx, int quantity, int nvm_quantity[], int *actual_nvm_count)
{
  int for_all, balance, j, proc_index;

  NSDL2_RUNTIME(NULL, NULL, "Method Called. grp_idx=%d, quantity=%d", grp_idx, quantity);

  for_all = (quantity) / nvm_count;
  balance = (quantity) % nvm_count;
  NSDL3_RUNTIME(NULL, NULL, "Distribution for added quantity. for_all=%d, balance=%d", for_all, balance);

  for (proc_index = 0; proc_index < global_settings->num_process; proc_index++)
  {
    if(nvm_idx[proc_index])
      nvm_quantity[proc_index] += for_all;
  }

  for (j = 0, proc_index=0; j < balance/*proc_index < global_settings->num_process*/; proc_index++) 
  {
    if(nvm_idx[proc_index]) {
      nvm_quantity[proc_index]++;
      NSDL3_RUNTIME(NULL, NULL, "nvm_quantity[%d] = %d", proc_index, nvm_quantity[proc_index]);
      j++;
    }
  }
  for (proc_index = 0; proc_index < global_settings->num_process; proc_index++)
  {
    if (nvm_quantity[proc_index])
      (*actual_nvm_count)++;
  }

  NSDL2_RUNTIME(NULL, NULL, "Exiting Method, Actual nvm count =%d", *actual_nvm_count);
  return RUNTIME_SUCCESS;
}

static int runtime_distribute_removed_quantity(char orig_nvm_ids[], int nvm_count, int grp_idx, int quantity, 
                          int nvm_quantity[], int ramped_up_users_or_sess, int *actual_nvm_count)
{
//  int nvm_quantity[MAX_NVMS] = {0};     //Stores distributed quantity per nvm
  int for_all, proc_index = 0;
  int total_leftover_qty_to_remove = 0;  //users/sessions which are not yet distributed
  int redistribution_flag = 0;
  int nvms_left_for_distribution = 0;   
  int assigned_qty;
  int available_qty_to_remove[MAX_NVM_NUM] = {0};  //Stores users/sessios currently existing per NVM - available to remove
  char nvm_idx[MAX_NVM_NUM];  
 
  NSDL3_RUNTIME(NULL, NULL,"Method Called. grp_idx=%d, quantity=%d, ramped_up_users_or_sess=%d\n", grp_idx, quantity, ramped_up_users_or_sess);

  //Making local copy of nvm array
  memcpy(nvm_idx, orig_nvm_ids, MAX_NVM_NUM);
  //Storing quantity as -ve for NVMs to identify increase/decrease of users/sessions 
  quantity = abs(quantity);
    
  //1. Divide equal quantity among nvms
  total_leftover_qty_to_remove = quantity;
  for_all = (quantity) / nvm_count;
  NSDL2_RUNTIME(NULL, NULL, "Process wise distributed quantity for group=%d...for_all=%d", grp_idx, for_all);

  //2. Get available quantity to remove ramped_up/not_ramped_up users/sessions
  runtime_get_nvm_quantity(grp_idx, ramped_up_users_or_sess, available_qty_to_remove);

  do 
  {
    NSDL2_RUNTIME(NULL, NULL, "NVM's available for distribution=%d", nvm_count);
    if(redistribution_flag)
    {
      for_all = (total_leftover_qty_to_remove)/nvm_count;
      redistribution_flag = 0;
      NSDL2_RUNTIME(NULL, NULL, "REDISTRIBUTION set...After redistibution quantity=%d among %d NVMs..for_all = %d", 
                                                  total_leftover_qty_to_remove, nvm_count, for_all);
    }
    if (!for_all) break;

    for(proc_index = 0; proc_index < global_settings->num_process; proc_index++)
    {
      NSDL2_RUNTIME(NULL, NULL, "**** NVM %d ****", proc_index);
      //3. Checking whether this NVM is set for distributed quantity
      if(nvm_idx[proc_index])
      {
        //3. Calculate quantity which can be assigned to the NVM i.e. (Users/session ramped_up/not_ramped_up - Quantity already set to remove)
      
        assigned_qty = for_all + nvm_quantity[proc_index];
      
        NSDL2_RUNTIME(NULL, NULL, "assigned_qty=%d,  available_qty_to_remove[%d]=%d, nvm_quantity[%d]=%d",  
                                      assigned_qty, proc_index, available_qty_to_remove[proc_index], proc_index, nvm_quantity[proc_index]);

        //If users to delete < users remaning in the NVM
        if (assigned_qty >= available_qty_to_remove[proc_index])
        {
          total_leftover_qty_to_remove -= (available_qty_to_remove[proc_index] - nvm_quantity[proc_index]);
          nvm_quantity[proc_index] = available_qty_to_remove[proc_index];
          nvm_idx[proc_index] = 0;
          nvm_count--;
          if (assigned_qty > available_qty_to_remove[proc_index])
            redistribution_flag = 1;
          else
          NSDL2_RUNTIME(NULL, NULL, "NVM CAPACITY FULL"); 
          NSDL2_RUNTIME(NULL, NULL, "quantity left for distribution=%d", total_leftover_qty_to_remove);
          //proc_index = 0;     //As requires redistribution, resetting proc_index=0 to start for all the NVMs
        }
        else
        {
          nvm_quantity[proc_index] += for_all;
          total_leftover_qty_to_remove -= for_all;
          NSDL2_RUNTIME(NULL, NULL, "total_leftover_qty_to_remove = %d, nvm_quantity[%d] = %d", 
                                     total_leftover_qty_to_remove, proc_index, nvm_quantity[proc_index]);
        }
      
      } 
     }  //end for
  } while (redistribution_flag && nvm_count);

  //Redistribute leftover quantity
  NSDL2_RUNTIME(NULL, NULL, "------> Users left to REDISTRIBUTE=%d nvm_count=%d", total_leftover_qty_to_remove, nvm_count);

  nvms_left_for_distribution = nvm_count;
  while(total_leftover_qty_to_remove && nvms_left_for_distribution)
  {
    for (proc_index = 0; proc_index < global_settings->num_process; proc_index++)
    {
     if(nvm_idx[proc_index])
      {
        if(nvm_quantity[proc_index] < available_qty_to_remove[proc_index])
        {  
          nvm_quantity[proc_index]++;
          total_leftover_qty_to_remove--;
          NSDL2_RUNTIME(NULL, NULL, "nvm_quantity[%d]=%d, total_leftover_qty_to_remove=%d",  
                             proc_index, nvm_quantity[proc_index], total_leftover_qty_to_remove);
        }
        else
        {
          nvm_idx[proc_index] = 0;
          nvm_count--;
          nvms_left_for_distribution--;
          NSDL2_RUNTIME(NULL, NULL, "Nvm's left for redistribution = %d", nvms_left_for_distribution);
        }
      }

      NSDL2_RUNTIME(NULL, NULL, "NVM %d, nvm_quantity=%d nvms_left_for_distribution=%d, total_leftover_qty_to_remove=%d", 
                      proc_index, nvm_quantity[proc_index], nvms_left_for_distribution, total_leftover_qty_to_remove);

      //Break when either users not left for redisribution or no nvms left for redistribution
      if (!(total_leftover_qty_to_remove && nvms_left_for_distribution))
        break;
    }
  }

  NSDL2_RUNTIME(NULL, NULL, "Distributed Quantity:");
  //Storing quantity as -ve for NVMs to identify increase/decrease of users/sessions 
  for (proc_index = 0; proc_index < global_settings->num_process; proc_index++)
  {
    NSDL2_RUNTIME(NULL, NULL, "NVM %d, nvm_quantity=%d", proc_index, nvm_quantity[proc_index]);
    nvm_quantity[proc_index] = -nvm_quantity[proc_index];
  }
  
  for (proc_index = 0; proc_index < global_settings->num_process; proc_index++)
  {
    if (nvm_quantity[proc_index])
      (*actual_nvm_count)++;
  }
  NSDL3_RUNTIME(NULL, NULL, "Actual nvm count =%d", *actual_nvm_count);
  NSDL2_RUNTIME(NULL, NULL, "Exiting Method");
  return total_leftover_qty_to_remove;
}

//Purpose: Based on Added quantity / Removed quantity and based on its mode, distribute quantity among available nvms
static void runtime_distribute_quantity(char nvm_idx[], int nvm_count, int grp_idx, int quantity, char *err_msg, int running_phase_idx, int *actual_nvm_count)
{
  int quantity_left_to_remove = 0;
  int nvm_quantity[MAX_NVM_NUM] = {0};
  int group_not_started = 0;
  Schedule *schedule;

  if(global_settings->schedule_by == SCHEDULE_BY_SCENARIO)
    schedule = scenario_schedule;
  else
    schedule = &group_schedule[grp_idx];
  if((schedule->phase_array[0].phase_type == SCHEDULE_PHASE_START) && (schedule->phase_array[0].phase_status != PHASE_IS_COMPLETED))
    group_not_started = 1;

  NSDL3_RUNTIME(NULL, NULL, "Method Called. grp_idx=%d, quantity=%d, nvm_count=%d", grp_idx, quantity, nvm_count);

  if(quantity > 0)
  {
    runtime_distribute_added_quantity(nvm_idx, nvm_count, grp_idx, quantity, nvm_quantity, actual_nvm_count);
    set_distributed_quantity(grp_idx, nvm_quantity, 0, running_phase_idx);
    NSDL3_RUNTIME(NULL, NULL, "grp_idx=%d, quantity=%d, nvm_count=%d", grp_idx, quantity, nvm_count);
  }
  else if(quantity < 0)
  {
    if(global_settings->runtime_decrease_quantity_mode == RUNTIME_DECREASE_QUANTITY_MODE_0)
    {
      quantity_left_to_remove = runtime_distribute_removed_quantity(nvm_idx, nvm_count, grp_idx, quantity, 
                                                             nvm_quantity, REMOVE_RAMPED_UP_VUSERS, actual_nvm_count);
      set_distributed_quantity(grp_idx, nvm_quantity, REMOVE_RAMPED_UP_VUSERS, running_phase_idx);
      NSDL2_RUNTIME(NULL, NULL, "Quantity which cannot be removed = %d", quantity_left_to_remove);
    }

    if(global_settings->runtime_decrease_quantity_mode == RUNTIME_DECREASE_QUANTITY_MODE_1)
    {
      if(!group_not_started)
      {
        quantity_left_to_remove = runtime_distribute_removed_quantity(nvm_idx, nvm_count, grp_idx, quantity, 
                                                               nvm_quantity, REMOVE_RAMPED_UP_VUSERS, actual_nvm_count);
        NSDL2_RUNTIME(NULL, NULL, "Quantity left to remove = %d", quantity_left_to_remove);
        set_distributed_quantity(grp_idx, nvm_quantity, REMOVE_RAMPED_UP_VUSERS, running_phase_idx);
        memset(nvm_quantity, 0, sizeof(nvm_quantity));
      }
      else
       quantity_left_to_remove = quantity;

      quantity_left_to_remove = runtime_distribute_removed_quantity(nvm_idx, nvm_count, grp_idx, quantity_left_to_remove, 
                                                             nvm_quantity, REMOVE_NOT_RAMPED_UP_VUSERS, actual_nvm_count);
      set_distributed_quantity(grp_idx, nvm_quantity, REMOVE_NOT_RAMPED_UP_VUSERS, running_phase_idx);
      NSDL2_RUNTIME(NULL, NULL, "Quantity which cannot be removed = %d", quantity_left_to_remove);
    }
    if(global_settings->runtime_decrease_quantity_mode == RUNTIME_DECREASE_QUANTITY_MODE_2)
    {
      quantity_left_to_remove = runtime_distribute_removed_quantity(nvm_idx, nvm_count, grp_idx, quantity, 
                                                             nvm_quantity, REMOVE_NOT_RAMPED_UP_VUSERS, actual_nvm_count);
      NSDL2_RUNTIME(NULL, NULL, "Quantity left to remove = %d", quantity_left_to_remove);
      set_distributed_quantity(grp_idx, nvm_quantity, REMOVE_NOT_RAMPED_UP_VUSERS, running_phase_idx);

      if(!group_not_started)
      {
        memset(nvm_quantity, 0, sizeof(nvm_quantity));
        quantity_left_to_remove = runtime_distribute_removed_quantity(nvm_idx, nvm_count, grp_idx, quantity_left_to_remove, 
                                                               nvm_quantity, REMOVE_RAMPED_UP_VUSERS, actual_nvm_count);
        set_distributed_quantity(grp_idx, nvm_quantity, REMOVE_RAMPED_UP_VUSERS, running_phase_idx);
        NSDL2_RUNTIME(NULL, NULL, "Quantity which cannot be removed = %d", quantity_left_to_remove);
      }
    }
  }

  fill_output_msg(grp_idx, quantity, -quantity_left_to_remove, err_msg, 0);
}


//------------------DISTRIBUTE QUANTITY SECTION ENDS------------------------------

//----------------  KEYWORD VALIDATION SECTION BEGINS ------------------------------------
//
//Validations: 1. Number of users/sessions to be Numeric only
//             2. Number of users/sessions cannot be less than equal to 0
static int validate_users_or_sessions(int grp_idx, char *quantity, int *num_users_or_sess, int runtime_operation, char *err_msg, int session_mode) 
{

  NSDL2_RUNTIME(NULL, NULL, "Method Called. grp_idx = %d, quantity = %s, runtime_operation = %d, existing quantity = %d" ,
                                         grp_idx, quantity, runtime_operation, runprof_table_shr_mem[grp_idx].quantity); 
  double users_sess_val;
  
  int grp_type = get_group_mode(grp_idx);

  //Session rtc is not allowed in FSR mode.
  if((session_mode) && (grp_type != TC_FIX_CONCURRENT_USERS))
  {
    sprintf(err_msg, "Warning: sessions increase/decrease is only allowed in FCU mode");
    return RUNTIME_ERROR;
  }
#if 0
  if(grp_type == TC_FIX_CONCURRENT_USERS)
  {
    if(ns_is_numeric(quantity) == 0) 
    {
      sprintf(err_msg, "Number of users can be integer type and positive number only");
      return RUNTIME_ERROR;
    }
  }
#endif

  //QQQQ: why atof ???? 
  users_sess_val = atof(quantity);
  if(grp_type == TC_FIX_CONCURRENT_USERS)
    users_sess_val = (int)users_sess_val;

  NSDL2_RUNTIME(NULL, NULL, "users_sess_val=%.3f", users_sess_val); 

  if(users_sess_val == 0)
  {
    sprintf(err_msg, "Ignoring RTC as number of users/sessions to increase/decrease is equal to 0.");
    *num_users_or_sess = 0;
    return RUNTIME_SUCCESS;
  }

  if(users_sess_val < 0)
  {
    sprintf(err_msg, "Number of users/sessions to increase/decrease cannot be <= 0. Users/Sessions given = %.3f", users_sess_val);
    return RUNTIME_ERROR;
  }

  if(runtime_operation == DECREASE_USERS_OR_SESSIONS)
  {
    NSDL2_RUNTIME(NULL, NULL, "Existing users/sess in grp_idx = %d is %d", 
                                         grp_idx, runprof_table_shr_mem[grp_idx].quantity);
    //Cannot decrease more users/sessions than existing in the group
    if(!session_mode)
      users_sess_val = (((runprof_table_shr_mem[grp_idx].quantity - users_sess_val) > 0)? users_sess_val: runprof_table_shr_mem[grp_idx].quantity);

    NSDL2_RUNTIME(NULL, NULL, "users_sess_val=%.3f", users_sess_val); 
    users_sess_val = -(users_sess_val);  //Storing Decrese Users/Sessions as -ve
  }

  NSDL2_RUNTIME(NULL, NULL, "users_sess_val=%.3f", users_sess_val); 

  if (grp_type == TC_FIX_USER_RATE) //FSR
    *num_users_or_sess = (int)((users_sess_val * SESSION_RATE_MULTIPLE));
  else
    *num_users_or_sess = (int)users_sess_val;

  NSDL2_RUNTIME(NULL, NULL, "Exiting Method. num_users_or_sess=%d", *num_users_or_sess); 
  return RUNTIME_SUCCESS;
}


/* Validating runtime operation in QUANTITY, permitted are only INCREASE and DECREASE
   Returns INCREASE_USERS_OR_SESSIONS/DECREASE_USERS_OR_SESSIONS in case of success
   and RUNTIME_ERROR otherwise
*/
static int validate_runtime_operations(char *kw_inc_dec, char *err_msg) 
{
  NSDL2_RUNTIME(NULL, NULL, "Method Called. kw_inc_dec = %s", kw_inc_dec); 

  if ((strcasecmp(kw_inc_dec, "INCREASE") != 0) && (strcasecmp(kw_inc_dec, "DECREASE") != 0))
  {
    sprintf(err_msg, "Can only INCREASE/DECREASE Users/Sessions in QUANTITY keyword at runtime"); 
    return RUNTIME_ERROR;
  }
  else if (!strcasecmp(kw_inc_dec, "INCREASE"))
    return INCREASE_USERS_OR_SESSIONS;
  else 
     return DECREASE_USERS_OR_SESSIONS;

}

static int set_running_rtc_and_phase_index(int id)
{
  int k = 0;

  NSDL2_RUNTIME(NULL, NULL, "Method called, running schedule id= %d, num_qty_rtc = %d", id, global_settings->num_qty_rtc);

  for(k = 0; k < (global_settings->num_qty_rtc * total_runprof_entries); k++)
  {
    NSDL2_RUNTIME(NULL, NULL, "Running schedule index= %d, k = %d", runtime_schedule[k].rtc_state, k);
    if (runtime_schedule[k].rtc_state == RTC_FREE)
    {
      runtime_schedule[k].phase_idx = 0;
      runtime_schedule[k].rtc_idx = k;
      runtime_schedule[k].rtc_state =  RTC_NEED_TO_PROCESS;
      runtime_schedule[k].rtc_id = id;
      NSDL4_RUNTIME(NULL, NULL, "Running phase index=%d, rtc id=%d, index=%d", runtime_schedule[k].phase_idx, runtime_schedule[k].rtc_id, runtime_schedule[k].rtc_idx);
      return(k);
    }
  }
  return(-1);
}

int find_available_rtc_id()
{
  int id;
  NSDL1_RUNTIME(NULL, NULL, "Method Called");

  for (id = 0; id < (global_settings->num_qty_rtc * total_runprof_entries); id++) 
  {
    if(runtime_schedule[id].rtc_id == -1) 
    {
      runtime_schedule[id].rtc_id++;
      NSDL1_RUNTIME(NULL, NULL, "Runtime Block Index=%d", runtime_schedule[id].rtc_id);
      return(id);
    }
  }
  return(-1);
}

/*Purpose: 
          a) Parse phases to add and delete users/sessions define in QUANTITY keyword
          b) Find avaiable RTC and phase index for this running phase 
*/
static int parse_phase_validation(int grp_idx, char *buf, int runtime_operation, char *err_msg, int local_runtime_id, char *quantity, int *runtime_index)
{
  char keyword[MAX_DATA_LINE_LENGTH], *tmp;
  char phase_name[MAX_DATA_LINE_LENGTH];
  int ret = 0, flag = 0;
  int group_mode = get_group_mode(grp_idx);

  NSDL1_RUNTIME(NULL, NULL, "Method Called, grp_idx = %d, phase_settings = %s, runtime_operation = %d, "
                            "local_runtime_id = %d", 
                             grp_idx, buf, runtime_operation, local_runtime_id);

  if((tmp = strstr(buf, "SETTINGS")) != NULL)
    tmp += 9;
 
  NSDL2_RUNTIME(NULL, NULL, "Available runtime change block index = %d", local_runtime_id);

  //Set phase index
  *runtime_index = set_running_rtc_and_phase_index(local_runtime_id);
  NSDL2_RUNTIME(NULL, NULL, "runtime_index = %d", *runtime_index);
  if (*runtime_index == -1) 
  {
    strcpy(err_msg, "Maximum limit to apply runtime changes has been exhausted." 
                    "Please wait till previous changes are completed.");
    return(-1); 
  }
  
  sprintf(phase_name, "RTC_PHASE_%d", local_runtime_id);

  //Find whether user needs to add or delete users/sessions
  if (runtime_operation == INCREASE_USERS_OR_SESSIONS) { 
    //In order to reuse schedule phase parse code we are framing schedule keyword
    if (global_settings->schedule_type == SCHEDULE_TYPE_SIMPLE) 
      sprintf(keyword, "SCHEDULE %s %s RAMP_UP ALL %s", (global_settings->schedule_by == SCHEDULE_BY_SCENARIO)?"ALL":runprof_table_shr_mem[grp_idx].scen_group_name, phase_name, tmp);
    else {
      if (group_mode == TC_FIX_CONCURRENT_USERS) 
        sprintf(keyword, "SCHEDULE %s %s RAMP_UP %d %s", (global_settings->schedule_by == SCHEDULE_BY_SCENARIO)?"ALL":runprof_table_shr_mem[grp_idx].scen_group_name, phase_name, atoi(quantity), tmp);
      else
        sprintf(keyword, "SCHEDULE %s %s RAMP_UP %0.3f %s", (global_settings->schedule_by == SCHEDULE_BY_SCENARIO)?"ALL":runprof_table_shr_mem[grp_idx].scen_group_name, phase_name, atof(quantity), tmp);
    }  
    ret = parse_runtime_schedule_phase_ramp_up(grp_idx, keyword, phase_name, err_msg, &flag, *runtime_index, quantity, local_runtime_id);
  } else { //Decrease users or sessions
    if (global_settings->schedule_type == SCHEDULE_TYPE_SIMPLE)
      sprintf(keyword, "SCHEDULE %s %s RAMP_DOWN ALL %s", (global_settings->schedule_by == SCHEDULE_BY_SCENARIO)?"ALL":runprof_table_shr_mem[grp_idx].scen_group_name, phase_name, tmp);
    else {
      if (group_mode == TC_FIX_CONCURRENT_USERS) 
        sprintf(keyword, "SCHEDULE %s %s RAMP_DOWN %d %s", (global_settings->schedule_by == SCHEDULE_BY_SCENARIO)?"ALL":runprof_table_shr_mem[grp_idx].scen_group_name, phase_name, atoi(quantity), tmp);
       else
        sprintf(keyword, "SCHEDULE %s %s RAMP_DOWN %0.3f %s", (global_settings->schedule_by == SCHEDULE_BY_SCENARIO)?"ALL":runprof_table_shr_mem[grp_idx].scen_group_name, phase_name, atof(quantity), tmp);
    }
    ret = parse_runtime_schedule_phase_ramp_down(grp_idx, keyword, phase_name, err_msg, &flag, *runtime_index, quantity, local_runtime_id); 
  }
  return ret;
}

//----------------  KEYWORD VALIDATION SECTION ENDS ------------------------------------


//----------------  KEYWORD PARSING SECTION BEGINS ------------------------------------

/*Purpose: Parse QUANTITY keyword used for runtime changes in users/sessions
           Calls functions for validating runtime_operation & quantity
           Group Cannot be ALL
           Output Parameters: grp_idx, num_users_or_sess & err_msg if any
           Return Value: RUNTIME_ERROR/RUNTIME_SUCCESS
           e.g. QUANTITY G1 INCREASE 10 SETTINGS IMMEDIATLY
                QUANTITY G3 DECREASE 5 SETTINGS IMMEDIATLY
*/
int parse_keyword_runtime_quantity(char *buf, char *err_msg, int *grp_idx, int *num_users_or_sess, int *runtime_operation, int local_runtime_id, int *runtime_index, int *session_mode) 
{
  char dummy[MAX_DATA_LINE_LENGTH];
  char sg_name[MAX_DATA_LINE_LENGTH];
  char kw_inc_dec[MAX_DATA_LINE_LENGTH];
  char kw_phase_settings[MAX_DATA_LINE_LENGTH];
  int num; 
  char quantity[MAX_DATA_LINE_LENGTH];
  char mode[MAX_DATA_LINE_LENGTH];
  char *smode;
  int sg_fields = 6;
  Schedule *schedule;

  NSDL2_RUNTIME(NULL, NULL, "Method Called, buf = %s, local_runtime_id, = %d", buf, local_runtime_id);

  if ((num = sscanf(buf, "%s %s %s %s %s %s", dummy, sg_name, kw_inc_dec, quantity, kw_phase_settings, mode)) < (sg_fields))
  {
    sprintf(err_msg, "Wrong QUANTITY Keyword passed.\n"
                     "Syntax:\n"
                     "\t\t QUANTITY <group_name> <Increase/Decrease> <number of users/sessions> SETTINGS <mode>\n"
                     "\t\t FCU Modes\n"
                     "\t\t\t IMMEDIATELY\n"
                     "\t\t\t SESSIONS <0/1>\n"
                     "\t\t\t STEP <step_users> <step_time in HH:MM:SS>\n"
                     "\t\t\t RATE <ramp_up_rate> <rate_unit (H/M/S)> <ramp_up_pattern>\n"
                     "\t\t\t\t <ramp_up_pattern>:\n"
                     "\t\t\t\t\t LINEARLY\n"
                     "\t\t\t\t\t RANDOMLY\n"
                     "\t\t\t TIME <ramp_up_time> <ramp_up_pattern>\n"
                     "\t\t\t\t <ramp_up_pattern>: \n"
                     "\t\t\t\t\t LINEARLY\n"
                     "\t\t\t\t\t RANDOMLY\n"
                     "\t\t FSR Modes\n"
                     "\t\t\t IMMEDIATELY\n"
                     "\t\t\t TIME_SESSIONS <ramp_up_time in HH:MM:SS> <mode> <step_time or num_steps> \n"
                     "\t\t\t\t <mode> 0|1|2 (default steps, steps of x seconds, total steps)\n");

    return RUNTIME_ERROR;
  }
  
  val_sgrp_name(buf, sg_name, 0);//validate group name
 
  if((smode = strstr(buf, "SETTINGS")) != NULL)
  {
    if((smode = strstr(smode + 9, " SESSIONS ")) != NULL)
    {
      int per_user_session = 0;
      *session_mode = SESSION_RTC;
      smode += 10;
      if(nslb_atoi(smode, &per_user_session)<0)
      {
        sprintf(err_msg, "Warning: Unknown mode in setting for QUANTITY keyword.");
        return RUNTIME_ERROR;
      }
      if(per_user_session)
        *session_mode = USER_SESSION_RTC;
    }
  }

  if ((*grp_idx = find_sg_idx_shr(sg_name)) == RUNTIME_ERROR)
  {
    if(!*session_mode) //user RTC case
    {
      sprintf(err_msg, "Warning: Unknown group %s for QUANTITY keyword.", sg_name);
      return RUNTIME_ERROR;
    }
    else //Session RTC case 
    { 
      if(strcasecmp(sg_name, "ALL"))
      {
        sprintf(err_msg, "Warning: Unknown group %s for QUANTITY keyword in Session mode.", sg_name);
        return RUNTIME_ERROR;
      }
      *grp_idx = -1; // For All Group
    }
  }

  /*validation of session rtc*/
  if(*session_mode)
  {
    if(global_settings->schedule_type == SCHEDULE_TYPE_SIMPLE)
    {
      if(global_settings->schedule_by == SCHEDULE_BY_SCENARIO)
        schedule = scenario_schedule;
      else
        schedule = &group_schedule[*grp_idx];
      NSDL2_RUNTIME(NULL, NULL, "duration mode = %d", schedule->phase_array[3].phase_cmd.duration_phase.duration_mode);
      if(schedule->phase_array[3].phase_cmd.duration_phase.duration_mode != DURATION_MODE_SESSION)
      {
        sprintf(err_msg, "Group %s duration mode is not SESSIONS", sg_name);
        return RUNTIME_ERROR;
      }
    }
    else
    {
      sprintf(err_msg, "Duration mode SESSIONS not supported in ADVANCED type of scenario");
      return RUNTIME_ERROR;
    }
  }

  if((*runtime_operation = validate_runtime_operations(kw_inc_dec, err_msg)) == RUNTIME_ERROR) 
    return RUNTIME_ERROR;

  if((validate_users_or_sessions(*grp_idx, quantity, num_users_or_sess, *runtime_operation, err_msg, *session_mode)) == RUNTIME_ERROR)
    return RUNTIME_ERROR;

  //In this case we need to increase/decrease sessions
  NSDL2_RUNTIME(NULL, NULL, "mode = %s", mode); 
  if(*session_mode == SESSION_RTC)
  {
    NSDL2_RUNTIME(NULL, NULL, "Session RTC detected returning."); 
    //As we don't have phase settings in session RTC so no need to go in phase_validation.
    return RUNTIME_SUCCESS;
  }
  if ((num > 4) && *num_users_or_sess)
  {
    if((parse_phase_validation(*grp_idx, buf, *runtime_operation, err_msg, local_runtime_id, quantity, runtime_index)) == RUNTIME_ERROR)
      return RUNTIME_ERROR;
  }

  NSDL2_RUNTIME(NULL, NULL, "After Parsing Quantity GroupName = %s, grp_idx = %d, User operation = %d, quantity = %d", 
                             sg_name, *grp_idx, *runtime_operation, *num_users_or_sess);

  return RUNTIME_SUCCESS;
}


/*Purpose: Engine for runtime quantity change
           Calls functions for:
           1. Parse runtime keyword QUANTITY for ADD/DELETE users/sessions
           2. Get number of processes serving this group
           3. Check for processes serving this group are still active or not
           4. Divide quantity in processes left from 3
           5. Divide quantity in Ramp-up phase
           6. Divide Added Users in Ramp-down phase
*/

int g_rtc_start_idx = 0;
int kw_set_runtime_users_or_sessions(char *buf, char *err_msg, int local_runtime_id, int local_first_time)
{
  int ret;
  int grp_idx, quantity, num_fetches, runtime_operation;
  char nvm_ids[MAX_NVM_NUM] = {0};  
  int nvm_count = 0;
  int proc_index;
  int runtime_index = 0;
  int actual_nvm_count = 0;
  int session_mode = 0;
  Schedule *schedule;
  
  NSDL2_RUNTIME(NULL, NULL, "Method Called buf=%s, local_runtime_id = %d, local_first_time = %d, index = %d", 
                             buf, local_runtime_id, local_first_time, index);

  if(sigterm_received)
  {
    strcpy(err_msg, "Cannot change users/sessions after test-run is stopped by the user");
    return RUNTIME_ERROR;
  }

  ret = parse_keyword_runtime_quantity(buf, err_msg, &grp_idx, &quantity, &runtime_operation, local_runtime_id, &runtime_index, &session_mode);
  if(ret == RUNTIME_ERROR) return ret;

  
  NSDL2_RUNTIME(NULL, NULL, "ret = %d, grp_idx = %d global_settings->num_fetches = %d, global_settings->schedule_by = %d", 
                             ret, grp_idx, global_settings->num_fetches, global_settings->schedule_by);
  
  if (!quantity)
    return RUNTIME_SUCCESS;
  //Session RTC case
  if(session_mode) 
  {
    if(get_group_mode(grp_idx) == TC_FIX_USER_RATE)
    {
      strcpy(err_msg, "Cannot change session in FSR mode");
      return RUNTIME_ERROR;
    }

    num_fetches = quantity;
   
    // Common method is created to handle increment/decrement of session through QUANTITY/SCHEDULE keyword   
    if(update_runtime_sessions(session_mode, num_fetches, grp_idx, err_msg) == RUNTIME_ERROR)
      return RUNTIME_ERROR; 
     
   /*
    if(global_settings->schedule_by == SCHEDULE_BY_SCENARIO) 
    {
      int i;
      for(i = 0; i < global_settings->num_process; i++)
      {
        if(is_process_still_active(grp_idx, i))
        {
           nvm_ids[i] = 1;
        } 
      } 
      if(session_mode==USER_SESSION_RTC)
      {
        if(scenario_schedule->phase_array[3].phase_cmd.duration_phase.per_user_fetches_flag)
          num_fetches *= scenario_schedule->phase_array[3].phase_cmd.duration_phase.num_fetches; 
      } 
      ret = runtime_distribute_fetches(nvm_ids, num_fetches, err_msg);
    } 
    else 
    {
      runtime_get_process_cnt_serving_grp(nvm_ids, &nvm_count, grp_idx);
      NSDL2_RUNTIME(NULL, NULL, "nvm_count = %d", nvm_count);
    
      if(nvm_count <= 0) 
      {
        NSDL2_RUNTIME(NULL, NULL, "Cannot apply runtime changes to group=%s as NVM's running this group are over OR group is over", 
                                                              runprof_table_shr_mem[grp_idx].scen_group_name);
        sprintf(err_msg, "Cannot apply runtime changes to group=%s as NVM's running this group are over OR group is over", 
                                                              runprof_table_shr_mem[grp_idx].scen_group_name);
        return RUNTIME_ERROR;
      }
      if(session_mode==USER_SESSION_RTC)
      {
        if(group_schedule[grp_idx].phase_array[3].phase_cmd.duration_phase.per_user_fetches_flag)
          num_fetches *= group_schedule[grp_idx].phase_array[3].phase_cmd.duration_phase.num_fetches; 
      } 
      ret = runtime_distribute_fetchs_over_nvms_and_grps(nvm_ids, num_fetches, nvm_count, grp_idx, err_msg);
    } */
    if(session_mode == SESSION_RTC)
    {
      fill_output_msg(grp_idx, num_fetches, 0, err_msg, 1);
      return ret;
    }
  }

  if((runtime_operation == DECREASE_USERS_OR_SESSIONS) &&
     (global_settings->runtime_decrease_quantity_mode == RUNTIME_DECREASE_QUANTITY_MODE_0))
  {
    if(global_settings->schedule_by == SCHEDULE_BY_SCENARIO)
      schedule = scenario_schedule;
    else
      schedule = &group_schedule[grp_idx];
    if((schedule->phase_array[0].phase_type == SCHEDULE_PHASE_START) && (schedule->phase_array[0].phase_status != PHASE_IS_COMPLETED))
    {
      NSDL3_SCHEDULE(NULL, NULL, "Group=%d is not yet started on parent",
                                 grp_idx);
      sprintf(err_msg, "Cannot apply runtime changes to group=%s as group is not yet started" 
                       " and no ramped up users to be removed",
                       runprof_table_shr_mem[grp_idx].scen_group_name);
      return RUNTIME_ERROR;
    }
  }

  //2. Get number of processes serving this group & are still running & active
  runtime_get_process_cnt_serving_grp(nvm_ids, &nvm_count, grp_idx);
  NSDL2_RUNTIME(NULL, NULL, "nvm_count = %d", nvm_count);
    
  if(nvm_count <= 0) 
  {
    NSDL2_RUNTIME(NULL, NULL, "Cannot apply runtime changes to group=%s as NVM's running this group"
                              " are over OR group is over", 
                              runprof_table_shr_mem[grp_idx].scen_group_name);
    sprintf(err_msg, "Cannot apply runtime changes to group=%s as NVM's running this group"
                     " are over OR group is over OR group is in last RampDown phase",
                     runprof_table_shr_mem[grp_idx].scen_group_name);
    return RUNTIME_ERROR;
  }

  if(!local_first_time) 
  { 
    g_rtc_start_idx = runtime_index;
  }

  runtime_schedule[runtime_index].start_idx = g_rtc_start_idx;  
  runtime_schedule[g_rtc_start_idx].total_rtcs_for_this_id++;  

  NSDL2_RUNTIME(NULL, NULL, "runtime_schedule[runtime_index].start_idx = %d, runtime_index = %d, "
                            "g_rtc_start_idx = %d, runtime_schedule[g_rtc_start_idx].total_rtcs_for_this_id = %d, "
                            "runtime_schedule[g_rtc_start_idx].total_done_msgs_need_to_come = %d", 
                             runtime_schedule[runtime_index].start_idx, runtime_index, g_rtc_start_idx, 
                             runtime_schedule[g_rtc_start_idx].total_rtcs_for_this_id, 
                             runtime_schedule[g_rtc_start_idx].total_done_msgs_need_to_come);
 

  //3. Divide quantity in processes left from 3 based on mode selected
  runtime_distribute_quantity(nvm_ids, nvm_count, grp_idx, quantity, err_msg, runtime_index, &actual_nvm_count);
  
  runtime_schedule[g_rtc_start_idx].total_done_msgs_need_to_come += actual_nvm_count;

  for (proc_index = 0; proc_index < global_settings->num_process; proc_index++)
  {
    if (nvm_ids[proc_index] <= 0)
      continue;
    s_child_ports *v_port_entry_ptr = &v_port_table[proc_index];
    void *shr_mem_ptr = v_port_entry_ptr->runtime_schedule;
    Schedule *cur_schedule = shr_mem_ptr + (runtime_index * find_runtime_qty_mem_size());
    cur_schedule->rtc_state = runtime_schedule[runtime_index].rtc_state;
    NSDL2_RUNTIME(NULL, NULL, "NVM ID = %d, state = %d", proc_index, cur_schedule->rtc_state);
  }

  runtime_dump_qty_distribution_among_nvm(grp_idx, quantity, runtime_index);

  return RUNTIME_SUCCESS;
}

static void kw_usage_runtime_change_quantity_settings(char *error_message, char *err_msg) 
{
  sprintf(err_msg, "Invalid use of Keyword:: %s\n", error_message);
  strcat(err_msg, "Usage:\n");
  strcat(err_msg, "   RUNTIME_CHANGE_QUANTITY_SETTINGS [<increase-mode>] [<decrease_mode>] [<stop_mode>]\n");
  strcat(err_msg, "   increase-mode(Applicable in Advanced Scenarios Only):\n");
  strcat(err_msg, "     Mode 0: Remove added quantity in subsequent Ramp-down phase (default)\n");
  strcat(err_msg, "     Mode 1: Remove added quantity in last Ramp-dwon phase\n");
  strcat(err_msg, "   decrease-mode:\n");
  strcat(err_msg, "     Mode 0: Remove only that quantity which is existing in the system\n");
  strcat(err_msg, "     Mode 1: Remove already ramped-up quantity + not-ramped up quantity as well\n");
  strcat(err_msg, "             a. First, removing quantity already ramped up\n");
  strcat(err_msg, "             b. Then, removing quantity not ramped up\n");
  strcat(err_msg, "     Mode 2: Remove already ramped-up quantity + not-ramped up quantity as well\n");
  strcat(err_msg, "             a. First, removing quantity no ramped up\n");
  strcat(err_msg, "             b. Then, removing quantity already ramped up\n");
  strcat(err_msg, "   stop-mode\n");
  strcat(err_msg, "     Mode 0/1: Removing Vusers as per G_RAMP_DOWN_METHOD OR IMMEDIATELY\n");
  NSTL1_OUT(NULL, NULL, "Error with the Keyword RUNTIME_CHANGE_QUANTITY_SETTINGS. Error: %s", error_message);
  //exit(-1);
}


/******
 @ Function Name	: kw_set_runtime_change_quantity_settings()

 @ Purpose		: This function will parse Keyword RUNTIME_CHANGE_QUANTITY_SETTINGS
 			    RUNTIME_CHANGE_QUANTITY_SETTINGS <increase-mode> [<decrease_mode>] <stop_mode>
                            
 @ Design Notes		: This keyword is responsible for increasing and decreasing VUsers 
                          and stop Vusers at Runtime.

                          Increasing VUsers:
                            If scenario is designed by advance setting i.e. Number of phases 
                            for single group is more than 5 (Start/RampUp/Stabilize/Duration/RampDown)  
                            then newly added VUsers can be RampDown -
                              * in next RampDown phase (excluding current phases) 
                              * in Last RampDown phase of that group 

                          Decreasing VUsers:
                            Provided VUsers can be removed -
                              * from already ramped up VUsers
                              * from all VUsers (Ramped Up + Non-Ramped Up) 
                                 - First from ramped up, then non-ramped up
                                 - First form non-ramped up then from ramped up
  
                          Stop Vusers:
                            Provide Vuser can be stop test in two ways
                              * Default setting of G_RAMP_DOWN_METHOD
                              * Stop Immediately 
                             
******/ 
int kw_set_runtime_change_quantity_settings(char *buf, char *err_msg)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char s_increase_mode[MAX_DATA_LINE_LENGTH] = "\0";
  char s_decrease_mode[MAX_DATA_LINE_LENGTH] = "\0";
  char s_stop_mode[MAX_DATA_LINE_LENGTH] = "";
  short i_increase_mode, i_decrease_mode;
  int num, i_stop_mode = 0;

  NSDL2_PARSING(NULL, NULL, "Method called, buf =%s", buf);
 
  num = sscanf(buf, "%s %s %s %s", keyword, s_increase_mode, s_decrease_mode, s_stop_mode);
  if((num < 3) || (num > 4)) {
    kw_usage_runtime_change_quantity_settings("Invalid number of fields", err_msg);
    return RUNTIME_ERROR;
  }
  
  if(ns_is_numeric(s_increase_mode) == 0) {
    kw_usage_runtime_change_quantity_settings("Cannot have non-numeric increase mode", err_msg);
    return RUNTIME_ERROR;
  }

  if(ns_is_numeric(s_decrease_mode) == 0) {
    kw_usage_runtime_change_quantity_settings("Cannot have non-numeric decrease mode", err_msg);
    return RUNTIME_ERROR;
  }

  if(s_stop_mode[0]) {
    if(nslb_atoi((char *)s_stop_mode, &i_stop_mode) < 0) {
      kw_usage_runtime_change_quantity_settings("Cannot have non-numeric stop mode", err_msg);
      return RUNTIME_ERROR;
    }
  }

  i_increase_mode = atoi(s_increase_mode);
  i_decrease_mode = atoi(s_decrease_mode);

  NSDL2_RUNTIME(NULL, NULL, "i_increase_mode=%d, i_decrease_mode=%d, i_stop_mode = %d", i_increase_mode, i_decrease_mode, i_stop_mode);
  
  if(i_increase_mode != 0 && i_increase_mode != 1) {
    kw_usage_runtime_change_quantity_settings("Invalid value of increase mode", err_msg);
    return RUNTIME_ERROR;
  }

  if(i_decrease_mode != 0 && i_decrease_mode != 1 && i_decrease_mode != 2) {
    kw_usage_runtime_change_quantity_settings("Invalid value of decrease mode", err_msg);
    return RUNTIME_ERROR;
  }

  if(i_stop_mode != 0 && i_stop_mode != 1) {
    kw_usage_runtime_change_quantity_settings("Invalid value of stop mode", err_msg);
    return RUNTIME_ERROR;
  }

  global_settings->runtime_increase_quantity_mode = i_increase_mode;
  global_settings->runtime_decrease_quantity_mode = i_decrease_mode;
  global_settings->runtime_stop_immediately = i_stop_mode;
  
  NSDL2_PARSING(NULL, NULL, "Exiting Method. runtime_increase_quantity_mode=%hd, runtime_decrease_quantity_mode= %hd," 
                            "runtime_stop_immediately = %hd", global_settings->runtime_increase_quantity_mode, 
                             global_settings->runtime_decrease_quantity_mode, global_settings->runtime_stop_immediately);
  return 0;
}

//----------------  KEYWORD PARSING SECTION ENDS ------------------------------------

//Malloc and memset parent's runtime schedule structre

void runtime_schedule_malloc_and_memset()
{
  int i;
  NSDL1_RUNTIME(NULL, NULL, "Method called, number of applicable runtime changes=%d, number of scenario groups=%d", 
                global_settings->num_qty_rtc, total_runprof_entries);
  MY_MALLOC_AND_MEMSET(runtime_schedule, (sizeof(Schedule) * global_settings->num_qty_rtc * total_runprof_entries), "Parent:runtime_schedule", -1); 
  for (i = 0; i < (global_settings->num_qty_rtc * total_runprof_entries); i++)
  {
    MY_MALLOC_AND_MEMSET(runtime_schedule[i].runtime_qty_change, (sizeof(int) * 2), "runtime_qty_change", -1);
    MY_MALLOC_AND_MEMSET(runtime_schedule[i].phase_array, sizeof(Phases), "runtime_phase_array", -1); 
    runtime_schedule[i].group_idx = -1;
    runtime_schedule[i].phase_idx = -1;
    runtime_schedule[i].rtc_id = -1;//Using as runtime block id
    runtime_schedule[i].rtc_state = RTC_FREE;//Runtime state
  }
}

static int usage_runtime_phase(char *err, char *buf, int runtime_flag, char *err_msg)
{
  sprintf(err_msg, "Error: Invalid value of RUNTIME_QTY_PHASE_SETTINGS keyword: = %s\nLine = %s\n", err, buf);
  strcat(err_msg, "   Usage: RUNTIME_QTY_PHASE_SETTINGS <number_of_runtime_changes> <number_of_phases_per_runtime_change>\n");
  strcat(err_msg, "   This keyword is used to define number of runtime changes done for quantity with various modes.\n");
  strcat(err_msg, "     number_of_runtime_changes - number of runtime changes done for quantity (default value is 5).\n");
  strcat(err_msg, "     number_of_phases_per_runtime_change - different number of phases per runtime change (default value is 5).\n");
  NSTL1_OUT(NULL, NULL,"%s", err_msg);
  if(runtime_flag == 0)
    exit(-1);
  else
    return -1;
}

//parsing the RUNTIME_PHASE_SETTINGS keyword
//syntex: RUNTIME_PHASE_SETTINGS umber_of_runtime_changes> <number_of_phases_per_runtime_change>
int kw_set_runtime_phases(char *buf, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char num_qty_rtc[32 + 1];
  char tmp[MAX_DATA_LINE_LENGTH]; //This used to check if some extra field is given
  int num, num_rtc_val = 0;   
  //Default values  
  num_qty_rtc[0] = '5';
  num_qty_rtc[1] = '\0';

  num = sscanf(buf, "%s %s %s", keyword, num_qty_rtc, tmp);
  NSDL1_PARSING(NULL, NULL, "Method called, buf = %s, num = %d , keyword = [%s], num_qty_rtc = [%s]", buf, num, keyword, num_qty_rtc);
    
  if(num != 2) { 
    usage_runtime_phase("Invalid number of arguments", buf, runtime_flag, err_msg);
  }

  if(ns_is_numeric(num_qty_rtc) == 0) {
    usage_runtime_phase("Number of runtime changes should be numeric", buf, runtime_flag, err_msg);
  }

  num_rtc_val = atoi(num_qty_rtc);

  if(num_rtc_val < 0) {
    usage_runtime_phase("Number of runtime changes should be positive", buf, runtime_flag, err_msg);
  }

  global_settings->num_qty_rtc = num_rtc_val;
  global_settings->num_qty_ph_rtc = total_runprof_entries;

  NSDL2_PARSING(NULL, NULL, "global_settings->num_qty_rtc = %d, global_settings->num_qty_ph_rtc = %d", global_settings->num_qty_rtc, global_settings->num_qty_ph_rtc);
  return 0;
}

void process_schedule_rtc(int fd, char *msg)
{
  int i;
  NSDL2_MESSAGES(NULL, NULL, "Method Called");
  //Resetting gen_keyword buffer.
  for(i = 0; i < sgrp_used_genrator_entries; i++)
  {
    generator_entry[i].gen_keyword[0] = '\0';
    generator_entry[i].msg_len = 0;
  }

  NSTL1(NULL, NULL, "(Parent <- Tool), opcode = APPLY_PHASE_RTC(156), pause_done = %d, rtc_timeout_mode = %d,"
                    " rtc_schedule_timeout_val = %d, loader_opcode = %d", global_settings->pause_done,
                    global_settings->rtc_timeout_mode, global_settings->rtc_schedule_timeout_val, loader_opcode);

  RUNTIME_UPDATION_OPEN_LOG
  if(!rtcdata->rtclog_fp)
    return;

  if(global_settings->pause_done)
  {
    //Bug35339: Here we are appending the above error msg in runtime_changes.log
    RUNTIME_UPDATION_FAILED_AND_CLOSE_FILES("RTC cannot be applied as test run is already in schedule paused state.")
    return;
  }
  //This flag is for run time changes.
    //If this flag is set and we get event on epoll_wait() in ns_pre_test_check.c file
    //Then its means this signal belongs to run time changes not for epoll_wait 
    //Done for bug-3283

  if((ns_parent_state == NS_PARENT_ST_INIT) || (ns_parent_state == NS_PARENT_ST_TEST_OVER))
  {
    flag_run_time_changes_called = 1;
    RUNTIME_UPDATION_VALIDATION
    return;
  }

  rtcdata->epoll_start_time = get_ms_stamp();
  //for_quantity_rtc = 0;
  if(loader_opcode == MASTER_LOADER) {
    int len;
    memcpy(&len, msg, 4);
    NSDL2_MESSAGES(NULL, NULL, "msg size = %d", len);
    memset(rtcdata->msg_buff, 0, RTC_QTY_BUFFER_SIZE);
    char *tool_msg = msg + 8; // skip -> 4 byte for msg len and 4 byte for opcode
    /* Get the name who applied the RTC */
    memcpy(g_rtc_owner, tool_msg, 64);
    tool_msg += 64;
    len -= 64;  // As name is 64 bytes, So get the original message 
    strncpy(rtcdata->msg_buff, tool_msg, (len - 4));
    NSDL2_MESSAGES(NULL, NULL, "rtcdata->msg_buff = %s", rtcdata->msg_buff); 
    handle_parent_rtc_msg(NULL);
  } else {
    handle_parent_rtc_msg(msg);
  }
}

void process_quantity_rtc(int fd, char *msg)
{
  char msg_buf[2048];
  //User_trace trace_msg;

  NSDL2_MESSAGES(NULL, NULL, "Method called");

  //TODO:
  NSTL1(NULL, NULL, "(Parent <- Tool), APPLY_QUANTITY_RTC(150), pause_done = %d, rtc_timeout_mode = %d,"
                    " rtc_schedule_timeout_val = %d, loader_opcode = %d", global_settings->pause_done,
                    global_settings->rtc_timeout_mode, global_settings->rtc_schedule_timeout_val, loader_opcode);

  RUNTIME_UPDATION_OPEN_LOG
  if(CHECK_RTC_FLAG(RUNTIME_FAIL))
    return;

  //BUG:-65257 User/Session RTC is not allow in case RBU
  if(global_settings->protocol_enabled & RBU_API_USED)
  {
    RUNTIME_UPDATION_FAILED_AND_CLOSE_FILES("RTC cannot be applied as RBU did not support runtime changes.")
    return;
  }

  /*In case of test started or during post processing RTC cannot be applied*/
  if((ns_parent_state == NS_PARENT_ST_INIT ) || (ns_parent_state == NS_PARENT_ST_TEST_OVER))
  {
    NSDL2_MESSAGES(NULL, NULL, "ns_parent_state=%d", ns_parent_state);
    //This flag is for run time changes.
    //If this flag is set and we get event on epoll_wait() in ns_pre_test_check.c file
    //Then its means this signal belongs to run time changes not for epoll_wait 
    //Done for bug-3283
    flag_run_time_changes_called = 1;
    RUNTIME_UPDATION_VALIDATION
    return;
  }

  /*Already in pause state*/
  if(global_settings->pause_done)
  {
    //Bug35339: Here we are appending the above error msg in runtime_changes.log
    RUNTIME_UPDATION_FAILED_AND_CLOSE_FILES("RTC cannot be applied as test run is already in schedule paused state.")
    return;
  }
  //We should not be doing PAUSE_SCHEDULE when applying RTC
  rtcdata->test_paused_at = get_ms_stamp();
  global_settings->pause_done = 1;
  sprintf(msg_buf, "RTC: Pausing schedule to update Quantity\n");
  print2f_always(rfp, msg_buf);
  rtcdata->epoll_start_time = get_ms_stamp();
  
  NSDL2_MESSAGES(NULL, NULL, "test_paused_at = %llu, pause_done = %d, epoll_start_time, rtclog_fp = %p",
                 rtcdata->test_paused_at, global_settings->pause_done, rtcdata->epoll_start_time, rtcdata->rtclog_fp);

  //TODO: malloc rtcdata->msg_buff 
  if(loader_opcode == MASTER_LOADER)
  {
    if((parse_rtc_quantity_keyword1(msg)) == -1)
    {
      NSTL1(NULL, NULL, "RTC: Error in parsing QUANTITY keyword, resuming schedule");
      rtc_resume_from_pause();
    }
  }
  else if(loader_opcode == STAND_ALONE)
  {
    //This is case for NS when going to apply QTY RTC:
    NSDL2_SCHEDULE(NULL, NULL, "In case of NS, going to send RTC_PAUSE msg");
    handle_parent_rtc_msg(msg);
    if(rtcdata->cur_state == RTC_PAUSE_STATE)
      SET_RTC_FLAG(RUNTIME_QUANTITY_FLAG);
  }
}

void rtc_quantity_pause_resume(int opcode, u_ns_ts_t now)
{
  int i;
  Schedule *cur_schedule; 
  //Pause_resume *pc_msg = (Pause_resume *)recv_msg;
  
  NSDL2_SCHEDULE(NULL, NULL, "Method called.now = %u, opcode = %d, g_quantity_flag = %d", now, opcode, g_quantity_flag);
  //pause-resume timers only in quantity RTC
  if(!g_quantity_flag)
    return;
  if(opcode == RTC_PAUSE)
   global_settings->pause_done = 1;
  else { 
    //This is common for schedule_by scenario and group when resume childs from quantity rtc.
    NSDL2_SCHEDULE(NULL, NULL, "v_port_entry->num_fetches = %d, sess_inst_num = %d", v_port_entry->num_fetches, sess_inst_num);
    /* Update vport num fetched in case number of session executed are more then available in session RTC*/
    if((v_port_entry->num_fetches < 0) || ((v_port_entry->num_fetches > 0) && (v_port_entry->num_fetches < sess_inst_num))) {
      v_port_entry->num_fetches = sess_inst_num + 1; //+1 bcs when method is_new_session_blocked() called then sess_inst_num will be updated
                                                     //and phase complete will skiped.
      NSDL2_SCHEDULE(NULL, NULL, "updating v_port_entry->num_fetches = %d", v_port_entry->num_fetches);
    }
    global_settings->pause_done = 0;
    g_quantity_flag = 0;
  }
   
  if(global_settings->schedule_by == SCHEDULE_BY_SCENARIO)
  {
    cur_schedule = v_port_entry->scenario_schedule; 
    pause_resume_timers(cur_schedule, opcode, now);
  }
  else if(global_settings->schedule_by == SCHEDULE_BY_GROUP)
  {      
    for(i = 0; i < total_runprof_entries; i++)
    {
      if(opcode == RTC_RESUME )
      {
        NSDL2_SCHEDULE(NULL, NULL, "per_group_fetches = %d, sess_inst_num = %d", 
                         per_proc_per_grp_fetches[(my_port_index * total_runprof_entries) + i], per_grp_sess_inst_num[i]);

        if((per_proc_per_grp_fetches[(my_port_index * total_runprof_entries) + i] < 0) || 
          ((per_proc_per_grp_fetches[(my_port_index * total_runprof_entries) + i] > 0) && (per_proc_per_grp_fetches[(my_port_index * total_runprof_entries) + i] < per_grp_sess_inst_num[i] ))) {
             per_proc_per_grp_fetches[(my_port_index * total_runprof_entries) + i] = per_grp_sess_inst_num[i] + 1;

             NSDL2_SCHEDULE(NULL, NULL, "updating per_proc_per_group_fetches = %d", 
                              per_proc_per_grp_fetches[(my_port_index * total_runprof_entries) + i]);
        }
      }
      cur_schedule = &(v_port_entry->group_schedule[i]);
      pause_resume_timers(cur_schedule, opcode, now);
    }
  }
}

void dump_rtc_log_in_runtime_all_conf_file(FILE *runtime_all_fp)
{
  NSDL2_RUNTIME(NULL, NULL, "Method Called, rtcdata->msg_buff = %s", rtcdata->msg_buff);

  char *line = rtcdata->msg_buff;
  char *start_ptr = NULL;
  char owner_name[256] = {0};

  while (*line != '\0') 
  {
    NSDL2_RUNTIME(NULL, NULL, "RTC: where line = %s", line);
    if((start_ptr = strchr(line, '\n')) != NULL)
    {
      *start_ptr = '\0';
      start_ptr++;
      fprintf(runtime_all_fp, "%s|%s|%s\n", get_relative_time(), nslb_get_owner(owner_name), line);
      line = start_ptr;
    }
  } 
}

#if 0
int make_data_for_gen(char *err_msg)
{
  //int ret = send_rtc_settings_to_generator(qty_msg_buff, nc_flag_set, 0, /*grp_idx,*/ runtime_id, 0 /*runtime_idx*/);
  int ret = send_rtc_settings_to_generator(rtcdata->msg_buff, nc_flag_set, runtime_id);
  if(ret) 
    return ret; //In case of failure we need to stop RTC and send resume message to all generators
 
  return 0;
}
#endif

void get_rtc_send_counter_for_group(char *buff, int *rtc_send_counter)
{
  char *start_ptr;
  while (*buff != '\0')
  {
    if((start_ptr = strchr(buff, '\n')) != NULL)
    {
      start_ptr++;
      if(!strncmp(buff, "RUNTIME_CHANGE_QUANTITY_SETTINGS", 32))
      {
        NSDL2_RUNTIME(NULL, NULL, "Ignoring keyword RUNTIME_CHANGE_QUANTITY_SETTINGS from count");
        buff = start_ptr;
        continue;
      } 
    }
    else
    {
      NSDL2_RUNTIME(NULL, NULL, "In generator line = '%s' newline not found..", buff);
      break;
    }
    (*rtc_send_counter)++;
    buff = start_ptr;
  }
}

int parse_qty_buff_for_generator(char *line, int runtime_id, int is_rtc_for_qty)
{
  char err_msg[4096] = {0};
  char *start_ptr = NULL;
  int rtc_rec_counter = 0;
  int rtc_send_counter = 0;
  int first_time, rtc_msg_len = 0;

  NSDL2_RUNTIME(NULL, NULL, "Method Called, where line = %s, is_rtc_for_qty = %d", line, is_rtc_for_qty);
 
  get_rtc_send_counter_for_group(line, &rtc_send_counter);
  NSDL2_RUNTIME(NULL, NULL, "***rtc_send_counter = %d", rtc_send_counter);
  while (line != NULL) 
  {
    if((start_ptr = strchr(line, '\n')) != NULL) {
      *start_ptr = '\0';
      start_ptr++;
    } else {
      NSDL2_RUNTIME(NULL, NULL, "In generator line = '%s' newline not found..", line);
      break;
    }
    first_time = -1;  //Require to update start index once while reading message
    NSDL2_RUNTIME(NULL, NULL, "line = %s, first_time = %d, runtime_id = %d", line, first_time, runtime_id);
    read_runtime_keyword(line, err_msg, NULL, &first_time, runtime_id, is_rtc_for_qty, rtc_send_counter, &rtc_rec_counter, &rtc_msg_len); 
    line = start_ptr;
  } 
  return 0;
}

int rtc_resume_from_pause()
{
  //when paused is not applied
  if(!rtcdata->test_paused_at)
   return 0;

  char time_str_to_log[512];
  char msg_buf[2048];
  u_ns_ts_t now;

  NSDL2_SCHEDULE(NULL, NULL, "RTC pause_done = %d", global_settings->pause_done);
  if(!global_settings->pause_done)
  {
    //sprintf(msg_buf, "RTC cannot be applied because Test is already in running(not in paused) state.\n");
    //print2f_always(rfp, "%s", msg_buf);
    NSTL1(NULL, NULL, "RTC cannot be applied because Test is already in running(not in paused) state");
    return -1;
  }
  now = get_ms_stamp();
  total_pause_time = (now - rtcdata->test_paused_at) + total_pause_time;
  convert_to_hh_mm_ss(now - global_settings->test_start_time, time_str_to_log);
  sprintf(msg_buf, "RTC: Resuming test run at %s ...\n", time_str_to_log);
  NSDL2_SCHEDULE(NULL, NULL, "total_pause_time = %u", total_pause_time);
  print2f_always(rfp, "%s", msg_buf);
  global_settings->pause_done = 0;
  rtcdata->test_paused_at = 0;
  return 0;
}

// Search which phase was paused when phase complete came with pause msg
void check_and_send_next_phase_msg()
{
  int i, j;
  parent_msg temp_msg;
  Schedule *schedule_ptr;
  if(global_settings->schedule_by == SCHEDULE_BY_SCENARIO) {
     schedule_ptr = scenario_schedule;
     for(j = 0; j < schedule_ptr->num_phases; j++ ) {
       if(schedule_ptr->phase_array[j].phase_status == PHASE_PAUSED) {
         NSDL2_SCHEDULE(NULL, NULL, "Phase name = %s", schedule_ptr->phase_array[j].phase_name);
         temp_msg.top.internal.opcode = PHASE_COMPLETE;
         temp_msg.top.internal.grp_idx = -1;
         temp_msg.top.internal.phase_idx = j;
         // send nxt phase
         check_before_sending_nxt_phase(&temp_msg);
         break;
       }
     }
  }
  else if(global_settings->schedule_by == SCHEDULE_BY_GROUP) {
    for(i = 0; i < total_runprof_entries; i++) {
      schedule_ptr = &group_schedule[i];
      for(j = 0; j < schedule_ptr->num_phases; j++ ) {
        if(schedule_ptr->phase_array[j].phase_status == PHASE_PAUSED) {
          NSDL2_SCHEDULE(NULL, NULL, "Group id = %d, Phase name = %s", schedule_ptr->group_idx, schedule_ptr->phase_array[j].phase_name);
          temp_msg.top.internal.opcode = PHASE_COMPLETE;
          temp_msg.top.internal.grp_idx = i;
          temp_msg.top.internal.phase_idx = j;
          // send nxt phase
          check_before_sending_nxt_phase(&temp_msg);
          break;
        }
      }
    }
  }
}

void process_rtc_quantity_resume_schedule(User_trace *msg)
{
  int i;
  Schedule *schedule_ptr;
  msg->gen_rtc_idx = ++(rtcdata->msg_seq_num);
  NSDL2_SCHEDULE(NULL, NULL, "opcode = %d, rtcdata->msg_seq_num = %d", msg->opcode, rtcdata->msg_seq_num);

  if(rtc_resume_from_pause() != 0)
    return;

  if(loader_opcode == MASTER_LOADER) {
    NSTL1(NULL, NULL, "(Master -> Generator), QUANTITY_RESUME_RTC(155) where pause state = %d", global_settings->pause_done);
    send_qty_resume_msg_to_all_clients(msg);
  }
  else {
    NSTL1(NULL, NULL, "Sending QUANTITY_RESUME_RTC(155) to all NVMs");
    for(i=0; i<global_settings->num_process ;i++) {
      msg->msg_len = sizeof(User_trace)- sizeof(int);
      write_msg(&g_msg_com_con[i], (char *)msg, sizeof(User_trace), 0, CONTROL_MODE);
    }
  }

  // Search which phase was paused when phase complete came with pause msg
  int j;
  parent_msg temp_msg;
  if(global_settings->schedule_by == SCHEDULE_BY_SCENARIO) {
     schedule_ptr = scenario_schedule;
     for(j = 0; j < schedule_ptr->num_phases; j++ ) {
       if(schedule_ptr->phase_array[j].phase_status == PHASE_PAUSED) {
         NSDL2_SCHEDULE(NULL, NULL, "Phase name = %s", schedule_ptr->phase_array[j].phase_name);
         temp_msg.top.internal.opcode = PHASE_COMPLETE;
         temp_msg.top.internal.grp_idx = -1;
         temp_msg.top.internal.phase_idx = j;
         // send nxt phase
         check_before_sending_nxt_phase(&temp_msg);
         break;
       }
     }
  }
  else if(global_settings->schedule_by == SCHEDULE_BY_GROUP) {
    for(i = 0; i < total_runprof_entries; i++) {
      schedule_ptr = &group_schedule[i];
      for(j = 0; j < schedule_ptr->num_phases; j++ ) {
        if(schedule_ptr->phase_array[j].phase_status == PHASE_PAUSED) {
          NSDL2_SCHEDULE(NULL, NULL, "Group id = %d, Phase name = %s", schedule_ptr->group_idx, schedule_ptr->phase_array[j].phase_name);
          temp_msg.top.internal.opcode = PHASE_COMPLETE;
          temp_msg.top.internal.grp_idx = i;
          temp_msg.top.internal.phase_idx = j;
          // send nxt phase
          check_before_sending_nxt_phase(&temp_msg);
          break;
        }
      }
    }
  }
}

int send_qty_resume_msg_to_all_clients(User_trace *pause_msg)
{
  int i;

  NSDL2_MESSAGES(NULL, NULL, "Method called, opcode = %d", pause_msg->opcode);

  for(i=0; i<sgrp_used_genrator_entries ;i++) {
    /* How to handle partial write as this method is the last called ?? */
    if (g_msg_com_con[i].fd == -1) {
      if (g_msg_com_con[i].ip)
        NSDL3_MESSAGES(NULL, NULL, "Connection with the client is already closed so not sending the msg %s", msg_com_con_to_str(&g_msg_com_con[i]));
    } else {
      NSTL1(NULL, NULL, "RTC: Sending msg to Client id = %d, opcode = %d, %s", i, pause_msg->opcode, msg_com_con_to_str(&g_msg_com_con[i]));
      NSDL3_MESSAGES(NULL, NULL, "Sending msg to Client id = %d, opcode = %d, %s", i, pause_msg->opcode,
                msg_com_con_to_str(&g_msg_com_con[i]));
      //Check if buff is not null. Then set send bit, copy data into send messgae and send
      if(generator_entry[i].send_buff[0] != '\0')
      {
        generator_entry[i].flags |= SCEN_DETAIL_MSG_SENT;
        sprintf(pause_msg->reply_msg, "%s", generator_entry[i].send_buff);
        NSDL3_MESSAGES(NULL, NULL, "RTC: reply_msg = %s", pause_msg->reply_msg);
      }
      pause_msg->msg_len = sizeof(User_trace) - sizeof(int);
      write_msg(&g_msg_com_con[i], (char *)pause_msg, sizeof(User_trace), 0, CONTROL_MODE);
    }
  }
  return(0);
}

int send_qty_pause_msg_to_all_clients()
{     
  int i, j;
  User_trace send_msg;
  send_msg.opcode = rtcdata->opcode = RTC_PAUSE;
  send_msg.grp_idx = -1;
  send_msg.gen_rtc_idx = ++(rtcdata->msg_seq_num);
  send_msg.rtc_qty_flag = (CHECK_RTC_FLAG(RUNTIME_QUANTITY_FLAG) == RUNTIME_QUANTITY_FLAG);
 
  NSDL2_MESSAGES(NULL, NULL, "Method called, opcode = %d", send_msg.opcode);
  NSTL1(NULL, NULL, "(Master -> Generator), Sending RTC_PAUSE(138) msg to all generators");

  for(i = 0; i < sgrp_used_genrator_entries; i++)
  {
    /* How to handle partial write as this method is the last called ?? */
    if (generator_entry[i].flags & IS_GEN_INACTIVE)
    {
      if (g_msg_com_con[i].ip)
        NSDL3_MESSAGES(NULL, NULL, "Connection with the client is already closed so not sending the msg %s",
                                   msg_com_con_to_str(&g_msg_com_con[i])); 
    }
    else
    {
      NSDL3_MESSAGES(NULL, NULL, "Sending msg to Client id = %d, opcode = %d, %s, send_buff = %s", i, send_msg.opcode,
                msg_com_con_to_str(&g_msg_com_con[i]), generator_entry[i].send_buff);
      //Check if buff is not null. Then set send bit, copy data into send messgae and send
      if(generator_entry[i].send_buff[0] != '\0') 
      {
        //TODO: malloc send_msg.reply_msg
        strcpy(send_msg.reply_msg, generator_entry[i].send_buff);
        generator_entry[i].flags |= SCEN_DETAIL_MSG_SENT;

        send_msg.msg_len = sizeof(User_trace) - sizeof(int);
        NSDL3_MESSAGES(NULL, NULL, "reply_msg = %s", send_msg.reply_msg);
        if((write_msg(&g_msg_com_con[i], (char *)&send_msg, sizeof(User_trace), 0, CONTROL_MODE)))
        {
          NSTL1(NULL, NULL, "RTC: We cannot parse QUANTITY keyword, Resuming schedule from pause");
          RUNTIME_UPDATION_FAILED_AND_CLOSE_FILES("ERROR: We cannot parse QUANTITY keyword, Resuming schedule from pause")
          if(i)
          {
            for(j = i-1; j >= 0; j--)
            {
              send_msg.opcode = RTC_RESUME;
              if((write_msg(&g_msg_com_con[j], (char *)&send_msg, sizeof(User_trace), 0, CONTROL_MODE)))
                NSTL1(NULL, NULL, "(Master -> Generator) Failed to send RTC_RESUME to paused generators");
              DEC_CHECK_RTC_RETURN_INT(g_msg_com_con[j].nvm_index, 0)
            }
            rtcdata->cur_state = RTC_RESUME_STATE;
          }
          return -1;
        }
        INC_RTC_MSG_COUNT(g_msg_com_con[i].nvm_index)
      }
    }
  }
  rtcdata->cur_state = RTC_PAUSE_STATE;

  return(0);
} 

int find_grp_idx_for_rtc(char *grp, char *err_msg)
{
  int idx;
  
  if (strcasecmp(grp, "ALL") == 0) {
    return NS_GRP_IS_ALL;
  } else {
    idx = find_sg_idx_shr(grp);
    if (idx == -1) {
      sprintf(err_msg, "Group '%s' is not a valid group", grp);
      return NS_GRP_IS_INVALID;
    }
    return idx;
  }
}

static void kw_usage_runtime_change_quantity_timeout(char *error_message, char *err_msg)
{
  sprintf(err_msg, "Error: Invalid use of RUNTIME_CHANGE_TIMEOUT keyword: = %s\n", error_message);
  strcat(err_msg, "   Usage:\n");
  strcat(err_msg, "   RUNTIME_CHANGE_TIMEOUT <Mode> <Schedule pause timeout val(seconds)> <System pause timeout val(seconds)>\n");
  strcat(err_msg, "   Mode:\n");
  strcat(err_msg, "     0: Disable the keyword\n");
  strcat(err_msg, "     1: Enable the keyword(default)\n");
  strcat(err_msg, "   Timeout-val:\n");
  strcat(err_msg, "     Schedule-pause: Default timeout value is 90 seconds- When Parent is not able to resume the child after the RTC then child will come out from schedule pause after given timeout.\n");
  NSTL1_OUT(NULL, NULL, "%s", err_msg);
  exit(-1);
}

int kw_set_runtime_qty_timeout(char *buf, char *err_msg) 
{
  char keyword[MAX_DATA_LINE_LENGTH] = {0};
  char s_mode[MAX_DATA_LINE_LENGTH] = {0};
  char s_rtc_timeout_val[MAX_DATA_LINE_LENGTH] = {0};
  int num, mode, rtc_timeout_val = 90;

  NSDL4_PARSING(NULL, NULL, "Method called, buf =%s", buf);

  num = sscanf(buf, "%s %s %s", keyword, s_mode, s_rtc_timeout_val);
  
  if(num != 3)
    kw_usage_runtime_change_quantity_timeout("Invalid number of fields", err_msg);
  
  if(ns_is_numeric(s_mode) == 0)
    kw_usage_runtime_change_quantity_timeout("Cannot have non-numeric mode", err_msg);

  if(ns_is_numeric(s_rtc_timeout_val) == 0)
    kw_usage_runtime_change_quantity_timeout("Cannot have non-numeric schedule pause timeout", err_msg);

  mode = atoi(s_mode);
  rtc_timeout_val = atoi(s_rtc_timeout_val);

  if(mode < 0 && mode > 1) //No need to have mode can remove later
    kw_usage_runtime_change_quantity_timeout("mode is either 0 or 1", err_msg);
 
  if(rtc_timeout_val < 90) //Schedule pause
    kw_usage_runtime_change_quantity_timeout("rtc_timeout_val cannot be less than 90 seconds", err_msg);
 
  global_settings->rtc_timeout_mode = mode;
  //Increasing timeout with a delay on immediate next node
  if(loader_opcode == CLIENT_LOADER)
    rtc_timeout_val += RTC_DELAY_ON_TIMEOUT;
  global_settings->rtc_schedule_timeout_val = rtc_timeout_val;
  global_settings->rtc_system_pause_timeout_val = rtc_timeout_val + RTC_DELAY_ON_TIMEOUT;
   
  NSDL4_PARSING(NULL, NULL, "Exiting Method. rtc_timeout_mode = %d, rtc_schedule_timeout_val = %d,"
                            " rtc_system_pause_timeout_val = %d", global_settings->rtc_timeout_mode,
                            global_settings->rtc_schedule_timeout_val, global_settings->rtc_system_pause_timeout_val);
  return 0;
}
