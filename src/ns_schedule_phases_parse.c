#include <stdio.h>
#include <stdlib.h>
#include <regex.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "nslb_util.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"

#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "user_tables.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "util.h"
#include "timing.h"
#include "logging.h"
#include "tmr.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "ns_log.h"
#include "ns_schedule_phases_parse.h"
#include "ns_schedule_ramp_up_fcu.h"
#include "ns_schedule_ramp_down_fcu.h"
#include "wait_forever.h"
#include "ns_trans_parse.h"
#include "ns_exit.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"

extern double round(double x);

/*void usage_kw_set_sgrp(char *buf)
{
  if (buf != NULL)
    NSTL1_OUT(NULL, NULL, "Error:\n%s\n", buf);
    NS_KW_PARSING_ERR(buf, 0, err_msg, SGRP_USAGE, CAV_ERR_1011306, CAV_ERR_MSG_1);
 // NS_EXIT(-1, "%s\n\tSGRP <GroupName> <ScenType> <user-profile> "
     //     "<type(0 for Script or 1 for URL)> <session-name/URL> <num-or-pct> <cluster_id>");
}*/

//SCHEDULE_TYPE   <Simple or Advanced>
/*void usage_kw_set_schedule_type(char *buf)
{
  if (buf != NULL)
    fprintf(stderr, "Error:\n%s\n", buf);
  fprintf(stderr, "Syntax\n");
  fprintf(stderr, "\t SCHEDULE_TYPE <SIMPLE or ADVANCED>\n");
  exit(-1);
}

//SCHEDULE_BY  <Scenario or Group>
void usage_kw_set_schedule_by(char *buf)
{
  if (buf != NULL)
    fprintf(stderr, "Error:\n%s\n", buf);
  fprintf(stderr, "Syntax\n");
  fprintf(stderr, "\t SCHEDULE_BY <SCENARIO or GROUP>\n");
  exit(-1);
}

//PROF_PCT_MODE <NUM or PCT or NUM_AUTO>
void usage_kw_set_prof_pct_mode(char *buf)
{
  if (buf != NULL)
    fprintf(stderr, "Error:\n%s\n", buf);
  fprintf(stderr, "Syntax\n");
  fprintf(stderr, "\t PROF_PCT_MODE <NUM or PCT or NUM_AUTO>\n");
  exit(-1);
}*/

/**
 * SCHEDULE <group_name> <phase_name> START <start_mode> <start_args> 
 *
 * start_mode and args:
 * IMMEDIATELY    (Start from beginning of test run) (Default)
 * AFTER TIME HH:MM:SS
 * AFTER GROUP G1 [HH:MM:SS] (valid only for group based scheduling)
 */
/*void usage_parse_schedule_phase_start(char *buf)
{
  if (buf != NULL)
    fprintf(stderr, "Error:\n%s\n", buf);
  fprintf(stderr, "Syntax\n");
  fprintf(stderr, "\t SCHEDULE <group_name> <phase_name> START <start_mode> <start_args>\n\n");
  fprintf(stderr, "\t\t <start_mode>:\n");
  fprintf(stderr, "\t\t\t IMMEDIATELY\n");
  fprintf(stderr, "\t\t\t AFTER TIME <HH:MM:SS>\n");
  fprintf(stderr, "\t\t\t AFTER GROUP G1 [HH:MM:SS]\n");
  exit(-1);
}*/

/** 
 * SCHEDULE <group_name> <phase_name> RAMP_UP <num_users/sessions rate> <ramp_up_mode> <mode_based_args>
 * 
 * FCU Scenario Type Ramp Up Modes
 * IMMEDIATELY
 * STEP <step_users> <step_time in HH:MM:SS>
 * RATE <ramp_up_rate> <rate_unit (H/M/S)> <ramp_up_pattern>
 *         <ramp_up_pattern>:
 *         	LINEARLY
 *         	RANDOMLY
 * 
 * TIME <ramp_up_time> <ramp_up_pattern>
 *         <ramp_up_pattern>: 
 *                 LINEARLY
 *                 RANDOMLY
 * 
 * FSR Scenario Type Ramp Up Mode
 * IMMEDIATELY
 * TIME_SESSIONS <ramp_up_time in HH:MM:SS> <mode> <step_time or num_steps> 
 *        <mode> 0|1|2 (default steps, steps of x seconds, total steps)
 * 
 */
/*void usage_parse_schedule_phase_ramp_up(char *buf)
{
  if (buf != NULL)
    fprintf(stderr, "Error:\n%s\n", buf);
  fprintf(stderr, "Syntax\n");

  fprintf(stderr, "\t\t SCHEDULE <group_name> <phase_name> RAMP_UP <num_users/sessions rate> <ramp_up_mode> <mode_based_args>\n");
  fprintf(stderr, "\t\t FCU Scenario Type Ramp Up Modes\n");
  fprintf(stderr, "\t\t\t IMMEDIATELY\n");
  fprintf(stderr, "\t\t\t STEP <step_users> <step_time in HH:MM:SS>\n");
  fprintf(stderr, "\t\t\t RATE <ramp_up_rate> <rate_unit (H/M/S)> <ramp_up_pattern>\n");
  fprintf(stderr, "\t\t\t\t <ramp_up_pattern>:\n");
  fprintf(stderr, "\t\t\t\t\t LINEARLY\n");
  fprintf(stderr, "\t\t\t\t\t RANDOMLY\n");
  fprintf(stderr, "\t\t\t TIME <ramp_up_time> <ramp_up_pattern>\n");
  fprintf(stderr, "\t\t\t\t <ramp_up_pattern>: \n");
  fprintf(stderr, "\t\t\t\t\t LINEARLY\n");
  fprintf(stderr, "\t\t\t\t\t RANDOMLY\n");
  fprintf(stderr, "\t\t FSR Scenario Type Ramp Up Mode\n");
  fprintf(stderr, "\t\t\t IMMEDIATELY\n");
  fprintf(stderr, "\t\t\t TIME_SESSIONS <ramp_up_time in HH:MM:SS> <mode> <step_time or num_steps> \n");
  fprintf(stderr, "\t\t\t\t <mode> 0|1|2 (default steps, steps of x seconds, total steps)\n");

  exit(-1);
}*/

/**
 * DURATION <duration_mode> <args>
 * Duration_mode:
 *
 * INDEFINITE (default)
 * TIME HH:MM:SS
 * SESSIONS <num_sessions>
 *
 */
/*void usage_parse_schedule_phase_duration(char *buf)
{
  if (buf != NULL)
    fprintf(stderr, "Error:\n%s\n", buf);
  fprintf(stderr, "Syntax\n");
  fprintf(stderr, "\t\t DURATION <duration_mode> <args>\n");
  fprintf(stderr, "\t\t\t Duration_mode:\n");
  fprintf(stderr, "\t\t\t\t INDEFINITE (default)\n");
  fprintf(stderr, "\t\t\t\t TIME HH:MM:SS\n");
  fprintf(stderr, "\t\t\t\t SESSIONS <num_sessions> <per_user_session_flag>\n");
  exit(-1);
}*/

/**
 * SCHEDULE G1 None STABILIZATION TIME <HH:MM:SS>
 */
/*void usage_parse_schedule_phase_stabilize(char *buf)
{
  if (buf != NULL)
    fprintf(stderr, "Error:\n%s\n", buf);
  fprintf(stderr, "Syntax\n");
  fprintf(stderr, "\t SCHEDULE G1 None STABILIZATION TIME <HH:MM:SS>\n");
  exit(-1);
}*/

/**
 * SCHEDULE <group_name> <phase_name> RAMP_DOWN <num_users/sessions rate> <ramp_down_mode> <mode_based_args>
 *
 * IMMEDIATELY (Default)
 * TIME <ramp_down_time in HH:MM:SS> <ramp_down_pattern>
 *
 * STEP <ramp-down-step-users> <ramp-down-step-time in HH:MM:SS> (For FCU)
 * Or
 * STEP < ramp_down_step_sessions> < ramp-down-step-time in HH:MM:SS> (For FSR)
 *
 */
/*void usage_parse_schedule_phase_ramp_down(char *buf)
{
  if (buf != NULL)
    fprintf(stderr, "Error:\n%s\n", buf);
  fprintf(stderr, "Syntax\n");
  fprintf(stderr, "\t\t SCHEDULE <group_name> <phase_name> RAMP_DOWN <num_users/sessions rate> <ramp_down_mode> <mode_based_args>\n");
  fprintf(stderr, "\t\t\t IMMEDIATELY (Default)\n");
  fprintf(stderr, "\t\t\t TIME <ramp_down_time in HH:MM:SS> <ramp_down_pattern>\n");
  fprintf(stderr, "\t\t\t STEP <ramp-down-step-users> <ramp-down-step-time in HH:MM:SS> (For FCU)\n");
  fprintf(stderr, "\t\t\t Or\n");
  fprintf(stderr, "\t\t\t STEP < ramp_down_step_sessions> < ramp-down-step-time in HH:MM:SS> (For FSR)\n");
  exit(-1);
}*/

/**
 * SCHEDULE <GroupName> <PhaseName> <PhaseType> <PhaseParameters>
 *
 * Phase Type:
 * START
 * RAMP_UP
 * DURATION
 * STABILIZATION
 * RAMP_DOWN
 */
/*void usage_kw_set_schedule(char *buf)
{
  if (buf != NULL)
    fprintf(stderr, "Error:\n%s\n", buf);
  fprintf(stderr, "Syntax\n");
  fprintf(stderr, "\t\t SCHEDULE <GroupName> <PhaseName> <PhaseType> <PhaseParameters>\n");
  fprintf(stderr, "\t\t Phase Type:\n");
  fprintf(stderr, "\t\t\t START\n");
  fprintf(stderr, "\t\t\t RAMP_UP\n");
  fprintf(stderr, "\t\t\t DURATION\n");
  fprintf(stderr, "\t\t\t STABILIZATION\n");
  fprintf(stderr, "\t\t\t RAMP_DOWN\n");

  exit(-1);
}
*/

/**
 * SCHEDULE_TYPE <Simple or Advanced>
 */
