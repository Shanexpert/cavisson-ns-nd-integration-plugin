#ifndef NS_SOCKET_UDP_CLIENT_RPT_H
#define NS_SOCKET_UDP_CLIENT_RPT_H 

#include "ns_socket_tcp_client_failures_rpt.h"

// UDP Client RTG (Graph Data) 
// Taking UPD RTG structure different form UDP as it may be possible
// There are different Graphs for UDP and UPD Client
typedef struct 
{
  // Connection Matrices
  Long_data conn_init_ps;                         //Graph(1): UDP Client Connections Initiated/Sec  
  Long_data conn_init_tot;                        //Graph(2): UDP Client Total Connections Initiated 
  Long_data conn_open_ps;                         //Graph(3): UDP Client Connections Opened/Sec 
  Long_data conn_open_tot;                        //Graph(4): UDP Client Total Connections Opened
  Long_data conn_failed_ps;                       //Graph(5): UDP Client Connections Failed/Sec
  Long_data conn_failed_tot;                      //Graph(6): UDP Client Total Connections Failed
  Long_data conn_close_ps;                        //Graph(7): UDP Client Connections Closed/Sec
  Long_data conn_close_tot;                       //Graph(8): UDP Client Total Connections Closed
  Long_data conn_close_ps_by_server_ps;           //Graph(9): UDP Client Connections Closed By Server/Sec

  Times_data dns_time;                            //Graph(10): UDP Client DNS Time (ms)
  Times_data conn_time;                           //Graph(11): UDP Client Connect Time (ms)
  Times_data ssl_time;                            //Graph(12): UDP Client SSL Time (ms)

  // Sent Matrices
  Long_data send_ps;                              //Graph(13): UDP Client Messages Sent/Sec
  Long_data send_tot;                             //Graph(14): UDP Client Total Messages Sent
  Long_data send_failed_ps;                       //Graph(15): UDP Client Send Failures/Sec
  Long_data send_failed_tot;                      //Graph(16): UDP Client Total Send Failures

  Times_data send_time;                           //Graph(17): UDP Client Send Time (ms)

  // Read Matrices
  Long_data recv_ps;                              //Graph(18): UDP Client Received Calls/Sec
  Long_data recv_tot;                             //Graph(19): UDP Client Total Received Calls
  Long_data recv_failed_ps;                       //Graph(20): UDP Client Receive Failures/Sec
  Long_data recv_failed_tot;                      //Graph(21): UDP Client Total Receive Failures
  Long_data recv_msg_ps;                          //Graph(22): UDP Client Messages Received/Sec

  Times_data recv_fb_time;                        //Graph(23): UDP Client First Byte Time (ms)
  Times_data recv_time;                           //Graph(24): UDP Client Receive Time (ms)

  // Sent/Read Throughput 
  Long_data tot_bytes_sent_ps;                    //Graph(25): UDP Client Send Throughput (Kbps)
  Long_data tot_bytes_recv_ps;                    //Graph(26): UDP Client Receive Throughput (Kbps) 
} UDPClientRTGData;


//Copy Progress Report data on Parent, 
// @src_avg  => incomig progress report data (child avgtime data) 
// @dst_avg  => parent avgtime memory 
// @dst_cavg => pareent cavgtime memory 
#define PARENT_COPY_SOCKET_UDP_CLIENT_PR(dst_avg, dst_cavg, src_avg)                       \
{                                                                                          \
  if (IS_UDP_CLIENT_API_EXIST)                                                             \
  {                                                                                        \
    NSDL2_SOCKETS(NULL, NULL, "[SocketStats] Copy avgtime data from child to parents - "   \
        "src_avg = %p, g_udp_client_avg_idx = %d, dst_avg = %p, dst_cavg = %p, "           \
        "g_udp_client_cavg_idx = %d",                                                      \
         src_avg, g_udp_client_avg_idx, dst_avg, dst_cavg, g_udp_client_cavg_idx);         \
                                                                                           \
    /* Copy TCP Clinet PERIODIC data i.e. avgtime data  */                                 \
    SocketClientAvgTime *dp, *s;                                                           \
    s = (SocketClientAvgTime *)((char *)src_avg + g_udp_client_avg_idx);                   \
                                                                                           \
    dp = (SocketClientAvgTime *)((char *)dst_avg + g_udp_client_avg_idx);                  \
    PARENT_COPY_SOCKET_PERIODIC_DATA(dp, s);                                               \
                                                                                           \
    /* Copy TCP Client CUMULATIVE data i.e. cavgtime data */                               \
    SocketClientCAvgTime *dc;                                                              \
    dc = (SocketClientCAvgTime *)((char *)dst_cavg + g_udp_client_cavg_idx);               \
    PARENT_COPY_SOCKET_CUMULATIVE_DATA(dc, s);                                             \
  }                                                                                        \
}

#define FILL_UDP_CLIENT_AVG                                                                \
{                                                                                          \
  if (IS_UDP_CLIENT_API_EXIST)                                                             \
  {                                                                                        \
    fill_udp_client_gp(g_avg, g_cavg);                                                     \
    fill_udp_client_failures_gp(g_avg, g_cavg);                                            \
  }                                                                                        \
}

#define UPDATE_AVG_SIZE4UDP_CLIENT_FAILURES                                                \
{                                                                                          \
  if (IS_UDP_CLIENT_API_EXIST)                                                             \
  {                                                                                        \
    update_avgtime_size_for_udp_client_failures();                                         \
    update_cavgtime_size_for_udp_client_failures();                                        \
  }                                                                                        \
}
 
// Expose fun and variable to others
#ifndef CAV_MAIN
extern int g_udp_client_avg_idx;
extern SocketClientAvgTime *g_udp_client_avg;
#else
extern __thread int g_udp_client_avg_idx;
extern __thread SocketClientAvgTime *g_udp_client_avg;
#endif
extern int g_udp_client_cavg_idx;
extern SocketClientCAvgTime *g_udp_client_cavg;
extern int g_udp_client_rpt_group_idx;
extern UDPClientRTGData *g_udp_client_rtg_ptr;

extern void update_avgtime_size4socket_udp_client();
extern void update_cavgtime_size4socket_udp_client();

extern void set_avgtime_ptr4socket_udp_client();

extern void fill_udp_client_avg(VUser *vptr, unsigned char metric, u_ns_8B_t value);
extern void fill_udp_client_gp (avgtime **g_avg, cavgtime **g_cavg);

#endif
