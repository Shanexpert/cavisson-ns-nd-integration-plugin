#define _GNU_SOURCE
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <v1/topolib_log.h>
#include <sys/wait.h>
#include <libgen.h>
#include <locale.h>
#ifdef USE_EPOLL
#include <sys/epoll.h>
#endif
#include <regex.h>
#include <libpq-fe.h>
#include <errno.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "nslb_util.h"
#include "nslb_cav_conf.h"
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
#include "ns_event_filter.h"
#include "ns_event_id.h"
#include "ns_common.h"
#include "ns_http_cache_reporting.h"
#include "dos_attack/ns_dos_attack_reporting.h"
#include "ns_http_cache_table.h"
#include "ns_vuser_trace.h"

#include "tr069/src/ns_tr069_lib.h"
#include "tr069/src/ns_tr069_data_file.h"
#include "ns_auto_fetch_parse.h"
#include "ns_url_hash.h"
#include "ns_netstorm_diagnostics.h"
#include "ns_runtime_changes.h"
#include "ns_ftp.h"
#include "ns_lps.h"

#include "ns_http_hdr_states.h"

#include "nslb_http_state_transition_init.h"
#include "nslb_db_util.h"
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
#include "ns_test_gdf.h"
#include "ns_monitoring.h"
#include "ns_trace_level.h"
#include "nslb_partition.h"
#include "ns_ndc.h"
#include "ns_auto_scale.h"
#include "ns_runtime_changes_monitor.h"
#include "ns_appliance_health_monitor.h"
#include "nslb_mon_registration_con.h"
#include "ns_ndc_outbound.h"
#include "ns_data_handler_thread.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"
#include "ns_monitor_profiles.h"
#include "ns_handle_alert.h"
#include "ns_file_upload.h"
#include "ns_standard_monitors.h"

#if OPENSSL_VERSION_NUMBER < 0x10100000L
  #include <openssl/ssl_cav.h>
#endif


#define MAX_SEND_BUFF_SIZE 128
#define NDE_MAX_LINE_LEN 1024
#define NDE_MAX_NAME_LEN 128
#define ORACLE_SQL_STATS_CSV_COUNT 13

#define SEND_SIGNAL_TO_WRITER \
    if(writer_pid > 0 ){ \
      if (kill(writer_pid, SIGRTMIN+2) == -1) { \
        if (errno != ESRCH) { \
          perror("Error in sending the logging_writer a signal"); \
          NS_EXIT(-1,CAV_ERR_1060001); \
        } \
      } \
    }else{ \
      NSTL1(NULL, NULL, "Not sending signal as writer_pid '%d' is less than or equal to 0", writer_pid); \
    } 

extern int monitor_log_fd;
extern  void fill_up_buffer(int size_written, char* buffer); 
extern inline void fillTestRunGdf(FILE *testRun_fp);
extern Long_data rtg_pkt_ts; 

//for monitoring, to send first partition idx created on NS start to cmon in all the msg communication
// Note - In shell and java based monitor, we are still checing for -1, so we need to keep it -1 for now
// We will change monitors to check > 0 for partition mode and then we can change this to 0
long long g_start_partition_idx = -1; //first partition of the current test (set when the test start/restart)
long long g_first_partition_idx = -1; //partition id when the test was started first time
long long g_partition_idx = 0; //current partition
long long g_prev_partition_idx = 0;
long long g_partition_idx_for_generator = 0; //partition passed from controller to generator on switch
int test_run_info_shr_mem_key;
PartitionInfo partitionInfo;
char base_dir[512 + 1] = "";

u_ns_ts_t partition_start_time = 0;

int *test_run_number;
int *partiton_index;
int *absolute_time;
int rtg_msg_seq = 0;

void set_idx_for_partition_switch(ClientData cd, u_ns_ts_t now);
ClientData g_client_data;
char g_set_check_var_to_switch_partition;
long long g_loc_next_partition_time;
long long g_loc_next_partition_idx;

//This structure is only used while doing entry in .multidisk_path in TR to compare multidisk path and add unique entries in file.
struct Multidisk_path_struct
{
  char multidisk_path[1024];
  int inode_num;
  char is_link;
}multidisk_path_struct_arr[25] = {0};


char *partition_info_buf_to_append;

#define PARTITION_SWITCHED_SUCCESSFULLY "Partition Switched Successfully"

int get_keyword_value_from_file(char *file, char *keyword, char *result, int size);
static int nde_calculate_switch_duration();
TestRunInfoTable_Shr *testruninfo_table_shr = NULL;
PartitionInfo partInfo;

/*  This function parses PARTITION_SETTINGS keyword 
 *  Usage: PARTITION_SETTINGS <PARTITION_CREATION_MODE[0/1/2]> <SYNC_ON_FIRST_SWITCH[0/1]> <PARTITION_SWITCH_DURATION><Hh/Mm>
 *  PARTITION_CREATION_MODE: 
 *      1 - Create partition based on time; 
 *      2 - Auto mode.(Value for keyword will be set according to the mode of test run.)
 *  SYNC_DURATION_ON_FIRST_SWITCH: 
 *      0 - Don't sync partition duration with midnight; 
 *      1 - Sync partition duration.
 */
int kw_set_partition_settings(char *buf, int runtime_flag, char *err_msg)
{
  int partition_creation_mode = 1, sync_on_first_switch = 0, duration = 12;
  char time_unit = 'H', key[NDE_MAX_LINE_LEN + 1] = {0};
  NSDL1_PARENT(NULL, NULL, "Method Called");
  
  if (sscanf(buf, "%s %d %d %d%c", key, &partition_creation_mode, &sync_on_first_switch, &duration, &time_unit) < 2)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, PARTITION_SETTINGS_USAGE, CAV_ERR_1011067, CAV_ERR_MSG_1);
  }

  if(partition_creation_mode <= 0 || partition_creation_mode > 2)
  {
   //Sourabh:: changes for BUG 89839 Comment 1 issue if partition setting keyowrd first arg value is wrong then print CAV error.
    fprintf(stderr, CAV_ERR_1011067, CAV_ERR_MSG_3);
    exit(-1);
  }
  global_settings->partition_creation_mode = partition_creation_mode;
  //mode 2 is auto mode.
  //In auto mode if test is runing in continous monitoring mode then default value will be 1 1 8H else value will be 1 0 8H
  if(global_settings->partition_creation_mode == 2)
  {
    if(global_settings->continuous_monitoring_mode)
      global_settings->sync_on_first_switch = 1;
    else
      global_settings->sync_on_first_switch = 0;
    
    global_settings->partition_switch_duration_mins = 8 * 60;
  }
  else
  {
    if(sync_on_first_switch != 0 && sync_on_first_switch != 1)
    {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, PARTITION_SETTINGS_USAGE, CAV_ERR_1011067, CAV_ERR_MSG_3);
    }
    global_settings->sync_on_first_switch = sync_on_first_switch;
  
    /*  duration must be either multiple of 24 hours or factor of 24 hours (ie 1, 2, 3, 4, 6, 8, 12, 24, 48, 72, ...)
     *  Also we are not supporting duration more than 12 days;
     *  reason is: timeout variable of timer is integer type and stores time in milliseconds;
     *  hence it cannot hold time more than 20 days. */
    //if(duration <= 0 || duration > 12 * 24 || ((24 % duration != 0) && (duration % 24 != 0))) //TODO Krishna uncomment
    //  duration = 12;      //setting to default

    /*  If duration is provided in minutes  */
    if(time_unit == 'M' || time_unit == 'm')
      global_settings->partition_switch_duration_mins = duration;
    /*  If duration is provided in hours  */
    else if(time_unit == 'H' || time_unit == 'h')
      global_settings->partition_switch_duration_mins = duration * 60;
    else
    {
      //NSTL1(NULL, NULL, "Warning: Time unit provided with keyword 'PARTITION_SETTINGS' is not valid, hence setting to default 'Hour'.\n");
      NS_DUMP_WARNING("Time unit provided with keyword 'PARTITION_SETTINGS' is not valid, hence setting to default 'Hour'.");
      global_settings->partition_switch_duration_mins = duration * 60;
    }
  }

  NSDL2_PARENT(NULL, NULL, "global_settings->partition_creation_mode = %d, global_settings->sync_on_first_switch = %d, global_settings->partition_switch_duration_mins = %d",
           global_settings->partition_creation_mode, global_settings->sync_on_first_switch, global_settings->partition_switch_duration_mins);

  return 0;
}

/*  This function parses CAV_EPOCH_YEAR keyword used for continuous monitoring  
 *  Usage : CAV_EPOCH_YEAR <year> */
int kw_set_cav_epoch_year(char *buf, int runtime_flag, char *err_msg)
{
  char key[NDE_MAX_LINE_LEN + 1] = {0};
  long cav_epoch_diff, cav_epoch_diff_from_file;
  int cav_epoch_year;
  char temp_cavinfo[NDE_MAX_LINE_LEN + 1];
  sprintf(base_dir, "%s/logs/TR%d", g_ns_wdir, testidx);  //TODO Krishna set somewhere else
  NSDL1_PARENT(NULL, NULL, "Method Called");
  if(sscanf(buf, "%s %d", key, &cav_epoch_year) < 2)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, CAV_EPOCH_YEAR_USAGE, CAV_ERR_1011074, CAV_ERR_MSG_1);
  }

  NSTL1(NULL, NULL, "cav_epoch_year provided by user = %d", cav_epoch_year);

  if(global_settings->monitor_type)
  {
    strcpy(temp_cavinfo, g_cavinfo.config);
    strcpy(g_cavinfo.config, "NA");
  }

  //calculating cav epoch diff using cav_epoch_year
  if((!strncmp(g_cavinfo.config, "NS", 2)) || (!strncmp(g_cavinfo.config, "NC", 2)))
    //In case of NS just set start time.
    cav_epoch_diff = g_ts_init_time_in_sec;
  else
    cav_epoch_diff = nslb_get_cav_epoch_diff(cav_epoch_year);

  if(global_settings->monitor_type)
  {
    strcpy(g_cavinfo.config, temp_cavinfo);
  }
  //In case of NC, if gen then set what controller has sent
  if(loader_opcode == CLIENT_LOADER) {
    char *ns_cav_epoch = getenv("NS_CAV_EPOCH_DIFF");
    if(ns_cav_epoch != NULL) {
      cav_epoch_diff = atol(ns_cav_epoch);
    }
    else
    {
      //Could not get the value from the env var. Setting as NS
      NSTL1_OUT(NULL, NULL, "Genertaor did not get cav epoch diff value from controller through command line. Variable cav_epoch_diff is set as per current time.\n");
    }
  }

  //getting cav epoch diff from .cav_epoch.diff file
  cav_epoch_diff_from_file = nslb_get_cur_epoch_diff(base_dir);

  if(cav_epoch_diff_from_file <= 0)  //could not read .cav_epoch.diff file, hence set cav_epoch_diff as calculated from cav_epoch_year
  {
    NSTL1(NULL, NULL, "Error in reading cav_epoch_diff from .cav_epoch.diff file"
                      " ,calculating cav_epoch_diff using cav_epoch_year");
    global_settings->unix_cav_epoch_diff = cav_epoch_diff;
    nslb_save_epoch_diff(base_dir, global_settings->unix_cav_epoch_diff);  
  }
  else
  {
    //two cav_epoch_diff are different(one is from .cav_epoch_diff file, another is calculated using cav_epoch_year
    if(cav_epoch_diff_from_file != cav_epoch_diff)
    {
      NSTL1(NULL, NULL, "Cannot change cav_epoch_year for same test run number, using previously saved cav_epoch_diff");
    }
    else
    {
      NSTL1(NULL, NULL, "cav_epoch_year has not been changed after test restart");
    }    
    global_settings->unix_cav_epoch_diff = cav_epoch_diff_from_file;
  }

  global_settings->cav_epoch_year = cav_epoch_year;   //TODO Krishna check

  g_time_diff_bw_cav_epoch_and_ns_start = g_ts_init_time_in_sec - global_settings->unix_cav_epoch_diff;
  g_time_diff_bw_cav_epoch_and_ns_start_milisec = g_time_diff_bw_cav_epoch_and_ns_start * 1000;

  NSDL1_PARENT(NULL, NULL, "global_settings->cav_epoch_year = %d, global_settings->unix_cav_epoch_diff = %ld", 
                  global_settings->cav_epoch_year, global_settings->unix_cav_epoch_diff);
  // We dont need testruninfo_table_shr in cav main 
  #ifndef CAV_MAIN
  NSTL1(NULL, NULL, "global_settings->cav_epoch_year = %d, global_settings->unix_cav_epoch_diff = %ld," 
                    " testruninfo_table_shr->cav_epoch_diff = [%ld]", global_settings->cav_epoch_year, 
                      global_settings->unix_cav_epoch_diff, testruninfo_table_shr->cav_epoch_diff);
  #endif
  return 0;
}

/* This function gets version of all components and writes to file version in partition or TR */
void save_version()
{
  char cmd[1024] = {0};
  char err_msg[1024] = "\0";

  NSDL2_LOGGING(NULL, NULL, "Method Called.");
  // Note - We can only get version of NS/GUI/CMON/LPS/NDC/NetDiognostics as these are on same machine
  // Do not get version of HPD as it can be on NO machine and password less ssh is not for all users
  //-n NS, -g GUI, -c CMON, -l LPS, -D NDC, -d NetDiognostics
  sprintf(cmd, "nsu_get_version -n -g -c -l -D -d >> %s/logs/%s/version", g_ns_wdir, global_settings->tr_or_partition);
  NSDL2_LOGGING(NULL, NULL, "Command to get version is %s", cmd);

  if(nslb_system(cmd,1,err_msg) != 0)
  {
    NSDL2_LOGGING(NULL, NULL, "ERROR: Could not save version in file %s/logs/%s/version", g_ns_wdir, global_settings->tr_or_partition);
    printf("ERROR: Could not save version in file %s/logs/%s/version\n", g_ns_wdir, global_settings->tr_or_partition);
  }

  NSDL2_LOGGING(NULL, NULL, "Method Exited");
}

