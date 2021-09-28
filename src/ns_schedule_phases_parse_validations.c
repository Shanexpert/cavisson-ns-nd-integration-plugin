#include <stdio.h>
#include <stdlib.h>
#include <regex.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"

#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "user_tables.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "ns_log.h"
#include "ns_alloc.h"
#include "util.h"
#include "timing.h"
#include "tmr.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "divide_users.h" 
#include "divide_values.h" 
#include "child_init.h"
#include "ns_schedule_phases.h"
#include "ns_schedule_phases_parse.h"
#include "ns_schedule_start_group.h"
#include "ns_schedule_stabilize.h"
#include "ns_schedule_duration.h"
#include "ns_schedule_ramp_up_fcu.h"
#include "ns_msg_com_util.h"
#include "ns_schedule_ramp_down_fcu.h"
#include "ns_schedule_stabilize.h"
#include "ns_schedule_duration.h"
#include "ns_global_dat.h"
#include "ns_schedule_phases_dat.h"
#include "ns_goal_based_run.h"
#include "wait_forever.h"
#include "ns_exit.h"
#include "output.h"
//extern void end_test_run( void );

static int check_runprof_tables_pct(void) {
  int total_pct = 0;
  int runprof_idx;

  NSDL2_MISC(NULL, NULL, "Method called");
  for (runprof_idx = 0; runprof_idx < total_runprof_entries; runprof_idx++) {
    total_pct += round(runprof_table_shr_mem[runprof_idx].percentage);
  }
  NSTL1(NULL, NULL, "Total percentage value after round of is %d", total_pct);
  if (total_pct != 10000) {
    return -1;
  }
  return 0;
}

/**
 * Function add_schedule_phase(), adds new phase in the end. If ph is NULL,
 * new phase is allocated and returned. 
 * Return value is the address of where the memory is attached.
 *
 */
Phases *add_schedule_phase(Schedule *schedule, int phase_type, char *phase_name)
{
  Phases *ph;

  schedule->num_phases++;
  MY_REALLOC(schedule->phase_array, sizeof(Phases) * schedule->num_phases, "phase_array", schedule->num_phases - 1);

  NSDL2_SCHEDULE(NULL, NULL, "Adding %s phase\n", phase_name);

  ph = &(schedule->phase_array[schedule->num_phases - 1]);
  memset(ph, 0, sizeof(Phases));

  strncpy(ph->phase_name, phase_name, sizeof(ph->phase_name) - 1);
  ph->phase_type = phase_type;
  ph->phase_status = PHASE_NOT_STARTED;
  ph->default_phase = 0;        /* Must be set to one if desired after calling this function */
  return ph;
}

/**
 * This function:
 * o    Adds start phase anyway at location 0. This happens for all schedule types
 * o    Adds other phases in case of simple schedule.
 * o    For RAL it needs only following phases :
 *    - SCHEDULE_PHASE_START
 *    - SCHEDULE_PHASE_DURATION
 *    - SCHEDULE_PHASE_RAMP_DOWN
 */
