/******************************************************************
 * Name    :    ns_parent.c
 * Purpose :    This file contains methods for NetStorm parent
 * Note    :
 * Author  :    Archana
 * Intial version date:    07/04/08
 * Last modification date: 08/04/08
 *****************************************************************/

//_GNU_SOURCE is defined for O_LARGE_FILE
#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>

#include <sys/wait.h>
#include <libgen.h>
#include <locale.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#ifdef USE_EPOLL
  #include <sys/epoll.h>
#endif
#include <regex.h>
#include <libpq-fe.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>
#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "nslb_util.h"
#include "nslb_ssl_lib.h"
#include "nslb_multi_thread_trace_log.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"
#include "nslb_time_stamp.h"
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
#include "ns_msg_com_util.h" 
#include "output.h"
#include "ns_string.h"
#include "ns_sock_list.h"
#include "src_ip.h"
#include "unique_vals.h"
#include "divide_users.h" 
#include "divide_values.h" 
#include "child_init.h"
#include "eth.h"
#include "wait_forever.h"
#include "ns_master_agent.h"
#include "ns_gdf.h"
#include "server_stats.h"
#include "ns_log.h"
#include "ns_cpu_affinity.h"
#include "ns_child.h"
#include "ns_parent.h"
#include "ns_health_monitor.h"
#include "ns_summary_rpt.h"
#include "ns_goal_based_sla.h"
#include "ns_goal_based_run.h"
#include "ns_wan_env.h"
#include "ns_url_req.h"
#include "ns_check_monitor.h"
#include "ns_pre_test_check.h"
#include "ns_debug_trace.h"
#include "ns_user_monitor.h"
#include "ns_alloc.h"
#include "ns_child_msg_com.h"
#include "ns_percentile.h"
#include "ns_event_log.h"
#include "ns_custom_monitor.h"
#include "smon.h"
#include "ns_parse_scen_conf.h"
#include "ns_cookie.h"
#include "ns_auto_cookie.h"
#include "ns_replay_access_logs_parse.h"
#include "ns_schedule_ramp_up_fcu.h"
#include "ns_schedule_ramp_up_fsr.h"
#include "ns_sock_com.h"
#include "ns_schedule_phases_parse_validations.h"
#include "ns_schedule_phases_parse.h"
#include "ns_schedule_phases.h"
#include "ns_global_dat.h"
#include "ns_dynamic_vector_monitor.h"
#include "ns_server_admin_utils.h"
#include "ns_smtp_parse.h"
#include "ns_keep_alive.h"
#include "ns_static_use_once.h"
#include "nslb_hash_code.h"
#include "nslb_date.h"
#include "nslb_cav_conf.h"
#include "ns_event_filter.h"
#include "ns_event_id.h"
#include "ns_common.h"
#include "ns_http_cache_reporting.h"
#include "dos_attack/ns_dos_attack_reporting.h"
#include "ns_http_cache_table.h"
#include "ns_vuser_trace.h"
#include "ns_nethavoc_handler.h"

#include "tr069/src/ns_tr069_lib.h"
#include "tr069/src/ns_tr069_data_file.h"
#include "ns_auto_fetch_parse.h"
#include "ns_url_hash.h"
#include "ns_netstorm_diagnostics.h"
#include "ns_runtime_changes.h"
#include "ns_ftp.h"
#include "ns_ldap.h"
#include "ns_imap.h"
#include "ns_jrmi.h"
#include "ns_lps.h"
#include "deliver_report.h"
#include "ns_http_hdr_states.h"
#include "ns_parse_netcloud_keyword.h"
#include "ns_wan_env.h"
#include "ns_h2_reporting.h"
#include "nslb_netcloud_util.h" 
#if OPENSSL_VERSION_NUMBER < 0x10100000L
  #include <openssl/ssl_cav.h>
#endif

#include "nslb_http_state_transition_init.h"
#include "nslb_db_util.h"
//#include "nslb_license.h"
#include "ns_http_process_resp.h"
#include "ns_dynamic_hosts.h"
#include "netomni/src/core/ni_user_distribution.h"
#include "netomni/src/core/ni_scenario_distribution.h"
#include "ns_page_dump.h"
#include "nslb_hm_disk_space.h"
#include "nslb_hm_disk_inode.h"
#include "ns_proxy_server.h"
#include "ns_proxy_server_reporting.h"
#include "ns_replay_access_logs_parse.h"
#include "ns_network_cache_reporting.h"
#include "ns_dns_reporting.h"
#include "ns_monitoring.h"
#include "ns_license.h"
#include "ns_njvm.h"
#include "ns_trace_level.h"
#include "ns_ndc.h"
#include "nslb_signal.h"
#include "nslb_server_admin.h"
#include "ns_session.h"
#include "nia_fa_function.h"
#include "nslb_db_upload_common.h"
#include "ns_send_mail.h"
#include "ns_rbu.h"
#include "ns_group_data.h"
#include "ns_rbu_page_stat.h"
#include "ns_page_based_stats.h"
#include "ns_runtime_changes_quantity.h"
#include "ns_ip_data.h"
#include "ns_inline_delay.h"
#include "ns_page_think_time.h"
#include "ns_trans_parse.h"
#include "nslb_netcloud_util.h"
#include "db_aggregator.h"
#include "ns_replay_db_query.h"
#include "ns_runtime_runlogic_progress.h"
#include "ns_ndc_outbound.h"
#include "ns_server_ip_data.h"
#include "ns_param_override.h"
#include "ns_websocket_reporting.h"
#include "ns_schedule_fcs.h"
#include "ns_exit.h"
#include "ns_appliance_health_monitor.h"
#include "ns_rbu_domain_stat.h"
#include "ns_jmeter.h"
#include "ns_custom_monitor_RDnew.h"
#include "ns_rte_api.h"
#include "ns_xmpp.h"
#include "ns_fc2.h"
#include "ns_test_monitor.h"
#include "ns_test_init_stat.h"
#include "ns_data_handler_thread.h"
#include "nslb_server_admin.h"
#include <fcntl.h>
#include "v1/topolib_structures.h"
#include "ns_monitor_profiles.h"
#include "ns_runtime.h"
#include "ns_mon_log.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"
#include "ns_handle_alert.h"
#include "ns_tsdb.h"
#include "cav_tsdb_interface.h"

#include "ns_socket.h"
#include "ns_file_upload.h"
#include "ns_svr_ip_normalization.h"
#include "ns_write_rtg_data_in_db_or_csv.h"

//RESERVED_FDS
#define MONITOR_CMD_LENGTH 100
/* How many file descriptors to not use. */
#define RESERVED_FDS 3
#define CPU_HEALTH_PORT 7999
//#define DEFAULT_NUM_CONN 50
#define PARENT parent_pid == getpid()
#define PARENT_PORT_NUMBER 1026

#define ULTIMATE_MAX_NUM_STAB_RUNS 100

#define HTTP_STATE_MODEL_FILE_NAME "etc/ns_http_hdr_state_model.txt"

// Listen Purpose
#define PARENT_LISTEN_PORT 1
#define DH_LISTEN_PORT     2


/*bug 78684   */
extern int trace_log_fd;
extern int script_execution_log_fd;

//extern char qty_msg_buff[RTC_QTY_BUFFER_SIZE];
extern char *extract_header_from_event(char *event, int *severity);
extern inline void init_form_api_escape_replacement_char_array();

//extern int start_accepting_metrics; //defined in wait_forever.c
static pid_t parent_pid;
extern int total_montable_entries;
//extern int num_connected;
extern int g_rtc_msg_seq_num;

//#### File globals ####
static u_ns_ts_t start_time;
int last_run;
static avgtime* end_results = NULL;
static cavgtime* c_end_results = NULL;
static int original_num_process;
static int add_delay=0;
int original_progress_msecs;
int g_debug_script=0;

char g_test_user_name[128 +1];
char g_rtc_owner[64];
char g_test_user_role[32 + 1];

int g_testrun_idx;
// This is used for just compileing the script

// Run mode option passed as -o option
// Currently used only for compilation of script used by script GUI using nsi_compile_script shell
// In future, we can use for testmonitor, check scenario


int run_mode_option = 0; //Default value is not compile script
int ni_make_tar_option = CREATE_TAR_AND_CONTINUE;//Default value is to make tar and continue
//#### End: File globals ####
//Production Monitoring
int start_test_min_dur = 0;
/* master_ip and master_port are used only by client */
char master_ip[32]; // This is used to store master IP in string format for nslb_udp_client(). Used in wait_forever.c
unsigned short master_port;
unsigned short dh_master_port;
int g_gui_bg_test = 0;

/* BugId: 39240 - to resolve it we need to send machine configuration to generator so that generators can 
   		  decide if rpf.csv does not exist then generator die or not, If machine is NVSM then die otherwise dont die 
*/
char master_sub_config[10]; //This is set with controller g_cavinfo.SUB_CONFIG in /home/cavisson/etc/cav.conf if set otherwise NA
char g_enable_test_init_stat = 0;
//#ifndef CAV_MAIN
char g_test_inst_file[64+1];
//#else
//__thread char g_test_inst_file[64+1];
//#endif
FILE *g_instance_fp;
//dlog collector id
pid_t dlog_collector_id = -1;
/*
  static void
  handle_parent_rtc_msg( int sig )
  {
  //printf("%d: sigterm rcd\n", getpid());
  printf("NetStorm master processe got killed. Exting.\n");
  //sigterm_received = 1;
  //flush_logging_buffers();
  exit(0);
  }
*/

int  g_set_args_type = KW_NONE;
char *g_set_args_value;

/* scratch buf for each nvm to use in pagedump and user trace. We can use it for other purposes also in future */
#ifndef CAV_MAIN
char *ns_nvm_scratch_buf = NULL;
int ns_nvm_scratch_buf_size = 0;
int ns_nvm_scratch_buf_len = 0;  //Filled data length

char *ns_nvm_scratch_buf_trace = NULL;
int ns_nvm_scratch_buf_size_trace = 0;
#else
__thread char *ns_nvm_scratch_buf = NULL;
__thread int ns_nvm_scratch_buf_size = 0;
__thread int ns_nvm_scratch_buf_len = 0;  //Filled data length

__thread char *ns_nvm_scratch_buf_trace = NULL;
__thread int ns_nvm_scratch_buf_size_trace = 0;
#endif

//Scratch buffer for monitors to send the cm_init request. It is a global buffer. Currently used only to send cm_init request for monitors
int monitor_scratch_buf_len = 0;
char *monitor_scratch_buf = NULL;
int monitor_scratch_buf_size = 0;
char ip_and_controller_tr[128+1];

//NC: Added counter to set RTC message count when received from generators
//int rec_all_gen_msg = 0;

//fd use for Net Diagnostic
Msg_com_con ndc_mccptr;
Msg_com_con ndc_data_mccptr;

//fd use for lps 
Msg_com_con lps_mccptr;

//To update last partition summary.top when testrun ends
u_ns_ts_t partition_start_time;

//NC:
//extern generatorList *gen_detail;
//extern int tot_gen_entries;

/*For creation of DB tables*/

pthread_t db_table_thread_id;
//pthread_attr_t db_table_attr;
int *db_thread_ret_val = NULL;
char controller_ns_build_ver[128];
char g_controller_wdir[512 + 1];
char g_controller_testrun[32 + 1];
__thread RTCData *rtcdata;  //RTC data pointer

/*Global variable table shared between Data and Control connection. And it will be handled by pthread_setspecific and pthread_getspecific*/
//pthread_key_t glob_var_key;
Data_Control_con g_data_control_var;
pthread_mutex_t glob_var_mutex_key = PTHREAD_MUTEX_INITIALIZER;

/*Global variable for skipping data from controller to generator.*/
int g_do_not_ship_test_assets = 0;

/*Global variable using for failed malloc or realloc new or old size*/
__thread int ns_cav_err_new_ds_size;
__thread int ns_cav_err_old_ds_size;
int is_goal_based = 0; // to control goal based scenario
extern void kill_children(void);

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Name         : realloc_scratch_buffer()
  Purpose      : Reallocating Scratch buffer size for Master & Client
  Bug (95203)  : In case of NetCloud test, we are compressing Progress Report
                  as well as Percentile data.
                  When Avgsize is BIG (~5MB >) then library function
                  nslb_compress() takes too much time (~5min) to compress DUE TO
                  reallocating memmory by a small delta size (~4K)
  Fix          : To resolve this bug, now allocating a BIG size on Master and
                  on controller.
                  => 20 MB on Master as here decompression performed
                  => 10 MB on Client as here compression performed
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
#define MASTER_SCRATCH_BUF_INIT_SIZE             20971520     //20 MB
#define CLIENT_SCRATCH_BUF_INIT_SIZE             10485760     //10 MB
inline void realloc_scratch_buffer(int size) {
  if (ns_nvm_scratch_buf_size < size) {
    MY_REALLOC(ns_nvm_scratch_buf, size,
                 "Re-alloc Scratch buffer", -1);

    ns_nvm_scratch_buf_size = size;
  }
}

/* This functions set the default valuse of epoll related fields keyword is not given*/
static inline void init_default_parent_epoll() {

  NSDL2_PARENT(NULL, NULL, "Method called");

  if(global_settings->parent_epoll_timeout <= 0) {
    //In case of controller we need to have epoll timeout greater than generator, hence adding 10 sec to epoll timeout
    if (loader_opcode == MASTER_LOADER)
      global_settings->parent_epoll_timeout = (3 * global_settings->progress_secs) + 10000;
    else //In case of standalone and generator.
      global_settings->parent_epoll_timeout = 3 * global_settings->progress_secs;
    /*count = 5 minute / epoll timeout*/
    global_settings->parent_timeout_max_cnt = (5*60*1000 / global_settings->parent_epoll_timeout) % 127;  // parent_timeout_max_cnt is char
    global_settings->dump_parent_on_timeout = 0;
  }
}

/**
 * This signal is used to update settings at run time.
 * Here, parse the new file runtime_changes.conf and update 
 * the settings for each keyword. If file does not exists we 
 * simply ignore it and do nothing.
 *
 * Note: We assume that the runtime_changes.conf is according to
 *       the new keywords (i.e. already migrated)
 */

/* 

Following signals are used for run time changes

SIGUSR2 -> From nsi_runtime_update tool to parent
SIGTRMIN -> From Parent to NVM for syncronization of shared memory updation by parent 

Note: Due to use of singnals, this will NOT work in master mode.
In future, we will change it to use TCP/IP
*/

/* Handle one or more NVM in pause state after sending finish report */
int flag_run_time_changes_called;
void handle_parent_rtc_msg(char *tool_msg)
{
  int only_monitor_runtime_change = 0;
  
  NSDL2_MESSAGES(NULL, NULL, "ns_parent_state = %d", ns_parent_state);

  if (loader_opcode == MASTER_LOADER)
  {
    process_pause_for_rtc(RTC_PAUSE, g_rtc_msg_seq_num, NULL);
  } 
  else 
  {
    //If only monitors runtime changes are done then no need to pause the nvm, as it doennot depemd on nvm's.
    if (only_monitor_runtime_change)
    {
      NSTL1(NULL, NULL, "Skipping Pause in case of only monitor related runtime changes.");
      apply_runtime_changes(1);
    }
    else
    { //In case of NS parent we need to pause all NVMs.
      NSTL1(NULL, NULL, "RTC: Send message to all NVMs to stop processing");
      int len;
      int rtc_opcode;
      char *msg;
      memcpy(&len, tool_msg, 4);
      memcpy(&rtc_opcode, tool_msg + 4, 4);
      memset(rtcdata->msg_buff, 0, RTC_QTY_BUFFER_SIZE);
      msg = tool_msg + 8; // skip -> 4 byte for msg len and 4 byte for opcode
      // need to retreive user in case of sync point
      if (rtc_opcode == APPLY_PHASE_RTC) {
        /* Get the owner of RTC */
        memcpy(g_rtc_owner, msg, 64);
        msg += 64; 
        len -= 64; // reduce length by username
      }
      strncpy(rtcdata->msg_buff, msg, (len - 4));
      NSDL2_MESSAGES(NULL, NULL, "In case of NS: rtcdata->msg_buff = %s", rtcdata->msg_buff);
      process_pause_for_rtc(RTC_PAUSE, g_rtc_msg_seq_num, NULL);
    }
  }
}

static void
handle_parent_sigint( int sig )
{
  NSDL2_MESSAGES(NULL, NULL, "Method called, Received sig = %d", sig);
  print2f_always(rfp, "Received Ctrl-C from user. Stopping test ...\n");
  sigterm_received = 1;
}

static void handle_parent_sigpipe(int sig) {
  NSDL2_MESSAGES(NULL, NULL, "Method called. sig = %d", sig);
}

//For parent or maser
// Parent - On getting SIGUSR1 to stop test run
// Client - When it receives FINISH_TEST_RUN_MESSAGE from master. sig is passed -1
void handle_parent_sigusr1( int sig )
{
  int i;
  //int status;

  NSDL2_MESSAGES(NULL, NULL, "Method called. sig = %d", sig);
  print2f_always(rfp, "Received signal for stopping the test. Stopping test ...\n");

  sigterm_received = 1;         /* Setting it so schedulig stops */
  // In following method we are checking for test_runphase_duration also for 0
  update_test_runphase_duration();
  //clear_shared_mem(shm_base, total_num_shared_segs);
  if(loader_opcode == MASTER_LOADER)
  {
    send_msg_to_all_clients(FINISH_TEST_RUN_MESSAGE, 0);
  }
  else if ((loader_opcode == CLIENT_LOADER) && (sig == SIGUSR1))
  {
    NSTL1(NULL, NULL, "Generator parent got SIGUR1, So send send_end_test_msg()");
    send_end_test_msg(NULL, 0);
  }
  else // Parent in standalone mode got SIGUSR1 or client recieved FINISH_TEST_RUN from master
  {
    for (i=0;i<global_settings->num_process;i++)
    {
      NSDL3_MESSAGES(NULL, NULL, "Sending SIGINT to child id = %d, pid = %d", i, v_port_table[i].pid);
      if (kill(v_port_table[i].pid, SIGINT))
        perror("kill");
        //waitpid(v_port_table[i].pid, &status, 0);
    }
  }
  //exit(0);
}

void
handle_parent_sickchild( int sig )
{
  int ret_pid, status;

  if (ns_parent_state == NS_PARENT_ST_TEST_OVER)  //exiting
    return;

  /* Check for nsa_log_mgr is it alive*/
  /* We have to continue if nsa_log_mgr dies, as all events will be logged at own process level*/

  // -1 meaning wait for any child process.
  // create options var if multi
  ret_pid = waitpid(-1, &status, WNOHANG);

  if(ret_pid == 0)
    return;

  /*NSTL1_OUT(NULL, NULL, "Parent got sickchild from pid '%d'. Netstorm Log Mgr pid is '%d', Netstorm Logging Writer pid is '%d'" 
                  " and event_logger_port_number is '%d'.", ret_pid, nsa_log_mgr_pid, writer_pid, event_logger_port_number);*/
  NSTL1(NULL, NULL, "Parent got sickchild from pid '%d'. Netstorm Log Mgr pid is '%d', Netstorm Logging Writer pid is '%d'"
                    " and event_logger_port_number is '%d'.", ret_pid, nsa_log_mgr_pid, writer_pid, event_logger_port_number);
  // In NC WAN test we found parent pid = -1 and hence test stop, so handle pid = -1
  if(ret_pid < 0)
    return;

  /*NC: In JCPenny on receiving sick child generator parent updated event logger port to 0, 
    but on genreator event logger is it not spawn hence parent should not update port. 
    Here NVMs were unable to connect to event logger as they found its port 0 */
  if (loader_opcode != CLIENT_LOADER)
  {
    if(ret_pid == nsa_log_mgr_pid)
    {
      //NSTL1_OUT(NULL, NULL, "NetStorm Log Manager process got killed. Process exit status = %d.", status);
      NSTL1(NULL, NULL, "NetStorm Log Manager process got killed. Process exit status = %d.", status);
      nsa_log_mgr_pid = -1;
      event_logger_port_number = 0;
    }
  }
  if(ret_pid == writer_pid)
  {
    //NSTL1_OUT(NULL, NULL, "nsa_logger process got killed. Process exit status = %d.", status);
    NSTL1(NULL, NULL, "nsa_logger process got killed. Process exit status = %d.", status);
    writer_pid = -1;
  }

  if(ret_pid == nia_file_aggregator_pid)
  {
    //NSTL1_OUT(NULL, NULL, "nia_file_aggregator got killed. Process exit status = %d.", status);
    NSTL1(NULL, NULL, "nia_file_aggregator got killed. Process exit status = %d.", status);
    nia_file_aggregator_pid = -1;
  }
  
  if(ret_pid == db_aggregator_pid)
  {
    //NSTL1_OUT(NULL, NULL, "db_aggregator got killed. Process exit status = %d.", status);
    NSTL1(NULL, NULL, "db_aggregator got killed. Process exit status = %d.", status);
    db_aggregator_pid = -1;
  }

  if(ret_pid == nia_req_rep_uploader_pid)
  {
    //NSTL1_OUT(NULL, NULL, "nia_req_rep_uploader got killed. Process exit status = %d.", status);
    NSTL1(NULL, NULL, "nia_req_rep_uploader got killed. Process exit status = %d.", status);
    nia_req_rep_uploader_pid = -2;
  }
  return;
}

#if 0

/* This handler used by both parent and master */
void
handle_parent_sickchild( int sig )
{
  NSDL2_MESSAGES(NULL, NULL, "Method called, Received sig = %d, ns_parent_state = %d, ns_sickchild_pending = %d", sig, ns_parent_state, ns_sickchild_pending);
  if (ns_parent_state == NS_PARENT_ST_TEST_OVER)  //exiting
    return;

  /* Check for nsa_log_mgr is it alive*/
  /* We have to continue if nsa_log_mgr dies, as all events will be logged at own process level*/
  if(chk_nsa_log_mgr()) {
    return;
  }

  //dirty clean
  //printf("Recived a sick child signal : (pid=%d parent_started=%d sick_pending=%d)\n", getpid(), ns_parent_started, ns_sickchild_pending);
  //printf("nun_child=%d pid=%d\n", global_settings->num_process, v_port_table[0].pid);
  if (ns_parent_state == NS_PARENT_ST_INIT) 
  {
    if (!ns_sickchild_pending) 
    {
      printf("Received a sick child signal\n");
      ns_sickchild_pending = 1;
    }
    return;
  }
  printf("Received a sick child signal\n");
#ifndef NS_PROFILE
  kill_all_children((char *)__FUNCTION__, __LINE__, __FILE__);
#endif
  parent_save_data_before_end();
  printf("Exiting: %d\n", getpid());
  exit(-1);
}

#endif

void
handle_parent_sigrtmin1( int sig )
{
  int i;

  NSDL2_MESSAGES(NULL, NULL, "Method called. sig = %d", sig);
    
  if(end_test_run_mode == 1) {
    NSDL2_MESSAGES(NULL, NULL, "Ignoring signal if received more than once");
    return; 
  }

  print2f_always(rfp, "Received signal for stopping the test immediately. Stopping test ...\n");

  sigterm_received = 1;         /* Setting it so schedulig stops */
  end_test_run_mode = 1;   /*Set first time when signal received, next time signal will be ignored*/

  // In following method we are checking for test_runphase_duration also for 0
  update_test_runphase_duration();

  if(loader_opcode == MASTER_LOADER)
  {
    /* In case of NetCloud we need to add new message */
    send_msg_to_all_clients(FINISH_TEST_RUN_IMMEDIATE_MESSAGE, 0);
  }
  else if ((loader_opcode == CLIENT_LOADER) && (sig == SIGRTMIN+1))
  {
    NSTL1(NULL, NULL, "Generator parent got SIGRTMIN+1, So send send_end_test_msg()");
    send_end_test_msg(NULL, 0);
  }
  else // Parent in standalone mode got SIGRTMIN+1 or client recieved FINISH_TEST_RUN from master
  {
    for (i=0;i<global_settings->num_process;i++)
    {
      NSDL3_MESSAGES(NULL, NULL, "Sending SIGRTMIN+1 to child id = %d, pid = %d", i, v_port_table[i].pid);
      if (kill(v_port_table[i].pid, SIGRTMIN+1))
        perror("kill");
    }
  }
}

static void handle_parent_sigrtmin3( int sig )
{
 dump_monitor_tables=1; 
}

void get_testid_and_set_debug_option(){
  /* set tr_or_partition initially testidx because in some keyword (eg: STATIC_URL_HASH_TABLE_OPTION, etc )parsing 
   * we are doing event_log on any error during parsing and event_log we are using this variable 'tr_or_partition' */
  MY_MALLOC(global_settings->tr_or_partition, TR_OR_PARTITION_NAME_LEN + 1, "global_settings->tr_or_partition", -1);
  snprintf(global_settings->tr_or_partition, TR_OR_PARTITION_NAME_LEN, "TR%d", testidx);
  global_settings->ns_trace_level = 1;
#ifdef NS_DEBUG_ON
  // So that if debug is on than it log all the debug call untill keyword is not parsed.
  group_default_settings->debug = 0xFFFFFFFF;
  group_default_settings->module_mask = MM_ALL;
#endif
}

/*This function creates NS_WDIR/logs/testrun/cav.conf file and writes contents of /home/cavisson/etc/cav.conf file into it*/
static void ns_write_cav_conf_file()
{
  FILE *fp;
  char cav_conf_file_path[256];

  sprintf(cav_conf_file_path, "%s/logs/TR%d/cav.conf", g_ns_wdir, testidx);

  fp = fopen(cav_conf_file_path, "w");

  if(fp)
  {
    fprintf(fp, "CONFIG %s\n", g_cavinfo.config);
    fprintf(fp, "NSLoadIF %s\n", g_cavinfo.NSLoadIF);
    fprintf(fp, "SRLoadIF %s\n", g_cavinfo.SRLoadIF);
    fprintf(fp, "NSAdminIP %s\n", g_cavinfo.NSAdminIP);
    fprintf(fp, "SRAdminIP %s\n", g_cavinfo.SRAdminIP);
    fprintf(fp, "NSAdminIF %s\n", g_cavinfo.NSAdminIF);
    fprintf(fp, "SRAdminIF %s\n", g_cavinfo.SRAdminIF);
    fprintf(fp, "NSAdminGW %s\n", g_cavinfo.NSAdminGW);
    fprintf(fp, "SRAdminGW %s\n", g_cavinfo.SRAdminGW);
    fclose(fp);
  }
}

