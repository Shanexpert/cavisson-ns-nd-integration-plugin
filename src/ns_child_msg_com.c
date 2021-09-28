/******************************************************************
 * Name    :    ns_child_msg_com.c
 * Purpose :    This file contains methods related to message
                communication from child to parent
 * Note    :
 * Author  :    Archana
 * Intial version date:    08/04/08
 * Last modification date: 08/04/08
*****************************************************************/

#include <stdio.h>
#include <gsl/gsl_randist.h>
#ifdef USE_EPOLL
#include <sys/epoll.h>
#endif
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
#include "weib_think.h"
#include "unique_vals.h"
#include "divide_users.h"
#include "divide_values.h"
#include "child_init.h"
#include "ns_msg_com_util.h" 
#include "output.h"
#include "timing.h"
#include "poi.h"
#include "ns_log.h"
#include "ns_child_msg_com.h"
#include "ns_user_monitor.h"
#include "ns_percentile.h"
#include "ns_event_log.h"
#include "ns_alloc.h"
#include "ns_replay_access_logs_parse.h"
#include "tmr.h"
#include "ns_schedule_phases_parse.h"
#include "ns_schedule_phases.h"
#include "ns_schedule_start_group.h"
#include "ns_schedule_stabilize.h"
#include "ns_schedule_duration.h"
#include "ns_schedule_ramp_up_fcu.h"
#include "ns_schedule_ramp_down_fcu.h"
#include "ns_schedule_ramp_down_fsr.h"
#include "wait_forever.h"
#include "ns_sock_com.h"
#include "ns_error_codes.h"
#include "ns_string.h"
#include "ns_gdf.h"
#include "ns_event_id.h"
#include "ns_http_cache_reporting.h"
#include "dos_attack/ns_dos_attack_reporting.h"
#include "ns_common.h"
#include "ns_vuser.h"
#include "tr069/src/ns_tr069_lib.h"
#include "ns_url_hash.h"
#include "ns_netstorm_diagnostics.h"
#include "ns_ftp.h"
#include "ns_vuser_thread.h"
#include "ns_connection_pool.h" //Added for total_conn_list_head and total_conn_list_tail
#include "ns_sync_point.h"
#include "ns_proxy_server_reporting.h"
#include "ns_network_cache_reporting.h"
#include "ns_dns_reporting.h"
#include "ns_group_data.h"
#include "ns_rbu_page_stat.h"
#include "ns_page_based_stats.h"
#include "ns_ldap.h"
#include "ns_trace_level.h"
#include "ns_debug_trace.h"
#include "ns_vuser_tasks.h"
#include "ns_ip_data.h"
#include "ns_imap.h"
#include "ns_jrmi.h"
#include "ns_url_resp.h"
#include "ns_schedule_ramp_down_fsr.h"
#include "ns_runtime_runlogic_progress.h"
#include "ns_server_ip_data.h"
#include "ns_websocket_reporting.h"
#include "ns_rbu_domain_stat.h"
#include "ns_xmpp.h"
#include "ns_http_status_codes.h"
#include "ns_socket.h"
#include "ns_data_handler_thread.h"
#include "ns_test_monitor.h"
#include "ns_file_upload.h"

Msg_com_con g_child_msg_com_con, g_dh_child_msg_com_con;
Msg_com_con g_el_subproc_msg_com_con;   // EL
Msg_com_con g_dh_el_subproc_msg_com_con;   // EL
Msg_com_con g_nvm_listen_msg_con;
Msg_com_con *g_jmeter_listen_msg_con;
unsigned short g_nvm_listen_port = 0;
unsigned long all_phase_complte_start_time = 0;
unsigned long all_phase_complte_cur_time = 0;
int log_vuser_data_count = 0;

inline void send_child_to_parent_msg(char *str_opcode, char *msg, int size, int thread_flag)
{
  //Do not print msg as it is not always character type
  NSDL2_MESSAGES(NULL, NULL, "Method called, str_opcode = %s, size = %d", str_opcode, size);

  if(loader_opcode == CLIENT_LOADER)
  ((parent_child *)msg)->child_id = g_parent_idx;
  
  //absolute timestamp
  ((parent_child *)msg)->abs_ts = (time(NULL)) * 1000;

  ((parent_child *)msg)->msg_len = size - sizeof(int);
  if(thread_flag == CONTROL_MODE)
    write_msg(&g_child_msg_com_con, msg, size, 0, CONTROL_MODE);
  else
    write_msg(&g_dh_child_msg_com_con, msg, size, 0, DATA_MODE);
}

inline void fill_and_send_child_to_parent_msg(char *str_opcode, parent_child *msg, int opcode)
{
  NSDL2_MESSAGES(NULL, NULL, "Method called, str_opcode = %s, opcode = %d", str_opcode, opcode);
  msg->opcode = opcode;
  msg->child_id = my_port_index; /* This gets modified in send_child_to_parent_msg
                                  * function in case of CLIENT_LOADER */

#ifdef NS_DEBUG_ON
  if (opcode == RAMPUP_MESSAGE || opcode == RAMPUP_DONE_MESSAGE || opcode == RAMPDOWN_MESSAGE || opcode == RAMPDOWN_DONE_MESSAGE ) {
    NSDL1_MESSAGES(NULL, NULL, "Sending %s to parent. child_id = %d, num_users = %d", str_opcode, msg->child_id, msg->num_users);
  } else {
    NSDL1_MESSAGES(NULL, NULL, "Sending %s to parent. child_id = %d", str_opcode, msg->child_id);
  }
#endif
  
  send_child_to_parent_msg(str_opcode, (char*) msg, sizeof(parent_child), CONTROL_MODE);
  if(opcode == RAMPUP_DONE_MESSAGE || opcode == RAMPDOWN_DONE_MESSAGE)
    NSTL1(NULL, NULL, "%s: (NVMID:%d -> Parent), Time(ms) = %.0lf, RemainingBytesToSent = %d", str_opcode,
                     msg->child_id, msg->abs_ts, g_child_msg_com_con.write_bytes_remaining);

}

void send_ramp_up_done_msg(Schedule *schedule_ptr)
{
#ifndef NS_PROFILE
  #ifndef CAV_MAIN
  parent_child ramping_msg;

  NSDL2_MESSAGES(NULL, NULL, "Method called. Group Index = %d, Phase Index = %d, max_users_or_sess = %d", 
                 schedule_ptr->group_idx, schedule_ptr->phase_idx, 
                 schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_cmd.ramp_up_phase.num_vusers_or_sess);

  ramping_msg.grp_idx = schedule_ptr->group_idx;
  ramping_msg.phase_idx = schedule_ptr->phase_idx; 
  
  // TODO - ramped_up_vusers is more accurate but can cause confusion
  /* In case of fetches we shows max_ramp_up_vuser_or_sess, because it may possible session are fetched during ramp up phase
     session fetched are check in is new session block, so when we ramp up done from there we have the last ramped up user
     not the current.
  */
  if(get_group_mode(schedule_ptr->group_idx) == TC_FIX_CONCURRENT_USERS && v_port_entry->num_fetches == 0)
    ramping_msg.num_users = schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_cmd.ramp_up_phase.ramped_up_vusers;
  else
    ramping_msg.num_users = schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_cmd.ramp_up_phase.max_ramp_up_vuser_or_sess;

  ramping_msg.cum_users = schedule_ptr->cur_vusers_or_sess;
  fill_and_send_child_to_parent_msg("RAMPUP_DONE_MESSAGE", &ramping_msg, RAMPUP_DONE_MESSAGE);
  #endif
#endif
}