int kw_set_schedule_type(char *buf, char *err_msg)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char schedule_type[MAX_DATA_LINE_LENGTH];
  int num;

  num = sscanf(buf, "%s %s", keyword, schedule_type);
  if (num < 2) {
   NS_KW_PARSING_ERR(buf, 0, err_msg, SCHEDULE_TYPE_USAGE, CAV_ERR_1011175, CAV_ERR_MSG_1);
   // fprintf(stderr, "keyword = %s, must have one argument\n", keyword);
    //usage_kw_set_schedule_type(NULL);
  }
  
  if (strcasecmp(schedule_type, "Simple") == 0) {
    global_settings->schedule_type = SCHEDULE_TYPE_SIMPLE;
  } else if (strcasecmp(schedule_type, "Advanced") == 0) {
    global_settings->schedule_type = SCHEDULE_TYPE_ADVANCED;
  } else {                      /* Unknown type */
    NS_KW_PARSING_ERR(buf, 0, err_msg, SCHEDULE_TYPE_USAGE, CAV_ERR_1011338, schedule_type);
    //fprintf(stderr, "keyword %s can only have values \"Simple\" or \"Advanced\"\n", keyword);
    //usage_kw_set_schedule_type(NULL);
  }

  NSDL2_SCHEDULE(NULL, NULL, "setting global_settings->schedule_type = %d", global_settings->schedule_type);
  return 0;
}

/**
 * SCHEDULE_BY  <Scenario or Group>
 */
int kw_set_schedule_by(char *buf, char *err_msg)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char schedule_by[MAX_DATA_LINE_LENGTH];
  int num;

  num = sscanf(buf, "%s %s", keyword, schedule_by);
  if (num < 2) {
   NS_KW_PARSING_ERR(buf, 0, err_msg, SCHEDULE_BY_USAGE, CAV_ERR_1011176, CAV_ERR_MSG_1);
  //  fprintf(stderr, "keyword = %s, must have one argument\n", keyword);
   // usage_kw_set_schedule_by(NULL);
  }
  
  if (strcasecmp(schedule_by, "Scenario") == 0) {
    global_settings->schedule_by = SCHEDULE_BY_SCENARIO;
  } else if (strcasecmp(schedule_by, "Group") == 0) {
    global_settings->schedule_by = SCHEDULE_BY_GROUP;
  } else {                      /* Unknown schedule */
    NS_KW_PARSING_ERR(buf, 0, err_msg, SCHEDULE_BY_USAGE, CAV_ERR_1011339, schedule_by);
   // fprintf(stderr, "keyword %s can only have values \"Scenario\" or \"Group\"\n", keyword);
   // usage_kw_set_schedule_by(NULL);
  }

  NSDL2_SCHEDULE(NULL, NULL, "setting global_settings->schedule_by = %d", global_settings->schedule_by);
  return 0;
}

/**
 * PROF_PCT_MODE  NUM or PCT or NUM_AUTO 
 *
 * NUM and NUM_AUTO mean the same for netstorm.
 *
 * Validations:
 * o    For STYPE MIXED, pct is not allowed.
 * o    For STYPE FSR or FCU, Advanced (SCHEDULE_TYPE_ADVANCED) PCT mode is not allowed in Group Based (SCHEDULE_BY GROUP)
 */
int kw_set_prof_pct_mode(char *buf, char *err_msg)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char value[MAX_DATA_LINE_LENGTH];
  //int num;
  
  //num = sscanf(buf, "%s %s", keyword, value);
  sscanf(buf, "%s %s", keyword, value);
  
  if (strcmp(value, "NUM") == 0) {
    global_settings->use_prof_pct = PCT_MODE_NUM;
  } else if (strcmp(value, "PCT") == 0) {
    /* Validations */
    if (testCase.mode == TC_MIXED_MODE) { 
        NS_KW_PARSING_ERR(buf, 0, err_msg,PROF_PCT_MODE_USAGE , CAV_ERR_1011169, "For Mixed mode scenario, pct mode is not allowed");
     // fprintf(stderr, "For Mixed mode scenarios, pct mode is not allowed.\n");
      //usage_kw_set_prof_pct_mode(NULL);
    } else if ((global_settings->schedule_type == SCHEDULE_TYPE_ADVANCED) &&
               (global_settings->schedule_by == SCHEDULE_BY_GROUP)) {
    NS_KW_PARSING_ERR(buf, 0, err_msg, PROF_PCT_MODE_USAGE , CAV_ERR_1011169, "In case of Schedule Type 'Advanced' and Schedule By "
                      "'Group' based scenario, pct mode is not allowed");
     // fprintf(stderr, "For Advanced and Group based schedule, PCT mode is not allowed\n");
     // usage_kw_set_prof_pct_mode(NULL);
    }

    global_settings->use_prof_pct = PCT_MODE_PCT;
  } else if (strcmp(value, "NUM_AUTO") == 0) {
    global_settings->use_prof_pct = PCT_MODE_NUM_AUTO;
  } else {
    NS_KW_PARSING_ERR(buf, 0, err_msg, PROF_PCT_MODE_USAGE, CAV_ERR_1011169, CAV_ERR_MSG_3);
   // fprintf(stderr, "Invalid argument to %s. Valid arguments are \"NUM\", \"PCT\", or \"NUM_AUTO\"\n", keyword);
   // usage_kw_set_prof_pct_mode(NULL);
  }
  return 0;
}

/**
 * In following functions buf contains the schedule line from the scenario
 * starting from PhaseType onwards
 * grp_idx -1 means SCHEDULE_TYPE is scenario based and we have to populate 
 * a common phase array.
 */

/**
 * START <start_mode> <args>
 *
 * //SCHEDULE <group_name> <phase_name> START <start_mode> <start_args>
 *
 * IMMEDIATELY    (Start from beginning of testrun) (Default)
 * AFTER TIME HH:MM:SS
 * AFTER GROUP G1 [HH:MM:SS]
 *
 * Validations:
 * o    More than one start phases for one group is not permitted.
 */
int parse_schedule_phase_start(int grp_idx, char *full_buf, char *phase_name, char *err_msg, int set_rtc_flag)
{
  Phases *tmp_ph;
  char *fields[20];
  char buf_backup[MAX_DATA_LINE_LENGTH];
  int dependent_grp_idx;
  int num_fields;
  char *buf;
  char *buf_more;


  if (full_buf[strlen(full_buf) - 1] == '\n') {
    full_buf[strlen(full_buf) - 1] = '\0';
  }

  strcpy(buf_backup, full_buf);      /* keep a backup to print */

  buf = strstr(full_buf, "START");

  /* Check if this is the last one */
  buf_more = buf;
  while (buf_more != NULL) {
    buf_more += strlen("START");
    buf_more = strstr(buf_more, "START");
    if (buf_more) buf = buf_more;
  }

  /* Start phase will always be there. */
  if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO) {
    tmp_ph = &(scenario_schedule->phase_array[0]);
  } else {
    tmp_ph = &(group_schedule[grp_idx].phase_array[0]);
  }
  
  /* By now, we already have start phase, we overwrite those. Exit if it has been overwritten before */
  if (!(tmp_ph->default_phase)) {
    NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011310,
                      "Can not have more than one 'Start' phase for Schedule By 'Group'");
  //  fprintf(stderr, "Can not have more than one start phases for %s\n", 
          //  global_settings->schedule_by == SCHEDULE_BY_SCENARIO ? "Scenario" : "Group");
   // usage_parse_schedule_phase_start(NULL);
  } /* else overwrite */

  tmp_ph->default_phase = 0;
  strncpy(tmp_ph->phase_name, phase_name, sizeof(tmp_ph->phase_name) - 1);
  
  num_fields = get_tokens(buf, fields, " ", 20);
  
  if (num_fields < 2) {
    NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011310,
                      "Atleast one argument required after 'START' for 'Start' Phase");
    //fprintf(stderr, "Atleast one argument required after START\n");
    //usage_parse_schedule_phase_start(NULL);
  }
  
  if (strcmp(fields[1], "IMMEDIATELY") == 0) {
    tmp_ph->phase_cmd.start_phase.time = 0;
    tmp_ph->phase_cmd.start_phase.dependent_grp = -1;
  } else if (strcmp(fields[1], "AFTER") == 0) {
    
    if (num_fields < 3) {
      NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011311,
                         "Atleast one argument required after 'AFTER' for 'Start' phase");

     // fprintf(stderr, "Atleast one argument required after AFTER in [%s]\n", buf_backup);
     // usage_parse_schedule_phase_start(NULL);
    }
    
    if (strcmp(fields[2], "TIME") == 0) {

      if (num_fields < 4) {
         NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011312, "Invalid time format for 'Start' phase");
       // fprintf(stderr, "Time in format HH:MM:SS required after TIME in [%s]\n", buf_backup);
       // usage_parse_schedule_phase_start(NULL);
      }
    
      tmp_ph->phase_cmd.start_phase.time = get_time_from_format(fields[3]);
      if (tmp_ph->phase_cmd.start_phase.time < 0) {
       NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011312,
                            "Invalid time format for 'Start' phase in case of start after time.");

        //fprintf(stderr, "Invalid time format in %s\n", buf_backup);
       // usage_parse_schedule_phase_start(NULL);
        
      }
      tmp_ph->phase_cmd.start_phase.dependent_grp = -1;
      NSDL4_SCHEDULE(NULL, NULL, "parsing grp time  = %u\n", 
                     tmp_ph->phase_cmd.start_phase.time);

    } else if (strcmp(fields[2], "GROUP") == 0) {
      
        if (num_fields < 4) {
        NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011313,
                          "Group name must be specified for 'Start' phase in case of start after group.");
       // fprintf(stderr, "Group must be specified after GROUP in [%s]\n", buf_backup);
       // usage_parse_schedule_phase_start(NULL);
      }
      /* Changes done for NS supports SGRP groups with quantity/pct zero
       * Earlier find_sg_idx used to return group idx(success case) or -1(group-name ALL)
       * This function has been modified and now it returns -2(invalid group name), hence 
       * modified validation check */
      if ((dependent_grp_idx = find_sg_idx(fields[3])) >= 0) {
        tmp_ph->phase_cmd.start_phase.dependent_grp = dependent_grp_idx;
        //if (fields[4] != NULL)  /* time */

        if (num_fields > 5) {
            NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011313,
                             "Invalid number of argument for 'Start' phase in case of start after group.");
            // usage_parse_schedule_phase_start(NULL);
        }

        if (num_fields == 5) { /* time */
          tmp_ph->phase_cmd.start_phase.time = get_time_from_format(fields[4]);
          if (tmp_ph->phase_cmd.start_phase.time < 0) {
             NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011313,
                              "Invalid time format for 'Start' phase in case of start after group with delay.");

           // fprintf(stderr, "Invalid time format in %s\n", buf_backup);
          }
        } else {
          tmp_ph->phase_cmd.start_phase.time = 0;
        }

      } else {
        NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011313, "Invalid group name for 'Start' phase.");
         //  fprintf(stderr, "Invalid group in %s\n", buf_backup);
       // usage_parse_schedule_phase_start(NULL);
      }
    } else {
     NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011310, "Invalid option for 'Start' phase.");
      //  fprintf(stderr, "Invalid option with START (%s)\n", buf_backup);
     // usage_parse_schedule_phase_start(NULL);
    }
  }
  return 0;
}

