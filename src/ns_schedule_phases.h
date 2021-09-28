#ifndef __NS_SCHEDULE_PHASES_H__
#define __NS_SCHEDULE_PHASES_H__

#include "nslb_bitflag.h"
//#include "ns_cavmain_child_thread.h"
#define SCHEDULE_PHASE_START            1
#define SCHEDULE_PHASE_RAMP_UP          2
#define SCHEDULE_PHASE_RAMP_DOWN        3
#define SCHEDULE_PHASE_STABILIZE        4
#define SCHEDULE_PHASE_DURATION         5
#define SCHEDULE_PHASE_RTC              6

#define PHASE_NOT_STARTED  -1 
#define PHASE_RUNNING       0
#define PHASE_IS_COMPLETED  1

/* PHASE_PAUSED is used in to get the status of timer. 
 * Why we used status ?
 * Ans - Phase -> Ramp Up 
 * Child 0       |  Child 1       | Child 2 | Child 3
 * Ramping Users |  Stagger       | Stagger | Stagger 
 * Ramping Users |  Ramping Users | Stagger | Stagger 
 * Ramping Users |  Ramping Done  | Stagger | Stagger    <----- Paused
 * 
 * When pause signal is recieved we delete all running timer & calculate remaining time,
 * when resume signal is recieved we have to start those timer.
 * But we do not know which timer was deleted in when pause came.
 * So to remember those timer we took a flag status.
 */
#define PHASE_PAUSED 100

#define PHASE_NAME_SIZE 48
#define LONG_LIMIT      0xffffffff 

#define RTC_FREE -1
#define RTC_NEED_TO_PROCESS 0
#define RTC_RUNNING 1

typedef struct {
  /* For keyword parsing */
  int num_vusers_or_sess;       // Num users or sessions to be ramp up, if rate its multiple of 1000

  unsigned char ramp_up_mode;                   // IMMEDIATE, STEP, RATE, TIME
 
  int ramp_up_step_users;             // Num users to be ramp up in ONE STEP
  int ramp_up_step_time;              // ramp up step time for users/sessions in secs

  double ramp_up_rate;                   // Fill if Rate is given, also it is caluclated in case of Time
  double rpc;                            /* Rate calculated for NVMs in case of time and rate both. */

  int ramp_up_time;                   // Can we take common var for Ramp Up Time & Ramp Up Step Time in secs

  char ramp_up_pattern;                // Linear/Random applicable only in ramp_up_mode Rate/Time

  /* for Time Sessions */
  //char step_mode;
  //int step_time_or_num_steps;
  
  int session_rate_per_step;          // session rate per step 


  /* otherwise */
  //unsigned int grp_max_user_limit;                 // max user limit for FSR scnario
  int tot_num_steps_for_sessions;     // total number of steps for sessions
  int max_ramp_up_vuser_or_sess;               // To keet track how man user is to generate
  int ramped_up_vusers;                // we generate at max 10 user at a time, so we need how many users has been ramped up

  int *per_grp_qty;            /* Array holds per group quantity of a particular phase only in case of 
                                 * scenario based schedule and FCU */
  int nvm_dist_index;          /*start index of active NVM*/
  int nvm_dist_count;          /*Count of active NVMs*/
} Ramp_up_schedule_phase;

typedef struct {
  unsigned int time;  // time may be zero
} Stabilize_schedule_phase;

typedef struct {
  int num_vusers_or_sess;            // Num users to be ramp down 
  int ramp_down_mode;                 // IMMEDIATE, STEP, TIME
  int ramp_down_pattern;              // Linear/Random

  int ramp_down_step_users;           // Num users to be ramp down in ONE STEP
  int ramp_down_time;                 // Ramp Down Time
  int ramp_down_step_time;            // Ramp Down Time for 1 STEP  
  int session_rate_per_step;

  int tot_num_steps_for_sessions;     // total number of steps for sessions
  int num_ramp_down_sessions;         // Num sessions to be ramp down

  //int ultimate_ramp_down_max_vusers;  
  int max_ramp_down_vuser_or_sess;               // To keep track how many user is to remove
  int ramped_down_vusers;                // how many user has to ramp down, took this because we dont want to update ramping delta (specially in case of random pattern) 

  double rpc;                            /* Rate calculated for NVMs in case of time and rate both. */

  int *per_grp_qty;            /* Array holds per group quantity of a particular phase only in case of 
                                 * scenario based schedule */

  /*   timer_type* ramp_down_msg_tmr;      // ramp down message timer      */
  /*   timer_type* ramp_down_tmr;          // ramp down timer for user removal */

  int nvm_dist_index;          /*start index of active NVM*/
  int nvm_dist_count;          /*Count of active NVMs*/
} Ramp_down_schedule_phase;

