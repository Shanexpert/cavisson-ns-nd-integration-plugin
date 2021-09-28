/********************************************************************
* Name: ns_goal_based_sla.c  
* Purpose: File containing fuctions for SLA type goal based scenarios 
* Author: Anuj
* Intial version date: 14/05/08
* Last modification date
********************************************************************/


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
#include "ns_trans_parse.h"
#include "ns_gdf.h"
#include "ns_log.h"
#include "ns_goal_based_sla.h"
#include "ns_alloc.h"
#include "ns_exit.h"

// Moved from netstorm.c, these were not used there hence commented
//#define MEET_SLA_HIGH_PCT 0
//#define MEET_SLA_LOW_PCT 5

//extern void end_test_run( void );
SLATableEntry *slaTable;
SLATableEntry_Shr *sla_table_shr_mem = NULL;

int max_sla_entries = 0; //moved from util.c
// unsigned long long sla_stats[SLA_NUM_OBJ_TYPE][SLA_NUM_OBJ_PROP];

// Following are defined as extern in util.h
int total_sla_entries; //moved from util.c

/*****************************************************************/

// START - Utililty Functions used withing this file

static int create_sla_table_entry(int *row_num) 
{
  NSDL3_TESTCASE(NULL, NULL, "Method called, total_sla_entries = %d, max_sla_entries = %d", total_sla_entries, max_sla_entries);
  if (total_sla_entries == max_sla_entries) 
  {
    MY_REALLOC (slaTable, (max_sla_entries + DELTA_SLA_ENTRIES) * sizeof(SLATableEntry), "slaTable entries", -1);
    max_sla_entries += DELTA_SLA_ENTRIES;
  }
  *row_num = total_sla_entries++;
  slaTable[*row_num].user_id       = -1;
  slaTable[*row_num].vector_option = -1;
  slaTable[*row_num].vector_name[0]= '\0';
  slaTable[*row_num].relation      = -1;
//  slaTable[*row_num].object_type = -1;
//  slaTable[*row_num].object_id = -1;
//  slaTable[*row_num].object_prop = -1;
  NSDL3_TESTCASE(NULL, NULL, "Method Ended sucessfully, total_sla_entries = %d, max_sla_entries = %d, row_num = %d", total_sla_entries, max_sla_entries, *row_num);
  return (SUCCESS);
}

#if 0
static void set_sla_stats_with_avg(int sla_obj_type, avgtime *avg)
{
  //NSDL3_TESTCASE(vptr, cptr, "Method called.");
  sla_stats[sla_obj_type][SLA_AVG] = avg->avg_time;
  sla_stats[sla_obj_type][SLA_MEAN] = avg->avg_time;
  sla_stats[sla_obj_type][SLA_MIN] = avg->min_time;
  sla_stats[sla_obj_type][SLA_MAX] = avg->max_time;
}
#endif

static void kw_sla_format(char *err)
{
  fprintf(stderr, "%s\n", err);
  fprintf(stderr, "Format of keyword is:\n");
  fprintf(stderr, "SLA userid gdf_rpt_gp_id gdf_rpt_graph_id vector_option vector_name relation value");
  exit(-1);
}

// END - Utililty Functions used withing this file

/*****************************************************************/

// Start: Functions which are called from other source files of netstorm.

