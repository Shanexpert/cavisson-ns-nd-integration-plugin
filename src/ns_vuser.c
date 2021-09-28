/*************************************************************************
 * Name            : ns_vuser.c 
 * Purpose         : This file contains all the vusers related function of netstorm
 * Initial Version : Monday, July 13 2009
 * Modification    : -
 *************************************************************************/

#include <regex.h>

#include "url.h"
#include "ns_tag_vars.h"
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
#include "logging.h"
#include "tmr.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_cavmain_child_thread.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "ns_log.h"
#include "ns_alloc.h"
#include "divide_users.h"
#include "divide_values.h"
#include "child_init.h"
#include "ns_cookie.h"
#include "ns_wan_env.h"
#include "ns_sock_list.h"
#include "src_ip.h"
#include "poi.h"
#include "ns_schedule_phases_parse.h"
#include "ns_schedule_ramp_up_fcu.h"
#include "ns_schedule_ramp_up_fsr.h"
#include "ns_session.h"
#include "ns_page.h"
#include "runlogic.h"
#include "ns_parallel_fetch.h"
#include "ns_replay_access_logs.h"
#include "unique_vals.h"
#include "ns_schedule_ramp_down_fcu.h"
#include "ns_auto_cookie.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "ns_sock_com.h"
#include "ns_vuser.h"
#include "ns_smtp.h"
#include "ns_http_cache.h"
#include "ns_http_cache_store.h"
#include "ns_js.h"
#include "ns_vuser_tasks.h"
#include "ns_event_log.h"
#include "ns_vuser_trace.h"
#include "tr069/src/ns_tr069_lib.h"
#include "ns_session_pacing.h"
#include "ns_connection_pool.h"
#include "ns_sync_point.h"
#include "ns_proxy_server.h"
#include "nslb_time_stamp.h"
#include "ns_rbu_api.h"
#include "ns_test_gdf.h"
#include "ns_string.h"
#include "ns_group_data.h"
#include "ns_user_define_headers.h"
#include "ns_debug_trace.h"
#include "ns_inline_delay.h"
#include "ns_websocket.h"
#include "ns_page_based_stats.h"
#include "ns_gdf.h"

#include "ns_date_vars.h"
#include "ns_random_string.h"
#include "ns_random_vars.h"
#include "ns_unique_numbers.h"
#include "ns_exit.h"
#include "ns_sockjs.h"
#include "ns_socket.h"

#define CM_MON_STOPPED          "Monitor Stopped Successfully"

#ifdef CAV_MAIN
extern __thread SSL_CTX **g_ssl_ctx;
extern __thread int g_cache_avgtime_idx;
extern __thread int g_network_cache_stats_avgtime_idx;
extern __thread int dns_lookup_stats_avgtime_idx;
extern __thread int g_ftp_avgtime_idx;
extern __thread int g_ldap_avgtime_idx;
extern __thread int g_imap_avgtime_idx;
extern __thread int g_jrmi_avgtime_idx;
extern __thread int g_ws_avgtime_idx;
extern __thread int g_xmpp_avgtime_idx;
extern __thread int g_avg_um_data_idx;
extern __thread unsigned int rbu_page_stat_data_gp_idx;
extern __thread unsigned int show_vuser_flow_idx;
extern __thread NormObjKey rbu_domian_normtbl;
extern __thread int rbu_domain_stat_avg_idx;
extern __thread Rbu_domain_loc2norm_table *g_domain_loc2norm_table;
//extern __thread jmeter_avgtime *g_jmeter_avgtime;
extern __thread PerHostSvrTableEntry_Shr* actsvr_table_shr_mem;
extern __thread int num_dyn_host_left;
extern __thread int is_static_host_shm_created;
extern __thread int num_dyn_host_add;
extern __thread SMMonSessionInfo *sm_mon_info;
#endif
static int gVuserBacklog=0;
VUser* gvptr;   // TODO -  Why Global ??
static unsigned int g_user_index = 0;
char *vuser_states[] = {"IDLE", "ACTIVE", "THINKING", "CLEANUP", "SESSION_THINK", "SYNCPOINT_WAITING", "BLOCKED", "PAUSED"}; //This is to save the sate of vuser

VUser*
allocate_user_tables(int num_users) {
  NSDL2_SCHEDULE(NULL, NULL, "Method called. num_users = %d", num_users);
  VUser* vuser_chunk;
  UserGroupEntry *user_groups_chunk;
  UserCookieEntry *user_cookies_chunk;
  UserDynVarEntry *user_dynamic_vars_chunk;
#ifdef RMI_MODE
  UserByteVarEntry *user_byte_vars_chunk;
#endif
  UserSvrEntry *user_svr_chunk;
  UserServerResolveEntry *usr_chunk = NULL;  
  HostSvrEntry *host_server_chunk;
  UserVarEntry *user_var_chunk;
  int* order_chunk;
  int vnum;
  timer_type *timer_ptr, *timer_chunk;
  MY_MALLOC(user_groups_chunk, num_users * user_group_table_size, "user_groups_chunk", -1);
  MY_MALLOC(user_cookies_chunk, num_users * user_cookie_table_size, "user_cookies_chunk", -1);
  MY_MALLOC(user_dynamic_vars_chunk, num_users * user_dynamic_vars_table_size, "user_dynamic_vars_chunk", -1);
#ifdef RMI_MODE
  MY_MALLOC(user_byte_vars_chunk, num_users * user_byte_vars_table_size, "user_byte_vars_chunk", -1);
#endif
  MY_MALLOC(user_svr_chunk, num_users * user_svr_table_size, "user_svr_chunk", -1);

  if(usr_table_size)
    MY_MALLOC(usr_chunk, num_users * usr_table_size, "usr_chunk", -1);

  MY_MALLOC(user_var_chunk, num_users * user_var_table_size, "user_var_chunk", -1);
  memset(user_var_chunk, 0, num_users * user_var_table_size);

  MY_MALLOC(host_server_chunk, num_users * sizeof(HostSvrEntry) * (g_cur_server + 1), "host_server_chunk", -1);
  MY_MALLOC(order_chunk, num_users * user_order_table_size, "order_chunk", -1);

  MY_MALLOC(timer_chunk, num_users * sizeof(timer_type), "timer_chunk", -1);
  timer_ptr = timer_chunk;
  // Allocate chunk for VUserCtx AN-CTX
  // Should we allocate conditionally if yes then copy carefully by checking null
  VUserCtx* vuser_ctx_chunk;
  MY_MALLOC(vuser_ctx_chunk, num_users * sizeof(VUserCtx), "vuser_ctx_chunk", -1);
  memset(vuser_ctx_chunk, 0, num_users * sizeof(VUserCtx));

  MY_MALLOC(vuser_chunk, num_users * sizeof(VUser), "vuser_chunk", -1);

  memset(vuser_chunk, 0, num_users * sizeof(VUser));
  for (vnum = 0; vnum < num_users; vnum++) {
    //vuser_chunk[vnum].first_cptr = &connections_chunk[vnum*global_settings->max_con_per_vuser];
    //vuser_chunk[vnum].first_cptr = NULL;

    if(vnum < (num_users - 1))
      vuser_chunk[vnum].free_next = (struct VUser*) &vuser_chunk[vnum+1];
    else
      vuser_chunk[vnum].free_next = NULL;

    vuser_chunk[vnum].busy_next = NULL;
    vuser_chunk[vnum].busy_prev = NULL;
    vuser_chunk[vnum].hptr = &host_server_chunk[vnum*(g_cur_server+1)];
    vuser_chunk[vnum].ugtable = &user_groups_chunk[vnum*total_group_entries];
    vuser_chunk[vnum].uctable = &user_cookies_chunk[vnum*max_cookie_hash_code];
    vuser_chunk[vnum].udvtable = &user_dynamic_vars_chunk[vnum*max_dynvar_hash_code];
#ifdef RMI_MODE
    vuser_chunk[vnum].ubvtable = &user_byte_vars_chunk[vnum*max_bytevar_hash_code];
#endif
    vuser_chunk[vnum].uvtable = &user_var_chunk[vnum*max_var_table_idx];
    //Setting 0, because value checked for REFRESH=SESSION. dont want to set the flag if there are no 
    //variables (user_var_table_size) - because the chunk is 0 size.
    // if (vuser_chunk[vnum].uvtable){
      // vuser_chunk[vnum].uvtable->filled_flag = 0; 
    // }
    vuser_chunk[vnum].order_table = &order_chunk[vnum*max_var_table_idx];
    vuser_chunk[vnum].ustable = &user_svr_chunk[vnum*total_svr_entries];
    if(usr_chunk)
      vuser_chunk[vnum].usr_entry = &usr_chunk[vnum*total_svr_entries]; 
    vuser_chunk[vnum].timer_ptr = timer_ptr;
    timer_ptr++;
    vuser_chunk[vnum].timer_ptr->timer_type = -1;
    vuser_chunk[vnum].timer_ptr->next = NULL;
    vuser_chunk[vnum].timer_ptr->prev = NULL;
    vuser_chunk[vnum].is_embd_autofetch = 0;
    vuser_chunk[vnum].redirect_count = 0;
    vuser_chunk[vnum].num_http_req_free = 0;
    // set to zero as we are using this flag for marking user ramp down
    vuser_chunk[vnum].flags = 0;
    vuser_chunk[vnum].reload_attempts = -1;  // Reload attempt filled in on page start

    vuser_chunk[vnum].svr_map_change = NULL;
    MY_MALLOC(vuser_chunk[vnum].httpData, sizeof(HTTPData_t), "HTTPData_t", -1);
    memset(vuser_chunk[vnum].httpData, 0, sizeof(HTTPData_t));
    /*vuser_chunk[vnum].js_fields = NULL; // for Java Script Engine */
    vuser_chunk[vnum].ctxptr = &(vuser_ctx_chunk[vnum]);
    vuser_chunk[vnum].head_cinuse = NULL;
    vuser_chunk[vnum].tail_cinuse = NULL;
    if(IS_PROXY_ENABLED) {
      MY_MALLOC_AND_MEMSET(vuser_chunk[vnum].httpData->proxy_con_resp_time, sizeof(Proxy_con_resp_time), "Malloc proxy_con_resp_time", 1);
    }
    vuser_chunk[vnum].server_entry_idx = -1;
   
    //websocket  
    NSDL2_SCHEDULE(NULL, NULL, "max_ws_conn = %d", max_ws_conn);
    if(max_ws_conn)
    {
      NSDL2_SCHEDULE(NULL, NULL, "Allocating memory to multiple websocket connectionis: "
                                 "Size of connection = %lu, max_ws_conn = %d\n", sizeof(connection *), max_ws_conn);
      MY_MALLOC_AND_MEMSET(vuser_chunk[vnum].ws_cptr, (max_ws_conn * sizeof(connection *)), "Malloc ws_cptr", 1);
    }

    //sockjs  
    NSDL2_SCHEDULE(NULL, NULL, "max_sockjs_conn = %d", max_sockjs_conn);
    if(max_sockjs_conn)
    {
      NSDL2_SCHEDULE(NULL, NULL, "Allocating memory to multiple sockjs connections: "
                                 "Size of connection = %lu, max_sockjs_conn = %d\n", sizeof(connection *), max_sockjs_conn);
      MY_MALLOC_AND_MEMSET(vuser_chunk[vnum].sockjs_cptr, (max_sockjs_conn * sizeof(connection *)), "Malloc sockjs_cptr", 1);
    }

    //Socket(TCP/UDP)
    NSDL2_SCHEDULE(NULL, NULL, "max_socket_conn = %d", g_socket_vars.max_socket_conn);
    if(g_socket_vars.max_socket_conn)
    {
      NSDL2_SCHEDULE(NULL, NULL, "Allocating memory to multiple socket connections: "
                                 "Size of connection = %lu, max_socket_conn = %d\n", sizeof(connection *), g_socket_vars.max_socket_conn);
      MY_MALLOC_AND_MEMSET(vuser_chunk[vnum].conn_array, (g_socket_vars.max_socket_conn * sizeof(connection *)), "Malloc socket_cptr", 1);
    }


  }
  gFreeVuserCnt += num_users;
  alloc_runlogic_stack((char *)vuser_chunk, num_users);

  return vuser_chunk;
}

