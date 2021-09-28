#ifndef CDR_CACHE_H
#define CDR_CACHE_H

#include <stdio.h>
#include <stdlib.h>

#include "cdr_main.h"

#define PARTION_LEN 16


enum cache_field
{
  TR_NUM = 0,
  SCEN_NAME,
  START_TIME,
  REPORT_SUMMARY,
  PAGE_DUMP,
  REPORT_PROGRESS,
  REPORT_DETAIL,
  REPORT_USER,
  REPORT_FAIL,
  REPORT_PAGE_BREAK_DOWN,
  WAN_ENV,
  REPORTING,
  TEST_NAME,
  TEST_MODE,
  RUNTIME,
  VUSERS,
  END_TIME_TS,
  LMD_SUMMARY_TOP,
  TR_TYPE,
  TR_DISK_SIZE,
  GRAPH_DATA_SIZE,
  CSV_SIZE,
  RAW_FILE_SIZE,
  TR_DB_TABLE_SIZE,
  TR_DB_INDEX_SIZE,
  KEY_FILE_SIZE,
  HAR_FILE_SIZE,
  PAGE_DUMP_SIZE,
  LOGS_SIZE,
  TEST_DATA_SIZE,
  REPORTS_SIZE,
  CONFIGS_SIZE,
  REMOVE_TR_F,
  GRAPH_DATA_REMOVE_F,
  CSV_REMOVE_F,
  RAW_FILE_REMOVE_F,
  TR_DB_REMOVE_F,
  KEY_FILE_REMOVE_F,
  HAR_FILE_REMOVE_F,
  PAGE_DUMP_REMOVE_F,
  LOGS_REMOVE_F,
  REPORTS_REMOVE_F,
  TEST_DATA_REMOVE_F,
  CONFIGS_REMOVE_F,

  TOTAL_CACHE_FIELDS
};


enum cmt_cache_field {
  CMT_PARTITION_NUM = 0,
  CMT_PARTITION_TYPE,
  CMT_PARTITION_DISK_SIZE,
  CMT_PARTITION_GRAPH_DATA_SIZE,
  CMT_PARTITION_CSV_SIZE,
  CMT_PARTITION_RAW_FILE_SIZE,
  CMT_PARTITION_DB_TABLE_SIZE,
  CMT_PARTITION_DB_INDEX_SIZE,
  CMT_PARTITION_HAR_FILE_SIZE,
  CMT_PARTITION_PAGE_DUMP_SIZE,
  CMT_PARTITION_LOGS_SIZE,
  CMT_PARTITION_REPORTS_SIZE,
  //CMT_PARTITION_REMOVE_F,
  CMT_PARTITION_GRAPH_DATA_REMOVE_F,
  CMT_PARTITION_CSV_REMOVE_F,
  CMT_PARTITION_RAW_FILE_REMOVE_F,
  CMT_PARTITION_DB_REMOVE_F,
  CMT_PARTITION_HAR_FILE_REMOVE_F,
  CMT_PARTITION_PAGE_DUMP_REMOVE_F,
  CMT_PARTITION_LOGS_REMOVE_F,
  CMT_PARTITION_REPORTS_REMOVE_F,

  TOTAL_CMT_CACHE_FIELDS
};

enum nv_cache_field {
  NV_PARTITION_NUM = 0, //use same index deom cmt enum
  NV_PARTITION_TYPE,
  
  NV_PARTITION_DISK_SIZE =1,
  NV_PARTITION_DB_TABLE_SIZE,
  NV_PARTITION_DB_INDEX_SIZE, 
  NV_PARTITION_CSV_SIZE,
  NV_PARTITION_OCX_SIZE,
  NV_PARTITION_NA_TRACES_SIZE,
  NV_PARTITION_ACCESS_LOG_SIZE,
  NV_PARTITION_LOGS_SIZE,

  NV_PARTITION_DB_REMOVE_F,
  NV_PARTITION_CSV_REMOVE_F,
  NV_PARTITION_OCX_REMOVE_F,
  NV_PARTITION_NA_TRACES_REMOVE_F,
  NV_PARTITION_ACCESS_LOG_REMOVE_F,
  NV_PARTITION_LOGS_REMOVE_F,
  NV_PARTITION_NUM_PROC,

  TOTAL_NV_CACHE_FIELDS
};

