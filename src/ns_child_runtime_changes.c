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
#include "util.h"
#include "tmr.h"
#include "timing.h"
#include "url.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "ns_parse_scen_conf.h"
#include "ns_ssl.h"
#include "ns_cookie.h"
#include "ns_auto_cookie.h"
#include "ns_log.h"
#include "ns_kw_set_non_rtc.h"
#include "ns_schedule_phases_parse.h"
#include "ns_schedule_ramp_up_fcu.h"
#include "ns_schedule_start_group.h"
#include "ns_schedule_stabilize.h"
#include "ns_schedule_duration.h"
#include "ns_schedule_ramp_down_fcu.h"
#include "ns_schedule_ramp_up_fsr.h"
#include "ns_schedule_ramp_down_fsr.h"
#include "ns_runtime_changes_quantity.h"
#include "poi.h"
#include "nslb_time_stamp.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "ns_trace_level.h"
#include "ns_vuser_runtime_control.h"

double round(double x);

#if 0
static void adjust_time(timer_type *tmr, u_ns_ts_t now, unsigned int *time_val) {
  int time_left, new_val, time_diff;
  new_val = time_diff = time_left = 0;

  NSDL2_RUNTIME(NULL, NULL, "Method Called, actual_timeout = %u, timeout = %u, now = %u, time_val = %u",
                                             tmr->actual_timeout, tmr->timeout, now, *time_val);
  if(tmr->timer_type > 0)  {
     time_left = tmr->timeout - now; 
     time_diff = *time_val - tmr->actual_timeout;
     new_val = time_left + time_diff;
     if(new_val > 0)
       *time_val = new_val;
     else
       *time_val = 0;
     NSDL2_RUNTIME(NULL, NULL, "Setting new time: time left = %d, time diff = %d, new time val = %u, prev_now = %u", time_left, time_diff, *time_val, prev_now);
  }
}
#endif

// --- Run Time Changes  ---START
static void runtime_start_start_phase(Schedule *cur_schedule, u_ns_ts_t now) {
  int time_val = cur_schedule->phase_array[cur_schedule->phase_idx].phase_cmd.start_phase.time;
  Phases *cur_phases = &(cur_schedule->phase_array[cur_schedule->phase_idx]);

  NSDL2_RUNTIME(NULL, NULL, "Method Called");

  if(cur_phases->phase_status == PHASE_RUNNING) {

    if(cur_schedule->phase_end_tmr->timer_type > 0)  {
       /* Fix Bug#13013: where we can apply multiple phases RTC (like: increasing or decrease time on start, stablize 
                    and duration phase), then in this case phase should not be completed with its actual time.*/ 
       //adjust_time(cur_schedule->phase_end_tmr, now, &time_val);
       dis_timer_del(cur_schedule->phase_end_tmr);
    }

    time_val = time_val - (now - cur_phases->phase_start_time);
    NSDL2_RUNTIME(NULL, NULL, "time_val = %u", time_val);

    cur_schedule->phase_array[cur_schedule->phase_idx].phase_cmd.start_phase.time = time_val;
    start_start_phase(cur_schedule, now);
  } else { 
    NSDL2_RUNTIME(NULL, NULL, "Phase is not running.");
  }
}

static void runtime_start_ramp_up_phase(Schedule *cur_schedule, u_ns_ts_t now) {

  Phases *cur_phases = &(cur_schedule->phase_array[cur_schedule->phase_idx]);

  NSDL2_RUNTIME(NULL, NULL, "Method Called");

  /*AN-TODO Need check for FSR*/
  if(cur_phases->phase_status == PHASE_RUNNING) {
     NSDL2_RUNTIME(NULL, NULL, "Phase is running.");

     if(cur_schedule->phase_end_tmr->timer_type > 0)
       dis_timer_del(cur_schedule->phase_end_tmr);

     if(cur_schedule->phase_ramp_tmr->timer_type > 0)
       dis_timer_del(cur_schedule->phase_ramp_tmr);

     //Changes for VuserRTC
     if(get_group_mode(cur_schedule->group_idx) == TC_FIX_CONCURRENT_USERS)
       gVUserSummaryTable[cur_schedule->group_idx].num_down_vuser += cur_phases->phase_cmd.ramp_up_phase.num_vusers_or_sess; //For VUser RTC  

      start_ramp_up_phase(cur_schedule, now);
  } else {
    NSDL2_RUNTIME(NULL, NULL, "Phase is not running.");
  } 
}

static void runtime_start_stabilize_phase(Schedule *cur_schedule, u_ns_ts_t now) {
	
  int time_val = cur_schedule->phase_array[cur_schedule->phase_idx].phase_cmd.stabilize_phase.time;
  Phases *cur_phases = &(cur_schedule->phase_array[cur_schedule->phase_idx]);

  NSDL2_RUNTIME(NULL, NULL, "Method Called, time_val = %u, now = %u", time_val, now);

  if(cur_phases->phase_status == PHASE_RUNNING) {
    if(cur_schedule->phase_end_tmr->timer_type > 0)  {
       /* Fix Bug#13013: where we can apply multiple phases RTC (like: increasing or decrease time on start, stablize 
		    and duration phase), then in this case phase should not be completed with its actual time.*/ 
       //adjust_time(cur_schedule->phase_end_tmr, now, &time_val);
       dis_timer_del(cur_schedule->phase_end_tmr);
    }
    time_val = time_val - (now - cur_phases->phase_start_time);
    NSDL2_RUNTIME(NULL, NULL, "time_val = %d", time_val);
    cur_schedule->phase_array[cur_schedule->phase_idx].phase_cmd.stabilize_phase.time = time_val;
    start_stabilize_phase(cur_schedule, now);
  } else { 
    NSDL2_RUNTIME(NULL, NULL, "Phase is not running.");
  }
}

static void runtime_start_duration_phase(Schedule *cur_schedule, u_ns_ts_t now) {
	
  int time_val = cur_schedule->phase_array[cur_schedule->phase_idx].phase_cmd.duration_phase.seconds;
  Phases *cur_phases = &(cur_schedule->phase_array[cur_schedule->phase_idx]);

  NSDL2_RUNTIME(NULL, NULL, "Method Called");

  if(cur_phases->phase_status == PHASE_RUNNING) {
    if(cur_schedule->phase_end_tmr->timer_type > 0)  {
       /* Fix Bug#13013: where we can apply multiple phases RTC (like: increasing or decrease time on start, stablize 
                    and duration phase), then in this case phase should not be completed with its actual time.*/ 
       //adjust_time(cur_schedule->phase_end_tmr, now, &time_val);
       dis_timer_del(cur_schedule->phase_end_tmr);
    }
    /*RTC_PAUSE opcode has already deleted phase_end_tmr*/
    time_val = time_val - (now - cur_phases->phase_start_time);
    NSDL2_RUNTIME(NULL, NULL, "time_val = %d", time_val);
    cur_schedule->phase_array[cur_schedule->phase_idx].phase_cmd.duration_phase.seconds = time_val;
    start_duration_phase(cur_schedule, now);
  } else { 
    NSDL2_RUNTIME(NULL, NULL, "Phase is not running.");
  }
}

static void runtime_start_ramp_down_phase(Schedule *cur_schedule, u_ns_ts_t now) {
	
  Phases *cur_phases = &(cur_schedule->phase_array[cur_schedule->phase_idx]);

  NSDL2_RUNTIME(NULL, NULL, "Method Called");

  /*AN-TODO Need check for FSR*/
  if(cur_phases->phase_status == PHASE_RUNNING) {
    NSDL2_RUNTIME(NULL, NULL, "Phase is running");
    if(cur_schedule->phase_end_tmr->timer_type > 0)
      dis_timer_del(cur_schedule->phase_end_tmr);
    if(cur_schedule->phase_ramp_tmr->timer_type > 0)
      dis_timer_del(cur_schedule->phase_ramp_tmr);

    start_ramp_down_phase(cur_schedule, now);
  } else { 
    NSDL2_RUNTIME(NULL, NULL, "Phase is not running.");
  }
}

static void runtime_phase_data_updation(Schedule *cur_schedule, u_ns_ts_t now) {
  NSDL2_RUNTIME(NULL, NULL, "now = %u", now); 
  Phases *cur_phase = &(cur_schedule->phase_array[cur_schedule->phase_idx]);
  NSDL2_RUNTIME(NULL, NULL, "Phase Type = %d", cur_phase->phase_type); 
  
NSDL2_RUNTIME(NULL, NULL, "Phase name = %s, Phase status = %d, runtime_flag = %d", 
                                cur_phase->phase_name, cur_phase->phase_status, cur_phase->runtime_flag); 

  // Return if phase is completed or run time flag is disable
  if(cur_phase->phase_status == PHASE_IS_COMPLETED || cur_phase->runtime_flag == 0) {
    return;
  }

  cur_phase->runtime_flag = 0; // Unmark this flag as we are going to apply changes

  switch(cur_phase->phase_type) {  
    case SCHEDULE_PHASE_START:
      runtime_start_start_phase(cur_schedule, now);
      break;
    case SCHEDULE_PHASE_RAMP_UP:
      gRunPhase = NS_RUN_PHASE_RAMP;
      runtime_start_ramp_up_phase(cur_schedule, now);
      break;
    case SCHEDULE_PHASE_STABILIZE:
       gRunPhase = NS_RUN_WARMUP;
       runtime_start_stabilize_phase(cur_schedule, now);
       break;
    case SCHEDULE_PHASE_DURATION:
       gRunPhase = NS_RUN_PHASE_EXECUTE;
       runtime_start_duration_phase(cur_schedule, now);
       break;
    case SCHEDULE_PHASE_RAMP_DOWN:
       gRunPhase = NS_RUN_PHASE_RAMP_DOWN;
       runtime_start_ramp_down_phase(cur_schedule, now);
       break;
   default:
       fprintf(stderr, "Invalid phase type (%d) recieved.", cur_phase->phase_type);
       return;
   }
}


//******************************************  RUNTIME CHANGE IN QUANTITY ************************************************8
/* Purpose
    Algo for searching appropriate RU phase:
    If current phase is ramp-up phase,
      add quantity in current phase
    Else search for closest RU phase which are done(in previous direction)
      If found, add quantity in that phase
    Else search for closest RU phase in forward direction
      If found, add quantity in that phase
    If ramp-up phase not found in the whole schedule, give error  
*/

