/************************************************************
* General utility functions
* parser of NetStorm's configuration file
*
* HISTORY
* 8/29/01	SSL_PCT added
* 9/11/01	Server entries added
************************************************************/


#ifndef UTIL_H
#define UTIL_H

#ifndef __USE_XOPEN
#define __USE_XOPEN
#endif

#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <limits.h>
#include <regex.h>
/*#include <db1/db.h>*/

/*#if (Fedora && RELEASE == 4)
  #include "db.h"
#else*/
  #include "/usr/include/db.h"
//#endif


#include "ns_msg_def.h"
#include "ns_global_settings.h"
#include <fcntl.h>
#include <arpa/nameser.h>
#include <arpa/nameser_compat.h>
#include "url.h"
#include "ns_server.h"
#include "ns_static_vars.h"
#include "ns_cookie_vars.h"
#include "ns_tag_vars.h"
#include "ns_search_vars.h"
#include "ns_unique_range_var.h"
#include "ns_json_vars.h"
#include "ns_check_point_vars.h"
#include "ns_check_replysize_vars.h"
#include "ns_click_script_parse.h"
#include "ns_proxy_server.h"
#include "ns_nsl_vars.h"
#include "ns_rbu.h"
#include "ns_master_agent.h"
#include "nslb_partition.h"
#include "ns_ssl.h"
#include "ns_sock_list.h"
#include "ns_jmeter.h"
#include "ns_rte_api.h"
#include "ns_kw_set_non_rtc.h"
#include "ns_body_encrypt.h"
#include "ns_nd_integration.h"
#include "ns_nsl_vars.h"
//#include "ns_inline_delay.h"

#define SRC_PORT_MODE_RANDOM 1
#define USE_DNS_ON_UDP 0
#define USE_DNS_ON_TCP 1
#define REFERER_ENABLED            0x0001  // This flag is used to check if referer is enabled or disabled.
#define CHANGE_REFERER_ON_REDIRECT 0x0010  // This flag is used to check whether we need to change referer on redirection or not.

#define STATUS_CODE_REDIRECT            0x01  // This flag is used to check whether we need to save the referer.
#define STATUS_CODE_DONT_CHANGE_REFERER 0x02  // This flag is used to check whether we need to save the referer.

/* For Saving URL body, header */
#define SAVE_URL_HEADER 1
#define SAVE_URL_BODY 2

//  Session Table flags bitmask
#define ST_FLAGS_SCRIPT_OLD_FORMAT  0x0001 // Script is in the old format (for runlogic)
#define ST_FLAGS_SCRIPT_NEW_FORMAT  0x0002 // Script is in the new format (for runlogic)
#define ST_FLAGS_SCRIPT_OLD_JAVA_PKG  0x0004 // Java type script has old pkg (com…)
#define ST_FLAGS_SCRIPT_NEW_JAVA_PKG 0x0008 // Java type script has new pkg (script.)

extern void dump_urlvar_data();
extern void fprint3f(FILE *fp1, FILE *fp2, FILE *fp3, char* format, ...);
extern void fprint2f(FILE *fp1, FILE *fp2, char* format, ...);
extern void print2f(FILE *fp, char* format, ...);
extern void print2f_always(FILE *fp, char* format, ...);
//int resolve_scen_group_pacing();
//Add - Achint 12/14/2006
extern int is_script_url_based(char *script_name);
extern FILE *console_fp;
#define DEFAULT_DATA_FILE "include/VendorData.default"
#define DEFAULT_KEYWORD_FILE "sys/site_keywords.default"

//  Error codes, if any changes made in no of errors these line should be changed.
//#define TOTAL_USER_URL_ERR 24
//#define TOTAL_USER_PAGE_ERR 64
//#define TOTAL_USER_TX_ERR 64
//#define TOTAL_USER_SESS_ERR 64

/* used by agent & coordinator */
#define OPCODE_INIT 1
#define OPCODE_KILL 2

// defines for user_reuse_mode of globals.
#define NS_REUSE_USER_AFTER_THINKTIME 0
#define NS_REUSE_USER_IMMIDIATE 1
#define NS_REUSE_USER_NONE 2
#define NS_REUSE_USER_AFTER_PAGETHINK 3
#define NS_REUSE_USER_AFTER_SESSTHINK 4

/* Run modes */
#define NORMAL_RUN 0
#define STABILIZE_RUN 1
#define FIND_NUM_USERS 2

/* Module masks */
#define HTTP_TX_OUTPUT 1
#define FUNCTION_CALL_OUTPUT 2
#define TESTCASE_OUTPUT 16


#define TX_PARSING             64  // For debug logging of Transaction Parsing
#define TX_EXECUTION           128 // For debug logging of Transaction Exceution

#define MAX_NTLM_PARAMETER_VAL 512

#define VUSER_REPORT 16

#define WORD_SIZE 64
#define WORD_ALIGNED(size) ((size + WORD_SIZE) & ~WORD_SIZE)

#define RANDOM_VAR_TABLE_MEMORY_CONVERSION(index) (randomvar_table_shr_mem + index);
#define RANDOM_STRING_TABLE_MEMORY_CONVERSION(index) (randomstring_table_shr_mem + index);
#define UNIQUE_VAR_TABLE_MEMORY_CONVERSION(index) (uniquevar_table_shr_mem + index);
#define UNIQUE_RANGE_VAR_TABLE_MEMORY_CONVERSION(index) (unique_range_var_table + index);
#define DATE_VAR_TABLE_MEMORY_CONVERSION(index) (datevar_table_shr_mem + index);

#define PAGE_TABLE_MEMORY_CONVERSION(index) (page_table_shr_mem + index)
#define WEIGHT_TABLE_MEMORY_CONVERSION(index) (weight_table_shr_mem + index)
#define GROUP_TABLE_MEMORY_CONVERSION(index) (group_table_shr_mem + index)
#define POINTER_TABLE_MEMORY_CONVERSION(index) (pointer_table_shr_mem + index)
#define FPARAMVALUE_TABLE_MEMORY_CONVERSION(index) (fparamValueTable_shr_mem + index)
#define VAR_TABLE_MEMORY_CONVERSION(index) (variable_table_shr_mem + index)
#define INDEX_VAR_TABLE_MEMORY_CONVERSION(index) (index_variable_table_shr_mem + index)
#define SEG_TABLE_MEMORY_CONVERSION(index) (seg_table_shr_mem + index)
#define POST_TABLE_MEMORY_CONVERSION(index) (post_table_shr_mem + index)
#define GSERVER_TABLE_MEMORY_CONVERSION(index) (gserver_table_shr_mem + index)
#define SERVERORDER_TABLE_MEMORY_CONVERSION(index) (serverorder_table_shr_mem + index)
#define REQCOOK_TABLE_MEMORY_CONVERSION(index) (reqcook_table_shr_mem + index)
#define REQDYNVAR_TABLE_MEMORY_CONVERSION(index) (reqdynvar_table_shr_mem + index)
#define REQBYTEVAR_TABLE_MEMORY_CONVERSION(index) (reqbytevar_table_shr_mem + index)
#define REQUEST_TABLE_MEMORY_CONVERSION(index) (request_table_shr_mem + index)
#define HOST_TABLE_MEMORY_CONVERSION(index) (host_table_shr_mem + index)
#define THINK_PROF_TABLE_MEMORY_CONVERSION(index) (thinkprof_table_shr_mem + index)

#define MAX_CONF_LINE_LENGTH 16*1024   //moved from util.c

//Using this macro in Custom monitor parsing, Dynamic monitor parsing, Standard monitor parsing & keyword parsing
#define MAX_MONITOR_BUFFER_SIZE 64*1024 
#define MAX_BUFFER_SIZE_FOR_MONITOR 128*1024
#define INITIAL_CM_INIT_BUF_SIZE 128*1024

#define RESET_SCRATCH_BUF 1
#define DONT_RESET_SCRATCH_BUF 0


//Using this macro to define max no of app that could be given in appname argument
#define MAX_NO_OF_APP 64 

#define SERVER_NAME_SIZE 64
#define MAX_SERVER_STATS 25 //change from 10 to 25(to increse the no of server stats),we will make it dynamic afterwards

/* DNS defines */
#define DNS_DEFAULT_QUERY_TYPE T_A
#define DNS_DEFAULT_RECURSION 1

#define LOG_LEVEL_FOR_DRILL_DOWN_REPORT  (vptr->flags & NS_VPTR_FLAGS_DDR_ENABLE)

// Session record is logged if DDR is enabled for the session and one HTTP page is used Or
// at least one page is dumped for page dump
#define DDR_CHECK_FOR_SESSION_RECORD \
       ((LOG_LEVEL_FOR_DRILL_DOWN_REPORT) && (vptr->flags & NS_VPTR_FLAGS_HTTP_USED)) || (vptr->flags & NS_VPTR_FLAGS_PAGE_DUMP_DONE)

/* Added macros for SGRP supporting 0 quantity or pct*/ 
#define NS_GRP_IS_ALL      -1
#define NS_GRP_IS_INVALID  -2
/*Manish: create static buffer for data file */

#define START_DT_RECORDING  1
#define STOP_DT_RECORDING   0

#define NULL_SESS_ID 0xFFFFFFFF    //Default Sess_id is '-1'
#define MAX_CHKSUM_HDR_LENGTH 128

#define MAX_NVM_NUM 255		//maximum nvms


/* used by agent & coordinator */
typedef struct {
  int conf_length;
  int url_length;
  int ip_length;
} Sysload_data;

extern int total_hdr_entries;
extern int max_hdr_entries;

extern int g_tomcat_port;  //Used in RBU for sending msgs from gen to cntlr
extern int default_Flag_SRC_IP_LIST;
extern int max_server_entries;
extern int total_server_entries;
#ifndef CAV_MAIN
extern char *data_file_buf;
extern long malloced_sized;
extern int max_cookie_entries;
extern int max_searchvar_entries;
extern int max_searchpage_entries;
extern int max_randomvar_entries;
extern int max_randomstring_entries;
extern int max_datevar_entries;
extern int max_uniquevar_entries;
extern int max_perpagechkpt_entries;
extern int max_perpagechkrepsize_entries;
extern int max_unique_range_var_entries;
extern int max_jsonvar_entries;  //For JSON Var
extern int max_jsonpage_entries; //For JSON Var
extern char * url_errorcode_table[];
extern int max_user_entries;
extern int total_user_entries;
extern int total_ip_entries;
extern int total_client_entries;
extern int g_actual_num_pages;   //Actual Number of Pages
extern int g_rbu_num_pages;      //Number of Pages Used in RBU
extern int max_check_replysize_page_entries; //can be moved to ns_check_reply_size.h
extern int total_check_replysize_page_entries; //can ne moved to ns_check_reply_size.h
extern int total_inusesvr_entries;
extern int total_proxy_excp_entries;
extern int total_proxy_ip_interfaces;  //Used to store number of interfaces of NS machine
extern int max_index_var_entries;
#else
extern __thread char *data_file_buf;
extern __thread long malloced_sized;
extern __thread int max_cookie_entries;
extern __thread int max_searchvar_entries;
extern __thread int max_searchpage_entries;
extern __thread int max_randomvar_entries;
extern __thread int max_randomstring_entries;
extern __thread int max_datevar_entries;
extern __thread int max_uniquevar_entries;
extern __thread int max_perpagechkpt_entries;
extern __thread int max_perpagechkrepsize_entries;
extern __thread int max_unique_range_var_entries;
extern __thread int max_jsonvar_entries;  //For JSON Var
extern __thread int max_jsonpage_entries; //For JSON Var
extern __thread char * url_errorcode_table[];
extern __thread int max_user_entries;
extern __thread int total_user_entries;
extern __thread int total_ip_entries;
extern __thread int total_client_entries;
extern __thread int g_actual_num_pages;   //Actual Number of Pages
extern __thread int g_rbu_num_pages;      //Number of Pages Used in RBU
extern __thread int max_check_replysize_page_entries; //can be moved to ns_check_reply_size.h
extern __thread int total_check_replysize_page_entries; //can ne moved to ns_check_reply_size.h
extern __thread int total_inusesvr_entries;
extern __thread int total_proxy_excp_entries;
extern __thread int total_proxy_ip_interfaces;  //Used to store number of interfaces of NS machine
extern __thread int max_index_var_entries;
#endif
extern int max_sla_entries;


typedef struct OverrideRecordedThinkTime {
  short mode;
  float multiplier;
  float min;
  float max;
} OverrideRecordedThinkTime;



typedef struct ntlm_settings_s
{
  int enable_ntlm;  // (0/1 Disable/Enable, Default is 1)
  int ntlm_version;  // (0/1/2 NTLM/NTLM2/NTLMv2, Default is 2)
  char domain[MAX_NTLM_PARAMETER_VAL + 1];
  char workstation[MAX_NTLM_PARAMETER_VAL + 1];
} ntlm_settings_t;

typedef struct kerb_settings_s
{
  int enable_kerb;  // (0/1 Disable/Enable, Default is 0)
} kerb_settings_t;

#define MAX_NS_TX_HTTP_HEADER 512

#define NS_TX_HTTP_HEADER_DO_NOT_SEND                   0
#define NS_TX_HTTP_HEADER_SEND_FOR_MAIN_URL             1
#define NS_TX_HTTP_HEADER_SEND_FOR_BOTH_MAIN_AND_INLINE 2
//To end transaction based on NetCache hit
#define END_TX_BASED_ON_NETCACHE_HIT  1

typedef struct ns_tx_http_header_settings
{
  char mode;  // (0/1/2 Do not send/Send for Main URL Only/For both Main and Inline URLs, Default is 1)
  char end_tx_mode;
  short header_len;
  char header_name[MAX_NS_TX_HTTP_HEADER + 1]; //(Default http header is CavTxName) //TODO: put this in big buf
  char tx_variable[MAX_NS_VARIABLE_NAME + 1];
  char end_tx_suffix[MAX_NS_VARIABLE_NAME + 1];
} ns_tx_http_header_settings;


