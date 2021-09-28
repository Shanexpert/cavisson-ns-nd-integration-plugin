
/************************************************************************************************************
 *  Name            : ns_schedule_ramp_up_fsr.c 
 *  Purpose         : To control Netstorm Ramp Down Phases
 *  Initial Version : Monday, July 06 2009
 *  Modification    : -
 ***********************************************************************************************************/

#include <regex.h>
#include <math.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"
#include "ns_msg_def.h"
#include "ns_static_vars.h"
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
#include "ns_log.h"
#include "poi.h"
#include "timing.h"
#include "ns_replay_access_logs.h"
#include "ns_schedule_ramp_down_fcu.h"
#include "ns_schedule_ramp_up_fcu.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "ns_schedule_phases_parse.h"
#include "ns_schedule_ramp_up_fsr.h"


double round(double x);

static int *num_nvm_users_shm = NULL;
static int *per_nvm_shm_index_ptr;

void init_user_num_shm()
{
  NSDL2_SCHEDULE(NULL, NULL, "Method called.");
  
  per_nvm_shm_index_ptr = &num_nvm_users_shm[my_port_index * total_runprof_entries]; 
}

// Create shared memory, which will have the num users per child & per group
// TODO - Support For Atom write in Netstorm --- Arun Nishad
void create_user_num_shm()
{
  // int i, j;
  NSDL2_SCHEDULE(NULL, NULL, "Method called. Creating shared memory.");
  num_nvm_users_shm = (int *) do_shmget(sizeof(int) * global_settings->num_process*total_runprof_entries, "Num_NVM_shm");

  memset(num_nvm_users_shm, 0, sizeof(int) * global_settings->num_process*total_runprof_entries);
}

void incr_nvm_users(int grp_index)
{
  NSDL4_SCHEDULE(NULL, NULL, "Method called, Incrementing num_nvm_users_shm(%u) for grp_index = %d", per_nvm_shm_index_ptr[grp_index], grp_index);

  ++(per_nvm_shm_index_ptr[grp_index]);
}

void decr_nvm_users(int grp_index)
{
  NSDL4_SCHEDULE(NULL, NULL, "Method called, Decrementing num_nvm_users_shm(%u) for grp_index = %d", per_nvm_shm_index_ptr[grp_index], grp_index);

  --(per_nvm_shm_index_ptr[grp_index]);
}

// Returns total user of a group 
static unsigned int get_group_vusers(int grp_index)
{
  int child_index;
  unsigned int tot_user = 0;
  int *local_per_nvm_shm_index_ptr;
  

  NSDL2_SCHEDULE(NULL, NULL, "Method called.");

  if(grp_index == -1)
   grp_index = 0;

  for (child_index = 0; child_index < global_settings->num_process; child_index++)
  {
    local_per_nvm_shm_index_ptr = &num_nvm_users_shm[child_index * total_runprof_entries];
    tot_user +=  local_per_nvm_shm_index_ptr[grp_index];
  }

  return tot_user;
}

// Returns total user of all groups 
static unsigned int get_total_vusers()
{
  int child_index, group_index;
  unsigned int tot_user = 0;
  int *local_per_nvm_shm_index_ptr;

  NSDL2_SCHEDULE(NULL, NULL, "Method called.");

  for (child_index = 0; child_index < global_settings->num_process; child_index++)
  {
    local_per_nvm_shm_index_ptr = &num_nvm_users_shm[child_index * total_runprof_entries];
    for(group_index = 0; group_index < total_runprof_entries; group_index++)
       tot_user += local_per_nvm_shm_index_ptr[group_index];
  }

  return tot_user;
}

