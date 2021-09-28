#ifndef NS_H2_STREAM_H
#define NS_H2_STREAM_H


#define MAX_VALUE_FOR_SID    2147483647  // (int)pow(2,31) 
#define INIT_STREAM_BUFFER   1024
#define MAX_FRAMES           1024

#define NS_H2_ON_FREE_LIST   0x0001
#define NS_H2_ON_INUSE_LIST  0x0002
#define NS_H2_IDLE           0x0004
#define NS_H2_OPEN           0x0008
#define NS_H2_HALF_CLOSED_R  0x0010
#define NS_H2_HALF_CLOSED_L  0x0020
#define NS_H2_CLOSED	     0x0040

#define RELEASE_STREAM(x, y) {\
  delete_stream_from_map(x, y); \
  release_stream(x, y); \
}

/*bug 84661 added macro DEFAULT_FRAME_SIZE */
#define DEFAULT_FRAME_SIZE 65536

typedef struct Http2FlowControl
{
  int local_window_size;
  int remote_window_size;
  int received_data_size;
  int data_left_to_send;
}Http2FlowControl;

typedef struct stream {
  unsigned int stream_id;
  int state;
  struct stream *next;
  struct stream *q_next;
  void *frame_ptr[MAX_FRAMES];
  Http2FlowControl  flow_control;
  char is_cptr_data_saved;
  // Added for request multiplexing
  char compression_type;// Added to keep track of request details for the http request
  action_request_Shr *url_num;
  char *location_url; /* stores redirected-to location */
  char *link_hdr_val; // stores link header val

  /* char *url will have the complete url (No method No version Only URL)*/
  char *url;
  unsigned short url_len; /* Length to URL null terminated */ 

  short redirect_count; /* Stores count of how many redirects followed so far. */
  int conn_state, header_state, chunked_state, proto_state;
  ns_ptr_t cookie_hash_code; // This is hash code of cookie or Head of the cookie linked list in Auto Cookie Mode
  short cookie_idx;
  short gServerTable_idx;
  u_ns_ts_t started_at; /* initial time */
  /*component start time stamp.This will get updated on every component start */
  u_ns_ts_t ns_component_start_time_stamp; 
  int dns_lookup_time; /*Total time taken (miliseconds) in DNS lookup*/
  int write_complete_time;  //Time taken (miliseconds) in write the request
  int first_byte_rcv_time;  //Time taken (miliseconds) to recv first byte of the response
  int request_complete_time; /*Request complete time (miliseconds) is the download time*/

  int http_payload_sent;
  int tcp_bytes_sent;
  int req_ok;
  unsigned char completion_code;
  unsigned char num_retries;
  unsigned char request_type;
  int req_code;
  int req_code_filled;
  int content_length;  // Body size in req/resp both
  int bytes;  // Bytes of body received or bytes of a chunk
  int tcp_bytes_recv; // all bytes (header and body)
  int total_bytes; /* used only for chunked pages and for JBOSS_CONNECT, RMI_CONNECT requests*/
  int body_offset ; // 02/08/07 -Atul For ResponseURL
  int checksum;
  union {
    int dir;
    int body_index;  // It will have the body start vector index in all vectors (Used for HTTP : 100 Continue)
  };
  struct sockaddr_in6 cur_server; /* Address of current server (Origin Server)*/
  /*Proxy*/
  struct sockaddr_in6 conn_server;  // Address of origin or proxy server where connection is made. 

  htmlParserCtxtPtr ctxt;
  timer_type* timer_ptr;
  u_ns_ts_t request_sent_timestamp;
  struct copy_buffer* buf_head;
  struct copy_buffer* cur_buf;

  //Following are added for  large request writes
  char * free_array; //free_array is also ovderloaded used for keeping alloacted send pointer for HTTPS
  struct iovec * send_vector;
  int bytes_left_to_send;
  short num_send_vectors;
  short first_vector_offset;
  int num_vectors;
  int http_size_to_send;
  union {
    char *last_iov_base;
    // Used while processing of HTTP response hdr and saving of hdr value if needed due to partial value. This is made NULL when we read the respone first time
    // Callback method MUST free it or use it and make it NULL or it will be freed in close_connection
    char *cur_hdr_value; 
  };

  // chunk_size_node *chunk_size_head;
  // chunk_size_node *chunk_size_tail;

  //Replay fields
  union {
    int req_file_fd;  //fd of request file in Replay Mode
    int tx_instance; //URL will have tx. In 4.1.3 embd url can have tx
  };

  void *conn_link;  /* Type connection keeps link to control con */

  //  connection *second_con;
  /* DNS and pipelining */
  /*bug 52092 : flags type chaged to long long int like cptr->flags */
  long long int flags;   // flags currently used for expecting 100 Continue state
  int  prev_digest_len;  
  unsigned char *prev_digest_msg; 
  struct cptr_data_t *cptr_data;
  unsigned int window_update_from_server; /*bug 84661 added window_update_from_server*/ 
  //cptr_data_t *cptr_data; 
  /* Proxy Chain Auth ---END*/
  //X-Dynatrace header data ptr
  //x_dynaTrace_data_t *x_dynaTrace_data;
} stream;

// This structure will contain front and rear for queue. This queue will be used in stream management and used to manage free slots in 
// the stream array 
typedef struct NsH2StreamQueue {

  int *ns_h2_slot;
  int front;
  int rear;
  int ns_h2_max_slot;
  int ns_h2_available_slot;
} NsH2StreamQueue;

typedef struct NsH2StreamMap {

  stream *stream_ptr;
  int stream_id;
  int next_slot;
} NsH2StreamMap;


extern void init_stream_map(NsH2StreamMap **map1, int max_concurrent_stream, int *available_streams);
extern void init_stream_queue(NsH2StreamQueue **stream_queue1, int max_concurrent_stream);
extern inline void remove_ifon_stream_inuse_list(stream* sptr);
#endif
