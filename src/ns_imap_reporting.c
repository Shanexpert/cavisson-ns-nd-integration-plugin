#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ns_error_codes.h"
#include "netomni/src/core/ni_scenario_distribution.h"
#include "ns_msg_def.h"
#include "ns_gdf.h"
#include "ns_log.h"
#include "url.h"
#include "ns_imap.h"
#include "ns_group_data.h"
#include "ns_gdf.h"

static double convert_long_ps (Long_data row_data)
{
  NSDL2_GDF(NULL, NULL, "Method called, row_data = %lu", row_data);
  return((double )((row_data))/((double )global_settings->progress_secs/1000.0));
}

extern int loader_opcode;
void delete_imap_timeout_timer(connection *cptr) {

  NSDL2_IMAP(NULL, cptr, "Method called, timer type = %d", cptr->timer_ptr->timer_type);

  if ( cptr->timer_ptr->timer_type == AB_TIMEOUT_IDLE ) {
    NSDL2_IMAP(NULL, cptr, "Deleting Idle timer.");
    dis_timer_del(cptr->timer_ptr);
  }
}

void imap_timeout_handle( ClientData client_data, u_ns_ts_t now ) {

  connection* cptr;
  cptr = (connection *)client_data.p;
  VUser* vptr = cptr->vptr;

  NSDL2_IMAP(vptr, cptr, "Method Called, vptr=%p cptr=%p conn state=%d, request_type = %d", 
             vptr, cptr, cptr->conn_state,
            cptr->url_num->request_type);

  if(vptr->vuser_state == NS_VUSER_PAUSED){
    vptr->flags |= NS_VPTR_FLAGS_TIMER_PENDING;
    return;
  }
  Close_connection( cptr , 0, now, NS_REQUEST_TIMEOUT, NS_COMPLETION_TIMEOUT);

}

static inline void fill_imap_net_throughput_gp (avgtime **g_avg, cavgtime **g_cavg)
{
  double result;
  int g_idx = 0, v_idx, grp_idx;
  IMAPAvgTime *imap_avg;
  IMAPCAvgTime *imap_cavg;
  avgtime *avg = NULL;
  cavgtime *cavg = NULL;
  IMAP_Net_throughput_gp *imap_net_throughput_local_gp_ptr = imap_net_throughput_gp_ptr;

  NSDL2_GDF(NULL, NULL, "Method called");
  for(v_idx = 0; v_idx < sgrp_used_genrator_entries + 1; v_idx++)
  {
    avg = (avgtime *)g_avg[v_idx];
    cavg = (cavgtime *)g_cavg[v_idx];
    for(grp_idx = 0; grp_idx < TOTAL_GRP_ENTERIES_WITH_GRP_KW; grp_idx++)
    {
      /*bug id 101742: using method convert_kbps() */
      imap_avg = (IMAPAvgTime*)((char*)((char *)avg + (grp_idx * g_avg_size_only_grp)) + g_imap_avgtime_idx);
      imap_cavg = (IMAPCAvgTime *)((char*)((char *)cavg + (grp_idx * g_cavg_size_only_grp)) + g_imap_cavgtime_idx);

      result = (imap_avg->imap_tx_bytes)*8/((double)global_settings->progress_secs/1000);

      NSDL2_GDF(NULL, NULL, "IMAP Send Throughput result = %f ", result);
      NSDL2_GDF(NULL, NULL, "IMAP Send Throughput after kbps result = %f ", convert_kbps(result));

      GDF_COPY_VECTOR_DATA(imap_net_throughput_gp_idx, g_idx, v_idx, 0,
                           convert_kbps(result), imap_net_throughput_local_gp_ptr->avg_send_throughput); g_idx++;
      GDF_COPY_VECTOR_DATA(imap_net_throughput_gp_idx, g_idx, v_idx, 0,
                           imap_cavg->imap_c_tot_tx_bytes, imap_net_throughput_local_gp_ptr->tot_send_bytes); g_idx++;
     
      result = (imap_avg->imap_rx_bytes)*8/((double)global_settings->progress_secs/1000);

      NSDL2_GDF(NULL, NULL, "IMAP Receive Throughput result = %f ", result);
      NSDL2_GDF(NULL, NULL, "IMAP Receive Throughput after kbps result = %f ", convert_kbps(result));

      GDF_COPY_VECTOR_DATA(imap_net_throughput_gp_idx, g_idx, v_idx, 0,
                           convert_kbps(result), imap_net_throughput_local_gp_ptr->avg_recv_throughput); g_idx++;
      GDF_COPY_VECTOR_DATA(imap_net_throughput_gp_idx, g_idx, v_idx, 0,
                           imap_cavg->imap_c_tot_rx_bytes, imap_net_throughput_local_gp_ptr->tot_recv_bytes); g_idx++;
     
      result = (imap_avg->imap_num_con_succ)/((double)global_settings->progress_secs/1000);
      GDF_COPY_VECTOR_DATA(imap_net_throughput_gp_idx, g_idx, v_idx, 0,
                           result, imap_net_throughput_local_gp_ptr->avg_open_cps); g_idx++;
     
      GDF_COPY_VECTOR_DATA(imap_net_throughput_gp_idx, g_idx, v_idx, 0,
                           imap_cavg->imap_c_num_con_succ, imap_net_throughput_local_gp_ptr->tot_conn_open); g_idx++;
     
      result = (imap_avg->imap_num_con_break)/((double)global_settings->progress_secs/1000);
      GDF_COPY_VECTOR_DATA(imap_net_throughput_gp_idx, g_idx, v_idx, 0,
                           result, imap_net_throughput_local_gp_ptr->avg_close_cps); g_idx++;
     
      GDF_COPY_VECTOR_DATA(imap_net_throughput_gp_idx, g_idx, v_idx, 0,
                           imap_cavg->imap_c_num_con_break, imap_net_throughput_local_gp_ptr->tot_conn_close); g_idx++;
      g_idx = 0;
      imap_net_throughput_local_gp_ptr++;
    }
  }
}

