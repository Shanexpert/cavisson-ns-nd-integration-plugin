/********************************************************************************************
 *Name: ns_trans_normalization.c
 *Purpose: This C file holds the functions for tansaction normalization.
 *Initial version date: June 2017
 *Author: Manmeet
 *Updated version date: Feb 2019
 *Author: ANUBHAV
 *Change Description: New design in NC mode for handling Generator Wise Graphs
********************************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/times.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <regex.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "nslb_util.h"
#include "netomni/src/core/ni_scenario_distribution.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"
#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "user_tables.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "url.h"
#include "util.h"
#include "timing.h"
#include "tmr.h"
#include "amf.h"
#include "ns_trans_parse.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"
#include "netstorm.h"
#include "runlogic.h"
#include "ns_gdf.h"
#include "ns_vars.h"
#include "ns_log.h"
#include "ns_cookie.h"
#include "ns_user_monitor.h"
#include "ns_alloc.h"
#include "ns_percentile.h"
#include "ns_parse_scen_conf.h"
#include "ns_server_admin_utils.h"
#include "ns_error_codes.h"
#include "ns_page.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "ns_ftp_parse.h"
#include "ns_ftp.h"
#include "ns_smtp_parse.h"
#include "ns_script_parse.h"
#include "ns_vuser.h"
#include "ns_group_data.h"
#include "ns_trace_level.h"
#include "ns_connection_pool.h"
#include "netomni/src/core/ni_scenario_distribution.h"
#include "ns_server_ip_data.h"
#include "ns_http_cache_reporting.h"
#include "ns_network_cache_reporting.h"
#include "ns_dns_reporting.h"
#include "ns_ldap.h"
#include "ns_imap.h"
#include "ns_jrmi.h"
#include "ns_netstorm_diagnostics.h"
#include "ns_rbu_page_stat.h"
#include "ns_page_based_stats.h"
#include "ns_runtime_runlogic_progress.h"
#include "ns_trans_normalization.h"
#include "ns_ip_data.h"
#include "ns_trans.h"
#include "nslb_log.h"
#include "netomni/src/core/ni_user_distribution.h"

#ifndef CAV_MAIN
TxLoc2NormTable *g_tx_loc2norm_table = NULL; //Global variable for starting address of g_tx_loc2norm table
#else
__thread TxLoc2NormTable *g_tx_loc2norm_table = NULL; //Global variable for starting address of g_tx_loc2norm table
#endif
extern int log_tx_table_record_v2(char *tx_name, int tx_len, unsigned int tx_index, int nvm_id);
//NormObjKey g_key_tx_normid;
#define TX_LOC2_NORM_DELTA_SIZE 64  // TODO - can we configure it

inline void ns_trans_init_loc2norm_table(int static_tx_count)
{
  int flag_new;
  char *tx_name;
  int i, j, k, idx;
  int nvmindex = 0;
  int  tx_name_len;
  int  norm_tx_id;
  int num_process;
  int grp_id = 0, gen_id;

  NSDL1_TRANS(NULL, NULL, "Method called , static_tx_count = %d global_settings->num_process = %d, "
                          "sgrp_used_genrator_entries = %d", 
                           static_tx_count, global_settings->num_process, sgrp_used_genrator_entries);

  if(g_tx_loc2norm_table)
  {
    NSTL1(NULL, NULL, "error -  g_tx_loc2norm_table must be null");
  }
  //Bug 35346, in NC case num_process = generators
  num_process = ((loader_opcode == MASTER_LOADER)?sgrp_used_genrator_entries:global_settings->num_process);
  if(!num_process)
  {
    NSTL1(NULL, NULL, "Error - num_process (number of childs) must not be 0");
    return;
  }
  
  MY_MALLOC_AND_MEMSET(g_tx_loc2norm_table, num_process * sizeof(TxLoc2NormTable), "TxLoc2NormTable", (int)-1);

  //if(!static_tx_count)
  //  return ;

  for(nvmindex = 0; nvmindex < num_process; nvmindex++)
  {
    if(static_tx_count)
    {
      MY_MALLOC(g_tx_loc2norm_table[nvmindex].nvm_tx_loc2norm_table, static_tx_count * sizeof(int), "g_tx_loc2norm_table", nvmindex);
      memset(g_tx_loc2norm_table[nvmindex].nvm_tx_loc2norm_table, -1, static_tx_count * sizeof(int));
      g_tx_loc2norm_table[nvmindex].loc2norm_alloc_size = static_tx_count;
    }
    //Keeping track of avg index in loc2normtbl
    g_tx_loc2norm_table[nvmindex].loc_tx_avg_idx = g_trans_avgtime_idx;
  }

  if(loader_opcode != MASTER_LOADER)
  {
    for(nvmindex = 0; nvmindex < num_process; nvmindex++)
    {
      int txindex = 0;
      for(txindex = 0; txindex < static_tx_count; txindex++)
      {
        g_tx_loc2norm_table[nvmindex].nvm_tx_loc2norm_table[txindex] = txindex;
      }
    }
  }  
  /* We need to support with no static tx also. Only dynamic tx case */
  else
  {
    for (j = 0; j < total_runprof_entries; j++)
    {
      for(k = 0 ; k < scen_grp_entry[grp_id].num_generator; k++)
      {
        SessTableEntry sess_ptr = gSessionTable[runProfTable[j].sessprof_idx];
        gen_id = scen_grp_entry[grp_id].generator_id_list[k];

        for(i = 0; i < sess_ptr.num_dyn_entries; i++)
        { 
          idx  = sess_ptr.dyn_norm_ids[i]; //change to txTable_idx
          tx_name = RETRIEVE_BUFFER_DATA(txTable[idx].tx_name);
          tx_name_len = strlen(tx_name);
 
          norm_tx_id = nslb_get_or_set_norm_id(&normRuntimeTXTable, tx_name, tx_name_len, &flag_new);
           
          if(g_tx_loc2norm_table[gen_id].nvm_tx_loc2norm_table[norm_tx_id] == -1)
            g_tx_loc2norm_table[gen_id].num_entries++;

          g_tx_loc2norm_table[gen_id].nvm_tx_loc2norm_table[norm_tx_id] = norm_tx_id;
          /*                
          if(flag_new)
          {
            //idx = g_tx_loc2norm_table[gen_id].num_entries; 
            //g_tx_loc2norm_table[gen_id].nvm_tx_loc2norm_table[idx] = norm_tx_id;
            g_tx_loc2norm_table[gen_id].num_entries++; 
          }*/
        }  
      }
      grp_id = grp_id + scen_grp_entry[grp_id].num_generator;
    }
  }

  for(i = 0; i < static_tx_count; i++)
  {
    tx_name = RETRIEVE_BUFFER_DATA(txTable[i].tx_name);
    tx_name_len = strlen(tx_name);
    
    norm_tx_id = nslb_get_or_set_norm_id(&normRuntimeTXTable, tx_name, tx_name_len, &flag_new);

    if(total_tx_entries >= global_settings->threshold_for_using_gperf)
    {
      txTable[i].tx_hash_idx = norm_tx_id;
    }
    // TODO: pass nvm_index specific rather than passing 0 
    for(nvmindex = 0; nvmindex < num_process; nvmindex++){
      log_tx_table_record_v2(tx_name, tx_name_len, norm_tx_id, nvmindex);
    }
   
    NSTL1(NULL, NULL, "Added static transaction[%d] = %s with norm_tx_id = %d "
                      "in normalization table, num_process = %d", i, tx_name, norm_tx_id, num_process);
  }
}