// max user limit can be at group/global level so we stop user generation if any level limit hits
static void control_user_limit(unsigned int *num_users, Schedule *schedule_ptr)
{
  int grp_level_user_limit;
  int over_all_users = 0;
  int tot_grp_users = 0;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d, num_users = %u", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, *num_users);

  if(global_settings->max_user_limit < INT_MAX)
  {
    over_all_users = get_total_vusers();
    if(over_all_users < global_settings->max_user_limit)
    {
      if((*num_users + over_all_users) > global_settings->max_user_limit)
        *num_users = global_settings->max_user_limit - over_all_users; 
    }
    else
     *num_users = 0;

    NSDL2_SCHEDULE(NULL, NULL, "Global max user limit(%u): Group Index = %d, Phases Index = %d, Phases Type = %d, num_users = %u, over_all_users = %u", global_settings->max_user_limit, schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, *num_users, over_all_users);
 
  }

  if (global_settings->schedule_by == SCHEDULE_BY_GROUP)
  {
     grp_level_user_limit = runprof_table_shr_mem[schedule_ptr->group_idx].gset.grp_max_user_limit;
     if (grp_level_user_limit < INT_MAX)
     {
        tot_grp_users = get_group_vusers(schedule_ptr->group_idx); 
        if(tot_grp_users < grp_level_user_limit)
        {
          if((*num_users + tot_grp_users) > grp_level_user_limit)
            *num_users = grp_level_user_limit - tot_grp_users;
        }
        else
           *num_users = 0;
     }
     NSDL2_SCHEDULE(NULL, NULL, "Group max user limit(%u): Group Index = %d, Phases Index = %d, Phases Type = %d, num_users = %u, tot_grp_users = %u", grp_level_user_limit, schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, *num_users, tot_grp_users);
  }
    
}


/* Genarte users using a poisson Inter Arrival Time
 * distribution. Mean of this distribution has earlier been
 * set
 * Need To support for Replay Access Logs Also  --- Arun Nishad
 */
void generate_users( ClientData cd, u_ns_ts_t now)
{
  int poi_sample=0;
  unsigned int num_users=1;
  double surplus_user_ratio = 0;
  Schedule *schedule_ptr = (Schedule *)cd.p;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type);

  // For SImple & scenario based we need to go in nxt phase
  if((sigterm_received &&
       !(global_settings->schedule_by == SCHEDULE_BY_SCENARIO && 
       global_settings->schedule_type == SCHEDULE_TYPE_SIMPLE))) 
    return;

  if(global_settings->user_rate_mode == SESSION_RATE_RANDOM)  // Random
  {
    while (1) {
      poi_sample = (int)ns_poi_sample(schedule_ptr->ns_iid_handle);
      if (!poi_sample)
        num_users++;
      else
        break;
    }
    num_users *= schedule_ptr->ramping_delta; 
  } else {
    poi_sample = schedule_ptr->iid_mu;
    NSDL2_SCHEDULE(NULL, NULL, "adjust_rampup_timer = %d, global_settings = %p, timer_started_at = %u, schedule_ptr->actual_timeout = %d, "
                               "now = %llu", global_settings->adjust_rampup_timer, global_settings, cd.timer_started_at, 
                                             schedule_ptr->actual_timeout, now);
    if(global_settings->adjust_rampup_timer && (cd.timer_started_at != 0) && (now > cd.timer_started_at))
    {
      surplus_user_ratio = (double)((double)(now - cd.timer_started_at) / (double)schedule_ptr->actual_timeout); 
      surplus_user_ratio = round(surplus_user_ratio);
    }
    else
      surplus_user_ratio = 0;

    NSDL2_SCHEDULE(NULL, NULL, "surplus_user_ratio = %f", surplus_user_ratio);
    schedule_ptr->ramping_delta = num_users = get_rpm_users(schedule_ptr, (int)surplus_user_ratio);
  }
  control_user_limit(&num_users, schedule_ptr);

  if(num_users > 0)
  {
    NSDL2_SCHEDULE(NULL, NULL, "Group Index = %d, Phases Index = %d, Phases Type = %d, num_users = %d, poi_sample = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, num_users, poi_sample);
    // in case of FSR, user will always complete session and exit. So passing 1 but it is not used in the function for FSR
    new_user(num_users, now, schedule_ptr->group_idx, 1, NULL, NULL);
  }

  if(poi_sample >= 0)
  {
    //ab_timers[AB_TIMEOUT_RAMP].timeout_val = poi_sample;
    schedule_ptr->phase_ramp_tmr->actual_timeout = poi_sample;
    /* Issue - Since sometime generate_users callback called after expected timeout and hence session rate not achived 
               To resolve this issue we save actual expected time and take a diff from real calling time and calculate ratio of surplus users*/
    cd.timer_started_at = now + schedule_ptr->phase_ramp_tmr->actual_timeout;
    schedule_ptr->actual_timeout = schedule_ptr->phase_ramp_tmr->actual_timeout;
    //dis_timer_add( AB_TIMEOUT_RAMP, schedule_ptr->phase_ramp_tmr, now, generate_users, cd, 0);
    NSDL2_SCHEDULE(NULL, NULL, "Add callback generate_users: actual_timeout = %d, cd.timer_started_at = %llu", 
                                 schedule_ptr->phase_ramp_tmr->actual_timeout, cd.timer_started_at);
    dis_timer_think_add( AB_TIMEOUT_RAMP, schedule_ptr->phase_ramp_tmr, now, generate_users, cd, 0);
  }
}

