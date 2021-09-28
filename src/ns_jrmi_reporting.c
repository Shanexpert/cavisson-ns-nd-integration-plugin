#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ns_error_codes.h"
#include "netomni/src/core/ni_scenario_distribution.h"
#include "ns_msg_def.h"
#include "ns_gdf.h"
#include "ns_log.h"
#include "url.h"
#include "ns_jrmi.h"
#include "ns_group_data.h"
#include "ns_gdf.h"

static double convert_long_ps (Long_data row_data)
{
  NSDL2_GDF(NULL, NULL, "Method called, row_data = %lu", row_data);
  return((double )((row_data))/((double )global_settings->progress_secs/1000.0));
}

extern int loader_opcode;
static inline void fill_jrmi_net_throughput_gp (avgtime **g_avg, cavgtime **g_cavg)
{
  double result;
  int g_idx = 0, v_idx, grp_idx;
  JRMIAvgTime *jrmi_avg;
  JRMICAvgTime *jrmi_cavg;
  avgtime *avg = NULL;
  cavgtime *cavg = NULL;
  JRMI_Net_throughput_gp *jrmi_net_throughput_local_gp_ptr = jrmi_net_throughput_gp_ptr;

  NSDL2_GDF(NULL, NULL, "Method called");
  for(v_idx = 0; v_idx < sgrp_used_genrator_entries + 1; v_idx++)
  {
    avg = (avgtime *)g_avg[v_idx];
    cavg = (cavgtime *)g_cavg[v_idx];
    for(grp_idx = 0; grp_idx < TOTAL_GRP_ENTERIES_WITH_GRP_KW; grp_idx++)
    {
      /*bug id 101742: using method convert_kbps() */
      jrmi_avg = (JRMIAvgTime*)((char*)((char *)avg + (grp_idx * g_avg_size_only_grp)) + g_jrmi_avgtime_idx);
      jrmi_cavg = (JRMICAvgTime *)((char*)((char *)cavg + (grp_idx * g_cavg_size_only_grp)) + g_jrmi_cavgtime_idx);

      result = (jrmi_avg->jrmi_tx_bytes)*8/((double)global_settings->progress_secs/1000);

      NSDL2_GDF(NULL, NULL, "JRMI Send Throughput result = %f ", result);
      NSDL2_GDF(NULL, NULL, "JRMI Send Throughput after kbps result = %f ", convert_kbps(result));

      GDF_COPY_VECTOR_DATA(jrmi_net_throughput_gp_idx, g_idx, v_idx, 0,
                           convert_kbps(result), jrmi_net_throughput_local_gp_ptr->avg_send_throughput); g_idx++;
      GDF_COPY_VECTOR_DATA(jrmi_net_throughput_gp_idx, g_idx, v_idx, 0,
                           jrmi_cavg->jrmi_c_tot_tx_bytes, jrmi_net_throughput_local_gp_ptr->tot_send_bytes); g_idx++;
     
      result = (jrmi_avg->jrmi_rx_bytes)*8/((double)global_settings->progress_secs/1000);

      NSDL2_GDF(NULL, NULL, "JRMI Receive Throughput result = %f ", result);
      NSDL2_GDF(NULL, NULL, "JRMI Receive Throughput after kbps result = %f ", convert_kbps(result));

      GDF_COPY_VECTOR_DATA(jrmi_net_throughput_gp_idx, g_idx, v_idx, 0,
                           convert_kbps(result), jrmi_net_throughput_local_gp_ptr->avg_recv_throughput); g_idx++;
      GDF_COPY_VECTOR_DATA(jrmi_net_throughput_gp_idx, g_idx, v_idx, 0,
                           jrmi_cavg->jrmi_c_tot_rx_bytes, jrmi_net_throughput_local_gp_ptr->tot_recv_bytes); g_idx++;
     
      result = (jrmi_avg->jrmi_num_con_succ)/((double)global_settings->progress_secs/1000);
      GDF_COPY_VECTOR_DATA(jrmi_net_throughput_gp_idx, g_idx, v_idx, 0,
                           result, jrmi_net_throughput_local_gp_ptr->avg_open_cps); g_idx++;
     
      GDF_COPY_VECTOR_DATA(jrmi_net_throughput_gp_idx, g_idx, v_idx, 0,
                           jrmi_cavg->jrmi_c_num_con_succ, jrmi_net_throughput_local_gp_ptr->tot_conn_open); g_idx++;
     
      result = (jrmi_avg->jrmi_num_con_break)/((double)global_settings->progress_secs/1000);
      GDF_COPY_VECTOR_DATA(jrmi_net_throughput_gp_idx, g_idx, v_idx, 0,
                           result, jrmi_net_throughput_local_gp_ptr->avg_close_cps); g_idx++;
     
      GDF_COPY_VECTOR_DATA(jrmi_net_throughput_gp_idx, g_idx, v_idx, 0,
                           jrmi_cavg->jrmi_c_num_con_break, jrmi_net_throughput_local_gp_ptr->tot_conn_close); g_idx++;
      g_idx = 0;
      jrmi_net_throughput_local_gp_ptr++;
    }
  }
}

