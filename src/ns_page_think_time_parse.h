#ifndef NS_PAGE_THINK_TIME_PARSE_H
#define NS_PAGE_THINK_TIME_PARSE_H

#include "ns_cache_include.h"
#define PAGE_THINK_TIME_MODE_NO_THINK_TIME 0
#define PAGE_THINK_TIME_MODE_INTERNET_RANDOM 1
#define PAGE_THINK_TIME_MODE_CONSTANT 2
#define PAGE_THINK_TIME_MODE_UNIFORM_RANDOM 3
#define PAGE_THINK_TIME_MODE_CUSTOM 4

extern void create_default_global_think_profile();
extern void copy_think_prof_to_shr();
extern int kw_set_g_page_think_time(char *buf, char *err_msg, int runtime_flag);
extern void initialize_runprof_page_think_idx();
extern void free_runprof_page_think_idx();
extern void  log_page_time_for_debug(VUser *vptr, ThinkProfTableEntry_Shr *think_prof_ptr, OverrideRecordedThinkTime* override_think, int think_time, u_ns_ts_t now);

/* Override recorded think time macro*/
#define OVERRIDE_REC_THINK_MODE_USE_SCEN_SETTING  1
#define OVERRIDE_REC_THINK_MODE_MULTIPLY_REC_THINK_TIME  2
#define OVERRIDE_REC_THINK_MODE_RANDOM_PCT_REC_THINK_TIME  3

extern int kw_set_override_recorded_think_time(char *buf, char *change_msg, int runtime_flag);
extern void free_runprof_overrided_rec_think_time();
extern void copy_recorded_think_time_to_shr();
extern void create_default_recorded_think_time(void);
extern void initialize_runprof_override_rec_think_time(); 

extern char ptt_cll_back_msg[1024];
#endif //NS_PAGE_THINK_TIME_PARSE_H