int runtime_find_RU_phase_for_increase_in_qty(Schedule *cur_schedule, int quantity)
{
  int phase_id, cur_phase_id;
  int ramp_up_phase_id = -1; 

  NSDL2_RUNTIME(NULL, NULL, "Method called. quantity=%d", quantity);

  cur_phase_id = cur_schedule->phase_idx;
  //1.Searching for appropriate RU phase
  //  a.If current phase is RAMP_UP phase, add quantity in this phase
  if(cur_schedule->phase_array[cur_phase_id].phase_type == SCHEDULE_PHASE_RAMP_UP)
  {
    ramp_up_phase_id = cur_schedule->phase_idx;
    NSDL3_RUNTIME(NULL, NULL, "Current phase with phase_id = %d is ramp-up phase", ramp_up_phase_id);
  }
  else
  {
    //b.Moving in backward direction for searching the RAMP_UP phase
    for(phase_id = cur_schedule->phase_idx - 1; phase_id >= 0; phase_id--)
    {
      if(cur_schedule->phase_array[phase_id].phase_type == SCHEDULE_PHASE_RAMP_UP)
      {
        ramp_up_phase_id = phase_id;
        NSDL3_RUNTIME(NULL, NULL, "Ramp-up phase found at phase_id = %d moving in previous direction",
                            ramp_up_phase_id);
        break;
      }
    }
    //If RAMP-UP phase not found in backward direction
    if(ramp_up_phase_id == -1)
    {
      //c.Moving in forward direction for serching the RAMP_UP phase
      for(phase_id = cur_schedule->phase_idx + 1; phase_id < cur_schedule->num_phases; phase_id++)
      {
        if(cur_schedule->phase_array[phase_id].phase_type == SCHEDULE_PHASE_RAMP_UP)
        {
          ramp_up_phase_id = phase_id;
          NSDL3_RUNTIME(NULL, NULL, "Ramp-up phase found at phase_id = %d moving in forward direction",
                               ramp_up_phase_id);
          break;
        }
      }
    }
  }
  //If RAMP-UP phase not found in whole schedule
  if(ramp_up_phase_id == -1)
  {
    fprintf(stderr, "No Ramp-Up Phase Found in Schedule");
    return RUNTIME_ERROR;
  }
  return ramp_up_phase_id;
}

/*
   Algo for searching appropriate RD phase:
    Search for closest RD phase in forward direction
      If found, add quantity in that phase
    Else if current phase is RD phase
      add quantity in that phase
*/
int runtime_find_RD_phase_for_increase_in_qty(Schedule *cur_schedule, int quantity)
{
  int phase_id, cur_phase_id;
  int ramp_down_phase_id = -1; 

  NSDL2_RUNTIME(NULL, NULL, "Method called. quantity=%d", quantity);

  cur_phase_id = cur_schedule->phase_idx;

  //a. Moving in forward direction for searching the RAMP_DOWN phase
  for(phase_id = cur_schedule->phase_idx + 1; phase_id < cur_schedule->num_phases; phase_id++)
  {
    if(cur_schedule->phase_array[phase_id].phase_type == SCHEDULE_PHASE_RAMP_DOWN)
    {
      ramp_down_phase_id = phase_id;
      NSDL3_RUNTIME(NULL, NULL, "Ramp-down phase found at phase_id = %d moving in forward direction",
                               ramp_down_phase_id);
      //If Mode is 0. Add users in first RD phase in forward direction
      if(!global_settings->runtime_increase_quantity_mode)
        break;
    }
  }

  if(ramp_down_phase_id == -1)
    //b. If current phase is RAMP_DOWN phase, add quantity in this phase
    if(cur_schedule->phase_array[cur_phase_id].phase_type == SCHEDULE_PHASE_RAMP_DOWN)
    {
      ramp_down_phase_id = cur_schedule->phase_idx;
      NSDL3_RUNTIME(NULL, NULL, "Current phase with phase_id = %d is ramp-down phase", ramp_down_phase_id);
    }
  
  //If RAMP-DOWN phase not found in whole schedule
  if(ramp_down_phase_id == -1)
     NSDL3_RUNTIME(NULL, NULL, "No Ramp-Down Phase Found.");
  
  return ramp_down_phase_id;
}

void runtime_update_in_time_mode_FCU(Schedule *cur_schedule, int ramp_down_phase_id, 
                                    Ramp_down_schedule_phase *ramp_down_phase_ptr, u_ns_ts_t now)
{
  double rpc;
  u_ns_ts_t phase_time_passed = 0;

  NSDL3_SCHEDULE(NULL, NULL, "Method Called ramp_down_phase_id=%d", ramp_down_phase_id);
  //If current phase is ramp down phase, recalculating iid_mu from recalculated rpc in case of Time Pattern and 
  //updating actual_timeout in case of Linear pattern and ns_iid_handle in case of Random pattern
  if(ramp_down_phase_id == cur_schedule->phase_idx)
  {
    NSDL3_SCHEDULE(NULL, NULL, "Before RTC in RD phase: Rate of this NVM = %f, iid_mu = %u, phase_start_time=%u, ramp_down_time=%u", ramp_down_phase_ptr->rpc, cur_schedule->iid_mu, cur_schedule->phase_array[cur_schedule->phase_idx].phase_start_time, ramp_down_phase_ptr->ramp_down_time);

    NSDL3_SCHEDULE(NULL, NULL, "Phase Time: ramp_down_time=%u, now=%u, phase_start_time=%u",
      ramp_down_phase_ptr->ramp_down_time, now, cur_schedule->phase_array[ramp_down_phase_id].phase_start_time);

    if(cur_schedule->phase_array[ramp_down_phase_id].phase_start_time > 0)
      phase_time_passed = (now - cur_schedule->phase_array[ramp_down_phase_id].phase_start_time);

    u_ns_ts_t phase_time_left =  ramp_down_phase_ptr->ramp_down_time - phase_time_passed;
    int users_left = ramp_down_phase_ptr->num_vusers_or_sess - ramp_down_phase_ptr->max_ramp_down_vuser_or_sess;

    rpc = ramp_down_phase_ptr->rpc = (double)(users_left)/(double)((phase_time_left)/(1000.0*60));

    NSDL3_SCHEDULE(NULL, NULL, "users_left=%d, phase_start_time=%u, rpc=%f",
                        users_left , cur_schedule->phase_array[ramp_down_phase_id].phase_start_time, rpc);

    if(rpc <= 0) return; //Bug#4513
      
    if(ramp_down_phase_ptr->ramp_down_pattern == RAMP_DOWN_PATTERN_LINEAR)
    {
      CALC_IID_MU(rpc, cur_schedule);
      if(cur_schedule->iid_mu == 0)  return;
      cur_schedule->phase_ramp_tmr->actual_timeout = cur_schedule->iid_mu;

      NSDL3_SCHEDULE(NULL, NULL, "After RTC in RD phase: RD Mode - Time Linear, Rate of this NVM = %f, iid_mu = %u, phase_time_left=%u, users_left=%d, actual_timeout=%d", ramp_down_phase_ptr->rpc, cur_schedule->iid_mu, phase_time_left, users_left, cur_schedule->phase_ramp_tmr->actual_timeout); 
    }
    else if(ramp_down_phase_ptr->ramp_down_pattern == RAMP_DOWN_PATTERN_RANDOM)
    {
      CALC_IID_HANDLE(rpc, cur_schedule);
      ramp_down_phase_ptr->ramped_down_vusers = cur_schedule->ramping_delta;  
      NSDL3_SCHEDULE(NULL, NULL, "After RTC in RD phase: RD Mode - Time Random Mode, Rate of this NVM = %f, iid_mu = %u, phase_time_left=%u, users_left=%d, ns_iid_handle=%d, ramped_down_vusers=%d ", ramp_down_phase_ptr->rpc, cur_schedule->iid_mu, phase_time_left, users_left, cur_schedule->phase_ramp_tmr->actual_timeout, cur_schedule->ns_iid_handle, ramp_down_phase_ptr->ramped_down_vusers); 
    }
  }
  else
  {
    ramp_down_phase_ptr->rpc = (double)ramp_down_phase_ptr->num_vusers_or_sess /
       (double)(ramp_down_phase_ptr->ramp_down_time/(1000.0*60));
    NSDL3_SCHEDULE(NULL, NULL, "After RTC not in RD phase: rpc=%.03f", ramp_down_phase_ptr->rpc);
  }
}

void runtime_update_in_time_mode_FSR(Schedule *cur_schedule, int ramp_down_phase_id, 
                                    Ramp_down_schedule_phase *ramp_down_phase_ptr, u_ns_ts_t now)
{

    if(ramp_down_phase_id == cur_schedule->phase_idx)
    {
      double temp, sess_rpc;  //temporary variable used to calculate rounded value

      NSDL3_SCHEDULE(NULL, NULL, "Before RTC: Ramp Down Mode - Time Linear. \nglobal_settings->user_rate_mode=%d \
          \ntot_num_steps_for_sessions=%d \nnum_vusers_or_sess=%d \nmax_ramp_down_vuser_or_sess=%d \nsession_rate_per_step=%d \ncur_schedule->iid_mu=%f\nramp_down_time=%d", 
                global_settings->user_rate_mode, ramp_down_phase_ptr->tot_num_steps_for_sessions,  
                ramp_down_phase_ptr->num_vusers_or_sess, ramp_down_phase_ptr->max_ramp_down_vuser_or_sess,
                ramp_down_phase_ptr->session_rate_per_step, cur_schedule->iid_mu, ramp_down_phase_ptr->ramp_down_time); 

      //u_ns_ts_t phase_time_left =  ramp_down_phase_ptr->ramp_down_time - (now - cur_schedule->phase_array[cur_schedule->phase_idx].phase_start_time);

      //temp = (double)(ramp_down_phase_ptr->num_vusers_or_sess - ramp_down_phase_ptr->max_ramp_down_vuser_or_sess)/
      temp = (double)(ramp_down_phase_ptr->num_vusers_or_sess)/
                   (double)ramp_down_phase_ptr->tot_num_steps_for_sessions;
      //Need number of steps already passed - num_ramp_down_steps

      ramp_down_phase_ptr->session_rate_per_step = round(temp);
      sess_rpc = ramp_down_phase_ptr->session_rate_per_step/SESSION_RATE_MULTIPLE;

      if(global_settings->user_rate_mode == SESSION_RATE_CONSTANT) //Constt
      {
        if(sess_rpc < 100.0)
          cur_schedule->iid_mu = (int)((double)(60000)/sess_rpc);
        else
          cur_schedule->iid_mu =(double)calc_iid_ms_from_rpm(cur_schedule, (int)sess_rpc);
      }
      //ramp_down_step_time  = time remaining / no. of steps remaining
      //
      //schedule_ptr->phase_end_tmr->actual_timeout = ramp_down_phase_ptr->ramp_down_step_time*1000;
      //ramp_down_step_time = ramp_down_time/tot_num_steps_for_sessions
      
      NSDL3_SCHEDULE(NULL, NULL, "Before RTC: Ramp Down Mode - Time Linear. \nglobal_settings->user_rate_mode=%d \
          \ntot_num_steps_for_sessions=%d \nnum_vusers_or_sess=%d \nmax_ramp_down_vuser_or_sess=%d \nsession_rate_per_step=%d \nsess_rpc=%f \ncur_schedule->iid_mu=%f", 
                global_settings->user_rate_mode, ramp_down_phase_ptr->tot_num_steps_for_sessions,  
                ramp_down_phase_ptr->num_vusers_or_sess, ramp_down_phase_ptr->max_ramp_down_vuser_or_sess,
                ramp_down_phase_ptr->session_rate_per_step, sess_rpc, cur_schedule->iid_mu); 
    }
  }

//Called in case of SCHEDULE_BY_SCENARIO & FCU to adjust per_grp_qty
//quantity will be -ve in case of decrease users
//adj_cur_users - Adjust Schedule->cur_users