// Get group num if passed -1 from new_user()
// Used also in Ramp down. So be carefule when u make change in this method
int generate_scen_group_num()
{
  RunProfTableEntry_Shr *rstart;
  int max = total_runprof_entries;
  int max_rand; 
  int sidx, sidx_save;
  int phase_idx;

  /* If we are here, it means, we are scenario based */
  Schedule *schedule = v_port_entry->scenario_schedule;
  phase_idx = schedule->phase_idx;

  Phases *ph = &(schedule->phase_array[phase_idx]);
  int *per_grp_qty;
  unsigned int rand_num;

  max_rand = schedule->cumulative_runprof_table[max -1];
  rand_num = ns_get_random(rp_handle);

  switch(ph->phase_type) {
  case SCHEDULE_PHASE_RAMP_UP:
    per_grp_qty = ph->phase_cmd.ramp_up_phase.per_grp_qty;
    break;
  case SCHEDULE_PHASE_RAMP_DOWN:
    per_grp_qty = ph->phase_cmd.ramp_down_phase.per_grp_qty;
    break;
  }


  NSDL3_SCHEDULE(NULL, NULL, "Method called. rand_num = %u, max_rand = %u", rand_num, max_rand);

  rand_num = (rand_num % max_rand);

  for (sidx = 0, rstart = runprof_table_shr_mem; sidx < max; sidx ++, rstart++) {
    // Protected due to my_runprof_table
    NSDL3_SCHEDULE(NULL, NULL, "Checking scenario group (%d) name = %s, cumulative user count = %u", 
                   rstart->group_num, rstart->scen_group_name, my_runprof_table[sidx]);
    if (rand_num < schedule->cumulative_runprof_table[sidx]) {

      if (get_group_mode(-1) == TC_FIX_CONCURRENT_USERS) {
        //if (my_runprof_table[sidx] == 0) {
        if (per_grp_qty[sidx] == 0) {
          sidx_save = sidx;
          NSDL3_SCHEDULE(NULL, NULL, "Found scenario group (%d) name = %s, cumulative user count = %u, with 0 users left", 
                         rstart->group_num, rstart->scen_group_name, per_grp_qty[sidx]/* my_runprof_table[sidx] */);

          for(; sidx < max; sidx ++, rstart++) {
            if (/* my_runprof_table[sidx] */ per_grp_qty[sidx] != 0)
              break;
          }
          if (sidx == max) {
            sidx = sidx_save;
            rstart = &runprof_table_shr_mem[sidx_save];
            for (; sidx >= 0; sidx--, rstart--) {
              if (/* my_runprof_table[sidx] */ per_grp_qty[sidx] != 0)
                break;
            }
            if (sidx < 0) {
              /* we start going backwards */
              NS_EXIT(-1, "No users left");
            }
          }
        }
      
        (per_grp_qty[sidx])--;
        NSDL3_SCHEDULE(NULL, NULL, "per_grp_qty[%d]=%d", sidx, per_grp_qty[sidx]);
        switch(ph->phase_type) 
        {
          case SCHEDULE_PHASE_RAMP_UP:
            schedule->cur_users[sidx]++;
            break;
          case SCHEDULE_PHASE_RAMP_DOWN:
            schedule->cur_users[sidx]--;
            break;
        }
        NSDL3_SCHEDULE(NULL, NULL, "Existing users(cur_users) in the NVM for group(%d) = %d", sidx, schedule->cur_users[sidx]);

     }
      // protected due to my_runprof_table
      NSDL3_SCHEDULE(NULL, NULL, "Returning  %d. scenario group (%d) name = %s, cumulative user count = %u, Phase Type = %d", 
                     sidx, rstart->group_num, rstart->scen_group_name, my_runprof_table[sidx], ph->phase_type);
      return sidx;
    }
  }
  
  return -1;
}

static inline void
free_user_slot(VUser *vptr, u_ns_ts_t now)
{
  NSDL2_SCHEDULE(vptr, NULL, "Method called, Entering free_user_slot:"
                             " Free Count=%d vnum_ptr=%p at %u", 
                             gFreeVuserCnt, vptr, now);

  VUSER_TO_IDLE(vptr); //changing the state of vuser to idle
  
  // unset mark_ramp_down
  vptr->flags &= ~NS_VUSER_RAMPING_DOWN;
  vptr->flags &= ~NS_VUSER_GRADUAL_EXITING;
  vptr->operation = VUT_NO_TASK;//Since we are cleaning up the user so no task should be there
#ifdef NS_USE_MODEM
  cav_close_modem(vptr);
#endif
  vptr->server_entry_idx = -1; 
  /* We are not freeing vptr->referer as we are reusing the same buffer. This will help in performance optimization */
  //  FREE_AND_MAKE_NOT_NULL(vptr->referer, "vptr->referer", -1);
  
  if(NS_IF_CACHING_ENABLE_FOR_USER)
    free_cache_for_user(vptr);

  // Always free stack as same vptr may get allcated to another user
  // running different scenario grp with different stack size setting
  // We can optimize later to free only if stack size if different
  // Note - Do not free ctxptr itself as part of malloced buffere
  // and will be required
  // BugId:14764, getting core evenif vptr->ctxptr and vptr->ctxptr->stack is not NULL, GDB trace is -
  /* 
     {ctx = {uc_flags = 0, uc_link = 0xd1dc00, uc_stack = {ss_sp = 0x68aac30, ss_flags = 0, ss_size = 25600}, uc_mcontext = {gregs = {0, 0, 0, 0, 2, 
        2, 285982, 0, 110167456, 13753344, 109776752, 109776928, 13753344, 0, 757098, 109776640, 6354526, 0, 0, 0, 0, 0, 0}, fpregs = 0x6910748, 
      __reserved1 = {0, 0, 0, 0, 0, 0, 0, 0}}, uc_sigmask = {__val = {0 <repeats 16 times>}}, __fpregs_mem = {cwd = 895, swd = 65535, ftw = 0, 
      fop = 65535, rip = 4294967295, rdp = 0, mxcsr = 8097, mxcr_mask = 0, _st = {{significand = {0, 0, 0, 0}, exponent = 0, padding = {0, 0, 0}}, {
          significand = {0, 0, 0, 0}, exponent = 0, padding = {0, 0, 0}}, {significand = {0, 0, 0, 0}, exponent = 0, padding = {0, 0, 0}}, {
          significand = {0, 0, 0, 0}, exponent = 0, padding = {0, 0, 0}}, {significand = {0, 0, 0, 0}, exponent = 0, padding = {0, 0, 0}}, {
          significand = {0, 0, 0, 0}, exponent = 0, padding = {0, 0, 0}}, {significand = {0, 0, 0, 0}, exponent = 0, padding = {0, 0, 0}}, {
          significand = {0, 0, 0, 0}, exponent = 0, padding = {0, 0, 0}}}, _xmm = {{element = {0, 0, 0, 0}} <repeats 16 times>}, padding = {
        0 <repeats 24 times>}}}, stack_size = 0, stack = 0x68aac30 "\370\276\333\003Y\177"}

  */
  //TODO: Finding out - Why we are getting stack_size = 0 and vptr->ctxptr->stack != NULL ?
  if(vptr->ctxptr && (vptr->ctxptr->stack_size != 0))  
    FREE_AND_MAKE_NULL(vptr->ctxptr->stack, "User stack", -1);

  vptr->free_next = (struct VUser*)gFreeVuserHead;
  gFreeVuserHead = vptr;
  vptr->last_cptr = NULL; //TODO:

  if (vptr == gBusyVuserHead)
    gBusyVuserHead = (VUser*) vptr->busy_next;

  if (vptr == gBusyVuserTail)
    gBusyVuserTail = (VUser*) vptr->busy_prev;

  if (vptr->busy_next)
    ((VUser*) vptr->busy_next)->busy_prev = vptr->busy_prev;

  if (vptr->busy_prev)
    ((VUser*) vptr->busy_prev)->busy_next = vptr->busy_next;

  vptr->busy_next = vptr->busy_prev = NULL;
add_srcip_to_list(vptr, vptr->user_ip);

  //TODO: Need to checck again
  //Temparary fix Khols core dump issue
  //Freeing proxy connect response time memory for reporting in case allocated
  //if(IS_PROXY_ENABLED)
  //{
    //FREE_AND_MAKE_NULL(vptr->httpData->proxy_con_resp_time, "Free proxy_con_resp_time", -1);
  //}
  

  //if (global_settings->debug &&  (global_settings->module_mask & FUNCTION_CALL_OUTPUT))
  if(vptr->ns_ws_info)
  {
    NSDL4_MISC(vptr, NULL, "calling ns_free_soap_ws_security");
    ns_free_soap_ws_security(vptr->ns_ws_info);
    vptr->ns_ws_info=NULL;
  }
  NSDL1_MISC(vptr, NULL, "Exiting free_user_slot: Free Count=%d", gFreeVuserCnt);
}

//For debug purpose only
void log_user_profile_for_debug(VUser *vptr)
{ 
  char file_name[4096];
  
  sprintf(file_name, "%s/logs/%s/ns_logs/user_profile_rec.dat", g_ns_wdir, global_settings->tr_or_common_files);
  // Format of file -> TimeStamp|Group|ProfileName|Location|ACCESS|BROWSER|SCREEN_SIZE
  ns_save_data_ex(file_name, NS_APPEND_FILE, "%s|%s|%s|%s|%s|%s|%dx%d", get_relative_time(), runprof_table_shr_mem[vptr->group_num].scen_group_name, (vptr->up_ptr->name) , (vptr->location->name), (vptr->access->name), (vptr->browser->name), (vptr->screen_size->width), (vptr->screen_size->height));
}

