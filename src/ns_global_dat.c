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

#include "nslb_time_stamp.h"
#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "user_tables.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "util.h"
#include "timing.h"
#include "tmr.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "ns_schedule_ramp_up_fcu.h"
#include "ns_schedule_ramp_down_fcu.h"
#include "ns_schedule_phases_parse.h"
#include "ns_log.h"
#include "ns_alloc.h"
#include "ns_global_dat.h"
#include "wait_forever.h"//Added for loader_opcode
#include "ns_exit.h"
#include "ns_global_settings.h"

char target_completion_time[32];

//extern void end_test_run();
extern targetCompletion *estimate_completion_schedule_phases(targetCompletion *tc,
                                                             Schedule *schedule);
void
log_global_dat (cavgtime *cavg, double *pg_data)
{
  FILE *fp;
  char buf[600];
  unsigned long long num_completed, num_samples;
  //unsigned long long num_succ;
  int rampup, warmup, runtime, rampdown;

  NSDL2_REPORTING(NULL, NULL, "Method called");
  sprintf(buf, "logs/TR%d/global.dat", testidx);
  if ((fp = fopen(buf, "a+" )) == NULL) {
    fprintf(stderr, "log_gloabl_dat: Error in opening global.dat %s\n", buf);
    return;
  }

  fprintf (fp, "PG_RESP_TIME_THRESHOLDS %1.3f %1.3f\n",
               ((double )global_settings->warning_threshold)/1000, 
               ((double )global_settings->alert_threshold)/1000);

  num_completed = cavg->pg_fetches_completed;
  //num_succ = cavg->pg_succ_fetches;
  num_samples = num_completed;
  //num_samples = global_settings->exclude_failed_agg?num_succ:num_completed;
  //num_initiated = avg->cum_pg_fetches_started;
  if (num_completed) {
    if (num_samples) {
      fprintf(fp, "OVER_ALL_PG_RESP_TIMES %1.3f %1.3f %1.3f %1.3f\n",
  	  pg_data[0], pg_data[2], pg_data[4],
         (double)(((double)(cavg->pg_c_tot_time))/((double)(1000.0*(double)num_samples))));
    } else {
      fprintf(fp, "OVER_ALL_PG_RESP_TIMES - - - -\n");
    }
  } else {
    fprintf(fp, "OVER_ALL_PG_RESP_TIMES - - - -\n");
  }

  fprintf(fp, "DESIRED_VUSERS %d\n", global_settings->num_connections);

  // All PHASE_TIMES are cumulative
  if( global_settings->schedule_by == SCHEDULE_BY_SCENARIO &&
      global_settings->schedule_type == SCHEDULE_TYPE_SIMPLE &&
     !global_settings->replay_mode) {
    rampup = global_settings->test_rampup_done_time - global_settings->test_start_time;
    if (rampup < 0) rampup = 0;

    warmup = global_settings->test_runphase_start_time - global_settings->test_start_time;
    if (warmup < rampup) warmup = rampup;

    runtime = warmup + global_settings->test_runphase_duration;
    if (runtime < warmup) runtime = warmup;

    rampdown = global_settings->test_duration;
    if (rampdown < runtime) rampdown = runtime;

    fprintf (fp, "PHASE_TIMES %1.3f %1.3f %1.3f %1.3f\n", 
                 ((double )rampup)/1000, ((double )warmup)/1000,
                 ((double )runtime)/1000, ((double )rampdown)/1000);
  }

  fclose(fp);
}

targetCompletion *estimate_completion_schedule_phase_start(targetCompletion *tc,
                                                           Schedule *schedule, 
                                                           int phase_idx)
{
  Phases *ph;
  Start_schedule_phase *start_phase;
  targetCompletion tc_new;

  NSDL4_SCHEDULE(NULL, NULL, "Method called");
  ph = &(schedule->phase_array[phase_idx]);
  start_phase = &(ph->phase_cmd.start_phase);
  
  tc->type = TC_TIME;
  NSDL4_SCHEDULE(NULL, NULL, "dependent_grp = %d",
                 start_phase->dependent_grp);

  if (start_phase->dependent_grp != -1) {
    tc_new.value = 0;
    tc_new.type = -1;
    estimate_completion_schedule_phases(&tc_new, 
                                        &(group_schedule[start_phase->dependent_grp]));
    tc->type = tc_new.type;
    tc->value += tc_new.value;
    NSDL4_SCHEDULE(NULL, NULL, "From dependent grop time = %u (%u)", 
           tc_new.value, tc->value);
  }

  /* Calculation for start phase completion time */
  tc->value += start_phase->time;
  NSDL4_SCHEDULE(NULL, NULL, "From our time = %lu (%u)", 
         start_phase->time, tc->value);

  return tc;
}