/**
 * RAMP_UP <num_users/sessions> <ramp_up_mode> <mode_based_args>
 *      <num_users/sessions> can also be ALL
 *
 * FCU Scenario Type Ramp Up Modes:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * IMMEDIATELY
 * STEP <step_users> <step_time in HH:MM:SS>
 * RATE <ramp_up_rate> <rate_unit (H/M/S)> <ramp_up_pattern>
 * TIME <ramp_up_time> <ramp_up_pattern>
 *
 * FSR:
 * ~~~
 * IMMEDIATELY
 * TIME_SESSIONS <ramp_up_time in HH:MM:SS> <mode> <step_time or num_steps> 
 *       <mode> 0|1|2 (default steps, steps of x seconds, total steps)
 *
 *
 * Validations:
 * o    Number of users rampup can never be more than the users defined.
 * o    If scenario based scheduling is used, then some ramp up mode will not apply. 
 *      Only Ramp up time or immediate will apply.
 * o    If group based, then all ramp up mode will apply based on the scenario type.
 * o    Only once for simple schedule
 * o    0 or more time for advanced scheduler
 * o    Can occur of at least 1 user left to be ramped up.
 */
int parse_schedule_phase_ramp_up(int grp_idx, char *full_buf, char *phase_name, char *err_msg, int set_rtc_flag)
{
  Phases *ph, *tmp_ph;
  char *fields[20];
  char buf_backup[MAX_DATA_LINE_LENGTH];
  int group_mode;
  Schedule *schedule;
  char *buf, *buf_more;
  int num_fields;
  char schedule_err[1024];

  NSDL2_SCHEDULE(NULL, NULL, "Parsing RampUp: grp_idx = %d, phase_name = %s, buf = %s", grp_idx, phase_name, full_buf);

  if (full_buf[strlen(full_buf) - 1] == '\n') {
    full_buf[strlen(full_buf) - 1] = '\0';
  }
 
  strcpy(buf_backup, full_buf);      /* keep a backup to print */
  buf = strstr(full_buf, "RAMP_UP");

  /* Check if this is the last one */
  buf_more = buf;
  while (buf_more != NULL) {
    buf_more += strlen("RAMP_UP");
    buf_more = strstr(buf_more, "RAMP_UP");
    if (buf_more) buf = buf_more;
  }
  
  if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO) {
    schedule = scenario_schedule;
  } else {
    schedule = &group_schedule[grp_idx];
  }

  if (global_settings->schedule_type == SCHEDULE_TYPE_SIMPLE) {
    ph = &(schedule->phase_array[1]);                /* 2nd Phase Ramp UP */
    if (ph->default_phase)
      tmp_ph = ph;
    else {
    NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011314,
                      "Can not have more than one Ramp Up phase in Simple Schedule.");
     // fprintf(stderr, "Can not have more than one Ramp Up phase in Simple Schedule. Line [%s]\n", buf_backup);
      //usage_parse_schedule_phase_ramp_up(NULL);
    }
    strncpy(ph->phase_name, phase_name, sizeof(ph->phase_name) - 1);
  } else {                         /* add Another */
    tmp_ph = add_schedule_phase(schedule, SCHEDULE_PHASE_RAMP_UP, phase_name);   /* This adds in the end and returns 
                                                                                  * the pointer of newly added node */
  }

  num_fields = get_tokens(buf, fields, " ", 20);
  group_mode = get_group_mode(grp_idx);

  if (num_fields < 3) {
   NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011314, "Invalid number of argument for 'Ramp Up' phase.");
  //  fprintf(stderr, "Atleast two arguments required with RAMP_UP, in line [%s]\n", buf_backup);
    //usage_parse_schedule_phase_ramp_up(NULL);
  }

  NSDL2_SCHEDULE(NULL, NULL, "group_mode = %d, num_fields = %d", group_mode, num_fields);

  tmp_ph->default_phase = 0;
  /* Validation */
  if (group_mode == TC_FIX_CONCURRENT_USERS) {
    if (strcmp(fields[2], "IMMEDIATELY") && strcmp(fields[2], "STEP") &&
        strcmp(fields[2], "RATE") && strcmp(fields[2], "TIME")) 
    {
      snprintf(schedule_err, 1024, "Phase '%s' given in Global Scenario is not a valid phase. "
                                   "Phase name can only IMMEDIATELY, STEP, RATE or TIME.", fields[2]);
      NS_KW_PARSING_ERR(buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011308, schedule_err); 
    //  fprintf(stderr, "Unknown Ramp Up mode for Fixed Concurrent Users scenarios. "
      //        "It can be : IMMEDIATELY, STEP, RATE or TIME\n");
     // usage_parse_schedule_phase_ramp_up(NULL);
    }
  } else if (group_mode == TC_FIX_USER_RATE) {
    if (strcmp(fields[2], "IMMEDIATELY") && strcmp(fields[2], "TIME_SESSIONS")) 
    {
      snprintf(schedule_err, 1024, "Phase '%s' given in Global Scenario is not a valid phase. "
                                   "Phase name can only IMMEDIATELY or TIME_SESSIONS.", fields[2]);
      NS_KW_PARSING_ERR(buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011308, schedule_err); 
      //fprintf(stderr, "Unknown Ramp Up mode for Fixed Sessions Rate scenarios. It can be : IMMEDIATELY or TIME_SESSIONS\n");
     // usage_parse_schedule_phase_ramp_up(NULL);
    }
  }
  else if (group_mode >= TC_FIX_HIT_RATE && group_mode <= TC_FIX_MEAN_USERS) {
     if (strcmp(fields[2], "IMMEDIATELY") && strcmp(fields[2], "TIME_SESSIONS")) {
      snprintf(schedule_err, 1024, "Phase '%s' given in Global Scenario is not a valid phase. "
                                   "Phase name can only IMMEDIATELY or TIME_SESSIONS.", fields[2]);
      NS_KW_PARSING_ERR(buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011308, schedule_err);
     }
  }

  char *users_or_sess_rate = fields[1];
  char *ramp_up_mode = fields[2];
  
  NSDL2_SCHEDULE(NULL, NULL, "users_or_sess_rate = %s, ramp_up_mode = %s", users_or_sess_rate, ramp_up_mode);

  /* Users_or_sess_rate can only be ALL in Simple mode and > 0 for others */
  if ((global_settings->schedule_type == SCHEDULE_TYPE_SIMPLE) &&
      strcmp(users_or_sess_rate, "ALL")) {
     NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011314, "User or Session rate can only be 'ALL' "
                      "for Schedule Type 'Simple' in 'Ramp Up' phase.");
    // fprintf(stderr, "For Simple Schedule, users_or_sess_rate can only be ALL\n");
    //usage_parse_schedule_phase_ramp_up(NULL);
  } else if (global_settings->schedule_type == SCHEDULE_TYPE_ADVANCED) {
    if (!strcmp(users_or_sess_rate, "ALL")) {
     NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011314, "User or session rate can not be 'ALL' "
                        "for Schedule Type 'Advance' in 'Ramp Up' phase.");
     //  fprintf(stderr, "For Advanced Schedule, users_or_sess_rate can not be ALL\n");
     // usage_parse_schedule_phase_ramp_up(NULL);
    } else if (atoi(users_or_sess_rate) <= 0) {  
     NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011314, "User or Session rate specified for "
                        "Schedule Type Advanced can not be less than or equal to zero.");
     // fprintf(stderr, "Users_or_sess_rate specified for Advanced Schedule can not be less than or equal to zero\n");
     // usage_parse_schedule_phase_ramp_up(NULL);
    }
  }

  /* Fill mode */
  if (strcmp(ramp_up_mode, "IMMEDIATELY") == 0) {
    tmp_ph->phase_cmd.ramp_up_phase.ramp_up_mode = RAMP_UP_MODE_IMMEDIATE;
    schedule->ramp_up_define = 1; //Setting flag if either of ramp-up method used
  } else if (strcmp(ramp_up_mode, "STEP") == 0) {
    
    if (num_fields < 5) {
     NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011315,
                        "Invalid number of arguments with 'STEP' mode for 'Ramp Up' phase.");
     // fprintf(stderr, "Atleast two arguments required with STEP, in line [%s]\n", buf_backup);
     // usage_parse_schedule_phase_ramp_up(NULL);
    }

    tmp_ph->phase_cmd.ramp_up_phase.ramp_up_mode = RAMP_UP_MODE_STEP;
    tmp_ph->phase_cmd.ramp_up_phase.ramp_up_step_users = atoi(fields[3]);
    tmp_ph->phase_cmd.ramp_up_phase.ramp_up_step_time = get_time_from_format(fields[4]);
    if (tmp_ph->phase_cmd.ramp_up_phase.ramp_up_step_time < 0) {
    NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011315,
                            "Time specified with 'STEP' mode is invalid for 'Ramp Up' phase.");
     // fprintf(stderr, "Time specified with STEP is invalid. Line [%s]\n", buf_backup);
     // usage_parse_schedule_phase_ramp_up(NULL);
    }
    schedule->ramp_up_define = 1; //Setting flag if either of ramp-up method used
  } else if (strcmp(ramp_up_mode, "RATE") == 0) {
    char tmp_buf[1024];
    tmp_ph->phase_cmd.ramp_up_phase.ramp_up_mode = RAMP_UP_MODE_RATE;

    if (num_fields < 5) {
     NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011316,
                        "Invalid number of argument with 'RATE' mode for 'Ramp Up' phase.");

     // fprintf(stderr, "Atleast two arguments required with RATE, in line [%s]\n", buf_backup);
     // usage_parse_schedule_phase_ramp_up(NULL);
    }

    sprintf(tmp_buf, "DUMMY_KW %s %s", fields[3], fields[4]); /* Sending dummy kw */
    tmp_ph->phase_cmd.ramp_up_phase.ramp_up_rate = convert_to_per_minute(tmp_buf); /* Decimal howto ?? */

    /* pattern */
    if(num_fields < 6)
    {
        NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011316,
                        "Ramp Up pattern is not specified for 'RATE' mode. It should be LINEARLY or RANDOMLY.");
      //fprintf(stderr, "<ramp_up_pattern>  not specified. Should be LINEARLY or RANDOMLY, in line [%s]\n", buf_backup);
      // usage_parse_schedule_phase_ramp_up(NULL);
    }
    else if (strcmp(fields[5], "LINEARLY") == 0) {
      tmp_ph->phase_cmd.ramp_up_phase.ramp_up_pattern = RAMP_UP_PATTERN_LINEAR;
    } else if (strcmp(fields[5], "RANDOMLY") == 0) {
      //tmp_ph->phase_cmd.ramp_up_phase.ramp_up_pattern = RAMP_UP_PATTERN_RANDOM;
      tmp_ph->phase_cmd.ramp_up_phase.ramp_up_pattern = RAMP_UP_PATTERN_LINEAR;
    } else {                    /* Unknown pattern */
        snprintf(schedule_err, 1024, "Ramp Up pattern '%s' is not a valid pattern "
                                     "for 'RATE' mode. It can be LINEARLY or RANDOMLY.", fields[5]);
        NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011316, schedule_err);
      // fprintf(stderr, "Unknown Pattern %s, in line [%s]\n", fields[5], buf_backup);
     // usage_parse_schedule_phase_ramp_up(NULL);
    }
    schedule->ramp_up_define = 1; //Setting flag if either of ramp-up method used

  } else if (strcmp(ramp_up_mode, "TIME") == 0) {

    if (num_fields < 5) {
    NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011317,
                        "Invalid number of argument provided in 'TIME' mode for 'Ramp Up' phase");
     // fprintf(stderr, "Atleast two arguments required with TIME, in line [%s]\n", buf_backup);
     // usage_parse_schedule_phase_ramp_up(NULL);
    }

    tmp_ph->phase_cmd.ramp_up_phase.ramp_up_mode = RAMP_UP_MODE_TIME;
    tmp_ph->phase_cmd.ramp_up_phase.ramp_up_time = get_time_from_format(fields[3]);

    if (tmp_ph->phase_cmd.ramp_up_phase.ramp_up_time < 0) {
      NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011317,
                          "Time specified with 'TIME' mode for 'Ramp Up' phase is invalid.");
     // fprintf(stderr, "Time specified is invalid in line [%s]\n", buf_backup);
     // usage_parse_schedule_phase_ramp_up(NULL);
    } else if (tmp_ph->phase_cmd.ramp_up_phase.ramp_up_time == 0)  // Ramp up time is 0 then do immediate
      tmp_ph->phase_cmd.ramp_up_phase.ramp_up_mode = RAMP_UP_MODE_IMMEDIATE;

    if (strcmp(fields[4], "LINEARLY") == 0) {
      tmp_ph->phase_cmd.ramp_up_phase.ramp_up_pattern = RAMP_UP_PATTERN_LINEAR;
    } else if (strcmp(fields[4], "RANDOMLY") == 0) {
      //tmp_ph->phase_cmd.ramp_up_phase.ramp_up_pattern = RAMP_UP_PATTERN_RANDOM;
      tmp_ph->phase_cmd.ramp_up_phase.ramp_up_pattern = RAMP_UP_PATTERN_LINEAR;
    } else {                    /* Unknown pattern */
        snprintf(schedule_err, 1024, "Ramp Up pattern '%s' is not a valid pattern "
                                     "for 'TIME' mode. It can be LINEARLY or RANDOMLY", fields[4]);
    NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011317, schedule_err);
      // fprintf(stderr, "Unknown Pattern %s, in line [%s]\n", fields[4], buf_backup);
     // usage_parse_schedule_phase_ramp_up(NULL);
    }
    schedule->ramp_up_define = 1; //Setting flag if either of ramp-up method used
    
  } else if (strcmp(ramp_up_mode, "TIME_SESSIONS") == 0) {
    
    if (num_fields < 5) {
    NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011318, 
                      "Invalid number of argument with 'TIME_SESSIONS' mode for 'Ramp Up' phase");
      //fprintf(stderr, "Atleast two arguments required with TIME_SESSIONS, in line [%s]\n", buf_backup);
      //usage_parse_schedule_phase_ramp_up(NULL);
    }

    /* TIME_SESSIONS <ramp_up_time in HH:MM:SS> <mode> <step_time or num_steps> 
       <mode> 0|1|2 (default steps, steps of x seconds, total steps) */
    tmp_ph->phase_cmd.ramp_up_phase.ramp_up_mode = RAMP_UP_MODE_TIME_SESSIONS;
    tmp_ph->phase_cmd.ramp_up_phase.ramp_up_time = get_time_from_format(fields[3]);
    if (tmp_ph->phase_cmd.ramp_up_phase.ramp_up_time < 0) {
    NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011318,
                           "Ramp Up step time with 'TIME_SESSIONS' mode is not valid time.");
     // fprintf(stderr, "Time specified is invalid in line [%s]\n", buf_backup);
     // usage_parse_schedule_phase_ramp_up(NULL);
    } else if (tmp_ph->phase_cmd.ramp_up_phase.ramp_up_time == 0)  { // Ramp up time is 0 then do immediate
      tmp_ph->phase_cmd.ramp_up_phase.ramp_up_mode = RAMP_UP_MODE_IMMEDIATE;
    }
