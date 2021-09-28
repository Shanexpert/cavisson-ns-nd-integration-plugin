#ifndef NETSTORM_H
#define NETSTORM_H

#include <ucontext.h>  // For Context Switching of NetStorm AN-CTX

#include <libxml/xmlmemory.h>
#include <libxml/HTMLparser.h>
#include <libxml/HTMLtree.h>
#include <libxml/debugXML.h>
#include <libxml/xmlerror.h>
#include <libxml/globals.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <sql.h>
#include <sqlext.h>
#include <gsl/gsl_randist.h>
#include <openssl/ssl.h>
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_http_cache_table.h"
#include "ns_data_types.h"
#include "ns_proxy_server_reporting.h"
#include "ns_rbu_api.h"
#include "timing.h"
#include "tmr.h"
#include "ns_schedule_phases.h"
#include "logging.h"
#include "ns_h2_req.h"
#include "ns_soap_api.h"
#include "ns_exit.h"
//#include "ns_mongodb_api.h"
#include "ns_jmeter_api.h"
//#include "ns_xmpp.h"
#include "ns_fc2.h"
#include "ns_tls_utils.h"
#include "ns_iovec.h"

//#define START_NONE 0
//#define START_PARALLEL 1
//#define START_RATE 2
#define USER_CHUNK_SIZE 64

/* different phases of a run */
#define NS_RUN_PHASE_RAMP 0
//#define NS_RUN_PHASE_SESS_AWAIT 1
//#define NS_RUN_PHASE_OBSERVE 3
//#define NS_RUN_PHASE_STABILIZE 4
#define NS_RUN_WARMUP 1
#define NS_RUN_WAIT_TO_SYNC 2
#define NS_RUN_PHASE_EXECUTE 3
#define NS_RUN_PHASE_RAMP_DOWN 4
//#define NS_RUN_PHASE_RAMP_DOWN_CONTROLLED 5
#define NS_ALL_PHASE_OVER 6

#define COPY_BUFFER_LENGTH 1022

/* Vuser states:*/
#define NS_VUSER_IDLE 0
#define NS_VUSER_ACTIVE 1
#define NS_VUSER_THINKING 2
#define NS_VUSER_CLEANUP 3
#define NS_VUSER_SESSION_THINK 4
#define NS_VUSER_SYNCPOINT_WAITING 5
#define NS_VUSER_BLOCKED 6
#define NS_VUSER_PAUSED 7

#define NS_EXCLUDE_STOPPED_STATS_FROM_PAGE_TX_USEONCE ((vptr->page_status == NS_USEONCE_ABORT ) && \
                                                       (global_settings->exclude_stopped_stats == 2))
#define NS_EXCLUDE_STOPPED_STATS_FROM_PAGE_TX ((vptr->page_status == NS_REQUEST_STOPPED ) && (global_settings->exclude_stopped_stats == 1))
#define NS_EXCLUDE_STOPPED_STATS_FROM_URL ((cptr->req_ok == NS_REQUEST_STOPPED) && (global_settings->exclude_stopped_stats == 1))
#define TX_NAME_MAX_LEN 1024
extern char g_test_user_name[];
extern char g_rtc_owner[64];
extern char g_test_user_role[];

extern short gRunPhase;
extern short g_ramp_down_completed;
extern int g_ns_instance_id;
extern int MaxStaticUrlIds;
extern int is_goal_based;

/*
 * this structure is used for forced server change on redirection. the new
 * recorded server to which redirection happens, is saved at a predefined depth
 * (given by the user through an api call - ns_setup_save_url), into a user
 * variable. The new rec server is saved upon redirection, at the requested
 * depth and changed when the api  ns_force_server_mapping is
 * called - the new mapping stays valid until another call to this api is made
 * or until the session ends. these api's are called from pre_page
 */

typedef struct { 
  u_char flag;      // if set, save redirection URL/hostname upon redirection
  u_char change_on_depth; // save hostname at this redirection depth
  u_short port;     // port # in the redirection URL
  u_short type;     // type to specify how the URL/hostname should be saved
  char *var_name;   // variable name given by the user to save hostname
} ServerMapChange;


struct copy_buffer {
  char buffer[COPY_BUFFER_LENGTH];
  struct copy_buffer* next;
};

typedef struct {
  pid_t pid;
  //Currently port selection is left to OS
  unsigned short min_port;
  unsigned short max_port;

  unsigned short min_listen_port;  // Port For Listening TR060
  unsigned short max_listen_port;  // Port For Listening TR069

  int msg;
  int num_vusers;               /* We still keep this to assist calculations */
  double vuser_rpm;
  int num_fetches;
  char* env_buf;
  shr_logging* logging_mem;
  struct sockaddr child_addr;
  int child_addr_len;
  //int warmup_sessions;
  //int ramping_done;
  struct sockaddr_in sockname; // Socket address of parent to NVM connection
  //double iid_mu; /* inter arrival time of user generation rate in centi-seconds*/
  //int ramping_delta;  
  /* num user has following values:
   * 0 : Ramp Linear, Ultimate Conns constant, x users/sec steps, use RAMPUP_TIME
   * 1 : Ramp Linear, Ultimate Conns constant, x users/sec steps, auto calc RAMPUP_TIME
   * 2 : Ramp Linear, Ultimate Conns constant, constant IID, useRAMPUP_TIME
   * 3 : Ramp Linear, Ultimate Conns constant, constant IID, auto calc RAMPUP_TIME
   * 4 : Ramp poisson, Ultimate Conns constant, Poisson IID, use RAMPUP_TIME
   * 5 : Ramp poisson, Ultimate Conns constant, Poisson IID, auto calc RAMPUP_TIME
   * 6 : Ramp poisson, Ultimate Conns avg poisson, Poisson IID, auto calc RAMPUP_TIME
   */
  //for Concurrent session  
  int limit_per_nvm;
  Schedule *scenario_schedule;
  Schedule *group_schedule;
  Schedule *runtime_schedule;
} s_child_ports;

#ifndef CAV_MAIN
extern s_child_ports* v_port_table;
extern s_child_ports* v_port_entry;
#else
extern __thread s_child_ports* v_port_table;
extern __thread s_child_ports* v_port_entry;
#endif
extern int ultimate_max_connections;

extern void* my_malloc(size_t size);

typedef struct {
  int chunk_sz;
  void *next;
} chunk_size_node;

/* example:
 * 1 1920
 * 2 4353
 */
typedef struct POP3_scan_listing {
  char maildrop_idx[9];
  int  octets;
} POP3_scan_listing;

/* This structure contains the pipeline doubly linked list */
typedef struct pipeline_page_list {
  action_request_Shr *url_num;
  struct pipeline_page_list *next;
} pipeline_page_list;

typedef struct cptr_data_t{

  void *cache_data; 

  /* buffer  - Used for the partial header read in network cache stats 
   * len     - Contains the length of malloced buffer 
   * use_len - contains content length used in buffer
   * Note: This is currently used during response processing, however, can be used for any other purposes as well. 
   * buffer will not get freed after a header is parsed. It will be kept malloced for next use. Only use_len will be set 0
   * This gets freed after compelte processing of the url, once the cptr is released. 
   */
  char *buffer; 
  int len;
  int use_len;

  unsigned int nw_cache_state;
} cptr_data_t;

typedef struct x_dynaTrace_data_t{

  char *buffer; 
  int len;
  int use_len;
} x_dynaTrace_data_t;