// use this fucntion instead of send_ramp_up_msg
void send_ramp_up_msg(ClientData cd, u_ns_ts_t now)
{
#ifndef NS_PROFILE
  Schedule *schedule_ptr = (Schedule *)cd.p;
  parent_child ramping_msg;

  NSDL2_MESSAGES(NULL, NULL, "Method called. Group Index = %d, Phase Index = %d, max_users_or_sess = %d", 
                 schedule_ptr->group_idx, schedule_ptr->phase_idx, 
                 schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_cmd.ramp_up_phase.num_vusers_or_sess);

  ramping_msg.grp_idx = schedule_ptr->group_idx;
  ramping_msg.phase_idx = schedule_ptr->phase_idx; 
  
  // TODO - ramped_up_vusers is more accurate but can cause confusion
  if(get_group_mode(schedule_ptr->group_idx) == TC_FIX_CONCURRENT_USERS)
    ramping_msg.num_users = schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_cmd.ramp_up_phase.ramped_up_vusers;
  else
    ramping_msg.num_users = schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_cmd.ramp_up_phase.max_ramp_up_vuser_or_sess;

  ramping_msg.cum_users = schedule_ptr->cur_vusers_or_sess; 

  fill_and_send_child_to_parent_msg("RAMPUP_MESSAGE", &ramping_msg, RAMPUP_MESSAGE);
#endif
}

void send_phase_complete(Schedule *schedule_ptr)
{ 
  VUser *vptr = NULL;
#ifndef NS_PROFILE
  parent_child parent_child_msg;
  void *schedule_mem_ptr;
  Schedule *runtime_schedule;
  Phases *ph_tmp;
  int rtc_idx, grp_idx;

  NSDL1_MESSAGES(NULL, NULL, "Method called. NVM ID = %d, Group Index = %d, Phases Index = %d, Phases Type = %d, "
                             "schedule_ptr->rtc_idx = %d, Scenario Type = %d", 
                              my_child_index, schedule_ptr->group_idx, schedule_ptr->phase_idx, 
                              schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, schedule_ptr->rtc_idx, schedule_ptr->type);

  schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_status = PHASE_IS_COMPLETED;
  #ifndef CAV_MAIN
  parent_child_msg.grp_idx = schedule_ptr->group_idx;
  parent_child_msg.phase_idx = schedule_ptr->phase_idx; 
  if(schedule_ptr->type == 0) //Non RTC schedule
  {
    fill_and_send_child_to_parent_msg("PHASE_COMPLETE", &parent_child_msg, PHASE_COMPLETE);
    // Switch and Send percentile report to the parent
    pct_run_phase_mode_chk_send_ready_msg(parent_child_msg.phase_idx, 0);
  }
  else //RTC schedule
  {
    parent_child_msg.rtc_idx = schedule_ptr->rtc_idx;
    parent_child_msg.rtc_id = schedule_ptr->rtc_id;
    NSDL4_MESSAGES(NULL, NULL, "parent_child_msg.rtc_idx = %d, parent_child_msg.rtc_id = %d", parent_child_msg.rtc_idx, parent_child_msg.rtc_id);
    NSTL1(NULL, NULL, "parent_child_msg.rtc_idx = %d, parent_child_msg.rtc_id = %d", parent_child_msg.rtc_idx, parent_child_msg.rtc_id);
    update_main_schedule(schedule_ptr);
    fill_and_send_child_to_parent_msg("RTC_PHASE_COMPLETED", &parent_child_msg, RTC_PHASE_COMPLETED);
    schedule_ptr->rtc_state = RTC_FREE; 
  }
  // If all phase over and no users left then send send_finish_report
  if(is_all_phase_over())
  {
    all_phase_complte_start_time = get_ms_stamp(); 

    NSDL2_MESSAGES(NULL, NULL, "All phase over, setting gRunPhase to NS_ALL_PHASE_OVER. Group Index = %d, Phases Index = %d, "
                               "Phases Type = %d, gNumVuserActive = %d, gNumVuserThinking = %d, gNumVuserWaiting = %d, "
                               "gNumVuserBlocked = %d, all_phase_complte_start_time = %u", 
                               schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, 
                               gNumVuserActive, gNumVuserThinking, gNumVuserWaiting, gNumVuserBlocked, all_phase_complte_start_time);

    //On completion of test phases we need to remove users or sessions added/deleted via runtime
    //Remove users or sessions immediately from system 
    if(global_settings->schedule_by == SCHEDULE_BY_SCENARIO)
    {
      schedule_mem_ptr = v_port_entry->runtime_schedule;
      for (rtc_idx = 0; rtc_idx < (global_settings->num_qty_rtc * total_runprof_entries); rtc_idx++) 
      {
        runtime_schedule = schedule_mem_ptr + (rtc_idx * find_runtime_qty_mem_size());
        if (runtime_schedule->rtc_state == RTC_RUNNING) 
        {
          ph_tmp = &runtime_schedule->phase_array[runtime_schedule->phase_idx];      
          //Stop timers eith ramp up or ramp down
          stop_phase_ramp_timer(runtime_schedule);
          stop_phase_end_timer(runtime_schedule);
          //Remove ramped up users from the system
          if (ph_tmp->phase_type == SCHEDULE_PHASE_RAMP_UP) {
             Ramp_up_schedule_phase *ramp_up_rtc_phase_ptr = &runtime_schedule->phase_array[runtime_schedule->phase_idx].phase_cmd.ramp_up_phase;
             NSDL2_MESSAGES(NULL, NULL, "Remove ramped users/session %d and making total phase user/sess %d to 0", 
                              ramp_up_rtc_phase_ptr->ramped_up_vusers, ramp_up_rtc_phase_ptr->num_vusers_or_sess);
             ramp_up_rtc_phase_ptr->num_vusers_or_sess = 0;
             if (get_group_mode(runtime_schedule->group_idx) == TC_FIX_CONCURRENT_USERS) 
               remove_users(runtime_schedule, 0, get_ms_stamp(), 1, ramp_up_rtc_phase_ptr->ramped_up_vusers, runtime_schedule->group_idx);
             else
               search_and_remove_user_for_fsr(runtime_schedule, get_ms_stamp());
          } else { //Ramp down
            Ramp_down_schedule_phase *rtc_ramp_down_phase_ptr = &runtime_schedule->phase_array[runtime_schedule->phase_idx].phase_cmd.ramp_down_phase;
            int remaining_usr_or_sess = rtc_ramp_down_phase_ptr->num_vusers_or_sess - rtc_ramp_down_phase_ptr->ramped_down_vusers;
            NSDL2_MESSAGES(NULL, NULL, "Remove remaining user in the system, num_vuser_sess = %d, ramped down user = %d, remaining_usr_sess = %d", 
                              rtc_ramp_down_phase_ptr->num_vusers_or_sess, rtc_ramp_down_phase_ptr->ramped_down_vusers, remaining_usr_or_sess);  
            if (get_group_mode(runtime_schedule->group_idx) == TC_FIX_CONCURRENT_USERS) {
              if (remaining_usr_or_sess) {
                remove_users(runtime_schedule, 0, get_ms_stamp(), 1, remaining_usr_or_sess, runtime_schedule->group_idx); 
              } 
            } else 
              search_and_remove_user_for_fsr(runtime_schedule, get_ms_stamp());
          }      
        }        
      }
    } 
    else //SCHEDULE_BY_GROUP
    {
      for (rtc_idx = 0; rtc_idx < (global_settings->num_qty_rtc * total_runprof_entries); rtc_idx++) 
      {
        for(grp_idx = 0; grp_idx < total_runprof_entries; grp_idx++) 
        {
          schedule_mem_ptr = v_port_entry->runtime_schedule;
          runtime_schedule = schedule_mem_ptr + (rtc_idx * find_runtime_qty_mem_size());
          NSDL4_RUNTIME(NULL, NULL, "runtime_schedule->rtc_state=%d, running grp_idx=%d, runtime_schedule->group_idx=%d",
                        runtime_schedule->rtc_state, grp_idx, runtime_schedule->group_idx);
          if (runtime_schedule->rtc_state == RTC_RUNNING && (grp_idx == runtime_schedule->group_idx)) 
          {
            ph_tmp = &runtime_schedule->phase_array[runtime_schedule->phase_idx];      
            //Stop timers eith ramp up or ramp down
            stop_phase_ramp_timer(runtime_schedule);
            stop_phase_end_timer(runtime_schedule);
            //Remove ramped up users from the system
            if (ph_tmp->phase_type == SCHEDULE_PHASE_RAMP_UP) {
               Ramp_up_schedule_phase *ramp_up_rtc_phase_ptr = &runtime_schedule->phase_array[runtime_schedule->phase_idx].phase_cmd.ramp_up_phase;
               NSDL2_MESSAGES(NULL, NULL, "Remove ramped users/session %d and making total phase user/sess %d to 0", 
                                ramp_up_rtc_phase_ptr->ramped_up_vusers, ramp_up_rtc_phase_ptr->num_vusers_or_sess);
               ramp_up_rtc_phase_ptr->num_vusers_or_sess = 0;
               if (get_group_mode(runtime_schedule->group_idx) == TC_FIX_CONCURRENT_USERS)  
                 remove_users(runtime_schedule, 0, get_ms_stamp(), 1, ramp_up_rtc_phase_ptr->ramped_up_vusers, runtime_schedule->group_idx);
               else
                 search_and_remove_user_for_fsr(runtime_schedule, get_ms_stamp());
            } else { //Ramp down
              Ramp_down_schedule_phase *rtc_ramp_down_phase_ptr = &runtime_schedule->phase_array[runtime_schedule->phase_idx].phase_cmd.ramp_down_phase;
              int remaining_usr_or_sess = rtc_ramp_down_phase_ptr->num_vusers_or_sess - rtc_ramp_down_phase_ptr->ramped_down_vusers;
              NSDL2_MESSAGES(NULL, NULL, "Remove remaining user in the system, num_vuser_sess = %d, ramped down user = %d, remaining_usr_sess = %d", 
                                rtc_ramp_down_phase_ptr->num_vusers_or_sess, rtc_ramp_down_phase_ptr->ramped_down_vusers, remaining_usr_or_sess);  
              if (get_group_mode(runtime_schedule->group_idx) == TC_FIX_CONCURRENT_USERS) {
                if (remaining_usr_or_sess) {
                  remove_users(runtime_schedule, 0, get_ms_stamp(), 1, remaining_usr_or_sess, runtime_schedule->group_idx); 
                } 
              } else
                search_and_remove_user_for_fsr(runtime_schedule, get_ms_stamp());  
            }      
          }
        }
      }
    }
    gRunPhase = NS_ALL_PHASE_OVER; // Setting it so that we do not need to call is_all_phase_over() from other places

    u_ns_ts_t now = get_ms_stamp(); // Needed in chk and close account macro
    CHK_AND_CLOSE_ACCOUTING(vptr);
  }
  #else
    if((schedule_ptr->phase_idx + 1) != schedule_ptr->num_phases)
       process_schedule(schedule_ptr->phase_idx + 1, schedule_ptr->phase_array[schedule_ptr->phase_idx + 1].phase_type, -1);
  #endif
#endif
}

