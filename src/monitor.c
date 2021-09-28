/*********************************************************************
* Name: monitor.c
* Purpose: This is for a recording script
* Intial version date: 07/03/03
* Last modification date: Thursday, May 20 2010

Note - For recording to work, DNS Server must be configured in Netstorm machine.
       This is done in file /etc/resolv.conf and should have line as follows:
       nameserver <IP Address of DNS Server>

ONLY FOR HTTP: 

Argument need to be given like this,
o URL will be a regular expression:
          nsu_monitor -n _newScript -l Port1 -s Server1 -u URL1 -i listenIp 
                                    -l Port2 -s Server2 -u URL2
                                    -l Port3 -s Server3 -u URL3
                                    -l Port1 -s Server4 -u URL4
                                    -l Port2 -s Server5 -u URL5
                                    -l Port1 -s Server6 -u URL7

How it works ?
o  We have port, server & url like this 
 
    -l Port1 -s Server1 -u URL1 
    -l Port2 -s Server2 -u URL2
    -l Port3 -s Server3 -u URL3
    -l Port1 -s Server4 -u URL4
    -l Port2 -s Server5 -u URL5
    -l Port1 -s Server6 -u URL7

o if will sort to it and will become 

    -l Port1 -s Server1 -u URL1 
    -l Port1 -s Server6 -u URL7
    -l Port1 -s Server4 -u URL4
    -l Port2 -s Server2 -u URL2
    -l Port2 -s Server5 -u URL5
    -l Port3 -s Server3 -u URL3

o Now it has only 3 port to listen Port1, Port2, Port3
  so only 3 child each on one port will be created

Whenever request comes on the listening port it find the urls in it server list
if it founds it makes the connection to that mapped server & send/recv req/resp.
E.g:

   A request has come on Port2. It will parse the URL from the request, 
   and will match with the regular expression provided by URL2 & URL5,
   if match is done with URL5 then it will communicate with the server Server5 as mapping is:
      -l Port2 -s Server5 -u URL5

   If NO match found then we will exit as we have found a request which ha no mapping.


NOTE: About regcomp : it gives error if you use only '*', following lines are from man page of regcomp
  REG_BADRPT
                    Invalid use of repetition operators such as using â as the first character.


*********************************************************************/
//TODO: if protocol is ftp etc
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <netinet/in.h> /* not needed on IRIX */
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/times.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/select.h>
#include <strings.h>
#include <string.h>
#include <string.h>
#include <ctype.h>
#include <wait.h>
#include <unistd.h>        /* not needed on IRIX */
#include <errno.h>        /* not needed on IRIX */
#include <fcntl.h>
#include <sys/types.h>
#include <regex.h>
#include <time.h>
#include <sys/prctl.h>
//#include <openssl/ssl_cav.h>

#include "nslb_ssl_lib.h"
#include "comp_decomp/nslb_comp_decomp.h"
#include "nslb_sock.h"
#include "ns_ssl.h"
//#include "IOLib.h"
#include "amf.h"
#include "monitor_utils.h"
#include "nslb_time_stamp.h"
#include "ns_data_types.h"
#include "nslb_hessian.h"
#include "nslb_util.h"

#define PAGE_THRESHOLD_TIME 60000 //500 mili-second
#define TRUE      1
#define FALSE     0
#define NAMEBUF   256   /* buffer for local hostname information */
#define HTTP      1
#define HTTPS     2
#define HEADER    1
#define BODY      2

#define BUFSIZE (1024*16)           /* size for read/write buffer   */
#define MAX_HOST_NAME 256
#define MAX_CERT_NAME 256

// Neeraj - 3.9.1 B2 - Increases size from 64 K to 1 MB as Macys XML are reching 128K or so
#define HESSIAN_MAX_BUF_SIZE (1024*1024) //max hessian buffer size
#define MAX_HEADER_SIZE (2*1024) //used in saving header strings

#define MAX_CHUNK_SIZE (10*1024*1024)  // Assumption is chunk size will be <= 10 MB. Need to see if we need to make it dynamic

#define CONTENT_OTHER       0
#define APPLICATION_AMF     1
#define APPLICATION_HESSIAN 2

#define CHAININGENABLED 1
// #define HTTPSERVERPROVIDED 1
// #define HTTPSSERVERPROVIDED 1

static char *proxy_mode_string[] = { "PROXY_MODE_RECORDER",
                                     "PROXY_MODE_CHAIN",
                                     "PROXY_MODE_WEB_SERVICE_HTTP",
                                     "PROXY_MODE_WEB_SERVICE_HTTPS"
                                   };

#define SERVICE_REQUEST_TYPE    1
#define SERVICE_RESPONSE_TYPE    2

static char *type_string[]  = { "Nothing",
                                "REQUEST",
                                "RESPONSE"
                              };

//for proxy_mode
// ?? 0 means use proxy host from header, 1 means use fixed host/port
#define PROXY_MODE_RECORDER          0 // nsu_monitor is the only proxy server
#define PROXY_MODE_CHAIN             1 // nsu_monitor and another proxy server in a chain
#define PROXY_MODE_WEB_SERVICE_HTTP  2 // Web Service - HTTP
#define PROXY_MODE_WEB_SERVICE_HTTPS 3 // Web Service - HTTPS

extern void write_all(int fd, char *buf, int size);
extern void get_cookie_names(char *buf, char *cookie);
extern void get_req_url_method(char *buf,char * method, char *url);
extern char * get_next_hdr (char *start, char *store);
extern char *get_page_from_url(char *);
extern void on_classic_mon_session_start();
extern void on_classic_mon_session_end();
extern void on_classic_mon_url_end(int first_url, int proto);
extern void on_classic_mon_url_start(int first_url);
// extern void mark_end_headers (char *buf, int type);
extern void copy_header(char *buf, int type);
extern void copy_data( char * data, int len, int type, int part);
extern void mark_if_new_page(int all_are_pages, char *ubuf);
extern void on_classic_mon_page_start();
extern void check_uniq_instance();
extern int peek_url_host(int sfd, int proxy_mode, char *host, char *ubuf, int old_proto);
extern int read_and_save_req_rep(void *from, void *to, int verbose, int type, int proto, char* url);

// Made static global variables aa these are needed by monitor sub child
static  char *url_map_msg = NULL;
static  int sfd, remote_server_fd; // Socket fd
static  char ubuf[1024*16]; //URL Name
static  void *src = NULL, *dest = NULL; // Sockets streams
static  FILE *sfp = NULL, *remote_server_fp = NULL; // File

char buf[10*1024*1024];         /* read/write data buffer        */
char buf1[10*1024*1024];        /* read/write data buffer        */


#define getline _getline

/*
   proxy.c -- a trivial proxy forwarder.
   This program "listens" at a defined port, and then forwards all
   data from this port to a port on a remote server, and also forwards
   all data  returned from the remote server to the client connecting
   at the defined port.

   All data is also copied to a cache file, so that the transactions
   can be saved and monitored.
*/

// Note that outgoing hdr and data are always captured

u_ns_ts_t last_stamp=0;
u_ns_ts_t cur_stamp=0;
u_ns_ts_t last_page_start_time=0;
int proxy_mode = PROXY_MODE_RECORDER;
int page_threshold = PAGE_THRESHOLD_TIME; // In milli-seconds

static int req_resp_id = 0; 
static int req_resp_idx = -1;

static char req_dir[1024]; //Contains req
static char resp_dir[1024]; //Contains resp
static char conf_dir[1024]; //Contains configuration

char trace_msg[4*1024];//to save debug trace msg

int global_ppid;
char wdir[1024]; //Contains dump dir
char ns_wdir[1024]; //Contains NS_WDIR 
char *scriptName=NULL;
char *g_listen_ip_addr=NULL;
char cur_page_name[64];
char last_page_name[64];
char remoteServerName[MAX_HOST_NAME + 1];
int is_first_url; //If the current url is the first url of the page
int cur_page_num=0;
FILE *capture_fp;
FILE *detail_fp;
FILE *c_fp;
FILE *h_fp;
FILE *index_fp;
FILE *hdr_fp; //contains all req hdr and response hdr for page
char *hdr_buf_out;
char *body_buf_out;
char *hdr_buf_in;
char *body_buf_in;
int max_hlen_out=0;
int cur_hlen_out=0;
int max_blen_out=0;
int cur_blen_out=0;
int max_hlen_in=0;
int cur_hlen_in=0;
int max_blen_in=0;
int cur_blen_in=0;
char cur_host_name[MAX_HOST_NAME + 1];
char lock_fname[1024];
short http_compression_req;
short http_compression_res;

short content_type_req=0;
short content_type_res=0;
int end_recording=0;
int num_embed=0;
int lock_fd = -1;
double svc_time = 0;
unsigned long req_time_in_ms = 0;

char chunk_buf[MAX_CHUNK_SIZE];// It used for reading chunk data in case of chunk Encoding
// Save requset and Response time to calculate service time
struct timeval req_time, res_time;


int recording_timeout = 900;
//FOR HTTP PROXYCHAIN
int httpchain = 0;
char httpchainserver[MAX_HOST_NAME + 1] ;
int httpchainport = 80;
//FOR HTTPS PROXYCHAIN
int httpschain = 0;
char httpschainserver[MAX_HOST_NAME + 1] ;
int httpschainport = 443;
int verbose = 0;

//FOR -S OPTION
char remoteservername_ssl[MAX_HOST_NAME + 1] ;
#define PARENT (parent_pid == getpid())

typedef struct GetOptData {
   int type;
   int local_port;
   int proxy_mode;
   char remote_server[MAX_HOST_NAME + 1];
   char host_url[BUFSIZE + 1];
   char client_cert[MAX_CERT_NAME + 1];
   char end_point_cert[MAX_CERT_NAME + 1];
} GetOptData;

typedef struct ChildData {
   int pid;
   int local_port;
   int proxy_mode;
   int num_map_host;
   char client_cert[MAX_CERT_NAME + 1];
   char end_point_cert[MAX_CERT_NAME + 1];
   char *remote_server[10];
   char *host_url[10];
   regex_t url_regex[10];
} ChildData;

// This structure keeps the pid to monitor sub child
// we need this to implement monitor child idle timeout
typedef struct
{
  pid_t sub_child_pid;
  int client_fd;
} SubChildData;

#define MAX_SUB_CHILD 1024 

static int active_sub_child = 0;
//static int last_sub_child_idx = 0;

static SubChildData sub_child_data[MAX_SUB_CHILD];

// Max 25 Host Port supported
#define MAX_HOST 25
#define MAX_URL  MAX_HOST
#define MAX_LOCAL_PORT MAX_HOST
static GetOptData getopt_data[MAX_HOST] = {0};
static ChildData child_data[MAX_HOST];
static ChildData *my_child_data;
static int new_page = 1;
static int all_are_pages = 0;
static int num_child = 0;

//For each endpoint (if more than one), we create a child. We assign running counter for each child (1, 2, 3, ..)
int my_child_id = 255;
//For each connnection, we create a child of child. We assign running counter for each sub child (1, 2, 3, ..)
int my_sub_child_id = 1;

static int mon_sickchild_pending = 0;
static int parent_started = 0;

//static int my_fgets(char *buf, int size, char *src){

//}

// This method make a file in resp dir with name service_parentId_childId_suchild_Id and copy SVC_TIME 2 svc_time to that
// file
static void create_service_time(int proto)
{
  int ser_fd = -1;
  char file_name[4096] = "\0";
  char input[1024];

  NSDL2_CLASSIC_MON("Method called. service_time = %lf", svc_time);
  
  //sprintf(file_name, "%s/service_%d_%d_%d", resp_dir, my_child_id, my_sub_child_id, req_resp_id);
  sprintf(file_name, "%s/response_%d_%d_%d", conf_dir, my_child_id, my_sub_child_id, req_resp_id);
  NSDL2_CLASSIC_MON("Opening service file [%s] for writing ...", file_name);
  ser_fd = open(file_name, O_CREAT|O_WRONLY|O_EXCL|O_CLOEXEC, 0666);
  if(ser_fd < 0) 
  {
    error_log(1, _FLN_, "Unable to open for writing request.\n", req_resp_id);
    exit(-1);
  }
  NSDL2_CLASSIC_MON("service time SVC_TIME 2 = %04d ", (int)svc_time);
  sprintf(input, "SVC_TIME 2 %04d\n", (int)svc_time);
  if(write(ser_fd, input, strlen(input)) < 0)
    error_log(1, _FLN_, "Unable to write for req_resp_idx = %d", req_resp_idx);

  if(proto == HTTPS)
    sprintf(input, "RECORDING_PARAMETERS HTTP https:%s\n", cur_host_name);
  else
    sprintf(input, "RECORDING_PARAMETERS HTTP http:%s\n", cur_host_name);
  if(write(ser_fd, input, strlen(input)) < 0) 
    error_log(1, _FLN_, "Unable to write for req_resp_idx = %d", req_resp_idx);

  //REQ_TIMESTAMP <time stamp in milli-seconds>
  NSDL2_CLASSIC_MON("req time stamp REQ_TIMESTAMP = %lu ", req_time_in_ms);
  sprintf(input, "REQ_TIMESTAMP %lu\n", req_time_in_ms);
  if(write(ser_fd, input, strlen(input)) < 0)
    error_log(1, _FLN_, "Unable to write for req_resp_idx = %d", req_resp_idx);

  close(ser_fd);  
}

static void create_request() {

  int req_fd = -1;
  char file_name[1024];
  char host_buf[1024];

  sprintf(file_name, "%s/request_%d_%d_%d", req_dir, my_child_id, my_sub_child_id, req_resp_id);

  NSDL2_CLASSIC_MON("Opening Request file [%s] for writing ...", file_name);
  req_fd = open(file_name, O_CREAT|O_WRONLY|O_EXCL|O_CLOEXEC, 0666);
  if(req_fd < 0) {
    error_log(1, _FLN_, "Unable to open for writing request.\n", req_resp_id);
    exit(-1);
  }

  NSDL2_CLASSIC_MON("cur_hlen_out = %d, hdr_buf_out = [%s]", cur_hlen_out, hdr_buf_out);
  if(write (req_fd, hdr_buf_out, cur_hlen_out) < 0)
     error_log(1, _FLN_, "Unable to write for req_resp_idx = %d", req_resp_idx);

  if((strcasestr(hdr_buf_out, "Host:")) == NULL)
  {
    sprintf(host_buf, "Host: %s\r\n", cur_host_name);
    NSDL2_CLASSIC_MON("Adding Host header in request. [%s]", host_buf);
    if(write (req_fd, host_buf, strlen(host_buf)) < 0)
       error_log(1, _FLN_, "Unable to write host heder in request file = %d", req_resp_idx);
  }

  if(write (req_fd, "\r\n", strlen("\r\n")) < 0)
     error_log(1, _FLN_, "Unable to write for req_resp_idx = %d", req_resp_idx);

  NSDL2_CLASSIC_MON("cur_blen_out = %d, body_buf_out = [%s]", cur_blen_out, body_buf_out);
  if(write (req_fd, body_buf_out, cur_blen_out) < 0)
     error_log(1, _FLN_, "Unable to write for req_resp_idx = %d", req_resp_idx);

  close(req_fd);
}
static void create_request_body(char *body_buff, int buf_len) {

  int req_fd = -1;
  char file_name[1024];
  sprintf(file_name, "%s/request_body_%d_%d_%d", req_dir, my_child_id, my_sub_child_id, req_resp_id);

  NSDL2_CLASSIC_MON("Opening Request file [%s] for writing ...", file_name);
  req_fd = open(file_name, O_CREAT|O_WRONLY|O_EXCL|O_CLOEXEC, 0666);
  if(req_fd < 0) {
    error_log(1, _FLN_, "Unable to open for writing request.\n", req_resp_id);
    exit(-1);
  }

  NSDL2_CLASSIC_MON("cur_blen_out = %d, body_buf_out = [%s]", buf_len, body_buff);
  if(write (req_fd, body_buff, buf_len) < 0)
     error_log(1, _FLN_, "Unable to write for req_resp_idx = %d", req_resp_idx);

  close(req_fd);
}