/*cptr flags bit masks*/
#define NS_HTTP_EXPECT_100_CONTINUE_HDR       0x0000000000000001  //bit for Expect Continue state 
#define NS_CPTR_FLAGS_FREE_URL		      0x0000000000000002  //bit to free cptr->url 
//To check if http response is AMF or not
//Set it using Content-Type: application/x-amf header
//Then set it to vptr->flag for use in do data processing
#define NS_CPTR_FLAGS_AMF		      0x0000000000000004  //Content-Type is AMF
#define NS_CPTR_FREE                          0x0000000000000008  //Not used.
#define NS_CPTR_RESP_FRM_AUTH_BASIC           0x0000000000000010  
#define NS_CPTR_RESP_FRM_AUTH_DIGEST          0x0000000000000020  
#define NS_CPTR_RESP_FRM_AUTH_NTLM            0x0000000000000040  
#define NS_CPTR_RESP_FRM_AUTH_KERBEROS        0x0000000000000080 
#define NS_CPTR_AUTH_MASK                     0x00000000000000F0  //bits for all auth types set
#define NS_CPTR_AUTH_TYPE_FIXED               0x0000000000000100  //auth type to use is decided
#define NS_CPTR_FLAGS_CON_USING_PROXY         0x0000000000000200  //Set for making connection to proxy
#define NS_CPTR_AUTH_HDR_RCVD                 0x0000000000000400  //Set When Authentication header is rcvd
//TODO: Need to check if this should be reset on every request complition or only in Close_connection
/*Proxy Authorization: Used to distinguish proxy authentication from server authentication*/
#define NS_CPTR_FLAGS_CON_PROXY_AUTH          0x0000000000000800  
#define NS_CPTR_FLAGS_HESSIAN		      0x0000000000001000  //Content-Type is hessian 

// Following bits are reset after http close connection
#define NS_CPTR_RESP_FRM_CACHING              0x0000000100000000  //SSL was resued on this connection
#define NS_CPTR_FLAGS_SSL_REUSED              0x0000000200000000  //SSL was resued on this connection
#define NS_CPTR_FLAGS_CON_USING_PROXY_CONNECT 0x0000000400000000  //Set for tracking connect state of proxy
#define NS_CPTR_FLAGS_SAVE_HTTP_RESP_BODY     0x0000000800000000  //Set if response body is to be saved into file based on                      
                                                                  //Header - Content-Type: application/octet-stream 
#define NS_CPTR_FLAGS_DNS_DONE                0x0000001000000000  //Set if dns_lookup done in nonblocking mode                      
#define NS_CPTR_FLAGS_JAVA_OBJ                0x0000002000000000  //Set if dns_lookup done in nonblocking mode                      
#define NS_CPTR_JRMI_REG_CON                  0x0000004000000000  //Set if protocol is jrmi and connection is registry type 
#define NS_CPTR_FINAL_RESP                    0x0000008000000000  //This is to tell if this is final resp
#define NS_CPTR_DO_NOT_CLOSE_WS_CONN          0x0000010000000000  //This is to tell if this is final resp
// To check if http is upgraded to http2 
#define NS_HTTP2_UPGRADE_DONE                 0x0000020000000000   //This flag is to tell that http is upgraded to http2
#define NS_HTTP2_SETTINGS_ALREADY_RECV        0x0000040000000000   //Used in case we received http2 settings with 101 response from server   
#define NS_CPTR_CONTENT_TYPE_MPEGURL          0x0000080000000000   //Set if we want to process m3u8 mpeg response
#define NS_CPTR_CONTENT_TYPE_MEDIA            0x0000100000000000   //set if we want to log media type response in seprate file
#define NS_CPTR_HTTP2_PUSH                    0x0000200000000000   //Flag for current request is pushed or not
#define NS_CPTR_CONTENT_TYPE_PROTOBUF         0x0000400000000000   //set if content type is protobuf 
#define NS_WEBSOCKET_UPGRADE_DONE             0x0000800000000000   //This flag is to tell that http is upgraded to websocket

#define NS_CPTR_FLAGS_RESET_ON_CLOSE_MASK     0x00000000FFFFFFFF   //Bits 0 to 31 are not reset

/*Note: Whenever u add a fields to cptr initialize it in allocate_user_tables*/
typedef struct {

  // This MUST be first field of this strcuture
  char con_type;  // Connection type

  /* Proxy Chain Auth ---BEGIN*/
  char proxy_auth_proto; //Storing proxy protocol in case of proxy-auth-chain
  //Required to implement proxy-chain to store proxy digest msg
  char connection_type; // KA or Non KA
  char compression_type;
  action_request_Shr *url_num;
  //http_request_Shr *redirect_url_num; /* Stores URL for auto redirect */
  char *location_url; /* stores redirected-to location */
  char *link_hdr_val; // stores link header val

  /* char *url will have the complete url (No method No version Only URL)*/
  char *url;
  unsigned short url_len; /* Length to URL null terminated */ 

  short redirect_count; /* Stores count of how many redirects followed so far. */
  union {
    short mail_sent_count;
    short duration;
  };
  void* vptr;
  struct sockaddr_in6 sin;
  int conn_fd;
  int conn_state, header_state, chunked_state, proto_state;
  ns_ptr_t cookie_hash_code; // This is hash code of cookie or Head of the cookie linked list in Auto Cookie Mode
  short cookie_idx;
  short gServerTable_idx;
  u_ns_ts_t started_at; /* initial time */
  /*component start time stamp.This will get updated on every component start */
  u_ns_ts_t ns_component_start_time_stamp; 
  int dns_lookup_time; /*Total time taken (miliseconds) in DNS lookup*/
  int connect_time; /* connect time */
  int ssl_handshake_time; /* SSL Handshake time*/
  int write_complete_time;  //Time taken (miliseconds) in write the request
  int first_byte_rcv_time;  //Time taken (miliseconds) to recv first byte of the response
  int request_complete_time; /*Request complete time (miliseconds) is the download time*/
  u_ns_ts_t con_init_time;
  union {
    u_ns_ts_t request_sent_timestamp;       //req sent time, used for response timeout 
    u_ns_ts_t end_time;
  };

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
  int uid;
  /*  int user_index; */
  int num_ka;
  //int not_ready;
  int file;
  int class;
  union {
    int dir;
    int body_index;  // It will have the body start vector index in all vectors (Used for HTTP : 100 Continue)
  };
  struct sockaddr_in6 cur_server; /* Address of current server (Origin Server)*/
  /*Proxy*/
  struct sockaddr_in6 conn_server;  // Address of origin or proxy server where connection is made. 

  struct connection* next_svr; //keep track unused of link list for reuse connections per server
  struct connection* prev_svr;

  union {
    struct connection* next_reuse; // keep track of unused link list of resue connections for the Vuser
    struct connection* next_inuse; // keep track of in use link list of inuse connection for the Vuser
    struct connection* next_free;  // keep track of free link list of free connection in connection pool
  };

  union {
    struct connection* prev_reuse; 
    struct connection* prev_inuse; 
  };

  int list_member; // keeps track of the lists on which this connection slot is
  htmlParserCtxtPtr ctxt;
#ifdef ENABLE_SSL
  //int num_ssl_reuse;
  // int cur_ssl_reuse;
  //SSL_CTX *ctx;
  SSL     *ssl;
  //SSL_SESSION     *sess;
#endif
  timer_type* timer_ptr;
  struct connection* next_in_list;  /* this pointer is to point to the next connection chunk (only the first connection struct in the chunk will have a "valid next_in_list pointer") */
  int chunk_size;  /* this is the number of connections in the chunk (only the first connection struct in the chunk will have a valid "chunk_size") */
  //#ifdef RMI_MODE
  PerHostSvrTableEntry_Shr* old_svr_entry;
  //#endif
  struct copy_buffer* buf_head;
  struct copy_buffer* cur_buf;
  //Following are added for  large request writes
  char * free_array; //free_array is also ovderloaded used for keeping alloacted send pointer for HTTPS
  struct iovec * send_vector;
  int bytes_left_to_send;
  short num_send_vectors;
  short first_vector_offset;
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

  /* pop3 scan listing for particular session */
  POP3_scan_listing *pop3_scan_list_head;
  int total_pop3_scan_list;
  int max_pop3_scan_list;
  int cur_pop3_in_scan_list;
  void *conn_link;  /* Type connection keeps link to control con */

  //  connection *second_con;
  /* DNS and pipelining */
  void *data; // XXXX
  cptr_data_t *cptr_data; // XXXX
  // making union with num_pipe as pipelining is now deprecated
  union {
    /* pipelining related */
    int num_pipe;   /* Values can be -1,0,1,2,.. (-1 to indicates this connection
                   * can not pipeline (example POST type). Zero is default value */
    int free_array_size; // Used in case of RAL for storing cptr->free_array size as cptr->free_array has the request to be send
  };

  long long int flags;   

  /* Net Diagnostics ---BEGIN*/
  ns_8B_t nd_fp_instance;
  /* Net diagnostics ---END*/

  int  prev_digest_len;  
  unsigned char *prev_digest_msg;  
  /* Proxy Chain Auth ---END*/
  //X-Dynatrace header data ptr
  x_dynaTrace_data_t *x_dynaTrace_data;
  char ws_reading_state;  //used for parsing reading frame state
  int http_protocol;
  union{
    char http2_state;
    char fc2_state;
  };
  ProtoHTTP2 *http2;
  int ssl_cert_id;
  int ssl_key_id;

  int (*proc_recv_data)(void *, char *, int, int *, int);  // This callback function will process received data
} connection;

