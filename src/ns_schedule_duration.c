/************************************************************************************************************
 *  Name            : ns_schedule_duration.c 
 *  Purpose         : To control Netstorm Duration Phase
 *  Initial Version : Monday, July 06 2009
 *  Modification    : -
 ***********************************************************************************************************/

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
#include "util.h"
#include "timing.h"
#include "tmr.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "ns_log.h"
#include "ns_schedule_phases.h"
#include "ns_schedule_duration.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "ns_schedule_phases_parse.h"
#include "ns_replay_access_logs.h"
#include "ns_session.h"
#include "ns_percentile.h"
#include "divide_users.h"

// send phase complete for duration
void start_duration_callback(ClientData cd, u_ns_ts_t now)
{
  Schedule *schedule_ptr = (Schedule *)cd.p;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phase Index = %d, Phase Type = %d", 
    schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type);

  send_phase_complete(schedule_ptr);   
}

// Adds timer for duration time
void start_duration_timer(Schedule *schedule_ptr, int time_val, u_ns_ts_t now)
{
  ClientData cd;
  cd.p = schedule_ptr;  

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phase Index = %d, Phase Type = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type);
  
  //ab_timers[AB_TIMEOUT_END].timeout_val = time_val;
  schedule_ptr->phase_end_tmr->actual_timeout = time_val;
  //dis_timer_add( AB_TIMEOUT_END, schedule_ptr->phase_end_tmr, now, start_duration_callback, cd, 0);
  dis_timer_think_add( AB_TIMEOUT_END, schedule_ptr->phase_end_tmr, now, start_duration_callback, cd, 0);
}

/* This method called when Duration phase has to run. 
 * Duration May have of 3 types:
 *   ---> Indefinite      : Will not send phase complete, hence parent will not send next phase to run.
 *   ---> Time in seconds : Will have to send phase complete message after this milliseconds (1000 * seconds)
 *   ---> Fethes          : Not Handled
 */
void start_duration_phase(Schedule *schedule_ptr, u_ns_ts_t now) 
{
  int time_val;
  Phases *phase_ptr = &schedule_ptr->phase_array[schedule_ptr->phase_idx];
  ClientData cd;
  cd.p = schedule_ptr;
  int scenario_type = get_group_mode(schedule_ptr->group_idx);

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phase Index = %d, Phase Type = %d, Duration Mode = %d, Scenario Type = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, phase_ptr->phase_cmd.duration_phase.duration_mode, scenario_type);

  // For Goal based scenario 
  if((scenario_type != TC_FIX_CONCURRENT_USERS && scenario_type != TC_FIX_USER_RATE) && 
     (run_mode == FIND_NUM_USERS || run_mode == STABILIZE_RUN))
  {

    time_val = global_settings->test_stab_time*1000; 
    NSDL2_SCHEDULE(NULL, NULL, "Goal Based: Group Index = %d, Phase Index = %d, Phase Type = %d, time = %d (ms)", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, time_val);

    start_duration_timer(schedule_ptr, time_val, now);

     return;
  }
  if(global_settings->replay_mode == 0)
    if(!schedule_ptr->cur_vusers_or_sess) {
      send_phase_complete(schedule_ptr);
      return;
    }
 
  if(phase_ptr->phase_cmd.duration_phase.duration_mode == DURATION_MODE_TIME )
  {
    time_val = schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_cmd.duration_phase.seconds; // Already in ms
    NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phase Index = %d, Phase Type = %d, time = %d (ms)", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, time_val);

    if(phase_ptr->phase_cmd.duration_phase.seconds <= 0 )
      start_duration_callback(cd, now);
    else
      start_duration_timer(schedule_ptr, time_val, now);
  }
  else  // Fetches & indefinite : Fethes will be ctrled in is_new_session_blocked
  {
    NSDL2_SCHEDULE(NULL, NULL, "Fetches & indefinite: my_port_index = %d, v_port_entry->num_fetches = %d, sess_inst_num = %d, group_idx = %d, "
                         "total_runprof_entries = %d, per_proc_per_grp_fetches = %d, per_grp_sess_inst_num = %d", 
                         my_port_index, v_port_entry->num_fetches, sess_inst_num, schedule_ptr->group_idx, total_runprof_entries,
                         per_proc_per_grp_fetches?per_proc_per_grp_fetches[(my_port_index * total_runprof_entries) + schedule_ptr->group_idx]:0,
                         per_grp_sess_inst_num?per_grp_sess_inst_num[schedule_ptr->group_idx]:0);

    if(scenario_type == TC_FIX_USER_RATE && global_settings->replay_mode != 0) // Replay 
       replay_mode_user_generation(schedule_ptr); 
    else if(v_port_entry->num_fetches) 
    {
      int send_flag = 0; 
      if((global_settings->schedule_by == SCHEDULE_BY_SCENARIO) && (sess_inst_num >= v_port_entry->num_fetches))
        send_flag = 1;
      else if((global_settings->schedule_by == SCHEDULE_BY_GROUP) && (per_grp_sess_inst_num[schedule_ptr->group_idx] >= per_proc_per_grp_fetches[(my_port_index * total_runprof_entries) + schedule_ptr->group_idx]))
       send_flag = 1;

      if(send_flag)
        send_phase_complete(schedule_ptr);   
    }
    return; 
  }
}
/********************************************************END OF FILE********************************************/