static void create_encoded_request_body(char* body_buff, int  buf_len, char *extn) {

  int req_fd = -1;
  char file_name[1024];
  sprintf(file_name, "%s/request_body_%s_%d_%d_%d", req_dir, extn, my_child_id, my_sub_child_id, req_resp_id);

  NSDL2_CLASSIC_MON("Opening Request file [%s] for writing ...", file_name);
  req_fd = open(file_name, O_CREAT|O_WRONLY|O_EXCL|O_CLOEXEC, 0666);
  if(req_fd < 0) {
    error_log(1, _FLN_, "Unable to open for writing request.\n", req_resp_id);
    exit(-1);
  }

  //NSDL2_CLASSIC_MON("body_len = %d, body_buf = [%s]", buf_len, body_buff);
  if(write (req_fd, body_buff, buf_len) < 0)
     error_log(1, _FLN_, "Unable to write for req_resp_idx = %d", req_resp_idx);

  close(req_fd);
}
// This method writes request body
static void create_response(char *body_buf, int body_len) {

  int resp_fd = -1;
  char file_name[1024];
  sprintf(file_name, "%s/response_%d_%d_%d", resp_dir, my_child_id, my_sub_child_id, req_resp_id);

  NSDL2_CLASSIC_MON("Opening Response file [%s] for writing ...", file_name);
  resp_fd = open(file_name, O_CREAT|O_WRONLY|O_EXCL|O_CLOEXEC, 0666);

  if(resp_fd < 0) {
    error_log(1, _FLN_, "Unable to open for writing response.\n", req_resp_id);
    exit(-1);
  }

  /** TODO Shalu 
 *  strstr on 'Content Length: '
 *  move index to end of 'Content Length: '
 *  now write body_len in place of 00000000
 *  e.g if body len is 25 then it should ne written like 00000025
 *
 */

  NSDL2_CLASSIC_MON("cur_hlen_in = %d, hdr_buf_in = [%s]", cur_hlen_in, hdr_buf_in);

  if(write (resp_fd, hdr_buf_in, cur_hlen_in) < 0)
     error_log(1, _FLN_, "Unable to write for req_resp_idx = %d", req_resp_idx);

  if(write (resp_fd, "\r\n", strlen("\r\n")) < 0)
     error_log(1, _FLN_, "Unable to write for req_resp_idx = %d", req_resp_idx);

  //NSDL2_CLASSIC_MON("body_len = %d, body_buf = [%s]", body_len, body_buf);
  if(write (resp_fd, body_buf, body_len) < 0)
     error_log(1, _FLN_, "Unable to write for req_resp_idx = %d", req_resp_idx);

  close(resp_fd);
}

static void create_response_body(char *body_buf, int body_len) {

  int resp_fd = -1;
  char file_name[1024];
  sprintf(file_name, "%s/response_body_%d_%d_%d", resp_dir, my_child_id, my_sub_child_id, req_resp_id);

  NSDL2_CLASSIC_MON("Opening Response body file [%s] for writing ...", file_name);
  resp_fd = open(file_name, O_CREAT|O_WRONLY|O_EXCL|O_CLOEXEC, 0666);

  if(resp_fd < 0) {
    error_log(1, _FLN_, "Unable to open for writing response.\n", req_resp_id);
    exit(-1);
  }

  //NSDL2_CLASSIC_MON("body_len = %d, body_buf = [%s]", body_len, body_buf);
  if(write (resp_fd, body_buf, body_len) < 0)
     error_log(1, _FLN_, "Unable to write for req_resp_idx = %d", req_resp_idx);

  close(resp_fd);
}

static void create_encoded_response_body(char *body_buf, int body_len, char *extn) {

  int resp_fd = -1;
  char file_name[1024];
  sprintf(file_name, "%s/response_body_%s_%d_%d_%d", resp_dir, extn, my_child_id, my_sub_child_id, req_resp_id);

  NSDL2_CLASSIC_MON("Opening Response body file [%s] for writing ...", file_name);
  resp_fd = open(file_name, O_CREAT|O_WRONLY|O_EXCL|O_CLOEXEC, 0666);

  if(resp_fd < 0) {
    error_log(1, _FLN_, "Unable to open for writing response.\n", req_resp_id);
    exit(-1);
  }

  //NSDL2_CLASSIC_MON("body_len = %d, body_buf = [%s]", body_len, body_buf);
  if(write (resp_fd, body_buf, body_len) < 0)
     error_log(1, _FLN_, "Unable to write for req_resp_idx = %d", req_resp_idx);

  close(resp_fd);
}

void kill_sub_children(void) {
  int i;

  NSDL2_CLASSIC_MON("Method Called");

  for (i = 0; i<active_sub_child; i++) {
    if (sub_child_data[i].sub_child_pid > 0) {
      kill(sub_child_data[i].sub_child_pid, SIGINT);
    }
  }
}

//For parent
void kill_children(void) {
  int i;

  NSDL2_CLASSIC_MON("Method Called");

  for (i = 0; i<num_child;i++) {
    if (child_data[i].pid > 0) {
      kill(child_data[i].pid, SIGINT);
      //waitpid(child_data[i].pid, &status, 0);
    }
  }
}

static void
handle_sickchild( int sig ) {
  pid_t pid;
  int status;

  NSDL2_CLASSIC_MON("Method Called, sig = %d");

  if (!parent_started) {
     mon_sickchild_pending = 1;
     signal (SIGCHLD, SIG_IGN);
     return;
  }

  pid = waitpid(-1, &status, WNOHANG);
  if (pid <= 0) return;

  int i;
    
  for (i = 0; i < num_child; i++) {
     if (child_data[i].pid == pid) {
       NSDL2_CLASSIC_MON("wait returned pid %d WIFEXITED(status) %d WEXITSTATUS(status) 0x%x", pid, WIFEXITED(status), WEXITSTATUS(status) );

       break;
     }
  }
  child_data[i].pid = -1;    /* should be set so parent knows all childs are gone */
}

// This is signal handler for SIGINT which is send when recording is to be stopped
static void handle_sigint(int sig) {
  if (global_ppid == getpid()){
    kill_children();
    kill_sub_children();
    exit(0);
  }
  NSDL2_CLASSIC_MON("Method Called, sig = %d");
  end_recording = 1;
}

static void handle_sigterm(int sig) {
  NSDL2_CLASSIC_MON("Method Called, sig = %d");
  end_recording = 1;
}

static void handle_sigusr1(int sig) {
  NSDL2_CLASSIC_MON("Method Called, sig = %d");
  kill_children();
}

static void sub_child_data_init()
{
  NSDL2_CLASSIC_MON("Method Called");
  memset(&sub_child_data, -1, MAX_SUB_CHILD * (sizeof(SubChildData)));
}


static void child_data_init() {

  NSDL2_CLASSIC_MON("Method Called");
  
  my_child_id = atoi(getenv("CHILD_INDEX"));
  my_sub_child_id = atoi(getenv("SUB_CHILD_INDEX")); // For child created by each child
  my_child_data = &child_data[my_child_id];

  NSDL2_CLASSIC_MON("my_child_id = %d, my_sub_child_id = %d", my_child_id, my_sub_child_id);
}

static void set_child_env(int child_index, int sub_child_index)
{
  char *env_buf;
  
  NSDL2_CLASSIC_MON("Method Called. child_index = %d, sub_child_index = %d", child_index, sub_child_index);

  // Note: Malloced buffer is adedd in the environment, so do not resue or free it
  env_buf = malloc(32);
  sprintf(env_buf, "CHILD_INDEX=%d", child_index);
  putenv(env_buf);

  env_buf = malloc(32);
  sprintf(env_buf ,"SUB_CHILD_INDEX=%d", sub_child_index);
  putenv(env_buf);

}


static void  write_mon_state_file(char *val) {
  FILE *fp;

  NSDL2_CLASSIC_MON("Method Called, val = [%s]", val);
  
  fp = fopen("/tmp/mon_state", "w");
  if(fp) {
    fwrite(val ,sizeof(char), strlen(val), fp);
    fclose(fp);
  }
}

#if 1
/* string.
* Argument:
*    preg: Precompiled pattern. 
*    buf_ptr_tmp: Pointer to the char pointer.
* Return Value: return 0 on match found and advance the pointer for next search.
* else return 1.
*/
int
my_regexec(regex_t preg, char **buf_ptr_tmp)
{
  regmatch_t pmatch[1];
  int status;
  int eflag = 0;

  NSDL2_CLASSIC_MON("Method Called");

  status = regexec(&preg, *buf_ptr_tmp, 1, pmatch, eflag);
  if(status == 0) { // Found
    *buf_ptr_tmp += pmatch[0].rm_eo;//update the buf_ptr_temp for next search
  }

  NSDL2_CLASSIC_MON("status = %d", status);
  
  return status;
}

static void
my_regcomp(regex_t *preg, short ignorecase, char *data, char *msg)
{
  int return_value;
  char err_msg[1000 + 1];

  NSDL2_CLASSIC_MON("Method Called");
  
  if (ignorecase)
    return_value = regcomp(preg, data, REG_EXTENDED|REG_ICASE);
  else
    return_value = regcomp(preg, data, REG_EXTENDED);

  if (return_value != 0) {
    regerror(return_value, preg, err_msg, 1000);
    printf("%s regcomp failed:%s\n", msg, err_msg);
    exit(-1);
  }
}
#endif

/* This function searches fo url in regualar expression list for a child
 * Returns:
 * 0 if FOUND 
 * 1 if NOT FOUND
 */

static char *search_url_in_list(char *url, char *cur_host_name) {

  int i, ret;
  static char err_msg[1024];
  NSDL2_CLASSIC_MON("Method Called, cur_host_name = [%s], url = [%s]",
                     cur_host_name, url);
  
  for(i = 0; i < my_child_data->num_map_host; i++) { 
     ret = my_regexec(my_child_data->url_regex[i], &url);
     if(ret == 0) {
       strcpy(cur_host_name, my_child_data->remote_server[i]);
       NSDL2_CLASSIC_MON("Host Mapping found for server = [%s], URL = [%s]", cur_host_name, url);
       return NULL;
     }
  }
  
  sprintf(err_msg, "Host mapping not found for URL: %s on the Port: %d", url, my_child_data->local_port); 
  error_log(0, _FLN_, err_msg); 
  return err_msg;
}

static void write_to_request_file(void *fp, int proto) {

  char buf[BUFSIZE + 1];         /* read/write data buffer        */

  NSDL2_CLASSIC_MON("Method Called");

  while(fgets(buf, BUFSIZE, (FILE*)fp)) {
     if( strcmp(buf, "\r\n") == 0 || strcmp(buf, "\r") == 0 || strcmp(buf, "\n") == 0 )
      break;
  }
}

static void copy_logs_to_scripts() {

  char cmd[2048];
  char err_msg[1024] = "\0";

  NSDL2_CLASSIC_MON("Method Called");
  
  sprintf(cmd, "mkdir -p %s/temp && cp -r %s/webapps/netstorm/logs %s/temp", wdir, ns_wdir, wdir);

  NSDL2_CLASSIC_MON("Running cmd = [%s]", cmd);
  nslb_system(cmd,1,err_msg);
}

static inline int read_request(void *src, void *dest, int verbose, int proto, char* url) {
  NSDL2_CLASSIC_MON("Method Called");
  return(read_and_save_req_rep(src, dest, verbose, 1, proto, url)); // Request
}

static inline int read_response(void *src, void *dest, int verbose, int proto) {

  NSDL2_CLASSIC_MON("Method Called");
  return(read_and_save_req_rep(dest, src, verbose, 2, proto, NULL)); // Response
}


static int keep_alive_time = 300;  // Default keep alive time for connection in seconds (5 Minutes)
// For quick testing
// static int keep_alive_time = 60;  // Default keep alive time for connection in seconds (5 Minutes)


// Returns
//  0 - Keep alive time is over or parent is dead
//  -1 - Error
//  1 - Got event on cleint fd
//  2 - Got event of server fd

int check_select(int sfd, int dfd)
{
  struct timeval temp;
  struct timeval *tvp = &temp;
  fd_set rfd;
  int count, ret;
  int max_fd;
  int select_time_out = 1; // 1 seconds

  int max_loop_count = keep_alive_time/select_time_out;
  int loop_count = 0;

  NSDL2_CLASSIC_MON("Method Called. Keep Alive time = %d seconds. max_loop_count = %d, sfd = %d, dfd = %d", keep_alive_time, max_loop_count, sfd, dfd);

  while(max_loop_count--)
  {
    loop_count++;
    NSDL2_CLASSIC_MON("Checking for new request. Loop count = %d", loop_count);

    // Check if parent is dead or not
    pid_t my_parent_pid = getppid();
    // If parent has died, then may be parent id has become 1, so check it and then kill self
    if(my_parent_pid == 1)
    {
      NSDL2_CLASSIC_MON("Parent of monitor sub child is dead (parent pid is 1). So stopping monitor sub child also");
      return 0;
    }

    kill(0, my_parent_pid);
    if(errno == ESRCH)
    {
      NSDL2_CLASSIC_MON("Parent of monitor sub child is dead. So stopping monitor sub child also");
      return 0;
    }

  max_fd = -1;
  FD_ZERO(&rfd);

  // Set cleint fd
  FD_SET(sfd, &rfd);
  if(sfd > max_fd)
    max_fd = sfd;

  // Set serive end point fd
  FD_SET(dfd, &rfd);
  if(dfd > max_fd)
    max_fd = dfd;


  temp.tv_sec = select_time_out;
  temp.tv_usec = 0;

    NSDL2_CLASSIC_MON("Waiting for any event for %d seconds on sfd = %d and dfd = %d", temp.tv_sec, sfd, dfd);

  count = select(max_fd + 1, &rfd, NULL, NULL, tvp);
  if(count == 0) // can only get timeout if this is set
  { 
    NSDL2_CLASSIC_MON("Timeout in select. Continuing");
    continue;
  }

  if(count < 0) // error
  {    
    if(errno == EINTR)
    {
      // This can happen on getting some singal. Should not come as we are not getting any signal for sub child
      NSDL2_CLASSIC_MON("Got some signal in select. Continuing");
      continue;
    }
    else // This should not happen
    {
      error_log(0, _FLN_, "Error in select. errno = %d, Error string = %s", errno, strerror(errno));
      return -1;
    }
  }

  // Check if event came on client fd
  ret = FD_ISSET(sfd, &rfd);
  if(ret)
  {
    NSDL2_CLASSIC_MON("Got event on client fd. sfd = %d", sfd);
    return 1;
  }

  // Check if event came on client fd
  ret = FD_ISSET(dfd, &rfd);
  if(ret)
  {
    NSDL2_CLASSIC_MON("Got event on server fd. dfd = %d", sfd);
    return 2;
  }

  // Code should never come here
  error_log(0, _FLN_, "Error: Got event on neither client nor server fd");
  return -1;
  }

  NSDL2_CLASSIC_MON("Keep alive time is over");
  return 0;

}


static int make_server_connection (int proto, char *url_map_msg)
{
  //Time out value for connect and SSL_connect.
  int timeout_val = 60;
  char err_msg[2048];

  NSDL2_CLASSIC_MON("Method Called");

  if(httpchain == CHAININGENABLED && proto == HTTP) {
    NSDL2_CLASSIC_MON("HTTP CHAIN PROXY = %s PORT = %d",httpchainserver, httpchainport);
    remote_server_fd = nslb_tcp_client_ex(httpchainserver, httpchainport, timeout_val, err_msg);
    if(remote_server_fd >= 0)
      sprintf(trace_msg, "Connection made to endpoint '%s' at port %d\n", httpchainserver, httpchainport);
  } else if(httpschain == CHAININGENABLED && proto == HTTPS) {
    NSDL2_CLASSIC_MON("SSL:HTTPS CHAIN PROXY = %s PORT = %d",httpschainserver, httpschainport);
    remote_server_fd = nslb_tcp_client_ex(httpschainserver, httpschainport, timeout_val, err_msg);
    proxy_chain(remote_server_fd, cur_host_name);
    if(remote_server_fd >= 0)
      sprintf(trace_msg, "Connection made to endpoint '%s' at port %d\n", httpschainserver, httpschainport);
  } else if(!url_map_msg) {
    NSDL2_CLASSIC_MON("Connection to recorder, cur_host_name = %s\n", cur_host_name);
    remote_server_fd = nslb_tcp_client_ex(cur_host_name, proto==HTTP?80:443, timeout_val, err_msg);
    if(remote_server_fd >= 0)
      sprintf(trace_msg, "Connection made to service endpoint at IP '%s'\n", cur_host_name);
  }

  NSDL2_CLASSIC_MON("url_map_msg = [%s], remote_server_fd = %d", url_map_msg, remote_server_fd ); 
  if (!url_map_msg && remote_server_fd < 0) {
    error_log(0, _FLN_, "Proxy server unable to create socket for server connection, %s\n", err_msg);
    return(-1);
  }
  trace_log(trace_msg);
  return (remote_server_fd);
}

