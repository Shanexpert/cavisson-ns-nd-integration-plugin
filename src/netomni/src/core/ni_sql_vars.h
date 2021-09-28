#ifndef NI_SQL_VAR_C 
#define NI_SQL_VAR_C

typedef struct NI_SQLVarTableEntry
{
  char host[64];
  char port[64];
  char driver[64];
  char user[64];
  char passwd[64];
  char db_name[64];
  char *query;
  char is_save_into_file[16];
  char* query_result;
  long int query_result_size;
  long int query_result_buff_size;
}NI_SQLVarTableEntry;

typedef struct NI_SQLConAttr
{
  char dsn[16];
  char host[16];
  char port[16];
  char user[16];
  char password[16];
  char database[16];
}NI_SQLConAttr;

typedef struct
{
  void *env;
  void *dbc;
}NI_DataBase_T;


#define NI_SQL_PARAM_MAX_QUERY_OUTPUT_LEN            10 * 1024
#define NI_SQL_PARAM_MAX_DB_DIAG_MSG_LEN             2048
#define NI_SQL_PARAM_DELTA_COL_SIZE                  1024 * 1024 
#define NI_SQL_PARAM_MAX_COL_SIZE                    10 * 1024 * 1024

#define NI_SQL_PARAM_HOST_PARSING_DONE               0x0001
#define NI_SQL_PARAM_PORT_PARSING_DONE               0x0002
#define NI_SQL_PARAM_DRIVER_PARSING_DONE             0x0004 
#define NI_SQL_PARAM_USER_PARSING_DONE               0x0008
#define NI_SQL_PARAM_PASSWD_PARSING_DONE             0x0010
#define NI_SQL_PARAM_DATABASE_PARSING_DONE           0x0020
#define NI_SQL_PARAM_QUERY_PARSING_DONE              0x0040
#define NI_SQL_PARAM_SAVEINTOFILE_PARSING_DONE       0x0080 


extern int ni_parse_sql_params(char* line_tok, char* value, NI_SQLVarTableEntry* sqlVarTable, int* state, char* script_file_msg, char* host,   char* port, short int* sql_args_done_flag, int* api_idx, int* create_group);
extern int ni_sql_param_validate_keywords(short int* sql_args_done_flag, char* script_file_msg);
extern int ni_sql_param_validate_host_and_port(char* host, char* port, char* script_file_msg, NI_SQLVarTableEntry* sqlVarTable);
extern int ni_set_sql_odbc_con_attr(NI_SQLVarTableEntry* sqlVarTable, NI_SQLConAttr* sqlAttr);
extern int ni_sql_get_data_from_db(NI_SQLVarTableEntry* sqlVarTable, NI_SQLConAttr* sqlAttr, char* column_delimiter, char *err_msg);

#endif
