/***************************************************************************
 Name		: ns_socket_udp_client_failures_rpt.c 
 Purpose	: This file will contain all functions related to UDP Client 
                  Failures graph.
                  Following metrices will be handle -
                    
 Design		:
 Author(s)	: Manish Mishra, 18 Aug 2020
 Mod. Hist.	:
***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "url.h"
#include "util.h"
#include "ns_common.h"
#include "ns_global_settings.h"
#include "ns_log.h"
#include "ns_script_parse.h"
#include "ns_http_process_resp.h"
#include "ns_log_req_rep.h"
#include "ns_string.h"
#include "ns_url_req.h"
#include "ns_vuser_tasks.h"
#include "ns_http_script_parse.h"
#include "ns_page_dump.h"
#include "ns_vuser_thread.h" 
#include "nslb_encode_decode.h"
#include "netstorm.h"
#include "ns_sock_com.h"
#include "ns_debug_trace.h"
#include "ns_trace_level.h"
#include "ns_gdf.h"

#include "tmr.h"
#include "timing.h"
#include "logging.h"
#include "ns_schedule_phases.h"
#include "wait_forever.h"
#include "ns_group_data.h"
#include "ns_msg_com_util.h"
#include "ns_dynamic_avg_time.h"

#include "ns_socket.h"

//Global variables ------------------
//UDP Client Failures
#ifndef CAV_MAIN
int g_udp_client_failures_avg_idx = -1;                           // Index into avgtime
NormObjKey g_udp_client_errs_normtbl;                             // Normlization table 
int g_total_udp_client_errs = 0;
UDPClientFailureAvgTime *g_udp_client_failures_avg = NULL;
#else
__thread int g_udp_client_failures_avg_idx = -1;                           // Index into avgtime
__thread NormObjKey g_udp_client_errs_normtbl;                             // Normlization table 
__thread int g_total_udp_client_errs = 0;
__thread UDPClientFailureAvgTime *g_udp_client_failures_avg = NULL;
#endif
int g_udp_client_failures_cavg_idx = -1;                          // Index into cavgtime
UDPClientFailureCAvgTime *g_udp_client_failures_cavg = NULL;

int g_udp_clinet_failures_rpt_group_idx = -1;                     // Index into Group_Info table 
UDPClientFailureRTGData *g_udp_clinet_failures_rtg_ptr = NULL;    // RTG pointer for UDP Client 

Local2Norm *g_udp_clinet_errs_loc2normtbl;

int g_max_total_udp_client_errs = 0;

/*====================================================================
  Name		: create_udp_client_errs_loc2normtbl() 

  Design	: Create normalize id mapping table for Parent
             
                   Suppose we have following error/codes -
                             ++++++++++++++++++++++++++++++++++++
                    errs =   | EC-1 | EC-2 | EC-3 | EC-4 | EC-5 |
                             ++++++++++++++++++++++++++++++++++++
                              0      1      2      3      4 
             
                   On NVM0, norm ids are -  
                             +++++++++++++++
                    normid = | EC-5 | EC-1 |
                             +++++++++++++++
                              0      1      
                   
                   On NVM1, norm ids are -  
                             +++++++++++++++
                    normid = | EC-4 | EC-3 |
                             +++++++++++++++
                              0      1      
             
                   Then on Parent mapping table will be -
             
                   loc2norm[ ]
                            |   +++++++++
                      nvm  [0]->| 4 | 0 |
                            |   +++++++++
                            |    0   1      
                            | 
                            |   +++++++++
                      nvm  [1]->| 3 | 2 |
                                +++++++++
                                 0   1      
 
=====================================================================*/
void create_udp_client_errs_loc2normtbl(int entries)
{
  int nvmindex = 0;
  int num_process;
  int i;

  Local2Norm *l2n;

  NSDL2_SOCKETS(NULL, NULL, "Method called , entries = %d global_settings->num_process = %d, "
      "sgrp_used_genrator_entries = %d, g_udp_clinet_errs_loc2normtbl = %p",
       entries, global_settings->num_process, sgrp_used_genrator_entries, g_udp_clinet_errs_loc2normtbl);

  num_process = ((loader_opcode == MASTER_LOADER)?sgrp_used_genrator_entries:global_settings->num_process);

  MY_MALLOC_AND_MEMSET(g_udp_clinet_errs_loc2normtbl, num_process * sizeof(Local2Norm), "UDPClientLocal2NormTbl", (int)-1);

  for(nvmindex = 0; nvmindex < num_process; nvmindex++)
  {
    l2n = &g_udp_clinet_errs_loc2normtbl[nvmindex];

    MY_MALLOC_AND_MEMSET_WITH_MINUS_ONE(l2n->loc2norm, entries * sizeof(int), 
        "UDPClientLocal2NormTbl-loc2norm array", (int)-1); 

    l2n->loc2norm_size = entries;
    l2n->avg_idx = g_udp_client_failures_avg_idx; 
    l2n->tot_entries = entries; 

    for(i = 0; i < entries; i++)
      l2n->loc2norm[i] = i;
  }

  NSDL2_SOCKETS(NULL, NULL, "Method End");
}

