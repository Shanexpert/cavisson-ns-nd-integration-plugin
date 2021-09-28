/********************************************************************************************************************
 * File Name      : ns_rbu_domain_stat.c                                                                            |
 |                                                                                                                  | 
 * Purpose        : This file contains all functions related to RBU Domain Stat graph                               |
 |                                                                                                                  |
 * HLD            : In GUI RBU Domain Stat graphs will be seen in following fashion                                 | 
 |                    GroupName (say G1)                                                                            |
 |                      |-> Domain Stats                                                                            |
 |                      |    |-> PageName (say P1)                                                                  |
 |                      |    |    |-> DomainName (say D1)                                                           |
 |                      |    |    |-> DomainName (say D2)                                                           |
 |                      |    |-> PageName (say P2)                                                                  |
 |                      |    |    |-> DomainName (say D3)                                                           |
 |                      |    |    |-> DomainName (say D1)                                                           |
 |                      |    |- PageName (say P3)                                                                   |
 |                      |    |    |-> DomainName (say D1)                                                           |
 |                      |    |    |-> DomainName (say D2)                                                           |
 |                      |-> Domain Stats                                                                            |
 |                           |-> PageName (say P1)                                                                  |
 |                           |    |-> DomainName (say D1)                                                           |
 |                           |    |-> DomainName (say D2)                                                           |
 |                           |-> PageName (say P2)                                                                  |
 |                           |    |-> DomainName (say D3)                                                           |
 |                           |    |-> DomainName (say D1)                                                           |
 |                           |-> PageName (say P3)                                                                  |
 |                                |-> DomainName (say D1)                                                           |
 |                                |-> DomainName (say D2)                                                           |
 *                                                                                                                  |
 * Author(s)      : Shikha/Nisha/Manish                                                                             |
 * Date           : 14 December 2017                                                                                |
 * Copyright      : (c) Cavisson Systems, 2017                                                                      |
 * Mod. History   :                                                                                                 |
 *******************************************************************************************************************/

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

#include "url.h"
#include "nslb_util.h"
#include "nslb_partition.h"
#include "nslb_get_norm_obj_id.h"
#include "nslb_log.h"
#include "nslb_big_buf.h"

#include "user_tables.h"
#include "ns_error_codes.h"
#include "url.h"
#include "util.h"
#include "timing.h"
#include "logging.h"
#include "ns_trace_level.h"
#include "netomni/src/core/ni_scenario_distribution.h"

#include "netstorm.h"
#include "runlogic.h"
#include "ns_gdf.h"
#include "ns_log.h"
#include "ns_alloc.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "ns_vuser.h"
#include "ns_rbu_page_stat.h"
#include "ns_test_gdf.h"
#include "ns_rbu_api.h"
#include "deliver_report.h"
#include "output.h"
#include "ns_rbu_domain_stat.h"
#include "ns_dynamic_avg_time.h"
#include "wait_forever.h"

/* ################# Global Variables ################## */

Rbu_domain_time_data_gp *rbu_domain_stat_gp_ptr;
Rbu_domains *rbu_domains;

int rbu_domain_stat_data_gp_idx = -1;

bigbuf_t dname_big_buf = {0};
#ifndef CAV_MAIN
NormObjKey rbu_domian_normtbl;
int rbu_domain_stat_avg_idx = -1;
Rbu_domain_loc2norm_table *g_domain_loc2norm_table = NULL; 
Rbu_domain_stat_avgtime *rbu_domain_stat_avg;
#else
__thread NormObjKey rbu_domian_normtbl;
__thread int rbu_domain_stat_avg_idx = -1;
__thread Rbu_domain_loc2norm_table *g_domain_loc2norm_table = NULL; 
__thread Rbu_domain_stat_avgtime *rbu_domain_stat_avg;
#endif
/* ################################################### */




/***********************************************************************************************
 |  • NAME:   	
 | 	update_rbu_domain_stat_avgtime_size() - update global variable rbu_domain_stat_avg_idx
 |        so that RBU Domain stats can be filled into avgtime. 
 |
 |  • SYNOPSIS: 
 | 	inline void update_rbu_domain_stat_avgtime_size()	
 |
 |  • DESCRIPTION:   	
 |      @ Create Norm Id table (rbu_domian_normtbl) to get unique domian id (i.e. hash code) 
 |        for each domain names 	
 |      @ Set global variable rbu_domain_stat_avg_idx by g_avgtime_size
 |      @ update g_avgtime_size by 'sizeof(Rbu_domain_stat_avgtime) * g_actual_num_pages' 
 |
 |  • RETURN VALUE:
 |	nothing
 ************************************************************************************************/
inline void update_rbu_domain_stat_avgtime_size()
{
  NSDL2_RBU(NULL, NULL, "Method called, g_avgtime_size = %d, rbu_domain_stat_avg_idx = %d, total_rbu_domain_entries = %d, "
                        "max_rbu_domain_entries = %d, rbu_domain_stats_mode = %d",
                         g_avgtime_size, rbu_domain_stat_avg_idx, total_rbu_domain_entries, 
                         max_rbu_domain_entries, global_settings->rbu_domain_stats_mode);

  if(!global_settings->rbu_domain_stats_mode)
  {
    NSDL2_RBU(NULL, NULL, "Domain Stats feature for RBU is not enabled");
    return;
  }

  // Creating NormID table for Domian so that every domain get unique domain id 
  nslb_init_norm_id_table_ex(&rbu_domian_normtbl, RBU_DOMAIN_NORM_TABLE_SIZE);
  
  rbu_domain_stat_avg_idx = g_avgtime_size;
  g_avgtime_size += RBU_DOMAIN_STAT_AVG_SIZE;

  //Allocating normalization map table for Doamins and Initialising g_domain_loc2norm_table table
  ns_rbu_domain_init_loc2norm_table(DELTA_RBU_DOMAIN_ENTRIES);

  NSDL2_RBU(NULL, NULL, "After updating avgsize for RBU Domain Stat, g_avgtime_size = %d, rbu_domain_stat_avg_idx = %d",
                         g_avgtime_size, rbu_domain_stat_avg_idx);
}

