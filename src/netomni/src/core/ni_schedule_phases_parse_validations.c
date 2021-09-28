/********************************************************************************
 * File Name            : ni_schedule_phase_parse_validations.c
 * Author(s)            : Manpreet Kaur
 * Date                 : 31 Aug 2012
 * Copyright            : (c) Cavisson Systems
 * Purpose              : Contains function to validate scheduling
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
#include <math.h>
#include <libgen.h>

#include "ni_user_distribution.h"
#include "ni_schedule_phases_parse.h"
#include "ni_scenario_distribution.h"
#include "../../../ns_exit.h"
#include "../../../ns_error_msg.h"

static double *start_ramp_up_for_gen;
gen_name_sch_setting *gen_sch_setting = NULL;
/* Int array used to save number of session corresponding to SGRP index*/
static int *save_sess_per_grp;
extern char *g_alert_info;

void update_schedule_structure_per_gen()
{
  NIDL (1, "Method called");
  int gen_id;

  for (gen_id = 0; gen_id < sgrp_used_genrator_entries; gen_id++) 
  {
    if (schedule_by == SCHEDULE_BY_SCENARIO) 
    {
      if (scenario_schedule_ptr != NULL) 
      {
        //generator_entry[gen_id].scenario_schedule_ptr = (schedule*)malloc(sizeof(schedule));
        NSLB_MALLOC(generator_entry[gen_id].scenario_schedule_ptr, (sizeof(schedule)), "generator entry scenario", gen_id, NULL);
        NIDL(2, "Malloc'ed generator_entry[gen_id].scenario_schedule_ptr = %p", generator_entry[gen_id].scenario_schedule_ptr);
        memcpy(generator_entry[gen_id].scenario_schedule_ptr, scenario_schedule_ptr, sizeof(schedule));
        NIDL(2, "Total number of phases = %d", scenario_schedule_ptr->num_phases);
        //generator_entry[gen_id].scenario_schedule_ptr->phase_array = (ni_Phases*)malloc(sizeof(ni_Phases) * scenario_schedule_ptr->num_phases); 
        NSLB_MALLOC(generator_entry[gen_id].scenario_schedule_ptr->phase_array, (sizeof(ni_Phases) * scenario_schedule_ptr->num_phases), "generator entry phase array", gen_id, NULL);
        NIDL(2, "Malloc'ed generator_entry[gen_id].scenario_schedule_ptr->phase_array = %p", generator_entry[gen_id].scenario_schedule_ptr->phase_array);
        memcpy(generator_entry[gen_id].scenario_schedule_ptr->phase_array, scenario_schedule_ptr->phase_array, sizeof(ni_Phases) * scenario_schedule_ptr->num_phases);
      }
    } 
  }
}

/**************************************************************************************
 * Description		: Function add_schedule_phase(), adds new phase 
 *                        in the end. If ph is NULL,new phase is allocated and returned. 
 * Input Parameter	: 
 * schedule_ptr		: schedule pointer which need to be update
 * phase_type		: Type of phase(START, RAMP-UP, DURATION etc)
 * phase_name		: Phase name given by user
 * cur_phase_idx	: Used in advance scenario to keep track of phase-id
 * Return		: Return value is the address of where the memory is attached.
 **************************************************************************************/
ni_Phases *add_schedule_ph(schedule *schedule_ptr, int phase_type, char *phase_name, int *cur_phase_idx)
{
  ni_Phases *ph;
  schedule_ptr->num_phases++;

  NIDL (2, "Method called, Adding %s phase", phase_name);

  schedule_ptr->phase_array = (ni_Phases*)realloc(schedule_ptr->phase_array, sizeof(ni_Phases) * schedule_ptr->num_phases);
  NIDL (2, "Reallocate phase_array = %p", schedule_ptr->phase_array);

  ph = &(schedule_ptr->phase_array[schedule_ptr->num_phases - 1]);
  memset(ph, 0, sizeof(ni_Phases));

  strncpy(ph->phase_name, phase_name, sizeof(ph->phase_name) - 1);
  ph->phase_type = phase_type;
  ph->phase_status = PHASE_NOT_STARTED;
  ph->default_phase = 0;        /* Must be set to one if desired after calling this function */
  *cur_phase_idx = schedule_ptr->num_phases - 1; //Used in advance scenario to keep track of phase-id
  NIDL (2, "Current Phase index, cur_phase_idx = %d", *cur_phase_idx);
  return ph;
}

/*************************************************************************
 * Description		: This function:
 * 			  1) Adds start phase anyway at location 0. 
 * 			     This happens for all schedule types
 * 			  2) Adds other phases in case of simple schedule.
 * Input Parameter	:
 * schedule_ptr		: schedule pointer which need to be update
 * grp_idx		: Group that need to be update
 * Return		: None
 *************************************************************************/
static void add_default_phase(schedule *schedule_ptr, int grp_idx)
{
  ni_Phases *ph;
  int grp_mode = get_grp_mode(grp_idx);
  int cur_phase_idx;
  NIDL (1, "Method called, grp_idx = %d", grp_idx);

  /* Start */
  ph = add_schedule_ph(schedule_ptr, SCHEDULE_PHASE_START, "Start", &cur_phase_idx);
  ph->default_phase = 1;
  ph->phase_cmd.start_phase.dependent_grp = -1;
  ph->phase_cmd.start_phase.time = 0;       /* now, Immediately */

  NIDL (2, "Adding START phase");
  ph->phase_cmd.ramp_down_phase.ramp_down_all = 0; 
  if (schedule_type == SCHEDULE_TYPE_SIMPLE) {
    /* Ramp UP */
    ramp_up_all = 1;//Added for default scheduling phases
    ph = add_schedule_ph(schedule_ptr, SCHEDULE_PHASE_RAMP_UP, "RampUP", &cur_phase_idx);
    ph->default_phase = 1;
    ph->phase_cmd.ramp_up_phase.num_vusers_or_sess = -1;

    if (grp_mode == TC_FIX_CONCURRENT_USERS) {
      ph->phase_cmd.ramp_up_phase.ramp_up_mode = RAMP_UP_MODE_RATE;
      ph->phase_cmd.ramp_up_phase.ramp_up_rate = convert_to_per_min("RAMP_UP 120 M"); /* Sending dummy keyword */
      NIDL (2, "ph->phase_cmd.ramp_up_phase.ramp_up_rate = %f", ph->phase_cmd.ramp_up_phase.ramp_up_rate);
      ph->phase_cmd.ramp_up_phase.ramp_up_rate = ph->phase_cmd.ramp_up_phase.ramp_up_rate * 1000;//Added for default scheduling phases
    } else {
      ph->phase_cmd.ramp_up_phase.ramp_up_mode = RAMP_UP_MODE_IMMEDIATE;
    }

    /* Stabilize */
    ph = add_schedule_ph(schedule_ptr, SCHEDULE_PHASE_STABILIZE, "Stabilization", &cur_phase_idx);
    ph->default_phase = 1;
    ph->phase_cmd.stabilize_phase.time = 0;

    /* If duration is not given we assume indefinite, we add it later when doing validations */
    /* Duration */
    ph = add_schedule_ph(schedule_ptr, SCHEDULE_PHASE_DURATION, "Duration", &cur_phase_idx);
    ph->default_phase = 1;
    ph->phase_cmd.duration_phase.duration_mode = 0; /* Indefinite */

    /* Ramp Down */
    ph = add_schedule_ph(schedule_ptr, SCHEDULE_PHASE_RAMP_DOWN, "RampDown", &cur_phase_idx);
    ph->default_phase = 1;
    ph->phase_cmd.ramp_down_phase.ramp_down_mode = RAMP_DOWN_MODE_IMMEDIATE;
    ph->phase_cmd.ramp_down_phase.num_vusers_or_sess = -1;
    ph->phase_cmd.ramp_down_phase.ramp_down_all = 1;/* Need to set for default phase, else it will core-dump while re-framing schedule phases*/
  }
  NIDL (2, "Exiting method");
}

/*Function used to intialize schedule struct, and add schedule phase*/
void initialize_schedule_struct(schedule *schedule_ptr, int grp_idx) 
{
  NIDL (2, "Method called, grp_idx =%d", grp_idx);

  memset(schedule_ptr, 0, sizeof(schedule));

  schedule_ptr->group_idx = -1;
  schedule_ptr->phase_idx = -1;
  schedule_ptr->num_phases = 0;
  schedule_ptr->phase_array = NULL;
  schedule_ptr->total_ramp = 0;
  schedule_ptr->cum_total_ramp = 0;
  schedule_ptr->total_ramp_done = 0;
  schedule_ptr->cum_total_ramp_done = 0;
  schedule_ptr->prev_total_ramp_done = 0;
  schedule_ptr->prev_cum_total_ramp_done = 0;
  
  add_default_phase(schedule_ptr, grp_idx);
}

/* Advance Scenario
 * Function used to validate user given in ramp-up/down phases*/
static int ni_calculate_high_water_mark(schedule *schedule_ptr, int num_phases)
{
  ni_Phases *phase_array = schedule_ptr->phase_array;
  ni_Phases *ph_tmp;
  int act_user = 0, hwm = 0;
  int i;

  NIDL(3, "Method called, num_phases = %d", num_phases);

  for (i = 0; i < num_phases; i++) {
    ph_tmp = &(phase_array[i]);

    if (ph_tmp->phase_type == SCHEDULE_PHASE_RAMP_UP) {
      act_user += ph_tmp->phase_cmd.ramp_up_phase.num_vusers_or_sess;
      NIDL(3, "RampUp: act_user = %d, index = %d", act_user, i);
    } else if (ph_tmp->phase_type == SCHEDULE_PHASE_RAMP_DOWN) {
      /* This method can be called before filling qty all. So check for -1 */
      if (ph_tmp->phase_cmd.ramp_up_phase.num_vusers_or_sess == -1)
        act_user = 0;
      else
        act_user -= ph_tmp->phase_cmd.ramp_up_phase.num_vusers_or_sess;
      NIDL(3, "RampDown: act_user = %d, index = %d", act_user, i);
    }

    if (act_user < 0) {
      NS_EXIT(-1, CAV_ERR_1011355, ph_tmp->phase_name);
    }

    if (hwm < act_user) {
      hwm = act_user;
    }
  }

  NIDL(3, "Hwm = %u, act_user = %d", hwm, act_user);
  return hwm;
}

/*Validate schedule settings*/
static void ni_high_water_mark_check(schedule *schedule_ptr, int grp_mode, int grp_idx)
{
  int total_users_or_sess = 0;
  int i, j;
  int hwm = 0; /* High water mark */

  NIDL(3, "Method called grp_mode = %d, grp_idx = %d", grp_mode, grp_idx);

  if (schedule_by == SCHEDULE_BY_SCENARIO) {
    total_users_or_sess = (grp_mode == TC_FIX_CONCURRENT_USERS) ? num_connections : vuser_rpm;
    NIDL(3, "schedule by scenario: total_users_or_sess = %d", total_users_or_sess);
  } else {
    for (j = 0, i = grp_idx; j < scen_grp_entry[grp_idx].num_generator; j++, i++) {
      total_users_or_sess += (grp_mode == TC_FIX_CONCURRENT_USERS) ? 
                          scen_grp_entry[i].quantity : scen_grp_entry[i].percentage;
      NIDL(3, "Group schedule settings: total_users_or_sess = %d, group num = %d, j = %d", total_users_or_sess, grp_idx + j, j);
    }
  }

  hwm = ni_calculate_high_water_mark(schedule_ptr, schedule_ptr->num_phases);
  NIDL(3, "High water mark, hwm = %d", hwm);
  if (hwm < total_users_or_sess) {
    NS_EXIT(-1, CAV_ERR_1011308, "Inconsistent scenario, all users not utilized");
  } else if (hwm > total_users_or_sess) {
    NS_EXIT(-1, CAV_ERR_1011308, "Inconsistent scenario, number of users/sessions defined in phases are more than total.");
  }

  NIDL(3, "High water mark check passed, hwm = %u, total_users_or_sess = %u",
                 hwm, total_users_or_sess);
}

int calculate_total_user_or_session()
{
  int total_qty,i;
  int group_mode  = get_grp_mode(-1);
  NIDL(3, "Method called.");

  /* for both FSR and FCU */
  if(schedule_by == SCHEDULE_BY_SCENARIO)  
     total_qty = ni_calculate_high_water_mark(scenario_schedule_ptr, scenario_schedule_ptr->num_phases);
  else
  {
     for(i=0; i < total_sgrp_entries; i++)
        total_qty += ni_calculate_high_water_mark(&group_schedule_ptr[i], group_schedule_ptr[i].num_phases); 
  }
 
  /* We also set num_connections, like it was set by NUM_USERS */
  if (group_mode == TC_FIX_CONCURRENT_USERS)
    num_connections = total_qty;
  else
    vuser_rpm = (double)total_qty;
  NIDL(3, "total_qty = %d", total_qty);
  return (total_qty);
}

/* If schedule settings with "ALL" user/sessions then we need to fill 
 * validate_num_vusers_or_sess bec we need to preserve num_vusers_or_sess var
 * for schedule reframing*/