void add_default_phases(Schedule *schedule, int grp_idx)
{
  Phases *ph;
  int grp_mode = get_group_mode(grp_idx);

  /* Start */
  ph = add_schedule_phase(schedule, SCHEDULE_PHASE_START, "Start");
  ph->default_phase = 1;
  ph->phase_cmd.start_phase.dependent_grp = -1;
  ph->phase_cmd.start_phase.time = 0;       /* now, Immediately */

  NSDL2_SCHEDULE(NULL, NULL, "Adding START phase\n");
            
  if (global_settings->schedule_type == SCHEDULE_TYPE_SIMPLE) {
    /* Ramp UP */
    if(global_settings->replay_mode == 0){
      ph = add_schedule_phase(schedule, SCHEDULE_PHASE_RAMP_UP, "RampUP");
      ph->default_phase = 1;
      ph->phase_cmd.ramp_up_phase.num_vusers_or_sess = -1;

      if (grp_mode == TC_FIX_CONCURRENT_USERS) {
        ph->phase_cmd.ramp_up_phase.ramp_up_mode = RAMP_UP_MODE_RATE;
        ph->phase_cmd.ramp_up_phase.ramp_up_rate = convert_to_per_minute("RAMP_UP 120 M"); /* Sending dummy keyword */
      } else {
        ph->phase_cmd.ramp_up_phase.ramp_up_mode = RAMP_UP_MODE_IMMEDIATE;
      }

      /* Stabilize */
      ph = add_schedule_phase(schedule, SCHEDULE_PHASE_STABILIZE, "Stabilization");
      ph->default_phase = 1;
      //ph->phase_cmd.stabilize_phase.duration_mode = 1; /* time */
      ph->phase_cmd.stabilize_phase.time = 0;
    }

    /* If duration is not given we assume indefinite, we add it later when doing validations */
    /* Duration */
    ph = add_schedule_phase(schedule, SCHEDULE_PHASE_DURATION, "Duration");
    ph->default_phase = 1;
    ph->phase_cmd.duration_phase.duration_mode = 0; /* Indefinite */
    /* NetOmni changes: Added check for SGRP groups with quantity/percentage value is 0
     * In regard to bug#4780, all those SGRP with quant/pct zero were getting default indefinite duration
     * where as in NI we simply parse these groups and use them for transactions.
     * Issue: Now these groups on a generator were running indefinitly, which made test to run indefinitly
     * Solution: Making default duration phase as TIME mode with 0 sec for all such groups*/
    if (loader_opcode == CLIENT_LOADER) {
      if (global_settings->schedule_by == SCHEDULE_BY_GROUP) {
        /* NOTE: In CLIENT_LOADER only NUM mode is supported therefore verfying only quantity field of runprof_table_shr_mem*/
        if (runProfTable[grp_idx].quantity == 0) {     
          ph->phase_cmd.duration_phase.duration_mode = DURATION_MODE_TIME;
          ph->phase_cmd.duration_phase.seconds = get_time_from_format("00:00:00");
        }
      } 
    }  
    /* Ramp Down */
    ph = add_schedule_phase(schedule, SCHEDULE_PHASE_RAMP_DOWN, "RampDown");
    ph->default_phase = 1;
    ph->phase_cmd.ramp_down_phase.ramp_down_mode = RAMP_DOWN_MODE_IMMEDIATE;
    ph->phase_cmd.ramp_down_phase.num_vusers_or_sess = -1;
  }
}

void initialize_schedule(Schedule *schedule, int grp_idx) 
{
  memset(schedule, 0, sizeof(Schedule));

  schedule->group_idx = -1;
  schedule->phase_idx = -1;
  schedule->num_phases = 0;
  schedule->phase_array = NULL;
  schedule->total_ramp = 0;
  schedule->cum_total_ramp = 0;
  schedule->total_ramp_done = 0;
  schedule->cum_total_ramp_done = 0;
  schedule->prev_total_ramp_done = 0;
  schedule->prev_cum_total_ramp_done = 0;
  schedule->actual_timeout = 0;
  
  add_default_phases(schedule, grp_idx);
}

/**
 * Function is used to calculate High water mark among the phases sequentially.
 *
 * HWM calculation:
 *
 *       		Active Users	LR Max Users	NS Max Users
 * RU 100		100		100		100
 * RU 200		300		300		300
 * RD 100		200		300		300
 * RU 200		400		500		400
 * RD ALL		0		500		400 (HWM)
 *
 */
int calculate_high_water_mark(Schedule *schedule, int num_phases)
{
  Phases *phase_array = schedule->phase_array;
  Phases *ph_tmp;
  int act_user = 0, hwm = 0;
  int i;

  NSDL3_SCHEDULE(NULL, NULL, "Method called, num_phases = %d", num_phases);

  for (i = 0; i < num_phases; i++) {
    ph_tmp = &(phase_array[i]);

    if (ph_tmp->phase_type == SCHEDULE_PHASE_RAMP_UP) {
      act_user += ph_tmp->phase_cmd.ramp_up_phase.num_vusers_or_sess;
    } else if (ph_tmp->phase_type == SCHEDULE_PHASE_RAMP_DOWN) {
      /* This method can be called before filling qty all. So check for -1 */
      if (ph_tmp->phase_cmd.ramp_up_phase.num_vusers_or_sess == -1)
        act_user = 0;
      else
        act_user -= ph_tmp->phase_cmd.ramp_up_phase.num_vusers_or_sess;
    }

    NSDL3_SCHEDULE(NULL, NULL, "Hwm = %d, act_user = %d", hwm, act_user);
    if (act_user < 0) {
      NS_EXIT(-1, "Phase %s is invalid", ph_tmp->phase_name);
    }
    
    if (hwm < act_user) {
      hwm = act_user;
    }
  }

  NSDL3_SCHEDULE(NULL, NULL, "Hwm = %u", hwm);
  return hwm;
}

