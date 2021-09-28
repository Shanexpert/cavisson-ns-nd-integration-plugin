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
#include "ns_server.h"
#include "ns_log.h"
#include "timing.h"
#include "tmr.h"
#include "util.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "ns_alloc.h"
#include "divide_users.h" 
#include "divide_values.h" 
#include "child_init.h"
#include "ns_schedule_phases.h"
#include "ns_schedule_phases_parse.h"
#include "ns_schedule_start_group.h"
#include "ns_schedule_stabilize.h"
#include "ns_schedule_duration.h"
#include "ns_schedule_ramp_up_fcu.h"
#include "ns_msg_com_util.h"
#include "ns_schedule_ramp_down_fcu.h"
#include "ns_schedule_stabilize.h"
#include "ns_schedule_duration.h"
#include "ns_schedule_divide_over_nvm.h"
#include "ns_exit.h"

//extern void end_test_run( void );

#define RTC_PHASE_SIZE       (sizeof(Phases) * global_settings->num_qty_ph_rtc * global_settings->num_process) 
#define RTC_TIMER_SIZE       (sizeof(timer_type) * global_settings->num_qty_ph_rtc * global_settings->num_process) 

/*Add size to base address of runtime schedule*/
#define START_RTC_PHASE               sizeof(Schedule)
#define START_RTC_RAMP_TIMER	      RTC_PHASE + RTC_PHASE_SIZE
#define START_RTC_END_TIMER	      RTC_RAMP_TIMER + RTC_TIMER_SIZE 
#define START_RTC_DISTRIBUTE_QTY      RTC_END_TIMER + RTC_TIMER_SIZE 

static int *per_grp_qty_per_phase = NULL;
int next_power_of_n(int value, int base)
{
  int div, mod;

  NSDL4_SCHEDULE(NULL, NULL, "Method called. value=%d, base=%d", value, base);

  div = value / base;
  mod = value % base;
  if(mod)
    value = div * base + base;

  NSDL4_SCHEDULE(NULL, NULL, "Rounding of to next base=%d, value = %d", base, value);
  return value;
}

int find_runtime_qty_mem_size()
{
  int size = 0;
  NSDL2_SCHEDULE(NULL, NULL, "Method Called");

  if(global_settings->num_qty_rtc)
  {
    //QQQQ: explain this  
    size = sizeof(Schedule) + sizeof(Phases) + (sizeof(int) * 2)/*runtime distributed quantity*/ + (sizeof(timer_type) * 2);
  }

  NSDL2_SCHEDULE(NULL, NULL, "Calculate runtime change quantity schedule size = %d, Phase size = %d, "
                             "timer size = %d, size = %d", 
                              sizeof(Schedule), sizeof(Phases), sizeof(timer_type), size);
  return(size);
}

static void update_child_runtime_schedule (void *schedule_shr_mem)
{ 
  int rtc_idx, proc_index;
  void *schedule_shr_mem_ptr;
  Schedule *runtime_schedule_ptr;
  long size = find_runtime_qty_mem_size();

  NSDL2_SCHEDULE(NULL, NULL, "Method Called, schedule_shr_mem = %p, global_settings->num_process = %d", 
                              schedule_shr_mem, global_settings->num_process);

  for(proc_index = 0; proc_index < global_settings->num_process; proc_index++)
  {
    //1. Schedule Structure
    schedule_shr_mem_ptr = schedule_shr_mem + (proc_index * (global_settings->num_qty_rtc * total_runprof_entries) * size);
    v_port_table[proc_index].runtime_schedule = schedule_shr_mem_ptr;
    NSDL2_SCHEDULE(NULL, NULL, "scenario_schedule= %p", schedule_shr_mem_ptr);
  
    //Pointing phases
    for (rtc_idx = 0; rtc_idx < (global_settings->num_qty_rtc * total_runprof_entries); rtc_idx++)
    {
      runtime_schedule_ptr = (Schedule *)schedule_shr_mem_ptr;
      runtime_schedule_ptr->rtc_state = -1;
      schedule_shr_mem_ptr += sizeof(Schedule);
      
      //2. Phase_array
      runtime_schedule_ptr->phase_array = (Phases *)schedule_shr_mem_ptr;
      schedule_shr_mem_ptr += sizeof(Phases);
 
      NSDL2_SCHEDULE(NULL, NULL, "phase_array=%p", schedule_shr_mem_ptr);       
 
      //3.Timers (Ramp Tmr, Ramp End Tmr)
 
      runtime_schedule_ptr->phase_ramp_tmr = (timer_type *)schedule_shr_mem_ptr;
      memset(runtime_schedule_ptr->phase_ramp_tmr, 0, sizeof(timer_type));
      runtime_schedule_ptr->phase_ramp_tmr->timer_type = -1;
      runtime_schedule_ptr->phase_ramp_tmr->next = NULL;
      runtime_schedule_ptr->phase_ramp_tmr->prev = NULL;
 
      schedule_shr_mem_ptr += (sizeof(timer_type));
 
      runtime_schedule_ptr->phase_end_tmr = (timer_type *)schedule_shr_mem_ptr;
      memset(runtime_schedule_ptr->phase_end_tmr, 0, sizeof(timer_type));
      runtime_schedule_ptr->phase_end_tmr->timer_type = -1;
      runtime_schedule_ptr->phase_end_tmr->next = NULL;
      runtime_schedule_ptr->phase_end_tmr->prev = NULL;
  
      schedule_shr_mem_ptr += (sizeof(timer_type));
 
      //4.Runtime Quantity Changes
      runtime_schedule_ptr->runtime_qty_change = (int *)schedule_shr_mem_ptr;
      NSDL2_SCHEDULE(NULL, NULL, "runtime_qty_change=%p", schedule_shr_mem_ptr);
      schedule_shr_mem_ptr += (sizeof(int) * 2);
    }
  }
}

