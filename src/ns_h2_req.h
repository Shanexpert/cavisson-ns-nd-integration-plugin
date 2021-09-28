#ifndef NS_H2_REQ_H
#define NS_H2_REQ_H

#include <nghttp2/nghttp2.h>
#include "ns_h2_frames.h"
#include "ns_parse_scen_conf.h"
#include "ns_h2_stream.h"

#define NS_HTTP2_INIT 128
#define NS_HTTP2_DELTA 64

#define HTTP_MODE_AUTO   0
#define HTTP_MODE_HTTP1  1
#define HTTP_MODE_HTTP2  2

#define NS_HTTP2_MAX_FRAME_SIZE 16777215

/* Http2 cptr proto states . Starting value of Macro from 20 . This is done to avoid conflicts with other Macro used with HTTP Proto state  */
#define HTTP2_CON_PREFACE_CNST   20
#define HTTP2_SETTINGS_CNST      21
#define HTTP2_SETTINGS_SENT      22
#define HTTP2_SETTINGS_DONE      23
#define HTTP2_SETTINGS_ACK       24
#define HTTP2_HANDSHAKE_DONE     25
#define HTTP2_PING_RCVD          26
#define HTTP2_SEND_WINDOW_UPDATE 27
#define HTTP2_SEND_RESET_FRAME  28
/*bug 93672: gRPC : macro added for PING_CONST*/
#define HTTP2_PING_CNST      	29

// Maximun buffer sizes 
#define READ_BUFF_SIZE 65536 
#define MAX_FRAME_HEADER_LEN  9 
#define RECV_FRAME_SIZE 3
#define SID_LEN 4 
// Maxium size for structure to malloc

#define INIT_H2_REQUEST_ENTRIES 1024

//Macro used for filling the headers in nghttp2 table
#define FILL_HEADERS_IN_NGHTTP2(hdr_name, hdr_name_len, hdr_value, hdr_value_len, free_arr_name, free_arr_val) \
{                                                         \
    create_ng_http_nv_entries(&idx);                      \
    http2_hdr_ptr[idx].name = (uint8_t *)hdr_name;        \
    http2_hdr_ptr[idx].namelen = hdr_name_len;            \
    http2_hdr_ptr[idx].value = (uint8_t *)hdr_value;      \
    http2_hdr_ptr[idx].valuelen = hdr_value_len;          \
    NSDL2_HTTP2(NULL, NULL, "Adding header - %*.*s %*.*s at idx = %d", http2_hdr_ptr[idx].namelen, http2_hdr_ptr[idx].namelen, \
                http2_hdr_ptr[idx].name, http2_hdr_ptr[idx].valuelen, http2_hdr_ptr[idx].valuelen, http2_hdr_ptr[idx].value, idx); \
    free_array_http2[idx].name = free_arr_name;           \
    free_array_http2[idx].value = free_arr_val;           \
}

#define LOG_PUSHED_REQUESTS(hdr_name, hdr_name_len, hdr_value, hdr_value_len) \
{                                                                     \
  fprintf(log_hdr_fp, "%*.*s ", (int)hdr_name_len, (int)hdr_name_len, hdr_name); \
  fprintf(log_hdr_fp, "%*.*s\n", (int)hdr_value_len, (int)hdr_value_len, hdr_value); \
}
// Error Codes 

#define HTTP2_SUCCESS    0 
#define HTTP2_ERROR     -1
#define HTTP2_PARTIAL   -2

extern FILE *log_hdr_fp;

typedef struct {
int name;
int value;
} FreeArrayHttp2;

typedef struct hash_table{
  short promised_stream_id; 
  short url_len;
  char *url;
  struct hash_table *next;
}hash_table;

typedef struct promise{
  nghttp2_nv *response_headers;
  char *response_data;
  short header_count;
  short header_count_max;
  int data_offset;
  int data_offset_max;
  char free_flag;
}promise;

extern FreeArrayHttp2 *free_array_http2;

// Needed for http2 
extern nghttp2_nv *http2_hdr_ptr;
extern int nghttp2_nv_max_entries;
extern int nghttp2_nv_tot_entries;

extern size_t idx;

extern inline void create_ng_http_nv_entries(size_t *rnum);

// Structure HTTP2
typedef struct ProtoHTTP2{
 //  char frame_header[MAX_FRAME_HEADER_LEN] ;
  Http2SettingsFrames settings_frame;
  Http2FlowControl flow_control;
  stream *http2_cur_stream;
  NsH2StreamMap *cur_stream_map;
  NsH2StreamQueue *cur_map_queue;
  int available_streams; 
  int total_open_streams; // This counter is used to keep a track of all open streams of a connection
  int max_stream_id;
  // *Assumtion: In case of partail frame of one stream, no other stream can get frame until previous partial frame is not completely read  
  unsigned char *partial_read_buff; // This buffer will be used to manage partial frames read
  int partial_buff_max_size; // Aloocated size
  int partial_buff_size; // Used size
  int byte_processed;
  char *continuation_buf;
  int continuation_buf_len;
  int continuation_max_buf_len;
  nghttp2_hd_deflater *deflater;
  nghttp2_hd_inflater *inflater;
  stream *front;
  stream *rear;
  hash_table *hash_arr; 
  promise *promise_buf;
  short promise_count_max;
  short promise_count;
  short curr_promise_count;
  char main_resp_received;
  char stream_one_timeout;
  char donot_release_cptr;
  int last_stream_id;
} ProtoHTTP2; 



extern void release_frame(data_frame_hdr* frame_chunk);


//extern void debug_log_http2_dump_pushed_hdr(connection *cptr);
//extern init_proto_http2(connection *cptr);
#endif
