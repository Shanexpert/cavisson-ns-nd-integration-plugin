/********************************************************************
 * Name            : ns_session.c 
 * Purpose         : This file contains all the session related function of netstorm
 * Initial Version : Monday, July 13 2009
 * Modification    : -
 *******************************************************************/

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
#include "ns_error_codes.h"
#include "ns_server.h"
#include "util.h"
#include "timing.h"
#include "logging.h"
#include "tmr.h"

#include "user_tables.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_cavmain_child_thread.h"
#include "ns_schedule_phases.h"

#include "ns_event_log.h"
#include "netstorm.h"
#include "ns_log.h"
#include "ns_debug_trace.h"
#include "ns_page.h"
#include "ns_page_think_time.h"
#include "runlogic.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "ns_session.h"
#include "ns_percentile.h"
#include "output.h"
#include "ns_vuser.h"
#include "divide_users.h"
#include "divide_values.h"
#include "child_init.h"
#include "ns_schedule_ramp_down_fcu.h"
#include "ns_schedule_ramp_up_fcu.h"
#include "ns_schedule_ramp_up_fsr.h"
#include "ns_schedule_phases_parse.h"
#include "ns_keep_alive.h"
#include "ns_data_types.h"
#include "ns_http_cache.h"
#include "ns_http_cache_reporting.h"
#include "ns_vuser_tasks.h"
#include "ns_auto_fetch_embd.h"
#include "ns_vuser_ctx.h"
#include "ns_http_cache_store.h"
#include "nslb_time_stamp.h"
#include "ns_trans.h"
#include "ns_replay_access_logs.h"

#include "ns_trans.h"
#include "ns_vuser_trace.h"
#include "ns_session_pacing.h"
#include "ns_child_thread_util.h"
#include "ns_vuser_thread.h"
#include "ns_page_dump.h"
#include "ns_event_id.h"
#include "ns_string.h"
#include "ns_alloc.h"
#include "ns_script_parse.h"

// For njvm changes
#include "ns_njvm.h"
#include "ns_nvm_njvm_msg_com.h"
#include "ns_trace_level.h"
#include "ns_group_data.h"
#include "ns_gdf.h"
//For FCS
#include "ns_schedule_fcs.h"
#include "nslb_mem_pool.h"
#include "ns_exit.h"
#include "ns_jmeter.h"
#include "ns_handle_alert.h"
#include "ns_xmpp.h"
#include "ns_sockjs.h"

unsigned int sess_inst_num = 0;
unsigned int concurrent_sess_num = 0;
nslb_mp_handler *sess_pool = NULL;

u_ns_ts_t cur_time;

void init_sess_inst_start_num()
{
  if(global_settings->continuous_monitoring_mode)
  {
    sess_inst_num = g_time_diff_bw_cav_epoch_and_ns_start;
    NSTL1(NULL, NULL, "Initializing sess_inst_num to g_time_diff_bw_cav_epoch_and_ns_start (%d), sess_inst_num = %u", g_time_diff_bw_cav_epoch_and_ns_start, sess_inst_num);
  }
  else
  {
    sess_inst_num = 0;
    NSTL1(NULL, NULL, "Initializing sess_inst_num to 0, sess_inst_num = %u.", g_time_diff_bw_cav_epoch_and_ns_start, sess_inst_num);
  }
}

/*-------------------------------------------------------
How session end is handled?

In case of legacy script:
  After normal completion of session
  If page fails and continue on page error is false
  If user is ramped down and ramp down method - Allow current page to complete
  If user is ramped down and ramp down method - stop immediately mode is used
    on_session_completion() is called.

In case of C script:
  After normal completion of session
    Script calls ns_exit_session()
  If page fails and continue on page error is false
  If user is ramped down and ramp down method - Allow current page to complete
  If user is ramped down and ramp down method - stop immediately mode is used
    on_session_completion() is called.

-------------------------------------------------------*/

extern void log_session_status(VUser *vptr);
extern int log_session_record(VUser* vptr, u_ns_ts_t now);

//Macro to check memory pool is FULL
/*
  FCS logic for alert generation in case of memory pool exhaustion
  due to more number of blocked users than memory pool size available
  HLD:
    Fill dashboardInfo and alertInfo and call library function to send alert
    dashboardInfo: structure to fill dashboard info (ip, port, protocol)
    alertInfo:     structure to fill alert values (message, alertValue, testid, policy, severity)
*/
#define IS_MEMPOOL_EXHAUSTED(pool) \
{  \
  if(pool && pool->free_head == NULL)   \
  { \
    NSTL1_OUT(NULL, NULL, "[Fix Concurrent Sessions] Memory Pool got exhausted for NVM%d, exiting.", my_child_index); \
    NS_EL_2_ATTR(EID_MISC,  -1, -1, EVENT_CORE, EVENT_CRITICAL, __FILE__, (char*)__FUNCTION__, "[Fix Concurrent Sessions] Memory Pool got exhausted for NVM%d", my_child_index); \
    char alert_msg[ALERT_MSG_SIZE];\
    sprintf(alert_msg, "[Fix Concurrent Sessions] Memory Pool got exhausted for NVM%d", my_child_index);\
    ns_send_alert(ALERT_CRITICAL, alert_msg);\
    end_test_run_ex("Mempool Exhausted", MEMPOOL_EXHAUST); \
  }\
}