void init_vport_table_phases()
{
  void *schedule_shr_mem = NULL;
  void *schedule_shr_mem_ptr = NULL;
  void *phase_shr_mem_ptr = NULL;
  void *per_grp_qty_ptr = NULL;
  long  per_process_schedule_size, per_group_schedule_size;

  int phase_id;
  int total_num_phases = 0; 
  Phases *ph;
  int proc_index, RU_RD_phase_count;
  int y, j, grp_idx;

  NSDL2_SCHEDULE(NULL, NULL, "Method Called, num_qty_rtc = %d, total_runprof_entries = %d", 
                              global_settings->num_qty_rtc, total_runprof_entries);

  //Find shared memory size require for quantity runtime changes
  long runtime_qty_mem_size = find_runtime_qty_mem_size() * global_settings->num_qty_rtc * total_runprof_entries;

  NSDL2_SCHEDULE(NULL, NULL, "runtime_qty_mem_size = %ld, schedule_by = %d", 
                              runtime_qty_mem_size, global_settings->schedule_by);

  if(global_settings->schedule_by == SCHEDULE_BY_SCENARIO) 
  {
    NSDL2_SCHEDULE(NULL, NULL, "Allocating shared memory for scenario based schedules");
    RU_RD_phase_count = get_RU_RD_phases_count(scenario_schedule);

    NSDL2_SCHEDULE(NULL, NULL, "RU + RD phase count=%d", RU_RD_phase_count);

    per_process_schedule_size = 
    (
      next_power_of_n(sizeof(Schedule), WORDSIZE) +                                               // 1. scenario_schedule
      next_power_of_n(sizeof(Phases) * scenario_schedule->num_phases, WORDSIZE) +                 // 2. phase_array
      next_power_of_n(sizeof(timer_type), WORDSIZE) +                                             // 3. phase_ramp_tmr
      next_power_of_n(sizeof(timer_type), WORDSIZE) +                                             // 4. phase_end_tmr
      next_power_of_n((sizeof(int) * total_runprof_entries * 2), WORDSIZE) + 	                  // 5. runtime_qty_change
      next_power_of_n((sizeof(int) * total_runprof_entries), WORDSIZE) +	                  // 6. cur_users
      next_power_of_n((sizeof(int) * total_runprof_entries), WORDSIZE) * RU_RD_phase_count +      // 7. per_grp_qty
      next_power_of_n((sizeof(int) * total_runprof_entries), WORDSIZE)                            // 8. my_runprof_table_cummulative
    );
    NSDL2_SCHEDULE(NULL, NULL, "per process scenario schedule size=%ld bytes", per_process_schedule_size);

    schedule_shr_mem = do_shmget((global_settings->num_process * (per_process_schedule_size + runtime_qty_mem_size)), "Schedule Scenario Size");

    for (proc_index = 0; proc_index < global_settings->num_process; proc_index++) 
    {
      // 1. scenario_schedule
      schedule_shr_mem_ptr = schedule_shr_mem + (proc_index * per_process_schedule_size);
      v_port_table[proc_index].scenario_schedule = schedule_shr_mem_ptr;
      NSDL2_SCHEDULE(NULL, NULL, "scenario_schedule=%p", schedule_shr_mem_ptr);

      /* Some fields will not be valid, re-initialized below */
      memcpy(v_port_table[proc_index].scenario_schedule, scenario_schedule, sizeof(Schedule));

      //2. Phase_array
      schedule_shr_mem_ptr += next_power_of_n(sizeof(Schedule), WORDSIZE);
      v_port_table[proc_index].scenario_schedule->phase_array = schedule_shr_mem_ptr;
      NSDL2_SCHEDULE(NULL, NULL, "phase_array=%p", schedule_shr_mem_ptr);

      /* Note while memcpy, the values going will be inaccurate, this will be recalculated */
      memcpy(v_port_table[proc_index].scenario_schedule->phase_array,
               scenario_schedule->phase_array, sizeof(Phases) * scenario_schedule->num_phases);

      //3. phase_ramp_tmr
      schedule_shr_mem_ptr += next_power_of_n(sizeof(Phases) * scenario_schedule->num_phases, WORDSIZE);
      NSDL2_SCHEDULE(NULL, NULL, "phase_ramp_tmr=%p", schedule_shr_mem_ptr);

      v_port_table[proc_index].scenario_schedule->phase_ramp_tmr = schedule_shr_mem_ptr;
      v_port_table[proc_index].scenario_schedule->phase_ramp_tmr->timer_type = -1;
      v_port_table[proc_index].scenario_schedule->phase_ramp_tmr->next = NULL;
      v_port_table[proc_index].scenario_schedule->phase_ramp_tmr->prev = NULL;

      for(y = 0; y < scenario_schedule->num_phases; y++) 
      {
        v_port_table[proc_index].scenario_schedule->phase_array[y].phase_status = PHASE_NOT_STARTED;
        v_port_table[proc_index].scenario_schedule->phase_array[y].runtime_flag = 0;
     }

      //4. phase_end_tmr
      schedule_shr_mem_ptr += next_power_of_n(sizeof(timer_type), WORDSIZE);
      NSDL2_SCHEDULE(NULL, NULL, "phase_end_tmr=%p", schedule_shr_mem_ptr);

      v_port_table[proc_index].scenario_schedule->phase_end_tmr = schedule_shr_mem_ptr;

      v_port_table[proc_index].scenario_schedule->phase_end_tmr->timer_type = -1;
      v_port_table[proc_index].scenario_schedule->phase_end_tmr->next = NULL;
      v_port_table[proc_index].scenario_schedule->phase_end_tmr->prev = NULL;

      //5. Runtime_qty_change
      // For Run Time Changes in quantity
      // Multiplication by 2 is required in case of remove users/sessions in mode-1, 2 to store 
      // ramped_up_vusers/sess, not_ramped_up_vusers/sess to be removed

      schedule_shr_mem_ptr += next_power_of_n(sizeof(timer_type), WORDSIZE);
      NSDL2_SCHEDULE(NULL, NULL, "runtime_qty_change=%p", schedule_shr_mem_ptr);
	
      v_port_table[proc_index].scenario_schedule->runtime_qty_change = schedule_shr_mem_ptr;
      memset(v_port_table[proc_index].scenario_schedule->runtime_qty_change, 0, (sizeof(int) * total_runprof_entries * 2));

      //6.cur_users
      //For group-wise existing users with that NVM 
      schedule_shr_mem_ptr += next_power_of_n((sizeof(int) * total_runprof_entries * 2), WORDSIZE);
      NSDL2_SCHEDULE(NULL, NULL, "cur_users=%p", schedule_shr_mem_ptr);

      v_port_table[proc_index].scenario_schedule->cur_users = schedule_shr_mem_ptr;
      memset(v_port_table[proc_index].scenario_schedule->cur_users, 0, (sizeof(int) * total_runprof_entries));

      //7. Per_grp_qty
      schedule_shr_mem_ptr += next_power_of_n((sizeof(int) * total_runprof_entries), WORDSIZE);
      NSDL2_SCHEDULE(NULL, NULL, "per_grp_qty=%p", schedule_shr_mem_ptr);
      per_grp_qty_ptr = schedule_shr_mem_ptr;

      for (phase_id = 0; phase_id < v_port_table[proc_index].scenario_schedule->num_phases; phase_id++) 
      {
        ph = &(v_port_table[proc_index].scenario_schedule->phase_array[phase_id]);
        if ((ph->phase_type == SCHEDULE_PHASE_RAMP_UP) || (ph->phase_type == SCHEDULE_PHASE_RAMP_DOWN))
        {      
          if(ph->phase_type == SCHEDULE_PHASE_RAMP_UP) {
            ph->phase_cmd.ramp_up_phase.per_grp_qty = (int *) per_grp_qty_ptr;
            ph->phase_cmd.ramp_up_phase.nvm_dist_index = -1;
            ph->phase_cmd.ramp_up_phase.nvm_dist_count = 0;
          }
          else {
            ph->phase_cmd.ramp_down_phase.per_grp_qty = (int *) per_grp_qty_ptr;
            ph->phase_cmd.ramp_down_phase.nvm_dist_index = -1;
            ph->phase_cmd.ramp_down_phase.nvm_dist_count = 0;
          }

          per_grp_qty_ptr += next_power_of_n(sizeof(int) * total_runprof_entries, WORDSIZE);
          NSDL2_SCHEDULE(NULL, NULL, "per_grp_qty=%p", per_grp_qty_ptr);
        }
      }
      //distribute_group_qty_over_phase(v_port_table[proc_index].scenario_schedule);

      //8. my_runprof_table_cummulative
      
      schedule_shr_mem_ptr += (next_power_of_n((sizeof(int) * total_runprof_entries), WORDSIZE) * RU_RD_phase_count);
      NSDL2_SCHEDULE(NULL, NULL, "cumulative_runprof_table=%p", schedule_shr_mem_ptr);

      v_port_table[proc_index].scenario_schedule->cumulative_runprof_table = schedule_shr_mem_ptr;
      //compute_runprof_table_cummulative();
    }
    schedule_shr_mem_ptr += (next_power_of_n((sizeof(int) * total_runprof_entries), WORDSIZE));
    update_child_runtime_schedule(schedule_shr_mem_ptr);
  } 
  else if (global_settings->schedule_by == SCHEDULE_BY_GROUP) 
  {
    NSDL2_SCHEDULE(NULL, NULL, "Allocating shared memory for group based schedules");
    
    for (j = 0; j < total_runprof_entries; j++)
    {
      total_num_phases += group_schedule[j].num_phases;
    }
    
    per_group_schedule_size =
    ( 
      next_power_of_n(sizeof(timer_type), WORDSIZE) +                      //3. phase_ramp_tmr
      next_power_of_n(sizeof(timer_type), WORDSIZE) +                      //4. phase_end_tmr
      next_power_of_n((sizeof(int) * 2), WORDSIZE)                         //5. runtime_qty_change
    );
    NSDL2_SCHEDULE(NULL, NULL, "per group schedule size=%ld bytes", per_group_schedule_size);
    
    per_process_schedule_size =
    (
      (next_power_of_n((sizeof(Schedule) * total_runprof_entries), WORDSIZE)) +                  //1. scenario_schedule
      (next_power_of_n(sizeof(Phases), WORDSIZE) * total_num_phases) +                           //2. phase_array
      (per_group_schedule_size * total_runprof_entries)
    );

    NSDL2_SCHEDULE(NULL, NULL, "per process schedule group size=%ld bytes", per_process_schedule_size);
    
    schedule_shr_mem = do_shmget((global_settings->num_process * (per_process_schedule_size + runtime_qty_mem_size)), "Schedule Group Size");

    NSDL2_SCHEDULE(NULL, NULL, "scenario_schedule=%p", schedule_shr_mem);
   
    for (proc_index = 0; proc_index < global_settings->num_process; proc_index++)
    {
      //1. group_schedule
      schedule_shr_mem_ptr = schedule_shr_mem + (proc_index * per_process_schedule_size);
      v_port_table[proc_index].group_schedule = schedule_shr_mem_ptr;
      NSDL2_SCHEDULE(NULL, NULL, "scenario_schedule=%p", schedule_shr_mem_ptr);
  
      //Some fields will not be valid, re-initialized below
      memcpy(v_port_table[proc_index].group_schedule, group_schedule, sizeof(Schedule) * total_runprof_entries);

      //2. Phase_array
      phase_shr_mem_ptr = schedule_shr_mem_ptr + next_power_of_n((sizeof(Schedule) * total_runprof_entries), WORDSIZE);
      schedule_shr_mem_ptr += next_power_of_n((sizeof(Schedule) * total_runprof_entries), WORDSIZE);

      NSDL2_SCHEDULE(NULL, NULL, "phase_shr_mem=%p", phase_shr_mem_ptr);
 
      for(grp_idx = 0; grp_idx < total_runprof_entries; grp_idx++)
      {
        v_port_table[proc_index].group_schedule[grp_idx].phase_array = phase_shr_mem_ptr;

        /* Note while memcpy, the values will also go which will be inaccurate, this will be recalculated */
   
        memcpy(v_port_table[proc_index].group_schedule[grp_idx].phase_array, group_schedule[grp_idx].phase_array, sizeof(Phases) * group_schedule[grp_idx].num_phases);

        phase_shr_mem_ptr += (next_power_of_n(sizeof(Phases), WORDSIZE) * group_schedule[grp_idx].num_phases);
      }
     NSDL2_SCHEDULE(NULL, NULL, "phase_shr_mem=%p", phase_shr_mem_ptr);

     schedule_shr_mem_ptr += (next_power_of_n(sizeof(Phases), WORDSIZE) * total_num_phases);

     for(grp_idx = 0; grp_idx < total_runprof_entries; grp_idx++)
     {
       for(y = 0; y < group_schedule[grp_idx].num_phases; y++) 
       {
         ph = &v_port_table[proc_index].group_schedule[grp_idx].phase_array[y];
         if (ph->phase_type == SCHEDULE_PHASE_RAMP_UP)
         {
           ph->phase_cmd.ramp_up_phase.nvm_dist_index = -1;
           ph->phase_cmd.ramp_up_phase.nvm_dist_count = 0;
         }
         else if(ph->phase_type == SCHEDULE_PHASE_RAMP_DOWN)
         {
           ph->phase_cmd.ramp_down_phase.nvm_dist_index = -1;
           ph->phase_cmd.ramp_down_phase.nvm_dist_count = 0;
         }
         v_port_table[proc_index].group_schedule[grp_idx].phase_array[y].phase_status = PHASE_NOT_STARTED;
         v_port_table[proc_index].group_schedule[grp_idx].phase_array[y].runtime_flag = 0;
       }

       //3. phase_ramp_tmr
       v_port_table[proc_index].group_schedule[grp_idx].phase_ramp_tmr = schedule_shr_mem_ptr;
       v_port_table[proc_index].group_schedule[grp_idx].phase_ramp_tmr->timer_type = -1;
       v_port_table[proc_index].group_schedule[grp_idx].phase_ramp_tmr->next = NULL;
       v_port_table[proc_index].group_schedule[grp_idx].phase_ramp_tmr->prev = NULL;
       schedule_shr_mem_ptr += next_power_of_n(sizeof(timer_type), WORDSIZE); 
       
        NSDL2_SCHEDULE(NULL, NULL, "phase_ramp_tmr=%p", schedule_shr_mem_ptr);
 
       //4. phase_end_tmr
       v_port_table[proc_index].group_schedule[grp_idx].phase_end_tmr = schedule_shr_mem_ptr;
       v_port_table[proc_index].group_schedule[grp_idx].phase_end_tmr->timer_type = -1;
       v_port_table[proc_index].group_schedule[grp_idx].phase_end_tmr->next = NULL;
       v_port_table[proc_index].group_schedule[grp_idx].phase_end_tmr->prev = NULL;
       schedule_shr_mem_ptr += next_power_of_n(sizeof(timer_type), WORDSIZE);

        NSDL2_SCHEDULE(NULL, NULL, "phase_end_tmr=%p", schedule_shr_mem_ptr);

       //5. Runtime_qty_change
       //For Run Time Changes in quantity
       v_port_table[proc_index].group_schedule[grp_idx].runtime_qty_change = schedule_shr_mem_ptr;
       memset(v_port_table[proc_index].group_schedule[grp_idx].runtime_qty_change, 0, (sizeof(int) * 2));
       schedule_shr_mem_ptr += next_power_of_n(sizeof(int) * 2, WORDSIZE);

       NSDL2_SCHEDULE(NULL, NULL, "Runtime_qty_change=%p", schedule_shr_mem_ptr);
    }
   }
   update_child_runtime_schedule(schedule_shr_mem_ptr);
 }
}

