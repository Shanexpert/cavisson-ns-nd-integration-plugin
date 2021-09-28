/************************************************************************************************************
 *  Name            : ns_msg_def.h 
 *  Purpose         : This header file includes all the OPCODEs & parent_childs struct moved from util.h
 *  		      Followings are dependent on parent_child struct:
 *  		      	o pause_resume
 *  		      	o Transaction (GUI)
 *
 ***********************************************************************************************************/

#ifndef NS_MSG_DEF_H 
#define NS_MSG_DEF_H

#include <openssl/md5.h>
#include "ns_data_types.h"
#include "ns_common.h"
#include "ns_error_codes.h"


// Opcode which NetStorm Used  -- Starts
#define PROGRESS_REPORT 1
#define FINISH_REPORT 2
#define AVG_RUN_TIME_REPORT 3
#define START_COLLECTING 4
#define READY_TO_COLLECT_DATA 5
#define RAMPUP_MESSAGE 6
#define RAMPUP_DONE_MESSAGE 7
#define WARMUP_MESSAGE 8
#define END_TEST_RUN_MESSAGE 9
#define RAMPDOWN_MESSAGE 10
#define START_MSG_BY_CLIENT 11  // Need to move to util.h
#define START_MSG_BY_CLIENT_DATA 11  
#define FINISH_TEST_RUN_MESSAGE 12 //Graceful finish
#define RAMPDOWN_DONE_MESSAGE 13 
#define COLLECTING_DATA       14
#define EVENT_MSG_LOG             15
#define SAVE_DATA_MSG             16
#define FINISH_TEST_RUN_IMMEDIATE_MESSAGE  17 //Stopping test immediately at generators
#define NSA_LOG_MGR_PORT_CHANGE_MESSAGE 18 //to send nsa_log_mgr port to nvm's
#define NSA_LOGGER_PID_CHANGE_MESSAGE 19 //to send nsa_logger pid change msg to nvm's
#define RTC_PHASE_COMPLETED  20 //NVM will send this opcode when they completed thier RTC phases
#define PROGRESS_REPORT_ACK 21
#define IP_MONITOR_DATA 22 
#define PERCENTILE_REPORT 23 
#define FPARAM_RTC_ATTACH_SHM_MSG 24 
#define FPARAM_RTC_ATTACH_SHM_DONE_MSG 25
#define ATTACH_PDF_SHM_MSG 26
#define END_TEST_RUN_ACK_MESSAGE 27 //Send end_test_ack_msg to master from generator
#define DEBUG_TRACE_MSG 28 //Add to process debug trace msg in case of Netcloud test.
#define GENERATOR_ALERT 29 //Generator alert
#define FINISH_REPORT_ACK 30 //Finish report ack
#define CHILD_REGISTRATION 31 //Registration from Generator to Controller

#define GET_ALL_TX_DATA 101
#define GET_MISSING_TX_DATA 102

#define NS_COMP_CNTRL_MSG 103

#define START_PHASE     105

#define PHASE_COMPLETE    110

#define PAUSE_SCHEDULE  120
#define RESUME_SCHEDULE 130

#define VUSER_TRACE_REQ 131
#define VUSER_TRACE_REP 132

//Sync point related opcodes
#define SP_VUSER_TEST      133//Sent by NVM to test for Syncpoint  
#define SP_VUSER_WAIT      134//Send by Parent if user need to wait at SyncPoint
#define SP_VUSER_CONTINUE  135//Send by Parent if user need not to wait.
#define SP_RELEASE         136//Sent by Parent on completion of SyncPoint.
#define REL_IATO           176
#define REL_OATO           177
#define REL_TARGET_REACHED 178
#define REL_MANUAL         179
#define REL_RTTO           180

//Production Monitoring
#define NEW_TEST_RUN       137
 
//NetCloud Message Communication for RTC
#define RTC_PAUSE                     138
#define RTC_RESUME                    139
#define NC_APPLY_RTC_MESSAGE          140
#define NC_RTC_APPLIED_MESSAGE        141
#define NC_RTC_FAILED_MESSAGE         142
//#define NC_SCHEDULE_DETAIL_RESPONSE   143
#define RTC_PAUSE_DONE                145 //System function pause()
#define RTC_RESUME_DONE               146 //System function pause()
#define STOP_PARTICULAR_GEN           147
#define SHOW_ACTIVE_GENERATOR         148
#define SHOW_ACTIVE_WITH_NVM_GEN      152
#define GET_NVM_DATA_FROM_GEN         153

//Opcode for RTC TOOL
#define APPLY_FPARAM_RTC                 149
#define APPLY_QUANTITY_RTC               150
#define APPLY_MONITOR_RTC                151
#define QUANTITY_PAUSE_RTC               154
#define QUANTITY_RESUME_RTC              155
#define APPLY_PHASE_RTC                  156
#define TIER_GROUP_RTC                   163
#define APPLY_ALERT_RTC                  181

#define TEST_TRAFFIC_STATS               174 // For traffic stats request from tool for NS/NC monitor data`

//Pasue Resume 
#define PAUSE_SCHEDULE_DONE              157
#define RESUME_SCHEDULE_DONE             158

//server data
#define NS_NEW_OBJECT_DISCOVERY          159
#define SRV_IP_MONITOR_DATA              160
#define NS_NEW_OBJECT_DISCOVERY_RESPONSE 161

//Opcode for Pause/Resume User
#define GET_VUSER_SUMMARY                164	
#define GET_VUSER_LIST                   165
#define PAUSE_VUSER                      166
#define RESUME_VUSER                     167
#define STOP_VUSER                       168
#define GET_VUSER_SUMMARY_ACK            169
#define GET_VUSER_LIST_ACK               170
#define PAUSE_VUSER_ACK                  171
#define RESUME_VUSER_ACK                 172
#define STOP_VUSER_ACK                   173

//Opcode for MemoryMap
#define CAV_MEMORY_MAP			 175

//Opcode for SM
#define APPLY_CAVMAIN_RTC                176

// Thread to NVM message opcodes
#define VUTD_INFO_REQ              1000
#define VUTD_INFO_REP              1001