//cptr->completion_code
#define NS_COMPLETION_NOT_DONE                            0
#define NS_COMPLETION_TIMEOUT                             1
#define NS_COMPLETION_CLOSE                               2
#define NS_COMPLETION_CONTENT_LENGTH                      3
#define NS_COMPLETION_CHUNKED                             4
#define NS_COMPLETION_BAD_BYTES                           5
#define NS_COMPLETION_ZERO_HDR                            6
#define NS_COMPLETION_EXACT                               7
#define NS_COMPLETION_BAD_READ                            7  // Please Check this Arun Nishad
#define NS_COMPLETION_RELOAD                              9
#define NS_COMPLETION_CLICKAWAY                           10
#define NS_COMPLETION_SMTP_ERROR                          11
#define NS_COMPLETION_POP3_ERROR                          12
#define NS_COMPLETION_FTP_ERROR                           13
#define NS_COMPLETION_FRESH_RESPONSE_FROM_CACHE           14
#define NS_COMPLETION_IMAP_ERROR                          15
#define NS_COMPLETION_USEONCE_ERROR                       16
#define NS_COMPLETION_UNIQUE_RANGE_ERROR                  17
#define NS_COMPLETION_WS_ERROR                            18
#define NS_COMPLETION_BIND_ERROR                          19

typedef struct {
	short num_parallel;
        short http_mode;
	short hurl_left;
	struct HostSvrEntry* prev_hlist;
	struct HostSvrEntry* next_hlist;
	action_request_Shr* cur_url;
	action_request_Shr* cur_url_head;
	connection* svr_con_head;
	connection* svr_con_tail;
#ifdef ENABLE_SSL
  //SSL_CTX *ctx;
  //SSL     *ssl;
  //char  ssl_mode;
  SSL_SESSION     *sess;
#endif
} HostSvrEntry;

/* vptr->flags bitmasks */
//#define NS_ACTION_ON_FAIL_STOP 0x00000004
////Group 1 //These bits dont reset on new page
#define NS_COOKIES_DISALLOWED                      0x0000000100000000 
#define NS_VUSER_RAMPING_DOWN                      0x0000000200000000 //bit to mark vuser ramp down 
#define NS_EMBD_OBJS_SET_BY_API                    0x0000000400000000 
#define NS_CACHING_ON                              0x0000000800000000 //User if flaged for caching
#define NS_DO_CLEANUP                              0x0000001000000000 //This bit is for checking if cleanup is to be done or not
#define NS_VUSER_TRACE_ENABLE                      0x0000002000000000 //This bit is for checking if debug trace is enable or not for user

//Bug#2426
//This bit is for checking if the user is a new user or same user is continuing over the session. If same user is continuing over the session NS_REUSE_USER will be set to 1.

//* In start_reuse_user() we set vptr->operation = VUT_REUSE_USER but immediately we overwrite this by VUT_WEB_URL.
//start_reuse_user ()->on_new_session_start()->ns_web_url_ext()->vut_add_task(vptr, VUT_WEB_URL);

//* In reuse_user() if we have session pacing than we add cptr to reuse list using add_to_reuse_list & later on through nsi_web_url we call execute_next page & use cptr from vptr->last_cptr while we should use cptr from that reuse list.
#define NS_REUSE_USER                              0x0000004000000000  
#define NS_PAGE_DUMP_ENABLE                        0x0000008000000000 //This bit is for checking if page dump is enable or not for user
#define NS_PAGE_DUMP_CAN_DUMP                      0x0000010000000000 //Used for checking if page dump can dump, will be set if page, tx fails
#define NS_ND_ENABLE                               0x0000020000000000 //This bit is for checking if ND is enabled
#define NS_SOAP_WSSE_ENABLE                        0x0000040000000000 //This bit is for checking if Soap WS Security is enabled
#define DO_NOT_INCLU_SPT_IN_TIME_GRAPH             0x0000080000000000 //Used for SHOW_GROUP_DATA skip in case of user_first_sess is there.
#define NS_XMPP_ENABLE		                   0x0000100000000000 //This bit is for checking if XMPP is enabled 
#define NS_FC2_ENABLE                              0x0000200000000000  //This bit is for checking if FC2 is enabled 
#define NS_VUSER_PAUSE	                           0x0000400000000000  
#define NS_VUSER_GRADUAL_EXITING                   0x0000800000000000  
#define NS_VPTR_FLAGS_DDR_ENABLE                   0x0001000000000000 // DDR enabled for the session
#define NS_VPTR_FLAGS_PAGE_DUMP_DONE               0x0002000000000000 // Page dump done for at least one page of the session
#define NS_VPTR_FLAGS_HTTP_USED                    0x0004000000000000 // HTTP used in any page of a session
#define NS_VPTR_FLAGS_SESSION_COMPLETE             0x0008000000000000 // This flag is use to mark Session Complete for a vuser
#define NS_VPTR_FLAGS_SESSION_EXIT                 0x0010000000000000 // This flag is use to mark Session Exit

//vptr flags - Group 2 These bits need to reset on every new page start
#define NS_ACTION_ON_FAIL_CONTINUE                 0x0000000000000001  
#define NS_RESP_FRM_CACHING                        0x0000000000000002 //Response of current url is from cache(Main or embedded if POST_CB used)
#define NS_RESP_AMF                                0x0000000000000004 //Response is amf. Set from cptr->flags
#define NS_RESP_HESSIAN                            0x0000000000000008 //Response is hessian. Set from cptr->flags
//TODO: comment
#define NS_RESP_NETCACHE                           0x0000000000000010 //Response is from NetCache. Set from cptr->flags

