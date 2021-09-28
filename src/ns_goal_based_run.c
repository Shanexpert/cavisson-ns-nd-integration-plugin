/******************************************************************
 * Name    : ns_goal_based_run.c
 * Purpose : This file contains methods related to goal based execution
 * Note    :
 * Author  : Neeraj
 * Intial version date: May 21, 08
 * Last modification date: May 21, 08
 *****************************************************************/

#include <stdio.h>
#include <math.h>
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
#include "util.h"
#include "tmr.h"
#include "timing.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "wait_forever.h"
#include "ns_string.h"
#include "ns_gdf.h"
#include "ns_log.h"
#include "ns_summary_rpt.h"
#include "ns_health_monitor.h"
#include "ns_goal_based_run.h"
#include "ns_schedule_phases_parse.h"
#include "ns_exit.h"

// For last_more (Holds the status of last run in SLA)
#define LAST_RUN_SYS_HEALTHY         -1 // Initial value
#define LAST_RUN_SYS_NOT_HEALTHY     -2
#define LAST_RUN_SYS_OVER_CAPACITY   -3

#define ULTIMATE_MAX_NUM_STAB_RUNS 100
#define STATE_NOT_INITIALIZED 99

static int last_sys_overload, last_cap_overload;
static int last_not_met = 0;               // Last max run_variable for which we got -2
static int state = STATE_NOT_INITIALIZED;    // Do we need static? 99 means not initialzed

int *run_variable, run_length;

static short get_run_mode_for_initilization(int mode)
{
  if(mode == FIND_NUM_USERS)
    return NS_GOAL_DISCOVERY;
 //else if(mode == STABILIZE_RUN)
  return NS_GOAL_STABILIZE;  
}
static inline char *show_vector_option_type(int vector_option)
{
  if (vector_option == SLA_VECTOR_OPTION_ALL)       return ("All");
  if (vector_option == SLA_VECTOR_OPTION_OVERALL)   return ("OverAll");
  if (vector_option == SLA_VECTOR_OPTION_SPECIFIED) return ("Specified");
  if (vector_option == SLA_VECTOR_OPTION_NA)        return ("NA");
  else return ("Invalid vector option");
}

static inline char *show_relation_type(int relation)
{
  if (relation == SLA_RELATION_LESS_THAN) return ("LESS_THAN");
  if (relation == SLA_RELATION_GR_THAN)   return ("GREATER_THAN");
  if (relation == SLA_RELATION_EQUAL)     return ("EQUAL_TO");
  else return ("Invalid relation");
}

