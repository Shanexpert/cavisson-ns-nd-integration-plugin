#ifndef NS_SOCKET_TCP_CLIENT_RPT_H
#define NS_SOCKET_TCP_CLIENT_RPT_H 

// TCP Client RTG (Graph Data) 
typedef struct 
{
  // Connection Matrices
  Long_data conn_init_ps;                         //Graph(1): TCP Client Connections Initiated/Sec  
  Long_data conn_init_tot;                        //Graph(2): TCP Client Total Connections Initiated 
  Long_data conn_open_ps;                         //Graph(3): TCP Client Connections Opened/Sec 
  Long_data conn_open_tot;                        //Graph(4): TCP Client Total Connections Opened
  Long_data conn_failed_ps;                       //Graph(5): TCP Client Connections Failed/Sec
  Long_data conn_failed_tot;                      //Graph(6): TCP Client Total Connections Failed
  Long_data conn_close_ps;                        //Graph(7): TCP Client Connections Closed/Sec
  Long_data conn_close_tot;                       //Graph(8): TCP Client Total Connections Closed
  Long_data conn_close_ps_by_server_ps;           //Graph(9): TCP Client Connections Closed By Server/Sec

  Times_data dns_time;                            //Graph(10): TCP Client DNS Time (ms)
  Times_data conn_time;                           //Graph(11): TCP Client Connect Time (ms)
  Times_data ssl_time;                            //Graph(12): TCP Client SSL Time (ms)

  // Sent Matrices
  Long_data send_ps;                              //Graph(13): TCP Client Messages Sent/Sec
  Long_data send_tot;                             //Graph(14): TCP Client Total Messages Sent
  Long_data send_failed_ps;                       //Graph(15): TCP Client Send Failures/Sec
  Long_data send_failed_tot;                      //Graph(16): TCP Client Total Send Failures

  Times_data send_time;                           //Graph(17): TCP Client Send Time (ms)

  // Read Matrices
  Long_data recv_ps;                              //Graph(18): TCP Client Received Calls/Sec
  Long_data recv_tot;                             //Graph(19): TCP Client Total Received Calls
  Long_data recv_failed_ps;                       //Graph(20): TCP Client Receive Failures/Sec
  Long_data recv_failed_tot;                      //Graph(21): TCP Client Total Receive Failures
  Long_data recv_msg_ps;                          //Graph(22): TCP Client Messages Received/Sec

  Times_data recv_fb_time;                        //Graph(23): TCP Client First Byte Time (ms)
  Times_data recv_time;                           //Graph(24): TCP Client Receive Time (ms)

  // Sent/Read Throughput 
  Long_data tot_bytes_sent_ps;                    //Graph(25): TCP Client Send Throughput (Kbps)
  Long_data tot_bytes_recv_ps;                    //Graph(26): TCP Client Receive Throughput (Kbps) 
} TCPClientRTGData;

typedef struct 
{
  u_ns_4B_t tot;                                  //Total time periodic time
  u_ns_4B_t min;                                  //Mixnum periodic time
  u_ns_4B_t max;                                  //Maxumum periodic time
  u_ns_4B_t count;                                //Number of sample in that period
}AvgTimesData_4BU;

// Socket Clinet (TCP/UDP) Stats, Average Times periodic 
typedef struct 
{
  u_ns_4B_t num_conn_init;          
  u_ns_4B_t num_conn_open;         
  u_ns_4B_t num_conn_failed;      
  u_ns_4B_t num_conn_closed;     
  u_ns_4B_t num_conn_closed_by_server;     

  AvgTimesData_4BU dns_time;

  AvgTimesData_4BU conn_time;

  AvgTimesData_4BU ssl_time;

  u_ns_4B_t num_send;
  u_ns_4B_t num_send_failed;

  AvgTimesData_4BU send_time;

  u_ns_8B_t num_recv;
  u_ns_8B_t num_recv_failed;
  u_ns_8B_t num_recv_msg;

  AvgTimesData_4BU recv_fb_time;

  AvgTimesData_4BU recv_time;

  u_ns_8B_t tot_bytes_sent;
  u_ns_8B_t tot_bytes_recv;

  //TODO: add socket connect, write and send error codes
} SocketClientAvgTime;  


