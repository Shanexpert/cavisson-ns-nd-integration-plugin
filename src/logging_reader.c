#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <regex.h>
#include <signal.h>
#include <libpq-fe.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"
#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "logging_reader.h"
#include "logging.h"
#include "util.h"
#include "ns_alloc.h"

#include "nslb_log.h"
#include "nslb_db_util.h"
#include "ns_objects_normalization.h"
#include "nslb_dyn_hash.h"
#include "nslb_util.h"
#include <sys/types.h>
#include <sys/wait.h>

#include "nslb_comp_recovery.h"
#include "nslb_multi_thread_trace_log.h"
#include "nslb_signal.h"
#include "nslb_partition.h"
#include "ns_common.h"
#include "wait_forever.h"

#define NS_EXIT_VAR 
#include "ns_exit.h"

#define IS_START_PARTITION   -1

#define PREV_PARTITION        0
#define NEXT_PARTITION        1
#define IS_FIRST_PARTITION   -2
#define READ_DLOG_OFFSET      0
#define WRITE_DLOG_OFFSET     1

#define MAX_RECOVERY_COUNT               5
#define RECOVERY_TIME_THRESHOLD_IN_SEC  10
#define RESTART_TIME_THRESHOLD_IN_SEC  900 

#define MAX_FILENAME_LENGTH 1024
#define ns_id_str_to_num(str) (unsigned int)atoi(str);

#define TXTABLE_LEN         7
#define TXTABLEV2_LEN	    9
#define URLTABLE_LEN        8
#define PAGETABLE_LEN       9
#define SESSIONTABLE_LEN   12
#define GENERATORTABLE_LEN 14
#define RUNPROFILE_LEN     10
#define HOSTTABLE_LEN       9

#define OFFLINE_MODE                      0
#define ONLINE_MODE_AND_DB_USING_DBUPLOAD 1

#define WRITE_CONDITIONALLY 0
#define WRITE_UNCONDITIONALLY 1

int loader_opcode = 0;
static int slog_offset = 0;
char slog_partial_buffer[LINE_LENGTH] = "\0";

int log_shr_buffer_size = 8192;// Block size

extern long long partition_idx;  
char base_dir[MAX_FILENAME_LENGTH] = {0};
char *multidisk_csv_path = NULL;

//Trace log key 
MTTraceLogKey *lr_trace_log_key;

TestRunInfoTable_Shr *testruninfo_tbl_shr = NULL;

int time_after_csv_write = -1;
int time_after_dlog_process = -1;

//Moved to global
struct sigaction sa;
sigset_t lr_sigset;

static FILE* s_in_fptr;
static FILE* d_in_fptr;
static FILE* rpr_out_fptr;
static FILE* upr_out_fptr;
static FILE* spr_out_fptr;
static FILE* sesstb_out_fptr;
static FILE* ptb_out_fptr;
static FILE* ttb_out_fptr;
static FILE* utb_out_fptr;
static FILE* hrt_out_fptr;
static FILE* hat_out_fptr;
static FILE* urc_out_fptr;
static FILE* prc_out_fptr;
static FILE* tptb_out_fptr;
static FILE* trc_out_fptr;
static FILE* src_out_fptr;
static FILE* lpt_out_fptr;
//static FILE* tc_out_fptr;
static FILE* data_out_fptr;
static FILE* msg_out_fptr;
static FILE* page_dump_out_fptr;
static FILE* generator_out_fptr;
static FILE* page_rbu_detail_record_fptr;
static FILE* rbu_lighthouse_record_fptr;
static FILE* rbu_mark_measure_record_fptr;
static FILE* host_out_fptr;
//static u_ns_ts_t ns_start_ts;
static int nsu_db_upload_pid = 0;
static int nsu_db_upload_tmp_table_pid = 0;

//static void receive_partition_switch_sig_frm_writer();

DB* data_hash_table;
DB* msg_hash_table;
unsigned short child_idx_with_gen_id;

#define DATA_BUFFER_SIZE (BLOCK_SIZE - 8)

struct data_log_id {
  unsigned short child_idx;
  unsigned int sess_inst;
  unsigned int url_idx;
};

struct data {
  char data[DATA_BUFFER_SIZE];
  struct data* next;
};

struct data_file_entry {
  unsigned int data_length;
  struct data* head_data;
  struct data* cur_data;
};

struct msg_log_id {
  unsigned short child_idx;
  unsigned int msg_num;
  unsigned int time;
};
/*BEGIN: PAGEDUMP*/
struct pg_dump_data {
  char data[DATA_BUFFER_SIZE];
  struct pg_dump_data* next;
};

struct pg_dump_data_entry {
  unsigned int data_length;
  unsigned int msg_num;
  unsigned int time;
  // Added in 3.9.6 - Total size of data part (parameterization line and body) of all records of this message. This is added for checking for any missing records. Page dump with missing records will be discarded
  unsigned int total_size;
  struct pg_dump_data* head_data;
  struct pg_dump_data* cur_data;
};
struct pg_dump_data_entry pg_data_entry[255];
/*END: PAGEDUMP*/
struct data* free_data = NULL;
struct data_file_entry* free_data_entry = NULL;

int total_run_profile_entries;
int max_run_profile_entries;
int total_user_profile_entries;
int max_user_profile_entries;
int total_sess_profile_entries;
int max_sess_profile_entries;
int total_recsvr_table_entries;
int max_recsvr_table_entries;
int total_actsvr_table_entries;
int max_actsvr_table_entries;
int total_phase_table_entries;
int max_phase_table_entries;

int running_mode = 0; //default value
int write_check_time = 10;


RunProfileLogEntry* runProfile;
UserProfileLogEntry* userProfile;
SessionProfileLogEntry* sessProfile;
ActSvrTableLogEntry* actSvrTable;
PhaseTableLogEntry* phaseTable;
RecSvrTableLogEntry* recSvrTable;
TestCaseLogEntry testCaseLog;

RunProfTableEntry_Shr* runprof_start;
UserProfIndexTableEntry_Shr* userprof_start;
SessProfIndexTableEntry_Shr *sessprof_start;
SessTableEntry_Shr* sess_start;
PageTableEntry_Shr* page_start;
TxTableEntry_Shr* tx_start;
action_request_Shr* url_start;
SvrTableEntry_Shr* recsvr_start;
PerHostSvrTableEntry_Shr* actsvr_start;

static char *deliminator = ",";
static int inline_output = 0;

int test_id;
char partition_name[100] = "";
char common_files_dir[16] = "";
long long dlog_read_offset = 0;
int dlog_off_fd = -1;

int trace_log_size = 10; //10 MB
int trace_level = 1; //default trace level is 1

static char wdir[512 + 1] = "";
int DynamicTableSizeiUrl = 0; //Need not be same as NS
int DynamicTableSizeiTx = 0; 
int skip_dlog_buffer = 0; 

static void sighup_handler(int sig);
static void sigalarm_handler(int sig);
static void sigterm_handler(int sig);
static void sigchild_handler(int sig);
static void partition_switch_sig_handler(int sig);
static void trace_level_change_sig_handler(int sig);
static void test_post_proc_sig_handler(int sig);


int lw_pid;
int sigusr1 = 0;
int sigalarm = 0;
int sigterm = 0;
int test_post_proc_sig = 0;
int partition_switch_sig = 0;
int trace_level_change_sig = 0;
int sigchild = 0;
void do_sigalarm();
void do_sigterm ();
void do_sigchild ();
void do_trace_level_change ();
void do_test_post_proc();

#define BUFF_FILL_SIZE       10000000 //~10MB
#define BUFF_80_PCT_FILL_SIZE 8000000 // ~ 80% of BUFF_FILL_SIZE 

char urc_write_buffer[BUFF_FILL_SIZE];
char pdrc_write_buffer[BUFF_FILL_SIZE];
char src_write_buffer[BUFF_FILL_SIZE];
char trc_pg_write_buffer[BUFF_FILL_SIZE]; 
char trc_write_buffer[BUFF_FILL_SIZE];
char prc_write_buffer[BUFF_FILL_SIZE];
char prrc_write_buffer[BUFF_FILL_SIZE];
char host_write_buffer[BUFF_FILL_SIZE];
char rlhr_write_buffer[BUFF_FILL_SIZE];
char rbu_mm_write_buffer[BUFF_FILL_SIZE];

char *urc_write_buf_ptr = urc_write_buffer;
char *pdrc_write_buf_ptr = pdrc_write_buffer;
char *src_write_buf_ptr = src_write_buffer;
char *trc_pg_write_buf_ptr = trc_pg_write_buffer;
char *trc_write_buf_ptr = trc_write_buffer;
char *prc_write_buf_ptr = prc_write_buffer;
char *prrc_write_buf_ptr = prrc_write_buffer;
char *host_write_buf_ptr = host_write_buffer;
char *rlhr_write_buf_ptr = rlhr_write_buffer;
char *rbu_mm_write_buf_ptr = rbu_mm_write_buffer;

int generator_idx = -1; //Used to store generator index 
int nvm_idx = -1; //Used to store nvm index 

int obj_norm_id = -1;
int total_nvms = 0;
int total_generator_entries = 0;

#define FCLOSE(fileptr) { \
  if (fileptr) \
  { \
    fclose (fileptr); \
    fileptr = NULL; \
  } \
}

#define GET_URL_NORM_ID(index, type, gen_id) {  \
    static char dummy_url_added = 0; \
    obj_norm_id = get_url_norm_id(index, gen_id); \
    NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "obj_norm_id = %d", obj_norm_id); \
    if(obj_norm_id == -1)  \
    { \
      NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_WARNING, "Dynamic URL ID (%u (0x%x) is not yet loaded from slog. Forcing reading of slog\n", index, index); \
      update_object_table(); \
      NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "after update_object_table()"); \
      obj_norm_id = get_url_norm_id(index, gen_id); \
      NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "check: again : obj_norm_id = %d", obj_norm_id);\
      /* Narendra: In this case we are setting retIdx to -1, This Url record will be mapped to INVALID_URL. \
         This will be added once. */ \
      if(obj_norm_id == -1) \
      {  \
        NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Now in second time: obj_norm_id = %d", obj_norm_id);\
        if(!dummy_url_added) \
        { \
          add_dummy_data_in_table(type); \
          dummy_url_added = 1; \
        } \
        obj_norm_id = -1; \
      } \
    } \
  } 

#define GET_HOST_NORM_ID(index, type, gen_id, nvm_id) {  \
    NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called GET_HOST_NORM_ID(), index = %d, type = %d, gen_id = %d, nvm_id = %d", index, type, gen_id, nvm_id); \
    static char dummy_host_added = 0; \
    obj_norm_id = get_host_norm_id(index, gen_id, nvm_id); \
    NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "obj_norm_id = %d", obj_norm_id); \
    if(obj_norm_id == -1)  \
    { \
      NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_WARNING, "HOST ID (%u (0x%x) is not yet loaded from slog. Forcing reading of slog\n", index, index); \
      update_object_table(); \
      NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "after update_object_table()"); \
      obj_norm_id = get_host_norm_id(index, gen_id, nvm_id); \
      NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "check: again : obj_norm_id = %d", obj_norm_id);\
      if(obj_norm_id == -1) \
      {  \
        if(!dummy_host_added) \
        { \
          add_dummy_data_in_table(type); \
          dummy_host_added = 1; \
        } \
        obj_norm_id = -1; \
      } \
    } \
  } 

#define GET_SESSION_NORM_ID(index, type) {  \
    static char dummy_session_added = 0; \
    obj_norm_id = get_session_norm_id(index); \
    NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "obj_norm_id = %d", obj_norm_id); \
    if(obj_norm_id == -1)  \
    { \
      NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_WARNING, "SESSION ID (%u (0x%x) is not yet loaded from slog. Forcing reading of slog\n", index, index); \
      update_object_table(); \
      NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "after update_object_table()"); \
      obj_norm_id = get_session_norm_id(index); \
      NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "check: again : obj_norm_id = %d", obj_norm_id);\
      if(obj_norm_id == -1) \
      {  \
        if(!dummy_session_added) \
        { \
          add_dummy_data_in_table(type); \
          dummy_session_added = 1; \
        } \
        obj_norm_id = -1; \
      } \
    } \
  }
 
#define GET_GENERATOR_NORM_ID(index, type) {  \
    static char dummy_generator_added = 0; \
    obj_norm_id = get_generator_norm_id(index); \
    NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "obj_norm_id = %d", obj_norm_id); \
    if(obj_norm_id == -1)  \
    { \
      NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_WARNING, "GENERATOR ID (%u (0x%x) is not yet loaded from slog. Forcing reading of slog\n", index, index); \
      update_object_table(); \
      NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "after update_object_table()"); \
      obj_norm_id = get_generator_norm_id(index); \
      NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "check: again : obj_norm_id = %d", obj_norm_id);\
      if(obj_norm_id == -1) \
      {  \
        if(!dummy_generator_added) \
        { \
          add_dummy_data_in_table(type); \
          dummy_generator_added = 1; \
        } \
        obj_norm_id = -1; \
      } \
    } \
  } 

#define GET_GROUP_NORM_ID(index, type) {  \
    static char dummy_group_added = 0; \
    obj_norm_id = get_group_norm_id(index); \
    NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "obj_norm_id = %d", obj_norm_id); \
    if(obj_norm_id == -1)  \
    { \
      NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_WARNING, "GROUP ID (%u (0x%x) is not yet loaded from slog. Forcing reading of slog\n", index, index); \
      update_object_table(); \
      NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "after update_object_table()"); \
      obj_norm_id = get_group_norm_id(index); \
      NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "check: again : obj_norm_id = %d", obj_norm_id);\
      if(obj_norm_id == -1) \
      {  \
        if(!dummy_group_added) \
        { \
          add_dummy_data_in_table(type); \
          dummy_group_added = 1; \
        } \
        obj_norm_id = -1; \
      } \
    } \
  } 

#define GET_PAGE_NORM_ID(index, type) {  \
    static char dummy_page_added = 0; \
    obj_norm_id = get_page_norm_id(index); \
    NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "obj_norm_id = %d", obj_norm_id); \
    if(obj_norm_id == -1)  \
    { \
      NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_WARNING, "PAGE ID (%u (0x%x) is not yet loaded from slog. Forcing reading of slog\n", index, index); \
      update_object_table(); \
      NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "after update_object_table()"); \
      obj_norm_id = get_page_norm_id(index); \
      NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "check: again : obj_norm_id = %d", obj_norm_id);\
      if(obj_norm_id == -1) \
      {  \
        if(!dummy_page_added) \
        { \
          add_dummy_data_in_table(type); \
          dummy_page_added = 1; \
        } \
        obj_norm_id = -1; \
      } \
    } \
  } 

#define GET_TX_NORM_ID_V2(nvm_id, index, type, gen_id) {  \
    static char dummy_tx_added = 0; \
    obj_norm_id = get_tx_norm_id_v2(nvm_id, index, gen_id); \
    NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "obj_norm_id = %d", obj_norm_id); \
    if(obj_norm_id == -1)  \
    { \
      NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_WARNING, "Dynamic Tx ID (%u (0x%x) is not yet loaded from slog. Forcing reading of slog\n", index, index); \
      update_object_table(); \
      NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "after update_object_table()"); \
      obj_norm_id = get_tx_norm_id_v2(nvm_id, index, gen_id); \
      NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "check: again : obj_norm_id = %d", obj_norm_id);\
      /* Narendra: In this case we are setting retIdx to -1, This Url record will be mapped to INVALID_URL. \
         This will be added once. */ \
      if(obj_norm_id == -1) \
      {  \
        NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Now in second time: obj_norm_id = %d", obj_norm_id);\
        if(!dummy_tx_added) \
        { \
          add_dummy_data_in_table(type); \
          dummy_tx_added = 1; \
        } \
        obj_norm_id = -1; \
      } \
    } \
  }

#define GET_TX_NORM_ID(index, type) {  \
    static char dummy_tx_added = 0; \
    obj_norm_id = get_tx_norm_id(index); \
    NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "obj_norm_id = %d", obj_norm_id); \
    if(obj_norm_id == -1)  \
    { \
      NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_WARNING, "TX ID (%u (0x%x) is not yet loaded from slog. Forcing reading of slog\n", index, index); \
      update_object_table(); \
      NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "after update_object_table()"); \
      obj_norm_id = get_tx_norm_id(index); \
      NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "check: again : obj_norm_id = %d", obj_norm_id);\
      if(obj_norm_id == -1) \
      {  \
        if(!dummy_tx_added) \
        { \
          add_dummy_data_in_table(type); \
          dummy_tx_added = 1; \
        } \
        obj_norm_id = -1; \
      } \
    } \
  } 

// check size, if size > 0, fwrite, check status,do trace log
#define WRITE_CSV_FILE(fp, size, buffer, buf_ptr, csv_name) { \
  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, " In WRITE_CSV_FILE: csv_name = %s, size = %d ", csv_name, size); \
  if(size > 0) { \
    if(fwrite(buffer, size, 1, fp) < 1) { \
      NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_ERROR, "write failed for csv %s.", csv_name); \
      return; \
    } \
    if(fflush(fp) != 0) { \
      NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_ERROR, "fflush failed for csv %s.", csv_name); \
    } \
    NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Written %d bytes for csv %s.", size, csv_name); \
    /* After writing the data we need to point buffer to the starting point */ \
    buf_ptr = buffer; \
  } \
}
   
static char *get_time(u_ns_ts_t ts) {
  unsigned int num;
  int hr, min, sec,msec ;
  static char time_buf[64];

	//num = ts - ns_start_ts;
	num = ts;
	msec = num%1000;
	num /= 1000;
	hr = num/3600;
	num = num%3600;
	min = num/60;
	sec = num%60;
	sprintf(time_buf, "%02d:%02d:%02d.%03d", hr, min, sec, msec);
	return (time_buf);
}

int get_recsvr_index(ns_ptr_t ptr) { return (SvrTableEntry_Shr*)ptr - recsvr_start; }
int get_actsvr_index(ns_ptr_t ptr) { return (PerHostSvrTableEntry_Shr*)ptr - actsvr_start; }

/* Temporarily fix for bug #9305
 * For this fix, 3 files is modify (logging_reader.c ns_db_upload.c and nsu_import) and a new shall is added nsi_upload_tmp_table.
 *
 * logging_reader.c changes.
 * Truncate hat.csv, hrt.csv, spf.csv, upf.csv and log_phase_table.csv.
 * Function start_db_upload_tmp_table_process is added for start nsi_upload_tmp_table while dupping csv files.
 *
 * ns_db_upload.c changes.
 * In nsu_db_upload comment the entry for these csv files. 
 * Means data is not upload from nsu_db_upload for these csv.
 *
 * nsi_upload_tmp_table added.
 * New shall nsi_upload_tmp_table added to truncate the coraspondance table of these csv 
 * and copy again the data in db.
 * 
 */
void start_db_upload_tmp_table_process(int testidx, int pid, char *partition_name);


/*  This function reads/writes dlog offset from/to file .dlog.offset based on flag passed to it.
 *  0 or READ_DLOG_OFFSET
 *  1 or WRITE_DLOG_OFFSET
 */ 
void read_or_write_dlog_offset(int flag)
{
  if (dlog_off_fd < 0) return; 
  if(READ_DLOG_OFFSET == flag)
  {
    NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "In read_or_write_dlog_offset(): READ_DLOG_OFFSET flag = %d", flag);

    //there might be a case when .dlog.offset is not found. (new test is started)
    //dlog_read_offset will be 0 in this case
    if(read(dlog_off_fd, &dlog_read_offset, sizeof(long long)) < sizeof(long long)) {
      //TL info fprintf(stderr, "Error in reading dlog read offset in file.\n");
      dlog_read_offset = 0;
    }
  }
  else if(WRITE_DLOG_OFFSET == flag)
  {
    NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "In read_or_write_dlog_offset(): WRITE_DLOG_OFFSET flag = %d", flag);

    lseek(dlog_off_fd, 0, SEEK_SET);// Seek to beginning

    if(write(dlog_off_fd, (char *)&dlog_read_offset, sizeof(long long)) != sizeof(long long))
    {
      NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_ERROR, "Error in writing dlog read offset in file. Error = %s\n", nslb_strerror(errno));
      fprintf(stderr, "Error in writing dlog read offset in file. Error = %s\n", nslb_strerror(errno));   
      perror(NULL);
    }
  }
}


/*This function will check :
 * (1) If partition switch or 
 * (2) any bufer is 80% full then write all in csv
 *     Currently we are checking for ramining memory size.
 *     If remaining memory size is less than 80% of logging memory
 *     then flush the buffers into files*/
static inline void write_rec_in_csv (int is_partition_switched)
{

  int url_rec_buf_size = urc_write_buf_ptr - urc_write_buffer ;
  int sess_rec_buf_size = src_write_buf_ptr - src_write_buffer;
  int tx_pg_rec_buf_size  = trc_pg_write_buf_ptr  - trc_pg_write_buffer;      
  int tx_rec_buf_size = trc_write_buf_ptr - trc_write_buffer;
  int page_rec_buf_size = prc_write_buf_ptr - prc_write_buffer;
  int page_dump_rec_buf_size = pdrc_write_buf_ptr - pdrc_write_buffer;
  int page_rbu_detail_record_buf_size = prrc_write_buf_ptr - prrc_write_buffer; 
  int rbu_lighthouse_record_buf_size = rlhr_write_buf_ptr - rlhr_write_buffer; 
  int rbu_mark_measure_record_buf_size = rbu_mm_write_buf_ptr - rbu_mm_write_buffer;
 
  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "In write_rec_in_csv(): url_rec_buf_size = %d, sess_rec_buf_size = %d, tx_pg_rec_buf_size = %d, tx_rec_buf_size = %d, page_rec_buf_size = %d, is_partition_switched = %d\n", 
    url_rec_buf_size, sess_rec_buf_size, tx_pg_rec_buf_size, tx_rec_buf_size, page_rec_buf_size, is_partition_switched);

//  fprintf(stderr, "\n  n write_rec_in_csv(): url_rec_buf_size = %d, sess_rec_buf_size = %d, tx_pg_rec_buf_size = %d, tx_rec_buf_size = %d, page_rec_buf_size = %d, is_partition_switched = %d\n",
//    url_rec_buf_size, sess_rec_buf_size, tx_pg_rec_buf_size, tx_rec_buf_size, page_rec_buf_size, is_partition_switched);

  //TIMER... TODO

  //If partition switch or any bufer is 80% full then write all csv 
  if((is_partition_switched) || ((sess_rec_buf_size >= BUFF_80_PCT_FILL_SIZE) || (tx_pg_rec_buf_size >= BUFF_80_PCT_FILL_SIZE) || (tx_rec_buf_size >= BUFF_80_PCT_FILL_SIZE) || (page_rec_buf_size >= BUFF_80_PCT_FILL_SIZE) || (url_rec_buf_size >= BUFF_80_PCT_FILL_SIZE) || (page_dump_rec_buf_size >= BUFF_80_PCT_FILL_SIZE) || (page_rbu_detail_record_buf_size >= BUFF_80_PCT_FILL_SIZE) || (rbu_lighthouse_record_buf_size >= BUFF_80_PCT_FILL_SIZE) || (rbu_mark_measure_record_buf_size >= BUFF_80_PCT_FILL_SIZE)))
  {
    //offset should be saved before writing to csv and offset should be written in binary using write
    read_or_write_dlog_offset(WRITE_DLOG_OFFSET);

    WRITE_CSV_FILE(src_out_fptr, sess_rec_buf_size, src_write_buffer, src_write_buf_ptr, "src.csv");

    WRITE_CSV_FILE(tptb_out_fptr, tx_pg_rec_buf_size, trc_pg_write_buffer, trc_pg_write_buf_ptr, "tprc.csv"); 

    WRITE_CSV_FILE(trc_out_fptr, tx_rec_buf_size, trc_write_buffer, trc_write_buf_ptr, "trc.csv");

    WRITE_CSV_FILE(prc_out_fptr, page_rec_buf_size, prc_write_buffer, prc_write_buf_ptr, "prc.csv");

    WRITE_CSV_FILE(urc_out_fptr, url_rec_buf_size, urc_write_buffer, urc_write_buf_ptr, "urc.csv");

    WRITE_CSV_FILE(page_dump_out_fptr, page_dump_rec_buf_size, pdrc_write_buffer, pdrc_write_buf_ptr, "page_dump.csv");

    WRITE_CSV_FILE(page_rbu_detail_record_fptr, page_rbu_detail_record_buf_size, prrc_write_buffer, prrc_write_buf_ptr, "PageRBUDetailRecord.csv");
    WRITE_CSV_FILE(rbu_lighthouse_record_fptr, rbu_lighthouse_record_buf_size, rlhr_write_buffer, rlhr_write_buf_ptr, "RBULightHouseRecord.csv");
    WRITE_CSV_FILE(rbu_mark_measure_record_fptr, rbu_mark_measure_record_buf_size, rbu_mm_write_buffer, rbu_mm_write_buf_ptr, "RBUUserTiming.csv");
   
    time_after_csv_write = time(NULL);
  }
}

int create_run_profile_entry(int* row_num) {
  if (total_run_profile_entries == max_run_profile_entries) {
    runProfile = (RunProfileLogEntry *) realloc ((char *)runProfile,
				   (max_run_profile_entries + DELTA_RUNPROFLOG_ENTRIES) *
				   sizeof(RunProfileLogEntry));
    if (!runProfile) {
      printf("create_run_profile_entry(): Error allocating more memory for runprof entries\n");
      return(FAILURE);
    } else max_run_profile_entries += DELTA_RUNPROFLOG_ENTRIES;
  }
  *row_num = total_run_profile_entries++;
  return (SUCCESS);
}

int create_user_profile_entry(int* row_num) {
  if (total_user_profile_entries == max_user_profile_entries) {
    userProfile = (UserProfileLogEntry *) realloc ((char *)userProfile,
				   (max_user_profile_entries + DELTA_USERPROFLOG_ENTRIES) *
				   sizeof(UserProfileLogEntry));
    if (!userProfile) {
      return(FAILURE);
    } else max_user_profile_entries += DELTA_USERPROFLOG_ENTRIES;
  }
  *row_num = total_user_profile_entries++;
  return (SUCCESS);
}

int create_session_profile_entry(int* row_num) {
  if (total_sess_profile_entries == max_sess_profile_entries) {
    sessProfile = (SessionProfileLogEntry *) realloc ((char *)sessProfile,
				   (max_sess_profile_entries + DELTA_SESSPROFLOG_ENTRIES) *
				   sizeof(SessionProfileLogEntry));
    if (!sessProfile) {
      return(FAILURE);
    } else max_sess_profile_entries += DELTA_SESSPROFLOG_ENTRIES;
  }
  *row_num = total_sess_profile_entries++;
  return (SUCCESS);
}

int create_recsvr_table_entry(int* row_num) {
  if (total_recsvr_table_entries == max_recsvr_table_entries) {
    recSvrTable = (RecSvrTableLogEntry *) realloc ((char *)recSvrTable,
				   (max_recsvr_table_entries + DELTA_RECSVRTABLELOG_ENTRIES) *
				   sizeof(RecSvrTableLogEntry));
    if (!recSvrTable) {
      return(FAILURE);
    } else max_recsvr_table_entries += DELTA_RECSVRTABLELOG_ENTRIES;
  }
  *row_num = total_recsvr_table_entries++;
  return (SUCCESS);
}

int create_actsvr_table_entry(int* row_num) {
  if (total_actsvr_table_entries == max_actsvr_table_entries) {
    actSvrTable = (ActSvrTableLogEntry *) realloc ((char *)actSvrTable,
				   (max_actsvr_table_entries + DELTA_ACTSVRTABLELOG_ENTRIES) *
				   sizeof(ActSvrTableLogEntry));
    if (!actSvrTable) {
      return(FAILURE);
    } else max_actsvr_table_entries += DELTA_ACTSVRTABLELOG_ENTRIES;
  }
  *row_num = total_actsvr_table_entries++;
  return (SUCCESS);
}

int create_phase_table_entry(int* row_num) {
  //printf("creating phase table: row_num = %d", *row_num);
  if (total_phase_table_entries == max_phase_table_entries) {
    phaseTable = (PhaseTableLogEntry *) realloc ((char *)phaseTable,
				   (max_phase_table_entries + DELTA_PHASETABLELOG_ENTRIES) *
				   sizeof(PhaseTableLogEntry));
    if (!phaseTable) {
      return(FAILURE);
    } else max_phase_table_entries += DELTA_PHASETABLELOG_ENTRIES;
  }
  *row_num = total_phase_table_entries++;
  return (SUCCESS);
}