#define NS_API_WEB_URL_REQ         1002
#define NS_API_PAGE_TT_REQ         1003
#define NS_API_START_TX_REQ        1004
#define NS_API_END_TX_REQ          1005
#define NS_API_END_TX_AS_REQ       1006
#define NS_API_GET_TX_TIME_REQ     1007
#define NS_API_SET_TX_STATUS_REQ   1008
#define NS_API_GET_TX_STATUS_REQ   1009
#define NS_API_SYNC_POINT_REQ      1010
#define NS_API_RBU_WEB_URL_END_REQ 1011
#define NS_API_CLICK_ACTION_REQ    1012
#define NS_API_ADVANCE_PARAM_REQ   1013
#define NS_API_USER_DATA_POINT_REQ 1014
#define NS_API_WEBSOCKET_SEND_REQ  1015
#define NS_API_WEBSOCKET_CLOSE_REQ 1016
#define NS_API_WEBSOCKET_READ_REQ  1017
/* Sock JS Close Api not safe in thread mode */
#define NS_API_SOCKJS_CLOSE_REQ    1018
#define NS_API_SOCKET_SEND_REQ     1019
#define NS_API_SOCKET_READ_REQ     1020
#define NS_API_SOCKET_CLOSE_REQ    1021
#define NS_API_CONN_TIMEOUT_REQ    1022
#define NS_API_SEND_TIMEOUT_REQ    1023
#define NS_API_SEND_IA_TIMEOUT_REQ 1024
#define NS_API_RECV_TIMEOUT_REQ    1025
#define NS_API_RECV_IA_TIMEOUT_REQ 1026
#define NS_API_RECV_FB_TIMEOUT_REQ 1027
#define NS_API_XMPP_SEND_REQ       1028
#define NS_API_XMPP_LOGOUT_REQ     1029

#define NS_API_WEB_URL_REP         1051
#define NS_API_PAGE_TT_REP         1052
#define NS_API_START_TX_REP        1053
#define NS_API_END_TX_REP          1054
#define NS_API_END_TX_AS_REP       1055
#define NS_API_GET_TX_TIME_REP     1056
#define NS_API_SET_TX_STATUS_REP   1057
#define NS_API_GET_TX_STATUS_REP   1058
#define NS_API_SYNC_POINT_REP      1059
#define NS_API_RBU_WEB_URL_END_REP 1060
#define NS_API_JMETER_SETUP        1061
#define NS_API_ADVANCE_PARAM_REP   1062
#define NS_API_USER_DATA_POINT_REP 1063
#define NS_API_WEBSOCKET_SEND_REP  1064
#define NS_API_WEBSOCKET_CLOSE_REP 1065
#define NS_API_WEBSOCKET_READ_REP  1066
/* Sock JS Close Api not safe in thread mode */
#define NS_API_SOCKJS_CLOSE_REP    1067
#define NS_API_SOCKET_SEND_REP     1068
#define NS_API_SOCKET_READ_REP     1069
#define NS_API_SOCKET_CLOSE_REP    1070
#define NS_API_CONN_TIMEOUT_REP    1071
#define NS_API_SEND_TIMEOUT_REP    1072
#define NS_API_SEND_IA_TIMEOUT_REP 1073
#define NS_API_RECV_TIMEOUT_REP    1074
#define NS_API_RECV_IA_TIMEOUT_REP 1075
#define NS_API_RECV_FB_TIMEOUT_REP 1076
#define NS_API_XMPP_SEND_REP       1077

#define NS_API_END_SESSION_REQ     1998
#define NS_API_END_SESSION_REP     1999
#define VUTD_STOP_THREAD_REQ       2000
#define VUTD_STOP_THREAD_REP       2001
#define VUTD_STOP_THREAD_FREE      2002 // Thread is done executing session, so release thread and put in thread pool
#define VUTD_STOP_THREAD_EXIT      2003 // Thread is done executing session but got some error in communicating to NVM, so exit thread
// ------------------------------ Ends

/*bug 92660*/
// Parent=>Child opcode
#define NS_START_TEST		0
#define NS_STOP_TEST            1
// Thread=>Child/Parent opcode
#define NS_TEST_STARTED		1
#define NS_TEST_RESULT		2
#define NS_TEST_COMPLETED	3
#define NS_TEST_ERROR		4
#define NS_TEST_STOPPED         5

/*=================================================================== 
  [HINT: NSDynObj]

    enum:DynObjs 
    => Provide Dynamic Object types
    => MAX_DYN_OBJS is used to for implementing loop  
===================================================================*/      
typedef enum
{
  NEW_OBJECT_DISCOVERY_NONE,
  NEW_OBJECT_DISCOVERY_TX, 
  NEW_OBJECT_DISCOVERY_TX_CUM, 
  NEW_OBJECT_DISCOVERY_SVR_IP,
  NEW_OBJECT_DISCOVERY_RBU_DOMAIN,
  NEW_OBJECT_DISCOVERY_STATUS_CODE,
  NEW_OBJECT_DISCOVERY_TCP_CLIENT_FAILURES,
  NEW_OBJECT_DISCOVERY_UDP_CLIENT_FAILURES,
  MAX_DYN_OBJS
}DynObjs;

#define DELTA_DYN_OBJ_RTG_IDX_ENTRIES                 10

#define  MSG_FROM_CMD 99

#define SGRP_NAME_MAX_LENGTH 32

#define CONTROL_MODE 0 //control connection to child
#define DATA_MODE 1   //data connection to child

//Set this flag when data <eg. PROGRESS_REPORT/PERCENTILE_REPORT> to be compressed 
#define MSG_NO_COMPRESSED   0x00
#define MSG_FLAG_COMPRESSED 0x01

/* NOTE: Before adding/removing an element form parent_child struct one must varify with GUI(Transaction Detail) code
 * & all the places where parent_child
 * is used for msg communicationi.
 */

/* child_id   : nvm id or client id 
 ns_version : 3.5.0 ==> 0x00030500
 testidx    : Test run number
*/

//Adding abs_ts
/* msg_len is the length of the complete message excluding msg_len field */
#define MSG_HDR        \
  int msg_len;         \
  int opcode;          \
  union{               \
  int child_id;        \
  int gen_id;          \
  };                   \
  int ns_version;      \
  union{               \
  int gen_rtc_idx;     \
  int gen_loc_idx;     \
  };                   \
  int testidx;         \
  long partition_idx;  \
  double abs_ts;       \
  char msg_flag;

