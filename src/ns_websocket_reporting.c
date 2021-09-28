/**********************************************************************
 * File Name            : ns_websocket_reporting.c
 * Author(s)            : Deepika
 * Date                 : 27 Sept 2017
 * Copyright            : (c) Cavisson Systems
 * Purpose              : Graphs & Reporting Websocket Stats
 *
 * Modification History :
 *              <Author(s)>, <Date>, <Change Description/Location>
**********************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <openssl/sha.h>

#include "url.h"
#include "util.h"
#include "ns_common.h"
#include "ns_global_settings.h"
#include "ns_log.h"
#include "ns_script_parse.h"
#include "ns_http_process_resp.h"
#include "ns_log_req_rep.h"
#include "ns_websocket.h"
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
#include "netomni/src/core/ni_scenario_distribution.h"
#include "wait_forever.h"
#include "ns_group_data.h"
#include "ns_websocket_reporting.h"
#ifndef CAV_MAIN
int g_ws_avgtime_idx = -1;
WSAvgTime *ws_avgtime = NULL;
#else
__thread int g_ws_avgtime_idx = -1;
__thread WSAvgTime *ws_avgtime = NULL;
#endif
static double convert_long_ps(Long_data row_data)
{
  NSDL2_GDF(NULL, NULL, "Method called, row_data = %lu", row_data);
  return((double )((row_data))/((double )global_settings->progress_secs/1000.0));
}
int g_ws_cavgtime_idx = -1;
int ws_msg_data_gp_idx = -1;
int total_ws_status_codes = 0;
WSCAvgTime *ws_cavgtime = NULL;

// called by parent
inline void update_websocket_data_cavgtime_size()
{
  NSDL2_WS(NULL, NULL, "Method Called, g_wscavgtime_size = %d, g_ws_cavgtime_idx = %d",
                        g_cavgtime_size, g_ws_cavgtime_idx);

  if(global_settings->protocol_enabled & WS_PROTOCOL_ENABLED) {
    NSDL2_WS(NULL, NULL, "DDDD ws is enabled.");
    g_ws_cavgtime_idx = g_cavgtime_size;
    g_cavgtime_size += sizeof(WSCAvgTime);
  } else {
    NSDL2_WS(NULL, NULL, "ws is disabled.");
  }

  NSDL2_WS(NULL, NULL, "After g_cavgtime_size = %d, g_ws_cavgtime_idx = %d",
                        g_cavgtime_size, g_ws_cavgtime_idx);
}

// Called by parent
inline void update_websocket_data_avgtime_size() 
{
  int ws_avg_size = sizeof(WSAvgTime);

  NSDL2_WS(NULL, NULL, "Method Called, protocol_enabled = %0x, g_avgtime_size = %d, g_ws_avgtime_idx = %d, WSAcgTime size = %d",
                        global_settings->protocol_enabled, g_avgtime_size, g_ws_avgtime_idx, ws_avg_size);

  if(global_settings->protocol_enabled & WS_PROTOCOL_ENABLED) 
  {
    g_ws_avgtime_idx = g_avgtime_size;
    g_avgtime_size += ws_avg_size;
  } 
  else 
    NSDL2_WS(NULL, NULL, "WS Protocol is disabled");

  NSDL2_WS(NULL, NULL, "Method exit, Updated g_avgtime_size = %d, g_ws_avgtime_idx = %d",
                        g_avgtime_size, g_ws_avgtime_idx);
}

inline void ws_set_ws_avgtime_ptr() {
  NSDL2_WS(NULL, NULL, "Method Called");
  if(global_settings->protocol_enabled & WS_PROTOCOL_ENABLED) {
    NSDL2_WS(NULL, NULL, "ws is enabled.");
   /* We have allocated average_time with the size of wsAvgTime
    * also now we can point that using g_ws_avgtime_idx*/
    ws_avgtime = (WSAvgTime*)((char *)average_time + g_ws_avgtime_idx);
    NSDL2_WS(NULL, NULL, "g_ws_avgtime_idx = %d, ws_avgtime = %p", g_ws_avgtime_idx, ws_avgtime);
  } else {
    NSDL2_WS(NULL, NULL, "ws is disabled.");
    ws_avgtime = NULL;
  }

  NSDL2_WS(NULL, NULL, "ws_avgtime = %p", ws_avgtime);
}

