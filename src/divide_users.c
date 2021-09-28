/**************************************************************************************
Divide users (for Fix Concurrent users) or 
Session PCT for all other cases over number of NVM's (netstorm processes)

3 modes for division
NVM_DISTRIBUTION 0|1|2
D 0 means max performance. i
	Auto distribution 
	A SG may be distributed over 1 or more NVM's
D 1 means max isolation among scripts
	Auto distribution 
	All SG using same script will be excuted only by one NVM
D 2 user based (user specifies which SG falls in which NVM).

FIX CONCURRENT USERS (LOAD KEY 0)
1)Find the number of procs 
	<= NUM_PRCOCESSES keyword
	<= # of users/seesion-rate
	<= # of fetches
	>= 1
	For D1: <= # scripts
	For D2 = User defined (overrides NUM PROCESSES, error if < # fetches or if <= session-rate)
2)Create temp per_script_totals 
	Add sessptr in var grouptable	
	create in mem temp struct { sessptr, sgroupid, pct/numusers, script-total) Done
3)per process runprof Table is shm and just has pct/number of users
	For D0:  
		Divide sequentially all SG's (for concurrent users) 
		All SG's get same pct for all procs Done (for session rate)
	For D1:  
		sort per_script_totals in order (script-total, sessptr)
		Arrange all SG's so as to add next group with least number of users
	For D2: nvm_id is part of SG table 

For Load Key 1 only
4)per process runprof table : calc session rate (or IID) 
	individual proc session rate = my pct for all sg's / all sg pct for all procs
5)per process runprof table : session pct upgrade to 100% 
	new  pct for sg  = sgroup pct * 1000000000 / my pct for all sg
	//effective rounding it to make it a total of 100%
	
Anil Kumar 09/08/05
**************************************************************************************/
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
#include "util.h"
#include "timing.h"
#include "tmr.h"

#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "ns_log.h"
#include "divide_users.h"
#include "divide_values.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "ns_alloc.h"
#include "ns_schedule_phases_parse.h"
#include "ns_percentile.h"
#include "wait_forever.h"
#include "netomni/src/core/ni_user_distribution.h"
#include "netomni/src/core/ni_scenario_distribution.h"

#include "ns_schedule_divide_over_nvm.h"

#include "ns_schedule_fcs.h"
#include "ns_exit.h"
#include "ns_trace_level.h"

#include "ns_error_msg.h"

#ifndef CAV_MAIN
int *per_proc_runprof_table;
#else
__thread int *per_proc_runprof_table;
#endif
int *per_proc_per_grp_fetches;
int *per_grp_sess_inst_num;

#define CHECK_NUM_PROCESS  \
    if(global_settings->num_process > 254) {                                                     \
      NS_EXIT(-1, "Error: NetStorm can not have more than 254 NVM." \
                      " Current NVM is %d. Exitting ...", global_settings->num_process);       \
    }                                                                   \

static int 
comp_by_stotal_script (const void *e1, const void *e2)
{
	NSDL2_SCHEDULE(NULL, NULL, "Method called");
	if (((PerScriptTotal *)e1)->script_total > (((PerScriptTotal *)e2)->script_total))
	    return 1;
	else if (((PerScriptTotal *)e1)->script_total < ((PerScriptTotal *)e2)->script_total)
	    return -1;
	else if (((PerScriptTotal *)e1)->script_total == ((PerScriptTotal *)e2)->script_total) {
	    if (((PerScriptTotal *)e1)->sess_name > ((PerScriptTotal *)e2)->sess_name)
	        return 1;
	    else if (((PerScriptTotal *)e1)->sess_name < ((PerScriptTotal *)e2)->sess_name)
	        return -1;
	    else 
	        return 0;
	}
	return 0;
}

PerScriptTotal *create_per_script_total()
{
PerScriptTotal * totals;
int i,j;

	NSDL2_SCHEDULE(NULL, NULL, "Method called. total_runprof_entries=%d", total_runprof_entries);
	MY_MALLOC (totals , (sizeof (PerScriptTotal)) * total_runprof_entries, "per script totals ", -1);
	
	for (i = 0; i < total_runprof_entries; i++) {
	    totals[i].sgroup_num = runprof_table_shr_mem[i].group_num;
	    totals[i].pct_or_users = runprof_table_shr_mem[i].quantity;
	    totals[i].sess_name = runprof_table_shr_mem[i].sess_ptr->sess_name;
	    totals[i].script_total = 0;
	    //Set the script_total to sum of all pct for this sess_name
	    for (j = 0; j < total_runprof_entries; j++) {
		if (totals[i].sess_name == runprof_table_shr_mem[j].sess_ptr->sess_name)
	    	    totals[i].script_total += runprof_table_shr_mem[j].quantity;
	    }
	}
	return totals;
}

