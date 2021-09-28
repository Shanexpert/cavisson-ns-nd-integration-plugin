/********************************************************************
 * Name            : ns_session.h 
 * Purpose         : - 
 * Initial Version : Monday, July 13 2009
 * Modification    : -
 ********************************************************************/

#ifndef NS_SESSION_H
#define NS_SESSION_H
#define NS_SESSION_FAILURE -2

extern int on_new_session_start (VUser *vptr, u_ns_ts_t now);
extern void on_session_completion(u_ns_ts_t now, VUser *vptr, connection * cptr, int do_cleanup);
extern void nsi_end_session(VUser *vptr, u_ns_ts_t now);
extern int ns_exit_session_ext(VUser *vptr);
extern int ns_end_session(VUser *vptr, int which_ctx);
extern u_ns_ts_t cur_time;
extern unsigned int sess_inst_num;

//RBU
extern int ns_rbu_on_session_start(VUser *vptr);
extern inline void ns_rbu_stop_browser_on_sess_end(VUser *vptr);
extern inline void ns_rbu_on_sess_end(VUser *vptr);
extern int ns_rbu_validate_profile(VUser *vptr, char *browser_base_log_path, char *prof_name);
extern void init_sess_inst_start_num();
extern int ns_rbu_set_screen_size(VUser *vptr, char *prof_name);
extern void remove_all_blocked_users();
extern void nsi_retry_session(VUser *vptr, int errorCode);
#endif
