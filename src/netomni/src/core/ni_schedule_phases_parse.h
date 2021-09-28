#ifndef __NI_SCHEDULE_PHASES_H__
#define __NI_SCHEDULE_PHASES_H__
#define GENERATOR_NAME_LEN 512

#define SCHEDULE_PHASE_START            1
#define SCHEDULE_PHASE_RAMP_UP          2
#define SCHEDULE_PHASE_RAMP_DOWN        3
#define SCHEDULE_PHASE_STABILIZE        4
#define SCHEDULE_PHASE_DURATION         5

#define PHASE_NOT_STARTED  -1 
#define PHASE_RUNNING       0
#define PHASE_IS_COMPLETED  1
//Start
#define START_MODE_IMMEDIATE            0
#define START_MODE_AFTER_TIME           1
#define START_MODE_AFTER_GROUP          2
//Duration
#define DURATION_MODE_INDEFINITE        0
#define DURATION_MODE_TIME              1
#define DURATION_MODE_SESSION           2
//Ramp up
#define RAMP_UP_PATTERN_LINEAR       0
#define RAMP_UP_PATTERN_RANDOM       1
#define RAMP_UP_MODE_IMMEDIATE       0
#define RAMP_UP_MODE_STEP            1
#define RAMP_UP_MODE_RATE            2
#define RAMP_UP_MODE_TIME            3
#define RAMP_UP_MODE_TIME_SESSIONS   4
//Ramp Down
#define RAMP_DOWN_PATTERN_LINEAR       0
#define RAMP_DOWN_PATTERN_RANDOM       1
#define RAMP_DOWN_MODE_IMMEDIATE       0
#define RAMP_DOWN_MODE_STEP            1
#define RAMP_DOWN_MODE_TIME            2
#define RAMP_DOWN_MODE_TIME_SESSIONS   3

#define NS_GRP_IS_ALL      -1
#define NS_GRP_IS_INVALID  -2

#define PHASE_NAME_SIZE 48
/*FSR: Step mode for TIME_SESSION*/
#define DEFAULT_STEP  0
#define STEP_TIME     1
#define NUM_STEPS     2
typedef unsigned int       u_ni_ts_t;

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
  int session_rate_per_step;          // session rate per step 

  /* otherwise */
  int tot_num_steps_for_sessions;     // total number of steps for sessions
  int max_ramp_up_vuser_or_sess;               // To keet track how man user is to generate
  int ramped_up_vusers;                // we generate at max 10 user at a time, so we need how many users has been ramped up

  int *per_grp_qty;            /* Array holds per group quantity of a particular phase only in case of 
                                 * scenario based schedule and FCU */
  int step_mode; /*FSR: Added new member to store step mode in case of TIME_SESSION, used while reframing schedule statements*/
  double stagger_time_per_group; /*Added for sending stagger time per group*/
  int *num_vuser_or_sess_per_gen; /*Array holds number of users distributed among generators 
                                    for particular phase in Advance scenario(FCU)*/
  double *num_of_sess_per_gen; /*Array holds number of sessions distributed among generators 
                                    for particular phase in Advance scenario(FSR)*/
  int validate_num_vusers_or_sess; /*Used to store vuser or session for validating schedule phase, need to preserve num_vusers_or_sessfor re-framing of schedule statement*/
  double num_sessions; /*Added for advance group(FSR) used to store session distribution among generators*/
} ni_Ramp_up_schedule_phase;

typedef struct {
  unsigned int time;  // time may be zero
} ni_Stabilize_schedule_phase;

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

  int max_ramp_down_vuser_or_sess;               // To keep track how many user is to remove
  int ramped_down_vusers;                // how many user has to ramp down, took this because we dont want to update ramping delta (specially in case of random pattern) 

  double rpc;                            /* Rate calculated for NVMs in case of time and rate both. */

  int *per_grp_qty;            /* Array holds per group quantity of a particular phase only in case of 
                                 * scenario based schedule */
  int step_mode;            /*FSR: Added new member to store step mode in case of TIME_SESSION,
                               used while reframing schedule statements*/
  int *num_vuser_or_sess_per_gen; /*Array holds number of users distributed among generators
                                    for particular phase in Advance scenario(FCU)*/
  int validate_num_vusers_or_sess; /*Used to store vuser or session for validating schedule phase, need to preserve num_vusers_or_sessfor re-framing of schedule statement*/
  int ramp_down_all; /*Need to add "ALL" string in schedule phase while reframing schedule*/
  double *num_of_sess_per_gen; /*Array holds number of sessions distributed among generators 
                                    for particular phase in Advance scenario(FSR)*/
  double num_sessions; /*Added for advance group(FSR) used to store session distribution among generators*/
} ni_Ramp_down_schedule_phase;

typedef struct {
  int start_mode;  /*Added to identify different start phases*/
  int dependent_grp;            /* -1 = no dependency, otherwise grp idx */
  unsigned int time;
} ni_Start_schedule_phase;

