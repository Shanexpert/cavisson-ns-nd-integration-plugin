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
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "poi.h"
#include "ns_page.h"
#include "ns_page_think_time.h"
#include "ns_log.h"
#include "ns_vuser_tasks.h"
#include "ns_session.h"
#include "ns_trans.h"
#include "ns_schedule_phases_parse.h"
#include "ns_auto_fetch_embd.h"
#include "ns_vuser_ctx.h"
#include "ns_vuser.h"
#include "ns_event_log.h"
#include "ns_vuser_trace.h"
#include "divide_users.h"
#include "divide_values.h"
#include "child_init.h"
#include "ns_page_think_time_parse.h"
#include "ns_child_thread_util.h"
#include "ns_vuser_thread.h"
#include "ns_connection_pool.h"
#include "ns_nvm_njvm_msg_com.h"
#include "ns_script_parse.h"
#include "ns_group_data.h"
#include "ns_page_based_stats.h"
#include "ns_exit.h"
// If user is ramping down and mode is allow current page to complete 
// then do not go to next page. We need to stop session
#define CHK_VUSER_RAMP_DOWN_STOP_SESSION() \
  if (vptr->flags & NS_VUSER_RAMPING_DOWN) \
  { \
    NSDL3_SCHEDULE(vptr, NULL, "User is marked as ramp down."); \
    if (runprof_table_shr_mem[vptr->group_num].gset.rampdown_method.mode == RDM_MODE_ALLOW_CURRENT_PAGE_COMPLETE) { \
      NSDL3_SCHEDULE(vptr, NULL, "Ramp down method mode is allow current page to complete. So stop sesssion now"); \
      on_session_completion(now, vptr, vptr->last_cptr, 1); \
      return; \
    } \
  }

inline void fill_custom_page_think_time_fun_ptr()
{
  int i, j;
  int num_pages;
  void *handle;
  ThinkProfTableEntry_Shr *ptr;
  char *error = NULL;

  NSDL2_PARSING(NULL, NULL, "total_runprof_entries = [%d]", total_runprof_entries);
  // As page think time is page based, we need to apply handle (which we have saved in session table in ns_script_parse.c) on each page.
  for(i=0; i< total_runprof_entries; i++) {
    // Fetch handle for current session.
    handle = gSessionTable[runprof_table_shr_mem[i].sess_ptr->sess_id].handle;
    // Fetch number of pages for current session.
    num_pages =  gSessionTable[runprof_table_shr_mem[i].sess_ptr->sess_id].num_pages;
    NSDL2_PARSING(NULL, NULL, "handle = [%p], num_pages = [%d]", handle, num_pages);
   
    for(j=0; j < num_pages; j++) {
      ptr = (ThinkProfTableEntry_Shr *)(runprof_table_shr_mem[i].page_think_table[j]);
      if(ptr && (ptr->mode == PAGE_THINK_TIME_MODE_CUSTOM) && ptr->custom_page_think_time_callback) {
        ptr->custom_page_think_func_ptr = dlsym(handle, ptr->custom_page_think_time_callback);
        if ((error = dlerror()) != NULL)  {
          NS_EXIT(-1, CAV_ERR_1031044, ptr->custom_page_think_time_callback, error);
        }
        NSDL2_PARSING(NULL, NULL, "custom_page_think_time_callback = [%s], custom_page_think_func_ptr = [%p]",
                                   ptr->custom_page_think_time_callback, ptr->custom_page_think_func_ptr);
      }
    }
  } 
} 

