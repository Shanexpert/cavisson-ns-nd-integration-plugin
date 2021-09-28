/******************************************************************************************************
 * Name                :  ns_cavmain_child.c
 * Purpose             :  Process SM Request of parent
 * Author              :  Devendar Jain/Sharad Jain
 * Intial version date :  10/09/2020
 * Last modification date:
*******************************************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <fcntl.h>

#include "nslb_util.h"
#include "nslb_alloc.h"
#include "nslb_thread_queue.h"

#include "ns_tls_utils.h"

#include "ns_common.h"
#include "ns_global_settings.h"
#include "ns_trace_level.h"
#include "nslb_map.h"
#include "ns_log.h"
#include "nslb_sock.h"
#include "signal.h"
#include "ns_cavmain_child_thread.h"
#include <fcntl.h>
//#include "ns_jmeter.h"

/*****************************************************************************/

typedef enum {

  SM_MON_INIT_REQ,
  SM_MON_STOP_REQ,
  SM_MON_UPDATE_REQ,
  SM_MON_CONFIG_REQ,
  SM_MON_PAUSE_RESUME_REQ,
  MAX_NUM_MON_SMREQ
} SmMonReqIdx;

#define NS_DEFAULT_SCENARIO_PATH	"/default/default/scenarios/"
#define WORDSIZE_MON                 8

typedef int (*smreq_thr_func_ptr)();

__thread SMMonSessionInfo *sm_mon_info;
__thread long int t_id;
__thread int sm_thread_nvm_fd = -1;
smreq_thr_func_ptr smreq_thr_func_arr[MAX_NUM_MON_SMREQ];
unsigned short sm_nvm_listen_port;
static void *g_sm_tm_obj;

extern int loader_opcode;
/*****************************************************************************/

inline void register_sm_request()
{
/*   smreq_thr_func_arr[SM_MON_INIT_REQ] = sm_process_mon_start_req;
   smreq_thr_func_arr[SM_MON_STOP_REQ] = sm_process_mon_stop_req;
   smreq_thr_func_arr[SM_MON_UPDATE_REQ] = sm_process_mon_update_req;
   smreq_thr_func_arr[SM_MON_CONFIG_REQ] = sm_process_mon_config_req; */
}

void sm_fill_global_settings_from_parent_data()
{
   get_testid_and_set_debug_option();
}

int sm_init_thread_manager(int trace_fd)
{
   /*set max limit at run time*/
   g_sm_tm_obj = nslb_tm_init(1, 0, 1, 0); 
   return nslb_tm_config(g_sm_tm_obj, (void *)sm_run_command, trace_fd);
}


/* Thread function to parse the scenario and script*/
int sm_run_command(void *args)
{

   t_id = pthread_self();

  // Before staring parsing make connection with child
   if(sm_thread_nvm_fd == -1)
     if(sm_make_connection_thread_child(args) == SM_ERROR) 
         return SM_ERROR;

  // Allocate memory for Global and Group settings
   group_default_settings = (GroupSettings *)malloc(sizeof(GroupSettings));
   global_settings = (Global_data *) malloc(sizeof(Global_data));
   memset(global_settings, 0, sizeof(Global_data));
   memset(group_default_settings, 0, sizeof(GroupSettings));
  
   // As we have group_default settings on tls we have fill this with default 
   // so that debug logs can be printed

   global_settings->test_start_time = get_ms_stamp();
   sm_fill_global_settings_from_parent_data(); 

   SET_CALLER_TYPE(IS_NVM_THREAD);
  
   ns_tls_init(VUSER_THREAD_LOCAL_BUFFER_SIZE);

   NSDL2_MESSAGES(NULL, NULL,"Method called. thread_id=%ld", t_id);
   
   sm_mon_info = *(SMMonSessionInfo **) args;
   NSDL2_MESSAGES(NULL, NULL,"Method called. sm_mon_info=%p", sm_mon_info);

   /* Initialization of all structs and global vars */
   sm_init_all_mon_global_mem_with_defaults();
   sm_init_all_mon_shared_mem_with_defaults();
   sm_init_all_mon_global_vars_with_defaults();

   // Update global settings and group settings, sort scenario
   sm_mon_info->status = sm_parse_script_scenario_for_mon();
   
   if(sm_mon_info->status == SM_SUCCESS)
   { 
      setup_schedule_for_nvm(global_settings->num_process);
      sm_initialize_req_vars_for_exe();
      sm_map_shared_mem_with_structs();
   } 
  
   /* We need to call this function to setup scehedule for NVM */
    sm_send_msg_thread_to_nvm();

    free_structs();
    NSDL3_SCHEDULE(NULL, NULL,"Method Exit");

   return SM_SUCCESS;  
}

void sm_stop_monitor(int opcode, SMMonSessionInfo *tmsg)
{

  char msg[128]={0};

  if(!tmsg)
     return;

  if((opcode == NS_TEST_COMPLETED) || (opcode == NS_TEST_STOPPED))
     sprintf(msg, "%s", CM_MON_STOPPED);
  else
     sprintf(msg, "%s", CM_MON_STOPPED_ERR);

  cm_send_message_to_parent(opcode, tmsg->mon_id, msg);
  // Delete this map
  NSLBMap *nslb_map_ptr = sm_mon_map;
  nslb_map_delete(sm_mon_map, tmsg->mon_id); 
  // Free shared memory and sm_mon struct
  sm_free_shared_memory_structs();  
  sm_free_monitor_data(tmsg);
  sm_mon_info = NULL;

}
/*
void sm_send_exit_msg_to_nvm()
{

  char msg[128];
  if(sm_mon_info->status == SM_ERROR)
     return;

  sprintf(msg, "%s", CM_MON_STOPPED_ERR);

  cm_send_message_to_parent(NS_TEST_ERROR, sm_mon_info->mon_id, msg);
  //free_structs();
  sm_mon_info->status = SM_ERROR;

// Call free struct
}*/

void sm_process_monitor_thread_msg(SMMonSessionInfo *tmsg)
{
   char msg[128];
   int opcode; 
   int norm_id;
  
    NSDL3_SCHEDULE(NULL, NULL,"Method Called sm_mon_info => %p", tmsg);
      
   if(tmsg->status == SM_ERROR)
   {
      sm_stop_monitor(NS_TEST_ERROR, tmsg);
      return;
   }   
   sprintf(msg, "%s", CM_MON_STARTED);
   cm_send_message_to_parent(NS_TEST_STARTED, tmsg->mon_id, msg);
 
   NSLBMap *nslb_map_ptr = sm_mon_map; 
   if((norm_id = nslb_map_insert(nslb_map_ptr, tmsg->mon_id, (void*)tmsg)) < NS_ZERO)
   {
     NSDL3_MESSAGES(NULL, NULL, "nslb_map_insert failed. status=%d", norm_id);
     return;
   }
   // Set Global variable here so NVM can have copy of global structures and shared memory 
   cav_main_set_global_vars(tmsg);

   // Start Scheduling of Phases defined in scenario
   process_schedule(0, SCHEDULE_PHASE_START, -1);

   NSDL3_SCHEDULE(NULL, NULL,"Method Exit");
}


