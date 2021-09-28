/******************************************************************
 * Name    : ns_url_req.h
 * Author  : Archana
 * Purpose : This file contains methods for processing of URL request
 * Note:
 * Modification History:
 * 08/10/08 - Initial Version
*****************************************************************/

#ifndef _NS_URL_REQ_BUF_H_
#define _NS_URL_REQ_BUF_H_

#define REQ_PART_REQ_LINE 0
#define REQ_PART_HEADERS  1
#define REQ_PART_BODY     2

// Request Type Macros for Protocol Specific
#define IS_REQUEST_HTTP            (cptr->request_type == HTTP_REQUEST)
#define IS_REQUEST_HTTPS           (cptr->request_type == HTTPS_REQUEST)
#define IS_REQUEST_HTTP_OR_HTTPS   ((cptr->request_type == HTTP_REQUEST) ||              \
                                    (cptr->request_type == HTTPS_REQUEST))

#define IS_REQUEST_WS            (cptr->request_type == WS_REQUEST)
#define IS_REQUEST_WSS           (cptr->request_type == WSS_REQUEST)
#define IS_REQUEST_WS_OR_WSS     ((cptr->request_type == WS_REQUEST) ||                  \
                                  (cptr->request_type == WS_REQUEST))

#define IS_REQUEST_SOCKET               (cptr->request_type == SOCKET_REQUEST)
#define IS_REQUEST_SSL_SOCKET           (cptr->request_type == SSL_SOCKET_REQUEST)
#define IS_REQUEST_SSL_OR_NONSSL_SOCKET ((cptr->request_type == SOCKET_REQUEST) ||       \
                                         (cptr->request_type == SSL_SOCKET_REQUEST))

#define IS_SSL_READ ((IS_REQUEST_HTTPS || IS_REQUEST_WSS || IS_REQUEST_SSL_SOCKET) &&    \
                     !((cptr->flags & NS_CPTR_FLAGS_CON_USING_PROXY_CONNECT) ||          \
                       (cptr->flags & NS_CPTR_FLAGS_CON_PROXY_AUTH)))

//#define REFERER_STRING_LENGTH 9
extern char* Referer_buf;
extern char* Host_header_buf;
extern char* Accept_buf;
extern char* Accept_enc_buf;
extern char* keep_alive_buf;
extern char* connection_buf;
extern char* post_content_ptr;
extern const int post_content_val_size;
extern char content_length_buf[];

#define REFERER_STRING_LENGTH 9
#define ACCEPT_BUF_STRING_LENGTH 174
#define ACCEPT_ENC_BUF_STRING_LENGTH 52
#define KEEP_ALIVE_BUF_STRING_LENGTH 17
#define CONNECTION_BUF_STRING_LENGTH 24
#define POST_CONTENT_VAR_SIZE 16 /* strlen("Content-Length: ") */
#define POST_CONTENT_HDR_SIZE 30
#define HESSIAN_MAX_BUF_SIZE (4*1024*1024)
#define IS_REFERER_NOT_HTTPS_TO_HTTP (vptr->referer_size && !(vptr->referer[4] == 's' && cptr->url_num->request_type == HTTP_REQUEST))

extern inline int make_request(connection* cptr, int *num_vectors, u_ns_ts_t now);
extern void set_num_additional_headers();
extern char *get_url_req_url(connection *cptr);
extern char *get_url_req_line(connection *cptr, char *req_line_buf, int *req_line_buf_len, int max_len);

extern inline void get_abs_url_req_line(action_request_Shr *request, VUser  *vptr, char *url, 
                       char *full_url, int *full_url_len, int *url_offset, int max_len);

extern int insert_segments(VUser *vptr, connection *cptr, const StrEnt_Shr* seg_tab_ptr, NSIOVector *ns_iovec,
                           int* content_size, int body_encoding, int var_val_flag, int req_part,
                           action_request_Shr* request, int cur_seq);

extern inline void http_make_url_and_check_cache(connection *cptr, u_ns_ts_t now, int *ret);

extern inline int make_cookie_segments(connection* cptr, http_request_Shr* request, NSIOVector *ns_iovec);

extern void make_part_of_relative_url_to_absolute(char *url, int url_len, char *out_url);

extern char *get_url_req_url(connection *cptr);

// For http2
extern char*
chk_ns_escape(int encode_type, int req_part, int body_encoding, int is_malloc_needed, char *in_buf, int in_size,
              int *out_size, int *free_array, char *specific_char, char *encodespaceby);
#define NS_SAVE_USED_PARAM(...) if(vptr->httpData->up_t.used_param) ns_save_used_param(__VA_ARGS__)
char *show_seg_ptr_type(int seg_ptr);
int remove_newline_frm_param_val(char *value, int value_len, char *out_buf);
extern int copy_cptr_to_stream(connection *cptr,  stream *stream_ptr);
extern int copy_stream_to_cptr(connection *cptr, stream *stream_ptr);
extern inline int insert_h2_auto_cookie(connection* cptr, VUser* vptr, int push_flag);

#define HOST_HEADER_STRING_LENGTH 6
#define USER_AGENT_STRING_LENGTH 12
//extern char *ns_escape(const char *string, int length, int *out_len, char *specified_char, char *EncodeSpaceBy);
extern unsigned char* make_encrypted_body(NSIOVector *ns_iovec, char encryption_algo, char base64_encode_option, char *key, int key_len, char *ivec, int ivec_len, int *content_length);
extern void get_key_ivec_buf(int consumed_vector, NSIOVector *ns_iovec, StrEnt_Shr *key_ivec, char *key_ivec_local_buf, int key_ivec_size);
extern int do_encrypt_req_body(action_request_Shr* request, int group_num);
//extern int make_encrypted_req_body(int group_num, action_request_Shr* request, connection *cptr, int* content_length);
extern int  make_encrypted_req_body(int group_num, action_request_Shr* request, connection *cptr, NSIOVector *ns_iovec);
extern int fill_req_body_ex(connection* cptr, NSIOVector *ns_iovec, action_request_Shr *request);
#endif /* _NS_URL_REQ_BUF_H_ */