//fill data from WSAvgTime to WSStats_gp
inline void fill_ws_stats_gp(avgtime **g_avg, cavgtime **g_cavg)
{
  double result = 0;
  int g_idx = 0;              //Graph index
  int v_idx = 0, grp_idx = 0;
  WSAvgTime *ws_avg = NULL;
  avgtime *avg = NULL;
  WSCAvgTime *ws_cavg = NULL;
  cavgtime *cavg = NULL;

  if(ws_stats_gp_ptr == NULL) return;
  WSStats_gp *ws_stats_local_gp_ptr = ws_stats_gp_ptr;
  NSDL2_GDF(NULL, NULL, "Method called");

  for(v_idx = 0; v_idx < sgrp_used_genrator_entries + 1; v_idx++)
  {
    avg = (avgtime *)g_avg[v_idx];
    cavg = (cavgtime *)g_cavg[v_idx];
    for(grp_idx = 0; grp_idx < TOTAL_GRP_ENTERIES_WITH_GRP_KW; grp_idx++)
    {
      ws_avg = (WSAvgTime*)((char*)((char *)avg + (grp_idx * g_avg_size_only_grp)) + g_ws_avgtime_idx);
      ws_cavg = (WSCAvgTime*)((char*)((char *)cavg + (grp_idx * g_avg_size_only_grp)) + g_ws_cavgtime_idx);

      NSDL2_WS(NULL, NULL, "ws_avg = [%p]", ws_avg);

      NSDL2_WS(NULL, NULL, "ws_avg->ws_num_con_succ = %llu, ws_avg->ws_tx_bytes = %llu, ws_avg->ws_rx_bytes = %llu, duration = %d", 
                            ws_avg->ws_num_con_succ, ws_avg->ws_tx_bytes, ws_avg->ws_rx_bytes, global_settings->progress_secs);
      NSDL2_WS(NULL, NULL, "group_data_ptr = %p, ws_stats_local_gp_ptr = %p, ws_stats_gp_idx = %d", 
                            group_data_ptr, ws_stats_local_gp_ptr, ws_stats_gp_idx);

      //1. WebSocket Connections Initiated/Sec
      result = ((double)(ws_avg->ws_num_con_initiated))/((double)global_settings->progress_secs/1000);
      NSDL2_WS(NULL, NULL, "Websocket Connections Initiate = %llu, Sec = %f", ws_avg->ws_num_con_initiated, result);

      GDF_COPY_VECTOR_DATA(ws_stats_gp_idx, g_idx, v_idx, 0,
                           result, ws_stats_local_gp_ptr->ws_stats_initiated_conn_per_sec); g_idx++;

      //2. WebSocket Total Connections Initiated
      result = (double)(ws_cavg->ws_c_num_con_initiated);
      NSDL2_WS(NULL, NULL, "Websocket Connections Initiate = %f", result);

      GDF_COPY_VECTOR_DATA(ws_stats_gp_idx, g_idx, v_idx, 0,
                           result, ws_stats_local_gp_ptr->ws_stats_tot_initiated_conn); g_idx++;
      
      //3. WebSocket Connections Established/Sec
      result = ((double)(ws_avg->ws_num_con_succ))/((double)global_settings->progress_secs/1000);
      NSDL2_WS(NULL, NULL, " Websocket Connections Success = %llu, Sec = %f", ws_avg->ws_num_con_succ, result);

      GDF_COPY_VECTOR_DATA(ws_stats_gp_idx, g_idx, v_idx, 0,
                           result, ws_stats_local_gp_ptr->ws_stats_established_conn_per_sec); g_idx++;

      //4. WebSocket Total Connections Established
      result = ((double)(ws_cavg->ws_c_num_con_succ));
      NSDL2_WS(NULL, NULL, " Websocket Connections Success = %f", result);

      GDF_COPY_VECTOR_DATA(ws_stats_gp_idx, g_idx, v_idx, 0,
                           result, ws_stats_local_gp_ptr->ws_stats_tot_established_conn); g_idx++;

      //5. WebSocket Connections Closed/Sec
      result = ((double)(ws_avg->ws_num_con_break))/((double)global_settings->progress_secs/1000);
      NSDL2_WS(NULL, NULL, "Websocket Connections Closed = %llu, sec = %f", ws_avg->ws_num_con_break, result);

      GDF_COPY_VECTOR_DATA(ws_stats_gp_idx, g_idx, v_idx, 0,
                           result, ws_stats_local_gp_ptr->ws_stats_closed_conn_per_sec); g_idx++;

      //6. Websocket Total Connections Closed
      result = (double)(ws_cavg->ws_c_num_con_break);
      NSDL2_WS(NULL, NULL, "Websocket Total Connections Closed = %f", result);

      GDF_COPY_VECTOR_DATA(ws_stats_gp_idx, g_idx, v_idx, 0,
                           result, ws_stats_local_gp_ptr->ws_stats_tot_closed_conn); g_idx++;

      //7. Websocket Connections Failed/Sec
      result = ((double)(ws_avg->ws_num_con_fail))/((double)global_settings->progress_secs/1000);
      NSDL2_WS(NULL, NULL, "Websocket Connections Failed = %llu, Sec = %f", ws_avg->ws_num_con_fail, result);

      GDF_COPY_VECTOR_DATA(ws_stats_gp_idx, g_idx, v_idx, 0,
                           result, ws_stats_local_gp_ptr->ws_stats_failed_conn_per_sec); g_idx++;

      //8. Websocket Total Connections Failed
      result = (double)(ws_cavg->ws_c_num_con_fail);
      NSDL2_WS(NULL, NULL, "Websocket Connections Fail = %f", result);

      GDF_COPY_VECTOR_DATA(ws_stats_gp_idx, g_idx, v_idx, 0,
                           result, ws_stats_local_gp_ptr->ws_stats_tot_failed_conn); g_idx++;

      //9. Websocket Message Sent/Sec
      result = ((double)(ws_avg->ws_num_msg_send))/((double)global_settings->progress_secs/1000);
      NSDL2_WS(NULL, NULL, "Websocket Total Message Send = %llu, Sec = %f", ws_avg->ws_num_msg_send, result);

      GDF_COPY_VECTOR_DATA(ws_stats_gp_idx, g_idx, v_idx, 0,
                           result, ws_stats_local_gp_ptr->ws_stats_msg_sent_per_sec); g_idx++;

      //10. Websocket Total Message Sent
      result = ((double)(ws_cavg->ws_c_num_msg_send));
      NSDL2_WS(NULL, NULL, "Websocket Total Message Sent = %f", result);

      GDF_COPY_VECTOR_DATA(ws_stats_gp_idx, g_idx, v_idx, 0,
                           result, ws_stats_local_gp_ptr->ws_stats_tot_msg_sent); g_idx++;

      //11. Websocket Message Read/Sec 
      result = ((double)(ws_avg->ws_num_msg_read))/((double)global_settings->progress_secs/1000);
      NSDL2_WS(NULL, NULL, "Websocket Message Received = %llu, Sec = %f", ws_avg->ws_num_msg_read, result);

      GDF_COPY_VECTOR_DATA(ws_stats_gp_idx, g_idx, v_idx, 0,
                           result, ws_stats_local_gp_ptr->ws_stats_msg_read_per_sec); g_idx++;

      //12. Websocket Total Message Read
      result = ((double)(ws_cavg->ws_c_num_msg_read));
      NSDL2_WS(NULL, NULL, "Websocket Total Message Received = %f", result);

      GDF_COPY_VECTOR_DATA(ws_stats_gp_idx, g_idx, v_idx, 0,
                           result, ws_stats_local_gp_ptr->ws_stats_tot_msg_read); g_idx++;

      //13. Websocket Message Send Failed/Sec
      result = ((double)(ws_avg->ws_num_msg_send_fail))/((double)global_settings->progress_secs/1000);
      NSDL2_WS(NULL, NULL, "Websocket Message send Fail = %llu, Sec = %f", ws_avg->ws_num_msg_send_fail, result);

      GDF_COPY_VECTOR_DATA(ws_stats_gp_idx, g_idx, v_idx, 0,
                           result, ws_stats_local_gp_ptr->ws_stats_msg_send_failed_per_sec); g_idx++;

      //14. Websocket Total Message Send Failed
      result = ((double)(ws_cavg->ws_c_num_msg_send_fail));
      NSDL2_WS(NULL, NULL, "Websocket Message send Fail = %f", result);

      GDF_COPY_VECTOR_DATA(ws_stats_gp_idx, g_idx, v_idx, 0,
                           result, ws_stats_local_gp_ptr->ws_stats_tot_msg_send_failed); g_idx++;

      //15. Websocket Message Read Failed/Sec
      result = ((double)(ws_avg->ws_num_msg_read_fail))/((double)global_settings->progress_secs/1000);
      NSDL2_WS(NULL, NULL, "Websocket Message read Fail = %llu, Sec = %f", ws_avg->ws_num_msg_read_fail, result);

      GDF_COPY_VECTOR_DATA(ws_stats_gp_idx, g_idx, v_idx, 0,
                           result, ws_stats_local_gp_ptr->ws_stats_msg_read_failed_per_sec); g_idx++;

      //16. Websocket Total Message Read Failed
      result = ((double)(ws_cavg->ws_c_num_msg_read_fail));
      NSDL2_WS(NULL, NULL, "Websocket Message read Fail = %f", result);

      GDF_COPY_VECTOR_DATA(ws_stats_gp_idx, g_idx, v_idx, 0,
                           result, ws_stats_local_gp_ptr->ws_stats_tot_msg_read_failed); g_idx++;

      //17. WebSocket Send Throughput (Converting Kbps)
      result = ((double)(ws_avg->ws_tx_bytes))/((((double)global_settings->progress_secs)/1000) * 128.0);
      NSDL2_WS(NULL, NULL, "WebSocket Send Throughput = %llu, Sec = %f", ws_avg->ws_tx_bytes, result);

      GDF_COPY_VECTOR_DATA(ws_stats_gp_idx, g_idx, v_idx, 0,
                           result, ws_stats_local_gp_ptr->ws_stats_tot_bytes_send_per_sec); g_idx++;

      //18. WebSocket Receive Throughput (Converting Kbps)
      result = ((double)(ws_avg->ws_rx_bytes))/((((double)global_settings->progress_secs)/1000) * 128.0);
      NSDL2_WS(NULL, NULL, "WebSocket Receive Throughput = %llu, Sec = %f", ws_avg->ws_rx_bytes, result);

      GDF_COPY_VECTOR_DATA(ws_stats_gp_idx, g_idx, v_idx, 0,
                           result, ws_stats_local_gp_ptr->ws_stats_tot_bytes_recv_per_sec); g_idx++;

      g_idx = 0;
      ws_stats_local_gp_ptr++;
    }
  }
}
 