void runtime_adjust_grp_wise_qty(Schedule *schedule, int grp_idx, int phase_id, int quantity, short adj_cur_users)
{

  int *per_grp_qty;
  Phases *ph = &(schedule->phase_array[phase_id]);
  char phase_type[16];

  NSDL3_RUNTIME(NULL, NULL, "Method Called. grp=%d phase_id=%d, phase_type=%s, quantity=%d, adj_cur_users=%h", 
                                  grp_idx, phase_id, phase_type, quantity, adj_cur_users);

  switch(ph->phase_type) 
  {
    case SCHEDULE_PHASE_RAMP_UP:
      per_grp_qty = ph->phase_cmd.ramp_up_phase.per_grp_qty;
      strcpy(phase_type, "RAMP_UP");
      break;
    case SCHEDULE_PHASE_RAMP_DOWN:
      per_grp_qty = ph->phase_cmd.ramp_down_phase.per_grp_qty;
      strcpy(phase_type, "RAMP_DOWN");
      break;
  }

    NSDL3_RUNTIME(NULL, NULL, "Before RTC - Adjusting %d users in grp=%d phase_id=%d, phase_type=%s, per_grp_qty=%d, cur_users=%d", 
                               quantity, grp_idx, phase_id, phase_type, per_grp_qty[grp_idx], schedule->cur_users[grp_idx]);
    per_grp_qty[grp_idx] += quantity;
    if(adj_cur_users)
      schedule->cur_users[grp_idx] += quantity;

    NSDL3_RUNTIME(NULL, NULL, "After RTC - Adjusting %d users in grp=%d phase_id=%d, phase_type=%s, per_grp_qty=%d, cur_users=%d", 
                               quantity, grp_idx, phase_id, phase_type, per_grp_qty[grp_idx], schedule->cur_users[grp_idx]);
}

/*Purpose
  Update the change in quantity in appropriate ramp-up & ramp-down DS
  1.Searching for appropriate RU phase
  2.Search for appropriate RD phase
  3.Updating DS
     a. Schedule:
        cur_schedule->cur_vusers_or_sess
     b. RU phase -
        num_vusers_or_sess
        ramped_up_vusers
        max_ramp_up_vuser_or_sess
     c. RD phase - 
        num_vusers_or_sess
        per_grp_qty
*/
void update_main_schedule(Schedule *runtime_schedule)
{
  Schedule *cur_schedule;
  int ramp_up_phase_id = -1, ramp_down_phase_id = -1;
  Ramp_up_schedule_phase *ramp_up_phase_ptr;
  Ramp_down_schedule_phase *ramp_down_phase_ptr;
  int grp_idx = runtime_schedule->group_idx;
  int grp_type = get_group_mode(grp_idx);
  int quantity, phase_users_available_to_delete, phase_id, users_to_remove_from_this_phase;
  double sess_rpc;
  int group_not_started = 0;

  NSDL1_RUNTIME(NULL, NULL, "Method called. grp_idx=%d, phase_index=%d", 
                     runtime_schedule->group_idx, runtime_schedule->phase_idx); 

  if(global_settings->schedule_by == SCHEDULE_BY_SCENARIO) 
    cur_schedule = v_port_entry->scenario_schedule;
  else
  {
    cur_schedule = &(v_port_entry->group_schedule[runtime_schedule->group_idx]);
    NSDL2_RUNTIME(NULL, NULL, "Target group=%d not started yet", runtime_schedule->group_idx);
  }

  if((cur_schedule->phase_array[0].phase_type == SCHEDULE_PHASE_START) && (cur_schedule->phase_array[0].phase_status != PHASE_IS_COMPLETED))
  {
    group_not_started = 1;
  }

  if (runtime_schedule->phase_array[runtime_schedule->phase_idx].phase_type == SCHEDULE_PHASE_RAMP_UP) {
    if (grp_type == TC_FIX_USER_RATE)  
      quantity = runtime_schedule->phase_array[runtime_schedule->phase_idx].phase_cmd.ramp_up_phase.max_ramp_up_vuser_or_sess;
    else
      quantity = runtime_schedule->phase_array[runtime_schedule->phase_idx].phase_cmd.ramp_up_phase.ramped_up_vusers;

    if(group_not_started)
      quantity = runtime_schedule->runtime_qty_change[0];
    NSDL2_RUNTIME(NULL, NULL, "Quantity=%d", quantity); 
  
    //1.Searching for appropriate RU phase
    ramp_up_phase_id = runtime_find_RU_phase_for_increase_in_qty(cur_schedule, quantity); 
    ramp_up_phase_ptr = &cur_schedule->phase_array[ramp_up_phase_id].phase_cmd.ramp_up_phase;
    
    NSDL2_RUNTIME(NULL, NULL, "Before adding %d users - cur_vusers_or_sess=%d\nramp_up->num_vusers_or_sess=%d\n"
                 " ramped_up_vusers=%d\nmax_ramp_up_vuser_or_sess=%d", quantity,
                    cur_schedule->cur_vusers_or_sess, ramp_up_phase_ptr->num_vusers_or_sess,
                    ramp_up_phase_ptr->ramped_up_vusers, ramp_up_phase_ptr->max_ramp_up_vuser_or_sess);

    if(!group_not_started)
      cur_schedule->cur_vusers_or_sess += quantity;

    if(grp_type == TC_FIX_USER_RATE)
    {
      NSDL2_RUNTIME(NULL, NULL, "RUNTIME (Before): IID_MU =%d", cur_schedule->iid_mu);
      sess_rpc = cur_schedule->cur_vusers_or_sess/SESSION_RATE_MULTIPLE;
      if ( sess_rpc < 100.0)
       cur_schedule->iid_mu = (int)((double)(60000)/sess_rpc);
      else
       cur_schedule->iid_mu = calc_iid_ms_from_rpm(cur_schedule, (int)sess_rpc);
       NSDL2_RUNTIME(NULL, NULL, "RUNTIME (After): IID_MU =%d", cur_schedule->iid_mu);
      //FSR timers should be stopped as we have updated main schedule with runtime changes and recalculated session rate
      if(runtime_schedule->phase_end_tmr->timer_type > 0)
        dis_timer_del(runtime_schedule->phase_end_tmr);
      if(runtime_schedule->phase_ramp_tmr->timer_type > 0)
        dis_timer_del(runtime_schedule->phase_ramp_tmr);
    }
  
    //Updating RU phase:
    ramp_up_phase_ptr->num_vusers_or_sess += quantity;
    if(!group_not_started)
    {
      ramp_up_phase_ptr->max_ramp_up_vuser_or_sess += quantity;
      ramp_up_phase_ptr->ramped_up_vusers += quantity;
    }
    runprof_table_shr_mem[runtime_schedule->group_idx].quantity += quantity;
    if(group_not_started && (global_settings->schedule_by == SCHEDULE_BY_SCENARIO) && (grp_type == TC_FIX_CONCURRENT_USERS))
      runtime_adjust_grp_wise_qty(cur_schedule, runtime_schedule->group_idx, ramp_up_phase_id, quantity, 1);
   
     //2. Search for appropriate RD phase
    ramp_down_phase_id = runtime_find_RD_phase_for_increase_in_qty(cur_schedule, quantity);
    if(ramp_down_phase_id != -1){
      ramp_down_phase_ptr = &cur_schedule->phase_array[ramp_down_phase_id].phase_cmd.ramp_down_phase;
      NSDL2_RUNTIME(NULL, NULL, "Before adding ramp_down->num_vusers_or_sess=%d", ramp_down_phase_ptr->num_vusers_or_sess);
      //Updating RD phase:
      ramp_down_phase_ptr->num_vusers_or_sess += quantity;

      //Recalculating rpc, iid_mu, actual_timeout in case of TIME mode
      if(ramp_down_phase_ptr->ramp_down_mode == RAMP_DOWN_MODE_TIME)
        runtime_update_in_time_mode_FCU(cur_schedule, ramp_down_phase_id, ramp_down_phase_ptr, get_ms_stamp());

      if(global_settings->schedule_by == SCHEDULE_BY_SCENARIO && grp_type == TC_FIX_CONCURRENT_USERS) //for scenario based schedule and FCU
        runtime_adjust_grp_wise_qty(cur_schedule, runtime_schedule->group_idx, ramp_down_phase_id, quantity, 1);

      if(ramp_down_phase_ptr->ramp_down_mode == RAMP_DOWN_MODE_TIME_SESSIONS)
        runtime_update_in_time_mode_FSR(cur_schedule, ramp_down_phase_id, ramp_down_phase_ptr, get_ms_stamp());

      NSDL2_RUNTIME(NULL, NULL, "After adding ramp_down->num_vusers_or_sess=%d", ramp_down_phase_ptr->num_vusers_or_sess);
    }
    else{
      NSTL1(NULL, NULL, "No Ramp-Up Phase Found in Current Schedule");
    }

  } 
  else //Ramp Down
  {
    int users_left_to_remove;
    if(grp_type == TC_FIX_CONCURRENT_USERS) 
    {
      /* Bug#13373: When we apply RTC to remove users in time mode then in this case NVM's were getting stuck in search_and_remove user
                    function. Now we have change the variable with max_ramp_down_vuser_or_sess instead of ramped_down_vusers.
       */  
      users_left_to_remove = runtime_schedule->phase_array[runtime_schedule->phase_idx].phase_cmd.ramp_down_phase.max_ramp_down_vuser_or_sess;
    }
    else
      users_left_to_remove = runtime_schedule->phase_array[runtime_schedule->phase_idx].phase_cmd.ramp_down_phase.max_ramp_down_vuser_or_sess;
   
    if(group_not_started)
      users_left_to_remove = -runtime_schedule->runtime_qty_change[REMOVE_NOT_RAMPED_UP_VUSERS];
    NSDL2_RUNTIME(NULL, NULL, "Need to remove user/session=%d, users_left_to_remove = %d", 
                  runtime_schedule->phase_array[runtime_schedule->phase_idx].phase_cmd.ramp_down_phase.ramped_down_vusers, users_left_to_remove);
    for(phase_id = cur_schedule->phase_idx; phase_id < cur_schedule->num_phases; phase_id++)
    {
      NSDL2_RUNTIME(NULL, NULL, "Phase=%d. phase_type=%d", phase_id, cur_schedule->phase_array[phase_id].phase_type);
      if(cur_schedule->phase_array[phase_id].phase_type == SCHEDULE_PHASE_RAMP_DOWN)
      {
        ramp_down_phase_ptr = &cur_schedule->phase_array[phase_id].phase_cmd.ramp_down_phase;
        NSDL2_RUNTIME(NULL, NULL, "ramp_down_phase_ptr=%p", ramp_down_phase_ptr);
         //Calculate how many can be removed from this phase
        if(global_settings->schedule_by == SCHEDULE_BY_SCENARIO)
        {
          if(grp_type == TC_FIX_CONCURRENT_USERS)
            phase_users_available_to_delete = ramp_down_phase_ptr->per_grp_qty[grp_idx]; //for scenario based schedule and FCU
          else
            //FSR Scenario (grp_ratio)
            phase_users_available_to_delete = (ramp_down_phase_ptr->num_vusers_or_sess -
                                                      ramp_down_phase_ptr->max_ramp_down_vuser_or_sess)
                                                      * get_fsr_grp_ratio(cur_schedule, grp_idx);
        }
        else  //GROUP
          phase_users_available_to_delete = ramp_down_phase_ptr->num_vusers_or_sess - ramp_down_phase_ptr->max_ramp_down_vuser_or_sess;
      
        NSDL2_RUNTIME(NULL, NULL, "phase_users_available_to_delete = %d, users_left_to_remove = %d", phase_users_available_to_delete, users_left_to_remove); 
        //Calculate minimum from users left to remove & phase users available to delete
        users_to_remove_from_this_phase = ((users_left_to_remove < phase_users_available_to_delete)?users_left_to_remove:phase_users_available_to_delete);
        users_left_to_remove -= users_to_remove_from_this_phase;
        NSDL2_RUNTIME(NULL, NULL, "users_left_to_remove=%d, users_to_remove_from_this_phase=%d, max_ramp_down_vuser_or_sess = %d, num_vusers_or_sess = %d", users_left_to_remove, users_to_remove_from_this_phase, ramp_down_phase_ptr->max_ramp_down_vuser_or_sess, ramp_down_phase_ptr->num_vusers_or_sess );

        if(phase_id == cur_schedule->phase_idx) //Current phase is RD phase
        {
          /* Fix Bug#13422: When we apply RTC (Removing VUsers in RD phase in time mode), Vusers was stucked in NS and NC both 
                            after getting phase completion message.
                            Fix: Commented below two lines. */
 
          //ramp_down_phase_ptr->max_ramp_down_vuser_or_sess += users_to_remove_from_this_phase;
          if (grp_type == TC_FIX_CONCURRENT_USERS) { }
            //ramp_down_phase_ptr->ramped_down_vusers += users_to_remove_from_this_phase;
        } else {
          if (grp_type == TC_FIX_CONCURRENT_USERS)
            if(!group_not_started)
              cur_schedule->cur_vusers_or_sess -= users_to_remove_from_this_phase;
        }
        ramp_down_phase_ptr->num_vusers_or_sess -= users_to_remove_from_this_phase;
        //Recalculating rpc in case of TIME mode
        if(ramp_down_phase_ptr->ramp_down_mode == RAMP_DOWN_MODE_TIME && grp_type == TC_FIX_CONCURRENT_USERS)
          runtime_update_in_time_mode_FCU(cur_schedule, phase_id, ramp_down_phase_ptr, get_ms_stamp());
        else if(ramp_down_phase_ptr->ramp_down_mode == RAMP_DOWN_MODE_TIME_SESSIONS && grp_type == TC_FIX_USER_RATE)
          runtime_update_in_time_mode_FSR(cur_schedule, phase_id, ramp_down_phase_ptr, get_ms_stamp());
         NSDL2_RUNTIME(NULL, NULL, "ramp_down_phase_ptr->num_vusers_or_sess=%d, cur_schedule->cur_vusers_or_sess=%d", 
                       ramp_down_phase_ptr->num_vusers_or_sess, cur_schedule->cur_vusers_or_sess);

        runprof_table_shr_mem[grp_idx].quantity -= users_to_remove_from_this_phase;

        if(global_settings->schedule_by == SCHEDULE_BY_SCENARIO && grp_type == TC_FIX_CONCURRENT_USERS)  //for scenario based schedule and FCU
        {
          runtime_adjust_grp_wise_qty(cur_schedule, grp_idx, phase_id, -users_to_remove_from_this_phase, 1);
        }
        else if(grp_type == TC_FIX_USER_RATE)
        {
          if(!group_not_started)
          {
            cur_schedule->cur_vusers_or_sess -= users_to_remove_from_this_phase;
            sess_rpc = cur_schedule->cur_vusers_or_sess/SESSION_RATE_MULTIPLE;
            NSDL2_RUNTIME(NULL, NULL, "Resetting runtime rate interval timings");
            init_rate_intervals(cur_schedule);
            if ( sess_rpc < 100.0)
              cur_schedule->iid_mu = (int)((double)(60000)/sess_rpc);
            else
              cur_schedule->iid_mu = calc_iid_ms_from_rpm(cur_schedule, (int)sess_rpc);
          }
        }
        NSDL2_RUNTIME(NULL, NULL, "After RTC(RD phase)- \nramp_down_phase_ptr->num_vusers_or_sess=%d\nramp_down_phase_ptr->ramped_down_vusers=%d\nramp_down_phase_ptr->max_ramp_down_vuser_or_sess=%d\nphase_users_available_to_delete=%d\nusers_to_remove=%d\nusers_left_to_remove=%d\nrunprof_table_shr_mem[%d].quantity=%d",
        ramp_down_phase_ptr->num_vusers_or_sess, ramp_down_phase_ptr->ramped_down_vusers, ramp_down_phase_ptr->max_ramp_down_vuser_or_sess, phase_users_available_to_delete, users_to_remove_from_this_phase, users_left_to_remove, ramp_down_phase_ptr->rpc, grp_idx, runprof_table_shr_mem[grp_idx].quantity);
        if (ramp_down_phase_ptr->num_vusers_or_sess == 0) {
          send_phase_complete(cur_schedule); 
        }     
        if (users_left_to_remove == 0) break;
      }
    } 
  }
}

