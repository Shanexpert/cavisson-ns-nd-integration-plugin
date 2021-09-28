#ifndef NS_SOCKET_TCP_CLIENT_FAILURES_RPT_H
#define NS_SOCKET_TCP_CLIENT_FAILURES_RPT_H 

#include "ns_socket.h"

#define  INIT_SOCKET_MAX_ERRORS                               64    
#define  DELTA_TCP_CLIENT_ERR_ENTRIES                         8
#define  DELTA_TCP_CLIENT_ERR_LOC2NORM_SIZE                   5

#define  LOC2NORM_NON_CUMM_IDX                                0
#define  LOC2NORM_CUMM_IDX                                    1

typedef struct 
{
  int loc2norm_size;   
  int *loc2norm;
  int avg_idx;          
  int tot_entries;
  int tot_dyn_entries[2];                    //[0]-non cummulative  [1]-cummulative
  int last_gen_local_norm_id[2];             //[0]-non cummulative  [1]-cummulative
}Local2Norm;

//TCP Client Failure
typedef struct 
{
  Long_data failure_ps;
  Long_data failure_tot;
}TCPClientFailureRTGData;

// Socket Clinet (TCP/UDP) Stats, Average Times periodic 
typedef struct 
{
  u_ns_4B_t num_failures;          
} TCPClientFailureAvgTime;  

// Socket Stats, CAvgTime - Cummulative 
typedef struct 
{
  u_ns_4B_t cum_num_failures;          
} TCPClientFailureCAvgTime;


#define PARENT_COPY_TCP_CLIENT_FAILURES_PERIODIC_DATA(a, b, nvmidx, num_entries)           \
{                                                                                          \
  int i, norm_id;                                                                          \
  for(i = 0; i < num_entries;  i++)                                                        \
  {                                                                                        \
    norm_id = g_tcp_clinet_errs_loc2normtbl[nvmidx].loc2norm[i];                           \
    if (norm_id >= 0)                                                                      \
      a[norm_id].num_failures += b[i].num_failures;                                        \
    NSDL2_SOCKETS(NULL, NULL, "[SocketStats-TCPClientFailures]: i = %d, norm_id = %d, "    \
        "b[i].num_failures = %d, a[norm_id].num_failures = %d, "                           \
        "avg_ptr = %p", i, norm_id, b[i].num_failures, a[norm_id].num_failures, a);        \
  }                                                                                        \
}

#define PARENT_COPY_TCP_CLIENT_FAILURES_CUMULATIVE_DATA(a, b, nvmidx, num_entries)         \
{                                                                                          \
  int i, norm_id;                                                                          \
  for(i = 0; i < num_entries;  i++)                                                        \
  {                                                                                        \
    norm_id = g_tcp_clinet_errs_loc2normtbl[nvmidx].loc2norm[i];                           \
    if (norm_id >= 0)                                                                      \
      a[norm_id].cum_num_failures += b[i].num_failures;                                    \
    NSDL2_SOCKETS(NULL, NULL, "[SocketStats-TCPClientFailures]: i = %d, norm_id = %d, "    \
        "b[i].num_failures = %d, a[norm_id].cum_num_failures = %d, cavg_ptr = %p",         \
         i, norm_id, b[i].num_failures, a[norm_id].cum_num_failures, a);                   \
  }                                                                                        \
}

#define PARENT_COPY_TCP_CLIENT_FAILURES_PERIODIC_DATA_CUR2NEXT(a, b, start_idx, total_idx) \
{                                                                                          \
  for (i = start_idx ; i < total_idx; i++)                                                 \
  {                                                                                        \
    a[i].num_failures += b[i].num_failures;                                                \
  }                                                                                        \
}