static int monitor_child_idle_time = 3600;

static int wait_and_accept_con_from_client(int listen_fd, int *new_fd, char** url_map_msg)
{
  fd_set rfd;
  struct sockaddr_in their_addr;
  socklen_t sin_size = sizeof(their_addr);
  int count =0;
  struct timeval temp;
  int count_timeout = 0;
  int select_time_out = page_threshold/1000;
  int max_child_loop_count = monitor_child_idle_time/select_time_out;

  NSDL2_CLASSIC_MON("Method called. listen_fd = %d, max_child_loop_counti = %d", listen_fd, max_child_loop_count);

  while (max_child_loop_count--) {
    temp.tv_sec = select_time_out;
    temp.tv_usec = (page_threshold%1000)*1000;
    FD_ZERO(&rfd);
    FD_SET(listen_fd, &rfd);

    NSDL2_CLASSIC_MON("Waiting for connection from the client");

    count = select(listen_fd+1, &rfd, NULL, NULL, &temp);

    if (count < 0) {
                        if(errno == EINTR)
                          NSDL2_CLASSIC_MON("Got interrupt in select. May be recording is stopped or select timed out");
                        else
        error_log(0, _FLN_, "Error in select, errno = %d\n", errno);
      break;
    } else if (count == 0) {
      NSDL2_CLASSIC_MON("timeout: %d", count_timeout);
      count_timeout++;
      if (!new_page) {
        NSDL2_CLASSIC_MON("******************START NEW PAGE*********************");
        new_page = 1;
        write_mon_state_file("1\n");
      }
    } else {
      *url_map_msg = NULL;
      new_page = 0;
      write_mon_state_file("0\n");
      *new_fd = accept(listen_fd, (struct sockaddr *)&their_addr, &sin_size);
      NSDL2_CLASSIC_MON("Got connection from [%s:%d]",
          inet_ntoa(their_addr.sin_addr),
          ntohs(their_addr.sin_port));
      break;
    }
  }
  return(count);
}

static void send_404(void *src)
{
  // If url map msg is there means we got an ERROR so print req & resp in files
  char tmp_resp[2048];
  
  NSDL2_CLASSIC_MON("Method Called");
  int err_size = strlen(url_map_msg) + 149; // 149 is the size of static body string here
  sprintf(tmp_resp, "HTTP/1.1 404 Not Found\r\n"
    "Content-Type: text/html;charset=ISO-8859-1\r\n"
    "Content-Language: en-US\r\n"
    "Content-Length: %d\r\n"
    "Server: NetOcean-HP/1.1\r\n\r\n"
    "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\n"
    "<html><head>\n<title>404 Not Found</title>\n"
    "</head><body>\n<h1>Not Found</h1>\n"
    "<p>%s</p>\n</body></html>\n",
     err_size, url_map_msg);

  write_to_request_file(src, 1);  // Request
  // Writing a dummy response to the client due to
  // mapping failure we did not made the connection to the actual server
  fwrite(tmp_resp , sizeof(char), strlen(tmp_resp), src);
}



void mon_close_src_dst(int proto)
{

  NSDL2_CLASSIC_MON("Method Called. src = %p, dest = %p, sfd = %d, dfd = %d", src, dest, sfd, remote_server_fd);

  if(src != NULL)
  {
    NSDL2_CLASSIC_MON("Closing source socket stream");
    if(proto == HTTP)
    {
      if(fclose((FILE *)src) != 0)
        error_log(0, _FLN_, "Error in closing source socket stream. Errno = %s", strerror(errno));
    }
    else
    {
      SSL_shutdown((SSL*) src);
      SSL_free((SSL*) src);
    }
  }

  if(dest != NULL)
  {
    NSDL2_CLASSIC_MON("Closing server endpoint socket stream");
    if(proto == HTTP)
    {
      NSDL2_CLASSIC_MON("mon_src_dest : address of dest= %p \n", &dest);
      if(fclose((FILE *)dest) != 0)
        error_log(0, _FLN_, "Error in closing endpoint socket stream. Errno = %s", strerror(errno));
    }
    else
    {
      SSL_shutdown((SSL*)dest);
      SSL_free((SSL*) dest);
    }
  }

  if(sfd > 0)
  {
    NSDL2_CLASSIC_MON("Closing client socket");
    if(close(sfd) < 0)
      error_log(0, _FLN_, "Error in closing client socket (sfd = %d). Errno = %s", sfd, strerror(errno));
      
  }
   
  if(remote_server_fd >0)
  {
    NSDL2_CLASSIC_MON("Closing server endpoint socket");
    if(close(remote_server_fd) < 0)
      error_log(0, _FLN_, "Error in closing server endpoint socket (dfd = %d). Errno = %s", remote_server_fd, strerror(errno));
  }
}

static void init_src_dest()
{
  NSDL2_CLASSIC_MON("Method Called");

  // sfd = remote_server_fd = -1;
  remote_server_fd = -1;
  src = dest = sfp = remote_server_fp = NULL;
}

static int setup_src_dest()
{
  int proto;

  NSDL2_CLASSIC_MON("Method Called. Child Data Proxy mode = [%s]", proxy_mode_string[my_child_data->proxy_mode]);
  // first_time = 0;

  init_src_dest();

  if(my_child_data->proxy_mode == PROXY_MODE_WEB_SERVICE_HTTPS) {
    proto = HTTPS;
    //This should be set on basis of coming request.
    strcpy(cur_host_name, my_child_data->remote_server[0]);
    NSDL2_CLASSIC_MON("cur host name = %s", cur_host_name);
  } else {
    //In case of http we will get host name from request.
    proto = peek_url_host(sfd, my_child_data->proxy_mode, cur_host_name, ubuf, 1);
    if(proto == -1){
      return -1;
    }
  }

  NSDL2_CLASSIC_MON("cur_host_name = [%s], ubuf = [%s], Protocol = %d, sfd = %d", cur_host_name, ubuf, proto, sfd);

  if(proto == HTTPS) {
    NSDL2_CLASSIC_MON("Calling ssl_setup. end_point_cert = %s", my_child_data->end_point_cert);
    src = ssl_setup (sfd, cur_host_name, ubuf, my_child_data->proxy_mode, my_sub_child_id, my_child_data->end_point_cert);
    if (!src)  { 
      NSDL2_CLASSIC_MON("returning -1 as src is NULL");
      return -1;
    }
  }

  //mark if a new page is starting
  mark_if_new_page(all_are_pages, ubuf);
  //Do page start activities, if page started
  if (is_first_url) {
    /* Earliar we were copying host to cur_host_name in peek_url_host,
    * but now as we have to select host on the basis of URL so doing here*/ 
    if (my_child_data->proxy_mode != PROXY_MODE_RECORDER && my_child_data->proxy_mode != PROXY_MODE_CHAIN && proto == HTTP)   {
      url_map_msg = search_url_in_list(ubuf, cur_host_name);
    }
    on_classic_mon_page_start();
  }

  /* Preceding line sets up the program to accept data from the remote browser.
     We now need to create a new socket that will connect to the remote server.
  */
  NSDL2_CLASSIC_MON("current host name for remote server connection = %s",cur_host_name );
  if( (remote_server_fd = make_server_connection(proto, url_map_msg)) == -1) {
    error_log(0, _FLN_, "remote_server_fd = -1");
    return -1;
  } 

  NSDL2_CLASSIC_MON("Proto = %d", proto);
  if(proto == HTTP ) {
    NSDL2_CLASSIC_MON("HTTP: Creating FILE * from fd");
    if((sfp = fdopen(sfd, "r+")) == NULL) {
      error_log(0, _FLN_, "can't create file pointer for reading from browser\n");
      return -1;
    }
    if(!url_map_msg) {
      if((remote_server_fp = fdopen(remote_server_fd, "r+")) == NULL)
      {
        error_log(0, _FLN_, "can't create file pointer for writing to server.\n");
        return -1;
      }
      dest = remote_server_fp;
    }
    src = sfp;
  } else {
    NSDL2_CLASSIC_MON("HTTPS: Calling ssl_set_remote, client_cert = %s", my_child_data->client_cert);
    dest = ssl_set_clients(remote_server_fd, cur_host_name, my_child_data->client_cert);
  }
  return proto;
}

static int process_request(int proto)
{

  // Not read request from client and send to server and then read response from sever and send to client

  NSDL2_CLASSIC_MON("Method Called");
  on_classic_mon_url_start(is_first_url);

  // read data sent by browser; forward it to the server

  req_resp_id++;
  NSDL2_CLASSIC_MON("Start data from Client:");

  if(!url_map_msg) {
    /*Here it will read whole request*/
    NSDL2_CLASSIC_MON("Going to process request. src = %p, dest = %p", src, dest);
    if(read_request(src, dest, verbose, proto, ubuf) < 0)
      return -1;

    NSDL2_CLASSIC_MON("Request Processed (Timestamp = %u)", get_ms_stamp());

    /*Here it will read whole Response*/
    NSDL2_CLASSIC_MON("Going to process response. src = %p, dest = %p", src, dest);
    if(read_response(src, dest, verbose, proto) < 0)
      return -1;
    svc_time = 0;
    req_time_in_ms = 0;

    svc_time =  ((res_time.tv_usec + (res_time.tv_sec * 1000000)) - (req_time.tv_usec + (req_time.tv_sec *1000000)))/1000;
  
    NSDL2_CLASSIC_MON("tv_usec %ld tv_sec %ld\n", req_time.tv_usec, req_time.tv_sec);    
    req_time_in_ms = (((long)req_time.tv_usec / 1000) + ((long)req_time.tv_sec * 1000));
    NSDL2_CLASSIC_MON("req_time_in_ms =  %lu \n", req_time_in_ms);

    if(svc_time < 0 || svc_time > 2147483647){
      svc_time = 0;
      NSDL2_CLASSIC_MON("svc_time is out of bound set to 0");
      error_log(0, _FLN_, "svc_time is out of bound , set to 0\n");
    }
    NSDL2_CLASSIC_MON("*********svc_time = %lf***********", svc_time);
    create_service_time(proto);
     
    if (proto == HTTP ) { 
      fflush(src);
    }
    NSDL2_CLASSIC_MON("Response Processed (Timestamp = %u)", get_ms_stamp());

  } else {
      // If url map msg is there means we got an ERROR so print req & resp in files
      send_404(src);
  }

  last_stamp = get_ms_stamp();
  NSDL2_CLASSIC_MON("Response Processed (Timestamp = %u)", last_stamp);
    
  // URL map msg if filled with error it means Remote connection is not done
  if(!url_map_msg) {
      on_classic_mon_url_end(is_first_url, proto);
  } else {
    return -1;
  }

  return 0;
}


static void
handle_sickchild_sub_child( int sig ) 
{
  int status;
  pid_t pid;
  NSDL2_CLASSIC_MON("Method Called.");
  // wait for pid
  pid = waitpid(-1, &status, WNOHANG);
  if(pid < 0)
  {
    NSDL2_CLASSIC_MON("Error occured in waitpid, ignoring sigchild singnal");
    active_sub_child--;
    return;
  }
  // Set pid in sub_child_data, as we can reuse it
  int i;
  for(i =0; i < MAX_SUB_CHILD; i++){ 
    if(sub_child_data[i].sub_child_pid == pid){
      NSDL2_CLASSIC_MON("wait returned pid %d WIFEXITED(status) %d WEXITSTATUS(status) 0x%x", pid, WIFEXITED(status), WEXITSTATUS(status) );
      sub_child_data[i].sub_child_pid = -1;
      //close connection between client(browser) to proxy.
      close(sub_child_data[i].client_fd);
      sub_child_data[i].client_fd = -1;
      break;
    }
  }
  // As child is finished, so decrease active_sub_child by one
  active_sub_child--; 
}


// This mthod close connections to old destination, handles close connection for both HTTP and HTTPS 
// and setup connection with new detination
int set_new_dest(int proto){

  NSDL2_CLASSIC_MON("Method called");

  /********Close old detinatopn pointer*********/
  if(dest != NULL)
  {
    NSDL2_CLASSIC_MON("Closing server endpoint socket stream");
    if(proto == HTTP)
    {
      NSDL2_CLASSIC_MON("in set_new_dest : address of dest= %p \n", &dest);
      if(fclose((FILE *)dest) != 0)
      error_log(0, _FLN_, "Error in closing endpoint socket stream. Errno = %s", strerror(errno));
    }
    else
    {
      SSL_shutdown((SSL*)dest);
      SSL_free((SSL*) dest);
    }
    dest = NULL ;
  }
   
  if(remote_server_fd >0)
  {
    NSDL2_CLASSIC_MON("Closing server endpoint socket");
    if(close(remote_server_fd) < 0)
      error_log(0, _FLN_, "Error in closing server endpoint socket (dfd = %d). Errno = %s", remote_server_fd, strerror(errno));
  }
  /********Close old detinatopn fd*********/

  // mark if a new page is starting
  mark_if_new_page(all_are_pages, ubuf);

  // Do page start activities, if page started
  if (is_first_url) {
    /* Earliar we were copying host to cur_host_name in peek_url_host,
    * but now as we have to select host on the basis of URL so doing here*/ 
    if (my_child_data->proxy_mode != PROXY_MODE_RECORDER && my_child_data->proxy_mode != PROXY_MODE_CHAIN && proto == HTTP)   {
      url_map_msg = search_url_in_list(ubuf, cur_host_name);
    }
    on_classic_mon_page_start();
  }
  on_classic_mon_url_start(is_first_url);

  /* Preceding line sets up the program to accept data from the remote browser.
     We now need to create a new socket that will connect to the remote server.
  */
  NSDL2_CLASSIC_MON("current host name for remote server connection = %s",cur_host_name );
  if( (remote_server_fd = make_server_connection(proto, url_map_msg)) == -1) {
    error_log(0, _FLN_, "remote_server_fd = -1");
    return -1;
  } 
  if(proto == HTTP ) {
    if(!url_map_msg) {
      if((remote_server_fp = fdopen(remote_server_fd, "r+")) == NULL)
      {
        error_log(0, _FLN_, "can't create file pointer for writing to server.\n");
        return -1;
      }
      dest = remote_server_fp;
    }
  } else {
    NSDL2_CLASSIC_MON("HTTPS: Calling ssl_set_remote");
    dest = ssl_set_clients(remote_server_fd, cur_host_name, my_child_data->client_cert);
  }
  return 0;
}

static int monitor_sub_child()
{
int proto;
int req_count = 1;

  NSDL2_CLASSIC_MON("Method Called");

  child_data_init();
  //sub_child_data_init();

  prctl(PR_SET_PDEATHSIG, SIGKILL);
  (void) signal( SIGTERM, handle_sigterm );
  (void) signal( SIGINT, handle_sigint);

  // First time, we need to make client and server connnections and open streams
  if((proto = setup_src_dest()) < 0)
    goto monitor_sub_child_end;

  while(1)
  {

    if(process_request(proto) < 0)
      break;
    
    if(end_recording) 
      break;

    NSDL2_CLASSIC_MON("Check for Connection to be reused or not");

    int ret = check_select(sfd, remote_server_fd);
    if(ret <= 0) { // Timeout or end recording
      NSDL2_CLASSIC_MON("Timeout or end recoding is done");
      break;
    } 

    if(ret == 2) // Got some event from server side. MUST be close connection only 
    {
      NSDL2_CLASSIC_MON("Server has closed connection. So stopping this child and closing all connections");
      break;
    }

    // Get event on client fd. Check if new request came or connection closed be client
    char new_host[MAX_HOST_NAME + 1] = "";
    //ret = recv(sfd, buf, BUFSIZE, MSG_PEEK);

    // Calling peek_url_host here to check the host of new url as in case of native app, we can get diffrent hosts at same connection because
    // native app is treating us as a proxy and it can send diffrent hosts at same connection. In this case we will break the connection to 
    // current destination and setup a new destination as per new url host. Bug Id : 

    ret = peek_url_host(sfd, my_child_data->proxy_mode, new_host, ubuf, proto); 
    if( ret <= 0) // Error or connection closed by client
    {
      if(ret < 0) // Error
        NSDL2_CLASSIC_MON("MSG_PEEK returned error client socket. Assuming connection is closed by client. Error = %s", strerror(errno));
      else if (ret == 0) //we're assuming connection was closed   by client
        NSDL2_CLASSIC_MON("MSG_PEEK returned 0 on client socket. Assuming connection is closed by client");
      break;
    } else if (ret == 3) {
      NSDL2_CLASSIC_MON("It seems that client wants to continue on same https con");
      continue;
    }

    if(new_host[0] && strcmp(cur_host_name, new_host)) {
      NSDL2_CLASSIC_MON("Diffrent host found on same connection. Going to break connection from current host");
      strcpy(cur_host_name, new_host);
      ret = set_new_dest(proto);
      if (ret == -1)
        goto monitor_sub_child_end;
    }
   
    req_count++;
    NSDL2_CLASSIC_MON("New request has come on the same connection. req_count  = %d", req_count);

  }

monitor_sub_child_end:
  mon_close_src_dst(proto); 

  NSDL2_CLASSIC_MON("monitor_sub_child: exiting");
  exit(0);
  //kill(my_child_data->pid, SIGINT); //Can not call this because it will stop recording.
  //exit(0);  Currently we need not to exit here.
}