/*Update require fields in runtime phase*/
static void runtime_add_users(Schedule *cur_schedule, Schedule *runtime_schedule, int grp_idx, int quantity, int group_not_started, u_ns_ts_t now)
{
  Phases *ph_tmp;
  int grp_type = get_group_mode(cur_schedule->group_idx);
  double sess_rpc; 
  NSDL2_RUNTIME(NULL, NULL, "Method called. grp_idx=%d, quantity=%d, group_not_started=%d, phase_idx=%d", 
                          grp_idx, quantity, group_not_started, runtime_schedule->phase_idx); 
  if(quantity == 0) return;

  //Not starting users in case group is not started
  if(group_not_started == 0)
  {
    runtime_schedule->cur_vusers_or_sess = 0; //Do delete users while deleting users
    ph_tmp = &runtime_schedule->phase_array[runtime_schedule->phase_idx];
    ph_tmp->phase_type = SCHEDULE_PHASE_RAMP_UP; 
    ph_tmp->phase_status = PHASE_RUNNING;
    Ramp_up_schedule_phase *ramp_up_rtc_phase_ptr = &runtime_schedule->phase_array[runtime_schedule->phase_idx].phase_cmd.ramp_up_phase;
    ramp_up_rtc_phase_ptr->ramped_up_vusers = 0;
    ramp_up_rtc_phase_ptr->max_ramp_up_vuser_or_sess = 0; 
    int tot_users = ramp_up_rtc_phase_ptr->num_vusers_or_sess;
    ramp_up_rtc_phase_ptr->num_vusers_or_sess = quantity; 
    NSDL2_RUNTIME(NULL, NULL, "Total users=%d, num_vusers_or_sess=%d, ramp_up_time = %d", 
                   tot_users, ramp_up_rtc_phase_ptr->num_vusers_or_sess, ramp_up_rtc_phase_ptr->ramp_up_time);
    if(grp_type == TC_FIX_CONCURRENT_USERS) 
    {
      if(ramp_up_rtc_phase_ptr->ramp_up_mode == RAMP_UP_MODE_RATE) 
        ramp_up_rtc_phase_ptr->rpc = ((((double) ramp_up_rtc_phase_ptr->ramp_up_rate * (double) ramp_up_rtc_phase_ptr->num_vusers_or_sess) / (double) tot_users));
      else if(ramp_up_rtc_phase_ptr->ramp_up_mode == RAMP_UP_MODE_TIME) 
        ramp_up_rtc_phase_ptr->rpc = (double)ramp_up_rtc_phase_ptr->num_vusers_or_sess / (double)(ramp_up_rtc_phase_ptr->ramp_up_time/(1000.0*60));        
    }
    if(grp_type == TC_FIX_USER_RATE)
    {
      sess_rpc = quantity/SESSION_RATE_MULTIPLE;
      if ( sess_rpc < 100.0)
        runtime_schedule->iid_mu = (int)((double)(60000)/sess_rpc);
      else
        runtime_schedule->iid_mu = calc_iid_ms_from_rpm(runtime_schedule, (int)sess_rpc);
      NSDL2_RUNTIME(NULL, NULL, "IID_MU=%d", runtime_schedule->iid_mu);
    }

    NSDL2_RUNTIME(NULL, NULL, "Adding users as per mode group is running");
    NSDL2_RUNTIME(NULL, NULL, "New Phase: ph_tmp = %p, cur_vusers_or_sess=%d, ramp_up->num_vusers_or_sess=%d, ramped_up_vusers=%d," 
                        " max_ramp_up_vuser_or_sess=%d, rpc=%d", ph_tmp, runtime_schedule->cur_vusers_or_sess, 
                        ramp_up_rtc_phase_ptr->num_vusers_or_sess, ramp_up_rtc_phase_ptr->ramped_up_vusers, 
                        ramp_up_rtc_phase_ptr->max_ramp_up_vuser_or_sess, ramp_up_rtc_phase_ptr->rpc); 
    runtime_phase_data_updation(runtime_schedule, now);
    //In case of FSR, users get generated within ramp up code rather in ramped_users() therefore we need to set state for FCU cases
    if(grp_type == TC_FIX_CONCURRENT_USERS)
      runtime_schedule->rtc_state = RTC_RUNNING;//Setting the state to running
  }
  else
  {
    runtime_schedule->phase_array[runtime_schedule->phase_idx].phase_type = SCHEDULE_PHASE_RAMP_UP; 
    Ramp_up_schedule_phase *ramp_up_rtc_phase_ptr = &runtime_schedule->phase_array[runtime_schedule->phase_idx].phase_cmd.ramp_up_phase;
    ramp_up_rtc_phase_ptr->ramped_up_vusers = 0;
    ramp_up_rtc_phase_ptr->max_ramp_up_vuser_or_sess = quantity;
    send_phase_complete(runtime_schedule);
  }

  NSDL2_RUNTIME(NULL, NULL, "Exiting Method");
  return;
}


