/**
 * FILE: ns_ftp_reporting.c
 * PURPOSE: contains all FTP reporting related functions
 * AUTHOR: Nikita
 */


#include "util.h"

#include "tmr.h"

#include "timing.h"

#include "logging.h"
#include "ns_schedule_phases.h"
#include "netstorm.h"

#include "ns_gdf.h"

#include "ns_log.h"
#include "ns_ftp.h"
#include "netomni/src/core/ni_scenario_distribution.h"
#include "wait_forever.h"
#include "ns_group_data.h"

#ifndef CAV_MAIN
int g_ftp_avgtime_idx = -1;
FTPAvgTime *ftp_avgtime = NULL;
#else
__thread int g_ftp_avgtime_idx = -1;
__thread FTPAvgTime *ftp_avgtime = NULL;
#endif

FTPCAvgTime *ftp_cavgtime = NULL;
int g_ftp_cavgtime_idx = -1;

static double convert_long_ps (Long_data row_data)
{
  NSDL2_GDF(NULL, NULL, "Method called, row_data = %lu", row_data);
  return((double )((row_data))/((double )global_settings->progress_secs/1000.0));
}

// called by parent
inline void ftp_update_ftp_cavgtime_size(){

  NSDL2_FTP(NULL, NULL, "Method Called, g_FTPcavgtime_size = %d, g_ftp_cavgtime_idx = %d",
                                          g_cavgtime_size, g_ftp_cavgtime_idx);

  if(global_settings->protocol_enabled & FTP_PROTOCOL_ENABLED) {
    NSDL2_FTP(NULL, NULL, "FTP is enabled.");
    g_ftp_cavgtime_idx = g_cavgtime_size;
    g_cavgtime_size += sizeof(FTPCAvgTime);
  } else {
    NSDL2_FTP(NULL, NULL, "FTP is disabled.");
  }

  NSDL2_FTP(NULL, NULL, "After g_cavgtime_size = %d, g_ftp_cavgtime_idx = %d",
                                          g_cavgtime_size, g_ftp_cavgtime_idx);
}


#if 0
inline void  ftp_set_ftp_cavgtime_ptr(){
  NSDL2_FTP(NULL, NULL, "Method Called");

  if(global_settings->protocol_enabled & FTP_PROTOCOL_ENABLED) {
    NSDL2_FTP(NULL, NULL, "FTP is enabled.");
   /* We have allocated average_time with the size of ftpAvgTime
 *     * also now we can point that using g_ftp_avgtime_idx*/
    ftp_cavgtime = (FTPcavgtime*)((char *)caverage_time + g_ftp_cavgtime_idx);
  } else {
    NSDL2_FTP(NULL, NULL, "FTP is disabled.");
    ftp_cavgtime = NULL;
  }

  NSDL2_FTP(NULL, NULL, "ftp_avgtime = %p", ftp_cavgtime);
}
#endif

// Called by parent
inline void ftp_update_ftp_avgtime_size() {
  NSDL2_FTP(NULL, NULL, "Method Called, g_avgtime_size = %d, g_ftp_avgtime_idx = %d",
                                          g_avgtime_size, g_ftp_avgtime_idx);

  if(global_settings->protocol_enabled & FTP_PROTOCOL_ENABLED) {
    NSDL2_FTP(NULL, NULL, "FTP is enabled.");
    g_ftp_avgtime_idx = g_avgtime_size;
    g_avgtime_size += sizeof(FTPAvgTime);

  } else {
    NSDL2_FTP(NULL, NULL, "FTP is disabled.");
  }

  NSDL2_FTP(NULL, NULL, "After g_avgtime_size = %d, g_ftp_avgtime_idx = %d",
                                          g_avgtime_size, g_ftp_avgtime_idx);
}