//In case of rbu, we will replace sleep with page think time.
#define NS_USER_PTT_AS_SLEEP                       0x0000000000000020   

#define NS_RESP_JAVA_OBJ                           0x0000000000000040 //Response is of java type obj. Set from cptr->flags
#define NS_JNVM_JRMI_RESP                          0x0000000000000080  
#define NS_BOTH_AUTO_AND_API_EMBD_OBJS_NEED_TO_GO  0x0000000000000100 

#define NS_VPTR_FLAGS_CON_USING_PROXY_CONNECT      0x0000000000000200 //Set for tracking connect state for reporting
#define NS_VPTR_FLAGS_INLINE_REPEAT                0x0000000000000400 //This bit is for checking inline repeat for current page
#define NS_VPTR_FLAGS_INLINE_DELAY                 0x0000000000000800 //This bit is for checking inline delay for current group
#define NS_VPTR_FLAGS_INLINE_REPEAT_DELAY_MASK     0x0000000000000C00 //For checking delay and repeat
#define NS_VPTR_FLAGS_SEND_REFERER                 0x0000000000001000 //For keeping a check on sending referer 
#define NS_VPTR_FLAGS_USER_CTX                     0x0000000000002000 //For keeping a check on vptr context
#define NS_VPTR_FLAGS_SP_WAITING                   0x0000000000004000 //For keeping a check on vptr sp waiting

#define NS_VPTR_FLAGS_TIMER_PENDING                0x0000000000008000 //Stop the callback when User is Paused
#define NS_RESP_PROTOBUF                           0x0000000000010000 //Stop the callback when User is Paused

#define RESET_GRP_FIRST_32_BITS                    0xFFFFFFFF00000000  

//As SSL buf does not this, it may be 1 to 4K now.
#define SEND_BUFFER_SIZE 32*1024
#define NS_USER_MULTIPLE 1

// IOV_MAX is used to check writev limit
#ifndef IOV_MAX
#define IOV_MAX 1024
#endif

// IOVECTOR_SIZE is changes 256 50 1024 as we need more segments for repeat block
#define IOVECTOR_SIZE 1024

#define CHK_NO_CHUNKS 0

// Connection States 
// Connection state order should not be touched, as CNST_MIN_CONFAIL is used with both greater than and less than sign
#define CNST_FREE                               0
#define CNST_REUSE_CON                          1
#define CNST_CONNECTING                         2
#define CNST_SSLCONNECTING                      3
#define CNST_LISTENING                          4
#define CNST_PAUSING                            5
#define CNST_WRITING                            6
#define CNST_SSL_WRITING                        7
#define CNST_HEADERS                            8
#define CNST_READING                            9
#define CNST_NOT_EXIST                          10
#define CNST_CHUNKED_READING                    11
#define CNST_SSL_READING_WRITING                12
#define CNST_REQ_READING                        13
#define CNST_READY_TO_ACCEPT                    14
#define CNST_ACCEPTED                           15
#define CNST_CONNECTED                          16
#define CNST_TIMEDOUT                           17
#define CNST_CONNECT_FAIL                       18
#define CNST_LDAP_WRITING                       19
#define CNST_JRMI_WRITING                       20
#define CNST_WS_READING                         21
  /* Description: CNST_WS_READING -> Websocket connection is in body frame reading state */ 
#define CNST_WS_FRAME_READING                   22
  /* Description: CNST_WS_FRAME_READING -> Websocket connection is in message response frame reading state */ 
#define CNST_WS_FRAME_WRITING                   23
  /* Description: CNST_WS_FRAME_WRITING -> Websocket connection is in writing state */ 
#define CNST_HTTP2_WRITING                      24
#define CNST_MIN_CONFAIL                        CNST_PAUSING
  /* Description: CNST_MIN_CONFAIL -> tells the minimum con state for considering it a connection failure */
#define CNST_WS_IDLE                            25
#define CNST_FC2_WRITING                        26
#define CNST_HTTP2_READING                      27 /*bug 51330: added for read event for WINDOW_UPDATE*/
#define CNST_IDLE                               28 

// RMI related connection states are defined in netstorm_rmi.h
#include "ns_fdset.h"

//#define NS_EPOLL_MAXFD 4096
#define NS_EPOLL_MAXFD (1024*32)
#define KA 1
#define NKA 2

#define NS_ON_SVR_REUSE_LIST 0x1
#define NS_ON_GLB_REUSE_LIST 0x2
#define NS_ON_FREE_LIST 0x4
#define NS_ON_INUSE_LIST 0x8

#define IN_CLASS_ZERO(x)  (x < 35)
#define IN_CLASS_ONE(x)  ((x > 34) && (x < 85))
#define IN_CLASS_TWO(x)  ((x > 84) && (x < 99))
#define IN_CLASS_THREE(x)  (x > 98)

NSFDSet ns_fdset;

/**
 * The following structure is a tmporary structure used to keep track of malloc'ed
 * values for embedded objects in case of auto fetch embedded.
 */
typedef struct { 
  action_request_Shr *http_req_ptr;
  short num_url;
} http_request_Shr_free;

typedef struct ntlm_s
{
  char *partial_hdr;
  int  hdr_len;
  char *challenge;
} ntlm_t;

typedef struct
{
  void *env;
  void *dbc;
  //SQLHSTMT stmt;
} db_t;

//User agent string set by user using NS API
typedef struct 
{
  unsigned short malloced_len;//Malloced length
  unsigned short ua_len; //user agent string length
  char *ua_string; //User agent string
} UA_handler;

// Used to save parameter value pointer for page dump and user trace
typedef struct
{
  int total_entries;
  int max_entries;
  void *used_param; // Taking void pointer to avoid including header file 
}Used_Param_t;

//It used for custom header API like: ns_web_add_header()
typedef struct User_header
{
  char *hdr_buff;   //contain the header data which is passed by ns_web_add_header() API.
  int malloced_len; //Used for malloc the structure.
  int used_len;     //buffer which is filled for main, embedded and both URL for ns_web_add_header() API and reset it after page completion. 
  int flag;         //set auto header bits for main, embedded and all URL.
  struct User_header *next;  
} User_header;