int find_userprof_entry(unsigned int userprof_id, char* userprof_type, unsigned int value_id) {
  int i;

  for (i = 0; i < total_user_profile_entries; i++) {
    if ((userProfile[i].userprof_id == userprof_id) &&
	!(strcmp(userProfile[i].userprof_type, userprof_type)) &&
	(userProfile[i].value_id == value_id))
      return i;
  }

  return -1;
}

char* translate_userprof(unsigned int id) {
  int i;

  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called translate_userprof(). id = %d", id);

  for (i = 0; i < total_user_profile_entries; i++) {
    NSLB_TRACE_LOG4(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Checking userProfile[%d].userprof_id = %d", i, userProfile[i].userprof_id);
    if (userProfile[i].userprof_id == id)
    {
      NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "User profile found for id = %d. userprof_name = %s", id, userProfile[i].userprof_name);
      return userProfile[i].userprof_name;
    }
  }
  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "User profile not found for id = %d", id);
  return NULL;
}

char* translate_access(unsigned int id) {
  int i;

  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. id = %d", id);

  for (i = 0; i < total_user_profile_entries; i++) {
    NSLB_TRACE_LOG4(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Checking userProfile[%d].value_id = %d, userprof_type = %s", i, userProfile[i].value_id, userProfile[i].userprof_type);
    if ((!(strcmp(userProfile[i].userprof_type, "ACCESS"))) && (userProfile[i].value_id == id))
    {
      NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Access found for value_id = %d. access_name = %s", id, userProfile[i].value);
      return userProfile[i].value;
    }
  }
  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Access not found for id = %d", id);
  return NULL;
}

char* translate_location(unsigned int id) {
  int i;

  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. id = %d", id);

  //for (i = locattr_start; i < total_user_profile_entries; i++) {
  for (i = 0; i < total_user_profile_entries; i++) {
    NSLB_TRACE_LOG4(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Checking userProfile[%d].value_id = %d", i, userProfile[i].value_id);
    if ((!(strcmp(userProfile[i].userprof_type, "LOCATION"))) && (userProfile[i].value_id == id))
    {
      NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Access found for value_id = %d. access_name = %s", id, userProfile[i].value);
      return userProfile[i].value;
    }
  }
  return NULL;
}

char* translate_browser(unsigned int id) {
  int i;

  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. id = %d", id);

  for (i = 0; i < total_user_profile_entries; i++) {
    if ((!(strcmp(userProfile[i].userprof_type, "BROWSER"))) && (userProfile[i].value_id == id))
    {
      NSLB_TRACE_LOG4(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, " Value_id(Browser id) is = %u", userProfile[i].value_id);
      return userProfile[i].value;
    }
  }
  return NULL;
}

char* translate_freq(unsigned int id) {
  int i;


  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called translate_freq(). id = %d", id);

  for (i = 0; i < total_user_profile_entries; i++) {
    if (userProfile[i].value_id == id)
      return userProfile[i].value;
  }
  return NULL;
}

char* translate_machine(unsigned int id) {
  int i;
  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. id = %d", id);

  for (i = 0; i < total_user_profile_entries; i++) {
    if (userProfile[i].value_id == id)
      return userProfile[i].value;
  }
  return NULL;
}

char* translate_sessprof(unsigned int id) {
  int i;


  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. id = %d", id);

  for (i = 0; i < total_sess_profile_entries; i++) {
    if (sessProfile[i].sessprof_id == id)
      NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Value_id(SessionProf id) is = %u", sessProfile[i].sessprof_id);
      return sessProfile[i].sessprof_name;
      NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Session profile name is = %s ", sessProfile[i].sessprof_name);
  }
  return NULL;
}

char* translate_sess(unsigned int id) {
  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. id = %d", id);

  /*for (i = 0; i < total_sess_table_entries; i++) {
    if (sessTable[i].sess_id == id)
      NSLB_TRACE_LOG1(lr_trace_log_key, 1, NULL, ERROR,  "Sess id is = %u", sessTable[i].sess_id);
      return sessTable[i].sess_name;
      NSLB_TRACE_LOG1(lr_trace_log_key, 1, NULL, ERROR,  "session instance = %s", sessTable[i].sess_name);
  }*/
  return NULL;
}

char* translate_gen(unsigned int id){
  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. id = %d", id);
  return NULL;
}

char* translate_group(unsigned int id){
  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. id = %d", id);
  return NULL;
}

char* translate_tx(unsigned int id) {

  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. id = %d", id);
/*
  if (id == -1)
    return NULL;

  for (i = 0; i < total_tx_table_entries; i++) {
    if (txLogTable[i].tx_id == id)
      return txLogTable[i].tx_name;
  }*/
  return NULL;
}

char* translate_pg(unsigned int id) {
  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. id = %d", id);


/*  for (i = 0; i < total_page_table_entries; i++) {
    if (pageTable[i].page_id == id)
      return pageTable[i].page_name;
  }*/
  return NULL;
}

char* translate_url(unsigned int id) {
  //int i;
  
  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called");
  //NSLB_TRACE_LOG1(lr_trace_log_key, 2, NULL, NSLB_TL_INFO, "id = %d", id);

/*  for (i = 0; i < total_url_table_entries; i++) {
    if (urlTable[i].url_id == id)
      return urlTable[i].url_name;
  } */
  return NULL;
}

// Return string to unsigned Long
// Note: atol() does not work when number become -ve
static inline u_ns_ptr_t ns_atoul(char *in_buf)
{
  return(((u_ns_ptr_t)strtoul(in_buf, NULL, 10)));
}

//#define ns_id_str_to_num(str) (unsigned int)atoi(str);
#define VALUE_LEN 512
static int write_generator_table_in_csv(char *input_data, char *csv_buf, char *delim, int test_id)
{
  int total_flds, csv_buf_len, generator_id, generator_len, generator_name_len;
  //char generator_ip[16], generator_resolved_ip[128], generator_work[512], generator_agent_port[6];
  char *field[10];
  char generator_name[VALUE_LEN + 1] = {0};
  char generator_ip[VALUE_LEN + 1] = {0};
  char generator_resolved_ip[VALUE_LEN + 1] = {0};
  char generator_work[VALUE_LEN + 1] = {0};
  char generator_agent_port[VALUE_LEN + 1] = {0};

  NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. input_data = %s , delim = %s, test_id = %d", input_data, delim, test_id);

  total_flds = get_tokens(input_data, field, delim, 10);

  NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. total_flds = %d", total_flds);
    
  //input_data will be like this: 0,hpd_tours_c_
  //strcpy(generator_name, field[0]);
  //strcpy(generator_ip, field[1]);
  //strcpy(generator_resolved_ip ,field[2]);
  //strcpy(generator_work, field[3]);
  //strcpy(generator_agent_port, field[4]);
 
  //NAME,ID,WORK,IP,RESOLVED IP,CAVMON PORT
  //MAC_60,0,/home/cavisson/Controller_1,192.168.1.60,192.168.1.60,7891  

  //Find generator name length
  generator_name_len = strlen(field[0]);
  if(generator_name_len > VALUE_LEN)
  {
    NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_WARNING, "generator name length (%d) is more than max len allowed (%d). ", generator_name_len, VALUE_LEN);
    generator_name_len = VALUE_LEN;
  }
  memcpy(generator_name, field[0], generator_name_len); // Using memcpy as it is faster than strcpy
  generator_name[generator_name_len] = '\0'; // NULL Terminate
  
  //Generator ID 
  generator_id = atoi(field[1]);

  //Find generator work length
  generator_len = strlen(field[2]);
  if(generator_len > VALUE_LEN)
  {
    NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_WARNING, "generator work length (%d) is more than max len allowed (%d). ", generator_len, VALUE_LEN);
    generator_len = VALUE_LEN;
  }
  memcpy(generator_work, field[2], generator_len);
  generator_work[generator_len] = '\0';

  //Find generator IP length
  generator_len = strlen(field[3]);
  if(generator_len > VALUE_LEN)
  {
    NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_WARNING, "generator IP length (%d) is more than max len allowed (%d). ", generator_len, VALUE_LEN);
    generator_len = VALUE_LEN;
  }
  memcpy(generator_ip, field[3], generator_len);
  generator_ip[generator_len] = '\0';

  //Find generator Resolved-IP length
  generator_len = strlen(field[4]);
  if(generator_len > VALUE_LEN)
  {
    NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_WARNING, "generator resolved IP length (%d) is more than max len allowed (%d). ", generator_len, VALUE_LEN);
    generator_len = VALUE_LEN;
  }
  memcpy(generator_resolved_ip, field[4], generator_len);
  generator_resolved_ip[generator_len] = '\0';

  //Find generator cavmon port length
  generator_len = strlen(field[5]);
  if(generator_len > VALUE_LEN)
  {
    NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_WARNING, "generator cavmon port length (%d) is more than max len allowed (%d). ", generator_len, VALUE_LEN);
    generator_len = VALUE_LEN;
  }
  memcpy(generator_agent_port, field[5], generator_len);
  generator_agent_port[generator_len] = '\0';

  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "generator_id = %d generator_name_len = %d generator_name = %s generator_ip = %s generator_resolved_ip = %s generator_work =%s generator_agent_port = %s", generator_id, generator_name_len, generator_name, generator_ip, generator_resolved_ip, generator_work, generator_agent_port);

  int is_new_generator = -1;

  unsigned int tGeneratorId = gen_generator_norm_id(generator_name, generator_id, generator_name_len, &is_new_generator);

  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "is_new_generator = %d", is_new_generator);

  if(is_new_generator == 0) //Already in our data base
    return 0;
  
  //This is new id, so write into generator_table.csv file
  csv_buf_len = sprintf(csv_buf, "%s,%u,%s,%s,%s,%s", generator_name, tGeneratorId, generator_work, generator_ip, generator_resolved_ip, generator_agent_port);
  
  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "csv_buf = %s, csv_buf_len = %d", csv_buf, csv_buf_len);
  
  fprintf(generator_out_fptr, "%s\n", csv_buf);
  
  //Doing flush here because there is posibblity that fprintf buffered the data bcoz of this drill down will not get data in sst.csv file
  fflush(generator_out_fptr);
  return(csv_buf_len);
  
}

static int write_group_table_in_csv(char *input_data, char *csv_buf, char *delim, int test_id)
{
  short pct;
  int group_len, total_flds, csv_buf_len, group_num, userprof_id, sessprof_id;
  char group_name[VALUE_LEN + 1] = {0};
  char *field[10];

  NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. input_data = %s , delim = %s, test_id = %d", input_data, delim, test_id);

  total_flds = get_tokens(input_data, field, delim, 10);

  NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. total_flds = %d", total_flds);
  //RUNPROFILE:0,0,0,20,G1
  //RUNPROFILE:0,0,12297829382473036970,1,g2
  group_num = atoi(field[0]);
  userprof_id = atoi(field[1]);
  sessprof_id = atoi(field[2]);
  pct = atoi(field[3]);

  group_len = strlen(field[4]);
  if(group_len > VALUE_LEN)
  {
    NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_WARNING, "group length (%d) is more than max len allowed (%d). ", group_len, VALUE_LEN);
    group_len = VALUE_LEN;
  }
  memcpy(group_name, field[4], group_len); // Using memcpy as it is faster than strcpy
  group_name[group_len] = '\0'; // NULL Terminate

  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "group_num = %d group_name = %s userprof_id = %d sessprof_id = %d pct = %d, group_len = %d", group_num, group_name, userprof_id, sessprof_id, pct, group_len);

  int is_new_group = -1;

  unsigned int tGroupId = gen_group_norm_id(group_num, group_name, userprof_id, sessprof_id, pct, &is_new_group, group_len);

  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "is_new_group = %d", is_new_group);

  if(is_new_group == 0) //Already in our data base
    return 0;
  
  //This is new id, so write into rpf.csv file
  //125814,0,0,-1,2,g2
  csv_buf_len = sprintf(csv_buf, "%d,%u,%d,%d,%hd,%s", test_id, tGroupId, userprof_id, sessprof_id, pct, group_name);
  
  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "csv_buf = %s, csv_buf_len = %d", csv_buf, csv_buf_len);
  
  fprintf(rpr_out_fptr, "%s\n", csv_buf);
  
  fflush(rpr_out_fptr);
  return(csv_buf_len);

}

static int write_host_table_in_csv(char *input_data, char *csv_buf, char *delim, int test_id)
{
  char *field[10];
  char host_name[VALUE_LEN + 1] = {0};
  int total_flds, host_len, host_id, gen_id, csv_buf_len, nvm_id;

  NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. input_data = %s , delim = %s, test_id = %d", input_data, delim, test_id);
  total_flds = get_tokens(input_data, field, delim, 10); 
  
  NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. total_flds = %d", total_flds);
  //HOSTTABLE: 10.10.70.5:9008,0,0,0
  //HOSTTABLE: HOST,HOST_ID,GEN_ID,NVM_ID
  host_len = strlen(field[0]);
  host_id = atoi(field[1]);
  gen_id = atoi(field[2]);
  nvm_id = atoi(field[3]);

  if(host_len > VALUE_LEN) 
  {
    NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_WARNING, "host length (%d) is more than max len allowed (%d). ", host_len, VALUE_LEN);
    host_len = VALUE_LEN;
  }
  memcpy(host_name, field[0], host_len); // Using memcpy as it is faster than strcpy
  host_name[host_len] = '\0'; // NULL Terminate

  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "host_id = %d, host_len = %d, host_name = %s", host_id, host_len, host_name);

  int is_new_host = -1;

  unsigned int tHostId = gen_host_norm_id(&is_new_host, host_id, host_len, host_name, gen_id, nvm_id);
  //unsigned int tHostId = get_host_norm_id_for_url(0, host_id, host_name, host_len, &is_new_host, gen_id);

  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "is_new_host = %d, tHostId = %d", is_new_host, tHostId);

  if(is_new_host == 0) //Already in our data base
    return 0;

  csv_buf_len = sprintf(csv_buf, "%d,%s", tHostId, host_name);

  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "csv_buf = %s, csv_buf_len = %d", csv_buf, csv_buf_len);

  fprintf(host_out_fptr, "%s\n", csv_buf);

  fflush(host_out_fptr);
  return(csv_buf_len);
}

// Return 0 if not to be added else csv_buf_len
static int write_tx_table_in_csv_v2(char *input_data, char *csv_buf, char *delim, int test_id) 
{
  int total_flds, csv_buf_len, tx_len;
  unsigned int tx_id;
  unsigned int nvm_id;
  unsigned int gen_id;
  char *field[10];
  char tx_name[MAX_BUFFER_LEN + 1] = {0};

  NSLB_TRACE_LOG4(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. input_data = %s , delim = %s, test_id = %d", input_data, delim, test_id);
  total_flds = get_tokens(input_data, field, delim, 10);

  NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. total_flds = %d", total_flds);

  //input_data will be like this: 4,tx_hpd_tours_c_1_findflight
  tx_id = atoi(field[0]);
  nvm_id = atoi(field[1]);
  gen_id = atoi(field[2]);
  tx_len = atoi(field[3]);

  if(tx_len > MAX_BUFFER_LEN)
  {
    NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_WARNING, "TX length (%d) is more than max len allowed (%d).", tx_len, MAX_BUFFER_LEN);
    tx_len = MAX_BUFFER_LEN;
  }
  memcpy(tx_name, field[4], tx_len); // Using memcpy as it is faster than strcpy
  tx_name[tx_len] = '\0'; // NULL Terminate
  
  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "tx_id = %d tx_len = %d tx = %s", tx_id, tx_len, tx_name);

  int is_new_tx = -1;
  unsigned int tTxIndxId = gen_tx_norm_id_v2(tx_id, tx_name, tx_len, &is_new_tx, nvm_id, gen_id);

  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "is_new_tx = %d", is_new_tx);

  if(is_new_tx == 0)//Already in our data base
    return 0;

  //This is new id, so write into trt.csv file
  csv_buf_len = sprintf(csv_buf, "%d,%d,%s", test_id, tTxIndxId, tx_name); 
 
  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "csv_buf = %s, csv_buf_len = %d", csv_buf, csv_buf_len);
  
  fprintf(ttb_out_fptr, "%s\n", csv_buf);
  //Doing flush here because there is posibblity that fprintf buffered the data bcoz of this drill down will not get data in trt.csv file 
  fflush(ttb_out_fptr);
  return(csv_buf_len);
}

// Return 0 if not to be added else csv_buf_len
static int write_tx_table_in_csv(char *input_data, char *csv_buf, char *delim, int test_id) 
{
  int total_flds, csv_buf_len, tx_len;
  unsigned int tx_id;
  char *field[10];
  char tx_name[MAX_BUFFER_LEN + 1] = {0};

  NSLB_TRACE_LOG4(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. input_data = %s , delim = %s, test_id = %d", input_data, delim, test_id);
  total_flds = get_tokens(input_data, field, delim, 10);

  NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. total_flds = %d", total_flds);

  //input_data will be like this: 4,tx_hpd_tours_c_1_findflight
  tx_id = atoi(field[0]);
  tx_len = strlen(field[1]);

  if(tx_len > MAX_BUFFER_LEN)
  {
    NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_WARNING, "TX length (%d) is more than max len allowed (%d).", tx_len, MAX_BUFFER_LEN);
    tx_len = MAX_BUFFER_LEN;
  }
  memcpy(tx_name, field[1], tx_len); // Using memcpy as it is faster than strcpy
  tx_name[tx_len] = '\0'; // NULL Terminate
  
  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "tx_id = %d tx_len = %d tx = %s", tx_id, tx_len, tx_name);

  int is_new_tx = -1;
  unsigned int tTxIndxId = gen_tx_norm_id(tx_id, tx_name, tx_len, &is_new_tx);

  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "is_new_tx = %d", is_new_tx);

  if(is_new_tx == 0)//Already in our data base
    return 0;

  //This is new id, so write into trt.csv file
  csv_buf_len = sprintf(csv_buf, "%d,%d,%s", test_id, tTxIndxId, tx_name); 
 
  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "csv_buf = %s, csv_buf_len = %d", csv_buf, csv_buf_len);
  
  fprintf(ttb_out_fptr, "%s\n", csv_buf);
  //Doing flush here because there is posibblity that fprintf buffered the data bcoz of this drill down will not get data in trt.csv file 
  fflush(ttb_out_fptr);
  return(csv_buf_len);
}

// Return 0 if not to be added else csv_buf_len
  // CSV File: sst.csv
  // DB Table Name: SessionTable_<TR>
  // test_id,sess_id,sess_name
  // Example:
  // 2086,0,Tours_CMode
static int write_session_table_in_csv(char *input_data, char *csv_buf, char *delim, int test_id) 
{
  int total_flds, csv_buf_len, session_len;
  unsigned int session_id;
  char *field[10];
  char session_name[MAX_BUFFER_LEN + 1] = {0};

  NSLB_TRACE_LOG4(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. input_data = %s , delim = %s, test_id = %d", input_data, delim, test_id);
  total_flds = get_tokens(input_data, field, delim, 10);

  NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. total_flds = %d", total_flds);

  //input_data will be like this: 0,hpd_tours_c_
  session_id = atoi(field[0]);
  session_len = strlen(field[1]);

  if(session_len > MAX_BUFFER_LEN)
  {
    NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_WARNING, "SESSION length (%d) is more than max len allowed (%d). ", session_len, MAX_BUFFER_LEN);
    session_len = MAX_BUFFER_LEN;
  }
  memcpy(session_name, field[1], session_len); // Using memcpy as it is faster than strcpy
  session_name[session_len] = '\0'; // NULL Terminate
  
  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "session_id = %d session_len = %d session = %s", session_id, session_len, session_name);

  int is_new_session = -1;

  unsigned int tSessionIndxId = gen_session_norm_id(session_id, session_name, session_len, &is_new_session);

  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "is_new_session = %d", is_new_session);

  if(is_new_session == 0) //Already in our data base
    return 0;

  //This is new id, so write into sst.csv file
  csv_buf_len = sprintf(csv_buf, "%d,%d,%s", test_id, tSessionIndxId, session_name); 
  
  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "csv_buf = %s, csv_buf_len = %d", csv_buf, csv_buf_len);
  
  fprintf(sesstb_out_fptr, "%s\n", csv_buf);
  //Doing flush here because there is posibblity that fprintf buffered the data bcoz of this drill down will not get data in sst.csv file 
  fflush(sesstb_out_fptr);
  return(csv_buf_len);
}

// Return 0 if not to be added else csv_buf_len
  // CSV File: pgt.csv
  // DB Table Name: PageTable_<TR>
  // test_id,page_id,sess_id,page_name
  // Example:
  // 2086,0,0,Welcome
  // 2086,1,0,Login
  // 2086,2,0,FindFlight
  // 2086,3,0,ChooseFlight
  // 2086,4,0,PurchaseFlight
  // 2086,5,0,Confirmation
  // 2086,6,0,Logout

static int write_page_table_in_csv(char *input_data, char *csv_buf, char *delim, int test_id) 
{
  int total_flds, csv_buf_len, page_len, sess_name_len;
  unsigned int page_id, session_id;
  char *field[10];
  char page_name[MAX_BUFFER_LEN + 1] = {0};
  char session_name[MAX_BUFFER_LEN + 1] = {0};
  char session_and_page_name[MAX_BUFFER_LEN + MAX_BUFFER_LEN + 1] = {0};

  NSLB_TRACE_LOG4(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. input_data = %s , delim = %s, test_id = %d", input_data, delim, test_id);
  total_flds = get_tokens(input_data, field, delim, 10);

  NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. total_flds = %d", total_flds);

  //input_data will be like this: 1,0,login,session_name
  page_id = atoi(field[0]);
  session_id = atoi(field[1]);
  page_len = strlen(field[2]);

  if(page_len > MAX_BUFFER_LEN)
  {
    NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_WARNING, "PAGE length (%d) is more than max len allowed (%d). ", page_len, MAX_BUFFER_LEN);
    page_len = MAX_BUFFER_LEN;
  }
  memcpy(page_name, field[2], page_len); // Using memcpy as it is faster than strcpy
  page_name[page_len] = '\0'; // NULL Terminate
 
  sess_name_len = strlen(field[3]); 

  if(sess_name_len > MAX_BUFFER_LEN)
  {
    NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_WARNING, "SESSION length (%d) is more than max len allowed (%d). ", sess_name_len, MAX_BUFFER_LEN);
    sess_name_len = MAX_BUFFER_LEN;
  }
  memcpy(session_name, field[3], sess_name_len); 
  session_name[sess_name_len] = '\0'; // NULL Terminate
  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "session_id = %d sess_name_len = %d session = %s", session_id, sess_name_len, session_name);

  /* In order of page normalization, we will combine session name with page name
   * */
  int combine_sess_page_name_len = sprintf(session_and_page_name, "%s:%s", session_name, page_name); 
  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Combine session name and page name %s length %d", session_and_page_name, combine_sess_page_name_len);

  int is_new_page = -1;
  unsigned int tPageIndxId = gen_page_norm_id(page_id, session_and_page_name, combine_sess_page_name_len, &is_new_page);
  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "is_new_page = %d", is_new_page);

  if(is_new_page == 0) //Already in our data base
    return 0;

  unsigned int session_norm_idx = get_session_norm_id(session_id);

  //This is new id, so write into sst.csv file
  if (inline_output) 
    csv_buf_len = sprintf(csv_buf, "%d,%d,%s,%s", test_id, tPageIndxId, translate_sess(session_id), page_name); 
  else 
    csv_buf_len = sprintf(csv_buf, "%d,%d,%d,%s,%d,%s", test_id, tPageIndxId, session_norm_idx, page_name, combine_sess_page_name_len, session_and_page_name); 

  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "csv_buf = %s, csv_buf_len = %d", csv_buf, csv_buf_len);
  
  fprintf(ptb_out_fptr, "%s\n", csv_buf);
  //Doing flush here because there is posibblity that fprintf buffered the data bcoz of this drill down will not get data in pgt.csv file 
  fflush(ptb_out_fptr);
  return(csv_buf_len);
}

// Return 0 if not to be added else csv_buf_len
static int write_url_table_in_csv(char *input_data, char *csv_buf, char *delim, int test_id) {
  int total_flds, csv_buf_len, url_len, page_len;
  unsigned int url_id;
  unsigned int page_id;
  unsigned int hash_id;
  unsigned int gen_id;
  char *field[10];
  char url_name[MAX_BUFFER_LEN + 1] = {0};
  char page_name[MAX_BUFFER_LEN + 1] = {0};
  char page_and_url_name[MAX_BUFFER_LEN + MAX_BUFFER_LEN + 1] = {0};

  NSLB_TRACE_LOG4(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. input_data = %s , delim = %s, test_id = %d", input_data, delim, test_id);
  total_flds = get_tokens(input_data, field, delim, 10);

  NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called. total_flds = %d", total_flds);


  /*UrlTableLogEntry tmp_url_Table;
  tmp_url_Table.url_id = ns_id_str_to_num(field[0]);
  tmp_url_Table.pg_id = ns_id_str_to_num(field[1]);
  tmp_url_Table.url_hash_id = ns_id_str_to_num(field[2]);
  tmp_url_Table.url_hash_code = ns_id_str_to_num(field[3]);
  tmp_url_Table.len = ns_id_str_to_num(field[4]);*/

  url_id = atoi(field[0]);
  page_id = atoi(field[1]);
  hash_id = atoi(field[2]);
  gen_id = atoi(field[3]);
  page_len = strlen(field[4]);
  url_len = atoi(field[5]);
 
  if(url_len > MAX_BUFFER_LEN)
  {
    NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_WARNING, "URL length (%d) is more than max len allowed (%d). ", url_len, MAX_BUFFER_LEN);
    url_len = MAX_BUFFER_LEN;
  }
  memcpy(url_name, field[6], url_len); // Using memcpy as it is faster than strcpy
  url_name[url_len] = '\0'; // NULL Terminate
  
  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "url_id = %d url_len = %d url = %s, gen_id = %d", url_id, url_len, url_name, gen_id);

  if(page_len > MAX_BUFFER_LEN)
  {
    NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_WARNING, "Page name length (%d) is more than max len allowed (%d). ", page_len, MAX_BUFFER_LEN);
    page_len = MAX_BUFFER_LEN;
  }
  memcpy(page_name, field[4], page_len); 
  page_name[page_len] = '\0'; // NULL Terminate
  
  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "page_id = %d page_len = %d page_name = %s", page_id, page_len, page_name);
  /* In order of page normalization, we will combine session name with page name
   * */
  int combine_page_url_name_len = sprintf(page_and_url_name, "%s:%s", page_name, url_name); 
  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Combine page name and url name %s length %d", page_and_url_name, combine_page_url_name_len);
  int is_new_url = -1;
  unsigned int tUrlIndxId = gen_url_norm_id(url_id, page_and_url_name, combine_page_url_name_len, &is_new_url, gen_id);

  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "is_new_url = %d", is_new_url);

  if(is_new_url == 0)
    return 0;

  csv_buf_len = sprintf(csv_buf, "%d,%d,%d,%u,%u,%d,%s,%d,%s", test_id, tUrlIndxId, page_id, hash_id, gen_id, url_len, url_name, combine_page_url_name_len, page_and_url_name); 
  
  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "csv_buf = %s, csv_buf_len = %d", csv_buf, csv_buf_len);
  
  fprintf(utb_out_fptr, "%s\n", csv_buf);
  //Doing flush here because there is posibblity that fprintf buffered the data bcoz of this drill down will not get data in urt.csv file 
  fflush(utb_out_fptr);
  return(csv_buf_len);
}

static void init_total_entries_count()
{
  total_run_profile_entries = 0;
  total_user_profile_entries = 0;
  total_sess_profile_entries = 0;
  total_actsvr_table_entries = 0;
  total_recsvr_table_entries = 0;
  total_phase_table_entries = 0;
}

void reset_tables(void) {
  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called");
  memset(runProfile, 0, max_run_profile_entries * sizeof(RunProfileLogEntry));
  memset(userProfile, 0, max_user_profile_entries  * sizeof(UserProfileLogEntry));
  memset(sessProfile, 0, max_sess_profile_entries * sizeof(SessionProfileLogEntry));
  memset(actSvrTable, 0, max_actsvr_table_entries * sizeof(ActSvrTableLogEntry));
  memset(phaseTable, 0, max_phase_table_entries * sizeof(PhaseTableLogEntry));
  memset(recSvrTable, 0, max_recsvr_table_entries * sizeof(RecSvrTableLogEntry));

  init_total_entries_count();

}

