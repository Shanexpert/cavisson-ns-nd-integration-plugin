/************************************************************************************************
 * File Name            : ns_http_cache.h
 * Author(s)            : Achint Agarwal
 * Date                 : 22 November 2010
 * Copyright            : (c) Cavisson Systems
 * Purpose              : Stores the Structures used for caching
 * Modification History :
 *              <Author(s)>, <Date>, <Change Description/Location>
 ***********************************************************************************************/
#ifndef NS_HTTP_CACHE_H
#define NS_HTTP_CACHE_H

#define CACHE_NOT_ENABLE           -2       //Caching is not enable
#define CACHE_RESP_IS_FRESH        -3       //Cache is fresh
#define CACHE_NEED_TO_VALIDATE     -4       //Cache need to revalidate

#define NS_NOT_CACHED 0
#define NS_CACHED     1

#define NS_IF_CACHING_ENABLE_FOR_USER (vptr->flags & NS_CACHING_ON)
#define MAX_CACHE_TABLE_SIZE 65536 //this is upper limit of cache table size 
#define CACHE_TABLE_RECORDED 1 //this is defined for cache table size mode 1

extern void cache_vuser_check_and_enable_caching(VUser *vptr);
extern int cache_check_if_caching_enable_for_user(VUser *vptr);
extern int cache_fill_validators_in_req(CacheTable_t *cacheptr, VUser *vptr);
extern void cache_update_cache_hit_count ();
extern void cache_update_cache_miss_count ();
extern int cache_check_if_url_cached(VUser *vptr, connection *cptr,
                              u_ns_ts_t now, char *url, int *);
extern void cache_set_resp_frm_cache ( connection *cptr, VUser *vptr);

extern int get_frlt_frm_max_age(CacheTable_t *cacheptr);
extern int get_frlt_frm_expir(CacheTable_t *cacheptr);
extern int get_frlt_frm_last_modified(CacheTable_t *cacheptr);
extern int cache_get_current_age (connection *cptr, CacheTable_t *cacheptr, u_ns_ts_t now);
extern char *get_time_in_secs(char *time);
extern char *get_time_in_nw_format(int time_sec);
extern int kw_set_g_http_caching(char *buf, GroupSettings *gset, char *err_msg, int rumtime_flag);
extern int kw_set_g_http_caching_test(char *buf, GroupSettings *gset, char *err_msg);
extern void validate_req_code (connection *cptr);
extern int cache_update_table_entries (connection *cptr);
extern unsigned int get_curr_time_in_sec ();
extern void cache_update_cache_time (CacheTable_t *cache_ptr);
extern int calc_doc_age_when_received(CacheTable_t *cache_ptr);
extern void free_cache_for_user(VUser *vptr);
extern void cache_init_cache_req_hdr(CacheRequestHeader *cache_req_hdr);
extern void cache_copy_req_hdr_shr(int i);
extern int kw_set_g_http_cache_table_size(char *buf, GroupSettings *gset, char *err_msg, int rumtime_flag);
extern int change_to_2_power_n(int size_value);
extern int kw_set_g_http_caching_master_table(char *buf, GroupSettings *gset, char *err_msg, int rumtime_flag);
extern inline void copy_main_url_in_vptr_http_data(connection *cptr, char *url_local, int url_local_len);
extern void cache_fill_validators_in_req_http2(CacheTable_t *cacheptr, int push_flag);

typedef enum
{
    NS_CACHE_ERROR = -1,
    NS_CACHE_SUCCESS = 0
} NS_CACHE_STATUS;

extern const char hdr_end[];
extern const int hdr_end_len;

/* cptr

While processing headers, we need to store all cache related headers which will be added in cache entry once we deide that cahing is to be done


*/
//=====================Request Headers=======================================
/****
 * Not used now. we will add it in future
typedef struct cacheControlReq {
  unsigned int max_age; // maximum age of cached entry in seconds 
  unsigned int max_stale; //we are ok with an old entry – but not older than this in seconds
  unsigned int min_fresh; // not enough to be fresh, cached entry has to be atleast this many                // seconds  fresh 
  union {
    unsigned char val;
    struct {
      unsigned char no_cache:1;  // if present, must revalidate // if stale no matter what other headers say
      unsigned char no_store:1 ; // Do not cache this –ever !!
      unsigned char only_if_cached:1; // return from cache only
    }bits;
  }u;
} cacheControlReq;

typedef struct {
  cacheControlReq cachecontrolreq; //Cache-control field in general header
} requestHeader;
**/


#endif  //NS_HTTP_CACHE_H