void print_ws_throughput(FILE *fp1, FILE *fp2, int is_periodic, avgtime *avg, cavgtime *cavg)
{

  double ws_tcp_rx, ws_tcp_tx;
  double duration;
  //char ubuffer[1024]="";
  WSAvgTime *ws_avg = NULL;
  WSCAvgTime *ws_cavg = NULL;

  NSDL2_WS(NULL, NULL, "Method called, protocol_enabled = [%x], is_periodic = %d", 
                        global_settings->protocol_enabled, is_periodic);

  if(!(global_settings->protocol_enabled & WS_PROTOCOL_ENABLED)) {
    return; 
  }
  /* We have allocated average_time with the size of wsAvgTime
   * also now we can point that using g_ws_avgtime_idx
   */
  ws_avg = (WSAvgTime*)((char *)avg + g_ws_avgtime_idx);
  ws_cavg = (WSCAvgTime*)((char *)cavg + g_ws_cavgtime_idx);

  u_ns_8B_t  ws_tot_tcp_rx = 0, ws_tot_tcp_tx = 0;
  double ws_con_made_rate, ws_con_break_rate;
  double ws_hit_tot_rate, ws_hit_succ_rate, ws_hit_initited_rate; 
  u_ns_8B_t ws_num_completed, ws_num_initiated, ws_num_succ; //,ws_num_samples;

  if (is_periodic) {
    duration = (double)((double)(global_settings->progress_secs)/1000.0);
    ws_tcp_rx = ((double)(ws_avg->ws_rx_bytes))/(duration*128.0);
    ws_tcp_tx = ((double)(ws_avg->ws_tx_bytes))/(duration*128.0);

    ws_con_made_rate = ((double)(ws_avg->ws_num_con_succ * 1000))/global_settings->progress_secs;
    ws_con_break_rate = ((double)(ws_avg->ws_num_con_break * 1000))/global_settings->progress_secs;
    ws_num_completed = ws_avg->ws_num_tries;
    //ws_num_samples = ws_num_succ = ws_avg->ws_num_hits;
    ws_num_succ = ws_avg->ws_num_hits;
    ws_num_initiated = ws_avg->ws_fetches_started;
    ws_hit_tot_rate = ((double)(ws_num_completed * 1000))/global_settings->progress_secs;
    ws_hit_succ_rate =((double)(ws_num_succ * 1000))/global_settings->progress_secs;
    ws_hit_initited_rate = ((double)(ws_num_initiated * 1000))/global_settings->progress_secs;
    NSDL2_MESSAGES(NULL, NULL, "In periodic, ws_num_completed = %llu, progress_secs = %d", ws_num_completed, global_settings->progress_secs);
  } else {
      duration = (double)((double)(global_settings->test_duration)/1000.0);
      ws_tcp_rx = ((double)(ws_cavg->ws_c_num_rx_bytes))/(duration*128.0);
      ws_tcp_tx = ((double)(ws_cavg->ws_c_num_tx_bytes))/(duration*128.0);

      ws_con_made_rate = ((double)(ws_cavg->ws_c_num_con_succ * 1000))/global_settings->test_duration;
      ws_con_break_rate = ((double)(ws_cavg->ws_c_num_con_break * 1000))/global_settings->test_duration;
      ws_num_completed = ws_cavg->ws_fetches_completed;
      //ws_num_samples = ws_num_succ = ws_cavg->ws_succ_fetches;
      ws_num_succ = ws_cavg->ws_succ_fetches;
      NSDL2_MESSAGES(NULL, NULL, "  ws_succ_fetches in ws file = %llu, no of succ in ws = %llu", ws_cavg->ws_succ_fetches , ws_num_succ); 
      ws_num_initiated = ws_cavg->cum_ws_fetches_started;
      ws_hit_tot_rate = ((double)(ws_num_completed * 1000))/global_settings->test_duration;
      ws_hit_succ_rate = ((double)(ws_num_succ * 1000))/global_settings->test_duration;
      ws_hit_initited_rate = ((double)(ws_num_initiated * 1000))/global_settings->test_duration;    
      NSDL2_MESSAGES(NULL, NULL, "In cumulative, ws_num_completed = %llu, progress_secs = %d", ws_num_completed, global_settings->progress_secs);
  }

  NSDL2_WS(NULL, NULL, "duration = %f, ws_tcp_rx = %f, ws_tcp_tx = %f, ws_con_made_rate = %f, ws_con_break_rate = %f, "
                       "ws_num_completed = %llu, ws_num_succ = %llu, ws_num_initiated = %llu, ws_hit_tot_rate = %.3f, "
                       "ws_hit_succ_rate = %.3f, ws_hit_initited_rate = %.3f", 
                        duration, ws_tcp_rx, ws_tcp_tx, ws_con_made_rate, ws_con_break_rate, ws_num_completed, 
                        ws_num_succ, ws_num_initiated, ws_hit_tot_rate, ws_hit_succ_rate, ws_hit_initited_rate);

 
  ws_tot_tcp_rx = (u_ns_8B_t)ws_cavg->ws_c_num_rx_bytes;
  ws_tot_tcp_tx = (u_ns_8B_t)ws_cavg->ws_c_num_tx_bytes;
 
  //if (ws_avg->ws_num_hits) 
  { 
    if(global_settings->show_initiated)
    fprint2f(fp1, fp2, "    WebSocket hit rate (per sec): Initiated=%.3f completed=%.3f Success=%.3f\n",
                            ws_hit_initited_rate, ws_hit_tot_rate, ws_hit_succ_rate);
    else
      fprint2f(fp1, fp2, "    WebSocket hit rate (per sec): Total=%.3f Success=%.3f\n", ws_hit_tot_rate, ws_hit_succ_rate);

    fprint2f(fp1, fp2, "    WebSocket (Kbits/s): TCP(Rx=%'.3f Tx=%'.3f)\n",
                            ws_tcp_rx, ws_tcp_tx);

    fprint2f(fp1, fp2, "    WebSocket (Total Bytes): TCP(Rx=%'llu Tx=%'llu)\n",
                            ws_tot_tcp_rx, ws_tot_tcp_tx);

    fprint2f(fp1, fp2, "    WebSocket TCP connections: Current=%llu Rate/s(Open=%.3f Close=%.3f) Total(Open=%'llu Close=%'llu)\n",
                            //ws_avg->ws_num_connections,
                            ws_avg->ws_num_con_succ,
                            ws_con_made_rate,
                            ws_con_break_rate,
                            ws_cavg->ws_c_num_con_succ,
                            ws_cavg->ws_c_num_con_break);
  }
}