// SLA ALL gdf_rpt_gp_id gdf_rpt_graph_id vector_option vector_name relation value
// char vector_option = NA (Default), will be pond defined
void kw_set_sla(char *buf)
{
  NSDL3_TESTCASE(NULL, NULL, "Method called.");
  int num, rnum;
  char keyword[MAX_DATA_LINE_LENGTH];
  char userid[MAX_DATA_LINE_LENGTH];
  int gdf_rpt_gp_id, gdf_rpt_graph_id; 
  char vector_option[MAX_DATA_LINE_LENGTH];
  char vector_name[MAX_DATA_LINE_LENGTH];
  char relation[MAX_DATA_LINE_LENGTH];
  double value;
  double pct_variation;

  if ((num =sscanf(buf, "%s %s %d %d %s %s %s %lf %lf", keyword, userid, &gdf_rpt_gp_id, &gdf_rpt_graph_id, vector_option, vector_name, relation, &value, &pct_variation)) != 9) 
    kw_sla_format("kw_set_sla(): Need EIGHT fields after keyword SLA\n");

  if (create_sla_table_entry(&rnum) != SUCCESS) 
  {
    fprintf(stderr, "kw_set_sla(): Failed to create no SLA Table entry\n");
    exit(-1);
  }

  if (!strcmp(userid, "ALL"))
    slaTable[rnum].user_id = SLA_ALL;
  else 
  {
    fprintf(stderr, "kw_set_sla(): Unknown userid <%s> in SLA entry\n", userid);
    exit(-1);
  }

  if (gdf_rpt_gp_id > 0)
    slaTable[rnum].gdf_rpt_gp_id = gdf_rpt_gp_id;
  else
  {
    fprintf(stderr, "Error: Invalid Report group id given with SLA keyword\n"); 
    exit(-1);
  }

  if (gdf_rpt_gp_id > 0)
    slaTable[rnum].gdf_rpt_graph_id = gdf_rpt_graph_id;
  else
  {
    fprintf(stderr, "Error: Invalid Report graph id given with SLA keyword\n"); 
    exit(-1);
  }

  if (!strcmp(vector_option, "NA"))
    slaTable[rnum].vector_option = SLA_VECTOR_OPTION_NA;
  else if (!strcmp(vector_option, "All"))
    slaTable[rnum].vector_option = SLA_VECTOR_OPTION_ALL; // Not implenmented yet
  else if (!strcmp(vector_option, "OverAll"))
    slaTable[rnum].vector_option = SLA_VECTOR_OPTION_OVERALL;
  else if (!strcmp(vector_option, "Specified"))
    slaTable[rnum].vector_option = SLA_VECTOR_OPTION_SPECIFIED;
  else 
  {
    fprintf(stderr, "kw_set_sla(): Unknown Vector option <%s> is given with SLA keyword\n", vector_option);
    exit(-1);
  }

  if ( slaTable[rnum].vector_option != SLA_VECTOR_OPTION_SPECIFIED)
  {
    if (strcmp(vector_name, "NA")) // NA not given
    {
      fprintf(stderr, "Error: kw_set_sla() - Can not give the vector name with vector_option <%s> in SLA keyword\n", vector_option);
      exit(-1);
    }
  }
  // How do we chk that given is valid or not, i.e user taking server_stats of 192...101 in the scenario and here he has given 192...102 :Anuj
  strcpy(slaTable[rnum].vector_name, vector_name);
  
  if (!strcmp(relation, "LESS_THAN"))
    slaTable[rnum].relation = SLA_RELATION_LESS_THAN;
  else if (!strcmp(relation, "GREATER_THAN"))
    slaTable[rnum].relation = SLA_RELATION_GR_THAN;
  else if (!strcmp(relation, "EQUAL_TO"))
    slaTable[rnum].relation = SLA_RELATION_EQUAL;
  else
  {
    fprintf(stderr, "Error: Unknown relation type <%s> has been given with SLA keyword, relation can be either 'LESS_THAN' or 'GREATER_THAN'\n", relation);
    exit(-1);
  }

  if (value < 0) 
  {
    fprintf(stderr, "Error: Value must be greater than or equal to 0 with SLA keyword\n");
    exit(-1);
  }
  slaTable[rnum].value = value;

  if (pct_variation < 0) 
  {
    fprintf(stderr, "Error: Percentage Variation must be greater than or equal to 0 with SLA keyword\n");
    exit(-1);
  }
  slaTable[rnum].pct_variation = pct_variation;
}

