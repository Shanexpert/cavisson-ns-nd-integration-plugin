#include <stdio.h>
#include <stdlib.h>
#include "ns_proxy_server_reporting.h"
#include "netomni/src/core/ni_scenario_distribution.h"
#include "ns_group_data.h"
#include "ns_gdf.h"
#include "util.h"

extern void fprint2f(FILE *fp1, FILE* fp2,char* format, ...);

/* Http Caching Graph info */
HttpProxy_gp *http_proxy_gp_ptr = NULL;
unsigned int http_proxy_gp_idx;

#ifndef CAV_MAIN
int g_proxy_avgtime_idx = -1;
ProxyAvgTime *proxy_avgtime = NULL; //used to fill proxy pointers 
#else
__thread int g_proxy_avgtime_idx = -1;
__thread ProxyAvgTime *proxy_avgtime = NULL; //used to fill proxy pointers 
#endif
int g_proxy_cavgtime_idx = -1;

inline void set_proxy_avgtime_ptr() {

  NSDL2_PROXY(NULL, NULL, "Method Called");

  if(IS_PROXY_ENABLED) {
    NSDL2_PROXY(NULL, NULL, "HTTP Proxy is enabled. g_proxy_avgtime_idx=%d", g_proxy_avgtime_idx);
   /* We have allocated average_time with the size of ProxyAvgTime
    * also now we can point that using g_proxy_avgtime_idx*/ 
    proxy_avgtime = (ProxyAvgTime*)((char *)average_time + g_proxy_avgtime_idx);
  } else {
    NSDL2_PROXY(NULL, NULL, "HTTP Proxy is disabled.");
    proxy_avgtime = NULL;
  }

  NSDL2_PROXY(NULL, NULL, "proxy_avgtime = %p", proxy_avgtime);
}


// Add size of Proxy if it is enabled 
inline void update_proxy_avgtime_size() {
 
  NSDL2_PROXY(NULL, NULL, "Method Called, g_avgtime_size = %d, g_proxy_avgtime_idx = %d",
					  g_avgtime_size, g_proxy_avgtime_idx);
  
  if(IS_PROXY_ENABLED){
    NSDL2_PROXY(NULL, NULL, "HTTP Proxy is enabled.");
    g_proxy_avgtime_idx = g_avgtime_size;
    g_avgtime_size +=  sizeof(ProxyAvgTime);
  } else {
    NSDL2_PROXY(NULL, NULL, "HTTP Proxy is disabled.");
  }

  NSDL2_PROXY(NULL, NULL, "After g_avgtime_size = %d, g_proxy_avgtime_idx = %d",
					  g_avgtime_size, g_proxy_avgtime_idx);
}


