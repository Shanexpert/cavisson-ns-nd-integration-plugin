/* 
 * this is the internal file for socket api defns. The other file (ns_socket_api.h) 
 * contains only defns that need to be available for the script to be compiled. They are kept
 * separate as there is no need to expose these interfaces or data to the script
 */
#ifndef NS_SOCKET_API_INT_H
#define NS_SOCKET_API_INT_H

#include "ns_socket_api.h"      //include the external defns as well

#define USER_SOCK_TCP 0x00000004
#define USER_SOCK_UDP 0x00000008
#define USER_SOCK_LISTEN  0x00000010
#define USER_SOCK_REMOTE  0x00000020
#define USER_SOCK_ACCEPT  0x00000040

#define DEFAULT_ACCEPT_TIMEOUT 10
#define DEFAULT_RECV_TIMEOUT 10
#define DEFAULT_RECV2_TIMEOUT 10
#define DEFAULT_SEND_TIMEOUT 10
#define DEFAULT_CONNECT_TIMEOUT 10

#define LASTARG_MARKER "NSLastarg"    //last arg in variable list
//for dynamic parsing, the # of args after fixed, excluding LASTARG_MARKER
#define NS_CREATE_SOCKET_MAX_VARARGS 3 
#define NS_SEND_MAX_VARARGS 2
#define NS_RECV_EX_MAX_VARARGS 5
#define NS_RECV_MAX_VARARGS 1
#define NS_RECV_BUFSIZE (1024)     //1k
#define MAXVAR  1024
#define HOST_PORT_SEPARATOR ':'

#define MAX_SOCKET_API_ARGS 8   //max # of args in any api call -includes last marker
#define NO_FIXED_ARGS_CREATE_SOCKET 3   // this is actually 2 + 1 (LASTARG_MARKER) 
#define NO_FIXED_ARGS_SEND 3   // this is actually 2 + 1 (LASTARG_MARKER) 
#define NO_FIXED_ARGS_RECV_EX 3   // this is actually 2 + 1 (LASTARG_MARKER) 
#define NO_FIXED_ARGS_RECV 3   // this is actually 2 + 1 (LASTARG_MARKER) 
#define MAX_API_NAME_LENGTH   256
#define EQUALS '='
#define COMMA ','
#define COMMA_STR ","

#define MAX_UDP_SEND_SIZE (1024)    //used in the send routine 

#define SKIP_SPACE(x) {\
while (isspace(*(x))) (x)++;    \
}

#define SKIP_SPACE_NL(x) {\
while ( ((*(x)) == ' ') || ((*(x)) == '\t') || ((*(x)) == '\n') ) (x)++;    \
}
//#define MIN(x,y)  ((x<y)?x:y)     -- also in param.h

#define UNQUOTE(x,y)  {\
u_char *px = (x);     \
u_char *py = (y);      \
if (*px == 0x22)  {     \
  px++;     \
  while (*px && (*px != 0x22))   { \
   *py++ = *px++;      \
  }   \
  *py = 0;      \
}     \
}

#define MALLOC_AND_MEMCPY(src, dest, size, msg, index)   \
{                        \
  MY_MALLOC(dest, size, msg, index);  \
  memcpy((char *)dest, (char *)src, size);     \
}


#define ESCAPE_CHAR '\\'

#define STRINGIFY(x)  #x

#define DEFAULT_LOCAL_PORT 0      //port in ns_create_socket
#define DEFAULT_REMOTE_PORT 80    //port in ns_create_socket
#define DEFAULT_TARGET_PORT 80    //port in ns_send
#define DEFAULT_BACKLOG 128
#define MAX_USER_SOCK_DATA 1024

#define DEFAULT_LOCAL_PORT_STR  "0" 
#define DEFAULT_REMOTE_PORT_STR "80"
#define DEFAULT_TARGET_PORT_STR  "80"
#define MAX_USER_TO_NVM_CTX_SWITCHES 5

//this is attached to the cptr
//we could put the timeouts here, but those are actually per group (they are in group settings)
typedef struct {
  int sockType;     //TCP, UDP etc.
  char* recvBuf;    //buffer to store send/ recv data
  int recvBufBytes; //# of bytes recvd
} userSocketData;