// Socket Stats, CAvgTime - Cummulative 
typedef struct 
{
  u_ns_4B_t num_cum_conn_init;          
  u_ns_4B_t num_cum_conn_open;         
  u_ns_4B_t num_cum_conn_failed;      
  u_ns_4B_t num_cum_conn_closed;     

  u_ns_4B_t num_cum_sent;     
  u_ns_4B_t num_cum_send_failed;     
  u_ns_4B_t num_cum_recv;     
  u_ns_4B_t num_cum_recv_failed;     
} SocketClientCAvgTime;

// These macro will use to fill avgtime for socket API
typedef enum
{
  CON_INIT,
  CON_OPENED,
  CON_FAILED,
  CON_CLOSED,
  CON_CLOSED_BY_PEER,
  DNS_TIME,
  CON_TIME,
  SSL_TIME,
  NUM_SEND,
  NUM_SEND_FAILED,
  SEND_TIME,
  NUM_RECV,
  NUM_RECV_FAILED,
  NUM_RECV_MSG,
  RECV_FB_TIME,
  RECV_TIME,
  RECV_THROUGHPUT,
  SEND_THROUGHPUT    
}ns_socket_metrics;


#define UPDATE_AVGTIME_SIZE4SOCKET_TCP_UDP_CLIENT                                         \
{                                                                                         \
  update_avgtime_size4socket_tcp_client();                                                \
  update_cavgtime_size4socket_tcp_client();                                               \
                                                                                          \
  update_avgtime_size4socket_udp_client();                                                \
  update_cavgtime_size4socket_udp_client();                                               \
}

#define SET_AVGTIME_PTR4SOCKET_TCP_UDP_CLIENT	                                          \
{                                                                                         \
  set_avgtime_ptr4socket_tcp_client();                                                    \
  set_avgtime_ptr4socket_udp_client();                                                    \
}

//Main Graphs types - sample, cummulative, sum, rate, times
#define DT_SAMPLE                  0  
#define DT_CUM                     1 
#define DT_SUM                     2
#define DT_RATE                    3
#define DT_THROUGHPUT              4
#define DT_TIMES                   5 
#define DT_TIMES_STD               6

#define PARENT_COPY_SOCKET_AVG2RTG(dst, src, data_type)                                    \
{                                                                                          \
  double interval = global_settings->progress_secs;  /* Interval in ms*/                   \
  double sdata = src;                                /* Convert into Double data type */   \
                                                                                           \
  NSDL2_SOCKETS(NULL, NULL, "[SocketStats] Filling RTG for - "                             \
        "rtg_idx = %d, graph_idx = %d, group_vect_idx = %d, data_type = %d, "              \
        "interval = %f, sdata = %f",                                                       \
         rtg_idx, graph_idx, group_vect_idx, data_type, interval, sdata);                  \
                                                                                           \
  if(data_type == DT_RATE)                                                                 \
    sdata = sdata/(interval/1000);              /* Calculate rate */                       \
  else if(data_type == DT_THROUGHPUT)                                                      \
    sdata = sdata/((interval/1000) * 128);      /* Calculate throughput in Kbps */         \
                                                                                           \
  NSDL2_SOCKETS(NULL, NULL, "[SocketStats] Before filling RTG - graph_idx = %d, "          \
      "src{tot = %f}, dst{avg = %f}", graph_idx, sdata, dst);                              \
                                                                                           \
  GDF_COPY_VECTOR_DATA(rtg_idx, graph_idx, group_vect_idx, 0, sdata, dst);                 \
                                                                                           \
  NSDL2_SOCKETS(NULL, NULL, "[SocketStats] After filling RTG - graph_idx = %d, "           \
      "dst{avg = %f}", graph_idx, dst);                                                    \
                                                                                           \
  graph_idx++;                                                                             \
}

