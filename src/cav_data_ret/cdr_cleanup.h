#ifndef CDR_CLEANUP_H
#define CDR_CLEANUP_H

extern struct cleanup_struct *g_cmt_cleanup_policy_ptr;
extern struct cleanup_struct *g_other_tr_cleanup_policy_ptr;


extern int cleanup_process();
extern void remove_tr(struct cdr_cache_entry *entry);
extern void check_ngve_range(struct cdr_cache_entry *entry);
extern int cur_time_in_ngve_days();
extern void remove_tr_ex(int tr_num);
extern void cdr_handle_custom_cleanup();
extern void cdr_handle_other_cleanup();
extern void component_in_remove_range(struct cdr_cache_entry *entry);
extern void remove_raw_data(struct cdr_cache_entry *entry);
extern void remove_har_file(struct cdr_cache_entry *entry);
extern void remove_pagedump(struct cdr_cache_entry *entry);
extern void remove_test_data(struct cdr_cache_entry *entry);
extern void remove_csv(struct cdr_cache_entry *entry);
extern void remove_logs(struct cdr_cache_entry *entry);
extern void remove_db(struct cdr_cache_entry *entry);
extern void cdr_recyclebin_cleanup();
extern void remove_reports(struct cdr_cache_entry *entry);
extern void cdr_remove_sm_data();
#endif