//return  value
// -2 - Increase load and rember the current load
// -1 - Increase load 
// -0 - SLA met
//  1 - Decresae load
static inline int check_sla_condition_discovery()
{
  int i;
  int less = 0; // Number of SLA which are less than target
  int more = 0; // Number of SLA which are more than target
  int less_not_met = 0; // Number of > or = SLA which are not met result < target
  SLATableEntry_Shr *slaTablePtr = sla_table_shr_mem;
  int group_vector_idx, graph_vector_idx;

  // Following variables are for debug log only
  int less_rule_sla = 0, more_rule_sla = 0, equal_rule_sla = 0; // Number of such SLA
  int less_rule_sla_met = 0, more_rule_sla_met = 0, equal_rule_sla_met = 0;
  int total_entries = 0;

  NSDL3_TESTCASE(NULL, NULL, "Method called. total_sla_entries = %d, slaTablePtr = %p", total_sla_entries, slaTablePtr);

  for (i = 0; i < total_sla_entries; i++, slaTablePtr++)
  {
    for (group_vector_idx = 0; group_vector_idx < slaTablePtr->gdf_group_num_vectors; group_vector_idx++)  {
      for (graph_vector_idx = 0; graph_vector_idx < slaTablePtr->gdf_graph_num_vectors; graph_vector_idx++) {
        total_entries++;
        if (slaTablePtr->vector_option == SLA_VECTOR_OPTION_ALL) {
          slaTablePtr->gdf_group_vector_idx = group_vector_idx;
          slaTablePtr->gdf_graph_vector_idx = graph_vector_idx;
        }
        double c_avg = get_gdf_data_c_avg (slaTablePtr->gdf_rpt_gp_num_idx, slaTablePtr->gdf_rpt_graph_num_idx, slaTablePtr->gdf_group_vector_idx, slaTablePtr->gdf_graph_vector_idx, slaTablePtr->vector_option);

       write_log_file(get_run_mode_for_initilization(run_mode),"Checking SLA %d. Group: '%s', Group Vector: '%s', Graph: '%s', Graph Vector: '%s', Option: '%s', Relation '%s', Target = %.3f, Variation = %.3f%%, Actual = %.3f\n", \
                  i + 1, get_gdf_group_name(slaTablePtr->gdf_rpt_gp_num_idx), get_gdf_group_vector_name(slaTablePtr->gdf_rpt_gp_num_idx, group_vector_idx), get_gdf_graph_name(slaTablePtr->gdf_rpt_gp_num_idx, slaTablePtr->gdf_rpt_graph_num_idx), get_gdf_graph_vector_name(slaTablePtr->gdf_rpt_gp_num_idx, slaTablePtr->gdf_rpt_graph_num_idx, graph_vector_idx), show_vector_option_type(slaTablePtr->vector_option), show_relation_type(slaTablePtr->relation), slaTablePtr->value, slaTablePtr->pct_variation, c_avg);

        NSDL3_TESTCASE(NULL, NULL, "NSController: Checking SLA %d. Group: '%s', Group Vector: '%s', Graph: '%s', Graph Vector: '%s', Option: '%s', Relation '%s', Target = %.3f, Variation = %.3f%%, Actual = %.3f\n", \
                  i + 1, get_gdf_group_name(slaTablePtr->gdf_rpt_gp_num_idx), get_gdf_group_vector_name(slaTablePtr->gdf_rpt_gp_num_idx, group_vector_idx), get_gdf_graph_name(slaTablePtr->gdf_rpt_gp_num_idx, slaTablePtr->gdf_rpt_graph_num_idx), get_gdf_graph_vector_name(slaTablePtr->gdf_rpt_gp_num_idx, slaTablePtr->gdf_rpt_graph_num_idx, graph_vector_idx), show_vector_option_type(slaTablePtr->vector_option), show_relation_type(slaTablePtr->relation), slaTablePtr->value, slaTablePtr->pct_variation, c_avg);
        
        fprint3f(rfp, stderr, NULL, "NSController: Checking SLA %d. Group: '%s', Group Vector: '%s', Graph: '%s', Graph Vector: '%s', Option: '%s', Relation '%s', Target = %.3f, Variation = %.3f%%, Actual = %.3f\n", \
                  i + 1, get_gdf_group_name(slaTablePtr->gdf_rpt_gp_num_idx), get_gdf_group_vector_name(slaTablePtr->gdf_rpt_gp_num_idx, group_vector_idx), get_gdf_graph_name(slaTablePtr->gdf_rpt_gp_num_idx, slaTablePtr->gdf_rpt_graph_num_idx), get_gdf_graph_vector_name(slaTablePtr->gdf_rpt_gp_num_idx, slaTablePtr->gdf_rpt_graph_num_idx, graph_vector_idx), show_vector_option_type(slaTablePtr->vector_option), show_relation_type(slaTablePtr->relation), slaTablePtr->value, slaTablePtr->pct_variation, c_avg);

        if (slaTablePtr->relation == SLA_RELATION_LESS_THAN)
          { 
            less_rule_sla++;
            if(c_avg < slaTablePtr->value)
              {
                less_rule_sla_met++;
                less++; // Condition met but we need to increase the load to maximize load
              } 
            else 
              more++; // Condition not met. So we need to decrease the load.
          }
        else if (slaTablePtr->relation == SLA_RELATION_GR_THAN)
          {
            more_rule_sla++;
            if(c_avg < slaTablePtr->value)
              {
                less++; // Condition not met. So we need to increase the load.
                less_not_met++; // Increament this var to indicate we need to continue increasing load even if less == total_sla_entries
              }
            else 
              {
                more_rule_sla_met++;
                less++; // Condition met but we need to increase the load to maximize load
              }
          }
        else // Equal rule
          {
            equal_rule_sla++;
            if(c_avg < (((slaTablePtr->value) * (100 - slaTablePtr->pct_variation))/100))
              {
                less++; // Condition not met. So we need to increase the load.
                less_not_met++; // Increament this var to indicate we need to continue increasing load even if less == total_sla_entries
              }
            else if(c_avg > (((slaTablePtr->value) * (100 + slaTablePtr->pct_variation))/100))
              more++; // Condition not met. So we need to decrease the load.
            else
              equal_rule_sla_met++;
          }
      }


    }
    }

  NSDL3_TESTCASE(NULL, NULL, "Number of less_rule_sla = %d, more_rule_sla = %d, equal_rule_sla = %d. Number of less_rule_sla_met = %d, more_rule_sla_met = %d, equal_rule_sla_met = %d. more = %d, less = %d", \
  less_rule_sla, more_rule_sla, equal_rule_sla, less_rule_sla_met, more_rule_sla_met, equal_rule_sla_met, more, less);

  if (more) // At least one SLA is more than target.
  {
    NSDL3_TESTCASE(NULL, NULL, "At least one SLA is more than target. Returning value 1");
    return 1;
  }
  else if(less_not_met) 
  {
    NSDL3_TESTCASE(NULL, NULL, "At least one greater or equal SLA is not met and is less than target. Returning value -2");
    return -2; 
  }
  else if (less == total_entries)
  {
    NSDL3_TESTCASE(NULL, NULL, "All SLA are not met and are less than target. Returning value -1");
    return -1;
  }
  else
  {
    NSDL3_TESTCASE(NULL, NULL, "All SLA met. Returning value 0");
    return 0;
  }
}


