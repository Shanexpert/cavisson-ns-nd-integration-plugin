#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ns_error_codes.h"
#include "ns_msg_def.h"
#include "ns_gdf.h"
#include "ns_log.h"
#include "url.h"
#include "ns_ldap.h"
#include "netomni/src/core/ni_scenario_distribution.h"
#include "ns_group_data.h"
#include "ns_gdf.h"

static double convert_long_ps (Long_data row_data)
{
  NSDL2_GDF(NULL, NULL, "Method called, row_data = %lu", row_data);
  return((double )((row_data))/((double )global_settings->progress_secs/1000.0));
}

extern int loader_opcode;
void delete_ldap_timeout_timer(connection *cptr) {

  NSDL2_POP3(NULL, cptr, "Method called, timer type = %d", cptr->timer_ptr->timer_type);

  if ( cptr->timer_ptr->timer_type == AB_TIMEOUT_IDLE ) {
    NSDL2_POP3(NULL, cptr, "Deleting Idle timer.");
    dis_timer_del(cptr->timer_ptr);
  }
}

void ldap_timeout_handle( ClientData client_data, u_ns_ts_t now ) {

  connection* cptr;
  cptr = (connection *)client_data.p;
  VUser* vptr = cptr->vptr;

  NSDL2_LDAP(vptr, cptr, "Method Called, vptr=%p cptr=%p conn state=%d, request_type = %d", 
            vptr, cptr, cptr->conn_state,
            cptr->url_num->request_type);

  if(vptr->vuser_state == NS_VUSER_PAUSED){
    vptr->flags |= NS_VPTR_FLAGS_TIMER_PENDING;
    return;
  }  
  Close_connection( cptr , 0, now, NS_REQUEST_TIMEOUT, NS_COMPLETION_TIMEOUT);

}

static inline void fill_ldap_net_throughput_gp (avgtime **g_avg, cavgtime **g_cavg)
{
  double result;
  int g_idx = 0, v_idx, grp_idx;
  LDAPAvgTime *ldap_avg = NULL;
  LDAPCAvgTime *ldap_cavg = NULL;
  avgtime *avg = NULL;
  cavgtime *cavg = NULL;
  LDAP_Net_throughput_gp *ldap_net_throughput_local_gp_ptr = ldap_net_throughput_gp_ptr;

  NSDL2_GDF(NULL, NULL, "Method called");

  for(v_idx = 0; v_idx < sgrp_used_genrator_entries + 1; v_idx++)
  {
    avg = (avgtime *)g_avg[v_idx];
    cavg = (cavgtime *)g_cavg[v_idx];
    for(grp_idx = 0; grp_idx < TOTAL_GRP_ENTERIES_WITH_GRP_KW; grp_idx++)
    {
      /*bug id 101742: using method convert_kbps() */
      ldap_avg = (LDAPAvgTime*)((char*)((char *)avg + (grp_idx * g_avg_size_only_grp)) + g_ldap_avgtime_idx);
      ldap_cavg = (LDAPCAvgTime *)((char*)((char *)cavg + (grp_idx * g_cavg_size_only_grp)) + g_ldap_cavgtime_idx);

      result = (ldap_avg->ldap_tx_bytes)*8/((double)global_settings->progress_secs/1000);

      NSDL2_GDF(NULL, NULL, "LDAP Send Throughput result = %f ", result);
      NSDL2_GDF(NULL, NULL, "LDAP Send Throughput after kbps result = %f ", convert_kbps(result));

      GDF_COPY_VECTOR_DATA(ldap_net_throughput_gp_idx, g_idx, v_idx, 0,
                           convert_kbps(result), ldap_net_throughput_local_gp_ptr->avg_send_throughput); g_idx++;
      GDF_COPY_VECTOR_DATA(ldap_net_throughput_gp_idx, g_idx, v_idx, 0,
                           ldap_cavg->ldap_c_tot_tx_bytes, ldap_net_throughput_local_gp_ptr->tot_send_bytes); g_idx++;
     
      result = (ldap_avg->ldap_rx_bytes)*8/((double)global_settings->progress_secs/1000);

      NSDL2_GDF(NULL, NULL, "LDAP Receive Throughput result = %f ", result);
      NSDL2_GDF(NULL, NULL, "LDAP Receive Throughput after kbps result = %f ", convert_kbps(result));

      GDF_COPY_VECTOR_DATA(ldap_net_throughput_gp_idx, g_idx, v_idx, 0,
                           convert_kbps(result), ldap_net_throughput_local_gp_ptr->avg_recv_throughput); g_idx++;
      GDF_COPY_VECTOR_DATA(ldap_net_throughput_gp_idx, g_idx, v_idx, 0,
                           ldap_cavg->ldap_c_tot_rx_bytes, ldap_net_throughput_local_gp_ptr->tot_recv_bytes); g_idx++;
     
      result = (ldap_avg->ldap_num_con_succ)/((double)global_settings->progress_secs/1000);
      GDF_COPY_VECTOR_DATA(ldap_net_throughput_gp_idx, g_idx, v_idx, 0,
                           result, ldap_net_throughput_local_gp_ptr->avg_open_cps); g_idx++;
     
      GDF_COPY_VECTOR_DATA(ldap_net_throughput_gp_idx, g_idx, v_idx, 0,
                           ldap_cavg->ldap_c_num_con_succ, ldap_net_throughput_local_gp_ptr->tot_conn_open); g_idx++;
     
      result = (ldap_avg->ldap_num_con_break)/((double)global_settings->progress_secs/1000);
      GDF_COPY_VECTOR_DATA(ldap_net_throughput_gp_idx, g_idx, v_idx, 0,
                           result, ldap_net_throughput_local_gp_ptr->avg_close_cps); g_idx++;
     
      GDF_COPY_VECTOR_DATA(ldap_net_throughput_gp_idx, g_idx, v_idx, 0,
                           ldap_cavg->ldap_c_num_con_break, ldap_net_throughput_local_gp_ptr->tot_conn_close); g_idx++;
      g_idx = 0;
      ldap_net_throughput_local_gp_ptr++;
    }
  }
}