/* ramp_up_time  tot_num_steps_for_sessions 
 * 0-179   secs    2
 * 180-239 secs    3
 * 240-299 secs    4
 */
    if (tmp_ph->phase_cmd.ramp_up_phase.ramp_up_time > 0) {
      double temp;
      int step_mode = atoi(fields[4]);
      if (step_mode == 0) {
        tmp_ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions = 2;
        int floor = tmp_ph->phase_cmd.ramp_up_phase.ramp_up_time/(1000*60); // chk it once
        if (floor > 1)
          tmp_ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions = floor;
        temp = (double)(tmp_ph->phase_cmd.ramp_up_phase.ramp_up_time/1000)/(double)tmp_ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions;
        tmp_ph->phase_cmd.ramp_up_phase.ramp_up_step_time = round(temp);
      }
      else if (step_mode == 1) {
        if (num_fields < 6) {
          NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011318,
                            "Step time or num steps are not specified with 'TIME_SESSIONS' mode for 'Ramp Up' phase.");
         // fprintf(stderr, "Step time or num steps should be specified, in line [%s]\n", buf_backup);
         // usage_parse_schedule_phase_ramp_up(NULL);
        }
        tmp_ph->phase_cmd.ramp_up_phase.ramp_up_step_time = atoi(fields[5]);
        if(tmp_ph->phase_cmd.ramp_up_phase.ramp_up_step_time <= 0){
          NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011318,
                                "Ramp Up step time with 'TIME_SESSIONS' mode is not valid time.");
         // NS_EXIT(-1, "Invalid step time '%d' at line [%s]", tmp_ph->phase_cmd.ramp_up_phase.ramp_up_step_time, buf_backup);
        }          
        if((tmp_ph->phase_cmd.ramp_up_phase.ramp_up_time/1000) < tmp_ph->phase_cmd.ramp_up_phase.ramp_up_step_time){
          NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011318,
                                "Ramp Up time can not be less then step time for 'TIME_SESSIONS' mode.");

        //  NS_EXIT(-1, "Ramp Up time can not be less then step time at line [%s]", buf_backup);
        }          
        temp = (double)(tmp_ph->phase_cmd.ramp_up_phase.ramp_up_time/1000)/(double)tmp_ph->phase_cmd.ramp_up_phase.ramp_up_step_time;
        tmp_ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions = round(temp);
      }
      else if (step_mode == 2) {
        if (num_fields < 6) {
         NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011318,
                              "Num steps with 'TIME_SESSIONS' mode is not specified for 'Ramp Up' phase.");
            //usage_parse_schedule_phase_ramp_up("Step time or num steps should be specified");

         // fprintf(stderr, "Step time or num steps should be specified, in line [%s]\n", buf_backup);
         // usage_parse_schedule_phase_ramp_up(NULL);
        }
        tmp_ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions = atoi(fields[5]);
        NSDL4_SCHEDULE(NULL, NULL, "tot_num_steps_for_sessions=%d", tmp_ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions);
        if(tmp_ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions <= 0){
         NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011318,
                              "Num steps can not less than or equal to zero with 'TIME_SESSIONS' mode for 'Ramp Up' phase.");
         // NS_EXIT(-1, "Invalid num steps '%d' at line [%s]", tmp_ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions, buf_backup);
        }          
        temp = (double)(tmp_ph->phase_cmd.ramp_up_phase.ramp_up_time/1000)/(double)tmp_ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions;
        // step time can not be 0 
        tmp_ph->phase_cmd.ramp_up_phase.ramp_up_step_time = (round(temp))?round(temp):1;
      }
      else {
        NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011318,
                            "Invalid step mode is selected with 'TIME_SESSIONS' mode for 'Ramp Down' phase.");
           // NS_EXIT(-1, "Invalid step mode specified in line [%s]\n", buf_backup);
      }
    }    
    schedule->ramp_up_define = 1; //Setting flag if either of ramp-up method used
  } 
  //Added check for netomni
  if (loader_opcode == CLIENT_LOADER) {
    if (global_settings->schedule_by == SCHEDULE_BY_GROUP) {
      /* NOTE: In CLIENT_LOADER only NUM mode is supported therefore verfying only quantity field of runProfTable*/
      if (runProfTable[grp_idx].quantity == 0) {
        NSDL4_SCHEDULE(NULL, NULL, "Making users_or_sess_rate = 0 for group_idx [%d], group with quantity given zero", grp_idx);
        strcpy(users_or_sess_rate, "0");
      }
    }
  }

  /* Fill users_or_sess_rate */
  if (strcmp(users_or_sess_rate, "ALL") == 0) {
    tmp_ph->phase_cmd.ramp_up_phase.num_vusers_or_sess = -1; /* Fill -1 for ALL for now, later filled during validation */
  } else if (group_mode == TC_FIX_USER_RATE) { /* FSR */
  /*In regard to BUG 9078, 
      FSR advance scenario, SCHEDULE ALL RampUp3 RAMP_UP 32.364 TIME_SESSIONS 00:30:00 0
      When storing a double variable into integer it might have floating point error.*/
    tmp_ph->phase_cmd.ramp_up_phase.num_vusers_or_sess = compute_sess_rate(users_or_sess_rate); 
  } else {
    tmp_ph->phase_cmd.ramp_up_phase.num_vusers_or_sess = atoi(users_or_sess_rate);
  }
  return 0;
}