static void calculate_rpc(Schedule *schedule, int group_mode, int grp_idx)
{
  Phases *phase_array = schedule->phase_array;
  Phases *ph_tmp;
  int i;
  int tot_users;
  
  NSDL3_SCHEDULE(NULL, NULL, "Method called grp_idx = %d, global_settings->num_connections = %d",
                 schedule->group_idx, global_settings->num_connections);

  for (i = 0; i < schedule->num_phases; i++) {
    ph_tmp = &(phase_array[i]);
    /* RAMP_UP ALL is only for simple scenario */
    if (ph_tmp->phase_type == SCHEDULE_PHASE_RAMP_UP) {
      if (group_mode == TC_FIX_CONCURRENT_USERS) { /* FCU */

        /* Below we are using values from parent's structures. With this we can not free
         * parent structures in child to save memory. */
        if (global_settings->schedule_by == SCHEDULE_BY_GROUP)
          tot_users = group_schedule[grp_idx].phase_array[i].phase_cmd.ramp_up_phase.num_vusers_or_sess;
        else
          tot_users = scenario_schedule->phase_array[i].phase_cmd.ramp_up_phase.num_vusers_or_sess;

/*         if (global_settings->schedule_by == SCHEDULE_BY_GROUP) */
/*           tot_users = runprof_table_shr_mem[grp_idx].quantity; */
/*         else */
/*           tot_users = global_settings->num_connections; /\* From NUM_USERS *\/ */

        if(ph_tmp->phase_cmd.ramp_up_phase.ramp_up_mode == RAMP_UP_MODE_RATE) {
          if (tot_users <= 0) {
            ph_tmp->phase_cmd.ramp_up_phase.rpc = 0;
          } else  {
            ph_tmp->phase_cmd.ramp_up_phase.rpc = 
              ((((double) ph_tmp->phase_cmd.ramp_up_phase.ramp_up_rate * 
                 (double)ph_tmp->phase_cmd.ramp_up_phase.num_vusers_or_sess) /
                (double)tot_users));
          }

          NSDL4_SCHEDULE(NULL, NULL, "grp = %d, phase = %d/%s , ramp_up_rate = %f, num_vusers_or_sess = %u, "
                         "tot_users = %u, rpc = %f",
                         grp_idx, i, ph_tmp->phase_name, 
                         ph_tmp->phase_cmd.ramp_up_phase.ramp_up_rate,
                         ph_tmp->phase_cmd.ramp_up_phase.num_vusers_or_sess,
                         tot_users, ph_tmp->phase_cmd.ramp_up_phase.rpc);

        } else if( ph_tmp->phase_cmd.ramp_up_phase.ramp_up_mode == RAMP_UP_MODE_TIME) {
          ph_tmp->phase_cmd.ramp_up_phase.rpc = (double)ph_tmp->phase_cmd.ramp_up_phase.num_vusers_or_sess /
                                                (double)(ph_tmp->phase_cmd.ramp_up_phase.ramp_up_time/(1000.0*60));
        }
      }
      
      NSDL3_SCHEDULE(NULL, NULL, "Phase id %d (%s), setting rps = %f", i, ph_tmp->phase_name, 
                     ph_tmp->phase_cmd.ramp_up_phase.rpc);
    } else if (ph_tmp->phase_type == SCHEDULE_PHASE_RAMP_DOWN) {
      if (group_mode == TC_FIX_CONCURRENT_USERS) { /* FCU */

        /* Below we are using values from parent's structures. With this we can not free
         * parent structures in child to save memory. */
        if (global_settings->schedule_by == SCHEDULE_BY_GROUP)
          tot_users = group_schedule[grp_idx].phase_array[i].phase_cmd.ramp_down_phase.num_vusers_or_sess;
        else
          tot_users = scenario_schedule->phase_array[i].phase_cmd.ramp_down_phase.num_vusers_or_sess;

/*         if (global_settings->schedule_by == SCHEDULE_BY_GROUP) */
/*           tot_users = runprof_table_shr_mem[grp_idx].quantity; */
/*         else */
/*           tot_users = global_settings->num_connections; /\* From NUM_USERS *\/ */

        if( ph_tmp->phase_cmd.ramp_down_phase.ramp_down_mode == RAMP_DOWN_MODE_TIME) {
          ph_tmp->phase_cmd.ramp_down_phase.rpc = (double)ph_tmp->phase_cmd.ramp_down_phase.num_vusers_or_sess /
            (double)(ph_tmp->phase_cmd.ramp_down_phase.ramp_down_time/(1000.0*60));
        }
      }
      
      NSDL3_SCHEDULE(NULL, NULL, "Phase id %d (%s), setting rps = %f", i, ph_tmp->phase_name, 
                     ph_tmp->phase_cmd.ramp_down_phase.rpc);
    }
  }
}