// Called by child
inline void ftp_set_ftp_avgtime_ptr() {

  NSDL2_FTP(NULL, NULL, "Method Called");

  if(global_settings->protocol_enabled & FTP_PROTOCOL_ENABLED) {
    NSDL2_FTP(NULL, NULL, "FTP is enabled.");
   /* We have allocated average_time with the size of ftpAvgTime
    * also now we can point that using g_ftp_avgtime_idx*/
    ftp_avgtime = (FTPAvgTime*)((char *)average_time + g_ftp_avgtime_idx);
  } else {
    NSDL2_FTP(NULL, NULL, "FTP is disabled.");
    ftp_avgtime = NULL;
  }

  NSDL2_FTP(NULL, NULL, "ftp_avgtime = %p", ftp_avgtime);
}

// GDF filling

static inline void fill_ftp_net_throughput_gp (avgtime **g_avg, cavgtime **g_cavg)
{
  double result;
  int g_idx = 0, v_idx, grp_idx;
  FTPAvgTime *ftp_avg = NULL;
  FTPCAvgTime *ftp_cavg = NULL;
  avgtime *avg = NULL;
  cavgtime *cavg = NULL;
  FTP_Net_throughput_gp *ftp_net_throughput_local_gp_ptr = ftp_net_throughput_gp_ptr; 
  NSDL2_GDF(NULL, NULL, "Method called");

  for(v_idx = 0; v_idx < sgrp_used_genrator_entries + 1; v_idx++)
  {
    avg = (avgtime *)g_avg[v_idx];
    cavg = (cavgtime *)g_cavg[v_idx];
    for(grp_idx = 0; grp_idx < TOTAL_GRP_ENTERIES_WITH_GRP_KW; grp_idx++)
    {
      /*bug id 101742: using method convert_kbps() */
      ftp_avg = (FTPAvgTime*)((char*)((char *)avg + (grp_idx * g_avg_size_only_grp)) + g_ftp_avgtime_idx);
      ftp_cavg = (FTPCAvgTime *)((char*)((char *)cavg + (grp_idx * g_cavg_size_only_grp)) + g_ftp_cavgtime_idx);

      result = (ftp_avg->ftp_tx_bytes)*8/((double)global_settings->progress_secs/1000);

      NSDL2_GDF(NULL, NULL, "FTP Send Throughput result = %f ", result);
      NSDL2_GDF(NULL, NULL, "FTP Send Throughput after kbps result = %f ", convert_kbps(result));

      GDF_COPY_VECTOR_DATA(ftp_net_throughput_gp_idx, g_idx, v_idx, 0,
                           convert_kbps(result), ftp_net_throughput_local_gp_ptr->avg_send_throughput); g_idx++;
      GDF_COPY_VECTOR_DATA(ftp_net_throughput_gp_idx, g_idx, v_idx, 0,
                           ftp_cavg->ftp_c_tot_tx_bytes, ftp_net_throughput_local_gp_ptr->tot_send_bytes); g_idx++;
     
      result = (ftp_avg->ftp_rx_bytes)*8/((double)global_settings->progress_secs/1000);

      NSDL2_GDF(NULL, NULL, "FTP Recieve Throughput result = %f ", result);
      NSDL2_GDF(NULL, NULL, "FTP Recieve Throughput after kbps result = %f ", convert_kbps(result));

      GDF_COPY_VECTOR_DATA(ftp_net_throughput_gp_idx, g_idx, v_idx, 0,
                           convert_kbps(result), ftp_net_throughput_local_gp_ptr->avg_recv_throughput); g_idx++;
      GDF_COPY_VECTOR_DATA(ftp_net_throughput_gp_idx, g_idx, v_idx, 0,
                          ftp_cavg->ftp_c_tot_rx_bytes, ftp_net_throughput_local_gp_ptr->tot_recv_bytes); g_idx++;
     
      result = (ftp_avg->ftp_num_con_succ)/((double)global_settings->progress_secs/1000);
      GDF_COPY_VECTOR_DATA(ftp_net_throughput_gp_idx, g_idx, v_idx, 0,
                           result, ftp_net_throughput_local_gp_ptr->avg_open_cps); g_idx++;
     
      GDF_COPY_VECTOR_DATA(ftp_net_throughput_gp_idx, g_idx, v_idx, 0,
                           ftp_cavg->ftp_c_num_con_succ, ftp_net_throughput_local_gp_ptr->tot_conn_open); g_idx++;
     
      result = (ftp_avg->ftp_num_con_break)/((double)global_settings->progress_secs/1000);
      GDF_COPY_VECTOR_DATA(ftp_net_throughput_gp_idx, g_idx, v_idx, 0,
                           result, ftp_net_throughput_local_gp_ptr->avg_close_cps); g_idx++;
     
      GDF_COPY_VECTOR_DATA(ftp_net_throughput_gp_idx, g_idx, v_idx, 0,
                           ftp_cavg->ftp_c_num_con_break, ftp_net_throughput_local_gp_ptr->tot_conn_close); g_idx++;
      g_idx = 0;
      ftp_net_throughput_local_gp_ptr++;
    }
  }
}

