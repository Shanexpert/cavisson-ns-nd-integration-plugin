/***********************************************************
* URL parsing
* SM, 12-05-2014: Added attribute PAGELOADWAITTIME
***********************************************************/
#ifndef URL_H
#define URL_H 1
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "ns_http_cache_table.h"
#include "ns_data_types.h"
#include "ns_rbu.h"
#include "nslb_json_parser.h"
#include "ns_server.h"
#include "ns_protobuf.h"

enum attribute_type{
/* Any change should be done to array attribute_name[][] also, which is in ns_click_script_parse.c */
  APINAME,
  ALT,
  TAG,
  VALUE,
  TYPE,
  ID,
  ORDINAL,
  ACTION,
  ONCLICK,
  NAME,
  CONTENT,
  SHAPE,
  COORDS,
  TITLE,
  SRC,
  TEXT,
  HREF,
  URL,
  XPATH,
  XPATH1,
  XPATH2,
  XPATH3,
  XPATH4,
  XPATH5,
  HARFLAG,  //Flag to create har file in case of RBU.
  IFRAMEID,
  IFRAMEXPATH,
  IFRAMEXPATH1,
  IFRAMEXPATH2,
  IFRAMEXPATH3,
  IFRAMEXPATH4,
  IFRAMEXPATH5,
  IFRAMECLASS,
  IFRAMEDOMSTRING,
  IFRAMECSSPATH,
  PAGELOADWAITTIME,
  DOMSTRING,
  FOCUSONLY,
  MERGEHARFILES,
  SCROLLPAGEX,  // This represents width (x axis)
  SCROLLPAGEY,  // This represents height (y axis)
  CSSPATH,
  CSSPATH1,
  HarLogDir,
  BrowserUserProfile,
  VncDisplayId,
  HarRenameFlag,
  ScrollPageX,
  ScrollPageY,
  PrimaryContentProfile,
  SPAFRAMEWORK,
  COOKIES,
  CLIPINTERVAL,
  CLASS,
  WaitForNextReq,
  WaitForActionDone,
  WaitUntil,
  PhaseInterval,
  PerformanceTraceLog,
  AuthCredential,
  COORDINATES,
  propertyName,
  propertyName1,
  valueType,
  abortTest,
  OPERATOR,
  AUTOSELECTOR,
  NUM_ATTRIBUTE_TYPES
};

#ifndef MAX_DATA_LINE_LENGTH
  #define MAX_DATA_LINE_LENGTH 8192
#endif
#define BIG_DATA_LINE_LENGTH 8192 
#define MAX_ERR_MSG_LENGTH 4096
typedef int (*nextpagefn_type)(void);
typedef int (*prepagefn_type)(void);
typedef int (*initpagefn_type)(void);
typedef void (*exitpagefn_type)(void);
#define DUMMY 0X01
typedef void (*runlogicfn_type)(void); // AN-CTX

// Achint start
typedef void (*preurlfn_type)(void);
typedef void (*posturlfn_type)(void);
// Achint End


typedef double (*custom_delay_fn_type)(void);
typedef double (*custom_page_think_time_fn_type)(void);

typedef struct SegTableEntry {
  int type;
  
  unsigned short pb_field_number;
    /* pb_field_number: this field will Protobuf field number */
  unsigned short pb_field_type;
    /* pb_field_type: this field will provide Protobuf wire type - int32, int64 etc */

  int offset; /* offset into big buffer (for string segments), or variable table, will be used to save repeat table idx for SEGMENT */
  int sess_idx;
  ns_bigbuf_t data; /*offset into big buffer for index value in case of ORD_ALL variable */
} SegTableEntry;

typedef struct RepeatBlock {
  char repeat_count_type; // Will be used to save the condition on which repeat block will be repeated. Its values can be: Count/Value/Num
  int hash_code; // Will be used to save hash code of variable in case of count and value
  ns_bigbuf_t data; // It will save variable name coming in count and value 
  ns_bigbuf_t rep_sep; // It will be used to save repeat separator for repeat block
  int rep_sep_len; // It will save repeat separator len
  int repeat_count; // It will be used to save repeat count in case of NUM
  int num_repeat_segments; // 
  int agg_repeat_segments;
} RepeatBlock;

/* for SegTableEntry.type */
#define STR                           1
#define VAR                           2
#define COOKIE_VAR                    3
#define NSL_VAR                       4
#define TAG_VAR                       5
#define DYN_VAR                       6
#define SEARCH_VAR                    7
#ifdef RMI_MODE
  #define UTF_VAR                     8
  #define BYTE_VAR                    9 
  #define LONG_VAR                    10