int get_total_from_nvm(int nvm_id, int grp_idx)
{
  int total = 0;
  int i;

  NSDL3_SCHEDULE(NULL, NULL, "Method called nvm_id = %d, grp_idx = %d", nvm_id, grp_idx);
  if (grp_idx != -1) {
    total = per_proc_runprof_table[(nvm_id * total_runprof_entries) + grp_idx];
    NSDL3_SCHEDULE(NULL, NULL, "returning %d", total);
    return total;
  } else {  /* total is going to be sum of all grps*/
    for (i = 0; i < total_runprof_entries; i++) {
      total += per_proc_runprof_table[(nvm_id * total_runprof_entries) + i];
    }
    NSDL3_SCHEDULE(NULL, NULL, "returning %d", total);
    return total;
  }
}

/* 
   From Man page of qsort
     The  comparison function must return an integer less than, equal to, or greater than zero if the first argu-
     ment is considered to be respectively less than, equal to, or greater than the second.  If two members  com-
     pare as equal, their order in the sorted array is undefined.

   NOTE:
   Here we want to sort in decending order so the signs are opposite.

 */
static int sort_priority_array(const void *P1, const void *P2)
{
  struct PriorityArray *p1, *p2;
  p1 = (struct PriorityArray *)P1;
  p2 = (struct PriorityArray *)P2;

  if (p1->value > p2->value)
    return -1;
  if (p1->value == p2->value)
    return 0;
  if (p1->value < p2->value)
    return 1;

  /* should not reach */
  return 0;
}