int sm_run_command_in_thread(char *mon_id, int mon_type, char *mon_name, int mon_index, char *tier, char *srv)
{
  
  NSDL2_MESSAGES(NULL, NULL,"Method called. mon_id=>%s, mon_type=>%d, mon_name=>%s, mon_index=>%d, tier=>%s, srv=>%s", mon_id, mon_type, mon_name, mon_index, tier, srv);
  
  SMMonSessionInfo *args;
  NSLB_MALLOC_AND_MEMSET(args, sizeof(SMMonSessionInfo), "SMMonSessionInfo", -1, NULL);

  NSDL2_MESSAGES(NULL, NULL,"args=%p", args);

  strncpy(args->mon_id, mon_id, SM_MON_ID_MAX_SIZE);
  strncpy(args->mon_name, mon_name, SM_MON_ID_MAX_SIZE);
  strncpy(args->tier_name, tier, SM_MON_ID_MAX_LEN);
  strncpy(args->srv_name, srv, SM_MON_ID_MAX_LEN);
  args->mon_type = mon_type;
  args->mon_index = mon_index;

  NSDL3_SCHEDULE(NULL, NULL,"Method Exit"); 

  return nslb_tm_exec(g_sm_tm_obj, (void *)&args, sizeof(SMMonSessionInfo *));
}

void sm_init_all_mon_global_mem_with_defaults()
{

   NSDL4_MESSAGES(NULL, NULL, "Method called");

   average_time = NULL;
   ips = NULL;
   clients = NULL;
   gSessionTable = NULL;
   gPageTable = NULL;
   requests = NULL;
   checkPageTable = NULL;
   checkReplySizePageTable = NULL;
   gServerTable = NULL;
   reqCookTable = NULL;
   segTable = NULL;
   pointerTable = NULL;
   postTable = NULL;
   varTable = NULL;
   groupTable = NULL;
   repeatBlock = NULL;
   fparamValueTable = NULL;
   weightTable = NULL;
   locAttrTable = NULL;
   accLocTable = NULL;
   scSzeAttrTable = NULL;
   pfBwScSzTable = NULL;
   lineCharTable = NULL;
   accAttrTable = NULL;
   brScSzTable = NULL;
   browAttrTable = NULL;
   sessProfTable = NULL;
   sessProfIndexTable = NULL;
   userProfTable = NULL;
   userIndexTable = NULL;
   runProfTable = NULL;
   runIndexTable = NULL;
   serverOrderTable = NULL;
   metricTable = NULL;
   hostTable = NULL;
   reportTable = NULL;
   dynVarTable = NULL;
   reqDynVarTable = NULL;
   inuseSvrTable = NULL; 
   inuseUserTable = NULL;
   thinkProfTable = NULL;
   inlineDelayTable = NULL;
   continueOnPageErrorTable = NULL;
   overrideRecordedThinktimeTable = NULL;
   autofetchTable = NULL; 
   clickActionTable = NULL;
   pacingTable = NULL;
   errorCodeTable  = NULL;
   nsVarTable = NULL;
   tagTable = NULL;
   attrQualTable = NULL;
   tagPageTable = NULL;
   attrQualTable = NULL;
   tagPageTable = NULL;
   searchVarTable = NULL;
   searchPageTable = NULL;
   uniquerangevarTable = NULL;
   jsonVarTable = NULL;
   jsonPageTable = NULL;
   randomVarTable = NULL;
   uniqueVarTable = NULL;
   randomStringTable = NULL;
   dateVarTable = NULL;
   perPageSerVarTable = NULL;
   perPageJSONVarTable = NULL;
   perPageChkPtTable = NULL;
   perPageChkRepSizeTable = NULL;
   clustVarTable = NULL;
   clustValTable = NULL;
   clustTable = NULL;
   groupVarTable = NULL;
   groupValTable = NULL;
   proxySvrTable = NULL;
   proxyExcpTable = NULL;
   proxyNetPrefixId = NULL;
   g_big_buf = NULL;
   g_buf_ptr = NULL;
   g_temp_buf = NULL;
   g_temp_ptr = NULL;
   txTable = NULL;
   seq_group_next = NULL;
   unique_group_table = NULL;

   NSDL4_MESSAGES(NULL, NULL,"Method Exit");
}

void sm_init_all_mon_global_vars_with_defaults()
{
   NSDL4_MESSAGES(NULL, NULL,"Method called");

   g_monitor_status = 0;
   total_pagereloadprof_entries = max_pagereloadprof_entries = 0;
   total_pageclickawayprof_entries = max_pageclickawayprof_entries = 0;
   g_auto_fetch_info_total_size = 0;
   total_cookie_entries = 0;
   max_cookie_entries = 0;
   total_reqcook_entries = 0;
   max_cookie_hash_code = 0;
   dns_lookup_stats_avgtime_idx = -1;
   total_errorcode_entries = 0;
   g_fc2_avgtime_idx = -1;
   g_ftp_avgtime_idx = -1;
   group_data_gp_idx = -1;
   g_cache_avgtime_idx = -1;
   rbu_web_url_host_id = -1;
   end_inline_url_count = 0 ;
   web_url_page_id = 0;
   http_resp_code_avgtime_idx = -1;
   g_imap_avgtime_idx = -1;
   g_jrmi_avgtime_idx = -1;
   g_ldap_avgtime_idx = -1;
   g_jrmi_avgtime_idx = -1;
   g_ldap_avgtime_idx = -1;
   g_jrmi_avgtime_idx = -1;
   g_ldap_avgtime_idx = -1;
   g_ns_diag_avgtime_idx = -1;
   g_network_cache_stats_avgtime_idx = -1;
   max_pagereloadprof_entries = 0;
   total_pagereloadprof_entries = 0 ;
   max_pageclickawayprof_entries = 0;
   total_pageclickawayprof_entries = 0;
   page_based_stat_gp_idx = -1;
   ns_nvm_scratch_buf_size = 0;
   ns_nvm_scratch_buf_len = 0; 
   g_proxy_avgtime_idx = -1;
   g_rbu_create_performance_trace_dir = 0;
   rbu_domain_stat_avg_idx = -1;
   rbu_page_stat_data_gp_idx = -1;
   show_vuser_flow_idx = -1;
   total_totsvr_entries = 0;
   static_host_table_shm_size = 0;
   gNAServerHost = -1;
   g_tcp_client_failures_avg_idx = -1;
   g_total_tcp_client_errs = 0;
   g_tcp_client_avg_idx = -1;
   g_udp_client_failures_avg_idx = -1;
   g_total_udp_client_errs = 0;
   g_udp_client_avg_idx = -1;
   g_cavtest_http_avg_idx = -1;
   g_cavtest_web_avg_idx = -1;
   total_tx_entries = 0;
   max_tx_entries = 0;
   g_trans_avgtime_idx = -1;
   total_um_entries = 0;
   g_ws_avgtime_idx = -1;
   g_xmpp_avgtime_idx = -1;
   g_cur_page = -1;
   g_cur_server = -1;
   g_max_num_embed = 0;
   g_avg_um_data_idx = 0;
   g_cavg_um_data_idx = 0; 
   g_avgtime_size = 0;
   g_static_avgtime_size = 0;
   g_avg_size_only_grp = 0;
   g_cavgtime_size = 0;
   g_cavg_size_only_grp = 0;
   total_vendor_locations = 0;
   used_tempbuffer_space = 0;
   max_user_entries = 0;
   max_ip_entries = 0;
   max_client_entries = 0;
   max_sess_entries = 0;
   g_actual_num_pages = 0; 
   g_rbu_num_pages = 0;  
   max_svr_entries = 0;
   max_var_entries = 0;
   max_group_entries = 0;
   max_fparam_entries = 0;
   max_weight_entries = 0;
   max_locattr_entries = 0;
   max_linechar_entries = 0;
   max_accattr_entries = 0;
   max_br_sc_sz_entries = 0;
   total_br_sc_sz_map_entries = 0;
   max_accloc_entries = 0;
   max_browattr_entries = 0;
   max_screen_size_entries = 0;
   total_screen_size_entries = 0;
   max_pf_bw_screen_size_entries = 0;
   total_pf_bw_screen_size_entries = 0;
   max_machattr_entries = 0;
   max_freqattr_entries = 0;
   max_sessprof_entries = 0;
   max_sessprofindex_entries = 0;
   max_userprof_entries = 0;
   max_userindex_entries = 0;
   max_runprof_entries = 0;
   max_runindex_entries = 0;
   max_metric_entries = 0;
   max_request_entries = 0;
   max_checkpage_entries = 0;
   total_checkpage_entries = 0;
   max_check_replysize_page_entries = 0; 
   total_check_replysize_page_entries = 0;
   max_pointer_entries = 0;
   max_seg_entries = 0;
   total_xmpp_request_entries = 0;
   total_fc2_request_entries = 0;
   max_host_entries = 0;
   max_dynvar_entries = 0;
   max_reqdynvar_entries = 0;
   max_thinkprof_entries = 0;
   max_inline_delay_entries = 0;
   max_pacing_entries = 0;
   max_autofetch_entries = 0; 
   max_cont_on_err_entries = 0;
   max_recorded_think_time_entries = 0;
   max_errorcode_entries = 0;
   max_nsvar_entries = 0;
   max_perpageservar_entries = 0;
   max_perpagejsonvar_entries = 0;
   max_clustvar_entries = 0;
   max_clust_entries = 0;
   max_groupvar_entries = 0;
   max_clickaction_entries = 0;
   total_clickaction_entries = 0;
   max_proxy_svr_entries = 0;
   max_proxy_excp_entries = 0;
   max_proxy_ip_interfaces = 0;
   max_buffer_space = 0;
   max_rbu_domain_entries = 0;
   max_http_resp_code_entries = 0; 
   total_http_resp_code_entries = 0;
   default_userprof_idx = 0;
   config_file_server_base = -1;
   config_file_server_idx = -1;
   unique_group_id = 0;
   max_ssl_cert_key_entries = 0;
   total_active_runprof_entries = 0;
   max_svr_group_num = -1;
   max_page_entries = 0;
   total_userprofshr_entries=0;
   max_var_table_idx = 0;
   num_dyn_host_left = 0;
   //is_static_host_shm_created = 0;
   //num_dyn_host_add = 0;
   NSDL4_MESSAGES(NULL, NULL,"Method Exit");
}


