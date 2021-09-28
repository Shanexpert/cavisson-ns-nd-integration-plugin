/********************************************************************************
 * File Name            : ni_runtime_changes.c 
 * Author(s)            : Manpreet Kaur
 * Date                 : 14 Feb 2013
 * Copyright            : (c) Cavisson Systems
 * Purpose              : Contains runtime change functionality 
 * Modification History : <Author(s)>, <Date>, <Change Description/Location>
 ********************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <malloc.h>
#include <string.h>
#include <stdarg.h> 
#include <errno.h>
#include <sys/stat.h> 
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "nslb_sock.h"
#include "../../../ns_exit.h"
#include "../../../ns_master_agent.h"
#include "ni_user_distribution.h"
#include "ni_scenario_distribution.h"
#include "ni_schedule_phases_parse.h"
#include "../../../ns_error_msg.h"
#include "../../../ns_kw_usage.h"
#include "../../../ns_global_settings.h"

/* This function is called for Advanced scenario,
 * it returns the index of phase which matches with the phase name
 * if return value is -1 must be checked
 * */
static int get_phase_index_by_phase_name(int grp_idx, char *phase_name) {

  schedule *ptr;
  int i;

  NIDL (1, "Method Called, grp_idx = %d, phase_name = %s", grp_idx, phase_name);

  if (schedule_by == SCHEDULE_BY_SCENARIO) {
    ptr = scenario_schedule_ptr;
  } else {
    ptr = &group_schedule_ptr[grp_idx];
  }

  for(i = 0; i< ptr->num_phases; i++) {
    if(strcmp(ptr->phase_array[i].phase_name, phase_name) == 0)
       return i;
  }

  return -1;
}

