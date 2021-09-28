#ifndef NS_UNIQUE_VARS_H
#define NS_UNIQUE_VARS_H
#define INIT_UNIQUEVAR_ENTRIES 64
#define DELTA_UNIQUEVAR_ENTRIES 32
typedef struct UniqueVarTableEntry {
  ns_bigbuf_t name;
  ns_bigbuf_t format;
 // char *uniq_num; // To store the unique number. Used only for Session
  int refresh;
 // int scon_idx;
  //int filled_flag; //For refresh, first time we need to get the unique value. Only for Session case
 int sess_idx; 
 int num_digits; //for length of the format
}UniqueVarTableEntry;

extern char *get_formated_unique_number(char *format,int len);
//check the arguments in the function 
extern int input_uniquevar_data(char* line, int line_number, int sess_idx, char *script_filename);
extern UniqueVarTableEntry_Shr *copy_uniquevar_into_shared_mem(void);
extern void init_uniquevar_info();
extern char* get_unique_var_value(UniqueVarTableEntry_Shr* var_ptr, VUser* vptr, int var_val_flag, int* total_len);
extern char* get_unique_range_var_value(VUser* vptr, int var_val_flag, int* total_len, int unique_var_idx);
extern void clear_uvtable_for_unique_var(VUser *vptr);





















//prototype
#ifndef CAV_MAIN
extern UniqueVarTableEntry* uniqueVarTable;
#else
extern __thread UniqueVarTableEntry* uniqueVarTable;
#endif

//#define USE 1
//#define SESSION 2
//#define ONCE 3
extern int find_uniquevar_idx(char* name, int sess_idx);
#endif /* RANDOM_VARS_H */