//Function used to open slog, dlog and sess_status_log file in append mode
int open_logging_files_in_append_mode(const Global_data* gdata, int testidx) 
{
  NSDL1_PARENT(NULL, NULL, "Method called, testidx = %d", testidx);
  char file_buf[MAX_FILE_NAME];

  if(static_logging_fd == -1)
  {  
    sprintf(file_buf, "%s/logs/TR%d/%lld/reports/raw_data/slog", g_ns_wdir, testidx, g_partition_idx);

    if ((static_logging_fd = open(file_buf, O_APPEND | O_RDWR | O_TRUNC |O_CLOEXEC,  S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)) == -1) {
      NSTL1_OUT(NULL, NULL, "start_logging: Error in opening logging file %s\n", file_buf);
      return -1;
    }
    if((static_logging_fp = fdopen(static_logging_fd, "a")) == NULL) {
      NSTL1_OUT(NULL, NULL, "start_logging: Error in opening logging file %s using fdopen\n", file_buf);
      return -1;
    }
  }
    
  sprintf(file_buf, "logs/%s/reports/raw_data/dlog", global_settings->tr_or_partition);
  if ((runtime_logging_fd = open(file_buf, O_APPEND | O_RDWR | O_TRUNC | O_NONBLOCK | O_LARGEFILE |O_CLOEXEC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)) == -1) {
    NSTL1_OUT(NULL, NULL, "start_logging: Error in opening logging file %s\n", file_buf);
    return -1;
  }

  /* Initialize session status recording file. */
  /*sprintf(file_buf, "logs/%s/sess_status_log", global_settings->tr_or_partition);
  if ((session_status_fd = open(file_buf, O_APPEND | O_RDWR | O_TRUNC | O_NONBLOCK | O_LARGEFILE, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)) == -1) {
    NSTL1_OUT(NULL, NULL, "Unable to open file %s\n", file_buf);
    return -1;
  }*/

  return 0;
}

//This function is to make .orl_process_done.txt inside partition if it is not made
//Function will return 0 if file is already exist, 1 if it has created file successfully, -1 in case of error

static int make_orl_process_done_file(long long partition)
{
  char buf[1024 + 1];
  FILE *fp;
  struct stat st;
  int result;

  sprintf(buf, "%s/logs/TR%d/%lld/reports/csv/.orl_process_done.txt", g_ns_wdir, testidx,  partition);
  result = stat(buf, &st);
  if(result == 0)
  {
    NSTL1(NULL, NULL, "%s file already exists in partition %lld", buf, partition);
    return 0;
  }

  fp = fopen(buf, "a");
  if(fp)
  {
    NSTL1(NULL, NULL, "NS is creating %s in partition %lld", buf, partition);
    fprintf(fp, "NS created this file\n");
    CLOSE_FP(fp);
    return 1;
  }

  NSTL1(NULL, NULL, "NS is not able to create %s in partition %lld. Error = %s", buf, partition, nslb_strerror(errno));
  return -1;
}

static int make_mssql_process_done_file(long long partition)
{
  char buf[1024 + 1];
  FILE *fp;
  struct stat st;
  int result;

  //sprintf(buf, "%s/logs/TR%d/%lld/reports/csv/.mssql_process_done.txt", g_ns_wdir, testidx,  partition);
  /*BUG 84397*/
  sprintf(buf, "%s/logs/TR%d/%lld/reports/csv/.genericDb_process_done.txt", g_ns_wdir, testidx,  partition);
  result = stat(buf, &st);
  if(result == 0)
  {
    NSTL1(NULL, NULL, "%s file already exists in partition %lld", buf, partition);
    return 0;
  }

  fp = fopen(buf, "a");
  if(fp)
  {
    NSTL1(NULL, NULL, "NS is creating %s in partition %lld", buf, partition);
    fprintf(fp, "NS created this file\n");
    CLOSE_FP(fp);
    return 1;
  }

  NSTL1(NULL, NULL, "NS is not able to create %s in partition %lld. Error = %s", buf, partition, nslb_strerror(errno));
  return -1;
}


//This function will be called once on starting test. 
//It will check for .orl_process_done_file.txt in all previous partition of TR
//If file is does not exist in any partition it will create it.

static int make_orl_process_done_file_on_start_test()
{
char prev_partition_name[16], cur_partition_name[16], base_dir[1024 + 1];
long long prev_partition;

  NSTL1(NULL, NULL, "make_orl_process_done_file_on_start_test called to check and make orl process done file for prev partitions");

  sprintf(base_dir, "%s/logs/TR%d", g_ns_wdir, testidx);
  sprintf(cur_partition_name, "%lld", g_partition_idx);
  
  for(;;)
  {
    int ret;
    NSDL1_PARENT(NULL, NULL, "Parent going to check .orl_process_done_file.txt file in partition previous to %s",cur_partition_name);

    ret = nslb_get_prev_partition(base_dir, cur_partition_name, prev_partition_name);
    if(ret != 0) // Prev parition is not present
      break;
    prev_partition = atoll(prev_partition_name);
    // File already present or error in making then stop.
    // It is assumed if file is present, than all prev to this are also present
    if(make_orl_process_done_file(prev_partition) <= 0) 
      break;
    strcpy(cur_partition_name, prev_partition_name);
  }

  return 1;
}

void create_csv_dir_and_link(char *partition_name)
{ 
  struct stat s;
  char buf[1024] = ""; 
  char out_buf[1024] = "";

  sprintf(buf, "%s/logs/TR%d/%s/reports/csv/", g_ns_wdir, testidx, partition_name);

  if(stat(buf, &s) == 0)
    return;
  else
  {
    sprintf(out_buf, "%s/logs/TR%d/%s/reports/", g_ns_wdir, testidx, partition_name);
    mkdir_ex(buf);
    mkdir_ex(out_buf);
  }

  return;
}

static int make_mssql_process_done_file_on_start_test()
{
  char prev_partition_name[16], cur_partition_name[16], base_dir[1024 + 1];
  long long prev_partition;

  NSTL1(NULL, NULL, "make_mssql_process_done_file_on_start_test called to check and make orl process done file for prev partitions");

  sprintf(base_dir, "%s/logs/TR%d", g_ns_wdir, testidx);
  sprintf(cur_partition_name, "%lld", g_partition_idx);
  
  for(;;)
  {
    int ret;
    //NSDL1_PARENT(NULL, NULL, "Parent going to check .mssql_process_done_file.txt file in partition previous to %s",cur_partition_name);
    NSDL1_PARENT(NULL, NULL, "Parent going to check .genericDb_process_done.txt file in partition previous to %s",                               cur_partition_name);

    ret = nslb_get_prev_partition(base_dir, cur_partition_name, prev_partition_name);
    if(ret != 0) // Prev parition is not present
      break;
    create_csv_dir_and_link(prev_partition_name);
    prev_partition = atoll(prev_partition_name);
    // File already present or error in making then stop.
    // It is assumed if file is present, than all prev to this are also present
    if(make_mssql_process_done_file(prev_partition) <= 0) 
      break;
    strcpy(cur_partition_name, prev_partition_name);
  }

  return 1;
}

//This function is called on partition switch or when test stop
//  On test stop it will create .orl_process_done.txt file
//  On partition switch it will check whether oracle stats monitor is running or not.
//    If monitor is not running and file is not created in partition it will assume that due to some reason monitor die and create file
//    If monitor is running it will return without creating file
void write_lps_partition_done_file(int test_stop_flag)
{
  int mon_id;
  CM_info *cm_ptr = NULL;

  NSDL2_PARENT(NULL, NULL, "Method Called, test_stop_flag = %d", test_stop_flag);

  //in case of test stop; create file anyway
  if(test_stop_flag)
  {
    NSTL2(NULL, NULL, "Test stop flag received");
  }
  else//If test is not stop check whether monitor is configured and running
  {
    for(mon_id = 0; mon_id < total_monitor_list_entries; mon_id++)
    {
      cm_ptr = monitor_list_ptr[mon_id].cm_info_mon_conn_ptr;

      //If monitor configured and it is running then return
      if((cm_ptr->flags & ORACLE_STATS) && cm_ptr->fd > 0) //running SQL_REPORT
      {
        NSTL2(NULL, NULL, "Oracle stats with SQL_REPORT metric monitor present and running, hence returning.");
        return;
      }
    }
  }

  make_orl_process_done_file(g_partition_idx);
}

//Send new test run message to all NVMs
static void send_msg_to_nvm(int opcode, int testidx)
{
  int i;
  HourlyMonitoringSess_msg testrun_msg;
  NSDL1_PARENT(NULL, NULL, "Method called, opcode = %d, testidx = %d, g_partition_idx = %lld, nvm = %d", 
                            opcode, testidx, g_partition_idx, global_settings->num_process);

  testrun_msg.opcode = opcode;
  testrun_msg.testidx = testidx;
  testrun_msg.partition_idx = g_partition_idx;

  for(i = 0; i < global_settings->num_process; i++)
  {
    NSDL2_PARENT(NULL, NULL, "writting msg at g_msg_com_con[%d].ip = %s, fd = %d",
                          i, g_msg_com_con[i].ip, g_msg_com_con[i].fd);
    testrun_msg.msg_len = sizeof(HourlyMonitoringSess_msg) - sizeof(int);
    write_msg(&g_dh_msg_com_con[i], (char *)&testrun_msg, sizeof(HourlyMonitoringSess_msg), 0, ISCALLER_DATA_HANDLER?DATA_MODE:CONTROL_MODE);
  }

  NSTL1(NULL, NULL, "Sent msg to nvm '%d', opcode '%d', testidx '%d', g_partition_idx '%lld'", 
                     global_settings->num_process, opcode, testidx, g_partition_idx);
}


void write_lps_partition_done_file_for_mssql(int test_stop_flag)
{
  NSDL2_PARENT(NULL, NULL, "Method Called, test_stop_flag = %d", test_stop_flag);

  //in case of test stop; create file anyway
  if(test_stop_flag)
  {
    NSTL2(NULL, NULL, "Test stop flag received");
  }

  make_mssql_process_done_file(g_partition_idx);
}

//Function is used to close and reset global fd,
//this function is called from netstorm_child()(ns_child.c file)
inline void close_and_reset_fd()
{
  NSTL1(NULL, NULL, "Closing debug_fd '%d', error_fd '%d', runtime_logging_fd '%d', session_status_fd '%d'", 
                     debug_fd, error_fd, runtime_logging_fd, session_status_fd);
  extern int ns_event_fd;
  //extern FILE * pct_msg_dat_fp;
  CLOSE_FD(ns_event_fd);
  //Bug 95447: Reset ns_event_fd for alert and configure the same.
  if(global_settings->alert_info->enable_alert)
  {
    ns_alert_config();
  }

  if(global_settings->monitor_type)
  {
    ns_config_file_upload(global_settings->file_upload_info->server_ip, global_settings->file_upload_info->server_port,
                          global_settings->file_upload_info->protocol, global_settings->file_upload_info->url,
                          global_settings->file_upload_info->max_conn_retry, global_settings->file_upload_info->retry_timer, ns_event_fd);
  }

  CLOSE_FD(debug_fd);
  CLOSE_FD(error_fd);
  CLOSE_FD(monitor_log_fd);
  CLOSE_FD(runtime_logging_fd);
  //CLOSE_FP(static_logging_fp);
  //CLOSE_FD(static_logging_fd);
  //CLOSE_FD(session_status_fd);
  //if(pct_msg_dat_fp != NULL) { CLOSE_FP(pct_msg_dat_fp); pct_msg_dat_fp = NULL; }

  if (!(g_tsdb_configuration_flag & TSDB_MODE))
  {
    CLOSE_FP(rtg_fp);
  }


  CLOSE_FD(script_execution_log_fd);
  CLOSE_FD(g_rbu_lighthouse_csv_fd);
}

void create_and_copy_testrun_gdf()
{
  char curpart_fname[MAX_LINE_LENGTH];
  char prevpart_fname[MAX_LINE_LENGTH];
  char err_msg[MAX_LINE_LENGTH];

  if(test_run_gdf_count == 0)
    sprintf(prevpart_fname, "%s/logs/TR%d/%lld/testrun.gdf", g_ns_wdir, testidx, g_prev_partition_idx);
  else
    sprintf(prevpart_fname, "%s/logs/TR%d/%lld/testrun.gdf.%d", g_ns_wdir, testidx, g_prev_partition_idx, test_run_gdf_count);

  sprintf(curpart_fname, "%s/logs/TR%d/%lld/testrun.gdf", g_ns_wdir, testidx, g_partition_idx);

  NSTL1(NULL, NULL, "Copying testrun.gdf from '%s' to '%s'",  prevpart_fname, curpart_fname);

  if(nslb_copy_file(curpart_fname, prevpart_fname, err_msg) < 0)
  {
      NSTL1(NULL, NULL, "Error in copying testrun.gdf from '%s' to '%s'. Error = %s",  prevpart_fname, curpart_fname, err_msg);
  }
}

static void create_and_copy_testrun_pdf()
{
  // TODO - Once GUI make percentile work for patition, then we will change this code switch files
  char curpart_fname[MAX_LINE_LENGTH];
  char prevpart_fname[MAX_LINE_LENGTH];

  NSDL1_PARENT(NULL, NULL, "Method called, g_percentile_report = %d, testrun_pdf_and_pctMessgae_version = %d", 
                            g_percentile_report, testrun_pdf_and_pctMessgae_version);

  if(g_percentile_report <= 0) // If not enabled, do nothing
    return;

  sprintf(curpart_fname, "%s/logs/TR%d/%lld/testrun.pdf", g_ns_wdir, testidx, g_partition_idx);

  /*****************************************************
    On partition switch cpoy testrun.pdf in partition
    with last version  
   ****************************************************/
  if(!testrun_pdf_and_pctMessgae_version)
    sprintf(prevpart_fname, "%s/logs/TR%d/%lld/testrun.pdf", g_ns_wdir, testidx, g_prev_partition_idx);
  else
    sprintf(prevpart_fname, "%s/logs/TR%d/%lld/testrun.pdf.%d.%llu", 
                            g_ns_wdir, testidx, g_prev_partition_idx, testrun_pdf_and_pctMessgae_version, testrun_pdf_ts);

  NSTL1(NULL, NULL, "Closing and Copying testrun.pdf from %s to %s", prevpart_fname, curpart_fname);

  pct_switch_partition(curpart_fname, prevpart_fname);

  /*****************************************************
    Reset pct var used in dyanmic tx
   ****************************************************/
  reset_pct_vars_for_dynameic_tx();
}