void cav_main_set_global_vars(SMMonSessionInfo *sm_mon_ptr)
{
#ifdef CAV_MAIN 
  
   sm_mon_info                                   = sm_mon_ptr;
   global_settings                               = sm_mon_ptr->global_settings;
   runprof_table_shr_mem                         = sm_mon_ptr->runprof_table_shr_mem;
   v_port_entry                                  = sm_mon_ptr->v_port_entry;
   v_port_table                                  = sm_mon_ptr->v_port_table;
   average_time                                  = sm_mon_ptr->average_time;
   big_buf_shr_mem                               = sm_mon_ptr->big_buf_shr_mem;
   pointer_table_shr_mem                         = sm_mon_ptr->pointer_table_shr_mem;
   weight_table_shr_mem                          = sm_mon_ptr->weight_table_shr_mem;
   group_table_shr_mem                           = sm_mon_ptr->group_table_shr_mem;   
   variable_table_shr_mem                        = sm_mon_ptr->variable_table_shr_mem;
   index_variable_table_shr_mem                  = sm_mon_ptr->index_variable_table_shr_mem;
   repeat_block_shr_mem                          = sm_mon_ptr->repeat_block_shr_mem;
   randomvar_table_shr_mem                       = sm_mon_ptr->randomvar_table_shr_mem;
   randomstring_table_shr_mem                    = sm_mon_ptr->randomstring_table_shr_mem;
   uniquevar_table_shr_mem                       = sm_mon_ptr->uniquevar_table_shr_mem;
   datevar_table_shr_mem                         = sm_mon_ptr->datevar_table_shr_mem;
   gserver_table_shr_mem                         = sm_mon_ptr->gserver_table_shr_mem;
   seg_table_shr_mem                             = sm_mon_ptr->seg_table_shr_mem;
   serverorder_table_shr_mem                     = sm_mon_ptr->serverorder_table_shr_mem;
   post_table_shr_mem                            = sm_mon_ptr->post_table_shr_mem;
   reqcook_table_shr_mem                         = sm_mon_ptr->reqcook_table_shr_mem;
   reqdynvar_table_shr_mem                       = sm_mon_ptr->reqdynvar_table_shr_mem;
   clickaction_table_shr_mem                     = sm_mon_ptr->clickaction_table_shr_mem;
   request_table_shr_mem                         = sm_mon_ptr->request_table_shr_mem;
   host_table_shr_mem                            = sm_mon_ptr->host_table_shr_mem;
   thinkprof_table_shr_mem                       = sm_mon_ptr->thinkprof_table_shr_mem;
   inline_delay_table_shr_mem                    = sm_mon_ptr->inline_delay_table_shr_mem;
   autofetch_table_shr_mem                       = sm_mon_ptr->autofetch_table_shr_mem;
   pacing_table_shr_mem                          = sm_mon_ptr->pacing_table_shr_mem;
   continueOnPageErrorTable_shr_mem              = sm_mon_ptr->continueOnPageErrorTable_shr_mem;
   perpagechkpt_table_shr_mem                    = sm_mon_ptr->perpagechkpt_table_shr_mem;
   page_table_shr_mem                            = sm_mon_ptr->page_table_shr_mem;
   session_table_shr_mem                         = sm_mon_ptr->session_table_shr_mem;
   locattr_table_shr_mem                         = sm_mon_ptr->locattr_table_shr_mem;
   accattr_table_shr_mem                         = sm_mon_ptr->accattr_table_shr_mem;
   browattr_table_shr_mem                        = sm_mon_ptr->browattr_table_shr_mem;
   scszattr_table_share_mem                      = sm_mon_ptr->scszattr_table_share_mem;
   sessprof_table_shr_mem                        = sm_mon_ptr->sessprof_table_shr_mem;
   sessprofindex_table_shr_mem                   = sm_mon_ptr->sessprofindex_table_shr_mem;
   proxySvr_table_shr_mem                        = sm_mon_ptr->proxySvr_table_shr_mem;
   proxyExcp_table_shr_mem                       = sm_mon_ptr->proxyExcp_table_shr_mem;
   metric_table_shr_mem                          = sm_mon_ptr->metric_table_shr_mem;
   inusesvr_table_shr_mem                        = sm_mon_ptr->inusesvr_table_shr_mem;
   errorcode_table_shr_mem                       = sm_mon_ptr->errorcode_table_shr_mem;
   userprofindex_table_shr_mem                   = sm_mon_ptr->userprofindex_table_shr_mem;
   runprofindex_table_shr_mem                    = sm_mon_ptr->runprofindex_table_shr_mem;
   tx_table_shr_mem                              = sm_mon_ptr->tx_table_shr_mem;
   http_method_table_shr_mem                     = sm_mon_ptr->http_method_table_shr_mem;
   pattern_table_shr                             = sm_mon_ptr->pattern_table_shr;
   group_default_settings                        = sm_mon_ptr->group_default_settings;

   v_port_entry->scenario_schedule->sm_mon_info  = sm_mon_ptr;
   g_cur_server                                  = sm_mon_ptr->g_cur_server;
   user_svr_table_size                           = sm_mon_ptr->user_svr_table_size;
   user_group_table_size                         = sm_mon_ptr->user_group_table_size;
   user_cookie_table_size                        = sm_mon_ptr->user_cookie_table_size;
   user_dynamic_vars_table_size                  = sm_mon_ptr->user_dynamic_vars_table_size;
   user_var_table_size                           = sm_mon_ptr->user_var_table_size;
   user_order_table_size                         = sm_mon_ptr->user_order_table_size;
   g_ssl_ctx                                     = sm_mon_ptr->g_ssl_ctx;

   // Avg Data

   g_avgtime_size                                = sm_mon_ptr->g_avgtime_size;
   g_cache_avgtime_idx                           = sm_mon_ptr->g_cache_avgtime_idx;
   g_proxy_avgtime_idx                           = sm_mon_ptr->g_proxy_avgtime_idx;
   g_network_cache_stats_avgtime_idx             = sm_mon_ptr->g_network_cache_stats_avgtime_idx;
   dns_lookup_stats_avgtime_idx                  = sm_mon_ptr->dns_lookup_stats_avgtime_idx;
   g_ftp_avgtime_idx                             = sm_mon_ptr->g_ftp_avgtime_idx;
   g_ldap_avgtime_idx                            = sm_mon_ptr->g_ldap_avgtime_idx;
   g_imap_avgtime_idx                            = sm_mon_ptr->g_imap_avgtime_idx;
   g_jrmi_avgtime_idx                            = sm_mon_ptr->g_jrmi_avgtime_idx;
   g_ws_avgtime_idx                              = sm_mon_ptr->g_ws_avgtime_idx;
   g_xmpp_avgtime_idx                            = sm_mon_ptr->g_xmpp_avgtime_idx;
   g_fc2_avgtime_idx                             = sm_mon_ptr->g_fc2_avgtime_idx;
   // becomes obsolete from 4.5.1 jmeter enhancement
  // g_jmeter_avgtime_idx                          = sm_mon_ptr->g_jmeter_avgtime_idx;
   g_tcp_client_avg_idx                          = sm_mon_ptr->g_tcp_client_avg_idx;
   g_udp_client_avg_idx                          = sm_mon_ptr->g_udp_client_avg_idx;
   g_avg_size_only_grp                           = sm_mon_ptr->g_avg_size_only_grp;
   g_avg_um_data_idx                             = sm_mon_ptr->g_avg_um_data_idx;
   group_data_gp_idx                             = sm_mon_ptr->group_data_gp_idx;
   rbu_page_stat_data_gp_idx                     = sm_mon_ptr->rbu_page_stat_data_gp_idx;
   page_based_stat_gp_idx                        = sm_mon_ptr->page_based_stat_gp_idx;
   show_vuser_flow_idx                           = sm_mon_ptr->show_vuser_flow_idx;
   g_static_avgtime_size                         = sm_mon_ptr->g_static_avgtime_size;
   g_udp_client_failures_avg_idx                 = sm_mon_ptr->g_udp_client_failures_avg_idx;
   memcpy(&g_udp_client_errs_normtbl, &sm_mon_ptr->g_udp_client_errs_normtbl,sizeof(NormObjKey));
   g_total_udp_client_errs                       = sm_mon_ptr->g_total_udp_client_errs;
   g_tcp_client_failures_avg_idx                 = sm_mon_ptr->g_tcp_client_failures_avg_idx;
   g_tcp_client_errs_normtbl                     = sm_mon_ptr->g_tcp_client_errs_normtbl;
   memcpy(&g_total_tcp_client_errs, &sm_mon_ptr->g_total_tcp_client_errs, sizeof(NormObjKey));
   http_resp_code_avgtime_idx                    = sm_mon_ptr->http_resp_code_avgtime_idx;
   total_http_resp_code_entries                  = sm_mon_ptr->total_http_resp_code_entries;
   g_http_status_code_loc2norm_table             = sm_mon_ptr->g_http_status_code_loc2norm_table;
   total_tx_entries                              = sm_mon_ptr->total_tx_entries;
   txData                                        = sm_mon_ptr->txData;
   g_trans_avgtime_idx                           = sm_mon_ptr->g_trans_avgtime_idx;
   g_tx_loc2norm_table                           = sm_mon_ptr->g_tx_loc2norm_table;
   memcpy(&rbu_domian_normtbl, &sm_mon_ptr->rbu_domian_normtbl, sizeof(NormObjKey));
   rbu_domain_stat_avg_idx                       = sm_mon_ptr->rbu_domain_stat_avg_idx;
   g_domain_loc2norm_table                       = sm_mon_ptr->g_domain_loc2norm_table;
   memcpy(&normRuntimeTXTable, &sm_mon_ptr->normRuntimeTXTable, sizeof(NormObjKey));
   
   um_info                                       = sm_mon_ptr->um_info;
   cache_avgtime                                 = sm_mon_ptr->cache_avgtime;
   proxy_avgtime                                 = sm_mon_ptr->proxy_avgtime;
   network_cache_stats_avgtime                   = sm_mon_ptr->network_cache_stats_avgtime;
   dns_lookup_stats_avgtime                      = sm_mon_ptr->dns_lookup_stats_avgtime;
   ftp_avgtime                                   = sm_mon_ptr->ftp_avgtime;
   ldap_avgtime                                  = sm_mon_ptr->ldap_avgtime;
   imap_avgtime                                  = sm_mon_ptr->imap_avgtime;
   jrmi_avgtime                                  = sm_mon_ptr->jrmi_avgtime;
   xmpp_avgtime                                  = sm_mon_ptr->xmpp_avgtime;
   rbu_domain_stat_avg                           = sm_mon_ptr->rbu_domain_stat_avg;
   http_resp_code_avgtime                        = sm_mon_ptr->http_resp_code_avgtime;
   g_tcp_client_failures_avg                     = sm_mon_ptr->g_tcp_client_failures_avg;
   g_udp_client_failures_avg                     = sm_mon_ptr->g_udp_client_failures_avg;
   ns_diag_avgtime                               = sm_mon_ptr->ns_diag_avgtime;
   g_tcp_client_avg                              = sm_mon_ptr->g_tcp_client_avg;
   g_udp_client_avg                              = sm_mon_ptr->g_udp_client_avg;
   grp_avgtime                                   = sm_mon_ptr->grp_avgtime;
   rbu_page_stat_avg                             = sm_mon_ptr->rbu_page_stat_avg;
   page_stat_avgtime                             = sm_mon_ptr->page_stat_avgtime;
   vuser_flow_avgtime                            = sm_mon_ptr->vuser_flow_avgtime;
   cavtest_http_avg                              = sm_mon_ptr->cavtest_http_avg;
   cavtest_web_avg                               = sm_mon_ptr->cavtest_web_avg;
   g_cavtest_http_avg_idx                        = sm_mon_ptr->g_cavtest_http_avg_idx;
   g_cavtest_web_avg_idx                         = sm_mon_ptr->g_cavtest_web_avg_idx;
//   g_jmeter_avgtime                              = sm_mon_ptr->g_jmeter_avgtime;
   sess_inst_num                                 = sm_mon_ptr->sess_inst_num;
 //  snprintf(g_controller_testrun, 32, "%d", sm_mon_ptr->test_run);

   total_runprof_entries                         = sm_mon_ptr->total_runprof_entries;

   gBusyVuserHead                                = sm_mon_ptr->gBusyVuserHead;
   gBusyVuserTail                                = sm_mon_ptr->gBusyVuserTail;
   gFreeVuserHead                                = sm_mon_ptr->gFreeVuserHead;
   gFreeVuserCnt                                 = sm_mon_ptr->gFreeVuserCnt;
   gFreeVuserMinCnt                              = sm_mon_ptr->gFreeVuserMinCnt;   
   my_runprof_table                              = sm_mon_ptr->my_runprof_table;
   my_vgroup_table                               = sm_mon_ptr->my_vgroup_table;
   unique_range_var_table                        = sm_mon_ptr->unique_range_var_table;
   actsvr_table_shr_mem                          = sm_mon_ptr->actsvr_table_shr_mem;
   num_dyn_host_left                             = sm_mon_ptr->num_dyn_host_left;
   num_dyn_host_add                              = sm_mon_ptr->num_dyn_host_add;
   is_static_host_shm_created                    = sm_mon_ptr->is_static_host_shm_created;
   // To Save current parition id
   global_settings->cavtest_partition_idx        = g_partition_idx;   
   per_proc_vgroup_table                         = sm_mon_ptr->per_proc_vgroup_table;
   fparamValueTable_shr_mem                      = sm_mon_ptr->fparamValueTable_shr_mem;
   nsl_var_table_shr_mem                         = sm_mon_ptr->nsl_var_table_shr_mem;
   searchvar_table_shr_mem                       = sm_mon_ptr->searchvar_table_shr_mem;
   seq_group_next                                = sm_mon_ptr->seq_group_next;

   used_buffer_space                             = sm_mon_ptr->used_buffer_space;
   total_pointer_entries                         = sm_mon_ptr->total_pointer_entries;
   total_weight_entries                          = sm_mon_ptr->total_weight_entries;
   total_group_entries                           = sm_mon_ptr->total_group_entries;
   total_var_entries                             = sm_mon_ptr->total_var_entries;
   total_index_var_entries                       = sm_mon_ptr->total_index_var_entries;
   total_repeat_block_entries                    = sm_mon_ptr->total_repeat_block_entries;
   total_nsvar_entries                           = sm_mon_ptr->total_nsvar_entries;
   total_randomvar_entries                       = sm_mon_ptr->total_randomvar_entries;
   total_randomstring_entries                    = sm_mon_ptr->total_randomstring_entries;
   total_uniquevar_entries                       = sm_mon_ptr->total_uniquevar_entries;
   total_datevar_entries                         = sm_mon_ptr->total_datevar_entries;
   total_svr_entries                             = sm_mon_ptr->total_svr_entries;
   total_seg_entries                             = sm_mon_ptr->total_seg_entries;
   total_serverorder_entries                     = sm_mon_ptr->total_serverorder_entries;
   total_post_entries                            = sm_mon_ptr->total_post_entries;
   total_reqcook_entries                         = sm_mon_ptr->total_reqcook_entries;
   total_reqdynvar_entries                       = sm_mon_ptr->total_reqdynvar_entries;
   total_clickaction_entries                     = sm_mon_ptr->total_clickaction_entries;
   total_request_entries                         = sm_mon_ptr->total_request_entries;
   total_http_method                             = sm_mon_ptr->total_http_method;
   total_host_entries                            = sm_mon_ptr->total_host_entries;
   total_jsonvar_entries                         = sm_mon_ptr->total_jsonvar_entries;
   total_perpagejsonvar_entries                  = sm_mon_ptr->total_perpagejsonvar_entries;
   total_checkpoint_entries                      = sm_mon_ptr->total_checkpoint_entries;
   total_perpagechkpt_entries                    = sm_mon_ptr->total_perpagechkpt_entries;
   total_checkreplysize_entries                  = sm_mon_ptr->total_checkreplysize_entries;
   total_perpagechkrepsize_entries               = sm_mon_ptr->total_perpagechkrepsize_entries;
   total_page_entries                            = sm_mon_ptr->total_page_entries;
   total_tx_entries                              = sm_mon_ptr->total_tx_entries;
   total_sess_entries                            = sm_mon_ptr->total_sess_entries;
   total_locattr_entries                         = sm_mon_ptr->total_locattr_entries;
   total_sessprof_entries                        = sm_mon_ptr->total_sessprof_entries;
   total_inusesvr_entries                        = sm_mon_ptr->total_inusesvr_entries;
   total_errorcode_entries                       = sm_mon_ptr->total_errorcode_entries;
   total_clustvar_entries                        = sm_mon_ptr->total_clustvar_entries;
   total_clust_entries                           = sm_mon_ptr->total_clust_entries;
   total_groupvar_entries                        = sm_mon_ptr->total_groupvar_entries;
   total_userindex_entries                       = sm_mon_ptr->total_userindex_entries;
   total_userprofshr_entries                     = sm_mon_ptr->total_userprofshr_entries;
   g_static_vars_shr_mem_size                    = sm_mon_ptr->g_static_vars_shr_mem_size;
   total_fparam_entries                          = sm_mon_ptr->total_fparam_entries;
   total_searchvar_entries                       = sm_mon_ptr->total_searchvar_entries;
   total_perpageservar_entries                   = sm_mon_ptr->total_perpageservar_entries;
   unique_group_table                            = sm_mon_ptr->unique_group_table;
   unique_group_id                               = sm_mon_ptr->unique_group_id;
#endif
}

