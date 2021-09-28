/******************************************************************
 * Name    :    ns_fill_gdf.c
 * Author  :    Anuj
 * Purpose :    This file contains the fuctions for filling the data in the Group structures, which will be called by the ns_gdf.c for populating the data.
 * Modification History: 1 - Changes required for making system and network groups "Vector" has been made. Anuj 12/19/07
 * Note:
 *   This file is included in deliver_report.c
 *
 * 10/09/07:  Anuj - Initial Version
*****************************************************************/

// To convert long long data from host to network format.
// this function will take long long data and split it into two longs and convert them into htonl format.
#include <time.h>
#include "ns_log.h"
#include "ns_gdf.h"
#include "ns_error_codes.h"
#include "ns_common.h"
#include "ns_http_cache_reporting.h"
#include "ns_netstorm_diagnostics.h"
#include "ns_monitoring.h"
#include "netomni/src/core/ni_user_distribution.h"
#include "netomni/src/core/ni_scenario_distribution.h"
#include "ns_trans_parse.h"
#include "ns_group_data.h"
#include "ns_appliance_health_monitor.h"
#include "ns_percentile.h"
#include "ns_exit.h"
#include "ns_h2_reporting.h"
#include "ns_error_msg.h"

#include "ns_write_rtg_data_in_db_or_csv.h"

extern Group_Info *group_data_ptr;
extern Graph_Info *graph_data_ptr;

 
static double kbps_factor;

void init_kbps_factor(){
  kbps_factor = (double)( 8 * 1000/(double)(global_settings->progress_secs * 1024));
  NSTL1(NULL, NULL, "kbps factor = %f", kbps_factor);
} 

//extern int test_run_gdf_count;

/* void htonll(long long in, unsigned int out[2]) */
/* { */
/*   int *l_long; */
/*   int *u_long; */
/*   NSDL2_GDF(vptr, cptr, "Method called"); */
/*   l_long = (long *)&in; */
/*   u_long = (long *)((char *)&in + 4); */
/*   out[0] = htonl(*l_long); */
/*   out[1] = htonl(*u_long); */
/* } */

// To convert long long data from network to host format.
// this function will take Long pointer of data and invertly join them to make long long to convert them network to host format.
/* unsigned long long ntohll(unsigned int in[2]) */
/* { */
/*   unsigned long long ll_value; */
/*   unsigned long long l_value; */
/*   unsigned long long u_value; */

/*   NSDL2_GDF(vptr, cptr, "Method called"); */
/*   l_value = (unsigned long long )ntohl(in[0]); */
/*   u_value = (unsigned long long )ntohl(in[1]);   */

/*   ll_value = ((u_value << 32) + l_value); */
/*   return(ll_value); */
/* } */


// All formula methods return in host format
// formula for converting the Row data from milli-sec to to Sec

// Issue - These methods are defined to return Uint but should be ULong. This is done as fprtinf(%u) give warning. To be fixed later.
double convert_sec (Long_data row_data)
{
  NSDL2_GDF(NULL, NULL, "Method called, row_data = %lu", row_data);
  return((double )((row_data))/1000.0);
}

// formula for converting the Row data (Periodic) to Per Second (PS)
double convert_ps (Long_data row_data)
{
  NSDL2_GDF(NULL, NULL, "Method called, row_data = %lu", row_data);
  return((double )((row_data))/((double )global_settings->progress_secs/1000.0));
}
double convert_long_ps (unsigned long long row_data)
{
  NSDL2_GDF(NULL, NULL, "Method called, row_data = %lu", row_data);
  return((double )((row_data))/((double )global_settings->progress_secs/1000.0));
}

// formula for converting the Row data (Periodic) to Per Minute (PM)
double convert_pm (Long_data row_data)
{
  NSDL2_GDF(NULL, NULL, "Method called, row_data = %lu", row_data);
  return((double )((row_data) * 60)/((double )global_settings->progress_secs/1000.0));
}

double convert_long_pm (unsigned long long row_data)
{
  NSDL2_GDF(NULL, NULL, "Method called, row_data = %lu", row_data);
  return((double )((row_data) * 60)/((double )global_settings->progress_secs/1000.0));
}

// formula for converting the Row data from Bits/Sec to Kilo Bits Per Sec (kbps)
double convert_kbps (Long_data row_data)
{
  NSDL2_GDF(NULL, NULL, "Method called, row_data = %f", row_data);
  return((double )((row_data))/1024.0);
}

// formula for converting the Row data (multiple of 100) in to Divide By 100 (dbh)
double convert_dbh(Long_data row_data)
{
  NSDL2_GDF(NULL, NULL, "Method called, row_data = %lu", row_data);
  return((double )((row_data))/100.0);
}

double convert_sec_long_long (char *row_data)
{
  NSDL2_GDF(NULL, NULL, "Method called");
  //return((double)(ntohll((Long_data *)row_data))/1000.0);
  return((*(Long_data *)row_data)/1000.0);
}

// formula for converting the Row data (Periodic) to Per Second (PS)
double convert_long_long_data_to_ps_long_long (unsigned long long row_data)
{
  NSDL2_GDF(NULL, NULL, "Method called");
  //return((double)(ntohll((Long_data *)row_data))/((double )global_settings->progress_secs/1000.0));
  return(((Long_data )row_data)/((double )global_settings->progress_secs/1000.0));
}

// formula for converting the Row data (Periodic) to Per Second (PS)
double convert_long_data_to_ps_long_long (unsigned int row_data)
{
  Long_data ret;
  NSDL2_GDF(NULL, NULL, "Method called. row_data = %d. per/sec = %.3f", row_data, ((Long_data )row_data * 1000.0)/((double )global_settings->progress_secs));
  
  ret = ((Long_data )row_data * 1000.0)/((double )global_settings->progress_secs);

  NSDL2_GDF(NULL, NULL, "ret = %.3f", ret);
  //return(((Long_data )row_data * 1000.0)/((double )global_settings->progress_secs));
  return ret;
}

// formula for converting the Row data (Periodic) to Per Second (PS)
double convert_ps_long_long (char *row_data)
{
  NSDL2_GDF(NULL, NULL, "Method called");
  //return((double)(ntohll((Long_data *)row_data))/((double )global_settings->progress_secs/1000.0));
  return((*(Long_data *)row_data)/((double )global_settings->progress_secs/1000.0));
}

// formula for converting the Row data (Periodic) to Per Minute (PM)
double convert_pm_long_long (char *row_data)
{
  NSDL2_GDF(NULL, NULL, "Method called");
  //return((double)(ntohll((Long_data *)row_data) * 60)/((double )global_settings->progress_secs/1000.0));
  return (((*(Long_data *)row_data) * 60)/((double )global_settings->progress_secs/1000.0));
}

// formula for converting the Row data from Bits/Sec to Kilo Bits Per Sec (kbps)
double convert_kbps_long_long (char *row_data)
{
  NSDL2_GDF(NULL, NULL, "Method called");
  return ((*(Long_data *)row_data)/1024.0);
}

// formula for converting the row data from unsigned long long to Kbps
double convert_8B_bytes_Kbps(u_ns_8B_t row_data)
{
  NSDL2_GDF(NULL, NULL, "Method called");
  return ((((Long_data)row_data)/ 1024 * 8)/((double )global_settings->progress_secs/1000.0));
}

// formula for converting the Row data (multiple of 100) in to Divide By 100 (dbh)
double convert_dbh_long_long(char *row_data)
{
  NSDL2_GDF(NULL, NULL, "Method called");
  //return((double)(ntohll((Long_data *)row_data))/100.0);
  return((*(Long_data *)row_data)/100.0);
}

// Function for Genrating the names of the Tunnels
void print_tunnel(char buff[], char *names[])
{
  int i = 0;
  NSDL2_GDF(NULL, NULL, "Method called");
  if(buff == NULL)
  {
    NS_EXIT(-1, CAV_ERR_1060019);
  }
  for (names[i] = strtok(buff," "); names[i] != NULL; names[i] = strtok(NULL, " ") )
    i++;
}

// Function for filliling the data in the structure of Vuser_gp
static inline void fill_vuser_gp (avgtime **g_avg)
{
  int g_idx = 0, gv_idx, j = 0, grp_idx;
  double result;
  avgtime *avg = NULL;
  avgtime *tmp_avg = NULL;

  NSDL2_GDF(NULL, NULL, "Method called, TOTAL_GRP_ENTERIES_WITH_GRP_KW = %d", TOTAL_GRP_ENTERIES_WITH_GRP_KW);

  if(vuser_gp_ptr == NULL) return;
  Vuser_gp *vuser_local_gp_ptr = vuser_gp_ptr;

  for(gv_idx = 0; gv_idx < sgrp_used_genrator_entries + 1; gv_idx++, j++)
  {
    tmp_avg = (avgtime *)g_avg[gv_idx];
    for(grp_idx = 0; grp_idx < TOTAL_GRP_ENTERIES_WITH_GRP_KW; grp_idx++)
    {
      avg = (avgtime*)((char*)tmp_avg + (grp_idx * g_avg_size_only_grp));
      GDF_COPY_VECTOR_DATA(vuser_group_idx, g_idx, j, 0, 
                           avg->running_users, vuser_local_gp_ptr->num_running_users); g_idx++;
      GDF_COPY_VECTOR_DATA(vuser_group_idx, g_idx, j, 0, 
                           avg->cur_vusers_active, vuser_local_gp_ptr->num_active_users); g_idx++;
      NSDL2_GDF(NULL, NULL, "grp_idx = %d, g_avg_size_only_grp = %d, tmp_avg = %p, avg = %p, cur_vusers_active = %d", grp_idx, g_avg_size_only_grp, tmp_avg, avg, avg->cur_vusers_active);

      GDF_COPY_VECTOR_DATA(vuser_group_idx, g_idx, j, 0, 
                           avg->cur_vusers_thinking,  vuser_local_gp_ptr->num_thinking_users); g_idx++;
      GDF_COPY_VECTOR_DATA(vuser_group_idx, g_idx, j, 0, 
                           avg->cur_vusers_waiting, vuser_local_gp_ptr->num_waiting_users); g_idx++;
      GDF_COPY_VECTOR_DATA(vuser_group_idx, g_idx, j, 0, 
                           avg->cur_vusers_cleanup, vuser_local_gp_ptr->num_idling_users); g_idx++;
      GDF_COPY_VECTOR_DATA(vuser_group_idx, g_idx, j, 0, 
                           avg->cur_vusers_blocked, vuser_local_gp_ptr->num_blocked_users); g_idx++;
      GDF_COPY_VECTOR_DATA(vuser_group_idx, g_idx, j, 0, 
                           avg->cur_vusers_paused, vuser_local_gp_ptr->num_paused_users); g_idx++;
      //SyncPoint Vusers
      GDF_COPY_VECTOR_DATA(vuser_group_idx, g_idx, j, 0, 
                           avg->cur_vusers_in_sp, vuser_local_gp_ptr->num_sp_users); g_idx++;

      GDF_COPY_VECTOR_DATA(vuser_group_idx, g_idx, j, 0, 
                           avg->num_connections, vuser_local_gp_ptr->num_connection); g_idx++;

      result = (avg->num_con_succ)/((double)global_settings->progress_secs/1000);
      GDF_COPY_VECTOR_DATA(vuser_group_idx, g_idx, j, 0,
                           result, vuser_local_gp_ptr->avg_open_cps); g_idx++;

      result = (avg->num_con_break)/((double)global_settings->progress_secs/1000);
      GDF_COPY_VECTOR_DATA(vuser_group_idx, g_idx, j, 0,
                           result, vuser_local_gp_ptr->avg_close_cps); g_idx++;

      result = (avg->tx_bytes)*8/((double)global_settings->progress_secs/1000);

      /*bug id 101742: using method convert_kbps() */ 
      NSDL2_GDF(NULL, NULL, "TCP Send Throughput result = %f TCP Send Throughput after kbps result = %f ",result,convert_kbps(result));

      GDF_COPY_VECTOR_DATA(vuser_group_idx, g_idx, j, 0,
                          convert_kbps(result), vuser_local_gp_ptr->avg_send_throughput); g_idx++;

      result = (avg->rx_bytes)*8/((double)global_settings->progress_secs/1000);

      NSDL2_GDF(NULL, NULL, "TCP Receive Throughput result = %f ", result);
      NSDL2_GDF(NULL, NULL, " TCP Receive Throughput after kbps result = %f ", convert_kbps(result));

      GDF_COPY_VECTOR_DATA(vuser_group_idx, g_idx, j, 0,
                           convert_kbps(result), vuser_local_gp_ptr->avg_recv_throughput); g_idx++;

      //result = (avg->bind_sock_fail_tot ? ((double)(((double)avg->bind_sock_fail_tot)/((double)((double)avg->bind_tries)))) : 0);
      NSDL2_GDF(NULL, NULL, "avg = %lf, bind_sock_fail_tot = %d, bind_sock_fail_min = %d, bind_sock_fail_max = %d", 
                             avg->bind_sock_fail_tot, avg->bind_sock_fail_tot, avg->bind_sock_fail_min , 
                             avg->bind_sock_fail_max);
      if(avg->bind_sock_fail_tot)
      {
        GDF_COPY_TIMES_VECTOR_DATA(vuser_group_idx, g_idx, j, 0,
                                  (double)(((double)avg->bind_sock_fail_tot) /
                                             ((double)(1000.0*(double)avg->fetches_started))*1000),
                                       avg->bind_sock_fail_min,
                                       avg->bind_sock_fail_max, avg->fetches_started,
                                       vuser_local_gp_ptr->sock_bind.avg_time,
                                       vuser_local_gp_ptr->sock_bind.min_time,
                                       vuser_local_gp_ptr->sock_bind.max_time,
                                       vuser_local_gp_ptr->sock_bind.succ); g_idx++;
      }
      else
      {
        GDF_COPY_TIMES_VECTOR_DATA(vuser_group_idx, g_idx, j, 0,
                                       0, -1, 0, avg->fetches_started,
                                       vuser_local_gp_ptr->sock_bind.avg_time,
                                       vuser_local_gp_ptr->sock_bind.min_time,
                                       vuser_local_gp_ptr->sock_bind.max_time,
                                       vuser_local_gp_ptr->sock_bind.succ); g_idx++;
      }

      g_idx = 0;
      vuser_local_gp_ptr++;
    }
  }
}

// Function for filliling the data in the structure of Vuser_gp
void log_vuser_gp ()
{
  if(vuser_gp_ptr == NULL) return;
  NSDL2_GDF(NULL, NULL, "Method called");
  
  fprintf(gui_fp, "\nVuser Info: Running Vusers=%0.0f, Active Vusers=%0.0f, Thinking Vusers=%0.0f, Waiting Vusers=%0.0f, Idling Vusers=%0.0f, Number Of Connections=%0.0f, SyncPoint Vusers=%0.0f, TCP Connections Open/Sec=%0.0f, TCP Connections Close/Sec=%0.0f, Network Throughput: TCP Send Throughput (Kbps)=%6.3f, TCP Receive Throughput (Kbps)=%6.3f\n",
          (vuser_gp_ptr->num_running_users),
          (vuser_gp_ptr->num_active_users),
          (vuser_gp_ptr->num_thinking_users),
          (vuser_gp_ptr->num_waiting_users),
          (vuser_gp_ptr->num_idling_users),
          (vuser_gp_ptr->num_connection),
          (vuser_gp_ptr->num_sp_users),
          (vuser_gp_ptr->avg_open_cps),
          (vuser_gp_ptr->avg_close_cps),
          convert_kbps(vuser_gp_ptr->avg_send_throughput),
          convert_kbps(vuser_gp_ptr->avg_recv_throughput));
}

// Function for filliling the data in the structure of SSL_gp
static inline void fill_ssl_gp (avgtime **g_avg, cavgtime **g_cavg)
{
  if(ssl_gp_ptr == NULL) return;
  int g_idx = 0, gv_idx, grp_idx;
  avgtime *avg = NULL;
  cavgtime *cavg = NULL;
  avgtime *tmp_avg = NULL;
  cavgtime *tmp_cavg = NULL;

  SSL_gp *ssl_local_gp_ptr = ssl_gp_ptr;

  NSDL2_GDF(NULL, NULL, "Method called");
  for(gv_idx = 0; gv_idx < sgrp_used_genrator_entries + 1; gv_idx++)
  {
    tmp_avg = (avgtime *)g_avg[gv_idx];
    tmp_cavg = (cavgtime *)g_cavg[gv_idx];
    for(grp_idx = 0; grp_idx < TOTAL_GRP_ENTERIES_WITH_GRP_KW; grp_idx++)
    {
      /*bug id 101742: using method convert_ps() */
      avg = (avgtime*)((char*)tmp_avg + (grp_idx * g_avg_size_only_grp));
      cavg = (cavgtime*)((char*)tmp_cavg + (grp_idx * g_cavg_size_only_grp));
  
      NSDL2_GDF(NULL, NULL, "SSL New Sessions/Sec avg->ssl_new = %f SSL New Sessions/Sec after PS  avg->ssl_new = %f ",avg->ssl_new,convert_ps(avg->ssl_new));
      GDF_COPY_VECTOR_DATA(ssl_gp_idx, g_idx, gv_idx, 0,
                           convert_ps(avg->ssl_new), ssl_local_gp_ptr->ssl_new); g_idx++;

      NSDL2_GDF(NULL, NULL, "SSL Total New Sessions cavg->c_ssl_new = %lu ", cavg->c_ssl_new);
      NSDL2_GDF(NULL, NULL, "SSL Total New Sessions after PS cavg->c_ssl_new = %lu ", convert_long_ps(cavg->c_ssl_new));     

      GDF_COPY_VECTOR_DATA(ssl_gp_idx, g_idx, gv_idx, 0,
                           convert_long_ps(cavg->c_ssl_new), ssl_local_gp_ptr->tot_ssl_new); g_idx++;

      NSDL2_GDF(NULL, NULL, "SSL Reused Sessions/Sec avg->ssl_reused = %f ", avg->ssl_reused);
      NSDL2_GDF(NULL, NULL, "SSL Reused Sessions/Sec after PS avg->ssl_reused = %f ", convert_ps(avg->ssl_reused));
     
      GDF_COPY_VECTOR_DATA(ssl_gp_idx, g_idx, gv_idx, 0,
                          convert_ps(avg->ssl_reused), ssl_local_gp_ptr->ssl_reused); g_idx++;
     
      NSDL2_GDF(NULL, NULL, "SSL Total Reused Sessions cavg->c_ssl_reused = %lu ", cavg->c_ssl_reused);
      NSDL2_GDF(NULL, NULL, "SSL Total Reused Sessions after PS cavg->c_ssl_reused = %lu ", convert_long_ps(cavg->c_ssl_reused));

      GDF_COPY_VECTOR_DATA(ssl_gp_idx, g_idx, gv_idx, 0,
                           convert_long_ps(cavg->c_ssl_reused), ssl_local_gp_ptr->tot_ssl_reused); g_idx++;

      NSDL2_GDF(NULL, NULL, "SSL Reuse attempted/Sec avg->ssl_reuse_attempted = %f ", avg->ssl_reuse_attempted);
      NSDL2_GDF(NULL, NULL, "SSL Reuse attempted/Sec after PS avg->ssl_reuse_attempted = %f ", convert_ps(avg->ssl_reuse_attempted));
     
      GDF_COPY_VECTOR_DATA(ssl_gp_idx, g_idx, gv_idx, 0,
                           convert_ps(avg->ssl_reuse_attempted), ssl_local_gp_ptr->ssl_reuse_attempted); g_idx++;

      NSDL2_GDF(NULL, NULL, "SSL Total  Reuse attempted cavg->c_ssl_reuse_attempted = %lu ", cavg->c_ssl_reuse_attempted);
      NSDL2_GDF(NULL, NULL, "SSL Total  Reuse attempted after PS cavg->c_ssl_reuse_attempted = %lu ", convert_long_ps(cavg->c_ssl_reuse_attempted));
     
      GDF_COPY_VECTOR_DATA(ssl_gp_idx, g_idx, gv_idx, 0,
                           convert_long_ps(cavg->c_ssl_reuse_attempted), ssl_local_gp_ptr->tot_ssl_reuse_attempted); g_idx++;

      NSDL2_GDF(NULL, NULL, "SSL Write Failures/Sec avg->url_error_codes[NS_REQUEST_SSLWRITE_FAIL] = %lu ", avg->url_error_codes[NS_REQUEST_SSLWRITE_FAIL]);
      NSDL2_GDF(NULL, NULL, "SSL Write Failures/Sec after PS avg->url_error_codes[NS_REQUEST_SSLWRITE_FAIL] = %lu ", convert_long_ps(avg->url_error_codes[NS_REQUEST_SSLWRITE_FAIL]));
     
      GDF_COPY_VECTOR_DATA(ssl_gp_idx, g_idx, gv_idx, 0,
                           convert_long_ps(avg->url_error_codes[NS_REQUEST_SSLWRITE_FAIL]), ssl_local_gp_ptr->ssl_write_fail); g_idx++;

        NSDL2_GDF(NULL, NULL, "SSL Handshake Failures/Sec avg->url_error_codes[NS_REQUEST_SSL_HSHAKE_FAIL] = %lu SSL Handshake Failures/Sec after PS avg->url_error_codes[NS_REQUEST_SSL_HSHAKE_FAIL] = %lu ",avg->url_error_codes[NS_REQUEST_SSL_HSHAKE_FAIL],convert_long_ps(avg->url_error_codes[NS_REQUEST_SSL_HSHAKE_FAIL]));
     
      GDF_COPY_VECTOR_DATA(ssl_gp_idx, g_idx, gv_idx, 0,
                           convert_long_ps(avg->url_error_codes[NS_REQUEST_SSL_HSHAKE_FAIL]), ssl_local_gp_ptr->ssl_handshake_fail); g_idx++;
     
      g_idx = 0;
      ssl_local_gp_ptr++;
    }
  }
}

// Function for filliling the data in the structure of Net_throughput_gp
static inline void fill_smtp_net_throughput_gp (avgtime **g_avg, cavgtime **g_cavg)
{
  if(smtp_net_throughput_gp_ptr == NULL) return;
  SMTP_Net_throughput_gp *smtp_net_throughput_local_gp_ptr = smtp_net_throughput_gp_ptr; 
  double result;
  int g_idx = 0, gv_idx, grp_idx; 
  avgtime *avg = NULL;
  cavgtime *cavg = NULL;
  avgtime *tmp_avg = NULL;
  cavgtime *tmp_cavg = NULL;

  NSDL2_GDF(NULL, NULL, "Method called");
  for(gv_idx = 0; gv_idx < sgrp_used_genrator_entries + 1; gv_idx++)
  {
    tmp_avg = (avgtime *)g_avg[gv_idx];
    tmp_cavg = (cavgtime *)g_cavg[gv_idx];
    for(grp_idx = 0; grp_idx < TOTAL_GRP_ENTERIES_WITH_GRP_KW; grp_idx++)
    {
      /*Bug id 101742: using method convert_kbps() */
      avg = (avgtime*)((char*)tmp_avg + (grp_idx * g_avg_size_only_grp));
      cavg = (cavgtime*)((char*)tmp_cavg + (grp_idx * g_cavg_size_only_grp));

      result = (avg->smtp_tx_bytes)*8/((double)global_settings->progress_secs/1000);
      GDF_COPY_VECTOR_DATA(smtp_net_throughput_gp_idx, g_idx, gv_idx, 0,
                           convert_kbps(result), smtp_net_throughput_local_gp_ptr->avg_send_throughput); g_idx++;
      GDF_COPY_VECTOR_DATA(smtp_net_throughput_gp_idx, g_idx, gv_idx, 0,
                           convert_kbps(cavg->smtp_c_tot_tx_bytes), smtp_net_throughput_local_gp_ptr->tot_send_bytes); g_idx++;
      NSDL2_GDF(NULL, NULL, " SMTP Send Throughput = %f ", result);
      NSDL2_GDF(NULL, NULL, " SMTP Send Throughput after kbps result = %f ", convert_kbps(result));
     
      result = (avg->smtp_rx_bytes)*8/((double)global_settings->progress_secs/1000);
      GDF_COPY_VECTOR_DATA(smtp_net_throughput_gp_idx, g_idx, gv_idx, 0,
                           convert_kbps(result), smtp_net_throughput_local_gp_ptr->avg_recv_throughput); g_idx++;
      GDF_COPY_VECTOR_DATA(smtp_net_throughput_gp_idx, g_idx, gv_idx, 0,
                           convert_kbps(cavg->smtp_c_tot_rx_bytes), smtp_net_throughput_local_gp_ptr->tot_recv_bytes); g_idx++;
      NSDL2_GDF(NULL, NULL, " SMTP Receive Throughput = %f ", result);
      NSDL2_GDF(NULL, NULL, " SMTP Receive Throughput after kbps result = %f ", convert_kbps(result));
     
      result = (avg->smtp_num_con_succ)/((double)global_settings->progress_secs/1000);
      GDF_COPY_VECTOR_DATA(smtp_net_throughput_gp_idx, g_idx, gv_idx, 0, 
                           result, smtp_net_throughput_local_gp_ptr->avg_open_cps); g_idx++;
     
      GDF_COPY_VECTOR_DATA(smtp_net_throughput_gp_idx, g_idx, gv_idx, 0,
                           cavg->smtp_c_num_con_succ, smtp_net_throughput_local_gp_ptr->tot_conn_open); g_idx++;
     
      result = (avg->smtp_num_con_break)/((double)global_settings->progress_secs/1000);
      GDF_COPY_VECTOR_DATA(smtp_net_throughput_gp_idx, g_idx, gv_idx, 0,
                           result, smtp_net_throughput_local_gp_ptr->avg_close_cps); g_idx++;
     
      GDF_COPY_VECTOR_DATA(smtp_net_throughput_gp_idx, g_idx, gv_idx, 0,
                           cavg->smtp_c_num_con_break, smtp_net_throughput_local_gp_ptr->tot_conn_close); g_idx++;
      g_idx = 0;
      smtp_net_throughput_local_gp_ptr++;
    }
  }
}

