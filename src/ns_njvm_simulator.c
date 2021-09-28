#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <getopt.h> 
#include <netdb.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/types.h>       
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <time.h>
#include <endian.h>
#include <sys/un.h>
#include "nslb_sock.h"
#include <dlfcn.h>
#include "ns_nvm_msg_def.pb-c.c"  
#define NS_EXIT_VAR
#include "ns_exit.h"  

#define MAX_LINE_LENGTH 4096
static int debug_flag = 0;
static char nvm_ip[256] = "127.0.0.1";
static int nvm_port = 0;
static int num_thread = 10;
static int nvm_id = 0;
static int control_fd = -1;

static FILE *debug_fp;
static FILE *error_fp;
time_t njvm_start_time;
char script_file[512] = "";

char *thread_done_string;
char *thread_compare_string;

//Define task function.
typedef int (*TaskFunction) (void);

TaskFunction flow_func = NULL;

typedef struct ThreadData{
  int fd;
  int thread_id;
  pthread_t thread_ptr;
  char *buf;
  int buf_length;
  struct ThreadData *next;
} ThreadData;

pthread_key_t thread_key;

//have track of free threads.
ThreadData *free_td = NULL;
int num_free_td = 0;

//Control thread data.
ThreadData ct;

int testrun = -1;
int g_task_mode = 0;

static int write_msg(ThreadData *td, int len);
static char *read_msg(ThreadData *td, int *msg_len);
int nslb_tcp_client_unixs(char *sock_path);
 

#define NJVM_DEBUG(thread_id, ...) if(debug_flag) debug_log(thread_id, __FUNCTION__, __VA_ARGS__)
#define NJVM_ERROR(thread_id, ...) error_log(thread_id, __FUNCTION__, __VA_ARGS__) 
static void debug_log(int thread_id, const char *fname, char *format, ...)
{
  va_list ap;
  char buffer[MAX_LINE_LENGTH + 1];

  va_start(ap, format);
  vsnprintf(buffer, MAX_LINE_LENGTH, format, ap);
  va_end(ap);
  buffer[MAX_LINE_LENGTH] = 0;
	fprintf(debug_fp, "[%5ld]\t%2d\t%s()\t%s\n", (time(NULL) - njvm_start_time), thread_id, fname, buffer);
	fflush(debug_fp);
}

static void error_log(int thread_id, const char *fname, char *format, ...)
{
  va_list ap;
  char buffer[MAX_LINE_LENGTH + 1];

  va_start(ap, format);
  vsnprintf(buffer, MAX_LINE_LENGTH, format, ap);
  va_end(ap);
  buffer[MAX_LINE_LENGTH] = 0;
	fprintf(error_fp, "%d\t%s()\t%s\n", thread_id, fname,  buffer);
	fflush(error_fp);
}

void parse_args(int argc, char *argv[]){
  
  NJVM_DEBUG(0, "Method Called");
  char c;
  int ip_flag = 0;
  int port_flag = 0;
  // array for long argument support
  struct option longopts[] = {
                               {"nvm_ip", 1, NULL, 'i'},
                               {"nvm_port", 1, NULL, 'p'},
                               {"debug",  0, NULL, 'd'},
                               {"num_thread", 1, NULL, 'n'},
                               {"testrun", 1, NULL, 't'},
                               {"task_mode", 1, NULL, 'm'},
                               {"script_file", 1, NULL, 's'},
                               {"nvm_id", 1, NULL, 'N'},
                               {0, 0, 0,0}
                             };


   while ((c = getopt_long(argc, argv, "i:p:dn:t:m:s:N:", longopts, NULL)) != -1){
    switch (c){
      case 'i':
        strcpy(nvm_ip, optarg);
        ip_flag = 1;
        break;
      case 'p':
        nvm_port = atoi(optarg);
        port_flag = 1;
        break;        
      case 'd':
        debug_flag = 1;
        break; 
      case 'n':
        num_thread = atoi(optarg); 
        break;
      case 't':
       testrun = atoi(optarg);
       break; 
      case 'm':
       g_task_mode = atoi(optarg);
       break;

      case 's':
        strcpy(script_file, optarg);
        break;
      case 'N':
        nvm_id = atoi(optarg);
        break;

      case ':':
      case '?':
      default:
        NJVM_ERROR(0, "argument \'%s\' ignored", argv[optind]);
    }
  }
  if(!ip_flag || !port_flag) {
    NJVM_ERROR(0, "ip or port argument missing");
    exit(-1);
  }
}