static void
init_user_session(VUser *vptr, u_ns_ts_t now, int scen_group_num)
{
  unsigned int rand_num;
  //  RunProfIndexTableEntry_Shr* runindex_ptr;
  SessProfIndexTableEntry_Shr* sessindex_ptr;
  UserProfIndexTableEntry_Shr* userindex_ptr;
  int sidx;
  RunProfTableEntry_Shr *rstart;
  SessProfTableEntry_Shr *sstart;
  UserProfTableEntry_Shr *ustart;
  int max;
  int set_user_characs = 0;
  int prof_pct_start_idx;
  int profile_idx;
  
  NSDL3_SCHEDULE(NULL, NULL, "Method called. scen_group_num=%d", scen_group_num);

  if ((scen_group_num == -1)
      /* && (global_settings->load_key) */) {
    scen_group_num = generate_scen_group_num();
  }

  //  runindex_ptr = testcase_shr_mem->runindex_ptr;
  //  TODO - can scen_group_num be -1 at this point. If yes, why?
  if (scen_group_num != -1) {
      rstart = &runprof_table_shr_mem[scen_group_num];
      if (global_settings->use_sess_prof)
          sessindex_ptr = rstart->sessindexprof_ptr;
      else
        vptr->sess_ptr = rstart->sess_ptr;
      
      NSDL3_SCHEDULE(vptr, NULL, "For group %s", rstart->scen_group_name);
      NSDL3_SCHEDULE(vptr, NULL, "**********rstart->userindexprof_ptr->length = %d", rstart->userindexprof_ptr->length);

      userindex_ptr = rstart->userindexprof_ptr;
      vptr->clust_id = rstart->cluster_id;
      vptr->group_num = rstart->group_num;
      // Check and enable user for caching
      cache_vuser_check_and_enable_caching(vptr);
      tr069_vuser_data_init(vptr);
  }
  rand_num = ns_get_random(sp_handle);

  if (global_settings->use_sess_prof) {
    max = sessindex_ptr->length;
    sstart = sessindex_ptr->sessprof_start;
    for (sidx = 0; sidx < max; sidx ++, sstart ++) {
    	if (rand_num < sstart->pct) {
        vptr->sess_ptr = sstart->session_ptr;
        break;
    	}
    }
  }

  // Now select the user profile attributes for this used
  //   - Location, Access, Browser, Machine, Frequency
  rand_num = ns_get_random(up_handle);

  max = userindex_ptr->length;
  ustart = userindex_ptr->userprof_start;


  if(global_settings->user_prof_sel_mode){

    NSDL2_WAN(vptr, NULL, "Profile selection mode is Uniform");
    // start index of profile count pct table
    prof_pct_start_idx = userindex_ptr->prof_pct_start_idx;

    // Get profile pct table index 
    profile_idx = get_profile_idx(prof_pct_start_idx, max, rand_num);
  
    // Adjust profile pointer as per index of total prof pct table 
    ustart = ustart + profile_idx - prof_pct_start_idx;

    // Code should not come here 
    if(ustart == NULL){
      NS_EXIT(-1, "This should not happen. Profile selection is not correct. Exiting ...."); 
    } 
    vptr->up_ptr = userindex_ptr;
    vptr->location = ustart->location;
    vptr->access = ustart->access;
    vptr->browser = ustart->browser;
    vptr->screen_size = ustart->screen_size;
    NSDL2_WAN(vptr, NULL, "Selected user profile attributes. sidx = %d, max = %d, rand_num = %d, ustart->pct = %d, location = %s, access = %s, browser = %s", sidx, max, rand_num, ustart->pct, vptr->location->name, vptr->access->name, vptr->browser->name);
    
  }else{

    NSDL2_WAN(vptr, NULL, "Profile selection mode is Random");

    for (sidx = 0; sidx < max; sidx ++, ustart ++) {
      NSDL4_WAN(vptr, NULL, "Checking for the selection of attributes. sidx = %d, max = %d, rand_num = %d, ustart->pct = %d", sidx, max, rand_num, ustart->pct);
      if (rand_num < ustart->pct) {
        vptr->up_ptr = userindex_ptr;
        vptr->location = ustart->location;
        vptr->access = ustart->access;
        vptr->browser = ustart->browser;
        //vptr->machine = ustart->machine;
        //vptr->freq = ustart->frequency;
        vptr->screen_size = ustart->screen_size;
        NSDL2_WAN(vptr, NULL, "Selected user profile attributes. sidx = %d, max = %d, rand_num = %d, ustart->pct = %d, location = %s, access = %s, browser = %s", sidx, max, rand_num, ustart->pct, vptr->location->name, vptr->access->name, vptr->browser->name);
        set_user_characs = 1;
        break;
      }
    }

    if (!set_user_characs) {   // There is a very slim chance that this may happen 
      ustart--;
      vptr->up_ptr = userindex_ptr;
      vptr->location = ustart->location;
      vptr->access = ustart->access;
      vptr->browser = ustart->browser;
      //vptr->machine = ustart->machine;
      //vptr->freq = ustart->frequency;
      vptr->screen_size = ustart->screen_size;
      NSDL1_WAN(vptr, NULL, "Selected user profile attributes after missing selection. sidx = %d, max = %d, rand_num = %d, ustart->pct = %d, location = %s, access = %s, browser = %s", sidx, max, rand_num, ustart->pct, vptr->location->name, vptr->access->name, vptr->browser->name);
    } 
  }

  if(global_settings->ns_trace_level & 0xffff0000)
    log_user_profile_for_debug(vptr);
 
  NSDL3_WAN(NULL, NULL, "vptr->browser->max_con_per_vuser = %d", vptr->browser->max_con_per_vuser);
  /* New design for G_MAX_CON_PER_VUSER keyword:
   * If given max connections per vuser is 0, then will be using browser setting vptr->browser->per_svr_max_conn_http1_1
   * Otherwise value given by user in keyword*/
  if (runprof_table_shr_mem[scen_group_num].gset.max_con_mode == 1) 
  {
    NSDL3_WAN(NULL, NULL, "Maximum connection mode is %d for group %d, hence using browser settings", runprof_table_shr_mem[scen_group_num].gset.max_con_mode, scen_group_num);
    NSDL3_WAN(NULL, NULL, "%s Browser Settings: Maximum connection per user = %d and per server maximum parallel connection = %d", vptr->browser->name, vptr->browser->max_con_per_vuser, vptr->browser->per_svr_max_conn_http1_1);
    vptr->cmax_parallel = vptr->browser->max_con_per_vuser; // TODO: We need to take from structure later after doing perf testing with max con
    //Setting per_svr_max_parallel
    vptr->per_svr_max_parallel = vptr->browser->per_svr_max_conn_http1_1;
  } else {
    NSDL3_WAN(NULL, NULL, "Maximum connection mode is %d for group %d, hence setting as per G_MAX_CON_PER_VUSER keyword", runprof_table_shr_mem[scen_group_num].gset.max_con_mode, scen_group_num);
    vptr->cmax_parallel = runprof_table_shr_mem[scen_group_num].gset.max_con_per_vuser;
    //Setting per_svr_max_parallel
    vptr->per_svr_max_parallel = runprof_table_shr_mem[scen_group_num].gset.max_con_per_svr_http1_1;
  }
    
  //vptr->cmax_parallel = UA_IE_MAX_PARALLEL; // TODO: We need to take from structure later after doing perf testing with max con
  //vptr->per_svr_max_parallel = vptr->browser->per_svr_max_parallel;

  NSDL2_WAN(NULL, NULL, "vptr->cmax_parallel = %d, vptr->per_svr_max_parallel = %d", vptr->cmax_parallel, vptr->per_svr_max_parallel);
#if 0
 /* Manpreet: Commented code, because now we dont require this check as cmax_parallel
  * depends on max_con_per_vuser 's value*/
  if (vptr->cmax_parallel > global_settings->max_con_per_vuser)
  {
    vptr->cmax_parallel = global_settings->max_con_per_vuser;
    NSDL2_WAN(NULL, NULL, "vptr->cmax_parallel is more than global_settings->max_con_per_vuser. So setting to this value. vptr->cmax_parallel = %d, vptr->per_svr_max_parallel = %d", vptr->cmax_parallel, vptr->per_svr_max_parallel);
  }
 /* Manpreet: Commented code, because now we dont require this check as we are handling validation at UBROWSER and  
  * G_MAX_CON_PER_VUSER keyword , per_svr_max_parallel cannot be greater than max_con_per_vuser*/
  if (vptr->per_svr_max_parallel > vptr->cmax_parallel)
  {
    vptr->per_svr_max_parallel = vptr->cmax_parallel;
    NSDL2_WAN(NULL, NULL, "vptr->per_svr_max_parallel is more than vptr->cmax_parallel. So setting to this value. vptr->cmax_parallel = %d, vptr->per_svr_max_parallel = %d", vptr->cmax_parallel, vptr->per_svr_max_parallel);
  }
#endif

#ifndef RMI_MODE
  vptr->cnum_parallel = 0;
  //dump_con(vptr, "init", NULL);
#endif

  if (cum_timestamp) {
    if (cum_timestamp > now)
      cum_timestamp = now;
    average_time->cum_user_ms += ((now - cum_timestamp) * (gNumVuserActive + gNumVuserThinking + gNumVuserWaiting + gNumVuserSPWaiting + gNumVuserBlocked));
  }
  cum_timestamp = now;

  // Increament of gNumVuserActive should be after above logic
  VUSER_TO_ACTIVE(vptr); //changing the state of vuser to active
  
  #ifndef CAV_MAIN
  incr_nvm_users(scen_group_num); //vusers count for each NVM 
  #else
  vptr->sm_mon_info->num_users++; 
  #endif

  vptr->modem_id = -1;
#ifdef NS_USE_MODEM
  cav_open_modem(vptr);
#endif


  if(runprof_table_shr_mem[vptr->group_num].gset.js_mode){
    /* Initialize ptr_html_doc, jscontext, clicked_url, clicked_url_len for new user */
    init_javascript_data(vptr, now);
  }

  /*************SSL**************/
  vptr->httpData->ssl_cert_id = runprof_table_shr_mem[vptr->group_num].gset.ssl_cert_id; 
  vptr->httpData->ssl_key_id = runprof_table_shr_mem[vptr->group_num].gset.ssl_key_id; 

  NSDL3_SCHEDULE(vptr, NULL, "vptr->httpData->ssl_cert_id - %d, vptr->httpData->ssl_key_id - %d",
                                        vptr->httpData->ssl_cert_id, vptr->httpData->ssl_key_id);   
 

  NSDL3_SCHEDULE(vptr, NULL, "finished with init_user_session with scenario group number = %d, user (vptr=%p)",
                       scen_group_num, vptr);
  return;
}

