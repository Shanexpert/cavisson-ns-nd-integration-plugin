#define REPLAY_DB_QUERY_USING_FILE             11
#define REPLAY_DB_QUERY_USING_ACCESS_LOG       12

extern int total_db_query_entries;
extern int max_db_query_entries;
extern int cum_query_pct;
extern unsigned int sp_handle;

#define MAX_DB_QUERY_FILE_ARGS                 20
#define DELTA_DB_QUERY_ENTRIES                 64
#define MAX_PARAM_NAME_SIZE                    64
#define MAX_POOL_TRANS_NAME_SIZE               128
#define REPLAY_DB_QUERY_NUM_PARAMETERS         256
#define MAX_BUFF_SIZE                          2048

typedef struct QueryParameters{
  char param_name[MAX_PARAM_NAME_SIZE + 1];
  char advance_param_flag;
  int param_type;
} QueryParameters;

typedef struct NsDbQuery{
  char pool_name[MAX_POOL_TRANS_NAME_SIZE + 1];
  char query[MAX_BUFF_SIZE + 1];
  char trans_name[MAX_POOL_TRANS_NAME_SIZE + 1];
  QueryParameters query_parameters[REPLAY_DB_QUERY_NUM_PARAMETERS];
  short query_type;
  int query_pct;
  int num_parameters;
} NsDbQuery; 
extern NsDbQuery *ns_db_query;

extern int *db_shr_ptr;

typedef struct {
  char pool_name[MAX_POOL_TRANS_NAME_SIZE + 1];
  char query[MAX_BUFF_SIZE + 1];
  char trans_name[MAX_POOL_TRANS_NAME_SIZE + 1];
  QueryParameters query_parameters[REPLAY_DB_QUERY_NUM_PARAMETERS];
  int num_parameters;
} NsDbQuery_Shr;

extern NsDbQuery_Shr *ns_db_query_shr;

extern void copy_query_table_into_shr_memory();
extern int ns_get_query_index();
extern void parse_query_file(char *replay_file_dir);
extern void set_advance_param_flag_in_db_replay();