typedef struct {
  /* Following HDR should be same in parent_child and avgtime msg */
  MSG_HDR

  union
  { 
    int num_users; // Number of user for FCU or Session rate/min for FSR in multiple of SESSION_RATE_MULTIPLE
    int operation;
    int rtc_idx;   //This is RTC index
    int shm_id; 
  };

  union
  {
    int data;
    int component;
    int event_logger_port;
    int nsa_logger_pid;
    int num_nvm_per_generator;/*In NC: generators need to send number of NVMs to controller*/
    int rtc_id; //This is used in case of RTC. In RTC this is used as RTC id. 
  };

  union
  {
    int avg_time_size; /* Size of the avg time struct which is send in the progress report */
    int total_pdf_data_size;
  };

  int grp_idx;

  int phase_idx;
  int cum_users; // Total number of user for FCU or Session rate/min for FSR in multiple of SESSION_RATE_MULTIPLE
  int num_killed_nvm;
  int num_process_in_generator;
  //int future2[2]; //To make structre 64 byte
  void *shm_addr; // To store shared memory address
} parent_child;


// This strcture size must be same as parent_child struct because we use this for parent/(child or tool communication)
typedef struct {
  /* Following HDR should be same in parent_child and avgtime msg */
  MSG_HDR

  int time;        // Pause time in seconds if given 
  char cmd_owner[128 + 1];   // Owner of the command
  int msg_from;  // 
  int dummy[6];    // Made dummy make same as parent_child
} Pause_resume;

// This message cannot be more than size of avgtime
#define USER_TRACE_REPLY_SIZE 64000  //updated with 64k as output of RTC message is large
typedef struct {
  /* Following HDR should be same in parent_child and avgtime msg */
  MSG_HDR

  char group_name[4096];
  int cmd_fd;  // connection fd between command and parent. Set by parent
  int reply_status; // Filled by parent if grp is incorrect or by NVM (0 - Pass, 1 - Fail)
  int time;        // Pause time in seconds if given 
  int rtc_qty_flag;
  int first_time;
  union
  {
    char grp_name[SGRP_NAME_MAX_LENGTH + 1];
    int  grp_idx; // Set by parent after grp is checked
    char reply_msg[USER_TRACE_REPLY_SIZE + 1];
  };
} User_trace;

#define MAX_DYNAMIC_OBJECT_NAME_SZ 1024
typedef struct {
  /* Following HDR should be same in parent_child and avgtime msg */
  MSG_HDR
  char type;
  char future[3];
  int local_norm_id;
  int norm_id;
  int dyn_avg_idx;
  int data_len;
  char data[MAX_DYNAMIC_OBJECT_NAME_SZ + 1];  //Should be set acc to global_settings->max_dyn_tx_name_len 
  void *vuser; // Not need. Just for debugging purpose
} Norm_Ids;

/* These enums are for comp control message (used by NS and tool) */
enum {
  COMP_NLM, // nsa_log_mgr (used for events and save data API)
  COMP_NLW  // Logging writter
};

enum {
  COMP_STOP, // stop component
  COMP_START  // start component
};

// This message is reply for ns comp control request from tool. Request message is same as parent_child
typedef struct {
  /* Following HDR should be same in parent_child and avgtime msg */
  MSG_HDR
  char status; 
  char reply_msg[128 + 1];
} ns_comp_ctrl_rep;


#define SP_MAX_DATA_LINE_LEN 256
//Message communication structure for Sync Point
typedef struct SyncPoint_msg {
  MSG_HDR
  int vuser_id;
  int sync_point_id;
  int sync_point_type;
  int grp_idx; 
  int nvm_fd;
#if ( (Fedora && RELEASE >= 14) || (Ubuntu && RELEASE >= 1204) )
  int dummy[1];
  void *vuser; //This is to hold the address of the vusers which being to check by parent.
#else
  int dummy[3];
  void *vuser; //This is to hold the address of the vusers which being to check by parent.
#endif
  /* New fields added for communication between child and parent */
  int sp_rel_step;
  int *sp_rel_step_duration;
  int *sp_rel_step_quantity;
}SP_msg;


//Message communication structure for NS Hourly Monitoring 
typedef struct HourlyMonitoringSess_msg {
  MSG_HDR
}HourlyMonitoringSess_msg;

// This message cannot be more than size of avgtime
#define ERROR_MSG_SIZE 1024
typedef struct {
  /* Following HDR should be same in parent_child and avgtime msg */
  MSG_HDR

  int status; //For error type, 0(other type) 1(Use-once exhausted) , 2(Generator Not Healthy)
  char error_msg[ERROR_MSG_SIZE + 1];
} EndTestRunMsg;

#define TX_NAME_MAX_LEN 1024  // The maximum length of tx_name

typedef struct {
  /* Following HDR should be same in parent_child and avgtime msg */
  MSG_HDR

  int page_id; 
} Ns_web_url_req;

typedef struct {
  /* Following HDR should be same in parent_child and avgtime msg */
  MSG_HDR

  int page_id; 
  int click_action_id; 
} Ns_click_action_req;

typedef struct {
  /* Following HDR should be same in parent_child and avgtime msg */
  MSG_HDR

  void *thread_info; // for pass the address of the Msg_com_con
} Vutd_info;

typedef struct {
  /* Following HDR should be same in parent_child and avgtime msg */
  MSG_HDR

  int ret_val; 
} Ns_api_rep;

typedef struct {
  /* Following HDR should be same in parent_child and avgtime msg */
  MSG_HDR
} Ns_api_req;

// Request for test traffic stats send by the tool and use in the monitor cm_test_traffic_stats
// Currently no addition info is send. We can add more later if needed
typedef struct {
  /* Following HDR should be same in parent_child and avgtime msg */
  MSG_HDR
} TestTrafficStatsReq;

//Structure for Times_data
typedef struct {
  Long_data avg_time;                   // avg time (millisec)
  Long_data min_time;                   // min time (millisec)
  Long_data max_time;                   // max time (millisec)
  Long_data succ;
} Timesdata;
#pragma pack(push, 1)        //Avoid auto padding by gcc
typedef struct
{
  Short_data sample;
  Int_data count;
} SampleCount_data;

