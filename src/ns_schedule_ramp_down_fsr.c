
/************************************************************************************************************
 *  Name            : ns_schedule_ramp_down_fsr.c 
 *  Purpose         : To control Netstorm Ramp Down Phases
 *  Initial Version : Monday, July 06 2009
 *  Modification    : -
 ***********************************************************************************************************/

#include <math.h>
#include <regex.h>

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
#include "timing.h"
#include "tmr.h"
#include "util.h"
#include "ns_log.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "ns_schedule_phases.h"
#include "ns_schedule_ramp_down_fsr.h"
#include "ns_schedule_ramp_down_fcu.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "ns_schedule_ramp_up_fsr.h"
#include "poi.h"
#include "ns_schedule_ramp_up_fcu.h"
#include "ns_schedule_ramp_up_fsr.h"
#include "ns_schedule_phases_parse.h"
#include "ns_percentile.h"
#include "ns_vuser.h"

double round(double x);

// Searches and remove user available in the system, if grp based then delete only group user else all available
void search_and_remove_user_for_fsr(Schedule *schedule_ptr, u_ns_ts_t now)
{
  int i = 0;
  VUser *cur_vptr, *next_vptr;

  NSDL2_SCHEDULE(NULL, NULL, "Method called, group_idx = %d", schedule_ptr->group_idx);

  cur_vptr = gBusyVuserHead;
  for(;;)
  {
    if(cur_vptr ==  NULL)
        break;
  
    next_vptr = cur_vptr->busy_next;  // Save here as cur_vptr may get cleaned up
   
    if(schedule_ptr->group_idx != -1)
    {
      if (cur_vptr->group_num != schedule_ptr->group_idx) 
      {
        NSDL2_SCHEDULE(cur_vptr, NULL, "Continueing as group num did not match, cur_vptr->group_num = %d, group_number = %d", 
                                 cur_vptr->group_num, schedule_ptr->group_idx);
        cur_vptr = next_vptr;
        continue;
      }
      else
        NSDL2_SCHEDULE(cur_vptr, NULL, "Group is matched,  cur_vptr->group_num = %d, group_number = %d",
                                        cur_vptr->group_num, schedule_ptr->group_idx);
    }

    if(cur_vptr->vuser_state == NS_VUSER_CLEANUP)  // Ramp Down only Users for group_number or Not in clean up stat
    {
      NSDL2_SCHEDULE(cur_vptr, NULL, "Continueing as User(%p) is in clean up stat.", cur_vptr);
      cur_vptr = next_vptr;
      continue;
    }

    if(!(cur_vptr->flags & NS_VUSER_RAMPING_DOWN))
    {
      cur_vptr->flags |= NS_VUSER_RAMPING_DOWN; 
      VUSER_INC_EXIT(cur_vptr);
    }
    else
    {
      NSDL3_SCHEDULE(cur_vptr, NULL, "User is already marked as ramped down, i = %d, vuser_state = %d, max_con_per_vuser = %d, gNumVuserActive = %d, gNumVuserThinking = %d, gNumVuserWaiting = %d, gNumVuserCleanup = %d", i, cur_vptr->vuser_state, global_settings->max_con_per_vuser, gNumVuserActive, gNumVuserThinking, gNumVuserWaiting, gNumVuserCleanup);
      cur_vptr = next_vptr;
      continue;
    }

    // cur_vusers_or_sess should be decrease here

    i++;  // Number of users has to ramped down

    NSDL3_SCHEDULE(cur_vptr, NULL, "Stopping user . i = %d, vuser_state = %d, max_con_per_vuser = %d, gNumVuserActive = %d, gNumVuserThinking = %d, gNumVuserWaiting = %d, gNumVuserCleanup = %d", i, cur_vptr->vuser_state, global_settings->max_con_per_vuser, gNumVuserActive, gNumVuserThinking, gNumVuserWaiting, gNumVuserCleanup);

    // mark this user to be ramped down
  if(cur_vptr->vuser_state != NS_VUSER_SYNCPOINT_WAITING)
  {
    if(runprof_table_shr_mem[cur_vptr->group_num].gset.rampdown_method.mode == RDM_MODE_ALLOW_CURRENT_SESSION_COMPLETE)  //Allow curent session to complete
      stop_user_and_allow_cur_sess_complete(cur_vptr, now);
    else if(runprof_table_shr_mem[cur_vptr->group_num].gset.rampdown_method.mode == RDM_MODE_ALLOW_CURRENT_PAGE_COMPLETE)  //Allow curent page to complete 
      stop_user_and_allow_current_page_to_complete(cur_vptr, now);
    else if(runprof_table_shr_mem[cur_vptr->group_num].gset.rampdown_method.mode == RDM_MODE_STOP_IMMEDIATELY)  // Stop user immediate with status
      stop_user_immediately(cur_vptr,  now);
  }

    cur_vptr = next_vptr;
    // If user is marked then we need to get group number again for nxt user by generate scen group num()
  }
}