#if 0
static void copy_ns_files_to_new_tr()
{
  char buf[1024];
  //char ready_reports_path[600];

  NSDL1_PARENT(NULL, NULL, "Method called");

  /*Create ready_reports directory*/
  //sprintf(ready_reports_path, "logs/TR%d/ready_reports", testidx);
  //sprintf(buf, "mkdir -p -m 777 %s; touch %s/drill_down_query.log %s/drill_down_query_err.log; chmod 777 %s/*", ready_reports_path, ready_reports_path, ready_reports_path, ready_reports_path); 
  //system(buf);
  //NSTL1(NULL, NULL, "Created dir '%s' and files: %s/drill_down_query.log %s/drill_down_query_err.log", ready_reports_path, ready_reports_path, ready_reports_path);

  //Files required for logging 
  //Copy slog file for static URL details
  //sprintf(buf, "cp logs/TR%d/%ld/reports/raw_data/slog logs/TR%d/%ld/reports/raw_data/slog", 
  //                                                    testidx, g_first_partition_idx, testidx, g_partition_idx);
  //system(buf);
  //To create ect.csv
  //sprintf(buf, "bin/nsi_create_error_codes_csv %s", global_settings->tr_or_partition);  
  //system(buf);
  //NSTL1(NULL, NULL, "Executed '%s'", buf);

  //Copy all csv files in new TR
  //sprintf(buf, "cp logs/TR%d/%ld/reports/csv/*.csv logs/TR%d/%ld/reports/csv/", 
  //                                      testidx, g_first_partition_idx, testidx, g_partition_idx);
  //system(buf);

  //Files copied for GUI
  //To open drill down reports screen we required "scenario" file in new TRs
  //sprintf(buf, "cp logs/TR%d/%ld/scenario logs/TR%d/%ld/", testidx, g_first_partition_idx, testidx, g_partition_idx); 
  //system(buf);
  //To open scenario file from gui
  //sprintf(buf, "cp logs/TR%d/%ld/%s.conf logs/TR%d/%ld/", 
  //                                    testidx, g_first_partition_idx, g_scenario_name, testidx, g_partition_idx); 
  //system(buf);
  //To open script in execution and drill down report screen
  //sprintf(buf, "cp -r logs/TR%d/%ld/scripts logs/TR%d/%ld/scripts", testidx, g_first_partition_idx, testidx, g_partition_idx); 
  //system(buf);

  //To open phase textbox in custom query screen
  //sprintf(buf, "cp logs/TR%d/%ld/global.dat logs/TR%d/%ld/", testidx, g_first_partition_idx, testidx, g_partition_idx); 
  //system(buf);
  //Parent port allocated by the system 
  //sprintf(buf, "cp logs/TR%d/%ld/NSPort logs/TR%d/%ld/", testidx, g_first_partition_idx, testidx, g_partition_idx); 
  //system(buf);
  //Copy list of host for event filter

  //sprintf(buf, "cp logs/TR%d/%ld/ns_files/netstorm_hosts.dat logs/TR%d/%ld/ns_files/", 
    //                                            testidx, g_first_partition_idx, testidx, g_partition_idx); 


  //sprintf(buf, "mkdir -p -m 777 logs/TR%d/%ld/ns_files; cp logs/TR%d/%ld/ns_files/netstorm_hosts.dat logs/TR%d/%ld/ns_files/", testidx, g_partition_idx, testidx, g_first_partition_idx, testidx, g_partition_idx); 
  //system(buf);  
  //NSTL1(NULL, NULL, "Copying logs/TR%d/%ld/ns_files/netstorm_hosts.dat to logs/TR%d/%ld/ns_files/", 
  //          testidx, g_first_partition_idx, testidx, g_partition_idx);

  //Copy summary.report file 
  //sprintf(buf, "cp logs/TR%d/%ld/summary.report logs/TR%d/%ld/summary.report", testidx, g_first_partition_idx, testidx, g_partition_idx); 
  //system(buf);
  //NSTL1(NULL, NULL, "Copying logs/TR%d/%ld/summary.report to logs/TR%d/%ld/summary.report", testidx, g_first_partition_idx, testidx, g_partition_idx);
}
#endif

static void create_reports_dir()
{
  char buf[4*1024];
  char ns_logs_path[2*1024];
  int ret;
  NSDL1_PARENT(NULL, NULL, "Method called");

  /*Create reports directory*/
  sprintf(ns_logs_path, "logs/TR%d/%lld/ns_logs",  testidx, g_partition_idx);
  sprintf(buf, "mkdir -p -m 777 %s/req_rep", ns_logs_path); //Create directory for request and response files
  system(buf);
  NSTL1(NULL, NULL, "Created ns_logs directory '%s', req_rep '%s/req_rep", ns_logs_path, ns_logs_path); 

  make_ns_common_files_dir_or_link(global_settings->tr_or_partition);
  make_ns_raw_data_dir_or_link(global_settings->tr_or_partition);

  //make directory according to keyword set in scenarios Atul Sh.
  if(global_settings->protocol_enabled & RBU_API_USED)
  {
    char rbu_logs_path[512 + 1];
    sprintf(rbu_logs_path, "logs/TR%d/%lld/rbu_logs",  testidx, g_partition_idx);


    if(global_settings->multidisk_ns_rbu_logs_path && global_settings->multidisk_ns_rbu_logs_path[0])
    {

      char symlink_buf[512 + 1];
      //Create rbu_logs dir on multidisk
      sprintf(symlink_buf , "%s/%s/rbu_logs/harp_files/", global_settings->multidisk_ns_rbu_logs_path, global_settings->tr_or_partition);
      mkdir_ex(symlink_buf);
      //This will create the path till rbu_logs.
      sprintf(buf , "%s/logs/%s/rbu_logs/harp_files", g_ns_wdir , global_settings->tr_or_partition);
      mkdir_ex(buf);
 
      NSDL1_RBU(NULL, NULL, "Created directory = %s, har_tr_directory = %s", symlink_buf, buf);
      if(symlink(symlink_buf, buf) < 0)  //creating link of harp_files
      {
        NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_MAJOR, __FILE__, (char*)__FUNCTION__,
                        "Could not create symbolic link %s to %s .Error = %s ", symlink_buf, buf, nslb_strerror(errno));  
      }
    }
    if (global_settings->create_screen_shot_dir == SNAP_SHOT_DIR_FLAG) {
      sprintf(buf, "mkdir -p -m 777 %s/snap_shots && "
                   "mkdir -p -m 777 %s/harp_files", 
                   rbu_logs_path, rbu_logs_path); 
    }
    else  if (global_settings->create_screen_shot_dir == SCREEN_SHOT_DIR_FLAG) {
      sprintf(buf, "mkdir -p -m 777 %s/screen_shot && "
                   "mkdir -p -m 777 %s/harp_files", 
                   rbu_logs_path, rbu_logs_path); 
    }
    else if (global_settings->create_screen_shot_dir == ALL_DIR_FLAG) {
      sprintf(buf, "mkdir -p -m 777 %s/snap_shots && " 
                   "mkdir -p -m 777 %s/screen_shot && "
                   "mkdir -p -m 777 %s/harp_files",
                   rbu_logs_path, rbu_logs_path, rbu_logs_path);
    }
    else 
      sprintf(buf, "mkdir -p -m 777 %s/harp_files", rbu_logs_path); 

    NSDL1_PARENT(NULL, NULL, "cmd run = [%s] and rbu_logs_path = [%s]", buf, rbu_logs_path); 
    ret = nslb_system2(buf);
    if (WEXITSTATUS(ret) == 1){
      NS_EXIT(1,CAV_ERR_1060002);
    }
    else {  
      NSTL1(NULL, NULL, "Created harp_files dir \'%s/harp_files\'", rbu_logs_path); 
      NSDL1_PARENT(NULL, NULL, "Created harp_files dir \'%s/harp_files\' and create_screen_shot_flag is [%d] and command buf is=[%s]", 
                                rbu_logs_path, global_settings->create_screen_shot_dir, buf); 
    }

    //Create lighthouse directory
    if (global_settings->create_lighthouse_dir == LIGHTHOUSE_DIR_FLAG) {
      sprintf(buf, "mkdir -p -m 777 %s/lighthouse", rbu_logs_path);

      NSDL1_PARENT(NULL, NULL, "cmd run = [%s] and rbu_logs_path = [%s]", buf, rbu_logs_path);
      ret = nslb_system2(buf);
      if (WEXITSTATUS(ret) == 1){
        NS_EXIT(1,CAV_ERR_1060003);
      }
      else {  
        NSTL1(NULL, NULL, "Created lighthouse dir \'%s/lighthouse\'", rbu_logs_path);
        NSDL1_PARENT(NULL, NULL, "Created lighthouse dir \'%s/lighthouse\' and create_lighthouse_dir_flag is [%d] and command buf is=[%s]",
                                  rbu_logs_path, global_settings->create_lighthouse_dir, buf);
      }
    }
    //Create performance_trace directory
    if (g_rbu_create_performance_trace_dir == PERFORMANCE_TRACE_DIR_FLAG) {
      sprintf(buf, "mkdir -p -m 777 %s/performance_trace", rbu_logs_path);

      NSDL1_PARENT(NULL, NULL, "cmd run = [%s] and rbu_logs_path = [%s]", buf, rbu_logs_path);
      ret = nslb_system2(buf);
      if (WEXITSTATUS(ret) == 1){
        NS_EXIT(1,CAV_ERR_1060004);
      }
      else {  
        NSTL1(NULL, NULL, "Created performance_trace dir \'%s/performance_trace\'", rbu_logs_path);
        NSDL1_PARENT(NULL, NULL, "Created performance_trace dir \'%s/performance_trace\' "
                                 "and create_performance_trace_dir is [%d] and command buf is=[%s]",
                                  rbu_logs_path, g_rbu_create_performance_trace_dir, buf);
      }
    }
  }

  /*Create execution_log directory*/
  if(debug_trace_log_value != 0 && (global_settings->protocol_enabled & RBU_API_USED))
  {
    sprintf(buf, "mkdir -p -m 777 %s/logs/%s/execution_log", g_ns_wdir, global_settings->tr_or_partition); 
    nslb_system2(buf);
    NSTL1(NULL, NULL, "Created execution_log directory '%s/logs/%s/execution_log'", g_ns_wdir, global_settings->tr_or_partition);
  
    script_execution_log_init();
  }
}

void create_page_dump_dir_or_link()
{
  char buf1[1024];
  char buf2[1024];
  if(global_settings->multidisk_nslogs_path && global_settings->multidisk_nslogs_path[0])
  {
    sprintf(buf1, "%s/logs/%s/",  g_ns_wdir, global_settings->tr_or_partition);
    mkdir_ex(buf1);
    
    /* Create page_dump directory on multidisk path */
    sprintf(buf1, "%s/%s/page_dump/", global_settings->multidisk_nslogs_path, global_settings->tr_or_partition);
    mkdir_ex(buf1);
    
    /* Make soft link */
    sprintf(buf2, "%s/logs/%s/page_dump", g_ns_wdir, global_settings->tr_or_partition);
    if(symlink(buf1, buf2) < 0)
    {
      NSTL1_OUT(NULL, NULL, "Unable to create link buf1 = [%s] buf2 = [%s] Error = %s\n", buf1, buf2, nslb_strerror(errno));
    }

    /* Create docs directory */ 
    sprintf(buf1, "%s/logs/%s/page_dump/docs/", g_ns_wdir, global_settings->tr_or_partition);
    mkdir_ex(buf1);
  }
  else
  {
    sprintf(buf1, "%s/logs/%s/page_dump/docs/", g_ns_wdir, global_settings->tr_or_partition);
    mkdir_ex(buf1);
  }
}

//Create NS file for new TR
static void creating_ns_files_for_new_partition()
{
  //Close and reset global fds
  close_and_reset_fd();
  NSDL1_PARENT(NULL, NULL, "Method called");

  sighandler_t prev_handler;
  prev_handler = signal( SIGCHLD, SIG_IGN);

  //Create report directory (raw_data and csv)
  create_reports_dir();  
  NSTL1(NULL, NULL, "Ignoring signal 'SIGCHLD'.");

  //create logging files(dlog, log, slog)
  if (start_logging(global_settings, testidx, 0) == -1) {  
    NS_EXIT(-1,CAV_ERR_1060005);
  }
  NSTL1(NULL, NULL, "Created dlog 'logs/%s/reports/raw_data/dlog' and sess_status_log 'logs/%s/sess_status_log'", 
                     global_settings->tr_or_partition, global_settings->tr_or_partition);

  // copy_ns_files_to_new_tr();  

  NSTL1(NULL, NULL, "Continuing with signal 'SIGCHLD' handler.");
  (void) signal( SIGCHLD, prev_handler);

  //File pointer to progress report is closed by child process
  //need to close summary.report file ptr
  NSTL1(NULL, NULL, "Closing summary.top fd '%d'", srfp);
  CLOSE_FP(srfp);
  CLOSE_FP(rfp);

  NSTL1(NULL, NULL, "Opening srfp_html and adding title, version header and test configuration.");
  //Create summary and progress report and summary.top for new testrun
  create_report_file(testidx, 1);
  
  //creating page dump directories
  create_page_dump_dir_or_link();

  NSTL1(NULL, NULL, "Created summary.top and progress.report in partition '%s', g_enable_new_gdf_on_partition_switch = %d", 
                     global_settings->tr_or_partition, g_enable_new_gdf_on_partition_switch);

  //the delete vector counter is decremented for escaping the writing of vectors at partition switch
  if(g_enable_delete_vec_freq && g_delete_vec_freq_cntr != 1)
  {
    NSTL1(NULL, NULL, "Before g_delete_vec_freq_cntr = %d and after g_delete_vec_freq_cntr = %d",g_delete_vec_freq_cntr, g_delete_vec_freq_cntr - 1);
    g_delete_vec_freq_cntr--;
  }
 
  //If g_enable_new_gdf_on_partition_switch is set we make new gdf and remove all deleted monitor and vector from structure.
  if(g_enable_new_gdf_on_partition_switch )
  {
    g_rtg_rewrite = 1;

    if(monitor_runtime_changes_applied || (is_rtc_applied_for_dyn_objs()))
    {
      runtime_change_mon_dr();
      monitor_runtime_changes_applied = 0;
      //We need to reset dynObjForGdf structure because if there is any new entry between last progress interval and partition switch, then it will assign new rtg index to newly added txns and if not reset here then it will give new rtg index to those newly added txns in new partition's 1st testrun.gdf.diff.
      //We can ignore this as if there are any dynamic objects involved, the control will not come here.
      NSTL1(NULL, NULL, "Found new entry just after last deliver report and partition switch. Added the same in the structure.");
      //reset_dynamic_obj_structure();
    }
 
    MY_MALLOC_AND_MEMSET(rtgRewrt_monitor_list_ptr, (sizeof(Monitor_list) * total_monitor_list_entries), "", -1)
    merge_monitor_list_into_rtgRewrt_monitor_list(monitor_list_ptr, rtgRewrt_monitor_list_ptr, total_monitor_list_entries);
    reset_mon_rtgRewrt_tables();

    //this will clean check monitor if status is
    check_mon_cleanup();

    qsort(monitor_list_ptr, total_monitor_list_entries, sizeof(Monitor_list), check_if_cm_vector);
      set_no_of_monitors();

    if(is_outbound_connection_enabled)
      make_mon_msg_and_send_to_NDC(1, 0, 1);
    else
      make_connection_on_runtime();

    //set msg_data_size to size of all ..... gdf
    msg_data_size = ns_gp_msg_data_size;

    create_tmp_gdf();
    fwrite(testrungdf_buffer, sizeof(char), fsize, write_gdf_fp);
    fseek(write_gdf_fp, 0, SEEK_END);

    //memset group_data_ptr of all mon
    free_group_graph_data_ptr();
    //memset_graph_data_ptr_of_mon();
    //If Appliance monitor is enabled
    if(global_settings->enable_health_monitor)
      process_hm_gdf();

    //process transaction groups
    set_group_idx_for_txn = 1;
    set_rtg_index_for_dyn_objs_discovered();
    write_rtc_details_for_dyn_objs();

    //process custom gdf
    process_custom_gdf(1);
    close_gdf(write_gdf_fp);
    tmp_msg_data_size = msg_data_size;
    allocMsgBuffer();

    test_run_gdf_count = 0;
    create_testrun_gdf(0);
    create_and_copy_testrun_pdf();
    //create_testrun_pdf();
    g_rtg_rewrite = 0;
  }
  else {
    //Testrun GDF & PDF files
    create_and_copy_testrun_gdf();
    test_run_gdf_count = 0;
    //NSTL1(NULL, NULL, "Created and copied testrun.gdf from partiton 'TR%d/%d' to partition 'TR%d/%ld'", 
    //                   testidx, g_first_partition_idx, testidx, g_partition_idx);
    create_and_copy_testrun_pdf();
  }
  //close monitor_data.log fd
    CLOSE_FP(g_nd_monLog_fd);
  
  if(loader_opcode == MASTER_LOADER)
  {
    int i;
    for (i = 0; i < sgrp_used_genrator_entries; i++) {
      CONTINUE_WITH_STARTED_GENERATOR(i);
      close_gen_pct_file(i);
      create_gen_pct_file(i);
    }
  }

  //RTG files
  create_rtg_file_data_file_send_start_msg_to_server(0);  
  NSTL1(NULL, NULL, "Created rtgMessage.dat, gui.data in partition '%s' and sent messgae data header to GUI server",
                      global_settings->tr_or_partition);
}

 void htonll(long long in, unsigned int out[2]) 
 { 
   long *l_long; 
   long *u_long; 
   l_long = (long *)&in; 
   u_long = (long *)((char *)&in + 4); 
   out[0] = htonl(*l_long); 
   out[1] = htonl(*u_long); 
 } 