// called form init_userinfo() util.c
void alloc_mem_for_sla_table()
{
  NSDL3_TESTCASE(NULL, NULL, "Method called.");
  total_sla_entries = 0;  

  MY_MALLOC (slaTable, INIT_SLA_ENTRIES * sizeof(SLATableEntry), "slaTable", -1);
  max_sla_entries = INIT_SLA_ENTRIES;
}

void *copy_slaTable_to_shr(void *prof_table_shr_mem)
{
  int i;
  int gdf_rpt_gp_num_idx = -1, gdf_rpt_graph_num_idx = -1;

  NSDL3_TESTCASE(NULL, NULL, "Method called. total_sla_entries = %d", total_sla_entries);

  sla_table_shr_mem = prof_table_shr_mem;

  for (i = 0; (i < total_sla_entries) && (total_groups_entries != 0); i++) 
  {
    if( get_gdf_group_graph_info_idx (slaTable[i].gdf_rpt_gp_id, slaTable[i].gdf_rpt_graph_id, &gdf_rpt_gp_num_idx, &gdf_rpt_graph_num_idx) == -1)
    {
      NS_EXIT(-1, "Error: There is no group/graph found for the group id '%d' and graph id '%d' given with SLA keyword", slaTable[i].gdf_rpt_gp_id, slaTable[i].gdf_rpt_graph_id);
    }

    sla_table_shr_mem[i].user_id               = slaTable[i].user_id;
    sla_table_shr_mem[i].gdf_rpt_gp_num_idx    = gdf_rpt_gp_num_idx; 
    sla_table_shr_mem[i].gdf_rpt_graph_num_idx = gdf_rpt_graph_num_idx; 
    sla_table_shr_mem[i].vector_option         = slaTable[i].vector_option;
    sla_table_shr_mem[i].gdf_group_num_vectors = 1;
    sla_table_shr_mem[i].gdf_graph_num_vectors = 1;
             
    if (slaTable[i].vector_option == SLA_VECTOR_OPTION_ALL)
      if (get_gdf_vector_num (gdf_rpt_gp_num_idx, gdf_rpt_graph_num_idx, (int *)(&sla_table_shr_mem[i].gdf_group_num_vectors),
                              (int *)(&sla_table_shr_mem[i].gdf_graph_num_vectors)) == -1)
      {
        NS_EXIT(-1, "Error: Invalid vector option has been specified with SLA keyword.");
      }

    if (slaTable[i].vector_option == SLA_VECTOR_OPTION_SPECIFIED)
      if (get_gdf_vector_idx (gdf_rpt_gp_num_idx, gdf_rpt_graph_num_idx, slaTable[i].vector_name, 
                              (int *)(&sla_table_shr_mem[i].gdf_group_vector_idx) , 
                              (int *)(&sla_table_shr_mem[i].gdf_graph_vector_idx)) == -1)
      {
        NS_EXIT(-1, "Error: Invalid vector name '%s' has been specified with SLA keyword.", slaTable[i].vector_name);
      }

    sla_table_shr_mem[i].relation              = slaTable[i].relation;
    sla_table_shr_mem[i].value                 = slaTable[i].value;
    sla_table_shr_mem[i].pct_variation         = slaTable[i].pct_variation;
    NSDL3_TESTCASE(NULL, NULL,
              "Components of Shared memory in SLA[%d] - user_id = %d, gdf_rpt_gp_num_idx = %d, gdf_rpt_graph_num_idx = %d, vector_option = %d, gdf_group_vector_idx = %d, gdf_graph_vector_idx = %d, relation = %d, value = %d, pct_variation = %d gdf_group_num_vectors = %d, gdf_graph_num_vectors = %d\n", 
              i, sla_table_shr_mem[i].user_id, sla_table_shr_mem[i].gdf_rpt_gp_num_idx, 
              sla_table_shr_mem[i].gdf_rpt_graph_num_idx, sla_table_shr_mem[i].vector_option, 
              sla_table_shr_mem[i].gdf_group_vector_idx, sla_table_shr_mem[i].gdf_graph_vector_idx, 
              sla_table_shr_mem[i].relation, sla_table_shr_mem[i].value, 
              sla_table_shr_mem[i].pct_variation,
              sla_table_shr_mem[i].gdf_group_num_vectors,
              sla_table_shr_mem[i].gdf_graph_num_vectors);
  }
  prof_table_shr_mem += WORD_ALIGNED(sizeof(SLATableEntry_Shr) * total_sla_entries);
  return prof_table_shr_mem;
}

