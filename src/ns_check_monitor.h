/******************************************************************
 * Name    : ns_check_monitor.h
 * Author  : Archana
 * Purpose : This file contains declaration of macros, structure,
             and methods.
 * Note:
 * Modification History:
 * 02/04/09 - Initial Version
*****************************************************************/
#ifndef _NS_CHECK_MONITOR_H
#define _NS_CHECK_MONITOR_H

#include "ns_server_admin_utils.h"
#include "nslb_get_norm_obj_id.h"
#include "ns_dynamic_vector_monitor.h" 
#include "ns_exit.h"
#include <stdbool.h>


// Uses ns_custom_monitor.h for common #defines
#define CHECK_COUNT_FOREVER -1

//Starting events for Check Monitors
#define CHECK_MONITOR_EVENT_BEFORE_TEST_IS_STARTED             1   //"Before test is started"
#define CHECK_MONITOR_EVENT_START_OF_TEST                      2   //"Start of Test"
#define CHECK_MONITOR_EVENT_START_OF_PHASE                     3   //"At Start of the Phase"
#define CHECK_MONITOR_EVENT_RETRY_INTERNAL_MON                 4   //For internal server signatrures
#define CHECK_MONITOR_EVENT_AFTER_TEST_IS_OVER                 90  //"After test is Over"

//End events for Check Monitors
#define TILL_TEST_COMPLETION          1
#define COMPLETE_SPECIFIED_EXECUTIONS 2
#define TILL_COMPLETION_OF_PHASE      3

//Status of Test Run
#define TEST_RUN_COMPLETED_SUCCESSFULLY                  "Test Run completed Successfully"
#define TEST_RUN_STOPPED_DUE_TO_FAILURE_OF_CHECK_MONITOR "Test Run stopped due to failure of Check Monitor"
#define TEST_RUN_STOPPED_BY_USER                         "Test Run stopped by User"
#define TEST_RUN_STOPPED_DUE_TO_SYSTEM_ERRORS            "Test Run stopped due to system errors"

//String line that send by Check Monitors 
#define CHECK_MONITOR_FAILED_LINE "CheckMonitorStatus:Fail"
#define CHECK_MONITOR_PASSED_LINE "CheckMonitorStatus:Pass"
#define CHECK_MONITOR_EVENT       "Event:"
#define CHECK_MONITOR_FTP_FILE    "FTPFile:"
#define CHECK_MONITOR_UNREACHABLE "CheckMonitorStatus:Unreachable"
#define CHECK_MONITOR_SEND_FAILED "CheckMonitorStatus:SendFailed"
#define CHECK_MONITOR_START_SESSION_LINE  "SESSION_STARTED"

//Read message state
#define CHECK_MON_FTP_FILE_STATE 1
#define CHECK_MON_COMMAND_STATE 2
#define CHK_MON_FTP_IN_PROGRESS 100
#define CHK_MON_FTP_COMPLETE 101

#define MAX_CHECK_MON_CMD_LENGTH 5120
#define MAX_CHECK_MONITOR_MSG_SIZE 5120
#define MAX_BUF_LENGTH 4096
#define MAX_LENGTH     1024
#define MAX_CM_BUFFER_LENGTH     2*1024

//Set check for server signature and check monitor
#define CHECK_MON_IS_SERVER_SIGNATURE 1
#define CHECK_MON_IS_CHECK_MONITOR    2
#define CHECK_MON_IS_BATCH_JOB        3
#define INTERNAL_CHECK_MON 3

//Batch job options
#define NS_BJ_USE_EXIT_STATUS   1
#define NS_BJ_SEARCH_IN_OUTPUT  2
#define NS_BJ_CHECK_IN_LOG_FILE 3
#define NS_BJ_RUN_CMD           4

/* ----------------- Disaster Recovery -------------------------- */
#define CHK_MON_DR_ARRAY_SIZE 5*1024   //disaster recovery array