void calculate_rpc_per_phase_per_child()
{
  int i, k;

  int group_mode;
  
  for (k = 0; k < global_settings->num_process; k++) {
    if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO) {
      if (scenario_schedule->num_phases == 1) {
        NS_EXIT(-1, "No run phase given.");
      }
      group_mode = get_group_mode(-1);
      //calculate_rpc(scenario_schedule, group_mode, -1);
      calculate_rpc(v_port_table[k].scenario_schedule, group_mode, -1);
    } else if (global_settings->schedule_by == SCHEDULE_BY_GROUP) {

      for (i = 0; i < total_runprof_entries; i++) {
        if ((loader_opcode != CLIENT_LOADER) || 
            ((loader_opcode == CLIENT_LOADER) && (runprof_table_shr_mem[i].quantity != 0))) {
          group_mode = get_group_mode(i);
          if (group_schedule[i].num_phases == 1) {
            NS_EXIT(-1, "No run phase given for Group id %d", i);
          }
          calculate_rpc(&(v_port_table[k].group_schedule[i]), group_mode, i);
        }
      }
    }
  }
}

/**
 * simple scheduling and scenario based schedule:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * o    In case of FCU and number mode, total users are taken from sum of all SGRP keywords.
 * o    In case of FCU and percentage mode, total users are taken from NUM_USERS keyword.
 * o    In case of FSR and number mode, total session rate are taken from sum of all SGRP keyword.
 * o    In case of FSR and percentage mode, total session rate are taken from TARGET_RATE keyword.
 *
 * simple scheduling and group based schedule:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * o    In case of FCU and number mode, total users are taken from the SGRP keyword of the group.
 * o    In case of FCU and percentage mode, total users are taken by applying percentage from SGRP keyword on NUM_USERS keyword.
 * o    In case of FSR and number mode, total session rate are taken from the SGRP keyword of the group.
 * o    In case of FSR and percentage mode, total session rate are taken applying percentage from SGRP keyword on TARGET_RATE keyword.
 *
 * o    In case of advanced scheduling and scenario based schedule,
 *      users or session rate specified by the phase are ramped up at a rate specified by this phase. 
 * o    In case of advanced scheduling and group based schedule,
 *      users or session rate specified by the phase of the group are ramped up at a rate specified by this phase. 
 *
 * o    All users for Ramp down should work in advanced mode.
 *
 */