static inline void fill_ldap_hits_gp (avgtime **g_avg, cavgtime **g_cavg)
{  
  int g_idx = 0, v_idx, grp_idx;
  LDAPAvgTime *ldap_avg = NULL;
  LDAPCAvgTime *ldap_cavg = NULL;
  avgtime *avg = NULL;
  cavgtime *cavg = NULL;
  LDAP_hits_gp *ldap_hits_local_gp_ptr = ldap_hits_gp_ptr;

  NSDL2_GDF(NULL, NULL, "Method called");

  for(v_idx = 0; v_idx < sgrp_used_genrator_entries + 1; v_idx++)
  {
    avg = (avgtime *)g_avg[v_idx];
    cavg = (cavgtime *)g_cavg[v_idx];
    for(grp_idx = 0; grp_idx < TOTAL_GRP_ENTERIES_WITH_GRP_KW; grp_idx++)
    {
      /*bug id 101742: using method convert_long_ps() */
      ldap_avg = (LDAPAvgTime*)((char*)((char *)avg + (grp_idx * g_avg_size_only_grp)) + g_ldap_avgtime_idx);
      ldap_cavg = (LDAPCAvgTime *)((char*)((char *)cavg + (grp_idx * g_cavg_size_only_grp)) + g_ldap_cavgtime_idx);

      NSDL2_GDF(NULL, NULL, "LDAP Request Sent/Sec ldap_avg->ldap_fetches_started = %lu ", ldap_avg->ldap_fetches_started);
      NSDL2_GDF(NULL, NULL, "LDAP Request Sent/Sec after PS ldap_avg->ldap_fetches_started = %lu ", convert_long_ps(ldap_avg->ldap_fetches_started));

      GDF_COPY_VECTOR_DATA(ldap_hits_gp_idx, g_idx, v_idx, 0,
                           convert_long_ps(ldap_avg->ldap_fetches_started), ldap_hits_local_gp_ptr->url_req); g_idx++;
    
      NSDL2_GDF(NULL, NULL, "LDAP Hits/Sec ldap_avg->ldap_num_tries = %lu ", ldap_avg->ldap_num_tries);
      NSDL2_GDF(NULL, NULL, "LDAP Hits/Sec after PS ldap_avg->ldap_num_tries = %lu ", convert_long_ps(ldap_avg->ldap_num_tries));
 
      GDF_COPY_VECTOR_DATA(ldap_hits_gp_idx, g_idx, v_idx, 0,
                           convert_long_ps(ldap_avg->ldap_num_tries), ldap_hits_local_gp_ptr->tries); g_idx++;
     
      NSDL2_GDF(NULL, NULL, "Success LDAP Responses/Sec ldap_avg->ldap_num_hits = %lu ", ldap_avg->ldap_num_hits);
      NSDL2_GDF(NULL, NULL, "Success LDAP Responses/Sec after PS ldap_avg->ldap_num_hits = %lu ", convert_long_ps(ldap_avg->ldap_num_hits));

      GDF_COPY_VECTOR_DATA(ldap_hits_gp_idx, g_idx, v_idx, 0,
                           convert_long_ps(ldap_avg->ldap_num_hits), ldap_hits_local_gp_ptr->succ); g_idx++;
     
      // Here the "response" variable of `Times_data` data type is getting filled
     
      GDF_COPY_TIMES_VECTOR_DATA(ldap_hits_gp_idx, g_idx, v_idx, 0,
                                 ldap_avg->ldap_avg_time, ldap_avg->ldap_min_time, ldap_avg->ldap_max_time, ldap_avg->ldap_num_hits,
                                 ldap_hits_local_gp_ptr->response.avg_time,
                                 ldap_hits_local_gp_ptr->response.min_time,
                                 ldap_hits_local_gp_ptr->response.max_time,
                                 ldap_hits_local_gp_ptr->response.succ); g_idx++;
     
      // This need to be fixed as these two are 'long long'
      GDF_COPY_VECTOR_DATA(ldap_hits_gp_idx, g_idx, v_idx, 0,
                           ldap_cavg->ldap_fetches_completed, ldap_hits_local_gp_ptr->cum_tries); g_idx++;
     
      GDF_COPY_VECTOR_DATA(ldap_hits_gp_idx, g_idx, v_idx, 0,
                           ldap_cavg->ldap_succ_fetches, ldap_hits_local_gp_ptr->cum_succ); g_idx++;
      g_idx = 0;
      ldap_hits_local_gp_ptr++;
    }
  }
}

