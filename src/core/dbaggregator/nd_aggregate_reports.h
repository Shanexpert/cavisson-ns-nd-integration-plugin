//typedef  NVResultSet* (*NVParserCallback)(NVResultSet *);
#ifndef NR_UPDATE_AGGREGATE_H
#define NR_UPDATE_AGGREGATE_H

//typedef char* NVColumn;
#include <pthread.h>
#include <libpq-fe.h>
#include <sys/types.h>
#include <assert.h>
//data type.
#define NV_STR 0
#define NV_NUMBER 1
#define NV_FLOAT 2
typedef struct NVColumn {
  short type;
  short free; //This flag is to tell if need to free the point in case of NV_STR.
  union {
    char *str;
    long num;
    double float_num; 
  }value;
}NVColumn;

typedef struct NVResultSet{
  int curIdx;
  int numRow;
  int numColumn;
  int maxTuples; //value for maximum column entry alloted. Just to handle realloc.
  NVColumn *column;
  int deltaTuples; //delta entries to realloc. 
}NVResultSet;

typedef struct AggOfflineInfo {
  char parserName[256];
  char cronString[256];
}AggOfflineInfo;

typedef struct SchedularProfile {
  short taskid;
  char cronstring[32];
  char taskname[256];
  unsigned long expirytime;
  char schedule[256];
  char status[256];
  char disable[256];
  char command[526]; //ff1
  char type[256];    //ff2
  int mode;          //ff3 
}SchedularProfile;

#define MY_MALLOC(new, size, cptr, msg) {                               \
    if (size < 0) {                                                     \
      assert(size >= 0);\
      ND_ERROR("Trying to malloc a negative size (%d)", size); \
      exit(1);                                                          \
    } else if (size == 0) {                                             \
      ND_ERROR("Trying to malloc a 0 size"); \
      new = NULL;                                                       \
    } else {                                                            \
      new = (void *)malloc( size );                                     \
      if ( new == (void*) 0 ) {                                         \
        ND_ERROR("Out of Memmory: %s", msg); \
        exit(1);                                                        \
      }                                                                 \
      ND_LOG4("MY_MALLOC'ed (%s) done. ptr = %p, size = %d", msg, new, size); \
    }                                                                   \
  }

#define MY_REALLOC(buf, size, cptr, msg) {                              \
    if (size <= 0) {                                                    \
      assert(size >= 0);\
      ND_ERROR("Trying to malloc a negative or 0 size (%d)", size); \
      exit(1);                                                          \
    } else {                                                            \
      buf = (void*)realloc(buf, size);                                  \
      if ( buf == (void*) 0 ) {                                         \
        ND_ERROR("Out of Memmory: %s", msg); \
        exit(1);                                                        \
      }                                                                 \
      ND_LOG4("MY_REALLOC'ed (%s) done. ptr = %p, size = %d", msg, buf, size); \
    }                                                                   \
  }

#define FREE_AND_MAKE_NULL(to_free, cptr, msg) {                        \
    if (to_free) {                                                      \
      ND_LOG4("MY_FREE'ed (%s) Freeing ptr = %p", msg, to_free); \
      free(to_free);                                    \
    }                                                                   \
  }

#ifdef NV_CALLBACK
typedef struct {
  long long partition_idx; 
  char partition_name[100 + 1];
  int tr_number;
  long absolute_time; //switched time stamp
  long cav_epoch_diff;
  int ns_parent_pid;
  int ns_lw_pid;
  int ns_lr_pid;
  int nia_file_aggregator_id;
  int test_status; //init, schedule, post processing, over
  int num_nvm;
  int ns_port; 
  int big_buff_shm_id;
  int ns_log_mgr_port;
  int reader_run_mode; //offline(0), online(1)
  long long ns_db_upload_chunk_size;
  long long nd_db_upload_chunk_size;
  int db_upload_idle_time_in_secs;
  int db_upload_num_cycles;
  char nlm_trace_level;
  char nlr_trace_level;
  char nlw_trace_level;
  char nsdbu_trace_level;
  char nifa_trace_level;
  char loader_opcode;
  char nsdbuTmpFilePath[1024];
  char nddbuTmpFilePath[1024];
}TestRunInfoTable_Shr;

typedef struct MTTraceLogKey {
  char ns_wdir[256];
  int testidx;

  unsigned int log_level;
  int max_log_file_size;

  int file_fd;
  char log_file[1024];

  char partition_name[16];
  long long partition_idx;
  long long cur_partition_idx;

  char print_header;
  pthread_mutex_t trace_mutex;
  uid_t ownerid;
  gid_t grpid;
  char base_dir[512];
} MTTraceLogKey;