#endif
#define CLUST_VAR                     11
#define GROUP_VAR                     12
#define GROUP_NAME_VAR                13
#define CLUST_NAME_VAR                14
#define USERPROF_NAME_VAR             15
#define HTTP_VERSION_VAR              16 //Anuj for HTTP_VERSION testing
#define RANDOM_VAR                    17       //for Random Vars
#define INDEX_VAR                     18        // Index Var
#define RANDOM_STRING                 19   
#define UNIQUE_VAR                    20 //for unique vars  
#define DATE_VAR                      21
#define SEGMENT		              22  //for segment handling.
#define JSON_VAR                      23  //for JSON vars
#define UNIQUE_RANGE_VAR              24  // for API nsl_unique_range_var()
#define PROTOBUF_MSG                  25

/* body_encoding_flag values */
#define BODY_ENCODING_URL_ENCODED     1  // set if Content-Type is x-www-form-urlencoded
#define BODY_ENCODING_AMF             2  // set if Content-Type is application/x-amf
#define BODY_ENCODING_HESSIAN         3  // set if Content-Type is application/x-hessian or x-application/hessian
#define BODY_ENCODING_JAVA_OBJ        4  // set if Content-Type is application/octet-stream 
#define BODY_ENCODING_PROTOBUF        5  // set if Content-Type is application/x-protobuf or x-application/protobuf
#define BODY_ENCODING_GRPC_PROTO      6  // set if Content-Type is application/grpc or application/grpc+proto
#define BODY_ENCODING_GRPC_JSON	      7  // set if Content-Type is application/grpc+json
//#define BODY_ENCODING_GRPC_CUSTOM     8  // set 

/*Feature Flag*/
#define NS_REQ_FLAGS_GRPC_CLIENT		0X0000000000000001  //set for GRPC client API
#define NS_REQ_FLAGS_WHITELIST          	0x0000000000000002  //set for WhiteList header info

/* Header Flag*/
#define NS_REQ_HDR_FLAGS_CONTENT_TYPE		0x0000000000000001   //set CONTENT_TYPE header
#define NS_REQ_HDR_FLAGS_USER_AGENT		0x0000000000000002   //set USER_AGENT header
#define NS_REQ_HDR_FLAGS_TE			0x0000000000000004   //set TE header
#define NS_REQ_HDR_FLAGS_GRPC_ACCEPT_ENCODING	0x0000000000000008   //set GRPC_ACCEPT_ENCODING header

//Http inline flag 
#define HTTP_NO_INLINE      0
#define HTTP_WITH_INLINE    1
#define GRPC_CLIENT         2

#define GRPC_REQ (cptr->url_num->flags & NS_REQ_FLAGS_GRPC_CLIENT)
#define GRPC_PROTO_REQUEST (cptr->url_num->proto.http.body_encoding_flag == BODY_ENCODING_GRPC_PROTO)
#define GRPC_PROTO_RESPONSE (vptr->flags & NS_VPTR_CONTENT_TYPE_GRPC_PROTO)
//#define GRPC_JSON_REQUEST (cptr->url_num->proto.http.body_encoding_flag == BODY_ENCODING_GRPC_JSON)
typedef struct StrEnt {
  u_ns_ptr_t seg_start; /* offset into segment table */
  int num_entries;
} StrEnt;

typedef struct ServerOrderTableEntry {
  unsigned int server_idx; /* index into the gserver table */
} ServerOrderTableEntry;

typedef struct ReqCookTab {
  u_ns_ptr_t cookie_start; /* index into the reqcook table */
  int num_cookies;
} ReqCookTab;

typedef struct ReqDynVarTab {
  u_ns_ptr_t dynvar_start; /* index into the dynvar table */
  int num_dynvars;
} ReqDynVarTab;

#ifdef RMI_MODE
typedef struct ReqByteVarTab {
   u_ns_ptr_t bytevar_start; /* index into the bytevar table */
   int num_bytevars;
} ReqByteVarTab;
#endif

typedef struct 
{
  short encryption_algo;
  char base64_encode_option;
  char key_size;
  char ivec_size;
  StrEnt key;
  StrEnt ivec; 
} BodyEncryptionArgs;

/****** CLICK and SCRIPT example*******************
 *
 * SCRIPT:
 ns_browser ("MacysHomePage",
              "browserurl=http://192.168.1.73:81/test_click.html");

  ns_link ("home_furnishings",
            "type=imageLink",
            "action=click",
            attributes =[
               "alt=bed & bath"
               "linkurl=http://www1.macys.com/shop/for-the-home?id=22672&edge=hybrid",
               "tag=img"
            ]
      );

  ns_link ("bed_&_bath",
            "type=imageLink",
            "action=click",
            "content=bed & bath",
            attributes =[
               "linkurl=http://www1.macys.com/shop/for-the-home?id=22672&edge=hybrid",
               "tag=img"
            ]
      );
*/

typedef struct ClickActionTableEntry{ // click actions as per recorded click script
  StrEnt att[NUM_ATTRIBUTE_TYPES];     // tag exmaple INPUT, IMG, A etc.
}ClickActionTableEntry;

