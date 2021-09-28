/***************************************************************************
 Name		: ns_socket_udp_client_rpt.c 
 Purpose	: This file will contain all functions related to Socket API
                  Graphs.
                  Following metrices will be handle in Socket API -
                  Socket Stats:
                    Client Connection - Open(r), Close(r), Connect(r),
                      Timeout(r), 
                    Client Sent - Writ(r), Throughtput
                    Client Read - Writ(r), Throughtput
                    Server Connection - Open(r), Close(r), Connect(r),
                      Timeout(r), 
                    Server Sent - Writ(r), Throughtput
                    Server Read - Writ(r), Throughtput
                      
                  
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

#include "ns_socket.h"


//Global variables ------------------
//UDP Client
#ifndef CAV_MAIN
int g_udp_client_avg_idx = -1;                           // Index into avgtime
SocketClientAvgTime *g_udp_client_avg = NULL;
#else
__thread int g_udp_client_avg_idx = -1;                           // Index into avgtime
__thread SocketClientAvgTime *g_udp_client_avg = NULL;
#endif
int g_udp_client_cavg_idx = -1;                          // Index into cavgtime
SocketClientCAvgTime *g_udp_client_cavg = NULL;

int g_udp_client_rpt_group_idx = -1;                     // Index into Group_Info table 
UDPClientRTGData *g_udp_client_rtg_ptr = NULL;           // RTG pointer for UDP Client 

//UDP Server
int g_udp_server_avg_idx = -1;                           // Index into avgtime
int g_udp_server_cavg_idx = -1;                          // Index into cavgtime
SocketClientAvgTime *g_udp_server_avg = NULL;
SocketClientCAvgTime *g_udp_server_cavg = NULL;

int g_udp_server_rpt_group_idx = -1;                     // Index into Group_Info table 
UDPClientRTGData *g_udp_server_rtg_ptr = NULL;           // RTG pointer for UDP Client 


//Function Definitions ---------------
void update_avgtime_size4socket_udp_client() 
{
  int socket_avgsize = sizeof(SocketClientAvgTime);

  NSDL2_SOCKETS(NULL, NULL, "Method Called, protocol_enabled = %0x, "
               "g_avgtime_size = %d, g_udp_client_avg_idx = %d, "
               "SocketAvgTime size = %d",
                global_settings->protocol_enabled, g_avgtime_size, 
                g_udp_client_avg_idx, socket_avgsize);

  if (IS_UDP_CLIENT_API_EXIST) 
  {
    g_udp_client_avg_idx = g_avgtime_size;
    g_avgtime_size += socket_avgsize;
  } 
  else 
    NSDL2_SOCKETS(NULL, NULL, "Socket Protocol is disabled");

  NSDL2_SOCKETS(NULL, NULL, "AvgIndex for Sockect API done, g_avgtime_size = %d, "
           "g_udp_client_avg_idx = %d",
            g_avgtime_size, g_udp_client_avg_idx);
}

void update_cavgtime_size4socket_udp_client() 
{
  #ifndef CAV_MAIN
  int socket_cavgsize = sizeof(SocketClientCAvgTime);

  NSDL2_SOCKETS(NULL, NULL, "Method Called, protocol_enabled = %0x, "
               "g_avgtime_size = %d, g_udp_client_avg_idx = %d, "
               "SocketAvgTime size = %d",
                global_settings->protocol_enabled, g_cavgtime_size, 
                g_udp_client_cavg_idx, socket_cavgsize);

  if (IS_UDP_CLIENT_API_EXIST) 
  {
    g_udp_client_cavg_idx = g_cavgtime_size;
    g_cavgtime_size += socket_cavgsize;
  } 
  else 
    NSDL2_SOCKETS(NULL, NULL, "Socket Protocol is disabled");

  NSDL2_SOCKETS(NULL, NULL, "CAvgIndex for Sockect API done, g_avgtime_size = %d, "
           "g_udp_client_avg_idx = %d",
            g_cavgtime_size, g_udp_client_cavg_idx);
  #endif
}

void set_avgtime_ptr4socket_udp_client() 
{
  if (IS_UDP_CLIENT_API_EXIST) 
  {
    g_udp_client_avg = (SocketClientAvgTime *)((char *)average_time + g_udp_client_avg_idx);
    NSDL2_SOCKETS(NULL, NULL, "After setting g_udp_client_avg = %p, g_udp_client_avg_idx = %d", 
      g_udp_client_avg, g_udp_client_avg_idx);
  } 
  else 
  {
    NSDL2_SOCKETS(NULL, NULL, "Socket Protocol is disabled.");
    g_udp_client_avg = NULL;
  }
}

void fill_udp_client_avg(VUser *vptr, unsigned char metric, u_ns_8B_t value)
{
  SocketClientAvgTime *group_avg_ptr;

  NSDL2_SOCKETS(NULL, NULL, "Method called, [SocketStats-UDPClient] g_udp_client_avg = %p, "
    "metric = %d, value = %lld", g_udp_client_avg, metric, value);

  CHILD_FILL_SOCKET_AVGTIME(g_udp_client_avg, metric, value);
  
  if(SHOW_GRP_DATA)
  {
    group_avg_ptr = (SocketClientAvgTime *)((char *)((char *)average_time + 
                      ((vptr->group_num + 1) * g_avg_size_only_grp)) + g_udp_client_avg_idx);
  
    NSDL2_SOCKETS(NULL, NULL, "[SocketStats] Fill Socket avgtime for group = %d, "
      "group_avg_ptr = %p, metric = %d, value = %lld", 
      vptr->group_num, group_avg_ptr, metric, value);

    CHILD_FILL_SOCKET_AVGTIME(group_avg_ptr, metric, value);
  }
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Name		: fill_udp_client_gp()
  Purpose	: Copy socket API data from avgtime/cavgtime to msg_data_ptr(RTG)
                  Graph will be like -
                  
                  Test Metrics
                    |-> UDP Client
                          |-> Overall
                                |-> Group-1
                                      |-> UDP Client Connections Initiated
                                      |-> ...
                                |-> Group-2
                                      |-> ...
                          |-> Generator-1
                                |-> Group-1
                                      |-> UDP Client Connections Initiated
                                      |-> ...
                                |-> Group-2
                                      |-> ...
                                      |-> ...
                          |-> Generator-2
                                |-> Group-1
                                      |-> ...
                                |-> Group-2
                                      |-> ...
 
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
void fill_udp_client_gp(avgtime **g_avg, cavgtime **g_cavg)
{
  int group_vect_idx = 0;   // GDF group vector index, Here generators are Group vectors
  int sgrp_idx = 0;         // SGRP group index
  int graph_idx = 0;        // GDF graph index, DONOT rename it as used in below MACROS 
  int rtg_idx = g_udp_client_rpt_group_idx;
  
  SocketClientAvgTime *avg_ptr = NULL;
  SocketClientCAvgTime *cavg_ptr = NULL;

  UDPClientRTGData *rtg_ptr = g_udp_client_rtg_ptr;

  NSDL2_SOCKETS(NULL, NULL, "Method called, g_avg = %p, g_cavg = %p, "
            "socket_stats_gp_ptr = %p, sgrp_used_genrator_entries = %d", 
             g_avg, g_cavg, g_udp_client_rtg_ptr, sgrp_used_genrator_entries);

  if (!IS_UDP_CLIENT_API_EXIST)
    return;

  if (!rtg_ptr)  
    return;

  // Loop for vectors of Group_Info
  for (group_vect_idx = 0; group_vect_idx < sgrp_used_genrator_entries + 1; group_vect_idx++)
  {
    // Loop for SGRP group wise data
    for (sgrp_idx = 0; sgrp_idx < TOTAL_GRP_ENTERIES_WITH_GRP_KW; sgrp_idx++)
    {
      NSDL2_SOCKETS(NULL, NULL, "[SocketStats] group_idx = %d, g_udp_client_avg_idx = %d", 
          sgrp_idx, g_udp_client_avg_idx);
      
      avg_ptr = (SocketClientAvgTime *)((char*)((char *)g_avg[group_vect_idx] + 
                  (sgrp_idx *g_avg_size_only_grp)) + g_udp_client_avg_idx);

      cavg_ptr = (SocketClientCAvgTime *)((char*)((char *)g_cavg[group_vect_idx] + 
                   (sgrp_idx *g_cavg_size_only_grp)) + g_udp_client_cavg_idx);

      NSDL2_SOCKETS(NULL, NULL, "Before filling : rtg_ptr = %p, "
                "avg_ptr = %p, cavg_ptr = %p, sgrp_idx = %d, group_vect_idx = %d",
                 rtg_ptr, avg_ptr, cavg_ptr, sgrp_idx, group_vect_idx);

      // Connection Initiated 
      NSDL2_SOCKETS(NULL, NULL, "[SocketStats] Fill connection metrics for SGRP group = %d and Generator = %d", 
          sgrp_idx, group_vect_idx);

      PARENT_COPY_SOCKET_AVG2RTG(rtg_ptr->conn_init_ps, avg_ptr->num_conn_init, DT_RATE); 
      PARENT_COPY_SOCKET_AVG2RTG(rtg_ptr->conn_init_tot, cavg_ptr->num_cum_conn_init, DT_CUM); 

      PARENT_COPY_SOCKET_AVG2RTG(rtg_ptr->conn_open_ps, avg_ptr->num_conn_open, DT_RATE); 
      PARENT_COPY_SOCKET_AVG2RTG(rtg_ptr->conn_open_tot, cavg_ptr->num_cum_conn_open, DT_CUM); 

      PARENT_COPY_SOCKET_AVG2RTG(rtg_ptr->conn_failed_ps, avg_ptr->num_conn_failed, DT_RATE); 
      PARENT_COPY_SOCKET_AVG2RTG(rtg_ptr->conn_failed_tot, cavg_ptr->num_cum_conn_failed, DT_CUM); 

      PARENT_COPY_SOCKET_AVG2RTG(rtg_ptr->conn_close_ps, avg_ptr->num_conn_closed, DT_RATE); 
      PARENT_COPY_SOCKET_AVG2RTG(rtg_ptr->conn_close_tot, cavg_ptr->num_cum_conn_closed, DT_CUM); 

      PARENT_COPY_SOCKET_AVG2RTG(rtg_ptr->conn_close_ps_by_server_ps, avg_ptr->num_conn_closed_by_server, DT_RATE); 
      //PARENT_COPY_SOCKET_AVG2RTG(rtg_ptr->conn_close_ps_by_server_tot, cavg_ptr->num_cum_conn_closed_by_server, DT_CUM); 

      PARENT_COPY_SOCKET_AVG2RTG_TIMES(rtg_ptr->dns_time, avg_ptr->dns_time, DT_TIMES);
      PARENT_COPY_SOCKET_AVG2RTG_TIMES(rtg_ptr->conn_time, avg_ptr->conn_time, DT_TIMES);
      PARENT_COPY_SOCKET_AVG2RTG_TIMES(rtg_ptr->ssl_time, avg_ptr->ssl_time, DT_TIMES);
 
      PARENT_COPY_SOCKET_AVG2RTG(rtg_ptr->send_ps, avg_ptr->num_send, DT_RATE); 
      PARENT_COPY_SOCKET_AVG2RTG(rtg_ptr->send_tot, cavg_ptr->num_cum_sent, DT_CUM); 

      PARENT_COPY_SOCKET_AVG2RTG(rtg_ptr->send_failed_ps, avg_ptr->num_send_failed, DT_RATE); 
      PARENT_COPY_SOCKET_AVG2RTG(rtg_ptr->send_failed_tot, cavg_ptr->num_cum_send_failed, DT_CUM); 

      PARENT_COPY_SOCKET_AVG2RTG_TIMES(rtg_ptr->send_time, avg_ptr->send_time, DT_TIMES);

      PARENT_COPY_SOCKET_AVG2RTG(rtg_ptr->recv_ps, avg_ptr->num_recv, DT_RATE); 
      PARENT_COPY_SOCKET_AVG2RTG(rtg_ptr->recv_tot, cavg_ptr->num_cum_recv, DT_CUM); 

      PARENT_COPY_SOCKET_AVG2RTG(rtg_ptr->recv_failed_ps, avg_ptr->num_recv_failed, DT_RATE); 
      PARENT_COPY_SOCKET_AVG2RTG(rtg_ptr->recv_failed_tot, cavg_ptr->num_cum_recv_failed, DT_CUM); 

      PARENT_COPY_SOCKET_AVG2RTG(rtg_ptr->recv_msg_ps, avg_ptr->num_recv_msg, DT_RATE); 
      //PARENT_COPY_SOCKET_AVG2RTG(rtg_ptr->recv_msg_tot, cavg_ptr->num_cum_recv_msg, DT_CUM); 

      PARENT_COPY_SOCKET_AVG2RTG_TIMES(rtg_ptr->recv_fb_time, avg_ptr->recv_fb_time, DT_TIMES);
      PARENT_COPY_SOCKET_AVG2RTG_TIMES(rtg_ptr->recv_time, avg_ptr->recv_time, DT_TIMES);

      PARENT_COPY_SOCKET_AVG2RTG(rtg_ptr->tot_bytes_sent_ps, avg_ptr->tot_bytes_sent, DT_THROUGHPUT); 
      PARENT_COPY_SOCKET_AVG2RTG(rtg_ptr->tot_bytes_recv_ps, avg_ptr->tot_bytes_recv, DT_THROUGHPUT); 

      graph_idx = 0;  // Reset graph_idx so that fill for next SGRP group
      rtg_ptr++;      // Increment rtg pointer
    }
  } 
}


void print_udp_client_throughput(FILE *fp1, FILE *fp2, int is_periodic, avgtime *avg, cavgtime *cavg)
{
  double udp_rx, udp_tx;
  double duration;

  u_ns_8B_t  tot_udp_rx = 0, tot_udp_tx = 0;
  double con_made_rate, con_break_rate;
  double hit_tot_rate, hit_succ_rate, hit_initited_rate; 
  u_ns_8B_t num_completed, num_initiated, num_succ;

  SocketClientAvgTime *avg_ptr = NULL;
  SocketClientCAvgTime *cavg_ptr = NULL;

  NSDL2_WS(NULL, NULL, "Method called, protocol_enabled = [%x], is_periodic = %d", 
                        global_settings->protocol_enabled, is_periodic);

  if(!IS_UDP_CLIENT_API_EXIST) 
    return; 

  avg_ptr = (SocketClientAvgTime *)((char *)avg + g_udp_client_avg_idx);
  cavg_ptr = (SocketClientCAvgTime *)((char *)cavg + g_udp_client_cavg_idx);

  if (is_periodic) 
  {
    tot_udp_rx = avg_ptr->tot_bytes_recv;
    tot_udp_tx = avg_ptr->tot_bytes_sent;
    
    duration = (double)((double)(global_settings->progress_secs)/1000.0);
    udp_rx = ((double)tot_udp_rx)/(duration*128.0);
    udp_tx = ((double)tot_udp_tx)/(duration*128.0);

    con_made_rate = ((double)(avg_ptr->num_conn_open * 1000))/global_settings->progress_secs;
    con_break_rate = ((double)(avg_ptr->num_conn_closed * 1000))/global_settings->progress_secs;

    num_initiated = avg_ptr->num_send;
    num_succ = avg_ptr->num_send - avg_ptr->num_send_failed;  //Success
    num_completed = avg_ptr->num_send;

    hit_tot_rate = ((double)(num_completed * 1000))/global_settings->progress_secs;
    hit_succ_rate =((double)(num_succ * 1000))/global_settings->progress_secs;
    hit_initited_rate = ((double)(num_initiated * 1000))/global_settings->progress_secs;

    NSDL2_MESSAGES(NULL, NULL, "In periodic, Progress Interval = %d, ws_num_initiated = %d, "
        "ws_num_succ = %d, ws_num_completed = %d", 
        global_settings->progress_secs, num_initiated, num_succ, num_completed); 
  }

  if(global_settings->show_initiated)
    fprint2f(fp1, fp2, "    UDP Client hit rate (per sec): Initiated=%.3f completed=%.3f Success=%.3f\n",
                          hit_initited_rate, hit_tot_rate, hit_succ_rate);
  else
    fprint2f(fp1, fp2, "    UDP Client hit rate (per sec): Total=%.3f Success=%.3f\n", hit_tot_rate, hit_succ_rate);

  fprint2f(fp1, fp2, "    UDP Client (Kbits/s): UDP(Rx=%'.3f Tx=%'.3f)\n",
                          udp_rx, udp_tx);

  fprint2f(fp1, fp2, "    UDP Client (Total Bytes): UDP(Rx=%'llu Tx=%'llu)\n",
                          tot_udp_rx, tot_udp_tx);

  fprint2f(fp1, fp2, "    UDP Client connections: Current=%llu Rate/s(Open=%.3f Close=%.3f) Total(Open=%'llu Close=%'llu)\n",
                          avg_ptr->num_conn_open,
                          con_made_rate,
                          con_break_rate,
                          cavg_ptr->num_cum_conn_open,
                          cavg_ptr->num_cum_conn_closed);
}

