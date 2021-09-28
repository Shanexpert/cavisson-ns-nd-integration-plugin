/*****************************************************************************************
 * Name            : ns_schedule_ramp_down.h
 * Purpose         : Header file for ns_schedule_ramp_up.c
 * Initial Version : Monday, July 06 2009
 * Modification    : -
 *
 ****************************************************************************************/
#ifndef NS_SCHEDULE_RAMP_DOWN_H 

#define NS_SCHEDULE_RAMP_DOWN_H 

//#define NS_SESSION_STOPPED  2

#define RAMP_DOWN_PATTERN_LINEAR       0
#define RAMP_DOWN_PATTERN_RANDOM       1

#define RAMP_DOWN_MODE_IMMEDIATE       0
#define RAMP_DOWN_MODE_STEP            1
#define RAMP_DOWN_MODE_TIME            2
#define RAMP_DOWN_MODE_TIME_SESSIONS   3


#define CALC_IID_MU(rpc, cur_schedule) {                                        \
        if ( rpc < 100.0)                                                       \
          cur_schedule->iid_mu = (int)((double)(60000)/rpc);                    \
        else                                                                    \
          cur_schedule->iid_mu = (double)calc_iid_ms_from_rpm(cur_schedule, (int)rpc);   \
        if(cur_schedule->iid_mu == 0)                                           \
          error_log("Calculated iid_mu became 0.");                             \
}

#define CALC_IID_HANDLE(rpc, cur_schedule) {                                    \
        int i = 0;                                                              \
        do                                                                      \
        {                                                                       \
          i++;                                                                  \
          cur_schedule->iid_mu = (double)(i*60*1000.0)/rpc;  /* IID in ms */    \
        } while (cur_schedule->iid_mu < 10.0);             /* Min granularity is 10 ms */ \
        cur_schedule->ramping_delta = i;                                        \
        cur_schedule->ns_iid_handle = ns_poi_init(cur_schedule->iid_mu, my_port_index);   \
}
extern void start_ramp_down_phase(Schedule *schedule_ptr, u_ns_ts_t now);       
extern void finish( u_ns_ts_t now );
extern void ramp_down_close_connection(VUser *vptr, connection *cptr, u_ns_ts_t now);
extern int is_all_phase_over();
extern void stop_ramp_down_timer(timer_type* ptr);
extern void stop_ramp_down_msg_timer(timer_type* ptr);
extern int stop_user_and_allow_cur_sess_complete(VUser *vptr, u_ns_ts_t now);
extern int stop_user_and_allow_current_page_to_complete(VUser *vptr, u_ns_ts_t now);
extern int stop_user_immediately(VUser *vptr,  u_ns_ts_t now);
extern void remove_users(Schedule *schedule_ptr, int do_all, u_ns_ts_t now, int runtime_flag, int quantity, int grp_idx);
extern void remove_users_ex(Schedule *schedule_ptr, int do_all, u_ns_ts_t now, int runtime_flag, int quantity, int grp_idx, int rampdown_method);
extern void time_ramp_down_users_linear_pattern_child_stagger_timer_start(u_ns_ts_t now, Schedule *schedule_ptr);
extern void start_ramp_down_timer(u_ns_ts_t now, Schedule *schedule_ptr, int periodic);
extern void check_and_finish_ramp_down_phase(Schedule *schedule_ptr, u_ns_ts_t now, Ramp_down_schedule_phase *ramp_down_phase_ptr);
extern void remove_all_user_stop_immediately(int do_all, u_ns_ts_t now);
#endif


/**********************************************End Of FIle******************************/