void sm_init_all_mon_shared_mem_with_defaults()
{
   NSDL4_MESSAGES(NULL, NULL,"Method called");

   big_buf_shr_mem = NULL;
   pointer_table_shr_mem = NULL;
   weight_table_shr_mem = NULL;
   group_table_shr_mem = NULL;
   variable_table_shr_mem = NULL;
   index_variable_table_shr_mem = NULL;
   repeat_block_shr_mem = NULL;
   randomvar_table_shr_mem = NULL;
   randomstring_table_shr_mem = NULL;
   uniquevar_table_shr_mem = NULL;
   datevar_table_shr_mem = NULL; 
   gserver_table_shr_mem = NULL;
   seg_table_shr_mem = NULL;
   serverorder_table_shr_mem = NULL;
   post_table_shr_mem = NULL;
   reqcook_table_shr_mem = NULL;
   reqdynvar_table_shr_mem = NULL;
   clickaction_table_shr_mem = NULL;
   request_table_shr_mem = NULL;
   host_table_shr_mem = NULL;
   thinkprof_table_shr_mem = NULL;
   inline_delay_table_shr_mem = NULL;
   autofetch_table_shr_mem = NULL;
   pacing_table_shr_mem = NULL;
   continueOnPageErrorTable_shr_mem = NULL;
// perpageservar_table_shr_mem = NULL;
// perpagejsonvar_table_shr_mem = NULL; //JSON var 
   perpagechkpt_table_shr_mem = NULL;
// perpagechk_replysize_table_shr_mem= NULL;
   page_table_shr_mem = NULL;
   session_table_shr_mem = NULL;
   locattr_table_shr_mem = NULL;
   accattr_table_shr_mem = NULL;
   browattr_table_shr_mem = NULL;
   freqattr_table_shr_mem = NULL;
   machattr_table_shr_mem = NULL;
   scszattr_table_share_mem = NULL; 
   sessprof_table_shr_mem = NULL;
   sessprofindex_table_shr_mem = NULL;
   runprof_table_shr_mem = NULL;
   proxySvr_table_shr_mem = NULL;
   proxyExcp_table_shr_mem = NULL;
// proxyNetPrefix_table_shr_mem = NULL;
   metric_table_shr_mem = NULL;
   inusesvr_table_shr_mem = NULL;
   errorcode_table_shr_mem = NULL;
// Need to discuss
// userprof_table_shr_mem = NULL;
   userprofindex_table_shr_mem = NULL;
   runprofindex_table_shr_mem = NULL;
   tx_table_shr_mem = NULL;
   http_method_table_shr_mem = NULL;
   testcase_shr_mem = NULL;
   pattern_table_shr = NULL;
   //actsvr_table_shr_mem= NULL;
   per_proc_vgroup_table = NULL;
   g_static_vars_shr_mem = NULL;
   fparamValueTable_shr_mem = NULL;
   nsl_var_table_shr_mem = NULL;
   searchvar_table_shr_mem = NULL; 
   NSDL2_MESSAGES(NULL, NULL,"Method Exit");
}

int sm_parse_script_scenario_for_mon()
{
   char err_msg[4096 + 1] = {0};
   char cmd[MAX_LINE_LENGTH];
   char mon_scenario[SM_MON_ID_MAX_SIZE + 1] = {0};;
  
   NSDL3_SCHEDULE(NULL, NULL,"Method Called");

   // Creation of scenario file name with path
   /*bug id: 101320: using g_ns_ta_dir instead of g_ns_wdir*/
   sprintf(mon_scenario, "%s%s%s.conf", GET_NS_TA_DIR(), NS_DEFAULT_SCENARIO_PATH, sm_mon_info->mon_id);
   NSDL3_SCHEDULE(NULL, NULL,"mon_scenario with NS_TA_DIR(%s) = %s", GET_NS_TA_DIR(), mon_scenario);

   /* Sorting of Scenario - Start */
   sprintf(g_sorted_conf_file, "%s/logs/TR%d/%s_sorted_scenario.conf", g_ns_wdir, testidx, sm_mon_info->mon_id);
   sprintf(cmd, "%s/bin/nsi_merge_sort_scen 0 %s %s", g_ns_wdir, mon_scenario, g_sorted_conf_file);
   
   if(run_and_get_cmd_output(cmd, 4096, err_msg) == -1)
      return SM_ERROR;   
   /* Sorting of Scenario - End */
   
   if((confirm_netstorm_uid(argv0, err_msg)) == -1)
      return SM_ERROR;
 
   /* First Level Parsing - Start */ 
   if(parse_keyword_before_init(g_sorted_conf_file, err_msg) == -1)
      return SM_ERROR;
   /* First Level Parsing - End */

   /* Second Level Parsing - Start */
   // This function will do all the parsing of script and scenario */
   if(sm_mon_init_after_parsing_args() == -1)
      return SM_ERROR;
   /* Second Level Parsing - End */
  
   NSDL3_SCHEDULE(NULL, NULL,"Method Exit");
   return SM_SUCCESS;
}
// After parsing has been done we have to override some setttings according to our need
void sm_override_mon_global_settings()
{

   debug_trace_log_value = 0;
   // For Thread num_process will be 1 only
   global_settings->num_process = 1;
   // Copy all monitor specific data 
   global_settings->monitor_type = sm_mon_info->mon_type;
   global_settings->monitor_idx = sm_mon_info->mon_index;
   // Set Monitor Name
   NSLB_MALLOC_AND_MEMSET(global_settings->monitor_name, SM_MON_ID_MAX_SIZE + 1, "sm_monitor_name", -1, NULL);
   snprintf(global_settings->monitor_name, SM_MON_ID_MAX_SIZE, "%s", sm_mon_info->mon_name);
   // Set Tier Name
   NSLB_MALLOC_AND_MEMSET(global_settings->cavtest_tier, MAX_MON_NAME_LENGTH + 1, "global_settings->cavtest_tier", -1, NULL);
   snprintf(global_settings->cavtest_tier, MAX_MON_NAME_LENGTH, "%s", sm_mon_info->tier_name);

   //Set Server Name
   NSLB_MALLOC_AND_MEMSET(global_settings->cavtest_server, MAX_MON_NAME_LENGTH + 1, "global_settings->cavtest_server", -1, NULL);
   snprintf(global_settings->cavtest_server, MAX_MON_NAME_LENGTH, "%s", sm_mon_info->srv_name);

   NSDL3_SCHEDULE(NULL, NULL,"Monitor Information:: monitor_type = %d, monitor_idx = %d, monitor_name = %s, cavtest_tier= %s, cavtest_server = %s", global_settings->monitor_type, global_settings->monitor_idx, global_settings->monitor_name, global_settings->cavtest_tier, global_settings->cavtest_server);
}