// Think timer callback
// Allow pg think (Used in nsi_page_think_time in page_think is there)
static void page_think_timer( ClientData client_data, u_ns_ts_t now )
{
  VUser *vptr;

  vptr = client_data.p; // p point to vptr

  TLS_SET_VPTR(vptr);
  NSDL2_HTTP(vptr, NULL, "Method Called");

  if(((vptr->flags & NS_USER_PTT_AS_SLEEP) != NS_USER_PTT_AS_SLEEP) && (vptr->vuser_state != NS_VUSER_PAUSED))
    VUSER_THINKING_TO_ACTIVE(vptr); //changing the state of vuser thinking to active

  // This is to handle case where think timer is reset when user is ramped down
  CHK_VUSER_RAMP_DOWN_STOP_SESSION();
  
  if(SHOW_GRP_DATA)
    set_grp_based_counter_for_page_think_time(vptr, (now - client_data.timer_started_at));

  if(global_settings->page_based_stat == PAGE_BASED_STAT_ENABLED)
    set_page_based_counter_for_page_think_time(vptr, (now - client_data.timer_started_at));

  if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT)
  {
    // Wakeup the ns_page_think_time() API
    NSDL3_SCHEDULE(vptr, NULL, "Page Think is over. Changing to vuser context");
    switch_to_vuser_ctx(vptr, "PageThinkTimeOver"); 
    return;
  }
  else if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_SEPARATE_THREAD)
  {
    send_msg_nvm_to_vutd(vptr, NS_API_PAGE_TT_REP, 0);
    return;
  }
  /*  JAVA SCRIPT HANDLING */
  else if(vptr->sess_ptr->script_type == NS_SCRIPT_TYPE_JAVA)
  {
    send_msg_to_njvm(vptr->mcctptr, NS_NJVM_API_PAGE_THINK_TIME_REP, 0); 
    return;
  }
}

static inline int start_page_think_timer(VUser *vptr, u_ns_ts_t now)
{
  ClientData cd;

  NSDL2_HTTP(vptr, NULL, "Method Called. vptr = %p, think time = %d", vptr, vptr->pg_think_time);

#ifdef NS_DEBUG_ON
  ThinkProfTableEntry_Shr *think_ptr; // added for debug file
  if(vptr->cur_page != NULL)
    think_ptr = (ThinkProfTableEntry_Shr *)runprof_table_shr_mem[vptr->group_num].page_think_table[vptr->cur_page->page_number];
  else
  {
    // If there is no page in the script, then cur_page will be NULL
    think_ptr = NULL;
    NSDL2_HTTP(vptr, NULL, "cur_page is NULL. Script may be without any page");
  }
#endif

  NSDL2_HTTP(vptr, NULL, "vptr->flags = %hd, vptr->vuser_state = %d", vptr->flags, vptr->vuser_state);
  if(((vptr->flags & NS_USER_PTT_AS_SLEEP) != NS_USER_PTT_AS_SLEEP) && (vptr->vuser_state != NS_VUSER_PAUSED))
    VUSER_ACTIVE_TO_THINKING(vptr); //changing the state of vuser from to thinking 

  if(NS_IF_TRACING_ENABLE_FOR_USER){
    NSDL2_USER_TRACE(vptr, NULL, "User tracing enable for %p vptr", vptr);
    ut_update_page_think_time(vptr);
  }
  NSDL1_SCHEDULE(vptr, NULL, "PageThinkTime: Page Think Time = %d ms, NS_USER_PGT_AS_SLEEP flag = %d", vptr->pg_think_time, (vptr->flags & NS_USER_PTT_AS_SLEEP));

#ifdef NS_DEBUG_ON
  if(think_ptr != NULL)
    log_page_time_for_debug(vptr, think_ptr, &think_ptr->override_think_time, vptr->pg_think_time, now);
#endif
 
  vptr->timer_ptr->actual_timeout = vptr->pg_think_time; 
 
  if((vptr->flags & NS_USER_PTT_AS_SLEEP) != NS_USER_PTT_AS_SLEEP)
  {
    NSDL2_HTTP(vptr, NULL, "Adding tx");
    add_tx_tot_think_time_node(vptr);  //defined in ns_trans.c
    vptr->sess_think_duration += vptr->timer_ptr->actual_timeout;
  }

  cd.timer_started_at = now;
  cd.p = vptr;
  (void) dis_timer_think_add( AB_TIMEOUT_THINK, vptr->timer_ptr, now, page_think_timer, cd, 0);
  return 0;
}

