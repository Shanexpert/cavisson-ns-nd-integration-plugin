/************************************************************************************************************
 *  Name            : ns_schedule_ramp_up.c 
 *  Purpose         : To control Netstorm Ramp Up Phases
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
#include "nslb_time_stamp.h"
#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "user_tables.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "util.h"
#include "tmr.h"
#include "timing.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "ns_schedule_ramp_up_fcu.h"
#include "ns_schedule_ramp_up_fsr.h"
#include "ns_schedule_phases_parse.h"
#include "timing.h"
#include "poi.h"
#include "tmr.h"
#include "ns_percentile.h"
#include "ns_schedule_ramp_down_fcu.h"
#include "ns_alloc.h"
#include "ns_gdf.h"
#include "ns_schedule_pause_and_resume.h"
#include "ns_log.h"
#include "wait_forever.h"
#include "ns_vuser_runtime_control.h"
#include "ns_exit.h"
#include "ns_runtime.h"

double round(double x);

void validate_run_phases_keyword()
{
  NSDL2_SCHEDULE(NULL, NULL, "Method called.");
  if (global_settings->num_fetches && g_percentile_mode == PERCENTILE_MODE_ALL_PHASES) {
    NS_EXIT(-1, "PERCENTILE_REPORT mode 1 is not supported with Run Time Change");
  }
}

static void update_ramp_up_vusers( ClientData cd, u_ns_ts_t now);

// ------ Timers 
static void start_ramp_up_msg_timer(Schedule *schedule_ptr, u_ns_ts_t now, int time_out)
{ 
  ClientData cd;
  cd.p = schedule_ptr;
  
  NSDL2_SCHEDULE(NULL, NULL, "Method called. Starting RampUp Msg Timer. now = %u, time_out = %d", now, time_out);

  //ab_timers[AB_TIMEOUT_END].timeout_val = time_out;
  //dis_timer_add( AB_TIMEOUT_END, schedule_ptr->phase_end_tmr, now, send_ramp_up_msg, cd, 1 );

  schedule_ptr->phase_end_tmr->actual_timeout = time_out;
  dis_timer_think_add( AB_TIMEOUT_END, schedule_ptr->phase_end_tmr, now, send_ramp_up_msg, cd, 1 );
}

// Purpose: To stop end timer. Used by all phases
void stop_phase_end_timer(Schedule *schedule_ptr)
{
  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type);

  if (schedule_ptr->phase_end_tmr->timer_type >= 0 )
  {
    NSDL2_SCHEDULE(NULL, NULL, "Deleting phase_end_tmr Timer");
    dis_timer_del(schedule_ptr->phase_end_tmr);
  }
  else
    NSDL2_SCHEDULE(NULL, NULL, "Timer not deleted, Group Index = %d, Phases Index = %d, Phases Type = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type);
}

static void start_ramp_timer(Schedule *schedule_ptr, u_ns_ts_t now, int periodic)
{
  ClientData cd;
 
  cd.p = schedule_ptr;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d, iid = %u", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, schedule_ptr->iid_mu);

  // chk arleady running
  //ab_timers[AB_TIMEOUT_RAMP].timeout_val = schedule_ptr->iid_mu;
  //dis_timer_add( AB_TIMEOUT_RAMP, schedule_ptr->phase_ramp_tmr, now, update_ramp_up_vusers, cd, periodic);
  schedule_ptr->phase_ramp_tmr->actual_timeout = schedule_ptr->iid_mu;
  dis_timer_think_add( AB_TIMEOUT_RAMP, schedule_ptr->phase_ramp_tmr, now, update_ramp_up_vusers, cd, periodic);
}


// Purpose: To stop ramp timer. Used by RampUp, RampDown phase
void stop_phase_ramp_timer(Schedule *schedule_ptr)
{
  NSDL2_SCHEDULE(NULL, NULL, "Method called.");

  if (schedule_ptr->phase_ramp_tmr->timer_type >= 0 )
  {
    NSDL2_SCHEDULE(NULL, NULL, "Stopping RampUp Timer");
    dis_timer_del(schedule_ptr->phase_ramp_tmr);
  }
}

void ramp_up_phase_done_fcu(Schedule *schedule_ptr)
{
  Phases *phase_ptr = &schedule_ptr->phase_array[schedule_ptr->phase_idx];

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d, Phases Status = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, phase_ptr->phase_status);

  if (phase_ptr->phase_status == PHASE_RUNNING)         // Runnning 
  {  
    if(schedule_ptr->type == 0)
      send_ramp_up_done_msg(schedule_ptr);
    stop_phase_ramp_timer(schedule_ptr);
    stop_phase_end_timer(schedule_ptr);
    send_phase_complete(schedule_ptr);
  }
  else
    NSDL2_SCHEDULE(NULL, NULL, "Phase is not completed. Group Index = %d, Phases Index = %d, Phases Type = %d, Phases Status = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, phase_ptr->phase_status);
}

// This traverse through all the phases of group, If phase is Ramp Up then it create users(10 at max at a time) if ramped_up_vusers < max_vusers
//
// TBD - Review get_ms_stamp()and clean the code
static u_ns_ts_t generate_users_in_chunk(Schedule *schedule_ptr, u_ns_ts_t now)
{
  Phases *phase_ptr = &schedule_ptr->phase_array[schedule_ptr->phase_idx];
  Ramp_up_schedule_phase *ramp_up_phase_ptr = &schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_cmd.ramp_up_phase;

  int scenario_type = get_group_mode(schedule_ptr->group_idx);

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d, "
                             "ramped_up_vusers = %d, max_ramp_up_vuser_or_sess = %d", 
                              schedule_ptr->group_idx, schedule_ptr->phase_idx, 
                              schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, 
                              ramp_up_phase_ptr->ramped_up_vusers, ramp_up_phase_ptr->max_ramp_up_vuser_or_sess);

  if(scenario_type != TC_FIX_CONCURRENT_USERS || 
     schedule_ptr->phase_idx == -1 || 
     schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type != SCHEDULE_PHASE_RAMP_UP ||
     phase_ptr->phase_status != PHASE_RUNNING )         // Not Runnning 
  {
    now = get_ms_stamp();
    return now;
  }


  if (ramp_up_phase_ptr->ramped_up_vusers < ramp_up_phase_ptr->max_ramp_up_vuser_or_sess)
  {
    int local_ramped_up_vusers = new_user((ramp_up_phase_ptr->max_ramp_up_vuser_or_sess - ramp_up_phase_ptr->ramped_up_vusers) > 10
                                     ? 10:(ramp_up_phase_ptr->max_ramp_up_vuser_or_sess - ramp_up_phase_ptr->ramped_up_vusers), 
                                       now, schedule_ptr->group_idx, 1, NULL, schedule_ptr->sm_mon_info); 

    ramp_up_phase_ptr->ramped_up_vusers += local_ramped_up_vusers;
    // cur_vusers_or_sess is used only for sending current number of users to parent
    schedule_ptr->cur_vusers_or_sess += local_ramped_up_vusers;

    NSDL2_SCHEDULE(NULL, NULL, "Group Index = %d, Phases Index = %d, Phases Type = %d, Cur User = %d, "
                               "ramped_up_vusers = %d, local_ramped_up_vusers = %d", 
                                schedule_ptr->group_idx, schedule_ptr->phase_idx, 
                                schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, schedule_ptr->cur_vusers_or_sess, 
                                ramp_up_phase_ptr->ramped_up_vusers, local_ramped_up_vusers);

    // We call dis_timer_run in for loop of child but when new users is called then it may possible, some delay in timers run
    // so called here 
    now = get_ms_stamp(); // Must call here
    dis_timer_run(now);
  }
  else if(ramp_up_phase_ptr->ramped_up_vusers == ramp_up_phase_ptr->num_vusers_or_sess)
  {
    now = get_ms_stamp(); // Must call here
    ramp_up_phase_done_fcu(schedule_ptr); // Since this phase (of scen or grp) complete, make it done
  }

  return now; 
}

int total_running_rtc = 1;
static u_ns_ts_t generate_users_in_chunk_for_rtc(u_ns_ts_t now)
{
  int rtc_idx, phase_idx;
  Schedule *schedule_ptr;
  void *schedule_mem_ptr;
    
  NSDL2_SCHEDULE(NULL, NULL, "Method called, now = %d, num_qty_rtc = %d", now, global_settings->num_qty_rtc);

  for(rtc_idx = 0; rtc_idx < (global_settings->num_qty_rtc * total_runprof_entries); rtc_idx++)
  {
    schedule_mem_ptr = v_port_entry->runtime_schedule;
    schedule_ptr = schedule_mem_ptr + (rtc_idx * find_runtime_qty_mem_size());

    NSDL2_SCHEDULE(NULL, NULL, "Group Index = %d, Phases Index = %d, RTC State = %d, RTC Idx = %d", 
                                schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->rtc_state, rtc_idx);

    if(schedule_ptr->rtc_state != RTC_RUNNING) //This RTC is not participating in runtime changes.
      continue;
      
    phase_idx = schedule_ptr->phase_idx;
    Phases *phase_ptr = &schedule_ptr->phase_array[phase_idx];
    Ramp_up_schedule_phase *ramp_up_phase_ptr = &schedule_ptr->phase_array[phase_idx].phase_cmd.ramp_up_phase;
  
    int scenario_type = get_group_mode(schedule_ptr->group_idx);

    NSDL2_SCHEDULE(NULL, NULL, "scenario_type=%d, Phases Index=%d, phase_ptr->phase_type=%d, "
                               "phase_ptr->phase_status=%d", 
                                scenario_type, schedule_ptr->phase_idx, phase_ptr->phase_type, phase_ptr->phase_status);

    if ( scenario_type != TC_FIX_CONCURRENT_USERS || 
         schedule_ptr->phase_idx == -1 || 
         phase_ptr->phase_type != SCHEDULE_PHASE_RAMP_UP ||
         phase_ptr->phase_status != PHASE_RUNNING )         // Not Runnning 
    { 
      now = get_ms_stamp();
      continue;
    } 
    
    NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d, "
                               "ramped_up_vusers = %d, max_ramp_up_vuser_or_sess = %d", 
                                schedule_ptr->group_idx, phase_idx, schedule_ptr->phase_array[phase_idx].phase_type, 
                                ramp_up_phase_ptr->ramped_up_vusers, ramp_up_phase_ptr->max_ramp_up_vuser_or_sess);

    if (ramp_up_phase_ptr->ramped_up_vusers < ramp_up_phase_ptr->max_ramp_up_vuser_or_sess)
    {
      int local_ramped_up_vusers = new_user((ramp_up_phase_ptr->max_ramp_up_vuser_or_sess - ramp_up_phase_ptr->ramped_up_vusers) > 10 
                                             ? 10 : (ramp_up_phase_ptr->max_ramp_up_vuser_or_sess - ramp_up_phase_ptr->ramped_up_vusers), 
                                             now, schedule_ptr->group_idx, 1, NULL, schedule_ptr->sm_mon_info); 

      ramp_up_phase_ptr->ramped_up_vusers += local_ramped_up_vusers;

      //TODO: Achint - Do we need this??
      // cur_vusers_or_sess is used only for sending current number of users to parent
      schedule_ptr->cur_vusers_or_sess += local_ramped_up_vusers;

      NSDL2_SCHEDULE(NULL, NULL, "Group Index = %d, Phases Index = %d, Phases Type = %d, Cur User = %d, "
                                 "ramped_up_vusers = %d, local_ramped_up_vusers = %d", 
                                  schedule_ptr->group_idx, phase_idx, schedule_ptr->phase_array[phase_idx].phase_type, 
                                  schedule_ptr->cur_vusers_or_sess, ramp_up_phase_ptr->ramped_up_vusers, local_ramped_up_vusers);

      // We call dis_timer_run in for loop of child but when new users is called then it may possible, some delay in timers run
      // so called here 
      now = get_ms_stamp(); // Must call here
      dis_timer_run(now);
    }
    else if(ramp_up_phase_ptr->ramped_up_vusers == ramp_up_phase_ptr->num_vusers_or_sess)
    {
      now = get_ms_stamp(); // Must call here
      ramp_up_phase_done_fcu(schedule_ptr); // Since this phase (of scen or grp) complete, make it done
    }
  }
  return now;
}
// This method is called in for loop of child process to generate users
// It will go through to all the groups & call generate_users_in_chunk
u_ns_ts_t ramp_up_users(u_ns_ts_t now)
{       
  Schedule *schedule_ptr;
  int i = 0;

  NSDL2_SCHEDULE(NULL, NULL, "Method called, now = %u, pause done = %d, total_running_rtc = %d", 
                              now, global_settings->pause_done, total_running_rtc);

  if(global_settings->pause_done) {
   NSDL2_SCHEDULE(NULL, NULL, "Returning as test run is in paused stat, now = %u, pause done = %d", now, global_settings->pause_done);
   return now;
  }

  if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO) 
  {
    schedule_ptr = v_port_entry->scenario_schedule;
    now = generate_users_in_chunk(schedule_ptr, now);
  }
  else if(global_settings->schedule_by == SCHEDULE_BY_GROUP)
  {
    for(i = 0; i < total_runprof_entries; i++)
    {
      schedule_ptr = &(v_port_entry->group_schedule[i]);
      now = generate_users_in_chunk(schedule_ptr, now);
    }
  }
   
  if(total_running_rtc) 
    now = generate_users_in_chunk_for_rtc(now);
   
  return(now);
}

// --------------------- Ramp Up Immediate ----------------------------
// Here we set all num_vusers_or_sess to max_ramp_up_vuser_or_sess, so that we can ramp up all users at a time 
static void immediate_ramp_up_users(Schedule *schedule_ptr)
{
  Ramp_up_schedule_phase *ramp_up_phase_ptr = &schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_cmd.ramp_up_phase;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d, num_vusers_or_sess = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, ramp_up_phase_ptr->num_vusers_or_sess);

  if(ramp_up_phase_ptr->num_vusers_or_sess == 0)
  {
    NSDL2_SCHEDULE(NULL, NULL, "No user to ramp up. Group Index = %d, Phases Index = %d, Phases Type = %d, rate = %d, num_vusers_or_sess = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, ramp_up_phase_ptr->ramp_up_rate, ramp_up_phase_ptr->num_vusers_or_sess);
    ramp_up_phase_done_fcu(schedule_ptr);
    return;
  }
  
  ramp_up_phase_ptr->max_ramp_up_vuser_or_sess = ramp_up_phase_ptr->num_vusers_or_sess;
}

// --------------------- Ramp Up Rate - Random ----------------------------

// Purpose: Calculates number of user and iid
// Since we are using poison distribution, iid can become 0, then
// we need to take next sample and increament users till iid is not 0
//
// Return:
//   > 0 : If more users are left
//   = 0 : Not more users are left
static int random_ramp_mode_calc_max_vuser(Schedule *schedule_ptr)
{
  int poi_sample = 0;
  Phases *phase_ptr = &schedule_ptr->phase_array[schedule_ptr->phase_idx];
  Ramp_up_schedule_phase *ramp_up_phase_ptr = &phase_ptr->phase_cmd.ramp_up_phase;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, phase_ptr->phase_type);

  while (1)
  {
    if (ramp_up_phase_ptr->max_ramp_up_vuser_or_sess >= ramp_up_phase_ptr->num_vusers_or_sess)
    {
      ramp_up_phase_ptr->max_ramp_up_vuser_or_sess = ramp_up_phase_ptr->num_vusers_or_sess;
      NSDL4_SCHEDULE(NULL, NULL, "Group Index = %d, Phases Index = %d, Phases Type = %d, max_ramp_up_vuser_or_sess = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, phase_ptr->phase_type, ramp_up_phase_ptr->max_ramp_up_vuser_or_sess);
      return(0);
    }
    poi_sample = (int)ns_poi_sample(schedule_ptr->ns_iid_handle);
    if(poi_sample) break;
    
    // If 0, then add users
    NSDL2_SCHEDULE(NULL, NULL, "Group Index = %d, Phases Index = %d, Phases Type = %d, max_ramp_up_vuser_or_sess = %d, ramping_delta = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, phase_ptr->phase_type, ramp_up_phase_ptr->max_ramp_up_vuser_or_sess, schedule_ptr->ramping_delta);
    ramp_up_phase_ptr->max_ramp_up_vuser_or_sess += schedule_ptr->ramping_delta;
  }

  return(poi_sample);
}

// Purpose: To update max_ramp_up_vuser_or_sess with number of users to be ramped up
//
static void update_ramp_up_vusers( ClientData cd, u_ns_ts_t now)
{
  Schedule *schedule_ptr = (Schedule *)cd.p;
  Ramp_up_schedule_phase *ramp_up_phase_ptr = &schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_cmd.ramp_up_phase;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type);

   //
   /**
   // We will send phase complete message from ramp up users
  if (ramp_up_phase_ptr->max_ramp_up_vuser_or_sess == ramp_up_phase_ptr->num_vusers_or_sess)
  {
    send_phase_complete(schedule_ptr);
    return;
  }
  **/

  {
    // This one we need to do every time.
    if ((ramp_up_phase_ptr->ramp_up_mode == RAMP_UP_MODE_TIME || ramp_up_phase_ptr->ramp_up_mode == RAMP_UP_MODE_RATE)
         && ramp_up_phase_ptr->ramp_up_pattern == RAMP_UP_PATTERN_LINEAR)
      schedule_ptr->ramping_delta = get_rpm_users(schedule_ptr, 0);

    if ((ramp_up_phase_ptr->max_ramp_up_vuser_or_sess + schedule_ptr->ramping_delta) >= ramp_up_phase_ptr->num_vusers_or_sess)
    {
      ramp_up_phase_ptr->max_ramp_up_vuser_or_sess = ramp_up_phase_ptr->num_vusers_or_sess; 
      // All users are accounted for, so no need to start timer
      //
      NSDL2_SCHEDULE(NULL, NULL, "iid = %u, Group Index = %d, Phases Index = %d, Phases Type = %d", schedule_ptr->iid_mu, schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type);
      if(schedule_ptr->phase_ramp_tmr->timer_type > 0 && schedule_ptr->phase_ramp_tmr->periodic == 1)
        dis_timer_del(schedule_ptr->phase_ramp_tmr);

      return;
    }

    // Users are left
    ramp_up_phase_ptr->max_ramp_up_vuser_or_sess += schedule_ptr->ramping_delta; 

    // Random Pattern
    if (ramp_up_phase_ptr->ramp_up_pattern == RAMP_UP_PATTERN_RANDOM &&
       (ramp_up_phase_ptr->ramp_up_mode == RAMP_UP_MODE_TIME   ||
         ramp_up_phase_ptr->ramp_up_mode == RAMP_UP_MODE_RATE))
    {
      // For Random mode, since iid is random at every time, we need to start timere with new value of iid
      schedule_ptr->iid_mu = random_ramp_mode_calc_max_vuser(schedule_ptr);
      if(schedule_ptr->iid_mu == 0)
        schedule_ptr->iid_mu = 10;
      start_ramp_timer(schedule_ptr, now, 0);
    }
  }
}