static inline void fill_ldap_fail_gp (avgtime **g_avg)
{
  unsigned int url_all_failures = 0;
  int i, v_idx, all_idx, j = 0, grp_idx;
  LDAPAvgTime *ldap_avg = NULL;
  avgtime *avg = NULL;
  LDAP_fail_gp *ldap_fail_local_gp_ptr = ldap_fail_gp_ptr;
  LDAP_fail_gp *ldap_fail_local_first_gp_ptr = NULL;

  NSDL2_GDF(NULL, NULL, "Method called");
  
  for(v_idx = 0; v_idx < sgrp_used_genrator_entries + 1; v_idx++)
  {
    avg = (avgtime *)g_avg[v_idx];
    for(grp_idx = 0; grp_idx < TOTAL_GRP_ENTERIES_WITH_GRP_KW; grp_idx++)
    {
      ldap_avg = (LDAPAvgTime*)((char*)((char *)avg + (grp_idx * g_avg_size_only_grp)) + g_ldap_avgtime_idx);
      all_idx = j;
      j++;
      ldap_fail_local_first_gp_ptr = ldap_fail_local_gp_ptr;
      ldap_fail_local_gp_ptr++;
      for (i = 0; i < (TOTAL_USED_URL_ERR - 1); i++, j++)
      {
        GDF_COPY_VECTOR_DATA(ldap_fail_gp_idx, 0, j, 0, ldap_avg->ldap_error_codes[i + 1], ldap_fail_local_gp_ptr->failures);
        url_all_failures += ldap_avg->ldap_error_codes[i + 1];
        ldap_fail_local_gp_ptr++;
      }
      /*bug id 101742: using method convert_long_ps() */
      NSDL2_GDF(NULL, NULL, "LDAP Failures/Sec url_all_failures = %lu ", url_all_failures);
      NSDL2_GDF(NULL, NULL, "LDAP Failures/Sec after PS url_all_failures = %lu ", convert_long_ps(url_all_failures));

      GDF_COPY_VECTOR_DATA(ldap_fail_gp_idx, 0, all_idx, 0, convert_long_ps(url_all_failures), ldap_fail_local_first_gp_ptr->failures);
      url_all_failures = 0;
    }
  }
}