static int is_page_think_allowed(VUser *vptr)
{
int allow_pg_think = 1;

  NSDL2_HTTP(vptr, NULL, "Method called.");

  if (vptr->flags & NS_VUSER_RAMPING_DOWN)
  {
    if (runprof_table_shr_mem[vptr->group_num].gset.rampdown_method.mode == RDM_MODE_ALLOW_CURRENT_SESSION_COMPLETE)
    {
      if (runprof_table_shr_mem[vptr->group_num].gset.rampdown_method.option >= RDM_OPTION_DISABLE_FUTURE_THINK_TIMES)
        allow_pg_think = 0;
    }
  }
  NSDL2_HTTP(vptr, NULL, "Returning allow_pg_think=%d", allow_pg_think);
  return(allow_pg_think);
}

//This method is called from vut_execute while executing task NS_PAGE_THINK_TIME
void nsi_page_think_time(VUser *vptr, u_ns_ts_t now)
{

  NSDL2_HTTP(vptr, NULL, "Method called, vptr = %p", vptr);

  // This is to handle case where user task for page think time was added to tasks list
  // At that point user was in active state. So ramp down method did not do any thing. Just
  // marked user as ramp down. So we need to stop the session in this case
  CHK_VUSER_RAMP_DOWN_STOP_SESSION();

  if(is_page_think_allowed(vptr) == 0)
  {
    if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT)
    {
      // Wakeup the ns_page_think_time() API
      NSDL2_SCHEDULE(vptr, NULL, "Page Think is not allowed. Changing to vuser context");
      switch_to_vuser_ctx(vptr, "PageThinkTimeNotAllowed");
    }
    else if(vptr->sess_ptr->script_type == NS_SCRIPT_TYPE_JAVA)
    {
      NSDL2_SCHEDULE(vptr, NULL, "Page Think is not allowed. Sending error code to NJVM");
      send_msg_to_njvm(vptr->mcctptr, NS_NJVM_API_PAGE_THINK_TIME_REP, -1); 
    }
    else if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_SEPARATE_THREAD)
    {
      NSDL2_SCHEDULE(vptr, NULL, "Page Think is not allowed. Sending error code to Thread");
      send_msg_nvm_to_vutd(vptr, NS_API_PAGE_TT_REP, -1);
    }
    return;
  }

  NSDL2_HTTP(vptr, NULL, "starting Page Think Timer");
  start_page_think_timer(vptr, now);
}