//step timer callback, which is called at every given ramp_up_step_time 
static void session_rate_ramp_up_step_stagger_timer_callback(ClientData cd, u_ns_ts_t now)
{
  Schedule *schedule_ptr = (Schedule *)cd.p;
  double sess_rpc;
  Ramp_up_schedule_phase *ramp_up_phase_ptr = &schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_cmd.ramp_up_phase;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d, max_ramp_up_vuser_or_sess = %d, num_vusers_or_sess = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, ramp_up_phase_ptr->max_ramp_up_vuser_or_sess, ramp_up_phase_ptr->num_vusers_or_sess);

  // Check if final session rate achieved.
  // If yes, stop step timer, send ramp up done msg and return
/*  if(ramp_up_phase_ptr->max_ramp_up_vuser_or_sess == ramp_up_phase_ptr->num_vusers_or_sess)   
  {
    //ramp_up_done_for_sessions(now, 0);
    //message time
    stop_phase_end_timer(schedule_ptr);
    send_ramp_up_done_msg(schedule_ptr);
    send_phase_complete(schedule_ptr);
  }
  else
*/
  {
    // Stop Ramp timer as we are going to increase session rate
    // Delete for this group only ---Arun Nishad
    stop_phase_ramp_timer(schedule_ptr);
    // Increase session rate
    if((ramp_up_phase_ptr->max_ramp_up_vuser_or_sess + ramp_up_phase_ptr->session_rate_per_step) >= ramp_up_phase_ptr->num_vusers_or_sess)
    {
      schedule_ptr->cur_vusers_or_sess += ramp_up_phase_ptr->num_vusers_or_sess - ramp_up_phase_ptr->max_ramp_up_vuser_or_sess; 
      ramp_up_phase_ptr->max_ramp_up_vuser_or_sess += ramp_up_phase_ptr->num_vusers_or_sess - ramp_up_phase_ptr->max_ramp_up_vuser_or_sess;
      stop_phase_end_timer(schedule_ptr);
      if (schedule_ptr->type == 0)
        send_ramp_up_done_msg(schedule_ptr);
      send_phase_complete(schedule_ptr);
      /*FSR: In order to maintain session rate per minute the ramp timer is added to timer link list and executed at defined interval, 
             later in ramp down phase these timers are deleted.
        In case of RTC: On obtaining desired session rate for runtime phase stop timers as for now main schedule phase will be updated 
        and will be generating sessions (Existing + RTC sessions)
      */
      if (schedule_ptr->type == 1)
        return;
    }
    else
    {
      ramp_up_phase_ptr->max_ramp_up_vuser_or_sess += ramp_up_phase_ptr->session_rate_per_step;
      schedule_ptr->cur_vusers_or_sess +=  ramp_up_phase_ptr->session_rate_per_step;
      if (schedule_ptr->type == 0)
        send_ramp_up_msg(cd, now);
    }

    sess_rpc = schedule_ptr->cur_vusers_or_sess/SESSION_RATE_MULTIPLE;
    
    // We need to reset var as whenever calc_iid_ms_from_rpm is called,
    // as get rpm users() is directly propotional to, number of times calc iid ms from rpm called
    init_rate_intervals(schedule_ptr);
    if(sess_rpc < 100.0)
      schedule_ptr->iid_mu = (int)((double)(60000)/sess_rpc); 
    else
      schedule_ptr->iid_mu = calc_iid_ms_from_rpm(schedule_ptr, (int)sess_rpc);

    NSDL2_SCHEDULE(NULL, NULL, "Group Index = %d, Phases Index = %d, Phases Type = %d, max_ramp_up_vuser_or_sess = %d, session_rate_per_step = %d, cur_vusers_or_sess = %d, sess_rpc = %f, iid_mu = %u", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, ramp_up_phase_ptr->max_ramp_up_vuser_or_sess, ramp_up_phase_ptr->session_rate_per_step, schedule_ptr->cur_vusers_or_sess, sess_rpc, schedule_ptr->iid_mu);

    if(global_settings->user_rate_mode == SESSION_RATE_RANDOM) //Random 
      schedule_ptr->ns_iid_handle = ns_poi_init(schedule_ptr->iid_mu, my_port_index);

    // Generate users using new iid
    generate_users(cd, now);
  }
}   