/* This function fills users or sessions ALL (-1) with value */
static void fill_qty_all(Schedule *schedule, int group_mode, int grp_idx)
{
  Phases *phase_array = schedule->phase_array;
  Phases *ph_tmp;
  int act_qty = 0;                /* Acvive users */
  int i, j;
  
  NSDL3_SCHEDULE(NULL, NULL, "Method called grp_idx = %d, global_settings->num_connections = %d, group_mode = %d",
                 schedule->group_idx, global_settings->num_connections, group_mode);

  for (i = 0; i < schedule->num_phases; i++) {
    ph_tmp = &(phase_array[i]);
    /* RAMP_UP ALL is only for simple scenario */
    if ((ph_tmp->phase_type == SCHEDULE_PHASE_RAMP_UP) && 
        ((ph_tmp->phase_cmd.ramp_up_phase.num_vusers_or_sess == -1) || 
         (run_mode == FIND_NUM_USERS || run_mode == STABILIZE_RUN))) {
      if (group_mode == TC_FIX_USER_RATE) { /* FSR */
        if (global_settings->schedule_by == SCHEDULE_BY_GROUP)
          ph_tmp->phase_cmd.ramp_up_phase.num_vusers_or_sess = runprof_table_shr_mem[grp_idx].quantity;
        else {
          ph_tmp->phase_cmd.ramp_up_phase.num_vusers_or_sess = global_settings->vuser_rpm; /* From TARGET_RATE */
        }
      } else if (group_mode == TC_FIX_CONCURRENT_USERS) { /* FCU */
        if (global_settings->schedule_by == SCHEDULE_BY_GROUP)
          ph_tmp->phase_cmd.ramp_up_phase.num_vusers_or_sess = runprof_table_shr_mem[grp_idx].quantity;
        else
          ph_tmp->phase_cmd.ramp_up_phase.num_vusers_or_sess = global_settings->num_connections; /* From NUM_USERS */
      } else {                  /* goal based */
        ph_tmp->phase_cmd.ramp_up_phase.num_vusers_or_sess = *run_variable;
      }

      NSDL3_SCHEDULE(NULL, NULL, "Phase id %d (%s), setting num_vusers_or_sess = %u", i, ph_tmp->phase_name, 
                     ph_tmp->phase_cmd.ramp_up_phase.num_vusers_or_sess);

    } else if ((ph_tmp->phase_type == SCHEDULE_PHASE_RAMP_DOWN) && 
               ((ph_tmp->phase_cmd.ramp_down_phase.num_vusers_or_sess == -1) ||
                (run_mode == FIND_NUM_USERS || run_mode == STABILIZE_RUN))) {
      /* In case of SCHEDULE_PHASE_RAMP_DOWN, ALL means ramp down all remaining active qty */
      act_qty = 0;
      for (j = 0; j < i; j++) {
        Phases *ph_new = &(phase_array[j]);
        if (ph_new->phase_type == SCHEDULE_PHASE_RAMP_UP) {
          act_qty += ph_new->phase_cmd.ramp_up_phase.num_vusers_or_sess;
        } else if (ph_new->phase_type == SCHEDULE_PHASE_RAMP_DOWN) {
          act_qty -= ph_new->phase_cmd.ramp_up_phase.num_vusers_or_sess;
        }
      }
      if (act_qty </* = */ 0) {     /* Something is wrong in construction of phases */
        NS_EXIT(-1, "Invalid scheduling definition for RampUp Phase id %d (%s) of group %s\n"
                    "Reason: RampUp and RampDown users will be mismatch.\n",
                    i, ph_tmp->phase_name, runprof_table_shr_mem[grp_idx].scen_group_name);
      }
      
      ph_tmp->phase_cmd.ramp_down_phase.num_vusers_or_sess = act_qty;
      
      NSDL3_SCHEDULE(NULL, NULL, "Phase id %d (%s), setting num_vusers_or_sess = %u", i, ph_tmp->phase_name, 
                     ph_tmp->phase_cmd.ramp_down_phase.num_vusers_or_sess);
    }
  }
}

/**
 */