int ns_trans_add_dynamic_tx (short nvmindex, int local_tx_id, char *tx_name, short tx_name_len, int *flag_new)
{
  NSDL1_TRANS(NULL, NULL, "Method called, nvmindex = %d, local_tx_id = %d, tx_name = %s tx_name_len = %d, "
                          "g_tx_loc2norm_table[nvmindex].loc2norm_alloc_size = %d", nvmindex, local_tx_id, tx_name, tx_name_len, 
                           g_tx_loc2norm_table[nvmindex].loc2norm_alloc_size);

  int norm_tx_id = 0, num_process, i;

  if(!tx_name || nvmindex < 0 || local_tx_id < 0)
  {
    NSTL1(NULL, NULL, "Dynamic transaction addition failed tx_name = NULL or nvmindex = %d or local_tx_id = %d", nvmindex, local_tx_id);
    return -1;
  }

  log_tx_table_record_v2(tx_name, tx_name_len, local_tx_id, nvmindex);

  num_process = ((loader_opcode == MASTER_LOADER)?sgrp_used_genrator_entries:global_settings->num_process);

  if(total_tx_entries >= g_tx_loc2norm_table[nvmindex].loc2norm_alloc_size)
  {
    int old_size = g_tx_loc2norm_table[nvmindex].loc2norm_alloc_size * sizeof(int);
    g_tx_loc2norm_table[nvmindex].loc2norm_alloc_size = total_tx_entries + TX_LOC2_NORM_DELTA_SIZE;
    int new_size = g_tx_loc2norm_table[nvmindex].loc2norm_alloc_size * sizeof(int);

    NSTL1(NULL, NULL, "local_tx_idx is Greater than the size of g_tx_loc2norm_table ,So reallocating this table with New_size = %d, Old_size = %d for NVM = %d", new_size, old_size, nvmindex);
    //Allocating same memory for all nvm/generators
    for(i = 0; i < num_process; i++){ 
      MY_REALLOC_AND_MEMSET_WITH_MINUS_ONE(g_tx_loc2norm_table[i].nvm_tx_loc2norm_table, new_size, old_size, "g_tx_loc2norm_table", i);
      g_tx_loc2norm_table[i].loc2norm_alloc_size = total_tx_entries + TX_LOC2_NORM_DELTA_SIZE; 
    }
  } 

  norm_tx_id = nslb_get_or_set_norm_id(&normRuntimeTXTable, tx_name, tx_name_len, flag_new); 
  
  if(g_tx_loc2norm_table[nvmindex].nvm_tx_loc2norm_table[local_tx_id] == -1){
    g_tx_loc2norm_table[nvmindex].num_entries++;
    g_tx_loc2norm_table[nvmindex].dyn_total_entries[NEW_OBJECT_DISCOVERY_TX - 1]++;
  }  

  g_tx_loc2norm_table[nvmindex].nvm_tx_loc2norm_table[local_tx_id] = norm_tx_id;
  
  if(*flag_new)
  {
//    g_tx_loc2norm_table[nvmindex].dyn_total_entries[NEW_OBJECT_DISCOVERY_TX]++; //It will store nvm/gen wise dyn tx discovered between every progress interval 
    dynObjForGdf[NEW_OBJECT_DISCOVERY_TX].total++;  //It will store dynamic transaction discovered between every progress interval
    check_if_realloc_needed_for_dyn_obj(NEW_OBJECT_DISCOVERY_TX);

    if(global_settings->g_tx_cumulative_graph != 0)
    {
      g_tx_loc2norm_table[nvmindex].dyn_total_entries[NEW_OBJECT_DISCOVERY_TX_CUM - 1]++; //It will store nvm/gen wise dyn tx discovered between every progress interval 
      dynObjForGdf[NEW_OBJECT_DISCOVERY_TX_CUM].total++;  //It will store dynamic transaction discovered between every progress interval for cumulative
      check_if_realloc_needed_for_dyn_obj(NEW_OBJECT_DISCOVERY_TX_CUM);
    }
  }

  NSTL1(NULL, NULL, "g_tx_loc2norm_table[%d].nvm_tx_loc2norm_table[%d] = %d", 
                     nvmindex, local_tx_id ,g_tx_loc2norm_table[nvmindex].nvm_tx_loc2norm_table[local_tx_id] ); 

  return norm_tx_id;
}