static void monitor_child() {

  int lfd, count;
  int parent_pid = getpid();
  char err_msg[1024];

  NSDL2_CLASSIC_MON("Method Called");

  // If any of it's sub child die, we do not need to do any thing.
  // Same as Sig Ignore.
  (void) signal( SIGCHLD, handle_sickchild_sub_child);

  open_error_log();
  open_trace_log();
  child_data_init();
  sub_child_data_init();
  
  //printf("monitor_child: sleeping for 30 secs\n");
  //sleep(30);
  NSDL2_CLASSIC_MON("remote_server = %s, port = %d, num_child = %d", 
                     my_child_data->remote_server[0], my_child_data->local_port, num_child);

  //1 - Green light in GUI
  //0 - Red light in GUI
  if(!my_child_id) {
    write_mon_state_file("1\n");
  }

  NSDL2_CLASSIC_MON("Start: Recording, Script Name: %s, Local port: %d",
                     scriptName, my_child_data->local_port);

  if (my_child_data->proxy_mode) {
    NSDL2_CLASSIC_MON("Remote server: [%s]", my_child_data->remote_server[0]);
  }

  /*
  A).Create and bind a server socket that will connect to the remote client
     (browser) (essentially, a local server socket). We use accept() to create
     a file descriptor that references this remote resource--and redo
     the accept every time we start a new client-server transaction.
  */

  (void) signal( SIGTERM, handle_sigterm );
  (void) signal( SIGINT, handle_sigint);


  // Calling tcp_listen_ex here so that if ip is given by user, it can listen on given ip
  
  // tcp_listen exits on error. How to get it's error in monitor error log
  lfd = nslb_tcp_listen_ex(my_child_data->local_port, 128, g_listen_ip_addr, err_msg);
  if(lfd < 0)
  {
    if(lfd == -2)
    {
      if(g_listen_ip_addr){
        error_log(0, _FLN_, "Recorder could not be started for %s:%d. This IP:port may already be in use by another controller or recorder. Please check with your admin for IP/port conflicts.", g_listen_ip_addr, my_child_data->local_port);
      }
      else
        error_log(0, _FLN_, "Recorder could not be started for Port %d. This Port may already be in use by another controller or recorder. Please check with your admin for Port conflicts.", my_child_data->local_port);
    }
    else 
      error_log(0, _FLN_, "%s", err_msg);
    exit(-1);
  } 
   

  NSDL2_CLASSIC_MON("Started Listen fd = %d with queue length = %d", lfd, 128);

  /*
  * B).  Ok, now create a socket structure for communicating with the
  *      remote server. This is, essentially, a local client  socket.
  *      This will need to open and close for each transaction.
  */
  if (my_child_data->proxy_mode) {
    NSDL2_CLASSIC_MON("Proxy connection to host [%s].", my_child_data->remote_server[0]);
  }

  NSDL2_CLASSIC_MON("Proxy server on port [%d].", my_child_data->local_port);

/*
   Now loop infinitelly, opening to accept messages from browser, and then
   creating an outgoing connection to the remote server.
*/

  on_classic_mon_session_start(scriptName);
  NSDL2_CLASSIC_MON("Entering in while loop ...");
  while (1) {

    if ( (count = wait_and_accept_con_from_client (lfd, &sfd, &url_map_msg)) == -1) {
      if (end_recording) {
        NSDL2_CLASSIC_MON("End Recording");
        break; 
      }
      continue;

    } else if (count == 0) {
      if(active_sub_child == 0){
        NSDL2_CLASSIC_MON("No subchild is active and idle time for the child is over, so stopping child");
        error_log(0, _FLN_, "No subchild is active and idle time for the child is over, so stopping child");
        break;
      }
      NSDL2_CLASSIC_MON("Not stopping child because %d sub childs are active", active_sub_child);
      continue;
    }

    NSDL2_CLASSIC_MON("sfd %d",sfd);

    if (sfd < 0) {
      error_log(0, _FLN_, "client accept failed");
      continue;
    }

    sprintf(trace_msg, "Connection accepted on recorder port %d for URL pattern [%s]\n", my_child_data->local_port, my_child_data->host_url[0]);
    trace_log(trace_msg); 

    // Fork sub child to handle req/response on the accepted connection
 
    NSDL1_CLASSIC_MON("Starting monitor sub child with child_id = %d and sub_child_id = %d", my_child_id, my_sub_child_id);
    set_child_env(my_child_id, my_sub_child_id);

    my_sub_child_id++;

    int child_pid; 
    if ((child_pid = fork()) < 0) {
      error_log(0, _FLN_, "Error: Failed to create sub child process.  Aborting...");
      kill_children();
      exit(1);
    }

    // Fill sub child pid in sub_child_data 
    int id;
    if(child_pid > 0)
    {
      for(id = 0; id < MAX_SUB_CHILD; id++)
      { 
        if(sub_child_data[id].sub_child_pid == -1) {
          sub_child_data[id].sub_child_pid = child_pid;
          //this is too have track of current client fd.
          sub_child_data[id].client_fd = sfd; 
          break;
        }
      }
      active_sub_child++;
      //last_sub_child_idx = id;
    }
    // TODO - How to keep track of child for killing. 
    // Currently sub child will die on keep alive timeout and parent is ignoring SIGCHILD signal from sub child
    // If Parent is still running, then these processes will become defunct and will be gone only when parent is dead
    

    if (!(PARENT))
    {
      NSDL1_CLASSIC_MON("Monitor sub child is starting");
      monitor_sub_child();
    }
    else
    {
      NSDL1_CLASSIC_MON("This is parent path");
    }
    
    if (end_recording)
      break;
  }

  on_classic_mon_session_end();
  NSDL2_CLASSIC_MON("End: Recording.");

  if(num_child == 1) { //means it is working as parent
    copy_logs_to_scripts(); 
  }

  NSDL2_CLASSIC_MON("monitor_child: exiting");
 
  exit(0);
}

static void create_script_dir (char *sname) {

  int len, i;
  //int pid;
  char file_name[1024];
  char dump_dir[1024];
  char script_name[1024];

  NSDL2_CLASSIC_MON("Method Called, script name = [%s]", sname);

  /*Validate script name*/
  len = strlen(sname);
  if (len > 31)
  {
    error_log(0, _FLN_, "Script Name should be limited to 31 characters\n");
    exit (1);
  }

  for (i=0; i<len;i++)
  {
    if (!(isalnum((sname[i])) || (sname[i] == '_')))
    {
      error_log(0, _FLN_, "Script Name Can only have alphanumeric characters or _, in its name");
      exit (1);
    }
  }

  sprintf(script_name, "default/default/%s/%s", "scripts", sname); 

  setenv("NS_SCRIPT_NAME", script_name, 1);
  /*Create Script Dir*/
  if (getenv("NS_WDIR") != NULL)
    strcpy(ns_wdir, getenv("NS_WDIR"));
  else
    strcpy(ns_wdir, ".");
  /*bug id: 101320: using ns_ta_dir instead of g_wdir, avoid using hardcoded scripts dir*/
  /*set ns_ta_dir*/
  nslb_set_ta_dir(ns_wdir);
  sprintf(wdir, "%s/%s", GET_NS_TA_DIR(), script_name);

  check_uniq_instance();
  if (!strcmp (script_name, "default/default/scripts/_newScript")) {
    sprintf (file_name, "rm -rf %s", wdir);
    NSDL2_CLASSIC_MON("Deleting %s", file_name);
    // Syatem Command can cause of SIGCHLD
    (void) signal (SIGCHLD, SIG_IGN);
    system(file_name);
    (void) signal( SIGCHLD, handle_sickchild );
  }

  NSDL2_CLASSIC_MON("Creating %s", wdir);

  if (mkdir(wdir, 0775)) {
    error_log(0, _FLN_, "Can not create Script with name '%s': %s\n", script_name, strerror(errno));
    unlink(lock_fname);
    exit (1);
  }

  // this will contain all the request files
  sprintf(req_dir, "%s/request", wdir);
  if (mkdir(req_dir, 0777)) {
    error_log(0, _FLN_, "Can not create Script requests direcetory: %s", strerror(errno));
    unlink(lock_fname);
    exit (1);
  }

  // this will contain all the response files
  sprintf(resp_dir, "%s/response", wdir);
  if (mkdir(resp_dir, 0777))
  {
    error_log(0, _FLN_, "Can not create Script responses direcetory: %s", strerror(errno));
    unlink(lock_fname);
    exit (1);
  }

  // this will contain all the configuration files
  sprintf(conf_dir, "%s/conf", wdir);
  if (mkdir(conf_dir, 0777))
  {
    error_log(0, _FLN_, "Can not create Script configuration direcetory: %s", strerror(errno));
    unlink(lock_fname);
    exit (1);
  }

  //create dump dir for keeping page dumps
  sprintf(dump_dir, "%s/dump", wdir);
  //strcat(wdir, "/dump");
  if (mkdir(dump_dir, 0777))
  {
    error_log(0, _FLN_, "Can not create Script dump direcetory: %s", strerror(errno));
    unlink(lock_fname);
    exit (1);
  }
  sprintf(file_name, "%s/index", dump_dir);
  if ((index_fp = fopen(file_name, "w")) == NULL)
  {
    error_log(0, _FLN_, "Error in opening the dump index file %s\n", file_name);
    unlink(lock_fname);
    exit (1);
  }

  //create headers dir for keeping page dumps
  sprintf(file_name, "%s/headers", dump_dir);
  if (mkdir(file_name, 0777))
  {
    error_log(0, _FLN_, "Can not create Script headers direcetory: %s", strerror(errno));
    unlink(lock_fname);
    exit (1);
  }

  NSDL2_CLASSIC_MON("Exitting.");
}

/* This function puts the .* url at the End*/
/* Done because for a port it may possible user has given
 * multiple url reagular expression: Server mapping. 
 * So it may possible user has given .* as a first regular expression,
 * as has given more regular expression.
 * But we searches sequentialy. hence in this case all url request will
 * lie in only first url regular expression, thats why we are putting it at the END
 */
static int condition_host_url(const void *x, const void *y) {
  NSDL2_CLASSIC_MON("Method Called");
  return(!strcmp(((GetOptData *)x)->host_url, ".*") ? 0:1);
}

/* This function sorts the port */ 
static int condition_local_port(const void *x, const void *y) {
  NSDL2_CLASSIC_MON("Method Called");
  return((((GetOptData *)x)->local_port < ((GetOptData *)y)->local_port) ? 0:1);
}

static void log_getopt_data(int n) {
  int i;
  NSDL2_CLASSIC_MON("Method Called");

  for(i = 0; i < n; i++) {
     NSDL2_CLASSIC_MON("Port = %d, Host = [%-20s], Url = [%s]",
                        getopt_data[i].local_port,
                        getopt_data[i].remote_server,
                        getopt_data[i].host_url);
  }
}

static void log_child_data(int n) {
  int i, j;

  NSDL2_CLASSIC_MON("Method Called, num_child = %d", n);

  for(i = 0; i < n; i++) {
     NSDL2_CLASSIC_MON("Port (%d) = %d", i, child_data[i].local_port);
     for(j = 0; j < child_data[i].num_map_host; j++) {
       NSDL2_CLASSIC_MON("Host: %-20s Url: %s",
                          child_data[i].remote_server[j], 
                          child_data[i].host_url[j]); 
     }
  }
}

/*
 * Port: 1100 Host: S2                   Url: URL2
 * Port: 2000 Host: S1                   Url: URL1
 * Port: 3000 Host: S3                   Url: URL3
 */
static void create_data_table(int count) {

  int getopt_idx = 0, child_data_idx = 0;

  NSDL2_CLASSIC_MON("Method Called, count = %d", count);

  memset(&child_data, 0, MAX_HOST * sizeof(ChildData));

  /* Assigining for 0th index*/
  num_child = 1;
  child_data[child_data_idx].num_map_host = 1; 
  child_data[child_data_idx].local_port = getopt_data[getopt_idx].local_port;
  child_data[child_data_idx].proxy_mode = getopt_data[getopt_idx].proxy_mode;
  if(*getopt_data[getopt_idx].client_cert)
    strcpy(child_data[child_data_idx].client_cert, getopt_data[getopt_idx].client_cert);
  if(*getopt_data[getopt_idx].end_point_cert)
    strcpy(child_data[child_data_idx].end_point_cert, getopt_data[getopt_idx].end_point_cert);
  child_data[child_data_idx].remote_server[child_data[child_data_idx].num_map_host - 1 ] = getopt_data[getopt_idx].remote_server;
  child_data[child_data_idx].host_url[child_data[child_data_idx].num_map_host - 1 ] = getopt_data[getopt_idx].host_url;

  // compile urlregex
  my_regcomp(&child_data[child_data_idx].url_regex[child_data[child_data_idx].num_map_host - 1 ],
             0, 
             child_data[child_data_idx].host_url[child_data[child_data_idx].num_map_host - 1 ],
             "Searching for url");

  for(getopt_idx = 1; getopt_idx < count; getopt_idx++) {
    NSDL2_CLASSIC_MON("num_map_host = %d", child_data[child_data_idx].num_map_host);
    if(getopt_data[getopt_idx].local_port == getopt_data[getopt_idx - 1].local_port) {
       (child_data[child_data_idx].num_map_host)++; 
    } else {
       num_child++;
       child_data_idx++;
       child_data[child_data_idx].num_map_host = 1; 
       child_data[child_data_idx].local_port = getopt_data[getopt_idx].local_port;
       child_data[child_data_idx].proxy_mode = getopt_data[getopt_idx].proxy_mode;
    }
    child_data[child_data_idx].remote_server[child_data[child_data_idx].num_map_host - 1] = getopt_data[getopt_idx].remote_server;
    child_data[child_data_idx].host_url[child_data[child_data_idx].num_map_host - 1 ] = getopt_data[getopt_idx].host_url;
    my_regcomp(&child_data[child_data_idx].url_regex[child_data[child_data_idx].num_map_host - 1 ],
               0, 
               child_data[child_data_idx].host_url[child_data[child_data_idx].num_map_host - 1 ],
               "Searching for url");
  }

  log_child_data(num_child);
}

/* It sort the given input with port*/
static void sort_getopt_data(int count) {

  NSDL2_CLASSIC_MON("Method Called, count = %d, Data before sorting ...", count);
  log_getopt_data(count);
  // this will put .* at the end
  qsort(&getopt_data, count, sizeof(GetOptData), condition_host_url);
  // this will sort using port
  qsort(&getopt_data, count, sizeof(GetOptData), condition_local_port);
  NSDL2_CLASSIC_MON("Data after sorting");
  log_getopt_data(count);
  create_data_table(count);
}