typedef struct {
  int duration_mode;            /* 0 == Indefinite, 1 = Time, 2 = Sessions */
  int seconds;
  int num_fetches;
  int per_user_fetches_flag;
  
} ni_Duration_schedule_phase;

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
  u_ni_ts_t phase_start_time;    //Used in runtime changes to keep the phase start time 
  
  unsigned short left_over;

  union {
    ni_Start_schedule_phase     start_phase;
    ni_Ramp_up_schedule_phase   ramp_up_phase;
    ni_Stabilize_schedule_phase stabilize_phase;
    ni_Duration_schedule_phase  duration_phase;
    ni_Ramp_down_schedule_phase ramp_down_phase;
  } phase_cmd;
} ni_Phases;

typedef struct {
  int num_phases;               /* Number of phases present */
  ni_Phases *phase_array;

  /* For Parent */
  int *bitmask;                 /* bitmask array to keep track of messages from NVMs */
  int bitmask_size;             /* bitmask_size will be common for both bitmask and
                                 * ramp_bitmask */
  int *ramp_bitmask;
  int ramp_msg_to_expect;       /* Keeps track of how many msgs to expect. used 
                                 * with ramp_bitmask while counting bits. */
  int ramp_done_per_cycle_count; /* Count of ramp done msg received once the ramp cycle 
                                  * starts */
  int *ramp_done_bitmask;
  u_ni_ts_t ramp_start_time;

  int total_ramp;
  int cum_total_ramp;
  int total_ramp_done;
  int cum_total_ramp_done;
  int prev_total_ramp_done;
  int prev_cum_total_ramp_done;
  
  /* For child */
  //timer_type* phase_ramp_tmr;      // ramp up/down message timer
  //timer_type* phase_end_tmr;          // Others.
  int group_idx;       /* Keeping for timer callbacks */
  int phase_idx;       /* Keeping for timer callbacks */
  
  u_ni_ts_t iid_mu;               // for timer calculation
  int ns_iid_handle;           // For random usger genaration

  int cur_vusers_or_sess;               //Current user/session of this grp
  int ramping_delta;                  // delta num users for each step:
  // structure used for method like get_rpm_usrs, calc_iid_ms_from_rpm 
  //RpmTimings rpm_timings;

  int *cur_users;              /* Array to hold per group quantity of existing users remaining in the system (i.e. RU - RD)*/
                               /* for scenario based schedule and FCU */
  //Moved from NVM memory to shared memory
  int *cumulative_runprof_table;  /* contains cumulative count of runprof_table for generating random groups */
} schedule;

struct ni_PriorityArray {
  int idx;
  double value;
};
/*Used to store generator name with corresponding schedule settings*/
typedef struct gen_name_sch_setting {
  char generator_name[GENERATOR_NAME_LEN];
  double ramp_up_rate;
  int num_sess;
}gen_name_sch_setting;

extern schedule *scenario_schedule_ptr;
extern schedule *group_schedule_ptr;
extern gen_name_sch_setting *gen_sch_setting;
extern int num_fetches;
extern int ramp_up_all;
//Need these variable to keep group and phase index
extern int rtc_group_idx;
extern int rtc_phase_idx;
//extern int ramp_down_all;
extern int kw_schedule(char *buf, int set_rtc_flag, char *err_msg);
extern int kw_schedule_type(char *buf, char *err_msg);
extern int kw_schedule_by(char *buf, char *err_msg);
extern void initialize_schedule_struct(schedule *schedule_ptr, int grp_idx);
extern ni_Phases *add_schedule_ph(schedule *schedule_ptr, int phase_type, char *phase_name, int *cur_phase_idx);
extern int get_grp_mode(int grp_idx);
extern float convert_to_per_min (char *buf);
extern void distribute_schedule_among_gen();
/*Function used to reconstruct schedule phase statement*/
extern void reframe_schedule_keyword(char *controller_dir, char *scenario_file_name, char *scen_proj_subproj_dir, int tool_call_at_init_rtc, int for_quantity_rtc);
extern double round(double x);
extern void distribute_vuser_or_sess_among_gen();
extern int calculate_total_user_or_session();
extern int ni_parse_runtime_schedule_phase_ramp_up(int grp_idx, char *full_buf, char *phase_name, char *err_msg);
extern int ni_parse_runtime_schedule_phase_ramp_down(int grp_idx, char *full_buf, char *phase_name, char *err_msg);
extern int ni_get_time_from_format(char *str);
extern void update_schedule_structure_per_gen();
extern void ni_validate_phases();
extern void reframe_fcs_keyword(char *controller_dir, char *scenario_file_name, char *scen_proj_subproj_dir);
extern void reframe_progress_msecs(char *controller_dir, char *scenario_file_name, char *scen_proj_subproj_dir);
#endif  /* __NI_SCHEDULE_PHASES_H__ */