/* void send_ramp_up_msg() */
/* { */
/*   NSDL2_MESSAGES(NULL, NULL, "Method called. max_vusers=%d", max_vusers); */
/* #ifndef NS_PROFILE */
/*   parent_child ramping_msg; */
/*   ramping_msg.num_users = max_vusers; */
/*   fill_and_send_child_to_parent_msg("RAMPUP_MESSAGE", &ramping_msg, RAMPUP_MESSAGE); */
/* #endif */
/* } */


void send_ramp_down_msg(ClientData cd, u_ns_ts_t now)
{
  NSDL2_MESSAGES(NULL, NULL, "Method called.");
#ifndef NS_PROFILE
  Schedule *schedule_ptr = (Schedule *)cd.p;
  parent_child ramping_msg;
  ramping_msg.grp_idx = schedule_ptr->group_idx;
  ramping_msg.phase_idx = schedule_ptr->phase_idx; 
 
  ramping_msg.num_users = schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_cmd.ramp_down_phase.max_ramp_down_vuser_or_sess;
  ramping_msg.cum_users = schedule_ptr->cur_vusers_or_sess;
  fill_and_send_child_to_parent_msg("RAMPDOWN_MESSAGE", &ramping_msg, RAMPDOWN_MESSAGE);
#endif
}

void send_ramp_down_done_msg(Schedule *schedule_ptr)
{
#ifndef CAV_MAIN
#ifndef NS_PROFILE
  parent_child ramping_msg;

  NSDL2_MESSAGES(NULL, NULL, "Method called. Group Index = %d, Phase Index = %d, max_users_or_sess = %d", 
                 schedule_ptr->group_idx, schedule_ptr->phase_idx, 
                 schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_cmd.ramp_up_phase.num_vusers_or_sess);

  ramping_msg.grp_idx = schedule_ptr->group_idx;
  ramping_msg.phase_idx = schedule_ptr->phase_idx; 
  
  // TODO - ramped_up_vusers is more accurate but can cause confusion
  ramping_msg.num_users = schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_cmd.ramp_down_phase.num_vusers_or_sess;
  ramping_msg.cum_users = schedule_ptr->cur_vusers_or_sess; 

  fill_and_send_child_to_parent_msg("RAMPDOWN_DONE_MESSAGE", &ramping_msg, RAMPDOWN_DONE_MESSAGE);
#endif
#endif
}

static void NVM_cleanup()
{
  NSDL1_MESSAGES(NULL, NULL, "Method called, Distroy memory for NVM = %d", my_port_index);

  if((global_settings->protocol_enabled & TR069_PROTOCOL_ENABLED)) {
    tr069_dump_cpe_data_for_each_users(v_port_entry->num_vusers, global_settings->tr069_data_dir);  
  }
  //To destroy the dynamic url hash table for each NVM
  dynamic_url_destroy();
  free_url_hash_table();
  free_big_buf();
}

static void send_error_msg_to_parent(char *msg, int status)
{
  EndTestRunMsg end_test_msg;

  NSDL1_MESSAGES(NULL, NULL, "Method called, status = %d", status);

  memset(&end_test_msg, 0, sizeof(EndTestRunMsg));

  end_test_msg.opcode = END_TEST_RUN_MESSAGE;
  end_test_msg.testidx = testidx;
  end_test_msg.child_id = my_child_index;
  end_test_msg.status = status;

  if (status == USE_ONCE_ERROR || status == MEMPOOL_EXHAUST) {
    NSDL1_MESSAGES(NULL, NULL, "message = %s", msg);
    sprintf(end_test_msg.error_msg, "%s", msg);
  }
  end_test_msg.msg_len = sizeof(EndTestRunMsg) - sizeof(int);
  write_msg(&g_dh_child_msg_com_con, (char *)(&end_test_msg), sizeof(EndTestRunMsg), 0, DATA_MODE);
}