int runtime_delete_users_not_ramped_up(Schedule *cur_schedule, Schedule *runtime_schedule, int grp_idx, int quantity, u_ns_ts_t now)
{
  int phase_id; 
  int users_to_remove_from_this_phase = 0;  //Min of users left to remove from this phase & users left to delete
  int phase_users_available_to_delete = 0;  //Users left to remove from this phase
  int users_left_to_remove;  //Users left to be adjusted for delete
  int users_to_adjust;    //Users to be adjusted for in RD phase
  Ramp_down_schedule_phase *ramp_down_phase_ptr;
  Ramp_up_schedule_phase *ramp_up_phase_ptr;
  int grp_type = get_group_mode(grp_idx);
  double rpc;
  u_ns_ts_t phase_time_passed = 0;

  users_left_to_remove = quantity;
  NSDL2_RUNTIME(NULL, NULL, "Method called. grp_idx=%d, quantity=%d", grp_idx, quantity); 
  if(quantity == 0)
  {
    NSDL2_RUNTIME(NULL, NULL, "No Users/Sessions to remove");
    return users_left_to_remove;
  }

  //Moving in forward direction starting with current phase searching for RAMP_UP phases
  for(phase_id = cur_schedule->phase_idx; phase_id < cur_schedule->num_phases; phase_id++)
  {
    NSDL2_RUNTIME(NULL, NULL, "Phase=%d. phase_type=%d", phase_id, cur_schedule->phase_array[phase_id].phase_type);
    //schedule not started then phase index will be negative
    if(phase_id < 0)
      continue;
    if(cur_schedule->phase_array[phase_id].phase_type == SCHEDULE_PHASE_RAMP_UP)
    {
      ramp_up_phase_ptr = &cur_schedule->phase_array[phase_id].phase_cmd.ramp_up_phase;
      NSDL3_RUNTIME(NULL, NULL, "Ramp-up phase found at phase_id = %d moving in forward direction", phase_id);

      NSDL2_RUNTIME(NULL, NULL, "Before RTC (RU phase)- \nramp_up_phase_ptr->num_vusers_or_sess=%d\nramp_up_phase_ptr->ramped_up_vusers=%d\nramp_up_phase_ptr->max_ramp_up_vuser_or_sess=%d\nphase_users_available_to_delete=%d\nusers_to_remove=%d\nusers_left_to_remove=%d",
      ramp_up_phase_ptr->num_vusers_or_sess, ramp_up_phase_ptr->ramped_up_vusers, ramp_up_phase_ptr->max_ramp_up_vuser_or_sess, phase_users_available_to_delete, users_to_remove_from_this_phase, users_left_to_remove); 

      //Calculate how many can be removed from this phase(how many are left to ramp-up)
      if(grp_type == TC_FIX_CONCURRENT_USERS) //for scenario based schedule and FCU
      {
        if(global_settings->schedule_by == SCHEDULE_BY_SCENARIO)
          phase_users_available_to_delete =  ramp_up_phase_ptr->per_grp_qty[grp_idx];
        else
          phase_users_available_to_delete = ramp_up_phase_ptr->num_vusers_or_sess - ramp_up_phase_ptr->ramped_up_vusers;
      }
      else  //FSR
      {
        if(global_settings->schedule_by == SCHEDULE_BY_SCENARIO)
          phase_users_available_to_delete = (ramp_up_phase_ptr->num_vusers_or_sess - 
                                                    ramp_up_phase_ptr->max_ramp_up_vuser_or_sess)
                                                    * get_fsr_grp_ratio(cur_schedule, grp_idx);      
        else
          phase_users_available_to_delete = ramp_up_phase_ptr->num_vusers_or_sess - ramp_up_phase_ptr->max_ramp_up_vuser_or_sess;
      }

      NSDL3_RUNTIME(NULL, NULL, "phase_users_available_to_delete=%d", phase_users_available_to_delete);
      //Calculate how many to remove from this phase based on min(phase_users_available_to_delete,users_left_to_remove)
      users_to_remove_from_this_phase = ((users_left_to_remove < phase_users_available_to_delete)?users_left_to_remove:phase_users_available_to_delete);

      users_left_to_remove -= users_to_remove_from_this_phase;

      NSDL3_RUNTIME(NULL, NULL, "users_left_to_remove=%d", users_left_to_remove);
      //Updating DS
      ramp_up_phase_ptr->num_vusers_or_sess -= users_to_remove_from_this_phase;

      //Reducing per_grp_qty 
      if(global_settings->schedule_by == SCHEDULE_BY_SCENARIO && grp_type == TC_FIX_CONCURRENT_USERS)  //for scenario based schedule and FCU
        runtime_adjust_grp_wise_qty(cur_schedule, grp_idx, phase_id, -users_to_remove_from_this_phase, 0);

      //Recalculating rpc in case of TIME mode
      if(ramp_up_phase_ptr->ramp_up_mode == RAMP_UP_MODE_TIME)
      {
        NSDL3_SCHEDULE(NULL, NULL, "Phase Time: ramp_up_time=%u, now=%u, phase_start_time=%u", 
                ramp_up_phase_ptr->ramp_up_time, now, cur_schedule->phase_array[phase_id].phase_start_time);

        if(cur_schedule->phase_array[phase_id].phase_start_time > 0)
          phase_time_passed = (now - cur_schedule->phase_array[phase_id].phase_start_time);
          
        u_ns_ts_t phase_time_left =  ramp_up_phase_ptr->ramp_up_time - phase_time_passed;

        int users_left = ramp_up_phase_ptr->num_vusers_or_sess - ramp_up_phase_ptr->max_ramp_up_vuser_or_sess;
        NSDL3_SCHEDULE(NULL, NULL, "Before RTC(RU phase): RU Mode - Time Mode, Rate of this NVM = %f, iid_mu = %u, phase_time_left=%u, users_left=%d, ns_iid_handle=%d, ramped_up_vusers=%d ", ramp_up_phase_ptr->rpc, cur_schedule->iid_mu, phase_time_left, users_left, cur_schedule->phase_ramp_tmr->actual_timeout, cur_schedule->ns_iid_handle, ramp_up_phase_ptr->ramped_up_vusers);
    
        rpc = (double)(users_left)/(double)((phase_time_left)/(1000.0*60));

        NSDL3_RUNTIME(NULL, NULL, "New rpc=%f", rpc);
        if(ramp_up_phase_ptr->ramp_up_pattern == RAMP_UP_PATTERN_LINEAR)
        {
          ramp_up_phase_ptr->rpc = rpc;
          CALC_IID_MU(rpc, cur_schedule);
          if(cur_schedule->iid_mu == 0)  return 0;
          cur_schedule->phase_ramp_tmr->actual_timeout = cur_schedule->iid_mu;

          NSDL3_SCHEDULE(NULL, NULL, "After RTC(RU phase): RU Mode - Time Linear, Rate of this NVM = %f, iid_mu = %u, phase_time_left=%u, users_left=%d, actual_timeout=%d", ramp_up_phase_ptr->rpc, cur_schedule->iid_mu, phase_time_left, users_left, cur_schedule->phase_ramp_tmr->actual_timeout);
        }
        else if(ramp_up_phase_ptr->ramp_up_pattern == RAMP_UP_PATTERN_RANDOM)
        {
          NSDL3_RUNTIME(NULL, NULL, "RAMP_UP_PATTERN_RANDOM");
          ramp_up_phase_ptr->rpc = rpc;
          if(rpc)
            CALC_IID_HANDLE(rpc, cur_schedule);
          NSDL3_SCHEDULE(NULL, NULL, "After RTC(RU phase): RU Mode - Time Random Mode, Rate of this NVM = %f, iid_mu = %u, phase_time_left=%u, users_left=%d, ns_iid_handle=%d, ramped_up_vusers=%d ", ramp_up_phase_ptr->rpc, cur_schedule->iid_mu, phase_time_left, users_left, cur_schedule->phase_ramp_tmr->actual_timeout, cur_schedule->ns_iid_handle, ramp_up_phase_ptr->ramped_up_vusers);
        }
      }

      NSDL2_RUNTIME(NULL, NULL, "After RTC(RU phase)- \nramp_up_phase_ptr->num_vusers_or_sess=%d\nramp_up_phase_ptr->ramped_up_vusers=%d\nramp_up_phase_ptr->max_ramp_up_vuser_or_sess=%d\nphase_users_available_to_delete=%d\nusers_to_remove=%d\nusers_left_to_remove=%d",
      ramp_up_phase_ptr->num_vusers_or_sess, ramp_up_phase_ptr->ramped_up_vusers, ramp_up_phase_ptr->max_ramp_up_vuser_or_sess, phase_users_available_to_delete, users_to_remove_from_this_phase, users_left_to_remove); 

      if (users_left_to_remove == 0) break;
    }
  }

  //If RU phase not found in FW direction, return users_left_to_remove
  if (users_left_to_remove == quantity)
    return users_left_to_remove;
  else
     users_to_adjust = quantity - users_left_to_remove;
 
  //Moving in forward direction starting with current phase searching for RAMP_DOWN phases
  for(phase_id = cur_schedule->phase_idx; phase_id < cur_schedule->num_phases; phase_id++)
  {
    if(phase_id < 0)
      continue;
    if(cur_schedule->phase_array[phase_id].phase_type == SCHEDULE_PHASE_RAMP_DOWN)
    {
      phase_users_available_to_delete = 0;
      ramp_down_phase_ptr = &cur_schedule->phase_array[phase_id].phase_cmd.ramp_down_phase;
      NSDL3_RUNTIME(NULL, NULL, "Ramp-down phase found at phase_id = %d moving in forward direction", phase_id);
      NSDL2_RUNTIME(NULL, NULL, "Before RTC(RD phase)- ramp_down_phase_ptr->num_vusers_or_sess=%d\nramp_down_phase_ptr->ramped_down_vusers=%d\nramp_down_phase_ptr->max_ramp_down_vuser_or_sess=%d\nphase_users_available_to_delete=%d\nusers_to_remove=%d\nusers_to_adjust=%d\n",
      ramp_down_phase_ptr->num_vusers_or_sess, ramp_down_phase_ptr->ramped_down_vusers, ramp_down_phase_ptr->max_ramp_down_vuser_or_sess, phase_users_available_to_delete, users_to_remove_from_this_phase, users_to_adjust); 

      //Calculate how many can be removed from this phase(how many are left to ramp-down)
      //phase_users_available_to_delete = ramp_down_phase_ptr->num_vusers_or_sess - ramp_down_phase_ptr->ramped_down_vusers;
      if(global_settings->schedule_by == SCHEDULE_BY_SCENARIO && grp_type == TC_FIX_CONCURRENT_USERS)  //for scenario based schedule and FCU
        phase_users_available_to_delete =  ramp_down_phase_ptr->per_grp_qty[grp_idx];
      else
        phase_users_available_to_delete = ramp_down_phase_ptr->num_vusers_or_sess - ramp_down_phase_ptr->max_ramp_down_vuser_or_sess;

      //Calculate how many to remove from this phase based on min(phase_users_available_to_delete, users_to_adjust)
      users_to_remove_from_this_phase = ((users_to_adjust < phase_users_available_to_delete)?users_to_adjust:phase_users_available_to_delete);

      users_to_adjust -= users_to_remove_from_this_phase;
      ramp_down_phase_ptr->num_vusers_or_sess -= users_to_remove_from_this_phase;

      if(global_settings->schedule_by == SCHEDULE_BY_SCENARIO && grp_type == TC_FIX_CONCURRENT_USERS) //for scenario based schedule and FCU
        runtime_adjust_grp_wise_qty(cur_schedule, grp_idx, phase_id, -users_to_remove_from_this_phase, 0);

      //Recalculating rpc in case of TIME mode
      if(ramp_down_phase_ptr->ramp_down_mode == RAMP_DOWN_MODE_TIME)
        runtime_update_in_time_mode_FCU(cur_schedule, phase_id, ramp_down_phase_ptr, now);

      NSDL2_RUNTIME(NULL, NULL, "After RTC(RD phase)- \nramp_down_phase_ptr->num_vusers_or_sess=%d\nramp_down_phase_ptr->ramped_down_vusers=%d\nramp_down_phase_ptr->max_ramp_down_vuser_or_sess=%d\nphase_users_available_to_delete=%d\nusers_to_remove=%d\nusers_to_adjust=%d",
      ramp_down_phase_ptr->num_vusers_or_sess, ramp_down_phase_ptr->ramped_down_vusers, ramp_down_phase_ptr->max_ramp_down_vuser_or_sess, phase_users_available_to_delete, users_to_remove_from_this_phase, users_to_adjust); 

      if (users_to_adjust == 0) break;
    }
  }
  return users_left_to_remove;
}