static inline void fill_jrmi_hits_gp (avgtime **g_avg, cavgtime **g_cavg)
{ 
  int g_idx = 0, v_idx, grp_idx;
  JRMIAvgTime *jrmi_avg = NULL;
  JRMICAvgTime *jrmi_cavg = NULL;
  avgtime *avg = NULL;
  cavgtime *cavg = NULL;
  NSDL2_GDF(NULL, NULL, "Method called");
  JRMI_hits_gp *jrmi_hits_local_gp_ptr = jrmi_hits_gp_ptr;

  for(v_idx = 0; v_idx < sgrp_used_genrator_entries + 1; v_idx++)
  {
    avg = (avgtime *)g_avg[v_idx];
    cavg = (cavgtime *)g_cavg[v_idx];
    for(grp_idx = 0; grp_idx < TOTAL_GRP_ENTERIES_WITH_GRP_KW; grp_idx++)
    {
      /*bug id 101742: using method convert_long_ps() */

      jrmi_avg = (JRMIAvgTime*)((char*)((char *)avg + (grp_idx * g_avg_size_only_grp)) + g_jrmi_avgtime_idx);
      jrmi_cavg = (JRMICAvgTime *)((char*)((char *)cavg + (grp_idx * g_cavg_size_only_grp)) + g_jrmi_cavgtime_idx);

      NSDL2_GDF(NULL, NULL, "JRMI Request Sent/Sec jrmi_avg->jrmi_fetches_started = %lu ", jrmi_avg->jrmi_fetches_started);
      NSDL2_GDF(NULL, NULL, "JRMI Request Sent/Sec after PS jrmi_avg->jrmi_fetches_started = %lu ", convert_long_ps(jrmi_avg->jrmi_fetches_started));

      GDF_COPY_VECTOR_DATA(jrmi_hits_gp_idx, g_idx, v_idx, 0, 
                           convert_long_ps(jrmi_avg->jrmi_fetches_started), jrmi_hits_local_gp_ptr->url_req); g_idx++;
    
      NSDL2_GDF(NULL, NULL, "JRMI Hits/Sec jrmi_avg->jrmi_num_tries = %lu ", jrmi_avg->jrmi_num_tries);
      NSDL2_GDF(NULL, NULL, "JRMI Hits/Sec after PS jrmi_avg->jrmi_num_tries = %lu ", convert_long_ps(jrmi_avg->jrmi_num_tries));
 
      GDF_COPY_VECTOR_DATA(jrmi_hits_gp_idx, g_idx, v_idx, 0,
                           convert_long_ps(jrmi_avg->jrmi_num_tries), jrmi_hits_local_gp_ptr->tries); g_idx++;
    
      NSDL2_GDF(NULL, NULL, "Success JRMI Responses/Sec jrmi_avg->jrmi_num_hits = %lu ", jrmi_avg->jrmi_num_hits);
      NSDL2_GDF(NULL, NULL, "Success JRMI Responses/Sec after PS jrmi_avg->jrmi_num_hits = %lu ", convert_long_ps(jrmi_avg->jrmi_num_hits));
 
      GDF_COPY_VECTOR_DATA(jrmi_hits_gp_idx, g_idx, v_idx, 0,
                           convert_long_ps(jrmi_avg->jrmi_num_hits), jrmi_hits_local_gp_ptr->succ); g_idx++;
     
      // Here the "response" variable of `Times_data` data type is getting filled
     
      GDF_COPY_TIMES_VECTOR_DATA(jrmi_hits_gp_idx, g_idx, v_idx, 0,
                                 jrmi_avg->jrmi_avg_time, jrmi_avg->jrmi_min_time, jrmi_avg->jrmi_max_time, jrmi_avg->jrmi_num_hits,
                                 jrmi_hits_local_gp_ptr->response.avg_time,
                                 jrmi_hits_local_gp_ptr->response.min_time,
                                 jrmi_hits_local_gp_ptr->response.max_time,
                                 jrmi_hits_local_gp_ptr->response.succ); g_idx++;
     
      // This need to be fixed as these two are 'long long'
      GDF_COPY_VECTOR_DATA(jrmi_hits_gp_idx, g_idx, v_idx, 0,
                           jrmi_cavg->jrmi_fetches_completed, jrmi_hits_local_gp_ptr->cum_tries); g_idx++;
     
      GDF_COPY_VECTOR_DATA(jrmi_hits_gp_idx, g_idx, v_idx, 0,
                           jrmi_cavg->jrmi_succ_fetches, jrmi_hits_local_gp_ptr->cum_succ); g_idx++;
      g_idx = 0;
      jrmi_hits_local_gp_ptr++;
    }
  }
}

