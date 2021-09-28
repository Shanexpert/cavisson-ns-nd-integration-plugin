#ifndef NS_NSL_VARS_H
#define NS_NSL_VARS_H

#define INIT_NSVAR_ENTRIES 64
#define DELTA_NSVAR_ENTRIES 32

#define NS_VAR_SCALAR 1
#define NS_VAR_ARRAY 2

extern int create_nsvar_table_entry(int* row_num);


typedef struct NsVarTableEntry {
  int name;  /* index into the temp buffer */
  short type;
  /*To handle array type*/
  ns_bigbuf_t default_value; /* index into big buffer */
  int length;
  int retain_pre_value;
} NsVarTableEntry;

extern int find_nslvar_idx(char* name, int len, int sess_idx);

/*We need to save default value and length, type and name have is already available*/
typedef struct NslVarTableEntry_Shr {
  char *default_value;
  int length;
  int sess_idx;
  int uv_table_idx;
}NslVarTableEntry_Shr;

#ifndef CAV_MAIN
extern NsVarTableEntry* nsVarTable;
extern NsVarTableEntry* grpNsVarTable;
#else
extern __thread NsVarTableEntry* nsVarTable;
extern __thread NsVarTableEntry* grpNsVarTable;
#endif
extern int input_nsl_array_var(char *line, int line_number, int session_idx, char *script_filename);
extern int input_nsl_var(char *line, int line_number, int session_idx, char *script_filename);
extern void copy_nsl_var_into_shared_mem();
extern char *get_nsl_var_default_value(int sess_idx, int uv_idx, int *value_len, int grp_idx);
#endif