#if 0
/* Function for per phase NVM distribution */
static int is_high_water_mark_reached(Schedule *schedule, int num_phases, int hwm)
{
  if (calculate_high_water_mark(schedule, num_phases) >= hwm) {
    NSDL4_SCHEDULE(NULL, NULL, "HWM reached hwm = %u\n", hwm);
    return 1;
  }
  /* else */
  NSDL4_SCHEDULE(NULL, NULL, "HWM did not reach. hwm = %u\n", hwm);
  return 0;
}
#endif 

#if 0
/* Function for per phase per grp distribution */
static int is_high_water_mark_reached2(Schedule *schedule, int num_phases, int grp_idx, int hwm)
{
  Phases *phase_array = schedule->phase_array;
  Phases *ph_tmp;
  int act_user = 0;
  int i;
  int tmp_hwm = 0;

  NSDL4_SCHEDULE(NULL, NULL, "Method Called. num_phases = %d, grp_idx = %d, hwm = %d",
                 num_phases, grp_idx, hwm);
  act_user = 0;
  for (i = 0; i < num_phases; i++) {
    ph_tmp = &(phase_array[i]);

    switch(ph_tmp->phase_type) {
    case SCHEDULE_PHASE_START:
      break;
    case SCHEDULE_PHASE_RAMP_UP:
      act_user += ph_tmp->phase_cmd.ramp_up_phase.per_grp_qty[grp_idx];
      break;
    case SCHEDULE_PHASE_RAMP_DOWN:
      act_user -= ph_tmp->phase_cmd.ramp_down_phase.per_grp_qty[grp_idx];
      break;
    case SCHEDULE_PHASE_DURATION:
      break;
    }

    if (act_user < 0) {
      fprintf(stderr, "Invalid phase definations\n");
      exit(-1);
    }
    
    if (tmp_hwm < act_user) {
      tmp_hwm = act_user;
    }
  }

  if (hwm == tmp_hwm) {
    NSDL4_SCHEDULE(NULL, NULL, "HWM reached");
    return 1;
  } else {
    NSDL4_SCHEDULE(NULL, NULL, "HWM did not reach (%u, limit = %u", tmp_hwm, hwm);
    return 0;
  }
}
#endif

static int active_quantity_left(Schedule *schedule, int num_phases)
{
  Phases *phase_array = schedule->phase_array;
  Phases *ph_tmp;
  int act_user = 0;
  int i;

  for (i = 0; i < num_phases; i++) {
    ph_tmp = &(phase_array[i]);

    switch(ph_tmp->phase_type) {
    case SCHEDULE_PHASE_START:
      break;
    case SCHEDULE_PHASE_RAMP_UP:
      act_user += ph_tmp->phase_cmd.ramp_up_phase.num_vusers_or_sess;
      break;
    case SCHEDULE_PHASE_RAMP_DOWN:
      act_user -= ph_tmp->phase_cmd.ramp_down_phase.num_vusers_or_sess;
      break;
    case SCHEDULE_PHASE_DURATION:
      break;
    }
    if (act_user < 0) {
      NS_EXIT(-1, "active users are less than 0, so active_quantity_left() failed.");
    }
  }

  NSDL3_SCHEDULE(NULL, NULL, "returning %u", act_user);
  return act_user;
}

static int active_quantity_left2(Schedule *schedule, int num_phases, int grp_idx)
{
  Phases *phase_array = schedule->phase_array;
  Phases *ph_tmp;
  int act_user = 0;
  int i;

  for (i = 0; i < num_phases; i++) {
    ph_tmp = &(phase_array[i]);

    switch(ph_tmp->phase_type) {
    case SCHEDULE_PHASE_START:
      break;
    case SCHEDULE_PHASE_RAMP_UP:
      act_user += ph_tmp->phase_cmd.ramp_up_phase.per_grp_qty[grp_idx];
      break;
    case SCHEDULE_PHASE_RAMP_DOWN:
      act_user -= ph_tmp->phase_cmd.ramp_down_phase.per_grp_qty[grp_idx];
      break;
    case SCHEDULE_PHASE_DURATION:
      break;
    }
    if (act_user < 0) {
      NS_EXIT(-1, "active users are less than 0, so active_quantity_left2() failed.");
    }
  }

  NSDL3_SCHEDULE(NULL, NULL, "returning %u", act_user);
  return act_user;
}

static int active_quantity_left_from_per_grp_qty(Schedule *schedule, int num_phases, int grp_idx)
{
  Phases *phase_array = schedule->phase_array;
  Phases *ph_tmp;
  int act_user = 0;
  int i;
  int *per_grp_qty;


  for (i = 0; i < num_phases; i++) {
    ph_tmp = &(phase_array[i]);


    switch(ph_tmp->phase_type) {
    case SCHEDULE_PHASE_START:
      break;
    case SCHEDULE_PHASE_RAMP_UP:
      //act_user += ph_tmp->phase_cmd.ramp_up_phase.num_vusers_or_sess;
      per_grp_qty = ph_tmp->phase_cmd.ramp_up_phase.per_grp_qty;
      act_user += per_grp_qty[grp_idx];
      break;
    case SCHEDULE_PHASE_RAMP_DOWN:
      //act_user -= ph_tmp->phase_cmd.ramp_down_phase.num_vusers_or_sess;
      per_grp_qty = ph_tmp->phase_cmd.ramp_down_phase.per_grp_qty;
      act_user -= per_grp_qty[grp_idx];
      break;
    case SCHEDULE_PHASE_DURATION:
      break;
    }
    if (act_user < 0) {
      NS_EXIT(-1, "active users are  less than 0, so active_quantity_left_from_per_grp_qty() failed.");
    }
  }

  return act_user;
}

static int is_quantity_left_zero(Schedule *schedule, int num_phases)
{
  Phases *phase_array = schedule->phase_array;
  Phases *ph_tmp;
  int act_user = 0;
  int i;

  for (i = 0; i < num_phases; i++) {
    ph_tmp = &(phase_array[i]);

    switch(ph_tmp->phase_type) {
    case SCHEDULE_PHASE_START:
      break;
    case SCHEDULE_PHASE_RAMP_UP:
      act_user += ph_tmp->phase_cmd.ramp_up_phase.num_vusers_or_sess;
      break;
    case SCHEDULE_PHASE_RAMP_DOWN:
      act_user -= ph_tmp->phase_cmd.ramp_down_phase.num_vusers_or_sess;
      break;
    case SCHEDULE_PHASE_DURATION:
      break;
    }
    if (act_user < 0) {
      NS_EXIT(-1, "active users are less than 0, so is_quantity_left_zero() failed.");
    }
  }

  if (act_user == 0)
    return 1;
  else 
    return 0;
}

static int is_quantity_left_zero2(Schedule *schedule, int num_phases, int grp_idx)
{
  Phases *phase_array = schedule->phase_array;
  Phases *ph_tmp;
  int act_user = 0;
  int i;

  NSDL4_SCHEDULE(NULL, NULL, "Method called. num_phases = %d, grp_idx = %d", num_phases, grp_idx);
  for (i = 0; i < num_phases; i++) {
    ph_tmp = &(phase_array[i]);

    switch(ph_tmp->phase_type) {
    case SCHEDULE_PHASE_START:
      break;
    case SCHEDULE_PHASE_RAMP_UP:
      act_user += ph_tmp->phase_cmd.ramp_up_phase.per_grp_qty[grp_idx];
      break;
    case SCHEDULE_PHASE_RAMP_DOWN:
      act_user -= ph_tmp->phase_cmd.ramp_down_phase.per_grp_qty[grp_idx];
      break;
    case SCHEDULE_PHASE_DURATION:
      break;
    }
    if (act_user < 0) {
      NS_EXIT(-1, "active users are less than 0 so is_quantity_left_zero2() failed.");
    }
  }

  NSDL4_SCHEDULE(NULL, NULL, "active users = %d", act_user);
  if (act_user == 0) {
    return 1;
  } else {
    return 0;
  }
}