#define PARENT_COPY_SOCKET_AVG2RTG_TIMES(dst, src, data_type)                              \
{                                                                                          \
  double sdata = src.tot;                            /* Convert into Double data type */   \
                                                                                           \
  NSDL2_SOCKETS(NULL, NULL, "[SocketStats] Filling RTG for - "                             \
        "rtg_idx = %d, graph_idx = %d, group_vect_idx = %d, data_type = %d, sdata = %f",   \
         rtg_idx, graph_idx, group_vect_idx, data_type, sdata);                            \
                                                                                           \
  sdata = sdata/src.count;                       /* Calculate average time */              \
                                                                                           \
  NSDL2_SOCKETS(NULL, NULL, "[SocketStats] Before filling RTG - graph_idx = %d, "          \
      "src{tot = %f, min = %d, max = %d, count = %d}, "                                    \
      "dst{avg = %f, min = %f, max = %f, count = %f}",                                     \
       graph_idx, src.tot, src.min, src.max, src.count,                                    \
       dst.avg_time, dst.min_time, dst.max_time, dst.succ);                                \
                                                                                           \
  if(src.count > 0)                                                                        \
  {                                                                                        \
    GDF_COPY_TIMES_VECTOR_DATA(rtg_idx, graph_idx, group_vect_idx, 0,                      \
                               src.tot, src.min, src.max, src.count,                       \
                               dst.avg_time, dst.min_time, dst.max_time, dst.succ);        \
  }                                                                                        \
  else                                                                                     \
  {                                                                                        \
    GDF_COPY_TIMES_VECTOR_DATA(rtg_idx, graph_idx, group_vect_idx, 0,                      \
                               0, -1, 0, 0,                                                \
                               dst.avg_time, dst.min_time, dst.max_time, dst.succ);        \
  }                                                                                        \
                                                                                           \
  NSDL2_SOCKETS(NULL, NULL, "[SocketStats] After filling RTG - graph_idx = %d, "           \
      "dst{avg_time = %f, min_time = %f, max_time = %f, succ = %f}",                       \
       graph_idx, dst.avg_time, dst.min_time, dst.max_time, dst.succ);                     \
                                                                                           \
  graph_idx++;                                                                             \
}


#define PARENT_COPY_SOCKET_PERIODIC_DATA(a, b)                                             \
{                                                                                          \
  /* Accumulate data */                                                                    \
  (a)->num_conn_init += (b)->num_conn_init;                                                \
  (a)->num_conn_open += (b)->num_conn_open;                                                \
  (a)->num_conn_failed += (b)->num_conn_failed;                                            \
  (a)->num_conn_closed += (b)->num_conn_closed;                                            \
  (a)->num_conn_closed_by_server += (b)->num_conn_closed_by_server;                        \
                                                                                           \
  (a)->dns_time.tot += (b)->dns_time.tot;                                                  \
  SET_MIN ((a)->dns_time.min, (b)->dns_time.min);                                          \
  SET_MAX ((a)->dns_time.max, (b)->dns_time.max);                                          \
  (a)->dns_time.count += (b)->dns_time.count;                                              \
                                                                                           \
  (a)->conn_time.tot += (b)->conn_time.tot;                                                \
  SET_MIN ((a)->conn_time.min, (b)->conn_time.min);                                        \
  SET_MAX ((a)->conn_time.max, (b)->conn_time.max);                                        \
  (a)->conn_time.count += (b)->conn_time.count;                                            \
                                                                                           \
  (a)->ssl_time.tot += (b)->ssl_time.tot;                                                  \
  SET_MIN ((a)->ssl_time.min, (b)->ssl_time.min);                                          \
  SET_MAX ((a)->ssl_time.max, (b)->ssl_time.max);                                          \
  (a)->ssl_time.count += (b)->ssl_time.count;                                              \
                                                                                           \
  (a)->num_send += (b)->num_send;                                                          \
  (a)->num_send_failed += (b)->num_send_failed;                                            \
                                                                                           \
  (a)->send_time.tot += (b)->send_time.tot;                                                \
  SET_MIN ((a)->send_time.min, (b)->send_time.min);                                        \
  SET_MAX ((a)->send_time.max, (b)->send_time.max);                                        \
  (a)->send_time.count += (b)->send_time.count;                                            \
                                                                                           \
  (a)->num_recv += (b)->num_recv;                                                          \
  (a)->num_recv_failed += (b)->num_recv_failed;                                            \
  (a)->num_recv_msg += (b)->num_recv_msg;                                                  \
                                                                                           \
  (a)->recv_fb_time.tot += (b)->recv_fb_time.tot;                                          \
  SET_MIN ((a)->recv_fb_time.min, (b)->recv_fb_time.min);                                  \
  SET_MIN ((a)->recv_fb_time.max, (b)->recv_fb_time.max);                                  \
  (a)->recv_fb_time.count += (b)->recv_fb_time.count;                                      \
                                                                                           \
  (a)->recv_time.tot += (b)->recv_time.tot;                                                \
  SET_MIN ((a)->recv_time.min, (b)->recv_time.min);                                        \
  SET_MIN ((a)->recv_time.max, (b)->recv_time.max);                                        \
  (a)->recv_time.count += (b)->recv_time.count;                                            \
                                                                                           \
  (a)->tot_bytes_sent += (b)->tot_bytes_sent;                                              \
  (a)->tot_bytes_recv += (b)->tot_bytes_recv;                                              \
}

