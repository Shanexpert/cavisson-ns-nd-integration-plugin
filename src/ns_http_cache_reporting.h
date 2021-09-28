#ifndef NS_HTTP_CACHE_REPORTING_H 
#define NS_HTTP_CACHE_REPORTING_H

#include "ns_gdf.h"

#define RESET_CACHE_AVGTIME(a)\
{\
  if(global_settings->protocol_enabled & HTTP_CACHE_ENABLED)\
  { \
    CacheAvgTime *loc_cache_avgtime = (CacheAvgTime*)((char*)a + g_cache_avgtime_idx);\
    loc_cache_avgtime = (CacheAvgTime*)((char*)a + g_cache_avgtime_idx); \
    loc_cache_avgtime->cache_num_tries = 0;\
    loc_cache_avgtime->cache_num_hits = 0;\
    loc_cache_avgtime->cache_num_missed = 0;\
    loc_cache_avgtime->cache_bytes_hit = 0;\
    loc_cache_avgtime->cache_num_entries_replaced = 0;\
    loc_cache_avgtime->cache_num_entries_revalidation_ims = 0;\
    loc_cache_avgtime->cache_num_entries_revalidation_etag = 0;\
    loc_cache_avgtime->cache_revalidation_not_modified = 0;\
    loc_cache_avgtime->cache_revalidation_success = 0;\
    loc_cache_avgtime->cache_revalidation_errors = 0;\
    loc_cache_avgtime->cache_num_entries_cacheable = 0;\
    loc_cache_avgtime->cache_num_entries_non_cacheable = 0;\
    loc_cache_avgtime->cache_num_entries_collisions = 0;\
    loc_cache_avgtime->cache_num_error_entries_creations = 0;\
    loc_cache_avgtime->cache_search_url_time_min = MAX_VALUE_4B_U;\
    loc_cache_avgtime->cache_search_url_time_max = 0;\
    loc_cache_avgtime->cache_search_url_time_total = 0;\
    loc_cache_avgtime->cache_add_url_time_min = MAX_VALUE_4B_U;\
    loc_cache_avgtime->cache_add_url_time_max = 0;\
    loc_cache_avgtime->cache_add_url_time_total = 0;\
  }\
}

#define ACC_CACHE_PERIODICS(a, b)\
                if(global_settings->protocol_enabled & HTTP_CACHE_ENABLED) {\
                  (a)->cache_num_tries += (b)->cache_num_tries;\
                  (a)->cache_num_hits += (b)->cache_num_hits;\
                  (a)->cache_num_missed += (b)->cache_num_missed;\
                  (a)->cache_bytes_used += (b)->cache_bytes_used;\
                  (a)->cache_bytes_hit += (b)->cache_bytes_hit;\
                  (a)->cache_num_entries += (b)->cache_num_entries;\
                  (a)->cache_num_entries_replaced += (b)->cache_num_entries_replaced;\
                  (a)->cache_num_entries_revalidation += (b)->cache_num_entries_revalidation;\
                  (a)->cache_num_entries_revalidation_ims += (b)->cache_num_entries_revalidation_ims;\
                  (a)->cache_num_entries_revalidation_etag += (b)->cache_num_entries_revalidation_etag;\
                  (a)->cache_revalidation_not_modified += (b)->cache_revalidation_not_modified;\
                  (a)->cache_revalidation_success += (b)->cache_revalidation_success;\
                  (a)->cache_revalidation_errors += (b)->cache_revalidation_errors;\
                  (a)->cache_num_entries_cacheable += (b)->cache_num_entries_cacheable;\
                  (a)->cache_num_entries_non_cacheable += (b)->cache_num_entries_non_cacheable;\
                  (a)->cache_num_entries_collisions += (b)->cache_num_entries_collisions;\
                  (a)->cache_num_error_entries_creations += (b)->cache_num_error_entries_creations;\
                  (a)->cache_add_url_time_total += (b)->cache_add_url_time_total;\
                  (a)->cache_search_url_time_total += (b)->cache_search_url_time_total;\
                  SET_MIN ((a)->cache_add_url_time_min, (b)->cache_add_url_time_min);\
                  SET_MAX ((a)->cache_add_url_time_max, (b)->cache_add_url_time_max);\
                  SET_MIN ((a)->cache_search_url_time_min, (b)->cache_search_url_time_min);\
                  SET_MAX ((a)->cache_search_url_time_max, (b)->cache_search_url_time_max);\
                }

