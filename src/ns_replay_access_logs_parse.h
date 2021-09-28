#ifndef NS_REPLAY_ACCCES_LOGS_PARSE_H 
#define NS_REPLAY_ACCCES_LOGS_PARSE_H

#define REPLAY_USING_WEB_SERVICE_LOGS 	1
#define REPLAY_USING_ACCESS_LOGS 	2

typedef struct
{
  u_ns_ts_t start_time;     // Relative start time in milliseconds
  int          page_id;     // numeric id name as used in script.h
  unsigned int offset;      // Offset in the file
  unsigned int size;        // Size of request in file (hdr size in access log, complete size in web services)
  unsigned int body_size;   // Size of body in case of access log
  // making union as creation_ts_offset is used in wells server and inline_start_idx is used in IIS RAL 
  union {
    int inline_start_idx;     // Start index of inline table
    unsigned int creation_ts_offset;
  };
  // making union as creation_ts_size is used in wells server and num_inline_entries is used in IIS RAL and we dont use have inline url in wells 
  union {
    short creation_ts_size;
    short num_inline_entries;   // No of Inlines for the request (Code has limit of 1000. need to fix)
  };
  short type;
} ReplayReq; 

// keeps the inline urls information
typedef struct 
{
  u_ns_ts_t start_time; // start time stamp of user, currently not used 
  int host_idx;
  unsigned int offset; // Offset will be start 
  unsigned int size;
  //unsigned int body_size; // Not used
} ReplayInLineReq;

typedef struct 
{
  char *user_id;        // Not to be used in phase 1
  unsigned int  start_index;    // Index in ReplayReq  of the first page
  unsigned short  num_req;       // Number of ReplayReq 
  unsigned short  cur_req;       // Current request which is being send 
  // Next request was added in 3.9.1 to handle case where cur_req does not increasment e.g. when connection fails or SSL handshake fails as
  // cur_req is increamened in make replay req which is not called in these cases
  unsigned short  next_req;      // Next request which is to be send next time. 
  //int req_file_idx;   //this i put to getfile name after parsing ..
  int req_file_fd;
  unsigned int line_num;
  u_ns_ts_t users_timestamp;
} ReplayUserInfo;

typedef struct 
{
  char request_file_name[1024];    
  int request_file_fd;
} ReqFile;

typedef struct
{
  u_ns_ts_t user_start_time;
  unsigned int line_index_from_last_file;
  int page_index_from_last_file;
} Replay_last_data;

// Host Table for replay inline hosts
// These host will be loaded to to host table by parent at the time of initialization
typedef struct
{
  int type;  // Type of host http or https
  char *host; // host name 
} ReplayHosts;

extern ReplayHosts *g_replay_host;

#define REPLAY_HOST_HTTP 1
#define REPLAY_HOST_HTTPS 2

extern ReplayReq *g_replay_req;
extern ReplayInLineReq *g_inline_replay_req; 
extern ReplayUserInfo *g_replay_user_info;

extern unsigned int total_replay_user_entries;
extern unsigned int g_cur_usr_index;
extern int replay_format_type;
extern char replay_file_dir[];

extern int g_inter_page_time_factor;

extern void kw_set_replay_file(char *buf, int flag);
extern int kw_set_replay_factor(char *buf, char *err_msg, int flag);
extern void kw_set_replay_resume_option(char *buf, int flag);
extern void ns_parse_usr_info_file(int child_id);
extern void write_to_last_file();
extern void read_ns_index_last();
extern int ntp_get_num_nvm();
extern void parent_replay_init();
extern void load_replay_host();
extern void copy_replay_profile();
#endif