// This method is called in stablization phase
//return  value
// -1 - load  is less
// -0 - SLA met
//  1 - load is more
static inline int check_sla_condition_stab()
{
  int i;
  int less = 0; // Number of SLA which are less than target
  int more = 0; // Number of SLA which are more than target
  SLATableEntry_Shr *slaTablePtr = sla_table_shr_mem;
  int group_vector_idx, graph_vector_idx;

  NSDL3_TESTCASE(NULL, NULL, "Method called. total_sla_entries = %d, slaTablePtr = %p", total_sla_entries, slaTablePtr);

  for (i = 0; i < total_sla_entries; i++, slaTablePtr++) {
    for (group_vector_idx = 0; group_vector_idx < slaTablePtr->gdf_group_num_vectors; group_vector_idx++)  {
      for (graph_vector_idx = 0; graph_vector_idx < slaTablePtr->gdf_graph_num_vectors; graph_vector_idx++) {
        if (slaTablePtr->vector_option == SLA_VECTOR_OPTION_ALL) {
          slaTablePtr->gdf_group_vector_idx = group_vector_idx;
          slaTablePtr->gdf_graph_vector_idx = graph_vector_idx;
        }
        double c_avg = get_gdf_data_c_avg (slaTablePtr->gdf_rpt_gp_num_idx, slaTablePtr->gdf_rpt_graph_num_idx, slaTablePtr->gdf_group_vector_idx, slaTablePtr->gdf_graph_vector_idx, slaTablePtr->vector_option);

        write_log_file(get_run_mode_for_initilization(run_mode), "Checking SLA %d. Group: '%s', Group Vector: '%s', Graph: '%s', Graph Vector: '%s', Option: '%s', Relation '%s', Target = %.3f, Variation = %.3f%%, Actual = %.3f\n", \
                  i + 1, get_gdf_group_name(slaTablePtr->gdf_rpt_gp_num_idx), get_gdf_group_vector_name(slaTablePtr->gdf_rpt_gp_num_idx, group_vector_idx), get_gdf_graph_name(slaTablePtr->gdf_rpt_gp_num_idx, slaTablePtr->gdf_rpt_graph_num_idx), get_gdf_graph_vector_name(slaTablePtr->gdf_rpt_gp_num_idx, slaTablePtr->gdf_rpt_graph_num_idx, graph_vector_idx), show_vector_option_type(slaTablePtr->vector_option), show_relation_type(slaTablePtr->relation), slaTablePtr->value, slaTablePtr->pct_variation, c_avg);

        NSDL3_TESTCASE(NULL, NULL, "NSController: Checking SLA %d. Group: '%s', Group Vector: '%s', Graph: '%s', Graph Vector: '%s', Option: '%s', Relation '%s', Target = %.3f, Variation = %.3f%%, Actual = %.3f\n", \
                  i + 1, get_gdf_group_name(slaTablePtr->gdf_rpt_gp_num_idx), get_gdf_group_vector_name(slaTablePtr->gdf_rpt_gp_num_idx, group_vector_idx), get_gdf_graph_name(slaTablePtr->gdf_rpt_gp_num_idx, slaTablePtr->gdf_rpt_graph_num_idx), get_gdf_graph_vector_name(slaTablePtr->gdf_rpt_gp_num_idx, slaTablePtr->gdf_rpt_graph_num_idx, graph_vector_idx), show_vector_option_type(slaTablePtr->vector_option), show_relation_type(slaTablePtr->relation), slaTablePtr->value, slaTablePtr->pct_variation, c_avg);
        
        fprint3f(rfp, stderr, NULL, "NSController: Checking SLA %d. Group: '%s', Group Vector: '%s', Graph: '%s', Graph Vector: '%s', Option: '%s', Relation '%s', Target = %.3f, Variation = %.3f%%, Actual = %.3f\n", \
                  i + 1, get_gdf_group_name(slaTablePtr->gdf_rpt_gp_num_idx), get_gdf_group_vector_name(slaTablePtr->gdf_rpt_gp_num_idx, group_vector_idx), get_gdf_graph_name(slaTablePtr->gdf_rpt_gp_num_idx, slaTablePtr->gdf_rpt_graph_num_idx), get_gdf_graph_vector_name(slaTablePtr->gdf_rpt_gp_num_idx, slaTablePtr->gdf_rpt_graph_num_idx, graph_vector_idx), show_vector_option_type(slaTablePtr->vector_option), show_relation_type(slaTablePtr->relation), slaTablePtr->value, slaTablePtr->pct_variation, c_avg);
        
        if(c_avg < (((slaTablePtr->value) * (100 - slaTablePtr->pct_variation))/100))
          less++; // Condition not met. 
        else if(c_avg > (((slaTablePtr->value) * (100 + slaTablePtr->pct_variation))/100))
          more++; // Condition not met. 
      }
    }
  }

  if((less == 0) && (more == 0))
  {
    NSDL3_TESTCASE(NULL, NULL, "All SLA met. Returning value 0");
    return 0;
  }
  if (more) // At least one SLA is more than target.
  {
    NSDL3_TESTCASE(NULL, NULL, "At least one SLA is more than target. Returning value 1");
    return 1;
  }
  else 
  {
    NSDL3_TESTCASE(NULL, NULL, "At least one SLA is less than target. Returning value -1");
    return -1;
  }
}