/* RBU group based settings structure */
typedef struct rbu_group_settings_s
{
  char broswer_app_path[512 +1];  

  int enable_rbu;            // This flag will tell whether script is rbu or normal
                             // 0 - script is Normal script (defaulr)
                             // 1 - script is rbu script
                             // 2 - script is rbu script(Node Mode)

  int enable_screen_shot;    // This flag will set and tell whether screen shot has been taken or not??
                             // 0 - disable
                             // 1 - enable

  int enable_auto_param;     // This flag will set and tell whether we should enable auto parametrisation or not?
                             // 0 - disable
                             // 1 - enable
  
  int stop_browser_on_sess_end_flag;      // This flag determine whether Netstorm automatically stop browsers on session end or not? 
                                          // 0 - Not stop browser at session end 
                                          // 1 - Stop browser at session end

  int browser_mode;        // This will set the browser mode
                            // 0 - firefox (Default)
                            // 1 - chrome  
                            // 99 - take browser from VendorData.default

  int lighthouse_mode;       // This will set the lighthouse mode
                             // 0 - disabled
                             // 1 - enabled

  int clear_cache_on_start;  // This flag will tell whether we have to clear cache on start of the session or not?
                             // 0 - don't clear cache on start of the session
                             // 1 - clear cache on start of the session
                             // 2 - clear cache on start of the page

  int clear_cookie_on_start;  // This flag will tell whether we have to clear cookie on start of the session or not?
                              // 0 - don't clear cookie on start of the session
                              // 1 - clear cookie on start of the session

  int enable_cache;         // This flag will tell caching has to be disabled or not? Default is enable 
                            // 0 - Caching is enable
                            // 1 - Caching is disable 
  int page_loaded_timeout;  // This will store page loaded time out

  int pg_load_phase_interval;  // This will store phase interval time for page load time calculation

  int enable_capture_clip;  // This flag will tell whether we want to capture the clips or not
                            // 0 - Capturing Clips is disable
                            // 1 - Capturing Clips is enable
                            // 2 - Capturing Clips is enable and when domload, onload threshold is provided
  int clip_frequency;       // This will tell about how many interval it will capture the clips. This is in milliseconds.
  int clip_quality;         // This will tell about the quality of clips. This is in range of 0 to 10000.
  int clip_domload_th;      // This will tell about the domload_threshold.
  int clip_onload_th;       // This will tell about the onload_threshold.
  
  int rbu_header_flag;      // This will tell whether header is enabled or not. Default is disable.

  char prof_cleanup_flag;   // This will tell whether profile is cleanup before each session or not.
                            // 0 - Disable
                            // 1 - Enable

  char sample_profile [MAX_SAMPLE_PROF_LEN + 1];   // This will copy all the profiles from this sample_profile.

  char rbu_har_setting_mode;                       /* Keyword G_RBU_HAR_SETTING <group_name> <mode> <compression> <request> <response> 
                                                      mode can be 0 - disable, 1 - enable   
                                                   */
  char rbu_har_setting_compression;
  char rbu_har_setting_request;
  char rbu_har_setting_response;
  char rbu_js_proc_tm_and_dt_uri;                  /* Enable/Disable capturing of js parsing time and data uri */

  char rbu_cache_domain_mode;                      /* Keyword G_RBU_AKA_DOMAIN <GRP_NAME> <MODE> <DOMAIN_LIST> 
                                                      mode can be 0 - disable, 1 - enable */

  char domain_list[1024 + 1];

  char rbu_nd_fpi_mode;                            /* Keyword G_RBU_ADD_ND_FPI <GRP_NAME> <MODE>  <send_mode>
                                                      mode can be 0 - disable, 1 - enable and send 'f' ad value of header                                                                            2 - enable and send  'F' as value of header  */
  char rbu_nd_fpi_send_mode;                       /* send mode can be 0 - send header in main url
                                                                       1 - send header in main and inline url   */

  char rbu_alert_setting_mode;                          /* Keyword is for disable or enable the alerts */

  char user_agent_mode;                                 //Group based storage for User-Agent mode
  char user_agent_name[1024];                           //Group based storage for User-Agent 

  char tti_mode;                                        //Group based storage for TTI mode
  char screen_size_sim;                                 //Group based storage got SCREEN_SIZE_SIMULATION
  int rbu_settings;                                     //settings for showing render time
  unsigned short har_timeout;                           //Group based storage for page_load_wait_time or HAR timeout value in seconds
  char rbu_domain_ignore_list[512 + 1];                 //Group based storage for G_RBU_DOMAIN_IGNORE, 
                                                        //it will store domain list which will be ignored in HAR
  char *rbu_block_url_list;                             // Group based storage for G_RBU_BLOCK_URL_LIST 
                                                        // it will store urls list which will be blocked before going over network
  char rbu_access_log_mode; 
  char rbu_acc_log_status[32]; 

  char rbu_rm_unwntd_dir_mode;                          //Mode for removing directory from profile dir.
  char rbu_rm_unwntd_dir_list[512 + 1];                 //List of directories to be removed

  char brwsr_vrsn[8 + 1];                               //Stores Browser Version According to Group 
  int har_threshold;                                    //time to wait for HAR file after onload (in miliseconds)
  char rbu_auto_selector_mode;                          //Enable or Disable autoSelector feature

  char performance_trace_mode;                          //Enable or Disable performance trace dump
  int performance_trace_timeout;                        //performance trace dump timeout value in millisecond
  char performance_trace_memory_flag;                   //capture memory stats with performance trace dump
  char performance_trace_screenshot_flag;               //capture screenshot with performance trace dump
  char performance_trace_duration_level;                //duration level(0/1)

  char selector_mapping_mode;                           //Mode for deprecated selector mapping
  char *selector_mapping_file;                          //Path of selector mapping profile file

  char reload_har;                                      //G_RBU_RELOAD_HAR <Group> <mode> where mode 0 disable(default) 1 Enable
  int wait_until_timeout;                               //WaitUntil timeout value in second

  char ntwrk_throttling_mode;                           //Network Throttling in RBU(Lighthouse)
  int ntwrk_down_tp;                                    //Network Throttling: download throughput
  int ntwrk_up_tp;                                      //Network Throttling: upload throughput
  int ntwrk_latency;                                    //Network Throttling: upload throughput
  char cpu_throttling_mode;                             //CPU Throttling in RBU
  int cpuSlowDownMultiplier;                            //CPU throttling in lighthouse
  char lh_device_mode;                                  //Device Simulation in RBU(Lighthouse)
}rbu_group_settings_t;

#define DT_REQUEST_ID_ENABLED       0x0001 // ID -> Request ID
#define DT_VIRTUAL_USER_ID_ENABLED  0x0002 // VU -> Virtual User ID
#define DT_LOCATION_ENABLED         0x0004 // GR -> Location
#define DT_SCRIPT_NAME_ENABLED      0x0008 // SN -> Script Name
#define DT_AGENT_NAME_ENABLED       0x0010 // AN -> Agent Name
#define DT_PAGE_NAME_ENABLED        0x0020 // PC -> Page Name

#define IS_DT_HEADER_STATS_ENABLED_FOR_GRP(group_num) \
  (runprof_table_shr_mem[group_num].gset.dynaTraceSettings.mode)

enum enumDTFieldOptions {
  DT_OPT_USER=0,
  DT_OPT_PASSWD,
  DT_OPT_HOST,
  DT_OPT_PROFILE,
  DT_OPT_PRESENTABLENAME,
  DT_OPT_DESCRIPTION,
  DT_OPT_IS_TIME_STAMP_ALLOWED,
  DT_OPT_OPTION,
  DT_OPT_IS_SESSION_LOCKED,
  DT_OPT_LABEL,
  DT_OPT_CONTINUE_ON_DT_RECORDING_FAILURE,
  DT_OPT_IS_SSL,
  __dtFieldOptionsCount__
};

typedef struct { 
  char mode;
  short requestOptionsFlag; 
  char sourceID[128];
  char fieldOptions[__dtFieldOptionsCount__][128];
} DynaTraceSettings;

typedef struct {
  unsigned int init_window_size_con;
  unsigned int init_window_size_str;
  short http_mode;
}HttpSettings;

typedef struct {
  short enable_push;
  int max_concurrent_streams;
  int initial_window_size;
  int max_frame_size;
  short header_table_size; 
}Http2Settings;


typedef struct {
  char enable_m3u8;
  int bandwidth;
} M3u8Settings;

typedef struct {
  char enable_rte;
  ns_rte rte;
} RteSettings;

typedef struct IP_data {
  //unsigned long ip_addr;
  //struct in_addr ip_addr; changed to ipv6 below 5/29/07
  struct sockaddr_in6 ip_addr;
  unsigned short port;
  short net_idx; //Add 03/17/2007 - Ipmgmt
  struct IP_data *next; 
  short ip_id;
  char ip_str[45 + 1];
} IP_data;

typedef struct Master_Src_Ip_Table{
  IP_data *ip_entry;
  int num_entries;
 // int seq_num;
  SOCK_data *sock_head; //to keep link list of availble sockets
  SOCK_data *sock_tail;
}Master_Src_Ip_Table;

typedef struct {
 IP_data *start_ip;
 int num_ip;
} PerGrpSrcIPTable;

typedef struct PerGrpUniqueSrcIPTable {
 IP_data *src_ip;
 struct PerGrpUniqueSrcIPTable *next; 
} PerGrpUniqueSrcIPTable;

typedef struct {
  PerGrpUniqueSrcIPTable *free_srcip_head; 
  PerGrpUniqueSrcIPTable *free_srcip_tail;
  PerGrpUniqueSrcIPTable *busy_srcip_head;
  PerGrpUniqueSrcIPTable *busy_srcip_tail;
} UniqueSrcIPTable;
 
typedef struct {
  int seq_num;
} SharedSrcIPTable;

typedef struct {
  /* Run Time Settings */
  //ThinkProfTableEntry_Shr *thinkprof_table_shr_mem; /* PAGE_THINK_TIME */
  int idle_secs;                                    /* IDLE_SECS, Moved from Global_data */
  int response_timeout;                             /* response_timeout in millisecond */
  int ka_pct;                                       /* KA_PCT, Moved from Global_data */
  int num_ka_min;                                   /* NUM_KA, Moved from Global_data */
  int num_ka_range;                                 /* NUM_KA, Moved from Global_data */
  short retry_on_timeout;       /*0: no retry 1: retry for safe method 2: retry for all methods */
  int max_url_retries;          /* MAX_URL_RETRIES, Moved from Global_data */
  short no_validation;          //0:do_validation , 1: no_validation /* NO_VALIDATION, Moved from Global_data */
  short disable_reuseaddr; //0: set reuseaddr, 1: dont reuseaddr sock option /* DISABLE_REUSEADDR, Moved from Global_data */
  unsigned int disable_headers; /* Moved from Global_data, for:
                                 * NO_HTTP_COMPRESSION
                                 * DISABLE_HOST_HEADER
                                 * DISABLE_UA_HEADER
                                 * DISABLE_ACCEPT_HEADER
                                 * DISABLE_ACCEPT_ENC_HEADER
                                 * DISABLE_KA_HEADER
                                 * DISABLE_CONNECTION_HEADER
                                 * DISABLE_ALL_HEADER
                                 */
  int num_additional_headers;   /* Derived based on disable_headers */

  int use_rec_host; /* If keyword value is 0, then actual server name is send in Host header. 
                     * Otherwise recorded server is send. :Anuj 08/03/08.
                     * For: USE_RECORDED_HOST_IN_HOST_HDR
                     */
  
  int avg_ssl_reuse;            /* AVG_SSL_REUSE, Moved from Global_data */
  short ssl_clean_close_only;  //1 only clean close 0 clean & unclesan, for SSL_CLEAN_CLOSE_ONLY
  short on_eurl_err;      /* ON_EURL_ERR */
  //short continue_on_pg_err;     /* CONTINUE_ON_PAGE_ERROR */
  short errcode_ok;   /* ERR_CODE_OK */

  short log_level;    /* LOGGING */
  short log_dest;     /* LOGGING */
  
  short trace_level;            /* TRACING */
  short max_trace_level;        /* TRACING RuntimeChanges: used to store maximum trace level value*/
  short trace_dest;             /* TRACING */
  short max_trace_dest;         /* TRACING RuntimeChanges: used to store maximum trace dest value*/
  short trace_on_fail;          /* TRACING */
  short trace_start;            /* TRACING */
  //Added new fields
  short trace_inline_url;            /* TRACING */
  short trace_limit_mode;       /* TRACING Limit mode*/
  double trace_limit_mode_val;  /* TRACING Limit mode value, in case of mode 1 or 2 
                                   user need to provide value either pct or number*/

  int  max_log_space;           /* TRACING */
  short report_level;           /* REPORTING */
  char ddr_session_pct;         /* Percentage of sessions to be logged in DDR database*/
  u_ns_8B_t module_mask;        /* MODULEMASK, Moved from Global_data */
  int debug;                    /* DEBUG, Moved from Global_data */

  short enable_referer;         // used as a bitmask for sending referer. For flags check above

  int url_idle_secs;            /*  URL_IDLE_SECS, Moved from Global_data */
  
  
  /* Scheduling */
  // int ramp_up_mode_for_session; /* RAMP_UP_TIME */

  // short ramp_down_mode;         /* RAMP_DOWN_MODE */
  int user_cleanup_time;        /* USER_CLEANUP_MSECS */
  // short user_rate_mode;         /* SESSION_RATE_MODE */
  
  unsigned int grp_max_user_limit;  // max user limit for FSR

  // Smtp time outs
  int smtp_timeout_greeting;
  int smtp_timeout_mail;
  int smtp_timeout_rcpt;
  int smtp_timeout_data_init;
  int smtp_timeout_data_block;
  int smtp_timeout_data_term;
  int smtp_timeout;

  int pop3_timeout;
  int ftp_timeout;
  int dns_timeout; 
  int ldap_timeout; 
  int imap_timeout;
  int jrmi_timeout; 
  /* user socket timeouts are group based for now. These can be moved into the cptr->data
   * to make them per connection. however, the apis to set timeout will need to be passed the
   * socket descriptor in that case (this is not done in LR version for this api)
   */
  struct timeval userSockAcceptTimeout;
  struct timeval userSockConnectTimeout;
  struct timeval userSockSendTimeout;
  struct timeval userSockRecvTimeout;
  struct timeval userSockRecv2Timeout;
  int ka_timeout;
  short ka_mode;
  short get_no_inlined_obj;
  /* Caching*/
  char cache_mode;  // Enable/Disable caching
  unsigned short cache_user_pct; // Caching Percentage its is in multiple of 100, mist divide by 100 when use
  unsigned short client_freshness_constraint; //For client's cache-control headers
  double cache_freshness_factor; // Caching Percentage its is in multiple of 100, mist divide by 100 when use
  char cache_table_size_mode; // For cache table size
  int cache_table_size_value;

  int cache_delay_resp;         /*This is used to avoid stack overflow if caching is happening so freq*/

  /*Java Script Engine related keywords*/
  int js_mode;
  /*unsigned short js_memsize;*/
  int js_all;

  /*Cache Master table related stuff*/
  short master_cache_table_size;
  char  master_cache_mode;
  char  master_cache_tbl_name[256];

  // For script execution mode
  char script_mode; // Script execution mode (see ns_vuser_ctx.h file for values)
  int  stack_size;  // Stack size for creation of user context
  char free_stack;  //this is to have control on free stack 

  short vuser_trace_enabled;

  /*Net Diagnostics:BEGIN*/
  //Now G_ENABLE_NET_DIAGNOSTICS is not support any more
  //char enable_net_diagnostic; //Enabling of net diagnostic keyword, here 0:disabling (default) and 1:enabling feature.
  //unsigned short nd_pct; // Percentage in multiple of 100, in 2 decimal precision (integer between 0 and 10000) of users to include the nd header with
  //char net_diagnostic_hdr[32 + 1]; //Header will be send with request if net_diagonstic key enabled. Here maximum string length can be32 and 1 for null.  
  /*Net Diagnostics:END*/

  ntlm_settings_t ntlm_settings;
  kerb_settings_t kerb_settings;

  char enable_network_cache_stats;
  char proxy_proto_mode;//Used for enabling absolute uri to be sent in case of proxy

  //This is for location hdr, we need to save location hdr on any resp code
  //This is done for VISA, In VISA we were getting LOCATION header with 200 OK 
  //resp code and we need to save location hdr.
  char save_loc_hdr_on_all_rsp_code; 
  unsigned short max_pages_per_tx;  // G_MAX_PAGES_PER_TX - Maximum number of page instances allowed to be part of one transaction
  /* Pipelining  */
  int max_pipeline;
  short enable_pipelining;
  //Added new group based keyword G_MAX_CON_PER_VUSER <max_con_mode> <MaxConPerServerHTTP1.0> <MaxConPerServerHTTP1.1> <MaxProxyPerServerHTTP1.0> <MaxProxyPerServerHTTP1.1> <MaxConPerUser>
  short max_con_mode; // Default value 0 and otherwise 1, 1 means browser settings
  int max_con_per_vuser;
  int max_con_per_svr_http1_0;
  int max_con_per_svr_http1_1;
  int max_proxy_per_svr_http1_0;
  int max_proxy_per_svr_http1_1;
  short use_dns; /* from kw: USE_DNS, 0 = no dns, 1=resolve dynamically 2=simulate DNS */
  short dns_conn_type; /* from kw: 0 - UDP, 1 - TCP .... Default is 0(UDP)*/
  short dns_debug_mode; /* USE_DNS debug file enabling*/
  int dns_ttl; /* Time for which resolved host address has to be cached */
  int dns_caching_mode; /* DNS caching mode to unbale and disable caching for resolved hosts */
  int ns_trace_level;

  //To send Tx name in http header
  ns_tx_http_header_settings ns_tx_http_header_s;

  //To end transaction based on NetCache hit
  //ns_end_tx_netcache ns_end_tx_netcache_s;
 
  /* tracing limit parameter */
  int max_trace_param_entries; //max parameter entries in used_param table used in user trace and page dump
  int max_trace_param_value_size;  //max length of parameter values in pagedump (and user trace ?)

  char consider_refresh_hit_as_hit; //do not include refresh hit in the hit graph if value is zero and vice versa.

  int ignore_hash;  // Igonre hash in url. 
                    // 1 - igonre hash
                    // 0 - dont ignore hash 

  /* RBU settings */
  rbu_group_settings_t rbu_gset;
  DynaTraceSettings dynaTraceSettings;
  RampDownMethod rampdown_method;
  int pattern_table_start_idx; 

  char grp_proj_name[MAX_FILE_NAME];         //Group Based project name 
  char grp_subproj_name[MAX_FILE_NAME];      //Group Based sub-project name

  char ssl_ciphers[CIPHER_BUF_MAX_SIZE + 1];   /* SSL_CIPHERS. Moved from global data.*/
  char data_dir[MAX_UNIX_FILE_NAME + 1]; //Data Directory name.
  int min_con_reuse_delay;
  int max_con_reuse_delay;
  int ssl_settings;
  HttpSettings http_settings;
  Http2Settings http2_settings;
  short include_exlclude_relative_url;
  int show_vuser_flow_mode;
  M3u8Settings m3u8_gsettings;
  u_ns_ts_t dns_cache_ttl;
  short ip_version_mode; // this will store ip version either ipv4 , ipv6 or auto .only possible values are 0,1,2. 
  short src_ip_mode; // -1 Primary 0:shared 1: uniq
  short use_same_netid_src;     //USE_SAME_NETID_SRC; 0 means NO, 1 = yes and acc to client, 2 = yes and acc to server
  IP_data *ips;
  int num_ip_entries;  
  Master_Src_Ip_Table *master_src_ip_table;
  int g_max_net_idx;
  short Flag_SRC_IP_LIST;
  GrpSvrHostSettings svr_host_settings;
  jmeter_group_settings jmeter_gset;
  jmeter_schedule_settings jmeter_schset;
  RteSettings rte_settings;
  int num_retry_on_page_failure;
  int retry_interval_on_page_failure;
  int idle_timer;                                        /* idle_timer */
  int response_timer;                                    /* response_timer */
 
  //HTTP Body Checksum header
  HttpBodyCheckSumHdr http_body_chksum_hdr;
  BodyEncryption body_encryption;

  int post_hndshk_auth_mode;
  int ssl_cert_id;
  int ssl_key_id;  
  int tls_version;  
  int ssl_regenotiation; 
  PerGrpStaticHostTable per_grp_static_host_settings; 
  CorrelationIdSettings correlationIdSettings;
  int connect_timeout;
  union {
    runlogicfn_type runlogic_func_ptr;   // For c type script
    char *runlogic_func;            // For java type script
  };
} GroupSettings;