/*Counters for Caching*/
typedef struct {
 
  u_ns_8B_t cache_num_tries;
  u_ns_8B_t cache_num_hits;
  u_ns_8B_t cache_num_missed;
  u_ns_8B_t cache_bytes_used;
  u_ns_8B_t cache_bytes_hit;
  u_ns_8B_t cache_num_entries;
  u_ns_8B_t cache_num_entries_replaced;
  u_ns_8B_t cache_num_entries_revalidation;
  u_ns_8B_t cache_num_entries_revalidation_ims;
  u_ns_8B_t cache_num_entries_revalidation_etag;

  u_ns_8B_t cache_revalidation_not_modified;
  u_ns_8B_t cache_revalidation_success;
  u_ns_8B_t cache_revalidation_errors;

  u_ns_8B_t cache_num_entries_cacheable;
  u_ns_8B_t cache_num_entries_non_cacheable;
  u_ns_8B_t cache_num_entries_collisions;

  u_ns_8B_t cache_num_error_entries_creations;

  u_ns_4B_t cache_search_url_time_min;
  u_ns_4B_t cache_search_url_time_max;
  u_ns_8B_t cache_search_url_time_total;

  u_ns_4B_t cache_add_url_time_min;
  u_ns_4B_t cache_add_url_time_max;
  u_ns_8B_t cache_add_url_time_total;

} CacheAvgTime;

/*Counters for Caching*/
/*
typedef struct {


} CacheCAvgTime;
*/

/* HTTP caching Cache */
typedef struct {
  //Number of HTTP requests per second in the sampling period
  Long_data cache_req;

  //Rate/Sec of requested URL found in the cache and served from the cache in the sampling period
  Long_data cache_hits; 
  //Percentage of requested URL found in the cache and served from the cache in the sampling period
  Long_data cache_hits_pct;

  //Rate/Sec of requested URL not found in the cache in the sampling period
  Long_data cache_misses;
  //Percentage of requested URL not found in the cache in the sampling period
  Long_data cache_misses_pct;

  //Cache used memory in mega bytes including hash table and other memory used for managing cache
  Long_data cache_used_mem;

  //Rate/Sec of bytes served from the cache in the sampling period
  Long_data cache_bytes_hit;
  //Percentage of bytes served from the cache in the sampling period
  Long_data cache_bytes_hit_pct;

  //Number of objects currently in the cache in the sampling period
  Long_data cache_entries;
  //Number of cached items that were removed to make room for newer entries as per the replacement policy.
  Long_data cache_entries_replaced;

  //Number of entries that were revalidations 
  Long_data cache_entries_revalidation;
  //Number of requests that contained an If-Modified-Since header.
  Long_data cache_revalid_ims;
  //Number of requests that contained an If-Non-Match header.
  Long_data cache_revalid_etag;

  //Number of entries that were revalidated by the server as not modified
  Long_data cache_revalid_not_modified;
  //Number of entries that were revalidated by the server as success 
  Long_data cache_revalid_success;
  //Number of entries that were revalidated by the server as errors 
  Long_data cache_revalid_errors;

  Long_data cache_response_cacheable;
  //Number of responses not cacheable
  Long_data cache_response_non_cacheable;

  //Number of cacheable items that were not cached because the size was larger than the configured maximum content size.
  Long_data cache_response_too_big;
  //Number of cacheable items that were not cached because the file size was smaller  than the configured maximum content size.
  Long_data cache_response_too_small;

  //Number of collisions while adding url response in the cache
  Long_data cache_num_collisions;

  //Number of errors while adding url response in the cache
  Long_data cache_error_entry_creations;

  Times_data cache_search_url_time; // Time taken to search for a URL in cache table
  Times_data cache_add_url_time; // Time taken to add a URL in cache table
} HttpCaching_gp;

extern HttpCaching_gp *http_caching_gp_ptr; 
extern unsigned int http_caching_gp_idx;
#ifndef CAV_MAIN
extern int g_cache_avgtime_idx;
extern CacheAvgTime *cache_avgtime;
#else
extern __thread int g_cache_avgtime_idx;
extern __thread CacheAvgTime *cache_avgtime;
#endif
extern int g_cache_cavgtime_idx;

extern inline void  cache_update_cache_avgtime_size();
extern inline void cache_set_cache_avgtime_ptr();
extern void cache_print_progress_report(FILE *fp1, FILE *fp2, int is_periodic, avgtime *avg);

#endif