#define NJVM_HDR_SIZE 24

#define SET_NJVM_HDR(buf, opcode, msg_size) \
{	 \
  *(int *)buf = msg_size + (NJVM_HDR_SIZE - sizeof(int)); \
  *(int *)(buf + sizeof(int)) = opcode;  \
  memset(buf + 2 * sizeof(int), 0, 4 *sizeof(int)); \
} 

int get_nvm_id()
{
  return nvm_id;
}

int get_user_id()
{
  ThreadData *ltd = (ThreadData *)pthread_getspecific(thread_key);
  return (ltd->thread_id);
}

char *ns_eval_string(char *param_name)
{
  ThreadData *ltd = (ThreadData *)pthread_getspecific(thread_key);
  int thread_id = ltd->thread_id;

  int packed_msg_len;
  //to send request
  NsMsg1Str out_msg_struct = NS_MSG1__STR__INIT; 

  out_msg_struct.field1 = param_name;
   
  packed_msg_len = ns_msg1__str__get_packed_size(&out_msg_struct);

  if(packed_msg_len + NJVM_HDR_SIZE > ltd->buf_length) {
    ltd->buf = realloc(ltd->buf, packed_msg_len + NJVM_HDR_SIZE);
    ltd->buf_length = packed_msg_len + NJVM_HDR_SIZE;
  }   
  
  //Pack this message
  if(ns_msg1__str__pack(&out_msg_struct, (void *)(ltd->buf + NJVM_HDR_SIZE)) != packed_msg_len) {
    NJVM_ERROR(thread_id, "Failed to pack ns_eval_string() message");
    pthread_exit(NULL);
  } 

  SET_NJVM_HDR(ltd->buf, 1014, packed_msg_len);

  NJVM_DEBUG(thread_id, "Sending ns_eval_string() message to nvm"); 
  if(write_msg(ltd, (packed_msg_len + NJVM_HDR_SIZE))) {
    NJVM_ERROR(thread_id, "Failed to send ns_eval_string() request");
    pthread_exit(NULL);
  }
 
  //check for response.
  NsMsg1Str *in_msg_struct;
  char *resp;
  int len;

  //read the response 
  resp = read_msg(ltd, &len);
  if(resp == NULL) {
    NJVM_ERROR(thread_id, "Connection closed from other end, closing this thread.");
    pthread_exit(NULL);
  }
  
  resp = ltd->buf + NJVM_HDR_SIZE; 
  packed_msg_len = (*(int *)ltd->buf) - (NJVM_HDR_SIZE - sizeof(int));
 
  if((in_msg_struct = ns_msg1__str__unpack(NULL, packed_msg_len, (void *)resp)) == NULL) {
    NJVM_ERROR(thread_id, "Invalid message recieved, Failed to unpack");
    return NULL;
  }
  
  if(in_msg_struct->field1 == NULL) {
    ns_msg1__str__free_unpacked(in_msg_struct, NULL); 
    return NULL;
  }

  len = strlen(in_msg_struct->field1);

  //if response length is greater than available buffer then realloc.
  if(len > ltd->buf_length) {
    ltd->buf = realloc(ltd->buf, len + 1);
    ltd->buf_length = len+1;
  }  
  
  //Copy this result to thread buffer. 
  strcpy(ltd->buf, in_msg_struct->field1);
  ns_msg1__str__free_unpacked(in_msg_struct, NULL);

  return ltd->buf;
}