typedef struct {
  Long_data url_req;                      // URL requests started/sec
  Long_data url_sent;                      // URL requests sent/sec
  Long_data tries;                        // 'URL Hits' - Total tries in a sampling period
  Long_data succ;                         // 'Success URL Responses' in a sampling period. This value is the value at 0 index of error counters
  Timesdata response;                    // 'Average URL Response Time (Seconds)'
  Timesdata succ_response;               // 'Average Successful URL Response Time (Seconds)'
  Timesdata fail_response;               // 'Average Failure URL Response Time (Seconds)'
  Timesdata dns;                         // 'Average URL DNS lookup Time (Seconds)'
  Timesdata conn;                        // 'Average URL Connect Time(Seconds)'
  Timesdata ssl;                         // 'Average SSL Handshake Time (Seconds)'
  Timesdata frst_byte_rcv;               // 'Average First Byte Recieve Time (Seconds)'
  Timesdata dwnld;                       // 'Average Download Time (Seconds)'
  Long_data cum_tries;                    // Total URLs completed (tried or Hits) since start of test run
  Long_data cum_succ;                     // Total URL Success since start of test run
  SampleCount_data failure;               /*bug  103688  */ // Shows the Number of failures in percentage corresponding to tries and succ 
  Long_data http_body_throughput;
  Long_data tot_http_body;
} HttpResponse;
#pragma pack(pop)

// Response for test traffic stats 
// Progress interval of monitor and NS/NC test can be different.
typedef struct {
  //Following HDR should be same in parent_child and avgtime msg
  MSG_HDR
  int progress_interval;                  // Progress interval in ms
  Long_data abs_timestamp;                // This will specify the rtg time stamp 
  Long_data seq_no;                       // Seq number of the pkt
  HttpResponse http_response;
} TestTrafficStatsRes;



// The name of the some variables in TxData structure has been changed to meet the naming convention. Anuj 19/11/07
// Anil - Why some periodic fields are long long in TxData -> Change to 4B in 3.7.8

// Notes - 
//   - This struct MUST be multiple of 8 byets (Otherewise there will be holes in 64 bits machine)
//   - Also 8 bytes date must be at proper place 

//Periodic Transaction Data
typedef struct {
  u_ns_4B_t tx_fetches_started;    // Number of initiated hits for a Transactions (periodic)
  u_ns_4B_t tx_fetches_completed;       // Number of completed hits for a Transactions (may be failure or sucessfull) (periodic)
  u_ns_4B_t tx_succ_fetches;            // Number of successfull hits for a Transactions (periodic)
  u_ns_4B_t tx_netcache_fetches;        // Number of hits served from netcache for a Transactions (periodic)
  u_ns_4B_t tx_avg_time;                // Avg time taken by a Transactions (periodic)
  u_ns_4B_t tx_min_time;                // Min time taken by a Transactions (periodic)
  u_ns_4B_t tx_max_time;                // Max time taken by a Transactions (periodic)

  u_ns_4B_t tx_succ_min_time;                // Min time taken by a success Transactions (periodic)
  u_ns_4B_t tx_succ_max_time;                // Max time taken by a successTransactions (periodic)

  u_ns_4B_t tx_failure_min_time;                // Min time taken by a failure Transactions (periodic)
  u_ns_4B_t tx_failure_max_time;                // Max time taken by a failure Transactions (periodic)

  u_ns_4B_t tx_netcache_hit_min_time;   // Min time taken by a Transactions (periodic)
  u_ns_4B_t tx_netcache_hit_max_time;   // Max time taken by a Transactions (periodic)

  u_ns_4B_t tx_netcache_miss_min_time;  // Min time taken by a Transactions (periodic)
  u_ns_4B_t tx_netcache_miss_max_time;  // Max time taken by a Transactions (periodic)

  /*In 64 bit system every data time must be start from the index which is multiple of its size*/
  /*Size till here is 48 which is multiple of 8 hence we can start an 8 byte data*/

  u_ns_8B_t tx_tot_time;           // Total time taken by a Transactions (periodic) (Success+Failure)
  u_ns_8B_t tx_tot_sqr_time;       // Square time of the download time for a Transactions , used in calculating "stddev" (periodic)

  u_ns_8B_t tx_succ_tot_time;           // Total time taken by a success Transactions (periodic) (Success)
  u_ns_8B_t tx_succ_tot_sqr_time;      // Square time of the download time for a success Transactions , used in calculating "stddev" (periodic)
  u_ns_8B_t tx_failure_tot_time;           // Total time taken by a failure Transactions (periodic) (Failure)
  u_ns_8B_t tx_failure_tot_sqr_time;     // Square time of the download time for a fail Transactions , used in calculating "stddev" (periodic)

  u_ns_8B_t tx_netcache_hit_tot_time;      // Total time taken by a Transactions (periodic)
  u_ns_8B_t tx_netcache_hit_tot_sqr_time;  // Square time of the download time for a Transactions , used in calculating "stddev" (periodic)

  u_ns_8B_t tx_netcache_miss_tot_time;     // Total time taken by a Transactions (periodic)
  u_ns_8B_t tx_netcache_miss_tot_sqr_time; // Square time of the download time for a Transactions , used in calculating "stddev" (periodic)

  u_ns_8B_t tx_tot_think_time;                 // think time taken by a Transactions (periodic) //Tot will be completed
  u_ns_8B_t tx_tx_bytes;               //total bytes send in the sample period - Kbps (in RTG)
  u_ns_8B_t tx_rx_bytes;               //total bytes recv in the sample period - Kbps (in RTG) 
  u_ns_4B_t tx_min_think_time;                // Min think time taken by a Transactions (periodic)
  u_ns_4B_t tx_max_think_time;                // Max think time taken by a Transactions (periodic)
} TxDataSample;

//Cumulative Transaction Data

