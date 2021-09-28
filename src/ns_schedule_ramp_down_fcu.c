/************************************************************************************************************
 *  Name            : ns_schedule_ramp_down_fcu.c 
 *  Purpose         : To control Netstorm Ramp Down Phases
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
#include "timing.h"
#include "tmr.h"
#include "util.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "ns_event_log.h"
#include "netstorm.h"
#include "ns_schedule_ramp_down_fcu.h"
#include "ns_log.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "ns_schedule_ramp_down_fcu.h"
#include "ns_schedule_ramp_down_fsr.h"
#include "ns_schedule_ramp_up_fcu.h"
#include "ns_schedule_phases_parse.h"
#include "timing.h"
#include "poi.h"
#include "divide_users.h"
#include "divide_values.h"
#include "child_init.h"
#include "ns_trans.h"
#include "ns_session.h"
#include "ns_alloc.h"
#include "ns_percentile.h"
#include "ns_sock_com.h"
#include "ns_vuser.h"
#include "ns_debug_trace.h"
#include "wait_forever.h"
#include "ns_sync_point.h"
#include "ns_url_hash.h"
#include "ns_event_id.h"
#include "ns_string.h"
#include "ns_trace_level.h"
#include "ns_group_data.h"
#include "ns_schedule_fcs.h"
#include "ns_vuser_runtime_control.h"
#include "ns_exit.h"
#include "ns_vuser_tasks.h"

static void ramp_down_users(ClientData client_data, u_ns_ts_t now);

static void del_vptr_timer(timer_type* ptr)
{
  NSDL2_SCHEDULE(NULL, NULL, "Method called. timer_type=%d", ptr->timer_type);
    
  if ( ptr->timer_type >= 0 )
   dis_timer_del(ptr);
}

void stop_ramp_down_timer(timer_type* ptr)
{   
  NSDL2_SCHEDULE(NULL, NULL, "Method called.");
    
  if (ptr->timer_type >= 0 )
  {
    NSDL2_SCHEDULE(NULL, NULL, "Stopping Ramp Down Timer");
    dis_timer_del(ptr);
  }
}

void stop_ramp_down_msg_timer(timer_type* ptr)
{
  NSDL2_SCHEDULE(NULL, NULL, "Method called.");
  
  if (ptr->timer_type >= 0 )
  {
   NSDL2_SCHEDULE(NULL, NULL, "Deleting Ramp Down Msg Timer");
   dis_timer_del(ptr);
  }
}


// 1 - All phase over
// 0-  Else
int is_all_phase_over()
{
  int i = 0;
  Schedule *schedule_ptr;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. gRunPhase = %d, schedule_by = %d, my_port_index = %d",
                              gRunPhase, global_settings->schedule_by, my_port_index);

  if(gRunPhase == NS_ALL_PHASE_OVER)
  {
    NSTL1_OUT(NULL, NULL, "Warning: is_all_phase_over() called with all phase already over\n");
    return(1);
  }

  if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO)
  {
    if(my_port_index == 255)
      schedule_ptr = scenario_schedule;
    else 
      schedule_ptr = v_port_entry->scenario_schedule;

    NSDL2_SCHEDULE(NULL, NULL, "schedule_ptr = %p, phase_idx = %d, num_phases = %d, phase_status = %d",
                               schedule_ptr, schedule_ptr->phase_idx, schedule_ptr->num_phases, 
                               schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_status);
    if((schedule_ptr->phase_idx + 1) != schedule_ptr->num_phases ||
        schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_status != PHASE_IS_COMPLETED)
        return 0;
  }
  else if(global_settings->schedule_by == SCHEDULE_BY_GROUP)
  {
    for(i = 0; i < total_runprof_entries; i++)
    {
      //NC: In case of generators, we need to ignore scenario groups with quantity 0
      if(my_port_index == 255)
      {
        if (!runprof_table_shr_mem[i].quantity)
          continue;
        schedule_ptr = &group_schedule[i];
      }
      else
      {
        if(!per_proc_runprof_table[(my_port_index * total_runprof_entries) + i])
          continue;
        schedule_ptr = &(v_port_entry->group_schedule[i]); 
      }

      NSDL2_SCHEDULE(NULL, NULL, "schedule_ptr = %p, phase_idx = %d, num_phases = %d, phase_status = %d",
                               schedule_ptr, schedule_ptr->phase_idx, schedule_ptr->num_phases, 
                               schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_status);

      if((schedule_ptr->phase_idx + 1) != schedule_ptr->num_phases || 
          schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_status != PHASE_IS_COMPLETED)
        return 0;
    }
  }
  return 1;
}

static void start_ramp_down_msg_timer(Schedule *schedule_ptr, u_ns_ts_t now, int time_out)
{ 
  ClientData cd;

  cd.p = schedule_ptr;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Starting RampUp Msg Timer. now = %u, time_out = %d", now, time_out);

  //ab_timers[AB_TIMEOUT_END].timeout_val = time_out;
  //dis_timer_add( AB_TIMEOUT_END, schedule_ptr->phase_end_tmr, now, send_ramp_down_msg, cd, 1 );
  schedule_ptr->phase_end_tmr->actual_timeout = time_out;
  dis_timer_think_add( AB_TIMEOUT_END, schedule_ptr->phase_end_tmr, now, send_ramp_down_msg, cd, 1 );
}  

//this method is a copy of close_connection defined in netstorm.c excludes some condition because not used in ramp down controlled phase
void ramp_down_close_connection (VUser *vptr, connection *cptr, u_ns_ts_t now)
{
  NSDL2_SCHEDULE(vptr, NULL, "Method called. now=%u, log_records = %d, run_mode = %d", now, log_records, run_mode);

  int url_ok = 0;
  int status;
  int url_id;
  int redirect_flag = 0;          /* Dummy */
  char taken_from_cache = 0;
  /* 3.9.0 Changes:
   * Before connection pool was implemented, we were using cptr index as con_num to report in drill down
   * Now we do not have cptr index as we are using pool. So we will use cptr->conn_fd
   * Need to preserve connection fd before calling close_fd()
   */
  const int con_num = cptr->conn_fd;
  ns_8B_t flow_path_instance = cptr->nd_fp_instance;

  NSDL2_CONN(vptr, NULL, "Method called.");

  // Cache is coming fresh without making a connection
  if(cptr->completion_code == NS_COMPLETION_FRESH_RESPONSE_FROM_CACHE)  {
     taken_from_cache = 1;
  }

  status = cptr->req_ok;
  url_ok = !status;

  if (cptr->url_num == NULL) return;

  close_fd (cptr, 1, now);

  handle_url_complete(cptr, cptr->url_num->request_type,
		      now, url_ok, redirect_flag, status, taken_from_cache);

  if (log_records && (run_mode == NORMAL_RUN))
  {
    if((global_settings->static_parm_url_as_dyn_url_mode == NS_STATIC_PARAM_URL_AS_DYNAMIC_URL_ENABLED) && (cptr->url_num->proto.http.url_index == -1))
      url_id = url_hash_get_url_idx_for_dynamic_urls((u_ns_char_t *)cptr->url,
                                                             cptr->url_len, vptr->cur_page->page_id, 0, 0, vptr->cur_page->page_name);
    else
      url_id = cptr->url_num->proto.http.url_index;

    // excluding the failed url statistics. If exclude_stopped_stats is on and page status is stopped, hence excluding the stopped stats from page dump, hits, drilldown database, response time & tracing 
    if(!NS_EXCLUDE_STOPPED_STATS_FROM_URL) {
      if (log_url_record(vptr, cptr, status, cptr->request_complete_time, 0, con_num, flow_path_instance, url_id) == -1)
        NSTL1_OUT(NULL, NULL, "Error in logging the url record\n");
    }
  }
  if (cptr->conn_state == CNST_FREE) {
    if (cptr->list_member & NS_ON_FREE_LIST) {
      NSTL1(vptr, cptr, "Connection slot is already in free connection list");
    } else {
      /* free_connection_slot remove connection from either or both reuse or inuse link list*/
      NSDL1_CONN(vptr, cptr, "Connection state is free, need to call free_connection_slot");
      free_connection_slot(cptr, now);
    }
  }

}

// This method is a copy of handle_page_complete() defined in ns_page.c excludes some condition because not used in ramp down controleed phase
static inline void ramp_down_handle_page_complete(VUser *vptr, u_ns_ts_t now)
{
  NSDL2_SCHEDULE(vptr, NULL, "Method called. now=%u", now);
  
  if(vptr->cur_page) {
    NS_DT2(vptr, NULL, DM_L1, MM_CONN, "Completed execution of page '%s'", vptr->cur_page->page_name);

    calc_pg_time(vptr, now); // TODO - should we include these in time calculation
    tx_logging_on_page_completion(vptr, now);
  }
}

int random_ramp_mode_calc_max_vuser_to_ramp_down(Schedule *schedule_ptr)
{
  int poi_sample = 0;
 
  Ramp_down_schedule_phase *ramp_down_phase_ptr = &schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_cmd.ramp_down_phase;
  ramp_down_phase_ptr->ramped_down_vusers = 1;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type);

  while (1)
  {
    poi_sample = (int)ns_poi_sample(schedule_ptr->ns_iid_handle);
    if (!poi_sample)
      ramp_down_phase_ptr->ramped_down_vusers += schedule_ptr->ramping_delta;
    else
      break;
  }

  return(poi_sample);
}