static void step_ramp_up_set_vusers(Schedule *schedule_ptr, u_ns_ts_t now)
{
  Ramp_up_schedule_phase *ramp_up_phase_ptr = &schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_cmd.ramp_up_phase;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d, ramp_up_step_users = %d, ramp_up_step_time = %d, num_vusers_or_sess = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, ramp_up_phase_ptr->ramp_up_step_users, ramp_up_phase_ptr->ramp_up_step_time, ramp_up_phase_ptr->num_vusers_or_sess);
  /* Netomni Changes: Calculate STEP time with respect to total generators
   * iid_mu = step-time * total_generators * total_nvms */ 
  int total_gen = 0;
  if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO)
    total_gen = global_settings->num_generators;
  else
    total_gen = runprof_table_shr_mem[schedule_ptr->group_idx].num_generator_per_grp;

  if (total_gen) {
    schedule_ptr->iid_mu = ramp_up_phase_ptr->ramp_up_step_time * total_gen * global_settings->num_process;
  } else   
    schedule_ptr->iid_mu = ramp_up_phase_ptr->ramp_up_step_time * global_settings->num_process;

  NSDL2_SCHEDULE(NULL, NULL, "STEP Time: iid_mu = %d, total_nvms = %d, total_generators = %d", schedule_ptr->iid_mu, global_settings->num_process, total_gen);
  schedule_ptr->ramping_delta = ramp_up_phase_ptr->ramp_up_step_users;

  if(schedule_ptr->ramping_delta >= ramp_up_phase_ptr->num_vusers_or_sess)
  {
    // This is the case when first step will ramp all users
    schedule_ptr->ramping_delta = ramp_up_phase_ptr->num_vusers_or_sess;
    ramp_up_phase_ptr->max_ramp_up_vuser_or_sess = schedule_ptr->ramping_delta;
    // Added check for Run Time Change as in case delta > users to ramp_up, max_ramp_up_vuser_or_sess users were 
    // getting more than num_vusers_or_sess
    if(ramp_up_phase_ptr->max_ramp_up_vuser_or_sess > ramp_up_phase_ptr->num_vusers_or_sess)
    {
      NSDL2_SCHEDULE(NULL, NULL, "max_ramp_up_vuser_or_sess=%d, num_vusers_or_sess=%d, ramping_delta=%d", ramp_up_phase_ptr->max_ramp_up_vuser_or_sess, ramp_up_phase_ptr->num_vusers_or_sess, schedule_ptr->ramping_delta);
      ramp_up_phase_ptr->max_ramp_up_vuser_or_sess = ramp_up_phase_ptr->num_vusers_or_sess;
    }
  }
  else
  {
    // This is the case when more than one step will ramp all users
    //we will set ramp_up_phase->iid_mu to Y secs*num_process so that this child will update_ramp_up_vusers after ramp_up_phase->iid_mu millisec
    ramp_up_phase_ptr->max_ramp_up_vuser_or_sess = schedule_ptr->ramping_delta + ramp_up_phase_ptr->ramped_up_vusers;
    // Added check for Run Time Change as in case delta > users to ramp_up, max_ramp_up_vuser_or_sess users were 
    // getting more than num_vusers_or_sess
    if(ramp_up_phase_ptr->max_ramp_up_vuser_or_sess > ramp_up_phase_ptr->num_vusers_or_sess)
    {
      NSDL2_SCHEDULE(NULL, NULL, "max_ramp_up_vuser_or_sess=%d, num_vusers_or_sess=%d, ramping_delta=%d", ramp_up_phase_ptr->max_ramp_up_vuser_or_sess, ramp_up_phase_ptr->num_vusers_or_sess, schedule_ptr->ramping_delta);
      ramp_up_phase_ptr->max_ramp_up_vuser_or_sess = ramp_up_phase_ptr->num_vusers_or_sess;
    }
    start_ramp_timer(schedule_ptr, now, 1);
  }
}