//return 
// -2 - Decrease load and rember the current load
// -1
// -0
// -1
static int tstate_criteria_met(avgtime* end_results, int pct, cavgtime **g_cavg) 
{
  NSDL3_TESTCASE(NULL, NULL, "Method called., Pct = %d, testcase_mode = %d, global_settings->test_stab_time = %d, total_sla_entries = %d", pct, testcase_shr_mem->mode, global_settings->test_stab_time, total_sla_entries);
  // int udp_array_idx;
  int comp_per_round;
  int num_users;
  cavgtime *cavg = (cavgtime *)(g_cavg)[0];

  switch (testcase_shr_mem->mode) 
  {
    case TC_FIX_MEAN_USERS:

      num_users = (int) rint((double)end_results->total_cum_user_ms / ((double)(global_settings->test_stab_time * 1000)));
     
      write_log_file(get_run_mode_for_initilization(run_mode),"Checking result. Actual mean users : %d, Target mean users: %d, Sess avg time: %lld, user/min:%d", num_users, global_settings->num_connections, end_results->sess_avg_time, global_settings->vuser_rpm);
      NSDL3_TESTCASE(NULL, NULL, "NSController: Checking result. Actual mean users : %d, Target mean users: %d, Sess avg time: %lld, user/min: %d\n", 
                    num_users, global_settings->num_connections, end_results->sess_avg_time, global_settings->vuser_rpm);
      fprint3f(rfp, stdout, NULL, 
               "NSController: Checking result. Actual mean users : %d, Target mean users: %d\n",
               num_users, global_settings->num_connections);
      
      if (cavg->url_fetches_completed - cavg->url_succ_fetches)  /* there are failures */ 
      {
        write_log_file(get_run_mode_for_initilization(run_mode),"Actual results have errors.  Assuming test run is an overload");
        NSDL3_TESTCASE(NULL, NULL, "NSController: Actual results have errors.  Assuming test run is an overload\n");
        fprint3f(rfp, stdout, NULL, "NSController: Actual results have errors.  Assuming test run is an overload\n");
        return 1;
      }

      if (num_users < (((global_settings->num_connections) * (100 - pct))/100))
        return -1;
      else if (num_users > (((global_settings->num_connections) * (100 + pct))/100))
        return 1;
      else
        return 0;
      break;

    case TC_FIX_HIT_RATE:
    case TC_FIX_PAGE_RATE:
    case TC_FIX_TX_RATE:
      NSDL3_TESTCASE(NULL, NULL, "total url completed = %llu, total url successful = %llu \n", 
                cavg->url_fetches_completed, cavg->url_succ_fetches);

      switch(testcase_shr_mem->mode) 
      {
        case TC_FIX_HIT_RATE:
          comp_per_round = ((float) cavg->url_succ_fetches/(float) global_settings->test_stab_time) * 60;
          break;
        case TC_FIX_PAGE_RATE:
          comp_per_round = ((float) cavg->pg_succ_fetches/ (float) global_settings->test_stab_time) * 60;
          break;
        case TC_FIX_TX_RATE:
          comp_per_round = ((float) cavg->tx_c_succ_fetches/ (float) global_settings->test_stab_time) * 60;
          break;
      }
      write_log_file(get_run_mode_for_initilization(run_mode),"Checking result. Actual %s: %d, target: %d", ns_target_buf, comp_per_round, testcase_shr_mem->target_rate);
      NSDL3_TESTCASE(NULL, NULL, "NSController: Checking result. Actual %s: %d, target: %d\n", ns_target_buf, comp_per_round, testcase_shr_mem->target_rate);
      fprint3f(rfp, stdout, NULL, "NSController: Checking result. Actual %s: %d, target: %d\n", ns_target_buf, comp_per_round, testcase_shr_mem->target_rate);
      
      if (cavg->url_fetches_completed - cavg->url_succ_fetches)  /* there are failures */ 
      {
        write_log_file(get_run_mode_for_initilization(run_mode),"Actual results have errors.  Assuming test run is an overload");
        fprint3f(rfp, stdout, NULL, "NSController: Actual results have errors.  Assuming test run is an overload. "
                 "url_fetches_completed = %llu, url_succ_fetches = %llu \n", 
                 cavg->url_fetches_completed, cavg->url_succ_fetches);

        NSDL3_TESTCASE(NULL, NULL, "NSController: Actual results have errors.  Assuming test run is an overload. "
                  "url_fetches_completed = %llu, url_succ_fetches = %llu \n", 
                  cavg->url_fetches_completed, cavg->url_succ_fetches);
        return 1;
      }

      if (comp_per_round < (((testcase_shr_mem->target_rate) * (100 - pct))/100))
        return -1;
      else if (comp_per_round > (((testcase_shr_mem->target_rate) * (100 + pct))/100))
        return 1;
      else
        return 0;

      break;

    case TC_MEET_SLA:
      if(run_mode == FIND_NUM_USERS) // discovery phasse
        return (check_sla_condition_discovery());
      else
        return (check_sla_condition_stab());
      break;

#if 0
  int i;
  MetricTableEntry_Shr* metricTablePtr;
  int less, more;
    case TC_MEET_SERVER_LOAD:
      if (end_results->url_fetches_completed - end_results->url_succ_fetches)  /* there are failures */ 
      {
        fprint3f(stdout, rfp, NULL, "NSController: Actual results have errors.  Assuming test run is an overload\n");
        NSDL3_TESTCASE(vptr, cptr, "NSController: Actual results have errors.  Assuming test run is an overload\n");
        return 1;
      }
      less = 0; more = 0;
      metricTablePtr = metric_table_shr_mem;
      for (i = 0; i < total_metric_entries; i++, metricTablePtr++) 
      {
        udp_array_idx = metricTablePtr->udp_array_idx;
        if ((global_settings->module_mask & TESTCASE_OUTPUT)) 
        {
          fprint3f(stdout, rfp, NULL, "NSController: actual metric result: %d, target result: %d\n", udp_array[udp_array_idx].value_rcv, metricTablePtr->target_value);
          NSDL3_TESTCASE(vptr, cptr, "NSController: actual metric result: %d, target result: %d\n", udp_array[udp_array_idx].value_rcv, metricTablePtr->target_value);
        }
        if (metricTablePtr->relation == METRIC_LESS_THAN)
        {
          if (udp_array[udp_array_idx].value_rcv < (((metricTablePtr->target_value) * (100 - pct))/100))
            less++;
          else if (udp_array[udp_array_idx].value_rcv > (((metricTablePtr->target_value) * (100 + pct))/100))
            more++;
        } else
          if (udp_array[udp_array_idx].value_rcv < (((metricTablePtr->target_value) * (100 - pct))/100))
            more++;
          else if (udp_array[udp_array_idx].value_rcv > (((metricTablePtr->target_value) * (100 + pct))/100))
            less++;
      }
      if (more)
        return 1;
      else if (less == total_metric_entries)
        return -1;
      else
        return 0;
      break;
#endif

  }
  return 0;
}

static inline int test_long_enough(cavgtime *c_end_results) 
{
  if (c_end_results->sess_succ_fetches == 0) {
      NSDL3_TESTCASE(NULL, NULL, "No successful Sessions completed.");
      return 0;
  }
  NSDL3_TESTCASE(NULL, NULL, "Method Returning 1, sess_succ_fetches = %llu", 
            c_end_results->sess_succ_fetches);
  return 1;
}