static int parent_init_before_parsing_args(char *argv0, char *err_msg)
{
  int ret;
  if((ret = ns_init_path(argv0, err_msg)) == -1)
    return ret;

  if((ret = confirm_netstorm_uid(argv0, err_msg)) == -1)
    return ret;
/* Code is commented for connection pool design, here we require
 * ultimate_max_connections which provide info about how many connections
 * can be made by one NVM
 * Single process cannot make connection more than ulimit
 * max user processes              (-u) 65536
 * In new design max user depends on browser settings whereas max connection
 * depend on ulimit
 * */
#if 0
  ultimate_max_connections = 64 - RESERVED_FDS;	/* a guess */
  /*#ifdef RLIMIT_NOFILE*/
  /* Try and increase the limit on # of files to the maximum. */
  if ( getrlimit( RLIMIT_NOFILE, &limits ) == 0 ) 
  {
    ultimate_max_connections = limits.rlim_cur - RESERVED_FDS;
  }
  printf("Ultimate limit = %d\n", ultimate_max_connections);
  /*#endif  RLIMIT_NOFILE */
  //init_avgtime(average_time);
#endif
  g_url_file[0] = '\0';
  /*#ifndef RMI_MODE
    g_det_url_file[0] = '\0';
    #endif*/
  g_conf_file[0] = '\0';

  do_checksum = do_throttle = do_verbose = 0;

  loader_opcode = STAND_ALONE;

  global_settings->use_prof_pct = -1;

  // Set this index as we need it in all case
  group_default_settings->pattern_table_start_idx = -1;

  /*bug id: 101320: set ns_ta_dir */
  if((ret = nslb_set_ta_dir(g_ns_wdir)) == -1)
    return ret;

  return 0;
}
#define TMP_CMD_LENGTH 1024
static int save_kw_args_in_file()
{
  char cmd_buf[4*TMP_CMD_LENGTH];

  if(g_set_args_type == KW_NONE)
    return 0; 
 
  if(g_set_args_type == KW_ARGS)
  {
    char tmpBuf[strlen(g_set_args_value) + 1];
    // Do decoding here
    ns_url_decoding_ex(tmpBuf, strlen(g_set_args_value)+1, strlen(g_set_args_value), g_set_args_value);
    sprintf(cmd_buf, "echo '%s' > %s/logs/TR%d/additional_kw.conf", tmpBuf, g_ns_wdir, testidx);
  }
  else if(g_set_args_type == KW_FILE)
  {
    if(g_set_args_value[0] == '/') // checking absolute path
    {
      sprintf(cmd_buf, "cp %s %s/logs/TR%d/additional_kw.conf", g_set_args_value, g_ns_wdir, testidx);
    }
    else
    {
      char buffer[TMP_CMD_LENGTH];
      strcpy(buffer, g_conf_file);
      char *ptr = strchr(buffer, '.');
      *ptr = '\0';
      sprintf(cmd_buf, "cp %s/%s %s/logs/TR%d/additional_kw.conf", buffer, g_set_args_value, g_ns_wdir, testidx);
    }
  }

  nslb_system2(cmd_buf);     
  return 0;
}

static int process_kw_set_argument(char *optargs)
{
 
  if(!strncmp(optargs, "KW=", 3))
    g_set_args_type = KW_ARGS;
  else if(!strncmp(optargs, "KW_FILE=", 8))
    g_set_args_type = KW_FILE;
  else
    return -1; // Invalid Keyword
  char *tptr = strchr(optargs, '=');
  tptr++;
  g_set_args_value =  strdup(tptr);  
  return 0;
}


static int parse_args(int argcount, char **argvector, char *err_msg)
{
  char c;
  int cflag=0, sflag=0, dflag=0, uflag=0, debug_script_flag=0;
  int mflag = 0, lflag = 0, role_flag=0, jflag=0;
  int rflag=0, iflag=0, tflag=0;
  int oflag=0, xflag=0, vflag=0;
  int ret=0, instflag=0;
  int Cflag = 0;
  g_test_inst_file[0] = 0; //initializing; may not be filled
  /* Parse args. */
  while ((c = getopt(argcount, argvector, "elm:c:s:d:o:N:R:I:T:x:u:r:D:j:i:Fv:C:")) != -1) 
  {
    switch (c) 
    {
      case 's':
        if (sflag) 
        {
	  //NSTL1_OUT(NULL, NULL, "s option cannot be specified more than once");
	  //exit(1);
          sprintf(err_msg, CAV_ERR_1031003, "-s (gui_server_address)");
          ret = -1;
        }
        sflag++;
        {
          char text[128];
          char* temp_ptr = text;
          int addr_size;

          strcpy(text, optarg);
          if(!strcmp(text,"0.0.0.0") || (loader_opcode == CLIENT_LOADER))
          {
       	    g_gui_bg_test = 1; 
          } 
          else
          {
            if ((temp_ptr = strchr(text, ':')))
              addr_size = temp_ptr - text;
            else 
            {
              //NSTL1_OUT(NULL, NULL, "-s option must be in format 'SERVER:PORT'");
              //exit(-1);
              sprintf(err_msg, CAV_ERR_1031004);
              ret = -1;
            }
            memcpy(global_settings->gui_server_addr, text, addr_size);
            global_settings->gui_server_addr[addr_size] = '\0';
            temp_ptr = strchr(text, ':') + 1;
            global_settings->gui_server_port = atoi(temp_ptr);
          }
        }
        break;

      case 'c':
        if (cflag) 
        {
	  //NSTL1_OUT(NULL, NULL, "c option cannot be specified more than once");
	  //exit(1);
          sprintf(err_msg, CAV_ERR_1031003, "-c (scenario)");
          ret = -1;
        }
        cflag++;
        strcpy(g_conf_file, optarg);
        break;

      case 'C':
        if (Cflag) 
        {
         //NSTL1_OUT(NULL, NULL, "C option cannot be specified more than once");
         //exit(1);
          sprintf(err_msg, CAV_ERR_1031003, "-C (Additional Keyword)");
          ret = -1;
        }
        Cflag++;
        if(!strcmp(optarg,"NA"))
          break;
        if(process_kw_set_argument(optarg) == -1)
          ret = -1;
        break;
      
      case 'o':
        if (oflag)
        {
          //NSTL1_OUT(NULL, NULL, "o option cannot be specified more than once");
          //exit(1);
          sprintf(err_msg, CAV_ERR_1031003, "-o (run mode)");
          ret = -1;
        }
        if(!strcmp(optarg, "compile")){
          run_mode_option |= RUN_MODE_OPTION_COMPILE;
        }
        oflag++;
        break;
      
       case 'v':
        if (vflag)
        {
          //NSTL1_OUT(NULL, NULL, "v option cannot be specified more than once");
          //exit(1);
          sprintf(err_msg, CAV_ERR_1031003, "-v (script validation)");
          ret = -1;
        }
        vflag++;
        global_settings->disable_script_validation = 1;
        break;


      case 'd':
        if (dflag) 
        {
	  //NSTL1_OUT(NULL, NULL, "d option cannot be specified more than once");
	  //exit(1);
          sprintf(err_msg, CAV_ERR_1031003, "-d (add delay)");
          ret = -1;
        }
        dflag++;
        add_delay = atoi(optarg);
        if (add_delay < 0) add_delay = 0;
        break;

      case 'e':
        g_collect_no_eth_data = 1;
        break;

      case 'm':
        if (mflag)
        {
          //NSTL1_OUT(NULL, NULL, "m option cannot be specified more than once");
          //exit(1);
          sprintf(err_msg, CAV_ERR_1031003, "-m (Netcloud information)");
          ret = -1;
        }
        mflag++;
        {
          char text[128];
          char *fields[10] = {0};
          int num_field;

          strcpy(text, optarg);
          /*MasterIP:MasterPort:Generator_id:EventLoggerPortOfMaster:PartitionIdx:TomcatPort */
          num_field = get_tokens(text, fields, ":", 10);
          if(num_field > 9) {
            //NSTL1_OUT(NULL, NULL, "-m option must be in format 'SERVER:PORT:GENERATOR_ID:EventLoggerPortOfMaster:PartitionIdx'");
            //exit(-1);
            sprintf(err_msg, CAV_ERR_1031005);
            ret = -1;
          }

          strcpy(master_ip, fields[0]);
          // printf("master ip = %s\n", master_ip);
          // Achint global_settings->master_ip=strtoul(temp_ptr, NULL, 10);  Pendgin
          master_port = atoi(fields[1]);
          loader_opcode = CLIENT_LOADER;
          send_events_to_master = 1;   // You all have to send events to yr masters log mgr
          g_parent_idx = g_generator_idx = atoi(fields[2]);
          /*Event Logger Port*/
          event_logger_port_number = atoi(fields[3]);
          
          /*PartitionIdx*/
          g_partition_idx = atoll(fields[4]);
          g_tomcat_port = atoi(fields[5]);
          /*Retain Master Machine Configuration*/
          if(fields[6])
            strcpy(master_sub_config, fields[6]);

          if(fields[7])
            strncpy((char *)global_settings->event_generating_host, fields[7], MAX_HOST_NAME_SIZE);

          if(fields[8])
            dh_master_port = atoi(fields[8]);
          
          global_settings->gui_server_addr[0] = '\0';
          global_settings->gui_server_port = 0;
          g_gui_bg_test = 1;
        }
        break;

      case 'l':
        if(lflag)
        {
          //NSTL1_OUT(NULL, NULL, "l option cannot be specified more than once");
          //exit(1);
          sprintf(err_msg, CAV_ERR_1031003, "-l (Master mode)");
          ret = -1;
        }
        lflag++;
        loader_opcode = MASTER_LOADER;
        break;

      case 'N':
        set_tname(optarg);
        break;

      case 'R':   //retry count
        if (rflag) 
        {
	  //NSTL1_OUT(NULL, NULL, "R option cannot be specified more than once");
	  //exit(1);
          sprintf(err_msg, CAV_ERR_1031003, "-R (Retry Count)");
          ret = -1;
        }
        rflag++;
        set_pre_test_check_retry_count(optarg);
        break;

      case 'I':   //retry interval
        if (iflag) 
        {
	  //NSTL1_OUT(NULL, NULL, "I option cannot be specified more than once");
	  //exit(1);
          sprintf(err_msg, CAV_ERR_1031003, "-I (Retry Interval)");
          ret = -1;
        }
        iflag++;
        set_pre_test_check_retry_interval(optarg);
        break;

      case 'T':   //timeout
        if (tflag) 
        {
	  //NSTL1_OUT(NULL, NULL, "T option cannot be specified more than once");
	  //exit(1);
          sprintf(err_msg, CAV_ERR_1031003, "-T (Timeout)");
          ret = -1;
        }
        tflag++;
        if((ret = set_pre_test_check_timeout(optarg, err_msg)) == -1)
          ret = -1;
        break;

      case 'x':
        if (xflag)
        {
          //NSTL1_OUT(NULL, NULL, "x option cannot be specified more than once");
          //exit(1);
          sprintf(err_msg, CAV_ERR_1031003, "-x");
          ret = -1;
        }
        ni_make_tar_option = 0;
        if(!strcmp(optarg, "do_not_create_tar")){
          ni_make_tar_option |= DO_NOT_CREATE_TAR;
        }
        if(!strcmp(optarg, "maketar_and_exit")) {
          ni_make_tar_option |= CREATE_TAR_AND_EXIT;
        } 
        if(!strcmp(optarg, "maketar_and_continue")) {   
          ni_make_tar_option |= CREATE_TAR_AND_CONTINUE;
        } 
        xflag++;
        break;
       
       case 'u':
         if (uflag)
         {
           //NSTL1_OUT(NULL, NULL, "u option cannot be specified more than once");
           //exit(1);
           sprintf(err_msg, CAV_ERR_1031003, "-u (User name)");
           ret = -1;
         }  
         strncpy(g_test_user_name, optarg, 128);
         uflag++;
         break;
    
      case 'r':
         if (role_flag)
         {
           //NSTL1_OUT(NULL, NULL, "r option cannot be specified more than once");
           //exit(1);
           sprintf(err_msg, CAV_ERR_1031003, "-r (User role)");
           ret = -1;
         }
         strncpy(g_test_user_role, optarg, 32);
         role_flag++;
         break;

      case 'D':
         if (debug_script_flag)
         {
           //NSTL1_OUT(NULL, NULL, "D option cannot be specified more than once");
           //exit(1);
           sprintf(err_msg, CAV_ERR_1031003, "-D (debug)");
           ret = -1;
         }
         g_debug_script = atoi(optarg);
         if (g_debug_script < 0) g_debug_script = 0;
         debug_script_flag++;
         break;
      
      case 'j':
         if (jflag)
         {
           sprintf(err_msg, CAV_ERR_1031003, "-j");
           ret = -1;
         }
         jflag++;
         g_do_not_ship_test_assets = atoi(optarg);
         break;
 
      case 'F':
         if(g_enable_test_init_stat)
         {
           //NSTL1_OUT(NULL, NULL, "F option cannot be specified more than once");
           //exit(1);
           sprintf(err_msg, CAV_ERR_1031003, "-F");
           ret = -1;
         }
         g_enable_test_init_stat = 1;
         break;

      case 'i':
          if(instflag)
          {
            sprintf(err_msg, CAV_ERR_1031003, "-i");
            ret = -1;
          }
          strncpy(g_test_inst_file, optarg, 64);
          break;
      case ':':

      case '?':
        //NSTL1_OUT(NULL, NULL, "Usage: %s -c conf-file {-s gui_server_address} {-d delay in sec} [-l] [-N Test Name] [-R retry count] [-I retry interval in sec] [-T timeout is sec]        -l option specifies Master mode", argv0);
        //exit(1);
        sprintf(err_msg, CAV_ERR_1031009, c);
        ret = -1;
    }
  }

  if ((optind != argcount) || (!cflag)) 
  {
    //NSTL1_OUT(NULL, NULL, "Usage: %s -c conf-file {-s gui_server_address} {-d delay in sec} [-l] [-N Test Name] [-R retry count] [-I retry interval in sec] [-T timeout is sec]        -l option specifies Master mode", argv0);
    //exit(1);
    
    sprintf(err_msg, CAV_ERR_1031057, argv0);
    ret = -1;
  }
  return ret;
}

void reset_test_start_time()
{
  time_t tt;
  struct tm *time_ptr, tm_struct;
  time(&tt);
  time_ptr = nslb_localtime(&tt, &tm_struct, 0);
  sprintf(g_test_start_time, "%02d/%02d/%2.2d  %02d:%02d:%02d",
        (time_ptr->tm_mon + 1), time_ptr->tm_mday, (time_ptr->tm_year-100),
        time_ptr->tm_hour, time_ptr->tm_min, time_ptr->tm_sec);
}

void init_test_start_time()
{
  char start_test_min[10];
  char time_string[100] = {0};
  time_t tt;
  struct tm *time_ptr, tm_struct;
  time(&tt);
  time_ptr = nslb_localtime(&tt, &tm_struct, 0);
  sprintf(g_test_start_time, "%02d/%02d/%2.2d  %02d:%02d:%02d",
        (time_ptr->tm_mon + 1), time_ptr->tm_mday, (time_ptr->tm_year-100),
        time_ptr->tm_hour, time_ptr->tm_min, time_ptr->tm_sec);
  sprintf(start_test_min, "%02d", time_ptr->tm_min); //In Production monitoring we need to trigger timer wrt to test start time
  start_test_min_dur = atoi(start_test_min); 

  sprintf(time_string, "%2.2d%d%d%02d%02d",(time_ptr->tm_year-100), (time_ptr->tm_mon + 1), time_ptr->tm_mday, time_ptr->tm_hour, time_ptr->tm_min);

}

//copy site_keywords.default & mprof (if used in scenario file) into test run directory.
static void copy_mprof_site_keywords_to_test_run()
{
  //Changing Mprof buffer to 64K
  char cmd[65536]="\0";
  NSDL2_TESTCASE(NULL, NULL, "Method called");

  //if hierarchical view is on; then first we need to find mprof in topology
  //if not found then copy from mprof/ dir
 
  FILE *fp;
  struct stat s;
  char buf[1024 + 1] = "";
  char mprof_name[256 + 1] = "";
  char *ptr = NULL;
  int len;

  sprintf(cmd, "cp %s", DEFAULT_KEYWORD_FILE);

  fp = fopen(g_conf_file, "r");
  if(fp == NULL)
  {
    NSTL1(NULL, NULL, "Could not open file %s, error is %s", g_conf_file, nslb_strerror(errno));
    NS_EXIT(-1, CAV_ERR_1000006, g_conf_file, errno, nslb_strerror(errno));
  }
  while(nslb_fgets(buf, 1024, fp, 0))
  {
    if(strncmp(buf, "MONITOR_PROFILE", 15) == 0)
    {
      len = strlen(buf);
      //removing new line characters
      if(buf[len - 2] == '\r' && buf[len - 1] == '\n')
        buf[len - 2 ] = '\0';
      else if(buf[len - 1] == '\r' || buf[len - 1] == '\n')
        buf[len - 1] = '\0';

      //removing blank spaces from right
      nslb_rtrim(buf);
      //getting last blank space in the line
      if((ptr = strrchr(buf, ' ')) == NULL)  //no space found-> invalid line; skip this line
        continue;
      else
        strcpy(mprof_name, ptr + 1);    //copy mprof name

      //check if mprof exist in topology dir
      //if no then copy from mprof/ dir
      sprintf(buf, "mprof/%s/%s.mprof", global_settings->hierarchical_view_topology_name, mprof_name);
      if(stat(buf, &s) != 0)  
        sprintf(buf, "mprof/%s.mprof", mprof_name);

      strcat(cmd, " ");
      strcat(cmd, buf);
    }
  }
  sprintf(buf, " logs/%s/ns_files 2>/dev/null", global_settings->tr_or_common_files);
  strcat(cmd, buf); 
 

  system(cmd);
}

/*this function copies the VendorData.default file into the /logs/TRxxxx file*/

static void copy_vendordefault_to_logs()
{
  char cmd[1024] = "\0";
  NSDL2_TESTCASE( NULL, NULL, "Method called");
  sprintf(cmd, "cp %s logs/%s/ 2>/dev/null", DEFAULT_DATA_FILE, global_settings->tr_or_common_files);
  if (system(cmd) != 0)
  {
    NSTL1(NULL, NULL, "Error in Copying VendorData.default file to TR/common_files");
    NS_EXIT(-1, CAV_ERR_1000019, cmd, errno, nslb_strerror(errno));
  }

  sprintf(cmd, "cp %s logs/%s/ 2>/dev/null", DEFAULT_DATA_FILE, global_settings->tr_or_partition);
  if (system(cmd) != 0)
  {
    NSTL1(NULL, NULL, "Error in Copying VendorData.default file to TR/partition");
    NS_EXIT(-1, CAV_ERR_1000019, cmd, errno, nslb_strerror(errno));
  }

}
#if 0
static void copy_adf_to_test_run() {
  char cmd[1024]="\0";

  NSDL2_TESTCASE(NULL, NULL, "Method called");

  sprintf(cmd, "cp %s `grep ^ALERT_PROFILE %s | awk '{print \"adf/\"$2\".adf\"}'` logs/TR%d/ns_files 2>/dev/null", DEFAULT_KEYWORD_FILE, g_conf_file ,testidx);
  system(cmd);
}
#endif
static void copy_files_to_test_run() {

  copy_mprof_site_keywords_to_test_run();
  copy_vendordefault_to_logs();

  // Copy Alert definition file
 // copy_adf_to_test_run();
}

int g_tomcat_port;
void get_tomcat_port()
{
  FILE *fp = NULL;
  char file_name[512] = {0};
  char port[28] = {0};
  
  NSDL1_PARSING(NULL, NULL, "Method Called");

  sprintf(file_name, "%s/webapps/.tomcat/tomcat_port", g_ns_wdir);

  if((fp = fopen(file_name, "r")) != NULL) {
    fread(port, 1, 1024, fp);
    fclose(fp);
  }
  else
    strcpy(port, "80");

  g_tomcat_port = atoi(port);
  NSDL2_PARSING(NULL, NULL, "Method End, tomcat_port = %d", g_tomcat_port);
}

#if 0

/*
We are using ns_proxy which is actually tiny proxy.
This function will validate and start the ns_proxy_server as per internet similation.  
Validation after parsing and shared memory is created
  (1) If WAN_ENV is ON -> Check only one access is used
     (i) In all SGRP, make sure only one profile is used and it have only one access
         * For internet simulation for external browser based API, only one access can be used.
     (ii) In all SGRP, make sure only one location is used and it have only one access 
          and also there is only  one server location
          * For internet simulation for external browser based API, only one user location 
              and server location can be used.
*/
static void start_ns_proxy_server(char *opt)
{
  char cmd_buf[1024 +  1];
  int i,j = -1;
  
  NSDL2_TESTCASE(NULL, NULL, "Method called, global_settings->wan_env = %d, opt = %s", global_settings->wan_env, opt);
  //WAN_ENV is ON 
  for (i = 0; i < total_runprof_entries; i++) {
    //NSDL2_TESTCASE(NULL, NULL, "profile[0] = [%s], profile[%d] = [%s}", 
      //       runprof_table_shr_mem[0].userindexprof_ptr->name, i, runprof_table_shr_mem[i].userindexprof_ptr->name);
 
    if(runprof_table_shr_mem[i].sess_ptr->sess_flag == 0)
      continue;
    else if(j == -1)
    {
      j = i;
       NSDL2_TESTCASE(NULL, NULL, "Tiny proxy is configured with profile[%d] = [%s]", 
                            j, runprof_table_shr_mem[j].userindexprof_ptr->name);
    }

    NSDL2_TESTCASE(NULL, NULL, "profile[%d] = [%s], profile[%d] = [%s]", 
             j, runprof_table_shr_mem[j].userindexprof_ptr->name, i, runprof_table_shr_mem[i].userindexprof_ptr->name);
    //Check uniqueness of profile
    if (i != 0 && strcmp(runprof_table_shr_mem[j].userindexprof_ptr->name, runprof_table_shr_mem[i].userindexprof_ptr->name))
    {
       //NSTL1_OUT(NULL, NULL, "Error: For internet simulation for external browser based API, all scenario groups must have same profile.");
       NSTL1_OUT(NULL, NULL, "Error: For internet simulation for external browser based API, all scenario's groups must have same profile if they have ns_exbrowser_url.");
       exit (-1);
    }
  
   NSDL2_TESTCASE(NULL, NULL, "length = %d, uprofindex_idx = %d, pct = %d",
                      runprof_table_shr_mem[i].userindexprof_ptr->length,
                      runprof_table_shr_mem[i].userindexprof_ptr->userprof_start->uprofindex_idx,
                      runprof_table_shr_mem[i].userindexprof_ptr->userprof_start->pct); 
   if (runprof_table_shr_mem[i].userindexprof_ptr->length > 1)
   {
     NSTL1_OUT(NULL, NULL, "Error: For internet simulation for external browser based API,"
                     " profile must have only one access and only one location.");
     exit (-1);
   } 
  } 

  if (global_settings->wan_env == 0) //WAN_ENV OFF
    //sprintf(cmd_buf, "nsi_restart_proxy -o restart -w 0:0:0:0:0:0:0 -D");
    sprintf(cmd_buf, "nsi_restart_proxy -o %s -w 0:0:0:0:0:0:0 -D", opt);
  else 
  { 
    UserProfIndexTableEntry_Shr* userindex_ptr;
    UserProfTableEntry_Shr *ustart;
    //LocAttrTableEntry_Shr *location;
    AccAttrTableEntry_Shr *access;

    //userindex_ptr = runprof_table_shr_mem[0].userindexprof_ptr;
    userindex_ptr = runprof_table_shr_mem[j].userindexprof_ptr;
    ustart = userindex_ptr->userprof_start;
    access = ustart->access;
    //location = ustart->location;
 
    //sprintf(cmd_buf, "nsi_restart_proxy -o restart -w %d:%d:0:%hu:%hu:%hu:%hu -D", 
    /*sprintf(cmd_buf, "nsi_restart_proxy -o %s -w %d:%d:0:%hu:%hu:%hu:%hu -D",
                    opt, runprof_table_shr_mem[0].userindexprof_ptr[0].userprof_start[0].access[0].fw_bandwidth,
                    runprof_table_shr_mem[0].userindexprof_ptr[0].userprof_start[0].access[0].rv_bandwidth,
                    runprof_table_shr_mem[0].userindexprof_ptr[0].userprof_start[0].location[0].linechar_array[0].fw_lat,
                    runprof_table_shr_mem[0].userindexprof_ptr[0].userprof_start[0].location[0].linechar_array[0].rv_lat,
                    runprof_table_shr_mem[0].userindexprof_ptr[0].userprof_start[0].location[0].linechar_array[0].fw_loss,
                    runprof_table_shr_mem[0].userindexprof_ptr[0].userprof_start[0].location[0].linechar_array[0].rv_loss);
*/
    /*Note: temporary we are using hard coded value for location parameter because we are getting wrong data in case of location*/
    sprintf(cmd_buf, "nsi_restart_proxy -o %s -w %d:%d:0:0:0:0:0",
                    opt, access->fw_bandwidth, access->rv_bandwidth);
  }

  NSDL2_TESTCASE(NULL, NULL, "cmd_buf = [%s]", cmd_buf);
  system(cmd_buf);
}
#endif

static void open_event_log_and_copy_files_to_tr()
{
  NSDL2_TESTCASE(NULL, NULL, "Method called");
  /*Bug3181: Added funct to add GUI_SERVER data to global.dat */
  write_log_file(NS_SCENARIO_PARSING, "Adding UI server address on global.dat");
  log_global_dat_gui_server();

  set_logfile_names();

  (void) signal( SIGHUP, SIG_IGN );
   /* Open file here for 2 reasons
    *1) GUI needs it in case of we are not logging any events
    *2) NVM/Log manager will not write any header to this file they all will open in append mode
   */

  /*sprintf(event_log_file, "%s/logs/%s/event.log", g_ns_wdir, global_settings->tr_or_partition); 
  open_event_log_file(event_log_file, O_CREAT | O_WRONLY | O_LARGEFILE | O_CLOEXEC, 1);
  if(elog_fd > 0) {
    close(elog_fd);
    elog_fd = -1;
   } */
  // Who needs event log file open in append mode
  // Moved here as we need g_test_start_time in process_all_gdf()
  init_test_start_time();

  /* removing this method from here as it needs to be called after partition creation  */
  //copy_files_to_test_run();
  //Allocating memory for static Hash URL Table
  //alloc_mem_for_urltable();
  // init_ms_stamp();
  //global_settings->test_start_time = get_ms_stamp();
  /* Read in and parse the files */

  NSDL2_TESTCASE(NULL, NULL, "Calling Parse_files");
  
}

static void health_check()
{
  char hm_buffer[8000];

  //This is for health monitor feature in NS
  //This is for disk space check before starting the test
  //Return values:
  // 0: Nothing happend just continue.
  // -1: Log the event and stop the test
  // 1: Log the event and continue with test
  int ret = nslb_check_system_disk(hm_buffer);

  if(ret == -1 || ret == 1)
  {
    NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_MAJOR,
                               __FILE__, (char*)__FUNCTION__,
                              "%s", hm_buffer);

    if(ret == -1)//need to stop test
    {
      NSTL1(NULL, NULL, "Test run stopped due to netstorm appliance health check failure");
      NSTL1(NULL, NULL, "%s", hm_buffer);
      NS_EXIT(-1, CAV_ERR_1031035, hm_buffer);
    }
  }

  int ret1 = nslb_check_system_inode(hm_buffer);

  if(ret1 == -1 || ret1 == 1)
  {
    NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_MAJOR,
                               __FILE__, (char*)__FUNCTION__,
                              "%s", hm_buffer);

    if(ret1 == -1)//need to stop test
    { 
      NSTL1(NULL, NULL, "Test run stopped due to netstorm appliance health check failure");
      NSTL1(NULL, NULL, "%s", hm_buffer);
      NS_EXIT(-1, CAV_ERR_1031035, hm_buffer);
    }
  }
}