void input_static_file(void) {
  int row_num = 0;
  char buf[LINE_LENGTH];
  char entry_name[LINE_LENGTH];
  char entry_data[LINE_LENGTH];
  int entry_size;
  char* token;
  unsigned int userprof_id;
  char userprof_name[LINE_LENGTH];
  char userprof_type[LINE_LENGTH];
  unsigned int value_id;
  char value[LINE_LENGTH];
  int pct;
  int idx;
  int buf_len;
  char *ptr = NULL;

  NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "INSIDE FUNCTION input_static_file(): test_id = %d, "
                       "slog_offset = %d", test_id, slog_offset);

  //reset all table entries count to 0
  reset_tables();
  
  while (fgets(buf, LINE_LENGTH, s_in_fptr)) {
    buf_len = strlen(buf);
    slog_offset += buf_len;
    if (buf_len > 0)
      buf[buf_len - 1] = '\0';

    NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Buffer data = %s", buf);
    ptr = strchr(buf, ':');
    if(ptr == NULL){
      fprintf(stderr, "\nError: Got invalid record. Buffer data = [%s]\n", buf);
      continue;
    }
    entry_size = ptr - buf;
    strncpy(entry_name, buf, entry_size);
    entry_name[entry_size] = 0;
    strcpy(entry_data, ptr+1);
    NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "entry_name = %s", entry_name);
    //printf("\n**entry_name = [%s]\n", entry_name);

    /* BUG Fixed: 1217 Mon Sep 27 17:29:38 IST 2010  changed atol -> ns_atoul, as atol does not work on unsigned Long*/
    /* Neeraj - Change remaining atol to ns_atoul on Oct 2, 2010 (e.g. in Url table etc) */
#if 0
    if (strncmp(entry_name, "RUNPROFSTART", strlen("RUNPROFSTART")) == 0) {
      runprof_start = (RunProfTableEntry_Shr*) ns_atoul(entry_data);
    } else if (strncmp(entry_name, "USERPROFSTART", strlen("USERPROFSTART")) == 0) {
      userprof_start = (UserProfIndexTableEntry_Shr*) ns_atoul(entry_data);
    } else if (strncmp(entry_name, "LOCATTRSTART", strlen("LOCATTRSTART")) == 0) {
      locattr_start = (LocAttrTableEntry_Shr *) ns_atoul(entry_data);
    } else if (strncmp(entry_name, "ACCATTRSTART", strlen("ACCATTRSTART")) == 0) {
      accattr_start = (AccAttrTableEntry_Shr *) ns_atoul(entry_data);
    } else if (strncmp(entry_name, "BROWATTRSTART", strlen("BROWATTRSTART")) == 0) {
      browattr_start = (BrowAttrTableEntry_Shr *) ns_atoul(entry_data);
    } else if (strncmp(entry_name, "FREQATTRSTART", strlen("FREQATTRSTART")) == 0) {
      freqattr_start = (FreqAttrTableEntry_Shr *) ns_atoul(entry_data);
    } else if (strncmp(entry_name, "MACHATTRSTART", strlen("MACHATTRSTART")) == 0) {
      machattr_start = (MachAttrTableEntry_Shr *) ns_atoul(entry_data);
    } else if (strncmp(entry_name, "SESSPROFSTART", strlen("SESSPROFSTART")) == 0) {
      sessprof_start = (SessProfIndexTableEntry_Shr *) ns_atoul(entry_data);
    } else if (strncmp(entry_name, "SESSTABLESTART", strlen("SESSTABLESTART")) == 0) {
      sess_start = (SessTableEntry_Shr*) ns_atoul(entry_data);
    } else if (strncmp(entry_name, "PAGETABLESTART", strlen("PAGETABLESTART")) == 0) {
      page_start = (PageTableEntry_Shr*) ns_atoul(entry_data);
    } else if (strncmp(entry_name, "TXTABLESTART", strlen("TXTABLESTART")) == 0) {
      tx_start = (TxTableEntry_Shr*) ns_atoul(entry_data);
    } else if (strncmp(entry_name, "URLTABLESTART", strlen("URLTABLESTART")) == 0) {
      url_start = (action_request_Shr*) ns_atoul(entry_data);
    } else if (strncmp(entry_name, "RECSVRTABLESTART", strlen("RECSVRTABLESTART")) == 0) {
      recsvr_start = (SvrTableEntry_Shr*) ns_atoul(entry_data);
    } else if (strncmp(entry_name, "ACTSVRTABLESTART", strlen("ACTSVRTABLESTART")) == 0) {
      actsvr_start = (TotSvrTableEntry_Shr*) ns_atoul(entry_data);
    } 
#endif
    if (strncmp(entry_name, "RUNPROFILE", strlen("RUNPROFILE")) == 0) {
#if 0
      if (create_run_profile_entry(&row_num) == FAILURE)
	      exit(-1);
      token = strtok(entry_data, deliminator);
      runProfile[row_num].group_num = ns_id_str_to_num(token);
      token = strtok(NULL, deliminator);
      runProfile[row_num].userprof_id = ns_id_str_to_num(token);
      token = strtok(NULL, deliminator);
      runProfile[row_num].sessprof_id = ns_id_str_to_num(token);
      token = strtok(NULL, deliminator);
      runProfile[row_num].pct = atoi(token);
#endif
      char csv_buf[LINE_LENGTH];

      NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "entry_data = %s, test_id=%d", entry_data, test_id);

      write_group_table_in_csv(entry_data, csv_buf, deliminator, test_id);

      NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "end of write_group_table_in_csv");

    } else if (strncmp(entry_name, "USERPROFILE", strlen("USERPROFILE")) == 0) {
      /* There may be userprofiles that are the same, but with diff. pct.  If that is the case, we have to add them together, not make two diff. entries */
      token = strtok(entry_data, deliminator);
      userprof_id = ns_id_str_to_num(token);
      token = strtok(NULL, deliminator);
      strcpy(userprof_name, token);
      token = strtok(NULL, deliminator);
      strcpy(userprof_type, token);
      token = strtok(NULL, deliminator);
      value_id = ns_id_str_to_num(token);
      token = strtok(NULL, deliminator);
      strcpy(value, token);
      token = strtok(NULL, deliminator);
      pct = atoi(token);


      if ((idx = find_userprof_entry(userprof_id, userprof_type, value_id)) == -1) {
	      if (create_user_profile_entry(&row_num) == FAILURE)
	        NS_EXIT(-1, "create_user_profile_entry(): Error allocating more memory for userprof entries");
      
        NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Addding new user profile row (%d) in memory. userprof_id = %d, userprof_name = %s, userprof_type = %s, value_id = %d, value = %s, pct = %d", row_num, userprof_id, userprof_name, userprof_type, value_id, value, pct);

	userProfile[row_num].userprof_id = userprof_id;
	strcpy(userProfile[row_num].userprof_name, userprof_name);
	strcpy(userProfile[row_num].userprof_type, userprof_type);
	userProfile[row_num].value_id = value_id;
	strcpy(userProfile[row_num].value, value);
	userProfile[row_num].pct = pct;
/*
        //This is to keep the start index for all 
        if(strcmp(userprof_type, "LOCATION") == 0) {
          if(locattr_start == -1) locattr_start = row_num;
        } else if(strcmp(userprof_type, "ACCESS") == 0) {
          if(accattr_start == -1) accattr_start = row_num;
        } else if(strcmp(userprof_type, "BROWSER") == 0) {
          if(browattr_start == -1) browattr_start = row_num;
        } else if(strcmp(userprof_type, "FREQUENCY") == 0) {
          if(freqattr_start == -1) freqattr_start = row_num;
        } else if(strcmp(userprof_type, "MACHINE") == 0) {
          if(machattr_start == -1) machattr_start = row_num;
        }
*/
      } 
      else
      {
	userProfile[idx].pct += pct;
        NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Updating pct of user profile in row (%d) in memory. userprof_id = %d, userprof_name = %s, userprof_type = %s, value_id = %d, value = %s, add_pct = %d, new_pct = %d", idx, userprof_id, userprof_name, userprof_type, value_id, value, pct, userProfile[idx].pct);
      }

      
    } else if (strncmp(entry_name, "SESSIONPROFILE", strlen("SESSIONPROFILE")) == 0) {
      if (create_session_profile_entry(&row_num) == FAILURE)
	      NS_EXIT(-1, "create_session_profile_entry(): Error allocating more memory for sessprof entries");
      token = strtok(entry_data, deliminator);
      sessProfile[row_num].sessprof_id = ns_id_str_to_num(token);
      token = strtok(NULL, deliminator);
      sessProfile[row_num].sess_id = ns_id_str_to_num(token);
      token = strtok(NULL, deliminator);
      strcpy(sessProfile[row_num].sessprof_name, token);
      token = strtok(NULL, deliminator);
      sessProfile[row_num].pct = atoi(token);
      NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "sessProfile[row_num].sessprof_id = %d, sessProfile[row_num].sess_id = %d, sessProfile[row_num].sessprof_name = %s, sessProfile[row_num].pct = %d",sessProfile[row_num].sessprof_id, sessProfile[row_num].sess_id, sessProfile[row_num].sessprof_name, sessProfile[row_num].pct);

#if 0
    } else if (strncmp(entry_name, "SESSIONTABLE", strlen("SESSIONTABLE")) == 0) { 

      char csv_buf[LINE_LENGTH];
 
      NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "entry_data = %s, test_id=%d", entry_data, test_id);
 
      write_session_table_in_csv(entry_data, csv_buf, deliminator, test_id);

      NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "end of write_session_table_in_csv");
    } else if (strncmp(entry_name, "PAGETABLE", strlen("PAGETABLE")) == 0) {

      char csv_buf[LINE_LENGTH];
 
      NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "entry_data = %s, test_id=%d", entry_data, test_id);
 
      write_page_table_in_csv(entry_data, csv_buf, deliminator, test_id);

      NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "end of write_page_table_in_csv");
#endif
    } else if (strncmp(entry_name, "TXTABLEV2", strlen("TXTABLEV2")) == 0) {   

      char csv_buf[LINE_LENGTH];
 
     NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "entry_data = %s, test_id=%d", entry_data, test_id);
 
      write_tx_table_in_csv_v2(entry_data, csv_buf, deliminator, test_id);

      NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO,"end of write_tx_table_in_csv");
    } else if (strncmp(entry_name, "TXTABLE", strlen("TXTABLE")) == 0) {   

      char csv_buf[LINE_LENGTH];
 
     NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "entry_data = %s, test_id=%d", entry_data, test_id);
 
      write_tx_table_in_csv(entry_data, csv_buf, deliminator, test_id);

      NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO,"end of write_tx_table_in_csv");
    } else if (strncmp(entry_name, "URLTABLE", strlen("URLTABLE")) == 0) {  
  
      //size calculation
     // 7 intis + 8 commas + url name + 1 null + page and url combine + 1 null + 1 null 
      char csv_buf [28 + 8 + MAX_BUFFER_LEN + 1+ MAX_BUFFER_LEN + MAX_BUFFER_LEN + 1 + 1];
 
      NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "entry_data = %s, test_id=%d", entry_data, test_id);
      
      write_url_table_in_csv(entry_data, csv_buf, deliminator, test_id);

      NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "end of write_url_table_in_csv");

    } else if (strncmp(entry_name, "RECSVRTABLE", strlen("RECSVRTABLE")) == 0) {
      if (create_recsvr_table_entry(&row_num) == FAILURE)
	      NS_EXIT(-1, "create_recsvr_table_entry(): Error allocating more memory for recsvr table entries");
      token = strtok(entry_data, deliminator);
      recSvrTable[row_num].server_group = ns_id_str_to_num(token);
      token = strtok(NULL, deliminator);
      recSvrTable[row_num].rec_svr_id = ns_id_str_to_num(token);
      token = strtok(NULL, deliminator);
      strcpy(recSvrTable[row_num].rec_svr_name, token);
      token = strtok(NULL, deliminator);
      recSvrTable[row_num].rec_svr_port = atoi(token);
      token = strtok(NULL, deliminator);
      strcpy(recSvrTable[row_num].select_agenda, token);
      token = strtok(NULL, deliminator);
      strcpy(recSvrTable[row_num].server_type, token);
    } else if (strncmp(entry_name, "ACTSVRTABLE", strlen("ACTSVRTABLE")) == 0) {
      if (create_actsvr_table_entry(&row_num) == FAILURE)
	      NS_EXIT(-1, "create_actsvr_table_entry(): Error allocating more memory for actsvr table entries");
      //Resolved Bug 43407 - nsu_logging_reader is not running while CM test is running
      char *act_fields[5] = {0};
      char *act_svr_fields[10] = {0};
      int code_num_tokens = get_tokens_ex2(entry_data, act_fields, "ACTSVRTABLE:",  5);
      NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "ACTSVRTABLE: code_num_tokens = %d, act_fields[0] = %s, act_fields[1] = %s", code_num_tokens, act_fields[0], act_fields[1]);
      code_num_tokens = get_tokens(act_fields[0], act_svr_fields, ",",  10);
      NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "ACTSVRTABLE: code_num_tokens = %d", code_num_tokens);

      if(code_num_tokens == 5)
      {
        actSvrTable[row_num].server_group = ns_id_str_to_num(act_svr_fields[0]);
        actSvrTable[row_num].group_idx = ns_id_str_to_num(act_svr_fields[1]);
        strcpy(actSvrTable[row_num].act_svr_name, act_svr_fields[2]);
        actSvrTable[row_num].act_svr_port = atoi(act_svr_fields[3]);
        strcpy(actSvrTable[row_num].location, act_svr_fields[4]);
      }
      else if(code_num_tokens == 4)
      {
        actSvrTable[row_num].server_group = ns_id_str_to_num(act_svr_fields[0]);
        strcpy(actSvrTable[row_num].act_svr_name, act_svr_fields[1]);
        actSvrTable[row_num].act_svr_port = atoi(act_svr_fields[2]);
        strcpy(actSvrTable[row_num].location, act_svr_fields[3]);
      }
    } else if (strncmp(entry_name, "PHASETABLE", strlen("PHASETABLE")) == 0) {
      if (create_phase_table_entry(&row_num) == FAILURE)
	      NS_EXIT(-1, "create_phase_table_entry(): Error allocating more memory for phase table entries");
      token = strtok(entry_data, deliminator);
      phaseTable[row_num].phase_id = atoi(token);
      token = strtok(NULL, deliminator);
      strcpy(phaseTable[row_num].group_name, token);
      token = strtok(NULL, deliminator);
      phaseTable[row_num].phase_type = atoi(token);
      token = strtok(NULL, deliminator);
      strcpy(phaseTable[row_num].phase_name, token);
      /*printf("\n***PhaseTable Dump: phase_id=%hd, group_name=%s, pahse_type=%hd, phase_name=%s\n", 
                   phaseTable[row_num].phase_id, phaseTable[row_num].group_name, 
                   phaseTable[row_num].phase_type, phaseTable[row_num].phase_name);*/
    } else if (strncmp(entry_name, "TESTCASE", strlen("TESTCASE")) == 0) {
      token = strtok(entry_data, deliminator);
      //test_id = atoi(token);
      NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Test id = %d", test_id);
      token = strtok(NULL, deliminator);
      strcpy(testCaseLog.test_name, token);
      token = strtok(NULL, deliminator);
      strcpy(testCaseLog.test_type, token);
      token = strtok(NULL, deliminator);
      testCaseLog.wan_env = atoi(token);
      token = strtok(NULL, deliminator);
      testCaseLog.conn_rpm_target = atoi(token);
      token = strtok(NULL, deliminator);
      testCaseLog.num_proc = atoi(token);
      token = strtok(NULL, deliminator);
      testCaseLog.ramp_up_rate = atoi(token);
      token = strtok(NULL, deliminator);
      testCaseLog.prog_msec = atoi(token);
      token = strtok(NULL, deliminator);
      testCaseLog.run_length = atoi(token);
      token = strtok(NULL, deliminator);
      testCaseLog.idle_sec = atoi(token);
      token = strtok(NULL, deliminator);
      testCaseLog.ssl_pct = atoi(token);
      token = strtok(NULL, deliminator);
      testCaseLog.ka_pct = atoi(token);
      token = strtok(NULL, deliminator);
      testCaseLog.num_ka = atoi(token);
      token = strtok(NULL, deliminator);
      testCaseLog.mean_think_ms = atoi(token);
      token = strtok(NULL, deliminator);
      testCaseLog.median_think_ms = atoi(token);
      token = strtok(NULL, deliminator);
      testCaseLog.var_think_ms = atoi(token);
      token = strtok(NULL, deliminator);
      testCaseLog.think_mode = atoi(token);
      token = strtok(NULL, deliminator);
      testCaseLog.reuse_mode = atoi(token);
      token = strtok(NULL, deliminator);
      testCaseLog.user_rate_mode = atoi(token);
      token = strtok(NULL, deliminator);
      testCaseLog.ramp_up_mode = atoi(token);
      token = strtok(NULL, deliminator);
      testCaseLog.user_cleanup_ms = atoi(token);
      token = strtok(NULL, deliminator);
      testCaseLog.max_conn_per_user = atoi(token);
      token = strtok(NULL, deliminator);
      strcpy(testCaseLog.sess_recording_file, token);
      token = strtok(NULL, deliminator);
      testCaseLog.health_mon = atoi(token);
      token = strtok(NULL, deliminator);
      testCaseLog.guess = atoi(token);
      token = strtok(NULL, deliminator);
      testCaseLog.guess_conf = atoi(token);
      token = strtok(NULL, deliminator);
      testCaseLog.stab_num_success = atoi(token);
      token = strtok(NULL, deliminator);
      testCaseLog.stab_max_run = atoi(token);
      token = strtok(NULL, deliminator);
      testCaseLog.stab_run_time = atoi(token);
      token = strtok(NULL, deliminator);
      strcpy(testCaseLog.sla_metric_entries, token);
      token = strtok(NULL, deliminator);
      testCaseLog.start_time = atoi(token);
      token = strtok(NULL, deliminator);
      testCaseLog.end_time = atoi(token);
    } else if (strncmp(entry_name, "GENERATORTABLE", strlen("GENERATORTABLE")) == 0) {

      char csv_buf[LINE_LENGTH];

      NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "entry_data = %s, test_id=%d", entry_data, test_id);

      write_generator_table_in_csv(entry_data, csv_buf, deliminator, test_id);

      NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "end of write_generator_table_in_csv");
    } else if (strncmp(entry_name, "HOSTTABLE", strlen("HOSTTABLE")) == 0) {
      char csv_buf[LINE_LENGTH];

      NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "entry_data = %s, test_id=%d", entry_data, test_id);

      write_host_table_in_csv(entry_data, csv_buf, deliminator, test_id);

      NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "end of write_host_table_in_csv");
    }
  }
}

void flush_all_fds ()
{
  fflush(rpr_out_fptr);
  fflush(upr_out_fptr);
  fflush(spr_out_fptr);
  fflush(ptb_out_fptr);
  fflush(tptb_out_fptr);
  fflush(ttb_out_fptr);
  fflush(utb_out_fptr);
  //fclose(utb_out_fptr);
  fflush(host_out_fptr);
  fflush(hat_out_fptr);
  fflush(lpt_out_fptr);
  //fflush(tc_out_fptr);
  fflush(sesstb_out_fptr);
  fflush(host_out_fptr);


  //nsu_db_upload_tmp_table_pid not 0 means process already running
  if(nsu_db_upload_tmp_table_pid == 0)
    start_db_upload_tmp_table_process(test_id, getpid(), partition_name);
}

//Save non object (non url,page,tx,session) metadata from buffer to csv file
void output_static_file() {
  int i;
  NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called");
#if 0
  for (i = 0; i < total_run_profile_entries; i++) {
    // Changed by Neeraj on July 12, 2011 as we are storing session table ptr (not session profile prtr)
    if (inline_output)
      fprintf(rpr_out_fptr, "%d,%d,%s,%s,%hd,%s\n", test_id, runProfile[i].group_num, 
	      translate_userprof(runProfile[i].userprof_id), translate_sess(runProfile[i].sessprof_id), runProfile[i].pct, runProfile[i].group_name);
    else {
      fprintf(rpr_out_fptr, "%d,%d,%d,%d,%hd,%s\n", test_id, runProfile[i].group_num, 
	      runProfile[i].userprof_id, runProfile[i].sessprof_id, runProfile[i].pct, runProfile[i].group_name);
    }
  }
#endif
  // CSV File: upf.csv
  // DB Table Name: UserProfile_<TR>
  // test_id,userprof_id,userprof_name,attr_type,pct,attr_id
  // Example:
  // 2086,0,Internet,LOCATION,0,25,SanFrancisco
  // 2086,0,Internet,ACCESS,0,25,56K
  // 2086,0,Internet,BROWSER,0,5,InternetExplorer9.0
  //
  for (i = 0; i < total_user_profile_entries; i++) {
    if (!strncmp(userProfile[i].userprof_type, "LOCATION", strlen("LOCATION")))
      fprintf(upr_out_fptr, "%d,%d,%s,%s,%d,%hd,%s\n", test_id, userProfile[i].userprof_id, userProfile[i].userprof_name,
	      userProfile[i].userprof_type, userProfile[i].value_id, userProfile[i].pct, userProfile[i].value);
    else if (!strncmp(userProfile[i].userprof_type, "ACCESS", strlen("ACCESS")))
      fprintf(upr_out_fptr, "%d,%d,%s,%s,%d,%hd,%s\n", test_id, userProfile[i].userprof_id, userProfile[i].userprof_name,
	      userProfile[i].userprof_type, userProfile[i].value_id, userProfile[i].pct, userProfile[i].value);
    else if (!strncmp(userProfile[i].userprof_type, "BROWSER", strlen("BROWSER")))
      fprintf(upr_out_fptr, "%d,%d,%s,%s,%d,%hd,%s\n", test_id, userProfile[i].userprof_id, userProfile[i].userprof_name,
	      userProfile[i].userprof_type, userProfile[i].value_id, userProfile[i].pct, userProfile[i].value);
    else if (!strncmp(userProfile[i].userprof_type, "FREQUENCY", strlen("FREQUENCY")))
      fprintf(upr_out_fptr, "%d,%d,%s,%s,%d,%hd,%s\n", test_id, userProfile[i].userprof_id, userProfile[i].userprof_name,
	      userProfile[i].userprof_type, userProfile[i].value_id, userProfile[i].pct, userProfile[i].value);
    else if (!strncmp(userProfile[i].userprof_type, "MACHINE", strlen("MACHINE")))
      fprintf(upr_out_fptr, "%d,%d,%s,%s,%d,%hd,%s\n", test_id, userProfile[i].userprof_id, userProfile[i].userprof_name,
	      userProfile[i].userprof_type, userProfile[i].value_id, userProfile[i].pct, userProfile[i].value);
  }

  // CSV File: spf.csv
  // DB Table Name: SessionProfile_<TR>
  // test_id,sessprof_id,sess_id,pct,sessprof_name
  // Example:
  // 2086,0,0,100,_DefaultSessProf
  for (i = 0; i < total_sess_profile_entries; i++) {
    if (inline_output)
      fprintf(spr_out_fptr, "%d,%d,%s,%hd,%s\n", test_id, sessProfile[i].sessprof_id, translate_sess(sessProfile[i].sess_id),
	      sessProfile[i].pct, sessProfile[i].sessprof_name);
    else
      fprintf(spr_out_fptr, "%d,%d,%d,%hd,%s\n", test_id, sessProfile[i].sessprof_id, sessProfile[i].sess_id,
	      sessProfile[i].pct, sessProfile[i].sessprof_name);
  }

  for (i = 0; i < total_actsvr_table_entries; i++) {
    fprintf(hat_out_fptr, "%d,%d,%s,%hd,%s,%d\n", test_id, actSvrTable[i].server_group,
	    actSvrTable[i].act_svr_name, actSvrTable[i].act_svr_port, actSvrTable[i].location, actSvrTable[i].group_idx);
  }

  for (i = 0; i < total_recsvr_table_entries; i++) {
    fprintf(hrt_out_fptr, "%d,%d,%d,%s,%hd,%s,%s\n", test_id, recSvrTable[i].server_group,
	    get_recsvr_index(recSvrTable[i].rec_svr_id), recSvrTable[i].rec_svr_name, recSvrTable[i].rec_svr_port, recSvrTable[i].select_agenda,
	    recSvrTable[i].server_type);
  }
  
  /* Nikita Pandey
   * CSV File     : log_phase_table.csv
   * DB Table Name: PhaseTable_<TR>
   * Format       : phase_id,group_name,phase_type,phase_name
   * Example      : 0,G1,1,ramp_up
   */
  for (i = 0; i < total_phase_table_entries; i++) {
    fprintf(lpt_out_fptr, "%hd,%s,%hd,%s\n", phaseTable[i].phase_id,
	    phaseTable[i].group_name, phaseTable[i].phase_type, phaseTable[i].phase_name);
  }

#if 0  
  if (inline_output)
    fprintf(tc_out_fptr, "%d,%s,%s,%hd,%d,%hd,%d,%d,%d,%d,%hd,%hd,%hd,%d,%d,%d,", 
	    test_id, testCaseLog.test_name, testCaseLog.test_type, 
	    testCaseLog.wan_env, testCaseLog.conn_rpm_target, testCaseLog.num_proc, testCaseLog.ramp_up_rate,
	    testCaseLog.prog_msec, testCaseLog.run_length, testCaseLog.idle_sec, testCaseLog.ssl_pct,
	    testCaseLog.ka_pct, testCaseLog.num_ka, testCaseLog.mean_think_ms, testCaseLog.median_think_ms,
	    testCaseLog.var_think_ms);
  else
    fprintf(tc_out_fptr, "%d,%s,%s,%hd,%d,%hd,%d,%d,%d,%d,%hd,%hd,%hd,%d,%d,%d,", 
	    test_id, testCaseLog.test_name, testCaseLog.test_type, 
	    testCaseLog.wan_env, testCaseLog.conn_rpm_target, testCaseLog.num_proc, testCaseLog.ramp_up_rate,
	    testCaseLog.prog_msec, testCaseLog.run_length, testCaseLog.idle_sec, testCaseLog.ssl_pct,
	    testCaseLog.ka_pct, testCaseLog.num_ka, testCaseLog.mean_think_ms, testCaseLog.median_think_ms,
	    testCaseLog.var_think_ms);
  
  switch (testCaseLog.think_mode) {
  case 0:
    fprintf(tc_out_fptr, "%s,", "NO_THINK_TIME");
    break;
  case 1:
    fprintf(tc_out_fptr, "%s,", "INTERNET_RANDOM_THINK_TIME");
    break;
  case 2:
    fprintf(tc_out_fptr, "%s,", "CONSTANT_THINK_TIME");
    break;
  case 3:
    fprintf(tc_out_fptr, "%s,", "UNIFORM_RANDOM_THINK_TIME");
    break;
  default:
    fprintf(tc_out_fptr, "%s,", "Unknown Think Mode");
    break;
  }

  switch (testCaseLog.reuse_mode) {
  case 0:
    fprintf(tc_out_fptr, "%s,", "THINK TIME AFTER SESSION");
    break;
  case 1:
    fprintf(tc_out_fptr, "%s,", "NO THINK TIME AFTER SESSION");   
    break;
  case 2:
    fprintf(tc_out_fptr, "%s,", "NEW USER AFTER SESSION");
    break;
  default:
    fprintf(tc_out_fptr, "%s,", "Unknown Reuse mode");
    break;
  }

  switch (testCaseLog.user_rate_mode) {
  case 0:
    fprintf(tc_out_fptr, "%s,", "CONSTANT USER CREATION IID");
    break;
  case 1:
    fprintf(tc_out_fptr, "%s,", "RANDOM USER CREATION IID");   
    break;
  default:
    fprintf(tc_out_fptr, "%s,", "Unknown User Rate Mode");   
    break;
  }

  switch (testCaseLog.ramp_up_mode) {
  case 0:
    fprintf(tc_out_fptr, "%s,", "STEP RAMPING UP");
    break;
  case 1:
    fprintf(tc_out_fptr, "%s,", "LINEAR RAMPING UP");   
    break;
  case 2:
    fprintf(tc_out_fptr, "%s,", "POISSON RAMPING UP");
    break;
  default:
    fprintf(tc_out_fptr, "%s,", "Unknown Ramp up Mode");
    break;
  }
  
  fprintf(tc_out_fptr, "%d,%hd,%s,%hd,%d,", testCaseLog.user_cleanup_ms, testCaseLog.max_conn_per_user,
	  testCaseLog.sess_recording_file, testCaseLog.health_mon, testCaseLog.guess);

  switch (testCaseLog.guess_conf) {
  case 'L':
    fprintf(tc_out_fptr, "%s,", "L");
    break;
  case 'M':
    fprintf(tc_out_fptr, "%s,", "M");
    break;
  case 'H':
    fprintf(tc_out_fptr, "%s,", "H");
    break;
  default:
    fprintf(tc_out_fptr, "%s,", "-");
    break;
  }

  fprintf(tc_out_fptr, "%hd,%hd,%hd,%s,%d,%d\n", testCaseLog.stab_num_success, testCaseLog.stab_max_run,
	  testCaseLog.stab_run_time, testCaseLog.sla_metric_entries, testCaseLog.start_time, testCaseLog.end_time);
  #endif
  flush_all_fds();
}