//To fill opcode, testidx & partition ix in msg
static int fill_opcode_testidx_and_partitionidx(char* send_buf)
{
  unsigned short opcode;
  unsigned int size_to_send = 0;

  NSDL1_PARENT(NULL, NULL, "Method called");

  //Insert opcode 
  opcode = NEW_TEST_RUN;
  memcpy(send_buf, &opcode, UNSIGNED_SHORT);
  send_buf += UNSIGNED_SHORT;
  size_to_send += UNSIGNED_SHORT;

  //Insert testrun number
  //test_run_num = htonl(testidx);
  memcpy(send_buf, &testidx, UNSIGNED_INT);
  send_buf += UNSIGNED_INT;
  size_to_send += UNSIGNED_INT;

  //Insert partition ix
  //htonll(g_partition_idx, partition_num);
  memcpy(send_buf, &g_partition_idx, UNSIGNED_LONG);
  send_buf += UNSIGNED_LONG;
  size_to_send += UNSIGNED_LONG; 

  NSDL2_PARENT(NULL, NULL, "size_to_send = %d", size_to_send);

  NSTL1(NULL, NULL, "Filled opcode '%u', testidx '%d', g_partition_idx '%lld'. Final size to send is '%d'",
                     opcode, testidx, g_partition_idx, size_to_send);

  return size_to_send;
}

//creating test_run msg
static void create_test_run_msg_for_event_log_mgr()
{
  char send_buf[MAX_SEND_BUFF_SIZE + 1];
  char *send_ptr = send_buf;
  unsigned int total_len = 0;
  unsigned int size_to_send = 0;

  NSDL1_PARENT(NULL, NULL, "Method called");
  size_to_send = UNSIGNED_INT;
  size_to_send += fill_opcode_testidx_and_partitionidx(send_ptr + UNSIGNED_INT); // + UNSIGNED_INT so that we can write to next position

  NSDL2_PARENT(NULL, NULL, "size_to_send = %d", size_to_send);

  total_len = (size_to_send - UNSIGNED_INT);

  NSDL2_PARENT(NULL, NULL, "total_len = %d", total_len);

  memcpy(send_ptr, &total_len, UNSIGNED_INT);

  write_msg(&g_el_subproc_msg_com_con, send_ptr, size_to_send, 1, CONTROL_MODE);

  NSTL1(NULL, NULL, "Sent msg of size '%d' to event log msg", size_to_send);
}


/*****************************************************************************************************************************/

static void nde_update_partition_info_next_parition()
{
  partitionInfo.cur_partition = g_partition_idx;
  sprintf(partitionInfo.cur_partition_name, "%lld", g_partition_idx);
  partitionInfo.next_partition = 0;
  partitionInfo.prev_partition = g_prev_partition_idx;

  partitionInfo.absolute_time = 0;   //TODO Krishna need to change
}

/*  This function generates partition name using current time.
 *  partition_idx is a long type variable; and is stored in 'YYYYMMDDHHMM' format.
 *  Cur_Partiton_idx and seconds since epoch are stored in file .curPartition file later;
 *  and are used in partition switching logic if test restarts. */

int nde_set_partition_time(long long *partition_idx)
{       
  long    tloc;
  struct  tm *lt, tm_struct;
  int ret_val;
  char buff[NDE_MAX_NAME_LEN + 1] = {0};
 
  /*  Getting current time  */ 
  (void)time(&tloc);
  lt = nslb_localtime(&tloc, &tm_struct, 1);
   
  g_loc_next_partition_time = (time(NULL)) * 1000;

  if (lt == (struct tm *)NULL)
    ret_val = -1;
  else
  {
    /*  Getting time as formatted string  */
    /*  "%Y%m%d%H%MS" returns "YYYYMMDDHHMMSS" format  */
    if(strftime(buff, 128, "%Y%m%d%H%M%S", lt) == 0)
      ret_val = -1;
    else
    {
      *partition_idx = atoll(buff);
      ret_val = 0;
    }
  }

  //NSTL1(NULL, NULL, "Generated partition name '%s' partition idx '%lld' and returning value '%d'", 
  //                   buff, g_partition_idx, ret_val);
  return(ret_val);
}

// void nde_update_partition_info_partitions()

void nde_init_partition_info()
{
  NSTL1(NULL, NULL, "Init partition info data structure. g_first_partition_idx = %lld, g_start_partition_idx = %lld, g_partition_idx = %lld, g_prev_partition_idx = %lld", g_first_partition_idx, g_start_partition_idx, g_partition_idx, g_prev_partition_idx);

  memset(&partitionInfo, 0, sizeof(PartitionInfo));

  partitionInfo.first_partition = g_first_partition_idx;
  partitionInfo.start_partition = g_start_partition_idx;
  partitionInfo.cur_partition = g_partition_idx;

  sprintf(partitionInfo.cur_partition_name, "%lld", g_partition_idx);

  partitionInfo.next_partition = 0;
  partitionInfo.prev_partition = g_prev_partition_idx;


  partitionInfo.test_run_info_shr_mem_id = test_run_info_shr_mem_key;

  partitionInfo.absolute_time = 0;   //TODO Krishna need to change
  partitionInfo.cav_epoch_diff = global_settings->unix_cav_epoch_diff; 

  partitionInfo.ns_port = parent_port_number;   
  partitionInfo.ns_parent_pid = getpid();  

  partitionInfo.reader_run_mode = global_settings->reader_run_mode;

  partitionInfo.nlm_trace_level = global_settings->nlm_trace_level;
  partitionInfo.nlw_trace_level = global_settings->nlw_trace_level;
  partitionInfo.nlr_trace_level = global_settings->nlr_trace_level;
  partitionInfo.nsdbu_trace_level = global_settings->nsdbu_trace_level;
  
}


/*  This function does following tasks:
 *    1)  Generates new partition name in format 'YYYYMMDDHHMMSS'.
 *    1)  creates new partition directory.
 *    2)  updates g_first_partition_idx and g_prev_partition_idx in case of test start or restart.
 *    3)  writes g_first_partition_idx and g_partition_idx in hidden curPartition file.
 */
int nde_set_partition_idx(long long loc_next_parition_idx)
{
  char new_partition_dir[NDE_MAX_LINE_LEN + 1] = {0};
  static CurPartInfo cur_part_info;

  /*In case of generator, partition generation code will not run: 
   * Because for the (1) first time generator will get its first partition from controller in 'nsu_start_test arguments' 
   * (2) and on partition switch generator will get partition from msg coming from controller with opcode 'NEW_TEST_RUN' */
  if(loader_opcode == CLIENT_LOADER)
  {
    if((g_first_partition_idx != -1) && (g_partition_idx_for_generator >= 0)) //switched partition not first time
    {
      g_prev_partition_idx = g_partition_idx; // Save
      //TODO handle this case for existing partition if required
      g_partition_idx = g_partition_idx_for_generator; //make partition passed from controller in NEW_TEST_RUN msg as generator switched partition
      NSDL2_MESSAGES(NULL, NULL, "Method Called, g_partition_idx = %lld, g_prev_partition_idx = %lld", g_partition_idx, g_prev_partition_idx);
    }
  }
  else
  {
    /*  set g_partition_idx  */
    g_prev_partition_idx = g_partition_idx; // Save

    g_partition_idx = loc_next_parition_idx;
  }

  /*  Updating path of cur partition  */
  sprintf(global_settings->tr_or_partition, "TR%d/%lld", testidx, g_partition_idx);

  /*  Creating partition directory  */
  snprintf(new_partition_dir, sizeof(new_partition_dir), "%s/logs/TR%d/%lld/ns_logs/", g_ns_wdir, testidx, g_partition_idx);
  
  if (mkdir_ex(new_partition_dir) != 1)  //Making directory
  {
    if(errno == ENOSPC)
    {
      NS_EXIT(-1, CAV_ERR_1000008);
    }
    if(errno == EACCES)
    {
      NS_EXIT(-1, CAV_ERR_1000028, g_ns_wdir, testidx, g_partition_idx);
    }
    MKDIR_ERR_MSG("logs/TR/partition/ns_logs", new_partition_dir)
  }

  /*if(mkdir(new_partition_dir, 0775))
  {
    perror("mkdir");
    fprintf(stderr, "#######error creating dir = %s\n", new_partition_dir);
    NSTL1_OUT(NULL, NULL, "Unable to create dir '%s'\n", new_partition_dir);
    NS_EXIT(-1,"Unable to create dir '%s'\n", new_partition_dir);
  }*/
  NSTL1(NULL, NULL, "Creating partition dir = %lld", g_partition_idx);

  /*  If Test is new or is restarted; g_first_partition_idx and g_prev_partition_idx will not be set.
   *  setting g_first_partition_idx and g_prev_partition_idx  */

  // This method called when parition switched? or first time
  if(g_first_partition_idx <= 0)  // Called first time on start/restart of tesst
  {
    /*  if .curPartition file not found, that means fresh test is started.
     *  Hence first_partition will be cur partition and prev partition will be 0.
     *  If file is found, that means test is restarted.
     *  Hence first partition is set to first partition and prev partition is set to curPartition written in file 
     */ 
    g_start_partition_idx = g_partition_idx;
    if(nslb_get_cur_partition_file(&cur_part_info, testidx) < 0) //file or keyword not found
    {
      g_first_partition_idx = g_partition_idx;
      g_prev_partition_idx = 0;
      cur_part_info.first_partition_idx = g_first_partition_idx; //to write in .curPartition file
    }
    else
    {
      g_first_partition_idx = cur_part_info.first_partition_idx;
      g_prev_partition_idx  = cur_part_info.cur_partition_idx;

      //Test is restarted .. hance filling next partition in last partition
      sprintf(new_partition_dir, "%s/logs/TR%d/", g_ns_wdir, testidx);
      nslb_update_next_partition(new_partition_dir, testidx, g_prev_partition_idx, g_partition_idx, partition_info_buf_to_append);
    }
    nde_init_partition_info(); // TODO Krishna Do it in non part also as we may use it in future
    // All set. save in partition dir
    nslb_save_partition_info(testidx, g_partition_idx, &partitionInfo, partition_info_buf_to_append);
    NSTL1(NULL, NULL, "Test has been restarted; testidx = %d", testidx);
  }
  else // Partition switch
  {
    partitionInfo.next_partition = g_partition_idx; // Set next of partition being switched
    nslb_save_partition_info(testidx, g_prev_partition_idx, &partitionInfo, partition_info_buf_to_append);

    nde_update_partition_info_next_parition();
    nslb_save_partition_info(testidx, g_partition_idx, &partitionInfo, partition_info_buf_to_append);
  }


  /*  create/update .curPartition file to save curPartition and firstPartition  */
  cur_part_info.cur_partition_idx = g_partition_idx;
  nslb_save_cur_partition_file(testidx, &cur_part_info);

  return 0;
}

/*  This function calculates switch duration of first partition in seconds.
 *
 *  This function is called just before applying timer.
 *
 *      test_start  partition_creation    setting_timer                           midnight
 *  --------|------------|---------------------|--------------------------------------|-------------  
 *                                             <------------time in sec--------------->
 *  We calculate time in secs between cur time and coming midnight.
 *  Then we take modulus with partition duration given by user.
 *  The first timer is applied for time  we got after modulus.
 *  For example if test was started at 22:29:35 and partition duration is 1 hour.
 *  Assuming, while applying timer, time was 22:29:45;
 *  we get diff between midnight and 22:29:45 ie 5415 secs.
 *  mod = 5415 % 3600(1 hour) = 1815 seconds
 *  Hence first timer will be applied for 1815 secs;
 *  and first switch will occur at 23:00:00 ensuring that partition switch will occur at first midnight.
 *
 *  This methos is called for first switch only;
 *  after first switch periodic timer is set according to duration given by user.
 */
static int nde_calculate_switch_duration()
{

  //duration in seconds
  int switch_duration = 0;
  int duration = global_settings->partition_switch_duration_mins * 60;  //partition duration provided by user
  long    tloc, partition_start_time;
  struct  tm *lt;
  struct  tm newtime;
  long partition_midnight_epoch; 

  /*  Getting current time  */
  (void)time(&tloc);
  lt = localtime_r(&tloc,&newtime);

  if (lt == (struct tm *)NULL)
    switch_duration = -1;
  else
  {
    /*  Getting seconds elapsed since unix epoch */
    partition_start_time = mktime(lt);

    /*  Getting seconds between unix epoch and coming midnight  */
    /*  Setting hour to 24 and min and sec to 0 */
    lt->tm_hour = 24;
    lt->tm_min  = 0;
    lt->tm_sec  = 0;
    partition_midnight_epoch = mktime(lt);

    if(partition_start_time <= 0 || partition_midnight_epoch <= 0)
    {
      NS_EXIT(-1, CAV_ERR_1060006, partition_start_time, partition_midnight_epoch);
    }

    /*  Calculating switch duration for first switch  */
    switch_duration = ((partition_midnight_epoch - partition_start_time) % duration);
    /*  If time diff of cur time and midnight time is multiple of duration  */
    if (switch_duration == 0)
      switch_duration = duration;
  }
  NSTL1(NULL, NULL, "Calculated first switched duration is '%d' seconds", switch_duration);
  return switch_duration;
}