typedef struct {
  u_ns_4B_t tx_c_avg_time;         // Avg time taken by a Transactions (cumulative)
  u_ns_4B_t tx_c_min_time;         // Min time taken by a Transactions (cumulative)
  u_ns_4B_t tx_c_max_time;         // Max time taken by a Transactions (cumulative)
  u_ns_8B_t tx_c_fetches_started;  // Number of initiated hits for a Transactions (cumulative)
  u_ns_8B_t tx_c_fetches_completed;// Number of completed hits for a Transactions (may be failure or sucessfull) (cumulative)
  u_ns_8B_t tx_c_succ_fetches;     // Number of successfull hits for a Transactions (cumulative)
  u_ns_8B_t tx_c_netcache_fetches; // Number of hits served from netcache for a Transactions (cumulative)
  u_ns_8B_t tx_c_tot_time;         // Total time taken by a Transactions (cumulative)
  u_ns_8B_t tx_c_tot_sqr_time;     // Square time of the download time for a Transactions , used in calculating "stddev" (cumulative)
} TxDataCum;

typedef struct {
  /* Following HDR should be same in parent_child and avgtime msg */
  MSG_HDR

  int elapsed;   /* This field is also overloaded to contain the port_idx of the child when it sends the "warmup_done" mesg to the parent */
  int complete; /* 0 : Not complete & 1 : Complete */
  int total_server_ip_entries;
  int total_tx_entries;
  int total_rbu_domain_entries;
  int total_http_resp_code_entries;
  int total_tcp_client_failures_entries;
  int total_udp_client_failures_entries;

  /*Added 04/30/03*/
  int num_connections; //current
  int smtp_num_connections; //current
  int pop3_num_connections;
  int dns_num_connections; //current

  //Users
  int cur_vusers_active;
  int cur_vusers_thinking;
  int cur_vusers_waiting;
  int cur_vusers_cleanup;
  int cur_vusers_in_sp;     // Current number of Vusers in syncpoint from all the used syncpoints.
  int cur_vusers_blocked;   // FCS vusers in queue/pool 
  int cur_vusers_paused;    
  int running_users;            // Total running users

  u_ns_4B_t bind_sock_fail_min;
  u_ns_4B_t bind_sock_fail_max;
  u_ns_4B_t bind_sock_fail_tot;

  u_ns_8B_t cum_user_ms;
  u_ns_8B_t total_cum_user_ms;

  //Bytes
  u_ns_8B_t total_bytes;
  u_ns_8B_t tx_bytes;
  u_ns_8B_t rx_bytes;

  /* SMTP */
  u_ns_8B_t smtp_total_bytes;
  u_ns_8B_t smtp_tx_bytes;
  u_ns_8B_t smtp_rx_bytes;

  /* POP3 */
  u_ns_8B_t pop3_total_bytes;
  u_ns_8B_t pop3_tx_bytes;
  u_ns_8B_t pop3_rx_bytes;

  /* DNS */
  u_ns_8B_t dns_total_bytes;
  u_ns_8B_t dns_tx_bytes;
  u_ns_8B_t dns_rx_bytes;

  //eth data - overloaded, in progress report, these are per period, in finish they for whole test run
  u_ns_8B_t eth_rx_bps;
  u_ns_8B_t eth_tx_bps;
  u_ns_8B_t eth_rx_pps;
  u_ns_8B_t eth_tx_pps;
  //connections
  u_ns_8B_t num_con_initiated;   //Number of initiated requests for making the connection (periodic)
  u_ns_8B_t num_con_succ;        //Number of succ requests for making the connection out of initiated (periodic) 
  u_ns_8B_t num_con_fail;        //Number of failures requests for making the connection out of initiated (periodic)
  u_ns_8B_t num_con_break;       //Number of initiated requests for closing the connection (periodic)

  /* SMTP */
  u_ns_8B_t smtp_num_con_initiated;   //Number of initiated requests for making the connection (periodic)
  u_ns_8B_t smtp_num_con_succ;        //Number of succ requests for making the connection out of initiated (periodic) 
  u_ns_8B_t smtp_num_con_fail;        //Number of failures requests for making the connection out of initiated (periodic)
  u_ns_8B_t smtp_num_con_break;       //Number of initiated requests for closing the connection (periodic)


  /* POP3 */
  u_ns_8B_t pop3_num_con_initiated;   //Number of initiated requests for making the connection (periodic)
  u_ns_8B_t pop3_num_con_succ;        //Number of succ requests for making the connection out of initiated (periodic) 
  u_ns_8B_t pop3_num_con_fail;        //Number of failures requests for making the connection out of initiated (periodic)
  u_ns_8B_t pop3_num_con_break;       //Number of initiated requests for closing the connection (periodic)


  /* DNS */
  u_ns_8B_t dns_num_con_initiated;   //Number of initiated requests for making the connection (periodic)
  u_ns_8B_t dns_num_con_succ;        //Number of succ requests for making the connection out of initiated (periodic) 
  u_ns_8B_t dns_num_con_fail;        //Number of failures requests for making the connection out of initiated (periodic)
  u_ns_8B_t dns_num_con_break;       //Number of initiated requests for closing the connection (periodic)


  u_ns_8B_t ssl_new;
  u_ns_8B_t ssl_reused;
  u_ns_8B_t ssl_reuse_attempted;
  /*04/30/03 addition complete */
  u_ns_8B_t num_hits;
  u_ns_8B_t num_tries;
  u_ns_8B_t fetches_started;
  u_ns_8B_t fetches_sent;

  //Success
  u_ns_8B_t avg_time;
  u_ns_4B_t min_time;
  u_ns_4B_t max_time;
  u_ns_8B_t tot_time;

  //Overall
  u_ns_8B_t url_overall_avg_time;
  u_ns_4B_t url_overall_min_time;
  u_ns_4B_t url_overall_max_time;
  u_ns_8B_t url_overall_tot_time;

  //Failed
  u_ns_8B_t url_failure_avg_time;
  u_ns_4B_t url_failure_min_time;
  u_ns_4B_t url_failure_max_time;
  u_ns_8B_t url_failure_tot_time;

  //DNS
  u_ns_4B_t url_dns_min_time;
  u_ns_4B_t url_dns_max_time;
  u_ns_8B_t url_dns_tot_time;
  u_ns_8B_t url_dns_count;

  //Connect
  u_ns_4B_t url_conn_min_time;
  u_ns_4B_t url_conn_max_time;
  u_ns_8B_t url_conn_tot_time;
  u_ns_8B_t url_conn_count;

  //SSL
  u_ns_4B_t url_ssl_min_time;
  u_ns_4B_t url_ssl_max_time;
  u_ns_8B_t url_ssl_tot_time;
  u_ns_8B_t url_ssl_count;

  //First Byte Recvd
  u_ns_4B_t url_frst_byte_rcv_min_time;
  u_ns_4B_t url_frst_byte_rcv_max_time;
  u_ns_8B_t url_frst_byte_rcv_tot_time;
  u_ns_8B_t url_frst_byte_rcv_count;

  //Download
  u_ns_4B_t url_dwnld_min_time;
  u_ns_4B_t url_dwnld_max_time;
  u_ns_8B_t url_dwnld_tot_time;
  u_ns_8B_t url_dwnld_count;

  // SMTP
  u_ns_8B_t smtp_num_hits;
  u_ns_8B_t smtp_num_tries;
  u_ns_8B_t smtp_avg_time;
  u_ns_4B_t smtp_min_time;
  u_ns_4B_t smtp_max_time;
  u_ns_8B_t smtp_tot_time;
  u_ns_8B_t smtp_fetches_started;

  /* POP3 */
  u_ns_8B_t pop3_num_hits;
  u_ns_8B_t pop3_num_tries;

  //Success
  u_ns_8B_t pop3_avg_time;
  u_ns_4B_t pop3_min_time;
  u_ns_4B_t pop3_max_time;
  u_ns_8B_t pop3_tot_time;

  u_ns_8B_t pop3_fetches_started;
  u_ns_8B_t cum_pop3_fetches_started;

  //Overall
  u_ns_8B_t pop3_overall_avg_time;
  u_ns_4B_t pop3_overall_min_time;
  u_ns_4B_t pop3_overall_max_time;
  u_ns_8B_t pop3_overall_tot_time;

  //Failed
  u_ns_8B_t pop3_failure_avg_time;
  u_ns_4B_t pop3_failure_min_time;
  u_ns_4B_t pop3_failure_max_time;
  u_ns_8B_t pop3_failure_tot_time;

  /* DNS */
  u_ns_8B_t dns_num_hits;
  u_ns_8B_t dns_num_tries;

  u_ns_8B_t dns_avg_time;          //For Success DNS Avg Time
  u_ns_4B_t dns_min_time;          //For Success DNS Avg Time  
  u_ns_4B_t dns_max_time;          //For Success DNS Avg Time
  u_ns_8B_t dns_tot_time;          //For Success DNS Avg Time

  u_ns_8B_t dns_fetches_started;

  //Overall
  u_ns_8B_t dns_overall_avg_time;
  u_ns_4B_t dns_overall_min_time;
  u_ns_4B_t dns_overall_max_time;
  u_ns_8B_t dns_overall_tot_time;

  //Failed
  u_ns_8B_t dns_failure_avg_time;
  u_ns_4B_t dns_failure_min_time;
  u_ns_4B_t dns_failure_max_time;
  u_ns_8B_t dns_failure_tot_time;

  //Page Stats
  u_ns_8B_t pg_succ_fetches;
  u_ns_8B_t pg_hits;
  u_ns_8B_t pg_tries;
  u_ns_8B_t pg_avg_time;
  u_ns_4B_t pg_min_time;
  u_ns_4B_t pg_max_time;
  u_ns_8B_t pg_tot_time;
  u_ns_8B_t pg_succ_avg_resp_time;
  u_ns_4B_t pg_succ_min_resp_time;
  u_ns_4B_t pg_succ_max_resp_time;
  u_ns_8B_t pg_succ_tot_resp_time;
  u_ns_8B_t pg_fail_avg_resp_time;
  u_ns_4B_t pg_fail_min_resp_time;
  u_ns_4B_t pg_fail_max_resp_time;
  u_ns_8B_t pg_fail_tot_resp_time;
  u_ns_8B_t pg_fetches_started;

  u_ns_4B_t page_js_proc_time_min;          //Minimum time taken to process the JS
  u_ns_4B_t page_js_proc_time_max;          //Maximum time taken to process the JS
  u_ns_8B_t page_js_proc_time_tot;

  u_ns_4B_t page_proc_time_min;             //Minimum time taken to process the PAGE
  u_ns_4B_t page_proc_time_max;	            //Maximum time taken to process the PAGE
  u_ns_8B_t page_proc_time_tot;

  //TX stats
  u_ns_8B_t tx_succ_fetches;
  u_ns_8B_t tx_fetches_completed;
  u_ns_8B_t tx_avg_time;      //Why we need this?, why 8B?
  u_ns_4B_t tx_min_time;      //why 8B ?
  u_ns_4B_t tx_max_time;      //why 8B ?
  u_ns_8B_t tx_tot_time;
  u_ns_8B_t tx_fetches_started;
  //Sqr elements are added for calcing std dev
  /*
 	n = 0
	sum = 0
	sum_sqr = 0

foreach x in data:
  n = n + 1
  sum = sum + x
  sum_sqr = sum_sqr + x*x
end for

	mean = sum/n
	variance = (sum_sqr - sum*mean)/(n - 1)  */
  u_ns_8B_t tx_tot_sqr_time;
  u_ns_8B_t tx_c_tot_sqr_time;

  u_ns_8B_t tx_succ_tot_resp_time;     
  u_ns_8B_t tx_succ_avg_resp_time;                      //Tx Success Response time
  u_ns_4B_t tx_succ_min_resp_time;                  //Tx Success Response min time
  u_ns_4B_t tx_succ_max_resp_time;                  //Tx Success Response max time

  u_ns_8B_t tx_fail_tot_resp_time;  
  u_ns_8B_t tx_fail_avg_resp_time;                      //Tx Failure Response time
  u_ns_4B_t tx_fail_min_resp_time;                  //Tx Failure Response min time
  u_ns_4B_t tx_fail_max_resp_time;                  //Tx Failure Response max time

  u_ns_8B_t tx_tot_think_time;                      //Tx think time
  u_ns_4B_t tx_min_think_time;                  //Tx think min time
  u_ns_4B_t tx_max_think_time;                  //Tx think max time
  //Sess Sats
  //u_ns_8B_t sess_succ_fetches;
  //u_ns_8B_t sess_fetches_completed;
  u_ns_8B_t sess_hits;
  u_ns_8B_t sess_tries;
  u_ns_8B_t sess_avg_time;
  u_ns_4B_t sess_min_time;
  u_ns_4B_t sess_max_time;
  u_ns_8B_t sess_tot_time;
  u_ns_8B_t ss_fetches_started;

  u_ns_8B_t sess_succ_avg_resp_time;
  u_ns_4B_t sess_succ_min_resp_time;
  u_ns_4B_t sess_succ_max_resp_time;
  u_ns_8B_t sess_succ_tot_resp_time;

  u_ns_8B_t sess_fail_avg_resp_time;
  u_ns_8B_t tx_rx_bytes;               //total bytes recv in the sample period - Kbps (in RTG)
  u_ns_8B_t tx_tx_bytes;               //total bytes send in the sample period - Kbps (in RTG)
  u_ns_4B_t sess_fail_min_resp_time;
  u_ns_4B_t sess_fail_max_resp_time;
  u_ns_8B_t sess_fail_tot_resp_time;

  u_ns_4B_t response[MAX_GRANULES+1];
  u_ns_4B_t smtp_response[MAX_GRANULES+1];
  u_ns_4B_t pop3_response[MAX_GRANULES+1];
  u_ns_4B_t dns_response[MAX_GRANULES+1];
  u_ns_4B_t pg_response[MAX_GRANULES+1];
  u_ns_4B_t sess_response[MAX_GRANULES+1];
  u_ns_4B_t tx_response[MAX_GRANULES+1];
  u_ns_4B_t url_error_codes[TOTAL_URL_ERR];   /* for url error codes 1-8 */
  u_ns_4B_t smtp_error_codes[TOTAL_URL_ERR];   /* for url error codes 1-8 */
  u_ns_4B_t pop3_error_codes[TOTAL_URL_ERR];   /* for url error codes 1-8 */
  u_ns_4B_t dns_error_codes[TOTAL_URL_ERR];   /* for url error codes 1-8 */
  u_ns_4B_t pg_error_codes[TOTAL_PAGE_ERR];   /* for page error codes 16-63 */
  u_ns_4B_t tx_error_codes[TOTAL_TX_ERR];   /* for tx error codes 2-15 */
  u_ns_4B_t sess_error_codes[TOTAL_SESS_ERR];   /* for tx error codes 2-15 */

  u_ns_4B_t dummy2; // Added for the hole getting created in FC14
  /* bug 70480 num_srv_push added in avgtime*/
  u_ns_4B_t num_srv_push;

} avgtime;