static void fill_qty_all(schedule *schedule_ptr, int group_mode, int grp_idx)
{
  ni_Phases *phase_array = schedule_ptr->phase_array;
  ni_Phases *ph_tmp;
  int act_qty = 0;                /* Acvive users */
  int i, j;

  NIDL(3, "Method called grp_idx = %d, num_connections = %d, group_mode = %d",
                 schedule_ptr->group_idx, num_connections, group_mode);

  for (i = 0; i < schedule_ptr->num_phases; i++) {
    ph_tmp = &(phase_array[i]);
    /* RAMP_UP ALL is only for simple scenario */
    if ((ph_tmp->phase_type == SCHEDULE_PHASE_RAMP_UP) &&
        ((ph_tmp->phase_cmd.ramp_up_phase.num_vusers_or_sess == -1) /*||
         (run_mode == FIND_NUM_USERS || run_mode == STABILIZE_RUN)*/)) {
      if (group_mode == TC_FIX_USER_RATE) { /* FSR */
        if (schedule_by == SCHEDULE_BY_GROUP)
          ph_tmp->phase_cmd.ramp_up_phase.validate_num_vusers_or_sess = scen_grp_entry[grp_idx].percentage;
        else {
          ph_tmp->phase_cmd.ramp_up_phase.validate_num_vusers_or_sess = vuser_rpm; /* From TARGET_RATE */
        }
        NIDL(3, "FSR: Number of user or sessions = %d", ph_tmp->phase_cmd.ramp_up_phase.validate_num_vusers_or_sess);
      } else if (group_mode == TC_FIX_CONCURRENT_USERS) { /* FCU */
        if (schedule_by == SCHEDULE_BY_GROUP)
          ph_tmp->phase_cmd.ramp_up_phase.validate_num_vusers_or_sess = scen_grp_entry[grp_idx].quantity;
        else
          ph_tmp->phase_cmd.ramp_up_phase.validate_num_vusers_or_sess = num_connections; /* From NUM_USERS */
        NIDL(3, "FCU: Number of user or sessions = %d", ph_tmp->phase_cmd.ramp_up_phase.validate_num_vusers_or_sess);
      } /*else {                  //goal based 
        ph_tmp->phase_cmd.ramp_up_phase.num_vusers_or_sess = *run_variable;
      }*/

      NIDL(3, "Phase id %d (%s), setting num_vusers_or_sess = %u", i, ph_tmp->phase_name,
                     ph_tmp->phase_cmd.ramp_up_phase.validate_num_vusers_or_sess);

    } else if ((ph_tmp->phase_type == SCHEDULE_PHASE_RAMP_DOWN) &&
               ((ph_tmp->phase_cmd.ramp_down_phase.num_vusers_or_sess == -1) /*||
                (run_mode == FIND_NUM_USERS || run_mode == STABILIZE_RUN)*/)) {
      /* In case of SCHEDULE_PHASE_RAMP_DOWN, ALL means ramp down all remaining active qty */
      act_qty = 0;
      for (j = 0; j < i; j++) {
        ni_Phases *ph_new = &(phase_array[j]);
        if (ph_new->phase_type == SCHEDULE_PHASE_RAMP_UP) {
          if (schedule_type == SCHEDULE_TYPE_ADVANCED)
            act_qty += ph_new->phase_cmd.ramp_up_phase.num_vusers_or_sess;
           else
             act_qty += ph_new->phase_cmd.ramp_up_phase.validate_num_vusers_or_sess;
          NIDL(3, "Ramp Up: act_qty = %d", act_qty);
        } else if (ph_new->phase_type == SCHEDULE_PHASE_RAMP_DOWN) {
          if (schedule_type == SCHEDULE_TYPE_ADVANCED)
            act_qty -= ph_new->phase_cmd.ramp_up_phase.num_vusers_or_sess;
          else    
            act_qty -= ph_new->phase_cmd.ramp_up_phase.validate_num_vusers_or_sess;
          NIDL(3, "Ramp Down: act_qty = %d", act_qty);
        }
      }
      if (act_qty </* = */ 0) {     /* something is wrong in construction of phases */
        NS_EXIT(-1, CAV_ERR_1011353, i, ph_tmp->phase_name, act_qty);
      }

      //ph_tmp->phase_cmd.ramp_down_phase.validate_num_vusers_or_sess = act_qty;
      ph_tmp->phase_cmd.ramp_down_phase.num_vusers_or_sess = act_qty;

      NIDL(3, "phase id %d (%s), setting num_vusers_or_sess = %u", i, ph_tmp->phase_name,
                     ph_tmp->phase_cmd.ramp_down_phase.num_vusers_or_sess);
    }
  }
}

/*fill dependent_grp_array and returns total number of dependent groups */
static int get_dependent_group(int grp_idx, int *dependent_grp_array)
{
  int num = 0;
  schedule *schedule_ptr;
  int i;

  NIDL(1, "method called");

  if (schedule_by == SCHEDULE_BY_SCENARIO)
    return -1;

  /* schedule by grp */
  for (i = 0; i < total_sgrp_entries; i++) {
    schedule_ptr = &(group_schedule_ptr[i]);
    if (schedule_ptr->phase_array[0].phase_cmd.start_phase.dependent_grp == grp_idx) {
      dependent_grp_array[num] = i;
      NIDL(2, "found dependent grp %d", i);
      num++;
    }
  }
  if (!num)
    return -1;
  else
    return num;
}

/*this function detects cyclic dependency among groups
 * start g1 after g2
 * start g2 after g1*/
static void cyclic_dependency_check(int grp_idx, int now_idx)
{
  int num;
  int i;
  int *dependent_grp_array = NULL;
  NIDL(1, "method called");
  
  //dependent_grp_array = (int *)malloc(sizeof(int) * total_sgrp_entries);
  NSLB_MALLOC(dependent_grp_array, (sizeof(int) * total_sgrp_entries), "dependent grp array", grp_idx, NULL);

  num = get_dependent_group(now_idx, dependent_grp_array);

  /* in case of schedule-by scenario we need to free dependent_grp_array,*/
  if (num == -1) {               /* terminating condition */
    free(dependent_grp_array);
    dependent_grp_array = NULL;
    return;
  }

  for (i = 0; i < num; i++) {
    if (grp_idx == dependent_grp_array[i]) {
      NS_EXIT(-1, CAV_ERR_1011308, "Cyclic group dependency found.");
    }
    cyclic_dependency_check(grp_idx, dependent_grp_array[i]);
  }
  /*dependent_grp_array need to be free and make pointer NULL*/
  free(dependent_grp_array);
  dependent_grp_array = NULL;
}

/***************************************************************************
 * description		: this function used validate schedule phase
 *                        checks any know anomalies, if found exits.
 *                        anomalies: (only left overs till now. all others 
 *                        should have already been handled)
 *                        if no phase defined after start phase, we give error.
 *                        high water mark should not exceed number of users 
 *                        defined for group/scenario
 * input parameter	: none
 * output parameter	: none
 * return		: none
 * **************************************************************************/
void ni_validate_phases()
{
  int i = 0;
  int group_mode;
  NIDL(1, "Method called");

  /* Check group cyclic dependency */
  if (schedule_by == SCHEDULE_BY_GROUP) {
    for (i = 0; i < total_sgrp_entries; i++) {
      cyclic_dependency_check(i, i);
    }
  }

  /* fill -1 with ALL */
  if (schedule_by == SCHEDULE_BY_SCENARIO) {
    group_mode  = get_grp_mode(-1);
    if (scenario_schedule_ptr->num_phases == 1) {
      NS_EXIT(-1, CAV_ERR_1011308, "Schedule phases are not provided.");
    }
    fill_qty_all(scenario_schedule_ptr, group_mode, -1);
  } else if (schedule_by == SCHEDULE_BY_GROUP) {
    for (i = 0; i < total_sgrp_entries; i++) {
      group_mode  = get_grp_mode(i);
      if (group_schedule_ptr[i].num_phases == 1) {
        NS_EXIT(-1, CAV_ERR_1011354, i);
      }
      fill_qty_all(&(group_schedule_ptr[i]), group_mode, i);
    }
  }
  /* Now check for High Water Mark Anomalies; should only be done for Advanced scenarios */
  if (schedule_type == SCHEDULE_TYPE_ADVANCED) {
    if (schedule_by == SCHEDULE_BY_SCENARIO) {
      group_mode = get_grp_mode(-1);
      NIDL(4, "Checking high water mark for Scenario schedule");
      ni_high_water_mark_check(scenario_schedule_ptr, group_mode, -1);
    } else {                      /* group based */
      i = 0;
      do 
      { 
        group_mode  = get_grp_mode(i);
        NIDL (2, "Distribute schedule settings for group_idx = %d, group_mode = %d", i, group_mode);
        ni_high_water_mark_check(&(group_schedule_ptr[i]), group_mode, i);
        if (total_sgrp_entries > 1) {
          i = i + scen_grp_entry[i].num_generator;
          NIDL (2, "Next group_idx = %d", i);
        } else
          break; 
      } while (i < total_sgrp_entries);
    }
  }
}

