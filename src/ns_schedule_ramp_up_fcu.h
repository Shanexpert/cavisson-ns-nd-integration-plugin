/*****************************************************************************************
 * Name            : ns_schedule_ramp_up.h
 * Purpose         : Header file for ns_schedule_ramp_up.c
 * Initial Version : Monday, July 06 2009
 * Modification    : -
 *
 ****************************************************************************************/
#ifndef NS_SCHEDULE_RAMP_UP_H 

#define NS_SCHEDULE_RAMP_UP_H 


#define RAMP_UP_PATTERN_LINEAR       0
#define RAMP_UP_PATTERN_RANDOM       1

#define RAMP_UP_MODE_IMMEDIATE       0
#define RAMP_UP_MODE_STEP            1
#define RAMP_UP_MODE_RATE            2
#define RAMP_UP_MODE_TIME            3
#define RAMP_UP_MODE_TIME_SESSIONS   4

extern void start_ramp_up_phase(Schedule *schedule_ptr, u_ns_ts_t now);
extern u_ns_ts_t ramp_up_users(u_ns_ts_t now);
extern void validate_run_phases_keyword();
extern void stop_phase_ramp_timer(Schedule *schedule_ptr);
extern void stop_phase_end_timer(Schedule *schedule_ptr);
extern void ramp_up_phase_done_fcu(Schedule *schedule_ptr);

#endif


/**********************************************End Of FIle******************************/