void translate_int(char** read_cursor, char** out_fptr, int end_record, int test_id) {
  int var;
  unsigned int amt_wrtn = 0;

  memcpy(&var, *read_cursor, UNSIGNED_INT_SIZE);
  NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "var = %d, read_cursor = %s", var, *read_cursor); 
  if (test_id)
  amt_wrtn = sprintf(*out_fptr, "%10.10d%c", var, end_record?'\n':deliminator[0]);
  else
  amt_wrtn = sprintf(*out_fptr, "%d%c", var, end_record?'\n':deliminator[0]);
   
  *out_fptr += amt_wrtn;
  *read_cursor += UNSIGNED_INT_SIZE;
}

//This is common func for all the tables 'T/P/U/S' to update data on runtime
static void update_object_table()
{
  int size = 0;
  struct stat s;
  char slog_file[MAX_FILENAME_LENGTH];
  char buf[LINE_LENGTH + 1] = {0};
  char *tmp_ptr;
  int buf_len = 0; 
  
  //In case of non partition slog is in TR/reports/raw_data
  //In case of partition slog file is in TR/common_files/reports/raw_data (slog.start_partition)
  //  and each partition has a hard link to slog file
  sprintf(slog_file, "logs/TR%d/%s/reports/raw_data/slog", test_id, partition_name);

  NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "slog_file = %s", slog_file);

  if(!stat(slog_file, &s))
    size = s.st_size;

  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "slog_offset = %d, Current file size = %d, current file offset using ftell = %d, s_in_fptr = %p", slog_offset, size, ftell(s_in_fptr), s_in_fptr);

  // We have changed this code due to below reason
  /*
     Bug 108620: This bug is specific to U20 where fgets after reading the static data returns EOF.
     So, when any new records(for ex - dynamic transaction) comes in slog logging reader while doing fgets return NULL(EOF).
     And due to which we are not able to process the dynamic data.
     To overcome this, we have used clearerr function which will clears the end-of file and error indicators for the given stream.
     We have also checked if any interrupt other than EINTR is captured while executing fgets we should continue to read the data.
  */
  if(size > slog_offset)
  {
    clearerr(s_in_fptr);
    while(1)
    {
      char *ptr = fgets(buf, LINE_LENGTH, s_in_fptr);
      if(!ptr)
      {
        NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "ERROR::: slog_offset = %d, Current file size = %d, current file offset using ftell = %d, s_in_fptr = %p feof = %d, err = %s", slog_offset, size, ftell(s_in_fptr), s_in_fptr, feof(s_in_fptr), nslb_strerror(errno));
        if(feof(s_in_fptr))
          break;
        if(errno != EINTR)
          break;

        continue;
      }

      NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Inside WHILE::: slog_offset = %d, Current file size = %d, current file offset using ftell = %d, s_in_fptr = %p", slog_offset, size, ftell(s_in_fptr), s_in_fptr);
      NSLB_TRACE_LOG4(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Read form file = %s", buf);
      buf_len = strlen(buf);
      slog_offset += buf_len;

      strcat(slog_partial_buffer, buf);
      buf_len = strlen(slog_partial_buffer);
      //if(buf_len > 0 && buf[buf_len - 1] != '\n')  //TODO Krishna discuss
      if(buf_len > 0 && slog_partial_buffer[buf_len - 1] != '\n')  //TODO Krishna discuss
      {
        //fprintf(stderr, "Partial read from slog, buf = \'%s\'\n", buf);
        fprintf(stderr, "Partial read from slog, buf = \'%s\'\n", slog_partial_buffer);
        continue;
      } 
      
    slog_partial_buffer[buf_len - 1] = '\0';  //removing newline char
    //size calculation
    // 7 intis + 8 commas + url name + 1 null + page and url combine + 1 null + 1 null 
    char csv_buf [28 + 8 + MAX_BUFFER_LEN + 1+ MAX_BUFFER_LEN + MAX_BUFFER_LEN + 1 + 1];
    //int ret;      //Fixed build warning, ret is not used.
    tmp_ptr = strchr(slog_partial_buffer, ':');
    tmp_ptr++;

    if(strncmp(slog_partial_buffer, "URLTABLE", URLTABLE_LEN) == 0)
      write_url_table_in_csv(tmp_ptr, csv_buf, ",", test_id);
    else if(strncmp(slog_partial_buffer, "SESSIONTABLE", SESSIONTABLE_LEN) == 0)
      write_session_table_in_csv(tmp_ptr, csv_buf, ",", test_id);
    else if(strncmp(slog_partial_buffer, "PAGETABLE", PAGETABLE_LEN) == 0)
      write_page_table_in_csv(tmp_ptr, csv_buf, ",", test_id);
    else if(strncmp(slog_partial_buffer, "TXTABLEV2", TXTABLEV2_LEN) == 0)
      write_tx_table_in_csv_v2(tmp_ptr, csv_buf, ",", test_id);
    else if(strncmp(slog_partial_buffer, "TXTABLE", TXTABLE_LEN) == 0)
      write_tx_table_in_csv(tmp_ptr, csv_buf, ",", test_id);
    else if(strncmp(slog_partial_buffer, "GENERATORTABLE", GENERATORTABLE_LEN) == 0)
      write_generator_table_in_csv(tmp_ptr, csv_buf, ",", test_id);
    else if(strncmp(slog_partial_buffer, "RUNPROFILE", RUNPROFILE_LEN) == 0)
      write_group_table_in_csv(tmp_ptr, csv_buf, ",", test_id);
    else if(strncmp(slog_partial_buffer, "HOSTTABLE", HOSTTABLE_LEN) == 0)
      write_host_table_in_csv(tmp_ptr, csv_buf, ",", test_id);
    else
      //fprintf(stderr, "Error: Invalid object type in line '%s'.\n", buf);
      NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Error: Invalid object type in line '%s'", buf);

    slog_partial_buffer[0] = '\0';
   }
 }
}

//This Method is to add dummy url in case if url index not found for a dynamic url after updating from slog
static inline void add_dummy_data_in_table(int type)
{
  char buf[LINE_LENGTH];
  int  buf_len;
  //int ret;       Fixed build Warning, ret is never used

  NSLB_TRACE_LOG4(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method Called");

  if(type == URL_TYPE)
  {
    //Narendra: adding a dummy URL in url table.(urt.csv). Because in Kohls (Bugid: m339), we was getting a dynamic url record,
    //which had no record in slog. Because of it was failed to get normalized index of that.
    //So now in that case we will set url index to -1, and that will be mapped to this dummy URL

    //tmp_url_Table.url_hash_id(field4), tmp_url_Table.url_hash_code(field5) are unsigned but in db these are int
    buf_len = sprintf(buf, "%d,%d,%d,%d,%d,%d,%s,%d,%s", test_id, -1, -1, -1, -1, 11/*strlen(INVALID_URL)*/, "INVALID_URL", 24/*strlen(INVALID_PAGE:INVALID_URL)*/, "INVALID_PAGE:INVALID_URL");
    NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Dummy Url Table entry = %s, length = %d", buf, buf_len);
  
    fprintf(utb_out_fptr, "%s\n", buf);
    //Doing flush here because there is posibblity that fprintf buffered the data bcoz of this drill down will not get data in urt.csv file 
    fflush(utb_out_fptr);
  }
#if 0
  else if(type == SESSION_TYPE)
  {
    buf_len = sprintf(buf, "%d,%d,%s", test_id, -1, "INVALID_SESSION");
    NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Dummy Session Table entry = %s, length = %d", buf, buf_len);
  
    fprintf(sesstb_out_fptr, "%s\n", buf);
    //Doing flush here because there is posibblity that fprintf buffered the data bcoz of this drill down will not get data in urt.csv file 
    fflush(sesstb_out_fptr);
  }
  else if(type == PAGE_TYPE)
  {
    buf_len = sprintf(buf, "%d,%d,%d,%s", test_id, -1, -1, "INVALID_PAGE");
    NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Dummy Page Table entry = %s, length = %d", buf, buf_len);
  
    fprintf(ptb_out_fptr, "%s\n", buf);
    //Doing flush here because there is posibblity that fprintf buffered the data bcoz of this drill down will not get data in urt.csv file 
    fflush(ptb_out_fptr);
  }
#endif
  else if(type == TX_TYPE)
  {
    buf_len = sprintf(buf, "%d,%d,%s", test_id, -1, "INVALID_TX");
    NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Dummy Tx Table entry = %s, length = %d", buf, buf_len);
  
    fprintf(ttb_out_fptr, "%s\n", buf);
    //Doing flush here because there is posibblity that fprintf buffered the data bcoz of this drill down will not get data in urt.csv file 
    fflush(ttb_out_fptr);
  }
  else if(type == GEN_TYPE)
  {
    buf_len = sprintf(buf, "%s,%d,%s,%s,%s,%d", "INVALID_GENERATOR_NAME", -1, "INVALID_WDIR", "INVALID_HOST_NAME", "INVALID_RESOLVED_IP", -1);
    NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Dummy Generator Table entry = %s, length = %d", buf, buf_len);
  
    fprintf(generator_out_fptr, "%s\n", buf);
    //Doing flush here because there is posibblity that fprintf buffered the data bcoz of this drill down will not get data in generator_table.csv file 
    fflush(generator_out_fptr);
  }
  else if(type == GROUP_TYPE)
  {
    buf_len = sprintf(buf, "%d,%d,%d,%d,%d,%s", test_id, -1, -1, -1, -1, "INVALID_GROUP");
    NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Dummy Tx Table entry = %s, length = %d", buf, buf_len);
  
    fprintf(rpr_out_fptr, "%s\n", buf);
    //Doing flush here because there is posibblity that fprintf buffered the data bcoz of this drill down will not get data in rpf.csv file 
    fflush(rpr_out_fptr);
  }
  else if(type == HOST_TYPE)
  {
    buf_len = sprintf(buf, "%s,%d", "INVALID_HOST_NAME", -1);
    NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Dummy HOST Table entry = %s, length = %d", buf, buf_len);
    
    fprintf(host_out_fptr, "%s\n", buf);
    //Doing flush here because there is posibblity that fprintf buffered the data bcoz of this drill down will not get data in HostTable.csv file 
    fflush(host_out_fptr);
  }
}

static int translate_index_norm(char** read_cursor, char** out_fptr, int end_record, char*(*translate_func)(unsigned int), int gen_id) {
  unsigned int var;
  unsigned int index;
  unsigned int amt_wrtn = 0;

  NSLB_TRACE_LOG4(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method Called");

  memcpy(&var, *read_cursor, UNSIGNED_INT_SIZE);
  index = var;
  NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "index = %u", index);

  GET_URL_NORM_ID(index, URL_TYPE, gen_id); 

  if (translate_func)
    amt_wrtn = sprintf(*out_fptr, "%s%c", translate_func(var), end_record?'\n':deliminator[0]);
  else
    amt_wrtn = sprintf(*out_fptr, "%u%c", obj_norm_id, end_record?'\n':deliminator[0]);

  NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "amt_wrtn = %u", amt_wrtn);
  *out_fptr += amt_wrtn;
  *read_cursor += UNSIGNED_INT_SIZE;

  NSLB_TRACE_LOG4(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method End");
  return 0;
}

void translate_index(char** read_cursor, char** out_fptr, int end_record, char*(*translate_func)(unsigned int), int obj_type) {
  unsigned int var;
  unsigned int index;
  unsigned int amt_wrtn = 0;
  //This should be int as we are saving -1 value in this. Also it can hold 2 billion values so no need for unsigend int
  int retIdx; 

  memcpy(&var, *read_cursor, UNSIGNED_INT_SIZE);
  index = var;
  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "index = %d obj_type = %d", index, obj_type);
#if 0
  if(obj_type == SESSION_TYPE) //session
  {
    GET_SESSION_NORM_ID(index, SESSION_TYPE); 
    retIdx = obj_norm_id;
  }
  else if(obj_type == PAGE_TYPE) //page
  {
    GET_PAGE_NORM_ID(index, PAGE_TYPE); 
    retIdx = obj_norm_id;
  }
#endif
  if(obj_type == GEN_TYPE) //Generator
  {
    if(index == -1)
      retIdx = index;
    else{
      GET_GENERATOR_NORM_ID(index, GEN_TYPE);
      retIdx = obj_norm_id;
    }
  }
  else if(obj_type == GROUP_TYPE) //Group
  {
    GET_GROUP_NORM_ID(index, GROUP_TYPE);
    retIdx = obj_norm_id;
  }
  else
    retIdx = index;

  if (translate_func)
    amt_wrtn = sprintf(*out_fptr, "%s%c", translate_func(var), end_record?'\n':deliminator[0]);
  else
    amt_wrtn = sprintf(*out_fptr, "%d%c", retIdx, end_record?'\n':deliminator[0]);

  *out_fptr += amt_wrtn;
  *read_cursor += UNSIGNED_INT_SIZE;
}

void translate_index_2(char** read_cursor, char** out_fptr, int end_record, char*(*translate_func)(unsigned int)) {

  unsigned long var;
  int index;
  unsigned int amt_wrtn = 0;

  //memcpy(&var, *read_cursor, INDEX_SIZE);
  memcpy(&var, *read_cursor, UNSIGNED_INT_SIZE);
  index = var;

  if (translate_func)
    amt_wrtn = sprintf(*out_fptr, "%s%c", translate_func(var), end_record?'\n':deliminator[0]);
  else
    amt_wrtn = sprintf(*out_fptr, "%u%c", index, end_record?'\n':deliminator[0]);

  // *read_cursor += INDEX_SIZE;
  *out_fptr += amt_wrtn;
  *read_cursor += UNSIGNED_INT_SIZE;
}

void translate_time_data_size_into_secs(char** read_cursor, char** out_fptr, int end_record) {
  unsigned long long var;
  unsigned int amt_wrtn = 0;

  memcpy(&var, *read_cursor, NS_TIME_DATA_SIZE);
  //NS will now put the timestamp in msecs from Build 4.1.1 21
  //var = var/1000;
  amt_wrtn = sprintf(*out_fptr, "%llu%c", var, end_record?'\n':deliminator[0]);

  *out_fptr += amt_wrtn;
  *read_cursor += NS_TIME_DATA_SIZE;
}

void translate_time_data_size(char** read_cursor, char** out_fptr, int end_record) {
  unsigned long long var;
  unsigned int amt_wrtn = 0;

  memcpy(&var, *read_cursor, NS_TIME_DATA_SIZE);
  amt_wrtn = sprintf(*out_fptr, "%llu%c", var, end_record?'\n':deliminator[0]);

  *out_fptr += amt_wrtn;
  *read_cursor += NS_TIME_DATA_SIZE;
}


static inline void insert_rep_time_into_records(unsigned int resp_time, char** out_fptr, int end_record) {
  unsigned int amt_wrtn = 0;

   amt_wrtn = sprintf(*out_fptr, "%u%c", resp_time, end_record?'\n':deliminator[0]);
   *out_fptr += amt_wrtn;
}

static inline void insert_gen_id_into_records(char** out_fptr, int end_record) {
  unsigned int amt_wrtn = 0;
   
   amt_wrtn = sprintf(*out_fptr, "%d%c", (total_generator_entries)?generator_idx:-1, end_record?'\n':deliminator[0]);
   *out_fptr += amt_wrtn;
}

void insert_ns_phase_into_records(char** read_cursor, char** out_fptr, int end_record) {
  unsigned int amt_wrtn = 0;

   amt_wrtn = sprintf(*out_fptr, "%s%c", *read_cursor, end_record?'\n':deliminator[0]);
   *out_fptr += amt_wrtn;
}

void translate_short(char** read_cursor, char** out_fptr, int end_record) {
  unsigned short var;
  unsigned int amt_wrtn = 0;

  memcpy(&var, *read_cursor, SHORT_SIZE);
  amt_wrtn = sprintf(*out_fptr, "%hu%c", var, end_record?'\n':deliminator[0]);

  *out_fptr +=  amt_wrtn;
  *read_cursor += SHORT_SIZE;
}

static void read_variable_len_value(char** read_cursor, char** out_fptr, short size) 
{
  memcpy(*out_fptr, *read_cursor, size);
  *out_fptr += size;
  *read_cursor += size;
}

void translate_unsigned_char(char** read_cursor, char** out_fptr, int end_record) {
  unsigned char var;
  unsigned int amt_wrtn = 0;

  memcpy(&var, *read_cursor, UNSIGNED_CHAR_SIZE);
  amt_wrtn = sprintf(*out_fptr, "%d%c", (int)var, end_record?'\n':deliminator[0]);

  *out_fptr += amt_wrtn;
  *read_cursor += UNSIGNED_CHAR_SIZE;
}

void translate_unsigned_long(char** read_cursor, char** out_fptr, int end_record) {
  ns_8B_t var;
  unsigned int amt_wrtn = 0;

  memcpy(&var, *read_cursor, UNSIGNED_LONG_SIZE);
  amt_wrtn = sprintf(*out_fptr, "%lld%c", var, end_record?'\n':deliminator[0]);

  *out_fptr += amt_wrtn;
  *read_cursor += UNSIGNED_LONG_SIZE;
}

void translate_tx_index_v2(char** read_cursor, char** out_fptr, int end_record, int nvm_id, int gen_id) {
  int var;
  unsigned int amt_wrtn = 0;
     
  memcpy(&var, *read_cursor, UNSIGNED_INT_SIZE);
  if (var == -1)
  amt_wrtn =  sprintf(*out_fptr, "%c", end_record?'\n':deliminator[0]);
  else
  {
    GET_TX_NORM_ID_V2(nvm_id, var, TX_TYPE, gen_id); 
    amt_wrtn =  sprintf(*out_fptr, "%d%c", obj_norm_id, end_record?'\n':deliminator[0]);
  }

  *out_fptr += amt_wrtn;
  *read_cursor += UNSIGNED_INT_SIZE;
}

void translate_tx_index(char** read_cursor, char** out_fptr, int end_record) {
  int var;
  unsigned int amt_wrtn = 0;
     
  memcpy(&var, *read_cursor, UNSIGNED_INT_SIZE);
  if (var == -1)
  amt_wrtn =  sprintf(*out_fptr, "%c", end_record?'\n':deliminator[0]);
  else
  {
    GET_TX_NORM_ID(var, TX_TYPE); 
    amt_wrtn =  sprintf(*out_fptr, "%d%c", obj_norm_id, end_record?'\n':deliminator[0]);
  }

  *out_fptr += amt_wrtn;
  *read_cursor += UNSIGNED_INT_SIZE;
}

unsigned int get_unsigned_int(char** read_cursor) {
  unsigned int return_val;

  memcpy(&return_val, *read_cursor, sizeof(unsigned int));
  *read_cursor += UNSIGNED_INT_SIZE;
  return return_val;
}

unsigned long long get_unsigned_long_long(char** read_cursor) {
  unsigned long long return_val;

  memcpy(&return_val, *read_cursor, sizeof(unsigned long long));
  *read_cursor += NS_TIME_DATA_SIZE;
  return return_val;
}
#if 0
unsigned long get_unsigned_long(char** read_cursor) { // Not Used
  unsigned long return_val; // Not Used

  memcpy(&return_val, *read_cursor, sizeof(unsigned int));
  *read_cursor += UNSIGNED_LONG_SIZE;
  return return_val;
}
#endif

unsigned char get_unsigned_char(char** read_cursor) {
  unsigned char return_val;

  memcpy(&return_val, *read_cursor, sizeof(unsigned char));
  *read_cursor += UNSIGNED_CHAR_SIZE;
  return return_val;
}

unsigned short get_unsigned_short(char** read_cursor) {
  unsigned short return_val;

  memcpy(&return_val, *read_cursor, sizeof(unsigned short));
  *read_cursor += SHORT_SIZE;
  return return_val;
}

unsigned short get_child_idx(char** read_cursor) {
  unsigned short return_val;

  memcpy(&return_val, *read_cursor, sizeof(unsigned short));
  //Need to preserve unique child index to fill in log file
  child_idx_with_gen_id = return_val;
  //Return nvm id from child_idx (generator_id + nvm_id)
  return_val = return_val & 0x00FF; 
  *read_cursor += SHORT_SIZE;
  return return_val;
}

unsigned int get_index(char** read_cursor) {
  unsigned int addr;

  memcpy(&addr, *read_cursor, sizeof(unsigned int));
  *read_cursor += UNSIGNED_INT_SIZE;
  return addr;
}

void write_data_entry(struct data_log_id* data_info, struct data_file_entry* data_entry, FILE* out_fptr) {
  struct data* iterate_ptr = data_entry->head_data;
  struct data* old_data_slot;
  unsigned int amount_to_write;

  fprintf(out_fptr, "--- child_id: %hu, sess_inst: %u, url: %d\n", data_info->child_idx, data_info->sess_inst, data_info->url_idx);

  while (data_entry->data_length) {
    amount_to_write = (data_entry->data_length > DATA_BUFFER_SIZE)?DATA_BUFFER_SIZE:data_entry->data_length;
    if(fwrite(iterate_ptr->data, sizeof(char), amount_to_write, out_fptr) < 1)
      //TL
    data_entry->data_length -= amount_to_write;
    old_data_slot = iterate_ptr;
    iterate_ptr = iterate_ptr->next;
    free(old_data_slot);
  }

  fprintf(out_fptr, "\n-----\n\n");

  assert (iterate_ptr == NULL);
}

void write_msg_entry(struct pg_dump_data_entry* data_entry, FILE* out_fptr, unsigned short child_idx) {
  struct pg_dump_data* iterate_ptr = data_entry->head_data;
  struct pg_dump_data* old_data_slot;
  unsigned int amount_to_write;

  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called, START_LOG: %s: NVMID=%d; ", get_time(data_entry->time), (int)child_idx);
  //fprintf(out_fptr, "%lu: ", msg_info->time);
  //offset should be saved before writing message in log, offset should be written in binary using write
  read_or_write_dlog_offset(WRITE_DLOG_OFFSET);
  fprintf(out_fptr, "START_LOG: %s: NVMID=%hd; ", get_time(data_entry->time), child_idx);

  while (data_entry->data_length) {
    amount_to_write = (data_entry->data_length > DATA_BUFFER_SIZE)?DATA_BUFFER_SIZE:data_entry->data_length;
    NSLB_TRACE_LOG4(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "data_entry->data_length %d iterate_ptr->data %s",data_entry->data_length, iterate_ptr->data);
    //TODO do trace logging if fwrite < 1
    fwrite(iterate_ptr->data, sizeof(char), amount_to_write, out_fptr);
    data_entry->data_length -= amount_to_write;
    old_data_slot = iterate_ptr;
    iterate_ptr = iterate_ptr->next;
    NSLB_TRACE_LOG4(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "FREE'ed old_data_slot done. Freeing ptr = $%p$ ", old_data_slot); 
    free(old_data_slot);
    NSLB_TRACE_LOG4(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "data_entry->data_length %d",data_entry->data_length);
  }

  fprintf(out_fptr, "\nEND_LOG\n");
  fflush(out_fptr);

  assert (iterate_ptr == NULL);
}

#define RESET_PG_DATA_ENTRTY_PTR \
/*pg_data_entry_ptr gets free in write_msg_entry funct, next head ptr needs to be NULL*/ \
pg_data_entry_ptr->head_data = NULL; \
pg_data_entry_ptr->cur_data = NULL; \
pg_data_entry_ptr->msg_num = -1; \
pg_data_entry_ptr->total_size = -1; 

/* Function is used to free message entry in case of discarding data message
 * */
static void free_msg_entry(struct pg_dump_data_entry* pg_data_entry_ptr)
{
  struct pg_dump_data* head_pg_data_entry_ptr = pg_data_entry_ptr->head_data;
  struct pg_dump_data* tmp_pg_data_entry_ptr;
   
  NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called, pg_data_entry_ptr = %p", pg_data_entry_ptr);
  while(head_pg_data_entry_ptr)
  {
    tmp_pg_data_entry_ptr = head_pg_data_entry_ptr;
    NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Free head data = %p", tmp_pg_data_entry_ptr);
    head_pg_data_entry_ptr = head_pg_data_entry_ptr->next;
    free(tmp_pg_data_entry_ptr);
  }
  RESET_PG_DATA_ENTRTY_PTR
}   

/* Function used to fill msg_entry in linklist
 * Here we need to verify whether msg num exist or 
 * not as per link list. In errornous case we exit from the funct.
 * Common code for both MSG_RECORD and MSG_LAST_RECORD*/

#define ADD_MSG_ENTRY(record_num, read_cursor, data_length, child_idx, pg_data_entry_ptr, offset, free_slot_bytes, copy_amount, pg_new_data_slot) \
  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "reached in %s", (record_num == MSG_RECORD)?"MSG_RECORD":"MSG_LAST_RECORD");\
  int msg_number; \
  int total_size = get_unsigned_int(&read_cursor);\
  data_length = get_unsigned_int(&read_cursor);\
  child_idx = get_child_idx(&read_cursor);\
  msg_number = get_unsigned_int(&read_cursor);\
  pg_data_entry[child_idx].time = get_unsigned_long_long(&read_cursor);\
  NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "msg_number = %d, pg_data_entry[child_idx].msg_num = %d, pg_data_entry[child_idx].total_size = %d", msg_number, pg_data_entry[child_idx].msg_num, pg_data_entry[child_idx].total_size);\
  /*Fill total message size*/\
  if (pg_data_entry[child_idx].total_size == -1) {\
    NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Total message size = %d", total_size);\
    pg_data_entry[child_idx].total_size = total_size;\
  }\
  /*Fill message number*/\
  if (pg_data_entry[child_idx].msg_num == -1) {\
    NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "New msg entry");\
    pg_data_entry[child_idx].msg_num = msg_number;\
  } else if (pg_data_entry[child_idx].msg_num != msg_number){\
    /* This will happen when LAST_MSG_RECORD is missing */ \
    NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_ERROR, "logging_reader: Error occurred while matching msg number %d, with existing message number %d for child %d at timestamp %lld ms (%s). Page dump of this is discarded", msg_number, pg_data_entry[child_idx].msg_num, (int)child_idx, pg_data_entry[child_idx].time, get_time(pg_data_entry[child_idx].time)); \
    free_msg_entry(&pg_data_entry[child_idx]); \
    /*Set page data entires for new entry*/\
    pg_data_entry[child_idx].total_size = total_size;\
    pg_data_entry[child_idx].msg_num = msg_number;\
    NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Reset pg_data_entry table values for child idx = %d, msg_num = %d, total size = %d", child_idx, pg_data_entry[child_idx].msg_num, pg_data_entry[child_idx].total_size); \
  }\
  NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "head dat = %p, data_length = %d, child_idx = %hu, msg_num = %d, time = %d", pg_data_entry[child_idx].head_data, data_length, child_idx, pg_data_entry[child_idx].msg_num, pg_data_entry[child_idx].time); \
  /* Add new entry in data_entry struct*/    \
  /* Subtract current size from total size*/ \
   pg_data_entry[child_idx].total_size -= data_length;\
  if (!(pg_data_entry[child_idx].head_data)) {\
    pg_data_entry_ptr = &pg_data_entry[child_idx];\
    pg_data_entry_ptr->data_length = 0;\
    pg_data_entry_ptr->cur_data = NULL;\
    /*new_entry = 1; Not required */\
    NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Not found in %s", (record_num == MSG_RECORD)?"MSG_RECORD":"MSG_LAST_RECORD");\
  } else if (pg_data_entry[child_idx].cur_data){\
    NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Found in %s", (record_num == MSG_RECORD)?"MSG_RECORD":"MSG_LAST_RECORD");\
    pg_data_entry_ptr = &pg_data_entry[child_idx];\
    NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "pg_data_entry_ptr->data_length = %d", pg_data_entry_ptr->data_length);\
    /*new_entry = 0; Not required*/ \
  }    \
  while (data_length) {\
    offset = pg_data_entry_ptr->data_length % DATA_BUFFER_SIZE; \
    if (!offset) { /* need new buffer */ \
      NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "offset = %d, pg_data_entry_ptr->data_length = %d", offset, pg_data_entry_ptr->data_length); \
      if (!pg_data_entry_ptr->head_data) { \
	pg_data_entry_ptr->head_data = pg_data_entry_ptr->cur_data = (struct pg_dump_data*) malloc(sizeof(struct pg_dump_data));\
        NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "IF part, head_data=%p, pg_data_entry_ptr = %p", pg_data_entry_ptr->head_data, pg_data_entry_ptr); \
	pg_data_entry_ptr->head_data->next = NULL; \
      } else { \
	pg_new_data_slot = (struct pg_dump_data*) malloc(sizeof(struct pg_dump_data)); \
        NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "MALLOC'ed pg_new_data_slot done. ptr = $%p$, size = %d", pg_new_data_slot, (int)(sizeof(struct pg_dump_data))); \
        pg_new_data_slot->next = NULL; \
	pg_data_entry_ptr->cur_data->next = pg_new_data_slot; \
	pg_data_entry_ptr->cur_data = pg_new_data_slot; \
      } \
    } \
    free_slot_bytes = DATA_BUFFER_SIZE - offset; \
    copy_amount = (data_length > free_slot_bytes)?free_slot_bytes:data_length; \
    NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "offset = %d, copy_amount = %d", offset, copy_amount); \
    memcpy(pg_data_entry_ptr->cur_data->data + offset, read_cursor, copy_amount); \
    pg_data_entry_ptr->data_length += copy_amount; \
    data_length -= copy_amount; \
    read_cursor += copy_amount;\
    NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "pg_data_entry_ptr->data_length = %d, data_length = %d,copy_amount = %d", pg_data_entry_ptr->data_length, data_length, copy_amount); \
  }\