static int inline
is_new_session_blocked (VUser *vptr, u_ns_ts_t now)
{
  unsigned int local_sess_inst_num;
  int ret;
 

  NSDL1_SCHEDULE(vptr, NULL, "Method called, v_port_entry->num_fetches = %d, sess_inst_num = %u, vptr = %p",
                              v_port_entry->num_fetches, sess_inst_num, vptr);  

  // If over, then it sill send finish report and will never return
  // CAV_MAIN: For the time being stopping here
  #ifndef CAV_MAIN
  CHK_AND_CLOSE_ACCOUTING(vptr);
  #endif

  /* Fix Concurrent sessions check, if limit of FCS reached then block/save vptr
     In G_NEW_USER_ON_SESSION no need to save vptr, as vptr is always new      */
  if(vptr->sess_status != NS_SESSION_ABORT)
  {
    if(global_settings->concurrent_session_mode && (runprof_table_shr_mem[vptr->group_num].pacing_ptr->refresh != REFRESH_USER_ON_NEW_SESSION))
    {
      /* Initialization of mem pool here*/
      if(sess_pool == NULL) {
        MY_MALLOC(sess_pool, sizeof(nslb_mp_handler), "nslb_mp_handler ptr", 1);
        if((ret = nslb_mp_init(sess_pool, sizeof(Pool_vptr), global_settings->concurrent_session_pool_size, 2, NON_MT_ENV)) == MP_EINVAL)
        {
          NSTL1(vptr, NULL, "\nERROR: invalid arguments passed for child = %d, errno = %d", my_child_index, ret);
          NS_EXIT(-1, "\nERROR: invalid arguments passed for child = %d, errno = %d", my_child_index, ret);
        }
  
        if((ret = nslb_mp_create(sess_pool)))
        {
          NSTL1(vptr, NULL, "\nERROR: in allocating memory pool for child = %d, errno = %d", my_child_index, ret);
          NS_EXIT(-1, "\nERROR: in allocating memory pool for child = %d, errno = %d", my_child_index, ret);
        }
      }
      //check if limit reached on machine and saving vptr in pool
      if(concurrent_sess_num == v_port_table[my_port_index].limit_per_nvm) {
        IS_MEMPOOL_EXHAUSTED(sess_pool);
        NSDL2_SCHEDULE(vptr, NULL, "[FCS] limit = %d, concurrent_sess_num = %u, vptr = %p", v_port_table[my_port_index].limit_per_nvm,
                                    concurrent_sess_num, vptr);
        NSTL1(vptr, NULL, "Sessions limit on system has been reached so not allowing session");
        Pool_vptr *pool_vptr  = (Pool_vptr *)nslb_mp_get_slot(sess_pool);
        VUSER_ACTIVE_TO_BLOCKED(vptr);
        pool_vptr->sav_vptr = vptr;
        NSTL1(vptr, NULL, "FCS Blocking: Child = %d, Group = %s, Script = %s, Sess Inst = %d", my_child_index,
                           runprof_table_shr_mem[vptr->group_num].scen_group_name, vptr->sess_ptr->sess_name, sess_inst_num);
        
        return -1;
      }
      else {
        //session is allowed
        concurrent_sess_num++;
        NSDL1_SCHEDULE(vptr, NULL, "[FCS] concurrent_sess_num = %u, vptr = %p", concurrent_sess_num, vptr);
      }
    } 
  }

 //Commented : No need to increment vptr session ptr.
  //vptr->sess_inst = sess_inst_num++;
  local_sess_inst_num = sess_inst_num++;
  #ifdef CAV_MAIN
  vptr->sm_mon_info->sess_inst_num = sess_inst_num;
  #endif
  Schedule *schedule_ptr;
  Phases *ph_ptr;
  int max_num_fetches, num_sess;
  if(global_settings->schedule_by == SCHEDULE_BY_SCENARIO)
  {
    NSDL1_SCHEDULE(vptr, NULL, "SCHEDULE_BY_SCENARIO");
    schedule_ptr = v_port_entry->scenario_schedule;
    ph_ptr = &(schedule_ptr->phase_array[schedule_ptr->phase_idx]);  //4th phase is DURATION Phase
    num_sess = sess_inst_num;
    max_num_fetches = v_port_entry->num_fetches; 
  }
  else
  { 
    NSDL1_SCHEDULE(vptr, NULL, "SCHEDULE_BY_GROUP, per_grp_sess_inst_num = %d", per_grp_sess_inst_num[vptr->group_num]);
    per_grp_sess_inst_num[vptr->group_num]++;

    schedule_ptr = &(v_port_entry->group_schedule[vptr->group_num]);
    ph_ptr = &(schedule_ptr->phase_array[schedule_ptr->phase_idx]);  //4th phase is DURATION Phase
    num_sess = per_grp_sess_inst_num[vptr->group_num]; 
    max_num_fetches = per_proc_per_grp_fetches[(my_port_index * total_runprof_entries) + vptr->group_num];
  }


  NSDL1_SCHEDULE(vptr, NULL, "my_port_index = %d, max_num_fetches = %d, num_sess = %d, group_num = %d, local_sess_inst_num = %d", 
                              my_port_index, max_num_fetches, num_sess, vptr->group_num, local_sess_inst_num);
  // > case should not be applicable. Is there, just in case
  // TODO:  take fetches from duration phase
  if (max_num_fetches && (num_sess == max_num_fetches)) {
     if(ph_ptr->phase_type == SCHEDULE_PHASE_RAMP_UP)
       ramp_up_phase_done_fcu(schedule_ptr);
     else if((ph_ptr->phase_type == SCHEDULE_PHASE_DURATION) && (ph_ptr->phase_status == PHASE_RUNNING))
     {
       NSDL1_SCHEDULE(vptr, NULL, "Sending duration phase complete message for my_port_index = %d, schedule_ptr->phase_idx = %d", 
                        my_port_index, schedule_ptr->phase_idx);
       send_phase_complete(schedule_ptr);
     }
     else {
       NSDL4_SCHEDULE(vptr, NULL, "ph_ptr->phase_type = %d, ph_ptr->phase_status = %d, schedule_ptr->phase_idx = %d", 
                                                            ph_ptr->phase_type, ph_ptr->phase_status, schedule_ptr->phase_idx );
     }
  }else if(max_num_fetches && (num_sess > max_num_fetches)){
    VUSER_DEC_ACTIVE(vptr);
    user_cleanup(vptr, now);
    //return 1;
    // BugId - 77969  As soon as user cleanup done here we have to 
    // call close accounting in case all user removed else test will stuck 
    #ifndef CAV_MAIN
    CHK_AND_CLOSE_ACCOUTING(vptr);
    #endif
    return -2;
  }

  vptr->sess_inst = local_sess_inst_num;

  return 0;
}

static inline void calc_avg_session_time(VUser *vptr, u_ns_ts_t now) {
  u_ns_ts_t download_time;

  //avgtime *lol_average_time;

  download_time = now - vptr->started_at;
  NSDL4_SCHEDULE(vptr, NULL, "At Session : download_time - %lld", download_time);

  if (!vptr->sess_status)   //Successful  Session 
  {
    NSDL4_SCHEDULE(vptr, NULL, "Session Successful");
    average_time->sess_hits++;
    SET_MIN (average_time->sess_succ_min_resp_time, download_time);
    SET_MAX (average_time->sess_succ_max_resp_time, download_time);
 
    average_time->sess_succ_tot_resp_time += download_time;

    if(SHOW_GRP_DATA) {
      avgtime *lol_average_time;
      lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
      SET_MIN (lol_average_time->sess_succ_min_resp_time, download_time);
      SET_MAX (lol_average_time->sess_succ_max_resp_time, download_time);
      lol_average_time->sess_succ_tot_resp_time += download_time;
      lol_average_time->sess_hits++;
    } 

    NSDL4_SCHEDULE(vptr, NULL, "Session Successful : average_time->sess_hits - %d, average_time->sess_succ_min_resp_time - %d, "
                               "average_time->sess_succ_max_resp_time - %d, average_time->sess_succ_tot_time - %d",
                                average_time->sess_hits, average_time->sess_succ_min_resp_time, average_time->sess_succ_max_resp_time,
                                average_time->sess_succ_tot_resp_time); 
  }
  else
  {
    NSDL4_SCHEDULE(vptr, NULL, "Session Failed");
    SET_MIN (average_time->sess_fail_min_resp_time, download_time);
    SET_MAX (average_time->sess_fail_max_resp_time, download_time);
 
    average_time->sess_fail_tot_resp_time += download_time;

    if(SHOW_GRP_DATA) {
      avgtime *lol_average_time;
      lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
      SET_MIN (lol_average_time->sess_fail_min_resp_time, download_time);
      SET_MAX (lol_average_time->sess_fail_max_resp_time, download_time);
      lol_average_time->sess_fail_tot_resp_time += download_time;
    } 

    NSDL4_SCHEDULE(vptr, NULL, "Session Failed: average_time->sess_hits - %d, average_time->sess_fail_min_resp_time - %d, "
                               "average_time->sess_fail_max_resp_time - %d, average_time->sess_fail_tot_time - %d",
                                average_time->sess_hits, average_time->sess_fail_min_resp_time, average_time->sess_fail_max_resp_time,
                                average_time->sess_fail_tot_resp_time);
  }

  //Overall Session Data
  SET_MIN (average_time->sess_min_time, download_time);
  SET_MAX (average_time->sess_max_time, download_time);
 
  average_time->sess_tot_time += download_time;

  if(SHOW_GRP_DATA) {
    avgtime *lol_average_time;
    lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
    SET_MIN (lol_average_time->sess_min_time, download_time);
    SET_MAX (lol_average_time->sess_max_time, download_time);
    lol_average_time->sess_tot_time += download_time;
  } 

  NSDL4_SCHEDULE(vptr, NULL, "Session Overall: average_time->sess_min_time - %d, "
                             "average_time->sess_max_time - %d, average_time->sess_tot_time - %d",
                              average_time->sess_min_time, average_time->sess_max_time,
                              average_time->sess_tot_time); 

  if (g_percentile_report == 1)
    update_pdf_data(download_time, pdf_average_session_response_time, 0, 0, 0);
}