void high_water_mark_check(Schedule *schedule, int grp_mode, int grp_idx)
{
  int total_users_or_sess;
  
  int hwm = 0; /* High water mark */
  
  NSDL3_SCHEDULE(NULL, NULL, "Method called\n");

  if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO) {
    total_users_or_sess = (grp_mode == TC_FIX_CONCURRENT_USERS) ? 
      global_settings->num_connections : global_settings->vuser_rpm; 
  } else {
    total_users_or_sess = runprof_table_shr_mem[grp_idx].quantity; /* Already converted from %age to Qty in convert_pct_to_qty() */
  }

  hwm = calculate_high_water_mark(schedule, schedule->num_phases);

  if (hwm < total_users_or_sess) {
    NS_EXIT(-1, "Inconstant scenario, all users not utilized");
  } else if (hwm > total_users_or_sess) {
    NS_EXIT(-1, "Inconstant scenario, number of users/sessions defined in phases are more than total.");
  }
  
  NSDL3_SCHEDULE(NULL, NULL, "High water mark check passed, hwm = %u, total_users_or_sess = %u\n", 
                 hwm, total_users_or_sess);
/*   if (act_user != 0) { */
/*     fprintf(stderr, "Invlid phases; Users are still left out in the system after the last phase.\n"); */
/*     exit(-1); */
/*   } */
}

/**
 * Magic functions.
 *
 * The function, over rides array with values removing fractions and 
 * arranging the them.
 * 
 * The array passed, is balanced based on its fractions. The larger fraction
 * receives the left over first, then the smaller fraction. This goes on in 
 * cyclic manner.
 */
void balance_array(double *array, int len, int total_qty)
{
  int i;
  int sum = 0;
  int left = 0;
  double fraction, max_fraction;
  int max_idx = 0;

  for (i = 0; i < len; i++) {
    sum += (int)array[i];
  }

  if (sum != total_qty) {
    left = total_qty - sum;
    
    while (left) {
      max_idx = 0;
      max_fraction = 0;
      for (i = 0; i < len; i++) {
        fraction = array[i] - (int)array[i];
        if (fraction > max_fraction) {
          max_fraction = fraction;
          max_idx = i;
        }
      }

      left--;                   /* Reduced so we know how much are left */
      array[max_idx] = array[max_idx] + 1;
      array[max_idx] = (int) array[max_idx];
    }
  }
  
  for (i = 0; i < len; i++) {
    array[i] = (int) array[i];
  }
}

/* Step 1: Convert pct to quantity. */
void convert_pct_to_qty()
{
  int grp_mode = testCase.mode;
  double *qty_array;
  int total_qty;
  int i;

  NSDL3_SCHEDULE(NULL, NULL, "Method called global_settings->use_prof_pct = %d\n", global_settings->use_prof_pct);

//  if (global_settings->use_prof_pct == PCT_MODE_PCT) 
    {

    /* total_qty */
    if (global_settings->schedule_type == SCHEDULE_TYPE_ADVANCED) {        /* for both FSR and FCU */
      total_qty = calculate_high_water_mark(scenario_schedule, scenario_schedule->num_phases);
      /* We also set num_connections, like it was set by NUM_USERS */
      if (grp_mode == TC_FIX_CONCURRENT_USERS)
        global_settings->num_connections = total_qty;
      else 
        global_settings->vuser_rpm = total_qty;

    } else {
      /* This will also work for goal based scenarios */
      total_qty = (grp_mode == TC_FIX_CONCURRENT_USERS) ? global_settings->num_connections : global_settings->vuser_rpm; 
    }

    MY_MALLOC(qty_array, sizeof(double) * total_runprof_entries, "qty_array", -1);

    for (i = 0; i < total_runprof_entries; i++) {
      qty_array[i] = (double)((double)total_qty * (double)((runprof_table_shr_mem[i].percentage / 100.0) / 100.0));
    }

#ifdef NS_DEBUG_ON
    NSDL4_SCHEDULE(NULL, NULL, "total_qty = %u\n", total_qty);
    for (i = 0; i < total_runprof_entries; i++) {
      NSDL4_SCHEDULE(NULL, NULL, "percentage  = %lf, qty_array[%d] = %lf", ((runprof_table_shr_mem[i].percentage / 100.0) / 100.0),
                     i, qty_array[i]);
    }
#endif  /* NS_DEBUG_ON */

    balance_array(qty_array, total_runprof_entries, total_qty);

#ifdef NS_DEBUG_ON
    for (i = 0; i < total_runprof_entries; i++) {
      NSDL4_SCHEDULE(NULL, NULL, "Adjusted qty_array[%d] = %f", i, qty_array[i]);
    }
#endif  /* NS_DEBUG_ON */

    for (i = 0; i < total_runprof_entries; i++) {
      runprof_table_shr_mem[i].quantity = (int)qty_array[i];
    }

    FREE_AND_MAKE_NOT_NULL(qty_array, "qty_array", -1);
  } 
}

