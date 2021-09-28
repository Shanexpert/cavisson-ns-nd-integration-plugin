
/*****************************************************************************************
 * Name            : ns_schedule_ramp_down_fsr.h
 * Purpose         : Header file for ns_schedule_ramp_up.c
 * Initial Version : Monday, July 06 2009
 * Modification    : -
 *
 ****************************************************************************************/

#ifndef NS_SCHEDULE_RAMP_DOWN_FSR_H
#define NS_SCHEDULE_RAMP_DOWN_FSR_H

extern void ramp_down_session_rate_immediate(Schedule *schedule_ptr, u_ns_ts_t now);
extern void const_ramp_down_session_rate_with_steps(Schedule *schedule_ptr, u_ns_ts_t now);
extern void random_ramp_down_session_rate_with_steps(Schedule *schedule_ptr, u_ns_ts_t now);
extern void start_ramp_down_phase_fsr(Schedule *schedule_ptr, u_ns_ts_t now);
extern void search_and_remove_user_for_fsr(Schedule *schedule_ptr, u_ns_ts_t now);
#endif