void print_slaTable()
{
  NSDL3_TESTCASE(NULL, NULL, "Method called.");
  int i;
  NSDL2_MISC(NULL, NULL, "SLA Table\n");
  for (i = 0; i < total_sla_entries; i++)
    NSDL2_MISC(NULL, NULL, "index: %d\t userid: %d\t gdf_rpt_gp_id: %d\t gdf_rpt_graph_id: %d\t vector_option: %d\t vector_name: %s\t relation: %d\t value: %f\t pct_variation: %f\n", i, slaTable[i].user_id, slaTable[i].gdf_rpt_gp_id, slaTable[i].gdf_rpt_graph_id, slaTable[i].vector_option, slaTable[i].vector_name, slaTable[i].relation, slaTable[i].value, slaTable[i].pct_variation);
  NSDL2_MISC(NULL, NULL, "\n");
}

#if 0
// Called form deliver_report.c
void fill_sla_stats(avgtime *avg)
{
  NSDL3_TESTCASE(vptr, cptr, "Method called.");
  int i;
  int eighty_set = 0;
  int ninety_set = 0;
  int ninetynine_set = 0;
  int running_total = 0;
  int total_fail;

  set_sla_stats_with_avg (SLA_URL, avg);
  if (avg->url_succ_fetches) 
  {
    for (i = 0; i <= global_settings->max_granules; i++) 
    {
      running_total += avg->response[i];
      if ((( running_total * 100 / avg->url_succ_fetches) >= 80) && !eighty_set) 
      {
        sla_stats[SLA_URL][SLA_80] = i * global_settings->granule_size;
        eighty_set = 1;
      }
      if ((( running_total * 100 / avg->url_succ_fetches) >= 90) && !ninety_set) 
      {
        sla_stats[SLA_URL][SLA_90] = i * global_settings->granule_size;
        ninety_set = 1;
      }
      if ((( running_total * 100 / avg->url_succ_fetches) >= 99) && !ninetynine_set) 
      {
        sla_stats[SLA_URL][SLA_99] = i * global_settings->granule_size;
        ninetynine_set = 1;
      }
      if (running_total >= avg->url_succ_fetches)
        break;
    }
  }

  sla_stats[SLA_URL][SLA_FAILURE] = avg->url_fetches_completed - avg->url_succ_fetches;

  eighty_set = 0;
  ninety_set = 0;
  ninetynine_set = 0;
  running_total = 0;

  set_sla_stats_with_avg (SLA_PAGE, avg);
  if (avg->pg_succ_fetches) 
  {
    for (i = 0; i <= global_settings->pg_max_granules; i++) 
    {
      running_total += avg->pg_response[i];
      if ((( running_total * 100 / avg->pg_succ_fetches) >= 80) && !eighty_set) 
      {
        sla_stats[SLA_PAGE][SLA_80] = i * global_settings->pg_granule_size;
        eighty_set = 1;
      }
      if ((( running_total * 100 / avg->pg_succ_fetches) >= 90) && !ninety_set) 
      {
        sla_stats[SLA_PAGE][SLA_90] = i * global_settings->pg_granule_size;
        ninety_set = 1;
      }
      if ((( running_total * 100 / avg->pg_succ_fetches) >= 99) && !ninetynine_set) 
      {
        sla_stats[SLA_PAGE][SLA_99] = i * global_settings->pg_granule_size;
        ninetynine_set = 1;
      }
      if (running_total >= avg->pg_succ_fetches)
        break;
    }
  }
  sla_stats[SLA_PAGE][SLA_FAILURE] = avg->pg_fetches_completed - avg->pg_succ_fetches;

  eighty_set = 0;
  ninety_set = 0;
  ninetynine_set = 0;
  running_total = 0;

  set_sla_stats_with_avg (SLA_SESS_W_WAIT, avg);
  if (avg->sess_succ_fetches) 
  {
    for (i = 0; i <= global_settings->sess_max_granules; i++) 
    {
      running_total += avg->sess_response[i];
      if ((( running_total * 100 / avg->sess_succ_fetches) >= 80) && !eighty_set) 
      {
        sla_stats[SLA_SESS_W_WAIT][SLA_80] = i * global_settings->sess_granule_size;
        eighty_set = 1;
      }
      if ((( running_total * 100 / avg->sess_succ_fetches) >= 90) && !ninety_set) 
      {
        sla_stats[SLA_SESS_W_WAIT][SLA_90] = i * global_settings->sess_granule_size;
        ninety_set = 1;
      }
      if ((( running_total * 100 / avg->sess_succ_fetches) >= 99) && !ninetynine_set) 
      {
        sla_stats[SLA_SESS_W_WAIT][SLA_99] = i * global_settings->sess_granule_size;
        ninetynine_set = 1;
      }
      if (running_total >= avg->sess_succ_fetches)
        break;
    }
  }
  total_fail = 0;
  for (i = 1; i < TOTAL_PAGE_ERR; i++) 
  {
    total_fail += (avg->cum_sess_error_codes[i]);
  }
  sla_stats[SLA_SESS_W_WAIT][SLA_FAILURE] = total_fail;

  eighty_set = 0;
  ninety_set = 0;
  ninetynine_set = 0;
  running_total = 0;

  set_sla_stats_with_avg (SLA_TX_W_WAIT, avg);
  if (avg->tx_c_succ_fetches) 
  {
    for (i = 0; i <= global_settings->tx_max_granules; i++) 
    {
      running_total += avg->tx_response[i];
      if ((( running_total * 100 / avg->tx_c_succ_fetches) >= 80) && !eighty_set) 
      {
        sla_stats[SLA_TX_W_WAIT][SLA_80] = i * global_settings->tx_granule_size;
        eighty_set = 1;
      }
      if ((( running_total * 100 / avg->tx_c_succ_fetches) >= 90) && !ninety_set) 
      {
        sla_stats[SLA_TX_W_WAIT][SLA_90] = i * global_settings->tx_granule_size;
        ninety_set = 1;
      }
      if ((( running_total * 100 / avg->tx_c_succ_fetches) >= 99) && !ninetynine_set) 
      {
        sla_stats[SLA_TX_W_WAIT][SLA_99] = i * global_settings->tx_granule_size;
        ninetynine_set = 1;
        ninetynine_set = 1;
      }
      if (running_total >= avg->tx_c_succ_fetches)
        break;
    }
  }
  sla_stats[SLA_TX_W_WAIT][SLA_FAILURE] = avg->tx_c_fetches_completed - avg->tx_c_succ_fetches;
}
#endif