static inline void fill_pop3_net_throughput_gp (avgtime **g_avg, cavgtime **g_cavg)
{
  if(pop3_net_throughput_gp_ptr == NULL) return;
  POP3_Net_throughput_gp *pop3_net_throughput_local_gp_ptr = pop3_net_throughput_gp_ptr;
  double result;
  int g_idx = 0, gv_idx, grp_idx;
  avgtime *avg = NULL;
  cavgtime *cavg = NULL;
  avgtime *tmp_avg = NULL;
  cavgtime *tmp_cavg = NULL;

  NSDL2_GDF(NULL, NULL, "Method called");
  
  for(gv_idx = 0; gv_idx < sgrp_used_genrator_entries + 1; gv_idx++)
  {
    tmp_avg = (avgtime *)g_avg[gv_idx];
    tmp_cavg = (cavgtime *)g_cavg[gv_idx];
    for(grp_idx = 0; grp_idx < TOTAL_GRP_ENTERIES_WITH_GRP_KW; grp_idx++)
    {
      /*Bug id 101742: using method convert_kbps() */
      avg = (avgtime*)((char*)tmp_avg + (grp_idx * g_avg_size_only_grp));
      cavg = (cavgtime*)((char*)tmp_cavg + (grp_idx * g_cavg_size_only_grp));

      result = (avg->pop3_tx_bytes)*8/((double)global_settings->progress_secs/1000);
      GDF_COPY_VECTOR_DATA(pop3_net_throughput_gp_idx, g_idx, gv_idx, 0,
                           convert_kbps(result), pop3_net_throughput_local_gp_ptr->avg_send_throughput); g_idx++;
      GDF_COPY_VECTOR_DATA(pop3_net_throughput_gp_idx, g_idx, gv_idx, 0,
                           convert_kbps(cavg->pop3_c_tot_tx_bytes), pop3_net_throughput_local_gp_ptr->tot_send_bytes); g_idx++;
      NSDL2_GDF(NULL, NULL, " pop3 Send Throughput = %f ", result);
      NSDL2_GDF(NULL, NULL, " pop3 Send Throughput after kbps result = %f ", convert_kbps(result));
     
      result = (avg->pop3_rx_bytes)*8/((double)global_settings->progress_secs/1000);
      GDF_COPY_VECTOR_DATA(pop3_net_throughput_gp_idx, g_idx, gv_idx, 0,
                           convert_kbps(result), pop3_net_throughput_local_gp_ptr->avg_recv_throughput); g_idx++;
      GDF_COPY_VECTOR_DATA(pop3_net_throughput_gp_idx, g_idx, gv_idx, 0,
                           convert_kbps(cavg->pop3_c_tot_rx_bytes), pop3_net_throughput_local_gp_ptr->tot_recv_bytes); g_idx++;
      NSDL2_GDF(NULL, NULL, " pop3 Receive Throughput = %f ", result);
      NSDL2_GDF(NULL, NULL, " pop3 Receive Throughput after kbps result = %f ", convert_kbps(result));
     
      result = (avg->pop3_num_con_succ)/((double)global_settings->progress_secs/1000);
      GDF_COPY_VECTOR_DATA(pop3_net_throughput_gp_idx, g_idx, gv_idx, 0, 
                           result, pop3_net_throughput_local_gp_ptr->avg_open_cps); g_idx++;
     
      GDF_COPY_VECTOR_DATA(pop3_net_throughput_gp_idx, g_idx, gv_idx, 0,
                           cavg->pop3_c_num_con_succ, pop3_net_throughput_local_gp_ptr->tot_conn_open); g_idx++;
     
      result = (avg->pop3_num_con_break)/((double)global_settings->progress_secs/1000);
      GDF_COPY_VECTOR_DATA(pop3_net_throughput_gp_idx, g_idx, gv_idx, 0,
                           result, pop3_net_throughput_local_gp_ptr->avg_close_cps); g_idx++;
     
      GDF_COPY_VECTOR_DATA(pop3_net_throughput_gp_idx, g_idx, gv_idx, 0,
                           cavg->pop3_c_num_con_break, pop3_net_throughput_local_gp_ptr->tot_conn_close); g_idx++;
      g_idx = 0;
      pop3_net_throughput_local_gp_ptr++;
    }
  }
}

/* DNS */
static inline void fill_dns_net_throughput_gp (avgtime **g_avg, cavgtime **g_cavg)
{
  if(dns_net_throughput_gp_ptr == NULL) return;
  DNS_Net_throughput_gp *dns_net_throughput_local_gp_ptr = dns_net_throughput_gp_ptr;
  double result;
  int g_idx = 0, gv_idx, grp_idx; 
  avgtime *avg = NULL;
  cavgtime *cavg = NULL;
  avgtime *tmp_avg = NULL;
  cavgtime *tmp_cavg = NULL;

  NSDL2_GDF(NULL, NULL, "Method called");

  for(gv_idx = 0; gv_idx < sgrp_used_genrator_entries + 1; gv_idx++)
  {
    tmp_avg = (avgtime *)g_avg[gv_idx];  
    tmp_cavg = (cavgtime *)g_cavg[gv_idx];  
    for(grp_idx = 0; grp_idx < TOTAL_GRP_ENTERIES_WITH_GRP_KW; grp_idx++)
    {
      avg = (avgtime*)((char*)tmp_avg + (grp_idx * g_avg_size_only_grp));
      cavg = (cavgtime*)((char*)tmp_cavg + (grp_idx * g_cavg_size_only_grp));

      result = (avg->dns_tx_bytes)*8/((double)global_settings->progress_secs/1000);
      GDF_COPY_VECTOR_DATA(dns_net_throughput_gp_idx, g_idx, gv_idx, 0,
                           result, dns_net_throughput_local_gp_ptr->avg_send_throughput); g_idx++;
      GDF_COPY_VECTOR_DATA(dns_net_throughput_gp_idx, g_idx, gv_idx, 0,
                           cavg->dns_c_tot_tx_bytes, dns_net_throughput_local_gp_ptr->tot_send_bytes); g_idx++;
     
      result = (avg->dns_rx_bytes)*8/((double)global_settings->progress_secs/1000);
      GDF_COPY_VECTOR_DATA(dns_net_throughput_gp_idx, g_idx, gv_idx, 0,
                           result, dns_net_throughput_local_gp_ptr->avg_recv_throughput); g_idx++;
      GDF_COPY_VECTOR_DATA(dns_net_throughput_gp_idx, g_idx, gv_idx, 0,
                           cavg->dns_c_tot_rx_bytes, dns_net_throughput_local_gp_ptr->tot_recv_bytes); g_idx++;
     
      result = (avg->dns_num_con_succ)/((double)global_settings->progress_secs/1000);
      GDF_COPY_VECTOR_DATA(dns_net_throughput_gp_idx, g_idx, gv_idx, 0,
                           result, dns_net_throughput_local_gp_ptr->avg_open_cps); g_idx++;
     
      GDF_COPY_VECTOR_DATA(dns_net_throughput_gp_idx, g_idx, gv_idx, 0,
                           cavg->dns_c_num_con_succ, dns_net_throughput_local_gp_ptr->tot_conn_open); g_idx++;
     
      result = (avg->dns_num_con_break)/((double)global_settings->progress_secs/1000);
      GDF_COPY_VECTOR_DATA(dns_net_throughput_gp_idx, g_idx, gv_idx, 0,
                           result, dns_net_throughput_local_gp_ptr->avg_close_cps); g_idx++;
     
      GDF_COPY_VECTOR_DATA(dns_net_throughput_gp_idx, g_idx, gv_idx, 0,
                            cavg->dns_c_num_con_break, dns_net_throughput_local_gp_ptr->tot_conn_close); g_idx++;
      g_idx = 0;
      dns_net_throughput_local_gp_ptr++;
    }
  }
}

static inline void log_ssl_gp ()
{
  if(ssl_gp_ptr == NULL) return;
  NSDL2_GDF(NULL, NULL, "Method called");
  fprintf(gui_fp, "\nSSL New Sessions/Sec=%6.3f, SSL Reused Sessions/Sec=%6.3f, SSL Reuse attempted/Sec=%6.3f, SSL Write Failures/Sec=%6.3f, SSL Handshake Failures/Sec=%6.3f\n",
  convert_ps(ssl_gp_ptr->ssl_new) ,
  convert_ps(ssl_gp_ptr->ssl_reused) ,
  convert_ps(ssl_gp_ptr->ssl_reuse_attempted),
  convert_ps(ssl_gp_ptr->ssl_write_fail),
  convert_ps(ssl_gp_ptr->ssl_handshake_fail));
}

// Function for filliling the data in the structure of Url_hits_gp
static inline void fill_url_hits_gp (avgtime **g_avg, cavgtime **g_cavg)
{
  //Long_data succ;
  int g_idx = 0, gv_idx, grp_idx;
  double result;
  if(url_hits_gp_ptr == NULL) return;
  NSDL2_GDF(NULL, NULL, "Method called");
  avgtime *avg = NULL;
  cavgtime *cavg = NULL;
  avgtime *tmp_avg = NULL;
  cavgtime *tmp_cavg = NULL;

  Url_hits_gp *url_hits_local_gp_ptr = url_hits_gp_ptr;
  for(gv_idx = 0; gv_idx < sgrp_used_genrator_entries + 1; gv_idx++)
  {
    tmp_avg = (avgtime*)g_avg[gv_idx];
    tmp_cavg = (cavgtime*)g_cavg[gv_idx];
    for(grp_idx = 0; grp_idx < TOTAL_GRP_ENTERIES_WITH_GRP_KW; grp_idx++)
    {
      /*bug id 101742: using method convert_long_ps() */
      avg = (avgtime*)((char*)tmp_avg + (grp_idx * g_avg_size_only_grp));
      cavg = (cavgtime*)((char*)tmp_cavg + (grp_idx * g_cavg_size_only_grp));

        NSDL2_GDF(NULL, NULL, "HTTP Request Started/Sec avg->fetches_started = %lu HTTP Request Started/Sec after PS avg->fetches_started = %lu ",avg->fetches_started,convert_long_ps(avg->fetches_started));
      GDF_COPY_VECTOR_DATA(url_hits_gp_idx, g_idx, gv_idx, 0,
                           convert_long_ps(avg->fetches_started), url_hits_local_gp_ptr->url_req); g_idx++;
      NSDL2_GDF(NULL, NULL, "HTTP Request Sent/Sec avg->fetches_sent = %lu ", avg->fetches_sent);
      NSDL2_GDF(NULL, NULL, "HTTP Request Sent/Sec after PS avg->fetches_sent = %lu ", convert_long_ps(avg->fetches_sent));

      GDF_COPY_VECTOR_DATA(url_hits_gp_idx, g_idx, gv_idx, 0,
                         convert_long_ps(avg->fetches_sent), url_hits_local_gp_ptr->url_sent); g_idx++;
      NSDL2_GDF(NULL, NULL, "HTTP Request Completed/Sec avg->num_tries = %lu ", avg->num_tries);
      NSDL2_GDF(NULL, NULL, "HTTP Request Completed/Sec after PS avg->num_tries = %lu ", convert_long_ps(avg->num_tries));
     
      GDF_COPY_VECTOR_DATA(url_hits_gp_idx, g_idx, gv_idx, 0,
                          convert_long_ps(avg->num_tries), url_hits_local_gp_ptr->tries); g_idx++;
      NSDL2_GDF(NULL, NULL, "HTTP Request Successful/Sec avg->num_hits = %lu ", avg->num_hits);
      NSDL2_GDF(NULL, NULL, "HTTP Request Successful/Sec after PS avg->num_hits = %lu ", convert_long_ps(avg->num_hits));

      GDF_COPY_VECTOR_DATA(url_hits_gp_idx, g_idx, gv_idx, 0,
                           convert_long_ps(avg->num_hits), url_hits_local_gp_ptr->succ); g_idx++;

      // Here the "response" variable of `Times_data` data type is getting filled
      NSDL2_GDF(NULL, NULL, " HTTP Response Time after ms avg->url_overall_avg_time = %lu avg->url_overall_min_time = %lu avg->url_overall_max_time = %lu avg->num_tries = %lu", avg->url_overall_avg_time, avg->url_overall_min_time, avg->url_overall_max_time, avg->num_tries);
      GDF_COPY_TIMES_VECTOR_DATA(url_hits_gp_idx, g_idx, gv_idx, 0, 
                               avg->url_overall_avg_time, avg->url_overall_min_time, 
                               avg->url_overall_max_time, avg->num_tries,
                               url_hits_local_gp_ptr->response.avg_time,
                               url_hits_local_gp_ptr->response.min_time,
                               url_hits_local_gp_ptr->response.max_time,
                               url_hits_local_gp_ptr->response.succ); g_idx++;

      if(avg->num_hits)
      {
        NSDL2_GDF(NULL, NULL, " HTTP Successful Response Time after ms avg->avg_time = %lu avg->min_time = %lu avg->max_time = %lu avg->num_hits = %lu", avg->avg_time, avg->min_time, avg->max_time, avg->num_hits);
        GDF_COPY_TIMES_VECTOR_DATA(url_hits_gp_idx, g_idx, gv_idx, 0, 
                                 avg->avg_time, avg->min_time, avg->max_time, avg->num_hits,
                                 url_hits_local_gp_ptr->succ_response.avg_time,
                                 url_hits_local_gp_ptr->succ_response.min_time,
                                 url_hits_local_gp_ptr->succ_response.max_time,
                                 url_hits_local_gp_ptr->succ_response.succ); g_idx++;
      }
      else
      {
        GDF_COPY_TIMES_VECTOR_DATA(url_hits_gp_idx, g_idx, gv_idx, 0, 
                                 0, -1, 0, 0,
                                 url_hits_local_gp_ptr->succ_response.avg_time,
                                 url_hits_local_gp_ptr->succ_response.min_time,
                                 url_hits_local_gp_ptr->succ_response.max_time,
                                 url_hits_local_gp_ptr->succ_response.succ); g_idx++;
      }

      if(avg->num_tries - avg->num_hits)
      {
        NSDL2_GDF(NULL, NULL, " HTTP Failure Response Time after ms avg->url_failure_avg_time = %lu avg->url_failure_min_time = %lu avg->url_failure_max_time = %lu avg->avg->num_tries - avg->num_hits = %lu", avg->url_failure_avg_time, avg->url_failure_min_time, avg->url_failure_max_time, avg->num_tries - avg->num_hits);
        GDF_COPY_TIMES_VECTOR_DATA(url_hits_gp_idx, g_idx, gv_idx, 0, 
                                 avg->url_failure_avg_time, avg->url_failure_min_time, 
                                 avg->url_failure_max_time, avg->num_tries - avg->num_hits,
                                 url_hits_local_gp_ptr->fail_response.avg_time,
                                 url_hits_local_gp_ptr->fail_response.min_time,
                                 url_hits_local_gp_ptr->fail_response.max_time,
                                 url_hits_local_gp_ptr->fail_response.succ); g_idx++;
      }
      else
      {
        GDF_COPY_TIMES_VECTOR_DATA(url_hits_gp_idx, g_idx, gv_idx, 0, 
                                 0, -1, 0, 0,
                                 url_hits_local_gp_ptr->fail_response.avg_time,
                                 url_hits_local_gp_ptr->fail_response.min_time,
                                 url_hits_local_gp_ptr->fail_response.max_time,
                                 url_hits_local_gp_ptr->fail_response.succ); g_idx++;
      }

      //DNS
      if(avg->url_dns_count)
      {
        NSDL2_GDF(NULL, NULL, " HTTP DNS time after ms avg->url_dns_tot_time/avg->url_dns_count = %lu avg->url_dns_min_time = %lu avg->url_dns_max_time = %lu avg->url_dns_count = %lu", avg->url_dns_tot_time/avg->url_dns_count, avg->url_dns_min_time, avg->url_dns_max_time, avg->url_dns_count); 
        GDF_COPY_TIMES_VECTOR_DATA(url_hits_gp_idx, g_idx, gv_idx, 0, 
                                 avg->url_dns_tot_time/avg->url_dns_count, avg->url_dns_min_time, 
                                 avg->url_dns_max_time, avg->url_dns_count,
                                 url_hits_local_gp_ptr->dns.avg_time,
                                 url_hits_local_gp_ptr->dns.min_time,
                                 url_hits_local_gp_ptr->dns.max_time,
                                 url_hits_local_gp_ptr->dns.succ); g_idx++;
      }
      else
      {
        GDF_COPY_TIMES_VECTOR_DATA(url_hits_gp_idx, g_idx, gv_idx, 0, 
                                 0, -1, 0, 0,
                                 url_hits_local_gp_ptr->dns.avg_time,
                                 url_hits_local_gp_ptr->dns.min_time,
                                 url_hits_local_gp_ptr->dns.max_time,
                                 url_hits_local_gp_ptr->dns.succ); g_idx++;
      }

      //Connect
      if(avg->url_conn_count)
      {
        NSDL2_GDF(NULL, NULL, " HTTP Connect time after ms avg->url_conn_tot_time/avg->url_conn_count = %lu avg->url_conn_min_time = %lu, avg->url_conn_max_time = %lu avg->url_conn_count = %lu", avg->url_conn_tot_time/avg->url_conn_count, avg->url_conn_min_time, avg->url_conn_max_time, avg->url_conn_count);
        GDF_COPY_TIMES_VECTOR_DATA(url_hits_gp_idx, g_idx, gv_idx, 0, 
                                 avg->url_conn_tot_time/avg->url_conn_count, avg->url_conn_min_time, 
                                 avg->url_conn_max_time, avg->url_conn_count,
                                 url_hits_local_gp_ptr->conn.avg_time,
                                 url_hits_local_gp_ptr->conn.min_time,
                                 url_hits_local_gp_ptr->conn.max_time,
                                 url_hits_local_gp_ptr->conn.succ); g_idx++;
      }
      else
      {
        GDF_COPY_TIMES_VECTOR_DATA(url_hits_gp_idx, g_idx, gv_idx, 0, 
                                 0, -1, 0, 0,
                                 url_hits_local_gp_ptr->conn.avg_time,
                                 url_hits_local_gp_ptr->conn.min_time,
                                 url_hits_local_gp_ptr->conn.max_time,
                                 url_hits_local_gp_ptr->conn.succ); g_idx++;
      }

      //SSL
      if(avg->url_ssl_count)
      {
        NSDL2_GDF(NULL, NULL, " HTTP SSL time after ms avg->url_ssl_tot_time/avg->url_ssl_count = %lu avg->url_ssl_min_time = %lu avg->url_ssl_max_time = %lu avg->url_ssl_count = %lu", avg->url_ssl_tot_time/avg->url_ssl_count, avg->url_ssl_min_time, avg->url_ssl_max_time, avg->url_ssl_count);
        GDF_COPY_TIMES_VECTOR_DATA(url_hits_gp_idx, g_idx, gv_idx, 0, 
                                 avg->url_ssl_tot_time/avg->url_ssl_count, avg->url_ssl_min_time, 
                                 avg->url_ssl_max_time, avg->url_ssl_count,
                                 url_hits_local_gp_ptr->ssl.avg_time,
                                 url_hits_local_gp_ptr->ssl.min_time,
                                 url_hits_local_gp_ptr->ssl.max_time,
                                 url_hits_local_gp_ptr->ssl.succ); g_idx++;
      }
      else
      {
        GDF_COPY_TIMES_VECTOR_DATA(url_hits_gp_idx, g_idx, gv_idx, 0, 
                                 0, -1, 0, 0,
                                 url_hits_local_gp_ptr->ssl.avg_time,
                                 url_hits_local_gp_ptr->ssl.min_time,
                                 url_hits_local_gp_ptr->ssl.max_time,
                                 url_hits_local_gp_ptr->ssl.succ); g_idx++;
      }

      //First-byte-rcvd
      if(avg->url_frst_byte_rcv_count)
      {
        NSDL2_GDF(NULL, NULL, " HTTP First Byte after ms avg->url_frst_byte_rcv_tot_time/avg->url_frst_byte_rcv_count = %lu avg->url_frst_byte_rcv_min_time = %lu avg->url_frst_byte_rcv_max_time = %lu avg->url_frst_byte_rcv_count = %lu", avg->url_frst_byte_rcv_tot_time/avg->url_frst_byte_rcv_count, avg->url_frst_byte_rcv_min_time, avg->url_frst_byte_rcv_max_time, avg->url_frst_byte_rcv_count);
        GDF_COPY_TIMES_VECTOR_DATA(url_hits_gp_idx, g_idx, gv_idx, 0, 
                                 avg->url_frst_byte_rcv_tot_time/avg->url_frst_byte_rcv_count, avg->url_frst_byte_rcv_min_time, 
                                 avg->url_frst_byte_rcv_max_time, avg->url_frst_byte_rcv_count,
                                 url_hits_local_gp_ptr->frst_byte_rcv.avg_time,
                                 url_hits_local_gp_ptr->frst_byte_rcv.min_time,
                                 url_hits_local_gp_ptr->frst_byte_rcv.max_time,
                                 url_hits_local_gp_ptr->frst_byte_rcv.succ); g_idx++;
      }
      else
      {
        GDF_COPY_TIMES_VECTOR_DATA(url_hits_gp_idx, g_idx, gv_idx, 0, 
                                 0, -1, 0, 0,
                                 url_hits_local_gp_ptr->frst_byte_rcv.avg_time,
                                 url_hits_local_gp_ptr->frst_byte_rcv.min_time,
                                 url_hits_local_gp_ptr->frst_byte_rcv.max_time,
                                 url_hits_local_gp_ptr->frst_byte_rcv.succ); g_idx++;
      }

      //Download
      if(avg->url_dwnld_count)
      {
        NSDL2_GDF(NULL, NULL, " HTTP Download Time after ms avg->url_dwnld_tot_time/avg->url_dwnld_count = %lu avg->url_dwnld_min_time = %lu avg->url_dwnld_max_time = %lu avg->url_dwnld_count = %lu", avg->url_dwnld_tot_time/avg->url_dwnld_count, avg->url_dwnld_min_time, avg->url_dwnld_max_time, avg->url_dwnld_count);
        GDF_COPY_TIMES_VECTOR_DATA(url_hits_gp_idx, g_idx, gv_idx, 0, 
                                 avg->url_dwnld_tot_time/avg->url_dwnld_count, avg->url_dwnld_min_time, 
                                 avg->url_dwnld_max_time, avg->url_dwnld_count,
                                 url_hits_local_gp_ptr->dwnld.avg_time,
                                 url_hits_local_gp_ptr->dwnld.min_time,
                                 url_hits_local_gp_ptr->dwnld.max_time,
                                 url_hits_local_gp_ptr->dwnld.succ); g_idx++;
      }
      else
      {
        GDF_COPY_TIMES_VECTOR_DATA(url_hits_gp_idx, g_idx, gv_idx, 0, 
                                 0, -1, 0, 0,
                                 url_hits_local_gp_ptr->dwnld.avg_time,
                                 url_hits_local_gp_ptr->dwnld.min_time,
                                 url_hits_local_gp_ptr->dwnld.max_time,
                                 url_hits_local_gp_ptr->dwnld.succ); g_idx++;
      }
     
      // This need to be fixed as these two are 'long long'
      GDF_COPY_VECTOR_DATA(url_hits_gp_idx, g_idx, gv_idx, 0,
                           cavg->url_fetches_completed, url_hits_local_gp_ptr->cum_tries); g_idx++;
     
      GDF_COPY_VECTOR_DATA(url_hits_gp_idx, g_idx, gv_idx, 0,
                           cavg->url_succ_fetches, url_hits_local_gp_ptr->cum_succ); g_idx++;

      /*Here to calculate URL failure corresponding to URL completed and URL Success.
        Formula is :- (((URL Completed - URL Success) * 100)/URL Completed) */
      /*GDF_COPY_VECTOR_DATA(url_hits_gp_idx, g_idx, gv_idx, 0,
                           (avg->num_tries == 0)?0:((double)((avg->num_tries - avg->num_hits) * 100)/avg->num_tries), 
                           url_hits_local_gp_ptr->failure);*/
      /*bug 103688 START ******************************************************************/
      NS_COPY_SAMPLE_2B_100_COUNT_4B_DATA((avg->num_tries == 0)?0:((double)((avg->num_tries - avg->num_hits) * 100)/avg->num_tries), avg->num_tries, url_hits_gp_idx, g_idx, gv_idx, 0, url_hits_local_gp_ptr->failure.sample, url_hits_local_gp_ptr->failure.count)
      g_idx++;
      /******* bug 103688 END  *********************************************************/
      NSDL3_GDF(NULL, NULL, "num_hits = %llu, num_tries = %llu, failure = %d, %d", 
                             avg->num_hits, avg->num_tries, url_hits_local_gp_ptr->failure.sample, url_hits_local_gp_ptr->failure.count); 

      result = (avg->total_bytes)*8/((double)global_settings->progress_secs/1000);

      /*bug id 101742: using method convert_kbps() */
      NSDL2_GDF(NULL, NULL, "HTTP Body receive throughput result = %f ", result);
      NSDL2_GDF(NULL, NULL, "HTTP Body receive throughput after kbps result = %f ", convert_kbps(result));

      GDF_COPY_VECTOR_DATA(url_hits_gp_idx, g_idx, gv_idx, 0,
                           convert_kbps(result), url_hits_local_gp_ptr->http_body_throughput); g_idx++;
   
      GDF_COPY_VECTOR_DATA(url_hits_gp_idx, g_idx, gv_idx, 0,
                           cavg->c_tot_total_bytes, url_hits_local_gp_ptr->tot_http_body); g_idx++;

      g_idx = 0;
      url_hits_local_gp_ptr++;
    }
  }
}