#define PARENT_COPY_SOCKET_CUMULATIVE_DATA(a, b)                                           \
{                                                                                          \
  /* Accumulate data */                                                                    \
  (a)->num_cum_conn_init += (b)->num_conn_init;                                            \
  (a)->num_cum_conn_open += (b)->num_conn_open;                                            \
  (a)->num_cum_conn_failed += (b)->num_conn_failed;                                        \
  (a)->num_cum_conn_closed += (b)->num_conn_closed;                                        \
  (a)->num_cum_sent += (b)->num_send;                                                      \
  (a)->num_cum_send_failed += (b)->num_send_failed;                                        \
  (a)->num_cum_recv += (b)->num_recv;                                                      \
  (a)->num_cum_recv_failed += (b)->num_recv_failed;                                        \
                                                                                           \
  NSDL2_SOCKETS(NULL, NULL, "[SocketStats] Copied avgtime data from child to parents - "   \
      "child{num_conn_init = %d, num_conn_open = %d, num_conn_failed = %d, "               \
      "num_conn_closed = %d, num_send = %d, num_send_failed = %d, num_recv = %d"           \
      "num_recv_failed = %d}, "                                                            \
      "parent{num_cum_conn_init = %d, num_cum_conn_open = %d, num_cum_conn_failed = %d, "  \
      "num_cum_conn_closed = %d, num_cum_sent = %d, num_cum_send_failed = %d, "            \
      "num_cum_recv = %d, num_cum_recv_failed = %d}",                                      \
       (b)->num_conn_init, (b)->num_conn_open, (b)->num_conn_failed, (b)->num_conn_closed, \
       (b)->num_send, (b)->num_send_failed, (b)->num_recv, (b)->num_recv_failed,           \
       (a)->num_cum_conn_init, (a)->num_cum_conn_open, (a)->num_cum_conn_failed,           \
       (a)->num_cum_conn_closed, (a)->num_cum_sent, (a)->num_cum_send_failed,              \
       (a)->num_cum_recv, (a)->num_cum_recv_failed);                                       \
}

#define PARENT_COPY_SOCKET_CUMULATIVE_DATA_NEXT2CUR(a, b)                                  \
{                                                                                          \
  /* Accumulate data */                                                                    \
  (a)->num_cum_conn_init += (b)->num_cum_conn_init;                                        \
  (a)->num_cum_conn_open += (b)->num_cum_conn_open;                                        \
  (a)->num_cum_conn_failed += (b)->num_cum_conn_failed;                                    \
  (a)->num_cum_conn_closed += (b)->num_cum_conn_closed;                                    \
  (a)->num_cum_sent += (b)->num_cum_sent;                                                  \
  (a)->num_cum_send_failed += (b)->num_cum_send_failed;                                    \
  (a)->num_cum_recv += (b)->num_cum_recv;                                                  \
  (a)->num_cum_recv_failed += (b)->num_cum_recv_failed;                                    \
}

