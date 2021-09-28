#ifndef NS_SOCKET_H
#define NS_SOCKET_H

#include "ns_socket_tcp_client_rpt.h"
#include "ns_socket_tcp_client_failures_rpt.h"

#include "ns_socket_udp_client_rpt.h"
#include "ns_socket_udp_client_failures_rpt.h"

#define SOCKET_MAX_ATTR_LEN     1024

#define DELTA_API_ID_ENTRIES    100

#define MAX_ID_LEN              32

#define TCP_PROTO               0
#define UDP_PROTO               1

#define NS_SOCKET_TCP_CLIENT              0x01
#define NS_SOCKET_TCP_SERVER              0x02
#define NS_SOCKET_UDP_CLIENT              0x04
#define NS_SOCKET_UDP_SERVER              0x08


#define SSL_DISABLE             0
#define SSL_ENABLE              1

#define CONNECT_TIMEOUT         60000    //60 secs
#define SEND_TIMEOUT            60000    //60 secs
#define READ_TIMEOUT_FB         60000    //60 secs
#define READ_TIMEOUT_IDLE       60000    //60 secs
                                
#define LEN_TYPE_TEXT           0
#define LEN_TYPE_BINARY         1
#define LEN_TYPE_HEX            2
                                
#define MSG_TYPE_TEXT           0
#define MSG_TYPE_BINARY         1
#define MSG_TYPE_HEX            2
#define MSG_TYPE_BASE64         3
                                
#define ENC_NONE                0 
#define ENC_BINARY              1 
#define ENC_HEX                 2
#define ENC_BASE64              3
#define ENC_TEXT                4
                                
#define SAVE_ON_FOUND           0
#define SAVE_ON_NOT_FOUND       1
#define DISCARD_ON_FOUND        2
#define DISCARD_ON_NOT_FOUND    3

#define CONTAINS_START          0
#define CONTAINS_INSIDE         1
#define CONTAINS_END            2

//Message format length bytes
#define NS_SOCKET_MSGLEN_BYTES_CHAR         1
#define NS_SOCKET_MSGLEN_BYTES_SHORT        2
#define NS_SOCKET_MSGLEN_BYTES_INT          4
#define NS_SOCKET_MSGLEN_BYTES_LONG         8


#define NS_PROTOSOCKET_READ_ENDPOLICY_NONE             0x00
#define NS_PROTOSOCKET_READ_ENDPOLICY_LENGTH_BYTES     0x01
#define NS_PROTOSOCKET_READ_ENDPOLICY_READ_BYTES       0x02
#define NS_PROTOSOCKET_READ_ENDPOLICY_SUFFIX           0x04
#define NS_PROTOSOCKET_READ_ENDPOLICY_TIMEOUT          0x08
#define NS_PROTOSOCKET_READ_ENDPOLICY_CLOSE            0x10
#define NS_PROTOSOCKET_READ_ENDPOLICY_CONTAINS         0x20

#define SOCKET_DISABLE_RECV                            0x01
#define SOCKET_DISABLE_SEND                            0x02

#define SET_SOCK_OPT                                   0
#define GET_SOCK_OPT                                   1

#define SLOCAL_ADDRESS                                 0
#define SLOCAL_HOSTNAME                                1
#define SLOCAL_PORT                                    2
#define SREMOTE_ADDRESS                                3
#define SREMOTE_HOSTNAME                               4
#define SREMOTE_PORT                                   5

#define NOTEXCLUDE_SOCKET                              0
#define EXCLUDE_SOCKET                                 1

#define SOPEN                                          0
#define SSEND                                          1
#define SRECV                                          2
#define SCLOSE                                         3

#define SLITTLE_ENDIAN                                 0
#define SBIG_ENDIAN                                    1

#define IS_TCP_CLIENT_API_EXIST                                                \
   (global_settings->protocol_enabled & SOCKET_TCP_CLIENT_PROTO_ENABLED)

#define IS_UDP_CLIENT_API_EXIST                                                \
   (global_settings->protocol_enabled & SOCKET_UDP_CLIENT_PROTO_ENABLED)

#define CHECK_AND_EXIT_IF_ID_IS_NOT_PROVIDED                                   \
{                                                                              \
  if(id_flag == -1)                                                            \
    SCRIPT_PARSE_ERROR(script_line, "ID is a mandatory argument "              \
        "and should be provided in the API");                                  \
} 

#define CHECK_AND_RETURN_IF_EXCLUDE_FLAG_SET                                   \
{                                                                              \
  if(requests[g_socket_vars.proto_norm_id_mapping_2_action_tbl[norm_id]].proto.socket.flag == EXCLUDE_SOCKET) \
  {                                                                            \
    NSTL1(NULL, NULL, "Socket id = [%s] is excluded, hence returning", attribute_value); \
    return NS_PARSE_SCRIPT_SUCCESS;                                            \
  }                                                                            \
}

#define NS_SOCKET_GET_CPTR(cptr, vptr)                                         \
{\
  cptr = vptr->conn_array[request_table_shr_mem[vptr->next_pg_id].proto.socket.norm_id]; \
}