static inline void fill_ftp_hits_gp (avgtime **g_avg, cavgtime **g_cavg)
{  
  int g_idx = 0, v_idx, grp_idx;
  FTPAvgTime *ftp_avg = NULL;
  FTPCAvgTime *ftp_cavg = NULL;
  avgtime *avg = NULL;
  cavgtime *cavg = NULL;
  FTP_hits_gp *ftp_hits_local_gp_ptr = ftp_hits_gp_ptr;
  NSDL2_GDF(NULL, NULL, "Method called");

  for(v_idx = 0; v_idx < sgrp_used_genrator_entries + 1; v_idx++)
  { 
    avg = (avgtime *)g_avg[v_idx];
    cavg = (cavgtime *)g_cavg[v_idx];
    for(grp_idx = 0; grp_idx < TOTAL_GRP_ENTERIES_WITH_GRP_KW; grp_idx++)
    {
      /*bug id 101742: using method convert_long_ps() */
      ftp_avg = (FTPAvgTime*)((char*)((char *)avg + (grp_idx * g_avg_size_only_grp)) + g_ftp_avgtime_idx);
      ftp_cavg = (FTPCAvgTime *)((char*)((char *)cavg + (grp_idx * g_cavg_size_only_grp)) + g_ftp_cavgtime_idx);

      NSDL2_GDF(NULL, NULL, "FTP Request Sent/Sec ftp_avg->ftp_fetches_started = %lu ", ftp_avg->ftp_fetches_started);
      NSDL2_GDF(NULL, NULL, "FTP Request Sent/Sec after PS ftp_avg->ftp_fetches_started = %lu ", convert_long_ps(ftp_avg->ftp_fetches_started));

      GDF_COPY_VECTOR_DATA(ftp_hits_gp_idx, g_idx, v_idx, 0,
                           convert_long_ps(ftp_avg->ftp_fetches_started), ftp_hits_local_gp_ptr->url_req); g_idx++;
    
      NSDL2_GDF(NULL, NULL, "FTP Hits/Sec ftp_avg->ftp_num_tries = %lu ", ftp_avg->ftp_num_tries);
      NSDL2_GDF(NULL, NULL, "FTP Hits/Sec after PS ftp_avg->ftp_num_tries = %lu ", convert_long_ps(ftp_avg->ftp_num_tries));
 
      GDF_COPY_VECTOR_DATA(ftp_hits_gp_idx, g_idx, v_idx, 0,
                           convert_long_ps(ftp_avg->ftp_num_tries), ftp_hits_local_gp_ptr->tries); g_idx++;
    
      NSDL2_GDF(NULL, NULL, "Success FTP Responses/Sec ftp_avg->ftp_num_hits = %lu ", ftp_avg->ftp_num_hits);
      NSDL2_GDF(NULL, NULL, "Success FTP Responses/Sec after PS ftp_avg->ftp_num_hits = %lu ", convert_long_ps(ftp_avg->ftp_num_hits));
 
      GDF_COPY_VECTOR_DATA(ftp_hits_gp_idx, g_idx, v_idx, 0,
                           convert_long_ps(ftp_avg->ftp_num_hits), ftp_hits_local_gp_ptr->succ); g_idx++;
      // Here the "response" variable of `Times_data` data type is getting filled
     

      //Overall
      GDF_COPY_TIMES_VECTOR_DATA(ftp_hits_gp_idx, g_idx, v_idx, 0,
                                 ftp_avg->ftp_overall_avg_time, ftp_avg->ftp_overall_min_time, 
                                 ftp_avg->ftp_overall_max_time, ftp_avg->ftp_num_tries,
                                 ftp_hits_local_gp_ptr->response.avg_time,
                                 ftp_hits_local_gp_ptr->response.min_time,
                                 ftp_hits_local_gp_ptr->response.max_time,
                                 ftp_hits_local_gp_ptr->response.succ); g_idx++;
      
      //Success
      if(ftp_avg->ftp_num_hits)
      {
        GDF_COPY_TIMES_VECTOR_DATA(ftp_hits_gp_idx, g_idx, v_idx, 0,
                                   ftp_avg->ftp_avg_time, ftp_avg->ftp_min_time, ftp_avg->ftp_max_time, ftp_avg->ftp_num_hits,
                                   ftp_hits_local_gp_ptr->succ_response.avg_time,
                                   ftp_hits_local_gp_ptr->succ_response.min_time,
                                   ftp_hits_local_gp_ptr->succ_response.max_time,
                                   ftp_hits_local_gp_ptr->succ_response.succ); g_idx++;
      }
      else
      {
        GDF_COPY_TIMES_VECTOR_DATA(ftp_hits_gp_idx, g_idx, v_idx, 0,
                                   0, -1, 0, 0,
                                   ftp_hits_local_gp_ptr->succ_response.avg_time,
                                   ftp_hits_local_gp_ptr->succ_response.min_time,
                                   ftp_hits_local_gp_ptr->succ_response.max_time,
                                   ftp_hits_local_gp_ptr->succ_response.succ); g_idx++;
      }

      //Failure
      if(ftp_hits_local_gp_ptr->tries - ftp_avg->ftp_num_hits)
      {
        GDF_COPY_TIMES_VECTOR_DATA(ftp_hits_gp_idx, g_idx, v_idx, 0,
                                   ftp_avg->ftp_failure_avg_time, ftp_avg->ftp_failure_min_time, 
                                   ftp_avg->ftp_failure_max_time, ftp_hits_local_gp_ptr->tries - ftp_avg->ftp_num_hits,
                                   ftp_hits_local_gp_ptr->fail_response.avg_time,
                                   ftp_hits_local_gp_ptr->fail_response.min_time,
                                   ftp_hits_local_gp_ptr->fail_response.max_time,
                                   ftp_hits_local_gp_ptr->fail_response.succ); g_idx++;
      }
      else
      {
        GDF_COPY_TIMES_VECTOR_DATA(ftp_hits_gp_idx, g_idx, v_idx, 0,
                                   0, -1, 0, 0,
                                   ftp_hits_local_gp_ptr->fail_response.avg_time,
                                   ftp_hits_local_gp_ptr->fail_response.min_time,
                                   ftp_hits_local_gp_ptr->fail_response.max_time,
                                   ftp_hits_local_gp_ptr->fail_response.succ); g_idx++;
      }
      
      // This need to be fixed as these two are 'long long'
      GDF_COPY_VECTOR_DATA(ftp_hits_gp_idx, g_idx, v_idx, 0,
                           ftp_cavg->ftp_fetches_completed, ftp_hits_local_gp_ptr->cum_tries); g_idx++;
     
      GDF_COPY_VECTOR_DATA(ftp_hits_gp_idx, g_idx, v_idx, 0,
                           ftp_cavg->ftp_succ_fetches, ftp_hits_local_gp_ptr->cum_succ); g_idx++;
      g_idx = 0;
      ftp_hits_local_gp_ptr++;
    }
  }
}