void end_test_run_int(char *msg, int status)
{
  NSDL1_MESSAGES(NULL, NULL, "Method Called. status = %d", status);
#ifndef NS_PROFILE
  complete_leftover_write(&g_dh_child_msg_com_con, DATA_MODE);
  send_error_msg_to_parent(msg, status);
  NSDL1_MESSAGES(NULL, NULL, "Pausing for signal from parent");
  complete_leftover_write(&g_dh_child_msg_com_con, DATA_MODE);
  NVM_cleanup();
  pause();
#else
    exit(0);
#endif
}


/*
void
end_test_run( )
{
  NSDL1_MESSAGES(NULL, NULL, "Method Called.");
  end_test_run_ex("System Error", SYS_ERROR);
}*/


// This method is called by child to send progress report to parent
void
progress_report( ClientData client_data, u_ns_ts_t now )
{
  int i;
  int cum_interval_duration = 0;
  //When now - interval_start_time == 0, then vusers coming NaN, retaining previous vuser value
  avgtime *tmp_average_time;
  //TODO avg calc, if needed
  //avg is not calced yet.

  NSDL2_MESSAGES(NULL, NULL, "Method called, now = %u", now);
  // Check if any data is available in local buffer and/or shm which is not marked for writing to disk
  // This will allow low traffic test to flush data every progress interval 
  if(run_mode == NORMAL_RUN) {
    if ((now - logging_shr_mem->prev_disk_timestamp) >= global_settings->progress_secs) 
    {
      flush_shm_buffer();
    }
  }
  UPDATE_NS_DIAG_DATA(ns_diag_avgtime);
  PRINT_AND_INIT_ALLOC_STATS;

  if (cum_timestamp > now) cum_timestamp = now;

  cum_interval_duration = now - cum_timestamp;
  //Here code has been removed as RUNNING_VUSERS will be numeric and running_users cannot be NaN
  average_time->cum_user_ms += 
     (cum_interval_duration * RUNNING_VUSERS );
     //(cum_interval_duration * (gNumVuserActive + gNumVuserThinking + gNumVuserWaiting + gNumVuserSPWaiting + gNumVuserBlocked));

  //bug : 54458 : User Ramp up showing in float value 
  //average_time->avg_users = gNumVuserActive + gNumVuserThinking + gNumVuserWaiting + gNumVuserSPWaiting + gNumVuserBlocked;
 
  if (!(global_settings->protocol_enabled & JMETER_PROTOCOL_ENABLED))
    average_time->running_users = (RUNNING_VUSERS);
  else
  {
    //TODO: handle case
    // One NVM is running both jmeter script and cavisson script
    average_time->running_users = jmeter_get_running_vusers();
  }

  average_time->total_cum_user_ms += average_time->cum_user_ms;

  NSDL1_MESSAGES(NULL, NULL, "total_cum_user_ms = %llu running_users = %d", average_time->total_cum_user_ms, average_time->running_users);

  FILL_GRP_BASED_DATA;

  cum_timestamp = interval_start_time = now;

#ifdef NS_TIME

  //set Cur values
  average_time->num_connections = num_connections;
  average_time->smtp_num_connections = smtp_num_connections;
  average_time->pop3_num_connections = pop3_num_connections;
  average_time->dns_num_connections = dns_num_connections;

  if (!(global_settings->protocol_enabled & JMETER_PROTOCOL_ENABLED))
    average_time->cur_vusers_active = gNumVuserActive;
  else
  {
    //TODO: handle case
    // One NVM is running both jmeter script and cavisson script
    average_time->cur_vusers_active = jmeter_get_active_vusers();
  }


  average_time->cur_vusers_thinking = gNumVuserThinking;
  average_time->cur_vusers_waiting = gNumVuserWaiting;
  average_time->cur_vusers_cleanup = gNumVuserCleanup;
  average_time->cur_vusers_blocked = gNumVuserBlocked;
  average_time->cur_vusers_paused = gNumVuserPaused;
  //Setting Cur value of SyncPoint Vusers
  average_time->cur_vusers_in_sp = gNumVuserSPWaiting;

  FILL_GRP_BASE_VUSER;
  // This will set cumulative fields int the avgtime except error code which is done above
  //CHILD_FTP_SET_CUM_FIELD_OF_AVGTIME(ftp_avgtime);
  //CHILD_LDAP_SET_CUM_FIELD_OF_AVGTIME(ldap_avgtime);
  
#endif

  v_cur_progress_num++;
  //printf("Sending %d pid - Progress report num%d at %u\n", getpid(), v_cur_progress_num, now);
#if 0
#ifndef NS_PROFILE
  // DL_ISSUE
  if (global_settings->debug)
  {
#endif
    printf("Sending %d pid - Progress report num%d\n", getpid(), v_cur_progress_num);
    sprintf(heading, "--- %d sec, (cur=%u) (pid:%d)", v_cur_progress_num*global_settings->progress_secs/1000, get_ms_stamp(), getpid());
    print_report(stdout, NULL, URL_REPORT, 1, average_time, heading);
    sprintf(heading, "--- Cum URL, (cur=%u) (pid:%d)", get_ms_stamp(), getpid());
    print_report(stdout, NULL, URL_REPORT, 0, average_time, heading);
#ifdef NS_TIME
    sprintf(heading, "---  Pages            (pid:%d)", getpid());
    print_report(stdout, NULL, PAGE_REPORT, 1, average_time, heading);
    sprintf(heading, "---  Cum Pages        (pid:%d)", getpid());
    print_report(stdout, NULL, PAGE_REPORT, 0, average_time, heading);
 
    sprintf(heading, "---  Transactions     (pid:%d)", getpid());
    print_report(stdout, NULL, TX_REPORT, 1, average_time, heading);
    sprintf(heading, "---  Cum Transactions (pid:%d)", getpid());
    print_report(stdout, NULL, TX_REPORT, 0, average_time, heading);
    
    (void) printf("Info: (pid:%d) Vusers: Active %d, Thinking %d, Waiting %d, Idling %d, Connections %d Bytes/Sec: Rx %llu Tx %llu Payload Rx %llu\n\n",
		  getpid(),
		  gNumVuserActive,
		  gNumVuserThinking,
		  gNumVuserWaiting,
		  gNumVuserCleanup,
		  num_connections,
		  average_time->rx_bytes/(global_settings->progress_secs/1000),
		  average_time->tx_bytes/(global_settings->progress_secs/1000),
		  average_time->total_bytes/(global_settings->progress_secs/1000));
#endif
    fflush(stdout);
#ifndef NS_PROFILE
  }
#endif

#endif

  average_time->opcode = PROGRESS_REPORT;
  average_time->child_id = my_port_index;
  average_time->elapsed = v_cur_progress_num;

  /*=========================================================================== 
    [HINT: NSDynObj] 
    => For Dynamic Groups, since every NVM's can have different
       dicovery so need to tell Parent about their number of discoveries.

    => Parent will copy avgtime data by looping this number    
  ===========================================================================*/
  average_time->total_tx_entries = total_tx_entries;
  average_time->total_tcp_client_failures_entries = g_total_tcp_client_errs;
  average_time->total_udp_client_failures_entries = g_total_udp_client_errs;
  average_time->total_rbu_domain_entries = total_rbu_domain_entries;
  average_time->total_http_resp_code_entries = total_http_resp_code_entries;
  average_time->total_server_ip_entries = total_normalized_svr_ips;

  #ifdef NS_DEBUG_ON
  if(SHOW_SERVER_IP) {
    IW_UNUSED(SrvIPAvgTime *srv_ip_msg = (SrvIPAvgTime*)((char*)average_time + srv_ip_data_idx));
    NSDL1_MESSAGES(NULL, NULL, "average_time->total_server_ip_entries = %d, srv_ip_data_idx = %d, g_avgtime_size = %d, srv_ip_msg = %p", average_time->total_server_ip_entries, srv_ip_data_idx, g_avgtime_size, srv_ip_msg);
    for(i = 0; i < average_time->total_server_ip_entries; i++) {
      NSDL1_MESSAGES(NULL, NULL, "average_time = %p, data in child: i = %d, cur_url_req = %d, g_avgtime_size = %d, "
                     "ip = %s", average_time, i, srv_ip_msg[i].cur_url_req, g_avgtime_size, srv_ip_msg[i].ip);
    }
  } 
  #endif

#ifndef NS_PROFILE
  average_time->complete = client_data.i;
  insert_um_data_in_avg_time((UM_data *)((char *)average_time + g_avg_um_data_idx), (UM_data *)um_data);

  //Switch and send percentile report
  pct_time_mode_chk_send_ready_msg(average_time->complete);

/*
  if(bind_fail_count || vut_task_overwrite_count) 
    NS_DT2(NULL, NULL, DM_L1, MM_MESSAGES, "Warning: (%u) times bind failed. total overwrite task:(%u)", 
                 bind_fail_count,vut_task_overwrite_count); */
 
  
  NSDL1_MESSAGES(NULL, NULL, "Sending PROGRESS_REPORT to parent. Elapsed = %d, Complete = %d, "
                             "child_id '%d', g_avgtime_size = %d, rbu_domain_stat_avg_idx = %d",
              average_time->elapsed, average_time->complete, average_time->child_id, g_avgtime_size, rbu_domain_stat_avg_idx);

  /* NVM sending PROGRESS_REPORT data to Parent on socket */
  send_child_to_parent_msg("PROGRESS_REPORT", (char *)average_time, g_avgtime_size, DATA_MODE);

  total_tx_pr += g_dh_child_msg_com_con.write_offset;

  NSTL1(NULL, NULL, "PROGRESS_REPORT: (NVMID:%d -> Parent), SampleID = %d, IsLastSample = %d, Time(ms) = %.0lf,"
                    "RemainingBytesToSent = %d,TotalBytesSent = %d, TotalBytesReceived = %ld, "
                    "PRTotalSentBytes = %ld, PRPacketSize = %d",
                    average_time->child_id, average_time->elapsed, average_time->complete, average_time->abs_ts,
                    g_dh_child_msg_com_con.write_bytes_remaining,
                    g_dh_child_msg_com_con.total_bytes_sent, 
                    g_dh_child_msg_com_con.total_bytes_recieved, total_tx_pr, g_avgtime_size);
#endif

  for (i = 0; i < TOTAL_GRP_ENTERIES_WITH_GRP_KW; i++)
  {
    NSDL4_MESSAGES(NULL, NULL, "Reset avgtime for all metrics, i = %d", i);
    tmp_average_time = (avgtime*)((char *)average_time + (i * g_avg_size_only_grp));

    memset(tmp_average_time->url_error_codes, 0, TOTAL_URL_ERR * sizeof(int));
    memset(tmp_average_time->smtp_error_codes, 0, TOTAL_URL_ERR * sizeof(int));
    memset(tmp_average_time->pop3_error_codes, 0, TOTAL_URL_ERR * sizeof(int));
 
    memset(tmp_average_time->dns_error_codes, 0, TOTAL_URL_ERR * sizeof(int));
    memset(tmp_average_time->pg_error_codes, 0, TOTAL_PAGE_ERR * sizeof(int));
    memset(tmp_average_time->tx_error_codes, 0, TOTAL_TX_ERR * sizeof(int));
    memset(tmp_average_time->sess_error_codes, 0, TOTAL_SESS_ERR * sizeof(int));
 
    tmp_average_time->num_hits = 0;
    tmp_average_time->num_tries = 0;
    tmp_average_time->min_time = 0xFFFFFFFF;
    tmp_average_time->avg_time = 0;
    tmp_average_time->max_time = 0;
    tmp_average_time->tot_time = 0;
    tmp_average_time->fetches_started = 0;
    tmp_average_time->fetches_sent = 0;
    tmp_average_time->num_con_initiated = 0;
    tmp_average_time->num_con_succ = 0;
    tmp_average_time->num_con_fail = 0;
    tmp_average_time->num_con_break = 0;
    tmp_average_time->ssl_new = 0;
    tmp_average_time->ssl_reused = 0;
    tmp_average_time->ssl_reuse_attempted = 0;

    tmp_average_time->url_overall_avg_time = 0;
    tmp_average_time->url_overall_min_time = 0xFFFFFFFF;
    tmp_average_time->url_overall_max_time = 0;
    tmp_average_time->url_overall_tot_time = 0;

    tmp_average_time->url_failure_avg_time = 0;
    tmp_average_time->url_failure_min_time = 0xFFFFFFFF;
    tmp_average_time->url_failure_max_time = 0;
    tmp_average_time->url_failure_tot_time = 0;

    tmp_average_time->url_dns_count = 0;
    tmp_average_time->url_dns_min_time = 0xFFFFFFFF;
    tmp_average_time->url_dns_max_time = 0;
    tmp_average_time->url_dns_tot_time = 0;

    tmp_average_time->url_conn_count = 0;
    tmp_average_time->url_conn_min_time = 0xFFFFFFFF;
    tmp_average_time->url_conn_max_time = 0;
    tmp_average_time->url_conn_tot_time = 0;

    tmp_average_time->url_ssl_count = 0;
    tmp_average_time->url_ssl_min_time = 0xFFFFFFFF;
    tmp_average_time->url_ssl_max_time = 0;
    tmp_average_time->url_ssl_tot_time = 0;

    tmp_average_time->url_frst_byte_rcv_count = 0;
    tmp_average_time->url_frst_byte_rcv_min_time = 0xFFFFFFFF;
    tmp_average_time->url_frst_byte_rcv_max_time = 0;
    tmp_average_time->url_frst_byte_rcv_tot_time = 0;

    tmp_average_time->url_dwnld_count = 0;
    tmp_average_time->url_dwnld_min_time = 0xFFFFFFFF;
    tmp_average_time->url_dwnld_max_time = 0;
    tmp_average_time->url_dwnld_tot_time = 0;

    /* SMTP */
    tmp_average_time->smtp_num_hits = 0;
    tmp_average_time->smtp_num_tries = 0;
    tmp_average_time->smtp_min_time = 0xFFFFFFFF;
    tmp_average_time->smtp_avg_time = 0;
    tmp_average_time->smtp_max_time = 0;
    tmp_average_time->smtp_tot_time = 0;
    tmp_average_time->smtp_fetches_started = 0;
    tmp_average_time->smtp_num_con_initiated = 0;
    tmp_average_time->smtp_num_con_succ = 0;
    tmp_average_time->smtp_num_con_fail = 0;
    tmp_average_time->smtp_num_con_break = 0;
 
    /* POP3 */
    tmp_average_time->pop3_num_hits = 0;
    tmp_average_time->pop3_num_tries = 0;
    tmp_average_time->pop3_overall_min_time = 0xFFFFFFFF;
    tmp_average_time->pop3_overall_avg_time = 0;
    tmp_average_time->pop3_overall_max_time = 0;
    tmp_average_time->pop3_overall_tot_time = 0;
    tmp_average_time->pop3_min_time = 0xFFFFFFFF;
    tmp_average_time->pop3_avg_time = 0;
    tmp_average_time->pop3_max_time = 0;
    tmp_average_time->pop3_tot_time = 0;
    tmp_average_time->pop3_failure_min_time = 0xFFFFFFFF;
    tmp_average_time->pop3_failure_avg_time = 0;
    tmp_average_time->pop3_failure_max_time = 0;
    tmp_average_time->pop3_failure_tot_time = 0;
    tmp_average_time->pop3_fetches_started = 0;
    tmp_average_time->pop3_num_con_initiated = 0;
    tmp_average_time->pop3_num_con_succ = 0;
    tmp_average_time->pop3_num_con_fail = 0;
    tmp_average_time->pop3_num_con_break = 0;
 
    /* DNS */
    tmp_average_time->dns_num_hits = 0;
    tmp_average_time->dns_num_tries = 0;
    tmp_average_time->dns_overall_min_time = 0xFFFFFFFF;
    tmp_average_time->dns_overall_avg_time = 0;
    tmp_average_time->dns_overall_max_time = 0;
    tmp_average_time->dns_overall_tot_time = 0;
    tmp_average_time->dns_min_time = 0xFFFFFFFF;
    tmp_average_time->dns_avg_time = 0;
    tmp_average_time->dns_max_time = 0;
    tmp_average_time->dns_tot_time = 0;
    tmp_average_time->dns_failure_min_time = 0xFFFFFFFF;
    tmp_average_time->dns_failure_avg_time = 0;
    tmp_average_time->dns_failure_max_time = 0;
    tmp_average_time->dns_failure_tot_time = 0;
    tmp_average_time->dns_fetches_started = 0;
    tmp_average_time->dns_num_con_initiated = 0;
    tmp_average_time->dns_num_con_succ = 0;
    tmp_average_time->dns_num_con_fail = 0;
    tmp_average_time->dns_num_con_break = 0;

    RESET_CACHE_AVGTIME(tmp_average_time);
    RESET_PROXY_AVGTIME(tmp_average_time);
    RESET_NETWORK_CACHE_STATS_AVGTIME(tmp_average_time); 
    RESET_DNS_LOOKUP_STATS_AVGTIME(tmp_average_time); 
    
    //For FTP
    CHILD_RESET_FTP_AVGTIME(tmp_average_time); 
 
    //LDAP
    CHILD_RESET_LDAP_AVGTIME(tmp_average_time);
     
    //IMAP
    CHILD_RESET_IMAP_AVGTIME(tmp_average_time); 
 
    //JRMI
    CHILD_RESET_JRMI_AVGTIME(tmp_average_time);
     
    //DOS Attack
    RESET_DOS_ATTACK_AVGTIME(tmp_average_time); 

    //WS
    CHILD_RESET_WS_AVGTIME(tmp_average_time);     

    // Reset all variables of Socket
    CHILD_RESET_TCP_UDP_CLIENT_AVGTIME(tmp_average_time);

  #ifdef NS_TIME
    tmp_average_time->pg_hits = 0;
    tmp_average_time->pg_tries = 0;
    tmp_average_time->pg_min_time = 0xFFFFFFFF;
    tmp_average_time->pg_avg_time = 0;
    tmp_average_time->pg_max_time = 0;
    tmp_average_time->pg_tot_time = 0;
    tmp_average_time->pg_fetches_started = 0;

    tmp_average_time-> pg_succ_min_resp_time = 0xFFFFFFFF;
    tmp_average_time->pg_succ_avg_resp_time = 0;
    tmp_average_time->pg_succ_max_resp_time = 0;
    tmp_average_time->pg_succ_tot_resp_time = 0;

    tmp_average_time-> pg_fail_min_resp_time = 0xFFFFFFFF;
    tmp_average_time->pg_fail_avg_resp_time = 0;
    tmp_average_time->pg_fail_max_resp_time = 0;
    tmp_average_time->pg_fail_tot_resp_time = 0;
 
    tmp_average_time->page_js_proc_time_min = 0xFFFFFFFF;
    tmp_average_time->page_js_proc_time_max = 0;
    tmp_average_time->page_js_proc_time_tot = 0;
 
    tmp_average_time->page_proc_time_min = 0xFFFFFFFF;
    tmp_average_time->page_proc_time_max = 0;
    tmp_average_time->page_proc_time_tot = 0;
 
    tmp_average_time->tx_succ_fetches = 0;
    tmp_average_time->tx_fetches_completed = 0;
    tmp_average_time->tx_min_time = 0xFFFFFFFF;
    tmp_average_time->tx_avg_time = 0;
    tmp_average_time->tx_max_time = 0;
    tmp_average_time->tx_tot_time = 0;
    tmp_average_time->tx_tot_sqr_time = 0;
    tmp_average_time->tx_fetches_started = 0;
    tmp_average_time->tx_succ_min_resp_time = 0xFFFFFFFF;
    tmp_average_time->tx_succ_max_resp_time = 0;
    tmp_average_time->tx_succ_avg_resp_time = 0;
    tmp_average_time->tx_succ_tot_resp_time = 0;
    tmp_average_time->tx_fail_min_resp_time = 0xFFFFFFFF;
    tmp_average_time->tx_fail_max_resp_time = 0;
    tmp_average_time->tx_fail_avg_resp_time = 0;
    tmp_average_time->tx_fail_tot_resp_time = 0;
    tmp_average_time->tx_min_think_time = 0xFFFFFFFF;
    tmp_average_time->tx_max_think_time = 0;
    tmp_average_time->tx_tot_think_time = 0;
    tmp_average_time->tx_rx_bytes = 0;
    tmp_average_time->tx_tx_bytes = 0;
 
    tmp_average_time->sess_hits = 0;
    tmp_average_time->sess_tries = 0;
    tmp_average_time->sess_min_time = 0xFFFFFFFF;
    tmp_average_time->sess_avg_time = 0;
    tmp_average_time->sess_max_time = 0;
    tmp_average_time->sess_tot_time = 0;
    tmp_average_time->ss_fetches_started = 0;

    tmp_average_time->sess_succ_min_resp_time = 0xFFFFFFFF;
    tmp_average_time->sess_succ_avg_resp_time = 0;
    tmp_average_time->sess_succ_max_resp_time = 0;
    tmp_average_time->sess_succ_tot_resp_time = 0;

    tmp_average_time->sess_fail_min_resp_time = 0xFFFFFFFF;
    tmp_average_time->sess_fail_avg_resp_time = 0;
    tmp_average_time->sess_fail_max_resp_time = 0;
    tmp_average_time->sess_fail_tot_resp_time = 0;
 
    tmp_average_time->cum_user_ms = 0;

    tmp_average_time->cur_vusers_active = 0;
    tmp_average_time->cur_vusers_thinking = 0;
    tmp_average_time->cur_vusers_waiting = 0;
    tmp_average_time->cur_vusers_cleanup = 0;
    tmp_average_time->cur_vusers_in_sp = 0;
    tmp_average_time->cur_vusers_blocked = 0;
    tmp_average_time->cur_vusers_paused = 0;
    tmp_average_time->running_users = 0;

 
    tmp_average_time->total_bytes = 0;
    tmp_average_time->tx_bytes = 0;
    tmp_average_time->rx_bytes = 0;
 
    tmp_average_time->smtp_total_bytes = 0;
    tmp_average_time->smtp_tx_bytes = 0;
    tmp_average_time->smtp_rx_bytes = 0;
 
    tmp_average_time->pop3_total_bytes = 0;
    tmp_average_time->pop3_tx_bytes = 0;
    tmp_average_time->pop3_rx_bytes = 0;
    
    tmp_average_time->dns_total_bytes = 0;
    tmp_average_time->dns_tx_bytes = 0;
    tmp_average_time->dns_rx_bytes = 0;

    tmp_average_time->bind_sock_fail_min = MAX_VALUE_4B_U;
    tmp_average_time->bind_sock_fail_max = 0;
    tmp_average_time->bind_sock_fail_tot = 0;

    //Reset XMPP Stat
    if((global_settings->protocol_enabled & XMPP_PROTOCOL_ENABLED)){
      CHILD_RESET_XMPP_STAT_AVGTIME(tmp_average_time);
    }
    //Reset FC2 Stat
    if((global_settings->protocol_enabled & FC2_PROTOCOL_ENABLED)){
      CHILD_RESET_FC2_STAT_AVGTIME(tmp_average_time);
    }
    /*bug 70480 : Reset HTTP2 Server Push*/
    tmp_average_time->num_srv_push = 0;

  #endif
  }

  //Reset Cavisson Test Monitor
  if(global_settings->monitor_type == HTTP_API)
  {
    CHILD_RESET_CAVTEST_HTTP_API_AVGTIME(cavtest_http_avg);
  }
  else if(global_settings->monitor_type == WEB_PAGE_AUDIT)
  {
    CHILD_RESET_CAVTEST_WEB_PAGE_AUDIT_AVGTIME(cavtest_web_avg);
  }

  //Reset HTTP Status Code 
  CHILD_RESET_HTTP_STATUS_CODE_AVGTIME();
  //Reset TX variables  
  CHILD_RESET_TX_AVGTIME(0, txData);
  //For RBU page stat data
  CHILD_RESET_RBU_PAGE_STAT_DATA_AVGTIME(rbu_page_stat_avg); 
  //For Page Based Stat 
  CHILD_RESET_PAGE_BASED_STAT_AVGTIME(page_stat_avgtime);
  //For Group data 
  CHILD_RESET_GROUP_DATA_AVGTIME(grp_avgtime);
  CHILD_RESET_NETSTORM_DIAGNOSTICS_AVGTIME(ns_diag_avgtime);

  CHILD_RESET_TCP_CLIENT_FAILURES_AVGTIME(average_time);
  CHILD_RESET_UDP_CLIENT_FAILURES_AVGTIME(average_time);

  //For Runtime Runlogic progress
  if(SHOW_RUNTIME_RUNLOGIC_PROGRESS) {
    for(i = 0; i < total_flow_path_entries; i++) {
      vuser_flow_avgtime[i].cur_vuser_running_flow = 0; 
    } 
  }
   
  // For IP Based Stats
  if(global_settings->show_ip_data == IP_BASED_DATA_ENABLED)
  {
    for (i = 0; i < total_group_ip_entries; i++) {
      ip_avgtime[i].cur_url_req = 0;
    }
  }

  // For SRV IP Based Stats
  if(SHOW_SERVER_IP)
  {
    for (i = 0; i < total_normalized_svr_ips; i++) {
      srv_ip_avgtime[i].cur_url_req = 0;
    }
  }

  //Reset RBU Domain Stats avg data 
  CHILD_RESET_RBU_DOMAIN_STAT_AVGTIME(rbu_domain_stat_avg); 

  /* Some time test is not stoping after all phase completion so Dump vptr data on specified interval */
  all_phase_complte_cur_time = get_ms_stamp();
  unsigned int elapse_time = all_phase_complte_cur_time - all_phase_complte_start_time;
  static char vptr_to_str[MAX_TRACE_LEVEL_BUF_SIZE + 1] = {0};
  memset(vptr_to_str, 0, sizeof(vptr_to_str));
  NSDL2_MESSAGES(NULL, NULL, "global_settings->log_vuser_mode = %d, all_phase_complte_start_time = %u, all_phase_complte_cur_time = %d, log_vuser_data_interval = %d, elapse_time = %d, "
                             "log_vuser_data_count = %d, global_settings->log_vuser_data_count = %d", 
                               global_settings->log_vuser_mode, all_phase_complte_start_time, all_phase_complte_cur_time, 
			       global_settings->log_vuser_data_interval, elapse_time, 
                               log_vuser_data_count, global_settings->log_vuser_data_count);
  if((global_settings->log_vuser_mode) && (all_phase_complte_start_time != 0) && (elapse_time >= global_settings->log_vuser_data_interval))
  {
    VUser *tmp_vptr = gBusyVuserHead;
    NSDL2_MESSAGES(NULL, NULL, "Log Vuser data tmp_vptr = %p, tmp_vptr->busy_next = %p", tmp_vptr, (tmp_vptr != NULL)?tmp_vptr->busy_next:0);

    assert(log_vuser_data_count < global_settings->log_vuser_data_count);  

    while(tmp_vptr != NULL)
    {
      //NSDL4_MESSAGES(NULL, NULL, "Log Vuser data tmp_vptr = %p, VUser Dump: %s", tmp_vptr, vptr_to_string(tmp_vptr, vptr_to_str, MAX_LINE_LENGTH));
      NSTL1(tmp_vptr, NULL, "Vuser Data Dump: %s", vptr_to_string(tmp_vptr, vptr_to_str, MAX_LINE_LENGTH)); 
      tmp_vptr = tmp_vptr->busy_next;
    }

    log_vuser_data_count++;
    all_phase_complte_start_time = all_phase_complte_cur_time;
  }
}