int ns_save_string(char *var_value, char *var_name)
{
  ThreadData *ltd = (ThreadData *)pthread_getspecific(thread_key);
  int thread_id = ltd->thread_id;
  int packed_msg_len;

  NsMsg2SS out_msg_struct = NS_MSG2__SS__INIT; 
   
  //prepare request.
  out_msg_struct.field1 = var_value;
  out_msg_struct.field2 = var_name;

  packed_msg_len = ns_msg2__ss__get_packed_size(&out_msg_struct);
  NJVM_DEBUG(thread_id, "packed message size = %d", packed_msg_len);

  if(packed_msg_len + NJVM_HDR_SIZE > ltd->buf_length) {
    ltd->buf = realloc(ltd->buf, packed_msg_len + NJVM_HDR_SIZE);
    ltd->buf_length = packed_msg_len + NJVM_HDR_SIZE;
  }  
 
  if(ns_msg2__ss__pack(&out_msg_struct, (void *)(ltd->buf + NJVM_HDR_SIZE)) != packed_msg_len) {
    NJVM_ERROR(thread_id, "Failed tp pack ns_save_string() message");
    pthread_exit(NULL);
  } 
  SET_NJVM_HDR(ltd->buf, 1015, packed_msg_len);
  //send this message
  NJVM_DEBUG(thread_id, "ns_save_string() request sending.");
  if(write_msg(ltd, (packed_msg_len + NJVM_HDR_SIZE))) {
    NJVM_ERROR(thread_id, "Failed to send ns_save_string()");
   pthread_exit(NULL);
  } 
  //read msg
  char *resp;
  int len;
  resp = read_msg(ltd, &len);
  if(resp == NULL) {
    NJVM_ERROR(thread_id, "Connection closed from other end, exiting thread");
    pthread_exit(NULL);
  }

  NJVM_DEBUG(thread_id, "ns_save_string() response recieved");
  return 0;
} 
 
int ns_web_url(int page_id)
{
  ThreadData *ltd;

  //Get thread data from key
  ltd = (ThreadData *)pthread_getspecific(thread_key);

  NsMsg1Int pageid = NS_MSG1__INT__INIT;  
  int packed_msg_len;
  int thread_id = ltd->thread_id;
  char *proc_buf = ltd->buf;
  int len;
  char * resp;

  pageid.field1 = page_id;
  packed_msg_len = ns_msg1__int__get_packed_size(&pageid);
  NJVM_DEBUG(thread_id, "packed message size = %d", packed_msg_len);
 
  if(ns_msg1__int__pack(&pageid, (void *)(proc_buf + NJVM_HDR_SIZE)) != packed_msg_len) {
    //what to do
    NJVM_ERROR(thread_id, "Failed to pack ns_web_url() message"); 
    pthread_exit(NULL);
  }  
  SET_NJVM_HDR(proc_buf, 1003, packed_msg_len);
  //send this message 
  NJVM_DEBUG(thread_id, "ns_web_url() request sending.");
  if(write_msg(ltd, (packed_msg_len + NJVM_HDR_SIZE))) {
    NJVM_ERROR(thread_id, "Failed to send ns_web_url() request");
    pthread_exit(NULL);
  }
  //now wait for response.
  resp = read_msg(ltd, &len);
  if(resp == NULL) {
    NJVM_ERROR(thread_id, "Connection closed from other end, closing this thread.");
    pthread_exit(NULL);
  }
  NJVM_DEBUG(thread_id, "ns_web_url() response recieved.");
  //opcode = *(int *)(resp + sizeof(int));
  //currently we are not validating message. not even unpacking the response 
  //do we need to do that.
  return 0; 
}