inline void fill_ws_status_codes(avgtime **g_avg)
{
  int i, gv_idx, grp_idx;

  WSAvgTime *ws_avg = NULL;
  avgtime *avg = NULL;

  NSDL2_WS(NULL, NULL, "Method called, g_avg = [%p]", g_avg);

  if(ws_status_codes_gp_ptr == NULL) return;
  WSStatusCodes_gp *ws_status_codes_local_gp_ptr = ws_status_codes_gp_ptr;
  for(gv_idx = 0; gv_idx < sgrp_used_genrator_entries + 1; gv_idx++)
  {
    avg = (avgtime *)g_avg[gv_idx];
    for(grp_idx = 0; grp_idx < TOTAL_GRP_ENTERIES_WITH_GRP_KW; grp_idx++)
    {
      ws_avg = (WSAvgTime*)((char*)((char *)avg + (grp_idx * g_avg_size_only_grp)) + g_ws_avgtime_idx);

      NSDL2_WS(NULL, NULL, "ws_avg = [%p], ws_status_codes_local_gp_ptr = [%p], ws_status_codes_gp_idx = [%d]", 
                            ws_avg, ws_status_codes_local_gp_ptr, ws_status_codes_gp_idx);

      for (i = 0; i < total_ws_status_codes; i++)
      {
        GDF_COPY_VECTOR_DATA(ws_status_codes_gp_idx, i, gv_idx, 0,
                             ws_avg->ws_status_codes[i], ws_status_codes_local_gp_ptr->ws_status_codes[i]);
        NSDL2_WS(NULL, NULL, "ws_avg->ws_status_codes[%d] = %d", i, ws_avg->ws_status_codes[i]);
      }
      ws_status_codes_local_gp_ptr++;
    }
  }
}