// TBD: can we make start_new_user and start_reuse_user same?
static void
start_new_user( VUser* vptr, u_ns_ts_t now ) {
//  connection *cptr;

  NSDL2_HTTP(vptr, NULL, "Method called. pid:%d start_new_user(): called start_new_user", getpid());

  //Setting the NS_REUSE_USER Bit to NEW USER
  //Bug#2426
  vptr->flags &= ~NS_REUSE_USER;

  CHECK_FOR_SYNC_POINT_FOR_SESSION(vptr->sess_ptr);

  if(on_new_session_start (vptr, now) == NS_SESSION_FAILURE){ 
    NSDL2_SCHEDULE(NULL, NULL, "Start new session failed from start new user"); 
    // Bug-77969
    // this code is not required as it is already handled throgh in function
    // is_new_session_blocked in ns_session.c. 
    // So, removing this code from here.
    
  }

  NSDL1_HTTP(vptr, NULL, "start_new_user(): finished with start_new_user()\n");
}

void
start_new_user_callback( ClientData cd, u_ns_ts_t now ) {
  VUser* vptr = cd.p;
  NSDL2_SCHEDULE(vptr, NULL, "Method called");

#ifdef NS_DEBUG_ON
  struct timeval tv;
  gettimeofday(&tv, NULL);
  NSDL1_SCHEDULE(vptr, NULL, "Timer expired at = %llu", 
                 (unsigned long long)((tv.tv_sec * 1000) + (tv.tv_usec / 1000)));
#endif

  if(SHOW_GRP_DATA)
    set_grp_based_counter_for_session_pacing(vptr, (now - cd.timer_started_at));

  if(vptr->vuser_state != NS_VUSER_PAUSED){
    VUSER_WAITING_TO_ACTIVE(vptr); //changing the state of vuser waiting to active
  }
  start_new_user(vptr, now);
}

inline VUser*
get_free_user_slot()
{
  NSDL3_SCHEDULE(NULL, NULL, "Method called");
#ifndef RMI_MODE
  HostSvrEntry *hptr;
  int jj;
#endif
  VUser *free, *next_free;
  //connection *cptr, *last_conn_ptr, *next_free_conn;
  //void* first_conn_ptr;
  //if (globals.debug & (globals.module_mask & FUNCTION_CALL_OUTPUT))
  NSDL1_MISC(NULL, NULL, "Entering get_free_user_slot: Free Count=%d", gFreeVuserCnt);

  if (gFreeVuserHead == NULL)
    gFreeVuserHead = allocate_user_tables(USER_CHUNK_SIZE);
  // gFreeVuserHead is allocated in child_init.c, code of that same file we are not considering.
  // So explicitly allocating it here

  free = gFreeVuserHead;
  gvptr = free;

  if (free) {
    next_free = (VUser*) free->free_next;
    free->free_next = NULL;
    gFreeVuserHead = next_free;
    gFreeVuserCnt--;
    if (gFreeVuserMinCnt > gFreeVuserCnt)
      gFreeVuserMinCnt = gFreeVuserCnt;
    // Initialize User group table for this vuser slot
    bzero ((char *)free->ugtable, user_group_table_size);
    set_uniq_vars(free->ugtable);
    // Initialize User cookie table for this vuser slot
    bzero ((char *)free->uctable, user_cookie_table_size);
    // Initailize User dynamic var table for this vuser slot
    bzero ((char *)free->udvtable, user_dynamic_vars_table_size);
#ifdef RMI_MODE
    // Initialize User byte var table for this vuser slot
    bzero ((char *)free->ubvtable, user_byte_vars_table_size);
#endif
    bzero ((char *)free->uvtable, user_var_table_size);
    // Initialize Host_Server table for this vuser slot
    bzero ((char *)free->ustable, user_svr_table_size);
    //Initiable usr_table size.
    bzero((char *)free->usr_entry, usr_table_size);

#ifndef RMI_MODE
    for (jj = 0, hptr = free->hptr; jj <= g_cur_server; jj++, hptr++) {
      hptr->num_parallel = 0;
      hptr->hurl_left = 0;
      hptr->svr_con_head = hptr->svr_con_tail = NULL;
      hptr->cur_url_head = NULL;
    }
#endif

    //free->xml_parser = NULL;

    // Initialize reuse connection slot (not currently active) link list for this vuser
    free->head_creuse = NULL;
    free->tail_creuse = NULL;
    free->head_cinuse = NULL;
    free->tail_cinuse = NULL;
    free->referer_size = 0;
    free->uniq_rangevar_ptr = NULL;
  }

  if (gBusyVuserTail) {
    gBusyVuserTail->busy_next = (struct VUser*) free;
    free->busy_prev = (struct VUser*) gBusyVuserTail;
    gBusyVuserTail = free;
  } else {
    gBusyVuserHead = free;
    gBusyVuserTail = free;
  }

  free->flags = 0;
  free->user_index = g_user_index++;
  //if (!global_settings->use_same_netid_src)
  free->ssl_mode = NS_SSL_UNINIT;

  //if (global_settings->debug && (global_settings->module_mask & FUNCTION_CALL_OUTPUT))
  NSDL3_SCHEDULE(NULL, NULL, "Exiting get_free_user_slot: Free Count=%d returns = %p", gFreeVuserCnt, free);

  return free;
}

// Neeraj - Added user_first_sess flag to indicating user is starting first session
//   1 - Means first session else 0
// This is relevant for FCU scenario as session pacing applies to FCU only
// This is added to overcome bug where
//   Before this change, scen_group_num was checked for first session (value of -1).
//   In case of group based schedule, scen_group_num is not -1. So first session was getting session pacing
//   even if session pacing for first session was off

//int new_user( int num_users, u_ns_ts_t now, int scen_group_num, int user_first_sess, UniqueRangeVarVuserTable *uniq_range_var_ptr)

int new_user( int num_users, u_ns_ts_t now, int scen_group_num, int user_first_sess, UniqueRangeVarVuserTable *uniq_range_var_ptr, SMMonSessionInfo *sm_mon_ptr)
{
  int idx;
  VUser *vptr = NULL;
  //int init_page;
  //unsigned int time_to_think;

  NSDL2_SCHEDULE(NULL, NULL, "Method called, num_users = %d, now = %llu, scen_group_num = %d, user_first_sess = %d, "
                             "uniq_range_var_ptr = %p\n", 
                              num_users, now, scen_group_num, user_first_sess, uniq_range_var_ptr);

  #ifdef CAV_MAIN
  gBusyVuserHead = sm_mon_ptr->gBusyVuserHead;
  gBusyVuserTail = sm_mon_ptr->gBusyVuserTail;
  gFreeVuserHead = sm_mon_ptr->gFreeVuserHead;
  gFreeVuserCnt = sm_mon_ptr->gFreeVuserCnt;
  gFreeVuserMinCnt = sm_mon_ptr->gFreeVuserMinCnt;
  #endif
  /* Find an empty connection slot. */
  for (idx = 0; idx < num_users; idx++ ) {
    int start_now = 1;
  
    vptr = get_free_user_slot();
    if (vptr == NULL) {
     	printf("Running short (backlog=%d) on Free user slot. need %d slots\n", gVuserBacklog, num_users - idx);
	    gVuserBacklog += num_users - idx;
	    break;
    }
    #ifdef CAV_MAIN
    vptr->sm_mon_info = sm_mon_ptr;
    vptr->sm_mon_info->gBusyVuserHead = gBusyVuserHead;
    vptr->sm_mon_info->gBusyVuserTail = gBusyVuserTail;
    vptr->sm_mon_info->gFreeVuserHead = gFreeVuserHead;
    vptr->sm_mon_info->gFreeVuserCnt = gFreeVuserCnt;
    vptr->sm_mon_info->gFreeVuserMinCnt = gFreeVuserMinCnt; 
    #endif
    TLS_SET_VPTR(vptr);
    

    init_user_session(vptr, now, scen_group_num);
    
    if(user_first_sess) //user_first_sess is 1 means user is becoming active first time so decreasing the down counts 
    {
      VUSER_DEC_DOWN(vptr);
    }
    else
    {
      VUSER_DEC_STOP(vptr);//stopped vuser count incremented on changing state to idle needs to be decremented. 
    }

    vptr->user_ip = get_src_ip(vptr, 0);
    NSDL2_SCHEDULE(NULL, NULL, "vptr = %p, vptr->group_num = %d\n", vptr, vptr->group_num);
      
    //set_user_connection_ka(vptr); /* Set value of KA */
      
    //this is to set user index in vptr in replay mode
    set_user_replay_user_idx(vptr);

    // if users is marked as ramped down do not add session pacingi,
    // when is_new_sesson_blocked is called inside on new session start it will return
    if (vptr->flags & NS_VUSER_RAMPING_DOWN) {
      /*bug 72858 trace added and called VUSER_DEC_ACTIVE*/
      NSDL2_SCHEDULE(vptr, NULL , "before VUSER_DEC_ACTIVE gNumVuserWaiting=%d ",gNumVuserWaiting);
      NSTL1(vptr, NULL, "before VUSER_DEC_ACTIVE gNumVuserWaiting=%d ",gNumVuserWaiting);
      
      VUSER_DEC_ACTIVE(vptr); /*bug 72858: reduce gNumVuserActive count*/
      
      NSDL2_SCHEDULE(vptr, NULL, "after VUSER_DEC_ACTIVE gNumVuserWaiting=%d ",gNumVuserWaiting);
      NSTL1(vptr, NULL, "after VUSER_DEC_ACTIVE gNumVuserWaiting=%d ",gNumVuserWaiting);

      user_cleanup(vptr, now); 
      continue;
    }

    start_now = 1; // Set to 1. It will be reset to 0 if pacing timer is started
    if (get_group_mode(vptr->group_num) == TC_FIX_CONCURRENT_USERS) {
      start_session_pacing_timer(vptr, NULL, user_first_sess, &start_now, now, 1);
    }
  
   NSDL1_MISC(vptr, NULL, "on new  user unique_range_var_vusertable_ptr = %p", uniq_range_var_ptr);
   if(uniq_range_var_ptr) { 
     NSDL1_MISC(vptr, NULL, "inside if unique_range_var_vusertable_ptr");
     vptr->uniq_rangevar_ptr = uniq_range_var_ptr;
   }

    if (start_now) {
      start_new_user(vptr, now);
    }
    NSDL1_MISC(vptr, NULL, "new_user:start=%d returning=%d", num_users, idx);
  }
 
  return idx;
}

void
inline_delay_callback(ClientData client_data, u_ns_ts_t now) {
 
  connection* cptr = client_data.p;
  start_new_socket(cptr, now);
}


/* Reset timer for DLE connection. This function
 * will get call only for IDLE timer for delay*/
inline void dis_idle_timer_reset_delay (u_ns_ts_t now, timer_type* tmr,u_ns_4B_t act_timeout)
{
  NSDL2_TIMER(NULL, NULL, "Method called. tmr=%p: now=%u, act_timeout = %u", tmr, now, act_timeout);
  
  if(tmr->timer_type > 0)
    dis_timer_del(tmr);
  
  tmr->actual_timeout = act_timeout;
  dis_timer_add_ex(AB_TIMEOUT_IDLE, tmr, now, inline_delay_callback, tmr->client_data, 0, 0); // TODO
  NSDL3_TIMER(NULL, NULL, "STS:reset tmr=%p Done: \n", tmr);
}