void check_system_capacity(avgtime *avg)
{
  // Moved from deleiver_report.c
  static int num_seq_increasing = 0;
  static unsigned long long first_increasing;
  static long long last_num_tries ;

  NSDL3_TESTCASE(NULL, NULL, "Method called. numseq_increasing = %d, first_increasing = %llu, last_num_tries = %llu, cap_status = %d, num_tries = %llu, cap_consec_samples = %d, cap_pct = %d", num_seq_increasing, first_increasing, last_num_tries, global_settings->cap_status, avg->num_tries, global_settings->cap_consec_samples, global_settings->cap_pct);

  if (global_settings->cap_status == NS_CAP_CHK_INIT) 
  {
    if (avg->num_tries == 0) 
    {
      printf("No request has been completed in a discovery sample. Recommend that the stabilization test run time is increased\n");
      NSDL3_TESTCASE(NULL, NULL, "No request has been completed in a discovery sample. Recommend that the stabilization test run time is increased");
      global_settings->cap_status = NS_CAP_OVERLOAD;
    }
    else 
    {
      num_seq_increasing = 1;
      first_increasing = last_num_tries = avg->num_tries;
      global_settings->cap_status = NS_CAP_CHK_ON;
      NSDL3_TESTCASE(NULL, NULL, "Changing to NS_CAP_CHK_ON");
    }
  }
  else if (global_settings->cap_status == NS_CAP_CHK_ON) 
  {
    if (is_rampdown_in_progress()) {
      NSDL3_TESTCASE(NULL, NULL, "Rampdown in progress. Not checking capacity");
      return;
    }

    if (avg->num_tries == 0)  
    {
      printf("No request has been completed in a discovery sample. Recommend that the stabilization test run time is increased\n");
      NSDL3_TESTCASE(NULL, NULL, "No request has been completed in a discovery sample. Recommend that the stabilization test run time is increased");
      global_settings->cap_status = NS_CAP_OVERLOAD;
    }
    else 
    {
      if (last_num_tries < avg->num_tries) 
      {
        num_seq_increasing++;
        last_num_tries = avg->num_tries;
      }
      else 
      {
        num_seq_increasing = 1;
        first_increasing = last_num_tries = avg->num_tries;
      }

      if (num_seq_increasing >= global_settings->cap_consec_samples) 
      {
        if ((avg->num_tries - first_increasing) > (first_increasing * global_settings->cap_pct)/100) 
        {
          printf("The Server or bandwidth is at full capacity\n");
          NSDL3_TESTCASE(NULL, NULL, "The Server or bandwidth is at full capacity");
          global_settings->cap_status = NS_CAP_OVERLOAD;
        }
      }
    }
    NSDL3_TESTCASE(NULL, NULL, "num_seq_increasing = %d, last_num_tries = %llu, cap_status = %d", num_seq_increasing, last_num_tries, global_settings->cap_status);
  }
}



char *show_run_type(int type)
{
  if (type == NORMAL_RUN)     return ("Normal Run");
  if (type == FIND_NUM_USERS) return ("Discovery Run");
  if (type == STABILIZE_RUN)  return ("Stabilize Run");
  return ("Invalid run type");
}

static int last_more = LAST_RUN_SYS_HEALTHY, last_less = 0;
static int num_stab_runs = 0, num_stab_succ = 0;

// Increase run_variable to generate more load
static void inc_load()
{
  NSDL3_TESTCASE(NULL, NULL, "Method called.");
  last_less = *run_variable;
  switch (testcase_shr_mem->guess_prob)
  {
    case NS_GUESS_PROB_LOW:
      *run_variable *= 2;
      break;

    case NS_GUESS_PROB_MED:
      *run_variable *= 1.5;
      break;

    case NS_GUESS_PROB_HIGH:
      *run_variable *= 1.1;
      break;
  }

  if (last_less == *run_variable)
    *run_variable = ((*run_variable / SESSION_RATE_MULTIPLE) + 1) * SESSION_RATE_MULTIPLE;
}

// Decrease run_variable to generate less load
static void dec_load()
{
  NSDL3_TESTCASE(NULL, NULL, "Method called.");
  last_more = *run_variable;
  if (last_less)
    *run_variable = (*run_variable + last_less)/2;
  else
  {
    switch (testcase_shr_mem->guess_prob)
    {
      case NS_GUESS_PROB_LOW:
        *run_variable /= 2;
        break;
      case NS_GUESS_PROB_MED:
        *run_variable /= 1.5;
        break;
      case NS_GUESS_PROB_HIGH:
        *run_variable /= 1.1;
        break;
    }
  }

  if (*run_variable == last_more)
    *run_variable = ((*run_variable / SESSION_RATE_MULTIPLE) - 1) * SESSION_RATE_MULTIPLE;
}


/* 
not_healthy_or_overload - 1 if called for not healthy else 0 for overload
last_value - last_sys_overload or last_cap_overload
*/
static int check_on_not_healthy_and_overload(int not_healthy_or_overload, int *last_value)
{
  int result = SLA_RESULT_OK;
  int last_sys_status_to_check;

  NSDL3_TESTCASE(NULL, NULL, "Method called. not_healthy_or_overload = %d, last_value = %d", 
            not_healthy_or_overload, *last_value);
  if(not_healthy_or_overload)
    last_sys_status_to_check = LAST_RUN_SYS_NOT_HEALTHY;
  else
    last_sys_status_to_check = LAST_RUN_SYS_OVER_CAPACITY;

  if (*run_variable <= last_less) 
  {
    result = SLA_RESULT_INCONSISTENT;
    NSDL3_TESTCASE(NULL, NULL, "SLA results are inconsistent as current run_variable (%d) is less than last less load (%d) and system is not healthy or overloaded", *run_variable, last_less);
  }
  else if (*run_variable == (last_less + 1))
  {
    if(not_healthy_or_overload)
      result = SLA_RESULT_CANT_LOAD_ENOUGH;
    else
      result = SLA_RESULT_SERVER_OVERLOAD;
    NSDL3_TESTCASE(NULL, NULL, "SLA results are 'Can not load enough/Server over load' as current run_variable (%d) is one more than last less load (%d) and system is not healthy or overloaded", *run_variable, last_less);
  }
  else if ((last_more == last_sys_status_to_check) && (*run_variable > *last_value)) 
  {
    result = SLA_RESULT_NA;
    NSDL3_TESTCASE(NULL, NULL, "SLA results are inconsistent as current run_variable (%d) is one greater than last less load (%d) and system is not healthy or overloaded", *run_variable, last_less);
  }
  else if (last_more > LAST_RUN_SYS_HEALTHY)
  {
    if (*run_variable  > last_more)
    {
      result = SLA_RESULT_NA;
      NSDL3_TESTCASE(NULL, NULL, "SLA results are NA as current run_variable (%d) is more than last more load (%d) and system is not healthy or overloaded", *run_variable, last_more);
    }
    else
    {
      result = SLA_RESULT_INCONSISTENT;
      NSDL3_TESTCASE(NULL, NULL, "SLA results are inconsistent as current run_variable (%d) is <= last more load (%d) and system is not healthy or overloaded", *run_variable, last_more);
    }
  }
  else 
  {
    last_more = last_sys_status_to_check;

    *last_value = *run_variable;
    *run_variable = (*run_variable + last_less)/2;
    NSDL3_TESTCASE(NULL, NULL, "System Overload or Server Capacity Overload: Going to next discovery round, using %s: %d", global_settings->load_key?"number of users":"user rate", *run_variable);
    fprint3f(stdout, rfp, NULL, "NSController: System Overload or Server Capacity Overload: Going to next discovery round, using %s: %d", global_settings->load_key?"number of users":"user rate", *run_variable);

  }
  return (result);
}