// Used in vptr
typedef struct HTTPData_s
{
  union {
    // Max number of cache entries for this user. Used for cache table size mode 1
    unsigned int max_cache_entries; 
    unsigned int flags; // Added for TR069. Can be used for others if caching is not applicable
  };
  unsigned int cache_table_size; // Size of cache table. Set based on the mode of cache table size keyword
  union {
    //Pointer to Array of Pointers to Cache Table
    CacheTable_t  **cacheTable;
    char *reboot_cmd_key;     //For TR069 protocol
  };
  union {
    /* This used for saving main page url as we may need in JS case to find parent url path 
     * if embd url is not full qualified*/
    char *page_main_url;
    char *download_cmd_key;   //For TR069 protocol
  };
  int   page_main_url_len;

  //Pointer to Array of Pointers to Master Cache Table
  CacheTable_t  **master_cacheTable;
  unsigned int master_cache_table_size;
  
  // Pointer to Cache Entry for url
  // Not used - CacheTable_t  *cacheEntry;

  /* Begin: Added for Java Script Click and Script */
  void *ptr_html_doc; // Pointe to DOM; For click and script feature
  void *js_context;
  void *global;
  int clickaction_id;
  char *clicked_url;
  int clicked_url_len;
  unsigned short server_port;
  char *server_hostname;
  int server_hostname_len;
  unsigned char request_type;
  int http_method;
  char *formencoding;
  int formenc_len;
  char *post_body;
  int post_body_len;
  void *js_rt;
  /* End: Added for Java Script Click and Script */
  ntlm_t ntlm;
  db_t db;
  //Used to store proxy connect response time for reporting in case proxy is enabled
  Proxy_con_resp_time *proxy_con_resp_time;
  /* Since in case of RBU User Agent featur is not used so make it union*/

 // This union creating core dump so commenting here
 /* union
  {
    UA_handler *ua_handler_ptr; //Added to save user agent string through API
    //RBU Response data
    RBU_RespAttr *rbu_resp_attr;
  };*/

  UA_handler *ua_handler_ptr; //Added to save user agent string through API
  RBU_RespAttr *rbu_resp_attr;

  /*handle used param table for user trace and page dump */
  Used_Param_t up_t; //used_param table info
  User_header *usr_hdr; //used for ns_web_add_hdr() API

  /* WebSocket session variable */
  int ws_uri_last_part_len;
  char *ws_uri_last_part;  //This is resource part of URI Eg: http://10.10.70.4/tours/index.html - /tours/index.html 
  char *ws_client_base64_encoded_key;
  char *ws_expected_srever_base64_encoded_key;

  /* MongoDB */
  short check_pt_fail_start;//Is used to store the index of first checkpoint have CVfail.
  //mongodb_t *mongodb;
  void *mongodb;

  /* JMeter */
  jmeter_attr_t *jmeter;

   /* Cassandra DB*/
  void *cassdb;

  int ssl_cert_id;
  int ssl_key_id;
} HTTPData_t;


// Data for user context. It is  used if script mode is to run in user context
typedef struct
{
//  ucontext_t nvm_ctx;    // NVM (Main) context
  ucontext_t ctx;        // User context
  int        stack_size; // Size of the stack allocted for user in bytes
  char       *stack;     // Allocated stack for user
} VUserCtx;


struct Msg_com_con;

//Added in 3.9.2 for DNS Caching 
typedef struct{
  struct sockaddr_in6 resolve_saddr; 
}UserServerResolveEntry;

typedef struct {
  int hdr_buf_len;
  int used_hdr_buf_len;
  char *hdr_buffer;
} ResponseHdr;

typedef struct 
{
  action_request_Shr* first_page_url;
  int uid_size;
  char *uid;
  int file_url_size;
  char *file_url;
  int partial_buf_size;
  int partial_buf_len;
  char *partial_buf;
  int last_action; 
}nsXmppInfo;

typedef struct {
  unsigned char *partial_read_buff; // This buffer will be used to manage partial frames read
  int partial_buff_max_size; // Aloocated size
  int partial_buff_size; // Used size
}FC2;

// This is allocated for user running as thread to keep data to make it thread safe
typedef struct {
 union {
   char *eval_buf; // Used for eval string and can be used for other in future
   char *tx_name;  // Transaction name (start and end tx api) with null termination
 };
 union {
   int eval_buf_size;
   int tx_name_size;  // size of the tx_name buffer
 };
 union {
   int page_id;  // Page id of ns_web_url() API
   int status;  // tx status 
   double page_think_time; // page think in seconds (decimal for ...)
 };
 char *end_as_tx_name;
 int end_as_tx_name_size;
} VUserThdData;

typedef struct SMMonSessionInfo SMMonSessionInfo;