//This function will initilized the buffers.
static void inline init_rec_bufs()
{
  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called");

  urc_write_buf_ptr = urc_write_buffer;
  src_write_buf_ptr = src_write_buffer;
  trc_pg_write_buf_ptr = trc_pg_write_buffer;
  trc_write_buf_ptr = trc_write_buffer;
  prc_write_buf_ptr = prc_write_buffer;

}

//This will called first time when we are going to write 
static void inline init_pg_dump_data() 
{
  /* At init time memset pg_data_entry struct*/
  //if (init_pg_dump_entry == 0) {
  int i;
  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "memset struct, size of struct = %d", sizeof(pg_data_entry));
    memset(pg_data_entry, 0, sizeof(pg_data_entry));
  for(i = 0; i < 255; i++){
    pg_data_entry[i].msg_num = -1; //Making default
    pg_data_entry[i].total_size = -1; //Making default
  }
    //init_pg_dump_entry++;
}

void copy_short_in_buff(char** out_fptr, short var, int end_record) {
  unsigned int amt_wrtn = 0;

  amt_wrtn = sprintf(*out_fptr, "%hu%c", var, end_record?'\n':deliminator[0]);
  *out_fptr +=  amt_wrtn;
}

/* this method will be called from translate_dyn_file to process complete block */
/*Complete Buffer should be passed(BLOCK_SIZE - 8K), read_amt should be BLOCK_SIZE*/
inline void process_lr_buffer(char *read_buffer, int read_amt, int total_read_amt)
{
  char* read_cursor;
  char record_num;
  //int blank_space;  Commenting as it provides warning
  int url_records = 0;
  unsigned char bitmask;
  unsigned int data_length;
  struct data_log_id key_data;
  struct data_file_entry data_entry;
  struct data_file_entry* data_entry_ptr;
  //struct msg_log_id key_msg;
  DBT key, data;
  unsigned int offset;
  struct data* new_data_slot;
  unsigned int free_slot_bytes, copy_amount;
  unsigned int amt_wrtn = 0;
  int new_entry;
  static int session_records = 0;
/*Begin: PAGEDUMP*/
  unsigned short child_idx = 0;
  struct pg_dump_data_entry* pg_data_entry_ptr = NULL;
  struct pg_dump_data* pg_new_data_slot = NULL;
  short size;
  int size_int;
  unsigned short nvm_id;
  
 // static int init_pg_dump_entry = 0;
/*End: PAGEDUMP*/
  u_ns_ts_t record_start_time, record_end_time;
  
  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "data length %d DATA FROM logs/TRXXXX/reports/raw_data/dlog file[%s] ",read_amt, read_buffer);
  read_cursor = read_buffer;

	while ((read_cursor - read_buffer) != read_amt) {
		memcpy(&record_num, read_cursor, UNSIGNED_CHAR_SIZE);
		read_cursor += UNSIGNED_CHAR_SIZE;
		
		//fprintf(stderr, "Record number = %d", record_num);
			//NSLB_TRACE_LOG1(lr_trace_log_key, 1, NULL, ERROR,  "Value of record num first time is %s",record_num);
		switch (record_num) {
		case SESSION_RECORD:

			// 2147,0,0,0,0,0,0,Mozilla1.0,Mumbai,13,1,0,6976,14000,217,0
			session_records++;
			NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Reached in SESSION_RECORD, session_records = %d", session_records);
                        amt_wrtn = sprintf(src_write_buf_ptr, "%d%c", test_id, deliminator[0]);
			src_write_buf_ptr += amt_wrtn;

			// Session Record Session Index
                        translate_int(&read_cursor, &src_write_buf_ptr, 0, 0);
                        //translate_index(&read_cursor, &src_write_buf_ptr, 0, inline_output?translate_sess:NULL, SESSION_TYPE);

			// Session Record Session Instance
                        translate_int(&read_cursor, &src_write_buf_ptr, 0, 0);

			//Session Record  User Index
                        translate_int(&read_cursor, &src_write_buf_ptr, 0, 0);

			// RunProfileIndex (group_num)
                        translate_int(&read_cursor, &src_write_buf_ptr, 0, 0);

			// Session Record Child id
                        memcpy(&child_idx, read_cursor, SHORT_SIZE);
                        generator_idx = (int)((child_idx & 0xFF00) >> 8);
                        translate_short(&read_cursor, &src_write_buf_ptr, 0);

			// Session Record Is Run Phase
                        translate_unsigned_char(&read_cursor, &src_write_buf_ptr, 0);

                        // Session Record Access
                        // translate_index(&read_cursor, &src_write_buf_ptr, 0, inline_output?translate_access:NULL, get_accattr_index);
                        translate_index(&read_cursor, &src_write_buf_ptr, 0, translate_access, NO_TYPE);

			// Session Record Location
                        // translate_index(&read_cursor, &src_write_buf_ptr, 0, inline_output?translate_location:NULL, get_locattr_index);
                        translate_index(&read_cursor, &src_write_buf_ptr, 0, translate_location, NO_TYPE);

			// Session Record Browser
                        //translate_index_2(&read_cursor, &src_write_buf_ptr, 0, inline_output?translate_browser:NULL, get_browattr_index);
			translate_index(&read_cursor, &src_write_buf_ptr, 0, translate_browser, NO_TYPE);

			// Session Record Freq
                        translate_index_2(&read_cursor, &src_write_buf_ptr, 0, inline_output?translate_freq:NULL);

			// Session Record Machine Attribute
                        translate_index_2(&read_cursor, &src_write_buf_ptr, 0, inline_output?translate_machine:NULL);


			// Session Record Started at
                        memcpy(&record_start_time, read_cursor, NS_TIME_DATA_SIZE);
                        translate_time_data_size_into_secs(&read_cursor, &src_write_buf_ptr, 0);

			// Session Record Ended at
                        memcpy(&record_end_time, read_cursor, NS_TIME_DATA_SIZE);
                        translate_time_data_size_into_secs(&read_cursor, &src_write_buf_ptr, 0);
			
			// Session Record Think Duration
                        translate_time_data_size(&read_cursor, &src_write_buf_ptr, 0);

			// Session Record Session Status
                        translate_unsigned_char(&read_cursor, &src_write_buf_ptr, 0);

			//Session response time
                        insert_rep_time_into_records((record_end_time - record_start_time), &src_write_buf_ptr, 0);

			//NSPhases currently we are passing value of phase's forcefully -1
                        //insert_ns_phase_into_records(&read_cursor, &src_write_buf_ptr, 1);
                        translate_short(&read_cursor, &src_write_buf_ptr, 0);
			//Insert generator index 
			insert_gen_id_into_records(&src_write_buf_ptr, 1);

                        if ((read_cursor - read_buffer) > read_amt)
	                  printf("shouldn't happen\n");
                        break;

		case TX_PG_RECORD_V2:
			NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "reached in TX_PG_RECORD_V2");

			// TestRunNum
			amt_wrtn = sprintf(trc_pg_write_buf_ptr, "%d%c", test_id, deliminator[0]);
			NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Test id = %d", test_id);
			trc_pg_write_buf_ptr += amt_wrtn;
		 
                        //Child idx
                        memcpy(&child_idx, read_cursor, SHORT_SIZE);
                        read_cursor += SHORT_SIZE;
                        generator_idx = (int)((child_idx & 0xFF00) >> 8);
                        nvm_idx = (int)(child_idx & 0x00FF);

			// Tx Record  hash code or tx index
                        translate_tx_index_v2(&read_cursor, &trc_pg_write_buf_ptr, 0, nvm_idx, generator_idx);
			
			//  Tx Record Session Index
                        translate_int(&read_cursor, &trc_pg_write_buf_ptr, 0, 0);
                        //translate_index(&read_cursor, &trc_pg_write_buf_ptr, 0, inline_output?translate_sess:NULL, SESSION_TYPE);

			//  Tx Record Session Instance
                        translate_int(&read_cursor, &trc_pg_write_buf_ptr, 0, 0);

			//  Tx Record Child Id
                        copy_short_in_buff(&trc_pg_write_buf_ptr, child_idx, 0);

			//  Tx Record Tx Instance
                        translate_short(&read_cursor, &trc_pg_write_buf_ptr, 0);
			
			//Tx Record Page Instance
			translate_short(&read_cursor, &trc_pg_write_buf_ptr, 0);

			//unsigned int start_time, end_time;
			memcpy(&record_start_time, read_cursor, NS_TIME_DATA_SIZE);
                        translate_time_data_size_into_secs(&read_cursor, &trc_pg_write_buf_ptr, 0);

			//  Tx Record Tx end at
                        memcpy(&record_end_time, read_cursor, NS_TIME_DATA_SIZE);
                        translate_time_data_size_into_secs(&read_cursor, &trc_pg_write_buf_ptr, 0);
	 
			//  Tx Record Tx Status
                        translate_unsigned_char(&read_cursor, &trc_pg_write_buf_ptr, 0);

                        //Transaction response time 
			insert_rep_time_into_records((record_end_time - record_start_time), &trc_pg_write_buf_ptr, 0);

                        //Trnasaction phase id
                        translate_short(&read_cursor, &trc_pg_write_buf_ptr, 0);
			//Insert generator index 
			insert_gen_id_into_records(&trc_pg_write_buf_ptr, 1);

			if ((read_cursor - read_buffer) > read_amt)
				printf("shouldn't happen\n");
			break;

		case TX_PG_RECORD:
			// dlog format:
			// RecordType, TransactionIndex, SessionIndex, SessionInstance, ChildIndex, TxInstance, PageInstance, StartTime, EndTime, Status, PhaseIndex, 
			// CSV Format:
			// TestRunNum, TransactionIndex, SessionIndex, SessionInstance, ChildIndex, TxInstance, PageInstance, StartTime, EndTime, Status, RespTime, PhaseIndex, 
			NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "reached in TX_PG_RECORD");

			// TestRunNum
			amt_wrtn = sprintf(trc_pg_write_buf_ptr, "%d%c", test_id, deliminator[0]);
			NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Test id = %d", test_id);
			trc_pg_write_buf_ptr += amt_wrtn;
		 
			// Tx Record  hash code or tx index
                        translate_tx_index(&read_cursor, &trc_pg_write_buf_ptr, 0);
			
			//  Tx Record Session Index
                        translate_int(&read_cursor, &trc_pg_write_buf_ptr, 0, 0);
                        //translate_index(&read_cursor, &trc_pg_write_buf_ptr, 0, inline_output?translate_sess:NULL, SESSION_TYPE);

			//  Tx Record Session Instance
                        translate_int(&read_cursor, &trc_pg_write_buf_ptr, 0, 0);

                        
                        memcpy(&child_idx, read_cursor, SHORT_SIZE);
                        generator_idx = (int)((child_idx & 0xFF00) >> 8);
			//  Tx Record Child Id
                        translate_short(&read_cursor, &trc_pg_write_buf_ptr, 0);

			//  Tx Record Tx Instance
                        translate_short(&read_cursor, &trc_pg_write_buf_ptr, 0);
			
			//Tx Record Page Instance
			translate_short(&read_cursor, &trc_pg_write_buf_ptr, 0);

			//unsigned int start_time, end_time;
			memcpy(&record_start_time, read_cursor, NS_TIME_DATA_SIZE);
                        translate_time_data_size_into_secs(&read_cursor, &trc_pg_write_buf_ptr, 0);

			//  Tx Record Tx end at
                        memcpy(&record_end_time, read_cursor, NS_TIME_DATA_SIZE);
                        translate_time_data_size_into_secs(&read_cursor, &trc_pg_write_buf_ptr, 0);
	 
			//  Tx Record Tx Status
                        translate_unsigned_char(&read_cursor, &trc_pg_write_buf_ptr, 0);

                        //Transaction response time 
			insert_rep_time_into_records((record_end_time - record_start_time), &trc_pg_write_buf_ptr, 0);

                        //Trnasaction phase id
                        translate_short(&read_cursor, &trc_pg_write_buf_ptr, 0);
			//Insert generator index 
			insert_gen_id_into_records(&trc_pg_write_buf_ptr, 1);

			if ((read_cursor - read_buffer) > read_amt)
				printf("shouldn't happen\n");
			break;


		case TX_RECORD_V2:
  	 	        NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "reached in TX_RECORD");
                        amt_wrtn = sprintf(trc_write_buf_ptr, "%d%c", test_id, deliminator[0]);
			NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Test id = %d", test_id);
			trc_write_buf_ptr += amt_wrtn;

                        //Child idx
                        memcpy(&child_idx, read_cursor, SHORT_SIZE);
                        read_cursor += SHORT_SIZE;
                        generator_idx = (int)((child_idx & 0xFF00) >> 8);
                        nvm_idx = (int)(child_idx & 0x00FF);

			// Tx Record  hash code or tx index
                        translate_tx_index_v2(&read_cursor, &trc_write_buf_ptr, 0, nvm_idx, generator_idx);
			
			//  Tx Record Session Index
                        translate_int(&read_cursor, &trc_write_buf_ptr, 0, 0);

			//  Tx Record Session Instance
                        translate_int(&read_cursor, &trc_write_buf_ptr, 0, 0);

			//  Tx Record Child Id
                        copy_short_in_buff(&trc_write_buf_ptr, child_idx, 0);

			//  Tx Record Tx Instance
                        translate_short(&read_cursor, &trc_write_buf_ptr, 0);

			//  Tx Record Tx start at
			memcpy(&record_start_time, read_cursor, NS_TIME_DATA_SIZE);
                        translate_time_data_size_into_secs(&read_cursor, &trc_write_buf_ptr, 0);

			//  Tx Record Tx end at
                        memcpy(&record_end_time, read_cursor, NS_TIME_DATA_SIZE);
                        translate_time_data_size_into_secs(&read_cursor, &trc_write_buf_ptr, 0);

			//  Tx Record Tx Think Time
                        translate_time_data_size(&read_cursor, &trc_write_buf_ptr, 0);

			//  Tx Record Tx Status
                        translate_unsigned_char(&read_cursor, &trc_write_buf_ptr, 0);

                        //Transaction response time 
			insert_rep_time_into_records((record_end_time - record_start_time), &trc_write_buf_ptr, 0);

                        //Trnasaction phase id
                        translate_short(&read_cursor, &trc_write_buf_ptr, 0);
			//Insert generator index 
			insert_gen_id_into_records(&trc_write_buf_ptr, 1);

                        if ((read_cursor - read_buffer) > read_amt)
	                  printf("shouldn't happen\n");
                    break;

		case TX_RECORD:
  	 	        NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "reached in TX_RECORD");
                        amt_wrtn = sprintf(trc_write_buf_ptr, "%d%c", test_id, deliminator[0]);
			NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Test id = %d", test_id);
			trc_write_buf_ptr += amt_wrtn;

			// Tx Record  hash code or tx index
                        //translate_index(&read_cursor, &trc_write_buf_ptr, 0, inline_output?translate_tx:NULL, get_tx_index);
                        translate_tx_index(&read_cursor, &trc_write_buf_ptr, 0);
			
			//  Tx Record Session Index
                        translate_int(&read_cursor, &trc_write_buf_ptr, 0, 0);
                        //translate_index(&read_cursor, &trc_write_buf_ptr, 0, inline_output?translate_sess:NULL, SESSION_TYPE);

			//  Tx Record Session Instance
                        translate_int(&read_cursor, &trc_write_buf_ptr, 0, 0);

                        memcpy(&child_idx, read_cursor, SHORT_SIZE);
                        generator_idx = (int)((child_idx & 0xFF00) >> 8);
			//  Tx Record Child Id
                        translate_short(&read_cursor, &trc_write_buf_ptr, 0);

			//  Tx Record Tx Instance
                        translate_short(&read_cursor, &trc_write_buf_ptr, 0);

			//  Tx Record Tx start at
			memcpy(&record_start_time, read_cursor, NS_TIME_DATA_SIZE);
                        translate_time_data_size_into_secs(&read_cursor, &trc_write_buf_ptr, 0);

			//  Tx Record Tx end at
                        memcpy(&record_end_time, read_cursor, NS_TIME_DATA_SIZE);
                        translate_time_data_size_into_secs(&read_cursor, &trc_write_buf_ptr, 0);

	 
			//  Tx Record Tx Think Time
                        translate_time_data_size(&read_cursor, &trc_write_buf_ptr, 0);

			//  Tx Record Tx Status
                        translate_unsigned_char(&read_cursor, &trc_write_buf_ptr, 0);

                        //Transaction response time 
			insert_rep_time_into_records((record_end_time - record_start_time), &trc_write_buf_ptr, 0);

                        //Trnasaction phase id
                        translate_short(&read_cursor, &trc_write_buf_ptr, 0);
			//Insert generator index 
			insert_gen_id_into_records(&trc_write_buf_ptr, 1);

                        if ((read_cursor - read_buffer) > read_amt)
	                  printf("shouldn't happen\n");
                    break;

		case PAGE_RECORD_V2:
			NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "reached in PAGE_RECORD");
                        amt_wrtn = sprintf(prc_write_buf_ptr, "%d%c", test_id, deliminator[0]);
			NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Test id = %d", test_id);
			prc_write_buf_ptr += amt_wrtn;

                        //Child idx
                        memcpy(&child_idx, read_cursor, SHORT_SIZE);
                        read_cursor += SHORT_SIZE;
                        generator_idx = (int)((child_idx & 0xFF00) >> 8);
                        nvm_idx = (int)(child_idx & 0x00FF);

			// Page Record Current Page
                        translate_int(&read_cursor, &prc_write_buf_ptr, 0, 0);
                        //translate_index(&read_cursor, &prc_write_buf_ptr, 0, inline_output?translate_pg:NULL, PAGE_TYPE);

			// Page Record Session Index
                        translate_int(&read_cursor, &prc_write_buf_ptr, 0, 0);
                        //translate_index(&read_cursor, &prc_write_buf_ptr, 0, inline_output?translate_sess:NULL, SESSION_TYPE);

			// Page Record Session Instance 
                        translate_int(&read_cursor, &prc_write_buf_ptr, 0, 0);

			// Page Record Current Tx
                        //translate_index(&read_cursor, &prc_write_buf_ptr, 0, inline_output?translate_tx:NULL, get_tx_index);
                        translate_tx_index_v2(&read_cursor, &prc_write_buf_ptr, 0, nvm_idx, generator_idx);

                        // Page Record Child Id
                        copy_short_in_buff(&prc_write_buf_ptr, child_idx, 0);

			// Page Record Tx Instance
                        translate_short(&read_cursor, &prc_write_buf_ptr, 0);

			// Page Record Page Instance
                        translate_short(&read_cursor, &prc_write_buf_ptr, 0);


			// Page Record Page Begin at
                        memcpy(&record_start_time, read_cursor, NS_TIME_DATA_SIZE);
                        translate_time_data_size_into_secs(&read_cursor, &prc_write_buf_ptr, 0);

			// Page Record Page end at
                        memcpy(&record_end_time, read_cursor, NS_TIME_DATA_SIZE);
                        translate_time_data_size_into_secs(&read_cursor, &prc_write_buf_ptr, 0);

			// Page Record Page Status
                        translate_unsigned_char(&read_cursor, &prc_write_buf_ptr, 0);

                        //Page response time
			insert_rep_time_into_records((record_end_time - record_start_time), &prc_write_buf_ptr, 0);
                        //Page phase id
                        translate_short(&read_cursor, &prc_write_buf_ptr, 0);
			//Insert generator index 
			insert_gen_id_into_records(&prc_write_buf_ptr, 1);

                        if ((read_cursor - read_buffer) > read_amt)
	                  printf("shouldn't happen\n");
                       break;

		case PAGE_RECORD:
			NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "reached in PAGE_RECORD");
                        amt_wrtn = sprintf(prc_write_buf_ptr, "%d%c", test_id, deliminator[0]);
			NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Test id = %d", test_id);
			prc_write_buf_ptr += amt_wrtn;

			// Page Record Current Page
                        translate_int(&read_cursor, &prc_write_buf_ptr, 0, 0);
                        //translate_index(&read_cursor, &prc_write_buf_ptr, 0, inline_output?translate_pg:NULL, PAGE_TYPE);

			// Page Record Session Index
                        translate_int(&read_cursor, &prc_write_buf_ptr, 0, 0);
                        //translate_index(&read_cursor, &prc_write_buf_ptr, 0, inline_output?translate_sess:NULL, SESSION_TYPE);

			// Page Record Session Instance 
                        translate_int(&read_cursor, &prc_write_buf_ptr, 0, 0);

			// Page Record Current Tx
                        //translate_index(&read_cursor, &prc_write_buf_ptr, 0, inline_output?translate_tx:NULL, get_tx_index);
                        translate_tx_index(&read_cursor, &prc_write_buf_ptr, 0);

                        memcpy(&child_idx, read_cursor, SHORT_SIZE);
                        generator_idx = (int)((child_idx & 0xFF00) >> 8);
			// Page Record Child Id
                        translate_short(&read_cursor, &prc_write_buf_ptr, 0);

			// Page Record Tx Instance
                        translate_short(&read_cursor, &prc_write_buf_ptr, 0);

			// Page Record Page Instance
                        translate_short(&read_cursor, &prc_write_buf_ptr, 0);


			// Page Record Page Begin at
                        memcpy(&record_start_time, read_cursor, NS_TIME_DATA_SIZE);
                        translate_time_data_size_into_secs(&read_cursor, &prc_write_buf_ptr, 0);

			// Page Record Page end at
                        memcpy(&record_end_time, read_cursor, NS_TIME_DATA_SIZE);
                        translate_time_data_size_into_secs(&read_cursor, &prc_write_buf_ptr, 0);

			// Page Record Page Status
                        translate_unsigned_char(&read_cursor, &prc_write_buf_ptr, 0);

                        //Page response time
			insert_rep_time_into_records((record_end_time - record_start_time), &prc_write_buf_ptr, 0);
                        //Page phase id
                        translate_short(&read_cursor, &prc_write_buf_ptr, 0);
			//Insert generator index 
			insert_gen_id_into_records(&prc_write_buf_ptr, 1);

                        if ((read_cursor - read_buffer) > read_amt)
	                  printf("shouldn't happen\n");
                       break;

		case PAGE_DUMP_RECORD:
                  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "reached in PAGE_DUMP_RECORD");
                  //Add test run number 
                  amt_wrtn = sprintf(pdrc_write_buf_ptr, "%d%c", test_id, deliminator[0]);
                  pdrc_write_buf_ptr += amt_wrtn;

                  // Page dump record Start time
                  memcpy(&record_start_time, read_cursor, NS_TIME_DATA_SIZE);
                  translate_time_data_size_into_secs(&read_cursor, &pdrc_write_buf_ptr, 0);

                  // Page dump record End time
                  memcpy(&record_end_time, read_cursor, NS_TIME_DATA_SIZE);
                  translate_time_data_size_into_secs(&read_cursor, &pdrc_write_buf_ptr, 0);

                  // Page dump record Page duration
                  insert_rep_time_into_records((record_end_time - record_start_time), &pdrc_write_buf_ptr, 0);

                  // Page dump record Generator Id
                  //translate_int(&read_cursor, &pdrc_write_buf_ptr, 0, 0);
                  memcpy(&generator_idx, read_cursor, UNSIGNED_INT_SIZE);
                  //TODO: Need to do normalization for generators. No need to do gen norm on generators. So, need mechanishm to check                              if this logging reader is running on generator or on controller
                  //translate_index(&read_cursor, &pdrc_write_buf_ptr, 0, inline_output?translate_gen:NULL, GEN_TYPE);
                  translate_int(&read_cursor, &pdrc_write_buf_ptr, 0, 0);

                  // Page dump record NVM ID 
                  memcpy(&nvm_id, read_cursor, UNSIGNED_INT_SIZE);
                  translate_short(&read_cursor, &pdrc_write_buf_ptr, 0);

                  // Page dump record User Id
                  translate_int(&read_cursor, &pdrc_write_buf_ptr, 0, 0);

                  // Page dump record Session Instance
                  translate_int(&read_cursor, &pdrc_write_buf_ptr, 0, 0);

                  // Page dump record Page Id
                  translate_int(&read_cursor, &pdrc_write_buf_ptr, 0, 0);
                  //translate_index(&read_cursor, &pdrc_write_buf_ptr, 0, inline_output?translate_sess:NULL, PAGE_TYPE);

                  // Page dump record Page Instance
                  translate_short(&read_cursor, &pdrc_write_buf_ptr, 0);

                  // Page dump record Page Status
                  translate_short(&read_cursor, &pdrc_write_buf_ptr, 0);

                  // Page dump record Page Response Time
                  translate_int(&read_cursor, &pdrc_write_buf_ptr, 0, 0);
          
                  // Page dump record Group Id
                  translate_int(&read_cursor, &pdrc_write_buf_ptr, 0, 0);
                  //translate_index(&read_cursor, &pdrc_write_buf_ptr, 0, inline_output?translate_group:NULL, GROUP_TYPE);

                  // Page dump record Session Id
                  translate_int(&read_cursor, &pdrc_write_buf_ptr, 0, 0);
                  
                  // Page dump record Partition
                  translate_unsigned_long(&read_cursor, &pdrc_write_buf_ptr, 0);

                  // Page dump record Trace Level
                  translate_short(&read_cursor, &pdrc_write_buf_ptr, 0);
   
                  child_idx = (generator_idx>0?(generator_idx << 8):0) + nvm_id;
                  amt_wrtn = sprintf(pdrc_write_buf_ptr, "%hu%c", child_idx, deliminator[0]); 
                  pdrc_write_buf_ptr += amt_wrtn;
                  //amt_wrtn = sprintf(pdrc_write_buf_ptr, ","); 
                  //pdrc_write_buf_ptr += 1;

                  // Insert future field 1
                  translate_int(&read_cursor, &pdrc_write_buf_ptr, 0, 0);

                  // Insert Tx Name
                  size = 0;
                  memcpy(&size, read_cursor, SHORT_SIZE);
                  read_cursor += SHORT_SIZE;
                  if (size > 0)
                    read_variable_len_value(&read_cursor, &pdrc_write_buf_ptr, size);
                  amt_wrtn = sprintf(pdrc_write_buf_ptr, ",");
                  pdrc_write_buf_ptr += 1;

                  // Insert Fetched Parameter
                  size = 0;
                  memcpy(&size, read_cursor, SHORT_SIZE);
                  read_cursor += SHORT_SIZE;
                  if (size > 0)
                    read_variable_len_value(&read_cursor, &pdrc_write_buf_ptr, size);
                  amt_wrtn = sprintf(pdrc_write_buf_ptr, ",");
                  pdrc_write_buf_ptr += 1;

                  size = 0;
                  // Page dump record flow name, 
                  memcpy(&size, read_cursor, UNSIGNED_CHAR_SIZE);
                  read_cursor += UNSIGNED_CHAR_SIZE;
                  // Read flow name into buffer and add delimiter(deliminator[0])
                  read_variable_len_value(&read_cursor, &pdrc_write_buf_ptr, size);
                  amt_wrtn = sprintf(pdrc_write_buf_ptr, ","); 
                  pdrc_write_buf_ptr += 1;

                  //page dump record log file 
                  size = 0;
                  memcpy(&size, read_cursor, SHORT_SIZE);
                  read_cursor += SHORT_SIZE;
                  // Read log name into buffer and add delimiter(deliminator[0])
                  read_variable_len_value(&read_cursor, &pdrc_write_buf_ptr, size);
                  amt_wrtn = sprintf(pdrc_write_buf_ptr, ",");
                  pdrc_write_buf_ptr += 1;
                  
                  //page dump record response body orig name
                  size = 0;
                  memcpy(&size, read_cursor, SHORT_SIZE);
                  read_cursor += SHORT_SIZE;
                  // Read orig name into buffer and add delimiter(deliminator[0])
                  if (size > 0)
                    read_variable_len_value(&read_cursor, &pdrc_write_buf_ptr, size);
                  amt_wrtn = sprintf(pdrc_write_buf_ptr, ",");
                  pdrc_write_buf_ptr += 1;

                  // Page dump record Parameter, 
                  // if size is greater than zero then read parameterize buffer
                  size = 0;
                  memcpy(&size, read_cursor, SHORT_SIZE);
                  read_cursor += SHORT_SIZE;
                  //In parameterized buffer newline is already added hence wont be adding newline here
                  if (size > 0)
                    read_variable_len_value(&read_cursor, &pdrc_write_buf_ptr, size);
                  //else //If parameterization not applied then we need to add newline
                  //{
                    amt_wrtn = sprintf(pdrc_write_buf_ptr, ","); 
                    pdrc_write_buf_ptr += 1;
                  //}
                  
                  //Page dump record CVfail check point. 
                  size = 0;
                  memcpy(&size, read_cursor, SHORT_SIZE);
                  read_cursor += SHORT_SIZE;
                  NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "size = %hi, read_cursor = %s", size, read_cursor);
                  //Read CVfail name into buffer,and add newline.
                  if (size > 0)
                    read_variable_len_value(&read_cursor, &pdrc_write_buf_ptr, size);
                  amt_wrtn = sprintf(pdrc_write_buf_ptr, "\n");
                  pdrc_write_buf_ptr += 1;
                 
                  if ((read_cursor - read_buffer) > read_amt)
                    printf("shouldn't happen\n");
                  break;

		case URL_RECORD:
		  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "reached in URL_RECORD");
                  //amt_wrtn = sprintf(urc_write_buf_ptr, "%d%c", test_id, deliminator[0]);
	          //urc_write_buf_ptr += amt_wrtn;
          
                  //changing size to size_int as logging writer is writing unsigned int size as reader is using unsigned short variable to read                    it.
                  size_int = 0;
                  memcpy(&size_int, read_cursor, UNSIGNED_INT_SIZE);
                  read_cursor += UNSIGNED_INT_SIZE;
                  if(size_int  > 0)
                    read_variable_len_value(&read_cursor, &urc_write_buf_ptr, size_int);
                  amt_wrtn = sprintf(urc_write_buf_ptr, ",");
                  urc_write_buf_ptr += 1;

			// Url Record Url num index
                        /*if(translate_index_norm(&read_cursor, &urc_write_buf_ptr, 0, inline_output?translate_url:NULL, get_url_index) < 0)
			{
				// TODO: How to handle this case
			} */
                  /*In NetCloud we need to normalize URLs wrt generator id*/
                  memcpy(&child_idx, read_cursor, SHORT_SIZE);
                  read_cursor += SHORT_SIZE;
	          generator_idx = (int)((child_idx & 0xFF00) >> 8); 
                  nvm_idx = (int)(child_idx & 0x00FF); 
                  // Page dump record Start time
                  //In release 3.9.7 we need to send generator id for url normalization
		  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "URL_RECORD: child_idx = %hd", child_idx);
                  translate_index_norm(&read_cursor, &urc_write_buf_ptr, 0, inline_output?translate_url:NULL, generator_idx);
		  //translate_index(&read_cursor, &urc_write_buf_ptr, 0, inline_output?translate_url:NULL, get_url_index);

                  // Url Record Session index 
                  translate_int(&read_cursor, &urc_write_buf_ptr, 0, 0);
                  //translate_index(&read_cursor, &urc_write_buf_ptr, 0, inline_output?translate_sess:NULL, SESSION_TYPE);

                  // Url Record Session Instance
                  translate_int(&read_cursor, &urc_write_buf_ptr, 0, 0);

                  // Url Record Current Tx
                  //translate_index(&read_cursor, &urc_write_buf_ptr, 0, inline_output?translate_tx:NULL, get_tx_index);
                  translate_tx_index_v2(&read_cursor, &urc_write_buf_ptr, 0, nvm_idx, generator_idx);

                  // Url Record Current Page
                  translate_int(&read_cursor, &urc_write_buf_ptr, 0, 0);
                  //translate_index(&read_cursor, &urc_write_buf_ptr, 0, inline_output?translate_pg:NULL, PAGE_TYPE);

                  // Url Record Child Id
                  amt_wrtn = sprintf(urc_write_buf_ptr, "%hd%c", child_idx, deliminator[0]);
                  urc_write_buf_ptr += amt_wrtn;

                  // Url Record Tx Instance
                  translate_short(&read_cursor, &urc_write_buf_ptr, 0);

                  // Url Record Page Instance
                  translate_short(&read_cursor, &urc_write_buf_ptr, 0);

                  // Url Record Bit Mask for Main,Emb,Redirect 
                  memcpy(&bitmask, read_cursor, UNSIGNED_CHAR_SIZE);
                  //Fetched from (takes up first 2 bits)
                  amt_wrtn = sprintf(urc_write_buf_ptr, "%hd%c", bitmask & 0x03, deliminator[0]);
		  urc_write_buf_ptr += amt_wrtn;
                  //HTTPS request reuse (takes up 3rd bit)
                  amt_wrtn = sprintf(urc_write_buf_ptr, "%hd%c", (bitmask & 0x04)?1:0, deliminator[0]);
                  urc_write_buf_ptr += amt_wrtn;
                  //HTTPS response reuse (takes up 4th bit)
                  amt_wrtn = sprintf(urc_write_buf_ptr, "%hd%c", (bitmask & 0x08)?1:0, deliminator[0]);
                  urc_write_buf_ptr += amt_wrtn;
                  //URL type (takes up 5th bit)
                  //sprintf(&urc_write_buf_ptr, "%hd%c", (bitmask & 0x10)?1:0, deliminator);
                  //URL type (takes up 5th and 6th bit)
                  amt_wrtn = sprintf(urc_write_buf_ptr, "%hd%c", (bitmask & 0x30) >> 4, deliminator[0]);
                  urc_write_buf_ptr += amt_wrtn;
                  //sprintf(&urc_write_buf_ptr, "%hd%c", (bitmask & 0x10)?1:0, deliminator);
                  read_cursor += UNSIGNED_CHAR_SIZE;

		  // Url Record Url Started at 
                  memcpy(&record_start_time, read_cursor, NS_TIME_DATA_SIZE);
                  translate_time_data_size_into_secs(&read_cursor, &urc_write_buf_ptr, 0);

		  // Url Record DNS Resolution start time 
                  translate_int(&read_cursor, &urc_write_buf_ptr, 0, 0);

		  // Url Record Connect time
                  translate_int(&read_cursor, &urc_write_buf_ptr, 0, 0);

		  // Url Record SSL Handshake done time
                  translate_int(&read_cursor, &urc_write_buf_ptr, 0, 0);

		  // Url Record Write Complete time
                  translate_int(&read_cursor, &urc_write_buf_ptr, 0, 0);

		  // First byte recieve time 
                  translate_int(&read_cursor, &urc_write_buf_ptr, 0, 0);

		  // Url Record Request complete time 
                  translate_int(&read_cursor, &urc_write_buf_ptr, 0, 0);

		  // Url Record Rendering time
                  translate_int(&read_cursor, &urc_write_buf_ptr, 0, 0);

		  // Url Record Started at (End time??)
                  memcpy(&record_end_time, read_cursor, NS_TIME_DATA_SIZE);
                  translate_time_data_size_into_secs(&read_cursor, &urc_write_buf_ptr, 0);

		  // Url Record Response code
                  translate_short(&read_cursor, &urc_write_buf_ptr, 0);

		  // Url Record Http Payload sent
                  translate_int(&read_cursor, &urc_write_buf_ptr, 0, 0);

		  // Url Record Tcp bytes sen 
                  translate_int(&read_cursor, &urc_write_buf_ptr, 0, 0);

		  // Url Record ethernet bytes sent
                  translate_int(&read_cursor, &urc_write_buf_ptr, 0, 0);

		  // Url Record cptr bytes
                  translate_int(&read_cursor, &urc_write_buf_ptr, 0, 0);

		  // Url Record Tcp bytes recieve 
                  translate_int(&read_cursor, &urc_write_buf_ptr, 0, 0);

		  // Url Record ethernet bytes recv
                  translate_int(&read_cursor, &urc_write_buf_ptr, 0, 0);

		  // Url Record Compression mode
                  translate_unsigned_char(&read_cursor, &urc_write_buf_ptr, 0);

		  // Url Record Status
                  translate_unsigned_char(&read_cursor, &urc_write_buf_ptr, 0);

		  // Url Record Num connection (conn_fd)
                  translate_int(&read_cursor, &urc_write_buf_ptr, 0, 0);

		  // Url Record Connection type
                  translate_unsigned_char(&read_cursor, &urc_write_buf_ptr, 0);

		  // Url Record retries
                  translate_unsigned_char(&read_cursor, &urc_write_buf_ptr, 0);

		  // Flow path Instance
                  translate_unsigned_long(&read_cursor, &urc_write_buf_ptr, 0);

		  //URL response time 
		  insert_rep_time_into_records((record_end_time - record_start_time), &urc_write_buf_ptr, 0);
                  //URL phase id
                  translate_short(&read_cursor, &urc_write_buf_ptr, 0);

		  //Insert generator index 
                  insert_gen_id_into_records(&urc_write_buf_ptr, 1); 

                  if ((read_cursor - read_buffer) > read_amt)
	          printf("shouldn't happen\n");
                  url_records++;
                break;

                case RBU_PAGE_DETAIL_RECORD:
                  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "reached in RBU_PAGE_DETAIL_RECORD:");          
 
                  //page rbu detail record partition_id
                  translate_unsigned_long(&read_cursor, &prrc_write_buf_ptr, 0); 

                  //page rbu detail record har_file_name
                  int rbu_size = 0;
                  memcpy(&rbu_size, read_cursor, UNSIGNED_INT_SIZE);
                  read_cursor += UNSIGNED_INT_SIZE;

                  if (rbu_size > 0)
                    read_variable_len_value(&read_cursor, &prrc_write_buf_ptr, rbu_size);
                  amt_wrtn = sprintf(prrc_write_buf_ptr, ",");
                  prrc_write_buf_ptr += 1;

                  //page rbu detail record page_idx
                  translate_int(&read_cursor, &prrc_write_buf_ptr, 0, 0); 
   
                  //page rbu detail record sess_idx
                  translate_int(&read_cursor, &prrc_write_buf_ptr, 0, 0); 
                  //translate_index(&read_cursor, &prrc_write_buf_ptr, 0, inline_output?translate_sess:NULL, SESSION_TYPE);

                  //page rbu detail record nvm_id
                  memcpy(&nvm_id, read_cursor, SHORT_SIZE);
                  translate_short(&read_cursor, &prrc_write_buf_ptr, 0);
 
                  //page rbu detail record gen_id
                  //memcpy(&child_idx, read_cursor, SHORT_SIZE);
                  //read_cursor += SHORT_SIZE;
                  //generator_idx = (int)((child_idx & 0xFF00) >> 8);
                  memcpy(&generator_idx, read_cursor, UNSIGNED_INT_SIZE);
                  if(generator_idx == -1) 
                  {
                    //NS case, no need to do normalization
                    obj_norm_id = -1;
                    generator_idx = 0;
                  }
                  else
                    GET_GENERATOR_NORM_ID(generator_idx, GEN_TYPE);

                  amt_wrtn = sprintf(prrc_write_buf_ptr, "%d,", obj_norm_id);
                  prrc_write_buf_ptr += amt_wrtn;
                  read_cursor += UNSIGNED_INT_SIZE;
                  //translate_int(&read_cursor, &prrc_write_buf_ptr, 0, 0);
 
                  //page rbu detail record group_num
                  translate_short(&read_cursor, &prrc_write_buf_ptr, 0);
 
                  //page rbu detail record host_id
                  short host_var = 0;
                  memcpy(&host_var, read_cursor, SHORT_SIZE);
                  GET_HOST_NORM_ID(host_var, HOST_TYPE, generator_idx, nvm_id);
                  amt_wrtn = sprintf(prrc_write_buf_ptr, "%u,", obj_norm_id);
                  prrc_write_buf_ptr += amt_wrtn;
                  read_cursor += SHORT_SIZE;
                  //translate_short(&read_cursor, &prrc_write_buf_ptr, 0);

                  //page rbu detail record browser_type
                  translate_short(&read_cursor, &prrc_write_buf_ptr, 0);

                  //page rbu detail record screen_height
                  translate_short(&read_cursor, &prrc_write_buf_ptr, 0);

                  //page rbu detail record screen_weidth
                  translate_short(&read_cursor, &prrc_write_buf_ptr, 0);
          
                  //page rbu detail record dom_content_time
                  translate_int(&read_cursor, &prrc_write_buf_ptr, 0, 0);
 
                  //page rbu detail record on_load_time
                  translate_int(&read_cursor, &prrc_write_buf_ptr, 0, 0);

                  //page rbu detail record page_load_time
                  translate_int(&read_cursor, &prrc_write_buf_ptr, 0, 0);
  
                  //page rbu detail record start_render_time
                  translate_int(&read_cursor, &prrc_write_buf_ptr, 0, 0); 

                  //page rbu detail record time_to_interact
                  translate_int(&read_cursor, &prrc_write_buf_ptr, 0, 0);                 

                  //page rbu detail record visually_complete_time
                  translate_int(&read_cursor, &prrc_write_buf_ptr, 0, 0);

                  //page rbu detail record req_server_count
                  translate_int(&read_cursor, &prrc_write_buf_ptr, 0, 0);
           
                  //page rbu detail record req_browser_cache_count
                  translate_int(&read_cursor, &prrc_write_buf_ptr, 0, 0);

                  //page rbu detail record req_text_type_count
                  translate_int(&read_cursor, &prrc_write_buf_ptr, 0, 0);

                  //page rbu detail record req_text_type_size
                  translate_int(&read_cursor, &prrc_write_buf_ptr, 0, 0);

                  //page rbu detail record req_js_type_count
                  translate_int(&read_cursor, &prrc_write_buf_ptr, 0, 0);

                  //page rbu detail record &req_js_type_cum_size
                  translate_int(&read_cursor, &prrc_write_buf_ptr, 0, 0);

                  //page rbu detail record req_css_type_count
                  translate_int(&read_cursor, &prrc_write_buf_ptr, 0, 0);

                  //page rbu detail record req_css_type_cum_size
                  translate_int(&read_cursor, &prrc_write_buf_ptr, 0, 0);

                  //page rbu detail record req_image_type_count
                  translate_int(&read_cursor, &prrc_write_buf_ptr, 0, 0);

                  //page rbu detail record req_image_type_cum_size
                  translate_int(&read_cursor, &prrc_write_buf_ptr, 0, 0);

                  //page rbu detail record req_other_type_count
                  translate_int(&read_cursor, &prrc_write_buf_ptr, 0, 0);

                  //page rbu detail record req_other_type_cum_size
                  translate_int(&read_cursor, &prrc_write_buf_ptr, 0, 0);

                  //page rbu detail record bytes_recieved
                  translate_int(&read_cursor, &prrc_write_buf_ptr, 0, 0);

                  //page rbu detail record bytes_sent
                  translate_int(&read_cursor, &prrc_write_buf_ptr, 0, 0);

                  //page rbu detail record speed_index
                  translate_int(&read_cursor, &prrc_write_buf_ptr, 0, 0);

                  //page rbu detail record main_url_start_date_time
                  translate_unsigned_long(&read_cursor, &prrc_write_buf_ptr, 0);

                  //page rbu detail record har_file_date_time
                  translate_unsigned_long(&read_cursor, &prrc_write_buf_ptr, 0);

                  //page rbu detail record req_count_before_dom_content
                  translate_int(&read_cursor, &prrc_write_buf_ptr, 0, 0);

                  //page rbu detail record req_count_before_on_load
                  translate_int(&read_cursor, &prrc_write_buf_ptr, 0, 0);

                  //page rbu detail record req_count_before_start_rendering
                  translate_int(&read_cursor, &prrc_write_buf_ptr, 0, 0);

                  //page rbu detail record browser_cached_req_count_before_dom_content
		  translate_int(&read_cursor, &prrc_write_buf_ptr, 0, 0);

                  //page rbu detail record browser_cached_req_count_before_on_load
                  translate_int(&read_cursor, &prrc_write_buf_ptr, 0, 0);

                  //page rbu detail record browser_cached_req_count_before_start_rendering
                  translate_int(&read_cursor, &prrc_write_buf_ptr, 0, 0);

                  //page rbu detail record cookies_cav_nv
                  rbu_size = 0;
                  memcpy(&rbu_size, read_cursor, UNSIGNED_INT_SIZE);
                  read_cursor += UNSIGNED_INT_SIZE;
                  
                  if(rbu_size > 0)
                    read_variable_len_value(&read_cursor, &prrc_write_buf_ptr, rbu_size);
                  amt_wrtn = sprintf(prrc_write_buf_ptr, ",");
                  prrc_write_buf_ptr += amt_wrtn;
                  //translate_int(&read_cursor, &prrc_write_buf_ptr, 1, 0);
                  
                  //page rbu detail record group_num
                  rbu_size = 0;
                  memcpy(&rbu_size, read_cursor, UNSIGNED_INT_SIZE);
                  read_cursor += UNSIGNED_INT_SIZE;

                  if(rbu_size > 0)
                    read_variable_len_value(&read_cursor, &prrc_write_buf_ptr, rbu_size);
                  amt_wrtn = sprintf(prrc_write_buf_ptr, ",");
                  prrc_write_buf_ptr += amt_wrtn;

                  //page rbu detail record profile_name
                  rbu_size = 0;
                  memcpy(&rbu_size, read_cursor, UNSIGNED_INT_SIZE);
                  read_cursor += UNSIGNED_INT_SIZE;

                  if(rbu_size > 0)
                    read_variable_len_value(&read_cursor, &prrc_write_buf_ptr, rbu_size);
                  amt_wrtn = sprintf(prrc_write_buf_ptr, ",");
                  prrc_write_buf_ptr += amt_wrtn;

                  //RBU page Status
                  translate_int(&read_cursor, &prrc_write_buf_ptr, 0, 0);

                  //Session Instance
                  translate_int(&read_cursor, &prrc_write_buf_ptr, 0, 0);

                  //page rbu detail record device info
                  rbu_size = 0;
                  memcpy(&rbu_size, read_cursor, UNSIGNED_INT_SIZE);
                  read_cursor += UNSIGNED_INT_SIZE;

                  if(rbu_size > 0)
                    read_variable_len_value(&read_cursor, &prrc_write_buf_ptr, rbu_size);
                  amt_wrtn = sprintf(prrc_write_buf_ptr, ",");
                  prrc_write_buf_ptr += amt_wrtn;

                  //Performance Trace Dump
                  translate_short(&read_cursor, &prrc_write_buf_ptr, 0);

                  //DOM Elements
                  translate_int(&read_cursor, &prrc_write_buf_ptr, 0, 0);

                  //Backend Response Time
                  translate_int(&read_cursor, &prrc_write_buf_ptr, 0, 0);

                  //Byte Received Before DomContent Event Fired
                  translate_int(&read_cursor, &prrc_write_buf_ptr, 0, 0);

                  //Byte Received Before OnLoad Event Fired
                  translate_int(&read_cursor, &prrc_write_buf_ptr, 0, 0);

                  //First Paint Time
                  translate_int(&read_cursor, &prrc_write_buf_ptr, 0, 0);

                  //First Contentful Paint Time
                  translate_int(&read_cursor, &prrc_write_buf_ptr, 0, 0);

                  //Largest Contentful Paint Time
                  translate_int(&read_cursor, &prrc_write_buf_ptr, 0, 0);

                  //Cumulative Layout Shift
                  rbu_size = 0;
                  memcpy(&rbu_size, read_cursor, UNSIGNED_INT_SIZE);
                  read_cursor += UNSIGNED_INT_SIZE;

                  if(rbu_size > 0)
                    read_variable_len_value(&read_cursor, &prrc_write_buf_ptr, rbu_size);
                  amt_wrtn = sprintf(prrc_write_buf_ptr, ",");
                  prrc_write_buf_ptr += amt_wrtn;

                  //Total Blocking Time
                  translate_int(&read_cursor, &prrc_write_buf_ptr, 1, 0);

                  if ((read_cursor - read_buffer) > read_amt)
                    printf("shouldn't happen\n"); 
                break;

                case RBU_LIGHTHOUSE_RECORD:
                  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "reached in RBU_LIGHTHOUSE_RECORD:");          
 
                  //rbu lighthouse detail record partition_id
                  translate_unsigned_long(&read_cursor, &rlhr_write_buf_ptr, 0); 

                  //rbu lighthouse detail record rbu_lh_file_name
                  int rbu_lh_size = 0;
                  memcpy(&rbu_lh_size, read_cursor, UNSIGNED_INT_SIZE);
                  read_cursor += UNSIGNED_INT_SIZE;

                  if (rbu_lh_size > 0)
                    read_variable_len_value(&read_cursor, &rlhr_write_buf_ptr, rbu_lh_size);
                  amt_wrtn = sprintf(rlhr_write_buf_ptr, ",");
                  rlhr_write_buf_ptr += 1;

                  //rbu lighthouse detail record page_idx
                  translate_int(&read_cursor, &rlhr_write_buf_ptr, 0, 0); 
   
                  //rbu lighthouse detail record sess_idx
                  translate_int(&read_cursor, &rlhr_write_buf_ptr, 0, 0); 

                  //rbu lighthouse detail record nvm_id
                  memcpy(&nvm_id, read_cursor, SHORT_SIZE);
                  translate_short(&read_cursor, &rlhr_write_buf_ptr, 0);
 
                  memcpy(&generator_idx, read_cursor, UNSIGNED_INT_SIZE);
                  if(generator_idx == -1) 
                  {
                    //NS case, no need to do normalization
                    obj_norm_id = -1;
                    generator_idx = 0;
		  }
                  else
                    GET_GENERATOR_NORM_ID(generator_idx, GEN_TYPE);

                  amt_wrtn = sprintf(rlhr_write_buf_ptr, "%d,", obj_norm_id);
                  rlhr_write_buf_ptr += amt_wrtn;
                  read_cursor += UNSIGNED_INT_SIZE;
 
                  //rbu lighthouse detail record group_num
                  translate_short(&read_cursor, &rlhr_write_buf_ptr, 0);
 
                  //rbu lighthouse detail record host_id
                  short lh_host_var = 0;
                  memcpy(&lh_host_var, read_cursor, SHORT_SIZE);
                  GET_HOST_NORM_ID(lh_host_var, HOST_TYPE, generator_idx, nvm_id);
                  amt_wrtn = sprintf(rlhr_write_buf_ptr, "%u,", obj_norm_id);
                  rlhr_write_buf_ptr += amt_wrtn;
                  read_cursor += SHORT_SIZE;

                  //rbu lighthouse detail record browser_type
                  translate_short(&read_cursor, &rlhr_write_buf_ptr, 0);

                  //rbu lighthouse detail record group_name
                  rbu_lh_size = 0;
                  memcpy(&rbu_lh_size, read_cursor, UNSIGNED_INT_SIZE);
                  read_cursor += UNSIGNED_INT_SIZE;

                  if(rbu_lh_size > 0)
                    read_variable_len_value(&read_cursor, &rlhr_write_buf_ptr, rbu_lh_size);
                  amt_wrtn = sprintf(rlhr_write_buf_ptr, ",");
                  rlhr_write_buf_ptr += amt_wrtn;

                  //rbu lighthouse detail record lh_file_date_time
                  translate_unsigned_long(&read_cursor, &rlhr_write_buf_ptr, 0);

                  //rbu lighthouse detail record performance score
                  translate_int(&read_cursor, &rlhr_write_buf_ptr, 0, 0);
 
                  //rbu lighthouse detail record PWA score
                  translate_int(&read_cursor, &rlhr_write_buf_ptr, 0, 0);

                  //rbu lighthouse detail record accessibility score 
                  translate_int(&read_cursor, &rlhr_write_buf_ptr, 0, 0);
  
                  //rbu lighthouse detail record Best Practice score 
                  translate_int(&read_cursor, &rlhr_write_buf_ptr, 0, 0); 

                  //rbu lighthouse detail record SEO Score
                  translate_int(&read_cursor, &rlhr_write_buf_ptr, 0, 0);                 

                  //rbu lighthouse detail record FirstContentfulPaint 
                  translate_int(&read_cursor, &rlhr_write_buf_ptr, 0, 0);

                  //rbu lighthouse detail record FirstMeaningfulPaint 
                  translate_int(&read_cursor, &rlhr_write_buf_ptr, 0, 0);
           
                  //rbu lighthouse detail record SpeedIndex 
                  translate_int(&read_cursor, &rlhr_write_buf_ptr, 0, 0);

                  //rbu lighthouse detail record FirstCPUIdle 
                  translate_int(&read_cursor, &rlhr_write_buf_ptr, 0, 0);

                  //rbu lighthouse detail record TimeToInteract 
                  translate_int(&read_cursor, &rlhr_write_buf_ptr, 0, 0);

                  //rbu lighthouse detail record InputLatency 
                  translate_int(&read_cursor, &rlhr_write_buf_ptr, 0, 0);

                  //rbu lighthouse detail record Largest Contentful Paint Time
                  translate_int(&read_cursor, &rlhr_write_buf_ptr, 0, 0);

                  //rbu lighthouse detail record Cumulative Layout Shift
                  rbu_lh_size = 0;
                  memcpy(&rbu_lh_size, read_cursor, UNSIGNED_INT_SIZE);
                  read_cursor += UNSIGNED_INT_SIZE;

                  if(rbu_lh_size > 0)
                    read_variable_len_value(&read_cursor, &rlhr_write_buf_ptr, rbu_lh_size);
                  amt_wrtn = sprintf(rlhr_write_buf_ptr, ",");
                  rlhr_write_buf_ptr += amt_wrtn;

                  //rbu lighthouse detail record Total Blocking Time
                  translate_int(&read_cursor, &rlhr_write_buf_ptr, 1, 0);

 
                  if ((read_cursor - read_buffer) > read_amt)
                    printf("shouldn't happen\n"); 
                break;
                case RBU_MARK_MEASURE_RECORD: 
                  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "reached in RBU_MARK_MEASURE_RECORD:");          

                  //rbu mark/measure record page_idx
                  translate_int(&read_cursor, &rbu_mm_write_buf_ptr, 0, 0); 
   
                  //rbu mark/measure detail record sess_idx
                  translate_int(&read_cursor, &rbu_mm_write_buf_ptr, 0, 0); 

                  //rbu lighthouse detail record nvm_id
                  memcpy(&nvm_id, read_cursor, SHORT_SIZE);
                  translate_short(&read_cursor, &rbu_mm_write_buf_ptr, 0);
 
                  memcpy(&generator_idx, read_cursor, UNSIGNED_INT_SIZE);
                  if(generator_idx == -1) 
                  {
                    //NS case, no need to do normalization
                    obj_norm_id = -1;
                    generator_idx = 0;
		  }
                  else
                    GET_GENERATOR_NORM_ID(generator_idx, GEN_TYPE);

                  amt_wrtn = sprintf(rbu_mm_write_buf_ptr, "%d,", obj_norm_id);
                  rbu_mm_write_buf_ptr += amt_wrtn;
                  read_cursor += UNSIGNED_INT_SIZE;
 
                  //rbu lighthouse detail record group_num
                  translate_short(&read_cursor, &rbu_mm_write_buf_ptr, 0);
 
                  //rbu lighthouse detail record host_id
                  memcpy(&lh_host_var, read_cursor, SHORT_SIZE);
                  GET_HOST_NORM_ID(lh_host_var, HOST_TYPE, generator_idx, nvm_id);
                  amt_wrtn = sprintf(rbu_mm_write_buf_ptr, "%u,", obj_norm_id);
                  rbu_mm_write_buf_ptr += amt_wrtn;
                  read_cursor += SHORT_SIZE;

                  //absStartTime.
                  translate_unsigned_long(&read_cursor, &rbu_mm_write_buf_ptr, 0);
 
                  //get name size.
                  short name_len = 0;
                  memcpy(&name_len, read_cursor, SHORT_SIZE);
                  read_cursor += SHORT_SIZE;

                  //copy name
                  if(name_len > 0)
                    read_variable_len_value(&read_cursor, &rbu_mm_write_buf_ptr, name_len);
                  amt_wrtn = sprintf(rbu_mm_write_buf_ptr, ",");
                  rbu_mm_write_buf_ptr += amt_wrtn;

                  //copy type.
                  translate_unsigned_char(&read_cursor, &rbu_mm_write_buf_ptr, 0);

                  //copy startTime
                  translate_int(&read_cursor, &rbu_mm_write_buf_ptr, 0, 0);

                  //copy duration.
                  translate_int(&read_cursor, &rbu_mm_write_buf_ptr, 0, 0);

                  //sess instance
                  translate_int(&read_cursor, &rbu_mm_write_buf_ptr, 0, 0);
 
                  //page instance
                  translate_short(&read_cursor, &rbu_mm_write_buf_ptr, 1);
                  break;
		case DATA_RECORD:
			NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "reached in DATA_RECORD");
