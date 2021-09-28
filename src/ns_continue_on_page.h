#ifndef NS_CONTINUE_ON_PAGE_H
#define NS_CONTINUE_ON_PAGE_H

#define RUNTIME_CHANGES_CONF "runtime_changes.conf"
#define SORTED_RUNTIME_CHANGES_CONF "sorted_runtime_changes.conf"
#define RUNTIME_CHANGES_CONF_ALL "runtime_changes_all.conf"

extern int kw_set_continue_on_page_error (char *buf, char *err_msg, int runtime_flag);
extern void copy_continue_on_page_err_to_shr();
extern void initialize_runprof_cont_on_page_err();
extern void create_default_cont_onpage_err(void);
extern void free_runprof_cont_on_page_err_idx();

#endif