enum {READ_SELECT, WRITE_SELECT};


#ifdef USER_SOCKET_STATS
// Set cumulative elements of USER_SOCKET struct a with b into a
#define SET_MIN_MAX_USER_SOCKET_CUMULATIVES(a, b)\
  NSDL2_MESSAGES(NULL, NULL, "Before user_socket_c_min_time = %llu, user_socket_c_max_time = %llu", (a)->user_socket_c_min_time, (b)->user_socket_c_min_time); \
  SET_MIN ((a)->user_socket_c_min_time, (b)->user_socket_c_min_time);\
  SET_MAX ((a)->user_socket_c_max_time, (b)->user_socket_c_max_time);\
  NSDL2_MESSAGES(NULL, NULL, "After user_socket_c_min_time = %llu, user_socket_c_max_time = %llu", (a)->user_socket_c_min_time, (b)->user_socket_c_min_time);

//Set  periodic elements of USER_SOCKET struct a with b into a
#define SET_MIN_MAX_USER_SOCKET_PERIODICS(a, b)\
  SET_MIN ((a)->user_socket_min_time, (b)->user_socket_min_time);\
  SET_MAX ((a)->user_socket_max_time, (b)->user_socket_max_time);

//accukumate cumulative elements of USER_SOCKET struct a with b into a 
#define ACC_USER_SOCKET_CUMULATIVES(a, b)\
  (a)->cum_user_socket_fetches_started += (b)->cum_user_socket_fetches_started;\
  (a)->user_socket_fetches_completed += (b)->user_socket_fetches_completed;\
  NSDL2_MESSAGES(NULL, NULL, "Before user_socket_succ_fetches = %llu, user_socket_succ_fetches = %llu",(a)->user_socket_succ_fetches, (b)->user_socket_succ_fetches); \
  (a)->user_socket_succ_fetches += (b)->user_socket_succ_fetches;\
  NSDL2_MESSAGES(NULL, NULL, "after user_socket_succ_fetches = %llu, user_socket_succ_fetches = %llu",(a)->user_socket_succ_fetches, (b)->user_socket_succ_fetches); \
  (a)->user_socket_c_tot_time += (b)->user_socket_c_tot_time;\
  (a)->user_socket_c_num_con_initiated += (b)->user_socket_c_num_con_initiated;\
  (a)->user_socket_c_num_con_succ += (b)->user_socket_c_num_con_succ;\
  (a)->user_socket_c_num_con_fail += (b)->user_socket_c_num_con_fail;\
  (a)->user_socket_c_num_con_break += (b)->user_socket_c_num_con_break;\
  for (i = 0; i < TOTAL_URL_ERR; i++) {\
    (a)->cum_user_socket_error_codes[i] += (b)->cum_user_socket_error_codes[i]; \
  }


#define ACC_USER_SOCKET_PERIODICS(a, b)\
  (a)->user_socket_total_bytes += (b)->user_socket_total_bytes ;\
  (a)->user_socket_tx_bytes += (b)->user_socket_tx_bytes;\
  (a)->user_socket_rx_bytes += (b)->user_socket_rx_bytes;\
  (a)->user_socket_c_tot_total_bytes += (b)->user_socket_c_tot_total_bytes;\
  (a)->user_socket_c_tot_tx_bytes += (b)->user_socket_c_tot_tx_bytes;\
  (a)->user_socket_c_tot_rx_bytes += (b)->user_socket_c_tot_rx_bytes;\
  (a)->user_socket_num_con_initiated += (b)->user_socket_num_con_initiated;\
  (a)->user_socket_num_con_succ += (b)->user_socket_num_con_succ;\
  (a)->user_socket_num_con_fail += (b)->user_socket_num_con_fail;\
  (a)->user_socket_num_con_break += (b)->user_socket_num_con_break;\
  NSDL2_MESSAGES(NULL, NULL, "Before user_socket_succ_fetches = %llu, user_socket_succ_fetches = %llu",(a)->user_socket_succ_fetches, (b)->user_socket_succ_fetches); \
  (a)->user_socket_succ_fetches += (b)->user_socket_succ_fetches;\
  NSDL2_MESSAGES(NULL, NULL, "after user_socket_succ_fetches = %llu, user_socket_succ_fetches = %llu",(a)->user_socket_succ_fetches, (b)->user_socket_succ_fetches); \
  (a)->user_socket_fetches_completed += (b)->user_socket_fetches_completed;\
  (a)->user_socket_num_hits += (b)->user_socket_num_hits;\
  (a)->user_socket_num_tries += (b)->user_socket_num_tries;\
  (a)->user_socket_avg_time += (b)->user_socket_avg_time;\
  (a)->user_socket_c_avg_time += (b)->user_socket_c_avg_time;\
  (a)->user_socket_tot_time += (b)->user_socket_tot_time;\
  (a)->user_socket_c_tot_time += (b)->user_socket_c_tot_time;\
  (a)->user_socket_fetches_started += (b)->user_socket_fetches_started;\
  (a)->cum_user_socket_fetches_started += (b)->cum_user_socket_fetches_started;\
 /*(a)->user_socket_response[MAX_GRANULES+1] += (b)->user_socket_response[MAX_GRANULES+1];\
   (a)->user_socket_error_codes[TOTAL_URL_ERR] += (b)->user_socket_error_codes[TOTAL_URL_ERR];\
   (a)->cum_user_socket_error_codes[TOTAL_URL_ERR] += (b)->cum_user_socket_error_codes[TOTAL_URL_ERR];*/\
  for (i = 0; i < TOTAL_URL_ERR; i++) {\
    (a)->user_socket_error_codes[i] += (b)->user_socket_error_codes[i];\
  }
                 