/**
 * DURATION <duration_mode> <args>
 *
 * Duration_mode:
 * INDEFINITE (default)         - 0
 * TIME HH:MM:SS                - 1
 * SESSIONS <num_sessions>      - 2
 *
 * Validations: None
 */
int parse_schedule_phase_duration(int grp_idx, char *full_buf, char *phase_name, char *err_msg, int set_rtc_flag)
{
  Phases *ph, *tmp_ph;
  char *fields[20] = {0};
  char buf_backup[MAX_DATA_LINE_LENGTH];
  Schedule *schedule;
  char *buf;
  char *buf_more;
  int num_fields;
  char schedule_err[1024];

  if (full_buf[strlen(full_buf) - 1] == '\n') {
    full_buf[strlen(full_buf) - 1] = '\0';
  }

  strcpy(buf_backup, full_buf);      /* keep a backup to print */
  buf = strstr(full_buf, "DURATION");

  /* Check if this is the last one */
  buf_more = buf;
  while (buf_more != NULL) {
    buf_more += strlen("DURATION");
    buf_more = strstr(buf_more, "DURATION");
    if (buf_more) buf = buf_more;
  }
  
  if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO) {
    schedule = scenario_schedule;
  } else {
    schedule = &group_schedule[grp_idx];
  }

  if (global_settings->schedule_type == SCHEDULE_TYPE_SIMPLE) {
    ph = &(schedule->phase_array[3]);                /* 4rd Phase Duration, 3rd Stabilize */
    if (ph->default_phase) {
      tmp_ph = ph;
    } else {
    NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011319,
                          "Can not have more than one 'Duration' phase in 'Simple' Scenario Type");
     // fprintf(stderr, "Can not have more than one Duration phase in Simple type scenario\n");
      //usage_parse_schedule_phase_duration(NULL);
    }
    strncpy(ph->phase_name, phase_name, sizeof(ph->phase_name) - 1);
  } else
    tmp_ph = add_schedule_phase(schedule, SCHEDULE_PHASE_DURATION, phase_name);       /* This adds in the end and returns 
                                                                              * the pointer of newly added node */

  num_fields = get_tokens(buf, fields, " ", 20);

  if (num_fields < 2) {
     NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011319, "Invalid number of argument with 'Duration' "
                          "phase. Atleast one field required with 'DURATION'");
   // fprintf(stderr, "Atleast one field required with DURATION, in line [%s]\n", buf_backup);
   // usage_parse_schedule_phase_duration(NULL);
  }

  tmp_ph->default_phase = 0;

  if (!strcmp(fields[1], "SESSIONS")) {
    if (global_settings->schedule_type != SCHEDULE_TYPE_SIMPLE /*||
        global_settings->schedule_by   != SCHEDULE_BY_SCENARIO*/) {
     NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011320, "SESSIONS option can only be configured with "
                          "'Simple' Schedule Type scenario. Either change Schedule Type 'Advance to Simple' or change 'SESSIONS option "
                          "to DURATION' in global schedule");
     // NS_EXIT(-1, "SESSIONS (fetches) are only applicable with simple scenario.");
    }

    if (get_group_mode(grp_idx) == TC_FIX_USER_RATE) {
    NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011320, "SESSIONS option can only be configured with "
                          "'Simple' Schedule Type scenario. Either change Schedule Type 'Advance to Simple' or change 'SESSIONS option "
                           "to DURATION' in global schedule");
     // NS_EXIT(-1, "SESSIONS (fetches) are not allowed in fixed session rate scenario.");
    }
  }

  //Only SESSIONS mode is supported for JMeter type script
  if((g_script_or_url == 100) && (strcmp(fields[1], "SESSIONS"))){
    snprintf(schedule_err, 1024, "Invalid mode '%s'in DURATION for jmeter type script.", fields[1]);
    NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011320, schedule_err);
      // NS_EXIT(-1, "Invalid mode %s in DURATION for JMeter type script.", fields[1]);  
  }
  if (strcmp(fields[1], "INDEFINITE") == 0) {
    tmp_ph->phase_cmd.duration_phase.duration_mode = DURATION_MODE_INDEFINITE;
  } else if (strcmp(fields[1], "TIME") == 0) {
    if (num_fields < 3) {
       NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011321,
                          "Invalid number of argument with 'TIME' option in 'Duration' phase. Time must be specified with format 'HH:MM:SS'");
     // fprintf(stderr, "Time in format HH:MM:SS must be specified with TIME, in line [%s]\n", buf_backup);
     // usage_parse_schedule_phase_duration(NULL);
    }

    tmp_ph->phase_cmd.duration_phase.duration_mode = DURATION_MODE_TIME;
    tmp_ph->phase_cmd.duration_phase.seconds = get_time_from_format(fields[2]);
    NSDL3_SCHEDULE(NULL, NULL, "seconds = %d\n", tmp_ph->phase_cmd.duration_phase.seconds);
    if (tmp_ph->phase_cmd.duration_phase.seconds < 0) {
    NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011321, "Time specified with 'TIME' option in 'Duration' "
                      "phase is not valid time format. Time must be specified with format 'HH:MM:SS'");

     // fprintf(stderr, "Invalid time specified in line [%s]\n", buf_backup);
     // usage_parse_schedule_phase_duration(NULL);
    }
  } else if (strcmp(fields[1], "SESSIONS") == 0) {

    if (num_fields < 3 || num_fields > 4) {
    NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011320,
                          "Invalid number of argument with 'SESSIONS' option in 'Duration' mode");
      // fprintf(stderr, "Sessions must be specified with SESSIONS, in line [%s]\n", buf_backup);
     // usage_parse_schedule_phase_duration(NULL);
    }

    tmp_ph->phase_cmd.duration_phase.duration_mode = DURATION_MODE_SESSION;

    // TODO Distibution must be using num_fetches from (duration phase) ---Arun Nishad 
    if(global_settings->schedule_by == SCHEDULE_BY_SCENARIO)
    {
      global_settings->num_fetches = tmp_ph->phase_cmd.duration_phase.num_fetches = atoi(fields[2]);

      /*For JMeter type script, number of sessions should be equal to number of users
        total_active_runprof_entries - total number of groups having atleast one user  */
      if((loader_opcode != MASTER_LOADER) && (g_script_or_url == 100) && (global_settings->num_fetches > total_active_runprof_entries))
        NS_EXIT(-1, "Number of SESSIONS %d is greater than number of groups %d for JMeter type script",
                                global_settings->num_fetches, total_active_runprof_entries);
    }
    else
    {
      tmp_ph->phase_cmd.duration_phase.num_fetches = atoi(fields[2]);
      global_settings->num_fetches += tmp_ph->phase_cmd.duration_phase.num_fetches;
    }

    if((ns_is_numeric(fields[2]) == 0) || (global_settings->num_fetches <= 0)){ 
        NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011320, "Number of sessions/fetches not configured "
                          "properly in 'Duration' phase. It should be numeric and greater than zero");

     // fprintf(stderr, "Invalid fetches specified in line [%s]\nSESSIONS should be numeric and greater than zero(0)\n", buf_backup);
     // usage_parse_schedule_phase_duration(NULL);
    }

    if(fields[3] && *fields[3]) {
      if(ns_is_numeric(fields[3]) == 0) {
        NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011320, "Number of sessions must be numeric with "
                          "'SESSIONS' option in 'Duration' phase");
       // fprintf(stderr, "Invalid per user session mode specified in line [%s]\nPer User Sessions should be numeric\n", buf_backup);
       // usage_parse_schedule_phase_duration(NULL);
      }

      tmp_ph->phase_cmd.duration_phase.per_user_fetches_flag = atoi(fields[3]);
      if((tmp_ph->phase_cmd.duration_phase.per_user_fetches_flag < 0) || (tmp_ph->phase_cmd.duration_phase.per_user_fetches_flag > 1)) {
       NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011320, "Number of sessions/fetches not configured "
                         "properly in 'Duration' phase. It should be numeric and greater than zero");

       // fprintf(stderr, "Invalid per user session mode specified in line [%s]\nPer User Sessions should be either 0 or 1\n", buf_backup);
       // usage_parse_schedule_phase_duration(NULL);
      }
    }
  } else {
      NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011320, "'Duration' phase is not configured properly");
    // fprintf(stderr, "Invalid format for DURATION, in line [%s]\n", buf_backup);
   // usage_parse_schedule_phase_duration(NULL);
  }
  return 0;
}


