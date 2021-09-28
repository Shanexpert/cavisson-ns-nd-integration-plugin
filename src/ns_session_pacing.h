#ifndef NS_SESSION_PACING_H 
#define NS_SESSION_PACING_H 


#define SESSION_PACING_MODE_NONE    0
#define SESSION_PACING_MODE_AFTER   1
#define SESSION_PACING_MODE_EVERY   2

#define SESSION_PACING_TIME_MODE_CONSTANT        0
#define SESSION_PACING_TIME_MODE_INTERNET_RANDOM 1
#define SESSION_PACING_TIME_MODE_UNIFORM_RANDOM  2

#define DO_NOT_REFRESH_USER_ON_NEW_SESSION  0
#define REFRESH_USER_ON_NEW_SESSION  1

#define SESSION_PACING_ON_FIRST_SESSION_OFF 0
#define SESSION_PACING_ON_FIRST_SESSION_ON  1
 

extern int kw_set_g_first_session_pacing(char *buf, char *err_msg, int runtime_flag);
extern int kw_set_g_new_user_on_session(char *buf, char *err_msg, int runtime_flag);
extern int kw_set_g_session_pacing(char *buf, char *err_msg, int runtime_flag);
extern void copy_session_pacing_to_shr();
extern void create_default_pacing();
extern int check_pacing_enable();
extern inline void start_session_pacing_timer(VUser *vptr, connection *cptr, int user_first_sess, int *start_now, u_ns_ts_t now, int new_or_reuse);

#endif //NS_SESSION_PACING_H 