//cum_users_to_remove    cummulative of users from all the phases left to remove. Max is total users existing in the system
int runtime_delete_ramped_up_vusers(Schedule *cur_schedule, Schedule *runtime_schedule, int grp_idx, int quantity, int *cum_users_to_remove, u_ns_ts_t now)
{
  int phase_id; 
  int users_to_remove_from_this_phase = 0;  //Min of users left to remove from this phase & users left to delete
  int phase_users_available_to_delete = 0;  //Users left to remove from this phase
  int users_left_to_remove;  //Users left to be adjusted for delete
  Ramp_down_schedule_phase *ramp_down_phase_ptr;
  Ramp_down_schedule_phase *rtc_ramp_down_phase_ptr = &runtime_schedule->phase_array[runtime_schedule->phase_idx].phase_cmd.ramp_down_phase;
  int grp_type = get_group_mode(grp_idx);
  double sess_rpc;

  NSDL2_RUNTIME(NULL, NULL, "Method called. grp_idx=%d, quantity=%d", grp_idx, quantity);
  users_left_to_remove = quantity;
  if(quantity == 0)
  {
    NSDL2_RUNTIME(NULL, NULL, "No Users/Sessions to remove");
    return users_left_to_remove;
  }
  NSDL2_RUNTIME(NULL, NULL, "cur_schedule->phase_idx=%d, cur_schedule->num_phases=%d", cur_schedule->phase_idx, cur_schedule->num_phases);

  //Moving in forward direction starting with current phase searching for RAMP_DOWN phases
  for(phase_id = cur_schedule->phase_idx; phase_id < cur_schedule->num_phases; phase_id++)
  {
    NSDL2_RUNTIME(NULL, NULL, "Phase=%d. phase_type=%d", phase_id, cur_schedule->phase_array[phase_id].phase_type);

    if(cur_schedule->phase_array[phase_id].phase_type == SCHEDULE_PHASE_RAMP_DOWN)
    {
      ramp_down_phase_ptr = &cur_schedule->phase_array[phase_id].phase_cmd.ramp_down_phase;

      NSDL3_RUNTIME(NULL, NULL, "Ramp-down phase found at phase_id = %d moving in forward direction", phase_id);
      NSDL2_RUNTIME(NULL, NULL, "Before RTC(RD phase)- \nramp_down_phase_ptr->num_vusers_or_sess=%d\nramp_down_phase_ptr->ramped_down_vusers=%d\nramp_down_phase_ptr->max_ramp_down_vuser_or_sess=%d\nphase_users_available_to_delete=%d\nusers_to_remove=%d, cum_users_to_remove=%d\nusers_left_to_remove=%d\nrunprof_table_shr_mem[%d].quantity=%d",
      ramp_down_phase_ptr->num_vusers_or_sess, ramp_down_phase_ptr->ramped_down_vusers, ramp_down_phase_ptr->max_ramp_down_vuser_or_sess, phase_users_available_to_delete, users_to_remove_from_this_phase, *cum_users_to_remove, users_left_to_remove, grp_idx, runprof_table_shr_mem[grp_idx].quantity); 

      //Calculate how many can be removed from this phase
      if(global_settings->schedule_by == SCHEDULE_BY_SCENARIO)        
      {
        if(grp_type == TC_FIX_CONCURRENT_USERS)
          phase_users_available_to_delete = ramp_down_phase_ptr->per_grp_qty[grp_idx]; //for scenario based schedule and FCU
        else
          //FSR Scenario (grp_ratio)
          phase_users_available_to_delete = (ramp_down_phase_ptr->num_vusers_or_sess - 
                                                    ramp_down_phase_ptr->max_ramp_down_vuser_or_sess) 
                                                    * get_fsr_grp_ratio(cur_schedule, grp_idx);      
      }
      else  //GROUP
        phase_users_available_to_delete = ramp_down_phase_ptr->num_vusers_or_sess - ramp_down_phase_ptr->max_ramp_down_vuser_or_sess;

      //Calculate minimum from users left to remove & phase users available to delete
      users_to_remove_from_this_phase = ((users_left_to_remove < phase_users_available_to_delete)?users_left_to_remove:phase_users_available_to_delete);
      
      /*Bug 30708:get_fsr_grp_ratio should be calculated according to updated num_vusers_or_sess and cur_schedule->cumulative_runprof_table[]       values 
      if(global_settings->schedule_by == SCHEDULE_BY_SCENARIO && grp_type == TC_FIX_USER_RATE){
        for(i = grp_idx; i < total_runprof_entries; i++)
          cur_schedule->cumulative_runprof_table[i] -= users_to_remove_from_this_phase;
      }
      */

      NSDL4_RUNTIME(NULL, NULL, "phase_users_available_to_delete=%d, users_to_remove_from_this_phase=%d", phase_users_available_to_delete, users_to_remove_from_this_phase);

      *cum_users_to_remove += users_to_remove_from_this_phase;
      users_left_to_remove -= users_to_remove_from_this_phase;
      //Reuse Old code for immediate mode
      if (rtc_ramp_down_phase_ptr->ramp_down_mode == RAMP_DOWN_MODE_IMMEDIATE)
      {
        if(phase_id == cur_schedule->phase_idx) //Current phase is RD phase
        {
          ramp_down_phase_ptr->max_ramp_down_vuser_or_sess += users_to_remove_from_this_phase;
          if (grp_type == TC_FIX_CONCURRENT_USERS)
            ramp_down_phase_ptr->ramped_down_vusers += users_to_remove_from_this_phase;
        } else {
          if (grp_type == TC_FIX_CONCURRENT_USERS)
            ramp_down_phase_ptr->num_vusers_or_sess -= users_to_remove_from_this_phase;
        }
        if (grp_type == TC_FIX_USER_RATE)
          ramp_down_phase_ptr->num_vusers_or_sess -= users_to_remove_from_this_phase;
        //Recalculating rpc in case of TIME mode
        if(ramp_down_phase_ptr->ramp_down_mode == RAMP_DOWN_MODE_TIME && grp_type == TC_FIX_CONCURRENT_USERS)
          runtime_update_in_time_mode_FCU(cur_schedule, phase_id, ramp_down_phase_ptr, now);
        else if(ramp_down_phase_ptr->ramp_down_mode == RAMP_DOWN_MODE_TIME_SESSIONS && grp_type == TC_FIX_USER_RATE)
          runtime_update_in_time_mode_FSR(cur_schedule, phase_id, ramp_down_phase_ptr, now);

        runprof_table_shr_mem[grp_idx].quantity -= users_to_remove_from_this_phase;
        //We are adding quantity in ramp_down->per_grp_qty only as it contains users to be ramped down
        //Not adding it in ramp_up->per_grp_qty as it contains users to be ramped-up, since those users
        //are already ramped up, we'll not increment it
        if(global_settings->schedule_by == SCHEDULE_BY_SCENARIO && grp_type == TC_FIX_CONCURRENT_USERS)  //for scenario based schedule and FCU
        {
          runtime_adjust_grp_wise_qty(cur_schedule, grp_idx, phase_id, -users_to_remove_from_this_phase, 1);
        }
        else if(grp_type == TC_FIX_USER_RATE)
        {
          cur_schedule->cur_vusers_or_sess -= users_to_remove_from_this_phase;
          sess_rpc = cur_schedule->cur_vusers_or_sess/SESSION_RATE_MULTIPLE;
       
          NSDL2_RUNTIME(NULL, NULL, "Resetting runtime rate interval timings in immediate mode");
          init_rate_intervals(cur_schedule);
          if ( sess_rpc < 100.0) 
            cur_schedule->iid_mu = (int)((double)(60000)/sess_rpc); 
          else
            cur_schedule->iid_mu = calc_iid_ms_from_rpm(cur_schedule, (int)sess_rpc);       
        }
        NSDL2_RUNTIME(NULL, NULL, "After RTC(RD phase)- \nramp_down_phase_ptr->num_vusers_or_sess=%d\nramp_down_phase_ptr->ramped_down_vusers=%d\nramp_down_phase_ptr->max_ramp_down_vuser_or_sess=%d\nphase_users_available_to_delete=%d\nusers_to_remove=%d\ncum_users_to_remove=%d\nusers_left_to_remove=%d\nrunprof_table_shr_mem[%d].quantity=%d",
        ramp_down_phase_ptr->num_vusers_or_sess, ramp_down_phase_ptr->ramped_down_vusers, ramp_down_phase_ptr->max_ramp_down_vuser_or_sess, phase_users_available_to_delete, users_to_remove_from_this_phase, *cum_users_to_remove, users_left_to_remove, ramp_down_phase_ptr->rpc, grp_idx, runprof_table_shr_mem[grp_idx].quantity); 
      } 
      if (ramp_down_phase_ptr->num_vusers_or_sess == 0) {
        send_phase_complete(cur_schedule); 
      }     
      if (users_left_to_remove == 0) break;
    }
  }
  if (rtc_ramp_down_phase_ptr->ramp_down_mode != RAMP_DOWN_MODE_IMMEDIATE)
  {
    *cum_users_to_remove = 0;  
    runtime_schedule->cur_vusers_or_sess = 0; 
    Phases *ph_tmp; 
    ph_tmp = &runtime_schedule->phase_array[runtime_schedule->phase_idx];
    ph_tmp->phase_type = SCHEDULE_PHASE_RAMP_DOWN; 
    ph_tmp->phase_status = PHASE_RUNNING;
    rtc_ramp_down_phase_ptr->ramped_down_vusers = 0;
    rtc_ramp_down_phase_ptr->max_ramp_down_vuser_or_sess = 0;
    //int tot_users = rtc_ramp_down_phase_ptr->num_vusers_or_sess;
    rtc_ramp_down_phase_ptr->num_vusers_or_sess = quantity;
    NSDL2_RUNTIME(NULL, NULL, "Total users=%d, num_vusers_or_sess=%d, ramp_up_time = %d",
                   rtc_ramp_down_phase_ptr->num_vusers_or_sess, rtc_ramp_down_phase_ptr->num_vusers_or_sess, rtc_ramp_down_phase_ptr->ramp_down_time);
    if(grp_type == TC_FIX_CONCURRENT_USERS)
    {
      if(rtc_ramp_down_phase_ptr->ramp_down_mode == RAMP_DOWN_MODE_TIME)
        rtc_ramp_down_phase_ptr->rpc = (double)rtc_ramp_down_phase_ptr->num_vusers_or_sess / (double)(rtc_ramp_down_phase_ptr->ramp_down_time/(1000.0*60));        
    }
    NSDL2_RUNTIME(NULL, NULL, "Deleting users as per mode group is running");
    NSDL2_RUNTIME(NULL, NULL, "New Phase: ph_tmp = %p, cur_vusers_or_sess=%d, ramp_up->num_vusers_or_sess=%d, ramped_up_vusers=%d,"
                        " max_ramp_up_vuser_or_sess=%d, rpc=%d", ph_tmp, runtime_schedule->cur_vusers_or_sess,
                        rtc_ramp_down_phase_ptr->num_vusers_or_sess, rtc_ramp_down_phase_ptr->ramped_down_vusers,
                        rtc_ramp_down_phase_ptr->max_ramp_down_vuser_or_sess, rtc_ramp_down_phase_ptr->rpc);

    runtime_phase_data_updation(runtime_schedule, now);
  }
  NSDL2_RUNTIME(NULL, NULL, "Exiting Method. users_left_to_remove=%d", users_left_to_remove);
  return users_left_to_remove;
}

/*
  delete_users_modes:
  Mode 0: Delete users existing in the system only
  Mode 1: Delete users existing in the system + not-ramped up users as well 
          Order of deletion will be:
          a. First, removing users already in the sytem
          b. Then, removing users not-ramped up
  Mode 2: Delete users existing in the system + not-ramped up users as well 
          Order of deletion will be:
          a. First, removing users not-ramped up
          b. Then, removing users already in the system
*/