//Check monitor connection states
#define CHK_MON_INIT                    0
#define CHK_MON_CONNECTED               1
#define CHK_MON_CONNECTING              2
#define CHK_MON_SENDING                 3     
#define CHK_MON_RUNNING                 4
#define CHK_MON_STOPPED                 5
#define CHK_MON_SESS_START_SENDING      6
#define CHK_MON_SESS_START_RECEIVING    7
//#
#define CHK_MON_NOT_REMOVE_FROM_EPOLL    0
#define CHK_MON_REMOVE_FROM_EPOLL        1

/* ----------------- Disaster Recovery End-------------------------- */

#define ALERT_MSG_SIZE 1024
#define ALERT_MSG_BUF_SIZE 4096

#define ALERT_FAIL_VALUE "0"
#define ALERT_PASS_VALUE "1"

#define ALERT_MSG_1017009 "Starting %s '%s' on server = '%s' of tier = '%s'."
#define ALERT_MSG_1017007 "%s '%s' failed on server = '%s' of tier = '%s' with error message '%s'"
#define ALERT_MSG_1017008 "%s '%s' passed on server = '%s' of tier = '%s'"
#define ALERT_MSG_1017010 "%s '%s' not started successfully on server = '%s' of tier = '%s'  error = '%s'"
#define ALERT_MSG_1017011 "%s '%s' started successfully on server = '%s' of tier = '%s'"


extern int num_pre_test_check;
extern int num_post_test_check;
extern int g_total_aborted_chk_mon_conn;


/*
  This CheckMonitorInfo structure has moved here from ns_pre_test_check.h 
  to reuse for check monitor with additional variable.
  This struct remain will use from ns_pre_test_check.c
*/
typedef struct CheckMonitorInfo
{
  char con_type; // Must be first field
  //File descriptor for TCP/FTP socket made to make connection with CS.
  int fd;                    // fd must be the 1st field as we are using this struct as epoll_event.data.ptr
  char *check_monitor_name; // Monitor name (This is for info only)
  char *tier_name;
  char *server_name;
  char *instance_name;
  char *mon_name;
  char *cs_ip;               // Create Server IP
  short cs_port;             // Create Server Port
  char access ;               // 1 as local (default) and 2 as remote
  char *rem_ip;              // remote IP
  char *rem_username;        // remote User name
  char *rem_password;        // remote Password
  char *pgm_path;            // Monitor program name with or without path
  char *pgm_args;            // program argument given by user eg; filename, etc.
  char *json_args;           // args passed in json file.
  int option;                // 1 is for run periodically and 2 is for run once
  int periodicity;           // this is frequency (in seconds)
  int max_count;             // count i.e. run depend upon count, -1 is run forever till test run is stopped
  char *end_phase_name;      // end phase name (if end event is till completion of phase)
  int from_event;            // starting event of test run 
  char *start_phase_name;    // start phase name
  int tier_idx;
  char monitor_type;         // monitor type - Check monitor or Server Signature
  int status;                // Status of the check monitor
  char state;                // State - Message or FTP

  // For keeping read line if partial
  char *partial_buffer;      // partial message
  int partial_buffer_len;    // for partial message length

  //To read FTPfile contents
  FILE *ftp_fp;           // FTP File pointer 
  unsigned long ftp_file_size;      // Size to be read

  // Following are used for batch jobs
  int bj_success_criteria;  // 0 - Means this is not for batch job
  union {
    char *bj_check_cmd;
    char *bj_log_file;  
  };
  char *bj_search_pattern;
 
  char conn_state;          // connection state: stop,abort,running
  int chk_mon_retry_attempts;
  int bytes_remaining;
  int send_offset;
  unsigned short topo_server_info_idx;
  char *partial_buf;      
  char *origin_cmon;      //Added for Heroku, this is the origin cmon server name or ip address as given by origin cmon to proxy cmon
  int retry_count;
  int max_retry_count;
  int retry_time_threshold_in_sec;
  int recovery_time_threshold_in_sec;    
  long last_retry_time_in_sec;
//  long last_recovery_time_in_sec;
  int server_index;
  int mon_id;
  int total_chunk_size;
  int mon_info_index;   // this is mon_config_list_ptr index
  char *g_mon_id;  //this is the id of monitor which is passed in json_info_struct and it is unique for all monitors
  bool any_server;
} CheckMonitorInfo;