/*  This function returns testidx in case of continuous monitoring;
 *  It reads 'nde.testRunNum=' keyword from work/webapps/sys/config.ini file.
 *  If file or keyword is not found; it returns -1, test should run in NS mode.
 */
int nde_get_testid()
{
    char buffer[NDE_MAX_LINE_LEN + 1] = {0};
    char nde_conf_file_path[NDE_MAX_LINE_LEN + 1] = {0};
    int test_id = 0;

    NSDL1_PARENT(NULL, NULL, "Method Called");
    //if(getenv("NS_WDIR"))
    snprintf(nde_conf_file_path, sizeof(nde_conf_file_path), "%s/webapps/sys/config.ini", g_ns_wdir);
    //else
    //{
    //  NS_EXIT(-1,"Cannot Find 'NS_WDIR'");
    //}
   
    /*  Getting value of keyword in buffer  */ 
    if(get_keyword_value_from_file(nde_conf_file_path, "nde.testRunNum", buffer, sizeof(buffer)) < 0)
      test_id = -1;    //if file or keyword not found; test shuold run in NS mode
    else
      test_id = atol(buffer);

    if(test_id < 0)
    {
      NSTL1_OUT(NULL, NULL, "Error in getting test run num from config.ini, "
                        "Error is '%s'\n", buffer);
    }

    return(test_id); 
}

/* This function creates a hidden file in TR/partition directory named .partition_info.txt which has four rows :
 * 1) First row has first partition number of the monitoring test started.
 * 2) Second row has partition number of the previous partition (Last partition number before it swichted). 
 * 3) Third row has the partition number of the next switched partition. 
 * */
void nde_write_hidden_partition_info_file ()  
{
  char file_buf[NDE_MAX_LINE_LEN + 1] = {0};
  FILE *partition_info_fp = NULL;

  NSDL1_PARENT(NULL, NULL, "Method Called. g_prev_partition_idx = %lld, g_partition_idx = %lld, testidx = %d", 
                            g_prev_partition_idx, g_partition_idx, testidx);

  //Updating two fields(first partition, Previous partition) in current partition's .partition_info.txt file 
  sprintf(file_buf, "%s/logs/TR%d/%lld/.partition_info.txt", g_ns_wdir, testidx, g_partition_idx);
  partition_info_fp = fopen(file_buf, "w");
  if(partition_info_fp == NULL)
  {
    NS_EXIT(-1, CAV_ERR_1060007, file_buf);
  }
  
  fprintf(partition_info_fp, "#First Partition, Previous Partition, Next Partition\n%lld,%lld", 
                                    g_first_partition_idx, g_prev_partition_idx);
  NSTL1(NULL, NULL, "Start Partition = %lld, Prev Partition = %lld, written at path = %s", 
                                    g_first_partition_idx, g_prev_partition_idx, file_buf);
  CLOSE_FP(partition_info_fp);

  /*  Returning in case of the first partition as it will not have the previous partition */
  if(!g_prev_partition_idx) 
    return;

  //Appending the next TR field of the last TR
  sprintf(file_buf, "%s/logs/TR%d/%lld/.partition_info.txt", g_ns_wdir, testidx, g_prev_partition_idx);
  partition_info_fp = fopen(file_buf, "a");
  if(partition_info_fp == NULL)
  {
    NS_EXIT(-1, CAV_ERR_1060007, file_buf);
  }
  fprintf(partition_info_fp, ",%lld", g_partition_idx);

  NSTL1(NULL, NULL, "Next Partition = %lld appended in file - %s", g_partition_idx, file_buf);
  
  CLOSE_FP(partition_info_fp);
}

/*****************************************************************************************************************************/

void make_partition_info_buf_to_append()
{
  char buf[2048] = {0};
  if(!(partition_info_buf_to_append))
    partition_info_buf_to_append=(char*)malloc(4096);
  
  memset(partition_info_buf_to_append,'\0',4096);

  if(global_settings->multidisk_rawdata_path)
  {
    sprintf(buf, "raw_data=%s\n",global_settings->multidisk_rawdata_path);
    strcat(partition_info_buf_to_append, buf);
  }
  if(global_settings->multidisk_nslogs_path)
  {
    sprintf(buf, "nslogs=%s\n",global_settings->multidisk_nslogs_path);
    strcat(partition_info_buf_to_append, buf);
  }
  if(global_settings->multidisk_ndlogs_path)
  {
    sprintf(buf, "ndlogs=%s\n",global_settings->multidisk_ndlogs_path);
    strcat(partition_info_buf_to_append, buf);
  } 
}


void save_status_of_partition(char *partition_status)
{
  char partition_status_file[MAX_LENGTH];
  NSDL1_MON(NULL, NULL, "Method called");
  FILE *status_fp = NULL;  //fp of test run file

  sprintf(partition_status_file, "%s/logs/TR%d/%lld/partition.status", g_ns_wdir, testidx, g_partition_idx);
  status_fp = fopen(partition_status_file, "a"); 
  if (status_fp == NULL)
  {
      print_core_events((char*)__FUNCTION__, __FILE__, 
                        "Error in opening file '%s', Error = %s",
                         partition_status_file, nslb_strerror(errno));
      NS_EXIT(-1, CAV_ERR_1060008, partition_status_file, nslb_strerror(errno));
  }
  fprintf(status_fp, "%s\n", partition_status);
  NSTL1(NULL, NULL, "Saved partition status '%s' in file '%s'", partition_status, partition_status_file);
  CLOSE_FP(status_fp);
}

static void create_script_soft_link()
{
  char buf[2*1024] = {0};
 
  //make script directory soft link in switched partition
  //sprintf(buf, "ln -s %s/logs/TR%d/%lld/scripts %s/logs/TR%d/%lld/scripts > /dev/null 2>&1", g_ns_wdir, testidx, g_prev_partition_idx, g_ns_wdir, testidx, g_partition_idx);
  sprintf(buf, "ln -s %s/logs/TR%d/%lld/scripts %s/logs/TR%d/%lld/scripts > /dev/null 2>&1", g_ns_wdir, testidx, g_start_partition_idx, g_ns_wdir, testidx, g_partition_idx);
  system(buf);
}

static void my_handler_ignore ()
{
}

static void create_partion_directory_in_gen_dir()
{
  int idx;
  char cmd[2048];
  NSDL1_PARENT(NULL, NULL, "Method called");
  char err_msg[1024] = "\0";
 
  sighandler_t prev_handler;
  prev_handler = signal(SIGCHLD, my_handler_ignore);

  for(idx = 0; idx < global_settings->num_process; idx++)
  {
    sprintf(cmd, "mkdir -p %s/logs/TR%d/NetCloud/%s/TR%d/%lld", g_ns_wdir, testidx, generator_entry[idx].gen_name,generator_entry[idx].testidx, g_partition_idx);
    NSDL1_PARENT(NULL, NULL, "Going to create path in NetCloud directory. Command is [%s]", cmd);
    if (nslb_system(cmd,1,err_msg) != 0)
    {
      NSDL2_MESSAGES(NULL, NULL, "Unable to create partition  directory in NetCloud directory");
      NS_EXIT(0, CAV_ERR_1060009);
    }
  }
  (void) signal(SIGCHLD, prev_handler);
}

static void close_all_gen_rtg_file()
{
  int idx;
  NSDL1_PARENT(NULL, NULL, "Method called");
  NSTL1(NULL, NULL, "Method called");

  for(idx = 0; idx < global_settings->num_process; idx++)
  {
      CLOSE_FP(generator_entry[idx].rtg_fp);
  }
}

static void open_all_gen_rtg_file_in_new_partition()
{
  int idx;
  char buf[2048];
  NSDL1_PARENT(NULL, NULL, "Method called");

  for(idx = 0; idx < global_settings->num_process; idx++)
  {
    sprintf(buf, "%s/logs/TR%d/NetCloud/%s/TR%d/%lld/rtgMessage.dat", g_ns_wdir, testidx, generator_entry[idx].gen_name,generator_entry[idx].testidx, g_partition_idx);
    generator_entry[idx].rtg_fp = fopen(buf, "w");
    if(!generator_entry[idx].rtg_fp)
    {
      NSDL1_PARENT(NULL, NULL, "Error in opening rtgMessage file pointer for generator idx = %d, File = %s", idx, buf);
      NS_EXIT(0, CAV_ERR_1060010,idx, buf, nslb_strerror(errno));
    }
  }
}

static void copy_files_from_previous_partition_to_new_partition ()
{
  int idx;
  char old_file_path[2048];
  char new_file_path[2048];
  NSDL1_PARENT(NULL, NULL, "Method called");

  for(idx = 0; idx < global_settings->num_process; idx++)
  {
    sprintf(old_file_path, "%s/logs/TR%d/NetCloud/%s/TR%d/%lld/testrun.gdf", g_ns_wdir, testidx, generator_entry[idx].gen_name,generator_entry[idx].testidx, g_prev_partition_idx);
    sprintf(new_file_path, "%s/logs/TR%d/NetCloud/%s/TR%d/%lld/testrun.gdf", g_ns_wdir, testidx, generator_entry[idx].gen_name,generator_entry[idx].testidx, g_partition_idx);
    NSDL1_PARENT(NULL, NULL, "Old path = %s, New path = %s", old_file_path, new_file_path);
    int ret = link(old_file_path, new_file_path);
    if(ret != 0)
    {
      //NSTL1(NULL, NULL, "Error in creating testrun.gdf file in new partiton. Error = %s\n", nslb_strerror(errno));
      //NS_EXIT(-1,"Error in creating testrun.gdf file in new partiton. Error = %s\n", nslb_strerror(errno));
      if((symlink(old_file_path, new_file_path)) == -1)
      {
        NS_EXIT(-1, CAV_ERR_1060011, nslb_strerror(errno));
      }
    }
  }
}

/*  This function sends partition switch msg to all custom monitors
 *  Msg format is 'cm_partition_switch:MON_PARTITION_IDX=%lld\n'
 */
static void send_partition_switch_to_custom_monitors()
{
  NSDL2_PARENT(NULL, NULL, "Method Called");
  int mon_id, msg_len;
  char SendMsg[1024 + 1] = "";
  CM_info *cm_ptr = NULL;

  msg_len = sprintf(SendMsg, "cm_partition_switch:MON_PARTITION_IDX=%lld\n", g_partition_idx);
  NSTL2(NULL, NULL, "send_partition_switch_to_custom_monitors(), SendMsg = %s, msg_len = %d", SendMsg, msg_len);

  //traverse all monitors
  for(mon_id = 0; mon_id < total_monitor_list_entries; mon_id++)
  {
    cm_ptr = monitor_list_ptr[mon_id].cm_info_mon_conn_ptr;

    if(cm_ptr->gdf_flag  == NA_GENERIC_DB)
    {
      //Creating file at time of partition switch.
//      CLOSE_FP(cm_ptr->csv_stats_ptr->csv_file_fp);
  //    check_for_csv_file(cm_ptr);
      serial_no = 0;
    }

    if((cm_ptr->fd <= 0) || (cm_ptr->conn_state == CM_CONNECTING))
    {
       NSTL2(NULL, NULL, "Invalid fd for monitor_name = %s, hence not sending partition switch message", cm_ptr->monitor_name);
       continue;
    }
    if(cm_ptr->is_monitor_registered == MONITOR_REGISTRATION)
      continue;
   

    //partial buf is not empty then append msg to it
    if((cm_ptr->partial_buf != NULL) && (cm_ptr->partial_buf)[0] != '\0')
    {
      NSTL1(NULL, NULL, "Partial buffer is not empty, hence appending partition switch msg to it");
      if(cm_ptr->partial_buf_len - cm_ptr->bytes_remaining < msg_len)
      {
        NSTL1(NULL, NULL, "Partial buf [%s]", cm_ptr->partial_buf);
        NSTL1(NULL, NULL, "Realloc partial buf size from %d to %d", cm_ptr->partial_buf_len, cm_ptr->partial_buf_len + msg_len);
        MY_REALLOC(cm_ptr->partial_buf, cm_ptr->partial_buf_len + msg_len, "cus_mon_ptr->partial_buf", -1);
        cm_ptr->partial_buf_len = cm_ptr->partial_buf_len + msg_len;
      }
      strcat(cm_ptr->partial_buf, SendMsg);
      cm_ptr->bytes_remaining =  cm_ptr->bytes_remaining + msg_len;
      NSTL1(NULL, NULL, "Now partial_buf is %s", cm_ptr->partial_buf);
      //continue as partial buf will be sent when epoll event is generated from wait_forever.c
      continue;
    }
    //send msg if partial buf was empty
    cm_send_msg(cm_ptr, SendMsg, 1); //including '\0' character
    NSTL2(NULL, NULL, "Sent msg %s to cmon", SendMsg);
  
    cm_ptr->no_log_OR_vector_format |= LOG_ONCE;
    cm_ptr->no_log_OR_vector_format &= ~NO_LOGGING;
  }

  NSTL2(NULL, NULL, "Method Exited");
}

/***************************************************************************
 * Description       : This function does following task:
 *                     a) Save current partition status 
 *                     b) Create new partition index and directory 
 *                     c) Close and open NS files for new partition
 *                     d) Send signal to different processes 
 *                     e) Add periodic timer for next partition with given
 *                        timeout    
 * Input Parameters  : 
 * Output Parameters : None
 * Return            : None 
 ***************************************************************************/