static void runtime_delete_users(Schedule *cur_schedule, Schedule *runtime_schedule, int grp_idx, int quantity[], int group_not_started, u_ns_ts_t now)
{
  int cum_qty_to_remove=0;
  IW_UNUSED(int qty_left_to_remove=0);

  NSDL2_RUNTIME(NULL, NULL, "Method called. grp_idx=%d, quantity=%d", grp_idx, quantity[0]); 

  NSDL2_RUNTIME(NULL, NULL,"runtime decrease quantity mode=%d", global_settings->runtime_decrease_quantity_mode);

  //Mode 0: Delete only those users which exists in the system
  if(global_settings->runtime_decrease_quantity_mode == RUNTIME_DECREASE_QUANTITY_MODE_0)
  {
    if(!group_not_started)
      IW_NDEBUG_UNUSED(qty_left_to_remove, runtime_delete_ramped_up_vusers(cur_schedule, runtime_schedule, grp_idx,
                       -quantity[0], &cum_qty_to_remove, /*phase_idx,*/now));
    NSDL2_RUNTIME(NULL, NULL, "Users which are left after removing ramped-up users= %d", qty_left_to_remove); 
  }
  //Mode 1: Delete users existing in the system + not-ramped up users as well, removing ramped-up users first
  else if(global_settings->runtime_decrease_quantity_mode == RUNTIME_DECREASE_QUANTITY_MODE_1)
  {
    if(!group_not_started)
      IW_NDEBUG_UNUSED(qty_left_to_remove, runtime_delete_ramped_up_vusers(cur_schedule, runtime_schedule, grp_idx,
                       -quantity[0], &cum_qty_to_remove, /*phase_idx,*/now));
    NSDL2_RUNTIME(NULL, NULL, "Users which are left after removing ramped-up users= %d", qty_left_to_remove); 
    IW_NDEBUG_UNUSED(qty_left_to_remove, runtime_delete_users_not_ramped_up(cur_schedule, runtime_schedule, grp_idx,
                     -quantity[1], now));
    NSDL2_RUNTIME(NULL, NULL, "Users which are left after removing NOT-ramped-up sessions = %d", qty_left_to_remove); 
  }
  //Mode 2: Delete users existing in the system + not-ramped up users as well, removing not ramped-up users first
  else if(global_settings->runtime_decrease_quantity_mode == RUNTIME_DECREASE_QUANTITY_MODE_2)
  {
    IW_NDEBUG_UNUSED(qty_left_to_remove, runtime_delete_users_not_ramped_up(cur_schedule, runtime_schedule, grp_idx,
                    -quantity[1], now));
    NSDL2_RUNTIME(NULL, NULL, "Users which are left after removing NOT-ramped-up users= %d", qty_left_to_remove); 
    if(!group_not_started)
      IW_NDEBUG_UNUSED(qty_left_to_remove, runtime_delete_ramped_up_vusers(cur_schedule, runtime_schedule, grp_idx, 
                                                  -quantity[0], &cum_qty_to_remove, now));
    NSDL2_RUNTIME(NULL, NULL, "Users which are left after removing ramped-up users= %d", qty_left_to_remove); 
  }

  NSDL2_RUNTIME(NULL, NULL, "Users to be removed from the system=%d\nUsers which cannot be removed from the system = %d", 
                     cum_qty_to_remove, qty_left_to_remove); 
  if (group_not_started ||
      runtime_schedule->phase_array[runtime_schedule->phase_idx].phase_cmd.ramp_down_phase.ramp_down_mode == RAMP_DOWN_MODE_IMMEDIATE)
  {
    parent_child parent_child_msg; 
    parent_child_msg.grp_idx = runtime_schedule->group_idx;
    runtime_schedule->phase_array[runtime_schedule->phase_idx].phase_status = PHASE_IS_COMPLETED;
    parent_child_msg.phase_idx = runtime_schedule->phase_idx;
    parent_child_msg.rtc_idx = runtime_schedule->rtc_idx;
    parent_child_msg.rtc_id = runtime_schedule->rtc_id;
    NSTL1(NULL, NULL, "runtime_delete_users: parent_child_msg.rtc_idx = %d, parent_child_msg.rtc_id = %d", parent_child_msg.rtc_idx, parent_child_msg.rtc_id);
    fill_and_send_child_to_parent_msg("RTC_PHASE_COMPLETED", &parent_child_msg, RTC_PHASE_COMPLETED);
    runtime_schedule->rtc_state = RTC_FREE;
    NSDL2_RUNTIME(NULL, NULL, "RUNTIME: runtime_schedule->rtc_state = %d, runtime_schedule =%p", runtime_schedule->rtc_state, runtime_schedule); 
  } else
    runtime_schedule->rtc_state = RTC_RUNNING;//Setting the state to running
  if(cum_qty_to_remove)
  {
    //remove_users_ex(cur_schedule, 0, now, 1, cum_qty_to_remove, grp_idx, runtime_schedule->rampdown_method);  //1 for runtime
    int runtime_method;
    if(!global_settings->runtime_stop_immediately)
       runtime_method = runprof_table_shr_mem[grp_idx].gset.rampdown_method.mode;
    else
       runtime_method = RDM_MODE_STOP_IMMEDIATELY;
    remove_users_ex(cur_schedule, 0, now, 1, cum_qty_to_remove, grp_idx, runtime_method);  //1 for runtime
  }
}

/*
  delete_users_modes:
  Mode 0: Delete users existing in the system only
  Mode 1: Delete users existing in the system + not-ramped up users as well 
          Order of deletion will be:
          a. First, removing users already in the sytem
          b. Then, removing users not-ramped up
  Mode 2: Delete users existing in the system + not-ramped up users as well 
          Order of deletion will be:
          a. First, removing users not-ramped up
          b. Then, removing users already in the system
*/

static void runtime_decrease_session_rate(Schedule *cur_schedule, Schedule *runtime_schedule, int grp_idx, int quantity[], int group_not_started, u_ns_ts_t now)
{
  int cum_qty_to_remove=0;
  IW_UNUSED(int qty_left_to_remove=0);

  NSDL2_RUNTIME(NULL, NULL, "Method called. grp_idx=%d, quantity=%d", grp_idx, quantity[0]); 

  NSDL2_RUNTIME(NULL, NULL,"runtime decrease quantity mode=%d", global_settings->runtime_decrease_quantity_mode);
  //Mode 0: Delete sessions existing in the system only
  if(global_settings->runtime_decrease_quantity_mode == RUNTIME_DECREASE_QUANTITY_MODE_0)
  {
    if(!group_not_started)
      IW_NDEBUG_UNUSED(qty_left_to_remove, runtime_delete_ramped_up_vusers(cur_schedule, runtime_schedule,
                       grp_idx, -quantity[0], &cum_qty_to_remove, now));
    NSDL2_RUNTIME(NULL, NULL, "Sessions which are left after removing ramped-up sessions = %d", qty_left_to_remove); 
  }
  //Mode 1: Delete sessions existing in the system + not-ramped up users as well, removing ramped-up users first
  else if(global_settings->runtime_decrease_quantity_mode == RUNTIME_DECREASE_QUANTITY_MODE_1)
  {
    if(!group_not_started)
      IW_NDEBUG_UNUSED(qty_left_to_remove, runtime_delete_ramped_up_vusers(cur_schedule, runtime_schedule,
                       grp_idx, -quantity[0], &cum_qty_to_remove, now));
    NSDL2_RUNTIME(NULL, NULL, "Sessions which are left after removing ramped-up sessions = %d", qty_left_to_remove); 
    IW_NDEBUG_UNUSED(qty_left_to_remove, runtime_delete_users_not_ramped_up(cur_schedule, runtime_schedule,
                     grp_idx, -quantity[1], now));
    NSDL2_RUNTIME(NULL, NULL, "Sessions which are left after removing NOT-ramped-up sessions = %d", qty_left_to_remove); 
  }
  //Mode 2: Delete sessions existing in the system + not-ramped up users as well, removing not ramped-up users first
  else if(global_settings->runtime_decrease_quantity_mode == RUNTIME_DECREASE_QUANTITY_MODE_2)
  {
    IW_NDEBUG_UNUSED(qty_left_to_remove, runtime_delete_users_not_ramped_up(cur_schedule, runtime_schedule,
                     grp_idx, -quantity[1], now));
    NSDL2_RUNTIME(NULL, NULL, "Sessions which are left after removing NOT-ramped-up sessions = %d", qty_left_to_remove); 
    if(!group_not_started)
      IW_NDEBUG_UNUSED(qty_left_to_remove, runtime_delete_ramped_up_vusers(cur_schedule, runtime_schedule,
                     grp_idx, -quantity[0], &cum_qty_to_remove, now));
    NSDL2_RUNTIME(NULL, NULL, "Sessions which are left after removing ramped-up sessions = %d", qty_left_to_remove); 
  }
  
  NSDL2_RUNTIME(NULL, NULL, "Sessions to be removed from the system=%d\nSessions which cannot be removed from the system = %d", 
                     cum_qty_to_remove, qty_left_to_remove); 
  if (group_not_started ||
      runtime_schedule->phase_array[runtime_schedule->phase_idx].phase_cmd.ramp_down_phase.ramp_down_mode == RAMP_DOWN_MODE_IMMEDIATE)
  {
    parent_child parent_child_msg; 
    runtime_schedule->group_idx = grp_idx;
    parent_child_msg.grp_idx = runtime_schedule->group_idx;
    runtime_schedule->phase_array[runtime_schedule->phase_idx].phase_status = PHASE_IS_COMPLETED;
    parent_child_msg.phase_idx = runtime_schedule->phase_idx;
    parent_child_msg.rtc_idx = runtime_schedule->rtc_idx;
    parent_child_msg.rtc_id = runtime_schedule->rtc_id;
    fill_and_send_child_to_parent_msg("RTC_PHASE_COMPLETED", &parent_child_msg, RTC_PHASE_COMPLETED);
    runtime_schedule->rtc_state = RTC_FREE;
    NSDL2_RUNTIME(NULL, NULL, "RUNTIME: runtime_schedule->rtc_state = %d, runtime_schedule =%p", runtime_schedule->rtc_state, runtime_schedule); 
  } else
    runtime_schedule->rtc_state = RTC_RUNNING;//Setting the state to running

  if (cum_qty_to_remove)
    search_and_remove_user_for_fsr(cur_schedule, now);
}

/*Purpose
  Update the change in quantity in appropriate ramp-up & ramp-down DS
  1.Searching for appropriate RU phase
  2.Search for appropriate RD phase
  3.Updating DS
     a. Schedule -
          cur_schedule->cur_vusers_or_sess
     b. RU phase -
          num_vusers_or_sess
          max_ramp_up_vuser_or_sess
     c. RD phase - 
          num_vusers_or_sess
*/

