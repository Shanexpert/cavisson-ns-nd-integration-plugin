#include <stdio.h>
#include <stdlib.h>
#include <regex.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"

#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "user_tables.h"
#include "ns_error_codes.h"
#include "ns_alloc.h"
#include "ns_log.h"
#include "ns_schedule_phases_parse.h"
#include "divide_users.h" 
#include "ns_server.h"
#include "util.h"
#include "timing.h"
#include "tmr.h"

#include "logging.h"
#include "ns_ssl.h" 
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "divide_values.h" 
#include "child_init.h"
#include "ns_schedule_start_group.h"
#include "ns_schedule_stabilize.h"
#include "ns_schedule_duration.h"
#include "ns_schedule_ramp_up_fcu.h"
#include "ns_msg_com_util.h"
#include "ns_schedule_ramp_down_fcu.h"
#include "ns_schedule_stabilize.h"
#include "ns_schedule_duration.h"
#include "ns_date_vars.h"
#include "tr069/src/ns_tr069_data_file.h"
#include "ns_page_dump.h"
#include "wait_forever.h"//Added for loader_opcode
#include "../../base/libnscore/nslb_bitflag.h"
#ifndef CAV_MAIN
Schedule *scenario_schedule = NULL;
Schedule *group_schedule = NULL;
#else
__thread Schedule *scenario_schedule = NULL;
__thread Schedule *group_schedule = NULL;
#endif

Schedule *runtime_schedule = NULL;
int num_grp_complete = 0;

//extern void end_test_run( void );
/*
static int alloc_bitmask(int **bitmask)
{
  int size;
  int rem;
  size = (global_settings->num_process) / (sizeof(int) * 8);
  rem = (global_settings->num_process) % (sizeof(int) * 8);
  if (rem) 
    size++;
  NSDL3_SCHEDULE(NULL, NULL, "Allocating bitmask for size %d bits, global_settings->num_process = %d, sizeof int = %d, size = %d\n", (size * sizeof(int) * 8), global_settings->num_process, sizeof(int), size);
  MY_MALLOC(*bitmask, sizeof(int) * size, "schedule->bitmask", -1);
  //schedule->bitmask_size = size;
  //memset(schedule->bitmask, 0xffffffff, sizeof(int) * size);
  memset(*bitmask, 0, size);
  return size;
}
*/
void init_schedule_bitmask() 
{
  Schedule *schedule;
  int i, num_process;

  num_process = (loader_opcode == MASTER_LOADER)?sgrp_used_genrator_entries:global_settings->num_process;
  if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO) {
    schedule = scenario_schedule;
    //schedule->bitmask_size = alloc_bitmask(&(schedule->bitmask));
    //alloc_bitmask(&(schedule->ramp_bitmask));
    //alloc_bitmask(&(schedule->ramp_done_bitmask));
    schedule->bitmask_size = num_process;
    schedule->bitmask = nslb_alloc_bitflag();
    schedule->ramp_bitmask = nslb_alloc_bitflag();
    schedule->ramp_done_bitmask = nslb_alloc_bitflag();
    schedule->ramp_msg_to_expect = global_settings->num_process;
    schedule->ramp_done_per_cycle_count = 0;
  } else if (global_settings->schedule_by == SCHEDULE_BY_GROUP) {
    for (i = 0; i < total_runprof_entries; i++) {
      schedule = &(group_schedule[i]);
      //schedule->bitmask_size = alloc_bitmask(&(schedule->bitmask));
      //alloc_bitmask(&(schedule->ramp_bitmask));
      //alloc_bitmask(&(schedule->ramp_done_bitmask));
      schedule->bitmask_size = num_process;
      schedule->bitmask = nslb_alloc_bitflag();
      schedule->ramp_bitmask = nslb_alloc_bitflag();
      schedule->ramp_done_bitmask = nslb_alloc_bitflag();
      //NC: In case of controller ramp msg to expect is equal to number of generators per scenario group 
      if (loader_opcode == MASTER_LOADER)
        schedule->ramp_msg_to_expect = runProfTable[i].num_generator_per_grp;
      else
        schedule->ramp_msg_to_expect = global_settings->num_process;
      schedule->ramp_done_per_cycle_count = 0;
    }
  }
}

int get_dependent_group(int grp_idx, int *dependent_grp_array) 
{
  int num = 0;
  Schedule *schedule;
  int i;

  if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO)
    return -1;
  
  /* schedule by grp */
  for (i = 0; i < total_runprof_entries; i++) {
    schedule = &(group_schedule[i]);
    if (schedule->phase_array[0].phase_cmd.start_phase.dependent_grp == grp_idx) {
      dependent_grp_array[num] = i;
      NSDL3_SCHEDULE(NULL, NULL, "Found dependent grp %d\n", i);
      num++;
    }
  }
  if (!num)
    return -1;
  else
    return num;
}

void set_phase_num_for_schedule(Schedule *cur_schedule)
{
  Phases *phase_ptr;
  int phase_idx;              /* Its a schedule phase id */
  static __thread int phase_num = 0; // Phase sequence number

  for(phase_idx = 0; phase_idx < cur_schedule->num_phases; phase_idx++)
  {
    NSDL3_MESSAGES(NULL, NULL, "Method Called, phase_idx = %d", phase_idx);
    phase_ptr = &(cur_schedule->phase_array[phase_idx]);
    phase_ptr->phase_num = phase_num;
    phase_num++;
  }
}