// return 1 if to do continue, else 0 on success and -1 on failure
int kw_set_guess(char *buf)
{
  NSDL3_TESTCASE(NULL, NULL, "Method called.");
  char keyword[MAX_DATA_LINE_LENGTH];
  int num;
  int guess_num;
  int min_steps;
  char guess_prob;
  char type[100];

  if ((num = sscanf(buf, "%s %s %d %c %d", keyword, type, &guess_num, &guess_prob, &min_steps)) != 5) 
  {
    fprintf(stderr,"Eroor: Need FOUR fields after keyword GUESS\n");
    exit(-1);
  }

  if ((testCase.mode == TC_FIX_CONCURRENT_USERS)) 
  {
      NSDL3_MISC(NULL, NULL, "Warning: GUESS entry is ignored for this test case type.");
      //continue;
      return 1;
  }

  if (!strcmp(type, "NUM_USERS"))
    testCase.guess_type = GUESS_NUM_USERS;
  else if (!strcmp(type, "RATE"))
    testCase.guess_type = GUESS_RATE;
  else 
  {
    fprintf(stderr, "Error: Unknown guess type. Valid values are NUM_USERS or RATE\n");
    exit(-1);
  }

  testCase.guess_num = guess_num;

  switch(guess_prob) 
  {
    case 'L':
      testCase.guess_prob = NS_GUESS_PROB_LOW;
      break;
    case 'M':
      testCase.guess_prob = NS_GUESS_PROB_MED;
      break;
    case 'H':
      testCase.guess_prob = NS_GUESS_PROB_HIGH;
      break;
    default:
      printf("Invalid value for GUESS prob. Valid values are 'L|M|H'. Setting it to 'L'\n");
      testCase.guess_prob = NS_GUESS_PROB_LOW;
  }

  if (min_steps <= 0)
  {
    fprintf(stderr, "Invalid value on Min Steps with GUESS keyword, it must be a value greater than 0\n");
    exit(-1);
  }
  else
    testCase.min_steps = min_steps;

  return 0;
}