int ns_page_think_time(double pg_time)
{
  ThreadData *ltd = (ThreadData *)pthread_getspecific(thread_key);
  NsMsg1Double pgtime = NS_MSG1__DOUBLE__INIT;  
  int packed_msg_len;
  int thread_id = ltd->thread_id;
  char *proc_buf = ltd->buf;
  int len;
  char * resp;

  pgtime.field1 = pg_time;
  packed_msg_len = ns_msg1__double__get_packed_size(&pgtime);
  NJVM_DEBUG(thread_id, "packed message size = %d", packed_msg_len);
 
  if(ns_msg1__double__pack(&pgtime, (void *)(proc_buf + NJVM_HDR_SIZE)) != packed_msg_len) {
    //what to do
    NJVM_ERROR(thread_id, "Failed to pack ns_web_url() message"); 
    pthread_exit(NULL);
  }  
  SET_NJVM_HDR(proc_buf, 1004, packed_msg_len);
  //send this message 
  NJVM_DEBUG(thread_id, "ns_page_think_time() request sending.");
  if(write_msg(ltd, (packed_msg_len + NJVM_HDR_SIZE))) {
    NJVM_ERROR(thread_id, "Failed to send ns_web_url() request");
    pthread_exit(NULL);
  }
  //now wait for response.
  resp = read_msg(ltd, &len);
  if(resp == NULL) {
    NJVM_ERROR(thread_id, "Connection closed from other end, closing this thread.");
    pthread_exit(NULL);
  }
  NJVM_DEBUG(thread_id, "ns_page_think_time() response recieved.");
  //opcode = *(int *)(resp + sizeof(int));
  //currently we are not validating message. not even unpacking the response 
  //do we need to do that.
  return 0; 
}

int ns_exit_session()
{
  ThreadData *ltd = (ThreadData *)pthread_getspecific(thread_key);
  int thread_id = ltd->thread_id;
  char *proc_buf = ltd->buf;
  int len;
  char * resp;

  //there is no argument. so just send header.
  SET_NJVM_HDR(proc_buf, 1099, 0);
  //send this message 
  NJVM_DEBUG(thread_id, "ns_exit_session() request sending.");
  if(write_msg(ltd, (0 + NJVM_HDR_SIZE))) {
    NJVM_ERROR(thread_id, "Failed to send ns_web_url() request");
    pthread_exit(NULL);
  }
  //now wait for response.
  resp = read_msg(ltd, &len);
  if(resp == NULL) {
    NJVM_ERROR(thread_id, "Connection closed from other end, closing this thread.");
    pthread_exit(NULL);
  }
  NJVM_DEBUG(thread_id, "ns_page_think_time() response recieved.");
  //opcode = *(int *)(resp + sizeof(int));
  //currently we are not validating message. not even unpacking the response 
  //do we need to do that.
  return 0; 
}

#define INFINITE_TASKS1 2
#define INFINITE_TASKS2 4
//define tasks here, like ns_web_url or whatever.
static inline void  thread_assignment(ThreadData *ltd, int task_mode)
{
  while(1) {
    ns_web_url(0);

    if(task_mode != INFINITE_TASKS1 && task_mode != INFINITE_TASKS2)
      break;
  }
  ns_exit_session();
}

#define START_USER_OPCODE 1000 