static inline void fill_imap_hits_gp (avgtime **g_avg, cavgtime **g_cavg)
{  
  int g_idx = 0, v_idx, grp_idx;
  IMAPAvgTime *imap_avg = NULL;
  IMAPCAvgTime *imap_cavg = NULL;
  avgtime *avg = NULL;
  cavgtime *cavg = NULL;
  IMAP_hits_gp *imap_hits_local_gp_ptr = imap_hits_gp_ptr;

  NSDL2_GDF(NULL, NULL, "Method called");

  for(v_idx = 0; v_idx < sgrp_used_genrator_entries + 1; v_idx++)
  {
    avg = (avgtime *)g_avg[v_idx];
    cavg = (cavgtime *)g_cavg[v_idx];
    for(grp_idx = 0; grp_idx < TOTAL_GRP_ENTERIES_WITH_GRP_KW; grp_idx++)
    {
      /*bug id 101742: using method convert_long_ps() */

      imap_avg = (IMAPAvgTime*)((char*)((char *)avg + (grp_idx * g_avg_size_only_grp)) + g_imap_avgtime_idx);
      imap_cavg = (IMAPCAvgTime *)((char*)((char *)cavg + (grp_idx * g_cavg_size_only_grp)) + g_imap_cavgtime_idx);

      NSDL2_GDF(NULL, NULL, "IMAP Request Sent/Sec imap_avg->imap_fetches_started = %lu ", imap_avg->imap_fetches_started);
      NSDL2_GDF(NULL, NULL, "IMAP Request Sent/Sec after PS imap_avg->imap_fetches_started = %lu ", convert_long_ps(imap_avg->imap_fetches_started));

      GDF_COPY_VECTOR_DATA(imap_hits_gp_idx, g_idx, v_idx, 0,
                           convert_long_ps(imap_avg->imap_fetches_started), imap_hits_local_gp_ptr->url_req); g_idx++;
     
      NSDL2_GDF(NULL, NULL, "IMAP Hits/Sec imap_avg->imap_num_tries = %lu ", imap_avg->imap_num_tries);
      NSDL2_GDF(NULL, NULL, "IMAP Hits/Sec after PS imap_avg->imap_num_tries = %lu ", convert_long_ps(imap_avg->imap_num_tries));

      GDF_COPY_VECTOR_DATA(imap_hits_gp_idx, g_idx, v_idx, 0,
                           convert_long_ps(imap_avg->imap_num_tries), imap_hits_local_gp_ptr->tries); g_idx++;
    
      NSDL2_GDF(NULL, NULL, "Success IMAP Responses/Sec imap_avg->imap_num_hits = %lu ", imap_avg->imap_num_hits);
      NSDL2_GDF(NULL, NULL, "Success IMAP Responses/Sec after PS imap_avg->imap_num_hits = %lu ", convert_long_ps(imap_avg->imap_num_hits));
 
      GDF_COPY_VECTOR_DATA(imap_hits_gp_idx, g_idx, v_idx, 0,
                           convert_long_ps(imap_avg->imap_num_hits), imap_hits_local_gp_ptr->succ); g_idx++;
     
      // Here the "response" variable of `Times_data` data type is getting filled
     
      //Overall
      GDF_COPY_TIMES_VECTOR_DATA(imap_hits_gp_idx, g_idx, v_idx, 0,
                                 imap_avg->imap_overall_avg_time, imap_avg->imap_overall_min_time, 
                                 imap_avg->imap_overall_max_time, imap_avg->imap_num_tries,
                                 imap_hits_local_gp_ptr->response.avg_time,
                                 imap_hits_local_gp_ptr->response.min_time,
                                 imap_hits_local_gp_ptr->response.max_time,
                                 imap_hits_local_gp_ptr->response.succ); g_idx++;
      //Success
      if(imap_avg->imap_num_hits)
      {
        GDF_COPY_TIMES_VECTOR_DATA(imap_hits_gp_idx, g_idx, v_idx, 0,
                                   imap_avg->imap_avg_time, imap_avg->imap_min_time, imap_avg->imap_max_time, imap_avg->imap_num_hits,
                                   imap_hits_local_gp_ptr->succ_response.avg_time,
                                   imap_hits_local_gp_ptr->succ_response.min_time,
                                   imap_hits_local_gp_ptr->succ_response.max_time,
                                   imap_hits_local_gp_ptr->succ_response.succ); g_idx++;
      }
      else
      {
        GDF_COPY_TIMES_VECTOR_DATA(imap_hits_gp_idx, g_idx, v_idx, 0,
                                   0, -1, 0, 0, 
                                   imap_hits_local_gp_ptr->succ_response.avg_time,
                                   imap_hits_local_gp_ptr->succ_response.min_time,
                                   imap_hits_local_gp_ptr->succ_response.max_time,
                                   imap_hits_local_gp_ptr->succ_response.succ); g_idx++;
      }           

      //Failure
      if(imap_avg->imap_num_tries - imap_avg->imap_num_hits)
      {
        GDF_COPY_TIMES_VECTOR_DATA(imap_hits_gp_idx, g_idx, v_idx, 0,
                                   imap_avg->imap_failure_avg_time, imap_avg->imap_failure_min_time, 
                                   imap_avg->imap_failure_max_time, imap_avg->imap_num_tries - imap_avg->imap_num_hits,
                                   imap_hits_local_gp_ptr->fail_response.avg_time,
                                   imap_hits_local_gp_ptr->fail_response.min_time,
                                   imap_hits_local_gp_ptr->fail_response.max_time,
                                   imap_hits_local_gp_ptr->fail_response.succ); g_idx++;
      }
      else
      {
        GDF_COPY_TIMES_VECTOR_DATA(imap_hits_gp_idx, g_idx, v_idx, 0,
                                   0, -1, 0, 0, 
                                   imap_hits_local_gp_ptr->fail_response.avg_time,
                                   imap_hits_local_gp_ptr->fail_response.min_time,
                                   imap_hits_local_gp_ptr->fail_response.max_time,
                                   imap_hits_local_gp_ptr->fail_response.succ); g_idx++;
      }

      // This need to be fixed as these two are 'long long'
      GDF_COPY_VECTOR_DATA(imap_hits_gp_idx, g_idx, v_idx, 0,
                           imap_cavg->imap_fetches_completed, imap_hits_local_gp_ptr->cum_tries); g_idx++;
     
      GDF_COPY_VECTOR_DATA(imap_hits_gp_idx, g_idx, v_idx, 0,
                           imap_cavg->imap_succ_fetches, imap_hits_local_gp_ptr->cum_succ); g_idx++;
      g_idx = 0;
      imap_hits_local_gp_ptr++;
    }
  }
}