static inline void fill_ftp_fail_gp (avgtime **g_avg, cavgtime **g_cavg)
{
  unsigned int url_all_failures = 0;
  int i, v_idx, all_idx, j = 0, grp_idx;
  FTPAvgTime *ftp_avg = NULL;
  avgtime *avg = NULL;
  FTP_fail_gp *ftp_fail_local_gp_ptr = ftp_fail_gp_ptr;
  FTP_fail_gp *ftp_fail_local_first_gp_ptr = NULL;
 
  NSDL2_GDF(NULL, NULL, "Method called");
 
  for(v_idx = 0; v_idx < sgrp_used_genrator_entries + 1; v_idx++)
  {
    avg = (avgtime *)g_avg[v_idx];
    for(grp_idx = 0; grp_idx < TOTAL_GRP_ENTERIES_WITH_GRP_KW; grp_idx++)
    {
      ftp_avg = (FTPAvgTime*)((char*)((char *)avg + (grp_idx * g_avg_size_only_grp)) + g_ftp_avgtime_idx);
      all_idx = j;
      j++;
      ftp_fail_local_first_gp_ptr = ftp_fail_local_gp_ptr;
      ftp_fail_local_gp_ptr++;
      for (i = 0; i < (TOTAL_USED_URL_ERR - 1); i++, j++)
      {
        GDF_COPY_VECTOR_DATA(ftp_fail_gp_idx, 0, j, 0, ftp_avg->ftp_error_codes[i + 1], ftp_fail_local_gp_ptr->failures);
        GDF_COPY_SCALAR_DATA(ftp_fail_gp_idx, 1, ftp_avg->ftp_error_codes[i + 1], ftp_fail_local_gp_ptr->failure_count);
        url_all_failures += ftp_avg->ftp_error_codes[i + 1];
        ftp_fail_local_gp_ptr++;
      }
      /*bug id 101742: using method convert_long_ps() */
      NSDL2_GDF(NULL, NULL, "FTP Failures/Sec url_all_failures = %lu ", url_all_failures);
      NSDL2_GDF(NULL, NULL, "FTP Failures/Sec after PS url_all_failures = %lu ", convert_long_ps(url_all_failures));

      GDF_COPY_VECTOR_DATA(ftp_fail_gp_idx, 0, all_idx, 0, convert_long_ps(url_all_failures), ftp_fail_local_first_gp_ptr->failures);
      GDF_COPY_SCALAR_DATA(ftp_fail_gp_idx, 1, url_all_failures, ftp_fail_local_first_gp_ptr->failure_count);
      url_all_failures = 0;
    }
  }
}

