/********************************************************************************
 * File Name            : ni_schedule_phase_parse.c
 * Author(s)            : Manpreet Kaur
 * Date                 : 31 Aug 2012
 * Copyright            : (c) Cavisson Systems
 * Purpose              : Contains parsing function for scheduling keywords
 * Modification History : <Author(s)>, <Date>, <Change Description/Location>
 ********************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/stat.h>
#include <time.h>

#include "nslb_util.h"

#include "ni_user_distribution.h"
#include "ni_scenario_distribution.h"
#include "ni_schedule_phases_parse.h"
#include "nslb_util.h"
#include "../../../ns_exit.h"
#include "../../../ns_error_msg.h"
#include "../../../ns_kw_usage.h"
#include "../../../ns_global_settings.h"
 
/*Number of sessions*/
int num_fetches = 0;
static int find_sg_index(char* scengrp_name);
double round(double x);

int rtc_group_idx = 0;
int rtc_phase_idx = 0;

static int remove_leading_zeros(char *src, char* dest, int start_idx, int upto_three_digit)
{
  int idx = start_idx;//For mantisa part it should be 0, whereas in case of mantisa it will be index in dest array
  NIDL (1, "Method called, idx = %d, upto_three_digit = %d, src = %s", idx, upto_three_digit, src);
  char *ptr = src;
  while (*ptr && upto_three_digit)
  {
    if ((*ptr == '0') && !idx)//Remove all leading zero
    {
      ptr++;//Fix done for 9952
      upto_three_digit--;//Used in case of mantisa where we need to truncate upto 3 decimal places
      continue;
    }
    else
    {
      dest[idx] = *ptr;
      NIDL (1, "dest[%d] = %c", idx, dest[idx]);
      idx++;//Next idx
      upto_three_digit--;
      NIDL (1, "idx = %d, upto_three_digit = %d", idx, upto_three_digit);
    }
    ptr++;//Fix done for 9952
  }
  return(idx);
}

/*In regard to BUG 9078, 
  FSR advance scenario, SCHEDULE ALL RampUp3 RAMP_UP 32.364 TIME_SESSIONS 00:30:00 0
                        SCHEDULE ALL RampDown3 RAMP_DOWN 32.364 TIME_SESSIONS 00:30:00 0
  When storing a double variable into integer it might have floating point error.*/
static int compute_sess_rate(char *sess_value)
{
  int num_field, idx, exponent_len, len;
  char *fields[2];
  char tmp_sess_str[100];//Local string which creates session rate 
  memset(tmp_sess_str, 0, 100);

  NIDL (1, "Method called, sess_value= %s", sess_value);
  num_field = get_tokens_(sess_value, fields, ".", 2);
  /*If session rate provided is a whole number then return session value * 1000*/
  if (num_field != 2)
  {
    NIDL (1, "Session rate without decimal point, ret = %d", (atoi(sess_value) * SESSION_RATE_MULTIPLIER));
    return((atoi(sess_value) * SESSION_RATE_MULTIPLIER));
  } 
  //Remove all leading 0s from mantisa part
  idx = remove_leading_zeros(fields[0], tmp_sess_str, 0, 999);
  //Find length of exponent part
  exponent_len = strlen(fields[1]);
  //Here we support upto 3 decimal place, if exponent part length greater than three then truncate string
  len = (exponent_len < 3)?999:3;
  //Remove all leading 0s from exponent part
  remove_leading_zeros(fields[1], tmp_sess_str, idx, len);
  //In case of exponent part less than 3
  //0.5 --- 0.500(add trailing 00)
  //0.05 --- 0.050(add trainling 0)
  if (exponent_len < 3)
  {
    if (exponent_len == 1)
      strcat(tmp_sess_str, "00");
    else if (exponent_len == 2)
      strcat(tmp_sess_str, "0");
  }
  NIDL (1, "tmp_sess_str = %s\n", tmp_sess_str);
  return(atoi(tmp_sess_str));//return session rate 
} 
/*Returns mode of group(FCU/FSR)*/
int get_grp_mode(int grp_idx)
{
  NIDL(3, "Method called, grp_idx = %d", grp_idx);
  if (grp_idx == -1)
    return mode;

  if (mode == TC_MIXED_MODE) {
      return scen_grp_entry[grp_idx].grp_type; /* FSR, FCU */
  } else
    return mode;
}

/* Method return time in millisecs, takes input
 * HH:MM:SS.MSECS
 * HH:MM:SS
 * MM:SS
 * SS 
 * */
int ni_get_time_from_format(char *str)
{
  int field1, field2, field3, field4;
  int num, total_time;
  NIDL (1, "Method called, str = %s", str);

  num = sscanf(str, "%d:%d:%d.%d", &field1, &field2, &field3, &field4);
  if(num == 1)
    total_time = field1*1000; //if only one value given it is seconds
  else if(num == 3)
    total_time = ((field1*60*60) + (field2*60) + field3)*1000; //if HH::MM:SS given
  else if(num == 4)
    total_time = ((field1*60*60) + (field2*60) + field3)*1000 + field4; //if HH::MM:SS.MSECS given
  else if(num == 2)
    total_time = ((field1*60) + field2)*1000; //if MM:SS given
  else
  {
    return -1;
    //NS_KW_PARSING_ERR(buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, kw_buf, "Invalid format of time '%s'", str);
    //NS_EXIT(-1, "Invalid format of time '%s'", str);
  }

  NIDL (3, "Returning total_time = %d", total_time);
  return(total_time);
}

/* Convert given rate to minute
 * Example: Given rate is 120 S, now we need to convert rate of 120 user per sec
 * into minute, here calculation would be 120 S = 120 * 60 = 7200 M
 * */
float convert_to_per_min (char *buf)
{
  float time_min;
  char buff[MAX_CONF_LINE_LENGTH + 1];
  char keyword[MAX_DATA_LINE_LENGTH];
  char option = 'M';
  int num;

  NIDL (4, "Method called, buf = %s", buf);
  strcpy(buff, buf);

  if ((num = sscanf(buff, "%s %f %c", keyword, &time_min, &option)) < 2)
  {
    NS_EXIT(-1, "Invalid  format of the keyword %s", keyword);
  }

  switch (option)
  {
    case 'S':
      time_min = time_min * 60.0;  //60 S = 60*60 M
      break;
    case 'H':
      time_min = time_min / 60.0;  //60 H = 60/60 M
      break;
    case 'M':
      break;
    default:
      NS_EXIT(-1, "Invalid  unit in the keyword %s", keyword);
      break;
  }
  NIDL (4, "time_min = %f", time_min);
  return time_min;
}

/*SCHEDULE_TYPE   <Simple or Advanced>*/
/*static void usage_kw_set_schedule_type(char *buf)
{
  NIDL (1, "Method called.");
  NS_EXIT(-1, "%s\nUsage:\tSCHEDULE_TYPE <SIMPLE or ADVANCED>", buf);
}*/

/*SCHEDULE_BY  <Scenario or Group>*/
/*static void usage_kw_set_schedule_by(char *buf)
{
  NIDL (1, "Method called.");
  NS_EXIT(-1, "%s\nUsage:\tSCHEDULE_BY <SCENARIO or GROUP>", buf);
}*/

/**
 * SCHEDULE <group_name> <phase_name> START <start_mode> <start_args> 
 *
 * start_mode and args:
 * IMMEDIATELY    (Start from beginning of test run) (Default)
 * AFTER TIME HH:MM:SS
 * AFTER GROUP G1 [HH:MM:SS] (valid only for group based scheduling)
 */
/*static void usage_parse_schedule_phase_start(char *buf)
{
  NIDL (1, "Method called.");

  fprintf(stderr, "Error:\n%s\n", buf);
  fprintf(stderr, "Syntax\n");
  fprintf(stderr, "\t SCHEDULE <group_name> <phase_name> START <start_mode> <start_args>\n\n");
  fprintf(stderr, "\t\t <start_mode>:\n");
  fprintf(stderr, "\t\t\t IMMEDIATELY\n");
  fprintf(stderr, "\t\t\t AFTER TIME <HH:MM:SS>\n");
  fprintf(stderr, "\t\t\t AFTER GROUP G1 [HH:MM:SS]\n");

  NS_EXIT(-1, "%s\nUsage:\n\t\t\tSCHEDULE <group_name> <phase_name> START <start_mode> <start_args>", buf);
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
/*static void usage_parse_schedule_phase_ramp_up(char *buf)
{
  NIDL (1, "Method called.");

  fprintf(stderr, "Error: %s\n", buf);
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
  
  NS_EXIT(-1, "%s\nUsage: SCHEDULE <group_name> <phase_name> RAMP_UP <num_users/sessions rate> <ramp_up_mode> <mode_based_args>", buf);
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
/*static void usage_parse_schedule_phase_duration(char *buf)
{
  NIDL (1, "Method called.");

  fprintf(stderr, "Error:%s\n", buf);
  fprintf(stderr, "Syntax\n");
  fprintf(stderr, "\t\t DURATION <duration_mode> <args>\n");
  fprintf(stderr, "\t\t\t Duration_mode:\n");
  fprintf(stderr, "\t\t\t\t INDEFINITE (default)\n");
  fprintf(stderr, "\t\t\t\t TIME HH:MM:SS\n");
  fprintf(stderr, "\t\t\t\t SESSIONS <num_sessions> <per_user_session_flag>\n");
  NS_EXIT(-1, "%s\nUsage: DURATION <duration_mode> <args> OR SESSIONS <num_sessions> <per_user_session_flag>", buf);
}*/

/*SCHEDULE G1 None STABILIZATION TIME <HH:MM:SS>*/
/*static void usage_parse_schedule_phase_stabilize(char *buf)
{
  NIDL (1, "Method called.");

  NS_EXIT(-1, "%s\nUsage: SCHEDULE G1 None STABILIZATION TIME <HH:MM:SS>", buf);
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
/*static void usage_parse_schedule_phase_ramp_down(char *buf)
{
  NIDL (1, "Method called.");

  fprintf(stderr, "Error:\n%s\n", buf);
  fprintf(stderr, "Syntax\n");
  fprintf(stderr, "\t\t SCHEDULE <group_name> <phase_name> RAMP_DOWN <num_users/sessions rate> <ramp_down_mode> <mode_based_args>\n");
  fprintf(stderr, "\t\t\t IMMEDIATELY (Default)\n");
  fprintf(stderr, "\t\t\t TIME <ramp_down_time in HH:MM:SS> <ramp_down_pattern>\n");
  fprintf(stderr, "\t\t\t STEP <ramp-down-step-users> <ramp-down-step-time in HH:MM:SS> (For FCU)\n");
  fprintf(stderr, "\t\t\t Or\n");
  fprintf(stderr, "\t\t\t STEP < ramp_down_step_sessions> < ramp-down-step-time in HH:MM:SS> (For FSR)\n");
  NS_EXIT(-1, "%s\nUsage: SCHEDULE <group_name> <phase_name> RAMP_DOWN <num_users/sessions rate> <ramp_down_mode> <mode_based_args>", buf);
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
/*static void usage_kw_set_schedule(char *buf)
{
  NIDL (1, "Method called.");

  fprintf(stderr, "Error:\n%s\n", buf);
  fprintf(stderr, "Syntax\n");
  fprintf(stderr, "\t\t SCHEDULE <GroupName> <PhaseName> <PhaseType> <PhaseParameters>\n");
  fprintf(stderr, "\t\t Phase Type:\n");
  fprintf(stderr, "\t\t\t START\n");
  fprintf(stderr, "\t\t\t RAMP_UP\n");
  fprintf(stderr, "\t\t\t DURATION\n");
  fprintf(stderr, "\t\t\t STABILIZATION\n");
  fprintf(stderr, "\t\t\t RAMP_DOWN\n");

  NS_EXIT(-1, "%s\nUsage: SCHEDULE <GroupName> <PhaseName> <PhaseType> <PhaseParameters>", buf);
}*/

