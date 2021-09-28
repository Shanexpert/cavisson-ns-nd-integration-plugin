/******************************************************************
 * Name    : ns_pre_test_check.h
 * Author  : Archana
 * Purpose : This file contains declaration of macros and methods. 
 * Note:
 * Modification History:
 * 15/10/08 - Initial Version
 * 02/04/09 - Last modification
*****************************************************************/

#ifndef _NS_PRE_TEST_CHECK_H
#define _NS_PRE_TEST_CHECK_H

//This file contains method related to Starting events "Before test is started" (1) for Check Monitors

//Error Status of check monitor. These macros in +ve num and start with 0 since these status used in array to print error msg for failed status monitor.
#define CHECK_MONITOR_FAIL         0
#define CHECK_MONITOR_CONN_FAIL    1
#define CHECK_MONITOR_CONN_CLOSED  2
#define CHECK_MONITOR_READ_FAIL    3
#define CHECK_MONITOR_SEND_FAIL    4
#define CHECK_MONITOR_EPOLLERR     5
#define CHECK_MONITOR_SYSERR       6
/* CHECK_MONITOR_NOT_STARTED used : 
   - To set Status of the check monitor at init time.
   - To set Error Status of check monitor in case of time out if check mon not run till test completed.
*/
#define CHECK_MONITOR_NOT_STARTED  7 

//Status of the check monitor should not be same as Error Status of check monitor
#define CHECK_MONITOR_PASS         8
#define CHECK_MONITOR_STOPPED      9 // This means check monitor is stopped at the end of test
#define CHECK_MONITOR_RETRY       10
#define CHECK_MONITOR_DATA_PROCESSED 11
#define COPY_CHK_TO_DR 12

//Return status of check result for check monitor
#define CHECK_MONITOR_DONE_WITH_ALL_FAIL  0
#define CHECK_MONITOR_DONE_WITH_SOME_FAIL 1
#define CHECK_MONITOR_DONE_WITH_ALL_PASS  2

#define PRE_TEST_CHECK_DEFAULT_TIMEOUT   15 //default value 15 seconds for pre timeout
#define POST_TEST_CHECK_DEFAULT_TIMEOUT 300 //default value 5 min for post timeout 

extern CheckMonitorInfo *pre_test_check_info_ptr;
extern int pre_test_check_timeout;
extern int post_test_check_timeout;
extern int total_pre_test_check_entries;  // Total Check monitor enteries
extern int max_pre_test_check_entries;    // Max Check monitor enteries
extern int epoll_fd; //this used by pre and post test check
extern int num_post_test_check;
extern int set_monitor;  //this is to differentiate server signature and check monitor

extern char* failed_monitor;

extern int kw_set_post_test_check_timeout(char *keyword, char *buf, char *kw_buf, char *err_msg, int runtime_flag);
extern int kw_set_pre_test_check_timeout(char *keyword, char *buf, char *kw_buf, char *err_msg, int runtime_flag);
extern void kw_set_tname(char *keyword, char *buf);
extern void set_pre_test_check_retry_count(char *value);
extern void set_pre_test_check_retry_interval(char *value);
extern int set_pre_test_check_timeout(char *value, char*);
extern void set_tname(char *buf);
extern int run_pre_test_check();
extern inline void mark_check_monitor_fail(CheckMonitorInfo *check_monitor_ptr, int deselect, int status);
extern inline void mark_check_monitor_fail_v2(CheckMonitorInfo *check_monitor_ptr, int deselect, int status);
extern inline void mark_check_monitor_pass(CheckMonitorInfo *check_monitor_ptr, int deselect, int status);
extern inline void mark_check_monitor_pass_v2(CheckMonitorInfo *check_monitor_ptr, int deselect, int status);
extern inline void abort_post_test_check_and_exit(char *err_msg);
extern inline void add_select_pre_test_check(char* data_ptr, int fd, int event, int check_mon_state);
extern void run_post_test_check();
extern inline int check_results(int check_mon_state);
extern void setup_server_sig_get_java_process();
extern int kw_set_java_process_server_signature(char *keyword, char *buf, int runtime_flag, char *err_msg);
extern void make_connections_for_java_process_server_sig();
extern void retry_connections_for_java_process_server_sig();
extern void start_server_sig(CheckMonitorInfo *pre_test_check_ptr);
extern void process_data(CheckMonitorInfo *pre_test_check_ptr);
extern inline void delete_select_pre_test_check(int fd);
extern inline void mod_select_pre_test_check(char* data_ptr, int fd, int event);

extern int java_process_server_sig_enabled;
extern void process_java_server_sig_data_and_save_in_instance_struct();

extern inline void abort_post_test_checks(int epoll_check, char *err);
extern inline void abort_pre_test_checks(int epoll_check, char *err); 
#endif