/***********************************************************************************************
 |  • NAME:   	
 | 	ns_rbu_domain_init_loc2norm_table() - create and initialise normalization table  
 |
 |  • SYNOPSIS: 
 |      void ns_rbu_domain_init_loc2norm_table(int domain_entries)	
 |
 |  • DESCRIPTION:   	
 |      @ Create table to map NVM's domain id to Parent's domain norm id 
 |
 |        Each NVMs has its unique domain id from 0 - n, and send its domain id to Parent. 
 |        To aggregate data of particular domain coming form differnt NVMs with differnt 
 |        domain id parent need to maintain normalisation map table
 |
 |        Eg: Suppose there are 2 NVMs and each has 2 domains and discoverd in following seq 
 |            NVM0:  D1(0)   D2(1) 
 |            NVM1:  D4(0)   D1(1)
 |
 |            And on Parent they comes in followin seq 
 |            NVM0:D1, NVM1:D4, NVM0:D2, NVM1:D1
 |
 |            On Parent: 
 |            1. Domain norm id will -
 |               D1 0
 |               D4 1
 |               D2 3
 |
 |            2. normalisation map table will be -
 |               It is a 2D array of integer, on 1st coloum it put NVM ids 
 |               and remaning coloums of each rows has mapped domain id.
 |               Row index => NVM's domain id
 |               Content on particulat row index => Parent's domain id 
 |
 |             NVMId   0   1   2   3   4   5   6 
 |            -------------------------------------
 |            | NVM0 | 0 | 3 |   |   |   |   |   |
 |            -------------------------------------
 |            | NVM1 | 1 | 0 |   |   |   |   |   |
 |            -------------------------------------
 |
 |  • RETURN VALUE:
 |	nothing
 ************************************************************************************************/
void ns_rbu_domain_init_loc2norm_table(int domain_entries)
{
  int nvmindex = 0;
  int num_process;

  NSDL2_RBU(NULL, NULL, "Method called , domain_entries = %d global_settings->num_process = %d, "
                         "sgrp_used_genrator_entries = %d, g_domain_loc2norm_table = %p", 
                          domain_entries, global_settings->num_process, sgrp_used_genrator_entries, g_domain_loc2norm_table);
  if(!domain_entries)
    return;
  
  //Bug 69239: On init, num_process is input from NUM_NVM keyword, for NC use sgrp_used_genrator_entries
  num_process = ((loader_opcode == MASTER_LOADER)?sgrp_used_genrator_entries:global_settings->num_process);

  MY_MALLOC_AND_MEMSET(g_domain_loc2norm_table, num_process * sizeof(Rbu_domain_loc2norm_table), "Rbu_domain_loc2norm_table", (int)-1);

  for(nvmindex = 0; nvmindex < num_process; nvmindex++)
  {
    MY_MALLOC(g_domain_loc2norm_table[nvmindex].nvm_domain_loc2norm_table, domain_entries * sizeof(int), "nvm_domain_loc2norm_table", (int)-1); 
    g_domain_loc2norm_table[nvmindex].loc2norm_domain_alloc_size = domain_entries;
    g_domain_loc2norm_table[nvmindex].loc_domain_avg_idx = rbu_domain_stat_avg_idx; 
  }
}

#define RBU_DOMAIN_LOC2_NORM_DELTA_SIZE       10