int main(int argc, char *argv[]) {

  int num_local_port = 0; // to count number of arg given with -l option i.e port
  int num_host_port = 0;  // to count number of arg given with -S option i.e server
  int num_url = 0;        // to count number of arg given with -u option i.e regex for URL
  extern char *optarg;
  int errflg=0;
  int c;
  char *httpchaintemp;
  int parent_pid = global_ppid = getpid();
  int i, child_pid;
  
  verbose = 1;
  NSDL2_CLASSIC_MON("Method Called.");
  
  getopt_data[num_local_port].local_port = 7890;  // Default port
  getopt_data[num_local_port].proxy_mode = PROXY_MODE_RECORDER;

  kw_set_time_stamp_mode ("2"); //arg is hardcoaded as we wil use gettimeofday 
  init_ms_stamp();

  (void) signal( SIGINT, handle_sigint);

  while(( c = getopt(argc, argv, "t:n:l:s:u:va:x:X:S:i:K:I:c:e:o:")) != -1 ) {
    switch(c) {
      case 'n':  // Script Name
        scriptName = strdup(optarg);
        break;
      case 't':  // Page threshold in milli-sec
        page_threshold = atoi(optarg);
        break;
      case 'l':  // Local port where is listens for connectin from browser (client)
        //local_port = atoi(optarg);
        if(num_local_port >= MAX_LOCAL_PORT) {
          error_log(0, _FLN_, "Max %d local port supported\n", optarg, MAX_LOCAL_PORT);
          exit(-1);
        }
        getopt_data[num_local_port].local_port = atoi(optarg);
        num_local_port++;
        break;
      case 'v':  // Detailed bebug log
        verbose = 1;
        break;
      case 'a':  // Treat each URLs as separate page
        all_are_pages = 1;
        break;
      case 'x':  // Proxy Server for HTTP
        NSDL2_CLASSIC_MON("-x activited");
        httpchain = CHAININGENABLED;
        proxy_mode = PROXY_MODE_CHAIN;
        strcpy(httpchainserver, strtok(optarg, ":"));
        httpchaintemp = NULL;
        httpchaintemp = strtok(NULL,":") ;
        if(httpchaintemp != NULL)
          httpchainport = atoi(httpchaintemp);
        break;
      case 'X':  // Proxy Server for HTTPS
        NSDL2_CLASSIC_MON("-X activited");
        httpschain =CHAININGENABLED;
        proxy_mode = PROXY_MODE_CHAIN;
        strcpy(httpschainserver,strtok(optarg,":"));
        httpchaintemp = NULL;
        httpchaintemp = strtok(NULL, ":") ;
        if(httpchaintemp != NULL)
          httpschainport = atoi(httpchaintemp);
        break;
      case 's': // Web Services - HTTP
        if (strlen(optarg) > MAX_HOST_NAME) {
          error_log(0, _FLN_, "Remote Server Name [%s] is too Long\n", optarg);
          exit(1);
        }
        if(num_host_port >= MAX_HOST) {
          error_log(0, _FLN_, "Max %d host supported\n", optarg, MAX_HOST);
          exit(1);
        }
        strcpy(getopt_data[num_host_port].remote_server, optarg);
        getopt_data[num_host_port].proxy_mode = PROXY_MODE_WEB_SERVICE_HTTP;
        num_host_port++;
        break;
      case 'S': // Web Services - HTTPS
        if (strlen(optarg) > MAX_HOST_NAME) {
          error_log(0, _FLN_, "Remote Server Name [%s] is too Long\n", optarg);
          exit(1);
        }
        if(num_host_port >= MAX_HOST) {
          error_log(0, _FLN_, "Max %d host supported\n", optarg, MAX_HOST);
          exit(1);
        }
        strcpy(getopt_data[num_host_port].remote_server, optarg);
        getopt_data[num_host_port].proxy_mode = PROXY_MODE_WEB_SERVICE_HTTPS;
        num_host_port++;
        break;
      case 'u': // Url 
        if(num_url >= MAX_HOST) {
          error_log(0, _FLN_, "Max %d URL regex supported\n", optarg, MAX_HOST);
          exit(1);
        }
        strcpy(getopt_data[num_url].host_url, optarg);
        num_url++;
        break;
      case 'i': // ip 
        g_listen_ip_addr = strdup(optarg);
        break;
      case 'K': // Keep alive time in seconds
        keep_alive_time = atoi(optarg);
        break;
      case 'I': // Keep alive time in seconds
        monitor_child_idle_time = atoi(optarg);
        break;
      case 'c': // client certificate Bug 62367,67875 
        strcpy(getopt_data[num_host_port - 1].client_cert, optarg);
        break;
      case 'e': // end point certificate Bug 62367,67875
        strcpy(getopt_data[num_host_port - 1].end_point_cert, optarg);
        break;
      case 'o': // parent Keep alive time in seconds
        recording_timeout = atoi(optarg);
        break;
      case '?':
        errflg++;
        break;
    }
  }
  NSDL2_CLASSIC_MON("client_cert = [%s], end_point_cert = [%s]", getopt_data[num_host_port - 1].client_cert, getopt_data[num_host_port - 1].end_point_cert);

  if( errflg || !scriptName) {
    error_log(0, _FLN_, "usage: monitor (-n scriptName) (-l<local_port>) (-v) (-a)\n");
    exit(1);
  }

  if(num_host_port != num_local_port || num_host_port != num_url) {
    
    //In case of proxy_mode user can specify port number using -l 
    if(num_local_port != 1 && num_host_port != 0){
      error_log(0, _FLN_, "Number of port argument (%d) with -l is not"
                        " same as number of host (%d) given with -S/-s and urls (%d).\n",
                        num_local_port, num_host_port, num_url);
      exit(1);
    }
  }

  sort_getopt_data(num_host_port);

  #if OPENSSL_VERSION_NUMBER < 0x10100000L  
    kw_set_ssl_lib_hpm("SSL_LIB_HPM 0");
  #endif

  signal(SIGALRM, handle_sigint);
  alarm(recording_timeout);

  (void) signal( SIGCHLD, handle_sickchild);
  
  create_script_dir(scriptName);
  
  NSDL2_CLASSIC_MON("proxy_mode = [%s], num_child = %d",
                     proxy_mode_string[proxy_mode], num_child); 
  /* We will fork child only if there is more than 1 hosts, ports
   * if num_child is 1 than we will set CHILD_INDEX to 0
   */
  if(num_child > 1) {
    for(i = 0; i< num_child ; i++) {
 
      set_child_env(i, my_sub_child_id);

      if ((child_pid = fork()) < 0) {
        error_log(0, _FLN_, "*** server:  Failed to create child process.  Aborting...");
        kill_children();
        exit(1);
      }
 
      if (child_pid > 0) {
        child_data[i].pid = child_pid;
      } else {
        break;
      }
    }
 
    if (PARENT) {
      NSDL2_CLASSIC_MON("mon_sickchild_pending = %d", mon_sickchild_pending);

      while (PARENT) {
        if (mon_sickchild_pending) {
            kill_children();
            exit(1);
        }
        parent_started = 1;
        (void) signal( SIGTERM, handle_sigusr1 );
        (void) signal( SIGINT, handle_sigusr1 );
        NSDL2_CLASSIC_MON("Parent while loop.");
       
        int flag = 1;
        for (i = 0; i < num_child; i++) {
          if (child_data[i].pid != -1) {
            NSDL2_CLASSIC_MON("i = %d, Not all children are killed. Waiting");
            flag = 0;
            break;
          }
        }
        if (flag) {
          copy_logs_to_scripts(); 
          NSDL2_CLASSIC_MON("Exitting from parent");
          return 0;
        }
        pause();
      }
    } else {
       prctl(PR_SET_PDEATHSIG, SIGKILL);
       monitor_child(); // This is child which will return when stopped
    }
  } else {
      set_child_env(0, my_sub_child_id);
      monitor_child(); // This is child which will return when stopped
  }
  return 0;
}

//------------------------------------------------------------------------
// remove the hostname from HTTP commandline
// It is part of URL
// in points to -> <Method> http[s]://hostname/URL <Version>
// out will be -> <Method> /URL <Version>
static void remove_host_from_cmd_line(char *in, char *out, int proto) {
  char *ptr;
  int index1 = 8;
  int n;

  NSDL2_CLASSIC_MON("Method Called, in = [%s], out = [%s], proto = [%d]",
                     in, out, proto);

  ptr = strtok(in, " ");
  sprintf(out, "%s ", ptr);
  n = strlen(ptr);     // n will be length of cmd GET, HEAD etc
  if(proto == HTTPS)
    index1 = 9;
  ptr = index(&in[n + index1], '/'); //index1 is for ' http://' or for ' https://'
  strcat(out, ptr); //out now has "HTTP-cmd URL HTTP/ver\r\n"
  NSDL2_CLASSIC_MON("out = [%s]", out);
}