//TODO: need to check why this is done for cumulative only ?
#define PARENT_COPY_SOCKET_TCP_UDP_CLIENT_CUMULATIVE_DATA_NEXT2CUR(a, b)                   \
{                                                                                          \
  if (IS_TCP_CLIENT_API_EXIST)                                                             \
  {                                                                                        \
    SocketClientCAvgTime *dc, *sc;                                                         \
    dc = (SocketClientCAvgTime *)((char *)a + g_tcp_client_cavg_idx);                      \
    sc = (SocketClientCAvgTime *)((char *)b + g_tcp_client_cavg_idx);                      \
    PARENT_COPY_SOCKET_CUMULATIVE_DATA_NEXT2CUR(dc, sc);                                   \
  }                                                                                        \
                                                                                           \
  if (IS_UDP_CLIENT_API_EXIST)                                                             \
  {                                                                                        \
    SocketClientCAvgTime *dc, *sc;                                                         \
    dc = (SocketClientCAvgTime *)((char *)a + g_udp_client_cavg_idx);                      \
    sc = (SocketClientCAvgTime *)((char *)b + g_udp_client_cavg_idx);                      \
    PARENT_COPY_SOCKET_CUMULATIVE_DATA_NEXT2CUR(dc, sc);                                   \
  }                                                                                        \
                                                                                           \
}

//Copy Progress Report data on Parent, 
// @src_avg  => incomig progress report data (child avgtime data) 
// @dst_avg  => parent avgtime memory 
// @dst_cavg => pareent cavgtime memory 
#define PARENT_COPY_SOCKET_TCP_CLIENT_PR(dst_avg, dst_cavg, src_avg)                       \
{                                                                                          \
  if (IS_TCP_CLIENT_API_EXIST)                                                             \
  {                                                                                        \
    NSDL2_SOCKETS(NULL, NULL, "[SocketStats] Copy avgtime data from child to parents - "   \
        "src_avg = %p, g_tcp_client_avg_idx = %d, dst_avg = %p, dst_cavg = %p, "           \
        "g_tcp_client_cavg_idx = %d",                                                      \
         src_avg, g_tcp_client_avg_idx, dst_avg, dst_cavg, g_tcp_client_cavg_idx);         \
                                                                                           \
    /* Copy TCP Clinet PERIODIC data i.e. avgtime data  */                                 \
    SocketClientAvgTime *dp, *s;                                                           \
    s = (SocketClientAvgTime *)((char *)src_avg + g_tcp_client_avg_idx);                   \
                                                                                           \
    dp = (SocketClientAvgTime *)((char *)dst_avg + g_tcp_client_avg_idx);                  \
    PARENT_COPY_SOCKET_PERIODIC_DATA(dp, s);                                               \
                                                                                           \
    /* Copy TCP Client CUMULATIVE data i.e. cavgtime data */                               \
    SocketClientCAvgTime *dc;                                                              \
    dc = (SocketClientCAvgTime *)((char *)dst_cavg + g_tcp_client_cavg_idx);               \
    PARENT_COPY_SOCKET_CUMULATIVE_DATA(dc, s);                                             \
  }                                                                                        \
}

#define PARENT_COPY_SOCKET_TCP_UDP_CLIENT_PR(dst_avg, dst_cavg, src_avg)                   \
{                                                                                          \
  PARENT_COPY_SOCKET_TCP_CLIENT_PR(dst_avg, dst_cavg, src_avg);                            \
  PARENT_COPY_SOCKET_UDP_CLIENT_PR(dst_avg, dst_cavg, src_avg);                            \
}

