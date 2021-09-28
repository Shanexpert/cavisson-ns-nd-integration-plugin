#ifndef NS_PAGE_THINK_TIME_H
#define NS_PAGE_THINK_TIME_H

#define OVERRIDE_THINK_TIME_DISABLED         0
#define OVERRIDE_REC_THINK_TIME         1
#define MULTIPLY_REC_THINK_TIME         2
#define USE_RANDOM_PCT_REC_THINK_TIME   3

extern int ns_set_pg_think_time_ext(VUser *vptr, int pg_think);

extern int ns_page_think_time_ext(VUser *vptr, int page_think_time);

// Called from execute tasks
extern void nsi_page_think_time(VUser *vptr, u_ns_ts_t now);
extern inline void calc_page_think_time(VUser *vptr);
extern inline void fill_custom_page_think_time_fun_ptr();
#endif  /* NS_PAGE_THINK_TIME_H */