//calls ramp_down_users with a new random timeout_val
static void random_ramp_down_users(ClientData cd, u_ns_ts_t now)
{
  int poi_sample;
  int periodic = 0;
  Schedule *schedule_ptr = (Schedule *)cd.p;

  NSDL2_SCHEDULE(NULL, NULL, "Method Called.");
  
  poi_sample = random_ramp_mode_calc_max_vuser_to_ramp_down(schedule_ptr);
  
  NSDL2_SCHEDULE(NULL, NULL, "poi_sample = %d", poi_sample);
  //ab_timers[AB_TIMEOUT_RAMP].timeout_val = poi_sample;
  schedule_ptr->phase_ramp_tmr->actual_timeout = poi_sample;
  //dis_timer_add( AB_TIMEOUT_RAMP, schedule_ptr->phase_ramp_tmr, now, ramp_down_users, cd, periodic);
  dis_timer_think_add( AB_TIMEOUT_RAMP, schedule_ptr->phase_ramp_tmr, now, ramp_down_users, cd, periodic);
}

// Currently this fuction is used only inuse list
static inline void mark_idle_inuse_reuse_connections(connection* cptr, int time_to_reset, u_ns_ts_t now)
{
  NSDL2_SCHEDULE(NULL, cptr, "Method called cptr = %p", cptr);
  /* To fetch next connection in list we are using next_inuse, next_inuse and next_reuse
   * belongs to union, which assure that only one union member is used at a time
   * hence to initiate traversing of list we require head node
   */
  for (; cptr != NULL; cptr = (connection *)cptr->next_inuse)
  {
    NSDL2_SCHEDULE(NULL, cptr, "timer type = %s", get_timer_type_by_name(cptr->timer_ptr->timer_type));
    if(cptr->timer_ptr->timer_type == AB_TIMEOUT_IDLE)
    {
      NSDL2_SCHEDULE(NULL, cptr, "timeout = %u, time to reset = %d", cptr->timer_ptr->timeout, time_to_reset);
      //if ( cptr->timer_ptr->timeout > time_to_reset)   // Earliar  
      {
        dis_timer_del(cptr->timer_ptr);
        //ab_timers[AB_TIMEOUT_IDLE].timeout_val = time_to_reset;
        cptr->timer_ptr->actual_timeout = time_to_reset;
        //dis_timer_add(AB_TIMEOUT_IDLE, cptr->timer_ptr, now,
          //            cptr->timer_ptr->timer_proc, cptr->timer_ptr->client_data, cptr->timer_ptr->periodic);
        dis_timer_add(AB_TIMEOUT_IDLE, cptr->timer_ptr, now,
                      cptr->timer_ptr->timer_proc, cptr->timer_ptr->client_data, cptr->timer_ptr->periodic);
      }
    }
  }
}

// Reset all idle time to time given by ramp down method
static inline void mark_idle_ramp_down(VUser *vptr, u_ns_ts_t now)
{
  int time_to_reset = runprof_table_shr_mem[vptr->group_num].gset.rampdown_method.time * 1000;
  NSDL2_SCHEDULE(vptr, NULL, "Method Called, time to reset = %d, max_con_per_vuser = %d", time_to_reset, global_settings->max_con_per_vuser);

  //  No need to do for reuse list as idle timer will not be running on these cptr as these are not active

  // Reset idle timer for all in use connections
  if(vptr->head_cinuse)
  {
    mark_idle_inuse_reuse_connections(vptr->head_cinuse, time_to_reset, now);
  }
 
}

// Return 1 if user is removed from busy list (e.g cleaned up) else 0
int stop_user_and_allow_current_page_to_complete(VUser *vptr, u_ns_ts_t now)
{
  NSDL2_SCHEDULE(vptr, NULL, "Method Called. Vuser_state = %d", vptr->vuser_state);

  switch(vptr->vuser_state) 
  {
    case NS_VUSER_IDLE:
    {
      NSDL4_SCHEDULE(vptr, NULL, "Stopping user. User is in idle state");
      user_cleanup(vptr, now);
      return 1;
    }
    case NS_VUSER_ACTIVE: // User is fetching URL(s) of a page
    {
      // Do not reduce active users as it is reduce in on session completion()
      // In case of hasten option, we need to reset all idle time to new value
      if (runprof_table_shr_mem[vptr->group_num].gset.rampdown_method.option == RDM_OPTION_HASTEN_COMPLETION_USING_IDLE_TIME) 
      {
        NSDL4_SCHEDULE(vptr, NULL, "User is in active state. Resetting idle time to %d", runprof_table_shr_mem[vptr->group_num].gset.rampdown_method.time);
        mark_idle_ramp_down(vptr, now);
      }
      else
        NSDL4_SCHEDULE(vptr, NULL, "User is in active state. Not resetting idle time");
      return 0;
    }
    case NS_VUSER_THINKING: // Page is complete and user is in thinking
    {
      NSDL4_SCHEDULE(vptr, NULL, "Stopping user. User is in page think state");
      
      if (vptr->timer_ptr->timer_type == AB_TIMEOUT_THINK)
      {
        // Need to call its call back immediate as we have to allow this page to complete, reset it
        vptr->timer_ptr->actual_timeout = 0;
        dis_timer_reset_ex(now, vptr->timer_ptr, 0);  // Reset with 0 if it is THINK TIMER

        // do not update gNumVuserThinking & gNumVuserActive here as we are calling callback function 'page_think_timer'
        // In this method these 2 are updated, finally gNumVuserActive will be reduced inside on session completion()
      }
      return 0;
      //break;
    }
    case NS_VUSER_SESSION_THINK: // Session is complete and user is in session pacing
    { 
      NSDL4_SCHEDULE(vptr, NULL, "Stopping user. User is in session think state");
      if (vptr->timer_ptr->timer_type == AB_TIMEOUT_STHINK) // Check session pacing timer
        dis_timer_del(vptr->timer_ptr);  // Delete timer it it is for session pacing

      // Since user is gone, we need to decreament it users in waiting state
      VUSER_DEC_WAITING(vptr);
      user_cleanup(vptr, now);
      return 1;
      //break;
    }/*  Do Not Select user which are in clean up stat
    case NS_VUSER_CLEANUP:
    {
      NSDL4_SCHEDULE(vptr, NULL, "Stopping user. User is in cleanup state");
      user_cleanup(vptr, now);
      return 1;
      //break;
    }*/
    case NS_VUSER_SYNCPOINT_WAITING:
    {
      sync_point_remove_users_frm_linked_list(vptr, now);
      return 0;
    }
    case NS_VUSER_BLOCKED:  //Session ended and user still in blocked state
    {
      NSDL4_SCHEDULE(vptr, NULL, "Stopping user. User is in session blocked state");
      vptr->sess_status = NS_SESSION_STOPPED;
      VUSER_DEC_BLOCKED(vptr);  //Since user is gone, we need to decreament it
      user_cleanup(vptr, now);
      return 1;
    }
    case NS_VUSER_PAUSED:
    {  
      NSDL4_SCHEDULE(vptr, NULL, "Stopping user. User is in session paused state");
      vptr->sess_status = NS_SESSION_STOPPED;
      vptr->operation = VUT_NO_TASK;
      remove_from_pause_list(vptr); 
      VUSER_DEC_PAUSED(vptr);
      on_session_completion(now, vptr, NULL, 0);
      return 1;
    }
    default:
      NSTL1_OUT(NULL, NULL, "Error: Unknown stat(%d) of user.\n", vptr->vuser_state);
      return 0;
  }
}