/* we dont need to worry about TO and con fails here since there req_code will be 0 */
inline void update_ws_status_codes(connection *cptr, WSAvgTime *ws_avgtime)
{
  VUser *vptr = ((VUser *)(cptr->vptr));

  NSDL3_WS(NULL, NULL, "Method called, cptr = %p, vptr = %p, ws_avgtime = %p, cptr->req_code = %d", cptr, vptr, ws_avgtime, cptr->req_code);

  //Updating WebSocket Failures Graph: Only 4xx and 5xx error codes are set
  if(cptr->req_code >= 400 && cptr->req_code <= 417)
  {
    vptr->ws_status = NS_REQUEST_4xx;
    ws_avgtime->ws_error_codes[NS_REQUEST_4xx]++;
    NSDL2_WS(NULL, NULL, "ws_avgtime->ws_error_codes[%d] = %llu", vptr->ws_status, ws_avgtime->ws_error_codes[NS_REQUEST_4xx]);
  }
  if(cptr->req_code >= 500 && cptr->req_code <= 505)
  {
    vptr->ws_status = NS_REQUEST_5xx;
    ws_avgtime->ws_error_codes[NS_REQUEST_5xx]++;
    NSDL2_WS(NULL, NULL, "ws_avgtime->ws_error_codes[%d] = %llu", vptr->ws_status, ws_avgtime->ws_error_codes[NS_REQUEST_5xx]);
  }

  //Updating WebSocket Response Codes Graph
  if (cptr->req_code == 100) {  /* Continue */
    ws_avgtime->ws_status_codes[0]++;
  } else if (cptr->req_code == 101) { /* Switching Protocols */
    ws_avgtime->ws_status_codes[1]++;
  } else if (cptr->req_code == 200) { /* OK */
    ws_avgtime->ws_status_codes[2]++;
  } else if (cptr->req_code == 201) { /* Created */
    ws_avgtime->ws_status_codes[3]++;
  } else if (cptr->req_code == 202) { /* Accepted */
    ws_avgtime->ws_status_codes[4]++;
  } else if (cptr->req_code == 203) { /* Non-Authoritative Information */
    ws_avgtime->ws_status_codes[5]++;
  } else if (cptr->req_code == 204) { /* No Content */
    ws_avgtime->ws_status_codes[6]++;
  } else if (cptr->req_code == 205) { /* Reset Content */
    ws_avgtime->ws_status_codes[7]++;
  } else if (cptr->req_code == 206) { /* Partial Content */
    ws_avgtime->ws_status_codes[8]++;
  } else if (cptr->req_code == 300) { /* Multiple Choices */
    ws_avgtime->ws_status_codes[9]++;
  } else if (cptr->req_code == 301) { /* Moved Permanently */
    ws_avgtime->ws_status_codes[10]++;
  } else if (cptr->req_code == 302) { /* Found */
    ws_avgtime->ws_status_codes[11]++;
  } else if (cptr->req_code == 303) { /* See Other */
    ws_avgtime->ws_status_codes[12]++;
  } else if (cptr->req_code == 304) { /* Not Modified */
    ws_avgtime->ws_status_codes[13]++;
  } else if (cptr->req_code == 305) { /* Use Proxy */
    ws_avgtime->ws_status_codes[14]++;
  } else if (cptr->req_code == 306) { /* (Switch Proxy) */
    ws_avgtime->ws_status_codes[15]++;
  } else if (cptr->req_code == 307) { /* Temporary Redirect */
    ws_avgtime->ws_status_codes[16]++;
  } else if (cptr->req_code == 400) { /* Bad Request */
    ws_avgtime->ws_status_codes[17]++;
  } else if (cptr->req_code == 401) { /* Unauthorized */
    ws_avgtime->ws_status_codes[18]++;
  } else if (cptr->req_code == 402) { /* Payment Required */
    ws_avgtime->ws_status_codes[19]++;
  } else if (cptr->req_code == 403) { /* Forbidden */
    ws_avgtime->ws_status_codes[20]++;
  } else if (cptr->req_code == 404) { /* Not Found */
    ws_avgtime->ws_status_codes[21]++;
  } else if (cptr->req_code == 405) { /* Method Not Allowed */
    ws_avgtime->ws_status_codes[22]++;
  } else if (cptr->req_code == 406) { /* Not Acceptable */
    ws_avgtime->ws_status_codes[23]++;
  } else if (cptr->req_code == 407) { /* Proxy Authentication Required */
    ws_avgtime->ws_status_codes[24]++;
  } else if (cptr->req_code == 408) { /* Request Timeout */
    ws_avgtime->ws_status_codes[25]++;
  } else if (cptr->req_code == 409) { /* Conflict */
    ws_avgtime->ws_status_codes[26]++;
  } else if (cptr->req_code == 410) { /* Gone */
    ws_avgtime->ws_status_codes[27]++;
  } else if (cptr->req_code == 411) { /* Length Required */
    ws_avgtime->ws_status_codes[28]++;
  } else if (cptr->req_code == 412) { /* Precondition Failed */
    ws_avgtime->ws_status_codes[29]++;
  } else if (cptr->req_code == 413) { /* Request Entity Too Large */
    ws_avgtime->ws_status_codes[30]++;
  } else if (cptr->req_code == 414) { /* Request-URI Too Long */
    ws_avgtime->ws_status_codes[31]++;
  } else if (cptr->req_code == 415) { /* Unsupported Media Type */
    ws_avgtime->ws_status_codes[32]++;
  } else if (cptr->req_code == 416) { /* Requested Range Not Satisfiable */
    ws_avgtime->ws_status_codes[33]++;
  } else if (cptr->req_code == 417) { /* Expectation Failed */
    ws_avgtime->ws_status_codes[34]++;
  } else if (cptr->req_code == 500) { /* Internal Server Error */
    ws_avgtime->ws_status_codes[35]++;
  } else if (cptr->req_code == 501) { /* Not Implemented */
    ws_avgtime->ws_status_codes[36]++;
  } else if (cptr->req_code == 502) { /* Bad Gateway */
    ws_avgtime->ws_status_codes[37]++;
  } else if (cptr->req_code == 503) { /* Service Unavailable */
    ws_avgtime->ws_status_codes[38]++;
  } else if (cptr->req_code == 504) { /* Gateway Timeout */
    ws_avgtime->ws_status_codes[39]++;
  } else if (cptr->req_code == 505) { /* WS Version Not Supported */
    ws_avgtime->ws_status_codes[40]++;
  }
}