#define CHILD_RESET_USER_SOCKET_AVGTIME(a) \
  if(global_settings->protocol_enabled & USER_SOCKET_PROTOCOL_ENABLED) { \
    memset(a->user_socket_error_codes, 0, TOTAL_URL_ERR * sizeof(int)); \
    a->user_socket_num_hits = 0;\
    a->user_socket_num_tries = 0;\
    a->user_socket_avg_time = 0;\
    a->user_socket_min_time = 0xFFFFFFFF;\
    a->user_socket_max_time = 0;\
    a->user_socket_tot_time = 0;\
    a->user_socket_fetches_started = 0;\
    a->user_socket_num_con_initiated = 0;\
    a->user_socket_num_con_succ = 0;\
    a->user_socket_num_con_fail = 0;\
    a->user_socket_num_con_break = 0;\
    a->user_socket_total_bytes = 0;\
    a->user_socket_tx_bytes = 0;\
    a->user_socket_rx_bytes = 0;\
  }
#define CHILD_USER_SOCKET_SET_CUM_FIELD_OF_AVGTIME(a)\
  if(global_settings->protocol_enabled & USER_SOCKET_PROTOCOL_ENABLED) { \
    SET_MIN(a->user_socket_c_min_time, a->user_socket_min_time); \
    SET_MAX(a->user_socket_c_max_time, a->user_socket_max_time); \
    (a)->user_socket_num_connections = user_socket_num_connections; \
    (a)->user_socket_fetches_completed += (a)->user_socket_num_tries;\
    (a)->user_socket_succ_fetches += (a)->user_socket_num_hits;\
    (a)->cum_user_socket_fetches_started += (a)->user_socket_fetches_started;\
    (a)->user_socket_c_tot_time += (a)->user_socket_tot_time;\
    (a)->user_socket_c_tot_total_bytes +=(a)->user_socket_total_bytes;\
    (a)->user_socket_c_tot_tx_bytes += (a)->user_socket_tx_bytes;\
    (a)->user_socket_c_tot_rx_bytes += (a)->user_socket_rx_bytes;\
    (a)->user_socket_c_num_con_succ += (a)->user_socket_num_con_succ;\
    (a)->user_socket_c_num_con_initiated += (a)->user_socket_num_con_initiated;\
    (a)->user_socket_c_num_con_fail += (a)->user_socket_num_con_fail;\
    (a)->user_socket_c_num_con_break += (a)->user_socket_num_con_break;\
    memcpy(a->cum_user_socket_error_codes, a->user_socket_error_codes, sizeof(int) * TOTAL_URL_ERR); \
  }