#ifndef CAV_MAIN
extern GroupSettings *group_default_settings;
#else
extern __thread GroupSettings *group_default_settings;
extern GroupSettings *p_group_default_settings;
extern Global_data *p_global_settings;
#endif
typedef struct {

  int warmup_sessions;
  
} ChildGlobalData;

// globals.disable_headers
#define NS_HOST_HEADER 1
#define NS_UA_HEADER 2
#define NS_ACCEPT_HEADER 4
#define NS_ACCEPT_ENC_HEADER 8
#define NS_KA_HEADER 16
#define NS_CONNECTION_HEADER 32
#define NS_NO_HEADER 63

#define URL_REPORT                      0x00000001
#define PAGE_REPORT                     0x00000002
#define TX_REPORT                       0x00000004
#define SESS_REPORT                     0x00000008
#define SMTP_REPORT                     0x00000010
#define POP3_REPORT                     0x00000020
#define FTP_REPORT                      0x00000040
#define DNS_REPORT                      0x00000080
#define STATUS_CODE_REPORT              0x00000100
#define USER_SOCKET_REPORT              0x00000200
#define LDAP_REPORT                     0x00000400
#define IMAP_REPORT                     0x00000800
#define JRMI_REPORT                     0x00001000
#define SVR_IP_REPORT                   0x00002000
#define WS_REPORT                       0x00004000
#define JMETER_REPORT                   0x00008000
#define TCP_CLIENT_REPORT               0x00010000

/*cap_status defs of globals*/
#define NS_CAP_CHK_OFF 0
#define NS_CAP_CHK_INIT 1
#define NS_CAP_CHK_ON 2
#define NS_CAP_OVERLOAD 3

/*default capacity check values*/
#define DEFAULT_CAP_CONSEC_SAMPLES 5
#define DEFAULT_CAP_PCT 20

/*use_host defs of globals */
#define USE_HOST_CONFIG 0
#define USE_HOST_HEADER 1

/*use_dns defs of globals */
/* DNS Mode */
#define USE_DNS_NO 0
#define USE_DNS_DYNAMIC 1
#define USE_DNS_SIMULATE 1
#define USE_DNS_NONBLOCK 2

/* DNS Cache Mode */
#define USE_DNS_CACHE_FOR_SESSION 0
#define USE_DNS_CACHE_DISABLED    1
#define USE_DNS_CACHE_FOR_TTL     2  // Not supported


/*UA types */  /* TODO: MUST FILL IN THE OTHER UA TYPES */
#define UA_OTHER 0
#define UA_IE 1
#define UA_NET 2

#define UA_OTHER_MAX_PARALLEL 8
#define UA_OTHER_SVR_MAX_PARALLEL 8
#define UA_IE_MAX_PARALLEL 8
#define UA_IE_SVR_MAX_PARALLEL 8
#define UA_NET_MAX_PARALLEL 8
#define UA_NET_SVR_MAX_PARALLEL 8
#define UA_MAX_PARALLEL 8    /* This is the max number of connections of all the browser types */
#define UA_SVR_MAX_PARALLEL 8   /* This is the max number of connections per server of all the browser types */

#define NS_NONE			0
#define NS_STATIC_VAR_NEW	1
#define NS_CLUST_VAR_NEW	2
#define NS_NSL_VAR_SCALAR	3
#define NS_NSL_VAR_ARRAY	4
#define NS_TAG_VAR_NEW		5
#define NS_SEARCH_VAR_NEW	6
#define NS_GROUP_VAR_UTF	7
#define NS_GROUP_VAR_LONG	8
#define NS_FINISH_LINE		9
#define NS_CHECKPOINT_NEW	10
// Add - Achint - for global cookie 10/04/2007
#define NS_GLOBAL_COOKIE	11
#define NS_CHECK_REPLY_SIZE_NEW 12
#define NS_RANDOM_VAR_NEW	13
#define NS_INDEX_VAR            14
#define NS_RANDOM_STRING_NEW    15
#define NS_UNIQUE_VAR_NEW       16
#define NS_DATE_VAR_NEW         17
#define NS_JSON_VAR_NEW         18
#define NS_UNIQUE_RANGE_VAR     19
#define NS_SQL_VAR              20
#define NS_CMD_VAR              21

#define NS_ST_VAR_START 1
#define NS_ST_GROUP_START 2
#define NS_ST_NEW_GROUP 3
#define NS_ST_GROUP_TYPE 4
#define NS_ST_GROUP_SEQUENCE 5
#define NS_ST_GROUP_WEIGHTS 6
#define NS_ST_GROUP_VARIABLE 7
#define NS_ST_GROUP_VARIABLE_INPUTTED 8
#define NS_ST_GROUP_VALUE 9
#define NS_ST_GROUP_ANY 10
#define NS_ST_GROUP_DONE 11
#define NS_ST_NSL_VAR 12
#define NS_ST_TAG_VAR 13
#define NS_ST_SEARCH_VAR 14
#define NS_ST_CLUST_VAR 15
#define NS_ST_CLUST_VAL 16
#define NS_ST_CLUST_ANY 17
/* For RDT type scenarios */

#define RDT_MOBILE 10
#define RDT_DESKTOP 11
#define SCRIPT_TYPE_JMETER 100

/* For GroupTableEntry.type */
#define SESSION 1
#define USE 2
#define ONCE 3

/* For GroupTableEntry.sequence */
#define SEQUENTIAL     1
#define RANDOM         2
#define WEIGHTED       3
#define UNIQUE         4
#define USEONCE        5

typedef struct {
  StrEnt MainUrlHeaderBuf;
  StrEnt InlineUrlHeaderBuf;
  StrEnt AllUrlHeaderBuf;
}AddHTTPHeaderList;

typedef struct {
  char name[MAX_NAME_LENGTH];
  int fw_speed;
  int rv_speed;
  int fw_pct_loss;
  int rv_pct_loss;
  int fw_delay;
  int rv_delay;
  int pct;
  int cum_pct;
} User_data;

typedef struct {
  unsigned long ip_addr;
  unsigned short port;
  int num_connections;
  int num_fetches;
  int conf_fd;
  int url_fd;
  int ip_fd;
  int collect_no_eth_data; // If 1, do not collect ETH data.
} Client_data;

#if 0
typedef struct {
  struct sockaddr_in saddr;
  unsigned short server_port;
  char server_hostname[MAX_FILE_NAME];
} Server_data;
#endif

typedef struct LocAttrTableEntry {
  u_ns_ptr_t name; /* offset into big buf */
} LocAttrTableEntry;

typedef struct LineCharTableEntry {
  int source;  /* index into the LocAttrTableEntry */
  int destination; /* index into the LocAttrTableEntry */
  short fw_lat;
  short rv_lat;
  short fw_loss;
  short rv_loss;
} LineCharTableEntry;

typedef struct AccAtttrTableEntry {
  u_ns_ptr_t name; /* offset into big buf */
  int fw_bandwidth;
  int rv_bandwidth;
  short compression;
  short shared_modem;
} AccAttrTableEntry;

typedef struct AccLocTableEntry {
  int userindex_idx;
  int access;   /* index into AccAttrTable */
  int location;   /* index into LocAttrTable */
  short pct;
} AccLocTableEntry;

// screen size table
typedef struct ScreenSizeAttrTableEntry {
  unsigned short width; 
  unsigned short height;
  short pct;
} ScreenSizeAttrTableEntry;

// browser screen size map table 
typedef struct BRScSzMapTableEntry {
  int  screenSize_idx; 
  int  prof_idx; 
  int  browserAttr_idx; 
} BRScSzMapTableEntry;

// profile browser screen size table
typedef struct PfBwScSzTableEntry {
  u_ns_ptr_t prof_idx; /* offset into big buf for profile */
  u_ns_ptr_t brow_idx; /* offset into big buf for browser */
  u_ns_ptr_t scsz_idx; /* index to attribute table */
  unsigned short pct; // screen size pct for thos entry
} PfBwScSzTableEntry;

typedef struct BrowAttrTableEntry {
  u_ns_ptr_t name; /* offset into big buf */
  u_ns_ptr_t UA; /* offset into big buf */
  int ka_timeout;
  int max_con_per_vuser;
  int per_svr_max_conn_http1_0;
  int per_svr_max_conn_http1_1;
  int per_svr_max_proxy_http1_0;
  int per_svr_max_proxy_http1_1;
} BrowAttrTableEntry;

#if 0 

typedef struct MachAttrTableEntru {
  u_ns_ptr_t name; /* offset into big buf */
  int type;
} MachAttrTableEntry;
#endif 

#define MACH_TYPICAL 0
#define MACH_POWERFUL 1
#define MACH_LOWEND 2

#if 0 
typedef struct FreqAttrTableEntry {
  u_ns_ptr_t name; /* offset into big buf */
  int type;
} FreqAttrTableEntry;

#endif 

#define FREQ_VERY_ACTIVE 0
#define FREQ_ACTIVE 1
#define FREQ_INACTIVE 2

typedef struct SessProfTableEntry {
  int sessprofindex_idx; /* index into the sessprofindex table */
  int session_idx; /* index into the session table */
  short pct;
} SessProfTableEntry;

typedef struct SessProfIndexTableEntry {
  unsigned int name; /* offset into big buf */
  int sessprof_start; /* index into sessprof table */
  int sessprof_length;
} SessProfIndexTableEntry;

typedef struct UserProfTableEntry {
  int userindex_idx; /*index into the userindex table */
  int type;
  int attribute_idx; /* index into the attribute table */
  int pct;
} UserProfTableEntry;

#define USERPROF_LOCATION 0
#define USERPROF_ACCESS 1
#define USERPROF_BROWSER 2
#define USERPROF_FREQ 3
#define USERPROF_MACHINE 4

typedef struct UserIndexTableEntry {
  ns_bigbuf_t name; /* offset into big_buf */
  int UPLoc_start_idx; /* index into the user profile table */
  int UPLoc_length;
  int UPAcc_start_idx; /* index into the user profile table */
  int UPAcc_length;
  int UPBrow_start_idx; /* index into the user profile table */
  int UPBrow_length;
  //int UPFreq_start_idx; /* index into the user profile table */
  //int UPFreq_length;
  //int UPMach_start_idx; /* index into the user profile table */
  //int UPMach_length;
  int UPAccLoc_start_idx;  /* index into the AccLocTable */
  int UPAccLoc_length;
  int UPBR_start_idx;  /* index into the AccLocTable */
  int UPBR_length;
} UserIndexTableEntry;

//Add define Achint 12/14/2006
#define URL_BASED_SCRIPT_PRIFIX ".UrlBasedScript_"
typedef struct RunProfTableEntry {
  short num_generator_per_grp; /*Added var to save total number of generator per group*/
  short num_generator_kill_per_grp;
  int userprof_name; //index into temp buf
  int userprof_idx; /* index into the user profile idnex table */
  int sessprof_idx; /* index into the sessprof index table */
  int quantity;
  double percentage;
  ns_bigbuf_t cluster_id;
  ns_bigbuf_t scen_group_name;
  int group_num;
  GroupSettings gset;  
  int pacing_idx;
  int *page_think_table;
  int *inline_delay_table;
  int *continue_onpage_error_table;
  int *override_recorded_think_time_table; 
  int *auto_fetch_table; // Array to index for auto fetch embedded table (index is int)
  int *page_reload_table;  // Array to index for page reload
  int *page_clickaway_table;  // Array to index for page reload
  int num_pages;
  unsigned int start_page_idx; //pointing to gPagetable
  int grp_type;
  int proxy_idx;
  int grp_ns_var_start_idx;
  AddHTTPHeaderList *addHeaders;
  char runlogic[256];
} RunProfTableEntry;

typedef struct RunIndexTableEntry {
  unsigned int name; /* offset into big buf */
  int runprof_start; /* index into the runprof table */
  int runprof_length;
} RunIndexTableEntry;

typedef struct UserProfIndexShrEntry {
  int name; /* offset into big buf */
  int userprofshr_start_idx;
  int userprofshr_length;
} UserProfIndexShrEntry;

typedef struct TestCaseType {
  int mode;
  int guess_type;
  int guess_num;
  int guess_prob; /* 0=L,1=M,2=H */
  int min_steps;  // This specifies the minimum value netstorm can go in case LESS_THAN in SLA, default is 10
  int stab_num_success;
  int stab_max_run;
  int stab_run_time;
  int stab_sessions;
  int stab_goal_pct;
  int target_rate;
  unsigned int stab_delay; // For putting the delay between two consequtive testruns, will in millisecs
} TestCaseType;

#define TC_FIX_CONCURRENT_USERS 0 //FCU
#define TC_FIX_USER_RATE 1     //FSR
#define TC_MIXED_MODE   99
#define TC_FIX_HIT_RATE 2
#define TC_FIX_PAGE_RATE 3
#define TC_FIX_TX_RATE 4
#define TC_MEET_SLA 5
#define TC_MEET_SERVER_LOAD 6
#define TC_FIX_MEAN_USERS 7
#define TC_CALC_CLEANUP_RUN 8
#define TC_REPLAY_ACCESS_LOGS 9

#define GUESS_NUM_USERS 0
#define GUESS_RATE 1

#define NS_GUESS_PROB_LOW 0
#define NS_GUESS_PROB_MED 1
#define NS_GUESS_PROB_HIGH 2
#define DEFAULT_GUESS_MIN_STEPS 10

#define DEFAULT_GUESS_NUM 60
#define DEFAULT_GUESS_PROB NS_GUESS_PROB_LOW

#define DEFAULT_IDLE_MSECS 60000 //changed to msecs according to keyword G_IDLE_MSECS

#define DEFAULT_STAB_NUM_SUCCESS 3
#define DEFAULT_STAB_MAX_RUN 5
#define DEFAULT_STAB_RUN_TIME 120
#define DEFAULT_STAB_GOAL_PCT 10
//#define DEFAULT_STAB_SESSIONS 1