/*bug 70480 fill_http2_srv_push_gp added*/
// Function for filliling the data in the structure of http2_srv_push_gp
static inline void fill_http2_srv_push_gp (cavgtime **g_cavg)
{
  if(http2_srv_push_gp_ptr == NULL)
     return;
  NSDL2_GDF(NULL, NULL, "Method called");
  cavgtime *cavg;
  cavgtime *tmp_cavg;
  Http2SrvPush_gp* http2_srv_push_local_gp_ptr = http2_srv_push_gp_ptr;
  int g_idx = 0;
  for(int gv_idx = 0; gv_idx < sgrp_used_genrator_entries + 1; gv_idx++)
  {
    tmp_cavg = (cavgtime*)g_cavg[gv_idx];
    for(int grp_idx = 0; grp_idx < TOTAL_GRP_ENTERIES_WITH_GRP_KW; grp_idx++)
    {
      cavg = (cavgtime*)((char*)tmp_cavg + (grp_idx * g_cavg_size_only_grp));
      NSDL2_GDF(NULL,NULL, "cavg->cum_srv_pushed_resources=%d", cavg->cum_srv_pushed_resources);
      GDF_COPY_VECTOR_DATA(http2_srv_push_gp_idx, g_idx, gv_idx, 0,
                           cavg->cum_srv_pushed_resources, http2_srv_push_local_gp_ptr->server_push);
      g_idx = 0;
      http2_srv_push_local_gp_ptr++;
    }
  }
}



static inline void fill_smtp_hits_gp (avgtime **g_avg, cavgtime **g_cavg)
{
  //Long_data succ;
  int g_idx = 0, gv_idx, grp_idx;
  avgtime *avg = NULL;
  cavgtime *cavg = NULL;
  avgtime *tmp_avg = NULL;
  cavgtime *tmp_cavg = NULL;

  if(smtp_hits_gp_ptr == NULL) return;
  SMTP_hits_gp *smtp_hits_local_gp_ptr = smtp_hits_gp_ptr;

  NSDL2_GDF(NULL, NULL, "Method called");

  for(gv_idx = 0; gv_idx < sgrp_used_genrator_entries + 1; gv_idx++)
  {
    tmp_avg = (avgtime *)g_avg[gv_idx];
    tmp_cavg = (cavgtime *)g_cavg[gv_idx];
    for(grp_idx = 0; grp_idx < TOTAL_GRP_ENTERIES_WITH_GRP_KW; grp_idx++)
    {
      /* bug id 101742 using method convert_ps()*/
      avg = (avgtime*)((char*)tmp_avg + (grp_idx * g_avg_size_only_grp));
      cavg = (cavgtime*)((char*)tmp_cavg + (grp_idx * g_cavg_size_only_grp));
      GDF_COPY_VECTOR_DATA(smtp_hits_gp_idx, g_idx, gv_idx, 0,
                           convert_long_ps(avg->smtp_fetches_started), smtp_hits_local_gp_ptr->url_req); g_idx++;
      NSDL2_GDF(NULL, NULL, " SMTP Request Sent/Sec avg->smtp_fetches_started = %lu ", avg->smtp_fetches_started);
      NSDL2_GDF(NULL, NULL, " SMTP Request Sent/Sec after ps avg->smtp_fetches_started = %lu ", convert_long_ps(avg->smtp_fetches_started));

      GDF_COPY_VECTOR_DATA(smtp_hits_gp_idx, g_idx, gv_idx, 0,
                           convert_long_ps(avg->smtp_num_tries), smtp_hits_local_gp_ptr->tries); g_idx++;
      NSDL2_GDF(NULL, NULL, "SMTP Hits/Sec  avg->smtp_num_tries avg->smtp_num_tries = %lu ", avg->smtp_num_tries);
      NSDL2_GDF(NULL, NULL, "SMTP Hits/Sec after ps avg->smtp_num_tries = %lu ", convert_long_ps(avg->smtp_num_tries));
     
      GDF_COPY_VECTOR_DATA(smtp_hits_gp_idx, g_idx, gv_idx, 0,
                           convert_long_ps(avg->smtp_num_hits), smtp_hits_local_gp_ptr->succ); g_idx++;
      NSDL2_GDF(NULL, NULL, "Success SMTP Responses/Sec  avg->smtp_num_tries avg->smtp_num_hits = %lu ", avg->smtp_num_hits);
      NSDL2_GDF(NULL, NULL, "Success SMTP Responses/Sec  after ps avg->smtp_num_hits = %lu ", convert_long_ps(avg->smtp_num_hits));
     
      // Here the "response" variable of `Times_data` data type is getting filled
      
      NSDL2_GDF(NULL, NULL, "Average SMTP Response Time  after ms avg->smtp_avg_time  %lu,", avg->smtp_avg_time);  
      NSDL2_GDF(NULL, NULL, "Average SMTP Response Time  after ms avg->smtp_min_time  %lu,", avg->smtp_min_time);
      NSDL2_GDF(NULL, NULL, "Average SMTP Response Time  after ms avg->smtp_max_time  %lu,", avg->smtp_max_time);
      NSDL2_GDF(NULL, NULL, "Average SMTP Response Time  after ms avg->smtp_num_hits  %lu,", avg->smtp_num_hits); 
      GDF_COPY_TIMES_VECTOR_DATA(smtp_hits_gp_idx, g_idx, gv_idx, 0,
                                 avg->smtp_avg_time, avg->smtp_min_time, avg->smtp_max_time, avg->smtp_num_hits,
                                 smtp_hits_local_gp_ptr->response.avg_time,
                                 smtp_hits_local_gp_ptr->response.min_time,
                                 smtp_hits_local_gp_ptr->response.max_time,
                                 smtp_hits_local_gp_ptr->response.succ); g_idx++;
     
      // This need to be fixed as these two are 'long long'
      GDF_COPY_VECTOR_DATA(smtp_hits_gp_idx, g_idx, gv_idx, 0,
                           cavg->smtp_fetches_completed, smtp_hits_local_gp_ptr->cum_tries); g_idx++;
     
      GDF_COPY_VECTOR_DATA(smtp_hits_gp_idx, g_idx, gv_idx, 0,
                           cavg->smtp_succ_fetches, smtp_hits_local_gp_ptr->cum_succ); g_idx++;
      g_idx = 0;
      smtp_hits_local_gp_ptr++;
    }
  }
}

static inline void fill_pop3_hits_gp (avgtime **g_avg, cavgtime **g_cavg)
{
  //Long_data succ;
  int g_idx = 0, gv_idx, grp_idx;
  avgtime *avg = NULL;
  cavgtime *cavg = NULL;
  avgtime *tmp_avg = NULL;
  cavgtime *tmp_cavg = NULL;

  if(pop3_hits_gp_ptr == NULL) return;
  POP3_hits_gp *pop3_hits_local_gp_ptr = pop3_hits_gp_ptr;

  NSDL2_GDF(NULL, NULL, "Method called");

  for(gv_idx = 0; gv_idx < sgrp_used_genrator_entries + 1; gv_idx++)
  {
    tmp_avg = (avgtime *)g_avg[gv_idx];
    tmp_cavg = (cavgtime *)g_cavg[gv_idx];
    for(grp_idx = 0; grp_idx < TOTAL_GRP_ENTERIES_WITH_GRP_KW; grp_idx++)
    {
      /* bug id 101742 using method convert_ps()*/
      avg = (avgtime*)((char*)tmp_avg + (grp_idx * g_avg_size_only_grp));
      cavg = (cavgtime*)((char*)tmp_cavg + (grp_idx * g_cavg_size_only_grp));

      GDF_COPY_VECTOR_DATA(pop3_hits_gp_idx, g_idx, gv_idx, 0,
                           convert_long_ps(avg->pop3_fetches_started), pop3_hits_local_gp_ptr->url_req); g_idx++;
      NSDL2_GDF(NULL, NULL, "POP3 Request Sent/Sec  avg->pop3_fetches_started %lu,", avg->pop3_fetches_started);
      NSDL2_GDF(NULL, NULL, "POP3 Request Sent/Sec after ps avg->pop3_fetches_started  %lu,", convert_long_ps(avg->pop3_fetches_started));
     
      GDF_COPY_VECTOR_DATA(pop3_hits_gp_idx, g_idx, gv_idx, 0,
                           convert_long_ps(avg->pop3_num_tries), pop3_hits_local_gp_ptr->tries); g_idx++;
      NSDL2_GDF(NULL, NULL, "POP3 Hits/Sec avg->pop3_num_tries %lu,", avg->pop3_num_tries);
      NSDL2_GDF(NULL, NULL, "POP3 Hits/Sec  after ps avg->pop3_num_tries  %lu,", convert_long_ps(avg->pop3_num_tries));
     
      GDF_COPY_VECTOR_DATA(pop3_hits_gp_idx, g_idx, gv_idx, 0,
                           convert_long_ps(avg->pop3_num_hits), pop3_hits_local_gp_ptr->succ); g_idx++;
      NSDL2_GDF(NULL, NULL, "Success POP3 Responses/Sec avg->pop3_num_hits %lu,", avg->pop3_num_hits);
      NSDL2_GDF(NULL, NULL, "Success POP3 Responses/Sec  after ps avg->pop3_num_hits  %lu,", convert_long_ps(avg->pop3_num_hits));
     
      // Here the "response" variable of `Times_data` data type is getting filled
     
      //Overall
      NSDL2_GDF(NULL, NULL, "POP3 Response Time after ms avg->pop3_overall_avg_time  %lu,", avg->pop3_overall_avg_time);
      NSDL2_GDF(NULL, NULL, "POP3 Response Time after ms avg->pop3_overall_min_time  %lu,", avg->pop3_overall_min_time);
      NSDL2_GDF(NULL, NULL, "POP3 Response Time after ms avg->pop3_overall_max_time  %lu,", avg->pop3_overall_max_time);
      NSDL2_GDF(NULL, NULL, "POP3 Response Time after ms avg->pop3_num_tries %lu,", avg->pop3_num_tries);
      GDF_COPY_TIMES_VECTOR_DATA(pop3_hits_gp_idx, g_idx, gv_idx, 0,
                                 avg->pop3_overall_avg_time, avg->pop3_overall_min_time, 
                                 avg->pop3_overall_max_time, avg->pop3_num_tries,
                                 pop3_hits_local_gp_ptr->response.avg_time,
                                 pop3_hits_local_gp_ptr->response.min_time,
                                 pop3_hits_local_gp_ptr->response.max_time,
                                 pop3_hits_local_gp_ptr->response.succ); g_idx++;

      //Success
      if(avg->pop3_num_hits)
      {
        NSDL2_GDF(NULL, NULL, "POP3 Successful Response Time  after ms avg->pop3_avg_time  %lu,", avg->pop3_avg_time);
        NSDL2_GDF(NULL, NULL, "POP3 Successful Response Time  after ms avg->pop3_min_time  %lu,", avg->pop3_min_time);
        NSDL2_GDF(NULL, NULL, "POP3 Successful Response Time  after ms avg->pop3_max_time  %lu,", avg->pop3_max_time);
        NSDL2_GDF(NULL, NULL, "POP3 Successful Response Time  after ms avg->pop3_num_hits  %lu,", avg->pop3_num_hits);
        GDF_COPY_TIMES_VECTOR_DATA(pop3_hits_gp_idx, g_idx, gv_idx, 0,
                                   avg->pop3_avg_time, avg->pop3_min_time, avg->pop3_max_time, avg->pop3_num_hits,
                                   pop3_hits_local_gp_ptr->succ_response.avg_time,
                                   pop3_hits_local_gp_ptr->succ_response.min_time,
                                   pop3_hits_local_gp_ptr->succ_response.max_time,
                                   pop3_hits_local_gp_ptr->succ_response.succ); g_idx++;
      }
      else
      {
        GDF_COPY_TIMES_VECTOR_DATA(pop3_hits_gp_idx, g_idx, gv_idx, 0,
                                   0, -1, 0 , 0,
                                   pop3_hits_local_gp_ptr->succ_response.avg_time,
                                   pop3_hits_local_gp_ptr->succ_response.min_time,
                                   pop3_hits_local_gp_ptr->succ_response.max_time,
                                   pop3_hits_local_gp_ptr->succ_response.succ); g_idx++;
      }

      //Failure
      if(avg->pop3_num_tries - avg->pop3_num_hits)
      {

        NSDL2_GDF(NULL, NULL, "POP3 Failure Response Time after ms avg->pop3_failure_avg_time  %lu,", avg->pop3_failure_avg_time);
        NSDL2_GDF(NULL, NULL, "POP3 Failure Response Time after ms avg->pop3_failure_min_time  %lu,", avg->pop3_failure_min_time);
        NSDL2_GDF(NULL, NULL, "POP3 Failure Response Time after ms avg->pop3_failure_max_time  %lu,", avg->pop3_failure_max_time);
        NSDL2_GDF(NULL, NULL, "POP3 Failure Response Time after ms avg->pop3_num_tries - avg->pop3_num_hits %lu,", avg->pop3_num_tries - avg->pop3_num_hits);
        GDF_COPY_TIMES_VECTOR_DATA(pop3_hits_gp_idx, g_idx, gv_idx, 0,
                                   avg->pop3_failure_avg_time, avg->pop3_failure_min_time, avg->pop3_failure_max_time, 
                                   avg->pop3_num_tries - avg->pop3_num_hits,
                                   pop3_hits_local_gp_ptr->fail_response.avg_time,
                                   pop3_hits_local_gp_ptr->fail_response.min_time,
                                   pop3_hits_local_gp_ptr->fail_response.max_time,
                                   pop3_hits_local_gp_ptr->fail_response.succ); g_idx++;
      }
      else
      {
        GDF_COPY_TIMES_VECTOR_DATA(pop3_hits_gp_idx, g_idx, gv_idx, 0,
                                   0, -1, 0 , 0,
                                   pop3_hits_local_gp_ptr->fail_response.avg_time,
                                   pop3_hits_local_gp_ptr->fail_response.min_time,
                                   pop3_hits_local_gp_ptr->fail_response.max_time,
                                   pop3_hits_local_gp_ptr->fail_response.succ); g_idx++;

      }
     
      // This need to be fixed as these two are 'long long'
      GDF_COPY_VECTOR_DATA(pop3_hits_gp_idx, g_idx, gv_idx, 0,
                           cavg->pop3_fetches_completed, pop3_hits_local_gp_ptr->cum_tries); g_idx++;
     
      GDF_COPY_VECTOR_DATA(pop3_hits_gp_idx, g_idx, gv_idx, 0,
                           cavg->pop3_succ_fetches, pop3_hits_local_gp_ptr->cum_succ); g_idx++;
      g_idx = 0;
      pop3_hits_local_gp_ptr++;
    }
  }
}


static inline void fill_dns_hits_gp (avgtime **g_avg, cavgtime **g_cavg)
{
  //Long_data succ;
  int g_idx = 0, gv_idx, grp_idx;
  avgtime *avg = NULL;
  cavgtime *cavg = NULL;
  avgtime *tmp_avg = NULL;
  cavgtime *tmp_cavg = NULL;

  if(dns_hits_gp_ptr == NULL) return;
  DNS_hits_gp *dns_hits_local_gp_ptr = dns_hits_gp_ptr;

  NSDL2_GDF(NULL, NULL, "Method called");

  for(gv_idx = 0; gv_idx < sgrp_used_genrator_entries + 1; gv_idx++)
  {
    tmp_avg = (avgtime *)g_avg[gv_idx]; 
    tmp_cavg = (cavgtime *)g_cavg[gv_idx]; 
    for(grp_idx = 0; grp_idx < TOTAL_GRP_ENTERIES_WITH_GRP_KW; grp_idx++)
    {
      avg = (avgtime*)((char*)tmp_avg + (grp_idx * g_avg_size_only_grp));
      cavg = (cavgtime*)((char*)tmp_cavg + (grp_idx * g_cavg_size_only_grp));

      GDF_COPY_VECTOR_DATA(dns_hits_gp_idx, g_idx, gv_idx, 0,
                           avg->dns_fetches_started, dns_hits_local_gp_ptr->url_req); g_idx++;
     
      GDF_COPY_VECTOR_DATA(dns_hits_gp_idx, g_idx, gv_idx, 0,
                           avg->dns_num_tries, dns_hits_local_gp_ptr->tries); g_idx++;
     
      GDF_COPY_VECTOR_DATA(dns_hits_gp_idx, g_idx, gv_idx, 0,
                           avg->dns_num_hits, dns_hits_local_gp_ptr->succ); g_idx++;
     
      // Here the "response" variable of `Times_data` data type is getting filled
     
      //Overall
      GDF_COPY_TIMES_VECTOR_DATA(dns_hits_gp_idx, g_idx, gv_idx, 0,
                                 avg->dns_overall_avg_time, avg->dns_overall_min_time, avg->dns_overall_max_time, avg->dns_num_tries,
                                 dns_hits_local_gp_ptr->response.avg_time,
                                 dns_hits_local_gp_ptr->response.min_time,
                                 dns_hits_local_gp_ptr->response.max_time,
                                 dns_hits_local_gp_ptr->response.succ); g_idx++;
       
      //Success
      if(avg->dns_num_hits)
      {
        GDF_COPY_TIMES_VECTOR_DATA(dns_hits_gp_idx, g_idx, gv_idx, 0,
                                   avg->dns_avg_time, avg->dns_min_time, avg->dns_max_time, avg->dns_num_hits,
                                   dns_hits_local_gp_ptr->succ_response.avg_time,
                                   dns_hits_local_gp_ptr->succ_response.min_time,
                                   dns_hits_local_gp_ptr->succ_response.max_time,
                                   dns_hits_local_gp_ptr->succ_response.succ); g_idx++;
      }
      else
      {
        GDF_COPY_TIMES_VECTOR_DATA(dns_hits_gp_idx, g_idx, gv_idx, 0,
                                   0, -1, 0, 0,
                                   dns_hits_local_gp_ptr->succ_response.avg_time,
                                   dns_hits_local_gp_ptr->succ_response.min_time,
                                   dns_hits_local_gp_ptr->succ_response.max_time,
                                   dns_hits_local_gp_ptr->succ_response.succ); g_idx++;
      }

      //Failure
      if(avg->dns_num_tries - avg->dns_num_hits)
      {
        GDF_COPY_TIMES_VECTOR_DATA(dns_hits_gp_idx, g_idx, gv_idx, 0,
                                   avg->dns_failure_avg_time, avg->dns_failure_min_time,
                                   avg->dns_failure_max_time, avg->dns_num_tries - avg->dns_num_hits,
                                   dns_hits_local_gp_ptr->fail_response.avg_time,
                                   dns_hits_local_gp_ptr->fail_response.min_time,
                                   dns_hits_local_gp_ptr->fail_response.max_time,
                                   dns_hits_local_gp_ptr->fail_response.succ); g_idx++;
      }
      else
      {
        GDF_COPY_TIMES_VECTOR_DATA(dns_hits_gp_idx, g_idx, gv_idx, 0,
                                   0, -1, 0, 0,
                                   dns_hits_local_gp_ptr->fail_response.avg_time,
                                   dns_hits_local_gp_ptr->fail_response.min_time,
                                   dns_hits_local_gp_ptr->fail_response.max_time,
                                   dns_hits_local_gp_ptr->fail_response.succ); g_idx++;
      }

     
      // This need to be fixed as these two are 'long long'
      GDF_COPY_VECTOR_DATA(dns_hits_gp_idx, g_idx, gv_idx, 0,
                           cavg->dns_fetches_completed, dns_hits_local_gp_ptr->cum_tries); g_idx++;
     
      GDF_COPY_VECTOR_DATA(dns_hits_gp_idx, g_idx, gv_idx, 0,
                           cavg->dns_succ_fetches, dns_hits_local_gp_ptr->cum_succ); g_idx++;
      g_idx = 0;
      dns_hits_local_gp_ptr++;
    }
  }
}