static int
get_num_proc (int conf_num_proc, int num_user_or_pct)
{
int num_proc;

	NSDL2_SCHEDULE(NULL, NULL, "Method called. conf_num_proc = %d, num_user_or_pct = %d, total_clust_entries=%d", conf_num_proc, num_user_or_pct, total_clust_entries);
        
        if(global_settings->replay_mode)
           return conf_num_proc;

        if(loader_opcode == MASTER_LOADER) {
	   NSDL2_SCHEDULE(NULL, NULL, "MASTER_LOADER: returning total_client_entries = %d", total_client_entries);
           //return total_client_entries;
           /*This need to be number of generators*/
           return sgrp_used_genrator_entries;
        }

	if (global_settings->nvm_distribution == 2) //User configured
	    num_proc = total_clust_entries;		
	else
	    num_proc = conf_num_proc;		

	if (num_proc < 1)
	    num_proc = 1;
	if (num_proc > num_user_or_pct)
        {
          NS_DUMP_WARNING("Number of NVMs '%d' are more than number of Vuser '%d' so setting NVM's value equal to number of Vusers.",
                          num_proc, num_user_or_pct);
	  num_proc = num_user_or_pct;
        }
#if 0
/*
        BugID: 112091
        Note:  Number of nvm process is based on VUser or Session Rate not on number of Sessions.
               Hence, commenting this code.
*/
	if ((global_settings->num_fetches) && (num_proc > global_settings->num_fetches))
        {
          NS_DUMP_WARNING("Number of NVMs '%d' are more than number of sessions '%d' so setting NVM's value equal to number of sessions.", 
                           num_proc, num_user_or_pct);
	  num_proc = global_settings->num_fetches;
        }
#endif
	if ((global_settings->nvm_distribution == 1) && (num_proc > total_sess_entries))
        {
          NS_DUMP_WARNING("Number of NVMs '%d' are more than number of distinct scripts '%d' " 
                          "so setting its value equal to number of distinct scripts.", num_proc, total_sess_entries);
	  num_proc = total_sess_entries;
        }
        if ((global_settings->concurrent_session_mode) && (num_proc > global_settings->concurrent_session_limit))
        {
          NS_DUMP_WARNING("Number of NVMs '%d' are more than limit of concurrent sessions '%d' so setting its value equal to number of limit concurrent sessions.", num_proc, global_settings->concurrent_session_limit);
          num_proc = global_settings->concurrent_session_limit;
        }
	// Bug 69032 - Limit Max NVM creation
        if (global_settings->per_proc_min_user_session)
        {
           int loc_num_proc = num_user_or_pct/global_settings->per_proc_min_user_session;

           if(!loc_num_proc)
             loc_num_proc = 1;//We need atleast 1 NVM

           if(num_proc > loc_num_proc)
             num_proc = loc_num_proc;
        }
        //TODO: Ayush 
	return num_proc;	
}

static int
find_proc_with_least_users_or_rate()
{
int i, least;

	least = 0;
	for (i = 1; i < global_settings->num_process; i++) {
	    if (v_port_table[least].num_vusers > v_port_table[i].num_vusers)
		least = i;
	}
	NSDL2_SCHEDULE(NULL, NULL, "Returning nvm id %d", least);
	return least;
}

//This is on the lines of distribute_values() in divide_values.c
//only for NUM user case. TBD for RATE case
static void
distribute_fetches ()
{
int i, min=1, num_val, num_users, used_val = 0, leftover;
int total_vals = global_settings->num_fetches;
int total_users = 0;
Phases *ph_ptr;

    NSDL2_SCHEDULE(NULL, NULL, "Method called");
    for (i = 0; i < global_settings->num_process; i++)
    	total_users += v_port_table[i].num_vusers;

    /*Bug 49725: Per user fetches scenario*/
    ph_ptr = &(scenario_schedule->phase_array[3]);
    if(ph_ptr->phase_cmd.duration_phase.per_user_fetches_flag)
      total_vals *= total_users;

    for (i = 0; i < global_settings->num_process; i++) {
    	num_users = v_port_table[i].num_vusers;
	if (!num_users) continue;
	min = num_users;
	num_val = (total_vals * num_users)/total_users;
		/*if (global_settings->load_key)
		    num_val = (total_vals * num_users)/total_users;
		else {
		    num_val = (total_vals * num_users)/(total_users * 100000); //all pct are mapped to 10 M
		    printf("num_users=%d last_pct =%d num_val=%d total_users=%d\n", num_users, last_pct, num_val, total_users);
		}*/
	//	printf("num_users=%d num_val=%d total_users=%d\n", num_users, num_val, total_users);
	if (num_val < min) num_val = min;
    	v_port_table[i].num_fetches = num_val;
	used_val += num_val;
    }

    NSDL2_SCHEDULE(NULL, NULL, "total_vals = %d, used_val = %d", total_vals, used_val);
    leftover = total_vals - used_val;
    if(ph_ptr->phase_cmd.duration_phase.per_user_fetches_flag)
      leftover = 0;
    if (leftover < 0 ) {
        NS_EXIT(1, CAV_ERR_1019001, total_vals, total_users);
    }
    i = 0;
    while (leftover) {
    	v_port_table[i].num_fetches++;
	leftover--;
    	i++;
    	if (i == global_settings->num_process) i = 0;
    }

    for (i = 0; i < global_settings->num_process; i++)
      NSDL3_SCHEDULE(NULL, NULL, "NVM=%d  fetches=%d\n", i, v_port_table[i].num_fetches);
}