void proxy_print_progress_report(FILE *fp1, FILE *fp2, int is_periodic, avgtime *avg) {

   double http_proxy_inspected_requests = 0;
   double http_proxy_excp_requests = 0;
   double http_proxy_requests = 0;
   u_ns_8B_t tot_http_proxy_requests = 0;

   double https_proxy_inspected_requests = 0;
   double https_proxy_excp_requests = 0;
   double https_proxy_requests = 0;
   u_ns_8B_t tot_https_proxy_requests = 0;

   double connect_successful = 0;
   double connect_failure = 0;

   double connect_success_response_time_max = 0;
   double connect_success_response_time_min = 0;
   double connect_success_response_time_avg = 0;

   double connect_failure_response_time_max = 0;
   double connect_failure_response_time_min = 0;
   double connect_failure_response_time_avg = 0;
/*
   double connect_1xx = 0;
   double connect_2xx = 0;
   double connect_3xx = 0;
   double connect_4xx = 0;
   double connect_5xx = 0;
   double connect_others = 0;
   double connect_confail = 0;
   double connect_TO = 0;

Note: removing warning: these variable are set but not used thats why commenting them
*/
   double proxy_auth_success = 0;
   double proxy_auth_failure = 0;

  ProxyAvgTime* proxy_avgtime_local = NULL;

  NSDL2_PROXY(NULL, NULL, "Method Called, is_periodic = %d");
  
   proxy_avgtime_local = (ProxyAvgTime*)((char*)avg + g_proxy_avgtime_idx);

   http_proxy_inspected_requests = (proxy_avgtime_local->http_proxy_inspected_requests * 1000)/(double)global_settings->progress_secs;
   http_proxy_excp_requests = (proxy_avgtime_local->http_proxy_excp_requests * 1000)/(double)global_settings->progress_secs;
   http_proxy_requests = (proxy_avgtime_local->http_proxy_requests * 1000)/(double)global_settings->progress_secs;
   tot_http_proxy_requests = (proxy_avgtime_local->tot_http_proxy_requests);

   https_proxy_inspected_requests = (proxy_avgtime_local->https_proxy_inspected_requests * 1000)/(double)global_settings->progress_secs;
   https_proxy_excp_requests = (proxy_avgtime_local->https_proxy_excp_requests * 1000)/(double)global_settings->progress_secs;
   https_proxy_requests = (proxy_avgtime_local->https_proxy_requests * 1000)/(double)global_settings->progress_secs;
   tot_https_proxy_requests = (proxy_avgtime_local->tot_https_proxy_requests);

   connect_successful = (proxy_avgtime_local->connect_successful * 1000)/(double)global_settings->progress_secs;
   connect_failure = (proxy_avgtime_local->connect_failure * 1000)/(double)global_settings->progress_secs;

   // Min and max are in milli-seconds. So we need to divide by 1000 to convert to seconds
   connect_success_response_time_min = (proxy_avgtime_local->connect_success_response_time_min == MAX_VALUE_4B_U)?0:proxy_avgtime_local->connect_success_response_time_min/(double)(1000);
   connect_success_response_time_max = (proxy_avgtime_local->connect_success_response_time_max)/(double)(1000);
   // time_total has total time in milli-seconds. So we need to divide by num completed * 1000 to convert to avg in seconds
   NSDL2_PROXY(NULL, NULL, "connect_success_response_time_total=%ld, connect_success_response_time_min=%d", proxy_avgtime_local->connect_success_response_time_total, proxy_avgtime_local->connect_success_response_time_min);
   if(proxy_avgtime_local->connect_successful)
     connect_success_response_time_avg = (proxy_avgtime_local->connect_success_response_time_total)/(double)(proxy_avgtime_local->connect_successful * 1000);

   // Min and max are in milli-seconds. So we need to divide by 1000 to convert to seconds
   connect_failure_response_time_min = (proxy_avgtime_local->connect_failure_response_time_min == MAX_VALUE_4B_U)?0:proxy_avgtime_local->connect_failure_response_time_min/(double)(1000);
   connect_failure_response_time_max = (proxy_avgtime_local->connect_failure_response_time_max)/(double)(1000);
   // time_total has total time in milli-seconds. So we need to divide by num completed * 1000 to convert to avg in seconds
   if(proxy_avgtime_local->connect_failure)
     connect_failure_response_time_avg = (proxy_avgtime_local->connect_failure_response_time_total)/(double)(proxy_avgtime_local->connect_failure * 1000);
/*
   connect_1xx = (proxy_avgtime_local->connect_1xx * 1000)/(double)global_settings->progress_secs;
   connect_2xx = (proxy_avgtime_local->connect_2xx * 1000)/(double)global_settings->progress_secs;
   connect_3xx = (proxy_avgtime_local->connect_3xx * 1000)/(double)global_settings->progress_secs;
   connect_4xx = (proxy_avgtime_local->connect_4xx * 1000)/(double)global_settings->progress_secs;
   connect_5xx = (proxy_avgtime_local->connect_5xx * 1000)/(double)global_settings->progress_secs;
   connect_others = (proxy_avgtime_local->connect_others * 1000)/(double)global_settings->progress_secs;
   connect_confail = (proxy_avgtime_local->connect_confail * 1000)/(double)global_settings->progress_secs;
   connect_TO = (proxy_avgtime_local->connect_TO * 1000)/(double)global_settings->progress_secs;

Note: removing warning: these variable are set but not used thats why commenting them
*/

   proxy_auth_success = (proxy_avgtime_local->proxy_auth_success * 1000)/(double)global_settings->progress_secs;
   proxy_auth_failure = (proxy_avgtime_local->proxy_auth_failure * 1000)/(double)global_settings->progress_secs;

   fprint2f(fp1, fp2, "    HTTP Proxy rate (per sec): Requests Inspected=%'.3f Exceptions=%'.3f Actual Hit=%'.3f\n"
                      "    HTTPS Proxy rate (per sec): Requests inspected=%'.3f Exceptions=%'.3f Actual Hit=%'.3f\n"
                      "    Total HTTP Proxy Requests=%'llu Total HTTPS Proxy Requests=%'llu\n" 
                      "    Connect Status (per sec): Success=%'.3f Failure=%'.3f\n"
                      "    Connect Status: Success/Period: min %'.3f  avg %'.3f max %'.3f  Failure/Period: min %'.3f avg %'.3f max %'.3f\n"
                      "    Proxy Authentication (per sec): Success=%'.3f Failure=%'.3f\n",
	                 http_proxy_inspected_requests, http_proxy_excp_requests, http_proxy_requests,
	                 https_proxy_inspected_requests, https_proxy_excp_requests, https_proxy_requests,
                         tot_http_proxy_requests, tot_https_proxy_requests, 
                         connect_successful, connect_failure,
                         connect_success_response_time_min, connect_success_response_time_avg, connect_success_response_time_max ,
                         connect_failure_response_time_min, connect_failure_response_time_avg, connect_failure_response_time_max ,
                         proxy_auth_success, proxy_auth_failure);
}

