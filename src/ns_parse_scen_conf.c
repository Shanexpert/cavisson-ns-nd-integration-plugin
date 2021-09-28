/*********************************************************************************************
* Name                   : ns_parse_scen_conf.c 
* Purpose                : This C file holds the function to parse scenario file keywords. 
* Author                 : Arun Nishad
* Intial version date    : Tuesday, January 27 2009 
* Last modification date : - Monday, July 13 2009 
* SM, 11-02-2014: Parsing for RBU_SCREEN_SIZE_SIM
*********************************************************************************************/

#include <libgen.h>
#include <regex.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pwd.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "nslb_sock.h"
#include "smon.h"
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
#include "ns_msg_com_util.h"
#include "ns_log.h"
#include "timing.h"
#include "tmr.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "ns_auto_cookie.h"
#include "ns_http_process_resp.h"
#include "ns_percentile.h"
#include "ns_debug_trace.h"
#include "ns_custom_monitor.h"
#include "ns_user_monitor.h"
#include "ns_check_monitor.h"
#include "ns_pre_test_check.h"
#include "ns_check_monitor.h"
#include "ns_monitor_profiles.h"
#include "ns_sock_list.h"
#include "ns_wan_env.h"
#include "ns_sock_com.h"
#include "ns_cookie.h"
#include "ns_parse_src_ip.h"
#include "ns_alloc.h"
#include "ns_trans_parse.h"
#include "ns_cpu_affinity.h"
#include "ns_event_log.h"
#include "ns_parse_scen_conf.h"
#include "ns_kw_set_non_rtc.h"
#include "ns_replay_access_logs_parse.h"
#include "ns_replay_access_logs.h"
#include "ns_schedule_phases_parse.h"
#include "ns_schedule_ramp_up_fcu.h"
#include "ns_child_msg_com.h"
#include "ns_schedule_phases_parse.h"
#include "ns_schedule_phases_parse_validations.h"
#include "ns_standard_monitors.h"
#include "ns_trans.h"
#include "ns_dynamic_vector_monitor.h"
#include "ns_smtp_parse.h"
#include "ns_page.h"
#include "ns_pop3_parse.h"
#include "ns_ftp_parse.h"
#include "ns_dns.h"
#include "ns_http_pipelining.h"
#include "ns_keep_alive.h"
#include "ns_event_log.h"
#include "ns_event_id.h"
#include "ns_string.h"
#include "ns_http_cache.h"
//#include "init_cav.h"
#include "ns_js.h"
#include "ns_vuser_ctx.h"
#include "ns_script_parse.h"
#include "ns_nethavoc_handler.h"
#include <sys/stat.h>

#if OPENSSL_VERSION_NUMBER < 0x10100000L
  #include <openssl/ssl_cav.h>
#endif

#include "ns_trans_parse.h"
#include "ns_vuser_trace.h"
#include "tr069/src/ns_tr069_lib.h"
#include "nslb_util.h"
#include "ns_url_hash.h"
#include "ns_auto_fetch_parse.h"
#include "ns_netstorm_diagnostics.h"
#include "ns_session_pacing.h"
#include "ns_lps.h"
#include "ns_page_think_time_parse.h"
#include "ns_nd_kw_parse.h"
#include "ns_dynamic_hosts.h"
#include "ns_runtime_changes_quantity.h"
#include "ns_http_auth.h"
#include "ns_child_thread_util.h"
#include "nslb_cav_conf.h"
#include "ns_parent.h"

#include "ns_trans_parse.h"

#include "nslb_hm_disk_space.h" 
#include "nslb_hm_disk_inode.h"
#include "nslb_ssl_lib.h"

#include "ns_proxy_server.h"
#include "ns_sync_point.h"
#include "ns_connection_pool.h"
#include "ns_network_cache_reporting.h"
#include "ns_monitoring.h"
#include "ns_dns_reporting.h"
#include "ns_trace_level.h"
#include "ns_parse_netcloud_keyword.h"
#include "ns_njvm.h"
#include "ns_rbu.h"
#include "nia_fa_function.h"
#include "ns_send_mail.h"
#include "ns_group_data.h"
#include "ns_ldap.h"
#include "ns_imap.h"
#include "ns_batch_jobs.h"
#include "ns_server_admin_utils.h"
#include "ns_runtime_changes.h"
#include "ns_jrmi.h"
#include "ns_common.h"
#include "ns_coherence_nid_table.h"
#include "ns_page_based_stats.h"
#include "ns_continue_on_page.h"
#include <sys/sysinfo.h>
#include "ns_inline_delay.h"
#include "ns_runtime_runlogic_progress.h"
#include "ns_ndc_outbound.h"
#include "ns_auto_scale.h"
#include "ns_server_ip_data.h"
#include "ns_param_override.h"
#include "ns_schedule_fcs.h"
#include "JSON_checker.h"
#include "ns_progress_report.h"
#include "ns_uri_encode.h"
#include "ns_runtime_changes_monitor.h"
#include "ns_appliance_health_monitor.h"
#include "ns_jmeter.h"
#include "ns_custom_monitor_RDnew.h"
#include "ns_monitor_metric_priority.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"
#include "ns_test_monitor.h"

static void kw_enable_gen_tr(char *buf); 
static void kw_check_partition_overtime(char *buf); 
static void kw_set_dbu_chunk_size(char *buf, int old_keyword_flag);
static void kw_set_partition_switch_delay_time(char *buf);
static void kw_set_db_idle_time(char *buf);
static void kw_set_db_num_cycles(char *buf);
static void validate_dbu_tmp_file_path();
int kw_set_tcp_keepalive(char *buff);
extern int loader_opcode;
extern int get_max_report_level_from_non_shr_mem();
int kw_set_tcp_keepalive(char *buff);
int kw_set_nd_enable_data_validation(char *keyword, char *buf, char *err_msg, int runtime);
int g_parent_epoll_event_size = 0;
int g_mssql_data_buf_size=512*1024;
int monitor_debug_level;
int enable_test_metrics_mode = 0;
int process_monitoring_flag = 0;

/*For checking 6th field of SGRP keyword i.e type of script field
  Before type of script can only be 0(script) or 1(url), now for java-type script will have 10 and 11 options*/
extern int kw_set_mon_log_trace_level(char *, char *);
int g_script_or_url = 0;
int grp_flag = 1;
#define MAX_TRACING_SCRATCH_BUFFER_SIZE  1048576 /* 1*1024*1024 = 1mb*/

static int kw_set_progress_msecs(char *text, char *buf, char *err_msg, int runtime_flag)
{
  int time = 0;
  time = atoi(text);

  if ( time == 0 ) {
    NSTL1(NULL, NULL, "Progress report time interval is %d. Setting PROGRESS_MSECS According to Product.", time);
  if (!strcmp (g_cavinfo.config, "NDE")) {
      if (!strcmp (g_cavinfo.SUB_CONFIG, "NVSM")) 
        global_settings->progress_secs = PROGRESS_INTERVAL_NDE_NVSM;
      else 
        global_settings->progress_secs = PROGRESS_INTERVAL_NDE;
    } else if(!((strncmp(g_cavinfo.config, "NV", 2)) || !((strncmp(g_cavinfo.SUB_CONFIG, "NV", 2))) || !((strncmp(g_cavinfo.SUB_CONFIG, "ALL", 3))))) {
        global_settings->progress_secs = PROGRESS_INTERVAL_NV;
    } else if (!strcmp (g_cavinfo.config, "NO")) {
      global_settings->progress_secs = PROGRESS_INTERVAL_NO;
    } else if (!strcmp (g_cavinfo.config, "ED")) {
      global_settings->progress_secs = PROGRESS_INTERVAL_ED;
    } else {
      if(loader_opcode == STAND_ALONE)
        global_settings->progress_secs = PROGRESS_INTERVAL_NS;
      else
        global_settings->progress_secs = PROGRESS_INTERVAL_NC;
    }
    NSTL1(NULL, NULL, "Progress report time interval is %d According to Product type.", global_settings->progress_secs); 
    NS_DUMP_WARNING("Progress report time interval is %d. Setting PROGRESS_MSECS '%d' according to Product '%s'.", time, global_settings->progress_secs, g_cavinfo.config);
  }
    else if ( time >= 1000 ) {
    global_settings->progress_secs = time;
  } else {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, PROGRESS_MSECS_USAGE, CAV_ERR_1011021, "");
  }
  init_kbps_factor(); //Formula to calculate throughput
  return 0;
}


static int sess_prof_comp(const void* prof1, const void* prof2) {
  NSDL2_MISC(NULL, NULL, "Method called");
  if (((SessProfTableEntry *) prof1)->sessprofindex_idx > ((SessProfTableEntry *) prof2)->sessprofindex_idx)
    return 1;
  else if  (((SessProfTableEntry *) prof1)->sessprofindex_idx < ((SessProfTableEntry *) prof2)->sessprofindex_idx)
    return -1;
  else if (((SessProfTableEntry *) prof1)->pct < ((SessProfTableEntry *)prof2)->pct)
    return 1;
  else if (((SessProfTableEntry *) prof1)->pct > ((SessProfTableEntry *)prof2)->pct)
    return -1;
  else return 0;
}

static int sort_sessprof_tables(void) {
  int i;
  int total_pct;
  int sessprof_idx;

  NSDL2_MISC(NULL, NULL, "Method called");
  qsort(sessProfTable, total_sessprof_entries, sizeof(SessProfTableEntry), sess_prof_comp);

  for (i = 0; i < total_sessprof_entries; i++) {
    if (sessProfIndexTable[sessProfTable[i].sessprofindex_idx].sessprof_start == -1) {
      sessProfIndexTable[sessProfTable[i].sessprofindex_idx].sessprof_start = i;
      sessProfIndexTable[sessProfTable[i].sessprofindex_idx].sessprof_length++;
    }
    else
      sessProfIndexTable[sessProfTable[i].sessprofindex_idx].sessprof_length++;
  }

  total_pct = 0;

  for (i = 0; i < total_sessprofindex_entries; i++) {
    for (sessprof_idx = 0; sessprof_idx < sessProfIndexTable[i].sessprof_length; sessprof_idx++) {
      total_pct += sessProfTable[sessProfIndexTable[i].sessprof_start + sessprof_idx].pct;
      sessProfTable[sessProfIndexTable[i].sessprof_start + sessprof_idx].pct = total_pct;
    }
    if (total_pct != 100) {
      NSTL1_OUT(NULL, NULL,  "SessProf %s does not add up to 100\n", RETRIEVE_BUFFER_DATA(sessProfIndexTable[sessProfTable[i-1].sessprofindex_idx].name));
      return -1;
    }
    total_pct = 0;
  }
  return 0;
}

static void kw_set_seg_mode_usage(char *err_msg, char *buf)
{
  NSTL1_OUT(NULL, NULL,  "Error: Invalid value of AMF_SEGMENT_MODE  keyword: %s\n", err_msg);
  NSTL1_OUT(NULL, NULL,  "       Line %s\n", buf);
  NSTL1_OUT(NULL, NULL,  "  Usage: AMF_SEGMENT_MODE <mode>\n");
  NSTL1_OUT(NULL, NULL,  "  Where:\n");
  NSTL1_OUT(NULL, NULL,  "         mode : Is used to specify the mode which we want to use segmentation of amf\n");
  NS_EXIT(-1, "%s\nUsage: AMF_SEGMENT_MODE <mode>", err_msg);
}

/* This method is used to parse and set keyword AMF_SEGMENT_MODE. This keyword has a single argument mode. Mode uses will be:
 * 0: Do amf segmentation as we were doing earlier
 * 1: Do amf segmentation as hessian segmentation is done
 * */
int kw_set_amf_seg_mode(char *buf)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH];
  int mode;
  int num;

  NSDL2_PARSING(NULL, NULL, "Method called, buf = %s", buf);

  num = sscanf(buf, "%s %d %s", keyword, &mode, tmp);

  if (num != 2){
    kw_set_seg_mode_usage("Invaid number of arguments", buf);
  }

  if (mode < 0 || mode > 1)
   kw_set_seg_mode_usage("Mode can have value 0 or 1", buf);

  global_settings->amf_seg_mode = mode;
  NSDL2_PARSING(NULL, NULL, "global_settings->amf_seg_mode  = %d", global_settings->amf_seg_mode);
  return 0;
}

static void kw_set_disable_script_validation_usage(char *err_msg, char *buf)
{
  NSTL1_OUT(NULL, NULL,  "Error: Invalid value of DISABLE_SCRIPT_VALIDATION keyword: %s\n", err_msg);
  NSTL1_OUT(NULL, NULL,  "       Line %s\n", buf);
  NSTL1_OUT(NULL, NULL,  "  Usage: DISABLE_SCRIPT_VALIDATION <mode>\n");
  NSTL1_OUT(NULL, NULL,  "  Where:\n");
  NSTL1_OUT(NULL, NULL,  "        mode) 0 :used to enable syntax error on NSApi\n");
  NSTL1_OUT(NULL, NULL,  "              1 :used to disable above\n");
  NS_EXIT(-1, "%s\nUsage: DISABLE_SCRIPT_VALIDATION <mode>", err_msg);
}

/* This method is used to parse and set keyword DISABLE_SCRIPT_VALIDATION. This keyword has a single argument mode. Mode uses will be:
0: Enable script validation
1: Disable script validation 
Default value 0*/
int kw_set_disable_script_validation(char *buf)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH];
  int mode;
  int num;

  NSDL2_PARSING(NULL, NULL, "Method called, buf = %s", buf);

  num = sscanf(buf, "%s %d %s", keyword, &mode, tmp);

  if (num != 2){
    kw_set_disable_script_validation_usage("Invaid number of arguments", buf);
  }

  if (mode < 0 || mode > 1)
    kw_set_disable_script_validation_usage("Mode can have value 0 or 1", buf);

  global_settings->disable_script_validation = mode;
  NSDL2_PARSING(NULL, NULL, "Method called, disable_script_validation = %d", global_settings->disable_script_validation);

  return 0;
}

/* This method is used to parse and set keyword DISABLE_SCRIPT_COPY_TO_TR. This keyword has a single argument mode. Mode uses will be:
0: Copy script to TR
1: Do not copy script to TR
2: Copy script dir, not subdir in script like dump...etc */
int kw_set_disable_copy_script(char *buf, int runtime_flag, char *err_msg)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH];
  int mode; 
  int num;
  
  NSDL2_PARSING(NULL, NULL, "Method called, buf = %s", buf);

  num = sscanf(buf, "%s %d %s", keyword, &mode, tmp);
 
  if (num != 2){
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, DISABLE_SCRIPT_COPY_TO_TR_USAGE, CAV_ERR_1011069, CAV_ERR_MSG_1);
  }
  
  if (mode < 0 || mode > 2)
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, DISABLE_SCRIPT_COPY_TO_TR_USAGE, CAV_ERR_1011069, CAV_ERR_MSG_3);

  global_settings->script_copy_to_tr = mode;

  return 0;
}

static void validate_no_validation_keyword()
{
  NSDL2_SCHEDULE(NULL, NULL, "Method called");

  int i;  
  for (i = 0; i < total_runprof_entries; i++) {
    if (runProfTable[i].gset.no_validation) {  
      if (group_default_settings->trace_level == 4) 
        NS_DUMP_WARNING("NO_VALIDATION and TRACING are conflicting with each other. NO_VALIDATION has higher priority than TRACING. If you want TRACING to be effective, please disable NO_VALIDATION.");

      break;                    /* No need to go further */
    }
  }
}

static void validate_script_mode_and_set_defaults(GroupSettings *gset, int script_type, char *script_name)
{
  
  NSDL2_PARSING(NULL, NULL, "Method Called. Script = %s, Mode = %d, script_type = %d", script_name, gset->script_mode, script_type);

  if(script_type == NS_SCRIPT_TYPE_LEGACY && gset->script_mode != NS_SCRIPT_MODE_LEGACY)
  {
    //NSTL1_OUT(NULL, NULL,  "Warning: Script execution mode set for c script '%s' is not compatible. Setting mode to legacy execution mode.\n", script_name);
    NSTL1(NULL,NULL, "Warning: Script execution mode set for c script '%s' is not compatible. Setting mode to legacy execution mode.", script_name);
    gset->script_mode = NS_SCRIPT_MODE_LEGACY;
  }
  else if(script_type == NS_SCRIPT_TYPE_C && (gset->script_mode != NS_SCRIPT_MODE_USER_CONTEXT && gset->script_mode != NS_SCRIPT_MODE_SEPARATE_THREAD))
  {
    gset->script_mode = NS_SCRIPT_MODE_USER_CONTEXT;
#ifdef NS_DEBUG_ON
    gset->stack_size = NS_MIN_STACK_SIZE_FOR_DEBUG*1024; // Min 256 KB stack size is need for debug
#else
    //TODO: increase stack size for RBU
    gset->stack_size = NS_DEFAULT_STACK_SIZE_FOR_NON_DEBUG*1024; // 16 KB stack size
    
#endif
    //NSTL1_OUT(NULL, NULL,  "Warning: Script execution mode set for legacy type script '%s' is not compatible. Setting mode to execution in user context mode\n", script_name);
    NSTL1(NULL,NULL, "Warning: Script execution mode set for legacy type script '%s' is not compatible. Setting mode to execution in user context mode", script_name);
    NS_DUMP_WARNING("Script execution mode set for legacy type script '%s' is not compatible. Setting mode to execution in user context mode", script_name);
  }
  else if(script_type == NS_SCRIPT_TYPE_JAVA)
  //  && gset->script_mode != NS_SCRIPT_MODE_SEPARATE_PROCESS)
  {
    // Commenting this code as script mode do not have any significance for java type script and we are setting it seprate process so that
    // we can pass conditions for non legacy scripts
    // NSTL1_OUT(NULL, NULL,  "Warning: Script execution mode set for other language script '%s' is not compatible. Setting mode to execution in separate process mode.\n", script_name);
    gset->script_mode = NS_SCRIPT_MODE_SEPARATE_PROCESS;
  }
  if((gset->script_mode) == NS_SCRIPT_MODE_USER_CONTEXT){
#ifdef NS_DEBUG_ON
    if((gset->stack_size) < (NS_MIN_STACK_SIZE_FOR_DEBUG*1024))
    {
      gset->stack_size = NS_MIN_STACK_SIZE_FOR_DEBUG*1024;
    /*  NSTL1_OUT(NULL, NULL,  "Warning: stack size is not enough to run the test in debug mode. Stack size must be greater or equal to %d. Setting stack size to %d KB\n",
                                                                                               NS_MIN_STACK_SIZE_FOR_DEBUG, gset->stack_size/1024);*/
      if(grp_flag == 1)
      {
          NSTL1(NULL, NULL, "Warning: stack size is not enough to run the test in debug mode. Stack size must be greater or equal to %d. " 
                        "Setting stack size to %d KB", NS_MIN_STACK_SIZE_FOR_DEBUG, gset->stack_size/1024);

          NS_DUMP_WARNING("Stack size is not enough to run the test in debug mode. Stack size must be greater or equal to %d. " 
                      "Setting stack size to %d KB", NS_MIN_STACK_SIZE_FOR_DEBUG, gset->stack_size/1024);
          grp_flag = 0;
      } 
    }
#else
    /* RBU - If group is running as RBU and stack size is less than 20K then forcefully set it to 20K otherwise will core dumpling*/
    NSDL2_PARSING(NULL, NULL, "Setting stck size - enable_rbu = %d, stack_size = %d", gset->rbu_gset.enable_rbu, set->stack_size);
    if(gset->rbu_gset.enable_rbu)
    {
      if((gset->stack_size) < (NS_RBU_STACK_SIZE_FOR_NON_DEBUG * 1024))
      {
        gset->stack_size = NS_RBU_STACK_SIZE_FOR_NON_DEBUG * 1024; 
      }
    }
    else if(g_script_or_url == JMETER_TYPE_SCRIPT)
    {
      if((gset->stack_size) < (NS_MIN_JMETER_STACK_SIZE*1024))
      {
        gset->stack_size = NS_MIN_JMETER_STACK_SIZE*1024;
      }
    }
    else
    {
      if((gset->stack_size) < (NS_MIN_STACK_SIZE_FOR_NON_DEBUG*1024))
      {
        gset->stack_size = NS_MIN_STACK_SIZE_FOR_NON_DEBUG*1024;
        /*NSTL1_OUT(NULL, NULL,  "Warning: stack size is not enough to run the test. Stack size must be greater or equal to %d. Setting stack size to %d KB\n",
                                                                                        NS_MIN_STACK_SIZE_FOR_NON_DEBUG, gset->stack_size/1024); */
        NSTL1(NULL,NULL, "Warning: stack size is not enough to run the test. Stack size must be greater or equal to %d. "
                         "Setting stack size to %d KB", NS_MIN_STACK_SIZE_FOR_NON_DEBUG, gset->stack_size/1024);
 
        NS_DUMP_WARNING("Stack size is not enough to run the test in non debug mode. Stack size must be greater or equal to %d. "
                        "Setting stack size to %d KB", NS_MIN_STACK_SIZE_FOR_NON_DEBUG, gset->stack_size/1024);
      }    
   }
#endif
  NSDL2_PARSING(NULL, NULL, "stack_size is %d", gset->stack_size/1024); 
  }

}

static void validate_click_away(RunProfTableEntry *runProfTable)
{
  int i;
  if(runProfTable->gset.script_mode != NS_SCRIPT_MODE_LEGACY) 
  {
    for (i = 0; i < runProfTable->num_pages; i++) {
      int idx = runProfTable->page_clickaway_table[i] == -1 ? 0 : runProfTable->page_clickaway_table[i];
      NSDL4_MISC(NULL, NULL, "runProfTable->page_reload_table[%d] = %d, idx = %d, pageClickAwayProfTable[idx].clicked_away_on = %d",
                 i, runProfTable->page_reload_table[i], idx, pageClickAwayProfTable[idx].clicked_away_on);

      if(pageClickAwayProfTable[idx].clicked_away_on != -1)
      {
        NS_EXIT(-1, "G_CLICK_AWAY keyword is invalid. Argument #3 (Clicked Away page) should be -1 in C type script");
      }

      if(pageClickAwayProfTable[idx].call_check_page != 0)
      {
        NS_EXIT(-1, "G_CLICK_AWAY keyword is invalid. Argument #6 (Call check page) should be 0 in C type script");
      }
    }
  }
}

static void user_data_check() {
  int i;
  //struct  hostent *hp;
  int goal_based = 1;
  //int found_interface = 0;
  //int config_interface = 0;
  //char read_buf[MAX_LINE_LENGTH];
  //FILE* eth_bytes_fp;

  NSDL2_SCHEDULE(NULL, NULL, "Method called");
  //Convert user-prof name ti user-prof-index
  for (i = 0; i < total_runprof_entries; i++) {
	char *uprof_name =  RETRIEVE_TEMP_BUFFER_DATA(runProfTable[i].userprof_name);
	int idx;
	if ((idx = find_userindex_idx(uprof_name)) == -1) {
          NS_EXIT(-1, "read_keywords(): Unknown user profile %s", uprof_name);
	}
	runProfTable[i].userprof_idx = idx;

    // Check if script execution mode and script type for each scenario grp are correct
    int sidx = runProfTable[i].sessprof_idx; // Index in session table for this grp
    validate_script_mode_and_set_defaults(&(runProfTable[i].gset), gSessionTable[sidx].script_type, 
        get_sess_name_with_proj_subproj_int(RETRIEVE_BUFFER_DATA(gSessionTable[sidx].sess_name), sidx, "/"));
        //Previously taking with only script name
        //get_sess_name_with_proj_subproj(RETRIEVE_BUFFER_DATA(gSessionTable[sidx].sess_name)));
    validate_click_away(&runProfTable[i]);

    if ((runProfTable[i].gset.use_same_netid_src) && (total_ip_entries == 0)) {
      NS_EXIT(-1, "USE SAME NETID mode is enabled but no source IPs being defined, runProfTable[i].gset.use_same_netid_src = %d",
                   runProfTable[i].gset.use_same_netid_src);
    }
 
    if ((runProfTable[i].gset.use_same_netid_src) && (runProfTable[i].gset.src_ip_mode != 0)) {
        NS_EXIT(-1, "Both Use Same NetID and Unique IP mode is enabled");
    }
 
    if(global_settings->high_perf_mode && runProfTable[i].gset.src_ip_mode && 
       (total_ip_entries < global_settings->max_sock)) {
      NS_EXIT(-1, "IP entries (%d) are less than sockets (%d) to be created in High Performance mode with Unique IP Mode", 
             total_ip_entries, global_settings->max_sock);
    }
  }
  if ((global_settings->test_stab_time) && (global_settings->num_fetches)) {
    global_settings->num_fetches = 0;
    NSTL1(NULL, NULL, "Both SECONDS or NUM_FETCHES have been provided, ignoring NUM_FETCHES\n");
  }

/**
  if (global_settings->num_fetches)
	global_settings->account_all = 1;

  if (global_settings->account_all)
  	global_settings->account_all = 1;
*/

#if 0
  if ((global_settings->user_cleanup_pct < 0) || (global_settings->user_cleanup_pct > 100)) {
    global_settings->user_cleanup_pct = 100;
    printf("USER_CLEANUP_MSECS pct has to be between 0-100, setting it to 100\n");
  }

#endif
  if (global_settings->ns_factor <= 0) {
    global_settings->ns_factor = 8;
  }
#if 0 
/* Commented code, we will be using global_settings->max_con_per_vuser
 * to set browser entries for particular vuser */
  if (global_settings->max_con_per_vuser < 0) {
    global_settings->max_con_per_vuser = UA_MAX_PARALLEL;
    printf("MAX_CON_PER_VUSER is set to %d. it has to be > 0\n", global_settings->max_con_per_vuser);
  }  else if (global_settings->max_con_per_vuser == 0)
    global_settings->max_con_per_vuser = UA_MAX_PARALLEL;
#endif
#ifdef RMI_MODE   /* hard code the max_con_per_vuser to 1 for RMI_MODE */
  global_settings->max_con_per_vuser = 1;
#endif

  /*  if ((global_settings->cmax_parallel <= 0) || (global_settings->cmax_parallel > global_settings->max_con_per_vuser)) {
    global_settings->cmax_parallel = global_settings->max_con_per_vuser;
    if (global_settings->debug >= 2)
      printf("CMAX_PARALLEL is set to %d. It has to be > 0 & <= MAX_CON_PER_VUSER\n", global_settings->cmax_parallel);
  }

  if ((global_settings->per_svr_max_parallel <= 0) || (global_settings->per_svr_max_parallel > global_settings->max_con_per_vuser)
	  || (global_settings-> per_svr_max_parallel > global_settings->cmax_parallel)) {
    global_settings->per_svr_max_parallel = global_settings->cmax_parallel;
    if (global_settings->debug >= 2)
      printf("PER_SVR_MAX_PARALLEL is set to %d. It has to be > 0 & <= MAX_CON_PER_VUSER and <= CMAX_PARALLEL\n",
	     global_settings->per_svr_max_parallel);
	     } */
  ssl_data_check(); //check data conditions for ssl

#if 0
  if (!global_settings->use_pct_prof) {
    if (testCase.mode != TC_FIX_CONCURRENT_USERS) {
      global_settings->use_pct_prof = 1;
      //printf("user_data_check(): use_pct_prof can only be 0 for test case mode FIX_CONCURRENT_USERS, forcing to 1\n");
    }
  }
#endif

  if (global_settings->interactive) {
      printf("Netstorn in Interactive Mode\n");
      if (total_runprof_entries != 1) {
	NS_EXIT(-1, "In Interactive mode, Only one scenario group must be provided");
      }
      NSTL1(NULL, NULL, "max_con_per_vuser setting to 1\n");
      global_settings->max_con_per_vuser = 1;
      if (testCase.mode != TC_FIX_CONCURRENT_USERS) {
          testCase.mode = TC_FIX_CONCURRENT_USERS;
	  NSTL1(NULL, NULL, "Forcing the Scenrio Mode to 'FIX_CONCURRENT_USERS with 1 user'\n");
      }
      //global_settings->use_pct_prof = 0;
      runProfTable[0].quantity = 1;
      for (i=0; i<total_thinkprof_entries; i++) {
	thinkProfTable[i].mode = 2; //Make it constant
	if (thinkProfTable[i].avg_time < 2000)
	   thinkProfTable[i].avg_time = 2000;
      }

      for (i=0; i<total_pacing_entries;i++){
	  pacingTable[i].pacing_mode = 1;
	  pacingTable[i].refresh = 0;
	  pacingTable[i].first_sess = 0;
	  pacingTable[i].think_mode = 0;
	  pacingTable[i].retain_param_value = 0;
	  if (pacingTable[i].time < 2000)
	      pacingTable[i].time = 2000;
      }
  }

  switch (testCase.mode) {
  case TC_MIXED_MODE:  /*Fix done for bug#4375, for mix-mode following vars were not set*/
    goal_based = 0;  /*disabling goal based init settings*/
    global_settings->load_key = 0; /*In mix mode we have both FSR and FCU, like FCU & FSR we are making load_key 0*/
    global_settings->user_reuse_mode = 0; /*Var need to be 0, as per FSR*/
    break;
  
  case TC_FIX_USER_RATE: /*Fix Session Rate(FSR)*/
    goal_based = 0;
    global_settings->load_key = 0;
    global_settings->user_reuse_mode = 0;
/*
    if (global_settings->user_rate_mode  == 0)
      global_settings->num_user_mode = NUM_USER_MODE_SESSION_RATE_CONST;
    else if (global_settings->user_rate_mode  == 1)
      global_settings->num_user_mode = NUM_USER_MODE_SESSION_RATE_RANDOM;
    else {
      global_settings->num_user_mode = NUM_USER_MODE_SESSION_RATE_CONST;
      global_settings->user_rate_mode = 0;
      printf("user_data_check(): USER_RATE_MODE must be 0 or 1. Setting it to 0\n");
    }
*/
    if ((global_settings->schedule_type != SCHEDULE_TYPE_ADVANCED) ||
        (global_settings->use_prof_pct != PCT_MODE_PCT)) {
      if (global_settings->vuser_rpm <= 0 && global_settings->replay_mode == 0) {
        NS_EXIT(-1, CAV_ERR_1011173, CAV_ERR_MSG_11);
      }
    }
    break;

  case TC_FIX_MEAN_USERS:
    global_settings->load_key = 0;
    global_settings->user_reuse_mode = 0;
/*
    if (global_settings->user_rate_mode  == 0)
      global_settings->num_user_mode = NUM_USER_MODE_SESSION_RATE_CONST;
    else if (global_settings->user_rate_mode  == 1)
      global_settings->num_user_mode = NUM_USER_MODE_SESSION_RATE_RANDOM;
    else {
      global_settings->num_user_mode = NUM_USER_MODE_SESSION_RATE_CONST;
      global_settings->user_rate_mode = 0;
      printf("user_data_check(): USER_RATE_MODE must be 0 or 1. Setting it to 0\n");
    }
*/
    //just   in case. shuld not needed though
//if((global_settings->num_connections == 0) && (global_settings->use_prof_pct == PCT_MODE_PCT)) global_settings->num_connections = 1;
//    if(global_settings->use_prof_pct == PCT_MODE_PCT && global_settings->schedule_type != SCHEDULE_TYPE_ADVANCED) {

    if (global_settings->num_connections <= 0) {
      NS_EXIT(-1, CAV_ERR_1011270, CAV_ERR_MSG_9);
    }
    break;

  case TC_FIX_CONCURRENT_USERS:
    goal_based = 0;
    global_settings->load_key = 1;
/*     if (global_settings->ramp_up_mode == RAMP_UP_MODE_2_SEC_STEP) */
/*       global_settings->num_user_mode = NUM_USER_MODE_2_SEC_STEP; */
/*     else if (global_settings->ramp_up_mode == RAMP_UP_MODE_LINEAR) */
/*       global_settings->num_user_mode = NUM_USER_MODE_LINEAR; */
/*     else if (global_settings->ramp_up_mode == RAMP_UP_MODE_RANDOM) */
/*       global_settings->num_user_mode = NUM_USER_MODE_RANDOM; */
/*     else if (global_settings->ramp_up_mode == RAMP_UP_MODE_IMMEDIATE) */
/*       global_settings->num_user_mode = 0; */
/*     else if (global_settings->ramp_up_mode == RAMP_UP_MODE_STEP) */
/*       global_settings->num_user_mode = NUM_USER_MODE_STEP; */
/*     else { */
/*       global_settings->num_user_mode = NUM_USER_MODE_LINEAR; */
/*       global_settings->ramp_up_mode = RAMP_UP_MODE_LINEAR; */
/*       printf("user_data_check(): RAMP_UP_MODE must be between 0-3. Setting it to 0\n"); */
/*     } */
    global_settings->user_reuse_mode = 1;


   // Ajeet:bug 313 fixed 
//   if(global_settings->use_prof_pct == PCT_MODE_PCT ) {
   if(global_settings->use_prof_pct == PCT_MODE_PCT && global_settings->schedule_type != SCHEDULE_TYPE_ADVANCED) {
      if(global_settings->num_connections <= 0){
        NS_EXIT(-1, CAV_ERR_1011269, CAV_ERR_MSG_9);
      }else if(global_settings->num_connections < total_runprof_entries){
        NS_EXIT(-1, CAV_ERR_1011269, "Value should be greater than or equal to number of scenario groups.");
      }
    }


    //just   in case. shuld not needed though
    //if ((global_settings->num_connections == 0) && (global_settings->use_pct_prof == 0)) global_settings->num_connections = 1;
/*     if (global_settings->num_connections <= 0) { */
/*       printf("user_data_check(): NUM_USERS must be set to >0 \n"); */
/*       exit(1); */
/*     } */
/*     if (global_settings->ramp_up_rate < 0) { */
/*       printf("user_data_check(): RAMP_UP_RATE must be >=0 Setting it to %d per Minute\n", DEFAULT_RAMP_UP_RATE); */
/*       global_settings->ramp_up_rate = DEFAULT_RAMP_UP_RATE; */
/*     } */
    break;

  default:
    switch (global_settings->load_key) {
    default:
      NSTL1(NULL, NULL, "user_data_check(): LOAD_KEY must be 0 or 1. Setting it to 0\n");
      global_settings->load_key = 0;
    case 0:
      global_settings->user_reuse_mode = 0;
/*
      if (global_settings->user_rate_mode  == 0)
	global_settings->num_user_mode = NUM_USER_MODE_SESSION_RATE_CONST;
      else if (global_settings->user_rate_mode  == 1)
	global_settings->num_user_mode = NUM_USER_MODE_SESSION_RATE_RANDOM;
      else {
	printf("user_data_check(): USER_RATE_MODE must be 0 or 1. Setting it to 0\n");
	global_settings->num_user_mode = NUM_USER_MODE_SESSION_RATE_CONST;
	global_settings->user_rate_mode = 0;
      }
*/
      break;
    case 1:
/*       if (global_settings->ramp_up_mode  == RAMP_UP_MODE_2_SEC_STEP) */
/* 	global_settings->num_user_mode = NUM_USER_MODE_2_SEC_STEP; */
/*       else if (global_settings->ramp_up_mode  == RAMP_UP_MODE_LINEAR) */
/* 	global_settings->num_user_mode = NUM_USER_MODE_LINEAR; */
/*       else if (global_settings->ramp_up_mode == RAMP_UP_MODE_RANDOM) */
/* 	global_settings->num_user_mode = NUM_USER_MODE_RANDOM; */
/*       else if (global_settings->ramp_up_mode == RAMP_UP_MODE_IMMEDIATE) */
/* 	global_settings->num_user_mode = 0; */
/*       else if (global_settings->ramp_up_mode == RAMP_UP_MODE_STEP) */
/*         global_settings->num_user_mode = NUM_USER_MODE_STEP; */
/*       else { */
/* 	global_settings->num_user_mode = NUM_USER_MODE_LINEAR; */
/* 	global_settings->ramp_up_mode = RAMP_UP_MODE_LINEAR; */
/* 	printf("user_data_check(): RAMP_UP_MODE must be between 0-3. Setting it to 0\n"); */
/*       } */
      global_settings->user_reuse_mode = 1;
/*       if (global_settings->ramp_up_rate < 0) { */
/* 	printf("user_data_check(): RAMP_UP_RATE must be >=0. Setting it to %d per Minute\n", DEFAULT_RAMP_UP_RATE); */
/* 	global_settings->ramp_up_rate = DEFAULT_RAMP_UP_RATE; */
/*       } */
      break;
    }

    switch (testCase.mode) {
    case TC_MEET_SLA:
      if (total_sla_entries == 0) {
	printf("Must enter in SLA entries\n");
	exit(1);
      }
      break;
    case TC_MEET_SERVER_LOAD:
      if (total_metric_entries == 0) {
	printf("Must enter in METRIC entries\n");
	exit(1);
      }
      break;
    default:
      if (testCase.target_rate <= 0) {
	printf("TARGET_RATE must be greater than 0\n");
	exit(1);
      }
    }
  }

  if (goal_based) {
    validate_capacity_check();
    validate_guess_type();
    validate_stabilize();
  }

  if ((global_settings->display_report < 0) || (global_settings->display_report > 2)) {
    global_settings->display_report = 0;
    NSDL2_MISC(NULL, NULL, "user_data_check(): forced global_settings->display_report to 0");
  }

  if ((global_settings->exclude_failed_agg < 0) || (global_settings->exclude_failed_agg > 1)) {
    global_settings->exclude_failed_agg = 0;
    NSDL2_MISC(NULL, NULL, "user_data_check(): forced global_settings->exclude_fail_agg to 0");
  }

#ifdef RMI_MODE
  if (global_settings->conn_retry_time < 0) {
    global_settings->conn_retry_time = 250;
    NSDL2_MISC(NULL, NULL, "user_data_check(): forced global_settings->conn_retry_time to %d", global_settings->conn_retry_time);
  }
#endif

#if 0
  //Check if interface has been set in the config file
  if (strlen(global_settings->eth_interface))
      config_interface = 1;

  if ((eth_bytes_fp = fopen("/proc/net/dev", "r")) == NULL) {
    NSTL1_OUT(NULL, NULL,  "Error in opening file '/proc/net/dev'\n");
    perror("fopen");
    global_settings->eth_interface[0] = 0;
  } else {
    while (fgets(read_buf, MAX_LINE_LENGTH, eth_bytes_fp)) {
      char* read_ptr, *dptr;
      unsigned int data;
      read_ptr = read_buf;
      while (*read_ptr == ' ') read_ptr++;
      if ((dptr = strchr(read_ptr, ':'))) {
	dptr[0] = 0;
	dptr++;
      } else
	continue;

      if (config_interface) {
      	if ((strncmp(read_ptr, global_settings->eth_interface, strlen("global_settings->eth_interface")) == 0) &&
	  strlen(global_settings->eth_interface) == strlen(read_ptr)) {
	  found_interface = 1;
	  break;
      	}
      } else { //Interface is set in config file, so discover it
        //Find any interface starting with name eth.
      	if (strncmp(read_ptr, "eth", 3) == 0)  {
	    if ((sscanf (dptr, "%lu", &data)) && (data != 0)) {
	    	strcpy(global_settings->eth_interface, read_ptr);
	    	found_interface++;
      	    }
	}
      }
    }

    CLOSE_FP(eth_bytes_fp);

    if (!found_interface) {
      if (config_interface)
          NSTL1_OUT(NULL, NULL,  "user_data_check(): Unable to locate  defined '%s' interface. Ether bytes will not be reported \n",
 global_settings->eth_interface);
     else
          NSTL1_OUT(NULL, NULL,  "user_data_check(): Unable to discover eth interface for ether byte reporting, Set it in the Secnarion\n");
      global_settings->eth_interface[0] = 0;
    } else if (found_interface == 1) {
      if (!config_interface)
          NSTL1_OUT(NULL, NULL,  "INFO: Discovered '%s' ether interface for ether byte reporting\n", global_settings->eth_interface);
    } else {
      NSTL1_OUT(NULL, NULL,  "user_data_check(): Discovered multiple eth interfaces for ether byte reporting, Set appropriate one  in the Secnarion configuration\n");
      global_settings->eth_interface[0] = 0;
    }
  }
#endif

  /* Verify below :BHAV:TODO */
/*   if ((global_settings->report_mask <= 0) || (global_settings->report_mask > (URL_REPORT | PAGE_REPORT | TX_REPORT | SESS_REPORT | VUSER_REPORT))) { */
/*     global_settings->report_mask = (URL_REPORT | SMTP_REPORT | POP3_REPORT | PAGE_REPORT | TX_REPORT | SESS_REPORT | VUSER_REPORT); */
/*     printf("user_data_check(): report_mask can only be between 0 and 15, forcing to log all reports\n"); */
/*   } */

  validate_replay_access_log_settings();
  validate_percentile_settings();
  validate_run_phases_keyword();
  validate_no_validation_keyword();
  validate_pipeline_keyword(); 
  validate_dbu_tmp_file_path();
}



static int create_clust_table_entry(int* row_num) {
  NSDL2_MISC(NULL, NULL, "Method called");
  if (total_clust_entries == max_clust_entries) {
    MY_REALLOC_EX (clustTable, (max_clust_entries + DELTA_CLUST_ENTRIES) * sizeof(ClustTableEntry), max_clust_entries * sizeof(ClustTableEntry),"clustTable", -1);
    if (!clustTable) {
      NSTL1_OUT(NULL, NULL,  "create_clust_table_entry(): Error allocating more memory for clusttable entries\n");
      return (FAILURE);
    } else max_clust_entries += DELTA_CLUST_ENTRIES;
  }
  *row_num = total_clust_entries++;
  return (SUCCESS);
}

//Function to create url based script.
//Parameterisation Case handling(curly brases)
//Case1: Full URL parameterisation starts
//Case2: If URL is not in correct format i.e., <scheme>://<domain>
//Case3: scheme param end
//Case4: host param start
//Case5: if url have '}?' or '}#' or '}/' , i.e., end of param before query param start
//Case6: scheme param start
static char *create_url_based_script(char *gname, char *arg_url)
{
  //gname used for future
  char cmd[MAX_DATA_LINE_LENGTH];
  //Taking static because returning the name.
  static char url_base_script[256];
  char cmd_args[MAX_DATA_LINE_LENGTH];
  char flow_file[32]; 
  char scrpt_name[MAX_DATA_LINE_LENGTH];      //script name taken from user 
  //char *ptr = NULL;
  char arg_url_orig[MAX_DATA_LINE_LENGTH];   //will be used for sending page:url info to nsu_gen_cscript, option -u
  //char pre_first_col[MAX_DATA_LINE_LENGTH + 1];   //will be used for storing pre-first-colon data
  //int count = 0;
  char *ptr1;
  ptr1 = NULL;
  char *p, *p1,*p2,*p3;
  char err_msg[1024]= "\0";

  p = NULL;
  p1 = NULL;
  p2 = NULL;
  p3 = NULL;
  
  NSDL2_HTTP(NULL, NULL, "Method called, gname = %s, arg_url = %s", gname, arg_url);
  
  // Copying Flow file name 
  strcpy(flow_file , "flow.c");
  
  if (run_mode_option & RUN_MODE_OPTION_COMPILE){
    NS_EXIT(-1, CAV_ERR_1011300, "TestRun mode option is set to compile"); 
  }
  
  if(loader_opcode == CLIENT_LOADER) {
    //ptr = getenv("NS_CONTROLLER_TEST_RUN");
    if(!g_controller_testrun[0]) {
      NS_EXIT(-1, CAV_ERR_1031058); 
    }
  }

  //Case1: Full URL parameterisation starts 
  if(*arg_url == '{')
    NS_EXIT(-1, CAV_ERR_1011300, "Parameterized URL is not supported in URL Based Script"); 

  strcpy(arg_url_orig, arg_url);

  if(strlen(arg_url_orig) > MAX_DATA_LINE_LENGTH)
    NS_EXIT(-1, CAV_ERR_1011300, "In URL Based Script, Script:Page:URL length can not be greater than 2048"); 

  //pre_first_col[0]='\0';

  //Case2
  if((ptr1 = strstr(arg_url_orig, "://")) == NULL)
    NS_EXIT(-1, CAV_ERR_1011300, "URL format is invalid in provided URL Based Script. URL format should be like protocol://domain-name/path"); 

  //Case3,Case4
  if( *(ptr1-1) == '}' || *(ptr1+3) == '{')
    NS_EXIT(-1, CAV_ERR_1011300, "Parameterized URL is not supported in URL Based Script"); 

  /* Find first occurance of any of the symbol '/' or '?' or '#' */ 
  p = ptr1 + 3;
  p1 = strchr(p, '/');
  p2 = strchr(p, '?');
  p3 = strchr(p, '#');
  if(p1 || p2 || p3)
  {
    if(p1)
    {
       p = p1;
       if(p2 && (p2 < p))
          p = p2;   
       if(p3 && (p3 < p))
          p = p3;   
    }
    else if(p2)
    {  
       p = p2;
       if(p3 && (p3 < p))
          p = p3;   
    }
    else 
      p = p3;
  }

  //Case5
  if( *(p-1) == '}')
    NS_EXIT(-1, CAV_ERR_1011300, "Parameterized URL is not supported in URL Based Script"); 

  *ptr1 = '\0';
  
  NSDL2_HTTP(NULL, NULL, "UBS | pre_first_col, [should be before ://] = %s", arg_url_orig);

  if((ptr1 = strrchr(arg_url_orig, ':')) != NULL)
  {
    //Case6: scheme parameterised
    if(*(ptr1+1) == '{')
      NS_EXIT(-1, CAV_ERR_1011300, "Parameterized URL is not supported in URL Based Script"); 

    //ignoring scheme
    *ptr1 = '\0';
    if((ptr1 = strrchr(arg_url_orig, ':')) != NULL)
    {
      //ignoring pagename, included with option -u
      *ptr1 = '\0';
      if(strchr(arg_url_orig, ':')) //validatation for extra ':'
        NS_EXIT(-1, CAV_ERR_1011300, "URL format is invalid in provided URL Based Script. URL format should be like protocol://domain-name/path"); 

      strcpy(scrpt_name, arg_url_orig);// storing data of pre colon at scrpt_name
      ptr1=strchr(arg_url,':');
      ptr1++;
      strcpy(arg_url_orig, ptr1);
      NSDL2_HTTP(NULL, NULL, "UBS | arg_url_orig = %s, arg_url =%s", arg_url_orig, arg_url);
    }
    else
    {
      strcpy(scrpt_name, arg_url_orig);// storing data of pre colon at scrpt_name
      strcpy(arg_url_orig, arg_url);
    }
  }
  else 
  {
 /* BUG 91870: Root cause: When we are not providing script names and having multiple URL base script in scenario,
                           hard coded script name "index"is getting used
                           In this case, script is getting overwrite and only last URL script is created.

               Solution: now we will use index_{number} as script name.*/

    static int count = 0;
    if (!count)
      strcpy(scrpt_name, "index");
    else
      sprintf(scrpt_name, "index_%d", count);
    count ++;
    //We have arg_url_orig as original url;
    strcpy(arg_url_orig, arg_url);
  }

  NSDL2_HTTP(NULL, NULL, "UBS | scrpt_name = %s, arg_url_orig = %s", scrpt_name, arg_url_orig);

  sprintf(url_base_script, "%s%d_%s", URL_BASED_SCRIPT_PRIFIX,
                           (!g_controller_testrun[0])?testidx:atoi(g_controller_testrun), scrpt_name);
  //First remove the previous existing url based scripts if any. This is the case of continues monitoring where
  //same TR is used to create the script
  /*bug id: 101320: new dir struc is $NS_RTA_DIR/<proj>/<sub_proj>/scripts/<script_dir> */
  sprintf(cmd_args, "-p %s/%s/%s/%s -t %d -g %s", GET_NS_RTA_DIR(), g_project_name, g_subproject_name, "scripts",
                    (!g_controller_testrun[0])?testidx:atoi(g_controller_testrun), scrpt_name);
  sprintf(cmd, "%s/bin/nsu_delete_url_based_script %s", getenv("NS_WDIR"), cmd_args);
  NSDL2_HTTP(NULL, NULL, "Going to delete url based script, cmd = %s", cmd);
  nslb_system(cmd,1,err_msg);
  
  //Create new url based script
  //sprintf(cmd_args, "-s %s/%s/%s -u \"%s\" -F %s", g_project_name, g_subproject_name, url_base_script, arg_url_orig, flow_file);
  sprintf(cmd_args, "-s %s/%s/scripts/%s -u \"%s\" -F %s -w %s/%s", g_project_name, g_subproject_name, url_base_script, arg_url_orig, flow_file,
                    GET_NS_WORKSPACE(), GET_NS_PROFILE());
  sprintf(cmd, "%s/bin/nsu_gen_cscript %s ", getenv("NS_WDIR"), cmd_args);
  
  NSDL2_HTTP(NULL, NULL, "Going to create url based script, cmd = %s", cmd);
  if (!nslb_system(cmd,1,err_msg))
    return url_base_script;
  else
    NS_EXIT(-1, CAV_ERR_1011363, url_base_script, cmd_args);

  return NULL;
}

static int create_runprof_table_entry(int *row_num) {
  NSDL2_MISC(NULL, NULL, "Method called");
  if (total_runprof_entries == max_runprof_entries) {
    MY_REALLOC_EX (runProfTable, (max_runprof_entries + DELTA_RUNPROF_ENTRIES) * sizeof(RunProfTableEntry), max_runprof_entries * sizeof(RunProfTableEntry),"runProfTable", -1);
    if (!runProfTable) {
      NSTL1_OUT(NULL, NULL, "create_runprof_table_entry(): Error allocating more memory for runprof entries\n");
      return(FAILURE);
    } else max_runprof_entries += DELTA_RUNPROF_ENTRIES;
  }
  *row_num = total_runprof_entries++;
  //runProfTable[*row_num].pacing_idx = -1;
  return (SUCCESS);
}


int find_group_idx_from_session(int session_idx)
{
  int i;
  for (i = 0; i < total_runprof_entries; i++) {
    if (runProfTable[i].sessprof_idx == session_idx) {
      NSDL4_MISC(NULL, NULL, "Session idx %d belongs to group Group idx %d", session_idx, i);
      return i;
    }
  }
  return -1;
}


// function for parsing keyword to set java object manager 

static void kw_set_java_obj_mgr_usage( char *err)
{
   NSTL1_OUT(NULL, NULL,  "Error: Invalid value of kw_set_java_obj_mgr_usage keyword: %s\n", err);
   NSTL1_OUT(NULL, NULL,  "  Usage: USE_JAVA_OBJECT_MANGER <mode> <port> <threshold> \n");
   NSTL1_OUT(NULL, NULL,  "  This keyword is to set standard kw_set_java_obj_mgr arguments \n");
   NS_EXIT(-1, "%s\nUsage: USE_JAVA_OBJECT_MANGER <mode> <port> <threshold>", err);
}

static void kw_set_java_obj_mgr(char *buf)
{
   char keyword[MAX_DATA_LINE_LENGTH];
   char mode[512] = "0";
   char port [512] = "0";
   char threshold [512] = "0";
   int num_fields = 0;
   char *ptr;
   int num; 
   int threshold_value = 0; 
   
   NSDL1_PARSING(NULL, NULL, "Method called buffer=[%s]", buf);
   num_fields = sscanf(buf,"%s %s %s %s",keyword , mode, port, threshold);
   NSDL3_PARSING(NULL,NULL,"number of fields is [%d]",num_fields);
   if (num_fields < 2 || num_fields > 4)
   {
      NSDL3_PARSING(NULL,NULL,"number of fields is [%d]",num_fields);
      kw_set_java_obj_mgr_usage("INVALID NUMBER Of FIELDS ");
   } 
 
   ptr =mode;
   NSDL3_PARSING(NULL,NULL,"mode is  [%s]",ptr);
   CLEAR_WHITE_SPACE (ptr);

  
   if (ptr == NULL)
      kw_set_java_obj_mgr_usage("mode is empty");

   if (ns_is_numeric(ptr) == 0) 
      kw_set_java_obj_mgr_usage("mode is not numeric");
   
   num = atoi(ptr);
   NSDL3_PARSING(NULL,NULL,"number is [%d]",num);
   if (num != 0 && num != 1)
      kw_set_java_obj_mgr_usage("incorrect mode");

   global_settings->use_java_obj_mgr = num ;
   NSDL3_PARSING(NULL, NULL, "Setting global_settings->use_java_obj_mgr = %d", global_settings->use_java_obj_mgr);

  if (port[0] !='\0')
  {
     ptr = port ;
     NSDL3_PARSING(NULL,NULL,"port is [%s]",ptr);
     CLEAR_WHITE_SPACE (ptr);

     if (ptr == NULL)
        kw_set_java_obj_mgr_usage("port is empty ");
   
     if (ns_is_numeric(ptr) == 0)
        kw_set_java_obj_mgr_usage("port is not numeric");

     num = atoi(ptr);
    
     global_settings->java_object_mgr_port = num ;
     NSDL3_PARSING(NULL, NULL, "Setting global_settings->java_object_mgr_port = %hd", global_settings->java_object_mgr_port);
  } 

  if (threshold[0] != '\0')
  {
    ptr = threshold;
    NSDL3_PARSING(NULL,NULL,"threshold is [%s]",ptr);
    CLEAR_WHITE_SPACE (ptr);

    if (ptr == NULL)
      kw_set_java_obj_mgr_usage("threshold is empty ");

    if (ns_is_numeric(ptr) == 0)
      kw_set_java_obj_mgr_usage("threshold is not numeric");

    threshold_value = atoi(ptr);

    global_settings->java_object_mgr_threshold = threshold_value ;
    NSDL3_PARSING(NULL, NULL, "Setting global_settings->java_object_mgr_threshold = %d", global_settings->java_object_mgr_threshold);
  }
}
  
static void usage_enable_tmpfs_in_dbu(char *err_msg, int exit_flag)
{
  NSTL1_OUT(NULL, NULL,  "\n%s\n", err_msg);
  NSTL1_OUT(NULL, NULL,  "Keyword Usage:\n");
  NSTL1_OUT(NULL, NULL,  "ENABLE_TMPFS_IN_DBU <Enable/Disable for NSDBU> <Enable/Disable for NDDBU> <Tmp File path (optional)>\n\n");
  NSTL1_OUT(NULL, NULL,  "Example: ENABLE_TMPFS_IN_DBU 1 0 /home/cavisson/work/mytmp\n"
                  "NSDBU tmp files will be stored in /mnt/tmp and NDDBU tmp files in /home/cavisson/work/mytmp\n"
                  "In case if tmpfs is disabled and tmp file path is not provided, then TRxxx/.tmp dir will be used\n" 
                  "It is highly recommended to use 'ENABLE_TMPFS_IN_DBU 1 1', because using tmpfs causes less load on system.\n"
                  "Before using this keyword, tmpfs must be mounted.\n"
                  "To check if tmpfs is mounted, use 'df -h | grep -w tmpfs | grep -w /mnt/tmp\n"
                  "If not mounted, then run following commands using root --\n"
                  "mkdir /mnt/tmp; chmod 777 /mnt/tmp; mount -t tmpfs -o size=10G tmpfs /mnt/tmp\n"
                  "10 GB size is recommended for machines having RAM >= 96 GB\n\n");
  if(exit_flag)
    NS_EXIT(-1, "%s\nUsage: ENABLE_TMPFS_IN_DBU <Enable/Disable for NSDBU> <Enable/Disable for NDDBU> <Tmp File path (optional)>", err_msg);
}

static void check_path_exist(char *path)
{
  struct stat s;
  char err_msg[1024] = {0};
  if(stat(path, &s) )
  {
    sprintf(err_msg, "file path '%s' does not exist.", path);
    usage_enable_tmpfs_in_dbu(err_msg, 1 /*exit flag*/);
  }
  if(!(s.st_mode & S_IFDIR))
  {
    sprintf(err_msg, "The path '%s' provided, is not a directory.\n", path);
    usage_enable_tmpfs_in_dbu(err_msg, 1 /*exit flag*/);
  }
}

static void check_tmpfs_mounted()
{
  char buf[1024] = {0};
  char cmd_out[1024] = {0};
  int ret;

  strcpy(buf, "df -h | grep -w tmpfs 2>/dev/null | grep -w /mnt/tmp 1>/dev/null 2>/dev/null; echo $?");
  ret = nslb_run_cmd_and_get_last_line (buf, 1024, cmd_out);

  if(ret < 0)
  {
    NS_EXIT(-1, "Not able to run command '%s' in function kw_enable_tmpfs_in_dbu", buf);
  }

  if(cmd_out[0] != '0')
  {
    usage_enable_tmpfs_in_dbu("ERROR: Test Run stopped as tmpfs is not mounted on /mnt/tmp.\nPlease either disable the keyword ENABLE_TMPFS_IN_DBU or manually mount tmpfs on /mnt/tmp.\ntmpfs will be mounted by default, if RAM size is >= 96GB.\n", 1 /*exit flag*/);
  }
}

static void validate_dbu_tmp_file_path()
{
  char tmpfs_validated = 0, path_validated = 0;

  //Return in case of generator for netCloud
  if(loader_opcode == CLIENT_LOADER)
    return;

  //Tmp file path check for NSDBU
  if(global_settings->NSDBTmpFilePath[0] != '\0' && 
            get_max_report_level_from_non_shr_mem() == 2) 
  {

    if(global_settings->ns_tmpfs_flag > 0)
    {
      check_path_exist(global_settings->NSDBTmpFilePath);
      check_tmpfs_mounted();
      tmpfs_validated = 1;
    }
    else if(global_settings->ns_tmpfs_flag == 0)
    {
      check_path_exist(global_settings->NSDBTmpFilePath);
      path_validated = 1;
    }
  }

  //TMP file path check for NDDBU
  if(global_settings->NDDBTmpFilePath[0] != '\0' && global_settings->net_diagnostics_mode > 0)
  {
    if(global_settings->nd_tmpfs_flag > 0 && tmpfs_validated == 0)
    {
      check_path_exist(global_settings->NDDBTmpFilePath);
      check_tmpfs_mounted();
    }
    else if(global_settings->nd_tmpfs_flag == 0 && path_validated == 0)
    {
      check_path_exist(global_settings->NDDBTmpFilePath);
    }
  }

  NSDL1_PARSING(NULL, NULL, "ns_tmpfs_flag = %d, nd_tmpfs_flag = %d, NSDBTmpFilePath = [%s], NDDBTmpFilePath = [%s]", 
                                   global_settings->ns_tmpfs_flag, global_settings->nd_tmpfs_flag,
                                   global_settings->NSDBTmpFilePath, global_settings->NDDBTmpFilePath);
}

static void kw_enable_tmpfs_in_dbu(char *buf, int old_keyword)
{
  //if machine type is fedora, then no use of this keyword.
  #if (Fedora)
    return;
  #endif

  char keyword[MAX_DATA_LINE_LENGTH];
  char dbTmpFilePath[1024] = {0};
  char ns_tmpfs_flag, nd_tmpfs_flag, ret;
  char ns_tmpfs_value[8] = "";  // Intialising with NULL as it may store junk values and we are doing atoi()  - Refer ENABLE_TMPFS_IN_DBU Bug
  char nd_tmpfs_value[8] = "";
  int ram_size;
  struct sysinfo info;

  NSDL1_PARSING(NULL, NULL, "Method called");

  if(old_keyword)
  {
    sscanf(buf, "%s %s", keyword, dbTmpFilePath);
    //'NA' is passed from KeywordDefinition.dat. In this case no need to print warning.
    if(strcmp(dbTmpFilePath, "NA") != 0)
    { 
      NSDL1_PARSING(NULL, NULL, "Buf = %s, This keyword is not supported now. Use ENABLE_TMPFS_IN_DBU keyword in scenario", buf);
      usage_enable_tmpfs_in_dbu("Keyword DB_TMP_FILE_PATH is not supported now. Use ENABLE_TMPFS_IN_DBU keyword.", 0/*exit flag*/);
    }
    return;
  }

  //Fixed Bug 17332(merged from 4.1.5 for 17209) 
  if(sysinfo(&info) != 0)
  {
    NSTL1_OUT(NULL, NULL,  "Error in sysinfo function. Error:%s so setting ram_size variable to 96 GB", nslb_strerror(errno));
    ram_size = 96;
  }
  else
    ram_size = info.totalram *(unsigned long long)info.mem_unit / (1024 * 1024 * 1024);

  /* Tanmay - We are using %c to store tmpfs_value and using atoi, chances for junk values so intialised the character array with NULL */
  ret = sscanf(buf, "%s %c %c %s", keyword, ns_tmpfs_value, nd_tmpfs_value, dbTmpFilePath);
  
  if(ns_tmpfs_value[0] == '-')
  {
    if(ram_size >= 96)
      ns_tmpfs_flag = 1;
    else
      ns_tmpfs_flag = 0;
  }
  else
    ns_tmpfs_flag = atoi(ns_tmpfs_value);
   

  if(nd_tmpfs_value[0] == '-')
  {
    if(ram_size >= 96)
      nd_tmpfs_flag = 1;
    else
      nd_tmpfs_flag = 0;
  }
  else
    nd_tmpfs_flag = atoi(nd_tmpfs_value);

  if(ret < 3)
    usage_enable_tmpfs_in_dbu("Invalid arguments", 1 /*exit flag*/);

  if(ns_tmpfs_flag != 0 && ns_tmpfs_flag != 1)
    usage_enable_tmpfs_in_dbu("Passed Value should be 0 or 1", 1 /*exit flag*/);
  if(nd_tmpfs_flag != 0 && nd_tmpfs_flag != 1)
    usage_enable_tmpfs_in_dbu("Passed Value should be 0 or 1", 1 /*exit flag*/);

  if(ns_tmpfs_flag > 0)
    strcpy(global_settings->NSDBTmpFilePath, "/mnt/tmp");
  else
    strcpy(global_settings->NSDBTmpFilePath, dbTmpFilePath);

  if(nd_tmpfs_flag > 0)
    strcpy(global_settings->NDDBTmpFilePath, "/mnt/tmp");
  else
    strcpy(global_settings->NDDBTmpFilePath, dbTmpFilePath);

  global_settings->ns_tmpfs_flag = ns_tmpfs_flag;
  global_settings->nd_tmpfs_flag = nd_tmpfs_flag;

  NSDL1_PARSING(NULL, NULL, "Keyword = [%s], ns_tmpfs_flag = %d, nd_tmpfs_flag = %d, " 
                            "NSDBTmpFilePath = [%s], NDDBTmpFilePath = [%s]", keyword, ns_tmpfs_flag, nd_tmpfs_flag,
                                   global_settings->NSDBTmpFilePath, global_settings->NDDBTmpFilePath);
}


/**
 * Note: The function can not be used in post parsing phase
 * Return: -1 on not found
 */
int find_group_idx(char *grp_name)
{
  int i;
  for (i = 0; i < total_runprof_entries; i++) {
    if (strcmp(grp_name, RETRIEVE_BUFFER_DATA(runProfTable[i].scen_group_name)) == 0) {
      NSDL4_MISC(NULL, NULL, "Group %s found at index %d", grp_name, i);
      return i;
    }
  } 
  return -1;
}

/**
 * The keywords are now parased in two passes. The first pass
 * will parse all group related global keywords. In the second
 * Parse, we will parse all group specific keywords.
 *
 * Note: We should have already parsed SCENARIO_GROUP keyword
 *       before we reach here. That is, we have knowledge of all
 *       the groups that are going to be used.
 */
//BHAV 
int    /* Returns group number on success -1 on not found */
check_keyword_with_group(char *keyword, char *line) {
  char loc_line[MAX_LINE_LENGTH + 1];
  char *tok;

  strncpy(loc_line, line, strlen(line) > MAX_LINE_LENGTH ?  MAX_LINE_LENGTH : strlen(line));
  
  tok = strtok(loc_line, " ");
  
  if (strcasecmp(tok, keyword) == 0) {
    tok = strtok(NULL, " ");
    if (strcasecmp("ALL", tok) == 0) {
      NSDL4_MISC(NULL, NULL, "Global group ALL for keyword %s", keyword);
      return 0;
    } else {
      return find_group_idx(tok); /* will return -1 if group is not found */
    }
  } else  {                      /* keyword not found */
    return -1;
  }
}

#if 0
static void kw_set_alert_profile(char *alert_profile_name, int num) {
  char file_name[2048];

  if (num != 2)
  {
    NSTL1_OUT(NULL, NULL,  "read_keywords(): Need ONE fields after key ALERT_PROFILE\n");
    exit(-1);
  }
  
  sprintf(file_name, "%s/adf/%s.adf", g_ns_wdir, alert_profile_name);

  if(access(file_name, R_OK)) {
    NSTL1_OUT(NULL, NULL,  "Error: Unable to access ALERT_PROFILE %s.\n", alert_profile_name);
    exit (-1);
  }
}
#endif

/*This function is to set the default value of global_settings variables.
 * If cant give default value in Keyword definition file, 
 * then set default values in this function*/
void set_default_value_for_global_keywords ()
{
  NSDL2_SCHEDULE(NULL, NULL, "Method called");
   /*Set NS Admin IP as default Controller IP*/
   strcpy(global_settings->ctrl_server_ip, g_cavinfo.NSAdminIP);
   get_java_home_path();
}

/*******************************************************************************************
 * Description        : kw_set_machine_name() method used to parse MACHINE_NAME keyword,
 * Format             : MACHINE_NAME <machine-name>
 * Input Parameters
 *           buf      : Providing entire buffer(including keyword).
 * Output Parameters  : Set event_generating_host in struct GlobalData
 * Return             : Retuns 0 for success and exit if fails.
 **************************************************************************************************************/

static int kw_set_machine_name(char *buf)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH]; //This used to check if some extra field is given
  int num;
  char machine_name[256];
  char machine_ip[256];
  char gen_id[256];
  char loc_id[256];

  NSDL1_PARSING(NULL, NULL, "Method called. buf = %s", buf);

  num = sscanf(buf, "%s %s %s %s %s %s", keyword, machine_name, machine_ip, gen_id, loc_id, tmp); // This is used to check number of arguments
  if (num != 5)
  {
    NSTL1_OUT(NULL, NULL,  "Error: Invalid value of MACHINE_NAME keyword: Invalid number of arguments\n");
    NSTL1_OUT(NULL, NULL,  "  Usage: MACHINE_NAME <machine name> <machine ip> <generator id> <location id>\n");
    NSTL1_OUT(NULL, NULL,  "  Where machine name:\n");
    NSTL1_OUT(NULL, NULL,  "        Is used to specify machine name.\n");
    NSTL1_OUT(NULL, NULL,  "  Where machine ip:\n");
    NSTL1_OUT(NULL, NULL,  "        Is used to specify machine ip.\n");
    NSTL1_OUT(NULL, NULL,  "  Where gen id:\n");
    NSTL1_OUT(NULL, NULL,  "        Is used to specify generator id.\n");
    NSTL1_OUT(NULL, NULL,  "  Where loc id:\n");
    NSTL1_OUT(NULL, NULL,  "        Is used to specify location id.\n");

    NS_EXIT(-1, "Invalid number of arguments\nUsage: MACHINE_NAME <machine name> <machine ip> <generator id> <location id>");
  }
  
  strcpy((char *)global_settings->event_generating_host, machine_name);
  strcpy(global_settings->event_generating_ip, machine_ip);
  global_settings->gen_id = atoi(gen_id);
  global_settings->loc_id = atoi(loc_id);

  NSDL1_PARSING(NULL, NULL, "global_settings->event_generating_host = [%s], global_settings->event_generating_ip = [%s],"
                            " global_settings->gen_id = %d, global_settings->loc_id = %d", 
                            global_settings->event_generating_host, global_settings->event_generating_ip, 
                            global_settings->gen_id, global_settings->loc_id);

  return 0;
}

static int kw_set_gperf_cmd_options(char *buf)
{
  char keyword[512 + 1];
  char tmp[128 + 1]; //This used to check if some extra field is given
  char gperf_opt[MAX_DATA_LINE_LENGTH] = ""; // store -s option of gperf by default it will be 1  
  char usages[1024 + 1];
  char value[MAX_DATA_LINE_LENGTH] = "";
  int num = 0;

  NSDL2_PARSING(NULL, NULL, "Method called, buf = %s", buf);

  sprintf(usages, "Usages:\n"
                  "Syntax - GPERF_CMD_OPTIONS <option>\n"
                  "Where:\n"
                  "option - Affects the size of the generated hash table. The numeric argument N indicates" 
                            "how many times larger or smaller the associated value range should be, "
                            "in relationship to the number of keys, e.g. a value of 3 means "
                            "allow the  maximum  associated  value  to  be about  3  times  larger  than the number of input keys." 
                            "Conversely, a value of 1/3(i.e. 0.33) means make the maximum associated value about 3"
                            "times smaller than the number of input keys. A larger table should decrease the time "
                            "required for an unsuccessful search, at the expense of extra table space. (Default value is 1)\n"
                  "For eg: GPERF_CMD_OPTIONS -s 1/3 ");

  num = sscanf(buf, "%s %s %s %s", keyword, gperf_opt, value, tmp);
  if(num != 3)
  {
    NS_EXIT(-1, "Invalid number of arguments provided\n%s", usages);
  }
 
  sprintf(global_settings->gperf_cmd_options, "%s %s", gperf_opt, value); 

  NSDL2_PARSING(NULL, NULL, "Method ends, gperf_size_multiple = %s", global_settings->gperf_cmd_options);  
  return 0;
}

static void enable_log_inline_block_time_usage(char *err_msg, char *buf)
{
  NSTL1_OUT(NULL, NULL,  "Error: Invalid value of LOG_INLINE_BLOCK_TIME keyword: %s\n", err_msg);
  NSTL1_OUT(NULL, NULL,  "       Line %s\n", buf);
  NSTL1_OUT(NULL, NULL,  "  Usage: LOG_INLINE_BLOCK_TIME <reporting mode>\n");
  NSTL1_OUT(NULL, NULL,  "  Where:\n");
  NSTL1_OUT(NULL, NULL,  "        reporting mode: To configure reporting details in log_inline_block_time report .\n");
  NSTL1_OUT(NULL, NULL,  "                    0 : Disable log_inline_block_time report.\n");
  NSTL1_OUT(NULL, NULL,  "                    1 : Enable log_inline_block_time report.\n");
  NS_EXIT(-1, "%s\nUsage: LOG_INLINE_BLOCK_TIME <reporting mode>", err_msg);
}

static void enable_progress_report_usage(char *err_msg, char *buf)
{
  NSTL1_OUT(NULL, NULL,  "Error: Invalid value of ENABLE_PROGRESS_REPORT keyword: %s\n", err_msg);
  NSTL1_OUT(NULL, NULL,  "       Line %s\n", buf);
  NSTL1_OUT(NULL, NULL,  "  Usage: ENABLE_PROGRESS_REPORT <reporting mode>\n");
  NSTL1_OUT(NULL, NULL,  "  Where:\n");
  NSTL1_OUT(NULL, NULL,  "        reporting mode: To configure reporting details in progress report.\n");
  NSTL1_OUT(NULL, NULL,  "                    0 : Disable progress report.\n");
  NSTL1_OUT(NULL, NULL,  "                    1 : Disable monitor details from progress report.\n");
  NSTL1_OUT(NULL, NULL,  "                    2 : Print complete progress report(including monitor details).\n");
  NS_EXIT(-1, "%s\nUsage: ENABLE_PROGRESS_REPORT <reporting mode>", err_msg);
}

/*************************************************************************************************************
 * Description        : kw_enable_progress_report() method used to parse ENABLE_PROGRESS_REPORT keyword,
 * Format             : ENABLE_PROGRESS_REPORT <enable/disable>
 * Input Parameters
 *           buf      : Providing entire buffer(including keyword).
 * Output Parameters  : Set progress_report_mode in struct GlobalData
 * Return             : Retuns 0 for success and exit if fails.
 **************************************************************************************************************/

static void kw_enable_progress_report(char *buf)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH]; //This used to check if some extra field is given
  int num, mode;
  char progress_report[MAX_DATA_LINE_LENGTH];
  strcpy(progress_report, "2");
  mode = 2;

  NSDL1_PARSING(NULL, NULL, "Method called. buf = %s", buf);

/***********************************************************************************
  [BugId:94911] Getting -ve Values for Vusers in Generator's progress report.

  => In case of data compression, we are filling compressed data into same AVGTIME memory
  => If message compression feature is enabled, only disable Progress Report on Gen.
     Set ENABLE_PROGRESS_REPORT 0 for generators
  => Now Progress Report will be print on controller only
************************************************************************************/
  if (loader_opcode == CLIENT_LOADER)
  {
    NSTL1(NULL, NULL, "From release 4.4.0 Progress Report on generator has been disabled because we are send compressed progress report generator to controller. Using compressed report so progress report can not be printed.");

    global_settings->progress_report_mode = 0;
    return;
  }

  num = sscanf(buf, "%s %s %s", keyword, progress_report, tmp); // This is used to check number of arguments

  if (num != 2) {
    enable_progress_report_usage("Invaid number of arguments", buf);
  } 

  if (ns_is_numeric(progress_report) == 0) {
    enable_progress_report_usage("Reporting mode can have only integer value", buf);
  }

  mode = atoi(progress_report);

  if ((mode < 0) || (mode > 2)) {
    enable_progress_report_usage("Invalid value for reporting mode", buf);
  }

  global_settings->progress_report_mode = mode;
  if (global_settings->progress_report_mode == 2)
    NS_DUMP_WARNING("Keyword 'ENABLE_PROGRESS_REPORT' is enabled with mode 2. In this case complete progress report(including monitor details) will be printed which will cause delay in getting data in dashboard and wrong elapsed time.");

  NSDL2_PARSING(NULL, NULL, "Monitor report option is = %d", global_settings->progress_report_mode);
}

static void kw_log_inline_block_time(char *buf)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH]; //This used to check if some extra field is given
  int num, mode;
  char log_inline[MAX_DATA_LINE_LENGTH];

  NSDL1_PARSING(NULL, NULL, "Method called. buf = %s", buf);

  num = sscanf(buf, "%s %s %s", keyword, log_inline, tmp); // This is used to check number of arguments

  if (num != 2) {
    enable_log_inline_block_time_usage("Invalid number of arguments", buf);
  }

  if (ns_is_numeric(log_inline) == 0) {
    enable_log_inline_block_time_usage("Reporting mode can have only integer value", buf);
  }

  mode = atoi(log_inline);

  if ((mode < 0) || (mode > 1)) {
    enable_log_inline_block_time_usage("Invalid value for reporting mode", buf);
  }

  global_settings->log_inline_block_time = mode;

  NSDL2_PARSING(NULL, NULL, "Log Inline Block Time is = %d", global_settings->log_inline_block_time);
}

static void server_select_mode_usage(char *err_msg, char *buf)
{
  NSTL1_OUT(NULL, NULL,  "Error: Invalid value of SERVER_SELECT_MODE keyword: %s\n", err_msg);
  NSTL1_OUT(NULL, NULL,  "       Line %s\n", buf);
  NSTL1_OUT(NULL, NULL,  "  Usage: SERVER_SELECT_MODE <mode> \n");
  NSTL1_OUT(NULL, NULL,  "  Where:\n");
  NSTL1_OUT(NULL, NULL,  "         0 : Disable server selecting feature, default value is 0\n");
  NSTL1_OUT(NULL, NULL,  "         1 : Enable  server selecting, here actual host can serve both HTTP and HTTPS request\n");
  NS_EXIT(-1, "%s\nUsage: SERVER_SELECT_MODE <mode>", err_msg);
}

/*******************************************************************************************
 * Description        : kw_set_ns_mode() method used to parse SERVER_SELECT_MODE keyword,
 * Format             : SERVER_SELECT_MODE <mode>
 * Input Parameters
 *           buf      : Providing entire buffer(including keyword).
 * Output Parameters  : Set server_select_mode in struct GlobalData
 * Return             : Retuns 0 for success and exit if fails.
 *******************************************************************************************/

static int kw_set_server_select_mode(char *buf)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH]; //This used to check if some extra field is given
  int num, mode = 0;
  char server_sel_mode[MAX_DATA_LINE_LENGTH];
  //Fill default value
  strcpy(server_sel_mode, "0");

  NSDL1_PARSING(NULL, NULL, "Method called. buf = %s", buf);

  num = sscanf(buf, "%s %s %s", keyword, server_sel_mode, tmp); // This is used to check number of arguments
  //Validate number of arguents
  if ((num != 2))
  {
    server_select_mode_usage("Invalid number of arguments", buf);
  }
  //Validate monitoring mode
  if(ns_is_numeric(server_sel_mode) == 0) {
    server_select_mode_usage("Server select mode can have only integer value", buf);
  }

  mode = atoi(server_sel_mode);

  if((mode < 0) || (mode > 1))
  {
    server_select_mode_usage("Server select mode can be either 0 or 1", buf);
  }
  global_settings->server_select_mode = mode;

  NSDL2_PARSING(NULL, NULL, "Mode = %c", global_settings->server_select_mode);

  return 0;
}

static void kw_set_reset_test_start_time_stamp_usage(char *err_msg, char *buf)
{
  NSTL1_OUT(NULL, NULL,  "Error: Invalid value of RESET_TEST_START_TIME_STAMP keyword: %s\n", err_msg);
  NSTL1_OUT(NULL, NULL,  "       Line %s\n", buf);
  NSTL1_OUT(NULL, NULL,  "  Usage: RESET_TEST_START_TIME_STAMP <mode>\n");
  NSTL1_OUT(NULL, NULL,  "  Where:\n");
  NSTL1_OUT(NULL, NULL,  "         mode : Is used to specify if reset Test Start Time stamp in summary.top after setting up monitors\n");
  NS_EXIT(-1, "%s\nUsage: RESET_TEST_START_TIME_STAMP <mode>", err_msg);
}

int kw_set_reset_test_start_time_stamp(char *buf)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char tmp_data[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH];
  int num;
  char tmp_val;
  
  NSDL2_PARSING(NULL, NULL, "Method called, buf =%s", buf);

  num = sscanf(buf, "%s %s %s", keyword, tmp_data, tmp);
 
  if (num != 2){
    kw_set_reset_test_start_time_stamp_usage("Invaid number of arguments", buf);
  }
  
  if(ns_is_numeric(tmp_data) == 0) {
    kw_set_reset_test_start_time_stamp_usage("Mode can have only integer value", buf);
  }

  tmp_val = (char)atoi(tmp_data);

  if(tmp_val < 0 || tmp_val > 1)
    kw_set_reset_test_start_time_stamp_usage("Mode can have value 0 or 1", buf);
  
  global_settings->reset_test_start_time_stamp = tmp_val;
  NSDL2_PARSING(NULL, NULL, "global_settings->reset_test_start_time_stamp = %d", global_settings->reset_test_start_time_stamp);
  return 0;
}

//ENABLE_DB_AGGREGATOR <0/1> <conf file name>
void kw_set_enable_db_aggregator(char *keyword, char *buf)
{
  int value, num;
  char conf_file_name[1024];

  NSDL2_PARSING(NULL, NULL, "Method called, keyword = %s, buf = %s, global_settings->net_diagnostics_mode=%d", keyword, buf,
                                                                                     global_settings->net_diagnostics_mode);

  num = sscanf(buf, "%s %d %s", keyword, &value, conf_file_name);

  if(num < 2 || num > 3)
  {
    NSDL2_PARSING(NULL, NULL, "Error: Invalid No. of arguments '%d' in ENABLE_DB_AGGREGATOR keyword", num);
    return;
  }

  if(value <= 0 || value > 1)
  {
    NSDL2_PARSING(NULL, NULL, "Error: mode is '%d' in ENABLE_DB_AGGREGATOR keyword", value);
    return;
  }
  else if(value == 1)
  {
    if(conf_file_name[0] == '\0')
    {
      NSDL2_PARSING(NULL, NULL, "Error: Conf file is not given in ENABLE_DB_AGGREGATOR keyword");
      return;
    }
    else
    {
      global_settings->db_aggregator_mode = value;
      snprintf(global_settings->db_aggregator_conf_file, 1024, "%s", conf_file_name);
    }
  }

  NSDL2_PARSING(NULL, NULL, "mode=%d, conf file=%s", global_settings->db_aggregator_mode,
                                               global_settings->db_aggregator_conf_file);
}

static void kw_set_ns_nvm_fail_usage(char *err_msg, char *buf)
{
  NSTL1_OUT(NULL, NULL,  "Error: Invalid value of CONTINUE_TEST_ON_NVM_FAILURE keyword: %s\n", err_msg);
  NSTL1_OUT(NULL, NULL,  "       Line %s\n", buf);
  NSTL1_OUT(NULL, NULL,  "  Usage: CONTINUE_TEST_ON_NVM_FAILURE <mode>\n");
  NSTL1_OUT(NULL, NULL,  "  Where:\n");
  NSTL1_OUT(NULL, NULL,  "         mode : Is used to specify whether user want to continue test if any NVM fails. Default value is 1\n");
  NS_EXIT(-1, "%s\nUsage: CONTINUE_TEST_ON_NVM_FAILURE <mode>", err_msg);
}

static int kw_set_ns_nvm_fail(char *buf, char *global_set)
{       
  char keyword[MAX_DATA_LINE_LENGTH];
  char tmp_data[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH];
  int num;
  char tmp_val; 
      
  NSDL2_PARSING(NULL, NULL, "Method called, buf =%s", buf);
      
  num = sscanf(buf, "%s %s %s", keyword, tmp_data, tmp);

  NSDL2_PARSING(NULL, NULL, "Method called, num_progress_fail_gen =%s", tmp_data);

  if (num != 2){
    kw_set_ns_nvm_fail_usage("Invaid number of arguments", buf);
  }     
      
  if(ns_is_numeric(tmp_data) == 0) {
    kw_set_ns_nvm_fail_usage("Mode can have only integer value", buf);
  }     

  tmp_val = (char)atoi(tmp_data);

  if(tmp_val < 0 || tmp_val > 1)
    kw_set_ns_nvm_fail_usage("Mode can have value 0 or 1", buf);

  *global_set = tmp_val;

  return 0;
}

void kw_set_rtg_size(char *buf)
{ 
  int num_args = 0;
  double value;
  char keyword[MAX_DATA_LINE_LENGTH];
  NSDL2_PARSING(NULL, NULL, "Method called, buf =%s", buf);
  num_args = sscanf(buf, "%s %lf", keyword, &value);
  if(num_args == 2)
  { 
    if((value <= 2048) && (value >= 0.005))
    {  
       NSTL1(0,NULL,"Desired input is given");
       global_settings->max_rtg_size = value * 1024 *1024;
    }  
    else if(value < 0.005)
    { 
      NSTL1(0,NULL,"Given rtg size is less than minimum rtg size (0.005) Mb. Hence setting it to default minimumum size");
      global_settings->max_rtg_size = 0.005 *(1024 * 1024);
    }
    else
    {
      NSTL1(0,NULL,"Given RTG size is greater than 2Gb,Hence setting it to default 2Gb");
      global_settings->max_rtg_size = (long long)(2048) * (1024 * 1024);
    }
  }
  else
  {
    NSTL1(0,NULL,"No size is given as setting rtg size to max size");
    global_settings->max_rtg_size = (long long)(2048) * (1024 * 1024);
  }
  NSTL1(0, NULL, "Num of keywords is less than rtg buffer is %d", num_args);
}

void kw_set_ns_alloc_extra_bytes(char *buffer)
{
  int size = 0, num;
  char keyword[MAX_DATA_LINE_LENGTH];
  num = sscanf(buffer, "%s %d", keyword, &size);
  if((num < 2) || (size < 0))
    g_alloc_extra_size = 1;
  else
    g_alloc_extra_size = size;
 
  NSTL1(0, NULL, "g_alloc_extra_size = %d", g_alloc_extra_size);
 
}

void whitelist_header_usages(char *err)
{
  NSTL1(NULL, NULL, "Error: invalid format of keyword WHITELIST_HEADER");
  NS_EXIT(-1, "%s\nUsage: WHITELIST_HEADER <Mode(0/1)> <Header_Name> <Header_Value>", err);
}

void kw_set_whitelist_header(char *buf, int flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char header_name[MAX_DATA_LINE_LENGTH];
  char header_value[MAX_DATA_LINE_LENGTH];
  char k_mode[8];
  int num = 0;
  int mode = 0;

  NSDL2_PARSING(NULL, NULL, "Method called, buf = %s", buf); 

  if ((num = sscanf(buf, "%s %s %s %s", keyword, k_mode, header_name, header_value)) < 2) 
  {
    whitelist_header_usages("Invalid number of arguments");
  }

  if(ns_is_numeric(k_mode) == 0)
  {
    whitelist_header_usages("Mode is not numeric");
  }
  mode = atoi(k_mode);

  if(mode != 0 && mode != 1)
  {
    whitelist_header_usages("Invalid Mode.");
  }

  if((mode != 0) && (num != 4))
  {
    whitelist_header_usages("Invalid number of arguments");
  }

  MY_MALLOC(global_settings->whitelist_hdr, sizeof(WhitelistHeader), "whitelist_hdr", -1);  
  global_settings->whitelist_hdr->mode = mode;

  if(mode)
  {
    MY_MALLOC(global_settings->whitelist_hdr->name, strlen(header_name) + 1, "whitelist_header_name", -1);  
    strcpy(global_settings->whitelist_hdr->name, header_name);

    char *ptr = NULL;
    if((ptr = strstr(header_value, "{ipaddr}")) != NULL)
    {
      int hdr_len = (ptr - header_value) + strlen(g_cavinfo.NSAdminIP) + 1;
      *ptr = '\0';
      MY_MALLOC(global_settings->whitelist_hdr->value, hdr_len + 1, "whitelist_header_value", -1);

      snprintf(global_settings->whitelist_hdr->value, hdr_len, "%s%s", header_value, g_cavinfo.NSAdminIP);
    }
    else
    {
      MY_MALLOC(global_settings->whitelist_hdr->value, strlen(header_value) + 1, "whitelist_header_value", -1);  
      strcpy(global_settings->whitelist_hdr->value, header_value);
    }
    
    NSDL2_PARSING(NULL, NULL, "Header Name = %s, Header Value = %s",
                               global_settings->whitelist_hdr->name, global_settings->whitelist_hdr->value); 
  }
  NSDL2_PARSING(NULL, NULL, "WHITELIST_HEADER, mode = %d, Header Name = %s", mode, header_name); 
}

void read_keywords(FILE* fp, int all_keywords)
{
  int num;
  char *buf;
  char buff[MAX_MONITOR_BUFFER_SIZE+1];
  char text[MAX_DATA_LINE_LENGTH];
  char keyword[MAX_DATA_LINE_LENGTH];
  char err_msg[MAX_DATA_LINE_LENGTH];
  int  line_num = 0;
  
  NSDL2_SCHEDULE(NULL, NULL, "Method called");
  set_default_value_for_global_keywords ();
  while (fgets(buff, MAX_MONITOR_BUFFER_SIZE, fp) != NULL) 
  {
    line_num++;
    NSDL2_SCHEDULE(NULL, NULL, "buff = [%s], line_no = %d, len = %d", buff, line_num, strlen(buff));
    buff[strlen(buff)-1] = '\0';  //Removing new line
    
    buf = buff;

    CLEAR_WHITE_SPACE(buf);
    CLEAR_WHITE_SPACE_FROM_END(buf);
    
    NSDL2_SCHEDULE(NULL, NULL, "buf = [%s]", buf);
    //NSTL1_OUT(NULL, NULL,  "Buffer1 = %s \n", buf);
    if((buf[0] == '#') || (buf[0] == '\0'))
      continue;

    if ((num = sscanf(buf, "%s %s", keyword, text)) != 2) 
    {
    	printf("read_keywords(): At least two fields required  <%s>\n", buf);
	    continue;
    } 
    else 
    {
      NSDL3_SCHEDULE(NULL, NULL, "keyword = %s, text = %s", keyword, text);  
      if (all_keywords) 
      {
/*      kw_set_avg_ssl_reuse(keyword, text);*/ 
/*        kw_set_ssl_cert_file(keyword, text); */
/*        kw_set_ssl_key_file(keyword, text); */
/*         kw_set_ssl_clean_close_only(keyword, text); */

        if (strcasecmp(keyword, "SCHEDULE") == 0) {
          kw_set_schedule(buf, err_msg, 0);
        }
        else if (strcasecmp(keyword, "NH_SCENARIO") == 0)
        {
          kw_set_nh_scenario(buf, err_msg);
        } 
        else if (strcasecmp(keyword, "SERVER_STATS") == 0) {
	  //printf("Calling function 1 get_server_perf_stats\n");
	  get_server_perf_stats(buf); // Achint 03/01/2007 - Add this function to get all server IP Addresses
	} else if (strcasecmp(keyword, "SERVER_PERF_STATS") == 0) {
	  //printf("Calling function 2 get_server_perf_stats\n");
	  get_server_perf_stats(buf); // Achint 03/01/2007 - Add this function to get all server IP Addresses
        } else if (strcasecmp(keyword, "CUSTOM_MONITOR") == 0 || 
		   strcasecmp(keyword, "SPECIAL_MONITOR") == 0 || 
		   strcasecmp(keyword, "LOG_MONITOR") == 0) {

          //setting unique monitorid for custom monitors added. For DVM we are sperrately incrementing in convert_dvm_to_cm
          //if(custom_config(keyword, buf, NULL, 0, NORMAL_CM_TABLE, err_msg, NULL, 0, -1, NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0) >= 0)
          if(custom_monitor_config(keyword, buf, NULL, 0, g_runtime_flag, err_msg, NULL, NULL, 0) >= 0)
          {
            if(strcasecmp(keyword, "LOG_MONITOR") != 0)
              g_mon_id = get_next_mon_id();
            monitor_added_on_runtime = 1;
          }

        } else if (strcasecmp(keyword, "DYNAMIC_VECTOR_MONITOR") == 0) {
          kw_set_dynamic_vector_monitor(keyword, buf, NULL, 0, 0, NULL, err_msg, NULL, NULL, 0);
        } else if (strcasecmp(keyword, "COH_CLUSTER_AND_CACHE_VECTORS") == 0) {
          kw_set_coh_cluster_cache_vectors(keyword, buf);
        } else if (strcasecmp(keyword, "ND_ENABLE_HOT_SPOT_MONITOR") == 0) {
          kw_set_nd_enable_hot_spot_monitor(keyword, buf, err_msg);
	} else if (strcasecmp(keyword, "ND_ENABLE_NODEJS_SERVER_MONITOR") == 0) {
          kw_set_nd_enable_nodejs_server_monitor(keyword, buf, err_msg);
	} else if (strcasecmp(keyword, "ND_ENABLE_NODEJS_ASYNC_EVENT_MONITOR") == 0) {
          kw_set_nd_enable_nodejs_async_event_monitor(keyword, buf, err_msg);
        } else if (strncasecmp(keyword, "ND_ENABLE_ENTRY_POINT_MONITOR", strlen("ND_ENABLE_ENTRY_POINT_MONITOR")) == 0) {
          kw_set_nd_enable_entry_point_monitor(keyword, buf, err_msg);
        } else if (strncasecmp(keyword, "ND_ENABLE_DB_CALL_MONITOR", strlen("ND_ENABLE_DB_CALL_MONITOR")) == 0) {
          kw_set_nd_enable_db_call_monitor(keyword, buf, err_msg);
        } else if (strncasecmp(keyword, "ND_ENABLE_BT_IP_MONITOR", strlen("ND_ENABLE_BT_IP_MONITOR")) == 0) {
          kw_set_nd_enable_bt_ip_monitor(keyword, buf, err_msg);
        } else if (strcasecmp(keyword, "ENABLE_DB_AGGREGATOR") == 0) {
            kw_set_enable_db_aggregator(keyword, buf);
        } else if (strcasecmp(keyword, "SKIP_UNKNOWN_BREADCRUMB") == 0) {
          kw_set_unknown_breadcrumb(keyword, buf);
        } else if (strncasecmp(keyword, "ND_ENABLE_BT_MONITOR", strlen("ND_ENABLE_BT_MONITOR")) == 0) {
          kw_set_nd_enable_bt_monitor(keyword, buf, err_msg);
        } else if (strncasecmp(keyword, "ND_ENABLE_METHOD_MONITOR_EX", strlen("ND_ENABLE_METHOD_MONITOR_EX")) == 0) {
          kw_set_nd_enable_method_monitor_ex(keyword, buf, err_msg);
        } else if (strncasecmp(keyword, "ND_ENABLE_HTTP_HEADER_CAPTURE_MONITOR", strlen("ND_ENABLE_HTTP_HEADER_CAPTURE_MONITOR")) == 0) {
          kw_set_nd_enable_http_header_capture_monitor(keyword, buf, err_msg);
        } else if (strncasecmp(keyword, "ND_ENABLE_EXCEPTIONS_MONITOR", strlen("ND_ENABLE_EXCEPTIONS_MONITOR")) == 0) {
          kw_set_nd_enable_exceptions_monitor(keyword, buf, err_msg);
        } else if (strncasecmp(keyword, "ND_ENABLE_BACKEND_CALL_MONITOR", strlen("ND_ENABLE_BACKEND_CALL_MONITOR")) == 0) {
          kw_set_nd_enable_backend_call_monitor(keyword, buf, err_msg);
        } else if (strncasecmp(keyword, "ND_ENABLE_NODE_GC_MONITOR", strlen("ND_ENABLE_NODE_GC_MONITOR")) == 0) {
          kw_set_nd_enable_node_gc_monitor(keyword, buf);
        } else if (strncasecmp(keyword, "ND_ENABLE_EVENT_LOOP_MONITOR", strlen("ND_ENABLE_EVENT_LOOP_MONITOR")) == 0) {
          kw_set_nd_enable_event_loop_monitor(keyword, buf);
        } else if (strncasecmp(keyword, "ND_ENABLE_FP_STATS_MONITOR_EX", strlen("ND_ENABLE_FP_STATS_MONITOR_EX")) == 0) {
          kw_set_nd_enable_fp_stats_monitor_ex(keyword, buf, err_msg);
        } else if (strncasecmp(keyword, "ND_ENABLE_BUSINESS_TRANS_STATS_MONITOR", strlen("ND_ENABLE_BUSINESS_TRANS_STATS_MONITOR")) == 0) {
          kw_set_nd_enable_business_trans_stats_monitor(keyword, buf, err_msg);
        } else if (strncasecmp(keyword, "ND_ENABLE_JVM_THREAD_MONITOR", strlen("ND_ENABLE_JVM_THREAD_MONITOR")) == 0) {
          kw_set_nd_enable_cpu_by_thread_monitor(keyword, buf, err_msg);
        } else if (strncasecmp(keyword, "ND_DATA_VALIDATION", strlen("ND_DATA_VALIDATION")) == 0) {
          kw_set_nd_enable_data_validation(keyword, buf, err_msg, 0);
        } else if (strncasecmp(keyword,"ND_ENABLE_MONITOR",strlen("ND_ENABLE_MONITOR")) == 0){
          kw_set_nd_enable_monitor(keyword, buf, err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_BP_ENTRY_PAGE_STATS", strlen("NV_ENABLE_BP_ENTRY_PAGE_STATS")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_bp_entry_page_stats.gdf", err_msg);
        } else if (strncasecmp(keyword, "ENABLE_VECTOR_HASH", strlen("ENABLE_VECTOR_HASH")) ==0) {
          kw_set_enable_vector_hash(keyword, buf, err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_BP_ABAND_TRANSIT_PAGE_EVENT_STATS", strlen("NV_ENABLE_BP_ABAND_TRANSIT_PAGE_EVENT_STATS")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_bp_aband_transit_page_event_stats.gdf", err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_BP_ABAND_SESSION_EXIT_PAGE_STATS", strlen("NV_ENABLE_BP_ABAND_SESSION_EXIT_PAGE_STATS")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_bp_aband_session_exit_page_stats.gdf", err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_BP_ABAND_SESSION_EXIT_PAGE_EVENT_STATS_MONITOR", strlen("NV_ENABLE_BP_ABAND_SESSION_EXIT_PAGE_EVENT_STATS_MONITOR")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_bp_aband_session_exit_page_event_stats.gdf", err_msg);         
        } else if (strncasecmp(keyword, "NV_ENABLE_BP_ABAND_BP_EXIT_PAGE_STATS_MONITOR", strlen("NV_ENABLE_BP_ABAND_BP_EXIT_PAGE_STATS_MONITOR")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_bp_aband_bp_exit_page_stats.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_BP_ABAND_BP_EXIT_PAGE_EVENT_STATS_MONITOR", strlen("NV_ENABLE_BP_ABAND_BP_EXIT_PAGE_EVENT_STATS_MONITOR")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_bp_aband_bp_exit_page_event_stats.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_PAGE_SCREEN_SIZE_MONITOR", strlen("NV_ENABLE_STATS_PAGE_SCREEN_SIZE_MONITOR")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_stats_page_screen_size.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_PAGE_PAGE_MONITOR", strlen("NV_ENABLE_STATS_PAGE_PAGE_MONITOR")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_stats_page_page.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_PAGE_OVERALL_MONITOR", strlen("NV_ENABLE_STATS_PAGE_OVERALL_MONITOR")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_stats_page_overall.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_PAGE_LOCATION_MONITOR", strlen("NV_ENABLE_STATS_PAGE_LOCATION_MONITOR")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_stats_page_location.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_PAGE_BROWSER_MONITOR", strlen("NV_ENABLE_STATS_PAGE_BROWSER_MONITOR")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_stats_page_browser.gdf", err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_PAGE_ACCESS_TYPE_MONITOR", strlen("NV_ENABLE_STATS_PAGE_ACCESS_TYPE_MONITOR")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_stats_page_access_type.gdf", err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_PAGE_DEVICE_TYPE_MONITOR", strlen("NV_ENABLE_STATS_PAGE_DEVICE_TYPE_MONITOR")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_stats_page_device_type.gdf", err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_EVENT_MONITOR", strlen("NV_ENABLE_STATS_EVENT_MONITOR")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_stats_event.gdf", err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_SEGMENT_EVENT_MONITOR", strlen("NV_ENABLE_STATS_SEGMENT_EVENT_MONITOR")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_stats_segment_events.gdf", err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_BP_STATS_MONITOR", strlen("NV_ENABLE_BP_STATS_MONITOR")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_bp_stats.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_PAGE_OPERATING_SYSTEM_MONITOR", strlen("NV_ENABLE_STATS_PAGE_OPERATING_SYSTEM_MONITOR")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_stats_page_operating_system.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_PAGE_STORE_MONITOR", strlen("NV_ENABLE_STATS_PAGE_STORE_MONITOR")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_stats_page_store.gdf",err_msg);
        }else if (strncasecmp(keyword, "NV_ENABLE_STATS_PAGE_EVENT_MONITOR", strlen("NV_ENABLE_STATS_PAGE_EVENT_MONITOR")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_stats_page_events.gdf",err_msg);
        }else if (strncasecmp(keyword, "NV_ENABLE_STATS_PAGE_BY_STORE_MONITOR", strlen("NV_ENABLE_STATS_PAGE_BY_STORE_MONITOR")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_stats_page_by_store.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_BROWSER_EVENT_MONITOR", strlen("NV_ENABLE_STATS_BROWSER_EVENT_MONITOR")) ==0){         kw_set_nv_enable_monitor(keyword, buf, "cm_nv_stats_browser_events.gdf", err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_EVENT_STORE_MONITOR", strlen("NV_ENABLE_STATS_PAGE_STORE_MONITOR")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_stats_store_events.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_SESSION_BROWSER_MONITOR", strlen("NV_ENABLE_STATS_SESSION_BROWSER_MONITOR")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_stats_session_browser.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_SESSION_ACCESS_TYPE_MONITOR", strlen("NV_ENABLE_STATS_SESSION_ACCESS_TYPE_MONITOR")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_stats_session_access_type.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_SESSION_LOCATION_MONITOR", strlen("NV_ENABLE_STATS_SESSION_LOCATION_MONITOR")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_stats_session_location.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_SESSION_OPERATING_SYSTEM_MONITOR", strlen("NV_ENABLE_STATS_SESSION_OPERATING_SYSTEM_MONITOR")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_stats_session_operating_system.gdf", err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_SESSION_SCREEN_SIZE_MONITOR", strlen("NV_ENABLE_STATS_SESSION_SCREEN_SIZE_MONITOR")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_stats_session_screen_size.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_SESSION_DEVICE_TYPE_MONITOR", strlen("NV_ENABLE_STATS_SESSION_DEVICE_TYPE_MONITOR")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_stats_session_device_type.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_SESSION_OVERALL_MONITOR", strlen("NV_ENABLE_STATS_SESSION_OVERALL_MONITOR")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_stats_session_overall.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_XHR_MONITOR", strlen("NV_ENABLE_STATS_XHR_MONITOR")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_xhr_stats.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_UA_MONITOR", strlen("NV_ENABLE_STATS_UA_MONITOR")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_stats_action.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_FORM_ANALYTICS", strlen("NV_ENABLE_STATS_FORM_ANALYTICS")) ==0){
            kw_set_nv_enable_monitor(keyword, buf, "cm_nv_stats_form_analytics.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_FORM_FIELD_ANALYTICS", strlen("NV_ENABLE_STATS_FORM_FIELD_ANALYTICS")) ==0){
            kw_set_nv_enable_monitor(keyword, buf, "cm_nv_stats_form_field_analytics.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_BOT_MONITOR", strlen("NV_ENABLE_STATS_BOT_MONITOR")) == 0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_bot_session_stats.gdf",err_msg);
        } else if(strncasecmp(keyword, "NV_ENABLE_STATS_VISITOR_MONITOR",strlen("NV_ENABLE_STATS_VISITOR_MONITOR")) == 0){
            kw_set_nv_enable_monitor(keyword,buf, "cm_nv_stats_visitor_info.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_UA_MONITOR_BY_STORE", strlen("NV_ENABLE_UA_MONITOR_BY_STORE")) == 0){
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_stats_action_by_store.gdf", err_msg);
        }  else if (strncasecmp(keyword, "NV_ENABLE_STATS_USER_SESS_SEGMENT_MONITOR", strlen("NV_ENABLE_STATS_USER_SESS_SEGMENT_MONITOR")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_stats_session_segments.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_USER_PAGE_SEGMENT_MONITOR", strlen("NV_ENABLE_STATS_USER_PAGE_SEGMENT_MONITOR")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_stats_page_segments_overall.gdf",err_msg);
        }
        else if (strncasecmp(keyword, "NV_ENABLE_STATS_USER_PAGE_LOCATION_SEGMENT_MONITOR", strlen("NV_ENABLE_STATS_USER_PAGE_LOCATION_SEGMENT_MONITOR")) == 0){
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_stats_page_segments_location.gdf", err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_USER_SESS_LOCATION_SEGMENT_MONITOR", strlen("NV_ENABLE_STATS_USER_SESS_SEGMENT_LOCATION_MONITOR")) == 0){
         kw_set_nv_enable_monitor(keyword, buf, "cm_nv_stats_session_segments_location.gdf", err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_USER_PAGE_PAGE_SEGMENT_MONITOR", strlen("NV_ENABLE_STATS_USER_PAGE_PAGE_SEGMENT_MONITOR")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_stats_page_segments_page.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_APP_CRASH", strlen("NV_ENABLE_STATS_APP_CRASH")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_crash_stats_application.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_CRASH_CONNECTION", strlen("NV_ENABLE_STATS_CRASH_CONNECTION")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_crash_stats_connection_type.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_CRASH_DEVICE", strlen("NV_ENABLE_STATS_CRASH_DEVICE")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_crash_stats_device.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_OS_CRASH", strlen("NV_ENABLE_STATS_OS_CRASH")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_crash_stats_os.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_OVERALL_CRASH", strlen("NV_ENABLE_STATS_OVERALL_CRASH")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_crash_stats_overall.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_APP_START_TIME_OVERALL", strlen("NV_ENABLE_APP_START_TIME_OVERALL")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_app_start_time_overall.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_APP_START_TIME_APPLICATION_MONITOR", strlen("NV_ENABLE_APP_START_TIME_APPLICATION_MONITOR")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_app_start_time_application_type.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_APP_START_TIME_LOCATION_MONITOR", strlen("NV_ENABLE_APP_START_TIME_LOCATION_MONITOR")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_app_start_time_location.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_APP_START_TIME_CARRIER_MONITOR", strlen("NV_ENABLE_APP_START_TIME_CARRIER_MONITOR")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_app_start_time_mobile_carrier_type.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_APP_START_TIME_OS_MONITOR", strlen("NV_ENABLE_APP_START_TIME_OS_MONITOR")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_app_start_time_os_type.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_HTTP_REQUEST_OVERALL", strlen("NV_ENABLE_STATS_HTTP_REQUEST_OVERALL")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_http_request_overall_stats.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_HTTP_REQUEST_OS", strlen("NV_ENABLE_STATS_HTTP_REQUEST_OS")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_http_request_os_stats.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_HTTP_REQUEST_DEVICE", strlen("NV_ENABLE_STATS_HTTP_REQUEST_DEVICE")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_http_request_device_stats.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_HTTP_REQUEST_CONNECTION_TYPE", strlen("NV_ENABLE_STATS_HTTP_REQUEST_CONNECTION_TYPE")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_http_request_connection_type_stats.gdf",err_msg);
        }else if (strncasecmp(keyword, "NV_ENABLE_STATS_HTTP_REQUEST_APPLICATION", strlen("NV_ENABLE_STATS_HTTP_REQUEST_APPLICATION")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_http_request_application_stats.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_HTTP_REQUEST_MOBILE_CARRIER", strlen("NV_ENABLE_STATS_HTTP_REQUEST_MOBILE_CARRIER")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_http_request_mobile_carrier_stats.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_HTTP_REQUEST_DOMAIN", strlen("NV_ENABLE_STATS_HTTP_REQUEST_DOMAIN")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_http_request_domain.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_HTTP_REQUEST_VIEW", strlen("NV_ENABLE_STATS_HTTP_REQUEST_VIEW")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_http_request_view.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_HTTP_REQUEST_BROWSER", strlen("NV_ENABLE_STATS_HTTP_REQUEST_BROWSER")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_http_request_browser.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_HTTP_REQUEST_LOCATION", strlen("NV_ENABLE_STATS_HTTP_REQUEST_LOCATION")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_http_request_location.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_HTTP_REQUEST_XHR", strlen("NV_ENABLE_STATS_HTTP_REQUEST_XHR")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_http_request_xhr.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_HTTP_REQUEST_STORE", strlen("NV_ENABLE_STATS_HTTP_REQUEST_STORE")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_http_request_store.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_HTTP_REQUEST_ACCESSTYPE", strlen("NV_ENABLE_STATS_HTTP_REQUEST_ACCESSTYPE")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_http_request_accesstype.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_HTTP_ERROR_OVERALL", strlen("NV_ENABLE_STATS_HTTP_ERROR_OVERALL")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_http_error_overall_stats.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_HTTP_ERROR_APPLICATION", strlen("NV_ENABLE_STATS_HTTP_ERROR_APPLICATION")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_http_error_application_stats.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_HTTP_ERROR_OS", strlen("NV_ENABLE_STATS_HTTP_ERROR_OS")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_http_error_os_stats.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_HTTP_ERROR_DOMAIN", strlen("NV_ENABLE_STATS_HTTP_ERROR_DOMAIN")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_http_error_domain.gdf",err_msg); 
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_HTTP_ERROR_VIEW", strlen("NV_ENABLE_STATS_HTTP_ERROR_VIEW")) ==0) {
           kw_set_nv_enable_monitor(keyword, buf, "cm_nv_http_error_view.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_HTTP_ERROR_BROWSER", strlen("NV_ENABLE_STATS_HTTP_ERROR_BROWSER")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_http_error_browser.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_HTTP_ERROR_LOCATION", strlen("NV_ENABLE_STATS_HTTP_ERROR_LOCATION")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_http_error_location.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_HTTP_ERROR_XHR", strlen("NV_ENABLE_STATS_HTTP_ERROR_XHR")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_http_error_xhr.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_HTTP_ERROR_STORE", strlen("NV_ENABLE_STATS_HTTP_ERROR_STORE")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_http_error_store.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_HTTP_ERROR_ACCESSTYPE", strlen("NV_ENABLE_STATS_HTTP_ERROR_ACCESSTYPE")) ==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_http_error_accesstype.gdf",err_msg); 
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_HTTP_ERROR_MOBILE_CARRIER", strlen("NV_ENABLE_STATS_HTTP_ERROR_MOBILE_CARRIER"))==0) {
          kw_set_nv_enable_monitor(keyword, buf, "cm_nv_http_error_mobile_carrier_stats.gdf",err_msg);
        } else if(strncasecmp(keyword, "NV_ENABLE_STATS_RESOURCE_DOMAIN_BY_PAGE", strlen("NV_ENABLE_STATS_RESOURCE_DOMAIN_BY_PAGE")) ==0) {
           kw_set_nv_enable_monitor(keyword, buf, "cm_nv_resource_domain_page.gdf",err_msg); 
        }else if(strncasecmp(keyword, "NV_ENABLE_STATS_RESOURCE_DOMAIN", strlen("NV_ENABLE_STATS_RESOURCE_DOMAIN")) ==0) {
           kw_set_nv_enable_monitor(keyword, buf, "cm_nv_resource_domain_overall.gdf",err_msg);
        }  else if (strncasecmp(keyword, "NV_ENABLE_TRAFFIC_STAT", strlen("NV_ENABLE_TRAFFIC_STAT")) ==0) {
            kw_set_nv_enable_monitor(keyword, buf, "cm_nv_traffic_stats.gdf",err_msg);
        }  else if (strncasecmp(keyword, "NV_ENABLE_VARIATION_STATS", strlen("NV_ENABLE_VARIATION_STATS")) ==0) {
            kw_set_nv_enable_monitor(keyword, buf, "cm_nv_variation_stats.gdf",err_msg);
        }  else if (strncasecmp(keyword, "NV_ENABLE_STATS_CONTAINING_PAGE_EVENT", strlen("NV_ENABLE_STATS_CONTAINING_PAGE_EVENT")) ==0) {
            kw_set_nv_enable_monitor(keyword, buf, "cm_nv_stats_session_containing_pages_and_event.gdf",err_msg);
        }  else if (strncasecmp(keyword, "NV_ENABLE_STATS_CONTAINING_PAGE", strlen("NV_ENABLE_STATS_CONTAINING_PAGE")) ==0) {
            kw_set_nv_enable_monitor(keyword, buf, "cm_nv_stats_session_containing_pages.gdf",err_msg);     
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_PAGE_BY_CONNECTIONTYPE_MONITOR", strlen("NV_ENABLE_STATS_PAGE_BY_CONNECTIONTYPE_MONITOR")) ==0) {
            kw_set_nv_enable_monitor(keyword, buf, "cm_nv_stats_page_by_connection_type.gdf",err_msg);
        } else if (strncasecmp(keyword, "NV_ENABLE_STATS_PAGE_CONNECTIONTYPE_MONITOR", strlen("NV_ENABLE_STATS_PAGE_CONNECTIONTYPE_MONITOR")) ==0) {
           kw_set_nv_enable_monitor(keyword, buf, "cm_nv_stats_page_connection_type.gdf",err_msg);
        } else if(strncasecmp(keyword, "NV_ENABLE_STATS_PAGE_APPLICATION_MONIOTR", strlen("NV_ENABLE_STATS_PAGE_APPLICATION_MONIOTR")) == 0) {
           kw_set_nv_enable_monitor(keyword, buf, "cm_nv_stats_application_by_page_type.gdf",err_msg);
        }  else if(strcasecmp(keyword, "STANDARD_MONITOR") == 0){
          kw_set_standard_monitor(keyword, buf, 0, NULL, err_msg, NULL);
        } else if (strcasecmp(keyword, "USER_MONITOR") == 0) {
          user_monitor_config(keyword, buf);
        } else if (strcasecmp(keyword, "PRE_TEST_CHECK") == 0) {
          printf("Keyword 'PRE_TEST_CHECK' no longer exist, it has been renamed as 'CHECK_MONITOR'\n");
        } else if (strcasecmp(keyword, "CHECK_MONITOR") == 0) {
          kw_set_check_monitor(keyword, buf, 0, err_msg, NULL);
        } else if (strcasecmp(keyword, "PRE_TEST_CHECK_TIMEOUT") == 0) {
          kw_set_pre_test_check_timeout(keyword, text, buf, err_msg, 0);
        } else if (strcasecmp(keyword, "POST_TEST_CHECK_TIMEOUT") == 0) {
          kw_set_post_test_check_timeout(keyword, text, buf, err_msg, 0);
        } else if (strcasecmp(keyword, "DYNAMIC_VECTOR_TIMEOUT") == 0) {
          kw_set_dynamic_vector_timeout(keyword, text, buf, err_msg, 0);
        } else if (strcasecmp(keyword, "DYNAMIC_VECTOR_MONITOR_RETRY_COUNT") == 0) {
          kw_set_dynamic_vector_monitor_retry_count(keyword, text, buf, err_msg, 0);
        } else if (strcasecmp(keyword, "SERVER_SIGNATURE") == 0) {
          kw_set_server_signature(keyword, buf, 0, err_msg, NULL);
        } else if (strcasecmp(keyword, "ENABLE_JAVA_PROCESS_SERVER_SIGNATURE") == 0) { 
          kw_set_java_process_server_signature(keyword, buf, 0, err_msg);
        } else if (strcasecmp(keyword, "COHERENCE_NID_TABLE_SIZE") == 0) { 
          kw_set_coherence_nodeid_table_size(keyword, buf, 0, err_msg);
        } else if (strcasecmp(keyword, "MONITOR_PROFILE") == 0) { 
          kw_set_monitor_profile(text, num, err_msg, 0, NULL);
        } else if(strcasecmp(keyword, "ENABLE_NS_MONITORS") == 0 ){
          kw_set_enable_ns_monitors(keyword, buf);
        } else if(strcasecmp(keyword, "ENABLE_NO_MONITORS") == 0 ){
          kw_set_enable_no_monitors(keyword, buf);
        } else if (strcasecmp(keyword, "DISABLE_NS_MONITORS") == 0 || strcasecmp(keyword, "DISABLE_NO_MONITORS") == 0) {
          kw_set_disable_ns_no_monitors(keyword, buf);
        } else if(strcasecmp(keyword, "ENABLE_MONITOR_DELETE_FREQUENCY") == 0 ){
          kw_set_enable_monitor_delete_frequency(keyword, buf);
        } else if(strcasecmp(keyword, "ENABLE_AUTO_JSON_MONITOR") == 0 ){
          kw_set_enable_auto_json_monitor(keyword, buf, 0, err_msg);
        } else if(strcasecmp(keyword, "ENABLE_CMON_AGENT") == 0 ){
          kw_set_enable_cmon_agent(keyword, buf);
	} else if (strcasecmp(keyword, "DISABLE_DVM_VECTOR_LIST") == 0) {
          kw_set_disable_dvm_vector_list(keyword, buf);
        } else if(strcasecmp(keyword,"ENABLE_AUTO_SERVER_SIGNATURE") == 0){
          kw_set_enable_auto_server_sig(keyword,buf);
        } else if(strcasecmp(keyword, "CMON_SETTINGS") == 0 ){
          kw_set_cmon_settings(buf, err_msg, 0);
        } else if(strcasecmp(keyword, "GROUP_HIERARCHY_CONFIG") == 0 ){
          kw_set_enable_store_config(keyword, buf);
        } else if(strcasecmp(keyword, "ENABLE_HML_GROUPS") == 0 ){
          kw_enable_hml_group_in_testrun_gdf(keyword, buf, err_msg, 0);
        } else if(strcasecmp(keyword, "ENABLE_MONITOR_DR") == 0 ){
          kw_set_enable_monitor_dr(keyword, buf);
        } else if(strcasecmp(keyword, "ENABLE_CHECK_MONITOR_DR") == 0 ){
          kw_set_enable_chk_monitor_dr(keyword, buf, 0, err_msg);
        } else if(strcasecmp(keyword, "ENABLE_DATA_CONN_HB") == 0 ){
          kw_set_enable_data_conn_hb(keyword, buf, err_msg, 0);
        } else if(strcasecmp(keyword, "ENABLE_ALERT_LOG_MONITOR") == 0 ){
          kw_set_enable_alert_rule_monitor(keyword, buf, err_msg);
        } /*else if (strncasecmp(keyword, "ALERT_PROFILE", strlen("ALERT_PROFILE")) == 0) 
          kw_set_alert_profile(text, num);
        */
          else if (strcasecmp(keyword, "SHOW_IP_DATA") == 0) {
           kw_set_ip_based_data(buf,err_msg);
        } else if (strcasecmp(keyword, "SSL_ATTACK_FILE") == 0) {
          #if OPENSSL_VERSION_NUMBER >= 0x10100000L
            NSTL1(NULL, NULL, "SSL_ATTACK_FILE keyword disabled");
          #else
            kw_set_ssl_attack_file(keyword, buf);
          #endif
        } else if (strcasecmp(keyword, "HIGH_PERF_MODE") == 0) {
	  set_high_perf_mode(text);
	} else if (strcasecmp(keyword, "MAX_SOCK") == 0) {
	  set_max_sock(text);
        } else if (strcasecmp(keyword, "BATCH_JOB_GROUP") == 0) {
          kw_set_batch_job_group(text, num, 0, err_msg);
        } else if (strcasecmp(keyword, "PERCENTILE_REPORT") == 0) {
          kw_set_percentile_report(buf, 0, err_msg);
        } else if (strcasecmp(keyword, "URL_PDF") == 0) {
          kw_set_url_pdf_file(buf, 0, err_msg);
        } else if (strcasecmp(keyword, "PAGE_PDF") == 0) {
          kw_set_page_pdf_file(buf, 0, err_msg);
        } else if (strcasecmp(keyword, "SESSION_PDF") == 0) {
          kw_set_session_pdf_file(buf, 0, err_msg);
        } else if (strcasecmp(keyword, "TRANSACTION_RESPONSE_PDF") == 0) {
          kw_set_trans_resp_pdf_file(buf, 0, err_msg);
        } else if (strcasecmp(keyword, "TRANSACTION_TIME_PDF") == 0) {
          kw_set_trans_time_pdf_file(buf, 0, err_msg);
        } else if (strcasecmp(keyword, "RUN_TIME") == 0) {
          kw_set_run_time(buf, global_settings, 0);
	} else if (strcasecmp(keyword, "TNAME") == 0) {
          kw_set_testname(buf, global_settings, 0);
	} else if (strcasecmp(keyword, "MASTER") == 0) {
          if(kw_set_master(buf, global_settings, 0)) 
            continue;
	} else if (strcasecmp(keyword, "NUM_USERS") == 0) {
          /* Now global_settings has no fields use_pct_prof 
          if(testCase.mode != TC_FIX_CONCURRENT_USERS && global_settings->use_pct_prof != 1) 
            NSTL1_OUT(NULL, NULL,  "Warning: NUM_USERS can only be specified with Fix Concurrent Users in Percentage mode.\n");
          else
          */
          if (global_settings->use_prof_pct == PCT_MODE_NUM)
          { 
            NS_DUMP_WARNING("Ignoring NUM_USERS in NUM mode");
          }
          else if (global_settings->use_prof_pct == PCT_MODE_PCT && 
                   global_settings->schedule_type == SCHEDULE_TYPE_ADVANCED)
          {
            NS_DUMP_WARNING("Ignoring NUM_USERS in SCHEDULE_TYPE_ADVANCED and PCT mode schedule");
          }
          else
	    global_settings->num_connections = atoi(text);
	} else if (strcasecmp(keyword, "MAX_USERS") == 0) {
          kw_set_max_users(buf, err_msg, 0);
	} else if (strcasecmp(keyword, "NUM_DIRS") == 0) {
	  global_settings->num_dirs = atoi(text);
	} else if (strcasecmp(keyword, "LOG_SHR_BUFFER_SIZE") == 0) {
          if (kw_set_log_shr_buf_size(buff, err_msg, 0) != 0)
            NS_EXIT(-1, "%s", err_msg);
        } else if (strcasecmp(keyword, "WAN_ENV") == 0) {
          kw_set_wan_env(text);
	}// According to bug 23122, parsing of keyword ENABLE_NS_FIREFOX move to read_scripts_glob_vars() 
           /* else if (strcasecmp(keyword, "ENABLE_NS_FIREFOX") == 0) {
            if(kw_set_ns_firefox(buf, err_msg, 0) != 0)
            {
              NSTL1_OUT(NULL, NULL,  "%s", err_msg);
              exit (-1);
            }
	}*/
       /* else if (strcasecmp(keyword, "ENABLE_NS_CHROME") == 0) {
          if(kw_set_ns_chrome(buf, err_msg, 0) != 0)
          {
            NSTL1_OUT(NULL, NULL,  "%s", err_msg);
            exit (-1);
          }
        }*/
          else if (strcasecmp(keyword, "RBU_USER_AGENT") == 0) {
           if(kw_set_rbu_user_agent(buf, NULL, err_msg, 0) == 1)
             NSDL3_SCHEDULE(NULL, NULL, "RBU_USER_AGENT disabled");
           else
             exit (-1);
        } else if (strcasecmp(keyword, "RBU_SCREEN_SIZE_SIM") == 0) {
           if(kw_set_rbu_screen_size_sim(buf, NULL, err_msg, 0) == 1)
             NSDL3_SCHEDULE(NULL, NULL, "RBU_SCREEN_SIZE_SIM disabled");
           else
             exit (-1);
	} else if (strcasecmp(keyword, "RBU_ENABLE_DUMMY_PAGE") == 0) {
          if (kw_set_rbu_enable_dummy_page(buf, err_msg, 0) != 0)
          {
            NS_EXIT(-1, "%s", err_msg);
          }
        } else if (strcasecmp(keyword, "LOG_VUSER_DATA_INTERVAL") == 0) {
          if (kw_set_log_vuser_data_interval(buf, err_msg, 0) != 0)
          {
            NS_EXIT(-1, "%s", err_msg);
          }
	} else if (strcasecmp(keyword, "ADJUST_RAMPUP_TIMER") == 0) {
          if (kw_set_adjust_rampup_timer(buf, err_msg, 0) != 0)
          {
            NS_EXIT(-1, "%s", err_msg);
          }
        } else if (strcasecmp(keyword, "SECONDARY_GUI_SERVER") == 0) {
          kw_set_ns_server_secondary(buf, 0);
        } else if (strcasecmp(keyword, "NUM_NVM") == 0 && (loader_opcode != MASTER_LOADER)) {
          kw_set_num_nvm(buf, global_settings, 0, err_msg);
	} else if (strcasecmp(keyword, "WARMUP_TIME") == 0) {
          kw_set_warmup_time(buf, global_settings, 0);
	} else if (strcasecmp(keyword, "PROGRESS_MSECS") == 0) {
           kw_set_progress_msecs(text, buf, err_msg, 0);
	  //global_settings->progress_secs = atoi(text);
	} //else if (strncasecmp(keyword, "DEFAULT_PAGE_THINK_TIME", strlen("DEFAULT_PAGE_THINK_TIME")) == 0) {
          //kw_set_default_page_think_time(buf, 0);
  /*	} else if (strncasecmp(keyword, "CLICKAWAY_GLOBAL_PROFILE", strlen("CLICKAWAY_GLOBAL_PROFILE")) == 0) {
          kw_set_clickaway_global_profile(buf, global_settings, 0);
	} else if (strncasecmp(keyword, "CLICKAWAY_PROFILE", strlen("CLICKAWAY_PROFILE")) == 0) {
          kw_set_clickaway_profile(buf, global_settings, 0);
	} else if (strncasecmp(keyword, "PAGE_CLICKAWAY", strlen("PAGE_CLICKAWAY")) == 0) {
          kw_set_page_clickaway(buf, global_settings, 0);
        } else if (strncasecmp(keyword, "THINK_TIME_MODE", strlen("THINK_TIME_MODE")) == 0) {
	  global_settings->think_time_mode = atoi(text);
	} else if (strncasecmp(keyword, "MEAN_THINK_MSECS", strlen("MEAN_THINK_MSECS")) == 0) {
	  global_settings->mean_think_time = atoi(text);
	} else if (strncasecmp(keyword, "MEDIAN_THINK_MSECS", strlen("MEDIAN_THINK_MSECS")) == 0) {
	  global_settings->median_think_time = atoi(text);
	} else if (strncasecmp(keyword, "VAR_THINK_MSECS", strlen("VAR_THINK_MSECS")) == 0) {
	  global_settings->var_think_time = atoi(text);
  */
          //global_settings->user_cleanup_pct = pct;
  /*	} else if (strncasecmp(keyword, "USER_REUSE_MODE", strlen("USER_REUSE_MODE")) == 0) {
          global_settings->user_reuse_mode = atoi(text);
  */
/*
        } else if (strncasecmp(keyword, "RAMP_UP_MODE", strlen("RAMP_UP_MODE")) == 0) {
          kw_set_ramp_up_mode(buf);
	} else if (strncasecmp(keyword, "RAMP_DOWN_MODE", strlen("RAMP_DOWN_MODE")) == 0)  
          kw_set_ramp_down_mode(buf);
*/
          else if (strcasecmp(keyword, "SESSION_RATE_MODE") == 0) {
	  global_settings->user_rate_mode = atoi(text);
#if 0
	  else if (strncasecmp(keyword, "NS_FACTOR", strlen("NS_FACTOR")) == 0) 
	  global_settings->ns_factor = atoi(text);
#endif
	} else if (strcasecmp(keyword, "CLIENT") == 0) {
          if(kw_set_client(text, 0))
            continue;
	} else if (strcasecmp(keyword, "MEMPERF") == 0) { 
          kw_set_mem_perf(buf);
	} else if (strcasecmp(keyword, "IP_FILE") == 0) {
	  ;
#if 0
	  strncpy(global_settings->ip_fname, text, MAX_FILE_NAME);
#endif
	} else if (strcasecmp(keyword, "SPEC_URL_PREFIX") == 0) {
	  strncpy(global_settings->spec_url_prefix, text, MAX_FILE_NAME);
	} else if (strcasecmp(keyword, "SPEC_URL_SUFFIX") == 0) {
	  strncpy(global_settings->spec_url_suffix, text, MAX_FILE_NAME);
	} else if (strcasecmp(keyword, "THRESHOLD") == 0) {
          kw_set_threshold(buf, global_settings, 0);
	} else if (strcasecmp(keyword, "LOGDATA_PROCESS") == 0) {
          kw_set_logdata_process(buf, global_settings, 0);
	} else if (strcasecmp(keyword, "INTERACTIVE") == 0) {
	  global_settings->interactive = atoi(text);
	} else if (strcasecmp(keyword, "SHOW_INITIATED") == 0) {
	  global_settings->show_initiated = atoi(text);
	} else if (strcasecmp(keyword, "NON_RANDOM_TIMERS") == 0) {
	  global_settings->non_random_timers = atoi(text);
	} else if (strcasecmp(keyword, "SRC_PORT_MODE") == 0) {
	  kw_set_src_port_mode(buf);
	} else if (strcasecmp(keyword, "HEALTH_MONITOR") == 0) {
	  //global_settings->health_monitor_on = atoi(text);
	  //global_settings->smon = atoi(text);
          kw_set_health_mon(buf, err_msg, 0);
        /*
	} else if (strcasecmp(keyword, "REPLAY_FILE") == 0) {
          kw_set_replay_file(buf, 0);
        */
	} else if (strcasecmp(keyword, "REPLAY_FACTOR") == 0) {
          kw_set_replay_factor(buf, err_msg, 0);
	} else if (strcasecmp(keyword, "REPLAY_RESUME_OPTION") == 0) {
          kw_set_replay_resume_option(buf, 0);
	} else if (strcasecmp(keyword, "GUESS") == 0) {
            if (kw_set_guess(buf) == 1)
              continue;
	} else if (strcasecmp(keyword, "STABILIZE") == 0) {
           kw_set_stablize(buf);
	} else if (strcasecmp(keyword, "TARGET_RATE") == 0) {
          kw_set_target_rate(buf, global_settings, 0);
	} else if (strcasecmp(keyword, "SLA") == 0) {
          kw_set_sla(buf); // Function in ns_goal_based_sla.c
	} else if (strcasecmp(keyword, "METRIC") == 0) {
          if(kw_set_metric(buf, 0))
            continue;;
        } else if (strcasecmp(keyword, "HOST_TLS_VERSION") == 0){
          kw_set_host_tls_version(keyword, buf, err_msg, 0);
	} else if (strcasecmp(keyword, "DEFAULT_SERVER_LOCATION") == 0) {
	  MY_MALLOC (default_svr_location, strlen(text) + 1, "default_svr_location", -1);
	  strcpy(default_svr_location, text);
	} else if (strcasecmp(keyword, "CAPACITY_CHECK") == 0) {
          kw_set_capacity_check(buf);
	} else if (strcasecmp(keyword, "ADVERSE_FACTOR") == 0) {
          kw_set_adverse_factor(buf, err_msg, 0);
	} else if (strcasecmp(keyword, "WAN_JITTER") == 0) {
          kw_set_wan_jitter(buf, err_msg, 0);
/*
	  else if (strncasecmp(keyword, "LOAD_KEY", strlen("LOAD_KEY")) == 0) 
          kw_set_load_key(text);
*/
	} else if (strcasecmp(keyword, "DISPLAY_REPORT") == 0) {
	  global_settings->display_report = atoi(text);
#if 0
	  else if (strncasecmp(keyword, "ETH_INTERFACE", strlen("ETH_INTERFACE")) == 0) 
	  strcpy(global_settings->eth_interface, text);
#endif
	} else if (strcasecmp(keyword, "RESP_LOGGING") == 0) {
	  global_settings->resp_logging_size = atoi(text);
#ifdef RMI_MODE
	} else if (strcasecmp(keyword, "CONN_RETRY_TIME") == 0) {
	  global_settings->conn_retry_time = atoi(text);
#endif
	/*} else if (strncasecmp(keyword, "USE_PROF_PCT", strlen("USE_PROF_PCT")) == 0) {
	  global_settings->use_pct_prof = atoi(text);*/
	} else if (strcasecmp(keyword, "REPORT_MASK") == 0) {
	  global_settings->report_mask = atoi(text);
	} else if (strcasecmp(keyword, "NET_REPORT_ONLY") == 0) {
	  if (atoi(text))
    	 	global_settings->report_mask = 0;
	} else if (strcasecmp(keyword,"LOG_INLINE_BLOCK_TIME") == 0) {
            kw_log_inline_block_time(buf);
        } else if (strcasecmp(keyword, "ENABLE_PAGE_BASED_STATS") == 0){
            kw_set_page_based_stat(buf, err_msg, 0);
        } else if (strcasecmp(keyword, "AUTO_SCALE_CLEANUP_SETTING") == 0) {
          kw_set_enable_auto_scale_cleanup_setting(buf);
        } else if (strcasecmp(keyword, "NS_ALLOC_EXTRA_BYTES") == 0) {
            kw_set_ns_alloc_extra_bytes(buf);
        }else if (strcasecmp(keyword, "DVM_MALLOC_DELTA_SIZE") == 0) {
            kw_set_dvm_malloc_delta_size(buf);
        }else if (strcasecmp(keyword, "SERVER_HEALTH") == 0) {
            kw_set_server_health(keyword,buf,err_msg);
        } else if(strcasecmp(keyword, "DR_TABLE_SIZE") == 0) {
           kw_set_allocate_size_for_dr_table(keyword, buf);
	} else if(strcasecmp(keyword, "COHERENCE_REMOVE_TRAILING_CHARACTER") == 0) {
           kw_coherenece_remove_trailing_space(keyword, buf);
        } else if(strcasecmp(keyword, "PARENT_EPOLL_EVENT_SIZE") == 0) {
           kw_set_parent_epoll_event_size(keyword, buf);
        } else if (strcasecmp(keyword, "MONITOR_DEBUG_LEVEL") == 0){
           kw_set_monitor_debug_level(keyword, buf);
        } else if (strcasecmp(keyword, "ENABLE_AUTO_MONITOR_REGISTRATION") == 0) {
           kw_set_enable_auto_monitor_registration(keyword, buf);
        } else if (strcasecmp(keyword, "TCP_KEEPALIVE") == 0) {
           kw_set_tcp_keepalive(buf);
        }
      }
      //out side if(all_keywords)
      if (strcasecmp(keyword, "LOCATION") == 0) {
        kw_set_location(buf, 0);
      } else if (strcasecmp(keyword, "ULOCATION") == 0) {
        kw_set_ulocation(buf, 0);
      } else if (strcasecmp(keyword, "UPLOCATION") == 0) {
        kw_set_uplocation(buf, 0);
      } else if (strcasecmp(keyword, "UACCESS") == 0) {
        kw_set_uaccess(buf, 0, err_msg);
      } else if (strcasecmp(keyword, "UPACCESS") == 0) {
        kw_set_upaccess(buf, 0);
      } else if (strcasecmp(keyword, "UPAL") == 0) {
        kw_set_upal(buf, 0);
      } else if (strcasecmp(keyword, "UBROWSER") == 0) {
        kw_set_ubrowser(buf, 0);
      } else if (strcasecmp(keyword, "UPBROWSER") == 0) {
        kw_set_upbrowser(buf, 0, err_msg);
      /*}	else if (strcasecmp(keyword, "UMACHINE") == 0) {
        kw_set_umachine(buf, 0);
      } else if (strcasecmp(keyword, "UPMACHINE") == 0) {
        kw_set_upmachine(buf, 0);
      } else if (strcasecmp(keyword, "UFREQ") == 0) {
        kw_set_ufreq(buf, 0);
      } else if (strcasecmp(keyword, "UPFREQ") == 0) {
        kw_set_upfreq(buf, 0);*/
      } else if (strcasecmp(keyword, "UPSCREEN_SIZE") == 0) {
        kw_set_upscreen_size(buf, 0);
//      } else if (strncasecmp(keyword, "URL_HASH_TABLE_OPTION", strlen("URL_HASH_TABLE_OPTION")) == 0) {
//        kw_set_url_hash_table_option(buf, 0);
      } else if (strcasecmp(keyword, "NETSTORM_DIAGNOSTICS") == 0) {
        kw_set_netstorm_diagnostics (buf);
      } else if (strcasecmp(keyword, "LPS_SERVER") == 0) {
        kw_set_log_server (buf, 0);
      } else if (strcasecmp(keyword, "CONTROLLER_IP") == 0) {
        kw_set_controller_server_ip(buf, 0, (global_settings->ctrl_server_ip), (int*)&(global_settings->ctrl_server_port));
      } 
       #ifdef NS_DEBUG_ON
         else if (strncasecmp(keyword, "LIB_DEBUG", strlen("LIB_DEBUG")) == 0) {
          set_nslb_debug(buf);

      } else if (strcasecmp(keyword, "LIB_MODULEMASK") == 0) {
        if (set_nslb_modulemask(buf, err_msg) != 0)
          NS_EXIT(-1, "Invalid module mask supplied by user, error: %s", err_msg);

          char log_file[1024];
          char error_log_file[1024];

          sprintf(log_file, "%s/logs/TR%d/debug.log", g_ns_wdir, testidx);
          sprintf(error_log_file, "%s/logs/%s/error.log", g_ns_wdir, global_settings->tr_or_partition);
 
          nslb_util_set_log_filename(log_file, error_log_file); 
      }
       #endif 
      else if (strncasecmp(keyword, "G_", strlen("G_")) == 0) {
        parse_group_keywords(buf, 2, line_num);
      } else if (strcasecmp(keyword, "HEALTH_MONITOR_DISK_FREE") == 0) {
        nslb_kw_set_disk_free(buf, 2, err_msg, 0);
      }else if (strcasecmp(keyword, "HEALTH_MONITOR_INODE_FREE") == 0) {
        nslb_kw_set_inode_free(buf,2, err_msg, 0);
      } else if (strcasecmp(keyword, "ENABLE_PROGRESS_REPORT") == 0) {
        kw_enable_progress_report(buf);
      } else if (strcasecmp(keyword, "NS_GET_GEN_TR") == 0) {
        kw_enable_gen_tr (buf);
      } else if (strcasecmp(keyword, "LOGGING_WRITER_ARG") == 0) {
        kw_set_ns_logging_writer_arg(buf);
      } else if (strcasecmp(keyword, "MAX_LOGGING_BUFS") == 0) {
        kw_set_max_logging_bufs(buf);
      } else if (strcasecmp(keyword, "CONTINUE_TEST_ON_NVM_FAILURE") == 0) {
        kw_set_ns_nvm_fail(buf, &(global_settings->nvm_fail_continue));
      } else if (strcasecmp(keyword, "NS_TRACE_LEVEL") == 0) {
        kw_set_ns_trace_level(buf, err_msg, 0);
      }else if (strcasecmp(keyword, "MONITOR_TRACE_LEVEL") == 0) {
        kw_set_mon_log_trace_level(buf, err_msg);
       /*else if (strcasecmp(keyword, "DISABLE_SCRIPT_COPY_TO_TR") == 0) {
        kw_set_disable_copy_script(buf);
      }*/
      }else if (strcasecmp(keyword, "SERVER_SELECT_MODE") == 0) {
        kw_set_server_select_mode(buf);
      } else if (strcasecmp(keyword, "CONTINUE_ON_MONITOR_ERROR") == 0) {
        kw_set_continue_on_monitor_error(buf, err_msg, 0);
      } else if(strcasecmp(keyword, "RESET_TEST_START_TIME_STAMP") == 0) {
        kw_set_reset_test_start_time_stamp(buf);
      } else if (strcasecmp(keyword, "NJVM_STD_ARGS") == 0) {
        kw_set_njvm_std_args(buf, err_msg, 0);
      } /*else if (strcasecmp(keyword, "USE_JAVA_OBJ_MGR") == 0) {
        kw_set_java_obj_mgr(buf);
      }*/ else if (strcasecmp(keyword, "NJVM_CUSTOM_ARGS") == 0) {
        kw_set_njvm_custom_args(buf, err_msg, 0);
      } else if (strcasecmp(keyword, "NJVM_JAVA_HOME") == 0) {
        kw_set_njvm_java_home_path(buf, err_msg, 0);
      } else if (strcasecmp(keyword, "NJVM_VUSER_THREAD_POOL") == 0) {
        kw_set_njvm_thread_pool(buf, err_msg, 0);
      } else if (strcasecmp(keyword, "NJVM_CONN_TIMEOUT") == 0) {
        kw_set_njvm_conn_timeout(buf, &(global_settings->njvm_settings.njvm_conn_timeout), err_msg, 0);
      } else if (strcasecmp(keyword, "NJVM_MSG_TIMEOUT") == 0) {
        kw_set_njvm_msg_timeout(buf, &(global_settings->njvm_settings.njvm_msg_timeout), err_msg, 0);
      } else if (strcasecmp(keyword, "NJVM_SIMULATOR_MODE") == 0) {
        kw_set_njvm_simulator_mode(buf);
      } else if (strcasecmp(keyword, "NJVM_CONN_TYPE") == 0) {
        kw_set_njvm_con_mode(buf, &(global_settings->njvm_settings.njvm_con_type));
      } else if (strcasecmp(keyword, "JRMI_CALL_TIMEOUT") == 0) {
        kw_set_jrmicall_timeout(buf, &(global_settings->jrmi_call_timeout), &(global_settings->jrmi_port));
      } else if (strcasecmp(keyword, "RBU_POST_PROC") == 0) {
        kw_set_rbu_post_proc_parameter(buf, err_msg, 0);
      } else if (strcasecmp(keyword, "RBU_SETTINGS") == 0) {
          if(kw_set_rbu_settings_parameter(buf, NULL, err_msg, 0) == 1){
            NSDL3_SCHEDULE(NULL, NULL, "RBU_SETTINGS is disabled");
          }	
          else
          {
            exit (-1);
          }
      } else if (strcasecmp(keyword, "RBU_ENABLE_CSV") == 0) {
        kw_set_rbu_enable_csv(buf, err_msg, 0);
      } else if (strcasecmp(keyword, "RBU_ALERT_POLICY") == 0) {
          if(kw_set_rbu_alert_policy(buf, err_msg, 0) == -1)
            exit(-1); 
      } else if (strcasecmp(keyword, "RBU_BROWSER_COM_SETTINGS") == 0) {
        kw_set_rbu_browser_com_settings(buf, err_msg, 0);
      } else if (strcasecmp(keyword, "USER_PROFILE_SELECTION_MODE") == 0) {
        kw_set_ser_prof_selc_mode(buf);
      } else if (strcasecmp(keyword, "NDE_DB_PARTITION_OVERLAP_MINS") == 0) {
        kw_check_partition_overtime(buf);
      } else if (strcasecmp(keyword, "RUNTIME_QTY_PHASE_SETTINGS") == 0) {
        kw_set_runtime_phases(buf, err_msg, 0);
      } else if (strcasecmp(keyword, "RUNTIME_CHANGE_TIMEOUT") == 0) {
        kw_set_runtime_qty_timeout(buf, err_msg);
      } else if (strcasecmp(keyword, "NVM_SCRATCH_BUF_SIZE") == 0) {
	global_settings->nvm_scratch_buf_size = atoi(text);
        if(global_settings->nvm_scratch_buf_size > MAX_TRACING_SCRATCH_BUFFER_SIZE) {
          //NSTL1_OUT(NULL, NULL,  "Warning: NVM_SCRATCH_BUF_SIZE keyword, scratch buf size is more than max value %d(1 MB), setting max value\n", MAX_TRACING_SCRATCH_BUFFER_SIZE);
          global_settings->nvm_scratch_buf_size = MAX_TRACING_SCRATCH_BUFFER_SIZE;
        }
      }else if (strcasecmp(keyword,"ENABLE_HEROKU_MONITOR") == 0) {
       kw_enable_heroku_monitor(keyword,buf, err_msg);
      }else if(strcmp(keyword, "RBU_ENABLE_TTI_MATRIX") == 0){
        if(kw_set_tti(buf, NULL, err_msg, 0) == 1){
          NSDL2_SCHEDULE(NULL, NULL, "RBU_ENABLE_TTI_MATRIX is disabled"); 
        }
        else
        {
          exit (-1);
        }
      }else if (strcasecmp(keyword, "RBU_EXT_TRACING") == 0) {
        if (kw_set_rbu_ext_tracing(buf, err_msg) != 0)
        {
          NS_EXIT(-1, "%s", err_msg);
        }     
      }else if(strcmp(keyword, "MAX_RTG_SIZE") == 0){
         kw_set_rtg_size(buf);
      }else if(strcmp(keyword, "ENABLE_MONITOR_DATA_LOG") == 0){
         kw_set_nd_monitor_log(buf, err_msg);
      }else if(strcmp(keyword, "ENABLE_OUTBOUND_CONNECTION") == 0){
         kw_set_outbound_connection(buf);
      }else if(strcmp(keyword, "CHECK_CONTINUOUS_MON_HEALTH") == 0) {
         kw_set_continous_monitoring_check_demon(buf);
      }else if(strcmp(keyword, "PROGRESS_REPORT_QUEUE") == 0) {
         kw_set_progress_report_queue(buf);
      } else if (strcasecmp(keyword, "RBU_DOMAIN_STAT") == 0) {
          kw_set_rbu_domain_stats(buf, err_msg, 0);
      } else if (strcasecmp(keyword, "RBU_MARK_MEASURE_MATRIX") == 0) {
          kw_set_rbu_mark_measure_matrix(buf, err_msg, 0);
      } else if (strcasecmp(keyword, "ENABLE_ALERT") == 0) {
         kw_set_enable_alert(buf, err_msg, 0);
      } else if (strcasecmp(keyword, "ENABLE_MEMORY_MAP") == 0) {
         kw_set_enable_memory_map(buf);
      }else if (strcasecmp(keyword, "ENABLE_PS_MON_ON_GENERATORS") == 0) {
         kw_set_enable_generator_process_monitoring(buf);
      }else if (strcasecmp(keyword,"ENABLE_TSDB_CONFIGURATION")==0){
         kw_set_tsdb_configuration(buf);
      }else if (strcasecmp(keyword, "NS_WRITE_RTG_DATA_IN_DB_OR_CSV") == 0) {
         kw_set_write_rtg_data_in_db_or_csv(buf);
      }else if (strcasecmp(keyword, "NS_WRITE_RTG_DATA_IN_INFLUX_DB") == 0) {
         kw_set_write_rtg_data_in_influx_db(buf);
      }else if (strcasecmp(keyword, "DEBUG_TEST_SETTINGS") == 0) {
         kw_set_debug_test_settings(buf);
    }//End while loop
  }
 }
}

int run_and_get_cmd_output(char *cmd, int out_len , char *output)
{ 
  char tmp[4096 + 1];
  int amt_written =0 ;
  FILE *app = NULL;
  int status;

  app = popen(cmd, "r");
  if (app == NULL)
  { 
    fprintf(stderr, "Error in executing command [%s]. Error = %s\n", cmd , nslb_strerror(errno));
    return -1;
  }

  while (!feof(app)) {
    status = fread(tmp, 1, 4096,  app);
    if (status <= 0){ //TODO: error handing of fread
      pclose(app); //close only does not give error if no output comes
      return 0;
    }
    tmp[status] = '\0';
    amt_written += snprintf(output + amt_written , out_len - amt_written, "%s", tmp);
  }

  if(WEXITSTATUS(pclose(app)))
    return -1;

  return 0;
}

int sort_conf_file(char *conf_file, char *new_conf_file, char *err_msg)
{
  char cmd[2048];
  char addKW[1024]="";

  if ((run_mode_option & RUN_MODE_OPTION_COMPILE) ||
       ((loader_opcode == MASTER_LOADER) && (ni_make_tar_option & CREATE_TAR_AND_EXIT))) 
  {
    sprintf(new_conf_file, "%s/.tmp/ns-inst/sorted_%s",
                         g_ns_wdir, basename(conf_file));
    sprintf(cmd, "mkdir -p %s/.tmp/ns-inst;%s/bin/nsi_merge_sort_scen 1 %s %s",
                     g_ns_wdir, g_ns_wdir, conf_file, new_conf_file);
  } 
  else 
  {
    if(g_set_args_type >= KW_ARGS)
       sprintf(addKW, "%s/logs/TR%d/additional_kw.conf",g_ns_wdir, testidx);
    sprintf(new_conf_file, "%s/logs/TR%d/sorted_scenario.conf", g_ns_wdir, testidx);
    sprintf(cmd, "%s/bin/nsi_merge_sort_scen 0 %s %s %s", g_ns_wdir, conf_file, new_conf_file, addKW);
  }

  NSDL1_PARENT(NULL, NULL,"Executing command %s", cmd);

  if(run_and_get_cmd_output(cmd, 4096, err_msg) == -1)
  {
    //NSTL1_OUT(NULL, NULL, "Failed to run command %s, error: %s", cmd, err_msg);
    NS_EXIT(-1, CAV_ERR_1011271, conf_file, err_msg);
  }

  return 0;
}

//set the default values of keywords befaure parsing any file
void init_default_values()
{
  FILE* process_ptr;
  char line_buf[MAX_LINE_LENGTH] = "\0";

  NSDL2_SCHEDULE(NULL, NULL, "Method called");

  process_ptr = fopen("/proc/cpuinfo", "r");
  if (process_ptr) {
    while (fgets(line_buf, MAX_LINE_LENGTH, process_ptr)) {
	if (strncmp(line_buf, "processor", strlen("processor")) == 0)
  	    num_processor++;
    }
    CLOSE_FP(process_ptr);
  }
  if (num_processor <= 0) {
	NSTL1_OUT(NULL, NULL, "Unable to determine number of processors. Assuming 1 processor\n");
	NS_DUMP_WARNING("Unable to determine number of processors. Hence, Setting it's value to 1 processor");
	num_processor = 1;
  }

  global_settings->num_process = DEFAULT_PROC_PER_CPU * num_processor;
  //global_settings->idle_secs = DEFAULT_IDLE_SECS;
  //global_settings->url_idle_secs = 1;
  global_settings->ssl_pct = 0;
  set_ssl_default();  //set ssl global value as default
  //global_settings->wait_end_secs = 180;
  //global_settings->wait_progress_secs = 5000;
  global_settings->progress_secs = 10000;
  global_settings->cap_consec_samples = DEFAULT_CAP_CONSEC_SAMPLES;
  global_settings->cap_pct = DEFAULT_CAP_PCT;
  //global_settings->log_level = 1;
  //global_settings->log_dest = 0;
  //global_settings->trace_level = 1;
  //global_settings->trace_dest = 0;
  //global_settings->trace_start = 1; //not used : account_all takes care of it
  //global_settings->trace_on_fail = 1;
  global_settings->log_postprocessing = 1;
  group_default_settings->max_log_space = 1000000000;
  //global_settings->report_level = 1;
  //global_settings->ka_pct = 70;
  //global_settings->num_ka_min = 5;
  //global_settings->num_ka_range = 10;
  //global_settings->errcode_ok = 1;
  global_settings->loss_factor = 1;
  global_settings->lat_factor = 1;
  global_settings->interactive = 0;
  global_settings->smon = 1;
  //global_settings->account_all = 1;
  //global_settings->ramp_down_mode = 0;
  //global_settings->ramp_up_mode = RAMP_UP_MODE_LINEAR;  //By arun as 2 sec step is removed
  //global_settings->report_mask = (URL_REPORT | PAGE_REPORT | TX_REPORT | SESS_REPORT | VUSER_REPORT);

  global_settings->conn_retry_time = 250;
  //global_settings->use_pct_prof = 0;
  global_settings->nvm_distribution = 0;
  global_settings->use_sess_prof = 0;
  global_settings->warning_threshold = 4000;
  global_settings->alert_threshold = 8000;
  //global_settings->vuser_rpm = 120;       //default value added by Arun Aug 28 2008  
  
  //Adding the default value for proxy flag
  global_settings->proxy_flag = -1;
  global_settings->max_sock = 10000;
  global_settings->high_perf_mode = 0; //Default is 0. To enable, set HIGH_PERF_MODE environment variable to 1
  //strcpy(global_settings->eth_interface, "eth0");
  //global_settings->eth_interface[0] = '\0';

  testCase.mode = TC_FIX_CONCURRENT_USERS;
  testCase.guess_type = GUESS_RATE;
  testCase.guess_num = DEFAULT_GUESS_NUM;
  testCase.guess_prob = NS_GUESS_PROB_LOW;
  testCase.min_steps  = DEFAULT_GUESS_MIN_STEPS;
  testCase.stab_num_success = DEFAULT_STAB_NUM_SUCCESS;
  testCase.stab_max_run = DEFAULT_STAB_MAX_RUN;
  testCase.stab_run_time = DEFAULT_STAB_RUN_TIME;
  testCase.stab_goal_pct = DEFAULT_STAB_GOAL_PCT;
  testCase.target_rate  = 120;  //default value added by Arun Aug 28 2008  

  //We are doing this malloc as we can override this by keyword
  MY_MALLOC_AND_MEMSET(global_settings->time_ptr, sizeof(timer_type), "timer ptr", -1);
  global_settings->time_ptr->timer_type = -1;
  global_settings->browser_used = -1; //default value added by Atul Sh. May 9 2015
  global_settings->create_screen_shot_dir = -1; //default value added by Atul Sh. 26 june 2015
  //global_settings->rbu_settings = -1; //default value added by Atul Sh. 9 january 2016
  global_settings->rbu_enable_csv = -1; //default value added by Atul Sh. 29 january 2016
  global_settings->rbu_com_setting_mode = -1; //default value added by Atul Sh. 1 february 2016
  global_settings->rbu_com_setting_max_retry = -1; //default value added by Atul Sh. 1 february 2016
  global_settings->rbu_com_setting_interval = 0; //default value added by Atul Sh. 1 february 2016

  // Initialising uri encoding array for 255 characters
  memset(&(global_settings->encode_uri), 0, 256);
  memset(&(global_settings->encode_query), 0, 256);
}


int kw_set_max_url_retries(char *buf, int *to_change, short *retry_on_timeout, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char grp[MAX_DATA_LINE_LENGTH];
  int max_url_retries;
  int retry_type = 0;

  NSDL1_PARENT(NULL, NULL, "Method Called, buf = [%s]", buf);

  sscanf(buf, "%s %s %d %d", keyword, grp, &max_url_retries, &retry_type);
 
  NSDL2_PARENT(NULL, NULL, "Group = [%s], max_url_retries = [%d], retry_type = [%d]", grp, max_url_retries, retry_type);

  if (max_url_retries < 0 || max_url_retries > 255) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_MAX_URL_RETRIES_USAGE, CAV_ERR_1011103, "");
  }

  *to_change = max_url_retries;

  if (retry_type < 0 || retry_type > 2) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_MAX_URL_RETRIES_USAGE, CAV_ERR_1011102, CAV_ERR_MSG_3);
  }
  
  *retry_on_timeout = retry_type;

  return 0;
}

/*------------------------------------------------------------------------------------------------------------------------
 * Purpose   : This function will parse keyword G_SESSION_RETRY <group> <num_retrires> <retry_interval> 
 *
 * Input     : buf   : G_SESSION_RETRY <group> <num_retrires> <retry_interval> 
 *                     Group_name        - ALL (Default)
 *                     num_retries       - number of retries"
                                           0       - disabled(Default)"
                                           1,2,... - enabled"
 *                     retry_interval    - time interval (milliseconds)                   
 *                     Eg: G_SESSION_RETRY ALL 3 2000 
 *
 * Output    : On error    -1
 *             On success   0
 *-----------------------------------------------------------------------------------------------------------------------*/
int kw_set_g_session_retry(char *buf, GroupSettings *gset, char *err_msg, int runtime_changes)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char group_name[MAX_DATA_LINE_LENGTH] = "ALL";
  char num_retries[MAX_DATA_LINE_LENGTH] = "0";  
  char retry_interval[MAX_DATA_LINE_LENGTH] = "0";
  char temp[MAX_DATA_LINE_LENGTH] = {0};
  int inum_retries = 0;
  int iretry_interval = 0;
  int num_args = 0;
 
  NSDL2_PARSING(NULL, NULL, "Method Called, buf = [%s], gset = [%p], runtime_changes = %d", buf, gset, runtime_changes);

  num_args = sscanf(buf, "%s %s %s %s %s", keyword, group_name, num_retries, retry_interval, temp);

  if(num_args < 3 || num_args > 4)
  {
    NSDL2_PARSING(NULL, NULL, "Error: provided number of argument (%d) is wrong.\n%s", num_args - 1, G_SESSION_RETRY_USAGE);
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_SESSION_RETRY_USAGE, CAV_ERR_1011088, CAV_ERR_MSG_1);
  }

  NSDL2_PARSING(NULL, NULL, "keyword = [%s], group_name = [%s], num_retries = [%s], retry_interval = [%s]",
                keyword, group_name, num_retries, retry_interval);

  /* Validate group Name */
  val_sgrp_name(buf, group_name, 0);

  if(ns_is_numeric(num_retries))
  {
    inum_retries = atoi(num_retries);
  }
  else
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_SESSION_RETRY_USAGE, CAV_ERR_1011088, CAV_ERR_MSG_2);
  }

  if(ns_is_numeric(retry_interval))
  {
    iretry_interval = atoi(retry_interval);
  }
  else
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_SESSION_RETRY_USAGE, CAV_ERR_1011088, CAV_ERR_MSG_2);
  }
 
  gset->num_retry_on_page_failure = inum_retries;
  gset->retry_interval_on_page_failure = iretry_interval;

  NSDL2_PARSING( NULL, NULL, "On end - gset->num_retry_on_page_failure = %d, gset->retry_interval_on_page_failure = %d", 
                 gset->num_retry_on_page_failure, gset->retry_interval_on_page_failure);
  return 0;
}

int kw_set_get_no_inlined_obj(char *buf, short *to_change, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char grp[MAX_DATA_LINE_LENGTH];
  int num_value, num_args;

  num_args = sscanf(buf, "%s %s %d", keyword, grp, &num_value);

  if(num_args != 3) {
    NSTL1_OUT(NULL, NULL,  "Two arguments expected for %s\n", keyword);
    sprintf(err_msg, "Two arguments expected for %s\n", keyword);
    return 1;
  }

  if (num_value < 0) {
    NSTL1_OUT(NULL, NULL,  "Keyword (%s) value must be >= 0.\n", keyword);
    sprintf(err_msg, "Keyword (%s) value must be >= 0.\n", keyword);
    return 1;
  }

  *to_change = num_value;
  return 0;
}

/*PARENT_EPOLL <Timeout in ms> <Max Count> <Dump Parent  0 | 1>*/
/* Parent epoll default value is dependent on progress report interval so we are doing this in init_default_parent_epoll*/
static void kw_set_parent_epoll(char *buf) {
  int num;
  char keyword[MAX_LINE_LENGTH]="\0";
  int field1 = 0;
  int field2 = 10;
  int field3 = 1;

  NSDL1_PARENT(NULL, NULL, "Method Called, buf = %s", buf);
        
  num = sscanf(buf, "%s %d %d %d", keyword, &field1, &field2, &field3);

  if(num < 2) {
    NS_EXIT(-1, "%s needs at least one arguments", keyword);
  }

  if(field1 <= 0) {
    NS_EXIT(-1, "%s: Parent epoll timeout must be greater than 0", keyword);
  }

  if(num == 2) {
    field2 = (5*60*1000) / field1;
  }

  if(field2 < 0 || field2 > 126) {
    NS_EXIT(-1, "%s: Parent time out max count must be in range (0, 127)", keyword);
  }

  global_settings->parent_epoll_timeout   = field1;
  global_settings->parent_timeout_max_cnt = field2;
  global_settings->dump_parent_on_timeout = field3;
}         

/* we do not need to set explicitly NVM epoll timeout beacuse it will be handled by Keywordefinition.dat*/
/*NVM_EPOLL <Timeout in ms> <Max Count> <Skip Timer calls 0 | 1> <wait for write>*/
static void kw_set_nvm_epoll(char *buf) {

  int num;
  char keyword[MAX_LINE_LENGTH]="\0";
  int field1 = 0;
  int field2 = 10;
  int field3 = 1;
  int field4 = 600;

  NSDL1_PARENT(NULL, NULL, "Method Called, buf = %s", buf);

  num = sscanf(buf, "%s %d %d %d %d", keyword, &field1, &field2, &field3, &field4);

  if(num < 2) {
    NS_EXIT(-1, "%s needs at least one arguments", keyword);
  }

  global_settings->nvm_epoll_timeout = field1;
  global_settings->nvm_timeout_max_cnt = field2;
  global_settings->skip_nvm_timer_call = field3;
  global_settings->wait_for_write = field4;

  if(global_settings->nvm_epoll_timeout <= 0) {
    NS_EXIT(-1, "%s: NVM epoll timeout must be greater than 0", keyword);
  }

  if(global_settings->nvm_timeout_max_cnt < 0) {
    NS_EXIT(-1, "%s: NVM time out max count can not be negative", keyword);
  }

  if(global_settings->wait_for_write < 0) {
    NS_EXIT(-1, "%s: NVM wait for write can not be negative", keyword);
  }
}

int kw_set_no_validation(char *buf, short *to_change, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char grp[MAX_DATA_LINE_LENGTH];
  int num_value;

  sscanf(buf, "%s %s %d", keyword, grp, &num_value);
  
  *to_change = num_value;
  return 0;
}

int kw_set_disable_reuseaddr(char *buf, short *to_change, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char grp[MAX_DATA_LINE_LENGTH];
  int num_value;

  sscanf(buf, "%s %s %d", keyword, grp, &num_value);
  
  *to_change = num_value;
  return 0;
}

int kw_set_no_http_compression(char *buf, unsigned int *to_change, char *err_msg)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char grp[MAX_DATA_LINE_LENGTH];
  int num_value = 0;

  sscanf(buf, "%s %s %d", keyword, grp, &num_value);
  
  if (num_value)
    *to_change |= NS_ACCEPT_ENC_HEADER;
  else if ((*to_change & NS_ACCEPT_ENC_HEADER) == NS_ACCEPT_ENC_HEADER)
    *to_change ^= NS_ACCEPT_ENC_HEADER;

  return 0;
}

int kw_set_disable_host_header(char *buf, unsigned int *to_change, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char grp[MAX_DATA_LINE_LENGTH];
  int num_value;

  sscanf(buf, "%s %s %d", keyword, grp, &num_value);
  
  if (num_value)
    *to_change |= NS_HOST_HEADER;
  else if ((*to_change & NS_HOST_HEADER) == NS_HOST_HEADER)
    *to_change ^= NS_HOST_HEADER;

  return 0;
}

int kw_set_disable_ua_header(char *buf, unsigned int *to_change, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char grp[MAX_DATA_LINE_LENGTH];
  int num_value;

  sscanf(buf, "%s %s %d", keyword, grp, &num_value);
  
  if (num_value)
    *to_change |= NS_UA_HEADER;
  else if ((*to_change & NS_UA_HEADER) == NS_UA_HEADER)
    *to_change ^= NS_UA_HEADER;

  return 0;
}

int kw_set_disable_accept_header(char *buf, unsigned int *to_change, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char grp[MAX_DATA_LINE_LENGTH];
  int num_value;

  sscanf(buf, "%s %s %d", keyword, grp, &num_value);
  
  if (num_value)
    *to_change |= NS_ACCEPT_HEADER;
  else if ((*to_change & NS_ACCEPT_HEADER) == NS_ACCEPT_HEADER)
    *to_change ^= NS_ACCEPT_HEADER;

  return 0;
}

int kw_set_disable_accept_enc_header(char *buf, unsigned int *to_change, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char grp[MAX_DATA_LINE_LENGTH];
  int num_value;

  sscanf(buf, "%s %s %d", keyword, grp, &num_value);
  
  if (num_value)
    *to_change |= NS_ACCEPT_ENC_HEADER;
  else if ((*to_change & NS_ACCEPT_ENC_HEADER) == NS_ACCEPT_ENC_HEADER)
    *to_change ^= NS_ACCEPT_ENC_HEADER;

  return 0;
}

int kw_set_disable_ka_header(char *buf, unsigned int *to_change, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char grp[MAX_DATA_LINE_LENGTH];
  int num_value;

  sscanf(buf, "%s %s %d", keyword, grp, &num_value);
  
  if (num_value)
    *to_change |= NS_KA_HEADER;
  else if ((*to_change & NS_KA_HEADER) == NS_KA_HEADER)
    *to_change ^= NS_KA_HEADER;
  
  return 0;
}

int kw_set_disable_connection_header(char *buf, unsigned int *to_change, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char grp[MAX_DATA_LINE_LENGTH];
  int num_value;

  sscanf(buf, "%s %s %d", keyword, grp, &num_value);
  
  if (num_value)
    *to_change  |= NS_CONNECTION_HEADER;
  else if ((*to_change & NS_CONNECTION_HEADER) == NS_CONNECTION_HEADER)
    *to_change  ^= NS_CONNECTION_HEADER;

  return 0;
}

int kw_set_disable_all_header(char *buf, unsigned int *to_change, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char grp[MAX_DATA_LINE_LENGTH];
  int num_value;

  sscanf(buf, "%s %s %d", keyword, grp, &num_value);
  
  if (num_value)
    *to_change  = NS_NO_HEADER;
  else if ((*to_change & NS_NO_HEADER) == NS_NO_HEADER)
    *to_change ^= NS_NO_HEADER;

  return 0;
}

int kw_set_use_recorded_host_in_host_hdr(char *buf, int *to_change, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char grp[MAX_DATA_LINE_LENGTH];
  int num_value;

  sscanf(buf, "%s %s %d", keyword, grp, &num_value);
  
  *to_change = num_value;
  
  return 0;
}

int kw_set_auto_redirect(char *buf, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  int num;

  if ((num = sscanf(buf, "%s %d %d", keyword, &global_settings->g_follow_redirects, 
                    &global_settings->g_auto_redirect_use_parent_method)) < 2) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, AUTO_REDIRECT_USAGE, CAV_ERR_1011025, CAV_ERR_MSG_1);
  }
  return 0;
}

// ***************************************************************

// ENABLE_TRANSACTION_CUMULATIVE_GRAPHS <option>

int kw_set_tx_cumulative_graph(char *buf, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char kw_val[MAX_DATA_LINE_LENGTH];
  char chk_field[MAX_DATA_LINE_LENGTH];
  int num = 0;

  num = sscanf(buf, "%s %s %s", keyword, kw_val, chk_field);
  if(num > 2 || num < 2) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, ENABLE_TRANSACTION_CUMULATIVE_GRAPHS_USAGE, CAV_ERR_1011220, CAV_ERR_MSG_1);
  }

  if(ns_is_numeric(kw_val) == 0) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, ENABLE_TRANSACTION_CUMULATIVE_GRAPHS_USAGE, CAV_ERR_1011220, CAV_ERR_MSG_2);
  }

  global_settings->g_tx_cumulative_graph = atoi(kw_val);
  if(global_settings->g_tx_cumulative_graph < 0 || global_settings->g_tx_cumulative_graph > 1) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, ENABLE_TRANSACTION_CUMULATIVE_GRAPHS_USAGE, CAV_ERR_1011220, CAV_ERR_MSG_3);
  }
  return 0;
}

static char runlogic_all_buf[MAX_DATA_LINE_LENGTH];
void kw_set_g_script_runlogic(char *buf, char *err_msg, int runtime_flag)
{
  int num;
  char keyword[MAX_DATA_LINE_LENGTH];
  char sg_name[MAX_DATA_LINE_LENGTH];
  int grp_idx;
  char temp[MAX_DATA_LINE_LENGTH]; //This used to check if some extra field is given
  char runlogic_name[MAX_DATA_LINE_LENGTH];

  num = sscanf(buf, "%s %s %s %s", keyword, sg_name, runlogic_name, temp);
  if(num < 3)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, CAV_ERR_1011109, CAV_ERR_MSG_1);
  }

  if (strcasecmp(sg_name, "ALL") == 0)
  {
    if(strlen(runlogic_name) > 255)
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, CAV_ERR_1011109, CAV_ERR_MSG_1);

    strcpy(runlogic_all_buf, runlogic_name);
  }
  else
  {
    grp_idx = find_sg_idx(sg_name);
    if(grp_idx < 0)
    {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, CAV_ERR_1011109, CAV_ERR_MSG_1);
    }

    if(strlen(runlogic_name) > 255)
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, CAV_ERR_1011109, CAV_ERR_MSG_1);

    strcpy(runProfTable[grp_idx].runlogic, runlogic_name);
  }
}


void kw_set_g_datadir(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];  //size=2048
  char grp[MAX_DATA_LINE_LENGTH];
  int mode;
  char kw_mode[MAX_DATA_LINE_LENGTH];
  char temp[MAX_DATA_LINE_LENGTH];
  int num;
  char data_dir[MAX_UNIX_FILE_NAME + 1]; //size=256

  num = sscanf(buf, "%s %s %s %s %s", keyword, grp, kw_mode, data_dir, temp);
  if(num>4) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_DATADIR_USAGE, CAV_ERR_1060085, CAV_ERR_MSG_1);
  }
  if(nslb_atoi(kw_mode, &mode) < 0) {  
  NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_DATADIR_USAGE, CAV_ERR_1060085, CAV_ERR_MSG_12);
  }
  if((mode < 0) || (mode > 1)) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_DATADIR_USAGE, CAV_ERR_1060085, CAV_ERR_MSG_12);
  }
  if(mode == USE_SPECIFIED_DATA_DIR) {
    if(validate_and_copy_datadir(0, data_dir, gset->data_dir, err_msg) < 0)
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_DATADIR_USAGE, CAV_ERR_1060085, err_msg);
  }
  else
  {
    gset->data_dir[0] = '\0';
  }

  NSDL2_MISC(NULL, NULL, "gset->data_dir = %s", gset->data_dir);
}
//**************************************************************************************

/* void kw_set_avg_ssl_reuse(char *buf, int *to_change) */
/* { */
/*   char keyword[MAX_DATA_LINE_LENGTH]; */
/*   char grp[MAX_DATA_LINE_LENGTH]; */
/*   int num_value; */

/*   sscanf(buf, "%s %s %d", keyword, grp, &num_value); */
  
/*   *to_change = num_value; */

/* } */

/* void kw_set_ssl_ciphers(char *buf, int *to_change) */
/* { */
/*   char keyword[MAX_DATA_LINE_LENGTH]; */
/*   char grp[MAX_DATA_LINE_LENGTH]; */
/*   int num_value; */

/*   sscanf(buf, "%s %s %d", keyword, grp, &num_value); */
  
/*   *to_change = num_value; */

/* } */

/* void kw_set_ssl_cert_file_path(char *buf, int *to_change) */
/* { */
/*   char keyword[MAX_DATA_LINE_LENGTH]; */
/*   char grp[MAX_DATA_LINE_LENGTH]; */
/*   int num_value; */

/*   sscanf(buf, "%s %s %d", keyword, grp, &num_value); */
  
/*   *to_change = num_value; */

/* } */

/* void kw_set_ssl_key_file_path(char *buf, int *to_change) */
/* { */
/*   char keyword[MAX_DATA_LINE_LENGTH]; */
/*   char grp[MAX_DATA_LINE_LENGTH]; */
/*   int num_value; */

/*   sscanf(buf, "%s %s %d", keyword, grp, &num_value); */
  
/*   *to_change = num_value; */

/* } */

/* void kw_set_ssl_clean_close_only(char *buf, int *to_change) */
/* { */
/*   char keyword[MAX_DATA_LINE_LENGTH]; */
/*   char grp[MAX_DATA_LINE_LENGTH]; */
/*   int num_value; */

/*   sscanf(buf, "%s %s %d", keyword, grp, &num_value); */
  
/*   *to_change = num_value; */

/* } */


/* OK below */
int kw_set_on_eurl_err(char *buf, short *to_change, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char grp[MAX_DATA_LINE_LENGTH];
  int num_value;

  sscanf(buf, "%s %s %d", keyword, grp, &num_value);
  
  if ((num_value != 0) && (num_value != 1)) {
    num_value = 0;
    NSDL2_MISC(NULL, NULL, "user_data_check(): forced global_settings->on_eurl_err to 0");
  }

  *to_change = num_value;
  
  return 0;
}

int kw_set_err_code_ok(char *buf, short *to_change, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char grp[MAX_DATA_LINE_LENGTH];
  int num_value;

  sscanf(buf, "%s %s %d", keyword, grp, &num_value);
  
  *to_change = num_value;

  return 0;
}

int kw_set_logging (char *buf, short *log_level_new, short *log_dest_new, char *err_msg)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char grp[MAX_DATA_LINE_LENGTH];

  int log_level, log_dest;
  int num;

    num = sscanf(buf, "%s %s %d %d", keyword, grp, &log_level, &log_dest);
    if (num != 4) {
      printf("wrong format, using default: LOGGING Keyword format is: LOGGING log-level log-dest\n");
    } else {
      if ((log_level < 0) || (log_level >3)) {
        printf ("read_keywords(): LOGGING : %d is not  valid log_level, valid values are 0-3, using default 1\n", log_level);
        log_level = 1;
      }
      if ((log_dest < 0) || (log_dest >2)) {
        printf ("read_keywords(): LOGGING : %d is not  valid log_dest, valid values are 0-2, using default 0\n", log_dest);
        log_dest = 0;
      }
      
      
      *log_level_new = log_level;
      *log_dest_new  = log_dest;

/*       if (pass == 1) { */
/*         group_default_settings->log_level = log_level; */
/*         group_default_settings->log_dest = log_dest; */
/*       }  */
/*       if (pass == 2) { */
/*         runProfTable[grp_idx].gset.log_level = log_level; */
/*         runProfTable[grp_idx].gset.log_dest = log_dest; */
/*       } */
    }

    return 0;
}

int kw_set_reporting (char *buf, short *to_change, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char grp[MAX_DATA_LINE_LENGTH];
  int num;
  int log_level;
  int percentage = 1;
  //Added for validation check
  char report_level[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH];
  tmp[0] = 0;
  report_level[0] = 1; //Default value is 1

  if ((num = sscanf(buf, "%s %s %s %s", keyword, grp, report_level, tmp)) < 3) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_REPORTING_USAGE, CAV_ERR_1011019, CAV_ERR_MSG_1);
  } 
   
  log_level = atoi(report_level);
  if(tmp[0]) {
    percentage = atoi(tmp);
    if(nslb_atoi(tmp, &percentage) < 0) {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_REPORTING_USAGE, CAV_ERR_1011019, CAV_ERR_MSG_1);
    }
  }

  // Reporting Value cannot be less than 0
  if (log_level < 0) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_REPORTING_USAGE, CAV_ERR_1011019, CAV_ERR_MSG_8);
  }

  if (ns_is_numeric(report_level) == 0) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_REPORTING_USAGE, CAV_ERR_1011019, CAV_ERR_MSG_2);
  }
 
  //Removing 0 level  
  //Here report level 2 and 3 are same, therefore removing level 3
  if (!log_level || log_level > 2) {  
    NS_DUMP_WARNING("G_REPORTING : %d is not a valid report_level, setting to its default 2", log_level);
    log_level = 2;
  }
      
  if (percentage < 0 || percentage > 10)
  {
     NS_DUMP_WARNING("Drill down session percentage (%d) is not between range 1 to 10, setting to its default 1", percentage);
     percentage = 1;
  }
  *to_change = log_level;
  //set DDR percentage value
  group_default_settings->ddr_session_pct = (char)percentage;
  return 0;
}

/* void kw_set_modulemask (char *buf, int *to_change) */
/* { */
/*   char keyword[MAX_DATA_LINE_LENGTH]; */
/*   char grp[MAX_DATA_LINE_LENGTH]; */
/*   int num_value; */

/*   sscanf(buf, "%s %s %d", keyword, grp, &num_value); */
  
/*   *to_change = num_value; */

/* } */

/* void kw_set_debug (char *buf, int *to_change) */
/* { */
/*   char keyword[MAX_DATA_LINE_LENGTH]; */
/*   char grp[MAX_DATA_LINE_LENGTH]; */
/*   int num_value; */

/*   sscanf(buf, "%s %s %d", keyword, grp, &num_value); */
  
/*   *to_change = num_value; */

/* } */

/**********************************************************************************************************************************
 * Description        : kw_set_exclude_failed_aggregates() method used to exclude failed and stopped stats.
                        First arg is used to exclude response time of failed urls from page, tx and session.
                        Second arg is used to exclude url, page and tx form hits, response time, drill down, page dump and tracing
 * Format             : EXCLUDE_FAILED_AGGREGATES <enable/disable> <enable/disable>
 * Input Parameters
 *           buf      : Providing entire buffer(including keyword).
 * Output Parameters  : Set exclude_failed_agg and exclude_stopped_stats in struct GlobalData
 **********************************************************************************************************************************/

void kw_set_exclude_failed_aggregates(char *buf, short *to_change)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  int mode = 0; // for including or excluding stopped stats
  int num_value = 0; 
  int num; // for total number of values

  NSDL2_PARSING(NULL, NULL, "Method called, buf = %s", buf);

  num = sscanf(buf, "%s %d %d", keyword, &num_value, &mode);

  if (num != 3) {
    NSTL1_OUT(NULL, NULL,  "Warning: Invalid number of arguments for the keyword EXCLUDE_FAILED_AGGREGATES, hence using default values\n");
  }

  if (num_value < 0 || num_value > 1) {
    NSTL1_OUT(NULL, NULL,  "Warning: Mode specified in the keyword EXCLUDE_FAILED_AGGREGATES can have value 0 or 1. Hence setting its value to 0");
    num_value = 0;
  }

  if (mode < 0 || mode > 2) {
    NSTL1_OUT(NULL, NULL,  "Warning: Mode specified in the keyword EXCLUDE_FAILED_AGGREGATES can have value 0 or 1 or 2. Hence setting its value to 0");
    mode = 0;
  }

  //global_settings->exclude_failed_agg = num_value;
  //global_settings->exclude_stopped_stats = mode;

  //For exclude response time of failed urls from page, tx and session
  global_settings->exclude_failed_agg = 0;

  //For exclude tx from hits, response time, drill down, page dump and tracing in case of Use-once
  if (mode == 2)
    global_settings->exclude_stopped_stats = mode;
  else
    global_settings->exclude_stopped_stats = 0;
   
  NSDL3_PARSING(NULL, NULL," global_settings->exclude_failed_agg = %d, global_settings->exclude_stopped_stats = %d", global_settings->exclude_failed_agg, global_settings->exclude_stopped_stats);

  return;
}

int kw_set_url_idle_secs(char *buf, int *to_change, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char grp[MAX_DATA_LINE_LENGTH];
  int num_value;

  sscanf(buf, "%s %s %d", keyword, grp, &num_value);
  
  *to_change = num_value;

  return 0;
}

int
kw_set_idle_secs(char *buf, int *to_change, char *err_msg)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char grp[MAX_DATA_LINE_LENGTH];
  int num_value;
  sscanf(buf, "%s %s %d", keyword, grp, &num_value);
  
  if (num_value <= 0) {
    num_value = 1;
    NSDL2_MISC(NULL, NULL, "user_data_check(): forced global_settings->url_idle_secs to 1");
  }

  *to_change = (num_value * 1000);
  return 0;
}

int
kw_set_idle_msecs(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char grp[MAX_DATA_LINE_LENGTH];
  int num_value;
  int resp_timeout;
  int conn_timeout;

  sscanf(buf, "%s %s %d %d %d", keyword, grp, &num_value, &resp_timeout, &conn_timeout);
  
  //Checking for idle timer, should be greater than 0
  if (num_value <= 0) {
    num_value = 1000;           //For msecs conversion
    NSDL2_MISC(NULL, NULL, "user_data_check(): forced gset->idle_secs to 1");
  }

  //Setting idle milliseconds
  gset->idle_secs = num_value;

  //Checking for response timer, should be greater than 0
  if (resp_timeout <= 0){
    gset->response_timeout = MAX_INT_16BIT_THOUSAND;   //MAX of Unsigned int '65535000' for milliseconds
    NSDL2_MISC(NULL, NULL, "user_data_check(): forced  to gset->response_timeout to 65535000");
  }
  else
    gset->response_timeout = resp_timeout;

  //Set connect timeout
  if(conn_timeout <= 0)
  {
    gset->connect_timeout = 60000;
    NSDL2_MISC(NULL, NULL, "user_data_check(): forced to gset->connect_timeout to 60000");
  }
  else
    gset->connect_timeout = conn_timeout;

  //If response timeOut is smaller take equal value
  if( gset->response_timeout < gset->idle_secs)
    gset->response_timeout = gset->idle_secs;

  NSDL2_MISC(NULL, NULL, "idle_secs = %d, response_timeout = %d, connect_timeout = %d", 
             gset->idle_secs, gset->response_timeout, gset->connect_timeout);
  
  return 0;
}

int
kw_set_user_cleanup_msecs(char *buf, int *to_change, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char grp[MAX_DATA_LINE_LENGTH];
  int time_local;
  int num;

  if((num = sscanf(buf, "%s %s %d", keyword, grp, &time_local)) != 3) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_USER_CLEANUP_MSECS_USAGE, CAV_ERR_1011084, CAV_ERR_MSG_1);
  }
  
  if (time_local < 0) {
    time_local = 0;
    NSTL1(NULL, NULL, "G_USER_CLEANUP_MSECS time has to be positive, setting it to Zero\n");
  }

  *to_change = time_local;
  return 0;
}

int kw_set_ka_pct(char *buf, int *to_change, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char grp[MAX_DATA_LINE_LENGTH];
  int num;
  int num_value;

  num = sscanf(buf, "%s %s %d", keyword, grp, &num_value);
  if (num != 3) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_KA_PCT_USAGE, CAV_ERR_1011041, CAV_ERR_MSG_1);
  } else if ((num_value < 0 ) || (num_value > 100)) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_KA_PCT_USAGE, CAV_ERR_1011041, CAV_ERR_MSG_6);
  } else {
    *to_change = num_value;
  }

  return 0;
}

int kw_set_num_ka(char *buf, int *to_change_min, int *to_change_range, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char grp[MAX_DATA_LINE_LENGTH];
  int num;
  int num_value, max_value;

  num = sscanf(buf, "%s %s %d %d", keyword, grp, &num_value, &max_value);
  if (num == 4) {
    if ((num_value < 0) || (max_value < 0)) {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_NUM_KA_USAGE, CAV_ERR_1011042, CAV_ERR_MSG_8);
    } else if (max_value < num_value) {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_NUM_KA_USAGE, CAV_ERR_1011042, CAV_ERR_MSG_5);
    } else {
      if ((num_value == 0)  && (max_value == 0)) {
        *to_change_min = 999999999;
        *to_change_range = 0;
      } else {
        *to_change_min = num_value;
        *to_change_range = max_value - num_value;
      }
    }
  } else {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_NUM_KA_USAGE, CAV_ERR_1011042, CAV_ERR_MSG_1);
  }
  return 0;
}

//This Method will parse G_M3U8_SETTING Keyword and fill data structure
static int kw_set_g_m3u8_enable(char *buf, GroupSettings *gset, char *err_msg, int runtime_changes)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char group_name[512];
  char mode[512];
  char bandwidth[512];
  char tmp_str[RBU_MAX_KEYWORD_LENGTH];
  int num_args = 0;

  NSDL3_RBU(NULL, NULL, "Method called buf = %s", buf);

  num_args = sscanf(buf, "%s %s %s %s %s", keyword, group_name, mode, bandwidth, tmp_str);
  if(num_args != 4)
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_M3U8_SETTING_USAGE, CAV_ERR_1011028, CAV_ERR_MSG_1);
  }

  /* Validate group Name */
  val_sgrp_name(buf, group_name, 0);

  if(ns_is_numeric(mode) == 0)
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_M3U8_SETTING_USAGE, CAV_ERR_1011028, CAV_ERR_MSG_2);
  }

  if(match_pattern(mode, "^[0-1]$") == 0)
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_M3U8_SETTING_USAGE, CAV_ERR_1011028, CAV_ERR_MSG_3);
  }
  gset->m3u8_gsettings.enable_m3u8 = atoi(mode);
  
  if(ns_is_numeric(bandwidth) == 0)  //TODO do we have any constrains on Bandwidth
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_M3U8_SETTING_USAGE, CAV_ERR_1011028, CAV_ERR_MSG_2);
  }
  gset->m3u8_gsettings.bandwidth = atoi(bandwidth);
  
  NSDL1_PARSING(NULL, NULL, "After Parsing gset->m3u8_settings.enable_m3u8 = %d,gset->m3u8_settings.bandwidth = %d ", 
                                                       gset->m3u8_gsettings.enable_m3u8, gset->m3u8_gsettings.bandwidth);

  return 0;
}

static int kw_set_g_rte_settings(char *buf, GroupSettings *gset, char *err_msg, int runtime_changes)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char group_name[512];
  char mode[512];
  char proto[512];
  char gui_mode[512];
  char tmp_str[RBU_MAX_KEYWORD_LENGTH];
  int num_args = 0;
  int protocol, terminal;

  NSDL3_RBU(NULL, NULL, "Method called buf = %s", buf);

  num_args = sscanf(buf, "%s %s %s %s %s %s", keyword, group_name, mode, proto, gui_mode, tmp_str);
  if(num_args != 5)
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RTE_SETTINGS_USAGE, CAV_ERR_1011082, CAV_ERR_MSG_1);
  }

  /* Validate group Name */
  val_sgrp_name(buf, group_name, 0);

  if(ns_is_numeric(mode) == 0)
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RTE_SETTINGS_USAGE, CAV_ERR_1011082, CAV_ERR_MSG_2);
  }

  if(match_pattern(mode, "^[0-1]$") == 0)
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RTE_SETTINGS_USAGE, CAV_ERR_1011082, CAV_ERR_MSG_3);
  }

  gset->rte_settings.enable_rte = atoi(mode);
  if(!gset->rte_settings.enable_rte)
  {
    return 0;
  }

  NSDL4_MISC(NULL, NULL, "Setting flag protocol_enabled to RTE_PROTOCOL_ENABLED");
  global_settings->protocol_enabled |= RTE_PROTOCOL_ENABLED;
 
 
  if(ns_is_numeric(proto) == 0)
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RTE_SETTINGS_USAGE, CAV_ERR_1011082, CAV_ERR_MSG_2);
  }

  if(match_pattern(proto, "^[1-2]$") == 0)
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RTE_SETTINGS_USAGE, CAV_ERR_1011082, CAV_ERR_MSG_3);
  }
  
  if(ns_is_numeric(gui_mode) == 0)
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RTE_SETTINGS_USAGE, CAV_ERR_1011082, CAV_ERR_MSG_2);
  }

  if(match_pattern(gui_mode, "^[0-1]$") == 0)
  {
    NS_KW_PARSING_ERR(buf, runtime_changes, err_msg, G_RTE_SETTINGS_USAGE, CAV_ERR_1011082, CAV_ERR_MSG_3);
  }
 
  protocol = atoi(proto);
  terminal = atoi(gui_mode);

  nsi_rte_init(&gset->rte_settings.rte,protocol,terminal);  

  NSDL1_PARSING(NULL, NULL, "After Parsing gset->rte_gset.enable_rte = %d,gset->rte_gset.proto = %d, gset->rte_gset.gui_mode = %d ", 
                                                       gset->rte_settings.enable_rte, protocol, terminal);
  return 0;

}

int kw_set_enable_referer(char *buf, short *to_change, char *err_msg, int runtime_flag)
{
  int num;
  char keyword[MAX_DATA_LINE_LENGTH];
  char sg_name[MAX_DATA_LINE_LENGTH];
  int num_value;
  int sg_idx;
  int change_referer_on_redirect;
  
  num = sscanf(buf, "%s %s %d %d", keyword, sg_name, &num_value, &change_referer_on_redirect);
  NSDL4_MISC(NULL, NULL, "value of num_value is %d", num_value);

  if (strcmp(sg_name, "ALL") != 0) { 
    if ((sg_idx = find_sg_idx(sg_name)) == -1)
    {
      NSTL1(NULL, NULL, "Scenario group (%s) used in G_ENABLE_REFERER is"
                                " not a valid group name. Group (%s) ignored.", sg_name, sg_name);
      return -1;
    } 
  }
  
  if(num == 3)
  {
    change_referer_on_redirect = 0;
  }

  if(num_value < 0 || num_value > 1) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_ENABLE_REFERER_USAGE, CAV_ERR_1011027, CAV_ERR_MSG_3);
  }

  if(change_referer_on_redirect < 0 || change_referer_on_redirect > 1) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_ENABLE_REFERER_USAGE, CAV_ERR_1011027, CAV_ERR_MSG_3);
  }
 
  *to_change = 0; // Clearing this and then setting it again if required
  if (num_value == 1) {
    *to_change |= REFERER_ENABLED;
    if (change_referer_on_redirect == 1) { 
      *to_change |= CHANGE_REFERER_ON_REDIRECT;
    } 
  }

  NSDL4_MISC(NULL, NULL, "value of num_value is %d change_referer_on_redirect %d to_change 0x%x", num_value, change_referer_on_redirect, *to_change);
  return 0;
}

/*
  Group name must start with Alpha and can have alpa, numeric, - or _
  Max lenght is 32
*/

void val_sgrp_name (char *line, char *sgrp_name, int line_num)
{
  int sgrp_len = strlen(sgrp_name);

  NSDL3_PARSING(NULL, NULL, "Method Called, sgrp_name = %s, sgrp_len = %d", sgrp_name, sgrp_len);

  if (sgrp_len > SGRP_NAME_MAX_LENGTH)
  {
    NS_EXIT(-1, CAV_ERR_1011244, sgrp_name);
  }

  if (match_pattern(sgrp_name, "^[a-zA-Z][a-zA-Z0-9_,.-]*$") == 0)
  {
    NS_EXIT(-1, CAV_ERR_1011245);
  }
}

static int kw_set_location_hdr_save_value(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag){
  char dummy[MAX_DATA_LINE_LENGTH];
  char sg_name[MAX_DATA_LINE_LENGTH];
  int value;
  int num;
  int sg_fields = 3;

  NSDL1_PARSING(NULL, NULL, "Method Called, buf = %s", buf);

  if ((num = sscanf(buf, "%s %s %d", dummy, sg_name, &value)) != (sg_fields))
  {   
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_SAVE_LOCATION_HDR_ON_ALL_RSP_CODE_USAGE, CAV_ERR_1011040, CAV_ERR_MSG_1);
  }

  val_sgrp_name(buf, sg_name, 0);//validate group name

  if(value < 0 && value > 2){
    NS_KW_PARSING_ERR(buf, 0, err_msg, G_SAVE_LOCATION_HDR_ON_ALL_RSP_CODE_USAGE, CAV_ERR_1011040, CAV_ERR_MSG_3);
  }

  gset->save_loc_hdr_on_all_rsp_code = value;

  NSDL1_PARSING(NULL, NULL, "After Parsing G_SAVE_LOCATION_HDR_ON_ALL_RSP_CODE GroupName = %s",
                            "keyword value = %d", sg_name, gset->save_loc_hdr_on_all_rsp_code);
  return 0;
}

/**
 * Returns: Zero if keyword is consumed, -1 otherwise
 * Args:
 *      pass == 1 :- first pass
 *      pass == 2 :- Second pass
 * Assuming line will always contain "<Keyword> <Group/All> ..."
 * All per group configuratble keywords parsing here.
 *
 */
int parse_group_keywords(char *line, int pass, int line_num) 
{
  char grp[MAX_DATA_LINE_LENGTH];
  char text[MAX_TEXT_LINE_LENGTH];
  char keyword[MAX_DATA_LINE_LENGTH];
  char *buf = line;
  int num;
  //int num_value, max_value;
  int grp_idx = -2;
  char err_msg[MAX_ERR_MSG_LENGTH];
  
  memset(keyword, 0, sizeof(keyword));
  memset(grp, 0, sizeof(grp));
  memset(text, 0, sizeof(text));

  NSDL2_PARSING(NULL, NULL, "method called. line = %s, pass = %d", line, pass);

  if ((num = sscanf(buf, "%s %s %s", keyword, grp, text)) < 3) {
    NS_EXIT(-1, CAV_ERR_1011243, keyword);
  }

  /* These checks ensure that we dont parse ALL in pass 2 and vice-versa */
  if (strcasecmp(grp, "ALL") == 0 && pass == 2)
    return -1;

  if (strcasecmp(grp, "ALL") != 0 && pass == 1)
    return -1;

  if (strcasecmp(grp, "ALL") == 0) {
    grp_idx = -1;
  } else {
    val_sgrp_name (buf, grp, line_num);
    grp_idx = find_group_idx(grp);
    if (grp_idx < 0) {           /* Group can only be either ALL or a valid group */
      NSDL2_PARSING(NULL, NULL, "group name not valid. line = %s, pass = %d", line, pass);
      NS_DUMP_WARNING("Group name (%s) used in keyword %s is not a valid scenario group. Hence, ignoring group.", grp, keyword);
      return -1;
    }

    /*
    NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_CRITICAL, __FILE__, (char*)__FUNCTION__,
                                "With keyword %s, group field can only be either ALL or a valid group name.", keyword);
    */

  }

      /*
      NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_CRITICAL, __FILE__, (char*)__FUNCTION__,
                                "Group name %s with keyword %s not found.", grp, keyword);
      */
      //NSTL1_OUT(NULL, NULL,  "Group name %s with keyword %s not found.\n", grp, keyword);
      //exit(-1);

/*
  if (strncasecmp(keyword, "G_IDLE_SECS", strlen("G_IDLE_SECS")) == 0) {
    if (pass == 1) {
      kw_set_idle_secs(buf, &group_default_settings->idle_secs, err_msg);
    } if (pass == 2) {
      kw_set_idle_secs(buf, &runProfTable[grp_idx].gset.idle_secs, err_msg);
    }
    
  } else */ if (strcasecmp(keyword, "G_IDLE_MSECS") == 0) {
    /* Add validation w.r.t. Page Reload & Click Away*/
    if(validate_idle_secs_wrt_reload_clickaway(pass, grp_idx, buf) == 0) {
      if (pass == 1) {
        kw_set_idle_msecs(buf, group_default_settings, err_msg, 0);
      } if (pass == 2) {
        kw_set_idle_msecs(buf, &runProfTable[grp_idx].gset, err_msg, 0);
      }
    }
  } else if (strcasecmp(keyword, "G_KA_PCT") == 0) {
    if (pass == 1) {
      kw_set_ka_pct(buf, &group_default_settings->ka_pct, err_msg, 0);
    } else if (pass == 2) {
      kw_set_ka_pct(buf, &runProfTable[grp_idx].gset.ka_pct, err_msg, 0);
    }
  } else if (strcasecmp(keyword, "G_NUM_KA") == 0) {
    if (pass == 1) {
      kw_set_num_ka(buf, &group_default_settings->num_ka_min, &group_default_settings->num_ka_range, err_msg, 0);
    } else if (pass == 2) {
      kw_set_num_ka(buf, &runProfTable[grp_idx].gset.num_ka_min, &runProfTable[grp_idx].gset.num_ka_range, err_msg, 0);
    }
  } else if (strcasecmp(keyword, "G_MAX_URL_RETRIES") == 0) {
    if (pass  == 1) {
      kw_set_max_url_retries(buf, &group_default_settings->max_url_retries, &group_default_settings->retry_on_timeout, err_msg, 0);
    } else if (pass == 2) {
      kw_set_max_url_retries(buf, &runProfTable[grp_idx].gset.max_url_retries, &runProfTable[grp_idx].gset.retry_on_timeout, err_msg, 0);
    }
  } else if (strcasecmp(keyword, "G_NO_VALIDATION") == 0) {
    if (pass  == 1)
      kw_set_no_validation(buf, &group_default_settings->no_validation, err_msg, 0);
    if (pass == 2)
      kw_set_no_validation(buf, &runProfTable[grp_idx].gset.no_validation, err_msg, 0);

  } else if (strcasecmp(keyword, "G_JAVA_SCRIPT") == 0) {
    if (pass  == 1)
      kw_set_g_java_script(buf, group_default_settings, err_msg, 0);
    if (pass == 2)
      kw_set_g_java_script(buf, &runProfTable[grp_idx].gset, err_msg, 0);
  } else if ((strcasecmp(keyword, "G_DISABLE_REUSEADDR") == 0)) {
    if (atoi(text)) {
      if (pass  == 1)
        kw_set_disable_reuseaddr(buf, &group_default_settings->disable_reuseaddr, err_msg, 0);
      if (pass  == 2)
        kw_set_disable_reuseaddr(buf, &runProfTable[grp_idx].gset.disable_reuseaddr, err_msg, 0);
    }
  } else if ((strcasecmp(keyword, "G_DISABLE_HOST_HEADER") == 0)) {
    if (pass  == 1)
      kw_set_disable_host_header(buf, &group_default_settings->disable_headers, err_msg, 0);
    //if (pass  == 2)
      //kw_set_disable_host_header(buf, &runProfTable[grp_idx].gset.disable_headers, err_msg, 0);
  } else if ((strcasecmp(keyword, "G_DISABLE_UA_HEADER") == 0)) {
    if (pass  == 1)
      kw_set_disable_ua_header(buf, &group_default_settings->disable_headers, err_msg, 0);
    //if (pass  == 2)
      //kw_set_disable_ua_header(buf, &runProfTable[grp_idx].gset.disable_headers, err_msg, 0);

  } else if ((strcasecmp(keyword, "G_DISABLE_ACCEPT_HEADER") == 0)){
      if (pass  == 1)
        kw_set_disable_accept_header(buf, &group_default_settings->disable_headers, err_msg, 0);
      //if (pass  == 2)
        //kw_set_disable_accept_header(buf, &runProfTable[grp_idx].gset.disable_headers, err_msg, 0);

  } else if ((strcasecmp(keyword, "G_DISABLE_ACCEPT_ENC_HEADER") == 0)){
    if (pass  == 1)
      kw_set_disable_accept_enc_header(buf, &group_default_settings->disable_headers, err_msg, 0);
    //if (pass  == 2)
      //kw_set_disable_accept_enc_header(buf, &runProfTable[grp_idx].gset.disable_headers, err_msg, 0);
  } else if ((strcasecmp(keyword, "G_DISABLE_KA_HEADER") == 0)) {
    if (pass  == 1)
      kw_set_disable_ka_header(buf, &group_default_settings->disable_headers, err_msg, 0);
    //if (pass  == 2)
      //kw_set_disable_ka_header(buf, &runProfTable[grp_idx].gset.disable_headers, err_msg, 0);

  } else if ((strcasecmp(keyword, "G_DISABLE_CONNECTION_HEADER") == 0)){
    if (pass  == 1)
      kw_set_disable_connection_header(buf, &group_default_settings->disable_headers, err_msg,  0);
    //if (pass  == 2)
      //kw_set_disable_connection_header(buf, &runProfTable[grp_idx].gset.disable_headers, err_msg, 0 );

  } else if ((strcasecmp(keyword, "G_DISABLE_ALL_HEADER") == 0)) {
    if (pass  == 1)
      kw_set_disable_all_header(buf, &group_default_settings->disable_headers, err_msg, 0);
    //if (pass  == 2)
      //kw_set_disable_all_header(buf, &runProfTable[grp_idx].gset.disable_headers, err_msg, 0);
  } else if (strcasecmp(keyword, "G_USE_RECORDED_HOST_IN_HOST_HDR") == 0) {
    if (pass  == 1)
      kw_set_use_recorded_host_in_host_hdr(buf, &group_default_settings->use_rec_host, err_msg, 0);
    if (pass  == 2)
      kw_set_use_recorded_host_in_host_hdr(buf, &runProfTable[grp_idx].gset.use_rec_host, err_msg, 0);

  } else if (strcasecmp(keyword, "G_AVG_SSL_REUSE") == 0) {
    if (pass == 1) {
      kw_set_avg_ssl_reuse(buf, &group_default_settings->avg_ssl_reuse, err_msg, 0);
      NSDL3_SSL(NULL, NULL, "Global group_default_settings->avg_ssl_reuse = %d", group_default_settings->avg_ssl_reuse);
    }
    if (pass == 2) {
      kw_set_avg_ssl_reuse(buf, &runProfTable[grp_idx].gset.avg_ssl_reuse, err_msg, 0);
      NSDL3_SSL(NULL, NULL, "Global group_default_settings->avg_ssl_reuse = %d", group_default_settings->avg_ssl_reuse);
    }
  } else if (strcasecmp(keyword, "G_SSL_CLEAN_CLOSE_ONLY") == 0) {
    if (pass == 1) {
      kw_set_ssl_clean_close_only(buf, &group_default_settings->ssl_clean_close_only, err_msg, 0);
      NSDL3_SSL(NULL, NULL, "group_default_settings->ssl_clean_close_only = %d", group_default_settings->ssl_clean_close_only);
    }
    if (pass == 2) {
      kw_set_ssl_clean_close_only(buf, &runProfTable[grp_idx].gset.ssl_clean_close_only, err_msg, 0);
      NSDL3_SSL(NULL, NULL, "runProfTable[grp_idx].gset.ssl_clean_close_only = %d", runProfTable[grp_idx].gset.ssl_clean_close_only);
    }
  } else if (strcasecmp(keyword, "G_CIPHER_LIST") == 0) {
    if (pass == 1) {
      kw_set_ssl_cipher_list(buf, group_default_settings->ssl_ciphers, 0, err_msg);
      NSDL3_SSL(NULL, NULL, "group_default_settings->ssl_ciphers = %s", group_default_settings->ssl_ciphers);
    }
    if (pass == 2) {
      kw_set_ssl_cipher_list(buf, runProfTable[grp_idx].gset.ssl_ciphers, 0, err_msg);
      NSDL3_SSL(NULL, NULL, "runProfTable[grp_idx].gset.ssl_ciphers = %s", runProfTable[grp_idx].gset.ssl_ciphers);
    }
  } else if (strcasecmp(keyword, "G_SSL_SETTINGS") == 0) {
    if (pass == 1)
      kw_set_ssl_settings(buf, &group_default_settings->ssl_settings, 0, err_msg);
    if (pass == 2) {
      kw_set_ssl_settings(buf, &runProfTable[grp_idx].gset.ssl_settings, 0, err_msg);
    }
  } else if (strcasecmp(keyword, "G_ERR_CODE_OK") == 0) {
    if (pass == 1)
      kw_set_err_code_ok(buf, &group_default_settings->errcode_ok, err_msg, 0);
    if (pass == 2)
      kw_set_err_code_ok(buf, &runProfTable[grp_idx].gset.errcode_ok, err_msg, 0);

  } else if (strcasecmp(keyword, "G_ON_EURL_ERR") == 0) {
    if (pass == 1)
      kw_set_on_eurl_err(buf, &group_default_settings->on_eurl_err, err_msg, 0);
    if (pass == 2)
      kw_set_on_eurl_err(buf, &runProfTable[grp_idx].gset.on_eurl_err, err_msg, 0);
  } else if (strcasecmp(keyword, "G_CONTINUE_ON_PAGE_ERROR") == 0) {
      kw_set_continue_on_page_error (buf, err_msg, 0);

  } else if (strcasecmp(keyword, "G_LOGGING") == 0) {
    if (pass == 1)
      kw_set_logging(buf, &group_default_settings->log_level, &group_default_settings->log_dest, err_msg);
    if (pass == 2)
      kw_set_logging(buf, &runProfTable[grp_idx].gset.log_level, &runProfTable[grp_idx].gset.log_dest, err_msg);

  } else if (strcasecmp(keyword, "G_TRACING") == 0) {
      if (pass == 1) {
        kw_set_tracing(buf, &group_default_settings->trace_level,
                       &group_default_settings->max_trace_level,
                       &group_default_settings->trace_dest,
                       &group_default_settings->max_trace_dest,
                       &group_default_settings->trace_on_fail,
                       &group_default_settings->max_log_space, 
                       &group_default_settings->trace_inline_url, 
                       &group_default_settings->trace_limit_mode, &group_default_settings->trace_limit_mode_val, err_msg, 0);
       }
      if (pass == 2) {
        kw_set_tracing(buf, &runProfTable[grp_idx].gset.trace_level,
                       &runProfTable[grp_idx].gset.max_trace_level,
                       &runProfTable[grp_idx].gset.trace_dest,
                       &runProfTable[grp_idx].gset.max_trace_dest,
                       &runProfTable[grp_idx].gset.trace_on_fail,
                       &runProfTable[grp_idx].gset.max_log_space,
                       &runProfTable[grp_idx].gset.trace_inline_url,
                       &runProfTable[grp_idx].gset.trace_limit_mode, &runProfTable[grp_idx].gset.trace_limit_mode_val, err_msg, 0);
      }
  } else if (strcasecmp(keyword, "G_REPORTING") == 0) {
    if (pass == 1)
      kw_set_reporting (buf, &group_default_settings->report_level, err_msg, 0);
    if (pass == 2)
      kw_set_reporting (buf, &runProfTable[grp_idx].gset.report_level, err_msg, 0);
  //} else if (strncasecmp(keyword, "G_MODULEMASK", strlen("G_MODULEMASK")) == 0) {
  } else if(strcasecmp(keyword, "G_TRACING_LIMIT") == 0) {
    if(pass == 1) {
      if(kw_set_traceing_limit(buf, &group_default_settings->max_trace_param_entries, &group_default_settings->max_trace_param_value_size, err_msg) != 0) 
      {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
    if(pass == 2) {
      if(kw_set_traceing_limit(buf, &runProfTable[grp_idx].gset.max_trace_param_entries, &runProfTable[grp_idx].gset.max_trace_param_value_size, err_msg) != 0)
      {
        NS_EXIT(-1, "%s", err_msg);
      }
    } 
  } else if (strcasecmp(keyword, "G_MODULEMASK") == 0) {
    if (pass == 1)
      kw_set_modulemask(buf, &group_default_settings->module_mask, err_msg, 0);
    if (pass == 2)
      kw_set_modulemask(buf, &runProfTable[grp_idx].gset.module_mask, err_msg, 0);
  //} else if (strncasecmp(keyword, "G_DEBUG_MASK", strlen("G_DEBUG_MASK")) == 0) {
  } else if (strcasecmp(keyword, "G_DEBUG_MASK") == 0) {
    if (pass == 1)
      kw_set_debug_mask(text, &group_default_settings->debug);
    if (pass == 2)
      kw_set_debug_mask(text, &runProfTable[grp_idx].gset.debug);

  } else if (strcasecmp(keyword, "G_DEBUG") == 0) {
    if (pass == 1)
      kw_set_debug(buf, &group_default_settings->debug, err_msg, 0);
    if (pass == 2) {
      kw_set_debug(buf, &runProfTable[grp_idx].gset.debug, err_msg, 0);
    }

  } else if (strcasecmp(keyword, "G_ENABLE_REFERER") == 0) {
    if (pass == 1)
      kw_set_enable_referer(buf, &group_default_settings->enable_referer, err_msg, 0);

      //group_default_settings->enable_referer = atoi(text);
    if (pass == 2)
    {
      kw_set_enable_referer(buf, &runProfTable[grp_idx].gset.enable_referer, err_msg, 0);
    }

  #ifdef RMI_MODE
  } else if (strcasecmp(keyword, "G_URL_IDLE_SECS") == 0) {
    if (pass == 1)
      kw_set_url_idle_secs(buf, &group_default_settings->url_idle_secs, err_msg, 0);
    if (pass == 2)
      kw_set_url_idle_secs(buf, &runProfTable[grp_idx].gset.url_idle_secs, err_msg, 0);
  #endif
  } else if (strcasecmp(keyword, "G_USER_CLEANUP_MSECS") == 0) {
    if (pass == 1)
      kw_set_user_cleanup_msecs(buf, &group_default_settings->user_cleanup_time, err_msg, 0);
    if (pass == 2)
      kw_set_user_cleanup_msecs(buf, &runProfTable[grp_idx].gset.user_cleanup_time, err_msg,  0);
  } else if (strcasecmp(keyword, "G_OVERRIDE_RECORDED_THINK_TIME") == 0) {
      if (kw_set_override_recorded_think_time(buf, err_msg, 0) != 0) // added runtime flag value 
      {
        NS_EXIT(-1, "%s", err_msg);
      }
  } else if (strcasecmp(keyword, "G_MAX_USERS") == 0) {
    if (pass == 1) {
      if (kw_set_g_max_users(buf, &group_default_settings->grp_max_user_limit, err_msg, 0) != 0)
      {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
    if (pass == 2) {
      if (kw_set_g_max_users(buf, &runProfTable[grp_idx].gset.grp_max_user_limit, err_msg, 0) != 0)
      {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
  } else if (strcasecmp(keyword, "G_HTTP_CACHING") == 0) {
    if (pass == 1) {
      kw_set_g_http_caching(buf, group_default_settings, err_msg, 0);
    }
    if (pass == 2) {
      kw_set_g_http_caching(buf, &runProfTable[grp_idx].gset, err_msg, 0);
    }
  } else if (strcasecmp(keyword, "G_HTTP_CACHE_TABLE_SIZE") == 0) {
      if (pass == 1) {
        kw_set_g_http_cache_table_size(buf, group_default_settings, err_msg, 0);
      }
      if (pass == 2) {
        kw_set_g_http_cache_table_size(buf, &runProfTable[grp_idx].gset, err_msg, 0);
      }
  } else if (strcasecmp(keyword, "G_HTTP_TEST_CACHING") == 0) {
    if (pass == 1) {
      if (kw_set_g_http_caching_test(buf, group_default_settings, err_msg) != 0) {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
    if (pass == 2) {
      if (kw_set_g_http_caching_test(buf, &runProfTable[grp_idx].gset, err_msg) != 0) {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
  } else if (strcasecmp(keyword, "G_HTTP_CACHE_MASTER_TABLE") == 0) {
    if (pass == 1) {
      kw_set_g_http_caching_master_table(buf, group_default_settings, err_msg, 0);
    }
    if (pass == 2) {
      kw_set_g_http_caching_master_table(buf, &runProfTable[grp_idx].gset, err_msg, 0);
    }
  } else if (strcasecmp(keyword, "G_VUSER_TRACE") == 0) {
    if (pass == 1) {
      kw_set_g_user_trace(buf, group_default_settings, err_msg, 0);
    }
    if (pass == 2) {
      kw_set_g_user_trace(buf, &runProfTable[grp_idx].gset, err_msg, 0);
    }
  } else if (strcasecmp(keyword, "G_PAGE_THINK_TIME") == 0) {
    kw_set_g_page_think_time(buf, err_msg, 0);
  } else if (strcasecmp(keyword, "G_HTTP_HDR") == 0) {
       kw_set_g_http_hdr(buf, err_msg, 0);
    } 
   else if (strcasecmp(keyword, "G_INLINE_DELAY") == 0) {
    kw_set_g_inline_delay(buf, err_msg, 0);
  } else if (strcasecmp(keyword, "G_INLINE_MIN_CON_REUSE_DELAY") == 0) {
    if (pass == 1) {
      kw_set_g_inline_min_con_reuse_delay(buf, err_msg, &group_default_settings->min_con_reuse_delay,
                                          &group_default_settings->max_con_reuse_delay);
    }
    if (pass == 2) {
      kw_set_g_inline_min_con_reuse_delay(buf, err_msg, &runProfTable[grp_idx].gset.min_con_reuse_delay,
                                          &runProfTable[grp_idx].gset.max_con_reuse_delay);
    }
  } else if (strcasecmp(keyword, "G_SESSION_PACING") == 0) {
    kw_set_g_session_pacing(buf, err_msg, 0);
  } else if (strcasecmp(keyword, "G_FIRST_SESSION_PACING") == 0) {
    kw_set_g_first_session_pacing(buf, err_msg, 0);
  } else if (strcasecmp(keyword, "G_NEW_USER_ON_SESSION") == 0) {
    kw_set_g_new_user_on_session(buf, err_msg, 0);
  } else if (strcasecmp(keyword, "G_PAGE_RELOAD") == 0) {
    kw_set_g_page_reload(buf, pass, err_msg, 0);
  } else if (strcasecmp(keyword, "G_CLICK_AWAY") == 0) {
    kw_set_g_page_click_away(buf, pass, err_msg, 0);
  } else if (strcasecmp(keyword, "G_USE_DNS") == 0) {
    if(pass == 1)
      kw_set_use_dns(buf, group_default_settings, err_msg, 0);
    else if(pass == 2) 
      kw_set_use_dns(buf, &runProfTable[grp_idx].gset, err_msg, 0);
  } else if (strcasecmp(keyword, "G_SMTP_TIMEOUT_GREETING") == 0) {
    if (pass == 1) {
      kw_set_smtp_timeout(buf, &group_default_settings->smtp_timeout_greeting, err_msg, 0);
    } else if (pass == 2) {
      kw_set_smtp_timeout(buf, &runProfTable[grp_idx].gset.smtp_timeout_greeting, err_msg, 0);
    }
  } else if (strcasecmp(keyword, "G_SMTP_TIMEOUT_MAIL") == 0) {
    if (pass == 1) {
      kw_set_smtp_timeout(buf, &group_default_settings->smtp_timeout_mail, err_msg, 0);
    } else if (pass == 2) {
      kw_set_smtp_timeout(buf, &runProfTable[grp_idx].gset.smtp_timeout_mail, err_msg, 0);
    }
  } else if (strcasecmp(keyword, "G_SMTP_TIMEOUT_RCPT") == 0) {
    if (pass == 1) {
      kw_set_smtp_timeout(buf, &group_default_settings->smtp_timeout_rcpt, err_msg, 0);
    } else if (pass == 2) {
      kw_set_smtp_timeout(buf, &runProfTable[grp_idx].gset.smtp_timeout_rcpt, err_msg, 0);
    }
  } else if (strcasecmp(keyword, "G_SMTP_TIMEOUT_DATA_INIT") == 0) {
    if (pass == 1) {
      kw_set_smtp_timeout(buf, &group_default_settings->smtp_timeout_data_init, err_msg, 0);
    } else if (pass == 2) {
      kw_set_smtp_timeout(buf, &runProfTable[grp_idx].gset.smtp_timeout_data_init, err_msg, 0);
    }
  } else if (strcasecmp(keyword, "G_SMTP_TIMEOUT_DATA_BLOCK") == 0) {
    if (pass == 1) {
      kw_set_smtp_timeout(buf, &group_default_settings->smtp_timeout_data_block, err_msg, 0);
    } else if (pass == 2) {
      kw_set_smtp_timeout(buf, &runProfTable[grp_idx].gset.smtp_timeout_data_block, err_msg, 0);
    }
  } else if (strcasecmp(keyword, "G_SMTP_TIMEOUT_DATA_TERM") == 0) {
    if (pass == 1) {
      kw_set_smtp_timeout(buf, &group_default_settings->smtp_timeout_data_term, err_msg, 0);
    } else if (pass == 2) {
      kw_set_smtp_timeout(buf, &runProfTable[grp_idx].gset.smtp_timeout_data_term, err_msg, 0);
    }
  } else if (strcasecmp(keyword, "G_SMTP_TIMEOUT") == 0) {
    if (pass == 1) {
      kw_set_smtp_timeout(buf, &group_default_settings->smtp_timeout, err_msg, 0);
    } else if (pass == 2) {
      kw_set_smtp_timeout(buf, &runProfTable[grp_idx].gset.smtp_timeout, err_msg, 0);
    }
  } else if (strcasecmp(keyword, "G_POP3_TIMEOUT") == 0) {
    if (pass == 1) {
      if (kw_set_pop3_timeout(buf, &group_default_settings->pop3_timeout, err_msg) != 0) {
        NS_EXIT(-1, "%s", err_msg);
      }
    } else if (pass == 2) {
      if (kw_set_pop3_timeout(buf, &runProfTable[grp_idx].gset.pop3_timeout, err_msg) != 0) {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
  } else if (strcasecmp(keyword, "G_FTP_TIMEOUT") == 0) {
    if (pass == 1) {
      if (kw_set_ftp_timeout(buf, &group_default_settings->ftp_timeout, err_msg) != 0) {
        NS_EXIT(-1, "%s", err_msg);
      }
    } else if (pass == 2) {
      if (kw_set_ftp_timeout(buf, &runProfTable[grp_idx].gset.ftp_timeout, err_msg) != 0) {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
  } else if (strcasecmp(keyword, "G_DNS_TIMEOUT") == 0) {
    if (pass == 1) {
      if (kw_set_dns_timeout(buf, &group_default_settings->dns_timeout, err_msg) != 0) {
        NS_EXIT(-1, "%s", err_msg);
      }
    } else if (pass == 2) {
      if (kw_set_dns_timeout(buf, &runProfTable[grp_idx].gset.dns_timeout, err_msg) != 0) {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
  } else if (strcasecmp(keyword, "G_LDAP_TIMEOUT") == 0) {
    if (pass == 1) {
      if (kw_set_ldap_timeout(buf, &group_default_settings->ldap_timeout, err_msg) != 0) {
        NS_EXIT(-1, "%s", err_msg);
      }
    } else if (pass == 2) {
      if (kw_set_ldap_timeout(buf, &runProfTable[grp_idx].gset.ldap_timeout, err_msg) != 0) {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
  } else if (strcasecmp(keyword, "G_IMAP_TIMEOUT") == 0) {
    if (pass == 1) {
      if (kw_set_imap_timeout(buf, &group_default_settings->imap_timeout, err_msg) != 0) {
        NS_EXIT(-1, "%s", err_msg);
      }
    } else if (pass == 2) {
      if (kw_set_imap_timeout(buf, &runProfTable[grp_idx].gset.imap_timeout, err_msg) != 0) {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
  }else if (strcasecmp(keyword, "G_JRMI_TIMEOUT") == 0) {
    if (pass == 1) {
      if (kw_set_jrmi_timeout(buf, &group_default_settings->jrmi_timeout, err_msg) != 0) {
        NS_EXIT(-1, "%s", err_msg);
      }
    } else if (pass == 2) {
      if (kw_set_jrmi_timeout(buf, &runProfTable[grp_idx].gset.jrmi_timeout, err_msg) != 0) {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
  } else if (strcasecmp(keyword, "G_KA_TIME_MODE") == 0) {
    if (pass == 1) {
      kw_set_ka_time_mode(buf, &group_default_settings->ka_mode, err_msg, 0);
    } else if (pass == 2) {
      kw_set_ka_time_mode(buf, &runProfTable[grp_idx].gset.ka_mode, err_msg, 0);
    }
  } else if (strcasecmp(keyword, "G_KA_TIME") == 0) {
    if (pass == 1) {
      kw_set_ka_timeout(buf, &group_default_settings->ka_timeout, err_msg, 0);
    } else if (pass == 2) {
      kw_set_ka_timeout(buf, &runProfTable[grp_idx].gset.ka_timeout, err_msg, 0);
    }
  } else if (strcasecmp(keyword, "G_SCRIPT_MODE") == 0) {
    // pass 2 is done earlier for this keyword
    if (pass == 1) {
      kw_set_g_script_mode(buf, group_default_settings, err_msg, 0); // method display error and exit, so no need to check
    } 
  } else if (strcasecmp(keyword, "G_ENABLE_PIPELINING") == 0) {
    if (pass == 1) {
      kw_set_g_enable_pipelining(buf, group_default_settings, err_msg);
      }
  } else if  (strcasecmp(keyword, "G_GET_NO_INLINED_OBJ") == 0) {
    NSDL4_MISC(NULL, NULL, "pass %d keyword %s grp %s text %s",pass, keyword, grp, text);
    // pass 2 is done earlier for this keyword 
    if (pass == 1 ){
      if (kw_set_get_no_inlined_obj(buf, &(group_default_settings->get_no_inlined_obj), err_msg, 0) != 0) {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
  } else if (strcasecmp(keyword, "G_AUTO_FETCH_EMBEDDED") == 0) {
    kw_set_auto_fetch_embedded(buf, err_msg, 0);
  } else if ((strcasecmp(keyword, "G_INLINE_INCLUDE_DOMAIN_PATTERN") == 0) || (strcasecmp(keyword, "G_INLINE_INCLUDE_URL_PATTERN") == 0) || (strcasecmp(keyword, "G_INLINE_EXCLUDE_DOMAIN_PATTERN") == 0) || (strcasecmp(keyword, "G_INLINE_EXCLUDE_URL_PATTERN") == 0)) {
    if(pass == 1){
      kw_set_inline_filter_patterns(buf, group_default_settings, -1);
    }
    if(pass == 2) {
      kw_set_inline_filter_patterns(buf, &runProfTable[grp_idx].gset, grp_idx);
    }
 
  } else if (strcasecmp(keyword, "G_INC_EX_DOMAIN_SETTINGS") == 0) {
    if (pass == 1) {
      NSDL2_PARSING(NULL, NULL, "Received G_INC_EX_DOMAIN_SETTINGS");
      kw_set_include_exclude_domain_settings(buf, group_default_settings);
    }
    if (pass == 2) {
      kw_set_include_exclude_domain_settings(buf, &runProfTable[grp_idx].gset);
    }
  } else if (strcasecmp(keyword, "G_ENABLE_NET_DIAGNOSTICS") == 0) {
    if (pass == 1) {
      if ( kw_set_g_enable_net_diagnostics(buf, group_default_settings, err_msg) != 0) {
        NS_EXIT(-1, "%s", err_msg);
      }
    } 
    if (pass == 2) {
      if ( kw_set_g_enable_net_diagnostics(buf, &runProfTable[grp_idx].gset, err_msg) != 0) {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
  } else if (strcasecmp(keyword, "G_HTTP_AUTH_NTLM") == 0) {
    if (pass == 1) {
      kw_set_g_http_auth_ntlm(buf, group_default_settings, err_msg, 0);
    } 
    if (pass == 2) {
      kw_set_g_http_auth_ntlm(buf, &runProfTable[grp_idx].gset, err_msg, 0);
    }
  } else if (strcasecmp(keyword, "G_HTTP_AUTH_KERB") == 0) {
    if (pass == 1) {
      kw_set_g_http_auth_kerb(buf, group_default_settings, err_msg, 0);
    } 
    if (pass == 2) {
      kw_set_g_http_auth_kerb(buf, &runProfTable[grp_idx].gset, err_msg, 0);
    }
  } else if (strcasecmp(keyword, "G_SAVE_LOCATION_HDR_ON_ALL_RSP_CODE") == 0) {
    if (pass == 1) {
      kw_set_location_hdr_save_value(buf, group_default_settings, err_msg, 0);
    } 
    if (pass == 2) {
      kw_set_location_hdr_save_value(buf, &runProfTable[grp_idx].gset, err_msg, 0);
    }
  } 
  else if (strcasecmp(keyword, "G_MAX_PAGES_PER_TX") == 0) {
    if (pass == 1) {
      if (kw_set_g_max_pages_per_tx(buf, &group_default_settings->max_pages_per_tx, err_msg, 0) != 0) {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
    if (pass == 2) {
      if (kw_set_g_max_pages_per_tx(buf, &runProfTable[grp_idx].gset.max_pages_per_tx, err_msg, 0) != 0) {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
  }
  else if (strcasecmp(keyword, "G_PROXY_SERVER") == 0) {
    if (pass == 1) {
      if(kw_set_g_proxy_server(buf, grp_idx, err_msg, RTC_DISABLE) != 0) {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
    if (pass == 2) {
      if(kw_set_g_proxy_server(buf, grp_idx, err_msg, RTC_DISABLE) != 0) {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
  }
  else if (strcasecmp(keyword, "G_PROXY_EXCEPTIONS") == 0) {
    if (pass == 1) {
      if(kw_set_g_proxy_exceptions(buf, grp_idx, err_msg, 0) != 0) {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
    if(pass == 2){
      if(kw_set_g_proxy_exceptions (buf, grp_idx, err_msg, 0) != 0) {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
  }
  else if (strcasecmp(keyword, "G_PROXY_AUTH") == 0) {
    if (pass == 1) {
      kw_set_g_proxy_auth(buf, grp_idx, err_msg, RTC_DISABLE);
    }
    if (pass == 2) {
      kw_set_g_proxy_auth(buf, grp_idx, err_msg, RTC_DISABLE);
    }
  } else if (strcasecmp(keyword, "G_MAX_CON_PER_VUSER") == 0) {
    if (pass  == 1) {
      kw_set_g_max_con_per_vuser(buf, group_default_settings, err_msg, 0);
    }
    if (pass == 2) {
      kw_set_g_max_con_per_vuser(buf, &runProfTable[grp_idx].gset, err_msg, 0);
    }
  } else if (strcasecmp(keyword, "G_ENABLE_NETWORK_CACHE_STATS") == 0) {
    if (pass == 1) {
      kw_set_g_enable_network_cache_stats(buf, group_default_settings, err_msg, 0);
    }
    if (pass == 2) {
      kw_set_g_enable_network_cache_stats(buf, &runProfTable[grp_idx].gset, err_msg, 0);
    }
  } else if (strcasecmp(keyword, "G_PROXY_PROTO_MODE") == 0) {
    if (pass == 1) {
      if(kw_set_g_proxy_proto_mode(buf, group_default_settings, err_msg) != 0) {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
    if (pass == 2) {
      if(kw_set_g_proxy_proto_mode(buf, &runProfTable[grp_idx].gset, err_msg) != 0) {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
  } else if (strcasecmp(keyword, "G_SEND_NS_TX_HTTP_HEADER") == 0) {
    //G_SEND_NS_TX_HTTP_HEADER  ALL  <mode>  [<HTTP Header name>]
    if (pass == 1) {
      kw_set_g_send_ns_tx_http_header(buf, group_default_settings, err_msg, 0);
    } 
    if (pass == 2) {
      kw_set_g_send_ns_tx_http_header(buf, &runProfTable[grp_idx].gset, err_msg, 0);
    }
  }else if (strcasecmp(keyword, "G_END_TX_NETCACHE") == 0) {
    // G_END_TX_NETCACHE  <group-name> <mode>
    if (pass == 1) {
      kw_set_g_end_tx_netcache(buf, group_default_settings, err_msg, 0);
    } 
    if (pass == 2) {
      kw_set_g_end_tx_netcache(buf, &runProfTable[grp_idx].gset, err_msg, 0);
    }
  }
  else if (strcasecmp(keyword, "G_SESSION_RETRY") == 0) {
    if (pass == 1) {
      NSDL4_MISC(NULL, NULL, "Pass 1: kw_set_g_session_retry()");
      kw_set_g_session_retry(buf, group_default_settings, err_msg, 0);
    }
    if (pass == 2) {
      NSDL4_MISC(NULL, NULL, "Pass 2: kw_set_g_session_retry()");
      kw_set_g_session_retry(buf, &runProfTable[grp_idx].gset, err_msg, 0);
    }
  } else if (strcasecmp(keyword, "G_RBU") == 0) {
    // G_RBU  <group-name> <mode>
    if (pass == 1) {
      NSDL4_MISC(NULL, NULL, "Pass 1: kw_set_g_rbu()");
      if (kw_set_g_rbu(buf, group_default_settings, err_msg, 0) != 0)
      {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
  }
  else if (strcasecmp(keyword, "G_RBU_CAPTURE_CLIPS") == 0) {
    if (pass == 1) {
      NSDL4_MISC(NULL, NULL, "Pass 1: kw_set_g_rbu_capture_clips()");
      if (kw_set_g_rbu_capture_clips(buf, group_default_settings, err_msg, 0) != 0)
      {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
    if (pass == 2) {
      NSDL4_MISC(NULL, NULL, "Pass 2: kw_set_g_rbu_capture_clips()");
      if (kw_set_g_rbu_capture_clips(buf, &runProfTable[grp_idx].gset, err_msg, 0) != 0)
      {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
  } 
   else if (strcasecmp(keyword, "G_RBU_CACHE_SETTING") == 0) {
    if(pass == 1) {
      NSDL4_MISC(NULL, NULL, "Pass 1: kw_set_g_rbu_cache_setting()");
      if(kw_set_g_rbu_cache_setting(buf, group_default_settings, err_msg, 0) != 0)
      {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
    if(pass == 2) {
      NSDL4_MISC(NULL, NULL, "Pass 2: kw_set_g_rbu_cache_setting()");
      if(kw_set_g_rbu_cache_setting(buf, &runProfTable[grp_idx].gset, err_msg, 0) != 0)
      {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
  } else if (strcasecmp(keyword, "G_RBU_PAGE_LOADED_TIMEOUT") == 0) {
    if(pass == 1) {
      NSDL4_MISC(NULL, NULL, "Pass 1: kw_set_g_rbu_page_loaded_timeout()");
      if(kw_set_g_rbu_page_loaded_timeout(buf, group_default_settings, err_msg, 0) != 0)
      {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
    if(pass == 2) {
      NSDL4_MISC(NULL, NULL, "Pass 2: kw_set_g_rbu_page_loaded_timeout()");
      if(kw_set_g_rbu_page_loaded_timeout(buf, &runProfTable[grp_idx].gset, err_msg, 0) != 0)
      {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
  } else if(strcasecmp(keyword, "G_RBU_ADD_HEADER") == 0) {
    if(pass == 1) {
      NSDL4_MISC(NULL, NULL, "Pass 1: kw_set_g_rbu_add_header()");
      if(kw_set_g_rbu_add_header(buf, group_default_settings, err_msg, 0) != 0)
      {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
    if(pass == 2) {
      NSDL4_MISC(NULL, NULL, "Pass 2: kw_set_g_rbu_add_header()");
      if(kw_set_g_rbu_add_header(buf, &runProfTable[grp_idx].gset, err_msg, 0) != 0)
      {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
  } else if (strcasecmp(keyword, "G_RBU_CLEAN_UP_PROF_ON_SESSION_START") == 0) {
    if (pass == 1) {
      NSDL4_MISC(NULL, NULL, "Pass 1: kw_set_g_rbu_clean_up_prof_on_session_start()");
      if (kw_set_g_rbu_clean_up_prof_on_session_start(buf, group_default_settings, err_msg, 0) != 0)
      {
        NS_EXIT(-1, "%s", err_msg);
      }
    }  
    if (pass == 2) {
      NSDL4_MISC(NULL, NULL, "Pass 2: kw_set_g_rbu_prof_cleanup_on_session()");
      if (kw_set_g_rbu_clean_up_prof_on_session_start(buf, &runProfTable[grp_idx].gset, err_msg, 0) != 0)
      { 
        NS_EXIT(-1, "%s", err_msg);
      }
    }
  } else if (strcasecmp(keyword, "G_RBU_HAR_SETTING") == 0) {
    if (pass == 1) {
      NSDL4_RBU(NULL, NULL, "Pass 1: kw_set_g_rbu_har_setting()");
      if (kw_set_g_rbu_har_setting(buf, group_default_settings, err_msg, 0) != 0) {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
    if (pass == 2) {
      NSDL4_RBU(NULL, NULL, "Pass 2: kw_set_g_rbu_har_setting()");
      if (kw_set_g_rbu_har_setting(buf, &runProfTable[grp_idx].gset, err_msg, 0) != 0) {
        NS_EXIT(-1, "%s", err_msg);
      }
    } 
  } else if (strcasecmp(keyword, "G_RBU_CACHE_DOMAIN") == 0) {
    if (pass == 1) {
      NSDL4_RBU(NULL, NULL, "Pass 1: kw_set_g_rbu_cache_domain()");
      if (kw_set_g_rbu_cache_domain(buf, group_default_settings, err_msg, 0) != 0) {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
    if (pass == 2) {
      NSDL4_RBU(NULL, NULL, "Pass 2: kw_set_g_rbu_cache_domain()");
      if (kw_set_g_rbu_cache_domain(buf, &runProfTable[grp_idx].gset, err_msg, 0) != 0) {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
  } else if (strcasecmp(keyword, "G_RBU_ADD_ND_FPI") == 0) {
    if (pass == 1) {
      NSDL4_RBU(NULL, NULL, "Pass 1: kw_set_g_rbu_add_nd_fpi()");
      if (kw_set_g_rbu_add_nd_fpi(buf, group_default_settings, err_msg, 0) != 0) {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
    if (pass == 2) {
      NSDL4_RBU(NULL, NULL, "Pass 2: kw_set_g_rbu_add_nd_fpi()");
      if (kw_set_g_rbu_add_nd_fpi(buf, &runProfTable[grp_idx].gset, err_msg, 0) != 0) {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
  } else if (strcasecmp(keyword, "G_RBU_ALERT_SETTING") == 0) {
    if (pass == 1) {
      NSDL4_RBU(NULL, NULL, "Pass 1: kw_set_g_rbu_alert_setting()");
      if (kw_set_g_rbu_alert_setting(buf, group_default_settings, err_msg, 0) != 0) {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
    if (pass == 2) {
      NSDL4_RBU(NULL, NULL, "Pass 2: kw_set_g_rbu_alert_setting()");
      if (kw_set_g_rbu_alert_setting(buf, &runProfTable[grp_idx].gset, err_msg, 0) != 0) {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
  } else if (strcasecmp(keyword, "G_RBU_HAR_TIMEOUT") == 0) {
    if (pass == 1) {
      NSDL4_RBU(NULL, NULL, "Pass 1: kw_set_g_rbu_har_timeout()");
      if (kw_set_g_rbu_har_timeout(buf, group_default_settings, err_msg, 0) != 0) {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
    if (pass == 2) {
      NSDL4_RBU(NULL, NULL, "Pass 2: kw_set_g_rbu_har_timeout()");
      if (kw_set_g_rbu_har_timeout(buf, &runProfTable[grp_idx].gset, err_msg, 0) != 0) {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
  } else if (strcasecmp(keyword, "G_RBU_WAITTIME_AFTER_ONLOAD") == 0) {
    if (pass == 1) {
      NSDL4_RBU(NULL, NULL, "Pass 1: kw_set_g_rbu_waittime_after_onload()");
      if (kw_set_g_rbu_waittime_after_onload(buf, group_default_settings, err_msg, 0) != 0) {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
    if (pass == 2) {
      NSDL4_RBU(NULL, NULL, "Pass 2: kw_set_g_rbu_waittime_after_onload()");
      if (kw_set_g_rbu_waittime_after_onload(buf, &runProfTable[grp_idx].gset, err_msg, 0) != 0) {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
  } else if (strcasecmp(keyword, "G_RBU_ENABLE_AUTO_SELECTOR") == 0) {
    if (pass == 1) {
      NSDL4_RBU(NULL, NULL, "Pass 1: kw_set_g_rbu_enable_auto_selector()");
      if (kw_set_g_rbu_enable_auto_selector(buf, group_default_settings, err_msg) != 0) {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
    if (pass == 2) {
      NSDL4_RBU(NULL, NULL, "Pass 2: kw_set_g_rbu_enable_auto_selector()");
      if (kw_set_g_rbu_enable_auto_selector(buf, &runProfTable[grp_idx].gset, err_msg) != 0) {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
  } else if (strcasecmp(keyword, "G_RBU_CAPTURE_PERFORMANCE_TRACE") == 0) {
    if (pass == 1) {
      NSDL4_RBU(NULL, NULL, "Pass 1: kw_set_g_rbu_capture_performance_trace()");
      kw_set_g_rbu_capture_performance_trace(buf, group_default_settings, err_msg, 0);
    }
    if (pass == 2) {
      NSDL4_RBU(NULL, NULL, "Pass 2: kw_set_g_rbu_capture_performance_trace()");
      kw_set_g_rbu_capture_performance_trace(buf, &runProfTable[grp_idx].gset, err_msg, 0);
    }
  } else if (strcasecmp(keyword, "G_RBU_SELECTOR_MAPPING_PROFILE") == 0) {
    if (pass == 1) {
      NSDL4_RBU(NULL, NULL, "Pass 1: kw_set_g_rbu_selector_mapping_profile()");
      if (kw_set_g_rbu_selector_mapping_profile(buf, group_default_settings, err_msg) != 0) {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
    if (pass == 2) {
      NSDL4_RBU(NULL, NULL, "Pass 2: kw_set_g_rbu_selector_mapping_profile()");
      if (kw_set_g_rbu_selector_mapping_profile(buf, &runProfTable[grp_idx].gset, err_msg) != 0) {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
  } else if (strcasecmp(keyword, "G_RBU_THROTTLING_SETTING") == 0) {
    if (pass == 1) {
      NSDL4_RBU(NULL, NULL, "Pass 1: kw_set_g_rbu_throttling_setting()");
      kw_set_g_rbu_throttling_setting(buf, group_default_settings, err_msg, 0);
    }
    if (pass == 2) {
      NSDL4_RBU(NULL, NULL, "Pass 2: kw_set_g_rbu_throttling_setting()");
      kw_set_g_rbu_throttling_setting(buf, &runProfTable[grp_idx].gset, err_msg, 0);
    }
  } else if (strcasecmp(keyword, "G_RBU_CPU_THROTTLING") == 0) {
    if (pass == 1) {
      NSDL4_RBU(NULL, NULL, "Pass 1: kw_set_g_rbu_cpu_throttling_setting()");
      kw_set_g_rbu_cpu_throttling_setting(buf, group_default_settings, err_msg, 0);
    }
    if (pass == 2) {
      NSDL4_RBU(NULL, NULL, "Pass 2: kw_set_g_rbu_cpu_throttling_setting()");
      kw_set_g_rbu_cpu_throttling_setting(buf, &runProfTable[grp_idx].gset, err_msg, 0);
    }
  } else if (strcasecmp(keyword, "G_RBU_LIGHTHOUSE_SETTING") == 0) {
    if (pass == 1) {
      NSDL4_RBU(NULL, NULL, "Pass 1: kw_set_g_rbu_lighthouse_setting()");
      if (kw_set_g_rbu_lighthouse_setting(buf, group_default_settings, err_msg) != 0) {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
    if (pass == 2) {
      NSDL4_RBU(NULL, NULL, "Pass 2: kw_set_g_rbu_lighthouse_setting()");
      if (kw_set_g_rbu_lighthouse_setting(buf, &runProfTable[grp_idx].gset, err_msg) != 0) {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
  } else if (strcasecmp(keyword, "G_IGNORE_HASH_IN_URL") == 0) {
    // G_IGNORE_HASH_IN_URL <group-name> <mode>
    if (pass == 1) {
      kw_set_g_ignore_hash(buf, group_default_settings, err_msg);
    } 
    if (pass == 2) {
      kw_set_g_ignore_hash(buf, &runProfTable[grp_idx].gset, err_msg);
    }
    /* We need to parse it before script parsing */
    /*if (pass == 2) {
      if ( kw_set_g_rbu(buf, &runProfTable[grp_idx].gset.enable_rbu, err_msg, 0) != 0)
        exit(-1);
    }*/
  } else if (strcasecmp(keyword, "G_ENABLE_DT") == 0) {
    if (pass == 1)
      kw_set_g_enable_dt(buf, group_default_settings, err_msg, 0);
    if (pass == 2)
      kw_set_g_enable_dt(buf, &runProfTable[grp_idx].gset, err_msg, 0);
  } else if (strcasecmp(keyword, "G_HTTP_MODE") == 0) {
    if (pass == 1)
      kw_set_g_http_mode(buf, &group_default_settings->http_settings.http_mode, err_msg, 0);
    if (pass == 2)
     kw_set_g_http_mode(buf, &runProfTable[grp_idx].gset.http_settings.http_mode, err_msg, 0); 
  } else if (strcasecmp(keyword, "G_HTTP2_SETTINGS") == 0) {   /*bug 70480: optimized kw_set_g_http2_settings*/
    if (pass == 1)
      kw_set_g_http2_settings(buf, group_default_settings,err_msg, 0);
    if (pass == 2)
      kw_set_g_http2_settings(buf, &runProfTable[grp_idx].gset,err_msg, 0);
  } else if (strcasecmp(keyword, "G_RAMP_DOWN_METHOD") == 0) {
   if (pass == 1) {
     kw_set_ramp_down_method(buf, group_default_settings, err_msg, 0);
   }
   if(pass ==2){
     kw_set_ramp_down_method(buf, &runProfTable[grp_idx].gset, err_msg, 0);
   }
  }else if (strcasecmp(keyword, "G_RBU_USER_AGENT") == 0) {
    if (pass == 1) {
      NSDL4_MISC(NULL, NULL, "Pass 1: kw_set_rbu_user_agent()");
      if (kw_set_rbu_user_agent(buf, group_default_settings, err_msg, 0) != 0)
      {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
    if (pass == 2) {
      NSDL4_MISC(NULL, NULL, "Pass 2: kw_set_rbu_user_agent()");
      if (kw_set_rbu_user_agent(buf, &runProfTable[grp_idx].gset, err_msg, 0) != 0)
      {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
  }else if (strcasecmp(keyword, "G_RBU_ENABLE_TTI_MATRIX") == 0) {
    if (pass == 1) {
      NSDL4_MISC(NULL, NULL, "Pass 1: kw_set_tti()");
      kw_set_tti(buf, group_default_settings, err_msg, 0);
    }
    if (pass == 2) {
      NSDL4_MISC(NULL, NULL, "Pass 2: kw_set_tti()");
      kw_set_tti(buf, &runProfTable[grp_idx].gset, err_msg, 0);
    }
  }else if (strcasecmp(keyword, "G_RBU_SCREEN_SIZE_SIM") == 0) {
   if (pass == 1) {
      NSDL4_MISC(NULL, NULL, "Pass 1: kw_set_rbu_screen_size_sim()");
      if (kw_set_rbu_screen_size_sim(buf, group_default_settings, err_msg, 0) != 0)
      {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
    if (pass == 2) {
      NSDL4_MISC(NULL, NULL, "Pass 2: kw_set_rbu_screen_size_sim()");
      if (kw_set_rbu_screen_size_sim(buf, &runProfTable[grp_idx].gset, err_msg, 0) != 0)
      {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
  }else if (strcasecmp(keyword, "G_RBU_SETTINGS") == 0) {
   if (pass == 1) {
      NSDL4_MISC(NULL, NULL, "Pass 1: kw_set_rbu_settings_parameter()");
      kw_set_rbu_settings_parameter(buf, group_default_settings, err_msg, 0);
    }
    if (pass == 2) {
      NSDL4_MISC(NULL, NULL, "Pass 2: kw_set_rbu_settings_parameter()");
      kw_set_rbu_settings_parameter(buf, &runProfTable[grp_idx].gset, err_msg, 0);
    }
  } else if (strcasecmp(keyword, "G_SHOW_RUNTIME_RUNLOGIC_PROGRESS") == 0) {
    if (pass == 1) {
      NSDL4_MISC(NULL, NULL, "Pass 1: kw_set_g_show_vuser_flow()");
      kw_set_g_show_vuser_flow(buf, group_default_settings, err_msg, 0);
    }
  }else if (strcasecmp(keyword, "G_RBU_BLOCK_URL_LIST") == 0) {
   if (pass == 1) {
      NSDL4_MISC(NULL, NULL, "Pass 1: kw_set_rbu_block_url_list()");
      if (kw_set_g_rbu_block_url_list(buf, group_default_settings, err_msg, 0) != 0)
      {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
    if (pass == 2) {
      NSDL4_MISC(NULL, NULL, "Pass 2: kw_set_g_rbu_bock_url_list()");
      if (kw_set_g_rbu_block_url_list(buf, &runProfTable[grp_idx].gset, err_msg, 0) != 0)
      {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
  }else if (strcasecmp(keyword, "G_RBU_DOMAIN_IGNORE_LIST") == 0) {
   if (pass == 1) {
      NSDL4_MISC(NULL, NULL, "Pass 1: kw_set_rbu_domain_ignore_list()");
      if (kw_set_g_rbu_domain_ignore_list(buf, group_default_settings, err_msg, 0) != 0)
      {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
    if (pass == 2) {
      NSDL4_MISC(NULL, NULL, "Pass 2: kw_set_g_rbu_domain_ignore_list()");
      if (kw_set_g_rbu_domain_ignore_list(buf, &runProfTable[grp_idx].gset, err_msg, 0) != 0)
      {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
  }else if (strcasecmp(keyword, "G_RBU_ACCESS_LOG") == 0) {
   if (pass == 1) {
      NSDL4_MISC(NULL, NULL, "Pass 1: kw_set_g_rbu_access_log()");
      if (kw_set_g_rbu_access_log(buf, group_default_settings, err_msg, 0) != 0)
      {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
    if (pass == 2) {
      NSDL4_MISC(NULL, NULL, "Pass 2: kw_set_g_rbu_access_log()");
      if (kw_set_g_rbu_access_log(buf, &runProfTable[grp_idx].gset, err_msg, 0) != 0)
      {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
  } else if (strcasecmp(keyword, "G_SCRIPT_PARAM") == 0) {
   if (pass == 1) {
      NSDL4_MISC(NULL, NULL, "Pass 1: set_script_param()");
      if (kw_set_g_script_param(buf, err_msg) != 0)
      {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
    if (pass == 2) {
      NSDL4_MISC(NULL, NULL, "Pass 2: set_script_param()");
      if (kw_set_g_script_param(buf, err_msg) != 0)
      {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
  } else if (strcasecmp(keyword, "G_RBU_RM_PROF_SUB_DIR") == 0) {
    if (pass == 1) {
      NSDL4_MISC(NULL, NULL, "Pass 1: kw_set_g_rbu_rm_prof_sub_dir()");
      if (kw_set_g_rbu_rm_prof_sub_dir(buf, group_default_settings, err_msg, 0) != 0)
      {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
    if (pass == 2) {
      NSDL4_MISC(NULL, NULL, "Pass 2: kw_set_g_rbu_rm_prof_sub_dir()");
      if (kw_set_g_rbu_rm_prof_sub_dir(buf, &runProfTable[grp_idx].gset, err_msg, 0) != 0)
      {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
  } else if (strcasecmp(keyword, "G_RBU_RELOAD_HAR") == 0){
      if (pass == 1){
        NSDL4_MISC(NULL, NULL, "Pass 1: kw_set_g_rbu_reload_har()");
    	if (kw_set_g_rbu_reload_har(buf, group_default_settings, err_msg) != 0){
          NS_EXIT(-1, "%s", err_msg);
        }
      }  
      if (pass == 2){
        NSDL4_MISC(NULL, NULL, "Pass 2: kw_set_g_rbu_reload_har()");
        if (kw_set_g_rbu_reload_har(buf, &runProfTable[grp_idx].gset, err_msg) != 0){
          NS_EXIT(-1, "%s", err_msg);
        } 
      }
  } else if (strcasecmp(keyword, "G_RBU_WAIT_UNTIL") == 0){
      if (pass == 1){
        NSDL4_MISC(NULL, NULL, "Pass 1: kw_set_g_rbu_wait_until()");
       kw_set_g_rbu_wait_until(buf, group_default_settings, err_msg);
      }
      if (pass == 2){
        NSDL4_MISC(NULL, NULL, "Pass 2: kw_set_g_rbu_wait_until()");
        kw_set_g_rbu_wait_until(buf, &runProfTable[grp_idx].gset, err_msg);
      }
  } else if (strcasecmp(keyword, "G_M3U8_SETTING") == 0) {
    if (pass == 1) {
      NSDL4_MISC(NULL, NULL, "Pass 1: kw_set_g_m3u8_enable()");
      kw_set_g_m3u8_enable(buf, group_default_settings, err_msg, 0);
    } 
    if (pass == 2) {
      NSDL4_MISC(NULL, NULL, "Pass 2: kw_set_g_m3u8_enable()");
      kw_set_g_m3u8_enable(buf, &runProfTable[grp_idx].gset, err_msg, 0);
    }
  } else if (strcasecmp(keyword, "G_RTE_SETTINGS") == 0) {
    if (pass == 1) {
      NSDL4_MISC(NULL, NULL, "Pass 1: kw_set_g_rte_settings()");
      kw_set_g_rte_settings(buf, group_default_settings, err_msg, 0);
      //Pass 2 will handle before script parse because we need to set display
      //and need to access all group at the time of .rte.spec parsing
    }
   } else if (strcasecmp(keyword,"G_IP_VERSION_MODE") == 0){ //This keyword is used for get the mode of IP version i.e 0,1,2
    if (pass == 1) {
      NSDL4_MISC(NULL, NULL, "Pass 1: kw_set_ip_version_mode()");
      kw_set_ip_version_mode(buf, group_default_settings, err_msg, 0);
    }
    if (pass == 2) {
      NSDL4_MISC(NULL, NULL, "Pass 2: kw_set_ip_version_mode()");
      kw_set_ip_version_mode(buf, &runProfTable[grp_idx].gset, err_msg, 0);
    }
  } else if (strcasecmp(keyword, "G_USE_SRC_IP") == 0) {
    if (pass == 1) {
      NSDL4_MISC(NULL, NULL, "Pass 1: kw_set_use_src_ip()");
      kw_set_use_src_ip(buf, group_default_settings, err_msg, 0);
    }
    if (pass == 2) {
      NSDL4_MISC(NULL, NULL, "Pass 2: kw_set_use_src_ip()");
      kw_set_use_src_ip(buf, &runProfTable[grp_idx].gset, err_msg, 0);
    }
  } else if (strcasecmp(keyword, "G_SRC_IP_LIST") == 0) { //Can be repeated
    if (pass == 1) {
      NSDL4_MISC(NULL, NULL, "Pass 1: kw_set_src_ip_list()");
      if(kw_set_src_ip_list(buf, group_default_settings, err_msg) != 0) 
      {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
    if (pass == 2) {
      NSDL4_MISC(NULL, NULL, "Pass 2: kw_set_src_ip_list()");
      if(kw_set_src_ip_list(buf, &runProfTable[grp_idx].gset, err_msg) != 0) 
      {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
  } else if (strcasecmp(keyword, "G_USE_SAME_NETID_SRC") == 0) { 
    if (pass == 1) {
      NSDL4_MISC(NULL, NULL, "Pass 1: kw_set_use_same_netid_src()");
      kw_set_use_same_netid_src(buf, &group_default_settings->use_same_netid_src, err_msg, 0);
    }
    if (pass == 2) {
      NSDL4_MISC(NULL, NULL, "Pass 2: kw_set_use_same_netid_src()");
      kw_set_use_same_netid_src(buf, &runProfTable[grp_idx].gset.use_same_netid_src, err_msg, 0);
    }
  } else if (strcasecmp(keyword, "G_SERVER_HOST") == 0) {
    if (pass == 1) {
      NSDL4_MISC(NULL, NULL, "Pass 1: kw_set_g_server_host()");
      kw_set_g_server_host(buf, group_default_settings, -1, err_msg, 0);
    }
    if (pass == 2) {
      NSDL4_MISC(NULL, NULL, "Pass 2: kw_set_g_server_host()");
      kw_set_g_server_host(buf, &runProfTable[grp_idx].gset, grp_idx, err_msg, 0);
    }
  } else if(strcasecmp(keyword, "G_STATIC_HOST") == 0) {
    if (pass == 1) {
      kw_set_g_static_host(buf, group_default_settings, -1, err_msg, 0);
   }
   if (pass == 2) {
     kw_set_g_static_host(buf, &runProfTable[grp_idx].gset, grp_idx, err_msg, 0);
   }
 } else if (strcasecmp(keyword, "G_JMETER_JVM_SETTINGS") == 0) {
    if (pass == 1) {
      NSDL4_MISC(NULL, NULL, "Pass 1: kw_set_jmeter_jvm_settings()");
      kw_set_jmeter_jvm_settings(buf, group_default_settings, err_msg, 0);
    }
    if (pass == 2) {
      NSDL4_MISC(NULL, NULL, "Pass 2: kw_set_jmeter_jvm_settings()");
      kw_set_jmeter_jvm_settings(buf, &runProfTable[grp_idx].gset, err_msg, 0);
    }
  } else if (strcasecmp(keyword, "G_JMETER_ADD_ARGS") == 0) {
    if (pass == 1) {
      NSDL4_MISC(NULL, NULL, "Pass 1: kw_set_jmeter_additional_settings()");
      kw_set_jmeter_additional_settings(buf, group_default_settings, err_msg, 0);
    }
    if (pass == 2) {
      NSDL4_MISC(NULL, NULL, "Pass 2: kw_set_jmeter_additional_settings()");
      kw_set_jmeter_additional_settings(buf, &runProfTable[grp_idx].gset, err_msg, 0);
    }
  } else if (strcasecmp(keyword, "G_JMETER_SCHEDULE_SETTINGS") == 0) {
    if (pass == 1) {
      NSDL4_MISC(NULL, NULL, "Pass 1: kw_set_jmeter_schedule_settings()");
      kw_set_jmeter_schedule_settings(buf, group_default_settings, err_msg, 0);
    }
    if (pass == 2) {
      NSDL4_MISC(NULL, NULL, "Pass 2: kw_set_jmeter_schedule_settings()");
      kw_set_jmeter_schedule_settings(buf, &runProfTable[grp_idx].gset, err_msg, 0);
    }
  } else if (strncasecmp(keyword, "G_HTTP_BODY_CHECKSUM_HEADER",strlen("G_HTTP_BODY_CHECKSUM_HEADER")) == 0) {
    if (pass == 1) {
      NSDL4_MISC(NULL, NULL, "Pass 1: kw_g_http_body_chksum_hdr()");
      if (kw_g_http_body_chksum_hdr(buf, group_default_settings, err_msg) != 0)
      {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
    if (pass == 2) {
      NSDL4_MISC(NULL, NULL, "Pass 2: kw_g_http_body_chksum_hdr()");
      if (kw_g_http_body_chksum_hdr(buf, &runProfTable[grp_idx].gset, err_msg) != 0)
      {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
  } else if (strcasecmp(keyword, "G_SSL_CERT_FILE_PATH") == 0) {
    if (pass == 1) {
      NSDL4_MISC(NULL, NULL, "Pass 1: kw_g_set_ssl_cert_file()");
      kw_g_set_ssl_cert_file(buf, group_default_settings, err_msg, 0, pass);
    }
  } else if (strcasecmp(keyword, "G_SSL_KEY_FILE_PATH") == 0) {
    // G_RBU  <group-name> <mode>
    if (pass == 1) {
      NSDL4_MISC(NULL, NULL, "Pass 1: kw_g_set_ssl_key_file()");
      kw_set_ssl_key_file(buf, group_default_settings, err_msg, 0, pass);
    }
  } else if (strcasecmp(keyword, "G_TLS_VERSION") == 0) {
    if (pass == 1) {
      NSDL4_MISC(NULL, NULL, "Pass 1: kw_set_tls_version()");
      kw_set_tls_version(buf, group_default_settings, err_msg, 0);
    }
    if (pass == 2) {
      NSDL4_MISC(NULL, NULL, "Pass 2: kw_set_tls_version()");
      kw_set_tls_version(buf, &runProfTable[grp_idx].gset, err_msg, 0);
    }
  } else if (strcasecmp(keyword, "G_ENABLE_POST_HANDSHAKE_AUTH") == 0) {
    if (pass == 1) {
      NSDL4_MISC(NULL, NULL, "Pass 1: kw_enable_post_handshake_auth()");
      if (kw_enable_post_handshake_auth(buf, group_default_settings, err_msg) != 0)
      {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
    if (pass == 2) {
      NSDL4_MISC(NULL, NULL, "Pass 2: kw_enable_post_handshake_auth()");
      if (kw_enable_post_handshake_auth(buf, &runProfTable[grp_idx].gset, err_msg) != 0)
      {
        NS_EXIT(-1, "%s", err_msg);
      }
    }
  } else if (strcasecmp(keyword, "G_SSL_RENEGOTIATION") == 0) {
    if (pass == 1) {
      NSDL4_MISC(NULL, NULL, "Pass 1: start_ssl_renegotiation()");
      start_ssl_renegotiation(buf, group_default_settings, err_msg, 0);
    }
    if (pass == 2) {
      NSDL4_MISC(NULL, NULL, "Pass 2: start_ssl_renegotiation()");
      start_ssl_renegotiation(buf, &runProfTable[grp_idx].gset, err_msg, 0);
    }
  } else if (strncasecmp(keyword, "G_BODY_ENCRYPTION",strlen("G_BODY_ENCRYPTION")) == 0) {
    if (pass == 1) {
      NSDL4_MISC(NULL, NULL, "Pass 1: kw_g_body_encryption()");
      kw_g_body_encryption(buf, group_default_settings, err_msg, 0);
    }
    if (pass == 2) {
      NSDL4_MISC(NULL, NULL, "Pass 2: kw_g_body_encryption()");
      kw_g_body_encryption(buf, &runProfTable[grp_idx].gset, err_msg, 0);
    }
  } else if (strcasecmp(keyword, "G_ENABLE_CORRELATION_ID") == 0) {
    if (pass == 1)
      kw_set_g_enable_corr_id(buf, group_default_settings, err_msg, 0);
    if (pass == 2)
      kw_set_g_enable_corr_id(buf, &runProfTable[grp_idx].gset, err_msg, 0);
  } else if (strcasecmp(keyword, "G_DATADIR") == 0) {
    if (pass == 1) 
    {
      NSDL4_MISC(NULL, NULL, "Pass 1: kw_set_g_datadir()");
      kw_set_g_datadir(buf, group_default_settings, err_msg, 0);
    }
  } else if (strcasecmp(keyword, "G_SCRIPT_RUNLOGIC") == 0) {
      if (pass == 1)
        kw_set_g_script_runlogic(buf, err_msg, 0);
  }


 
/*  else { */
/*     NSTL1_OUT(NULL, NULL,  "Unknown Keyword %s\n", keyword); */
/*     exit(-1); */
/*   } */
  
  return 0;                     /* Consumed. */
}


  /* 
  * rewind and parse again for the following group based keywords:
  *   G_GET_NO_INLINED_OBJ 
  *   G_SCRIPT_MODE
  * we're doing this here as we need it before the 2nd pass for group based
  * keywords - the 2nd pass happens only after parsing scripts -but we need this
  * info before parsing scripts
  * we check only for the group specific case - the case where the keyword is
  * qualified by ALL (applies to all groups) is done earlier in pass 1
  */
static int parse_kw_needed_before_script_parse(FILE *conf_file)
{
  char buf[MAX_LINE_LENGTH];
  char grp[MAX_LINE_LENGTH];
  char text[MAX_LINE_LENGTH];
  char keyword[MAX_LINE_LENGTH];
  int num, grp_idx;
  char err_msg[MAX_DATA_LINE_LENGTH];
  
  memset(keyword, 0, sizeof(keyword));
  memset(grp, 0, sizeof(grp));
  memset(text, 0, sizeof(text));

  rewind(conf_file);
  NSDL4_MISC(NULL, NULL, "Method called");
  while (fgets(buf, MAX_LINE_LENGTH, conf_file) != NULL) {
    num = sscanf(buf, "%s %s %s", keyword, grp, text);

    //Narendra: NJVM_SYSTEM_CLASS_PATH and NJVM_CLASS_PATH are needed before script parse so we are parsing these kw here.
    if(num < 2) {
      continue;
    }

     //These two kw are required in script parsing.
		if (strcasecmp(keyword, "NJVM_SYSTEM_CLASS_PATH") == 0) {
			kw_set_njvm_system_class_path(buf, err_msg, 0);
      continue;
		} else if (strcasecmp(keyword, "NJVM_CLASS_PATH") == 0) {
			kw_set_njvm_class_path(buf, err_msg, 0);
      continue;
    } else if (strcasecmp(keyword, "AUTO_COOKIE") == 0) {  //Narendra: moved here because it was needed in script parsing.
      kw_set_auto_cookie(buf, err_msg, 0);
      continue;
    } else if (strcasecmp(keyword, "AMF_SEGMENT_MODE") == 0) {
      kw_set_amf_seg_mode(buf);
      continue;
    }
    if (strcasecmp(keyword, "DISABLE_SCRIPT_VALIDATION") == 0)
    {
      kw_set_disable_script_validation(buf); 
      continue;
    }
    if (strcasecmp(keyword, "WHITELIST_HEADER") == 0) {
      kw_set_whitelist_header(buf, 0);
      continue;
    }
    if (num < 3) {
      continue;
    }
    if(strcasecmp(keyword, "G_SSL_CERT_FILE_PATH") == 0)
    {
      //kw_set_ssl_cert_file(keyword, text); 

      if (strcasecmp(grp, "ALL") != 0) {
        grp_idx = find_group_idx(grp);
        NSDL4_MISC(NULL, NULL, "G_SSL_CERT_FILE_PATH (pass 2): grp_idx %d",grp_idx);
        if (grp_idx == -1)
        {
          continue;  //In this case skip it and parse next keyword if any
        }
        if(kw_g_set_ssl_cert_file(buf, &runProfTable[grp_idx].gset, err_msg, 0, 2) != 0)
        {
          NS_EXIT(-1, "%s", err_msg);
        }
      }
    }
    if(strcasecmp(keyword, "G_SSL_KEY_FILE_PATH") == 0)
    {
      if (strcasecmp(grp, "ALL") != 0) {
        grp_idx = find_group_idx(grp);
        NSDL4_MISC(NULL, NULL, "G_SSL_KEY_FILE_PATH (pass 2): grp_idx %d",grp_idx);
        if (grp_idx == -1)
        {
          continue;  //In this case skip it and parse next keyword if any
        }
        if(kw_set_ssl_key_file(buf, &runProfTable[grp_idx].gset, err_msg, 0, 2) != 0)
        {
          NS_EXIT(-1, "%s", err_msg);
        }
      }
    }
    if (strcasecmp(keyword, "G_GET_NO_INLINED_OBJ") == 0) {
      NSDL4_MISC(NULL, NULL, "pass2 keyword %s grp %s text %s",keyword, grp, text);
      if (strcasecmp(grp, "ALL") != 0) {  // looking only for group specific setting
        grp_idx = find_group_idx(grp);
        if(kw_set_get_no_inlined_obj(buf, &runProfTable[grp_idx].gset.get_no_inlined_obj, err_msg, 0) != 0)
          NS_EXIT(-1, "%s", err_msg);
      }
    }
    if (strcasecmp(keyword, "G_SCRIPT_MODE") == 0) {
      NSDL4_MISC(NULL, NULL, "pass2 keyword %s grp %s text %s",keyword, grp, text);
      if (strcasecmp(grp, "ALL") != 0) {  // looking only for group specific setting
        grp_idx = find_group_idx(grp);
        kw_set_g_script_mode(buf, &runProfTable[grp_idx].gset, err_msg, 0); // method display error and exit, so no need to check
      }
    }
    if (strcasecmp(keyword, "G_DISABLE_HOST_HEADER") == 0) {
      NSDL4_MISC(NULL, NULL, "pass2 keyword %s grp %s text %s",keyword, grp, text);
      if (strcasecmp(grp, "ALL") != 0) {  // looking only for group specific setting
        grp_idx = find_group_idx(grp);
        kw_set_disable_host_header(buf, &runProfTable[grp_idx].gset.disable_headers, err_msg, 0);
      }
    }
    if (strcasecmp(keyword, "G_DISABLE_UA_HEADER") == 0) {
      NSDL4_MISC(NULL, NULL, "pass2 keyword %s grp %s text %s",keyword, grp, text);
      if (strcasecmp(grp, "ALL") != 0) {  // looking only for group specific setting
        grp_idx = find_group_idx(grp);
        kw_set_disable_ua_header(buf, &runProfTable[grp_idx].gset.disable_headers, err_msg, 0);
      }
    }
    if (strcasecmp(keyword, "G_DISABLE_ACCEPT_HEADER") == 0) {
      NSDL4_MISC(NULL, NULL, "pass2 keyword %s grp %s text %s",keyword, grp, text);
      if (strcasecmp(grp, "ALL") != 0) {  // looking only for group specific setting
        grp_idx = find_group_idx(grp);
        kw_set_disable_accept_header(buf, &runProfTable[grp_idx].gset.disable_headers, err_msg, 0);
      }
    }
    if (strcasecmp(keyword, "G_DISABLE_ACCEPT_ENC_HEADER") == 0) {
      NSDL4_MISC(NULL, NULL, "pass2 keyword %s grp %s text %s",keyword, grp, text);
      if (strcasecmp(grp, "ALL") != 0) {  // looking only for group specific setting
        grp_idx = find_group_idx(grp);
        kw_set_disable_accept_enc_header(buf, &runProfTable[grp_idx].gset.disable_headers, err_msg, 0);
      }
    } 
    if (strcasecmp(keyword, "G_DISABLE_KA_HEADER") == 0) {
      NSDL4_MISC(NULL, NULL, "pass2 keyword %s grp %s text %s",keyword, grp, text);
      if (strcasecmp(grp, "ALL") != 0) {  // looking only for group specific setting
        grp_idx = find_group_idx(grp);
        kw_set_disable_ka_header(buf, &runProfTable[grp_idx].gset.disable_headers, err_msg, 0);
      }
    }
    if (strcasecmp(keyword, "G_DISABLE_CONNECTION_HEADER") == 0) {
      NSDL4_MISC(NULL, NULL, "pass2 keyword %s grp %s text %s",keyword, grp, text);
      if (strcasecmp(grp, "ALL") != 0) {  // looking only for group specific setting
        grp_idx = find_group_idx(grp);
        kw_set_disable_connection_header(buf, &runProfTable[grp_idx].gset.disable_headers, err_msg, 0 );
      }
    }
    if (strcasecmp(keyword, "G_DISABLE_ALL_HEADER") == 0) {
      NSDL4_MISC(NULL, NULL, "pass2 keyword %s grp %s text %s",keyword, grp, text);
      if (strcasecmp(grp, "ALL") != 0) {  // looking only for group specific setting
        grp_idx = find_group_idx(grp);
        kw_set_disable_all_header(buf, &runProfTable[grp_idx].gset.disable_headers, err_msg, 0);
      }
    }
    if (strcasecmp(keyword, "G_ENABLE_PIPELINING") == 0) {
      if (strcasecmp(grp, "ALL") != 0) {  // looking only for group specific setting
        grp_idx = find_group_idx(grp);
        NSDL4_MISC(NULL, NULL, "grp_idx %d",grp_idx);
        if (grp_idx == -1)
        {
          NS_EXIT(-1, "Error: Invalid group name given for scenario group name for keyword = %s", keyword);
        }
        kw_set_g_enable_pipelining(buf, &runProfTable[grp_idx].gset, err_msg);
      }
    }
    if (strcasecmp(keyword, "SYNC_POINT") == 0) {
      kw_set_sync_point(buf, err_msg, 0);
    }
    if (strcasecmp(keyword, "SYNC_POINT_TIME_OUT") == 0) {
      kw_set_sync_point_time_out(buf, 0, err_msg); 
    }

    // Manish (15 Jan 2014): G_RBU  <group-name> <mode>
    // Handling pass 2 here 
    /* We need to parse it before script parsing because - 
       (1) We need to parse ENABLE_NS_FIREFOX keyword if and only if at least one group has RBU script 
           but since normaly G_RBU for pass 2 parse after parsing keyword ENABLE_NE_FIREFOX hence in 
           normal case we have no way to know there exist RBU script.
           To resolve this issue we set set global_settings->protocol_enabled gere for pass 

       (2) In case of RBU we are supporting Automatic file parameter since All parametrs parse before 
           G_RBU for pass 2 and hence again we have no way to know at that time that, Is script RBU? */
    if (strcasecmp(keyword, "G_RBU") == 0)
    {
      if (strcasecmp(grp, "ALL") != 0) {
        grp_idx = find_group_idx(grp);
        NSDL4_MISC(NULL, NULL, "G_RBU (pass 2): grp_idx %d",grp_idx);
        if (grp_idx == -1)
        {
          continue;  //In this case skip it and parse next keyword if any
        }
        if(kw_set_g_rbu(buf, &runProfTable[grp_idx].gset, err_msg, 0) != 0)
        {
          NS_EXIT(-1, "%s", err_msg);
        }
      }
    }
    //Atul 25-May-2018 G_RTE_SETTINGS
    //Pass 2 will handle before script parse because we need to set display
    //and need to access all group at the time of .rte.spec parsing

    if (strcasecmp(keyword, "G_RTE_SETTINGS") == 0)
    {
      if (strcasecmp(grp, "ALL") != 0) {
        grp_idx = find_group_idx(grp);
        NSDL4_MISC(NULL, NULL, "G_RTE_SETTINGS (pass 2): grp_idx %d",grp_idx);
        if (grp_idx == -1)
        {
          continue;  //In this case skip it and parse next keyword if any
        }
        kw_set_g_rte_settings(buf, &runProfTable[grp_idx].gset, err_msg, 0);
      }
    }
    if (strcasecmp(keyword, "G_SHOW_RUNTIME_RUNLOGIC_PROGRESS") == 0)
    {
      if (strcasecmp(grp, "ALL") != 0) {
        grp_idx = find_group_idx(grp);
        NSDL4_MISC(NULL, NULL, "G_SHOW_RUNTIME_RUNLOGIC_PROGRESS (pass 2): grp_idx %d",grp_idx);
        if (grp_idx == -1)
        {
          continue;  //In this case skip it and parse next keyword if any
        }
        kw_set_g_show_vuser_flow(buf, &runProfTable[grp_idx].gset, err_msg, 0);
      }
    }
    if (strcasecmp(keyword, "G_SCRIPT_RUNLOGIC") == 0) {
      if (strcasecmp(grp, "ALL") != 0) {
        NSDL4_MISC(NULL, NULL, "G_SCRIPT_RUNLOGIC (pass 2)");
        kw_set_g_script_runlogic(buf, err_msg, 0);
      }
    }
    if (strcasecmp(keyword, "G_DATADIR") == 0) {
      if (strcasecmp(grp, "ALL") != 0) {
        grp_idx = find_group_idx(grp);
        NSDL4_MISC(NULL, NULL, "G_DATADIR (pass 2): grp_idx %d", grp_idx);
        if(grp_idx == -1)
        {
          continue; //Invalid Keyword
        }
        kw_set_g_datadir(buf, &runProfTable[grp_idx].gset, err_msg, 0);
      }
    }
  } //while
  return(0);
}


/*This is to add acs main url of acs main http url like a ADD recorded host*/
void tr069_validates_urls() {
  char tmp_url[1024];
  int request_type;
  char hostname[512];
  char request_line[1024];
  if(global_settings->tr069_acs_url[0] != '\0') {
    if(global_settings->tr069_main_acs_url[0] == '\0') {
     NS_EXIT(-1, "Error: TR069_MAIN_ACS_URL/TR069_MAIN_ACS_HTTP_URL must be given with TR069_ACS_URL\n");
    } else {
      strcpy(tmp_url, global_settings->tr069_main_acs_url);
      if (RET_PARSE_NOK == parse_url(tmp_url, "{/?#", &request_type, hostname, request_line)) {
        NS_EXIT(-1, "Error: Invalid main acs url: [%s]\n", global_settings->tr069_main_acs_url);
      }
      get_server_idx(hostname, request_type, -1);
    }
  }
}


/*static void usage_kw_set_sgrp_new_format(char *buf)
{
  if (buf != NULL)
    NSTL1_OUT(NULL, NULL,  "Error:\n%s\n", buf);
   NS_KW_PARSING_ERR(buf, 0, err_msg, SGRP_USAGE, CAV_ERR_1011306, CAV_ERR_MSG_6);
   // NS_EXIT(-1, "%s\n\tSGRP <GroupName> <GeneratorName> <ScenType> <user-profile> "
        //  "<type(0 for Script or 1 for URL)> <session-name/URL> <num-or-pct> <cluster_id>", buf);
}*/

//IO_VECTOR_SIZE <init_io_vector_size> <io_vector_delta_size> <io_vector_max_size>
int kw_set_io_vector_size(char *buf, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char init_size[MAX_DATA_LINE_LENGTH];
  char delta_size[MAX_DATA_LINE_LENGTH];
  char max_size[MAX_DATA_LINE_LENGTH];
  char *val;
  int num, ret;
  NSDL3_PARSING(NULL, NULL, "Method Called, buf = %s", buf);
  
  ret = sscanf(buf, "%s %s %s %s", keyword, init_size, delta_size, max_size);

  if(ret < 1 || ret > 4)
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, IO_VECTOR_SIZE_USAGE, CAV_ERR_1011078, CAV_ERR_MSG_1);

  if(ret > 1){ 
    val = init_size;
    CLEAR_WHITE_SPACE(val); 
    if(ns_is_numeric(val) == 0)
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, IO_VECTOR_SIZE_USAGE, CAV_ERR_1011078, CAV_ERR_MSG_2);
  
    num = atoi(val);
    if(num < 100)
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, IO_VECTOR_SIZE_USAGE, CAV_ERR_1011079, "");
    
    //io_vector_size = num; //TODO IOVEC
    io_vector_init_size = num;
  }

  if(ret > 2){    
    val = delta_size;
    CLEAR_WHITE_SPACE(val);
    num = atoi(val);
    if(num == 0)
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, IO_VECTOR_SIZE_USAGE, CAV_ERR_1011078, CAV_ERR_MSG_4);

    if(num < 0)
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, IO_VECTOR_SIZE_USAGE, CAV_ERR_1011078, CAV_ERR_MSG_8);

    if(ns_is_numeric(val) == 0)
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, IO_VECTOR_SIZE_USAGE, CAV_ERR_1011078, CAV_ERR_MSG_2);
    
    io_vector_delta_size = num;
  }

  if(ret > 3){   
    val = max_size;
    CLEAR_WHITE_SPACE(val);
    if(ns_is_numeric(val) == 0)
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, IO_VECTOR_SIZE_USAGE, CAV_ERR_1011078, CAV_ERR_MSG_2);
    
    num = atoi(val);
    io_vector_max_size = num;
    g_req_rep_io_vector.tot_size = num;
  }
  
  if(io_vector_init_size > io_vector_max_size)
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, IO_VECTOR_SIZE_USAGE, CAV_ERR_1011080, "");
  
  if((io_vector_init_size + io_vector_delta_size) > io_vector_max_size)
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, IO_VECTOR_SIZE_USAGE, CAV_ERR_1011081, "");
    
  NSDL3_PARSING(NULL, NULL, "Method Exiting, io_vector_init_size = %d io_vector_delta_size = %d io_vector_max_size = %d",
                                             io_vector_init_size, io_vector_delta_size, io_vector_max_size);

  return 0;
}

static void ns_parent_child_con_timout_usages(char *err)
{
  NSTL1_OUT(NULL, NULL,  "Error: Invalid value of PARENT_CHILD_CON_TIMEOUT keyword: %s\n", err);
  NSTL1_OUT(NULL, NULL,  "  Usage: PARENT_CHILD_CON_TIMEOUT <interval>\n");
  NSTL1_OUT(NULL, NULL,  "    Interval:\n");
  NSTL1_OUT(NULL, NULL,  "      <60> sec - default interval.\n");
  NSTL1_OUT(NULL, NULL,  "      Interval should not be less than 60.\n");
  NS_EXIT(-1, "%s\nUsage: PARENT_CHILD_CON_TIMEOUT <interval>");
}

static int kw_set_parent_child_con_timout(char *buf)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char interval_str[32 + 1];
  char tmp[MAX_DATA_LINE_LENGTH]; //This used to check if some extra field is given
  int num;
  int interval = 0;

  num = sscanf(buf, "%s %s %s", keyword, interval_str, tmp);

  NSDL2_PARSING(NULL, NULL, "Method called, buf = %s, num= %d , key=[%s], mode_str=[%s]", buf, num, keyword, interval_str);

  if(num != 2)
  {
    ns_parent_child_con_timout_usages("Invalid number of arguments");
  }

  if(ns_is_numeric(interval_str) == 0)
  {
    ns_parent_child_con_timout_usages("PARENT_CHILD_CON_TIMEOUT mode is not numeric");
  }

  interval = atoi(interval_str);
  if(interval < 60)
  {
    ns_parent_child_con_timout_usages("PARENT_CHILD_CON_TIMEOUT mode is not valid");
  }

  global_settings->parent_child_con_timeout = interval;

  NSDL2_PARSING(NULL, NULL, "global_settings->parent_child_con_timeout = %d", global_settings->parent_child_con_timeout);

  return 0;
}
/**/
static int remove_leading_zeros(char *src, char* dest, int start_idx, int upto_three_digit)
{
  int idx = start_idx;//For mantisa part it should be 0, whereas in case of mantisa it will be index in dest array
  NSDL1_PARSING(NULL, NULL, "Method called, idx = %d, upto_three_digit = %d, src = %s", idx, upto_three_digit, src);
  char *ptr = src;
  while (*ptr && upto_three_digit)
  {
    if (*ptr == '0' && !idx)//Remove all leading zero
    {
      ptr++;//Fix done for 9952
      upto_three_digit--;//Used in case of mantisa where we need to truncate upto 3 decimal places
      continue;
    }
    else
    {
      dest[idx] = *ptr;
      NSDL1_PARSING(NULL, NULL, "dest[%d] = %c", idx, dest[idx]);
      idx++;//Next idx
      upto_three_digit--;
      NSDL1_PARSING(NULL, NULL, "idx = %d, upto_three_digit = %d", idx, upto_three_digit);
    }
    ptr++;//Fix done for 9952
  }
  return(idx);
}


/*In regard to BUG 8949, 
  FSR advance scenario, SGRP G1 NA NA Internet 0 my_script 123.800
  When store session rate in a double variable (multiplying with 1000) and later typecast same into integer reported
  floating point error.
  In interger varibale value stored was 123799 and being advance scenario test terminated 
  while calculating high water mark.  
 */
int compute_sess_rate(char *sess_value)
{
  int num_field, idx, exponent_len, len;
  char *fields[2];
  char tmp_sess_str[100];//Local string which creates session rate 
  memset(tmp_sess_str, 0, 100);

  NSDL1_PARSING(NULL, NULL, "Method called, sess_value= %s", sess_value);
  num_field = get_tokens(sess_value, fields, ".", 2);
  /*If session rate provided is a whole number then return session value * 1000*/
  if (num_field != 2)
  {
    NSDL1_PARSING(NULL, NULL, "Session rate without decimal point, ret = %d", (atoi(sess_value) * SESSION_RATE_MULTIPLE));
    return((atoi(sess_value) * SESSION_RATE_MULTIPLE));
  } 
  //Remove all leading 0s from mantisa part
  idx = remove_leading_zeros(fields[0], tmp_sess_str, 0, 999);
  //Find length of exponent part
  exponent_len = strlen(fields[1]);
  //Here we support upto 3 decimal place, if exponent part length greater than three then truncate string
  len = (exponent_len < 3)?999:3;
  //Remove all leading 0s from exponent part
  remove_leading_zeros(fields[1], tmp_sess_str, idx, len);
  //In case of exponent part less than 3
  //0.5 --- 0.500(add trailing 00)
  //0.05 --- 0.050(add trainling 0)
  if (exponent_len < 3)
  {
    if (exponent_len == 1)
      strcat(tmp_sess_str, "00");
    else if (exponent_len == 2)
      strcat(tmp_sess_str, "0");
  }
  NSDL1_PARSING(NULL, NULL, "tmp_sess_str = %s\n", tmp_sess_str);
  return(atoi(tmp_sess_str));//return session rate 
}

#define MAX_JMX_FILE_NAME_LENGTH 256
void get_jmeter_script_name(char *jmx_sess_name, char *grp_proj_name, char *grp_subproj_name, char *sess_name)
{
  char jmx_fname[MAX_DATA_LINE_LENGTH + 1];
  char read_line[256 + 1]; // contains JMX_SESSION_NAME=FILENAME.JMX
  FILE *fp;
  int num_toks;//,len;
  char *field[8];
  char jmx_file_name[MAX_JMX_FILE_NAME_LENGTH  + 1];
  //int csvdatasetfound = 0;
  char filename[16 + 1]; // This variable is not used from 4.5.1 Jmeter Redesign
  //char *ptr = NULL;
  //char *tmp_ptr = NULL;
  //struct stat stat_st;

  //snprintf(sess_name_with_proj_subproj, 2048, "%s/%s/cav_jmeter", grp_proj_name, grp_subproj_name);
  global_settings->protocol_enabled |= JMETER_PROTOCOL_ENABLED;
  /*bug id: 101320: using g_ns_ta_dir instead of g_ns_wdir, avoid using hardcoded scripts dir*/
  sprintf(jmx_fname , "%s/%s/%s/%s/%s/.Main", GET_NS_TA_DIR(), grp_proj_name, grp_subproj_name, "scripts", sess_name);
  NSDL2_PARSING(NULL, NULL, "jmx_fname = %s", jmx_fname);
  if((fp = fopen(jmx_fname, "r")) != NULL)
  {
    while(fgets(read_line, sizeof(read_line), fp) != NULL)
    {
      NSDL2_PARSING(NULL, NULL, "line = [%s]", read_line);
      read_line[strlen(read_line)] = '\0';
      if(strchr(read_line, '\n'))
        *(strchr(read_line, '\n')) = '\0';
      NSDL2_PARSING(NULL, NULL, "line = [%s]", read_line);

      // Ignore Empty & commented lines
      if(read_line[0] == '#' || read_line[0] == '\0') {
        NSDL2_PARSING(NULL, NULL, "Commented/Empty line continuing..");
        continue;
      }

      num_toks = get_tokens(read_line, field, "=", 8);
      NSDL2_PARSING(NULL, NULL, "num_toks = %d", num_toks);

      if(num_toks < 2) {
        NSDL2_PARSING(NULL, NULL, "num_toks < 2, continuing...");
        // No keyword value -- Give warning
        continue;
      }
      NSDL2_PARSING(NULL, NULL, "field[0] = [%s], field[1] = [%s]", field[0], field[1]);

      if(strcasecmp(field[0], "JMX_FILE_NAME") == 0)
        strcpy(jmx_file_name, field[1]);
      NSDL2_PARSING(NULL, NULL, "jmx_file_name = %s", jmx_file_name);
    }
    snprintf(jmx_sess_name, 1024, "%s/%s", sess_name, jmx_file_name);
    fclose(fp);
  }
  else if(fp == NULL)
  {
    //NS_KW_PARSING_ERR(buf, 0, err_msg, SGRP_USAGE, CAV_ERR_1011300,  "Selected script is not JMeter type script..");
    NS_EXIT(-1, CAV_ERR_1011347);
  //  NS_EXIT(-1, "ERROR: Selected script is not JMeter type script..");
  }

  //Check if file given for fileparameter exists or not
  sprintf(jmx_fname , "%s/%s/%s/%s/%s/%s", GET_NS_TA_DIR(), grp_proj_name, grp_subproj_name, "scripts", sess_name, jmx_file_name);
  if((fp = fopen(jmx_fname, "r")) != NULL)
  {
    // Bug - 104764 Handling of Parameter in JMX file will be done later. Commenting it in 4.6.0 
    #if 0
    while(fgets(buffer, 1024, fp) != NULL)
    {
      if((ptr = strstr(buffer, "testclass=\"CSVDataSet\"")) != NULL && (tmp_ptr = strstr(buffer, "enabled=\"true\"")) != NULL)
      {
        csvdatasetfound = 1;  //Setting if CSVDataSet is found.
        //continue;
      }
      else if((csvdatasetfound == 1) && ((ptr = strstr(buffer, "filename")) != NULL && (tmp_ptr = strstr(buffer, "</stringProp>")) != NULL))
      {
        len = snprintf(filename, (tmp_ptr-(ptr+9)), "%s", ptr+10);
        if(filename[0] == '/')   //Check for absolute path
        {
          strncpy(jmx_file_name, filename, len);
          jmx_file_name[len] = '\0';
        }
        else
          sprintf(jmx_file_name, "%s/%s/%s/%s/%s/%s", GET_NS_TA_DIR(), grp_proj_name, grp_subproj_name, "scripts", sess_name, filename);
        if(stat(jmx_file_name, &stat_st) == -1)
        {
          NS_EXIT(-1, CAV_ERR_1000016, filename);
          //NS_KW_PARSING_ERR(buf, 0, err_msg, SGRP_USAGE, CAV_ERR_1011300, "ERROR: File %s does not exists. Exiting", filename);
         // NS_EXIT(-1, "ERROR: File %s does not exists. Exiting", filename);
        }
        else if(stat_st.st_size == 0)
        {
          NS_EXIT(-1, CAV_ERR_1000017, filename);
          //NS_KW_PARSING_ERR(buf, 0, err_msg, SGRP_USAGE, CAV_ERR_1011300,  "ERROR: File %s is of zero size. Exiting", filename);
        // NS_EXIT(-1, "ERROR: File %s is of zero size. Exiting", filename);
        }   
        csvdatasetfound = 0; //unsetting variable. Set only if CSVDataSet found
      }
    }
    #endif
  }
  else if(fp == NULL)
  {
    //NS_KW_PARSING_ERR(buf, 0, err_msg, SGRP_USAGE, CAV_ERR_1011300,  "ERROR: File %s is of zero size. Exiting", filename);
    NS_EXIT(-1, CAV_ERR_1000006, filename, errno, nslb_strerror(errno));
  }
  NSDL2_PARSING(NULL, NULL, "returning..jmx_fname = %s", jmx_fname);
} 

/*--------------------------------------------------------------------------------------------- 
 * Purpose   : This function will parse keyword ENABLE_NC_TCPDUMP 
 *
 * Input     : buf   : ENABLE_NC_TCPDUMP <controller_mode> <generator_mode> <connection_type> <duration>  
 *                     controller_mode can be 0 or 1 
 *                          0: No tcpdump will be captured on controller
 *                          1: tcpdump will be captured on controller at path $NS_WDIR/logs/TR<>/ns_logs/tcpdump.mmddyyhhmmss.pcap
 *                          2: tcpdump will be captured at time of connection failure on controller 
 *                     generator_mode can be 0 or 1 or 2 
 *                          0: No tcpdump will be captured on generator
 *                          1: tcpdump will be captured everytime on generator 
 *                             at path $NS_WDIR/logs/TR<>/ns_logs/tcpdump.mmddyyyyhhmmss.pcap
 *                          2: tcpdump will be captured at time of connection failure on generator 
 *                             at path $NS_WDIR/logs/TR<>/ns_logs/tcpdump.mmddyyyyhhmmss.pcap
 *                    connection_type can be 0 or 1 or 2 
 *                          0: tcpdump on control connection only
 *                          1: tcpdump on data connection only 
 *                          2: tcpdump on both type of connection 
 *                    duration will be time in seconds, till which tcpdump will be captured 
 * Default value : ENABLE_NC_TCPDUMP 2 2 2 120
 *
 * In mode, if alphabet is passed then '0' will set for that mode
 * On duration, if alphabet is passed then 120 will set for duration.
 * On any incorrect value except incorrect number of argument, trace will not be logged. 
 *--------------------------------------------------------------------------------------------*/
void kw_set_enable_nc_tcpdump(char *buf)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char cont_val[MAX_DATA_LINE_LENGTH];
  char gen_val[MAX_DATA_LINE_LENGTH];
  char con_type[MAX_DATA_LINE_LENGTH];
  char dur[MAX_DATA_LINE_LENGTH];
  int icntrl_mode, igen_mode, icon_type, idru;
  icntrl_mode = igen_mode =  icon_type = idru = 0;
  int num = 0;
  NCTcpDumpSettings *nc_tcpdump;

  NSDL2_PARENT(NULL, NULL, "Method Called.");

  num = sscanf(buf,"%s %s %s %s %s", keyword, cont_val, gen_val, con_type, dur);

  if( num > 5)
  {
    NSTL1_OUT(NULL, NULL, "Invalid argument for Keyword %s", buf);
    return;
  }

  MY_MALLOC (global_settings->nc_tcpdump_settings, sizeof(NCTcpDumpSettings), "NCTcpDumpSettings", -1);
  nc_tcpdump = global_settings->nc_tcpdump_settings; 
  
  nslb_atoi(cont_val, &icntrl_mode);
  if(icntrl_mode > 2)
    icntrl_mode = 2;

  nslb_atoi(gen_val, &igen_mode);
  if(igen_mode > 2)
    igen_mode = 2;

  nslb_atoi(con_type, &icon_type);
  if(icon_type > 2)
    icon_type = 2;
    
  nslb_atoi(dur, &idru);
//adding check for duration as it can't be more than 5 min, because for long duration it will result high LA and CPU.
  if(idru == 0 || idru > 300)
  {
    NS_DUMP_WARNING("Duration passed in keyword ENABLE_NC_TCPDUMP is %d sec what is wrong, so changing it to default 120 sec.Please use value from 1 to 300 only.", idru);
    idru = 120;
  }
  nc_tcpdump->cntrl_mode = icntrl_mode;
  nc_tcpdump->gen_mode = igen_mode;
  if(icon_type == 0)
    nc_tcpdump->con_type_mode |= CNTRL_CONN;
  else if(icon_type == 1)
    nc_tcpdump->con_type_mode |= DATA_CONN;
  else
  {
    nc_tcpdump->con_type_mode |= CNTRL_CONN;
    nc_tcpdump->con_type_mode |= DATA_CONN;
  }

  nc_tcpdump->tcpdump_duration = idru;

  NSDL2_PARENT(NULL, NULL, "Value for ENABLE_NC_TCPDUMP | Controller Mode : %d, Generator Mode = %d, Connetion Type = %d, Duration = %d",
                             nc_tcpdump->cntrl_mode, nc_tcpdump->gen_mode, 
                             nc_tcpdump->con_type_mode, nc_tcpdump->tcpdump_duration);
}

//Add SGRP keyword - Achint 12/11/200
int read_scripts_glob_vars(char *file_name)
{
  FILE* conf_file;
  //Changing the limit from MAX_DATA_LINE_LENGTH(512) to BIG_DATA_LINE_LENGTH(8192) 
  //as was limiting the exception list
  char buf[MAX_LINE_LENGTH];
  char gbuf[MAX_DATA_LINE_LENGTH];
  char cbuf[MAX_DATA_LINE_LENGTH];
  char dummy[MAX_DATA_LINE_LENGTH];
  double pct;
  char keyword[MAX_DATA_LINE_LENGTH];
  char text[MAX_LINE_LENGTH];
  char sess_or_url_name[MAX_DATA_LINE_LENGTH]; //Change it may be script or url
  char sess_name[MAX_DATA_LINE_LENGTH];
  char jmx_sess_name[MAX_DATA_LINE_LENGTH];
  char uprof_name[MAX_DATA_LINE_LENGTH];
  char sg_name[MAX_DATA_LINE_LENGTH];
  char scen_type[MAX_DATA_LINE_LENGTH];
  char cluster_id[MAX_DATA_LINE_LENGTH];
  int num, idx;
  //int cluster_id;
  int rnum, i;
  char* tok;
  int get_filename;
  char clustval_filename[MAX_FILE_NAME];
  char groupval_filename[MAX_FILE_NAME];
  char* buf_ptr;
  int sg_fields = 5;
  int script_or_url = 0;
  char err_msg[MAX_DATA_LINE_LENGTH];
  int line_num = 0;
  char gen_name[BIG_DATA_LINE_LENGTH];
  int sps_flag = 0; //This is to check for the duplicacy of SYSTEM_PROXY_SERVER
  int spe_flag = 0; //This is to check for the duplicacy of SYSTEM_PROXY_EXCEPTION
  int spa_flag = 0; //This is to check for the duplicacy of SYSTEM_PROXY_AUTH
  //Store SGRP quantity/sess in string
  char sgrp_value[100];
  char *fields[10];
  char grp_proj_name[MAX_DATA_LINE_LENGTH];
  char grp_subproj_name[MAX_DATA_LINE_LENGTH];
  char proj_subproj_sess_name[MAX_DATA_LINE_LENGTH]; //Change it may be script or url
  char sess_name_with_proj_subproj[MAX_DATA_LINE_LENGTH];

  NSDL2_SCHEDULE(NULL, NULL, "Method called");
#if 0
  //SG that do not belong to any ckuster, belong to default cluster
  if (create_default_cluster() == -1)
    exit(-1);
#endif

  global_settings->get_no_inlined_obj = 0;
  global_settings->read_vendor_default = 1;
  conf_file = fopen(file_name, "r");
  if (!conf_file) {
    //NSTL1_OUT(NULL, NULL,  "read_scripts_glob_vars(): Error in opening file %s\n", file_name);
    //write_log_file(NS_SCENARIO_PARSING, "Failed to open scenario file %s, error:%s", file_name, nslb_strerror(errno));
    NS_EXIT(-1, CAV_ERR_1000006, file_name, errno, nslb_strerror(errno));
  }

  //Firstly check out if cluster variables are being used.
  //and set the cluser vars in in cbuf, if present in file
  cbuf[0] = '\0';
  while (fgets(buf, MAX_LINE_LENGTH, conf_file) != NULL) {
    line_num++;
    //NSTL1_OUT(NULL, NULL,  "Buffer2 = %s\n", buf);
    if ((num = sscanf(buf, "%s %s", keyword, text)) < 2) {
	continue;
    } else if (!strcasecmp(buf, "CLUSTER_VARS")) {
	if (strlen(cbuf)) {
          //write_log_file(NS_SCENARIO_PARSING, "CLUSTER_VARS entries can only be used once in the scenario conf file");
          NS_EXIT(-1, CAV_ERR_1011358);
	}

	if (strchr(buf, '\n'))
	  *(strchr(buf, '\n')) = '\0';

 	strcpy (cbuf, buf);
	sg_fields = 6;
    } else if (!strcasecmp(buf, "USE_CLUSTERS")) {

	if (strchr(buf, '\n'))
	  *(strchr(buf, '\n')) = '\0';

	if (atoi(text) == 1)
	    sg_fields = 6;
    } else if (strcasecmp(keyword, "NVM_DISTRIBUTION") == 0) {
      kw_set_nvm_distribution(text, global_settings, 0, err_msg, buf);
      if(global_settings->nvm_distribution == 2) {
        sg_fields = 6;
      }
    } else if (!strcasecmp(buf, "SIGNATURE")) {
	global_settings->read_vendor_default = 0;
    } else if (strcasecmp(keyword, "USE_HTTP_10") == 0){
        kw_set_use_http_10(buf);
    } else if ((strcasecmp(keyword, "OPTIMIZE_ETHER_FLOW") == 0)){
	  set_optimize_ether_flow(buf, 0, err_msg);
    }
    else if (strcasecmp(keyword, "DEBUG_TRACE") == 0) {
      kw_set_debug_trace(text, keyword, buf);
    } 
    // Added by Anuj: 30/5/08
    else if (strcasecmp(keyword, "MAX_DEBUG_LOG_FILE_SIZE") == 0) {
      //printf("Calling kw_set_max_debug_log_file_size()\n");
     // if (kw_set_max_debug_log_file_size(buf, err_msg, 0) != 0) 
      //  exit(-1);
      kw_set_max_debug_log_file_size(buf, err_msg, 0);
    }else if (strcasecmp(keyword, "PARTITION_SETTINGS") == 0) {
        kw_set_partition_settings(buf, 0, err_msg);
    }else if (strcasecmp(keyword, "EVENT_LOG") == 0) {
      kw_set_event_log(buf, 0, err_msg);
    } else if (strcasecmp(keyword, "ENABLE_LOG_MGR") == 0) {
      kw_set_enable_log_mgr(buf, 0, err_msg);
    } else if (strcasecmp(keyword, "EVENT_DEFINITION_FILE") == 0) {
      kw_set_event_definition_file(buf);
    } else if (strcasecmp(keyword, "SSL_KEY_LOG") == 0) {
      kw_set_ssl_key_log(buf);
    }

    #if OPENSSL_VERSION_NUMBER < 0x10100000L
     else if (strcasecmp(keyword, "SSL_LIB_TRACE") == 0) {
      kw_set_ssl_lib_trace(buf);
    }  else if (strcasecmp(keyword, "SSL_LIB_HPM") == 0) {
      kw_set_ssl_lib_hpm(buf); 
    }
    #endif  

    /* else if (strcasecmp(keyword, "TLS_VERSION") == 0){
      kw_set_tls_version(keyword, text);
     }else if (strcasecmp(keyword, "SSL_RENEGOTIATION") == 0){
      start_ssl_renegotiation(buf);
    } */ else if (strcasecmp(keyword, "PARENT_EPOLL") == 0) {
      kw_set_parent_epoll(buf);
    } else if (strcasecmp(keyword, "NVM_EPOLL") == 0) {
      kw_set_nvm_epoll(buf);
    }else if (strcasecmp(keyword, "USE_JAVA_OBJ_MGR") == 0) {
        kw_set_java_obj_mgr(buf);
      }
    // Added by Anuj: 30/5/08
    else if (strcasecmp(keyword, "MAX_ERROR_LOG_FILE_SIZE") == 0) {
      kw_set_max_error_log_file_size(buf, err_msg, 0);
    }
    //Added by Anuj: 30/10/07
    else if (strcasecmp(keyword, "PAGE_AS_TRANSACTION") == 0){
      set_page_as_trans (buf, err_msg, 0);
    }
    //Added by Anuj: 25/03/08 
    else if (strcasecmp(keyword, "CPU_AFFINITY") == 0){
      parse_cpu_affinity(buf);
    } /* Not Used
    else if (strncasecmp(keyword, "UNCOMPRESS_CHUNK", strlen("UNCOMPRESS_CHUNK")) == 0){
      g_uncompress_chunk = atoi(text);
    } */
    else if (strcasecmp(keyword, "AUTO_REDIRECT") == 0) {
      kw_set_auto_redirect(buf, err_msg, 0);
    } else if (strcasecmp(keyword, "IO_VECTOR_SIZE") == 0) {
      kw_set_io_vector_size(buf, err_msg, 0);
    } else if (strcasecmp(keyword, "ENABLE_TRANSACTION_CUMULATIVE_GRAPHS") == 0) {
      kw_set_tx_cumulative_graph(buf, err_msg, 0);
    } else if (strcasecmp(keyword, "ADD_RECORDED_HOST") == 0) {
      char r_host[MAX_LINE_LENGTH];
      char r_type[MAX_LINE_LENGTH];
      if ((num = sscanf(buf, "%s %s %s", keyword, r_host, r_type)) < 2) {
        NS_EXIT(-1, "read_keywords(): Need Three fields after key ADD_RECORDED_HOST");
      }
      total_add_rec_host_entries++;
      if (num == 2)
        get_server_idx(r_host, HTTP_REQUEST, -1);
      else {
        if (strcasecmp("HTTPS", r_type) == 0)
          get_server_idx(r_host, HTTPS_REQUEST, -1);
        else if (strcasecmp("HTTP", r_type) == 0)
          get_server_idx(r_host, HTTP_REQUEST, -1);
        else {
          NS_EXIT(-1, "read_keywords(): ADD_RECORDED_HOST third field is of unknown type");
        }
      }
    } else if (strcasecmp(keyword, "DISABLE_COOKIES") == 0) {
      kw_set_cookies(keyword, text, err_msg);
    /*} else if (strcasecmp(keyword, "TIME_STAMP") == 0) {
      kw_set_time_stamp_mode(text);*/
    } else if (strcasecmp(keyword, "EXCLUDE_FAILED_AGGREGATES") == 0) {
      kw_set_exclude_failed_aggregates(buf, &global_settings->exclude_failed_agg);
    } else if (strcasecmp(keyword, "SCHEDULE_TYPE") == 0) {
      kw_set_schedule_type(buf, err_msg);
    } else if (strcasecmp(keyword, "SCHEDULE_BY") == 0) {
      kw_set_schedule_by(buf, err_msg);
    } else if (strcasecmp(keyword, "ENABLE_SYNC_POINT") == 0) {
      kw_enable_sync_point(buf, err_msg, 0);
    } else if (strcasecmp(keyword, "PROF_PCT_MODE") == 0) {
      kw_set_prof_pct_mode(buf, err_msg);
    } else if (strcasecmp(keyword, "JAVA_SCRIPT_RUNTIME_MEM") == 0) {
      kw_set_java_script_runtime_mem(buf);
    } else if (strcasecmp(keyword, "STYPE") == 0) {
      kw_set_stype(buf, 0, err_msg);
    } else if (strcasecmp(keyword, "ERROR_LOG") == 0) {
      kw_set_error_log(buf);
   // } //else if (strcasecmp(keyword, "ENABLE_PIPELINING") == 0) {
      /* Moved here as we need to set pipelinig keyword before script Expect: header parsing*/
     // kw_set_enable_pipelining(buf, global_settings, 0);
    } else if (strcasecmp(keyword, "TR069_CPE_DATA_DIR") == 0) {
      kw_set_tr069_cpe_data_dir(buf, global_settings->tr069_data_dir, 0);
    } else if (strcasecmp(keyword, "TR069_ACS_URL") == 0) {
      kw_set_tr069_acs_url(buf, global_settings->tr069_acs_url, 0);
    } else if (strcasecmp(keyword, "TR069_MAIN_ACS_URL") == 0) {
      kw_set_tr069_acs_url(buf, global_settings->tr069_main_acs_url, 0);
    } else if (strcasecmp(keyword, "TR069_MAIN_ACS_HTTP_URL") == 0) {
      kw_set_tr069_acs_url(buf, global_settings->tr069_main_acs_url, 0);
    } else if (strcasecmp(keyword, "TR069_OPTIONS") == 0) {
      kw_set_tr069_options(buf, &(global_settings->tr069_options), 0);
    }  else if (strcasecmp(keyword, "TR069_CPE_REBOOT_TIME") == 0) {
      kw_set_tr069_cpe_reboot_time(buf, &(global_settings->tr069_reboot_min_time), &(global_settings->tr069_reboot_max_time), 0);
    } else if  (strcasecmp(keyword,"TR069_CPE_DOWNLOAD_TIME") == 0) {
      kw_set_tr069_cpe_download_time(buf, &(global_settings->tr069_download_min_time), &(global_settings->tr069_download_max_time), 0);

    } else if  (strcasecmp(keyword,"TR069_CPE_PERIODIC_INFORM_TIME") == 0) {
      kw_set_tr069_cpe_periodic_inform_time(buf, &(global_settings->tr069_periodic_inform_min_time), &(global_settings->tr069_periodic_inform_max_time), 0);    
    } else if (strcasecmp(keyword, "DYNAMIC_URL_HASH_TABLE_OPTION") == 0) {
       kw_set_dynamic_url_hash_table_option(buf, 0);
    } else if (strcasecmp(keyword, "STATIC_URL_HASH_TABLE_OPTION") == 0) {
       kw_set_static_url_hash_table_option(buf, 0);
    } else if (strcasecmp(keyword, "MAX_DYNAMIC_HOST") == 0) {
      kw_set_max_dyn_host(buf, err_msg, 0);
    } else if (strcasecmp(keyword, "VUSER_THREAD_POOL") == 0) {
        kw_set_vuser_thread_pool(buf, 0, err_msg);
    } else if (strcasecmp(keyword, "NET_DIAGNOSTICS_SERVER") == 0) {
        kw_set_net_diagnostics_server(buf, 0); //Earlier this keyword was in read keyword function but since it is a global keyword so moving it here as we wanted to parse this keyword before G_ENABLE_NET_DIAGNOSTICS keyword.
    } else if (strncasecmp(keyword, "G_", strlen("G_")) == 0) {
      parse_group_keywords(buf, 1, line_num);
    }else if (strcasecmp(keyword, "ENABLE_DNS_GRAPH") == 0) {
      kw_set_enable_dns_lookup_graphs(buf);
    }else if (strcasecmp(keyword, "HEALTH_MONITOR_DISK_FREE") == 0) {
      nslb_kw_set_disk_free(buf, 1, err_msg, 0);
    }else if (strcasecmp(keyword, "HEALTH_MONITOR_INODE_FREE") == 0) {
      nslb_kw_set_inode_free(buf,1, err_msg, 0);
    }else if (strcasecmp(keyword, "HIERARCHICAL_VIEW") == 0) {
      kw_enable_hierarchical_view(keyword, buf);
    }else if (strcasecmp(keyword, "CAVMON_INBOUND_PORT") == 0) {
      kw_set_enable_cavmon_inbound_port(keyword, buf);
    }else if (strcasecmp(keyword, "DEBUG_LOGGING_WRITER") == 0) {
      /*This is to enable/disable logging writer debug*/
      kw_logging_writer_debug(buf);
    }else if (strcasecmp(keyword, "SYSTEM_PROXY_SERVER") == 0) {
      if(!sps_flag)
        if(kw_set_system_proxy_server(buf, err_msg, RTC_DISABLE) == PROXY_ERROR) {
          NS_EXIT(-1, "%s", err_msg);
        }
      sps_flag = 1;
    }else if (strcasecmp(keyword, "SYSTEM_PROXY_EXCEPTIONS") == 0) {
      if(!spe_flag)
       if(kw_set_system_proxy_exceptions(buf, err_msg)!= 0) {
         NS_EXIT(-1, "%s", err_msg);
       }
      spe_flag = 1;
    }
    else if (strcasecmp(keyword, "SYSTEM_PROXY_AUTH") == 0) {
      if(!spa_flag)
        if(kw_set_system_proxy_auth(buf, err_msg, RTC_DISABLE) == PROXY_ERROR) {
          NS_EXIT(-1, "%s", err_msg);
        }
      spa_flag = 1;
    }else if (strcasecmp(keyword, "TEST_MONITOR_CONFIG") == 0) {
       #ifndef CAV_MAIN
       kw_set_test_monitor_config(buf, err_msg, 0);
       #endif
    }else if (strcasecmp(keyword, "CAV_EPOCH_YEAR") == 0) {
        kw_set_cav_epoch_year(buf, 0, err_msg);
    }else if (strcasecmp(keyword, "READER_RUN_MODE") == 0) {
        #ifndef CAV_MAIN
        kw_set_reader_run_mode (buf, 0, err_msg);
        #endif
    }else if (strcasecmp(keyword, "NSDBU_CHUNK_SIZE") == 0) {
        //This keyword is not supported now. Use DBU_CHUNK_SIZE keyword in scenario.
        kw_set_dbu_chunk_size(buf, 1 /*Old Keyword */);
    }else if (strcasecmp(keyword, "DBU_CHUNK_SIZE") == 0) {
        //This keyword is used now in place of NSDBU_CHUNK_SIZE keyword.
        kw_set_dbu_chunk_size(buf, 0 /* New Keyword */);
    }else if (strcasecmp(keyword, "NSDBU_IDLE_TIME") == 0) {
        kw_set_db_idle_time(buf);
    }else if (strcasecmp(keyword, "PARTITION_SWITCH_DELAY") == 0) {
        kw_set_partition_switch_delay_time(buf);
    }else if (strcasecmp(keyword, "TABLESPACE_INFO") == 0) {
        kw_set_tablespace_info(buf);
    }else if (strcasecmp(keyword, "NSDBU_NUM_CYCLES") == 0) {
        kw_set_db_num_cycles(buf);
    }else if (strcasecmp(keyword, "NLW_TRACE_LEVEL") == 0) {
        kw_set_nlw_trace_level(buf, err_msg, RTC_DISABLE);
    }else if (strcasecmp(keyword, "NLR_TRACE_LEVEL") == 0) {
        kw_set_nlr_trace_level(buf, err_msg, RTC_DISABLE);
    }else if (strcasecmp(keyword, "NSDBU_TRACE_LEVEL") == 0) {
        kw_set_nsdbu_trace_level(buf, err_msg, RTC_DISABLE);
    }else if (strcasecmp(keyword, "NIFA_TRACE_LEVEL") == 0) {
        kw_set_nifa_trace_level(buf, err_msg, RTC_DISABLE);
    }else if (strcasecmp(keyword, "NIRRU_TRACE_LEVEL") == 0) {
        kw_set_nirru_trace_level(buf, err_msg, RTC_DISABLE);
    }else if (strcasecmp(keyword, "NLM_TRACE_LEVEL") == 0) {
        kw_set_nlm_trace_level(buf, err_msg, RTC_DISABLE);
    }else if (strcasecmp(keyword, "NS_SEND_MAIL") == 0){
        kw_set_send_mail_value(buf, err_msg, RTC_DISABLE);
    }else if (strcasecmp(keyword, "SHOW_GROUP_DATA") == 0) {
        kw_set_group_based_data(buf, err_msg, 0);
    }//else if (strcasecmp(keyword, "SHOW_IP_DATA") == 0) {
       // kw_set_ip_based_data(buf,err_msg);
    //}
    else if (strcasecmp(keyword, "DB_TMP_FILE_PATH") == 0) {
        //This keyword is not supported now. Use ENABLE_TMPFS_IN_DBU keyword in scenario.
        kw_enable_tmpfs_in_dbu(buf, 1 /*Old Keyword */);
    }else if (strcasecmp(keyword, "ENABLE_TMPFS_IN_DBU") == 0) {
        //This keyword is used now in place of DB_TMP_FILE_PATH keyword.
        kw_enable_tmpfs_in_dbu(buf, 0 /* New Keyword */);
    /*}else if (strcasecmp(keyword, "MULTIDISK_PATH") == 0) {
        kw_set_multidisk_path(buf);*/
    }else if (strcasecmp(keyword, "PARENT_CHILD_CON_TIMEOUT") == 0) {
        kw_set_parent_child_con_timout(buf);
    }
    // RBU_ENABLE_AUTO_PARAM <mode 0/1>  0 - Disable (Default), 1 - Enable 
    // This will automate profile and vnc's automatically.
    else if (strcasecmp(keyword, "RBU_ENABLE_AUTO_PARAM") == 0) {
      kw_set_rbu_enable_auto_param(buf);
    }
    else if (strcasecmp(keyword, "ENABLE_NS_CHROME") == 0) {
      kw_set_ns_chrome(buf, err_msg, 0);
    }
    else if (strcasecmp(keyword, "ENABLE_NS_FIREFOX") == 0) {
      kw_set_ns_firefox(buf, err_msg, 0);
    } 
    else if (strcasecmp(keyword, "MACHINE_NAME") == 0) {
        kw_set_machine_name(buf);

      #if 0
        else if (strncasecmp(keyword, "SP", strlen("SP")) == 0)
        kw_set_sp(buf, global_settings, 0);
      #endif
    }
    else if (strcasecmp(keyword, "GPERF_CMD_OPTIONS") == 0) {
      kw_set_gperf_cmd_options(buf);
    } else if(strcasecmp(keyword, "STOP_TEST_IF_DNSMASQ_NOT_RUNNING") == 0) {
      kw_set_stop_test_if_dnsmasq_not_run(buf);
    /*} else if(strcasecmp(keyword, "CHECK_NC_BUILD_COMPATIBILITY") == 0) {
      kw_check_nc_build_compatibility(buf);*/
    } else if(strcasecmp(keyword, "DISABLE_USE_OF_GEN_SPECIFIC_KWD_FILE") == 0) {
      kw_check_use_of_gen_specific_kwd_file(buf);
    } else if(strcasecmp(keyword, "NS_PARENT_LOGGER_LISTEN_PORTS") == 0) {
      kw_ns_parent_logger_listen_ports(buf);
    }else if(strcasecmp(keyword, "SAVE_NVM_FILE_PARAM_VAL") == 0){
      kw_save_nvm_file_param_val(buf, 0, err_msg);
    } else if (strcasecmp(keyword, "REPLAY_FILE") == 0) {
      kw_set_replay_file(buf, 0);
    } else if (strcasecmp(keyword, "SHOW_SERVER_IP_DATA") == 0) {
      kw_set_show_server_ip_data(buf, err_msg, 0);
    } else if (strcasecmp(keyword, "DYNAMIC_TX_SETTINGS") == 0) {
      kw_set_dynamic_tx_settings(buf);
    } else if (strcasecmp(keyword, "ENABLE_FCS_SETTINGS") == 0) {
      kw_set_enable_schedule_fcs(buf, 0, err_msg);
    } else if (strcasecmp(keyword, "DYNAMIC_CM_RT_HASH_TABLE_SETTINGS") == 0) {
      kw_set_dynamic_cm_rt_table_settings(buf);
    } else if(strcmp(keyword, "LIB_EXIT") == 0) {
      kw_set_lib_do_exit(buf);
    } else if(strcmp(keyword, "URI_ENCODING") == 0) {
        kw_set_uri_encoding(buf, err_msg, 0);
    } else if(strcmp(keyword, "TEST_METRICS_MODE") == 0) {
      kw_set_test_metrics_mode(buf);
    } else if(strcmp(keyword, "MSSQL_STATS_MONITOR_BUF_SIZE") == 0) {
      kw_set_mssql_stats_monitor_buf_size(buf);
    }
    else if (strcasecmp(keyword, "DISABLE_SCRIPT_COPY_TO_TR") == 0) {
      kw_set_disable_copy_script(buf, 0, err_msg);
    }
    else if (strncasecmp(keyword, "ENABLE_NC_TCPDUMP", strlen("ENABLE_NC_TCPDUMP")) == 0) {
      kw_set_enable_nc_tcpdump(buf);
    }

/*else if (strcasecmp(keyword, "SYSTEM_PROXY_EXCEPTIONS") == 0 && sps_flag) {
      kw_set_system_proxy_ex_list(buf);
    }*/
  }

  // Validate if TR069_ACS_URL given 
  tr069_validates_urls(); 

  rewind(conf_file);
  line_num = 0;

  //Initialize Scenrio group table with scenrio group name & Script names
  //and set the group vars in gbuf , if present in file
  gbuf[0] = '\0';
  while (fgets(buf, MAX_LINE_LENGTH, conf_file) != NULL) {

    line_num++;

    // ignore commented and blank lines
    if((buf[0] == '\n') || buf[0] == '#')
      continue;

    if ((num = sscanf(buf, "%s %s", keyword, text)) != 2) {
      NSTL1_OUT(NULL, NULL,  "Warning: Group name is not given in line = %s, in line %d", buf, line_num);// Show warning message for line, line number
      continue;
    } else {
      if (!strcasecmp(keyword, "SG") || !strcasecmp(keyword, "SGRP" )) {
                    // SG keyword will be deleted later. This code to for backward compatibility.


	if (!strcasecmp(keyword, "SG") )
	{
                script_or_url = 0; //As default is 0 for script
	        if ((num = sscanf(buf, "%s %s %s %s %d %s", dummy, sg_name, uprof_name, sess_or_url_name, (int *)&pct, cluster_id)) < sg_fields){
	         // NSTL1_OUT(NULL, NULL, "read_scripts_glob_vars(): Need %d fields after keyword SG\n", sg_fields-1);
                  write_log_file(NS_SCENARIO_PARSING, "%s keyword requires %d number of arguments", keyword, sg_fields-1);
	          return -1;
        	}
                val_sgrp_name(buf, sg_name, line_num);//validate group name

	} 
        else 
        {
          /* SGRP <GroupName> <Gen name> <ScenType> <user-profile> <type(0 for Script or 1 for URL)> <session-name/URL> <num-or-pct> <cluster_id>*/
          num = sscanf(buf, "%s %s %s %s %s %d %s %s %s", dummy, sg_name, gen_name, scen_type,
                             uprof_name, &script_or_url, sess_or_url_name, sgrp_value, cluster_id);
          NSDL2_PARSING(NULL, NULL, "gen_name = %s, scen_type = %s, uprof_name = %s, sgrp_value = %s", gen_name, scen_type, 
                        uprof_name, sgrp_value); 
          if(ns_is_float(sgrp_value))
          {
            pct = atof(sgrp_value);
          }
          else
          {
            NS_KW_PARSING_ERR(buf, 0, err_msg, SGRP_USAGE, CAV_ERR_1011349, sgrp_value);
          }
          if((loader_opcode == STAND_ALONE) && (strcmp(gen_name, "NA")))
          {
           // snprintf(dummy, MAX_DATA_LINE_LENGTH - 1, "In non controller mode generator name should be NA, but"
                          //" given generator name is \"%s\".", gen_name);
            NS_KW_PARSING_ERR(buf, 0, err_msg, SGRP_USAGE, CAV_ERR_1011348, gen_name);
            //usage_kw_set_sgrp_new_format(dummy);
          }

          if ((strcasecmp(scen_type, "NA") == 0) || (strcmp(scen_type, "FIX_CONCURRENT_USERS") == 0)
                                       || (strcmp(scen_type, "FIX_SESSION_RATE") == 0))
          {
            NSDL2_SCHEDULE(NULL, NULL, "New SGRP format, hence validate with incremented field count");
            if (num < (sg_fields + 1 + 1 + 1)) //Added generator_name field
            {
              //NSTL1_OUT(NULL, NULL, "read_scripts_glob_vars(): Need %d fields after keyword SGRP\n", sg_fields -1 + 1 + 1 + 1);
              NS_KW_PARSING_ERR(buf, 0, err_msg, SGRP_USAGE, CAV_ERR_1011300, CAV_ERR_MSG_1);
              //usage_kw_set_sgrp_new_format(NULL);
            }
          }
          /* SGRP <GroupName> <ScenType> <user-profile> <type(0 for Script or 1 for URL)> <session-name/URL> <num-or-pct> <cluster_id>           */
          else
          { 
            NSDL2_SCHEDULE(NULL, NULL, "Old SGRP format");
            if ((num = sscanf(buf, "%s %s %s %s %d %s %s %s", dummy, sg_name, scen_type, uprof_name, &script_or_url, sess_or_url_name, sgrp_value, cluster_id)) < (sg_fields + 1 + 1))// Keyword will take another field script type
	    {
              NS_KW_PARSING_ERR(buf, 0, err_msg, SGRP_USAGE, CAV_ERR_1011300, CAV_ERR_MSG_1);
	      //NSTL1_OUT(NULL, NULL, "read_scripts_glob_vars(): Need %d fields after keyword SGRP\n", sg_fields -1 + 1 + 1);
              //snprintf(dummy, MAX_DATA_LINE_LENGTH - 1, "Number of fields required for new SGRP keyword is %d, given %d",
                              //sg_fields + 1 + 1, num);
              //usage_kw_set_sgrp(dummy);
	    }
            //new_sgrp_format_used = 0;
            strcpy(gen_name, "NA");
            gen_name[2] = '\0';
            if (!ns_is_float(sgrp_value))
            {
              NS_KW_PARSING_ERR(buf, 0, err_msg, SGRP_USAGE, CAV_ERR_1011349, sgrp_value);
            }
            pct = atof(sgrp_value); 
          }

          NSDL2_PARSING(NULL, NULL, "scen_type = %s, uprof_name = %s", scen_type, uprof_name);  
          
          /* STAND_ALONE:
           * Ignore groups with quantity/percentage zero*/ 
           if (loader_opcode != CLIENT_LOADER) {
             if (pct == 0) {
               NSDL2_SCHEDULE(NULL, NULL, "Ignoring SGRP groups with quantity or percentage given zero");
               continue;
             }
           }

           if(pct)
             total_active_runprof_entries++;  //increment when grp have atleast 1 user

           NSDL2_SCHEDULE(NULL, NULL, "total_active_runprof_entries = %d", total_active_runprof_entries);

          val_sgrp_name(buf, sg_name, line_num);//validate group name
              
          /* Validation:
           * o  if STYPE is not MIXED_MODE, then scen_type can have only "None" is its values.
           * o  if STYPE is MIXED_MODE, scen_type can either be FCU or FSR, nothing else.
           * o  sg_name can not be all/ALL
           */
          if (strcasecmp(sg_name, "ALL") == 0) {
             NS_KW_PARSING_ERR(buf, 0, err_msg, SGRP_USAGE, CAV_ERR_1011301, "Group name can never be 'ALL' in Scenario Group.");
          }
  
          if (testCase.mode != TC_MIXED_MODE) {
            if (strcmp(scen_type, "NA") != 0) {
                NS_KW_PARSING_ERR(buf, 0, err_msg, SGRP_USAGE, CAV_ERR_1011303, 
                                  "ScenType can only be 'NA' in Scenario Group for 'Mixed Mode' scenario");  
            }
          } else if (testCase.mode == TC_MIXED_MODE) {
            if ((strcmp(scen_type, "FIX_CONCURRENT_USERS") != 0) && 
                (strcmp(scen_type, "FIX_SESSION_RATE") != 0)) {
                 NS_KW_PARSING_ERR(buf, 0, err_msg, SGRP_USAGE, CAV_ERR_1011303, "ScenType can be either FIX_CONCURRENT_USER or FIX_SESSION_RATE for Mixed type scenario");
             // usage_kw_set_sgrp("ScenType in SGRP should be either FIX_CONCURRENT_USERS or FIX_SESSION_RATE");
            }
          }
	} //endif SGRP <GroupName>
	//Check if the scenrio group name already added
	for (i = 0; i < total_runprof_entries; i++) {
	  if (!strcmp(sg_name, RETRIEVE_BUFFER_DATA(runProfTable[i].scen_group_name))) { 
            NS_KW_PARSING_ERR(buf, 0, err_msg, SGRP_USAGE, CAV_ERR_1011350, sg_name); 
	  //  NS_EXIT(-1, "read_scripts_glob_vars(): Scenario group %s already defined", sg_name);
	  }
	}

	if (create_runprof_table_entry(&rnum) != SUCCESS) {
          NS_EXIT(-1, "read_scripts_glob_vars(): Error in getting sessprof_table entry");
	}

	runProfTable[rnum].group_num = rnum; 
        if (loader_opcode == CLIENT_LOADER) {
          if (ns_is_numeric(gen_name) == 0)
          {
            write_log_file(NS_SCENARIO_PARSING, "In generator mode generator count should be numeric, but"
                          " given generator count is \"%s\" for group '%s'", gen_name, runProfTable[rnum].scen_group_name);
            NS_KW_PARSING_ERR(buf, 0, err_msg, SGRP_USAGE, CAV_ERR_1011351, gen_name, runProfTable[rnum].scen_group_name);
           // NSTL1_OUT(NULL, NULL, "%sError: In generator mode generator count should be numeric, but given generator count is \"%s\".\n",
            //                 buf, gen_name);
          //  NSTL1_OUT(NULL, NULL,  "For generator mode SGRP keyword syntax: \n");
          //  NSTL1_OUT(NULL, NULL,  "SGRP <GroupName> <GeneratorCount> <ScenType> <user-profile> <type(0 for Script or 1 for URL)> <session-name/URL> <num-or-pct> <cluster_id>\n");
          //  return -1;
          } else {
            if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO) {
              global_settings->num_generators = atoi(gen_name); 
            } else {
              runProfTable[rnum].num_generator_per_grp = atoi(gen_name);
            }
          }               
        } else { //In stand-alone let total generator be 0
          global_settings->num_generators = 0;  
          runProfTable[rnum].num_generator_per_grp = 0;
        }
        if (strcmp(scen_type, "NA") == 0)
          runProfTable[rnum].grp_type = -1;
        if (strcmp(scen_type, "FIX_CONCURRENT_USERS") == 0)
          runProfTable[rnum].grp_type = TC_FIX_CONCURRENT_USERS;
        if (strcmp(scen_type, "FIX_SESSION_RATE") == 0)
          runProfTable[rnum].grp_type = TC_FIX_USER_RATE;
 
        //Initial proxy_idx
        runProfTable[rnum].proxy_idx = -1;

        if (global_settings->schedule_by == SCHEDULE_BY_GROUP) { /* We construct basic phases */
          //if (global_settings->schedule_type = SCHEDULE_TYPE_SIMPLE)  /* 1 phase, Start */
          MY_REALLOC_EX(group_schedule, sizeof(Schedule) * (rnum + 1), sizeof(Schedule) * rnum,  "grp_schedule", rnum);
          initialize_schedule(&group_schedule[rnum], rnum);

        } else if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO) {
          #ifdef CAV_MAIN
          FREE_AND_MAKE_NULL_EX (scenario_schedule, sizeof(Schedule), "schedule", -1);
          #endif
          if (scenario_schedule == NULL) { /* Only once */
            MY_MALLOC(scenario_schedule, sizeof(Schedule), "grp_schedule", rnum);
            initialize_schedule(scenario_schedule, -1);
          }
        }
	//Add the scenrio group name
	if ((runProfTable[rnum].scen_group_name = copy_into_big_buf(sg_name, 0)) == -1) {
	  NS_EXIT(-1, CAV_ERR_1000018, sg_name);
	}

        //Bug:41770 :- In case of RDT type scenarios user profile should be Internet.
        if(script_or_url == RDT_MOBILE || script_or_url == RDT_DESKTOP){
          NSDL2_SCHEDULE(NULL, NULL, "Found RDT type scenarios hence changing user profile  '%s' to 'Intenet'", uprof_name);
          strncpy(uprof_name, "Internet", 10); // If the length of src is less than n, strncpy() writes additional null bytes to dest.
        }
        else if(script_or_url == SCRIPT_TYPE_JMETER)
        {
          NSDL2_SCHEDULE(NULL, NULL, "Found JMETER type scenarios hence changing user profile '%s' to 'Intenet'", uprof_name);
          strncpy(uprof_name, "Internet", 10);
        }

	//Note the User Profile Name
	if ((runProfTable[rnum].userprof_name = copy_into_temp_buf(uprof_name, 0)) == -1) {
	  NS_EXIT(-1, CAV_ERR_1000018, uprof_name);
	 // NS_EXIT(-1, "Failed to copy the user-prof name %s into  temp buf for  SG/SGRP =  %s\n", uprof_name, sg_name);
	}

        // Before parsing script arguments from SGRP Keyword
        // Store global project and subproject name in local variable, grp_proj_name and grp_subproj_name, 
        // These variables will store value of proj_name and subproj_name according to group
        // These variables will then be used to find sessidx 
        strcpy(grp_proj_name, g_project_name);
        strcpy(grp_subproj_name, g_subproject_name);
      
        NSDL2_PARSING(NULL, NULL, "script_or_url = %d", script_or_url);

	//Call function create_url_based_script() to create URL script
	if (script_or_url == 1)
	{
          //TODO: for url based script 
    	  strcpy(sess_name, create_url_based_script( "gname", sess_or_url_name));
	}
	else
	{
          if(script_or_url == RDT_MOBILE || script_or_url == RDT_DESKTOP)
            g_script_or_url = 1;
           
          NSDL2_SCHEDULE(NULL, NULL, "sess_or_url_name = %s", sess_or_url_name);
          if(strchr(sess_or_url_name, '/')) 
          {
	    /*bug id: 101320: using g_ns_ta_dir instead of g_ns_wdir, avoid using hardcoded scripts dir*/
                 /*get script_dir name */
            char *script_dir_name = basename(sess_or_url_name);
            /*get proj/sub_proj_name*/
            char *pro_sub_proj_name = dirname(sess_or_url_name);
            sprintf(proj_subproj_sess_name, "%s/%s/%s/%s", GET_NS_RTA_DIR(), pro_sub_proj_name, "scripts", script_dir_name);
            NSDL2_SCHEDULE(NULL, NULL, "proj_subproj_sess_name = %s", proj_subproj_sess_name);
            if(access(proj_subproj_sess_name, F_OK) == 0)
            {
              get_tokens(sess_or_url_name, fields, "/", 10); 
              strcpy(grp_proj_name, fields[0]);
              strcpy(grp_subproj_name, fields[1]);
              strcpy(sess_name, script_dir_name);
              NSDL2_SCHEDULE(NULL, NULL, "sess_name = %s", sess_name);
            }
            else
            {
              write_log_file(NS_SCENARIO_PARSING, "Script path %s not found for group %s", proj_subproj_sess_name,
                        RETRIEVE_BUFFER_DATA(runProfTable[rnum].scen_group_name));
              NS_KW_PARSING_ERR(buf, 0, err_msg, SGRP_USAGE, CAV_ERR_1011352, proj_subproj_sess_name);
            //  NSTL1_OUT(NULL, NULL,  "[%s] provided path doesn't exist, hence returning" , proj_subproj_sess_name);
             // return -1; 
  	    }
           }
           else
           {
             strcpy(sess_name, sess_or_url_name);
           } 
        }

        //Stored script name along with project and sub-project names
        // Add macro for script type - 0   => SCRIPT_TYPE_DEFAULT
        //                             1   => SCRIPT_TYPE_URL
        //                             10  => SCRIPT_TYPE_RDT_MOBILE
        //                             11  => SCRIPT_TYPE_RDT_DESKTOP
        //                             100 => SCRIPT_TYPE_JMETER 
        if(script_or_url == SCRIPT_TYPE_JMETER)
        {
          g_script_or_url = 100;
          get_jmeter_script_name(jmx_sess_name, grp_proj_name, grp_subproj_name, sess_name);
        }
       
        snprintf(sess_name_with_proj_subproj, 2048, "%s/%s/%s", grp_proj_name, grp_subproj_name, sess_name);

	//Add Session (script Name)
        //Passing full name (proj/subproj/script_name) to match if session idx found or not
	//if ((idx = find_session_idx(sess_name)) == -1)
	if ((idx = find_session_idx(sess_name_with_proj_subproj)) == -1) {
	  if (create_sess_table_entry(&idx) != SUCCESS) {
            write_log_file(NS_SCENARIO_PARSING, "Failed to create session table entry for group '%s'",
                                  RETRIEVE_BUFFER_DATA(runProfTable[rnum].scen_group_name));
	    NS_EXIT(-1, "read_scripts_glob_vars(): Error in creating session table");
	  //  return -1;
	  }
          //if(global_settings->protocol_enabled & JMETER_PROTOCOL_ENABLED)
          if(script_or_url == SCRIPT_TYPE_JMETER)
          {
            NSDL2_MISC(NULL, NULL, "Adding session '%s' in session table at index %d "
                                  "and jmeter_sess_name '%s' in session table at index %d", 
                                   sess_name, idx, jmx_sess_name, idx);
	    if ((gSessionTable[idx].jmeter_sess_name = copy_into_big_buf(jmx_sess_name, 0)) == -1) {
              write_log_file(NS_SCENARIO_PARSING, "Failed to add session '%s' in session table for jmeter"
                                   " type script for group '%s'", sess_name, RETRIEVE_BUFFER_DATA(runProfTable[rnum].scen_group_name));
	      NS_EXIT(-1, CAV_ERR_1000018, jmx_sess_name);
	    }
          }
          NSDL2_MISC(NULL, NULL, "Adding %s in session table at index %d", sess_name, idx);
	  if ((gSessionTable[idx].sess_name = copy_into_big_buf(sess_name, 0)) == -1) {
            write_log_file(NS_SCENARIO_PARSING, "Failed to add session '%s' in session table for group '%s'",
                             sess_name, RETRIEVE_BUFFER_DATA(runProfTable[rnum].scen_group_name));
            NS_EXIT(-1, CAV_ERR_1000018, sess_name);
	  }
          NSDL2_MISC(NULL, NULL, "Adding %s in session table at index %d", grp_proj_name, idx);
          if ((gSessionTable[idx].proj_name = copy_into_big_buf(grp_proj_name, 0)) == -1) {
           write_log_file(NS_SCENARIO_PARSING, "Failed to add project name %s for group '%s'", grp_proj_name,
                     RETRIEVE_BUFFER_DATA(runProfTable[rnum].scen_group_name));
            NS_EXIT(-1, CAV_ERR_1000018, grp_proj_name);
          }
          NSDL2_MISC(NULL, NULL, "Adding %s in session table at index %d", grp_subproj_name, idx);
          if ((gSessionTable[idx].sub_proj_name = copy_into_big_buf(grp_subproj_name, 0)) == -1) {
            write_log_file(NS_SCENARIO_PARSING, "Failed to add sub project name %s for group '%s'", grp_subproj_name,
                            RETRIEVE_BUFFER_DATA(runProfTable[rnum].scen_group_name));
            NS_EXIT(-1, CAV_ERR_1000018, grp_subproj_name);
          }
          gSessionTable[idx].sess_norm_id = get_norm_id_for_session(sess_name_with_proj_subproj);
	}

        /*if(global_settings->protocol_enabled & JMETER_PROTOCOL_ENABLED)
        {
          NSDL2_MISC(NULL, NULL, "Adding jmeter_sess_name '%s' in runproftable at index %d", 
                                  jmx_sess_name, rnum);
          if ((gSessionTable[idx].jmeter_sess_name = copy_into_big_buf(jmx_sess_name, 0)) == -1) {
            NSTL1_OUT(NULL, NULL,  "read_scripts_glob_vars(): Failed to copy into big buffer\n");
            return -1;
          }
        }*/

	//printf("gSessionTable[idx].sess_name= %s\n", gSessionTable[idx].sess_name);
  	runProfTable[rnum].sessprof_idx = idx;
        if (global_settings->use_prof_pct == PCT_MODE_PCT) { /* Use multiples of 100 in case of pct */
          //runProfTable[rnum].quantity = (int)(pct * 100);
          runProfTable[rnum].percentage = (pct * 100.0);
        } else {
          int grp_type = get_group_mode(rnum);
          
          if (grp_type == TC_FIX_USER_RATE) { /* FSR */
            /* Multiple of 1000 */
            runProfTable[rnum].quantity = compute_sess_rate(sgrp_value);
            global_settings->vuser_rpm += runProfTable[rnum].quantity;
          } else {
            runProfTable[rnum].quantity = (int) pct;
            global_settings->num_connections += runProfTable[rnum].quantity;
          }
        }

	//global_settings->num_connections += pct; /* This will be irrelevant in Mixed mode scenarios */
        /* Changes done for Netomni, by-pass code for generator mode
         * TODO: When running non controller mode then need to check for less than only.
         * as we need to allow 0 in standalone and ignore the group
         * 
         * Basic check for both stand alone and controller mode 
         * pct cannot be less than zero
        */ 
	if (pct < 0) {
          write_log_file(NS_SCENARIO_PARSING, "Percentage value (%f) is not valid (less than 0) for group '%s'", pct,
                         RETRIEVE_BUFFER_DATA(runProfTable[rnum].scen_group_name));
          NS_KW_PARSING_ERR(buf, 0, err_msg, SGRP_USAGE, CAV_ERR_1011306, "Invalid value of pct or num user for Sceanrio Group. "
                            "Value must be greater than zero.");
	//  NSTL1_OUT(NULL, NULL,  "Invalid value (%f) of pct or num user. Need to be > 0\n", pct);
	 // return -1;
	}
       

	if (sg_fields == 6) {
	  int clusttab_idx;
	  if ((clusttab_idx = find_clust_idx(cluster_id)) == -1) {
	    if (create_clust_table_entry(&clusttab_idx) == -1) {
	      NS_EXIT(-1, "Error in creating cluster table Entry");
	    }
	    //clustTable[clusttab_idx].cluster_id = cluster_id;
	    if ((clustTable[clusttab_idx].cluster_id = copy_into_big_buf(cluster_id, 0)) == -1) {
              NS_EXIT(-1, CAV_ERR_1000018, cluster_id);
	    }
	  }
	  runProfTable[rnum].cluster_id = clusttab_idx;
	 } else
	  runProfTable[rnum].cluster_id = DEFAULT_CLUST_IDX;
      } else if (!strncasecmp(buf, "GROUP_VARS", strlen("GROUP_VARS"))) {
	if (strlen(gbuf)) {
	  NSTL1_OUT(NULL, NULL,  "read_scripts_glob_vars(): GROUP_VARS entries can only be used once in the conf file");
          write_log_file(NS_SCENARIO_PARSING, "GROUP_VARS entries can only be used once in the scenario conf file");
	  return -1;
	}

	if (strchr(buf, '\n'))
	  *(strchr(buf, '\n')) = '\0';

 	strcpy (gbuf, buf);
      } 
      }
    }

  /* For per group parse */
  //rewind(conf_file);
  /* Copy global settings to each group */
  for (i = 0; i < total_runprof_entries; i++) {
    memcpy(&runProfTable[i].gset, group_default_settings, sizeof(GroupSettings));
    runProfTable[i].gset.per_grp_static_host_settings.static_host_table = NULL;
    runProfTable[i].gset.per_grp_static_host_settings.total_static_host_entries = 0;
    runProfTable[i].gset.per_grp_static_host_settings.max_static_host_entries = 0;

    strcpy(runProfTable[i].runlogic, runlogic_all_buf);
  }
 /*
  * check for the case where
  * grp != ALL - which is done during pass2 normally. we're doing it early here
  * just for this keyword
  * the values set for pass1 (if the keyword value applies to all ALL groups)
  * have already been copied to gset just above this
  */
  parse_kw_needed_before_script_parse(conf_file);


/*   while (fgets(buf, MAX_DATA_LINE_LENGTH, conf_file) != NULL) { */
/*     //if (strncasecmp(keyword, "G_", strlen("G_")) == 0) { */
/*     if ((num = sscanf(buf, "%s %s", keyword, text)) < 2) {  */
/*       continue; */
/*     } */
/*     if (strncasecmp(keyword, "G_", strlen("G_")) == 0) { */
/*       parse_group_keywords(buf, 2); */
/*     } */
/*   } */
  
    CLOSE_FP(conf_file);

  //Now Process cluster vars and groups vars, if they were present in config file
  if (strlen(cbuf)) { //cluseter var defined
	buf_ptr = cbuf + strlen("CLUSTER_VARS");
	for (tok = strtok(buf_ptr, " "), get_filename = 1; tok; tok = strtok(NULL, " ")) {
	  if (get_filename) {
	    strcpy(clustval_filename, tok);
	    get_filename = 0;
	    continue;
	  } else {
	    if (create_clustervar_table_entry(&rnum) != SUCCESS) {
	      NSTL1_OUT(NULL, NULL,  "read_scripts_glob_vars(): Error in creating cluster var table entry\n");
              write_log_file(NS_SCENARIO_PARSING, "Failed to create cluster entry for group '%s'",
               RETRIEVE_BUFFER_DATA(runProfTable[i].scen_group_name));
	      return -1;
	    }
	    if ((clustVarTable[rnum].name = copy_into_big_buf(tok, 0)) == -1) {
               NSTL1_OUT(NULL, NULL,  "read_scripts_glob_vars(): Failed to copy into big buffer\n");
	      return -1;
	    }
	  }
	}
	if (input_clust_values(clustval_filename) == -1) {
	  NSTL1_OUT(NULL, NULL,  "read_scripts_glob_vars(): Error in reading cluster values from file %s\n", clustval_filename);
	  return -1;
	}
  }
  if (strlen(gbuf)) { //group vars defined
	buf_ptr = gbuf + strlen("GROUP_VARS");

	for (tok = strtok(buf_ptr, " "), get_filename = 1; tok; tok = strtok(NULL, " ")) {
	  if (get_filename) {
	    strcpy(groupval_filename, tok);
	    get_filename = 0;
	    continue;
	  } else {
	    if (create_groupvar_table_entry(&rnum) != SUCCESS) {
	      NSTL1_OUT(NULL, NULL,  "read_scripts_glob_vars(): Error in creating group var table entry\n");
	      return -1;
	    }
	    if ((groupVarTable[rnum].name = copy_into_big_buf(tok, 0)) == -1) {
              NSTL1_OUT(NULL, NULL,  "read_scripts_glob_vars(): Failed to copy into big buffer\n");
	     return -1;
	    }
	  }
	}
	if (input_group_values(groupval_filename) == -1) {
         NSTL1_OUT(NULL, NULL,  "read_scripts_glob_vars(): Error in reading group values from file %s\n", groupval_filename);
	  return -1;
	}
  }
  #ifdef CAV_MAIN
  if(g_monitor_status == -1)
     return -1;
  #endif
  return 0;
}

//reads keywords from vendor_default file
void read_default_file()
{
  FILE *fp;

  NSDL2_SCHEDULE(NULL, NULL, "Method called");
  if (!global_settings->read_vendor_default)
	return;

  if ((fp = fopen(DEFAULT_DATA_FILE, "r")) == NULL) {
    NS_EXIT(-1, CAV_ERR_1000006, DEFAULT_DATA_FILE, errno, nslb_strerror(errno));
  }

  read_keywords(fp, 0);
  CLOSE_FP(fp);
}

//this method reads the keywords from site_keywords.default file 
#if 0
void read_default_keyword_file(char *file_name) 
{
  FILE *fp;

  NSDL2_SCHEDULE(vptr, cptr, "Method called");

  if ((fp = fopen(file_name, "r")) == NULL) {
    NSDL2_SCHEDULE(vptr, cptr, "Error in opening %s.", file_name);
    return;
  }

  NSDL2_SCHEDULE(vptr, cptr, "Reading keywords from %s file.", file_name);

  read_keywords(fp, 1);
  CLOSE_FP(fp);
}
#endif

void read_conf_file(char *filename)
{
  FILE *fp;
  //struct timeval want_time;

  NSDL2_SCHEDULE(NULL, NULL, "Method called, filename = %s", filename);

//  init_default_values();

  if ((fp = fopen(filename, "r")) == NULL) {
    NS_EXIT(-1, CAV_ERR_1000006, filename, errno, nslb_strerror(errno));
  }

  read_keywords(fp, 1);
 
  CLOSE_FP(fp);
  read_ip_file();

#if 0
  if (total_ip_entries) {
    create_alias_script();
  }
#endif
  user_data_check();

  if (sort_userprof_tables() == -1) {
    NS_EXIT(-1, "read_conf_file(): error in sorting the userprof table\n");
  }
/*   if (sort_runprof_tables() == -1) { */
/*     NSTL1_OUT(NULL, NULL,  "read_conf_file(): error in sorting the runprof table\n"); */
/*     exit(-1); */
/*   } */
  if (sort_sessprof_tables() == -1) {
    NS_EXIT(-1, "read_conf_file(): error in sorting the sessprof table\n");
  }
}


static void enable_gen_tr_usage(char *err_msg, char *buf)
{
  NSTL1_OUT(NULL, NULL,  "Error: Invalid value of NS_GET_GEN_DATA keyword: %s\n", err_msg);
  NSTL1_OUT(NULL, NULL,  "       Line %s\n", buf);
  NSTL1_OUT(NULL, NULL,  "  Usage: NS_GET_GEN_DATA <mode>\n");
  NSTL1_OUT(NULL, NULL,  "  Where:\n");
  NSTL1_OUT(NULL, NULL,  "        Mode: Can have two valid values :\n");
  NSTL1_OUT(NULL, NULL,  "          0 : Dont get the generator TR data.(Default)\n");
  NSTL1_OUT(NULL, NULL,  "          1 : Get the generator TR data to the controller.\n");
  NS_EXIT(-1, "%s\nUsage: NS_GET_GEN_DATA <mode>", err_msg);
}

static void kw_enable_gen_tr(char *buf) 
{
  char mode[32];
  char keyword[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH];
  int num_fields = 0;
  int mode_val = 0;

  NSDL1_PARSING(NULL, NULL, "Method called");
  num_fields = sscanf(buf, "%s %s %s", keyword, mode, tmp);

  NSDL2_PARSING(NULL, NULL, "Keyword=[%s], mode=[%s]", keyword, mode);

  if(num_fields != 2)
  {
    enable_gen_tr_usage("Need 1 field after the keyword GET_GEN_DATA", buf);
  }

  mode_val = atoi(mode); 

  if(mode_val < 0 || mode_val > 1)
    enable_gen_tr_usage("Not a valid value of mode given.", buf);
  
  global_settings->get_gen_tr_flag = mode_val;

  NSDL1_PARSING(NULL, NULL, "mode val=[%d], get_gen_tr_flag=[%hd]", mode_val, global_settings->get_gen_tr_flag);
}

static void nde_partition_overlap_time_usage(char *err_msg, char *buf)
{
  NSTL1_OUT(NULL, NULL,  "Error: Invalid value of NDE_DB_PARTITION_OVERLAP_MINS keyword: %s\n", err_msg);
  NSTL1_OUT(NULL, NULL,  "       Line %s\n", buf);
  NSTL1_OUT(NULL, NULL,  "  Usage: NDE_DB_PARTITION_OVERLAP_MINS <overlap time>\n");
  NSTL1_OUT(NULL, NULL,  "  Where:\n");
  NSTL1_OUT(NULL, NULL,  "          overlap time : Overlap Time to be used in NDE mode for applying check constraint (in mins).\n");
  NS_EXIT(-1, "%s\nUsage: NDE_DB_PARTITION_OVERLAP_MINS <overlap time>", err_msg);
}

static void kw_check_partition_overtime(char *buf)
{
  char overlap_time[32];
  char keyword[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH];
  int fields = 0;
  int overlap_time_in_mins = 0;

  NSDL1_PARSING(NULL, NULL, "Method called");
  fields = sscanf(buf, "%s %s %s", keyword, overlap_time, tmp);

  NSDL2_PARSING(NULL, NULL, "Keyword=[%s], overlap_time =[%s]", keyword, overlap_time);

  if(fields != 2)
  {
    enable_gen_tr_usage("Need 1 field after the keyword NDE_DB_PARTITION_OVERLAP_MINS", buf);
  }

  if (ns_is_numeric(overlap_time) == 0) {
    nde_partition_overlap_time_usage("Overlap time can only be integer value.", buf);
  }

  overlap_time_in_mins = atoll(overlap_time);

  if (overlap_time_in_mins < 0) {
    nde_partition_overlap_time_usage("Invalid value for Overlap Time.", buf);
  }
}


static void dbu_chunk_size_usage(char *err_msg, char *buf, int exit_flag)
{
  NSTL1_OUT(NULL, NULL,  "%s\n", err_msg);
  NSTL1_OUT(NULL, NULL,  "Line in scenario is '%s'\n", buf);
  NSTL1_OUT(NULL, NULL,  "Usage: DBU_CHUNK_SIZE <NSDBU CHUNK SIZE in BYTES> <NDDBU_CHUNK_SIZE IN BYTES>\n");
  NSTL1_OUT(NULL, NULL,  "If you want to calculate chunk size automatically, then pass value '0'\n");
  NSTL1_OUT(NULL, NULL,  "Example: <DBU_CHUNK_SIZE 1024 0>\n");
  NSTL1_OUT(NULL, NULL,  "Here chunk size in NSDBU will be 1024 Bytes.\n");
  NSTL1_OUT(NULL, NULL,  "And chunk size of NDDBU will be automatically calculated.\n");

  if(exit_flag)
    NS_EXIT(-1, "%s\nUsage: DBU_CHUNK_SIZE <NSDBU CHUNK SIZE in BYTES> <NDDBU_CHUNK_SIZE IN BYTES>", err_msg);
}

static void kw_set_dbu_chunk_size(char *buf, int old_keyword_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH] = {0};
  char nsdbu_chunk_size[64] = {0};
  char nddbu_chunk_size[64] = {0};
  char min_chunk_size_buf[64] = {0};
  char max_chunk_size_buf[64] = {0};
  int min_chunk_size =  64 * 1024 * 1024;  //64 MB
  int max_chunk_size = 512 * 1024 * 1024;  //512 MB
  int ret = 0;

  NSDL1_PARSING(NULL, NULL, "Method called");

  if(old_keyword_flag == 1)
  {
    sscanf(buf, "%s %s", keyword, nsdbu_chunk_size);
    //-1 is passed from KeywordDefinition.dat. In this case no need to print warning.
    if(atoi(nsdbu_chunk_size) != -1)
    {
      NSDL1_PARSING(NULL, NULL, "Buf = %s, This keyword is not supported now. Use DBU_CHUNK_SIZE keyword in scenario", buf);
      dbu_chunk_size_usage("Keyword NSDBU_CHUNK_SIZE is not supported now, use DBU_CHUNK_SIZE instead.", buf, 0);
    }
    return;
  }

  ret = sscanf(buf, "%s %s %s %s %s", keyword, nsdbu_chunk_size, nddbu_chunk_size, min_chunk_size_buf, max_chunk_size_buf);

  if(ret < 3)
    dbu_chunk_size_usage("Invalid number of arguments.", buf, 1);

  NSDL2_PARSING(NULL, NULL, "Keyword = [%s], nsdbu_chunk_size = [%s], nddbu_chunk_size =[%s]", 
							keyword, nsdbu_chunk_size, nddbu_chunk_size);

  if(ns_is_numeric(nsdbu_chunk_size) == 0)
    dbu_chunk_size_usage("Chunk size can only be integer value.", buf, 1);

  if(ns_is_numeric(nddbu_chunk_size) == 0)
    dbu_chunk_size_usage("Chunk size can only be integer value.", buf, 1);

  if(min_chunk_size_buf[0] != '\0' && ns_is_numeric(min_chunk_size_buf) == 0)
    dbu_chunk_size_usage("Min chunk size can only be integer value.", buf, 1);

  if(max_chunk_size_buf[0] != '\0' && ns_is_numeric(max_chunk_size_buf) == 0)
    dbu_chunk_size_usage("Max chunk size can only be integer value.", buf, 1);

  global_settings->ns_db_upload_chunk_size = atoll(nsdbu_chunk_size);
  global_settings->nd_db_upload_chunk_size = atoll(nddbu_chunk_size);
  
  if(min_chunk_size_buf[0] != '\0')
    min_chunk_size = atoll(min_chunk_size_buf);
    
  if(max_chunk_size_buf[0] != '\0')
    max_chunk_size = atoll(max_chunk_size_buf);

  if((global_settings->ns_db_upload_chunk_size != 0) &&
                (global_settings->ns_db_upload_chunk_size < min_chunk_size || 
                global_settings->ns_db_upload_chunk_size > max_chunk_size))
  {
    dbu_chunk_size_usage("NSDBU chunk size can only be between range 67108864(64 MB) " 
                             "and 536870912 (512 MB). Setting it to default", buf, 0);
    global_settings->ns_db_upload_chunk_size = 0;
  }

  if((global_settings->nd_db_upload_chunk_size != 0) &&
                (global_settings->nd_db_upload_chunk_size < min_chunk_size ||
                global_settings->nd_db_upload_chunk_size > max_chunk_size))
  {
    dbu_chunk_size_usage("NDDBU chunk size can only be between range 67108864(64 MB) " 
                             "and 536870912 (512 MB). Setting it to default", buf, 0);
    global_settings->nd_db_upload_chunk_size = 0;
  }

  NSDL1_PARSING(NULL, NULL, "ns_db_upload_chunk_size = [%lld], nd_db_upload_chunk_size = [%lld]", 
                        global_settings->ns_db_upload_chunk_size, global_settings->nd_db_upload_chunk_size);
}

static void db_upload_idle_time_usage(char *err_msg, char *buf)
{
  NSTL1_OUT(NULL, NULL,  "Error: Invalid value of NSDBU_IDLE_TIME keyword: %s\n", err_msg);
  NSTL1_OUT(NULL, NULL,  "       Line %s\n", buf);
  NSTL1_OUT(NULL, NULL,  "  Usage: NSDBU_IDLE_TIME <idle time in secs>\n");
  NSTL1_OUT(NULL, NULL,  "  Where:\n");
  NSTL1_OUT(NULL, NULL,  "          idle time : DB upload idle time for which the process thread sleeps after each chunk read.\n");
  NS_EXIT(-1, "%s\nUsage: NSDBU_IDLE_TIME <idle time in secs>", err_msg);
}

static void kw_set_db_idle_time(char *buf)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH];
  char idle_time_in_secs[64];
  int num_fields = 0;

  NSDL1_PARSING(NULL, NULL, "Method called");
  num_fields = sscanf(buf, "%s %s %s", keyword, idle_time_in_secs, tmp);

  NSDL2_PARSING(NULL, NULL, "Keyword = [%s], idle_time_in_secs = [%s]", keyword, idle_time_in_secs);

  if(num_fields != 2)
  {
    db_upload_idle_time_usage("Need 1 field after the keyword NSDBU_IDLE_TIME", buf);
  }

  if (ns_is_numeric(idle_time_in_secs) == 0) {
    db_upload_idle_time_usage("Idle Time can only be integer value.", buf);
  }

  global_settings->db_upload_idle_time_in_secs = atoi(idle_time_in_secs);

  if (global_settings->db_upload_idle_time_in_secs < 0) {
    db_upload_idle_time_usage("Invalid value for Idle Time.", buf);
  }

  NSDL1_PARSING(NULL, NULL, "db_upload_idle_time_in_secs = [%d]", global_settings->db_upload_idle_time_in_secs);
}

static void kw_set_partition_switch_delay_time(char *buf)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  int num_fields = 0;
  int partition_switch_delay_time = 0;

  NSDL1_PARSING(NULL, NULL, "Method called");
  num_fields = sscanf(buf, "%s %d", keyword, &partition_switch_delay_time);

  if(num_fields != 2)
  {
    NS_EXIT(-1, "ERROR: Need value after the keyword PARTITION_SWITCH_DELAY.Invalid line :%s", buf);
  }

  NSDL2_PARSING(NULL, NULL, "Keyword = [%s], partition_switch_delay_time = [%d]", keyword, partition_switch_delay_time);

  if(partition_switch_delay_time < 0)
  {
    NS_EXIT(-1, "ERROR: Value of keyword = [%s] should be grater than zero.", keyword);
  }
}

/* This function validate only number of fields in TABLESPACE_INFO keyword and syn_flag
 * Validation of tablespace_name is done in db_tbl_mgr
 */
void kw_set_tablespace_info(char *buf)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  int num_fields = 0;
  char tablespace_name[128] = "";
  char flag[16] = "";
  int syn_flag = 0;
  char tables_name[1024] = "";

  NSDL1_PARSING(NULL, NULL, "Method called");
  
  num_fields = sscanf(buf, "%s %s %s %s", keyword, tablespace_name, flag, tables_name);
  
  if(num_fields < 2)
  {
    NS_EXIT(-1, "ERROR: Invalid line : '%s'\n"
                    "Usages: TABLESPACE_INFO <tablespace_name> <syn flag(0/1)> <tables_name(T1,T2,...)>\n", buf);
  }
  
  if((num_fields == 2) && strcmp(tablespace_name, "-") == 0) 
    return;


  syn_flag = atoi(flag);

  if((tables_name[0] == '\0') || (syn_flag < 0 || syn_flag > 1))
  {
    NS_EXIT(-1, "ERROR: Value of syn_flag should be 0/1\nUsages: TABLESPACE_INFO <tablespace_name> <syn flag(0/1)> <tables_name(T1,T2,...)>");
  }
}

void kw_set_multidisk_path(char *buf)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  int num_fields = 0;
  char file_type[64] = "";
  char multidisk_path[2048] = "";
  int len;
  
  NSDL1_PARSING(NULL, NULL, "Method called");
  num_fields = sscanf(buf, "%s %s %s", keyword, file_type, multidisk_path);
  
  len = strlen(multidisk_path);

  if(num_fields < 2)
  {
    NS_EXIT(-1, "Invalid arguments\nUsage: MULTIDISK_PATH <file_type(raw_data/csv)> <disk_path>");
  } 
  
  NSDL2_PARSING(NULL, NULL, "Keyword = [%s], file_type = [%s], multidisk_path = [%s]", 
                                     keyword, file_type, multidisk_path);

  /* "-" is passed from Keyworddefinition.dat file */
  if(strcmp(file_type, "-") == 0)
    return;

  /* "-" is passed from Keyworddefinition.dat file */
  if(multidisk_path[0] == '\0')
    return;

  if(strcasecmp(file_type, "raw_data") == 0)
  {
    if(access(multidisk_path, W_OK) != 0)
    {
      NS_EXIT(-1, CAV_ERR_1060084, multidisk_path);
    } 
    MALLOC_AND_COPY(multidisk_path, global_settings->multidisk_rawdata_path, len+1, "Allocating memory to multidisk_rawdata_path", -1); 
  }
  else if(strcasecmp(file_type, "nslogs") == 0)
  {
    if(access(multidisk_path, W_OK) != 0)
    {
      NS_EXIT(-1, CAV_ERR_1060084, multidisk_path);
    }  
    MALLOC_AND_COPY(multidisk_path, global_settings->multidisk_nslogs_path, len+1, "Allocating memory to multidisk_nslogs_path", -1);
  }
  else if(strcasecmp(file_type, "ndlogs") == 0)
  {
    if(access(multidisk_path, W_OK) != 0)
    {
      NS_EXIT(-1, CAV_ERR_1060084, multidisk_path);
    }  
    MALLOC_AND_COPY(multidisk_path, global_settings->multidisk_ndlogs_path, len+1, "Allocating memory to multidisk_ndlogs_path", -1);
  }
  else if(strcasecmp(file_type, "processed_data") == 0)
  {
    if(access(multidisk_path, W_OK) != 0)
    {
      NS_EXIT(-1, CAV_ERR_1060084, multidisk_path);
    }  
    MALLOC_AND_COPY(multidisk_path, global_settings->multidisk_processed_data_path, len+1, "Allocating memory to multidisk_processed_data_path", -1);
  }
  else if(strcasecmp(file_type, "percentile_data") == 0)
  {
    if(access(multidisk_path, W_OK) != 0)
    {
      NS_EXIT(-1, CAV_ERR_1060084, multidisk_path);
    }  
    MALLOC_AND_COPY(multidisk_path, global_settings->multidisk_percentile_data_path, len+1, "Allocating memory to multidisk_percentile_data_path", -1);
  }
  else if(strcasecmp(file_type, "nscsv") == 0)
  {
    if(access(multidisk_path, W_OK) != 0)
    {
      NS_EXIT(-1, CAV_ERR_1060084, multidisk_path);
    }  
    MALLOC_AND_COPY(multidisk_path, global_settings->multidisk_nscsv_path, len+1, "Allocating memory to multidisk_nscsv_path", -1);
  }
  else if(strcasecmp(file_type, "ns_rawdata") == 0)
  {
    if(access(multidisk_path, W_OK) != 0)
    {
      NS_EXIT(-1, CAV_ERR_1060084, multidisk_path);
    }  
    MALLOC_AND_COPY(multidisk_path, global_settings->multidisk_ns_rawdata_path, len+1, "Allocating memory to multidisk_ns_rawdata_path", -1);
  }
  else if(strcasecmp(file_type, "harp_files") == 0)
  {
    if(access(multidisk_path, W_OK) != 0)
    {
      NS_EXIT(-1, CAV_ERR_1060084, multidisk_path);
    }  
    MALLOC_AND_COPY(multidisk_path, global_settings->multidisk_ns_rbu_logs_path, len+1, "Allocating memory to multidisk_rbu_logs_path", -1);
  }
  create_and_fill_multi_disk_path(file_type, multidisk_path);
}

static void db_upload_num_cycles_usage(char *err_msg, char *buf)
{
  NSTL1_OUT(NULL, NULL,  "Error: Invalid value of NSDBU_NUM_CYCLES keyword: %s\n", err_msg);
  NSTL1_OUT(NULL, NULL,  "       Line %s\n", buf);
  NSTL1_OUT(NULL, NULL,  "  Usage: NSDBU_NUM_CYCLES <Number of iteration cycles.>\n");
  NS_EXIT(-1, "%s\nUsage: NSDBU_NUM_CYCLES <Number of iteration cycles>", err_msg);
}

static void kw_set_db_num_cycles(char *buf)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH];
  char num_cycles[64];
  int num_fields = 0;

  NSDL1_PARSING(NULL, NULL, "Method called");
  num_fields = sscanf(buf, "%s %s %s", keyword, num_cycles, tmp);

  NSDL2_PARSING(NULL, NULL, "Keyword = [%s], num_cycles = [%s]", keyword, num_cycles);

  if(num_fields != 2)
  {
    db_upload_num_cycles_usage("Need 1 field after the keyword NSDBU_NUM_CYCLES", buf);
  }

  if (ns_is_numeric(num_cycles) == 0) {
    db_upload_num_cycles_usage("Num Cycles can only be integer value.", buf);
  }

  global_settings->db_upload_num_cycles = atoi(num_cycles);

  if (global_settings->db_upload_num_cycles < 0) {
    db_upload_num_cycles_usage("Invalid value for Num Cycles.", buf);
  }

  NSDL1_PARSING(NULL, NULL, "db_upload_idle_time_in_secs = [%d]", global_settings->db_upload_num_cycles);
}

int kw_set_nd_monitor_log(char *buff, char* err)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char argument[MAX_DATA_LINE_LENGTH];
  char temp_buff[MAX_DATA_LINE_LENGTH];
  char file_path[MAX_DATA_LINE_LENGTH];
  int LogMode = 0, num = 0;
  //global_settings->nd_monData_log.size = 1;

  num = sscanf(buff,"%s %d %s %s",keyword, &LogMode, argument, temp_buff);
  NSDL1_PARSING(NULL, NULL, "Method called. num = %d temp_buff = %s", num, temp_buff);
  if( (num > 3) || (num < 2))
  {
    NSTL1_OUT(NULL, NULL, "Invalid argument for Keyword ENABLE_MONITOR_DATA_LOG"); 
    strcpy(err, "Invalid argument for Keyword ENABLE_MONITOR_DATA_LOG");
    return -1;
  }

  if(LogMode == 1) 
    g_log_mode = 1;
  else
    g_log_mode = 0;
  
  if(g_log_mode)
  {
    strcpy(gdf_log_pattern,argument);
    strcpy(file_path, g_ns_wdir); 
    sprintf(file_path,"%s/logs/%s/ns_logs/monitor_data.log", file_path, global_settings->tr_or_partition);
    g_nd_monLog_fd = fopen(file_path, "a+");
    if(!g_nd_monLog_fd)
    {
      NSTL1_OUT(NULL, NULL, "Unable to open %s", file_path);
      sprintf(err, "Unable to open %s", file_path);
    }
  } 
  return 0;
}

int check_json_format(char *filename)
{ 
/*
    Exit with a message if the input is not well-formed JSON text.
    jc will contain a JSON_checker with a maximum depth of 20.
*/
  
  FILE *fp;
  JSON_checker jc = new_JSON_checker(20);
  int next_char;
  int line = 1;
  
  NSDL2_MON(NULL, NULL, "Method called, filename = %s", filename);

  fp = fopen(filename, "r");
  if(!fp)
  {
    NSTL1_OUT(NULL, NULL, "Not able to open the file\n");
    return -1;
  }

  while((next_char = fgetc(fp)))
  {
    if (next_char <= 0) 
      break;

    if(next_char == 10)
      ++line;
  
    if (!JSON_checker_char(jc, next_char))
    {
      NSTL1(NULL, NULL, "JSON_checker_char: syntax error at line number = %d\n", line);
      return -1;
    }
  }

  if (!JSON_checker_done(jc))
  {
    NSTL1(NULL, NULL, "JSON_checker_end: syntax error\n");
    return -1;
  }

  NSTL1(NULL, NULL, "JSON syntax correct\n");
  return 0;
}


//In parent epoll, max event count was set to 1024. There was an issue in production and we assumed that event count reached its maximum limit and parent was busy in processing those events. Now made max_event on epoll keyword configurable and setting default max event count to 10000.
int kw_set_parent_epoll_event_size(char *keyword, char *buf)
{
  char key[DYNAMIC_VECTOR_MON_MAX_LEN] = "";
  int value, num;

  NSDL2_MON(NULL, NULL, "Method called, keyword = %s, buf = %s", keyword, buf);

  num = sscanf(buf, "%s %d", key, &value);

  if((num != 2) || (value <= 0))
  {
    NSTL1(NULL, NULL, "Wrong usage of keyword %s. Buffer = %s. USAGE (PARENT_EPOLL_EVENT_SIZE <max_event>. max_event will always be more than 0. It can't be negative or 0. Setting value to 10000 and continuinig.", keyword, buf);
    g_parent_epoll_event_size = 10000;
  }
  else
    g_parent_epoll_event_size = value; 

  NSTL1(NULL, NULL, "PARENT_EPOLL_EVENT_SIZE keyword has been parsed. Values are : g_parent_epoll_event_size = %d", g_parent_epoll_event_size);

  return 0;
}


//TCP_KEEPALIVE 
int kw_set_tcp_keepalive(char *buff)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char text1[MAX_DATA_LINE_LENGTH] = "";
  char text2[MAX_DATA_LINE_LENGTH] = "";
  char text3[MAX_DATA_LINE_LENGTH] = ""; 
  char text4[MAX_DATA_LINE_LENGTH] = "";
  int num;

  num = sscanf(buff, "%s %s %s %s %s", keyword, text1, text2, text3, text4);

  g_enable_tcp_keepalive_opt = text1[0];

  if(g_enable_tcp_keepalive_opt == '0')
    return 0;

  if((num != 5) || (g_enable_tcp_keepalive_opt != '1')) 
  {
    NSTL1(NULL, NULL, "TCP_KEEPALIVE keyword is used with wrong usage. Enabling this keyword with default values. TCP_KEEPALIVE <0/1> <idle_time> <probes_send_interval> <probes_count>.\nDefault:  TCP_KEEPALIVE 1 180 10 10");
    g_enable_tcp_keepalive_opt = '1';
    g_tcp_conn_idle = 180;
    g_tcp_conn_keepintvl = 10;
    g_tcp_conn_keepcnt = 10;
 
    return 0;
  }
  
  g_tcp_conn_idle = atoi(text2);
  if(g_tcp_conn_idle == 0)
  {
    NSTL1(NULL, NULL, "Wrong input for TCP_KEEPALIVE keyword. TCP_KEEPALIVE <0/1> <g_tcp_conn_idle> <g_tcp_conn_keepintvl> <g_tcp_conn_keepcnt>. Entered value for g_tcp_conn_idle: %s. g_tcp_conn_idle is set to 180 by default.", text2);
    g_tcp_conn_idle = 180;
  }
  
  g_tcp_conn_keepintvl = atoi(text3);
  if(g_tcp_conn_keepintvl == 0)
  {
    NSTL1(NULL, NULL, "Wrong input for TCP_KEEPALIVE keyword. TCP_KEEPALIVE <0/1> <g_tcp_conn_idle> <g_tcp_conn_keepintvl> <g_tcp_conn_keepcnt>. Entered value for g_tcp_conn_keepintvl: %s. g_tcp_conn_keepintvl is set to 10 by default.", text3);
    g_tcp_conn_keepintvl = 10;
  }

  g_tcp_conn_keepcnt = atoi(text4);
  if(g_tcp_conn_keepcnt == 0)
  {
    NSTL1(NULL, NULL, "Wrong input for TCP_KEEPALIVE keyword. TCP_KEEPALIVE <0/1> <g_tcp_conn_idle> <g_tcp_conn_keepintvl> <g_tcp_conn_keepcnt>. Entered value for g_tcp_conn_keepcnt: %s. g_tcp_conn_keepcnt is set to 10 by default.", text4);
    g_tcp_conn_keepcnt = 10;
  }
 
  return 0; 
}

//MONITOR_DEBUG_LEVEL_KEYWORD

int kw_set_monitor_debug_level(char *keyword, char *buf)
{
  char key[DYNAMIC_VECTOR_MON_MAX_LEN] = "";
  int value, num;
  
  num = sscanf(buf, "%s %d", key, &value);
  
  if((num !=2) || (value < 0) || (value > 4))
  {
    NSTL1(NULL, NULL, "Wrong usage of keyword %s. Buffer = %s. USAGE (MONITOR_DEBUG_LEVEL <max_event>. max_event will always be more than 0. It can't be negative, and cant be more than 4. Setting value to 1 and continuinig.", keyword, buf);
    monitor_debug_level = 1;
  }
  else
    monitor_debug_level = value;
  
  NSTL1(NULL, NULL, "MONITOR_DEBUG_LEVEL keyword has been parsed. Values are : monitor_debug_level = %d", monitor_debug_level);   
  return 0;
}

int kw_set_test_metrics_mode(char *buff)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char keyValue[MAX_DATA_LINE_LENGTH];
  int num = 0;
  
  num = sscanf(buff,"%s %s",keyword, keyValue);

  if( num != 2 )
  {
    NSTL1_OUT(NULL, NULL, "Invalid argument for Keyword TEST_METRICS_MODE %s", buff); 
    return -1;
  }
  else
    enable_test_metrics_mode = atoi(keyValue);

  if(enable_test_metrics_mode < 0 || enable_test_metrics_mode > 1)
  {
     NSTL1(NULL, NULL, "Value for Keyword TEST_METRICS_MODE is allowed between 0 and 1. Setting enable_test_metrics_mode = [%d] default value to 0", enable_test_metrics_mode);
    enable_test_metrics_mode = 0;
  }
 
  return 0;
}

int kw_set_mssql_stats_monitor_buf_size(char *buff)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  int keyValue=0;
  int num = 0;
 
  num = sscanf(buff,"%s %d",keyword, &keyValue);

  if( num != 2 )
  {
    NSTL1_OUT(NULL, NULL, "Invalid argument for Keyword MSSQL_STATS_MONITOR_BUF_SIZE %s", buff);
    return -1;
  }
  else
  {
    if(keyValue<=64)
      g_mssql_data_buf_size=64*1024;
    else if(keyValue>=1024)
      g_mssql_data_buf_size=1024*1024;
    else
      g_mssql_data_buf_size=keyValue*1024;
  }

  return 0;
}