static void start_session_ramp_up_step_timer(Schedule *schedule_ptr, u_ns_ts_t now)
{
  NSDL2_SCHEDULE(NULL, NULL, "Method called.");
  ClientData cd;
  Phases *phase_ptr = &schedule_ptr->phase_array[schedule_ptr->phase_idx];

  memset(&cd, 0, sizeof(ClientData));
  cd.p = schedule_ptr;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, phase_ptr->phase_type);

  if(!my_port_index)
    session_rate_ramp_up_step_stagger_timer_callback(cd, now);

  // ab_timers[AB_TIMEOUT_END].timeout_val = phase_ptr->phase_cmd.ramp_up_phase.ramp_up_step_time*1000;  //setting timer value for each step to run
  schedule_ptr->phase_end_tmr->actual_timeout = phase_ptr->phase_cmd.ramp_up_phase.ramp_up_step_time*1000; 
  //dis_timer_add( AB_TIMEOUT_END, schedule_ptr->phase_end_tmr, now, session_rate_ramp_up_step_stagger_timer_callback, cd, 1);
  dis_timer_think_add( AB_TIMEOUT_END, schedule_ptr->phase_end_tmr, now, session_rate_ramp_up_step_stagger_timer_callback, cd, 1);
}

static void start_session_ramp_up_timer_immediate(ClientData cd, u_ns_ts_t now)
{

  Schedule *schedule_ptr = (Schedule *)cd.p;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type);

  generate_users(cd, now);
  //Send Phases Complete phases
  if (schedule_ptr->type == 0)
    send_ramp_up_done_msg(schedule_ptr);
  send_phase_complete(schedule_ptr); 
}
// Immediate Session Rate Function -- Start
void immediate_session_rate_child_stagger_timer_callback(ClientData cd, u_ns_ts_t now)
{
  NSDL2_SCHEDULE(NULL, NULL, "Method called. now = %u", now);

  start_session_ramp_up_timer_immediate(cd, now);
}

static void immediate_session_rate_ramp_up_child_stagger_timer_start(ClientData cd, u_ns_ts_t now)
{
  double stagger_time;
  Schedule *schedule_ptr = (Schedule *)cd.p;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type);

  stagger_time = (schedule_ptr->iid_mu * my_port_index)/global_settings->num_process;

  //ab_timers[AB_TIMEOUT_END].timeout_val = stagger_time;
  schedule_ptr->phase_end_tmr->actual_timeout = stagger_time;
  //dis_timer_add( AB_TIMEOUT_END, schedule_ptr->phase_end_tmr, now, immediate_session_rate_child_stagger_timer_callback, cd, 0);
  dis_timer_think_add( AB_TIMEOUT_END, schedule_ptr->phase_end_tmr, now, immediate_session_rate_child_stagger_timer_callback, cd, 0);
}

