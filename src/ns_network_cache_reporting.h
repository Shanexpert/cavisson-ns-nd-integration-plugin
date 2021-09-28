#ifndef NS_NETWORK_CACHE_REPORTING
#define NS_NETWORK_CACHE_REPORTING

#include "ns_data_types.h"
#include "ns_gdf.h"

#define MAX_FILE_DATA_LENGTH 4096


// Currently we are not using these bits, we are setting bits and just using HIT and MISS masks to increment their corresponding counters
#define TCP_HIT 				0x00000001
#define TCP_REFRESH_HIT_AS_HIT			0x00000002
#define TCP_IMS_HIT				0x00000004
#define TCP_NEGATIVE_HIT			0x00000008
#define TCP_MEM_HIT 				0x00000010
#define TCP_REFRESH_FAIL_HIT			0x00000020 // Same bit will be used for TCP_OFFLINE_HIT also 
#define HIT_FROM_CLOUD_FRONT                    0x00000040
#define TCP_HIT_MASK				0x000000FF

#define TCP_MISS 				0x00000100
#define TCP_REFRESH_MISS			0x00000200 // Same bit will be used for TCP_CLIENT_REFRESH_MISS and TCP_SWAPFAIL_MISS also	
#define MISS_FROM_CLOUD_FRONT                   0x00000400
#define TCP_REFRESH_HIT_AS_MISS			0x00000800
#define TCP_MISS_MASK                           0x00000F00

#define TCP_DENIED   				0x00001000
#define TCP_COOKIE_DENY    			0x00002000  
#define TCP_FAILURE_MASK			0x0000F000

#define NW_CACHE_STATS_CACHEABLE_UNKNOWN	0x00010000
#define NW_CACHE_STATS_CACHEABLE_YES		0x00020000
#define NW_CACHE_STATS_CACHEABLE_NO		0x00040000
#define NO_HEADER_RECIEVED			0x000FFFFF

#define TCP_REFRESH_HIT_AS_HIT_OR_MISS		0x00000802	

#define RESET_NETWORK_CACHE_STATS_AVGTIME(a)\
{\
  if(IS_NETWORK_CACHE_STATS_ENABLED) \
  { \
    NetworkCacheStatsAvgTime *loc_network_cache_stats_avgtime = (NetworkCacheStatsAvgTime*)((char*)a + g_network_cache_stats_avgtime_idx); \
    loc_network_cache_stats_avgtime->network_cache_stats_probe_req = 0;\
    loc_network_cache_stats_avgtime->non_network_cache_used_req = 0;\
    loc_network_cache_stats_avgtime->network_cache_stats_num_hits = 0;\
    loc_network_cache_stats_avgtime->network_cache_stats_num_misses = 0;\
    loc_network_cache_stats_avgtime->network_cache_stats_num_fail = 0;\
    loc_network_cache_stats_avgtime->network_cache_stats_state_others = 0;\
                                  \
    loc_network_cache_stats_avgtime->num_cacheable_requests = 0;\
    loc_network_cache_stats_avgtime->num_non_cacheable_requests = 0;\
                                       \
    loc_network_cache_stats_avgtime->network_cache_stats_hits_response_time_min = MAX_VALUE_4B_U;\
    loc_network_cache_stats_avgtime->network_cache_stats_hits_response_time_max = 0;\
    loc_network_cache_stats_avgtime->network_cache_stats_hits_response_time_total = 0;\
    loc_network_cache_stats_avgtime->network_cache_stats_miss_response_time_min = MAX_VALUE_4B_U;\
    loc_network_cache_stats_avgtime->network_cache_stats_miss_response_time_max = 0;\
    loc_network_cache_stats_avgtime->network_cache_stats_miss_response_time_total = 0;\
                                       \
    loc_network_cache_stats_avgtime->content_size_recv_from_cache = 0;\
    loc_network_cache_stats_avgtime->content_size_not_recv_from_cache = 0;\
    loc_network_cache_stats_avgtime->network_cache_refresh_hits = 0;\
  }\
}