void start_collecting_reports(u_ns_ts_t now)
{
  NSDL2_MESSAGES(NULL, NULL, "Method called, now = %u, Entering start_collecting_reports", now);

#ifndef NS_PROFILE
/*   if (remove_select_msg_com_con(parent_fd) < 0) */ /* BHAV */
/*       end_test_run(); */
/*   remove_select_msg_com_con(parent_fd); */
#endif

  global_settings->test_runphase_start_time = now = get_ms_stamp(); //Child Runphase start

  gRunPhase = NS_RUN_PHASE_EXECUTE;
}

void warmup_done()
{
  parent_child report_msg;
  NSDL2_MESSAGES(NULL, NULL, "Method called");
  
#ifndef NS_PROFILE
  
  fill_and_send_child_to_parent_msg("READY_TO_COLLECT_DATA", &report_msg, READY_TO_COLLECT_DATA);
  //printf("%d sent ready_to_collect\n", getpid());
  gRunPhase = NS_RUN_WAIT_TO_SYNC;
#endif
}

/*
void do_warmup(u_ns_ts_t now)
{
  ClientData rampcd;
  rampcd.l = 0L;

  NSDL2_MESSAGES(NULL, NULL, "Method called, now = %u", now);
  gRunPhase = NS_RUN_WARMUP;
  //if (child_global_data.warmup_sessions) {
  if (v_port_entry->warmup_sessions) {
    warmup_session_done = 0;
  }
  if (global_settings->warmup_seconds) {
    warmup_seconds_done = 0;
    ab_timers[AB_TIMEOUT_END].timeout_val = global_settings->warmup_seconds * 1000;
    dis_timer_add( AB_TIMEOUT_END, end_tmr, now, warmup_seconds_callback, rampcd, 0);
  }
  if (warmup_session_done && warmup_seconds_done)
    warmup_done();
  else {
    //if (global_settings->log_level >= 1)
    {
#ifndef NS_PROFILE
      parent_child ramping_msg;
      fill_and_send_child_to_parent_msg("WARMUP_MESSAGE", &ramping_msg, WARMUP_MESSAGE);
#endif
    }
  }
}
*/