static void step_ramp_up_child_stagger_timer_callback(ClientData cd, u_ns_ts_t now)
{
  Schedule *schedule_ptr = (Schedule *)cd.p;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type);

  step_ramp_up_set_vusers(schedule_ptr, now);
}

static void step_ramp_up_child_stagger_timer_start(Schedule *schedule_ptr, u_ns_ts_t now)
{
  ClientData cd;
  cd.p = schedule_ptr;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d, iid_mu = %u", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, schedule_ptr->iid_mu);

  // Stagger timer value
  //ab_timers[AB_TIMEOUT_RAMP].timeout_val = schedule_ptr->iid_mu;
  //dis_timer_add( AB_TIMEOUT_RAMP, schedule_ptr->phase_ramp_tmr, now, step_ramp_up_child_stagger_timer_callback, cd, 0);
  schedule_ptr->phase_ramp_tmr->actual_timeout = schedule_ptr->iid_mu;
  dis_timer_think_add( AB_TIMEOUT_RAMP, schedule_ptr->phase_ramp_tmr, now, step_ramp_up_child_stagger_timer_callback, cd, 0);
}

// Step Ramp UP
static void step_ramp_up_users(Schedule *schedule_ptr, u_ns_ts_t now)
{
  Ramp_up_schedule_phase *ramp_up_phase_ptr = &schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_cmd.ramp_up_phase;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d, ramp_up_step_users = %d, ramp_up_step_time = %d, num_vusers_or_sess = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, ramp_up_phase_ptr->ramp_up_step_users, ramp_up_phase_ptr->ramp_up_step_time, ramp_up_phase_ptr->num_vusers_or_sess);

  if(ramp_up_phase_ptr->num_vusers_or_sess == 0)
  {
    NSDL2_SCHEDULE(NULL, NULL, "No user to ramp up. Group Index = %d, Phases Index = %d, Phases Type = %d, rate = %d, num_vusers_or_sess = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, ramp_up_phase_ptr->ramp_up_rate, ramp_up_phase_ptr->num_vusers_or_sess);
    ramp_up_phase_done_fcu(schedule_ptr);
    return;
  }
  // Ramp Up Message Timer
  if(schedule_ptr->type == 0)
    start_ramp_up_msg_timer(schedule_ptr, now, 2000);   

  int total_gen = 0;
  if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO)
    total_gen = global_settings->num_generators;
  else
    total_gen = runprof_table_shr_mem[schedule_ptr->group_idx].num_generator_per_grp;


  if((!my_port_index) && (!g_generator_idx))
  {
    step_ramp_up_set_vusers(schedule_ptr, now);
  }
  else
  {
    /*Calculate iid_mu*/ 
    schedule_ptr->ramping_delta = 0;
    if (total_gen) //Controller mode
    { /*iid_mu = (step * gen_id) + (step * total_gen)* nvm_id*/ 
      schedule_ptr->iid_mu = (ramp_up_phase_ptr->ramp_up_step_time * g_generator_idx) + ((ramp_up_phase_ptr->ramp_up_step_time * total_gen) * my_port_index);
    } 
    else  
    {
      schedule_ptr->iid_mu = ramp_up_phase_ptr->ramp_up_step_time * my_port_index;
      //schedule_ptr->iid_mu = ramp_up_phase_ptr->ramp_up_step_time * global_settings->num_process;
    }
    NSDL2_SCHEDULE(NULL, NULL, "STAGGER Time: Group Index = %d, step = %d, nvm_id = %d, iid = %u, generator_id = %d, total_generator = %d", schedule_ptr->group_idx, ramp_up_phase_ptr->ramp_up_step_time, my_child_index, schedule_ptr->iid_mu, g_generator_idx, total_gen);
    step_ramp_up_child_stagger_timer_start(schedule_ptr, now);
  }
}

