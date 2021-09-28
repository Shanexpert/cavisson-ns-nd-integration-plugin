

typedef struct {
  char con_type;
  char protocol;                 /* This variable is a flag to select protocol */
  char dummy[2]; //To minimize the holes
  int fd;
  int modem_id;     /* Wan Env */
} listener;

/*
//static int *child_table;
typedef struct {
  pid_t pid;
  int spawn_count;
  u_ns_ts_t last_spawn_time;
  unsigned short min_port;
  unsigned short max_port;
  unsigned short cur_port;
} child_table_t;
*/

typedef struct {
  char ip[1024]; /* check if big enough for ipv6 */
  char  status;  // Active/Deactive/Deleted/Not Initired
  listener   *socket_tcp_lfd;    // Listen fd (one per port for SOCKET)
  listener   *socket_tcps_lfd;    // Listen fd (one per port for SOCKET)
} ChildIPData;

typedef struct {
  int child_pid;
  ChildIPData *child_ip_data;
  int num_ip_data_entries;
  int max_slot;
  int shmid;
} ChildInfo;

#define MAX_DELIMETER_SIZE 64
#define RPR_FIRST_BYTE_RECV_TIMEOUT 	60000
#define RPR_IDLE_TIMEOUT 		60000
#define RPR_MAX_READ_TIMEOUT 		900000

typedef struct
{
  //RESPONSE
  int socket_decoding_type;
  int socket_end_policy_mode;
  long socket_end_policy_value;

  int socket_dynamic_len_type;
  char socket_delimeter[MAX_DELIMETER_SIZE];
  long read_msg_max_len;       /*Max msg len to be read*/

  //SSL certificates
  char socket_ssl_cert[1024];

  int first_byte_recv_timeout; //first_byte_recv_timeout
  int idle_timeout;     //timeout between two data packets  
  int max_read_timeout; // max time to read data when data is coming continously.
  int len_endian;
} SocketConfiguration;

typedef struct
{
  char recording_name[64];
  int  recording_port;
  char server_ip[24];

  int  socket_port;
  char socket_is_ssl;
  char socket_protocol[10];

  int keep_alive_timeout;  //connection timeout

  int recorder_timeout; //Overall timeout. Ather this recorder will stop.

  SocketConfiguration req_conf;
  SocketConfiguration resp_conf;
} ConfigurationSettings;

struct connection
{
  char con_type;
  char state;
  char proto_type;   // for protocol selection, like HTTP, SMTP, SSL or Non SSL ; earliar we were using is_ssl_on
  int fd; 

  timer_type* timer_ptr;
  long bytes_remaining;           // Total bytes remaining to be send in the response or bytes left to send in handling incomplete iovector for CR URL

  off_t offset;                  
  struct connection *free_next;
  struct connection *busy_next;
  struct connection *busy_prev;
  SSL *ssl;    			//client ssl connction
  char starttls;
  int content_length;           // Content length of the request.

  struct copy_buffer* buf_head;  // Pointer to body linked list
  struct copy_buffer* cur_buf;   // Current position in the body linked list (for filling next read bytes) or for buffer used to save the whole request for CR URL

  long bytes;                    // Total size of the body in the request recieved by HPD
  struct sockaddr_in6 server_addr;
  long read_start_timestamp;

  char type;
  int body_offset;
  char is_data_complete;

  struct connection *cptr_paired;
  ConfigurationSettings *conf_settings;
} ;

typedef struct connection connection;

#define COPY_BUFFER_LENGTH            1022

struct copy_buffer
{
  char buffer[COPY_BUFFER_LENGTH];
  struct copy_buffer* next;
};
/*
typedef struct
{
  //REQUEST
  char socket_decoding_type;
  char socket_end_policy_mode;
  char socket_end_policy_value;
  char socket_end_policy_value_2;

  //SSL certificates
  char socket_ssl_cert[1024];
} RequestConfiguration;
*/

#define REQUEST 1
#define RESPONSE 2

#define SOCKET_PASS 10      
#define SOCKET_FAIL 11     
#define MAX_HDR_SIZE 64 * 1024

#define MAX_REQ_SIZE 64 * 1024

// Bits which are not reset after request is processed
#define RPR_CPTR_FLAGS_RESET_ON_CLOSE_MASK  0xFF000000 // bits 24 to 31 are not reset