struct cdr_cache_entry
{
  int  tr_num;
  char scenario_name[CDR_FILE_PATH_SIZE];
  char start_time[CDR_BUFFER_SIZE];
  char report_summary;
  char page_dump;
  char report_progress[CDR_BUFFER_SIZE];
  char report_detail;
  char report_user;
  char report_fail;
  char report_page_break_down;
  int  wan_env;
  int  reporting;
  char test_name[CDR_BUFFER_SIZE];
  char test_mode[64];
  char runtime[CDR_BUFFER_SIZE]; 
  int  vusers;
  // extra fields
  long long  end_time_stamp;
  long long  start_time_ts;
  int  tr_type;
  long long tr_disk_size;
  long long  int  tr_db_table_size;
  long long int  tr_db_index_size;
  long long int  lmd_summary_top;
  long long key_file_size;
  long long graph_data_size;
  long long har_file_size;
  long long csv_size;
  long long page_dump_size;
  long long logs_size;
  long long raw_file_size;
  long long test_data_size;
  long long reports_size;
  long long configs_size;

  long long int remove_tr_f;

  long long int  graph_data_remove_f;
  long long int  csv_remove_f;
  long long int  raw_file_remove_f;
  long long int  tr_db_remove_f;
  long long int  key_file_remove_f;
  long long int  har_file_remove_f;
  long long int  page_dump_remove_f;
  long long int  logs_remove_f;
  long long int  test_data_remove_f;
  long long int  reports_remove_f;
  long long int configs_remove_f;

  struct dirent **partition_list;
  int count;

  char is_tr_present; // flag to check if TR is present in disk
};

struct cdr_cmt_cache_entry
{
  long long partition_num;
  long long partition_disk_size;
  long long partition_db_table_size;
  long long partition_db_index_size;
  long long partition_graph_data_size;
  long long partition_har_file_size;
  long long partition_csv_size;
  long long partition_page_dump_size;
  long long partition_logs_size;
  long long partition_raw_file_size;
  long long partition_reports_size;

  long long int partition_graph_data_remove_f;
  long long int partition_csv_remove_f;
  long long int partition_raw_file_remove_f;
  long long int partition_db_remove_f;
  long long int partition_har_file_remove_f;
  long long int partition_page_dump_remove_f;
  long long int partition_logs_remove_f;
  long long int partition_reports_remove_f;
 
  int  partition_type;
  char is_partition_present; // flag to check if partiton is present in disk
};

struct cdr_nv_cache_entry
{
  long long int partition_num;
  char *nv_client_id;
  long long partition_disk_size;
  long long   partition_db_table_size;
  long long   partition_db_index_size;
  long long partition_csv_size;
  long long partition_ocx_size;
  long long partition_na_traces_size;
  long long partition_access_log_size;
  long long partition_logs_size;

  long long int partition_db_remove_f;
  long long int partition_csv_remove_f;
  long long int partition_ocx_remove_f;
  long long int partition_na_traces_remove_f;
  long long int partition_access_log_remove_f; 
  long long int partition_logs_remove_f;

  int  partition_type;
  int num_proc;
  char is_partition_present; // flag to check if partiton is present in disk
};

extern int total_cache_entry;
extern int max_cache_entry;
extern struct cdr_cache_entry *cdr_cache_entry_list; // contains all the cache entries
extern struct NormObjKey cache_entry_norm_table; 

extern int total_cmt_cache_entry;
extern int max_cmt_cache_entry;
extern struct cdr_cmt_cache_entry *cdr_cmt_cache_entry_list; // contains all the cache entries
extern struct NormObjKey cmt_cache_entry_norm_table; 

extern int total_nv_cache_entry;
extern int max_nv_cache_entry;
extern struct cdr_nv_cache_entry *cdr_nv_cache_entry_list; // contains all the cache entries
extern struct NormObjKey nv_cache_entry_norm_table; 

extern int nv_cache_entry_add(char *cache_line_buff, char present_flag);

extern int cdr_read_cache_file(char present_flag);
extern int cdr_read_cmt_cache_file(char present_flag);
extern int cdr_read_nv_cache_file(char present_flag);
extern int create_cache_entry_from_tr(int tr_num, int norm_id, int cal_size);
extern int cache_entry_add(char *cache_buff, char present_flag);
extern struct cdr_cache_entry *get_cache_entry(int tr_num); // get cache entry by tr numbeer (without the TR prefix)
extern void cdr_dump_cache_to_file();
extern int cmt_cache_entry_add(char *cache_line_buff, char present_flag);
extern int cmt_cache_entry_add_from_partition(long long int partition_num, char *partition_num_str, int norm_id, int cur_idx);
extern int nv_cache_entry_add_from_partition(long long int partition_num, char *partition_num_str, int norm_id, int cur_idx);
extern int nv_cache_entry_add_overall(char *nv_client_id);
extern void partiton_tableSize_with_indexes_size(long long int partition_num, double *sz, double *sz_index);

#endif