data_length = get_unsigned_int(&read_cursor);
key_data.child_idx = get_child_idx(&read_cursor);
key_data.sess_inst = get_unsigned_int(&read_cursor);
key_data.url_idx = get_index(&read_cursor);

			memset(&key, 0, sizeof(DBT));
			memset(&data, 0, sizeof(DBT));
key.data = &key_data;
key.size = sizeof(key_data);

if (data_hash_table->get(data_hash_table, NULL, &key, &data, 0) == DB_NOTFOUND) {
	data_entry.data_length = 0;
	data_entry.head_data = NULL;
	data_entry.cur_data = NULL;
	data_entry_ptr = &data_entry;
} else
	data_entry_ptr = data.data;

while (data_length) {
	offset = data_entry_ptr->data_length % DATA_BUFFER_SIZE;
	if (!offset) { /* need new buffer */
		if (!data_entry_ptr->head_data) {
			data_entry_ptr->head_data = data_entry_ptr->cur_data = (struct data*) malloc(sizeof(struct data));
			data_entry_ptr->head_data->next = NULL;
		} else {
			new_data_slot = (struct data*) malloc(sizeof(struct data));
			new_data_slot->next = NULL;
			data_entry_ptr->cur_data->next = new_data_slot;
			data_entry_ptr->cur_data = new_data_slot;
		}
	}
	free_slot_bytes = DATA_BUFFER_SIZE - offset;
	copy_amount = (data_length > free_slot_bytes)?free_slot_bytes:data_length;
	memcpy(data_entry_ptr->cur_data->data + offset, read_cursor, copy_amount);
	data_entry_ptr->data_length += copy_amount;
	data_length -= copy_amount;
	read_cursor += copy_amount;
}

			memset(&data, 0, sizeof(DBT));