typedef struct SockJs_Param
{
  int conn_id;             /*Connection ID */
}SockJs_Param;

typedef struct SockJs_Param_Shr
{
  int conn_id;             /*Connection ID */
}SockJs_Param_Shr;

typedef struct http_request {
  /* entries below can be in any order; depending on shr mem creation */
  int pct; /* percentage of request */
  int content_length;

  //url_index is Id of URLs coming from script like 1,2,3
  //and combination of NVM_ID & sequence number for dynamic urls
  unsigned int url_index;

  StrEnt url_without_path; // url_without_path: stores url part before #?/ 
  StrEnt url;  //url contains URL's path i.e url part after '#?/' 
  StrEnt hdrs;
  StrEnt auth_uname;/*Added for NTLM support */
  StrEnt auth_pwd;/*Added for NTLM support */
  StrEnt repeat_inline;/*Added for repeat of inline URL */
  u_ns_ptr_t post_idx; /* idx into str table */
  int type;     // URL Type- Main, Embedded, Redirect
  char *redirected_url; /* This is malloc'ed to store Request line for the redirect URL. 
                       * The redirect URL is extracted from Location header of the main/embedded url */
  int got_checksum;
  int checksum;
  int url_got_bytes;  //Note: This field is also used to store media file duration in case if media streaming is enable in NS
  ReqCookTab cookies;
  ReqDynVarTab dynvars;
#ifdef RMI_MODE
  ReqByteVarTab bytevars;
#endif
  unsigned char http_method;  // Get, Post etc
  unsigned char http_method_idx; // Index in array of all HTTP method names. Names have one space after that
  short tx_ratio;
  short rx_ratio;
  short exact;
  int bytes_to_recv;
  int first_mesg_len;
  short keep_conn_flag;
  short body_encoding_flag;
  int header_flags;
  int tx_idx;
  u_ns_ptr_t tx_prefix; //Used to store transaction prefix in case of repeat
#ifdef WS_MODE_OLD
  int wss_idx; /* idx into webspec table */
#endif

  char http_version;   // 0 for HTTP/1.0 and 1 for HTTP/1.1
 
  //#Shilpa 16Feb2011 Bug#2037
  //Implementing Client Freshness Constraint
  CacheRequestHeader cache_req_hdr;
  RBU_Param rbu_param;
  BodyEncryptionArgs body_encryption_args;

  SockJs_Param sockjs;
  ProtobufUrlAttr protobuf_urlattr;
} http_request;

typedef struct smtp_request {
  short int num_to_emails;
  short int num_cc_emails;
  short int num_bcc_emails;
  short int num_attachments;


  StrEnt user_id;
  StrEnt passwd;
  ns_str_ent_t to_emails_idx;
  ns_str_ent_t cc_emails_idx;
  ns_str_ent_t bcc_emails_idx;
  StrEnt from_email;
  ns_str_ent_t body_idx;  //email body
  //StrEnt subject_idx; // subject line
  //StrEnt msg_count;
  short int msg_count_min;
  short int msg_count_max;
  StrEnt hdrs;  // 0 or more hdrs
  ns_str_ent_t attachment_idx;
  int enable_rand_bytes;
  int rand_bytes_min;
  int rand_bytes_max;
  int authentication_type; //indicates ssl/non-ssl authentication
} smtp_request;

typedef struct pop3_request {
  int pop3_action_type; // indicates if its a stat, list or get command
  int authentication_type; //indicates ssl/non-ssl authentication
  StrEnt user_id;
  StrEnt passwd;

} pop3_request;

typedef struct ftp_request {
  int ftp_action_type; // indicates if its a RETR or STOR command
  StrEnt user_id;
  StrEnt passwd;
  StrEnt ftp_cmd;
  int file_type;
  int num_get_files;
  ns_str_ent_t get_files_idx;
  char passive_or_active;
} ftp_request;


//jrmi request 
typedef struct jrmi_request {
  int jrmi_protocol; 
  char method[1024];
  char server[1024];
  int port;
  short no_param;
  StrEnt object_id;
  StrEnt number;
  StrEnt count;
  StrEnt time;
  StrEnt method_hash;
  StrEnt operation;
  u_ns_ptr_t post_idx;
  //StrEnt method;
}jrmi_request;

//ws request
typedef struct ws_request {  
  int  conn_id;         /* This ID will indicate websocket connect id */
  StrEnt uri;           /* Store WebSocket Connect URI */
  StrEnt uri_without_path;           /* Store WebSocket Connect URI */
  StrEnt hdrs;          /* Store header */
  ns_bigbuf_t origin;   /* Store Origin server*/
  int  opencb_idx;      /* Store index of open_callback function which will be stored on WebSocket_CB_Table */
  int  sendcb_idx;      /* Store index of send_callback function which will be stored on WebSocket_CB_Table */
  int  msgcb_idx;       /* Store index of message_callback function which will stored on WebSocket_CB_Table */
  int  errorcb_idx;     /* Store index of error_callback function which will be stored on WebSocket_CB_Table */
  //ReqCookTab cookies;   /* Store cookies */
}ws_request;