static inline void upd_session_time(VUser *vptr, u_ns_ts_t now) 
{
  if(vptr->sess_status == NS_USEONCE_ABORT || vptr->sess_status == NS_UNIQUE_RANGE_ABORT_SESSION)
    vptr->sess_status = NS_REQUEST_ERRMISC; 

  calc_avg_session_time(vptr, now);

  if (vptr->sess_status)
  {
      if ((vptr->sess_status > 0) && (vptr->sess_status < TOTAL_SESS_ERR)) {
        average_time->sess_error_codes[vptr->sess_status]++;
        if(SHOW_GRP_DATA)
        {
          avgtime *lol_average_time;
          lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
          lol_average_time->sess_error_codes[vptr->sess_status]++;
        }
      }
      else
      {
        printf("Unkonw Session Status\n");
        average_time->sess_error_codes[NS_REQUEST_ERRMISC]++;
        if(SHOW_GRP_DATA)
        {
          avgtime *lol_average_time;
          lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
          lol_average_time->sess_error_codes[NS_REQUEST_ERRMISC]++;
        }
      }
  }
  /* vptr->sess_status is used later to check for session sucesss and failure if continuing with same vuser and user want(s) to 
     retain parameter and cookie values used by vuser if session is success. So this MUST NOT RESET HERE
     vptr->sess_status sets to 0 whenever new session starts . Therfore skipping to reset vptr->sess_status here */ 
  //vptr->sess_status = 0;
  
}

// In case of do not reuse use, cleanup time is valid
// If cleanup time is started, then there will be two users
//    Once in cleanup state and one new started
static inline void chk_for_cleanup(VUser *vptr, connection *cptr, int do_cleanup, u_ns_ts_t now)
{
  NSDL3_SCHEDULE(vptr, cptr, "Method called. do_cleanup = %d, Vuser stat = %d, do_cleanup = %d, gNumVuserActive = %d, gNumVuserThinking = %d, gNumVuserWaiting = %d, gNumVuserCleanup = %d, gNumVuserBlocked = %d", do_cleanup, vptr->vuser_state, do_cleanup, gNumVuserActive, gNumVuserThinking, gNumVuserWaiting, gNumVuserCleanup, gNumVuserBlocked); 
  //Ignore cleanup pct fro now
  //if (global_settings->user_cleanup_time && (global_settings->user_cleanup_pct <= (ns_get_random(gen_handle) % 100)))
 
  // If cleanup time is given and do cleanup is true, then we need to 
  // start timer for cleanup
  if (runprof_table_shr_mem[vptr->group_num].gset.user_cleanup_time && do_cleanup)
  {
    //TODO: Achint Need to check if PAGE DUMP need to check
    if(NS_IF_TRACING_ENABLE_FOR_USER){
      ut_vuser_check_and_disable_tracing(vptr);
    }

    VUSER_TO_CLEANUP(vptr); //changing the state to cleanup 

    ClientData cd;
    /* Following code is commented because we may have no more free
     * connections left for that user when we reach here. */
    /*
    if (!cptr)
    {
      cptr = get_free_connection_slot(vptr);
      assert(cptr != NULL);
    }
    cd.p = cptr;
    */

    cd.p = vptr;

    //ab_timers[AB_TIMEOUT_UCLEANUP].timeout_val = runprof_table_shr_mem[vptr->group_num].gset.user_cleanup_time;
    vptr->timer_ptr->actual_timeout = runprof_table_shr_mem[vptr->group_num].gset.user_cleanup_time;

    NSDL3_SCHEDULE(vptr, cptr, "Starting user cleanup timer: user_cleanup_time = %d", 
		    runprof_table_shr_mem[vptr->group_num].gset.user_cleanup_time);

    (void) dis_timer_add( AB_TIMEOUT_UCLEANUP, vptr->timer_ptr, now, user_cleanup_timer, cd, 0);
  }
  else // otherwise remove the user from the system by doing all cleanup
  {
    user_cleanup(vptr, now);
  }
}

static void allocate_vuser_thread_data(VUser *vptr)
{
  NSDL1_SCHEDULE(vptr, NULL, "Method called");
  if(vptr->thdd_ptr == NULL) {
    MY_MALLOC(vptr->thdd_ptr, sizeof(VUserThdData), "vptr->thdd_ptr", -1);
    memset(vptr->thdd_ptr, 0, sizeof(VUserThdData));
  }
}


/* Commenting it as it is not called due to performance issues
void free_vuser_thread_data(VUser *vptr)
{
  NSDL1_SCHEDULE(vptr, NULL, "Method called");
  FREE_AND_MAKE_NULL(vptr->thdd_ptr->eval_buf, "vptr->thdd_ptr->eval_buf", -1);
  vptr->thdd_ptr->eval_buf_size = 0;
  FREE_AND_MAKE_NULL(vptr->thdd_ptr->end_as_tx_name, "vptr->thdd_ptr->eval_buf", -1);
  vptr->thdd_ptr->end_as_tx_name_size = 0;
  FREE_AND_MAKE_NULL(vptr->thdd_ptr, "vptr->thdd_ptr", -1);
}
*/
// This method is called in following cases:
// Normal cases
//   1. After last page (including page think time) is done (before sesion pacing)
//   2. In case of dummy script, after init_script() method 
// User flaged for ramp down
//   1. Ramp down method mode 0 - same as above
//   2. Ramp down method mode 1 - After current page is complete (before page think time)
//   3. Ramp down method mode 2 - called if user is active or in page think state
//   
// In case mode > 0, this will be called from C code/thread