int read_db_monitorinfo_file(FILE *fp)
{
  HiddenFileInfo *hidden_local_ptr = NULL;
  int num_fields;
  char *fields[4];
  char *buf;
  int row_no;
  char line[MAX_MONITOR_BUFFER_SIZE+1];
  char gdf_name[128];
  char file_name[128];
  while (fgets(line, MAX_MONITOR_BUFFER_SIZE, fp) != NULL)
  {
    line[strlen(line) - 1] = '\0';
    buf = line;

    if((buf[0] == '#') || (buf[0] == '\0'))
       continue;
    num_fields =  get_tokens(buf, fields, "|", 4);
    if( num_fields != 4)
      continue;
    strcpy(gdf_name, fields[3]);
    strcpy(file_name,fields[2]);
    if(create_table_entry(&row_no, &total_hidden_file_entries, &max_hiddlen_file_entries, (char **)&hidden_file_ptr, sizeof(HiddenFileInfo), "Allocating Hidden File Table") == -1)
    {
       NSTL1(NULL, NULL, "Could not create hidden file table");
      return -1;
    }
    hidden_local_ptr = &(hidden_file_ptr[row_no]);
    memset(hidden_local_ptr, 0, sizeof(HiddenFileInfo));
  //TODO directly use fields arrays
     MALLOC_AND_COPY (file_name, hidden_local_ptr->hidden_file_name, strlen(file_name) + 1, "Hidden name file for Hidden Table", -1);              MALLOC_AND_COPY (gdf_name, hidden_local_ptr->gdf_name, strlen(gdf_name) + 1, "GDF name for Hidden file Table", -1);
     if(!strncasecmp(gdf_name,"NA_genericDB_postgres",20))  
         hidden_local_ptr->gdf_flag = NA_GENERICDB_POSTGRES;
      else if(!strncasecmp(gdf_name,"NA_genericDB_mysql",18))
         hidden_local_ptr->gdf_flag = NA_GENERICDB_MYSQL;
      else if(!strncasecmp(gdf_name,"NA_SR",5))
         hidden_local_ptr->gdf_flag = NA_SR;
      else if(!strncasecmp(gdf_name,"NA_genericDB_oracle",19))
         hidden_local_ptr->gdf_flag = NA_GENERICDB_ORACLE;
      else if(!strncasecmp(gdf_name,"NA_genericDB_mssql",18))
         hidden_local_ptr->gdf_flag = NA_GENERICDB_MSSQL;
      else if(!strncasecmp(gdf_name,"NA_genericDB_mongoDb",20))
         hidden_local_ptr->gdf_flag = NA_GENERICDB_MONGO_DB;
 }    
  return 0;
}

int read_and_init_hidden_file_struct()
{
  char buf[1024]="";
  char err_msg[2*1024];
  FILE *fp = NULL;

 sprintf(buf,"%s/etc/dbconf/ns_db_monitorsInfo.conf",g_ns_wdir);
 if((fp = fopen(buf, "r")) == NULL)
 {
   sprintf(err_msg, "Failed to open the ns_db_monitorsInfo file = %s. Error: %s.\n", buf, nslb_strerror(errno));
   return -1;
 }

 if (read_db_monitorinfo_file(fp) < 0 );
 return -1;

 CLOSE_FP(fp);

return 0;
}


void make_new_partition_dir_and_send_msg_to_proc(ClientData cd, u_ns_ts_t now)
{
  //char cmd_buf[1024];
  char buf[2*1024];
  char partition_duration_in_hhmmss[64];
  /* We need to set periodic timer with time interval given by user, 
   * hence will set flag once leaving the function */
  rtg_msg_seq = 0; //Reset rtg message seq for new TR
  NSDL2_MESSAGES(NULL, NULL, "Method Called.");

  /*long long loc_next_parition_idx;

  NSTL1(NULL, NULL, "Started Partition switching. Partition '%lld'.", g_partition_idx);

  nde_set_partition_time(&loc_next_parition_idx);

  if(loc_next_parition_idx == g_partition_idx)
  {
    NSTL1(NULL, NULL, "Partition already exists. Partition = '%lld'.Using existing partition", loc_next_parition_idx);
    return;
  }*/
  
  
  //Saving the previous partition num as we need it to append the next partition num field in previous partition's .partition_info.txt file.

  /* In finish report NS updates test duration time wrt to cur time, in case of monitoring
   * finish report will be received by last test, therefore duration field remains 0 for rest TRs
   * We need to update summary.top with test duration which help in viewing test in different time intervals
   * Therefore updating duration field wrt diff of current time and test start time. 
   * */

  if(global_settings->sql_report_mon == 1)
    write_lps_partition_done_file(0);

  /*BUG 84397*/
  /*if(is_mssql_monitor_applied)
    write_lps_partition_done_file_for_mssql(0);*/

  global_settings->partition_duration = now - partition_start_time;
  sprintf(partition_duration_in_hhmmss, "%s", (char *)get_time_in_hhmmss((int)(global_settings->partition_duration/1000)));

  NSTL1(NULL, NULL, "Updating testrun dir TR%d/summary.top with duration '%s'", testidx, partition_duration_in_hhmmss);
  update_summary_top_field(14, partition_duration_in_hhmmss, 0); 

  sprintf(partition_duration_in_hhmmss, "%s", (char *)get_time_in_hhmmss((int)(global_settings->partition_duration/1000)));
  NSTL1(NULL, NULL, "Updating partition %s/summary.top with duration '%s'", global_settings->tr_or_partition, partition_duration_in_hhmmss);
  update_summary_top_field(14, partition_duration_in_hhmmss, 1);  

  NSTL1(NULL, NULL, "Updated TR%d/summary.top and %s/summary.top with duration '%s'", testidx, global_settings->tr_or_partition, partition_duration_in_hhmmss);

  //Save status of current test run
  save_status_of_partition(PARTITION_SWITCHED_SUCCESSFULLY);

  //Test start time 
  //Set start time for new Parition
  partition_start_time = get_ms_stamp();
  init_test_start_time();   

  NSTL1(NULL, NULL, "New Partition start time stamp %ld, (global_settings->test_start_time) = %ld, Start Date/Time (g_test_start_time) = %s", partition_start_time, global_settings->test_start_time, g_test_start_time);
 
  //This function generates new partition name and creates partition dir
  nde_set_partition_idx(g_loc_next_partition_idx); 
  NSTL1(NULL, NULL, "New partition name is '%s'", global_settings->tr_or_partition);
   
  if(global_settings->multidisk_nslogs_path)
    create_links_for_logs(0);

  make_partition_info_buf_to_append(); 

  if(loader_opcode == MASTER_LOADER)
  {
    if (!(g_tsdb_configuration_flag & TSDB_MODE))
    {
      close_all_gen_rtg_file();
    }

    create_partion_directory_in_gen_dir();
    if (!(g_tsdb_configuration_flag & TSDB_MODE))
    {
      open_all_gen_rtg_file_in_new_partition();
    } 
    copy_files_from_previous_partition_to_new_partition();
  }

  //make script soft link in new partition from prev partition
  create_script_soft_link();
  
  //Closing file pointer of old TR and creating files in new testrun
  creating_ns_files_for_new_partition();  

  sprintf(buf, "cp %s logs/%s 2>/dev/null", g_conf_file, global_settings->tr_or_partition);
  if (system(buf) != 0)
  {
    NSTL1_OUT(NULL, NULL, "Error in Copying scenario file to TR/partition");
    //NS_EXIT(-1,"Default Message");
  }

  sprintf(buf, "cp %s logs/%s 2>/dev/null", DEFAULT_DATA_FILE, global_settings->tr_or_partition);
  if (system(buf) != 0)
  {
    NSTL1_OUT(NULL, NULL, "Error in Copying VendorData.default file to TR/partition during Partition switch");
    //NS_EXIT(-1,"Default Message");
  }

  //creating csv of oracle sql stat monitor

   //creating csv of oracle sql stat monitor
  if(global_settings->sql_report_mon)
    create_oracle_sql_stats_csv();


  create_alerts_stats_csv();
  //send partition switch msg to all custom monitors
  if(total_monitor_list_entries) //at least one custom mon present
    send_partition_switch_to_custom_monitors();


  sprintf(buf, "logs/TR%d/%lld/version", testidx, g_start_partition_idx);
  char linkbuf[600];
  sprintf(linkbuf, "logs/%s/version", global_settings->tr_or_partition);
  //get version of all components excluding HPD using nsu_get_version
  //save to file 'version' in TR or partition
  if(link(buf, linkbuf) != 0)
  {
    NSTL1_OUT(NULL, NULL, "Error in creating hard link of version file %s on %s, err = %s", buf, linkbuf, nslb_strerror(errno));
    save_version();
  }

  /*  create/update .partition_info.txt file;
   *  This file contains prev, start and next partition idx.    */
  //nde_write_hidden_partition_info_file();    //TODO Remove this method

  //Send msg to event log manager
  if((loader_opcode != CLIENT_LOADER) && (global_settings->enable_event_logger))
  {
    NSTL1(NULL, NULL, "Sending msg to event log manager, g_partition_idx = %lld, testidx = %d", g_partition_idx, testidx);
    create_test_run_msg_for_event_log_mgr();
  }
  //Send msg to NVM of new TR
  NSTL1(NULL, NULL, "Sending msg to NVM, opcode = %d, testidx = %d, g_partition_idx = %lld", NEW_TEST_RUN, testidx, g_partition_idx);
  send_msg_to_nvm(NEW_TEST_RUN, testidx);

  //Fill shared memory int ptr for new partition idx then send signal to 

  testruninfo_table_shr->partition_idx = g_partition_idx;
  sprintf(testruninfo_table_shr->partition_name, "%lld", testruninfo_table_shr->partition_idx);
  testruninfo_table_shr->absolute_time = time(NULL) - global_settings->unix_cav_epoch_diff;
  
  //Send signal to wirter for updating aprtition number
  SEND_SIGNAL_TO_WRITER

  NSTL1(NULL, NULL, "Updated shared memory with values: testidx = %d, g_partition_idx = %lld, absolute_time = %ld" 
                    " and sent signal to logging writer on fd '%d'", testidx, g_partition_idx, 
                      testruninfo_table_shr->absolute_time, writer_pid);

  NSTL1(NULL, NULL, "Going to send partition switch message to NDC. partition idx '%lld'", g_partition_idx); 
  send_partition_switching_msg_to_ndc();

  NSTL1(NULL, NULL, "Partition switched successfully from '%lld' to '%lld'.", g_prev_partition_idx, g_partition_idx);
}
 
void check_if_partition_switch_to_be_done(ClientData cd, u_ns_ts_t now)
{
  if (((rtg_pkt_ts + global_settings->progress_secs) >= g_loc_next_partition_time ) || (g_tsdb_configuration_flag & TSDB_MODE))
  {
    NSDL2_MESSAGES(NULL, NULL, "Going to change partition Directory. Partition Idx: %lld, Partition for Generator: %lld", g_partition_idx, g_partition_idx_for_generator);
    if(loader_opcode != CLIENT_LOADER)
    {
      if(global_settings->sync_on_first_switch == 1)
	global_settings->time_ptr->actual_timeout = nde_calculate_switch_duration() * 1000;  //convert to milisecs 
      else
          global_settings->time_ptr->actual_timeout = global_settings->partition_switch_duration_mins * 60 * 1000;  //convert to milisecs
      }
      g_set_check_var_to_switch_partition = 0;
      make_new_partition_dir_and_send_msg_to_proc(cd ,now);
    }
    else
    {
   /*if(delay > partition)
   {
     NSTL1(NULL, NULL, Partition %lld skipped due to delay , g_loc_next_partition_idx);
   }*/
     g_client_data = cd;
     NSDL2_MESSAGES(NULL, NULL, "setting g_set_check_var_to_switch_partition");
     g_set_check_var_to_switch_partition = 1;
   }
}

// set next partition idx& call function to check if partition switch to happen
void set_idx_for_partition_switch(ClientData cd, u_ns_ts_t now)
{
  long long loc_next_partition_idx;

  NSTL1(NULL, NULL, "Started Partition switching. Partition '%lld'.", g_partition_idx);

  nde_set_partition_time(&loc_next_partition_idx);
  g_loc_next_partition_idx = loc_next_partition_idx;

  if(g_loc_next_partition_idx == g_partition_idx)
  {
    NSTL1(NULL, NULL, "Partition already exists. Partition = '%lld'.Using existing partition", g_loc_next_partition_idx);
    return;
  }
  check_if_partition_switch_to_be_done(cd, now);
}


void set_nia_file_aggregator_pid(int nia_fa_id)
{
  testruninfo_table_shr->nia_file_aggregator_id = nia_fa_id;
}

void set_test_run_info_writer_pid(int writer_pid)
{
  testruninfo_table_shr->ns_lw_pid = writer_pid;
}

void set_test_run_info_status(int test_run_status)
{
  testruninfo_table_shr->test_status = test_run_status;
}

void set_test_run_info_big_buff_shm_id(int big_buf_shmid)
{
  testruninfo_table_shr->big_buff_shm_id = big_buf_shmid;
}

void set_test_run_info_event_logger_port(int event_logger_port)
{
  testruninfo_table_shr->ns_log_mgr_port = event_logger_port;
}

/***************************************************************************
 * Description       : This function is called from start_test (ns_parent.c) 
 *                     does following task:
 *                     a) Add nonperiodic timer for first partition with 
 *                        calculated timeout    
 * Input Parameters  : 
 *   ClientData & ip : To fill timer function.
 * start_test_min_dur: Value of minute at time of start test                    
 * Output Parameters : None
 * Return            : None 
 ***************************************************************************/

void apply_timer_for_new_tr(ClientData cd, char *ip, int start_test_min_dur)
{
  //Calling shm mem trace log here because of keyword parsing 
  NSTL1(NULL, NULL, "Created shared memory test_run_number = %d, g_partition_index = %lld, absolute_time = %d, " 
                           "testruninfo_table_shr->partition_name = %s, testruninfo_table_shr->cav_epoch_diff = %ld", 
                            testruninfo_table_shr->tr_number, 
                            testruninfo_table_shr->partition_idx, testruninfo_table_shr->absolute_time,  
                            testruninfo_table_shr->partition_name, testruninfo_table_shr->cav_epoch_diff); 
  //cd.p = ip;
  // This must be called if oracle AWR monitors are enabled and ONLY once on start of the test
  if(global_settings->sql_report_mon)
    make_orl_process_done_file_on_start_test();

  if(is_mssql_monitor_applied)
    make_mssql_process_done_file_on_start_test();

  NSDL1_PARENT(NULL, NULL, "Method called, global_settings->continuous_monitoring_mode = %d, minutes for start test = %d", 
                  global_settings->continuous_monitoring_mode, start_test_min_dur);

  /*  If sync_on_first_switch is 1; we need to sync first partition duration such that a partition must switch at first midnight
   *  Otherwise, first partition will switch after given duration irrespective of midnight time  */
  /*  Calculating timeout for first switch and converting to miliseconds  */
  if(global_settings->sync_on_first_switch == 1)
    global_settings->time_ptr->actual_timeout = nde_calculate_switch_duration() * 1000;  //convert to milisecs 
  else
    global_settings->time_ptr->actual_timeout = global_settings->partition_switch_duration_mins * 60 * 1000;  //convert to milisecs


  g_next_partition_switch_time_stamp =  get_ms_stamp() + global_settings->time_ptr->actual_timeout; 
  /*  Adding timer  */
  //dis_timer_add(AB_PARENT_NEW_TEST_RUN_TIMEOUT, global_settings->time_ptr,
  //                    get_ms_stamp(), set_idx_for_partition_switch, cd, 0);


  NSTL1(NULL, NULL, "Applied nonperiodic timer with timeout '%d' in miliseconds", global_settings->time_ptr->actual_timeout);
}

