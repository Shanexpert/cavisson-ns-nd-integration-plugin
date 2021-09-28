#ifndef NS_KEEP_ALIVE_H
#define NS_KEEP_ALIVE_H

#define KA_MODE_NONE       0
#define KA_MODE_BROWSER    1
#define KA_MODE_GROUP      2

extern void check_and_add_ka_timer(connection *cptr, VUser *vptr, u_ns_ts_t now);
extern int kw_set_ka_time_mode(char *buf, short *to_change, char *err_msg, int runtime_flag);
extern int kw_set_ka_timeout(char *buf, int *to_change, char *err_msg, int runtime_flag);
extern void check_and_set_flag_for_ka_timer ();
extern void fill_ka_time_out(VUser *vptr, u_ns_ts_t now);
#endif