void nsi_end_session(VUser *vptr, u_ns_ts_t now)
{
connection * cptr;
int do_cleanup;
int group_mode;
int group_num;
int grp_idx;

  /* RBU: Here we stop browser if stop_browser_on_sess_end_flag is on */
  // stop_browser_on_sess_end_flag
  NSDL3_SCHEDULE(vptr, vptr->last_cptr, "RBU handle on session end for group %d, enable_rbu = %d", 
                                         vptr->group_num, runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_rbu); 
  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_rbu) 
    ns_rbu_on_sess_end(vptr);

  if((runprof_table_shr_mem[vptr->group_num].gset.num_retry_on_page_failure > 0) && (vptr->sess_status != NS_SESSION_ABORT))
    vptr->retry_count_on_abort = 0;

  NSDL3_SCHEDULE(vptr, vptr->last_cptr, "retry_count_on_abort = %d", vptr->retry_count_on_abort);

  
  if((vptr->flags & NS_XMPP_ENABLE) && vptr->xmpp_cptr) 
     xmpp_close_disconnect(vptr->xmpp_cptr, now, 1, 0);

  if(global_settings->protocol_enabled & SOCKJS_PROTOCOL_ENABLED)
    sockjs_close_connection_ex(vptr);

  // This will close all Tx will are not yet closed 
  // Two cases:
  //   User is ramped down with stop user immediately mode
  //   In non legacy script, end tx api is not called in the script
  // Tx time will include think time if any
  tx_logging_on_session_completion (vptr, now, vptr->page_status);
 
  NSDL3_SCHEDULE(vptr, vptr->last_cptr, "Calling script exit log function");
  //run_script_exit_func(vptr, now);
  script_exit_log(vptr, now);

  if(g_debug_script)
    NS_DT1(vptr, NULL, DM_L1, MM_SCHEDULE, "Ending Session '%u'",  vptr->sess_inst + 1);

  grp_idx = vptr->group_num;
  //Bug#2426
  do_cleanup = vptr->flags & NS_DO_CLEANUP;
  vptr->flags &= ~NS_DO_CLEANUP;

  cptr = vptr->last_cptr;

  NSDL3_SCHEDULE(vptr, cptr, "vptr->last_cptr = %p", cptr);
  group_mode = get_group_mode(vptr->group_num);
#if 0
//In release 3.9.7: Page dump feature has been redesign now session status log file is not created
  if(NS_IF_PAGE_DUMP_ENABLE && (runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_ALL_SESS || runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_PAGE_ON_HEADER_BASED))
    log_session_status(vptr); /* logs session status to sess_status_log file */
#endif

#ifdef NS_TIME
  if(vptr->sess_ptr->jmeter_sess_name == NULL)
  {
    average_time->sess_tries++;
    if(SHOW_GRP_DATA)
    {
      avgtime *lol_average_time;
      lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
      lol_average_time->sess_tries++;
    }
  }
