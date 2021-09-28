#ifndef NS_H2_REPORTING_H
#define NS_H2_REPORTING_H
/* bug 70480 : HTTP2 Server Push Metrics support*/
#include "ns_gdf.h"

#define IS_SERVER_PUSH_ENABLED\
 global_settings->protocol_enabled & HTTP2_SERVER_PUSH_ENABLED

#define MAX_HTTP2_LOG_FILES 64000

extern void debug_log_http2_req(connection *cptr, int size, char *data);
extern void debug_log_http2_rep(connection *cptr, int size, unsigned char *data);
extern void debug_log_http2_header_dump(connection *cptr, unsigned char *data, int size);
extern void log_http2_protocol_error(char *file, int line, char *fname, char *line_buf, char *format, ...);
extern void debug_log_http2_res(connection *cptr, unsigned char *buf, int size);
extern void log_http2_res_ex(connection *cptr, unsigned char *buf, int size);

/*bug 78106 :  method to dump both HTTP2 request header and body*/
extern void debug_log_http2_dump_req(connection *cptr, struct iovec *vector, int body_start_idx, int total_body_vectors);
extern void log_http2_dump_req(connection *cptr, struct iovec *vector, int body_start_idx, int total_body_vectors);
extern void http2_dump_req(connection *cptr, struct iovec *vector, int body_start_idx, int total_body_vectors);


/*bug 70480 : HTTP2 Server Push Metrics support*/
/*Counters for Srv Push*/
typedef struct {

  u_ns_8B_t num_server_push;

} SrvPushAvgTime;

/* HTTP2 Srv Push */
typedef struct {
  //Number of HTTP requests per second in the sampling period
  Long_data server_push;
} Http2SrvPush_gp;

extern int g_srv_push_avgtime_idx;
extern Http2SrvPush_gp* http2_srv_push_gp_ptr;
extern unsigned int http2_srv_push_gp_idx;

inline void h2_server_push_update_avgtime_size();
inline void h2_server_push_print_progress_report(FILE *fp1, FILE *fp2, cavgtime*);

#endif