/* This function also fills the value for all NVMs */
void balance_phase_array_per_nvm(int qty_to_distribute, double *pct_array, double *qty_array, Schedule *schedule, int grp_idx, int phase_id)
{
  struct PriorityArray  *priority_array;
  int sum = 0, diff;
  Phases  *ph_dest;
  int i;
  Schedule *dest_schedule;
  int *users_or_sess;
  int nvm_id;
  int act_users = 0;
  int hwm;// = calculate_high_water_mark(schedule, schedule->num_phases);
  static int last_proc_idx = 0;

  MY_MALLOC(priority_array, sizeof(struct PriorityArray) * global_settings->num_process, "priority_array", -1);
  
#ifdef NS_DEBUG_ON
  NSDL3_SCHEDULE(NULL, NULL, "Qty array:");
  for (i = 0; i < global_settings->num_process; i++) {
    NSDL3_SCHEDULE(NULL, NULL, "qty_array[%d] = %f", i, qty_array[i]);
  }
  NSDL3_SCHEDULE(NULL, NULL, "Pct array:");
  for (i = 0; i < global_settings->num_process; i++) {
    NSDL3_SCHEDULE(NULL, NULL, "pct_array[%d] = %f", i, pct_array[i]);
  }

#endif /* NS_DEBUG_ON */
  int proc_index = 0;
  for (i = 0; i < global_settings->num_process; i++) {
    priority_array[i].idx = i;
    priority_array[i].value = pct_array[i];

    if (grp_idx == -1)
      dest_schedule = v_port_table[i].scenario_schedule;
    else
      dest_schedule = &(v_port_table[i].group_schedule[grp_idx]);

    ph_dest = &(dest_schedule->phase_array[phase_id]);
     

    hwm = get_total_from_nvm(i, grp_idx);
    act_users = active_quantity_left(dest_schedule, phase_id);
    switch(ph_dest->phase_type) {
    case SCHEDULE_PHASE_RAMP_UP:
      users_or_sess = &(ph_dest->phase_cmd.ramp_up_phase.num_vusers_or_sess);
      if ((act_users + (int)qty_array[i]) > hwm)
        *users_or_sess = hwm - act_users; 
      else 
        *users_or_sess = (int)qty_array[i]; /* Fill initially */
      if(*users_or_sess)
        ph_dest->phase_cmd.ramp_up_phase.nvm_dist_index = proc_index++;
      break;
    case SCHEDULE_PHASE_RAMP_DOWN:
      users_or_sess = &(ph_dest->phase_cmd.ramp_down_phase.num_vusers_or_sess);
      if (act_users < (int)qty_array[i])
        *users_or_sess = act_users;
      else
        *users_or_sess = (int)qty_array[i]; /* Fill initially */
      if(*users_or_sess)
        ph_dest->phase_cmd.ramp_down_phase.nvm_dist_index = proc_index++;
      break;
    }
    sum += *users_or_sess;
    NSDL4_SCHEDULE(NULL, NULL, "For NVM id = %d, nvm_dist_index = %d, nvm_dist_count = %d, proc_index = %d, users_or_sess = %d", i,
                               ph_dest->phase_cmd.ramp_down_phase.nvm_dist_index,
                               ph_dest->phase_cmd.ramp_down_phase.nvm_dist_count, proc_index, *users_or_sess);
  }
  
  qsort(priority_array, global_settings->num_process, sizeof(struct PriorityArray), sort_priority_array);

#ifdef NS_DEBUG_ON
  NSDL4_SCHEDULE(NULL, NULL, "After qsort priority_array : ");
  for (i = 0; i < global_settings->num_process; i++) {
    NSDL4_SCHEDULE(NULL, NULL, "priority_array[%d].idx = %d, value = %f", i, priority_array[i].idx, priority_array[i].value);
  }
#endif  /* NS_DEBUG_ON */

  diff = qty_to_distribute - sum;

  NSDL4_SCHEDULE(NULL, NULL, "qty_to_distribute = %u, sum = %u, diff = %u", qty_to_distribute, sum, diff);
  i = last_proc_idx;
  while(diff) {
    for (; (i < global_settings->num_process) && (diff); i++) {
      nvm_id = priority_array[i].idx;

      NSDL4_SCHEDULE(NULL, NULL, "Filling in nvm_id = %d, diff = %u, i = %d", nvm_id,  diff, i);
      if (grp_idx == -1)
        dest_schedule = v_port_table[nvm_id].scenario_schedule;
      else
        dest_schedule = &(v_port_table[nvm_id].group_schedule[grp_idx]);
        
      ph_dest = &(dest_schedule->phase_array[phase_id]);
      if (ph_dest->phase_type == SCHEDULE_PHASE_RAMP_UP) {
        hwm = get_total_from_nvm(nvm_id, grp_idx);
        act_users = active_quantity_left(dest_schedule, phase_id + 1);

        //if (!is_high_water_mark_reached(dest_schedule, phase_id + 1, hwm)) {
        if (act_users < hwm) {
          ph_dest->phase_cmd.ramp_up_phase.num_vusers_or_sess++;
          diff--;
          if(ph_dest->phase_cmd.ramp_up_phase.nvm_dist_index < 0)
            ph_dest->phase_cmd.ramp_up_phase.nvm_dist_index = proc_index++;
          NSDL4_SCHEDULE(NULL, NULL, "Filled %s now = %u, diff = %d, nvm_dist_index = %d", ph_dest->phase_name, 
                         ph_dest->phase_cmd.ramp_up_phase.num_vusers_or_sess, diff, ph_dest->phase_cmd.ramp_up_phase.nvm_dist_index);
          continue;
        } else {
          NSDL4_SCHEDULE(NULL, NULL, "HWM reached hwm = %u\n", hwm);
        }
      } else if (ph_dest->phase_type == SCHEDULE_PHASE_RAMP_DOWN) {
        if (!is_quantity_left_zero(dest_schedule, phase_id + 1)) {
          ph_dest->phase_cmd.ramp_down_phase.num_vusers_or_sess++;
          diff--;
          if(ph_dest->phase_cmd.ramp_down_phase.nvm_dist_index < 0)
            ph_dest->phase_cmd.ramp_down_phase.nvm_dist_index = proc_index++;
          NSDL4_SCHEDULE(NULL, NULL, "Filled %s now = %u, diff = %d, nvm_dist_index = %d", ph_dest->phase_name, 
                         ph_dest->phase_cmd.ramp_down_phase.num_vusers_or_sess, diff, ph_dest->phase_cmd.ramp_up_phase.nvm_dist_index);
          continue;
        }
      }
    }
    if (i == global_settings->num_process) i = 0;
  }
  last_proc_idx = i%global_settings->num_process;
  //proc_index--;  //Reducing 1  as intialized with 1
  //NSDL4_SCHEDULE(NULL, NULL,"phase_id = %d, group_id =%d, nvm_dist_count = %d", phase_id, grp_idx, proc_index); 
  for(i=0; i< global_settings->num_process; i++)
  {
    if (grp_idx == -1)
      dest_schedule = v_port_table[i].scenario_schedule;
    else
      dest_schedule = &(v_port_table[i].group_schedule[grp_idx]);

    ph_dest = &(dest_schedule->phase_array[phase_id]);
    if (ph_dest->phase_type == SCHEDULE_PHASE_RAMP_UP) 
      ph_dest->phase_cmd.ramp_up_phase.nvm_dist_count = proc_index;
    else if (ph_dest->phase_type == SCHEDULE_PHASE_RAMP_DOWN)
      ph_dest->phase_cmd.ramp_down_phase.nvm_dist_count = proc_index;
  }
  NSDL4_SCHEDULE(NULL, NULL,"phase_id = %d, group_id =%d, nvm_dist_count = %d", phase_id, grp_idx, proc_index); 
 
  FREE_AND_MAKE_NOT_NULL(priority_array, "priority_array", -1);
}