//Calculates & store cummulative count of runprof table for generating random group
static void compute_runprof_table_cummulative(Schedule *schedule, int proc_index)
{
  int *my_runprof_table;  //Pointer to per_proc_runprof_table for proc_index
  int grp_idx;

  NSDL2_SCHEDULE(NULL, NULL, "Cumulative runprof table for NVM=%d", proc_index);

  my_runprof_table = &per_proc_runprof_table[proc_index * total_runprof_entries];

  for (grp_idx = 0; grp_idx < total_runprof_entries; grp_idx++)   /* Creating cumulative count for scenario 
                                                 * group used for randomly selecting group 
                                                 * based on their weight .*/
  {
    NSDL2_SCHEDULE(NULL, NULL, "cumulative runprof value=%d", schedule->cumulative_runprof_table[grp_idx]);
    if (grp_idx == 0)
      schedule->cumulative_runprof_table[grp_idx] = my_runprof_table[grp_idx];
    else 
      schedule->cumulative_runprof_table[grp_idx] = schedule->cumulative_runprof_table[grp_idx - 1] + my_runprof_table[grp_idx];          

    NSDL3_SCHEDULE(NULL, NULL, "Cumulative runprof table: Group id=%d, per_proc_runprof_table[%d]=%d, cumulative runprof value=%d", 
                     grp_idx, grp_idx, my_runprof_table[grp_idx], schedule->cumulative_runprof_table[grp_idx]);
  }
}

static void validation_fetches_versus_user()
{
  Schedule *schedule_ptr;
  Phases *ph_ptr;
  int num_fetches;
  int num_users;
  int i;

  NSDL2_SCHEDULE(NULL, NULL, "Method called, total_runprof_entries = %d", total_runprof_entries);

  for (i = 0 ; i < total_runprof_entries; i++) 
  {
    NSDL2_SCHEDULE(NULL, NULL, "Group Id = %d, num_fetches = %d, quantity = %d", 
                    i, group_schedule[i].phase_array[3].phase_cmd.duration_phase.num_fetches, runprof_table_shr_mem[i].quantity);

    schedule_ptr = &(group_schedule[i]);

    ph_ptr = &(schedule_ptr->phase_array[3]);  //4th phase is DURATION Phase
    num_fetches = ph_ptr->phase_cmd.duration_phase.num_fetches;
    num_users = runprof_table_shr_mem[i].quantity;
    
    /* -----------------------------------------------------------------------
      Abhay: Resolved bugid 49228 in Release: 4.1.14#8
      Adding below check of num_fetches > 0
      RCA  - When there are more than one SGRP groups and at least two run in different DURATION mode 
             i.e one run in TIME mode and other one run in SESSION mode then num_fetches value will be 0 for group running in TIME mode
             and validation check (num_fetches < num_users) kills the test. 
      Resolution - so we need to check if num_fetches is greater than 0 then only we should proceed 
                   for validation check (num_fetches < num_users) 
      ----------------------------------------------------------------------*/
    if(num_fetches > 0)
    {
      if(ph_ptr->phase_cmd.duration_phase.per_user_fetches_flag)
      {
        num_fetches = num_fetches * num_users; 
      }
    
      if(num_fetches < num_users)
      {
        NS_EXIT(1, CAV_ERR_1019002,
          runprof_table_shr_mem[i].scen_group_name, 
          v_port_table->group_schedule[i].phase_array[3].phase_cmd.duration_phase.num_fetches, 
          runprof_table_shr_mem[i].quantity);
      }
    }
  }
}