targetCompletion * estimate_completion_schedule_phase_ramp_up(targetCompletion *tc,
                                                              Schedule *schedule, 
                                                              int phase_idx)
{
  Phases *ph;
  Ramp_up_schedule_phase *ramp_up_phase;

  ph = &(schedule->phase_array[phase_idx]);
  ramp_up_phase = &(ph->phase_cmd.ramp_up_phase);

  NSDL4_SCHEDULE(NULL, NULL, "Method called");
 /* Calculation  for ramp up phase completion time */
  switch (ramp_up_phase->ramp_up_mode) {
  case RAMP_UP_MODE_IMMEDIATE:
    tc->type = TC_TIME;
    tc->value += 0;
    break;
  case RAMP_UP_MODE_STEP:
    tc->type = TC_TIME;
    tc->value += 
      (ramp_up_phase->ramp_up_step_time * ramp_up_phase->num_vusers_or_sess) / ramp_up_phase->ramp_up_step_users;
    
    break;
  case RAMP_UP_MODE_RATE:        /* Linear/Random ?? */
    tc->type = TC_TIME;
    tc->value +=                /* * 60 Since rate is in Min */
      (((60 * ramp_up_phase->num_vusers_or_sess) / ramp_up_phase->ramp_up_rate) * 1000);
    NSDL4_SCHEDULE(NULL, NULL, 
                   "tc->value = %u, num_vusers_or_sess = %u, ramp_up_rate = %f\n",
                   tc->value, ramp_up_phase->num_vusers_or_sess, ramp_up_phase->ramp_up_rate);
    break;
  case RAMP_UP_MODE_TIME:       /* Linear/Random ?? */
    tc->type = TC_TIME;
    tc->value += ramp_up_phase->ramp_up_time;
    break;
  case RAMP_UP_MODE_TIME_SESSIONS:
    tc->type = TC_TIME;
    tc->value += ramp_up_phase->ramp_up_time;
    break;
  default:
    break;
  }

  NSDL4_SCHEDULE(NULL, NULL, "tc->value = %u\n", tc->value);
  return tc;
}

targetCompletion * estimate_completion_schedule_phase_ramp_down(targetCompletion *tc,
                                                                Schedule *schedule, 
                                                                int phase_idx)
{
  Phases *ph;
  Ramp_down_schedule_phase *ramp_down_phase;

  ph = &(schedule->phase_array[phase_idx]);
  ramp_down_phase = &(ph->phase_cmd.ramp_down_phase);

  NSDL4_SCHEDULE(NULL, NULL, "Method called");

  switch (ramp_down_phase->ramp_down_mode) {
  case RAMP_DOWN_MODE_IMMEDIATE:
    tc->type = TC_TIME;
    tc->value += 0;
    break;
  case RAMP_DOWN_MODE_STEP:
    tc->type = TC_TIME;
    tc->value += 
      (ramp_down_phase->ramp_down_step_time * ramp_down_phase->num_vusers_or_sess) / ramp_down_phase->ramp_down_step_users;
    break;
  case RAMP_DOWN_MODE_TIME:
    tc->type = TC_TIME;
    tc->value += ramp_down_phase->ramp_down_time;
    break;
  case RAMP_DOWN_MODE_TIME_SESSIONS:
    tc->type = TC_TIME;
    tc->value += ramp_down_phase->ramp_down_time;
    break;
  default:
    break;
  }

  return tc;
}

targetCompletion * estimate_completion_schedule_phase_stabilize(targetCompletion *tc,
                                                                Schedule *schedule, 
                                                                int phase_idx)
{
  Phases *ph;
  Stabilize_schedule_phase *stabilize_phase;
  
  ph = &(schedule->phase_array[phase_idx]);
  stabilize_phase = &(ph->phase_cmd.stabilize_phase);
  
  NSDL4_SCHEDULE(NULL, NULL, "Method called");
  
  tc->type = TC_TIME;
  tc->value += stabilize_phase->time;
  NSDL4_SCHEDULE(NULL, NULL, "tc->value = %u, stabilize_phase->time = %u\n", 
         tc->value, stabilize_phase->time);

  return tc;
}

