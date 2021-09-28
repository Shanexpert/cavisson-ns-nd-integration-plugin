/********************************************************************
* Name: ns_sock_com.h
* Purpose: Socket Communication related  function prototype
* Author: Archana
* Intial version date: 14/12/07
* Last modification date: 22/12/07
********************************************************************/

/* Bit mask for Optimizing Ether Flow. These are initializing for keyword OPTIMIZE_ETHER_FLOW */
#define OPTIMIZE_HANDSHAKE_MERGE_ACK   		0x0001
#define OPTIMIZE_DATA_MERGE_ACK   	   	0x0002
#define OPTIMIZE_CLOSE_BY_RST		    	0x0004

// Bit mask for checking SNI Mode 
#define SSL_SNI_ENABLED 	0x00000001 

extern unsigned int bind_fail_count;
#define FREE_SOCKET_MEM                         27     //used by free_sock_mem() to free kernel memory
#define BIND_SOCKET(addr, min_port, max_port) {                    \
    int bind_done = 0;                         \
    int test_bind = 0;                         \
    while (!bind_done) {                       \
      test_bind++;                             \
      /*memcpy ((char *)&(cptr->sin), (char *) &(vptr->user_ip->ip_addr), sizeof(struct sockaddr_in6)); */\
      memcpy ((char *)&(cptr->sin), addr, sizeof(struct sockaddr_in6)); \
      if (total_ip_entries) {  \
        if(global_settings->src_port_mode == SRC_PORT_MODE_RANDOM) { \
           select_port = calculate_rand_number(min_port, max_port); \
           cptr->sin.sin6_port = htons(select_port); \
        } else { \
           cptr->sin.sin6_port = htons(vptr->user_ip->port); \
           vptr->user_ip->port++; \
           if (!vptr->user_ip->port || vptr->user_ip->port > max_port) \
             vptr->user_ip->port = min_port; \
        } \
      } \
      if ((bind( cptr->conn_fd, (struct sockaddr*) &cptr->sin, sizeof(cptr->sin))) < 0 ) { \
        average_time->bind_sock_fail_tot = (test_bind - 1);  /*As tried one less than test_bind */\
        SET_MIN (average_time->bind_sock_fail_min, average_time->bind_sock_fail_tot); \
        SET_MAX (average_time->bind_sock_fail_max, average_time->bind_sock_fail_tot); \
        if((test_bind > global_settings->num_retry_on_bind_fail)) {\
            cptr->req_ok = NS_SOCKET_BIND_FAIL; \
          if(global_settings->action_on_bind_fail) {\
            end_test_run(); \
          } else { \
            Close_connection(cptr, 0, now, NS_SOCKET_BIND_FAIL, NS_COMPLETION_BIND_ERROR); \
            return -1;\
          } \
        } \
      } else { \
        bind_done = 1; \
      } \
    } \
    /* Set the file descriptor to no-delay mode. */ \
    if ( fcntl( cptr->conn_fd, F_SETFL, O_NDELAY ) < 0 ) { \
      fprintf(stderr, "Error: Setting fd to no-delay failed\n"); \
      perror( get_url_req_url(cptr) ); \
      end_test_run(); \
    } \
}

#define IS_HTTP2_INLINE_REQUEST (cptr->http_protocol == HTTP_MODE_HTTP2 && cptr->http2->front) 

#define HTTP2_SET_INLINE_QUEUE \
if(cptr->http2->http2_cur_stream){ \
  copy_cptr_to_stream(cptr, cptr->http2->http2_cur_stream); \
} else { \
 NSDL2_HTTP2(cptr->vptr, cptr, "Http2 connection, cptr->http2->http2_cur_stream not set, this should not happen"); \
} \
if(cptr->bytes_left_to_send){ \
  NSDL2_HTTP2(cptr->vptr, cptr, "Current request is not completely written, adding it into queue of streams"); \
  add_node_to_queue(cptr, cptr->http2->http2_cur_stream);  \
} 

#define UPDATE_GROUP_BASED_NETWORK_TIME(src, dest, dest_min, dest_max, dest_count) \
{\
  if(SHOW_GRP_DATA) { \
    avgtime *lol_average_time; \
    lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp)); \
    SET_MIN (lol_average_time->dest_min, cptr->src); \
    SET_MAX (lol_average_time->dest_max, cptr->src); \
    lol_average_time->dest += cptr->src; \
    lol_average_time->dest_count++; \
   } \
}