// --------------------- Ramp Up Rate - Linear ----------------------------

static inline void linear_ramp_up_set_vusers(Schedule *schedule_ptr, u_ns_ts_t now)
{
  Ramp_up_schedule_phase *ramp_up_phase_ptr = &schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_cmd.ramp_up_phase;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type);

  schedule_ptr->ramping_delta = get_rpm_users(schedule_ptr, 0);

  // it may possible get_rpm_users return more then users to be ramp up for this phase
  if(schedule_ptr->ramping_delta > ramp_up_phase_ptr->num_vusers_or_sess)
  {
    // Since all users are taken care, no need to start timer
    schedule_ptr->ramping_delta = ramp_up_phase_ptr->num_vusers_or_sess;
    ramp_up_phase_ptr->max_ramp_up_vuser_or_sess = schedule_ptr->ramping_delta; 
    NSDL2_SCHEDULE(NULL, NULL, "Group Index = %d, Phases Index = %d, Phases Type = %d, ramping_delta = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, schedule_ptr->ramping_delta);
  }
  else 
  {
    ramp_up_phase_ptr->max_ramp_up_vuser_or_sess = schedule_ptr->ramping_delta + ramp_up_phase_ptr->ramped_up_vusers; 
    NSDL2_SCHEDULE(NULL, NULL, "Group Index = %d, Phases Index = %d, Phases Type = %d, ramping_delta = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, schedule_ptr->ramping_delta);
    start_ramp_timer(schedule_ptr, now, 1);
  }
}

static void linear_ramp_up_child_stagger_timer_callback(ClientData cd, u_ns_ts_t now)
{
  Schedule *schedule_ptr = (Schedule *)cd.p;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type);

  linear_ramp_up_set_vusers(schedule_ptr, now);

}