#endif
  /* 
    Bug - 111183, 111297 - We have moved this code before log_session record due to the below reason:
    During session start, we have masked the PAGE_DUMP FLAG for Vuser. This page dump flag is a new flag introduced
    for setting ddr limit. Now, In macro DDR_CHECK_FOR_SESSION_RECORD, We have a check for if PAGE_DUMP is enable for the Vuser or not.
    Consider a situation where we have setting of G_TRACING ALL 4 1 2. According to this configuration we have to trace page dump of
    failed page/transaction. In previous code, check for DDR_CHECK_FOR_SESSION_RECORD was not getting passed as PAGE_DUMP was not dumped
    for the failed session as this dumping was happening in the later part of the code. And due to this, records in DB were getting
    mismatched between SessionRecord and PageDump table and hence we were not having page dump report getting displayed.
    So, with this movement of code, now start_dumping_page_dump will call first log_page_dump_record and make PAGE_DUMP flag on for
    vptr and which will make the check DDR_CHECK_FOR_SESSION_RECORD as PASS.
  */
  //Check if page dump is enable for this user
  //Also check for trace on faile only for failure cases will dump from here 
  if(NS_IF_PAGE_DUMP_ENABLE_WITH_TRACE_ON_FAIL){
    start_dumping_page_dump(vptr, now);
  }

  /* In release 3.9.7: Page dump feature has been redesign we need src.csv to get session status hence creating csv file 
  *  if tracing is enable*/
  if (DDR_CHECK_FOR_SESSION_RECORD) {
    NSDL3_SCHEDULE(vptr, cptr, "Call log_session_record function");
    #ifndef CAV_MAIN
    if (log_session_record(vptr, now) == -1)
      NSTL1_OUT(NULL, NULL, "Error in creating a session record\n");
    #endif
  }

  if(NS_IF_TRACING_ENABLE_FOR_USER ||
     NS_IF_PAGE_DUMP_ENABLE_WITH_TRACE_ON_FAIL)
    ut_add_end_session_node(vptr);
  
  if(vptr->sess_ptr->jmeter_sess_name == NULL)  
    upd_session_time(vptr, now) ;

  if (cum_timestamp > now) cum_timestamp = now;
  average_time->cum_user_ms += ((now - cum_timestamp) * (gNumVuserActive + gNumVuserThinking + gNumVuserWaiting + gNumVuserSPWaiting + gNumVuserBlocked));

  if(SHOW_GRP_DATA)
  {
    avgtime *lol_average_time;
    lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
    lol_average_time->cum_user_ms += (now - cum_timestamp) * (grp_vuser[vptr->group_num].cur_vusers_active + grp_vuser[vptr->group_num].cur_vusers_thinking + grp_vuser[vptr->group_num].cur_vusers_waiting + grp_vuser[vptr->group_num].cur_sp_users + grp_vuser[vptr->group_num].cur_vusers_blocked + grp_vuser[vptr->group_num].cur_vusers_paused); 
  }

  cum_timestamp = now;
  // Dcreament active acount as it is again incrementing if we are
  // resusing user or starting new user
  VUSER_DEC_ACTIVE(vptr);
  #ifndef CAV_MAIN
  decr_nvm_users(vptr->group_num); //vusers count for each NVM
  #endif

  if(NS_IF_CACHING_ENABLE_FOR_USER)
  { //this is the check for cache table size mode 1
    if(runprof_table_shr_mem[grp_idx].gset.cache_table_size_mode == CACHE_TABLE_RECORDED)
    {
      cache_set_cache_table_size_value(grp_idx, vptr->httpData->max_cache_entries);
    }
  }


  if(NS_IF_CACHING_ENABLE_FOR_USER)
  {
    // Here same user is continuing and mode is set to do not cache across session
    // So we are freeing cache of the user
    if(runprof_table_shr_mem[vptr->group_num].gset.cache_mode) // Do not cache across session
    {
      NSDL2_CACHE(vptr, cptr, "Caching mode is %d", runprof_table_shr_mem[vptr->group_num].gset.cache_mode);
      //Decrement total cache entries
      //cache_avgtime->cache_num_entries--;
      free_cache_for_user(vptr);
    }
  }

  // Stack cannot be freed till context is switched back to NVM
  // We are freeing the stack as we allocating in create_vuser_ctx()
  // This is done for memory optimization as during session pacing
  // we do not want to hold on to this memory.
  // This will have peformance impact
  // Must be done after switching to NVM context as for switching stack is required
  // If thread, stop thread, free and send msg to vuser thread
  if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT)
  {
    free_vuser_ctx(vptr);
  } 
  else if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_SEPARATE_THREAD)
  {
    if(vptr->mcctptr)
    {
      vutd_stop_thread(NULL, vptr->mcctptr, VUTD_STOP_THREAD_FREE);
      send_msg_nvm_to_vutd(vptr, NS_API_END_SESSION_REQ, 0);
      vptr->mcctptr->vptr = NULL;
      vptr->mcctptr = NULL;
    }
  } 
  else if(vptr->sess_ptr->script_type == NS_SCRIPT_TYPE_JAVA)
  {
    if(vptr->mcctptr)
    {
      /* End session is added for the case when USE_ONCE DATA exhaust case is found */
      send_msg_to_njvm(vptr->mcctptr, NS_NJVM_API_END_SESSION_REP, 0);
      njvm_free_thread(vptr->mcctptr);
      vptr->mcctptr->vptr = NULL;
      vptr->mcctptr = NULL;
    }
  }

  if(vptr->sess_status != NS_SESSION_ABORT)
  {
    if (vptr->flags & NS_VUSER_RAMPING_DOWN)
    {
      NSDL2_SCHEDULE(vptr, cptr, "User Ramping down");
      user_cleanup(vptr, now); // Since user is flaged for ramp down, clean up
      #ifndef CAV_MAIN
      CHK_AND_CLOSE_ACCOUTING(vptr);
      #endif
      return;
    }
  }

  //Effectively, user_reuse_mode is load_key. Load_key is by default 0 for session rate
  //and 1 for num_user mode. 1 for fix concurrent users, and 0 for session rate and mean users

  // Bug 415 - On Ramp down users were becoming negative.
  // Earliar this was a switch case for TC_FIX_CONCURRENT_USERS & TC_FIX_USER_RATE for GOAL BASED it was not doing user cleanup,
  // as we have a concept of: Each user will hit 1 session & cleaned up.
  // So now we are doing chk_for_cleanup for all else
  if(group_mode == TC_FIX_CONCURRENT_USERS) {
      //PacingTableEntry_Shr* pacing_ptr = runprof_table_shr_mem[vptr->group_num].pacing_ptr;
      NSDL2_SCHEDULE(vptr, cptr, "vptr->sess_ptr->pacing_ptr->refresh = %d"
                      " vptr->sess_ptr->pacing_ptr->pacing_mode = %d, cptr=%p, sess_status = %d",
                      runprof_table_shr_mem[vptr->group_num].pacing_ptr->refresh,
                      runprof_table_shr_mem[vptr->group_num].pacing_ptr->pacing_mode, cptr, vptr->sess_status);

      group_num = vptr->group_num;
      if (runprof_table_shr_mem[group_num].pacing_ptr->pacing_mode == SESSION_PACING_MODE_EVERY)
      {
        scen_group_adjust[group_num] += (now - vptr->started_at);
      }

      // Start new user
      // Check if we need to start new user after session or not
      if(vptr->sess_status != NS_SESSION_ABORT) {
        if (runprof_table_shr_mem[group_num].pacing_ptr->refresh == REFRESH_USER_ON_NEW_SESSION) 
        {
          chk_for_cleanup(vptr, cptr, do_cleanup, now);
          // Start new user
          new_user(1, now, group_num, 0, vptr->uniq_rangevar_ptr, vptr->sm_mon_info);
          //new_user(1, now, group_num, 0, vptr->uniq_rangevar_ptr);
        }
        else
        {
          //FCS check in reuse as in new_user vptr will be cleaned
          if(global_settings->concurrent_session_mode)
          {
            IS_MEMPOOL_EXHAUSTED(sess_pool);
            //decreasing concurrent session counter
            concurrent_sess_num--;
            Pool_vptr *pool_vptr = nslb_mp_get_slot(sess_pool);
            vptr->vuser_state = NS_VUSER_BLOCKED;
            VUSER_INC_BLOCKED(vptr);
            NSDL3_SCHEDULE(vptr, NULL, "[FCS] Session ends, Previous: pool_vptr = %p, vptr = %p, concurrent_sess_num = %d",
                                        pool_vptr , vptr, concurrent_sess_num);
            pool_vptr->sav_vptr = vptr;
            //Now getting the least recent vuser from the blocking user pool
            Pool_vptr *vptr_pool_head = (Pool_vptr *)sess_pool->busy_head;
            vptr = (VUser *)vptr_pool_head->sav_vptr;
            /*cptr needs to update as cptr is saved in vptr->last_cptr and thus last_cptr will be lost */
            cptr = vptr->last_cptr;
            //cleaning of least recent vuser slot
            nslb_mp_free_slot(sess_pool, (void *)vptr_pool_head);
 
            //Bug 36128: In rampdown blocked users may already be cleaned so state is changed to IDLE
            Schedule *schedule_ptr = (global_settings->schedule_by == SCHEDULE_BY_SCENARIO)?v_port_entry->scenario_schedule:&v_port_entry->group_schedule[vptr->group_num];
            if(schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type == SCHEDULE_PHASE_RAMP_DOWN)
            {
              if(vptr->vuser_state == NS_VUSER_IDLE)
              {
                NSDL3_SCHEDULE(vptr, NULL, "In rampdown phase, vusers are cleaned but left in session pool, hence returning");
                return;
              }
            }
            NSTL1(vptr, cptr, "FCS Releasing: Child = %d, Group = %s, Script = %s, concurrent_sess_num = %u", my_child_index,
                               runprof_table_shr_mem[vptr->group_num].scen_group_name, vptr->sess_ptr->sess_name, concurrent_sess_num);
          }
        // Active count is increamented in reuse user()
        reuse_user(vptr, cptr, now);
      }
    }
    else 
    {
      reuse_user(vptr, cptr, now);
    }
  } else {
      chk_for_cleanup(vptr, cptr, do_cleanup, now);
      // For fix sesson rate scenario we should not start new user
  }
}

#define NS_NVM_CTX     0
#define NS_USER_CTX    1

int ns_end_session(VUser *vptr, int which_ctx)
{

#ifdef NS_DEBUG_ON
int group_mode;
//u_ns_ts_t now;
int do_cleanup = 0;

  NSDL2_API(vptr, NULL, "Method called. vptr = %p, vptr->last_cptr=%p, concurrent_sess_num = %u", vptr, vptr->last_cptr, concurrent_sess_num);
  // TODO: How to handle now and do_cleanup in other modes
  //Bug#2426
  do_cleanup = vptr->flags & NS_DO_CLEANUP;
  //now = vptr->now;

  group_mode = get_group_mode(vptr->group_num);

  NSDL3_SCHEDULE(vptr, vptr->last_cptr, "Method called. group_mode = %d, Vuser stat = %d, do_cleanup = %d, gNumVuserActive = %d, gNumVuserThinking = %d, gNumVuserWaiting = %d, gNumVuserCleanup = %d, gNumVuserBlocked = %d", group_mode, vptr->vuser_state, do_cleanup, gNumVuserActive, gNumVuserThinking, gNumVuserWaiting, gNumVuserCleanup, gNumVuserBlocked); 

#endif

  /*if (sbrk(0) - start_sbrk) printf ("*********************************Session done : Size increated by %d\n", ((unsigned int)sbrk(0)- start_sbrk));*/ 
  // Moved from handle page complete
  if (vptr->is_embd_autofetch) {
    NSDL1_HTTP(NULL, vptr->last_cptr, "is_embd_autofetch is ON, freeing all_embedded_urls");
    free_all_embedded_urls(vptr);
  }


  if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_LEGACY)
  {
    vut_add_task(vptr, VUT_END_SESSION);
  }
  //For mode 1 -> we need to switch to NVM context and return.
  else if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT)
  {
    vut_add_task(vptr, VUT_END_SESSION);

    if(which_ctx == NS_USER_CTX)
      switch_to_nvm_ctx(vptr, "EndSession"); // context/stack is freed in end session
    return 0;
  }
  else if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_SEPARATE_THREAD)
  {
    NSDL1_HTTP(NULL, NULL, "NS_SCRIPT_MODE_SEPARATE_THREAD which_ctx = %d", which_ctx);

    //free_vuser_thread_data(vptr); //Not freeing thread data due to performance reasons

    if(which_ctx == NS_NVM_CTX)
    {
      vut_add_task(vptr, VUT_END_SESSION);
    }
    else
    {
      //Ns_end_session_t tmp_end_session;
      //tmp_end_session.opcode = NS_API_END_SESSION_REQ;     
      Ns_api_req api_req_opcode;
      api_req_opcode.opcode = NS_API_END_SESSION_REQ;
      //vutd_send_msg_to_nvm(VUT_END_SESSION, (char *)(&tmp_end_session), sizeof(Ns_end_session_t));
      vutd_send_msg_to_nvm(VUT_END_SESSION, (char *)(&api_req_opcode), sizeof(Ns_api_req));
    }
    return 0;
  }
  else if(vptr->sess_ptr->script_type == NS_SCRIPT_TYPE_JAVA)
  {
    NSDL1_HTTP(NULL, NULL, "NS_SCRIPT_TYPE_JAVA, adding task of VUT_END_SESSION"); 
    vut_add_task(vptr, VUT_END_SESSION);
    NSTL2(NULL, NULL, "VUT_END_SESSION is added to tasks for nvm %d", my_child_index);
  }
 return 0;
}