data.data = data_entry_ptr;
data.size = sizeof(struct data_file_entry);

if (data_hash_table->put(data_hash_table, NULL, &key, &data, 0) != 0) {
	NS_EXIT(-1, "logging_reader: Error in entering into hash table");
}
break;
		case DATA_LAST_RECORD:
			NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "reached in DATA_LAST_RECORD");
data_length = get_unsigned_int(&read_cursor);
key_data.child_idx = get_child_idx(&read_cursor);
key_data.sess_inst = get_unsigned_int(&read_cursor);
key_data.url_idx = get_index(&read_cursor);

			memset(&key, 0, sizeof(DBT));
			memset(&data, 0, sizeof(DBT));
key.data = &key_data;
key.size = sizeof(key_data);

if (data_hash_table->get(data_hash_table, NULL, &key, &data, 0) == DB_NOTFOUND) {
	data_entry.data_length = 0;
	data_entry.head_data = NULL;
	data_entry.cur_data = NULL;
	data_entry_ptr = &data_entry;
	new_entry = 1;
} else {
	data_entry_ptr = data.data;
	new_entry = 0;
}

while (data_length) {
	offset = data_entry_ptr->data_length % DATA_BUFFER_SIZE;
	if (!offset) { /* need new buffer */
		if (!data_entry_ptr->head_data) {
			data_entry_ptr->head_data = data_entry_ptr->cur_data = (struct data*) malloc(sizeof(struct data));
			data_entry_ptr->head_data->next = NULL;
		} else {
			new_data_slot = (struct data*) malloc(sizeof(struct data));
			new_data_slot->next = NULL;
			data_entry_ptr->cur_data->next = new_data_slot;
			data_entry_ptr->cur_data = new_data_slot;
		}
	}
	free_slot_bytes = DATA_BUFFER_SIZE - offset;
	copy_amount = (data_length > free_slot_bytes)?free_slot_bytes:data_length;
	memcpy(data_entry_ptr->cur_data->data + offset, read_cursor, copy_amount);
	data_entry_ptr->data_length += copy_amount;
	data_length -= copy_amount;
	read_cursor += copy_amount;
}

write_data_entry(&key_data, data_entry_ptr, data_out_fptr);

if (!new_entry)
	if (data_hash_table->del(data_hash_table, NULL, &key, 0) != 0) {
		NS_EXIT(-1, "logging_reader: Error in deleting entry in hash table");
	}
break;

/*Begin: PAGEDUMP*/
		case MSG_RECORD:
		case MSG_LAST_RECORD:
			ADD_MSG_ENTRY(record_num, read_cursor, data_length, child_idx, pg_data_entry_ptr, offset, free_slot_bytes, copy_amount, pg_new_data_slot)
			/*If record_num is msg last record then we will write msg in log file*/
			if (record_num == MSG_LAST_RECORD) {
                          //If total_size is NOT 0 then free msg entry and log in trace log. 
                          if (pg_data_entry_ptr->total_size != 0) {
                            NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, 
                             "Total message size (%d) is not zero hence free message entry structure", pg_data_entry_ptr->total_size); 
    		            free_msg_entry(pg_data_entry_ptr); 
                          } else {   
 	                    write_msg_entry(pg_data_entry_ptr, msg_out_fptr, child_idx_with_gen_id);
                            RESET_PG_DATA_ENTRTY_PTR
			  }
                        }
break;
/*End: PAGEDUMP*/

		case -1:
			NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "reached in -1 case, where block_size is = %d, and its processed = %d", read_amt, (read_cursor - read_buffer));
read_cursor = read_buffer + read_amt;
break;

		default:
			NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "reached in default case, where block_size is = %d, and bytes processed = %d, no. of skip_dlog_buffer = %d", read_amt, (read_cursor - read_buffer), ++skip_dlog_buffer);

//Commenting below code as in old design, logging writer used to insert spaces in the extra left bytes but in new design, logging writer writes -1 to indicate the end of data in read_buffer.
/* blank_space = (record_num - TOTAL_RECORDS) - 1;  // the decrement of 1 is b/c read_cursor is already incremented by 1 after reading in record_num 
if (blank_space != (read_amt - (read_cursor - read_buffer)))
	printf("url_records is %d, total_read_amt: %d\n", url_records, total_read_amt);
assert(blank_space == (read_amt - (read_cursor - read_buffer)));
read_cursor += blank_space;
if ((read_cursor - read_buffer) > read_amt)
	printf("shouldn't happen\n");
*/
			read_cursor = read_buffer + read_amt;
			break;

		}
	}

  write_rec_in_csv(WRITE_CONDITIONALLY);
}

static void switch_partition(char *next_partition);

int translate_dyn_file(void) {
  char read_buffer[log_shr_buffer_size];
  int read_amt;
  char* read_cursor;
  int total_read_amt = 0;
  int need_to_read; //for handling partial read case
  short partial_error_count = 0;
  char buf[MAX_FILENAME_LENGTH];
  struct stat s;

  //commenting becoz we are already resetting the buffer in WRITE_IN_CSV macro
  //we should not call this func here, as we do not write csv on every exit of this func.
  //init_rec_bufs();
 
  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO,  "Method called");
  if (!d_in_fptr)
    return 0; //As we have check for bytes read
  while (1) {
    read_amt = fread(read_buffer, sizeof(char), log_shr_buffer_size, d_in_fptr);
    if (read_amt < 0)
    {
      if(errno == EINTR) //TODO check if fread gets interupted
        continue;
      return total_read_amt;
    }
    if (read_amt != log_shr_buffer_size)
      break;

    total_read_amt += read_amt;
    dlog_read_offset += read_amt;
    NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Writing offset %lld in paritition %s", dlog_read_offset, partition_name);
    process_lr_buffer(read_buffer, read_amt/*BLOCK_SIZE*/, total_read_amt);      
  }

  if (read_amt > 0) { // Partial Block read
    //fprintf(stderr, "Error: Partial block read. read_amt = %d\n", read_amt);
    NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Partial Block read. read_amt = %d\n", read_amt);
    read_cursor = read_buffer + read_amt;
    need_to_read = log_shr_buffer_size - read_amt;
    
    /* This infinite loop will only be breaked if buffer of BLOCK_SIZE will be read */
    while(1){
      if((read_amt = fread(read_cursor, sizeof(char), need_to_read, d_in_fptr)) == need_to_read){
        //Increment offset by BLOCK_SIZE as we may read this block in many read
        dlog_read_offset += log_shr_buffer_size;
        NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Writing offset %lld in paritition %s", dlog_read_offset, partition_name);
        NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Partial Block Handling: Complete block recieved, Processing block");
        total_read_amt += log_shr_buffer_size;
        process_lr_buffer(read_buffer, log_shr_buffer_size, total_read_amt);
        //once completed block processed then break and wait for signal
        break;
      }
      else if(read_amt > 0){
        NSLB_TRACE_LOG3(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Partial Block Handling: Partial Block read. read_amt = %d", read_amt);
        read_cursor += read_amt;
        need_to_read -= read_amt;
        partial_error_count = 0;
      }
      else {
        //Print error once in 100 time 
        if(!((partial_error_count++) % 100)) {
          if(read_amt == 0) {
            NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Partial Block Handling: End of file reached, read_amt = 0");
            //fprintf(stderr, "Partial Block Handling: End of file reached, read_amt = 0\n");
          }
          else  
            fprintf(stderr, "Partial Block Handling: error in reading file, error = %s\n", nslb_strerror(errno));
        }

/***** bug 79529 begin */
        if((partition_idx > 0) && (testruninfo_tbl_shr->partition_idx != partition_idx))
        {
          process_lr_buffer(read_buffer, log_shr_buffer_size, total_read_amt);

          //TL - Shm is diff
          char next_partition[128 + 1];
          while(1)
          {
            if(nslb_get_next_partition(wdir, test_id, partition_name, next_partition) == 0)
            {
              NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "next_partition = %s", next_partition);
           
              //TL - next is found
              //if yes then only switch the partition
              snprintf(buf, MAX_FILENAME_LENGTH, "%s/logs/TR%d/%s/reports/raw_data/dlog",
                  wdir, test_id, next_partition);     
           
              //Recovery Switching: Switch partition if next partition is not equal to partition in shared memory
              //Normal Switching: Switch partition if next partition has dlog
              if(stat(buf, &s) == 0 || atoll(next_partition) != testruninfo_tbl_shr->partition_idx)
              {
                //TL - next is found
                switch_partition(next_partition); 
                break;
              }
              //else continue to next partition
            }
            else
            {
              if(partial_error_count == 3000) /*3000*100ms = 300s = 5 min*/
              {
                NS_EXIT(-1, "Error: nsu_logging_reader, Error in reading dlog file, Exiting!");
             } 
              NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "sleeping for 150ms");
              usleep(100000/*100 ms*/);
              break;
            }
          }
          //else 
            //TL - next is not found
        }
        else
        {        /* if 5 minute reached and still error coming then exit */
          if(partial_error_count == 3000) /*3000*100ms = 300s = 5 min*/{
            NS_EXIT(-1, "Error: nsu_logging_reader, Error in reading dlog file, Exiting!");
          } 
          NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "sleeping for 150ms");
          usleep(100000/*100 ms*/);
        }
        
/***** bug 79529 end */
      }
    }
  }
  else if (read_amt < 0) // Error in reading file
   {
    fprintf(stderr, "Error: Error in reading file. Error = %s\n", nslb_strerror(errno));
   }
  else // read_amt is 0. End of file reached. This will happen at the end. So it is not error
  {
    NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "End of file reached. read_amt = %d", read_amt);
  }

  //Write into file
  //Need to write in file at last
  write_rec_in_csv(WRITE_CONDITIONALLY); //TODO why we need here. 
  return(total_read_amt);

}

void allocate_tables(void) {

  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called");

  init_total_entries_count();

  runProfile = (RunProfileLogEntry *)malloc(INIT_RUNPROFLOG_ENTRIES * sizeof(RunProfileLogEntry));
  userProfile = (UserProfileLogEntry *)malloc(INIT_USERPROFLOG_ENTRIES * sizeof(UserProfileLogEntry));
  sessProfile = (SessionProfileLogEntry *)malloc(INIT_SESSPROFLOG_ENTRIES * sizeof(SessionProfileLogEntry));
  actSvrTable = (ActSvrTableLogEntry *)malloc(INIT_ACTSVRTABLELOG_ENTRIES * sizeof(ActSvrTableLogEntry));
  phaseTable = (PhaseTableLogEntry *)malloc(INIT_PHASETABLELOG_ENTRIES * sizeof(PhaseTableLogEntry));
  recSvrTable = (RecSvrTableLogEntry *)malloc(INIT_RECSVRTABLELOG_ENTRIES * sizeof(RecSvrTableLogEntry));

  if (runProfile && userProfile && sessProfile && actSvrTable && recSvrTable && phaseTable) {
    init_total_entries_count();

    max_run_profile_entries = INIT_RUNPROFLOG_ENTRIES;
    max_user_profile_entries = INIT_USERPROFLOG_ENTRIES;
    max_sess_profile_entries = INIT_SESSPROFLOG_ENTRIES;
    max_recsvr_table_entries = INIT_RECSVRTABLELOG_ENTRIES;
    max_actsvr_table_entries = INIT_ACTSVRTABLELOG_ENTRIES;
    max_phase_table_entries = INIT_PHASETABLELOG_ENTRIES;
  } else {
    NS_EXIT(-1, "error in allocating tables");
  }
}

static void open_file_in_large_mode (FILE **file_fp, char *file_to_open)
{
  int file_fd = 0;

  if ((file_fd = open(file_to_open, O_CREAT | O_CLOEXEC | O_APPEND | O_RDWR | O_NONBLOCK | O_LARGEFILE, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)) == -1) {
    perror("logging_reader");
    NS_EXIT(-1, "Error in opening file %s. Test Run Number '%d' is not valid\n", file_to_open, test_id);
  }

  if((*file_fp = fdopen(file_fd, "w+")) == NULL) {
    perror("logging_reader");
    NS_EXIT(-1, "Error in opening file %s. Test Run Number '%d' is not valid\n", file_to_open, test_id);
  }
} 

static void open_static_csv_file()
{
  char buf[MAX_FILENAME_LENGTH];

  //RunProf
  sprintf(buf, "logs/TR%d/%s/reports/csv/rpf.csv", test_id, common_files_dir);
  if (!(rpr_out_fptr = fopen(buf, "a+"))) {   
    perror("logging_reader");
    NS_EXIT(-1, "Error in opening file %s. Test Run Number '%d' is not valid\n", buf, test_id);
  }
  //UserProf
  sprintf(buf, "logs/TR%d/%s/reports/csv/upf.csv", test_id, common_files_dir);
  if (!(upr_out_fptr = fopen(buf, "w"))) {
    perror("logging_reader");
    NS_EXIT(-1, "Error in opening file %s. Test Run Number '%d' is not valid\n", buf, test_id);
  }
  //SessProf
  sprintf(buf, "logs/TR%d/%s/reports/csv/spf.csv", test_id, common_files_dir);
  if (!(spr_out_fptr = fopen(buf, "w"))) {
    perror("logging_reader");
    NS_EXIT(-1, "Error in opening file %s. Test Run Number '%d' is not valid\n", buf, test_id);
  }
  //Session Table
  sprintf(buf, "logs/TR%d/%s/reports/csv/sst.csv", test_id, common_files_dir);
  if (!(sesstb_out_fptr = fopen(buf, "a+"))) {
    perror("logging_reader");
    NS_EXIT(-1, "Error in opening file %s. Test Run Number '%d' is not valid\n", buf, test_id);
  }
  //Page Table
  sprintf(buf, "logs/TR%d/%s/reports/csv/pgt.csv", test_id, common_files_dir);
  if (!(ptb_out_fptr = fopen(buf, "a+"))) {
    perror("logging_reader");
    NS_EXIT(-1, "Error in opening file %s. Test Run Number '%d' is not valid", buf, test_id);
  }
  //Transaction Table
  sprintf(buf, "logs/TR%d/%s/reports/csv/trt.csv", test_id, common_files_dir);
  if (!(ttb_out_fptr = fopen(buf, "a+"))) {
    perror("logging_reader");
    NS_EXIT(-1, "Error in opening file %s. Test Run Number '%d' is not valid\n", buf, test_id);
  }
  //Url Table
  sprintf(buf, "logs/TR%d/%s/reports/csv/urt.csv", test_id, common_files_dir);
  if (!(utb_out_fptr = fopen(buf, "a+"))) {
    perror("logging_reader");
    NS_EXIT(-1, "Error in opening file %s. Test Run Number '%d' is not valid\n", buf, test_id);
  }
  sprintf(buf, "logs/TR%d/%s/reports/csv/hrt.csv", test_id, common_files_dir);
  if (!(hrt_out_fptr = fopen(buf, "w"))) {
    perror("logging_reader");
    NS_EXIT(-1, "Error in opening file %s. Test Run Number '%d' is not valid\n", buf, test_id);
  }
  sprintf(buf, "logs/TR%d/%s/reports/csv/hat.csv", test_id, common_files_dir);
  if (!(hat_out_fptr = fopen(buf, "w"))) {
    perror("logging_reader");
    NS_EXIT(-1, "Error in opening file %s. Test Run Number '%d' is not valid\n", buf, test_id);
  }
  //Phase Table
  // No ned to do fdopen as it will not be more than 2 GB
  sprintf(buf, "logs/TR%d/%s/reports/csv/log_phase_table.csv", test_id, common_files_dir);
  if (!(lpt_out_fptr = fopen(buf, "w"))) {
    perror("logging_reader");
    NS_EXIT(-1, "Error in opening file %s. Test Run Number '%d' is not valid\n", buf, test_id);
  }
  //Generator Table
  sprintf(buf, "logs/TR%d/%s/reports/csv/generator_table.csv", test_id, common_files_dir);
  if (!(generator_out_fptr = fopen(buf, "a+"))) {
    perror("logging_reader");
    NS_EXIT(-1, "Error in opening file %s. Test Run Number '%d' is not valid\n", buf, test_id);
  }
  //Host Table
  sprintf(buf, "logs/TR%d/%s/reports/csv/HostTable.csv", test_id, common_files_dir);
  if (!(host_out_fptr = fopen(buf, "a+"))) {
    perror("logging_reader");
    NS_EXIT(-1, "Error in opening file %s. Test Run Number '%d' is not valid\n", buf, test_id);
  }
}
 
static void chk_and_reload_slog()
{
  char buf[MAX_FILENAME_LENGTH];
  static unsigned int slog_inode_num = 0; //inode number data type is unsigned int
  struct stat s;
  /*  slog is created in TR/reports/raw_data or TR/common_files/reports/raw_data.
   *  file name is slog.partition_name. New slog is created on every test restart.
   *  A hard link is created in partition/reports/raw_data to slog.
   *  There might be a case when logging_reader crashes and test restarts before recovery of reader.
   *  While filling csv's after recovery, reader must reload tables according to slog.
   *  Hence during recovery when we go to next partition, we must check if slog has changed.
   */
  sprintf(buf, "logs/TR%d/%s/reports/raw_data/slog", test_id, partition_name);

  NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "slog file = %s\n", buf);

  //using stat method to know inode of slog
  if(stat(buf, &s) < 0)
  {
    perror("stat"); //TL
    NS_EXIT(-1, "file does not exist %s\n", buf);
  }

  if(slog_inode_num != s.st_ino) //checking if inode number of slog of last partition is same as current partition.
  {
    if (slog_inode_num != 0) //No need to reset first time
    {
      //Reset all normalization local mapping tables    
      reset_norm_id_mapping_tbl();
      reset_tables();
    } 
    //inode's are different, we need to open slog related to current partition.
  
    NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "New slog found %s in partition %s"
              " Old slog inode = %u, new slog inode = %u", buf, partition_name, slog_inode_num, s.st_ino);

    slog_inode_num = s.st_ino;

    if(s_in_fptr != NULL)
    {
      fclose(s_in_fptr);
      s_in_fptr = NULL;
      /* Gaurav: Bug 44930: Got "out of range" errors in db_upload_error.log file which is appending continuously
         Problem - slog_offset is retaining previous slog size hence not reading dynamic urls from it
         Solution - Resetting slog_offset of previous slog file */
      slog_offset = 0;
    }
    if (!(s_in_fptr = fopen(buf, "r")))
    {
      perror("logging_reader"); //TL
      NS_EXIT(-1, "Error in opening file %s. Test Run Number '%d' is not valid\n", buf, test_id);
    }


    //calling of this method is moved here to make it common for new test and recovery.
    input_static_file(); //Loading tables
  }
}

static void open_dynamic_csv_file()
{

  char buf[MAX_FILENAME_LENGTH];
  static int first_time = 0;
  sprintf(buf, "logs/TR%d/%s/reports/raw_data/dlog", test_id, partition_name);
  int d_in_fd;
  // Neeraj - Added this on Oct 2, 2010 to support opening of file more than 2 Gig in size
  // Cant use open_file_in_large_mode () as it truncte the file in opening
  // 
  NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Opening dlog file = %s", buf);
  if ((d_in_fd = open(buf, O_RDONLY | O_LARGEFILE | O_CLOEXEC)) == -1) {
    fprintf(stderr, "Error in opening logging file %s\n", buf);
    perror("logging_reader");
    return;
  }
  // if (!(d_in_fptr = fopen(buf, "r"))) 
  if (!(d_in_fptr = fdopen(d_in_fd, "r"))) {
    perror("logging_reader");
    NS_EXIT(-1, "Error in opening file %s. Test Run Number '%d' is not valid", buf, test_id);
  }

  sprintf(buf, "logs/TR%d/%s/reports/csv/urc.csv", test_id, partition_name);
  open_file_in_large_mode (&urc_out_fptr, buf);

  sprintf(buf, "logs/TR%d/%s/reports/csv/prc.csv", test_id, partition_name);
  open_file_in_large_mode (&prc_out_fptr, buf);

  //added new table here
  sprintf(buf, "logs/TR%d/%s/reports/csv/tprc.csv", test_id, partition_name);
  open_file_in_large_mode (&tptb_out_fptr, buf);
 
  sprintf(buf, "logs/TR%d/%s/reports/csv/trc.csv", test_id, partition_name);
  open_file_in_large_mode (&trc_out_fptr, buf);

  sprintf(buf, "logs/TR%d/%s/reports/csv/src.csv", test_id, partition_name);
  open_file_in_large_mode (&src_out_fptr, buf);

  sprintf(buf, "logs/TR%d/%s/reports/csv/page_dump.csv", test_id, partition_name);
  open_file_in_large_mode (&page_dump_out_fptr, buf);

  sprintf(buf, "logs/TR%d/%s/reports/csv/PageRBUDetailRecord.csv", test_id, partition_name);
  open_file_in_large_mode (&page_rbu_detail_record_fptr, buf);

  sprintf(buf, "logs/TR%d/%s/reports/csv/RBULightHouseRecord.csv", test_id, partition_name);
  open_file_in_large_mode (&rbu_lighthouse_record_fptr, buf);

  sprintf(buf, "logs/TR%d/%s/reports/csv/RBUUserTiming.csv", test_id, partition_name);
  open_file_in_large_mode (&rbu_mark_measure_record_fptr, buf);
  

  #if 0 
  Commented code in release 3.8.2 
  now this code will not use
  sprintf(buf, "logs/TR%d/%s/tcf.csv", optarg, partition_name);
  if (!(tc_out_fptr = fopen(buf, "w+"))) {
    fprintf(stderr, "Error in opening file %s. Test Run Number '%d' is not valid\n", buf, optarg);
    perror("logging_reader");
    exit(-1);
  }
  #endif
  sprintf(buf, "logs/TR%d/%s/data_log_file", test_id, partition_name);
  if (!(data_out_fptr = fopen(buf, "w+"))) {
    perror("logging_reader");
    NS_EXIT(-1, "Error in opening file %s. Test Run Number '%d' is not valid", buf, test_id);
  }


  //open file .dlog.offset
  snprintf(buf, MAX_FILENAME_LENGTH, "%s/logs/TR%d/%s/reports/raw_data/.dlog.offset",
                               wdir, test_id, partition_name);
  //Do not open in append mode
  dlog_off_fd = open(buf, O_CREAT|O_RDWR|O_CLOEXEC,0666);
  if (dlog_off_fd < 0)
  {
    NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_ERROR, "Error in creating/opening dlog offset file. Filename = %s, Error = %s", buf, nslb_strerror(errno));
    NS_EXIT(-1, "Error in creating/opening dlog offset file. Filename = %s, Error = %s", buf, nslb_strerror(errno));
  }

  /*  When test is started or restarted, we need to read .dlog.offset file, if exists.
   *  At this point we already know the partition number to start recovery.
   */  
  if(first_time == 0)
  {
    read_or_write_dlog_offset(READ_DLOG_OFFSET);

    NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Seeking dlog to offset = %lld", dlog_read_offset);
    /* set file pointer to dlog_read_offset */
    if(fseek(d_in_fptr, dlog_read_offset, SEEK_SET) == -1)
    {
      //TL : Make it ti trace log
      perror(NULL);
      NS_EXIT(-1, "Error in fseek()");
    }
    first_time = 1;
  }
  /* In release 3.9.7, page dump feature has been redesign noe NVMs will write data 
   * and LR need to update page_dump.csv instead of writing data into log file
   * Hence commenting the code */
#if 0
  //log file
  sprintf(buf, "logs/TR%d/log", test_id);
  open_file_in_large_mode (&msg_out_fptr, buf);
#endif
}


  
/*static void open_csv_files (char *optarg)
{
   open_static_csv_file(optarg);
   open_dynamic_csv_file(optarg);
}*/

void close_csv_file_ptr_fd()
{
  NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called");
  FCLOSE(d_in_fptr); //dlog
  FCLOSE(urc_out_fptr);
  FCLOSE(prc_out_fptr);
  FCLOSE(tptb_out_fptr);
  FCLOSE(trc_out_fptr);
  FCLOSE(src_out_fptr);
  FCLOSE(data_out_fptr);
  FCLOSE(msg_out_fptr);
  FCLOSE(page_dump_out_fptr);
  FCLOSE(page_rbu_detail_record_fptr);
  FCLOSE(rbu_lighthouse_record_fptr);
  FCLOSE(rbu_mark_measure_record_fptr);
  dlog_read_offset = 0; //resetting dlog read offset  

  if(dlog_off_fd >= 0) //close dlog read offset fd
  {
    close(dlog_off_fd);
    dlog_off_fd = -1;
  }
  d_in_fptr = NULL;
}

void start_db_upload_tmp_table_process(int testidx, int pid, char *partition_name)
{
   
  NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Starting nsi_upload_tmp_table");
  if ((nsu_db_upload_tmp_table_pid = fork()) ==  0 ) 
  {
    char test_run_num[1024];
    char  db_upload_bin_path[512 + 1];

    sprintf(test_run_num, "%d", testidx);
 
    sprintf(db_upload_bin_path, "%s/bin/nsi_upload_tmp_table", wdir); 

    //not passing partition_name to nsu_db_upload, nsu_db_upload will get the partition_name from library
    if (execlp(db_upload_bin_path, "nsi_upload_tmp_table", 
       test_run_num, wdir, partition_name,
       NULL) == -1) {

       NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_ERROR, "Initializing DB upload tmp table: error in execl.");
       perror("execl");
       NS_EXIT(-1, "Initializing DB upload tmp table: error in execl");
      }
  }
  else 
  {
    if (nsu_db_upload_tmp_table_pid < 0) {
      NS_EXIT(-1, "error in forking the nsu_db_upload_tmp_table process");
    }
  } 
}