static void linear_ramp_up_child_stagger_timer_start(Schedule *schedule_ptr, u_ns_ts_t now)
{
  ClientData cd;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type);
  
  cd.p = schedule_ptr;

  //ab_timers[AB_TIMEOUT_RAMP].timeout_val = (schedule_ptr->iid_mu * my_port_index) / global_settings->num_process;
  //schedule_ptr->phase_ramp_tmr->actual_timeout = (schedule_ptr->iid_mu * my_port_index) / global_settings->num_process;

  NSDL2_SCHEDULE(NULL, NULL, "Group Index = %d, Phases Index = %d, Phases Type = %d, timeout_val = %u", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, schedule_ptr->phase_ramp_tmr->actual_timeout);

  // chk timer is already running or not
  //dis_timer_add( AB_TIMEOUT_RAMP, schedule_ptr->phase_ramp_tmr, now, linear_ramp_up_child_stagger_timer_callback, cd, 0);
  dis_timer_think_add( AB_TIMEOUT_RAMP, schedule_ptr->phase_ramp_tmr, now, linear_ramp_up_child_stagger_timer_callback, cd, 0);
}

// Purpose: Ramp up users linearly. 
//
static void ramp_up_users_linear_pattern(Schedule *schedule_ptr, u_ns_ts_t now)
{
  double rpc; // rate per NVM in users/minute
  Phases *phase_ptr = &schedule_ptr->phase_array[schedule_ptr->phase_idx];
 
  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d, rate = %f, num_vusers_or_sess = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, phase_ptr->phase_type, phase_ptr->phase_cmd.ramp_up_phase.ramp_up_rate, phase_ptr->phase_cmd.ramp_up_phase.num_vusers_or_sess);

  // It is possible in some cases that it can be 0
  // For example, RAMP UP 3 users and there are 4 NVM. 
  // Then last NVM will have 0 users
  // Note - If users are 0 for a NVM, then rpc has no meaning for that NVM
  if(phase_ptr->phase_cmd.ramp_up_phase.num_vusers_or_sess == 0)
  {
    NSDL2_SCHEDULE(NULL, NULL, "No user to ramp up. Group Index = %d, Phases Index = %d, Phases Type = %d, rate = %d, num_vusers_or_sess = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, phase_ptr->phase_type, phase_ptr->phase_cmd.ramp_up_phase.ramp_up_rate, phase_ptr->phase_cmd.ramp_up_phase.num_vusers_or_sess);
    ramp_up_phase_done_fcu(schedule_ptr);
    return;
  }

  // Step 1 - Start ramp up msg timer
  if (schedule_ptr->type == 0)
    start_ramp_up_msg_timer(schedule_ptr, now, 2000);   
  
  // Step 2 - Calcualate rate per NVM (Done in parent)

  rpc = phase_ptr->phase_cmd.ramp_up_phase.rpc;
  // Step 3 - Calculate iid in milli seconds
  // If rpc is less thn 100.0/minute then we need to set ramping delta 1 
  // & v_port_entry->iid_mu will be calculated by formula ==> int(double(60000)/rpc)
  // Ramping_delta will be set when we call get_rpm_users(). 
  // It will return 1 for rpc < 100.0. as we are not calling calc_iid_ms_from_rpm()
  if ( rpc < 100.0) // Check for rpc < 0 is in parent
    schedule_ptr->iid_mu = (int)((double)(60000)/rpc);
  else
    schedule_ptr->iid_mu = (double)calc_iid_ms_from_rpm(schedule_ptr, (int)rpc);    

  if(schedule_ptr->iid_mu == 0)
  {
    error_log("Calculated iid_mu became 0.");
    END_TEST_RUN
  }
  /* NetOmni Changes: Need to calculate stagger time for each generator, hence 
   * assignment of var actual_timeout is taken out of function 
   * "linear_ramp_up_child_stagger_timer_start()"
   * Here NVM 0 of first generator(g_generator_idx = 0) starts immediately, Whereas
   * remaining NVMs(1-n) of first generator and NVMs(0-n) of generators(1-n) need to stagger 
   * hence stagger time is modified with respect to number of generators*/
  int total_gen = 0;

  if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO) 
    total_gen = global_settings->num_generators;
  else
    total_gen = runprof_table_shr_mem[schedule_ptr->group_idx].num_generator_per_grp;
  
  NSDL2_SCHEDULE(NULL, NULL, "Group Index = %d, Phases Index = %d, Phases Type = %d, rpc = %f, iid = %u, nvm_dist = %d, nvm_count = %d",
                              schedule_ptr->group_idx, schedule_ptr->phase_idx, phase_ptr->phase_type, rpc, schedule_ptr->iid_mu,
                              phase_ptr->phase_cmd.ramp_up_phase.nvm_dist_index, phase_ptr->phase_cmd.ramp_up_phase.nvm_dist_count);

  if (total_gen) //Controller mode 
  {
    schedule_ptr->phase_ramp_tmr->actual_timeout = (g_generator_idx * ((schedule_ptr->iid_mu / global_settings->num_process) / total_gen)) + ((schedule_ptr->iid_mu * my_port_index) / global_settings->num_process);
  } 
  else if (phase_ptr->phase_cmd.ramp_up_phase.nvm_dist_count) 
  {
    schedule_ptr->phase_ramp_tmr->actual_timeout = (schedule_ptr->iid_mu * phase_ptr->phase_cmd.ramp_up_phase.nvm_dist_index) / phase_ptr->phase_cmd.ramp_up_phase.nvm_dist_count;
  }
  else
  {
    schedule_ptr->phase_ramp_tmr->actual_timeout = (schedule_ptr->iid_mu * my_port_index) / global_settings->num_process;
  }
  NSDL2_SCHEDULE(NULL, NULL, "Group Index = %d, rpc = %f, iid = %u, timeout = %d, total_gen = %d, gen_idx = %d, nvm_id = %d", schedule_ptr->group_idx, rpc, schedule_ptr->iid_mu, schedule_ptr->phase_ramp_tmr->actual_timeout, total_gen, g_generator_idx, my_child_index);
 
  // Step 4 - Start timer for user generation or stagger timer
  //if((!my_port_index) && (!g_generator_idx))  // For NVM 0 - Start user generation
  if(!schedule_ptr->phase_ramp_tmr->actual_timeout)
  {
    linear_ramp_up_set_vusers(schedule_ptr, now);
  }
  else // For NVM > 0 - Start stagger timer
  {
    linear_ramp_up_child_stagger_timer_start(schedule_ptr, now);
  }
}