//Function Definitions ---------------
void update_avgtime_size_for_udp_client_failures() 
{
  int socket_avgsize = sizeof(UDPClientFailureAvgTime);

  NSDL2_SOCKETS(NULL, NULL, "Method Called, protocol_enabled = %0x, "
               "g_avgtime_size = %d, g_udp_client_failures_avg_idx = %d, "
               "SocketAvgTime size = %d",
                global_settings->protocol_enabled, g_avgtime_size, 
                g_udp_client_failures_avg_idx, socket_avgsize);

  /*====================================================================
    Update g_avgtime_size by size of UDPClientFailureAvgTime 
  =====================================================================*/
  g_udp_client_failures_avg_idx = g_avgtime_size;
  g_avgtime_size += socket_avgsize;

  /*====================================================================
    Create Normlization table for UDP Client Errors 
    Parent and NVM have their own normalization tables
  =====================================================================*/
  nslb_init_norm_id_table_ex(&g_udp_client_errs_normtbl, INIT_SOCKET_MAX_ERRORS);

  /*====================================================================
    Create mapping table to map NVM's norm id into Parent's norm id
  =====================================================================*/
  create_udp_client_errs_loc2normtbl(g_total_udp_client_errs);

  NSDL2_SOCKETS(NULL, NULL, "AvgIndex for Sockect API done, g_avgtime_size = %d, "
      "g_udp_client_failures_avg_idx = %d", g_avgtime_size, g_udp_client_failures_avg_idx);
}

void update_cavgtime_size_for_udp_client_failures() 
{
  #ifndef CAV_MAIN
  int socket_cavgsize = sizeof(UDPClientFailureCAvgTime);

  NSDL2_SOCKETS(NULL, NULL, "Method Called, protocol_enabled = %0x, "
               "g_avgtime_size = %d, g_udp_client_failures_cavg_idx = %d, "
               "SocketAvgTime size = %d",
                global_settings->protocol_enabled, g_cavgtime_size, 
                g_udp_client_failures_cavg_idx, socket_cavgsize);

  g_udp_client_failures_cavg_idx = g_cavgtime_size;
  g_cavgtime_size += socket_cavgsize;

  NSDL2_SOCKETS(NULL, NULL, "CAvgIndex for Sockect API done, g_avgtime_size = %d, "
           "g_udp_client_failures_cavg_idx = %d",
            g_cavgtime_size, g_udp_client_failures_cavg_idx);
  #endif
}

void set_udp_client_failures_avg_ptr() 
{
  if(IS_UDP_CLIENT_API_EXIST) 
  {
    g_udp_client_failures_avg = (UDPClientFailureAvgTime *)((char *)average_time + g_udp_client_failures_avg_idx);

    NSDL2_SOCKETS(NULL, NULL, "After setting g_udp_client_failures_avg = %p, "
        "g_udp_client_failures_avg_idx = %d", 
         g_udp_client_failures_avg, g_udp_client_failures_avg_idx);
  } 
  else 
  {
    NSDL2_SOCKETS(NULL, NULL, "Socket Protocol For UDP is disabled.");
    g_udp_client_failures_avg = NULL;
  }
}