void balance_phase_array_per_group(int qty_to_distribute, double *pct_array, 
                                   double *qty_array, Schedule *schedule, int phase_id, int proc_index)
{
  struct PriorityArray  *priority_array = NULL;
  int sum = 0, diff;
  Phases *ph;
  int grp_idx, i;
  int *per_grp_qty;
  int act_qty;
  int hwm;
  int *my_runprof_table;  //Pointer to per_proc_runprof_table for proc_index
  
  NSDL4_SCHEDULE(NULL, NULL, "Method Called. NVM=%d, qty_to_distribute=%d, phase_id=%d", 
                                proc_index, qty_to_distribute,phase_id);
  my_runprof_table = &per_proc_runprof_table[proc_index * total_runprof_entries];

  MY_MALLOC(priority_array, sizeof(struct PriorityArray) * total_runprof_entries, "priority_array", -1);
  NSDL4_SCHEDULE(NULL, NULL, "Initialized priority_array = %p", priority_array);

  ph = &(schedule->phase_array[phase_id]);
  
  switch (ph->phase_type) {
  case SCHEDULE_PHASE_RAMP_UP:
    NSDL4_SCHEDULE(NULL, NULL, "RU per_grp_qty= %p", ph->phase_cmd.ramp_up_phase.per_grp_qty);
    per_grp_qty = ph->phase_cmd.ramp_up_phase.per_grp_qty;
    break;
  case SCHEDULE_PHASE_RAMP_DOWN:
    NSDL4_SCHEDULE(NULL, NULL, "RD per_grp_qty= %p", ph->phase_cmd.ramp_down_phase.per_grp_qty);
    per_grp_qty = ph->phase_cmd.ramp_down_phase.per_grp_qty;
    break;
  }

  for (i = 0; i < total_runprof_entries; i++) {
    priority_array[i].idx = i;
    //priority_array[i].value = pct_array[i];
    priority_array[i].value = qty_array[i] - (int)qty_array[i];
    act_qty = active_quantity_left_from_per_grp_qty(schedule, phase_id, i);
    hwm = get_total_from_nvm(proc_index, i);

    if (ph->phase_type == SCHEDULE_PHASE_RAMP_DOWN) {
      if (act_qty < (int)qty_array[i])
        per_grp_qty[i] = act_qty; /* Fill it initially */
      else
        per_grp_qty[i] = (int)qty_array[i]; /* Fill it initially */
    } else if (ph->phase_type == SCHEDULE_PHASE_RAMP_UP) {
      if (act_qty + (int)qty_array[i] > hwm)
        per_grp_qty[i] = hwm - act_qty;
      else
        per_grp_qty[i] = (int)qty_array[i]; /* Fill it initially */
    }

    sum += (int)per_grp_qty[i];
    per_grp_qty_per_phase[(i * schedule->num_phases) + phase_id] -= (int)per_grp_qty[i];
    NSDL2_SCHEDULE(NULL, NULL, "NVM[%d] phase_id = %d, (%p) priority_array[%d].value = %f, idx = %d, "
                   "per_grp_qty[%d] = %u, qty_array[%d] = %f, sum = %u ,per_grp_qty_per_phase = %d,", proc_index, phase_id,
                   priority_array, i, priority_array[i].value, i, 
                   i, per_grp_qty[i], i, qty_array[i], sum, per_grp_qty_per_phase[(i * schedule->num_phases) + phase_id]);
  }
  
#ifdef NS_DEBUG_ON
  NSDL4_SCHEDULE(NULL, NULL, "Before qsort priority_array : ");
  for (i = 0; i < total_runprof_entries; i++) {
    NSDL4_SCHEDULE(NULL, NULL, "priority_array[%d].idx = %d, value = %f", i, priority_array[i].idx, priority_array[i].value);
  }
#endif  /* NS_DEBUG_ON */

  if ((diff = qty_to_distribute - sum) != 0) {
    qsort(priority_array, total_runprof_entries, sizeof(struct PriorityArray), sort_priority_array);

    NSDL2_SCHEDULE(NULL, NULL, "sum = %u, diff = %u\n", sum, diff);
    
#ifdef NS_DEBUG_ON
    NSDL4_SCHEDULE(NULL, NULL, "After qsort priority_array : ");
    for (i = 0; i < total_runprof_entries; i++) {
      NSDL4_SCHEDULE(NULL, NULL, "priority_array[%d].idx = %d, value = %f", i, priority_array[i].idx, priority_array[i].value);
    }
#endif  /* NS_DEBUG_ON */

    NSDL4_SCHEDULE(NULL, NULL, "After qsort priority_array = %p, diff = %u, sum = %u", priority_array, diff, sum);
  }

  int hwm_flag;
  i = 0;
  int retry = 0;
  //retry should be 0 until diff got 0
  while(diff) {
    hwm_flag=1;
    for (; (i < total_runprof_entries) && (diff); i++) {
      grp_idx = priority_array[i].idx;

      if (ph->phase_type == SCHEDULE_PHASE_RAMP_UP) {
        hwm = my_runprof_table[grp_idx];

        act_qty = active_quantity_left2(schedule, phase_id + 1, grp_idx);
        NSDL4_SCHEDULE(NULL, NULL, "PHASE[%d] GROUP[%d] NVM[%d] act_qty = %d, hwm = %d, per_grp_qty_per_phase = %d,"
                                   " diff=%d",phase_id, grp_idx, proc_index, act_qty, hwm,
                                   per_grp_qty_per_phase[(grp_idx * schedule->num_phases) + phase_id], diff);
        if ((act_qty < hwm) && (retry || (per_grp_qty_per_phase[(grp_idx * schedule->num_phases) + phase_id] > 0))) {
          if(per_grp_qty_per_phase[(grp_idx * schedule->num_phases) + phase_id] > 0)
            per_grp_qty_per_phase[(grp_idx * schedule->num_phases) + phase_id]--;
          per_grp_qty[grp_idx]++;
          diff--;
          hwm_flag=0;
          continue;
        }
      } else if (ph->phase_type == SCHEDULE_PHASE_RAMP_DOWN) {
        if (!is_quantity_left_zero2(schedule, phase_id + 1, grp_idx)) {
          per_grp_qty[grp_idx]++;
          if(per_grp_qty_per_phase[(grp_idx * schedule->num_phases) + phase_id] > 0)
            per_grp_qty_per_phase[(grp_idx * schedule->num_phases) + phase_id]--;
          diff--;
          hwm_flag=0; 
          continue;
        }
      }
    }
    if(hwm_flag)
    {
      NSDL4_SCHEDULE(NULL, NULL, "All group HWM reached, Retry");
      if(!retry) 
        retry = 1; 
      else
      { 
        NSDL4_SCHEDULE(NULL, NULL, "All group HWM reached, Retry Done");
        break;
      }
    }
    i %= total_runprof_entries;
  }

  NSDL4_SCHEDULE(NULL, NULL, "Going to Free priority_array = %p", priority_array);
  FREE_AND_MAKE_NOT_NULL(priority_array, "priority_array", -1);
}