// ----------- Ramp UP Rate - Random
//
static void random_start_ramp_timer(Schedule *schedule_ptr, u_ns_ts_t now)
{
  ClientData cd;
  cd.p = schedule_ptr;
 
  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type);

  if (schedule_ptr->iid_mu)
    // check why we are setting periodic timer while we have non-periodic timer in update_ramp_up_vusers also ---Arun Nishad
    start_ramp_timer(schedule_ptr, now, 1);
  else
    update_ramp_up_vusers(cd, now);
}

static void random_ramp_up_child_stagger_timer_callback(ClientData cd, u_ns_ts_t now)
{
  Schedule *schedule_ptr = (Schedule *)cd.p;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type);

  random_start_ramp_timer(schedule_ptr, now);
}

static void random_ramp_up_child_stagger_timer_start(Schedule *schedule_ptr, u_ns_ts_t now)
{
  int poi_sample;
  ClientData cd;
  cd.p = schedule_ptr;
 
  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type);


  poi_sample = (int)ns_poi_sample(schedule_ptr->ns_iid_handle);
  poi_sample = (poi_sample * my_port_index)/global_settings->num_process;

  //ab_timers[AB_TIMEOUT_RAMP].timeout_val =  poi_sample;
  schedule_ptr->phase_ramp_tmr->actual_timeout = poi_sample;
  //dis_timer_add( AB_TIMEOUT_RAMP, schedule_ptr->phase_ramp_tmr, now, random_ramp_up_child_stagger_timer_callback, cd, 0);
  dis_timer_think_add( AB_TIMEOUT_RAMP, schedule_ptr->phase_ramp_tmr, now, random_ramp_up_child_stagger_timer_callback, cd, 0);
}