// Immediate Ramp Up Sessions  
void const_ramp_up_session_rate_immediate(Schedule *schedule_ptr, u_ns_ts_t now)
{
  ClientData cd;
  double sess_rpc;
 
  memset(&cd, 0, sizeof(ClientData));

  cd.p = schedule_ptr;
  Phases *phase_ptr = &schedule_ptr->phase_array[schedule_ptr->phase_idx];
  Ramp_up_schedule_phase *ramp_up_phase_ptr = &phase_ptr->phase_cmd.ramp_up_phase;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, phase_ptr->phase_type);

  if(ramp_up_phase_ptr->num_vusers_or_sess == 0)
  {
    NSDL2_SCHEDULE(NULL, NULL, "Session became 0, Group Index = %d, Phases Index = %d, Phases Type = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, phase_ptr->phase_type);
    if (schedule_ptr->type == 0)
      send_ramp_up_done_msg(schedule_ptr);
    send_phase_complete(schedule_ptr); 
    return;
  }

  ramp_up_phase_ptr->max_ramp_up_vuser_or_sess = ramp_up_phase_ptr->num_vusers_or_sess;
  schedule_ptr->cur_vusers_or_sess +=  ramp_up_phase_ptr->max_ramp_up_vuser_or_sess;
  sess_rpc = schedule_ptr->cur_vusers_or_sess/SESSION_RATE_MULTIPLE;

  if ( sess_rpc < 100.0) 
   schedule_ptr->iid_mu = (int)((double)(60000)/sess_rpc); 
  else
   schedule_ptr->iid_mu = calc_iid_ms_from_rpm(schedule_ptr, (int)sess_rpc);

  NSDL2_SCHEDULE(NULL, NULL, "Group Index = %d, Phases Index = %d, Phases Type = %d, max_ramp_up_vuser_or_sess = %d, num_vusers_or_sess = %d, cur_vusers_or_sess = %d, iid_mu = %d, sess_rpc = %f", schedule_ptr->group_idx, schedule_ptr->phase_idx, phase_ptr->phase_type, ramp_up_phase_ptr->max_ramp_up_vuser_or_sess, ramp_up_phase_ptr->num_vusers_or_sess, schedule_ptr->cur_vusers_or_sess, schedule_ptr->iid_mu, sess_rpc);

  if (!my_port_index)
    start_session_ramp_up_timer_immediate(cd, now);
  else if(sess_rpc < 100.0)  // Do not stagger if session rate is too low
    immediate_session_rate_child_stagger_timer_callback(cd, now);
  else
    immediate_session_rate_ramp_up_child_stagger_timer_start(cd, now);
}

void random_ramp_up_session_rate_immediate(Schedule *schedule_ptr, u_ns_ts_t now)
{
  int i=0;
  double sess_rpc;
  ClientData cd;

  memset(&cd, 0, sizeof(ClientData));

  cd.p = schedule_ptr;
  Ramp_up_schedule_phase *ramp_up_phase_ptr = &schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_cmd.ramp_up_phase;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d, num_vusers_or_sess = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, ramp_up_phase_ptr->num_vusers_or_sess);

  if(ramp_up_phase_ptr->num_vusers_or_sess == 0)
  {
    NSDL2_SCHEDULE(NULL, NULL, "Session became 0, Group Index = %d, Phases Index = %d, Phases Type = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type);
    if (schedule_ptr->type == 0)
      send_ramp_up_done_msg(schedule_ptr);
    send_phase_complete(schedule_ptr); 
    return;
  }

  ramp_up_phase_ptr->max_ramp_up_vuser_or_sess = ramp_up_phase_ptr->num_vusers_or_sess;
  schedule_ptr->cur_vusers_or_sess +=  ramp_up_phase_ptr->max_ramp_up_vuser_or_sess;
  sess_rpc = schedule_ptr->cur_vusers_or_sess/SESSION_RATE_MULTIPLE;

  do
  {
    i++;
    schedule_ptr->iid_mu =(double)((double)(60000.0*i)/sess_rpc);
  } while (schedule_ptr->iid_mu < 10.0);
  schedule_ptr->ramping_delta = i;

  NSDL2_SCHEDULE(NULL, NULL, "Group Index = %d, Phases Index = %d, Phases Type = %d, max_ramp_up_vuser_or_sess = %d, cur_vusers_or_sess = %d, sess_rpc = %f, ramping_delta = %d, iid_mu = %u", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, ramp_up_phase_ptr->max_ramp_up_vuser_or_sess, schedule_ptr->cur_vusers_or_sess, sess_rpc, schedule_ptr->ramping_delta, schedule_ptr->iid_mu);

  schedule_ptr->ns_iid_handle = ns_poi_init(schedule_ptr->iid_mu, my_port_index);

  if (!my_port_index)
    start_session_ramp_up_timer_immediate(cd, now);
  else if(sess_rpc < 100.0)  // Do not stagger if session rate is too low
    immediate_session_rate_child_stagger_timer_callback(cd, now);
  else
    immediate_session_rate_ramp_up_child_stagger_timer_start(cd, now);
}
// Immediate Session Rate Function -- Ends