// Default value for testCase.stab_delay
#define DEFAULT_STAB_DELAY 0

typedef struct DynVarTableEntry {
  ns_bigbuf_t name; /* offset into big buf */
} DynVarTableEntry;

typedef struct ReqDynVarTableEntry {
  unsigned int name; /* offset into big buf */
  short length;
} ReqDynVarTableEntry;

typedef struct MetricTableEntry {
  int name;
  int port;
  int qualifier;
  int relation;
  int target_value;
  int min_samples;
  short udp_array_idx;
} MetricTableEntry;

#define METRIC_CPU 0
#define METRIC_PORT 1
#define METRIC_RUN_QUEUES 2
#define METRIC_LESS_THAN 3
#define METRIC_GREATER_THAN 4

typedef struct InuseSvrTableEntry {
  short location_idx;
} InuseSvrTableEntry;

typedef struct InuseUserTableEntry {
  short location_idx;
} InuseUserTableEntry;

typedef struct ThinkProfTableEntry {
  short mode;
  int avg_time;
  int median_time;
  int var_time;
  int rand_gen_idx;
  double a;
  double b;
  u_ns_ptr_t custom_page_think_time_callback; //used to store the callback name in case of CUSTOM_PAGE_THINK_TIME (mode 4)
  custom_page_think_time_fn_type custom_page_think_func_ptr;
} ThinkProfTableEntry;

typedef struct ThinkProfTableEntry_Shr {
  short mode;
  int avg_time;
  int median_time;
  int var_time;
  double a;
  double b;
  char *custom_page_think_time_callback; //used to store the callback name in case of CUSTOM_PAGE_THINK_TIME (mode 4), This is only for Debug log
  custom_page_think_time_fn_type custom_page_think_func_ptr;
  OverrideRecordedThinkTime override_think_time; //This is taken for making OVERRIDE_RECORDED_THIK_TIME mode group based
} ThinkProfTableEntry_Shr;

typedef struct InlineDelayTableEntry {
  short mode;
  int avg_time;
  int median_time;
  int var_time;
  int min_limit_time; // used for specifying the min time limit for INTERNET RANDOM (mode 1)
  int max_limit_time; // used for specifying the max time limit for INTERNET RANDOM (mode 1)
  int rand_gen_idx;
  double a;
  double b;
  int additional_delay_mode1; //used for specifying the additional delay for INTERNET RANDOM (mode 1)
  u_ns_ptr_t custom_delay_callback; //used to store the callback name in case of CUSTOM_DELAY (mode 4)
  custom_delay_fn_type custom_delay_func_ptr;
} InlineDelayTableEntry;

typedef struct InlineDelayTableEntry_Shr {
  short mode;
  int avg_time;
  int median_time;
  int var_time;
  int min_limit_time;
  int max_limit_time;
  double a;
  double b;
  int additional_delay_mode1;
  char *custom_delay_callback; //used to store the callback name in case of CUSTOM_DELAY (mode 4), This is only for Debug log
  custom_delay_fn_type custom_delay_func_ptr;
} InlineDelayTableEntry_Shr;

typedef struct ContinueOnPageErrorTableEntry{
  int continue_error_value;
}ContinueOnPageErrorTableEntry;

typedef struct ContinueOnPageErrorTableEntry_Shr{
  int continue_error_value;
}ContinueOnPageErrorTableEntry_Shr;

typedef struct AutoFetchTableEntry { // for auto fetch embedded
  int auto_fetch_embedded; // for fetch option
} AutoFetchTableEntry;

typedef struct AutoFetchTableEntry_Shr { // for shared memory 
  int auto_fetch_embedded; // for fetch option
} AutoFetchTableEntry_Shr;

typedef struct PacingTableEntry {
  char refresh;
  char retain_param_value;
  char retain_cookie_val;
  short pacing_mode;
  short first_sess;
  short think_mode;
  int time;
  int max_time;
} PacingTableEntry;

typedef struct PacingTableEntry_Shr {
  char refresh;
  char retain_param_value;
  char retain_cookie_val;
  short pacing_mode;
  short first_sess;
  short think_mode;
  int time;
  int max_time;
} PacingTableEntry_Shr;

typedef struct ErrorCodeTableEntry {
  short error_code;
  ns_bigbuf_t error_msg;  /* index into the big buffer */
} ErrorCodeTableEntry;

typedef struct CheckPageTableEntry {
  int checkpoint_idx; /* index into the checkpoint table */
  int page_name; /* index into the temp big buffer */
  int page_idx;
  //int sess_name; /* index into the temp big buffer */
  int sess_idx; //Add by Manish
} CheckPageTableEntry;

typedef struct AddHeaderTableEntry
{
  int groupid;
  int pageid;  
  int mode;
  char headername[MAX_HEADER_NAME_LEN+1];
  char headervalue[MAX_HEADER_VALUE_LEN+1];
}AddHeaderTableEntry;


#if 0
typedef struct {
  unsigned int opcode;
  unsigned int seq_number;
  unsigned int num_active_users;
  unsigned int num_thinking_users;
  unsigned int num_idling_users;
  unsigned int url_avg_time;
  unsigned int url_cumulative_time;
  unsigned int page_avg_time;
  unsigned int page_cumulative_time;
  unsigned int transaction_avg_time;
  unsigned int transaction_cumulative_time;
  unsigned int avg_send_throughput;  /* bytes/sec */
  unsigned int avg_recv_throughput;  /* bytes/sec */
  unsigned int cumul_avg_send_throughput; /* bytes/sec */
  unsigned int cumul_avg_recv_throughput; /* bytes/sec */
} data_point;
#endif

typedef struct ClustVarTableEntry {
  ns_bigbuf_t name;
  int start;
  int length;
} ClustVarTableEntry;

typedef struct ClustValTableEntry {
  int cluster_id;
  int value;
} ClustValTableEntry;

typedef struct ClustTableEntry {
  int cluster_id;
} ClustTableEntry;

//Group Variables
typedef struct GroupVarTableEntry {
  ns_bigbuf_t name;
  int start;
  int length;
} GroupVarTableEntry;

typedef struct GroupValTableEntry {
  int group_id;
  int value;
} GroupValTableEntry;

// Table for storing %age and count of users information
// This table is used to make profile selection accurate
typedef struct ProfilePctCountTable {
  unsigned int pct;      //individual percentage of user
  unsigned int acc_pct;  //accumulative percentage
  unsigned int count;    //count of selection of this profile
} ProfilePctCountTable;

//extern Global_data *global_settings;
extern ChildGlobalData child_global_data;

// Used for G_HTTP_HDR
extern AddHeaderTableEntry* addHeaderTable;

//extern User_data *users;
#ifndef CAV_MAIN
extern IP_data *ips;
extern Client_data *clients;
extern SessTableEntry *gSessionTable;
extern PageTableEntry *gPageTable;
extern WeightTableEntry *weightTable;
extern LocAttrTableEntry *locAttrTable;
extern AccLocTableEntry *accLocTable;
extern AccAttrTableEntry *accAttrTable;
extern BrowAttrTableEntry *browAttrTable;
extern CheckPageTableEntry* checkPageTable;
extern SvrTableEntry *gServerTable;
extern SegTableEntry* segTable;
extern PointerTableEntry *pointerTable;
extern PointerTableEntry *fparamValueTable;
extern VarTableEntry *varTable;
extern GroupTableEntry *groupTable;
extern RepeatBlock* repeatBlock;
extern LineCharTableEntry *lineCharTable;
extern SessProfTableEntry *sessProfTable;
extern SessProfIndexTableEntry *sessProfIndexTable;
extern UserProfTableEntry *userProfTable;
extern UserIndexTableEntry *userIndexTable;
extern RunProfTableEntry *runProfTable;
extern RunIndexTableEntry *runIndexTable;
extern TestCaseType testCase;
extern MetricTableEntry *metricTable;
extern avgtime* reportTable;
extern ReqDynVarTableEntry* reqDynVarTable;
extern ErrorCodeTableEntry* errorCodeTable;
extern DynVarTableEntry* dynVarTable;
extern ThinkProfTableEntry* thinkProfTable;
extern InlineDelayTableEntry* inlineDelayTable;
extern ClustVarTableEntry* clustVarTable;
extern GroupVarTableEntry* groupVarTable;
extern TxTableEntry *txTable;
extern int *tx_hash_to_index_table; 
#else
extern __thread IP_data *ips;
extern __thread Client_data *clients;
extern __thread SessTableEntry *gSessionTable;
extern __thread PageTableEntry *gPageTable;
extern __thread WeightTableEntry *weightTable;
extern __thread LocAttrTableEntry *locAttrTable;
extern __thread AccLocTableEntry *accLocTable;
extern __thread AccAttrTableEntry *accAttrTable;
extern __thread BrowAttrTableEntry *browAttrTable;
extern __thread CheckPageTableEntry* checkPageTable;
extern __thread SvrTableEntry *gServerTable;
extern __thread SegTableEntry* segTable;
extern __thread PointerTableEntry *pointerTable;
extern __thread PointerTableEntry *fparamValueTable;
extern __thread VarTableEntry *varTable;
extern __thread GroupTableEntry *groupTable;
extern __thread RepeatBlock* repeatBlock;
extern __thread LineCharTableEntry *lineCharTable;
extern __thread SessProfTableEntry *sessProfTable;
extern __thread SessProfIndexTableEntry *sessProfIndexTable;
extern __thread UserProfTableEntry *userProfTable;
extern __thread UserIndexTableEntry *userIndexTable;
extern __thread RunProfTableEntry *runProfTable;
extern __thread RunIndexTableEntry *runIndexTable;
extern __thread TestCaseType testCase;
extern __thread MetricTableEntry *metricTable;
extern __thread avgtime* reportTable;
extern __thread ReqDynVarTableEntry* reqDynVarTable;
extern __thread ErrorCodeTableEntry* errorCodeTable;
extern __thread DynVarTableEntry* dynVarTable;
extern __thread ThinkProfTableEntry* thinkProfTable;
extern __thread InlineDelayTableEntry* inlineDelayTable;
extern __thread ClustVarTableEntry* clustVarTable;
extern __thread GroupVarTableEntry* groupVarTable;
extern __thread TxTableEntry *txTable;
extern __thread int *tx_hash_to_index_table; 
#endif
//extern Server_data *servers;
//extern MachAttrTableEntry *machAttrTable;
//extern FreqAttrTableEntry *freqAttrTable;

#ifndef CAV_MAIN
extern char* g_big_buf;
extern char* g_temp_buf;
extern char* big_buf_shr_mem;
extern char g_url_file[MAX_FILE_NAME];
extern char g_conf_file[MAX_SCENARIO_LEN];
extern char g_var_file[MAX_FILE_NAME];
extern char g_sorted_conf_file[MAX_SCENARIO_LEN];
extern char g_testrun[MAX_SCENARIO_LEN];
extern char g_c_file[MAX_FILE_NAME];
extern char g_groupvar_filename[MAX_FILE_NAME];
extern char g_ns_tmpdir[MAX_FILE_NAME];
extern char g_tmp_fname[MAX_FILE_NAME];
extern char g_ns_login_user[MAX_FILE_NAME];
extern int cur_ip_entry;
extern int max_ip_entries;
extern int max_user_entries;
extern int total_user_entries;
extern int total_group_ip_entries;
extern int max_client_entries;
extern int cur_server_entry;
extern int max_pointer_entries;
extern int total_pointer_entries;
#else
extern __thread char* g_big_buf;
extern __thread char* g_temp_buf;
extern __thread char* big_buf_shr_mem;
extern __thread char g_url_file[MAX_FILE_NAME];
extern __thread char g_conf_file[MAX_SCENARIO_LEN];
extern __thread char g_var_file[MAX_FILE_NAME];
extern __thread char g_sorted_conf_file[MAX_SCENARIO_LEN];
extern __thread char g_testrun[MAX_SCENARIO_LEN];
extern __thread char g_c_file[MAX_FILE_NAME];
extern __thread char g_groupvar_filename[MAX_FILE_NAME];
extern __thread char g_ns_tmpdir[MAX_FILE_NAME];
extern __thread char g_tmp_fname[MAX_FILE_NAME];
extern __thread char g_ns_login_user[MAX_FILE_NAME];
extern __thread int cur_ip_entry;
extern __thread int max_ip_entries;
extern __thread int max_user_entries;
extern __thread int total_user_entries;
extern __thread int total_group_ip_entries;
extern __thread int max_client_entries;
extern __thread int cur_server_entry;
extern __thread int max_pointer_entries;
extern __thread int total_pointer_entries;
#endif
#ifndef RMI_MODE
extern char g_det_url_file[MAX_FILE_NAME];
#endif
extern char g_prefix[MAX_FILE_NAME];

extern int ns_init_path(char *bin_path, char *err_msg);