#define MAX_CON_PER_CHILD (32*1024)

//FOR cptr
#define USER_CHUNK_SIZE 1024

#define SOCKET_TCP_PROTOCOL          0
#define SOCKET_TCPS_PROTOCOL         1

//Connection type 
#define RPR_CON_TYPE_LISTENER         0
#define RPR_CON_TYPE_CPTR             1
#define RPR_CON_TYPE_PARENT_CHILD     2

#define RPR_DL(cptr, ...) rpr_debug_log_ex(_FLN_, cptr, __VA_ARGS__)
//#define PARENT (parent_pid == getpid())
#define RPR_EXIT(exit_status, ...) rpr_exit(_FLN_, exit_status, __VA_ARGS__)
#define _FLN_  __FILE__, __LINE__, (char *)__FUNCTION__
//#define rpr_error_log if(g_err_log) rpr_error_log_ex
//#define rpr_error_log rpr_error_log_ex

#define MAX_FILE_NAME_LEN 	   2048
#define MAX_LOCAL_BUFF_SIZE        1024 

#define RPR_TCP_SEGMENT_SOCKET_EPM       0
#define RPR_FIX_LEN_SOCKET_EPM           1
#define RPR_DELIMETER_SOCKET_EPM         2
#define RPR_DYNAMIC_LEN_SOCKET_EPM       3
#define RPR_REQ_READ_TIMEOUT_SOCKET_EPM	 4
#define RPR_REQ_CONTAINS_SOCKET_EPM	 5
#define RPR_CONN_CLOSE_SOCKET_EPM	 6

#define SLITTLE_ENDIAN                   0
#define SBIG_ENDIAN                      1

#define CONNECT_TIMEOUT         60000    //60 secs
#define SEND_TIMEOUT            60000    //60 secs
#define READ_TIMEOUT_FB         60000    //60 secs
#define READ_TIMEOUT_IDLE       60000    //60 secs

#define LEN_TYPE_TEXT           0
#define LEN_TYPE_BINARY         1
                                
#define MSG_TYPE_TEXT           0
#define MSG_TYPE_BINARY         1 
#define MSG_TYPE_HEX            2
#define MSG_TYPE_BASE64         3
                                
#define ENC_NONE                0 
#define ENC_BINARY              1 
#define ENC_HEX                 2
#define ENC_BASE64              3

//Message format length bytes
#define RPR_SOCKET_MSGLEN_BYTES_CHAR         1
#define RPR_SOCKET_MSGLEN_BYTES_SHORT        2
#define RPR_SOCKET_MSGLEN_BYTES_INT          4
#define RPR_SOCKET_MSGLEN_BYTES_LONG         8

#if 0
#define DEFAULT_SOCKET_EPM               0
#define FIX_REQ_LEN_SOCKET_EPM           1
#define REQ_DELIMETER_SOCKET_EPM         2
#define DYNAMIC_REQ_LEN_SOCKET_EPM       3
#define TIMEOUT_SOCKET_EPM               4
#endif

#define MAX_TOKEN 64
#define ERROR_BUF_SIZE 4096

// Connection states
#define UNALLOC 0
#define READ_REQUEST 1
#define CNST_CONNECTING 2
#define PROCESS_SOCKET_TCP_WRITE 3
#define SSL_ACCEPTING 4
#define CNST_SSLCONNECTING 5

#define CNST_REQ_LINE 1
#define CHK_NO_CHUNKS 0
#define HDST_TEXT 1 

#define MAX_AFFINITY_CHILD_PROCS 800 // Max number of affinity childs

#define RPR_DEFAULT_CONN_TIMEOUT 450000

#define DEBUG_HEADER "\nAbsolute Time Stamp|File|Line|Function|Child Index|Parent/Child ID|Request ID|FD|State|Log Messages"

#define _FL_  __FILE__, __LINE__
#define _FLN_  __FILE__, __LINE__, (char *)__FUNCTION__

#define DO_NOT_CLOSE_CONNECTION                   0 
#define CLOSE_CONNECTION_DUE_TO_DONE              1 
#define CLOSE_CONNECTION_DUE_TO_KA_TIMEOUT        100
#define CLOSE_CONNECTION_DUE_TO_FIRST_REQ_TIMEOUT 101