//Get an URL on the connection. Make a new connection, if not reuse.
inline void
renew_connection(connection* cptr, u_ns_ts_t now )
{
  NSDL2_CONN(NULL, cptr, "Method called, cptr=%p", cptr);

  VUser *vptr = cptr->vptr;
  //if (cptr->not_ready) return;
  cptr->num_retries = 0;

/* Replace by start_new_socket on Feb 16th, 2011 for cleanup and to make sure on_url_start is called before start_socket
  if ( cptr->conn_state == CNST_FREE ) {
    start_new_socket(cptr, now );
  } else if ( cptr->conn_state == CNST_REUSE_CON ) {
    start_socket(cptr, now );
    // printf("Second Calling function on_url_start\n");
    on_url_start(cptr);
    //(average_time->fetches_started)++;
  }
*/
  // Inline url delay and connection reuse delay
  // Case1 - schedule time is not set (it is 0) - Apply timer if min con resue is given (1a - No timer, 1b - timer)
  // Case2 - now >= schedule time - Apply timer if min con resue is given (2a - No timer, 2b - timer)
  // Case3 - now <= schedule time - Always apply timer
  if(cptr->url_num->proto.http.type == EMBEDDED_URL) { // Coming here for main page also
    u_ns_ts_t schedule_time = cptr->url_num->schedule_time;
    // NSDL3_HTTP(vptr, cptr, "schedule_time = [%d]", cptr->url_num->schedule_time);
    int con_reuse_delay = 0, min_con_reuse_delay;
    // get timer value if schedule time is not 0
    if(schedule_time) // Only for repeat or delay case, it is non zero
      con_reuse_delay = schedule_time - now; // This can be -ve also
    NSDL3_HTTP(vptr, cptr, "con_reuse_delay = [%d]", con_reuse_delay);

    // get min_con_resuse_delay as per G_INLINE_MIN_CON_REUSE_DELAY 
    calculate_con_reuse_delay(vptr, &min_con_reuse_delay);

    if(con_reuse_delay < min_con_reuse_delay) { 
      NSDL3_HTTP(vptr, cptr, "con_reuse_delay < min_con_reuse_delay, so setting con_reuse_delay = min_con_reuse_delay", con_reuse_delay);
      if(global_settings->page_based_stat == PAGE_BASED_STAT_ENABLED) 
        set_page_based_counter_for_conn_reuse_delay(vptr, min_con_reuse_delay - con_reuse_delay); 
      con_reuse_delay = min_con_reuse_delay;
    }
  
    if(con_reuse_delay > 0) {
      // Check if keep alive timer is running on this connection, if its running then first check if keep alive time is less than schedule   
      // time then close this fd and set schedule time  
      if(cptr->timer_ptr->timer_type == AB_TIMEOUT_KA)
      {
        NSDL3_HTTP(vptr, cptr, "cptr->timer_ptr->actual_timeout = %d, con_reuse_delay = %d", cptr->timer_ptr->actual_timeout, con_reuse_delay);
        if(cptr->timer_ptr->timeout < schedule_time)  // If keep alive is set less then schedule, we should close it
          close_fd(cptr, NS_FD_CLOSE_REMOVE_RESP, now); // Timer will be deleted inside close fd 
        else{ 
          NSDL3_HTTP(vptr, cptr, "Going to delete keep alive timer as this connection will be used by scheduled URL");
          dis_timer_del(cptr->timer_ptr);
        }
      }        
      cptr->timer_ptr->client_data.p = cptr;
      dis_idle_timer_reset_delay (now, cptr->timer_ptr, con_reuse_delay);
      NS_DT2(vptr, cptr, DM_L1, MM_HTTP, "Got delay, Adding timer for inline url execution, now = [%llu], schedule_time = [%llu], "
             "min_con_reuse_delay = [%d], con_reuse_delay = [%d]", now, schedule_time, min_con_reuse_delay, con_reuse_delay); 
      NSDL3_HTTP(vptr, cptr, "Got delay, Adding timer for inline url execution, now = [%llu], schedule_time = [%llu], "
                 "min_con_reuse_delay = [%d], con_reuse_delay = [%d]", now, schedule_time, min_con_reuse_delay, con_reuse_delay); 
      cptr->url_num->schedule_time = 0; // Must reset?
      return;
    }
  } 
  start_new_socket(cptr, now );
}

/*bug 40306, 68086 */
/*return if max open concurrent stream count reached, instead of calling try_hurl_on_any_con and opening new tcp connection */
#define RETURN_IF_MAX_CONCURRENT_STREAM_REACHED(cptr) \
if(cptr->http_protocol == HTTP_MODE_HTTP2) \
{ \
    if(cptr->http2 && (cptr->http2->total_open_streams ==  \
            cptr->http2->settings_frame.settings_max_concurrent_streams)){ \
        NSDL2_CONN(vptr, cptr, " total_open_streams reached max_concurrent_stream"); \
        return; \
    } \
}
//used to make non-first url request of a page
void
next_connection( VUser *vptr, connection* cptr, u_ns_ts_t now )
{
  int cmax_parallel;//, per_svr_max_parallel;
  HostSvrEntry* host_head;
  NSDL2_CONN(vptr, cptr, "Method Called, cptr=%p and urls left is %d and urls->awaited is %d, type = %d",
			 cptr, vptr->urls_left, vptr->urls_awaited, cptr?cptr->url_num->proto.http.type:-1);
 
  if (cptr == NULL) {
    if (!(vptr->urls_left)) {
      NSDL2_CONN(vptr, cptr, "Returning as urls_left is 0.");
      return;
    }
  } else {
    if(cptr->url_num->proto.http.type == MAIN_URL) 
    vptr->pg_main_url_end_ts = now; // Used in reporing of inline block time, in case delay is not present 
  NSDL2_CONN(vptr, cptr, "vptr->pg_main_url_end_ts= %u, cptr->url_num->proto.http.url_index = %u", vptr->pg_main_url_end_ts, cptr->url_num->proto.http.url_index);
         
    if(!(cptr->http_protocol == HTTP_MODE_HTTP2 && cptr->http2->total_open_streams))
      assert(cptr->conn_state == CNST_REUSE_CON);
    if (!(vptr->urls_left)) {
      if(!(cptr->http_protocol == HTTP_MODE_HTTP2 && cptr->http2->total_open_streams))
        add_to_reuse_list(cptr);
      return;
    }
    try_hurl_on_cur_con (cptr, now);
    /*bug 40306, 68086 */
    /*return if concurrent open stream count reached max count*/
    /*for http2, its not required to call try_hurl_on_any_con, as http2 BR says to use only one cptr for connection one host*/
    RETURN_IF_MAX_CONCURRENT_STREAM_REACHED(cptr)
  }
  //per_svr_max_parallel = vptr->per_svr_max_parallel;
  cmax_parallel = vptr->cmax_parallel;

  NSDL2_CONN(vptr, cptr, "cnum_parallel = %d, cmax_parallel = %d, head_creuse = [%p]",
                          vptr->cnum_parallel, cmax_parallel, vptr->head_creuse);

  for (host_head = vptr->head_hlist; (host_head && vptr->urls_left &&
			       ((vptr->cnum_parallel < cmax_parallel) || vptr->head_creuse)) ; ) {
    NSDL2_CONN(vptr, cptr, "host_head = %p", host_head);
    //  3.9.2 – Added code to reset now as req can take time specifically if DNS lookup is done …
    //  so next request start time need to change accordingly
    now = get_ms_stamp();
    if (try_hurl_on_any_con(vptr, host_head, now)) {
      host_head = next_from_hlist(vptr, host_head);
    } else { //All URLS of this host executed
      host_head = next_remove_from_hlist(vptr, host_head);
    }

    //printf("doing connections %d, num_connections is %d, vptr->cnum_parallel is %d, cmax_parallel is %d, host_head is 0x%x, vptr->urls_left is %d, vptr->head_creuse is 0x%x\n", i, num_connections, vptr->cnum_parallel, cmax_parallel, host_head, vptr->urls_left, vptr->head_creuse);
  }
}

//used to make second (redirect) url request of a page,
void
redirect_connection( VUser *vptr, connection* cptr, u_ns_ts_t now , action_request_Shr* last_url)
{
  action_request_Shr* url_num;

  NSDL2_CONN(vptr, cptr, "Method called, cptr=%p and urls left is %d and urls->awaited is %d",
                         cptr, vptr->urls_left, vptr->urls_awaited);

  url_num = last_url +1;

  //we must execute first URL anyhow
  if (cptr) {
    if  (!try_url_on_cur_con (cptr, url_num, now)) {
      if (!try_url_on_any_con (vptr, url_num, now, NS_DO_NOT_HONOR_REQUEST)) {
	printf("redirect_connection:Unable to run Main URL\n");
	end_test_run();
      }
    }
  } else {
    if (!try_url_on_any_con (vptr, url_num, now, NS_DO_NOT_HONOR_REQUEST)) {
      printf("redirect_connection:Unable to run Main URL\n");
      end_test_run();
    }
  }
}



static inline void
start_reuse_user ( VUser * vptr, connection* cptr, u_ns_ts_t now )
{
  NSDL1_SCHEDULE(vptr, cptr, "Method called: pid:%d", getpid());
  //Bug#2426
  vptr->flags |= NS_REUSE_USER;
  vptr->last_cptr = cptr;

  CHECK_FOR_SYNC_POINT_FOR_SESSION(vptr->sess_ptr);
  if(on_new_session_start (vptr, now) == NS_SESSION_FAILURE) 
  {
    NSDL1_SCHEDULE(vptr, cptr, "Start new session failed on resuing user.");
    // Bug-77969
    // this code is not required as it is already handled throgh in function
    // is_new_session_blocked in ns_session.c. 
    // So, removing this code from here.
  }
}


void
start_reuse_user_callback( ClientData cd, u_ns_ts_t now ) {
  VUser* vptr = cd.p;

  NSDL2_SCHEDULE(vptr, NULL, "Method called session status - %d", vptr->sess_status);

  if(vptr->vuser_state != NS_VUSER_PAUSED){
    VUSER_WAITING_TO_ACTIVE(vptr); //changing the state of vuser waiting to active
  }

  if(vptr->sess_status != NS_SESSION_ABORT){
    if(SHOW_GRP_DATA)
      set_grp_based_counter_for_session_pacing(vptr, (now - cd.timer_started_at));
  }

  start_reuse_user(vptr, NULL, now);
}

/* Common function that is called from user_cleanup() and reuse_user()
*  This function is used to clear the vars table 
*/

static void 
clear_var_table(VUser *vptr)
{
  int i,j;
  int max_idx;

  NSDL1_VARS(vptr, NULL, "Method called  to cleared the var table");

  // Initialize User dynamic vars table for this vuser slot
  //KQ: Dynamic var is same as tag, or search var or old html tags
  for (i = 0; i < max_dynvar_hash_code; i++) {
    FREE_AND_MAKE_NOT_NULL(vptr->udvtable[i].dynvar_value, "Dynamic vars", i);
  }
  bzero ((char *)vptr->udvtable, user_dynamic_vars_table_size);

#ifdef RMI_MODE
  // Initialize User byte vars table for this vuser slot
  for (i = 0; i < max_bytevar_hash_code; i++) {
    FREE_AND_MAKE_NOT_NULL(vptr->udvtable[i].bytevar_value, "Byte vars", i);
  }
  bzero ((char *)vptr->ubvtable, user_byte_vars_table_size);
#endif

  // in case of jmeter script we allocate one vptr for message_comm_com. 
  // This is not a virtural user ptr we are using it for other purpose.
  // so we will have a check for vptr->sess_ptr not NULL so no core dump will come.
  if (vptr->sess_ptr != NULL)
  {
    max_idx = vptr->sess_ptr->numUniqVars;
    NSDL1_VARS(vptr, NULL, "Clear all malloc memory in vptr->uvtable, num_vars = %d", max_idx);
    for (i = 0; i < max_idx; i++) {
     // Check if for a vuser retain previous value is set in API then, do not clear vuser value 
     int var_trans_table_idx = vptr->sess_ptr->vars_rev_trans_table_shr_mem[i];
     NSDL1_VARS(vptr, NULL, "numUniqVars %d var_trans_table_idx = %d", max_idx, var_trans_table_idx);
     
     if((runprof_table_shr_mem[vptr->group_num].pacing_ptr->refresh != REFRESH_USER_ON_NEW_SESSION) &&
        (vptr->sess_ptr->vars_trans_table_shr_mem[var_trans_table_idx].retain_pre_value == RETAIN_PRE_VALUE))
       
     {
       if(vptr->sess_ptr->vars_trans_table_shr_mem[var_trans_table_idx].var_type == VAR)
       {  
         int group_idx = vptr->sess_ptr->vars_trans_table_shr_mem[var_trans_table_idx].fparam_grp_idx; 
         if(group_table_shr_mem[group_idx].type != ONCE)
         {
           // If Refresh type is SESSION OR USE then filled flag must be reset to get the next value on next session
           vptr->uvtable[i].filled_flag = 0;
           NSDL1_VARS(vptr, NULL, "Retain previous value for this user is ON in API.");
         }
       }
       NSDL1_VARS(vptr, NULL, "Retain previous value for this user is ON in API.");
       continue;
     }
   
     if (vptr->sess_ptr->var_type_table_shr_mem[i] == 1) {  /* is an array */
        if (vptr->uvtable[i].value.array) {
          for (j = 0; j < vptr->uvtable[i].length; j++) {
            FREE_AND_MAKE_NOT_NULL(vptr->uvtable[i].value.array[j].value, "vptr->uvtable[i].value.array[j].value", j)
          }
          FREE_AND_MAKE_NOT_NULL(vptr->uvtable[i].value.array, "vptr->uvtable[i].value.array", i);
        }
      }
      else{
        if((vptr->sess_ptr->var_type_table_shr_mem[i] == 2) && (vptr->uvtable[i].value.value != NULL)) {
          FREE_AND_MAKE_NOT_NULL(((PointerTableEntry_Shr *)vptr->uvtable[i].value.value)->big_buf_pointer, "vptr->uvtable[i].value.value", i);
        }
        if(vptr->sess_ptr->var_type_table_shr_mem[i] != 3)
          FREE_AND_MAKE_NOT_NULL(vptr->uvtable[i].value.value, "vptr->uvtable[i].value.value", i);
      }
      memset(&(vptr->uvtable[i]), 0, sizeof(UserVarEntry));
    }
  }

  //TODO: To free allocated memory for file param.   
  //bzero((char *)vptr->uvtable, user_var_table_size);
 
  // Added in 3.9.2 for use dns caching mode
  // Set sin6_port to 0 to indicate that is address is not longer valid for reuse
  // as caching is for session only
  if(vptr->usr_entry) {
    for (i = 0; i < total_svr_entries; i++) {
      NSDL1_VARS(vptr, NULL, "Set sin6_port to 0 for index = %d", i);
      vptr->usr_entry[i].resolve_saddr.sin6_port = 0;
    }
  }
  NSDL1_VARS(vptr, NULL, "cleared the user var table");

}