static inline int set_stabilize_run()
{
  run_mode = STABILIZE_RUN;
  num_stab_succ = 1;   /* Our last run was successful, so we can count it as a successful stabiliziation run */
  num_stab_runs = 2;
  
  write_log_file(NS_GOAL_DISCOVERY, "Target Point Discovered. Going to run Next Stabilizations run with %s: %d", global_settings->load_key?"number of users":"user rate", (int)((*run_variable) / SESSION_RATE_MULTIPLE));

  end_stage(NS_GOAL_DISCOVERY, TIS_FINISHED, NULL);

  NSDL3_TESTCASE(NULL, NULL, "NSController: Target Point Discovered. Going to run Next Stabilizations run with %s: %d", global_settings->load_key?"number of users":"user rate", (int)((*run_variable) / SESSION_RATE_MULTIPLE));
  fprint3f(stdout, rfp, NULL, "NSController: Target Point Discovered. Going to run Next Stabilizations run with %s: %d\n", global_settings->load_key?"number of users":"user rate", (int)((*run_variable) / SESSION_RATE_MULTIPLE));

  init_stage(NS_GOAL_STABILIZE);  

  if (num_stab_succ >= testcase_shr_mem->stab_num_success) 
  {
    global_settings->progress_secs = original_progress_msecs;
    global_settings->cap_status = NS_CAP_CHK_OFF;
    run_mode = NORMAL_RUN;
    
    write_log_file(NS_GOAL_STABILIZE, "Stabilization reached. Going to run actual run with %d %s",             
                                       (int)((*run_variable) / SESSION_RATE_MULTIPLE),
                                        global_settings->load_key?"number of users": "user rate");
        
    end_stage(NS_GOAL_STABILIZE, TIS_FINISHED, NULL);

    fprint3f(stdout, rfp, NULL, "NSController: Stabilization reached. Going to run actual run with %d %s\n", 
             (int)((*run_variable) / SESSION_RATE_MULTIPLE), 
             global_settings->load_key?"number of users": "user rate");
    NSDL3_TESTCASE(NULL, NULL, "NSController: Stabilization reached. Going to run actual run with %d %s", 
                   (int)((*run_variable) / SESSION_RATE_MULTIPLE), 
                   global_settings->load_key?"number of users": "user rate");
    return SLA_RESULT_CONTINUE;
  }
  return(SLA_RESULT_OK);
}