/*SCHEDULE_TYPE <Simple or Advanced>*/
int kw_schedule_type(char *buf, char *err_msg)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char type[MAX_DATA_LINE_LENGTH];
  int num;

  NIDL (1, "Method called. buf = %s", buf);

  num = sscanf(buf, "%s %s", keyword, type);
  if (num < 2) {
    NS_KW_PARSING_ERR(buf, 0, err_msg, SCHEDULE_TYPE_USAGE, CAV_ERR_1011175, CAV_ERR_MSG_1);
    //usage_kw_set_schedule_type("Keyword SCHEDULE_TYPE must have one argument");
  }
  
  if (strcasecmp(type, "Simple") == 0) {
    schedule_type = SCHEDULE_TYPE_SIMPLE;
  } else if (strcasecmp(type, "Advanced") == 0) {
    schedule_type = SCHEDULE_TYPE_ADVANCED;
  } else {                      /* Unknown type */
   NS_KW_PARSING_ERR(buf, 0, err_msg, SCHEDULE_TYPE_USAGE, CAV_ERR_1011338, type);
    //usage_kw_set_schedule_type("Keyword SCHEDULE_TYPE can only have values \"Simple\" or \"Advanced\"");
  }

  NIDL (2, "setting schedule_type = %d", schedule_type);
  return 0;
}

/*SCHEDULE_BY  <Scenario or Group>*/
int kw_schedule_by(char *buf,char *err_msg)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char sch_by[MAX_DATA_LINE_LENGTH];
  int num;
  NIDL (1, "Method called. buf = %s", buf);

  num = sscanf(buf, "%s %s", keyword, sch_by);
  if (num < 2) {
   NS_KW_PARSING_ERR(buf, 0, err_msg, SCHEDULE_BY_USAGE, CAV_ERR_1011176, CAV_ERR_MSG_1);
   //  usage_kw_set_schedule_by("Keyword SCHEDULE_BY must have only one argument");
  }
  
  if (strcasecmp(sch_by, "Scenario") == 0) {
    schedule_by = SCHEDULE_BY_SCENARIO;
  } else if (strcasecmp(sch_by, "Group") == 0) {
    schedule_by = SCHEDULE_BY_GROUP;
  } else {                      /* Unknown schedule */
    NS_KW_PARSING_ERR(buf, 0, err_msg, SCHEDULE_BY_USAGE, CAV_ERR_1011339, sch_by);
    //usage_kw_set_schedule_by("Keyword SCHEDULE_BY can only have values \"Scenario\" or \"Group\"");
  }

  NIDL (2, "setting schedule_by = %d", schedule_by);
  return 0;
}
/*Int var used to save dependent group index*/
static int save_dependent_gp_idx = -1;

/*Need to verify whether generator name match or not*/
static int find_dependent_grp_index(char* scengrp_name, int grp_idx, int dependent_grp_idx)
{
  int i, j;
  NIDL (2, "Method called, scengrp_name = %s, dependent_grp_idx = %d", scengrp_name, dependent_grp_idx);

  /* Group name ALL */
  if (strcasecmp(scengrp_name, "ALL") == 0) {
    NIDL (2, "SGRP keyword received with group name ALL");
    NS_EXIT(-1, CAV_ERR_1011301, "Group name can never be ALL");
  }
  /* Search group name, in case of same group name we will find group name and save grp idx
   * else need to search group name*/
  if (dependent_grp_idx == -1)
  { 
    for (i = 0; i < total_sgrp_entries; i++) 
    {
      if (!strcmp(scen_grp_entry[i].scen_group_name, scengrp_name))
      { 
        NIDL (2, "Group name matches");
        save_dependent_gp_idx = i;  
        break;  
      }
    }
  }
  /*Search generator name */
  if (save_dependent_gp_idx != -1)  
  {
    for (j = 0, i = save_dependent_gp_idx; j < scen_grp_entry[save_dependent_gp_idx].num_generator; j++, i++) 
    {
      if (!strcmp(scen_grp_entry[grp_idx].generator_name, scen_grp_entry[i].generator_name))
      {
        NIDL (2, "Generator name match");
        return i; //SGRP name found
      } else {
        continue;
      }    
    }
    /*Generator mis-match*/
    NIDL (2, "Generator name mismatch");
    NS_EXIT(-1, CAV_ERR_1011364, scen_grp_entry[grp_idx].scen_group_name, scen_grp_entry[save_dependent_gp_idx].scen_group_name);
  } 
   
  NIDL (2, "Group name mis-match");
  return NS_GRP_IS_INVALID; //Invalid group name
}

/**
 *
 * SCHEDULE <group_name> <phase_name> START <start_mode> <start_args>
 * START <start_mode> <args>
 *
 * IMMEDIATELY    (Start from beginning of testrun) (Default)
 * AFTER TIME HH:MM:SS
 * AFTER GROUP G1 [HH:MM:SS]
 *
 * Validations:
 * o    More than one start phases for one group is not permitted.
 */