int ni_parse_runtime_schedule_phase_ramp_up(int grp_idx, char *full_buf, char *phase_name, char *err_msg) 
{
  int phase_index;
  ni_Phases *tmp_ph, *ph;
  char *fields[20];
  char buf_backup[MAX_DATA_LINE_LENGTH];
  char *buf, *buf_more;
  int num_fields;
  int i, j;
  schedule *cur_schedule;

  NIDL (4, "Method called grp_idx = %d, full_buf = %s, phase_name = %s", grp_idx, full_buf, phase_name);  
  
  if(schedule_type == SCHEDULE_TYPE_SIMPLE) 
    phase_index = 1; 
  else {
    phase_index = get_phase_index_by_phase_name(grp_idx, phase_name); 
    if(phase_index == -1) {
      NS_KW_PARSING_ERR(phase_name, 1, err_msg, SCHEDULE_USAGE, CAV_ERR_1011336, phase_name);
      //fprintf(stderr, "Phase '%s' does not exists.\n", phase_name);
      //return -1;
    }
  }
  //Fill phase index in case of rtc
  rtc_phase_idx = phase_index;

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
  
  num_fields = get_tokens_(buf, fields, " ", 20);

  //char *users_or_sess_rate = fields[1];
  char *ramp_up_mode_str = fields[2];
  
  if (schedule_by == SCHEDULE_BY_SCENARIO) {
    cur_schedule = scenario_schedule_ptr;
    tmp_ph = &(cur_schedule->phase_array[phase_index]);
  } else {
    cur_schedule = &(group_schedule_ptr[grp_idx]);
    tmp_ph = &(cur_schedule->phase_array[phase_index]);
  }

  /* Fill mode */
  if (strcmp(ramp_up_mode_str, "RATE") == 0) 
  {
    char tmp_buf[1024];
    if (schedule_by == SCHEDULE_BY_SCENARIO) {
      tmp_ph->phase_cmd.ramp_up_phase.ramp_up_mode = RAMP_UP_MODE_RATE;
    } else {
      for (j = 0, i = grp_idx; j < scen_grp_entry[grp_idx].num_generator; j++, i++){ 
        if (schedule_type == SCHEDULE_TYPE_SIMPLE)
          ph = &(group_schedule_ptr[i].phase_array[1]);
        else
          ph = &(group_schedule_ptr[i].phase_array[phase_index]);
        ph->phase_cmd.ramp_up_phase.ramp_up_mode = RAMP_UP_MODE_RATE;
      }
    }
    if (num_fields < 5) {
      NS_KW_PARSING_ERR(full_buf, 1, err_msg, SCHEDULE_USAGE, CAV_ERR_1011316, 
                        "Invalid number of argument with 'RATE' mode for 'Ramp Up' phase.");
      //fprintf(stderr, "Atleast two arguments required with RATE, in line [%s]\n", buf_backup);
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
          ph = &(group_schedule_ptr[i].phase_array[phase_index]);
        ph->phase_cmd.ramp_up_phase.ramp_up_rate = convert_to_per_min(tmp_buf); 
      }
    }
    /* pattern */
    if(num_fields < 6)
    {
      NS_KW_PARSING_ERR(full_buf, 1, err_msg, SCHEDULE_USAGE, CAV_ERR_1011316, 
                        "Ramp Up pattern is not specified for 'RATE' mode. It should be LINEARLY or RANDOMLY.");
       //fprintf(stderr, "<ramp_up_pattern>  not specified. Should be LINEARLY or RANDOMLY, in line [%s]\n", buf_backup);
    }
    else if (strcmp(fields[5], "LINEARLY") == 0) {
      if (schedule_by == SCHEDULE_BY_SCENARIO) {
        tmp_ph->phase_cmd.ramp_up_phase.ramp_up_pattern = RAMP_UP_PATTERN_LINEAR;
      } else {
        for (j = 0, i = grp_idx; j < scen_grp_entry[grp_idx].num_generator; j++, i++){ 
          if (schedule_type == SCHEDULE_TYPE_SIMPLE)
            ph = &(group_schedule_ptr[i].phase_array[1]);
          else
            ph = &(group_schedule_ptr[i].phase_array[phase_index]);
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
            ph = &(group_schedule_ptr[i].phase_array[phase_index]);
          ph->phase_cmd.ramp_up_phase.ramp_up_pattern = RAMP_UP_PATTERN_RANDOM;
        } 
      }
    } else {                    /* Unknown pattern */
      NS_KW_PARSING_ERR(full_buf, 1, err_msg, SCHEDULE_USAGE, CAV_ERR_1011337, fields[5]);
      //fprintf(stderr, "Unknown Pattern %s, in line [%s]\n", fields[5], buf_backup);
    }
  } else if (strcmp(ramp_up_mode_str, "TIME_SESSIONS") == 0) {
    
    if (num_fields < 5) {
      NS_KW_PARSING_ERR(full_buf, 1, err_msg, SCHEDULE_USAGE, CAV_ERR_1011318, "Invalid number of argument with 'TIME_SESSIONS' mode for 'Ramp Up' phase");
      //fprintf(stderr, "Atleast two arguments required with TIME_SESSIONS, in line [%s]\n", buf_backup);
    }
    if (schedule_by == SCHEDULE_BY_SCENARIO) 
    {
    /* TIME_SESSIONS <ramp_up_time in HH:MM:SS> <mode> <step_time or num_steps> 
       <mode> 0|1|2 (default steps, steps of x seconds, total steps) */
      tmp_ph->phase_cmd.ramp_up_phase.ramp_up_mode = RAMP_UP_MODE_TIME_SESSIONS;
      tmp_ph->phase_cmd.ramp_up_phase.ramp_up_time = ni_get_time_from_format(fields[3]);
      if (tmp_ph->phase_cmd.ramp_up_phase.ramp_up_time < 0) {
        NS_KW_PARSING_ERR(full_buf, 1, err_msg, SCHEDULE_USAGE, CAV_ERR_1011318, 
                          "Time specified with 'TIME_SESSIONS' mode for 'Ramp Up' phase is invalid.");
        //fprintf(stderr, "Time specified is invalid in line [%s]\n", buf_backup);
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
            NS_KW_PARSING_ERR(full_buf, 1, err_msg, SCHEDULE_USAGE, CAV_ERR_1011318, 
                              "Step time or num steps are not specified with 'TIME_SESSIONS' mode for 'Ramp Up' phase.");
            //fprintf(stderr, "Step time or num steps should be specified, in line [%s]\n", buf_backup);
          }
          tmp_ph->phase_cmd.ramp_up_phase.ramp_up_step_time = atoi(fields[5]);
          tmp_ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions = tmp_ph->phase_cmd.ramp_up_phase.ramp_up_step_time;
          if (tmp_ph->phase_cmd.ramp_up_phase.ramp_up_step_time <= 0){
            NS_KW_PARSING_ERR(full_buf, 1, err_msg, SCHEDULE_USAGE, CAV_ERR_1011318, 
                              "Ramp Up step time with 'TIME_SESSIONS' mode is not valid time.");
            //NS_EXIT(-1, "Invalid step time '%d' at line [%s]", tmp_ph->phase_cmd.ramp_up_phase.ramp_up_step_time, buf_backup);
          }          
          if ((tmp_ph->phase_cmd.ramp_up_phase.ramp_up_time/1000) < tmp_ph->phase_cmd.ramp_up_phase.ramp_up_step_time){
            NS_KW_PARSING_ERR(full_buf, 1, err_msg, SCHEDULE_USAGE, CAV_ERR_1011318, 
                              "Ramp Up time can not be less then step time for 'TIME_SESSIONS' mode.");
            //NS_EXIT(-1, "Ramp Up time can not be less then step time at line [%s]", buf_backup);
          }          
          temp = (double)(tmp_ph->phase_cmd.ramp_up_phase.ramp_up_time/1000)/(double)tmp_ph->phase_cmd.ramp_up_phase.ramp_up_step_time;
          NIDL (4, "tmp_ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions =%d", tmp_ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions);
          tmp_ph->phase_cmd.ramp_up_phase.step_mode = STEP_TIME;
        }
        else if (step_mode == 2) {
          if (num_fields < 6) {
            NS_KW_PARSING_ERR(full_buf, 1, err_msg, SCHEDULE_USAGE, CAV_ERR_1011318,
                              "Num steps with 'TIME_SESSIONS' mode is not specified for 'Ramp Up' phase.");
            //fprintf(stderr, "Step time or num steps should be specified, in line [%s]\n", buf_backup);
          }
          tmp_ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions = atoi(fields[5]);
          NIDL (4, "tmp_ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions =%d", tmp_ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions);
          if (tmp_ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions <= 0){
            NS_KW_PARSING_ERR(full_buf, 1, err_msg, SCHEDULE_USAGE, CAV_ERR_1011318,
                              "Num steps can not less than or equal to zero with 'TIME_SESSIONS' mode for 'Ramp Up' phase.");
            //NS_EXIT(-1, "Invalid num steps '%d' at line [%s]", tmp_ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions, buf_backup);
          }          
          temp = (double)(tmp_ph->phase_cmd.ramp_up_phase.ramp_up_time/1000)/(double)tmp_ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions;
          // step time can not be 0 
          tmp_ph->phase_cmd.ramp_up_phase.ramp_up_step_time = (round(temp))?round(temp):1;
          tmp_ph->phase_cmd.ramp_up_phase.step_mode = NUM_STEPS;
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
          ph = &(group_schedule_ptr[i].phase_array[phase_index]);
        ph->phase_cmd.ramp_up_phase.ramp_up_mode = RAMP_UP_MODE_TIME_SESSIONS;
        ph->phase_cmd.ramp_up_phase.ramp_up_time = ni_get_time_from_format(fields[3]);
        if (ph->phase_cmd.ramp_up_phase.ramp_up_time < 0) {
          NS_KW_PARSING_ERR(full_buf, 1, err_msg, SCHEDULE_USAGE, CAV_ERR_1011318,
                           "Ramp Up step time with 'TIME_SESSIONS' mode is not valid time.");
          //fprintf(stderr, "Time specified is invalid in line [%s]\n", buf_backup);
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
              NS_KW_PARSING_ERR(full_buf, 1, err_msg, SCHEDULE_USAGE, CAV_ERR_1011318, 
                                "Step time or num steps are not specified with 'TIME_SESSIONS' mode for 'Ramp Up' phase.");
              //fprintf(stderr, "Step time or num steps should be specified, in line [%s]\n", buf_backup);
             }
             ph->phase_cmd.ramp_up_phase.ramp_up_step_time = atoi(fields[5]);
             ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions = ph->phase_cmd.ramp_up_phase.ramp_up_step_time;
             if (ph->phase_cmd.ramp_up_phase.ramp_up_step_time <= 0){
               NS_KW_PARSING_ERR(full_buf, 1, err_msg, SCHEDULE_USAGE, CAV_ERR_1011318, 
                                 "Ramp Up time can not be less then step time for 'TIME_SESSIONS' mode.");
               //NS_EXIT(-1, "Invalid step time '%d' at line [%s]", ph->phase_cmd.ramp_up_phase.ramp_up_step_time, buf_backup);
             }          
             if ((ph->phase_cmd.ramp_up_phase.ramp_up_time/1000) < ph->phase_cmd.ramp_up_phase.ramp_up_step_time){
               NS_KW_PARSING_ERR(full_buf, 1, err_msg, SCHEDULE_USAGE, CAV_ERR_1011318, 
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
              NS_KW_PARSING_ERR(full_buf, 1, err_msg, SCHEDULE_USAGE, CAV_ERR_1011318,
                                "Num steps with 'TIME_SESSIONS' mode is not specified for 'Ramp Up' phase.");
              //fprintf(stderr, "Step time or num steps should be specified, in line [%s]\n", buf_backup);
            }
            ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions = atoi(fields[5]);
            NIDL (4, "ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions =%d", ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions);
            if (ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions <= 0){
              NS_KW_PARSING_ERR(full_buf, 1, err_msg, SCHEDULE_USAGE, CAV_ERR_1011318,
                                "Num steps can not less than or equal to zero with 'TIME_SESSIONS' mode for 'Ramp Up' phase.");
              //NS_EXIT(-1, "Invalid num steps '%d' at line [%s]", ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions, buf_backup);
            }          
            temp = (double)(ph->phase_cmd.ramp_up_phase.ramp_up_time/1000)/(double)ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions;
            // step time can not be 0 
            ph->phase_cmd.ramp_up_phase.ramp_up_step_time = (round(temp))?round(temp):1;
            ph->phase_cmd.ramp_up_phase.step_mode = NUM_STEPS;
          } else {
            NS_KW_PARSING_ERR(full_buf, 1, err_msg, SCHEDULE_USAGE, CAV_ERR_1011318, 
                              "Invalid step mode is selected with 'TIME_SESSIONS' mode for 'Ramp Down' phase.");
            //NS_EXIT(-1, "Invalid step mode specified in line [%s]", buf_backup);
          } 
        }
      }//for loop
    }//else 
  }    
  return 0;
}