int sm_create_rbu_logs_dir(char *sm_mon_id)
{
  char rbu_logs_path[512 + 1];
  char cmd_buf[1024 + 1];
  int ret = 0;

  NSDL3_PARSING(NULL, NULL, "Method Called, sm_mon_id = %s", sm_mon_id);

  sprintf(rbu_logs_path, "%s/logs/TR%d/%s/rbu_logs", g_ns_wdir, testidx, sm_mon_id);

  if (global_settings->create_screen_shot_dir == SNAP_SHOT_DIR_FLAG) {
    sprintf(cmd_buf, "mkdir -p -m 777 %s/snap_shots && "
                 "mkdir -p -m 777 %s/harp_files",
                 rbu_logs_path, rbu_logs_path);
  }
  else  if (global_settings->create_screen_shot_dir == SCREEN_SHOT_DIR_FLAG) {
    sprintf(cmd_buf, "mkdir -p -m 777 %s/screen_shot && "
                 "mkdir -p -m 777 %s/harp_files",
                 rbu_logs_path, rbu_logs_path);
  }
  else if (global_settings->create_screen_shot_dir == ALL_DIR_FLAG) {
    sprintf(cmd_buf, "mkdir -p -m 777 %s/snap_shots && "
                 "mkdir -p -m 777 %s/screen_shot && "
                 "mkdir -p -m 777 %s/harp_files",
                 rbu_logs_path, rbu_logs_path, rbu_logs_path);
  }
  else
    sprintf(cmd_buf, "mkdir -p -m 777 %s/harp_files", rbu_logs_path);

  NSDL1_PARENT(NULL, NULL, "cmd run = [%s] and rbu_logs_path = [%s]", cmd_buf, rbu_logs_path);
  ret = nslb_system2(cmd_buf);
  if (WEXITSTATUS(ret) == 1){
    return -1;
  }
  else {
    NSTL1(NULL, NULL, "Created harp_files dir \'%s/harp_files\'", rbu_logs_path);
    NSDL1_PARENT(NULL, NULL, "Created harp_files dir \'%s/harp_files\' and create_screen_shot_flag is [%d] and command cmd_buf = [%s]",
                              rbu_logs_path, global_settings->create_screen_shot_dir, cmd_buf);
  }

  return 0;
}

/* This Function contains the part required for scenario and script parsing.
   We have used the code of parent_init_after_parsing_args function */ 
int sm_mon_init_after_parsing_args()
{
   char fname[512] = {0};
   char tname[512] = {0};

   NSDL3_SCHEDULE(NULL, NULL,"Method Called");
   /* Before parse file we have to update project and sub-project name */
   strcpy(g_project_name, "default");
   strcpy(g_subproject_name, "default");

   
   sprintf(tname, "ns_inst%lld_%ld",nslb_get_cur_time_in_ms(),t_id);
   sprintf(g_ns_tmpdir, ".tmp/%s",tname);
   sprintf(fname,"%s/.tmp/%s",g_ns_wdir,tname);

   if(mkdir(fname, 0755))
   {
      if(errno == ENOSPC)
         fprintf(stderr, "Disk running out of space, can not start new test run\n Please release some space and try again");
      else if(errno == EACCES)
         fprintf(stderr, "Error: %s/webapps/logs do not have write permission to create test run directory\n"
                      "       Please correct permission attributes, and try again", g_ns_wdir);
      else
         fprintf(stderr, "Failed to create instance directory %s", fname);
      return SM_ERROR;
   }

   if(parse_files() == -1)
      return SM_ERROR;

   if(g_monitor_status == SM_ERROR)
      return SM_ERROR;
  
   max_tx_entries = total_tx_entries;

   //Create rbu_logs directory
   if(sm_mon_info->mon_type == WEB_PAGE_AUDIT)
   {
     if(sm_create_rbu_logs_dir(sm_mon_info->mon_id) == -1)
       return SM_ERROR;
   }

   //Setting page_start in RunProfTable and relative_page_id in PageTable for Page Based Stat feature
   init_page_based_stat();    //For page based stat

   // After parsing args we have to make some settings as globals
   sm_override_mon_global_settings();
   
   init_all_avgtime();

   /* As we are calling the main function of ns_parent where after end of parsing structs
      are copied to shared memory so here are mapping the shared memory with their
      corresponding structs */
   copy_structs_into_shared_mem(); 
    
   // This function will set member custom_delay_func_ptr of table inlineDelayTable.
   // We are calling this function from here as PASS1, PASS2 is completed and Shared mem is created.
   fill_custom_delay_fun_ptr();
   fill_custom_page_think_time_fun_ptr();

   create_per_proc_sess_table(); // TO DO CAVMAIN
   check_and_set_flag_for_ka_timer();
   check_and_set_flag_for_idle_timer();
   //Validation of G_AUTO_FETCH_EMBEDDED keyword with AUTO_REDIRECT, AUTO_COOKIE and G_NO_VALIDATION  
   validate_g_auto_fetch_embedded_keyword();

   set_num_additional_headers();

   setup_rec_tbl_dyn_host(global_settings->max_dyn_host); 

   /* If nsl_unique_range var API is present then create_unique_range_var_table_per_proc method is called for each NVM, this method
      is used to create UniqueRangeVarPerProcessTable.
   * Memory is allocated for UniqueRangeVarPerProcessTable only for NVM0, other NVMs overwrite this memory*/
   if(total_unique_rangevar_entries){
      MY_MALLOC(unique_range_var_table, total_unique_rangevar_entries * sizeof(UniqueRangeVarPerProcessTable), "unique_range_var_table", i);
      create_unique_range_var_table_per_proc(0);
   }

   NSDL3_SCHEDULE(NULL, NULL,"Method Exit");
   return SM_SUCCESS;

}