typedef struct {
  MSG_HDR
}MsgHdr;

typedef struct {
  /* Following HDR should be same in parent_child and avgtime msg */
  MSG_HDR
  /* PDF data will be after hdr */
}PercentileMsgHdr;

typedef struct {
    union {
	parent_child internal;
	avgtime avg;
        //Percentile pct;
    } top;
} parent_msg;


typedef struct {

  //Bytes
  u_ns_8B_t c_tot_total_bytes;  
  u_ns_8B_t c_tot_tx_bytes;
  u_ns_8B_t c_tot_rx_bytes;
  
  //connections
  u_ns_8B_t c_num_con_initiated; //Number of initiated requests for making the connection(cumulative)
  u_ns_8B_t c_num_con_succ;      //Number of succ requests for making the connection out of initiated(cumulative) 
  u_ns_8B_t c_num_con_fail;      //Number of fail requests for making the connection out of initiated(cumulative) 
  u_ns_8B_t c_num_con_break;     //Number of initiated requests for closing the connection(cumulative)
 
  u_ns_8B_t c_ssl_new;
  u_ns_8B_t c_ssl_reused;
  u_ns_8B_t c_ssl_reuse_attempted;
  u_ns_8B_t url_succ_fetches;
  u_ns_8B_t url_fetches_completed;
  u_ns_8B_t c_min_time;                  //Overall url time - url_overall_min_time
  u_ns_8B_t c_max_time;                  //Overall url time - url_overall_max_time             
  u_ns_8B_t c_tot_time;                  //Overall url time - url_overall_tot_time
  u_ns_8B_t c_avg_time;                  //Overall url time - url_overall_avg_time
  u_ns_8B_t cum_fetches_started;
  
  /* SMTP */
  u_ns_8B_t smtp_c_num_con_initiated; //Number of initiated requests for making the connection(cumulative)
  u_ns_8B_t smtp_c_num_con_succ;      //Number of succ requests for making the connection out of initiated(cumulative) 
  u_ns_8B_t smtp_c_num_con_fail;      //Number of fail requests for making the connection out of initiated(cumulative) 
  u_ns_8B_t smtp_c_num_con_break;     //Number of initiated requests for closing the connection(cumulative)
  u_ns_8B_t smtp_succ_fetches;
  u_ns_8B_t smtp_fetches_completed;
  u_ns_8B_t smtp_c_min_time;
  u_ns_8B_t smtp_c_max_time;
  u_ns_8B_t smtp_c_tot_time;
  u_ns_8B_t smtp_c_avg_time;             //PP
  u_ns_8B_t cum_smtp_fetches_started;
  u_ns_8B_t smtp_c_tot_total_bytes;
  u_ns_8B_t smtp_c_tot_tx_bytes;
  u_ns_8B_t smtp_c_tot_rx_bytes;

  /* pop3 */
  u_ns_8B_t pop3_c_num_con_initiated; //Number of initiated requests for making the connection(cumulative)
  u_ns_8B_t pop3_c_num_con_succ;      //Number of succ requests for making the connection out of initiated(cumulative) 
  u_ns_8B_t pop3_c_num_con_fail;      //Number of fail requests for making the connection out of initiated(cumulative) 
  u_ns_8B_t pop3_c_num_con_break;     //Number of initiated requests for closing the connection(cumulative)
  u_ns_8B_t pop3_succ_fetches;
  u_ns_8B_t pop3_fetches_completed;
  u_ns_8B_t pop3_c_min_time;                  //Overall pop3 time - pop3_overall_min_time
  u_ns_8B_t pop3_c_max_time;                  //Overall pop3 time - pop3_overall_max_time
  u_ns_8B_t pop3_c_tot_time;                  //Overall pop3 time - pop3_overall_tot_time
  u_ns_8B_t pop3_c_avg_time;                  //Overall pop3 time - pop3_overall_avg_time
  u_ns_8B_t cum_pop3_fetches_started;
  u_ns_8B_t pop3_c_tot_total_bytes;
  u_ns_8B_t pop3_c_tot_tx_bytes;
  u_ns_8B_t pop3_c_tot_rx_bytes;

  /* DNS */
  u_ns_8B_t dns_c_num_con_initiated; //Number of initiated requests for making the connection(cumulative)
  u_ns_8B_t dns_c_num_con_succ;      //Number of succ requests for making the connection out of initiated(cumulative) 
  u_ns_8B_t dns_c_num_con_fail;      //Number of fail requests for making the connection out of initiated(cumulative) 
  u_ns_8B_t dns_c_num_con_break;     //Number of initiated requests for closing the connection(cumulative)
  u_ns_8B_t dns_succ_fetches;
  u_ns_8B_t dns_fetches_completed;
  u_ns_8B_t dns_c_min_time;                  //Overall dns time - dns_overall_min_time
  u_ns_8B_t dns_c_max_time;                  //Overall dns time - dns_overall_max_time
  u_ns_8B_t dns_c_tot_time;                  //Overall dns time - dns_overall_tot_time
  u_ns_8B_t dns_c_avg_time;                  //Overall dns time - dns_overall_avg_time
  u_ns_8B_t cum_dns_fetches_started;
  u_ns_8B_t dns_c_tot_total_bytes;
  u_ns_8B_t dns_c_tot_tx_bytes;
  u_ns_8B_t dns_c_tot_rx_bytes;

  u_ns_8B_t pg_succ_fetches;
  u_ns_8B_t pg_fetches_completed;
  u_ns_8B_t pg_c_min_time;
  u_ns_8B_t pg_c_max_time;
  u_ns_8B_t pg_c_tot_time;
  u_ns_8B_t cum_pg_fetches_started;
  u_ns_8B_t pg_c_avg_time;

  u_ns_8B_t tx_c_succ_fetches;
  u_ns_8B_t tx_c_fetches_completed;
  u_ns_8B_t tx_c_min_time;
  u_ns_8B_t tx_c_max_time;
  u_ns_8B_t tx_c_tot_time;
  u_ns_8B_t tx_c_tot_sqr_time;
  u_ns_8B_t tx_c_fetches_started;
  u_ns_8B_t tx_c_avg_time;

  u_ns_8B_t sess_succ_fetches;
  u_ns_8B_t sess_fetches_completed;
  u_ns_8B_t sess_c_min_time;
  u_ns_8B_t sess_c_max_time;
  u_ns_8B_t sess_c_tot_time;
  u_ns_8B_t sess_c_avg_time;
  u_ns_8B_t cum_ss_fetches_started;
  u_ns_4B_t cum_url_error_codes[TOTAL_URL_ERR];   /* for url error codes 1-8 */
  u_ns_4B_t cum_smtp_error_codes[TOTAL_URL_ERR];   /* for url error codes 1-8 */
  u_ns_4B_t cum_pop3_error_codes[TOTAL_URL_ERR];   /* for url error codes 1-8 */
  u_ns_4B_t cum_dns_error_codes[TOTAL_URL_ERR];   /* for url error codes 1-8 */
  u_ns_4B_t cum_pg_error_codes[TOTAL_PAGE_ERR];   /* for page error codes 16-63 */
  u_ns_4B_t cum_tx_error_codes[TOTAL_TX_ERR];
  u_ns_4B_t cum_sess_error_codes[TOTAL_SESS_ERR];
  /*bug 70480 : cum_srv_pushed_resources added in cavgtime*/
  u_ns_8B_t cum_srv_pushed_resources;
} cavgtime;


