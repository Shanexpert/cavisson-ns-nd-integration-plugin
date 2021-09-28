#ifndef ns_websock_h__
#define ns_websock_h__

//typedef struct ws_callback 
//{
//  char *cb_name;	 	/* Store name of call back function */
//  union
//  {
//    opencbfn_type opencb_ptr;	/* Store address of open callback function */
//    sendcbfn_type sendcb_ptr;	/* Store address of send callback function */
//    msgcbfn_type msgcb_ptr;	/* Store address of message callback func */
//    errorcbfn_type errorcb_ptr;	/* store address of Error callback function */
//  }
//}ws_callback;

extern char *g_WebsocketCB;

#define WS_SSL_ERROR -1
#define WS_SSL_PARTIAL_WRITE -2


/* WS Request component length */
#define HOST_HEADER_STRING_LENGTH                     6
#define METHOD_LENGTH                                 4
#define UPGRADE_BUF_STRING_LENGTH                     20 
#define CONN_BUF_STRING_LENGTH                        21
#define SEC_BUF_STRING_LENGTH                         19
#define SEC_WS_PROTO_BUF_STRING_LENGTH                41
#define SEC_WS_PROTO_EXT_BUF_STRING_LENGTH            69
#define SEC_WS_VERSION_STRING_LENGTH                  27
#define WS_USER_AGENT_STRING_LENGTH                   12 
#define WS_ACCEPT_ENCODING_STRING_LENGTH              38 
#define WS_HTTP_VERSION_STRING_LENGTH                 11

/* WS parsing buffers length */
#define WS_MAX_HRD_LEN                                4 * 1024
#define WS_MAX_ATTR_LEN                               1024

#define WS_CB_ENTRIES                                 1024
#define WS_SEND_ENTRIES                               1024
#define WS_CLOSE_ENTRIES                              1024
#define BUFFER_LENGTH                                 128

#define SYSTEM_RANDOM_FILEPATH                        "/dev/urandom"
#define MAX_MUX_RECURSION                             2
#define WS_SEND_BUFFER_PRE_PADDING                    (4 + 10 + (2 * MAX_MUX_RECURSION))
#define WS_SEND_BUFFER_POST_PADDING                   4

typedef void (* opencbfn_type)(const char *connection_id, const char *accumulated_headers_str, int accumulated_headers_len);
typedef void (* msgcbfn_type)(const char *connection_id, int isBinary, const char *data, int length);
typedef void (* errorcbfn_type)(const char *connection_id, const char *msg, int length);
typedef void (* closecbfn_type)(const char *connection_id, int isClosedByClient, int code, char *reason, int length);

typedef struct ws_callback
{
  ns_bigbuf_t cb_name;        /* Store name of call back function */
  opencbfn_type opencb_ptr;   /* Store address of open callback function */
  msgcbfn_type msgcb_ptr;     /* Store address of message callback func */
  errorcbfn_type errorcb_ptr; /* store address of error callback function */
  closecbfn_type closecb_ptr; /* store address of close callback function */
} ws_callback;

typedef struct ws_send_table
{
  int id;                 /* Connection ID */
  StrEnt send_buf;        /* Store Send message */
  char isbinary;          /* Given value is in binary or not. 1- Binary, 0- other */
  int ws_idx;             /* Point WebSocket Request Table */
} ws_send_table;

typedef struct ws_callback_shr
{   
  char* cb_name;              /* Store name of call back function */
  opencbfn_type opencb_ptr;   /* Store address of open callback function */
  msgcbfn_type msgcb_ptr;     /* Store address of message callback func */
  errorcbfn_type errorcb_ptr; /* store address of error callback function */
  closecbfn_type closecb_ptr; /* store address of close callback function */
} ws_callback_shr;

typedef struct ws_send_table_shr
{
  int id;                   /* Connection ID */
  StrEnt_Shr send_buf;      /* Store Send message */
  char isbinary;            /* Given value is in binary or not. 1- Binary, 0- other */
  int ws_idx;               /* Point WebSocket Request Table */
} ws_send_table_shr;

typedef struct ws_close_table
{
  int conn_id;             /*Connection ID */
  int status_code;         /*Satatus code */
  char reason[4 * 1024];   /*Reason for close the connection  */
} ws_close_table;


typedef struct ws_close_table_shr
{
  int conn_id;             /*Connection ID */
  int status_code;         /*Satatus code */
  char reason[4 * 1024];   /*Reason for close the connection  */
} ws_close_table_shr;