typedef struct {
  int dependent_grp;            /* -1 = no dependency, otherwise grp idx */
  unsigned int time;
} Start_schedule_phase;

typedef struct {
  int duration_mode;            /* 0 == Indefinite, 1 = Time, 2 = Sessions */
  int seconds;
  int num_fetches;
  int per_user_fetches_flag;    
      /*per_user_fetches_flag: This flag will tell whether fetches will distribute over all the users of same group or 
        applied per user of same group. It can have two values - 0 for disable and 1 for enable
        Eg: G1 has 2 Vusers and number of fetches for this group is 10 then 
            for 0(i.e. disable) => total number of fetches = 10 
            And for 1(i.e. enable) => total number of fetches = 10 * 2 = 20 */
} Duration_schedule_phase;

typedef struct {
  char phase_type;               /* START/RAMP_UP/RAMP_DOWN/RUN_PHASE */
  char phase_name[PHASE_NAME_SIZE + 1];
  char phase_status;             /* -1: not started, 0: running, 1: completed */
  char default_phase;                  /* Marks if default value is filled, i.e. without keyword */
  /* Phase number will be a running sequence number, starting with 0*/
  unsigned short phase_num;                  // Phase sequence number
  /* This run time flag is used in runtime updation
   * If it is 1 it shows that phase data has been 
   * change so now process according to new changes
   */ 
  char runtime_flag;
  u_ns_ts_t phase_start_time;    //Used in runtime changes to keep the phase start time 
  
  unsigned short left_over;
  union {
    Start_schedule_phase     start_phase;
    Ramp_up_schedule_phase   ramp_up_phase;
    Stabilize_schedule_phase stabilize_phase;
    Duration_schedule_phase  duration_phase;
    Ramp_down_schedule_phase ramp_down_phase;
  } phase_cmd;
  //Phases *next;
} Phases;

typedef struct SMMonSessionInfo SMMonSessionInfo;

typedef struct {
  char type; //0 means non runtime schedule, 1 menas runtime 
  short ramp_up_define;       /*Set flag for ramp up phase used in case of advance scenario*/
  int num_phases;               /* Number of phases present */
  Phases *phase_array;

  /* For Parent */
  void *bitmask;                 /* bitmask array to keep track of messages from NVMs */
  int bitmask_size;             /* bitmask_size will be common for both bitmask and
                                 * ramp_bitmask */
  void *ramp_bitmask;
  int ramp_msg_to_expect;       /* Keeps track of how many msgs to expect. used 
                                 * with ramp_bitmask while counting bits. */
  int ramp_done_per_cycle_count; /* Count of ramp done msg received once the ramp cycle 
                                  * starts */
  void *ramp_done_bitmask;
  u_ns_ts_t ramp_start_time;

  int total_ramp;
  int cum_total_ramp;
  int total_ramp_done;
  int cum_total_ramp_done;
  int prev_total_ramp_done;
  int prev_cum_total_ramp_done;
  
  /* For child */
  timer_type* phase_ramp_tmr;      // ramp up/down message timer
  timer_type* phase_end_tmr;          // Others.
  int group_idx;       /* Keeping for timer callbacks */
  int phase_idx;       /* Keeping for timer callbacks */
  
  u_ns_ts_t iid_mu;               // for timer calculation
  int ns_iid_handle;           // For random usger genaration

  int cur_vusers_or_sess;               //Current user/session of this grp
  int ramping_delta;                  // delta num users for each step:
  // structure used for method like get_rpm_usrs, calc_iid_ms_from_rpm 
  RpmTimings rpm_timings;

  //Keeps the phase-idx in which change quantity is required(used by NVMs)
  int runtime_ramp_up_phase_idx;
  //Keeps the phase-idx in which change quantity is required(used by NVMs)
  int runtime_ramp_down_phase_idx;
  //Keeps the group wise quantity both (+/-) changed at runtime 
  int *runtime_qty_change;
  
  int *cur_users;              /* Array to hold per group quantity of existing users remaining in the system (i.e. RU - RD)*/
                               /* for scenario based schedule and FCU */
  //Moved from NVM memory to shared memory
  int *cumulative_runprof_table;  /* contains cumulative count of runprof_table for generating random groups */
  int rtc_id;//Stores runtime id
  int start_idx; 
  int rtc_state; //Stores state
  int rtc_idx; //Index
  int total_done_msgs_need_to_come;
  int total_rtcs_for_this_id;
  int actual_timeout;
  SMMonSessionInfo *sm_mon_info; 
} Schedule;

