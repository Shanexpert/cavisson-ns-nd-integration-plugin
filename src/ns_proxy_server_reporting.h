#ifndef NS_PROXY_SERVER_REPORTING_H 
#define NS_PROXY_SERVER_REPORTING_H

#include <stdio.h>
#include <stdlib.h>
#include "ns_data_types.h"
#include "ns_msg_def.h"
#include "ns_proxy_server.h"
#include "ns_global_settings.h"
#include "ns_log.h"
#include "ns_gdf.h"

#define RESET_PROXY_AVGTIME(a)\
{\
  if(IS_PROXY_ENABLED)\
  { \
    ProxyAvgTime *loc_proxy_avgtime = (ProxyAvgTime*)((char *)a + g_proxy_avgtime_idx); \
    loc_proxy_avgtime->http_proxy_inspected_requests = 0;\
    loc_proxy_avgtime->http_proxy_excp_requests = 0;\
    loc_proxy_avgtime->http_proxy_requests = 0;\
                                       \
    loc_proxy_avgtime->https_proxy_inspected_requests = 0;\
    loc_proxy_avgtime->https_proxy_excp_requests = 0;\
    loc_proxy_avgtime->https_proxy_requests = 0;\
                                       \
    loc_proxy_avgtime->connect_successful = 0;\
    loc_proxy_avgtime->connect_failure = 0;\
                                         \
    loc_proxy_avgtime->connect_success_response_time_min = MAX_VALUE_4B_U;\
    loc_proxy_avgtime->connect_success_response_time_max = 0;\
    loc_proxy_avgtime->connect_success_response_time_total = 0;\
    loc_proxy_avgtime->connect_failure_response_time_min = MAX_VALUE_4B_U;\
    loc_proxy_avgtime->connect_failure_response_time_max = 0;\
    loc_proxy_avgtime->connect_failure_response_time_total = 0;\
                                       \
    loc_proxy_avgtime->connect_1xx = 0;\
    loc_proxy_avgtime->connect_2xx = 0;\
    loc_proxy_avgtime->connect_3xx = 0;\
    loc_proxy_avgtime->connect_4xx = 0;\
    loc_proxy_avgtime->connect_5xx = 0;\
    loc_proxy_avgtime->connect_others = 0;\
    loc_proxy_avgtime->connect_confail = 0;\
    loc_proxy_avgtime->connect_TO = 0;\
                                       \
    loc_proxy_avgtime->proxy_auth_success = 0;\
    loc_proxy_avgtime->proxy_auth_failure = 0;\
  }\
}

#define ACC_PROXY_PERIODICS(a, b)\
                if(IS_PROXY_ENABLED) {\
                  (a)->http_proxy_inspected_requests += (b)->http_proxy_inspected_requests;\
                  (a)->http_proxy_excp_requests += (b)->http_proxy_excp_requests;\
                  (a)->http_proxy_requests += (b)->http_proxy_requests;\
                  (a)->tot_http_proxy_requests += (b)->tot_http_proxy_requests;\
                                                                                       \
                  (a)->https_proxy_inspected_requests += (b)->https_proxy_inspected_requests;\
                  (a)->https_proxy_excp_requests += (b)->https_proxy_excp_requests;\
                  (a)->https_proxy_requests += (b)->https_proxy_requests;\
                  (a)->tot_https_proxy_requests+= (b)->tot_https_proxy_requests;\
                                                                                       \
                  (a)->connect_successful += (b)->connect_successful ;\
                  (a)->connect_failure += (b)->connect_failure ;\
                                                                  \
                  (a)->connect_success_response_time_total += (b)->connect_success_response_time_total;\
                  (a)->connect_failure_response_time_total += (b)->connect_failure_response_time_total;\
                                                                                                        \
                  SET_MIN ((a)->connect_success_response_time_min, (b)->connect_success_response_time_min);\
                  SET_MAX ((a)->connect_success_response_time_max, (b)->connect_success_response_time_max);\
                                                                                     \
                  SET_MIN ((a)->connect_failure_response_time_min, (b)->connect_failure_response_time_min);\
                  SET_MAX ((a)->connect_failure_response_time_max, (b)->connect_failure_response_time_max);\
                                                                                     \
                  (a)->connect_1xx += (b)->connect_1xx;\
                  (a)->connect_2xx += (b)->connect_2xx;\
                  (a)->connect_3xx += (b)->connect_3xx;\
                  (a)->connect_4xx += (b)->connect_4xx;\
                  (a)->connect_5xx += (b)->connect_5xx;\
                  (a)->connect_others += (b)->connect_others;\
                  (a)->connect_confail += (b)->connect_confail;\
                  (a)->connect_TO += (b)->connect_TO;\
                                                      \
                  (a)->proxy_auth_success += (b)->proxy_auth_success;\
                  (a)->proxy_auth_failure += (b)->proxy_auth_failure;\
                }

