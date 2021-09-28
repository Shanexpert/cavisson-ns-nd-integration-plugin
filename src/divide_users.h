#ifndef DIVIDE_USERS_H
#define DIVIDE_USERS_H

#define THOUSAND 1000.00
#define HUNDRED 100.00

typedef struct PerScriptTotal {
  char* sess_name;
  int sgroup_num;
  int pct_or_users;
  int script_total; //sum of pct_or_users having same script
} PerScriptTotal;

extern void divide_users_or_pct_per_proc(int conf_num_proc);
#ifndef CAV_MAIN
extern int * per_proc_runprof_table;
#else
extern __thread int * per_proc_runprof_table;
#endif
extern int *per_proc_per_grp_fetches;
extern int *per_grp_sess_inst_num;
extern char *get_grptype(int grp_idx);
extern PerScriptTotal *create_per_script_total();
extern char *find_type_from_type_number(short g_type);
extern char *find_mode_from_seq_number(short seq);
extern void get_str_mode(char *str_mode, char *req_ext, int mode);
extern void detach_shared_memory(void *ptr);
extern int chk_rbu_group(int file_api_idx);
#endif
