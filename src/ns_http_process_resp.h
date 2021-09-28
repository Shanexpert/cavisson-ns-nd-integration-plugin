
#ifndef _NS_HANDLE_READ_H_
#define _NS_HANDLE_READ_H_
/* defines */

/* Max bytes/second in throttle mode. */
//#define THROTTLE 3360

#define CHK_SIZE 1
#define CHK_TEXT 2
#define CHK_CR 3
#define CHK_READ_CHUNK 4
#define CHK_FINISHED 5

extern int g_uncompress_chunk;
extern char *HdrStateArray[];

/* function protos */
extern void handle_read( connection* cptr, u_ns_ts_t now );
extern void debug_log_http_res_line_break(connection *cptr);
//extern void debug_log_http_res(connection *cptr, char *buf, int size);
extern int reset_idle_connection_timer(connection *cptr, u_ns_ts_t now);
extern void handle_partial_recv(connection* cptr, char *buffer, int length, int total_size);
extern void copy_retrieve_data( connection* cptr, char* buffer, int length, int total_size );
extern void free_cptr_buf(connection* cptr);

extern int body_starts(connection *cptr, char *buf, int byte_left, int *bytes_consumed, u_ns_ts_t now);
extern int change_state_to_hdst_bol(connection *cptr, char *buf, int byte_left, int *bytes_consumed, u_ns_ts_t now);
extern int is_chunked_done_if_any(connection *cptr, char *buf, int byte_left, int *bytes_consumed, u_ns_ts_t now);
extern int proc_http_hdr_content_encoding_deflate(connection *cptr, char *buf, int byte_left, int *bytes_consumed, u_ns_ts_t now);
extern int proc_http_hdr_content_encoding_gzip(connection *cptr, char *buf, int byte_left, int *bytes_consumed, u_ns_ts_t now);
extern int proc_http_hdr_content_encoding_br(connection *cptr, char *buf, int byte_left, int *bytes_consumed, u_ns_ts_t now);
extern int proc_http_hdr_content_length(connection *cptr, char *buf, int byte_left, int *bytes_consumed, u_ns_ts_t now);
extern int proc_http_hdr_location(connection *cptr, char *buf, int byte_left, int *bytes_consumed, u_ns_ts_t now);
extern int proc_http_hdr_set_cookie(connection *cptr, char *buf, int byte_left, int *bytes_consumed, u_ns_ts_t now);
extern int proc_http_hdr_transfer_encoding_chunked(connection *cptr, char *buf, int byte_left, int *bytes_consumed, u_ns_ts_t now);
extern int proc_http_hdr_content_type_amf(connection *cptr, char *buf, int byte_left, int *bytes_consumed, u_ns_ts_t now);
extern int proc_http_hdr_content_type_hessian(connection *cptr, char *buf, int byte_left, int *bytes_consumed, u_ns_ts_t now);

// void *(*start_routine)(void*)
// extern ProcessHdrCallback *(ns_get_hdr_callback_fn_address)(char *func_name);
extern void *ns_get_hdr_callback_fn_address(char *func_name);
extern inline int copy_data_to_full_buffer(connection* cptr);
extern inline void save_header(VUser *vptr, char *buf, int bytes_read);
//For HTTP2
extern int http2_process_read_bytes(connection *cptr, u_ns_ts_t now, int len, unsigned char *buf);
extern void handle_fast_read(connection*cptr, action_request_Shr* url_num, u_ns_ts_t now );
extern inline void handle_bad_read (connection *cptr, u_ns_ts_t now);

#endif /* _NS_HANDLE_READ_H_ */