targetCompletion *estimate_completion_schedule_phase_duration(targetCompletion *tc,
                                                              Schedule *schedule, 
                                                              int phase_idx)
{
  Phases *ph;
  Duration_schedule_phase  *duration_phase;

  ph = &(schedule->phase_array[phase_idx]);
  duration_phase = &(ph->phase_cmd.duration_phase);

  NSDL4_SCHEDULE(NULL, NULL, "Method called");

  if (duration_phase->duration_mode == DURATION_MODE_INDEFINITE) {
    tc->type = TC_INDEFINITE;
  } else if (duration_phase->duration_mode == DURATION_MODE_SESSION) {
    tc->type = TC_SESSION;
    if(duration_phase->per_user_fetches_flag)
    {
      if(global_settings->schedule_by == SCHEDULE_BY_SCENARIO)
      {
        NSDL4_SCHEDULE(NULL, NULL, "TC: num_fetches = %d, num_connection = %d", duration_phase->num_fetches,
                                    global_settings->num_connections);
        tc->value = duration_phase->num_fetches * global_settings->num_connections; //session * total_users
      }
      else
      {
        NSDL4_SCHEDULE(NULL, NULL, "TC: group = %d, quantity = %d, num_fetches = %d", schedule->group_idx,
                                    runprof_table_shr_mem[schedule->group_idx].quantity, duration_phase->num_fetches);
        tc->value = duration_phase->num_fetches * runprof_table_shr_mem[schedule->group_idx].quantity; //session * total_users
      }
    }
    else
      tc->value = duration_phase->num_fetches;
  } else {                      /* TIME */
    tc->type = TC_TIME;
    tc->value += duration_phase->seconds;
  }
  NSDL4_SCHEDULE(NULL, NULL, "Method exit, tc value = %d, tc type = %d", tc->value, tc->type);

  return tc;
}

targetCompletion *estimate_completion_schedule_phases(targetCompletion *tc,
                                                      Schedule *schedule)
{
  Phases *ph;
  int phase_idx;
  
/*   tc->type = -1; */
/*   tc->value = 0; */

  NSDL4_SCHEDULE(NULL, NULL, "Method called");
  for (phase_idx = 0; phase_idx < schedule->num_phases; phase_idx++) {

    if (tc->type == TC_INDEFINITE || tc->type == TC_SESSION) {
      return tc;
    }

    ph = &(schedule->phase_array[phase_idx]);
    switch (ph->phase_type) {
    case SCHEDULE_PHASE_START:
      estimate_completion_schedule_phase_start(tc, schedule, phase_idx);
      break;
    case SCHEDULE_PHASE_RAMP_UP:
      estimate_completion_schedule_phase_ramp_up(tc, schedule, phase_idx);
      break;
    case SCHEDULE_PHASE_RAMP_DOWN:
      estimate_completion_schedule_phase_ramp_down(tc, schedule, phase_idx);
      break;
    case SCHEDULE_PHASE_STABILIZE:
      estimate_completion_schedule_phase_stabilize(tc, schedule, phase_idx);
      break;
    case SCHEDULE_PHASE_DURATION:
      estimate_completion_schedule_phase_duration(tc, schedule, phase_idx);
      break;
    default:                  /* can not be */
      NS_EXIT(-1, "Unknown phase. Exiting ...");
      break;
    }
  }
  return tc;
}

/**
 * Write in global.dat
 * Format:
 *      TARGET_COMPLETION <mode> <value>
 *
 *      <mode>:
 *              INDEFINITE
 *              SESSION <num>
 *              TIME <HH:MM:SS>
 *              COMPLETION
 *
 * mode COMPLETION is only applicable to RAL mode.
 */