enum ns_websocket_opcodes_07
{
  LWS_WS_OPCODE_07__CONTINUATION = 0,
  LWS_WS_OPCODE_07__TEXT_FRAME = 1,
  LWS_WS_OPCODE_07__BINARY_FRAME = 2,

  LWS_WS_OPCODE_07__NOSPEC__MUX = 7,

  /* control extensions 8+ */

  LWS_WS_OPCODE_07__CLOSE = 8,
  LWS_WS_OPCODE_07__PING = 9,
  LWS_WS_OPCODE_07__PONG = 0xa,
};

enum libwebsocket_write_protocol {
  LWS_WRITE_CONTINUATION,
  LWS_WRITE_TEXT, 
  LWS_WRITE_BINARY,
  LWS_WRITE_HTTP,
                  
  /* special 04+ opcodes */
                  
  LWS_WRITE_CLOSE,
  LWS_WRITE_PING, 
  LWS_WRITE_PONG, 
                  
  /* Same as write_http but we know this write ends the transaction */
  LWS_WRITE_HTTP_FINAL,
                  
  /* HTTP2 */
                  
  LWS_WRITE_HTTP_HEADERS,
  
  /* flags */

  LWS_WRITE_NO_FIN = 0x40,
  /*
   * client packet payload goes out on wire unmunged
   * only useful for security tests since normal servers cannot
   * decode the content if used
   */
  LWS_WRITE_CLIENT_IGNORE_XOR_MASK = 0x80
};

enum ws_frame_parse_state
{
  NEW_FRAME,

  FRAME_HDR_LEN,
  FRAME_HDR_LEN16_2,
  FRAME_HDR_LEN16_1,
  FRAME_HDR_LEN64_8,
  FRAME_HDR_LEN64_7,
  FRAME_HDR_LEN64_6,
  FRAME_HDR_LEN64_5,
  FRAME_HDR_LEN64_4,
  FRAME_HDR_LEN64_3,
  FRAME_HDR_LEN64_2,
  FRAME_HDR_LEN64_1,

  FRAME_MASK_KEY1,
  FRAME_MASK_KEY2,
  FRAME_MASK_KEY3,
  FRAME_MASK_KEY4,

  FRAME_PAYLOAD,

  PAYLOAD_UNTIL_LENGTH_EXHAUSTED
};

extern int max_ws_conn;
extern ws_callback *g_ws_callback;
extern ws_send_table *g_ws_send;
extern unsigned short int ws_idx_list[65535];

extern ws_callback_shr *g_ws_callback_shr;
extern ws_send_table_shr *g_ws_send_shr;

extern int total_ws_callback_entries;

extern void copy_websocket_send_table_to_shr();
extern void copy_websocket_callback_to_shr();

extern int create_webSocket_send_table_entry(int* row_num);
extern int create_websocket_callback_table_entry(int* row_num);
extern int proc_ws_hdr_upgrade(connection *cptr, char *buf, int byte_left, int *bytes_consumed, u_ns_ts_t now);
extern int proc_ws_hdr_sec_websocket_accept(connection *cptr, char *buf, int byte_left, int *bytes_consumed, u_ns_ts_t now);
extern void copy_websocket_send_table_to_shr(void);
extern int nsi_websocket_send(VUser *vptr);
extern int ns_websocket_ext(VUser *vptr, int ws_api_id, int ws_api_flag);
extern int nsi_websocket_close(VUser *vptr);
extern void copy_websocket_close_table_to_shr(void);
extern void handle_send_ws_frame(connection* cptr, u_ns_ts_t now);
extern void send_ws_connect_req(connection *cptr, int ws_size, u_ns_ts_t now);
extern int make_ws_request(connection* cptr, int *ws_req_size, u_ns_ts_t now);
extern int handle_frame_read(connection *cptr, unsigned char *frame, size_t frame_len, char **payload_start);
extern int ns_parse_websocket_close(FILE *flow_fp, FILE *outfp, char *flow_filename, char *flowout_filename, int sess_idx);
extern void ws_resp_timeout(ClientData client_data, u_ns_ts_t now);
extern int nsi_web_websocket_read(VUser *vptr, int con_id, int timeout);
extern void websocket_close_connection(connection* cptr, int done, u_ns_ts_t now, int req_ok, int completion);
extern inline void fill_ws_status_codes(avgtime **g_avg);
extern int set_api(char *api_to_run, FILE *flow_fp, char *flow_file, char *line_ptr, FILE *outfp,  char *flow_outfile, int send_tb_idx);
#endif
extern int parse_uri(char *in_uri, char *host_end_markers, int *request_type, char *hostname, char *path);