/*
   read_and_save_req_rep() function
   copies data from input file descriptor to output file
   descriptor, with a copy sent to the dump_file.
*/
//type ==1 for OG (request) and 2 for IC (response)
int read_and_save_req_rep(void *from_fp, void *to_fp, int verbose, int type, int proto, char* url){ 
 // char buf[BUFSIZE];         /* read/write data buffer        */
 // char buf1[BUFSIZE];        /* read/write data buffer        */
  int i, isHeader;           /* isHeader flag for HTTP header */
  static char *pattern = "content-length:";   /* string for content-length header       */
  int  chunks, nbytes, remainder;            /* For calcualting length of message body */
  int first=1, chunked=0;
  char content_type_string[MAX_HEADER_SIZE];//strings to save Content-Type 
  char content_encoding_string[MAX_HEADER_SIZE];//Strings to save Content-Encoding
  char content_length_string[MAX_HEADER_SIZE];//Strings to save Content-Length
  int is_first_line = 1;
 // char *ptr;

  isHeader = TRUE;
  nbytes = -1;
  if(type == 1){
    content_type_req = CONTENT_OTHER;
  }
  else if(type == 2){
    content_type_res = CONTENT_OTHER;
 } 

  NSDL2_CLASSIC_MON("Method Called, type = [%s], proto = %d, url = %s", 
                     type_string[type], proto, url);

  //if (proxy_mode) first = 0;
  while(isHeader) {
   if( getline(buf, sizeof(buf), from_fp, proto, type) == SUCCESS) {
      if(type == 2){
        if(is_first_line){
          gettimeofday(&res_time, NULL);
          NSDL2_CLASSIC_MON("Getting time stamp for response to calculate service time");
          is_first_line = 0;
        }
      }  
      NSDL2_CLASSIC_MON("buf = [%s], first = %d", buf, first);
      if( strcmp(buf, "\r\n") == 0 || strcmp(buf, "\r") == 0 || strcmp(buf, "\n") == 0) {
        NSDL2_CLASSIC_MON("Body Started.");
        isHeader = FALSE;
        putline (buf, to_fp, proto);
        // mark_end_headers (buf, type);
        break;
      }
      if ((type == 1) && (first == 1)) { //First line to parse
        first = 0;
        if (my_child_data->proxy_mode || (proto == HTTPS)) {
          NSDL2_CLASSIC_MON("proxy mode");
          putline (buf, to_fp, proto);
          // remove the hostname from HTTP commandline. It is part of URL
          // Format http://hostname/URL
          // TODO:
          //remove_host_from_cmd_line(buf, buf1, proto);
          copy_header (buf, type);
        } else  { //HTTP & Using Proxy
          // remove the hostname from HTTP commandline. It is part of URL
          // Format http://hostname/URL -- I think format is GET /tours/index_page.html HTTP/1.1
          // TODO: URL is not going right in capture file
          NSDL2_CLASSIC_MON("proxy mode");
          remove_host_from_cmd_line(buf, buf1, proto);
          putline (buf1, to_fp, proto);
          copy_header (buf1, type);
        }
        //add host header
        sprintf(buf1, "Host: %s\r\n", cur_host_name);
        putline (buf1, to_fp, proto);
        copy_header (buf1, type);
      } else if (type == 1) { //Remaining lines
        /* if-* and proxy-connection header is not supported */
        if((strncasecmp(buf, "proxy-connection:", 17) != 0 ) 
        && (strncasecmp(buf, "If-", 3) != 0 ) && (strncasecmp(buf, "host:", 5) != 0 )) { /* if not host-connection header */
          /* proxy-connection is filtered and replaced by netstorm by connection header
           * host header is generated and is inserted just after the HTTP command in the
           * above code block.
           * Block the browser Host header as it would point to proxy which is NetStorm IP address
           */
          putline(buf, to_fp, proto);
          copy_header(buf, type);
        }
      } else { // Response
       /* type == 2 */
        // Force connection to Close as we want broswer to send next req in new connection
        // Also do not copy Keep-Alive
        //if(strncasecmp(buf, "Connection:", 11) == 0 )
         // putline("Connection: Close\r\n", to_fp, proto);
        //else if(strncasecmp(buf, "Keep-Alive:", 11) != 0 )
          putline(buf, to_fp, proto);
        copy_header(buf, type);
      }
      NSDL2_CLASSIC_MON("buf = [%s]", buf);

      // HTTP Req/Resp can have content length or Transfer-Encoding: chunked or none of these
      if(strncasecmp(buf, pattern, 15) == 0 ) { // Check content length
        nbytes = atoi(&buf[15]);
        NSDL2_CLASSIC_MON("Content-length = %d", nbytes);
      } else if(strncasecmp(buf, "Transfer-Encoding:", 18) == 0 ) {
        chunked=1;
      } 

      // Check Content-Encoding in request and set the flag
      if(type == 1)
      {
        if(strncasecmp(buf, "Content-Encoding: gzip", 22) == 0 ) {
          NSDL2_CLASSIC_MON("Got gzip header");
          http_compression_req = NSLB_COMP_GZIP; //printf("Its gzip\n");
        } else if(strncasecmp(buf, "Content-Encoding: deflate", 25) == 0 ) {
          http_compression_req = NSLB_COMP_DEFLATE;
        } else if(strncasecmp(buf, "Content-Encoding: br", 20) == 0 ) {
          http_compression_req = NSLB_COMP_BROTLI;
        } 
      // Check Content-Encoding in response and set the flag
      }else if(type == 2){
        if(strncasecmp(buf, "Content-Encoding: gzip", 22) == 0 ) {
          NSDL2_CLASSIC_MON("Got gzip header");
          http_compression_res = NSLB_COMP_GZIP; //printf("Its gzip\n");
        } else if(strncasecmp(buf, "Content-Encoding: deflate", 25) == 0 ) {
          http_compression_res = NSLB_COMP_DEFLATE;
        } else if(strncasecmp(buf, "Content-Encoding: br", 20) == 0 ) {
          http_compression_res = NSLB_COMP_BROTLI;
        }
      }
      // These will be used in saving header string for diffrent headers. We need to save header for trace log
      char *hdr_start_ptr = NULL; 
      char *hdr_end_ptr = NULL; 
      int hdr_str_len;
      //Save Content-Type header string for trace log
      if(strncasecmp(buf, "Content-Type:", 13) == 0 ){
        hdr_start_ptr = strcasestr(buf, "Content-Type:");
        hdr_end_ptr = strstr(buf, "\r\n");
        if(!hdr_end_ptr){
          error_log(0, _FLN_, "Header end not found for Content-Type, type = %d, errorno = %d", type, errno);
        }
        else{
          hdr_str_len = hdr_end_ptr - hdr_start_ptr;
          strncpy(content_type_string, hdr_start_ptr, hdr_str_len); 
          content_type_string[hdr_str_len] = '\0';
        }
        hdr_str_len = 0;
        hdr_start_ptr = NULL;
        hdr_end_ptr = NULL;
      }
       //Save Content-Encoding header string for trace log
      if(strncasecmp(buf, "Content-Encoding:", 17) == 0 ){
        hdr_start_ptr = strcasestr(buf, "Content-Encoding:");
        hdr_end_ptr = strstr(buf, "\r\n");
        if(!hdr_end_ptr){
          error_log(0, _FLN_, "Header end not found for Content-Encoding, type = %d, errorno = %d", type, errno);
        }
        else{
          hdr_str_len = hdr_end_ptr - hdr_start_ptr;
          strncpy(content_encoding_string, hdr_start_ptr, hdr_str_len); 
          content_encoding_string[hdr_str_len] = '\0';
        }
        hdr_str_len = 0;
        hdr_start_ptr = NULL;
        hdr_end_ptr = NULL;
      }
       //Save Content-Length header string for trace log
      if(strncasecmp(buf, "Content-Length:", 15) == 0 ){
        hdr_start_ptr = strcasestr(buf, "Content-Length:");
        hdr_end_ptr = strstr(buf, "\r\n");
        if(!hdr_end_ptr){
          error_log(0, _FLN_, "Header end not found for Content-Length, type = %d, errorno = %d", type, errno);
        }
        else{
          hdr_str_len = hdr_end_ptr - hdr_start_ptr;
          strncpy(content_length_string, hdr_start_ptr, hdr_str_len); 
          content_length_string[hdr_str_len] = '\0';
        }
        hdr_str_len = 0;
        hdr_start_ptr = NULL;
        hdr_end_ptr = NULL;
      }
      // Now check for content type
      if(type == 1){
        if(strncasecmp(buf, "Content-Type: application/x-amf", 31) == 0 ){
          content_type_req = APPLICATION_AMF;
        } else if((strncasecmp(buf, "Content-Type: x-application/hessian", 35) == 0) || (strncasecmp(buf, "Content-Type: application/x-hessian", 35) == 0)) {
          content_type_req = APPLICATION_HESSIAN;
          NSDL2_CLASSIC_MON("bit set for hessian request");
        }
      }else if(type == 2){ 
        if(strncasecmp(buf, "Content-Type: application/x-amf", 31) == 0 ){
          content_type_res = APPLICATION_AMF;
        } else if((strncasecmp(buf, "Content-Type: x-application/hessian", 35) == 0) || (strncasecmp(buf, "Content-Type: application/x-hessian", 35) == 0)) {
          content_type_res = APPLICATION_HESSIAN;
          NSDL2_CLASSIC_MON("bit set for hessian response");
        }
      }
    } else { // Error in getline()
      error_log(0, _FLN_, "Error in getline() while reading headers. type = %d,  errno = %d\n", type, errno);
      return(-1);
    }
  }//end of while

  NSDL2_CLASSIC_MON("Finished with headers");
  if( nbytes >= 0 ) {
    /* calculate how many full buffers we can read, and then calculate the size
    of the remainder we'll need to copy after all full buffers have been
    transferred. */
    chunks = nbytes / BUFSIZE;
    remainder = nbytes % BUFSIZE;
    NSDL2_CLASSIC_MON("Content Length = %i, BUFSIZE= %i, Segments = %i, Remainder = %i",
                       nbytes,  BUFSIZE, chunks, remainder);

    //memset(buf, 0, BUFSIZE);

    for(i=0; i<chunks; i++) {
      if( dataread(buf, BUFSIZE, from_fp, proto) > 0) {
        copy_data(buf, BUFSIZE, type, BODY);
        if (datawrite(buf, BUFSIZE, to_fp, proto) <= 0) {
          error_log(0, _FLN_, "datawrite() failed for body of type = %d for segment number = %d.  Error = %s\n", type, i+1, strerror(errno));
          return(-1);
        }
      } else {
        break;
      }
    }
    /* Done buffer chunks; now copy over the remainder */
    NSDL2_CLASSIC_MON("remainder = %d", remainder);
    if (remainder) {
      if( dataread(buf, remainder, from_fp, proto) > 0  ) {
        copy_data(buf, remainder, type, BODY);
        if (datawrite(buf, remainder, to_fp, proto) <= 0) {
          error_log(0, _FLN_, "datawrite() failed for body of type = %d for last segment number = %d.  Error = %s\n", type, i+1, strerror(errno));
          return(-1);
        }
      }
    }
    NSDL2_CLASSIC_MON("Finished data transfer.");
    if(type == 1){//Trace for non chunked request
      sprintf(trace_msg, "Request (%d bytes) captured for URL %s (%s, %s, %s)\n", nbytes, url, content_type_string, content_encoding_string, content_length_string);
      trace_log(trace_msg);
      gettimeofday(&req_time, NULL);
      NSDL2_CLASSIC_MON("Getting timestamp for request to calculate service time");
}
  } else if (chunked) {
    NSDL2_CLASSIC_MON("Data is chunked");
    int chunk_size;
    i = 0;  // Used to track chunk number
    // Anil - How chunk data is stored in dump files?
    while (1) {
      //chunk_buf is used for reading chunk data in case of chunk encoding
      if( getline(chunk_buf, MAX_CHUNK_SIZE, from_fp, proto, type) == SUCCESS) {
        putline(chunk_buf, to_fp, proto);
        chunk_size = strtol(chunk_buf, NULL, 16); // chunk size is in hex
        NSDL2_CLASSIC_MON("Chunk Size = x%x (%d)", chunk_size, chunk_size);
        if((chunk_size < 0) || (chunk_size > MAX_CHUNK_SIZE)) {
          error_log(0, _FLN_, "Chunk size (%d) is > max buffer size (%d). Chunk number =%d\n", chunk_size, MAX_CHUNK_SIZE, i);
          return(-1);
        }
        if (chunk_size) {
          if( dataread(chunk_buf, chunk_size + 2, from_fp, proto) > 0  ) {
            copy_data(chunk_buf, chunk_size, type, BODY);
            if (datawrite(chunk_buf, chunk_size + 2, to_fp, proto) <= 0) {
              error_log(0, _FLN_, "datawrite() failed for body of type = %d for chunk data of size = %d. Chunk number =%d. Error = %s\n", type, chunk_size, i, strerror(errno));
              return(-1);
            }
          }
        } else {
          // Chunk size is 0 which means it is last chunk
          NSDL2_CLASSIC_MON("Last chunk received. Reading next line of last chunk.");
          if(getline(chunk_buf, MAX_CHUNK_SIZE, from_fp, proto, type) == SUCCESS) {
            if( strcmp(chunk_buf, "\r\n") == 0 || strcmp(chunk_buf, "\r") == 0 || strcmp(chunk_buf, "\n") == 0  ) {
              putline(chunk_buf, to_fp, proto);
              break; // Break as we got last chunk
            } else {
              error_log(0, _FLN_, "Last chunk is not followed by empty line. Chunk number =%d. Line read is %s\n", i, chunk_buf);
              return(-1);
            }
          }
          error_log(0, _FLN_, "Error in reading line after last chunk. Chunk number =%d. Error = %s\n", i, strerror(errno));
          return(-1);
        }
        i++; 
      } else {
        error_log(0, _FLN_, "Error in reading line for chunk. Chunk number =%d. Error = %s\n", i, strerror(errno));
        return(-1);
      }
    }
    if(type ==1){//Trace for chunked request
      sprintf(trace_msg, "Request captured for URL %s (%s, %s, Transfer-Encoding: chunked)\n", url, content_type_string, content_encoding_string);
      trace_log(trace_msg);  
      gettimeofday(&req_time, NULL);
      NSDL2_CLASSIC_MON("Getting timestamp for request to calculate service time");
    }
  } else {
    NSDL2_CLASSIC_MON("Body (%s) processing: No length or Chunked.", type==1?"OG":"IC");
    /* don't know message length because there was no content-length header, so go
    into an infinite loop copying single bytes.  This is slow and ineffecient,
    but that doesn't matter for this demo.
    */
    if (type == 1) { /* client will send no data */
      sprintf(trace_msg, "Request captured for URL %s (%s, %s)\n" ,url, content_type_string, content_encoding_string);
      trace_log(trace_msg);
      NSDL2_CLASSIC_MON("type = %d, returning ...", type);
      gettimeofday(&req_time, NULL);
      NSDL2_CLASSIC_MON("Getting timestamp for request to calculate service time");
      return 0;
    }

    i = 1;  // Used to track number of segments
    while ( dataread(buf, 1, from_fp, proto) > 0 ) {
      copy_data(buf, 1, type, BODY);
      if (datawrite(buf, 1, to_fp, proto) <= 0) {
        error_log(0, _FLN_, "Error in datawrite(). Segment number =%d. Error = %s\n", i, strerror(errno));
        break;
        // Anil - Should we exit here??
      }
      i++;
    }
  }
  //Trace log for request
  if(type == 2){//Trace log for response
    if(chunked){//For chunked response
      sprintf(trace_msg, "Reesponse captured (%s, %s, Transfer-Encoding: chunked)\n", content_type_string, content_encoding_string);
    }else{//For other than chunked response
       sprintf(trace_msg, "Response (%d bytes) captured (%s, %s, %s)\n", nbytes, content_type_string, content_encoding_string, content_length_string);
    }
    trace_log(trace_msg);
  }
  NSDL2_CLASSIC_MON("returning from read_and_save_req_rep");
  return 0;
}

void on_classic_mon_session_start(char *sname) {
  //int pid;
  char file_name[1024];
  //char script_name[1024];
  //char buf[32];

#if 0
  /*Validate script name*/
  len = strlen(sname);
  if (len > 31)
  {
    error_log(0, _FL_, "on_classic_mon_session_start", "Script Name should be limited to 31 characters\n");
    exit (1);
  }

  for (i=0; i<len;i++)
  {
    if (!(isalnum((sname[i])) || (sname[i] == '_')))
    {
      error_log(0, _FL_, "on_classic_mon_session_start", "Script Name Can only have alphanumeric characters or _, in its name");
      exit (1);
    }
  }

  sprintf(script_name, "default/default/%s", sname); 

  setenv("NS_SCRIPT_NAME", script_name, 1);
  /*Create Script Dir*/
  if (getenv("NS_WDIR") != NULL)
    strcpy(wdir, getenv("NS_WDIR"));
  else
    strcpy(wdir, ".");

  strcat(wdir, "/scripts/");
  strcat(wdir, script_name);

  //check_uniq_instance();
  if (!strcmp (script_name, "default/default/_newScript"))
  {
    sprintf (file_name, "rm -rf %s", wdir);
    system(file_name);
  }

  if (mkdir(wdir, 0775))
  {
    error_log(0, _FL_, "on_classic_mon_session_start", "Can not create Script with name '%s': %s\n", script_name, strerror(errno));
    unlink(lock_fname);
    exit (1);
  }

#endif
  NSDL2_CLASSIC_MON("Method Called, Script dir = [%s]", wdir);

  // For child 0 we need to make only one file so we are trying not to add id
  if(!my_child_id)
    sprintf(file_name, "%s/script.capture", wdir);
  else
    sprintf(file_name, "%s/script.capture.%d", wdir, my_child_id);

  if ((capture_fp = fopen(file_name, "w")) == NULL) {
    error_log(0, _FLN_, "Error in opening the capture file %s\n", file_name);
    unlink(lock_fname);
    exit (1);
  }

  if(!my_child_id) {
    sprintf(file_name, "%s/script.detail", wdir);
  } else {
    sprintf(file_name, "%s/script.detail.%d", wdir, my_child_id);
  }

  if ((detail_fp = fopen(file_name, "w")) == NULL) {
    error_log(0, _FLN_, "Error in opening the detail file %s\n", file_name);
    unlink(lock_fname);
    exit (1);
  }

  if(!my_child_id)
    sprintf(file_name, "%s/script.c", wdir);
  else 
    sprintf(file_name, "%s/script.c.%d", wdir, my_child_id);

  if ((c_fp = fopen(file_name, "w")) == NULL) {
    error_log(0, _FLN_, "Error in opening the C file %s\n", file_name);
    unlink(lock_fname);
    exit (1);
  }

  if(!my_child_id) {
    sprintf(file_name, "%s/script.h", wdir);
  } else {
    sprintf(file_name, "%s/script.h.%d", wdir, my_child_id);
  }

  if ((h_fp = fopen(file_name, "w")) == NULL) {
    error_log(0, _FLN_, "Error in opening the h file %s\n", file_name);
    unlink(lock_fname);
    exit (1);
  }

  //Write Header part in c and capture files
  fprintf(c_fp, "#include <stdio.h>\n");
  fprintf(c_fp, "#include <stdlib.h>\n");
  fprintf(c_fp, "#include <string.h>\n");
  fprintf(c_fp, "#include \"../../include/ns_string.h\"\n");
  fprintf(c_fp, "#include \"script.h\"\n\n");
  fprintf(c_fp, "int init_script() {\n");

  fprintf(capture_fp, "main()\n{\n  int next_page, think_time;\n//Define Any NS Variables here. Do not remove or modify this line\n\n//End of NS Variable declarations. Do not remove or modify this line\n\n  next_page = init_script();\n\n  while(next_page != -1) {\n    switch(next_page) {\n");

#if  0
  // this will contain all the request files
  sprintf(req_dir, "%s/request", wdir);
  if (mkdir(req_dir, 0777))
  {
    error_log(0, _FL_, "on_classic_mon_session_start", "Can not create Script requests direcetory: %s", strerror(errno));
    unlink(lock_fname);
    exit (1);
  }

  // this will contain all the response files
  sprintf(resp_dir, "%s/response", wdir);
  if (mkdir(resp_dir, 0777))
  {
    error_log(0, _FL_, "on_classic_mon_session_start", "Can not create Script responses direcetory: %s", strerror(errno));
    unlink(lock_fname);
    exit (1);
  }

  //create dump dir for keeping page dumps
  strcat(wdir, "/dump");
  if (mkdir(wdir, 0777))
  {
    error_log(0, _FL_, "on_classic_mon_session_start", "Can not create Script dump direcetory: %s", strerror(errno));
    unlink(lock_fname);
    exit (1);
  }
  sprintf(file_name, "%s/index", wdir);
  if ((index_fp = fopen(file_name, "w")) == NULL)
  {
    error_log(0, _FL_, "on_classic_mon_session_start", "Error in opening the dump index file %s\n", file_name);
    unlink(lock_fname);
    exit (1);
  }

  //create headers dir for keeping page dumps
  sprintf(file_name, "%s/headers", wdir);
  if (mkdir(file_name, 0777))
  {
    error_log(0, _FL_, "on_classic_mon_session_start", "Can not create Script headers direcetory: %s", strerror(errno));
    unlink(lock_fname);
    exit (1);
  }

  cur_page_name[0] = 0;
  last_page_name[0] = 0;

  // Must write this exact message on the stdout as it is used by nsi_start_monitor
#endif
  printf("Ready to Record for port: %d\n", my_child_data->local_port);
  fflush(NULL);
  NSDL2_CLASSIC_MON("Ready to Record for port: [%d]", my_child_data->local_port);

#if 0
  pid = fork();
  if (pid  == 0)
  {
    sprintf(buf, "%d\n", getpid());
    write(lock_fd, buf, strlen(buf));
    close(lock_fd);
  }
  else if (pid < 0)
  {
    error_log(0, _FL_, "on_classic_mon_session_start", "fork failed for capturing\n");
    unlink(lock_fname);
    exit(1);
  }
  else
  {
    unlink(lock_fname);
    exit(0);
  }
#endif
}

void on_classic_mon_session_end() {
  char cmd_buf[128];

  NSDL2_CLASSIC_MON("Method Called");

  if (last_page_start_time) {
    NSDL2_CLASSIC_MON("Last Page [%s] : Took %u ms", 
                       cur_page_name, (last_stamp-last_page_start_time));
  }

  //Write trailer part in c and capture
  fprintf(capture_fp, "          NUM_EMBED=%d);\n", num_embed);
  fprintf(capture_fp, "        next_page = check_page_%s();\n", cur_page_name);
  fprintf(capture_fp, "        break;\n");
  fprintf(capture_fp, "\n      default:\n        next_page = -1;\n    }\n    do_think(think_time);\n  }\n  exit_script();\n}\n");
  fprintf(c_fp, "\n\treturn -1;\n}\n");
  fprintf(c_fp, "\nvoid exit_script() {\n\treturn;\n}\n");
  sprintf(cmd_buf, "nsi_capture_dump");
  system(cmd_buf);
  //Close all fd's
  fclose(capture_fp);
  fclose(detail_fp);
  fclose(c_fp);
  fclose(h_fp);
  fclose(index_fp);
  //remove uniq lock file
  unlink(lock_fname);
  ssl_end();
}