//After sending PR by Child reset all variables 
#define CHILD_RESET_SOCKET_CLIENT_AVGTIME(a)                                               \
{                                                                                          \
  (a)->num_conn_init = 0;                                                                  \
  (a)->num_conn_open = 0;                                                                  \
  (a)->num_conn_failed = 0;                                                                \
  (a)->num_conn_closed = 0;                                                                \
  (a)->num_conn_closed_by_server = 0;                                                      \
                                                                                           \
  (a)->dns_time.tot = 0;                                                                   \
  (a)->dns_time.min = MAX_VALUE_4B_U;                                                      \
  (a)->dns_time.max = 0;                                                                   \
  (a)->dns_time.count = 0;                                                                 \
                                                                                           \
  (a)->conn_time.tot = 0;                                                                  \
  (a)->conn_time.min = MAX_VALUE_4B_U;                                                     \
  (a)->conn_time.max = 0;                                                                  \
  (a)->conn_time.count = 0;                                                                \
                                                                                           \
  (a)->ssl_time.tot = 0;                                                                   \
  (a)->ssl_time.tot = MAX_VALUE_4B_U;                                                      \
  (a)->ssl_time.tot = 0;                                                                   \
  (a)->ssl_time.count = 0;                                                                 \
                                                                                           \
  (a)->num_send = 0;                                                                       \
  (a)->num_send_failed = 0;                                                                \
                                                                                           \
  (a)->send_time.tot = 0;                                                                  \
  (a)->send_time.min = MAX_VALUE_4B_U;                                                     \
  (a)->send_time.max = 0;                                                                  \
  (a)->send_time.count = 0;                                                                \
                                                                                           \
  (a)->num_recv = 0;                                                                       \
  (a)->num_recv_failed = 0;                                                                \
  (a)->num_recv_msg = 0;                                                                   \
                                                                                           \
  (a)->recv_fb_time.tot = 0;                                                               \
  (a)->recv_fb_time.min = MAX_VALUE_4B_U;                                                  \
  (a)->recv_fb_time.max = 0;                                                               \
  (a)->recv_fb_time.count = 0;                                                             \
                                                                                           \
  (a)->recv_time.tot = 0;                                                                  \
  (a)->recv_time.min = MAX_VALUE_4B_U;                                                     \
  (a)->recv_time.max = 0;                                                                  \
  (a)->recv_time.count = 0;                                                                \
                                                                                           \
  (a)->tot_bytes_sent = 0;                                                                 \
  (a)->tot_bytes_recv = 0;                                                                 \
}

#define CHILD_RESET_TCP_UDP_CLIENT_AVGTIME(a)                                              \
{                                                                                          \
  if (IS_TCP_CLIENT_API_EXIST)                                                             \
  {                                                                                        \
    SocketClientAvgTime *socket_avg;                                                       \
    socket_avg = (SocketClientAvgTime *)((char*)a + g_tcp_client_avg_idx);                 \
                                                                                           \
    CHILD_RESET_SOCKET_CLIENT_AVGTIME(socket_avg);                                         \
  }                                                                                        \
                                                                                           \
  if (IS_UDP_CLIENT_API_EXIST)                                                             \
  {                                                                                        \
    SocketClientAvgTime *socket_avg;                                                       \
    socket_avg = (SocketClientAvgTime *)((char*)a + g_udp_client_avg_idx);                 \
                                                                                           \
    CHILD_RESET_SOCKET_CLIENT_AVGTIME(socket_avg);                                         \
  }                                                                                        \
}