int ns_rbu_add_dynamic_domain(short nvmindex, int local_rbu_domain_id, char *rbu_domain_name, short rbu_domain_name_len, int *flag_new)
{
  NSDL1_RBU(NULL, NULL, "Method called, nvmindex = %d, local_rbu_domain_id = %d, rbu_domain_name = %s rbu_domain_name_len = %d, "
                         "g_domain_loc2norm_table[nvmindex].loc2norm_domain_alloc_size = %d", 
                          nvmindex, local_rbu_domain_id, rbu_domain_name,
                          rbu_domain_name_len, g_domain_loc2norm_table[nvmindex].loc2norm_domain_alloc_size);
  int norm_domain_id = 0;

  if(!rbu_domain_name || nvmindex < 0 || local_rbu_domain_id < 0)
  {
    NSTL1(NULL, NULL, "Dynamic RBU domain addition failed rbu_domain_name = NULL or nvmindex = %d or local_rbu_domain_id = %d", 
                       nvmindex, local_rbu_domain_id);
    return -1;
  }

  if(local_rbu_domain_id >= g_domain_loc2norm_table[nvmindex].loc2norm_domain_alloc_size)
  {
    int old_size = g_domain_loc2norm_table[nvmindex].loc2norm_domain_alloc_size * sizeof(int);

    g_domain_loc2norm_table[nvmindex].loc2norm_domain_alloc_size = local_rbu_domain_id + RBU_DOMAIN_LOC2_NORM_DELTA_SIZE;

    int new_size = g_domain_loc2norm_table[nvmindex].loc2norm_domain_alloc_size * sizeof(int);
    
    NSTL1(NULL, NULL, "local_rbu_domain_id is Greater than the size of g_domain_loc2norm_table ,So reallocating this table with New_size = %d,"
                      "Old_size = %d for NVM = %d", new_size, old_size, nvmindex);

    MY_REALLOC_AND_MEMSET_WITH_MINUS_ONE(g_domain_loc2norm_table[nvmindex].nvm_domain_loc2norm_table, new_size, old_size, 
                      "g_domain_loc2norm_table", nvmindex);
  }

  norm_domain_id = nslb_get_or_set_norm_id(&rbu_domian_normtbl, rbu_domain_name, rbu_domain_name_len, flag_new);
  g_domain_loc2norm_table[nvmindex].nvm_domain_loc2norm_table[local_rbu_domain_id] = norm_domain_id;
  //g_domain_loc2norm_table[nvmindex].loc_domain_avg_idx = rbu_domain_stat_avg_idx; 
  
  NSDL2_RBU(NULL, NULL, "norm_domain_id = %d, flag_new = %d", norm_domain_id, *flag_new);
  if(*flag_new)
  {
    dynObjForGdf[NEW_OBJECT_DISCOVERY_RBU_DOMAIN].total++;  //It will store dynamic domain discovered between every progress interval
    check_if_realloc_needed_for_dyn_obj(NEW_OBJECT_DISCOVERY_RBU_DOMAIN);
  } 
    
  NSTL1(NULL, NULL, "g_domain_loc2norm_table[%d].nvm_domain_loc2norm_table[%d] = %d", 
                     nvmindex, local_rbu_domain_id ,g_domain_loc2norm_table[nvmindex].nvm_domain_loc2norm_table[local_rbu_domain_id] );

  NSDL2_RBU(NULL, NULL, "Method exit: local_rbu_domain_id = %d, norm_domain_id = %d, total = %d", 
                         local_rbu_domain_id, norm_domain_id, dynObjForGdf[NEW_OBJECT_DISCOVERY_RBU_DOMAIN].total);

  return norm_domain_id; 
}

inline void initialise_rbu_domain_stat_min(avgtime *msg)
{
  Rbu_domain_stat_avgtime *rbu_domain_msg = NULL;
  rbu_domain_msg = ((Rbu_domain_stat_avgtime*)((char*)msg + rbu_domain_stat_avg_idx));

  NSDL2_PARENT(NULL, NULL, "Initialising all min and max value for RBUDomainStats: "
                           "g_actual_num_pages = %d, total_rbu_domain_entries = %d "
                           "rbu_domain_msg = %p, rbu_domain_stat_data_gp_idx = %d",
                            g_actual_num_pages, total_rbu_domain_entries, 
                            rbu_domain_msg, rbu_domain_stat_data_gp_idx);

  if(!global_settings->rbu_domain_stats_mode || !max_rbu_domain_entries)
    return;

  memset(rbu_domain_msg, 0, RBU_DOMAIN_STAT_AVG_SIZE);
  RESET_RBU_DOMAIN_STAT_AVG(rbu_domain_msg, 0, max_rbu_domain_entries);
}

// Called by child
inline void set_rbu_domain_stat_data_avgtime_ptr()
{
  NSDL2_RBU(NULL, NULL, "Method Called, rbu_domain_stats_mode = %d, rbu_domain_stat_avg_idx = %d", 
                         global_settings->rbu_domain_stats_mode, rbu_domain_stat_avg_idx);

  if(global_settings->rbu_domain_stats_mode == 1)
  {
    rbu_domain_stat_avg = (Rbu_domain_stat_avgtime*)((char *)average_time + rbu_domain_stat_avg_idx);
    NSDL2_RBU(NULL, NULL, "Domain Stats feature for RBU is enabled, rbu_domain_stat_avg = %p, rbu_domain_stat_avg_idx = %d", 
                            rbu_domain_stat_avg, rbu_domain_stat_avg_idx);
  } 

  NSDL2_RBU(NULL, NULL, "rbu_domain_stat_avg = %p", rbu_domain_stat_avg);
}