static void distribute_fetchs_over_nvms_and_grps(PerScriptTotal * psTable)
{
  int i,j;
  int for_all, leftover;
  int cur_proc;
  //char* last_sess_name = NULL;
  //int last=0;
  int tot_distributed_fetches;

  Schedule *schedule_ptr;
  Phases *ph_ptr;
  int num_fetches;
  int num_users;

  NSDL2_SCHEDULE(NULL, NULL, "Method called, psTable = %p", psTable);

  //Vlidate number of fetches can not be less than number of users
  validation_fetches_versus_user();  

  /* Step 2: distribute quantity as if for load key == 1 */
  if(global_settings->nvm_distribution == 0 ) { //Default. all groups are distributed equally over NVM's
    NSDL2_SCHEDULE(NULL, NULL, "NVM Distribution Mode=0");
    
    cur_proc = 0;
    for (i = 0 ; i < total_runprof_entries; i++) {
      schedule_ptr = &(group_schedule[i]);

      //Manish: here we using 3 as Duration phase as group based duration session is not allowed for advance scheduling
      ph_ptr = &(schedule_ptr->phase_array[3]);  //4th phase is DURATION Phase
      num_fetches = ph_ptr->phase_cmd.duration_phase.num_fetches;

      tot_distributed_fetches = 0;

      //Handle per user fetches
      for_all = num_fetches / global_settings->num_process;
      leftover = num_fetches % global_settings->num_process;

      NSDL2_SCHEDULE(NULL, NULL, "Group Id = %d, num_fetches =  %d, for_all = %d, leftover = %d, per_user_fetches_flag = %d", 
                           i, num_fetches, for_all, leftover, ph_ptr->phase_cmd.duration_phase.per_user_fetches_flag);
      for (j = 0; j < global_settings->num_process; j++) {
        num_users = per_proc_runprof_table[(j * total_runprof_entries) + i];
        if(!num_users) 
          continue;

        if(ph_ptr->phase_cmd.duration_phase.per_user_fetches_flag)
        {
          for_all = num_fetches * num_users;
          leftover = 0;
        }

        per_proc_per_grp_fetches[(j * total_runprof_entries) + i] = for_all;
        v_port_table[j].num_fetches += for_all;
        tot_distributed_fetches += for_all;
      }

      if(!ph_ptr->phase_cmd.duration_phase.per_user_fetches_flag)
      { 
        //Handle if group not distributed over all the NVMs
        if((tot_distributed_fetches + leftover) != num_fetches)
          leftover += num_fetches - (tot_distributed_fetches + leftover);

        for (j = 0; j < leftover; ) {
          if(per_proc_runprof_table[(cur_proc * total_runprof_entries) + i])
          {
            per_proc_per_grp_fetches[(cur_proc * total_runprof_entries) + i]++;
            v_port_table[cur_proc].num_fetches++;
            j++;
          }
          cur_proc++;
          if(cur_proc == global_settings->num_process) 
            cur_proc = 0;           /* This ensures that we fill left over for new group to the next NVM */
        }
      }
    } 
  } else if ( global_settings->nvm_distribution == 1 ) { //All groups are distrbuted in a way to putc one script in one NVM
    NSDL2_SCHEDULE(NULL, NULL, "NVM Distribution Mode = 1");
    //TODO: Manish need to discuss with NJ
    NS_EXIT(-1, CAV_ERR_1031054);
    #if 0
    NSDL2_SCHEDULE(NULL, NULL, "NVM Distribution Mode=1");
    qsort(psTable, total_runprof_entries, sizeof(PerScriptTotal), comp_by_stotal_script);
    last_sess_name = NULL;
    for (i=0; i < total_runprof_entries; i++) {
      if (psTable[i].sess_name != last_sess_name)
        last = find_proc_with_least_users_or_rate();
      per_proc_runprof_table[(last * total_runprof_entries) + psTable[i].sgroup_num] = psTable[i].pct_or_users;
      v_port_table[last].num_vusers += psTable[i].pct_or_users;
      last_sess_name = psTable[i].sess_name;
    }
    #endif
  } else {//User defined distribution
    NSDL2_SCHEDULE(NULL, NULL, "NVM Distribution Mode=2");
    RunProfTableEntry_Shr* rstart = runprof_table_shr_mem;
    for (i=0; i < total_runprof_entries; i++, rstart++) {
      schedule_ptr = &(v_port_table->group_schedule[i]);
      ph_ptr = &(schedule_ptr->phase_array[3]);  //4th phase is DURATION Phase
      num_fetches = ph_ptr->phase_cmd.duration_phase.num_fetches;

      per_proc_per_grp_fetches[(rstart->cluster_id * total_runprof_entries) + rstart->group_num] = num_fetches;
      v_port_table[rstart->cluster_id].num_fetches += num_fetches;
    }
  }

#ifdef NS_DEBUG_ON
  for (i = 0; i < total_runprof_entries; i++) {
    for (j = 0; j < global_settings->num_process; j++) {
      NSDL4_SCHEDULE(NULL, NULL, "NVM Distribution: Group id = %d, NVM = %d, QTY = %d, Session = %d", i, j, 
                     per_proc_runprof_table[(j * total_runprof_entries) + i], per_proc_per_grp_fetches[(j * total_runprof_entries) + i]);
    }
  }

  for (j = 0; j < global_settings->num_process; j++) {
    NSDL4_SCHEDULE(NULL, NULL, "nvm id = %d, totat fetches = %d", j, v_port_table[j].num_fetches);
  }
#endif /* NS_DEBUG_ON */

  //TODO: malloc memory on child start
  //Allocate shared memory for all groups so that they can keep trac how many session instance has been completed.
  //per_grp_sess_inst_num = do_shmget((sizeof(int) * total_runprof_entries * global_settings->num_process), "per_grp_sess_inst_num");
  //memset(per_grp_sess_inst_num, 0, (sizeof(int) * total_runprof_entries * global_settings->num_process)); 
}

