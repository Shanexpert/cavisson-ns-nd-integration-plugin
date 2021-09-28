#ifndef NS_PARAM_OVERRIDE_H
#define NS_PARAM_OVERRIDE_H

typedef struct ParamOverrideTable_t {
  int grp_idx; 
  char grp_name[4096]; 
  char param_name[4096]; 
  char param_value[4096]; 
} ParamOverrideTable_t;

#define DELTA_PARAM_OVERRIDE_ENTRIES 10
extern int total_paramoverride_entries;
extern int max_paramoverride_entries;
extern ParamOverrideTable_t *paramOverrideTable;
extern void replace_overide_values ();
extern int kw_set_g_script_param(char *buf, char *err_msg);

#endif