inline void set_rbu_domain_stat_data_avgtime_data(VUser *vptr)
{
  int domain_idx;

  NSDL2_RBU(vptr, NULL, "vptr = %p, total_rbu_domain_entries = %d, rbu_domains = %p, "
                         "rbu_domain_stat_avg = %p", 
                         vptr, total_rbu_domain_entries, rbu_domains, rbu_domain_stat_avg);

  if(rbu_domains == NULL || rbu_domain_stat_avg == NULL)
    return; 

  for(domain_idx = 0; domain_idx < total_rbu_domain_entries; domain_idx++)
  {
    if(!rbu_domains[domain_idx].is_filled)
      continue;
    
    FILL_RBU_DOMAIN_STAT_AVG_TIMES_GRP(dns_time, dns_time, dns_min_time, dns_max_time, dns_counts); 
    FILL_RBU_DOMAIN_STAT_AVG_TIMES_GRP(tcp_time, tcp_time, tcp_min_time, tcp_max_time, tcp_counts);
    FILL_RBU_DOMAIN_STAT_AVG_TIMES_GRP(ssl_time, ssl_time, ssl_min_time, ssl_max_time, ssl_counts);
    FILL_RBU_DOMAIN_STAT_AVG_TIMES_GRP(connect_time, connect_time, connect_min_time, connect_max_time, connect_counts);
    FILL_RBU_DOMAIN_STAT_AVG_TIMES_GRP(wait_time, wait_time, wait_min_time, wait_max_time, wait_counts);
    FILL_RBU_DOMAIN_STAT_AVG_TIMES_GRP(rcv_time, rcv_time, rcv_min_time, rcv_max_time, rcv_counts);
    FILL_RBU_DOMAIN_STAT_AVG_TIMES_GRP(blckd_time, blckd_time, blckd_min_time, blckd_max_time, blckd_counts);
    FILL_RBU_DOMAIN_STAT_AVG_TIMES_GRP(url_resp_time, url_resp_time, url_resp_min_time, url_resp_max_time, url_resp_counts);  
    FILL_RBU_DOMAIN_STAT_AVG_TIMES_GRP(num_request, num_req, num_req_min, num_req_max, num_req_counts);  

    //Dump 
    NSDL4_RBU(vptr, NULL, "Filling rbu_domain_stat_avg for Domain: id = %d, dns_time(cur = %f, agg = %f, min = %f, max = %f, count = %d), "
                          "tcp_time(cur = %f, agg = %f, min = %f, max = %f, count = %d), "
                          "ssl_time(cur = %f, agg = %f, min = %f, max = %f, count = %d), "
                          "connect_time(cur = %f, agg = %f, min = %f, max = %f, count = %d), "
                          "wait_time(cur = %f, agg = %f, min = %f, max = %f, count = %d), " 
                          "rcv_time(cur = %f, agg = %f, min = %f, max = %f, count = %d), ",
                           domain_idx, rbu_domains[domain_idx].dns_time, rbu_domain_stat_avg[domain_idx].dns_time, 
                           rbu_domain_stat_avg[domain_idx].dns_min_time, rbu_domain_stat_avg[domain_idx].dns_max_time,
                           rbu_domain_stat_avg[domain_idx].dns_counts, rbu_domains[domain_idx].tcp_time, 
                           rbu_domain_stat_avg[domain_idx].tcp_time, rbu_domain_stat_avg[domain_idx].tcp_min_time, 
                           rbu_domain_stat_avg[domain_idx].tcp_max_time, rbu_domain_stat_avg[domain_idx].tcp_counts,
                           rbu_domains[domain_idx].ssl_time, rbu_domain_stat_avg[domain_idx].ssl_time, 
                           rbu_domain_stat_avg[domain_idx].ssl_min_time, rbu_domain_stat_avg[domain_idx].ssl_max_time, 
                           rbu_domain_stat_avg[domain_idx].ssl_counts, rbu_domains[domain_idx].connect_time, 
                           rbu_domain_stat_avg[domain_idx].connect_time, rbu_domain_stat_avg[domain_idx].connect_min_time, 
                           rbu_domain_stat_avg[domain_idx].connect_max_time, rbu_domain_stat_avg[domain_idx].connect_counts, 
                           rbu_domains[domain_idx].wait_time, rbu_domain_stat_avg[domain_idx].wait_time, 
                           rbu_domain_stat_avg[domain_idx].wait_min_time, rbu_domain_stat_avg[domain_idx].wait_max_time,
                           rbu_domain_stat_avg[domain_idx].wait_counts, rbu_domains[domain_idx].rcv_time, 
                           rbu_domain_stat_avg[domain_idx].rcv_time, rbu_domain_stat_avg[domain_idx].rcv_min_time, 
                           rbu_domain_stat_avg[domain_idx].rcv_max_time, rbu_domain_stat_avg[domain_idx].rcv_counts
                           );
  }

  /* Reset rbu_domains */
  memset(rbu_domains, 0, total_rbu_domain_entries * sizeof(Rbu_domains));
}

/***********************************************************************************************
 |  • NAME:   	
 | 	fill_rbu_domain_stat_gp() - to fill graph data for RBU Domain Stats  
 |
 |  • SYNOPSIS: 
 |      void fill_rbu_domain_stat_gp(avgtime **g_avg);	
 |
 |	Arguments:
 |        g_avg 	- provide cur_avg for Overall(among Generators/NVMs) and Generators 
 |                                 _______________
 |                        g_avg = |_|_|_|_|_|_|_|_|
 |                                 0 1 2 3 4 5 6 7
 |                            0 = For Overall(among Generators/NVMs) 
 |                            1 = For Generator 0 
 |                            2 = For Generator 1 and so on ...
 |
 |  • DESCRIPTION:   	
 |      @ This function will fill graph data (Rbu_domain_time_data_gp) of RBU Domain Stats by  
 |        g_avg.
 |
 |  • RETURN VALUE:
 |	nothing
 ************************************************************************************************/