inline void fill_proxy_gp (avgtime **g_avg) {
  int g_idx = 0, v_idx, grp_idx;
  ProxyAvgTime *proxy_avg = NULL;
  Long_data connect_success_response_time_avg=0;
  Long_data connect_failure_response_time_avg=0;
  avgtime *avg = NULL;  
  HttpProxy_gp *http_proxy_local_gp_ptr = http_proxy_gp_ptr;

  if(http_proxy_local_gp_ptr == NULL) 
    return;

  NSDL2_GDF(NULL, NULL, "Method called");
  
  for(v_idx = 0; v_idx <sgrp_used_genrator_entries + 1; v_idx++)
  {
    avg = (avgtime *)g_avg[v_idx];
    for(grp_idx = 0; grp_idx < TOTAL_GRP_ENTERIES_WITH_GRP_KW; grp_idx++)
    {
      proxy_avg = (ProxyAvgTime *) ((char *)((char *)avg + (grp_idx * g_avg_size_only_grp)) + g_proxy_avgtime_idx);

      GDF_COPY_VECTOR_DATA(http_proxy_gp_idx, g_idx, v_idx, 0,
                           convert_long_long_data_to_ps_long_long((proxy_avg->http_proxy_inspected_requests)),
                           http_proxy_local_gp_ptr->http_proxy_inspected_requests); g_idx++;
    
      GDF_COPY_VECTOR_DATA(http_proxy_gp_idx, g_idx, v_idx, 0,
                           convert_long_long_data_to_ps_long_long((proxy_avg->http_proxy_excp_requests)),
                           http_proxy_local_gp_ptr->http_proxy_excp_requests); g_idx++;
    
      GDF_COPY_VECTOR_DATA(http_proxy_gp_idx, g_idx, v_idx, 0,
                           convert_long_long_data_to_ps_long_long((proxy_avg->http_proxy_requests)),
                           http_proxy_local_gp_ptr->http_proxy_requests); g_idx++;
    
      GDF_COPY_VECTOR_DATA(http_proxy_gp_idx, g_idx, v_idx, 0,
                           proxy_avg->tot_http_proxy_requests, http_proxy_local_gp_ptr->tot_http_proxy_requests); g_idx++;
    
      GDF_COPY_VECTOR_DATA(http_proxy_gp_idx, g_idx, v_idx, 0,
                           convert_long_long_data_to_ps_long_long((proxy_avg->https_proxy_inspected_requests)),
                           http_proxy_local_gp_ptr->https_proxy_inspected_requests); g_idx++;
    
      GDF_COPY_VECTOR_DATA(http_proxy_gp_idx, g_idx, v_idx, 0,
                           convert_long_long_data_to_ps_long_long((proxy_avg->https_proxy_excp_requests)),
                           http_proxy_local_gp_ptr->https_proxy_excp_requests); g_idx++;
    
      GDF_COPY_VECTOR_DATA(http_proxy_gp_idx, g_idx, v_idx, 0,
                           convert_long_long_data_to_ps_long_long((proxy_avg->https_proxy_requests)),
                           http_proxy_local_gp_ptr->https_proxy_requests); g_idx++;
    
      GDF_COPY_VECTOR_DATA(http_proxy_gp_idx, g_idx, v_idx, 0,
                           proxy_avg->tot_https_proxy_requests, http_proxy_local_gp_ptr->tot_https_proxy_requests); g_idx++;
    
      GDF_COPY_VECTOR_DATA(http_proxy_gp_idx, g_idx, v_idx, 0,
                           convert_long_long_data_to_ps_long_long((proxy_avg->connect_successful)),
                           http_proxy_local_gp_ptr->connect_successful); g_idx++;
    
      GDF_COPY_VECTOR_DATA(http_proxy_gp_idx, g_idx, v_idx, 0,
                           convert_long_long_data_to_ps_long_long((proxy_avg->connect_failure)),
                           http_proxy_local_gp_ptr->connect_failure); g_idx++;
     
      if(proxy_avg->connect_successful)
        connect_success_response_time_avg = (Long_data )proxy_avg->connect_success_response_time_total/((Long_data )proxy_avg->connect_successful * 1000.0);
      proxy_avg->connect_success_response_time_min = (proxy_avg->connect_success_response_time_min == MAX_VALUE_4B_U)?0:proxy_avg->connect_success_response_time_min;
      GDF_COPY_TIMES_VECTOR_DATA(http_proxy_gp_idx, g_idx, v_idx, 0,
                                 connect_success_response_time_avg, proxy_avg->connect_success_response_time_min/1000.0, proxy_avg->connect_success_response_time_max/1000.0, proxy_avg->connect_successful,
                                 http_proxy_local_gp_ptr->connect_success_response_time.avg_time,
                                 http_proxy_local_gp_ptr->connect_success_response_time.min_time,
                                 http_proxy_local_gp_ptr->connect_success_response_time.max_time,
                                 http_proxy_local_gp_ptr->connect_success_response_time.succ); g_idx++;
    
      if(proxy_avg->connect_failure)
        connect_failure_response_time_avg = (Long_data )proxy_avg->connect_failure_response_time_total/((Long_data )proxy_avg->connect_failure * 1000.0);
      proxy_avg->connect_failure_response_time_min = (proxy_avg->connect_failure_response_time_min == MAX_VALUE_4B_U)?0:proxy_avg->connect_failure_response_time_min;
      GDF_COPY_TIMES_VECTOR_DATA(http_proxy_gp_idx, g_idx, v_idx, 0,
                                 connect_failure_response_time_avg, proxy_avg->connect_failure_response_time_min/1000.0, proxy_avg->connect_failure_response_time_max/1000.0, proxy_avg->connect_failure,
                                 http_proxy_local_gp_ptr->connect_failure_response_time.avg_time,
                                 http_proxy_local_gp_ptr->connect_failure_response_time.min_time,
                                 http_proxy_local_gp_ptr->connect_failure_response_time.max_time,
                                 http_proxy_local_gp_ptr->connect_failure_response_time.succ); g_idx++;
    
    
      GDF_COPY_VECTOR_DATA(http_proxy_gp_idx, g_idx, v_idx, 0,
                           convert_long_long_data_to_ps_long_long((proxy_avg->connect_1xx)),
                           http_proxy_local_gp_ptr->connect_1xx); g_idx++;
    
      GDF_COPY_VECTOR_DATA(http_proxy_gp_idx, g_idx, v_idx, 0,
                           convert_long_long_data_to_ps_long_long((proxy_avg->connect_2xx)),
                           http_proxy_local_gp_ptr->connect_2xx); g_idx++;
    
      GDF_COPY_VECTOR_DATA(http_proxy_gp_idx, g_idx, v_idx, 0,
                           convert_long_long_data_to_ps_long_long((proxy_avg->connect_3xx)),
                           http_proxy_local_gp_ptr->connect_3xx); g_idx++;
    
      GDF_COPY_VECTOR_DATA(http_proxy_gp_idx, g_idx, v_idx, 0,
                           convert_long_long_data_to_ps_long_long((proxy_avg->connect_4xx)),
                           http_proxy_local_gp_ptr->connect_4xx); g_idx++;
    
      GDF_COPY_VECTOR_DATA(http_proxy_gp_idx, g_idx, v_idx, 0,
                           convert_long_long_data_to_ps_long_long((proxy_avg->connect_5xx)),
                           http_proxy_local_gp_ptr->connect_5xx); g_idx++;
    
      GDF_COPY_VECTOR_DATA(http_proxy_gp_idx, g_idx, v_idx, 0,
                           convert_long_long_data_to_ps_long_long((proxy_avg->connect_others)),
                           http_proxy_local_gp_ptr->connect_others); g_idx++;
    
      GDF_COPY_VECTOR_DATA(http_proxy_gp_idx, g_idx, v_idx, 0,
                           convert_long_long_data_to_ps_long_long((proxy_avg->connect_confail)),
                           http_proxy_local_gp_ptr->connect_confail); g_idx++;
    
      GDF_COPY_VECTOR_DATA(http_proxy_gp_idx, g_idx, v_idx, 0,
                           convert_long_long_data_to_ps_long_long((proxy_avg->connect_TO)),
                           http_proxy_local_gp_ptr->connect_TO); g_idx++;
    
      GDF_COPY_VECTOR_DATA(http_proxy_gp_idx, g_idx, v_idx, 0,
                           convert_long_long_data_to_ps_long_long((proxy_avg->proxy_auth_success)),
                           http_proxy_local_gp_ptr->proxy_auth_success); g_idx++;
    
      GDF_COPY_VECTOR_DATA(http_proxy_gp_idx, g_idx, v_idx, 0,
                           convert_long_long_data_to_ps_long_long((proxy_avg->proxy_auth_failure)),
                           http_proxy_local_gp_ptr->proxy_auth_failure); g_idx++;
      g_idx = 0;
      http_proxy_local_gp_ptr++;
    }
  }
}