// Mapping of sm_mon_info structs with shared memory
void sm_map_shared_mem_with_structs()
{

   #ifdef CAV_MAIN 
   NSDL3_SCHEDULE(NULL, NULL,"Method Called"); 
   
   sm_mon_info->big_buf_shr_mem                    = big_buf_shr_mem;
   sm_mon_info->pointer_table_shr_mem              = pointer_table_shr_mem;
   sm_mon_info->weight_table_shr_mem               = weight_table_shr_mem;
   sm_mon_info->group_table_shr_mem                = group_table_shr_mem;
   sm_mon_info->variable_table_shr_mem             = variable_table_shr_mem;
   sm_mon_info->index_variable_table_shr_mem       = index_variable_table_shr_mem;
   sm_mon_info->repeat_block_shr_mem               = repeat_block_shr_mem;
   sm_mon_info->randomvar_table_shr_mem            = randomvar_table_shr_mem;
   sm_mon_info->randomstring_table_shr_mem         = randomstring_table_shr_mem;
   sm_mon_info->uniquevar_table_shr_mem            = uniquevar_table_shr_mem;
   sm_mon_info->datevar_table_shr_mem              = datevar_table_shr_mem; 
   sm_mon_info->gserver_table_shr_mem              = gserver_table_shr_mem;
   sm_mon_info->seg_table_shr_mem                  = seg_table_shr_mem;
   sm_mon_info->serverorder_table_shr_mem          = serverorder_table_shr_mem;
   sm_mon_info->post_table_shr_mem                 = post_table_shr_mem;
   sm_mon_info->reqcook_table_shr_mem              = reqcook_table_shr_mem;
   sm_mon_info->reqdynvar_table_shr_mem            = reqdynvar_table_shr_mem;
   sm_mon_info->clickaction_table_shr_mem          = clickaction_table_shr_mem;
   sm_mon_info->request_table_shr_mem              = request_table_shr_mem;
   sm_mon_info->host_table_shr_mem                 = host_table_shr_mem;
   sm_mon_info->thinkprof_table_shr_mem            = thinkprof_table_shr_mem;
   sm_mon_info->inline_delay_table_shr_mem         = inline_delay_table_shr_mem;
   sm_mon_info->autofetch_table_shr_mem            = autofetch_table_shr_mem;
   sm_mon_info->pacing_table_shr_mem               = pacing_table_shr_mem;
   sm_mon_info->continueOnPageErrorTable_shr_mem   = continueOnPageErrorTable_shr_mem;
// perpageservar_table_shr_mem = NULL;
// perpagejsonvar_table_shr_mem = NULL; //JSON var 
   sm_mon_info->perpagechkpt_table_shr_mem         = perpagechkpt_table_shr_mem;
// perpagechk_replysize_table_shr_mem= NULL;
   sm_mon_info->page_table_shr_mem                 = page_table_shr_mem;
   sm_mon_info->session_table_shr_mem              = session_table_shr_mem;
   sm_mon_info->locattr_table_shr_mem              = locattr_table_shr_mem;
   sm_mon_info->accattr_table_shr_mem              = accattr_table_shr_mem;
   sm_mon_info->browattr_table_shr_mem             = browattr_table_shr_mem;
   sm_mon_info->scszattr_table_share_mem           = scszattr_table_share_mem; 
   sm_mon_info->sessprof_table_shr_mem             = sessprof_table_shr_mem;
   sm_mon_info->sessprofindex_table_shr_mem        = sessprofindex_table_shr_mem;
   sm_mon_info->runprof_table_shr_mem              = runprof_table_shr_mem;
   sm_mon_info->proxySvr_table_shr_mem             = proxySvr_table_shr_mem;
   sm_mon_info->proxyExcp_table_shr_mem            = proxyExcp_table_shr_mem;
// proxyNetPrefix_table_shr_mem = NULL;
   sm_mon_info->metric_table_shr_mem               = metric_table_shr_mem;
   sm_mon_info->inusesvr_table_shr_mem             = inusesvr_table_shr_mem;
   sm_mon_info->errorcode_table_shr_mem            = errorcode_table_shr_mem;
// Need to discuss
// userprof_table_shr_mem = NULL;
   sm_mon_info->userprofindex_table_shr_mem        = userprofindex_table_shr_mem;
   sm_mon_info->runprofindex_table_shr_mem         = runprofindex_table_shr_mem;
   sm_mon_info->tx_table_shr_mem                   = tx_table_shr_mem;
   sm_mon_info->http_method_table_shr_mem          = http_method_table_shr_mem;
   sm_mon_info->testcase_shr_mem                   = &testCase;
   sm_mon_info->pattern_table_shr                  = pattern_table_shr;

   sm_mon_info->group_default_settings             = group_default_settings;
   sm_mon_info->global_settings                    = global_settings;
   
   sm_mon_info->v_port_table                       = v_port_table;
   sm_mon_info->v_port_entry                       = v_port_entry;
   sm_mon_info->average_time                       = average_time;
   sm_mon_info->g_cur_server                       = g_cur_server;
   sm_mon_info->user_svr_table_size                = user_svr_table_size;
   sm_mon_info->user_group_table_size              = user_group_table_size;
   sm_mon_info->user_cookie_table_size             = user_cookie_table_size;
   sm_mon_info->user_dynamic_vars_table_size       = user_dynamic_vars_table_size;
   sm_mon_info->user_var_table_size                = user_var_table_size;
   sm_mon_info->user_order_table_size              = user_order_table_size;
   sm_mon_info->g_ssl_ctx                          = g_ssl_ctx;


   sm_mon_info->g_avgtime_size                     = g_avgtime_size;
   sm_mon_info->g_cache_avgtime_idx                = g_cache_avgtime_idx;
   sm_mon_info->g_proxy_avgtime_idx                = g_proxy_avgtime_idx;
   sm_mon_info->g_network_cache_stats_avgtime_idx  = g_network_cache_stats_avgtime_idx;
   sm_mon_info->dns_lookup_stats_avgtime_idx       = dns_lookup_stats_avgtime_idx;
   sm_mon_info->g_ftp_avgtime_idx                  = g_ftp_avgtime_idx;
   sm_mon_info->g_ldap_avgtime_idx                 = g_ldap_avgtime_idx;
   sm_mon_info->g_imap_avgtime_idx                 = g_imap_avgtime_idx;
   sm_mon_info->g_jrmi_avgtime_idx                 = g_jrmi_avgtime_idx;
   sm_mon_info->g_ws_avgtime_idx                   = g_ws_avgtime_idx;
   sm_mon_info->g_xmpp_avgtime_idx                 = g_xmpp_avgtime_idx;
   sm_mon_info->g_fc2_avgtime_idx                  = g_fc2_avgtime_idx;
   // Its now obsoleter from 4.5.1 JMeter Redesign
   //sm_mon_info->g_jmeter_avgtime_idx               = g_jmeter_avgtime_idx;
   sm_mon_info->g_tcp_client_avg_idx               = g_tcp_client_avg_idx;
   sm_mon_info->g_udp_client_avg_idx               = g_udp_client_avg_idx;
   sm_mon_info->g_avg_size_only_grp                = g_avg_size_only_grp;
   sm_mon_info->g_avg_um_data_idx                  = g_avg_um_data_idx;
   sm_mon_info->group_data_gp_idx                  = group_data_gp_idx;
   sm_mon_info->rbu_page_stat_data_gp_idx          = rbu_page_stat_data_gp_idx;
   sm_mon_info->page_based_stat_gp_idx             = page_based_stat_gp_idx;
   sm_mon_info->g_cavtest_http_avg_idx             = g_cavtest_http_avg_idx;
   sm_mon_info->g_cavtest_web_avg_idx              = g_cavtest_web_avg_idx;
// unsigned int (*tx_hash_func)(const char*, unsigned int); - TBD
   sm_mon_info->show_vuser_flow_idx                = show_vuser_flow_idx;
   sm_mon_info->g_static_avgtime_size              = g_static_avgtime_size;
   sm_mon_info->g_udp_client_failures_avg_idx      = g_udp_client_failures_avg_idx;
   memcpy(&sm_mon_info->g_udp_client_errs_normtbl, &g_udp_client_errs_normtbl, sizeof(NormObjKey));
   sm_mon_info->g_total_udp_client_errs            = g_total_udp_client_errs;
   sm_mon_info->g_tcp_client_failures_avg_idx      = g_tcp_client_failures_avg_idx;
   memcpy(&sm_mon_info->g_tcp_client_errs_normtbl, &g_tcp_client_errs_normtbl, sizeof(NormObjKey));
   sm_mon_info->g_total_tcp_client_errs            = g_total_tcp_client_errs;
   sm_mon_info->http_resp_code_avgtime_idx         = http_resp_code_avgtime_idx;
   sm_mon_info->total_http_resp_code_entries       = total_http_resp_code_entries;
   sm_mon_info->g_http_status_code_loc2norm_table  = g_http_status_code_loc2norm_table;
   sm_mon_info->total_tx_entries                   = total_tx_entries;
   sm_mon_info->txData                             = txData;
   sm_mon_info->g_trans_avgtime_idx                = g_trans_avgtime_idx;
   sm_mon_info->g_tx_loc2norm_table                = g_tx_loc2norm_table;
   memcpy(&sm_mon_info->rbu_domian_normtbl, &rbu_domian_normtbl, sizeof(NormObjKey));
   sm_mon_info->rbu_domain_stat_avg_idx            = rbu_domain_stat_avg_idx;
   sm_mon_info->g_domain_loc2norm_table            = g_domain_loc2norm_table;
   memcpy(&sm_mon_info->normRuntimeTXTable, &normRuntimeTXTable, sizeof(NormObjKey));
    
   sm_mon_info->um_info                            = um_info;
   sm_mon_info->cache_avgtime                      = cache_avgtime;
   sm_mon_info->proxy_avgtime                      = proxy_avgtime;
   sm_mon_info->network_cache_stats_avgtime        = network_cache_stats_avgtime;
   sm_mon_info->dns_lookup_stats_avgtime           = dns_lookup_stats_avgtime;
   sm_mon_info->ftp_avgtime                        = ftp_avgtime;
   sm_mon_info->ldap_avgtime                       = ldap_avgtime;
   sm_mon_info->imap_avgtime                       = imap_avgtime;
   sm_mon_info->jrmi_avgtime                       = jrmi_avgtime;
   sm_mon_info->xmpp_avgtime                       = xmpp_avgtime;
   sm_mon_info->rbu_domain_stat_avg                = rbu_domain_stat_avg;
   sm_mon_info->http_resp_code_avgtime             = http_resp_code_avgtime;
   sm_mon_info->g_tcp_client_failures_avg          = g_tcp_client_failures_avg;
   sm_mon_info->g_udp_client_failures_avg          = g_udp_client_failures_avg;
   sm_mon_info->ns_diag_avgtime                    = ns_diag_avgtime;
   sm_mon_info->g_tcp_client_avg                   = g_tcp_client_avg;
   sm_mon_info->g_udp_client_avg                   = g_udp_client_avg;
   sm_mon_info->grp_avgtime                        = grp_avgtime;
   sm_mon_info->rbu_page_stat_avg                  = rbu_page_stat_avg;
   sm_mon_info->page_stat_avgtime                  = page_stat_avgtime;
   sm_mon_info->vuser_flow_avgtime                 = vuser_flow_avgtime;
   sm_mon_info->cavtest_http_avg                   = cavtest_http_avg;
   sm_mon_info->cavtest_web_avg                    = cavtest_web_avg;
   sm_mon_info->total_runprof_entries              = total_runprof_entries;
   sm_mon_info->my_runprof_table                   = my_runprof_table;
   sm_mon_info->my_vgroup_table                    = my_vgroup_table;
   //sm_mon_info->g_jmeter_avgtime                   = g_jmeter_avgtime;    
   sm_mon_info->unique_range_var_table             = unique_range_var_table;
   sm_mon_info->actsvr_table_shr_mem               = actsvr_table_shr_mem;
   sm_mon_info->num_dyn_host_left                  = num_dyn_host_left;
   sm_mon_info->is_static_host_shm_created         = is_static_host_shm_created;
   sm_mon_info->num_dyn_host_add                   = num_dyn_host_add;
   sm_mon_info->per_proc_vgroup_table              = per_proc_vgroup_table;
   sm_mon_info->fparamValueTable_shr_mem           = fparamValueTable_shr_mem;
   sm_mon_info->nsl_var_table_shr_mem              = nsl_var_table_shr_mem;
   sm_mon_info->searchvar_table_shr_mem            = searchvar_table_shr_mem; 
   sm_mon_info->seq_group_next                     = seq_group_next;
   sm_mon_info->used_buffer_space                  = used_buffer_space;
   sm_mon_info->total_pointer_entries              = total_pointer_entries;
   sm_mon_info->total_weight_entries               = total_weight_entries;
   sm_mon_info->total_group_entries                = total_group_entries;
   sm_mon_info->total_var_entries                  = total_var_entries;
   sm_mon_info->total_index_var_entries            = total_index_var_entries;
   sm_mon_info->total_repeat_block_entries         = total_repeat_block_entries;
   sm_mon_info->total_nsvar_entries                = total_nsvar_entries;
   sm_mon_info->total_randomvar_entries            = total_randomvar_entries;
   sm_mon_info->total_randomstring_entries         = total_randomstring_entries;
   sm_mon_info->total_uniquevar_entries            = total_uniquevar_entries;
   sm_mon_info->total_datevar_entries              = total_datevar_entries;
   sm_mon_info->total_svr_entries                  = total_svr_entries;
   sm_mon_info->total_seg_entries                  = total_seg_entries;
   sm_mon_info->total_serverorder_entries          = total_serverorder_entries;
   sm_mon_info->total_post_entries                 = total_post_entries;
   sm_mon_info->total_reqcook_entries              = total_reqcook_entries;
   sm_mon_info->total_reqdynvar_entries            = total_reqdynvar_entries;
   sm_mon_info->total_clickaction_entries          = total_clickaction_entries;
   sm_mon_info->total_request_entries              = total_request_entries;
   sm_mon_info->total_http_method                  = total_http_method;
   sm_mon_info->total_host_entries                 = total_host_entries;
   sm_mon_info->total_jsonvar_entries              = total_jsonvar_entries;
   sm_mon_info->total_perpagejsonvar_entries       = total_perpagejsonvar_entries;
   sm_mon_info->total_checkpoint_entries           = total_checkpoint_entries;
   sm_mon_info->total_perpagechkpt_entries         = total_perpagechkpt_entries;
   sm_mon_info->total_checkreplysize_entries       = total_checkreplysize_entries;
   sm_mon_info->total_perpagechkrepsize_entries    = total_perpagechkrepsize_entries;
   sm_mon_info->total_page_entries                 = total_page_entries;
   sm_mon_info->total_tx_entries                   = total_tx_entries;
   sm_mon_info->total_sess_entries                 = total_sess_entries;
   sm_mon_info->total_locattr_entries              = total_locattr_entries;
   sm_mon_info->total_sessprof_entries             = total_sessprof_entries;
   sm_mon_info->total_inusesvr_entries             = total_inusesvr_entries;
   sm_mon_info->total_errorcode_entries            = total_errorcode_entries;
   sm_mon_info->total_clustvar_entries             = total_clustvar_entries;
   sm_mon_info->total_clust_entries                = total_clust_entries;
   sm_mon_info->total_groupvar_entries             = total_groupvar_entries;
   sm_mon_info->total_userindex_entries            = total_userindex_entries;
   sm_mon_info->total_userprofshr_entries          = total_userprofshr_entries;
   sm_mon_info->g_static_vars_shr_mem_size         = g_static_vars_shr_mem_size;
   sm_mon_info->total_fparam_entries               = total_fparam_entries;
   sm_mon_info->total_searchvar_entries            = total_searchvar_entries;
   sm_mon_info->total_perpageservar_entries        = total_perpageservar_entries;
   sm_mon_info->unique_group_table                 = unique_group_table;    
   sm_mon_info->unique_group_id                    = unique_group_id;
   NSDL3_SCHEDULE(NULL, NULL,"Method Exit");
   #endif
}