void reuse_user( VUser *vptr, connection* cptr, u_ns_ts_t now )
{
  //unsigned int time_to_think;
  //unique_group_table_type* unique_group_ptr = unique_group_table;
  //ClientData cd;
  int start_now = 1;

  NSDL3_SCHEDULE(vptr, cptr, "Method called, pid:%d reuse_user:cptr=%p", getpid(), cptr);

  // Since we are reusing VUser slot, let us inititialize link lists in this slot
  // This is usally done in get_free_user_slot
  // Initialize Host_Server table for this vuser slot

  /*Changing state from blocked to active else incrementing active as state is already active*/
  if(global_settings->concurrent_session_mode && vptr->sess_status != NS_SESSION_ABORT) {
    VUSER_BLOCKED_TO_ACTIVE(vptr);
  }
  else {
    VUSER_INC_ACTIVE(vptr);
  }

  //reset the custom headers for ns_web_add_header() and ns_web_add_auto_header() API
  if(vptr->httpData != NULL && vptr->httpData->usr_hdr != NULL)
  {
    reset_all_api_headers(vptr);
  }
  /* we should inrease nvm users only from init user session()
  incr_nvm_users(my_port_index, group_num);
  */
  
  // Initialize User static var group table for this vuser slot
  free_uniq_var_if_any (vptr);

  NSDL2_VARS(vptr, cptr, "group_num = %d, retain_param_value = %d retain_cookie_value = %d", 
                   vptr->group_num, runprof_table_shr_mem[vptr->group_num].pacing_ptr->retain_param_value, 
                   runprof_table_shr_mem[vptr->group_num].pacing_ptr->retain_cookie_val);

  /* Flushing value of vuser here in below condition(s)
     1. retain_param_value is set to default (retain_param_value == 0).
     2. Session fails (due to 4xx, 5xx , or CV fail (etc)).
     
  */
  if ((!runprof_table_shr_mem[vptr->group_num].pacing_ptr->retain_param_value)  || 
     (runprof_table_shr_mem[vptr->group_num].pacing_ptr->retain_param_value == 2 && vptr->sess_status != NS_REQUEST_OK))
  { 
    // Going to clean file Parameter 
    NSDL2_VARS(vptr, cptr, "b4 clearing the user group table, user_group_table is %d", user_group_table_size);
    // This is done to prevent ugtable clearing in case of Refresh=ONCE - FILEPARAMETER
    for(int i=0; i<total_group_entries; i++)
    {
      if((group_table_shr_mem[i].type != ONCE))
        memset((char*)&vptr->ugtable[i], 0, sizeof(UserGroupEntry));
    } 

    // bzero ((char *)vptr->ugtable, user_group_table_size);
    // Going to clear Cookie and user parameter value
    NSDL2_VARS(vptr, cptr, "Going to clear var table");

    clear_var_table(vptr);
  }

  // In case if retain_cookie_value is 0 then not retain value of cookie 
  if ((!runprof_table_shr_mem[vptr->group_num].pacing_ptr->retain_cookie_val) || 
     (runprof_table_shr_mem[vptr->group_num].pacing_ptr->retain_cookie_val == 2 && vptr->sess_status != NS_REQUEST_OK))
  {
    NSDL2_VARS(vptr, cptr, "Going to clear cookie as retain_cookie_val = %d", 
    runprof_table_shr_mem[vptr->group_num].pacing_ptr->retain_cookie_val);
    free_cookie_value(vptr);
  }
 
  /* Value(s) of certain ns variables like  unique var , date var and random number will not be retained in any cases.
      We will clear these value of these variables forcefully */
  if (runprof_table_shr_mem[vptr->group_num].pacing_ptr->retain_param_value) {
    NSDL2_VARS(vptr, NULL, "Going to Flush values for random var, date var and random string");
    // check if using random var 
    if (randomvar_table_shr_mem)
      clear_uvtable_for_random_var(vptr);
    // Check for random string 
    if (randomstring_table_shr_mem)
      clear_uvtable_for_random_string_var(vptr);
    // check for unique var 
    if (uniquevar_table_shr_mem)
      clear_uvtable_for_unique_var(vptr);
    // Check for date var
    if (datevar_table_shr_mem)
      clear_uvtable_for_date_var(vptr);
  }

  set_uniq_vars(vptr->ugtable);

 /*
 This must be reset by zero for every session
 */
  vptr->redirect_count = 0; //TODO --bhushan

  // Commented by Neeraj on Oct 17, 07 as cookie should not be freed here
#if 0
  // Initialize User cookie table for this vuser slot
  for (i = 0; i < max_cookie_hash_code; i++) {  /* We need to free up cookie values, since the last user may have had some */
    if (vptr->uctable[i].cookie_value)
      free(vptr->uctable[i].cookie_value);
  }
  bzero ((char *)vptr->uctable, user_cookie_table_size);
#endif

  //Reset server entry index
  vptr->server_entry_idx = -1;

  // Initailize User server table for this vuser slot
  bzero((char *)vptr->ustable, user_svr_table_size);

#if 0
  if(global_settings->protocol_enabled & TR069_PROTOCOL_ENABLED) {
    if(vptr->httpData->flags & NS_TR069_REMAP_SERVER) {
      char* url_ptr = tr069_is_cpe_new(vptr);
      if(url_ptr) {
        tr069_switch_acs_to_acs_main_url(vptr, url_ptr);
      }
      vptr->httpData->flags &= ~NS_TR069_REMAP_SERVER;
    }
  }
#endif

  if (vptr->svr_map_change) {
    FREE_AND_MAKE_NULL(vptr->svr_map_change->var_name, "vptr->svr_map_change->var_name", -1);
    FREE_AND_MAKE_NULL(vptr->svr_map_change, "vptr->svr_map_change", -1);
  }

  if(runprof_table_shr_mem[vptr->group_num].gset.js_mode != NS_JS_DISABLE)
  {
    free_javascript_data(vptr, now); /* Free JS context, DOM tree and url string if any */
  }


  start_session_pacing_timer(vptr, cptr, 0, &start_now, now, 0); // Last 0 for not new user

  NSDL2_SCHEDULE(vptr, cptr, "cleared all the tables, start_now = %d", start_now);
  if (start_now) {
    start_reuse_user(vptr, cptr, now);
  }
  else  // In case of pacing timer, we need to add cptr in resue list/
    // In RBU and JRMI cptr is dummy 
    if(cptr && (!runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_rbu) && !(vptr->flags & NS_JNVM_JRMI_RESP)) 
      add_to_reuse_list(cptr);
}

/* Function used to close connection entry present in reuse list. 
 * */
static inline void close_reuse_connections(connection *cptr, VUser *vptr, u_ns_ts_t now)
{
  /* In below code if connection state is REUSE, we need to close connection
   * Delete timer
   * For doing above, local variable(cptr_next) is used to save next cptr in list bec in close_fd
   * we remove cptr from link list which breaks the list. 
   * */
  connection *cptr_next = NULL; 
  NSDL1_CONN(vptr, NULL, "Method called"); 

  for (; cptr != NULL; )
  {
    cptr_next = (connection *)cptr->next_inuse;//local var to save next cptr in link list
    NSDL2_CONN(vptr, NULL, "cptr=%p, cptr->conn_state = %d, cptr->next_inuse = %p, conn_fd = %d", cptr, cptr->conn_state, (connection *)cptr->next_inuse, cptr->conn_fd);

    if (cptr->conn_state == CNST_REUSE_CON) // Just check. We can remove this later
    {
      NSDL2_CONN(vptr, cptr, "Closing Connection");
      // CHECK cptr->conn_state = CNST_FREE;
      if ( cptr->timer_ptr->timer_type >= 0 ) {
        dis_timer_del(cptr->timer_ptr);
      }
      close_fd_and_release_cptr(cptr, NS_FD_CLOSE_REMOVE_RESP, now); //close_fd reset connection state to CNST_FREE 
    }
    else //Code should never come here
      print_core_events((char*)__FUNCTION__, __FILE__, "Connection is neither reuse nor active");

    cptr = cptr_next; //Update cptr with next cptr in list
  }
  NSDL1_CONN(vptr, NULL, "Method end"); 
}


/* Function used to close connection entry within inuse list
 * */
static inline void close_inuse_connections(connection *cptr, VUser *vptr, u_ns_ts_t now)
{
  /* In below code if connection state is inuse, we need to close connection
   * For doing above, local variable(cptr_next) is used to save next cptr in list bec in close fd
   * we remove cptr which breaks the link list. 
   * */
  connection *cptr_next = NULL; 
  NSDL1_CONN(vptr, NULL, "Method called");

  for (; cptr != NULL; )
  {
    cptr_next = (connection *)cptr->next_inuse;//local var to save next cptr in link list
    NSDL2_CONN(vptr, NULL, "cptr=%p, cptr->conn_state = %d, cptr->next_inuse = %p", cptr, cptr->conn_state, (connection *)cptr->next_inuse);
     /* Changes done for Bug#4989: In case if NS_VUSER_RAMPING_DOWN flag is set on vptr and connection state
      * is other than FREE or REUSE then we need to stop connection/URL with stopped status
      * Here in inuse list we expect connection can be in either state FREE or REUSE, which changed last
      * URL with stopped status
      * */ 
    if((vptr->flags & NS_VUSER_RAMPING_DOWN) && (cptr->conn_state != CNST_FREE) && (cptr->conn_state != CNST_REUSE_CON) && (cptr->request_type != JRMI_REQUEST))//Here we verify if connection other than FREE and REUSE need to be close
    {
      NSDL2_CONN(vptr, cptr, "Ramp Down - Stopping Connection/URL, conn_state = %d, conn_fd = %d req_ok=%d  ", cptr->conn_state, cptr->conn_fd,cptr->req_ok);
      if(cptr->http_protocol != HTTP_MODE_HTTP2) 
        cptr->req_ok = NS_REQUEST_STOPPED;
      // close this URL for reporting
      // CHECK cptr->conn_state = CNST_FREE;
      if ( cptr->timer_ptr->timer_type >= 0 )
        dis_timer_del(cptr->timer_ptr);
      ramp_down_close_connection(vptr, cptr, now);
    }
    else
    {
      if(cptr->conn_state != CNST_FREE) //Changes done for Bug#5087, Close all states other than FREE, if connetcion state is FREE then we need to call release_cptr 
      {
        NSDL2_CONN(vptr, cptr, "Connection state other than free so closing connection conn_state = %d, conn_fd = %d", cptr->conn_state, cptr->conn_fd);
        if ( cptr->timer_ptr->timer_type >= 0 )
          dis_timer_del(cptr->timer_ptr);
        close_fd_and_release_cptr(cptr, NS_FD_CLOSE_REMOVE_RESP, now);
      }
      else {
        NSDL2_CONN(vptr, cptr, "Connection state is free hence releasing connections conn_state = %d, conn_fd = %d", cptr->conn_state, cptr->conn_fd);
        update_parallel_connection_counters(cptr, now);
        /* free_connection_slot remove connection from either or both reuse or inuse link list*/
        NSDL1_CONN(vptr, cptr, "Need to call free_connection_slot");
        free_connection_slot(cptr, now);
      }
    }

    cptr = cptr_next; //Update cptr with next cptr in list
  }
}
void xmpp_free_user_info(VUser *vptr)
{

  NSDL2_XMPP(vptr, NULL, "Method Called");
  if(!vptr->xmpp)
    return;

  if(vptr->xmpp->file_url)
    FREE_AND_MAKE_NULL_EX(vptr->xmpp->file_url, vptr->xmpp->file_url_size ,"vptr->xmpp->file_url" , -1);

  if(vptr->xmpp->uid)
    FREE_AND_MAKE_NULL_EX(vptr->xmpp->uid, vptr->xmpp->uid_size, "vptr->xmpp->uid_size" , -1);

  if(vptr->xmpp->partial_buf)
    FREE_AND_MAKE_NULL_EX(vptr->xmpp->partial_buf, vptr->xmpp->partial_buf_size,"vptr->xmpp->partial_buf_size" , -1);

  FREE_AND_MAKE_NULL_EX(vptr->xmpp, sizeof(nsXmppInfo),"vptr->xmpp_uid" , -1);

}