#define ACC_NETWORK_CACHE_STATS_PERIODICS(a, b)\
  if(IS_NETWORK_CACHE_STATS_ENABLED) {\
    (a)->network_cache_stats_probe_req += (b)->network_cache_stats_probe_req;\
    (a)->non_network_cache_used_req += (b)->non_network_cache_used_req;\
    (a)->network_cache_stats_num_hits += (b)->network_cache_stats_num_hits;\
    (a)->network_cache_stats_num_misses += (b)->network_cache_stats_num_misses;\
    (a)->network_cache_stats_num_fail += (b)->network_cache_stats_num_fail;\
    (a)->network_cache_stats_state_others += (b)->network_cache_stats_state_others;\
                                                                       \
    (a)->num_cacheable_requests += (b)->num_cacheable_requests;\
    (a)->num_non_cacheable_requests += (b)->num_non_cacheable_requests;\
                                                                                          \
    (a)->network_cache_stats_hits_response_time_total += (b)->network_cache_stats_hits_response_time_total;\
    (a)->network_cache_stats_miss_response_time_total += (b)->network_cache_stats_miss_response_time_total;\
                                                                                         \
    SET_MIN ((a)->network_cache_stats_hits_response_time_min, (b)->network_cache_stats_hits_response_time_min);\
    SET_MAX ((a)->network_cache_stats_hits_response_time_max, (b)->network_cache_stats_hits_response_time_max);\
                                                                       \
    SET_MIN ((a)->network_cache_stats_miss_response_time_min, (b)->network_cache_stats_miss_response_time_min);\
    SET_MAX ((a)->network_cache_stats_miss_response_time_max, (b)->network_cache_stats_miss_response_time_max);\
                                                                       \
    (a)->content_size_recv_from_cache += (b)->content_size_recv_from_cache;\
    (a)->content_size_not_recv_from_cache += (b)->content_size_not_recv_from_cache;\
                                                                             \
    (a)->network_cache_refresh_hits += (b)->network_cache_refresh_hits;\
                                                                                   \
  }

#define UPDATE_HIT_RESP_TIME_AND_THROUGHPUT_COUNTERS(d_time, vptr)   \
  if(d_time < network_cache_stats_avgtime->network_cache_stats_hits_response_time_min) \
  {                                                                                   \
    network_cache_stats_avgtime->network_cache_stats_hits_response_time_min = d_time; \
  }                                                                                   \
  if(d_time > network_cache_stats_avgtime->network_cache_stats_hits_response_time_max)\
  {                                                                                   \
    network_cache_stats_avgtime->network_cache_stats_hits_response_time_max = d_time; \
  }                                                                                   \
  network_cache_stats_avgtime->network_cache_stats_hits_response_time_total += d_time;\
  network_cache_stats_avgtime->content_size_recv_from_cache += cptr->tcp_bytes_recv;  \
  GRP_BASED_UPDATE_HIT_RESP_TIME_AND_THROUGHPUT_COUNTERS(d_time, vptr);   
 
#define UPDATE_MISS_RESP_TIME_AND_THROUGHPUT_COUNTERS(d_time, vptr) \
  if(d_time < network_cache_stats_avgtime->network_cache_stats_miss_response_time_min)\
  {                                                                                   \
    network_cache_stats_avgtime->network_cache_stats_miss_response_time_min = d_time; \
  }                                                                                   \
                                                                                      \
  if(d_time > network_cache_stats_avgtime->network_cache_stats_miss_response_time_max)\
  {                                                                                   \
    network_cache_stats_avgtime->network_cache_stats_miss_response_time_max = d_time; \
  }                                                                                   \
  network_cache_stats_avgtime->network_cache_stats_miss_response_time_total += d_time;\
  network_cache_stats_avgtime->content_size_not_recv_from_cache += cptr->tcp_bytes_recv;\
  GRP_BASED_UPDATE_MISS_RESP_TIME_AND_THROUGHPUT_COUNTERS(d_time, vptr);

//Checks if G_ENABLE_NETWORK_CACHE_STATS keyword is enabled/disabled
#define IS_NETWORK_CACHE_STATS_ENABLED \
  (global_settings->protocol_enabled & NETWORK_CACHE_STATS_ENABLED)

#define IS_NETWORK_CACHE_STATS_ENABLED_FOR_GRP(group_num) \
  (runprof_table_shr_mem[group_num].gset.enable_network_cache_stats)

extern int network_cache_stats_header_buf_len;
extern char *network_cache_stats_header_buf_ptr;

