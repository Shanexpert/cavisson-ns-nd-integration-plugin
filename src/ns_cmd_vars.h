#ifndef NS_CMD_VAR_C 
#define NS_CMD_VAR_C 
  
typedef struct CMDVarTableEntry
{   
  ns_bigbuf_t persist_flag;
  ns_bigbuf_t prog_with_args;
  char *prog_result;
  long int prog_result_buff_size;
}CMDVarTableEntry;

//cmd_vars_done flag
#define CMD_PARAM_PERSIST_FLAG_PARSING_DONE   0x0001
#define CMD_PARAM_PROG_PARSING_DONE           0x0002

#define NS_CMD_PARAM_MAX_PROG_OUTPUT_LEN      1024 * 1024
#define NS_CMD_MAX_LEN                        4096

extern int parse_cmd_var_params(char* keyword, char* value, CMDVarTableEntry* cmdVarTable, int* state, char* script_file_msg, short int *done, char* staticvar_buf, int* group_idx, int* var_start, int sess_idx, int* rw_data, int* create_group, int* var_idx, int* column_flag, unsigned short* reltv_var_idx);

extern int ns_cmd_param_validate_keywords(short int* cmd_args_done_flag, char* script_file_msg);

extern int ns_cmd_var_get_data_from_prog(CMDVarTableEntry* cmdVarTable, char* column_delimiter, char *err_msg, int grp_idx, char *file_name, GroupTableEntry* groupTable);

#endif