// Pending task - Convert two longs into one long long for logging
static inline void log_url_hits_gp ()
{
  if(url_hits_gp_ptr == NULL) return;
  NSDL2_GDF(NULL, NULL, "Method called");
  /* bug id 101742:Updating unit seconds to ms */
  fprintf(gui_fp, "\nURL Hits: URL Hits/Second=%6.3f, Success URL Responses/Second =%6.3f, Average URL Response Time (ms)=%6.3f, Minimum URL Response Time (ms)=%6.3f, Maximum URL Response Time (ms)=%6.3f, Success URL Response Time (ms)=%6.3f, Total URL Hits=%0.0f , Total URL Successful=%0.0f\n",
  convert_ps(url_hits_gp_ptr->tries),
  convert_ps(url_hits_gp_ptr->succ),
/*convert_sec*/(url_hits_gp_ptr->response.avg_time),  //Updating unit seconds to ms
/*convert_sec*/(url_hits_gp_ptr->response.min_time),  //Updating unit seconds to ms
/*convert_sec*/(url_hits_gp_ptr->response.max_time),  //Updating unit seconds to ms
/*convert_sec*/(url_hits_gp_ptr->response.succ),      //Updating unit seconds to ms
  (url_hits_gp_ptr->cum_tries),   // Assuming no function is to be applied
  (url_hits_gp_ptr->cum_succ));   // Assuming no function is to be applied
}
/*
static inline void fill_memory_debug_gp (avgtime *avg) {
  int i = 0;
  MemoryAvgTime *memory_debug_avgtime = NULL;
  NSDL2_GDF(NULL, NULL, "Method called");

  if(global_settings->g_enable_ns_diag) {
    memory_debug_avgtime = (MemoryAvgTime*)((char*) avg + g_ns_diag_avgtime_idx);
    if(ns_diag_gp_ptr == NULL) {
      fprintf(stderr, "ns_diag_gp_ptr is NULL\n");
      return;
    }
    else
      fprintf(stderr, "ns_diag_gp_ptr is not NULL\n");

    GDF_COPY_SCALAR_DATA(ns_diag_gp_idx, i, memory_debug_avgtime->p_mem_allocated /((double)(1024*1024)),
                         ns_diag_gp_ptr->p_mem_allocated); i++;
    GDF_COPY_SCALAR_DATA(ns_diag_gp_idx, i, memory_debug_avgtime->p_mem_cum_allocated /((double)(1024*1024)),
                         ns_diag_gp_ptr->p_mem_cum_allocated); i++;
    GDF_COPY_SCALAR_DATA(ns_diag_gp_idx, i, memory_debug_avgtime->p_mem_freed/((double)(1024*1024)),
                         ns_diag_gp_ptr->p_mem_freed); i++;
    GDF_COPY_SCALAR_DATA(ns_diag_gp_idx, i, memory_debug_avgtime->p_mem_cum_freed/((double)(1024*1024)),
                         ns_diag_gp_ptr->p_mem_cum_freed); i++;
    GDF_COPY_SCALAR_DATA(ns_diag_gp_idx, i, memory_debug_avgtime->p_mem_shared_allocated/((double)(1024*1024)),
                         ns_diag_gp_ptr->p_mem_shared_allocated); i++;
    GDF_COPY_SCALAR_DATA(ns_diag_gp_idx, i, memory_debug_avgtime->p_mem_num_malloced,
                         ns_diag_gp_ptr->p_mem_num_malloced); i++;
    GDF_COPY_SCALAR_DATA(ns_diag_gp_idx, i, memory_debug_avgtime->p_mem_num_freed,
                         ns_diag_gp_ptr->p_mem_num_freed); i++;
    GDF_COPY_SCALAR_DATA(ns_diag_gp_idx, i, memory_debug_avgtime->p_mem_num_shared_allocated,
                         ns_diag_gp_ptr->p_mem_num_shared_allocated); i++;
    //Child data
    GDF_COPY_SCALAR_DATA(ns_diag_gp_idx, i, memory_debug_avgtime->c_mem_allocated /((double)(1024*1024)),
                         ns_diag_gp_ptr->c_mem_allocated); i++;
    GDF_COPY_SCALAR_DATA(ns_diag_gp_idx, i, memory_debug_avgtime->c_mem_cum_allocated /((double)(1024*1024)),
                         ns_diag_gp_ptr->c_mem_cum_allocated); i++;
    fprintf(stderr, "*******************c_mem_freed = %llu\n", memory_debug_avgtime->c_mem_freed);
    GDF_COPY_SCALAR_DATA(ns_diag_gp_idx, i, (memory_debug_avgtime->c_mem_freed/(double)(1024*1024)),
                         ns_diag_gp_ptr->c_mem_freed); i++;
    GDF_COPY_SCALAR_DATA(ns_diag_gp_idx, i, (memory_debug_avgtime->c_mem_cum_freed),
                         ns_diag_gp_ptr->c_mem_cum_freed); i++;
    GDF_COPY_SCALAR_DATA(ns_diag_gp_idx, i, (memory_debug_avgtime->c_mem_shared_allocated/(double)(1024*1024)),
                         ns_diag_gp_ptr->c_mem_shared_allocated); i++;
    GDF_COPY_SCALAR_DATA(ns_diag_gp_idx, i, memory_debug_avgtime->c_mem_num_malloced,
                         ns_diag_gp_ptr->c_mem_num_malloced); i++;
    GDF_COPY_SCALAR_DATA(ns_diag_gp_idx, i, memory_debug_avgtime->c_mem_num_freed,
                         ns_diag_gp_ptr->c_mem_num_freed); i++;
    GDF_COPY_SCALAR_DATA(ns_diag_gp_idx, i, memory_debug_avgtime->c_mem_num_shared_allocated,
                         ns_diag_gp_ptr->c_mem_num_shared_allocated); i++;
  }
}
*/
static inline void fill_cache_gp (avgtime **g_avg) 
{
  int g_idx = 0, gv_idx, grp_idx;
  CacheAvgTime *cache_avg = NULL;
  avgtime *avg = NULL;
  Long_data lol_cache_add_url_time_avg;
  Long_data lol_cache_search_url_time_avg;

  if(http_caching_gp_ptr == NULL) 
    return;
  
  HttpCaching_gp *http_caching_local_gp_ptr = http_caching_gp_ptr;

  NSDL2_GDF(NULL, NULL, "Method called");

  for(gv_idx = 0; gv_idx < sgrp_used_genrator_entries + 1; gv_idx++)
  { 
    avg = (avgtime *)g_avg[gv_idx]; 
    for(grp_idx = 0; grp_idx < TOTAL_GRP_ENTERIES_WITH_GRP_KW; grp_idx++)
    {
      cache_avg = (CacheAvgTime*)((char*)((char *)avg + (grp_idx * g_avg_size_only_grp)) + g_cache_avgtime_idx);

      GDF_COPY_VECTOR_DATA(http_caching_gp_idx, g_idx, gv_idx, 0,
                           convert_long_long_data_to_ps_long_long((cache_avg->cache_num_tries)), 
                           http_caching_local_gp_ptr->cache_req); g_idx++;

      GDF_COPY_VECTOR_DATA(http_caching_gp_idx, g_idx, gv_idx, 0,
                           convert_long_long_data_to_ps_long_long((cache_avg->cache_num_hits)), 
                           http_caching_local_gp_ptr->cache_hits); g_idx++;

      GDF_COPY_VECTOR_DATA(http_caching_gp_idx, g_idx, gv_idx, 0,
                           (cache_avg->cache_num_hits * 100)/(cache_avg->cache_num_tries?cache_avg->cache_num_tries:1),
                           http_caching_local_gp_ptr->cache_hits_pct); g_idx++;

      GDF_COPY_VECTOR_DATA(http_caching_gp_idx, g_idx, gv_idx, 0,
                           convert_long_long_data_to_ps_long_long((cache_avg->cache_num_missed)), 
                           http_caching_local_gp_ptr->cache_misses); g_idx++;

      GDF_COPY_VECTOR_DATA(http_caching_gp_idx, g_idx, gv_idx, 0,
                           (cache_avg->cache_num_missed * 100)/(cache_avg->cache_num_tries?cache_avg->cache_num_tries:1),
                           http_caching_local_gp_ptr->cache_misses_pct); g_idx++;
  
      GDF_COPY_VECTOR_DATA(http_caching_gp_idx, g_idx, gv_idx, 0,
                           (cache_avg->cache_bytes_used / (double)(1024*1024)), 
                           http_caching_local_gp_ptr->cache_used_mem); g_idx++;

      GDF_COPY_VECTOR_DATA(http_caching_gp_idx, g_idx, gv_idx, 0,
                           convert_long_long_data_to_ps_long_long((cache_avg->cache_bytes_hit)), 
                           http_caching_local_gp_ptr->cache_bytes_hit); g_idx++;


      GDF_COPY_VECTOR_DATA(http_caching_gp_idx, g_idx, gv_idx, 0,
                           (cache_avg->cache_bytes_hit * 100)/((avg->total_bytes + cache_avg->cache_bytes_hit)?(avg->total_bytes + cache_avg->cache_bytes_hit): 1), 
                           http_caching_local_gp_ptr->cache_bytes_hit_pct); g_idx++;

      GDF_COPY_VECTOR_DATA(http_caching_gp_idx, g_idx, gv_idx, 0,
                           cache_avg->cache_num_entries, 
                           http_caching_local_gp_ptr->cache_entries); g_idx++;
 
      GDF_COPY_VECTOR_DATA(http_caching_gp_idx, g_idx, gv_idx, 0,
                           cache_avg->cache_num_entries_replaced, 
                           http_caching_local_gp_ptr->cache_entries_replaced); g_idx++;

      GDF_COPY_VECTOR_DATA(http_caching_gp_idx, g_idx, gv_idx, 0,
                           cache_avg->cache_num_entries_revalidation, 
                           http_caching_local_gp_ptr->cache_entries_revalidation); g_idx++;

      GDF_COPY_VECTOR_DATA(http_caching_gp_idx, g_idx, gv_idx, 0,
                           convert_long_long_data_to_ps_long_long((cache_avg->cache_num_entries_revalidation_ims)), 
                           http_caching_local_gp_ptr->cache_revalid_ims); g_idx++;

      GDF_COPY_VECTOR_DATA(http_caching_gp_idx, g_idx, gv_idx, 0,
                           convert_long_long_data_to_ps_long_long((cache_avg->cache_num_entries_revalidation_etag)), 
                           http_caching_local_gp_ptr->cache_revalid_etag); g_idx++;

      GDF_COPY_VECTOR_DATA(http_caching_gp_idx, g_idx, gv_idx, 0,
                           convert_long_long_data_to_ps_long_long((cache_avg->cache_revalidation_not_modified)), 
                           http_caching_local_gp_ptr->cache_revalid_not_modified); g_idx++;

      GDF_COPY_VECTOR_DATA(http_caching_gp_idx, g_idx, gv_idx, 0,
                           convert_long_long_data_to_ps_long_long((cache_avg->cache_revalidation_success)), 
                           http_caching_local_gp_ptr->cache_revalid_success); g_idx++;

      GDF_COPY_VECTOR_DATA(http_caching_gp_idx, g_idx, gv_idx, 0,
                           convert_long_long_data_to_ps_long_long((cache_avg->cache_revalidation_errors)), 
                           http_caching_local_gp_ptr->cache_revalid_errors); g_idx++;

      GDF_COPY_VECTOR_DATA(http_caching_gp_idx, g_idx, gv_idx, 0,
                           convert_long_long_data_to_ps_long_long((cache_avg->cache_num_entries_cacheable)), 
                           http_caching_local_gp_ptr->cache_response_cacheable); g_idx++;

      GDF_COPY_VECTOR_DATA(http_caching_gp_idx, g_idx, gv_idx, 0,
                           convert_long_long_data_to_ps_long_long((cache_avg->cache_num_entries_non_cacheable)),
                           http_caching_local_gp_ptr->cache_response_non_cacheable); g_idx++;

      GDF_COPY_VECTOR_DATA(http_caching_gp_idx, g_idx, gv_idx, 0,
                           0, http_caching_local_gp_ptr->cache_response_too_big); g_idx++;

      GDF_COPY_VECTOR_DATA(http_caching_gp_idx, g_idx, gv_idx, 0,
                           0, http_caching_local_gp_ptr->cache_response_too_small); g_idx++;

      GDF_COPY_VECTOR_DATA(http_caching_gp_idx, g_idx, gv_idx, 0,
                           cache_avg->cache_num_entries_collisions, 
                           http_caching_local_gp_ptr->cache_num_collisions); g_idx++;

      GDF_COPY_VECTOR_DATA(http_caching_gp_idx, g_idx, gv_idx, 0,
                           convert_long_long_data_to_ps_long_long((cache_avg->cache_num_error_entries_creations)), 
                           http_caching_local_gp_ptr->cache_error_entry_creations); g_idx++;
      /* Bug 101742, after ms */
      NSDL2_GDF(NULL, NULL, "cache_avg->cache_search_url_time_total = %d cache_avg->cache_num_tries = %d", cache_avg->cache_search_url_time_total, cache_avg->cache_num_tries);
      if(cache_avg->cache_search_url_time_total && cache_avg->cache_num_tries){
        NSDL2_GDF(NULL, NULL, "inside if");
        lol_cache_search_url_time_avg = (Long_data )cache_avg->cache_search_url_time_total/((Long_data )cache_avg->cache_num_tries );
      }
      else{
        NSDL2_GDF(NULL, NULL, "inside else");
        lol_cache_search_url_time_avg = 0;
          }
      NSDL2_GDF(NULL, NULL, " Average cache search Time after ms lol_cache_search_url_time_avg %f,", lol_cache_search_url_time_avg);
      NSDL2_GDF(NULL, NULL, " Average cache search Time after ms cache_avg->cache_search_url_time_min %f,", cache_avg->cache_search_url_time_min);
      NSDL2_GDF(NULL, NULL, " Average cache search Time after ms cache_avg->cache_search_url_time_max %f,", cache_avg->cache_search_url_time_max);
      NSDL2_GDF(NULL, NULL, " Average cache search Time after ms cache_avg->cache_num_tries %lu,", cache_avg->cache_num_tries);
      GDF_COPY_TIMES_VECTOR_DATA(http_caching_gp_idx, g_idx, gv_idx, 0,
                                 lol_cache_search_url_time_avg, cache_avg->cache_search_url_time_min, cache_avg->cache_search_url_time_max, cache_avg->cache_num_tries,
                                 http_caching_local_gp_ptr->cache_search_url_time.avg_time,
                                 http_caching_local_gp_ptr->cache_search_url_time.min_time,
                                 http_caching_local_gp_ptr->cache_search_url_time.max_time,
                                 http_caching_local_gp_ptr->cache_search_url_time.succ); g_idx++;
    
      NSDL2_GDF(NULL, NULL, "cache_avg->cache_add_url_time_total = %d cache_avg->cache_num_tries = %d", cache_avg->cache_add_url_time_total, cache_avg->cache_num_tries); 
      if(cache_avg->cache_add_url_time_total && cache_avg->cache_num_tries){
        NSDL2_GDF(NULL, NULL, "inside if");
        lol_cache_add_url_time_avg = (Long_data )cache_avg->cache_add_url_time_total/((Long_data )cache_avg->cache_num_tries );
        }
      else{
        NSDL2_GDF(NULL, NULL, "inside else");
        lol_cache_add_url_time_avg = 0;
        }
      NSDL2_GDF(NULL, NULL, " Average cache add Time after ms lol_cache_add_url_time_avg %lu,", lol_cache_add_url_time_avg);
      NSDL2_GDF(NULL, NULL, " Average cache add Time after ms cache_avg->cache_add_url_time_min %lu,", cache_avg->cache_add_url_time_min);
      NSDL2_GDF(NULL, NULL, " Average cache add Time after ms cache_avg->cache_add_url_time_max %lu,", cache_avg->cache_add_url_time_max);
      NSDL2_GDF(NULL, NULL, " Average cache add Time after ms cache_avg->cache_num_tries %lu,", cache_avg->cache_num_tries);
      GDF_COPY_TIMES_VECTOR_DATA(http_caching_gp_idx, g_idx, gv_idx, 0,
                                 lol_cache_add_url_time_avg, cache_avg->cache_add_url_time_min, cache_avg->cache_add_url_time_max, cache_avg->cache_num_tries,
                                 http_caching_local_gp_ptr->cache_add_url_time.avg_time,
                                 http_caching_local_gp_ptr->cache_add_url_time.min_time,
                                 http_caching_local_gp_ptr->cache_add_url_time.max_time,
                                 http_caching_local_gp_ptr->cache_add_url_time.succ); g_idx++;
       
      g_idx = 0;
      http_caching_local_gp_ptr++;
    }
  }
}
// Function for filliling the data in the structure of Page_download_gp
static inline void fill_page_download_gp (avgtime **g_avg, cavgtime **g_cavg)
{
  int g_idx = 0, gv_idx, grp_idx;
  u_ns_4B_t succ;
  Long_data lol_page_js_proc_time_avg;
  Long_data lol_page_proc_time_avg;
  avgtime *avg = NULL;
  cavgtime *cavg = NULL;
  avgtime *tmp_avg = NULL;
  cavgtime *tmp_cavg = NULL;

  if(page_download_gp_ptr == NULL) return;

  Page_download_gp *page_download_local_gp_ptr = page_download_gp_ptr;

  NSDL2_GDF(NULL, NULL, "Method called");

  for(gv_idx = 0; gv_idx < sgrp_used_genrator_entries + 1; gv_idx++)
  {
    tmp_avg = (avgtime *)g_avg[gv_idx];
    tmp_cavg = (cavgtime *)g_cavg[gv_idx];
    for(grp_idx = 0; grp_idx < TOTAL_GRP_ENTERIES_WITH_GRP_KW; grp_idx++)
    {
      /*bug id 101742: using method convert_long_ps() */
      avg = (avgtime*)((char*)tmp_avg + (grp_idx * g_avg_size_only_grp));
      cavg = (cavgtime*)((char*)tmp_cavg + (grp_idx * g_cavg_size_only_grp));

      NSDL2_GDF(NULL, NULL, "Page Download Started/Second avg->pg_fetches_started = %lu ", avg->pg_fetches_started);
      NSDL2_GDF(NULL, NULL, " Page Download Started/Second after pm avg->pg_fetches_started = %lu ", convert_long_ps(avg->pg_fetches_started));

      GDF_COPY_VECTOR_DATA(page_download_gp_idx, g_idx, gv_idx, 0,
                           convert_long_ps(avg->pg_fetches_started), page_download_local_gp_ptr->pg_dl_started); g_idx++;

      NSDL2_GDF(NULL, NULL, "Page Download completed/Second avg->pg_tries = %lu ", avg->pg_tries);
      NSDL2_GDF(NULL, NULL, "Page Download completed/Second after pm avg->pg_tries = %lu ", convert_long_ps(avg->pg_tries));
     
      GDF_COPY_VECTOR_DATA(page_download_gp_idx, g_idx, gv_idx, 0,
                           convert_long_ps(avg->pg_tries), page_download_local_gp_ptr->tries); g_idx++;

      NSDL2_GDF(NULL, NULL, "Page Success  Responses/Second avg->pg_hits = %lu ", avg->pg_hits);
      NSDL2_GDF(NULL, NULL, "Page Success  Responses/Second after pm avg->pg_hits = %lu ", convert_long_ps(avg->pg_hits));
     
      GDF_COPY_VECTOR_DATA(page_download_gp_idx, g_idx, gv_idx, 0,
                           convert_long_ps(avg->pg_hits), page_download_local_gp_ptr->succ); g_idx++;
      
      // Here the "response" variable of `Times_data` data type is getting filled
      succ = avg->pg_tries;
      //succ = global_settings->exclude_failed_agg ? avg->pg_hits : avg->pg_tries;
      NSDL2_GDF(NULL, NULL, " Page Response Time after ms avg->pg_avg_time = %d avg->pg_min_time = %d avg->pg_max_time = %d succ = %d ", avg->pg_avg_time, avg->pg_min_time, avg->pg_max_time, succ);      
      GDF_COPY_TIMES_VECTOR_DATA(page_download_gp_idx, g_idx, gv_idx, 0,
                                 avg->pg_avg_time, avg->pg_min_time, avg->pg_max_time, succ,
                                 page_download_local_gp_ptr->response.avg_time,
                                 page_download_local_gp_ptr->response.min_time,
                                 page_download_local_gp_ptr->response.max_time,
                                 page_download_local_gp_ptr->response.succ); g_idx++;

      succ = avg->pg_hits; 
      if(succ)
      {
        NSDL2_GDF(NULL, NULL, " Page Success Response Time after ms avg->pg_succ_avg_resp_time = %d avg->pg_succ_min_resp_time = %d avg->pg_succ_max_resp_time = %d succ = %d ", avg->pg_succ_avg_resp_time, avg->pg_succ_min_resp_time, avg->pg_succ_max_resp_time, succ);      
        GDF_COPY_TIMES_VECTOR_DATA(page_download_gp_idx, g_idx, gv_idx, 0,
                                   avg->pg_succ_avg_resp_time, avg->pg_succ_min_resp_time, avg->pg_succ_max_resp_time, succ,
                                   page_download_local_gp_ptr->succ_response.avg_time,
                                   page_download_local_gp_ptr->succ_response.min_time,
                                   page_download_local_gp_ptr->succ_response.max_time,
                                   page_download_local_gp_ptr->succ_response.succ); g_idx++;
      }
      else
      {
        GDF_COPY_TIMES_VECTOR_DATA(page_download_gp_idx, g_idx, gv_idx, 0,
                                   0, -1, 0, 0,
                                   page_download_local_gp_ptr->succ_response.avg_time,
                                   page_download_local_gp_ptr->succ_response.min_time,
                                   page_download_local_gp_ptr->succ_response.max_time,
                                   page_download_local_gp_ptr->succ_response.succ); g_idx++;
      }
     
      succ = avg->pg_tries - avg->pg_hits; 
      if(succ)
      {
        NSDL2_GDF(NULL, NULL, " Page Failure Response Time after ms avg->pg_fail_avg_resp_time = %d avg->pg_fail_min_resp_time = %d avg->pg_fail_max_resp_time = %d succ = %d ", avg->pg_fail_avg_resp_time, avg->pg_fail_min_resp_time, avg->pg_fail_max_resp_time, succ);
        GDF_COPY_TIMES_VECTOR_DATA(page_download_gp_idx, g_idx, gv_idx, 0,
                                   avg->pg_fail_avg_resp_time, avg->pg_fail_min_resp_time, avg->pg_fail_max_resp_time, succ,
                                   page_download_local_gp_ptr->fail_response.avg_time,
                                   page_download_local_gp_ptr->fail_response.min_time,
                                   page_download_local_gp_ptr->fail_response.max_time,
                                   page_download_local_gp_ptr->fail_response.succ); g_idx++;
      }
      else
      {
        GDF_COPY_TIMES_VECTOR_DATA(page_download_gp_idx, g_idx, gv_idx, 0,
                                   0, -1, 0, succ,
                                   page_download_local_gp_ptr->fail_response.avg_time,
                                   page_download_local_gp_ptr->fail_response.min_time,
                                   page_download_local_gp_ptr->fail_response.max_time,
                                   page_download_local_gp_ptr->fail_response.succ); g_idx++;
      }
     
      /* For Cumulative Page Hits */
      GDF_COPY_VECTOR_DATA(page_download_gp_idx, g_idx, gv_idx, 0,
                           cavg->pg_fetches_completed, page_download_local_gp_ptr->cum_tries); g_idx++;  
      GDF_COPY_VECTOR_DATA(page_download_gp_idx, g_idx, gv_idx, 0,
                           cavg->pg_succ_fetches, page_download_local_gp_ptr->cum_succ); g_idx++;

      
      /*JS proc time*/
      //succ = global_settings->exclude_failed_agg ? avg->pg_hits : avg->pg_tries;
      /*bug id 101742: using ms*/
      succ = avg->pg_tries;
      lol_page_js_proc_time_avg = (Long_data ) avg->page_js_proc_time_tot/((Long_data)succ);
      NSDL2_GDF(NULL, NULL, " Page JS Processing Time after ms lol_page_js_proc_time_avg = %lu avg->page_js_proc_time_min = %lu avg->page_js_proc_time_max  = %lu succ = %lu ",lol_page_js_proc_time_avg, avg->page_js_proc_time_min, avg->page_js_proc_time_max, succ);        GDF_COPY_TIMES_VECTOR_DATA(page_download_gp_idx, g_idx, gv_idx, 0,
                                 lol_page_js_proc_time_avg, avg->page_js_proc_time_min, avg->page_js_proc_time_max, succ,
                                 page_download_local_gp_ptr->page_js_proc_time.avg_time,
                                 page_download_local_gp_ptr->page_js_proc_time.min_time,
                                 page_download_local_gp_ptr->page_js_proc_time.max_time,
                                 page_download_local_gp_ptr->page_js_proc_time.succ); g_idx++;
     
      /*Page proc time*/
      //succ = global_settings->exclude_failed_agg ? avg->pg_hits : avg->pg_tries.
      lol_page_proc_time_avg = (Long_data ) avg->page_proc_time_tot/ ((Long_data)succ);
      NSDL2_GDF(NULL, NULL, " Page Processing Time after ms lol_page_proc_time_avg = %lu avg->page_proc_time_min = %lu avg->page_proc_time_max = %lu succ = %lu ",lol_page_proc_time_avg, avg->page_proc_time_min, avg->page_proc_time_max, succ);
      GDF_COPY_TIMES_VECTOR_DATA(page_download_gp_idx, g_idx, gv_idx, 0,
                                 lol_page_proc_time_avg, avg->page_proc_time_min, avg->page_proc_time_max, succ,
                                 page_download_local_gp_ptr->page_proc_time.avg_time,
                                 page_download_local_gp_ptr->page_proc_time.min_time,
                                 page_download_local_gp_ptr->page_proc_time.max_time,
                                 page_download_local_gp_ptr->page_proc_time.succ); g_idx++;

      /*Here to calculate Page failure corresponding to Page completed and Page Success.
        Formula is :- (((Page Completed - Page Success) * 100)/Page Completed) */
      /*GDF_COPY_VECTOR_DATA(page_download_gp_idx, g_idx, gv_idx, 0,
                           (avg->pg_tries == 0)?0:((double)((avg->pg_tries - avg->pg_hits) * 100)/avg->pg_tries), 
                           page_download_local_gp_ptr->failure); g_idx++;*/

      /*bug 103688 START ******************************************************************/
      NS_COPY_SAMPLE_2B_100_COUNT_4B_DATA((avg->pg_tries == 0)?0:((double)((avg->pg_tries - avg->pg_hits) * 100)/avg->pg_tries), avg->pg_tries, page_download_gp_idx, g_idx, gv_idx, 0,  page_download_local_gp_ptr->failure.sample, page_download_local_gp_ptr->failure.count)
      g_idx++;
      /******* bug 103688 END  *********************************************************/


      NSDL2_GDF(NULL, NULL, "Jagat: pg_tries = %llu, pg_hits = %llu, failure = %d, %d", avg->pg_tries, avg->pg_hits, page_download_local_gp_ptr->failure.sample, page_download_local_gp_ptr->failure.count);

      g_idx = 0;
      page_download_local_gp_ptr++;
    }
  }
}

static inline void log_page_download_gp ()
{
  /* bug id 101742*/
  if(page_download_gp_ptr == NULL) return;

  NSDL2_GDF(NULL, NULL, "Method called"); 
  fprintf(gui_fp, "\nPage Download: Page Download/Minute=%6.3f, Success Page Responses/Minute=%6.3f, Average Page Response Time (ms)=%6.3f, Minimum Page Response Time (ms)=%6.3f, Maximum Page Response Time (ms)=%6.3f, Successful Page Response Time (ms)=%6.3f, Total Page Hits=%0.0f , Total Page Successful=%0.0f, Page Proc Time(ms) Total:   min  %6.3f sec, avg  %6.3f sec, max  %6.3f sec JS: min  %6.3f sec, avg  %6.3f sec, max  %6.3f sec\n\n",
  convert_pm(page_download_gp_ptr->tries),
  convert_pm(page_download_gp_ptr->succ),
/*convert_sec*/(page_download_gp_ptr->response.avg_time), //Updating unit seconds to ms
/*convert_sec*/(page_download_gp_ptr->response.min_time), //Updating unit seconds to ms
/*convert_sec*/(page_download_gp_ptr->response.max_time), //Updating unit seconds to ms
/*convert_sec*/(page_download_gp_ptr->response.succ),      //Updating unit seconds to ms
  (page_download_gp_ptr->cum_tries),   // Assuming no function is to be applied
  (page_download_gp_ptr->cum_succ),   // Assuming no function is to be applied
  page_download_gp_ptr->page_proc_time.min_time,
  page_download_gp_ptr->page_proc_time.avg_time,
  page_download_gp_ptr->page_proc_time.max_time,
  page_download_gp_ptr->page_js_proc_time.min_time,
  page_download_gp_ptr->page_js_proc_time.avg_time,
  page_download_gp_ptr->page_js_proc_time.max_time);
}

