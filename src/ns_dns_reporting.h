#ifndef NS_DNS_REPORTING
#define NS_DNS_REPORTING

#include "ns_data_types.h"
#include "ns_msg_def.h"
#include "ns_global_settings.h"
#include "ns_log.h"
#include "ns_gdf.h"

#define RESET_DNS_LOOKUP_STATS_AVGTIME(a)   \
{\
  if(IS_DNS_LOOKUP_STATS_ENABLED)\
  { \
    DnsLookupStatsAvgTime *loc_dns_lookup_stats_avgtime = (DnsLookupStatsAvgTime*)((char *)a + dns_lookup_stats_avgtime_idx); \
    loc_dns_lookup_stats_avgtime->dns_lookup_per_sec = 0;              \
    loc_dns_lookup_stats_avgtime->dns_failure_per_sec = 0;             \
    loc_dns_lookup_stats_avgtime->dns_from_cache_per_sec = 0;              \
                                            \
    loc_dns_lookup_stats_avgtime->dns_lookup_time_min = MAX_VALUE_4B_U;\
    loc_dns_lookup_stats_avgtime->dns_lookup_time_max = 0;             \
    loc_dns_lookup_stats_avgtime->dns_lookup_time_total = 0;           \
  }\
}

#define ACC_DNS_LOOKUP_STATS_PERIODICS(a, b)                     \
  if(IS_DNS_LOOKUP_STATS_ENABLED) {                              \
    (a)->dns_lookup_per_sec += (b)->dns_lookup_per_sec;          \
    (a)->dns_failure_per_sec += (b)->dns_failure_per_sec;        \
                                                                 \
    (a)->dns_from_cache_per_sec += (b)->dns_from_cache_per_sec;          \
                                                                 \
    (a)->total_dns_lookup_req += (b)->total_dns_lookup_req;      \
    (a)->dns_lookup_time_total += (b)->dns_lookup_time_total;\
                                                                 \
    SET_MIN ((a)->dns_lookup_time_min, (b)->dns_lookup_time_min);\
    SET_MAX ((a)->dns_lookup_time_max, (b)->dns_lookup_time_max);\
                                                                 \
  }

#define UPDATE_DNS_LOOKUP_TIME_COUNTERS(vptr, diff_time)   \
  if(IS_DNS_LOOKUP_STATS_ENABLED)                    \
  {                                                  \
    if(diff_time < dns_lookup_stats_avgtime->dns_lookup_time_min)\
    {                                                            \
      dns_lookup_stats_avgtime->dns_lookup_time_min = diff_time; \
    }                                                            \
    if(diff_time > dns_lookup_stats_avgtime->dns_lookup_time_max)\
    {                                                            \
      dns_lookup_stats_avgtime->dns_lookup_time_max = diff_time; \
    }                                                            \
    dns_lookup_stats_avgtime->dns_lookup_time_total += diff_time;\
    UPDATE_GRP_BASED_DNS_LOOKUP_TIME_COUNTERS(vptr, diff_time); \
  }

    
#define IS_DNS_LOOKUP_STATS_ENABLED \
  (global_settings->enable_dns_graph_mode == 0 || global_settings->enable_dns_graph_mode == 1)


#define INCREMENT_DNS_LOOKUP_CUM_SAMPLE_COUNTER(vptr)\
  if(IS_DNS_LOOKUP_STATS_ENABLED)   \
  {                                 \
    dns_lookup_stats_avgtime->dns_lookup_per_sec++;\
    dns_lookup_stats_avgtime->total_dns_lookup_req++;\
    INCREMENT_GRP_BASED_DNS_LOOKUP_CUM_SAMPLE_COUNTER(vptr); \
  }

#define INCREMENT_DNS_LOOKUP_FAILURE_COUNTER(vptr) \
  if(IS_DNS_LOOKUP_STATS_ENABLED) \
  {                               \
    dns_lookup_stats_avgtime->dns_failure_per_sec++;\
    INCREMENT_GRP_BASED_DNS_LOOKUP_FAILURE_COUNTER(vptr); \
  }

#define INCREMENT_DNS_LOOKUP_FROM_CACHE_COUNTER(vptr) \
  if(IS_DNS_LOOKUP_STATS_ENABLED) \
  {                               \
    dns_lookup_stats_avgtime->dns_from_cache_per_sec++;\
    INCREMENT_GRP_BASED_DNS_LOOKUP_FROM_CACHE_COUNTER(vptr); \
  }

/* Counters For DNS Lookup */
typedef struct DnsLookupStatsAvgTime {

  u_ns_4B_t dns_lookup_per_sec;                
  u_ns_4B_t dns_failure_per_sec;

  u_ns_4B_t dns_from_cache_per_sec;

  //Cum
  u_ns_8B_t total_dns_lookup_req;

  //Times
  u_ns_4B_t dns_lookup_time_min; 
  u_ns_4B_t dns_lookup_time_max; 
  u_ns_8B_t dns_lookup_time_total;

} DnsLookupStatsAvgTime;

//For GUI
typedef struct DnsLookupStats_gp{ 

  Long_data dns_lookup_ps;    
  Long_data dns_failure_ps;    

  Long_data dns_from_cache_ps;    

  Long_data total_dns_lookup_req;         

  Times_data dns_lookup_time;  // Response time when resolved or not 

} DnsLookupStats_gp;

extern DnsLookupStats_gp *dns_lookup_stats_gp_ptr; 
extern unsigned int dns_lookup_stats_gp_idx;

#ifndef CAV_MAIN
extern int dns_lookup_stats_avgtime_idx;
extern DnsLookupStatsAvgTime *dns_lookup_stats_avgtime;
#else
extern __thread int dns_lookup_stats_avgtime_idx;
extern __thread DnsLookupStatsAvgTime *dns_lookup_stats_avgtime;
#endif

extern inline void update_dns_lookup_stats_avgtime_size();
extern inline void set_dns_lookup_stats_avgtime_ptr();
extern void print_dns_lookup_stats_progress_report(FILE *fp1, FILE *fp2, int is_periodic, avgtime *avg);
extern inline void fill_dns_lookup_stats_gp(avgtime **avg);
extern int kw_set_enable_dns_lookup_graphs(char *buf); 
extern void dns_lookup_stats_init();
extern inline void dns_lookup_cache_init();
#endif