static inline void fill_imap_fail_gp (avgtime **g_avg)
{
  unsigned int url_all_failures = 0;
  int i, v_idx, all_idx, j = 0, grp_idx;
  IMAPAvgTime *imap_avg = NULL; 
  avgtime *avg = NULL; 
  IMAP_fail_gp *imap_fail_local_gp_ptr = imap_fail_gp_ptr;
  IMAP_fail_gp *imap_fail_local_first_gp_ptr = NULL;

  NSDL2_GDF(NULL, NULL, "Method called");

  for(v_idx = 0; v_idx < sgrp_used_genrator_entries + 1; v_idx++)
  {
    avg = (avgtime *)g_avg[v_idx];
    for(grp_idx = 0; grp_idx < TOTAL_GRP_ENTERIES_WITH_GRP_KW; grp_idx++)
    {
      imap_avg = (IMAPAvgTime*)((char*)((char *)avg + (grp_idx * g_avg_size_only_grp)) + g_imap_avgtime_idx);
      all_idx = j;
      j++;
      imap_fail_local_first_gp_ptr = imap_fail_local_gp_ptr;
      imap_fail_local_gp_ptr++;
      for (i = 0; i < (TOTAL_USED_URL_ERR - 1); i++, j++)
      {
        GDF_COPY_VECTOR_DATA(imap_fail_gp_idx, 0, j, 0, imap_avg->imap_error_codes[i + 1], imap_fail_local_gp_ptr->failures);
        GDF_COPY_SCALAR_DATA(imap_fail_gp_idx, 1, imap_avg->imap_error_codes[i + 1], imap_fail_local_gp_ptr->failure_count);
        url_all_failures += imap_avg->imap_error_codes[i + 1];
        imap_fail_local_gp_ptr++;
      }
      /*bug id 101742: using method convert_long_ps() */

      NSDL2_GDF(NULL, NULL, "IMAP Failures/Sec url_all_failures = %lu ", url_all_failures);
      NSDL2_GDF(NULL, NULL, "IMAP Failures/Sec after PS url_all_failures = %lu ", convert_long_ps(url_all_failures));

      GDF_COPY_VECTOR_DATA(imap_fail_gp_idx, 0, all_idx, 0, convert_long_ps(url_all_failures), imap_fail_local_first_gp_ptr->failures);
      GDF_COPY_SCALAR_DATA(imap_fail_gp_idx, 1, url_all_failures, imap_fail_local_first_gp_ptr->failure_count);
      url_all_failures = 0;
    }
  }
}