static int discovery_run(avgtime *end_results, cavgtime *c_end_results)
{
  static int first_run = 1;
  int result = SLA_RESULT_OK;

  NSDL3_TESTCASE(NULL, NULL, "Method called.");
  if(first_run) 
  {
    first_run = 0;
    write_log_file(NS_GOAL_DISCOVERY, "Starting Discovery for target %s, using %s: %d", ns_target_buf, global_settings->load_key?"number of users":"user rate", *run_variable);
    NSDL3_TESTCASE(NULL, NULL, "NSController: Starting Discovery for target %s, using %s: %d\n", ns_target_buf, global_settings->load_key?"number of users":"user rate", *run_variable);
    return(result);
  }

  if (!test_long_enough(c_end_results)) 
  {
    /* Did not recieved enough metric values for the MEET_SERVER_LOAD, or did not
    * finish at least one session for the MEET_SLA test mode */
    global_settings->test_stab_time *= 2;
    fprintf(stderr, "find_num_users run not long enough.  Going to increase the test run to %d seconds\n", global_settings->test_stab_time);
    NSDL3_TESTCASE(NULL, NULL, "find_num_users run not long enough.  Going to increase the test run to %d seconds\n", global_settings->test_stab_time);
    return(result);
  }

  if (is_sys_healthy(0, end_results) == SYS_NOT_HEALTHY) // Passing 0, since is has been called first time already
    return(check_on_not_healthy_and_overload(1, &last_sys_overload));

  if (global_settings->cap_status == NS_CAP_OVERLOAD) 
    return(check_on_not_healthy_and_overload(0, &last_cap_overload));

  if(state == STATE_NOT_INITIALIZED)
    state = tstate_criteria_met(end_results, testcase_shr_mem->stab_goal_pct, g_cur_finished);
  if (state < 0) 
  {
    if(state == -2)
      if(last_not_met < *run_variable)
        last_not_met = *run_variable;

    state = STATE_NOT_INITIALIZED;
    if  (*run_variable < last_less) {
      NSDL3_TESTCASE(NULL, NULL, "SLA results are inconsistent as current run_variable (%d) is less than last less load (%d) and system is not healthy or overloaded", *run_variable, last_less);

      return(SLA_RESULT_NA);
    }
    if (last_more == LAST_RUN_SYS_HEALTHY) /* Have not got last more yet */
    {
      inc_load();
      NSDL3_TESTCASE(NULL, NULL, "NSController: Going to run next discovery round, using %s: %d\n", global_settings->load_key?"number of users":"user rate", *run_variable);
    }
    else if (last_more == LAST_RUN_SYS_NOT_HEALTHY) 
    {
      if (*run_variable >= last_sys_overload) {
        NSDL3_TESTCASE(NULL, NULL, "SLA results are inconsistent as current run_variable (%d) is greater than or equal to last system overload (%d)", *run_variable, last_sys_overload);

        return(SLA_RESULT_INCONSISTENT);
      } else if (*run_variable == (last_sys_overload - 1)) {
        NSDL3_TESTCASE(NULL, NULL, "SLA results are 'Can not load enough' as current run_variable (%d) is equal to last system overload (%d) - 1", *run_variable, last_sys_overload);
 
        return(SLA_RESULT_CANT_LOAD_ENOUGH);
      } else {
        last_less = *run_variable;
        *run_variable = (last_sys_overload+*run_variable)/2;
        NSDL3_TESTCASE(NULL, NULL, "NSController: Going to run next discovery round, using %s: %d\n", global_settings->load_key?"number of users":"user rate", *run_variable);
      }
    }
    else if (last_more == LAST_RUN_SYS_OVER_CAPACITY) 
    {
      if (*run_variable >= last_cap_overload) {
        NSDL3_TESTCASE(NULL, NULL, "SLA results are inconsistent as current run_variable (%d) is greater than or equal to last capacity overload (%d)", *run_variable, last_cap_overload);
        return(SLA_RESULT_INCONSISTENT);
      } else if (*run_variable == last_cap_overload -1) {
        NSDL3_TESTCASE(NULL, NULL, "SLA results are 'Can not load enough' as current run_variable (%d) is equal to last capacity overload (%d) - 1", *run_variable, last_cap_overload);
        return(SLA_RESULT_SERVER_OVERLOAD);
      } else {
        last_less = *run_variable;
        *run_variable = (last_cap_overload+*run_variable)/2;
        NSDL3_TESTCASE(NULL, NULL, "NSController: Going to run next discovery round, using %s: %d\n", global_settings->load_key?"number of users":"user rate", *run_variable);
      }
    }
    else /* System is ok */
    {
      if (*run_variable >= last_more) {
        NSDL3_TESTCASE(NULL, NULL, "SLA results are inconsistent as current run_variable (%d) is greater than or equal to last more (%d)", *run_variable, last_more);
        return(SLA_RESULT_INCONSISTENT);
      } else if (*run_variable == last_more -1) {
        NSDL3_TESTCASE(NULL, NULL, "SLA results are 'Can not load enough' as current run_variable (%d) is equal to last more (%d) - 1", *run_variable, last_more);
        return(SLA_RESULT_NF);
      } else {
        last_less = *run_variable;

        if ((last_more - *run_variable) <= 
            (testcase_shr_mem->min_steps * SESSION_RATE_MULTIPLE)) // If actual variable is less than min_steps given by user
        {
          write_log_file(get_run_mode_for_initilization(run_mode),"Current load (%d) is less than prev max load (%d) by min steps (%d). Moving to stabilize phase", *run_variable, last_more, testcase_shr_mem->min_steps);
          NSDL3_TESTCASE(NULL, NULL, "NSController: Current load (%d) is less than prev max load (%d) by min steps (%d). Moving to stabilize phase", *run_variable, last_more, testcase_shr_mem->min_steps);
          fprint3f(stdout, rfp, NULL, "NSController: Current load (%d) is less than prev max load (%d) by min steps (%d). Moving to stabilize phase\n", *run_variable, last_more, testcase_shr_mem->min_steps);
          return (set_stabilize_run());
        }
        *run_variable = (last_more+*run_variable)/2;

        NSDL3_TESTCASE(NULL, NULL, "NSController: Going to run next discovery round, using %s: %d\n", global_settings->load_key?"number of users":"user rate", *run_variable);
      }
    }
  }
  else if (state > 0) //criteria more at cur users
  {
    state = STATE_NOT_INITIALIZED;
    if ((last_more > LAST_RUN_SYS_HEALTHY) && (*run_variable > last_more)) {
      NSDL3_TESTCASE(NULL, NULL, "SLA results are inconsistent as current run_variable (%d) is greater than last more (%d)", *run_variable, last_more);
      
      return(SLA_RESULT_INCONSISTENT);
    } else if (*run_variable <= last_less) {
      NSDL3_TESTCASE(NULL, NULL, "SLA results are inconsistent as current run_variable (%d) is less than or equal to last less (%d)", *run_variable, last_less);

      return(SLA_RESULT_INCONSISTENT);
    } else if (*run_variable == (last_less + 1)) {
      NSDL3_TESTCASE(NULL, NULL, "SLA results are 'Can not load enough' as current run_variable (%d) is equal to last less (%d) + 1", *run_variable, last_less);

      return(SLA_RESULT_NF);
    } else {
      dec_load(&last_less, &last_more);
      if(*run_variable <= last_not_met) {
        NSDL3_TESTCASE(NULL, NULL, "SLA results are 'Can not load enough' as current run_variable (%d) is less than or equal to last not met (%d)", *run_variable, last_not_met);

          return(SLA_RESULT_NF); // Cannot meet the given SLAs
      }
      
      NSDL3_TESTCASE(NULL, NULL, "NSController: Going to run next discovery round, using %s: %d\n", global_settings->load_key?"number of users":"user rate", *run_variable);
    }
  }
  else
  {
    state = STATE_NOT_INITIALIZED;
    return (set_stabilize_run());
  }

  return(SLA_RESULT_OK);
}