static int ni_parse_schedule_phase_start(int grp_idx, char *full_buf, char *phase_name, int set_rtc_flag, char *err_msg)
{
  ni_Phases *tmp_ph;
  char *fields[20];
  char buf_backup[MAX_DATA_LINE_LENGTH];
  int dependent_grp_idx;
  int num_fields;
  char *buf;
  char *buf_more;
  int i, j;
  NIDL (4, "Method called grp_idx = %d, full_buf = %s, phase_name = %s", grp_idx, full_buf, phase_name);

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
  /* For schedule_by_scenario we can populate ni_Phases ptr, 
   * But for schedule_by_group we need to update each group
   * hence will run a loop with respect to number of generator per group*/
  if (schedule_by == SCHEDULE_BY_SCENARIO) {
    tmp_ph = &(scenario_schedule_ptr->phase_array[0]);
  } 
  /* Start phase will always be there. */
  if (schedule_by == SCHEDULE_BY_SCENARIO) {
    /* By now, we already have start phase, we overwrite those. Exit if it has been overwritten before */
    if (!(tmp_ph->default_phase)) {
      NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011310, 
                        "Can not have more than one 'Start' phase for Schedule By 'Scenario'");
      //usage_parse_schedule_phase_start("In SCHEDULE_BY SCENARIO, more than one start phase is not allowed");
    }
    tmp_ph->default_phase = 0;
    strncpy(tmp_ph->phase_name, phase_name, sizeof(tmp_ph->phase_name) - 1);
  } else {
    ni_Phases *ph = &(group_schedule_ptr[grp_idx].phase_array[0]);
    if (!(ph->default_phase)) {
      //fprintf(stderr, "Can not have more than one start phases for %s\n",
        //        schedule_by == SCHEDULE_BY_SCENARIO ? "Scenario" : "Group");
      NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011310, 
                        "Can not have more than one 'Start' phase for Schedule By 'Group'");
    }
    for (j = 0, i = grp_idx; j < scen_grp_entry[grp_idx].num_generator; j++, i++) { 
      ni_Phases *ph = &(group_schedule_ptr[i].phase_array[0]);
      ph->default_phase = 0;
      strncpy(ph->phase_name, phase_name, sizeof(ph->phase_name) - 1);
    }
  }
  num_fields = get_tokens_(buf, fields, " ", 20);
  
  if (num_fields < 2) {
    NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011310, 
                      "Atleast one argument required after 'START' for 'Start' Phase");
    //usage_parse_schedule_phase_start("Atleast one argument required after START");
  }
  
  if (strcmp(fields[1], "IMMEDIATELY") == 0) {
    if (schedule_by == SCHEDULE_BY_SCENARIO) {
      tmp_ph->phase_cmd.start_phase.time = 0;
      tmp_ph->phase_cmd.start_phase.dependent_grp = -1;
      tmp_ph->phase_cmd.start_phase.start_mode = START_MODE_IMMEDIATE;
    } else {
      for (j = 0, i = grp_idx; j < scen_grp_entry[grp_idx].num_generator; j++, i++) {
        ni_Phases *ph = &(group_schedule_ptr[i].phase_array[0]);
        NIDL (3, "Filling schedule struct for group-id = %d", i);
        ph->phase_cmd.start_phase.time = 0;
        ph->phase_cmd.start_phase.dependent_grp = -1;
        ph->phase_cmd.start_phase.start_mode = START_MODE_IMMEDIATE;
      }
    }
  } else if (strcmp(fields[1], "AFTER") == 0) {
    NIDL (3, "AFTER option used");
    if (num_fields < 3) {
      NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011311, 
                        "Atleast one argument required after 'AFTER' for 'Start' phase");
      //usage_parse_schedule_phase_start("Atleast one argument required after AFTER");
    }
    
    if (strcmp(fields[2], "TIME") == 0) {
      NIDL (3, "AFTER option used with TIME format");
      if (num_fields < 4) {
        NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011312, "Invalid time format for 'Start' phase");
        //usage_parse_schedule_phase_start("Time in format HH:MM:SS required after TIME");
      }
      if (schedule_by == SCHEDULE_BY_SCENARIO) {  
        tmp_ph->phase_cmd.start_phase.time = ni_get_time_from_format(fields[3]);
      } else {
        for (j = 0, i = grp_idx; j < scen_grp_entry[grp_idx].num_generator; j++, i++) {
          NIDL (3, "Filling schedule struct for group-id = %d", i);
          ni_Phases *ph = &(group_schedule_ptr[i].phase_array[0]);
          ph->phase_cmd.start_phase.time = ni_get_time_from_format(fields[3]);
        }
      } 
      if (schedule_by == SCHEDULE_BY_SCENARIO) {
        if (tmp_ph->phase_cmd.start_phase.time < 0) {
          NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011312, 
                            "Invalid time format for 'Start' phase in case of start after time.");
          //usage_parse_schedule_phase_start("Invalid time format");       
        }
        tmp_ph->phase_cmd.start_phase.dependent_grp = -1;
        tmp_ph->phase_cmd.start_phase.start_mode = START_MODE_AFTER_TIME;
        NIDL (3, "parsing grp time  = %u\n", 
                       tmp_ph->phase_cmd.start_phase.time);
      } else {
        ni_Phases *ph = &(group_schedule_ptr[i].phase_array[0]); 
        if (ph->phase_cmd.start_phase.time < 0) {
          NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011312, 
                            "Invalid time format for 'Start' phase in case of start after time.");
          //usage_parse_schedule_phase_start("Invalid time format");
        }
        for (j = 0, i = grp_idx; j < scen_grp_entry[grp_idx].num_generator; j++, i++){ 
          ni_Phases *ph = &(group_schedule_ptr[i].phase_array[0]);
          ph->phase_cmd.start_phase.dependent_grp = -1;
          ph->phase_cmd.start_phase.start_mode = START_MODE_AFTER_TIME;
          NIDL (3, "parsing grp time  = %u\n",
                       ph->phase_cmd.start_phase.time);
        }
      } 
    } else if (strcmp(fields[2], "GROUP") == 0) {
      NIDL (1, "AFTER option used with GROUP");
      if (schedule_by == SCHEDULE_BY_SCENARIO) {
        NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011313, 
                          "Invalid option after 'AFTER GROUP' for 'Start' phase in case of Schedule By Scenario.");
        //usage_parse_schedule_phase_start("Invalid option for phase 'START'");
      } 
      else 
      {
        NIDL (1, "ELSE part, group_idx = %d", grp_idx);
        if (num_fields < 4) 
        {
          NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011313, 
                            "Group name must be specified for 'Start' phase in case of start after group.");
          //usage_parse_schedule_phase_start("Group must be specified after GROUP");
        }
        for (j = 0, i = grp_idx; j < scen_grp_entry[grp_idx].num_generator; j++, i++) 
        {
          NIDL (1, "Entering loop for group_idx = %d", i);
          if ((dependent_grp_idx = find_dependent_grp_index(fields[3], i, save_dependent_gp_idx)) >= 0) 
          {
            NIDL (3, "dependent_grp_idx = %d, cur_grp_idx = %d", dependent_grp_idx, i);
            ni_Phases *ph = &(group_schedule_ptr[i].phase_array[0]);
            ph->phase_cmd.start_phase.dependent_grp = dependent_grp_idx;
            ph->phase_cmd.start_phase.start_mode = START_MODE_AFTER_GROUP;
        

            if (num_fields > 5) {
              NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011313, 
                                "Invalid number of argument for 'Start' phase in case of start after group.");
              //usage_parse_schedule_phase_start("Invalid keyword START");
            }

            if (num_fields == 5) { /* time */
              ni_Phases *ph = &(group_schedule_ptr[i].phase_array[0]);
              ph->phase_cmd.start_phase.time = ni_get_time_from_format(fields[4]);
              if (ph->phase_cmd.start_phase.time < 0) {
                //fprintf(stderr, "Invalid time format in %s\n", buf_backup);
                NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011313,
                                "Invalid time format for 'Start' phase in case of start after group with delay.");
              }
            } else {
              ni_Phases *ph = &(group_schedule_ptr[i].phase_array[0]);
              ph->phase_cmd.start_phase.time = 0; 
            } 
          } else {//if block
            NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011313, "Invalid group name for 'Start' phase.");
            //usage_parse_schedule_phase_start("Invalid group name");
          }
        }    
      }
    } else { //if (GROUP)
      NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011310, "Invalid option for 'Start' phase.");
      //usage_parse_schedule_phase_start("Invalid option with START");
    }
  }
  save_dependent_gp_idx = -1; /*Reset static var for next group*/
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
static int ni_parse_schedule_phase_ramp_up(int grp_idx, char *full_buf, char *phase_name, char *err_msg)
{
  ni_Phases *ph, *tmp_ph;
  char *fields[20];
  char buf_backup[MAX_DATA_LINE_LENGTH];
  int group_mode;
  char *buf, *buf_more;
  int num_fields;
  int i, j, cur_phase_idx;
  
  NIDL (4, "Method called grp_idx = %d, full_buf = %s, phase_name = %s", grp_idx, full_buf, phase_name);

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

  if (schedule_type == SCHEDULE_TYPE_SIMPLE) 
  {
    if (schedule_by == SCHEDULE_BY_SCENARIO) 
    {
      ph = &(scenario_schedule_ptr->phase_array[1]);                /* 2nd Phase Ramp UP */
      if (ph->default_phase)
        tmp_ph = ph;
      else {
        NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011314, 
                          "Can not have more than one Ramp Up phase in Simple Schedule.");
       // usage_parse_schedule_phase_ramp_up("Can not have more than one Ramp Up phase in Simple Schedule");
      }
      strncpy(ph->phase_name, phase_name, sizeof(ph->phase_name) - 1);
    } else {
      for (j = 0, i = grp_idx; j < scen_grp_entry[grp_idx].num_generator; j++, i++)
      { 
        ph = &(group_schedule_ptr[i].phase_array[1]);
        if (ph->default_phase)
          NIDL (1, "Default phase defined");
        else {
          NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011314, 
                            "Can not have more than one Ramp Up phase in Simple Schedule.");
          //usage_parse_schedule_phase_ramp_up("Can not have more than one Ramp Up phase in Simple Schedule");
        }
        strncpy(ph->phase_name, phase_name, sizeof(ph->phase_name) - 1);
      }
    }
  } else {                         /* add Another */
    if (schedule_by == SCHEDULE_BY_SCENARIO)
      tmp_ph = add_schedule_ph(scenario_schedule_ptr, SCHEDULE_PHASE_RAMP_UP, phase_name, &cur_phase_idx);   /* This adds in the end and returns 
                                                                                  * the pointer of newly added node */
    else 
    {
      for (j = 0, i = grp_idx; j < scen_grp_entry[grp_idx].num_generator; j++, i++)
      {
        tmp_ph = add_schedule_ph(&group_schedule_ptr[i], SCHEDULE_PHASE_RAMP_UP, phase_name, &cur_phase_idx); 
      }
    }
  }

  num_fields = get_tokens_(buf, fields, " ", 20);
  group_mode = get_grp_mode(grp_idx);

  if (num_fields < 3) {
    NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011314, "Invalid number of argument for 'Ramp Up' phase.");
    //usage_parse_schedule_phase_ramp_up("Atleast two arguments required with RAMP_UP");
  }
  if (schedule_by == SCHEDULE_BY_SCENARIO) {
    tmp_ph->default_phase = 0;
  } else {
    for (j = 0, i = grp_idx; j < scen_grp_entry[grp_idx].num_generator; j++, i++){ 
      if (schedule_type == SCHEDULE_TYPE_SIMPLE)
        ph = &(group_schedule_ptr[i].phase_array[1]);
      else
        ph = &(group_schedule_ptr[i].phase_array[cur_phase_idx]);
      ph->default_phase = 0;
    }
  }
  /* Validation */
  if (group_mode == TC_FIX_CONCURRENT_USERS) {
    if (strcmp(fields[2], "IMMEDIATELY") && strcmp(fields[2], "STEP") &&
        strcmp(fields[2], "RATE") && strcmp(fields[2], "TIME")) {
      NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011340, fields[2]);
      //usage_parse_schedule_phase_ramp_up("Unknown Ramp Up mode for Fixed Concurrent Users scenarios. "
        //                                 "It can be : IMMEDIATELY, STEP, RATE or TIME");
    }
  } else if (group_mode == TC_FIX_USER_RATE) {
    if (strcmp(fields[2], "IMMEDIATELY") && strcmp(fields[2], "TIME_SESSIONS")) {
      NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011341, fields[2]);
      //usage_parse_schedule_phase_ramp_up("Unknown Ramp Up mode for Fixed Sessions Rate scenarios. It can be : IMMEDIATELY or TIME_SESSIONS");
    }
  }

  char *users_or_sess_rate = fields[1];
  char *ramp_up_mode = fields[2];
  
  /* Users_or_sess_rate can only be ALL in Simple mode and > 0 for others */
  if ((schedule_type == SCHEDULE_TYPE_SIMPLE) &&
      strcmp(users_or_sess_rate, "ALL")) {
    NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011314, "User or Session rate can only be 'ALL' "
                      "for Schedule Type 'Simple' in 'Ramp Up' phase.");
    //usage_parse_schedule_phase_ramp_up("For Simple Schedule, users_or_sess_rate can only be ALL");
  } else if (schedule_type == SCHEDULE_TYPE_ADVANCED) {
    if (!strcmp(users_or_sess_rate, "ALL")) {
      NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011314, "User or session rate can not be 'ALL' "
                        "for Schedule Type 'Advance' in 'Ramp Up' phase.");
      //usage_parse_schedule_phase_ramp_up("For Advanced Schedule, users_or_sess_rate can not be ALL");
    } else if (atoi(users_or_sess_rate) <= 0) {  
      NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011314, "User or Session rate specified for "
                        "Schedule Type Advanced can not be less than or equal to zero.");
      //usage_parse_schedule_phase_ramp_up("Users_or_sess_rate specified for Advanced Schedule can not be less than or equal to zero");
    }
  }

  /* Fill mode */
  if (strcmp(ramp_up_mode, "IMMEDIATELY") == 0) {
    if (schedule_by == SCHEDULE_BY_SCENARIO) { 
      tmp_ph->phase_cmd.ramp_up_phase.ramp_up_mode = RAMP_UP_MODE_IMMEDIATE;
    } else {
      for (j = 0, i = grp_idx; j < scen_grp_entry[grp_idx].num_generator; j++, i++){ 
        if (schedule_type == SCHEDULE_TYPE_SIMPLE)
          ph = &(group_schedule_ptr[i].phase_array[1]);
        else
          ph = &(group_schedule_ptr[i].phase_array[cur_phase_idx]);
        ph->phase_cmd.ramp_up_phase.ramp_up_mode = RAMP_UP_MODE_IMMEDIATE;
      } 
    }
  } else if (strcmp(ramp_up_mode, "STEP") == 0) {
    if (num_fields < 5) {
      NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011315, 
                        "Invalid number of arguments with 'STEP' mode for 'Ramp Up' phase.");
      //usage_parse_schedule_phase_ramp_up("Atleast two arguments required with STEP");
    }
    if (schedule_by == SCHEDULE_BY_SCENARIO) {
      tmp_ph->phase_cmd.ramp_up_phase.ramp_up_mode = RAMP_UP_MODE_STEP;
      tmp_ph->phase_cmd.ramp_up_phase.ramp_up_step_users = atoi(fields[3]);
      tmp_ph->phase_cmd.ramp_up_phase.ramp_up_step_time = ni_get_time_from_format(fields[4]);
      if (tmp_ph->phase_cmd.ramp_up_phase.ramp_up_step_time < 0) {
        NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011315, 
                          "Time specified with 'STEP' mode is invalid for 'Ramp Up' phase.");
        //usage_parse_schedule_phase_ramp_up("Time specified with STEP is invalid");
      }
    } else {
      for (j = 0, i = grp_idx; j < scen_grp_entry[grp_idx].num_generator; j++, i++){ 
        if (schedule_type == SCHEDULE_TYPE_SIMPLE)
          ph = &(group_schedule_ptr[i].phase_array[1]);
        else
          ph = &(group_schedule_ptr[i].phase_array[cur_phase_idx]);
        ph->phase_cmd.ramp_up_phase.ramp_up_mode = RAMP_UP_MODE_STEP;
        ph->phase_cmd.ramp_up_phase.ramp_up_step_users = atoi(fields[3]);
        ph->phase_cmd.ramp_up_phase.ramp_up_step_time = ni_get_time_from_format(fields[4]);
        if (ph->phase_cmd.ramp_up_phase.ramp_up_step_time < 0) {
          NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011315, 
                            "Time specified with 'STEP' mode is invalid for 'Ramp Up' phase.");
          //usage_parse_schedule_phase_ramp_up("Time specified with STEP is invalid");
        }
      }
    } 
  } else if (strcmp(ramp_up_mode, "RATE") == 0) {
    char tmp_buf[1024];
    if (schedule_by == SCHEDULE_BY_SCENARIO) {
      tmp_ph->phase_cmd.ramp_up_phase.ramp_up_mode = RAMP_UP_MODE_RATE;
    } else {
      for (j = 0, i = grp_idx; j < scen_grp_entry[grp_idx].num_generator; j++, i++){ 
        if (schedule_type == SCHEDULE_TYPE_SIMPLE)
          ph = &(group_schedule_ptr[i].phase_array[1]);
        else
          ph = &(group_schedule_ptr[i].phase_array[cur_phase_idx]);
        ph->phase_cmd.ramp_up_phase.ramp_up_mode = RAMP_UP_MODE_RATE;
      }
    }
    if (num_fields < 5) {
      NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011316, 
                        "Invalid number of argument with 'RATE' mode for 'Ramp Up' phase.");
      //usage_parse_schedule_phase_ramp_up("Atleast two arguments required with RATE");
    }
    /*Need to multiply ramp-up rate with 1000, to support upto 3 decimal*/
    double ramp_rate_modify = atof(fields[3]);
    ramp_rate_modify = ramp_rate_modify * SESSION_RATE_MULTIPLIER;
    sprintf(tmp_buf, "DUMMY_KW %f %s", ramp_rate_modify, fields[4]); /* Sending dummy kw */
    if (schedule_by == SCHEDULE_BY_SCENARIO) {
      tmp_ph->phase_cmd.ramp_up_phase.ramp_up_rate = convert_to_per_min(tmp_buf); /* Decimal howto ?? */
    } else {
      for (j = 0, i = grp_idx; j < scen_grp_entry[grp_idx].num_generator; j++, i++){ 
        if (schedule_type == SCHEDULE_TYPE_SIMPLE)
          ph = &(group_schedule_ptr[i].phase_array[1]);
        else
          ph = &(group_schedule_ptr[i].phase_array[cur_phase_idx]);
        ph->phase_cmd.ramp_up_phase.ramp_up_rate = convert_to_per_min(tmp_buf); 
      }
    }
    /* pattern */
    if(num_fields < 6)
    {
      NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011316, 
                        "Ramp Up pattern is not specified for 'RATE' mode. It should be LINEARLY or RANDOMLY.");
       //usage_parse_schedule_phase_ramp_up("<ramp_up_pattern>  not specified. Should be LINEARLY or RANDOMLY");
    }
    else if (strcmp(fields[5], "LINEARLY") == 0) {
      if (schedule_by == SCHEDULE_BY_SCENARIO) {
        tmp_ph->phase_cmd.ramp_up_phase.ramp_up_pattern = RAMP_UP_PATTERN_LINEAR;
      } else {
        for (j = 0, i = grp_idx; j < scen_grp_entry[grp_idx].num_generator; j++, i++){ 
          if (schedule_type == SCHEDULE_TYPE_SIMPLE)
            ph = &(group_schedule_ptr[i].phase_array[1]);
          else
            ph = &(group_schedule_ptr[i].phase_array[cur_phase_idx]);
          ph->phase_cmd.ramp_up_phase.ramp_up_pattern = RAMP_UP_PATTERN_LINEAR;
        }
      }
    } else if (strcmp(fields[5], "RANDOMLY") == 0) {
      if (schedule_by == SCHEDULE_BY_SCENARIO) {
        tmp_ph->phase_cmd.ramp_up_phase.ramp_up_pattern = RAMP_UP_PATTERN_RANDOM;
      } else {
        for (j = 0, i = grp_idx; j < scen_grp_entry[grp_idx].num_generator; j++, i++){ 
          if (schedule_type == SCHEDULE_TYPE_SIMPLE)
            ph = &(group_schedule_ptr[i].phase_array[1]);
          else
            ph = &(group_schedule_ptr[i].phase_array[cur_phase_idx]);
          ph->phase_cmd.ramp_up_phase.ramp_up_pattern = RAMP_UP_PATTERN_RANDOM;
        } 
      }
    } else {                    /* Unknown pattern */
      NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011342, fields[5]);
      //usage_parse_schedule_phase_ramp_up("Unknown Pattern");
    }

  } else if (strcmp(ramp_up_mode, "TIME") == 0) {

    if (num_fields < 5) {
      NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011317, 
                        "Invalid number of argument provided in 'TIME' mode for 'Ramp Up' phase");
      //usage_parse_schedule_phase_ramp_up("Atleast two arguments required with TIME");
    }
    if (schedule_by == SCHEDULE_BY_SCENARIO) {
      tmp_ph->phase_cmd.ramp_up_phase.ramp_up_mode = RAMP_UP_MODE_TIME;
      tmp_ph->phase_cmd.ramp_up_phase.ramp_up_time = ni_get_time_from_format(fields[3]);

      if (tmp_ph->phase_cmd.ramp_up_phase.ramp_up_time < 0) {
        NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011317, 
                          "Time specified with 'TIME' mode for 'Ramp Up' phase is invalid.");
        //usage_parse_schedule_phase_ramp_up("Time specified is invalid");
      } else if (tmp_ph->phase_cmd.ramp_up_phase.ramp_up_time == 0)  // Ramp up time is 0 then do immediate
        tmp_ph->phase_cmd.ramp_up_phase.ramp_up_mode = RAMP_UP_MODE_IMMEDIATE;
    } else {
      for (j = 0, i = grp_idx; j < scen_grp_entry[grp_idx].num_generator; j++, i++){ 
        if (schedule_type == SCHEDULE_TYPE_SIMPLE)
          ph = &(group_schedule_ptr[i].phase_array[1]);
        else
          ph = &(group_schedule_ptr[i].phase_array[cur_phase_idx]);
        ph->phase_cmd.ramp_up_phase.ramp_up_mode = RAMP_UP_MODE_TIME;
        ph->phase_cmd.ramp_up_phase.ramp_up_time = ni_get_time_from_format(fields[3]);

        if (ph->phase_cmd.ramp_up_phase.ramp_up_time < 0) {
          NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011317,
                            "Time specified with 'TIME' mode for 'Ramp Up' phase is invalid.");
          //usage_parse_schedule_phase_ramp_up("Time specified is invalid");
        } else if (ph->phase_cmd.ramp_up_phase.ramp_up_time == 0)  // Ramp up time is 0 then do immediate
          ph->phase_cmd.ramp_up_phase.ramp_up_mode = RAMP_UP_MODE_IMMEDIATE;
      }
    }
    if (strcmp(fields[4], "LINEARLY") == 0) {
      if (schedule_by == SCHEDULE_BY_SCENARIO) {
        tmp_ph->phase_cmd.ramp_up_phase.ramp_up_pattern = RAMP_UP_PATTERN_LINEAR;
      } else {
        for (i = grp_idx; i < scen_grp_entry[grp_idx].num_generator; i++) {
          if (schedule_type == SCHEDULE_TYPE_SIMPLE)
            ph = &(group_schedule_ptr[i].phase_array[1]);
          else
            ph = &(group_schedule_ptr[i].phase_array[cur_phase_idx]);
          ph->phase_cmd.ramp_up_phase.ramp_up_pattern = RAMP_UP_PATTERN_LINEAR;
        }
      }
    } else if (strcmp(fields[4], "RANDOMLY") == 0) {
      if (schedule_by == SCHEDULE_BY_SCENARIO) {
        tmp_ph->phase_cmd.ramp_up_phase.ramp_up_pattern = RAMP_UP_PATTERN_RANDOM;
      } else {
        for (j = 0, i = grp_idx; j < scen_grp_entry[grp_idx].num_generator; j++, i++){ 
          if (schedule_type == SCHEDULE_TYPE_SIMPLE)
            ph = &(group_schedule_ptr[i].phase_array[1]);
          else
            ph = &(group_schedule_ptr[i].phase_array[cur_phase_idx]);
          ph->phase_cmd.ramp_up_phase.ramp_up_pattern = RAMP_UP_PATTERN_RANDOM;
        }
      }
    } else {                    /* Unknown pattern */
      NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011343, fields[4]);
      //usage_parse_schedule_phase_ramp_up("Unknown Pattern");
    }
    
  } else if (strcmp(ramp_up_mode, "TIME_SESSIONS") == 0) {
    
    if (num_fields < 5) {
      NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011318, "Invalid number of argument with 'TIME_SESSIONS' mode for 'Ramp Up' phase");
      //usage_parse_schedule_phase_ramp_up("Atleast two arguments required with TIME_SESSIONS");
    }
    if (schedule_by == SCHEDULE_BY_SCENARIO) 
    {
    /* TIME_SESSIONS <ramp_up_time in HH:MM:SS> <mode> <step_time or num_steps> 
       <mode> 0|1|2 (default steps, steps of x seconds, total steps) */
      tmp_ph->phase_cmd.ramp_up_phase.ramp_up_mode = RAMP_UP_MODE_TIME_SESSIONS;
      tmp_ph->phase_cmd.ramp_up_phase.ramp_up_time = ni_get_time_from_format(fields[3]);
      if (tmp_ph->phase_cmd.ramp_up_phase.ramp_up_time < 0) {
        NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011318, 
                          "Time specified with 'TIME_SESSIONS' mode for 'Ramp Up' phase is invalid.");
      //  usage_parse_schedule_phase_ramp_up("Time specified is invalid");
      } else if (tmp_ph->phase_cmd.ramp_up_phase.ramp_up_time == 0)  { // Ramp up time is 0 then do immediate
        tmp_ph->phase_cmd.ramp_up_phase.ramp_up_mode = RAMP_UP_MODE_IMMEDIATE;
      }