extern int max_server_entries;
extern int total_server_entries;
#ifndef CAV_MAIN
extern int max_page_entries;
extern int total_page_entries;
extern int total_tx_entries;
extern int total_errorcode_entries;
extern DB* var_hash_table;
extern int max_sess_entries;
extern int total_sess_entries;
extern int max_svr_entries;
extern int total_svr_entries;
extern int max_var_entries;
extern int total_var_entries;
extern int total_index_var_entries;
extern int max_group_entries;
extern int total_group_entries;
extern int max_fparam_entries;
extern int total_fparam_entries;
extern int max_weight_entries;
extern int total_weight_entries;
extern int max_locattr_entries;
extern int total_locattr_entries;
extern int max_accattr_entries;
extern int total_accattr_entries;
extern int max_browattr_entries;
extern int total_browattr_entries;
extern int total_repeat_block_entries;
extern int max_machattr_entries;
extern int total_machattr_entries;
extern int max_freqattr_entries;
extern int total_freqattr_entries;
extern int max_sessprof_entries;
extern int total_sessporf_entries;
extern int max_sessprofindex_entries;
extern int total_sessprofindex_entries;
extern int max_userprof_entries;
extern int total_userprof_entries;
extern int max_userindex_entries;
extern int total_userindex_entries;
extern int max_runprof_entries;
extern int total_runprof_entries;
extern int max_runindex_entries;
extern int total_runindex_entries;
extern int max_metric_entries;
extern int total_metric_entries;
extern int max_request_entries;
// It has total (HTTP + HTTPS + SMTP + POP3 + FTP + LDAP + DNS) requests 
extern int total_request_entries;
extern int max_checkpage_entries;
extern int total_checkpage_entries;
extern int total_seg_entries;
extern int max_seg_entries;
extern int total_smtp_request_entries;
extern int total_pop3_request_entries;
extern int total_ftp_request_entries;
extern int total_jrmi_request_entries;
extern int total_dns_request_entries;
extern int total_imap_request_entries;
extern int total_ws_request_entries;
extern int max_host_entries;
extern int total_host_entries;
extern int max_dynvar_entries;
extern int total_dynvar_entries;
extern int max_reqdynvar_entries;
extern int total_reqdynvar_entries;
extern int max_thinkprof_entries;
extern int total_thinkprof_entries;
extern int max_inline_delay_entries;
extern int total_inline_delay_entries;
extern int max_autofetch_entries; // for auto fetch embedded
extern int total_autofetch_entries; //for auto fetch embedded
extern int max_cont_on_err_entries; // for continue on page error
extern int max_recorded_think_time_entries;
extern int total_cont_on_err_entries; // for continue on page error
extern int total_recorded_think_time_entries;
extern int total_socket_request_entries;
extern int max_errorcode_entries;
extern int total_nsvar_entries;
extern int max_nsvar_entries;
extern int max_perpageservar_entries;
extern int max_perpagejsonvar_entries;
extern int max_clustvar_entries;
extern int total_clustvar_entries;
extern int total_clust_entries;
extern int max_groupvar_entries;
extern int total_groupvar_entries;
extern int max_clickaction_entries;
extern int total_clickaction_entries;
extern int total_proxy_svr_entries;
extern int max_proxy_svr_entries;
extern int max_proxy_excp_entries;
extern int max_proxy_ip_interfaces;
extern int total_rbu_domain_entries;
extern int max_rbu_domain_entries;
extern int total_http_resp_code_entries;
extern int max_http_resp_code_entries;
extern int config_file_server_base;
extern int config_file_server_idx;
extern int default_userprof_idx;
extern int total_ssl_cert_key_entries;
extern int max_ssl_cert_key_entries;
extern int total_add_rec_host_entries;
extern int total_active_runprof_entries;
#else
extern __thread int max_page_entries;
extern __thread int total_page_entries;
extern __thread int total_tx_entries;
extern __thread int total_errorcode_entries;
extern __thread DB* var_hash_table;
extern __thread int max_sess_entries;
extern __thread int total_sess_entries;
extern __thread int max_svr_entries;
extern __thread int total_svr_entries;
extern __thread int max_var_entries;
extern __thread int total_var_entries;
extern __thread int total_index_var_entries;
extern __thread int max_group_entries;
extern __thread int total_group_entries;
extern __thread int max_fparam_entries;
extern __thread int total_fparam_entries;
extern __thread int max_weight_entries;
extern __thread int total_weight_entries;
extern __thread int max_locattr_entries;
extern __thread int total_locattr_entries;
extern __thread int max_accattr_entries;
extern __thread int total_accattr_entries;
extern __thread int max_browattr_entries;
extern __thread int total_browattr_entries;
extern __thread int max_pointer_entries;
extern __thread int total_pointer_entries;
extern __thread int total_repeat_block_entries;
extern __thread int max_machattr_entries;
extern __thread int total_machattr_entries;
extern __thread int max_freqattr_entries;
extern __thread int total_freqattr_entries;
extern __thread int max_sessprof_entries;
extern __thread int total_sessporf_entries;
extern __thread int max_sessprofindex_entries;
extern __thread int total_sessprofindex_entries;
extern __thread int max_userprof_entries;
extern __thread int total_userprof_entries;
extern __thread int max_userindex_entries;
extern __thread int total_userindex_entries;
extern __thread int max_runprof_entries;
extern __thread int total_runprof_entries;
extern __thread int max_runindex_entries;
extern __thread int total_runindex_entries;
extern __thread int max_metric_entries;
extern __thread int total_metric_entries;
extern __thread int max_request_entries;
// It has total (HTTP + HTTPS + SMTP + POP3 + FTP + LDAP + DNS) requests 
extern __thread int total_request_entries;
extern __thread int max_checkpage_entries;
extern __thread int total_checkpage_entries;
extern __thread int total_seg_entries;
extern __thread int max_seg_entries;
extern __thread int total_smtp_request_entries;
extern __thread int total_pop3_request_entries;
extern __thread int total_ftp_request_entries;
extern __thread int total_jrmi_request_entries;
extern __thread int total_dns_request_entries;
extern __thread int total_imap_request_entries;
extern __thread int total_ws_request_entries;
extern __thread int max_host_entries;
extern __thread int total_host_entries;
extern __thread int max_dynvar_entries;
extern __thread int total_dynvar_entries;
extern __thread int max_reqdynvar_entries;
extern __thread int total_reqdynvar_entries;
extern __thread int max_thinkprof_entries;
extern __thread int total_thinkprof_entries;
extern __thread int max_inline_delay_entries;
extern __thread int total_inline_delay_entries;
extern __thread int max_autofetch_entries; // for auto fetch embedded
extern __thread int total_autofetch_entries; //for auto fetch embedded
extern __thread int max_cont_on_err_entries; // for continue on page error
extern __thread int max_recorded_think_time_entries;
extern __thread int total_cont_on_err_entries; // for continue on page error
extern __thread int total_recorded_think_time_entries;
extern __thread int total_socket_request_entries;
extern __thread int max_errorcode_entries;
extern __thread int total_nsvar_entries;
extern __thread int max_nsvar_entries;
extern __thread int max_perpageservar_entries;
extern __thread int max_perpagejsonvar_entries;
extern __thread int max_clustvar_entries;
extern __thread int total_clustvar_entries;
extern __thread int total_clust_entries;
extern __thread int max_groupvar_entries;
extern __thread int total_groupvar_entries;
extern __thread int max_clickaction_entries;
extern __thread int total_clickaction_entries;
extern __thread int total_proxy_svr_entries;
extern __thread int max_proxy_svr_entries;
extern __thread int max_proxy_excp_entries;
extern __thread int max_proxy_ip_interfaces;
extern __thread int total_rbu_domain_entries;
extern __thread int max_rbu_domain_entries;
extern __thread int total_http_resp_code_entries;
extern __thread int max_http_resp_code_entries;
extern __thread int config_file_server_base;
extern __thread int config_file_server_idx;
extern __thread int default_userprof_idx;
extern __thread int total_ssl_cert_key_entries;
extern __thread int max_ssl_cert_key_entries;
extern __thread int total_add_rec_host_entries;
extern __thread int total_active_runprof_entries;
#endif

extern int total_sla_entries;

extern int total_ldap_request_entries; //for ldap
//extern int total_http_method;
//extern int max_http_method;

/* It has only SMTP requests used as we need not to load smtp gdf group
 * if no smtp request exists, smpt gdf group is ignored in getGroupNumVectorByID
 */
extern int max_temparrayval_entries;
extern int total_temparrayval_entries;
//extern int total_rungroup_entries;

extern int g_last_acked_sample;
extern u_ns_ts_t g_tcpdump_started_time;

typedef struct LineCharEntry {
  short fw_lat;
  short rv_lat;
  short fw_loss;
  short rv_loss;
} LineCharEntry;

typedef struct LocAttrTableEntry_Shr {
  char* name; /* pointer into shared big buf */
  LineCharEntry* linechar_array;  /* array of the line characteristics */
} LocAttrTableEntry_Shr;

typedef struct AccAtttrTableEntry_Shr {
  char* name; /* pointer into shared big buf */
  int fw_bandwidth;
  int rv_bandwidth;
  short compression;
  short shared_idx;
} AccAttrTableEntry_Shr;

// screen size table
typedef struct ScreenSizeAttrTableEntry_Shr {
  unsigned short width; 
  unsigned short height;
  short pct;
} ScreenSizeAttrTableEntry_Shr;

typedef struct BrowAttrTableEntry_Shr {
  char* name; /* pointer into shared big buf */
  char* UA; /* pointer into shared big buf */
  int ka_timeout;
  int max_con_per_vuser;
  int per_svr_max_conn_http1_0;
  int per_svr_max_conn_http1_1;
  int per_svr_max_proxy_http1_0;
  int per_svr_max_proxy_http1_1;
} BrowAttrTableEntry_Shr;

typedef struct MachAttrTableEntry_Shr {
  char* name; /* pointer into shared big buf */
  int type;
} MachAttrTableEntry_Shr;

typedef struct FreqAttrTableEntry_Shr {
  char* name; /* pointer into shared big buf */
  int type;
} FreqAttrTableEntry_Shr;

typedef struct ServerOrderTableEntry_Shr {
  SvrTableEntry_Shr* server_ptr; /* pointer into the shared gserver table */
} ServerOrderTableEntry_Shr;


/*Table for random variable's shard memory*/
typedef struct RandomVarTableEntry_Shr {
 char* var_name;
 //index into User Variable table 
 int uv_table_idx; 
 int max;
 int min;
 char *format;
 int format_len;
 int refresh;
 int sess_idx; // This is done to keep track of variables per session 
} RandomVarTableEntry_Shr;

typedef struct RandomStringTableEntry_Shr {
 char* var_name;
 //index into User Variable table
 int uv_table_idx;
 int max;
 int min;
 char *char_set;
 int refresh;
 int sess_idx; 
} RandomStringTableEntry_Shr;

typedef struct UniqueVarTableEntry_Shr {
 char* var_name;
 //index into User Variable table
 int uv_table_idx;
 char *format;
 int num_digits;
 int refresh;
 int sess_idx;
} UniqueVarTableEntry_Shr;

/* This struct is used to store the 
 * used second. This struct is used only
 * for unique date var.
 * This struct holds 
 * cur_sec: this is for used current sec. 
 * end_sec: This is for limit the total sec.*/

typedef struct Cav_uniq_date_time_data
{
  int cur_sec;
  int end_sec;
  u_ns_ts_t init_time; //This is starting time
}Cav_uniq_date_time_data;

typedef struct DateVarTableEntry_Shr {
  char* var_name;
  char *format;
  char unique_date;
  int uv_table_idx;
  int format_len;
  int refresh;
  int day_type;
  int day_offset;
  int time_offset;
  int sess_idx;
  int min_days;
  int max_days;
  Cav_uniq_date_time_data *date_time_data;
} DateVarTableEntry_Shr;

typedef struct SegTableEntry_Shr {
  int type;
  short pb_field_number;   //Protobuf Field number 
  short pb_field_type;     //Protobuf Wire Type - int32,
  union {
    PointerTableEntry_Shr* str_ptr;   /* USED IF TYPE=STR; pointer into the shared big buf */
    int fparam_hash_code;
    VarTableEntry_Shr* var_ptr; /* USED IF TYPE=VAR; pointer into the shared variable table */
    /*This is random variable's shared memory.*/
    RandomVarTableEntry_Shr *random_ptr;
    RandomStringTableEntry_Shr *random_str;
    UniqueVarTableEntry_Shr *unique_ptr;
    DateVarTableEntry_Shr *date_ptr;
    int cookie_hash_code;   /* USED IF TYPE=COOKIE_VAR; hash code for the cookie */
    int dynvar_hash_code;   /* USED IF TYPE=DYN_VAR; hash code for the dynvar */
    int bytevar_hash_code;  /* USED IF TYPE=BYTE_VAR; hash code for the bytevar */
    int var_idx;
  } seg_ptr;
  void *data;
} SegTableEntry_Shr;

typedef struct StrEnt_Shr {
  SegTableEntry_Shr* seg_start; /*pointer into the shared seg table */
  int num_entries;
} StrEnt_Shr;

typedef struct ReqDynVarTableEntry_Shr {
  char* name; /* pointer into shared big buf */
  short length;
} ReqDynVarTableEntry_Shr;

typedef struct ReqDynVarTab_Shr {
  ReqDynVarTableEntry_Shr* dynvar_start; /* pointer into the shared dynvar table */
  int num_dynvars;
} ReqDynVarTab_Shr;

/*This table will be used to hold parameter values to use in user trace and page dump*/
typedef struct usedParamTable{
  SegTableEntry_Shr *seg_ptr;
  void *value;
  int length;
  unsigned short cur_seq;
  char type;
  char flag;  //used as free_flag
}usedParamTable;

#define FREE_USED_PARAM_ENTRY 0x01

#define DELTA_USED_PARAM_TABLE_ENTRY 16 /*to fill used param table */

#define NS_HTTP_100_CONTINUE_HDR 0x00000001
#define NS_URL_KEEP_IN_CACHE     0x00000002
#define NS_URL_BODY_AMF0         0x00000004
#define NS_URL_BODY_AMF3         0x00000008
#define NS_URL_CLICK_TYPE        0x00000010
#define NS_MULTIPART             0x00000020
#define NS_FULLY_QUALIFIED_URL   0x00000040
#define NS_XMPP_UPLOAD_FILE      0x00000080
#define NS_HTTP_AUTH_HDR      	 0x00000100

/****** CLICK and SCRIPT example*******************
 *
 * SCRIPT:
 ns_browser ("MacysHomePage",
              "browserurl=http://192.168.1.73:81/test_click.html");

  ns_link ("home_furnishings",
            "type=imageLink",
            "action=click",
            "content=bed & bath",
            attributes =[
               "alt=home furnishings",
               "linkurl=http://www1.macys.com/shop/for-the-home?id=22672&edge=hybrid",
               "tag=img"
            ]
      );

  RESPOSE: 

  li class="first ">
     <a href="http://www1.macys.com/catalog/index.ognc?CategoryID=7495">Bed & Bath</a>
  </li>
*/

typedef struct AddHTTPHeaderList_Shr{
  StrEnt_Shr MainUrlHeaderBuf;
  StrEnt_Shr InlineUrlHeaderBuf;
  StrEnt_Shr AllUrlHeaderBuf;
}AddHTTPHeaderList_Shr;


typedef struct ClickActionTableEntry_Shr{ // click actions as per recorded click script
  StrEnt_Shr att[NUM_ATTRIBUTE_TYPES];     //  attributes table
}ClickActionTableEntry_Shr;

typedef struct {
  char encryption_algo;
  char base64_encode_option;
  char key_size;
  char ivec_size;
  StrEnt_Shr key;
  StrEnt_Shr ivec;
} BodyEncryptionArgs_Shr;

// This struct should have same fields as in http_request (url.h) and in same sequence
typedef struct http_request_Shr {

  /* Entries below can be in any order */
  int pct; /* percentage of request */
  int content_length;

  //url_index is Id of URLs coming from script like 1,2,3
  //and combination of NVM_ID & sequence number for dynamic urls
  unsigned int url_index;   
  StrEnt_Shr url_without_path;
  StrEnt_Shr url;
  StrEnt_Shr hdrs;
  StrEnt_Shr auth_uname;/*Added for NTLM support */
  StrEnt_Shr auth_pwd;/*Added for NTLM support */
  StrEnt_Shr repeat_inline;/*Added for repeat of inline URL */
  StrEnt_Shr* post_ptr;
  int type;     // URL Type- Main, Embedded, Redirect
  char *redirected_url; /* This is malloc'ed to store Request line for the redirect URL. 
                       * The redirect URL is extracted from Location header of the main/embedded url */
  int got_checksum;
  int checksum;
  int url_got_bytes;    //Note: In case of media streaming we are using this variable
  ReqCookTab_Shr cookies;
  ReqDynVarTab_Shr dynvars;
#ifdef RMI_MODE
  ReqByteVarTab_Shr bytevars;
#endif
  unsigned char http_method;  // Get, Post etc
  unsigned char http_method_idx; // Index in array of all HTTP method names. Names have one space after that
  short tx_ratio;
  short rx_ratio;
  short exact;
  int bytes_to_recv;
  int first_mesg_len;
  short keep_conn_flag;
  short body_encoding_flag;
  int header_flags; // This bit mask can be used for multiple header bit set currently used for 100 Continue
  int tx_hash_idx; 
  char *tx_prefix;  //Used to track inline transaction in case of repeat
  // Achint - 02/01/07 - Added for Pre and Post URL Callback
  // Achint End

  char http_version; // 0 for HTTP/1.0 and 1 for HTTP/1.1
  //#Shilpa 16Feb2011 Bug#2037
  //Implementing Client Freshness Constraint
  CacheRequestHeader cache_req_hdr;
  RBU_Param_Shr rbu_param;
  BodyEncryptionArgs_Shr body_encryption_args;

  SockJs_Param_Shr sockjs;
  ProtobufUrlAttr_Shr protobuf_urlattr_shr;
} http_request_Shr;

typedef struct smtp_request_Shr {
  short int num_to_emails;
  short int num_cc_emails;
  short int num_bcc_emails;
  short int num_attachments;

  StrEnt_Shr user_id;
  StrEnt_Shr passwd;
  StrEnt_Shr *to_emails;        /* RCPT TO: */
  StrEnt_Shr *cc_emails;
  StrEnt_Shr *bcc_emails;
  StrEnt_Shr from_email;        /* MAIL FROM:  */
  StrEnt_Shr* body_ptr;
  //StrEnt_Shr subject_idx; // subject line
  //StrEnt_Shr msg_count;
  short int msg_count_min;
  short int msg_count_max;
  StrEnt_Shr hdrs;  // 0 or more hdrs
  StrEnt_Shr* attachment_ptr;
  int authentication_type; //indicates ssl/non-ssl authentication
  int enable_rand_bytes;
  int rand_bytes_min;
  int rand_bytes_max;

} smtp_request_Shr;

typedef struct dns_request_Shr {
  StrEnt_Shr name;		//DNS resource to lookup
  char qtype;		// DNS query type could be -- A|NS|MD|MF|CNAME|SOA|MB|MG|MR|NULL|WKS|MX|PTR||HINFO|MINFO|TXT
  char recursive;		//query is recursive or not
  char proto;           // UDP/TCP
  StrEnt_Shr assert_rr_type;	//whether to assert that atleast one RR of this type was returned, see types above
  StrEnt_Shr assert_rr_data;	//whether to assert that data returned matches the match text
} dns_request_Shr;