inline void fill_ftp_gp (avgtime **g_avg, cavgtime **g_cavg) {
  
  NSDL2_FTP(NULL, NULL, "Method called. g_avg = %p", g_avg);
  if (ftp_hits_gp_ptr)
    fill_ftp_hits_gp (g_avg, g_cavg);
  if (ftp_net_throughput_gp_ptr)
    fill_ftp_net_throughput_gp (g_avg, g_cavg);//TODO: send one argument g_cavg
  if (ftp_fail_gp_ptr)
    fill_ftp_fail_gp (g_avg, g_cavg);
}

void
print_ftp_throughput(FILE *fp1, FILE *fp2, int is_periodic, avgtime *avg, cavgtime *cavg)
{

double ftp_tcp_rx, ftp_tcp_tx;
double duration;
FTPAvgTime *ftp_avg = NULL;
FTPCAvgTime *ftp_cavg = NULL;

  if(!(global_settings->protocol_enabled & FTP_PROTOCOL_ENABLED)) {
    return; 
  }
  /* We have allocated average_time with the size of ftpAvgTime
   * also now we can point that using g_ftp_avgtime_idx
   */
  ftp_avg = (FTPAvgTime*)((char *)avg + g_ftp_avgtime_idx);
  ftp_cavg = (FTPCAvgTime*)((char *)cavg + g_ftp_cavgtime_idx);

  u_ns_8B_t  ftp_tot_tcp_rx = 0, ftp_tot_tcp_tx = 0;
  u_ns_8B_t ftp_con_made_rate, ftp_con_break_rate, ftp_hit_tot_rate, ftp_hit_succ_rate, ftp_hit_initited_rate; 
  u_ns_8B_t ftp_num_completed, ftp_num_initiated, ftp_num_succ; //,ftp_num_samples;

  char ftp_tbuffer[1024]="";
  
    NSDL2_REPORTING(NULL, NULL, "Method called");
    if (is_periodic) {
      duration = (double)((double)(global_settings->progress_secs)/1000.0);
      sprintf(ftp_tbuffer, " FTP (Rx=%'.3f)", (double)(((double)(ftp_avg->ftp_total_bytes))/(duration*128.0)));
      ftp_tcp_rx = ((double)(ftp_avg->ftp_rx_bytes))/(duration*128.0);
      ftp_tcp_tx = ((double)(ftp_avg->ftp_tx_bytes))/(duration*128.0);

      ftp_con_made_rate = (ftp_avg->ftp_num_con_succ * 1000)/global_settings->progress_secs;
      ftp_con_break_rate = (ftp_avg->ftp_num_con_break * 1000)/global_settings->progress_secs;
      ftp_num_completed = ftp_avg->ftp_num_tries;
      //ftp_num_samples = ftp_num_succ = ftp_avg->ftp_num_hits;
      ftp_num_succ = ftp_avg->ftp_num_hits;
      ftp_num_initiated = ftp_avg->ftp_fetches_started;
      ftp_hit_tot_rate = (ftp_num_completed * 1000)/global_settings->progress_secs;
      ftp_hit_succ_rate = (ftp_num_succ * 1000)/global_settings->progress_secs;
      ftp_hit_initited_rate = (ftp_num_initiated * 1000)/global_settings->progress_secs;
    } else {
        duration = (double)((double)(global_settings->test_duration)/1000.0);
        sprintf(ftp_tbuffer, " FTP (Rx=%'.3f)", (double)(((double)(ftp_cavg->ftp_c_tot_total_bytes))/(duration*128.0)));
        ftp_tcp_rx = ((double)(ftp_cavg->ftp_c_tot_rx_bytes))/(duration*128.0);
        ftp_tcp_tx = ((double)(ftp_cavg->ftp_c_tot_tx_bytes))/(duration*128.0);

        ftp_con_made_rate = (ftp_cavg->ftp_c_num_con_succ * 1000)/global_settings->test_duration;
        ftp_con_break_rate = (ftp_cavg->ftp_c_num_con_break * 1000)/global_settings->test_duration;
        ftp_num_completed = ftp_cavg->ftp_fetches_completed;
        //ftp_num_samples = ftp_num_succ = ftp_cavg->ftp_succ_fetches;
        ftp_num_succ = ftp_cavg->ftp_succ_fetches;
        NSDL2_MESSAGES(NULL, NULL, "  ftp_succ_fetches in ftp file = %llu, no of succ in ftp = %llu", ftp_cavg->ftp_succ_fetches , ftp_num_succ); 
        ftp_num_initiated = ftp_cavg->cum_ftp_fetches_started;
        ftp_hit_tot_rate = (ftp_num_completed * 1000)/global_settings->test_duration;
        ftp_hit_succ_rate = (ftp_num_succ * 1000)/global_settings->test_duration;
        ftp_hit_initited_rate = (ftp_num_initiated * 1000)/global_settings->test_duration;     
      }

      ftp_tot_tcp_rx = (u_ns_8B_t)ftp_cavg->ftp_c_tot_rx_bytes;
      ftp_tot_tcp_tx = (u_ns_8B_t)ftp_cavg->ftp_c_tot_tx_bytes;

       
      if (ftp_avg->ftp_num_hits) {
        if (global_settings->show_initiated)
        fprint2f(fp1, fp2, "    ftp hit rate (per sec): Initiated=%'llu completed=%'llu Success=%'llu\n",
        ftp_hit_initited_rate, ftp_hit_tot_rate, ftp_hit_succ_rate);
      else
        fprint2f(fp1, fp2, "    ftp hit rate (per sec): Total=%'llu Success=%'llu\n", ftp_hit_tot_rate, ftp_hit_succ_rate);

/*       sprintf(ubuffer, " ftp Body(Rx=%'llu)",  */
/*               (u_ns_8B_t)(avg->ftp_c_tot_total_bytes)); */

      fprint2f(fp1, fp2, "    ftp (Kbits/s) TCP(Rx=%'.3f Tx=%'.3f)\n",
                              ftp_tcp_rx, ftp_tcp_tx);
      fprint2f(fp1, fp2, "    ftp (Total Bytes) TCP(Rx=%'llu Tx=%'llu)\n",
                              ftp_tot_tcp_rx, ftp_tot_tcp_tx);

      fprint2f(fp1, fp2, "    ftp TCP Conns: Current=%'d Rate/s(Open=%'llu Close=%'llu) Total(Open=%'llu Close=%'llu)\n",
                              ftp_avg->ftp_num_connections,
                              ftp_con_made_rate,
                              ftp_con_break_rate,
                              ftp_cavg->ftp_c_num_con_succ,
                              ftp_cavg->ftp_c_num_con_break);
      }
  
}