void fill_rbu_domain_stat_gp(avgtime **g_avg)
{
  int i = 0, k = 0, v_idx;
  avgtime *avg = NULL;
  Rbu_domain_stat_avgtime *loc_rbu_domain_stat_avg = NULL;
  Rbu_domain_time_data_gp *rbu_domain_stat_data_local_gp_ptr;

  NSDL2_RBU(NULL, NULL, "Method Called, loc_rbu_domain_stat_avg = %p, rbu_domain_stat_gp_ptr = %p, rbu_domain_stat_avg_idx = %d "
                        "total_runprof_entries = %d, g_rbu_num_pages = %d, sgrp_used_genrator_entries = %d, total_rbu_domain_entries = %d",
                         loc_rbu_domain_stat_avg, rbu_domain_stat_gp_ptr, rbu_domain_stat_avg_idx, total_runprof_entries, g_rbu_num_pages,
                         sgrp_used_genrator_entries, total_rbu_domain_entries);
  
  for(v_idx = 0; v_idx < sgrp_used_genrator_entries + 1; v_idx++)
  {
    avg = (avgtime *)g_avg[v_idx];
    loc_rbu_domain_stat_avg = (Rbu_domain_stat_avgtime *) ((char*) avg + rbu_domain_stat_avg_idx);

    NSDL2_REPORTING(NULL, NULL, "Filling graph data of RBBU Domain Stat for child %d and progress report_num = %d, "
                                "rbu_domain_stat_avg_idx = %d, total_rbu_domain_entries = %d, v_idx = %d, loc_rbu_domain_stat_avg = %p", 
                                avg->child_id, avg->elapsed, rbu_domain_stat_avg_idx, total_rbu_domain_entries, 
                                v_idx, loc_rbu_domain_stat_avg);

      for (i = 0; i < total_rbu_domain_entries; i++, k++)
      {
        NSDL3_RBU(NULL, NULL, "Fill rbu_domain_stat_data_local_gp_ptr for Domain id = %d, "
                              "dns_time = %f, tcp_time = %f,  ssl_time = %f, connect_time = %f, "
                              "wait_time = %f, rcv_time = %f, blckd_time =%f, url_resp_time = %f",
                               i, loc_rbu_domain_stat_avg[i].dns_time, loc_rbu_domain_stat_avg[i].tcp_time, 
                               loc_rbu_domain_stat_avg[i].ssl_time,
                               loc_rbu_domain_stat_avg[i].connect_time, loc_rbu_domain_stat_avg[i].wait_time, 
                               loc_rbu_domain_stat_avg[i].rcv_time,
                               loc_rbu_domain_stat_avg[i].blckd_time, loc_rbu_domain_stat_avg[i].url_resp_time);
     
        if(dynObjForGdf[NEW_OBJECT_DISCOVERY_RBU_DOMAIN].rtg_index_tbl[v_idx][i] < 0)
          continue;

        rbu_domain_stat_data_local_gp_ptr = 
                (Rbu_domain_time_data_gp *)((char *)msg_data_ptr + dynObjForGdf[NEW_OBJECT_DISCOVERY_RBU_DOMAIN].rtg_index_tbl[v_idx][i]);

        NSDL3_RBU(NULL, NULL, "GenId = %d, rtg_index = %d", v_idx, dynObjForGdf[NEW_OBJECT_DISCOVERY_RBU_DOMAIN].rtg_index_tbl[v_idx][i]);
        // DNS time Graph
        if(loc_rbu_domain_stat_avg[i].dns_counts > 0)
        {
          GDF_COPY_TIMES_VECTOR_DATA(rbu_domain_stat_data_gp_idx, 0, k, 0,
                      (double)(((double)loc_rbu_domain_stat_avg[i].dns_time) /
                                 ((double)(1000.0*(double)loc_rbu_domain_stat_avg[i].dns_counts))),
                      (double)loc_rbu_domain_stat_avg[i].dns_min_time/1000.0,
                      (double)loc_rbu_domain_stat_avg[i].dns_max_time/1000.0,
                      loc_rbu_domain_stat_avg[i].dns_counts,
                      rbu_domain_stat_data_local_gp_ptr->dns_time_gp.avg_time,
                      rbu_domain_stat_data_local_gp_ptr->dns_time_gp.min_time,
                      rbu_domain_stat_data_local_gp_ptr->dns_time_gp.max_time,
                      rbu_domain_stat_data_local_gp_ptr->dns_time_gp.succ);
        }
        else
        {
          GDF_COPY_TIMES_VECTOR_DATA(rbu_domain_stat_data_gp_idx, 0, k, 0,
                      0,
                     -1,
                      0,
                      0,
                      rbu_domain_stat_data_local_gp_ptr->dns_time_gp.avg_time,
                      rbu_domain_stat_data_local_gp_ptr->dns_time_gp.min_time,
                      rbu_domain_stat_data_local_gp_ptr->dns_time_gp.max_time,
                      rbu_domain_stat_data_local_gp_ptr->dns_time_gp.succ);
        }

        // tcp time Graph
        if(loc_rbu_domain_stat_avg[i].tcp_counts > 0)
        {
          GDF_COPY_TIMES_VECTOR_DATA(rbu_domain_stat_data_gp_idx, 1, k, 0,
                      (double)(((double)loc_rbu_domain_stat_avg[i].tcp_time) /
                                 ((double)(1000.0*(double)loc_rbu_domain_stat_avg[i].tcp_counts))),
                      (double)loc_rbu_domain_stat_avg[i].tcp_min_time/1000.0,
                      (double)loc_rbu_domain_stat_avg[i].tcp_max_time/1000.0,
                      loc_rbu_domain_stat_avg[i].tcp_counts,
                      rbu_domain_stat_data_local_gp_ptr->tcp_time_gp.avg_time,
                      rbu_domain_stat_data_local_gp_ptr->tcp_time_gp.min_time,
                      rbu_domain_stat_data_local_gp_ptr->tcp_time_gp.max_time,
                      rbu_domain_stat_data_local_gp_ptr->tcp_time_gp.succ);
        }
        else
        {
          GDF_COPY_TIMES_VECTOR_DATA(rbu_domain_stat_data_gp_idx, 1, k, 0,
                      0,
                      -1,
                      0,
                      0,
                      rbu_domain_stat_data_local_gp_ptr->tcp_time_gp.avg_time,
                      rbu_domain_stat_data_local_gp_ptr->tcp_time_gp.min_time,
                      rbu_domain_stat_data_local_gp_ptr->tcp_time_gp.max_time,
                      rbu_domain_stat_data_local_gp_ptr->tcp_time_gp.succ);
        }
 
        // ssl time Graph
        if(loc_rbu_domain_stat_avg[i].ssl_counts > 0)
        {
          GDF_COPY_TIMES_VECTOR_DATA(rbu_domain_stat_data_gp_idx, 2, k, 0, 
                      (double)(((double)loc_rbu_domain_stat_avg[i].ssl_time) /
                                 ((double)(1000.0*(double)loc_rbu_domain_stat_avg[i].ssl_counts))),
                      (double)loc_rbu_domain_stat_avg[i].ssl_min_time/1000.0,
                      (double)loc_rbu_domain_stat_avg[i].ssl_max_time/1000.0,
                      loc_rbu_domain_stat_avg[i].ssl_counts,
                      rbu_domain_stat_data_local_gp_ptr->ssl_time_gp.avg_time,
                      rbu_domain_stat_data_local_gp_ptr->ssl_time_gp.min_time,
                      rbu_domain_stat_data_local_gp_ptr->ssl_time_gp.max_time,
                      rbu_domain_stat_data_local_gp_ptr->ssl_time_gp.succ);
        }
        else
        {
          GDF_COPY_TIMES_VECTOR_DATA(rbu_domain_stat_data_gp_idx, 2, k, 0,
                      0,
                      -1,
                      0,
                      0,
                      rbu_domain_stat_data_local_gp_ptr->ssl_time_gp.avg_time,
                      rbu_domain_stat_data_local_gp_ptr->ssl_time_gp.min_time,
                      rbu_domain_stat_data_local_gp_ptr->ssl_time_gp.max_time,
                      rbu_domain_stat_data_local_gp_ptr->ssl_time_gp.succ);
        }

        // connect time Graph
        if(loc_rbu_domain_stat_avg[i].connect_counts > 0)
        {
          GDF_COPY_TIMES_VECTOR_DATA(rbu_domain_stat_data_gp_idx, 3, k, 0,
                      (double)(((double)loc_rbu_domain_stat_avg[i].connect_time) /
                                 ((double)(1000.0*(double)loc_rbu_domain_stat_avg[i].connect_counts))),
                      (double)loc_rbu_domain_stat_avg[i].connect_min_time/1000.0,
                      (double)loc_rbu_domain_stat_avg[i].connect_max_time/1000.0,
                      loc_rbu_domain_stat_avg[i].connect_counts,
                      rbu_domain_stat_data_local_gp_ptr->connect_time_gp.avg_time,
                      rbu_domain_stat_data_local_gp_ptr->connect_time_gp.min_time,
                      rbu_domain_stat_data_local_gp_ptr->connect_time_gp.max_time,
                      rbu_domain_stat_data_local_gp_ptr->connect_time_gp.succ);
        }
        else
        {
          GDF_COPY_TIMES_VECTOR_DATA(rbu_domain_stat_data_gp_idx, 3, k, 0,
                      0,
                      -1,
                      0,
                      0,
                      rbu_domain_stat_data_local_gp_ptr->connect_time_gp.avg_time,
                      rbu_domain_stat_data_local_gp_ptr->connect_time_gp.min_time,
                      rbu_domain_stat_data_local_gp_ptr->connect_time_gp.max_time,
                      rbu_domain_stat_data_local_gp_ptr->connect_time_gp.succ);
        }

        // wait time Graph
        if(loc_rbu_domain_stat_avg[i].wait_counts > 0)
        {
          GDF_COPY_TIMES_VECTOR_DATA(rbu_domain_stat_data_gp_idx, 4, k, 0,
                      (double)(((double)loc_rbu_domain_stat_avg[i].wait_time) /
                                 ((double)(1000.0*(double)loc_rbu_domain_stat_avg[i].wait_counts))),
                      (double)loc_rbu_domain_stat_avg[i].wait_min_time/1000.0,
                      (double)loc_rbu_domain_stat_avg[i].wait_max_time/1000.0,
                      loc_rbu_domain_stat_avg[i].wait_counts,
                      rbu_domain_stat_data_local_gp_ptr->wait_time_gp.avg_time,
                      rbu_domain_stat_data_local_gp_ptr->wait_time_gp.min_time,
                      rbu_domain_stat_data_local_gp_ptr->wait_time_gp.max_time,
                      rbu_domain_stat_data_local_gp_ptr->wait_time_gp.succ);
        }
        else
        {
          GDF_COPY_TIMES_VECTOR_DATA(rbu_domain_stat_data_gp_idx, 4, k, 0,
                      0,
                      -1,
                      0,
                      0,
                      rbu_domain_stat_data_local_gp_ptr->wait_time_gp.avg_time,
                      rbu_domain_stat_data_local_gp_ptr->wait_time_gp.min_time,
                      rbu_domain_stat_data_local_gp_ptr->wait_time_gp.max_time,
                      rbu_domain_stat_data_local_gp_ptr->wait_time_gp.succ);
        }

        // rcv time Graph
        if(loc_rbu_domain_stat_avg[i].rcv_counts > 0)
        {
          GDF_COPY_TIMES_VECTOR_DATA(rbu_domain_stat_data_gp_idx, 5, k, 0,
                      (double)(((double)loc_rbu_domain_stat_avg[i].rcv_time) /
                                 ((double)(1000.0*(double)loc_rbu_domain_stat_avg[i].rcv_counts))),
                      (double)loc_rbu_domain_stat_avg[i].rcv_min_time/1000.0,
                      (double)loc_rbu_domain_stat_avg[i].rcv_max_time/1000.0,
                      loc_rbu_domain_stat_avg[i].rcv_counts,
                      rbu_domain_stat_data_local_gp_ptr->rcv_time_gp.avg_time,
                      rbu_domain_stat_data_local_gp_ptr->rcv_time_gp.min_time,
                      rbu_domain_stat_data_local_gp_ptr->rcv_time_gp.max_time,
                      rbu_domain_stat_data_local_gp_ptr->rcv_time_gp.succ);
        }
        else
        {
          GDF_COPY_TIMES_VECTOR_DATA(rbu_domain_stat_data_gp_idx, 5, k, 0,
                      0,
                      -1,
                      0,
                      0,
                      rbu_domain_stat_data_local_gp_ptr->rcv_time_gp.avg_time,
                      rbu_domain_stat_data_local_gp_ptr->rcv_time_gp.min_time,
                      rbu_domain_stat_data_local_gp_ptr->rcv_time_gp.max_time,
                      rbu_domain_stat_data_local_gp_ptr->rcv_time_gp.succ);
        }

        // blckd time Graph
        if(loc_rbu_domain_stat_avg[i].blckd_counts > 0)
        {
          GDF_COPY_TIMES_VECTOR_DATA(rbu_domain_stat_data_gp_idx, 6, k, 0,
                      (double)(((double)loc_rbu_domain_stat_avg[i].blckd_time) /
                                 ((double)(1000.0*(double)loc_rbu_domain_stat_avg[i].blckd_counts))),
                      (double)loc_rbu_domain_stat_avg[i].blckd_min_time/1000.0,
                      (double)loc_rbu_domain_stat_avg[i].blckd_max_time/1000.0,
                      loc_rbu_domain_stat_avg[i].blckd_counts,
                      rbu_domain_stat_data_local_gp_ptr->blckd_time_gp.avg_time,
                      rbu_domain_stat_data_local_gp_ptr->blckd_time_gp.min_time,
                      rbu_domain_stat_data_local_gp_ptr->blckd_time_gp.max_time,
                      rbu_domain_stat_data_local_gp_ptr->blckd_time_gp.succ);
        }
        else
        {
          GDF_COPY_TIMES_VECTOR_DATA(rbu_domain_stat_data_gp_idx, 6, k, 0,
                      0,
                      -1,
                      0,
                      0,
                      rbu_domain_stat_data_local_gp_ptr->blckd_time_gp.avg_time,
                      rbu_domain_stat_data_local_gp_ptr->blckd_time_gp.min_time,
                      rbu_domain_stat_data_local_gp_ptr->blckd_time_gp.max_time,
                      rbu_domain_stat_data_local_gp_ptr->blckd_time_gp.succ);
        }

        // url_resp time Graph
        if(loc_rbu_domain_stat_avg[i].url_resp_counts > 0)
        {
          GDF_COPY_TIMES_VECTOR_DATA(rbu_domain_stat_data_gp_idx, 7, k, 0,
                      (double)(((double)loc_rbu_domain_stat_avg[i].url_resp_time) /
                                 ((double)(1000.0*(double)loc_rbu_domain_stat_avg[i].url_resp_counts))),
                      (double)loc_rbu_domain_stat_avg[i].url_resp_min_time/1000.0,
                      (double)loc_rbu_domain_stat_avg[i].url_resp_max_time/1000.0,
                      loc_rbu_domain_stat_avg[i].url_resp_counts,
                      rbu_domain_stat_data_local_gp_ptr->url_resp_time_gp.avg_time,
                      rbu_domain_stat_data_local_gp_ptr->url_resp_time_gp.min_time,
                      rbu_domain_stat_data_local_gp_ptr->url_resp_time_gp.max_time,
                      rbu_domain_stat_data_local_gp_ptr->url_resp_time_gp.succ);
        }
        else
        {
          GDF_COPY_TIMES_VECTOR_DATA(rbu_domain_stat_data_gp_idx, 7, k, 0,
                      0,
                      -1,
                      0,
                      0,
                      rbu_domain_stat_data_local_gp_ptr->url_resp_time_gp.avg_time,
                      rbu_domain_stat_data_local_gp_ptr->url_resp_time_gp.min_time,
                      rbu_domain_stat_data_local_gp_ptr->url_resp_time_gp.max_time,
                      rbu_domain_stat_data_local_gp_ptr->url_resp_time_gp.succ);
        }

        // Num request Graph
        if(loc_rbu_domain_stat_avg[i].num_req_counts > 0)
        {
          GDF_COPY_TIMES_VECTOR_DATA(rbu_domain_stat_data_gp_idx, 8, k, 0,
                      (double)(((double)loc_rbu_domain_stat_avg[i].num_req) /
                                 ((double)(1000.0*(double)loc_rbu_domain_stat_avg[i].num_req_counts)) *1000),
                      (double)loc_rbu_domain_stat_avg[i].num_req_min,
                      (double)loc_rbu_domain_stat_avg[i].num_req_max,
                      loc_rbu_domain_stat_avg[i].num_req_counts,
                      rbu_domain_stat_data_local_gp_ptr->num_req_gp.avg_time,
                      rbu_domain_stat_data_local_gp_ptr->num_req_gp.min_time,
                      rbu_domain_stat_data_local_gp_ptr->num_req_gp.max_time,
                      rbu_domain_stat_data_local_gp_ptr->num_req_gp.succ);
        }
        else
        {
          GDF_COPY_TIMES_VECTOR_DATA(rbu_domain_stat_data_gp_idx, 8, k, 0,
                      0,
                      -1,
                      0,
                      0,
                      rbu_domain_stat_data_local_gp_ptr->num_req_gp.avg_time,
                      rbu_domain_stat_data_local_gp_ptr->num_req_gp.min_time,
                      rbu_domain_stat_data_local_gp_ptr->num_req_gp.max_time,
                      rbu_domain_stat_data_local_gp_ptr->num_req_gp.succ);
        }

        NSDL2_RBU(NULL, NULL, "Method end: Fill rbu_domain_stat_data_local_gp_ptr for Domain id = %d, "
                              "dns_time = %f, tcp_time = %f,  ssl_time = %f, connect_time = %f, "
                              "wait_time = %f, rcv_time = %f, blckd_time =%f, url_resp_time = %f",
                               i, loc_rbu_domain_stat_avg[i].dns_time, loc_rbu_domain_stat_avg[i].tcp_time,
                               loc_rbu_domain_stat_avg[i].ssl_time,
                               loc_rbu_domain_stat_avg[i].connect_time, loc_rbu_domain_stat_avg[i].wait_time,
                               loc_rbu_domain_stat_avg[i].rcv_time,
                               loc_rbu_domain_stat_avg[i].blckd_time, loc_rbu_domain_stat_avg[i].url_resp_time);
    }
  }   
}