//fc2 request
typedef struct fc2_request {
  StrEnt uri;           /* Store fc2 Connect URI */
  StrEnt message;        // for message 
}fc2_request;

typedef struct xmpp_request {
      char action;
      char user_type;
      char accept_contact;
      int starttls;          //for tls
      StrEnt user;       // for username
      StrEnt password;       // for login passwd
      StrEnt domain;         // for domain
      StrEnt sasl_auth_type; // for auth type
      StrEnt group;           // for file
      StrEnt message;        // for message 
      StrEnt file;           // for file
}xmpp_request;

//ldap request
typedef struct {
  int operation; // to indicate operation type like add, delete, search, login, update, modify
  int type;

  char user[512];
  char password[512];
 
  StrEnt dn;  // Used for dn in add, delete and used for base dn in search
  StrEnt username; // for login id 
  StrEnt passwd; // for login passwd
 
  StrEnt scope;  // will be used in search request 
  StrEnt filter; // will be used in search operation for filters
  StrEnt base; // will be used in search operation for filters
  StrEnt deref_aliases; // will be used in search operation for filters
  StrEnt time_limit; // will be used in search 
  StrEnt size_limit; // will be used in search 
  StrEnt types_only; // will be used in search 
  StrEnt mode; // will be used in search 
 
  StrEnt attributes;  // for attributes and their values (used for name value pair in add api, and for atrribute names only in search api)
  StrEnt attr_value;
  StrEnt attr_name;
} ldap_request;

typedef struct dns_request {
  StrEnt name;        //DNS resource to lookup
  char qtype;        // DNS query type could be -- A|NS|MD|MF|CNAME|SOA|MB|MG|MR|NULL|WKS|MX|PTR||HINFO|MINFO|TXT
  char recursive;     //query is recursive or not
  char proto;        // UDP/TCP
  StrEnt assert_rr_type;      //whether to assert that atleast one RR of this type was returned, see types above
  StrEnt assert_rr_data;   
} dns_request;

typedef struct imap_request {
  int imap_action_type; // indicates if its a store, list, fetch ..... command
  int authentication_type;  //for ssl
  StrEnt user_id;
  StrEnt passwd;
  StrEnt mail_seq;
  StrEnt fetch_part;
} imap_request;

typedef struct
{
  char len_bytes;                  /*Provide Length Format - Bytes Len-type B(1/2/4/8) and Len-type T(1 - 20)*/
  char len_type;                   /*Provide Length Format - Type (Binary/Text)*/
  char len_endian;                 /*Provide Length Format - Endianness (Big/Little)*/
  char msg_type;                   /*Provide Message Format - Type(text/binary/hex/base64)*/
  char msg_enc_dec;                /*Provide Message Encoding/Decoding(binary/hex/base64/none)*/
  StrEnt prefix;                   /*Provide Message prefix*/
  StrEnt suffix;                   /*Provide Message suffix*/
}ProtoSocket_MsgFmt;

typedef struct
{
  char protocol;                   /*Provide socket stream type (TCP/UDP)*/
  char ssl_flag;                   /*Provide SSL is enable or not*/
  int backlog;                     /*Provide max waiting connections*/
  StrEnt local_host;               /*Provide Local IP:PORT*/
  StrEnt remote_host;              /*Provide Remote IP:PORT*/
}ProtoSocket_Open;

typedef struct
{
  ProtoSocket_MsgFmt msg_fmt;      /*Provide message format specifications*/ 
  StrEnt buffer;                   /*Provide buffer for send*/  
  long buffer_len;                 /*Provide max length of buffer that can be sent according to len-byte*/
}ProtoSocket_Send;

typedef struct
{
  ProtoSocket_MsgFmt msg_fmt;      /*Provide message format specifications*/
  char end_policy;                 /*Provide end policy for reading data*/ 
  char msg_contains_ord;           /*Provide message contains place (Start/End)*/
  char msg_contains_action;        /*Provide what action need to take*/
  int fb_timeout_msec;            /*Provide timeout to first byte*/
  StrEnt msg_contains;             /*Provide message contains*/
  ns_bigbuf_t buffer;              /*Provide buffer for read*/ 
  long buffer_len;                 /*Provide max length of buffer that can be read according to len-byte*/ 
}ProtoSocket_Recv;