//This Function will assign a unique phase number
void set_phase_num()
{
  int grp_idx;
  if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO)
  {
    //NSDL3_MESSAGES(NULL, NULL, "Method Called, grp_idx = %d", grp_idx);
    set_phase_num_for_schedule(scenario_schedule);
  }
  else
  {
    for(grp_idx = 0; grp_idx < total_runprof_entries; grp_idx++)
    {
      NSDL3_MESSAGES(NULL, NULL, "Method Called, grp_idx = %d", grp_idx);
      set_phase_num_for_schedule(&group_schedule[grp_idx]);
    }
  }
}

void setup_schedule_for_nvm(int original_num_process) 
{
  if(run_mode == NORMAL_RUN)
     write_log_file(NS_START_INST, "Validating scenario phases");
  validate_phases();
  #ifndef CAV_MAIN
  set_scenario_type();
  #endif
  set_phase_num();
  
  if(run_mode == NORMAL_RUN)
     write_log_file(NS_START_INST, "Dividing vusers to Cavisson Virtual Machines (CVMs)");
  divide_users_or_pct_per_proc(original_num_process);
  /*Divide day among NVMs in secs*/
  fill_uniq_date_var_data();
  /* TRACING: Divide sessions among nvm*/
  divide_session_per_proc();
  if(global_settings->tr069_data_dir[0] != '\0')
    tr069_read_count_file();
  
}

/*
 * Phase_element
 * 0 - PhaseNum
 * 1 - PhaseType
 * 2 - PhaseName
*/
void get_current_phase_info(int grp_idx, int phase_element, char *phase_element_val)
{
  Phases *phase_ptr;
  //int phase_id;
  //ar phase_element_val[512];
  Schedule *cur_schedule;

  if(global_settings->schedule_by == SCHEDULE_BY_SCENARIO) 
       cur_schedule = v_port_entry->scenario_schedule;	
  else 
      cur_schedule = &(v_port_entry->group_schedule[grp_idx]);
  
  phase_ptr = &(cur_schedule->phase_array[cur_schedule->phase_idx]);

  switch (phase_element)
  {
     case 0: 
       //strcpy(phase_element_val, phase_ptr->phase_num);	
       sprintf(phase_element_val, "%hd", phase_ptr->phase_num);	
       break;	
     case 1: 
       //strcpy(phase_element_val, phase_ptr-> phase_type);
       sprintf(phase_element_val,"%d", phase_ptr-> phase_type);
       break;
     case 2: 
       strcpy(phase_element_val, phase_ptr-> phase_name);
       break;
     default:
       /*Log Error Msg	*/
       sprintf(phase_element_val,"%d", -1);
  }
  //return phase_element_val;
}


int get_RU_RD_phases_count(Schedule *schedule)
{
  int phase_id;
  Phases *ph;
  int RU_RD_phase_count = 0;

  NSDL2_SCHEDULE(NULL, NULL, "Method Called, num_phases = %d", schedule->num_phases);
  for(phase_id = 0; phase_id < schedule->num_phases; phase_id++)
  {
    ph = &(schedule->phase_array[phase_id]);

    if((ph->phase_type == SCHEDULE_PHASE_RAMP_UP) || (ph->phase_type == SCHEDULE_PHASE_RAMP_DOWN))
      RU_RD_phase_count++;
  }
  NSDL3_SCHEDULE(NULL, NULL, "Ramp-Up + Ramp-Down Phase Count=%d", RU_RD_phase_count);
  return RU_RD_phase_count;
}
  

int is_group_over(int grp_idx, int proc_index)
{
  Schedule *cur_schedule;
  s_child_ports *v_port_entry_ptr;

  NSDL3_SCHEDULE(NULL, NULL, "Method Called. grp_idx=%d, proc_index=%d", grp_idx, proc_index);

  v_port_entry_ptr = &v_port_table[proc_index];
  if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO)
    cur_schedule = v_port_entry_ptr->scenario_schedule;
  else
    cur_schedule = &(v_port_entry_ptr->group_schedule[grp_idx]);

  int last_phase_id = cur_schedule->num_phases - 1;
  if(cur_schedule->phase_array[last_phase_id].phase_status == PHASE_IS_COMPLETED)
  {
    NSDL3_SCHEDULE(NULL, NULL, "Group=%d is over on NVM=%d", grp_idx, proc_index);
    return 1;
  }
  else
  {
    NSDL3_SCHEDULE(NULL, NULL, "Group=%d is not over on NVM=%d", grp_idx, proc_index);
    return 0;
  }
}


double get_fsr_grp_ratio(Schedule *cur_schedule, int grp_idx)
{
  double grp_ratio;
  NSDL3_RUNTIME(NULL, NULL, "Method Called. grp_idx=%d", grp_idx);
  int i;
  for(i=0; i < total_runprof_entries; i++)
    NSDL3_RUNTIME(NULL, NULL, "cumulative_runprof_table[%d]=%d", i, cur_schedule->cumulative_runprof_table[i]);

    NSDL3_RUNTIME(NULL, NULL, "cumulative_runprof_table[%d]=%d", total_runprof_entries -1, cur_schedule->cumulative_runprof_table[total_runprof_entries-1]);

  if(grp_idx == 0)
    grp_ratio = (double)(cur_schedule->cumulative_runprof_table[grp_idx] /
                               (double)cur_schedule->cumulative_runprof_table[total_runprof_entries -1]);
  else
    grp_ratio = (double)((double)(cur_schedule->cumulative_runprof_table[grp_idx] - cur_schedule->cumulative_runprof_table[grp_idx - 1]) /
                               (double)cur_schedule->cumulative_runprof_table[total_runprof_entries -1]);
  NSDL3_RUNTIME(NULL, NULL, "Exitting Method Called. grp_ratio=%.2f", grp_ratio);
  return grp_ratio;
}