typedef struct VUser {
  // Following are initiliased athe begining of a new page
  PageTableEntry_Shr* cur_page;
  PageTableEntry_Shr* next_page;
  //int cur_tx;
  action_request_Shr* first_page_url;
  //Abhishek
  //update_proto
  // int start_url_ridx; //start index in fdset like urlset
  // int end_url_ridx;   // end index in fdset like urlset
  //fd_set urlset; //url's for the current page : relative idx wrt first_page_url
  union{
    unsigned int page_norm_id;  /*Using in JMeter for page norm id*/
    int urls_left;              /* Url still need to be asked for */
  };
  union{
    int urls_awaited;   /* Urls responses awaited for page to complete */
    int tx_hash_code;   /*Using in JMeter for transaction hash code*/
  };  
  u_ns_ts_t pg_begin_at;
  unsigned char is_cur_pg_bad; /* some URL returned failure */
  int is_embd_autofetch; /* This flag is set if we are autofetching the emebdded URLs.
                          * The purpose is to know when to free the allocated memory used for creating
                          * spoof embedded URLs structures.
                          */
  int redirect_count;    /* to keep backup */

  /* Next two variables are used to keep track of newly malloc'ed structures in case of auto fetch embd 
   * This is a temporary fix.
   */
  http_request_Shr_free *http_req_free;
  short num_http_req_free; 

  unsigned char sess_status;
  short vuser_state;
  u_ns_4B_t sess_think_duration; //Changing this to 4 byte as this will not cross the 4 byte limit 
  HostSvrEntry* head_hlist; //These head and tail are for keeping track of hosts with unexecuted URL's
  HostSvrEntry* tail_hlist; // tracked by prev_hlist & next_hlist of HostSvrEntry
  HostSvrEntry *hptr; // start of host server table

  //Following fields are initialised at the time of user creation
  u_ns_ts_t started_at;
  int modem_id;
  SessTableEntry_Shr* sess_ptr;
  /* Added to capture test case info */
  //    RunProfIndexTableEntry_Shr* rp_ptr;
  //	UserProfIndexTableEntry_Shr* up_ptr;
  //	SessProfIndexTableEntry_Shr* sp_ptr;
  RunProfIndexTableEntry_Shr* rp_ptr;
  UserProfIndexTableEntry_Shr* up_ptr;
  LocAttrTableEntry_Shr *location;
  AccAttrTableEntry_Shr *access;
  BrowAttrTableEntry_Shr *browser;
  ScreenSizeAttrTableEntry_Shr *screen_size;
  //MachAttrTableEntry_Shr *machine;
  //FreqAttrTableEntry_Shr *freq;
  //End of additions

  short cnum_parallel; //Current number of parallel connections overall
  short cmax_parallel; // Max number of parallel cinnections overall
  short per_svr_max_parallel; // Max number of parallel cinnections per server
  short compression_type; //compression type of mail URL
  connection* head_creuse; // keep track of unused link list of resue connections for the Vuser
  connection* tail_creuse; // head & tails for the vusers -> tracked by next_reuse and prev_reuse of connections
  connection* freeConnHead; // head of connection table slots that are corrently free
  //connection* first_cptr;
  /* Connection Pool Design: Added new connection ptr to store current cptr, which are 
   * neither free nor reused*/ 
  connection* head_cinuse; // keep track of current connections for a Vuser
  connection* tail_cinuse; // head & tails for the vusers -> tracked by next_reuse and prev_reuse of connections

  // Start - Added for new script design
  unsigned short operation; // Task to be performed
  short       next_pg_id;  // Page id of next if any else < 0

  // Union with now is made, as now is used in ns_end_session 
  union {
    u_ns_ts_t now;
    u_ns_ts_t pg_main_url_end_ts; // Used in reporting of inline block time, In case delay is not present
  };
  //Prachi(28/12/2012): Removed union because sp_vuser_next getting reset on setting task_next. 
  struct VUser* task_next; /* next Vuser slot in the task list */
  union{
    struct VUser* sp_vuser_next; //This is vuser linked list which is maintained by NVM  
    struct VUser* pause_vuser_next; //This is vuser pause linked list which is maintained by NVM
  };
  connection* last_cptr;   // Storing the last cptr used before going to next page
  // End - Added for new script design

  UserGroupEntry *ugtable; // start of User Group table
  UserCookieEntry *uctable;  // start of User Cookie table
  UserDynVarEntry* udvtable; //start of User Dynamic Variable table
#ifdef RMI_MODE
  UserByteVarEntry* ubvtable;
#endif
#ifdef WS_MODE_OLD
  UserTagAttrEntry* utatable;
#endif
  UserSvrEntry* ustable;
  UserVarEntry* uvtable;
  int* order_table;
  int server_entry_idx; /* index to config'ed servers for this user. Used if USE_HOST_CONFIG*/
  int page_status;
  int tx_ok;
  union{
    struct VUser* free_next; /* next free Vuser slot */
    struct VUser* pause_vuser_prev;
  };
  struct VUser* busy_next;
  struct VUser* busy_prev;
  unsigned int sess_inst;
  timer_type* timer_ptr;
#ifdef RMI_MODE
action_request_Shr* cur_url;
  ReqByteVarTableEntry_Shr* cur_bytevar;
  ReqByteVarTableEntry_Shr* end_bytevar;
  char *buffer_ptr;
  char buffer[4096];
#endif
  THITableEntry_Shr* taghashptr;
  union {
    int hashcode;
    int sync_point_id;
  };
  // Removing xmlParserCtxtPtr xml_parser from vptr as it is not used anywhere
  //xmlParserCtxtPtr xml_parser;
  //char referer[1096];
  //char referer[4];
  char *referer;
  unsigned short referer_size;        // this is used to store the length of the current referer
  unsigned short referer_buf_size;    // this is buffer size allocated to hold the referer
  int clust_id;
  int group_num;
  unsigned int user_index;
  IP_data *user_ip;
  //Following added for keeping the main URL data on the page
  struct copy_buffer* buf_head;
  struct copy_buffer* cur_buf;
  action_request_Shr *url_num;
  int bytes;
  //struct in_addr sin_addr;
  struct sockaddr_in6 sin_addr;
  //Following are added for keeping Page and Tx instances for a particular sess instabce
  unsigned short page_instance;
  unsigned short tx_instance;	// To generate unique transaction instance for each session
  int pg_think_time;
  u_ns_8B_t flags;
  int body_offset; // 02/08/07 -Atul For ResponseURL
#ifdef ENABLE_SSL
  //SSL_CTX *ctx;
  //SSL     *ssl;
  char  ssl_mode;
  //SSL_SESSION     *sess;
#endif

  char *tx_info_ptr; // We are making it char ptr so that other files need not include ns_trans.h. Do typecast where ever used.
  char *pcRunLogic; //Anuj: runlogic 21/02, this will be set through create_page_script_ptr(), url.c

  chunk_size_node *chunk_size_head; // Delete if possible

  union { 
    int replay_user_idx;          //replay index for user
    int num_requests;             // Generic currently used in TR069 for ACS connections
    int inline_req_delay;         // Used to save inline delay in case its set
  };

  int reload_attempts;    // Num of reloads to be attempted
  ServerMapChange *svr_map_change; // data to store new rec server mapping and
                                   // force mapping to this for a session
  int ka_timeout;

  HTTPData_t *httpData; //ptr to HTTPData, currently used for caching

  /*Used and allocate only if Java Script Engine is Enabled*/
  //VuserJSFields *js_fields;
  VUserCtx  *ctxptr; // For keeping virtual user context
  VUserThdData *thdd_ptr;  // Allocated only if running as thread and then freed at end of session
  struct Msg_com_con *mcctptr; //pointer for thread
  unsigned short phase_num;
  //userTraceData *pd_head; //To save page dump data
  void *pd_head; //To save page dump data
  
  int hash_index;  //To store hash index of transaction for sync point.
  //Will be used in Dns Caching.
  UserServerResolveEntry *usr_entry;
  long long partition_idx; //In release 3.9.7, in case of NDE/NS with partition, save partition index where we need to dump request/response files whereas in non partition request response files will logged in test run directory
  ResponseHdr *response_hdr;
  /* Websocket members - START*/
  union {
    struct
    {
     int ws_send_id; 
     int ws_close_id;
     connection **ws_cptr;
     char ws_status;      /* This field tell Websocket APIs status */
    };
    struct
    {
     int sockjs_close_id;
     connection **sockjs_cptr;
     char sockjs_status;      /* This field tell SockJs APIs status */
    };  
    struct
    {
     nsXmppInfo *xmpp;
     connection *xmpp_cptr;
     char xmpp_status;      /* This field tell XMPP APIs status */
    };
    struct
    {
     FC2 *fc2;
     connection *fc2_cptr;
     char fc2_status;      /* This field tell FC2 APIs status */
    };
    
    struct
    {
     void* session;
     void* channel;
    };
    struct
    {  
      int rp[2];
      int wp[2];
    };
    struct   /*bug 79149*/
    {
      FILE* xwp;
      char rdp_status; /*bug 79149*/
    };
  };
  connection **conn_array;
  //TODO remove ws_cptr and take simply pointer then malloc it.
  //connection *ws_cptr[1];
  /* Websocket members - END*/

  /*SOAP WS Security -START*/
  //int apply_ws_security;
  nsSoapWSSecurityInfo *ns_ws_info;
  /*SOAP WS Security -END*/
  int runtime_runlogic_flow_id;
  UniqueRangeVarVuserTable *uniq_rangevar_ptr;

  int retry_count_on_abort;                    //User's Retry Count on session abort.
  char *url_resp_buff; // URL response buff
  int url_resp_buff_size; // URL response buffer size
  int http_resp_code;
  SMMonSessionInfo *sm_mon_info;
} VUser;


/**
 * This macro assignes cptr->url_num with supplised value and
 * fills in gServerTable_idx which we require in case of auto fetch
 * embedded. There in, once we have freed the initialized structure (cptr->url_num) 
 * the index (which is extracted from cptr->url_num->index.svr_ptr->idx)
 * is also lost. Hence the backup.
 */
#define SET_URL_NUM_IN_CPTR(cptr, _url_num_) \
  if(cptr->url_num != _url_num_)\
  {\
    FREE_CPTR_PARAM_URL(cptr);\
  }\
  cptr->url_num = _url_num_;  \
  cptr->gServerTable_idx = cptr->url_num->index.svr_ptr->idx; \
  /*We have some issues with the url_num req type so we will use directly from cptr*/\
  cptr->request_type = cptr->url_num->request_type;\
  if(HTTP_MODE_HTTP2 == cptr->url_num->proto.http.http_version) \
    cptr->http_protocol = HTTP_MODE_HTTP2;      \
  NSDL2_HTTP(NULL, cptr, "Request type SET_URL_NUM_IN_CPTR = %d cptr is %p cptr->http_protocol=%d", cptr->request_type, cptr, cptr->http_protocol); \
  

extern VUser* allocate_user_tables(int num_users);

extern char *url_resp_buff;
extern int url_resp_size;
extern __thread char compression_type;
extern __thread int http_header_size;
//extern avgtime *average_time;
extern int log_records;
extern unsigned long long total_tx_errors;
extern int run_mode;

extern int num_select; /*TST */
extern int num_set_select; /*TST */
extern int num_reset_select; /*TST */