typedef struct
{
  char operation;                         /* This is common to all Socket APIs - Open+Send+Read so make a difference taking this flag, 
                                             It will used at the time of copying data from non-shared to shared memoy. */
  char flag;                              /*Provide whether socket operation should be take care or not (Enable/Disable socket Operation)*/    
  int timeout_msec;                     /*Max timeout for socket operations - Open/Send/Recv*/
  int norm_id;                            /*It is norm id of Socket API open*/
  char* (*enc_dec_cb)(char*, int, int*);
  union
  {
    ProtoSocket_Open open;
    ProtoSocket_Send send;
    ProtoSocket_Recv recv;
  };
}ProtoSocket;

/***Start: Bug 79149**************************************************************************/
typedef struct
{
  StrEnt user;               /**/
  StrEnt password;               /**/
  StrEnt host;              /**/
  StrEnt domain;              /**/
} rdp_Conn;

typedef struct
{
  int x_pos;               /**/
  int y_pos;
  int x1_pos;               /**/
  int y1_pos;
  int button_type;
  int origin;
} ns_mouse;


typedef struct
{
  StrEnt key_value;               /**/
} ns_key;

typedef struct
{
  int timeout;               /**/
} ns_sync;



typedef struct
{
  char operation;                         /* This is common to all Socket APIs - Open+Send+Read so make a difference taking this flag, 
                                             It will used at the time of copying data from non-shared to shared memoy. */
  int norm_id;                            /*It is norm id of Socket API open*/
  union
  {
    rdp_Conn connect;
    ns_key   key; 
    ns_key   key_up; 
    ns_key   key_down; 
    ns_key   type; 
    ns_sync  sync; 
    ns_mouse mouse_down; 
    ns_mouse mouse_up; 
    ns_mouse mouse_click; 
    ns_mouse mouse_double_click; 
    ns_mouse mouse_move; 
    ns_mouse mouse_drag; 
 };
} rdp_request;
/***End: Bug 79149**************************************************************************/

typedef struct action_request {
  /* Following entries must be in order for all protocols */
  short request_type; // HTTP or HTTPS
  unsigned long hdr_flags;   /*For Header Info*/
  unsigned long flags;          /*For Feature Ingo*/

  union {
    u_ns_ptr_t svr_idx;  /* index into gServerTable http_request; if this is -1, then it means that we use the server of the last request */
    u_ns_ptr_t group_idx; /* index into the group table */
  } index;

  u_ns_ptr_t server_base; /* index into the Pointer Table; can be -1*/

  // achint - 02/01/07 - added for pre and post url callback
  // removing union of pre_url_fname and schedule_time as pre_url_fname is used 
  char pre_url_fname[31 + 1];
  u_ns_ts_t schedule_time;

  char post_url_fname[31 + 1];
  preurlfn_type pre_url_func_ptr;
  posturlfn_type post_url_func_ptr;
  int postcallback_rdepth_bitmask; // added for supporting redirection depth in post url callback

  union {
    http_request http;
    smtp_request smtp;
    pop3_request pop3;
    ftp_request  ftp;
    dns_request  dns;
    ldap_request ldap;
    imap_request imap;
    jrmi_request jrmi;
    ws_request ws; 
    xmpp_request xmpp;
    fc2_request fc2_req;
    ProtoSocket socket;
    rdp_request rdp; /*bug 79149*/
  } proto;

  char is_url_parameterized; // This flag will tell whether provided url is parameterized or not.
  struct action_request *parent_url_num;
} action_request;

#define NS_URL_PARAM_VAR                                   1
#define NS_URL_PARAM_VAL                                   2

#define HTTP_REQUEST                                       1
#define HTTPS_REQUEST                                      2
#ifdef RMI_MODE
#define RMI_REQUEST                                        3
#define JBOSS_CONNECT_REQUEST                              4
#define RMI_CONNECT_REQUEST                                5
#define PING_ACK_REQUEST                                   6
#endif
#define SMTP_REQUEST                                       7
#define POP3_REQUEST                                       8
#define FTP_REQUEST                                        9
#define FTP_DATA_REQUEST                                   10
#define DNS_REQUEST                                        11
#define XHTTP_REQUEST                                      12
#define CPE_RFC_REQUEST                                    13

#define USER_SOCKET_REQUEST                                14

#define LDAP_REQUEST                                       15
#define LDAPS_REQUEST                                      16

#define IMAP_REQUEST                                       17
#define IMAPS_REQUEST                                      18
#define SPOP3_REQUEST                                      19
#define JRMI_REQUEST                                       20
#define JNVM_JRMI_REQUEST                                  21
#define SMTPS_REQUEST                                      22
#define WS_REQUEST                                         23
#define WSS_REQUEST                                        24
#define XMPP_REQUEST                                       25
#define XMPPS_REQUEST                                      26
#define FC2_REQUEST                                        27
#define PARAMETERIZED_URL                                  28
#define SOCKET_REQUEST                                     29
#define SSL_SOCKET_REQUEST                                 30
#define RDP_REQUEST					31	/*bug 79149*/
//#define WS_FRAME_REQUEST 25