// Same as on_session_completion() for script type C and execution mode 1
// Not used by Legacy Script
// Called by C script code in following cases:
//   1. At the end of session in normal execution

int ns_exit_session_ext(VUser *vptr)
{
  NSDL2_API(vptr, NULL, "Method called. vptr = %p", vptr);

  vptr->flags |= NS_DO_CLEANUP;
  vptr->now = get_ms_stamp();

  // Bug - 111105 - Session is going to end so clearing this flag. New session can be started with flag cleared.
  vptr->flags &= ~NS_VPTR_FLAGS_SESSION_EXIT;

  ns_end_session(vptr, NS_USER_CTX);

  return 0;
}

// Called on session completion for Legacy Script in all cases
// Called by C script code in following cases (Internally by NS, not by script)
//   1. After page fails and continue on page error is false (handle_page_complete())
//   2. User is ramped down with Ramp down method set to (
//        - Stop immediately (stop_user_immediately())
//        - Allow current page to complete (handle_page_complete())
//
void on_session_completion(u_ns_ts_t now, VUser *vptr, connection * cptr, int do_cleanup)
{
  NSDL3_SCHEDULE(vptr, NULL, "Method called, cptr = %p, do_cleanup = %d, cptr=%p", cptr, do_cleanup, cptr);
  //Bug#2426
  if(do_cleanup)
    vptr->flags |= NS_DO_CLEANUP;
  else
    vptr->flags &= ~NS_DO_CLEANUP;

  vptr->now = now;
  vptr->last_cptr = cptr; // TODO: This may be already set in close_fd()

  // Bug - 111105 - On Session completion, set flag to end the session.
  vptr->flags |= NS_VPTR_FLAGS_SESSION_COMPLETE;

  // Bug 111105:
  // No change has been for JAVA type script.
  // Switch to User context to continue the execution of Exit script  function before end session.
  if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT)
    switch_to_vuser_ctx(vptr, "SessionCompletion");
  else if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_SEPARATE_THREAD)
    send_msg_nvm_to_vutd(vptr, NS_API_WEB_URL_REP, 0); // Response code is no use, so selected WEB_URL
  else if(vptr->sess_ptr->script_type == NS_SCRIPT_TYPE_JAVA)
    ns_end_session(vptr, NS_NVM_CTX);

}

//This funtion is used to enable DDR settings
void check_set_ddr_enable(VUser *vptr)
{
  int ran_num;
  NSDL2_SCHEDULE(vptr, NULL, "Drill down check: log_records = %d, report_level = %d, run_mode = %d result = %d",
                             log_records, runprof_table_shr_mem[vptr->group_num].gset.report_level, run_mode, 
                             (global_settings->flags & GS_FLAGS_TEST_IS_DEBUG_TEST));

  // Reset DDR enabled and Page dump done for this session
  vptr->flags &= ~NS_VPTR_FLAGS_DDR_ENABLE;
  vptr->flags &= ~NS_VPTR_FLAGS_PAGE_DUMP_DONE;

  if(log_records && (runprof_table_shr_mem[vptr->group_num].gset.report_level >= 2) && (run_mode == NORMAL_RUN))
  {
    //set DDR flag if debug flag is on
    if (global_settings->flags & GS_FLAGS_TEST_IS_DEBUG_TEST) {
      vptr->flags |= NS_VPTR_FLAGS_DDR_ENABLE;
    }
    else
    {
      //generate random number and check if its less than percentage of session for ddr
      ran_num = ns_get_random_number_int(1, 100);
      if(ran_num <= runprof_table_shr_mem[vptr->group_num].gset.ddr_session_pct)
      {
        vptr->flags |= NS_VPTR_FLAGS_DDR_ENABLE;
        NSTL1(NULL, NULL, "Drill down check: ran_num = %d, vptr->flags = %llu, ddr_session_pct = %d", ran_num, vptr->flags,
                           runprof_table_shr_mem[vptr->group_num].gset.ddr_session_pct);
      }
    }
  }
  NSDL2_SCHEDULE(vptr, NULL, "Drill down check: flags = %d, vptr->flags = %llu, pacing_mode = %d, ran_num = %d", 
                              global_settings->flags, vptr->flags,
                              runprof_table_shr_mem[vptr->group_num].pacing_ptr->pacing_mode, ran_num);
}