extern int v_epoll_fd;
//extern int max_vusers;
//extern int max_vusers_to_ramp_down;
//extern int max_sessions_to_ramp_down;
//extern int max_session;
extern int sigterm_received;
extern int end_test_run_mode; //Added var to save end test run mode 
extern int gNumVuserWaiting;
extern u_ns_ts_t cum_timestamp;
extern int gNumVuserActive;
extern int gNumVuserThinking;
extern int gNumVuserSPWaiting;
extern int gNumVuserBlocked; //added for FCS
extern int gNumVuserPaused;
extern u_ns_ts_t interval_start_time;
extern int gNumVuserCleanup;
//extern int ns_iid_handle;
//extern int max_connections;
//extern int warmup_session_done;
//extern int warmup_seconds_done;
extern unsigned int v_cur_progress_num;


extern char ns_target_buf[64];
extern char* argv0;
extern int do_checksum, do_throttle, do_verbose;
extern int ns_sickchild_pending; /*TST */
extern struct sockaddr_in parent_addr;
extern FILE* ssl_logs;
#ifndef CAV_MAIN
extern VUser* gBusyVuserTail;
extern VUser* gBusyVuserHead;
#else
extern __thread VUser* gBusyVuserTail;
extern __thread VUser* gBusyVuserHead;
#endif
//extern unsigned long user_index;
extern __thread int http_resp_code;

extern inline void Close_connection( connection* cptr, int chk, u_ns_ts_t now, int req_ok, int completion);
extern void dns_http_close_connection(connection* cptr, int chk, u_ns_ts_t now, int req_ok, int completion);
//extern inline PointerTableEntry_Shr* get_var_val(VUser* vptr, int var_val_flag, int var_hashcode);
extern inline SvrTableEntry_Shr* get_svr_ptr(action_request_Shr* request, VUser* vptr);
extern inline void free_connection_slot(connection* cptr, u_ns_ts_t now);
extern inline connection* get_free_connection_slot(VUser *vptr);
extern inline VUser* get_free_user_slot();
extern inline void start_new_socket(connection *cptr, u_ns_ts_t now);
extern int set_ssl_con (connection *cptr);
extern int handle_ssl_write (connection *cptr, u_ns_ts_t now);
extern void SetSockMinBytes(int fd, int minbytes);
extern void inline on_request_write_done ( connection* cptr);
extern PerHostSvrTableEntry_Shr* get_svr_entry(VUser* vptr, SvrTableEntry_Shr* svr_ptr);
extern void remap_all_urls_to_other_host(VUser *vptr, char *hostname, int port);
extern inline int get_req_status (connection *cptr);
extern inline void on_url_start(connection *cptr, u_ns_ts_t now);

extern int get_testid(void);
extern void set_log_dirs();
extern void *copy_scripts_to_tr();
extern void create_summary_report();
 
extern int string_init(void);
extern void set_logfile_names();
extern int confirm_netstorm_uid(char *cmd_name, char *err);
extern void handle_sickchild( int sig );
extern inline void reset_udp_array();
extern shr_logging* initialize_logging_memory(int num_children, const Global_data* gdata, int testidx, int g_generator_idx);
extern int log_test_case(int test_idx, const TestCaseType_Shr* test_case, 
                         const Global_data* gdata, GroupSettings *global_gset,
                         const SLATableEntry_Shr* sla_table, const MetricTableEntry_Shr* metric_table,
                         const ThinkProfTableEntry_Shr* think_table,
                         u_ns_ts_t start, u_ns_ts_t end);
extern void stop_logging(void);
extern char *get_time_in_hhmmss(int );
/* bug 79062 renamed*/
int h2_handle_non_ssl_write(connection* cptr, u_ns_ts_t now);
extern int handle_write( connection* cptr, u_ns_ts_t now );
extern void handle_read( connection* cptr, u_ns_ts_t now );
extern void handle_sigpipe( int sig );
//extern void handle_sigterm( int sig );
extern void handle_master_sigint( int sig );
extern inline void reset_udp_array();
extern int new_user( int num_users, u_ns_ts_t now, int scen_group_num, int user_first_sess, UniqueRangeVarVuserTable *unique_range_var_vusertable_ptr, SMMonSessionInfo *sm_mon_info);
extern inline void ssl_main_init();
extern void renew_connection(connection* cptr, u_ns_ts_t now );
extern void add_to_reuse_list(connection* cptr);
extern void hurl_done(VUser *vptr, action_request_Shr* url_num, HostSvrEntry *hptr, u_ns_ts_t now);
extern HostSvrEntry* next_remove_from_hlist(VUser* vptr, HostSvrEntry* hptr);
extern connection* remove_head_svr_reuse_list(VUser *vptr, HostSvrEntry* svr_ptr);
extern connection * remove_head_glb_reuse_list(VUser *vptr);
extern void close_accounting (u_ns_ts_t now);
extern void run_script_exit_func(VUser *vptr/*, u_ns_ts_t now*/);
extern void user_cleanup( VUser *vptr, u_ns_ts_t now);
extern void calc_pg_time(VUser*, u_ns_ts_t now);
extern void handle_url_complete(connection* cptr, int request_type, u_ns_ts_t now,
			        int url_ok, int is_redirect, int status,
				char taken_from_cache);

extern int log_url_record(VUser* vptr, connection* cptr, unsigned char status, u_ns_ts_t now, int is_redirect, const int con_num, ns_8B_t flow_path_instance, int url_id);
extern int get_max_log_dest();
extern int get_max_log_level();
extern int get_max_trace_dest();
extern int get_max_tracing_level();
extern int get_max_report_level();
//extern int generate_scen_group_num(int runtime_flag);

/* Proxy methods prototype */
extern void inline proxy_check_and_set_conn_server(VUser *vptr, connection *cptr);
extern inline int proxy_make_and_send_connect(connection* cptr, VUser *vptr, u_ns_ts_t now);
extern int handle_proxy_connect(connection *cptr, u_ns_ts_t now);

//extern char version_buf[80];
extern time_t g_start_time;

extern char *get_time_in_hhmmss(int );
extern gsl_rng* exp_rangen;
extern gsl_rng* weib_rangen;

extern void ssl_sess_free (VUser *vptr);

extern void copy_cptr_to_vptr(VUser *vptr, connection *cptr);
extern char* ns_eval_string_flag_internal(char* string, int encode_flag, long *size, VUser *api_vptr);

extern char CRLFString[3];
extern int CRLFString_Length;

#define FREE_LOCATION_URL(cptr, flag) {                                         \
    if (!(flag & NS_HTTP_REDIRECT_URL_IN_CACHE)) {                              \
      NSDL1_HTTP(NULL, NULL, "Location Url[%p] = %s.", cptr->location_url, cptr->location_url);         \
      FREE_AND_MAKE_NULL(cptr->location_url, "Freeing cptr->location_url", -1); \
    } else { \
       /* Force location_url to NULL as it may not 				\
        * freed in because of caching */ 					\
        cptr->location_url = NULL;						\
    }                                                                           \
  }

/* We can not FREE/NULL cptr->url before populate_auto_fetch and auto_redirect
 * as it uses this to find parent line, we we need to free before making a new
 * request because cptr can not hold it so Long*/
// Note - cptr->url_len does not have 1 extra for NULL
#define FREE_CPTR_URL(cptr) {                                     \
    if (cptr->flags & NS_CPTR_FLAGS_FREE_URL) {                   \
      NSDL1_HTTP(NULL, NULL, "Url = %s.", cptr->url);             \
      FREE_AND_MAKE_NOT_NULL_EX(cptr->url, (cptr->url_len + 1), "Freeing cptr->url", -1); \
      cptr->flags &= ~NS_CPTR_FLAGS_FREE_URL;                     \
    }                                                             \
    cptr->url = NULL;                                             \
    cptr->url_len = 0;                                            \
  }