//Return 1 if user is cleaned up else 0
int stop_user_and_allow_cur_sess_complete(VUser *vptr, u_ns_ts_t now)
{
  NSDL2_SCHEDULE(vptr, NULL, "Method Called, vuser_state = %d", vptr->vuser_state);

  
  switch(vptr->vuser_state) 
  {
    case NS_VUSER_IDLE:
    {
      // its a default stat of user 
      NSDL4_SCHEDULE(vptr, NULL, "Stopping user. User is in idle state");
      user_cleanup(vptr, now);
      return 1;
      break;
    }
    case NS_VUSER_ACTIVE:
    {
      // All things session & pages are controlled in sesson completion & page complete
      NSDL4_SCHEDULE(vptr, NULL, "Stopping user. User is in active state");
      if (runprof_table_shr_mem[vptr->group_num].gset.rampdown_method.option > RDM_OPTION_HASTEN_COMPLETION_DISREGARDING_THINK_TIMES) 
        mark_idle_ramp_down(vptr, now);
      return 0;
    }
    case NS_VUSER_THINKING:
    {
      NSDL4_SCHEDULE(vptr, NULL, "Stopping user. User is in page think state");
      if (runprof_table_shr_mem[vptr->group_num].gset.rampdown_method.option > RDM_OPTION_USE_NORMAL_THINK_TIME) 
      {
        if (vptr->timer_ptr->timer_type == AB_TIMEOUT_THINK)
        {
          //ab_timers[AB_TIMEOUT_THINK].timeout_val = now;
          vptr->timer_ptr->actual_timeout = 0;
          dis_timer_reset_ex(now, vptr->timer_ptr, 0);  // Reset with 0 if it is THINK TIMER
        }
      }
/**
      if (global_settings->rampdown_method.option > RDM_OPTION_HASTEN_COMPLETION_DISREGARDING_THINK_TIMES) 
        mark_idle_ramp_down(vptr, now);
**/
      return 0;
      //break;
    }
    case NS_VUSER_SESSION_THINK:
    { 
      NSDL4_SCHEDULE(vptr, NULL, "Stopping user. User is in session think state");
      if (vptr->timer_ptr->timer_type == AB_TIMEOUT_STHINK) // Check session pacing timer
      {
        // 111105: To maintain the count of login and logout, if a user in session pacing we have to delete the timer
        // and run atleast one session so that logout count of a user can match to its login.
        // user_cleanup would be done in nsi_end_session when the new session gets completed. 
        //dis_timer_del(vptr->timer_ptr);  // Delete timer it it is for session pacing
        vptr->timer_ptr->actual_timeout = 0;
        dis_timer_reset_ex(now, vptr->timer_ptr, 0);  // Reset with 0 if it is THINK TIMER
      }
      // User is in think state so we need to reduce waiting users
      //VUSER_DEC_WAITING(vptr);  //Since user is gone, we need to decreament it
      //user_cleanup(vptr, now);
      return 1;
      //break;
    }/*  Do Not Select user which are in clean up stat
    case NS_VUSER_CLEANUP:
    {
      // User is in clean up stat remove this users
      NSDL4_SCHEDULE(vptr, NULL, "Stopping user. User is in cleanup state");
      user_cleanup(vptr, now);
      return 1;
      //break;
    }*/
    case NS_VUSER_SYNCPOINT_WAITING:
    {
      sync_point_remove_users_frm_linked_list(vptr, now);
      return 0;
    }
    case NS_VUSER_BLOCKED:  //Session ended and user still in blocked state
    {
      NSDL4_SCHEDULE(vptr, NULL, "Stopping user. User is in session blocked state");
      vptr->sess_status = NS_SESSION_STOPPED;
      VUSER_DEC_BLOCKED(vptr);  //Since user is gone, we need to decreament it
      user_cleanup(vptr, now);
      return 1;
    }
    case NS_VUSER_PAUSED:
    {  
      NSDL4_SCHEDULE(vptr, NULL, "Stopping user. User is in session paused state");
      vptr->sess_status = NS_SESSION_STOPPED;
      vptr->operation = VUT_NO_TASK;
      remove_from_pause_list(vptr); 
      VUSER_DEC_PAUSED(vptr);
      on_session_completion(now, vptr, NULL, 0);
      return 1;
    }
    default:
      NSTL1_OUT(NULL, NULL, "Error: Unknown stat(%d) of user.\n", vptr->vuser_state);
      return 0;
  }
}

// Stop user immdetialy. After this call, user will be removed
// So it always returns 1
int stop_user_immediately(VUser *vptr, u_ns_ts_t now)
{
  NSDL2_SCHEDULE(vptr, NULL, "Method Called.");

  // Do it here as it is not done is user cleanup method
  // It will delete timer which can be page think or sesion pacing or cleanup
  del_vptr_timer(vptr->timer_ptr); 

   // Stop all running transactions if any with status Stopped
   // This is called here as it is not done in on session completion()
   // TODO: We should call this based on the user state

  // Moved to nsi_end_session. Tx will be ended based on the last page status
  // tx_logging_on_session_completion (vptr, now, NS_REQUEST_STOPPED);

  switch(vptr->vuser_state) 
  {
    case NS_VUSER_IDLE:
    {
      NSDL4_SCHEDULE(vptr, NULL, "Stopping user. User is in idle state");
      user_cleanup(vptr, now);
      break;
    }
    case NS_VUSER_ACTIVE:
    {
      NSDL4_SCHEDULE(vptr, NULL, "Stopping user. User is in active state");

      // URL status is set to Stopped in user_cleanup() 
      // as it is to be set to each connection which was active
      NSDL3_SCHEDULE(vptr, NULL, "vptr->page_status = %d", vptr->page_status);
      vptr->page_status = (vptr->page_status != NS_USEONCE_ABORT) ? NS_REQUEST_STOPPED : NS_USEONCE_ABORT;
      ramp_down_handle_page_complete (vptr, now);
      vptr->sess_status = NS_SESSION_STOPPED;
      // Since user is gone, we need to decreament it gNumVuserActive which is done
      // on session completion()
      on_session_completion(now, vptr, NULL, 0);
      break;
    }
    case NS_VUSER_THINKING:
    {
      NSDL4_SCHEDULE(vptr, NULL, "Stopping user. User is in page think state");
      vptr->sess_status = NS_SESSION_STOPPED;
      // Since user is gone, we need to decreament thinking user count,Since sesson comp decrement, we need to increment here active user count
      VUSER_THINKING_TO_ACTIVE_WITHOUT_STATE_CHANGE(vptr);
      on_session_completion(now, vptr, NULL, 0);
      break;
    }
    case NS_VUSER_SESSION_THINK:
    { 
      NSDL4_SCHEDULE(vptr, NULL, "Stopping user. User is in session think state");
      VUSER_DEC_WAITING(vptr);   // Since user is gone, we need to decreament it
      // Do not increament active as user is gone
      user_cleanup(vptr, now);
      break;
    }/*  Do Not Select user which are in clean up stat --- Should we do in case of immediate also*/
    /* In case of stop user immediately if user is in cleanup state then call user_cleanup*/
    case NS_VUSER_CLEANUP:
    {
      NSDL4_SCHEDULE(vptr, NULL, "Stopping user. User is in cleanup state");
      // Counter will be decremented in user cleanup()
      user_cleanup(vptr, now);
      break;
    }
    case NS_VUSER_SYNCPOINT_WAITING:
    {
      sync_point_remove_users_frm_linked_list(vptr, now);
      //VUSER_SYNCPOINT_DEC_WAITING(vptr);
      VUSER_SYNCPOINT_WAITING_TO_ACTIVE(vptr);
      on_session_completion(now, vptr, NULL, 0);
      //user_cleanup(vptr, now);
      //CHK_AND_CLOSE_ACCOUTING(vptr);
      break;
    }
    case NS_VUSER_BLOCKED:  //Session ended and user still in blocked state
    {
      NSDL4_SCHEDULE(vptr, NULL, "Stopping user. User is in session blocked state");
      vptr->sess_status = NS_SESSION_STOPPED;
      VUSER_DEC_BLOCKED(vptr);  //Since user is gone, we need to decreament it
      user_cleanup(vptr, now);
      break;
    }
    case NS_VUSER_PAUSED:
    {  
      NSDL4_SCHEDULE(vptr, NULL, "Stopping user. User is in paused state");
      vptr->sess_status = NS_SESSION_STOPPED;
      vptr->operation = VUT_NO_TASK;
      remove_from_pause_list(vptr);
      VUSER_DEC_PAUSED(vptr);
      on_session_completion(now, vptr, NULL, 0);
      break;
    }
    default:
      NSTL1_OUT(NULL, NULL, "Error: Unknown stat(%d) of user.\n", vptr->vuser_state);
      break;
  }
  return 1;
}

/* Function used to mark users for ramp down
 * call stop_user_immediately() as we set RAMP_DOWN_METHOD 2 
 * once we receive signal to stop test immediately
 * */
void remove_all_user_stop_immediately(int do_all, u_ns_ts_t now)
{
  VUser *cur_vptr, *next_vptr;

  NSDL2_SCHEDULE(NULL, NULL, "Method called, do_all = %d", do_all);

  //Set current vptr
  cur_vptr = gBusyVuserHead;

  //Loop till current vptr becomes NULL
  for(;;)
  {
    NSDL2_SCHEDULE(NULL, NULL, "cur_vptr=%p", cur_vptr);
    /*Fix for segment fault occured while stopping test immediately:
      Test Case: In a runnning test while ramping down users the test was stopped 
                 immmediately using nsu_stop_test -s. While ramp down users/session 
                 flow calls function remove_users(), but on receiving signal to stop test 
                 end_test_run_mode flag was set which called remove_all_user_stop_immediately() 
                 with do_all = 0.
                 In this function for loop breaks once current vptr becomes NULL and do_all is equal to 1.
                 This was a race condidtion, 
                 hence loop continued with vptr having NULL value which result into segentation fault.  
     Solution:   Removing do_all from if condition bec this loop should terminate if last vptr is NULL.*/
    if(cur_vptr ==  NULL)
        break;

    // Save here as cur_vptr may get cleaned up
    next_vptr = cur_vptr->busy_next;

    //Set vptr flag to RAMP_DOWN
    if(!(cur_vptr->flags & NS_VUSER_RAMPING_DOWN) && (cur_vptr->sess_status != NS_SESSION_ABORT))
    {
      cur_vptr->flags |= NS_VUSER_RAMPING_DOWN;
      VUSER_INC_EXIT(cur_vptr); 
    }
    NSDL3_SCHEDULE(cur_vptr, NULL, "Stopping user. vuser_state = %d, max_con_per_vuser = %d, gNumVuserActive = %d, gNumVuserThinking = %d, gNumVuserWaiting = %d, gNumVuserCleanup = %d, gNumVuserSPWaiting = %d, gNumVuserBlocked = %d", cur_vptr->vuser_state, global_settings->max_con_per_vuser, gNumVuserActive, gNumVuserThinking, gNumVuserWaiting, gNumVuserCleanup, gNumVuserSPWaiting, gNumVuserBlocked);

    // mark this user to be ramped down
    stop_user_immediately(cur_vptr, now);
    //Update current vptr
    cur_vptr = next_vptr;
  }
}