// This is used for NS Dynamics objects like Tx, Server IP for runtime changes in GDF
typedef struct 
{
  int startId;      	 	  // Start index of dynamically discovered objects.
  int total;         	 	  // Number of new dynamic object discovered during current progress interval 
  int total_num_vectors;          // total number of vectors
  int rtg_group_size;   	 // Overall Size of  group.
                                  // GroupSize = num_element * seizeof(double)
  char *gdf_name;                 // GDF name

  void *normTable;                // This is pointer to NormObjKey e.g. &normRuntimeTXTable;
  int gp_info_idx;                // GDF Group idx in grp data ptr
  int pdf_id;                     //It will store pdf id of graphs.
  int graphId;
  char *graphDesc;

  int max_rtg_index_tbl_col_entries; // Transaction id array
  int max_rtg_index_tbl_row_entries; // NS or NC Controller (0) + Generator array
  int **rtg_index_tbl;       	  //2-D array because we need to add rtg index of generators also.
  char is_gp_info_filled; // This flag will provide informatiom that GroupInfo is already filled for this DynObj or not? 
} DynObjForGDF;

// MD5_DIGEST_LENGTH is defined ??? 
// Message send by generator to controller for registration
typedef struct {
  MSG_HDR  
  char token[2*MD5_DIGEST_LENGTH +1];  // MD5 of generator name
} ChildRegistration;