// TIME_SESSIONS Session Rate Function -- Start

static void calc_ramp_up_session_rate_per_step(Schedule *schedule_ptr)
{
  double temp;  //temporary variable used to calculate rounded value
  Phases *phase_ptr = &schedule_ptr->phase_array[schedule_ptr->phase_idx];

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d, num_vusers_or_sess = %d, tot_num_steps_for_sessions = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, phase_ptr->phase_cmd.ramp_up_phase.num_vusers_or_sess, phase_ptr->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions);

  temp = (double)(phase_ptr->phase_cmd.ramp_up_phase.num_vusers_or_sess)/(double)phase_ptr->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions;
  phase_ptr->phase_cmd.ramp_up_phase.session_rate_per_step = round(temp);

  //Removed calculation of sess_rpc & iid_mu from here as it was scerwing iid_mu by calculating
  //iid_mu on the basis of session_rate_per_step causing issues in Advanced scenario, 
  //sess_rpc & iid_mu is correctly getting calulated in the callback based on cur_vusers_or_sessions

  NSDL2_SCHEDULE(NULL, NULL, "Group Index = %d, Phases Index = %d, Phases Type = %d, temp = %f, session_rate_per_step = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, temp, phase_ptr->phase_cmd.ramp_up_phase.session_rate_per_step);
}

//For NVMs(1-n), callback function which generator sessions and then apply periodic timer for staggering time
//calculated with respect to ramp up step time  
static void session_rate_ramp_up_step_nvm_stagger_timer_callback(ClientData cd, u_ns_ts_t now)
{
  Schedule *schedule_ptr = (Schedule *)cd.p;
  Phases *phase_ptr = &schedule_ptr->phase_array[schedule_ptr->phase_idx];

  NSDL2_SCHEDULE(NULL, NULL, "Method called");
  session_rate_ramp_up_step_stagger_timer_callback(cd, now);

  //Calculate staggering time for NVMs with regard to ramp up step time
  schedule_ptr->phase_end_tmr->actual_timeout = phase_ptr->phase_cmd.ramp_up_phase.ramp_up_step_time*1000;

  NSDL2_SCHEDULE(NULL, NULL, "NVM id=%d, Number of process=%d, step time=%d, actual_timeout=%d\n", 
      my_port_index, global_settings->num_process, phase_ptr->phase_cmd.ramp_up_phase.ramp_up_step_time, schedule_ptr->phase_end_tmr->actual_timeout);
  dis_timer_think_add( AB_TIMEOUT_END, schedule_ptr->phase_end_tmr, now, session_rate_ramp_up_step_stagger_timer_callback, cd, 1);
}

static void session_rate_ramp_up_child_stagger_timer_callback(ClientData cd, u_ns_ts_t now)
{
  Schedule *schedule_ptr = (Schedule *)cd.p;
  start_session_ramp_up_step_timer(schedule_ptr, now);
}