void ramp_down_session_rate_immediate(Schedule *schedule_ptr, u_ns_ts_t now)
{
  ClientData cd;
  double sess_rpc;
  cd.p = schedule_ptr;
  Phases *phase_ptr = &schedule_ptr->phase_array[schedule_ptr->phase_idx];
  Ramp_down_schedule_phase *ramp_down_phase_ptr = &phase_ptr->phase_cmd.ramp_down_phase;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx]);

  // this deletes the timer for genearet_users(), so that we can continue user generation with new iid_mu & reduced rate
  stop_ramp_down_timer(schedule_ptr->phase_ramp_tmr);
  ramp_down_phase_ptr->max_ramp_down_vuser_or_sess = ramp_down_phase_ptr->num_vusers_or_sess;   
  schedule_ptr->cur_vusers_or_sess -= ramp_down_phase_ptr->max_ramp_down_vuser_or_sess;
  sess_rpc = schedule_ptr->cur_vusers_or_sess/SESSION_RATE_MULTIPLE;
  
  if(schedule_ptr->type == 0)
    send_ramp_down_done_msg(schedule_ptr);

  send_phase_complete(schedule_ptr);

  search_and_remove_user_for_fsr(schedule_ptr, now);

  // If all session are ramped downed then we need not to generate users
  if(sess_rpc < 0.1) 
    return;

  if(sess_rpc < 100.0)
    schedule_ptr->iid_mu = (int)((double)(60000)/sess_rpc); 
  else
    schedule_ptr->iid_mu = calc_iid_ms_from_rpm(schedule_ptr, (int)sess_rpc);

  if(global_settings->user_rate_mode == SESSION_RATE_RANDOM)
    schedule_ptr->ns_iid_handle = ns_poi_init(schedule_ptr->iid_mu, my_port_index);

  generate_users(cd, now);
}

//step timer callback, which is called at every given session_ramp_down_step_time 
static void session_rate_ramp_down_step_stagger_timer_callback(ClientData cd, u_ns_ts_t now)
{
  Schedule *schedule_ptr = (Schedule *)cd.p;
  double sess_rpc;
  Phases *phase_ptr = &schedule_ptr->phase_array[schedule_ptr->phase_idx];
  Ramp_down_schedule_phase *ramp_down_phase_ptr = &phase_ptr->phase_cmd.ramp_down_phase;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d, max_ramp_down_vuser_or_sess = %d, session_rate_per_step = %d, num_vusers_or_sess = %d, cur_vusers_or_sess = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, phase_ptr->phase_type, ramp_down_phase_ptr->max_ramp_down_vuser_or_sess, ramp_down_phase_ptr->session_rate_per_step, ramp_down_phase_ptr->num_vusers_or_sess, schedule_ptr->cur_vusers_or_sess);

  // Check if final session rate achieved.
  // If yes, stop step timer, send ramp up done msg and return