int get_group_mode(int grp_idx)
{
  if (grp_idx == -1)
    return testCase.mode;
  
  if (testCase.mode == TC_MIXED_MODE) {
    if (runProfTable) /* This is done in case this function is called once we have freed tmp structs. 
                       * After this the data is in shr mem. */
      return runProfTable[grp_idx].grp_type; /* FSR, FCU */
    else 
      return runprof_table_shr_mem[grp_idx].grp_type; /* FSR, FCU */
  } else 
    return testCase.mode;
}

/**
 * This function detects cyclic dependency among groups
 */
static void cyclic_dependency_check(int grp_idx, int now_idx)
{
  int num;
  int i;
  int *dependent_grp_array = NULL;

  MY_MALLOC(dependent_grp_array, sizeof(int) * total_runprof_entries, "dependent_grp_array", -1);

  num = get_dependent_group(now_idx, dependent_grp_array);

  if (num == -1) {               /* Terminating condition */
    FREE_AND_MAKE_NULL(dependent_grp_array, "dependent_grp_array", -1);
    return;
  }

  for (i = 0; i < num; i++) {
    if (grp_idx == dependent_grp_array[i]) {
      NS_EXIT(-1, "Cyclic Group dependency found.");
    }
    cyclic_dependency_check(grp_idx, dependent_grp_array[i]);
  }

  FREE_AND_MAKE_NULL(dependent_grp_array, "dependent_grp_array", -1);
}

/**
 * This Function:
 * o    Substitutes -1 for ALL (users) - not doing now.
 * o    Checks any know anomalies, if found exits.
 * 
 * Anomalies: (only left overs till now. All others should have already been handled)
 * o    If no phase defined after START phase, we give error.
 * o    No phase should be added after Duration phase Indefinate. -- not doing
 * o    High Water mark should not exceed number of users defined for group/scenario
 * o    The sum of all users should be Zero in the end (Removing Ramp Down). -- should we do it?
 */