// Function for filliling the data in the structure of Session_gp
static inline void fill_session_gp (avgtime **g_avg, cavgtime **g_cavg)
{
  int g_idx = 0, gv_idx, grp_idx;
  u_ns_4B_t succ = 0;
  avgtime *avg = NULL;
  cavgtime *cavg = NULL;
  avgtime *tmp_avg = NULL;
  cavgtime *tmp_cavg = NULL;

  if(session_gp_ptr == NULL) return;

  Session_gp *session_local_gp_ptr = session_gp_ptr;
 
  NSDL2_GDF(NULL, NULL, "Method called");
  for(gv_idx = 0; gv_idx < sgrp_used_genrator_entries + 1; gv_idx++)
  {
    tmp_avg = (avgtime *)g_avg[gv_idx];
    tmp_cavg = (cavgtime *)g_cavg[gv_idx];
    for(grp_idx = 0; grp_idx < TOTAL_GRP_ENTERIES_WITH_GRP_KW; grp_idx++)
    {
      /*bug id 101742: using method convert_long_pm() */
      avg = (avgtime*)((char*)tmp_avg + (grp_idx * g_avg_size_only_grp));
      cavg = (cavgtime*)((char*)tmp_cavg + (grp_idx * g_cavg_size_only_grp));

        NSDL2_GDF(NULL, NULL, "Sessions Started/Minute avg->ss_fetches_started = %lu Sessions Started/Minute after pm avg->ss_fetches_started = %f ",avg->ss_fetches_started,convert_long_pm(avg->ss_fetches_started));

      GDF_COPY_VECTOR_DATA(session_gp_idx, g_idx, gv_idx, 0,
                         convert_long_pm(avg->ss_fetches_started), session_local_gp_ptr->sess_started); g_idx++;

      NSDL2_GDF(NULL, NULL, "Sessions Completed/Minute avg->sess_tries = %lu ", avg->sess_tries);
      NSDL2_GDF(NULL, NULL, "Sessions Completed/Minute after pm avg->sess_tries = %f ", convert_long_pm(avg->sess_tries));
     
      GDF_COPY_VECTOR_DATA(session_gp_idx, g_idx, gv_idx, 0,
                          convert_long_pm(avg->sess_tries), session_local_gp_ptr->tries); g_idx++;

      NSDL2_GDF(NULL, NULL, "Successful Sessions/Minute avg->sess_hits = %lu ", avg->sess_hits);
      NSDL2_GDF(NULL, NULL, " Successful Sessions/Minute after pm avg->sess_hits = %f ", convert_long_pm(avg->sess_hits));
   
      GDF_COPY_VECTOR_DATA(session_gp_idx, g_idx, gv_idx, 0,
                           convert_long_pm(avg->sess_hits), session_local_gp_ptr->succ); g_idx++;
       
      // Here the "response" variable of `Times_data` data type is getting filled
   
      //succ = global_settings->exclude_failed_agg ? avg->sess_hits : avg->sess_tries;
      succ = avg->sess_tries;
      NSDL2_GDF(NULL, NULL, " Session Response Time after ms avg->sess_avg_time = %d avg->sess_min_time = %d avg->sess_max_time = %d succ = %d ",avg->sess_avg_time, avg->sess_min_time, avg->sess_max_time, succ);
      GDF_COPY_TIMES_VECTOR_DATA(session_gp_idx, g_idx, gv_idx, 0,
                                 avg->sess_avg_time, avg->sess_min_time, avg->sess_max_time, succ,
                                 session_local_gp_ptr->response.avg_time, 
                                 session_local_gp_ptr->response.min_time, 
                                 session_local_gp_ptr->response.max_time, 
                                 session_local_gp_ptr->response.succ); g_idx++;

      succ = avg->sess_hits;
      if(succ)
      {
        NSDL2_GDF(NULL, NULL, " Session Successful Response Time after ms avg->sess_succ_avg_resp_time = %lu avg->sess_succ_min_resp_time = %lu avg->sess_succ_max_resp_time = %lu succ = %lu ",avg->sess_succ_avg_resp_time, avg->sess_succ_min_resp_time, avg->sess_succ_max_resp_time, succ);
        GDF_COPY_TIMES_VECTOR_DATA(session_gp_idx, g_idx, gv_idx, 0,
                                   avg->sess_succ_avg_resp_time, avg->sess_succ_min_resp_time, avg->sess_succ_max_resp_time, succ,
                                   session_local_gp_ptr->succ_response.avg_time, 
                                   session_local_gp_ptr->succ_response.min_time, 
                                   session_local_gp_ptr->succ_response.max_time, 
                                   session_local_gp_ptr->succ_response.succ); g_idx++;

      }
      else
      {
        GDF_COPY_TIMES_VECTOR_DATA(session_gp_idx, g_idx, gv_idx, 0,
                                   0, -1, 0, 0, 
                                   session_local_gp_ptr->succ_response.avg_time, 
                                   session_local_gp_ptr->succ_response.min_time, 
                                   session_local_gp_ptr->succ_response.max_time, 
                                   session_local_gp_ptr->succ_response.succ); g_idx++;
      }

      succ = (avg->sess_tries - avg->sess_hits);
      if(succ)
      {
        NSDL2_GDF(NULL, NULL, " Session Failure Response Time after ms avg->sess_fail_avg_resp_time = %lu avg->sess_fail_min_resp_time = %lu avg->sess_fail_max_resp_time = %lu succ = %lu ",avg->sess_fail_avg_resp_time, avg->sess_fail_min_resp_time, avg->sess_fail_max_resp_time, succ);
        GDF_COPY_TIMES_VECTOR_DATA(session_gp_idx, g_idx, gv_idx, 0,
                                   avg->sess_fail_avg_resp_time, avg->sess_fail_min_resp_time, avg->sess_fail_max_resp_time, succ, 
                                   session_local_gp_ptr->fail_response.avg_time, 
                                   session_local_gp_ptr->fail_response.min_time, 
                                   session_local_gp_ptr->fail_response.max_time, 
                                   session_local_gp_ptr->fail_response.succ); g_idx++;
      }
      else
      {
        GDF_COPY_TIMES_VECTOR_DATA(session_gp_idx, g_idx, gv_idx, 0,
                                   0, -1, 0, 0, 
                                   session_local_gp_ptr->fail_response.avg_time, 
                                   session_local_gp_ptr->fail_response.min_time, 
                                   session_local_gp_ptr->fail_response.max_time, 
                                   session_local_gp_ptr->fail_response.succ); g_idx++;
      }
     
      /* For Cumulative session hits */
      GDF_COPY_VECTOR_DATA(session_gp_idx, g_idx, gv_idx, 0,
                           cavg->sess_fetches_completed, session_local_gp_ptr->cum_tries); g_idx++;
      GDF_COPY_VECTOR_DATA(session_gp_idx, g_idx, gv_idx, 0,
                           cavg->sess_succ_fetches, session_local_gp_ptr->cum_succ); g_idx++;

      /*Here to calculate Session failure corresponding to Session completed and Session Success.
        Formula is :- (((Session Completed - Session Success) * 100)/Session Completed) */
      /*GDF_COPY_VECTOR_DATA(session_gp_idx, g_idx, gv_idx, 0,
                           (avg->sess_tries == 0)?0:((double)((avg->sess_tries - avg->sess_hits) * 100)/avg->sess_tries), 
                           session_local_gp_ptr->failure); g_idx++;*/
      /*bug 103688 START ******************************************************************/
      NS_COPY_SAMPLE_2B_100_COUNT_4B_DATA((avg->sess_tries == 0)?0:((double)((avg->sess_tries - avg->sess_hits) * 100)/avg->sess_tries), avg->sess_tries, session_gp_idx, g_idx, gv_idx, 0, session_local_gp_ptr->failure.sample, session_local_gp_ptr->failure.count)
      g_idx++;
      /******* bug 103688 END  *********************************************************/
   
 
      NSDL3_GDF(NULL, NULL, "sess_tries = %llu, sess_hits = %llu, failure = %d, %d", avg->sess_tries, avg->sess_hits, session_local_gp_ptr->failure.sample, session_local_gp_ptr->failure.count);
      g_idx = 0;
      session_local_gp_ptr++;
    }      
  }
}

static inline void log_session_gp ()
{
  /* bug id 101742 using ms*/
  if(session_gp_ptr == NULL) return;
  NSDL2_GDF(NULL, NULL, "Method called");
  fprintf(gui_fp, "\nSession: Sessions/Minute=%6.3f, Successful Sessions/Minute=%6.3f, Average Session Response Time (ms)=%6.3f, Minimum Session Response Time (ms)=%6.3f, Maximum Session Response Time (ms)=%6.3f, Successful Session Response Time (ms)=%6.3f, Total Session Hits=%0.0f , Total Session Successful=%0.0f\n\n",
  convert_pm(session_gp_ptr->tries),
  convert_pm(session_gp_ptr->succ),
/*convert_sec*/(session_gp_ptr->response.avg_time),   //Updating unit seconds to ms
/*convert_sec*/(session_gp_ptr->response.min_time),   //Updating unit seconds to ms
/*convert_sec*/(session_gp_ptr->response.max_time),   //Updating unit seconds to ms
/*convert_sec*/(session_gp_ptr->response.succ),       //Updating unit seconds to ms
  (session_gp_ptr->cum_tries),       // Assuming no function is to be applied
  (session_gp_ptr->cum_succ));       // Assuming no function is to be applied
}

// Function for filliling the data in the structure of Trans_overall_gp
static inline void fill_trans_overall_gp (avgtime **g_avg, cavgtime **g_cavg)
{
  int g_idx = 0, gv_idx, grp_idx;
  u_ns_4B_t succ;
  avgtime *avg = NULL;  
  cavgtime *cavg = NULL;  
  avgtime *tmp_avg = NULL;  
  cavgtime *tmp_cavg = NULL;  

  if(trans_overall_gp_ptr == NULL) return;
  Trans_overall_gp *trans_overall_local_gp_ptr = trans_overall_gp_ptr;

  NSDL2_GDF(NULL, NULL, "Method called");

  for(gv_idx = 0; gv_idx < sgrp_used_genrator_entries + 1; gv_idx++)
  {
    /*bug id 101742: using method convert_long_ps() */
    tmp_avg = (avgtime *)g_avg[gv_idx];
    tmp_cavg = (cavgtime *)g_cavg[gv_idx];
    for(grp_idx = 0; grp_idx < TOTAL_GRP_ENTERIES_WITH_GRP_KW; grp_idx++)
    {
      avg = (avgtime*)((char*)tmp_avg + (grp_idx * g_avg_size_only_grp));
      cavg = (cavgtime*)((char*)tmp_cavg + (grp_idx * g_cavg_size_only_grp));

      NSDL2_GDF(NULL, NULL, "Transactions Started/Second avg->tx_fetches_started %lu,", avg->tx_fetches_started);
      NSDL2_GDF(NULL, NULL, "Transactions Started/Second after PS avg->tx_fetches_started %lu,", convert_long_ps(avg->tx_fetches_started));

      GDF_COPY_VECTOR_DATA(trans_overall_gp_idx, g_idx, gv_idx, 0,
                           convert_long_ps(avg->tx_fetches_started), trans_overall_local_gp_ptr->trans_started); g_idx++;

      NSDL2_GDF(NULL, NULL, "Transactions Completed/Second avg->tx_fetches_completed %lu,", avg->tx_fetches_completed);
      NSDL2_GDF(NULL, NULL, "Transactions Completed/Second after PS avg->tx_fetches_completed %lu,", convert_long_ps(avg->tx_fetches_completed));     

      GDF_COPY_VECTOR_DATA(trans_overall_gp_idx, g_idx, gv_idx, 0,
                            convert_long_ps(avg->tx_fetches_completed), trans_overall_local_gp_ptr->tries); g_idx++;
 
      NSDL2_GDF(NULL, NULL, "Transactions Successful/Second avg->tx_succ_fetches %lu,", avg->tx_succ_fetches);
      NSDL2_GDF(NULL, NULL, "Transactions Successful/Second after PS avg->tx_succ_fetches %lu,", convert_long_ps(avg->tx_succ_fetches));
    
      GDF_COPY_VECTOR_DATA(trans_overall_gp_idx, g_idx, gv_idx, 0,
                           convert_long_ps(avg->tx_succ_fetches), trans_overall_local_gp_ptr->succ); g_idx++;
     
            //Here the "response" variable of `Times_data` data type is getting filled
   
      //succ = global_settings->exclude_failed_agg ? avg->tx_succ_fetches : avg->tx_fetches_completed;
      succ = avg->tx_fetches_completed;
      NSDL2_GDF(NULL, NULL, " Transaction Response Time after ms avg->tx_avg_time = %d avg->tx_min_time = %d avg->tx_max_time = %d succ = %d ",avg->tx_avg_time, avg->tx_min_time, avg->tx_max_time, succ);
      GDF_COPY_TIMES_VECTOR_DATA(trans_overall_gp_idx, g_idx, gv_idx, 0, 
                                 avg->tx_avg_time, avg->tx_min_time, avg->tx_max_time, succ,
                                 trans_overall_local_gp_ptr->response.avg_time, 
                                 trans_overall_local_gp_ptr->response.min_time, 
                                 trans_overall_local_gp_ptr->response.max_time, 
                                 trans_overall_local_gp_ptr->response.succ); g_idx++;

      succ = avg->tx_succ_fetches;   //For Success response Time
      if(succ)
      {
        NSDL2_GDF(NULL, NULL, " Transaction Successful Response Time after ms avg->tx_succ_avg_resp_time = %d avg->tx_succ_min_resp_time = %d avg->tx_succ_max_resp_time = %d succ = %d ",avg->tx_succ_avg_resp_time , avg->tx_succ_min_resp_time, avg->tx_succ_max_resp_time, succ);
        GDF_COPY_TIMES_VECTOR_DATA(trans_overall_gp_idx, g_idx, gv_idx, 0, 
                                   avg->tx_succ_avg_resp_time, avg->tx_succ_min_resp_time, avg->tx_succ_max_resp_time, succ,
                                   trans_overall_local_gp_ptr->succ_response.avg_time, 
                                   trans_overall_local_gp_ptr->succ_response.min_time, 
                                   trans_overall_local_gp_ptr->succ_response.max_time, 
                                   trans_overall_local_gp_ptr->succ_response.succ); g_idx++;
      }
      else
      {
        GDF_COPY_TIMES_VECTOR_DATA(trans_overall_gp_idx, g_idx, gv_idx, 0, 
                                   0, -1, 0, 0,
                                   trans_overall_local_gp_ptr->succ_response.avg_time, 
                                   trans_overall_local_gp_ptr->succ_response.min_time, 
                                   trans_overall_local_gp_ptr->succ_response.max_time, 
                                   trans_overall_local_gp_ptr->succ_response.succ); g_idx++;
      }

      succ = avg->tx_fetches_completed - avg->tx_succ_fetches;   //For Failure response Time
      if (succ)
      {
        NSDL2_GDF(NULL, NULL, " Transaction Failure Response Time after ms avg->tx_fail_avg_resp_time = %d avg->tx_fail_min_resp_time = %d avg->tx_fail_max_resp_time = %d succ = %d ",avg->tx_fail_avg_resp_time , avg->tx_fail_min_resp_time, avg->tx_fail_max_resp_time, succ);
        GDF_COPY_TIMES_VECTOR_DATA(trans_overall_gp_idx, g_idx, gv_idx, 0,
                                   avg->tx_fail_avg_resp_time, avg->tx_fail_min_resp_time, avg->tx_fail_max_resp_time, succ,
                                   trans_overall_local_gp_ptr->fail_response.avg_time, 
                                   trans_overall_local_gp_ptr->fail_response.min_time, 
                                   trans_overall_local_gp_ptr->fail_response.max_time, 
                                   trans_overall_local_gp_ptr->fail_response.succ); g_idx++;
      }
      else 
      {
        GDF_COPY_TIMES_VECTOR_DATA(trans_overall_gp_idx, g_idx, gv_idx, 0,
                                   0, -1, 0, 0,
                                   trans_overall_local_gp_ptr->fail_response.avg_time, 
                                   trans_overall_local_gp_ptr->fail_response.min_time, 
                                   trans_overall_local_gp_ptr->fail_response.max_time, 
                                   trans_overall_local_gp_ptr->fail_response.succ); g_idx++;
      }

      NSDL4_GDF(NULL, NULL, "g_idx - %d", g_idx);
     
      NSDL2_GDF(NULL, NULL, "tx_fetches_completed = %llu, tx_c_succ_fetches = %llu", cavg->tx_c_fetches_completed, cavg->tx_c_succ_fetches);
      /* For Cumulative Transaction Hits */
      GDF_COPY_VECTOR_DATA(trans_overall_gp_idx, g_idx, gv_idx, 0,
                           cavg->tx_c_fetches_completed, trans_overall_local_gp_ptr->cum_tries); g_idx++;  

      NSDL4_GDF(NULL, NULL, "g_idx - %d", g_idx);
      GDF_COPY_VECTOR_DATA(trans_overall_gp_idx, g_idx, gv_idx, 0,
                           cavg->tx_c_succ_fetches, trans_overall_local_gp_ptr->cum_succ); g_idx++;

      /*Here to calculate Transaction failure corresponding to Transaction completed and Transaction Success.
        Formula is :- (((TX Completed - TX Success) * 100)/TX Completed) */
      NSDL4_GDF(NULL, NULL, "g_idx - %d", g_idx);
      //GDF_COPY_VECTOR_DATA(trans_overall_gp_idx, g_idx, gv_idx, 0, (avg->tx_fetches_completed == 0)?0:((double)((avg->tx_fetches_completed - avg->tx_succ_fetches) * 100)/avg->tx_fetches_completed), trans_overall_local_gp_ptr->failure); g_idx++;
      /*bug 85621 START ******************************************************************/
      short pct = (avg->tx_fetches_completed == 0)?0:(((int)((avg->tx_fetches_completed - avg->tx_succ_fetches) * 100)/avg->tx_fetches_completed));
      int count =  avg->tx_fetches_completed;//(avg->tx_fetches_completed - avg->tx_succ_fetches);
      pct *= 100;
      NSDL1_GDF(NULL, NULL, "pct=%ld count =%d", pct, count);
      GDF_COPY_SAMPLE_2B_100_COUNT_4B_DATA(trans_overall_gp_idx, g_idx, gv_idx, 0, pct, count, trans_overall_local_gp_ptr->failure.sample, trans_overall_local_gp_ptr->failure.count);
      g_idx++;
      /******* bug 85621 END  *********************************************************/


      succ = avg->tx_fetches_completed;   //For Think time 
      NSDL2_GDF(NULL, NULL, " Transaction Think Time after ms ((double )avg->tx_tot_think_time/succ %d,", ((double )avg->tx_tot_think_time/succ));
      NSDL2_GDF(NULL, NULL, " Transaction Think Time after ms (double)avg->tx_min_think_time %d,", (double)avg->tx_min_think_time);
      NSDL2_GDF(NULL, NULL, " Transaction Think Time after ms (double)avg->tx_max_think_time %d,", (double)avg->tx_max_think_time);
      NSDL2_GDF(NULL, NULL, " Transaction Think Time after ms succ %d,", succ); 
      GDF_COPY_TIMES_VECTOR_DATA(trans_overall_gp_idx, g_idx, gv_idx, 0, 
                                 ((double )avg->tx_tot_think_time/succ),
                                 (double)avg->tx_min_think_time, 
                                 (double)avg->tx_max_think_time, succ,
                                 trans_overall_local_gp_ptr->think_time.avg_time, 
                                 trans_overall_local_gp_ptr->think_time.min_time, 
                                 trans_overall_local_gp_ptr->think_time.max_time, 
                                 trans_overall_local_gp_ptr->think_time.succ); g_idx++;

      GDF_COPY_VECTOR_DATA(trans_overall_gp_idx, g_idx, gv_idx, 0,
                           (avg->tx_tx_bytes * kbps_factor), trans_overall_local_gp_ptr->tx_tx_throughput); g_idx++;

      GDF_COPY_VECTOR_DATA(trans_overall_gp_idx, g_idx, gv_idx, 0,
                           (avg->tx_rx_bytes * kbps_factor), trans_overall_local_gp_ptr->tx_rx_throughput);

      NSDL2_GDF(NULL, NULL, "tx_fetches_completed = %llu, tx_succ_fetches = %llu, succ = %f avg->tx_tx_bytes = %llu,"
                            "avg->tx_rx_bytes=%llu, kbps_factor = %f, send_throughput(Kbps) = %f, recv_throughput(Kbps) = %f",
                          avg->tx_fetches_completed, avg->tx_succ_fetches, trans_overall_local_gp_ptr->failure,
                          avg->tx_tx_bytes, avg->tx_rx_bytes, kbps_factor, (avg->tx_tx_bytes * kbps_factor), (avg->tx_rx_bytes * kbps_factor));
      g_idx = 0;
      trans_overall_local_gp_ptr++;
    }
  }
}

static inline void log_trans_overall_gp ()
{
  /* bug id 101742 using ms*/
  if(trans_overall_gp_ptr == NULL) return;
  NSDL2_GDF(NULL, NULL, "Method called");
  fprintf(gui_fp, "\nTransactions: Transactions/Minute=%6.3f, Successful Transactions/Minute=%6.3f, Average Transaction Response Time (ms)=%6.3f, Minimum Transaction Response Time (ms)=%6.3f, Maximum Transaction Response Time (ms)=%6.3f, Successful Transaction Response Time (ms)=%6.3f, Total Transaction Hits=%0.0f, Total Transaction Successful=%0.0f\n",
  convert_pm(trans_overall_gp_ptr->tries),
  convert_pm(trans_overall_gp_ptr->succ),
/*convert_sec*/(trans_overall_gp_ptr->response.avg_time),  //Updating unit seconds to ms
/*convert_sec*/(trans_overall_gp_ptr->response.min_time),  //Updating unit seconds to ms
/*convert_sec*/(trans_overall_gp_ptr->response.max_time),  //Updating unit seconds to ms
/*convert_sec*/(trans_overall_gp_ptr->response.succ),      //Updating unit seconds to ms
  (trans_overall_gp_ptr->cum_tries),         // Assuming no function is to be applied
  (trans_overall_gp_ptr->cum_succ));         // Assuming no function is to be applied
}

// Function for filliling the data in the structure of URL_fail_gp
static inline void fill_url_fail_gp (avgtime **g_avg)
{
  if(url_fail_gp_ptr == NULL) return;

  unsigned int url_all_failures = 0;
  int i, gv_idx, all_idx, j = 0, grp_idx;
  avgtime *avg = NULL;
  avgtime *tmp_avg = NULL;
  URL_fail_gp *url_fail_local_gp_ptr = url_fail_gp_ptr;
  URL_fail_gp *url_fail_local_first_gp_ptr = NULL;
 
  NSDL2_GDF(NULL, NULL, "Method called");

  // Note avg->url_error_codes[0] is success, do index with + 1
  // Changed in 3.7.7 to show only  used errors code
  for(gv_idx = 0; gv_idx < sgrp_used_genrator_entries + 1; gv_idx++)
  {
    tmp_avg = (avgtime *)g_avg[gv_idx];
    for(grp_idx = 0; grp_idx < TOTAL_GRP_ENTERIES_WITH_GRP_KW; grp_idx++)
    {
      avg = (avgtime*)((char*)tmp_avg + (grp_idx * g_avg_size_only_grp));
      all_idx = j;
      j++;
      url_fail_local_first_gp_ptr = url_fail_local_gp_ptr;
      url_fail_local_gp_ptr++;
      for (i = 0; i < (TOTAL_USED_URL_ERR - 1); i++, j++) // -1 as one is for success
      {
        GDF_COPY_VECTOR_DATA(url_fail_gp_idx, 0, j, 0, avg->url_error_codes[i + 1], url_fail_local_gp_ptr->failures);
        GDF_COPY_SCALAR_DATA(url_fail_gp_idx, 1, avg->url_error_codes[i + 1], url_fail_local_gp_ptr->failure_count);
        url_all_failures += avg->url_error_codes[i + 1];
        url_fail_local_gp_ptr++;
      }
      /*bug id 101742: using method convert_long_ps() */
      NSDL2_GDF(NULL, NULL, "HTTP Failures/Sec url_all_failures = %lu ", url_all_failures);
      NSDL2_GDF(NULL, NULL, "HTTP Failures/Sec after PS url_all_failures = %lu ", convert_long_ps(url_all_failures));

      GDF_COPY_VECTOR_DATA(url_fail_gp_idx, 0, all_idx, 0, convert_long_ps(url_all_failures), url_fail_local_first_gp_ptr->failures);
      GDF_COPY_SCALAR_DATA(url_fail_gp_idx, 1, url_all_failures, url_fail_local_first_gp_ptr->failure_count);
      url_all_failures = 0;
    }
  }
}