// do_all == 1 delete all users (Be carefull: Schedule will be NULL )
// do_all == 0 delete user specified by ramped_down_vusers
static void search_and_remove_user(Schedule *schedule_ptr, int do_all, u_ns_ts_t now, int runtime_flag, int num_users, const int runtime_group_no, int rampdown_method)
{
  int i = 0, group_number = -1;
  VUser *cur_vptr, *next_vptr;

  NSDL2_SCHEDULE(NULL, NULL, "Method called, do_all = %d, runtime_flag = %d, num_users = %d, runtime_group_no = %d",
                                   do_all, runtime_flag, num_users, runtime_group_no);

  if (do_all == 0 && num_users > 0)
  {
    num_users = ns_rampdown_vuser_quantity((global_settings->schedule_by == SCHEDULE_BY_GROUP)?schedule_ptr->group_idx:runtime_group_no, num_users, now, runtime_flag);
  } 
 
  NSDL2_SCHEDULE(NULL, NULL, "num_users=%d", num_users);
  cur_vptr = gBusyVuserHead;
  for(;;)
  {
    if(do_all == 0 && i >= num_users)
      break;

    NSDL2_SCHEDULE(NULL, NULL, "cur_vptr=%p", cur_vptr);
    if(cur_vptr ==  NULL && do_all == 1)
        break;
    if(cur_vptr ==  NULL)
    {
      if(gBusyVuserHead == NULL)
      {
        if(do_all == 0)
            fprintf (stderr, "Warning: Child '%d' Ramp down - No more users are in the system.\n", my_child_index);
        break;
      }
      else
       cur_vptr = gBusyVuserHead;
    }
  
    next_vptr = cur_vptr->busy_next;  // Save here as cur_vptr may get cleaned up
   
    if(do_all == 0)
    {
      if( global_settings->schedule_by == SCHEDULE_BY_SCENARIO)
      {
        NSDL2_SCHEDULE(NULL, NULL, "group_number = %d, runtime_group_no=%d, runtime_flag=%d", group_number, runtime_group_no, runtime_flag);
        if(runtime_flag)
        {
          group_number = runtime_group_no;
          NSDL2_SCHEDULE(NULL, NULL, "group_number = %d, runtime_group_no=%d", group_number, runtime_group_no);
        }
        if( group_number == -1)
        {
          group_number = generate_scen_group_num();
          NSDL2_SCHEDULE(NULL, NULL, "group_number = %d, runtime_group_no=%d", group_number, runtime_group_no);
        }
      }
      else if(global_settings->schedule_by == SCHEDULE_BY_GROUP)
        group_number = schedule_ptr->group_idx; 
      else
      {
        NSTL1(NULL, NULL, "Scenario is neither schedule by scenario nor schedule by group.\n");
        NS_EXIT(-1, "Scenario is neither schedule by scenario nor schedule by group.");
      }

      if (cur_vptr->group_num != group_number) 
      {
        NSDL2_SCHEDULE(cur_vptr, NULL, "Continueing as group num did not match, cur_vptr->group_num = %d, group_number = %d", cur_vptr->group_num, group_number);
        cur_vptr = next_vptr;
        continue;
      }
      else
        NSDL2_SCHEDULE(cur_vptr, NULL, "Group is matched,  cur_vptr->group_num = %d, group_number = %d", cur_vptr->group_num, group_number);
    }
    //User Cleanup is handle in stop_test_immediately, therefore if ramp down method mode is other than stop immediately we need to ignore such user 
    if((cur_vptr->vuser_state == NS_VUSER_CLEANUP) && (rampdown_method != RDM_MODE_STOP_IMMEDIATELY))  // Ramp Down only Users for group_number or Not in clean up stat
    {
      NSDL2_SCHEDULE(cur_vptr, NULL, "Continueing as User(%p) is in clean up stat.", cur_vptr);
      cur_vptr = next_vptr;
      continue;
    }

    if(!(cur_vptr->flags & NS_VUSER_RAMPING_DOWN) && (cur_vptr->sess_status != NS_SESSION_ABORT)) 
    {
      cur_vptr->flags |= NS_VUSER_RAMPING_DOWN;
      if(rampdown_method != RDM_MODE_STOP_IMMEDIATELY)
        cur_vptr->flags |= NS_VUSER_GRADUAL_EXITING; 
      #ifndef CAV_MAIN
      VUSER_INC_EXIT(cur_vptr); 
      #endif
    }
    else
    {
      NSDL3_SCHEDULE(cur_vptr, NULL, "User is already marked as ramped down, i = %d, vuser_state = %d, max_con_per_vuser = %d, gNumVuserActive = %d, gNumVuserThinking = %d, gNumVuserWaiting = %d, gNumVuserCleanup = %d, gNumVuserBlocked = %d", i, cur_vptr->vuser_state, global_settings->max_con_per_vuser, gNumVuserActive, gNumVuserThinking, gNumVuserWaiting, gNumVuserCleanup, gNumVuserBlocked);
      cur_vptr = next_vptr;
      continue;
    }

    if(!do_all)
    {
      --schedule_ptr->cur_vusers_or_sess;
      NSDL4_SCHEDULE(cur_vptr, NULL, "After removing user cur_vusers_or_sess = %d", schedule_ptr->cur_vusers_or_sess);
    }
    
    // cur_vusers_or_sess should be decrease here

    i++;  // Number of users has to ramped down

    NSDL3_SCHEDULE(cur_vptr, NULL, "Stopping user, i = %d, vuser_state = %d, max_con_per_vuser = %d, "
                                   "gNumVuserActive = %d, gNumVuserThinking = %d, gNumVuserWaiting = %d, gNumVuserCleanup = %d, "
                                   "gNumVuserSPWaiting = %d, gNumVuserBlocked = %d", 
                                    i, cur_vptr->vuser_state, global_settings->max_con_per_vuser, 
                                    gNumVuserActive, gNumVuserThinking, gNumVuserWaiting, gNumVuserCleanup, 
                                    gNumVuserSPWaiting, gNumVuserBlocked);


    // mark this user to be ramped down

    if(cur_vptr->vuser_state != NS_VUSER_SYNCPOINT_WAITING) 
    {
      if(rampdown_method == RDM_MODE_ALLOW_CURRENT_SESSION_COMPLETE)  //Allow curent session to complete
        stop_user_and_allow_cur_sess_complete(cur_vptr, now);
      else if(rampdown_method == RDM_MODE_ALLOW_CURRENT_PAGE_COMPLETE)  //Allow curent page to complete 
        stop_user_and_allow_current_page_to_complete(cur_vptr, now);
      // Stop user immediate with status
      else if(rampdown_method == RDM_MODE_STOP_IMMEDIATELY){
        stop_user_immediately(cur_vptr,  now);
      }
    }
    cur_vptr = next_vptr;
    // If user is marked then we need to get group number again for nxt user by generate scen group num()
    group_number = -1;
  }
}

/*Parameters:
     do_all : 1 for removing all users, 0 otherwise. Passing 0 in case of deleting users during RTC
     runtime_flag: 1 in case of removing during RTC
     quantity: required in case runtime_flag is 1
     grp_idx: group_idx in case runtime_flag is 1, -1 otherwise

// do_all == 1 delete all users (Be carefull: Schedule will be NULL )
// do_all == 0 delete user specified by ramped_down_vusers
*/
void remove_users(Schedule *schedule_ptr, int do_all, u_ns_ts_t now, int runtime_flag, int num_users, int grp_idx)
{
  int rampdown_method;
  if (grp_idx != -1)
    rampdown_method = runprof_table_shr_mem[grp_idx].gset.rampdown_method.mode;
  else
    rampdown_method = group_default_settings->rampdown_method.mode;
 
  NSDL2_SCHEDULE(NULL, NULL, "Method called, do_all = %d, num_users = %d, runtime_flag=%d, grp_idx=%d, end_test_run_mode = %d",
                                         do_all, num_users, runtime_flag, grp_idx, end_test_run_mode);
  remove_users_ex(schedule_ptr, do_all, now, runtime_flag, num_users, grp_idx, rampdown_method);
}