//#define topo_idx                   0 
#define REQUEST_TYPE_NOT_FOUND -1
#define RET_PARSE_OK 1
#define RET_PARSE_NOK 0

// For G_HTTP_HDR
extern int all_group_all_page_header_entries;

extern int topo_idx;
extern char g_proto_str[][0xff];
extern int g_proto_str_max_idx;
// Default ports --

#define HTTPS_IPV4_PORT 443
#define HTTP_IPV4_PORT  80
#define SMTP_IPV4_PORT  25
#define POP3_IPV4_PORT  110
#define FTP_IPV4_PORT   21
#define DNS_IPV4_PORT   53
#define IMAP_IPV4_PORT  143
#define IMAPS_IPV4_PORT 993

//IPV6
#define HTTPS_IPV6_PORT  6443
#define HTTP_IPV6_PORT   6880

struct http_charac {
  short type;
  short tx_ratio;
  short rx_ratio;
  short exact;
  short redirect;
  int bytes_to_recv;
  int first_mesg_len;
  short keep_conn_open;
  int num_embed;
#ifdef WS_MODE_OLD
  int wss_idx;
#endif
  // Achint - 02/01/07 - Added for Pre and Post URL Callback
  char pre_url_fname[31 + 1];
  char post_url_fname[31 + 1];
  int content_type;
  // Achint End
};

typedef struct HostElement {
  int first_url;
  short svr_idx;
  short num_url;
  int next; /* index into the host table */
} HostElement;

typedef struct PageTableEntry {
  u_ns_ptr_t page_name; /* offset of big buf */
  u_ns_ptr_t flow_name; /* offset of big buf */
  unsigned int page_id;  //Page_id is the running index of pages for all flow files
  unsigned int relative_page_idx; //This will be relative to script 
  unsigned short save_headers; // To keep the track that if this page has a searh parameter having its search from header 
  unsigned short num_eurls;
  u_ns_ptr_t first_eurl;  /* index into the requests table */
  //fd_set urlset;
  u_ns_ptr_t head_hlist; /* index into the host table */
  u_ns_ptr_t tail_hlist; /* index into the host table */
  u_ns_ptr_t think_prof_idx;           /* TODO:BHAV: to be removed */
#ifndef RMI_MODE
  u_ns_ptr_t tag_root_idx;
  int num_tag_entries;          /* number of tag entries */
#endif
  nextpagefn_type nextpage_func_ptr; //Function pointer to check_page_<pgname>()
  prepagefn_type prepage_func_ptr; //Function pointer to get_think_time_for_page_<pgname>()
  u_ns_ptr_t first_searchvar_idx;   /* index into the perpageservar table . Pointing to first entry for this page*/
  int num_searchvar; //Number of search vars pointing to this page
  u_ns_ptr_t first_jsonvar_idx;
  int num_jsonvar;
  u_ns_ptr_t first_checkpoint_idx;   /* index into the perpagechkpt table . Pointing to first entry for this page*/
  int num_checkpoint; //Number of checkpoints pointing to this page
  u_ns_ptr_t first_check_replysize_idx;   /* index into the perpagechkrepsize table . Pointing to first entry for this page*/
  int num_check_replysize; //Number of checkreplysize pointing to this page
  int tx_table_idx; // Index in the TxTable for the main transaction of this page. Valid only if page as trans is used
  int page_number;
  unsigned int redirection_depth_bitmask;
  int sp_grp_tbl_idx; //Index into SPGroupTable table - For sync point
  char flags; //currently this flag will be set incase of repeat and delay for inline url
  unsigned int page_norm_id;
  int page_num_relative_to_flow;
} PageTableEntry;

typedef struct TxTableEntry {
  u_ns_ptr_t tx_name; /* offset of big buf */
  int tx_hash_idx;
  int sp_grp_tbl_idx; //Index into SPGroupTable table - For sync point
} TxTableEntry;

//This is list of all NS vars. It is per session
typedef struct VarTransTableEntry_Shr {
  //short is_array;
  short var_type;
  /*Manish: Since in design (i.e. 4.1.6 variable table will not made hence for file parameter user_var_table_idx will be the relative index) */
  unsigned short user_var_table_idx; //Uniq variable index as opposed to perfect hash
			  //That may have some sparce entreis
			  //But it is really minimum uniq among tag_vars, serach and scratch vars.
			  //group vars, cluster_vars and static vars have their won name space.
  unsigned short var_idx;  //In case of search/Tag/NSL we will set table index(index of that perticular parameter table).
  unsigned short fparam_grp_idx; // point file parameter group index
  int retain_pre_value;
} VarTransTableEntry_Shr;