void estimate_target_completion()
{
  char buf[MAX_FILE_NAME];
  FILE *fp;
  targetCompletion *tc = NULL, *tc_max;
  char time[0xff];
  Schedule *schedule;
  int grp_idx;
  unsigned long long tot_session = 0;
  int duration_time = 0;
  int cal_session_rate = 0;
 


  NSDL4_SCHEDULE(NULL, NULL, "Method called");

/*   if (global_settings->schedule_by != SCHEDULE_BY_SCENARIO) */
/*     return ; */

  if (global_settings->replay_mode == 0) {
    if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO) {
      schedule = scenario_schedule;
      MY_MALLOC(tc, sizeof (targetCompletion), "tc", -1);
      grp_idx = -1;
      tc->type = -1;
      tc->value = 0;
      estimate_completion_schedule_phases(tc, schedule);
      NSDL4_SCHEDULE(NULL, NULL, "schedule by scenario tc value = %d, tc type = %d", tc->value, tc->type);
      if(tc->type == TC_SESSION)
        tot_session = tc->value;
      tc_max = tc;
    } else if (global_settings->schedule_by == SCHEDULE_BY_GROUP) {
      MY_MALLOC(tc, sizeof (targetCompletion) * total_runprof_entries, "tc", -1);
      for (grp_idx = 0; grp_idx < total_runprof_entries; grp_idx++) {
        /* NetCloud: In case of generator, we can have scenario configuration with SGRP sessions/users 0.
         * Here we need to by-pass RAMP_UP check, because on generator schedule phases for such group does not exists*/
        tc[grp_idx].type = -1; //moved here as quantity 0 for generator junk value result is undesirable
        tc[grp_idx].value = 0;
        if ((loader_opcode != CLIENT_LOADER) || ((loader_opcode == CLIENT_LOADER) && (runprof_table_shr_mem[grp_idx].quantity != 0)))
        {
          schedule = &(group_schedule[grp_idx]);
          schedule->group_idx = grp_idx;  //setting group index as it is not set
          //tc[grp_idx].type = -1;
          //tc[grp_idx].value = 0;
          estimate_completion_schedule_phases(&(tc[grp_idx]), schedule);
          schedule->group_idx = -1;  //resetting grp idx as it was set before
      
          if (tc[grp_idx].type == TC_INDEFINITE ) {
            break;
          }
        }
      }
      /* find tc_max */
      tc_max = &(tc[0]);
    
      for (grp_idx = 0; grp_idx < total_runprof_entries; grp_idx++) {
        if ((loader_opcode != CLIENT_LOADER) || 
             ((loader_opcode == CLIENT_LOADER) && (runprof_table_shr_mem[grp_idx].quantity != 0)))
        {
          if (tc[grp_idx].type == TC_INDEFINITE ) {
            tc_max = &(tc[grp_idx]);
            break;  
          } 
          else if(tc[grp_idx].type == TC_SESSION) {
            tc_max = &(tc[grp_idx]);
            tot_session += tc[grp_idx].value;
          }          
          else if(tc[grp_idx].type == TC_TIME && !tot_session)
          {
           //Find maximum duration available in all groups 
             if (tc_max->value < tc[grp_idx].value)
               tc_max = &(tc[grp_idx]);
              
    NSDL4_SCHEDULE(NULL, NULL, "schedule by group value= %d, tc type = %d", tc_max->value, tc->type);
          }
        }
      }
    }
  }

  sprintf(buf, "logs/TR%d/global.dat", testidx);
  if ((fp = fopen(buf, "a+" )) == NULL) {
    fprintf(stderr, "log_global_dat: Error in opening global.dat %s\n", buf);
    return;
  }
  

  if (global_settings->replay_mode != 0) {
    fprintf(fp, "TARGET_COMPLETION COMPLETION\n");
    sprintf(target_completion_time, "Till Completion");
  } else if (tc_max->type == -1) {            /* Something is wrong */
    fprintf(fp, "TARGET_COMPLETION TIME 00:00:00\n");
    sprintf(target_completion_time, "00:00:00");
  } else if (tc_max->type == TC_INDEFINITE) {
    fprintf(fp, "TARGET_COMPLETION INDEFINITE\n");
    sprintf(target_completion_time, "Indefinite");
  } else if (tc_max->type == TC_SESSION) {
    fprintf(fp, "TARGET_COMPLETION SESSIONS %llu\n", tot_session);
    sprintf(target_completion_time, "%llu sessions", tot_session);
  } else if (tc_max->type == TC_TIME) {
    convert_to_hh_mm_ss(tc_max->value, time);
    duration_time = round((double)tc_max->value)/1000.0;
    sprintf(target_completion_time, "%s", time);
    fprintf(fp, "TARGET_COMPLETION TIME %s\n", time);
  }

  //Calculate session rate because value of vusers_rpm is multiply by 1000
  if(global_settings->vuser_rpm)
    cal_session_rate = global_settings->vuser_rpm/1000;

  NSTL1(NULL, NULL, "flags = %d, num_connections = %d, vuser_rpm = %d, time = %d, tot_session = %llu",
                               global_settings->flags, global_settings->num_connections, cal_session_rate,
                               duration_time, tot_session);

  //Check the value of debug test setting and enable debug flag
  if(global_settings->num_connections <= global_settings->debug_setting.debug_test_value_vuser &&
     cal_session_rate <= global_settings->debug_setting.debug_test_value_session_rate &&
     duration_time <= global_settings->debug_setting.debug_test_value_duration &&
     tot_session <= global_settings->debug_setting.debug_test_value_session)  
  {    
    NSTL1(NULL, NULL, "flags = %d, num_connections = %d, vuser_rpm = %d, time = %d, tot_session = %llu", 
                               global_settings->flags, global_settings->num_connections, cal_session_rate,
                               duration_time, tot_session);
    global_settings->flags |= GS_FLAGS_TEST_IS_DEBUG_TEST;   
  }

  FREE_AND_MAKE_NOT_NULL(tc, "tc", -1);
  fclose(fp);
}