//return -1 if unable to start session or 0 if started
int on_new_session_start (VUser *vptr, u_ns_ts_t now) 
{
  //TotSvrTableEntry_Shr* svr_entry;

  NSDL3_SCHEDULE(vptr, NULL, "Method called");
  int ret;

  if(vptr->vuser_state == NS_VUSER_PAUSED){
    vptr->operation = VUT_START_SESSION;
    return 0;
  }
  /* ----- Changes for Netdiagnostics - BEGIN ----- */

  /* Enable certain percent of user sessions for sending CavFPInstance header */
  vptr->flags &= ~NS_ND_ENABLE; /* Initialize the bit to 0 */

  //NSDL3_SCHEDULE(vptr, NULL,"runprof_table_shr_mem[vptr->group_num].gset.nd_pct=%hd",runprof_table_shr_mem[vptr->group_num].gset.nd_pct );
  //if(runprof_table_shr_mem[vptr->group_num].gset.nd_pct == 10000.00)
  //dont send header when net_diagnostics_mode is 2
  NSDL3_SCHEDULE(vptr, NULL,"global_settings->net_diagnostics_mode=%d", global_settings->net_diagnostics_mode);
  if(global_settings->net_diagnostics_mode == 1)  
  {
    NSDL3_SCHEDULE(vptr, NULL,"Setting bit to send the CAVNDP header.");
    vptr->flags |= NS_ND_ENABLE;
  }

#if 0
  else if (runprof_table_shr_mem[vptr->group_num].gset.nd_pct > 0.0)
  {
    int rand_num = (1 + (int) (100.0 * (rand() / (RAND_MAX + 1.0))))*100;

    NSDL4_SCHEDULE(vptr, NULL, "rand_num = %d, "
                               "runprof_table_shr_mem[vptr->group_num].gset.nd_pct = %d", 
                               rand_num, 
                               runprof_table_shr_mem[vptr->group_num].gset.nd_pct);

    if(rand_num <= runprof_table_shr_mem[vptr->group_num].gset.nd_pct)
      vptr->flags |= NS_ND_ENABLE;
  }
#endif
  /* ----- Changes for Netdiagnostics - END ----- */

  NSDL2_SCHEDULE(vptr, NULL, "Retain param value = [%d]",  runprof_table_shr_mem[vptr->group_num].pacing_ptr->retain_param_value);
  // This variable is to store the status of old session, this will be used to retain the value of declare array   
  int local_sess_status = vptr->sess_status;
  NSDL2_SCHEDULE(vptr, NULL, "local_sess_status = [%d], vptr - %p", local_sess_status, vptr);

  if ((ret = is_new_session_blocked(vptr, now)))
  {
    NSDL4_SCHEDULE(vptr, NULL, "User cannot do any more sessions.");
    // Bug - 77969 - Session got stuck
    //return -1;
    return ret;
  }

  vptr->page_status = 0;
  vptr->sess_status = 0;
  vptr->sess_think_duration = 0;

  // Added this by Neeraj on Jun 7, 2011
  // In case of C type script, we are not setting cur_page before ns_web_url()
  // In many place, we are using cur_page (e.g DL, DT), so it is core dumping
  // We need to check if value of cur_page need to be accurate or not before ns_web_url()
  if(vptr->sess_ptr->num_pages) // To handle script with no pages
    vptr->cur_page = vptr->sess_ptr->first_page;     
  else
    vptr->cur_page = NULL; // Set to NULL as it is used in many debug and trace logs


  /* Fill default values in uvtable for variable as old values have been freed after calling clear_var_table in following cases 
     i) using new user 
     ii) Continuing with same user but do not want to retain value used in prevous session .
   NOTE : Retaining previously used values if retain_param_value = 1 and previous session succeds */

  if(!vptr->sess_inst){
    NSDL2_SCHEDULE(vptr, NULL, "Going to Add default value to  uvtable first session");
    if (nsl_var_table_shr_mem)
      init_nsl_array_var_uvtable(vptr);
  }
  else if ((nsl_var_table_shr_mem) && ((!runprof_table_shr_mem[vptr->group_num].pacing_ptr->retain_param_value) || 
          ((runprof_table_shr_mem[vptr->group_num].pacing_ptr->retain_param_value == 2) && local_sess_status != NS_REQUEST_OK)) ) {
    NSDL2_SCHEDULE(vptr, NULL, "Going to Add default value to  uvtable");
    init_nsl_array_var_uvtable(vptr);
  }

  vptr->started_at = now;

  vptr->tx_instance = 0; // Do not delete this line as we need to reset tx_instance to 0
  //tx_close(vptr, 1); // We dont need tx_close (), since we are calling from on session completion() via tx logging on session completion()
  vptr->url_num = 0;
  vptr->bytes = 0;

  vptr->ka_timeout = -1;//setting default.

  // For Jmeter we dont need to update fetches counter as incresting from jmeter_sample in ns_jmeter.c
  if(vptr->sess_ptr->jmeter_sess_name == NULL)
  {
    average_time->ss_fetches_started++;

    if(SHOW_GRP_DATA)
    {
      avgtime *lol_average_time;
      lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));
      lol_average_time->ss_fetches_started++;
    }
  }

  TLS_SET_VPTR(vptr);
  cur_time = now;

  /* Enable the user for Vuser_trace at the start of the session.
     If any user is not active then enable the fisrt user,
     If active user is remp down then active another user  */
     
  if(my_port_index == 0 && runprof_table_shr_mem[vptr->group_num].gset.vuser_trace_enabled){
    NSDL3_USER_TRACE(vptr, NULL, "Going to check user for vuser tracing");
    ut_vuser_check_and_enable_tracing(vptr);
  }

  /* Page Dump
   * In case of trace level greater than 1, 
   * here we set vptr->flag if page dump enable or disable
   * initialize vuser link list*/ 
  if(runprof_table_shr_mem[vptr->group_num].gset.trace_level > TRACE_URL_DETAIL)
  {
    //Start tracing only for failure cases.
    //for all cases it will not save any data.
    //It will dump all data at run time
    int ret = 0;
    if(runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_ALL_SESS || runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_PAGE_ON_HEADER_BASED){
      /*Added check for trace-on-failure 0*/
      //This is for ALL INTERCATIONS
      need_to_dump_session(vptr, runprof_table_shr_mem[vptr->group_num].gset.trace_limit_mode, vptr->group_num);
    }
    else {
      ret = need_to_enable_page_dump(vptr, runprof_table_shr_mem[vptr->group_num].gset.trace_limit_mode, vptr->group_num);
    }

    NSDL3_USER_TRACE(vptr, NULL, "Manish: vptr->flags = %0x\n", vptr->flags);
    //This id for only failure cases and whole session on failure
    //All interaction is handled seprately 
    if(runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail > TRACE_ALL_SESS && 
       runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail != TRACE_PAGE_ON_HEADER_BASED) 
    {
      /*First check if need to save session*/
      if(ret)
      {
        init_page_dump_data_and_add_in_list(vptr);
      }
    } 
  }
  
  /*For pade dump we will use vuser tarce functions*/
  if(NS_IF_TRACING_ENABLE_FOR_USER ||
     NS_IF_PAGE_DUMP_ENABLE_WITH_TRACE_ON_FAIL) {
    NSDL2_USER_TRACE(vptr, NULL, "User tracing enable for %p vptr", vptr);
    ut_add_start_session_node(vptr);
  }

  // Moved above
  // TODO: make sure it is OK to move
  fill_ka_time_out(vptr, now);

  // Initialize the page instance with -1 as it is increamented by on_page_start()
  vptr->page_instance = -1;

  //Removed NULL, as nvmId is coming NA.
  //NS_DT1(NULL, NULL, DM_L1, MM_SCHEDULE, "Starting execution of script '%s'", vptr->sess_ptr->sess_name);
  if(g_debug_script)
    NS_DT1(vptr, NULL, DM_L1, MM_SCHEDULE, "Starting Session '%u'",  vptr->sess_inst + 1);

  NS_DT1(vptr, NULL, DM_L1, MM_SCHEDULE, "Starting execution of script '%s'", vptr->sess_ptr->sess_name);

  if(runprof_table_shr_mem[vptr->group_num].gset.script_mode != NS_SCRIPT_MODE_LEGACY)
  {
    if(vptr->cur_page != NULL)    // we are doing this to handle the case if we do not have any url in script, but think time api is used to 
    {                             //start timer. one should not use override keyword in this case 
      calc_page_think_time(vptr); //We need to set page think time to handle a case where ns_page_think_time API is called before first ns_web_url in the flow file
    }   
    // If scenario is replay access logs, then call method to set next page to be executed in a NS variable which is used in the Runlogic of the script
    if(global_settings->replay_mode)    
      set_next_replay_page(vptr);
  }

  if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_rbu)
  {
    if((ns_rbu_on_session_start(vptr)) == -1)
    {  
      rbu_fill_page_status(vptr, NULL);
      // This needs to be done only for RBU
      // so it has been removed from ns_vuser.c where new user is starting
      vptr->sess_status = NS_REQUEST_ERRMISC;
      vut_add_task(vptr, VUT_END_SESSION);
      return NS_SESSION_FAILURE;
    }
  }

  check_set_ddr_enable(vptr);

  /* Manish: In case of RBU we are using ns_eval_string so that we need to call ns_rbu_on_session_start() after setting cur_vptr etc.. 
             and before running runlogic ().*/
  // For mode 1 -> call create_vuser_ctx() and return.

  NSDL2_SCHEDULE(vptr, NULL, "enable_rbu = %d", runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_rbu);
  if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT)
  {
    create_vuser_ctx(vptr);  
    return 0;
  }
  else if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_SEPARATE_THREAD)
  {
    NSDL2_SCHEDULE(vptr, NULL, "Going to get free thread vptr %p", vptr);
    Msg_com_con *tmp_mcctptr;
    tmp_mcctptr = get_thread();
    vptr->mcctptr = tmp_mcctptr;
    if(!tmp_mcctptr)
    {
      NSDL4_SCHEDULE(vptr, NULL, "Error: All threads are busy So ending this session vptr %p", vptr);
      NS_EL_2_ATTR(EID_VUSER_THREAD, vptr->user_index,
                                vptr->sess_inst,
                                EVENT_CORE, EVENT_CRITICAL,
                                (char *)__FILE__,(char *) __FUNCTION__,
                                "Error: All threads are busy So ending this session.");
      vptr->sess_status = NS_REQUEST_ERRMISC;
      vut_add_task(vptr, VUT_END_SESSION);
      return 0;
    } 
    NSDL4_SCHEDULE(vptr, NULL, "tmp_mcctptr = %p, run_thread = %d\n", tmp_mcctptr, tmp_mcctptr->run_thread);
    allocate_vuser_thread_data(vptr);

    tmp_mcctptr->vptr = vptr;
    tmp_mcctptr->vptr->mcctptr = tmp_mcctptr;

    if(sem_post(&(tmp_mcctptr->run_thread)) == -1)
    {
      perror("sem_post");
      NS_EXIT(-1, "Failed to create semaphore");
    }
    NSDL4_SCHEDULE(vptr, NULL, "after sem_post tmp_mcctptr = %p, run_thread = %d\n", tmp_mcctptr, tmp_mcctptr->run_thread);
    //NSTL1_OUT(NULL, NULL, "tmp_mcctptr = %p, tmp_mcctptr->run_thread = %d\n", tmp_mcctptr, tmp_mcctptr->run_thread);
    //vutd_create_thread(vptr);
    //NSTL1_OUT(NULL, NULL, "\n%s|%d| tmp_mcctptr= %p, tmp_mcctptr->vptr = %p\n", __FILE__, __LINE__, tmp_mcctptr, tmp_mcctptr->vptr);
    //NSTL1_OUT(NULL, NULL, "\n%s|%d| after get_thread total_busy_thread = %d\n", __FILE__, __LINE__, total_busy_thread);
    return 0;
  }
  else if(vptr->sess_ptr->script_type == NS_SCRIPT_TYPE_JAVA)
  {
    NSDL2_SCHEDULE(vptr, NULL, "Going to get free njvm thread for java type script. vptr %p", vptr);
    Msg_com_con *tmp_mcctptr;
    tmp_mcctptr = njvm_get_thread();
    vptr->mcctptr = tmp_mcctptr;
     
    if(!tmp_mcctptr)
    {
      NSDL4_SCHEDULE(vptr, NULL, "Error: All njvm threads are busy So ending this session vptr %p", vptr);
      NS_EL_2_ATTR(EID_VUSER_THREAD, vptr->user_index,
                                vptr->sess_inst,
                                EVENT_CORE, EVENT_CRITICAL,
                                (char *)__FILE__,(char *) __FUNCTION__,
                                "Error: All njvm threads are busy So ending this session.");
      vptr->sess_status = NS_REQUEST_ERRMISC;
      vut_add_task(vptr, VUT_END_SESSION);
      return 0;
    } 

    tmp_mcctptr->vptr = vptr;
    tmp_mcctptr->vptr->mcctptr = tmp_mcctptr;


    // Send start mesg to njvm to start the session 
    NSDL2_SCHEDULE(vptr, NULL, "sending start user message to njvm %p", vptr);
    send_msg_to_njvm(tmp_mcctptr, NS_NJVM_START_USER_REQ, 0); 
    NSTL2(NULL, NULL, "Send NS_NJVM_START_USER_REQ to njvm for nvm %d. tmp_mcctptr = %s", my_child_index, msg_com_con_to_str(tmp_mcctptr));
    return 0;
  }
  else
  {
    NSTL1(NULL, NULL, "Error: Legacy script is removed.");
    return -1;
  }

  return 0;
}