static inline void fill_jrmi_fail_gp (avgtime **g_avg)
{
  unsigned int url_all_failures = 0;
  int i, v_idx, all_idx, j = 0, grp_idx;
  JRMIAvgTime *jrmi_avg;
  avgtime *avg = NULL;
  JRMI_fail_gp *jrmi_fail_local_gp_ptr = jrmi_fail_gp_ptr;
  JRMI_fail_gp *jrmi_fail_local_first_gp_ptr = NULL;

  NSDL2_GDF(NULL, NULL, "Method called");
  
  for(v_idx = 0; v_idx < sgrp_used_genrator_entries + 1; v_idx++)
  {
    avg = (avgtime *)g_avg[v_idx];
    for(grp_idx = 0; grp_idx < TOTAL_GRP_ENTERIES_WITH_GRP_KW; grp_idx++)
    {
      jrmi_avg = (JRMIAvgTime*)((char*)((char *)avg + (grp_idx * g_avg_size_only_grp)) + g_jrmi_avgtime_idx);
      all_idx = j;
      j++;
      jrmi_fail_local_first_gp_ptr = jrmi_fail_local_gp_ptr;
      jrmi_fail_local_gp_ptr++;
      for (i = 0; i < (TOTAL_USED_URL_ERR - 1); i++, j++)
      {
        GDF_COPY_VECTOR_DATA(jrmi_fail_gp_idx, 0, j, 0, jrmi_avg->jrmi_error_codes[i + 1], jrmi_fail_local_gp_ptr->failures);
        url_all_failures += jrmi_avg->jrmi_error_codes[i + 1];
        jrmi_fail_local_gp_ptr++;
      }
      /*bug id 101742: using method convert_long_ps() */
      NSDL2_GDF(NULL, NULL, "JRMI Failures/Sec url_all_failures = %lu ", url_all_failures);
      NSDL2_GDF(NULL, NULL, "JRMI Failures/Sec after PS url_all_failures = %lu ", convert_long_ps(url_all_failures));

      GDF_COPY_VECTOR_DATA(jrmi_fail_gp_idx, 0, all_idx, 0, convert_long_ps(url_all_failures), jrmi_fail_local_first_gp_ptr->failures);
      url_all_failures = 0;
    }
  }
}