// This is structure for  user_socket avgtime
// Filled by child and send to parent in progress report
typedef struct {
  int user_socket_num_connections;

  u_ns_8B_t user_socket_total_bytes;
  u_ns_8B_t user_socket_tx_bytes;
  u_ns_8B_t user_socket_rx_bytes;
  u_ns_8B_t user_socket_c_tot_total_bytes;
  u_ns_8B_t user_socket_c_tot_tx_bytes;
  u_ns_8B_t user_socket_c_tot_rx_bytes;

  u_ns_8B_t user_socket_num_con_initiated;   //Number of initiated requests for making the connection (periodic)
  u_ns_8B_t user_socket_num_con_succ;        //Number of succ requests for making the connection out of initiated (periodic)
  u_ns_8B_t user_socket_num_con_fail;        //Number of failures requests for making the connection out of initiated (periodic)
  u_ns_8B_t user_socket_num_con_break;       //Number of initiated requests for closing the connection (periodic)

  u_ns_8B_t user_socket_c_num_con_initiated; //Number of initiated requests for making the connection(cumulative)
  u_ns_8B_t user_socket_c_num_con_succ;      //Number of succ requests for making the connection out of initiated(cumulative)
  u_ns_8B_t user_socket_c_num_con_fail;      //Number of fail requests for making the connection out of initiated(cumulative)
  u_ns_8B_t user_socket_c_num_con_break;     //Number of initiated requests for closing the connection(cumulative)

  u_ns_8B_t user_socket_succ_fetches;
  u_ns_8B_t user_socket_fetches_completed;
  u_ns_8B_t user_socket_num_hits;
  u_ns_8B_t user_socket_num_tries;
  u_ns_8B_t user_socket_avg_time;
  u_ns_8B_t user_socket_c_avg_time;
  u_ns_8B_t user_socket_min_time;
  u_ns_8B_t user_socket_c_min_time;
  u_ns_8B_t user_socket_max_time;
  u_ns_8B_t user_socket_c_max_time;
  u_ns_8B_t user_socket_tot_time;
  u_ns_8B_t user_socket_c_tot_time;
  u_ns_8B_t user_socket_fetches_started;
  u_ns_8B_t cum_user_socket_fetches_started;

  u_ns_4B_t user_socket_response[MAX_GRANULES+1];
  u_ns_4B_t user_socket_error_codes[TOTAL_URL_ERR];   /* for url error codes 1-8 */
  u_ns_4B_t cum_user_socket_error_codes[TOTAL_URL_ERR];   /* for url error codes 1-8 */
} userSocketAvgTime;  


// Cumulative counters - Used by parent ONLY
typedef struct {
  u_ns_8B_t user_socket_c_num_con_initiated;
  u_ns_8B_t user_socket_c_num_con_succ;
  u_ns_8B_t user_socket_c_num_con_fail;
  u_ns_8B_t user_socket_c_num_con_break;
  u_ns_8B_t user_socket_succ_fetches;
  u_ns_8B_t user_socket_fetches_completed;
  u_ns_8B_t user_socket_c_min_time;
  u_ns_8B_t user_socket_c_max_time;
  u_ns_8B_t user_socket_c_tot_time;
  u_ns_8B_t cum_user_socket_fetches_started;
  u_ns_4B_t cum_user_socket_error_codes[TOTAL_URL_ERR];   /* for url error codes 1-8 */
} userSocketCAvgTime;


extern int g_user_socket_avgtime_idx;
extern int g_user_socket_cavgtime_idx;
extern int g_FTPcavgtime_size;

extern userSocketAvgTime *user_socket_avgtime;
extern userSocketCAvgTime *user_socket_cavgtime;
//reporting related
extern void user_socket_update_avgtime_size();
extern  void fill_user_socket_gp (userSocketAvgTime *user_socket_avg);
extern void print_user_socket_throughput(FILE *fp1, FILE *fp2, int is_periodic, avgtime *avg);
extern void user_socket_log_summary_data (avgtime *avg, double *user_socket_data, FILE *fp);
extern  void user_socket_update_cavgtime_size();
#endif    //USER_SOCKET_STATS

void debug_log_user_socket_data(connection *cptr, char *buf, int size, int complete, int flag);
extern int parse_ns_socket_api(FILE *flow_fp, FILE *outfp, char *flow_filename, char *flowout_filename, int sess_idx);

#endif  //NS_SOCKET_API_INT_H
