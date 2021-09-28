
#ifndef NS_HTTP_CACHE_HDR_H
#define NS_HTTP_CACHE_HDR_H
#define CACHE_ALWAYS_SERVE_FROM_CACHE  -2

//Cache Response Header's Id for setting which header is received
typedef enum
{
  CACHE_CONTROL = 0,
  CACHE_DATE = 1,
  CACHE_EXPIRES = 2,
  CACHE_LMD = 3,
  CACHE_AGE = 4,
  CACHE_ETAG = 5,
  CACHE_PRAGMA = 6,
  COOKIE_EXPIRES = 7   // This was added for cookie (not related caching)
} cacheHeaders_et ;

extern int proc_http_hdr_cache_control(connection* cptr, char* cache_buffer, int cache_buffer_len,
                                                                int *consumed_bytes, u_ns_ts_t now);

extern int proc_http_hdr_last_modified(connection* cptr, char* cache_buffer, int cache_buffer_len,
                                                                int *consumed_bytes, u_ns_ts_t now);

extern int proc_http_hdr_expires(connection* cptr, char* cache_buffer, int cache_buffer_len,
                                                                int *consumed_bytes, u_ns_ts_t now);

extern int proc_http_hdr_etag (connection* cptr, char* cache_buffer, int cache_buffer_len,
                                                                int *consumed_bytes, u_ns_ts_t now);

extern int proc_http_hdr_date (connection* cptr, char* cache_buffer, int cache_buffer_len,
                                                                int *consumed_bytes, u_ns_ts_t now);

extern int proc_http_hdr_age (connection* cptr, char* cache_buffer, int cache_buffer_len,
                                                                int *consumed_bytes, u_ns_ts_t now);


extern int proc_http_hdr_pragma (connection* cptr, char* cache_buffer, int cache_buffer_len,
                                                                int *consumed_bytes, u_ns_ts_t now);

extern void cache_ability_on_caching_headers(connection *cptr);
//extern void cache_ability_on_method(connection *cptr);

extern void cache_ability_on_resp_code(connection *cptr);

extern void cache_set_cacheEntryState(connection *cptr, CacheState_et cache_State);
 
extern int cache_parse_req_cache_control(char *cCacheHeaderValue, CacheRequestHeader *cache_req_hdr, int sess_idx, int line_num, char *fname);
extern u_ns_ts_t cache_convert_http_hdr_date_in_secs(connection *cptr, char *http_hdr_date, char *hdr_name, cacheHeaders_et hdr_type);
extern int cache_save_header(connection *cptr, char* cache_buffer, int cache_buffer_len, int *consumed_bytes,
                                                                u_ns_ts_t now, cacheHeaders_et cache_header);
extern inline void cache_save_partial_headers(char* cache_buffer, connection *cptr, int length);
extern void cache_headers_parse_set(char *cCacheHeaderValue, int iCacheHeaderLength, connection *cptr, cacheHeaders_et cache_header);
extern void cache_free_partial_hdr(connection *cptr);
#endif //NS_HTTP_CACHE_HDR_H