void remove_users_ex(Schedule *schedule_ptr, int do_all, u_ns_ts_t now, int runtime_flag, int num_users, int grp_idx, int rampdown_method)
{
  NSDL2_SCHEDULE(NULL, NULL, "Method called, do_all = %d, num_users = %d, runtime_flag=%d, grp_idx=%d, end_test_run_mode = %d, rampdown_method = %d", 
                                         do_all, num_users, runtime_flag, grp_idx, end_test_run_mode, rampdown_method);
  if(runtime_flag)
  {
    NSDL2_SCHEDULE(NULL, NULL, "Removing num_users = %d for RTC", num_users);
    if(end_test_run_mode == 1) //For stopping test immediately
      remove_all_user_stop_immediately(do_all, now);
    else 
      search_and_remove_user(schedule_ptr, 0, now, runtime_flag, num_users, grp_idx, rampdown_method);
  }
  else  
  {
    if(do_all == 0)
      num_users = schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_cmd.ramp_down_phase.ramped_down_vusers;

    NSDL2_SCHEDULE(NULL, NULL, "Removing num_users = %d for non-RTC", num_users);
    if(end_test_run_mode == 1) //For stopping test immediately
      remove_all_user_stop_immediately(do_all, now);
    else
     { 
      search_and_remove_user(schedule_ptr, do_all, now, runtime_flag, num_users, grp_idx, rampdown_method);
     }
  }
  
  //remove_all_blocked_users();
  NSDL2_SCHEDULE(NULL, NULL, "Exiting Method.");

}


/*

ramp_down_users does the ramp down users in following way :

Tasks            I      A       T       S       C

exit_func        N      Y       Y       N       N
--------------------------------------------------
vptr_timer       Y      N       Y       Y       Y
--------------------------------------------------
cptr_timer       Y      Y       N       N       N
--------------------------------------------------
set status :U    N      Y       N       N       N
           :P    N      Y       N       N       N
           :T    N      Y       Y       N       N
           :S    N      Y       Y       Y       N
--------------------------------------------------
close conn       Y      Y       Y       Y       Y
--------------------------------------------------
close trans      Y      Y       Y       N       N

*/

static void ramp_down_users(ClientData client_data, u_ns_ts_t now)
{
  Schedule *schedule_ptr =  (Schedule *)client_data.p;
  Ramp_down_schedule_phase *ramp_down_phase_ptr = &schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_cmd.ramp_down_phase;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type);
  
  // This one we need to do every time.
  if(ramp_down_phase_ptr->ramp_down_mode == RAMP_DOWN_MODE_TIME && ramp_down_phase_ptr->ramp_down_pattern == RAMP_DOWN_PATTERN_LINEAR) 
    schedule_ptr->ramping_delta = ramp_down_phase_ptr->ramped_down_vusers = get_rpm_users(schedule_ptr, 0);

  NSDL2_SCHEDULE(NULL, NULL, "Group Index = %d, Phases Index = %d, Phases Type = %d, ramped_down_vusers = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, ramp_down_phase_ptr->ramped_down_vusers);

  if ((ramp_down_phase_ptr->max_ramp_down_vuser_or_sess + ramp_down_phase_ptr->ramped_down_vusers) >= ramp_down_phase_ptr->num_vusers_or_sess)
  {
    //here we r setting v_port_entry->ramping_delta as it has more value than the num user to be ramp down
    NSDL2_SCHEDULE(NULL, NULL, "Setting ramped_down_vusers, max_ramp_down_vuser_or_sess = %d, ramped_down_vusers = %d, ", ramp_down_phase_ptr->max_ramp_down_vuser_or_sess, ramp_down_phase_ptr->ramped_down_vusers);

    ramp_down_phase_ptr->ramped_down_vusers = ramp_down_phase_ptr->num_vusers_or_sess - ramp_down_phase_ptr->max_ramp_down_vuser_or_sess;
    ramp_down_phase_ptr->max_ramp_down_vuser_or_sess = ramp_down_phase_ptr->num_vusers_or_sess;
  }
  else
    ramp_down_phase_ptr->max_ramp_down_vuser_or_sess += ramp_down_phase_ptr->ramped_down_vusers;
   

  NSDL2_SCHEDULE(NULL, NULL, "max_ramp_down_vuser_or_sess = %d, ramped_down_vusers = %d", ramp_down_phase_ptr->max_ramp_down_vuser_or_sess, ramp_down_phase_ptr->ramped_down_vusers);
  if(schedule_ptr->type == 0)
    remove_users(schedule_ptr, 0, now, 0, 0, -1);
  else
    remove_users(schedule_ptr, 0, now, 1, ramp_down_phase_ptr->ramped_down_vusers, schedule_ptr->group_idx);

  if(ramp_down_phase_ptr->max_ramp_down_vuser_or_sess == ramp_down_phase_ptr->num_vusers_or_sess)
  { 
    NSDL2_SCHEDULE(NULL, NULL, "All users of this phase ramped down for this child. Stopping msg timer, cycle timer.");
    //stoping msg timeer if exist 
    if (schedule_ptr->type == 0)
      stop_ramp_down_msg_timer(schedule_ptr->phase_end_tmr);
    //stoping cycle timer for this child if exists
    stop_ramp_down_timer(schedule_ptr->phase_ramp_tmr);
    if (schedule_ptr->type == 0)
      send_ramp_down_done_msg(schedule_ptr);
    send_phase_complete(schedule_ptr);   // TODO: Phase will be complete when all users are gone from the system
  }
  // For random, we need to calculate users and timer again
  else if (ramp_down_phase_ptr->ramp_down_mode == RAMP_DOWN_MODE_TIME && ramp_down_phase_ptr->ramp_down_pattern == RAMP_DOWN_PATTERN_RANDOM)
  {
    random_ramp_down_users(client_data, now);
  }

  // TODO: We tried this so that all the expired timer can called but did'nt work 
  //dis_timer_run(now);
}

void start_ramp_down_timer(u_ns_ts_t now, Schedule *schedule_ptr, int periodic)
{
  ClientData cd;

  cd.p = schedule_ptr;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d, iid_mu = %u", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, schedule_ptr->iid_mu);

  ramp_down_users(cd, now);
  if(schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_status == PHASE_IS_COMPLETED)  // it phase done do not add timer 
    return;

  //ab_timers[AB_TIMEOUT_RAMP].timeout_val = schedule_ptr->iid_mu;
  schedule_ptr->phase_ramp_tmr->actual_timeout = schedule_ptr->iid_mu;
  //dis_timer_add( AB_TIMEOUT_RAMP, schedule_ptr->phase_ramp_tmr, now, ramp_down_users, cd, periodic);
  dis_timer_think_add( AB_TIMEOUT_RAMP, schedule_ptr->phase_ramp_tmr, now, ramp_down_users, cd, periodic);
}

static void step_ramp_down_child_stagger_timer_callback(ClientData cd, u_ns_ts_t now)
{
  int periodic = 1;
  Schedule *schedule_ptr = (Schedule *)cd.p;
  Ramp_down_schedule_phase *ramp_down_phase_ptr = &schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_cmd.ramp_down_phase;

  NSDL2_SCHEDULE(NULL, NULL, "Method Called");
  /*NetOmni:Calculate step time for generators(1-n)*/
  int total_gen = 0;
  if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO)
    total_gen = global_settings->num_generators;
  else
    total_gen = runprof_table_shr_mem[schedule_ptr->group_idx].num_generator_per_grp;
  
  if (total_gen)
  {
    /*step-time for nvm0 of Generator0 = step_time * total generator * number of nvms */
    schedule_ptr->iid_mu = ramp_down_phase_ptr->ramp_down_step_time * total_gen * global_settings->num_process;
  }
  else 
  {
    schedule_ptr->iid_mu = ramp_down_phase_ptr->ramp_down_step_time * global_settings->num_process;
  }
  start_ramp_down_timer(now, schedule_ptr, periodic);
}

void step_ramp_down_child_stagger_timer_start(u_ns_ts_t now, Schedule *schedule_ptr)
{
  ClientData cd;
  cd.p = schedule_ptr;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type);

  //ab_timers[AB_TIMEOUT_RAMP].timeout_val = schedule_ptr->iid_mu;  // Stagger timer value
  schedule_ptr->phase_ramp_tmr->actual_timeout = schedule_ptr->iid_mu;
  //dis_timer_add( AB_TIMEOUT_RAMP, schedule_ptr->phase_ramp_tmr, now, step_ramp_down_child_stagger_timer_callback, cd, 0); // ---Chk Timer  Arun Nishad
  dis_timer_think_add( AB_TIMEOUT_RAMP, schedule_ptr->phase_ramp_tmr, now, step_ramp_down_child_stagger_timer_callback, cd, 0); // ---Chk Timer  Arun Nishad
}

