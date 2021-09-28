/********************************************************************************
 * File Name            : ni_user_distribution.c
 * Author(s)            : Manpreet Kaur
 * Date                 : 21 March 2012
 * Copyright            : (c) Cavisson Systems
 * Purpose              : Contains function to distribute user among scenarios
 * Modification History : <Author(s)>, <Date>, <Change Description/Location>
 ********************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <regex.h>
#include <sys/types.h>
#include <errno.h>
#include <malloc.h>
#include <time.h>

#include "ni_user_distribution.h"
#include "ni_scenario_distribution.h"
#include "../../../ns_exit.h"
#include "../../../ns_error_msg.h"
#include "../../../ns_kw_usage.h"
#include "../../../ns_log.h"
#include "../../../ns_global_settings.h"
#include "../../../ns_alloc.h"

//List of generator name
gen_name_quantity_list* gen_list;
gen_name_quantity_list* gen_num_list = NULL;
gen_qty_as_per_capacity *gen_qty;

//List of quantity per generator
grp_name_and_qty *grp_qty_list;

int divide_usr_wrt_generator_capacity(int rnum, gen_capacity_per_gen* gen_cap)
{
  int i, total_cvms = 0, gen_idx;
  int total_quantity = 0, remaning_quantity = 0, per_gen_avail = 0, per_gen_qty = 0;
  /*Rounding errors in floating point on internal conversion of numbers in binary format when typecast to integer
     TODO: Tried same code in standalone was working fine, but when running with tool rounding errors are reported
     For eample pct = 514800, number_of_generators = 2 
                for_all = (int)pct / number_of_generators
              Result: for_all = 257399 it should be 257400
     Hence solution, save pct value in local string and then atoi same in local integer*/

  NIDL(2, "Method called, number_of_generators = %d, pct = %lf", scen_grp_entry[rnum].num_generator, scen_grp_entry[rnum].pct_value);

  NSLB_MALLOC_AND_MEMSET(gen_qty, (sizeof(int) * scen_grp_entry[rnum].num_generator), "gen_qty_as_per_capacity", -1, NULL); 
  //Calculate total cvms per group   
  for(i = 0; i < scen_grp_entry[rnum].num_generator; i++) {
    gen_idx = scen_grp_entry[rnum].generator_id_list[i];
    total_cvms += generator_entry[gen_idx].num_cvms; 
  }
  NIDL(4, "Total_cvms = %d", total_cvms);

  //Calculate per generator quantity as per generator capacity 
  for(i = 0; i < scen_grp_entry[rnum].num_generator; i++) {
    gen_idx =  scen_grp_entry[rnum].generator_id_list[i];
  
    //Find per generator quantity on per_gen_qty variable 
    per_gen_qty = (generator_entry[gen_idx].num_cvms * (int)scen_grp_entry[rnum].pct_value) / total_cvms;
    per_gen_avail = gen_cap[gen_idx].cap_per_gen - gen_cap[gen_idx].per_gen_quantity_distributed;
    NIDL(4, "per_gen_qty = %d, per_gen_avail = %d", per_gen_qty, per_gen_avail);
  
    if(per_gen_avail > 0) {
      if(per_gen_avail > per_gen_qty) 
        gen_qty[i].qty_per_gen_cap = per_gen_qty; 
      else
        gen_qty[i].qty_per_gen_cap = per_gen_avail;
    }

    gen_cap[gen_idx].per_gen_quantity_distributed += gen_qty[i].qty_per_gen_cap;
    total_quantity +=  gen_qty[i].qty_per_gen_cap;
  
    NIDL(4, "gen_qty[%d].qty_per_gen_cap = %d, num_cvms = %d, gen_cap[%d].per_gen_quantity_distributed = %d", 
             i, gen_qty[i].qty_per_gen_cap, generator_entry[i].num_cvms, i, gen_cap[i].per_gen_quantity_distributed);
  }

  //Find remaning quantity
  remaning_quantity = (int)scen_grp_entry[rnum].pct_value - total_quantity;
  per_gen_qty = remaning_quantity / scen_grp_entry[rnum].num_generator;
  NIDL (2, "Remaning_quantity = %d, Total_quantity = %d, pct_gen_qty = %d", remaning_quantity, total_quantity, per_gen_qty);
  if(!per_gen_qty) 
    per_gen_qty = 1;
  while(remaning_quantity > 0) {
    for(i = 0; i < scen_grp_entry[rnum].num_generator && remaning_quantity > 0; i++) {
      gen_idx =  scen_grp_entry[rnum].generator_id_list[i];
      if(gen_qty[i].qty_per_gen_cap != 0) {
        if(gen_cap[gen_idx].cap_per_gen != gen_cap[gen_idx].per_gen_quantity_distributed || scen_grp_entry[rnum].num_generator == 1) {
          gen_qty[i].qty_per_gen_cap += per_gen_qty;
          gen_cap[gen_idx].per_gen_quantity_distributed += per_gen_qty;
          remaning_quantity -= per_gen_qty;
        }
      }
    }
    per_gen_qty = 1;
  }
  NIDL (2, "Remaning_quantity = %d, pct_gen_qty = %d", remaning_quantity, per_gen_qty);