void send_finish_report()
{
  #ifndef CAV_MAIN
  int i;
  connection* conn_cursor;

  NSDL2_MESSAGES(NULL, NULL, "Method called");

  if(global_settings->replay_mode != 0)
    write_to_last_file();
  //printf("Pid %d: Sending Finish report\n", getpid());

  // Switch and send percentile data for PERCENTILE_MODE_TOTAL_RUN 
  //TODO: Need to discuss with NJ
  pct_run_phase_mode_chk_send_ready_msg(-1, 1);

  /* Can we figure out mean bytes/sec/connection? */

  if ( do_checksum )
    {
      if ( total_badchecksums != 0 )
	(void) printf( "%d bad checksums\n", total_badchecksums );
    }

  for (conn_cursor = total_conn_list_head; conn_cursor != NULL; conn_cursor = (connection *)conn_cursor->next_in_list) {
    for (i=0; i<conn_cursor->chunk_size; i++) {
#if 0
      if (conn_cursor[i].ctx)
	SSL_CTX_free(conn_cursor[i].ctx);
#endif
#ifdef USE_EPOLL
      if (conn_cursor[i].conn_state == CNST_CONNECTING ) {
	num_reset_select++;
	if (remove_select(conn_cursor[i].conn_fd) < 0) {
	  printf("reset Select failed on WRITE  at finish\n");
          NSTL1(NULL, NULL, "reset Select failed on WRITE  at finish");
	  end_test_run();
	}
	close(conn_cursor[i].conn_fd);
      } else if (conn_cursor[i].conn_state != CNST_FREE ) {
	num_reset_select++;
	if (remove_select(conn_cursor[i].conn_fd) < 0) {
	  printf("reset Select failed on READ  at finish\n");
	  NSTL1(NULL, NULL, "reset Select failed on READ  at finish");
	  end_test_run();
	}
	close(conn_cursor[i].conn_fd);
      }
#endif
    }
  }

#ifndef NS_PROFILE
  // Check if opcode is set
  average_time->child_id = my_port_index;
  complete_leftover_write(&g_dh_child_msg_com_con, DATA_MODE);

  //Wait For File Uploader to finish
  if(global_settings->monitor_type)
  {
    int count = 600;
    while(count)
    {
      if(!nslb_alert_is_pending(g_ns_file_upload))
      {
        NSTL1(NULL, NULL, "File Upload Completed.");
        break;
      }
      //NSTL1(NULL, NULL, "File Upload is in progress, count = %d", count);
      usleep(500000);
      count--;
    }
  }

  NSDL1_MESSAGES(NULL, NULL, "Sending FINISH_REPORT to parent. Elapsed = %d, Complete = %d child_id '%d'", 
              average_time->elapsed, average_time->complete, ((parent_child *)average_time)->child_id);

  send_child_to_parent_msg("FINISH_REPORT", (char *)average_time, g_avgtime_size, DATA_MODE);
  send_child_to_parent_msg("FINISH_REPORT", (char *)average_time, g_avgtime_size, CONTROL_MODE);
  //printf("Pid %d: Sending Finish report Sent at %u\n", getpid(), get_ms_stamp());
  NSDL1_MESSAGES(NULL, NULL, "Pausing for signal from parent");

  NSDL1_MESSAGES(NULL, NULL, "Calling for CAV_MEMORY_DUMP");
  ns_process_cav_memory_map();

  complete_leftover_write(&g_dh_child_msg_com_con, DATA_MODE);
  complete_leftover_write(&g_child_msg_com_con, CONTROL_MODE);

  //Arun -- Monday, April 13 2009
  /* if a child get paused while other are working, during this if any signal comes like(SIGINT  Ctrl-C) (pause() function only returns when a signal was caught and the signal-catching  function  returned), then this child returns and start working, so to handle this we are pausing this child again.
 */
  //tr069_dump_cpe_data_for_each_users(v_port_entry->num_vusers, global_settings->tr069_data_dir);
  NVM_cleanup();

  while (1)
  {
    pause();
    NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_INFO, __FILE__, (char*)__FUNCTION__,
				"Child %d got signal other then SIGTERM during pause"
				" so pausing it again.",
				my_port_index);
  }