void validate_phases()
{
  int i;

  //  set_total_users();

  int group_mode;
  char vuser_sess_buf[64];
  //Bug#3942: In case of advance scenario, we dont have default phase values
  //Here RAMP_UP phase is mandatory, hence added check to verify whether user
  //has given RAMP_UP phase of not

  if(global_settings->schedule_type == SCHEDULE_TYPE_ADVANCED) //Advance scenario
  {
    if((global_settings->schedule_by == SCHEDULE_BY_GROUP)) //Group
    {
      for(i = 0; i < total_runprof_entries; i++)
      {
        //NetCloud: In case of generator, we can have scenario configuration with SGRP sessions/users 0.
        //Here we need to by-pass RAMP_UP check, because on generator schedule phases for such group does not exists
        if ((loader_opcode != CLIENT_LOADER) || 
            ((loader_opcode == CLIENT_LOADER) && (runprof_table_shr_mem[i].quantity != 0))) 
        {
          if(!group_schedule[i].ramp_up_define)
          {
            NS_EXIT(-1, "In group %s, RAMP_UP phase is mandatory in ADVANCED scenario type,"
                          " please add RAMP_UP phase and re-run the test", runprof_table_shr_mem[i].scen_group_name);
          }
       }
      }
    }
    else //scenario
    {
      if(!scenario_schedule->ramp_up_define)
      {
         NS_EXIT(-1, "RAMP_UP phase is mandatory in ADVANCED scenario type,"
                        " please add RAMP_UP phase and re-run the test");
      }
    } 
  }
  
  /* Check group cyclic dependency */
  if (global_settings->schedule_by == SCHEDULE_BY_GROUP) {
    for (i = 0; i < total_runprof_entries; i++) {
      cyclic_dependency_check(i, i);
    }
  }
  
  if (global_settings->use_prof_pct == PCT_MODE_PCT) 
  {
    if (check_runprof_tables_pct() < 0)
      NS_EXIT(-1, "Total percentage of scenario groups is not adding up to 100");
    convert_pct_to_qty();
    //Bug: 67989 - vusers showing 0 on summary.top
    if(get_group_mode(-1) == TC_FIX_CONCURRENT_USERS)
      snprintf(vuser_sess_buf, 63, "%d", global_settings->num_connections); 
    else
      snprintf(vuser_sess_buf, 63, "%.3f", global_settings->vuser_rpm/THOUSAND); 

    NSDL4_SCHEDULE(NULL, NULL, "setting summary top last column as %s", vuser_sess_buf);
    update_summary_top_field(15, vuser_sess_buf, 0);
  }

  //For JMeter type script number of users per group should be 1
  if((loader_opcode != MASTER_LOADER) && (g_script_or_url == 100))
  {
    for(i = 0; i < total_runprof_entries; i++)
    {
      if(runprof_table_shr_mem[i].sess_ptr->jmeter_sess_name && (runprof_table_shr_mem[i].quantity > 1))   
        NS_EXIT(-1, "Only one user is required for JMeter type script, group %s has more than one user",
                       runprof_table_shr_mem[i].scen_group_name);
    }
  }

  /* fill -1 with ALL */
  if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO) {
    group_mode  = get_group_mode(-1);
    if (scenario_schedule->num_phases == 1) {
      NS_EXIT(-1, "No run phase given.");
    }
    fill_qty_all(scenario_schedule, group_mode, -1);
  } else if (global_settings->schedule_by == SCHEDULE_BY_GROUP) {
    for (i = 0; i < total_runprof_entries; i++) {
      //NetCloud: In case of generator, we can have scenario configuration with SGRP sessions/users 0.
      //Here we need to by-pass RAMP_UP check, because on generator schedule phases for such group does not exists
      if ((loader_opcode != CLIENT_LOADER) || 
            ((loader_opcode == CLIENT_LOADER) && (runprof_table_shr_mem[i].quantity != 0))) 
      {
        group_mode  = get_group_mode(i);
        if (group_schedule[i].num_phases == 1) {
          NS_EXIT(-1, "No run phase given for Group id %d.", i);
        }
        fill_qty_all(&(group_schedule[i]), group_mode, i);
      }
    }
  }

  /* Now check for High Water Mark Anomalies; should only be done for Advanced scenarios */
  if (global_settings->schedule_type == SCHEDULE_TYPE_ADVANCED) {
    if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO) {
      NSDL4_SCHEDULE(NULL, NULL, "Checking high water mark for Scenario Schedule");
      high_water_mark_check(scenario_schedule, group_mode, -1);
    } else {                      /* group based */
      for (i = 0; i < total_runprof_entries; i++) {
        NSDL4_SCHEDULE(NULL, NULL, "Checking high water mark for Group Schedule grp idx  = %d", i);
        //Added check for netomni
        if (loader_opcode == CLIENT_LOADER) {
          if (global_settings->schedule_by == SCHEDULE_BY_GROUP) {
            /* NOTE: In CLIENT_LOADER only NUM mode is supported therefore verfying only quantity field of runprof_table_shr_mem*/
            if (runprof_table_shr_mem[i].quantity == 0) {
              NSDL4_SCHEDULE(NULL, NULL, "Ignoring scenario group with quantity given zero, index = %d", i);
              continue;
            }
          }
        }   
        high_water_mark_check(&(group_schedule[i]), group_mode, i);
      }
    }
  }

  /* For goal based we do not need to do estimations and logs. These should be done
   * only for normal run */
  if((get_group_mode(-1) != TC_FIX_CONCURRENT_USERS && 
      get_group_mode(-1) != TC_FIX_USER_RATE) && 
     (run_mode == FIND_NUM_USERS || run_mode == STABILIZE_RUN))
    return;
  #ifndef CAV_MAIN
  //Check debug test default values
  estimate_target_completion();
  /* Schedule phases.dat will not be created in replay mode. */
  if(!global_settings->replay_mode)
    log_schedule_phases_dat();
  #endif
}