extern CheckMonitorInfo *check_monitor_info_ptr;
extern char s_check_monitor_buf[];
extern int total_check_monitors_entries;
extern int max_check_monitors_entries;
extern int check_duplicate_monitor_name(CheckMonitorInfo *check_mon_ptr, int total_monitor_entries, char *check_mon_name, int runtime_flag, char *err_msg,int tier_idx,int server_index);
extern int kw_set_check_monitor(char *keyword, char *buf, int runtime_flag, char *err_msg, JSON_info *json_info_ptr);
//extern void kw_set_batch_job_group(char *batch_group_name, int num, int runtime_flag, char *err_msg);
extern void start_check_monitor(int from_event, char *phase_name);
extern void stop_check_monitor(int end_event, char *end_phase_name);
extern int handle_if_check_monitor_fd(void *ptr);
extern char *CheckMonitor_to_string(CheckMonitorInfo *check_monitor_ptr, char *buf);
extern char* get_ftp_user_name();
extern char* get_ftp_password();
extern int send_msg_to_cs(CheckMonitorInfo *check_monitor_ptr);
extern int read_ftp_file(char* check_monitor_ftpfile_msg, CheckMonitorInfo *check_monitor_ptr);
extern void save_status_of_test_run(char* test_run_status);
extern int read_output_from_cs(CheckMonitorInfo *check_monitor_ptr);
extern int add_check_monitor(CheckMonitorInfo *check_monitor_ptr, char *check_mon_name, int from_event, char *start_phase_name, char *pgm_path, char *cs_ip, int access, char *rem_ip, char *rem_username, char *rem_password, char *pgm_args, int option, int periodicity, char *end_event, char *max_count_or_end_phase_name, int monitor_type, char *origin_cmon, int runtime_flag, char *err_msg, int server_index, JSON_info *json_info_ptr,int tier_idx);
extern inline void close_check_monitor_connection(CheckMonitorInfo *check_monitor_ptr);
extern inline void stop_all_check_monitors();  //use in ns_parent.c to stop all check monitor

//check monitor disaster recovery
extern int kw_set_enable_chk_monitor_dr(char *keyword, char *buf, int runtime_flag, char *err_msg);
extern inline void handle_chk_mon_disaster_recovery();
extern int chk_mon_send_msg_to_cmon(void *ptr);
extern int chk_mon_make_nb_conn(CheckMonitorInfo *chk_mon_ptr);

extern void reset_chk_mon_values(CheckMonitorInfo *chk_mon_ptr);

extern inline void chk_mon_handle_err_case(CheckMonitorInfo *chk_mon_ptr, int remove_from_epoll);
extern int pre_chk_mon_send_msg_to_cmon(void *ptr);

extern void close_ftp_file(CheckMonitorInfo *check_monitor_ptr);
extern int extract_ftpfilename_size_createfile_with_size(char *cmd_line, CheckMonitorInfo *check_monitor_ptr);
extern inline void chk_mon_make_send_msg(CheckMonitorInfo *check_monitor_ptr, char *msg_buf, int *msg_len);
extern void end_check_monitor(CheckMonitorInfo *check_monitor_ptr); 
#define CHK_MON_RUNTIME_RETURN_OR_EXIT(runtime_flag, err_msg, flag) \
{ \
  if(runtime_flag) \
  { \
    if(flag) \
      topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_index].server_ptr->topo_servers_list->cmon_monitor_count--; \
    return -1; \
  } \
  else \
  { \
    NS_EXIT(-1,"%s", err_msg); \
  } \
}


extern char pid_received_from_ndc;

extern inline void chk_mon_update_dr_table(CheckMonitorInfo *chk_mon_ptr);
#endif