void distribute_phase_over_nvm(int grp_idx, Schedule *schedule)
{
  double *qty_array;
  double *pct_array;
  int total_qty;
  int grp_mode = get_group_mode(grp_idx);
  int phase_id, nvm_id;
  Phases *ph;
  int qty_to_distribute;
  
  MY_MALLOC(qty_array, sizeof(double) * global_settings->num_process, "qty_array", -1);
  MY_MALLOC(pct_array, sizeof(double) * global_settings->num_process, "pct_array", -1);

  //if (grp_idx == -1 /* scenario based */) {
  if (grp_idx == -1)
    total_qty = (grp_mode == TC_FIX_CONCURRENT_USERS) ? global_settings->num_connections : global_settings->vuser_rpm; 
  else 
    total_qty = runprof_table_shr_mem[grp_idx].quantity;

  NSDL3_SCHEDULE(NULL, NULL, "total_qty to distribute = %u, grp_idx = %d ", total_qty, grp_idx);

  for (phase_id = 0; phase_id < schedule->num_phases; phase_id++) {
    ph = &(schedule->phase_array[phase_id]);

    if ((ph->phase_type != SCHEDULE_PHASE_RAMP_UP) && (ph->phase_type != SCHEDULE_PHASE_RAMP_DOWN)) continue;
    switch(ph->phase_type) {
    case SCHEDULE_PHASE_RAMP_UP:
      qty_to_distribute = (ph->phase_cmd.ramp_up_phase.num_vusers_or_sess);
      NSDL3_SCHEDULE(NULL, NULL, "grp = %d, Phase %s qty_to_distribute = %u", grp_idx, ph->phase_name, qty_to_distribute);
      break;
    case SCHEDULE_PHASE_RAMP_DOWN:
      qty_to_distribute = (ph->phase_cmd.ramp_down_phase.num_vusers_or_sess);
      NSDL3_SCHEDULE(NULL, NULL, "grp = %d, Phase %s qty_to_distribute = %u", grp_idx, ph->phase_name, qty_to_distribute);
      break;
    }
      
    for (nvm_id = 0; nvm_id < global_settings->num_process; nvm_id++) {
      pct_array[nvm_id] = (double)(((double)get_total_from_nvm(nvm_id, grp_idx)) / 
                                   ((double)total_qty));
      qty_array[nvm_id] = (double)(qty_to_distribute * pct_array[nvm_id]);
    }

    balance_phase_array_per_nvm(qty_to_distribute, pct_array, qty_array, schedule, grp_idx, phase_id);

#ifdef NS_DEBUG_ON
    Schedule *dest_schedule;
    Phases *ph_dest;
    /* Print distributed phase. */
    NSDL2_SCHEDULE(NULL, NULL, "Phase %d, qty = %u distributed as:", phase_id, qty_to_distribute);
    for (nvm_id = 0; nvm_id < global_settings->num_process; nvm_id++) {
      if (grp_idx == -1)
        dest_schedule = v_port_table[nvm_id].scenario_schedule;
      else
        dest_schedule = &(v_port_table[nvm_id].group_schedule[grp_idx]);
      
      ph_dest = &(dest_schedule->phase_array[phase_id]);

      if (ph_dest->phase_type == SCHEDULE_PHASE_RAMP_UP) {
        NSDL2_SCHEDULE(NULL, NULL, "nvm (%d) phase (%d) Ramp Up, qty = %u", 
                       nvm_id, phase_id, 
                       dest_schedule->phase_array[phase_id].phase_cmd.ramp_up_phase.num_vusers_or_sess);
        
      } else if (ph_dest->phase_type == SCHEDULE_PHASE_RAMP_DOWN) {
        NSDL2_SCHEDULE(NULL, NULL, "nvm (%d) phase (%d) Ramp Down, qty = %u", 
                       nvm_id, phase_id, 
                       dest_schedule->phase_array[phase_id].phase_cmd.ramp_down_phase.num_vusers_or_sess);
      }
    }
#endif  /* NS_DEBUG_ON */
  }

  FREE_AND_MAKE_NOT_NULL(qty_array, "qty_array", -1);
  FREE_AND_MAKE_NOT_NULL(pct_array, "pct_array", -1);
}