void printRbuGraph(char **TwoD , int *Idx2d, char *prefix, int groupId, int genId)
{
  char buff[1024 + 16];
  char vector_name[1024];
  char *name;
  int i = 0;
  int dyn_obj_idx, count;

  NSDL2_GDF(NULL, NULL, " Method called. Idx2d = %d, prefix = %s, genId = %d, groupId = %d", *Idx2d, prefix, genId, groupId);

  if(groupId == RBU_DOMAIN_STAT_GRP_ID)
  {
    dyn_obj_idx = NEW_OBJECT_DISCOVERY_RBU_DOMAIN;
    count = dynObjForGdf[dyn_obj_idx].total + dynObjForGdf[dyn_obj_idx].startId;
  }

  for(i = 0; i < count; i++)
  {
    name = nslb_get_norm_table_data(dynObjForGdf[dyn_obj_idx].normTable, i);

    if(g_runtime_flag == 0)
    {
      dynObjForGdf[dyn_obj_idx].rtg_index_tbl[genId][i] = msg_data_size + ((dynObjForGdf[dyn_obj_idx].rtg_group_size) * (*Idx2d));
      NSDL2_GDF(NULL, NULL, "RTG index set for NS/NC Controller/GeneratorId = %d, and DomainName = %s is %d. Index of DynObjForGdf = %d", genId, name, dynObjForGdf[dyn_obj_idx].rtg_index_tbl[genId][i], dyn_obj_idx);
    }

    sprintf(vector_name, "%s%s", prefix, name);
    sprintf(buff, "%s %d", vector_name, dynObjForGdf[dyn_obj_idx].rtg_index_tbl[genId][i]);
    fprintf(write_gdf_fp, "%s\n", buff);

    fill_2d(TwoD, *Idx2d, vector_name);
    *Idx2d = *Idx2d  + 1;
    NSDL2_GDF(NULL, NULL, "Idx2d = %d", *Idx2d);
  }
}