//STEP ramp down mode function
//function sets the values for staggring childs
static void step_ramp_down_users(u_ns_ts_t now, Schedule *schedule_ptr)
{
  Ramp_down_schedule_phase *ramp_down_phase_ptr = &schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_cmd.ramp_down_phase;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type);

  if(ramp_down_phase_ptr->num_vusers_or_sess == 0)
  {
    NSDL2_SCHEDULE(NULL, NULL, "No user to ramp down. Group Index = %d, Phases Index = %d, Phases Type = %d, num_vusers_or_sess = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, ramp_down_phase_ptr->num_vusers_or_sess);
    if (schedule_ptr->type != 1)
      send_ramp_down_done_msg(schedule_ptr);
    send_phase_complete(schedule_ptr);
    return;
  }

  if (schedule_ptr->type != 1)
    start_ramp_down_msg_timer(schedule_ptr, now, 2000);
  schedule_ptr->ramping_delta = ramp_down_phase_ptr->ramped_down_vusers = ramp_down_phase_ptr->ramp_down_step_users;
  /* Netomni: In step mode we need to stagger step among generators and 
   * between NVMs within generators*/
  int total_gen = 0;
  if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO)
    total_gen = global_settings->num_generators;
  else
    total_gen = runprof_table_shr_mem[schedule_ptr->group_idx].num_generator_per_grp;

  //for child 0 we will set cycle time to ramp_down_step_time*num childs
  if ((!my_port_index) && (!g_generator_idx))
  {
    // Validation for ramp_down_step_time is already done in ns_schedule_phases_parse.c (Parent)
    if (total_gen) 
    { /*step-time for nvm0 of Generator0 = step_time * total generator * number of nvms */
      schedule_ptr->iid_mu = ramp_down_phase_ptr->ramp_down_step_time * total_gen * global_settings->num_process;
    }
    else 
    {
      schedule_ptr->iid_mu = ramp_down_phase_ptr->ramp_down_step_time*global_settings->num_process;
    }
    NSDL3_SCHEDULE(NULL, NULL, "Ramp Down Mode - STEP. GenId = %d, NVMId = %d, iid_mu = %llu, ramped_down_vusers = %d, total_generator = %d", g_generator_idx, my_child_index, schedule_ptr->iid_mu, ramp_down_phase_ptr->ramped_down_vusers, total_gen);
    start_ramp_down_timer(now, schedule_ptr, 1);
  }
  else
  {
    //this child has to run after ramp_down_step_time secs w.r.t previous child
    if (total_gen)
    { /*Stagger-time for nvm(1-n) of Generator(0-n) = (step_time * gen_id) + (step_time * total_gen) * nvm_id*/
      schedule_ptr->iid_mu = (ramp_down_phase_ptr->ramp_down_step_time * g_generator_idx) + ((ramp_down_phase_ptr->ramp_down_step_time * total_gen) * my_port_index);
    }
    else 
    { 
      schedule_ptr->iid_mu = ramp_down_phase_ptr->ramp_down_step_time*my_port_index;
    }
    NSDL3_SCHEDULE(NULL, NULL, "Ramp Down Mode - STEP - NVM Staggered. GenId = %d, NVMId = %d, iid_mu = %llu, ramped_down_vusers = %d, total_generator = %d", g_generator_idx, my_child_index, schedule_ptr->iid_mu, ramp_down_phase_ptr->ramped_down_vusers, total_gen);
    step_ramp_down_child_stagger_timer_start(now, schedule_ptr);
  }
}

static void time_ramp_down_users_linear_pattern_child_stagger_timer_callback(ClientData cd, u_ns_ts_t now)
{
  int periodic = 1;

  NSDL2_SCHEDULE(NULL, NULL, "Method called.");

  start_ramp_down_timer(now, (Schedule *)cd.p, periodic);
}

void time_ramp_down_users_linear_pattern_child_stagger_timer_start(u_ns_ts_t now, Schedule *schedule_ptr)
{
  ClientData cd;
  cd.p = schedule_ptr;
  Phases *ph = &schedule_ptr->phase_array[schedule_ptr->phase_idx];

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type);
  
  //ab_timers[AB_TIMEOUT_RAMP].timeout_val = (schedule_ptr->iid_mu*my_port_index)/global_settings->num_process;
  //schedule_ptr->phase_ramp_tmr->actual_timeout =  (schedule_ptr->iid_mu*my_port_index)/global_settings->num_process;
  schedule_ptr->phase_ramp_tmr->actual_timeout =  (schedule_ptr->iid_mu*ph->phase_cmd.ramp_down_phase.nvm_dist_index)/ph->phase_cmd.ramp_down_phase.nvm_dist_count;
  //dis_timer_add( AB_TIMEOUT_RAMP, schedule_ptr->phase_ramp_tmr, now, time_ramp_down_users_linear_pattern_child_stagger_timer_callback, cd, 0);
  dis_timer_think_add( AB_TIMEOUT_RAMP, schedule_ptr->phase_ramp_tmr, now, time_ramp_down_users_linear_pattern_child_stagger_timer_callback, cd, 0);
}

static void time_ramp_down_users_linear_pattern(u_ns_ts_t now, Schedule *schedule_ptr)
{
  double rpc; //rate per child
  Ramp_down_schedule_phase *ramp_down_phase_ptr = &schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_cmd.ramp_down_phase;
  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type);

  if(ramp_down_phase_ptr->num_vusers_or_sess == 0)
  {
    NSDL2_SCHEDULE(NULL, NULL, "No user to ramp down. Group Index = %d, Phases Index = %d, Phases Type = %d, num_vusers_or_sess = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, ramp_down_phase_ptr->num_vusers_or_sess);
    if(schedule_ptr->type == 0)
      send_ramp_down_done_msg(schedule_ptr);
    send_phase_complete(schedule_ptr);
    return;
  }
 
  if (schedule_ptr->type != 1)
   start_ramp_down_msg_timer(schedule_ptr, now, 2000);
  //rpc = (double)(ramp_down_phase_ptr->num_vusers_or_sess * 1000 * 60)/(double)ramp_down_phase_ptr->ramp_down_time;
  rpc = ramp_down_phase_ptr->rpc;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. total_users of this child = %d, rpc = %f", global_settings->num_connections, rpc);

  CALC_IID_MU(rpc, schedule_ptr);
  if(schedule_ptr->iid_mu == 0) END_TEST_RUN;
  // Validations will be done in ramp down users()
  schedule_ptr->ramping_delta = ramp_down_phase_ptr->ramped_down_vusers = get_rpm_users(schedule_ptr, 0);

  NSDL3_SCHEDULE(NULL, NULL, "Ramp Down Mode - Linear. Rate of this NVM = %f, schedule_ptr->iid_mu = %u, ramp_down_phase_ptr->ramped_down_vusers = %d, ramping_delta=%d", rpc, schedule_ptr->iid_mu, ramp_down_phase_ptr->ramped_down_vusers, schedule_ptr->ramping_delta);

  if(!ramp_down_phase_ptr->nvm_dist_index)    //for child 0
    start_ramp_down_timer(now, schedule_ptr, 1);
  else                  //for  other childs
    time_ramp_down_users_linear_pattern_child_stagger_timer_start(now, schedule_ptr);
}

void random_start_ramp_down_timer(u_ns_ts_t now, Schedule *schedule_ptr)
{
  ClientData cd;
  cd.p = schedule_ptr;

  NSDL2_SCHEDULE(NULL, NULL, "Method Called");

  ramp_down_users(cd, now);
}

void random_ramp_down_child_stagger_timer_callback(ClientData cd, u_ns_ts_t now)
{
  Schedule *schedule_ptr = (Schedule *)cd.p;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type);

  random_start_ramp_down_timer(now, schedule_ptr);
}


void random_ramp_down_child_stagger_timer_start(u_ns_ts_t now, Schedule *schedule_ptr)
{
  int poi_sample;
  ClientData cd;

  cd.p = schedule_ptr;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type);

  poi_sample = (int)ns_poi_sample(schedule_ptr->ns_iid_handle);
  poi_sample = (poi_sample * my_port_index)/global_settings->num_process;

  NSDL2_SCHEDULE(NULL, NULL, "poi_sample = %d", poi_sample);

  //ab_timers[AB_TIMEOUT_RAMP].timeout_val =  poi_sample;
  schedule_ptr->phase_ramp_tmr->actual_timeout = poi_sample;
  //dis_timer_add( AB_TIMEOUT_RAMP, schedule_ptr->phase_ramp_tmr, now, random_ramp_down_child_stagger_timer_callback, cd, 0);   // ---Arun Nishad
  dis_timer_think_add( AB_TIMEOUT_RAMP, schedule_ptr->phase_ramp_tmr, now, random_ramp_down_child_stagger_timer_callback, cd, 0);   // ---Arun Nishad
}