int ni_parse_runtime_schedule_phase_ramp_down(int grp_idx, char *full_buf, char *phase_name, char *err_msg) 
{
  int phase_index;			
  ni_Phases *tmp_ph, *ph;
  char *fields[20];
  char buf_backup[MAX_DATA_LINE_LENGTH];
  char *buf;
  char *buf_more;
  int num_fields;
  int i, j;
  schedule *cur_schedule;

  NIDL (1, "Method Called, grp_idx = %d, full_buf = %s, phase_name = %s", grp_idx, full_buf, phase_name);

  if(schedule_type == SCHEDULE_TYPE_SIMPLE) 
    phase_index = 4; 
  else {
    phase_index = get_phase_index_by_phase_name(grp_idx, phase_name); 
    if(phase_index == -1) {
      NS_KW_PARSING_ERR(phase_name, 1, err_msg, SCHEDULE_USAGE, CAV_ERR_1011336, phase_name);
      //fprintf(stderr, "Phase '%s' does not exists in '%s'", phase_name, full_buf);
      //return -1;
    }
  }
  //Fill phase index in case of rtc
  rtc_phase_idx = phase_index;

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

  num_fields = get_tokens_(buf, fields, " ", 20);  

  /* RAMP_DOWN <num_users/sessions rate> <ramp_down_mode> <mode_based_args> */
  //char *users = fields[1];
  char *ramp_down_mode = fields[2];

  if (schedule_by == SCHEDULE_BY_SCENARIO) {
    cur_schedule = scenario_schedule_ptr;
  } else {
    cur_schedule = &(group_schedule_ptr[grp_idx]);
  }
  tmp_ph = &(cur_schedule->phase_array[phase_index]);
  
  if (strcmp(ramp_down_mode, "TIME_SESSIONS") == 0) 
  {
    if (schedule_by == SCHEDULE_BY_SCENARIO) {
    /* TIME_SESSIONS <ramp_down_time in HH:MM:SS> <mode> <step_time or num_steps> 
       <mode> 0|1|2 (default steps, steps of x seconds, total steps) */
      tmp_ph->phase_cmd.ramp_down_phase.ramp_down_mode = RAMP_DOWN_MODE_TIME_SESSIONS;
      tmp_ph->phase_cmd.ramp_down_phase.ramp_down_time = ni_get_time_from_format(fields[3]);
      if (tmp_ph->phase_cmd.ramp_down_phase.ramp_down_time < 0) {
        NS_KW_PARSING_ERR(full_buf, 1, err_msg, SCHEDULE_USAGE, CAV_ERR_1011327, 
                          "Time specified with 'TIME_SESSIONS' mode for 'Ramp Down' phase is invalid.");
        //fprintf(stderr, "Time specified is invalid in line [%s]\n", buf_backup);
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
            NS_KW_PARSING_ERR(full_buf, 1, err_msg, SCHEDULE_USAGE, CAV_ERR_1011327, 
                              "Step time or num steps are not specified with 'TIME_SESSIONS' mode for 'Ramp Down' phase.");
            //fprintf(stderr, "Step time or num steps should be specified, in line [%s]\n", buf_backup);
          }
          tmp_ph->phase_cmd.ramp_down_phase.ramp_down_step_time = atoi(fields[5]);
          tmp_ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions = tmp_ph->phase_cmd.ramp_down_phase.ramp_down_step_time;
          if (tmp_ph->phase_cmd.ramp_down_phase.ramp_down_step_time <= 0){
            NS_KW_PARSING_ERR(full_buf, 1, err_msg, SCHEDULE_USAGE, CAV_ERR_1011327, 
                                "Ramp Down step time with 'TIME_SESSIONS' mode is not valid time.");
            //NS_EXIT(-1, "Invalid step time '%d' at line [%s]", tmp_ph->phase_cmd.ramp_down_phase.ramp_down_step_time, buf_backup);
          }          
          if ((tmp_ph->phase_cmd.ramp_down_phase.ramp_down_time/1000) < tmp_ph->phase_cmd.ramp_down_phase.ramp_down_step_time){
            NS_KW_PARSING_ERR(full_buf, 1, err_msg, SCHEDULE_USAGE, CAV_ERR_1011327, 
                              "Ramp Down time can not be less then step time for 'TIME_SESSIONS' mode.");
            //NS_EXIT(-1, "Ramp Down time can not be less then step time at line [%s]", buf_backup);
          }          
          temp = (double)(tmp_ph->phase_cmd.ramp_down_phase.ramp_down_time/1000)/(double)tmp_ph->phase_cmd.ramp_down_phase.ramp_down_step_time;
          NIDL (4, "tmp_ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions =%d", tmp_ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions);
          tmp_ph->phase_cmd.ramp_down_phase.step_mode = STEP_TIME;
        }
        else if (step_mode == 2) {
          if (num_fields < 6) {
            NS_KW_PARSING_ERR(full_buf, 1, err_msg, SCHEDULE_USAGE, CAV_ERR_1011327,
                              "Num steps with 'TIME_SESSIONS' mode is not specified for 'Ramp Down' phase.");
            //fprintf(stderr, "Step time or num steps should be specified, in line [%s]\n", buf_backup);
          }
          tmp_ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions = atoi(fields[5]);
          NIDL (4, "tmp_ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions =%d", tmp_ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions);
          if (tmp_ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions <= 0){
            NS_KW_PARSING_ERR(full_buf, 1, err_msg, SCHEDULE_USAGE, CAV_ERR_1011327,
                              "Num steps can not less than or equal to zero with 'TIME_SESSIONS' mode for 'Ramp Down' phase.");
            //NS_EXIT(-1, "Invalid num steps '%d' at line [%s]", tmp_ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions, buf_backup);
          }          
          temp = (double)(tmp_ph->phase_cmd.ramp_down_phase.ramp_down_time/1000)/(double)tmp_ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions;
          // step time can not be 0 
          tmp_ph->phase_cmd.ramp_down_phase.ramp_down_step_time = (round(temp))?round(temp):1; 
          tmp_ph->phase_cmd.ramp_down_phase.step_mode = NUM_STEPS;
        }
        else {
          NS_KW_PARSING_ERR(full_buf, 1, err_msg, SCHEDULE_USAGE, CAV_ERR_1011327, 
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
          ph = &(group_schedule_ptr[i].phase_array[phase_index]);
        ph->phase_cmd.ramp_down_phase.ramp_down_mode = RAMP_DOWN_MODE_TIME_SESSIONS;
        ph->phase_cmd.ramp_down_phase.ramp_down_time = ni_get_time_from_format(fields[3]);
        if (ph->phase_cmd.ramp_down_phase.ramp_down_time < 0) {
          NS_KW_PARSING_ERR(full_buf, 1, err_msg, SCHEDULE_USAGE, CAV_ERR_1011327,
                           "Ramp Down step time with 'TIME_SESSIONS' mode is not valid time.");
          //fprintf(stderr, "Time specified is invalid in line [%s]\n", buf_backup);
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
              NS_KW_PARSING_ERR(full_buf, 1, err_msg, SCHEDULE_USAGE, CAV_ERR_1011327, 
                                "Step time or num steps are not specified with 'TIME_SESSIONS' mode for 'Ramp Down' phase.");
              //fprintf(stderr, "Step time or num steps should be specified, in line [%s]\n", buf_backup);
            }
            ph->phase_cmd.ramp_down_phase.ramp_down_step_time = atoi(fields[5]);
            ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions = ph->phase_cmd.ramp_down_phase.ramp_down_step_time;
            if (ph->phase_cmd.ramp_down_phase.ramp_down_step_time <= 0){
              NS_KW_PARSING_ERR(full_buf, 1, err_msg, SCHEDULE_USAGE, CAV_ERR_1011327,
                               "Ramp Down step time with 'TIME_SESSIONS' mode is not valid time.");
              //NS_EXIT(-1, "Invalid step time '%d' at line [%s]", ph->phase_cmd.ramp_down_phase.ramp_down_step_time, buf_backup);
            }          
            if ((ph->phase_cmd.ramp_down_phase.ramp_down_time/1000) < ph->phase_cmd.ramp_down_phase.ramp_down_step_time){
	     NS_KW_PARSING_ERR(full_buf, 1, err_msg, SCHEDULE_USAGE, CAV_ERR_1011327, 
                               "Ramp Down time can not be less then step time for 'TIME_SESSIONS' mode.");
              //NS_EXIT(-1, "Ramp Down time can not be less then step time at line [%s]", buf_backup);
            }          
            temp = (double)(ph->phase_cmd.ramp_down_phase.ramp_down_time/1000)/(double)ph->phase_cmd.ramp_down_phase.ramp_down_step_time;
            NIDL (4, "ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions =%d", ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions);
            ph->phase_cmd.ramp_down_phase.step_mode = STEP_TIME;
          }
          else if (step_mode == 2) {
            if (num_fields < 6) {
              NS_KW_PARSING_ERR(full_buf, 1, err_msg, SCHEDULE_USAGE, CAV_ERR_1011327,
                                "Number of steps with 'TIME_SESSIONS' mode is not specified for 'Ramp Down' phase.");
              //fprintf(stderr, "Step time or num steps should be specified, in line [%s]\n", buf_backup);
            }
            ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions = atoi(fields[5]);
            NIDL (4, "ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions =%d", ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions);
            if (ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions <= 0){
              NS_KW_PARSING_ERR(full_buf, 1, err_msg, SCHEDULE_USAGE, CAV_ERR_1011327,
                               "Number of steps can not less than or equal to zero with 'TIME_SESSIONS' mode for 'Ramp Down' phase."); 
              //NS_EXIT(-1, "Invalid num steps '%d' at line [%s]", ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions, buf_backup);
            }          
            temp = (double)(ph->phase_cmd.ramp_down_phase.ramp_down_time/1000)/(double)ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions;
            // step time can not be 0 
            ph->phase_cmd.ramp_down_phase.ramp_down_step_time = (round(temp))?round(temp):1; 
            ph->phase_cmd.ramp_down_phase.step_mode = NUM_STEPS;
          }
          else {
            NS_KW_PARSING_ERR(full_buf, 1, err_msg, SCHEDULE_USAGE, CAV_ERR_1011318, 
                              "Invalid step mode is selected with 'TIME_SESSIONS' mode for 'Ramp Down' phase.");
            //NS_EXIT(-1, "Invalid step mode specified in line [%s]", buf_backup);
          }
        }//if ramp_down_time >0
      }//for loop	
    }//Schedule by group
  }
  return 0;
}