inline void fill_ldap_gp (avgtime **g_avg, cavgtime **g_cavg) {
  
  NSDL2_LDAP(NULL, NULL, "Method called. g_avg = %p", g_avg);
  if (ldap_hits_gp_ptr)
    fill_ldap_hits_gp (g_avg, g_cavg);
  if (ldap_net_throughput_gp_ptr)
    fill_ldap_net_throughput_gp (g_avg, g_cavg);
  if (ldap_fail_gp_ptr)
    fill_ldap_fail_gp (g_avg);
}

void
print_ldap_throughput(FILE *fp1, FILE *fp2, int is_periodic, avgtime *avg, cavgtime *cavg)
{

double ldap_tcp_rx, ldap_tcp_tx;
double duration;
LDAPAvgTime *ldap_avg = NULL;
LDAPCAvgTime *ldap_cavg = NULL;

  if(!(global_settings->protocol_enabled & LDAP_PROTOCOL_ENABLED)) {
    return; 
  }
  /* We have allocated average_time with the size of ldapAvgTime
   * also now we can point that using g_ldap_avgtime_idx
   */
  ldap_avg = (LDAPAvgTime*)((char *)avg + g_ldap_avgtime_idx);
  ldap_cavg = (LDAPCAvgTime*)((char *)cavg + g_ldap_cavgtime_idx);

  u_ns_8B_t  ldap_tot_tcp_rx = 0, ldap_tot_tcp_tx = 0;
  u_ns_8B_t ldap_con_made_rate, ldap_con_break_rate, ldap_hit_tot_rate, ldap_hit_succ_rate, ldap_hit_initited_rate; 
  u_ns_8B_t ldap_num_completed, ldap_num_initiated, ldap_num_succ; //, ldap_num_samples;

  char ldap_tbuffer[1024]="";
  
  NSDL2_REPORTING(NULL, NULL, "Method called");
  if (is_periodic) {
    duration = (double)((double)(global_settings->progress_secs)/1000.0);
    sprintf(ldap_tbuffer, " LDAP (Rx=%'.3f)", (double)(((double)(ldap_avg->ldap_total_bytes))/(duration*128.0)));
    ldap_tcp_rx = ((double)(ldap_avg->ldap_rx_bytes))/(duration*128.0);
    ldap_tcp_tx = ((double)(ldap_avg->ldap_tx_bytes))/(duration*128.0);

    ldap_con_made_rate = (ldap_avg->ldap_num_con_succ * 1000)/global_settings->progress_secs;
    ldap_con_break_rate = (ldap_avg->ldap_num_con_break * 1000)/global_settings->progress_secs;
    ldap_num_completed = ldap_avg->ldap_num_tries;
    //ldap_num_samples = ldap_num_succ = ldap_avg->ldap_num_hits;
    ldap_num_succ = ldap_avg->ldap_num_hits;
    ldap_num_initiated = ldap_avg->ldap_fetches_started;
    ldap_hit_tot_rate = (ldap_num_completed * 1000)/global_settings->progress_secs;
    ldap_hit_succ_rate = (ldap_num_succ * 1000)/global_settings->progress_secs;
    ldap_hit_initited_rate = (ldap_num_initiated * 1000)/global_settings->progress_secs;
  } else {
    duration = (double)((double)(global_settings->test_duration)/1000.0);
    sprintf(ldap_tbuffer, " LDAP (Rx=%'.3f)", (double)(((double)(ldap_cavg->ldap_c_tot_total_bytes))/(duration*128.0)));
    ldap_tcp_rx = ((double)(ldap_cavg->ldap_c_tot_rx_bytes))/(duration*128.0);
    ldap_tcp_tx = ((double)(ldap_cavg->ldap_c_tot_tx_bytes))/(duration*128.0);

    ldap_con_made_rate = (ldap_cavg->ldap_c_num_con_succ * 1000)/global_settings->test_duration;
    ldap_con_break_rate = (ldap_cavg->ldap_c_num_con_break * 1000)/global_settings->test_duration;
    ldap_num_completed = ldap_cavg->ldap_fetches_completed;
    //ldap_num_samples = ldap_num_succ = ldap_cavg->ldap_succ_fetches;
    ldap_num_succ = ldap_cavg->ldap_succ_fetches;
    NSDL2_MESSAGES(NULL, NULL, "  ldap_succ_fetches in ldap file = %llu, no of succ in ldap = %llu", ldap_cavg->ldap_succ_fetches , ldap_num_succ); 
    ldap_num_initiated = ldap_cavg->cum_ldap_fetches_started;
    ldap_hit_tot_rate = (ldap_num_completed * 1000)/global_settings->test_duration;
    ldap_hit_succ_rate = (ldap_num_succ * 1000)/global_settings->test_duration;
    ldap_hit_initited_rate = (ldap_num_initiated * 1000)/global_settings->test_duration;     
  }

  ldap_tot_tcp_rx = (u_ns_8B_t)ldap_cavg->ldap_c_tot_rx_bytes;
  ldap_tot_tcp_tx = (u_ns_8B_t)ldap_cavg->ldap_c_tot_tx_bytes;

  if (ldap_avg->ldap_num_hits) {
    if (global_settings->show_initiated)
      fprint2f(fp1, fp2, "    ldap hit rate (per sec): Initiated=%'llu completed=%'llu Success=%'llu\n",
      ldap_hit_initited_rate, ldap_hit_tot_rate, ldap_hit_succ_rate);
    else
      fprint2f(fp1, fp2, "    ldap hit rate (per sec): Total=%'llu Success=%'llu\n", ldap_hit_tot_rate, ldap_hit_succ_rate);

/*       sprintf(ubuffer, " ldap Body(Rx=%'llu)",  */
/*               (u_ns_8B_t)(avg->ldap_c_tot_total_bytes)); */

    fprint2f(fp1, fp2, "    ldap (Kbits/s) TCP(Rx=%'.3f Tx=%'.3f)\n",
                             ldap_tcp_rx, ldap_tcp_tx);
    fprint2f(fp1, fp2, "    ldap (Total Bytes) TCP(Rx=%'llu Tx=%'llu)\n",
                              ldap_tot_tcp_rx, ldap_tot_tcp_tx);

    fprint2f(fp1, fp2, "    ldap TCP Conns: Current=%'d Rate/s(Open=%'llu Close=%'llu) Total(Open=%'llu Close=%'llu)\n",
                            ldap_avg->ldap_num_connections,
                            ldap_con_made_rate,
                            ldap_con_break_rate,
                            ldap_cavg->ldap_c_num_con_succ,
                            ldap_cavg->ldap_c_num_con_break);
  }
}