static void time_ramp_down_users_random_pattern(u_ns_ts_t now, Schedule *schedule_ptr)
{
  double rpc;
  Ramp_down_schedule_phase *ramp_down_phase_ptr = &schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_cmd.ramp_down_phase;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type);

  if(ramp_down_phase_ptr->num_vusers_or_sess == 0)
  {
    NSDL2_SCHEDULE(NULL, NULL, "No user to ramp up. Group Index = %d, Phases Index = %d, Phases Type = %d, num_vusers_or_sess = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, ramp_down_phase_ptr->num_vusers_or_sess);
    if(schedule_ptr->type == 0)
      send_ramp_down_done_msg(schedule_ptr);
    send_phase_complete(schedule_ptr);
    return;
  }
 
  if (schedule_ptr->type != 1)
    start_ramp_down_msg_timer(schedule_ptr, now, 2000);
  //rpc = (double)(ramp_down_phase_ptr->num_vusers_or_sess * 1000 * 60)/(double)ramp_down_phase_ptr->ramp_down_time;
  rpc = ramp_down_phase_ptr->rpc;

  CALC_IID_HANDLE(rpc, schedule_ptr);
  ramp_down_phase_ptr->ramped_down_vusers = schedule_ptr->ramping_delta;  
  NSDL3_SCHEDULE(NULL, NULL, "Ramp Down Mode - Random. Rate of this NVM = %f, schedule_ptr->iid_mu = %u, ramp_down_phase_ptr->ramped_down_vusers = %d", rpc, schedule_ptr->iid_mu, ramp_down_phase_ptr->ramped_down_vusers);

  if (!my_port_index)
    random_start_ramp_down_timer(now, schedule_ptr);
  else
    random_ramp_down_child_stagger_timer_start(now, schedule_ptr);
}

void delete_phase_end_timer(Schedule *schedule_ptr)
{
  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phase Index = %d, Phase Type = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type);

  if (schedule_ptr->phase_end_tmr->timer_type == AB_TIMEOUT_END )
  {
     NSDL2_SCHEDULE(NULL, NULL, "Deleting Run Time Timer.");
     dis_timer_del(schedule_ptr->phase_end_tmr);
  }
}


//set cur phase runnig to RAMP Down Phase
static void force_phase_end(Schedule *schedule_ptr, u_ns_ts_t now)
{
  NSDL2_SCHEDULE(NULL, NULL, "Method Called");

  if(schedule_ptr->phase_end_tmr->timer_type > 0)
    dis_timer_del(schedule_ptr->phase_end_tmr);

  if(schedule_ptr->phase_ramp_tmr->timer_type > 0)
    dis_timer_del(schedule_ptr->phase_ramp_tmr);

  //pct_run_phase_mode_chk_send_ready_msg(-1, 1);
}

//Bug#4067 - This is to remove staggering timer and periodic step timer and briniging the phase to end in case all users gets deleted
void check_and_finish_ramp_down_phase(Schedule *schedule_ptr, u_ns_ts_t now, Ramp_down_schedule_phase *ramp_down_phase_ptr)
{
  NSDL3_SCHEDULE(NULL, NULL, "Method Called. num_vusers_or_sess=%d", ramp_down_phase_ptr->num_vusers_or_sess);
  if(ramp_down_phase_ptr->num_vusers_or_sess == ramp_down_phase_ptr->max_ramp_down_vuser_or_sess)
  {
    NSDL2_SCHEDULE(NULL, NULL, "No user to ramp down. Group Index = %d, Phases Index = %d, Phases Type = %d, num_vusers_or_sess = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, ramp_down_phase_ptr->num_vusers_or_sess);
    force_phase_end(schedule_ptr, now);
//    send_ramp_down_done_msg(schedule_ptr);
//    send_phase_complete(schedule_ptr);
  }
}

/* This method resets the pct array for ramp down phase if control + C has been done in ramp up phase
 * because generate_scen_group_num uses per_grp_qty to return group number but during the Ramp Up if ctrl + C recieved, 
 * the we will have to ramp down only those users which has ramped up.
 *
 */
static void reset_pct_array(Schedule *schedule_ptr)
{
  int total_user_avail = 0;
  int i;
  Ramp_up_schedule_phase *ramp_up_phase_ptr = &schedule_ptr->phase_array[1].phase_cmd.ramp_up_phase;
  Ramp_down_schedule_phase *ramp_down_phase_ptr = &schedule_ptr->phase_array[schedule_ptr->num_phases - 1].phase_cmd.ramp_down_phase;
  int *per_grp_qty_ru = ramp_up_phase_ptr->per_grp_qty;
  int *per_grp_qty_rd = ramp_down_phase_ptr->per_grp_qty; 

  NSDL2_SCHEDULE(NULL, NULL, "Resetting num_vusers_or_sess (of ramp down) as next phase will be ramp down, Group Index = %d, Phases Index = %d, Phases Type = %d, ramped_up_vusers = %d, max_ramp_up_vuser_or_sess = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, ramp_up_phase_ptr->ramped_up_vusers, ramp_up_phase_ptr->max_ramp_up_vuser_or_sess);

  //Bug#4114 - This is happening as in ^C in simple scenario based schedule
  //earlier ramped_up users were taken to ramp-down...
  //however in RTC, when we have deleted some users, 
  //ramped_up_users is still the users which are ramped-up
  //e.g we ramped up 20 users and 10 we ramped-down, we r left with 10 users to remove in case of ^C

  //ramp_down_phase_ptr->num_vusers_or_sess = ramp_up_phase_ptr->ramped_up_vusers; 
  ramp_down_phase_ptr->num_vusers_or_sess = schedule_ptr->cur_vusers_or_sess;
  
  for(i = 0; i< total_runprof_entries; i++ )
  {
    NSDL2_SCHEDULE(NULL, NULL, "per_grp_qty_rd[%d] = %d, per_grp_qty_ru[%d] = %d", i, per_grp_qty_rd[i], i, per_grp_qty_ru[i]);
    per_grp_qty_rd[i] =  per_grp_qty_rd[i] - per_grp_qty_ru[i];
    NSDL2_SCHEDULE(NULL, NULL, "per_grp_qty_rd[%d] = %d", i, per_grp_qty_rd[i]);
    total_user_avail += per_grp_qty_rd[i]; 
  }
  NSDL2_RUNTIME(NULL, NULL, "ramp_down_phase_ptr=%p", ramp_down_phase_ptr);

  if(total_user_avail != ramp_down_phase_ptr->num_vusers_or_sess) 
  {
    NSDL2_SCHEDULE(NULL, NULL, "total_user_avail = %d, num_vusers_or_sess = %d", total_user_avail, ramp_down_phase_ptr->num_vusers_or_sess);
    NSTL1_OUT(NULL, NULL, "Warning: Mismatch is total users avilable in system.\n"); 
  }
}

// this function has to be called when sigterm recieved
static void mark_finish(u_ns_ts_t now)
{
  Schedule *schedule_ptr, *cur_runtime_schedule;
  void *schedule_mem_ptr;

  NSDL2_SCHEDULE(NULL, NULL, "Method called, now=%u, end_test_run_mode = %d, schedule_by = %d", 
                              now, end_test_run_mode, global_settings->schedule_by);

  int i, rtc_idx;

  if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO)
  {
    schedule_ptr = v_port_entry->scenario_schedule;
    //Find running runtime phase and mark them complete 
    schedule_mem_ptr = v_port_entry->runtime_schedule;
    for (rtc_idx = 0; rtc_idx < (global_settings->num_qty_rtc * total_runprof_entries); rtc_idx++)
    {
      cur_runtime_schedule = schedule_mem_ptr + (rtc_idx * find_runtime_qty_mem_size());
      NSDL4_RUNTIME(NULL, NULL, "cur_runtime_schedule->rtc_state=%d", cur_runtime_schedule->rtc_state);
      if (cur_runtime_schedule->rtc_state == RTC_RUNNING)
      { 
         stop_phase_ramp_timer(cur_runtime_schedule);
         stop_phase_end_timer(cur_runtime_schedule);
         send_phase_complete(cur_runtime_schedule); 
      }
    }
    if (global_settings->schedule_type == SCHEDULE_TYPE_SIMPLE && 
        schedule_ptr->phase_idx == (schedule_ptr->num_phases - 1) && 
        v_port_entry->num_fetches == 0 && (end_test_run_mode == 0)) { //In case of stopping test gracefully we ignore signal whereas for stop immediately we need to honor signal and terinate test
        NSDL2_SCHEDULE(NULL, NULL, "Last phase is running, sigterm will not effect anything.");
        return;
    }

    /*For stopping test immediately need to set gRunPhase for schedule type simple*/
    if(end_test_run_mode == 1 && (global_settings->schedule_type == SCHEDULE_TYPE_SIMPLE))
     gRunPhase = NS_ALL_PHASE_OVER; 

    force_phase_end(schedule_ptr, now);

    /*In case of stopping test gracefully, we need to set phase to ramp down*/
    if (global_settings->schedule_type == SCHEDULE_TYPE_SIMPLE && global_settings->replay_mode == 0 && (v_port_entry->num_fetches == 0) && (end_test_run_mode == 0)) {
      Ramp_up_schedule_phase *ramp_up_phase_ptr = &schedule_ptr->phase_array[1].phase_cmd.ramp_up_phase;
      Ramp_down_schedule_phase *ramp_down_phase_ptr = &schedule_ptr->phase_array[schedule_ptr->num_phases - 1].phase_cmd.ramp_down_phase;
      //Send phase complete so that nxt phase can come, as parent will send the ramp down phase;
      send_phase_complete(schedule_ptr);
      // We have to ramp down only those users which are ramped up

      if(get_group_mode(schedule_ptr->group_idx) == TC_FIX_CONCURRENT_USERS)
         reset_pct_array(schedule_ptr);
      else
         ramp_down_phase_ptr->num_vusers_or_sess = ramp_up_phase_ptr->max_ramp_up_vuser_or_sess; 
    } else {
       //In case of stopping test immediately we need to remove users
      // For RAL OR Advanced scenario we will call search and remove user
      remove_users(NULL, 1, now, 0, 0, -1);
    }
  }
  else if(global_settings->schedule_by == SCHEDULE_BY_GROUP)
  {
    //Find running runtime phase and mark them complete 
    schedule_mem_ptr = v_port_entry->runtime_schedule;
    for (rtc_idx = 0; rtc_idx < (global_settings->num_qty_rtc * total_runprof_entries); rtc_idx++)
    {
      cur_runtime_schedule = schedule_mem_ptr + (rtc_idx * find_runtime_qty_mem_size());
      NSDL4_RUNTIME(NULL, NULL, "cur_runtime_schedule->rtc_state=%d", cur_runtime_schedule->rtc_state);
      if (cur_runtime_schedule->rtc_state == RTC_RUNNING)
      { 
         stop_phase_ramp_timer(cur_runtime_schedule);
         stop_phase_end_timer(cur_runtime_schedule);
         send_phase_complete(cur_runtime_schedule); 
      }
    }
    gRunPhase = NS_ALL_PHASE_OVER;
    for(i = 0; i < total_runprof_entries; i++)
    {
      /* NetCloud: In case of generator, we can have scenario configuration with SGRP sessions/users 0.
       * Here we need to by-pass RAMP_UP check, because on generator schedule phases for such group does not exists*/
      if ((loader_opcode != CLIENT_LOADER) || 
           ((loader_opcode == CLIENT_LOADER) && (runprof_table_shr_mem[i].quantity != 0)))
      {
        schedule_ptr = &(v_port_entry->group_schedule[i]); 
        force_phase_end(schedule_ptr, now);
      }
    }
    remove_users(NULL, 1, now, 0, 0, -1);  
  }
}

/**
 * finish will be called at :
 * o    CTRL-c, 
 * o    Fetches completion, and 
 * o    last users of replay access logs started.
 */
void finish( u_ns_ts_t now )
{
  VUser *vptr = NULL;
  static short int is_finished_called; 

  /* Since, finish is called from child loop we need to protect by checking 
   * all phase over.  Earliar
   * I used flag as gRunPhase is already set to NS_ALL_PHASE_OVER,
   * in case of if all phase is over & some users are not said to ramp down  */
  //if (gRunPhase != NS_ALL_PHASE_OVER) 

  /* While ramping down users finish function is called in case of sessions used in duration phase, 
   * therefore need to check end_test_run_mode. If set then mark users for ramp-down*/
  if ((!is_finished_called) || (end_test_run_mode == 1)) {
    NSDL2_SCHEDULE(NULL, NULL, "Method called. gNumVuserActive = %d, gNumVserThinking = %d, "
                               "gNumVuserWaiting = %d, gNumVuserBlocked = %d, end_test_run_mode = %d", 
                                gNumVuserActive, gNumVuserThinking, gNumVuserWaiting, gNumVuserBlocked, end_test_run_mode);

    if (!(global_settings->schedule_by == SCHEDULE_BY_SCENARIO &&
        global_settings->schedule_type == SCHEDULE_TYPE_SIMPLE && 
        global_settings->replay_mode == 0 &&
        v_port_entry->num_fetches == 0))
        gRunPhase = NS_ALL_PHASE_OVER;

    mark_finish(now);

    NSDL2_SCHEDULE(NULL, NULL, "gNumVuserActive = %d, gNumVserThinking = %d, gNumVuserWaiting = %d, gNumVuserBlocked = %d, gRunPhase = %d", 
                         gNumVuserActive, gNumVuserThinking, gNumVuserWaiting, gNumVuserBlocked);

    //It is possible no user is left now.
    CHK_AND_CLOSE_ACCOUTING(vptr);
    /* In case of stopping test immediately code should never come here
     * because after mark finish all users should become 0 
     * and user must exit the system afer sending progress report to parent
     * */
    if ((end_test_run_mode == 1) && ((gRunPhase == NS_ALL_PHASE_OVER) && (gNumVuserActive == 0) && (gNumVuserThinking == 0) && (gNumVuserWaiting == 0) && (gNumVuserSPWaiting == 0) && (gNumVuserBlocked == 0)))
    {
      
      NS_EL_2_ATTR(EID_NS_INIT, -1, -1, EVENT_CORE, EVENT_MAJOR,
                                  __FILE__, (char*)__FUNCTION__,
                                  "In case of stopping test immediately no user should left in the system. Users exit the system and source flow should never come back");
      end_test_run();
    } 
  }
  is_finished_called = 1;
}

// --------------------- Ramp Down Immediate ----------------------------
// Here we set all num_vusers_or_sess to max_ramp_down_vuser_or_sess, so that we can ramp down all users at a time 
void immediate_ramp_down_users(u_ns_ts_t now, Schedule *schedule_ptr)
{
  ClientData cd;
  cd.p = schedule_ptr;
  Ramp_down_schedule_phase *ramp_down_phase_ptr = &schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_cmd.ramp_down_phase;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type);

  schedule_ptr->ramping_delta = ramp_down_phase_ptr->ramped_down_vusers = ramp_down_phase_ptr->num_vusers_or_sess;
  
  ramp_down_users(cd, now);
}