static inline void fill_smtp_fail_gp (avgtime **g_avg)
{
  if(smtp_fail_gp_ptr == NULL) return;
  SMTP_fail_gp *smtp_fail_local_gp_ptr = smtp_fail_gp_ptr;
  SMTP_fail_gp *smtp_fail_local_first_gp_ptr = NULL;
  unsigned int url_all_failures = 0;
  int i, gv_idx, all_idx, j = 0, grp_idx;
  avgtime *avg = NULL;
  avgtime *tmp_avg = NULL;
  NSDL2_GDF(NULL, NULL, "Method called");

  for(gv_idx = 0; gv_idx < sgrp_used_genrator_entries + 1; gv_idx++)
  { 
    tmp_avg = (avgtime *)g_avg[gv_idx];
    for(grp_idx = 0; grp_idx < TOTAL_GRP_ENTERIES_WITH_GRP_KW; grp_idx++)
    {
      avg = (avgtime*)((char*)tmp_avg + (grp_idx * g_avg_size_only_grp));
      all_idx = j;
      j++;
      smtp_fail_local_first_gp_ptr = smtp_fail_local_gp_ptr;
      smtp_fail_local_gp_ptr++;
      for (i = 0 ; i < (TOTAL_USED_URL_ERR - 1); i++, j++)
      {
        GDF_COPY_VECTOR_DATA(smtp_fail_gp_idx, 0, j, 0, avg->smtp_error_codes[i + 1], smtp_fail_local_gp_ptr->failures);
        url_all_failures += avg->smtp_error_codes[i + 1];
        smtp_fail_local_gp_ptr++;
      }
      GDF_COPY_VECTOR_DATA(smtp_fail_gp_idx, 0, all_idx, 0, convert_long_ps(url_all_failures), smtp_fail_local_first_gp_ptr->failures);
      url_all_failures = 0;
      NSDL2_GDF(NULL, NULL, " SMTP Failures/Sec url_all_failures = %lu ", url_all_failures);
      NSDL2_GDF(NULL, NULL, " SMTP Failures/Sec after ps url_all_failures = %lu ", convert_long_ps(url_all_failures));
    }
  }
}

static inline void fill_pop3_fail_gp (avgtime **g_avg)
{
  if(pop3_fail_gp_ptr == NULL) return;
  POP3_fail_gp *pop3_fail_local_gp_ptr = pop3_fail_gp_ptr;
  POP3_fail_gp *pop3_fail_local_first_gp_ptr = NULL;
  unsigned int url_all_failures = 0;
  int i, gv_idx, all_idx, j = 0, grp_idx;
  avgtime *avg = NULL;
  avgtime *tmp_avg = NULL;

  NSDL2_GDF(NULL, NULL, "Method called");
  for(gv_idx = 0; gv_idx < sgrp_used_genrator_entries + 1; gv_idx++)
  {
    tmp_avg = (avgtime *)g_avg[gv_idx];
    for(grp_idx = 0; grp_idx < TOTAL_GRP_ENTERIES_WITH_GRP_KW; grp_idx++)
    {
      avg = (avgtime*)((char*)tmp_avg + (grp_idx * g_avg_size_only_grp));
      all_idx = j;
      j++;
      pop3_fail_local_first_gp_ptr = pop3_fail_local_gp_ptr;
      pop3_fail_local_gp_ptr++;
      for (i = 0; i < (TOTAL_USED_URL_ERR - 1); i++, j++)
      {
        GDF_COPY_VECTOR_DATA(pop3_fail_gp_idx, 0, j, 0, avg->pop3_error_codes[i + 1], pop3_fail_local_gp_ptr->failures);
        GDF_COPY_SCALAR_DATA(pop3_fail_gp_idx, 1, avg->pop3_error_codes[i + 1], pop3_fail_local_gp_ptr->failure_count);
        url_all_failures += avg->pop3_error_codes[i + 1];
        pop3_fail_local_gp_ptr++;
      }
      GDF_COPY_VECTOR_DATA(pop3_fail_gp_idx, 0, all_idx, 0, convert_long_ps(url_all_failures), pop3_fail_local_first_gp_ptr->failures);
      NSDL2_GDF(NULL, NULL, "POP3  Failures/Sec url_all_failures %lu,", url_all_failures);
      NSDL2_GDF(NULL, NULL, "POP3 Failures/Sec after ps url_all_failures  %lu,", convert_long_ps(url_all_failures));
      GDF_COPY_SCALAR_DATA(pop3_fail_gp_idx, 1, url_all_failures, pop3_fail_local_first_gp_ptr->failure_count);
      url_all_failures = 0;
    }
  }
}

/* dns */
static inline void fill_dns_fail_gp (avgtime **g_avg)
{
  if(dns_fail_gp_ptr == NULL) return;
  DNS_fail_gp *dns_fail_gp_local_ptr = dns_fail_gp_ptr;
  DNS_fail_gp *dns_fail_gp_local_first_ptr = NULL;
  unsigned int url_all_failures = 0;
  int i, gv_idx, j = 0, all_idx, grp_idx;
  avgtime *avg = NULL;
  avgtime *tmp_avg = NULL;

  NSDL2_GDF(NULL, NULL, "Method called");
  for(gv_idx = 0; gv_idx < sgrp_used_genrator_entries + 1; gv_idx++)
  {
    tmp_avg = (avgtime *)g_avg[gv_idx];
    for(grp_idx = 0; grp_idx < TOTAL_GRP_ENTERIES_WITH_GRP_KW; grp_idx++)
    {
      avg = (avgtime*)((char*)tmp_avg + (grp_idx * g_avg_size_only_grp));
      all_idx = j;
      j++;
      dns_fail_gp_local_first_ptr = dns_fail_gp_local_ptr;
      dns_fail_gp_local_ptr++;
      for (i = 0; i < (TOTAL_USED_URL_ERR - 1); i++, j++)
      {
        GDF_COPY_VECTOR_DATA(dns_fail_gp_idx, 0, j, 0, avg->dns_error_codes[i + 1], dns_fail_gp_local_ptr->failures);
        GDF_COPY_SCALAR_DATA(dns_fail_gp_idx, 1, avg->dns_error_codes[i + 1], dns_fail_gp_local_ptr->failure_count);
        url_all_failures += avg->dns_error_codes[i + 1];
        dns_fail_gp_local_ptr++; 
      }
      GDF_COPY_VECTOR_DATA(dns_fail_gp_idx, 0, all_idx, 0, url_all_failures, dns_fail_gp_local_first_ptr->failures);
      GDF_COPY_SCALAR_DATA(dns_fail_gp_idx, 1, url_all_failures, dns_fail_gp_local_first_ptr->failure_count);
      url_all_failures = 0;
    }
  }
}

static inline void log_url_fail_gp ()
{
  if(url_fail_gp_ptr == NULL) return;
  int i;
  NSDL2_GDF(NULL, NULL, "Method called");
  fprintf(gui_fp,"\nURL Failures: ");
  // if (url_fail_gp_ptr->failures) // Since we are printing only non Zero element
    fprintf(gui_fp,"Failed URL Responses (All Errors)/Second=%6.3f\n", convert_ps(url_fail_gp_ptr->failures));

  // Since log is for debugging, we log all errors including not used
  for (i = 0; i < (TOTAL_URL_ERR - 1); i++) // -1 as one is for success
  {
    if (url_fail_gp_ptr->failures) // Since we are printing only non Zero element
      fprintf(gui_fp,"Failed URL Responses/Second[%s (%d)]=%6.3f, ",  get_error_code_name(i), i, convert_ps(url_fail_gp_ptr->failures)); // idx of URL errors is 0 in the errorCodeTable
  }
  fprintf(gui_fp, "\n");
}

// Function for filliling the data in the structure of Page_fail_gp
static inline void fill_page_fail_gp (avgtime **g_avg)
{
  if(page_fail_gp_ptr == NULL) return;

  unsigned int pg_all_failures = 0;
  int i, j = 0, gv_idx, all_idx, grp_idx;
  avgtime *avg = NULL;
  avgtime *tmp_avg = NULL;
  Page_fail_gp *page_fail_local_gp_ptr = page_fail_gp_ptr;
  Page_fail_gp *page_fail_local_first_gp_ptr = NULL;
 
  NSDL2_GDF(NULL, NULL, "Method called");
  //change in 3.7.7 to skip undef in gdf
  for(gv_idx = 0; gv_idx < sgrp_used_genrator_entries + 1; gv_idx++)
  {
    tmp_avg = (avgtime *)g_avg[gv_idx];
    for(grp_idx = 0; grp_idx < TOTAL_GRP_ENTERIES_WITH_GRP_KW; grp_idx++)
    {
      avg = (avgtime*)((char*)tmp_avg + (grp_idx * g_avg_size_only_grp));
      all_idx = j;
      j++;
      page_fail_local_first_gp_ptr = page_fail_local_gp_ptr;
      page_fail_local_gp_ptr++;
     
      for (i = 0; i < (TOTAL_USED_URL_ERR - 1); i++, j++) {   //same as url
        pg_all_failures += avg->pg_error_codes[i + 1];
        GDF_COPY_VECTOR_DATA(page_fail_gp_idx, 0, j, 0, avg->pg_error_codes[i + 1], page_fail_local_gp_ptr->failures);
        GDF_COPY_SCALAR_DATA(page_fail_gp_idx, 1, avg->pg_error_codes[i + 1], page_fail_local_gp_ptr->failure_count);
        page_fail_local_gp_ptr++;
      }
   
      for (i = TOTAL_URL_ERR; i < TOTAL_USED_PAGE_ERR; i++, j++)
      {
        pg_all_failures += avg->pg_error_codes[i]; // Index using i (not i + 1)
        GDF_COPY_VECTOR_DATA(page_fail_gp_idx, 0, j, 0, avg->pg_error_codes[i], page_fail_local_gp_ptr->failures);
        GDF_COPY_SCALAR_DATA(page_fail_gp_idx, 1, avg->pg_error_codes[i], page_fail_local_gp_ptr->failure_count);
        page_fail_local_gp_ptr++;
      }
      NSDL2_GDF(NULL, NULL, "Page Failures/Minute pg_all_failures = %lu ", pg_all_failures);
      NSDL2_GDF(NULL, NULL, "Page Failures/Minute after PM pg_all_failures = %lu ", convert_long_pm(pg_all_failures));
     
      GDF_COPY_VECTOR_DATA(page_fail_gp_idx, 0, all_idx, 0, convert_long_pm(pg_all_failures), page_fail_local_first_gp_ptr->failures);
      GDF_COPY_SCALAR_DATA(page_fail_gp_idx, 1, pg_all_failures, page_fail_local_first_gp_ptr->failure_count);
      pg_all_failures = 0;
    }
  }
}

static inline void log_page_fail_gp ()
{
  if(page_fail_gp_ptr == NULL) return;
  int i;
  NSDL2_GDF(NULL, NULL, "Method called");
  fprintf(gui_fp,"\nPage Failures: ");
  // if (page_fail_gp_ptr->failures) // Since we are printing only non Zero element
    fprintf(gui_fp,"Failed Page Responses (All Errors)/Minute=%6.3f\n", convert_pm(page_fail_gp_ptr->failures));

  // Since log is for debugging, we log all errors including not used
  for (i = 0; i < (TOTAL_PAGE_ERR - 1); i++)
  {
    if (page_fail_gp_ptr->failures) // Since we are printing only non Zero element
      fprintf(gui_fp,"Failed Page Responses/Minute[%s (%d)]=%6.3f, ", get_error_code_name(i), i, convert_pm(page_fail_gp_ptr->failures));
  }
  fprintf(gui_fp, "\n");
}

// Function for filliling the data in the structure of Session_fail_gp
static inline void fill_session_fail_gp (avgtime **g_avg)
{
  if(session_fail_gp_ptr == NULL) return;

  unsigned int sess_all_failures = 0;
  int i, gv_idx, all_idx, j = 0, grp_idx;
  avgtime *avg = NULL;
  avgtime *tmp_avg = NULL;
  Session_fail_gp *session_fail_local_gp_ptr = session_fail_gp_ptr;
  Session_fail_gp *session_fail_local_first_gp_ptr = NULL;

  NSDL2_GDF(NULL, NULL, "Method called");
  for(gv_idx = 0; gv_idx < sgrp_used_genrator_entries + 1; gv_idx++)
  {
    tmp_avg = (avgtime *)g_avg[gv_idx];
    for(grp_idx = 0; grp_idx < TOTAL_GRP_ENTERIES_WITH_GRP_KW; grp_idx++)
    {
      avg = (avgtime*)((char*)tmp_avg + (grp_idx * g_avg_size_only_grp));
      all_idx = j;
      j++;
      session_fail_local_first_gp_ptr = session_fail_local_gp_ptr;
      session_fail_local_gp_ptr++;
      for (i = 0; i < (TOTAL_SESS_ERR - 1); i++, j++)
      {
        sess_all_failures += avg->sess_error_codes[i + 1];
        GDF_COPY_VECTOR_DATA(session_fail_gp_idx, 0, j, 0, avg->sess_error_codes[i + 1], session_fail_local_gp_ptr->failures);
        GDF_COPY_SCALAR_DATA(session_fail_gp_idx, 1, avg->sess_error_codes[i + 1], session_fail_local_gp_ptr->failure_count);
        session_fail_local_gp_ptr++;
      }
      /*bug id 101742: using method convert_long_pm() */
      NSDL2_GDF(NULL, NULL, "Sessions Failures/Minute sess_all_failures = %lu ", sess_all_failures);
      NSDL2_GDF(NULL, NULL, "Sessions Failures/Minute after PM sess_all_failures = %lu ", convert_long_pm(sess_all_failures));

      GDF_COPY_VECTOR_DATA(session_fail_gp_idx, 0, all_idx, 0, convert_long_pm(sess_all_failures), session_fail_local_first_gp_ptr->failures);
      GDF_COPY_SCALAR_DATA(session_fail_gp_idx, 1, sess_all_failures, session_fail_local_first_gp_ptr->failure_count);
      sess_all_failures = 0;
    }
  }
}

static inline void log_session_fail_gp ()
{
  if(session_fail_gp_ptr == NULL) return;

  int i;
  NSDL2_GDF(NULL, NULL, "Method called");
  fprintf(gui_fp,"\nSession Failures: ");
  //if (session_fail_gp_ptr->failures) // Since we are printing only non Zero element
  fprintf(gui_fp,"Failed Sessions (All Errors)/Minute=%6.3f\n", convert_pm(session_fail_gp_ptr->failures));

  for (i = 0; i < (TOTAL_SESS_ERR - 1); i++)
  {
    if (session_fail_gp_ptr->failures)  // Since we are printing only non Zero element 
      fprintf(gui_fp,"Failed Sessions/Minute[%s (%d)]=%6.3f, ", get_session_error_code_name(i), i, convert_pm(session_fail_gp_ptr->failures));
  }
  fprintf(gui_fp, "\n");
}

// Function for filliling the data in the structure of Trans_fail_gp
static inline void fill_trans_fail_gp (avgtime **g_avg)
{
  if(trans_fail_gp_ptr == NULL) return;

  unsigned int trans_all_failures = 0;
  int i, j = 0, gv_idx, all_idx, grp_idx;
  avgtime *avg = NULL;
  avgtime *tmp_avg = NULL;
  Trans_fail_gp *trans_fail_local_gp_ptr = trans_fail_gp_ptr;
  Trans_fail_gp *trans_fail_local_first_gp_ptr = NULL;

  //changed in 3.7.7 to skip undefs in gdf
  NSDL2_GDF(NULL, NULL, "Method called");

  for(gv_idx = 0; gv_idx < sgrp_used_genrator_entries + 1; gv_idx++)
  {
    tmp_avg = (avgtime *)g_avg[gv_idx];
    for(grp_idx = 0; grp_idx < TOTAL_GRP_ENTERIES_WITH_GRP_KW; grp_idx++)
    {
      avg = (avgtime*)((char*)tmp_avg + (grp_idx * g_avg_size_only_grp));
      all_idx = j;         //Saving index of all indices because this will filled at the last when all grafhs count is done
      j++;
      trans_fail_local_first_gp_ptr = trans_fail_local_gp_ptr;
      trans_fail_local_gp_ptr++;
      //Trans_fail_gp *trans_fail_for_first_gp_ptr = trans_fail_local_gp_ptr;
      for (i = 0; i < (TOTAL_USED_URL_ERR - 1); i++, j++)
      {
        trans_all_failures += avg->tx_error_codes[i + 1];
        GDF_COPY_VECTOR_DATA(trans_fail_gp_idx, 0, j, 0, avg->tx_error_codes[i + 1], 
                             trans_fail_local_gp_ptr->failures);
        GDF_COPY_SCALAR_DATA(trans_fail_gp_idx, 1, avg->tx_error_codes[i + 1], trans_fail_local_gp_ptr->failure_count);
        trans_fail_local_gp_ptr++;
      }
      for (i = TOTAL_URL_ERR; i < TOTAL_USED_PAGE_ERR; i++, j++)
      {
        trans_all_failures += avg->tx_error_codes[i];
        GDF_COPY_VECTOR_DATA(trans_fail_gp_idx, 0, j, 0, avg->tx_error_codes[i], trans_fail_local_gp_ptr->failures);
        GDF_COPY_SCALAR_DATA(trans_fail_gp_idx, 1, avg->tx_error_codes[i], trans_fail_local_gp_ptr->failure_count);
        trans_fail_local_gp_ptr++;
      } 
      for (i = TOTAL_PAGE_ERR; i < TOTAL_TX_ERR; i++, j++)
      {
        trans_all_failures += avg->tx_error_codes[i];
        GDF_COPY_VECTOR_DATA(trans_fail_gp_idx, 0, j, 0, avg->tx_error_codes[i], 
                             trans_fail_local_gp_ptr->failures);
        GDF_COPY_SCALAR_DATA(trans_fail_gp_idx, 1, avg->tx_error_codes[i], trans_fail_local_gp_ptr->failure_count);
        trans_fail_local_gp_ptr++;
      }
      /*bug id 101742: using method convert_long_pm() */
      NSDL2_GDF(NULL, NULL, "Transactions Failures/Minute trans_all_failures = %lu ", trans_all_failures);
      NSDL2_GDF(NULL, NULL, "Transactions Failures/Minute after PM trans_all_failures = %lu ", convert_long_pm(trans_all_failures));

      GDF_COPY_VECTOR_DATA(trans_fail_gp_idx, 0, all_idx, 0, convert_long_pm(trans_all_failures), trans_fail_local_first_gp_ptr->failures);
      GDF_COPY_SCALAR_DATA(trans_fail_gp_idx, 1, trans_all_failures, trans_fail_local_first_gp_ptr->failure_count);
      trans_all_failures = 0;
    }
  }
}

static inline void log_trans_fail_gp ()
{
  if(trans_fail_gp_ptr == NULL) return;

  int i;
  NSDL2_GDF(NULL, NULL, "Method called");
  fprintf(gui_fp,"\nTransaction Failures: ");
  // if(trans_fail_gp_ptr->failures) // Since we are printing only non Zero element 
    fprintf(gui_fp, "Failed Transactions (All Errors)/Minute=%6.3f\n", convert_pm(trans_fail_gp_ptr->failures));
  for (i = 0; i < (TOTAL_TX_ERR - 1); i++)
  {
    if (trans_fail_gp_ptr->failures) // Since we are printing only non Zero element
      fprintf(gui_fp, "Failed Transactions/Minute[%s (%d)]=%6.3f, ", get_error_code_name(i), i, convert_pm(trans_fail_gp_ptr->failures));
  }
  fprintf(gui_fp, "\n");
}

#define COPY_TIMES_DATA_IN_RTG_DATA_BUFF(Times_data_obj, rtgDataBuffer, rtgDataBufferCurrLoc)                      \
{                                                                                                                          \
  *rtgDataBufferCurrLoc += sprintf(rtgDataBuffer + *rtgDataBufferCurrLoc, "%.3lf,%.3lf,%.3lf,%.3lf,",                      \
                                  Times_data_obj.avg_time, Times_data_obj.min_time, Times_data_obj.max_time,               \
                                  Times_data_obj.succ);                                                                    \
}
#define COPY_TIMES_STD_DATA_IN_RTG_DATA_BUFF(Times_std_data_obj, rtgDataBuffer, rtgDataBufferCurrLoc)                      \
{                                                                                                                          \
  *rtgDataBufferCurrLoc += sprintf(rtgDataBuffer + *rtgDataBufferCurrLoc, "%.3lf,%.3lf,%.3lf,%.3lf,%.3lf,",                \
                                  Times_std_data_obj.avg_time, Times_std_data_obj.min_time, Times_std_data_obj.max_time,   \
                                  Times_std_data_obj.succ, Times_std_data_obj.sum_of_sqr);                                 \
}

#define COPY_DATA_IN_RTG_DATA_BUFF(data, rtgDataBuffer, rtgDataBufferCurrLoc)                      \
{                                                                                                   \
  *rtgDataBufferCurrLoc += sprintf(rtgDataBuffer + *rtgDataBufferCurrLoc, "%.3lf,", data);          \
}


//the data.sample is stored as multiple as 100 in rtg so divide by 100 to get actual pct.
#define COPY_SAMPLE_COUNT_DATA_IN_RTG_DATA_BUFF(data, rtgDataBuffer, rtgDataBufferCurrLoc)                      \
{                                                                                                   \
  *rtgDataBufferCurrLoc += sprintf(rtgDataBuffer + *rtgDataBufferCurrLoc, "%f,", (double)(data.sample/100.0));          \
}

void copy_trans_stats_gp_local_ptr_in_rtgDataBuffer(Trans_stats_gp *trans_stats_gp_local_ptr, char *rtgDataBuffer, 
                                                    int *rtgDataBufferCurrLoc)
{
  COPY_TIMES_STD_DATA_IN_RTG_DATA_BUFF(trans_stats_gp_local_ptr->time, rtgDataBuffer, rtgDataBufferCurrLoc);
  COPY_DATA_IN_RTG_DATA_BUFF(trans_stats_gp_local_ptr->completed_ps, rtgDataBuffer, rtgDataBufferCurrLoc); 
  COPY_DATA_IN_RTG_DATA_BUFF(trans_stats_gp_local_ptr->netcache_pct, rtgDataBuffer, rtgDataBufferCurrLoc); 
  COPY_SAMPLE_COUNT_DATA_IN_RTG_DATA_BUFF(trans_stats_gp_local_ptr->failures_pct, rtgDataBuffer, rtgDataBufferCurrLoc); 
  COPY_TIMES_STD_DATA_IN_RTG_DATA_BUFF(trans_stats_gp_local_ptr->succ_time, rtgDataBuffer, rtgDataBufferCurrLoc);
  COPY_TIMES_STD_DATA_IN_RTG_DATA_BUFF(trans_stats_gp_local_ptr->fail_time, rtgDataBuffer, rtgDataBufferCurrLoc);
  COPY_TIMES_STD_DATA_IN_RTG_DATA_BUFF(trans_stats_gp_local_ptr->netcache_hits_time, rtgDataBuffer, rtgDataBufferCurrLoc);
  COPY_TIMES_STD_DATA_IN_RTG_DATA_BUFF(trans_stats_gp_local_ptr->netcache_miss_time, rtgDataBuffer, rtgDataBufferCurrLoc);
  COPY_TIMES_DATA_IN_RTG_DATA_BUFF(trans_stats_gp_local_ptr->think_time, rtgDataBuffer, rtgDataBufferCurrLoc);

  /*LINE COMPLETED*/
  if(rtgDataMsgQueueAndTablesInfo_obj.writeInCsvOrDb & (1 << DB_WRITE_MODE))                                                  
    sprintf(rtgDataBuffer + *rtgDataBufferCurrLoc - 1, ")");                                                                 
  else if(rtgDataMsgQueueAndTablesInfo_obj.writeInCsvOrDb & (1 << CSV_WRITE_MODE))                                                   sprintf(rtgDataBuffer + *rtgDataBufferCurrLoc - 1, "\n");                                                                 
}

#define MAX_BYTES_FOR_DOUBLE 20

char *get_rtgDataBuffer(int sgrp_used_genrator_entries, int total_tx_entries, rtgDataMsgInfo **rtgDataMsgInfo_ptr)
{
  //TODO is it enough??
  //Comment
  int total_generators = sgrp_used_genrator_entries ? sgrp_used_genrator_entries+1 : 1;

  int rtgDataBufferMallocSize = (total_generators * total_tx_entries * (sizeof(Trans_stats_gp)/sizeof(double)) * 
                                 MAX_BYTES_FOR_DOUBLE) * 2;
 
  *rtgDataMsgInfo_ptr = nslb_mp_get_slot(rtgDataMsgQueueAndTablesInfo_obj.rtgDataMsgQueueInfo_obj.rtgDataMsgMpoolInfo);
  
  //msgQueue is not full
  if(*rtgDataMsgInfo_ptr)
  {
    NSDL1_GDF(NULL, NULL, "get slot from queue");

    //TODO csv_fd exist and its size 
    if((*rtgDataMsgInfo_ptr)->msgMallocSize < rtgDataBufferMallocSize);
    {
      MY_REALLOC((*rtgDataMsgInfo_ptr)->msg, rtgDataBufferMallocSize, "msg_data_ptr", -1);
      (*rtgDataMsgInfo_ptr)->msgMallocSize = rtgDataBufferMallocSize;
    }
    return (*rtgDataMsgInfo_ptr)->msg;
  }
  else/*msgQueue is full write buff in csv*/
  {
    NSDL1_GDF(NULL, NULL, "msgQueue is full");

    if(rtgDataMsgQueueAndTablesInfo_obj.rtgDataBufferMallocSize < rtgDataBufferMallocSize)
    {
      MY_REALLOC(rtgDataMsgQueueAndTablesInfo_obj.rtgDataBuffer, rtgDataBufferMallocSize, "msg_data_ptr", -1);
      rtgDataMsgQueueAndTablesInfo_obj.rtgDataBufferMallocSize = rtgDataBufferMallocSize;
  
      /*if(!rtgDataMsgQueueAndTablesInfo_obj.csv_fd)
      {
        rtgDataMsgQueueAndTablesInfo_obj.csv_fd = open("/tmp/1", O_CREAT|O_RDWR, 0666);
        if(rtgDataMsgQueueAndTablesInfo_obj.csv_fd == -1) 
          NSDL1_GDF(NULL, NULL, "ERROR[%s] in opening file", strerror(errno));
      }*/
    }
    return rtgDataMsgQueueAndTablesInfo_obj.rtgDataBuffer;
  }
}