void
ftp_log_summary_data (avgtime *avg, double *ftp_data, FILE *fp, cavgtime *cavg)
{
u_ns_8B_t num_completed, num_succ, num_samples;
//FTPAvgTime *ftp_avg = NULL;
FTPCAvgTime *ftp_cavg = NULL;

  NSDL2_MESSAGES(NULL, NULL, "Method Called");
  if(!(global_settings->protocol_enabled & FTP_PROTOCOL_ENABLED)) {
      NSDL2_MESSAGES(NULL, NULL, "Returning: FTP_PROTOCOL is not enabled.");
      return;
  }

  NSDL2_MESSAGES(NULL, NULL, "avg = %p, g_ftp_avgtime_idx = %d", avg, g_ftp_avgtime_idx);
  //ftp_avg = (FTPAvgTime *) ((char*) avg + g_ftp_avgtime_idx); 
  ftp_cavg = (FTPCAvgTime *) ((char*) avg + g_ftp_cavgtime_idx); 
  //NSDL2_MESSAGES(NULL, NULL, "ftp_avg = %p", ftp_avg);
    
     if (total_ftp_request_entries) {
       num_completed = ftp_cavg->ftp_fetches_completed;
       num_samples = num_succ = ftp_cavg->ftp_succ_fetches;
       NSDL2_MESSAGES(NULL, NULL, "num_completed = %llu, num_samples = num_succ = %llu", num_completed, num_samples);
                                  
       if (num_completed) {
          if (num_samples) {
             fprintf(fp, "101|ftp|10|ftp Min Time(Sec)|%1.3f\n", (double)(((double)(ftp_cavg->ftp_c_min_time))/1000.0));
             fprintf(fp, "101|ftp|20|ftp Avg Time(Sec)|%1.3f\n", (double)(((double)(ftp_cavg->ftp_c_tot_time))/((double)(1000.0*(double)num_samples))));
             fprintf(fp, "101|ftp|30|ftp Max Time(Sec)|%1.3f\n", (double)(((double)(ftp_cavg->ftp_c_max_time))/1000.0));
             fprintf(fp, "101|ftp|40|ftp Median Time(Sec)|%1.3f\n", ftp_data[0]);
             fprintf(fp, "101|ftp|50|ftp 80th percentile Time(Sec)|%1.3f\n", ftp_data[1]);
             fprintf(fp, "101|ftp|60|ftp 90th percentile Time(Sec)|%1.3f\n", ftp_data[2]);
             fprintf(fp, "101|ftp|70|ftp 95th percentile Time(Sec)|%1.3f\n", ftp_data[3]);
             fprintf(fp, "101|ftp|80|ftp 99th percentile Time(Sec)|%1.3f\n", ftp_data[4]);
             fprintf(fp, "101|ftp|90|ftp Total|%llu\n", num_completed);
             fprintf(fp, "101|ftp|100|ftp Success|%llu\n", num_succ);
             fprintf(fp, "101|ftp|110|ftp Failures|%llu\n", num_completed -  num_succ);
             fprintf(fp, "101|ftp|120|ftp Failure PCT|%1.2f\n", (double)((((double)((num_completed -  num_succ)))*100.0)/((double)(num_completed))));
            } else {
             fprintf(fp, "101|ftp|10|ftp Min Time(Sec)|-\n");
             fprintf(fp, "101|ftp|20|ftp Avg Time(Sec)|-\n");
             fprintf(fp, "101|ftp|30|ftp Max Time(Sec)|-\n");
             fprintf(fp, "101|ftp|40|ftp Median Time(Sec)|-\n");
             fprintf(fp, "101|ftp|50|ftp 80th percentile Time(Sec)|-\n");
             fprintf(fp, "101|ftp|60|ftp 90th percentile Time(Sec)|-\n");
             fprintf(fp, "101|ftp|70|ftp 95th percentile Time(Sec)|-\n");
             fprintf(fp, "101|ftp|80|ftp 99th percentile Time(Sec)|-\n");
             fprintf(fp, "101|ftp|90|ftp Total|%llu\n", num_completed);
             fprintf(fp, "101|ftp|100|ftp Success|%llu\n", num_succ);
             fprintf(fp, "101|ftp|110|ftp Failures|%llu\n", num_completed -  num_succ);
             fprintf(fp, "101|ftp|120|ftp Failure PCT|%1.2f\n", (double)((((double)((num_completed -  num_succ)))*100.0)/((double)(num_completed))));
            }
          } else {
              fprintf(fp, "101|ftp|10|ftp Min Time(Sec)|-\n");
              fprintf(fp, "101|ftp|20|ftp Avg Time(Sec)|-\n");
              fprintf(fp, "101|ftp|30|ftp Max Time(Sec)|-\n");
              fprintf(fp, "101|ftp|40|ftp Median Time(Sec)|-\n");
              fprintf(fp, "101|ftp|50|ftp 80th percentile Time(Sec)|-\n");
              fprintf(fp, "101|ftp|60|ftp 90th percentile Time(Sec)|-\n");
              fprintf(fp, "101|ftp|70|ftp 95th percentile Time(Sec)|-\n");
              fprintf(fp, "101|ftp|80|ftp 99th percentile Time(Sec)|-\n");
              fprintf(fp, "101|ftp|90|ftp Total|0\n");
              fprintf(fp, "101|ftp|100|ftp Success|0\n");
              fprintf(fp, "101|ftp|110|ftp Failures|0\n");
              fprintf(fp, "101|ftp|120|ftp Failure PCT|0.00\n");
          }
          fprintf(fp, "101|ftp|140|ftp Hits/Sec|%1.2f\n", (float) ((double)(ftp_cavg->ftp_fetches_completed * 1000)/ (double)(global_settings->test_duration)));
         }
  
}