inline void fill_imap_gp (avgtime **g_avg, cavgtime **g_cavg) {
  
  NSDL2_IMAP(NULL, NULL, "Method called. g_avg = %p", g_avg);

  if (imap_hits_gp_ptr)
    fill_imap_hits_gp (g_avg, g_cavg);
  if (imap_net_throughput_gp_ptr)
    fill_imap_net_throughput_gp (g_avg, g_cavg);
  if (imap_fail_gp_ptr)
    fill_imap_fail_gp (g_avg);
}

void
print_imap_throughput(FILE *fp1, FILE *fp2, int is_periodic, avgtime *avg, cavgtime *cavg)
{

  double imap_tcp_rx, imap_tcp_tx;
  double duration;
  IMAPAvgTime *imap_avg = NULL;
  IMAPCAvgTime *imap_cavg = NULL;

  //if(!(global_settings->protocol_enabled & IMAP_PROTOCOL_ENABLED) || !(global_settings->protocol_enabled & IMAPS_PROTOCOL_ENABLED)) {
  if(!(global_settings->protocol_enabled & IMAP_PROTOCOL_ENABLED)) {
    return; 
  }
  /* We have allocated average_time with the size of imapAvgTime
   * also now we can point that using g_imap_avgtime_idx
   */
  imap_avg = (IMAPAvgTime*)((char *)avg + g_imap_avgtime_idx);
  imap_cavg = (IMAPCAvgTime*)((char *)cavg + g_imap_cavgtime_idx);

  u_ns_8B_t  imap_tot_tcp_rx = 0, imap_tot_tcp_tx = 0;
  u_ns_8B_t imap_con_made_rate, imap_con_break_rate, imap_hit_tot_rate, imap_hit_succ_rate, imap_hit_initited_rate; 
  u_ns_8B_t imap_num_completed, imap_num_initiated, imap_num_succ; //, imap_num_samples;

  char imap_tbuffer[1024]="";
  
  NSDL2_REPORTING(NULL, NULL, "Method called");
  if (is_periodic) {
    duration = (double)((double)(global_settings->progress_secs)/1000.0);
    sprintf(imap_tbuffer, " IMAP (Rx=%'.3f)", (double)(((double)(imap_avg->imap_total_bytes))/(duration*128.0)));
    imap_tcp_rx = ((double)(imap_avg->imap_rx_bytes))/(duration*128.0);
    imap_tcp_tx = ((double)(imap_avg->imap_tx_bytes))/(duration*128.0);

    imap_con_made_rate = (imap_avg->imap_num_con_succ * 1000)/global_settings->progress_secs;
    imap_con_break_rate = (imap_avg->imap_num_con_break * 1000)/global_settings->progress_secs;
    imap_num_completed = imap_avg->imap_num_tries;
    //imap_num_samples = imap_num_succ = imap_avg->imap_num_hits;
    imap_num_succ = imap_avg->imap_num_hits;
    imap_num_initiated = imap_avg->imap_fetches_started;
    imap_hit_tot_rate = (imap_num_completed * 1000)/global_settings->progress_secs;
    imap_hit_succ_rate = (imap_num_succ * 1000)/global_settings->progress_secs;
    imap_hit_initited_rate = (imap_num_initiated * 1000)/global_settings->progress_secs;
  } else {
    duration = (double)((double)(global_settings->test_duration)/1000.0);
    sprintf(imap_tbuffer, " IMAP (Rx=%'.3f)", (double)(((double)(imap_cavg->imap_c_tot_total_bytes))/(duration*128.0)));
    imap_tcp_rx = ((double)(imap_cavg->imap_c_tot_rx_bytes))/(duration*128.0);
    imap_tcp_tx = ((double)(imap_cavg->imap_c_tot_tx_bytes))/(duration*128.0);

    imap_con_made_rate = (imap_cavg->imap_c_num_con_succ * 1000)/global_settings->test_duration;
    imap_con_break_rate = (imap_cavg->imap_c_num_con_break * 1000)/global_settings->test_duration;
    imap_num_completed = imap_cavg->imap_fetches_completed;
    //imap_num_samples = imap_num_succ = imap_cavg->imap_succ_fetches;
    imap_num_succ = imap_cavg->imap_succ_fetches;
    NSDL2_MESSAGES(NULL, NULL, "  imap_succ_fetches in imap file = %llu, no of succ in imap = %llu", imap_cavg->imap_succ_fetches , imap_num_succ); 
    imap_num_initiated = imap_cavg->cum_imap_fetches_started;
    imap_hit_tot_rate = (imap_num_completed * 1000)/global_settings->test_duration;
    imap_hit_succ_rate = (imap_num_succ * 1000)/global_settings->test_duration;
    imap_hit_initited_rate = (imap_num_initiated * 1000)/global_settings->test_duration;     
  }

  imap_tot_tcp_rx = (u_ns_8B_t)imap_cavg->imap_c_tot_rx_bytes;
  imap_tot_tcp_tx = (u_ns_8B_t)imap_cavg->imap_c_tot_tx_bytes;

  if (imap_avg->imap_num_hits) {
    if (global_settings->show_initiated)
      fprint2f(fp1, fp2, "    imap hit rate (per sec): Initiated=%'llu completed=%'llu Success=%'llu\n",
      imap_hit_initited_rate, imap_hit_tot_rate, imap_hit_succ_rate);
    else
      fprint2f(fp1, fp2, "    imap hit rate (per sec): Total=%'llu Success=%'llu\n", imap_hit_tot_rate, imap_hit_succ_rate);

/*       sprintf(ubuffer, " imap Body(Rx=%'llu)",  */
/*               (u_ns_8B_t)(avg->imap_c_tot_total_bytes)); */

    fprint2f(fp1, fp2, "    imap (Kbits/s) TCP(Rx=%'.3f Tx=%'.3f)\n",
                             imap_tcp_rx, imap_tcp_tx);
    fprint2f(fp1, fp2, "    imap (Total Bytes) TCP(Rx=%'llu Tx=%'llu)\n",
                              imap_tot_tcp_rx, imap_tot_tcp_tx);

    fprint2f(fp1, fp2, "    imap TCP Conns: Current=%'d Rate/s(Open=%'llu Close=%'llu) Total(Open=%'llu Close=%'llu)\n",
                            imap_avg->imap_num_connections,
                            imap_con_made_rate,
                            imap_con_break_rate,
                            imap_cavg->imap_c_num_con_succ,
                            imap_cavg->imap_c_num_con_break);
  }
}

