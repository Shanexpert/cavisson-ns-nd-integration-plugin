#ifndef NS_HTTP_AUTH_NTLM_H
#define NS_HTTP_AUTH_NTLM_H


#define ST_AUTH_BASIC_RCVD                   1 //Basic Handshake State  
#define ST_AUTH_DIGEST_RCVD                  2 //Digest Handshake State  
//NTLM Handshake States
#define ST_AUTH_NTLM_RCVD                    3
#define ST_AUTH_NTLM_TYPE2_RCVD              4
#define ST_AUTH_KERBEROS_RCVD                5 //Kerberos Handshake State  
#define ST_AUTH_KERBEROS_CONTINUE            6 //Kerberos Handshake continues

#define ST_AUTH_HANDSHAKE_FAILURE            9

//NTLM Versions
#define NTLM_VER_NTLMv1                      0
#define NTLM_VER_NTLM2                       1
#define NTLM_VER_NTLMv2                      2

#define MAX_NTLM_PKT_SIZE  256*1024
#define MAX_NTLM_LOG_SIZE  MAX_NTLM_PKT_SIZE * 2

#define AUTH_ERROR                     -1
#define AUTH_SUCCESS                    0

#define HANDLE_INVALID_AUTH_MSG(ntlm_pkt, ntlm_pkt_size, err_msg)  \
{ \
  auth_handle_invalid_msg(cptr, vptr, err_msg, ntlm_pkt, ntlm_pkt_size);   \
}



extern int proc_http_hdr_auth_ntlm(connection *cptr, char *buf, int bytes_left, int *bytes_consumed, u_ns_ts_t now);
extern int auth_create_authorization_hdr(connection *cptr, int *body_start_idx, int grp_idx, int proxy_chain, int http2, int push_flag);
extern int auth_create_authorization_h2_hdr(connection *cptr, int grp_idx,  int proxy_chain, int push_flag);
extern int auth_handle_response(connection *cptr, int status, u_ns_ts_t now);
extern void auth_validate_handshake_complete(connection *cptr);
extern int kw_set_g_http_auth_ntlm(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag);
extern int kw_set_g_http_auth_kerb(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag);
extern int proc_http_hdr_auth_proxy(connection *cptr, char *buf, int bytes_left, int *bytes_consumed, u_ns_ts_t now);
extern inline void parse_authenticate_hdr(connection *cptr, char *header_buffer, int header_buffer_len, int *consumed_bytes, u_ns_ts_t now);
extern int check_and_set_single_auth(connection *cptr, int grp_idx);
#endif