inline void update_test_runphase_duration()
{
  NSDL4_SCHEDULE(NULL, NULL, "Method called, test_runphase_start_time = %u, test_runphase_duration = %u",
                      global_settings->test_runphase_start_time, global_settings->test_runphase_duration);

  if (global_settings->test_runphase_start_time && !global_settings->test_runphase_duration)
        global_settings->test_runphase_duration = get_ms_stamp() - global_settings->test_runphase_start_time;
}

void phase_type_to_str(int phase_type, char *phase_type_name, char *phase_name)
{
  if (phase_type == 1) {
    strcpy(phase_type_name, "START");
  } else if (phase_type == 2) {
    strcpy(phase_type_name, "RAMP_UP");
  } else if (phase_type == 3) {
    strcpy(phase_type_name, "RAMP_DOWN");
  } else if (phase_type == 4) {
    strcpy(phase_type_name, "STABILIZATION");
  } else if (phase_type == 5) {
    strcpy(phase_type_name, "DURATION");
  } else if (phase_type == 6) {
    strcpy(phase_type_name, "RUNTIME_CHANGE_DONE");
  } else {
    fprintf(stderr, "Invalid Phase %s given in scenario file. Exiting..\n", phase_name);
  }
}


void log_phase_time(int start_end, int phase_type, char *phase_name, char *time)
{
  char file[MAX_FILE_NAME];
  FILE *fp;
  char phase_type_name[32]; // START, RAMP_UP etc

  OPEN_GLOBAL_DATA_FILE(file, fp);

  NSDL2_MESSAGES(NULL, NULL, "phase_name = %s", phase_name);
  phase_type_to_str(phase_type, phase_type_name, phase_name);
  if(start_end == PHASE_IS_COMPLETED){
    fprintf(fp, "PHASE_END_TIME %s %s %s\n", phase_type_name, phase_name, time);
  } else {
    fprintf(fp, "PHASE_START_TIME %s %s %s\n", phase_type_name, phase_name, time);
  }

  CLOSE_GLOBAL_DATA_FILE(fp);
}

/*Funct added to provide GUI_SERVER information in global.dat file
 *Here information added if nsu_start_test cmd given with -s option
 */

void log_global_dat_gui_server()
{
  char file[MAX_FILE_NAME];
  FILE *fp;

  OPEN_GLOBAL_DATA_FILE(file, fp);
  
  NSDL1_SCHEDULE(NULL, NULL, "Method called");
  
  if(global_settings->gui_server_addr[0])
    fprintf(fp, "GUI_SERVER %s:%hd\n", global_settings->gui_server_addr, global_settings->gui_server_port);
  else
    fprintf(fp, "GUI_SERVER NONE\n");
    
  CLOSE_GLOBAL_DATA_FILE(fp);
}