static void ramp_up_users_random_pattern(Schedule *schedule_ptr, u_ns_ts_t now)
{
  int i = 0;
  double rpc;
 
  Phases *phase_ptr = &schedule_ptr->phase_array[schedule_ptr->phase_idx];
 
  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d, num_vusers_or_sess = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, phase_ptr->phase_type, phase_ptr->phase_cmd.ramp_up_phase.num_vusers_or_sess);

  if(phase_ptr->phase_cmd.ramp_up_phase.num_vusers_or_sess == 0)
  {
    NSDL2_SCHEDULE(NULL, NULL, "No user to ramp up. Group Index = %d, Phases Index = %d, Phases Type = %d, rate = %f, num_vusers_or_sess = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, phase_ptr->phase_type, phase_ptr->phase_cmd.ramp_up_phase.ramp_up_rate, phase_ptr->phase_cmd.ramp_up_phase.num_vusers_or_sess);
    ramp_up_phase_done_fcu(schedule_ptr);
    return;
  }
  
  if (schedule_ptr->type == 0)
    start_ramp_up_msg_timer(schedule_ptr, now, 2000);   
  rpc = phase_ptr->phase_cmd.ramp_up_phase.rpc;

  do
  {
    i++;
    schedule_ptr->iid_mu = (double)(i*60*1000.0)/rpc;  // IID in ms 
  } while (schedule_ptr->iid_mu < 10.0);             // Min granularity is 10 ms
  schedule_ptr->ramping_delta = i;

  // ns_iid_handle sud b in structure
  schedule_ptr->ns_iid_handle = ns_poi_init(schedule_ptr->iid_mu, my_port_index);
  schedule_ptr->iid_mu = (double)random_ramp_mode_calc_max_vuser(schedule_ptr);
  
  //if ((!my_port_index) && (!g_generator_idx))
  if (!my_port_index)
    random_start_ramp_timer(schedule_ptr, now);
  else
    random_ramp_up_child_stagger_timer_start(schedule_ptr, now);
}
 