//cur_page_name and last_page_name contains page names
//cur_page_num contains page num
void on_classic_mon_page_start() {
  char file_name[1024];

  NSDL2_CLASSIC_MON("Method Called");

  //if not first page, write the last page trailer NUM_EMBED, next_page=.. etc in capture
  if (last_page_name[0] != '\0') {
    fprintf(capture_fp, "          NUM_EMBED=%d);\n", num_embed);
    fprintf(capture_fp, "        next_page = check_page_%s();\n", last_page_name);
    fprintf(capture_fp, "        break;\n\n");
    fclose(hdr_fp);
  }

  num_embed = 0;
  //cur page header and cur page header in detail,
  //cur page in c and h files
  fprintf(capture_fp, "      case %s:\n", cur_page_name);
  fprintf(capture_fp, "        think_time = pre_page_%s();\n", cur_page_name);
  fprintf(capture_fp, "        web_url (%s,\n", cur_page_name);
  fprintf(detail_fp, "--Page %s\n", cur_page_name);
  fprintf(c_fp, "\n\treturn %s;\n}\n\nint pre_page_%s(void) {\n   return NS_USE_CONFIGURED_THINK_TIME;\n}\n\nint check_page_%s(void) {\n", cur_page_name, cur_page_name,cur_page_name);

  fprintf(h_fp, "#define %s %d\n", cur_page_name, cur_page_num-1);

  sprintf(file_name, "%s/dump/headers/page_%d.txt", wdir, cur_page_num);
  if ((hdr_fp = fopen(file_name, "w")) == NULL) {
    error_log(0, _FLN_, "Error in opening the headers file %s\n", file_name);
    exit (1);
  }
  fflush(capture_fp);
  fflush(detail_fp);
  fflush(c_fp);
  fflush(h_fp);
}

//Input:all_are_page ==1, if all urls are conisdered pages
//Input:ubuf contains URL line till HTTP/1.1\r\n
//e,g.,  Tours HTTP/1.1\r\n
void mark_if_new_page(int all_are_pages, char *ubuf)
{
  char *ptr;
  cur_stamp = get_ms_stamp();

  NSDL2_CLASSIC_MON("Method Called");

  if (all_are_pages || (last_stamp == 0) || ((cur_stamp - last_stamp) > page_threshold)) {
    //Its a new page
    ptr = strchr(ubuf, ' '); //Mark the URL end
    if (ptr) {
      *ptr='\0';
    }
    ptr = get_page_from_url(ubuf);
    //fprintf(dump_file, "Page: %s : Took %u ms\n", ptr, last_page_start_time?(last_stamp-last_page_start_time):0);
    NSDL2_CLASSIC_MON("Starting Page: [%s]", ptr);

    if (last_page_start_time) {
      NSDL2_CLASSIC_MON("Last Page [%s]: Took %u millisecs",
                        last_page_name,
                        (last_stamp-last_page_start_time));
    }

    strcpy(last_page_name, cur_page_name);
    strcpy(cur_page_name, ptr);
    last_page_start_time = cur_stamp;
    is_first_url = 1;
    cur_page_num++;
  } else {
    is_first_url = 0;
  }
  //fprintf(dump_file, "%u:stamp is %u\n", cur_stamp, last_stamp?(cur_stamp-last_stamp):0);
  NSDL2_CLASSIC_MON("is_first_url = %d", is_first_url);
  if (last_stamp) {
    NSDL2_CLASSIC_MON("Time since last hit: %u (%u)", (cur_stamp - last_stamp), cur_stamp);
  }
}

int get_modem_compression (char *buf1, int len1, char *buf2, int len2) {
  int filepipe[2];
  int compressionpipe[2];
  char read_file_fd[8];
  char write_file_fd[8];
  char write_compression_fd[8];
  char comp_buff[16];
  int child_pid, status;
  int tx_comp_ratio = 1;

  pipe(filepipe);
  pipe(compressionpipe);

  child_pid = fork();

  if (child_pid == 0) {
    sprintf(read_file_fd, "%d", filepipe[0]);
    sprintf(write_file_fd, "%d", filepipe[1]);
    sprintf(write_compression_fd, "%d", compressionpipe[1]);
    execl("./v42/compact", "./v42/compact", "-T", "10", "-i", read_file_fd, "-g", write_file_fd, "-h", write_compression_fd, "-q", NULL);
  } else {
    if (len1) {
      write(filepipe[1], buf1, len1);
    } 
   
    if (len2) {
      write(filepipe[1], buf2, len2);
    }
    close(filepipe[1]);
    close(filepipe[0]);
    read(compressionpipe[0], comp_buff, 16);
    close(compressionpipe[0]);
    close(compressionpipe[1]);
    tx_comp_ratio = atoi(comp_buff);
    if (tx_comp_ratio < 0) {
      tx_comp_ratio = 0;
    }
    waitpid(child_pid, &status, 0);
  }
  return (tx_comp_ratio);
}

void copy_data( char * data, int len, int type, int part) {
  int chunk;
  int *cur, *max;
  char *buf;

  NSDL2_CLASSIC_MON("Methid Called type = [%s], len = %d, part = %d", 
                     type_string[type], len, part);

  if (type == 1) {
    if (part == HEADER) {
      buf = hdr_buf_out;
      cur = &cur_hlen_out;
      max = &max_hlen_out;
      chunk = 4096;
    } else {
      buf = body_buf_out;
      cur = &cur_blen_out;
      max = &max_blen_out;
      chunk = 4096;
    }
  } else {
    if (part == HEADER) {
      buf = hdr_buf_in;
      cur = &cur_hlen_in;
      max = &max_hlen_in;
      chunk = 4096;
    } else {
      buf = body_buf_in;
      cur = &cur_blen_in;
      max = &max_blen_in;
      chunk = (4096*4);
    }
  }

  NSDL2_CLASSIC_MON("cur = %d, chunk = %d, max = %d", *cur, chunk, *max);

  while (*cur+len > *max) {
    NSDL2_CLASSIC_MON("Reallocing for max+chunk = %d byte", *max + chunk);
    buf = (char *)realloc (buf, *max+chunk);
    if (buf == NULL) {
      error_log(0, _FLN_, "Error in realloc(). buf = %p, size = %d, errno = %d\n", buf, *max + chunk, errno);
      exit(1);
    }
    *max += chunk;
    if (type == 1) {
      if (part == HEADER) {
        hdr_buf_out = buf;
      } else {
        body_buf_out = buf;
      }
    } else {
      if (part == HEADER) {
        hdr_buf_in = buf;
      } else {
        body_buf_in = buf;
      }
    }
  }

  bcopy (data, buf+*cur, len);
  *cur += len;
}

/* void mark_end_headers (char *buf, int type)
{ return; } */

void copy_header(char *buf, int type)
{
  NSDL2_CLASSIC_MON("Methid Called, type = [%s], buf = [%s]", type_string[type], buf);
  /* TODO Shalu
 *  Check if it is response & strcmp with Content length
 *  create local buffer Content Length: 00000000 & pass this to copy_data instead of buf
 */
  copy_data(buf, strlen(buf), type, HEADER);
}

void on_classic_mon_url_start(int first_url) {

  NSDL2_CLASSIC_MON("Method Called, num_embed = %d, first_url = %d",
                                    num_embed, first_url);

  cur_hlen_out = 0;
  cur_hlen_in = 0;
  cur_blen_out = 0;
  cur_blen_in = 0;
  http_compression_req = NSLB_COMP_NONE;
  http_compression_res = NSLB_COMP_NONE;

  if (!first_url) num_embed++;
}

void on_classic_mon_url_end(int first_url, int proto) {
  char *ptr,*ptr2;
  char buf[4096];
  char cookie[4096];
  char method[64];
  char url[4096];
  char dump_file_name[1024];
  int tx_ratio=0, rx_ratio=0;
  int resp_code;
  int redirect = 0;
  static int last_redirect = 0;
  int is_main_url = 0;
  int cav_fd = -1;
  int amf_blen;
  static int amf_cnt=0; //To make AMF URL uiq for inlined urls

  NSDL2_CLASSIC_MON("Method called. first_url = %d, host_name = [%s], page_name = %s",
                     first_url, cur_host_name, cur_page_name);

  if (hdr_buf_out) hdr_buf_out[cur_hlen_out] = '\0';
  if (hdr_buf_in) hdr_buf_in[cur_hlen_in] = '\0';
  if (body_buf_out) body_buf_out[cur_blen_out] = '\0';
  if (body_buf_in) body_buf_in[cur_blen_in] = '\0';
  cookie[0] = '\0';

  //Get compression ratios
  //tx_ratio = get_modem_compression (hdr_buf_out,cur_hlen_out, body_buf_out, cur_blen_out);
  //rx_ratio = get_modem_compression (hdr_buf_in,cur_hlen_in, body_buf_in, cur_blen_in);

  //log headers.
  fprintf(hdr_fp, "--Request\n");
  fwrite(hdr_buf_out,sizeof(char), cur_hlen_out, hdr_fp);
  if (cur_blen_out) {
    fprintf(hdr_fp, "\n");
    fwrite(body_buf_out,sizeof(char), cur_blen_out, hdr_fp);
    fprintf(hdr_fp, "----\n");
  }
  fprintf(hdr_fp, "--Response\n");
  fwrite(hdr_buf_in,sizeof(char), cur_hlen_in, hdr_fp);
  fprintf(hdr_fp, "----\n\n");
  fflush(hdr_fp);

  //if (first_url) {//write in capture :cmd, url, hdr and body//Write in detail: compression and Cookie
  //} else {
  //Write in detail, compression, Method, URL, All Headers, body
  //}

  //get response code
  ptr = strstr(hdr_buf_in, "\r\n");
  if (!ptr) {
    error_log(0, _FLN_, "Response does not seem to have proper termination (%s)\n", hdr_buf_in);
    exit(1);
  }
  ptr2 = index(hdr_buf_in, ' ');
  if (!ptr2 || ptr2 > ptr) {
    error_log(0, _FLN_, "Response does not seem to have space separted fields (%s)\n", ptr);
    exit(1);
  }
  resp_code = atoi(ptr2);

  if  ((resp_code/100 == 3) && (resp_code != 304)) {
    redirect = 1;
  }

  //Get first line of request
  ptr = get_next_hdr(hdr_buf_out, buf);
  if (!ptr) {
    error_log(0, _FLN_, "Request cmd does not have terminator (%s)\n", hdr_buf_out);
    exit(1);
  }
  get_req_url_method(buf, method, url);
  if (first_url) {
    fprintf(capture_fp, "          METHOD=%s,\n", method);
    fprintf(capture_fp, "          URL=%s://%s%s,\n", proto==HTTP?"http":"https", cur_host_name, url);
  } else { //Non-first url
    fprintf (detail_fp, "---- %s%sTX_RAT:%d RX_RAT:%d\n", proto==HTTP?"":"HTTPS ", redirect?"REDIRECT=Y ":"", tx_ratio, rx_ratio);
    fprintf (detail_fp, "%s\r\n", buf); // Req line (e.g. GET /abc.html HTTP/1.1)
    fprintf(detail_fp, "Host: %s\r\n", cur_host_name);
  }

  //Log rest of the headers
  while ((ptr = get_next_hdr(ptr, buf))) {
    // Following headers are send by NetStorm so should not be in script
    // for both main and enbedded URL. 
    // Note that for embedded URL, Host is required and is added above

    if (!strncasecmp(buf, "Host:" , 5)) continue;
    if (!strncasecmp(buf, "User-Agent:" , 11)) continue;
    //if (!strncasecmp(buf, "Accept-Language:" , 16)) continue;
    if (!strncasecmp(buf, "Accept-Encoding:" , 16)) continue;
    //if (!strncasecmp(buf, "Accept-Charset:" , 15)) continue;
    if (!strncasecmp(buf, "Keep-Alive:" , 11)) continue;
    if (!strncasecmp(buf, "Referer:" , 8)) continue;
    if (!strncasecmp(buf, "Accept:" , 7)) continue;
    if (!strncasecmp(buf, "Connection:" , 11)) continue;

    if (!strncasecmp(buf, "Cookie:" , 7)) {
      if (first_url) {
        get_cookie_names(buf, cookie);
      } else {
        fprintf (detail_fp, "%s\r\n", buf);
      }
      continue;
    }

    if (first_url) {
      fprintf (capture_fp, "          HEADER=%s,\n", buf);
    } else {
      fprintf (detail_fp, "%s\r\n", buf);
    }
  }

  if (!first_url) {
    fprintf (detail_fp, "\r\n");
  }

  // Write body of the request if body was present
  if (cur_blen_out) {
    if (first_url) {
      fprintf (capture_fp, "          BODY=");
      if (content_type_req == APPLICATION_AMF) {
        int amflen = cur_blen_out;
        fprintf (capture_fp, "\n");
        amf_decode (0, capture_fp, 4, body_buf_out, &amflen);
        put_amf_debug (capture_fp, body_buf_out, cur_blen_out);
      } else {
        fwrite(body_buf_out,sizeof(char), cur_blen_out, capture_fp);
      }
      fprintf (capture_fp, ",\n");
    } else {
      if (content_type_req == APPLICATION_AMF) {
        int amflen = cur_blen_out;
        amf_decode(0, detail_fp, 0, body_buf_out, &amflen);
        put_amf_debug (detail_fp, body_buf_out, cur_blen_out);
      } else {
        fwrite(body_buf_out,sizeof(char), cur_blen_out, detail_fp);
      }
      fprintf (detail_fp, "\n");
    }
  }

  if (first_url) {
    fprintf (detail_fp, "CMP_RAT=%d/%d%s\n", tx_ratio, rx_ratio, redirect?";REDIRECT=YES":"");
    if (strlen(cookie)) {
      fprintf (detail_fp, "%s\n", cookie);
    }
    fprintf (detail_fp, "----\n");
  } else {
    fprintf (detail_fp, "----\n");
  }

  //dump index info for page and create file for dumping data
  if (redirect) {
    // Anil - Why we are not creating dump file for redirect case
    NSDL2_CLASSIC_MON("Redirect is 1. No dump file is created. first_url = %d,"
                      " host_name = [%s], page_name = [%s], url = [%s]",
                      first_url, cur_host_name, cur_page_name, url);
    if (first_url) {
      last_redirect = 1;
    }
  } else {
    if (first_url || last_redirect) {
      last_redirect = 0;
      is_main_url = 1;
    }
    ptr = rindex(url, '/');  // Last occurence of '/' in URL
    if (!ptr) {
      NSDL2_CLASSIC_MON("URL %s does not contain /.", url);
    }

    if (is_main_url) {
      char root_path[128], *tptr;
      int first_depth = 1;

      amf_cnt=0;
      ptr++;  *ptr = '\0'; // Anil - This will truncate main URL after last /

      // Why we are using cur_page_name in dump_file_name of main url
      // This caused one issue in gamania. 
      // Main URL was http://tw.mabinogi.gamania.com/main1.aspx
      // But dump file was main1_aspx
      // Also in index file, URL was "/", it should be /main1.aspx

      sprintf (dump_file_name,"%s/dump/%s%s%s", wdir, cur_host_name, url, cur_page_name);

      tptr = index(url, '/');
      root_path[0] = '\0';
      while (tptr) {
        tptr++;
        tptr = index(tptr, '/');
        if (first_depth) {
          first_depth = 0;
          strcpy(root_path, "./");
        } else {
          strcat(root_path, "../");
        }
      }
      fprintf (index_fp, "%d,%s,%s,%s,%s\n", cur_page_num, cur_page_name, url, cur_host_name, root_path);
      fflush(index_fp);
    } else {
      if ((content_type_req == APPLICATION_AMF)|| (content_type_res == APPLICATION_AMF)) {
        amf_cnt++;
        sprintf (dump_file_name,"%s/dump/%s%s%s_%d", wdir, cur_host_name,url, cur_page_name, amf_cnt);
      } else {
        sprintf (dump_file_name,"%s/dump/%s%s", wdir, cur_host_name,url);
      }
      if (url[strlen(url) -1] == '/')
        strcat (dump_file_name, "index");
    }
  /*  get_req_url_method() - Method called. in = GET /module/spirit/pa_module.php?module=mail&section=pa_mail&fpsrc=pa&cd=1&ulmver=3&output=json&ver=501&.crumb=8d90f24c35fd33446cc3bdb368633753,1202808409,RYLwuA8jM2/&rnd=0.12691947018942396&pid=1202806610&partner= HTTP/1.1
get_sh: .crumb=8d90f24c35fd33446cc3bdb368633753,1202808409,RYLwuA8jM2: command not found
[1]   Done                    mkdir -p /home/cavisson/work/scripts/_newScript/dump/p.www.yahoo.com/module/spirit/pa_module.php?module=mail
*/
    ptr = index (dump_file_name, '?');
    if(ptr != NULL) *ptr = '\0';
    ptr = rindex (dump_file_name, '/');
    *ptr = '\0';
    NSDL2_CLASSIC_MON("Creating dump directory. Dir = %s", dump_file_name);
    sprintf (buf, "mkdir -p %s", dump_file_name);
    system(buf);
    *ptr = '/';
    NSDL2_CLASSIC_MON("Creating dump file. File = %s", dump_file_name);
    cav_fd = open(dump_file_name, O_CREAT|O_RDWR|O_EXCL|O_CLOEXEC, 0666);
  }
 // These two buffers will be allocated in case of hessian request resposes
  char *hessian_buf = NULL;
  char *hessian_req_buf = NULL;
  char err[1024];

  //Request Buffers
  static char *body_buf_out_en = NULL; // For request
  static int body_buf_out_size = 0;
  int body_len_out = cur_blen_out; //Request length
  static int decompressed_body_len_out = 0;
  NSDL2_CLASSIC_MON("http_compression_req = %d", http_compression_req);

  //Response Buffers
  static char *body_buf = NULL;// For Response
  static int body_buf_size = 0;
  int body_len = cur_blen_in; // Response length
  static int decompressed_body_len = 0;
  NSDL2_CLASSIC_MON("http_compression_res = %d", http_compression_res);
  
  // Uncompress request, if it is compressed 
  if(body_buf_out){
    if(http_compression_req){
      NSDL2_CLASSIC_MON("Uncompressing Body, len = %d, body = [%s]", 
                         cur_blen_out, body_buf_out);
      if(nslb_decompress(body_buf_out, (size_t)cur_blen_out, &body_buf_out_en, (size_t *)&body_buf_out_size, (size_t *)&decompressed_body_len_out, http_compression_req, err, 1024) != 0) {
        error_log(0, _FLN_, "ns_decomp_do() failed. Reason %s.\n ", err);
        if(decompressed_body_len_out < 2048)
          body_buf_out_en = (char *)realloc(body_buf_out_en, 2048+1);
        body_buf_out_size = decompressed_body_len_out = snprintf(body_buf_out_en, 2048, "nslb_decompress() failed. Reason %s.\n ", err);  
        //return;
        //exit(1);
      }
      //body_buf_out = body_buf_out_en;
      //body_len_out = body_buf_out_size;
    }
  } 
   if(body_buf_in){ //Uncompress response, if it is compressed
    if(http_compression_res){
      NSDL2_CLASSIC_MON("Uncompressing Body, len = %d, body = [%s]", 
                         cur_blen_in, body_buf_in);
      if(nslb_decompress(body_buf_in, (size_t)cur_blen_in, &body_buf, (size_t *)&body_buf_size, (size_t *)&decompressed_body_len, http_compression_res, err, 1024) != 0) {
        error_log(0, _FLN_, "ns_decomp_do() failed. Reason %s.\n ", err);
        if(decompressed_body_len < 2048)
          body_buf = (char *)realloc(body_buf, 2048 + 1);
        body_buf_size = decompressed_body_len = snprintf(body_buf, 2048, "nslb_decompress() failed. Reason %s.\n ", err);  
        //return;
        //exit(1);
      }
      //body_buf_in = body_buf;
      //body_len = body_buf_size;
    }
  } 
  //For request check content type and decode request body to xml if it is AMF or hessian 
  if(body_buf_out){
      NSDL2_CLASSIC_MON("Request body found, content_type_req = %d", content_type_req);
    if(content_type_req == APPLICATION_AMF) {// Content-Type is amf
      NSDL2_CLASSIC_MON("Request body is amf.");
      amf_blen = ns_amf_binary_to_xml(body_buf_out, &cur_blen_out);
      body_buf_out_en = amf_asc_ptr;
      body_len_out = amf_blen;
    } else if(content_type_req == APPLICATION_HESSIAN){//Content-Type is hessian
      NSDL2_CLASSIC_MON("Request body is hessian. Going to decode");
      hessian_req_buf = (char *)malloc(HESSIAN_MAX_BUF_SIZE);
      hessian_set_version(2);
      NSDL2_CLASSIC_MON("cur_blen_out = %d", cur_blen_out);
      if(!(hessian2_decode(HESSIAN_MAX_BUF_SIZE, hessian_req_buf, 0, body_buf_out, &cur_blen_out))) {
        NSDL2_CLASSIC_MON("Error in hessian_decode req decoding ..............");
        error_log(0, _FLN_, "Error in hessian_decode req decoding ..............\n");
      }
      else {
        body_buf_out_en = hessian_req_buf;
        body_len_out = strlen(hessian_req_buf);
        NSDL2_CLASSIC_MON("Decoded Req Body, len = %d", body_len_out);
      }
    }//End oh heesain request condition
  //For response check content type and decode request body to xml if it is AMF or hessian 
  }
  if(body_buf_in){   
    NSDL2_CLASSIC_MON("Response body found, content_type_res = %d", content_type_res);
    if(content_type_res == APPLICATION_AMF) {//Content-Type is amf
      //amf_blen = convert_amf_hextoasc (body_buf_in, &cur_blen_in);
      amf_blen = ns_amf_binary_to_xml(body_buf_in, &cur_blen_in);
      body_buf = amf_asc_ptr;
      body_len = amf_blen;
    } else if(content_type_res == APPLICATION_HESSIAN){//Content-Type is hessian 
      NSDL2_CLASSIC_MON("response is hessian");
      hessian_buf = (char *)malloc(HESSIAN_MAX_BUF_SIZE);
      NSDL2_CLASSIC_MON("Setting heassian version 2.0");
      hessian_set_version(2);
      NSDL2_CLASSIC_MON("cur_blen_in = %d", cur_blen_in);
      if(!(hessian2_decode(HESSIAN_MAX_BUF_SIZE, hessian_buf, 0, body_buf_in, &cur_blen_in))) {
        NSDL2_CLASSIC_MON("Error in hessian_decode resp decoding ..............");
        //Here we are resetting cur_blen_in because if hessian_decode fails, it will change cur_blen_in, and as per
        //requirement if decode fails, original hessian body should be written in response_body file and response file
        cur_blen_in = body_len;
      }
      else {
        body_buf = hessian_buf;
        body_len = strlen(hessian_buf);
        //NSDL2_CLASSIC_MON("Decoded Body, len = %d, body = [%s]",
         //                body_len, body_buf);
      }
    }
  }//End of response if condition

  create_request();
  if(http_compression_req)
    create_request_body(body_buf_out_en, decompressed_body_len_out);
  else
    create_request_body(body_buf_out, body_len_out);

// To create encoded request body files 
  if(content_type_req == APPLICATION_HESSIAN)// To create hessian request body file in binary format 
    create_encoded_request_body(body_buf_out, cur_blen_out, "hessian");

  create_response(body_buf_in, cur_blen_in);

// To create encoded response body files
  if(content_type_res == APPLICATION_HESSIAN)// To create hessian reponse body file in binary format 
    create_encoded_response_body(body_buf_in, cur_blen_in, "hessian");
  else if(content_type_res == APPLICATION_AMF)
    create_encoded_response_body(body_buf_in, cur_blen_in, "amf");


  // Neeraj: What if hessian and amf is coming as compressed (Need to take care of this)
  else if(http_compression_res == NSLB_COMP_DEFLATE)
    create_encoded_response_body(body_buf_in, cur_blen_in, "deflate");
  else if (http_compression_res == NSLB_COMP_GZIP)
    create_encoded_response_body(body_buf_in, cur_blen_in, "gzip");
  else if (http_compression_res == NSLB_COMP_BROTLI)
    create_encoded_response_body(body_buf_in, cur_blen_in, "brotli");

  if(http_compression_res) {
    create_response_body(body_buf, decompressed_body_len); 
    write_all(cav_fd, body_buf, decompressed_body_len);
  }
  else { 
    create_response_body(body_buf_in, body_len); 
    write_all(cav_fd, body_buf_in, body_len);
  }
  
  fflush(c_fp);
  fflush(capture_fp);
  fflush(h_fp);
  fflush(detail_fp);
  if(hessian_buf) free(hessian_buf);
  if(hessian_req_buf) free(hessian_req_buf);
}