struct PriorityArray {
  int idx;
  double value;
};

#ifndef CAV_MAIN
extern Schedule *scenario_schedule;
extern Schedule *group_schedule;
#else
extern __thread Schedule *scenario_schedule;
extern __thread Schedule *group_schedule;
#endif
extern Schedule *runtime_schedule;
extern int total_rtc_running;
extern size_t *g_child_status_mask;
extern size_t **g_child_group_status_mask;


extern Phases *add_schedule_phase(Schedule *schedule, int phase_type, char *phase_name);
extern int calculate_high_water_mark(Schedule *schedule, int num_phases);
extern int calculate_total_users_or_sess(Schedule *schedule, int grp_idx);
extern void high_water_mark_check(Schedule *schedule, int grp_mode, int grp_idx);
extern void balance_array(double *array, int len, int total_qty);
extern void convert_pct_to_qty();
extern int get_group_mode(int grp_idx);
extern void validate_phases();
extern void init_vport_table_phases();
extern void send_schedule_phase_start(Schedule *schedule,  int grp_idx, int phase_idx);
extern void init_schedule_bitmask() ;
extern int get_total_users(Schedule *schedule);
extern int get_dependent_group(int grp_idx, int *dependent_grp_array) ;
extern int get_total_from_nvm(int nvm_id, int grp_idx);
extern void balance_phase_array_per_nvm(int qty_to_distribute, double *pct_array, double *qty_array, Schedule *schedule, int grp_idx, int phase_id);
extern void balance_phase_array_per_group(int qty_to_distribute, double *pct_array, 
                                          double *qty_array, Schedule *schedule, int phase_id, int proc_index);
extern void distribute_phase_over_nvm(int grp_idx, Schedule *schedule);
extern void distribute_group_qty_over_phase(Schedule *schedule, int proc_index);
extern void process_schedule_msg_from_parent(parent_child *msg);

extern int calc_iid_ms_from_rpm(Schedule *ptr, int rpm);
extern int init_n_calc_iid_ms_from_rpm(Schedule *ptr, int rpm);
extern int get_rpm_users (Schedule *ptr, int surplus_user_ratio);
extern void init_rate_intervals(Schedule *ptr);
extern void setup_schedule_for_nvm(int original_num_process);

extern void add_phase_num();
extern void set_phase_num_for_schedule(Schedule *cur_schedule);
extern void get_current_phase_info(int grp_idx, int phase_element, char *phase_element_val);
extern int get_RU_RD_phases_count(Schedule *schedule);
extern int is_group_over(int grp_idx, int proc_index);
extern double get_fsr_grp_ratio(Schedule *cur_schedule, int grp_idx);

#define SET_PHASE_NUM(phase_num)  \
  Schedule *cur_schedule;         \
  Phases *phase_ptr; \
  if(global_settings->schedule_by == SCHEDULE_BY_SCENARIO) \
       cur_schedule = v_port_entry->scenario_schedule; \
  else \
  { \
    cur_schedule = &(v_port_entry->group_schedule[vptr->group_num]); \
  } \
  phase_ptr = &(cur_schedule->phase_array[cur_schedule->phase_idx]); \
  phase_num = phase_ptr->phase_num;

