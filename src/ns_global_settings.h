#ifndef NS_GLOBAL_SETTINGS_H 
#define NS_GLOBAL_SETTINGS_H

#include "ns_parse_netcloud_keyword.h"
#include "ns_nethavoc_handler.h"
typedef struct {
  int mode;
  int option;
  int time;
} RampDownMethod;

#define MAX_LPS_SERVER_NAME_SIZE 128
#define MAX_HOST_NAME_SIZE 256
#define FILE_NAME_LENGTH  1024
#define COPY_SRCIPT_DIR_TO_TR 0
#define DO_NOT_COPY_SRCIPT_TO_TR 1
#define COPY_SCRIPT_NOT_SUBDIR_TO_TR 2

#define NC_MAX_RETRY_ON_CONN_FAIL	 5

#define CNTRL_CONN			 0x00000001
#define DATA_CONN			 0x00000010

// For TCPDUMP in case of NC
#define ALWAYS 				 1
#define CONFAIL				 2

#define GS_FLAGS_TEST_IS_DEBUG_TEST      0x0000000000000001  // Test is a debug/smoke test
#define IS_ENABLE_NC_TCPDUMP(state) (((loader_opcode == MASTER_LOADER)?(global_settings->nc_tcpdump_settings->cntrl_mode == state):(global_settings->nc_tcpdump_settings->gen_mode == state))?((ISCALLER_DATA_HANDLER)?global_settings->nc_tcpdump_settings->con_type_mode & DATA_CONN : global_settings->nc_tcpdump_settings->con_type_mode & CNTRL_CONN):0)

#define IS_LAST_TCPDUMP_DUR_ENDED ((g_tcpdump_started_time)?(((get_ms_stamp() - g_tcpdump_started_time)/1000) > global_settings->nc_tcpdump_settings->tcpdump_duration):1)

// njvm settings
typedef struct
{
  int njvm_init_thrd_pool_size;
  int njvm_increment_thrd_pool_size;
  int njvm_max_thrd_pool_size;
  int njvm_thrd_threshold_pct;
  char *njvm_class_path;
  char *njvm_system_class_path;
  int  njvm_min_heap_size;
  int  njvm_max_heap_size;
  char njvm_con_type;
  char njvm_gc_logging_mode;
  char *njvm_custom_config;
  char *njvm_java_home;
  unsigned long njvm_conn_timeout;
  unsigned long njvm_msg_timeout;
  int njvm_simulator_mode;
  char *njvm_simulator_flow_file;
}NjvmSettings;

// URIEncoding table
typedef struct URIEncoding
{
  char encode[3];  // Array indexed by character value with value 0 (not to be encoded) or 1 (to be encode)
} URIEncoding;

typedef struct NCTcpDumpSettings{
  int cntrl_mode;                 //Added for controller_mode for taking TCPDUMP in case of NC
  int gen_mode;                   //Added for generator_mode for taking TCPDUMP in case of NC
  char con_type_mode;              //Added for connection_type for taking TCPDUMP in case of NC
  int tcpdump_duration;           //Added for duration for taking TCPDUMP in case of NC
} NCTcpDumpSettings;

typedef struct AlertInfo
{
  char enable_alert;
  char type;
  char method;
  char protocol;
  char alert_config_mode;
  unsigned short rate_limit;
  unsigned short max_conn_retry;
  unsigned short retry_timer;
  unsigned short tp_init_size;
  unsigned short tp_max_size;
  unsigned short mq_init_size;
  unsigned short mq_max_size;
  unsigned short server_port;
  char server_ip[256 + 1];
  char url[256 + 1];
  char policy[256 + 1];
}AlertInfo;

typedef struct FileUploadInfo
{
  unsigned short max_conn_retry;
  unsigned short retry_timer;
  unsigned short tp_init_size;
  unsigned short tp_max_size;
  unsigned short mq_init_size;
  unsigned short mq_max_size;
  unsigned short server_port;
  char protocol;
  char server_ip[256 + 1];
  char url[256 + 1];
}FileUploadInfo;

typedef struct WhitelistHeader
{
  int mode;
  char *name;
  char *value;
}WhitelistHeader;