#define CHECK_READ_END_POLICY_AND_SWITCH                                       \
{                                                                              \
  if(cptr->timer_ptr->timer_type == AB_TIMEOUT_IDLE)                           \
    dis_timer_del(cptr->timer_ptr);                                            \
  cptr->conn_state = CNST_IDLE;                                                \
  ns_socket_modify_epoll(cptr, -1, EPOLLOUT);                                  \
  copy_url_resp(cptr);                                                         \
  copy_cptr_to_vptr(vptr, cptr);                                               \
  do_data_processing(cptr, now, VAR_IGNORE_REDIRECTION_DEPTH, 1);              \
  if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_USER_CONTEXT) \
    switch_to_vuser_ctx(vptr, "Socket Read Done");                             \
  else if(runprof_table_shr_mem[vptr->group_num].gset.script_mode == NS_SCRIPT_MODE_SEPARATE_THREAD) \
    send_msg_nvm_to_vutd(vptr, NS_API_SOCKET_READ_REP, 0);                     \
  free_cptr_buf(cptr);                                                         \
}

#define UPDATE_PAGE_SESS_STATUS(vptr, status)                                  \
{                                                                              \
  vptr->page_status = status;                                                  \
  vptr->sess_status = status;                                                  \
} 

//Socket Request structures are declared in file .. 

typedef struct
{
  int conn_to;     //Connect timeout
  int send_to;     //Max send timeout
  int send_ia_to;  //Send inactivity timeout
  int recv_to;     //Max receive timeout
  int recv_ia_to;  //receive inactivity timeout
  int recv_fb_to;  //receive firstbyte timeout
}SocketSettings;

typedef struct
{
  char socket_disable;
  int max_socket_conn;
  int write_buf_len;
  int max_api_id_entries;
  int cur_post_buf_len;
  int *proto_norm_id_mapping_2_action_tbl;
  NormObjKey proto_norm_id_tbl;
  SocketSettings socket_settings;
}GlobalSocketData;

#ifndef CAV_MAIN
extern GlobalSocketData g_socket_vars;
#else
extern __thread GlobalSocketData g_socket_vars;
#endif
extern SocketSettings socket_settings;

extern float g_conn_timeout;
extern float g_send_timeout;
extern float g_recv_timeout;
extern float g_recv_timeout2;

extern NormObjKey SocketNormIdTable;
extern int init_socket_default_values();
extern int ns_parse_socket_open(FILE *flow_fp, FILE *outfp,  char *flow_filename, char *flowout_filename, int sess_idx);
extern int ns_parse_socket_send(FILE *flow_fp, FILE *outfp,  char *flow_filename, char *flowout_filename, int sess_idx);
extern int ns_parse_socket_recv(FILE *flow_fp, FILE *outfp,  char *flow_filename, char *flowout_filename, int sess_idx);
extern int ns_parse_socket_close(FILE *flow_fp, FILE *outfp, char *flow_filename, char *flowout_filename, int sess_idx);

extern int ns_parse_socket_c_apis(FILE *flow_fp, FILE *outfp, char *flow_filename, char *flowout_filename, int sess_idx, char *api_name);

extern int nsi_socket_send(VUser *vptr);
extern int ns_socket_ext(VUser *vptr, int socket_api_id, int socket_api_flag);
extern int nsi_socket_read(VUser *vptr);
extern int nsi_socket_recv(VUser *vptr);
extern int nsi_get_or_set_sock_opt(VUser *vptr, int action_idx, char *level, char *optname, void *optval, socklen_t optlen, int set_or_get);
extern connection* nsi_get_cptr_from_sock_id(VUser *vptr, int action_idx);
extern int nsi_get_socket_attribute_from_name(char* attribute_name);

extern int ns_socket_modify_epoll(connection *cptr, int action_idx, int event);
extern int ns_parse_socket_exclude(FILE *flow_fp, FILE *outfp, char *flow_filename, char *flowout_filename, int sess_idx);
extern int ns_parse_connect_timeout(FILE *flow_fp, FILE *outfp, char *flow_filename, char *flowout_filename, int sess_idx);
extern int ns_parse_recv_timeout2(FILE *flow_fp, FILE *outfp, char *flow_filename, char *flowout_filename, int sess_idx);

extern int ns_socket_disable_ex(int action_idx, char *operation);
extern int ns_socket_enable_ex(int action_idx, char *operation);
extern int ns_parse_send_timeout(FILE *flow_fp, FILE *outfp, char *flow_filename, char *flowout_filename, int sess_idx);
extern int ns_parse_recv_timeout(FILE *flow_fp, FILE *outfp, char *flow_filename, char *flowout_filename, int sess_idx);
extern int process_socket_recv_data(void *cptr_in, char *buf, int bytes_read, int *read_offset, int msg_peek);
extern void init_global_timeout_values();

extern int ns_write_excluded_api_in_tmp_file(char *api_to_run, FILE *flow_fp, char *flow_file, char *line_ptr,
                                         FILE *outfp, char *flow_outfile, char *api_attributes);

extern int ns_socket_get_num_msg_ex(VUser *vptr);
extern inline int before_read_start(connection *cptr, u_ns_ts_t now, action_request_Shr **url_num, int *bytes_read);
extern inline void ns_socket_bytes_to_read(connection *cptr, VUser *vptr, int *bytes_read, int *msg_peek);
extern int ns_socket_set_version_cipher(VUser *vptr, char *version, char *ciphers);
extern void upgrade_ssl(connection *cptr, u_ns_ts_t now);
extern int ns_socket_open_ex(VUser *vptr);

#endif