void *njvm_thread_func(void *arg)
{
  ThreadData *ltd = (ThreadData *)arg;
  int thread_id = ltd->thread_id;
 
  //set local attribute to key
  pthread_setspecific( thread_key, (void *)ltd); 
 
  //Create connection to nvm.
  if(nvm_port != 0) {
		if((ltd->fd = nslb_tcp_client(nvm_ip, nvm_port)) == -1) 
		{
			NJVM_ERROR(thread_id, "jthread failed to connect to NVM, error = %s", nslb_strerror(errno));
			pthread_exit(NULL);
		}
  }
  else {
		if((ltd->fd = nslb_tcp_client_unixs(nvm_ip)) == -1) 
		{
			NJVM_ERROR(thread_id, "jthread failed to connect to NVM, error = %s", nslb_strerror(errno));
			pthread_exit(NULL);
		}
  }

  thread_done_string[thread_id - 1] = 1;
  
  NJVM_DEBUG(thread_id, "successfully connected to nvm");

  // check for flag
  int flags = fcntl(ltd->fd, F_GETFL, 0);
  if(flags == -1) {
    NJVM_ERROR(thread_id, "Failed to get socket flag");
    pthread_exit(NULL);
  }
  if((flags & O_NONBLOCK) == O_NONBLOCK) {
    NJVM_DEBUG(thread_id, "connection to njvm is non blocking");
    if(fcntl(ltd->fd, F_SETFL, flags & ~O_NONBLOCK) == -1) {
      NJVM_ERROR(thread_id, "Failed to set BLOCING flag");
      pthread_exit(NULL);
    }
  }
 
  //set read_buf and write_buf
  ltd->buf = malloc(512);
  ltd->buf_length = 512;

  //wait for start user. on recieve do task and continue waiting for next start_user signal.
  while(1) {
    NJVM_DEBUG(thread_id, "Waiting for next assignment on fd = %d", ltd->fd);
    //wait for command.
    int len;
    char *resp;
    int opcode;
    resp = read_msg(ltd, &len);
    if(resp == NULL) {
      NJVM_ERROR(thread_id, "Connection closed from other end, closing this thread.");
      pthread_exit(NULL);
    }
    opcode = *(int *)(resp + 4);
    NJVM_DEBUG(thread_id, "opcode recieved = %4d", opcode);
    //just safty check
    if(opcode != START_USER_OPCODE) {
      NJVM_ERROR(thread_id, "Invalid user init msg.");
      continue;
    } 
    NJVM_DEBUG(thread_id, "Got assignment");
    //thread_assignment(ltd, g_task_mode); 

    //This is the flow function from flow file.
    if(flow_func) { 
      NJVM_DEBUG(thread_id, "Running flow");
      flow_func();
    }
    else 
     thread_assignment(ltd, g_task_mode); 
  }
  return NULL; 
}

//create thread pools
int init_njvm_threads(int num_thread)
{
  static int key_created = 0;
  static int thread_count = 0;
  
  NJVM_DEBUG(0, "Method called");
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
  pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
  //check if we need to set stack size. because default size is very big.

  ThreadData *td; 

  td = malloc(sizeof(ThreadData) * num_thread);
  memset(td, 0, sizeof(ThreadData) * num_thread);

  //Create thread key.
  if(!key_created) {
    NJVM_DEBUG(0, "pthread key creating");
    pthread_key_create(&thread_key, NULL);
    key_created = 1;
  }
 
  int i = 0;
  for(i = 0; i<num_thread; i++) {
    td[i].thread_id = ++thread_count;
    if(pthread_create(&(td[i].thread_ptr), &attr, njvm_thread_func,  &td[i])) {
       NJVM_ERROR(0, "Failed to create thread, error = %s", nslb_strerror(errno));
       NS_EXIT(-1, "Failed to create thread, error = %s", nslb_strerror(errno));
    }
    //add in free_td list
    td[i].next = free_td;
    free_td = &(td[i]);
    num_free_td++; 
    NJVM_DEBUG(0, "Thread[%d] successfully created", thread_count);
  }
  return 0; 
}