static void start_ramp_down_phase_fcu(Schedule *schedule_ptr, u_ns_ts_t now)
{
  int num_fetches;
  Ramp_down_schedule_phase *ramp_down_phase_ptr = &schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_cmd.ramp_down_phase;
  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d, Fetches = %u", 
                              schedule_ptr->group_idx, schedule_ptr->phase_idx, 
                              schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, v_port_entry->num_fetches);

  //Handle Duration mode SESSION 
  if(global_settings->schedule_by == SCHEDULE_BY_SCENARIO)
    num_fetches = v_port_entry->num_fetches;
  else
    num_fetches = per_proc_per_grp_fetches[(my_port_index * total_runprof_entries) + schedule_ptr->group_idx];

  NSDL2_SCHEDULE(NULL, NULL, "num_fetches = %d", num_fetches);

  //if(v_port_entry->num_fetches)
  if(num_fetches > 0)
  {
    NSDL2_SCHEDULE(NULL, NULL, "Check whether NVM ID = %d has completed all his fetches? sess_inst_num = %d, "
                               "v_port_entry->num_fetches = %d, g_ramp_down_completed = %d, total_active_runprof_entries = %d", 
                                my_child_index, sess_inst_num, v_port_entry->num_fetches, g_ramp_down_completed, total_active_runprof_entries);

    send_phase_complete(schedule_ptr);
    NSDL2_SCHEDULE(NULL, NULL, "AFTER Check whether NVM ID = %d has completed all his fetches? sess_inst_num = %d, "
                               "v_port_entry->num_fetches = %d, g_ramp_down_completed = %d, total_active_runprof_entries = %d", 
                                my_child_index, sess_inst_num, v_port_entry->num_fetches, g_ramp_down_completed, total_active_runprof_entries);

    if((sess_inst_num >= v_port_entry->num_fetches) && 
       (((global_settings->schedule_by == SCHEDULE_BY_GROUP) && (g_ramp_down_completed == total_active_runprof_entries)) ||
       (global_settings->schedule_by == SCHEDULE_BY_SCENARIO)))
      finish(now);

    return;
  }

  switch(ramp_down_phase_ptr->ramp_down_mode)
  { 
    case RAMP_DOWN_MODE_IMMEDIATE:
      immediate_ramp_down_users(now, schedule_ptr);
      break;
    case RAMP_DOWN_MODE_STEP:
      step_ramp_down_users(now, schedule_ptr);
      break;
    case RAMP_DOWN_MODE_TIME:
    {
      if(ramp_down_phase_ptr->ramp_down_pattern == RAMP_DOWN_PATTERN_LINEAR)
        time_ramp_down_users_linear_pattern(now, schedule_ptr);
      else if(ramp_down_phase_ptr->ramp_down_pattern == RAMP_DOWN_PATTERN_RANDOM)
        time_ramp_down_users_random_pattern(now, schedule_ptr);
      break;
    }
    default:
      NSTL1_OUT(NULL, NULL, "Error: Invalid ramp down mode for Group: %d, Phase: %d\n", schedule_ptr->group_idx, schedule_ptr->phase_idx);
      break;
  }
}

void start_ramp_down_phase(Schedule *schedule_ptr, u_ns_ts_t now)
{
  int scenario_type = get_group_mode(schedule_ptr->group_idx);

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phase Index = %d, Phase Type = %d, scenario_type = %d", 
                              schedule_ptr->group_idx, schedule_ptr->phase_idx, 
                              schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, scenario_type);

  //this methods sets the values of variables used in calc_iid_ms_from_rpm() & get_rpm_users()
  init_rate_intervals(schedule_ptr);

  if(scenario_type == TC_FIX_CONCURRENT_USERS)
    start_ramp_down_phase_fcu(schedule_ptr, now);
  else
    start_ramp_down_phase_fsr(schedule_ptr, now);
}
