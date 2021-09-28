#ifndef NS_H2_HEADER_FILES_h
#define NS_H2_HEADER_FILES_h


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <openssl/err.h>

#include "util.h"
#include "ns_msg_def.h"
#include "ns_goal_based_sla.h"
#include "tmr.h"
#include "timing.h"
#include "ns_schedule_phases.h"
#include "ns_log.h"


#include "netstorm.h"

#include "ns_data_types.h"
#include "ns_log_req_rep.h"
#include "ns_http_script_parse.h"
#include "ns_http_cache.h"
#include "ns_msg_com_util.h"
#include "ns_parent.h"
#include "ns_trans.h"
#include "ns_http_auth.h"
#include "ns_url_req.h"
#include "ns_user_define_headers.h"
#include "ns_network_cache_reporting.h"
#include "ns_alloc.h"
#include "ns_string.h"
#include "ns_event_id.h"
#include "ns_event_log.h"
#include "ns_vars.h"
#include "ns_http_version.h"
#include "ns_page.h"
#include "ns_debug_trace.h"
#include "nslb_util.h"
#include "ns_auto_cookie.h"
#include "ns_auto_redirect.h"
#include "decomp.h"
#include "ns_http_cache_hdr.h"
#include "ns_h2_stream.h"
#include "ns_h2_req.h"
#include "ns_h2_frames.h"
#include "ns_index_vars.h"
#include "nslb_time_stamp.h"
#include "ns_trace_log.h"
#include "amf.h"
#include "ns_random_vars.h"
#include "ns_unique_numbers.h"
#include "ns_random_string.h"
#include "ns_date_vars.h"
#include "ns_cookie.h"
#include "ns_sock_com.h"
#include "ns_http_cache_table.h"
#include "ns_nd_kw_parse.h"
#include "ns_http_cache.h"
#include "ns_page_dump.h"
#include "ns_http_process_resp.h"
#include "ns_http_status_codes.h"
#include "ns_h2_reporting.h"

#define MAX_CONCURRENT_STREAM 100
 
//extern int http2_handle_read( connection *cptr, u_ns_ts_t now );
//extern int http2_make_handshake_frames(connection *cptr, u_ns_ts_t now);
//extern int http2_handle_write(connection *cptr, u_ns_ts_t now);
/*bug 86575: idx updated according to the promise_count*/
#define NS_H2_ALIGN_IDX(stream_id)\
{\
  NSDL2_HTTP2(NULL, NULL, "promise_buf idx =%d corresponding to the stream_id=%d cptr->http2->promise_count=%d", idx, stream_id, cptr->http2->promise_count);\
  if(cptr->http2->promise_count) \
      idx = idx % cptr->http2->promise_count;\
  NSDL2_HTTP2(NULL, NULL, "Now promise_buf idx =%d corresponding to the stream_id=%d cptr->http2->promise_count=%d", idx, stream_id, cptr->http2->promise_count);\
}

#define NS_SAVE_REQUEST_TIME_IN_CACHE()\
{ \
 if(NS_IF_CACHING_ENABLE_FOR_USER && cptr->cptr_data != NULL && cptr->cptr_data->cache_data != NULL) \
  { \
     CacheTable_t *cacheptr = cptr->cptr_data->cache_data; \
     NSDL2_CONN(NULL, cptr, "cacheptr[%p]->request_ts=%d", cacheptr, cacheptr->request_ts);\
     cacheptr->request_ts = get_ns_start_time_in_secs() + get_ms_stamp()/1000 + get_timezone_diff_with_gmt();\
     NSDL2_CONN(NULL, cptr, "Now cacheptr[%p]->request_ts=%d", cacheptr, cacheptr->request_ts);\
  }\
}

extern int pack_settings_frames(connection *cptr, unsigned char *settings_frames);
extern int ns_process_flow_control_frame(connection *cptr, stream *sptr, int size, u_ns_ts_t now);
extern int ns_h2_fill_stream_into_map(connection *cptr , stream *strm_ptr);
extern void delete_stream_from_map(connection *cptr, stream * sptr);
extern stream* get_free_stream(connection *cptr, int *error_code);
extern int pack_header(connection *cptr, unsigned char *header_fragment, unsigned int header_fragment_len, char padding, unsigned char pad_length, stream* stream_ptr, int content_length);
extern int pack_frame (connection *cptr, unsigned char *frame, unsigned int frame_length , unsigned char frame_type , int end_stream_flag , int end_header_flag,  unsigned int stream_identifier , unsigned char padding, unsigned char *flag);
extern stream *get_stream(connection *cptr, unsigned int strm_id);
extern int pack_goaway(connection *cptr, unsigned int stream_identifier, int error_code, char *debug_data, u_ns_ts_t now);
extern int pack_reset(connection *cptr, stream *sptr, unsigned char *reset_frame, int error_code);
extern void inline makeDtHeaderReqWithOptions(char *dtHeaderValueString, int *len, VUser* vptr, TxInfo *txPtr);
extern void process_response_headers(connection *cptr, nghttp2_nv nv , u_ns_ts_t now);
extern int make_ping_frame(connection *cptr, unsigned char *data, unsigned char *ping_frame, int ack_flag);
extern void http2_cache_save_header(connection *cptr, char *cache_buffer, int cache_buffer_len, cacheHeaders_et cache_header);
extern void release_stream(connection *cptr, stream * sptr);
extern inline void init_stream_map_http2(connection *cptr);
extern void log_http2_res(connection *cptr, VUser *vptr, unsigned char *buf, int size);
extern int pack_window_update_frame(connection *cptr, unsigned char *window_update , int window_update_value , unsigned int stream_id);
extern void set_id_in_hash_table(hash_table *hash_arr, char *url, int url_len, int promise_stream_id);
extern int pack_and_send_reset_frame(connection *cptr, stream *sptr, int error_code, u_ns_ts_t now);

extern int h2_handle_incomplete_write(connection* cptr, u_ns_ts_t now );
int h2_make_handshake_and_ping_frames(connection* cptr, u_ns_ts_t now );
int h2_handle_write_ex(connection* cptr, u_ns_ts_t now);
#endif