void start_db_upload_process(int testidx, int pid, char *partition_name, int testruninfo_shr_id)
{
   
  NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Starting nsu_db_upload");
  if ((nsu_db_upload_pid = fork()) ==  0 ) 
  {
    int ret;
    char test_run_num[1024];
    char shm_key[100];
    char pid_str[1024], db_upload_bin_path[512 + 1], reader_run_mode[8];
    //char args[1024] = {0};

    sprintf(test_run_num, "%d", testidx);
    sprintf(pid_str, "%d", pid);
    sprintf(reader_run_mode, "%d", running_mode);
    sprintf(shm_key, "%d", testruninfo_shr_id);

    sprintf(db_upload_bin_path, "%s/bin/nsu_db_upload", wdir);
 
    //write db_upload pid in get_all_process_pid file.
    ret = nslb_write_all_process_pid(getpid(), "db upload's pid", wdir, testidx, "a");
    if( ret == -1 )
    {
      fprintf(stderr, "failed to open the db upload's pid file\n");
      exit(-1);
    }
    ret = nslb_write_all_process_pid(getppid(), "db_upload parent's pid", wdir, testidx, "a");
    if( ret == -1 )
    {
      fprintf(stderr, "failed to open the db upload parent's pid file\n");
      exit(-1);
    }

    //not passing partition_name to nsu_db_upload, nsu_db_upload will get the partition_name from library
    if (execlp(db_upload_bin_path, db_upload_bin_path, 
       "--testrun", test_run_num,
       "--ppid", pid_str,
       "--running_mode", reader_run_mode, 
       "--test_run_info_shm_key", shm_key,
       NULL) == -1) {

       NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_ERROR, "Initializing DB upload: error in execl.");
       perror("execl");
       NS_EXIT(-1, "Initializing DB upload: error in execl");
      }
  }
  else 
  {
    if (nsu_db_upload_pid < 0) {
      NS_EXIT(-1, "error in forking the nsu_db_upload process\n");
    }
  } 
}

void read_and_close_dlog()
{
  translate_dyn_file();
  write_rec_in_csv(WRITE_UNCONDITIONALLY); 
  //Close csv files ptr and fd
  close_csv_file_ptr_fd();
}

static void save_success_info()
{
  FILE *fp;
  char buf[1024] = "";

  sprintf(buf, "%s/logs/TR%d/%s/reports/csv/.nlr_process_done.txt", wdir, test_id, partition_name); 

  fp = fopen(buf, "w");
  if(!fp)
    return;
  
  fprintf(fp, "Logging Reader: Partition Processed Successfully");
  fclose(fp);
}

void parse_multidisk_keyword()
{
  char buf[1024];
  int ret = 0;
  char component[512];
  char cmd_out[1024];
  char keyword[512];
  int length;
  char path[1024]="";

  sprintf(buf, "if [ -f %s/logs/TR%d/sorted_scenario.conf ];then "
                   "grep -i \"^MULTIDISK_PATH\" %s/logs/TR%d/sorted_scenario.conf;" 
                   "grep -i \"nscsv\" %s/logs/TR%d/sorted_scenario.conf; fi",
                   wdir, test_id, wdir, test_id, wdir, test_id);

  ret = nslb_run_cmd_and_get_last_line(buf, 1024, cmd_out);
  if(ret)
  {
    NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Failed to get MULTIDISK_PATH csvpath keyword values, ERROR executing command: '%s'", buf);
  }

  sscanf(cmd_out, "%s %s %s", keyword, component, path);
  length = strlen(path);
  multidisk_csv_path = malloc(length + 1);
  strcpy(multidisk_csv_path, path);
}

void create_csv_dir_and_link()
{
  struct stat s;
  char buf[1024] = "";
  char out_buf[1024] = "";
  
  sprintf(buf, "%s/logs/TR%d/%s/reports/csv/", wdir, test_id, partition_name);

  if(stat(buf, &s) == 0)
    return;
    
  if(multidisk_csv_path[0] == '\0')
  {
    mkdir_ex(buf);
  }
  else
  {
    sprintf(out_buf, "%s/logs/TR%d/%s/reports/", multidisk_csv_path, test_id, partition_name);
    mkdir_ex(buf);
    mkdir_ex(out_buf);
    sprintf(out_buf, "%s/logs/TR%d/%s/reports/csv", multidisk_csv_path, test_id, partition_name);
    symlink(out_buf, buf);
  }
}

/* Making empty csv even in bad partition, to support DBU bad partition handeling */
void make_empty_csv()
{
  char buf[1024] = "";
  FILE *fp;

  create_csv_dir_and_link();

  sprintf(buf, "logs/TR%d/%s/reports/csv/urc.csv", test_id, partition_name);
  open_file_in_large_mode (&fp, buf);
  fclose(fp);

  sprintf(buf, "logs/TR%d/%s/reports/csv/prc.csv", test_id, partition_name);
  open_file_in_large_mode (&fp, buf);
  fclose(fp);

  //added new table here
  sprintf(buf, "logs/TR%d/%s/reports/csv/tprc.csv", test_id, partition_name);
  open_file_in_large_mode (&fp, buf);
  fclose(fp);
 
  sprintf(buf, "logs/TR%d/%s/reports/csv/trc.csv", test_id, partition_name);
  open_file_in_large_mode (&fp, buf);
  fclose(fp);

  sprintf(buf, "logs/TR%d/%s/reports/csv/src.csv", test_id, partition_name);
  open_file_in_large_mode (&fp, buf);
  fclose(fp);

  sprintf(buf, "logs/TR%d/%s/reports/csv/page_dump.csv", test_id, partition_name);
  open_file_in_large_mode (&fp, buf);
  fclose(fp);

  sprintf(buf, "logs/TR%d/%s/reports/csv/PageRBUDetailRecord.csv", test_id, partition_name);
  open_file_in_large_mode (&fp, buf);
  fclose(fp);

  sprintf(buf, "logs/TR%d/%s/reports/csv/RBULightHouseRecord.csv", test_id, partition_name);
  open_file_in_large_mode (&fp, buf);
  fclose(fp);

  sprintf(buf, "logs/TR%d/%s/reports/csv/RBUUserTiming.csv", test_id, partition_name);
  open_file_in_large_mode (&fp, buf);
  fclose(fp);

}

static int check_slog_exists()
{ 
  char buf[MAX_FILENAME_LENGTH];
  char next_partition[128 + 1];
  struct stat s;

  while(1)
  {
    make_empty_csv();
    sprintf(buf, "logs/TR%d/%s/reports/raw_data/slog", test_id, partition_name);

    NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Checking if slog file %s exists or not\n", buf);

    if(stat(buf, &s) < 0)
    {
      //fprintf(stderr, "slog file '%s' does not exist\n", buf); 
      NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "slog file '%s' does not exist", buf);
      //perror("stat"); 
      save_success_info();
    }
    else
      return 1; //found slog somewhere

    if(nslb_get_next_partition(wdir, test_id, partition_name, next_partition) == 0) //found next partition 
    {
      NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "next_partition = %s", next_partition);
      strcpy(partition_name, next_partition);
      partition_idx = atoll(partition_name);
    }
    else
      return 0; //error in getting next partition/next_partition value is not > 0
  }
  return 0;
}

static void switch_partition(char *next_partition)
{
  NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called switch_partition(). next_partition = %s", next_partition);

  read_and_close_dlog();
  save_success_info();

  NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "From switch_partition(), Checking if slog exists or not. partition is = %s", partition_name);

  strcpy(partition_name, next_partition);
  partition_idx = atoll(partition_name);

  if (check_slog_exists() == 0)
  {
    NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "From switch_partition(), Not found slog in any partition, current partition is = %s", partition_name); 
    NS_EXIT(-1, "From switch_partition(), Not found slog in any partition, current partition is = %s", partition_name);
  }

  //Open csv files
  //
  chk_and_reload_slog();
  open_dynamic_csv_file();

  if(running_mode && (nsu_db_upload_pid > 0))
  {
    kill(nsu_db_upload_pid, PARTITION_SWITCH_SIG);
  }
}

/*  If loggin reader was crashed and restarted, This function fills csv's that couldn't be processed meanwhile. 
 *  We have already found the partition to start with; 
 *  Now we process dlog and go to next partition untill last partition is encountered
 *  We find last partition when there is no entry of next partition in .partition_info file.
 */
//This function is called in two cases
//1. Offline mode
//2. Online mode after getting TEST_POST_PROC signal
static void run_offline()
{
  sighandler_t prev_handler;
  sighandler_t prev_handler2;
  prev_handler = signal(SIGUSR1, SIG_IGN); //ignore dlog signal from writer
  prev_handler2 = signal(PARTITION_SWITCH_SIG, SIG_IGN); //ignore partition switch signal from NS

  /*  Calling these two methods before loop.
   *  When we start filling csv of all partitions, first we open csv files, then process, then close.
   *  Then we switch to next partition. 
   *  After processing last partition, we cannot close csv as data is being written in this partition.
   *  In while loop we cannot determine whether partition is last or not,
   *  as we determine last partition when we try to get next partition and no next partition is found.
   *  Hence calling these two methods for the partition we selected to start with after crash.
   *  Then we close its csv in loop and csv of next partition are opened.
   */
  //open_dynamic_csv_file already called in main 
  //open_dynamic_csv_file();
  read_and_close_dlog();
  //Loop will continue untill last partition is processes 
  //and csv of last partition will not be closed.
  if (partition_idx > 0) {
    while(nslb_get_next_partition(wdir, test_id, partition_name, partition_name) == 0)
    {
      save_success_info();
      NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Traversing to partition %s for processing dlog.", partition_name);
      partition_idx = atoll(partition_name); 

      NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "From run_offline(), Checking if slog exists or not. partition is = %s", partition_name);
      if (check_slog_exists() == 0)
      {
        NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "From run_offline(), Not found slog in any partition, current partition is = %s", partition_name); 
        NS_EXIT(-1, "From run_offline(), Not found slog in any partition, current partition is = %s", partition_name);
      }

      open_dynamic_csv_file();
      chk_and_reload_slog();
      read_and_close_dlog();
    }
    NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "No more partition to process dlog.");
  }
  //release signals
  (void) signal(SIGUSR1, prev_handler);  
  (void) signal(PARTITION_SWITCH_SIG, prev_handler2);   //Fixed build warning, where prev_handler should be used in case of prev_handler2
}

// Returns
//   0 - Continue in same partition
//   1 - Continue in next parition
//   2 - Done
static int process_dlog_file()
{
  int bytes_read = 0;
  char buf[MAX_FILENAME_LENGTH];
  struct stat s;

  bytes_read = translate_dyn_file();

  //No data is read from dlog && test is running in partition creation mode && partition name is diff in shared memory
  //This case may happen if partition switch signal is lost
  if(bytes_read == 0 && (partition_idx > 0) && (testruninfo_tbl_shr->partition_idx != partition_idx))
  {
    //TL - Shm is diff
    char next_partition[128 + 1];
    if(nslb_get_next_partition(wdir, test_id, partition_name, next_partition) == 0)
    {
      NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "next_partition = %s", next_partition);

      //TL - next is found
      //if yes then only switch the partition
      snprintf(buf, MAX_FILENAME_LENGTH, "%s/logs/TR%d/%s/reports/raw_data/dlog",
          wdir, test_id, next_partition);     

      //Recovery Switching: Switch partition if next partition is not equal to partition in shared memory
      //Normal Switching: Switch partition if next partition has dlog
      if(stat(buf, &s) == 0 || atoll(next_partition) != testruninfo_tbl_shr->partition_idx)
      {
        //TL - next is found
        switch_partition(next_partition); 
      }
      //else
        //TL dlog in not
    }
    //else 
      //TL - next is not found
  }
  return 0;
}

static void init_parition_idx(int testruninfo_shr_id)
{
  //If wdir not set, set wdir to /home/cavisson/work
  if(wdir[0] == '\0') 
    strcpy(wdir,"/home/cavisson/work");

  sprintf(base_dir, "%s/logs/TR%d", wdir, test_id);
  // Set parition idx and name 
  
  // Only in online mode, shared memory is available 
  if(running_mode > 0)
  {
  //If shared memory ptr option pass then attach share memory to reader  
  //Now shared memory will always be passed
    testruninfo_tbl_shr = (TestRunInfoTable_Shr *) shmat(testruninfo_shr_id, NULL, 0);
    if (testruninfo_tbl_shr == NULL) {
      perror("logging_reader: error in attaching shared memory");
      NS_EXIT (-1, "");
    }
    partition_idx = testruninfo_tbl_shr->partition_idx;
  }
  else
  {
    //Return 0 if non partition mode
    partition_idx = nslb_get_cur_partition(base_dir);
    if(partition_idx < 0) // Error  //TODO Krishan need to discuss
    {
      perror("logging_reader: error in getting current partition.");
      NS_EXIT(-1, "");
    }

  }
  //if partition creation mode is off and normal test is running.
  //By default partition name will be 0 or empty and idx will be 0
  if(partition_idx > 0)
  {
    sprintf(partition_name, "%lld", partition_idx);
    strcpy(common_files_dir, "common_files");
  }
}


static void setup_signal()
{
    NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Setting up signals..");

    bzero (&sa, sizeof(struct sigaction));
    sa.sa_handler = SIG_IGN;
    sigaction(SIGINT, &sa, NULL);

    bzero (&sa, sizeof(struct sigaction));
    sa.sa_handler = SIG_IGN;
    sigaction(SIGHUP, &sa, NULL);

    bzero (&sa, sizeof(struct sigaction));
    sa.sa_handler = sigterm_handler;
    sigaction(SIGTERM, &sa, NULL);

    bzero (&sa, sizeof(struct sigaction));
    sa.sa_handler = sighup_handler;
    sigaction(SIGUSR1, &sa, NULL);

    bzero (&sa, sizeof(struct sigaction));
    sa.sa_handler = sigalarm_handler;
    sigaction(SIGALRM, &sa, NULL);

    bzero (&sa, sizeof(struct sigaction));
    sa.sa_handler = partition_switch_sig_handler;
    sigaction(PARTITION_SWITCH_SIG, &sa, NULL);

    bzero (&sa, sizeof(struct sigaction));
    sa.sa_handler = sigchild_handler;
    sigaction(SIGCHLD, &sa, NULL);

    bzero (&sa, sizeof(struct sigaction));
    sa.sa_handler = trace_level_change_sig_handler;
    sigaction(TRACE_LEVEL_CHANGE_SIG, &sa, NULL);

    bzero (&sa, sizeof(struct sigaction));
    sa.sa_handler = test_post_proc_sig_handler;
    sigaction(TEST_POST_PROC_SIG, &sa, NULL);

    alarm(write_check_time);
    sigfillset (&lr_sigset);
    sigprocmask (SIG_SETMASK, &lr_sigset, NULL);

    //TL
}


static void run_online(ComponentRecoveryData *DBURecoveryData, int testruninfo_shr_id)
{
  int status;
  // read dlog
  translate_dyn_file();

  while(1){
    if(sigterm){
        //Got sigterm from Writer
        do_sigterm ();
      }

      if(trace_level_change_sig) {
        //Got trace level change signal from Writer
        do_trace_level_change();
      }

      if(test_post_proc_sig)
      {
        do_test_post_proc();
        return;
      }
 
      if(sigchild){
        //Got sigterm from Writer
        int pid = (int) waitpid(-1, &status, WNOHANG);
        if(pid == nsu_db_upload_pid)
        {
          if(nslb_recover_component_or_not(DBURecoveryData) == 0)
            start_db_upload_process(test_id, getpid(), partition_name, testruninfo_shr_id);
          else
          {
            NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Max counter of recovery has been expired, hence not starting nsu_db_upload.");
          }
        }
        else if (pid == nsu_db_upload_tmp_table_pid)
          nsu_db_upload_tmp_table_pid = 0;
        sigchild = 0;
      }

      if (sigusr1)  {
        //Ignore sigur1
        sighandler_t prev_handler;
        //sighandler_t prev_handler2;
        prev_handler = signal(SIGUSR1, SIG_IGN);
        //prev_handler2 = signal(SIGTERM, SIG_IGN);
        //resert alram
        alarm(0);
        update_object_table();
        process_dlog_file(); 
        //Reset SIGUSR1
        (void) signal(SIGUSR1, prev_handler); 
        //(void) signal(SIGTERM, prev_handler2); 
        //Start alarm
        sigalarm = 0;
        alarm(write_check_time);
        sigusr1 = 0;
      }

      if (sigalarm)  {
        do_sigalarm();
      }
       
      if (partition_switch_sig) {
        //This partition switch signal is not used any more
        //just logging it
        NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Got parition switch signal.");
        partition_switch_sig = 0;  
      }

      sigemptyset(&lr_sigset);
      sigsuspend(&lr_sigset); //Wait till any signal comes

     time_after_dlog_process = time(NULL);
     if((time_after_dlog_process - time_after_csv_write) >= write_check_time)
       write_rec_in_csv(WRITE_UNCONDITIONALLY);
  } // End of while loop
}

void check_and_kill_prev_reader()
{
  int ret;
  ret = nslb_save_pid_ex(base_dir, "nlr", "nsu_logging_reader");
  if(ret == 1)
  {
    NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, "Prev LR killed, pid saved");
  }
  else if(ret == 0)
  {
    NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, "Prev LR was not running, pid saved");
  }
  else
  {
    NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main thread", NSLB_TL_ERROR, "Error in saving LR pid");
  }
}

int main(int argc, char** argv) {
  int c;
  //char *ptr;
  int testruninfo_shr_id;

  if (argc < 3) {
    NS_EXIT(-1, "Usage: ./logging_reader -l [Test Run Number]  -d [Deliminator character] -i");
  }

  //Mandatory arguments are 
  //TestRun Number, testruninfo_shr key for online mode

  while ((c = getopt(argc, argv, "T:W:l:d:im:t:Z:k:g:S:p:n:G:o:X:")) != -1) {
    switch (c) {
    case 'T': // To enable trace log
      trace_level = atoi(optarg);
      break;

    case 'd':
      deliminator[0] = optarg[0];
      break;
    
    case 'W':
      strcpy(wdir, optarg);
      break;

    case 'i':
      inline_output = 1;
      break;

    case 't':
      write_check_time = atoi(optarg);
      break;
    
    case 'Z':
      DynamicTableSizeiUrl = atoi(optarg);
      break;

    case 'l':
      test_id = atoi(optarg);
      break;

    case 'm':
      running_mode = atoi(optarg);
      break;

    case 'k':
      testruninfo_shr_id = strtoul(optarg, NULL, 10);;
      break;

    case 'g':
      generator_idx = atoi(optarg);
      break;

    case 'S':
      log_shr_buffer_size = atoi(optarg);
      break; 

    case 'n':
      total_nvms = atoi(optarg);
      break;

    case 'G':
      total_generator_entries = atoi(optarg);
      break; 

    case 'o':
      loader_opcode = atoi(optarg);
      break;

    case 'X':
      DynamicTableSizeiTx = atoi(optarg);
      break;

    case '?':
      NS_EXIT(-1, "Usage: ./logging_reader -l [Test Run Number]  -d [Deliminator character] -i");
    }
  }

  //TODO Krishna check for mand args
  init_parition_idx(testruninfo_shr_id);

  //Init for trace log
  lr_trace_log_key = nslb_init_mt_trace_log(wdir, test_id, partition_idx, "ns_logs/nlr_trace.log", trace_level, trace_log_size);

  NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Logging Reader Started: testrun = %d, partition_idx=%lld, partition_name=%s, common_files_dir=%s, PID=%d, trace_level = %d, trace_log_size = %d", 
            test_id, partition_idx, partition_name, common_files_dir, getpid(), trace_level, trace_log_size);

  //if previous logging reader is already running after reader restarts, then first kill it
  NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Checking if previous logging reader is running");
  check_and_kill_prev_reader();
  parse_multidisk_keyword();

  object_norm_init(lr_trace_log_key, total_nvms, total_generator_entries);

  //Loading of URL, Page, Tx, Session normalization table from csv file on start
  NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Build norm tables from csv.");
  build_norm_tables_from_metadata_csv(test_id, common_files_dir);

  open_static_csv_file(); //open static csv's one time

  NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "loader_opcode = %d", loader_opcode);
  if(loader_opcode == 0) {
    char csv_buf[512];
    char buf[512];
    FILE *generator_ptr;

    sprintf(buf, "logs/TR%d/%s/reports/csv/generator_table.csv", test_id, common_files_dir);
    if (!(generator_ptr = fopen(buf, "w"))) {
      perror("logging_reader");
      NS_EXIT(-1, "Error in opening file %s. Test Run Number '%d' is not valid\n", buf, test_id);
    }

    sprintf(csv_buf, "dummy,-1,-1,-1,-1,-1");
    NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "csv_buf = %s", csv_buf);
    fprintf(generator_ptr, "%s\n", csv_buf);
    fclose(generator_ptr);
  }
  /*Load slog starts*/
  allocate_tables(); 

  //Do this only one time
  nslb_init_big_buf(); 

  /*  Here we need to identify if reader is just started or it is recovered.
   *  If reader is recovered, we need to fill csv's that were left during recovery.  
   *  If current partition is first partition and .dlog.offset is not found,
   *  we say that fresh test is started and logging reader should not recover csv .
   *  We recognize first partition if .partition_info file has 0 in place of prev partition.
   *  nslb_get_last_processed_partition() method returns 'IS_START_PARTITION' only if following conditions are true.
   *    1.  .dlog.offset
   *    2.  first partition is reached.
 */

  int ret_val = 0;

  NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "base_dir is %s\n", base_dir);  //TODO Krishna check
  //put current partition name in structure req by lib func
  DataFileInfo *dfi = NULL;

  dfi = (DataFileInfo *)malloc(sizeof(DataFileInfo));
  memset(dfi, 0, (sizeof(DataFileInfo)));
  strcpy(dfi->partition, partition_name);
  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "dfi->partition is %s\n", dfi->partition);

  if (partition_idx > 0)
  {
    ret_val = nslb_get_last_processed_partition(base_dir, "reports/raw_data", "dlog", dfi);
    if(ret_val < 0)  //error
    {
      NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_ERROR, "Error in getting last processed partition. Error =  %s\n", dfi->err_msg);
      NS_EXIT (-1, "Error in getting last processed partition. Error =  %s", dfi->err_msg);
    }
    strcpy(partition_name, dfi->partition); //save obtained partition name in local
    partition_idx = atoll(partition_name);

    NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Last Processed partition is %s and err_msg = %s", partition_name, dfi->err_msg);

    NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Checking if slog exists or not. partition is = %s", partition_name);
    if (check_slog_exists() == 0)
    {
      NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Not found slog in any partition, current partition is = %s", partition_name); 
      NS_EXIT(-1, "Not found slog in any partition, current partition is = %s", partition_name);
    }
  }

  if (db_create(&data_hash_table, NULL, 0)) {
    NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Failed in createing data hash table.");
    return (FAILURE);
  }
  
  if (data_hash_table->open(data_hash_table, NULL, NULL, NULL, DB_HASH, DB_CREATE, 0)) {
    NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Failed in opening data hash table.");
    return (FAILURE);
  }

  if (db_create(&msg_hash_table, NULL, 0)) {
    NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Failed in ng data hash table.");
    return (FAILURE);
  }

  if (msg_hash_table->open(msg_hash_table, NULL, NULL, NULL, DB_HASH, DB_CREATE, 0)) {
    NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Failed in opening msg hash table.");
    return (FAILURE);
  }

  //open dlog, seek to the correct offset and create all dynamic csv files
  //open dlog first before reading slog, so that if dlog is not present we don't consume slog
  open_dynamic_csv_file();
  chk_and_reload_slog();

  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Before output_static_file() called: test_id = %d", test_id);
  output_static_file(); 



  lw_pid = getppid();
  
  init_pg_dump_data(); 
  //Added sleep for 1 sec
  //Befiore adding sleep reader was not able to read dlog file
  //if running for 1 session without debug
  //sleep(1); TODO test this case by Prachi

  //If running mode is 1 
  //set signals
  if(running_mode)
  {

    NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Logging Reader is running in online mode");
    //Create process 
    /*Bug 9415
      NC: In order to process complete dlog, NIFA needs to run logging reader in online mode. But db_upload process 
      when spawn online requires db tables which are created by NS parent whereas in offline mode table are created by nsu_import. 
      Case: NC offline test (partition/non-partition) will spawn db_upload in post processing 
      (Logging reader -> Online, db_upload -> Offline) hence testruninfo_tbl_shr->reader_run_mode will be 0*/
    if (testruninfo_tbl_shr->reader_run_mode)
      start_db_upload_process(test_id, getpid(), partition_name, testruninfo_shr_id); 

    setup_signal();

  /*  We need to avoid a situation when a component is recovered again and again,
   *  if component continues to die because of some reason.
   *  Hence we have a counter and a threshold timer;
   *  Component can be recovered limited number of times.
   *  If component doesn't die again within threshold time, then the counter is reset to 0.
   */
    ComponentRecoveryData DBURecoveryData;  
    nslb_init_component_recovery_data(&DBURecoveryData, MAX_RECOVERY_COUNT, RECOVERY_TIME_THRESHOLD_IN_SEC, RESTART_TIME_THRESHOLD_IN_SEC); 
    /*Run dynamic file creation in loop
     * logging writer will send signal every time when 
     * writer write into dlog file*/
    run_online(&DBURecoveryData, testruninfo_shr_id);
  } // End of running mode
  else
  {
    NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Logging Reader is running in offline mode");
    run_offline();
  }
  
  // TODO - make sure all buffer is saves in CSV files
  //write_eof_in_files();
  fcloseall();
  return 0;
}


void sighup_handler(int sig) {
  sigusr1 = 1;
}

void sigalarm_handler (int sig) {
    sigalarm = 1;
}

void sigchild_handler (int sig) {
    sigchild = 1;
}

void sigterm_handler(int sig) {
    sigterm = 1;
}

void partition_switch_sig_handler(int sig) {
  partition_switch_sig = 1;
}

void trace_level_change_sig_handler(int sig) {
  trace_level_change_sig = 1;
}

void test_post_proc_sig_handler(int sig)
{
  test_post_proc_sig = 1;
}


//Function to check if Netstorm is running
//If not then exit
void do_sigalarm() {
  int alarm_time;
  long start_time = time(NULL);

  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called, write_check_time = %d", write_check_time);
  //Partition switch signal may be lost,
  //hence after reading dlog, if read amount is 0, we need to check if partition is switched. 
  process_dlog_file(); 
  write_rec_in_csv(WRITE_UNCONDITIONALLY);

  //Check if parrent is alive, if not exit
  if ((kill (lw_pid, 0) == -1) && ((errno == ESRCH) || (errno == EPERM))) {
    NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_ERROR, "Logging reader: Logging writer process got killed. Exiting ...");
    exit(1);
  }

  //If alarm_time is -ve then override it with default alarm time i.e. write_check_time
  alarm_time = write_check_time - (time(NULL) - start_time);
  if(alarm_time <= 0)
    alarm_time = write_check_time;

  sigalarm = 0;
  alarm(alarm_time);
}

void do_trace_level_change () {
  nslb_change_atb_mt_trace_log(lr_trace_log_key, "NLR_TRACE_LEVEL"); 
  trace_level = lr_trace_log_key->log_level;
  NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, "Trace Level changed to %d", trace_level); 
  trace_level_change_sig = 0;
}

void do_sigterm () {
  NSLB_TRACE_LOG2(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Method called");
  //Go and check if any thing needs to read
  
  //Read dlog and flush all data to csv and close files
  read_and_close_dlog();

  NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Logging reader going to exit.");

  //write_eof_in_files();
  sigterm = 0;
  NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Logging reader going to exit.");
  exit(0);
}


void do_test_post_proc()
{
  int status;
  NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, "Logging Reader: Recieved signal TEST_POST_PROC_SIG.");
  //Writer has already processed, so run as offline mode.
  run_offline();

  if(running_mode && (nsu_db_upload_pid > 0))
  {
    if (kill(nsu_db_upload_pid, TEST_POST_PROC_SIG) == -1)
    {
      NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main thread", NSLB_TL_ERROR, "Error in sending signal TEST_POST_PROC_SIG to logging reader");
    }

    //Wait for logging reader pid. Logging writer will exit only when logging reader exits.
    NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, "Logging Reader: Waiting for NSU DBUpload to exit.");
    //fprintf(stdout, "Logging Reader: Waiting for NSU DBUpload to exit.\n");

    //TODO Error handling if required
    waitpid(nsu_db_upload_pid, &status, 0);
    //fprintf(stdout, "Logging Reader: NSU DBUpload exited. Going to exit\n");
    NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, "Logging Reader: NSU DBUpload exited. Going to exit");
  }
  NSLB_TRACE_LOG1(lr_trace_log_key, partition_idx, "Main", NSLB_TL_INFO, "Do test data process successfull.");
  exit(0);
}
  