static void
distribute_over_nvm(shr_logging* logging_memory, PerScriptTotal * psTable)
{
  int i,j;
  int for_all, leftover;
  int cur_proc;
  char* last_sess_name = NULL;
  int last=0;
  int proc_index;

  NSDL2_SCHEDULE(NULL, NULL, "Method called");
  /* Initialize per v_port phases tables */
  init_vport_table_phases();
  char *ptr;
  ptr =(char *) logging_memory;
  for (i = 0; i < global_settings->num_process; i++) {
    if (logging_memory)
    {
      v_port_table[i].logging_mem = (shr_logging *) (ptr + (i * (LOGGING_PER_CHILD_SHR_MEM_SIZE(log_shr_buffer_size, logging_shr_blk_count))));
    }
    else
      v_port_table[i].logging_mem = NULL;
    //v_port_table[i].ramping_done = 0; /* TODO:BHAV:REMOVE:XXXX */
    v_port_table[i].num_vusers = 0;
    if(global_settings->schedule_by == SCHEDULE_BY_SCENARIO)
      v_port_table[i].num_fetches = (global_settings->num_fetches + global_settings->num_process - 1 - i)/global_settings->num_process;
    else
      v_port_table[i].num_fetches = 0; 

    //v_port_table[i].warmup_sessions = (child_global_data.warmup_sessions + global_settings->num_process - 1 - i)/global_settings->num_process;
    //v_port_table[i].warmup_sessions = (global_settings->num_process - 1 - i)/global_settings->num_process; /* TODO:BHAV:REMOVE:XXXX */
  }

  /* Step 2: distribute quantity as if for load key == 1 */
  if ( global_settings->nvm_distribution == 0 ) { //Default. all groups are distributed equally over NVM's
    NSDL2_SCHEDULE(NULL, NULL, "NVM Distribution Mode=0");
    //Distribute num_vusers over different NVM's. Start extra user from next NVM
    //v_port_table[proc].num_vusers would have the users for the NVM
    //for sess rate  case, pct is copied as as for all nvm. v_port_tabe[proc].num_vuser 
    //would contain the sum of pct for sgroups for an NVM
    
    RunProfTableEntry_Shr* rstart = runprof_table_shr_mem;
    cur_proc = 0;
    for (i = 0 ; i < total_runprof_entries; i++, rstart++) {
      for_all = (rstart->quantity) / global_settings->num_process;
      leftover = (rstart->quantity) % global_settings->num_process;

      for (j = 0; j < global_settings->num_process; j++) {
        per_proc_runprof_table[(j * total_runprof_entries) + i] = for_all;
        v_port_table[j].num_vusers += for_all;
      }

      for (j = 0; j < leftover; j++) {
        per_proc_runprof_table[(cur_proc * total_runprof_entries) + i]++;
        v_port_table[cur_proc].num_vusers++;
        cur_proc++;
        if (cur_proc >= global_settings->num_process) cur_proc = 0; /* This ensures that we fill left over for
                                                                     * new group to the next NVM */
      }
    } 
  } else if ( global_settings->nvm_distribution == 1 ) { //All groups are distrbuted in a way to putc one script in one NVM
    NSDL2_SCHEDULE(NULL, NULL, "NVM Distribution Mode=1");
    qsort(psTable, total_runprof_entries, sizeof(PerScriptTotal), comp_by_stotal_script);
    last_sess_name = NULL;
    for (i=0; i < total_runprof_entries; i++) {
      if (psTable[i].sess_name != last_sess_name)
        last = find_proc_with_least_users_or_rate();
      per_proc_runprof_table[(last * total_runprof_entries) + psTable[i].sgroup_num] = psTable[i].pct_or_users;
      v_port_table[last].num_vusers += psTable[i].pct_or_users;
      last_sess_name = psTable[i].sess_name;
    }
  } else {//User defined distribution
    NSDL2_SCHEDULE(NULL, NULL, "NVM Distribution Mode=2");
    RunProfTableEntry_Shr* rstart = runprof_table_shr_mem;
    for (i=0; i < total_runprof_entries; i++, rstart++) {
      per_proc_runprof_table[(rstart->cluster_id * total_runprof_entries) + rstart->group_num] = rstart->quantity;
      v_port_table[rstart->cluster_id].num_vusers += rstart->quantity;
    }
  }

#ifdef NS_DEBUG_ON
  for (i = 0; i < total_runprof_entries; i++) {
    for (j = 0; j < global_settings->num_process; j++) {
      NSDL4_SCHEDULE(NULL, NULL, "NVM Distribution: Group id = %d, NVM = %d, QTY = %d", i, j, 
                     per_proc_runprof_table[(j * total_runprof_entries) + i]);
    }
  }
#endif /* NS_DEBUG_ON */


  /* For replay access logs, we do not need phase distribution. so return */
  if (global_settings->replay_mode != 0)
    return;

  /* Step 3 */
  /* After individual group quantity distribution, we need to divide per phase
   * quantity across NVMs
   */

  /* Divide per phase per group */
  if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO)
  {
    if(get_group_mode(-1) == TC_FIX_CONCURRENT_USERS) 
      fill_per_group_phase_table(scenario_schedule);
  }


  /* Phase division */
  if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO) {
    distribute_phase_over_nvm(-1, scenario_schedule);
  } else if (global_settings->schedule_by == SCHEDULE_BY_GROUP) {
    for (j = 0; j < total_runprof_entries; j++) {
      //Added check for netomni
      if (loader_opcode == CLIENT_LOADER) {
        /* NOTE: In CLIENT_LOADER only NUM mode is supported therefore verfying only quantity field of runprof_table_shr_mem*/
        if (runprof_table_shr_mem[j].quantity == 0) {
          NSDL4_SCHEDULE(NULL, NULL, "Ignoring scenario group with quantity given zero, index = %d", j); 
          continue; 
        }  
      }
      distribute_phase_over_nvm(j, &group_schedule[j]);
    }
  }

  for (proc_index = 0; proc_index < global_settings->num_process; proc_index++) 
  {
    //per_grp_qty calculations now moved from NVM to parent
    if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO)
    {
      if(get_group_mode(-1) == TC_FIX_CONCURRENT_USERS) 
        distribute_group_qty_over_phase(v_port_table[proc_index].scenario_schedule, proc_index);

      //runprof_table_cummulative computation now moved from NVM to parent
      compute_runprof_table_cummulative(v_port_table[proc_index].scenario_schedule, proc_index);
    }
  }

  if(global_settings->concurrent_session_mode)
    divide_on_fcs_mode();
  
  if(global_settings->num_fetches)
  {
    if(global_settings->schedule_by == SCHEDULE_BY_SCENARIO) //&& it is a num user case
      distribute_fetches();
    else
      distribute_fetchs_over_nvms_and_grps(psTable);
  }
}

