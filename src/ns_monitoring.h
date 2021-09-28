#ifndef NS_MONITORING_H
#define NS_MONITORING_H

#include "nslb_partition.h"

#define DISABLE_PROGRESS_REPORT 0
#define DISABLE_MONITOR_FROM_PROGRESS_REPORT 1
#define COMPLETE_PROGRESS_REPORT 2

//Test run status
#define TEST_RUN_INIT 0
#define TEST_RUN_SCHEDULE 1
#define TEST_RUN_POST_PROCESSING 2
#define TEST_RUN_OVER 3

//Macros used in the function for creating links on the disk path passed with MULTIDISK_PATH keyword
#define REGULAR_FILE_AND_KEYWORD_ENABLE				0
#define DIRECTORY_AND_KEYWORD_ENABLE				1
#define DEBUG_LOG_FILE_AND_KEYWORD_ENABLE			2
#define DIRECTORY_OR_REGULAR_FILE_AND_KEYWORD_DISABLE		3
#define DEBUG_LOG_FILE_AND_KEYWORD_DISABLE			4


#define TR_OR_PARTITION_NAME_LEN 64

/*  in case of continuous mon, dir may exist as test may restart; hence error msg is not displayed. */
#define MKDIR_ERR_MSG(dir, buf){ \
      if(!(EEXIST == errno && global_settings->continuous_monitoring_mode)) \
      { \
        NS_EXIT(-1, "Unable to Create %s directory '%s'. Error: '%s'", dir, buf, nslb_strerror(errno)); \
      } \
    }

extern char *partition_info_buf_to_append;

extern int test_run_info_shr_mem_key;
extern PartitionInfo partInfo;
extern int rtg_msg_seq;
extern void apply_timer_for_new_tr(ClientData cd, char *ip, int start_test_min_dur);
extern int create_test_info_shared_memory();
extern int open_logging_files_in_append_mode(const Global_data* gdata, int testidx);
extern int kw_enable_monitor_report(char *buf);
//extern int kw_set_ns_mode(char *buf);
extern int kw_set_partition_settings(char *buf, int runtime_flag, char *err_msg);
extern int kw_set_cav_epoch_year(char *buf, int runtime_flag, char *err_msg);
extern int nde_set_partition_idx();
extern int nde_get_testid();
extern int get_keyword_value_from_file(char *file, char *keyword, char *result, int size);
extern void nde_write_hidden_partition_info_file();

extern void nde_init_partition_info_struct();
extern void update_test_run_info_shm();
extern int  create_test_run_info_shm();

extern void set_test_run_info_writer_pid(int writer_pid);
extern void set_test_run_info_status(int test_run_status);
extern void set_test_run_info_big_buff_shm_id(int big_buf_shmid);
extern void set_test_run_info_event_logger_port(int event_logger_port);
extern void nde_set_parent_port_number(int port);
extern void save_version();
extern void create_page_dump_dir_or_link();

extern long long g_loc_next_partition_time;
extern long long g_loc_next_partition_idx;
extern ClientData g_client_data;
extern char g_set_check_var_to_switch_partition;
extern int read_and_init_struct();

//for oracle sql stats monitor
extern void create_oracle_sql_stats_csv();
extern void create_alerts_stats_csv();
extern void write_lps_partition_done_file();
extern void save_status_of_partition(char *partition_status);
extern void create_oracle_sql_stats_hidden_file();
extern void check_and_rename(char *name, int file_type_and_keyword_status);
extern void make_partition_info_buf_to_append();
extern void write_lps_partition_done_file_for_mssql();
extern void create_mssql_stats_hidden_file();
extern void create_csv_dir_and_link(char *partition_name);
extern int read_and_init_hidden_file_struct();
#endif