int ns_add_dyn_udp_client_failures(short nvmindex, int local_norm_id, char *error_msg, short error_len, int *flag_new)
{ 
  int norm_id, num_process, i;
  Local2Norm *l2n = &g_udp_clinet_errs_loc2normtbl[nvmindex];
 
   NSDL1_SOCKETS(NULL, NULL, "Method called, nvmindex = %d, local_norm_id = %d, "
       "error_msg = %s, error_len = %d, loc2norm_size = %d",
       nvmindex, local_norm_id, error_msg, error_len, l2n->loc2norm_size);

   num_process = ((loader_opcode == MASTER_LOADER) ? sgrp_used_genrator_entries : global_settings->num_process);
 
   if(!error_msg || nvmindex < 0 || local_norm_id < 0)
   {
     NSTL1(NULL, NULL, "Status Code addition failed (Error: Invalid argument) "
         "error_msg = %s OR nvmindex = %d OR local_norm_id = %d", 
          error_msg, nvmindex, local_norm_id);
     return 0;    //for any error case, setting '0' i.e., Others
   }

   // Realloc loc2norm table
   if (g_total_udp_client_errs >= l2n->loc2norm_size)
   {
     int old_size = l2n->loc2norm_size * sizeof(int);
     int new_size = (g_total_udp_client_errs + DELTA_TCP_CLIENT_ERR_LOC2NORM_SIZE) * sizeof(int);
   
     NSTL1(NULL, NULL, "Reallocating Local2Norm table for UDP Client Failures, "
         "NVM:%d, old_size = %d, new_size = %d",
          nvmindex, old_size, new_size);     

     //MM: WHY allocating memory for all NVMs ???
     for(i = 0; i < num_process; i++)
     { 
       MY_REALLOC_AND_MEMSET_WITH_MINUS_ONE(g_udp_clinet_errs_loc2normtbl[i].loc2norm, 
           new_size, old_size, "UDPClientLocal2NormTbl-loc2norm array", i);   

       g_udp_clinet_errs_loc2normtbl[i].loc2norm_size = g_total_udp_client_errs + DELTA_TCP_CLIENT_ERR_LOC2NORM_SIZE;
     }
   }
   
   norm_id = nslb_get_or_set_norm_id(&g_udp_client_errs_normtbl, error_msg, error_len, flag_new);

   if(l2n->loc2norm[norm_id] == -1)
   {
     l2n->tot_entries++;
     l2n->tot_dyn_entries[LOC2NORM_NON_CUMM_IDX]++;
     l2n->tot_dyn_entries[LOC2NORM_CUMM_IDX]++;
   }

   l2n->loc2norm[local_norm_id] = norm_id;
   
   if(*flag_new)
   {
     dynObjForGdf[NEW_OBJECT_DISCOVERY_UDP_CLIENT_FAILURES].total++;  //It will store total discovered status codes btwn every progress interval
     check_if_realloc_needed_for_dyn_obj(NEW_OBJECT_DISCOVERY_UDP_CLIENT_FAILURES);
   } 
   
   NSTL1(NULL, NULL, "UDPClientFailures: on new discovery for NVM:%d, "
       "error_msg = %s, l2n->loc2norm[%d] = %d, tot_entries = %d, "
       "tot_dyn_entries = %d, dynObjForGdf total = %d", 
        nvmindex, error_msg, local_norm_id, norm_id, l2n->loc2norm[local_norm_id],
        l2n->tot_entries,
        l2n->tot_dyn_entries, 
        dynObjForGdf[NEW_OBJECT_DISCOVERY_UDP_CLIENT_FAILURES].total);

   return norm_id;
}