void detach_shared_memory(void *ptr) {
  int ret;
  
  NSDL2_SCHEDULE(NULL, NULL, "Method called.");

  if(ptr) {
    ret = shmdt(ptr);
    if (ret) {
     	NS_EXIT(-1, CAV_ERR_1000041, errno, nslb_strerror(errno));
    }
  }
}


//Add By Manish:
/*
TC_FIX_CONCURRENT_USERS 0 //FCU
TC_FIX_USER_RATE 1     //FSR
TC_FIX_HIT_RATE 2
TC_FIX_PAGE_RATE 3
TC_FIX_TX_RATE 4
TC_MEET_SLA 5
TC_MEET_SERVER_LOAD 6
TC_FIX_MEAN_USERS 7
*/

char *get_grptype(int grp_idx)
{
  static char type[1024];
  
  int group_mode = get_group_mode(grp_idx);
  switch(group_mode)
  {
    case TC_FIX_CONCURRENT_USERS:
      strcpy(type,"FCU");
      break;
    case TC_FIX_USER_RATE:
      strcpy(type,"FSR");
      break;
    case TC_FIX_HIT_RATE:
      strcpy(type, "FixHitRate");
      break;
    case TC_FIX_PAGE_RATE:
      strcpy(type, "FixPageRate");
      break;
    case TC_FIX_TX_RATE:
      strcpy(type, "FixTxRate");
      break;
    case TC_MEET_SLA:
      strcpy(type, "FixMeetSlaRate");
      break;
    case TC_MEET_SERVER_LOAD:
      strcpy(type, "FixMeetServerLoadRate");
      break;
    case TC_FIX_MEAN_USERS:
      strcpy(type, "FixMeanUsersRate");
      break;
  }
 
  return type;
}

static void dump_user_distribution_among_nvm_for_FCU()
{
  int i,j;
  char *type;
  
  //printf("TotalUsers=%d\n", global_settings->num_connections);
  NSTL1(NULL, NULL, "TotalUsers=%d", global_settings->num_connections);
  for (i = 0; i < total_runprof_entries; i++)
  {
    type = get_grptype(i);
    if(strcmp(type,"FCU") == 0)
    {
      //printf("Group=%s, Type=%s, TotalUsers=%d, Pct=%.2f%%\n", runprof_table_shr_mem[i].scen_group_name, type, runprof_table_shr_mem[i].quantity, (runprof_table_shr_mem[i].percentage/100.0));
      NSTL1(NULL, NULL, "Group=%s, Type=%s, TotalUsers=%d, Pct=%.2f%%", runprof_table_shr_mem[i].scen_group_name, type, runprof_table_shr_mem[i].quantity, (runprof_table_shr_mem[i].percentage/100.0));
      for (j = 0; j < global_settings->num_process; j++) {
        //printf("    NVM%d: Users=%d, Pct=%.2f%%\n", j, per_proc_runprof_table[(j * total_runprof_entries) + i], (per_proc_runprof_table[(j * total_runprof_entries) + i] * 100)/(runprof_table_shr_mem[i].quantity * 1.0));
        NSTL1(NULL, NULL, "    NVM%d: Users=%d, Pct=%.2f%%", j, per_proc_runprof_table[(j * total_runprof_entries) + i], (per_proc_runprof_table[(j * total_runprof_entries) + i] * 100)/(runprof_table_shr_mem[i].quantity * 1.0));
      }
    }
  }
}