#ifdef NS_DEBUG_ON
  for (i = 0; i < scen_grp_entry[rnum].num_generator; i++) {
    gen_idx =  scen_grp_entry[rnum].generator_id_list[i];
    NIDL (2,"gen_qty[%d].qty_per_gen_cap = %d, gen_cap[%d].per_gen_quantity_distributed = %d",
             i, gen_qty[i].qty_per_gen_cap, gen_idx, gen_cap[gen_idx].per_gen_quantity_distributed);
  }
#endif

  //Filled data in scenario grp entry
  for (i = 0; i < scen_grp_entry[rnum].num_generator; i++) {
    gen_idx =  scen_grp_entry[rnum].generator_id_list[i];
    if (scen_grp_entry[rnum].grp_type == TC_FIX_USER_RATE) {
      /* Update percentage in case of FSR instead of quantity, becoz double var was required
       * to store session rate*/
      //scen_grp_entry[rnum + i].percentage = (double)gen_cap[gen_idx].per_gen_quantity_distributed;
      scen_grp_entry[rnum + i].percentage = (double)gen_qty[i].qty_per_gen_cap;
      NIDL(2, "scen_grp_entry[%d].percentage = %0.2f", rnum + i, scen_grp_entry[rnum].percentage); 
      /*Populate vuser_rpm with total session rate (sum all groups)*/
      vuser_rpm += scen_grp_entry[rnum + i].percentage;
      NIDL(4, "vuser_rpm = %f", vuser_rpm);
    } else { 
      //scen_grp_entry[rnum + i].quantity = gen_cap[gen_idx].per_gen_quantity_distributed;
      scen_grp_entry[rnum + i].quantity = gen_qty[i].qty_per_gen_cap;
      /*Populate num_connections with total SGRP quantity*/
      num_connections += scen_grp_entry[rnum + i].quantity;
      NIDL(2, "scen_grp_entry[%d].quantity = %d", rnum + i, scen_grp_entry[rnum + i].quantity);
    }
  }

  FREE_AND_MAKE_NULL_EX(gen_qty, sizeof(gen_qty_as_per_capacity) * scen_grp_entry[rnum].num_generator, "gen_qty_as_per_capacity", -1);
  return 0;
}

/******************************************************************************************* 
 * Description		: Function is used to divide users with respect to number of 
 *                        generator assosiated in a group.(Distribute in round-robin fashion) 
 *                        Called from create_table_for_sgrp_keyword()when PROF_PCT_MODE 
 *                        is NUM
 * Input-Parameters	: 
 * number_of_generators : Number of generator per group
 * pct			: Quantity to divide
 * gen_list		: structure used to fill distributed quantity
 * Output-Parameters	: Update gen_name_quantity_list with quantity
 * Return 		: None
 *******************************************************************************************/