typedef struct pop3_request_Shr {
  int pop3_action_type; // indicates if its a stat, list or get command
  int authentication_type; //indicates ssl/non-ssl authentication
  StrEnt_Shr user_id;
  StrEnt_Shr passwd;

} pop3_request_Shr;

typedef struct imap_request_Shr {
  int imap_action_type; // indicates if its a select, list or fetch command
  int authentication_type; //for ssl or simple tcp
  StrEnt_Shr user_id;
  StrEnt_Shr passwd;
  StrEnt_Shr mail_seq;
  StrEnt_Shr fetch_part;

} imap_request_Shr;

typedef struct ftp_request_Shr {
  int ftp_action_type; // indicates if its a RETR or STOR command
  StrEnt_Shr user_id;
  StrEnt_Shr passwd;
  StrEnt_Shr ftp_cmd;
  int num_get_files;
  int file_type;
  StrEnt_Shr *get_files_idx;
  char passive_or_active; // Active or Passive. Default = Passive

} ftp_request_Shr;

typedef struct jrmi_request_Shr {
  int jrmi_protocol; 
  char method[1024];
  char server[1024];
  int port;
  short no_param;
  StrEnt_Shr object_id;
  StrEnt_Shr number;
  StrEnt_Shr count;
  StrEnt_Shr time;
  StrEnt_Shr method_hash;
  StrEnt_Shr operation;
  StrEnt_Shr* post_ptr;
 
} jrmi_request_Shr;

//for ws

typedef struct ws_request_Shr {
  int  conn_id;              /* This ID will indicate websocket connect id */
  StrEnt_Shr uri;            /* Store WebSocket Connect URI path */
  StrEnt_Shr uri_without_path;       /* Store WebSocket Connect URI */
  StrEnt_Shr hdrs;     /* Store index of custom headers  */
  char *origin;         /*origin server*/
  int  opencb_idx;      /* Store index of open_callback function which will be stored on WebSocket_CB_Table */
  int  sendcb_idx;      /* Store index of send_callback function which will be stored on WebSocket_CB_Table */
  int  msgcb_idx;       /* Store index of message_callback function which will stored on WebSocket_CB_Table */
  int  errorcb_idx;     /* Store index of error_callback function which will be stored on WebSocket_CB_Table */
}ws_request_Shr;

typedef struct xmpp_request_Shr {
   char action;
   char user_type;
   char accept_contact;
   int starttls;
   StrEnt_Shr user; // for login id
   StrEnt_Shr password; // for login passwd
   StrEnt_Shr domain; // for login passwd
   StrEnt_Shr sasl_auth_type; // for login passwd
   StrEnt_Shr message; //for message
   StrEnt_Shr file;    // for file
   StrEnt_Shr group; //for group
}xmpp_request_Shr;

//fc2 request
typedef struct fc2_request_Shr {
  StrEnt_Shr uri;           /* Store fc2 Connect URI */
  StrEnt_Shr message;        // for message 
}fc2_request_Shr;

typedef struct ldap_request_Shr {
  int operation;
  int type;
  char user[512];
  char password[512];

  StrEnt_Shr dn;
  StrEnt_Shr username;
  StrEnt_Shr passwd;
  StrEnt_Shr scope;
  StrEnt_Shr filter;
  StrEnt_Shr base;
  StrEnt_Shr deref_aliases;
  StrEnt_Shr time_limit;
  StrEnt_Shr size_limit;
  StrEnt_Shr types_only;
  StrEnt_Shr mode;
  StrEnt_Shr attributes;
  StrEnt_Shr attr_value;
  StrEnt_Shr attr_name;

} ldap_request_Shr;

typedef struct
{
  char len_bytes;                   /*Provide Length Format - Bytes Len-type B(1/2/4/8) and Len-type T(1 - 20)*/
  char len_type;                    /*Provide Length Format - Type (Binary/Text)*/
  char len_endian;                  /*Provide Length Format - Endianness (Big/Little)*/
  char msg_type;                    /*Provide Message Format - Type(text/binary/hex/base64)*/
  char msg_enc_dec;                 /*Provide Message Encoding/Decoding(binary/hex/base64/none)*/
  StrEnt_Shr prefix;                /*Provide Message prefix*/
  StrEnt_Shr suffix;                /*Provide Message suffix*/
}ProtoSocket_MsgFmt_Shr;

typedef struct
{
  char protocol;                     /*Provide socket stream type (TCP/UDP)*/
  char ssl_flag;                     /*Provide SSL is enable or not*/
  int backlog;                       /*Provide max waiting connections*/
  StrEnt_Shr local_host;             /*Provide Local IP:PORT*/
  StrEnt_Shr remote_host;            /*Provide Remote IP:PORT*/
}ProtoSocket_Open_Shr;

typedef struct
{
  ProtoSocket_MsgFmt_Shr msg_fmt;    /*Provide message format specifications*/ 
  StrEnt_Shr buffer;                 /*Provide buffer for send*/  
  long buffer_len;                   /*Provide max length of buffer that can be sent according to len-byte*/
}ProtoSocket_Send_Shr;

typedef struct
{
  ProtoSocket_MsgFmt_Shr msg_fmt;    /*Provide message format specifications*/
  char end_policy;                   /*Provide end policy for reading data*/ 
  char msg_contains_ord;             /*Provide message contains place (Start/End)*/
  char msg_contains_action;          /*Provide what action need to take*/
  int fb_timeout_msec;               /*Provide timeout to first byte*/
  StrEnt_Shr msg_contains;           /*Provide message contains*/
  char *buffer;                      /*Provide buffer for read*/
  long buffer_len;                   /*Provide max length of buffer that can be read according to len-byte*/ 
}ProtoSocket_Recv_Shr;

typedef struct
{
  char operation;                    /* This is common to all Socket APIs - Open+Send+Read so make a difference taking this flag, 
                                        It will used at the time of copying data from non-shared to shared memoy. */
  char flag;                         /*Provide whether socket operation should be take care or not (Enable/Disable socket Operation)*/    
  int timeout_msec;                 /*Max timeout for socket operations - Open/Send/Recv*/
  int norm_id;                       /*It is norm id of Socket API open*/
  char* (*enc_dec_cb)(char*, int, int*);

  union
  {
    ProtoSocket_Open_Shr open;  
    ProtoSocket_Send_Shr send;  
    ProtoSocket_Recv_Shr recv;  
  };
}ProtoSocket_Shr;

/***Start: Bug 79149**************************************************************************/
typedef struct
{
  StrEnt_Shr user;             /**/
  StrEnt_Shr password;             /**/
  StrEnt_Shr domain;            /**/
  StrEnt_Shr host;            /**/
} rdp_Conn_Shr;

typedef struct
{
  int x_pos;               /**/
  int y_pos;
  int x1_pos;               /**/
  int y1_pos;
  int button_type;
  int origin;
} ns_mouse_Shr;


typedef struct
{
  StrEnt_Shr key_value;               /**/
} ns_key_Shr;

typedef struct
{
  int timeout;               /**/
} ns_sync_Shr;

typedef struct
{
  char operation;                    /* This is common to all Socket APIs - Open+Send+Read so make a difference taking this flag, 
                                        It will used at the time of copying data from non-shared to shared memoy. */
  int norm_id;                       /*It is norm id of Socket API open*/

  union
  {
    rdp_Conn_Shr connect; 
    //rdp_Disconn_Shr disconnect;  
    ns_key_Shr   key;
    ns_key_Shr   key_up; 
    ns_key_Shr   key_down;
    ns_key_Shr   type; 
    ns_sync_Shr  sync;
    ns_mouse_Shr mouse_down;
    ns_mouse_Shr mouse_up; 
    ns_mouse_Shr mouse_click; 
    ns_mouse_Shr mouse_double_click; 
    ns_mouse_Shr mouse_move;
    ns_mouse_Shr mouse_drag;  
  };
} rdp_request_Shr;
/***End: Bug 79149**************************************************************************/

/* Page can have more than 1 url in case of http if embedded url exists.
 * If we have more than one urls in a page than we will have an array of action_request_Shr,
 * So to traverse this list we must use increment of action_request_Shr nothing else 
 * (Do not use http_request_Shr, smtp_request_Shr or else )
 */
typedef struct action_request_Shr {
  /* Following entries must be in same order for all protocols */
  /* All entries must be in same order as for non_shared http_request*/
  short request_type; // HTTP or HTTPS
  unsigned long hdr_flags;          /*For Header Info*/
  unsigned long flags;          /*For Feature Info*/

  union {
    SvrTableEntry_Shr* svr_ptr;  /* pointer into shared request table; server_base is NULL if this is valid*/
    GroupTableEntry_Shr* group_ptr; /* pointer into the shared group table */
  } index;

  ServerOrderTableEntry_Shr* server_base; /* pointer into the shared server order Table; can be NULL*/

  // Function names are not used but kept here to make sure size of http_request and http_rerquest_Shr are same

  // removing union of pre_url_fname and schedule_time as pre_url_fname is used
  char pre_url_fname[31 + 1]; 
  u_ns_ts_t schedule_time;

  char post_url_fname[31 + 1];
  preurlfn_type pre_url_func_ptr;
  posturlfn_type post_url_func_ptr;
  int postcallback_rdepth_bitmask;  // added for supporting redirection depth in post url callback

  union {
    http_request_Shr http;
    smtp_request_Shr smtp;
    pop3_request_Shr pop3;
    dns_request_Shr dns;			
    ftp_request_Shr  ftp;
    ldap_request_Shr  ldap;
    imap_request_Shr  imap;
    jrmi_request_Shr  jrmi;
    ws_request_Shr ws;
    xmpp_request_Shr xmpp;
    fc2_request_Shr fc2_req;
    ProtoSocket_Shr socket;
    rdp_request_Shr rdp; /*bug 79149*/
  } proto;

  char is_url_parameterized; // PARAMETERIZED_URL or NOT
  struct action_request_Shr *parent_url_num;
} action_request_Shr;

typedef struct HostTableEntry_Shr {
action_request_Shr* first_url;
  SvrTableEntry_Shr* svr_ptr;
  short num_url;
  struct HostTableEntry_Shr *next;
} HostTableEntry_Shr;

typedef struct PageTableEntry_Shr {
  char* page_name; /* pointer of shared big buf */
  char *flow_name; /* pointer to flow file name of shared bif buf */
  unsigned int page_id;  //Page_id is the running index of pages for all flow files
  unsigned int relative_page_idx; //This will be relative to script  
  unsigned short save_headers; // To keep the track that if this page has a searh parameter having its search from header 
  unsigned short num_eurls;
  action_request_Shr* first_eurl;  /* pointer into shared requests table */
  //fd_set urlset;
  HostTableEntry_Shr *head_hlist;
  HostTableEntry_Shr *tail_hlist;
  ThinkProfTableEntry_Shr *think_prof_ptr;
  //AutoFetchTableEntry_Shr *auto_fetch_ptr;
#ifndef RMI_MODE
  THITableEntry_Shr* thi_table_ptr;
  int num_tag_entries;
#endif
  nextpagefn_type nextpage_func_ptr;
  prepagefn_type prepage_func_ptr;
  PerPageSerVarTableEntry_Shr* first_searchvar_ptr;
  int num_searchvar;
  PerPageJSONVarTableEntry_Shr* first_jsonvar_ptr; //for JSON 
  int num_jsonvar;                                 //for JSON  
  PerPageChkPtTableEntry_Shr* first_checkpoint_ptr;
  int num_checkpoint;
  PerPageCheckReplySizeTableEntry_Shr* first_check_reply_size_ptr;
  int num_check_replysize;
  int tx_table_idx; // Index in the TxTable for the main transaction of this page. Valid only if page as trans is used
  int page_number;
  unsigned int redirection_depth_bitmask; //This bitmaks contains the redirection depth 
  int sp_grp_tbl_idx; //Index into SPGroupTable table - For sync point
  char flags; //currently this flag will be set incase of repeat and delay for inline url
  unsigned int page_norm_id;
  int page_num_relative_to_flow;
} PageTableEntry_Shr;

typedef struct PageTransTableEntry_Shr {
  PageTableEntry_Shr* page_ptr;
} PageTransTableEntry_Shr;

typedef struct TxTableEntry_Shr {
  char* name; /* pointer into the big buf */
  int tx_hash_idx;
  int sp_grp_tbl_idx; //Index into SPGroupTable table - For sync point
} TxTableEntry_Shr;

typedef struct SessTableEntry_Shr {
  char* sess_name; /* pointer into the big buffer */
  char* jmeter_sess_name; /* pointer into the big buffer */
  unsigned int sess_id;//sess_id is the running index for groups in scenario
  unsigned short num_pages;
  short completed;
  PageTableEntry_Shr* first_page; /* pointer into the shared page table */
  initpagefn_type init_func_ptr;
  exitpagefn_type exit_func_ptr;
  runlogicfn_type runlogic_func_ptr; 
  void (*user_test_init)();  //function pointer to user_test_init from util.h
  void (*user_test_exit)();  //function pointer to user_test_exit from util.h
  VarTransTableEntry_Shr *vars_trans_table_shr_mem;
  char* var_type_table_shr_mem;
  int* vars_rev_trans_table_shr_mem;
  int (*var_hash_func)(const char*, unsigned int);
  const char* (*var_get_key)(unsigned int);
  unsigned short numUniqVars;
  //PacingTableEntry_Shr* pacing_ptr;
  ReqCookTab_Shr cookies; // -- Add Achint- For global cookie - 10/04/2007
  char *ctrlBlock;  // Anuj: for runlogic, 21/02/08
  char* proj_name;       /* pointer into the big buffer for project name of session*/
  char* sub_proj_name;   /* pointer into the big buffer for sub-project name of session*/
  int sp_grp_tbl_idx; //Index into SPGroupTable table - For sync point
  int sess_flag;
  int script_type;
  nslb_jsont *rbu_tti_prof_tree;
  //unsigned short num_ws_send;  //for websocket send api
  //PageTableEntry_Shr *ws_first_send;  //for websocket send api
  unsigned int sess_norm_id;
  int num_of_flow_path; 
  int flow_path_start_idx;
  char *rbu_alert_policy;  //keyword RBU_ALERT_POLICY
  int netTest_page_executed;       //number of pages executed per script
  u_ns_ts_t netTest_start_time;    //netTest Script execution Time in ms
  char save_url_body_head_resp;
  int flags;
} SessTableEntry_Shr;

typedef struct SessProfTableEntry_Shr {
  SessTableEntry_Shr* session_ptr; /* pointer into the shared session table */
  short pct;
} SessProfTableEntry_Shr;

typedef struct SessProfIndexTableEntry_Shr {
  char* name; /* pointer into shared big buf */
  SessProfTableEntry_Shr* sessprof_start; /* pointer into shared sessprof table */
  int length;
} SessProfIndexTableEntry_Shr;

typedef struct UserProfTableEntry_Shr {
  int uprofindex_idx;
  LocAttrTableEntry_Shr *location; /* pointer into shared location attr table */
  AccAttrTableEntry_Shr *access; /* pointer into shared access attr table */
  BrowAttrTableEntry_Shr *browser; /* pointer into shared browser attr table */
  //FreqAttrTableEntry_Shr *frequency; /* pointer into shared frequency attr table */
  //MachAttrTableEntry_Shr *machine; /* pointer into machine attr table */
  ScreenSizeAttrTableEntry_Shr *screen_size; /*pointer into screen size table */
  unsigned int pct;
} UserProfTableEntry_Shr;

typedef struct UserProfIndexTableEntry_Shr {
  char* name; /* pointer into shared big buf */
  UserProfTableEntry_Shr* userprof_start; /* pointer to shared userprof table */
  int prof_pct_start_idx;
  int length;
} UserProfIndexTableEntry_Shr;