inline void fill_jrmi_gp (avgtime **g_avg, cavgtime **g_cavg) {
  
  NSDL2_JRMI(NULL, NULL, "Method called. g_avg = %p", g_avg);

  if (jrmi_hits_gp_ptr)
    fill_jrmi_hits_gp (g_avg, g_cavg);
  if (jrmi_net_throughput_gp_ptr)
    fill_jrmi_net_throughput_gp (g_avg, g_cavg);
  if (jrmi_fail_gp_ptr)
    fill_jrmi_fail_gp (g_avg);
}

void
print_jrmi_throughput(FILE *fp1, FILE *fp2, int is_periodic, avgtime *avg,  cavgtime *cavg)
{

double jrmi_tcp_rx, jrmi_tcp_tx;
double duration;
JRMIAvgTime *jrmi_avg = NULL;
JRMICAvgTime *jrmi_cavg = NULL;

  if(!(global_settings->protocol_enabled & JRMI_PROTOCOL_ENABLED)) {
    return; 
  }
  /* We have allocated average_time with the size of jrmiAvgTime
   * also now we can point that using g_jrmi_avgtime_idx
   */
  jrmi_avg = (JRMIAvgTime*)((char *)avg + g_jrmi_avgtime_idx);
  jrmi_cavg = (JRMICAvgTime*)((char *)cavg + g_jrmi_cavgtime_idx);

  u_ns_8B_t  jrmi_tot_tcp_rx = 0, jrmi_tot_tcp_tx = 0;
  u_ns_8B_t jrmi_con_made_rate, jrmi_con_break_rate, jrmi_hit_tot_rate, jrmi_hit_succ_rate, jrmi_hit_initited_rate; 
  u_ns_8B_t jrmi_num_completed, jrmi_num_initiated, jrmi_num_succ; //, jrmi_num_samples;

  char jrmi_tbuffer[1024]="";
  
  NSDL2_REPORTING(NULL, NULL, "Method called");
  if (is_periodic) {
    duration = (double)((double)(global_settings->progress_secs)/1000.0);
    sprintf(jrmi_tbuffer, " JRMI (Rx=%'.3f)", (double)(((double)(jrmi_avg->jrmi_total_bytes))/(duration*128.0)));
    jrmi_tcp_rx = ((double)(jrmi_avg->jrmi_rx_bytes))/(duration*128.0);
    jrmi_tcp_tx = ((double)(jrmi_avg->jrmi_tx_bytes))/(duration*128.0);

    jrmi_con_made_rate = (jrmi_avg->jrmi_num_con_succ * 1000)/global_settings->progress_secs;
    jrmi_con_break_rate = (jrmi_avg->jrmi_num_con_break * 1000)/global_settings->progress_secs;
    jrmi_num_completed = jrmi_avg->jrmi_num_tries;
    //jrmi_num_samples = jrmi_num_succ = jrmi_avg->jrmi_num_hits;
    jrmi_num_succ = jrmi_avg->jrmi_num_hits;
    jrmi_num_initiated = jrmi_avg->jrmi_fetches_started;
    jrmi_hit_tot_rate = (jrmi_num_completed * 1000)/global_settings->progress_secs;
    jrmi_hit_succ_rate = (jrmi_num_succ * 1000)/global_settings->progress_secs;
    jrmi_hit_initited_rate = (jrmi_num_initiated * 1000)/global_settings->progress_secs;
  } else {
    duration = (double)((double)(global_settings->test_duration)/1000.0);
    sprintf(jrmi_tbuffer, " JRMI (Rx=%'.3f)", (double)(((double)(jrmi_cavg->jrmi_c_tot_total_bytes))/(duration*128.0)));
    jrmi_tcp_rx = ((double)(jrmi_cavg->jrmi_c_tot_rx_bytes))/(duration*128.0);
    jrmi_tcp_tx = ((double)(jrmi_cavg->jrmi_c_tot_tx_bytes))/(duration*128.0);

    jrmi_con_made_rate = (jrmi_cavg->jrmi_c_num_con_succ * 1000)/global_settings->test_duration;
    jrmi_con_break_rate = (jrmi_cavg->jrmi_c_num_con_break * 1000)/global_settings->test_duration;
    jrmi_num_completed = jrmi_cavg->jrmi_fetches_completed;
    //jrmi_num_samples = jrmi_num_succ = jrmi_cavg->jrmi_succ_fetches;
    jrmi_num_succ = jrmi_cavg->jrmi_succ_fetches;
    NSDL2_MESSAGES(NULL, NULL, "  jrmi_succ_fetches in jrmi file = %llu, no of succ in jrmi = %llu", jrmi_cavg->jrmi_succ_fetches , jrmi_num_succ); 
    jrmi_num_initiated = jrmi_cavg->cum_jrmi_fetches_started;
    jrmi_hit_tot_rate = (jrmi_num_completed * 1000)/global_settings->test_duration;
    jrmi_hit_succ_rate = (jrmi_num_succ * 1000)/global_settings->test_duration;
    jrmi_hit_initited_rate = (jrmi_num_initiated * 1000)/global_settings->test_duration;     
  }

  jrmi_tot_tcp_rx = (u_ns_8B_t)jrmi_cavg->jrmi_c_tot_rx_bytes;
  jrmi_tot_tcp_tx = (u_ns_8B_t)jrmi_cavg->jrmi_c_tot_tx_bytes;

  if (jrmi_avg->jrmi_num_hits) {
    if (global_settings->show_initiated)
      fprint2f(fp1, fp2, "    jrmi hit rate (per sec): Initiated=%'llu completed=%'llu Success=%'llu\n",
      jrmi_hit_initited_rate, jrmi_hit_tot_rate, jrmi_hit_succ_rate);
    else
      fprint2f(fp1, fp2, "    jrmi hit rate (per sec): Total=%'llu Success=%'llu\n", jrmi_hit_tot_rate, jrmi_hit_succ_rate);

/*       sprintf(ubuffer, " jrmi Body(Rx=%'llu)",  */
/*               (u_ns_8B_t)(avg->jrmi_c_tot_total_bytes)); */

    fprint2f(fp1, fp2, "    jrmi (Kbits/s) TCP(Rx=%'.3f Tx=%'.3f)\n",
                             jrmi_tcp_rx, jrmi_tcp_tx);
    fprint2f(fp1, fp2, "    jrmi (Total Bytes) TCP(Rx=%'llu Tx=%'llu)\n",
                              jrmi_tot_tcp_rx, jrmi_tot_tcp_tx);

    fprint2f(fp1, fp2, "    jrmi TCP Conns: Current=%'d Rate/s(Open=%'llu Close=%'llu) Total(Open=%'llu Close=%'llu)\n",
                            jrmi_avg->jrmi_num_connections,
                            jrmi_con_made_rate,
                            jrmi_con_break_rate,
                            jrmi_cavg->jrmi_c_num_con_succ,
                            jrmi_cavg->jrmi_c_num_con_break);
  }
}