static void remove_nsport_file(char *port_file)
{
  char file_path[1024];
  struct stat st;
  
  sprintf(file_path, "%s/logs/TR%d/%s", g_ns_wdir, testidx, port_file);

  //We will only remove if its present.
  if(!stat(file_path, &st))
  {
    if(remove(file_path) < 0)
      NSTL1(NULL, NULL, "Error in removing file %s. Error: %s", file_path, nslb_strerror(errno));
  }
}

/* This function is used to create netstorm_hosts.dat file 
 * which is used by GUI to show filteration with respect 
 * to host name.
 * Update file with NS machine name, monitors and in case of 
 * NetCloud mode add controller and generator name.
 * Next sort file in alphabetic order*/

static void create_netstorm_host_file()
{
  int i,j;
  char ns_host_file_name[1024];
  char cmd[1024];
  FILE *fp;
  //ServerInfo *servers_list = NULL;
  //int total_no_of_servers = 0;
  char err_msg[1024]= "\0";

  NSDL1_PARENT(NULL, NULL, "Method called");

  sprintf(ns_host_file_name, "%s/logs/%s/ns_files/netstorm_hosts.dat", g_ns_wdir, global_settings->tr_or_common_files);

  if((fp = fopen(ns_host_file_name, "w")) == NULL)
  {
    NSTL1(NULL, NULL, "Error in opening %s file.", ns_host_file_name);
    NS_EXIT(-1, CAV_ERR_1000006, ns_host_file_name, errno, nslb_strerror(errno));
  }

  //Update file with NS machine name
  fprintf(fp, "%s\n", global_settings->event_generating_host);

  //servers_list = (ServerInfo *) topolib_get_server_info();
  //Total no. of servers in ServerInfo structure
  //total_no_of_servers = topolib_get_total_no_of_servers();

  //Updte monitor name 
  /*for (i=0; i < total_no_of_servers; i++)
  {
    if (servers_list[i].cmon_monitor_count) 
    {
      NSDL2_PARENT(NULL, NULL, "Filling monitor name");
      if(!strcmp(servers_list[i].server_ip, "127.0.0.1")) {
        fprintf(fp, "NSAppliance\n");
      if(!strcmp (g_cavinfo.config, "NDE"))
        fprintf(fp, "NDAppliance\n");
      }
      else
        fprintf(fp, "%s\n", servers_list[i].server_disp_name);
    }
  } */
  for(i=0;i<topo_info[topo_idx].total_tier_count;i++)
   {
    for(j=0;j<topo_info[topo_idx].topo_tier_info[i].total_server;j++)
     {
      if((topo_info[topo_idx].topo_tier_info[i].topo_server[j].used_row != -1) && (topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->cmon_monitor_count))
      {
        NSDL2_PARENT(NULL, NULL, "Filling monitor name");
        if(!strcmp(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->server_ip, "127.0.0.1")){
          fprintf(fp, "NSAppliance\n");
        if(!strcmp (g_cavinfo.config, "NDE"))
          fprintf(fp, "NDAppliance\n");
       }
        else
          fprintf(fp, "%s\n", topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_disp_name);
      }
    } 
  }


  //In case of NetCloud we need to update generator name 
  if (loader_opcode == MASTER_LOADER) 
  {
    NSDL2_PARENT(NULL, NULL, "Filling generator name");
    for (i = 0; i < sgrp_used_genrator_entries; i++)
      fprintf(fp, "%s\n", generator_entry[i].gen_name);   
  }

  fclose(fp);
  //Sort host name in alphabetic order
  sprintf(cmd, "sort -u -d -o %s/logs/%s/ns_files/netstorm_hosts.dat %s/logs/%s/ns_files/netstorm_hosts.dat", 
            g_ns_wdir, global_settings->tr_or_common_files, g_ns_wdir, global_settings->tr_or_common_files);
  if (nslb_system(cmd,1,err_msg) != 0)
  {
    NSTL1(NULL, NULL, "Error in sorting netstorm_hosts.dat");
    NS_EXIT(-1, CAV_ERR_1000019, cmd, errno, nslb_strerror(errno));
  }
}

  
static void handle_reset_test_start_time_stamp()
{
  static int first_flag = 0;
  if(first_flag) return;
  char tr_buff[1024] = {0};

  if(global_settings->reset_test_start_time_stamp){
    NSDL2_PARENT(NULL, NULL, "Resetting test run start time stamp");
    reset_test_start_time();
    sighandler_t prev_handler;
    prev_handler = signal( SIGCHLD, SIG_IGN);
    init_ms_stamp();
    (void) signal( SIGCHLD, prev_handler);
    global_settings->test_start_time = get_ms_stamp(); 
    sprintf(tr_buff, "%d", testidx);
    create_summary_top(tr_buff);
    first_flag = 1;
  }
}

/* Create main NS and ND tables after creating ns_log dir as we are going to use ns_logs in this method
   when NC test is running then on generator the db tables logs will not be created on this path:
   logs/TRXXX/ns_logs/ns_trace.log  
   return status, 0  - postgresql is running and created tables and indexes successfully
                  -1 - postgresql not running
                  -2 - db_tbl_mgr failed to create tables and indexes
 */
void *create_main_db_tables(void *args)
{
 
  int *ret = (int *)malloc(sizeof(int));
  //call only when starting new test
  char cmd_buf[1024] = "";
 // char cmd_out[1024] = "";
  char log_file[2048] = "";
  IW_UNUSED(long long time1);
  IW_UNUSED(long long diff);
  IW_UNUSED(time1=time(NULL));

  // Initializing thread local storage
  ns_tls_init(VUSER_THREAD_BUFFER_SIZE);
 
  write_log_file(NS_DB_CREATION, "Checking postgresql connectivity");
  //Check postgresql is running
  if((*ret = chk_postgresql_is_running()) == -1)
  {
    NS_EXIT_EX(-1, NS_DB_CREATION, CAV_ERR_1031017);
  }
  
  write_log_file(NS_DB_CREATION, "Postgresql is running");
  //We will create table in case partition mode is off 
  //or partition mode is on and partition idx is first parition_idx means test is running for first time. 
  if(g_first_partition_idx == g_partition_idx) 
  {
    NSTL1(NULL, NULL, "Creating NS data base tables and indexes");
    write_log_file(NS_DB_CREATION, "Creating database tables and indexes");
    get_out_log_file_name(g_partition_idx, g_ns_wdir, testidx, log_file, 0);

    sprintf(cmd_buf, "%s/bin/db_tbl_mgr -t %d -m NS -c -i -M \"%s\" -O %s/%s.log -E %s", g_ns_wdir, testidx,
               g_cavinfo.config, testInitPath_buf, testInitStageTable[NS_DB_CREATION].stageFile, log_file);

    NSDL1_PARENT(NULL, NULL, "NS Parent is going to run %s command", cmd_buf);
    write_log_file(NS_DB_CREATION, "Executing command %s", cmd_buf);
    system(cmd_buf);
/*
    if((*ret = nslb_run_cmd_and_get_last_line(cmd_buf, 1024, cmd_out)) != 0)
    {
      NSTL1(NULL, NULL, "ERROR: Could not run cmd [%s]", cmd_buf);
      NS_EXIT_EX(-1, NS_DB_CREATION, "Failed to create database tables, please configure and re-run the test");
      return (ret);
    }

    NSTL1(NULL, NULL, "cmd_out = %s", cmd_out);
    if(cmd_out[0] != '0')
    {
      NSTL1_OUT(NULL, NULL, "ERROR:Could not create NS main table because of either invalid table space "
                        "in Keyword TABLESPACE_INFO or could not make database connection");
      NS_EXIT_EX(-1, NS_DB_CREATION, "Failed to create database tables, please configure and re-run the test");
      return (ret);
    }
    else
    {
      NSTL1(NULL, NULL, "Created NS data base tables and indexes successfully");
      write_log_file(NS_DB_CREATION, "Created database tables and indexes");
    }
*/
    NSTL4(NULL, NULL, "About to create nd tables. global_settings->net_diagnostics_mode = %hd", global_settings->net_diagnostics_mode);
    /* If ND is enabled, then create ND tables now. 
     * Also if reader run mode is 1 (online) then create ND table indexes lso.
     */
    if(global_settings->net_diagnostics_mode)
    {
      NSTL1(NULL, NULL, "Creating ND data base tables");
      write_log_file(NS_DB_CREATION, "Creating database tables and indexes for monitoring");
      get_out_log_file_name(g_partition_idx, g_ns_wdir, testidx, log_file, 1);
    
      /* Create .ndp_version file */
      sprintf(cmd_buf, "%s/logs/TR%d/nd/logs/.ndp_version", g_ns_wdir, testidx);
      FILE *fp = fopen(cmd_buf, "a");
      if(fp == NULL)
      {
        NSTL1_OUT(NULL, NULL, "ERROR: Could not create file [%s]", cmd_buf);
      }
      else
        fclose(fp);

      /* Create ND main tables */
      sprintf(cmd_buf, "%s/bin/db_tbl_mgr -t %d -m NS -c -i -M \"%s\" -O %s/%s.log -E %s", g_ns_wdir, testidx,
               g_cavinfo.config, testInitPath_buf, testInitStageTable[NS_DB_CREATION].stageFile, log_file);
      write_log_file(NS_DB_CREATION, "Executing command %s", cmd_buf);
      system(cmd_buf);

   /*   if((*ret = nslb_run_cmd_and_get_last_line(cmd_buf, 1024, cmd_out)) != 0)
      {
        NSTL1(NULL, NULL, "ERROR: Could not run cmd [%s]", cmd_buf);
        NS_EXIT_EX(-1, NS_DB_CREATION, "Failed to create database tables, please configure and re-run the test");
        return(ret);
      }

      NSTL1(NULL, NULL, "cmd_out = %s", cmd_out);
      if(cmd_out[0] != '0')
      {
        NSTL1(NULL, NULL, "ERROR:Could not create ND main table because of either invalid table space "
                        "in Keyword TABLESPACE_INFO or could not make database connection");
        NS_EXIT_EX(-1, NS_DB_CREATION, "Failed to create database tables, please configure and re-run the test");
        return(ret);
      }
      else
      {
        NSTL1(NULL, NULL, "Created ND data base tables and indexes successfully");
        write_log_file(NS_DB_CREATION, "Created database tables and indexes for monitoring");
      }*/
    }  
  }
  end_stage(NS_DB_CREATION, TIS_FINISHED, NULL);
  NSDL2_PARENT(NULL, NULL, "Creation of main db tables took %lld seconds", IW_UNUSED(diff = time(NULL) - time1));
  *ret = 0;
  
  // To free buffer of TLS
  TLS_FREE_AND_RETURN(ret);
}

void handle_child_ignore()
{
}

/*
  This function will perform two task simultaneously
  1. Checking of controller-generator compatibility (MASTER)
  2. Creation of DB tables and indexes (STANDALONE/MASTER)
*/
void create_db_tables_and_check_compatibility ()
{
  if(global_settings->reader_run_mode)
  {
    init_stage(NS_DB_CREATION);
    //pthread_attr_init(&db_table_attr);
    //pthread_attr_setdetachstate(&db_table_attr, PTHREAD_CREATE_JOINABLE);
 
    int ret = pthread_create(&db_table_thread_id, NULL, create_main_db_tables, NULL);
    if(ret)
    {
      NSTL1(NULL, NULL, "Not able to create db tables creation thread");
      NS_EXIT_EX(-1, NS_DB_CREATION, CAV_ERR_1031018);
    }
    NSDL2_PARENT(NULL, NULL, "Thread for creation of db tables and indexes is started successfully");
  }
}

static inline void update_avg_size_with_num_groups() 
{
  g_avg_size_only_grp = g_avgtime_size;
  NSDL2_MISC(NULL, NULL, "g_avg_size_only_grp = %d", g_avg_size_only_grp);
  if(SHOW_GRP_DATA)
  {
    NSDL2_MISC(NULL, NULL, "Show group based data is enabled. So going to increase the size of g_avgtime_size");
    g_avgtime_size = g_avgtime_size * (total_runprof_entries + 1);
  } else {
    NSDL2_MISC(NULL, NULL, "show group data is disabled.");
  }
}

static inline void update_cavg_size_with_num_groups() 
{
  g_cavg_size_only_grp = g_cavgtime_size;
  if(SHOW_GRP_DATA)
  {
    NSDL2_MISC(NULL, NULL, "Show group based data is enabled. So going to increase the size of g_cavgtime_size");
    g_cavgtime_size  = g_cavgtime_size * (total_runprof_entries + 1);
  } else {
    NSDL2_MISC(NULL, NULL, "show group data is disabled.");
  }
}

int create_format_for_csv_file(char *err)
{
  char buf[1024] = {0};
  char csv_path[512] = {0};
  FILE *fp = NULL;
  
  //static csv files format
  NSDL1_PARENT(NULL, NULL, "Method called");
  sprintf(csv_path, "%s/logs/TR%d/common_files/reports/csv", g_ns_wdir, testidx);
  
  sprintf(buf, "%s/urt.csv.format", csv_path);
  if(!(fp = fopen(buf, "w"))) {
    sprintf(err, CAV_ERR_1000006, buf, errno, nslb_strerror(errno));
    return -1;
  }
  fprintf(fp, "#TestId,UrlId,PageId,HashId,GenId,UrlLen,UrlName,CombinePageAndUrlNameLen,PageAndUrlName\n");
  fflush(fp);
  
  sprintf(buf, "%s/sst.csv.format", csv_path);
  if(!(fp = fopen(buf, "w"))) {
    sprintf(err, CAV_ERR_1000006, buf, errno, nslb_strerror(errno));
    return -1;
  }
  fprintf(fp, "#TestId,SessId,SessName\n");
  fflush(fp);

  sprintf(buf, "%s/pgt.csv.format", csv_path);
  if(!(fp = fopen(buf, "w"))) {
    sprintf(err, CAV_ERR_1000006, buf, errno, nslb_strerror(errno));
    return -1;
  }
  fprintf(fp, "#TestId,PageId,SessId,PageName,ComnbineSessPageLen,SessAndPageName\n");
  fflush(fp);

  sprintf(buf, "%s/trt.csv.format", csv_path);
  if(!(fp = fopen(buf, "w"))) {
    sprintf(err, CAV_ERR_1000006, buf, errno, nslb_strerror(errno));
    return -1;
  }
  fprintf(fp, "#TestId,TxId,TxName\n");
  fflush(fp);
  
  sprintf(buf, "%s/generator_table.csv.format", csv_path);
  if(!(fp = fopen(buf, "w"))) {
    sprintf(err, CAV_ERR_1000006, buf, errno, nslb_strerror(errno));
    return -1;
  }
  fprintf(fp, "#GeneratorName,GeneratorId,GeneratorWork,GeneratorIp,GeneratorResolvedIp,GeneratorAgentPort\n");
  fflush(fp);
  
  sprintf(buf, "%s/rpf.csv.format", csv_path);
  if(!(fp = fopen(buf, "w"))) {
    sprintf(err, CAV_ERR_1000006, buf, errno, nslb_strerror(errno));
    return -1;
  }
  fprintf(fp, "#TestId,GroupId,UserProfId,SessProfId,Pct,GroupName\n");
  fflush(fp);

  sprintf(buf, "%s/HostTable.csv.format", csv_path);
  if(!(fp = fopen(buf, "w"))) {
    sprintf(err, CAV_ERR_1000006, buf, errno, nslb_strerror(errno));
    return -1;
  }
  fprintf(fp, "#HostId,HostName\n");
  fflush(fp);

  sprintf(buf, "%s/log_phase_table.csv.format", csv_path);
  if(!(fp = fopen(buf, "w"))) {
    sprintf(err, CAV_ERR_1000006, buf, errno, nslb_strerror(errno));
    return -1;
  }
  fprintf(fp, "#PhaseId,GroupName,PhaseType,PhaseName");
  fflush(fp);
  
  sprintf(buf, "%s/hat.csv.format", csv_path);
  if(!(fp = fopen(buf, "w"))) {
    sprintf(err, CAV_ERR_1000006, buf, errno, nslb_strerror(errno));
    return -1;
  }
  fprintf(fp, "#TestId,ActSvrGroup,ActSvrName,ActSvrPort,ActSvrLocation,GroupIdx");
  fflush(fp);
   
  sprintf(buf, "%s/hrt.csv.format", csv_path);
  if(!(fp = fopen(buf, "w"))) {
    sprintf(err, CAV_ERR_1000006, buf, errno, nslb_strerror(errno));
    return -1;
  }
  fprintf(fp, "#TestId,RecSvrId,RecSvrName,RecSvrPort,SelectAgenda,RecSvrType");
  fflush(fp);
  
  sprintf(buf, "%s/spf.csv.format", csv_path);
  if(!(fp = fopen(buf, "w"))) {
    sprintf(err, CAV_ERR_1000006, buf, errno, nslb_strerror(errno));
    return -1;
  }
  fprintf(fp, "#TestId,SessProfId,SessId,SessPct,SessProfName");
  fflush(fp);
  
  sprintf(buf, "%s/upf.csv.format", csv_path);
  if(!(fp = fopen(buf, "w"))) {
    sprintf(err, CAV_ERR_1000006, buf, errno, nslb_strerror(errno));
    return -1;
  }
  fprintf(fp, "#TestId,UserProfId,UserProfName,UserProfType,UserProfValueId,Pct,Value");
  fflush(fp);

  sprintf(buf, "%s/ect.csv.format", csv_path);
  if(!(fp = fopen(buf, "w"))) {
    sprintf(err, CAV_ERR_1000006, buf, errno, nslb_strerror(errno));
    return -1;
  }
  fprintf(fp, "#ObjType,ErrCode,ErrMsg");
  fflush(fp);
  
  //dynamic csv file format 
  sprintf(buf, "%s/urc.csv.format", csv_path);
  if(!(fp = fopen(buf, "w"))) {
    sprintf(err, CAV_ERR_1000006, buf, errno, nslb_strerror(errno));
    return -1;
  }
  fprintf(fp, "#RecordNum,DynaTraceStrSize,DynaTraceData,ChildIdx,UrlId,SessNormId,SessInst,CurTx,PageId,TxInst,PageInst,BitMask,StartTime,DnsLUTime,ConnectTime,SslHSTime,WrCompTime,FirstByteRecTime,ReqCompTime,RenderingTime,EndTime,ResponseTime,HttpPayload,TcpBytes,EthByteSent,CptrBytes,TcpByteRec,EthByteRec,CompressMode,Status,ConnNum,ConnType,Retries,FlowPathInst,PhaseNum");
  fflush(fp);
  
  sprintf(buf, "%s/prc.csv.format", csv_path);
  if(!(fp = fopen(buf, "w"))) {
    sprintf(err, CAV_ERR_1000006, buf, errno, nslb_strerror(errno));
    return -1;
  }
  fprintf(fp, "#RecordNum,PageId,SessId,SessInst,CurTx,ChildIdx,TxInst,PageInst,StartTime,EndTime,PageStatus,PhaseNum");
  fflush(fp);
  
  sprintf(buf, "%s/trc.csv.format", csv_path);
  if(!(fp = fopen(buf, "w"))) {
    sprintf(err, CAV_ERR_1000006, buf, errno, nslb_strerror(errno));
    return -1;
  }
  fprintf(fp, "#RecordNum,TxId,SessId,SessInst,ChildIdx,TxInst,TxStart,TxEnd,TxThinkTime,TxStatus,PhaseNum");
  fflush(fp);
  
  sprintf(buf, "%s/tprc.csv.format", csv_path);
  if(!(fp = fopen(buf, "w"))) {
    sprintf(err, CAV_ERR_1000006, buf, errno, nslb_strerror(errno));
    return -1;
  }
  fprintf(fp, "#RecordNum,TxId,SessId,SessInst,ChildId,TxInst,PageInst,StartTime,EndTime,TxStatus,PhaseNum");
  fflush(fp);

  sprintf(buf, "%s/src.csv.format", csv_path);
  if(!(fp = fopen(buf, "w"))) {
    sprintf(err, CAV_ERR_1000006, buf, errno, nslb_strerror(errno));
    return -1;
  }
  fprintf(fp, "#RecordNum,SessId,SessInst,UserId,GroupNum,ChildId,IsRunPhase,AccessId,Location,Browser,Frequency,MachAttr,StartTime,EndTime,SessThinkTime,SessStatus,PhaseNum");
  fflush(fp);

  sprintf(buf, "%s/pagedump.csv.format", csv_path);
  if(!(fp = fopen(buf, "w"))) {
    sprintf(err, CAV_ERR_1000006, buf, errno, nslb_strerror(errno));
    return -1;
  }
  fprintf(fp, "#RecordNum,StartTime,EndTime,GenId,ChildId,UserId,SessInst,PageId,PageInst,PageStatus,PageRespTime,GroupIdx,SessId,PartitionId,TraceLevel,Future,TxNameLen,TxName,FetchParamLen,FetchParam,FlowName,LogFileSFX,RespBodyOrigName,Parameter");
  fflush(fp);
  
  sprintf(buf, "%s/PageRBUDetailRecord.csv.format", csv_path);
  if(!(fp = fopen(buf, "w"))) {
    sprintf(err, CAV_ERR_1000006, buf, errno, nslb_strerror(errno));
    return -1;
  }
  fprintf(fp, "#RecordNum,PartitionId,HarFileName,PageId,SessId,ChildId,GenId,GroupId,HostId,BrowserType,ScreenHeight,ScreenWidth,DomContentTime,OnLoadTime,PageLoadTime,StartRenderTime,TimeToInteract,VisuallyCompTime,ReqServerCount,ReqBrowserCacheCount,ReqTextTypeCount,ReqTextTypeSize,ReqJsTypeCount,ReqJsTypeCumSize,ReqCssTypeCount,ReqCssTypeSizeCumSize,ReqImageTypeCount,ReqImageTypeCumSize,ReqOtherTypeCount,ReqOtherTypeCumSize,ByteRec,ByteSent,SpeedIdx,MainUrlStartDateTime,HarFileDateTime,ReqCountBeforeDomContent,ReqCountBeforeOnLoad,ReqCountBeforeStartRendering,BrowserCacheReqCountBeforeDomContent,BrowserCacheReqCountBeforeOnLoad,BrowserCacheReqCountBeforeStartRendering,CookiesCavNVSize,CookiesCavNV,GroupNameSize,GroupName,ProfileName,TransStatus,SessionInst,DeviceInfo,performanceTraceMode,DOMElements,BackendResponseTime,ByteRecBeforeDomContent,ByteRecBeforeOnLoad,FirstPaint,FirstContentfulPaint,LargestContentfulPaint,CumLayoutShift,TotalBlockingTime");
  fflush(fp);

  sprintf(buf, "%s/RBULightHouseRecord.csv.format", csv_path);
  if(!(fp = fopen(buf, "w"))) {
    sprintf(err, CAV_ERR_1000006, buf, errno, nslb_strerror(errno));
    return -1;
  }
  fprintf(fp, "#RecordNum,PartitionId,LighthouseReportFileName,PageId,SessId,ChildId,GenId,GroupId,HostId,BrowserType,GroupName,LighthouseFileDateTime,PerformanceScore,PWAScore,AccessibilityScore,BestPracticeScore,SEOScore,FirstContentfulPaint,FirstMeaningfulPaint,SpeedIndex,FirstCPUIdle,TTI,InputLatency");
  fflush(fp);

  sprintf(buf, "%s/RBUUserTiming.csv.format", csv_path);
  if(!(fp = fopen(buf, "w"))) {
    sprintf(err, CAV_ERR_1000006, buf, errno, nslb_strerror(errno));
    return -1;
  }
  fprintf(fp, "#RecordNum,PageId,SessId,ChildId,GenId,GroupId,HostId,StartTime,Name,Type,RelStartTime,Duration,SessInstance,PageInstance");
  fflush(fp);

  fclose(fp); 

  NSDL1_PARENT(NULL, NULL, "Method ended().");
  return 0;
}