void validate_guess_type()
{
  NSDL3_TESTCASE(NULL, NULL, "Method called.");
  if ((global_settings->load_key == 0) && (testCase.guess_type != GUESS_RATE)) 
  {
    NS_EXIT(1, "GUESS type must be of type RATE");
  }

  if ((global_settings->load_key == 1) && (testCase.guess_type != GUESS_NUM_USERS)) 
  {
    printf("GUESS type must be of type NUM_USERS\n");
    NS_EXIT(1, "GUESS type must be of type NUM_USERS");
  }

  if (testCase.guess_num <= 0) 
  {
    printf("GUESS number must be >0\n. Setting it to %d", DEFAULT_GUESS_NUM);
    testCase.guess_num = DEFAULT_GUESS_NUM;

  }
}

void validate_stabilize()
{
// Commented for testing only
#if 0
  if (testCase.stab_run_time < DEFAULT_STAB_RUN_TIME) {
    printf("STABILIZE run time should be at least %d sec.  Setting it to %d sec.\n", DEFAULT_STAB_RUN_TIME, DEFAULT_STAB_RUN_TIME);
    testCase.stab_run_time = DEFAULT_STAB_RUN_TIME;
  }
#endif

  if ((testCase.stab_num_success < 0) ||
      (testCase.stab_num_success > testCase.stab_max_run) ||
      (testCase.stab_run_time < 0) ||
      (testCase.stab_goal_pct < 0) ||
      (testCase.stab_goal_pct > 100) ||
      (testCase.stab_delay < 0))
  {
    NSTL1(NULL, NULL, "Invalid values for STABALIZE. Setting to default values\n");
    testCase.stab_num_success = DEFAULT_STAB_NUM_SUCCESS;
    testCase.stab_max_run     = DEFAULT_STAB_MAX_RUN;
    testCase.stab_run_time    = DEFAULT_STAB_RUN_TIME;
    testCase.stab_goal_pct    = DEFAULT_STAB_GOAL_PCT;
    testCase.stab_delay       = DEFAULT_STAB_DELAY;
  }
}