//Copy Progress Report data on Parent, 
// @src_avg  => incomig progress report data (child avgtime data) 
// @dst_avg  => parent avgtime memory 
// @dst_cavg => pareent cavgtime memory 
#define PARENT_COPY_TCP_CLIENT_FAILURES_PR(dst_avg, dst_cavg, src_avg, nvmidx, num_entries)\
{                                                                                          \
  if (IS_TCP_CLIENT_API_EXIST)                                                             \
  {                                                                                        \
    int src_avg_idx = g_tcp_clinet_errs_loc2normtbl[nvmidx].avg_idx;                       \
                                                                                           \
    NSDL2_SOCKETS(NULL, NULL, "[SocketStats-TCPClientFailure] "                            \
        "Copy avgtime data from child to parents - "                                       \
        "src_avg = %p, src_avg_idx = %d, dst_avg = %p, dst_cavg = %p, "                    \
        "g_tcp_client_failures_avg_idx = %d, num_entries = %d",                            \
         src_avg, src_avg_idx, dst_avg, dst_cavg,                                          \
         g_tcp_client_failures_avg_idx, num_entries);                                      \
                                                                                           \
    /* Copy TCP Clinet PERIODIC data i.e. avgtime data  */                                 \
    TCPClientFailureAvgTime *dp, *s;                                                       \
    s = (TCPClientFailureAvgTime *)((char *)src_avg + src_avg_idx);                        \
                                                                                           \
    dp = (TCPClientFailureAvgTime *)((char *)dst_avg + g_tcp_client_failures_avg_idx);     \
    PARENT_COPY_TCP_CLIENT_FAILURES_PERIODIC_DATA(dp, s, nvmidx, num_entries);             \
                                                                                           \
    /* Copy TCP Client CUMULATIVE data i.e. cavgtime data */                               \
    TCPClientFailureCAvgTime *dc;                                                          \
    dc = (TCPClientFailureCAvgTime *)((char *)dst_cavg +                                   \
             g_tcp_client_failures_cavg_idx);                                              \
                                                                                           \
    PARENT_COPY_TCP_CLIENT_FAILURES_CUMULATIVE_DATA(dc, s, nvmidx, num_entries);           \
  }                                                                                        \
}


//After sending PR by Child reset all variables 
#define CHILD_RESET_TCP_CLIENT_FAILURES_AVGTIME(a)                                         \
{                                                                                          \
  if (IS_TCP_CLIENT_API_EXIST)                                                             \
  {                                                                                        \
    TCPClientFailureAvgTime *socket_avg;                                                   \
    socket_avg = (TCPClientFailureAvgTime *)((char*)a + g_tcp_client_failures_avg_idx);    \
                                                                                           \
    int i;                                                                                 \
    for(i = 0; i < g_total_tcp_client_errs;  i++)                                          \
    {                                                                                      \
      socket_avg->num_failures = 0;                                                        \
    }                                                                                      \
  }                                                                                        \
}

#define FILL_TCP_UDP_CLIENT_AVG(vptr, metric, value)                                       \
{                                                                                          \
  if (IS_TCP_CLIENT_API_EXIST)                                                             \
  {                                                                                        \
    fill_tcp_client_avg(vptr, metric, value);                                              \
  }                                                                                        \
                                                                                           \
  if(IS_UDP_CLIENT_API_EXIST)                                                              \
  {                                                                                        \
    fill_udp_client_avg(vptr, metric, value);                                              \
  }                                                                                        \
}

// Extern here 
extern int g_max_total_tcp_client_errs;
extern Local2Norm *g_tcp_clinet_errs_loc2normtbl;

#ifndef CAV_MAIN
extern int g_tcp_client_failures_avg_idx;
extern NormObjKey g_tcp_client_errs_normtbl;
extern int g_total_tcp_client_errs;
extern TCPClientFailureAvgTime *g_tcp_client_failures_avg;
#else
extern __thread int g_tcp_client_failures_avg_idx;
extern __thread NormObjKey g_tcp_client_errs_normtbl;
extern __thread int g_total_tcp_client_errs;
extern __thread TCPClientFailureAvgTime *g_tcp_client_failures_avg;
#endif
extern int g_tcp_client_failures_cavg_idx;
extern TCPClientFailureCAvgTime *g_tcp_client_failures_cavg;

extern int g_tcp_clinet_failures_rpt_group_idx;
extern TCPClientFailureRTGData *g_tcp_clinet_failures_rtg_ptr;

extern void create_tcp_client_errs_loc2normtbl(int entries);
extern void update_avgtime_size_for_tcp_client_failures();
extern void set_tcp_client_failures_avg_ptr();
extern void set_and_move_below_tcp_client_failure_avg_ptr(avgtime *avgtime_ptr, int updated_avg_sz, int avgtime_inc_sz, int update_idx, int nvm_id);
extern int ns_add_dyn_tcp_client_failures(short nvmindex, int local_norm_id, char *error_msg, short error_len, int *flag_new);
extern void fill_tcp_client_failure_avg(VUser *vptr, int err_code); 
extern char **print_tcp_client_failures_grp_vectors();
extern void fill_tcp_client_failures_gp(avgtime **g_avg, cavgtime **g_cavg);
extern void update_cavgtime_size_for_tcp_client_failures();
extern void print_tcp_client_throughput(FILE *fp1, FILE *fp2, int is_periodic, avgtime *avg, cavgtime *cavg);


#endif 