#define NSLB_TL__FLN_  __FILE__, __LINE__, (char *)__FUNCTION__
#define NSLB_TRACE_LOG1(trace_log_key, partition_name, thread_name, severity, ...) \
                       if(trace_log_key && (trace_log_key->log_level & 0x000000FF)) \
                         nslb_mt_trace_log(trace_log_key, partition_name,  thread_name, severity, NSLB_TL__FLN_, __VA_ARGS__);

#define NSLB_TRACE_LOG2(trace_log_key, partition_name, thread_name, severity, ...) \
                       if(trace_log_key && (trace_log_key->log_level & 0x0000FF00)) \
                         nslb_mt_trace_log(trace_log_key, partition_name,  thread_name, severity, NSLB_TL__FLN_, __VA_ARGS__);

#define NSLB_TRACE_LOG3(trace_log_key, partition_name, thread_name, severity, ...) \
                       if(trace_log_key && (trace_log_key->log_level & 0x00FF0000)) \
                         nslb_mt_trace_log(trace_log_key, partition_name,  thread_name, severity, NSLB_TL__FLN_, __VA_ARGS__);

#define NSLB_TRACE_LOG4(trace_log_key, partition_name, thread_name, severity, ...) \
                       if(trace_log_key && (trace_log_key->log_level & 0xFF000000)) \
                         nslb_mt_trace_log(trace_log_key, partition_name,  thread_name, severity, NSLB_TL__FLN_, __VA_ARGS__);

//Extern normalzize table related apis.
//../../libnscore/nslb_big_buf.h
typedef struct bigbuf_t
{
  char *buffer;
  long offset;
  long bufsize;
} bigbuf_t;
int nslb_bigbuf_init(bigbuf_t *bigbuf);
long nslb_bigbuf_copy_into_bigbuf(bigbuf_t *bigbuf, char* string, int len);
void nslb_bigbuf_free(bigbuf_t *bigbuf);
char* nslb_bigbuf_get_value(bigbuf_t *bigbuf, long offset);
/* this macro can also be used to get value from bigbuf */
#define NSLB_BIGBUF_GET_VALUE(bigbuf, offset) (char *)(bigbuf->buffer + offset)

//../../libnscore/nslb_get_norm_obj_id.h
typedef struct NormTable{
  long str; /* Offset into bigbuf */
  int len; 
  int idx;    //normalized index
  //int hash_value;  //hash value
  struct NormTable *next;
} NormTable;

typedef struct NormObjKey{
  bigbuf_t bigbuf;
  int size;        //size of normalized table
  int prime;        //size of normalized table
  NormTable *nt;   
  int prev_id;    //previous index
} NormObjKey;  

extern int nslb_obj_hash_destroy(NormObjKey *key);
extern int nslb_init_norm_id_table(NormObjKey *key, int Size);
//unsigned int nslb_get_or_gen_norm_id(NormObjKey *key, char *in_str, int in_strlen);
extern unsigned int nslb_set_norm_id(NormObjKey *key, char *in_str, int in_strlen, int norm_id);
extern unsigned int nslb_get_or_gen_norm_id(NormObjKey *key, char *in_str, int in_strlen, char *flag_new);
extern unsigned int nslb_set_norm_id(NormObjKey *key, char *in_str, int in_strlen, int normid);
extern void nslb_set_start_norm_id(NormObjKey *key, int start_normid);
extern int nslb_pg_bulkload(char *tablename, char *tmp_file, char *pg_bulkload_log, char delimiter);
#endif
extern int g_nv_filter_outlier;
extern int g_nv_page_load_time_outlier;
extern int g_nv_dom_content_load_time_outlier;
extern int g_nv_dom_time_outlier;


extern MTTraceLogKey *g_trace_log_key;
extern char g_process_name[512];
extern TestRunInfoTable_Shr *test_run_info_shm_ptr;
//In case if we unable to attach test_run_info_shm_ptr then get the ccurrent partition. we can not do any thing.
extern long g_cur_partition;
extern char g_client_id[512];
extern int g_progress_interval;
extern char g_tmpfs_path[512];
//extern char docrootPrefix[1024] = "";

#define CURRENT_PATITION (test_run_info_shm_ptr != NULL?test_run_info_shm_ptr->partition_idx:g_cur_partition)

#define ND_LOG(...) NSLB_TRACE_LOG1(g_trace_log_key, CURRENT_PATITION, g_process_name, "debug", __VA_ARGS__)
#define ND_LOG1(...) NSLB_TRACE_LOG1(g_trace_log_key, CURRENT_PATITION, g_process_name, "debug", __VA_ARGS__)
#define ND_LOG2(...) NSLB_TRACE_LOG2(g_trace_log_key, CURRENT_PATITION, g_process_name, "debug", __VA_ARGS__)
#define ND_LOG3(...) NSLB_TRACE_LOG3(g_trace_log_key, CURRENT_PATITION, g_process_name, "debug", __VA_ARGS__)
#define ND_LOG4(...) NSLB_TRACE_LOG4(g_trace_log_key, CURRENT_PATITION, g_process_name, "debug", __VA_ARGS__)

