#ifndef _NS_TR_DURATION_H_
#define _NS_TR_DURATION_H_
/* 
 * Output will be :
 * ----------------------------------------------------------- 
 *  TR|Duration|OrigDuration|Operation|Status (header line)
 *  1234|00:10:10|00:00:00|get or update|OK or NotOK or NotZero
 */

/* summary.top has 16 pipe seperated fields,
   Test Run|Scenario Name|Start Time|Report Summary|Page Dump|Report Progress|Report Detail|
   Report User|Report Fail|Report|Page Break Down|WAN_ENV|REPORTING|Test Name|Test Mode
   |Run Time|Virtual Users
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "nslb_util.h" 

#define MAXLENGTH               1024
#define MAX_SUMMARY_TOP_FIELDS  16

#define FirstPartitionIdx_LENGTH 18 //17 + 1 (lenth of string 17 + 1 for '=')
#define CurPartitionIdx_LENGTH 16 //15 + 1 (lenth of string 15 + 1 for '=')

#define PARTITION_LENGTH 15 // length of format '20140523114529'
#define PARTITION_ARRAY_SIZE 2 

extern int test_run_running;
extern int is_testrun_running(int test_run_num);
extern char *trpath(int trnum, char *ns_wdir_path); 
extern int get_actual_duration(char *summary_top, char *rtg_msg_dat, char *test_run_gdf, int tr_running_flag);
extern char *get_orig_duration(char *filepath, int *trnum);
extern int get_tm_from_format(char *str);
extern char *format_time(int tm);
extern int write_summary(char *duartion, char *summary_top);
extern void print_result(int trnum,char *duration, char *orig_duration, char *operation, char *status, int hflag);
extern char *calc_duration(int trnum, char *ns_log_path, char *sum_top_file, char *str_orig_duration, int tr_or_partition, long long partition_idx, int Rflag, int get_is_test_running_flag);
extern long long get_duration(int test_run_number, char *NSLogsPath, char *total_time_str, int Rflag, int get_is_test_running_flag, char *partition_duration);
extern long long local_get_time_from_format(char *str);
extern void fill_min_max_duration(char *time_string, long long *max_duration, long long *min_duration);
extern int get_summary_top_start_time_in_secs(char *date_str);
extern void logging_start_end_date_time(char *time_string, long long *start_time_sec, long long *end_time_sec);
extern long long format_date_get_seconds_from_epoch(char *str, char del);
extern int get_first_and_last_partition(char *NSLogsPath, int test_run_number, char partition_name[][PARTITION_LENGTH]);
#endif