inline void fill_ws_failure_stats_gp(avgtime **g_avg)
{
  unsigned int ws_all_failures = 0;   // First graph of WebSocket Failures (ALL)
  int i, v_idx, all_idx, j = 0, grp_idx;
  WSAvgTime *ws_avg = NULL;
  avgtime *avg = NULL;

  if(ws_failure_stats_gp_ptr == NULL)  return;
  WSFailureStats_gp *ws_failure_local_gp_ptr = ws_failure_stats_gp_ptr;
  WSFailureStats_gp *ws_failure_local_first_gp_ptr = NULL;

  NSDL2_GDF(NULL, NULL, "Method called, g_avg = %p", g_avg);

  for(v_idx = 0; v_idx < sgrp_used_genrator_entries + 1; v_idx++)
  {
    avg = (avgtime *)g_avg[v_idx];
    for(grp_idx = 0; grp_idx < TOTAL_GRP_ENTERIES_WITH_GRP_KW; grp_idx++)
    {
      NSDL2_WS(NULL, NULL, "group_data_ptr = %p, ws_failure_local_gp_ptr = %p, ws_failure_stats_gp_idx = %d", 
                            group_data_ptr, ws_failure_local_gp_ptr, ws_failure_stats_gp_idx);
      ws_avg = (WSAvgTime*)((char*)((char *)avg + (grp_idx * g_avg_size_only_grp)) + g_ws_avgtime_idx);
      all_idx = j;
      j++;         /* First Case is for ALL in WebSocket Failure Graph, hence we are not filling it */ 
      ws_failure_local_first_gp_ptr = ws_failure_local_gp_ptr;
      ws_failure_local_gp_ptr++;
      for (i = 0; i < (TOTAL_USED_URL_ERR - 1); i++, j++)
      {
        GDF_COPY_VECTOR_DATA(ws_failure_stats_gp_idx, 0, j, 0, ws_avg->ws_error_codes[i + 1], ws_failure_local_gp_ptr->ws_failures);
        ws_all_failures += ws_avg->ws_error_codes[i + 1];
        ws_failure_local_gp_ptr++;
      }
      /*bug id 101742: using method convert_ps() */
      GDF_COPY_VECTOR_DATA(ws_failure_stats_gp_idx, 0, all_idx, 0, convert_long_ps(ws_all_failures), ws_failure_local_first_gp_ptr->ws_failures);
      ws_all_failures = 0;
        NSDL2_GDF(NULL, NULL, " WebSocket Failures/Sec  ws_all_failures= %lu ", ws_all_failures);
        NSDL2_GDF(NULL, NULL, " WebSocket Failures/Sec after ps result ws_all_failures = %lu ", convert_long_ps(ws_all_failures));
    }
  }
}