extern DynObjForGDF dynObjForGdf[];

extern TxDataCum *txCData;

#ifndef CAV_MAIN
extern int g_avgtime_size;
extern int g_static_avgtime_size;
extern int g_avg_size_only_grp;
extern int g_cavgtime_size;
extern int g_cavg_size_only_grp;
extern TxDataSample *txData;
extern avgtime *average_time;
#else
extern __thread int g_avgtime_size;
extern __thread int g_static_avgtime_size;
extern __thread int g_avg_size_only_grp;
extern __thread int g_cavgtime_size;
extern __thread int g_cavg_size_only_grp;
extern __thread TxDataSample *txData;
extern __thread avgtime *average_time;
#endif
extern avgtime *tmp_reset_avg;
extern cavgtime *tmp_reset_cavg;
extern SP_msg *rcv_msg;
extern HourlyMonitoringSess_msg *testrun_msg;
extern EndTestRunMsg *end_test_run_msg;

#define GET_AVG_STRUCT(avg, j)\
  tmp_reset_avg = (avgtime*)((char*)avg + (j * g_avg_size_only_grp));\

#define GET_CAVG_STRUCT(cavg, j)\
  tmp_reset_cavg = (cavgtime*)((char*)cavg + (j * g_cavg_size_only_grp));\

#endif