typedef struct RunProfTableEntry_Shr {
  short num_generator_per_grp; /*Added var to save total number of generator per group*/
  short num_generator_kill_per_grp; /*Added var to save total number of generator killed per group*/
  UserProfIndexTableEntry_Shr* userindexprof_ptr; /* index into the user profile index table */
  union {
      SessProfIndexTableEntry_Shr* sessindexprof_ptr; /* index into the sessprof index table; if USE_SESS_PROF =1 */
      SessTableEntry_Shr* sess_ptr; /* index into the sess table . default */
  };
  int cluster_id;
  int group_num;
  int quantity;
  double percentage;
  char* scen_group_name; /* pointer into the big buffer */   //This should be inside GroupSettings  
  char* cluster_name; /* pointer into the big buffer */
  PacingTableEntry_Shr* pacing_ptr;
  GroupSettings gset;  
  void **page_think_table;
  void **inline_delay_table;
  void **auto_fetch_table; // auto fetch embedded table
  void **continue_onpage_error_table;
  void **override_recorded_think_time_table;
  void **page_reload_table;
  void **page_clickaway_table;
  int num_pages;
  unsigned int start_page_idx;
  int grp_type;
  ProxyServerTable_Shr *proxy_ptr;
  RunningGenValue *running_gen_value;
  int grp_norm_id;
  int total_nsl_var_entries;
  NslVarTableEntry_Shr *nsl_var_table_shr_mem;
  AddHTTPHeaderList_Shr *addHeaders;
} RunProfTableEntry_Shr;

typedef struct RunProfIndexTableEntry_Shr {
  char* name; /* pointer into shared big buf */
  RunProfTableEntry_Shr* runprof_start; /* index into the runprof table */
  int length;
} RunProfIndexTableEntry_Shr;

typedef struct TestCaseType_Shr {
  int mode;
  int guess_type;
  int guess_num;
  int guess_prob;
  int min_steps;
  int stab_num_success;
  int stab_max_run;
  int stab_run_time;
  int stab_sessions;
  int stab_goal_pct;
  int target_rate;
} TestCaseType_Shr;

typedef struct MetricTableEntry_Shr {
  int name;
  int port;
  int qualifier;
  int relation;
  int target_value;
  int min_samples;
  short udp_array_idx;
} MetricTableEntry_Shr;

typedef struct InuseSvrTableEntry_Shr {
  short location_idx;
} InuseSvrTableEntry_Shr;

typedef struct ClustValTableEntry_Shr {
  char* value;
  short length;
} ClustValTableEntry_Shr;

typedef struct GroupValTableEntry_Shr {
  char* value;
  short length;
} GroupValTableEntry_Shr;

typedef struct RepeatBlock_Shr {
  char repeat_count_type;
  int hash_code;
  char *data;
  char *rep_sep;
  int rep_sep_len;
  int repeat_count;
  int num_repeat_segments;
  int agg_repeat_segments;
} RepeatBlock_Shr;

typedef struct status_codes {
  char status_settings;
} status_codes;

#ifndef CAV_MAIN
extern char *file_param_value_big_buf_shr_mem;
extern PointerTableEntry_Shr* pointer_table_shr_mem;
extern WeightTableEntry* weight_table_shr_mem;
extern GroupTableEntry_Shr* group_table_shr_mem;
extern VarTableEntry_Shr* variable_table_shr_mem;
extern VarTableEntry_Shr* index_variable_table_shr_mem;
extern RepeatBlock_Shr* repeat_block_shr_mem;
extern RandomVarTableEntry_Shr *randomvar_table_shr_mem;
extern RandomStringTableEntry_Shr *randomstring_table_shr_mem;
extern DateVarTableEntry_Shr *datevar_table_shr_mem;
extern UniqueVarTableEntry_Shr *uniquevar_table_shr_mem;
extern ServerOrderTableEntry_Shr* serverorder_table_shr_mem;
extern StrEnt_Shr* post_table_shr_mem;
extern SegTableEntry_Shr *seg_table_shr_mem;
extern ClickActionTableEntry_Shr *clickaction_table_shr_mem;
extern action_request_Shr* request_table_shr_mem;
extern HostTableEntry_Shr* host_table_shr_mem;
extern ThinkProfTableEntry_Shr *thinkprof_table_shr_mem;
extern InlineDelayTableEntry_Shr *inline_delay_table_shr_mem;
extern AutoFetchTableEntry_Shr *autofetch_table_shr_mem; // for auto fetch embedded
extern SessTableEntry_Shr* session_table_shr_mem;
extern PacingTableEntry_Shr* pacing_table_shr_mem;
extern ContinueOnPageErrorTableEntry_Shr *continueOnPageErrorTable_shr_mem; 
extern PageTableEntry_Shr* page_table_shr_mem;
extern LocAttrTableEntry_Shr *locattr_table_shr_mem;
extern AccAttrTableEntry_Shr *accattr_table_shr_mem;
extern BrowAttrTableEntry_Shr *browattr_table_shr_mem;
extern FreqAttrTableEntry_Shr *freqattr_table_shr_mem;
extern MachAttrTableEntry_Shr *machattr_table_shr_mem;
extern SessProfTableEntry_Shr *sessprof_table_shr_mem;
extern SessProfIndexTableEntry_Shr *sessprofindex_table_shr_mem;
extern RunProfTableEntry_Shr *runprof_table_shr_mem;
extern MetricTableEntry_Shr *metric_table_shr_mem;
extern InuseSvrTableEntry_Shr *inusesvr_table_shr_mem;
extern UserProfTableEntry_Shr *userprof_table_shr_mem;
extern UserProfIndexTableEntry_Shr *userprofindex_table_shr_mem;
extern RunProfIndexTableEntry_Shr *runprofindex_table_shr_mem;
extern TestCaseType_Shr *testcase_shr_mem;
extern TxTableEntry_Shr* tx_table_shr_mem;
extern int *tx_hash_to_index_table_shr_mem;
#else
extern __thread char *file_param_value_big_buf_shr_mem;
extern __thread PointerTableEntry_Shr* pointer_table_shr_mem;
extern __thread WeightTableEntry* weight_table_shr_mem;
extern __thread VarTableEntry_Shr* variable_table_shr_mem;
extern __thread VarTableEntry_Shr* index_variable_table_shr_mem;
extern __thread GroupTableEntry_Shr* group_table_shr_mem;
extern __thread RepeatBlock_Shr* repeat_block_shr_mem;
extern __thread  RandomVarTableEntry_Shr *randomvar_table_shr_mem;
extern __thread RandomStringTableEntry_Shr *randomstring_table_shr_mem;
extern __thread DateVarTableEntry_Shr *datevar_table_shr_mem;
extern __thread UniqueVarTableEntry_Shr *uniquevar_table_shr_mem;
extern __thread ServerOrderTableEntry_Shr* serverorder_table_shr_mem;
extern __thread StrEnt_Shr* post_table_shr_mem;
extern __thread SegTableEntry_Shr *seg_table_shr_mem;
extern __thread ClickActionTableEntry_Shr *clickaction_table_shr_mem;
extern __thread action_request_Shr* request_table_shr_mem;
extern __thread HostTableEntry_Shr* host_table_shr_mem;
extern __thread ThinkProfTableEntry_Shr *thinkprof_table_shr_mem;
extern __thread InlineDelayTableEntry_Shr *inline_delay_table_shr_mem;
extern __thread AutoFetchTableEntry_Shr *autofetch_table_shr_mem; // for auto fetch embedded
extern __thread SessTableEntry_Shr* session_table_shr_mem;
extern __thread PacingTableEntry_Shr* pacing_table_shr_mem;
extern __thread ContinueOnPageErrorTableEntry_Shr *continueOnPageErrorTable_shr_mem; 
extern __thread PageTableEntry_Shr* page_table_shr_mem;
extern __thread LocAttrTableEntry_Shr *locattr_table_shr_mem;
extern __thread AccAttrTableEntry_Shr *accattr_table_shr_mem;
extern __thread BrowAttrTableEntry_Shr *browattr_table_shr_mem;
extern __thread FreqAttrTableEntry_Shr *freqattr_table_shr_mem;
extern __thread MachAttrTableEntry_Shr *machattr_table_shr_mem;
extern __thread SessProfTableEntry_Shr *sessprof_table_shr_mem;
extern __thread SessProfIndexTableEntry_Shr *sessprofindex_table_shr_mem;
extern __thread RunProfTableEntry_Shr *runprof_table_shr_mem;
extern __thread MetricTableEntry_Shr *metric_table_shr_mem;
extern __thread InuseSvrTableEntry_Shr *inusesvr_table_shr_mem;
extern __thread UserProfTableEntry_Shr *userprof_table_shr_mem;
extern __thread UserProfIndexTableEntry_Shr *userprofindex_table_shr_mem;
extern __thread RunProfIndexTableEntry_Shr *runprofindex_table_shr_mem;
extern __thread TestCaseType_Shr *testcase_shr_mem;
extern __thread TxTableEntry_Shr* tx_table_shr_mem;
extern __thread int *tx_hash_to_index_table_shr_mem;
#endif

extern PageTransTableEntry_Shr *page_trans_table_shr_mem;

#ifndef CAV_MAIN
extern PointerTableEntry_Shr* fparamValueTable_shr_mem;
extern NslVarTableEntry_Shr *nsl_var_table_shr_mem;
extern ProxyServerTable *proxySvrTable;
extern ProxyExceptionTable *proxyExcpTable;
extern ProxyNetPrefix *proxyNetPrefixId;
extern status_codes status_code[STATUS_CODE_ARRAY_SIZE];
extern ProxyServerTable_Shr *proxySvr_table_shr_mem; 
extern ProxyExceptionTable_Shr *proxyExcp_table_shr_mem; 
extern ProxyNetPrefix_Shr *proxyNetPrefix_table_shr_mem;
#else
extern __thread PointerTableEntry_Shr* fparamValueTable_shr_mem;
extern __thread NslVarTableEntry_Shr *nsl_var_table_shr_mem;
extern __thread ProxyServerTable *proxySvrTable;
extern __thread ProxyExceptionTable *proxyExcpTable;
extern __thread ProxyNetPrefix *proxyNetPrefixId;
extern __thread status_codes status_code[STATUS_CODE_ARRAY_SIZE];
extern __thread ProxyServerTable_Shr *proxySvr_table_shr_mem; 
extern __thread ProxyExceptionTable_Shr *proxyExcp_table_shr_mem; 
extern __thread ProxyNetPrefix_Shr *proxyNetPrefix_table_shr_mem;
#endif
//extern ErrorCodeTableEntry_Shr *errorcode_table_shr_mem;
extern ClustValTableEntry_Shr* clust_table_shr_mem;
extern GroupValTableEntry_Shr* rungroup_table_shr_mem;
extern PerGrpSrcIPTable *per_proc_src_ip_table_shr_mem;
extern Master_Src_Ip_Table *master_src_ip_table_shr_mem;
extern IP_data *ips_table_shr_mem; 

extern char** clust_name_table_shr_mem;
extern char** rungroup_name_table_shr_mem;
extern int g_max_script_decl_param;
#define GLOBAL_THINK_IDX 0
#define IS_GLOBAL_THINK(ptr) (!(ptr - &thinkprof_table_shr_mem[GLOBAL_THINK_IDX]))
#define GLOBAL_AUTO_FETCH_IDX 0
#define IS_GLOBAL_AUTOFETCH(ptr) (!(ptr - &autofetch_table_shr_mem[GLOBAL_AUTO_FETCH_IDX]))
#define DEFAULT_CLUST_IDX -1
#define GLOBAL_INLINEDELAY_IDX 0
#define GLOBAL_CONTINUE_PAGE_ERROR_IDX 0
#define GLOBAL_RECORDED_THINK_TIME_IDX 0

struct logging_table_entry {
  int sess_inst_id;
  int pg_id;
  int url_id;
  int req_len;
  int resp_len;
  char status;
  u_ns_ts_t req_start_time;   //Used in RMI Code
  u_ns_ts_t resp_start_time;  //Used in RMI Code
};

extern int gen_ascii_report(int testidx, int child_id);


extern int find_dynvar_idx(char* name);
extern int find_errorcode_idx(int error_code);
extern int find_page_idx(char* name, int sess_idx);
extern int find_page_idx_shr(char* name, SessTableEntry_Shr* sess_ptr);
extern int find_session_idx(char* name);
extern int find_clustvar_idx(char* name);
extern int find_groupvar_idx(char* name);

extern int create_requests_table_entry(int *row_num);
extern int proto_based_init(int row_num, int proto);
extern int create_big_buf_space(void);
extern int create_page_table_entry(int *row_num);
extern int create_sess_table_entry(int *row_num);
extern int create_pointer_table_entry(int *row_num);
//extern int create_pointer_table_entry_ex(int num_values_pre_var);
extern int create_post_table_entry(int *row_num);
extern int create_weight_table_entry(int *row_num);
extern int create_serverorder_table_entry(int *row_num);
extern int create_host_table_entry(int *row_num);
extern int create_dynvar_table_entry(int *row_num);
extern int create_reqdynvar_table_entry(int *row_num);
extern int create_seg_table_entry(int *row_num);
extern int create_sessprofindex_table_entry(int *row_num);
extern int create_sessprof_table_entry(int *row_num);
extern int create_perpagechkpt_table_entry(int *row_num);
extern int create_temparrayval_table_entry(int* row_num);
extern int create_clustervar_table_entry(int* row_num);
extern int create_groupvar_table_entry(int* row_num);
extern ns_bigbuf_t copy_into_big_buf(char* data, ns_bigbuf_t size);
extern int copy_into_temp_buf(char* data, int size);
extern void free_structs(void);
extern int init_userinfo();
extern void free_big_buf();
extern int create_add_header_table_entry(int *rnum);
#define RETRIEVE_BUFFER_DATA(offset) (g_big_buf + offset)
#define RETRIEVE_SHARED_BUFFER_DATA(offset) (big_buf_shr_mem + offset)

#define RETRIEVE_TEMP_BUFFER_DATA(offset) (g_temp_buf + offset)

#define BIG_BUF_MEMORY_CONVERSION(offset) (big_buf_shr_mem + offset)
#define PROXY_EXCP_TABLE_MEMORY_CONVERSION(index) (proxyExcp_table_shr_mem + index)

#define CLEAR_WHITE_SPACE(ptr) {while ((*ptr == ' ') || (*ptr == '\t')) ptr++;}

#define CLEAR_WHITE_SPACE_FROM_END(ptr) { int end_len = strlen(ptr); \
                                          while((end_len > 0) && ((ptr[end_len - 1] == ' ') || (ptr[end_len - 1] == '\t'))) { \
                                            ptr[end_len - 1] = '\0';\
                                            end_len--;\
                                          }\
                                        }
// This macro decreases end_len, so it advisible that pass end_len in local variable
#define CLEAR_WHITE_SPACE_FROM_END_LEN(ptr, end_len) { \
                                          while((end_len > 0) && ((ptr[end_len - 1] == ' ') || (ptr[end_len - 1] == '\t'))) { \
                                            ptr[end_len - 1] = '\0';\
                                            end_len --;\
                                          }\
                                        }
#ifndef  CLEAR_WHITE_SPACE_AND_NEWLINE_FROM_END
#define CLEAR_WHITE_SPACE_AND_NEWLINE_FROM_END(ptr) { int end_len = strlen(ptr); \
                                          while((ptr[end_len - 1] == ' ') || (ptr[end_len - 1] == '\t' )|| (ptr[end_len - 1] == '\n') || (ptr[end_len - 1] == '\r')) { \
                                            ptr[end_len - 1] = '\0';\
                                            end_len = strlen(ptr);\
                                          }\
                                        }
#endif
#define CLEAR_WHITE_SPACE_AND_NEWLINE_FROM_START(ptr, skip_count) { \
                                          skip_count = 0;\
                                          while((ptr[skip_count] == ' ') || (ptr[skip_count] == '\t' )|| (ptr[skip_count] == '\n') || (ptr[skip_count] == '\r')) { \
                                            skip_count++;\
                                          }\
                                        }

