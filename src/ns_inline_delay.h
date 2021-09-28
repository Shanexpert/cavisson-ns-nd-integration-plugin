#ifndef NS_INLINE_DELAY_H
#define NS_INLINE_DELAY_H

#include "ns_cache_include.h"

#define GLOBAL_INLINE_DELAY_IDX 0

#define INLINE_DELAY_MODE_NO_DELAY 0
#define INLINE_DELAY_MODE_INTERNET_RANDOM 1
#define INLINE_DELAY_MODE_CONSTANT 2
#define INLINE_DELAY_MODE_UNIFORM_RANDOM 3
#define INLINE_DELAY_MODE_CUSTOM_DELAY 4

extern void create_default_global_inline_delay_profile(void);
extern inline void calculate_inline_delay(VUser *vptr, u_ns_ts_t *return_time, u_ns_ts_t now);
extern inline void free_repeat_urls(VUser *vptr);
extern void dump_inline_block_time(VUser *vptr, connection* cptr, u_ns_ts_t time_diff);
extern inline void calculate_con_reuse_delay(VUser *vptr, int *min_con_reuse_delay);
extern inline void fill_custom_delay_fun_ptr();
extern int kw_set_g_inline_delay(char *buf, char *change_msg, int runtime_flag);
extern int kw_set_g_inline_min_con_reuse_delay(char *buf, char *change_msg, int *min_con_delay, int *max_con_delay);
extern void initialize_runprof_inline_delay_idx();
extern void free_runprof_inline_delay_idx();
extern void copy_inline_delay_to_shr(void);
extern inline void set_inline_schedule_time(VUser *vptr, u_ns_ts_t now);
#endif