static void dump_user_distribution_among_nvm_for_FSR()
{ 
  int i,j;
  char *type;
 
  //printf("TotalSessions=%.3f\n", global_settings->vuser_rpm/THOUSAND);
  NSTL1(NULL, NULL, "TotalSessions=%.3f", global_settings->vuser_rpm/THOUSAND);
  for (i = 0; i < total_runprof_entries; i++)
  {
    type = get_grptype(i);
    if(strcmp(type,"FSR") == 0)
    {
      //printf("Group=%s, Type=%s, SessionRate/Min=%.3f, Pct=%.2f%%\n", runprof_table_shr_mem[i].scen_group_name, type, runprof_table_shr_mem[i].quantity/THOUSAND, runprof_table_shr_mem[i].percentage/HUNDRED);
      NSTL1(NULL, NULL, "Group=%s, Type=%s, SessionRate/Min=%.3f, Pct=%.2f%%", runprof_table_shr_mem[i].scen_group_name, type, runprof_table_shr_mem[i].quantity/THOUSAND, runprof_table_shr_mem[i].percentage/HUNDRED); 
      for (j = 0; j < global_settings->num_process; j++) {
        //printf("    NVM%d: SessionRate/Min=%.3f, Pct=%.2f%%\n", j, per_proc_runprof_table[(j * total_runprof_entries) + i]/THOUSAND, (per_proc_runprof_table[(j * total_runprof_entries) + i] * HUNDRED)/(runprof_table_shr_mem[i].quantity * 1.0));
        NSTL1(NULL, NULL, "    NVM%d: SessionRate/Min=%.3f, Pct=%.2f%%", j, per_proc_runprof_table[(j * total_runprof_entries) + i]/THOUSAND, (per_proc_runprof_table[(j * total_runprof_entries) + i] * HUNDRED)/(runprof_table_shr_mem[i].quantity * 1.0));
      }
    }
  }
}

static void dump_user_distribution_among_nvm()
{
  if (testCase.mode == TC_MIXED_MODE)
  {
    //printf("User/Session Rate Distribution among NVMs:\n");
    NSTL1(NULL, NULL, "User/Session Rate Distribution among NVMs:");
    dump_user_distribution_among_nvm_for_FCU();
    dump_user_distribution_among_nvm_for_FSR();
  } 
  else if (get_group_mode(-1) == TC_FIX_CONCURRENT_USERS) 
  {
    //printf("User Distribution among NVMs:\n");
    NSTL1(NULL, NULL, "User Distribution among NVMs:");
    dump_user_distribution_among_nvm_for_FCU();
  } 
  else
  {
    //printf("Session Rate Distribution among NVMs:\n");
    NSTL1(NULL, NULL, "Session Rate Distribution among NVMs:");
    dump_user_distribution_among_nvm_for_FSR();
  }
}