#define ND_ERROR(...) NSLB_TRACE_LOG1(g_trace_log_key, CURRENT_PATITION, g_process_name, "error", __VA_ARGS__)

#define NV_MIN(a, b) ((a<b)?a:b)
#define NV_MAX(a, b) ((a>b)?a:b)

extern NVColumn *nvGetValue(NVResultSet *nvrs, int row, int column);
extern inline void free_nv_result_set(NVResultSet *nvrs);
extern void gen_nonce(const char *buf, int size);
extern void to64frombits(unsigned char *nonce, const char *buf, int size);
extern int nv_allocate_table_entry(int *total_alloted, int used, int required, void **entry_ptr, int entry_size, int delta_entries);
extern NVResultSet *getNVResultSet(int numColumn, int deltaRow);
extern int nvAddRow(NVResultSet *nvrs, NVColumn *columns);
extern int compareNVColumn(NVColumn *column1, NVColumn *column2);
extern inline void setNVColumnNumber(NVColumn *column, long value);
extern inline void setNVColumnFloat(NVColumn *column, double value);
extern inline void setNVColumnString(NVColumn *column, char *value, char free);
extern int execute_db_query(char *query, PGresult **res);
extern int execute_db_query_ex(char *query, PGresult **res, PGconn *connection, int *db_con_pid);

#ifndef NV_CALLBACK

#define MAX_LINE_LENGTH 8*1024
#define MAX_AGG_TABLE 8
#define MAX_SQL_QUERY_SIZE 32*1024

//TODO: parse this.
extern int g_trace_level;

typedef NVResultSet* (NVParserCallback)(NVResultSet *, int *, long);
//Note: assumption is that both the resultset will be of same length.
//Syntax: NVResultSet *sample_merge_agg_records(NVResultSet *res1, NVResultSet *res2, int numResultSet, int *totalOutResultSet);
//Two resultset will be passed of same length and will return new resultset with length returned by totalOutResultSet.
typedef NVResultSet* (NVMergeAggCallback)(NVResultSet*, NVResultSet *, int);

//Note: this callback will be used to parser profile specific keywords. 
//Note: It will return 0 - for success and -1 for failure. First arg is conf file and second arg is error string.
//In case of error callback will fill the error string.
typedef int (NVParseKeywordCallback)(char *, char *);
typedef struct aggregate_profile
{
  char aprofname[512];
  void *so_handle;
  char table_name[MAX_AGG_TABLE][100]; //Maximum 32 tables.
  int numAggTable;
  char create_table_sql[MAX_AGG_TABLE][MAX_SQL_QUERY_SIZE]; //each table will have seperate create table statement.
  char data_collection_sql[MAX_SQL_QUERY_SIZE];
  //This query will check for last record timestamp in db.
  char record_timestamp_sql[MAX_SQL_QUERY_SIZE];
  unsigned long timestamp_column;
  int progress_interval;
  int max_session_duration;
  int process_id;  
  int trno;
  int test_state;  //0 completed , 1 active
  int retrycount; 
  int process_old_data;
  char gcc_args[1024]; 
  NVParserCallback *parser_callback;
  //Note: there will be a default merge_agg_callback in case if not given in parser file.
  NVMergeAggCallback *merge_agg_callback;
  NVParseKeywordCallback *parse_keywords_callback;
  char parser_callback_src[512];
  int numPrimaryColumn;
  int primaryColumnIdx[32];
  ComponentRecoveryData recovery; 
  //Note: data source can be a remote db server. in that case we need these parameters.
  //remote server connection string.
  char src_db_server_conn_string[512];
  char src_client_id[512];
  int totalTableSpace;
}aggregate_profile ;    
 
typedef struct NVActiveAggBucket{
  NVResultSet *aggResultSet;
  int numAggResultSet;
  unsigned long partitionid; 
  time_t bucketid;
  NormObjKey normalizeTable;
  NVResultSet nvres; 
  NVResultSet **pendingNvres; //this will be used while parsing data from different bucket. Will be length of totalActiveBucket.
  int pendingNvresCount;
  int retryCount;
}NVActiveAggBucket;

extern int reconnect_to_db();
extern int reconnect_to_db_ex(PGconn *connection,  int *db_con_pid);
extern NVResultSet* dbRSToNVRS(PGresult *dbRS);
extern PGconn *db_connection;
extern int g_db_pid;
extern int g_remote_db_pid;
extern int sigterm_flag;
#endif

#endif