// Called by parent
inline void ldap_update_ldap_avgtime_size() {
  NSDL2_LDAP(NULL, NULL, "Method Called, g_avgtime_size = %d, g_ldap_avgtime_idx = %d",
                                          g_avgtime_size, g_ldap_avgtime_idx);

  if(global_settings->protocol_enabled & LDAP_PROTOCOL_ENABLED) {
    NSDL2_LDAP(NULL, NULL, "LDAP is enabled.");
    g_ldap_avgtime_idx = g_avgtime_size;
    g_avgtime_size += sizeof(LDAPAvgTime);

  } else {
    NSDL2_LDAP(NULL, NULL, "LDAP is disabled.");
  }

  NSDL2_LDAP(NULL, NULL, "After g_avgtime_size = %d, g_ldap_avgtime_idx = %d",
                                          g_avgtime_size, g_ldap_avgtime_idx);
}

// called by parent
inline void ldap_update_ldap_cavgtime_size(){

  NSDL2_LDAP(NULL, NULL, "Method Called, g_LDAPcavgtime_size = %d, g_ldap_cavgtime_idx = %d",
                                          g_cavgtime_size, g_ldap_cavgtime_idx);

  if(global_settings->protocol_enabled & LDAP_PROTOCOL_ENABLED) {
    NSDL2_LDAP(NULL, NULL, "LDAP is enabled.");
    g_ldap_cavgtime_idx = g_cavgtime_size;
    g_cavgtime_size += sizeof(LDAPCAvgTime);
  } else {
    NSDL2_LDAP(NULL, NULL, "LDAP is disabled.");
  }

  NSDL2_LDAP(NULL, NULL, "After g_cavgtime_size = %d, g_ldap_cavgtime_idx = %d",
                                          g_cavgtime_size, g_ldap_cavgtime_idx);
}