void fill_udp_client_failure_avg(VUser *vptr, int err_code)
{
  int norm_id; 
  int is_new_err;
  int row_num = 0;
  int len;
  ErrorCodeTableEntry_Shr *err;

  err = &errorcode_table_shr_mem[err_code];
  len = strlen(err->error_msg);

  NSDL1_SOCKETS(vptr, NULL, "Method called, err_code = %d, error_msg = %s, error len = %d, "
      "g_udp_client_failures_avg = %p, g_udp_client_failures_avg_idx = %d", 
      err_code, err->error_msg, len, g_udp_client_failures_avg, g_udp_client_failures_avg_idx);

  norm_id = nslb_get_or_set_norm_id(&g_udp_client_errs_normtbl, err->error_msg, len, &is_new_err);  
 
  if(is_new_err)
  {
    int g_avgtime_size_prev = g_avgtime_size;

    create_dynamic_data_avg(&g_child_msg_com_con, &row_num, my_port_index, NEW_OBJECT_DISCOVERY_UDP_CLIENT_FAILURES);
  
    //send local_norm_id to parent
    send_new_object_discovery_record_to_parent(vptr, len, err->error_msg, NEW_OBJECT_DISCOVERY_UDP_CLIENT_FAILURES, norm_id);

    check_if_need_to_realloc_connection_read_buf(&g_dh_child_msg_com_con, my_port_index, 
        g_avgtime_size_prev, NEW_OBJECT_DISCOVERY_UDP_CLIENT_FAILURES);
  }

  g_udp_client_failures_avg[norm_id].num_failures++;    //Increment by 1 as error found 

  NSDL1_SOCKETS(vptr, NULL, "Method exit, g_udp_client_failures_avg = %p, "
      "g_udp_client_failures_avg_idx = %d, norm_id = %d, num_failures = %d, is_new_err = %d",
       g_udp_client_failures_avg, g_udp_client_failures_avg_idx, norm_id, 
       g_udp_client_failures_avg[norm_id].num_failures, is_new_err);
}

static void print_udp_client_failures_grp_vectors_of_one_generator(char **TwoD , int *Idx2d, char *prefix, int genId)
{
  char buff[MAX_VAR_SIZE + 1];
  char vector_name[MAX_VAR_SIZE + 1]; 
  char *name;
  int i = 0;
  int dyn_obj_idx, count;
  dyn_obj_idx = 0;
  count = 0;
  int write_bytes = 0;

  NSDL2_SOCKETS(NULL, NULL, " Method called. Idx2d = %d, prefix = %s, genId = %d", *Idx2d, prefix, genId);
  
  dyn_obj_idx = NEW_OBJECT_DISCOVERY_UDP_CLIENT_FAILURES;

  count = dynObjForGdf[dyn_obj_idx].total + dynObjForGdf[dyn_obj_idx].startId;

  NSDL2_SOCKETS(NULL, NULL, "UDPClientFailures: total = %d, strtId = %d, count = %d", 
                        dynObjForGdf[dyn_obj_idx].total, dynObjForGdf[dyn_obj_idx].startId, count);
  
  for(i = 0; i < count; i++)
  { 
    name = nslb_get_norm_table_data(dynObjForGdf[dyn_obj_idx].normTable, i);
    
    if(g_runtime_flag == 0)
    { 
      dynObjForGdf[dyn_obj_idx].rtg_index_tbl[genId][i] = msg_data_size + 
          ((dynObjForGdf[dyn_obj_idx].rtg_group_size) * (*Idx2d));     

      NSDL2_SOCKETS(NULL, NULL, "RTG index set for NS/NC Controller/GeneratorId = %d, "
          "and Name = %s is %d. Index of DynObjForGdf = %d", 
           genId, name, dynObjForGdf[dyn_obj_idx].rtg_index_tbl[genId][i], dyn_obj_idx);
    }

    write_bytes = snprintf(vector_name, MAX_VAR_SIZE, "%s%s", prefix, name);
    vector_name[write_bytes] = 0;

    write_bytes = snprintf(buff, MAX_VAR_SIZE, "%s %d", vector_name, dynObjForGdf[dyn_obj_idx].rtg_index_tbl[genId][i]);
    buff[write_bytes] = 0;

    fprintf(write_gdf_fp, "%s\n", buff);
    
    fill_2d(TwoD, *Idx2d, vector_name);
    *Idx2d = *Idx2d  + 1; 
    NSDL2_SOCKETS(NULL, NULL, "Idx2d = %d", *Idx2d);
  }
}