typedef struct
{
  unsigned short debug_test_value_vuser;          //Store value of vusers given in keyword
  unsigned short debug_test_value_session_rate;   //Store value of session rate given in keyword
  unsigned short debug_test_value_duration;       //Store value of duration given in keyword
  unsigned short debug_test_value_session;        //Store value of session given in keyword
}DebugTestSetting;

typedef struct {
  /*Added 05/22/03*/
  int ns_factor;                /* NS_FACTOR */
  int max_con_per_vuser;        /* MAX_CON_PER_VUSER */
  int max_con_per_proxy;       /*MAX_CON_PER_PROXY */
  int cmax_parallel;            /* Commented out */
  int per_svr_max_parallel;     /* Commented out */
  /*05/22/03 additions complete*/
  int num_connections;          /* NUM_USERS */
  int num_fetches;              /* RUN_TIME x C */
  int test_stab_time;                  /* RUN_TIME x S/M/H */
  int num_process;              /* NUM_PROCESSES */
  int clickaway_pct;            /* Not initialized; initialized to dafault 0 while memsetting */
  // short think_time_mode;        /* Not used */
  // int mean_think_time;          /* Not used */
  // int median_think_time;        /* Not used */
  // int var_think_time;           /* Not used */
  //short user_cleanup_pct;       /* Commented */
  //  short use_host; /* Use Host in NSConf = 0, Use host from Host header = 1 */
  short interactive;            /* INTERACTIVE */
  short user_reuse_mode;        /* Initialized during user_data_check() */
  // short log;                    /* Not used */
  short ramp_down_mode;         /* RAMP_DOWN_MODE */
  int progress_secs;            /* PROGRESS_MSECS */
  int num_dirs;                 /* NUM_DIRS */
  int wan_env;                  /* WAN_ENV */
  int ramp_up_rate;             /* RAMP_UP_RATE */

  short num_user_mode;          /* Initialized during user_data_check */
  int ssl_pct;                  /* Initialized during init_default_values */

  int warmup_seconds;           /* WARMUP_TIME */

  int vuser_rpm;                /* TARGET_RATE; Vusers request per minute */
  unsigned short cookies;       /* DISABLE_COOKIES */
  // unsigned short health_monitor_on; /* HEALTH_MONITOR but commented out */
  unsigned short smon;              /* HEALTH_MONITOR */
  int cap_mode;                     /* CAPACITY_CHECK */
  int cap_consec_samples;           /* CAPACITY_CHECK */
  int cap_pct;                      /* CAPACITY_CHECK */

  short lat_factor;             /* ADVERSE_FACTOR */
  short loss_factor;            /* ADVERSE_FACTOR */
  int fw_jitter;                /* random value will swing +/- x% of forward delay -- WAN JITTER  */
  int rv_jitter;                /* random value will swing +/- x% of backword delay -- WAN_JITTER */
  //struct sockaddr_in saddr;
  char spec_url_prefix[MAX_FILE_NAME]; /* SPEC_URL_PREFIX */
  char spec_url_suffix[MAX_FILE_NAME]; /* SPEC_URL_SUFFIX */

  char gui_server_addr[MAX_FILE_NAME]; // From netstorm arguments -s IP:port
  short gui_server_port;

  char secondary_gui_server_addr[MAX_FILE_NAME]; // From netstorm arguments -s IP:port
  short secondary_gui_server_port;

  short load_key;               /* LOAD_KEY */
  short ramp_up_mode;           /* RAMP_UP_MODE */
  short user_rate_mode;         /* SESSION_RATE_MODE */
  short display_report;         /* DISPLAY_REPORT */
  int resp_logging_size;        /* RESP_LOGGING */
  int conn_retry_time;          /* CONN_RETRY_TIME */
  //char use_pct_prof;
  char use_sess_prof;           /* Initialized to zero during init_user_session */
  char nvm_distribution;        /* NVM_DISTRIBUTION */
  //char eth_interface[MAX_FILE_NAME];

  short log_postprocessing;     /* LOGDATA_PROCESS */
  short remove_log_file_mode;   /* 1 - remove log file, 0 - donot remove log file */
  short read_vendor_default;    /* for vendor defaults. Set to 0 is SIGNATURE is found */
  //short account_all;            /* initialized to 1 --- discontinueing*/
  short show_initiated;         //SHOW_INITIATED; 0:no , 1: yes
  short non_random_timers;      //NON_RANDOM_TIMERS; 0:no , 1: yes
  // short no_compression;         //Not used; 0:no , 1: yes
  short use_http_10;            //USE_HTTP_10; 0 1_1 and 1 means 1_0
  short optimize_ether_flow;    //OPTIMIZE_ETHER_FLOW; 0 default 1 quick ack 2 Reset close
  short pg_as_tx;   // PAGE_AS_TRANSACTION; Added by Anuj: For treating PAGE_AS_TRANSACTION
  short pg_as_tx_name;   // PAGE_AS_TRANSACTION: added for transaction name
  char testname[MAX_TNAME_LENGTH + 1];            /* TNAME or -N argument */


  int warning_threshold;        /* THRESHOLD */
  int alert_threshold;          /* THRESHOLD */
  int min_con_reuse_delay;
  int max_con_reuse_delay;

  //Atul 08/16/2007 -Number of Retries on Connection, SSL or Request or Faliure
  //unsigned int ramp_down_ideal_msecs; // This will keep the the connection request timeout in msec, will be given with RAMP_DOWN mode 3 only, default is 3000 msec

  int g_auto_cookie_mode;       /* AUTO_COOKIE mode */
  int g_auto_cookie_expires_mode;  /* AUTO_COOKIE expires_mode */ 

  int g_follow_redirects; /* Depth of how many redirects to follow for one URL */
  int g_auto_redirect_use_parent_method; /* Used with auto redirect */
  short exclude_failed_agg;     /* EXCLUDE_FAILED_AGGREGATES */


  int cap_status;                   /* Set initially using CAPACITY_CHECK and then updated by parent */

  // Following are set by parent only, so we can keep in the shared memory
  u_ns_ts_t test_start_time; /* initialized from parent_init_before_starting_test_run */
  u_ns_ts_t test_rampup_done_time; /* For parent */
  u_ns_ts_t test_runphase_start_time; /* PARENT */
  //u_ns_ts_t test_runphase_end_time;
  u_ns_ts_t test_duration;
  u_ns_ts_t partition_duration;
  u_ns_ts_t test_runphase_duration;
  short get_no_inlined_obj;     /* GET_NO_INLINED_OBJ, Moved from Global_data */

  int report_mask;            /* REPORT_MASK */
  short replay_mode;            /* Replay mode for netstorm */

  int max_sock;
  //sock list r()fd_table) is a singly linked list
  //elements are added on the tail and always
  //taken out at head
  //static SOCK_data *sock_head, *sock_tail;
  
  int high_perf_mode;
  unsigned int max_user_limit;   /* global max user limit for user genarationin FSR */

  int schedule_type;    /* 0 - Simple, 1 - Advanced */
  int schedule_by;      /* 0 - Scenario Based, 1 - Group Based */

  int use_prof_pct;

  /* For percentile user defined PDF file */
  char url_pdf_file[MAX_FILE_NAME]; //URL pdf File
  char page_pdf_file[MAX_FILE_NAME]; //Page pdf file
  char session_pdf_file[MAX_FILE_NAME]; //Session pdf file
  char trans_resp_pdf_file[MAX_FILE_NAME]; //Trans response time pdf file
  char trans_time_pdf_file[MAX_FILE_NAME]; //Trans time pdf file

  unsigned int error_log;   /* for error logging */

  char pause_done;            // Flag is to know scenario is paused or not

  /*enable or disable event deduplication*/
  char enable_event_deduplication;

  /* An epoll timeout for parent */ 
  int parent_epoll_timeout;
  /* A epoll timeout for NVMs  */ 
  int nvm_epoll_timeout;
  /*Max number of parent epoll timeouts after which an action to be taken*/
  char parent_timeout_max_cnt;
  /*Max number of NVMs epoll timeouts after which an action to be taken*/
  char nvm_timeout_max_cnt;
  /* Flag to dump NVMs by its parent on parent_timeout_max_cnt number of parent epolls timeout*/
  char dump_parent_on_timeout;
  /* Flag to skip timercall (dis_time_next/dis_time_run) in netstorm_child for(;;) loop*/
  char skip_nvm_timer_call;
  short wait_for_write;
  /*Event Logging Related remove if we can use static variables*/
  char event_log;
  char filter_mode;
  char enable_event_logger; 
  char event_file_name[MAX_EVENT_DEFINITION_FILE][4 * MAX_FILE_NAME];
  short event_logger_port; //Added to give port on which event logger will listen

  /*If all timer are same. Then set this flag. 
  * This flag is for optimization in adding timer at run time*/
  char ka_timeout_all_flag;
  /*This is for checking, if given 
 * IDLE timeout is same for all groups then set it to 1*/
  char idle_timeout_all_flag;

  char src_port_mode;  // Used for PORT randomization
  unsigned int protocol_enabled;
  
  unsigned char js_enabled;
  unsigned int js_runtime_mem;
  unsigned char js_runtime_mem_mode;
  unsigned int js_stack_size;

  char tr069_data_dir[1024];
  char tr069_acs_url[1024];
  char tr069_main_acs_url[1024];
  int tr069_options;
  int tr069_reboot_min_time;
  int tr069_reboot_max_time;
  int tr069_download_min_time;
  int tr069_download_max_time;
  int tr069_periodic_inform_min_time;
  int tr069_periodic_inform_max_time;

  /* URL Hash--BEGIN*/
  short static_url_hash_mode;
  short static_parm_url_as_dyn_url_mode;
  int static_url_table_size;
  int static_url_table_search_time_threshold;

  short dynamic_url_hash_mode;
  union
  {
    int dynamic_url_table_size;
    int dummy_dynamic_url_index;
  } url_hash;
  int dynamic_tx_table_size;
  int dynamic_table_threshold;
  int dynamic_url_table_search_time_threshold;
  //int total_dynamic
  /* URL Hash--END*/

  char g_tx_cumulative_graph;    /* ENABLE_TRANSACTION_CUMULATIVE_GRAPHS*/
  char g_enable_ns_diag; //Variable to diagnosis netstorm

  char lps_server[MAX_LPS_SERVER_NAME_SIZE + 1]; // LPS_SERVER 
  int lps_port;              // LPS_SERVER  port
  int lps_mode;              // LPS MODE 0,1,2. Default value is 2

  short runtime_increase_quantity_mode;
  short runtime_decrease_quantity_mode;
  short runtime_stop_immediately;
  char  reader_run_mode;        /*This is for logging reader's running mode*/
  int   reader_csv_write_time;  /* This is for frquency on which logging reader will write in csv file*/
  char net_diagnostics_server[MAX_LPS_SERVER_NAME_SIZE + 1]; // LPS_SERVER 
  int net_diagnostics_port;              // LPS_SERVER  port
  short net_diagnostics_mode;              // net diagnostics mode
  char nd_profile_name[64];

  //move from group base settings
  char net_diagnostic_hdr[32 + 1]; //Header will be send with request if net_diagonstic key enabled. Here maximum string length can be32 and 1 for null.
  

  //Controller ip. I will use in master modeto send controller ip to generators 
  char ctrl_server_ip[MAX_LPS_SERVER_NAME_SIZE + 1]; 
  short ctrl_server_port;

  /* Dynamic Host--BEGIN*/
  int max_dyn_host; //Maximum number of dynamic host
  int dns_threshold_time_reporting; //Threshold for DNS lookup timeout for reporting  
  /* Dynamic Host--END*/

  /*Script run in saparate threadi --BEGIN*/
  int init_thread;             // Total no of threads in system at init timt
  int incremental_thread;      // No of thread can increment in one time
  int max_thread;              // Max No thread can create by system
  int stack_size;             // Satack size for thread
  char logging_writer_debug;
  /*Script run in saparate threadi --END*/
  /*Netomni: Added new member to store total number of generator*/
  short num_generators;
  unsigned char event_generating_host[MAX_HOST_NAME_SIZE]; /*Buffer added to save host, one who is generating event*/
  char event_generating_ip[MAX_HOST_NAME_SIZE]; /*Buffer added to save host, one who is generating event*/
  int gen_id;
  int loc_id;

  char sp_enable;            // For keyword : ENABLE_SYNC_POINT 
  short proxy_flag;          //Flag taken to track if the proxy is set or not. -1: Initial Value; 0 - No Proxy Set; 1 - Proxy Set;

  /* For replay access log */
  double arrival_time_factor; // User arrival time multiplication factor
  double users_playback_factor;  // Percentage of uses to be played back
  double inter_page_time_factor;  // Inter page time multiplication factor
  short progress_report_mode; //Added to enable/disable progress report
  //Added variable for Production Monitoring Plans, generation of new TR after particular time interval
  //char monitoring_mode; //monitoring mode:: removing variable
  char nvm_fail_continue;//Added flag to check whether one need to continue test or not on NVM failure

  struct timer_type* time_ptr; //Need to pass timeout to timer
  short monitor_report_mode; //Enable/disable monitor details in progress report
  short get_gen_tr_flag;//Added a flag to get the generator TR data to the controller. 0 - Disable, 1 - Enable
  int ns_trace_level;//Added a flag to get trace level
  short enable_dns_graph_mode;
  char script_copy_to_tr;//Added a flag to set whether scripts will be copied with/without sub directories or donot copy
  char server_select_mode;// To decide whether we need to use same host for both HTTP and HTTPS request for a particular virtual user

//for continuous monitoring
  char continuous_monitoring_mode;
  long partition_switch_duration_mins;
  int cav_epoch_year;
  long unix_cav_epoch_diff;

  char *tr_or_partition;
  char *tr_or_common_files;

  int partition_creation_mode;
  int sync_on_first_switch;
  // Continue on monitor error
  char continue_on_mon_failure; // Standard/Custom monitors
  char continue_on_dyn_vector_mon_failure;
  char continue_on_pre_test_check_mon_failure;

  /* Firefox */
  int enable_ns_firefox;      /* Keyword: ENABLE_NS_FIREFOX 
                               * 1 - If ns firefox is enabled 
                               * 0 - If ns firefox is disabled
                               */
  int enable_ns_chrome;       /* Keyword: ENABLE_NS_CHROME
                               * 1 - If ns chrome is enabled
                               * 0 - If ns chrome is disabled 
                               */

  int browser_used;           /*  0 - firefox
                               *  1 - chrome
                               *  2 - firefox_and_chrome
                               */

  int create_screen_shot_dir;  /* Keyword G_RBU_CAPTURE_CLIPS
                                * 0 - Dont create any directory
                                * 1 - create screen shot directory
                                * 2 - create snap shot directory 
                                * 3 - create screen shot as well as snap shot directory
                                */
  char create_lighthouse_dir;  /* 0 - Do not create lighthouse directory
                                * 4 - create lighthouse directory
                                */

  int rbu_settings;            /* Keyword RBU_SETTINGS <freq>
                                * freq can be 0   - disable rendering time
                                *             100 - It will call for rendering after 100 milliseconds
                                *                 - This can't be less than 0 and it should be either equals to 0 or 
                                *                   greater than 100.
                                */

  char rbu_enable_csv;         /* Keyword RBU_ENABLE_CSV <mode>
                                * mode can be  : 0 - disable
                                *              : 1 - enable 
                                */

  char rbu_com_setting_mode;                    /* Keyword RBU_BROWSER_COM_SETTINGS <mode> <num_retries> <interval>
                                                 * mode can be  : 0 - disable
                                                 *              : 1 - enable 
                                                 */ 
  int rbu_com_setting_max_retry;                /* frequency    : positive integer number less then 257 */ 

  unsigned int rbu_com_setting_interval;        /* interval : Time interval for connection retry  */  

  char rbu_enable_auto_param;                   /* This will enable(1) or disable(0) the automation of profiles and vnc in RBU */
  char rbu_enable_tti;                          /* To enable(1)/disable(0) TTI Matrix, If disable then _tti field not be visible in HAR */
  char reset_test_start_time_stamp;
  char rbu_alert_policy_mode;                   /* Keyword RBU_ALERT_POLICY <mode> <policy_and script_name> */
  char rbu_ext_tracing_mode;                    /* Keyword RBU_EXT_TRACING <mode> 
                                                 * mode can be  : 0 - disable
                                                 *              : 1 - enable
                                                 */
  char rbu_move_downloads_to_TR_enabled;

  //Keywords related to njvm settings
  NjvmSettings njvm_settings;
  int rbu_user_agent;
  int rbu_enable_dummy_page;

  //Hierarchical view mode, conf file and vector separator
  int hierarchical_view;
  char hierarchical_view_topology_name[256];
  char hierarchical_view_vector_separator;

  char amf_seg_mode;  //To set amf parameterization like hessian

  //SM, 10-02-14: variables related to rbu screen resolution
  unsigned char rbu_screen_sim_mode;
  char *rbu_har_rename_info_file;
  char user_prof_sel_mode;

  //Number of max bufs a NVM can use if shared memory is full
  int max_logging_bufs;

  //Trace levels and file size
  char nlm_trace_level;
  char nlr_trace_level;
  char nlw_trace_level;
  char nsdbu_trace_level;

  int nlm_trace_file_sz;
  int nlr_trace_file_sz;
  int nlw_trace_file_sz;
  int nsdbu_trace_file_sz;
  
  char ns_tmpfs_flag;   //flag to save if tmpfs is enabled
  char nd_tmpfs_flag;
  
  // Block size
  int log_shr_buffer_size; 
  int nvm_scratch_buf_size;
  int nifa_trace_file_sz;
  int nifa_trace_level;
  /* NC: In case of URL normalization, on controller we require maximum number of NVM */
  int max_num_nvm_per_generator;
  int nirru_trace_level;

  //for oracle sql stats monitor
  int sql_report_mon;
  char generic_mon_flag;
  char show_group_data;
  char page_based_stat;
  long long ns_db_upload_chunk_size;
  long long nd_db_upload_chunk_size;
  int db_upload_idle_time_in_secs;
  int db_upload_num_cycles;
  int parent_child_con_timeout;
  //Runtime Quantity Changes
  int num_qty_rtc;
  int num_qty_ph_rtc; 
  char NSDBTmpFilePath[1024];
  char NDDBTmpFilePath[1024];

 //for java object manager 
  char use_java_obj_mgr;
  short java_object_mgr_port;
  int java_object_mgr_threshold;
  int jrmi_call_timeout;
  int jrmi_port;
  
  short log_vuser_mode;
  unsigned int log_vuser_data_interval;
  int log_vuser_data_count;
  int adjust_rampup_timer;
  int log_inline_block_time; // for inline block time

  //To save raw data file path, Paths can be more than one so we are taking buffer of 2028
  //because we don't have any check if the string is more than given string
  char *multidisk_rawdata_path;
  char *multidisk_nslogs_path;
  char *multidisk_ndlogs_path;
  char *multidisk_processed_data_path;
  char *multidisk_percentile_data_path;
  char *multidisk_nscsv_path;
  char *multidisk_ns_rawdata_path;
  char *multidisk_ns_rbu_logs_path;
  int show_ip_data;
  char gperf_cmd_options[256];
  char stop_test_if_dnsmasq_not_run;
  char disable_use_of_gen_spec_kwd_file;
  short exclude_stopped_stats; // this will be used to include or exclude failed statistics.
  int ns_use_port_min;
  int ns_use_port_max;
  char save_nvm_file_param_val;   //This will save distributed data over the nvm at path ~/TRxx/scripts/<script_name>/
                                  //<Data File>.<first_paramter>.<nvm_id>.<mode> 0-> NO, 1-> Yes
  long long max_rtg_size;

  /*Data for ssl key log feature*/
  int ssl_key_log; 
  int ssl_key_log_fd;
  char ssl_key_log_file[1024];
  char db_aggregator_mode;
  char db_aggregator_conf_file[MAX_FILE_NAME];

  int rtc_timeout_mode;
  int rtc_schedule_timeout_val;
  int rtc_system_pause_timeout_val;
  short db_replay_mode; //used for replay mode db query
  char json_mode;
  char auto_json_monitors_filepath[1024];
  char auto_json_monitors_diff_filepath[1024];
  char *json_files_directory_path;   //this variable is used when ENABLE_AUTO_JSON_MONITOR mode is 2
  char show_vuser_flow_mode;

  //for SHOW_SERVER_IP_DATA
  char show_server_ip_data;
  //int num_ip_size; 
  
  //for CHECK_CONTINUOUS_MON_HEALTH
  short continuous_mon_check_demon;
  int continuous_mon_check_demon_start_test_val;
  int continuous_mon_check_demon_start_ndc_val;
  int continuous_mon_check_demon_rtg_diff_count;
  int continuous_mon_check_demon_trace_log_level;
  char continuous_mon_check_demon_to_list[2048];
  char continuous_mon_check_demon_cc_list[2048];
  char continuous_mon_check_demon_bcc_list[2048];

  //for Dynamaic Tx support
  short max_dyn_tx_name_len;  // Maximum lenght of dynamics transactions
  int total_tx_limit;  // Max limit of total transactions
  short dyn_tx_norm_table_size;  // Table size of dynamic normalization table
  short dyn_tx_initial_row_size;  // Initial number of rows to allocate for dyn tx 
  short dyn_tx_delta_row_size;  // Delta number of rows to allocate for dyn tx 
  int   threshold_for_using_gperf;
  
  //Fix Concurrent Session
  char concurrent_session_mode;
  int concurrent_session_limit;
  int concurrent_session_pool_size;

  //DYNAMIC_CM_RT_TABLE_SIZE
  char dynamic_cm_rt_table_mode;
  int dynamic_cm_rt_table_size; 

  //On the basis of this element, appliance health monitor will be enabled.
  char enable_health_monitor;

  //child pool
  short progress_report_max_queue_to_flush;  

  short progress_report_queue_size; //number of samples in queue/mpool
  URIEncoding encode_uri[256];
  URIEncoding encode_query[256];

  char rbu_domain_stats_mode;    // RBU/NVSM Domain Stats 
  int netTest_total_executed_page; //For NetTest, number of pages executed. TODO: We need to think where it should be kept
  char rbu_enable_mark_measure;    //Enable or disable capturing of mark and measure
  int jmeter_idx;
  int jmeter_port[MAX_FILE_NAME];
  int num_retry_on_bind_fail;     //Number of Retries when bind failed
  char action_on_bind_fail;       //Action 0 :to Continue and 1: to Stop on bind failed
  ContinueTestOnGenFailure con_test_gen_fail_setting;
  char disable_script_validation; //Added a flag to set whether scripts will be validated or not.
  int tw_sockets_limit;
  char enable_hml_group_in_testrun_gdf;
  Long_data g_rtg_hpts;
  NCTcpDumpSettings *nc_tcpdump_settings;      //Added for TCP dump in case of NC
  char enable_memory_map;	  //To dump memory map
  char is_thread_grp;             //To check whether one grp is running in thread mode
  AlertInfo *alert_info;
  int rtc_pause_seq;             //Sequence number pf pause is message.
  
  //Cavisson Test Monitor
  FileUploadInfo *file_upload_info;            //Store information related to file upload
  int monitor_type;                            //Test Monitor Type
  int monitor_idx;                             //Test Monitor Index
  char *monitor_name;                          //Test Monitor Name
  char *cavtest_tier;                          //Test Tier Name
  char *cavtest_server;                        //Test Server Name
  long long cavtest_partition_idx;             //Test Partition Index

  int cavgen_version;                         //CAVGEN VERSION

  //nethavoc variable
  Nh_scenario_settings *nethavoc_scenario_array;
  
  //SocketAPI
  char socket_api_proto_flag;    // 0x01 = TCPClient, 0x02 = TCPServer, 0x04 = UDPClient, 0x08 = UDPServer 
  char data_comp_type;           //For data compression  
  int per_proc_min_user_session; // For creating required num of NVM's
  short page_as_tx_jm_parent_sample_mode; // Jmeter parent sampler mode
  int flags;                     //Flags for various purpose
  char write_rtg_data_in_db_or_csv;

  WhitelistHeader *whitelist_hdr;
  DebugTestSetting debug_setting;
} Global_data;

#ifndef CAV_MAIN
extern Global_data *global_settings;
#else
extern __thread Global_data *global_settings;
#endif
#endif