#else
  exit(0);
#endif
  #endif // end of CAV_MAIN
}
void process_schedule(int phase_idx,int phase_type, int grp_idx)
{
   u_ns_ts_t now;
   Schedule *cur_schedule;
   Phases *cur_phase;
   NSDL2_SCHEDULE(NULL, NULL, "Method Called");

   now = get_ms_stamp();
   //Get current phase
   if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO) // must check for  msg->grp_idx -1
     cur_schedule = v_port_entry->scenario_schedule;
   else
     cur_schedule = &(v_port_entry->group_schedule[grp_idx]);

   //set phase_idx here by msg->phase_idx
   cur_schedule->phase_idx = phase_idx;
   cur_schedule->group_idx = grp_idx;
   cur_phase = &(cur_schedule->phase_array[cur_schedule->phase_idx]);
   cur_phase->phase_status = PHASE_RUNNING;

   //Bug#3966 - Keeping it for runtime quantity to recalculate phase time left
   cur_phase->phase_start_time = now;

   // Get phase type using grp_idx & phase_idx
   
   switch(cur_phase->phase_type)
   {
     case SCHEDULE_PHASE_START:
       start_start_phase(cur_schedule, now);
       break;
     case SCHEDULE_PHASE_RAMP_UP:
       gRunPhase = NS_RUN_PHASE_RAMP;
       start_ramp_up_phase(cur_schedule, now); 
       #ifdef CAV_MAIN
       while(cur_phase->phase_status != PHASE_IS_COMPLETED)
          ramp_up_users(now);
       #endif
       break;
     case SCHEDULE_PHASE_STABILIZE:
       gRunPhase = NS_RUN_WARMUP;   
       start_stabilize_phase(cur_schedule, now); 
       break;
     case SCHEDULE_PHASE_DURATION:
       gRunPhase = NS_RUN_PHASE_EXECUTE;
       start_duration_phase(cur_schedule, now); 
       break;
     case SCHEDULE_PHASE_RAMP_DOWN:
       gRunPhase = NS_RUN_PHASE_RAMP_DOWN;
       g_ramp_down_completed++;
       start_ramp_down_phase(cur_schedule, now);
       break;
     default:
       NSTL1_OUT(NULL, NULL, "Invalid phase type (%d) recieved.", cur_phase->phase_type);
       end_test_run();
   }
}
//this method run the phase which is ordered by parent
void process_schedule_msg_from_parent(parent_child *msg)
{

   NSDL2_SCHEDULE(NULL, NULL, "Method Called," 
                  "msg->opcode = %d, msg->child_id = %d, msg->grp_idx = %d, msg->phase_idx = %d",
                  msg->opcode, msg->child_id, msg->grp_idx, msg->phase_idx);

   process_schedule(msg->phase_idx,0, msg->grp_idx);

}
