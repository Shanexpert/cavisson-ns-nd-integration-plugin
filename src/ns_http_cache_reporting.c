#include <stdio.h>
#include <stdlib.h>
#include "ns_data_types.h"
#include "ns_msg_def.h"
#include "ns_http_cache_reporting.h"
#include "ns_global_settings.h"
#include "ns_log.h"


extern void fprint2f(FILE *fp1, FILE* fp2,char* format, ...);

/* Http Caching Graph info */
HttpCaching_gp *http_caching_gp_ptr = NULL;
unsigned int http_caching_gp_idx;

#ifndef CAV_MAIN
int g_cache_avgtime_idx = -1;
CacheAvgTime *cache_avgtime = NULL; //used to fill cache pointers 
#else
__thread int g_cache_avgtime_idx = -1;
__thread CacheAvgTime *cache_avgtime = NULL; //used to fill cache pointers 
#endif 

inline void cache_set_cache_avgtime_ptr() {

  NSDL2_CACHE(NULL, NULL, "Method Called");

  if(global_settings->protocol_enabled & HTTP_CACHE_ENABLED) {
    NSDL2_CACHE(NULL, NULL, "HTTP Caching is enabled.");
   /* We have allocated average_time with the size of CacheAvgTime
    * also now we can point that using g_cache_avgtime_idx*/ 
    cache_avgtime = (CacheAvgTime*)((char *)average_time + g_cache_avgtime_idx);
  } else {
    NSDL2_CACHE(NULL, NULL, "HTTP Caching is disabled.");
    cache_avgtime = NULL;
  }

  NSDL2_CACHE(NULL, NULL, "cache_avgtime = %p", cache_avgtime);
}


// Add size of Cache if it is enabled 
inline void cache_update_cache_avgtime_size() {
 
  NSDL2_CACHE(NULL, NULL, "Method Called, g_avgtime_size = %d, g_cache_avgtime_idx = %d",
					  g_avgtime_size, g_cache_avgtime_idx);
  
  if(global_settings->protocol_enabled & HTTP_CACHE_ENABLED) {
    NSDL2_CACHE(NULL, NULL, "HTTP Caching is enabled.");
    g_cache_avgtime_idx = g_avgtime_size;
    g_avgtime_size +=  sizeof(CacheAvgTime);

    //g_cache_cavgtime_idx = g_cavgtime_size;
    //g_cavgtime_size +=  sizeof(CacheCAvgTime);
  } else {
    NSDL2_CACHE(NULL, NULL, "HTTP Caching is disabled.");
  }

  NSDL2_CACHE(NULL, NULL, "After g_avgtime_size = %d, g_cache_avgtime_idx = %d",
					  g_avgtime_size, g_cache_avgtime_idx);
}

void cache_print_progress_report(FILE *fp1, FILE *fp2, int is_periodic, avgtime *avg) {

  u_ns_8B_t cache_num_succ = 0;
  u_ns_8B_t cache_num_completed = 0;
  //u_ns_8B_t cache_num_samples = 0;
  double cache_hit_tot_rate = 0;
  double cache_hit_succ_rate = 0;
  double cache_missed_rate = 0;
  u_ns_8B_t cache_num_entries = 0;
  double cache_revalidation_not_modified = 0;
  double cache_revalidation_success = 0;
  double cache_revalidation_errors = 0;
  /*Time related */
  double cache_search_url_time_min = 0;
  double cache_search_url_time_max = 0;
  double cache_search_url_time_avg = 0;
  double cache_add_url_time_min = 0;
  double cache_add_url_time_max = 0;
  double cache_add_url_time_avg = 0;
  CacheAvgTime* cache_avgtime_local = NULL;

  NSDL2_CACHE(NULL, NULL, "Method Called, is_periodic = %d");
  
  if(global_settings->protocol_enabled & HTTP_CACHE_ENABLED) {
    cache_avgtime_local = (CacheAvgTime*)((char*)avg + g_cache_avgtime_idx);

    cache_num_completed = cache_avgtime_local->cache_num_tries; 
    if(cache_num_completed) {
      //cache_num_samples = cache_num_succ = cache_avgtime_local->cache_num_hits;
      cache_num_succ = cache_avgtime_local->cache_num_hits;
 
      cache_hit_tot_rate = (cache_num_completed * 1000)/(double)global_settings->progress_secs;
      cache_hit_succ_rate = (cache_num_succ * 1000)/(double)global_settings->progress_secs;
      cache_missed_rate = (cache_avgtime_local->cache_num_missed * 1000)/(double)global_settings->progress_secs;
      cache_num_entries = cache_avgtime_local->cache_num_entries; 
      cache_revalidation_not_modified = (cache_avgtime_local->cache_revalidation_not_modified * 100)/(double)global_settings->progress_secs;
      cache_revalidation_success = (cache_avgtime_local->cache_revalidation_success * 100)/(double)global_settings->progress_secs;
      cache_revalidation_errors = (cache_avgtime_local->cache_revalidation_errors * 100)/(double)global_settings->progress_secs;

      // Min and max are in milli-seconds. So we need to divide by 1000 to convert to seconds
      cache_search_url_time_min = (cache_avgtime_local->cache_search_url_time_min)/(double)(1000);
      cache_search_url_time_max = (cache_avgtime_local->cache_search_url_time_max)/(double)(1000);
      cache_add_url_time_min = (cache_avgtime_local->cache_add_url_time_min == MAX_VALUE_4B_U)?0:cache_avgtime_local->cache_add_url_time_min/(double)(1000);
      cache_add_url_time_max = (cache_avgtime_local->cache_add_url_time_max)/(double)(1000);
      
      // time_total has total time in milli-seconds. So we need to divide by num completed * 1000 to convert to avg in seconds
      cache_search_url_time_avg = (cache_avgtime_local->cache_search_url_time_total)/(double)(cache_num_completed * 1000);
      cache_add_url_time_avg = (cache_avgtime_local->cache_add_url_time_total)/(double)(cache_num_completed * 1000);
    
      fprint2f(fp1, fp2, "    Cache hit rate (per sec): Total=%'.3f Success=%'.3f Missed=%'.3f\n"
                         "    Cache revalidation (per sec): Not Modified=%'.3f Success=%'.3f Errors=%'.3f\n"
                         "    Cache num entries=%'llu\n" 
                         "    Cache Search/Period: min %'.3f  avg %'.3f max %'.3f    Cache Add/Period: min %'.3f  avg %'.3f max %'.3f\n",
	                 cache_hit_tot_rate, cache_hit_succ_rate, cache_missed_rate,
	  	         cache_revalidation_not_modified, cache_revalidation_success, cache_revalidation_errors,
		         cache_num_entries, cache_search_url_time_min, cache_search_url_time_avg, cache_search_url_time_max, 
                         cache_add_url_time_min, cache_add_url_time_avg, cache_add_url_time_max); 
    }
  } else {
    NSDL2_CACHE(NULL, NULL, "Http Caching is not enabled, hence not showing into progress report");
    return;
  }
}