typedef struct SessTableEntry {
  u_ns_ptr_t sess_name; /* is the offset of the big buffer */
//  u_ns_ptr_t jmeter_sess_name; /* is the offset of the big buffer */
  ns_bigbuf_t jmeter_sess_name; /* is the offset of the big buffer */
  unsigned int sess_id; //sess_id is the running index for groups in scenario
  unsigned short num_pages;
  short completed;
  u_ns_ptr_t first_page; /* index into the group table */
  initpagefn_type init_func_ptr; //function pointer to init_<sess>()
  exitpagefn_type exit_func_ptr; //function pointer to exit_<sess>()
  //runlogicfn_type runlogic_func_ptr;  //
  void (*user_test_init)();  //function pointer to user_test_init from util.h 
  void (*user_test_exit)();  //function pointer to user_test_exit from util.h
  int tagpage_start_idx;
  int num_tagpage_entries;
  int tag_start_idx;
  int num_tag_entries;
  int nslvar_start_idx;
  int num_nslvar_entries;
  int var_start_idx;  //Static var- pointer into varTable
  int num_var_entries; //number of static variable
  int index_var_start_idx;
  int num_index_var_entries;
  int searchvar_start_idx;
  int unique_range_var_start_idx;
  int num_unique_range_var_entries;
  int jsonvar_start_idx; //Start idx of JSON vars
  int randomvar_start_idx; //Start idx of random vars 
  int randomstring_start_idx; //Start idx of random string
  int uniquevar_start_idx; 
  int datevar_start_idx;
  int num_searchvar_entries;
  int num_jsonvar_entries; //Total entries of JSON var in the table
  int num_randomvar_entries;//Total entries of random var in the table
  int num_randomstring_entries;
  int num_datevar_entries;
  int num_uniquevar_entries;
  int searchpagevar_start_idx;
  int num_searchpagevar_entries;
  int jsonpagevar_start_idx; //JSON Page start index
  int num_jsonpagevar_entries; //JSON Page numbers
  int checkpoint_start_idx;
  int num_checkpoint_entries;
  int checkpage_start_idx;
  int num_checkpage_entries;
  int num_dyn_entries; //Storing script wise transaction entries 
  int max_dyn_entries; //Max script wise tansaction entries
  int *dyn_norm_ids; //Storing script wise norm_ids of transaction

  int checkreplysize_start_idx;
  int num_checkreplysize_entries;
  int checkreplysizepage_start_idx;
  int num_checkreplysizepage_entries;

  int cookievar_start_idx;
  int num_cookievar_entries;
  VarTransTableEntry_Shr *vars_trans_table_shr_mem; //Indexed by NS vars hash code for this sess
  char* var_type_table_shr_mem;
  int* vars_rev_trans_table_shr_mem;
  int (*var_hash_func)(const char*, unsigned int);
  const char* (*var_get_key)(unsigned int);
  unsigned short numUniqVars; //number of searv+tag+nsl vars. they use uniq index
  int pacing_idx;
  ReqCookTab cookies; // -- Add Achint- For global cookie - 10/04/2007
  char *ctrlBlock;  // Anuj: for runlogic, 21/02
  int script_type; // Type of script - Legacy, C etc
  int sp_grp_tbl_idx; //Index into SPGroupTable table - For sync point
  int sess_flag;
  nslb_jsont *rbu_tti_prof_tree;       //Atul: For store json profile string
  //unsigned short num_ws_send;
  //u_ns_ptr_t ws_first_send; 
  void *handle; 
  unsigned int sess_norm_id;

  int flow_path;
  int num_of_flow_path;
  int flow_path_start_idx;
  char *rbu_alert_policy_ptr;
  u_ns_ptr_t proj_name;         /* is the offset of the big buffer for project name */
  u_ns_ptr_t sub_proj_name;     /* is the offset of the big buffer for sub-project name*/
  //char *host_table_entry[MAX_DATA_LENGTH]; /* stored all recorded host entry of script */
  sessHostTableEntry *host_table_entries;
  int total_sess_host_table_entries;
  int max_sess_host_table_entries;
  int netTest_page_executed;       //number of pages executed per script
  u_ns_ts_t netTest_start_time;    //netTest Script execution Time in ms.
  char save_url_body_head_resp;
  int flags;
} SessTableEntry;

/* Defines maximum number of characters on a line.*/
#define MAX_LINE_LENGTH 35000 // Changed for handel the url of big size
#define URL_BUFFER_SIZE MAX_LINE_LENGTH
// This is to hold one field parsed from a line. Earlier we were using MAX_LINE_LENGTH but it was increased to 32K
// So we added this new define as we are defining several local variable with this size and cause stack over flow
#define MAX_FIELD_LENGTH 10000

//extern int g_follow_redirects;
//extern int g_auto_redirect_use_parent_method;

typedef struct {
  char loc[MAX_LINE_LENGTH];
  void *next;
  void *prev;
} redirect_location;