#if 0
static void runtime_increase_session_rate(Schedule *cur_schedule, Schedule *runtime_schedule, int grp_idx, int quantity, int group_not_started, u_ns_ts_t now)
{
  int ramp_up_phase_id = -1, ramp_down_phase_id = -1;
  Ramp_up_schedule_phase *ramp_up_phase_ptr;
  Ramp_down_schedule_phase *ramp_down_phase_ptr;
  //double sess_rpc;

  NSDL2_RUNTIME(NULL, NULL, "Method called. grp_idx=%d, quantity=%d, group_not_started=%h", grp_idx, quantity, group_not_started); 
  if(quantity == 0) return;

  //1.Searching for appropriate RU phase
  ramp_up_phase_id = runtime_find_RU_phase_for_increase_in_qty(cur_schedule, quantity); 
  ramp_up_phase_ptr = &cur_schedule->phase_array[ramp_up_phase_id].phase_cmd.ramp_up_phase;
  
  //2. Search for appropriate RD phase
  ramp_down_phase_id = runtime_find_RD_phase_for_increase_in_qty(cur_schedule, quantity); 
  ramp_down_phase_ptr = &cur_schedule->phase_array[ramp_down_phase_id].phase_cmd.ramp_down_phase;

  //3. Updating DS
  NSDL2_RUNTIME(NULL, NULL, "Before increasing %d sessions - \ncur_vusers_or_sess=%d \nramp_up->num_vusers_or_sess=%d    \
                  \nmax_ramp_up_vuser_or_sess=%d \nramp_down->num_vusers_or_sess=%d \nramp_down_phase_ptr->rpc=%f", quantity, 
                  cur_schedule->cur_vusers_or_sess, ramp_up_phase_ptr->num_vusers_or_sess, 
                  ramp_up_phase_ptr->max_ramp_up_vuser_or_sess, 
                  ramp_down_phase_ptr->num_vusers_or_sess, ramp_down_phase_ptr->rpc); 

  //a. Updating Schedule:
  if(group_not_started == 0)
  {
    runtime_schedule->cur_vusers_or_sess = quantity;
#if 0
    sess_rpc = runtime_schedule->cur_vusers_or_sess/SESSION_RATE_MULTIPLE;

    if ( sess_rpc < 100.0)
     runtime_schedule->iid_mu = (int)((double)(60000)/sess_rpc);
    else
     runtime_schedule->iid_mu = calc_iid_ms_from_rpm(runtime_schedule, (int)sess_rpc);
#endif
    runtime_phase_data_updation(runtime_schedule, now);
  }
  runprof_table_shr_mem[grp_idx].quantity += quantity;

  //Updating RU phase:
  ramp_up_phase_ptr->num_vusers_or_sess += quantity;
  if(group_not_started == 0) ramp_up_phase_ptr->max_ramp_up_vuser_or_sess += quantity; 

  //Updating RD phase:
  ramp_down_phase_ptr->num_vusers_or_sess += quantity;  

  //Recalculating rpc in case of TIME mode
  if(ramp_down_phase_ptr->ramp_down_mode == RAMP_DOWN_MODE_TIME_SESSIONS)
    runtime_update_in_time_mode_FSR(cur_schedule, ramp_down_phase_id, ramp_down_phase_ptr, now);

  NSDL2_RUNTIME(NULL, NULL, "After increasing %d sessions - cur_vusers_or_sess=%d \nramp_up->num_vusers_or_sess=%d        \
                  \nmax_ramp_up_vuser_or_sess=%d \nramp_down->num_vusers_or_sess=%d \nramp_down_phase_ptr->rpc=%f", quantity, 
                  cur_schedule->cur_vusers_or_sess, ramp_up_phase_ptr->num_vusers_or_sess, 
                  ramp_up_phase_ptr->max_ramp_up_vuser_or_sess, 
                  ramp_down_phase_ptr->num_vusers_or_sess, ramp_down_phase_ptr->rpc); 
#if 0
  if(group_not_started == 0)
    generate_users(cd, now);
#endif
  NSDL2_RUNTIME(NULL, NULL, "Exiting Method");
  return;
}
#endif

//Taking decision whether to add users/delete users/increase session rate/decrease session rate
//based on quantity and Scenario Type
static void runtime_quantity_updation(int grp_idx, Schedule *cur_schedule, Schedule *cur_runtime_schedule, int *quantity, u_ns_ts_t now)
{
  int group_not_started = 0;
  int scenario_type = get_group_mode(grp_idx);
  
  NSDL2_RUNTIME(NULL, NULL, "Method called. grp_idx=%d, ramped_up quantity=%d, not ramped_up quantity=%d", grp_idx, quantity[0], quantity[1]); 
  if(sigterm_received) return;

  //if((cur_schedule->phase_array[0].dependent_grp != -1) 
  //Check for non runtime group started or not
  NSDL2_RUNTIME(NULL, NULL, "cur_schedule->phase_array[0].phase_type=%d, cur_schedule->phase_array[0].phase_status=%d", cur_schedule->phase_array[0].phase_type, cur_schedule->phase_array[0].phase_status); 
  if((cur_schedule->phase_array[0].phase_type == SCHEDULE_PHASE_START) && (cur_schedule->phase_array[0].phase_status != PHASE_IS_COMPLETED))
  {
    group_not_started = 1;
    NSDL2_RUNTIME(NULL, NULL, "Target group=%d not started yet", grp_idx);
  }

  //If quantity = 0, nothing to change at runtime
  if(quantity[0] > 0) //Increase Users/Sessions
  {
    NSDL2_RUNTIME(NULL, NULL, "Increased User/Session quantity=%d, scenario_type=%s", quantity[0], 
           ((scenario_type==TC_FIX_CONCURRENT_USERS)?"FCU":"FSR")); 
    runtime_add_users(cur_schedule, cur_runtime_schedule, grp_idx, quantity[0], group_not_started, now);
  }
  else if((quantity[0] < 0) || (quantity[1] < 0)) //Decrease Users/Sessions
  {
    NSDL2_RUNTIME(NULL, NULL, "Decreased User/Session quantity Ramped=%d,NonRamped=%d scenario_type=%s", quantity[0], quantity[1],
           ((scenario_type==TC_FIX_CONCURRENT_USERS)?"FCU":"FSR")); 
    if(scenario_type == TC_FIX_CONCURRENT_USERS)
      runtime_delete_users(cur_schedule, cur_runtime_schedule, grp_idx, quantity, group_not_started, now);
    else
      runtime_decrease_session_rate(cur_schedule, cur_runtime_schedule, grp_idx, quantity, group_not_started, now);
  }
  NSDL2_RUNTIME(NULL, NULL, "Exiting Method"); 
  return;
}

void runtime_data_updation(u_ns_ts_t now) {

  Schedule *cur_schedule, *cur_runtime_schedule;
  void *schedule_mem_ptr;
  int grp_idx, rtc_idx;

  NSDL2_RUNTIME(NULL, NULL, "Method called. now = %u, global_settings->num_qty_rtc = %d, total_runprof_entries = %d", now, global_settings->num_qty_rtc, total_runprof_entries); 

  /* Start phase will always be there. */
  if(global_settings->schedule_by == SCHEDULE_BY_SCENARIO) 
  {
    NSDL4_RUNTIME(NULL, NULL, "SCHEDULE is SCHEDULE_BY_SCENARIO"); 
    cur_schedule = v_port_entry->scenario_schedule;
    runtime_phase_data_updation(cur_schedule, now);
    //Go in a loop for each group and apply runtime quantity change
    schedule_mem_ptr = v_port_entry->runtime_schedule;
    for (rtc_idx = 0; rtc_idx < (global_settings->num_qty_rtc * total_runprof_entries); rtc_idx++)
    {  
      cur_runtime_schedule = schedule_mem_ptr + (rtc_idx * find_runtime_qty_mem_size());
      NSDL4_RUNTIME(NULL, NULL, "cur_runtime_schedule->rtc_state=%d", cur_runtime_schedule->rtc_state); 
      if (cur_runtime_schedule->rtc_state == RTC_NEED_TO_PROCESS) 
      {
        NSDL4_RUNTIME(NULL, NULL, "Running Runtime %p, RTC Idx=%d, Phase index=%d," 
                    "ramped_up quantity=%d, not ramped_up quantity=%d", cur_runtime_schedule, rtc_idx, 
                    cur_runtime_schedule->phase_idx, cur_runtime_schedule->runtime_qty_change[0], 
                    cur_runtime_schedule->runtime_qty_change[1]); 

        runtime_quantity_updation(cur_runtime_schedule->group_idx, cur_schedule, cur_runtime_schedule, &(cur_runtime_schedule->runtime_qty_change[0]), now);
        cur_runtime_schedule->runtime_qty_change[0] = 0;  //Resetting quantity to 0
        cur_runtime_schedule->runtime_qty_change[1] = 0;  //Resetting quantity to 0
        NSDL4_RUNTIME(NULL, NULL, "cur_runtime_schedule->rtc_state=%d, RTC Idx = %d", cur_runtime_schedule->rtc_state, rtc_idx); 
      }
    }
  } else {
    NSDL4_RUNTIME(NULL, NULL, "SCHEDULE is SCHEDULE_BY_GROUP"); 
    for(grp_idx = 0; grp_idx < total_runprof_entries; grp_idx++) 
    {
      cur_schedule = &(v_port_entry->group_schedule[grp_idx]);
      runtime_phase_data_updation(cur_schedule, now);
    }
    for (rtc_idx = 0; rtc_idx < (global_settings->num_qty_rtc * total_runprof_entries); rtc_idx++)
    {  
      for(grp_idx = 0; grp_idx < total_runprof_entries; grp_idx++) 
      {
        cur_schedule = &(v_port_entry->group_schedule[grp_idx]);
        schedule_mem_ptr = v_port_entry->runtime_schedule;
        cur_runtime_schedule = schedule_mem_ptr + (rtc_idx * find_runtime_qty_mem_size());
        NSDL4_RUNTIME(NULL, NULL, "cur_runtime_schedule->rtc_state=%d, running grp_idx=%d, cur_runtime_schedule->group_idx=%d", 
                      cur_runtime_schedule->rtc_state, grp_idx, cur_runtime_schedule->group_idx); 
        if (cur_runtime_schedule->rtc_state == RTC_NEED_TO_PROCESS && (grp_idx == cur_runtime_schedule->group_idx)) 
        {
          NSDL4_RUNTIME(NULL, NULL, "Running Runtime %p, RTC Idx=%d, Phase index=%d," 
                     "ramped_up quantity=%d, not ramped_up quantity=%d", cur_runtime_schedule, rtc_idx, 
                     cur_runtime_schedule->phase_idx, cur_runtime_schedule->runtime_qty_change[0], 
                     cur_runtime_schedule->runtime_qty_change[1]); 

          runtime_quantity_updation(grp_idx, cur_schedule, cur_runtime_schedule, &(cur_runtime_schedule->runtime_qty_change[0]), now);
          cur_runtime_schedule->runtime_qty_change[0] = 0;  //Resetting quantity to 0
          cur_runtime_schedule->runtime_qty_change[1] = 0;  //Resetting quantity to 0
          NSDL4_RUNTIME(NULL, NULL, "cur_runtime_schedule->rtc_state=%d, RTC Idx = %d", cur_runtime_schedule->rtc_state, rtc_idx); 
        }
      }
    }
  }
  NSDL2_RUNTIME(NULL, NULL, "Exiting Method."); 
}

static int find_phase_id (int phase_index)
{
  int phase_id = 0;
  NSDL1_RUNTIME(NULL, NULL, "Method called, phase_index=%d", phase_index); 
  phase_id = phase_index - (global_settings->num_qty_ph_rtc * my_port_index);
  return(phase_id);
}

int find_running_phase_tmr(int phase_idx)
{
  int tmr_index = 0;
  NSDL1_RUNTIME(NULL, NULL, "Method called");

  int phase_id = find_phase_id(phase_idx);
  //Ramp Timer Index
  tmr_index = phase_id + (global_settings->num_qty_ph_rtc * my_port_index);  
  return(tmr_index);
}