static char *read_msg(ThreadData *td, int *msg_len)
{
  //first read first 4 bytes to get message size.
  int offset = 0;
  int remain_bytes = 4;
  int ret;
  int thread_id = td->thread_id;
  int fd = td->fd;
  char *msg_buf = td->buf; 
  

  while(remain_bytes > 0)  {
    ret = read(fd, msg_buf + offset, remain_bytes);
    if(ret < 0) {
      if(errno == EINTR)
        continue;
      else  {
        NJVM_ERROR(thread_id, "Failed to read from njvm, fd = %d, error = %s", fd, nslb_strerror(errno));
        return NULL;    
      }
    }
    if(ret == 0) {
      NJVM_ERROR(thread_id, "Failed to read, connection closed from other size");
      return NULL;
    }
    remain_bytes -= ret;
    offset += ret;
  }
  
  //get size of message.
   
  *msg_len = (*(int *)(msg_buf)) + 4;
  NJVM_DEBUG(thread_id, "Message size = %d", *msg_len);
  
  //If message length is reached to maximum then realloc
  if(*msg_len > td->buf_length) {
    NJVM_DEBUG(thread_id, "Reallocing read_buf to length = %d", *msg_len);
    td->buf = msg_buf = realloc(td->buf, *msg_len);
    td->buf_length = *msg_len;
  }

  remain_bytes = (*msg_len) - 4; //as 4 bytes already read
  offset = 4; 
 
  //again play the same loop
  while(remain_bytes > 0)  {
    ret = read(fd, msg_buf + offset, remain_bytes);
    if(ret < 0) {
      if(errno == EINTR)
        continue;
      else  {
        NJVM_ERROR(thread_id, "Failed to read from njvm, fd = %d, error = %s", fd, nslb_strerror(errno));
        return NULL;    
      }
    }
    if(ret == 0) {
      NJVM_ERROR(thread_id, "Failed to read, connection closed from other size");
      return NULL;
    }
    remain_bytes -= ret;
    offset += ret;
  }
  NJVM_DEBUG(thread_id, "Message successfully read");
  return msg_buf; 
}

static int write_msg(ThreadData *td, int len)
{
  int bytes_remain = len;
  int offset = 0;
  int ret;
  int thread_id = td->thread_id;
  char *msg = td->buf;
  int fd = td->fd;
 
  
  NJVM_DEBUG(thread_id, "Method called, msg len = %d", len);
  while(bytes_remain > 0)
  {
    ret = write(fd, msg+offset, bytes_remain);
    if(ret < 0)  {
      NJVM_ERROR(thread_id, "Failed to write to nvm, error = %s", nslb_strerror(errno));
      return -1;
    }
    bytes_remain -= ret;
    offset += ret;  
  }
  NJVM_DEBUG(thread_id, "Message successfully written");
  return 0;
}

int send_bind_message()
{
  //create bind message
  int msg_len;
  char *buf = ct.buf;
  
  NsMsg1Int msg = NS_MSG1__INT__INIT;
  msg.field1 = 0;  //set successful.
  int p_msg_size = ns_msg1__int__get_packed_size(&msg);
  //pack the message.
  if(ns_msg1__int__pack(&msg, (void *)buf + NJVM_HDR_SIZE) != p_msg_size)
  {
    NJVM_ERROR(0, "Failed to pack bind message");
    NS_EXIT(-1, "Failed to pack bind message");
  }
   
  NJVM_DEBUG(0, "bind message successfully packed, size = %d", p_msg_size);
  //set size and opcode header.
  *(int *)(buf) = p_msg_size + (NJVM_HDR_SIZE - 4);
  *(int *)(buf + 4) = 1001; 

  msg_len = p_msg_size + NJVM_HDR_SIZE;
  //send to njvm.
  if(write_msg(&ct, msg_len)) {
    NJVM_ERROR(0, "Failed to send bind request");
    return -1;
  }
  
  //read bind response.
  if(!read_msg(&ct, &msg_len)) {
    NJVM_ERROR(0, "Failed to recieve bind response");
    return -1;
  }
  NJVM_DEBUG(0, "Bind message successfully recieved");
  //for now we are not verifying message. 
  return 0; 
}