char *get_next_hdr (char *start, char *store) {
  char *ptr;

  NSDL2_CLASSIC_MON("Method Called, start = [%s]", start);
  
  //Get first line of request
  ptr = strstr(start, "\r\n");
  if (!ptr) {
    return NULL;
  }
  if (ptr-start > 4094) {
    error_log(0, _FLN_, "Request cmd too Long (%s)\n", ptr);
    exit(1);
  }
  bcopy (start, store, ptr-start);
  store[ptr-start] = '\0'; //store complete header line
  return(ptr+2);  //get past \r\n
}

void get_req_url_method(char *in,char * method, char *url) {
  char *ptr;
  char buf[4096];
  NSDL2_CLASSIC_MON("Method Called, in = [%s]", in);

  //Get the method and URL from the first line of headers
  strcpy(buf, in);
  ptr = strtok(buf, " ");
  if (strlen(ptr) > 63) {
    NSDL2_CLASSIC_MON("method name too Long (%s)", ptr);
    exit(1);
  }
  strcpy (method, ptr);
  ptr = strtok(NULL, " ");
  strcpy (url, ptr);
  NSDL2_CLASSIC_MON("method name = %s, url = [%s]", method, url);
  return;
}

void get_cookie_names(char *buf, char *cookie) {
  char *ptr2, *ptr3;
  //Format is Cookie: cookie-name=value; cooki-name=value..
  NSDL2_CLASSIC_MON("Method Called, buf = [%s]", buf);
  strcpy(cookie, "COOKIE=");
  ptr2 = buf+7;
  while (isspace(*ptr2)) ptr2++;
  while (1) {
    ptr3 = index(ptr2, '=');
    if (!ptr3) { 
       break;
    }
    *ptr3 = '\0'; ptr3++;
    strcat (cookie, ptr2);
    strcat (cookie, ";");
    ptr2 = index(ptr3, ';');
    if (!ptr2) {
      break;
    }
    ptr2++; //get past ;
    while (isspace(*ptr2)) ptr2++;
  }
}

void write_all(int fd, char *buf, int size) {
  int written=0;
  int len;
  
  //NSDL2_CLASSIC_MON("Method called, fd = %d, size = %d, buf = [%s]", fd, size, buf);
  NSDL2_CLASSIC_MON("Method called, fd = %d, size = %d", fd, size);
  if(fd < 0) return;  // Why call this if -ve, check later

  while (written < size) {
    len = write (fd, buf+written, size - written);
    if (len < 0 ) {
      //perror("write failed while dumping data");
      break;
    }
    written += len;
  }
  close (fd);
}

//exclusive create a lock file
void check_uniq_instance() {
  static char buf[1024];
  char *ptr;
  //int fd;
  int pid;
  FILE *fp;

  ptr = getenv("NS_WDIR");
  if (!ptr) {
    error_log(0, _FLN_, "NS_WDIR env variable must be defined\n");
    exit (1);
  }

  NSDL2_CLASSIC_MON("Method Called, ptr = [%s]", ptr);
  sprintf (lock_fname, "%s/bin/.monitor.lock", ptr);
  //Exclusive create lock file
  lock_fd = open(lock_fname, O_CREAT|O_EXCL|O_RDWR|O_CLOEXEC, 0666);
  NSDL2_CLASSIC_MON("Lock file %s fd=%d\n", lock_fname, lock_fd);
  if (lock_fd < 0) {
    //May be lock file exist but script recorder not running, left over lock
    //Get the pid from the lock file
    fp = fopen(lock_fname, "r");
    if (!fp) {
      error_log(0, _FLN_, "Unable to create monitor loc file\n");
      exit(1);
    }
    if (fgets(buf, 1024, fp) == NULL) {
      error_log(0, _FLN_, "Bad format of monitor loc file\n");
      exit(1);
    }
    pid = atoi(buf);
    //Check if last script recorder alive
    if (kill (pid, 0)) {
      //abondoned lock file. remove it
      fclose(fp);
      unlink(lock_fname);
      //Try to get the exclusive lock now
      lock_fd = open(lock_fname, O_CREAT|O_EXCL|O_RDWR|O_CLOEXEC, 0666);
      if (lock_fd < 0) {
        error_log(0, _FLN_, "Unable to new create monitor loc file\n");
        exit(1);
      }
      sprintf(buf, "%d\n", getpid());
      write(lock_fd, buf, strlen(buf));
      close(lock_fd);
    } else {
      //pid exist. capture is running
      error_log(0, _FLN_, "Another instance (%d) of NetStorm/Capture is running\n", pid);
      exit(1);
    }
  } else {
    //Got the lock , put in pid
    sprintf(buf, "%d\n", getpid());
    NSDL2_CLASSIC_MON("buf = [%s]", buf);
    //printf("Adding pid %s\n", buf);
    write_all(lock_fd, buf, strlen(buf));
    close(lock_fd);
  }
}

int peek_url_host(int sfd, int proxy_mode, char *host, char *ubuf, int old_proto) {
  int len;
  char buf[16*1024];
  char *ptr, *ptr2;

  NSDL2_CLASSIC_MON("Method Called, "
                    "sfd = %d. proxy_mode = [%s], host = [%s], ubuf = [%s]",
                     sfd, proxy_mode_string[proxy_mode], host, ubuf);

  NSDL2_CLASSIC_MON("Calling recv with MSG_PEEK ...");
  len = recv(sfd, buf, 1024*16, MSG_PEEK);
  NSDL2_CLASSIC_MON("length received by recv = [%d]", len);
  
  if (len < 0) {
    error_log(0, _FLN_, "recv peek failed. Error = %s", strerror(errno));
    return(-1);
    // exit(1);
  } else if (len == 0){
    NSDL2_CLASSIC_MON("Connection closed by client while doing MSG_PEEK.Error = %s", strerror(errno));
    error_log(0, _FLN_, "Connection closed by client. while doing MSG_PEEK in peek_url_host,Error = %s ", strerror(errno));
    return -1;
  } 

  buf[len - 1] = 0; // Null termination
  NSDL2_CLASSIC_MON("Received buffer = [%s]", buf);
  ptr = strtok(buf, " ");
  if((ptr == NULL) && (old_proto == HTTPS)) {
      return 3; // HTTPS
  } else if (ptr == NULL){
    error_log(0, _FLN_, "Expecting http: in the URL, got <%s>\n", ptr);
    return(-1);
  }
  
  NSDL2_CLASSIC_MON("ptr = [%s]", ptr);

  if (!strcasecmp(ptr, "CONNECT")) {
    NSDL2_CLASSIC_MON("HTTPS connection: host = [%s], ubuf = [%s]",
                       ptr, host, ubuf);
    return HTTPS;
  }
  ptr = strtok(NULL, " ");
  if((ptr == NULL) && (old_proto == HTTPS)) {
    return 3; // HTTPS
  } else if (ptr == NULL){
    error_log(0, _FLN_, "Expecting http: in the URL, got <%s>\n", ptr);
    return(-1);
  }
  
  NSDL2_CLASSIC_MON("ptr = [%s]", ptr);
  if (proxy_mode == PROXY_MODE_RECORDER || proxy_mode  == PROXY_MODE_CHAIN ) {
    if (strncasecmp(ptr, "http:", 5)) {
     if(old_proto == HTTPS){ 
       return 3;
      } else {
        error_log(0, _FLN_, "Expecting http: in the URL, got <%s>\n", ptr);
        return -1;
      }
    } else {
      ptr = ptr + 7;
      
      NSDL2_CLASSIC_MON("ptr after moving 7 bytes = [%s]", ptr);
      ptr2 = index(ptr, '/');
      if (ptr2) {
        strcpy(ubuf, ptr2);
        *ptr2 = '\0';
      } else {
        strcpy(ubuf, "");
      }
      if (strlen(ptr) > 255) {
        error_log(0, _FLN_, "hostname is too Long (%s)\n", ptr);
        //exit(1);
        return -1;
      }
      
      strcpy(host, ptr);
      //printf("4th token is <%s>\n", ptr);
    }
  } else {
    NSDL2_CLASSIC_MON("proxy_mdoe = [%s]", proxy_mode_string[proxy_mode]);
    if(ptr)
      strcpy(ubuf, ptr);
    // Now we will copy this in 'search url in list'
    //strcpy(host, my_child_data->remote_server[0]);
  }
  NSDL2_CLASSIC_MON("HostName = [%s], ubuf = [%s]",
                     host, ubuf);
  return HTTP;
}