/* Counters For Network Cache */
/* All are for the sample period. No cummulatives */
/* This struct must be multiple of 8 so that size is same on FC9 (32 bits) and FC14 (64 bits) */
typedef struct NetworkCacheStatsAvgTime {

  u_ns_4B_t network_cache_stats_probe_req;                /* Number of http requests with network cacheable requests */   
  u_ns_4B_t non_network_cache_used_req;

  u_ns_4B_t num_cacheable_requests;
  u_ns_4B_t num_non_cacheable_requests;
  u_ns_4B_t network_cache_stats_num_fail;               /* Number of network cache failures*/                   

  u_ns_4B_t network_cache_stats_num_hits;               /* Number of network cache hits */                                
  u_ns_4B_t network_cache_stats_num_misses;             /* Number of network cache  misses*/     
  u_ns_4B_t network_cache_stats_state_others;

  //Times
  u_ns_4B_t network_cache_stats_hits_response_time_min; 
  u_ns_4B_t network_cache_stats_hits_response_time_max; 
  u_ns_8B_t network_cache_stats_hits_response_time_total;

  //Times
  u_ns_4B_t network_cache_stats_miss_response_time_min; 
  u_ns_4B_t network_cache_stats_miss_response_time_max; 
  u_ns_8B_t network_cache_stats_miss_response_time_total; 

  u_ns_8B_t content_size_recv_from_cache;         /* Network cache content size received */
  u_ns_8B_t content_size_not_recv_from_cache;

  u_ns_4B_t network_cache_refresh_hits;
  u_ns_4B_t future;
} NetworkCacheStatsAvgTime;

        

//For GUI
typedef struct HttpNetworkCacheStats_gp{ 

  Long_data network_cache_stats_probe_req_ps;    // Request/Sec
  Long_data non_network_cache_used_req_ps;    // Origin Request/Sec

  Long_data num_cacheable_requests_ps;         // Cacheable/Sec
  Long_data num_non_cacheable_requests_ps;     // Non Cacheable/Sec
  Long_data network_cache_stats_num_fail_ps;      // Failures/Sec

  Long_data network_cache_stats_num_hits_ps;      // Hits/Sec
  Long_data network_cache_stats_num_misses_ps;      // Miss/Sec
  Long_data network_cache_stats_state_others_ps;     // Other/Sec

  Times_data network_cache_stats_hits_response_time;  // Response time when hit
  Times_data network_cache_stats_miss_response_time;  // Response time when miss

  Long_data content_size_recv_from_cache_ps;  // Cache Hit Recieve Throughput (kbps)
  Long_data content_size_not_recv_from_cache_ps; // Cache Miss Recieve Throughput (kbps)
 
  Long_data network_cache_stats_hits_percentage;
  Long_data network_cache_stats_miss_percentage;
  Long_data network_cache_stats_failure_percentage;
  //Long_data cacheable_requests_percentage;
  Long_data non_cacheable_requests_percentage;
  Long_data non_network_cache_used_req_percentage;
  Long_data network_cache_stats_state_others_percentage;

  Long_data network_cache_refresh_hits_ps;   

} HttpNetworkCacheStats_gp;

extern HttpNetworkCacheStats_gp *http_network_cache_stats_gp_ptr; 
extern unsigned int http_network_cache_stats_gp_idx;

#ifndef CAV_MAIN
extern int g_network_cache_stats_avgtime_idx;
extern NetworkCacheStatsAvgTime *network_cache_stats_avgtime;
#else
extern __thread int g_network_cache_stats_avgtime_idx;
extern __thread NetworkCacheStatsAvgTime *network_cache_stats_avgtime;
#endif
//extern int g_proxy_cavgtime_idx;

extern NetworkCacheStatsAvgTime *local_nw_cache_avg;

extern int network_cache_stats_header_buf_len;
extern char *network_cache_stats_header_buf_ptr; 

extern inline void update_nw_cache_stats_avgtime_size();
extern inline void set_nw_cache_stats_avgtime_ptr();
extern void print_nw_cache_stats_progress_report(FILE *fp1, FILE *fp2, int is_periodic, avgtime *avg);
extern inline void fill_nw_cache_stats_gp(avgtime **avg);
extern int kw_set_g_enable_network_cache_stats(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag);
//extern int proc_http_hdr_network_cache_stats(connection *cptr, char *header_buffer, int header_buffer_len, int *consumed_bytes, u_ns_ts_t now);
extern void network_cache_stats_init();

extern int proc_http_hdr_x_cache(void *cur_cptr, char *header_buffer, int header_buffer_len, int *consumed_bytes, u_ns_ts_t now);

extern int proc_http_hdr_x_cache_remote(void *cur_cptr, char *header_buffer, int header_buffer_len, int *consumed_bytes, u_ns_ts_t now);
extern int proc_http_hdr_x_check_cacheable(void *cur_cptr, char *header_buffer, int header_buffer_len, int *consumed_bytes, u_ns_ts_t now);
extern void nw_cache_stats_update_counter(void *cur_cptr, unsigned int download_time, int url_ok);
#endif
