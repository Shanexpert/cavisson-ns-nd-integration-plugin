#include "util.h"

#ifndef __NS_SCHEDULE_PHASES_PARSE_H__
#define __NS_SCHEDULE_PHASES_PARSE_H__

#define SCHEDULE_TYPE_SIMPLE    0
#define SCHEDULE_TYPE_ADVANCED  1

#define SCHEDULE_BY_SCENARIO    0
#define SCHEDULE_BY_GROUP       1

#define PCT_MODE_NUM            0
#define PCT_MODE_PCT            1
#define PCT_MODE_NUM_AUTO       PCT_MODE_NUM

#define DURATION_MODE_INDEFINITE        0
#define DURATION_MODE_TIME              1
#define DURATION_MODE_SESSION           2

#define SESSION_RATE_MULTIPLE   1000.0

/* ramp down method */
#define RDM_MODE_ALLOW_CURRENT_SESSION_COMPLETE      0
#define RDM_MODE_ALLOW_CURRENT_PAGE_COMPLETE       1
#define RDM_MODE_STOP_IMMEDIATELY                  2

/* For RDM_MODE_ALLOW_CURRENT_SESSION_COMPLETE */
#define RDM_OPTION_USE_NORMAL_THINK_TIME                                          0
#define RDM_OPTION_DISABLE_FUTURE_THINK_TIMES                                      1
#define RDM_OPTION_HASTEN_COMPLETION_DISREGARDING_THINK_TIMES                      2
#define RDM_OPTION_HASTEN_COMPLETION_DISREGARDING_THINK_TIMES_USE_IDLE_TIME        3

/* For RDM_MODE_ALLOW_CURRENT_PAGE_COMPLETE */
#define RDM_OPTION_USE_NORMAL_IDLE_TIME                                           0
#define RDM_OPTION_HASTEN_COMPLETION_USING_IDLE_TIME                               1

extern int  kw_set_schedule_type(char *buf, char *err_msg);
extern int kw_set_schedule_by(char *buf, char *err_msg);
extern int kw_set_prof_pct_mode(char *buf, char *err_msg);
extern int parse_schedule_phase_start(int grp_idx, char *buf, char *phaes_name, char *err_msg, int set_rtc_flag);
extern int parse_schedule_phase_ramp_up(int grp_idx, char *buf, char *phaes_name, char *err_msg, int set_rtc_flag);
extern int parse_schedule_phase_duration(int grp_idx, char *buf, char *phaes_name, char *err_msg, int set_rtc_flag);
extern int parse_schedule_phase_stabilize(int grp_idx, char *buf, char *phaes_name, char *err_msg, int set_rtc_flag);
extern int parse_schedule_phase_ramp_down(int grp_idx, char *buf, char *phaes_name, char *err_msg, int set_rtc_flag);
extern int kw_set_schedule(char *buf, char *err_msg, int set_rtc_flag);
extern int kw_set_ramp_down_method(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag);
extern void usage_kw_set_sgrp(char *buf);
extern int kw_set_adjust_rampup_timer(char *buf, char *err_msg, int runtime_flag);
extern char *get_phase_name(int phase_idx);

#endif  /* __NS_SCHEDULE_PHASES_PARSE_H__ */