/* ramp_up_time  tot_num_steps_for_sessions 
 * 0-179   secs    2
 * 180-239 secs    3
 * 240-299 secs    4
 */
      if (tmp_ph->phase_cmd.ramp_up_phase.ramp_up_time > 0) 
      {
        double temp;
        int step_mode = atoi(fields[4]);
        if (step_mode == 0) {
          tmp_ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions = 2;
          int floor = tmp_ph->phase_cmd.ramp_up_phase.ramp_up_time/(1000*60); // chk it once
          if (floor > 1)
            tmp_ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions = floor;
          temp = (double)(tmp_ph->phase_cmd.ramp_up_phase.ramp_up_time/1000)/(double)tmp_ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions;
          tmp_ph->phase_cmd.ramp_up_phase.ramp_up_step_time = round(temp);
          tmp_ph->phase_cmd.ramp_up_phase.step_mode = DEFAULT_STEP;
        }
        else if (step_mode == 1) {
          if (num_fields < 6) {
        NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011318, 
                          "Step time or num steps are not specified with 'TIME_SESSIONS' mode for 'Ramp Up' phase.");
        // usage_parse_schedule_phase_ramp_up("Step time or num steps should be specified");
          }
          tmp_ph->phase_cmd.ramp_up_phase.ramp_up_step_time = atoi(fields[5]);
          tmp_ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions = tmp_ph->phase_cmd.ramp_up_phase.ramp_up_step_time;
          if (tmp_ph->phase_cmd.ramp_up_phase.ramp_up_step_time <= 0){
              NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011318, 
                                "Ramp Up step time with 'TIME_SESSIONS' mode is not valid time.");
           // NS_EXIT(-1, "Invalid step time '%d' at line [%s]", tmp_ph->phase_cmd.ramp_up_phase.ramp_up_step_time, buf_backup);
          }          
          if ((tmp_ph->phase_cmd.ramp_up_phase.ramp_up_time/1000) < tmp_ph->phase_cmd.ramp_up_phase.ramp_up_step_time){
              NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011318, 
                                "Ramp Up time can not be less then step time for 'TIME_SESSIONS' mode.");
            //NS_EXIT(-1, "Ramp Up time can not be less then step time at line [%s]", buf_backup);
          }          
          temp = (double)(tmp_ph->phase_cmd.ramp_up_phase.ramp_up_time/1000)/(double)tmp_ph->phase_cmd.ramp_up_phase.ramp_up_step_time;
          NIDL (4, "tmp_ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions =%d", tmp_ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions);
          tmp_ph->phase_cmd.ramp_up_phase.step_mode = STEP_TIME;
        }
        else if (step_mode == 2) {
          if (num_fields < 6) {
            NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011318,
                              "Num steps with 'TIME_SESSIONS' mode is not specified for 'Ramp Up' phase.");
            //usage_parse_schedule_phase_ramp_up("Step time or num steps should be specified");
          }
          tmp_ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions = atoi(fields[5]);
          NIDL (4, "tmp_ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions =%d", tmp_ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions);
          if (tmp_ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions <= 0){
            NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011318,
                              "Num steps can not less than or equal to zero with 'TIME_SESSIONS' mode for 'Ramp Up' phase.");
            //NS_EXIT(-1, "Invalid num steps '%d' at line [%s]", tmp_ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions, buf_backup);
          }          
          temp = (double)(tmp_ph->phase_cmd.ramp_up_phase.ramp_up_time/1000)/(double)tmp_ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions;
          // step time can not be 0 
          tmp_ph->phase_cmd.ramp_up_phase.ramp_up_step_time = (round(temp))?round(temp):1;
          tmp_ph->phase_cmd.ramp_up_phase.step_mode = NUM_STEPS;
        }
        else{
          NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011318, 
                            "Invalid step mode is selected with 'TIME_SESSIONS' mode for 'Ramp Down' phase.");
        }
      }
    } 
    else 
    {
        
      /* TIME_SESSIONS <ramp_up_time in HH:MM:SS> <mode> <step_time or num_steps> 
       <mode> 0|1|2 (default steps, steps of x seconds, total steps) */
      for (j = 0, i = grp_idx; j < scen_grp_entry[grp_idx].num_generator; j++, i++){ 
        if (schedule_type == SCHEDULE_TYPE_SIMPLE)
          ph = &(group_schedule_ptr[i].phase_array[1]);
        else
          ph = &(group_schedule_ptr[i].phase_array[cur_phase_idx]);
        ph->phase_cmd.ramp_up_phase.ramp_up_mode = RAMP_UP_MODE_TIME_SESSIONS;
        ph->phase_cmd.ramp_up_phase.ramp_up_time = ni_get_time_from_format(fields[3]);
        if (ph->phase_cmd.ramp_up_phase.ramp_up_time < 0) {
         NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011318,
                           "Ramp Up step time with 'TIME_SESSIONS' mode is not valid time.");
        //  usage_parse_schedule_phase_ramp_up("Time specified is invalid");
        } else if (ph->phase_cmd.ramp_up_phase.ramp_up_time == 0)  { // Ramp up time is 0 then do immediate
          ph->phase_cmd.ramp_up_phase.ramp_up_mode = RAMP_UP_MODE_IMMEDIATE;
        }
/* ramp_up_time  tot_num_steps_for_sessions 
 * 0-179   secs    2
 * 180-239 secs    3
 * 240-299 secs    4
 */
        if (ph->phase_cmd.ramp_up_phase.ramp_up_time > 0) 
        {
          double temp;
          int step_mode = atoi(fields[4]);
          if (step_mode == 0) {
            ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions = 2;
            int floor = ph->phase_cmd.ramp_up_phase.ramp_up_time/(1000*60); // chk it once
            if (floor > 1)
              ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions = floor;
            temp = (double)(ph->phase_cmd.ramp_up_phase.ramp_up_time/1000)/(double)ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions;
            ph->phase_cmd.ramp_up_phase.ramp_up_step_time = round(temp);
            ph->phase_cmd.ramp_up_phase.step_mode = DEFAULT_STEP;
          }
          else if (step_mode == 1) {
            if (num_fields < 6) {
              NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011318, 
                                "Step time or num steps are not specified with 'TIME_SESSIONS' mode for 'Ramp Up' phase.");
            // usage_parse_schedule_phase_ramp_up("Step time or num steps should be specified");
             }
             ph->phase_cmd.ramp_up_phase.ramp_up_step_time = atoi(fields[5]);
             ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions = ph->phase_cmd.ramp_up_phase.ramp_up_step_time;
             if (ph->phase_cmd.ramp_up_phase.ramp_up_step_time <= 0){
             NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011318,
                               "Ramp Up step time with 'TIME_SESSIONS' mode is not valid time.");
             // NS_EXIT(-1, "Invalid step time '%d' at line [%s]", ph->phase_cmd.ramp_up_phase.ramp_up_step_time, buf_backup);
             }          
             if ((ph->phase_cmd.ramp_up_phase.ramp_up_time/1000) < ph->phase_cmd.ramp_up_phase.ramp_up_step_time){
	     NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011318, 
                               "Ramp Up time can not be less then step time for 'TIME_SESSIONS' mode.");
             //NS_EXIT(-1, "Ramp Up time can not be less then step time at line [%s]", buf_backup);
             }          
             temp = (double)(ph->phase_cmd.ramp_up_phase.ramp_up_time/1000)/(double)ph->phase_cmd.ramp_up_phase.ramp_up_step_time;
             NIDL (4, "ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions =%d", ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions);
             ph->phase_cmd.ramp_up_phase.step_mode = STEP_TIME;
          }
          else if (step_mode == 2) 
          {
            if (num_fields < 6) {
            NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011318,
                              "Number of steps with 'TIME_SESSIONS' mode is not specified for 'Ramp Up' phase.");
                              
            // usage_parse_schedule_phase_ramp_up("Step time or num steps should be specified");
            }
            ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions = atoi(fields[5]);
            NIDL (4, "ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions =%d", ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions);
            if (ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions <= 0){
             NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011318,
                               "Number of steps can not less than or equal to zero with 'TIME_SESSIONS' mode for 'Ramp Up' phase."); 
            //NS_EXIT(-1, "Invalid num steps '%d' at line [%s]", ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions, buf_backup);
            }          
            temp = (double)(ph->phase_cmd.ramp_up_phase.ramp_up_time/1000)/(double)ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions;
            // step time can not be 0 
            ph->phase_cmd.ramp_up_phase.ramp_up_step_time = (round(temp))?round(temp):1;
            ph->phase_cmd.ramp_up_phase.step_mode = NUM_STEPS;
          } else {
            NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011318, 
                              "Invalid step mode is selected with 'TIME_SESSIONS' mode for 'Ramp Down' phase.");
           //NS_EXIT(-1, "Invalid step mode specified in line [%s]", buf_backup);
          } 
        }
      }//for loop
    }//else
  }
  /* Fill users_or_sess_rate */
  if (strcmp(users_or_sess_rate, "ALL") == 0) {
    if (schedule_by == SCHEDULE_BY_SCENARIO) {
      tmp_ph->phase_cmd.ramp_up_phase.num_vusers_or_sess = -1; /* Fill -1 for ALL for now, later filled during validation */
    } else {
      for (j = 0, i = grp_idx; j < scen_grp_entry[grp_idx].num_generator; j++, i++){ 
         ph = &(group_schedule_ptr[i].phase_array[1]);
         ph->phase_cmd.ramp_up_phase.num_vusers_or_sess = -1;
      }
    }
  } else if (group_mode == TC_FIX_USER_RATE) { /* FSR */
    if (schedule_by == SCHEDULE_BY_SCENARIO) {
      tmp_ph->phase_cmd.ramp_up_phase.num_vusers_or_sess = compute_sess_rate(users_or_sess_rate); 
    } else {
      for (j = 0, i = grp_idx; j < scen_grp_entry[grp_idx].num_generator; j++, i++){ 
        if (schedule_type == SCHEDULE_TYPE_SIMPLE) 
          ph = &(group_schedule_ptr[i].phase_array[1]);
        else
          ph = &(group_schedule_ptr[i].phase_array[cur_phase_idx]);
        ph->phase_cmd.ramp_up_phase.num_vusers_or_sess = compute_sess_rate(users_or_sess_rate);
      }
    }
  } else {
    if (schedule_by == SCHEDULE_BY_SCENARIO) {
      tmp_ph->phase_cmd.ramp_up_phase.num_vusers_or_sess = atoi(users_or_sess_rate);
    } else {
      for (j = 0, i = grp_idx; j < scen_grp_entry[grp_idx].num_generator; j++, i++){ 
        if (schedule_type == SCHEDULE_TYPE_SIMPLE)
          ph = &(group_schedule_ptr[i].phase_array[1]);
        else
          ph = &(group_schedule_ptr[i].phase_array[cur_phase_idx]);
        ph->phase_cmd.ramp_up_phase.num_vusers_or_sess = atoi(users_or_sess_rate);
      }
    }
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
static int ni_parse_schedule_phase_duration(int grp_idx, char *full_buf, char *phase_name, int set_rtc_flag, char *err_msg)
{
  ni_Phases *ph, *tmp_ph;
  char *fields[20];
  char buf_backup[MAX_DATA_LINE_LENGTH];
  char *buf;
  char *buf_more;
  int num_fields;
  int i, j, cur_phase_idx;
  int per_user_session_flag = 0;
  NIDL (4, "Method called, grp_idx =%d, full_buf = %s, phase_name = %s", grp_idx, full_buf, phase_name);

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

  if (schedule_type == SCHEDULE_TYPE_SIMPLE) {
    if (schedule_by == SCHEDULE_BY_SCENARIO) {
      ph = &(scenario_schedule_ptr->phase_array[3]);                /* 4rd Phase Duration, 3rd Stabilize */
      if (ph->default_phase) {
        tmp_ph = ph;
      } else {
        NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011319, 
                          "Can not have more than one 'Duration' phase in 'Simple' Scenario Type");
       //usage_parse_schedule_phase_duration("Can not have more than one Duration phase in Simple type scenario");
      }
      strncpy(ph->phase_name, phase_name, sizeof(ph->phase_name) - 1);
    } else {
      for (j = 0, i = grp_idx; j < scen_grp_entry[grp_idx].num_generator; j++, i++){ 
        ph = &(group_schedule_ptr[i].phase_array[3]);
        if (ph->default_phase) {
          NIDL (1, "Default phase define");
        } else {
        NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011319,
                          "Can not have more than one 'Duration' phase in 'Simple' Scenario Type");
        //usage_parse_schedule_phase_duration("Can not have more than one Duration phase in Simple type scenario");
        }
        strncpy(ph->phase_name, phase_name, sizeof(ph->phase_name) - 1);
      }
    }
  } else {                         /* add Another */
    if (schedule_by == SCHEDULE_BY_SCENARIO)
      tmp_ph = add_schedule_ph(scenario_schedule_ptr, SCHEDULE_PHASE_DURATION, phase_name, &cur_phase_idx);   /* This adds in the end and returns 
                                                                                  * the pointer of newly added node */
    else 
    {
      for (j = 0, i = grp_idx; j < scen_grp_entry[grp_idx].num_generator; j++, i++)
      {
        tmp_ph = add_schedule_ph(&group_schedule_ptr[i], SCHEDULE_PHASE_DURATION, phase_name, &cur_phase_idx); 
      }
    }
  }

  num_fields = get_tokens_(buf, fields, " ", 20);

  if (num_fields < 2) {
        NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011319, "Invalid number of argument with 'Duration' "
                          "phase. Atleast one field required with 'DURATION'");
       // usage_parse_schedule_phase_duration("Atleast one field required with DURATION");
  }

  if (schedule_by == SCHEDULE_BY_SCENARIO) {
    tmp_ph->default_phase = 0;
  } else {
    for (j = 0, i = grp_idx; j < scen_grp_entry[grp_idx].num_generator; j++, i++){ 
      if (schedule_type == SCHEDULE_TYPE_SIMPLE)
        ph = &(group_schedule_ptr[i].phase_array[3]);
      else
        ph = &(group_schedule_ptr[i].phase_array[cur_phase_idx]);
      ph->default_phase = 0;
    } 
  }

  if (!strcmp(fields[1], "SESSIONS")) {
    if (schedule_type != SCHEDULE_TYPE_SIMPLE){
        NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011320, "SESSIONS option can only be configured with "
                          "'Simple' Schedule Type scenario. Either change Schedule Type 'Advance to Simple' or change 'SESSIONS option "
                          "to DURATION' in global schedule");
       //NS_EXIT(-1, "SESSIONS (fetches) are only applicable with simple based schedule.");
    }

    if (get_grp_mode(grp_idx) == TC_FIX_USER_RATE) {
        NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011320, "SESSIONS option can only be configured with "
                          "'Simple' Schedule Type scenario. Either change Schedule Type 'Advance to Simple' or change 'SESSIONS option "
                           "to DURATION' in global schedule");
       //NS_EXIT(-1, "SESSIONS (fetches) are not allowed in fixed session rate scenarios.");
    }
  }

  if (strcmp(fields[1], "INDEFINITE") == 0) {
    if (schedule_by == SCHEDULE_BY_SCENARIO) {
      tmp_ph->phase_cmd.duration_phase.duration_mode = DURATION_MODE_INDEFINITE;
    } else {
      for (j = 0, i = grp_idx; j < scen_grp_entry[grp_idx].num_generator; j++, i++){ 
        if (schedule_type == SCHEDULE_TYPE_SIMPLE)
          ph = &(group_schedule_ptr[i].phase_array[3]);
        else
          ph = &(group_schedule_ptr[i].phase_array[cur_phase_idx]);
        ph->phase_cmd.duration_phase.duration_mode = DURATION_MODE_INDEFINITE;
      }
    }
  } else if (strcmp(fields[1], "TIME") == 0) {
    if (num_fields < 3) {
        NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011321, 
                          "Invalid number of argument with 'TIME' option in 'Duration' phase. Time must be specified with format 'HH:MM:SS'");
       //usage_parse_schedule_phase_duration("Time in format HH:MM:SS must be specified with TIME");
    }

    if (schedule_by == SCHEDULE_BY_SCENARIO) {
      tmp_ph->phase_cmd.duration_phase.duration_mode = DURATION_MODE_TIME;
      tmp_ph->phase_cmd.duration_phase.seconds = ni_get_time_from_format(fields[2]);
      NIDL (3, "seconds = %d\n", tmp_ph->phase_cmd.duration_phase.seconds);
      if (tmp_ph->phase_cmd.duration_phase.seconds < 0) {
        NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011321, "Time specified with 'TIME' option in 'Duration' phase is not valid time format. Time must be specified with format 'HH:MM:SS'");
        //usage_parse_schedule_phase_duration("Invalid time specified");
      }
    } else {
      for (j = 0, i = grp_idx; j < scen_grp_entry[grp_idx].num_generator; j++, i++){ 
        if (schedule_type == SCHEDULE_TYPE_SIMPLE)
          ph = &(group_schedule_ptr[i].phase_array[3]);
        else
          ph = &(group_schedule_ptr[i].phase_array[cur_phase_idx]);
        ph->phase_cmd.duration_phase.duration_mode = DURATION_MODE_TIME;
        ph->phase_cmd.duration_phase.seconds = ni_get_time_from_format(fields[2]);
        NIDL (3, "seconds = %d\n", ph->phase_cmd.duration_phase.seconds);
        if (ph->phase_cmd.duration_phase.seconds < 0) {
          NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011321, "Time specified with 'TIME' option in 'Duration' phase is not valid time format. Time must be specified with format 'HH:MM:SS'");
          //usage_parse_schedule_phase_duration("Invalid time specified");      
        }
      } 
    }
  } else if (strcmp(fields[1], "SESSIONS") == 0) {

    if (num_fields < 3) {
        NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011320, 
                          "Invalid number of argument with 'SESSIONS' option in 'Duration' mode");
        //usage_parse_schedule_phase_duration("Sessions must be specified with SESSIONS");
    }

    if(fields[3] && *fields[3]) {
      if(ns_is_numeric(fields[3]) == 0) {
        NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011320, "Number of sessions must be numeric with 'SESSIONS' option in 'Duration' phase");
        //usage_parse_schedule_phase_duration("Invalid per user session mode specified\nPer User Sessions should be numeric");
      }
      
      per_user_session_flag = atoi(fields[3]);
      if((per_user_session_flag < 0) || (per_user_session_flag > 1)) {
        NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011320,
                          "'Per User Sessions' value should be 0 or 1 with 'SESSIONS' option in 'Duration' phase");
        //usage_parse_schedule_phase_duration("Invalid per user session mode specified\nPer User Sessions value should be 0 or 1");
      }
    }

    if (schedule_by == SCHEDULE_BY_SCENARIO) {
      tmp_ph->phase_cmd.duration_phase.duration_mode = DURATION_MODE_SESSION;
      num_fetches = tmp_ph->phase_cmd.duration_phase.num_fetches = atoi(fields[2]);
      if ((ns_is_numeric(fields[2]) == 0) || (num_fetches <= 0)) {
        NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011320, "Number of sessions/fetches not configured properly in 'Duration' phase. It should be numeric and greater than zero");
        //usage_parse_schedule_phase_duration("Invalid fetches specified\nSESSIONS should be numeric and greater than zero(0)");
      }
      tmp_ph->phase_cmd.duration_phase.per_user_fetches_flag = per_user_session_flag;
    }
    else { /*group based sessions*/
      for (j = 0, i = grp_idx; j < scen_grp_entry[grp_idx].num_generator; j++, i++){
        if (schedule_type == SCHEDULE_TYPE_SIMPLE)
          ph = &(group_schedule_ptr[i].phase_array[3]);
        else
          ph = &(group_schedule_ptr[i].phase_array[cur_phase_idx]);
        ph->phase_cmd.duration_phase.duration_mode = DURATION_MODE_SESSION;
        ph->phase_cmd.duration_phase.num_fetches = atoi(fields[2]);
        if ((ns_is_numeric(fields[2]) == 0) || (ph->phase_cmd.duration_phase.num_fetches <= 0)) {
          NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011320, "Number of sessions/fetches not configured properly in 'Duration' phase. It should be numeric and greater than zero");
          //usage_parse_schedule_phase_duration("Invalid fetches specified\nSESSIONS should be numeric and greater than zero(0)");
        }
        ph->phase_cmd.duration_phase.per_user_fetches_flag = per_user_session_flag;
      }
    }
  } else {
        NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011320, "'Duration' phase is not configured properly");
       //usage_parse_schedule_phase_duration("Invalid format for DURATION");
  } 
  return 0;
}