// Below starting actual execution need this for environment setup
void sm_initialize_req_vars_for_exe()
{

   test_data_init();
   init_vuser_summary_table(0);

}

// Making connection from Thread => Child
int sm_make_connection_thread_child(void *args)
{

   char *ip = "127.0.0.1";
   int flags;

   if((sm_thread_nvm_fd = nslb_tcp_client_r(ip, sm_nvm_listen_port)) == -1)
   {
      //NSDL3_SCHEDULE(NULL, NULL,"Failed: Thread while making connection with NVM for mon_id(%s)", ((SMMonSessionInfo*)args)->mon_id );
      return SM_ERROR; // TO DO CAVMAIN what thread will do in this case
   }
   flags = fcntl(sm_thread_nvm_fd, F_GETFL , 0);
   flags &= ~O_NONBLOCK;
   fcntl(sm_thread_nvm_fd, F_SETFL, flags);
   return SM_SUCCESS;
}

void sm_send_msg_thread_to_nvm()
{
 
  NSDL3_SCHEDULE(NULL, NULL,"Method Called"); 
  Vutd_info sm_req; 
  sm_req.thread_info = (void *)sm_mon_info;
  NSDL2_MESSAGES(NULL, NULL, "Sending the message: mon_info %p and thread_info:: %p ", sm_mon_info, sm_req.thread_info);
  vutd_write_msg(sm_thread_nvm_fd, (char *)(&sm_req), sizeof(Vutd_info));
  NSDL3_SCHEDULE(NULL, NULL,"Method Exit");

}

