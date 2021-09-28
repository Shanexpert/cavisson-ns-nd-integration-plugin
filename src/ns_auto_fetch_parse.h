#ifndef NS_AUTO_FETCH_PARSE_H 
#define NS_AUTO_FETCH_PARSE_H 


#include <regex.h>
#include "ns_cache_include.h"

#ifndef CAV_MAIN
extern char *g_auto_fetch_info;
extern int g_auto_fetch_info_total_size;
#else
extern __thread char *g_auto_fetch_info;
extern __thread int g_auto_fetch_info_total_size;
#endif

extern int kw_set_auto_fetch_embedded(char *buf, char *err_msg, int runtime_flag);
extern void copy_auto_fetch_to_shr();
extern void create_default_auto_fetch_table();
extern void initialize_runprof_auto_fetch_idx();
extern void free_runprof_auto_fetch_idx();
extern int check_auto_fetch_enable();
extern void validate_g_auto_fetch_embedded_keyword();
extern void kw_set_inline_filter_patterns(char * buff, GroupSettings *gset, int grp_idx);
extern void create_pattern_shr_mem();
extern void kw_set_include_exclude_domain_settings(char *buf, GroupSettings *gset);
#endif //NS_AUTO_FETCH_PARSE_H 