/*
  if(ramp_down_phase_ptr->max_ramp_down_vuser_or_sess == ramp_down_phase_ptr->num_vusers_or_sess)   
  {
    stop_ramp_down_msg_timer(schedule_ptr->phase_end_tmr);
    send_ramp_down_done_msg(schedule_ptr);
    send_phase_complete(schedule_ptr);   // --- Arun Nishad
  }
  else
*/
  {
    // Stop Ramp timer as we are going to increase session rate
    // Delete for this group only ---Arun Nishad
    stop_ramp_down_timer(schedule_ptr->phase_ramp_tmr);
    
    if((ramp_down_phase_ptr->max_ramp_down_vuser_or_sess + 
        ramp_down_phase_ptr->session_rate_per_step) >= 
        ramp_down_phase_ptr->num_vusers_or_sess
      )
    {
       schedule_ptr->cur_vusers_or_sess -= ramp_down_phase_ptr->num_vusers_or_sess - ramp_down_phase_ptr->max_ramp_down_vuser_or_sess; 
       ramp_down_phase_ptr->max_ramp_down_vuser_or_sess += ramp_down_phase_ptr->num_vusers_or_sess - ramp_down_phase_ptr->max_ramp_down_vuser_or_sess;
       stop_phase_end_timer(schedule_ptr);
       if(schedule_ptr->type == 0)
         send_ramp_down_done_msg(schedule_ptr);
       send_phase_complete(schedule_ptr);
    }
    else
    {
      ramp_down_phase_ptr->max_ramp_down_vuser_or_sess +=  ramp_down_phase_ptr->session_rate_per_step;
      schedule_ptr->cur_vusers_or_sess -=  ramp_down_phase_ptr->session_rate_per_step;
      if(schedule_ptr->type == 0)
        send_ramp_down_msg(cd, now);
    }
    // Bug 92186
/*    if (global_settings->user_rate_mode == SESSION_RATE_CONSTANT)
      sess_rpc = ramp_down_phase_ptr->session_rate_per_step/SESSION_RATE_MULTIPLE;
    else */
      sess_rpc = schedule_ptr->cur_vusers_or_sess/SESSION_RATE_MULTIPLE;

    search_and_remove_user_for_fsr(schedule_ptr, now);

    // If all session are ramped downed then we need not to generate users
    if(sess_rpc < 0.1) 
      return;

    // We need to reset var as whenever calc_iid_ms_from_rpm is called,
    // as get rpm users() is directly propotional to, number of times calc iid ms from rpm called
    init_rate_intervals(schedule_ptr);
    if(sess_rpc < 100.0)
      schedule_ptr->iid_mu = (int)((double)(60000)/sess_rpc); 
    else
      schedule_ptr->iid_mu = calc_iid_ms_from_rpm(schedule_ptr, (int)sess_rpc);

    NSDL2_SCHEDULE(NULL, NULL, "Group Index = %d, Phases Index = %d, Phases Type = %d, max_ramp_down_vuser_or_sess = %d, session_rate_per_step = %d, cur_vusers_or_sess = %d, sess_rpc = %f, iid_mu = %lu", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, ramp_down_phase_ptr->max_ramp_down_vuser_or_sess, ramp_down_phase_ptr->session_rate_per_step, schedule_ptr->cur_vusers_or_sess, sess_rpc, schedule_ptr->iid_mu);

    if(global_settings->user_rate_mode == SESSION_RATE_RANDOM)
      schedule_ptr->ns_iid_handle = ns_poi_init(schedule_ptr->iid_mu, my_port_index);

    // Generate users using new iid
    generate_users(cd, now);
  }
}   

static void start_session_ramp_down_step_timer(Schedule *schedule_ptr, u_ns_ts_t now)
{
  ClientData cd;
  Phases *phase_ptr = &schedule_ptr->phase_array[schedule_ptr->phase_idx];
  cd.p = schedule_ptr;
  Ramp_down_schedule_phase *ramp_down_phase_ptr = &phase_ptr->phase_cmd.ramp_down_phase;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, phase_ptr->phase_type);

  if(!my_port_index)
    session_rate_ramp_down_step_stagger_timer_callback(cd, now);

  //ab_timers[AB_TIMEOUT_END].timeout_val = ramp_down_phase_ptr->ramp_down_step_time*1000;  //setting timer value for each step to run
  schedule_ptr->phase_end_tmr->actual_timeout = ramp_down_phase_ptr->ramp_down_step_time*1000;
  //dis_timer_add( AB_TIMEOUT_END, schedule_ptr->phase_end_tmr, now, session_rate_ramp_down_step_stagger_timer_callback, cd, 1);
  dis_timer_think_add( AB_TIMEOUT_END, schedule_ptr->phase_end_tmr, now, session_rate_ramp_down_step_stagger_timer_callback, cd, 1);
}

static void session_rate_ramp_down_child_stagger_timer_callback(ClientData cd, u_ns_ts_t now)
{
  Schedule *schedule_ptr = (Schedule *)cd.p;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type);

  start_session_ramp_down_step_timer(schedule_ptr, now);
}

static void session_rate_ramp_down_child_stagger_timer_start(Schedule *schedule_ptr, u_ns_ts_t now)
{
  double stagger_time;
  ClientData cd;
  cd.p = schedule_ptr;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type);

  stagger_time = (schedule_ptr->iid_mu*my_port_index)/global_settings->num_process;

  //ab_timers[AB_TIMEOUT_END].timeout_val = stagger_time;
  schedule_ptr->phase_end_tmr->actual_timeout = stagger_time;
  
  //dis_timer_add( AB_TIMEOUT_END, schedule_ptr->phase_end_tmr, now, session_rate_ramp_down_child_stagger_timer_callback, cd, 0);
  dis_timer_think_add( AB_TIMEOUT_END, schedule_ptr->phase_end_tmr, now, session_rate_ramp_down_child_stagger_timer_callback, cd, 0);
}