#define SCHEDULE_MSG 1
#define RAMP_MSG     2
#define RAMPDONE_MSG 3
/* Handle schedule using bitflag */
#define INC_SCHEDULE_MSG_COUNT(child_id)\
  if(nslb_set_bitflag(schedule->bitmask, child_id))\
    NSDL1_SCHEDULE(NULL, NULL, "START_PHASE: Already sent msg to child %d", child_id);

#define INC_STATUS_MSG_COUNT(child_id, grp_idx)\
  if(nslb_set_bitflag(g_child_group_status_mask[(grp_idx < 0)?0:grp_idx], child_id))\
    NSDL1_SCHEDULE(NULL, NULL, "STATUS: Already set for child %d, group %d", child_id, grp_idx);

#define INC_RAMP_MSG_COUNT(grp_idx) memcpy(schedule->ramp_bitmask, g_child_group_status_mask[(grp_idx < 0)?0:grp_idx], MAX_BITFLAG_SIZE);
#define INC_RAMPDONE_MSG_COUNT(child_id)\
  if(nslb_set_bitflag(schedule->ramp_done_bitmask, child_id))\
    NSDL1_SCHEDULE(NULL, NULL, "RAMPUP_DONE_MESSAGE/RAMPDOWN_DONE_MESSAGE: Already sent msg from child %d", child_id);\

#define INC_CHILD_STATUS_COUNT(child_id)\
  if(nslb_set_bitflag(g_child_status_mask, child_id))\
    NSDL1_SCHEDULE(NULL, NULL, "STATUS: Already set for child %d", child_id);

#define DEC_SCHEDULE_MSG_COUNT(child_id, is_failed, grp_idx)\
  if(nslb_reset_bitflag(schedule->bitmask, child_id)){\
    NSDL1_SCHEDULE(NULL, NULL, "PHASE_COMPLETE: Already received msg from child %d", child_id);\
    if(!is_failed)\
      return 0;\
  } else if(is_failed)\
    handle_child_failure(schedule, grp_idx, child_id, SCHEDULE_MSG);

#define DEC_STATUS_MSG_COUNT(child_id, grp_idx){\
  if(nslb_reset_bitflag(g_child_group_status_mask[(grp_idx < 0)?0:grp_idx], child_id))\
    NSDL1_SCHEDULE(NULL, NULL, "Already child %d is over", child_id);\
}

#define DEC_RAMP_MSG_COUNT(child_id, is_failed, grp_idx)\
  if(nslb_reset_bitflag(schedule->ramp_bitmask, child_id)){\
    NSDL1_SCHEDULE(NULL, NULL, "RAMPUP_MESSAGE/RAMPDOWN_MESSAGE: Already received msg from child %d", child_id);\
  } else if(is_failed)\
    handle_child_failure(schedule, grp_idx, child_id, RAMP_MSG);

#define DEC_RAMPDONE_MSG_COUNT(child_id, is_failed, grp_idx)\
  if(nslb_reset_bitflag(schedule->ramp_done_bitmask, child_id)) {\
    NSDL1_SCHEDULE(NULL, NULL, "RAMPUP_DONE_MESSAGE/RAMPDOWN_DONE_MESSAGE: Already received msg from child %d", child_id);\
  } else if(is_failed)\
    handle_child_failure(schedule, grp_idx, child_id, RAMPDONE_MSG);

#define DEC_CHILD_STATUS_COUNT(child_id){\
  if(nslb_reset_bitflag(g_child_status_mask, child_id))\
    NSDL1_SCHEDULE(NULL, NULL, "Already child %d is over", child_id);\
}

#define CHECK_ALL_SCHEDULE_MSG_DONE nslb_check_all_reset_bits(schedule->bitmask)
#define CHECK_ALL_RAMP_MSG_DONE nslb_check_all_reset_bits(schedule->ramp_bitmask)
#define CHECK_ALL_RAMPDONE_MSG_DONE nslb_check_all_reset_bits(schedule->ramp_done_bitmask)
#define CHECK_ALL_CHILD_DONE nslb_check_all_reset_bits(g_child_status_mask)

extern int find_runtime_qty_mem_size();
extern void update_main_schedule(Schedule *runtime_schedule);
extern int find_running_phase_tmr(int index);
extern void handle_child_failure(Schedule *schedule, int grp_idx, int child_idx, int msg_type);

#endif  /* __NS_SCHEDULE_PHASES_H__ */