int parse_schedule_phase_stabilize(int grp_idx, char *full_buf, char *phase_name, char *err_msg, int set_rtc_flag)
{
  Phases *ph, *tmp_ph;
  char *fields[20];
  char buf_backup[MAX_DATA_LINE_LENGTH];
  Schedule *schedule;
  char *buf;
  char *buf_more;
  int num_fields;

  if (full_buf[strlen(full_buf) - 1] == '\n') {
    full_buf[strlen(full_buf) - 1] = '\0';
  }
  strcpy(buf_backup, full_buf);      /* keep a backup to print */
  buf = strstr(full_buf, "STABILIZATION");

  /* Check if this is the last one */
  buf_more = buf;
  while (buf_more != NULL) {
    buf_more += strlen("STABILIZATION");
    buf_more = strstr(buf_more, "STABILIZATION");
    if (buf_more) buf = buf_more;
  }

  if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO) {
    schedule = scenario_schedule;
  } else {
    schedule = &group_schedule[grp_idx];
  }

  if (global_settings->schedule_type == SCHEDULE_TYPE_SIMPLE) {
    ph = &(schedule->phase_array[2]);                /* 3rd Phase Stabilize */
    if (ph->default_phase) {
      tmp_ph = ph;
    } else {
    NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011322,
                          "Can not have more than one 'Stabilization' phase in 'Simple' Scenario Type");
    //  fprintf(stderr, "Can not have more than one Duration phase in Simple type scenario\n");
    //  usage_parse_schedule_phase_stabilize(NULL);
    }
    strncpy(ph->phase_name, phase_name, sizeof(ph->phase_name) - 1);
  } else
    tmp_ph = add_schedule_phase(schedule, SCHEDULE_PHASE_STABILIZE, phase_name);       /* This adds in the end and returns 
                                                                                        * the pointer of newly added node */

  num_fields = get_tokens(buf, fields, " ", 20);
  

  if (num_fields < 3) {
    NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011322,
                         "Invalid number of argument with 'Stabilization' phase. Atleast two fields reqiured with 'STABILIZATION'");
     // fprintf(stderr, "Atleast two fields reqiured with STABILIZATION, in line [%s]\n", buf_backup);
     // usage_parse_schedule_phase_stabilize(NULL);
  }

  tmp_ph->default_phase = 0;

  if (strcmp(fields[1], "TIME") == 0) {
    tmp_ph->phase_cmd.stabilize_phase.time = get_time_from_format(fields[2]);
    if (tmp_ph->phase_cmd.stabilize_phase.time < 0) {
     NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011322, "Time specified with 'TIME' option in "
                          "'Stabilization' phase is not valid time format. Time must be specified with format 'HH:MM:SS'");
      //fprintf(stderr, "Invalid time specified with TIME, in line [%s]\n", buf_backup);
      //usage_parse_schedule_phase_stabilize(NULL);
    }
  } else {
     NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011322,
                       "Invalid number of argument with 'Stabilization' phase.");
    //fprintf(stderr, "Invalid format for STABILIZATION (%s)\n", buf_backup);
  }
  return 0;
}

int parse_schedule_phase_ramp_down(int grp_idx, char *full_buf, char *phase_name, char *err_msg, int set_rtc_flag)
{
  Phases *ph, *tmp_ph;
  char *fields[20];
  char buf_backup[MAX_DATA_LINE_LENGTH];
  int group_mode;
  Schedule *schedule;
  char *buf;
  char *buf_more;
  int num_fields;
  char schedule_err[1024];

  if (full_buf[strlen(full_buf) - 1] == '\n') {
    full_buf[strlen(full_buf) - 1] = '\0';
  }
  strcpy(buf_backup, full_buf);      /* keep a backup to print */
  buf = strstr(full_buf, "RAMP_DOWN");

  /* Check if this is the last one */
  buf_more = buf;
  while (buf_more != NULL) {
    buf_more += strlen("RAMP_DOWN");
    buf_more = strstr(buf_more, "RAMP_DOWN");
    if (buf_more) buf = buf_more;
  }

  if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO) {
    schedule = scenario_schedule;
  } else {
    schedule = &(group_schedule[grp_idx]);
  }
 
  if (global_settings->schedule_type == SCHEDULE_TYPE_SIMPLE) {
    ph = &(schedule->phase_array[4]); /* 5rd Phase Ramp Down */
    if (ph->default_phase) {
      tmp_ph = ph;
    } else {
     NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011324,
                          "Can not have more than one 'Ramp Down' phase in 'Simple' type Schedule.");
      //fprintf(stderr, "Can not have more than one Duration phase in Simple type scenario\n");
      //usage_parse_schedule_phase_ramp_down(NULL);
    }
    strncpy(ph->phase_name, phase_name, sizeof(ph->phase_name) - 1);
  } else
    tmp_ph = add_schedule_phase(schedule, SCHEDULE_PHASE_RAMP_DOWN, phase_name);       /* This adds in the end and returns 
                                                                                        * the pointer of newly added node */

  num_fields = get_tokens(buf, fields, " ", 20);
  
  group_mode = get_group_mode(grp_idx);

  //if ((global_settings->schedule_type != SCHEDULE_TYPE_SIMPLE))

  tmp_ph->default_phase = 0;

  /* Validation */
  if (num_fields < 3) {
    NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011324, "Invalid number of argument for 'Ramp Down' phase.");
    //fprintf(stderr, "Atleast two field required after RAMP_DOWN, in line [%s]\n", buf_backup);
   // usage_parse_schedule_phase_ramp_down(NULL);
  }


  if (group_mode == TC_FIX_CONCURRENT_USERS) {
    if (strcmp(fields[2], "IMMEDIATELY") && strcmp(fields[2], "STEP") && strcmp(fields[2], "TIME")) {
     snprintf(schedule_err, 1024, "Ramp Down mode '%s' selected for Fixed Concurrent Users is not a valid mode. "
                                  "It can be IMMEDIATELY, STEP or TIME.", fields[2]);
     NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011324, schedule_err);
      // fprintf(stderr, "Unknown Ramp Down mode. It can be : IMMEDIATELY, STEP or TIME\n");
      //usage_parse_schedule_phase_ramp_down(NULL);
    }
  } else if (group_mode == TC_FIX_USER_RATE) {
     if (strcmp(fields[2], "IMMEDIATELY") && strcmp(fields[2], "TIME_SESSIONS")) {
      snprintf(schedule_err, 1024, "Ramp Down mode '%s' selected for Fixed Sessions Rate scenarios. "
                                   "It can be IMMEDIATELY or TIME_SESSIONS.", fields[2]); 
      NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011324, schedule_err);
      // fprintf(stderr, "Unknown Ramp Down mode for Fixed Sessions Rate scenarios. It can be : IMMEDIATELY or TIME_SESSIONS\n");
      // usage_parse_schedule_phase_ramp_up(NULL);
     }
  }
  else if (group_mode >= TC_FIX_HIT_RATE && group_mode <= TC_FIX_MEAN_USERS) {
     if (strcmp(fields[2], "IMMEDIATELY") && strcmp(fields[2], "TIME_SESSIONS")) {
      snprintf(schedule_err, 1024, "Ramp Down mode '%s' selected for Goal Based Scenarios. "
                                   "It can be IMMEDIATELY or TIME_SESSIONS.", fields[2]);
      NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011324, schedule_err);
     }
  }

  /* RAMP_DOWN <num_users/sessions rate> <ramp_down_mode> <mode_based_args> */
  char *users = fields[1];
  char *ramp_down_mode = fields[2];
  
  /* Users can only be ALL in Simple mode */
  if ((global_settings->schedule_type == SCHEDULE_TYPE_SIMPLE) &&
      strcmp(users, "ALL")) {
   // fprintf(stderr, "For Simple Scenario Types, Ramp Down users can only be ALL\n");
     // usage_parse_schedule_phase_ramp_down(NULL);
      NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011324, "Users can only be 'ALL' "
                      "for Schedule Type 'Simple' in 'Ramp Down' phase.");

  } /* All for Advanced should work */