/*"STABILIZATION Phase*/
static int ni_parse_schedule_phase_stabilize(int grp_idx, char *full_buf, char *phase_name, int set_rtc_flag, char *err_msg)
{
  ni_Phases *ph, *tmp_ph;
  char *fields[20];
  char buf_backup[MAX_DATA_LINE_LENGTH];
  char *buf;
  char *buf_more;
  int num_fields;
  int i, j, cur_phase_idx;

  NIDL (4, "Method called, grp_idx =%d, full_buf = %s, phase_name = %s", grp_idx, full_buf, phase_name);

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
  /* For schedule_by_scenario we can populate ni_Phases ptr,
   * But for schedule_by_group we need to update each group
   * hence will run a loop with respect to number of generator per group*/
  if (schedule_type == SCHEDULE_TYPE_SIMPLE) {
    if (schedule_by == SCHEDULE_BY_SCENARIO) {
      ph = &(scenario_schedule_ptr->phase_array[2]);                /* 3rd Phase Stabilize */
      if (ph->default_phase) {
        tmp_ph = ph;
      } else {
        NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011322, 
                          "Can not have more than one 'Stabilization' phase in 'Simple' Scenario Type");
        //usage_parse_schedule_phase_stabilize("Can not have more than one Stabilization phase in Simple type scenario");
      }
      strncpy(ph->phase_name, phase_name, sizeof(ph->phase_name) - 1);
    } else {
      for (j = 0, i = grp_idx; j < scen_grp_entry[grp_idx].num_generator; j++, i++){ 
        ph = &(group_schedule_ptr[i].phase_array[2]);
        if (ph->default_phase) {
          //tmp_ph = ph;
          NIDL (1, "Default stablization");
        } else {
          NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011322, 
                            "Can not have more than one 'Stabilization' phase in 'Simple' Scenario Type");
          //usage_parse_schedule_phase_stabilize("Can not have more than one Stabilization phase in Simple type scenario");
        }
        strncpy(ph->phase_name, phase_name, sizeof(ph->phase_name) - 1);
      }
    }
  } else {                         /* add Another */
    if (schedule_by == SCHEDULE_BY_SCENARIO)
      tmp_ph = add_schedule_ph(scenario_schedule_ptr, SCHEDULE_PHASE_STABILIZE, phase_name, &cur_phase_idx);   /* This adds in the end and returns 
                                                                                  * the pointer of newly added node */
    else 
    {
      for (j = 0, i = grp_idx; j < scen_grp_entry[grp_idx].num_generator; j++, i++)
      {
        tmp_ph = add_schedule_ph(&group_schedule_ptr[i], SCHEDULE_PHASE_STABILIZE, phase_name, &cur_phase_idx); 
      }
    }
  }
      
  num_fields = get_tokens_(buf, fields, " ", 20);
  
  if (num_fields < 3) {
       NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011322,
                         "Invalid number of argument with 'Stabilization' phase. Atleast two fields reqiured with 'STABILIZATION'");
      //usage_parse_schedule_phase_stabilize("Atleast two fields reqiured with STABILIZATION");
  }

  if (schedule_by == SCHEDULE_BY_SCENARIO) {
    tmp_ph->default_phase = 0;
  } else {
    for (j = 0, i = grp_idx; j < scen_grp_entry[grp_idx].num_generator; j++, i++){ 
      if (schedule_type == SCHEDULE_TYPE_SIMPLE)
        ph = &(group_schedule_ptr[i].phase_array[2]);
      else
        ph = &(group_schedule_ptr[i].phase_array[cur_phase_idx]);
      ph->default_phase = 0;
    }
  }

  if (strcmp(fields[1], "TIME") == 0) {
    if (schedule_by == SCHEDULE_BY_SCENARIO) {
      tmp_ph->phase_cmd.stabilize_phase.time = ni_get_time_from_format(fields[2]);
      if (tmp_ph->phase_cmd.stabilize_phase.time < 0) {
        NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011322, "Time specified with 'TIME' option in "
                          "'Stabilization' phase is not valid time format. Time must be specified with format 'HH:MM:SS'");
        //usage_parse_schedule_phase_stabilize("Invalid time specified with TIME");
      }
    } else {
      for (j = 0, i = grp_idx; j < scen_grp_entry[grp_idx].num_generator; j++, i++){ 
        if (schedule_type == SCHEDULE_TYPE_SIMPLE)
          ph = &(group_schedule_ptr[i].phase_array[2]);
        else
          ph = &(group_schedule_ptr[i].phase_array[cur_phase_idx]);
        ph->phase_cmd.stabilize_phase.time = ni_get_time_from_format(fields[2]);
        if (ph->phase_cmd.stabilize_phase.time < 0) {
          NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011323, "Time specified with 'TIME' option in "
                          "'Stabilization' phase is not valid time format. Time must be specified with format 'HH:MM:SS'");
          //usage_parse_schedule_phase_stabilize("Invalid time specified with TIME");
        }
      }
    }
  } else {
     NS_KW_PARSING_ERR(full_buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011322,
                       "Invalid number of argument with 'Stabilization' phase.");
    //fprintf(stderr, "Invalid format for STABILIZATION (%s)\n", buf_backup);
  }
  NIDL (1, "Exiting method");
  return 0;
}