/* 
STABILIZE stab_suc_run stab_max_run run_time pct stab_delay
*/
void kw_set_stablize(char *buf)
{
  NSDL3_TESTCASE(NULL, NULL, "Method called.");
  char keyword[MAX_DATA_LINE_LENGTH];
  unsigned int stab_delay = DEFAULT_STAB_DELAY;
  int num_success = DEFAULT_STAB_NUM_SUCCESS, max_runs = DEFAULT_STAB_MAX_RUN, time = DEFAULT_STAB_RUN_TIME;
  int pct = DEFAULT_STAB_GOAL_PCT;

  /* All missing fields in the end will take default value. 
   * pct field is not used by MEET_SLA types.
   */
  // for specifying the value of stab_delay, user has to specify all the fields prior to this in keyword SATBILIZE
  sscanf(buf, "%s %d %d %d %d %u", keyword, &num_success, &max_runs, &time, &pct, &stab_delay);

  testCase.stab_num_success  = num_success;
  testCase.stab_max_run      = max_runs;
  testCase.stab_run_time     = time;
  testCase.stab_goal_pct     = pct;
  testCase.stab_delay        = stab_delay; // will be used in ns_parent.c for putting the delay betwwen 2 testruns
  //  testCase.stab_sessions = sessions;

// For testing only
#if 0
  if (time < DEFAULT_STAB_RUN_TIME)
  {
    printf("STABILIZE run time should be at least %d sec.  Setting it to %d sec.\n", DEFAULT_STAB_RUN_TIME, DEFAULT_STAB_RUN_TIME);
    testCase.stab_run_time = DEFAULT_STAB_RUN_TIME;
  }
#endif
}

void validate_capacity_check()
{
  if ((global_settings->cap_mode < 0) || (global_settings->cap_mode > 4)) 
  {
    NSTL1(NULL, NULL, "CAPACITY_CHECK mode must be between 0-4. Setting it to 0\n");
    global_settings->cap_mode = 0;
  }
  if (global_settings->cap_mode == 4)
    global_settings->cap_status = NS_CAP_CHK_OFF;
  else
    global_settings->cap_status = NS_CAP_CHK_INIT;

  if (global_settings->cap_status == NS_CAP_CHK_INIT) 
  {
    if (global_settings->cap_consec_samples <= 1) 
    {
      NSTL1(NULL, NULL, "CAPACITY_CHECK consec. samples must be >1. Setting consec. samples to %d\n", DEFAULT_CAP_CONSEC_SAMPLES);
      NSTL1(NULL, NULL, "Setting pct to %d\n", DEFAULT_CAP_PCT);
      global_settings->cap_consec_samples = DEFAULT_CAP_CONSEC_SAMPLES;
      global_settings->cap_pct = DEFAULT_CAP_PCT;
    }
    if (global_settings->cap_pct <= 0) 
    {
      NSTL1(NULL, NULL, "CAPACITY_CHECK pct must be >0. Setting consec. samples to %d\n", DEFAULT_CAP_CONSEC_SAMPLES);
      NSTL1(NULL, NULL, "Setting pct to %d\n", DEFAULT_CAP_PCT);
      global_settings->cap_consec_samples = DEFAULT_CAP_CONSEC_SAMPLES;
      global_settings->cap_pct = DEFAULT_CAP_PCT;
    }
  }
}
void kw_set_capacity_check(char *buf)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  int num;
  int cap_mode, consec_samples, pct;
  
  NSDL3_TESTCASE(NULL, NULL, "Method called");
  
  if ((num = sscanf(buf, "%s %d %d %d", keyword, &cap_mode, &consec_samples, &pct)) != 4) 
  {
    fprintf(stderr, "read_keywords(): Need THREE fields after key CAPACITY_CHECK\n");
    exit(-1);
  }
  
  global_settings->cap_mode = cap_mode;
  global_settings->cap_consec_samples = consec_samples;
  global_settings->cap_pct = pct;

  if (global_settings->cap_consec_samples <= 0) 
  {
    fprintf(stderr, "read_keywords(): capacity check consec samples must be greater than zero\n");
    exit(-1);
  }

  if (global_settings->cap_pct <= 0) 
  {
    fprintf(stderr, "read_keywords(): capacity pct must be greater than zero\n");
    exit(-1);
  }
}

// End: Functions which are called from other source files of netstorm.

// End of File