// This function set and init average time 
void  init_all_avgtime() 
{ 
  NSDL1_PARENT(NULL, NULL, "Method called, g_avgtime_size = %d", g_avgtime_size);

  // 4.1.15, moving this from first avg ptr to the first dyn avg ptr.
  //Update g_avg_time_size for HTTP response code
  //update_http_resp_code_avgtime_size(); 
 
  // Update g_avgtime_size if HTTP Caching is Enabled
  cache_update_cache_avgtime_size();

  /*bug 70480 :  Update g_avgtime_size for  HTTP2 Server Push*/
  h2_server_push_update_avgtime_size();

  // To Update g_avgtime_size if HTTP Proxy is Enabled for proxy structure
  update_proxy_avgtime_size();   //ProxyAvgTime

  update_nw_cache_stats_avgtime_size(); //To update g_avgtime_size if G_ENABLE_NETWORK_CACHE_HEADERS is enabled 

  update_dns_lookup_stats_avgtime_size(); 
  //Update g_avgtime_size if FTP is Enabled
  ftp_update_ftp_avgtime_size();
  #ifndef CAV_MAIN
  ftp_update_ftp_cavgtime_size();
  #endif

  //Update g_avgtime_size if LDAP is Enabled
  ldap_update_ldap_avgtime_size();
  #ifndef CAV_MAIN
  ldap_update_ldap_cavgtime_size();
  #endif

  //Update g_avgtime_size if IMAP is Enabled
  imap_update_imap_avgtime_size();
  #ifndef CAV_MAIN
  imap_update_imap_cavgtime_size();
  #endif

  //Update g_avgtime_size if JRMI is Enabled
  jrmi_update_jrmi_avgtime_size();
  #ifndef CAV_MAIN
  jrmi_update_jrmi_cavgtime_size();
  #endif

  //Updating Websocket
  update_websocket_data_avgtime_size();
  #ifndef CAV_MAIN
  update_websocket_data_cavgtime_size();
  #endif

  //Update g_avgtime_size if XMPP is Enabled
  update_xmpp_avgtime_size();
  #ifndef CAV_MAIN
  update_xmpp_cavgtime_size();
  #endif

  //Update g_avgtime_size if FC2 is Enabled
  update_fc2_avgtime_size();
  #ifndef CAV_MAIN
  update_fc2_cavgtime_size();
  #endif
  
  //Update g_avgtime_size if DOS Attack is Enabled
  extern void dos_attack_update_avgtime_size(); // Add to avoid warning
  dos_attack_update_avgtime_size();

  #ifndef CAV_MAIN
  //update_jmeter_avgtime_size();
  #endif

  //Update g_avgtime_size for SocketAPIs-TCP/UDP client/server
  UPDATE_AVGTIME_SIZE4SOCKET_TCP_UDP_CLIENT;
  
   //Update avg size for Cavisson Test Monitor
   // CAV_MAIN 
   update_avgtime_size_for_cavtest();

  /*==== || Please pay your attention here || ======
    ALL METRICS WHICH WILL NOT COME IN GROUP WISE DATA MUST BE SET AFTER update_avg_size_with_num_group() FUNCTION
    ALL METRICS WHICH NEEDS TO COME IN GROUP WISE DATA MUST BE SET BEFORE update_avg_size_with_num_group() FUNCTION
    ==============================================*/
  update_avg_size_with_num_groups();
  #ifndef CAV_MAIN
  update_cavg_size_with_num_groups();
  #endif


  //Update avg size for user moniter data 
  update_um_data_avgtime_size();
  #ifndef CAV_MAIN
  update_um_data_cavgtime_size();
  #endif
  
  //Update g_avgtime_size if Show Group Data keyword enable
  update_group_data_avgtime_size();

  //Updating RBU Page Stat avg_time size if G_RBU is enabled
  update_rbu_page_stat_data_avgtime_size();

  //Updating Page Based Stat avg_time_size if ENABLE_PAGE_BASED_STATS is enabled
  update_page_based_stat_avgtime_size();

  //Update g_avgtime_size if memory debug is Enabled
  update_ns_diag_avgtime_size();
  #ifndef CAV_MAIN
  update_ns_diag_cavgtime_size();
  #endif

  //Update g_avg_time_size if G_SHOW_VUSER_FLOW is enabled
  update_show_vuser_flow_avgtime_size();

  /*In copy_progress_data, while copying msg data to generators avg we will copy only static_avg_size excluding transactions and server ip data as those will be handled using normalization and ip data is handled separately*/ 
  g_static_avgtime_size = g_avgtime_size; 


  /*#################### || Update g_avgtime_size for Dynamic Groups || #####################*/

  /*======================================================================
    Manish Mishra, 13Aug2020
    -------------

     [HINT: NSDynObj]

          Be careful to increase avgtime for dynamic Group as
          there are lots technical hooks exist.
          
          1. Memory allocation in avgtime for dyn objs are - 
                          
                                          
                           |               |
                           +---------------+
            StaticGrps  => |               |
                           +---------------+
            FirstDynGrp => |  UDPClienFail |
                           +---------------+
                           |  TCPClienFail |
                           +---------------+
                           |  HTTP SC      |
                           +---------------+
                           |  Trans        |
                           +---------------+
                           |  SRVIP        |
                           +---------------+
                           |  SRCIP        | 
                           +---------------+
            LastDynGrp =>  |  RBUDomain    |
                           +---------------+
                
          2. If adding Group at before HTTP SC, then on discovering new
             object, MUST shift all pointer form HTTP SC to last

          3. If adding Group at last, then on discoverying new object,
             MUST handle this Group in all the above Groups in shifting 
             pointer 

          ==> DO NOT add in between other you have to handle 2 & 3 both.

          ==> Always add at first position of dynamic Group so that other
              other group code not disturbed 
  ======================================================================*/  

  //First dynamic avg ptr
  UPDATE_AVG_SIZE4UDP_CLIENT_FAILURES;

  UPDATE_AVG_SIZE4TCP_CLIENT_FAILURES;

  update_http_resp_code_avgtime_size(); 
  
  update_trans_avgtime_size();
  #ifndef CAV_MAIN
  update_trans_cavgtime_size();
  update_srv_ip_data_avgtime_size();
  //Updating IP Based Stat avg_time_size if ENABLE_IP_BASED_STATS is enabled
  //IP based must be update at last
  update_ip_data_avgtime_size();
  #endif

  update_rbu_domain_stat_avgtime_size();
  // For CAV GEN we will not called this function
  #ifndef CAV_MAIN
  malloc_avgtime(); 
  #else
  realloc_avgtime_and_set_ptrs(g_avgtime_size, 0, NEW_OBJECT_DISCOVERY_NONE);
  #endif

  NSDL1_PARENT(NULL, NULL, "Initialisation of avgtime done. Total size of avgtime = %d. Other indexs: "
                           "g_cache_avgtime_idx = %d, g_proxy_avgtime_idx = %d, g_network_cache_stats_avgtime_idx = %d, "
                           "dns_lookup_stats_avgtime_idx = %d, g_ftp_avgtime_idx = %d, g_ftp_cavgtime_idx = %d, "
                           "g_ldap_avgtime_idx = %d, g_ldap_cavgtime_idx = %d, g_imap_avgtime_idx = %d, g_imap_cavgtime_idx = %d, "
                           "g_jrmi_avgtime_idx = %d, g_jrmi_cavgtime_idx = %d, g_ws_avgtime_idx = %d, g_ws_cavgtime_idx = %d, "
                           "g_dos_attack_avgtime_idx = %d, g_avg_size_only_grp = %d, g_cavg_size_only_grp = %d, "
                           "g_avg_um_data_idx = %d, g_cavg_um_data_idx = %d, group_data_gp_idx = %d, rbu_page_stat_data_gp_idx = %d, "
                           "page_based_stat_gp_idx = %d, g_ns_diag_avgtime_idx = %d, g_ns_diag_cavgtime_idx = %d, "
                           "show_vuser_flow_idx = %d, g_trans_avgtime_idx = %d, g_trans_cavgtime_idx = %d, srv_ip_data_gp_idx = %d, "
                           "ip_data_gp_idx = %d, rbu_domain_stat_avg_idx = %d, http_status_codes_gp_idx = %d", 
                            g_avgtime_size, g_cache_avgtime_idx, g_proxy_avgtime_idx, g_network_cache_stats_avgtime_idx,
                            dns_lookup_stats_avgtime_idx, g_ftp_avgtime_idx, g_ftp_cavgtime_idx, g_ldap_avgtime_idx,
                            g_ldap_cavgtime_idx, g_imap_avgtime_idx, g_imap_cavgtime_idx, g_jrmi_avgtime_idx, g_jrmi_cavgtime_idx,
                            g_ws_avgtime_idx, g_ws_cavgtime_idx, g_dos_attack_avgtime_idx, g_avg_size_only_grp,
                            g_cavg_size_only_grp, g_avg_um_data_idx, g_cavg_um_data_idx, group_data_gp_idx, rbu_page_stat_data_gp_idx,
                            page_based_stat_gp_idx, g_ns_diag_avgtime_idx, g_ns_diag_cavgtime_idx, show_vuser_flow_idx,
                            g_trans_avgtime_idx, g_trans_cavgtime_idx, srv_ip_data_gp_idx, ip_data_gp_idx, 
                            rbu_domain_stat_avg_idx, http_status_codes_gp_idx);
}

void parent_init_after_parsing_args()
{
  //time_t mseconds_before, mseconds_after, mseconds_elapsed;
  //char mseconds_buff[100];
  //int port;
  char err_msg[1024];   /* Buffer for dumping error*/
  char type[9]; //Find controller type

  /*If run_mode_option is given then dont call
   * this function*/
  if(!(run_mode_option & RUN_MODE_OPTION_COMPILE))
    open_event_log_and_copy_files_to_tr();
  /*To get syetem data.
 * Eg: Disk free
 * We are doing here because we need data before parsing 
 * scenario file*/
  write_log_file(NS_SCENARIO_PARSING, "Filling file system size and inode information");
  nslb_get_system_data(); 
  nslb_get_inode_data();
  write_log_file(NS_SCENARIO_PARSING, "Creating metadata (page,session,group etc.) files");
  build_norm_table_from_csv_files();

  if( total_hidden_file_entries < 1)
    read_and_init_hidden_file_struct();

  if(parse_files() == -1)  // NEW CODE
  {
    /* Adding ns_exit to end_stage*/
    NS_EXIT(1, "Error in parsing files");
  }

  //this will read and parse http_response_codes.dat
  write_log_file(NS_SCENARIO_PARSING, "Creating response code table");
  // CAV MAIN: PARENT_INIT OR CHILD INIT
  init_http_response_codes();

  //Get tomcate port
  write_log_file(NS_SCENARIO_PARSING, "Fill tomcat port");
  if(loader_opcode != CLIENT_LOADER)
    get_tomcat_port();

  //get version of all components excluding HPD using nsu_get_version
  //save to file 'version' in TR or partition
  //save_version(); //bug:52786] moving this to create_partition_dir

  /* Moving this method here as it needs to be called after partition creation  */
  write_log_file(NS_SCENARIO_PARSING, "Copying site specific settings file to %s directory", g_test_or_session);
  copy_files_to_test_run();

  #if OPENSSL_VERSION_NUMBER < 0x10100000L
    char ssl_log_path[1024];
    write_log_file(NS_SCENARIO_PARSING, "Initializing ssl trace log");
    sprintf(ssl_log_path, "%s/logs/TR%d", g_ns_wdir, testidx);
    cav_ssl_init_trace_log(ssl_log_path);
  #endif

  //called here as TIME_STAMP keyword is parsed in parse_files --->read_scripts_glob_vars()
  //init_ms_stamp();
  // We are calling this to clear all shared modems if hanging from some other test
  // This should not cause any problem if another test is running as
  // modem is not closed unless reference count becomes is 0
  // Issue may come, if reference count becomes 0 in another running test, then it may get closed
  if (loader_opcode != MASTER_LOADER)
    clear_shared_modems();

  /* Moved nsu_create_table from logging reader as in case of ND, when
   * URL discovery events are received, Entry goes in URLtable. Since
   * logging reader is spawned much later in the init tasks as compared to
   * start_net_diagnostic message to NDC, it is possible that ND tries to 
   * write to url table even before logging reader is spawned.
   */
  NSDL1_PARENT(NULL, NULL, "global_settings->reader_run_mode = %d", global_settings->reader_run_mode);

  //28Apr2014.
  //We have splited set_log_dirs method into three methods.
  //1. set_log_dirs(), copy_scripts_to_tr() and create_summary_report().
  //and moved set_log_dirs() here because we need log directries in create_main_db_tables().
  write_log_file(NS_SCENARIO_PARSING, "Creating scripts directory inside %s", g_test_or_session);
  set_log_dirs();

  //Gaurav: Bug #16225, dump csv file format
  if(create_format_for_csv_file(err_msg) != 0) {
    NSTL1(NULL, NULL, "%s", err_msg);
    NS_EXIT(-1, "%s", err_msg);
  }

  autoRecordingDTServer(START_DT_RECORDING);

  //This is done, becaue in continuous monitoring test, if any component finds this file then it goes to make connection on that port. It is possible that the port present in that file is of previous test.
  remove_nsport_file("NSPort");
  remove_nsport_file("NSDHPort");

  //monitor and topolog code moved to after parent_init_after_parsing_args
  // Get vector names of all dynamic vector monitors (if any)
  // process_all_gdf() must be called after this
  /*In case of generator, fd will not be initialise hence at time of add_select_msg_com_con fd received was 0*/

   //  added Process data and Gc monitor for jts 
  njvm_add_auto_monitors();

  network_cache_stats_init(); // Initialize for network cache stats if enabled

  if(res_init() < 0)
  {
    NSTL1(NULL, NULL, "Error: res_init failed to initialize state structure");
    NS_EXIT(-1, CAV_ERR_1031034);
  }
  dns_lookup_stats_init();
  dns_lookup_cache_init();

  if (start_logging(global_settings, testidx, 1) == -1)
    NS_EXIT(-1, "Failed to start static (slog) and dynamic (dlog) logging");
  // This must be done before GDF processing as GDF needs access to avgtime for filling rtg_index for Tx etc  
  /**Bug 79836***/
  max_tx_entries = total_tx_entries; //NEW CODE
  init_all_avgtime();  // NEW CODE

  //check if same group id not used as CM and DVM
  //TODO MSR: Use if duplicate gdf for custom and dynamic monitor is required
  //check_dup_grp_id_in_CM_and_DVM();  
  
  /* NetCloud: Updating event generating host for internal controller
   * In case of internal controller we will be filling host with controller server ip */
  if (loader_opcode == MASTER_LOADER)
  { 
    find_controller_type(type);
    if ((!strcmp(type, "Internal")))
    {
      strcpy((char *)global_settings->event_generating_host, global_settings->ctrl_server_ip);
      NSDL2_PARENT(NULL, NULL, "Event generating host for internal controller = %s", global_settings->event_generating_host); 
    }
  }
 
  create_netstorm_host_file();   

  health_check();


  //Setting page_start in RunProfTable and relative_page_id in PageTable for Page Based Stat feature
  init_page_based_stat();    //For page based stat // NEW CODE

  if(global_settings->protocol_enabled & RBU_API_USED && global_settings->rbu_enable_csv)
  {
    if(ns_rbu_generate_csv_file() == -1)
      NSDL2_PARENT(NULL, NULL, "Error occured in function ns_rbu_generate_csv_file");
  }

  //If user has given over ride keyword then override the values.
  if(total_paramoverride_entries)
  {
    replace_overide_values();
  } 
  write_log_file(NS_SCENARIO_PARSING, "Creating sharing tables");
  copy_structs_into_shared_mem(); // NEW CODE
  // This function will set member custom_delay_func_ptr of table inlineDelayTable.
  // We are calling this function from here as PASS1, PASS2 is completed and Shared mem is created.
  fill_custom_delay_fun_ptr(); // NEW CODE
  fill_custom_page_think_time_fun_ptr(); // NEW CODE

  if (loader_opcode == MASTER_LOADER)
    update_generator_list();

  // divide thrd pool init and max size among nvms 
  if(is_java_type_script()){
    divide_thrd_among_nvm();
  }

  /*allocate nvm scratch buffer*/
  // TODO:: CAV MAIN CHILD INIT OR PARENT INIT
  init_ns_nvm_scratch_buf(); 

  // initialize the status code for which referer should not be modified
  // CAV MAIN CHILD INIT OR PARENT INIT
  
  init_status_code_table(); // NEW_CODE

  /*Manish: For testing only*/
  /*Dumping Shared memory Proxy Data:*/
  //ns_proxy_shr_data_dump();

  /* Initialise event logger process 
     Note - event logger (nsa_log_mgr) is required for events as well for save data API
     This must be done after shared memory is initialized
  */
  write_log_file(NS_SCENARIO_PARSING, "Initializing event logger");
  init_event_logger(0);
  // TBD 
  create_per_proc_sess_table(); // TO DO CAVMAIN
  check_and_set_flag_for_ka_timer(); // NEW CODE
  check_and_set_flag_for_idle_timer(); // NEW CODE
  validate_g_auto_fetch_embedded_keyword(); // NEW CODE // Validation of G_AUTO_FETCH_EMBEDDED keyword with AUTO_REDIRECT, AUTO_COOKIE and G_NO_VALIDATION
  //TODO:Manpreet commented code in 3.9.0 MAX_CON_PER_VUSER is group based keyword, need to print connections for all groups
  //printf ("max con per vuser=%d\n", global_settings->max_con_per_vuser);
  if (get_any_pipeline_enabled() == 1)
      printf("Pipelining is enabled\n"); 
   
  /*********************************/

  //freeeing servers_list as it will be used in custom_config of monitors
  //free_servers_list(); we can not free as we wil use this server list at the end of test run send_end_msg_to_all_cavmonserver_used

  //Manish: start tiny proxy if RBU_API_USED is enabled 
  //if (global_settings->protocol_enabled & RBU_API_USED) 
    //start_ns_proxy_server("restart"); 
    //start_ns_proxy_server(); 

  // 1. Check whether the browser exists or not. 
  // 2. Before starting test stop all firefox command running through that user and with that controller
  if((global_settings->protocol_enabled & RBU_API_USED) && (loader_opcode != MASTER_LOADER))
  {
    //Resolved Bug 16658- When we are running any test then it is taking 34 as a default chrome version 
    if(ns_rbu_check_for_browser_existence() == -1)
      NS_EXIT(-1, "Failed ns_rbu_check_for_browser_existence()"); 
    ns_rbu_kill_browsers_before_start_test();
  }


  if (loader_opcode == MASTER_LOADER) {
    if (testcase_shr_mem->mode != TC_FIX_USER_RATE && 
        testcase_shr_mem->mode != TC_FIX_CONCURRENT_USERS &&
        testcase_shr_mem->mode != TC_MIXED_MODE ) {
      NSTL1(NULL, NULL, "Goal based Scenarios are not supported in Master Mode");
      NS_EXIT(-1, "Goal based Scenarios are not supported in Master Mode");
    }
  }

  set_num_additional_headers(); // NEW CODE

  dump_urlvar_data(); 

  //28Apr2014. splited set_log_dirs into three parts.
  //if (global_settings->script_copy_to_tr != DO_NOT_COPY_SRCIPT_TO_TR) 
  //  copy_scripts_to_tr();
  /*In order to fix browser inspecific referencing in javascript, need to call site_page_dump_post_proc intially on scripts
   NC: In case of generator dump files are not copied in script hence by-pass code
   TODO: In the light of keyword "DISABLE_SCRIPT_COPY_TO_TR" we need to check G_TRACING, in current design these keywords can exists mutually*/

// In 415 release we will call site_page_dump_post_proc command only once when script is recorded.
/*
  if (loader_opcode != CLIENT_LOADER)
  {
    sprintf(cmd, "%s/sys/site_page_dump_post_proc -t %d", g_ns_wdir, testidx); 
    NSDL2_PARENT(NULL, NULL, "Running command [%s] to fix browser inspecific referencing in javascript", cmd);
    nslb_system(cmd,1,err_msg);
  }
*/
  create_summary_report();
 //Always set page dump directory as some one can enable page dump at run time
 //if((get_max_tracing_level() != 0) && (get_max_trace_dest() != 0)) 
 {
   create_page_dump_dir_or_link();
 }
 
  // For replay 
  if(global_settings->replay_mode)
  {
    // Set NVMs based on number of data files available
    global_settings->num_process = ntp_get_num_nvm();
    read_ns_index_last();
    if(global_settings->replay_mode == REPLAY_USING_ACCESS_LOGS)
      copy_replay_profile(); 
  }

  if (add_delay)
    sleep(add_delay);

  //Manish: we need non-shared memory to make per_proc_vgroup_table
  //free_structs();  /* This is done here because log_user_profile needs to read the old structs.  After this, cannot call log_user_profile */

  /*This function should call after allocating shared memory*/
  init_cache_table();
  init_vuser_grp_data(); //To enable vuser trace group data

  tr069_parent_init();

  //Alloc for NS vars string APIs
  if (string_init() == -1)
    NS_EXIT(-1, CAV_ERR_1031046, sizeof(char)*1024);

  call_user_test_init_for_each_sess();

  if (use_geoip_db)
    get_an_IP_address_for_area(0);

  /* Netomni Changes: As we are not using sys/master.conf file
   * Hence Commented code to copy sys/master.conf in test run directory
   * if (loader_opcode == MASTER_LOADER) // we are coping it here because system generates sig child signal 
  {
    // Copy master.conf to testrun dir 
    snprintf(cmd, sizeof(cmd), "cp sys/master.conf logs/TR%d/master.conf", testidx);
    nslb_system(cmd,1,err_msg);
  }
  else if (loader_opcode == CLIENT_LOADER)
    fprint2f(console_fp, rfp, "Running as generator. Master IP address is %s, port = %d\n", master_ip, master_port);*/

  //(void) signal( SIGCHLD, handle_parent_sickchild );

  /* End of common init related stuff */

  run_length = global_settings->test_stab_time;
  original_num_process = global_settings->num_process;
  original_progress_msecs = global_settings->progress_secs;

  ns_target_buf[0] = '\0';
  switch (testcase_shr_mem->mode) 
  {
    case TC_MEET_SLA:
      run_mode = FIND_NUM_USERS;
      strcpy(ns_target_buf, "SLAs");
      break;

    case TC_MEET_SERVER_LOAD:
#if 0
      udp_array =  (udp_ports *) realloc ((char *)udp_array, (1 /* from the CPU_HEALTH port */ + total_metric_entries) * sizeof(udp_ports));
      for (i = 0; i < total_metric_entries; i++) 
      {
        if ((udp_temp_fd = nslb_udp_server(metric_table_shr_mem[i].port, 1)) < 0) 
        {
	  perror("netstorm:  Error in creating cpu UDP listen socket.  Aborting...\n");
	  exit(1);
        }

        udp_array[total_udpport_entries].fd_num = udp_temp_fd;
        udp_array[total_udpport_entries].samples_awaited = metric_table_shr_mem[i].min_samples + 1;  /* adding one since the first one must be ignored */
        udp_array[total_udpport_entries].cmp = metric_table_shr_mem[i].relation;
        udp_array[total_udpport_entries].value_rcv = -1;

        metric_table_shr_mem[i].udp_array_idx = total_udpport_entries;

        total_udpport_entries++;
      }
#endif
      run_mode = FIND_NUM_USERS;
      strcpy(ns_target_buf, "Server Loading");
      break;

    case TC_FIX_CONCURRENT_USERS:

    case TC_FIX_USER_RATE:
      run_mode = NORMAL_RUN; // CHILD_INIT
      break;

    case TC_FIX_TX_RATE:
      strcpy(ns_target_buf, "Tx Rate (Per Minute)");
      if (testcase_shr_mem->target_rate == 0) 
      {
        NSTL1(NULL, NULL, "need a target hit rate for FIX_TX_RATE test mode");
        NS_EXIT(-1, CAV_ERR_1031047, "Transaction", "Transactions", "transaction", "transaction", "transaction");
      }
      run_mode = FIND_NUM_USERS;
      break;

    case TC_FIX_PAGE_RATE:
      strcpy(ns_target_buf, "Page Hit Rate (Per Minute)");
      if (testcase_shr_mem->target_rate == 0) 
      {
        NSTL1(NULL, NULL, "need a target hit rate for FIX_PAGE_RATE test mode");
        NS_EXIT(-1, CAV_ERR_1031047, "Page Hit", "Pages", "page", "page", "page");
      }
      run_mode = FIND_NUM_USERS;
      break;

    case TC_FIX_HIT_RATE:
      strcpy(ns_target_buf, "URL Hit Rate (Per Minute)");
      if (testcase_shr_mem->target_rate == 0) 
      {
        NSTL1(NULL, NULL, "need a target hit rate for FIX_HIT_RATE test mode");
        NS_EXIT(-1, CAV_ERR_1031047, "URL Hit", "Hits", "URL", "URL", "URL");
      }
      run_mode = FIND_NUM_USERS;
      break;

    case TC_FIX_MEAN_USERS:
      strcpy(ns_target_buf, "Mean Users: ");
      run_mode = FIND_NUM_USERS;
      break;
  }

  if (run_mode == FIND_NUM_USERS) 
  {
    is_goal_based = 1;
    if (global_settings->load_key) 
    {
      run_variable = &global_settings->num_connections;
    } 
    else 
    {
      run_variable = &global_settings->vuser_rpm;
    }

    if (testcase_shr_mem->guess_type == GUESS_RATE)
      *run_variable = testcase_shr_mem->guess_num * SESSION_RATE_MULTIPLE;
    else
      *run_variable = testcase_shr_mem->guess_num;

    global_settings->test_stab_time = testcase_shr_mem->stab_run_time;

    if (original_progress_msecs % global_settings->cap_consec_samples)
      global_settings->progress_secs = (global_settings->test_stab_time * 1000) / global_settings->cap_consec_samples;
    else
      global_settings->progress_secs = (global_settings->test_stab_time * 1000) / (global_settings->cap_consec_samples+1);

    if (global_settings->progress_secs < 1000) 
    {
      global_settings->progress_secs = 1000;
      global_settings->test_stab_time = (global_settings->progress_secs * (global_settings->cap_consec_samples+1)) / 1000;
    }

    NSDL3_TESTCASE(NULL, NULL, "The run_mode = 'Discovery Run', load_key = %d, *run_variable = %d, cap_consec_samples = %d, stab_run_time = %d, global_settings->progress_secs has been set to '%d', and global_settings->test_stab_time set to '%d'", global_settings->load_key, *run_variable, global_settings->cap_consec_samples, testcase_shr_mem->stab_run_time, global_settings->progress_secs, global_settings->test_stab_time);
  }
  
  //for KEYWORD:NS_PARENT_LOGGER_LISTEN_PORTS changes applied here
  if(global_settings->ns_use_port_min && loader_opcode == MASTER_LOADER) 
  {
    //parent_port_number = get_ns_port_defined(global_settings->ns_use_port_max, global_settings->ns_use_port_min, 1);   
    parent_port_number = get_ns_port_defined(global_settings->ns_use_port_max, global_settings->ns_use_port_min, 1, &listen_fd, PARENT_LISTEN_PORT);  
    g_dh_listen_port = get_ns_port_defined(global_settings->ns_use_port_max, global_settings->ns_use_port_min, 1, &data_handler_listen_fd, DH_LISTEN_PORT);  
   //calling for dh port
  }
  else 
  {
    parent_port_number = init_parent_listner_socket_new(&listen_fd, global_settings->ctrl_server_port);
    if(parent_port_number == 0) {
      NS_EXIT(-1, "Parent port number cannot be 0, please re-run the test");
    }
    g_dh_listen_port = init_parent_listner_socket_new(&data_handler_listen_fd, 0); 
    if(g_dh_listen_port == 0) {
      NSTL1(NULL, NULL, "Thread port number is 0.");
      NS_EXIT(-1, "Thread port number cannot be 0, please re-run the test");
    }
  }
  NSTL1(NULL, NULL, "Parent/Controller is listenning on %d port and "
                    "data controller port = %d.", parent_port_number, g_dh_listen_port);
  nde_set_parent_port_number(parent_port_number);  //setting port in partitionInfo struct
  nslb_save_partition_info(testidx, g_partition_idx, &partitionInfo, partition_info_buf_to_append);  //saving port in partition_info

  if(loader_opcode != CLIENT_LOADER)  //rstat will run only for Standalone and Master
    init_rstat(num_server_unix_rstat, server_stat_ip);


  if (total_smtp_request_entries) {
    sprintf(smtp_body_hdr_begin,
            "--%s\r\n"
            "Content-Type: text/plain;\r\n"
            "Content-Transfer-Encoding: 7 bit\r\n\r\n",
            attachment_boundary);
    smtp_body_hdr_begin_len = strlen(smtp_body_hdr_begin);
  }
 
  // CAV MAIN: PARENT_INIT OR CHILD_INIT
  char state_transition_model_file[MAX_FILE_NAME];
  char log_file_name[1024];
  sprintf(state_transition_model_file, "%s/%s", g_ns_wdir, HTTP_STATE_MODEL_FILE_NAME);
  sprintf(log_file_name, "%s/logs/%s/ns_files/state_transition_table.log", g_ns_wdir, global_settings->tr_or_common_files); 
  /*State Transition map for HTTP(S)*/
  nslb_init_http_state_transition_model(state_transition_model_file, HdrStateArray, log_file_name, NS_MAX_HDST_ID, HDST_TEXT, ns_get_hdr_callback_fn_address);
   /*
   Here we have processed all the static transaction and max_tx_entries is no more useful now 
   as we have created the shared memory using total_tx_entries. 
   So we are reusing the max_tx_entries to control the dynamic transaction memory reallocation. 
   So setting the max_tx_entries to total_tx_entries that will be checked to against the 
   max_tx_entries to reallocate the memory for dynamic transactions.
  */
  //max_tx_entries = total_tx_entries;  

  // Dump all transition states to logs/TRXXX/ns_files/state_transition_table.log
  // nslb_dump_state_transition_table();
  //parent_init_src_ip();

}

struct iovec *vector;
int *free_array;
int *body_array; /*bug 78106 - array to identify post body*/
//int io_vector_size = 10000; //Initial number of io vectors.This value gets updated if grow_io_vector gets called.
//int io_vector_init_size = 10000; //Initial number of io vectors malloced
//int io_vector_delta_size = 1000; //Increment size for reallocing io vectors
//int io_vector_max_size = 100000; //Maximum numbers of io vectors for whole test
//int io_vector_max_used = 0;//Used to keep check of last maximum number vectors used to make request