void sm_free_monitor_data(SMMonSessionInfo *sm_mon_info_var)
{
  
   NSDL3_SCHEDULE(NULL, NULL,"Method Called: SM info => %p", sm_mon_info_var);
   FREE_AND_MAKE_NOT_NULL_EX (sm_mon_info_var->average_time, g_avgtime_size, "average time" , -1);
   FREE_AND_MAKE_NOT_NULL_EX (sm_mon_info_var->group_default_settings, sizeof(GroupSettings), "Group Settings" , -1);
   if(sm_mon_info_var->global_settings)
   {
      NSDL3_SCHEDULE(NULL, NULL,"Method Called: SM info alert info=> %p", sm_mon_info_var->global_settings->alert_info);
      FREE_AND_MAKE_NOT_NULL_EX (sm_mon_info_var->global_settings->monitor_name, SM_MON_ID_MAX_SIZE, "Monitor Name" , -1);
      FREE_AND_MAKE_NOT_NULL_EX (sm_mon_info_var->global_settings->cavtest_tier, MAX_MON_NAME_LENGTH + 1, "cavtest tier" , -1);
      FREE_AND_MAKE_NOT_NULL_EX (sm_mon_info_var->global_settings->cavtest_server, MAX_MON_NAME_LENGTH + 1, "cavtest server" , -1);
      FREE_AND_MAKE_NOT_NULL_EX (sm_mon_info_var->global_settings, sizeof(Global_data), "Global Settings" , -1);
   }
   FREE_AND_MAKE_NULL_EX (sm_mon_info_var, sizeof(SMMonSessionInfo), "SM MON INFO", -1);  
}