static void calc_ramp_down_session_rate_per_step(Schedule *schedule_ptr)
{
  double temp, sess_rpc;  //temporary variable used to calculate rounded value
  Phases *phase_ptr = &schedule_ptr->phase_array[schedule_ptr->phase_idx];

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type);

  temp = (double)(phase_ptr->phase_cmd.ramp_down_phase.num_vusers_or_sess)/(double)phase_ptr->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions;

  phase_ptr->phase_cmd.ramp_down_phase.session_rate_per_step = round(temp);
  sess_rpc = phase_ptr->phase_cmd.ramp_down_phase.session_rate_per_step/SESSION_RATE_MULTIPLE;
  
  if(global_settings->user_rate_mode == SESSION_RATE_CONSTANT) //Constt
  {
    if(sess_rpc < 100.0)
      schedule_ptr->iid_mu = (int)((double)(60000)/sess_rpc); 
    else
      schedule_ptr->iid_mu =(double)calc_iid_ms_from_rpm(schedule_ptr, (int)sess_rpc);
  }
}

void const_ramp_down_session_rate_with_steps(Schedule *schedule_ptr, u_ns_ts_t now)
{
  double sess_rpc;
  ClientData cd;
  cd.p = schedule_ptr;
  Ramp_down_schedule_phase *ramp_down_phase_ptr = &schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_cmd.ramp_down_phase;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx]);

  if(ramp_down_phase_ptr->num_vusers_or_sess == 0)
  {
    NSDL2_SCHEDULE(NULL, NULL, "Session became 0, Group Index = %d, Phases Index = %d, Phases Type = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type);
    if(schedule_ptr->type == 0)
      send_ramp_down_done_msg(schedule_ptr);
    send_phase_complete(schedule_ptr); 
    return;
  }

  calc_ramp_down_session_rate_per_step(schedule_ptr);

  sess_rpc = ramp_down_phase_ptr->session_rate_per_step/SESSION_RATE_MULTIPLE;

  if(!my_port_index)
    start_session_ramp_down_step_timer(schedule_ptr, now);
  else if(sess_rpc < 100.0)  // Do not stagger if session rate is too low
    session_rate_ramp_down_child_stagger_timer_callback(cd, now);
  else
    session_rate_ramp_down_child_stagger_timer_start(schedule_ptr, now);
}

void random_ramp_down_session_rate_with_steps(Schedule *schedule_ptr, u_ns_ts_t now)
{
  int i = 0;
  double sess_rpc;
  ClientData cd;
  cd.p = schedule_ptr;
  Ramp_down_schedule_phase *ramp_down_phase_ptr = &schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_cmd.ramp_down_phase;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type);

  if(ramp_down_phase_ptr->num_vusers_or_sess == 0)
  {
    NSDL2_SCHEDULE(NULL, NULL, "Session became 0, Group Index = %d, Phases Index = %d, Phases Type = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type);
    if(schedule_ptr->type == 0)
      send_ramp_down_done_msg(schedule_ptr);
    send_phase_complete(schedule_ptr); 
    return;
  }

  calc_ramp_down_session_rate_per_step(schedule_ptr);
  sess_rpc = ramp_down_phase_ptr->session_rate_per_step/SESSION_RATE_MULTIPLE;
  // Calculate iid using per step session rate
  do
  {
    i++;
    schedule_ptr->iid_mu =(double)((double)(60000.0*i)/sess_rpc);
  } while (schedule_ptr->iid_mu < 10.0);
  schedule_ptr->ramping_delta = i;

  // If child is not 0, then start stagger timer  and return
  if(!my_port_index)
    start_session_ramp_down_step_timer(schedule_ptr, now);
  else if(sess_rpc < 100.0)  // Do not stagger if session rate is too low
    session_rate_ramp_down_child_stagger_timer_callback(cd, now);
  else
    session_rate_ramp_down_child_stagger_timer_start(schedule_ptr, now);
}

void start_ramp_down_phase_fsr(Schedule *schedule_ptr, u_ns_ts_t now)
{
  Ramp_down_schedule_phase *ramp_down_phase_ptr = &schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_cmd.ramp_down_phase;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phase Index = %d, Phase Type = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx]);

  switch(ramp_down_phase_ptr->ramp_down_mode)
  {
    case RAMP_DOWN_MODE_IMMEDIATE:
      ramp_down_session_rate_immediate(schedule_ptr, now);
      break;
    case RAMP_DOWN_MODE_TIME_SESSIONS:
      if(global_settings->user_rate_mode == SESSION_RATE_CONSTANT)   // constt
        const_ramp_down_session_rate_with_steps(schedule_ptr, now);
      else  // Random 
        random_ramp_down_session_rate_with_steps(schedule_ptr, now);
      break;
    default:
      fprintf(stderr, "Error: Invalid ramp down mode for Group: %d, Phase: %d\n", schedule_ptr->group_idx, schedule_ptr->phase_idx);
      break;
  }
}