// Called by child
inline void ldap_set_ldap_avgtime_ptr() {

  NSDL2_LDAP(NULL, NULL, "Method Called");

  if(global_settings->protocol_enabled & LDAP_PROTOCOL_ENABLED) {
    NSDL2_LDAP(NULL, NULL, "LDAP is enabled.");
   /* We have allocated average_time with the size of ldapAvgTime
    * also now we can point that using g_ldap_avgtime_idx*/
    ldap_avgtime = (LDAPAvgTime*)((char *)average_time + g_ldap_avgtime_idx);
  } else {
    NSDL2_LDAP(NULL, NULL, "LDAP is disabled.");
    ldap_avgtime = NULL;
  }

  NSDL2_LDAP(NULL, NULL, "ldap_avgtime = %p", ldap_avgtime);
}

void
ldap_log_summary_data (avgtime *avg, double *ldap_data, FILE *fp, cavgtime *cavg)
{
u_ns_8B_t num_completed, num_succ, num_samples;
//LDAPAvgTime *ldap_avg = NULL;
LDAPCAvgTime *ldap_cavg = NULL;

  NSDL2_MESSAGES(NULL, NULL, "Method Called");
  if(!(global_settings->protocol_enabled & LDAP_PROTOCOL_ENABLED)) {
      NSDL2_MESSAGES(NULL, NULL, "Returning: LDAP_PROTOCOL is not enabled.");
      return;
  }

  NSDL2_MESSAGES(NULL, NULL, "avg = %p, g_ldap_avgtime_idx = %d", avg, g_ldap_avgtime_idx);
  //ldap_avg = (LDAPAvgTime *) ((char*) avg + g_ldap_avgtime_idx); 
  ldap_cavg = (LDAPCAvgTime *) ((char*) cavg + g_ldap_cavgtime_idx); 
  //NSDL2_MESSAGES(NULL, NULL, "ldap_avg = %p", ldap_avg);
    
     if (total_ldap_request_entries) {
       num_completed = ldap_cavg->ldap_fetches_completed;
       num_samples = num_succ = ldap_cavg->ldap_succ_fetches;
       NSDL2_MESSAGES(NULL, NULL, "num_completed = %llu, num_samples = num_succ = %llu", num_completed, num_samples);
                                  
       if (num_completed) {
          if (num_samples) {
             fprintf(fp, "101|ldap|10|ldap Min Time(Sec)|%1.3f\n", (double)(((double)(ldap_cavg->ldap_c_min_time))/1000.0));
             fprintf(fp, "101|ldap|20|ldap Avg Time(Sec)|%1.3f\n", (double)(((double)(ldap_cavg->ldap_c_tot_time))/((double)(1000.0*(double)num_samples))));
             fprintf(fp, "101|ldap|30|ldap Max Time(Sec)|%1.3f\n", (double)(((double)(ldap_cavg->ldap_c_max_time))/1000.0));
             fprintf(fp, "101|ldap|40|ldap Median Time(Sec)|%1.3f\n", ldap_data[0]);
             fprintf(fp, "101|ldap|50|ldap 80th percentile Time(Sec)|%1.3f\n", ldap_data[1]);
             fprintf(fp, "101|ldap|60|ldap 90th percentile Time(Sec)|%1.3f\n", ldap_data[2]);
             fprintf(fp, "101|ldap|70|ldap 95th percentile Time(Sec)|%1.3f\n", ldap_data[3]);
             fprintf(fp, "101|ldap|80|ldap 99th percentile Time(Sec)|%1.3f\n", ldap_data[4]);
             fprintf(fp, "101|ldap|90|ldap Total|%llu\n", num_completed);
             fprintf(fp, "101|ldap|100|ldap Success|%llu\n", num_succ);
             fprintf(fp, "101|ldap|110|ldap Failures|%llu\n", num_completed -  num_succ);
             fprintf(fp, "101|ldap|120|ldap Failure PCT|%1.2f\n", (double)((((double)((num_completed -  num_succ)))*100.0)/((double)(num_completed))));
            } else {
             fprintf(fp, "101|ldap|10|ldap Min Time(Sec)|-\n");
             fprintf(fp, "101|ldap|20|ldap Avg Time(Sec)|-\n");
             fprintf(fp, "101|ldap|30|ldap Max Time(Sec)|-\n");
             fprintf(fp, "101|ldap|40|ldap Median Time(Sec)|-\n");
             fprintf(fp, "101|ldap|50|ldap 80th percentile Time(Sec)|-\n");
             fprintf(fp, "101|ldap|60|ldap 90th percentile Time(Sec)|-\n");
             fprintf(fp, "101|ldap|70|ldap 95th percentile Time(Sec)|-\n");
             fprintf(fp, "101|ldap|80|ldap 99th percentile Time(Sec)|-\n");
             fprintf(fp, "101|ldap|90|ldap Total|%llu\n", num_completed);
             fprintf(fp, "101|ldap|100|ldap Success|%llu\n", num_succ);
             fprintf(fp, "101|ldap|110|ldap Failures|%llu\n", num_completed -  num_succ);
             fprintf(fp, "101|ldap|120|ldap Failure PCT|%1.2f\n", (double)((((double)((num_completed -  num_succ)))*100.0)/((double)(num_completed))));
            }
          } else {
              fprintf(fp, "101|ldap|10|ldap Min Time(Sec)|-\n");
              fprintf(fp, "101|ldap|20|ldap Avg Time(Sec)|-\n");
              fprintf(fp, "101|ldap|30|ldap Max Time(Sec)|-\n");
              fprintf(fp, "101|ldap|40|ldap Median Time(Sec)|-\n");
              fprintf(fp, "101|ldap|50|ldap 80th percentile Time(Sec)|-\n");
              fprintf(fp, "101|ldap|60|ldap 90th percentile Time(Sec)|-\n");
              fprintf(fp, "101|ldap|70|ldap 95th percentile Time(Sec)|-\n");
              fprintf(fp, "101|ldap|80|ldap 99th percentile Time(Sec)|-\n");
              fprintf(fp, "101|ldap|90|ldap Total|0\n");
              fprintf(fp, "101|ldap|100|ldap Success|0\n");
              fprintf(fp, "101|ldap|110|ldap Failures|0\n");
              fprintf(fp, "101|ldap|120|ldap Failure PCT|0.00\n");
          }
          fprintf(fp, "101|ldap|140|ldap Hits/Sec|%1.2f\n", (float) ((double)(ldap_cavg->ldap_fetches_completed * 1000)/ (double)(global_settings->test_duration)));
         }
  
}