static void parent_init_before_starting_test_run()
{ 
  NSDL2_PARENT(NULL, NULL, "Method called");
  static int db_flag = 0;
  if (global_settings->cap_status != NS_CAP_CHK_OFF)
    global_settings->cap_status = NS_CAP_CHK_INIT;

  /*Join db table thread here as we are now starting test*/
  if(global_settings->reader_run_mode && (loader_opcode != CLIENT_LOADER)) {
     if(!db_flag)
     { 
        //db_flag = 1;
        //pthread_attr_destroy(&db_table_attr);
        if(pthread_join(db_table_thread_id, (void *)&db_thread_ret_val))
        {
           NSTL1(NULL, NULL, "pthread_join(): Error in joining DB tables thread, exiting!");
           NS_EXIT_EX(-1, NS_DB_CREATION, CAV_ERR_1000040);
        }
     }
  }
  
  // reset_udp_array();
  //start_accepting_metrics = 0;
  // Below code has been moved outside of this function
  /* 
  if (global_settings->smon)
  {
    write_log_file(NS_MONITOR_SETUP, "Waiting for sockets to clear if time-wait sockets are greater than %d",
                                      global_settings->tw_sockets_limit);
    wait_sockets_clear();
  } */
  dis_timer_init();


  //if (global_settings->report_level >= 1) Always on. Either to stdout or to GUI
  do_verbose = 1;
  //    init_parent_listner_socket();

#ifndef NS_PROFILE
  parent_pid = getpid();
#else
  global_settings->num_process = 1;
#endif

  //dump HOST TABLE data in slog file
  // Below code has been moved outside of this function
/*
  write_log_file(NS_MONITOR_SETUP, "Saving host table");
  if(log_host_table() == -1) {
    NSTL1_OUT(NULL, NULL, "Error in writing HOSTTABLE in slog file");
    NSDL2_RBU(NULL, NULL, "Error in writing HOSTTABLE in slog file..");
  }

  end_stage(NS_MONITOR_SETUP, TIS_FINISHED, NULL); */
  //TODO: end monitor setup here
  /* master_init must be called befaore setup_schedule_for_nvm as this function sets
   * global_settings->num_process whch need total_client_entries which is set by master_init*/
  if(loader_opcode == MASTER_LOADER) {
    SAVE_IP_AND_CONTROLLER_TR
    init_stage(NS_UPLOAD_GEN_DATA);  // stage upload generator data
    master_init(g_conf_file);  // It will not return error
    log_generator_table();
    end_stage(NS_UPLOAD_GEN_DATA, TIS_FINISHED, NULL);  //end of stage upload generator data
    //write_generator_table_in_csv();
  }

  if(run_mode == NORMAL_RUN)
  {
     init_stage(NS_START_INST);
     write_test_init_header_file(target_completion_time);
       
  }
  //Runtime changes for quantity with different increase or decrease mode  
  runtime_schedule_malloc_and_memset();

  
  setup_schedule_for_nvm(original_num_process);
  //write_test_init_header_file(target_completion_time);

  /*This is to log  log_phase_table.csv file.
 * In this we dump all phases. Phases distributon happens in setup_schedule_for_nvm()
 * So, moved code here to log static table.*/
  if(log_records && (get_max_report_level() >= 2))
  {
    extern int log_phase_table();
    if (log_phase_table() == -1)
      NS_EXIT(-1, "log_phase_table() failed");
  }
  
  //Check Pool size and user details for FCS
  if((global_settings->concurrent_session_mode) && (loader_opcode != MASTER_LOADER)) {
      check_fcs_user_and_pool_size();
  }

  //creates shared memory for users w.r.t each child
  create_user_num_shm();
  //mv to parent_init_after_parsing_args - last line, comment it, but not working, was making segmentation fault, so has been reverted back
  if(!db_flag)
  {
    parent_init_src_ip();
    db_flag = 1;
  }
  global_settings->test_start_time = get_ms_stamp();
  global_settings->test_runphase_start_time = 0;
  global_settings->test_duration = 0;
  global_settings->test_runphase_duration = 0;
  init_eth_data();

/*   if(loader_opcode != CLIENT_LOADER)  //rstat will run only for Standalone and Master */
/*     init_rstat(num_server_unix_rstat, server_stat_ip); */

  /*update_eth_bytes();
    first_eth_rx_bytes = next_eth_rx_bytes;
    first_eth_tx_bytes = next_eth_tx_bytes;*/

  init_gdf_all_data(); // To init GDF data for each Test Sample // TO DO CAV MAIN: CHILD INIT
  is_sys_healthy(1, NULL); // Pass 1 as 1st argument for init

  /* We will have to called this as this default value is dependent upon PROGRESS_SECS */
  init_default_parent_epoll();

  init_vuser_summary_table(0); // To init vuser summary table for runtime control // // TO DO CAV MAIN: CHILD INIT
  if(loader_opcode == MASTER_LOADER) {
    fprint2f(console_fp, rfp, "Running as controller. Total number of generators are %d\n", sgrp_used_genrator_entries);
    return;
  }
  validate_cpu_affinity();  // Added by Anuj for CPU_AFFINITY : 25/03/08 // TO DO CAV MAIN: PARENT INIT
  init_io_vector(); // To init io vector used in insert segments //// TO DO CAV MAIN: CHILD INIT
  // Done for Goal based scenario
  if(is_goal_based)
    init_all_avgtime();
}

#define CREATE_EMPTY_FILE_IN_TR_FOR_STATUS(fname) \
{ \
  FILE *fp = NULL; \
  char filename[512] = {0}; \
  snprintf(filename, 512, "%s/logs/TR%d/%s", g_ns_wdir, testidx, fname); \
  fp = fopen (filename, "w"); \
  if(fp) \
    fclose(fp); \
}

static void parent_chk_all_process_running_befor_start_test()
{
 }

static void kw_set_continous_monitoring_check_demon_usage(char *err_msg)
{
  fprintf(stderr, "Error: Invalid value of CHECK_CONTINUOUS_MON_HEALTH keyword: %s\n", err_msg);
  fprintf(stderr, "  Usage: CHECK_CONTINUOUS_MON_HEALTH <mode> <start test val> <start ndc val> <rtg diff count> "
                  "<tracing log level> <send mail to list> <send mail to cc list> <send mail to bcc list>\n");
  fprintf(stderr, "  Where:\n");
  fprintf(stderr, "    <mode> : Mode for enable/disable. It can be 0, 1 (default is enable). Keyword is enabled only in case of Continuous Monitoring\n");
  fprintf(stderr, "    <Start test val> : Start test val is count that indicate how many times tool will check test is started or not in the interval of 30 secs in case of failure to start test.\n");
  fprintf(stderr, "    <Start ndc val>  : Start ndc val is count that indicate how many times tool will try to restart the ndc in interval of 20 secs in case of failure to start ndc.\n");
  fprintf(stderr, "    <Rtg diff count> : Maximum time to wait for rtg update and it will be multiple of progress interval(for e.g Progress Interval * rtg diff count )\n");
  fprintf(stderr, "    <Tracing log level> : Level for logs. It can be 1 or 2 (1: Update on Error, 2: Update on progress Interval)\n");
  fprintf(stderr, "    <To list> : to send mail in to list\n");
  fprintf(stderr, "    <CC list> : to send mail in cc list (Optional)\n");
  fprintf(stderr, "    <BCC list> : to send mail in bcc list (Optional)\n");
  NS_EXIT(-1, "%s\nUsage: CHECK_CONTINUOUS_MON_HEALTH <mode> <start test val> <start ndc val> <rtg diff count> "
                  "<tracing log level> <TO list> <CC list> <BCC list>", err_msg);
}

int kw_set_continous_monitoring_check_demon(char *buf)
{
  char keyword[1024] = {0};
  char mode_str[16] = "1";
  char start_test_str[16] = {0};
  char start_ndc_str[16] = {0};
  char rtg_diff_ptr[16] = {0};
  char trace_log_level[16] = {0};
  char send_mail_to_list[2048] = "NA";
  char send_mail_cc_list[2048] = "NA";
  char send_mail_bcc_list[2048] = "NA";
  char tmp[1024] = {0};
  int num, mode = 1, start_test_val = 20, start_ndc_val = 1, rtg_diff_count = 2, log_level = 1;

  num = sscanf(buf, "%s %s %s %s %s %s %s %s %s %s", keyword, mode_str, start_test_str, start_ndc_str, rtg_diff_ptr, trace_log_level, send_mail_to_list, send_mail_cc_list, send_mail_bcc_list, tmp);

  NSDL2_PARSING(NULL, NULL, "Method Called, buf = %s", buf);

  if (num < 2 || num > 9){
     kw_set_continous_monitoring_check_demon_usage("Invalid number of arguments");
  }

  if((ns_is_numeric(mode_str) == 0) || (ns_is_numeric(start_test_str) == 0) || (ns_is_numeric(start_ndc_str) == 0) || (ns_is_numeric(rtg_diff_ptr) == 0) || (ns_is_numeric(trace_log_level) == 0))
  {
    kw_set_continous_monitoring_check_demon_usage("CHECK_CONTINUOUS_MON_HEALTH (mode||start test val||start_ndc_val||rtg_diff_count||trace_log_level) is not numeric");
  }

  mode = atoi(mode_str);
  if(mode < 0 || mode > 1)
  {
    kw_set_continous_monitoring_check_demon_usage("CHECK_CONTINUOUS_MON_HEALTH mode is not valid");
  }

  if(start_test_str[0] != '\0') {
    start_test_val = atoi(start_test_str);
  }

  if(start_ndc_str[0] != '\0') {
    start_ndc_val = atoi(start_ndc_str);
  }
  
  if(rtg_diff_ptr[0] != '\0') {
    rtg_diff_count = atoi(rtg_diff_ptr);
  }
  
  if(trace_log_level[0] != '\0') {
    log_level = atoi(trace_log_level);
    if(log_level < 1 || log_level > 2) {
      kw_set_continous_monitoring_check_demon_usage("CHECK_CONTINUOUS_MON_HEALTH (log level) is invalid");
    }
  }

  global_settings->continuous_mon_check_demon = mode;
  global_settings->continuous_mon_check_demon_start_test_val = start_test_val;
  global_settings->continuous_mon_check_demon_start_ndc_val = start_ndc_val;
  global_settings->continuous_mon_check_demon_rtg_diff_count = rtg_diff_count;
  global_settings->continuous_mon_check_demon_trace_log_level = log_level;
  strcpy(global_settings->continuous_mon_check_demon_to_list, send_mail_to_list);
  strcpy(global_settings->continuous_mon_check_demon_cc_list, send_mail_cc_list);
  strcpy(global_settings->continuous_mon_check_demon_bcc_list, send_mail_bcc_list);

  NSDL2_PARSING(NULL, NULL, "global_settings->continuous_mon_check_demon = %d, global_settings->continuous_mon_check_demon_start_test_val = %d, global_settings->continuous_mon_check_demon_start_ndc_val = %d, global_settings->continuous_mon_check_demon_to_list = %s, global_settings->continuous_mon_check_demon_cc_list = %s, global_settings->continuous_mon_check_demon_bcc_list = %s, global_settings->continuous_mon_check_demon_rtg_diff_count = %d, global_settings->continuous_mon_check_demon_trace_log_level = %d", global_settings->continuous_mon_check_demon, global_settings->continuous_mon_check_demon_start_test_val, global_settings->continuous_mon_check_demon_start_ndc_val, global_settings->continuous_mon_check_demon_to_list, global_settings->continuous_mon_check_demon_cc_list, global_settings->continuous_mon_check_demon_bcc_list, global_settings->continuous_mon_check_demon_rtg_diff_count, global_settings->continuous_mon_check_demon_trace_log_level);
  return 0;
}

/*bug 92660: cavisson lite : removed static type as need to use in cavmain*/
void create_nsport_file(char *port_file, unsigned short port_num)
{
  char mybuf[64];
  FILE *mfp;

  sprintf(mybuf, "logs/TR%d/%s", testidx, port_file);
  mfp = fopen(mybuf, "w");
  if (mfp) 
  {
    fprintf(mfp, "%hu\n", port_num); /* changed to %hu because in fc8 it was writing negative port. */
    fclose(mfp);
  }
}

//Runtime changes init
void init_runtime_data_struct(){
  MY_MALLOC_AND_MEMSET(rtcdata, sizeof(RTCData), "rtcdata variable", -1);
  rtcdata->cur_state = RESET_RTC_STATE;
}

//To apply breakpoint
static void start_debug_script()
{
  NSDL2_PARENT(NULL, NULL, "Method called");
}
 
static void start_test()
{
  pid_t child_pid;
  static int first_time = 1; // to specify that it the first test sample
  int i, ret, ret1;
  char err_msg[100];
  char* env_buf;
  char *ip;
  ClientData cd;
  time_t mseconds_before, mseconds_after, mseconds_elapsed;
  char mseconds_buff[100];
  char tmp_buff[1024] = {0};
  pthread_t data_handler_thid;
  static int per_one_time = 1;
    
  NSDL2_PARENT(NULL, NULL, "Method called");

  //We are making stout as unbuffered, this is for NetStormScriptDebugger test 
  if(g_debug_script)
    setvbuf(stdout, NULL, _IONBF, 0);

  /* Moved from wait_forever,
  * As we should do all monitor setup task before forking child, we dont want to setup all monitor at that time when
  * all childs has started their jobs
  */
  // Monitor are handled by standalone parent or master only
  
  // TODO-AN : We create epoll for parent inside PARENT block but here in monitor_setup/custom_monitor_setup we have some events
  if(loader_opcode != CLIENT_LOADER) {
    ip = "127.0.0.1"; // used by Event Logger
  } else {
    ip = master_ip; // used by Event Logger
  }

  // We will send message to NDC to start the monitor for custom_monitors.

/*  if(global_settings->continuous_monitoring_mode && global_settings->continuous_mon_check_demon)
  {
    char *ptr;
    if((ptr = getenv("START_TEST_BY_DAEMON")) == NULL) {
      NSTL1(NULL, NULL, "Here test was stopped by nsu_stop_test command, then start deamon(nsu_check_cont_test).");
      sprintf(tmp_buff, "nohup %s/bin/nsu_check_cont_test -s %s/%s/%s -t %d -p %d -c %d -n %d -T %s -C %s -B %s -D %d -d %d -l %d -u %s -r admin &", g_ns_wdir, g_project_name, g_subproject_name, g_scenario_name, testidx, global_settings->net_diagnostics_mode ? global_settings->net_diagnostics_port : -1, global_settings->continuous_mon_check_demon_start_test_val, global_settings->continuous_mon_check_demon_start_ndc_val, global_settings->continuous_mon_check_demon_to_list, global_settings->continuous_mon_check_demon_cc_list, global_settings->continuous_mon_check_demon_bcc_list, (global_settings->progress_secs/1000), global_settings->continuous_mon_check_demon_rtg_diff_count, global_settings->continuous_mon_check_demon_trace_log_level, g_test_user_name);
      if(system(tmp_buff) != 0) {
        NSTL1(NULL, NULL, "ERROR: Could not run cmd [%s]", tmp_buff);
        NS_EXIT(-1, "ERROR: Could not run cmd [%s]\n", tmp_buff);
      }
    } 
  }
*/
  if(global_settings->continuous_monitoring_mode && global_settings->continuous_mon_check_demon)
  {
    char *ptr;
    int pid;
    write_log_file(NS_START_INST, "Starting continuous monitoring daemon. Daemon restarts monitoring when it got stopped");
    if((ptr = getenv("START_TEST_BY_DAEMON")) == NULL)
    {
      NSTL1(NULL, NULL, "Here test was stopped by nsu_stop_test command, then start deamon(nsu_check_cont_test).");
     // Create child process for daemon(nsu_check_cont_test)
     if((pid = fork()) == 0) {
       NSTL1(NULL, NULL, "nsu_check_cont_test process created(fork) with pid = %d", getpid());
       // Close all connection for the child(nsu_check_cont_test) which was inherited from its parent.
       // So that it can not be able to make connection with other processes(like ndc etc.) when its parent got killed.
       nslb_close_all_open_files(-1,0,NULL);

       char nsu_check_cont_test_bin_path[1024 + 1];
       char scen_path[512 + 1];
       char test_run_num [10 + 1];
       char nd_port [10 + 1];
       char nd_ip [32 + 1];
       char cont_mon_demon_start_test_val [10 + 1];
       char cont_mon_check_start_ndc_val [10 + 1];
       char cont_mon_check_demon_rtg_diff_count [10 + 1];
       char cont_mon_check_demon_trace_log_level [10 + 1];
       char cont_mon_check_demon_tsdb_mode [10 + 1];
       char progress_interval [10 + 1];
       char *cont_mon_check_demon_to_list;
       char *cont_mon_check_demon_cc_list;
       char *cont_mon_check_demon_bcc_list;
       char *test_user_name;
       
       sprintf(nsu_check_cont_test_bin_path, "%s/bin/nsu_check_cont_test", g_ns_wdir);
       sprintf(scen_path, "%s/%s/%s", g_project_name, g_subproject_name, g_scenario_name);
       sprintf(test_run_num, "%d", testidx);
       sprintf(nd_port, "%d", global_settings->net_diagnostics_mode ? global_settings->net_diagnostics_port : -1);
       sprintf(nd_ip, "%s", global_settings->net_diagnostics_mode ? global_settings->net_diagnostics_server : "NA");
       sprintf(cont_mon_demon_start_test_val, "%d", global_settings->continuous_mon_check_demon_start_test_val);
       sprintf(cont_mon_check_start_ndc_val, "%d", global_settings->continuous_mon_check_demon_start_ndc_val);
       sprintf(cont_mon_check_demon_rtg_diff_count, "%d", global_settings->continuous_mon_check_demon_rtg_diff_count);
       sprintf(cont_mon_check_demon_trace_log_level, "%d", global_settings->continuous_mon_check_demon_trace_log_level);
       sprintf(progress_interval, "%d", (global_settings->progress_secs/1000));
       sprintf(cont_mon_check_demon_tsdb_mode, "%d", g_tsdb_configuration_flag);
       cont_mon_check_demon_to_list = global_settings->continuous_mon_check_demon_to_list;
       cont_mon_check_demon_cc_list = global_settings->continuous_mon_check_demon_cc_list;
       cont_mon_check_demon_bcc_list = global_settings->continuous_mon_check_demon_bcc_list;
       test_user_name = g_test_user_name;

       if(execlp(nsu_check_cont_test_bin_path, "nsu_check_cont_test",
                 "-s", scen_path,
                 "-t", test_run_num,
                 "-I", nd_ip,
                 "-p", nd_port,
                 "-c", cont_mon_demon_start_test_val,
                 "-n", cont_mon_check_start_ndc_val,
                 "-T", cont_mon_check_demon_to_list,
                 "-C", cont_mon_check_demon_cc_list,
                 "-B", cont_mon_check_demon_bcc_list,
                 "-D", progress_interval,
                 "-d", cont_mon_check_demon_rtg_diff_count,
                 "-l", cont_mon_check_demon_trace_log_level,
                 "-u", test_user_name,
		 "-S", cont_mon_check_demon_tsdb_mode,  //Add for TSDB mode for RTG disabled
                 "-r",
                 "admin", NULL) == -1)
       {
        NSTL1(NULL, NULL, "ERROR: Could not run cmd [%s]", tmp_buff);
        NS_EXIT(-1, "ERROR: Could not run cmd [%s]\n", tmp_buff);
       }
     }
     else {
       if(pid < 0)
        fprintf(stderr, "Error: In creating nsu_check_cont_test process\n");
     }
    }
  }
  else
  {
    int pid;
    write_log_file(NS_START_INST, "Starting Performance test daemon. Daemon did post processing if its stop.");
    {
      NSTL1(NULL, NULL, "Here test was stopped by nsu_stop_test command, then start deamon(nsu_check_perf_test).");
     // Create child process for daemon(nsu_check_perf_test)
     if((pid = fork()) == 0) {
       NSTL1(NULL, NULL, "nsu_check_perf_test process created(fork) with pid = %d", getpid());
       // Close all connection for the child(nsu_check_perf_test) which was inherited from its parent.
       // So that it can not be able to make connection with other processes(like ndc etc.) when its parent got killed.
       nslb_close_all_open_files(-1,0,NULL);

       char test_run_num [10 + 1];
       char nsu_check_perf_test[1024 + 1];
       char cont_mon_check_demon_trace_log_level [10 + 1];
       char progress_interval [10 + 1];
       
       sprintf(nsu_check_perf_test, "%s/bin/nsu_check_perf_test", g_ns_wdir);
       sprintf(test_run_num, "%d", testidx);
       sprintf(progress_interval, "%d", (global_settings->progress_secs/1000));
       sprintf(cont_mon_check_demon_trace_log_level, "%d", global_settings->continuous_mon_check_demon_trace_log_level);

       if(execlp(nsu_check_perf_test, "nsu_check_perf_test",
                 "-t", test_run_num,
                 "-D", progress_interval,
                 "-l", cont_mon_check_demon_trace_log_level,
                 NULL) == -1)
       {
        NSTL1(NULL, NULL, "ERROR: Could not run cmd [%s]", tmp_buff);
        NS_EXIT(-1, "ERROR: Could not run cmd [%s]\n", tmp_buff);
       }
     }
     else {
       if(pid < 0)
        fprintf(stderr, "Error: In creating nsu_check_perf_test process\n");
     }
    }
  }
  
  // moved just before starting goal based while loop
  //if (loader_opcode != CLIENT_LOADER) 
  /* fflush must be between the last write to all files in parent and forking of the children */
  fflush(NULL); /* flush all fd's */
/*   fflush(rfp);  */
/*   fflush(srfp); */
  
    
  //Create .test_started.status file for bug 210 and to check all the scripts parsing have been done succussfully  
  if((!g_enable_test_init_stat) && (per_one_time == 1)){
    per_one_time = 0;
    WRITE_TR_STATUS
  }

  if(loader_opcode == MASTER_LOADER)
    goto Master_entry;

#ifndef NS_PROFILE
 
  // Dynamic Hosts
  // If > 0, then malloc gserxxx with total + dyn host
  // change total 
  // Dynamic Host: Parent will check if max_dyn_host value > 0 then next malloc recorded host tbl.
  setup_rec_tbl_dyn_host(global_settings->max_dyn_host); 

  /********************************************************************************************************
   DO NOT WRITE ANY CODE WHICH FORK ANY CHILD PROCESS OTHER THAN NVM BELOW THIS LINE TILL NVM ARE FORKED.
   ********************************************************************************************************/

  if(g_debug_script) 
    start_debug_script();
 
  for (i=0; i < global_settings->num_process ; i++)  
  {

   /* If nsl_unique_range var API is present then create_unique_range_var_table_per_proc method is called for each NVM, this method
      is used to create UniqueRangeVarPerProcessTable.
    * Memory is allocated for UniqueRangeVarPerProcessTable only for NVM0, other NVMs overwrite this memory*/
    if(total_unique_rangevar_entries){
      if(i == 0){
        MY_MALLOC(unique_range_var_table, total_unique_rangevar_entries * sizeof(UniqueRangeVarPerProcessTable), "unique_range_var_table", i);
      }
      create_unique_range_var_table_per_proc(i);
    }
    if(run_mode == NORMAL_RUN)
       write_log_file(NS_START_INST, "Creating Cavisson Virtual Machine (%d out of %d)", i+1, global_settings->num_process);
    MY_MALLOC(env_buf , 32, "env_buf ", -1); /* Memory leak - ignoring for now */
    v_port_table[i].env_buf = env_buf;
    sprintf(env_buf, "CHILD_INDEX=%d", i);
    putenv(env_buf);
    if ((child_pid = fork()) < 0)  
    {
      perror("*** server:  Failed to create child process.  Aborting...\n");
      NSTL1(NULL, NULL, "*** server:  Failed to create child process.  Aborting...");
      NS_EXIT(1,  "");
    }
    if (child_pid > 0) 
    {
      v_port_table[i].pid = child_pid;
        NSDL2_MISC(NULL, NULL, "### server:  Created child process with pid = %d.\n", child_pid);
      set_cpu_affinity(i, child_pid); // Added by Anuj for CPU_AFFINITY : 25/03/08
      sprintf(tmp_buff, "child_pid[%d]", i);
      ret = nslb_write_all_process_pid(child_pid, tmp_buff, g_ns_wdir, testidx, "a");
      if( ret == -1)
      {
        //NSTL1_OUT(NULL, NULL, "failed to open the child_pid[%d] pid file", i);
        NSTL1(NULL, NULL, "failed to open the child_pid[%d] pid file", i);
        END_TEST_RUN
      }
      char tmp_buff[100];
      sprintf(tmp_buff,"CVM%d.pid",i+1);
      ret1 = nslb_write_process_pid(child_pid,"ns child's pid" ,g_ns_wdir, testidx, "w",tmp_buff,err_msg);
      if( ret1 == -1)
      { 
        NSTL1_OUT(NULL, NULL, "failed to open the child_pid[%d] pid file","%s",i,err_msg);
      }
    }
    else
    {
      INIT_ALL_ALLOC_STATS
      break;
    }
  }

  free_unique_range_var();


  if (PARENT) 
  {

    Master_entry:
      if (ns_sickchild_pending) 
      {
        kill_all_children((char *)__FUNCTION__, __LINE__, __FILE__);
        exit(1);
      }
      ns_parent_state = NS_PARENT_ST_TEST_STARTED;
      //if RESET_TEST_START_TIME_STAMP is set then create summary.top here.
      handle_reset_test_start_time_stamp();
      NSDL2_MESSAGES(NULL, NULL, "ns_parent_stat = %d\n", ns_parent_state);      
      //printf ("Parent Starting (%d)\n", getpid());
      start_time = get_ms_stamp();
      // (void) signal( SIGINT, SIG_IGN );
      (void) signal( SIGINT, handle_parent_sigint);
      (void) signal( SIGUSR1, handle_parent_sigusr1 );
      (void) signal( SIGPIPE, handle_parent_sigpipe); 
      (void) signal( SIGRTMIN+1, handle_parent_sigrtmin1); 
      (void) signal( SIGRTMIN+3, handle_parent_sigrtmin3); 
      //(void) signal( SIGINT, handle_sigint );
      wait_for_child_registration_control_and_data_connection(global_settings->num_process , CONTROL_MODE);
      //accept_and_timeout(global_settings->num_process);

      //Creating NSPort here after making connection with NVM's so that component that makes connection
      create_nsport_file("NSPort", parent_port_number);
      create_nsport_file("NSDHPort", g_dh_listen_port);
  //    if (!(g_tsdb_configuration_flag & RTG_MODE))
//       tsdb_init();
      start_ns_lps();    //It needs nsport, hence calling it there. 

      /* Resolve bugs - 26444, 20005 and 22515 
         Root Cause:-
           This is due to NetStorm engine design limitation.
           Current design - It is assumed that NS parent first make connection with NVMs then  only make connection with any tools. 
           Here while NS parent is waiting to make connection with NVMs, at the same time tool get a chance to make connection with NS parent 
           and g_msg_com_con store (tool + NVMs) connection information. It corrupt g_msg_com_con array for one of the NVMs 
           and hence RTC become failed. 
         New Design:-
           NS will make empty file ".parent_nvm_con_done" on TRxxx.  Any tools like nsi_rtc_invoker, transaction etc will check existance of 
           this file, if this file exist then only tools can try to make TCP-IP connection with NS otherwise through an error message -
           "NetStorm is in initilation state. Runtime changes is not applied now. Please try after sometime! :)".
      */
      CREATE_EMPTY_FILE_IN_TR_FOR_STATUS(".parent_nvm_con.status");
 
      //make connections for failed java process server signature & add in parent epoll
      //if(java_process_server_sig_enabled)
      //{
        //make_connections_for_java_process_server_sig();          
      //}

      mseconds_before = get_ms_stamp();
      start_check_monitor(CHECK_MONITOR_EVENT_START_OF_TEST, "");
      mseconds_after = get_ms_stamp();
      mseconds_elapsed = mseconds_after - mseconds_before;
      convert_to_hh_mm_ss(mseconds_elapsed, mseconds_buff);
      //NSTL1_OUT(NULL, NULL, "Time taken in start_check_monitor(): %s",mseconds_buff);
      NSTL1(NULL, NULL, "Time taken in start_check_monitor(): %s",mseconds_buff);
      /*Event Logger*************************************************/
      /*Connect PARENT to Event Logger*/
      if(global_settings->enable_event_logger) {
         if ((event_logger_fd = connect_to_event_logger(ip, event_logger_port_number)) < 0)  {
            fprintf( stderr, "%s:  Error in creating the TCP socket to"
   					"communicate with the nsa_event_logger (%s:%d)."
					" Aborting...\n", (loader_opcode == STAND_ALONE)?"NS parent":(loader_opcode == CLIENT_LOADER)?"Generator parent":"Controller parent", ip, event_logger_port_number);
            END_TEST_RUN
         }
         add_select_msg_com_con_ex(__FILE__, __LINE__, (char *)__FUNCTION__,(char*)&g_el_subproc_msg_com_con, event_logger_fd, EPOLLIN | EPOLLERR | EPOLLHUP);
         sprintf(g_el_subproc_msg_com_con.conn_owner, "PARENT_TO_EVENT_LOGGER"); 
      }
      /*Event Logger*************************************************/
      /*This function is used to save ctrl file.
       * Control file is used in static varivale for USE_ONCE mode.*/
      /*NC: In case of controller control file will be created by scenario distribution tool*/
      if (loader_opcode != MASTER_LOADER) {
        if(first_time)
          save_ctrl_file();
      } 
      first_time = 0;

      if (loader_opcode != CLIENT_LOADER)
      {
        add_select_cntrl_conn();
        add_select_custom_monitor();
      }
     
      if (loader_opcode == MASTER_LOADER)
      {
        //this below line should not be change, used in nsu_start_test for checking that All Clients are up and Master is running successfully ,it is used for showing status on GUI.
        //print2f_always(rfp, "Controller is ready for collecting data\n");
        NSTL1(NULL, NULL, "Controller is ready for collecting data");

        // Allocate memory for ns_nvm_scratch_buf of 20MB
        realloc_scratch_buffer(MASTER_SCRATCH_BUF_INIT_SIZE);
      }
      else if (loader_opcode == CLIENT_LOADER)
      {
        // Allocate memory for ns_nvm_scratch_buf of 20MB
        realloc_scratch_buffer(CLIENT_SCRATCH_BUF_INIT_SIZE);
      }

      //Production Monitoring enable 
      if(loader_opcode != CLIENT_LOADER) {
        apply_timer_for_new_tr(cd, ip, start_test_min_dur);
      }
      set_test_run_info_status(TEST_RUN_SCHEDULE);
      
      /*MSG QUEUE CREATION AND THREAD CREATION*/
      if(global_settings->write_rtg_data_in_db_or_csv)
      {
        NSTL1(NULL, NULL, "Creating msg queue for communication between ns_data_handler_thread "
                          "and ns_upload_rtg_data_in_db_or_csv_thread");
        msg_queue_creation_for_writing_rtg_data_in_db_or_csv();

        if(rtgDataMsgQueueAndTablesInfo_obj.rtgDataMsgQueueInfo_obj.rtgDataMsgMpoolInfo)
        {
          ns_upload_rtg_data_in_db_or_csv_thread_creation();
        }
      }
   
      ns_data_handler_thread_create(&data_handler_thid);
      nethavoc_init();
      end_results = wait_forever(global_settings->num_process, &c_end_results);
      ns_process_cav_memory_map();
      pthread_join(data_handler_thid, NULL);

      // We need this data for next run in case of goal based scenario
      if(is_goal_based){
        c_end_results = (cavgtime *)g_cur_finished[0];
        end_results = (avgtime *)g_end_avg[0];
        // This function would be called only in case of goal based
        // as in goal based we are ending test only when TARGET is achieved.
        // We have to release memory when a test sample gets over 
        clean_parent_memory_for_goal_based();
     }
  
      kill_children(); 
      ns_parent_state = NS_PARENT_ST_TEST_OVER;
      NSDL3_MESSAGES(NULL, NULL, "Setting parent test run status = %d", ns_parent_state);
     
      // Done for Goal based scenario as we need it again with initialized value 0, not with the last value
      memset(gen_updated_timeout, 0, MAX_GENERATOR_ENTRIES*sizeof(int));

      if(loader_opcode != CLIENT_LOADER)
        trace_log_end();    //to print footer of debug_trace.log file
      /*for (i = 0; i < global_settings->num_process; i++) 
      {
        free(v_port_table[i].env_buf);
      }
      free(v_port_table);*/
      if(debug_trace_log_value != 0 && (global_settings->protocol_enabled & RBU_API_USED))
        script_execution_log_end();    //to print footer of script_execution.log file
  
    }
    else 
    {
      loader_opcode = -1;  // Thu Feb 10 10:04:46 IST 2011
      (void) signal( SIGUSR2, SIG_IGN);
      prctl(PR_SET_PDEATHSIG, SIGKILL);

      if(global_settings->alert_info->enable_alert)
      {
        g_ns_alert = NULL;
        ns_alert_config();
      }

      if(global_settings->monitor_type)
      {
        g_ns_file_upload = NULL;
        ns_init_file_upload(global_settings->file_upload_info->tp_init_size, global_settings->file_upload_info->tp_max_size,
                            global_settings->file_upload_info->mq_init_size, global_settings->file_upload_info->mq_max_size);
       
        ns_config_file_upload(global_settings->file_upload_info->server_ip, global_settings->file_upload_info->server_port,
                              global_settings->file_upload_info->protocol, global_settings->file_upload_info->url,
                              global_settings->file_upload_info->max_conn_retry, global_settings->file_upload_info->retry_timer, ns_event_fd);
      }

      netstorm_child();
    }
#endif
}