//extern int num_urls;
extern int g_cmt_found;
#ifndef CAV_MAIN
extern action_request *requests;
extern int g_cur_server;
extern int g_max_num_embed;
extern SessTableEntry *gSessionTable;
extern PageTableEntry *gPageTable;
extern StrEnt* postTable;
extern ServerOrderTableEntry *serverOrderTable;
extern HostElement* hostTable;
#else
extern __thread action_request *requests;
extern __thread int g_cur_server;
extern __thread int g_max_num_embed;
extern __thread  SessTableEntry *gSessionTable;
extern __thread PageTableEntry *gPageTable;
extern __thread StrEnt* postTable;
extern __thread ServerOrderTableEntry *serverOrderTable;
extern __thread HostElement* hostTable;
#endif

#ifdef RMI_MODE
#define FIRST_NUM_STATE 1
#define SECOND_NUM_STATE 2
#define SPACE_STATE 3
#define MAX_NUM_ELEMENTS 4096
#endif

// URL Type
#define MAIN_URL 1
#define EMBEDDED_URL 2
#define REDIRECT_URL 3

// HTTP Method
#define HTTP_METHOD_GET    1
#define HTTP_METHOD_POST   2
#define HTTP_METHOD_HEAD   3
#define HTTP_METHOD_PUT    4
#define HTTP_METHOD_PATCH  5
#define HTTP_METHOD_CONNECT 6

/* defines default percentage if no 'PCT' key is in request */
#define DEF_PCT 0
#define ILLEGAL_PCT -1

/* defines number of allowable requests in the data file. */
#define MAX_REQUEST_SIZE 4096

/* defines maxumum number of characters in a port number */
#define MAX_PORT_SIZE 5

/* Defines maximum size of a request */
//#define MAX_REQUEST_BUF_LENGTH 16384
//For AMF
#define MAX_REQUEST_BUF_LENGTH 262144

#ifndef CAV_MAIN
#ifdef NS_DEBUG_ON
  #define NS_STRING_API "ns_string_api_debug.so"
#else
  #define NS_STRING_API "ns_string_api.so"
#endif
#else
#ifdef NS_DEBUG_ON
  #define NS_STRING_API "cg_string_api_debug.so"
#else
  #define NS_STRING_API "cg_string_api.so"
#endif
#endif

/* Parses HTTP requests from file and returns number of requests read. */
extern int parse_files(void);
extern void process_additional_header();
extern short get_server_idx(char *hostname, int request_type, int line_num);

extern void check_end_hdr_line(char* buf, int max_length);

extern int parse_url(char *url, char *host_end_markers, int *request_type, char *hostname, char *path);
extern int parse_url_param(char *url, char *host_end_markers, int *request_type, char *hostname, char *path);

#ifndef CAV_MAIN
extern int max_var_table_idx;
#else
extern __thread int max_var_table_idx;
#endif
extern const char* (*get_key_parse)(unsigned int);

extern int max_dynvar_hash_code;
extern unsigned int (*dynvar_hash)(const char*, unsigned int);
extern int (*in_dynvar_hash)(const char*, unsigned int);

extern const char* (*bytevar_get_key)(int);

#ifdef RMI_MODE
extern int max_bytevar_hash_code;
extern unsigned int (*bytevar_hash)(const char*, unsigned int);
extern int (*in_bytevar_hash)(const char*, unsigned int);

extern inline int hex_to_int(char digit);
#endif

#ifdef WS_MODE_OLD
extern int init_webspec_tables(void);
extern int parse_web_serv_spec(FILE* fptr);
extern int find_webspec_table_entry(char*);
#endif

//extern int create_tagtables();

extern int pg_error_code_start_idx;
extern int tx_error_code_start_idx;
extern int sess_error_code_start_idx;

extern void call_user_test_init_for_each_sess(void);
extern void call_user_test_exit_for_each_sess(void);
extern void free_runprof_page_think_idx();

extern void init_post_buf();

extern int copy_to_post_buf(char *buf, int blen);
#ifndef CAV_MAIN
extern char *g_post_buf; // Needed in other protocol files also so keeping in url.h
extern int g_cur_page;
#else
extern __thread char *g_post_buf; // Needed in other protocol files also so keeping in url.h
extern __thread int g_cur_page;
#endif
extern int script_type; // Do we need this ?

extern int
parse_headers(char *line_ptr, char *header_buf, int line_number, char *fname, int *header_bits, int url_idx);
extern void read_script_libs(char *sess_name, char *script_libs_flag, int sess_idx);
// These methods are putted here due to c type script and called from ns_http_script_parse.c
extern redirect_location *search_redirect_location(char *url);
extern void delete_red_location(redirect_location *r);
extern int add_redirect_location(int redirect, char *location);
extern void free_leftover_red_location();
extern void arrange_pages(int sess_idx);
extern int val_fname(char *fname, int max_len);
extern int ns_rte_on_test_start();
extern int get_no_inlined();
extern void ns_parse_decrypt_api(char *input);
extern void free_runprof_add_header_table();
#endif /* URL_H */