int divide_usr_wrt_generator(int number_of_generators, double pct, gen_name_quantity_list* gen_list, char *buf, char *err_msg)
{
  int for_all, left_over;
  int i, j;
  /*Rounding errors in floating point on internal conversion of numbers in binary format when typecast to integer
     TODO: Tried same code in standalone was working fine, but when running with tool rounding errors are reported
     For eample pct = 514800, number_of_generators = 2 
                for_all = (int)pct / number_of_generators
              Result: for_all = 257399 it should be 257400
     Hence solution, save pct value in local string and then atoi same in local integer*/
  char local_string[100];
  sprintf(local_string, "%lf", pct);
  int pct_value = atoi(local_string);   
  
  NIDL(2, "Method called, number_of_generators = %d, pct = %lf", number_of_generators, pct);
  if (pct < number_of_generators)
  {
    NIDL(2, "pct = %d, number_of_generators = %d", (int)pct, number_of_generators);
    if (buf[0] != '\0'){
      NS_KW_PARSING_ERR(buf, 0, err_msg, SGRP_USAGE, CAV_ERR_1011346, (int)pct, number_of_generators);
    }else{
      NS_EXIT(-1, CAV_ERR_1014008, (int)pct, number_of_generators);  
    }
  
    //NS_EXIT(-1, "Users (%d) are unavailable to run test on generators (%d) used in a group",
      //                 (int)pct, number_of_generators);
  } 
  //Calculate quantity to distribute to each generator and remainder
  for_all = pct_value / number_of_generators;
  left_over = pct_value % number_of_generators;
  NIDL (2, "for_all = %d, left_over = %d", for_all, left_over);

  for (i = 0; i < number_of_generators; i++) {
    gen_list[i].qty_per_gen = for_all;
    NIDL (2, "gen_list[%d].qty_per_gen = %d", i, gen_list[i].qty_per_gen);
  }
  for ( j = 0; j < left_over; j++) {
    gen_list[j].qty_per_gen ++;
    NIDL(3, "gen_list[%d].qty_per_gen = %d", j, gen_list[j].qty_per_gen);
  }
  return 0;
}

/*****************************************************************************************************
 * Description:    This function divides the fcs concurrent limit generator (Distribute in round-robin)
 *
 * Input:          total_generators: Total number of generators 
 *                 sess_limit      : Concurrent sess Limit
 *                 gen_list        : Generator list
 * Return:         None
******************************************************************************************************/

void divide_limit_wrt_generators(int total_generators, int sess_limit, int max_pool_size)
{
  int for_all, left_over;
  int i, j, k = 0;
  int estimated_pool_size = 0;
  
  if(sess_limit < total_generators)
  {
    NIDL(2, "sess_limit = %d, total_generators = %d", sess_limit, total_generators);
    NS_EXIT(-1, "Limit of concurrent session (%d) should be more than total number of"
                  " generators (%d) used in a group", sess_limit, total_generators);
  }

  /* Adding new strcuture and filling per quantity */
  //per_gen_fcs_table = (perGenFCSTable *)malloc(total_generators * sizeof(perGenFCSTable));
  //memset(per_gen_fcs_table, 0 , total_generators * sizeof(perGenFCSTable));
  NSLB_MALLOC_AND_MEMSET(per_gen_fcs_table, (total_generators * sizeof(perGenFCSTable)), "per gen fcs table", -1, NULL);
  /*Total SGRP groups*/
  while(k < total_sgrp_entries) {
    for (j = 0; j < total_generators; j++) {
      if (!strcmp(scen_grp_entry[k].generator_name, (char *)generator_entry[j].gen_name)) {
        per_gen_fcs_table[j].quantity += scen_grp_entry[k].quantity;
        NIDL(4, "Adding %d users on per_gen_fcs_table[%d] = %d for generator %s", scen_grp_entry[k].quantity, j,
                 per_gen_fcs_table[j].quantity, generator_entry[j].gen_name);
        break;
      }
    }
    k++;
  }

  for_all = sess_limit / total_generators;
  left_over = sess_limit % total_generators;
  NIDL(2, "for_all = %d, left_over = %d", for_all, left_over);

  for (i = 0; i < total_generators; i++) {
    per_gen_fcs_table[i].session_limit = for_all;
    NIDL(2, "per_gen_fcs_table[%d].session_limit = %d", i, per_gen_fcs_table[i].session_limit);
  }
  for ( j = 0; j < left_over; j++) {
    per_gen_fcs_table[j].session_limit++;
    NIDL(3, "per_gen_fcs_table[%d].session_limit = %d", j, per_gen_fcs_table[j].session_limit);
    //NIDL(3, "per_gen_fcs_table[%d].session_limit = %lf", j, per_gen_fcs_table[j].session_limit);
  }
  for(i = 0; i < total_generators; i++)
  {
    /*dividing pool size on every generator*/
    per_gen_fcs_table[i].pool_size = (per_gen_fcs_table[i].quantity - per_gen_fcs_table[i].session_limit);
    if (estimated_pool_size < per_gen_fcs_table[i].pool_size)
      estimated_pool_size = per_gen_fcs_table[i].pool_size; 
    NIDL(3, "per_gen_fcs_table[%d].pool_size = %d", i, per_gen_fcs_table[i].pool_size);
  }
  NIDL(3, "estimated_pool_size = %d", estimated_pool_size);
  
  if (max_pool_size)
  {
    if (estimated_pool_size > max_pool_size)
    {
      //Validation: Checking that total generator pool size should be less than max_pool_size given
      NS_EXIT(-1, "Concurrent sessions pool size (%d) should be more than estimated sessions pool size (%d)"
                 " for running this test, please increase concurrent session pool size", max_pool_size, estimated_pool_size);
    }
  }
}