#define FREE_CPTR_PARAM_URL(cptr) {				  \
  if( cptr->url_num && (cptr->url_num->is_url_parameterized == NS_URL_PARAM_VAL))\
  { \
    NSDL2_CONN(NULL, cptr, "Freeing url_num = %p", cptr->url_num); \
    action_request_Shr *parent_url_num = cptr->url_num->parent_url_num; \
    FREE_AND_MAKE_NULL(cptr->url_num, "cptr->url_num", -1);  \
    cptr->url_num = parent_url_num; \
  } \
}

//GRPC cptr and vptr flag 
#define NS_CPTR_CONTENT_TYPE_GRPC_PROTO   	0x0001000000000000
#define NS_CPTR_CONTENT_TYPE_GRPC_JSON	    	0x0002000000000000	
#define NS_CPTR_CONTENT_TYPE_GRPC_CUSTOM  	0x0004000000000000
#define NS_CPTR_CONTENT_TYPE_GRPC               0x0007000000000000 //NS_CPTR_CONTENT_TYPE_GRPC_PROTO|_JSON|_CUSTOM

#define NS_CPTR_GRPC_ENCODING			0x0008000000000000  

#define NS_VPTR_CONTENT_TYPE_GRPC_PROTO   	0x0000000000020000
#define NS_VPTR_CONTENT_TYPE_GRPC_JSON	    	0x0000000000040000	
#define NS_VPTR_CONTENT_TYPE_GRPC_CUSTOM  	0x0000000000080000
#define NS_VPTR_CONTENT_TYPE_GRPC		0x00000000000E0000 //NS_VPTR_CONTENT_TYPE_GRPC_PROTO|_JSON|_CUSTOM 

#define NS_VPTR_GRPC_ENCODING		  	0x0000000000100000

extern int flag_run_time_changes_called;
extern unsigned char get_request_type(connection *cptr);
extern inline void free_location_url_if_not_cached(connection *cptr, int is_redirect);

extern int inline ns_strncpy(char* dest, char* source, int num);
extern void create_report_file(int testidx, int create_summary_top_file);
extern int start_logging(const Global_data* gdata, int testidx, int create_slog);
extern inline void close_and_reset_fd();
extern int log_error_code();
extern void init_test_start_time();
extern char netstorm_usr_and_grp_name[258]; //128+1+128+1 Maximum chars for user and group name(128) colon(1) null char(1) 
/*Added new macro for close_fd*/
#define NS_DO_NOT_CLOSE_FD         0 /*In case of resusing connection, do not close fd*/
#define NS_FD_CLOSE_AS_PER_RESP    1 /*In success case close connection fd*/
#define NS_FD_CLOSE_REMOVE_RESP    2 /*For error case, connection failure and need to close old fd*/ 
//NetCloud: added variable to hold both generator index and nvm id to form unique child_idx for DDR.
unsigned short child_idx;

// Per nvm scratch buf to be used for varius purposes currntly used for page dump and user trace
#ifndef CAV_MAIN
extern char *ns_nvm_scratch_buf;
extern char *ns_nvm_scratch_buf_trace;
extern int ns_nvm_scratch_buf_size;
extern int ns_nvm_scratch_buf_len;
#else
extern __thread char *ns_nvm_scratch_buf;
extern __thread char *ns_nvm_scratch_buf_trace;
extern __thread int ns_nvm_scratch_buf_size;
extern __thread int ns_nvm_scratch_buf_len;
#endif
extern int monitor_scratch_buf_len;
extern char *monitor_scratch_buf;
extern int monitor_scratch_buf_size;

extern void create_summary_top(char* testrun);

extern inline void abort_page_based_on_type(connection *cptr, VUser *vptr, int url_type, int redirect_flag, int status);
extern inline void http_do_checksum(connection *cptr, action_request_Shr *url_num);

extern void create_partition_dir();
extern int compute_sess_rate(char *sess_value);
extern void http_close_connection( connection* cptr, int chk, u_ns_ts_t now );
extern inline void repeat_hurl(VUser *vptr, action_request_Shr* url_num, HostSvrEntry *hptr, u_ns_ts_t now);
extern void make_ns_common_files_dir_or_link(char *path);
extern void make_ns_raw_data_dir_or_link(char *path);
extern int create_links_for_logs(int path_flag);
extern void make_rbu_logs_dir_and_link();

extern inline VarTableEntry_Shr *get_fparam_var(VUser *vptr, int sess_id, int hash_code);
extern char* get_version(char* component);

extern void handle_imap_ssl_write (connection *cptr, u_ns_ts_t now);
extern void handle_pop3_ssl_write (connection *cptr, u_ns_ts_t now);

extern inline void  proc_nw_cache_stats_hdr(connection *cptr, char *header_buffer, int header_buffer_len,
                                             int *consumed_bytes, u_ns_ts_t now, unsigned int *nw_cache_stats_state);
extern void nw_cache_stats_chk_cacheable_header_parse_set_val(char *nw_cache_stats_hdr, int nw_cache_stats_hdr_len, connection *cptr);
extern inline void nw_cache_stats_save_partial_headers(char *header_buffer, connection *cptr, int length);
extern inline void nw_cache_stats_free_partial_hdr(connection *cptr);
 
//For HTTP2
extern void init_cptr_for_http2(connection *cptr);
extern int handle_http2_upgraded_connection(connection *cptr, u_ns_ts_t now, int chk, int req_type);
extern int http2_handle_write(connection *cptr, u_ns_ts_t now);
extern int http2_handle_read(connection *cptr, u_ns_ts_t now);
extern inline void free_http2_data( connection *cptr);
extern inline void http2_copy_data_to_buf(connection *cptr, unsigned char *buf, int bytes_to_process);
extern void add_node_to_queue(connection *cptr, stream *node);
extern void create_scripts_list();
extern inline void add_srcip_to_list(VUser *vptr, IP_data * ip);
extern void child_init_src_ip();
extern IP_data *get_src_ip(VUser *vptr, int net_idx);
extern void parent_init_src_ip();
extern void add_sock_to_list(int fd, int shut, GroupSettings *gset);

extern Long_data rtg_pkt_ts;
extern char g_test_or_session[32];
extern char g_controller_wdir[];
extern char g_controller_testrun[];
extern int copy_cptr_to_stream(connection *cptr,  stream *stream_ptr);

extern int extract_buffer(FILE *flow_fp, char *start_ptr, char **end_ptr, char *flow_file);
extern void nw_cache_stats_headers_parse_set_value(char *nw_cache_stats_hdr, int nw_cache_stats_hdr_len, connection *cptr, unsigned int *nw_cache_stats_hdr_pkt); 
extern int kw_set_jmeter_settings(char *buf, GroupSettings *gset, char *err_msg, int rumtime_flag);
extern void set_ssl_recon (connection *cptr);
extern int fc2_handle_write(connection *cptr, u_ns_ts_t now); 
extern int fc2_handle_read(connection *cptr, u_ns_ts_t now); 
extern void init_fc2(connection *cptr, VUser *vptr);
extern int fc2_make_and_send_frames(VUser *vptr, u_ns_ts_t now);
/* bug 52092: make_proxy_and_send_connect() added */
inline int make_proxy_and_send_connect(connection* cptr, VUser *vptr, u_ns_ts_t now);
extern void socket_close_connection(connection* cptr, int done, u_ns_ts_t now, int req_ok, int completion);
#endif