/*Function used to write scehdule phase statements in scenario file*/
static void add_schedule_ph_scen(FILE *fp, ni_Phases *tmp_ph, int grp_idx, int gen_idx, int phase_idx, int tool_call_at_init_rtc, int for_quantity_rtc) 
{  
  char time_format[255];
  NIDL(1, "Method called, for_quantity_rtc = %d", for_quantity_rtc);

  switch (tmp_ph->phase_type) 
  { 
    /* Begin: START PHASE */
    /* Case 1: IMMEDIATELY
     * Syntax: SCHEDULE <group-name> <phase-name> START IMMEDIATELY*/
    case SCHEDULE_PHASE_START: 
      if (tmp_ph->phase_cmd.start_phase.start_mode == START_MODE_IMMEDIATE) 
      {
        NIDL (2, "Start phase with option IMMEDIATELY");
        fprintf(fp, "SCHEDULE %s %s START IMMEDIATELY\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name);
      }
      /* Case 2: AFTER certain TIME
       * Syntax: SCHEDULE <group-name> <phase-name> START AFTER TIME <time>*/
      else if (tmp_ph->phase_cmd.start_phase.start_mode == START_MODE_AFTER_TIME) 
      {
        NIDL (2, "Start phase with option AFTER TIME");
        convert_to_hh_mm_ss(tmp_ph->phase_cmd.start_phase.time, time_format);
        fprintf(fp, "SCHEDULE %s %s START AFTER TIME %s\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, time_format);
      }
      /* Case 3: AFTER certain dependent GROUP
       * Syntax:  SCHEDULE <group-name> <phase-name> START AFTER GROUP <dependent group-name>*/
      else if (tmp_ph->phase_cmd.start_phase.start_mode == START_MODE_AFTER_GROUP) 
      {
        int dependnt_grp = tmp_ph->phase_cmd.start_phase.dependent_grp;
        if (tmp_ph->phase_cmd.start_phase.time) 
        {
          convert_to_hh_mm_ss(tmp_ph->phase_cmd.start_phase.time, time_format);
          NIDL (2, "Start phase with option AFTER GROUP with delay");
          fprintf(fp, "SCHEDULE %s %s START AFTER GROUP %s %s\n", scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, scen_grp_entry[dependnt_grp].scen_group_name, time_format);
        } 
        else 
        { 
          NIDL (2, "Start phase with option AFTER GROUP");
          fprintf(fp, "SCHEDULE %s %s START AFTER GROUP %s\n", scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, scen_grp_entry[dependnt_grp].scen_group_name);
        }
      }
      /* End: START PHASE */
    break;
  
    case SCHEDULE_PHASE_RAMP_UP:
      /* Begin: RAMP-UP */
      /* Case 1: Ramping up immmediately
       * Syntax: SCHEDULE <group-name> <phase-name> RAMP_UP ALL IMMEDIATELY */
      if (tmp_ph->phase_cmd.ramp_up_phase.ramp_up_mode == RAMP_UP_MODE_IMMEDIATE)
      {
        NIDL (2, "Ramping up users immediately");
        if (schedule_type == SCHEDULE_TYPE_SIMPLE) {//Simple Scenario
          fprintf(fp, "SCHEDULE %s %s RAMP_UP ALL IMMEDIATELY\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name);
        }
        else if (schedule_by == SCHEDULE_BY_SCENARIO) //Advance Scenario 
        {
          if (get_grp_mode(grp_idx) == TC_FIX_USER_RATE) {//FSR
          fprintf(fp, "SCHEDULE %s %s RAMP_UP %0.3f IMMEDIATELY\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, (double)(generator_entry[gen_idx].scenario_schedule_ptr->phase_array[phase_idx].phase_cmd.ramp_up_phase.num_vusers_or_sess/SESSION_RATE_MULTIPLIER));
          } else {//FCU      
            fprintf(fp, "SCHEDULE %s %s RAMP_UP %d IMMEDIATELY\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, generator_entry[gen_idx].scenario_schedule_ptr->phase_array[phase_idx].phase_cmd.ramp_up_phase.num_vusers_or_sess);
          }  
        } 
        else 
        { //For schedule_by GROUP, In case of advance scenario distribute vuser_or_sess
          if (get_grp_mode(grp_idx) == TC_FIX_USER_RATE) {//FSR
            fprintf(fp, "SCHEDULE %s %s RAMP_UP %0.3f IMMEDIATELY\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, (double)(tmp_ph->phase_cmd.ramp_up_phase.num_vusers_or_sess/SESSION_RATE_MULTIPLIER));
          }
          else {//FCU
            fprintf(fp, "SCHEDULE %s %s RAMP_UP %d IMMEDIATELY\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, tmp_ph->phase_cmd.ramp_up_phase.num_vusers_or_sess); 
          }
        }  
      }
      /* Case 2: Rampup rate
       * Syntax: SCHEDULE <group-name> <phase-name> RAMP_UP <users/ALL> RATE <rate> <per hr/min/sec> <pattern>*/
      else if (tmp_ph->phase_cmd.ramp_up_phase.ramp_up_mode == RAMP_UP_MODE_RATE)
      {
        NIDL (2, "Ramping user with respect to rate");
        if (schedule_by == SCHEDULE_BY_SCENARIO) 
        {
          if (schedule_type == SCHEDULE_TYPE_SIMPLE) {//Simple Scenario
            if (!tool_call_at_init_rtc)
              fprintf(fp, "SCHEDULE %s %s RAMP_UP ALL RATE %.3f M %s\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, (gen_sch_setting[gen_idx].ramp_up_rate / 1000), (tmp_ph->phase_cmd.ramp_up_phase.ramp_up_pattern == RAMP_UP_PATTERN_LINEAR) ? "LINEARLY" : "RANDOMLY");
            /*if (tool_call_at_init_rtc)
              sprintf(generator_entry[gen_idx].gen_keyword, "SCHEDULE %s %s RAMP_UP ALL RATE %.3f M %s\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, (gen_sch_setting[gen_idx].ramp_up_rate / 1000), (tmp_ph->phase_cmd.ramp_up_phase.ramp_up_pattern == RAMP_UP_PATTERN_LINEAR) ? "LINEARLY" : "RANDOMLY");*/
          } else {//Advance scenario FCU
            if (!tool_call_at_init_rtc)
              fprintf(fp, "SCHEDULE %s %s RAMP_UP %d RATE %.3f M %s\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, generator_entry[gen_idx].scenario_schedule_ptr->phase_array[phase_idx].phase_cmd.ramp_up_phase.num_vusers_or_sess, (gen_sch_setting[gen_idx].ramp_up_rate / 1000), (tmp_ph->phase_cmd.ramp_up_phase.ramp_up_pattern == RAMP_UP_PATTERN_LINEAR) ? "LINEARLY" : "RANDOMLY");
            else if (!for_quantity_rtc)
              sprintf(generator_entry[gen_idx].gen_keyword, "SCHEDULE %s %s RAMP_UP %d RATE %.3f M %s\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, generator_entry[gen_idx].scenario_schedule_ptr->phase_array[phase_idx].phase_cmd.ramp_up_phase.num_vusers_or_sess, (gen_sch_setting[gen_idx].ramp_up_rate / 1000), (tmp_ph->phase_cmd.ramp_up_phase.ramp_up_pattern == RAMP_UP_PATTERN_LINEAR) ? "LINEARLY" : "RANDOMLY");
          }
        } 
        else //Group Based FCU
        {
          if (schedule_type == SCHEDULE_TYPE_SIMPLE)  {//Simple Group
            if (!tool_call_at_init_rtc)
              fprintf(fp, "SCHEDULE %s %s RAMP_UP ALL RATE %.3f M %s\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, (tmp_ph->phase_cmd.ramp_up_phase.ramp_up_rate / 1000), (tmp_ph->phase_cmd.ramp_up_phase.ramp_up_pattern == RAMP_UP_PATTERN_LINEAR) ? "LINEARLY" : "RANDOMLY");

            else if (!for_quantity_rtc)
              sprintf(generator_entry[gen_idx].gen_keyword, "SCHEDULE %s %s RAMP_UP ALL RATE %.3f M %s\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, (tmp_ph->phase_cmd.ramp_up_phase.ramp_up_rate / 1000), (tmp_ph->phase_cmd.ramp_up_phase.ramp_up_pattern == RAMP_UP_PATTERN_LINEAR) ? "LINEARLY" : "RANDOMLY"); 
          } else {//Advance Group
            if (!tool_call_at_init_rtc)
              fprintf(fp, "SCHEDULE %s %s RAMP_UP %d RATE %.3f M %s\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, tmp_ph->phase_cmd.ramp_up_phase.num_vusers_or_sess, (tmp_ph->phase_cmd.ramp_up_phase.ramp_up_rate / 1000), (tmp_ph->phase_cmd.ramp_up_phase.ramp_up_pattern == RAMP_UP_PATTERN_LINEAR) ? "LINEARLY" : "RANDOMLY");
            else if(!for_quantity_rtc) 
              sprintf(generator_entry[gen_idx].gen_keyword, "SCHEDULE %s %s RAMP_UP %d RATE %.3f M %s\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, tmp_ph->phase_cmd.ramp_up_phase.num_vusers_or_sess, (tmp_ph->phase_cmd.ramp_up_phase.ramp_up_rate / 1000), (tmp_ph->phase_cmd.ramp_up_phase.ramp_up_pattern == RAMP_UP_PATTERN_LINEAR) ? "LINEARLY" : "RANDOMLY"); 
          }
        }  
      } 
      /* Case 3: Ramp-up step
       * SCHEDULE G1 G1RampUp RAMP_UP ALL STEP 100 00:00:10*/
      else if (tmp_ph->phase_cmd.ramp_up_phase.ramp_up_mode == RAMP_UP_MODE_STEP)
      {
        NIDL (2, "Ramping user with respect to step");
        convert_to_hh_mm_ss(tmp_ph->phase_cmd.ramp_up_phase.ramp_up_step_time, time_format);
        if (schedule_type == SCHEDULE_TYPE_SIMPLE) {//Simple Scenario FCU
          fprintf(fp, "SCHEDULE %s %s RAMP_UP ALL STEP %d %s\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, tmp_ph->phase_cmd.ramp_up_phase.ramp_up_step_users, time_format);
        } 
        else 
        { //Advance Scenario FCU
          if (schedule_by == SCHEDULE_BY_SCENARIO) {
            fprintf(fp, "SCHEDULE %s %s RAMP_UP %d STEP %d %s\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, generator_entry[gen_idx].scenario_schedule_ptr->phase_array[phase_idx].phase_cmd.ramp_up_phase.num_vusers_or_sess, tmp_ph->phase_cmd.ramp_up_phase.ramp_up_step_users, time_format); 
          } else { 
            fprintf(fp, "SCHEDULE %s %s RAMP_UP %d STEP %d %s\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, tmp_ph->phase_cmd.ramp_up_phase.num_vusers_or_sess, tmp_ph->phase_cmd.ramp_up_phase.ramp_up_step_users, time_format); 
          }
        }
      }
      /* Case 4: Ramp-up time
       * SCHEDULE G1 G1RampUp RAMP_UP ALL TIME <time> <pattern>*/
      else if (tmp_ph->phase_cmd.ramp_up_phase.ramp_up_mode == RAMP_UP_MODE_TIME)
      {
        NIDL (2, "Ramping user with respect to time");
        convert_to_hh_mm_ss(tmp_ph->phase_cmd.ramp_up_phase.ramp_up_time, time_format);
        if (schedule_type == SCHEDULE_TYPE_SIMPLE) 
        { //Simple Scenario/Group FCU
          fprintf(fp, "SCHEDULE %s %s RAMP_UP ALL TIME %s %s\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, time_format, (tmp_ph->phase_cmd.ramp_up_phase.ramp_up_pattern == RAMP_UP_PATTERN_LINEAR) ? "LINEARLY" : "RANDOMLY");
        } 
        else //Advance Scenario FCU
        { 
          if (schedule_by == SCHEDULE_BY_SCENARIO) {
            fprintf(fp, "SCHEDULE %s %s RAMP_UP %d TIME %s %s\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, generator_entry[gen_idx].scenario_schedule_ptr->phase_array[phase_idx].phase_cmd.ramp_up_phase.num_vusers_or_sess, time_format, (tmp_ph->phase_cmd.ramp_up_phase.ramp_up_pattern == RAMP_UP_PATTERN_LINEAR) ? "LINEARLY" : "RANDOMLY");
          } else {//Advance Group
            fprintf(fp, "SCHEDULE %s %s RAMP_UP %d TIME %s %s\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, tmp_ph->phase_cmd.ramp_up_phase.num_vusers_or_sess, time_format, (tmp_ph->phase_cmd.ramp_up_phase.ramp_up_pattern == RAMP_UP_PATTERN_LINEAR) ? "LINEARLY" : "RANDOMLY");
          }
        }
      }
      /*FSR: SCHEDULE <grp-name> <phase-name> RAMP_UP <ramp-up-user> TIME_SESSIONS <ramp-up-time> <mode> <step-time/num-steps>*/
      else if (tmp_ph->phase_cmd.ramp_up_phase.ramp_up_mode == RAMP_UP_MODE_TIME_SESSIONS)
      {
        NIDL (2, "Ramping user with respect to time-sessions");
        convert_to_hh_mm_ss(tmp_ph->phase_cmd.ramp_up_phase.ramp_up_time, time_format);
        if (schedule_type == SCHEDULE_TYPE_SIMPLE) //Simple scenario/group
        {
          if (tmp_ph->phase_cmd.ramp_up_phase.step_mode == DEFAULT_STEP) {
            if (!tool_call_at_init_rtc)
              fprintf(fp, "SCHEDULE %s %s RAMP_UP ALL TIME_SESSIONS %s %d\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, time_format, tmp_ph->phase_cmd.ramp_up_phase.step_mode);
            else if(!for_quantity_rtc)
              sprintf(generator_entry[gen_idx].gen_keyword, "SCHEDULE %s %s RAMP_UP ALL TIME_SESSIONS %s %d\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, time_format, tmp_ph->phase_cmd.ramp_up_phase.step_mode); 
          } else {
            if (!tool_call_at_init_rtc)
              fprintf(fp, "SCHEDULE %s %s RAMP_UP ALL TIME_SESSIONS %s %d %d\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, time_format, tmp_ph->phase_cmd.ramp_up_phase.step_mode, tmp_ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions);
            else if(!for_quantity_rtc)
              sprintf(generator_entry[gen_idx].gen_keyword, "SCHEDULE %s %s RAMP_UP ALL TIME_SESSIONS %s %d %d\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, time_format, tmp_ph->phase_cmd.ramp_up_phase.step_mode, tmp_ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions); 
          }
        }
        else //Advance scenario/group
        {
          if (schedule_by == SCHEDULE_BY_SCENARIO) 
          { //FSR Advance scenario
            if (tmp_ph->phase_cmd.ramp_up_phase.step_mode == DEFAULT_STEP) {//default step
              if (!tool_call_at_init_rtc)
                fprintf(fp, "SCHEDULE %s %s RAMP_UP %0.3f TIME_SESSIONS %s %d\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, (double)(generator_entry[gen_idx].scenario_schedule_ptr->phase_array[phase_idx].phase_cmd.ramp_up_phase.num_vusers_or_sess/SESSION_RATE_MULTIPLIER), time_format, tmp_ph->phase_cmd.ramp_up_phase.step_mode);
              else if(!for_quantity_rtc)
                sprintf(generator_entry[gen_idx].gen_keyword, "SCHEDULE %s %s RAMP_UP %0.3f TIME_SESSIONS %s %d\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, (double)(generator_entry[gen_idx].scenario_schedule_ptr->phase_array[phase_idx].phase_cmd.ramp_up_phase.num_vusers_or_sess/SESSION_RATE_MULTIPLIER), time_format, tmp_ph->phase_cmd.ramp_up_phase.step_mode); 
            } else {//Step-time or num-step
              if (!tool_call_at_init_rtc)
                fprintf(fp, "SCHEDULE %s %s RAMP_UP %0.3f TIME_SESSIONS %s %d %d\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, (double)(generator_entry[gen_idx].scenario_schedule_ptr->phase_array[phase_idx].phase_cmd.ramp_up_phase.num_vusers_or_sess/SESSION_RATE_MULTIPLIER), time_format, tmp_ph->phase_cmd.ramp_up_phase.step_mode, tmp_ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions);
              else if(!for_quantity_rtc)
                sprintf(generator_entry[gen_idx].gen_keyword, "SCHEDULE %s %s RAMP_UP %0.3f TIME_SESSIONS %s %d %d\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, (double)(generator_entry[gen_idx].scenario_schedule_ptr->phase_array[phase_idx].phase_cmd.ramp_up_phase.num_vusers_or_sess/SESSION_RATE_MULTIPLIER), time_format, tmp_ph->phase_cmd.ramp_up_phase.step_mode, tmp_ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions); 
            }
          }
          else 
          { //FSR Advance Group
            if (tmp_ph->phase_cmd.ramp_up_phase.step_mode == DEFAULT_STEP) {//default step
              if (!tool_call_at_init_rtc)
                fprintf(fp, "SCHEDULE %s %s RAMP_UP %0.3f TIME_SESSIONS %s %d\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, (double)(tmp_ph->phase_cmd.ramp_up_phase.num_vusers_or_sess/SESSION_RATE_MULTIPLIER), time_format, tmp_ph->phase_cmd.ramp_up_phase.step_mode);
              else if(!for_quantity_rtc)
                sprintf(generator_entry[gen_idx].gen_keyword, "SCHEDULE %s %s RAMP_UP %0.3f TIME_SESSIONS %s %d\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, (double)(tmp_ph->phase_cmd.ramp_up_phase.num_vusers_or_sess/SESSION_RATE_MULTIPLIER), time_format, tmp_ph->phase_cmd.ramp_up_phase.step_mode); 
            } else { //Step-time or num-step
              if (!tool_call_at_init_rtc)
                fprintf(fp, "SCHEDULE %s %s RAMP_UP %0.3f TIME_SESSIONS %s %d %d\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, (double)(tmp_ph->phase_cmd.ramp_up_phase.num_vusers_or_sess/SESSION_RATE_MULTIPLIER), time_format, tmp_ph->phase_cmd.ramp_up_phase.step_mode, tmp_ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions);
              else if(!for_quantity_rtc)
                sprintf(generator_entry[gen_idx].gen_keyword, "SCHEDULE %s %s RAMP_UP %0.3f TIME_SESSIONS %s %d %d\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, (double)(tmp_ph->phase_cmd.ramp_up_phase.num_vusers_or_sess/SESSION_RATE_MULTIPLIER), time_format, tmp_ph->phase_cmd.ramp_up_phase.step_mode, tmp_ph->phase_cmd.ramp_up_phase.tot_num_steps_for_sessions); 
            }
          } 
        }
      }
    /* End: RAMP-UP */
    break;  
    case SCHEDULE_PHASE_STABILIZE:
      /* Begin: STABLIZATION */
      /* Syntax: SCHEDULE <group-name> <phase-name> STABILIZATION TIME <time> */
      if (tmp_ph->phase_cmd.stabilize_phase.time)
      {
        NIDL (2, "Add stablization phase");
        convert_to_hh_mm_ss(tmp_ph->phase_cmd.stabilize_phase.time, time_format);
        fprintf(fp, "SCHEDULE %s %s STABILIZATION TIME %s\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, time_format);
      } 
      else 
      {
        NIDL (2, "Add stablization phase with time 0");
        fprintf(fp, "SCHEDULE %s %s STABILIZATION TIME 0\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name);
      }
      /* End: STABLIZATION */
    break; 
    case SCHEDULE_PHASE_DURATION:
      /* Begin: DURATION */
      /* Case 1: Indefinite time
       * Syntax: SCHEDULE <group-name> <phase-name> DURATION INDEFINITE*/
      if (tmp_ph->phase_cmd.duration_phase.duration_mode == DURATION_MODE_INDEFINITE)
      {
        NIDL (2, "Test running for indefinite time");
        fprintf(fp, "SCHEDULE %s %s DURATION INDEFINITE\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name);
      }
      /* Case 2: Certain time
       * Syntax: SCHEDULE <group-name> <phase-name> DURATION TIME <time> */
      else if (tmp_ph->phase_cmd.duration_phase.duration_mode == DURATION_MODE_TIME)
      {
        NIDL (2, "Test running for definite time");
        convert_to_hh_mm_ss(tmp_ph->phase_cmd.duration_phase.seconds, time_format);
        fprintf(fp, "SCHEDULE %s %s DURATION TIME %s\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, time_format);
      }
      /* Case 3: Certain time
       * Syntax: SCHEDULE <group-name> <phase-name> DURATION SESSIONS <number-of-sessions>
       * Here we are going to use gen_list struct to retrieve session value*/
      else 
      {
        NIDL (2, "Test running for certain number of sessions");
        if(grp_idx == -1)
          fprintf(fp, "SCHEDULE ALL %s DURATION SESSIONS %d %d\n", tmp_ph->phase_name, (tmp_ph->phase_cmd.duration_phase.per_user_fetches_flag)?tmp_ph->phase_cmd.duration_phase.num_fetches:gen_sch_setting[gen_idx].num_sess, tmp_ph->phase_cmd.duration_phase.per_user_fetches_flag);
        else
          fprintf(fp, "SCHEDULE %s %s DURATION SESSIONS %d %d\n", scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, scen_grp_entry[grp_idx].num_fetches, tmp_ph->phase_cmd.duration_phase.per_user_fetches_flag);
        if (tool_call_at_init_rtc)
          sprintf(generator_entry[gen_idx].gen_keyword, "SCHEDULE %s %s DURATION SESSIONS %d\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, gen_sch_setting[gen_idx].num_sess);
      }
      /* End: DURATION */
    break;
    case SCHEDULE_PHASE_RAMP_DOWN: 
      /* Begin: RAMP-DOWN*/
      /* Case 1: Ramping down immmediately
       * Syntax: SCHEDULE <group-name> <phase-name> RAMP_DOWN ALL IMMEDIATELY */
      if (tmp_ph->phase_cmd.ramp_down_phase.ramp_down_mode == RAMP_DOWN_MODE_IMMEDIATE) 
      {
        NIDL (2, "Ramping down users immediately");
        if (tmp_ph->phase_cmd.ramp_down_phase.ramp_down_all) {
          fprintf(fp, "SCHEDULE %s %s RAMP_DOWN ALL IMMEDIATELY\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name);
        } else if (schedule_by == SCHEDULE_BY_SCENARIO) { //Advance case
          if (get_grp_mode(grp_idx) == TC_FIX_USER_RATE) {//FSR
            fprintf(fp, "SCHEDULE %s %s RAMP_DOWN %0.3f IMMEDIATELY\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, (double)(generator_entry[gen_idx].scenario_schedule_ptr->phase_array[phase_idx].phase_cmd.ramp_down_phase.num_vusers_or_sess/SESSION_RATE_MULTIPLIER));
          } else {
            fprintf(fp, "SCHEDULE %s %s RAMP_DOWN %d IMMEDIATELY\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, generator_entry[gen_idx].scenario_schedule_ptr->phase_array[phase_idx].phase_cmd.ramp_down_phase.num_vusers_or_sess);
          }
        } 
        else  
        {//For schedule_by GROUP, In case of advance scenario distribute vuser_or_sess
          if (get_grp_mode(grp_idx) == TC_FIX_USER_RATE) {//FSR
            fprintf(fp, "SCHEDULE %s %s RAMP_DOWN %0.3f IMMEDIATELY\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, (double)(tmp_ph->phase_cmd.ramp_down_phase.num_vusers_or_sess/SESSION_RATE_MULTIPLIER));
          } else {
            fprintf(fp, "SCHEDULE %s %s RAMP_DOWN %d IMMEDIATELY\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, tmp_ph->phase_cmd.ramp_down_phase.num_vusers_or_sess);
          }
        }
      } 
      /* Case 2: Ramping down in certain time
       * Syntax: SCHEDULE <group-name> <phase-name> RAMP_DOWN ALL TIME <time> <pattern> */
      else if (tmp_ph->phase_cmd.ramp_down_phase.ramp_down_mode == RAMP_DOWN_MODE_TIME) 
      {
        NIDL (2, "Ramping down users in certain time");
        convert_to_hh_mm_ss(tmp_ph->phase_cmd.ramp_down_phase.ramp_down_time, time_format);
        if (tmp_ph->phase_cmd.ramp_down_phase.ramp_down_all) {
          fprintf(fp, "SCHEDULE %s %s RAMP_DOWN ALL TIME %s %s\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, time_format, (tmp_ph->phase_cmd.ramp_down_phase.ramp_down_pattern == RAMP_DOWN_PATTERN_LINEAR) ? "LINEARLY" : "RANDOMLY");
        } else if (schedule_by == SCHEDULE_BY_SCENARIO) {//Advance Case
            fprintf(fp, "SCHEDULE %s %s RAMP_DOWN %d TIME %s %s\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, generator_entry[gen_idx].scenario_schedule_ptr->phase_array[phase_idx].phase_cmd.ramp_down_phase.num_vusers_or_sess, time_format, (tmp_ph->phase_cmd.ramp_down_phase.ramp_down_pattern == RAMP_DOWN_PATTERN_LINEAR) ? "LINEARLY" : "RANDOMLY");
        } else {//For schedule_by GROUP, In case of advance scenario distribute vuser_or_sess
          fprintf(fp, "SCHEDULE %s %s RAMP_DOWN %d TIME %s %s\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, tmp_ph->phase_cmd.ramp_down_phase.num_vusers_or_sess, time_format, (tmp_ph->phase_cmd.ramp_down_phase.ramp_down_pattern == RAMP_DOWN_PATTERN_LINEAR) ? "LINEARLY" : "RANDOMLY");
       }
      }
      /* Case 3: Ramping down step mode
       * Syntax: SCHEDULE <group-name> <phase-name> RAMP_DOWN ALL STEP <number of user> <step-time> */
      else if (tmp_ph->phase_cmd.ramp_down_phase.ramp_down_mode == RAMP_DOWN_MODE_STEP)
      {
        NIDL (2, "Ramping users down with respect to step");
        convert_to_hh_mm_ss(tmp_ph->phase_cmd.ramp_down_phase.ramp_down_step_time, time_format);
        if (tmp_ph->phase_cmd.ramp_down_phase.ramp_down_all) {
          fprintf(fp, "SCHEDULE %s %s RAMP_DOWN ALL STEP %d %s\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, tmp_ph->phase_cmd.ramp_down_phase.ramp_down_step_users, time_format);
        } else if (schedule_by == SCHEDULE_BY_SCENARIO) {//Advance Case
          fprintf(fp, "SCHEDULE %s %s RAMP_DOWN %d STEP %d %s\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, generator_entry[gen_idx].scenario_schedule_ptr->phase_array[phase_idx].phase_cmd.ramp_down_phase.num_vusers_or_sess, tmp_ph->phase_cmd.ramp_down_phase.ramp_down_step_users, time_format);
        } else {//For schedule_by GROUP, In case of advance scenario distribute vuser_or_sess
          fprintf(fp, "SCHEDULE %s %s RAMP_DOWN %d STEP %d %s\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, tmp_ph->phase_cmd.ramp_down_phase.num_vusers_or_sess, tmp_ph->phase_cmd.ramp_down_phase.ramp_down_step_users, time_format);
        }
      }
/*FSR: SCHEDULE <ALL/grp-name> <phase-name> RAMP_DOWN <ALL/ramp-up-user> TIME_SESSIONS <ramp-up-time> <mode> <step-time/num-steps>*/
      else if (tmp_ph->phase_cmd.ramp_down_phase.ramp_down_mode == RAMP_DOWN_MODE_TIME_SESSIONS)
      {
        NIDL (2, "Ramping user with respect to time-sessions");
        convert_to_hh_mm_ss(tmp_ph->phase_cmd.ramp_down_phase.ramp_down_time, time_format);
        if (tmp_ph->phase_cmd.ramp_down_phase.ramp_down_all) //Simple Scenario
        {
          if (tmp_ph->phase_cmd.ramp_down_phase.step_mode == DEFAULT_STEP) {
            if (!tool_call_at_init_rtc)
              fprintf(fp, "SCHEDULE %s %s RAMP_DOWN ALL TIME_SESSIONS %s %d\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, time_format, tmp_ph->phase_cmd.ramp_down_phase.step_mode);
            else if(!for_quantity_rtc)
              sprintf(generator_entry[gen_idx].gen_keyword, "SCHEDULE %s %s RAMP_DOWN ALL TIME_SESSIONS %s %d\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, time_format, tmp_ph->phase_cmd.ramp_down_phase.step_mode); 
          } else {
            if (!tool_call_at_init_rtc)
              fprintf(fp, "SCHEDULE %s %s RAMP_DOWN ALL TIME_SESSIONS %s %d %d\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, time_format, tmp_ph->phase_cmd.ramp_down_phase.step_mode, tmp_ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions);
            else if(!for_quantity_rtc)
              sprintf(generator_entry[gen_idx].gen_keyword, "SCHEDULE %s %s RAMP_DOWN ALL TIME_SESSIONS %s %d %d\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, time_format, tmp_ph->phase_cmd.ramp_down_phase.step_mode, tmp_ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions); 
          }
        }
        else if (schedule_by == SCHEDULE_BY_SCENARIO) //Advance scenario
        {
          if (tmp_ph->phase_cmd.ramp_down_phase.step_mode == DEFAULT_STEP) {
            if (!tool_call_at_init_rtc)
              fprintf(fp, "SCHEDULE %s %s RAMP_DOWN %0.3f TIME_SESSIONS %s %d\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, (double)(generator_entry[gen_idx].scenario_schedule_ptr->phase_array[phase_idx].phase_cmd.ramp_down_phase.num_vusers_or_sess/SESSION_RATE_MULTIPLIER), time_format, tmp_ph->phase_cmd.ramp_down_phase.step_mode);
            else if(!for_quantity_rtc)
              sprintf(generator_entry[gen_idx].gen_keyword, "SCHEDULE %s %s RAMP_DOWN %0.3f TIME_SESSIONS %s %d\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, (double)(generator_entry[gen_idx].scenario_schedule_ptr->phase_array[phase_idx].phase_cmd.ramp_down_phase.num_vusers_or_sess/SESSION_RATE_MULTIPLIER), time_format, tmp_ph->phase_cmd.ramp_down_phase.step_mode); 
          } else {
            if (!tool_call_at_init_rtc)
              fprintf(fp, "SCHEDULE %s %s RAMP_DOWN %0.3f TIME_SESSIONS %s %d %d\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, (double)(generator_entry[gen_idx].scenario_schedule_ptr->phase_array[phase_idx].phase_cmd.ramp_down_phase.num_vusers_or_sess/SESSION_RATE_MULTIPLIER), time_format, tmp_ph->phase_cmd.ramp_down_phase.step_mode, tmp_ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions);
            else if(!for_quantity_rtc)
              sprintf(generator_entry[gen_idx].gen_keyword, "SCHEDULE %s %s RAMP_DOWN %0.3f TIME_SESSIONS %s %d %d\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, (double)(generator_entry[gen_idx].scenario_schedule_ptr->phase_array[phase_idx].phase_cmd.ramp_down_phase.num_vusers_or_sess/SESSION_RATE_MULTIPLIER), time_format, tmp_ph->phase_cmd.ramp_down_phase.step_mode, tmp_ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions); 
          }
        }
        else //For schedule_by GROUP, In case of advance scenario distribute vuser_or_sess
        {
          if (tmp_ph->phase_cmd.ramp_down_phase.step_mode == DEFAULT_STEP) {
            if (!tool_call_at_init_rtc)
              fprintf(fp, "SCHEDULE %s %s RAMP_DOWN %0.3f TIME_SESSIONS %s %d\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, (double)(tmp_ph->phase_cmd.ramp_down_phase.num_vusers_or_sess/SESSION_RATE_MULTIPLIER), time_format, tmp_ph->phase_cmd.ramp_down_phase.step_mode);
            else if(!for_quantity_rtc)
              sprintf(generator_entry[gen_idx].gen_keyword, "SCHEDULE %s %s RAMP_DOWN %0.3f TIME_SESSIONS %s %d\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, (double)(tmp_ph->phase_cmd.ramp_down_phase.num_vusers_or_sess/SESSION_RATE_MULTIPLIER), time_format, tmp_ph->phase_cmd.ramp_down_phase.step_mode); 
          } else {
            if (!tool_call_at_init_rtc)
              fprintf(fp, "SCHEDULE %s %s RAMP_DOWN %0.3f TIME_SESSIONS %s %d %d\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, (double)(tmp_ph->phase_cmd.ramp_down_phase.num_vusers_or_sess/SESSION_RATE_MULTIPLIER), time_format, tmp_ph->phase_cmd.ramp_down_phase.step_mode, tmp_ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions);
            else if(!for_quantity_rtc)
              sprintf(generator_entry[gen_idx].gen_keyword, "SCHEDULE %s %s RAMP_DOWN %0.3f TIME_SESSIONS %s %d %d\n", (grp_idx == -1) ? "ALL" : scen_grp_entry[grp_idx].scen_group_name, tmp_ph->phase_name, (double)(tmp_ph->phase_cmd.ramp_down_phase.num_vusers_or_sess/SESSION_RATE_MULTIPLIER), time_format, tmp_ph->phase_cmd.ramp_down_phase.step_mode, tmp_ph->phase_cmd.ramp_down_phase.tot_num_steps_for_sessions); 
          }
        }
      }
     /* End: RAMP-DOWN*/
    break;
    default: /*Flow should never come here*/
      NIDL (2, "Invalid phase type");
      NS_EXIT(-1, "Invalid phase type");
    break; 
  }
  NIDL (1, "Exiting method, keyword = %s", generator_entry[gen_idx].gen_keyword);       
}
/* Wrapper used to distinguish schedule ptr on the basis of schedule_by(Scenario/Group) */
static void check_sch_by_scn_or_grp (FILE *fp, int gen_idx, int tool_call_at_init_rtc, int for_quantity_rtc)
{
  ni_Phases *tmp_ph;
  int grp_id, i;
  NIDL (1, "Method called");
  
  if (tool_call_at_init_rtc) {
    NIDL (1, "rtc_phase_idx = %d, rtc_group_idx = %d", rtc_phase_idx, rtc_group_idx);
    if (schedule_by == SCHEDULE_BY_SCENARIO) 
      tmp_ph = &scenario_schedule_ptr->phase_array[rtc_phase_idx]; 
    else
      tmp_ph = &group_schedule_ptr[rtc_group_idx].phase_array[rtc_phase_idx];
    add_schedule_ph_scen(fp, tmp_ph, rtc_group_idx, gen_idx, rtc_phase_idx, tool_call_at_init_rtc, for_quantity_rtc);
    return;
  }
  if (schedule_by == SCHEDULE_BY_SCENARIO) {
    /*Loop till total number of schedule phases*/
    for (i = 0; i < scenario_schedule_ptr->num_phases; i++) {
      tmp_ph = &scenario_schedule_ptr->phase_array[i]; 
      NIDL (3, "schedule by scenario, curr_phases = %d", i);
      add_schedule_ph_scen(fp, tmp_ph, -1, gen_idx, i, tool_call_at_init_rtc, for_quantity_rtc);
    } 
  }
  else 
  {
    for (grp_id = 0; grp_id < total_sgrp_entries; grp_id++) 
    {
      /*Match generator name, if match then print schedule in file else continue*/
      if (!strcmp(scen_grp_entry[grp_id].generator_name, (char *)generator_entry[gen_idx].gen_name)) {
        NIDL (4, "Generator name match for grp_id = %d", grp_id);
        for (i = 0; i < group_schedule_ptr[grp_id].num_phases; i++) {  
          tmp_ph = &group_schedule_ptr[grp_id].phase_array[i];
          add_schedule_ph_scen(fp, tmp_ph, grp_id, gen_idx, i, tool_call_at_init_rtc, for_quantity_rtc);
        }
      }
    }
  }
  return;
  NIDL (1, "Exit called");
}

/* Function used to reconstruct schedule phase statement for each generator
 * Here we reopen scenario file in append mode */
void reframe_schedule_keyword(char *controller_dir, char *scenario_file_name, char *scen_proj_subproj_dir, int tool_call_at_init_rtc, int for_quantity_rtc)
{
  FILE *fp;
  char generator_dir[FILE_PATH_SIZE];
  char new_scen_file_name[FILE_PATH_SIZE];
  int gen_idx, i;

  NIDL (1, "Method called, sgrp_used_genrator_entries = %d, tool_call_at_init_rtc = %d", sgrp_used_genrator_entries, tool_call_at_init_rtc);
 
  for (gen_idx = 0; gen_idx < sgrp_used_genrator_entries; gen_idx++)
  {
    if(!tool_call_at_init_rtc)
    {
      if (generator_entry[gen_idx].mark_gen == 1)
        return;
      sprintf(generator_dir, "%s/%s/rel/%s/scenarios", controller_dir, generator_entry[gen_idx].gen_name, scen_proj_subproj_dir);
      sprintf(new_scen_file_name, "%s/%s_%s", generator_dir, generator_entry[gen_idx].gen_name, scenario_file_name);
      NIDL(2, "generator_name = %s, new_scen_file_name = %s", generator_dir, new_scen_file_name);  
      if ((fp = fopen(new_scen_file_name, "a")) == NULL)
      {
          NS_EXIT(-1, "Error in opening %s file.", new_scen_file_name);
      }
    }
    /* Added wrapper to distinguish if called for schedule_by scenario/group
     * For group we need to match generator names and print its detail in scenario*/
    check_sch_by_scn_or_grp(fp, gen_idx, tool_call_at_init_rtc, for_quantity_rtc);   
    if(!tool_call_at_init_rtc)
      fclose (fp);
  }
  /* Advance Scenario: Free and make NULL int array malloced in RAMP-UP/DOWN phase
   * in case distributing vuser_or_sess among groups*/ 
  if (schedule_type == SCHEDULE_TYPE_ADVANCED)
  {
    if (schedule_by == SCHEDULE_BY_SCENARIO) 
    {
      for (i = 0; i < scenario_schedule_ptr->num_phases; i++) 
      {
        ni_Phases *ph = &scenario_schedule_ptr->phase_array[i];
        if (ph->phase_type == SCHEDULE_PHASE_RAMP_UP) 
        {
          if (mode == TC_FIX_USER_RATE) {
            free(ph->phase_cmd.ramp_up_phase.num_of_sess_per_gen);  
            ph->phase_cmd.ramp_up_phase.num_of_sess_per_gen = NULL;  
          } else {
            free(ph->phase_cmd.ramp_up_phase.num_vuser_or_sess_per_gen);  
            ph->phase_cmd.ramp_up_phase.num_vuser_or_sess_per_gen = NULL;  
          }
        }  
        if ((ph->phase_type == SCHEDULE_PHASE_RAMP_DOWN) && (!ph->phase_cmd.ramp_down_phase.ramp_down_all)) 
        {
          if (mode == TC_FIX_USER_RATE) {
            free(ph->phase_cmd.ramp_down_phase.num_of_sess_per_gen);  
            ph->phase_cmd.ramp_down_phase.num_of_sess_per_gen = NULL;  
          } else {
            free(ph->phase_cmd.ramp_down_phase.num_vuser_or_sess_per_gen);  
            ph->phase_cmd.ramp_down_phase.num_vuser_or_sess_per_gen = NULL;  
          }
        }  
      } 
    }
  }
  NIDL (1, "Exiting method");
}

/**************************************************************************
 * Description : To validate number of sessions limit with number of users
 *
 *************************************************************************/
static void ni_distribute_sess_limit()
{
  int total_users = 0;
  int i, leftover;
 
  /*Calculate total users, adding users per SGRP group*/
  for (i = 0; i < total_sgrp_entries; i++)
    total_users += scen_grp_entry[i].quantity;
  
  NIDL(1, "Method called, total_users = %d", total_users);
  /*Verify whether given session limit is greater than total users*/
  leftover = total_users - enable_fcs_settings_limit;
  if (leftover < 0 ) {
    NS_EXIT(-1, "Error: Number of users (%d) cannot be less than Concurrent Session Limit (%d)",
              total_users, enable_fcs_settings_limit);
  }
}
/***************************************************************************************************
 * Description:  Redesign the ENABLE_FCS_SETTINGS keyword for the generators
 *
 **************************************************************************************************/
void reframe_fcs_keyword(char *controller_dir, char *scenario_file_name, char *scen_proj_subproj_dir)
{
  FILE *fp;
  char generator_dir[FILE_PATH_SIZE];
  char new_scen_file_name[FILE_PATH_SIZE]; 
  int gen_idx;

  NIDL (1, "Method called, sgrp_used_genrator_entries = %d",sgrp_used_genrator_entries); 

  ni_distribute_sess_limit();
  for (gen_idx = 0; gen_idx < sgrp_used_genrator_entries; gen_idx++)
  {
    if (generator_entry[gen_idx].mark_gen == 1)
      return;
    sprintf(generator_dir, "%s/%s/rel/%s/scenarios", controller_dir, generator_entry[gen_idx].gen_name,
                           scen_proj_subproj_dir);
    sprintf(new_scen_file_name, "%s/%s_%s", generator_dir, generator_entry[gen_idx].gen_name, scenario_file_name);
    NIDL(2, "generator_name = %s, new_scen_file_name = %s", generator_dir, new_scen_file_name);
    if ((fp = fopen(new_scen_file_name, "a")) == NULL)
    {
      NS_EXIT(-1, "Error in opening %s file.", new_scen_file_name);
    }
    fprintf(fp, "ENABLE_FCS_SETTINGS %d %d %d\n",
                 enable_fcs_settings_mode, per_gen_fcs_table[gen_idx].session_limit, enable_fcs_settings_queue_size);
    fclose(fp);
  }
}

void reframe_progress_msecs(char *controller_dir, char *scenario_file_name, char *scen_proj_subproj_dir)
{
  FILE *fp;
  char generator_dir[FILE_PATH_SIZE];
  char new_scen_file_name[FILE_PATH_SIZE];
  int gen_idx;
  NIDL (1, "Method called, progress_msecs = %d", progress_msecs);
  for (gen_idx = 0; gen_idx < sgrp_used_genrator_entries; gen_idx++)
  {
    if (generator_entry[gen_idx].mark_gen == 1)
      return;
    sprintf(generator_dir, "%s/%s/rel/%s/scenarios", controller_dir, generator_entry[gen_idx].gen_name,
                           scen_proj_subproj_dir);
    sprintf(new_scen_file_name, "%s/%s_%s", generator_dir, generator_entry[gen_idx].gen_name, scenario_file_name);
    NIDL(2, "generator_name = %s, new_scen_file_name = %s", generator_dir, new_scen_file_name);
    if ((fp = fopen(new_scen_file_name, "a")) == NULL)
    {   
      NS_EXIT(-1, "Error in opening %s file.", new_scen_file_name);
    }
    fprintf(fp, "PROGRESS_MSECS %d\n", progress_msecs);
    fclose(fp);
  }
}

/* TASK:
 * 1) Function to validate number of sessions
 * 2) Distribute sessions among SGRP groups with respect to users*/
static void ni_distribute_fetches ()
{
  int i, min=1, num_val, num_users, used_val = 0, leftover;
  int total_vals = num_fetches; /*Sessions*/
  int total_users = 0;

  //save_sess_per_grp = (int *)malloc(sizeof(int) * total_sgrp_entries);
  //memset(save_sess_per_grp, 0, sizeof(int) * total_sgrp_entries); //Bug 48835
  NSLB_MALLOC_AND_MEMSET(save_sess_per_grp, (sizeof(int) * total_sgrp_entries), "save sess grp based", -1, NULL);
 
  NIDL (1, "Method called");
  /*Calculate total users, adding users per SGRP group*/
  for (i = 0; i < total_sgrp_entries; i++)
    total_users += scen_grp_entry[i].quantity;

  NIDL (2, "Added SGRP qunatity to obtain total_users = %d", total_users);

  /*Verify number of sessions given by user is not less than total users */
  for (i = 0; i < total_sgrp_entries; i++) 
  {
    num_users = scen_grp_entry[i].quantity;   
    if (!num_users) continue;
    min = num_users;
    num_val = (total_vals * num_users)/total_users;
    NIDL (3, "For group_id [%d]: num_users = %d, minimum usr = %d, num_val = %d", i, num_users, min, num_val);
    if (num_val < min) num_val = min;
    /*Save session value corresponding to SGRP grp index*/
    save_sess_per_grp[i] = num_val;
    used_val += num_val;
    NIDL (3, "Number of sessions per group = %d, Session distributed = %d", save_sess_per_grp[i], used_val);
  }
  /*Verify whether given session are greater than total users*/
  leftover = total_vals - used_val;
  NIDL (3, "Remaining sessions = %d", leftover);
  if(scenario_schedule_ptr->phase_array[3].phase_cmd.duration_phase.per_user_fetches_flag)
    leftover = 0;
  if (leftover < 0 ) {
    NS_EXIT(FAILURE_EXIT, CAV_ERR_1011356, total_vals, total_users);
  }
  /*Distribute leftover among SGRP group*/
  i = 0;
  while (leftover) 
  {
    save_sess_per_grp[i]++;
    leftover--;
    i++;
    if (i == total_sgrp_entries) i = 0;
  }
  /*Debug purpose*/
#ifdef NS_DEBUG_ON //AA
  for (i = 0; i < total_sgrp_entries; i++)
    NIDL(4, "SGRP Group idx=%d  Sessions=%d\n", i,  save_sess_per_grp[i]);
#endif
}

//parsing of num fetches for group schedule
static void ni_distribute_grp_fetches (int grp_idx)
{
  int i, min=1, num_val, num_users, used_val = 0, leftover;
  //int total_vals = num_fetches; /*Sessions*/
  int total_vals = num_fetches;
  int total_users = 0;
 
  NIDL (1, "Method called");
  /*Calculate total users, adding users per SGRP group*/

  for (i = grp_idx; i < (grp_idx + scen_grp_entry[grp_idx].num_generator); i++)
    total_users += scen_grp_entry[i].quantity;

  NIDL (2, "Added SGRP qunatity to obtain total_users = %d, grp_idx = %d, num_fetches = %d", total_users, grp_idx, num_fetches);
  /*Verify number of sessions given by user is not less than total users */
  /* grp_idx is sgrp start idx for each sgrp*/
  for (i = grp_idx; i < (grp_idx + scen_grp_entry[grp_idx].num_generator); i++) 
  {
    num_users = scen_grp_entry[i].quantity;
    if (!num_users) continue;
    min = num_users;
    num_val = (total_vals * num_users)/total_users;
    NIDL (3, "For group_id [%d]: num_users = %d, minimum usr = %d, num_val = %d", i, num_users, min, num_val);
    if (num_val < min) num_val = min;

    if(!group_schedule_ptr[grp_idx].phase_array[3].phase_cmd.duration_phase.per_user_fetches_flag)
      used_val += num_val;
    else
      used_val = num_val = total_vals; //per user session num_val = num_fetches

    /*Save session value corresponding to SGRP grp index*/
    save_sess_per_grp[i] = num_val;
    NIDL (3, "Number of sessions per group = %d, Session distributed = %d", save_sess_per_grp[i], used_val);
  }
  /*Verify whether given session are greater than total users*/
  leftover = total_vals - used_val;
  NIDL (3, "Remaining sessions = %d", leftover);
  if (leftover < 0 ) {
    NS_EXIT(FAILURE_EXIT, CAV_ERR_1011356, total_vals, total_users);
  }
  /*Distribute leftover among SGRP group*/
  i = grp_idx;
  while (leftover) 
  {
    save_sess_per_grp[i]++;
    leftover--;
    i++;
    if (i == (scen_grp_entry[grp_idx].num_generator + grp_idx)) i = grp_idx;
  }
  /*Debug purpose*/
#ifdef NS_DEBUG_ON //AA
  for (i = grp_idx; i < (grp_idx + scen_grp_entry[grp_idx].num_generator); i++)
    NIDL(4, "SGRP Group idx=%d  Sessions=%d\n", i,  save_sess_per_grp[i]);
#endif
}

/*Round double var upto 3 decimal*/
double func_to_round_double_var (double num_to_round)
{
  double result;
  NIDL(1, "Method called, num_to_round = %f", num_to_round);
  num_to_round = num_to_round * 1000;
  result = round(num_to_round);
  result = result / 1000;
  NIDL(2, "After rounding number result = %f", result);
  return(result);
}

/* Function required to calculate init stagger time for each generator
 * Here generator with slowest rate will be initiated first whereas
 * for remaining generators we will be calculating init time to start 
 * them with certain delay*/
static void calculate_rate_per_generator ()
{
  int i;
  double user_per_sec;
  double user_ramp_up_per_sec;
  NIDL (1, "Method called, sgrp_used_genrator_entries = %d", sgrp_used_genrator_entries);

  if (schedule_by == SCHEDULE_BY_SCENARIO)
  {
    /*Malloc start_ramp_up_for_gen to store init stagger time per generator*/
    //start_ramp_up_for_gen = (double *)malloc(sizeof(double) * sgrp_used_genrator_entries);
    NSLB_MALLOC(start_ramp_up_for_gen, (sizeof(double) * sgrp_used_genrator_entries), "ramp up for gen", -1, NULL);
    /*Total generators per group*/
    for (i = 0; i < sgrp_used_genrator_entries; i++)
    {
      /*Calculate rate per second for each generator*/
      user_per_sec = ((gen_sch_setting[i].ramp_up_rate/1000) / 60);
      /*Round variable upto 3 decimal*/
      user_ramp_up_per_sec = func_to_round_double_var(user_per_sec);
      /*Convert time in millsec*/
      start_ramp_up_for_gen[i] = user_ramp_up_per_sec * 1000;
      NIDL (3, "Calculated data user_per_sec = %f, start_ramp_up_for_gen[%d] = %f", user_per_sec, i,
                  start_ramp_up_for_gen[i]);
    }
  }
  NIDL (1, "Exiting method");
}

/* Function required to calculate init stagger time for each generator
 * Here generator with slowest rate will be initiated first whereas
 * for remaining generators we will be calculating init time to start 
 * them with certain delay*/
static void calculate_rate_per_group (int grp_idx)
{
  int i, j;
  double user_per_sec;
  double user_ramp_up_per_sec;

  NIDL (1, "Method called, grp_idx = %d", grp_idx);

  /*Total generators per group*/
  for (j = 0, i = grp_idx; j < scen_grp_entry[grp_idx].num_generator; j++, i++)
  {
    /*Calculate rate per second for each group*/
    ni_Phases *ph = &(group_schedule_ptr[i].phase_array[1]);
    user_per_sec = ((ph->phase_cmd.ramp_up_phase.ramp_up_rate/1000) / 60);
    /*Round variable upto 3 decimal*/
    user_ramp_up_per_sec = func_to_round_double_var(user_per_sec);
    /*Convert time in millsec*/
    ph->phase_cmd.ramp_up_phase.stagger_time_per_group = user_ramp_up_per_sec * 1000;
    NIDL (3, "Calculated data for group_id = %d, user_per_sec = %f, ph->phase_cmd.ramp_up_phase.stagger_time_per_group = %f", i, user_per_sec, ph->phase_cmd.ramp_up_phase.stagger_time_per_group);
  }
  NIDL (1, "Exiting method");
}

/******************************************************************************************************** 
 * Description		:Distribute rate/vuser/session with respect to users in 
 *                       a particular generator
 * 		 	 Formulae => 
 *                       RAMP-Up rate distribution: 
 *                       controller_rate:controller_total_user = gen_rate:gen_total_user 
 *                       RAMP-Up/Down vuser-or-session distribution:
 *  	                 controller_total_usr:controller_vuser-or-sess = gen_total_usr : gen_vuser-or-sess 
 * Input Parameters	:
 * 		 ph     : Phase pointer to update phase distribution
 * 	       flag     : Used to distinguish between rate calculation, vuser or sess distribution
 * Output Parameters	: None
 * Return		: None
 * ********************************************************************************************************/ 
static void distribute_among_total_usr_ratio(ni_Phases *ph, int flag)
{
  int i,j;
  int sum = 0;
  double sum_fsr = 0.0;

  NIDL (1, "Method called");
  /*For ramp-up rate we need to malloc gen_sch_setting ptr*/
  if (!flag) 
  { 
    if (!num_fetches) /*Duration phase with sessions not given then malloc gen_sch_setting struct array ptr*/
      //gen_sch_setting = (gen_name_sch_setting *)malloc(sgrp_used_genrator_entries * sizeof(gen_name_sch_setting));
      NSLB_MALLOC(gen_sch_setting, (sgrp_used_genrator_entries * sizeof(gen_name_sch_setting)), "gen_sch_setting", -1, NULL);
  }
  /*Calculate total users or sessions per generator*/
  for (i = 0; i < sgrp_used_genrator_entries; i++)
  {
    for (j = 0; j < total_sgrp_entries; j++)
    {
      if (!strcmp(scen_grp_entry[j].generator_name, (char *)generator_entry[i].gen_name))
      {
        if (mode == TC_FIX_CONCURRENT_USERS)
          sum += scen_grp_entry[j].quantity;
        else           
          sum_fsr += scen_grp_entry[j].percentage;
        NIDL (1, "Total quantity per generator= %d for gen_id = %d", sum, i);
      }
    }
    //Advance Scenario RAMP-Up vuser-or-sess distribution
    if (flag == 1) 
    { 
      if (mode == TC_FIX_CONCURRENT_USERS) {
        NIDL (3, "FCU: sum = %d, num_connections = %d, ramp_up_num_vuser_or_sess = %d", sum, num_connections, ph->phase_cmd.ramp_up_phase.num_vusers_or_sess);      
        double round_vuser_or_sess = (double)(sum * ph->phase_cmd.ramp_up_phase.num_vusers_or_sess) / num_connections;
        NIDL (3, "Calculated users = %f", round_vuser_or_sess);
        round_vuser_or_sess = round(round_vuser_or_sess); 
        NIDL (3, "Round off calculated users = %f", round_vuser_or_sess);
        ph->phase_cmd.ramp_up_phase.num_vuser_or_sess_per_gen[i] = (int)round_vuser_or_sess; 
        NIDL (3, "Number of users for gen id = %d is %d", i, ph->phase_cmd.ramp_up_phase.num_vuser_or_sess_per_gen[i]);  
      } else { //FSR
        NIDL (3, "FSR: sum_fsr = %f, vuser_rpm = %f, ramp_up_num_vuser_or_sess = %d", sum_fsr, vuser_rpm, ph->phase_cmd.ramp_up_phase.num_vusers_or_sess);
        double round_vuser_or_sess = (double)((sum_fsr * (ph->phase_cmd.ramp_up_phase.num_vusers_or_sess/ SESSION_RATE_MULTIPLIER)) / (vuser_rpm/ SESSION_RATE_MULTIPLIER));
        NIDL (3, "Calculated sessions = %f", round_vuser_or_sess);
        //In case of FSR phases can support upto 3 decimal number, hence we are not going to round off session value
        //While reframing phases for generators value will be printed upto 3 decimal number
        //round_vuser_or_sess = round(round_vuser_or_sess);
        ph->phase_cmd.ramp_up_phase.num_of_sess_per_gen[i] = round_vuser_or_sess;
        NIDL (3, "Number of sessions for gen id = %d is %f", i, ph->phase_cmd.ramp_up_phase.num_of_sess_per_gen[i]);  
      }
    } 
    //Advance Scenario RAMP-Down vuser-or-sess distribution 
    else if (flag == 2) 
    { 
      if (mode == TC_FIX_CONCURRENT_USERS) {
        NIDL (3, "FCU: sum = %d, num_connections = %d, ramp_down_num_vuser_or_sess = %d", sum, num_connections, ph->phase_cmd.ramp_down_phase.num_vusers_or_sess);      
       double round_vuser_or_sess = (double)(sum * ph->phase_cmd.ramp_down_phase.num_vusers_or_sess) / num_connections; 
       round_vuser_or_sess = round(round_vuser_or_sess);
       ph->phase_cmd.ramp_down_phase.num_vuser_or_sess_per_gen[i] = (int)round_vuser_or_sess; 
      } else { //FSR
        NIDL (3, "FSR: sum_fsr = %f, vuser_rpm = %d, ramp_down_num_vuser_or_sess = %d", sum_fsr, vuser_rpm, ph->phase_cmd.ramp_down_phase.num_vusers_or_sess);
        double round_vuser_or_sess = (double)((sum_fsr * (ph->phase_cmd.ramp_down_phase.num_vusers_or_sess / SESSION_RATE_MULTIPLIER)) / (vuser_rpm/ SESSION_RATE_MULTIPLIER));
        //In case of FSR phases can support upto 3 decimal number, hence we are not going to round off session value
        //While reframing phases for generators value will be printed upto 3 decimal number
        //round_vuser_or_sess = round(round_vuser_or_sess);
        ph->phase_cmd.ramp_down_phase.num_of_sess_per_gen[i] = round_vuser_or_sess;
        NIDL (3, "Number of sessions for gen id = %d is %f", i, ph->phase_cmd.ramp_down_phase.num_of_sess_per_gen[i]);  
      }
    } 
    /*Calculate rate ratio per generator*/
    else 
    { 
      NIDL (3, "sum = %d, num_connections = %d, ramp_up_num_vuser_or_sess = %d", sum, num_connections, ph->phase_cmd.ramp_up_phase.ramp_up_rate);      
      gen_sch_setting[i].ramp_up_rate = (sum * ph->phase_cmd.ramp_up_phase.ramp_up_rate) / num_connections;
      NIDL (1, "gen_sch_setting[i].ramp_up_rate = %f", gen_sch_setting[i].ramp_up_rate);
    }
    sum = 0;
    sum_fsr = 0.0;
  }
}

/*Function used to calculate ramp-up rate per group, common code for schedule_by (scenario/group)*/
static void calc_ramp_up_rate_per_grp(int grp_idx)
{
  int sum = 0;
  int i, j;
  NIDL(1, "Method called");
  /*Sum all quantities*/
  for (j = 0, i = grp_idx; j < scen_grp_entry[grp_idx].num_generator; j++, i++)
  {
    sum += scen_grp_entry[i].quantity;
  }
  /*Calculate ramp-up rate per group*/
  for (j = 0, i = grp_idx; j < scen_grp_entry[grp_idx].num_generator; j++, i++)
  {
    ni_Phases *ph = &(group_schedule_ptr[i].phase_array[1]);
    ph->phase_cmd.ramp_up_phase.ramp_up_rate = (double)(scen_grp_entry[i].quantity * ph->phase_cmd.ramp_up_phase.ramp_up_rate) / sum;
  }  
  calculate_rate_per_group(grp_idx);     
}

/************************************************************************************ 
 * Description		: This function is used to distribute schedule settings among 
 * 			  generators. Here we will verify phases, redistribute 
 * 			  schedule setting and save data in memory
 *                        DURATION:
 *                        	Need to divide sessions among number of generators
 *                        RAMP-UP:
 *                         	RampUp Rate distribution among generators 
 *                         	Step-mode distribution
 *                        RAMP-DOWN:
 *                        	Step-mode distribution
 * Input Parameter	: Function gets call for each UNIQUE scenario group
 * Output Parameter	: Save data in schedule structure
 * Return		: None 
 ************************************************************************************/

static void schedule_distribution_among_gen(int grp_idx)
{
  NIDL (1, "Method called"); 
  int i, j;

  if (schedule_type == SCHEDULE_TYPE_SIMPLE) {
    if(grp_idx != -1) {
      for (i = 0; i < group_schedule_ptr[grp_idx].num_phases; i++) {
        if(group_schedule_ptr[grp_idx].phase_array[i].phase_type == SCHEDULE_PHASE_DURATION) {
          num_fetches = group_schedule_ptr[grp_idx].phase_array[i].phase_cmd.duration_phase.num_fetches;
          NIDL(1, "num_fetches = %d", num_fetches);
        }
      }
    }
  } else {
    if(grp_idx != -1) {
      for (i = 0; i < group_schedule_ptr[grp_idx].num_phases; i++) {
        if(group_schedule_ptr[grp_idx].phase_array[i].phase_type == SCHEDULE_PHASE_DURATION) {
          num_fetches = group_schedule_ptr[grp_idx].phase_array[i].phase_cmd.duration_phase.num_fetches; 
          NIDL(1, "num_fetches = %d", num_fetches);
        }
      }
    }
  }
  /* DURATION PHASE: 
   * In schedule setting SESSION mode is selected then these sessions 
   * need to be distributed among generator
   * Here we simply distribute sessions among SGRP groups and then add all 
   * those groups that belong to same generator */
  //if (num_fetches) 
  if (num_fetches) 
  {
    NIDL (2, "Distribute sessions among generators");
    if(grp_idx == -1) {
      ni_distribute_fetches();
      //gen_sch_setting = (gen_name_sch_setting *)malloc(sgrp_used_genrator_entries * sizeof(gen_name_sch_setting));
      //memset(gen_sch_setting, 0, (sgrp_used_genrator_entries * sizeof(gen_name_sch_setting)));
      NSLB_MALLOC_AND_MEMSET(gen_sch_setting, (sgrp_used_genrator_entries * sizeof(gen_name_sch_setting)), "gen sch settings", -1, NULL);
      for (i = 0; i < sgrp_used_genrator_entries; i++)
      {
        for (j = 0; j < total_sgrp_entries; j++)
        {
          if (!strcmp(scen_grp_entry[j].generator_name, (char *)generator_entry[i].gen_name))
          {
            gen_sch_setting[i].num_sess += save_sess_per_grp[j];
            NIDL (2, "Add sessions of all those groups that belong to same generator, gen_id = %d, grp_id = %d, num_sess = %d ", i, j, gen_sch_setting[i].num_sess);
          }
        }
      }
      //TODO - AA - Need to free gen_sch_setting
      free(save_sess_per_grp);
      save_sess_per_grp = NULL;
    }
    else {
      ni_distribute_grp_fetches(grp_idx);
    }
  }

  /* RAMP UP:
   * Case 1: RampUp Rate distribution among generators */
  /*For schedule-by scenario, we need to distribute rate among total users per generators*/
  if (schedule_by == SCHEDULE_BY_SCENARIO) 
  {    
    if (schedule_type == SCHEDULE_TYPE_SIMPLE) 
    {
      ni_Phases *ph = &(scenario_schedule_ptr->phase_array[1]);
      if (ph->phase_cmd.ramp_up_phase.ramp_up_mode == RAMP_UP_MODE_RATE) 
      {
        NIDL(2, "SCHEDULE_BY_SCENARIO: Distribute schedule among generators");
        distribute_among_total_usr_ratio(ph, 0);
        //TODO - AA - Need to free gen_sch_setting
        calculate_rate_per_generator();
      }
    } 
    else 
    {
      for (i = 0; i < scenario_schedule_ptr->num_phases; i++) {
        ni_Phases *ph = &scenario_schedule_ptr->phase_array[i];
        if (ph->phase_type == SCHEDULE_PHASE_RAMP_UP) {
          if (ph->phase_cmd.ramp_up_phase.ramp_up_mode == RAMP_UP_MODE_RATE) {
            NIDL(2, "SCHEDULE_BY_SCENARIO: Distribute schedule among generators");
            distribute_among_total_usr_ratio(ph, 0);
            calculate_rate_per_generator();
          }
        } 
      }  
    }
  } /*For schedule-by group we need to send number of generators used in SGRP group*/
  else
  {
    if (schedule_type == SCHEDULE_TYPE_SIMPLE) 
    {
      ni_Phases *ph = &(group_schedule_ptr[grp_idx].phase_array[1]);
      if (ph->phase_cmd.ramp_up_phase.ramp_up_mode == RAMP_UP_MODE_RATE) 
      {
        NIDL (2, "SCHEDULE_BY_GROUP: Distribute schedule among generator");
        calc_ramp_up_rate_per_grp(grp_idx);
      }
    } 
    else 
    {
      for (i = 0; i < group_schedule_ptr[grp_idx].num_phases; i++) 
      {
        ni_Phases *ph = &(group_schedule_ptr[grp_idx].phase_array[i]);
        if (ph->phase_type == SCHEDULE_PHASE_RAMP_UP) 
        { 
          if (ph->phase_cmd.ramp_up_phase.ramp_up_mode == RAMP_UP_MODE_RATE) 
          {
            NIDL (2, "SCHEDULE_BY_GROUP: Distribute schedule among generator");
            calc_ramp_up_rate_per_grp(grp_idx); 
          }
        }
      } 
    }  
  }//Outer else
}

/* This function is wrapper for schedule phase distribution
 * Purpose: In SCHEDULE_BY_GROUP schedule settings received 
 * for unique SGRP entries, and we need to distribute phases among expanded groups
 * */
void distribute_schedule_among_gen()
{
  int grp_idx = 0, j; 

  NIDL (1, "Method called, total_sgrp_entries = %d", total_sgrp_entries);

  if (schedule_by == SCHEDULE_BY_SCENARIO) 
  {
    NIDL (2, "Distribute phase for schedule_by scenario");
    schedule_distribution_among_gen(-1);
  } 
  else 
  {
    //save_sess_per_grp = (int *)malloc(sizeof(int *) * total_sgrp_entries);
    NSLB_MALLOC(save_sess_per_grp, (sizeof(int *) * total_sgrp_entries), "save_sess_per_grp", -1, NULL);
    while (grp_idx < total_sgrp_entries)  
    {
      if(scen_grp_entry[grp_idx].grp_type == TC_FIX_USER_RATE)
      {
         grp_idx = grp_idx + scen_grp_entry[grp_idx].num_generator;
         continue;
      }
      NIDL (2, "Distribute schedule settings for group_idx = %d", grp_idx);
      schedule_distribution_among_gen(grp_idx);
      grp_idx = grp_idx + scen_grp_entry[grp_idx].num_generator;
      NIDL (2, "Next group_idx = %d", grp_idx);
    }

    for (j = 0; j < total_sgrp_entries; j++)
    {   
      if(scen_grp_entry[j].grp_type == TC_FIX_USER_RATE)
         continue;

      scen_grp_entry[j].num_fetches = save_sess_per_grp[j];
      NIDL (2, "Add sessions of all those groups that belong to same generator grp_id = %d, num_sess = %d",
                            j, scen_grp_entry[j].num_fetches);
    }
    free(save_sess_per_grp);
    save_sess_per_grp = NULL;
  }
  NIDL (1, "Exiting method");
}


/* For FCU: return total number of quantity
 *     FSR: Fill total number of sessions 
 * Therefore in FCU total_sessions will be 0 similarly for FSR total users will be 0
 * */
static int get_total_from_generator_or_group(int idx, int grp_idx, double *total_sess_fsr)
{
  int total = 0;
  double total_fsr = 0.0;
  *total_sess_fsr = total_fsr;
  int j;

  NIDL(1, "Method called gen_id = %d, grp_idx = %d", idx, grp_idx);
  /* total is going to be sum of all grps per generator*/
  if (grp_idx == -1) 
  {
    for (j = 0; j < total_sgrp_entries; j++) 
    {
      NIDL(1, "scen_grp_entry[j].generator_name = %s, generator_entry[idx].gen_name = %s", scen_grp_entry[j].generator_name, generator_entry[idx].gen_name);
      if (!strcmp(scen_grp_entry[j].generator_name, (char *)generator_entry[idx].gen_name)) 
      {
        if (scen_grp_entry[j].grp_type == TC_FIX_CONCURRENT_USERS) { 
          total += scen_grp_entry[j].quantity;
        } else { 
          total_fsr += scen_grp_entry[j].percentage;  
          NIDL (1, "total_fsr = %f scen_grp_entry[%d].percentage = %f", total_fsr, j, scen_grp_entry[j].percentage); 
        }
      }
    }
    *total_sess_fsr = total_fsr;
    NIDL (1, "Total number of users = %d or sessions = %f per generator", total, total_sess_fsr);
    return total;
  } else {  /* Return quantity per group */
    if (scen_grp_entry[grp_idx].grp_type == TC_FIX_CONCURRENT_USERS)
      total = scen_grp_entry[grp_idx + idx].quantity;
    else
      *total_sess_fsr = scen_grp_entry[grp_idx + idx].percentage;
    NIDL (1, "For group index = %d, total number of users = %d or sessions = %f", grp_idx, total, *total_sess_fsr);
    return total;
  }
}

static int active_quantity_left(schedule *schedule_ptr, int num_phases)
{
  ni_Phases *phase_array = schedule_ptr->phase_array;
  ni_Phases *ph_tmp;
  int act_user = 0;
  int i;

  NIDL(1, "num_phases %d", num_phases);
  for (i = 0; i < num_phases; i++) {
    ph_tmp = &(phase_array[i]);

    switch(ph_tmp->phase_type) {
    case SCHEDULE_PHASE_START:
      break;
    case SCHEDULE_PHASE_RAMP_UP:
      act_user += ph_tmp->phase_cmd.ramp_up_phase.num_vusers_or_sess;
      break;
    case SCHEDULE_PHASE_RAMP_DOWN:
      act_user -= ph_tmp->phase_cmd.ramp_down_phase.num_vusers_or_sess;
      break;
    case SCHEDULE_PHASE_DURATION:
      break;
    }
    if (act_user < 0) {
      //TODO: Have to replace exit msg. 
      NS_EXIT(-1, "Something is wrong");
    }
  }

  NIDL(1, "returning %u", act_user);
  return act_user;
}

/* Here we want to sort in decending order so the signs are opposite.*/
static int sort_priority_array(const void *P1, const void *P2)
{
  struct ni_PriorityArray *p1, *p2;
  p1 = (struct ni_PriorityArray *)P1;
  p2 = (struct ni_PriorityArray *)P2;

  if (p1->value > p2->value)
    return -1;
  if (p1->value == p2->value)
    return 0;
  if (p1->value < p2->value)
    return 1;

  /* should not reach */
  return 0;
}



static int is_quantity_left_zero(schedule *schedule_ptr, int num_phases)
{
  ni_Phases *phase_array = schedule_ptr->phase_array;
  ni_Phases *ph_tmp;
  int act_user = 0;
  int i;

  for (i = 0; i < num_phases; i++) {
    ph_tmp = &(phase_array[i]);

    switch(ph_tmp->phase_type) {
    case SCHEDULE_PHASE_START:
      break;
    case SCHEDULE_PHASE_RAMP_UP:
      act_user += ph_tmp->phase_cmd.ramp_up_phase.num_vusers_or_sess;
      break;
    case SCHEDULE_PHASE_RAMP_DOWN:
      act_user -= ph_tmp->phase_cmd.ramp_down_phase.num_vusers_or_sess;
      break;
    case SCHEDULE_PHASE_DURATION:
      break;
    }
    if (act_user < 0) {
      //TODO: Have to change exit msg.
      NS_EXIT(-1, "Something is wrong");
    }
  }

  if (act_user == 0)
    return 1;
  else 
    return 0;
}

/* This function also fills the value for all NVMs */
static void balance_phase_array_per_generator(int qty_to_distribute, double *pct_array, double *qty_array, schedule *schedule_ptr, int grp_idx, int phase_id, int distribute_over_gen_or_groups)
{
  struct ni_PriorityArray  *priority_array;
  int sum = 0, diff = 0;
  ni_Phases  *ph_dest;
  int i, group_idx;
  schedule *dest_schedule;
  int *users_or_sess;
  int gen_id;
  int act_users = 0;
  int hwm;// = calculate_high_water_mark(schedule, schedule->num_phases);
  double hwm_fsr;
  static int last_gen_or_grp = 0;

  NIDL(1, "Method called. phase id = %d, qty_to_distribute = %d, group index = %d, distribute_over_gen_or_group = %d", phase_id, qty_to_distribute, grp_idx, distribute_over_gen_or_groups);

  //priority_array = (struct ni_PriorityArray *)malloc(sizeof(struct ni_PriorityArray) * distribute_over_gen_or_groups);
  NSLB_MALLOC(priority_array, (sizeof(struct ni_PriorityArray) * distribute_over_gen_or_groups), "priority_array", grp_idx, NULL);
  
  for (i = 0; i < distribute_over_gen_or_groups; i++) 
  {
    NIDL(4, "qty_array[%d] = %f", i, qty_array[i]);
    NIDL(4, "pct_array[%d] = %f", i, pct_array[i]);
  }

  for (i = 0; i < distribute_over_gen_or_groups; i++) 
  {
    priority_array[i].idx = i;
    priority_array[i].value = pct_array[i];

    if (grp_idx == -1) 
      dest_schedule = generator_entry[i].scenario_schedule_ptr; 
    else 
      dest_schedule = &(group_schedule_ptr[grp_idx + i]);
    ph_dest = &(dest_schedule->phase_array[phase_id]);
    
    hwm = get_total_from_generator_or_group(i, grp_idx, &hwm_fsr);
    act_users = active_quantity_left(dest_schedule, phase_id);
    NIDL(4, "For id = %d, group_id = %d, hwm = %d, act_users = %d, hwm_fsr = %f, phase_type = %d", i, grp_idx + i, hwm, act_users,
             hwm_fsr, ph_dest->phase_type);
    switch(ph_dest->phase_type) 
    {
      case SCHEDULE_PHASE_RAMP_UP:
        users_or_sess = &(ph_dest->phase_cmd.ramp_up_phase.num_vusers_or_sess);
        if ((act_users + (int)qty_array[i]) > ((scen_grp_entry[i].grp_type == TC_FIX_CONCURRENT_USERS)?hwm:hwm_fsr))
          *users_or_sess = ((scen_grp_entry[i].grp_type == TC_FIX_CONCURRENT_USERS)?hwm:hwm_fsr) - act_users; 
        else 
          *users_or_sess = (int)qty_array[i]; /* Fill initially */
        break;
      case SCHEDULE_PHASE_RAMP_DOWN:
        users_or_sess = &(ph_dest->phase_cmd.ramp_down_phase.num_vusers_or_sess);
        if (act_users < (int)qty_array[i])
          *users_or_sess = act_users;
        else
          *users_or_sess = (int)qty_array[i]; /* Fill initially */ 
        break;
    }
    sum += *users_or_sess;
  }
  
  qsort(priority_array, distribute_over_gen_or_groups, sizeof(struct ni_PriorityArray), sort_priority_array);

  NIDL(4, "After qsort priority_array : ");
  for (i = 0; i < distribute_over_gen_or_groups; i++) {
    NIDL(4, "priority_array[%d].idx = %d, value = %f", i, priority_array[i].idx, priority_array[i].value);
  }
  diff = qty_to_distribute - sum;

  NIDL(4, "Total sum = %d, diff = %u", sum, diff);
  i = last_gen_or_grp;
  while(diff) 
  {
    for (; (i < distribute_over_gen_or_groups) && (diff); i++) 
    {
      gen_id = priority_array[i].idx;

      NIDL(3, "Filling in gen_id = %d, diff = %u, current index i = %d", gen_id,  diff, i);
      if (grp_idx == -1) 
        dest_schedule = generator_entry[i].scenario_schedule_ptr; 
      else 
        dest_schedule = &(group_schedule_ptr[grp_idx + i]);
      ph_dest = &(dest_schedule->phase_array[phase_id]);
      if (ph_dest->phase_type == SCHEDULE_PHASE_RAMP_UP) 
      {
        hwm = get_total_from_generator_or_group(i, grp_idx, &hwm_fsr);
        act_users = active_quantity_left(dest_schedule, phase_id + 1);
        NIDL(3, "Calculated hwm = %d, hwm_fsr = %f, act_users = %d", hwm, hwm_fsr, act_users);

        group_idx = (grp_idx == -1)?i:grp_idx;
        if (act_users < ((scen_grp_entry[group_idx].grp_type == TC_FIX_CONCURRENT_USERS)?hwm:hwm_fsr)) {
          ph_dest->phase_cmd.ramp_up_phase.num_vusers_or_sess++;
          diff--;
          continue;
        } else {
          NIDL(4, "HWM reached hwm = %u, hwm_fsr = %f\n", hwm, hwm_fsr);
        }
      } 
      else if (ph_dest->phase_type == SCHEDULE_PHASE_RAMP_DOWN) 
      {
        if (!is_quantity_left_zero(dest_schedule, phase_id + 1)) 
        {
          ph_dest->phase_cmd.ramp_down_phase.num_vusers_or_sess++;
          diff--;
          NIDL(4, "Filled %s now = %u, diff = %d", ph_dest->phase_name, 
                       ph_dest->phase_cmd.ramp_up_phase.num_vusers_or_sess, diff);
          continue;
        }
      }
    }
    if (i == distribute_over_gen_or_groups) i = 0;
  }
  last_gen_or_grp = i%distribute_over_gen_or_groups;
  NIDL(4, "Free priority array");
  free(priority_array);
  priority_array = NULL;
}
/* distribute_over_gen_or_groups : In case of schedule by scenario we need to distribute schedule phase over
 * generator. In case of schedule by group one need to distribute schedule with respect to group*/
static void distribute_phase_over_generator(int grp_idx, schedule *schedule_ptr, int distribute_over_gen_or_groups)
{ 
  double *qty_array;
  double *pct_array;
  int total_qty = 0;
  double total_fsr = 0.0;
  int grp_mode = get_grp_mode(grp_idx);
  int phase_id, idx;
  ni_Phases *ph;
  int qty_to_distribute;
  schedule *dest_schedule;
  ni_Phases *ph_dest;
  int i, j, group_idx;
  //Array to store value of generator   
  //qty_array = (double *)malloc((sizeof(double) * distribute_over_gen_or_groups));
  NSLB_MALLOC(qty_array, ((sizeof(double) * distribute_over_gen_or_groups)), "qty array phase over gen", grp_idx, NULL);
  //pct_array = (double *)malloc((sizeof(double) * distribute_over_gen_or_groups));
  NSLB_MALLOC(pct_array, ((sizeof(double) * distribute_over_gen_or_groups)), "pct array phase over gen", grp_idx, NULL);

  NIDL(2, "Method called. Total schedule phases %d to distribute over number of generator or within groups %d.", schedule_ptr->num_phases, distribute_over_gen_or_groups);
  //Total users or sessions on controller
  if (grp_idx == -1) 
    total_qty = (grp_mode == TC_FIX_CONCURRENT_USERS)?num_connections : vuser_rpm;
  else //Phase distribution per group within generator
  { 
    for (j = 0, i = grp_idx; j < scen_grp_entry[grp_idx].num_generator; j++, i++)
    {
      if (scen_grp_entry[grp_idx].grp_type == TC_FIX_CONCURRENT_USERS)
        total_qty += scen_grp_entry[i].quantity;
      else
        total_qty += scen_grp_entry[i].percentage;
    }
  }
  NIDL(2, "total_qty to distribute = %u, grp_idx = %d", total_qty, grp_idx);

  //Users/sessions per ramp-up or ramp-down phase to distribute among generators      
  for (phase_id = 0; phase_id < schedule_ptr->num_phases; phase_id++) 
  {
    ph = &(schedule_ptr->phase_array[phase_id]);
    //If phase is neither RAMP-UP nor RAMP-DOWN then continue      
    //if (((ph->phase_type != SCHEDULE_PHASE_RAMP_UP) && (ph->phase_type != SCHEDULE_PHASE_RAMP_DOWN)) || ((ph->phase_type == SCHEDULE_PHASE_RAMP_DOWN) && (ph->phase_cmd.ramp_down_phase.ramp_down_all))) continue;
    if (((ph->phase_type != SCHEDULE_PHASE_RAMP_UP) && (ph->phase_type != SCHEDULE_PHASE_RAMP_DOWN))) continue;

    switch(ph->phase_type) 
    {
      case SCHEDULE_PHASE_RAMP_UP:
        qty_to_distribute = (ph->phase_cmd.ramp_up_phase.num_vusers_or_sess);
        NIDL(1, "grp = %d, Phase %s qty_to_distribute = %u", grp_idx, ph->phase_name, qty_to_distribute);
        break;
      case SCHEDULE_PHASE_RAMP_DOWN:
          qty_to_distribute = (ph->phase_cmd.ramp_down_phase.num_vusers_or_sess);
        NIDL(1, "grp = %d, Phase %s qty_to_distribute = %u", grp_idx, ph->phase_name, qty_to_distribute);
        break;
    }

    //calculate users/sessions per generator   
    for (idx = 0; idx < distribute_over_gen_or_groups; idx++) {
      group_idx = (grp_idx == -1)?idx:grp_idx;
      if (scen_grp_entry[group_idx].grp_type == TC_FIX_CONCURRENT_USERS) {
        pct_array[idx] = (double)(((double)get_total_from_generator_or_group(idx, grp_idx, &total_fsr)) / ((double)total_qty)); 
      } else {
        get_total_from_generator_or_group(idx, grp_idx, &total_fsr);
        pct_array[idx] = (double)(total_fsr / ((double)total_qty)); 
      }
      qty_array[idx] = (double)(qty_to_distribute * pct_array[idx]);
    }

    balance_phase_array_per_generator(qty_to_distribute, pct_array, qty_array, schedule_ptr, grp_idx, phase_id, distribute_over_gen_or_groups);
    /* Print distributed phase. */
    NIDL(2, "Phase %d, qty = %u distributed as:", phase_id, qty_to_distribute);
    for (idx = 0; idx < distribute_over_gen_or_groups; idx++) 
    {
      if (grp_idx == -1)
        dest_schedule = generator_entry[idx].scenario_schedule_ptr; 
      else 
        dest_schedule = &(group_schedule_ptr[grp_idx + idx]);
      ph_dest = &(dest_schedule->phase_array[phase_id]);

      if (ph_dest->phase_type == SCHEDULE_PHASE_RAMP_UP) {
        NIDL(2, "Generator (%d) phase (%d) Ramp Up, qty = %u",
                       idx, phase_id,
                       dest_schedule->phase_array[phase_id].phase_cmd.ramp_up_phase.num_vusers_or_sess);

      } else if (ph_dest->phase_type == SCHEDULE_PHASE_RAMP_DOWN) {
        NIDL(2, "Generator (%d) phase (%d) Ramp Down, qty = %u",
                   idx, phase_id,
                   dest_schedule->phase_array[phase_id].phase_cmd.ramp_down_phase.num_vusers_or_sess);
      }
    }
  }

  //Free both qty_array and pct_array
  free(qty_array);
  qty_array = NULL;
  free(pct_array);
  pct_array = NULL;
}

/* This function is wrapper for schedule phase user/sessions distribution
 * Purpose: In SCHEDULE_BY_GROUP schedule settings received 
 * for unique SGRP entries, and we need to distribute user/session among expanded groups*/
void distribute_vuser_or_sess_among_gen()
{
  int grp_idx = 0; 

  NIDL (1, "Method called, total_sgrp_entries = %d", total_sgrp_entries);

  if (schedule_by == SCHEDULE_BY_SCENARIO) 
  {
    NIDL (2, "Distribute phase for schedule_by scenario");
    distribute_phase_over_generator(-1, scenario_schedule_ptr, sgrp_used_genrator_entries);
  } 
  else 
  {
    while (grp_idx < total_sgrp_entries)  
    {
      NIDL (2, "Distribute schedule settings for group_idx = %d", grp_idx);
      distribute_phase_over_generator(grp_idx, &group_schedule_ptr[grp_idx], scen_grp_entry[grp_idx].num_generator);
      grp_idx = grp_idx + scen_grp_entry[grp_idx].num_generator;
      NIDL (2, "Next group_idx = %d", grp_idx);
    }
  }
  NIDL (1, "Exiting method");
}

/*Alert keyword*/ 
void reframe_alert_keyword(char *controller_dir, char *scenario_file_name, char *scen_proj_subproj_dir)
{
  FILE *fp;
  char generator_dir[FILE_PATH_SIZE];
  char new_scen_file_name[FILE_PATH_SIZE]; 
  int gen_idx;

  NIDL (1, "Method called"); 

  for (gen_idx = 0; gen_idx < sgrp_used_genrator_entries; gen_idx++)
  {
    if (generator_entry[gen_idx].mark_gen == 1)
      return;
    sprintf(generator_dir, "%s/%s/rel/%s/scenarios", controller_dir, generator_entry[gen_idx].gen_name,
                           scen_proj_subproj_dir);
    sprintf(new_scen_file_name, "%s/%s_%s", generator_dir, generator_entry[gen_idx].gen_name, scenario_file_name);
    NIDL(2, "generator_name = %s, new_scen_file_name = %s", generator_dir, new_scen_file_name);
    if ((fp = fopen(new_scen_file_name, "a")) == NULL)
    {
      NS_EXIT(-1, "Error in opening %s file.", new_scen_file_name);
    }
    fprintf(fp, "ENABLE_ALERT %s\n", g_alert_info);

    fclose(fp);
  }
}