static void set_log_files()
{
  char filepath[1024];
  if(testrun != -1) {
    char *ns_wdir = getenv("NS_WDIR");
    if(ns_wdir == NULL) {
      NJVM_ERROR(0, "NS_WDIR not set");
      NS_EXIT(-1, "NS_WDIR not set");
    }
    //set debug log and error log in tr directory.
    if(debug_flag) {
      sprintf(filepath, "%s/webapps/logs/TR%d/njvm_simulator.debug", ns_wdir, testrun);
      debug_fp = fopen(filepath, "w");
      if(debug_fp == NULL) debug_fp = stdout;
    }
    
    sprintf(filepath, "%s/webapps/logs/TR%d/njvm_simulator.error", ns_wdir, testrun);
    error_fp = fopen(filepath, "w");
    if(error_fp == NULL) error_fp = stderr;
  }
}

int nslb_tcp_client_unixs(char *sock_path)
{
  int fd;
  
  fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if(fd < 0) {
    NJVM_ERROR(0, "Failed to create socket, err msg: %s", nslb_strerror(errno));
    return -1;
  }

  //set address to connect
  struct sockaddr_un sock_addr;
  memset(&sock_addr, 0, sizeof(struct sockaddr_un));
  sock_addr.sun_family = AF_UNIX;
  strcpy(sock_addr.sun_path, sock_path);
  int len = strlen(sock_addr.sun_path) + sizeof(sock_addr.sun_family); 
  
  //now connect to address
  if(connect(fd, (struct sockaddr *)&sock_addr, len) < 0) {
    NJVM_ERROR(0, "Failed to connect to address = %s, error = %s", sock_path, nslb_strerror(errno));
    return -1;
  }
  return fd;
}


//Load flow function from flow file.
static void load_flow_function()
{
  if(!strcasecmp(script_file, "NA")) {
    NJVM_DEBUG(0, "Script file not given will be use default flow.");
    return;
  }
  struct stat sf_stat;
  if(stat(script_file, &sf_stat)) {
    NJVM_ERROR(0, "Failed to open flow file.");
    NS_EXIT(-1, "Failed to open flow file.");
  }
  //compile this file.
  //Currently we are using header file, and flow file should include that header file.
  char cmd[2* 1024] = "";
  int ret;
  char err_file[512];
  char so_file[512];
  void *handler;
  char err_msg[1024] = "\0";
 
  sprintf(err_file, "/tmp/sim_flow_err.%d", getpid());
  sprintf(so_file, "/tmp/simulator_flow_%d.so", getpid());
  sprintf(cmd, "gcc -g -Wall -shared -o %s -I%s/include %s -fpic 2>%s", 
                    so_file, getenv("NS_WDIR"), script_file, err_file);

  NJVM_DEBUG(0, "commmand to compile script = \'%s\'", cmd);
  //Ignore sigchild 
  ret = nslb_system(cmd,1,err_msg);
  if(ret != 0) {
    NJVM_ERROR(0, "Failed to compile flow file \'%s\', error are saved in file \'%s\'", script_file, err_file);
    NS_EXIT(-1, "Failed to compile flow file \'%s\', error are saved in file \'%s\'", script_file, err_file);
  }  
  
  //delete err file
  unlink(err_file);
  
  //load so file.
  handler = dlopen(so_file, RTLD_NOW);
  if(handler == NULL) {
    NJVM_ERROR(0, "Failed to open flow shared library \'%s\'", so_file);
    NS_EXIT(-1, "Failed to open flow shared library \'%s\'", so_file);
  }
   
  //once so file loaded unlink so file from tmp
  unlink(so_file); 

  //load flow function.
  flow_func = dlsym(handler, "flow");    
  if(flow_func == NULL) {
    NJVM_ERROR(0, "Failed to get flow() function from flow file \'%s\'", script_file);
    NS_EXIT(-1, "Failed to get flow() function from flow file \'%s\'", script_file);
  }
  NJVM_DEBUG(0, "Flow function have been loaded successfully");
}

