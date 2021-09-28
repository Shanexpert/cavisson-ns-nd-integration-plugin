#ifndef NS_SOCKET_UDP_CLIENT_FAILURES_RPT_H
#define NS_SOCKET_UDP_CLIENT_FAILURES_RPT_H 

#include "ns_socket.h"


#define UDPClientFailureAvgTime  TCPClientFailureAvgTime 
#define UDPClientFailureCAvgTime TCPClientFailureCAvgTime 
#define UDPClientFailureRTGData  TCPClientFailureRTGData 

#define PARENT_COPY_UDP_CLIENT_FAILURES_PERIODIC_DATA(a, b, nvmidx, num_entries)           \
{                                                                                          \
  int i, norm_id;                                                                          \
  for(i = 0; i < num_entries;  i++)                                                        \
  {                                                                                        \
    norm_id = g_udp_clinet_errs_loc2normtbl[nvmidx].loc2norm[i];                           \
    if (norm_id >= 0)                                                                      \
      a[norm_id].num_failures += b[i].num_failures;                                        \
    NSDL2_SOCKETS(NULL, NULL, "[SocketStats-UDPClientFailures]: i = %d, norm_id = %d, "    \
        "b[i].num_failures = %d, a[norm_id].num_failures = %d, "                           \
        "avg_ptr = %p", i, norm_id, b[i].num_failures, a[norm_id].num_failures, a);        \
  }                                                                                        \
}

#define PARENT_COPY_UDP_CLIENT_FAILURES_CUMULATIVE_DATA(a, b, nvmidx, num_entries)         \
{                                                                                          \
  int i, norm_id;                                                                          \
  for(i = 0; i < num_entries;  i++)                                                        \
  {                                                                                        \
    norm_id = g_udp_clinet_errs_loc2normtbl[nvmidx].loc2norm[i];                           \
    if (norm_id >= 0)                                                                      \
      a[norm_id].cum_num_failures += b[i].num_failures;                                    \
    NSDL2_SOCKETS(NULL, NULL, "[SocketStats-UDPClientFailures]: i = %d, norm_id = %d, "    \
        "b[i].num_failures = %d, a[norm_id].cum_num_failures = %d, cavg_ptr = %p",         \
         i, norm_id, b[i].num_failures, a[norm_id].cum_num_failures, a);                   \
  }                                                                                        \
}

#define PARENT_COPY_UDP_CLIENT_FAILURES_PERIODIC_DATA_CUR2NEXT(a, b, start_idx, total_idx) \
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
#define PARENT_COPY_UDP_CLIENT_FAILURES_PR(dst_avg, dst_cavg, src_avg, nvmidx, num_entries)\
{                                                                                          \
  if (IS_UDP_CLIENT_API_EXIST)                                                             \
  {                                                                                        \
    int src_avg_idx = g_udp_clinet_errs_loc2normtbl[nvmidx].avg_idx;                       \
                                                                                           \
    NSDL2_SOCKETS(NULL, NULL, "[SocketStats-UDPClientFailure] "                            \
        "Copy avgtime data from child to parents - "                                       \
        "src_avg = %p, src_avg_idx = %d, dst_avg = %p, dst_cavg = %p, "                    \
        "g_udp_client_failures_avg_idx = %d, num_entries = %d",                            \
         src_avg, src_avg_idx, dst_avg, dst_cavg,                                          \
         g_udp_client_failures_avg_idx, num_entries);                                      \
                                                                                           \
    /* Copy UDP Clinet PERIODIC data i.e. avgtime data  */                                 \
    UDPClientFailureAvgTime *dp, *s;                                                       \
    s = (UDPClientFailureAvgTime *)((char *)src_avg + src_avg_idx);                        \
                                                                                           \
    dp = (UDPClientFailureAvgTime *)((char *)dst_avg + g_udp_client_failures_avg_idx);     \
    PARENT_COPY_UDP_CLIENT_FAILURES_PERIODIC_DATA(dp, s, nvmidx, num_entries);             \
                                                                                           \
    /* Copy UDP Client CUMULATIVE data i.e. cavgtime data */                               \
    UDPClientFailureCAvgTime *dc;                                                          \
    dc = (UDPClientFailureCAvgTime *)((char *)dst_cavg +                                   \
             g_udp_client_failures_cavg_idx);                                              \
                                                                                           \
    PARENT_COPY_UDP_CLIENT_FAILURES_CUMULATIVE_DATA(dc, s, nvmidx, num_entries);           \
  }                                                                                        \
}


//After sending PR by Child reset all variables 
#define CHILD_RESET_UDP_CLIENT_FAILURES_AVGTIME(a)                                         \
{                                                                                          \
  if (IS_UDP_CLIENT_API_EXIST)                                                             \
  {                                                                                        \
    UDPClientFailureAvgTime *socket_avg;                                                   \
    socket_avg = (UDPClientFailureAvgTime *)((char*)a + g_udp_client_failures_avg_idx);    \
                                                                                           \
    int i;                                                                                 \
    for(i = 0; i < g_total_udp_client_errs;  i++)                                          \
    {                                                                                      \
      socket_avg->num_failures = 0;                                                        \
    }                                                                                      \
  }                                                                                        \
}


// Extern here 
extern int g_max_total_udp_client_errs;
extern Local2Norm *g_udp_clinet_errs_loc2normtbl;
#ifndef CAV_MAIN
extern int g_udp_client_failures_avg_idx;
extern NormObjKey g_udp_client_errs_normtbl;
extern int g_total_udp_client_errs;
extern UDPClientFailureAvgTime *g_udp_client_failures_avg;
#else
extern __thread int g_udp_client_failures_avg_idx;
extern __thread NormObjKey g_udp_client_errs_normtbl;
extern __thread int g_total_udp_client_errs;
extern __thread UDPClientFailureAvgTime *g_udp_client_failures_avg;
#endif
extern int g_udp_client_failures_cavg_idx;
extern UDPClientFailureCAvgTime *g_udp_client_failures_cavg;

extern int g_udp_clinet_failures_rpt_group_idx;
extern UDPClientFailureRTGData *g_udp_clinet_failures_rtg_ptr;

extern void create_udp_client_errs_loc2normtbl(int entries);
extern void update_avgtime_size_for_udp_client_failures();
extern void set_udp_client_failures_avg_ptr();
extern void set_and_move_below_udp_client_failure_avg_ptr(avgtime *avgtime_ptr, int updated_avg_sz, int avgtime_inc_sz, int update_idx, int nvm_id);
extern int ns_add_dyn_udp_client_failures(short nvmindex, int local_norm_id, char *error_msg, short error_len, int *flag_new);
extern void fill_udp_client_failure_avg(VUser *vptr, int err_code); 
extern char **print_udp_client_failures_grp_vectors();
extern void fill_udp_client_failures_gp(avgtime **g_avg, cavgtime **g_cavg);
extern void update_cavgtime_size_for_udp_client_failures();
extern void print_udp_client_throughput(FILE *fp1, FILE *fp2, int is_periodic, avgtime *avg, cavgtime *cavg);


#endif 