static void tr069_processing_after_test_run(char *dir_path, unsigned long num_unused_records){
  char buf[1024];

  sprintf(buf, "nsu_tr069_post_process -d %s -r %lu -D %d -t %d > /dev/null 2>&1",
               dir_path, num_unused_records, group_default_settings->debug, testidx);
  printf("Cmd for tr069 = %s\n", buf);
  system(buf);
}

#if 0
static inline void run_nia_req_rep_uploader_post_proc()
{
  int status;
  NSDL2_PARENT(NULL, NULL, "Method called.");
  printf("Running nia_req_rep_uploader.\n");
  start_req_rep_uploader(0, global_settings->reader_run_mode);
  if(nia_req_rep_uploader_pid > 0)
  {
    waitpid(nia_req_rep_uploader_pid, &status, 0);
    printf("nia_req_rep_upload Exited.\n");
  }
  else {
    printf("Failed to start nia_req_rep_uploader.\n");
  }
}
#endif

int rm_log = 0;
static void post_processing_after_test_run(int test_result)
{
  NSDL2_PARENT(NULL, NULL, "Method called.");
  char buf[600];
  char* env_buf;
  RunProfTableEntry_Shr* rstart = runprof_table_shr_mem;

  //for stopping all the nethavoc scenario after the test stopped gracefully
  nethavoc_cleanup();

  if (test_result == SLA_RESULT_CANT_LOAD_ENOUGH) 
    fprint2f(console_fp, rfp, "The client cannot reach the criteria given by the user\n");

  if (test_result == SLA_RESULT_SERVER_OVERLOAD) 
    fprint2f(console_fp, rfp, "The Server reached Capacity before reaching the criteria given by the user\n");

  if (test_result == SLA_RESULT_INCONSISTENT) 
    fprint2f(console_fp, rfp, "Result Inconsistent. It is suggested to increase the Stablization Run time or the Goal Percent Error\n"); 

  if (test_result == SLA_RESULT_NA) 
    fprint2f(console_fp, rfp, "Result Inconsistent (NA). It is suggested to increase the Stablization Run time or the Goal Percent Error\n"); 

  if (test_result == SLA_RESULT_NF) 
    fprint2f(console_fp, rfp, "Result Inconsistent (NF). It is suggested to increase the Stablization Run time or the Goal Percent Error\n"); 

  call_user_test_exit_for_each_sess();

  //signal (SIGCHLD, SIG_IGN);
  /* In release 3.9.6:
   * In case of online, in post processing logging writer will wait for logging reader. 
   * Here if waitpid fails then we simply proceed with NS post processing task.
   *
   * In case of offline waitpid will return as logging reader is not yet spwaned 
   * 
   * In NetCloud in case of controller we are not swpaning logging writer, therefore writer_pid should be 0*/ 

  if(nia_file_aggregator_pid > 0)
  {
    //NSTL1_OUT(NULL, NULL, "Sending TEST_POST_PROC_SIG to NIA File Aggregator (pid = %d)", nia_file_aggregator_pid);
    NSTL1(NULL, NULL,"Sending TEST_POST_PROC_SIG to NIA File Aggregator (pid = %d)", nia_file_aggregator_pid);
    if(kill(nia_file_aggregator_pid, TEST_POST_PROC_SIG) < 0)
      NSTL1_OUT(NULL, NULL, "Error in sending TEST_POST_PROC_SIG to NIA File Aggregator (pid = %d)", nia_file_aggregator_pid);
  }
  
  if(db_aggregator_pid > 0)
  {
    int status;
    //NSTL1_OUT(NULL, NULL, "Sending TEST_POST_PROC_SIG to DB Aggregator (pid = %d)", db_aggregator_pid);
    NSTL1(NULL, NULL, "Sending TEST_POST_PROC_SIG to DB Aggregator (pid = %d)", db_aggregator_pid);
    if(kill(db_aggregator_pid, TEST_POST_PROC_SIG) == 0) 
    {
      //NSTL1_OUT(NULL, NULL, "Waiting for DB Aggregator to exit");
      NSTL1(NULL, NULL, "Waiting for DB Aggregator to exit");
      if(waitpid(db_aggregator_pid, &status, 0) == -1) 
      {
      	NSTL1_OUT(NULL, NULL, "Error in waiting for the DB Aggregator, Error: %s", nslb_strerror(errno));
      }
      //NSTL1_OUT(NULL, NULL, "DB Aggregator exited");
      NSTL1(NULL, NULL, "DB Aggregator exited");
    }
    else
      NSTL1_OUT(NULL, NULL, "Error in sending TEST_POST_PROC_SIG to DB Aggregator (pid = %d)", db_aggregator_pid);
  }


  if (writer_pid > 0) 
  {
    int status;
    if (kill(writer_pid, TEST_POST_PROC_SIG) == 0) 
    {
      //NSTL1_OUT(NULL, NULL, "Waiting for logging writer to exit");
      NSTL1(NULL, NULL, "Waiting for logging writer to exit");
      fflush(stdout);
      if (waitpid(writer_pid, &status, 0) == -1) 
      {
      	NSTL1_OUT(NULL, NULL, "Error in waiting for the logging writer");
      	perror("waidpid");
      }
      //NSTL1_OUT(NULL, NULL, "logging writer exited");
      NSTL1(NULL, NULL, "logging writer exited");
      fflush(stdout);
    }
    else
      NSTL1_OUT(NULL, NULL, "Error in sending TEST_POST_PROC_SIG to Logging Writer. Pid = %d", writer_pid);
  }

  //send TEST_POST_PROC_SIG to nia_file_aggregator.
  if(nia_file_aggregator_pid > 0)
  {
    int status;
    //NSTL1_OUT(NULL, NULL, "Waiting for nia_file_collector to exit");
    NSTL1(NULL, NULL, "Waiting for nia_file_collector to exit");
    fflush(stdout);
    if (waitpid(nia_file_aggregator_pid, &status, 0) == -1) 
    {
      NSTL1_OUT(NULL, NULL, "Error in waiting for the nia_file_collector");
      perror("waidpid");
    }
    NSTL1(NULL, NULL, "nia_file_collector exited");
    //NSTL1_OUT(NULL, NULL, "nia_file_collector exited");
    fflush(stdout);
  }


  /* Release 3.9.6:
   * In case of reading NDC stop message, now we will be reading STOP message after clearing writer_pid in offline*/
  if (loader_opcode != CLIENT_LOADER)
    read_ndc_stop_msg();

  /*if(global_settings->reader_run_mode != 0){
    if(reader_pid > 0)
      kill(reader_pid, SIGTERM);
  }*/

/*   if (global_settings->health_monitor_on) */
/*     kill(cpu_mon_pid, SIGTERM); */

  int rm_slog_and_dlog = 0;
   if (((get_max_log_level() == 0) || (get_max_log_dest() == 0)) && //logs need no file logging
      ((get_max_tracing_level() == 0) || (get_max_trace_dest() == 0)) && //trace need no file logging
      (get_max_report_level() < 2)) {//report need no file logging
    log_records = 0;
    global_settings->log_postprocessing = 0; //If nothing to log, force post-proceesing to 0
  }

  if (log_records) 
  {
    /*
    Commented code in release 3.8.2
    if (log_test_case(testidx, testcase_shr_mem, global_settings,
                      group_default_settings, sla_table_shr_mem, 
                      metric_table_shr_mem, thinkprof_table_shr_mem, 
                      start_time, get_ms_stamp()) == -1) {
      exit(-1);
    }*/

    stop_logging();

    //If log is generted as a side effect of post-processing but log not enabled, just remove

    if (((get_max_log_level() == 0) || (get_max_log_dest() == 0)) && //logs need no file logging
  	((get_max_tracing_level() == 0) || (get_max_trace_dest() == 0)) && //trace need no file logging
  	(get_max_report_level() >= 2)) //report needs file logging
    {
      rm_log = 1;
    }

    if (global_settings->remove_log_file_mode > 0)
    {
      rm_slog_and_dlog = 1;
    }

    /*    if ((user_pw = getpwnam("anil")) == NULL) 
  
          {
            NSTL1_OUT(NULL, NULL, "Error in getting uid");
            perror("getpwnam");
          }
          else
            if (setuid(user_pw->pw_uid) != 0)
            perror("setuid");*/

    //Report generation level is below detailed reports, set REPORT_MASK to 0.
    //This would disable report genertaion.
    //In case of tracing enable REPORT_MASK should be set
    MY_MALLOC (env_buf , 32, "env_buf ", -1);
    if ((get_max_report_level() < 2) && ((get_max_tracing_level() < 0) && (get_max_trace_dest() < 0)))
      sprintf(env_buf, "REPORT_MASK=0");
    else
      sprintf(env_buf, "REPORT_MASK=%d", global_settings->report_mask);

    putenv(env_buf);

    if (global_settings->wan_env)
      putenv("WAN_ENV=1");
    else
      putenv("WAN_ENV=0");

    //if (get_max_tracing_level() == 4)
    /* To create page_dump.txt for tracing levels greater than 0
     * following check was added */
    if ((get_max_tracing_level() > 0) && (get_max_trace_dest() > 0))
    {
      putenv("PAGE_SNAPSHOTS=1");
      /*Updating summary.top for page dump field. This is done because
       * post processing can take much time and in offline mode user will not be 
       * able to see the page dump till post processing is not over.
       * Now, user will be able to see the page_dump link even Netsorm is in postprocessing*/
      update_summary_top_field(4, "Available_New", 0);
    }
    else
      putenv("PAGE_SNAPSHOTS=0");

#ifdef RMI_MODE
    if (global_settings->log_postprocessing > 0) 
    {
      sprintf(buf, "nsu_post_proc_rmi %d %d > /dev/null 2>&1", testidx, rm_log);
      system(buf);
    }
#else
    //In netcloud mode , On generator call nia_req_rep_uploader if reader run mode 0.
    //if(loader_opcode == CLIENT_LOADER && global_settings->reader_run_mode == 0)
    //{
     // run_nia_req_rep_uploader_post_proc();
    //}
  
    //In release 3.9.7, in case of generator processing will be done on controller hence by pass the code for generator
    if (loader_opcode != CLIENT_LOADER)
    { 
      if (global_settings->log_postprocessing > 0) 
      {
        // Pass one more argument as Run time(HH:MM:SS) used for summary.top
        //              testidx, rm_log, 
        // -Z is used to pass dynamic url table size of URL ID normalization
        // -L option added to remove slog and dlog file.
        // NetCloud Changes: -g option added to pass generator index
        //                   -u option added to call nsu_import and nsu_cache_db_data shell. In case of DDR, controller needs to call these shell scripts rather shells being called from nsu_post_proc 
        // T option added for trace level for logging reader 
        // For normalization added fields G <total number of generator> and n <total number of NVMs> required by logging reader
        sprintf(buf, "nsu_post_proc -t %d -T %d -r %d -d %s -m %d -Z %d -L %d -g %d -S %d -n %d -G %d -p %d -B %d -F %d -X %d > /dev/null 2>&1", 
                      testidx, global_settings->nlr_trace_level, rm_log,  
                      (char *)get_time_in_hhmmss((int)(global_settings->test_duration/1000)), global_settings->reader_run_mode, global_settings->url_hash.dynamic_url_table_size, rm_slog_and_dlog, g_generator_idx, global_settings->log_shr_buffer_size, (loader_opcode == MASTER_LOADER)?global_settings->max_num_nvm_per_generator:global_settings->num_process, sgrp_used_genrator_entries, g_percentile_report, total_pdf_data_size, global_settings->nifa_trace_level, global_settings->dyn_tx_norm_table_size);
        NSDL2_PARENT(NULL, NULL, "Command to run = %s", buf);
        NSTL1_OUT(NULL, NULL, "Command to run = %s", buf);
        sleep(2); //TODO : Added for mode 1 Need to review
        system(buf);
      }
    }
		
#endif
#if 0
    if (global_settings->log_postprocessing == 2) 
    {
      sprintf(buf, "nsu_rm_trun -n %d > /dev/null 2>&1", testidx);
      system(buf);
    }
#endif
  }
  /*Fix done for Bug#8385, 
    In offline test where G_TRACING, G_REPORTING are disable but PERCENTILE_REPORT is enable, 
    then nsu_post_proc shell is not invoke therefore nifa_file_aggregator will not be spawn.
    Hence added check for invoking nia_file_aggregator in case of offline test with percentile report enable*/ 
  else if ((loader_opcode == MASTER_LOADER) && (g_percentile_report == 1) && (global_settings->reader_run_mode == 0))
  {
    sprintf(buf, "nia_file_aggregator -t %d -k -1 -m %d -p %d -B %d -b %d -l %d -L %d -D %d -g %d -n %d -G %d -X %d -R %d", 
                 testidx, global_settings->reader_run_mode, g_percentile_report, total_pdf_data_size, global_settings->log_shr_buffer_size, global_settings->nifa_trace_level, global_settings->nlr_trace_level, global_settings->url_hash.dynamic_url_table_size, g_generator_idx, global_settings->max_num_nvm_per_generator, sgrp_used_genrator_entries, global_settings->dyn_tx_norm_table_size, (global_settings->protocol_enabled & RBU_API_USED ? 1 : 0)); 
    NSDL2_PARENT(NULL, NULL, "Command to run = %s", buf);
    NSTL1_OUT(NULL, NULL, "Command to run = %s", buf);
    system(buf);
  }

  //Update end partition summary.top 
  char partition_duration_in_hhmmss[64];
  u_ns_ts_t now;

  now = get_ms_stamp();

  global_settings->partition_duration = now - partition_start_time;
  sprintf(partition_duration_in_hhmmss, "%s", (char *)get_time_in_hhmmss((int)(global_settings->partition_duration/1000)));

  update_summary_top_field(14, partition_duration_in_hhmmss, 1);
  int num_field = 0;     
  char *fields[10];
  char temp_file[2014];
  char workspace[512];
  char work_profile[512];
  strcpy(temp_file, g_conf_file);
  NSDL2_PARENT(NULL, NULL, "Method called. temp_file=%s", temp_file);
  num_field=  get_tokens(temp_file, fields, "/", 12);
  NSDL2_PARENT(NULL, NULL, "num_field = %d", num_field);
   if(num_field == 11)
  {
     strcpy(workspace, fields[4]);
     strcpy(work_profile, fields[5]);
  }
  //Add by Atul to call nsu_post_proc_always which will update summar.top -- 07/05/2007

  // Pass one more argument as Run time(HH:MM:SS) used for summary.top
  // tr069_data_dir
  sprintf(buf, "nohup nsu_post_proc_always -t %d -r %d -d %s -w %s/%s > /dev/null 2>&1 &", 
               testidx, rm_log,
               (char *)get_time_in_hhmmss((int)(global_settings->test_duration/1000)), workspace, work_profile);
  system(buf);
  sprintf(buf, "cd %s;rm -f cookie.txt cookie_parse.txt "
               "cookie_parse_u.txt hcookies.c hcookies_parse.c "
               "cookie_hash.c cookie_hash_parse.c cookie_hash.so "
               "cookie_hash_parse.so", g_ns_tmpdir);
  system(buf);
  sprintf(buf, "cd %s; rm -f dynvar.txt hdynvars.c dynvar_hash.c dynvar_hash.so", g_ns_tmpdir);
  system(buf);
#ifdef RMI_MODE
  //sprintf(buf, "cd tmp; rm -f bytevar.txt hbytevars.c bytevar_hash.c bytevar_hash.so");
  sprintf(buf, "cd %s; rm -f bytevar.txt hbytevars.c bytevar_hash.c bytevar_hash.so", g_ns_tmpdir);
  system(buf);
#endif

  /*Commented on 1/sep/2010. In Index var its core dumping.
   * When we are using only index var in parameterization that time 
   * it was core dumping. We also need to  understand the funcinality
   * of this function*/
  //writeback_static_values();
  //

  //tr069_processing_after_test_run(global_settings->tr069_data_dir, idx_table_ptr[(tr069_total_idx_entries - rstart->quantity)], (tr069_total_idx_entries - rstart->quantity),1);

  if (global_settings->protocol_enabled & TR069_PROTOCOL_ENABLED){
    tr069_processing_after_test_run(global_settings->tr069_data_dir, tr069_total_data_count - rstart->quantity);
  }
  
  //Run this if rbu post processing is enabled
  if((global_settings->protocol_enabled & RBU_API_USED) && (loader_opcode != MASTER_LOADER))
  {
    //This function will handle all rbu post processing
    if (ns_rbu_post_proc() == -1 )
    {
      NSDL1_PARENT(NULL, NULL, "Error occured in function ns_rbu_post_proc"); 
      NSTL1(NULL, NULL, "Error occured in function ns_rbu_post_proc"); 
    }
  }
  //Run this if rte post processing is enabled
  if((global_settings->protocol_enabled & RTE_PROTOCOL_ENABLED) && (loader_opcode != MASTER_LOADER))
  {
    //This function will handle all rte post processing
    if (ns_rte_post_proc() == -1 )
      NSDL1_PARENT(NULL, NULL, "Error occured in function ns_rte_post_proc"); 
  }

  fflush(stdout);

  //Resolve bugs 20005, 26444, 22515
  char filename[512];
  sprintf(filename, "%s/logs/TR%d/.parent_nvm_con.status", g_ns_wdir, testidx);

  NSDL1_PARENT(NULL, NULL, "Removing file %s", filename);
  unlink(filename);  

  // In generator and in offline  mode, start NIA Req/Rep Uploader to ship
  if((loader_opcode == CLIENT_LOADER) && (global_settings->reader_run_mode == 0))
  {
    NSDL1_PARENT(NULL, NULL, "Starting req_rep_uploader is offline mode, loader opcode = %d", loader_opcode);
    //first arg is recovery mode, second is reader run mode
    start_req_rep_uploader(0, 0);
  }

  //sending test over signal to nia_req_rep_uploader (No need to send in offline mode
  NSDL1_PARENT(NULL, NULL, "nia_req_rep_uploader_pid = %d, reader_run_mode = %d", nia_req_rep_uploader_pid, global_settings->reader_run_mode);
  if ((nia_req_rep_uploader_pid > 0) && (global_settings->reader_run_mode))
  {
    //NSTL1_OUT(NULL, NULL, "Sending TEST_POST_PROC_SIG to NIA Req/Rep Uploaded. Pid = %d", nia_req_rep_uploader_pid);
    NSTL1(NULL, NULL, "Sending TEST_POST_PROC_SIG to NIA Req/Rep Uploaded. Pid = %d", nia_req_rep_uploader_pid);
    if (kill(nia_req_rep_uploader_pid, TEST_POST_PROC_SIG) < 0)
      NSTL1_OUT(NULL, NULL, "Error in sending TEST_POST_PROC_SIG to NIA Req/Rep Uploaded. Pid = %d", nia_req_rep_uploader_pid);
  }
  //send TEST_POST_PROC_SIG to nia_file_aggregator.
  if(nia_req_rep_uploader_pid > 0)
  {
    int status;
    NSTL1(NULL, NULL, "Waiting for NIA Req/Rep Uploader to exit");
    //NSTL1_OUT(NULL, NULL, "Waiting for NIA Req/Rep Uploader to exit");
    fflush(stdout);
    if (waitpid(nia_req_rep_uploader_pid, &status, 0) == -1) 
    {
      NSTL1_OUT(NULL, NULL, "Error in waiting for the NIA Req/Rep Uploader");
      perror("waidpid");
    }
    NSTL1(NULL, NULL, "NIA Req/Rep Uploader exited");
    //NSTL1_OUT(NULL, NULL, "NIA Req/Rep Uploader exited");
    fflush(stdout);
  }
 
  /* In release 3.9.6, kill command will be send at the end to event logger pid, 
   * as someone might wants to log an event in event.log file
   * For event Logger*/
  if(nsa_log_mgr_pid > 0) kill(nsa_log_mgr_pid, TEST_POST_PROC_SIG);
}

//This function is for writting data which is required to
//write before parent ends.
void parent_save_data_before_end ()
{
  /* In case of Master MODE, per_proc_vgroup_table is not initialized
   * as shared memory to above pointer is assigned in divide_values()
   * which isnt called in case of controller, hence causing core dump
   * 
   * In case of controller we need to create data files from .last files created by NVM */
   if (loader_opcode != MASTER_LOADER) {
     close_last_file_fd ();
     remove_ctrl_and_last_files();
     //if (loader_opcode != CLIENT_LOADER) 
       divide_data_files();
   } else if (loader_opcode == MASTER_LOADER) { //In case of controller
     ni_create_data_files_frm_last_files();
   }
   //save_last_data_file();
}

/* Save scenario name (without .conf)
 * Input:
 *   conf_file: scenario conf file name (e.g. my_scen.conf)
 *
 */