static void session_rate_ramp_up_child_stagger_timer_start(Schedule *schedule_ptr, u_ns_ts_t now, double sess_rpc)
{
  ClientData cd;

  memset(&cd, 0, sizeof(ClientData));

  cd.p = schedule_ptr;
  Phases *phase_ptr = &schedule_ptr->phase_array[schedule_ptr->phase_idx];
  u_ns_ts_t iid_mu;               // Calculating local iid_mu for calculating stagerring time only

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d, sess_rpc = %f", 
                   schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, sess_rpc);
  //In kohls, test was running with SESSION_RATE_RANDOM and sess_rpc became greater than 100
  //and in random local variable iid_mu is not initialze. Therefore NVM's were not able to generate sessions.  
  //if(global_settings->user_rate_mode == SESSION_RATE_CONSTANT) //Const
  {
    if(sess_rpc < 100.0)
      iid_mu = (int)((double)(60000)/sess_rpc); 
    else
      iid_mu =(double)init_n_calc_iid_ms_from_rpm(schedule_ptr, sess_rpc);
  }
 
  // For session rate per child greater than 100 then apply non periodic initial staggering timer using iid_mu and NVM id
  // No users are generated at this point or after this initial stagger timer is over
  // After this stagger time, periodic timer stagger time is started wrt ramp-up step and NVM ID
  // Users are generated only after first periodic stagger time is over and then after we generate users with respect to ramp up step time
  //   Stagger Using iid_mu -> Stagger using step -> Generate Users -> Stagger using step -> Generate Users -> ....
  if(sess_rpc > 100.0)   // per minute
  {
    schedule_ptr->phase_end_tmr->actual_timeout = (iid_mu * my_port_index)/global_settings->num_process;
    NSDL2_SCHEDULE(NULL, NULL, "Group Index = %d, Phases Index = %d, Phases Type = %d, iid_mu = %u, stagger time = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, iid_mu, schedule_ptr->phase_end_tmr->actual_timeout);
    //dis_timer_add( AB_TIMEOUT_END, schedule_ptr->phase_end_tmr, now, session_rate_ramp_up_child_stagger_timer_callback, cd, 0);
    dis_timer_think_add( AB_TIMEOUT_END, schedule_ptr->phase_end_tmr, now, session_rate_ramp_up_child_stagger_timer_callback, cd, 0);
  } else { 
  // For session rate per child less than 100 then apply non periodic initial staggering timer using ramp up step time and NVM id
  // Users are generated and after this initial stagger timer is over
  // After this stagger time, periodic timer stagger time is started with respect to ramp-up step and NVM ID
  //   Stagger Using ramp up step time -> Generate Users -> Stagger using step -> Generate Users -> Stagger using step -> Generate Users -> ....
  // TODO - Keep it in ms is easy and all places where we log, we log as ms
    schedule_ptr->phase_end_tmr->actual_timeout = ((phase_ptr->phase_cmd.ramp_up_phase.ramp_up_step_time*my_port_index*1000)/global_settings->num_process);

    NSDL2_SCHEDULE(NULL, NULL, "NVM id=%d, Number of process=%d, Ramp-up step time=%d sec, Stagger time = %d", 
          my_child_index, global_settings->num_process, phase_ptr->phase_cmd.ramp_up_phase.ramp_up_step_time, schedule_ptr->phase_end_tmr->actual_timeout);
    dis_timer_think_add( AB_TIMEOUT_END, schedule_ptr->phase_end_tmr, now, session_rate_ramp_up_step_nvm_stagger_timer_callback, cd, 0);
  } 
}


