
/*****************************************************************************************
 * Name            : ns_schedule_ramp_up_fsr.h
 * Purpose         : Header file for ns_schedule_ramp_up.c
 * Initial Version : Monday, July 06 2009
 * Modification    : -
 *
 ****************************************************************************************/

#ifndef NS_SCHEDULE_RAMP_UP_FSR_H 
#define NS_SCHEDULE_RAMP_UP_FSR_H

#define SESSION_RATE_CONSTANT 0
#define SESSION_RATE_RANDOM   1

extern void init_user_num_shm();
extern void create_user_num_shm();
extern void incr_nvm_users(int grp_index);
extern void decr_nvm_users(int grp_index);

extern void const_ramp_up_session_rate_with_steps(Schedule *schedule_ptr, u_ns_ts_t now);
extern void random_ramp_up_session_rate_with_steps(Schedule *schedule_ptr, u_ns_ts_t now);
extern void const_ramp_up_session_rate_immediate(Schedule *schedule_ptr, u_ns_ts_t now);
extern void random_ramp_up_session_rate_immediate(Schedule *schedule_ptr, u_ns_ts_t now);
extern void generate_users( ClientData cd, u_ns_ts_t now);
extern void start_ramp_up_phase_fsr(Schedule *schedule_ptr, u_ns_ts_t now);

#endif