// API called from script - Only for C script
// Inputs:
//   page_think_time : Think time in milli-seconds
int ns_page_think_time_ext(VUser *vptr, int page_think_time)
{

  NSDL2_HTTP(vptr, NULL, "Method called. vptr = %p, cptr = %p, page_think_time = %d milli-secs", vptr, vptr->last_cptr, page_think_time);
  // cptr is still connected, so add in the reuse list as we will use it later
  // if (done != 1)   // use vptr->last_cptr

  //Now we are using pagethink time as sleep in between page execution in rbu so we need to check both the conditions.
  //In case of rbu, need not to add in reuse list as we are not creating connection.
  NSDL2_HTTP(vptr, NULL, "enable_rbu = %d, vptr->flags = %hd, vptr->vuser_state = %d", 
                          runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_rbu, vptr->flags, vptr->vuser_state);
  /*bug 54315: updated code to avoid using url_num to access request_type*/
  if(vptr->last_cptr  && (!runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_rbu) && !(vptr->flags & NS_JNVM_JRMI_RESP) && 
     ((vptr->last_cptr->request_type != WS_REQUEST) && (vptr->last_cptr->request_type != WSS_REQUEST)) &&
     ((vptr->last_cptr->request_type != XMPP_REQUEST) && (vptr->last_cptr->request_type != XMPPS_REQUEST)) &&
     !(global_settings->protocol_enabled & SOCKJS_PROTOCOL_ENABLED)) 
  {
    NSDL2_HTTP(vptr, NULL, "Adding last_cptr=%p vptr->last_cptr->list_member=%d to resue list", vptr->last_cptr, vptr->last_cptr->list_member);
    if(!(vptr->last_cptr->list_member & (NS_ON_GLB_REUSE_LIST | NS_ON_SVR_REUSE_LIST)))
      add_to_reuse_list(vptr->last_cptr);
    vptr->last_cptr = NULL; // TODO
  }

  /* Free if using auto fetch embedded */
  /* This should be done here after add_to_reuse_list() because add_to_reuse_list
  * Uses index.svr_ptr which we will be free'ing */
  if (vptr->is_embd_autofetch) {
    NSDL1_HTTP(NULL, vptr->last_cptr, "is_embd_autofetch is ON, freeing all_embedded_urls");
    free_all_embedded_urls(vptr);
  }

  // In Legacy Mode - Add task and return
  // In legacy, this is called to set think time from keyword
  if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_LEGACY)
  {
    if(page_think_time <= 0)
    {
      NSDL2_HTTP(vptr, NULL, "Returning as page_think_time is 0 (page_think_time = %d)", page_think_time);
      return 0;
    }
    vptr->pg_think_time = page_think_time;
    vut_add_task(vptr, VUT_PAGE_THINK_TIME);
  }
  // In User Context Mode, Add task, switch to nvm contxt
  //   wait for api to be complete
  else if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT)
  {
    // This method will set think time according to override keyword
    int ret = ns_set_pg_think_time_ext(vptr, page_think_time);
    NSDL2_HTTP(vptr, NULL, "Applying page think time = %d, for VUser = %p", vptr->pg_think_time, vptr); 
    if((ret < 0) || (vptr->pg_think_time <= 0))
    {
      //Updating Counters if page think = 0
      if(global_settings->page_based_stat == PAGE_BASED_STAT_ENABLED){
        NSDL2_GDF(vptr, NULL, "AT NS_SCRIPT_MODE_USER_CONTEXT mode, pg_think_time = %d", vptr->pg_think_time);
        set_page_based_counter_for_page_think_time(vptr, 0);
      }
 
      NSDL2_HTTP(vptr, NULL, "Returning as page_think_time is 0 (page_think_time = %d)", page_think_time);
      return 0;
    }
    vut_add_task(vptr, VUT_PAGE_THINK_TIME);
    NSDL2_HTTP(vptr, NULL, "Waiting for think timer to be over");
    switch_to_nvm_ctx(vptr, "PageThinkTimeStart");
    NSDL2_HTTP(vptr, NULL, "Page think time has been overed");
    return 0;
  }
  // In Separate Thread Mode, Send message to NVM and wait for reply
  else if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_SEPARATE_THREAD)
  {
    // TBD
    /*Ns_page_think_time_req page_tt;
    page_tt.opcode = NS_API_PAGE_TT_REQ;
    page_tt.page_think_time = page_think_time;
    vutd_send_msg_to_nvm(VUT_PAGE_THINK_TIME, (char *) (&page_tt), sizeof(Ns_page_think_time_req));
    NSDL2_API(vptr, NULL, "Sending message to NVM and waiting for reply");*/
    int ret = ns_set_pg_think_time_ext(vptr, page_think_time);
    if((ret < 0) || (vptr->pg_think_time <= 0))
     {
       //Updating Counters if page think = 0
       if(global_settings->page_based_stat == PAGE_BASED_STAT_ENABLED){
         NSDL2_GDF(vptr, NULL, "AT NS_SCRIPT_MODE_SEPARATE_THREAD mode, pg_think_time = %d", vptr->pg_think_time);
         set_page_based_counter_for_page_think_time(vptr, 0);
       }

       //fprintf(stdout,"Returning as page_think_time is 0 (page_think_time = %d)", page_think_time);
       NSDL2_HTTP(vptr, NULL, "Returning as page_think_time is 0 (page_think_time = %d)", page_think_time);
       send_msg_nvm_to_vutd(vptr, NS_API_PAGE_TT_REP, 0);
       return 0;
     }
     vut_add_task(vptr, VUT_PAGE_THINK_TIME);
    return 0;
  }
  /* Java Script Case (same as seperate thread mode)*/
  else if(vptr->sess_ptr->script_type == NS_SCRIPT_TYPE_JAVA) {
    int ret = ns_set_pg_think_time_ext(vptr, page_think_time);
    
    if((ret < 0) || (vptr->pg_think_time <= 0))
    {
      //Updating Counters if page think = 0
      if(global_settings->page_based_stat == PAGE_BASED_STAT_ENABLED){
        NSDL2_GDF(vptr, NULL, "AT NS_SCRIPT_TYPE_JAVA mode, pg_think_time = %d", vptr->pg_think_time);
        set_page_based_counter_for_page_think_time(vptr, 0);
      }
   
      NSDL2_HTTP(vptr, NULL, "Returning as page_think_time is 0 (page_think_time = %d)", page_think_time);
      send_msg_to_njvm(vptr->mcctptr, NS_NJVM_API_PAGE_THINK_TIME_REP, 0); 
      return 0;
    }
    vut_add_task(vptr, VUT_PAGE_THINK_TIME);
    return 0;
  }
  // Here we come for lagacy mode only
  NSDL2_HTTP(vptr, NULL, "Exiting Method.");
  return 0;
}