//This method will be called at partition switch
char **printRbuDomainStat(int groupId)
{
  int i = 0;
  char **TwoD;
  char prefix[1024];
  int Idx2d = 0, dyn_obj_idx = 0;

  NSDL2_GDF(NULL, NULL, "Method called, total_rbu_domain_entries = %d", total_rbu_domain_entries);

  int total_rbu_entry = total_rbu_domain_entries * (sgrp_used_genrator_entries + 1);
  TwoD = init_2d(total_rbu_entry);
  NSDL2_GDF(NULL, NULL, "Method Called");

  for(i=0; i < sgrp_used_genrator_entries + 1; i++)
  {
    getNCPrefix(prefix, i-1, -1, ">", 0); //for controller or NS as grp_data_flag is disabled and grp index fixed
    NSDL2_GDF(NULL, NULL, "in trans prefix is = %s", prefix);
    printRbuGraph(TwoD, &Idx2d, prefix, groupId, i);
  }
  if(groupId == RBU_DOMAIN_STAT_GRP_ID)
    dyn_obj_idx = NEW_OBJECT_DISCOVERY_RBU_DOMAIN;

  msg_data_size = msg_data_size + ((dynObjForGdf[dyn_obj_idx].rtg_group_size) * (sgrp_used_genrator_entries));

  return TwoD;
}