void const_ramp_up_session_rate_with_steps(Schedule *schedule_ptr, u_ns_ts_t now)
{
  double sess_rpc;
  Ramp_up_schedule_phase *ramp_up_phase_ptr = &schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_cmd.ramp_up_phase;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type);

  if(ramp_up_phase_ptr->num_vusers_or_sess == 0)
  {
    NSDL2_SCHEDULE(NULL, NULL, "Session became 0, Group Index = %d, Phases Index = %d, Phases Type = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type);
  if (schedule_ptr->type == 0)
    send_ramp_up_done_msg(schedule_ptr);
    send_phase_complete(schedule_ptr); 
    return;
  }

  calc_ramp_up_session_rate_per_step(schedule_ptr);

  // TODO -- What if sess_rpc became 0
  sess_rpc = (ramp_up_phase_ptr->session_rate_per_step)/SESSION_RATE_MULTIPLE;

  if(!my_port_index)
    start_session_ramp_up_step_timer(schedule_ptr, now);
  else  
    session_rate_ramp_up_child_stagger_timer_start(schedule_ptr, now, sess_rpc);
}

void random_ramp_up_session_rate_with_steps(Schedule *schedule_ptr, u_ns_ts_t now)
{
  int i = 0;
  double sess_rpc;
  Ramp_up_schedule_phase *ramp_up_phase_ptr = &schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_cmd.ramp_up_phase;
  Phases *phase_ptr = &schedule_ptr->phase_array[schedule_ptr->phase_idx];

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type);

  if(ramp_up_phase_ptr->num_vusers_or_sess == 0)
  {
    NSDL2_SCHEDULE(NULL, NULL, "Session became 0, Group Index = %d, Phases Index = %d, Phases Type = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, phase_ptr->phase_type);
    if (schedule_ptr->type == 0)
      send_ramp_up_done_msg(schedule_ptr);
    send_phase_complete(schedule_ptr); 
    return;
  }

  calc_ramp_up_session_rate_per_step(schedule_ptr);

  // TODO -- What if sess_rpc became 0
  sess_rpc = (phase_ptr->phase_cmd.ramp_up_phase.session_rate_per_step)/SESSION_RATE_MULTIPLE;
  // Calculate iid using per step session rate
  do
  {
    i++;
    schedule_ptr->iid_mu =(double)((double)(60000.0*i)/sess_rpc);
  } while (schedule_ptr->iid_mu < 10.0);
  schedule_ptr->ramping_delta = i;

  NSDL2_SCHEDULE(NULL, NULL, "Group Index = %d, Phases Index = %d, Phases Type = %d, max_ramp_up_vuser_or_sess = %d, session_rate_per_step = %d, cur_vusers_or_sess = %d, sess_rpc = %f, iid_mu = %u", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, ramp_up_phase_ptr->max_ramp_up_vuser_or_sess, ramp_up_phase_ptr->session_rate_per_step, schedule_ptr->cur_vusers_or_sess, sess_rpc, schedule_ptr->iid_mu);

  // If child is not 0, then start stagger timer  and return
  if(!my_port_index)
    start_session_ramp_up_step_timer(schedule_ptr, now);
  else
    session_rate_ramp_up_child_stagger_timer_start(schedule_ptr, now, sess_rpc);
}

void start_ramp_up_phase_fsr(Schedule *schedule_ptr, u_ns_ts_t now) 
{
  Phases *phase_ptr = &schedule_ptr->phase_array[schedule_ptr->phase_idx];

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phase Index = %d, Phase Type = %d, cur users = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, schedule_ptr->cur_vusers_or_sess);

  switch(phase_ptr->phase_cmd.ramp_up_phase.ramp_up_mode) 
  {
    case RAMP_UP_MODE_IMMEDIATE:
      if(global_settings->user_rate_mode == SESSION_RATE_CONSTANT)
        const_ramp_up_session_rate_immediate(schedule_ptr, now);
      else
        random_ramp_up_session_rate_immediate(schedule_ptr, now);
      break;

    case RAMP_UP_MODE_TIME_SESSIONS:
      if(global_settings->user_rate_mode == SESSION_RATE_CONSTANT)   // constt
        const_ramp_up_session_rate_with_steps(schedule_ptr, now);
      else  // Random 
        random_ramp_up_session_rate_with_steps(schedule_ptr, now);
       break;

    default:
      fprintf(stderr, "Error: Invalid ramp up mode for Group: %d, Phase: %d\n", schedule_ptr->group_idx, schedule_ptr->phase_idx);
      break;
  }

}