void read_fd_and_append_data_into_rtgDataMsgInfo_ptr(rtgDataMsgInfo *rtgDataMsgInfo_ptr, int rtgDataBufferCurrLoc)
{
  struct stat fstat;
  
  NSDL1_GDF(NULL, NULL, "Function called");

  if((!stat(rtgDataMsgQueueAndTablesInfo_obj.csvFileNameWithPath, &fstat)) && fstat.st_size)
  {
    if(rtgDataMsgInfo_ptr ->msgMallocSize < rtgDataBufferCurrLoc + fstat.st_size + 1)
    {
      MY_REALLOC(rtgDataMsgInfo_ptr->msg, rtgDataBufferCurrLoc + fstat.st_size + 1, "msg_data_ptr", -1);
      rtgDataMsgInfo_ptr->msgMallocSize = rtgDataBufferCurrLoc + fstat.st_size + 1;
    }
    
    if(lseek(rtgDataMsgQueueAndTablesInfo_obj.csv_fd, 0, SEEK_SET) == -1)
    {
      NSDL1_GDF(NULL, NULL, "ERROR[%s] in lseek of file[%s]", strerror(errno), 
                rtgDataMsgQueueAndTablesInfo_obj.csvFileNameWithPath);
    }
    
    rtgDataBufferCurrLoc += sprintf(rtgDataMsgInfo_ptr->msg + rtgDataBufferCurrLoc, ",");

    int bytes_read = read(rtgDataMsgQueueAndTablesInfo_obj.csv_fd, rtgDataMsgInfo_ptr->msg + rtgDataBufferCurrLoc, 
                          fstat.st_size);
  
    if(bytes_read == -1)
    {
      //TODO remove ,
      NSDL1_GDF(NULL, NULL, "ERROR[%s] in reading from file[%s]", strerror(errno), 
                rtgDataMsgQueueAndTablesInfo_obj.csvFileNameWithPath);
    }
    else
    {
      NSDL1_GDF(NULL, NULL, "bytes_read[%d]", bytes_read);

      rtgDataMsgInfo_ptr->msg[rtgDataBufferCurrLoc + bytes_read - 1 ] = '\0';
     
      if(ftruncate(rtgDataMsgQueueAndTablesInfo_obj.csv_fd, 0))
      {
        NSDL1_GDF(NULL, NULL, "ERROR[%s] in ftruncate file[%s]", strerror(errno), 
                  rtgDataMsgQueueAndTablesInfo_obj.csvFileNameWithPath);
      }

      if(lseek(rtgDataMsgQueueAndTablesInfo_obj.csv_fd, 0, SEEK_SET) == -1)
      {
        NSDL1_GDF(NULL, NULL, "ERROR[%s] in ftruncate file[%s]", strerror(errno), 
                  rtgDataMsgQueueAndTablesInfo_obj.csvFileNameWithPath);
      }
    }
  }
  else
  {
    NSDL1_GDF(NULL, NULL, "file[%s] doesnt exist or size[%d] is 0", rtgDataMsgQueueAndTablesInfo_obj.csvFileNameWithPath, 
              fstat.st_size);
  }
}

void perform_sem_post_or_write_in_csv(rtgDataMsgInfo *rtgDataMsgInfo_ptr, char *rtgDataBuffer, int rtgDataBufferCurrLoc)
{
  char errorMsg[1024];
 
  if(rtgDataMsgInfo_ptr)
  {
    //TODO check if file is present and data is there then append file data also*/
    if(rtgDataMsgQueueAndTablesInfo_obj.writeInCsvOrDb & (1 << DB_WRITE_MODE))
      if(rtgDataMsgQueueAndTablesInfo_obj.csv_fd)
        read_fd_and_append_data_into_rtgDataMsgInfo_ptr(rtgDataMsgInfo_ptr, rtgDataBufferCurrLoc);

    rtgDataMsgInfo_ptr->msgType = TRANSACTION_STATS;

    if(sem_post(&(rtgDataMsgQueueAndTablesInfo_obj.mutex)) == -1)
    { //TODO
      NSDL1_GDF(NULL, NULL, "ERROR[%s] in sem_post", strerror(errno));
    }
    NSDL1_GDF(NULL, NULL, "writing buf[%s] in Queue", rtgDataMsgInfo_ptr->msg);
  }
  else
  {
    if(write_rtg_data_in_csv(rtgDataBuffer, rtgDataBufferCurrLoc, errorMsg, 1024))
      NSDL1_GDF(NULL, NULL, "ERROR[%s] in writing in file", errorMsg);
      
    NSDL1_GDF(NULL, NULL, "writing buf[%s] in file", rtgDataBuffer);
    //writeincsv
  }
}

//TODO check
#ifndef CAV_MAIN
extern NormObjKey normRuntimeTXTable;
#else
extern __thread NormObjKey normRuntimeTXTable;
#endif

//#define WRITE_RTG_DATA_IN_CSV_OR_DB(rtgDataBuffer, rtgDataBufferCurrLoc, i, gv_idx, generator_entry, utcTime)            
#define WRITE_RTG_DATA_IN_CSV_OR_DB()                                                                                       \
{                                                                                                                           \
  if(rtgDataMsgQueueAndTablesInfo_obj.writeInCsvOrDb & (1 << DB_WRITE_MODE))                                                \
  {                                                                                                                         \
    rtgDataBufferCurrLoc += sprintf(rtgDataBuffer + rtgDataBufferCurrLoc, i+gv_idx?",('%s','%s',%ld,":"('%s','%s',%ld,",    \
                                    generator_entry?(gv_idx?generator_entry[gv_idx-1].gen_name:"Overall"):"-",              \
                                    nslb_get_norm_table_data(&normRuntimeTXTable, i),                                       \
                                    utcTime);                                                                               \
  }                                                                                                                         \
  else if(rtgDataMsgQueueAndTablesInfo_obj.writeInCsvOrDb & (1 << CSV_WRITE_MODE))                                          \
  {                                                                                                                         \
    rtgDataBufferCurrLoc += sprintf(rtgDataBuffer + rtgDataBufferCurrLoc, "%s,%s,%ld,",                                     \
                                    generator_entry?(gv_idx?generator_entry[gv_idx-1].gen_name:"Overall"):"-",              \
                                    nslb_get_norm_table_data(&normRuntimeTXTable, i),                                       \
                                    utcTime);                                                                               \
  }                                                                                                                         \
}                                                                                                                           

// This fn is filling the Trans_overall_gp, Trans_fail_gp, Trans_time_gp, Trans_completed_gp and Trans_success_gp
static inline void fill_all_trans_data(avgtime **g_avg, cavgtime **g_cavg)
{
  int i, gv_idx;
  long num_samples = 0; // Taking long as it is periodic
  avgtime *avg = NULL;
  cavgtime *cavg = NULL;
  TxDataSample *txData;
  TxDataCum *txCData;
  int j = 0;

  /*rtgBuffer vars*/
  rtgDataMsgInfo * rtgDataMsgInfo_ptr = NULL;
  char *rtgDataBuffer = NULL;
  int rtgDataBufferCurrLoc = 0;
  long utcTime;

  NSDL2_GDF(NULL, NULL, "Method called");

  fill_trans_overall_gp(g_avg, g_cavg);        // Trans_overall_gp
  fill_trans_fail_gp(g_avg);                   // Trans_fail_gp

  Trans_stats_gp *trans_stats_gp_local_ptr;
  Trans_cum_stats_gp *trans_cum_stats_gp_local_ptr;

  /*get rtgDataBuffer in which data is to buffered*/
  if(global_settings->write_rtg_data_in_db_or_csv)
  {
    rtgDataBuffer = get_rtgDataBuffer(sgrp_used_genrator_entries, total_tx_entries, &rtgDataMsgInfo_ptr);
    utcTime = time(NULL);
  }

  for(gv_idx = 0; gv_idx < sgrp_used_genrator_entries + 1; gv_idx++)
  {

    avg = (avgtime *)g_avg[gv_idx];
    txData = (TxDataSample*)((char *)avg + g_trans_avgtime_idx); 
    // No need to do memset as we are filling always
    for(i = 0; i < total_tx_entries; i++)
    {
      if(global_settings->write_rtg_data_in_db_or_csv)
        WRITE_RTG_DATA_IN_CSV_OR_DB();
 
      NSDL3_GDF(NULL, NULL, "DynObjForGdf info: TxIdx = %d, dynObjForGdf[1].startId=%d, dynObjForGdf[1].total=%d, dynObjForGdf[1].rtg_index_tbl[gv_idx][i] = %d, txData[i].tx_succ_fetches = %d, txData[i].tx_fetches_completed = %d, global_settings->exclude_failed_agg = %d", i, dynObjForGdf[NEW_OBJECT_DISCOVERY_TX].startId, dynObjForGdf[NEW_OBJECT_DISCOVERY_TX].total, dynObjForGdf[NEW_OBJECT_DISCOVERY_TX].rtg_index_tbl[gv_idx][i], txData[i].tx_succ_fetches, txData[i].tx_fetches_completed, global_settings->exclude_failed_agg);
      if(dynObjForGdf[NEW_OBJECT_DISCOVERY_TX].rtg_index_tbl[gv_idx][i] < 0)
        continue;

      trans_stats_gp_local_ptr = (Trans_stats_gp *)((char *)msg_data_ptr + dynObjForGdf[NEW_OBJECT_DISCOVERY_TX].rtg_index_tbl[gv_idx][i]);
      //num_samples = global_settings->exclude_failed_agg?txData[i].tx_succ_fetches:txData[i].tx_fetches_completed;
      //Independent of Keyword
      num_samples = txData[i].tx_fetches_completed;

      if(num_samples > 0) // Due to some issue, it was coming -ve to changed to > 0 check
      {
        NSDL4_GDF(NULL, NULL, "txData[%d].tx_tot_sqr_time = %d ms sqr, %.f sec sqr", i, (txData[i].tx_tot_sqr_time), NS_MS_SQR_TO_SEC_SQR(txData[i].tx_tot_sqr_time));

        // We need to send this data in milli-secs as percentile data is also in milli-sec
        GDF_COPY_TIMES_STD_VECTOR_DATA(trans_stats_gp_idx, 0, j, 0, 
                                     (((double )txData[i].tx_tot_time/(double )num_samples)),
                                     (txData[i].tx_min_time), 
                                     (txData[i].tx_max_time), 
                                     (double )num_samples,
                                     (txData[i].tx_tot_sqr_time),
                                     trans_stats_gp_local_ptr->time.avg_time,
                                     trans_stats_gp_local_ptr->time.min_time,
                                     trans_stats_gp_local_ptr->time.max_time,
                                     trans_stats_gp_local_ptr->time.succ,
                                     trans_stats_gp_local_ptr->time.sum_of_sqr);

        NSDL4_GDF(NULL, NULL, "gv_idx = %d, TxIdx = %d, name = %s, num_samples = %ld, "
                              "tx_tot_time = %llu ms (avg_time = %.3f sec), "
                              "tx_min_time = %u   ms (tx_min_time = %.3f sec), "
                              "tx_max_time = %u   ms (tx_max_time = %.3f sec), "
                              "tx_tot_sqr_time = %llu ms (tx_tot_sqr_time = %.3f sec)\n",
                              gv_idx, i, nslb_get_norm_table_data(dynObjForGdf[NEW_OBJECT_DISCOVERY_TX].normTable, i), num_samples,
                              txData[i].tx_tot_time, trans_stats_gp_local_ptr->time.avg_time,
                              txData[i].tx_min_time, trans_stats_gp_local_ptr->time.min_time,
                              txData[i].tx_max_time, trans_stats_gp_local_ptr->time.max_time,
                              txData[i].tx_tot_sqr_time, trans_stats_gp_local_ptr->time.sum_of_sqr);
      }
      else
      {
        GDF_COPY_TIMES_STD_VECTOR_DATA(trans_stats_gp_idx, 0, j, 0, 
                                       0,
                                       -1, // Must fill -1 to indicate the min is not valid
                                       -1, // Must fill -1 to indicate the max is not valid
                                       0,
                                       0,
                                       trans_stats_gp_local_ptr->time.avg_time,
                                       trans_stats_gp_local_ptr->time.min_time,
                                       trans_stats_gp_local_ptr->time.max_time,
                                       trans_stats_gp_local_ptr->time.succ,
                                       trans_stats_gp_local_ptr->time.sum_of_sqr);
        NSDL4_GDF(NULL, NULL, "gv_idx = %d, TxIdx = %d, name = %s, num_samples = %ld\n", gv_idx, i, nslb_get_norm_table_data(dynObjForGdf[NEW_OBJECT_DISCOVERY_TX].normTable, i), num_samples);
      }

      GDF_COPY_VECTOR_DATA(trans_stats_gp_idx, 1, j, 0,
                           NS_NUM_TO_RATE_PS(txData[i].tx_fetches_completed),
                           trans_stats_gp_local_ptr->completed_ps);

      // New graph for netcache pct is enabled only if network cache stats are enabled
      //Netcache graph must be last graph
      if(global_settings->protocol_enabled & NETWORK_CACHE_STATS_ENABLED)
      {
        // Calculate netcache hit pct. Since netacahe hits are ended with diferent tx, tx_fetches_completed also get increamented in that tx
        // So we need to add tx_netcache_fetches also to find the pct
        if (global_settings->protocol_enabled & TX_END_NETCACHE_ENABLED)
        {
          GDF_COPY_VECTOR_DATA(trans_stats_gp_idx, 2, j, 0, 
                               (double)(((double)(txData[i].tx_netcache_fetches * 100.00))/(double )(txData[i].tx_fetches_completed + txData[i].tx_netcache_fetches)),
                               trans_stats_gp_local_ptr->netcache_pct);
        }
        else 
        {
          GDF_COPY_VECTOR_DATA(trans_stats_gp_idx, 2, j, 0, 
                               (double)(((double)(txData[i].tx_netcache_fetches * 100.00))/(double )(txData[i].tx_fetches_completed)),
                               trans_stats_gp_local_ptr->netcache_pct);
        }
      }

      NSDL4_GDF(NULL, NULL, "TxIdx = %d, name = %s, tx_fetches_completed = %u (tx_fetches_completed/Sec = %.3f), "
                            "tx_netcache_fetches = %u, (tx_netcache_pct = %.3f)",   
                            i, nslb_get_norm_table_data(dynObjForGdf[NEW_OBJECT_DISCOVERY_TX].normTable, i), 
                            txData[i].tx_fetches_completed, trans_stats_gp_local_ptr->completed_ps,
                            txData[i].tx_netcache_fetches, trans_stats_gp_local_ptr->netcache_pct);
 
      /*Sample graph of transaction failure pct*/
      /*GDF_COPY_VECTOR_DATA(trans_stats_gp_idx, 3, j, 0, 
      (txData[i].tx_fetches_completed == 0)?0:(double)(((double)((txData[i].tx_fetches_completed - txData[i].tx_succ_fetches) * 100.00))/(double )(txData[i].tx_fetches_completed)),
      trans_stats_gp_local_ptr->failures_pct);*/
      /*bug 103688 START ******************************************************************/
       NS_COPY_SAMPLE_2B_100_COUNT_4B_DATA((txData[i].tx_fetches_completed == 0)?0:(double)(((double)((txData[i].tx_fetches_completed - txData[i].tx_succ_fetches) * 100.00))/(double )(txData[i].tx_fetches_completed)), txData[i].tx_fetches_completed, trans_stats_gp_idx, 3, j, 0, trans_stats_gp_local_ptr->failures_pct.sample, trans_stats_gp_local_ptr->failures_pct.count)
      /******* bug 103688 END  *********************************************************/
     
 
      NSDL4_GDF(NULL, NULL, "TxIdx = %d, name = %s, tx_fetches_completed = %u (tx_fetches_completed/Sec = %.3f), "
                            "tx_succ_fetches = %u sec "
                            "tx_failures = %u, (tx_failures/Sec = %d, %d) ",
                            i, nslb_get_norm_table_data(dynObjForGdf[NEW_OBJECT_DISCOVERY_TX].normTable, i), 
                            txData[i].tx_fetches_completed, trans_stats_gp_local_ptr->completed_ps, txData[i].tx_succ_fetches,
                            (txData[i].tx_fetches_completed - txData[i].tx_succ_fetches), trans_stats_gp_local_ptr->failures_pct.sample, trans_stats_gp_local_ptr->failures_pct.count);

      /*TX Time graphs for transaction successful graphs*/  
      if(txData[i].tx_succ_fetches > 0)
      {
        GDF_COPY_TIMES_STD_VECTOR_DATA(trans_stats_gp_idx, 4, j, 0, 
                                       (((double )txData[i].tx_succ_tot_time/(double )txData[i].tx_succ_fetches)),
                                       (txData[i].tx_succ_min_time), 
                                       (txData[i].tx_succ_max_time), 
                                       (double )txData[i].tx_succ_fetches,
                                       (txData[i].tx_succ_tot_sqr_time),
                                       trans_stats_gp_local_ptr->succ_time.avg_time,
                                       trans_stats_gp_local_ptr->succ_time.min_time,
                                       trans_stats_gp_local_ptr->succ_time.max_time,
                                       trans_stats_gp_local_ptr->succ_time.succ,
                                       trans_stats_gp_local_ptr->succ_time.sum_of_sqr);
        
         NSDL4_GDF(NULL, NULL, "gv_idx = %d, TxIdx = %d, name = %s, Success fetches = %ld, "
                               "tx_succ_tot_time = %u ms (succ_avg_time = %.3f sec), "
                               "tx_succ_min_time = %u   ms (gp_tx_succ_min_time = %.3f sec), "
                               "tx_succ_max_time) = %u   ms (gp_tx_succ_max_time) = %.3f sec), "
                               "tx_succ_tot_sqr_time = %llu ms (gp_tx_succ_tot_sqr_time = %.3f sec)\n",
                               gv_idx, i, nslb_get_norm_table_data(dynObjForGdf[NEW_OBJECT_DISCOVERY_TX].normTable, i), txData[i].tx_succ_fetches,
                               txData[i].tx_succ_tot_time, trans_stats_gp_local_ptr->succ_time.avg_time,
                               txData[i].tx_succ_min_time, trans_stats_gp_local_ptr->succ_time.min_time,
                               txData[i].tx_succ_max_time, trans_stats_gp_local_ptr->succ_time.max_time,
                               txData[i].tx_succ_tot_sqr_time, trans_stats_gp_local_ptr->succ_time.sum_of_sqr);
      }
      else
      {
        GDF_COPY_TIMES_STD_VECTOR_DATA(trans_stats_gp_idx, 4, j, 0,
                                       0,
                                       -1, // Must fill -1 to indicate the min is not valid
                                       -1, // Must fill -1 to indicate the max is not valid
                                       0,
                                       0,
                                       trans_stats_gp_local_ptr->succ_time.avg_time,
                                       trans_stats_gp_local_ptr->succ_time.min_time,
                                       trans_stats_gp_local_ptr->succ_time.max_time,
                                       trans_stats_gp_local_ptr->succ_time.succ,
                                       trans_stats_gp_local_ptr->succ_time.sum_of_sqr);
        NSDL4_GDF(NULL, NULL, "gv_idx = %d, TxIdx = %d, name = %s, success_fetches = %ld\n", gv_idx, i,
                               nslb_get_norm_table_data(dynObjForGdf[NEW_OBJECT_DISCOVERY_TX].normTable, i), txData[i].tx_succ_fetches);
      }

      
     /*TX Time graphs for failure transactions time graphs*/  
      if((txData[i].tx_fetches_completed - txData[i].tx_succ_fetches) > 0)
      {
        GDF_COPY_TIMES_STD_VECTOR_DATA(trans_stats_gp_idx, 5, j, 0, 
                              (((double )txData[i].tx_failure_tot_time/(double )(txData[i].tx_fetches_completed - txData[i].tx_succ_fetches))),
                              (txData[i].tx_failure_min_time), 
                              (txData[i].tx_failure_max_time), 
                              (double )(txData[i].tx_fetches_completed - txData[i].tx_succ_fetches),
                              (txData[i].tx_failure_tot_sqr_time),
                              trans_stats_gp_local_ptr->fail_time.avg_time,
                              trans_stats_gp_local_ptr->fail_time.min_time,
                              trans_stats_gp_local_ptr->fail_time.max_time,
                              trans_stats_gp_local_ptr->fail_time.succ,
                              trans_stats_gp_local_ptr->fail_time.sum_of_sqr);

        NSDL4_GDF(NULL, NULL, "**** gv_idx = %d, TxIdx = %d, name = %s, failures = %ld, "
                              "tx_failure_tot_time = %.3f ms (gp_tx_failure_tot_time = %.3f sec), "
                              "tx_failure_min_time = %u   ms (tx_failure_min_time = %.3f sec), "
                              "tx_failure_max_time = %u   ms (tx_failure_max_time = %.3f sec), "
                              "tx_failure_tot_sqr_time = %llu ms (tx_failure_tot_sqr_time = %.3f sec)\n",
                              gv_idx, i, nslb_get_norm_table_data(dynObjForGdf[NEW_OBJECT_DISCOVERY_TX].normTable, i), (txData[i].tx_fetches_completed - txData[i].tx_succ_fetches),
                              txData[i].tx_failure_tot_time,
                              trans_stats_gp_local_ptr->fail_time.avg_time,
                              txData[i].tx_failure_min_time, trans_stats_gp_local_ptr->fail_time.min_time,
                              txData[i].tx_failure_max_time, trans_stats_gp_local_ptr->fail_time.max_time,
                              txData[i].tx_failure_tot_sqr_time, trans_stats_gp_local_ptr->fail_time.sum_of_sqr);
      }
      else
      {
        GDF_COPY_TIMES_STD_VECTOR_DATA(trans_stats_gp_idx, 5, j, 0,
                                       0,
                                       -1, // Must fill -1 to indicate the min is not valid
                                       -1, // Must fill -1 to indicate the max is not valid
                                       0,
                                       0,
                                       trans_stats_gp_local_ptr->fail_time.avg_time,
                                       trans_stats_gp_local_ptr->fail_time.min_time,
                                       trans_stats_gp_local_ptr->fail_time.max_time,
                                       trans_stats_gp_local_ptr->fail_time.succ,
                                       trans_stats_gp_local_ptr->fail_time.sum_of_sqr);
        NSDL4_GDF(NULL, NULL, "gv_idx = %d, TxIdx = %d, name = %s, failures = %ld\n", gv_idx, i,
                               nslb_get_norm_table_data(dynObjForGdf[NEW_OBJECT_DISCOVERY_TX].normTable, i), (txData[i].tx_fetches_completed - txData[i].tx_succ_fetches));
      }


      if(txData[i].tx_netcache_fetches > 0)
      {
        GDF_COPY_TIMES_STD_VECTOR_DATA(trans_stats_gp_idx, 6, j, 0, 
                                       (((double )txData[i].tx_netcache_hit_tot_time/(double )txData[i].tx_netcache_fetches)),
                                       (txData[i].tx_netcache_hit_min_time), 
                                       (txData[i].tx_netcache_hit_max_time), 
                                       (double )txData[i].tx_netcache_fetches,
                                       (txData[i].tx_netcache_hit_tot_sqr_time),
                                       trans_stats_gp_local_ptr->netcache_hits_time.avg_time,
                                       trans_stats_gp_local_ptr->netcache_hits_time.min_time,
                                       trans_stats_gp_local_ptr->netcache_hits_time.max_time,
                                       trans_stats_gp_local_ptr->netcache_hits_time.succ,
                                       trans_stats_gp_local_ptr->netcache_hits_time.sum_of_sqr);
        
         NSDL4_GDF(NULL, NULL, "gv_idx = %d, TxIdx = %d, name = %s, netcache hits succ = %ld, "
                               "tx_netcache_hit_tot_time = %u ms (avg_time = %.3f sec), "
                               "tx_netcache_hit_min_time = %u   ms (gp_tx_netcache_hit_min_time = %.3f sec), "
                               "tx_netcache_hit_max_time = %u   ms (gp_tx_netcache_hit_max_time = %.3f sec), "
                               "tx_netcache_hit_tot_sqr_time = %llu ms (gp_tx_netcache_hit_tot_sqr_time = %.3f sec)\n",
                               gv_idx, i, nslb_get_norm_table_data(dynObjForGdf[NEW_OBJECT_DISCOVERY_TX].normTable, i), txData[i].tx_netcache_fetches,
                               txData[i].tx_netcache_hit_tot_time, trans_stats_gp_local_ptr->netcache_hits_time.avg_time,
                               txData[i].tx_netcache_hit_min_time, trans_stats_gp_local_ptr->netcache_hits_time.min_time,
                               txData[i].tx_netcache_hit_max_time, trans_stats_gp_local_ptr->netcache_hits_time.max_time,
                               txData[i].tx_netcache_hit_tot_sqr_time, trans_stats_gp_local_ptr->netcache_hits_time.sum_of_sqr);
      }
      else
      {
        GDF_COPY_TIMES_STD_VECTOR_DATA(trans_stats_gp_idx, 6, j, 0,
                                       0,
                                       -1, // Must fill -1 to indicate the min is not valid
                                       -1, // Must fill -1 to indicate the max is not valid
                                       0,
                                       0,
                                       trans_stats_gp_local_ptr->netcache_hits_time.avg_time,
                                       trans_stats_gp_local_ptr->netcache_hits_time.min_time,
                                       trans_stats_gp_local_ptr->netcache_hits_time.max_time,
                                       trans_stats_gp_local_ptr->netcache_hits_time.succ,
                                       trans_stats_gp_local_ptr->netcache_hits_time.sum_of_sqr);
        NSDL4_GDF(NULL, NULL, "gv_idx = %d, TxIdx = %d, name = %s, tx_netcache_fetches = %ld\n", gv_idx, i,
                               nslb_get_norm_table_data(dynObjForGdf[NEW_OBJECT_DISCOVERY_TX].normTable, i), txData[i].tx_netcache_fetches);
      }

      if((txData[i].tx_fetches_completed - txData[i].tx_netcache_fetches) > 0)
      {
        GDF_COPY_TIMES_STD_VECTOR_DATA(trans_stats_gp_idx, 7, j, 0, 
                  (((double )txData[i].tx_netcache_miss_tot_time/(double )(txData[i].tx_fetches_completed - txData[i].tx_netcache_fetches))),
                  (txData[i].tx_netcache_miss_min_time), 
                  (txData[i].tx_netcache_miss_max_time), 
                  (double )(txData[i].tx_fetches_completed - txData[i].tx_netcache_fetches),
                  (txData[i].tx_netcache_miss_tot_sqr_time),
                  trans_stats_gp_local_ptr->netcache_miss_time.avg_time,
                  trans_stats_gp_local_ptr->netcache_miss_time.min_time,
                  trans_stats_gp_local_ptr->netcache_miss_time.max_time,
                  trans_stats_gp_local_ptr->netcache_miss_time.succ,
                  trans_stats_gp_local_ptr->netcache_miss_time.sum_of_sqr);
        
         NSDL4_GDF(NULL, NULL, "gv_idx = %d, TxIdx = %d, name = %s, tx_netcache_fetches = %ld, "
                               "tx_netcache_miss_tot_time = %u ms (gp_tx_netcache_miss_tot_time = %.3f sec), "
                               "tx_netcache_miss_min_time = %u   ms (gp_tx_netcache_miss_min_time = %.3f sec), "
                               "tx_netcache_miss_max_time = %u   ms (gp_tx_netcache_miss_max_time = %.3f sec), "
                               "tx_netcache_miss_tot_sqr_time = %llu ms (gp_tx_netcache_miss_tot_sqr_time = %.3f sec)\n",
                               gv_idx, i, nslb_get_norm_table_data(dynObjForGdf[NEW_OBJECT_DISCOVERY_TX].normTable, i), txData[i].tx_netcache_fetches,
                               txData[i].tx_netcache_miss_tot_time, trans_stats_gp_local_ptr->netcache_miss_time.avg_time,
                               txData[i].tx_netcache_miss_min_time, trans_stats_gp_local_ptr->netcache_miss_time.min_time,
                               txData[i].tx_netcache_miss_max_time, trans_stats_gp_local_ptr->netcache_miss_time.max_time,
                               txData[i].tx_netcache_miss_tot_sqr_time, trans_stats_gp_local_ptr->netcache_miss_time.sum_of_sqr);
      }
      else
      {
        GDF_COPY_TIMES_STD_VECTOR_DATA(trans_stats_gp_idx, 7, j, 0,
                                       0,
                                       -1, // Must fill -1 to indicate the min is not valid
                                       -1, // Must fill -1 to indicate the max is not valid
                                       0,
                                       0,
                                       trans_stats_gp_local_ptr->netcache_miss_time.avg_time,
                                       trans_stats_gp_local_ptr->netcache_miss_time.min_time,
                                       trans_stats_gp_local_ptr->netcache_miss_time.max_time,
                                       trans_stats_gp_local_ptr->netcache_miss_time.succ,
                                       trans_stats_gp_local_ptr->netcache_miss_time.sum_of_sqr);
        NSDL4_GDF(NULL, NULL, "gv_idx = %d, TxIdx = %d, name = %s, tx_netcache_fetches = %ld\n", gv_idx, i,
                               nslb_get_norm_table_data(dynObjForGdf[NEW_OBJECT_DISCOVERY_TX].normTable, i), txData[i].tx_netcache_fetches);
      }
      //Tx Stat Think Time
      if(num_samples > 0) 
      {
        // We need to send this data in milli-secs as percentile data is also in milli-sec
        GDF_COPY_TIMES_VECTOR_DATA(trans_stats_gp_idx, 8, j, 0, 
                                     (((double )txData[i].tx_tot_think_time/(double )num_samples)),
                                     (txData[i].tx_min_think_time), 
                                     (txData[i].tx_max_think_time), 
                                     (double )num_samples,
                                     trans_stats_gp_local_ptr->think_time.avg_time,
                                     trans_stats_gp_local_ptr->think_time.min_time,
                                     trans_stats_gp_local_ptr->think_time.max_time,
                                     trans_stats_gp_local_ptr->think_time.succ);

        NSDL4_GDF(NULL, NULL, "gv_idx = %d, TxIdx = %d, name = %s, num_samples = %ld, "
                              "tx_tot_think_time = %llu ms (avg_time = %.3f sec), "
                              "tx_min_think_time = %u   ms (min_time = %.3f sec), "
                              "tx_min_think_time = %u   ms (max_time = %.3f sec) \n",
                              gv_idx, i, nslb_get_norm_table_data(dynObjForGdf[NEW_OBJECT_DISCOVERY_TX].normTable, i), num_samples,
                              txData[i].tx_tot_think_time, trans_stats_gp_local_ptr->think_time.avg_time,
                              txData[i].tx_min_think_time, trans_stats_gp_local_ptr->think_time.min_time,
                              txData[i].tx_max_think_time, trans_stats_gp_local_ptr->think_time.max_time);
      }
      else
      {
        GDF_COPY_TIMES_VECTOR_DATA(trans_stats_gp_idx, 8, j, 0, 
                                       0,
                                       -1, // Must fill -1 to indicate the min is not valid
                                       0, // Must fill -1 to indicate the max is not valid
                                       0,
                                       trans_stats_gp_local_ptr->think_time.avg_time,
                                       trans_stats_gp_local_ptr->think_time.min_time,
                                       trans_stats_gp_local_ptr->think_time.max_time,
                                       trans_stats_gp_local_ptr->think_time.succ);
        NSDL4_GDF(NULL, NULL, "gv_idx = %d, TxIdx = %d, name = %s, num_samples = %ld\n", gv_idx, i, nslb_get_norm_table_data(dynObjForGdf[NEW_OBJECT_DISCOVERY_TX].normTable, i), num_samples);
      }
      GDF_COPY_VECTOR_DATA(trans_stats_gp_idx, 9, j, 0,
                           (txData[i].tx_tx_bytes * kbps_factor), trans_stats_gp_local_ptr->tx_tx_throughput);

      GDF_COPY_VECTOR_DATA(trans_stats_gp_idx, 10, j, 0,
                           (txData[i].tx_rx_bytes * kbps_factor), trans_stats_gp_local_ptr->tx_rx_throughput);

      NSDL2_GDF(NULL, NULL, "tx_fetches_completed = %llu, tx_succ_fetches = %llu, tx_tx_bytes = %llu,"
                            "tx_rx_bytes=%llu, kbps_factor = %f, send_throughput(kbps) = %f, recv_throughput(kbps) = %f",
                             txData[i].tx_fetches_completed, txData[i].tx_succ_fetches,
                             txData[i].tx_tx_bytes, txData[i].tx_rx_bytes, kbps_factor, (txData[i].tx_tx_bytes * kbps_factor), (txData[i].tx_rx_bytes * kbps_factor));

      j++;

      if(global_settings->write_rtg_data_in_db_or_csv)
        copy_trans_stats_gp_local_ptr_in_rtgDataBuffer(trans_stats_gp_local_ptr, rtgDataBuffer, &rtgDataBufferCurrLoc);
    }
  }

  if(rtgDataBuffer && global_settings->write_rtg_data_in_db_or_csv)
    perform_sem_post_or_write_in_csv(rtgDataMsgInfo_ptr, rtgDataBuffer, rtgDataBufferCurrLoc);

  if(global_settings->g_tx_cumulative_graph == 0) return;

  j = 0;

  for(gv_idx = 0; gv_idx < sgrp_used_genrator_entries + 1; gv_idx++)
  {
    avg = (avgtime *)g_avg[gv_idx]; 
    cavg = (cavgtime *)g_cavg[gv_idx]; 

    txCData = (TxDataCum*)((char *)cavg + g_trans_cavgtime_idx); 

    // No need to do memset as we are filling always
    for(i = 0; i < total_tx_entries; i++)
    {
      if(dynObjForGdf[NEW_OBJECT_DISCOVERY_TX_CUM].rtg_index_tbl[gv_idx][i] < 0)
        continue;

      trans_cum_stats_gp_local_ptr = (Trans_cum_stats_gp *)(msg_data_ptr + dynObjForGdf[NEW_OBJECT_DISCOVERY_TX_CUM].rtg_index_tbl[gv_idx][i]);  
  
      GDF_COPY_VECTOR_DATA(trans_cum_stats_gp_idx, 0, j, 0, txCData[i].tx_c_fetches_completed,
                           trans_cum_stats_gp_local_ptr->completed_cum);
      GDF_COPY_VECTOR_DATA(trans_cum_stats_gp_idx, 1, j, 0, txCData[i].tx_c_succ_fetches,
                           trans_cum_stats_gp_local_ptr->succ_cum);
      GDF_COPY_VECTOR_DATA(trans_cum_stats_gp_idx, 2, j, 0, (txCData[i].tx_c_fetches_completed - txCData[i].tx_c_succ_fetches),
                           trans_cum_stats_gp_local_ptr->failures_cum);
     
       NSDL4_GDF(NULL, NULL, "TxIdx = %d, name = %s, tx_c_fetches_completed = %llu (completed_cum = %.3f), "
                             "tx_c_succ_fetches = %llu (succ_cum = %.3f), "
                             "tx_c_failures = %llu (failures_cum = %.3f)",
                             i, nslb_get_norm_table_data(dynObjForGdf[NEW_OBJECT_DISCOVERY_TX].normTable, i),
                             txCData[i].tx_c_fetches_completed, trans_cum_stats_gp_local_ptr->completed_cum,
                             txCData[i].tx_c_succ_fetches, trans_cum_stats_gp_local_ptr->succ_cum,
                             (txCData[i].tx_c_fetches_completed - txCData[i].tx_c_succ_fetches),
                             trans_cum_stats_gp_local_ptr->failures_cum);
      j++;
    }
  }
}