#define CHILD_FILL_SOCKET_AVGTIME(socket_avg, metric, value)                               \
{                                                                                          \
  switch(metric)                                                                           \
  {                                                                                        \
    case CON_INIT:                                                                         \
      /*++num_connections;  */                                                             \
      (socket_avg)->num_conn_init++;                                                       \
      break;                                                                               \
    case CON_OPENED:                                                                       \
      (socket_avg)->num_conn_open++;                                                       \
      break;                                                                               \
    case CON_FAILED:                                                                       \
      (socket_avg)->num_conn_failed++;                                                     \
      break;                                                                               \
    case CON_CLOSED:                                                                       \
      (socket_avg)->num_conn_closed++;                                                     \
      break;                                                                               \
    case CON_CLOSED_BY_PEER:                                                               \
      (socket_avg)->num_conn_closed_by_server++;                                           \
      break;                                                                               \
    case DNS_TIME:                                                                         \
      (socket_avg)->dns_time.tot += (u_ns_4B_t)value;                                      \
      SET_MIN((socket_avg)->dns_time.min, (u_ns_4B_t)value);                               \
      SET_MAX((socket_avg)->dns_time.max, (u_ns_4B_t)value);                               \
      (socket_avg)->dns_time.count++;                                                      \
      break;                                                                               \
    case CON_TIME:                                                                         \
      (socket_avg)->conn_time.tot += (u_ns_4B_t)value;                                     \
      SET_MIN((socket_avg)->conn_time.min, (u_ns_4B_t)value);                              \
      SET_MAX((socket_avg)->conn_time.max, (u_ns_4B_t)value);                              \
      (socket_avg)->conn_time.count++;                                                     \
      break;                                                                               \
    case SSL_TIME:                                                                         \
      (socket_avg)->ssl_time.tot += (u_ns_4B_t)value;                                      \
      SET_MIN((socket_avg)->ssl_time.min, (u_ns_4B_t)value);                               \
      SET_MAX((socket_avg)->ssl_time.max, (u_ns_4B_t)value);                               \
      (socket_avg)->ssl_time.count++;                                                      \
      break;                                                                               \
    case NUM_SEND:                                                                         \
      (socket_avg)->num_send++;                                                            \
      break;                                                                               \
    case NUM_SEND_FAILED:                                                                  \
      (socket_avg)->num_send_failed++;                                                     \
      break;                                                                               \
    case SEND_TIME:                                                                        \
      (socket_avg)->send_time.tot += (u_ns_4B_t)value;                                     \
      SET_MIN((socket_avg)->send_time.min, (u_ns_4B_t)value);                              \
      SET_MAX((socket_avg)->send_time.max, (u_ns_4B_t)value);                              \
      (socket_avg)->send_time.count++;                                                     \
      break;                                                                               \
    case NUM_RECV:                                                                         \
      (socket_avg)->num_recv++;                                                            \
      break;                                                                               \
    case NUM_RECV_FAILED:                                                                  \
      (socket_avg)->num_recv_failed++;                                                     \
      break;                                                                               \
    case NUM_RECV_MSG:                                                                     \
      (socket_avg)->num_recv_msg++;                                                        \
      break;                                                                               \
    case RECV_FB_TIME:                                                                     \
      (socket_avg)->recv_fb_time.tot += (u_ns_4B_t)value;                                  \
      SET_MIN((socket_avg)->recv_fb_time.min, (u_ns_4B_t)value);                           \
      SET_MAX((socket_avg)->recv_fb_time.max, (u_ns_4B_t)value);                           \
      (socket_avg)->recv_fb_time.count++;                                                  \
      break;                                                                               \
    case RECV_TIME:                                                                        \
      (socket_avg)->recv_time.tot += (u_ns_4B_t)value;                                     \
      SET_MIN((socket_avg)->recv_time.min, (u_ns_4B_t)value);                              \
      SET_MAX((socket_avg)->recv_time.max, (u_ns_4B_t)value);                              \
      (socket_avg)->recv_time.count++;                                                     \
      break;                                                                               \
    case RECV_THROUGHPUT:                                                                  \
      (socket_avg)->tot_bytes_sent += value;                                               \
      break;                                                                               \
    case SEND_THROUGHPUT:                                                                  \
      (socket_avg)->tot_bytes_recv += value;                                               \
      break;                                                                               \
  }                                                                                        \
}

#define FILL_TCP_CLIENT_AVG                                                                \
{                                                                                          \
  if (IS_TCP_CLIENT_API_EXIST)                                                             \
  {                                                                                        \
    fill_tcp_client_gp(g_avg, g_cavg);                                                     \
    fill_tcp_client_failures_gp(g_avg, g_cavg);                                            \
  }                                                                                        \
}

#define UPDATE_AVG_SIZE4TCP_CLIENT_FAILURES                                                \
{                                                                                          \
  if (IS_TCP_CLIENT_API_EXIST)                                                             \
  {                                                                                        \
    update_avgtime_size_for_tcp_client_failures();                                         \
    update_cavgtime_size_for_tcp_client_failures();                                        \
  }                                                                                        \
}
 
// Expose fun and variable to others
#ifndef CAV_MAIN
extern int g_tcp_client_avg_idx;
extern SocketClientAvgTime *g_tcp_client_avg;
#else
extern __thread int g_tcp_client_avg_idx;
extern __thread SocketClientAvgTime *g_tcp_client_avg;
#endif
extern int g_tcp_client_cavg_idx;
extern SocketClientCAvgTime *g_tcp_client_cavg;
extern int g_tcp_client_rpt_group_idx;
extern TCPClientRTGData *g_tcp_client_rtg_ptr;

extern void update_avgtime_size4socket_tcp_client();
extern void update_cavgtime_size4socket_tcp_client();

extern void set_avgtime_ptr4socket_tcp_client();
extern void ns_socket_client_init_avgtime(avgtime *avgbuf, int avg_idx);

extern void fill_tcp_client_avg(VUser *vptr, unsigned char metric, u_ns_8B_t value);
extern void fill_tcp_client_gp (avgtime **g_avg, cavgtime **g_cavg);

#endif