/*Counters for Proxy */
typedef struct {
  u_ns_8B_t http_proxy_inspected_requests;
  u_ns_8B_t http_proxy_excp_requests;
  u_ns_8B_t http_proxy_requests;
  u_ns_8B_t tot_http_proxy_requests;  //Cum

  u_ns_8B_t https_proxy_inspected_requests;
  u_ns_8B_t https_proxy_excp_requests;
  u_ns_8B_t https_proxy_requests;
  u_ns_8B_t tot_https_proxy_requests;  //Cum

  u_ns_8B_t connect_successful;
  u_ns_8B_t connect_failure;

  //Times
  u_ns_4B_t connect_success_response_time_min;
  u_ns_4B_t connect_success_response_time_max;
  u_ns_8B_t connect_success_response_time_total;

  //Times
  u_ns_4B_t connect_failure_response_time_min;
  u_ns_4B_t connect_failure_response_time_max;
  u_ns_8B_t connect_failure_response_time_total;

  u_ns_8B_t connect_1xx;
  u_ns_8B_t connect_2xx;
  u_ns_8B_t connect_3xx;
  u_ns_8B_t connect_4xx;
  u_ns_8B_t connect_5xx;
  u_ns_8B_t connect_others;
  u_ns_8B_t connect_confail;
  u_ns_8B_t connect_TO;

  u_ns_8B_t proxy_auth_success;
  u_ns_8B_t proxy_auth_failure;
} ProxyAvgTime;

//For GUI
typedef struct {
  Long_data http_proxy_inspected_requests;
  Long_data http_proxy_excp_requests;
  Long_data http_proxy_requests;
  Long_data tot_http_proxy_requests;

  Long_data https_proxy_inspected_requests;
  Long_data https_proxy_excp_requests;
  Long_data https_proxy_requests;
  Long_data tot_https_proxy_requests;

  Long_data connect_successful;
  Long_data connect_failure;
  Times_data connect_success_response_time;
  Times_data connect_failure_response_time;

  Long_data connect_1xx;
  Long_data connect_2xx;
  Long_data connect_3xx;
  Long_data connect_4xx;
  Long_data connect_5xx;
  Long_data connect_others;
  Long_data connect_confail;
  Long_data connect_TO;

  Long_data proxy_auth_success;
  Long_data proxy_auth_failure;
} HttpProxy_gp;

typedef struct {
  u_ns_ts_t http_connect_start;
  u_ns_4B_t http_connect_success;
  u_ns_4B_t http_connect_failure; 
} Proxy_con_resp_time;

extern HttpProxy_gp *http_proxy_gp_ptr; 
extern unsigned int http_proxy_gp_idx;

#ifndef CAV_MAIN
extern int g_proxy_avgtime_idx;
extern ProxyAvgTime *proxy_avgtime;
#else
extern __thread int g_proxy_avgtime_idx;
extern __thread ProxyAvgTime *proxy_avgtime;
#endif
extern int g_proxy_cavgtime_idx;

extern inline void  update_proxy_avgtime_size();
extern inline void set_proxy_avgtime_ptr();
extern void proxy_print_progress_report(FILE *fp1, FILE *fp2, int is_periodic, avgtime *avg);
extern inline void fill_proxy_gp (avgtime **avg);

#endif