/* RAMP_DOWN
 * */
static int ni_parse_schedule_phase_ramp_down(int grp_idx, char *full_buf, char *phase_name, char *err_msg)
{
  ni_Phases *ph, *tmp_ph;
  char *fields[20];
  char buf_backup[MAX_DATA_LINE_LENGTH];
  int group_mode;
  char *buf;
  char *buf_more;
  int num_fields;
  int i, j, cur_phase_idx;
  NIDL (4, "Method called, grp_idx =%d, full_buf = %s, phase_name = %s", grp_idx, full_buf, phase_name);

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
 
  if (schedule_type == SCHEDULE_TYPE_SIMPLE) {
    if (schedule_by == SCHEDULE_BY_SCENARIO) {
      ph = &(scenario_schedule_ptr->phase_array[4]); /* 5rd Phase Ramp Down */
      if (ph->default_phase) {
        tmp_ph = ph;
      } else {
        NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011324,
                          "Can not have more than one 'Ramp Down' phase in 'Simple' type Schedule.");
        //usage_parse_schedule_phase_ramp_down("Can not have more than one Ramp down phase in Simple type scenario");
      }
      strncpy(ph->phase_name, phase_name, sizeof(ph->phase_name) - 1);
    } else {
      for (j = 0, i = grp_idx; j < scen_grp_entry[grp_idx].num_generator; j++, i++){ 
        ph = &(group_schedule_ptr[i].phase_array[4]);
        if (ph->default_phase) {
          NIDL (1, "Default phase define");
        } else {
            NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011324,
                            "Can not have more than one 'Ramp Down' phase in 'Simple' type Schedule.");
            //usage_parse_schedule_phase_ramp_down("Can not have more than one Ramp down phase in Simple type scenario");
        }
        strncpy(ph->phase_name, phase_name, sizeof(ph->phase_name) - 1);
      }
    }
  } else {                         /* add Another */
    if (schedule_by == SCHEDULE_BY_SCENARIO)
      tmp_ph = add_schedule_ph(scenario_schedule_ptr, SCHEDULE_PHASE_RAMP_DOWN, phase_name, &cur_phase_idx);   /* This adds in the end and returns 
                                                                                  * the pointer of newly added node */
    else 
    {
      for (j = 0, i = grp_idx; j < scen_grp_entry[grp_idx].num_generator; j++, i++)
      {
        tmp_ph = add_schedule_ph(&group_schedule_ptr[i], SCHEDULE_PHASE_RAMP_DOWN, phase_name, &cur_phase_idx); 
      }
    }
  }

  num_fields = get_tokens_(buf, fields, " ", 20);
  
  group_mode = get_grp_mode(grp_idx);

  if (schedule_by == SCHEDULE_BY_SCENARIO) {
    tmp_ph->default_phase = 0;
  } else {
    for (j = 0, i = grp_idx; j < scen_grp_entry[grp_idx].num_generator; j++, i++){ 
      if (schedule_type == SCHEDULE_TYPE_SIMPLE)
        ph = &(group_schedule_ptr[i].phase_array[4]);
      else
        ph = &(group_schedule_ptr[i].phase_array[cur_phase_idx]);
      ph->default_phase = 0;
    }
  }

  /* Validation */
  if (num_fields < 3) {
        NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011324, "Invalid number of argument for 'Ramp Down' phase.");
       //usage_parse_schedule_phase_ramp_down("Atleast two field required after RAMP_DOWN");
  }

  if (group_mode == TC_FIX_CONCURRENT_USERS) {
    if (strcmp(fields[2], "IMMEDIATELY") && strcmp(fields[2], "STEP") && strcmp(fields[2], "TIME")) {
      NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011344, fields[2]);
      //usage_parse_schedule_phase_ramp_down("Unknown Ramp Down mode. It can be : IMMEDIATELY, STEP or TIME");
    }
  } else if (group_mode == TC_FIX_USER_RATE) {
     if (strcmp(fields[2], "IMMEDIATELY") && strcmp(fields[2], "TIME_SESSIONS")) {
       NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011324, "Ramp Down mode '%s' selected for Fixed Sessions Rate"
                        " scenarios. It can be IMMEDIATELY or TIME_SESSIONS.", fields[2]); 
       //usage_parse_schedule_phase_ramp_up("Unknown Ramp Down mode for Fixed Sessions Rate scenarios. "
                                         // "It can be : IMMEDIATELY or TIME_SESSIONS");
     }
  }

  /* RAMP_DOWN <num_users/sessions rate> <ramp_down_mode> <mode_based_args> */
  char *users = fields[1];
  char *ramp_down_mode = fields[2];
  
  /* Users can only be ALL in Simple mode */
  if ((schedule_type == SCHEDULE_TYPE_SIMPLE) &&
      strcmp(users, "ALL")) {
    NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011324, "Users can only be 'ALL' "
                      "for Schedule Type 'Simple' in 'Ramp Down' phase.");
       //usage_parse_schedule_phase_ramp_down("For Simple Scenario Types, Ramp Down users can only be ALL");
  } /* All for Advanced should work */

  /* Fill mode */
  if (strcmp(ramp_down_mode, "IMMEDIATELY") == 0) {
    if (schedule_by == SCHEDULE_BY_SCENARIO) {
      tmp_ph->phase_cmd.ramp_down_phase.ramp_down_mode = RAMP_DOWN_MODE_IMMEDIATE;
    } else {
      for (j = 0, i = grp_idx; j < scen_grp_entry[grp_idx].num_generator; j++, i++){ 
        if (schedule_type == SCHEDULE_TYPE_SIMPLE)
          ph = &(group_schedule_ptr[i].phase_array[4]);
        else
          ph = &(group_schedule_ptr[i].phase_array[cur_phase_idx]);
        ph->phase_cmd.ramp_down_phase.ramp_down_mode = RAMP_DOWN_MODE_IMMEDIATE;
      }
    } 
  } else if (strcmp(ramp_down_mode, "STEP") == 0) {
    if (num_fields < 4) {
      NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011325, 
                        "Invalid number of arguments with 'STEP' mode for 'Ramp Down' phase.");
       //usage_parse_schedule_phase_ramp_down("Atleast one argument required with STEP");
    }
    if (schedule_by == SCHEDULE_BY_SCENARIO) {
      tmp_ph->phase_cmd.ramp_down_phase.ramp_down_mode = RAMP_DOWN_MODE_STEP;
      tmp_ph->phase_cmd.ramp_down_phase.ramp_down_step_users = atoi(fields[3]);
      if (tmp_ph->phase_cmd.ramp_down_phase.ramp_down_step_users < 0) {
        NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011325, 
                          "'Ramp Down' Steps count should be greater than zero for 'STEP' mode.");
        //usage_parse_schedule_phase_ramp_down("Steps can not be less than zero");
      }
      tmp_ph->phase_cmd.ramp_down_phase.ramp_down_step_time = ni_get_time_from_format(fields[4]);
      if (tmp_ph->phase_cmd.ramp_down_phase.ramp_down_step_time < 0) {
        NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011325, 
                          "Time specified with 'STEP' mode is invalid for 'Ramp Down' phase.");
        //usage_parse_schedule_phase_ramp_down("Invalid time with STEPS");
      }
    }
    else {
      NIDL (4, "cur_phase_idx = %d", cur_phase_idx);
      for (j = 0, i = grp_idx; j < scen_grp_entry[grp_idx].num_generator; j++, i++)
      {
        //ph = &(group_schedule_ptr[i].phase_array[4]);
        ph = &(group_schedule_ptr[i].phase_array[cur_phase_idx]);
        ph->phase_cmd.ramp_down_phase.ramp_down_mode = RAMP_DOWN_MODE_STEP;
        ph->phase_cmd.ramp_down_phase.ramp_down_step_users = atoi(fields[3]);
        if (ph->phase_cmd.ramp_down_phase.ramp_down_step_users < 0) {
          NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011325, 
                            "'Ramp Down' Steps count should be greater than zero for 'STEP' mode.");
         //usage_parse_schedule_phase_ramp_down("Steps can not be less than zero");
        }
        ph->phase_cmd.ramp_down_phase.ramp_down_step_time = ni_get_time_from_format(fields[4]);
        if (ph->phase_cmd.ramp_down_phase.ramp_down_step_time < 0) {
          NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011325, 
                            "Time specified with 'STEP' mode is invalid for 'Ramp Down' phase.");
          //usage_parse_schedule_phase_ramp_down("Invalid time with STEPS");
        }
      }
    }
  } else if (strcmp(ramp_down_mode, "TIME") == 0) {
    if (num_fields < 5) {
        NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011326, 
                        "Invalid number of argument provided in 'TIME' mode for 'Ramp Down' phase");
        //usage_parse_schedule_phase_ramp_down("Atleast Two argument required after TIME");
    }

    if (schedule_by == SCHEDULE_BY_SCENARIO) {
      tmp_ph->phase_cmd.ramp_down_phase.ramp_down_mode = RAMP_DOWN_MODE_TIME;
      tmp_ph->phase_cmd.ramp_down_phase.ramp_down_time = ni_get_time_from_format(fields[3]);
      if (tmp_ph->phase_cmd.ramp_down_phase.ramp_down_time < 0) {
        NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011326, 
                          "Time specified with 'TIME' mode for 'Ramp Down' phase is invalid.");
        //usage_parse_schedule_phase_ramp_down("Invalid time specified");
      } else if (tmp_ph->phase_cmd.ramp_down_phase.ramp_down_time == 0) {
        tmp_ph->phase_cmd.ramp_down_phase.ramp_down_mode = RAMP_DOWN_MODE_IMMEDIATE;
      }

      if (strcmp(fields[4], "LINEARLY") == 0) {
        tmp_ph->phase_cmd.ramp_down_phase.ramp_down_pattern = RAMP_DOWN_PATTERN_LINEAR;
      } else if (strcmp(fields[4], "RANDOMLY") == 0) {
        tmp_ph->phase_cmd.ramp_down_phase.ramp_down_pattern = RAMP_DOWN_PATTERN_RANDOM;
      } else {                    /* Unknown pattern */
        NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011345, fields[4]);
        //usage_parse_schedule_phase_ramp_down("Unknown Pattern, in RAMP_DOWN");
      }
    } else {
      for (j = 0, i = grp_idx; j < scen_grp_entry[grp_idx].num_generator; j++, i++){ 
        if (schedule_type == SCHEDULE_TYPE_SIMPLE)
          ph = &(group_schedule_ptr[i].phase_array[4]);
        else
          ph = &(group_schedule_ptr[i].phase_array[cur_phase_idx]);
        ph->phase_cmd.ramp_down_phase.ramp_down_mode = RAMP_DOWN_MODE_TIME;
        ph->phase_cmd.ramp_down_phase.ramp_down_time = ni_get_time_from_format(fields[3]);
        if (ph->phase_cmd.ramp_down_phase.ramp_down_time < 0) {
        NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011326, 
                          "Time specified with 'TIME' mode for 'Ramp Down' phase is invalid.");
       // usage_parse_schedule_phase_ramp_down("Invalid time specified");
        } else if (ph->phase_cmd.ramp_down_phase.ramp_down_time == 0) {
          ph->phase_cmd.ramp_down_phase.ramp_down_mode = RAMP_DOWN_MODE_IMMEDIATE;
        }
        if (strcmp(fields[4], "LINEARLY") == 0) {
          ph->phase_cmd.ramp_down_phase.ramp_down_pattern = RAMP_DOWN_PATTERN_LINEAR;
        } else if (strcmp(fields[4], "RANDOMLY") == 0) {
          ph->phase_cmd.ramp_down_phase.ramp_down_pattern = RAMP_DOWN_PATTERN_RANDOM;
        } else {                    /* Unknown pattern */
          NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011345, fields[4]);
          //usage_parse_schedule_phase_ramp_down("Unknown Pattern, in RAMP_DOWN");
        } 
      }
    }
  } else if (strcmp(ramp_down_mode, "TIME_SESSIONS") == 0) {
    if (num_fields < 5) {
        NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011327, 
                          "Invalid number of argument with 'TIME_SESSIONS' mode for 'Ramp Down' phase");
       //usage_parse_schedule_phase_ramp_down("Atleast Two argument required after TIME");
    }
    if (schedule_by == SCHEDULE_BY_SCENARIO) {
    /* TIME_SESSIONS <ramp_down_time in HH:MM:SS> <mode> <step_time or num_steps> 
       <mode> 0|1|2 (default steps, steps of x seconds, total steps) */
      tmp_ph->phase_cmd.ramp_down_phase.ramp_down_mode = RAMP_DOWN_MODE_TIME_SESSIONS;
      tmp_ph->phase_cmd.ramp_down_phase.ramp_down_time = ni_get_time_from_format(fields[3]);
      if (tmp_ph->phase_cmd.ramp_down_phase.ramp_down_time < 0) {
        NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011327, 
                          "Time specified with 'TIME_SESSIONS' mode for 'Ramp Down' phase is invalid.");
        //usage_parse_schedule_phase_ramp_down("Time specified is invalid");
      } else if (tmp_ph->phase_cmd.ramp_down_phase.ramp_down_time == 0) {
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
            tmp_ph->phase_cmd.ramp_down_phase.step_mode = DEFAULT_STEP;
        }
        else if (step_mode == 1) {
          if (num_fields < 6) {
            NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011327, 
                              "Step time or num steps are not specified with 'TIME_SESSIONS' mode for 'Ramp Down' phase.");
          //usage_parse_schedule_phase_ramp_down("Step time or num steps should be specified");
          }
          tmp_ph->phase_cmd.ramp_down_phase.ramp_down_step_time = atoi(fields[5]);
          tmp_ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions = tmp_ph->phase_cmd.ramp_down_phase.ramp_down_step_time;
          if (tmp_ph->phase_cmd.ramp_down_phase.ramp_down_step_time <= 0){
              NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011327, 
                                "Ramp Down step time with 'TIME_SESSIONS' mode is not valid time.");
            //NS_EXIT(-1, "Invalid step time '%d' at line [%s]", tmp_ph->phase_cmd.ramp_down_phase.ramp_down_step_time, buf_backup);
          }          
          if ((tmp_ph->phase_cmd.ramp_down_phase.ramp_down_time/1000) < tmp_ph->phase_cmd.ramp_down_phase.ramp_down_step_time){
            NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011327, 
                              "Ramp Down time can not be less then step time for 'TIME_SESSIONS' mode.");
            //NS_EXIT(-1, "Ramp Down time can not be less then step time at line [%s]", buf_backup);
          }          
          temp = (double)(tmp_ph->phase_cmd.ramp_down_phase.ramp_down_time/1000)/(double)tmp_ph->phase_cmd.ramp_down_phase.ramp_down_step_time;
          NIDL (4, "tmp_ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions =%d", tmp_ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions);
          tmp_ph->phase_cmd.ramp_down_phase.step_mode = STEP_TIME;
        }
        else if (step_mode == 2) {
          if (num_fields < 6) {
            NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011327,
                              "Num steps with 'TIME_SESSIONS' mode is not specified for 'Ramp Down' phase.");
            //usage_parse_schedule_phase_ramp_down("Step time or num steps should be specified");
          }
          tmp_ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions = atoi(fields[5]);
          NIDL (4, "tmp_ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions =%d", tmp_ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions);
          if (tmp_ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions <= 0){
            NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011327,
                              "Num steps can not less than or equal to zero with 'TIME_SESSIONS' mode for 'Ramp Down' phase.");
            //NS_EXIT(-1, "Invalid num steps '%d' at line [%s]", tmp_ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions, buf_backup);
          }          
          temp = (double)(tmp_ph->phase_cmd.ramp_down_phase.ramp_down_time/1000)/(double)tmp_ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions;
          // step time can not be 0 
          tmp_ph->phase_cmd.ramp_down_phase.ramp_down_step_time = (round(temp))?round(temp):1; 
          tmp_ph->phase_cmd.ramp_down_phase.step_mode = NUM_STEPS;
        }
        else {
          NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011327, 
                            "Invalid step mode is selected with 'TIME_SESSIONS' mode for 'Ramp Down' phase.");
          //NS_EXIT(-1, "Invalid step mode specified in line [%s]", buf_backup);
        }
      }
    } else {
       /* TIME_SESSIONS <ramp_down_time in HH:MM:SS> <mode> <step_time or num_steps>
        * <mode> 0|1|2 (default steps, steps of x seconds, total steps) */
      for (j = 0, i = grp_idx; j < scen_grp_entry[grp_idx].num_generator; j++, i++){ 
        if (schedule_type == SCHEDULE_TYPE_SIMPLE)
          ph = &(group_schedule_ptr[i].phase_array[4]);
        else
          ph = &(group_schedule_ptr[i].phase_array[cur_phase_idx]);
        ph->phase_cmd.ramp_down_phase.ramp_down_mode = RAMP_DOWN_MODE_TIME_SESSIONS;
        ph->phase_cmd.ramp_down_phase.ramp_down_time = ni_get_time_from_format(fields[3]);
        if (ph->phase_cmd.ramp_down_phase.ramp_down_time < 0) {
          NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011327,
                           "Ramp Down step time with 'TIME_SESSIONS' mode is not valid time.");
          //usage_parse_schedule_phase_ramp_down("Time specified is invalid");
        } else if (ph->phase_cmd.ramp_down_phase.ramp_down_time == 0) {
          ph->phase_cmd.ramp_down_phase.ramp_down_mode = RAMP_DOWN_MODE_IMMEDIATE;
        }
        if (ph->phase_cmd.ramp_down_phase.ramp_down_time > 0) {
          double temp;
          int step_mode = atoi(fields[4]);
          if (step_mode == 0) {
            ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions = 2;
            int floor = ph->phase_cmd.ramp_down_phase.ramp_down_time/(1000*60); // chk it once
            if (floor > 1)
              ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions = floor;
              temp = (double)(ph->phase_cmd.ramp_down_phase.ramp_down_time/1000)/(double)ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions;
              ph->phase_cmd.ramp_down_phase.ramp_down_step_time = round(temp);
              ph->phase_cmd.ramp_down_phase.step_mode = DEFAULT_STEP;
          }
          else if (step_mode == 1) {
            if (num_fields < 6) {
              NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011327, 
                              "Step time or num steps are not specified with 'TIME_SESSIONS' mode for 'Ramp Down' phase.");
              //usage_parse_schedule_phase_ramp_down("Step time or num steps should be specified");
            }
            ph->phase_cmd.ramp_down_phase.ramp_down_step_time = atoi(fields[5]);
            ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions = ph->phase_cmd.ramp_down_phase.ramp_down_step_time;
            if (ph->phase_cmd.ramp_down_phase.ramp_down_step_time <= 0){
             NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011327,
                               "Ramp Down step time with 'TIME_SESSIONS' mode is not valid time.");
             // NS_EXIT(-1, "Invalid step time '%d' at line [%s]", ph->phase_cmd.ramp_down_phase.ramp_down_step_time, buf_backup);
            }          
            if ((ph->phase_cmd.ramp_down_phase.ramp_down_time/1000) < ph->phase_cmd.ramp_down_phase.ramp_down_step_time){
	     NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011327, 
                               "Ramp Down time can not be less then step time for 'TIME_SESSIONS' mode.");
            // NS_EXIT(-1, "Ramp Down time can not be less then step time at line [%s]", buf_backup);
            }          
            temp = (double)(ph->phase_cmd.ramp_down_phase.ramp_down_time/1000)/(double)ph->phase_cmd.ramp_down_phase.ramp_down_step_time;
            NIDL (4, "ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions =%d", ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions);
            ph->phase_cmd.ramp_down_phase.step_mode = STEP_TIME;
          }
          else if (step_mode == 2) {
            if (num_fields < 6) {
              NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011327,
                                "Number of steps with 'TIME_SESSIONS' mode is not specified for 'Ramp Down' phase.");
              //usage_parse_schedule_phase_ramp_down("Step time or num steps should be specified");
            }
            ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions = atoi(fields[5]);
            NIDL (4, "ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions =%d", ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions);
            if (ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions <= 0){
              NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011327,
                               "Number of steps can not less than or equal to zero with 'TIME_SESSIONS' mode for 'Ramp Down' phase."); 
              //NS_EXIT(-1, "Invalid num steps '%d' at line [%s]", ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions, buf_backup);
            }          
            temp = (double)(ph->phase_cmd.ramp_down_phase.ramp_down_time/1000)/(double)ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions;
            // step time can not be 0 
            ph->phase_cmd.ramp_down_phase.ramp_down_step_time = (round(temp))?round(temp):1; 
            ph->phase_cmd.ramp_down_phase.step_mode = NUM_STEPS;
          }
          else {
            NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011318, 
                              "Invalid step mode is selected with 'TIME_SESSIONS' mode for 'Ramp Down' phase.");
            //NS_EXIT(-1, "Invalid step mode specified in line [%s]", buf_backup);
          }
        }
      }
    }
  }
  /* Fill users */
  if (strcmp(users, "ALL") == 0) 
  {
    if (schedule_by == SCHEDULE_BY_SCENARIO) {
      tmp_ph->phase_cmd.ramp_down_phase.num_vusers_or_sess = -1; /* Fill -1 for ALL for now, later filled during validation */
      /* In case of ramping down all users then set flag, check this flag while recreating schedule phase 
       * keyword for generator scenario file
       * TODO: Can we reuse phase_cmd.ramp_up_phase.num_vusers_or_sess struct variable tro serve above purpose*/
      tmp_ph->phase_cmd.ramp_down_phase.ramp_down_all = 1;
    } else {
      for (j = 0, i = grp_idx; j < scen_grp_entry[grp_idx].num_generator; j++, i++){ 
        if (schedule_type == SCHEDULE_TYPE_SIMPLE)
          ph = &(group_schedule_ptr[i].phase_array[4]);
        else
          ph = &(group_schedule_ptr[i].phase_array[cur_phase_idx]);
         ph->phase_cmd.ramp_down_phase.num_vusers_or_sess = -1;
         ph->phase_cmd.ramp_down_phase.ramp_down_all = 1;
      }
    }
  } 
  else  
  { /* FSR */
    if (group_mode == TC_FIX_USER_RATE) { 
      if (schedule_by == SCHEDULE_BY_SCENARIO) {
        tmp_ph->phase_cmd.ramp_down_phase.num_vusers_or_sess = compute_sess_rate(users); 
      } else {
        for (j = 0, i = grp_idx; j < scen_grp_entry[grp_idx].num_generator; j++, i++){ 
          if (schedule_type == SCHEDULE_TYPE_SIMPLE)
            ph = &(group_schedule_ptr[i].phase_array[4]);
          else
            ph = &(group_schedule_ptr[i].phase_array[cur_phase_idx]);
          ph->phase_cmd.ramp_down_phase.num_vusers_or_sess = compute_sess_rate(users);
        }
      }
    } else {
      if (schedule_by == SCHEDULE_BY_SCENARIO) 
      {
        tmp_ph->phase_cmd.ramp_down_phase.num_vusers_or_sess = atoi(users);
        if (tmp_ph->phase_cmd.ramp_down_phase.num_vusers_or_sess < 0) {
          NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011327, 
                            "Number of users for 'Ramp Down' phase should be greater than zero in 'TIME_SESSIONS' mode.");
          //usage_parse_schedule_phase_ramp_down("Quantity can not be less than zero");
        }
      } else  {
        for (j = 0, i = grp_idx; j < scen_grp_entry[grp_idx].num_generator; j++, i++){ 
          if (schedule_type == SCHEDULE_TYPE_SIMPLE)
            ph = &(group_schedule_ptr[i].phase_array[4]);
          else
            ph = &(group_schedule_ptr[i].phase_array[cur_phase_idx]);
          ph->phase_cmd.ramp_down_phase.num_vusers_or_sess = atoi(users);
          if (ph->phase_cmd.ramp_down_phase.num_vusers_or_sess < 0) {
            NS_KW_PARSING_ERR(full_buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011327, 
                            "Number of users for 'Ramp Down' phase should be greater than zero in 'TIME_SESSIONS' mode.");
            //usage_parse_schedule_phase_ramp_down("Quantity can not be less than zero");
          }
        }
      }
    }
  }
  return 0;
}
/*Need to find duplicate phase name given in scenario*/
static int check_duplicate_phase_name(int grp_idx, char *phase_name, char *err_msg, char *buf)
{
  ni_Phases *ph;
  schedule *schedule_ptr;
  int i;
  NIDL (1, "Method called"); 
  if (schedule_by == SCHEDULE_BY_SCENARIO) {
    schedule_ptr = scenario_schedule_ptr;
  } else {
    schedule_ptr = &group_schedule_ptr[grp_idx];
  }

  ph = schedule_ptr->phase_array;

  NIDL (3, "Method Called. grp_idx = %d, phase_name = %s, "
                 "num_phases = %d\n", grp_idx, phase_name, schedule_ptr->num_phases);
  for (i = 0; i < schedule_ptr->num_phases; i++) {
    if (!(ph[i].default_phase) && !strcmp(phase_name, ph[i].phase_name)) {
      NS_KW_PARSING_ERR(buf, 0, err_msg, SCHEDULE_USAGE, CAV_ERR_1011308, "Phase name '%s' used more than once for same group", phase_name);
      //NS_EXIT(-1, "Duplicate phase names not allowed. Phase name '%s' used more than once for same group",
        //             phase_name);
    }
  }
  return 0;
}