/*  else if ((global_settings->schedule_type == SCHEDULE_TYPE_ADVANCED) && */
/*              !strcmp(users, "ALL")) { */
/*     fprintf(stderr, "For Advanced Scenario Types, Ramp Down users can not be ALL\n"); */
/*       usage_parse_schedule_phase_ramp_down(NULL); */
/*   } */

  /* Fill mode */
  if (strcmp(ramp_down_mode, "IMMEDIATELY") == 0) {
    tmp_ph->phase_cmd.ramp_down_phase.ramp_down_mode = RAMP_DOWN_MODE_IMMEDIATE;

  } else if (strcmp(ramp_down_mode, "STEP") == 0) {
    if (num_fields < 4) {
     NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011325,
                        "Invalid number of arguments with 'STEP' mode for 'Ramp Down' phase.");
    //  fprintf(stderr, "Atleast one argument required with STEP, in line [%s]\n", buf_backup);
    //  usage_parse_schedule_phase_ramp_down(NULL);
    }

    tmp_ph->phase_cmd.ramp_down_phase.ramp_down_mode = RAMP_DOWN_MODE_STEP;
    tmp_ph->phase_cmd.ramp_down_phase.ramp_down_step_users = atoi(fields[3]);
    if (tmp_ph->phase_cmd.ramp_down_phase.ramp_down_step_users < 0) {
   NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011325,
                          "'Ramp Down' Steps count should be greater than zero for 'STEP' mode.");
     // fprintf(stderr, "Steps can not be less than Zero, in line [%s]\n", buf_backup);
     // usage_parse_schedule_phase_ramp_down(NULL);
    }
    tmp_ph->phase_cmd.ramp_down_phase.ramp_down_step_time = get_time_from_format(fields[4]);
    if (tmp_ph->phase_cmd.ramp_down_phase.ramp_down_step_time < 0) {
     NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011325,
                          "Time specified with 'STEP' mode is invalid for 'Ramp Down' phase.");
     // fprintf(stderr, "Invalid time with STEPS, in line [%s]\n", buf_backup);
     // usage_parse_schedule_phase_ramp_down(NULL);
    }

  } else if (strcmp(ramp_down_mode, "TIME") == 0) {
    if (num_fields < 5) {
      NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011326,
                        "Invalid number of argument provided in 'TIME' mode for 'Ramp Down' phase");

     // fprintf(stderr, "Atleast Two argument required after TIME, in line [%s]\n", buf_backup);
     // usage_parse_schedule_phase_ramp_down(NULL);
    }

    tmp_ph->phase_cmd.ramp_down_phase.ramp_down_mode = RAMP_DOWN_MODE_TIME;
    tmp_ph->phase_cmd.ramp_down_phase.ramp_down_time = get_time_from_format(fields[3]);
    if (tmp_ph->phase_cmd.ramp_down_phase.ramp_down_time < 0) {
     NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011326,
                          "Time specified with 'TIME' mode for 'Ramp Down' phase is invalid.");
    // fprintf(stderr, "Invalid time specified in line %s\n", buf_backup);
     // usage_parse_schedule_phase_ramp_down(NULL);
    }else if (tmp_ph->phase_cmd.ramp_down_phase.ramp_down_time == 0) {
     tmp_ph->phase_cmd.ramp_down_phase.ramp_down_mode = RAMP_DOWN_MODE_IMMEDIATE;
    }

    if (strcmp(fields[4], "LINEARLY") == 0) {
      tmp_ph->phase_cmd.ramp_down_phase.ramp_down_pattern = RAMP_DOWN_PATTERN_LINEAR;
    } else if (strcmp(fields[4], "RANDOMLY") == 0) {
      //tmp_ph->phase_cmd.ramp_down_phase.ramp_down_pattern = RAMP_DOWN_PATTERN_RANDOM;
      tmp_ph->phase_cmd.ramp_down_phase.ramp_down_pattern = RAMP_DOWN_PATTERN_LINEAR;
    } else {                    /* Unknown pattern */
      snprintf(schedule_err, 1024, "Ramp Down pattern '%s' is not a valid pattern "
                                   "for 'TIME' mode. It can be LINEARLY or RANDOMLY", fields[4]); 
    NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011326, schedule_err);
     // fprintf(stderr, "Unknown Pattern %s, in RAMP_DOWN\n", fields[4]);
     // usage_parse_schedule_phase_ramp_down(NULL);
    }
  } else if (strcmp(ramp_down_mode, "TIME_SESSIONS") == 0) {
    if (num_fields < 5) {
       NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011327,
                          "Invalid number of argument with 'TIME_SESSIONS' mode for 'Ramp Down' phase");
     // fprintf(stderr, "Atleast Two argument required after TIME, in line [%s]\n", buf_backup);
     // usage_parse_schedule_phase_ramp_down(NULL);
    }

    /* TIME_SESSIONS <ramp_down_time in HH:MM:SS> <mode> <step_time or num_steps> 
       <mode> 0|1|2 (default steps, steps of x seconds, total steps) */
    tmp_ph->phase_cmd.ramp_down_phase.ramp_down_mode = RAMP_DOWN_MODE_TIME_SESSIONS;
    tmp_ph->phase_cmd.ramp_down_phase.ramp_down_time = get_time_from_format(fields[3]);
    if (tmp_ph->phase_cmd.ramp_down_phase.ramp_down_time < 0) {
   NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011327,
                          "Time specified with 'TIME_SESSIONS' mode for 'Ramp Down' phase is invalid.");
     // fprintf(stderr, "Time specified is invalid in line [%s]\n", buf_backup);
     // usage_parse_schedule_phase_ramp_down(NULL);
    }else if (tmp_ph->phase_cmd.ramp_down_phase.ramp_down_time == 0) {
     tmp_ph->phase_cmd.ramp_down_phase.ramp_down_mode = RAMP_DOWN_MODE_IMMEDIATE;
    }

/* ramp_down_time  tot_num_steps_for_sessions 
 * 0-179   secs    2
 * 180-239 secs    3
 * 240-299 secs    4
 */
    if (tmp_ph->phase_cmd.ramp_down_phase.ramp_down_time > 0) {
      double temp;
      int step_mode = atoi(fields[4]);
      if (step_mode == 0) {
        tmp_ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions = 2;
        int floor = tmp_ph->phase_cmd.ramp_down_phase.ramp_down_time/(1000*60); // chk it once
        if (floor > 1)
          tmp_ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions = floor;
        temp = (double)(tmp_ph->phase_cmd.ramp_down_phase.ramp_down_time/1000)/(double)tmp_ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions;
        tmp_ph->phase_cmd.ramp_down_phase.ramp_down_step_time = round(temp);
      }
      else if (step_mode == 1) {
        if (num_fields < 6) {
     NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011327,
                              "Step time or num steps are not specified with 'TIME_SESSIONS' mode for 'Ramp Down' phase.");
         // fprintf(stderr, "Step time or num steps should be specified, in line [%s]\n", buf_backup);
          //usage_parse_schedule_phase_ramp_down(NULL);
        }
        tmp_ph->phase_cmd.ramp_down_phase.ramp_down_step_time = atoi(fields[5]);
        if(tmp_ph->phase_cmd.ramp_down_phase.ramp_down_step_time <= 0){
      NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011327,
                                "Ramp Down step time with 'TIME_SESSIONS' mode is not valid time.");
        //  NS_EXIT(-1, "Invalid step time '%d' at line [%s]", tmp_ph->phase_cmd.ramp_down_phase.ramp_down_step_time, buf_backup);
        }          
        if((tmp_ph->phase_cmd.ramp_down_phase.ramp_down_time/1000) < tmp_ph->phase_cmd.ramp_down_phase.ramp_down_step_time){
        NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011327,
                              "Ramp Down time can not be less then step time for 'TIME_SESSIONS' mode.");
        //  NS_EXIT(-1, "Ramp Down time can not be less then step time at line [%s]", buf_backup);
        }          
        temp = (double)(tmp_ph->phase_cmd.ramp_down_phase.ramp_down_time/1000)/(double)tmp_ph->phase_cmd.ramp_down_phase.ramp_down_step_time;
        tmp_ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions = round(temp);
      }
      else if (step_mode == 2) {
        if (num_fields < 6) {
        NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011327,
                              "Num steps with 'TIME_SESSIONS' mode is not specified for 'Ramp Down' phase.");
         // fprintf(stderr, "Step time or num steps should be specified, in line [%s]\n", buf_backup);
         // usage_parse_schedule_phase_ramp_down(NULL);
        }
        tmp_ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions = atoi(fields[5]);
        if(tmp_ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions <= 0){
          NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011327,
                              "Num steps can not less than or equal to zero with 'TIME_SESSIONS' mode for 'Ramp Down' phase.");

        //  NS_EXIT(-1, "Invalid num steps '%d' at line [%s]", tmp_ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions, buf_backup);
        }          
        temp = (double)(tmp_ph->phase_cmd.ramp_down_phase.ramp_down_time/1000)/(double)tmp_ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions;
        // step time can not be 0 
        tmp_ph->phase_cmd.ramp_down_phase.ramp_down_step_time = (round(temp))?round(temp):1; 
      }
      else {
       NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011327,
                             "Invalid step mode is selected with 'TIME_SESSIONS' mode for 'Ramp Down' phase.");
       // NS_EXIT(-1, "Invalid step mode specified in line [%s]", buf_backup);
      }
    }
  }
  //Added check for netomni
  if (loader_opcode == CLIENT_LOADER) {
    if (global_settings->schedule_by == SCHEDULE_BY_GROUP) {
      /* NOTE: In CLIENT_LOADER only NUM mode is supported therefore verfying only quantity field of runProfTable*/
      if (runProfTable[grp_idx].quantity == 0) {
        NSDL4_SCHEDULE(NULL, NULL, "Making users_or_sess_rate = 0 for group_idx [%d], group with quantity given zero", grp_idx);
        strcpy(users, "0");
      }
    }
  }
  /* Fill users */
  if (strcmp(users, "ALL") == 0) {
    //tmp_ph->ramp_up_phase.num_vusers_or_sess = global_settings->num_connections;/* Sum of all SGRP users */
    tmp_ph->phase_cmd.ramp_up_phase.num_vusers_or_sess = -1; /* Fill -1 for ALL for now, later filled during validation */
  } else if (group_mode == TC_FIX_USER_RATE) { /* FSR */
  /*In regard to BUG 9078, 
      FSR advance scenario, SCHEDULE ALL RampDown3 RAMP_DOWN 32.364 TIME_SESSIONS 00:30:00 0
      When storing a double variable into integer it might have floating point error.*/
    tmp_ph->phase_cmd.ramp_down_phase.num_vusers_or_sess = compute_sess_rate(users); 
  } else {
    tmp_ph->phase_cmd.ramp_down_phase.num_vusers_or_sess = atoi(users);
    if (tmp_ph->phase_cmd.ramp_down_phase.num_vusers_or_sess < 0) {
      NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011327,
                        "Number of users for 'Ramp Down' phase should be greater than zero in 'TIME_SESSIONS' mode.");
     // fprintf(stderr, "Quantity can not be less than zero, in line [%s]\n", buf_backup);
     // usage_parse_schedule_phase_ramp_down(NULL);
    }
  }
  return 0;
}

static int check_duplicate_phase_name(int grp_idx, char *phase_name, char *buf, char *err_msg)
{
  Phases *ph;
  Schedule *schedule;
  int i;
  char schedule_err[1024]; 
  if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO) {
    schedule = scenario_schedule;
  } else {
    schedule = &group_schedule[grp_idx];
  }

  ph = schedule->phase_array;

  NSDL3_SCHEDULE(NULL, NULL, "Method Called. grp_idx = %d, phase_name = %s, "
                 "num_phases = %d schedule = %p\n", grp_idx, phase_name, schedule->num_phases, schedule);
  for (i = 0; i < schedule->num_phases; i++) {
    if (!(ph[i].default_phase) && !strcmp(phase_name, ph[i].phase_name)) {
      sprintf(schedule_err, "Phase name '%s' used more than once for same group", phase_name);
      NS_KW_PARSING_ERR(buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011308, schedule_err);
    //  NS_EXIT(-1, "Duplicate phase names not allowed. Phase name '%s' used more than once for same group.", phase_name);
    }
  }
  return 0;
}

char *get_phase_name(int phase_idx)
{
  static char buf[32 + 1];
 
  if(phase_idx == SCHEDULE_PHASE_START)
    strcpy(buf, "START");
  else if(phase_idx == SCHEDULE_PHASE_RAMP_UP)
    strcpy(buf, "RAMP_UP");
  else if(phase_idx == SCHEDULE_PHASE_RAMP_DOWN)
    strcpy(buf, "RAMP_DOWN");
  else if(phase_idx == SCHEDULE_PHASE_STABILIZE)
    strcpy(buf, "STABILIZE");
  else if(phase_idx == SCHEDULE_PHASE_DURATION)
    strcpy(buf, "DURATION");
  else if(phase_idx == SCHEDULE_PHASE_RTC)
    strcpy(buf, "RTC");

  return buf; 
}