//**************************************************************************
// It will log the trans stats data

#if 0
void log_trans_stats_gp()
{
  NSDL2_GDF(NULL, NULL, "Method called");
  int i;

  Trans_stats_gp *trans_stats_gp_local_ptr = trans_stats_gp_ptr;

  fprintf(gui_fp,"\nTransaction Time (Sec):\n");
  for(i = 0; i < total_tx_entries; i++)
  {
    // Not complete as log is not used
    if((trans_stats_gp_local_ptr->time.succ))
    {
      fprintf(gui_fp, "  Transaction Time - %s: Min=%6.3f, Max=%6.3f, Avg=%6.3f, Successful=%6.3f\n", //, Sum fo Squares Transaction (Sec)=%0.0f\n",
        nslb_get_norm_table_data(dynObjForGdf[NEW_OBJECT_DISCOVERY_TX].normTable, i), (trans_stats_gp_local_ptr->time.min_time),
        (trans_stats_gp_local_ptr->time.max_time), (trans_stats_gp_local_ptr->time.avg_time),
        (trans_stats_gp_local_ptr->time.succ));
      trans_stats_gp_local_ptr++;
        
    }
  }
}
#endif

void log_trans_cum_stats_gp()
{
  if(trans_cum_stats_gp_ptr == NULL) return;
  NSDL2_GDF(NULL, NULL, "Method called");
  int i;

  // Trans_cum_stats_gp *trans_cum_stats_gp_local_ptr = trans_cum_stats_gp_ptr;

  fprintf(gui_fp,"\nTransaction Time (Sec):\n");
  for(i = 0; i < total_tx_entries; i++)
  {
    // Not complete as log is not used
  }
}

//*************************************************************************


// It will log the trans completed data
#if 0
void log_trans_completed_gp()
{
  if(trans_completed_gp_ptr == NULL)  return;
  NSDL2_GDF(NULL, NULL, "Method called");
  int i;
  Trans_completed_gp *trans_completed_gp_local_ptr = trans_completed_gp_ptr;

  fprintf(gui_fp,"\nTransactions Completed/Sec:\n");
  for(i = 0; i < total_tx_entries; i++)
  {
    if((trans_completed_gp_local_ptr->completed))
    {
      fprintf(gui_fp, "  Transaction Completed/Sec - %s=%6.3f\n",
        nslb_get_norm_table_data(dynObjForGdf[NEW_OBJECT_DISCOVERY_TX].normTable, i), convert_ps(trans_completed_gp_local_ptr->completed));
      trans_completed_gp_local_ptr++;
    }
  }
  fprintf(gui_fp, "\n");
}


//It will log the trans  cumulative completed data
void log_trans_cum_completed_gp()
{
  if(trans_completed_cum_gp_ptr == NULL)  return;
  NSDL2_GDF(NULL, NULL, "Method called");
  int i;
  Trans_cum_completed_gp *trans_completed_cum_gp_local_ptr = trans_completed_cum_gp_ptr;
 
  fprintf(gui_fp,"\nCumulative Transactions Completed/Sec:\n");
  for(i = 0; i < total_tx_entries; i++)
  {
    if((trans_completed_cum_gp_local_ptr->completed_cum))
    {
      fprintf(gui_fp, "  Cumulative Transaction Completed/Sec - %s=%6.3f\n",
      nslb_get_norm_table_data(dynObjForGdf[NEW_OBJECT_DISCOVERY_TX].normTable, i), convert_ps(trans_completed_cum_gp_local_ptr->completed_cum));
      trans_completed_cum_gp_local_ptr++;
    }
  }
  fprintf(gui_fp, "\n");
}

  

// It will log the trans sucess data
void log_trans_success_gp()
{
  if(trans_success_gp_ptr == NULL)  return;
  NSDL2_GDF(NULL, NULL, "Method called");
  int i;
  Trans_success_gp *trans_success_gp_local_ptr = trans_success_gp_ptr;

  fprintf(gui_fp,"\nTransaction Successful/Sec: \n");
  for(i = 0; i < total_tx_entries; i++)
  {
    if((trans_success_gp_local_ptr->succ))
    {
      fprintf(gui_fp, "  Transaction Successful/Sec - %s=%6.3f\n",
        nslb_get_norm_table_data(dynObjForGdf[NEW_OBJECT_DISCOVERY_TX].normTable, i), convert_ps(trans_success_gp_local_ptr->succ));
      trans_success_gp_local_ptr++;
    }
  }
  fprintf(gui_fp, "\n");
}
// It will log the Cumulative  Transaction success data
void log_trans_cum_success_gp()
{
  if(trans_cum_success_gp_ptr == NULL)  return;
  NSDL2_GDF(NULL, NULL, "Method called");
  int i;
  Trans_cum_success_gp *trans_cum_success_gp_local_ptr = trans_cum_success_gp_ptr;

  fprintf(gui_fp,"\nCumulative Transaction Successful/Sec: \n");
  for(i = 0; i < total_tx_entries; i++)
  {
    if((trans_cum_success_gp_local_ptr->succ_cum))
    {
      fprintf(gui_fp, "Cumulative Transaction Successful/Sec - %s=%6.3f\n",
        nslb_get_norm_table_data(dynObjForGdf[NEW_OBJECT_DISCOVERY_TX].normTable, i), convert_ps(trans_cum_success_gp_local_ptr->succ_cum));
      trans_cum_success_gp_local_ptr++;
    }
  }
  fprintf(gui_fp, "\n");
}
//It will log the Transaction Failure data
void log_trans_failure_gp()
{
  if(trans_failure_gp_ptr == NULL) return;
  NSDL2_GDF(NULL, NULL, "Method called");
  int i;
  Trans_failure_gp *trans_failure_gp_local_ptr = trans_failure_gp_ptr;
  
  fprintf(gui_fp,"\nTransaction Failure/Sec: \n");
  for(i = 0; i < total_tx_entries; i++)
  {
    if((trans_failure_gp_local_ptr->failure))
    {
      fprintf(gui_fp, "  Transaction Failure/Sec - %s=%6.3f\n",
        nslb_get_norm_table_data(dynObjForGdf[NEW_OBJECT_DISCOVERY_TX].normTable, i), convert_ps(trans_failure_gp_local_ptr->failure));
      trans_failure_gp_local_ptr++;
    }
  }
  fprintf(gui_fp, "\n");
}
//It will log the cumulative Transaction Failure data
void log_trans_cum_failure_gp()
{
  if(trans_cum_failure_gp_ptr == NULL) return;
  NSDL2_GDF(NULL, NULL, "Method called");
  int i;
  Trans_cum_failure_gp *trans_cum_failure_gp_local_ptr = trans_cum_failure_gp_ptr;

  fprintf(gui_fp,"\n Cumulative Transaction Failure/Sec: \n");
  for(i = 0; i < total_tx_entries; i++)
  {
    if((trans_cum_failure_gp_local_ptr->failure_cum))
    {
      fprintf(gui_fp, "  Transaction Failure/Sec - %s=%6.3f\n",
        nslb_get_norm_table_data(dynObjForGdf[NEW_OBJECT_DISCOVERY_TX].normTable, i), convert_ps(trans_cum_failure_gp_local_ptr->failure_cum));
      trans_cum_failure_gp_local_ptr++;
    }
  }
  fprintf(gui_fp, "\n");
}
#endif   
// It will log the all trans data
void log_all_trans_data()
{
  if(trans_overall_gp_ptr == NULL) return; //This is for avoiding the below line if there is no TX in the scenario Anuj:12/20/07
  NSDL2_GDF(NULL, NULL, "Method called");
  fprintf(gui_fp,"All Transaction Info: \n");

  log_trans_overall_gp();
  log_trans_fail_gp();
  // log_trans_stats_gp(); // Commented  in DynTx project as this method needs changed to be done later
  // log_trans_cum_stats_gp();
//  log_trans_completed_gp();
//  log_trans_completed_gp();
//  log_trans_cum_completed_gp();
//  log_trans_success_gp();
//  log_trans_cum_success_gp();
//  log_trans_failure_gp();
//  log_trans_cum_failure_gp();
}

void fill_pre_post_test_msg_hdr(int opcode)
{
  NSDL2_GDF(NULL, NULL, "Method called");
  extern long long g_partition_idx;
  MY_MALLOC(msg_data_ptr,sizeof(Msg_data_hdr) ,"msg_data_ptr", -1);
  Msg_data_hdr *msg_data_hdr_ptr  = (Msg_data_hdr *) msg_data_ptr; // msg_data_ptr is of char type, defined in ns_gdf.c

  msg_data_hdr_ptr->opcode        = opcode;
  msg_data_hdr_ptr->test_run_num  = testidx;               // it is global variable for test_run_no
  msg_data_hdr_ptr->interval      = global_settings->progress_secs; 
  msg_data_hdr_ptr->seq_no        = 0;
  msg_data_hdr_ptr->partition_idx = g_partition_idx;               // it is global variable for partition index
  msg_data_hdr_ptr->abs_timestamp = (time(NULL)) * 1000;
  msg_data_hdr_ptr->gdf_seq_no    = 0;
  msg_data_hdr_ptr->pdf_seq_no    = 0;
  msg_data_hdr_ptr->cav_main_pid  = getpid();
   
}


//Function for creating the "msg start header"
void fill_msg_start_hdr ()
{
  NSDL2_GDF(NULL, NULL, "Method called");
  extern long long g_partition_idx;
  Msg_data_hdr *msg_data_hdr_start_ptr  = (Msg_data_hdr *) msg_data_ptr; // msg_data_ptr is of char type, defined in ns_gdf.c

  msg_data_hdr_start_ptr->opcode        = /* htonl */(MSG_START_PKT);
  msg_data_hdr_start_ptr->test_run_num  = /* htonl */(testidx);               // it is global variable for test_run_no
  msg_data_hdr_start_ptr->interval      = /* htonl */(global_settings->progress_secs); // PROGRESS_MESCS is the keyword in Netstorm, which sets the interval b/w two sampling period.
  msg_data_hdr_start_ptr->seq_no        = /* htonl */(0);
  msg_data_hdr_start_ptr->partition_idx  = g_partition_idx;               // it is global variable for partition index
  //If time() method fails, a negative value may be sent.
  msg_data_hdr_start_ptr->abs_timestamp  = (time(NULL)) * 1000;
  global_settings->g_rtg_hpts = (time(NULL)) * 1000;
  //fill gdf file count
  msg_data_hdr_start_ptr->gdf_seq_no = test_run_gdf_count;
}

//Function for creating the "msg data header"
/* static inline void fill_msg_data_opcode (avgtime *avg) */
/* { */
/*   Msg_data_hdr *msg_data_hdr_data_ptr   = (Msg_data_hdr *) msg_data_ptr; */
/*   msg_data_hdr_data_ptr->opcode         =  htonl(MSG_DATA_PKT); */
/* } */

Long_data rtg_pkt_ts = -1; 
inline void reset_rtg_pkt_ts()
{
 //TODO  partition switch
}
inline void fill_msg_data_seq (avgtime *avg, int testidx)
{
  Long_data current_ts;
  Long_data diff_ts;
  Long_data abs_diff_ts;
  float check_diff_ts;
  NSDL2_GDF(NULL, NULL, "Method called");
  Msg_data_hdr *msg_data_hdr_data_ptr   = (Msg_data_hdr *) msg_data_ptr;
  NSDL2_GDF(NULL, NULL, "rtg_msg_seq = %d", rtg_msg_seq);
  ++rtg_msg_seq;
  msg_data_hdr_data_ptr->seq_no         =  rtg_msg_seq;
  NSDL2_GDF(NULL, NULL, "rtg_msg_seq = %d", rtg_msg_seq);
  msg_data_hdr_data_ptr->opcode         =  /* htonl */(MSG_DATA_PKT);
  msg_data_hdr_data_ptr->test_run_num  = /* htonl */(testidx);               // it is global variable for test_run_no
  msg_data_hdr_data_ptr->gdf_seq_no = test_run_gdf_count;
  msg_data_hdr_data_ptr->pdf_seq_no = testrun_pdf_and_pctMessgae_version;
  current_ts = (time(NULL)) * 1000;

/*Earlier we do malloc/memset of msg_data_ptr only one time, and in start hdr only we were writing interval in msg_data_ptr, in successive 
  packets we were not writing interval again and again because we do not need to write same data on same memory.   
  But now we need to Write interval again in msg_data_ptr because in monitor runtime changes, if next partition obtained by 
  func nde_set_partition_time() is same as current partition then we do not switch partition, but before this check, we have already 
  done free and malloc/memset of msg_data_ptr, hence we need to write interval here also. */
  msg_data_hdr_data_ptr->interval      = global_settings->progress_secs; // PROGRESS_MESCS is the keyword in Netstorm, which sets the interval b/w two sampling period.

  msg_data_hdr_data_ptr->partition_idx = g_partition_idx;
  //msg_data_hdr_data_ptr->abs_timestamp = (time(NULL)) * 1000;
  //TODO will timestamp become less then partition switch 
  if(rtg_pkt_ts == -1)
    rtg_pkt_ts = time(NULL) * 1000;
  else
  {
    rtg_pkt_ts += global_settings->progress_secs;
  }
  diff_ts = current_ts - rtg_pkt_ts;
  abs_diff_ts = abs(diff_ts);
  check_diff_ts = (global_settings->progress_secs * 0.5);
  if ( abs_diff_ts >= check_diff_ts )
  {
    if ( diff_ts > 0)
    {
      NSTL1( NULL, NULL, "RTG data packet timestamp (%lld) is more than current timestamp (%lld) difference (%lld)", rtg_pkt_ts, current_ts, abs_diff_ts );
      mon_update_times_data(&hm_times_data->rtg_pkt_ts_diff, abs_diff_ts);
    }
    else if ( diff_ts != 0 )
    {
      NSTL1( NULL, NULL, "RTG data packet timestamp (%lld) is less than current timestamp (%lld) difference (%lld)", rtg_pkt_ts, current_ts, abs_diff_ts );
      mon_update_times_data(&hm_times_data->rtg_pkt_ts_diff, abs_diff_ts);
    }
  }
  msg_data_hdr_data_ptr->abs_timestamp = rtg_pkt_ts;
  append_testrun_all_gdf_info();
}   

//Function for creating the "msg end header"
void fill_msg_end_hdr (avgtime *avg)
{
  NSDL2_GDF(NULL, NULL, "Method called");
  Msg_data_hdr *msg_data_hdr_end_ptr  = (Msg_data_hdr *) msg_data_ptr;

  msg_data_hdr_end_ptr->opcode   =  /* htonl */(MSG_END_PKT);  // Other fields are same as filed in start
  msg_data_hdr_end_ptr->seq_no   =  /* htonl */(0);
  msg_data_hdr_end_ptr->gdf_seq_no = test_run_gdf_count;
}

// End of File