void sm_free_shared_memory_structs()
{
   int i, actual_num_pages;
   FREE_AND_MAKE_NULL_EX (big_buf_shr_mem, used_buffer_space, "bigbuf", -1);
   FREE_AND_MAKE_NULL_EX (pointer_table_shr_mem, sizeof(PointerTableEntry_Shr) * total_pointer_entries, "pointer table", -1);
  // FREE_AND_MAKE_NULL_EX (event_def_shr_mem, sizeof(EventDefinitionShr) * num_total_event_id, "event def", -1);
   if(total_weight_entries)
   {
      FREE_AND_MAKE_NULL_EX (weight_table_shr_mem, (sizeof(WeightTableEntry) * total_weight_entries) + (sizeof(GroupTableEntry_Shr) * total_group_entries) + (sizeof(VarTableEntry_Shr) * total_var_entries), "weight table", -1);
   }
   else if(total_group_entries)
   {
      FREE_AND_MAKE_NULL_EX (group_table_shr_mem, (sizeof(WeightTableEntry) * total_weight_entries) + (sizeof(GroupTableEntry_Shr) * total_group_entries) + (sizeof(VarTableEntry_Shr) * total_var_entries), "group table", -1);
   }
   else
   {
      FREE_AND_MAKE_NULL_EX (variable_table_shr_mem, (sizeof(WeightTableEntry) * total_weight_entries) + (sizeof(GroupTableEntry_Shr) * total_group_entries) + (sizeof(VarTableEntry_Shr) * total_var_entries), "variable table", -1);
   }
   FREE_AND_MAKE_NULL_EX (index_variable_table_shr_mem, sizeof(VarTableEntry_Shr) * total_index_var_entries, "index var table", -1);
   FREE_AND_MAKE_NULL_EX (repeat_block_shr_mem, total_repeat_block_entries * sizeof(RepeatBlock_Shr), "repeat block", -1);
   FREE_AND_MAKE_NULL_EX (nsl_var_table_shr_mem, sizeof(NslVarTableEntry_Shr) * (total_nsvar_entries + 1), "nsl var table", -1);
   FREE_AND_MAKE_NULL_EX (randomvar_table_shr_mem, total_randomvar_entries * sizeof(RandomVarTableEntry_Shr), "randomvar table", -1);
   FREE_AND_MAKE_NULL_EX (randomstring_table_shr_mem, total_randomstring_entries * sizeof(RandomStringTableEntry_Shr), "randomstring table", -1);
   FREE_AND_MAKE_NULL_EX (uniquevar_table_shr_mem, total_uniquevar_entries * sizeof(UniqueVarTableEntry_Shr), "uniquevar table", -1);
   FREE_AND_MAKE_NULL_EX (datevar_table_shr_mem, total_datevar_entries * sizeof(DateVarTableEntry_Shr), "datevar table", -1);
   FREE_AND_MAKE_NULL_EX (gserver_table_shr_mem, sizeof(SvrTableEntry_Shr) * total_svr_entries, "gserver table", -1);
   FREE_AND_MAKE_NULL_EX (seg_table_shr_mem, sizeof(SegTableEntry_Shr) * total_seg_entries, "seg table", -1);
   FREE_AND_MAKE_NULL_EX (serverorder_table_shr_mem, WORD_ALIGNED(sizeof(ServerOrderTableEntry_Shr) * total_serverorder_entries), "serverorder table", -1);
   //FREE_AND_MAKE_NULL_EX (post_table_shr_mem, WORD_ALIGNED(sizeof(StrEnt_Shr) * total_post_entries), "variable table", -1);
   FREE_AND_MAKE_NULL_EX (reqcook_table_shr_mem, WORD_ALIGNED(sizeof(ReqCookTableEntry_Shr) * total_reqcook_entries) + WORD_ALIGNED(sizeof(ReqDynVarTableEntry_Shr) * total_reqdynvar_entries), "reqcook table", -1);
   FREE_AND_MAKE_NULL_EX (clickaction_table_shr_mem, sizeof(ClickActionTableEntry_Shr) * total_clickaction_entries, "clickaction table", -1);
   FREE_AND_MAKE_NULL_EX (request_table_shr_mem, sizeof(action_request_Shr) * total_request_entries, "request table", -1);
   FREE_AND_MAKE_NULL_EX (http_method_table_shr_mem, sizeof(http_method_t_shr) * total_http_method, "http method table", -1);
   FREE_AND_MAKE_NULL_EX (host_table_shr_mem, sizeof(HostTableEntry_Shr) * total_host_entries, "host table", -1);
  
   for (i = 0; i < total_runprof_entries; i++) 
   {
      actual_num_pages += runprof_table_shr_mem[i].num_pages;
      NSDL4_PARSING(NULL, NULL, "Total number of pages of all scenario groups = %d", actual_num_pages);
   }
   FREE_AND_MAKE_NULL_EX (thinkprof_table_shr_mem, sizeof(ThinkProfTableEntry_Shr) * actual_num_pages, "thinkprof table", -1);
   FREE_AND_MAKE_NULL_EX (inline_delay_table_shr_mem, sizeof(InlineDelayTableEntry_Shr) * actual_num_pages, "inline delaytable", -1);
   FREE_AND_MAKE_NULL_EX (autofetch_table_shr_mem, sizeof(AutoFetchTableEntry_Shr) * actual_num_pages, "autofetch table", -1);
   FREE_AND_MAKE_NULL_EX (pacing_table_shr_mem, sizeof(PacingTableEntry_Shr) * total_runprof_entries, "pacing table", -1);
   FREE_AND_MAKE_NULL_EX (continueOnPageErrorTable_shr_mem, sizeof(ContinueOnPageErrorTableEntry_Shr) * actual_num_pages, "continueOnPageError table", -1);
   FREE_AND_MAKE_NULL_EX (pagereloadprof_table_shr_mem, sizeof(PageReloadProfTableEntry_Shr) * actual_num_pages, "pagereloadprof table", -1);
   FREE_AND_MAKE_NULL_EX (pageclickawayprof_table_shr_mem, sizeof(PageClickAwayProfTableEntry_Shr) * actual_num_pages, "pageclickawayprof table", -1);
//   FREE_AND_MAKE_NULL_EX (searchvar_perpage_table_shr_mem, (total_searchvar_entries * sizeof(SearchVarTableEntry_Shr)) + (total_perpageservar_entries * sizeof(PerPageSerVarTableEntry_Shr)), "perpageservar table", -1);
   FREE_AND_MAKE_NULL_EX (jsonvar_table_shr_mem, (total_jsonvar_entries * sizeof(JSONVarTableEntry_Shr)) + (total_perpagejsonvar_entries * sizeof(PerPageJSONVarTableEntry_Shr)), "perpagejsonvar table", -1);
   FREE_AND_MAKE_NULL_EX (checkpoint_table_shr_mem, (total_checkpoint_entries * sizeof(CheckPointTableEntry_Shr)) + (total_perpagechkpt_entries * sizeof(PerPageChkPtTableEntry_Shr)), "perpagechkpt table", -1);
   FREE_AND_MAKE_NULL_EX (checkReplySizeTable_shr_mem, (total_checkreplysize_entries * sizeof(CheckPointTableEntry_Shr)) + (total_perpagechkrepsize_entries * sizeof(PerPageCheckReplySizeTableEntry_Shr)), "perpagechk replysize table", -1);
   FREE_AND_MAKE_NULL_EX (page_table_shr_mem, WORD_ALIGNED(sizeof(PageTableEntry_Shr) * total_page_entries), "page table", -1);
   FREE_AND_MAKE_NULL_EX (tx_hash_to_index_table_shr_mem, sizeof(int) * total_tx_entries, "tx hash index table", -1);
   FREE_AND_MAKE_NULL_EX (session_table_shr_mem, sizeof(SessTableEntry) * total_sess_entries, "session table", -1);
   FREE_AND_MAKE_NULL_EX (locattr_table_shr_mem, WORD_ALIGNED(total_locattr_entries * sizeof(LocAttrTableEntry_Shr)), "locattr table", -1);
   FREE_AND_MAKE_NULL_EX (sessprof_table_shr_mem, WORD_ALIGNED(sizeof(SessProfTableEntry_Shr) * total_sessprof_entries), "sessprof table", -1);
   //FREE_AND_MAKE_NULL_EX (g_static_file_table_shr, g_static_file_table_total*sizeof(nsFileInfo_Shr), "variable table", -1);
   FREE_AND_MAKE_NULL_EX (inusesvr_table_shr_mem, sizeof(InuseSvrTableEntry_Shr) * total_inusesvr_entries, "inusesvr table", -1);
   FREE_AND_MAKE_NULL_EX (errorcode_table_shr_mem, sizeof(ErrorCodeTableEntry_Shr) * total_errorcode_entries, "errorcode table", -1);
   FREE_AND_MAKE_NULL_EX (clust_table_shr_mem, sizeof(ClustValTableEntry_Shr) * (total_clustvar_entries * total_clust_entries) + sizeof(char*) * total_clustvar_entries, "clust table", -1);
   FREE_AND_MAKE_NULL_EX (rungroup_table_shr_mem, sizeof(GroupValTableEntry_Shr) * (total_groupvar_entries * total_runprof_entries) + sizeof(char*) * total_groupvar_entries, "rungroup table", -1);
   FREE_AND_MAKE_NULL_EX (userprof_table_shr_mem, WORD_ALIGNED(sizeof(UserProfTableEntry_Shr) * total_userprofshr_entries) + WORD_ALIGNED(sizeof(UserProfIndexTableEntry_Shr) * total_userindex_entries), "userprof table", -1);
   FREE_AND_MAKE_NULL_EX(prof_pct_count_table, sizeof(ProfilePctCountTable) * (total_userprofshr_entries + 1), "prof pct count table", -1); 
   FREE_AND_MAKE_NULL_EX(per_proc_vgroup_table, global_settings->num_process * total_group_entries * sizeof(PerProcVgroupTable), "per proc group table", -1);
   FREE_AND_MAKE_NULL_EX(g_static_vars_shr_mem, g_static_vars_shr_mem_size, "static vars", -1);
   FREE_AND_MAKE_NULL_EX(fparamValueTable_shr_mem, sizeof(PointerTableEntry_Shr) * total_fparam_entries, "fileParam Value table", -1);
   FREE_AND_MAKE_NULL_EX(searchvar_table_shr_mem, total_searchvar_entries * sizeof(SearchVarTableEntry_Shr) + total_perpageservar_entries * sizeof(PerPageSerVarTableEntry_Shr), "search var table" , -1);
   FREE_AND_MAKE_NULL_EX(seq_group_next,(total_group_entries * sizeof(int)),"seq group", -1);
}