static inline void handle_increase_thrd_msg()
{
  //get thread count.
  NsMsg1Int *in_msg_struct;
  int packed_msg_len;
  char *msg;
  int incr_thrd_count = 0;

  NJVM_DEBUG(0, "Increase thread message recieved");
  msg = ct.buf + NJVM_HDR_SIZE;
  packed_msg_len = (*(int *)ct.buf) - (NJVM_HDR_SIZE - sizeof(int));

  if((in_msg_struct = ns_msg1__int__unpack(NULL, packed_msg_len, (void *)msg)) == NULL) {
    NJVM_ERROR(0, "Invalid message recieved, Failed to unpack");
    return;
  }
  
  incr_thrd_count = in_msg_struct->field1; 

  //free unpacked msg 
  ns_msg1__int__free_unpacked(in_msg_struct, NULL);
  
  //init_threads.
  NJVM_DEBUG(0, "Increasing threads by = %d", incr_thrd_count);
  init_njvm_threads(incr_thrd_count);
  NJVM_DEBUG(0, "Request threads %d successfully created", incr_thrd_count); 
}

static inline void process_control_thrd_msg()
{
  //Check for opcode
  int opcode;
  
  opcode = *(int *)(ct.buf + 4); 
  //Currently we are just handling increase thread msg.
  if(opcode == 1055)
    handle_increase_thrd_msg();
  else {
    NJVM_DEBUG(0, "message recieved with opcode %d not handled", opcode);
  }
}

#define ALL_THREAD_CONNECTION_NOT_DONE memcmp(thread_compare_string, thread_done_string, num_thread)

int main(int argc, char *argv[])
{
  njvm_start_time = time(NULL);
  debug_fp = stdout;
  error_fp = stderr;
  parse_args(argc, argv); 

  set_log_files(); 
  NJVM_DEBUG(0, "nvm_ip = %s, nvm_port = %d, num_thread = %d, task_mode = %d", nvm_ip, nvm_port, num_thread, g_task_mode);
  //create control connectionu.
  //means case of unix domain.
  if(nvm_port == 0) {
    NJVM_DEBUG(0, "Unix domain socket used");
    //nvm_ip is address for unix socket address.
    if((control_fd = nslb_tcp_client_unixs(nvm_ip)) == -1) {
      NJVM_ERROR(0, "Failed to connect to nvm");
      NS_EXIT(-1, "Failed to connect to nvm");
    }
  } 
  else {
    if((control_fd = nslb_tcp_client(nvm_ip, nvm_port)) == -1) 
    {
      NJVM_ERROR(0, "Failed to connect to nvm");
      NS_EXIT(-1, "Failed to connect to nvm"); 
    }   
  }

  //get flow method.
  load_flow_function(); 

/********Initialize ct*************/
  memset(&ct, 0, sizeof(ThreadData));
  ct.fd = control_fd;
  ct.thread_id = 0;
  ct.buf = malloc(512);
  ct.buf_length = 512;
/************/


/**************This code can be replace by semaphore *************/
  thread_done_string = malloc(num_thread);
  memset(thread_done_string, 0, num_thread);

  thread_compare_string = malloc(num_thread);
  memset(thread_compare_string, 1, num_thread);
/**************************/

  init_njvm_threads(num_thread);
   
  while(ALL_THREAD_CONNECTION_NOT_DONE)
  {
    usleep(500);
  }
  sleep(2); 
  //now thread created, it's time to send BIND message using protobuf.
  if(send_bind_message() != 0)
  {
     NJVM_ERROR(0, "Failed to bind nvm");
     NS_EXIT(-1, "Failed to bind nvm");
  }
 
   
  NJVM_DEBUG(0, "I am also going to sleep");
  char *msg;
  int msg_len;
  //wait for somwthig.
  while(1) {
    msg = read_msg(&ct, &msg_len);
    if(msg == NULL) {
      NJVM_ERROR(0, "control connection closed, closing..");
      NS_EXIT(0, "control connection closed, closing..");
    }
    //This will process control thread messages
    process_control_thrd_msg(&ct, msg_len);
  }
}