// Called by parent
inline void imap_update_imap_avgtime_size() {
  NSDL2_IMAP(NULL, NULL, "Method Called, g_avgtime_size = %d, g_imap_avgtime_idx = %d",
                                          g_avgtime_size, g_imap_avgtime_idx);

  if((global_settings->protocol_enabled & IMAP_PROTOCOL_ENABLED)) {
    NSDL2_IMAP(NULL, NULL, "IMAP is enabled.");
    g_imap_avgtime_idx = g_avgtime_size;
    g_avgtime_size += sizeof(IMAPAvgTime);

  } else {
    NSDL2_IMAP(NULL, NULL, "IMAP is disabled.");
  }

  NSDL2_IMAP(NULL, NULL, "After g_avgtime_size = %d, g_imap_avgtime_idx = %d",
                                          g_avgtime_size, g_imap_avgtime_idx);
}

// called by parent
inline void imap_update_imap_cavgtime_size(){

  NSDL2_IMAP(NULL, NULL, "Method Called, g_IMAPcavgtime_size = %d, g_imap_cavgtime_idx = %d",
                                          g_cavgtime_size, g_imap_cavgtime_idx);

  //if(global_settings->protocol_enabled & IMAP_PROTOCOL_ENABLED || (global_settings->protocol_enabled & IMAPS_PROTOCOL_ENABLED)) {
  if(global_settings->protocol_enabled & IMAP_PROTOCOL_ENABLED) {
    NSDL2_IMAP(NULL, NULL, "IMAP is enabled.");
    g_imap_cavgtime_idx = g_cavgtime_size;
    g_cavgtime_size += sizeof(IMAPCAvgTime);
  } else {
    NSDL2_IMAP(NULL, NULL, "IMAP is disabled.");
  }

  NSDL2_IMAP(NULL, NULL, "After g_cavgtime_size = %d, g_imap_cavgtime_idx = %d",
                                          g_cavgtime_size, g_imap_cavgtime_idx);
}