static void start_ramp_up_phase_fcu(Schedule *schedule_ptr, u_ns_ts_t now) 
{
  Phases *phase_ptr = &schedule_ptr->phase_array[schedule_ptr->phase_idx];

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phase Index = %d, Phase Type = %d, cur users = %d", 
   schedule_ptr->group_idx, schedule_ptr->phase_idx, phase_ptr->phase_type, schedule_ptr->phase_idx, schedule_ptr->cur_vusers_or_sess);

  switch(phase_ptr->phase_cmd.ramp_up_phase.ramp_up_mode) 
  {
    case RAMP_UP_MODE_IMMEDIATE:
      immediate_ramp_up_users(schedule_ptr);
      break;

    case RAMP_UP_MODE_STEP:
      step_ramp_up_users(schedule_ptr, now);
      break;

    case RAMP_UP_MODE_TIME:
    case RAMP_UP_MODE_RATE:
      if(phase_ptr->phase_cmd.ramp_up_phase.ramp_up_pattern == RAMP_UP_PATTERN_LINEAR)
       ramp_up_users_linear_pattern(schedule_ptr, now);
      else if(phase_ptr->phase_cmd.ramp_up_phase.ramp_up_pattern == RAMP_UP_PATTERN_RANDOM)
       ramp_up_users_random_pattern(schedule_ptr, now);
      break;

    default:
      fprintf(stderr, "Error: Invalid ramp up mode for Group: %d, Phase: %d\n", schedule_ptr->group_idx, schedule_ptr->phase_idx);
      break;
  }

}
// Purpose: Start ramp up phase after getting start phase message from parent
//          For both FCU and FSR
//
void start_ramp_up_phase(Schedule *schedule_ptr, u_ns_ts_t now) 
{
  int scenario_type = get_group_mode(schedule_ptr->group_idx);

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phase Index = %d, Phase Type = %d, Scenario Type = %d, cur users = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, scenario_type, schedule_ptr->cur_vusers_or_sess);

  // Initialize data struct used for calculation of iid and rpm users on start of 
  // ramp up phase
  // Then we need to call calc_iid_ms_from_rpm() after we calculate rate per nvm
  // Note - In case of FSR, rate per nvm should be cumulative rate from the start
  // of the test. For example, if we ramp up 100 session/min in step of 10, then rate
  // will be 10, 20, ...., 100. Then we ramp down 10 session/min. Then new rate will be
  // 90/min. Then again if we ramp up 50/min, then rate will be 140/min
  init_rate_intervals(schedule_ptr); 

  if(scenario_type == TC_FIX_CONCURRENT_USERS)
    start_ramp_up_phase_fcu(schedule_ptr, now);
  else  // Fix Sesson, Goal Based
    start_ramp_up_phase_fsr(schedule_ptr, now);
}

char *ns_get_schedule_phase_name_int(VUser *vptr)
{
  Schedule *schedule_ptr;
  if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO)
  {
    schedule_ptr = v_port_entry->scenario_schedule;
  }
  else if(global_settings->schedule_by == SCHEDULE_BY_GROUP)
  {
    schedule_ptr = &(v_port_entry->group_schedule[vptr->group_num]);
  }
  return schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_name;
}

int get_schedule_phase_type_int(VUser *vptr)
{
  Schedule *schedule_ptr;
  if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO)
  {
    schedule_ptr = v_port_entry->scenario_schedule;
  }
  else if(global_settings->schedule_by == SCHEDULE_BY_GROUP)
  {
    schedule_ptr = &(v_port_entry->group_schedule[vptr->group_num]);
  }
  
  return schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type; 
}
/***********************************************************************************************************/
