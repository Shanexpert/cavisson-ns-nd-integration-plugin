#ifndef SCRIPT_PARSE_H
#define SCRIPT_PARSE_H


#include "ni_sql_vars.h"

/* Different modes of script*/
#define SCRIPT_TYPE_LEGACY 0
#define SCRIPT_TYPE_C      1 
#define SCRIPT_TYPE_JAVA   2 

//Script parsing
#define INIT_SCRIPT_ENTRIES 128
#define DELTA_SCRIPT_ENTRIES 128
//API table
#define INIT_API_ENTRIES    128
#define DELTA_API_ENTRIES   128
//Offset array per API
#define INIT_OFFSET_ENTRIES 100
#define DELTA_OFFSET_ENTRIES 10
//unique_range_var api 
#define DELTA_UNIQUE_RANGE_ENTRIES 32
#define INIT_UNIQUE_RANGE_ENTRIES 64

#define MAXIMUM_SCRIPT_FILE_NAME_LEN 1152  // 1024 + 128

#define REGISTRATION_FILENAME "registrations.spec"
#define MAX_LINE_LENGTH 35000

/*Parameter type*/
#define STATIC_VAR_NEW             1
#define UNIQUE_RANGE_VAR_NEW       2
#define NI_SQL_VAR                 3
#define NI_INDEX_VAR               4
#define FINISH_LINE                9

/* States at time of parsing APIs nsl_sql_var(), nsl_static_var()*/ 
#define NI_ST_STAT_VAR_NAME 1
#define NI_ST_STAT_VAR_OPTIONS 2
#define NI_ST_STAT_VAR_NAME_OR_OPTIONS 3
 
#define MAX_COLUMNS 200
#define MAX_FIELD_LENGTH 10000

/*Sequence*/
#define SEQUENTIAL     1
#define RANDOM         2
#define WEIGHTED       3
#define UNIQUE         4
#define USEONCE        5

/* Refresh Modes */
#define SESSION_NEW 1
#define USE_NEW 2
      
/* Action Modes */
#define SESSION_FAILURE_NEW 0
#define CONTINUE_WITH_LAST_VALUE_NEW 1
#define REPEAT_THE_RANGE_FROM_START_NEW 2
#define UNIQUE_RANGE_STOP_TEST_NEW 3

#define NS_ST_SE_NAME_NEW      1
#define NS_ST_SE_OPTIONS_NEW   2

#define UNQRANG_MAX_LEN_512B          512
#define UNQRANG_MAX_LEN_1K            1024

/* Var Value filed*/
#define IS_FILE 1

typedef struct OffSetTableEntry {
  char *offset_end_ptr_array;//Holds end pointer array per offsets for data file
  int line_num;
} OffSetTableEntry;

typedef struct APITableEntry {
  char column_delimiter[16];
  char ignore_invalid_line; //NO ->0 , Yes -> 1
  short type;
  short sequence;
  unsigned int num_values;
  int num_vars;
  short is_file;             /* If is file type == 1 otherwise 0 */
  int idx;
  int script_idx;
  int start_var_idx;
  unsigned int first_data_line;  // From where the data line will start
  int max_column_index;//Taking this variable so as to set the maximum column index in parsing and ignoring that line while filling in pointer table. 
  int abs_or_relative_data_file_path;
  long data_file_size;
  char *data_fname;
  char *data_file_path;
  char *data_file_buf;
  char *end_head_ptr;//Holds header end pointer 
  OffSetTableEntry *offset_table_entry;
  int total_offset;
  int UseOnceAbort;
  int rtc_flag;
  int is_sql_var;
  char UseOnceWithinTest[10];
  char *UsedFilePath;
  int data_dir_idx;
} APITableEntry;

typedef struct ScriptTable {
  char* script_name;
  char* unique_var_line;
  int sgroup_num;
  int pct_or_users;
  int script_total; //sum of pct_or_users having same script
  int num_unique_range_var_entries;
  int unique_range_var_start_idx;
  char* proj_name;
  char* sub_proj_name;
} ScriptTable;

typedef struct PerGenAPITable {
  int start_val;
  int num_val;
  int total_val;
  int num_script_users; //sum of pct_or_users having same script
  int used_val;
} PerGenAPITable;

//unique_range_var table
typedef struct UniqueRangeTableEntry {
  char name[512]; /* index into the big buffer */
  char format[512]; /* Format of the variable */
  int format_len;
  int action; /* Action taken on rage exhaustance */
  int refresh;
  int sess_idx;
  unsigned long start; /* Starting number */
  unsigned long block_size ; /* Block size per Vuser */
} UniqueRangeTableEntry;

extern ScriptTable* script_table;
extern APITableEntry* api_table;
extern PerGenAPITable* per_gen_api_table;
extern PerGenAPITable* per_gen_api_table_unique_var;
extern UniqueRangeTableEntry* uniquerangeTable;

extern int max_api_entries;
extern int total_api_entries;
extern int max_script_entries;
extern int total_script_entries;
//unique range var entries
extern int total_unique_range_entries;
extern int max_unique_range_entries;
extern int create_script_table(char* script_name, char *err_msg);
extern int ni_parse_registration_file(char *script_path, int script_id);
extern int ni_divide_values_per_generator(int runtime); 
extern int find_script_idx(char* name);

extern int ni_input_static_values(char* file_name, int api_idx, int script_idx, int runtime, char *data_file_buf_rtc, int is_sql_var, NI_SQLVarTableEntry* sqlVarTable, char *err_msg_buf);
extern int ni_divide_values_unique_var_per_generator(int runtime);
extern void create_ctrl_files();
extern int ni_get_script_type(char *path, int *script_type, char *version);
extern int create_api_table_entry(int *row_num);
extern int ni_set_last_option(int *idx, int *state, int *need_to_create, char* script_file_msg, char* line_tok);

#endif