char **print_udp_client_failures_grp_vectors()
{
  int i = 0;
  char **TwoD;
  char prefix[1024];
  int Idx2d = 0;

  NSDL2_SOCKETS(NULL, NULL, "Method called, total_discovered_status_code = %d", g_total_udp_client_errs);

  int num_vectors = g_total_udp_client_errs * (sgrp_used_genrator_entries + 1);

  TwoD = init_2d(num_vectors);

  for(i=0; i < sgrp_used_genrator_entries + 1; i++)
  {
    getNCPrefix(prefix, i-1, -1, ">", 0); //for controller or NS as grp_data_flag is disabled and grp index fixed
    NSDL2_SOCKETS(NULL, NULL, "in trans prefix is = %s", prefix);
    print_udp_client_failures_grp_vectors_of_one_generator(TwoD, &Idx2d, prefix, i);
  }

  msg_data_size = msg_data_size + ((dynObjForGdf[NEW_OBJECT_DISCOVERY_UDP_CLIENT_FAILURES].rtg_group_size) * (sgrp_used_genrator_entries));

  return TwoD;
}

void fill_udp_client_failures_gp(avgtime **g_avg, cavgtime **g_cavg)
{
  int i;
  int group_vect_idx = 0;   // GDF group vector index, Here generators are Group vectors
  //int sgrp_idx = 0;         // SGRP group index
  int graph_idx = 0;        // GDF graph index, DONOT rename it as used in below MACROS 
  int rtg_idx = g_udp_clinet_failures_rpt_group_idx;

  UDPClientFailureAvgTime *avg_ptr = NULL;
  UDPClientFailureCAvgTime *cavg_ptr = NULL;
  UDPClientFailureRTGData *rtg_ptr = NULL;
  
  NSDL4_SOCKETS(NULL, NULL, "Method called, g_total_udp_client_errs = %d", g_total_udp_client_errs);

  if(!IS_UDP_CLIENT_API_EXIST)
  {
    NSDL2_SOCKETS(NULL, NULL, "Socket UDP Client API does not exist hence returning.");  
    return;
  }

  //When no request is involved in test, ignore filling data, init is already ignored with other dyn objects
  //if(!g_dont_skip_test_metrics)
    //return;

  for(group_vect_idx = 0; group_vect_idx < sgrp_used_genrator_entries + 1; group_vect_idx++)
  {
    avg_ptr = (UDPClientFailureAvgTime *)((char *)g_avg[group_vect_idx] + g_udp_client_failures_avg_idx);
    cavg_ptr = (UDPClientFailureCAvgTime *)((char *)g_cavg[group_vect_idx] + g_udp_client_failures_cavg_idx);

    for (i = 0; i < g_total_udp_client_errs; i++) 
    {
       rtg_idx = dynObjForGdf[NEW_OBJECT_DISCOVERY_UDP_CLIENT_FAILURES].rtg_index_tbl[group_vect_idx][i]; 

       NSDL2_SOCKETS(NULL, NULL, "[SocketStats-UDPClientFailure] Fill UDPClienFailure metrics, "
           "i = %d, group_vect_idx = %d, graph_idx = %d, rtg_idx = %d, "
           "g_udp_client_failures_avg_idx = %d, avg_ptr = %p, "
           "g_udp_client_failures_cavg_idx = %d, cavg_ptr = %p", 
            i, group_vect_idx, graph_idx, rtg_idx, g_udp_client_failures_avg_idx, 
            avg_ptr, g_udp_client_failures_cavg_idx, cavg_ptr);

      if(rtg_idx < 0)
        continue;

      rtg_ptr = (UDPClientFailureRTGData *)((char *)msg_data_ptr + rtg_idx);

      PARENT_COPY_SOCKET_AVG2RTG(rtg_ptr->failure_ps, avg_ptr[i].num_failures, DT_RATE);
      PARENT_COPY_SOCKET_AVG2RTG(rtg_ptr->failure_tot, cavg_ptr[i].cum_num_failures, DT_CUM);

      graph_idx = 0;
      rtg_ptr++;
    }
  }
}