void user_cleanup( VUser *vptr, u_ns_ts_t now) 
{
  Schedule *schedule_ptr;
  int phase_id;
  Ramp_down_schedule_phase *ramp_down_phase_ptr;
  //unique_group_table_type* unique_group_ptr = unique_group_table;
  NSDL1_SCHEDULE(vptr, NULL, "Method called, vptr=%p, vptr->flags = %d, gNumVuserCleanup = %d",
                             vptr, vptr->flags ,gNumVuserCleanup);
  if (vptr->timer_ptr->timer_type > 0)
    dis_timer_del(vptr->timer_ptr); 
  /* In connection pool design, connections can be available in either or both 
   * lists(reuse list, inuse list).
   * Here we are sending head node of each list, and performing connection cleanup
   * for cptr*/
  if(vptr->head_creuse)
    close_reuse_connections(vptr->head_creuse, vptr, now);

  if(vptr->head_cinuse)
    close_inuse_connections(vptr->head_cinuse, vptr, now);
  
#ifdef ENABLE_SSL
    ssl_sess_free(vptr);
#endif
  
  free_uniq_var_if_any (vptr);
  /*for (j = 0; j < unique_group_id; j++, unique_group_ptr++) {
    if (vptr->ugtable[unique_group_ptr->group_table_id].cur_val_idx != -1)
      free_unique_val(unique_group_ptr, vptr->ugtable[unique_group_ptr->group_table_id].cur_val_idx);
  }*/

  free_cookie_value(vptr);
  clear_var_table(vptr);

  NSDL1_SCHEDULE(vptr, NULL, "vptr->flags = %x, vptr->uniq_rangevar_ptr = %p", vptr->flags,vptr->uniq_rangevar_ptr);
  if((vptr->flags & NS_VUSER_RAMPING_DOWN) && vptr->uniq_rangevar_ptr)
    FREE_AND_MAKE_NULL_EX(vptr->uniq_rangevar_ptr, total_unique_rangevar_entries * sizeof(UniqueRangeVarVuserTable), 
                          "unique range var vuser table", -1);
 
  //if((vptr->flags & NS_VUSER_RAMPING_DOWN) != NS_VUSER_RAMPING_DOWN && vptr->vuser_state == NS_VUSER_CLEANUP)  
  if(vptr->vuser_state == NS_VUSER_CLEANUP)  
    VUSER_FROM_CLEANUP(vptr); //changing the state of vuser from cleanup

  if(NS_IF_TRACING_ENABLE_FOR_USER){
    ut_vuser_check_and_disable_tracing(vptr);
  }

  if(runprof_table_shr_mem[vptr->group_num].gset.js_mode != NS_JS_DISABLE)
  {
    free_javascript_data(vptr, now); // For dom and jscontext cleanup
    destroy_js_runtime_per_user(vptr, now);
  }

  if(global_settings->schedule_by == SCHEDULE_BY_SCENARIO)
    schedule_ptr = v_port_entry->scenario_schedule;
  else
    schedule_ptr = &(v_port_entry->group_schedule[vptr->group_num]);

  //Here removing ramped_down users as FP data exhausted
  //BUG 67892: we not need to modify ramped_down_vusers, max_ramp_down_vuser_or_sess, cur_vusers_or_sess counters in case of 
  //QUNATITY RTC.Below case is for USEONCE_ABORT only
  if ((vptr->page_status == NS_USEONCE_ABORT) && (vptr->flags & NS_VUSER_RAMPING_DOWN) && (get_group_mode(-1) == TC_FIX_CONCURRENT_USERS) &&
                      (schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type != SCHEDULE_PHASE_RAMP_DOWN))
  {
    //phase complete should not be sent from here when phase is ramp down
    for(phase_id = schedule_ptr->phase_idx; phase_id < schedule_ptr->num_phases; phase_id++)
    {
      if(schedule_ptr->phase_array[phase_id].phase_type == SCHEDULE_PHASE_RAMP_DOWN)
      {
        ramp_down_phase_ptr = &schedule_ptr->phase_array[phase_id].phase_cmd.ramp_down_phase;
        NSDL2_SCHEDULE(vptr, NULL, "grp = %d, ramped_user = %d, phase_id = %d, cur_vuser = %d, max_ramp_down_vuser_or sess = %d", 
                                    vptr->group_num, ramp_down_phase_ptr->ramped_down_vusers, phase_id, 
                                    schedule_ptr->cur_vusers_or_sess, ramp_down_phase_ptr->max_ramp_down_vuser_or_sess);

        if(global_settings->schedule_by == SCHEDULE_BY_SCENARIO) {
          NSDL2_SCHEDULE(vptr, NULL, "grp_ramped_down_users = %d", ramp_down_phase_ptr->per_grp_qty[vptr->group_num]);
          ramp_down_phase_ptr->per_grp_qty[vptr->group_num]--;
        }
        ramp_down_phase_ptr->ramped_down_vusers++;
        ramp_down_phase_ptr->max_ramp_down_vuser_or_sess += ramp_down_phase_ptr->ramped_down_vusers;
        schedule_ptr->cur_vusers_or_sess--;
        //if(ramp_down_phase_ptr->ramped_down_vusers == schedule_ptr->cur_vusers_or_sess)
        if(!schedule_ptr->cur_vusers_or_sess)
        {
          NSDL2_VARS(vptr, NULL, "Send Phase Complete");
          send_phase_complete(schedule_ptr);
        }
      }
    } //end of for
  } // end of if

  if(vptr->httpData)
  {
    NSDL1_SCHEDULE(vptr, NULL, "httpData exists");
     
    if(vptr->httpData->ua_handler_ptr != NULL)
    {
      NSDL1_SCHEDULE(vptr, NULL, "vptr->httpData->ua_handler_ptr exists");
      if(vptr->httpData->ua_handler_ptr->ua_string != NULL)
      {
        NSDL1_SCHEDULE(vptr, NULL, "vptr->httpData->ua_handler_ptr->ua_string exists");
        FREE_AND_MAKE_NULL_EX(vptr->httpData->ua_handler_ptr->ua_string, vptr->httpData->ua_handler_ptr->malloced_len, "UA String", -1);
        vptr->httpData->ua_handler_ptr->ua_len = 0;
        vptr->httpData->ua_handler_ptr->malloced_len = 0;
      }
      FREE_AND_MAKE_NULL_EX(vptr->httpData->ua_handler_ptr, sizeof(UA_handler), "UA Handler structure", -1);
    } 

    /* RBU - freeing memory malloced(by json parser) for response body */
    if(vptr->httpData->rbu_resp_attr != NULL)
    {
      ns_rbu_user_cleanup(vptr);
    }

    //freeing memory malloced for ns_web_add_header() and ns_web_add_auto_header() API
    if(vptr->httpData->usr_hdr != NULL)
    {
      delete_all_api_headers(vptr);
    }
    
    //Free websocket session members
    if(vptr->httpData->ws_uri_last_part)
    {
      //+128 because at the time om allocation we malloced 128 bytes extra then ws_uri_last_part_len
      FREE_AND_MAKE_NULL_EX(vptr->httpData->ws_uri_last_part, vptr->httpData->ws_uri_last_part_len + 128, "vptr->httpData->ws_uri_last_part", -1);
    }
    if(vptr->httpData->ws_client_base64_encoded_key)
    {
      FREE_AND_MAKE_NULL_EX(vptr->httpData->ws_client_base64_encoded_key, 40, "vptr->httpData->ws_client_base64_encoded_key", -1);
    }
    if(vptr->httpData->ws_expected_srever_base64_encoded_key)
    {
      FREE_AND_MAKE_NULL_EX(vptr->httpData->ws_expected_srever_base64_encoded_key, 1024, "vptr->httpData->ws_expected_srever_base64_encoded_key", -1);
    }
    
    //MONGODB - destroy all mongo obkects
    if(vptr->httpData->mongodb)
    {
      ns_mongodb_client_cleanup(vptr);
    }

    if(vptr->httpData->jmeter)
      ns_jmeter_user_cleanup(vptr);
   
    //CASSANDRA clean up
    if (vptr->httpData->cassdb)
    {
      ns_cassdb_free_cluster_and_session(vptr);
    }
  }
  if(vptr->flags & NS_XMPP_ENABLE) 
  {
    xmpp_free_user_info(vptr);
    vptr->flags &= ~NS_XMPP_ENABLE; 
  }
  /*Bug Id 71023: Freeing vptr->response_hdr here as it is accessed from ns_url_get_body_msg*/ 
  if(vptr->response_hdr)
  {
    FREE_AND_MAKE_NULL(vptr->response_hdr->hdr_buffer, "vptr->response_hdr->hdr_buffer", -1);
    FREE_AND_MAKE_NULL(vptr->response_hdr, "vptr->response_hdr", -1);
  }
  free_user_slot(vptr, now);
  #ifdef CAV_MAIN
  // In CAVMAIN we are creating our own Vuser list and their count.
  // If total number of  Vusers of a particular mon request becomes 0
  // mark that monitor request as COMPLETED
  vptr->sm_mon_info->num_users--;
  vptr->sm_mon_info->gBusyVuserHead = gBusyVuserHead;
  vptr->sm_mon_info->gBusyVuserTail = gBusyVuserTail;
  vptr->sm_mon_info->gFreeVuserHead = gFreeVuserHead;
  vptr->sm_mon_info->gFreeVuserCnt = gFreeVuserCnt;
  vptr->sm_mon_info->gFreeVuserMinCnt = gFreeVuserMinCnt; 
  if(!(vptr->sm_mon_info->num_users))
  {
     int opcode;
     if(vptr->sm_mon_info->status == SM_STOP)    
        opcode = NS_TEST_STOPPED;
     else if(vptr->sm_mon_info->status == SM_ERROR)
        opcode = NS_TEST_ERROR;
     else
        opcode = NS_TEST_COMPLETED;
     // Stop monitor
     sm_stop_monitor(opcode, vptr->sm_mon_info);
     vptr->sm_mon_info = NULL;
     global_settings = p_global_settings;
     group_default_settings = p_group_default_settings;
   }  
  #endif
}

void user_cleanup_timer( ClientData client_data, u_ns_ts_t now ) {
  VUser *vptr;
  //connection* cptr;

  /* see commend in function: chk_for_cleanup() */
  /*
  cptr = client_data.p;
  vptr = cptr->vptr;
  */

  vptr = client_data.p;

  NSDL1_SCHEDULE(vptr, NULL, "Method called");
  user_cleanup(vptr, now);
}