/*------------------------------------------------------------------------------------------------------------- 
 * Purpose   : To retry session on Error Code.
 *    
 * Input     : VUser     : to set and retrieve data  
 *             errorCode : error code of session on which retry will be done. 
 *                         Right now, supported with NS_SESSION_ABORT
 *
 * Called from ns_rbu.c 
 *------------------------------------------------------------------------------------------------------------*/
void nsi_retry_session(VUser *vptr, int errorCode)
{
  char errorMsg[64];

  NSDL2_SCHEDULE(vptr, NULL, "Method call, errorCode = %d", errorCode);

  switch(errorCode)
  {
    case NS_SESSION_ABORT:
      vptr->retry_count_on_abort++;
      vptr->page_status = NS_REQUEST_ABORT;
      vptr->sess_status = NS_SESSION_ABORT;
      strcpy(errorMsg,"sessionAborted");
      NS_DT3(vptr, NULL, DM_L1, MM_VARS, "Retrying Session as current session failed due to timeout and G_SESSION_RETRY is enabled");
      break;
    default:
      NSTL1(vptr, NULL, "Unknown Error Code for Session Retry");
      vptr->page_status = NS_REQUEST_ERRMISC;
      vptr->sess_status = NS_REQUEST_ERRMISC;
      strcpy(errorMsg,"miscError");
  }

  if(vptr->sess_status != NS_REQUEST_ERRMISC)
  {
    vptr->sess_inst++;

    //For Indefinite mode
    if(v_port_entry->num_fetches > 0)
      v_port_entry->num_fetches++;

    //Increase number of fetches for this group as this a session for this group has been failed
    if((global_settings->schedule_by == SCHEDULE_BY_GROUP) &&
        (per_proc_per_grp_fetches[(my_port_index * total_runprof_entries) + vptr->group_num] > 0))
        per_proc_per_grp_fetches[(my_port_index * total_runprof_entries) + vptr->group_num]++;  
  }

  vptr->last_cptr->req_ok = NS_REQUEST_ERRMISC;
  vut_add_task(vptr, VUT_END_SESSION);
  return;
}