#if 0
void ws_log_summary_data (avgtime *avg, double *ws_data, FILE *fp, cavgtime *cavg)
{
  u_ns_8B_t num_completed, num_succ, num_samples;
  //WSAvgTime *ws_avg = NULL;
  WSCAvgTime *ws_cavg = NULL;

  NSDL2_WS(NULL, NULL, "Method Called");
  if(!(global_settings->protocol_enabled & WS_PROTOCOL_ENABLED)) {
      NSDL2_MESSAGES(NULL, NULL, "Returning: ws_PROTOCOL is not enabled.");
      return;
  }

  NSDL2_WS(NULL, NULL, "avg = %p, g_ws_avgtime_idx = %d", avg, g_ws_avgtime_idx);
  //ws_avg = (WSAvgTime *) ((char*) avg + g_ws_avgtime_idx); 
  ws_cavg = (WSCAvgTime *) ((char*) cavg + g_ws_cavgtime_idx); 
  //NSDL2_MESSAGES(NULL, NULL, "ws_avg = %p", ws_avg);
    
     if (total_ws_request_entries) {
       num_completed = ws_cavg->ws_fetches_completed;
       num_samples = num_succ = ws_cavg->ws_succ_fetches;
       NSDL2_MESSAGES(NULL, NULL, "num_completed = %llu, num_samples = num_succ = %llu", num_completed, num_samples);
                                  
       if (num_completed) {
          if (num_samples) {
             fprintf(fp, "101|ws|10|ws Min Time(Sec)|%1.3f\n", (double)(((double)(ws_cavg->ws_c_min_time))/1000.0));
             fprintf(fp, "101|ws|20|ws Avg Time(Sec)|%1.3f\n", (double)(((double)(ws_cavg->ws_c_tot_time))/((double)(1000.0*(double)num_samples))));
             fprintf(fp, "101|ws|30|ws Max Time(Sec)|%1.3f\n", (double)(((double)(ws_cavg->ws_c_max_time))/1000.0));
             fprintf(fp, "101|ws|40|ws Median Time(Sec)|%1.3f\n", ws_data[0]);
             fprintf(fp, "101|ws|50|ws 80th percentile Time(Sec)|%1.3f\n", ws_data[1]);
             fprintf(fp, "101|ws|60|ws 90th percentile Time(Sec)|%1.3f\n", ws_data[2]);
             fprintf(fp, "101|ws|70|ws 95th percentile Time(Sec)|%1.3f\n", ws_data[3]);
             fprintf(fp, "101|ws|80|ws 99th percentile Time(Sec)|%1.3f\n", ws_data[4]);
             fprintf(fp, "101|ws|90|ws Total|%llu\n", num_completed);
             fprintf(fp, "101|ws|100|ws Success|%llu\n", num_succ);
             fprintf(fp, "101|ws|110|ws Failures|%llu\n", num_completed -  num_succ);
             fprintf(fp, "101|ws|120|ws Failure PCT|%1.2f\n", (double)((((double)((num_completed -  num_succ)))*100.0)/((double)(num_completed))));
            } else {
             fprintf(fp, "101|ws|10|ws Min Time(Sec)|-\n");
             fprintf(fp, "101|ws|20|ws Avg Time(Sec)|-\n");
             fprintf(fp, "101|ws|30|ws Max Time(Sec)|-\n");
             fprintf(fp, "101|ws|40|ws Median Time(Sec)|-\n");
             fprintf(fp, "101|ws|50|ws 80th percentile Time(Sec)|-\n");
             fprintf(fp, "101|ws|60|ws 90th percentile Time(Sec)|-\n");
             fprintf(fp, "101|ws|70|ws 95th percentile Time(Sec)|-\n");
             fprintf(fp, "101|ws|80|ws 99th percentile Time(Sec)|-\n");
             fprintf(fp, "101|ws|90|ws Total|%llu\n", num_completed);
             fprintf(fp, "101|ws|100|ws Success|%llu\n", num_succ);
             fprintf(fp, "101|ws|110|ws Failures|%llu\n", num_completed -  num_succ);
             fprintf(fp, "101|ws|120|ws Failure PCT|%1.2f\n", (double)((((double)((num_completed -  num_succ)))*100.0)/((double)(num_completed))));
            }
          } else {
              fprintf(fp, "101|ws|10|ws Min Time(Sec)|-\n");
              fprintf(fp, "101|ws|20|ws Avg Time(Sec)|-\n");
              fprintf(fp, "101|ws|30|ws Max Time(Sec)|-\n");
              fprintf(fp, "101|ws|40|ws Median Time(Sec)|-\n");
              fprintf(fp, "101|ws|50|ws 80th percentile Time(Sec)|-\n");
              fprintf(fp, "101|ws|60|ws 90th percentile Time(Sec)|-\n");
              fprintf(fp, "101|ws|70|ws 95th percentile Time(Sec)|-\n");
              fprintf(fp, "101|ws|80|ws 99th percentile Time(Sec)|-\n");
              fprintf(fp, "101|ws|90|ws Total|0\n");
              fprintf(fp, "101|ws|100|ws Success|0\n");
              fprintf(fp, "101|ws|110|ws Failures|0\n");
              fprintf(fp, "101|ws|120|ws Failure PCT|0.00\n");
          }
          fprintf(fp, "101|ws|140|ws Hits/Sec|%1.2f\n", (float) ((double)(ws_cavg->ws_fetches_completed * 1000)/ (double)(global_settings->test_duration)));
         }
  
}
#endif