static void set_scenario_name(char *conf_file)
{
  char *conf; // Pointer to .conf part of file
  int conf_len = strlen(conf_file);

  NSDL2_PARENT(NULL, NULL, " Method called. conf_file = %s", conf_file);

  if ((conf = rindex(conf_file, '.')) != NULL){
    conf_len = conf - conf_file;
  }
  strncpy(g_scenario_name, conf_file, conf_len);
  g_scenario_name[conf_len] = '\0'; 
  NSDL2_PARENT(NULL, NULL, " Method called. g_scenario_name = %s", g_scenario_name);
}

//Archana - Added for project subproject feature (3.2.3) 
static void extract_proj_subproj_from_scenario()
{
  int num_field = 0;
  char *fields[10];
  char temp_file[2014];
  /* bug id: 101320: scenaio path format would be
    <workspace>/<user_name>/<profile_name>/cavisson/<proj>/<sub_proj>/scenarios/<scenario_name>.conf
  */
   
  strcpy(temp_file, g_conf_file);
  //printf("temp=%s",temp_file); 
  NSDL2_PARENT(NULL, NULL, "Method called. temp_file=%s", temp_file);
  num_field=  get_tokens(temp_file, fields, "/", 12);
  NSDL2_PARENT(NULL, NULL, "num_field = %d", num_field);
  //printf("num_field=%d",num_field);
  if(num_field == 11)
  {
   //ex -: /home/cavisson/NetOcean/workspace/admin/default/cavisson/default/default/scenarios/cache.conf
   //<workspace>/<user_name>/<profile_name>/cavisson/<proj>/<sub_proj>/scenarios/<scenario_name>.conf 
   // this is the case where file is passed as scenarios/<project>/<subprojecgt>/<scenario filename with .conf>
    strcpy(g_project_name, fields[7]);
    strcpy(g_subproject_name, fields[8]);
    set_scenario_name(fields[10]);
  }
  else
  {
    NS_EXIT(-1, CAV_ERR_1011171);
    //NS_EXIT (-1, "'Project/Subproject/<ScenarioName>' is not given in proper format.");
  }
}

static void update_global_dat_for_generator()
{
  char file[MAX_FILE_NAME];
  FILE *fp;
  NSDL1_PARENT(NULL, NULL, "Method called");
  OPEN_GLOBAL_DATA_FILE(file, fp);
    fprintf(fp, "GENERATOR_ID %d\n", g_generator_idx);
    fprintf(fp, "GENERATOR_NAME %s\n", global_settings->event_generating_host);
    fprintf(fp, "CONTROLLER_IP %s\n", master_ip);
    fprintf(fp, "CONTROLLER_ENV %s\n", g_controller_wdir);
    fprintf(fp, "CONTROLLER_TESTRUN_NUMBER %s\n", g_controller_testrun);
  CLOSE_GLOBAL_DATA_FILE(fp);
}

int parse_keyword_before_init(char *file_name, char *err_msg)
{
  FILE* conf_file;
  char keyword[MAX_DATA_LINE_LENGTH];
  char text[BIG_DATA_LINE_LENGTH];
  char buf[BIG_DATA_LINE_LENGTH];
  int num;

  NSDL2_PARENT(NULL, NULL, "Method called, parse_keyword_before_init(), file_name = %s", file_name);
  conf_file = fopen(file_name, "r");
  if (!conf_file) {
    NS_EXIT(-1, CAV_ERR_1000006, file_name, errno, nslb_strerror(errno));
  }
  while (nslb_fgets(buf, BIG_DATA_LINE_LENGTH, conf_file, 0) != NULL) {
    if ((num = sscanf(buf, "%s %s", keyword, text)) < 2) {
      continue;
    }
    if (strcasecmp(keyword, "PARTITION_SETTINGS") == 0) {
      NSDL2_PARENT(NULL, NULL, "PARTITION_SETTINGS keyword found");
      kw_set_partition_settings(buf, 0, err_msg);
    }
    else if (strcasecmp(keyword, "MULTIDISK_PATH") == 0) {	//parsing this keyword here because we want disk path at early stage of test
      NSDL2_PARENT(NULL, NULL, "MULTIDISK_PATH keyword found");
      kw_set_multidisk_path(buf);
    }
    else if (strcasecmp(keyword, "TIME_STAMP") == 0) {  //parsing this keyword here because we want relative time should be correct
      NSDL2_PARENT(NULL, NULL, "TIME_STAMP keyword found");
      kw_set_time_stamp_mode(text);
      init_ms_stamp();
      global_settings->test_start_time = get_ms_stamp();
    }
    else if(!strcasecmp(keyword, "ENABLE_TMPFS")) {
      NSDL2_PARENT(NULL, NULL, "ENABLE_TMPFS keyword found");
      kw_enable_tmpfs(buf);
    }
    #ifdef NS_DEBUG_ON
    else if (strncasecmp(keyword, "LIB_DEBUG", strlen("LIB_DEBUG")) == 0) {
     set_nslb_debug(buf);
    } else if (strcasecmp(keyword, "LIB_MODULEMASK") == 0) {
      if (set_nslb_modulemask(buf, err_msg) != 0)
        NS_EXIT(-1, "%s", err_msg);

        char log_file[1024];
        char error_log_file[1024];
        sprintf(log_file, "%s/logs/TR%d/debug.log", g_ns_wdir, testidx);
        sprintf(error_log_file, "%s/logs/%s/error.log", g_ns_wdir, global_settings->tr_or_partition);
        nslb_util_set_log_filename(log_file, error_log_file);
    }
    #endif
  }
  fclose(conf_file);
  return 0; 
}

static void ns_parent_logger_listen_ports(char *err) {
  NSTL1_OUT(NULL, NULL, "ERROR : %s", err);
  NSTL1_OUT(NULL, NULL, "USAGES: NS_PARENT_LOGGER_LISTEN_PORTS  <MINVAL-MAXVAL>");
  NSTL1_OUT(NULL, NULL, "This Keyword used for defining port ranges on which parent and event logger will listen");
  NSTL1_OUT(NULL, NULL, "Ranges must be between 1025 to 65000 ");
  NS_EXIT(-1, "%s\nUsage: NS_PARENT_LOGGER_LISTEN_PORTS  <Min val - Max val>", err);
}

//keyword NS_PARENT_LOGGER_LISTEN_PORTS definition here
void kw_ns_parent_logger_listen_ports(char *buffer) {
  char keyword[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH];
  char port_range_min[6], port_range_max[6];
  int num;
 
  if(loader_opcode != MASTER_LOADER)
    return;

  num = sscanf(buffer, "%s %s %s %s", keyword, port_range_min, port_range_max, tmp);

  NSDL2_PARSING(NULL, NULL, "Method Called, buf = %s, num = %d, Key = [%s], port_range_min = %s, port_range_max = %s", 
                            buffer, num, keyword, port_range_min, port_range_max);
  
  if(num != 3) 
  { 
    ns_parent_logger_listen_ports("Error: Too few arguments for NS_PARENT_LOGGER_LISTEN_PORTS");
  }

  if((ns_is_numeric(port_range_min) == 1) && (ns_is_numeric(port_range_max) == 1))
  {
    global_settings->ns_use_port_min = atoi(port_range_min);
    global_settings->ns_use_port_max = atoi(port_range_max);
  }
  else 
  {
    ns_parent_logger_listen_ports("Range of port is not numeric.");
  }
  
  if(((global_settings->ns_use_port_min) < 1025) || ((global_settings->ns_use_port_max) > 65000)) 
  {
    ns_parent_logger_listen_ports("INVALID Port Number.");
  }

  //TODO: Add control, data and event logger port number, data = ns_use_port_min + 1
  if((global_settings->ns_use_port_min) >= (global_settings->ns_use_port_max))
    ns_parent_logger_listen_ports("Max port value cannot be smaller or equal to min port value.");
   
  if((global_settings->ns_use_port_max - global_settings->ns_use_port_min) < 2)
    ns_parent_logger_listen_ports("Min-Max listen port range must be greater then or equal to 3.");
  
  NSDL3_PARSING(NULL, NULL, "Found parent(start from min port) and event logger(start from max port) port range from [%d] to [%d]", global_settings->ns_use_port_min, global_settings->ns_use_port_max);
}

/*
Purpose: This function is used when NS_PARENT_LOGGER_LISTEN_PORTS keyword is enabled.
         It assign listen port for parent/controller and event logger.
Variables: Here, flag ==> 0 means event logger port
			  1 means parent port
               ns_max ==> start port for event logger. If given port is not free then we can decreament one by one until ns_min - 1
               ns_min ==> start port for parent/controller. If given port is not free then we can increment one by one until ns_max + 1
*/
int get_ns_port_defined(int ns_max, int ns_min, int flag, int *new_listen_fd, int listenPurpose)
//int get_ns_port_defined(int ns_max, int ns_min, int flag)
{
  int ret_port, port;

  NSDL1_PARSING(NULL, NULL, "Method called, ns_max = [%d] , ns_min = [%d] , flag == [%d] ", ns_max, ns_min, flag);

  if(flag)
    port = ns_min;
  else
    port = ns_max;

  while(1)
  {
    if(flag == 1) /* flag is set to assign port for parent */
    {  
      NSDL3_PARSING(NULL, NULL, "Going to check parent/controller port %d for listening", port);
      if (listenPurpose == PARENT_LISTEN_PORT)
      {
         NSDL3_PARSING(NULL, NULL, "Going to assign parent port %d for listening", port);
         ret_port = init_parent_listner_socket_new(new_listen_fd, port);
      }else {
         port += 1;  
         NSDL3_PARSING(NULL, NULL, "Going to data connection port %d for listening", port);
         ret_port = init_parent_listner_socket_new(new_listen_fd, port);
      }
      if(ret_port == 0) //Could not assign the port go to next port
      { 
        NSTL1(NULL, NULL, "[Controller] Port %d is already assigned, will check for next port", port);
        write_log_file(NS_SCENARIO_PARSING, "Port %d is already assigned, will check for next port", port);
        port++;     /* port incremented by 1 from ns_min to get next port */
        if(port == (ns_max + 1))
        {
          NS_EXIT(1,  "Port range exhaust, no port is available on which parent can listen. Terminating the test.");
        }
        continue;
      }
      else {
        NSTL1(NULL, NULL, "NS_PARENT_LOGGER_LISTEN_PORTS: Listen port for parent/controller is [%d]", port);
        write_log_file(NS_SCENARIO_PARSING, "[Controller] Listening on port %d", port);
        return port;
      }
    }   
    else {     /* if flag not set then assign port for event logger */
      NSDL2_PARSING(NULL, NULL, "Going to check event logger port %d for listening", port);
      //ret_port = init_parent_listner_socket_new(&el_lfd, port);
      ret_port = init_parent_listner_socket_new_v2(new_listen_fd, port, 1);
      if(ret_port == 0) 
      {
        NSTL1(NULL, NULL, "Port %d is already assigned, will check for next port", port);
        write_log_file(NS_SCENARIO_PARSING, "[Event Logger] Port %d is already assigned, will check for next port", port);
        port--;      /*port decremented to get next port  */
        if(port == (ns_min - 1))
        {
          NSTL1(NULL, NULL, "Port range exhaust, no port is available on which event logger can listen. Terminating the test.");
          NS_EXIT(1, "Port range exhaust, no port is available on which event logger can listen. Terminating the test.");
        }
        continue;
      }   
      else {
        NSTL1(NULL, NULL, "NS_PARENT_LOGGER_LISTEN_PORTS: Listen port for event logger is [%d].", port);
        write_log_file(NS_SCENARIO_PARSING, "[Event Logger] Listening on port %d", port);
        return port;
      }
    }
  }
  return 0;
}

void get_current_time(char *buff)
{
  long tloc;
  struct tm *lt, tm_struct;
  (void)time(&tloc);
  lt = nslb_localtime(&tloc, &tm_struct, 0);
  strftime(buff, 128, "%Y_%m_%d_%H_%M_%S", lt);
}

void setup_for_ndc()
{
  time_t mseconds_before, mseconds_after, mseconds_elapsed;
  char mseconds_buff[100];

  ndc_mccptr.fd = -1;
  ndc_data_mccptr.fd = -1;
  // we need to start ndc only when loader is master or standalone 
  if(loader_opcode != CLIENT_LOADER) 
  {
    mseconds_before = get_ms_stamp();
    if((global_settings->net_diagnostics_mode) && (global_settings->net_diagnostics_port != 0))
    {
      init_stage(NS_DIAGNOSTICS_SETUP);
      write_log_file(NS_DIAGNOSTICS_SETUP, "Making control connection Netdiagnostics server %s", global_settings->net_diagnostics_server);
      start_nd_ndc();               //Control connection only in case of ND enabled
      
    }
  
    if(global_settings->net_diagnostics_port != 0)
    {
      write_log_file(NS_DIAGNOSTICS_SETUP, "Making data connection to Netdiagnostics server %s port %d",
                                            global_settings->net_diagnostics_server, global_settings->net_diagnostics_port);
      start_nd_ndc_data_conn();    //Data connection: We will be passing seperate message to NDC for creating data connection.
    }
    else
    {
      NSTL1(NULL, NULL, "NDC port is 0. Donot make connection to NDC");
    }

    mseconds_after = get_ms_stamp();
    mseconds_elapsed = mseconds_after - mseconds_before;
    convert_to_hh_mm_ss(mseconds_elapsed, mseconds_buff);
    NSTL1(NULL, NULL, "Time taken by start_nd_ndc() %s",mseconds_buff);
    if((global_settings->net_diagnostics_mode) && (global_settings->net_diagnostics_port != 0))
      end_stage(NS_DIAGNOSTICS_SETUP, TIS_FINISHED, NULL);
  }
}

void setup_for_monitors()
{
  time_t mseconds_before, mseconds_after, mseconds_elapsed;
  char mseconds_buff[100];
  
  //if pid received from ndc for any one process then do not run server signature else run server signature on all the servers
  pid_received_from_ndc = topolib_chk_if_any_pid_received_from_ndc(topo_idx);

  /*ENABLE_NS_MONITOR <mode> <iperf monitoring>
    if iperf monitoring value 1 then it parse function and start iperf server on Controller
    if g_iperf_monitoring_port < 0 then port is set to 0.
   */
  if(g_enable_iperf_monitoring)   {
    start_iperf_server_for_netcloud_test(g_ns_wdir, &g_iperf_monitoring_port);
    if(g_iperf_monitoring_port < 0)
      g_iperf_monitoring_port = 0;
  }

  //TODO: CHECK CONTROL CONN ON SERVER WHERE JAVA PROCESS SERVER SIG SET UP ???NEEDED OR NOT???

  // Sending message "test_run_starts:MON_TEST_RUN=1234;ProgressMsec=10000" from NS to CavMonAgent as test starts 

  //Skipping cntrl connection with cmon if outbound connection is enabled

  //Now starting pre test check monitors before dynamic vector monitor because now we need server signature 'nsi_get_java_instance' output during dynamic vector list processing of coherence monitors & for this we are running server signature on each server of topology, 
  //if ((loader_opcode == MASTER_LOADER) || (loader_opcode == STAND_ALONE)) 
  
  // Manish:making ns_auto.mprof. Moved this code from ns_parse_scen.c for the support of TcpStateCountV3 montior for NetCloud (mode=2).
  if(g_auto_ns_mon_flag || g_auto_no_mon_flag || g_cmon_agent_flag || g_auto_server_sig_flag)
    make_ns_auto_mprof();

  if(! is_outbound_connection_enabled)
  {
    write_log_file(NS_MONITOR_SETUP, "Making connection to cavmon servers");
    mseconds_before = get_ms_stamp();
    send_testrun_starts_msg_to_all_cavmonserver_used();
    mseconds_after = get_ms_stamp();
    mseconds_elapsed = mseconds_after - mseconds_before;
    convert_to_hh_mm_ss(mseconds_elapsed, mseconds_buff);
    //NSTL1_OUT(NULL, NULL, "Time taken to make Control connection %s", mseconds_buff);
    NSTL1(NULL, NULL, "Time taken to make Control connection %s", mseconds_buff);
  }

  if(loader_opcode != CLIENT_LOADER)
  {
    write_log_file(NS_MONITOR_SETUP, "Applying check monitors");
    mseconds_before = get_ms_stamp();
    start_check_monitor(CHECK_MONITOR_EVENT_BEFORE_TEST_IS_STARTED, "");
    mseconds_after = get_ms_stamp();
    mseconds_elapsed = mseconds_after - mseconds_before;
    convert_to_hh_mm_ss(mseconds_elapsed, mseconds_buff);
    //NSTL1_OUT(NULL, NULL, "Time taken in start_check_monitor(): %s",mseconds_buff); 
    NSTL1(NULL, NULL, "Time taken in start_check_monitor(): %s",mseconds_buff);
  }

  if(((java_process_server_sig_enabled == 1) && (pid_received_from_ndc == 0)))
    process_java_server_sig_data_and_save_in_instance_struct();

  //reset so that server signature can run later
  pid_received_from_ndc = 0;

  if(java_process_server_sig_enabled == 1) 
  {
    //This is to setup server signature on all the servers of topology, to get java instances using shell 'nsi_get_java_instances'
    write_log_file(NS_MONITOR_SETUP, "Setup server signatures");
    setup_server_sig_get_java_process();
  }

  set_coherence_cache_format_type();
  if(is_mssql_monitor_applied)
  {
    int i;
    for(i=0; i < total_monitor_list_entries; i++)
    {
   //   if(!strncmp(monitor_list_ptr[i].gdf_name, "NA_genericDB_", 13))
     //   check_for_csv_file(monitor_list_ptr[i].cm_info_mon_conn_ptr);
    }
    create_mssql_stats_hidden_file();
  }

  if(global_settings->sql_report_mon == 1)
  {
    //create csv files in case of oracle stat monitor
    //need to call this function after Tr%d/reports/csv dir is created and
    //before starting db upload
    create_oracle_sql_stats_hidden_file();
    create_oracle_sql_stats_csv();
  }

  create_alerts_stats_csv();

   //Initialize norm table for custom monitors
  write_log_file(NS_MONITOR_SETUP, "Normalizing monitors");
  initialize_dyn_cm_rt_hash_key();

  // GDF must be processed after all files and keywords are parsed as it uses
  // parsed data to generate GDF.
  // Also it mused be called before shared memory is copied, as some method
  // in shared memory copy needs GDF data (e.g. SLA)

  if(global_settings->enable_health_monitor)
  {
    write_log_file(NS_MONITOR_SETUP, "Applying cavisson health monitors");
    apply_component_process_monitor();
    //apply server count only when topology is present
    if(strcmp(global_settings->hierarchical_view_topology_name,"NA") != 0)
      apply_tier_server_count_monitor();
  }

  write_log_file(NS_MONITOR_SETUP, "Sorting monitor entries");
  qsort(monitor_list_ptr, total_monitor_list_entries, sizeof(Monitor_list), check_if_cm_vector);
  set_no_of_monitors();

  write_log_file(NS_MONITOR_SETUP, "Processing graph defination file");
  process_all_gdf(0);

  //NC: In order to fix bug#6688, generator will send testrun.gdf on receiving first schedule phase from controller
  if (loader_opcode == CLIENT_LOADER) {
    write_log_file(NS_MONITOR_SETUP, "Sending %s graph defination file to master", g_test_or_session);
    send_testrun_gdf_to_controller();
    NSTL1(NULL, NULL, "Send testrun.gdf to controller");
  }
 
  /*Moved here as total_pdf_data_size is calculated in process_all_gdf*/
  if (g_percentile_report == 1 && total_pdf_data_size > PDF_DATA_HEADER_SIZE)
    create_testrun_pdf();
}

void parent_epoll_create(int num_connections)
{
  int num_connections_estimate;

  NSDL1_PARENT(NULL, NULL, "num_connections = %d", num_connections);

  UPDATE_GLOB_DATA_CONTROL_VAR(g_data_control_var.num_connected = num_connections)

  num_connections_estimate = num_connections + 5 + total_monitor_list_entries +
    total_check_monitors_entries + total_pre_test_check_entries; 
  if ((g_msg_com_epfd = epoll_create(num_connections_estimate)) == -1)
  {
    NS_EXIT(-1, CAV_ERR_1000039, errno, nslb_strerror(errno));
  }
  //fcntl(g_msg_com_epfd, F_SETFD, FD_CLOEXEC);
  fcntl(listen_fd, F_SETFD, FD_CLOEXEC);
  NSDL1_PARENT(NULL, NULL, "Method exit, parent epfd = %d", g_msg_com_epfd);
}

void dh_epoll_create()
{
 /*Since Linux 2.6.8, the size argument on epoll_create () is ignored, but must be greater than zero;*/
  if ((g_dh_msg_com_epfd = epoll_create(1)) == -1)
  {
    NS_EXIT(-1, CAV_ERR_1000039, errno, nslb_strerror(errno));
  }
  NSDL1_PARENT(NULL, NULL, "Method exit, data handler epfd = %d", g_dh_msg_com_epfd);
  //fcntl(g_dh_msg_com_epfd, F_SETFD, FD_CLOEXEC);  
  fcntl(data_handler_listen_fd, F_SETFD, FD_CLOEXEC);  
}

void monitor_connection()
{
  time_t mseconds_before, mseconds_after, mseconds_elapsed;
  char mseconds_buff[100];

  if (loader_opcode != CLIENT_LOADER) 
  {
    if(is_outbound_connection_enabled)
      make_mon_msg_and_send_to_NDC(0, 0, 0);
    else
    {
      if(total_monitor_list_entries > 0)
      {
        //NSTL1_OUT(NULL, NULL, "Info: Setting up data monitors ...");
        NSTL1(NULL, NULL, "Info: Setting up data monitors ...");
        mseconds_before = get_ms_stamp();
        NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_INFO,
                                 __FILE__, (char*)__FUNCTION__,
                                "Setting up data monitors ...");
 
        write_log_file(NS_MONITOR_SETUP, "Making connection for the monitors, setting up data monitors");
        custom_monitor_setup(global_settings->progress_secs);
        //sleeping so that createserver data can be shown in first sample(10 secs) : Assumption
        sleep(2);
 
        mseconds_after = get_ms_stamp();
        mseconds_elapsed = mseconds_after - mseconds_before;
        convert_to_hh_mm_ss(mseconds_elapsed, mseconds_buff);
        //NSTL1_OUT(NULL, NULL, "Info: Data monitor setup done. Time taken is : %s", mseconds_buff);
        NSTL1(NULL, NULL, "Info: Data monitor setup done. Time taken is : %s", mseconds_buff);
  
        NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_INFO,
                                 __FILE__, (char*)__FUNCTION__,
                                 "Data monitor setup done.");
      }
    }
  } 
}

/*If any stage is not yet finished then mark it error before going to post processing*/
void end_test_init_running_stage(char *controller_ip)
{
  char cmd_buf[256];

  NSDL2_PARENT(NULL,NULL, "Method called, testidx = %d, IP = %d", testidx, controller_ip);
  if(testInitStageTable[NS_COPY_SCRIPTS].stageStatus == TIS_RUNNING)
    end_stage(NS_COPY_SCRIPTS, TIS_ERROR, "Failed to copy scripts in test run directory, Before post processing.\n");

  if(loader_opcode == MASTER_LOADER)
  {
    sprintf(cmd_buf, "%s/bin/nsu_get_running_stage_info %d %s", g_ns_wdir, testidx, controller_ip);
    system(cmd_buf);
  }
}

void send_test_run_id_to_master()
{
  char filepath[MAX_FILE_NAME + 1], dest_path[MAX_FILE_NAME + 1];
  int ret;
  FILE *gen_tr_fp;
  ServerCptr server_ptr;

  snprintf(g_controller_wdir, 512, "%s", getenv("NS_CONTROLLER_WDIR"));
  snprintf(g_controller_testrun, 32, "%s", getenv("NS_CONTROLLER_TEST_RUN"));
  snprintf(filepath, MAX_FILE_NAME, "%s/logs/TR%d/ready_reports/TestRunNumber", g_ns_wdir, testidx);
  snprintf(dest_path, MAX_FILE_NAME, "%s/logs/TR%s/ready_reports/TestInitStatus/Generators/%s/", g_controller_wdir,
                                g_controller_testrun, global_settings->event_generating_host);
  gen_tr_fp = fopen(filepath, "w");
  if(!gen_tr_fp)
  {
    fprintf(stderr, "Generator %s is not saved on generator", g_test_or_session);
  }
  else
  {
    fprintf(gen_tr_fp, "%d", testidx);
    fclose(gen_tr_fp);
    memset(&server_ptr, 0, sizeof(ServerCptr));
    MY_MALLOC_AND_MEMSET(server_ptr.server_index_ptr, sizeof(ServerInfo), "server admin ptr", -1);
    MY_MALLOC(server_ptr.server_index_ptr->server_ip, 32+1, "controller ip", -1);
    snprintf(server_ptr.server_index_ptr->server_ip, 32, "%s", master_ip);
    ret = nslb_ftp_file(&server_ptr, filepath, dest_path, 0);
    if(ret)
      fprintf(stderr, CAV_ERR_1014015, g_test_or_session);
     
    nslb_clean_cmon_var(&server_ptr);
  }
}