/*******************************************************************************
 *      FUNCTIONS FOR DIVIDING PERCENTAGE AMONG SGRP WITH RESPECT TO GENERATORS
 * *****************************************************************************/
/* Function to balance array*/
static void balance_array_(double *array, int len, int total_qty)
{
  int i;
  int sum = 0;
  int left = 0;
  double fraction, max_fraction;
  int max_idx = 0;

  NIDL(1, "Method called");

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

int pct_division_among_gen_per_grp(gen_name_quantity_list* gen_list, int sum, int total_gen_entries, int id, char *buf, char *err_msg)
{
  double *qty_array;
  int i;
  double verify_sum_pct = 0.0;

  NIDL(2, "Method called, total_gen_entries = %d, sum = %d", total_gen_entries, sum);

  for( i = 0; i < total_gen_entries; i++)
  {
    NIDL(2, "grp_qty_list[%d].pct_per_gen = %lf", i, gen_list[i].pct_per_gen );
    verify_sum_pct += gen_list[i].pct_per_gen;
  }

  //Verify sum of given ratio, sum of pcts must be equal to 100
  verify_sum_pct = verify_sum_pct / 100.0;
  NIDL(2, "Method called, verify_sum_pct = %lf", verify_sum_pct);

  if (verify_sum_pct != 100)
  {
    if (buf[0] != '\0'){
      NS_KW_PARSING_ERR(buf, 0, err_msg, SGRP_USAGE, CAV_ERR_1011330, scen_grp_entry[i].generator_name);
    }else{
      NS_EXIT(-1, CAV_ERR_1014009, scen_grp_entry[i].generator_name);
    }
    //NS_EXIT(-1, "Scenario group %s, generator percentage should add up to total 100 to run Netcloud test",
      //                               scen_grp_entry[i].generator_name);
  }
  //qty_array = (double *)malloc(sizeof(double) * total_gen_entries);
  NSLB_MALLOC(qty_array, (sizeof(double) * total_gen_entries), "quantity array", -1, NULL);

  for (i = 0; i < total_gen_entries; i++) {
    qty_array[i] = (double)((double)sum * (double)((gen_list[i].pct_per_gen / 100.0) / 100.0));
  }

  for (i = 0; i < total_gen_entries; i++) {
    NIDL(3, "i = %d, qty_array[i] = %lf", i, qty_array[i]);
  }

  balance_array_(qty_array, total_gen_entries, sum);

  for (i = 0; i < total_gen_entries; i++) {
    NIDL(3, "i = %d, qty_array[i] = %lf", i, qty_array[i]);
  }

  //Copy balanced array in quantity list
  for (i = 0; i < total_gen_entries; i++) {
    gen_list[i].qty_per_gen = (int)qty_array[i];
    NIDL(3, "gen_list[%d].qty_per_gen = %d", i, gen_list[i].qty_per_gen);
  }

  //Free and make null malloced qty_array
  free(qty_array);
  qty_array = NULL;
  return 0;
}
/********************************************************************************** 
 * Description		: Function used to divide users with respect to given ratio
 *                        Tasks:
 *                        a) Verify sum of given ratio, sum of pcts must be 
 *                           equal to 100
 *                        b) Fill qty_array with pct entries
 *                        c) Call balance_array funct to divide 
 *                           quantity equally among groups
 *                        d) Copy pct into grp_qty_list struct
 * Input-Parameter	: 
 * grp_qty_list			: To get group name and quantity list
 * total_quantity		: Total number of quantity
 * total_unique_grp_entries	: Number of unique group entries
 * Output-Parameter	: None
 * Return		: None
 ***********************************************************************************/

static void pct_division_among_grps(grp_name_and_qty *grp_qty_list, int total_quantity, int total_unique_grp_entries)
{
  double *qty_array;
  int i;
  double verify_sum_pct = 0.0;

  NIDL(2, "Method called, total_unique_grp_entries = %d", total_unique_grp_entries);

  //qty_array = (double *)malloc(sizeof(double) * total_unique_grp_entries);
  NSLB_MALLOC(qty_array, (sizeof(double) * total_unique_grp_entries), "quantity array for unique grp entries", -1, NULL);

  for( i = 0; i < total_unique_grp_entries; i++)
  {
    NIDL(2, "grp_qty_list[%d].pct_per_grp = %lf", i, grp_qty_list[i].pct_per_grp);
    verify_sum_pct += grp_qty_list[i].pct_per_grp;
  }
  //Verify sum of given ratio, sum of pcts must be equal to 100
  verify_sum_pct = verify_sum_pct / 100.0;
  NIDL(2, "Method called, verify_sum_pct = %lf", verify_sum_pct);

  if (verify_sum_pct != 100)
  {
    NS_EXIT(-1, CAV_ERR_1011300, "Scenario group percentage should add up to total 100 to run NetCloud test.");
  }

  for (i = 0; i < total_unique_grp_entries; i++) {
    qty_array[i] = (double)((double)total_quantity * (double)((grp_qty_list[i].pct_per_grp / 100.0) / 100.0));
  }

  for (i = 0; i < total_unique_grp_entries; i++) {
    NIDL(3, "i = %d, qty_array[i] = %lf", i, qty_array[i]);
  }

  balance_array_(qty_array, total_unique_grp_entries, total_quantity);

  for (i = 0; i < total_unique_grp_entries; i++) {
    NIDL(3, "i = %d, qty_array[i] = %lf", i, qty_array[i]);
  }
  //Copy balanced array in quantity list
  for (i = 0; i < total_unique_grp_entries; i++) {
    grp_qty_list[i].pct_per_grp = qty_array[i];
    NIDL(3, "grp_qty_list[%d].pct_per_grp = %f", i, grp_qty_list[i].pct_per_grp);
  }
  //Free and make null malloced qty_array
  free(qty_array);
  qty_array = NULL;
}

/* Function used to update ScenGrpEntry entries as per number of generators */
static void update_sgrp_idx(int idx, int no_generator, ScenGrpEntry *scen_grp_entry)
{
  int i, count = 0;

  NIDL(2, "Method called, idx = %d, no_generator = %d", idx, no_generator);
  for ( i = 0; i < no_generator; i++)
  {
    if (scen_grp_entry[idx + count].grp_type == TC_FIX_USER_RATE) {
      /* Update percentage in case of FSR instead of quantity, becoz double var was required
       * to store session rate, and divide qty by session multiplier */
      scen_grp_entry[idx + count].percentage = (double)gen_num_list[i].qty_per_gen;
      NIDL(3, "idx = %d, count = %d, scen_grp_entry[idx + count].percentage = %f", idx, count, scen_grp_entry[idx + count].percentage);
      }
    else {
      scen_grp_entry[idx + count].quantity = gen_num_list[i].qty_per_gen;
      NIDL(3, "scen_grp_entry[idx + i].quantity = %d", scen_grp_entry[idx + i].quantity);
    }
    count ++;
    if (count >= no_generator) {
      NIDL(3, "count = %d", count);
      break;
    }
  }
}

static void fill_pct_per_gen(int id, gen_name_quantity_list* gen_list)
{
  int i;
  NIDL(1, "Method called, num generators = %d for group id = %d", scen_grp_entry[id].num_generator, id);
  for(i = 0; i < scen_grp_entry[id].num_generator; i++)
  {
    gen_list[i].pct_per_gen = scen_grp_entry[id].gen_pct_array[i];
    NIDL(4, "gen_list[%d].pct_per_gen = %f", i, gen_list[i].pct_per_gen);
  }
}
/********************************************************************************
 * Description		: Distribute percentage among sgrp groups,
 *                        Tasks:
 *                        a) Sort unique group entries, and store respective pct 
 *                           in grp_name_and_qty struct
 *                        b) pct_division_among_grps() Divide pct among unique 
 *                           group entries
 *                        c) Distribute users among groups wrt to generator
 *                        Function called from main funct of tool
 * Input-Parameter	: 
 * scen_grp_entry	: Pointer to ScenGrpEntry table
 * total_quantity	: Total number of quantity
 * Output-Parameter	: None
 * Return		: None
 *******************************************************************************/

void distribute_pct_among_grps(ScenGrpEntry *scen_grp_entry, int total_quantity)
{
  int i, j, save_init_grp = 0; //Need to save inital group index in case of pct distribution  
  int total_unique_grp_entries = 0;
  char buf[1024];
  char err_msg[1024];
  NIDL(2, "Method called, total_quantity = %d, total_sgrp_entries = %d", total_quantity, total_sgrp_entries);

  //Struct used to store group name and corresponding pct
  //Size to malloc struct = total number of SGRP entries * sizeof(grp_name_and_qty)
  //grp_qty_list = (grp_name_and_qty *)malloc(total_sgrp_entries * sizeof(grp_name_and_qty));
  NSLB_MALLOC(grp_qty_list, (total_sgrp_entries * sizeof(grp_name_and_qty)), "grp qty list", -1, NULL);

  for (i = 0, j = 0; i < total_sgrp_entries; i++)
  {
    if (i == 0) //First scenario data is copied
    {
      grp_qty_list[j].group_name = scen_grp_entry[i].scen_group_name;
      grp_qty_list[j].pct_per_grp = scen_grp_entry[i].percentage;
      NIDL(2, "grp_qty_list[%d].group_name = %s, grp_qty_list[%d].pct_per_grp = %lf", j, 
               grp_qty_list[j].group_name, j, grp_qty_list[j].pct_per_grp);
      total_unique_grp_entries ++;
      NIDL(2, "total_unique_grp_entries = %d", total_unique_grp_entries);
    } 
    else {
      if (!strcmp(scen_grp_entry[i].scen_group_name, scen_grp_entry[i - 1].scen_group_name)) {
        NIDL(2, "scen_grp_entry[%d].scen_group_name = %s", i, scen_grp_entry[i].scen_group_name);
      }
      else {
        j++; 
        grp_qty_list[j].group_name = scen_grp_entry[i].scen_group_name;
        grp_qty_list[j].pct_per_grp = scen_grp_entry[i].percentage;
        NIDL(2, "grp_qty_list[%d].group_name = %s, grp_qty_list[%d].pct_per_grp = %lf", j, 
               grp_qty_list[j].group_name, j, grp_qty_list[j].pct_per_grp);
        total_unique_grp_entries ++;
        NIDL(2, "total_unique_grp_entries = %d", total_unique_grp_entries);
      }
    }
  }
  //Divide pct
  pct_division_among_grps (grp_qty_list, total_quantity, total_unique_grp_entries);
  
  for (i = 0, j = 0; i < total_sgrp_entries; i++)
  {
    if(i == 0) {
     save_init_grp = i;
     NIDL(2, "Need to save initiating group index, save_init_grp = %d", save_init_grp);
    }
    /* Groups with generator name NA or having single generator name value, does not need user distribution */
    if ((!strcmp(scen_grp_entry[i].generator_name, "NA"))||
               ((strcmp(scen_grp_entry[i].generator_name, "NA") != 0) && 
                               (scen_grp_entry[i].num_generator == 1))) {
      if (!strcmp(scen_grp_entry[i].scen_group_name, grp_qty_list[j].group_name))
      {  
        if (scen_grp_entry[i].grp_type == TC_FIX_USER_RATE)
        /* Update percentage in case of FSR instead of quantity, becoz double var was required
         * to store session rate, and divide qty by session multiplier */
          scen_grp_entry[i].percentage = (double)grp_qty_list[j].pct_per_grp;
        else 
          scen_grp_entry[i].quantity = grp_qty_list[j].pct_per_grp;
      }
      //Save initial group number and increment index for grp_qty_list entry
      save_init_grp = i + 1;
      j++;
      if (scen_grp_entry[i].pct_flag_set)
      {
         if((scen_grp_entry[i].gen_pct_array[0]/100.0) != 100.0)
         {
           NS_EXIT(-1, CAV_ERR_1011330, scen_grp_entry[i].generator_name);
         } 
      }
    } /* Whereas groups with generator name list need to be divide as per generator entries */
    else if (scen_grp_entry[i].num_generator > 1) {
      if ((i < total_sgrp_entries) && !strcmp(scen_grp_entry[i].scen_group_name, scen_grp_entry[i + 1].scen_group_name))
      {
        NIDL(2, "Index = %d, scen_grp_entry[i].scen_group_name = %s, scen_grp_entry[i + 1].scen_group_name = %s", i, scen_grp_entry[i].scen_group_name, scen_grp_entry[i + 1].scen_group_name);
        continue;
      }
      // Malloc gen_name_quantity_list array
      //gen_num_list = (gen_name_quantity_list*)malloc( scen_grp_entry[save_init_grp].num_generator * sizeof(gen_name_quantity_list)); 
      NSLB_MALLOC(gen_num_list, (scen_grp_entry[save_init_grp].num_generator * sizeof(gen_name_quantity_list)), "gen_name_quantity_list array", -1, NULL);

      if (!strcmp(scen_grp_entry[save_init_grp].scen_group_name, grp_qty_list[j].group_name)) {
        //Divide user with respect to generator
        if (scen_grp_entry[save_init_grp].pct_flag_set == 0) 
           divide_usr_wrt_generator(scen_grp_entry[save_init_grp].num_generator, (double)grp_qty_list[j].pct_per_grp, gen_num_list, buf, err_msg);
        else {
          fill_pct_per_gen(save_init_grp, gen_num_list);
          pct_division_among_gen_per_grp(gen_num_list, (double)grp_qty_list[j].pct_per_grp, scen_grp_entry[save_init_grp].num_generator, save_init_grp, buf, err_msg);
        }
        //Updating runProfEntry table entries
        update_sgrp_idx(save_init_grp, scen_grp_entry[save_init_grp].num_generator, scen_grp_entry);
      }
      //Save initial group number and increment index for grp_qty_list entry
      save_init_grp = i + 1;
      j++;
    }
  }
  //For Debug
  for (i = 0; i < total_sgrp_entries; i++)
  {
    if (scen_grp_entry[i].grp_type == TC_FIX_USER_RATE) {
      NIDL(2, "group_name = %s, scen_grp_entry[%d].percentage = %f", scen_grp_entry[i].scen_group_name, 
             i, scen_grp_entry[i].percentage);
    } else {
      NIDL(2, "group_name = %s, scen_grp_entry[%d].quantity = %d", scen_grp_entry[i].scen_group_name, 
                i, scen_grp_entry[i].quantity);
    }
  }  
  //Free and make NULL malloced pointers gen_num_list, grp_qty_list
  NIDL(4, "Free and make NULL malloced pointers grp_qty_list = %p", grp_qty_list);
  free(grp_qty_list);
  grp_qty_list = NULL;
  if (gen_num_list !=NULL) {
    NIDL(4, "Free and make NULL malloced pointers gen_num_list = %p", gen_num_list);
    free(gen_num_list);
    gen_num_list = NULL;  
  }
}