// This method is called from C API which sets page think time
// This C API is only for legacy script
// But this method is also used in C Type script internally
int ns_set_pg_think_time_ext(VUser *vptr, int pg_think)
{
  NSDL2_HTTP(vptr, NULL, "Method called. vptr = %p, pg_think = %d ms", vptr, pg_think);

  ThinkProfTableEntry_Shr *think_ptr;
  if(vptr == NULL)
  {
    fprintf(stderr, "ns_set_pg_think_time() called with NULL cur_vptr\n");
    return -1;
  }

  if(vptr->cur_page == NULL)
  {
    NSDL2_HTTP(vptr, NULL, "ns_set_pg_think_time() called with vptr->cur_page = NULL");
    vptr->pg_think_time = pg_think;
    return 0;
  }
  think_ptr = (ThinkProfTableEntry_Shr *)runprof_table_shr_mem[vptr->group_num].page_think_table[vptr->cur_page->page_number];
  /* Mode OVERRIDE_REC_THINK_TIME */
  if (think_ptr->override_think_time.mode == OVERRIDE_REC_THINK_TIME) {

    // In case of legacy, we already called calc page think time so no need to call again
     
    // Bug - 81658 => calc_page_think_time was not done in thread mode, so the issue was occurring. It has to be done for all cases now
    /*if((runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT) || (runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_SEPARATE_THREAD)) */
      calc_page_think_time(vptr);

    NSDL2_HTTP(vptr, NULL, "PageThinkTime override: grp_id = %d, mode = %d, think_time = %d ms", 
                            vptr->group_num, think_ptr->override_think_time.mode, 
                            vptr->pg_think_time);
    //return -1;
    return 0;
  }
     
  if (pg_think > 0)
      vptr->pg_think_time = pg_think;
  else
      vptr->pg_think_time = 0;

  /* Mode MULTIPLY_REC_THINK_TIME */
  NSDL2_HTTP(vptr, NULL, "think_ptr->override_think_time.mode = %d", think_ptr->override_think_time.mode);
  if (think_ptr->override_think_time.mode == MULTIPLY_REC_THINK_TIME) {
    NSDL2_HTTP(vptr, NULL, "pg_think_time = %d, multiplier = %f\n", vptr->pg_think_time, 
               think_ptr->override_think_time.multiplier);
    vptr->pg_think_time = (double) vptr->pg_think_time * think_ptr->override_think_time.multiplier;
  }
  
  /* Mode USE_RANDOM_PCT_REC_THINK_TIME */
  if (think_ptr->override_think_time.mode == USE_RANDOM_PCT_REC_THINK_TIME) {
    float a, b;
    int j;

    a = think_ptr->override_think_time.min;
    b = think_ptr->override_think_time.max;
    j = a + (int) ((b - (a - 1)) * (rand() / (RAND_MAX + b)));

    vptr->pg_think_time = (j / 100.0) * vptr->pg_think_time;
  }

  NSDL2_HTTP(vptr, NULL, "grp_id = %d, mode = %d, think_time = %d\n", vptr->group_num, 
             think_ptr->override_think_time.mode, 
             vptr->pg_think_time);

  return 0;
}
// Calculate and set page think time
// API can overide from pre or check page
inline void calc_page_think_time(VUser *vptr)
{
  NSDL2_HTTP(vptr, NULL, "Method called, vptr = %p", vptr);
 
  ThinkProfTableEntry_Shr *think_ptr;

  // If there is no page in the script, then cur_page will be NULL
  if(vptr->cur_page == NULL)
  {
    // In this case, page think time will be what is passed in the API in case of C Type
    NSDL2_HTTP(vptr, NULL, "cur_page is NULL. Script may be without any page");
    // return; // Do not to use return as it will not be inline
  }
  else
  {

  think_ptr = (ThinkProfTableEntry_Shr *)runprof_table_shr_mem[vptr->group_num].page_think_table[vptr->cur_page->page_number];

  NSDL4_SCHEDULE(vptr, NULL, "think ptr for grp = %d, page number = %d, mode = %d,      \
          avg_time = %d, var_time = %d, rand_gen_idx = %d, a = %u, b = %u",
               vptr->group_num, vptr->cur_page->page_number, think_ptr->mode, think_ptr->avg_time,
               think_ptr->median_time, think_ptr->var_time, think_ptr->a, think_ptr->b);

  if (think_ptr->mode)
  {
    switch (think_ptr->mode)
    {
      case PAGE_THINK_TIME_MODE_INTERNET_RANDOM:
        NSDL3_TESTCASE(vptr, NULL, "Passing a and b values for Page_Think_Time Mode = 1, a = %f, b = %f", think_ptr->a, think_ptr->b);
        vptr->pg_think_time = gsl_ran_weibull(weib_rangen, think_ptr->a, think_ptr->b);
        break;
      case PAGE_THINK_TIME_MODE_CONSTANT:
        vptr->pg_think_time = think_ptr->avg_time;
        break;
      case PAGE_THINK_TIME_MODE_UNIFORM_RANDOM:
      {
        int range = think_ptr->b - think_ptr->a;
        vptr->pg_think_time = think_ptr->a + ns_get_random_max(gen_handle, range);
        break;
      }
      case PAGE_THINK_TIME_MODE_CUSTOM:
      {
        if(think_ptr->custom_page_think_func_ptr) {
          double val = think_ptr->custom_page_think_func_ptr();
          if(val < 0)
          {
            NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_MAJOR,
                                __FILE__, (char*)__FUNCTION__,
                                "Got negative value of 'custom_page_think_time_func_ptr', setting think time = 0");

            NSDL2_SCHEDULE(vptr, NULL, "Got negative value of 'custom_page_think_time_func_ptr', setting think time = 0");
            val = 0;
          }
          vptr->pg_think_time = val * 1000;
          NSDL3_SCHEDULE(vptr, NULL, "In case PAGE_THINK_TIME_MODE_CUSTOM: vptr->pg_think_time = [%d]", vptr->pg_think_time);
        }else 
         {
           NSDL2_SCHEDULE(vptr, NULL, "Error: Unable to find page think time Callback method [%s], thus exiting.\n",
                                       think_ptr->custom_page_think_time_callback);
           NS_EXIT (-1, "Error: Unable to find Custom page think time Callback method [%s], thus exiting.\n",
                            think_ptr->custom_page_think_time_callback);
         }
        break;
     }
      default:
        NSDL2_TESTCASE(vptr, NULL, "Invalid think mode hence assigning default value");
        fprintf(stderr, "Warning: Invalid think mode hence assigning default value");
        vptr->pg_think_time = 0;
     }
   }
   else
   {
      vptr->pg_think_time = 0;
   }
  }
}