/**
 * SCHEDULE <GroupName> <PhaseName> <PhaseType> <PhaseParameters>
 *
 * Phase Type:
 * o    START
 * o    RAMP_UP
 * o    STABILIZATION
 * o    DURATION
 * o    RAMP_DOWN
 *
 * Validations:
 * o    Scenario based or group based - Simple; Group can only be "ALL" (Mixed mode or FCU or FSR)
 * o    
 */
int kw_set_schedule(char *buf , char *err_msg ,int set_rtc_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char grp[MAX_DATA_LINE_LENGTH];
  char phase_name[MAX_DATA_LINE_LENGTH];
  char phase_type[MAX_DATA_LINE_LENGTH];
  int grp_idx;
  char schedule_err[1024];
  //char text[MAX_DATA_LINE_LENGTH];

  int num;
  
  num = sscanf(buf, "%s %s %s %s", keyword, grp, phase_name, phase_type);
  if (num < 4) {
      NS_KW_PARSING_ERR(buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011308, CAV_ERR_MSG_1);
   // fprintf(stderr, "Invalid SCHEDULE entry\nSyntax: SCHEDULE <GroupName> <PhaseName> <PhaseType> <PhaseParameters>\n");
   // usage_kw_set_schedule(NULL);
  }

  grp_idx = find_sg_idx(grp);
  
  if (grp_idx == NS_GRP_IS_INVALID) {
    NSDL3_SCHEDULE(NULL, NULL, "Invalid group name in schedule.It does not match with any SGRP group name.Hence exit.");
      NS_KW_PARSING_ERR(buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011308, "Invalid group name provided in 'Global Schedule'.");
      //return;    
   // NS_EXIT(-1, "Group name in schedule = %s, does not match with any SGRP group name.Hence exit", grp);
  }

  if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO) {
    if (grp_idx != -1)   /* i.e  not ALL */ {       
     NS_KW_PARSING_ERR(buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011308, 
                       "Group field can only be 'ALL' for Schedule By 'Scenario'.");
      // fprintf(stderr, "In Schedule Based Scenario, group field can only be 'ALL'\n");
     // usage_kw_set_schedule(NULL);
    }
    if(strcmp(grp,"ALL"))
    {
        NS_KW_PARSING_ERR(buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011308, 
                          "Group field can only be 'ALL' for Schedule By 'Scenario'.");
     // NS_EXIT(-1, "In SCHEDULE_BY_SCENARIO scenario groupname must be ALL");
    }
  } else if (global_settings->schedule_by == SCHEDULE_BY_GROUP) {
    if (grp_idx == -1)   /* i.e ALL */ {
      NS_KW_PARSING_ERR(buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011308, "Group field can not be 'ALL' for Schedule By 'Group'");
    //  fprintf(stderr, "In Group Based Scenario, group field can not be 'ALL'\n"); 
     // usage_kw_set_schedule(NULL);
    }
  }

// Added validation for phase name. It should be of maximum 48 characters and must start with alpha charcter, Other characters may be alpha, Numeric     or Underscore '_'.

  if(strlen(phase_name) > 48)
  {
    NS_KW_PARSING_ERR(buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011308, 
                      "Provided phase name is invalid. Length can be greater than 48 characters.");
       // NS_EXIT(-1, "Error:Invalid phase name (%s) is greater than 48 characters in  line (%s).", phase_name, buf);
  }

  if (match_pattern(phase_name, "^[a-zA-Z][a-zA-Z0-9_]*$") == 0)
  {
    NS_KW_PARSING_ERR(buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011308, "Phase name '%s' is invalid. Phase name should contain "
                      "only alphanumeric character, and first character must be alpha, other characters can be alpha, numeric or underscore");
      // NS_EXIT(-1, "Error: Invalid phase name (%s). Name of phase should contain only alphanumeric character, first character must be alpha. Other characters may be alpha, Numeric or Underscore '_' in line (%s).", phase_name, buf);
  }

  check_duplicate_phase_name(grp_idx, phase_name, buf, err_msg);

  if (strcmp(phase_type, "START") == 0) {
    NSDL4_SCHEDULE(NULL, NULL, "parsing START");
    parse_schedule_phase_start(grp_idx, buf, phase_name, err_msg, 0);
  } else if (strcmp(phase_type, "RAMP_UP") == 0) {
    NSDL4_SCHEDULE(NULL, NULL, "parsing RAMP_UP");
    parse_schedule_phase_ramp_up(grp_idx, buf, phase_name, err_msg, 0);
  } else if (strcmp(phase_type, "DURATION") == 0) {
    NSDL4_SCHEDULE(NULL, NULL, "parsing DURATION");
    parse_schedule_phase_duration(grp_idx, buf, phase_name, err_msg, 0);
  } else if (strcmp(phase_type, "STABILIZATION") == 0) {
    NSDL4_SCHEDULE(NULL, NULL, "parsing STABILIZATION");
    parse_schedule_phase_stabilize(grp_idx, buf, phase_name, err_msg, 0);
  } else if (strcmp(phase_type, "RAMP_DOWN") == 0) {
    NSDL4_SCHEDULE(NULL, NULL, "parsing RAMP_DOWN");
    parse_schedule_phase_ramp_down(grp_idx, buf, phase_name, err_msg, 0);
  } else {
    snprintf(schedule_err, 1024, "Phase '%s' given in Global Scenario is not a valid phase.", phase_type);
    NS_KW_PARSING_ERR(buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011308, schedule_err);
  }
  return 0;
}

/**
 * RAMP_DOWN_METHOD <group_name> <ramp_down_method> <mode based fields>
 * 0 - Allow current session to complete
 *     Mode based fields:
 *	0 - User normal think time
 *	1 - Disable all future Think Times
 *	2 - Hasten completion by disregarding all Think Times 
 *	3 - Hasten completion by disregarding all Think Times and use idle time of [X] seconds
 * 1 - Allow current page to complete
 *	0 - User normal idle time
 *	1 - Hasten completion by using idle time [X] seconds
 * 2 - Stop immediately
 */
int kw_set_ramp_down_method(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  int num;
  char mode[8] = "0"; 
  char option[8] = "0";
  char time[32] = "0";
  char groupName[64] = "";

  NSDL2_PARSING(NULL, NULL, "Method Called, buf = %s", buf);

  num = sscanf(buf, "%s %s %s %s %s", keyword, groupName, mode, option, time);

  if(ns_is_numeric(mode) == 0 || ns_is_numeric(option) == 0 || ns_is_numeric(time) == 0 )
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RAMP_DOWN_METHOD_USAGE, CAV_ERR_1011279, CAV_ERR_MSG_2);
  }

  gset->rampdown_method.mode = atoi(mode);
  gset->rampdown_method.option = atoi(option);
  gset->rampdown_method.time = atoi(time);

  NSDL2_PARSING(NULL, NULL, "group name = %s mode = %d option = %d time = %d", groupName, gset->rampdown_method.mode, 
                gset->rampdown_method.option, gset->rampdown_method.time);
   
  if((gset->rampdown_method.mode < 0) || (gset->rampdown_method.option < 0)){
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RAMP_DOWN_METHOD_USAGE, CAV_ERR_1011279, CAV_ERR_MSG_8);
  }

  //checking option for mode 0.
  if((gset->rampdown_method.mode == RDM_MODE_ALLOW_CURRENT_SESSION_COMPLETE) && ((gset->rampdown_method.option < 0) || gset->rampdown_method.option > 3))
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RAMP_DOWN_METHOD_USAGE, CAV_ERR_1011279, CAV_ERR_MSG_3);
  }
  //checking option for mode 1.
  else if((gset->rampdown_method.mode == RDM_MODE_ALLOW_CURRENT_PAGE_COMPLETE) && ((gset->rampdown_method.option < 0) || (gset->rampdown_method.option > 1))) 
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RAMP_DOWN_METHOD_USAGE, CAV_ERR_1011279, CAV_ERR_MSG_3);
  }
  else if((gset->rampdown_method.mode == RDM_MODE_STOP_IMMEDIATELY) && num != 3 )
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RAMP_DOWN_METHOD_USAGE, CAV_ERR_1011279, CAV_ERR_MSG_1);
  } 

  if ((gset->rampdown_method.mode == RDM_MODE_ALLOW_CURRENT_SESSION_COMPLETE) && 
      (gset->rampdown_method.option == 
       RDM_OPTION_HASTEN_COMPLETION_DISREGARDING_THINK_TIMES_USE_IDLE_TIME) && 
      num != 5) {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RAMP_DOWN_METHOD_USAGE, CAV_ERR_1011279, CAV_ERR_MSG_1);
  }
  else if ((gset->rampdown_method.mode == 
       RDM_MODE_ALLOW_CURRENT_PAGE_COMPLETE) && 
      (gset->rampdown_method.option == 
       RDM_OPTION_HASTEN_COMPLETION_USING_IDLE_TIME) && 
      num != 5) {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_RAMP_DOWN_METHOD_USAGE, CAV_ERR_1011279, CAV_ERR_MSG_1);
      }

  return 0;
}


/* This function will set flag enable_adjust_rampup_timer */
int kw_set_adjust_rampup_timer(char *buf, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH];
  int mode = 0;   // In mili sec
  int num;
  
  NSDL2_PARSING(NULL, NULL, "Method called, buf = %s", buf);
  
  num = sscanf(buf, "%s %d %s", keyword, &mode, tmp);
  NSDL2_PARSING(NULL, NULL, "keyword = %s, interval = %d", keyword, mode);
 
  if (num < 2){
    NS_EXIT(-1, "Invalid number of arguments in line %s", buf); 
  }
  
  global_settings->adjust_rampup_timer = mode; 
  NSDL2_PARSING(NULL, NULL, "adjust_rampup_timer = %d, global_settings = %p", global_settings->adjust_rampup_timer, global_settings);

  return 0;
}