/***************************************************************************
 * Description       : This function is used to create share memory between 
 *                     NS parent and logging writer. Update test_run_number
 *                     with current testidx.
 *                     Function is called from initialize_logging_memory()
 *                     (logging.c file)
 * Input Parameters  : None
 * Output Parameters : None
 * Return            : In case of success returns identifier of the shared 
 *                     memory segment.
 *                     In case of failure returns -1 
 ***************************************************************************/
int create_test_run_info_shm()
{
  int share_mem_fd; 
  NSDL1_PARENT(NULL, NULL, "Method called");

  if ((share_mem_fd = shmget(shm_base , sizeof(TestRunInfoTable_Shr) * 1, IPC_CREAT | IPC_EXCL | 0666)) == -1) { 
    perror("error in allocating shared memory for NS parent and logging writer");
    check_shared_mem(sizeof(TestRunInfoTable_Shr));
    return -1;
  }
  testruninfo_table_shr = shmat(share_mem_fd, NULL, 0);
  if (testruninfo_table_shr == NULL) {
    perror("error in getting shared memory for NS parent and logging writer");
    return -1;
  }

  if (shmctl(share_mem_fd, IPC_RMID, NULL)) {
    printf ("ERROR: unable to mark shm removal for 'NS parent and logging writer' of size=%lu err=%s\n",
                (u_ns_ptr_t)sizeof(int), nslb_strerror(errno));
    return -1;
  }
  return share_mem_fd; //success case
}

void update_test_run_info_shm()
{
  NSDL1_PARENT(NULL, NULL, "Method called");

  testruninfo_table_shr->partition_idx = g_partition_idx;  
  sprintf(testruninfo_table_shr->partition_name, "%lld", testruninfo_table_shr->partition_idx);

  testruninfo_table_shr->absolute_time = time(NULL) - global_settings->unix_cav_epoch_diff;  // TODO

  // Following are set once during the life time of test
  testruninfo_table_shr->tr_number = testidx;  
  testruninfo_table_shr->cav_epoch_diff = global_settings->unix_cav_epoch_diff;
  testruninfo_table_shr->ns_parent_pid = getpid();
  testruninfo_table_shr->num_nvm = global_settings->num_process; 
  testruninfo_table_shr->reader_run_mode = global_settings->reader_run_mode; 
  testruninfo_table_shr->ns_port = parent_port_number;
  testruninfo_table_shr->nlm_trace_level = global_settings->nlm_trace_level; 
  testruninfo_table_shr->nlr_trace_level = global_settings->nlr_trace_level; 
  testruninfo_table_shr->nlw_trace_level = global_settings->nlw_trace_level; 
  testruninfo_table_shr->nsdbu_trace_level = global_settings->nsdbu_trace_level; 
  testruninfo_table_shr->nifa_trace_level = global_settings->nifa_trace_level; 
  testruninfo_table_shr->ns_db_upload_chunk_size = global_settings->ns_db_upload_chunk_size; 
  testruninfo_table_shr->nd_db_upload_chunk_size = global_settings->nd_db_upload_chunk_size; 
  testruninfo_table_shr->db_upload_idle_time_in_secs = global_settings->db_upload_idle_time_in_secs; 
  testruninfo_table_shr->db_upload_num_cycles = global_settings->db_upload_num_cycles; 
  strcpy(testruninfo_table_shr->nsdbuTmpFilePath, global_settings->NSDBTmpFilePath);
  strcpy(testruninfo_table_shr->nddbuTmpFilePath, global_settings->NDDBTmpFilePath);
  testruninfo_table_shr->loader_opcode = loader_opcode; 

  set_test_run_info_status(TEST_RUN_INIT);

}

void nde_set_parent_port_number(int port)
{
  testruninfo_table_shr->ns_port = port;
  partitionInfo.ns_port = port;   
  // Since First time port is not set in info file we are saving again
  //nslb_save_partition_info(testidx, g_partition_idx, &partitionInfo, partition_info_buf_to_append);  //saving port in partition_info
}


/*************************************************************************************************************
 * Description        : kw_enable_monitor_report() method used to parse ENABLE_MONITOR_REPORT keyword,
 * Format             : ENABLE_MONITOR_REPORT <enable/disable>
 * Input Parameters
 *           buf      : Providing entire buffer(including keyword).
 * Output Parameters  : Set monitor_report_mode in struct GlobalData
 * Return             : Retuns 0 for success and exit if fails.
 **************************************************************************************************************/

int kw_enable_monitor_report(char *buf)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH]; //This used to check if some extra field is given
  char err_msg[MAX_AUTO_MON_BUF_SIZE];
  int num, mode;
  char monitor_report[MAX_DATA_LINE_LENGTH];
  //Fill default value  
  strcpy(monitor_report, "2");
  mode = 2;

  NSDL1_PARSING(NULL, NULL, "Method called. buf = %s", buf);

  num = sscanf(buf, "%s %s %s", keyword, monitor_report, tmp); // This is used to check number of arguments

  //Validate number of arguents
  if (num != 2) 
  {
    NS_KW_PARSING_ERR(keyword, 0, err_msg, ENABLE_MONITOR_REPORT_USAGE, CAV_ERR_1060012); 
  }

  //Validate monitor report option
  if (ns_is_numeric(monitor_report) == 0)
  {
    NS_KW_PARSING_ERR(keyword, 0, err_msg, ENABLE_MONITOR_REPORT_USAGE, CAV_ERR_MSG_2); 
  }

  mode = atoi(monitor_report);

  if ((mode < 0) || (mode > 2))
  {
    NS_KW_PARSING_ERR(keyword, 0, err_msg, ENABLE_MONITOR_REPORT_USAGE, CAV_ERR_MSG_3);
  }

  global_settings->monitor_report_mode = mode;

  NSDL2_PARSING(NULL, NULL, "Monitor report option is = %d", global_settings->monitor_report_mode);
  return 0;
}


#if 0 
void init_trans_cumulative_entries()
{
  int i;
  NSDL2_PARENT(NULL, NULL, "Method called. total_tx_entries = %d", total_tx_entries);
  for(i = 0; i < total_tx_entries; i++)
  {
    average_time->txData[i].tx_c_avg_time = 0;  
    average_time->txData[i].tx_c_min_time = 0;  
    average_time->txData[i].tx_c_max_time = 0;  
    average_time->txData[i].tx_c_fetches_started = 0;  
    average_time->txData[i].tx_c_fetches_completed = 0;  
    average_time->txData[i].tx_c_succ_fetches = 0;  
    average_time->txData[i].tx_c_tot_time = 0;  
    average_time->txData[i].tx_c_tot_sqr_time = 0;  
  }
}
#endif

/*  This function finds value of a keyword from given file.
 *  Input: file path, keyword, buffer to store result, size of buffer.
 *  output: value just after keyword.
 *  If a line in file is like keyword=1234;
 *  and if provided keyword to function is keyword=; then function will write 1234 to buffer.
 *  Return values:
 *   0  success
 *  -1  File Not found.
 *  -2  Keyword not found.
 *  -3  Keyword found, but no or non numeric value is associated.
 *  -4  any of char pointer passed to function is NULL of given size is 0.
 *      In case of every error execpt (-4), it writes error message in buffer.
 */

int get_keyword_value_from_file(char *file, char *keyword, char *result, int size)
{
  NSDL1_PARENT(NULL, NULL, "Method Called");
  char buffer[NDE_MAX_LINE_LEN + 1] = {0};
  FILE *fp;
  char keyword_found = 0;
  char *ptr;
  int len;

  /*  Checking if any of char pointer passed to function is NULL of given size is 0 */
  if(!file || !keyword || !result || !size)
  {
    return(-4);
  }
  fp = fopen(file, "r");
  if(!fp)
  {
    snprintf(result, size, "Could not open file '%s'", file);
    return(-1);
  }

  while(nslb_fgets(buffer, NDE_MAX_LINE_LEN, fp, 0))
  {
    //check if keyword present; 
    if(strstr(buffer, keyword) == NULL)
      continue;

    len = strlen(buffer);
    
    //remove newline. newline might be combination of '\r\n' or '\r' or '\n'
    //update len also if it is to be used further
    if(len >= 2 && '\r' == buffer[len - 2] && '\n' == buffer[len - 1])
      buffer[len - 2] = '\0';
    else if(len >= 1 && ('\r' == buffer[len - 1] || '\n' == buffer[len - 1]))
      buffer[len - 1] = '\0';

    //remove spaces from left and right of the string
    nslb_trim(buffer);

    //now match if keyword is present
    if(!strncasecmp(buffer, keyword, strlen(keyword)))
    {
      keyword_found = 1;
      break;
    }
  }
  CLOSE_FP(fp);

  if(!keyword_found)
  {
    snprintf(result, size, "Keyword '%s' not found. line = '%s'", keyword, buffer);
    return(-2);
  }

  //move pointer after keyword
  ptr = buffer + strlen(keyword);

  //remove all spaces before '='
  nslb_ltrim(ptr);

  /*for( ; *ptr && *ptr != '='; ptr++)
  {
    if(*ptr != ' ' && *ptr != '\t')
    {
      strncpy(result, "Invalid keyword found.", size);
      return(-2);
    }
  } */
  
  //if '=' not found or no value found after '='
  if(*ptr != '=' || '\0' == *(++ptr))
  {
    snprintf(result, size, "No value found for keyword %s. line = '%s'", keyword, buffer);
    return(-3);
  }

  //remove all spaces after '='
  nslb_ltrim(ptr);
  
  //validate var for numeric
  if(nslb_validate_var_value(ptr, 2) != 0)
  {
    snprintf(result, size, "Invalid (non numeric) value found. line = %s", buffer);
    return(-3);
  }

  strncpy(result, ptr, size);
  //NSTL1(NULL, NULL, "Found keyword '%s' with value '%s' in file '%s'", keyword, result, file);
  return 0;
}

void copy_prev_trace_in_partition()
{
  char new_path[1024], old_path[1024];

  NSDL2_LOGGING(NULL, NULL, "Method called."); 
  /*Close and reset fd */
  close(ns_event_fd);
  ns_event_fd = -1; 
  /*Move file to partition/ns_logs and fd will be opened by NSTL call*/
  sprintf(new_path, "%s/logs/%s/ns_logs/ns_trace.log", g_ns_wdir, global_settings->tr_or_partition);
  sprintf(old_path, "%s/logs/TR%d/ns_logs/ns_trace.log", g_ns_wdir, testidx);
  rename(old_path, new_path);
}

/*  This function does following tasks.
 *    1.  Filling  tr_or_partition and tr_or_common_files vars according to partition_creation_mode.
 *    2.  Creates partition idx and partition dir.
 *    3.  Fills g_start_parition_idx, g_prev_partition_idx.
 *    4.  Writes in .curPartition file.
 */
void create_partition_dir()
{
  char buf[1024 + 1] = {0};
  /*  In case of continuous monitoring, some files are stored in partition and common_files instead of TR dir */
  //Set common_files here and tr_or_partition just after getting testidx
  MY_MALLOC(global_settings->tr_or_common_files, TR_OR_PARTITION_NAME_LEN + 1, "global_settings->tr_or_common_files", -1);
     
  NSDL2_LOGGING(NULL, NULL, "Creating shared memory for test_run_info.");
  write_log_file(NS_INITIALIZATION, "Creating partition info share memory");
  if((test_run_info_shr_mem_key = create_test_run_info_shm()) == -1){
    NS_EXIT(-1, CAV_ERR_1031010);
  }
  sprintf(shr_mem_new_tr_key, "%d", test_run_info_shr_mem_key);
  NSDL2_LOGGING(NULL, NULL, "test_run_info_shr_mem_key = %d", test_run_info_shr_mem_key);


  /*  Generating partition Id; also creating partition directory  */
  long long loc_next_partition_idx;
  nde_set_partition_time(&loc_next_partition_idx);

  write_log_file(NS_INITIALIZATION, "Saving partition %lld", loc_next_partition_idx);
  nde_set_partition_idx(loc_next_partition_idx);
  
  snprintf(global_settings->tr_or_partition, TR_OR_PARTITION_NAME_LEN, "TR%d/%lld", testidx, g_partition_idx);
  snprintf(global_settings->tr_or_common_files, TR_OR_PARTITION_NAME_LEN, "TR%d/common_files", testidx);

  if (ns_event_fd > 0)
    copy_prev_trace_in_partition();
  /*  Creating common_files directory  */
  sprintf(buf, "logs/%s/", global_settings->tr_or_common_files);
  if (mkdir(buf, 0775) != 0)
  {
    MKDIR_ERR_MSG("common_files", buf)
  }


  //Creating ns_files dir in common_files/TR in case partition creation mode ON/OFF
  sprintf(buf, "%s/logs/%s/ns_files", g_ns_wdir, global_settings->tr_or_common_files);
  // Create ns_file so that we can copy files into it 
  if (mkdir(buf, 0775) != 0) 
  {
    MKDIR_ERR_MSG("logs", buf)
  }

  write_log_file(NS_INITIALIZATION, "Saving build version");
  //bug:52786] save 'version' file inside TR/partition
  save_version();
  update_test_run_info_shm();
}

void create_alerts_stats_csv()
{
  FILE *fp;
  int i;
  char file_path[1024] = "";
  char buf[2*1024] = "";
  
  char *csv_filename[ORACLE_SQL_STATS_CSV_COUNT] = 
  {
    "alertHistory.csv",
    "alertAction.csv"
  };

  sprintf(file_path, "%s/logs/%s/reports/csv", g_ns_wdir, global_settings->tr_or_partition);

  for(i = 0; i < 2; i++)
  {
    sprintf(buf, "%s/%s", file_path, csv_filename[i]);

    fp = fopen(buf, "a"); 
    if(!fp)
    {
      if(errno != EEXIST)
      {
        NSDL1_PARENT(NULL, NULL, "Could not create file %s, Error = %s", buf, nslb_strerror(errno));
        NSTL1_OUT(NULL, NULL, "Could not create file %s, Error = %s\n", buf, nslb_strerror(errno));
      }
    }

      CLOSE_FP(fp);
  }
}