int netstorm_parent( int argc, char** argv )
{
  int test_result = SLA_RESULT_OK;
  static int first_time = 1; // to specify that it the first test sample
  argv0 = argv[0]; // argv0 is defined in netstorm.h. Need to remove this
  int org_percentile_report = 0;
  char net_cloud_data_path[1024];
  char buf[1024];
  char controller_path[1024];
  char rm_controller_dir[1024];
  char cur_time[128];
  char netcloud_path[1024];
  char new_netcloud_file[1024];
  char controller_ip[64] = {0};
  char err_msg[4096 + 1] = {0};
  char gen_file[512] = {0};
  //char output_file[512];
  char validation_status1, validation_status2;

  int ret;
  static int default_gen_file = 0;
  struct stat s;
  
 
  //Creating test initialization stages
  create_test_init_stages();

  group_default_settings = (GroupSettings *)malloc(sizeof(GroupSettings));
  global_settings = (Global_data *) malloc(sizeof(Global_data));
  memset(global_settings, 0, sizeof(Global_data));
  memset(group_default_settings, 0, sizeof(GroupSettings));
  //memset(g_data_control_var, 0, sizeof(Data_Control_con));
  //Till now debug and trace log is not ready.
  // Initializing g_tls variable
  ns_tls_init(VUSER_THREAD_BUFFER_SIZE);
  SET_CALLER_TYPE(IS_PARENT);
 
  //this is to print large value in ','
  setlocale(LC_ALL, ""); //Arun
  //Setting GMT Time Diff used in Caching     #Shilpa 09Dec10
  set_timezone_diff_with_gmt (); 
  //Setting Current Time in seconds used in Caching    #Shilpa 28Dec10
  set_ns_start_time_in_secs(); 
  //saving current time to show in Test Initialization status

  //initialize_reused_deleted_vector();
  validation_status1 = parent_init_before_parsing_args(argv0, err_msg);
  validation_status2 = parse_args(argc, argv, err_msg);
  //sprintf(output_file, "%s/.tmp/%s/TestRunOutput.log", g_ns_wdir, g_test_inst_file);
  //freopen(output_file, "a", stdout);
  //dup2(fileno(stdout), fileno(stderr));
  //fprintf(stderr, "output file is - %s\n", output_file);

   
  /*If run_mode_option is given then dont call
   * this function, we dont need to genertate test run for 
   * compiling script*/
  //NSDL2_PARENT(NULL, NULL, "run_mode_option = %d, ni_make_tar_option = %d", run_mode_option, ni_make_tar_option);
  //Reverse condition, 
  //In case of master loader, if we creating tar and exiting then no need to generate test run
  if(!((run_mode_option & RUN_MODE_OPTION_COMPILE) || 
       (loader_opcode == MASTER_LOADER && (ni_make_tar_option & CREATE_TAR_AND_EXIT)))) 
  {
    //NSDL2_PARENT(NULL, NULL, "Need to generate test run and set debug level");
    testidx = get_testid();
    get_testid_and_set_debug_option(); 

    open_test_init_files(); //zero size open stage files and delete CM last files
    extract_proj_subproj_from_scenario(); //to get proj subproj
    write_init_stage_files(NS_INITIALIZATION);
    write_log_file(NS_INITIALIZATION, "%s generated", g_test_or_session);

    /* Moving code is here as parsing failure should be seen on offline testrun*/
    set_logfile_names();                  //to get scenario name without .conf
    sprintf(buf, "logs/TR%d/summary.top", testidx);
    if(stat(buf, &s) != 0)
    {
      sprintf(buf, "%d", testidx);
      create_summary_top(buf);            //create dummy summary report, update  on next
    }

    /* send gen test id to master to show init screen*/
    if(loader_opcode == CLIENT_LOADER)
      send_test_run_id_to_master();

    NSDL2_PARENT(NULL, NULL, "validation_status1 = %d, validation_status2 = %d, err_msg = %s",
                              validation_status1, validation_status2, err_msg);
    if(validation_status1 == -1 || validation_status2 == -1 || testidx < 0) {
      //end_stage(NS_INITIALIZATION, TIS_ERROR, err_msg);
      //exit(-1);  //do not use NS_EXIT as g_partition_idx is 0
      NS_EXIT(-1, "%s", err_msg); 
    }
  }
  else //
  {
    if(validation_status1 == -1 || validation_status2 == -1) {
      //fprintf(stderr, "%s\n", err_msg);
      //exit(-1);
      NS_EXIT(-1, "%s", err_msg);
    }
  }
  NSDL2_PARENT(NULL, NULL, "ns_ta_dir = %s, wdir = %s", GET_NS_TA_DIR(), g_ns_wdir);
  start_testidx = testidx; //storing testidx to global start_testidx for monitoring purpose


    /*If run_mode_option is given then 
     * 
     * compiling script*/
 
  if (run_mode_option & RUN_MODE_OPTION_COMPILE) {
    sprintf(g_ns_tmpdir, ".tmp/ns-inst");
  }

  //strcpy(g_ns_login_user, pw->pw_name);

  // Before creating sort_file we need to copy additional_kw.conf if present
  save_kw_args_in_file();  
  write_log_file(NS_INITIALIZATION, "Parsing scenario settings");
  if (sort_conf_file(g_conf_file, g_sorted_conf_file, err_msg) == -1) {
    //end_stage(NS_INITIALIZATION, TIS_ERROR, "Failed to merge and sort scenario file %s\n%s", g_conf_file, err_msg);
    //exit(-1);  //do not use NS_EXIT as g_partition_idx is 0
    NS_EXIT(-1, "Failed to merge and sort scenario file %s\n%s", g_conf_file, err_msg);
  }

  if(!((run_mode_option & RUN_MODE_OPTION_COMPILE) ||
       (loader_opcode == MASTER_LOADER && (ni_make_tar_option & CREATE_TAR_AND_EXIT))))
  {
    write_log_file(NS_INITIALIZATION, "Parsing partition and multidisk keys", g_conf_file);
    if(parse_keyword_before_init(g_sorted_conf_file, err_msg) == -1) {
      //NS_EXIT(-1, "Failure in usage of PARTITION_SETTINGS or MULTIDISK_PATH");
      //end_stage(NS_INITIALIZATION, TIS_ERROR, "Failure in sorting scenario file %s, error:%s", g_conf_file, err_msg);
      //exit(-1);  //do not use NS_EXIT as g_partition_idx is 0
      NS_EXIT(-1, "Failure in sorting scenario file %s, error:%s", g_conf_file, err_msg); 
    }
    write_log_file(NS_INITIALIZATION, "Creating partition directory inside %s", g_test_or_session);
    create_partition_dir();
  
    write_log_file(NS_INITIALIZATION, "Saving controller configuration file");
    //Shibani Patra Add calling for ns_write_cav_conf_file() to dump cav.conf file in test run.
    ns_write_cav_conf_file();

    write_log_file(NS_INITIALIZATION, "Creating links and directories inside %s", g_test_or_session);
    if(global_settings->multidisk_nslogs_path)
    {
      create_links_for_logs(1); 
    }
    else
    {
      sprintf(buf, "TR%d/ns_logs/", testidx);
      check_and_rename(buf, DIRECTORY_OR_REGULAR_FILE_AND_KEYWORD_DISABLE);
      sprintf(buf, "%s/logs/TR%d/ns_logs/", g_ns_wdir, testidx);
      if (mkdir_ex(buf) != 1) 
      {
        if(errno == ENOSPC)
        {
          NS_EXIT(-1, CAV_ERR_1000008);
        }
        if(errno == EACCES)
        {
          NS_EXIT(-1, CAV_ERR_1000009, g_ns_wdir);
        }
        MKDIR_ERR_MSG("logs/TR/ns_logs", buf)
      }
 
      sprintf(buf, "TR%d/server_logs/", testidx);
      check_and_rename(buf, DIRECTORY_OR_REGULAR_FILE_AND_KEYWORD_DISABLE);
      sprintf(buf, "TR%d/server_signatures/", testidx);
      check_and_rename(buf, DIRECTORY_OR_REGULAR_FILE_AND_KEYWORD_DISABLE);
      sprintf(buf, "TR%d/debug.log", testidx);
      check_and_rename(buf, DEBUG_LOG_FILE_AND_KEYWORD_DISABLE);
      sprintf(buf, "TR%d/debug.log.prev", testidx);
      check_and_rename(buf, DIRECTORY_OR_REGULAR_FILE_AND_KEYWORD_DISABLE);
    }
 
    //creating sess warning file in ns_logs and truncate
    open_sess_file(&sess_warning_fd);
    end_stage(NS_INITIALIZATION, TIS_FINISHED, NULL);
  }
   
  if(loader_opcode == MASTER_LOADER && !(ni_make_tar_option & DO_NOT_CREATE_TAR)) {
    /*TODO: Need to check debug level*/
    init_stage(NS_GEN_VALIDATION);
    NSDL2_PARENT(NULL, NULL, "Scenario distribution tool call");
    //Create NetCloud directory in Controller's TR
    write_log_file(NS_GEN_VALIDATION, "Checking and creating necessary Netcloud specific directories inside %s", g_test_or_session);
    sprintf(net_cloud_data_path, "%s/logs/TR%d/NetCloud", g_ns_wdir, testidx);
    sprintf(controller_path, "%s/logs/TR%d/.controller", g_ns_wdir, testidx);
    if(ni_make_tar_option & CREATE_TAR_AND_EXIT)
      sprintf(controller_path, "%s/.tmp/.controller", g_ns_wdir);
    if(mkdir(controller_path, 0755))
    {
      NSDL2_PARENT(NULL, NULL, "controller_path = [%s] exists, error: %s", controller_path, nslb_strerror(errno));
      if(errno == EEXIST) {
        NSDL2_PARENT(NULL, NULL, "removing previous controller directory[%s]", controller_path);
        sprintf(rm_controller_dir, "rm -rf %s", controller_path);
        if(system(rm_controller_dir) != 0) {
          NS_EXIT(-1, CAV_ERR_1000004, controller_path, errno, nslb_strerror(errno));
          //NS_EXIT(-1, "Failed to remove directory = [%s]", controller_path);
        }
      }

      NSDL2_PARENT(NULL, NULL, "creating controller directory[%s]", controller_path);
      if(mkdir(controller_path, 0755))
        NS_EXIT(-1, CAV_ERR_1000005, controller_path, errno, nslb_strerror(errno));
        //NS_EXIT(-1, "Failed to create directory = [%s], error:%s", controller_path, nslb_strerror(errno));

      sprintf(netcloud_path, "%s/logs/TR%d/NetCloud/NetCloud.data", g_ns_wdir, testidx);
      if(!stat(netcloud_path, &s)) {
        get_current_time(cur_time);
        sprintf(new_netcloud_file, "%s_%s", netcloud_path, cur_time);
        NSTL1(NULL, NULL, "Creating backup of NetCloud.data inside %s, renaming into %s_%s",
                                           g_test_or_session, netcloud_path, cur_time);
        if (rename(netcloud_path, new_netcloud_file) != 0) {
          NS_EXIT(-1, CAV_ERR_1000012, netcloud_path, new_netcloud_file, errno, nslb_strerror(errno));
          //NS_EXIT(-1, "Failed to rename NetCloud.data into backup, path = [%s]", netcloud_path);
        }
      }
    }
    else if(ni_make_tar_option & CREATE_TAR_AND_CONTINUE)
    {
      NSTL1(NULL, NULL, "Creating NetCloud directory inside %s", g_test_or_session);
      if(mkdir(net_cloud_data_path, 0755))
      {
        if(errno != EEXIST) {
          NS_EXIT(-1, CAV_ERR_1000005, net_cloud_data_path, errno, nslb_strerror(errno));
          //NS_EXIT(-1, "Failed to create directory %s, error %s", net_cloud_data_path, nslb_strerror(errno));
        }
      }
    }
    
    write_log_file(NS_GEN_VALIDATION, "Filling controller IP and generator conf file");
    ret = nslb_get_controller_ip_and_gen_list_file(controller_ip, g_conf_file, gen_file, g_ns_wdir, testidx, global_settings->tr_or_partition, err_msg, &default_gen_file);
    if(ret != SUCCESS_RETURN) {
      NS_EXIT(-1, "%s", err_msg);
    }
    if (used_generator_entries)
    {
      ret = nslb_get_gen_for_given_controller(g_ns_wdir, gen_file, err_msg, 0);
      if(ret != SUCCESS_RETURN) {
        NS_EXIT(-1, "%s", err_msg);
      }
      write_log_file(NS_GEN_VALIDATION, "Validating configured generators for the server %s", controller_ip);
      ret = nslb_validate_gen_are_unique_as_per_gen_name(g_ns_wdir, controller_ip, err_msg, 0);
      if(ret != SUCCESS_RETURN) {
        NS_EXIT(-1, "%s", err_msg);
      } 
    }
    init_scenario_distribution_tool(g_conf_file, 4, testidx, 0, default_gen_file, 0, err_msg);
    if(ni_make_tar_option & CREATE_TAR_AND_EXIT) 
    {
      //If we are running in CREATE_TAR then no need to go further just stop here
      NS_EXIT(0, "Tar file creation over, now exiting from the NS");
    }
    create_generator_dir_by_name();
    // For external controller we need to read used generator list 
    // and need to fill generator structure. 
    extract_external_controller_data();

    end_stage(NS_GEN_VALIDATION, TIS_FINISHED, NULL);
  }
 
 
  init_stage(NS_SCENARIO_PARSING);
  write_log_file(NS_SCENARIO_PARSING, "Saving build version number");
  //set ns version
  get_ns_version_with_build_number();

  /*Create thead specific data*/
  //create_thread_specific_data();

  parent_init_after_parsing_args();

  // Calling set_advance_param_flag_in_db_replay function to mark advance param flag for file parameter in single & multiple groups.
  if(global_settings->db_replay_mode == REPLAY_DB_QUERY_USING_FILE) 
  {
    set_advance_param_flag_in_db_replay();
  }

  //Set tcp stats for WAN
  //NC: In case of WAN, on controller modems should not be open as not required.
  if (loader_opcode != MASTER_LOADER)
  {
    if(global_settings->wan_env)
      init_wan_setsockopt();
  }
  //Only for RBU
  if(global_settings->wan_env && !global_settings->enable_ns_firefox && (global_settings->protocol_enabled & RBU_API_USED))
  {
    NSTL1_OUT(NULL, NULL, "Warring: WAN setting is ignoring because in case of RBU based scripts WAN simulation is only applied "
                    "for NETSTORM FIREFOX. To apply WAN setting add keyword ENABLE_NS_FIREFOX in your scenario.");
  }
  /*Bug 8900: If license file is not found on generators still they should continue test
    In NC setup clients are using our machines as generators here license verification isnt require
    By pass flow*/
  if (loader_opcode != CLIENT_LOADER)
  {
    write_log_file(NS_SCENARIO_PARSING, "Validating license for the product");
    //set ns version
    if((ret = ns_validate_license("Netstorm", err_msg)))
    {
      if(ret == -2){ //ret = -2 means we need to stop test on invalid license
        NS_EXIT(-1, "%s", err_msg);
      }
    }
  }
  if((check_and_send_mail(1) == -1)){
    NS_EL_2_ATTR(EID_MISC,  -1, -1, EVENT_CORE, EVENT_INFO,
                __FILE__, (char*)__FUNCTION__, "Failed to send mail\n");
  }

  /* check processes is running, befor the test is start
   * Making this as dummy function which might be used in future */
  parent_chk_all_process_running_befor_start_test();

  if(debug_trace_log_value != 0 && (global_settings->protocol_enabled & RBU_API_USED))
    script_execution_log_init(); //to print header of script_execution.log file

  org_percentile_report = g_percentile_report; /* Initialize first */

  //Initialize seeson id here because for continous monitoring we have to start session with a unique number
  init_sess_inst_start_num();

  end_stage(NS_SCENARIO_PARSING, TIS_FINISHED, NULL);

  /*Now copy script is parallel*/
  //Bug-94145: Need to copy .rbu_parameter.csv inside TR script, it will be used to stop VNC and clean profile
  if ((global_settings->script_copy_to_tr != DO_NOT_COPY_SRCIPT_TO_TR) || (global_settings->protocol_enabled & RBU_API_USED)) 
  {
    sighandler_t prev_handler;
    pthread_t copy_script_thread_id;
    pthread_attr_t copy_script_attr_id;
    pthread_attr_init(&copy_script_attr_id);
    pthread_attr_setdetachstate(&copy_script_attr_id, PTHREAD_CREATE_DETACHED);
    prev_handler = signal( SIGCHLD, SIG_IGN);
    int result = pthread_create(&copy_script_thread_id, &copy_script_attr_id, copy_scripts_to_tr, NULL);
    if(result)
    {
      write_log_file(NS_COPY_SCRIPTS, "Failed to create thread to copy scripts to %s", g_test_or_session);
      NSTL1_OUT(NULL, NULL, "Failed to create thread to copy scripts to %s", g_test_or_session);
    }
    (void) signal( SIGCHLD, prev_handler);
    pthread_attr_destroy(&copy_script_attr_id);
  }

  //Send pre test message to tomcat
  fill_pre_post_test_msg_hdr(MSG_PRE_TEST_PKT);
  send_pre_post_test_msg_to_tomcat();

  //ndc setup
  setup_for_ndc();

  init_stage(NS_MONITOR_SETUP);
  setup_for_monitors();
  // copying to shared memory done outside of copy_struct_to_shr_mem as below 
  // function used gdf data but this gdf data is populated in process_all_gdf function
  // called from setup_for_monitors function
  copy_slaTable_to_shr(sla_table_shr_mem);
  monitor_connection();
  parent_epoll_create(global_settings->num_process);
  dh_epoll_create();
 
  (void) signal( SIGCHLD, handle_parent_sickchild );
  NSTL1(NULL, NULL, "Size of Progress Report g_avgtime_size = [%d]", g_avgtime_size);

  if (global_settings->smon)
  {
    write_log_file(NS_MONITOR_SETUP, "Waiting for sockets to clear if time-wait sockets are greater than %d",
                                      global_settings->tw_sockets_limit);
    wait_sockets_clear();
  }
  write_log_file(NS_MONITOR_SETUP, "Saving host table");
  if(log_host_table() == -1) {
     NSTL1_OUT(NULL, NULL, "Error in writing HOSTTABLE in slog file");
     NSDL2_RBU(NULL, NULL, "Error in writing HOSTTABLE in slog file..");
  }
  
  end_stage(NS_MONITOR_SETUP, TIS_FINISHED, NULL);
  
  if(run_mode == FIND_NUM_USERS)
     init_stage(NS_GOAL_DISCOVERY);

  while((test_result == SLA_RESULT_OK) || (test_result == SLA_RESULT_CONTINUE))
  {
    test_result = check_proc_run_mode(end_results , c_end_results);
    if(test_result == SLA_RESULT_CONTINUE)
      continue;
    else if((test_result != SLA_RESULT_OK) && (test_result != SLA_RESULT_LAST_RUN))
      break;

    // Delay should be only for Goal Based Scen and not for last Test Sample
    if((test_result != SLA_RESULT_LAST_RUN) && (first_time != 1))
    {
      if(testCase.stab_delay > 0) // if delay has been specified by the user with STABILIZE keyword
      {
        fprint3f(stdout, rfp, NULL, "\nSleeping for %u millisecs before starting the next Test Sample\n\n", testCase.stab_delay);

        NSDL3_PARENT(NULL, NULL, "Sleeping for %u millisecs before starting the next Test Sample", testCase.stab_delay);
        usleep(1000 * testCase.stab_delay); // Since testCase.stab_delay will be in the milliseconds
        NSDL3_PARENT(NULL, NULL, "Sleeping is over");
        if(sigterm_received) 
        {
          save_status_of_test_run(TEST_RUN_STOPPED_BY_USER);
          break; // To handle ctrl C during sleep
        }
      }
    }

    parent_init_before_starting_test_run();
    //Each time when a test is started we have to reset cur_sample, avgtime 
    if(is_goal_based)
      reset_avgtime();
    // Call free_structs when: Test is in NORMAL_RUN(normal test) or in goal based tests when we moved to NORMAL_RUN at the end
    if(run_mode == NORMAL_RUN)
//    if(first_time)
       free_structs(); 
    //dump_sharedmem();
// this writes to summary.html
  //making seperate process to read dlog file from generator and copy itto controller dlog
  /*  if(loader_opcode == MASTER_LOADER)
      init_component_rec_and_start_nia_fa();
  */


    //start db_aggregator only if net diagnostics mode is 1,2 and db_aggregator_mode is 1
    if((global_settings->net_diagnostics_mode >= 1) && (global_settings->db_aggregator_mode == 1))
    {
      write_log_file(NS_START_INST, "Starting database aggregator");
      NSDL1_PARENT(NULL, NULL, "starting db aggregator process");
      init_component_rec_and_start_db_aggregator();
    }
   

// Start writing to summary.report
// Folloing block moved from netstorm.c as at that time we had not target_completion_time
    fprint3f(rfp, srfp, NULL, "Target Completion: %s\n", target_completion_time);
    if (global_settings->num_dirs) {
    fprint3f(rfp, srfp, NULL, "Using SpecWeb file set with %d directories\n\n", global_settings->num_dirs);
    } else {
    fprint3f(rfp, srfp, NULL, "Using %d URL's\n\n", total_request_entries);
    }
// End writing to summary.report

    if(loader_opcode != MASTER_LOADER) {
      calculate_rpc_per_phase_per_child();
    }

    /* Set actual value for g_percentile_report if test run is NORMAL_RUN */
    set_percentile_report_for_sla(run_mode, org_percentile_report);
    /* Update global.dat file of generator with controller-generator information*/
    if(loader_opcode == CLIENT_LOADER) 
    update_global_dat_for_generator();

    /* For ns_set_form_body API */
    init_form_api_escape_replacement_char_array();
    
    if(loader_opcode == CLIENT_LOADER)
      g_testrun_idx = atoi(g_controller_testrun); 
    else
      g_testrun_idx = testidx; 
    start_test();
    log_gdf_all_data();
    if(sigterm_received) break;
    first_time = 0;
  }

  /*Stop the proxy if ns_exbrowser api is used */
  //if (global_settings->protocol_enabled & BRU_API_USED)
    //start_ns_proxy_server("stop"); 

  // Added in 3.7.6
  // We are calling this to clear all shared modems used
  if (loader_opcode != MASTER_LOADER)
    clear_shared_modems();
  parent_save_data_before_end();

  close_pct_msg_dat_fd();

  //Stop all monitor connections after test is over so that all monitors close gracefully
  NSDL2_MON(NULL, NULL, "Stopping all monitors if any still running.");
  //stop_all_monitors();         //stop all monitors/netocean monitors connection
  //stop_all_custom_monitors();  //stop all custom monitors/standard monitors connection

  /* In release 3.9.6, 
   * In case of stopping NDC, NS will send STOP message to NDC and wait for response
   * this response message will be computed after post processing task are completed
   * */
  if(loader_opcode != CLIENT_LOADER) 
  {
    if(global_settings->net_diagnostics_port != 0)
      stop_nd_ndc_data_conn();
    
    if((global_settings->net_diagnostics_mode >= 1) && (global_settings->net_diagnostics_port != 0))
      stop_nd_ndc(); 
  }

  stop_all_check_monitors();   //stop all check monitors/server signature connection

  // Neeraj - this was moved before post processing on Oct 7, 2010
  // This was done so that Cmon does not have to keep Long timeout for removing
  // test run from the hash table to indicate that test is over
  // Also it make sense to do it before post processing as post processig make take Long time.
  start_check_monitor(CHECK_MONITOR_EVENT_AFTER_TEST_IS_OVER, ""); //After Test Run is over
  /* Send end test run message to all servers which are marked used*/
  
  //Send post test message to tomcat
  fill_pre_post_test_msg_hdr(MSG_POST_TEST_PKT);
  send_pre_post_test_msg_to_tomcat();
 
  //We will send end message to NDC in case outbound connection is enabled.
  if(! is_outbound_connection_enabled)
   // send_end_msg_to_NDC();
 // else
    send_end_msg_to_all_cavmonserver_used();

  //free ServerInfo tables
  //topolib_free_servers_list(topo_idx); 
  //free tier configuration tables
  topolib_free_topo_list(topo_idx);  

  dynamic_url_destroy();

  //write 'partiion processing done' file if no sql report mon was active 
  if(g_partition_idx > 0 && global_settings->sql_report_mon)
    write_lps_partition_done_file(1);  //1 means this is test stop

  //if(g_partition_idx > 0 && is_norm_init_done)
  if(g_partition_idx > 0 && is_mssql_monitor_applied)
    write_lps_partition_done_file_for_mssql(1);

  //We determine if partition is most recent or not by using partition.status file.
  //This file is created in every partition after switch.
  //Creating this file on test stop, so that nsu_archive_trun doesn't misinterpret it as running partition
  if(g_partition_idx > 0)//TODO
    save_status_of_partition("Test is stopped");

  autoRecordingDTServer(STOP_DT_RECORDING);

  end_test_init_running_stage(controller_ip);

  NSTL1_OUT(NULL, NULL, "Starting post processing at %s. It may take time. Please wait ...", get_relative_time());
  
  //Write aggregate transaction data in csv
  if(global_settings->write_rtg_data_in_db_or_csv)
  {
    //Write aggregate data only if db mode is enabled
    if(rtgDataMsgQueueAndTablesInfo_obj.writeInCsvOrDb & (1 << DB_WRITE_MODE))
    {
      NSTL1(NULL, NULL, "writing txn aggregate data in csv");
      write_transaction_aggregate_data_in_csv();
      
      if(rtgDataMsgQueueAndTablesInfo_obj.influxDbInfo_obj.influxDbIP[0] && 
         rtgDataMsgQueueAndTablesInfo_obj.influxDbInfo_obj.influxDbPort)
      { 
        make_conn_and_insert_data_into_influx_db();
      }

      char errorMsg[1024];
      errorMsg[0] = '\0';
      nslb_odbc_free_handles(&(rtgDataMsgQueueAndTablesInfo_obj.odbcVarInfo_obj), errorMsg, 1024);
      if(errorMsg[0])
      {
        NSTL1(NULL, NULL, "ERROR[%s] in nslb_odbc_free_handles");
      }
    }
  }

  set_test_run_info_status(TEST_RUN_POST_PROCESSING);
  post_processing_after_test_run(test_result);
  NSTL1_OUT(NULL, NULL, "Post processing completed at %s.", get_relative_time());

  NSDL2_PARENT(NULL,NULL, "global_settings->get_gen_tr_flag = %hd", global_settings->get_gen_tr_flag);
  
  //NC: Call nii_generate_compare_report shell script to create compare report
  if (loader_opcode == MASTER_LOADER) 
  {
    //sprintf(net_cloud_data_path, "nii_generate_compare_report %d", testidx);
    struct passwd *pw;
    pw = getpwuid(getuid());
    if (pw != NULL)
    {
      NSDL4_PARENT(NULL, NULL, "user_name = %s", pw->pw_name);
      sprintf(net_cloud_data_path, "nsi_gen_cmp_rep_set_data %d %s", testidx, pw->pw_name);
      if(system(net_cloud_data_path) != 0)
      {
        NSTL1(NULL, NULL, "Error in running compare report generation command");
        NS_EXIT(-1, "Error in running compare report generation command");
      }
    }
    else
      printf("Error: Unable to get the real user name\n");
  }
  set_test_run_info_status(TEST_RUN_OVER);

  if((check_and_send_mail(4) == -1)){
    NS_EL_2_ATTR(EID_MISC,  -1, -1, EVENT_CORE, EVENT_INFO,
                __FILE__, (char*)__FUNCTION__, "Failed to send mail\n");
  }
    
  ns_clean_current_instance();
  save_status_of_test_run(TEST_RUN_COMPLETED_SUCCESSFULLY);
  //close_event_log_fd();
  return 0;  // Dont remove this return, this is intentional, for comming out of main (the function return type is void)
}

void clean_parent_memory_for_goal_based()
{
  // We have to free all data allocated by Parent everytime at the end of test
  // Free Vusersummary Table
  FREE_AND_MAKE_NULL_EX(gVUserSummaryTable, gVUserSummaryTableSize, "VUserSummaryTable", 0);

  int nvmindex;
  // Free http_satus_code norm table
  for(nvmindex = 0; nvmindex < global_settings->num_process; nvmindex++)
  {
    FREE_AND_MAKE_NULL_EX(g_http_status_code_loc2norm_table[nvmindex].nvm_http_status_code_loc2norm_table, total_http_resp_code_entries * sizeof(int), "nvm_http_status_code_loc2norm_table", (int)-1);

  }
  FREE_AND_MAKE_NULL_EX(g_http_status_code_loc2norm_table, (global_settings->num_process) * sizeof(HTTP_Status_Code_loc2norm_table), "HTTP_Status_Code_loc2norm_table", (int)-1);

  // Free tx norm table
  for(nvmindex = 0; nvmindex < global_settings->num_process; nvmindex++)
  {
     FREE_AND_MAKE_NULL_EX(g_tx_loc2norm_table[nvmindex].nvm_tx_loc2norm_table, total_tx_entries * sizeof(int), "g_tx_loc2norm_table", nvmindex);
  }
  FREE_AND_MAKE_NULL_EX(g_tx_loc2norm_table, (global_settings->num_process) * sizeof(TxLoc2NormTable), "TxLoc2NormTable", (int)-1);

}
// End of File
