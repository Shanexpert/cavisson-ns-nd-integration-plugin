/********************************************************************************************
 *Name: ns_svr_ip_normalization.c
 *Author: ANUBHAV
 *Purpose: This C file holds the functions for ServerIp normalization.
 *Initial version date: Jan 2018
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
#include "nslb_log.h"
#include "ns_svr_ip_normalization.h"
//NORMALIZATION AT PARENT

extern NormObjKey normServerIPTable;
SvrIpLoc2NormTable *g_svr_ip_loc2norm_table = NULL; //Global variable for starting address of g_svr_ip_loc2norm table

inline void ns_svr_ip_init_loc2norm_table()
{
  int num_process, nvm_idx;
  
  NSDL1_SVRIP(NULL, NULL, "Method called global_settings->num_process = %d, sgrp_used_genrator_entries = %d", global_settings->num_process, sgrp_used_genrator_entries);
  
  if(g_svr_ip_loc2norm_table)
  {
    NSTL1(NULL, NULL, "error -  g_svr_ip_loc2norm_table must be null");
  }

  //On init, num_process is input from NUM_NVM keyword, for NC use sgrp_used_genrator_entries
  num_process = ((loader_opcode == MASTER_LOADER)?sgrp_used_genrator_entries:global_settings->num_process);
  
  if(!num_process)
  {
    NSTL1(NULL, NULL, "Error - num_process (number of childs) must not be 0");
    return;
  }

  MY_MALLOC_AND_MEMSET(g_svr_ip_loc2norm_table, num_process* sizeof(SvrIpLoc2NormTable), "SvrIpLoc2NormTable", (int)-1);
  
  for(nvm_idx = 0; nvm_idx < num_process; nvm_idx++)
  {
    g_svr_ip_loc2norm_table[nvm_idx].loc_srv_ip_avg_idx = srv_ip_data_idx;
  }
}

#define SVR_IP_LOC2_NORM_DELTA_SIZE 64

int ns_add_svr_ip(short nvmindex, int local_svr_ip_id, char *server_name, short server_name_len, int *flag_new)
{  
   NSDL1_SVRIP(NULL, NULL, "Method called, nvmindex = %d, local_svr_ip_id = %d, server_name = %s server_name_len = %d, "
                          "g_svr_ip_loc2norm_table[nvmindex].loc2norm_alloc_size = %d", nvmindex, local_svr_ip_id, server_name, 
                          server_name_len, g_svr_ip_loc2norm_table[nvmindex].loc2norm_alloc_size);

   int norm_svr_ip_id, num_process, i;
  
   num_process = ((loader_opcode == MASTER_LOADER)?sgrp_used_genrator_entries:global_settings->num_process);
 
   if(!server_name || nvmindex < 0 || local_svr_ip_id < 0)
   {
     NSTL1(NULL, NULL, "ServerIp addition failed server_name = NULL or nvmindex = %d or local_svr_ip_id = %d", nvmindex, local_svr_ip_id);
     return -1;
   }

   if(total_normalized_svr_ips >= g_svr_ip_loc2norm_table[nvmindex].loc2norm_alloc_size)
   {
     int old_size = g_svr_ip_loc2norm_table[nvmindex].loc2norm_alloc_size * sizeof(int);
     g_svr_ip_loc2norm_table[nvmindex].loc2norm_alloc_size = total_normalized_svr_ips + SVR_IP_LOC2_NORM_DELTA_SIZE;
     int new_size = g_svr_ip_loc2norm_table[nvmindex].loc2norm_alloc_size * sizeof(int);
     
     NSTL1(NULL, NULL, "local_svr_ip_id is Greater than the size of g_svr_ip_loc2norm_table ,So reallocating this table with New_size = %d, Old_size = %d for NVM = %d", new_size, old_size, nvmindex);
    
     for(i = 0; i < num_process; i++){ 
       MY_REALLOC_AND_MEMSET_WITH_MINUS_ONE(g_svr_ip_loc2norm_table[i].nvm_svr_ip_loc2norm_table, new_size, old_size, "g_svr_ip_loc2norm_table", i);   
       g_svr_ip_loc2norm_table[i].loc2norm_alloc_size = total_normalized_svr_ips + SVR_IP_LOC2_NORM_DELTA_SIZE;
     }
   }
   
   norm_svr_ip_id = nslb_get_or_set_norm_id(&normServerIPTable, server_name, server_name_len, flag_new);

   if(g_svr_ip_loc2norm_table[nvmindex].nvm_svr_ip_loc2norm_table[local_svr_ip_id] == -1){
     g_svr_ip_loc2norm_table[nvmindex].num_entries++;
     g_svr_ip_loc2norm_table[nvmindex].dyn_total_entries++;
   }

   g_svr_ip_loc2norm_table[nvmindex].nvm_svr_ip_loc2norm_table[local_svr_ip_id] = norm_svr_ip_id;
   //g_svr_ip_loc2norm_table[nvmindex].loc_srv_ip_avg_idx = srv_ip_data_idx;
   
   if(*flag_new)
   {
    // g_svr_ip_loc2norm_table[nvmindex].num_entries++;
    // g_svr_ip_loc2norm_table[nvmindex].dyn_total_entries++;
     dynObjForGdf[NEW_OBJECT_DISCOVERY_SVR_IP].total++;  //It will store dynamic server_ips discovered between every progress interval
     check_if_realloc_needed_for_dyn_obj(NEW_OBJECT_DISCOVERY_SVR_IP);
   } 
   
   NSTL1(NULL, NULL, "g_svr_ip_loc2norm_table[%d].nvm_svr_ip_loc2norm_table[%d] = %d", nvmindex, local_svr_ip_id, g_svr_ip_loc2norm_table[nvmindex].nvm_svr_ip_loc2norm_table[local_svr_ip_id] );

   return norm_svr_ip_id;

}