//This macro is defined to check all buffer allocated in connection struct are free or not
#define CHECK_BUF_FREE(cptr) \
{ \
  if((cptr->buf_head != NULL) || (cptr->cur_buf != NULL)) \
  { \
    rpr_error_log(_FLN_, cptr, "fd = %d, cptr->buf_head = %p cptr->cur_buf = %p", cptr->fd, cptr->buf_head, cptr->cur_buf); \
  } \
}

#define CLOSE_FD_CLOSE_CONN(cptr){ \
  rpr_error_log(_FLN_, cptr, "Connection is closed by client. Closing fd and connection."); \
  close_fd(cptr, 1, now); \
  close_fd(cptr->cptr_paired, 1, now); \
}

#define CLOSE_FD_REUSE_CONN(cptr){ \
  RPR_DL(cptr, "Connection reuse."); \
  close_fd(cptr, 0, now); \
  close_fd(cptr->cptr_paired, 0, now); \
}

#define MY_MALLOC(new, size, cptr, msg) {                               \
    if (size < 0) {                                                     \
      rpr_error_log_ex(_FLN_, cptr, "Trying to malloc a negative size (%d)", size); \
      exit(1);                                                          \
    } else if (size == 0) {                                             \
      rpr_error_log_ex(_FLN_, cptr, "Trying to malloc a 0 size"); \
      new = NULL;                                                       \
    } else {                                                            \
      new = (void *)malloc( size );                                     \
      if ( new == (void*) 0 ) {                                         \
        rpr_error_log_ex(_FLN_, cptr, "Out of Memmory: %s", msg); \
        exit(1);                                                        \
      }                                                                 \
      RPR_DL(cptr, "MY_MALLOC'ed (%s) done. ptr = %p, size = %d", msg, new, size); \
    }                                                                   \
  }

#define FREE_AND_MAKE_NULL(to_free, cptr, msg) {                        \
    if (to_free) {                                                      \
      RPR_DL(cptr, "MY_FREE'ed (%s) Freeing ptr = %p", msg, to_free); \
      free(to_free); to_free = NULL;                                    \
    }                                                                   \
  }

#define FREE_AND_MAKE_NOT_NULL(to_free, cptr, msg)  \
{                        \
  if (to_free) {                                                      \
   RPR_DL(cptr, "MY_FREE'ed (%s) Freeing ptr = %p", msg, to_free); \
   free(to_free);  \
  } \
}


#define SSL_CLEANUP \
if (cptr->proto_type == SOCKET_TCP_PROTOCOL) { \
  if(cptr->ssl){ \
    SSL_shutdown(cptr->ssl); \
    SSL_free(cptr->ssl); \
    cptr->ssl = NULL; \
  } \
}

#define SOCKET_TCP_HANDLE_EAGAIN_NEW \
  cptr->bytes_remaining = to_send; \
  if(cptr->state == READ_REQUEST) \
  {\
    cptr->state = PROCESS_SOCKET_TCP_WRITE;\
    MOD_SELECT_AFTER_WRITE_BLOCKS();\
  } \
  return;

#define MOD_SELECT_AFTER_WRITE_BLOCKS() { \
  mod_select(cptr, cptr->fd, EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP); \
} 

// This macro will call if there was error in sending response
// And error in reading from file or sending on socket connection
#define RPR_HANDLE_ERROR   \
  close_fd(cptr, 1, now); \
  return;

extern char g_wdir[];

extern void rpr_parse_conf_file(int child_id, ConfigurationSettings *conf_settings);
extern void rpr_debug_log_ex(char *file, int line, char *fname, void *cptr_void, char *format, ...);
extern void rpr_exit(char *file, int line, char *fname, int exit_status, char *format, ...);
extern void close_fd(connection *cptr, int finish, u_ns_ts_t now);
extern inline void mod_select(connection* cptr, int fd, int event);
extern inline void add_select(void* ptr, int fd, int event);
extern inline void remove_select(int fd);
extern void rpr_create_child_script_dir(ConfigurationSettings conf_settings);
extern void rpr_write_data_on_socket(connection *cptr, u_ns_ts_t now, char *data_to_send, ConfigurationSettings *conf_settings);
extern void rpr_dump_req_resp_in_file(connection *cptr, ConfigurationSettings *conf_settings, char *input_data, int input_data_len);