static int stabilize_run(avgtime *end_results)
{

  NSDL3_TESTCASE(NULL, NULL, "Method called.");
  num_stab_runs++;
  if(state == STATE_NOT_INITIALIZED) /* if state is already set we dont need to check condition again */
    state = tstate_criteria_met(end_results, testcase_shr_mem->stab_goal_pct, g_cur_finished); // Anil - We need this, previously this was not here
  if (state == 0) 
  {
    state = STATE_NOT_INITIALIZED;
    num_stab_succ++;
    NSDL3_TESTCASE(NULL, NULL, "NSController: Stabilization run with %d %s succeeded. Now have total of %d successes\n", *run_variable, global_settings->load_key?"users":"user rate", num_stab_succ );
  } 
  else  // include -2, -1, and +1
  {
    NSDL3_TESTCASE(NULL, NULL, "NSController: Stabilization run with %d %s failed (%s)\n", *run_variable, global_settings->load_key?"users":"user rate", (state>0)?"More":"Less");

    if ((testcase_shr_mem->stab_max_run - num_stab_runs) < (testcase_shr_mem->stab_num_success - num_stab_succ)) 
    {
          // Added by Anuj
      num_stab_succ = num_stab_runs = 0;
      testcase_shr_mem->guess_prob = NS_GUESS_PROB_HIGH;
          
      //last_not_met = -1; last_met = 0;
      //last_more = -1; last_less = 0;
      last_more = LAST_RUN_SYS_HEALTHY; last_less = 0;
      run_mode = FIND_NUM_USERS;
      NSDL3_TESTCASE(NULL, NULL, "Returning from STABILIZE_RUN, the (stab_max_run'%d' - num_stab_runs'%d') < (stab_num_success'%d' - num_stab_succ'%d')", testcase_shr_mem->stab_max_run, num_stab_runs, testcase_shr_mem->stab_num_success, num_stab_succ);
      return SLA_RESULT_CONTINUE;
    }
    state = STATE_NOT_INITIALIZED;
  }
  if (num_stab_succ >= testcase_shr_mem->stab_num_success) 
  {
    global_settings->progress_secs = original_progress_msecs;
    global_settings->cap_status = NS_CAP_CHK_OFF;
    run_mode = NORMAL_RUN;
    write_log_file(NS_GOAL_STABILIZE, "NSController: Stabilization reached. Going to run actual run with %d %s", 
                                       (int)((*run_variable) / SESSION_RATE_MULTIPLE), 
                                       global_settings->load_key?"number of users": "user rate");
    end_stage(NS_GOAL_STABILIZE, TIS_FINISHED, NULL);
    fprint3f(stdout, rfp, NULL, "NSController: Stabilization reached. Going to run actual run with %d %s\n", 
             (int)((*run_variable) / SESSION_RATE_MULTIPLE), 
             global_settings->load_key?"number of users": "user rate");
    NSDL3_TESTCASE(NULL, NULL, "NSController: Stabilization reached. Going to run actual run with %d %s\n", 
                   (int)((*run_variable) / SESSION_RATE_MULTIPLE), 
                   global_settings->load_key?"number of users": "user rate");
    return SLA_RESULT_CONTINUE;
  }
  NSDL3_TESTCASE(NULL, NULL, "NSController: Going to run stabiliziation run #%d with %s: %d\n", num_stab_runs, global_settings->load_key?"number of users":"user rate", *run_variable);
  return(SLA_RESULT_OK);
}

int run_num = 0; // Getting set in this mthd only
// This will return 1 , if to do continue in the outer while loop, else returns 0
int check_proc_run_mode(avgtime *end_results , cavgtime *c_end_results)
{
  int result = SLA_RESULT_OK;
  if (run_mode == NORMAL_RUN)
  { 
    NSDL3_TESTCASE(NULL, NULL, "Method called. run_type is '%s', run_num = %d, run_length = %d, end_results = %p", show_run_type(run_mode), run_num, run_length, end_results);
  }
  else
  {
    NSDL3_TESTCASE(NULL, NULL, "Method called. run_type is '%s', run_num = %d, state = %d, num_stab_runs = %d, num_stab_succ = %d, stab_max_run = %d, stab_num_success = %d, last_more = %d, last_less = %d, last_sys_overload = %d, last_cap_overload = %d, run_time = %d, run_variable = %d, test sample run = %d, load_key = %d, progress_secs %d, cap_status = %d", show_run_type(run_mode), run_num, state, num_stab_runs, num_stab_succ, testcase_shr_mem->stab_max_run, testcase_shr_mem->stab_num_success, last_more, last_less, last_sys_overload, last_cap_overload, run_length, *run_variable, global_settings->test_stab_time, global_settings->load_key, global_settings->progress_secs, global_settings->cap_status);
  }

  if (run_num == ULTIMATE_MAX_NUM_STAB_RUNS) 
  {
    NS_EXIT(0, "Have run too many stabilization runs, exiting...");
  }

  switch (run_mode) 
  {
    case FIND_NUM_USERS:
      result = discovery_run(end_results, c_end_results);
      break;

    case STABILIZE_RUN:
      // Anil - We are not doing capicity_chk and health_monitor and test_long_enough chk here, may be we need these in future
      result = stabilize_run(end_results);
      break;

    case NORMAL_RUN:
      // run_variable is not set in case of non SLA.
      // NSDL3_TESTCASE(vptr, cptr, "Going to run a regular test run with %d (users/ user rate) for run_length %d", *run_variable, run_length);
      NSDL3_TESTCASE(NULL, NULL, "Going to run a regular test run for run_length %d", run_length);
      global_settings->test_stab_time = run_length;
      result = SLA_RESULT_LAST_RUN;
      break;
  }

  if (result != SLA_RESULT_CONTINUE) {
    run_num++;
    if (run_variable != NULL)
    {
      if((run_mode == FIND_NUM_USERS) || (run_mode == STABILIZE_RUN))
        write_log_file(get_run_mode_for_initilization(run_mode),"Test Run is in '%s' phase. Session rate per min = %d. Test sample number is '                        %d'",show_run_type(run_mode), (int)((*run_variable) / SESSION_RATE_MULTIPLE), run_num);
      fprint3f(stdout, rfp, NULL, "\nTest Run is in '%s' phase. Session rate per min = %d. Test sample number is '%d'\n", 
               show_run_type(run_mode), (int)((*run_variable) / SESSION_RATE_MULTIPLE), run_num);
    }
  }

  return result;
}