// Called by parent
inline void jrmi_update_jrmi_avgtime_size() {
  NSDL2_JRMI(NULL, NULL, "Method Called, g_avgtime_size = %d, g_jrmi_avgtime_idx = %d",
                                          g_avgtime_size, g_jrmi_avgtime_idx);

  if(global_settings->protocol_enabled & JRMI_PROTOCOL_ENABLED) {
    NSDL2_JRMI(NULL, NULL, "JRMI is enabled.");
    g_jrmi_avgtime_idx = g_avgtime_size;
    g_avgtime_size += sizeof(JRMIAvgTime);

  } else {
    NSDL2_JRMI(NULL, NULL, "JRMI is disabled.");
  }

  NSDL2_JRMI(NULL, NULL, "After g_avgtime_size = %d, g_jrmi_avgtime_idx = %d",
                                          g_avgtime_size, g_jrmi_avgtime_idx);
}

// called by parent
inline void jrmi_update_jrmi_cavgtime_size(){

  NSDL2_JRMI(NULL, NULL, "Method Called, g_JRMIcavgtime_size = %d, g_jrmi_cavgtime_idx = %d",
                                          g_cavgtime_size, g_jrmi_cavgtime_idx);

  if(global_settings->protocol_enabled & JRMI_PROTOCOL_ENABLED) {
    NSDL2_JRMI(NULL, NULL, "JRMI is enabled.");
    g_jrmi_cavgtime_idx = g_cavgtime_size;
    g_cavgtime_size += sizeof(JRMICAvgTime);
  } else {
    NSDL2_JRMI(NULL, NULL, "JRMI is disabled.");
  }

  NSDL2_JRMI(NULL, NULL, "After g_cavgtime_size = %d, g_jrmi_cavgtime_idx = %d",
                                          g_cavgtime_size, g_jrmi_cavgtime_idx);
}