void fill_per_group_phase_table(Schedule *schedule)
{
  int phase_id;
  Phases *ph;
  int total_qty = 0;
  int grp_idx;
  int *phase_leftover;
  int *rampup_leftover;
  int *leftover_ptr;
  int *rampdown_leftover;
  int qty_to_distribute;
  int idx;

  NSDL3_SCHEDULE(NULL, NULL, "Method Called");

  MY_MALLOC(phase_leftover, sizeof(int) * schedule->num_phases, "phase_leftover", -1);
  MY_MALLOC(rampup_leftover, sizeof(int) * total_runprof_entries, "rampup_leftover", -1);
  MY_MALLOC(rampdown_leftover, sizeof(int) * total_runprof_entries, "rampdown_leftover", -1);
  if(!per_grp_qty_per_phase)
  {
    MY_MALLOC(per_grp_qty_per_phase , sizeof(int) * total_runprof_entries * schedule->num_phases, "per_grp_qty_per_phase", -1); 
  }
  memset(per_grp_qty_per_phase, 0, sizeof(int) * total_runprof_entries * schedule->num_phases);
  for (grp_idx = 0 ; grp_idx < total_runprof_entries; grp_idx++)
  {
     rampup_leftover[grp_idx] = runprof_table_shr_mem[grp_idx].quantity;
     rampdown_leftover[grp_idx] = runprof_table_shr_mem[grp_idx].quantity;
     total_qty += runprof_table_shr_mem[grp_idx].quantity;
  }
  for (phase_id = 0; phase_id < schedule->num_phases; phase_id++) 
  {
    ph = &(schedule->phase_array[phase_id]);
    if ((ph->phase_type != SCHEDULE_PHASE_RAMP_UP) && (ph->phase_type != SCHEDULE_PHASE_RAMP_DOWN)) continue;
    if (ph->phase_type == SCHEDULE_PHASE_RAMP_UP) 
    {
        qty_to_distribute = (ph->phase_cmd.ramp_up_phase.num_vusers_or_sess);  
        leftover_ptr = rampup_leftover;   
        NSDL3_SCHEDULE(NULL, NULL, "Schedule phase RAMP_UP qty_to_distribute = %u", qty_to_distribute);
    }
    else 
    {
        qty_to_distribute = (ph->phase_cmd.ramp_down_phase.num_vusers_or_sess);    
        leftover_ptr = rampdown_leftover;   
        NSDL3_SCHEDULE(NULL, NULL, "Schedule phase RAMP_DOWN qty_to_distribute = %u", qty_to_distribute);
    }
    phase_leftover[phase_id] = qty_to_distribute;
    for (grp_idx = 0 ; grp_idx < total_runprof_entries; grp_idx++)
    {
      idx = (grp_idx * schedule->num_phases) + phase_id; 
      per_grp_qty_per_phase[idx] = (qty_to_distribute * runprof_table_shr_mem[grp_idx].quantity) / total_qty;
      leftover_ptr[grp_idx] -= per_grp_qty_per_phase[idx];
      phase_leftover[phase_id] -= per_grp_qty_per_phase[idx];
    } 
  }
  /*
  for (phase_id = 0; phase_id < schedule->num_phases; phase_id++)
  {
    ph = &(schedule->phase_array[phase_id]);
    if ((ph->phase_type != SCHEDULE_PHASE_RAMP_UP) && (ph->phase_type != SCHEDULE_PHASE_RAMP_DOWN)) continue;
    NSDL3_SCHEDULE(NULL, NULL, "MM: Check, phase_id = %d, type = %d", phase_id, ph->phase_type);
    for (grp_idx = 0 ; grp_idx < total_runprof_entries; grp_idx++)
    {
     idx = (grp_idx * schedule->num_phases) + phase_id; 
     if (ph->phase_type == SCHEDULE_PHASE_RAMP_UP)
        NSDL3_SCHEDULE(NULL, NULL, "Check Rampup idx = %d, phase_id = %d, grp_idx = %d, qty=%d",
                                    idx, phase_id, grp_idx, per_grp_qty_per_phase[idx]);
     else
      NSDL3_SCHEDULE(NULL, NULL, "Check RampDown phase_id[%d] phase_leftover=%d, grp_idx[%d] rampup_leftover=%d qty=%d",phase_id,phase_leftover2[phase_id],grp_idx, rampdown_leftover[grp_idx],per_grp_qty_per_phase[(grp_idx * total_runprof_entries) + phase_id]);
    }
  }
  */ 
  int next_grp_idx = 0; 
  int last_grp_idx = -1; 
  for (phase_id = 0; phase_id < schedule->num_phases; phase_id++) 
  {
    ph = &(schedule->phase_array[phase_id]);
    if ((ph->phase_type != SCHEDULE_PHASE_RAMP_UP) && (ph->phase_type != SCHEDULE_PHASE_RAMP_DOWN)) continue;
    while(phase_leftover[phase_id])
    {
      next_grp_idx = last_grp_idx + 1;
      NSDL2_SCHEDULE(NULL, NULL, "next_grp_idx %d",next_grp_idx);
      for (grp_idx = next_grp_idx ; grp_idx < ( total_runprof_entries + next_grp_idx) && phase_leftover[phase_id] > 0; grp_idx++) 
      {
        grp_idx%=total_runprof_entries;
        idx = (grp_idx * schedule->num_phases) + phase_id; 
        if (ph->phase_type == SCHEDULE_PHASE_RAMP_UP) 
          leftover_ptr = rampup_leftover;
        else
          leftover_ptr = rampdown_leftover;
  
        if(leftover_ptr[grp_idx])
        {
          per_grp_qty_per_phase[idx]++;
          leftover_ptr[grp_idx]--;
          phase_leftover[phase_id]--; 
          last_grp_idx = grp_idx;
        }
      }
    }
#ifdef NS_DEBUG_ON
    /* Print distributed phase. */
    for (grp_idx = 0; grp_idx < total_runprof_entries; grp_idx++) {
      idx = (grp_idx * schedule->num_phases) + phase_id; 
      if (ph->phase_type == SCHEDULE_PHASE_RAMP_UP) {
        NSDL2_SCHEDULE(NULL, NULL, "phase (%d/%s), grp (%d), Ramp Up, qty = %u",
                       phase_id, schedule->phase_array[phase_id].phase_name, grp_idx,
                       per_grp_qty_per_phase[idx]);
      } else if (ph->phase_type == SCHEDULE_PHASE_RAMP_DOWN) {
        NSDL2_SCHEDULE(NULL, NULL, "phase (%d/%s), grp (%d), Ramp Down, qty = %u",
                       phase_id, schedule->phase_array[phase_id].phase_name, grp_idx,
                       per_grp_qty_per_phase[idx]);
      }
    }
#endif  /* NS_DEBUG_ON */
  }
  FREE_AND_MAKE_NOT_NULL(phase_leftover, "phase_leftover", -1);
  FREE_AND_MAKE_NOT_NULL(rampup_leftover, "rampup_leftover", -1);
  FREE_AND_MAKE_NOT_NULL(rampdown_leftover, "rampdown_leftover", -1);
}
/* Function can be called only if scenario is of type `scenario based' and by child*/
void distribute_group_qty_over_phase(Schedule *schedule, int proc_index)
{
  int total_qty;
  int phase_id;
  Phases *ph;
  double *qty_array;  //actual qty distribution among groups in an nvm
  double *pct_array;  //% division of group qty in an nvm
  int qty_to_distribute;
  int grp_idx;
  
  NSDL3_SCHEDULE(NULL, NULL, "Method Called. NMV=%d", proc_index);
  MY_MALLOC(qty_array, sizeof(double) * total_runprof_entries, "qty_array", -1);
  MY_MALLOC(pct_array, sizeof(double) * total_runprof_entries, "pct_array", -1);

  for (phase_id = 0; phase_id < schedule->num_phases; phase_id++) {
    ph = &(schedule->phase_array[phase_id]);

    if ((ph->phase_type != SCHEDULE_PHASE_RAMP_UP) && (ph->phase_type != SCHEDULE_PHASE_RAMP_DOWN)) continue;

    if (ph->phase_type == SCHEDULE_PHASE_RAMP_UP) {
      qty_to_distribute = (ph->phase_cmd.ramp_up_phase.num_vusers_or_sess);
      NSDL3_SCHEDULE(NULL, NULL, "Schedule phase RAMP_UP qty_to_distribute = %u", qty_to_distribute);
      //MY_MALLOC(ph->phase_cmd.ramp_up_phase.per_grp_qty, sizeof(int) * total_runprof_entries,  "per_grp_qty", -1);
    } else if (ph->phase_type == SCHEDULE_PHASE_RAMP_DOWN) {
      qty_to_distribute = (ph->phase_cmd.ramp_down_phase.num_vusers_or_sess);
      NSDL3_SCHEDULE(NULL, NULL, "Schedule phase RAMP_DOWN qty_to_distribute = %u", qty_to_distribute);
      //MY_MALLOC(ph->phase_cmd.ramp_down_phase.per_grp_qty, sizeof(int) * total_runprof_entries,  "per_grp_qty", -1);
    }
      
    total_qty = get_total_from_nvm(proc_index, -1);
    for (grp_idx = 0; grp_idx < total_runprof_entries; grp_idx++) {
      pct_array[grp_idx] = (double)(((double)get_total_from_nvm(proc_index, grp_idx)) / 
                                    ((double)total_qty));
      qty_array[grp_idx] = (double)(qty_to_distribute * pct_array[grp_idx]);
      NSDL4_SCHEDULE(NULL, NULL, "pct_array[%d] = %f, qty_array[%d] = %f", 
                     grp_idx, pct_array[grp_idx], grp_idx, qty_array[grp_idx]);
    }

    balance_phase_array_per_group(qty_to_distribute, pct_array, qty_array, schedule, phase_id, proc_index);

#ifdef NS_DEBUG_ON
    /* Print distributed phase. */
    NSDL2_SCHEDULE(NULL, NULL, "NMV=%d qty = %u distributed as:", proc_index, qty_to_distribute);
    for (grp_idx = 0; grp_idx < total_runprof_entries; grp_idx++) {

      if (ph->phase_type == SCHEDULE_PHASE_RAMP_UP) {
        NSDL2_SCHEDULE(NULL, NULL, "phase (%d/%s), grp (%d), Ramp Up, qty = %u", 
                       phase_id, schedule->phase_array[phase_id].phase_name, grp_idx,
                       schedule->phase_array[phase_id].phase_cmd.ramp_up_phase.per_grp_qty[grp_idx]);
      } else if (ph->phase_type == SCHEDULE_PHASE_RAMP_DOWN) {
        NSDL2_SCHEDULE(NULL, NULL, "phase (%d/%s), grp (%d), Ramp Down, qty = %u", 
                       phase_id, schedule->phase_array[phase_id].phase_name, grp_idx,
                       schedule->phase_array[phase_id].phase_cmd.ramp_down_phase.per_grp_qty[grp_idx]);
      }
    }
#endif  /* NS_DEBUG_ON */

  }

  FREE_AND_MAKE_NOT_NULL(qty_array, "qty_array", -1);
  FREE_AND_MAKE_NOT_NULL(pct_array, "pct_array", -1);
}