// Called by child
inline void imap_set_imap_avgtime_ptr() {

  NSDL2_IMAP(NULL, NULL, "Method Called");

  //if((global_settings->protocol_enabled & IMAP_PROTOCOL_ENABLED) || (global_settings->protocol_enabled & IMAPS_PROTOCOL_ENABLED)) {
  if((global_settings->protocol_enabled & IMAP_PROTOCOL_ENABLED)) {
    NSDL2_IMAP(NULL, NULL, "IMAP is enabled.");
   /* We have allocated average_time with the size of imapAvgTime
    * also now we can point that using g_imap_avgtime_idx*/
    imap_avgtime = (IMAPAvgTime*)((char *)average_time + g_imap_avgtime_idx);
  } else {
    NSDL2_IMAP(NULL, NULL, "IMAP is disabled.");
    imap_avgtime = NULL;
  }

  NSDL2_IMAP(NULL, NULL, "imap_avgtime = %p", imap_avgtime);
}

void
imap_log_summary_data (avgtime *avg, double *imap_data, FILE *fp, cavgtime *cavg)
{
u_ns_8B_t num_completed, num_succ, num_samples;
//IMAPAvgTime *imap_avg = NULL;
IMAPCAvgTime *imap_cavg = NULL;

  NSDL2_MESSAGES(NULL, NULL, "Method Called");
  //if((global_settings->protocol_enabled & IMAP_PROTOCOL_ENABLED) || (global_settings->protocol_enabled & IMAPS_PROTOCOL_ENABLED)) {
  if(!(global_settings->protocol_enabled & IMAP_PROTOCOL_ENABLED)) {
      NSDL2_MESSAGES(NULL, NULL, "Returning: IMAP_PROTOCOL is not enabled.");
      return;
  }

  NSDL2_MESSAGES(NULL, NULL, "avg = %p, g_imap_avgtime_idx = %d", avg, g_imap_avgtime_idx);
  //imap_avg = (IMAPAvgTime *) ((char*) avg + g_imap_avgtime_idx); 
  imap_cavg = (IMAPCAvgTime *) ((char*) cavg + g_imap_cavgtime_idx); 
  //NSDL2_MESSAGES(NULL, NULL, "imap_avg = %p", imap_avg);
    
     if (total_imap_request_entries) {
       num_completed = imap_cavg->imap_fetches_completed;
       num_samples = num_succ = imap_cavg->imap_succ_fetches;
       NSDL2_MESSAGES(NULL, NULL, "num_completed = %llu, num_samples = num_succ = %llu", num_completed, num_samples);
                                  
       if (num_completed) {
          if (num_samples) {
             fprintf(fp, "101|imap|10|imap Min Time(Sec)|%1.3f\n", (double)(((double)(imap_cavg->imap_c_min_time))/1000.0));
             fprintf(fp, "101|imap|20|imap Avg Time(Sec)|%1.3f\n", (double)(((double)(imap_cavg->imap_c_tot_time))/((double)(1000.0*(double)num_samples))));
             fprintf(fp, "101|imap|30|imap Max Time(Sec)|%1.3f\n", (double)(((double)(imap_cavg->imap_c_max_time))/1000.0));
             fprintf(fp, "101|imap|40|imap Median Time(Sec)|%1.3f\n", imap_data[0]);
             fprintf(fp, "101|imap|50|imap 80th percentile Time(Sec)|%1.3f\n", imap_data[1]);
             fprintf(fp, "101|imap|60|imap 90th percentile Time(Sec)|%1.3f\n", imap_data[2]);
             fprintf(fp, "101|imap|70|imap 95th percentile Time(Sec)|%1.3f\n", imap_data[3]);
             fprintf(fp, "101|imap|80|imap 99th percentile Time(Sec)|%1.3f\n", imap_data[4]);
             fprintf(fp, "101|imap|90|imap Total|%llu\n", num_completed);
             fprintf(fp, "101|imap|100|imap Success|%llu\n", num_succ);
             fprintf(fp, "101|imap|110|imap Failures|%llu\n", num_completed -  num_succ);
             fprintf(fp, "101|imap|120|imap Failure PCT|%1.2f\n", (double)((((double)((num_completed -  num_succ)))*100.0)/((double)(num_completed))));
            } else {
             fprintf(fp, "101|imap|10|imap Min Time(Sec)|-\n");
             fprintf(fp, "101|imap|20|imap Avg Time(Sec)|-\n");
             fprintf(fp, "101|imap|30|imap Max Time(Sec)|-\n");
             fprintf(fp, "101|imap|40|imap Median Time(Sec)|-\n");
             fprintf(fp, "101|imap|50|imap 80th percentile Time(Sec)|-\n");
             fprintf(fp, "101|imap|60|imap 90th percentile Time(Sec)|-\n");
             fprintf(fp, "101|imap|70|imap 95th percentile Time(Sec)|-\n");
             fprintf(fp, "101|imap|80|imap 99th percentile Time(Sec)|-\n");
             fprintf(fp, "101|imap|90|imap Total|%llu\n", num_completed);
             fprintf(fp, "101|imap|100|imap Success|%llu\n", num_succ);
             fprintf(fp, "101|imap|110|imap Failures|%llu\n", num_completed -  num_succ);
             fprintf(fp, "101|imap|120|imap Failure PCT|%1.2f\n", (double)((((double)((num_completed -  num_succ)))*100.0)/((double)(num_completed))));
            }
          } else {
              fprintf(fp, "101|imap|10|imap Min Time(Sec)|-\n");
              fprintf(fp, "101|imap|20|imap Avg Time(Sec)|-\n");
              fprintf(fp, "101|imap|30|imap Max Time(Sec)|-\n");
              fprintf(fp, "101|imap|40|imap Median Time(Sec)|-\n");
              fprintf(fp, "101|imap|50|imap 80th percentile Time(Sec)|-\n");
              fprintf(fp, "101|imap|60|imap 90th percentile Time(Sec)|-\n");
              fprintf(fp, "101|imap|70|imap 95th percentile Time(Sec)|-\n");
              fprintf(fp, "101|imap|80|imap 99th percentile Time(Sec)|-\n");
              fprintf(fp, "101|imap|90|imap Total|0\n");
              fprintf(fp, "101|imap|100|imap Success|0\n");
              fprintf(fp, "101|imap|110|imap Failures|0\n");
              fprintf(fp, "101|imap|120|imap Failure PCT|0.00\n");
          }
          fprintf(fp, "101|imap|140|imap Hits/Sec|%1.2f\n", (float) ((double)(imap_cavg->imap_fetches_completed * 1000)/ (double)(global_settings->test_duration)));
         }
  
}