#define IGNORE_COMMENTS(ptr) {\
		               if (!g_cmt_found && !strncmp(ptr, "//", 2))\
			       {\
    			         NSDL2_MISC(NULL, NULL, "Ignoring comment = %s", ptr);\
			         continue;\
			       }\
			       else {\
                                if (!strncmp(ptr, "/*", 2))\
			          g_cmt_found = 1;\
				if (g_cmt_found && strstr(ptr, "*/") && strncmp(ptr, "BODY", 4) && strncmp(ptr, "Cookie", 6) && strncmp(ptr, "URL", 3))\
			        {\
                                  g_cmt_found = 0;\
    				  NSDL2_MISC(NULL, NULL, "Ignoring comment = %s", ptr);\
				  continue;\
				}\
				if (g_cmt_found)\
				{\
    				  NSDL2_MISC(NULL, NULL, "Ignoring comment = %s", ptr);\
				  continue;\
				}\
			       }\
		             }	
						
 					

#define CHECK_CHAR(str, constchr, msg)  {if (*str != constchr) { \
                                                 fprintf(stderr, "%s invalid format. Character = '%c'. Expected = '%c'. Source code line number = %d\n", msg, *str, constchr, __LINE__);  \
	                                         exit(-1); } \
	                                      str++;}

#define GET_QUOTED_STRING (line_ptr, msg, data) {\
	CLEAR_WHITE_SPACE(line_ptr);\
	CHECK_CHAR(line_ptr, '"', msg);\
	for (int i = 0 ; line_ptr ; line_ptr++, i++) {\
	  if (*line_ptr == '\\') {\
	    line_ptr++;\
	    if (line_ptr) {\
	      switch (*line_ptr) {\
	      case 'n': data[i] = '\n'; break;\
	      case '\\': data[i] = '\\'; break;\
	      case '"': data[i] = '"'; break;\
	      case 't': data[i] = '\t'; break;\
	      case 'b': data[i] = '\b'; break;\
	      case 'v': data[i] = '\v'; break;\
	      case 'f': data[i] = '\f'; break;\
	      case 'r': data[i] = '\r'; break;\
	      default:\
		fprintf(stderr, "%s Bad format. unrecognised '%s' \n", msg, line_ptr); exit (-1);\
	      }\
	    }\
	  } else if (*line_ptr == '\"') {\
	    data[i] = '\0'; line_ptr++; break;\
	  } else {\
	    data[i] = *line_ptr;\
	  }\
	}\
}

#define CLOSE_INHERITED_EPOLL_FROM_CHILD  \
        close(g_msg_com_epfd);         \
        close(g_dh_msg_com_epfd);      \

#define CLOSE_INHERITED_FD_FROM_CHILD  \
        close(g_msg_com_epfd);         \
        close(g_dh_msg_com_epfd);      \
        close(data_handler_listen_fd); \
        close(listen_fd); \

//Earlier we were getting the value of page_id and sess_id by going through all pages/sessions in a loop
//Now optimized the same by keeping the page_id and sess_id in their resp. structures and directly fetching them.
//Earlier the function name was get_page_id_by_name and get_sess_id_by_name, So the name of these macros are GET_PAGE_ID_BY_NAME and GET_SESS_ID_BY_NAME , butnow these macros will return the current value.
#define GET_PAGE_ID_BY_NAME(vptr) ((vptr->cur_page)?(vptr->cur_page->page_id):-1)
#define GET_SESS_ID_BY_NAME(vptr) (vptr->sess_ptr->sess_id)

//Resolve warning - variable 'xyz' set but not used [-Wunused-but-set-variable]
#ifdef NS_DEBUG_ON
  #define IW_UNUSED(e) e
#else
  #define IW_UNUSED(e)
#endif

#ifdef NS_DEBUG_ON
  #define IW_NDEBUG_UNUSED(l, r) l = r 
#else
  #define IW_NDEBUG_UNUSED(l, r) r 
#endif

extern void granule_data_output(int mode, FILE *rfp, FILE *srfp, avgtime *avg, double *data);

/* granule_data_output modes */
#define NS_HIT_REPORT 1
#define NS_PG_REPORT 2
#define NS_TX_REPORT 3
#define NS_SESS_REPORT 4
#define NS_SMTP_REPORT 5
#define NS_POP3_REPORT 6
#define NS_FTP_REPORT 7
#define NS_DNS_REPORT 8
#define NS_USER_SOCKET_REPORT 9
//#define NS_LDAP_REPORT 10

/* max number of characters in a varialbe name */
#define MAX_VAR_SIZE 1024

/* the truncate number */
#define TRUNCATE_NUMBER 100

#define DEFAULT_RAMP_UP_RATE 120

#define SEG_IS_NOT_REPEAT_BLOCK -1

#define DEFAULT_PROC_PER_CPU 1
extern int read_sess_user_conf_file(char *);
extern void insert_default_svr_location(void);
extern void insert_default_location_values(void);
extern int validate_and_process_user_profile();
extern void copy_structs_into_shared_mem(void);
extern void dump_sharedmem(void);
extern void calculate_ramping_delta();
extern int location_data_compute();
extern int get_profile_idx(int start_idx, int max, int rnd_num);
extern int read_urlvar_file();
extern void update_eth_bytes(void);
extern inline void kill_all_children(char *function_name, int line_num, char *file_name);
extern void * do_shmget(long int, char *);
extern void * do_shmget_with_id(long int, char *, int*);

#ifndef CAV_MAIN
extern unsigned int (*page_hash_func)(const char*, unsigned int);
extern unsigned int (*tx_hash_func)(const char*, unsigned int);
extern char g_project_name[];    // this is add to implement for project dir
extern char g_subproject_name[]; // this is add to implement for sub project dir
extern char g_scenario_name[MAX_SCENARIO_LEN];
extern char g_proj_subproj_name[MAX_FILE_NAME];
#else
extern __thread unsigned int (*page_hash_func)(const char*, unsigned int);
extern __thread unsigned int (*tx_hash_func)(const char*, unsigned int);
extern __thread char g_project_name[];    // this is add to implement for project dir
extern __thread char g_subproject_name[]; // this is add to implement for sub project dir
extern __thread char g_scenario_name[MAX_SCENARIO_LEN];
extern __thread char g_proj_subproj_name[MAX_FILE_NAME];
#endif
//extern unsigned int (*tx_hash_func)(const char*, unsigned int);

extern unsigned long long first_eth_rx_bytes, first_eth_tx_bytes, last_eth_rx_bytes, last_eth_tx_bytes, next_eth_rx_bytes, next_eth_tx_bytes;

//extern int v_cavmodem_fd;
extern int gui_fd;
extern int gui_fd2;
extern FILE *gui_fp;
extern FILE *rtg_fp;

extern int testidx;
extern int start_testidx;
extern int g_testrun_idx;
extern long long g_start_partition_idx;
extern long long g_partition_idx;
extern long long g_first_partition_idx;
extern long long g_prev_partition_idx;
extern long long g_partition_idx_for_generator;
extern int is_test_restarted; 

extern PartitionInfo partitionInfo;

extern int *test_run_number;
///////////////////////////////////
extern int num_processor;
extern int num_server_win_perfmon;
extern char temp_server_unix_rstat[(SERVER_NAME_SIZE + 1) * MAX_SERVER_STATS + 1];
extern char temp_server_win_perfmon[(SERVER_NAME_SIZE + 1)* MAX_SERVER_STATS + 1];

// Added for screen size

// Added for screen size
#ifndef CAV_MAIN
extern int total_sessprof_entries;
extern ScreenSizeAttrTableEntry *scSzeAttrTable;
extern PfBwScSzTableEntry *pfBwScSzTable;
extern BRScSzMapTableEntry *brScSzTable;
extern ScreenSizeAttrTableEntry_Shr *scszattr_table_share_mem; 
extern int max_linechar_entries;
extern int total_linechar_entries;
extern int max_br_sc_sz_entries;
extern int total_br_sc_sz_map_entries;
extern int max_accloc_entries;
extern int total_accloc_entries;
extern int max_screen_size_entries;
extern int total_screen_size_entries;
extern int max_pf_bw_screen_size_entries;
extern int total_pf_bw_screen_size_entries;
extern int max_pacing_entries;
extern int total_pacing_entries;
extern int max_clust_entries;
#else
extern __thread int total_sessprof_entries;
extern __thread ScreenSizeAttrTableEntry *scSzeAttrTable;
extern __thread PfBwScSzTableEntry *pfBwScSzTable;
extern __thread BRScSzMapTableEntry *brScSzTable;
extern __thread ScreenSizeAttrTableEntry_Shr *scszattr_table_share_mem; 
extern __thread int max_linechar_entries;
extern __thread int total_linechar_entries;
extern __thread int max_br_sc_sz_entries;
extern __thread int total_br_sc_sz_map_entries;
extern __thread int max_accloc_entries;
extern __thread int total_accloc_entries;
extern __thread int max_screen_size_entries;
extern __thread int total_screen_size_entries;
extern __thread int max_pf_bw_screen_size_entries;
extern __thread int total_pf_bw_screen_size_entries;
extern __thread int max_pacing_entries;
extern __thread int total_pacing_entries;
extern __thread int max_clust_entries;
#endif


#ifndef CAV_MAIN
extern OverrideRecordedThinkTime* overrideRecordedThinktimeTable; // for override recorded think time
extern ContinueOnPageErrorTableEntry* continueOnPageErrorTable; // for continue on page error
extern ClickActionTableEntry* clickActionTable;
extern AutoFetchTableEntry* autofetchTable; // for auto fetch embedded
extern PacingTableEntry* pacingTable;
extern ClustTableEntry* clustTable;
#else 
extern __thread OverrideRecordedThinkTime* overrideRecordedThinktimeTable; // for override recorded think time
extern __thread ContinueOnPageErrorTableEntry* continueOnPageErrorTable; // for continue on page error
extern __thread ClickActionTableEntry* clickActionTable;
extern __thread AutoFetchTableEntry* autofetchTable; // for auto fetch embedded
extern __thread PacingTableEntry* pacingTable;
extern __thread ClustTableEntry* clustTable;
#endif

///////////////////////////////////
extern key_t shm_base;
extern int total_num_shared_segs;
#ifndef CAV_MAIN
extern char* default_svr_location;
extern int max_svr_group_num;
#else
extern __thread char* default_svr_location;
extern __thread int max_svr_group_num;
#endif
extern int find_locattr_idx(char* name);
extern int find_locattr_shr_idx(char* name);
extern void clear_shared_mem(key_t, int, int);
extern int is_ip_numeric (char *ipaddr);

extern int input_clust_values(char*);
extern int input_group_values(char*);

#define TEN_MILLION 10000000
extern void writeback_static_values();
extern char **server_stat_ip;
extern int no_of_host;
 //Achint 03/01/2007 PERFMON
extern int num_server_unix_rstat;  // Number of servers for getting Unix Rstat perf data
extern inline double get_std_dev (u_ns_8B_t sum_sqr,u_ns_8B_t sum_sample, double avg_time, u_ns_8B_t num_samples);

//NC: In release 3.9.3, TxData has been changed to double pointer now it holds TxData pointers 
//For NetCloud: number of generators + controller.
//For standalone: one pointer
extern TxDataCum **gsavedTxData;
extern void* My_malloc( size_t size );
extern unsigned char my_port_index; /* will remain -1 for parent */
extern unsigned char my_child_index; /* will remain -1 for parent */
extern int find_tagvar_idx(char* name, int len, int sess_idx);
extern void get_server_perf_stats(char *buffer);
extern float convert_to_per_minute (char *buf);
extern void kw_set_mem_perf(char *buf);
//////////////////////////
extern int find_clust_idx(char * cluster_id);
extern int find_userindex_idx(char* name);
extern int create_client_table_entry(int *row_num);
//extern char *get_sess_name_with_proj_subproj(char *sess_name);
extern char *get_sess_name(char *sess_name);
extern char *get_req_type_by_name(int type); 
extern void get_test_completion_time_detail(char *buf);
extern int find_sg_idx(char* scengrp_name);
extern int find_sg_idx_shr(char* scengrp_name);
extern int get_time_from_format(char *str);
extern int create_repeat_block_table_entry(int* row_num);
//common state to string routines for all protocols
extern char *get_request_string();
extern char *proto_to_str();

//#ifdef NS_DEBUG_ON
extern unsigned int get_sess_id_by_name(char *sess_name);
extern unsigned int get_page_id_by_name(SessTableEntry_Shr *sess_ptr, char *page_name);
extern int save_last_data_file();
extern char master_ip[32];
//#endif /* NS_DEBUG_ON */
//
void check_and_set_flag_for_idle_timer();
extern int ns_weibthink_calc(int mean, int median, int variance, double* a, double* b);

extern int kw_set_reader_run_mode (char *buf, int flag, char *err_msg);
extern int kw_set_g_proxy_proto_mode(char *buf, GroupSettings *gset, char *err_msg);
extern inline void init_ns_nvm_scratch_buf();
extern inline void init_status_code_table();

/* RBU */
#ifndef CAV_MAIN
extern char g_rbu_create_performance_trace_dir;
#else
extern __thread char g_rbu_create_performance_trace_dir;
#endif
extern int kw_set_g_rbu(char *buf, GroupSettings *gset, char *err_mag, int runtime_changes);
extern int kw_set_g_rbu_cache_setting(char *buf, GroupSettings *gset, char *err_msg, int runtime_changes);
extern int kw_set_g_rbu_page_loaded_timeout(char *buf, GroupSettings *gset, char *err_msg, int runtime_changes);
extern int kw_set_g_rbu_capture_clips(char *buf, GroupSettings *gset, char *err_mag, int runtime_changes);
extern int kw_set_g_rbu_add_header(char *buf, GroupSettings *gset, char *err_msg, int runtime_changes);
extern int kw_set_g_rbu_clean_up_prof_on_session_start(char *buf, GroupSettings *gset, char *err_msg, int runtime_changes);
extern int kw_set_g_rbu_har_setting(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag);
extern int kw_set_g_rbu_cache_domain(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag);
extern int kw_set_g_rbu_add_nd_fpi(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag);
extern int kw_set_g_rbu_alert_setting(char *buf, GroupSettings *gset, char *err_msg, int runtime_changes);
extern int kw_set_g_rbu_har_timeout(char *buf, GroupSettings *gset, char *err_msg, int runtime_changes);
extern int kw_set_g_rbu_domain_ignore_list(char *buf, GroupSettings *gset, char *err_msg, int runtime_changes);
extern int kw_set_g_rbu_block_url_list(char *buf, GroupSettings *gset, char *err_msg, int runtime_changes);
extern int kw_set_g_rbu_rm_prof_sub_dir(char *buf, GroupSettings *gset, char *err_msg, int runtime_changes);

extern void kw_set_g_ignore_hash(char *buf, GroupSettings *gset, char *err_msg);
extern int kw_set_enable_dt(char *buf, GroupSettings *gset);
extern void autoRecordingDTServer(int startStopDTRecording);
 
extern inline void set_runproftable_start_page_idx(); 
extern inline void set_gpagetable_relative_page_idx();

extern int kw_set_rbu_user_agent(char *buf, GroupSettings *gset, char *err_msg, int runtime_changes);
extern inline int kw_set_tti(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag);
extern int kw_set_rbu_settings_parameter(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag);
extern char *get_sess_name_with_proj_subproj_int(char *sess_name, int grp_idx, char *delim);
extern int kw_set_rbu_screen_size_sim(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag);

extern int kw_set_g_rbu_access_log(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag);
/* normalization */
extern inline void write_buffer_into_csv_file (char *file_name, char *buffer, int size);
extern inline void build_norm_table_from_csv_files();

extern int log_host_table();
extern int get_norm_id_for_page(char *old_new_page_name, char *sess_name, int sess_norm_id);
extern int kw_g_http_body_chksum_hdr(char *buf, GroupSettings *gset, char *err_msg);
extern int kw_set_use_dns(char *text, GroupSettings *gset, char *err_msg, int flag);

extern int kw_set_g_server_host(char *buf, GroupSettings *gset, int grp_idx, char *err_msg, int runtime_changes);
extern int kw_g_body_encryption(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag);
extern void process_cav_memory_map(Msg_com_con *mccptr);
extern void ns_process_cav_memory_map();
extern long ns_get_delay_in_secs(char *date_str);
extern void ns_sleep(timer_type* timer, u_ns_ts_t timeout, TimerProc* cb_func, void *cb_args);

#endif /* UTIL_H */