void 
divide_users_or_pct_per_proc(int conf_num_proc)
{
  PerScriptTotal * psTable;
  //int num_user_or_pct = global_settings->load_key?global_settings->num_connections:global_settings->vuser_rpm;
  int num_user_or_pct = 0;
  int i;
  shr_logging *logging_memory;
  int v_port_table_size = 0;
  int is_one_grp_fsr = -1;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. conf_num_proc = %d, mode = %d", conf_num_proc, testCase.mode);
  //v_port_table = (s_child_ports*)do_shmget(global_settings->num_process*sizeof(s_child_ports), "PerProcPortsTable");

  if (testCase.mode == TC_MIXED_MODE) { /* In case of mixed mode, if even one group
                                         * is FSR, we set num_user_or_pct to its rate */
    for (i = 0; i < total_runprof_entries; i++) {
      if ((get_group_mode(i) == TC_FIX_USER_RATE) && (runprof_table_shr_mem[i].quantity)) {
          is_one_grp_fsr = i;
          break;
      }
    }
    if (is_one_grp_fsr != -1) {
      num_user_or_pct = runprof_table_shr_mem[is_one_grp_fsr].quantity;
    } else { 
      num_user_or_pct = global_settings->num_connections;
    }
  } else if (get_group_mode(-1) == TC_FIX_CONCURRENT_USERS) {
    num_user_or_pct = global_settings->num_connections;
  } else {
    num_user_or_pct = global_settings->vuser_rpm;
  }

  // Done to make required number of NVM's in low session rate
  if(((testCase.mode == TC_MIXED_MODE) && (is_one_grp_fsr != -1)) || (testCase.mode == TC_FIX_USER_RATE))
  {
    if(!global_settings->per_proc_min_user_session)
      global_settings->per_proc_min_user_session = SESSION_RATE_MULTIPLE;
    else
      global_settings->per_proc_min_user_session = global_settings->per_proc_min_user_session * SESSION_RATE_MULTIPLE;
  }

  global_settings->num_process = get_num_proc (conf_num_proc, num_user_or_pct);
  //CHECK_NUM_PROCESS  //bug 40284

  NSDL2_SCHEDULE(NULL, NULL, "global_settings->num_process = %d, total_pdf_data_size = %d, g_percentile_report = %d", 
                              global_settings->num_process, total_pdf_data_size, g_percentile_report);
  
  if (g_percentile_report == 1 && total_pdf_data_size > PDF_DATA_HEADER_SIZE) 
  {
    if (loader_opcode == MASTER_LOADER){
      create_memory_for_parent();
    }else{
      /* Initialize shared memory. */
      init_pdf_shared_mem();
    }
    open_pct_message_file();
  } 

  //ports_per_child = (MAX_PORT_NUMBER - MIN_PORT_NUMBER) / global_settings->num_process; 
  /* Earliar we were mallocing so it was neccessary to FREE but now for run time changes,
  we are using shared memory so detach it,
  It comes here more than once in case of GOAL BASED SCENARIO */
  #ifndef CAV_MAIN
  detach_shared_memory(v_port_table);
  #endif 
  //Manish: add size of 
  if(global_settings->schedule_by == SCHEDULE_BY_SCENARIO)
    v_port_table_size = (global_settings->num_process * sizeof(s_child_ports)) +
                        next_power_of_n(sizeof(int) * global_settings->num_process * total_runprof_entries, WORDSIZE);  
  else
    v_port_table_size = (global_settings->num_process * sizeof(s_child_ports)) +
                        2 * next_power_of_n(sizeof(int) * global_settings->num_process * total_runprof_entries, WORDSIZE);  
  
  v_port_table = do_shmget(v_port_table_size, "v_port_table + PerProcRunProf");

  //per_proc_runprof_table keeps num_vuser/pct for all sgrps for all procs
  per_proc_runprof_table = (int *) (v_port_table + global_settings->num_process);
 
  if(global_settings->schedule_by == SCHEDULE_BY_GROUP)
    per_proc_per_grp_fetches = (int *) (per_proc_runprof_table + (total_runprof_entries * global_settings->num_process));

  psTable = create_per_script_total();

  /* PageDump: In case for making G_TRACING kw runtime changeable we need to initialize
   * logging memory for logging writer, hence shared memory became creates at init time*/
  #ifndef CAV_MAIN
  if (run_mode == NORMAL_RUN && loader_opcode != MASTER_LOADER) {
    if (!(logging_memory = initialize_logging_memory(global_settings->num_process, global_settings, testidx, g_generator_idx))) {
      NS_EXIT(-1, "Failed to initialize static and dynamic log memory");
    }
  } else {
    logging_memory = NULL;
  }
  #else
    logging_memory = NULL;
  #endif


  if(loader_opcode != MASTER_LOADER) {
    distribute_over_nvm(logging_memory, psTable); 

/* Code is commented for connection pool design, here we require
 * ultimate_max_connections which provide info about how many connections 
 * can be made by one NVM
 * Single process cannot make connection more than ulimit
 * max user processes              (-u) 65536
 * In new design max user depends on browser settings whereas max connection 
 * depend on ulimit
 * */
#if 0
    if (global_settings->load_key) {
      for (i = 0; i < global_settings->num_process; i++) {
        assert(v_port_table[i].num_vusers);
        if ( (v_port_table[i].num_vusers)*global_settings->max_con_per_vuser > ultimate_max_connections ) {
	    fprintf(stderr, "Max Vuser (NVM=%d) is %d and con per user=%d."
                            " Max connections cant be more than %d\n", 
	    	            i, v_port_table[i].num_vusers,
                            global_settings->max_con_per_vuser, ultimate_max_connections);
	    exit(1);
        }
      }
    }
#endif 
 //Add By Manish: to show user and session rate distribution in progress report 
 #ifndef CAV_MAIN
 dump_user_distribution_among_nvm();
 #endif

  //To test detailed distribution
  // DL_ISSUE
#ifdef NS_DEBUG_ON
    int j;

    NSDL3_SCHEDULE(NULL, NULL, "number of children: %d, num_users=%d,"
                               " sess_rate=%d, num_fetches=%d, num_seconds=%d, load_key=%d\n", 
 		               global_settings->num_process, global_settings->num_connections,
                               global_settings->vuser_rpm, global_settings->num_fetches, 
		               global_settings->test_stab_time, global_settings->load_key);

    for (i = 0; i < global_settings->num_process; i++) {
      NSDL1_SCHEDULE(NULL, NULL, "\tNVM %d: vusers=%d sess/min=%1.2f fetches=%d\n",
                                 i, v_port_table[i].num_vusers,
    		                 v_port_table[i].vuser_rpm, v_port_table[i].num_fetches);

      for (j = 0; j < total_runprof_entries; j++)
        NSDL1_SCHEDULE(NULL, NULL, "\t\tsgrp %d: vusers/pct=%d\n", j, (per_proc_runprof_table[(i * total_runprof_entries) + j]));
    }
#endif
    divide_values(psTable, 0, 1);
  }
}

