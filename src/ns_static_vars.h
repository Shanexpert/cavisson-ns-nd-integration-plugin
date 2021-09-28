#ifndef NS_STATIC_VAR_H
#define NS_STATIC_VAR_H

/* API Parsing macros */
#define NS_ST_STAT_VAR_NAME                          1
#define NS_ST_STAT_VAR_OPTIONS                       2
#define NS_ST_STAT_VAR_NAME_OR_OPTIONS               3

/* Encode type for file and index parameters */
#define ENCODE_ALL       0 // Encode all special chars except . - _ +
#define ENCODE_NONE      1 // Do not encode any chars
#define ENCODE_SPECIFIED 2 // Encode specified chars only

/*On USEONCE error status code*/
#define ABORT_USEONCE_PAGE       0
#define ABORT_USEONCE_SESSION    1
#define ABORT_USEONCE_TEST       2
#define ABORT_USEONCE_USER       3

#define TOTAL_CHARS 128
#define MAX_COLUMNS 200

/*VAR_VALUE attribute*/
#define IS_FILE 1
#define IS_FILE_PARAM 2

#define IS_SQL_MODE 1
#define IS_CMD_MODE 2

typedef struct WeightTableEntry {
  unsigned short value_weight;
} WeightTableEntry;

typedef struct PointerTableEntry { /*Pointer for Values*/
  union{
    ns_bigbuf_t big_buf_pointer; /* offset into the big buf */
    u_ns_ptr_t seg_start;
  };
  union{
    int size;
    int num_entries;
  };
} PointerTableEntry;

typedef struct VarTableEntry {
  unsigned int group_idx; /* index into the group Table */
  unsigned int value_start_ptr; /* index into the pointer table */
  ns_bigbuf_t name_pointer; /* big buf offset */
  short is_file;             /* If is file type == 1 otherwise 0 */
  unsigned int server_base;  /*index into the pointer table and can be -1*/
  short var_type;
  short var_size; //Assumed size will be less than 32K. Defulat -1, > means writable with size specified.
		  // = 0 means writable with no size limitation. 0 not supported yet.
  int column_idx;//Index to the column in file whose value we want in file parameter var
  unsigned short self_idx_wrt_fparam_grp; //Relative variable index w.r.t file parameter group 
  int encrypted; //flag to indicate value is encrypted or not 
} VarTableEntry;


typedef int (*Str_to_hash_code)(const char*, unsigned int);
typedef const char *(*Hash_code_to_str)(unsigned int);

//HoldsStatic variable groups
typedef struct GroupTableEntry {
  short type;
  short sequence;
  unsigned int weight_idx;  /* entry into the weight table */
  unsigned int num_values;
  int num_vars;
  int idx;
  int sess_idx;
  //int num_users;
  int start_var_idx;
  ns_bigbuf_t data_fname; //Offset in big buf table
  ns_bigbuf_t data_dir; //Offset in big buf table
  ns_bigbuf_t column_delimiter; //Offset in big buf table
  unsigned int first_data_line;  // From where the data line will start

  ns_bigbuf_t index_var;
  Str_to_hash_code idx_var_hash_func;

  // For encoding
  char encode_type;   // Encoding type. See util.h
  ns_bigbuf_t encode_space_by; // Offset in big buf table
  char encode_chars[TOTAL_CHARS]; // Offset in big buf table
  int UseOnceOptiontype;
  int UseOnceAbort;
  //unsigned int (*idx_var_hash_func)(const char*, unsigned int); // for index var
  char copy_file_to_TR_flag; //NO -> -1, YES -> 1
  int max_column_index;//Taking this variable so as to set the maximum column index in parsing and ignoring that line while filling in pointer table. 
  char ignore_invalid_line; //NO ->0 , Yes -> 1
  char absolute_path_flag;  //To handle nc case save last data and control file.
  int sql_or_cmd_var;
  ns_bigbuf_t UseOnceWithinTest;
  int is_save_into_file_flag;
  int persist_flag;
} GroupTableEntry;

extern int input_staticvar_data(char* line, int line_number, int sess_idx, char *script_filename, int api_type); 
void copy_staticvar_into_shared_mem();


/**
 * Shr mem:
 */
/* shared memory pointers and structures */
typedef struct PointerTableEntry_Shr {
  union
  {
    char* big_buf_pointer; /* pointer into the shared big buf */
    void* seg_start; /*pointer into the shared seg table */
  };
  union
  {  
    int size;
    int num_entries;
  };
} PointerTableEntry_Shr;

typedef struct GroupTableEntry_Shr {
  short type;
  short sequence;
  union {
    WeightTableEntry *weight_ptr; /* pointer to the weight table index */
    int unique_group_id;
  } group_wei_uni;
  unsigned int num_values;
  int num_vars;
  int idx;
  char* sess_name;
  char * data_fname; //input file name
  char * data_dir; //input file name
  char *column_delimiter; //field seperator
  int index_key_var_idx;  // index of Index var key in uvtable
  int start_var_idx;

  char encode_type;
  char *encode_space_by;
  char encode_chars[TOTAL_CHARS];

  Str_to_hash_code idx_var_hash_func;
  int UseOnceOptiontype;
  int UseOnceAbort;
  int first_data_line;
  char absolute_path_flag;
  int max_column_index;//Taking this variable so as to set the maximum column index in parsing and ignoring that line while filling in pointer table. 
  char ignore_invalid_line; //NO ->0 , Yes -> 1
  int sql_or_cmd_var;
  char UseOnceWithinTest[16];
  int sess_idx;
} GroupTableEntry_Shr;

#ifndef CAV_MAIN
extern int unique_group_id;
#else
extern __thread int unique_group_id;
#endif

typedef struct VarTableEntry_Shr {
  GroupTableEntry_Shr* group_ptr; /* pointer into the shared group Table */
  PointerTableEntry_Shr* value_start_ptr; /* pointer into the shared pointer table */
  //void* seg_value_start_ptr; /* pointer into the shared pointer table */
  char* name_pointer; /* pointer into the shared big buf */
  int var_size; //Assumed size will be less than 32K. Defulat -1, > means writable with size specified.
		  // = 0 means writable with no size limitation. 0 not supported yet.
		//Made int here for 4 byte boundry. otherwise short is ok.
  short is_file;             /* If is file type == 1 otherwise 0 */
  int column_idx;//Index to the column in file whose value we want in file parameter var
  int uvtable_idx; /* Index into uvtable*/
} VarTableEntry_Shr;

//extern char fp_rtc_err_msg[1024];

extern void AddPointerTableEntry (int *rnum, char *buf, int len);
extern int create_group_table_entry(int *row_num);
extern int chk_buf(char *line);
extern void dump_per_proc_vgroup_table();
extern void per_proc_create_staticvar_shr_mem(int nvm_id, int sv_grp_id, int runtime);
//extern inline VarTableEntry_Shr *get_fparam_var(VUser *vptr, int sess_id, int hash_code);
extern int get_num_values(char *buf, char *dfname, int first_data_line);
extern int input_static_values(char* file_name, int group_idx, int sess_idx, int var_start, int runtime, char *data_file_buf_rtc, int sql_or_cmd_var);
extern int set_last_option(char *value, char *line_tok, char *staticvar_buf, int *group_idx, int *var_start, int sess_idx, char *script_file_msg, int *state, int *rw_data, int *create_group, int *var_idx, int *column_flag, unsigned short reltv_var_idx);

extern void create_shm_for_static_index_var();
extern void alloc_static_vars_shr_mem();

extern int validate_and_copy_datadir(int mode, char *value, char *dest, char *err_msg);

#endif /* NS_STATIC_VAR_H */