//this function creates csv in case of oracle sql stats monitor
void create_oracle_sql_stats_csv()
{
  FILE *fp;
  int i;
  char file_path[1024] = "";
  char metadata_file_path[1024] = "";
  char buf[4*1024] = "";
  
  char *csv_filename[ORACLE_SQL_STATS_CSV_COUNT] = 
  {
    //do not remove orlStatsSQLIDTable.csv from index 0, as it is used in upcoming loop
    "orlStatsSQLIDTable.csv", 
    "orlStatsSnapTable.csv",
    "orlStatsSQLStmtOrdByElapsedTime.csv",
    "orlStatsSQLStmtOrdByCPUTime.csv",
    "orlStatsSQLStmtOrdByUsrIOTime.csv",
    "orlStatsSQLStmtOrdByGets.csv",
    "orlStatsSQLStmtOrdByReads.csv",
    "orlStatsSQLStmtOrdByPhyReadsUnopt.csv",
    "orlStatsSQLStmtOrdByExec.csv",
    "orlStatsSQLStmtOrdByParseCalls.csv",
    "orlStatsSQLStmtOrdBySharableMemory.csv",
    "orlStatsSQLStmtOrdByVersionCount.csv",
    "orlStatsSQLStmtOrdByClusterWaitTime.csv"
  };

  sprintf(file_path, "%s/logs/%s/reports/csv", g_ns_wdir, global_settings->tr_or_partition);
  //metadata csv needs to be created in common_files or TR
  sprintf(metadata_file_path, "%s/logs/%s/reports/csv", g_ns_wdir, global_settings->tr_or_common_files);

  if(g_partition_idx > 0)
  {
    sprintf(buf, "%s/.oracle_report_partition", file_path);

    fp = fopen(buf, "w"); 
    if(!fp)
    {
      if(errno != EEXIST)
      {
        NSDL1_PARENT(NULL, NULL, "Could not create file %s, Error = %s", buf, nslb_strerror(errno));
        NSTL1_OUT(NULL, NULL, "Could not create file %s, Error = %s\n", buf, nslb_strerror(errno));
      }
    }
      CLOSE_FP(fp);
  }

  for(i = 0; i < ORACLE_SQL_STATS_CSV_COUNT; i++)
  {
    if(i == 0)
      sprintf(buf, "%s/%s", metadata_file_path, csv_filename[i]);
    else
      sprintf(buf, "%s/%s", file_path, csv_filename[i]);

    fp = fopen(buf, "a"); 
    if(!fp)
    {
      if(errno != EEXIST)
      {
        NSDL1_PARENT(NULL, NULL, "Could not create file %s, Error = %s", buf, nslb_strerror(errno));
        NSTL1_OUT(NULL, NULL, "Could not create file %s, Error = %s\n", buf, nslb_strerror(errno));
      }
    }

      CLOSE_FP(fp);
  }
}

void create_oracle_sql_stats_hidden_file()
{
  char buf[1024] = "";
  FILE *fp = NULL;

  sprintf(buf, "%s/logs/%s/.oracle_sql_report", g_ns_wdir, global_settings->tr_or_common_files);

  fp = fopen(buf, "w");
  if(!fp)
  {
    if(errno != EEXIST)
    {
      NSDL1_PARENT(NULL, NULL, "Could not create file %s", buf);
      NSTL1_OUT(NULL, NULL, "Could not create file %s\n", buf);
    }
  }

    CLOSE_FP(fp);
}

void create_mssql_stats_hidden_file()
{
  char buf[1024] = "";
  FILE *fp = NULL;

  //sprintf(buf, "%s/logs/%s/.mssql_report", g_ns_wdir, global_settings->tr_or_common_files);
  /*BUG 84397*/
  sprintf(buf, "%s/logs/%s/.genericDb_report", g_ns_wdir, global_settings->tr_or_common_files);

  fp = fopen(buf, "w");
  if(!fp)
  {
    if(errno != EEXIST)
    {
      NSDL1_PARENT(NULL, NULL, "Could not create file %s", buf);
      NSTL1_OUT(NULL, NULL, "Could not create file %s\n", buf);
    }
  }

    CLOSE_FP(fp);
}


/*-----------------------STARTS:CODE of Creating links to the disk of the logs if MULTIDISK Keyword is enable--------------------------------*/
/*
Maninder Singh
*/

void check_and_rename(char *name, int file_type_and_keyword_status)
{
  char newname_file[2*1024] = {0};
  char oldname_file[1024] = {0};
  struct stat s;
  long    tloc;
  struct  tm *lt;
  struct  tm newtime;
  int ret_val;
  char buff[128] = {0};
  
  ret_val = sprintf(oldname_file, "%s/logs/%s", g_ns_wdir, name);

  if(oldname_file[ret_val-1] == '/')
    oldname_file[ret_val-1] = '\0';

  /*  Getting current time  */
  (void)time(&tloc);
  lt = localtime_r(&tloc,&newtime);

  if (lt == (struct tm *)NULL)
    ret_val = -1;
  else
  {
    /*  Getting time as formatted string  */
    /*  "%Y%m%d%H%MS" returns "YYYYMMDDHHMMSS" format  */
    if(strftime(buff, 128, "%Y%m%d%H%M%S", lt) == 0)
      ret_val = -1;
    else
    {
      sprintf(newname_file, "%s_%s", oldname_file, buff);
      ret_val = 0;
    }
  }
  
  if( (lstat(oldname_file,&s) == 0 ) )
  {
  switch (s.st_mode & S_IFMT) 
  {
    case S_IFDIR:
    case S_IFREG:       
        if(file_type_and_keyword_status == REGULAR_FILE_AND_KEYWORD_ENABLE || file_type_and_keyword_status == DIRECTORY_AND_KEYWORD_ENABLE)  
	{
	   //it's a directory or regular_file
           rename(oldname_file, newname_file);
 	}
 	else if(file_type_and_keyword_status == DEBUG_LOG_FILE_AND_KEYWORD_ENABLE)
        {
	  //it's regular file and debug.log or debug.log.prev
      	    CLOSE_FD(debug_fd);
      	  rename(oldname_file, newname_file);

      	  if (debug_fd <= 0 )
      	  {
              sprintf(oldname_file, "%s/%s", global_settings->multidisk_nslogs_path, name);
              debug_fd = open (oldname_file, O_CREAT|O_WRONLY|O_APPEND|O_CLOEXEC, 00666);
       	      if (debug_fd <= 0)
              {
                NS_EXIT(-1, CAV_ERR_1000006, oldname_file, errno, nslb_strerror(errno));
              }
              write(debug_fd, DEBUG_HEADER, strlen(DEBUG_HEADER));
          }
	}
    break;
    case S_IFLNK:   
        if(file_type_and_keyword_status == DIRECTORY_OR_REGULAR_FILE_AND_KEYWORD_DISABLE)
        {
 	  //it's a link
          rename(oldname_file, newname_file);
 	}
	else if(file_type_and_keyword_status == DEBUG_LOG_FILE_AND_KEYWORD_DISABLE)
	{
 	  //it's a link and debug log file
            CLOSE_FD(debug_fd);
          rename(oldname_file, newname_file);

          if (debug_fd <= 0 )
          {
              debug_fd = open (oldname_file, O_CREAT|O_WRONLY|O_APPEND|O_CLOEXEC, 00666);
              if (debug_fd <= 0)
              {
                NS_EXIT(-1, CAV_ERR_1000006, oldname_file, errno, nslb_strerror(errno));
              }
              write(debug_fd, DEBUG_HEADER, strlen(DEBUG_HEADER));
          }
	}
    break;
  }
  }
}

void create_and_link_file(char *name, int file_type_and_keyword_status)  //value of file_type_and_keyword_status is 0 for file and 1 for dir, 2 is for special case if file or directory already exist and we have to move it first to disk and then create its link
{
  char symlink_file[1024] = {0};
  char original_file[1024] = {0};
  int len;
  len = sprintf(original_file, "%s/%s", global_settings->multidisk_nslogs_path, name);
  sprintf(symlink_file, "%s/logs/%s", g_ns_wdir, name);
  check_and_rename(name, file_type_and_keyword_status);
  if(file_type_and_keyword_status == DIRECTORY_AND_KEYWORD_ENABLE) //checking if it is directory
  {
    //len=strlen(original_file);  //TODO:CHECK THIS GOOGLE
    if(original_file[len-1] != '/')    //Check for /
    {
      original_file[len]='/';
      original_file[len+1]='\0';
    }
    if (mkdir_ex(original_file) != 1)  //Making directory
    {
      if(errno == ENOSPC)
      {
        NS_EXIT(-1, CAV_ERR_1000008);
      }
      if(errno == EACCES)
      {
        NS_EXIT(-1, CAV_ERR_1000029, global_settings->multidisk_nslogs_path, name);
      }
      MKDIR_ERR_MSG(name, original_file)
    }
    len=strlen(original_file);  //TODO:CHECK THIS GOOGLE
    if(original_file[len-1] == '/')    //Check for /
      original_file[len-1]='\0';
    len=strlen(symlink_file);
    if(symlink_file[len-1] == '/')    //Check for /
      symlink_file[len-1]='\0';
  }

  if((symlink(original_file, symlink_file)) < 0)  //creating link
  { 
    if(errno != EEXIST)    
      printf("Could not create symbolic link %s to %s\n", symlink_file, original_file);
  }
}

int create_links_for_logs(int path_flag)  //value of path_flag would be 0 in case we have to create link inside the partition and 1 for the links to be created outside the partition
{
  struct stat st;
  int need_to_dump_header = 0;
  char original_file[1024] = "";

  if(path_flag)//calling for creating all links in TR directory i.e debug log, common_files/ns_logs,  and ns_log directory.
  {
    sprintf(original_file, "TR%d/ns_logs", testidx);
    create_and_link_file(original_file, DIRECTORY_AND_KEYWORD_ENABLE);

    sprintf(original_file, "TR%d/server_logs", testidx);
    create_and_link_file(original_file, DIRECTORY_AND_KEYWORD_ENABLE); 
    
    sprintf(original_file, "TR%d/server_signatures", testidx);
    create_and_link_file(original_file, DIRECTORY_AND_KEYWORD_ENABLE);  
 
    sprintf(original_file, "%s/ns_logs", global_settings->tr_or_common_files);
    create_and_link_file(original_file, DIRECTORY_AND_KEYWORD_ENABLE);

    sprintf(original_file, "TR%d/debug.log", testidx);
    create_and_link_file(original_file, DEBUG_LOG_FILE_AND_KEYWORD_ENABLE);
    
    sprintf(original_file, "TR%d/debug.log.prev", testidx);
    create_and_link_file(original_file, REGULAR_FILE_AND_KEYWORD_ENABLE);

  }
  else// creating links in the partition i.e partitio/ns_logs, partition/monitor.log, partrition/event.log
  {
    sprintf(original_file, "%s/ns_logs", global_settings->tr_or_partition);  //for partition/ns_logs dir
    create_and_link_file(original_file, DIRECTORY_AND_KEYWORD_ENABLE);

    /*Fix bug#24855: where ns_trace.log created in ns_logs_20170301104413 directory instead of partition/ns_logs/ in case of Multidisk.
                     Because before parsing of multidisk keyword some trace logs will be there and this trace logs will be created at 
                     'ns_logs_20170301104413' directory which exist in partition.
                     Now we can close the trace fd and then open again and created trace.log file on partition/ns_logs/ directory.*/
      CLOSE_FD(ns_event_fd);
     
    sprintf(original_file, "%s/logs/%s/ns_logs/ns_trace.log", g_ns_wdir, global_settings->tr_or_partition);

    if(stat(original_file, &st) < 0)
      need_to_dump_header = 1;
      
    ns_event_fd = open (original_file, O_CREAT|O_WRONLY|O_APPEND|O_CLOEXEC, 00666);
    if (ns_event_fd <= 0)
    {
      NS_EXIT(-1, CAV_ERR_1000006, original_file, errno, nslb_strerror(errno));
    }
    if(need_to_dump_header)
      write(ns_event_fd, TRACE_HEADER, strlen(TRACE_HEADER)); 
 
    //Configuring alert with new trace fd
    ns_alert_config();

    //Configuring file upload with new trace fd
    if(global_settings->monitor_type)
    {
      ns_config_file_upload(global_settings->file_upload_info->server_ip, global_settings->file_upload_info->server_port,
                            global_settings->file_upload_info->protocol, global_settings->file_upload_info->url,
                            global_settings->file_upload_info->max_conn_retry, global_settings->file_upload_info->retry_timer, ns_event_fd);
    }

    sprintf(original_file, "%s/server_logs", global_settings->tr_or_partition); //for partition/server_logs
    create_and_link_file(original_file, DIRECTORY_AND_KEYWORD_ENABLE);
    sprintf(original_file, "%s/monitor.log", global_settings->tr_or_partition);//for partition/monitor.log file
    create_and_link_file(original_file, REGULAR_FILE_AND_KEYWORD_ENABLE);
    sprintf(original_file, "%s/event.log", global_settings->tr_or_partition); //for partition/event.log file
    create_and_link_file(original_file, REGULAR_FILE_AND_KEYWORD_ENABLE);
  }
  return 0;
}
/*-------------------------------ENDS: Code of Symbolic links--------------------------------------------------------------------------------*/


/*int check_for_inode(int inode_num)
{
  int i = 0;
  while(inode_arr[i] != '\0')
  {
    if(inode_num == inode_arr[i])
      return 0;
    i++;
  }
  inode_arr[i] = inode_num;
  return 1;
}*/

int check_multidisk_path(int inode, char *multidisk_path, char is_link)
{
  int i=0;
  while(multidisk_path_struct_arr[i].multidisk_path[0] != '\0')
  {
    if((is_link == 1) || (multidisk_path_struct_arr[i].is_link == 1))
    {
      if(multidisk_path_struct_arr[i].inode_num == inode)
        return 0;
    }
    else
    {
      if(!strcmp(multidisk_path_struct_arr[i].multidisk_path, multidisk_path) )
        return 0;
    }
    i++;
  }

  strcpy(multidisk_path_struct_arr[i].multidisk_path, multidisk_path);
  multidisk_path_struct_arr[i].inode_num = inode;

  if(is_link == 1)
    multidisk_path_struct_arr[i].is_link = 1;
  else
    multidisk_path_struct_arr[i].is_link = 0;

  return 1;
}


void create_and_fill_multi_disk_path(char *file_type, char *multidisk_path)
{
  int num_fields;
  char buf[1024] = "";
  char *fields[20];
  FILE *fp = NULL;
  struct stat st;
  int i;

  sprintf(buf, "%s/logs/TR%d/.multidisk_path", g_ns_wdir, testidx);

  if(access(buf, F_OK) !=0)
    NSDL1_PARENT(NULL, NULL, "This Path is not accessible %s", buf);
 
  fp = fopen(buf, "a");
  if(!fp)
  {
    if(errno != EEXIST)
    {
      NSDL1_PARENT(NULL, NULL, "Could not create file %s", buf);
      NSTL1_OUT(NULL, NULL, "Could not create file %s\n", buf);
    }
  }

  num_fields = get_tokens_with_multi_delimiter(multidisk_path, fields, ",", 20);
  for(i = 0; i < num_fields; i++)
  {
    lstat(fields[i], &st);
    if (S_ISLNK(st.st_mode)) 
    {
      stat(fields[i], &st);
      if(check_multidisk_path((int)st.st_ino, fields[i], 1) == 1)
        if(fp)
          fprintf(fp, "%s\n", fields[i]);
    } 
    else
    {
      if(check_multidisk_path((int)st.st_ino, fields[i], 0) == 1)
        if(fp)
	  fprintf(fp, "%s\n", fields[i]);
    }
  }

  CLOSE_FP(fp);
}