/*Function used to find scenario group*/
static int find_sg_index(char* scengrp_name)
{
  int i;
  NIDL (2, "Method called, scengrp_name = %s", scengrp_name);

  /* Group name ALL */
  if (strcasecmp(scengrp_name, "ALL") == 0) {
    NIDL (2, "SGRP keyword received with group name ALL");
    return NS_GRP_IS_ALL;
  }
  /* Search group name*/
  for (i = 0; i < total_sgrp_entries; i++) {
    if (!strcmp(scen_grp_entry[i].scen_group_name, scengrp_name))
      return i; //SGRP name found
  }
  return NS_GRP_IS_INVALID; //Invalid group name
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
int kw_schedule(char *buf, int set_rtc_flag, char *err_msg)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char grp[MAX_DATA_LINE_LENGTH];
  char phase_name[MAX_DATA_LINE_LENGTH];
  char phase_type[MAX_DATA_LINE_LENGTH];
  int grp_idx;
  int num;
  

  NIDL (4, "Method called, buf = %s", buf);
  
  num = sscanf(buf, "%s %s %s %s", keyword, grp, phase_name, phase_type);
  if (num < 4) {
    NS_KW_PARSING_ERR(buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011308, CAV_ERR_MSG_1);
    //usage_kw_set_schedule("Arguments are missing for keyword: SCHEDULE");
  }

  NIDL (2, "Total number of arguments =  %d", num);

  NIDL(2, "Total number of scenario group = %d", total_sgrp_entries);
  if(total_sgrp_entries == 0)
  {
    NS_KW_PARSING_ERR(buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011309, "Please add atleast one Scenario Group to run test"); 
   // NS_EXIT(-1, "Please add atleast one scenario group to run test");
  } 

  grp_idx = find_sg_index(grp);
  
  if (grp_idx == NS_GRP_IS_INVALID) {
    NIDL (2, "Invalid SGRP group name");
    NS_KW_PARSING_ERR(buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011308, "Invalid Scenario Group name in Global Schedule setting"); 
  }

  if (schedule_by == SCHEDULE_BY_SCENARIO) {
    if (grp_idx != -1)   /* i.e  not ALL */ {
     NS_KW_PARSING_ERR(buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011308, "Group field can only be 'ALL' for Schedule By 'Scenario'.");
     // usage_kw_set_schedule("In SCHEDULE_BY SCENARIO mode, group field can only be 'ALL'");
    }
    if(strcmp(grp,"ALL"))
    {
      NS_KW_PARSING_ERR(buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011308, "Group field can only be 'ALL' for Schedule By 'Scenario'.");
    // NS_EXIT(-1, "In SCHEDULE_BY SCENARIO mode, group field can only be 'ALL', please update and re-run the test");
    }
  } else if (schedule_by == SCHEDULE_BY_GROUP) {
    if (grp_idx == -1)   /* i.e ALL */ {
       NS_KW_PARSING_ERR(buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011308, "Group field can not be 'ALL' for Schedule By 'Group'");
      //usage_kw_set_schedule("In SCHEDULE_BY GROUP mode, group field can not be 'ALL'");
    }
  }

  if (!set_rtc_flag)
    check_duplicate_phase_name(grp_idx, phase_name, err_msg, buf);

  if (set_rtc_flag)
    rtc_group_idx = grp_idx;

  if (strcmp(phase_type, "START") == 0) {
    NIDL (4, "parsing START");
    ni_parse_schedule_phase_start(grp_idx, buf, phase_name, set_rtc_flag, err_msg);
  } else if (strcmp(phase_type, "RAMP_UP") == 0) {
    NIDL (4, "parsing RAMP_UP");
    if(set_rtc_flag){
      //TODO: AYUSH Need to handle for RTC
      ni_parse_runtime_schedule_phase_ramp_up(grp_idx, buf, phase_name, err_msg);
    }
    else 
      ni_parse_schedule_phase_ramp_up(grp_idx, buf, phase_name, err_msg);
  } else if (strcmp(phase_type, "DURATION") == 0) {
    NIDL (4, "parsing DURATION");
    ni_parse_schedule_phase_duration(grp_idx, buf, phase_name, set_rtc_flag, err_msg);
  } else if (strcmp(phase_type, "STABILIZATION") == 0) {
    NIDL (4, "parsing STABILIZATION");
    ni_parse_schedule_phase_stabilize(grp_idx, buf, phase_name, set_rtc_flag, err_msg);
  } else if (strcmp(phase_type, "RAMP_DOWN") == 0) {
    NIDL (4, "parsing RAMP_DOWN");
    if(set_rtc_flag)
      ni_parse_runtime_schedule_phase_ramp_down(grp_idx, buf, phase_name, err_msg);
    else 
      ni_parse_schedule_phase_ramp_down(grp_idx, buf, phase_name, err_msg);
  } else {
    NS_KW_PARSING_ERR(buf, set_rtc_flag, err_msg, SCHEDULE_USAGE, CAV_ERR_1011336, phase_type);
   // usage_kw_set_schedule("Invalid Phase given in scenario file");
  }
  return 0;
}