// Called by child
inline void jrmi_set_jrmi_avgtime_ptr() {

  NSDL2_JRMI(NULL, NULL, "Method Called");

  if(global_settings->protocol_enabled & JRMI_PROTOCOL_ENABLED) {
    NSDL2_JRMI(NULL, NULL, "JRMI is enabled.");
   /* We have allocated average_time with the size of jrmiAvgTime
    * also now we can point that using g_jrmi_avgtime_idx*/
    jrmi_avgtime = (JRMIAvgTime*)((char *)average_time + g_jrmi_avgtime_idx);
  } else {
    NSDL2_JRMI(NULL, NULL, "JRMI is disabled.");
    jrmi_avgtime = NULL;
  }

  NSDL2_JRMI(NULL, NULL, "jrmi_avgtime = %p", jrmi_avgtime);
}

void
jrmi_log_summary_data (avgtime *avg, double *jrmi_data, FILE *fp, cavgtime *cavg)
{
u_ns_8B_t num_completed, num_succ, num_samples;
//JRMIAvgTime *jrmi_avg = NULL;
JRMICAvgTime *jrmi_cavg = NULL;

  NSDL2_MESSAGES(NULL, NULL, "Method Called");
  if(!(global_settings->protocol_enabled & JRMI_PROTOCOL_ENABLED)) {
      NSDL2_MESSAGES(NULL, NULL, "Returning: JRMI_PROTOCOL is not enabled.");
      return;
  }

  NSDL2_MESSAGES(NULL, NULL, "avg = %p, g_jrmi_avgtime_idx = %d", avg, g_jrmi_avgtime_idx);
  //jrmi_avg = (JRMIAvgTime *) ((char*) avg + g_jrmi_avgtime_idx); 
  jrmi_cavg = (JRMICAvgTime *) ((char*) cavg + g_jrmi_cavgtime_idx); 
  //NSDL2_MESSAGES(NULL, NULL, "jrmi_avg = %p", jrmi_avg);
    
     if (total_jrmi_request_entries) {
       num_completed = jrmi_cavg->jrmi_fetches_completed;
       num_samples = num_succ = jrmi_cavg->jrmi_succ_fetches;
       NSDL2_MESSAGES(NULL, NULL, "num_completed = %llu, num_samples = num_succ = %llu", num_completed, num_samples);
                                  
       if (num_completed) {
          if (num_samples) {
             fprintf(fp, "101|jrmi|10|jrmi Min Time(Sec)|%1.3f\n", (double)(((double)(jrmi_cavg->jrmi_c_min_time))/1000.0));
             fprintf(fp, "101|jrmi|20|jrmi Avg Time(Sec)|%1.3f\n", (double)(((double)(jrmi_cavg->jrmi_c_tot_time))/((double)(1000.0*(double)num_samples))));
             fprintf(fp, "101|jrmi|30|jrmi Max Time(Sec)|%1.3f\n", (double)(((double)(jrmi_cavg->jrmi_c_max_time))/1000.0));
             fprintf(fp, "101|jrmi|40|jrmi Median Time(Sec)|%1.3f\n", jrmi_data[0]);
             fprintf(fp, "101|jrmi|50|jrmi 80th percentile Time(Sec)|%1.3f\n", jrmi_data[1]);
             fprintf(fp, "101|jrmi|60|jrmi 90th percentile Time(Sec)|%1.3f\n", jrmi_data[2]);
             fprintf(fp, "101|jrmi|70|jrmi 95th percentile Time(Sec)|%1.3f\n", jrmi_data[3]);
             fprintf(fp, "101|jrmi|80|jrmi 99th percentile Time(Sec)|%1.3f\n", jrmi_data[4]);
             fprintf(fp, "101|jrmi|90|jrmi Total|%llu\n", num_completed);
             fprintf(fp, "101|jrmi|100|jrmi Success|%llu\n", num_succ);
             fprintf(fp, "101|jrmi|110|jrmi Failures|%llu\n", num_completed -  num_succ);
             fprintf(fp, "101|jrmi|120|jrmi Failure PCT|%1.2f\n", (double)((((double)((num_completed -  num_succ)))*100.0)/((double)(num_completed))));
            } else {
             fprintf(fp, "101|jrmi|10|jrmi Min Time(Sec)|-\n");
             fprintf(fp, "101|jrmi|20|jrmi Avg Time(Sec)|-\n");
             fprintf(fp, "101|jrmi|30|jrmi Max Time(Sec)|-\n");
             fprintf(fp, "101|jrmi|40|jrmi Median Time(Sec)|-\n");
             fprintf(fp, "101|jrmi|50|jrmi 80th percentile Time(Sec)|-\n");
             fprintf(fp, "101|jrmi|60|jrmi 90th percentile Time(Sec)|-\n");
             fprintf(fp, "101|jrmi|70|jrmi 95th percentile Time(Sec)|-\n");
             fprintf(fp, "101|jrmi|80|jrmi 99th percentile Time(Sec)|-\n");
             fprintf(fp, "101|jrmi|90|jrmi Total|%llu\n", num_completed);
             fprintf(fp, "101|jrmi|100|jrmi Success|%llu\n", num_succ);
             fprintf(fp, "101|jrmi|110|jrmi Failures|%llu\n", num_completed -  num_succ);
             fprintf(fp, "101|jrmi|120|jrmi Failure PCT|%1.2f\n", (double)((((double)((num_completed -  num_succ)))*100.0)/((double)(num_completed))));
            }
          } else {
              fprintf(fp, "101|jrmi|10|jrmi Min Time(Sec)|-\n");
              fprintf(fp, "101|jrmi|20|jrmi Avg Time(Sec)|-\n");
              fprintf(fp, "101|jrmi|30|jrmi Max Time(Sec)|-\n");
              fprintf(fp, "101|jrmi|40|jrmi Median Time(Sec)|-\n");
              fprintf(fp, "101|jrmi|50|jrmi 80th percentile Time(Sec)|-\n");
              fprintf(fp, "101|jrmi|60|jrmi 90th percentile Time(Sec)|-\n");
              fprintf(fp, "101|jrmi|70|jrmi 95th percentile Time(Sec)|-\n");
              fprintf(fp, "101|jrmi|80|jrmi 99th percentile Time(Sec)|-\n");
              fprintf(fp, "101|jrmi|90|jrmi Total|0\n");
              fprintf(fp, "101|jrmi|100|jrmi Success|0\n");
              fprintf(fp, "101|jrmi|110|jrmi Failures|0\n");
              fprintf(fp, "101|jrmi|120|jrmi Failure PCT|0.00\n");
          }
          fprintf(fp, "101|jrmi|140|jrmi Hits/Sec|%1.2f\n", (float) ((double)(jrmi_cavg->jrmi_fetches_completed * 1000)/ (double)(global_settings->test_duration)));
         }
  
}