extern void free_sock_mem (int fd);
extern int set_optimize_ether_flow (char *buf, int runtime_flag, char *err_msg);
extern int inline set_socketopt_for_close_by_rst(int fd);
extern int inline set_socketopt_for_quickack(int fd, int after_connect);

extern int inline get_socket(int family, int grp_idx);
extern int inline get_udp_socket(int family, int grp_idx);
extern int start_socket( connection* cptr, u_ns_ts_t now );
extern void handle_connect( connection* cptr, u_ns_ts_t now, int double_check );
extern void inline retry_connection(connection* cptr, u_ns_ts_t now, int req_sts);
extern inline void close_fd (connection* cptr, int done, u_ns_ts_t now);
extern inline void cache_close_fd (connection* cptr, u_ns_ts_t now);
extern inline void wait_sockets_clear();
extern void init_parent_listner_socket();
extern inline int ns_epoll_init(int *fd);
extern inline int remove_select(int fd);
extern int add_select(void* cptr, int fd, int event);
extern int mod_select(int sfd, connection* cptr, int fd, int event);
extern inline void handle_server_close (connection *cptr, u_ns_ts_t now);

extern inline void ssl_struct_free (connection *cptr);
extern void ssl_init_write (connection *cptr, int http_size, int num_vectors, struct iovec *vector_ptr, int *free_array);
extern inline void ssl_sess_save (connection *cptr);
extern inline void save_referer(connection* cptr);
extern inline void free_vectors (int num_vectors, int *free_array, struct iovec* vector);
//extern void debug_log_http_req(connection *cptr, char *buf, int bytes_to_log, int complete_data, int first_trace_write_flag);

extern void
handle_incomplete_write( connection* cptr,  NSIOVector *ns_iovec, int num_vectors, int bytes_to_send, int bytes_sent);
extern void reset_cptr_attributes(connection *cptr);
extern void idle_connection( ClientData client_data, u_ns_ts_t now );
extern int el_epoll_fd;
extern inline void set_cptr_for_new_req(connection* cptr, VUser *vptr, u_ns_ts_t now);
extern int calculate_rand_number(double min, double max);
extern inline void inc_con_num_and_succ(connection *cptr);
extern int send_http_req(connection *cptr, int http_size, NSIOVector *vector, u_ns_ts_t now);
extern inline void update_parallel_connection_counters(connection *cptr, u_ns_ts_t now);
extern void free_all_vectors(connection *cptr);
extern inline void reset_cptr_attributes_after_proxy_connect(connection *cptr);
extern inline void close_fd_and_release_cptr (connection* cptr, int done, u_ns_ts_t now);
extern inline void ssl_free_send_buf(connection *cptr);
extern void copy_request_into_buffer(connection *cptr, int http_size, NSIOVector *ns_iovec);
extern inline void free_dns_cptr(connection *cptr);
extern void free_cptr_send_vector(connection *cptr, int num_vectors);
inline void free_cptr_vector_idx(connection *cptr, int idx);
void inline reset_after_100_continue_hdr_sent(connection *cptr);
extern int http2_make_handshake_frames(connection *cptr, u_ns_ts_t now);
extern inline int http2_make_request (connection* cptr, int *num_vectors, u_ns_ts_t now);
extern void process_response_headers(connection *cptr, nghttp2_nv nv , u_ns_ts_t now);
extern int get_id_from_hash_table(hash_table *hash_array, char *url, int url_len);
extern void log_http2_res(connection *cptr, VUser *vptr, unsigned char *buf, int size);
extern void debug_log_http2_res(connection *cptr, unsigned char *buf, int size);
extern void debug_log_http2_dump_pushed_hdr(connection *cptr, int promised_stream_id);
extern action_request_Shr *process_segmented_url(VUser *vptr, action_request_Shr *url_num, char **url, int *url_len);
/*bug 52092: http2_make_proxy_request() added */
inline int http2_make_proxy_request (connection* cptr, int *num_vectors, u_ns_ts_t now);
