#ifndef NS_SQL_VAR_C 
#define NS_SQL_VAR_C 

typedef struct SQLVarTableEntry
{
  ns_bigbuf_t host;
  ns_bigbuf_t port; 
  ns_bigbuf_t driver;
  ns_bigbuf_t user;
  ns_bigbuf_t passwd;
  ns_bigbuf_t db_name;
  ns_bigbuf_t query;
  ns_bigbuf_t is_save_into_file;
  char* query_result;
  long int query_result_size;
  long int query_result_buff_size;
}SQLVarTableEntry;

//SQL odbc Connection Attribute
typedef struct SQLConAttr
{
  char dsn[16];
  char host[16];
  char port[16];
  char user[16];
  char password[16];
  char database[16];
}SQLConAttr;

typedef struct
{
  void *env;
  void *dbc;
}DataBase_T;

#define NS_SQL_PARAM_MAX_QUERY_LEN  10 * 1024 
#define NS_SQL_PARAM_MAX_QUERY_OUTPUT_LEN 10 * 1024 
#define NS_SQL_PARAM_MAX_DB_DIAG_MSG_LEN 2048
#define NS_SQL_PARAM_MAX_COL_SIZE 10 * 1024 * 1024
#define NS_SQL_PARAM_DELTA_COL_SIZE 1024 * 1024 

//sql_args_done_flag
#define SQL_PARAM_HOST_PARSING_DONE                          0x0001 
#define SQL_PARAM_PORT_PARSING_DONE                          0x0002 
#define SQL_PARAM_DRIVER_PARSING_DONE                        0x0004 
#define SQL_PARAM_USER_PARSING_DONE                          0x0008 
#define SQL_PARAM_PASSWD_PARSING_DONE                        0x0010 
#define SQL_PARAM_DATABASE_PARSING_DONE                      0x0020 
#define SQL_PARAM_QUERY_PARSING_DONE                         0x0040 
#define SQL_PARAM_SAVEINTOFILE_PARSING_DONE                  0x0080

extern int parse_sql_params(char* keyword, char* value, SQLVarTableEntry* sqlVarTable, int* state, char* script_file_msg, char* host, char* port, short int *done, char* staticvar_buf, int* group_idx, int* var_start, int sess_idx, int* rw_data, int* create_group, int* var_idx, int* column_flag, unsigned short* reltv_var_idx);

extern int ns_sql_param_validate_keywords(short int* sql_args_done_flag, char* script_file_msg);
extern int ns_sql_param_validate_host_and_port(char* host, char* port, char* script_file_msg, SQLVarTableEntry* sqlVarTable);
extern int ns_sql_get_data_from_db(SQLVarTableEntry* sqlVarTable, SQLConAttr* sqlAttr, char* column_delimiter, char *err_msg);
extern int set_ns_sql_odbc_con_attr(SQLVarTableEntry* sqlVarTable, SQLConAttr* sqlAttr);
extern int ns_sql_save_query_result_into_file(SQLVarTableEntry* sqlVarTable, char* file_name, GroupTableEntry* groupTable, int group_idx);
 
#endif
