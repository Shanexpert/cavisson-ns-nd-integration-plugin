
//gcc -g -m64 -fgnu89-inline -Wall -DUbuntu -DRELEASE=1604 -DENABLE_JIFFY_TS -DENABLE_WAN_EX -DPG_BULKLOAD -DNS_TIME -DNS_USE_MODEM -DUSE_EPOLL -DENABLE_SSL -DCAV_SSL_ATTACK -c -I. -I/usr/include/ -I/usr/include/gsl -I/usr/include/postgresql -I/home/anil/WORK/cavisson/base/include/libnscore -I/home/anil/WORK/cavisson/base/include/libnstopo -I/home/anil/WORK/cavisson/base/include/xml2 -I/home/anil/WORK/cavisson/base/include/cprops -I/home/anil/WORK/cavisson/base/include/protobuf-c -I/home/anil/WORK/cavisson/base/include/protobuf -I/home/anil/WORK/cavisson/base/include/mongodb -I/home/anil/WORK/cavisson/base/include/cassdb -I/home/anil/WORK/cavisson/base/include/jms -I/home/anil/WORK/cavisson/base/include/jms -I/home/anil/WORK/cavisson/base/include/jms -I/home/anil/WORK/cavisson/base/include/brotli/c -I/home/anil/WORK/cavisson/base/include/brotli/c/include -I/home/anil/WORK/cavisson/base/include/openssl/openssl-1.0.2h/include -I/home/anil/WORK/cavisson/base/include/ntlm/libntlm-1.3 -I/home/anil/WORK/cavisson/base/include/http2 -I/home/anil/WORK/cavisson/base/include/magic -I/home/anil/WORK/cavisson/base/include/ssh -I/home/anil/WORK/cavisson/base/include/js -I/home/anil/WORK/cavisson/base/include/ldap  -o /home/anil/WORK/cavisson/prod-src/core/netstorm/src/build/obj/non_debug/test_rpr.o test_rpr.c 2>err

//gcc -o /home/anil/WORK_GIT/cavisson/prod-src/core/netstorm/src/build/bin/nsu_rpr /home/anil/WORK_GIT/cavisson/prod-src/core/netstorm/src/build/obj/debug/ns_rpr.o /home/anil/WORK_GIT/cavisson/prod-src/core/netstorm/src/build/obj/debug/hash.o /home/anil/WORK_GIT/cavisson/prod-src/core/netstorm/src/build/obj/debug/decomp.o         /home/anil/WORK_GIT/cavisson/prod-src/core/netstorm/src/build/obj/debug/ns_amf_encode_for_nsi_amf.o /home/anil/WORK_GIT/cavisson/prod-src/core/netstorm/src/build/obj/debug/ns_amf_decode_for_nsi_amf.o         /home/anil/WORK_GIT/cavisson/prod-src/core/netstorm/src/build/obj/debug/ns_amf_common_for_nsi_amf.o /home/anil/WORK_GIT/cavisson/prod-src/core/netstorm/src/build/obj/debug/ns_amf_ext_object_for_nsi_amf.o         /home/anil/WORK_GIT/cavisson/prod-src/core/netstorm/src/build/obj/debug/ns_rpr_util.o "-Wl,-rpath,thirdparty/lib" -DNS_DEBUG_ON -DDB_DEBUG_ON -g -m64 -fgnu89-inline -Wall -DUbuntu -DRELEASE=1604 -DENABLE_JIFFY_TS -DENABLE_WAN_EX -DPG_BULKLOAD -DNS_TIME -DNS_USE_MODEM -DUSE_EPOLL -DENABLE_SSL -DCAV_SSL_ATTACK -I. -I/usr/include/ -I/usr/include/gsl -I/usr/include/postgresql -I/home/anil/WORK_GIT/cavisson/base/include/libnscore -I/home/anil/WORK_GIT/cavisson/base/include/libnstopo -I/home/anil/WORK_GIT/cavisson/base/include/xml2 -I/home/anil/WORK_GIT/cavisson/base/include/cprops -I/home/anil/WORK_GIT/cavisson/base/include/protobuf-c -I/home/anil/WORK_GIT/cavisson/base/include/protobuf -I/home/anil/WORK_GIT/cavisson/base/include/mongodb -I/home/anil/WORK_GIT/cavisson/base/include/cassdb -I/home/anil/WORK_GIT/cavisson/base/include/jms -I/home/anil/WORK_GIT/cavisson/base/include/jms -I/home/anil/WORK_GIT/cavisson/base/include/jms -I/home/anil/WORK_GIT/cavisson/base/include/brotli/c -I/home/anil/WORK_GIT/cavisson/base/include/brotli/c/include -I/home/anil/WORK_GIT/cavisson/base/include/openssl/openssl-1.0.2h/include -I/home/anil/WORK_GIT/cavisson/base/include/ntlm/libntlm-1.3 -I/home/anil/WORK_GIT/cavisson/base/include/http2 -I/home/anil/WORK_GIT/cavisson/base/include/magic -I/home/anil/WORK_GIT/cavisson/base/include/ssh -I/home/anil/WORK_GIT/cavisson/base/include/js -I/home/anil/WORK_GIT/cavisson/base/include/ldap  -L/home/anil/WORK_GIT/cavisson/base/lib -lnstopo_debug -lnscore_debug -lnghttp2 -lmagic -lxml2_cav -lcprops -lprotobuf-c -lprotobuf -lprotoc -lmongoc-1.0 -lbson-1.0 -lcassandra -luv -lmqe_r -lmqic_r -ltibems64 -lrdkafka -lbrotlicommon -lbrotlienc -lbrotlidec -lssl -lcrypto -lntlm -lssh -ljs -lodbc -lldap -lcrypt -lpam -lnsc++ -L/usr/lib64 -L/usr/lib/x86_64-linux-gnu/ -lpthread -lgsl -lgslcblas -lm -lpq -ldl -lz -lm -lcurl -lrt -lgssapi_krb5 -lstdc++ /usr/lib/x86_64-linux-gnu/libdb-5.3.a



#define _GNU_SOURCE
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/prctl.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <asm/poll.h>
#include <linux/unistd.h>
#include <sys/epoll.h>
#include <asm/unistd.h>
#include <math.h>
#include <sys/sendfile.h>
#include <netinet/tcp.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <stdarg.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "tmr.h"
#include "nslb_util.h"
#include "nslb_sock.h"
#include "ns_data_types.h"
#include "nslb_ssl_lib.h"
#include "ns_rpr.h"
#include "ns_rpr_util.h"
#include "nslb_log.h"
#include "nslb_encode_decode.h"

//static int *child_table;
typedef struct {
  pid_t pid;
  int spawn_count;
  u_ns_ts_t last_spawn_time;
  unsigned short min_port;
  unsigned short max_port;
  unsigned short cur_port;
} child_table_t;

unsigned int total_max_epoll_fds = 0;

int num_socket_port_service_list;
//SOCKET TCP/UDP service
static listener *socket_tcp_udp_lfd = NULL;
static listener *socket_tcps_udp_lfd = NULL;
int v_epoll_fd;
int g_msg_com_listen_epfd;

//static char rpr_conf_dir[MAX_FILE_NAME_LEN];
static char g_rpr_script_dir[MAX_FILE_NAME_LEN];
static char g_rpr_home_dir[MAX_FILE_NAME_LEN];
//static char g_rpr_logs_dir[MAX_FILE_NAME_LEN];
//static char rpr_script_dir[MAX_FILE_NAME_LEN];
int g_rpr_num_childs = 0; // Number of childs

int total_req = 0;
int g_req_count = 0;
int g_resp_count = 0;

//long g_rpr_timeout = 900000;
long g_rpr_timeout = 1800;
char *g_rpr_script_name="_newSocketScript";
char *conf_file_name_list[MAX_TOKEN];
int conf_file_name_count = 0;
/*
int debug_log = 0;
int error_fd = -1;
int debug_fd = -1;
unsigned int max_error_log_file_size = DEFAULT_MAX_ERROR_FILE_SIZE;  // Approx 10 MB
unsigned int max_debug_log_file_size = DEFAULT_MAX_DEBUG_FILE_SIZE;  // Approx 100MB
*/

pid_t parent_pid;
/*
int child_info_shmid = 0;
ChildInfo *child_info_shm = NULL;
*/
static connection* gFreeVuserHead = NULL;
static connection* gFreeVuserTail = NULL;  //To Add free user slot at tail.
static connection* gBusyVuserHead = NULL;
static connection* gBusyVuserTail = NULL;
static int gFreeVuserCnt;
static int gBusyVuserCnt;
static int gFreeVuserMinCnt;

int ns_parent_started=0; /*TST */
int ns_sickchild_pending=0; /*TST */
int sigterm_received = 0;
int g_rpr_id = 0;                    //ID 0 used by parent. Used in debug/error log file name

child_table_t *child_table = NULL;

//LOCK file concept
char lock_fname[1024];
int lock_fd = -1;

// Save requset and Response time to calculate service time
struct timeval req_time, res_time;
double svc_time = 0;
unsigned long req_time_in_ms = 0;

#if 0
char *rpr_get_cur_date_time()
{
  time_t  tloc;
  struct  tm *lt;
  static  char cur_date_time[100];
  sigset_t sigset, oldset;

  (void)time(&tloc);
  /*In case of HPD recovery, while running hpd with debug it uses localtime function which acquire mutux lock 
    to compute time.Now if sick child is receive then signal the handler also calls localtime(), 
    it causes localtime to take the mutex. The mutex is already locked by the same thread. Hence causes Deadlock. 
    Solution: Block all signals while getting current date time stamp 
  */
  sigemptyset(&sigset);
  sigfillset(&sigset);
  sigprocmask(SIG_BLOCK, &sigset, &oldset);
  lt = localtime(&tloc);
  sigprocmask(SIG_UNBLOCK, &sigset, &oldset);

  if(lt  == (struct tm *)NULL)
    strcpy(cur_date_time, "Error");
  else
    sprintf(cur_date_time, "%02d/%02d/%02d %02d:%02d:%02d", 1900 + lt->tm_year, lt->tm_mon + 1, lt->tm_mday, lt->tm_hour, lt->tm_min, lt->tm_sec);
  return(cur_date_time);
}

/*
  This method will :
    - create the error and debug log file in the append_mode
    - get error and debug log file size using stat
    - move log file to .prev, when debug or error log file is greater than max size of debug or error log file
    - call when debug or error log file already open
*/
static
void open_log(char *name, int *fd, unsigned int max_size, char *header)
{
  char log_file[1024], log_file_prev[1024];
  struct stat stat_buf;
  int status;

  sprintf(log_file, "%s/logs/%s", rpr_conf_dir, name);
  //printf("In side %s for file %s\n", (char *)__FUNCTION__, log_file);
  // get debug or error log file size using stat and check this size > max size of debug or error log file
  if((stat(log_file, &stat_buf) == 0) && (stat_buf.st_size > max_size))
  {
   // check if fd is open, close it
    if(*fd > 0)
    {
      close(*fd);
      *fd = -1;
    }
    sprintf(log_file_prev, "%s.prev", log_file);

    // Never use debug_log from here
    if (debug_log) printf("Moving file %s with size %lu to %s, Max size = %u\n", log_file, stat_buf.st_size, log_file_prev, max_size);
   status = rename(log_file, log_file_prev);
   if(status < 0)
    // Never use debug_log from here
     fprintf(stderr, "Error in moving '%s' file, err = %s\n", log_file, strerror(errno));
  }

  if (*fd < 0 ) //if fd is not open then open it
  {
    *fd = open (log_file, O_CREAT|O_WRONLY|O_APPEND|O_CLOEXEC, 00666);
    if (*fd < 0)
{
      fprintf(stderr, "Error: Error in opening file '%s', fd = %d, Error = '%s'\n", log_file, *fd, strerror(errno));
      exit (-1);
    }
    // To set debug file for hessian
   /* if(strncmp(name, "hpd_debug", 9) == 0){
      FILE *fp;  
      fp = fdopen(*fd, "a");
      if(fp == NULL)
      {
        fprintf(stderr, "Error: Error in fdopen(%s). Error = %s", log_file, strerror(errno));
      }
      else
        hessian_set_debug_log_fp(fp);
        nslb_util_set_debug_log_fp(fp);
    }*/
    if(header)
    {
      write(debug_fd, DEBUG_HEADER, strlen(DEBUG_HEADER));
      write(error_fd, DEBUG_HEADER, strlen(DEBUG_HEADER));
    }
  }
}

void rpr_error_log_ex(char *file, int line, char *fname, void *cptr_void, char *format, ...)
{
  va_list ap;
  char buffer[MAX_LOG_BUF_SIZE + 1];
  int amt_written1 = 0, amt_written = 0;
  char log_file[1024];
  connection *cptr = cptr_void;

  //if(log_level > g_err_log) return;

  if(cptr != NULL)
    amt_written1 = sprintf(buffer, "\n%s|%s|%d|%s|%d|%d|%u|%d|", rpr_get_cur_date_time(), file, line, fname, g_rpr_id, getpid(), cptr->fd, cptr->state);
  else
    amt_written1 = sprintf(buffer, "\n%s|%s|%d|%s|%d|%d|NA|NA|NA|", rpr_get_cur_date_time(), file, line, fname, g_rpr_id, getpid());

  va_start (ap, format);
  amt_written = vsnprintf(buffer + amt_written1, MAX_LOG_BUF_SIZE - amt_written1, format, ap);
  va_end(ap);

  buffer[MAX_LOG_BUF_SIZE] = 0;

  // In some cases, vsnprintf return -1 but data is copied in buffer
  if(amt_written < 0)
  {
    amt_written = strlen(buffer) - amt_written1;
  }

  if(amt_written > (MAX_LOG_BUF_SIZE - amt_written1))
  {
    amt_written = (MAX_LOG_BUF_SIZE - amt_written1);
  }

  //TODO: change the path of error log file to capture directory
  sprintf(log_file, "rpr_error.%d.log", g_rpr_id);
  open_log(log_file, &error_fd, max_error_log_file_size, DEBUG_HEADER);
  write(error_fd, buffer, amt_written + amt_written1);
}

/*
void rpr_exit(int exit_status, char *file, int line, char *fname, char *format, ...)
{
  char exit_status_file_name[ERROR_BUF_SIZE/4];
  FILE *exit_file_fp;
  va_list ap;
  char buffer[ERROR_BUF_SIZE + 1];
  int amt_written1, amt_written; 
  char *ptr;
  char rpr_conf_dir[1024] = "";  

  amt_written1 = sprintf(buffer, "\n%s|%s|%d|%s|", rpr_get_cur_date_time(), file, line, fname);

  va_start (ap, format);
  amt_written = vsnprintf(buffer + amt_written1, ERROR_BUF_SIZE - amt_written1, format, ap);
  va_end(ap);

  //TODO: change the path of exit status file to capture directory
  ptr = getenv("NS_WDIR");
  if(ptr)
    strcpy(rpr_conf_dir, ptr);
  else
  {
    RPR_DL(NULL, "NS_WDIR env variable is not set. Setting it to default value /home/cavisson/work");
    strcpy(rpr_conf_dir, "/home/cavisson/work");
  }

  buffer[amt_written + amt_written1] = '\0';
  sprintf(exit_status_file_name, "%s/logs/rpr_exit_status.log", rpr_conf_dir);
  exit_file_fp = fopen(exit_status_file_name, "w");

  fprintf(exit_file_fp, "%s\n", buffer);
  fprintf(stderr, "%s\n", buffer);
  rpr_error_log(file, line, fname, NULL, "%s", buffer + amt_written1);

  fclose(exit_file_fp);
  exit(exit_status);
}

void rpr_debug_log_ex(char *file, int line, char *fname, void *cptr_void, char *format, ...)
{
  va_list ap;
  char buffer[MAX_LOG_BUF_SIZE + 1];
  int amt_written1 = 0, amt_written = 0;
  char log_file[1024];

  connection *cptr = cptr_void;

  if(debug_log == 0)
    return;

  if(cptr != NULL)
    amt_written1 = sprintf(buffer, "\n%s|%s|%d|%s|%d|%d|%u|%d|%d|", rpr_get_cur_date_time(), file, line, fname, g_rpr_id, getpid(), cptr->req_id, cptr->fd, cptr->state);
  else
    amt_written1 = sprintf(buffer, "\n%s|%s|%d|%s|%d|%d|NA|NA|NA|", rpr_get_cur_date_time(), file, line, fname, g_rpr_id, getpid());

  va_start (ap, format);
  amt_written = vsnprintf(buffer + amt_written1, MAX_LOG_BUF_SIZE - amt_written1, format, ap);
  va_end(ap);

  buffer[MAX_LOG_BUF_SIZE] = 0;

  // In some cases, vsnprintf return -1 but data is copied in buffer
  if(amt_written < 0)
  {
    amt_written = strlen(buffer) - amt_written1;
  }

  if(amt_written > (MAX_LOG_BUF_SIZE - amt_written1))
  {
    amt_written = (MAX_LOG_BUF_SIZE - amt_written1);
  }

  sprintf(log_file, "rpr_debug.%d.log", g_rpr_id);
  open_log(log_file, &debug_fd, max_debug_log_file_size, DEBUG_HEADER);
  write(debug_fd, buffer, amt_written + amt_written1);
}
*/
#endif

/*
// This method is for binding the process to the particular CPU
int set_cpu_affinity(int index, pid_t child_pid)
{
  HPDDL2_CPU_AFFINITY(NULL, "Method called, with Index = %d, child_pid = %d, IP = %s", index, child_pid, cpu_affinity_ip_addr[index]);

  if(child_cpu_mask[index] == 0) // This means CPU num was not specified for child for this IP
  {
    HPDDL2_CPU_AFFINITY(NULL, "Child cpu mask is not defined for index = %d, child_pid = %d, IP = %s", index, child_pid, cpu_affinity_ip_addr[index]);
   return 1;
  }

  if(sched_setaffinity(child_pid, sizeof(unsigned long long), (cpu_set_t *)&child_cpu_mask[index]) < 0)
  {
    rpr_error_log(_FLN_, NULL, "set_cpu_affinity", NULL, "Error in setting CPU affinity for child index = %d, child_pid = %d, IP = %s, CPU mask is 0X%llX. Error = %s", index, child_pid, cpu_affinity_ip_addr[index], child_cpu_mask[index], strerror(errno));
    return(-1);
  }
  HPDDL2_CPU_AFFINITY(NULL, "Successfully set CPU affinity for child index = %d, child_pid = %d, IP = %s, CPU mask is 0X%llX", index, child_pid, cpu_affinity_ip_addr[index], child_cpu_mask[index]);

  return 0;
}
*/

/* This function convert host to netword and network to host */

#define is_bigendian(n) ((*(char *)&n) == 0)

/*Assumption:
    @in  =>  Input argument point to memory of int/short/long only
*/
void nslb_endianness(unsigned char *out, unsigned char *in, int bytes, char len_endianness)
{
  const int n = 0x01;  //To test endianness
  int m_arch = SLITTLE_ENDIAN;
  int i, j;

  if((&n)[0] == 0)  //It means machine is BigEndian
    m_arch = SBIG_ENDIAN;
  else
    m_arch = SLITTLE_ENDIAN;

  RPR_DL(NULL, "m_arch = %d, len_endianness = %d", m_arch, len_endianness);

  if(((m_arch == SBIG_ENDIAN) && (len_endianness == SBIG_ENDIAN)) ||
     ((m_arch == SLITTLE_ENDIAN) && (len_endianness == SLITTLE_ENDIAN)))
  {
    for(i = 0; i < bytes; i++)
    {
      out[i] = in[i];
    }
  }
  else   //Except BigEndian Arch always reverse the order
  {
    for(i = 0, j = (bytes - 1); i < bytes; i++, j--)
    {
      out[i] = in[j];
    }
  }
  for(i = 0; i < bytes; i++)
    RPR_DL(NULL, "out[%d] = 0x%x, in[%d] = 0x%x", i, out[i], i, in[i]);
}

/*
#define is_bigendian(n) ((*(char *)&n) == 0)

void nslb_endianness(unsigned char *out, unsigned char *in, int bytes, char len_endianness)
{
  //const int n = 0x01;  //To test endianness
  int i, j;

  if(len_endianness == SBIG_ENDIAN)
  {
    for(i = 0; i < bytes; i++)
    {
      out[i] = in[i];
    }
  }
  else
  {
    for(i = 0, j = (bytes - 1); i < bytes; i++, j--)
    {
      out[i] = in[j];
    }
  }
}
*/

//For for parent and child
static void handle_sigusr2( int sig )
{
  int i;
  RPR_DL(NULL, "Method called. Received SIGUSR2.");
  //hpd_read_conf_when_running();

  if(!g_rpr_id) {
    for(i = 0; i < g_rpr_num_childs; i++) {
      if(child_table[i].pid > 0) {
        kill(child_table[i].pid, SIGUSR2);
      }
    }
  }
}

//For parent
static void handle_sigusr1( int sig )
{
  int i;
  RPR_DL(NULL, "recorder signalled to stop. Stopping recorder...");
  for (i=0; i < g_rpr_num_childs; i++) {
    RPR_DL(NULL, "Sending sigint to %d", child_table[i].pid);
    if (child_table[i].pid > 0)
        if (kill(child_table[i].pid, SIGINT))
          rpr_error_log(_FLN_, NULL, "kill");
  }
}

//For parent
void kill_children(void) {
  int i;
  char process_desc[64];

  RPR_DL(NULL, "Method called");

  (void) signal( SIGCHLD, SIG_IGN );
  for (i = 0; i< g_rpr_num_childs; i++) {
    //printf ("killing... %d\n", v_port_table[i].pid);
    if (child_table[i].pid > 0) {
      //replacing waitpid with library api
      sprintf(process_desc, "rpr child process [%d]", i);
      nslb_kill_and_wait_for_pid_ex(child_table[i].pid, process_desc, 1, 5);
    }
  }
}

char* get_child_process_name(pid_t pid)
{
  int i;
  static char child_name[512];
  //First check for child.
  //Name should be HPD Child1
  for (i = 0; i < g_rpr_num_childs; i++)
  {
    if (child_table[i].pid == pid)
    {
      sprintf(child_name, "child%d", i+1);
      return child_name;
    }
  }
  return "Unknown";
}

static void log_child_exit_status(int status, pid_t pid)
{
  char *process_name = get_child_process_name(pid);
  //int status;
  RPR_DL(NULL, "Method called");

  if(WIFEXITED(status))
  {
    RPR_DL(NULL, "Child (%s) terminated normally. Exit status = %d", process_name, WEXITSTATUS(status));
    rpr_error_log(_FLN_, NULL, "Child (%s) terminated normally. Exit status = %d", process_name, WEXITSTATUS(status));
  }
  else if(WIFSIGNALED(status))
  {
    RPR_DL(NULL, "Child(%s)  terminated because of a signal which was not caught. Signal = %d", process_name, WTERMSIG(status));
    rpr_error_log(_FLN_, NULL, "Child(%s)  terminated because of a signal which was not caught. Signal = %d", process_name,  WTERMSIG(status));
  }
  else if(WIFSTOPPED(status))
  {
    RPR_DL(NULL, "Child(%s) which caused the return is currently stopped.   = %d", process_name, WSTOPSIG(status));
    rpr_error_log(_FLN_, NULL, "Child(%s) which caused the return is currently stopped.   = %d", process_name, WSTOPSIG(status));
  }
#ifdef WCOREDUMP 
  else if(WCOREDUMP(status))
  {
    RPR_DL(NULL, "Child process core dump is generated");
  }
#endif
}

#if 0
static void respawn_child(pid_t pid)
{
  int i;

  RPR_DL(NULL, "Method Called. pid = %d", pid);

  for (i = 0; i < g_rpr_num_childs; i++) {
    if (child_table[i].pid == pid)
      break;
  }

  RPR_DL(NULL, "Child id = %d", i);

  if (i == g_rpr_num_childs) {         /* can not happen */
    /* something is wrong; exit?? */
    rpr_error_log(_FLN_, NULL, "pid = %d is not one of ours. Ignoring", pid);
    return;
  }
  spawn_child_process(i);
}
#endif 

static void handle_sickchild( int sig )
{
  pid_t pid;
  int status;
  int i;

  //dirty clean
  RPR_DL(NULL, "Recived a sick child signal, ns_parent_started = %d, ns_sickchild_pending = %d", ns_parent_started, ns_sickchild_pending);
  //printf("Recived a sick child signal : (pid=%d parent_started=%d sick_pending=%d)\n", getpid(), ns_parent_started, ns_sickchild_pending);
  //printf("nun_child=%d pid=%d\n", globals.num_process, v_port_table[0].pid);
  if (!ns_parent_started) {
        ns_sickchild_pending = 1;
        signal (SIGCHLD, SIG_IGN);
        return;
  }

  pid = waitpid(-1, &status, WNOHANG);
  if (pid <= 0) return;

  log_child_exit_status(status, pid);
  if(!WIFEXITED(status)) {   
    //respawn_child(pid);
    //TODO: whether to respawn or not
  }
  else
  {
    for (i = 0; i < g_rpr_num_childs; i++) {
      if (child_table[i].pid == pid)
        break;
    }
    if(i != g_rpr_num_childs)
      child_table[i].pid = -1;    /* should be set so parent knows all childs are gone */

  }
}

//For children static void
static void handle_sigrtmin1(int sig)
{
  RPR_DL(NULL, "Child %d(%d) recieved SIGRTMIN+1, switching partition", g_rpr_id, getpid());
}

static void rpr_recording_timout(int sig)
{
  RPR_DL(NULL, "Recording timeout reached for pid %d.", getpid());
  //sigterm_received = 1;
  RPR_EXIT(0, "Stopping recording as Recording timeout reached for pid : %d", getpid());
}

//For children
static void handle_sigint( int sig )
{
  RPR_DL(NULL, "%d: interrupt Rcd", getpid());
  //sigterm_received = 1;
  RPR_EXIT(0, "%d: interrupt Rcd", getpid());
}

static void handle_sigterm( int sig )
{
  RPR_DL(NULL, "handle_sigterm_child called");
  //printf("%d: sigterm rcd\n", getpid());
  sigterm_received = 1;

  RPR_EXIT(0, "Sigterm signal received at Child, Process is Killed/Stopped by the User");
}

static void handle_sigpipe( int sig )
{
  /* Nothing special.  We only catch the signal so that syscalls return
  ** an error, instead of just exitting the process.
  */
  //  printf("Signal %d rcd\n", sig);
}

static void handle_sigterm_parent(int sig)
{
  RPR_DL(NULL, "Sigterm Signal Received");
  RPR_EXIT(0, "NetOcean Process is Killed/Stopped by the User");
}

#if 0
/* Function used for Initializing the random number generator */
u_ns_ts_t base_timestamp = 946684800;   /* approx time to substract from 1/1/70 */
  
u_ns_ts_t get_ms_stamp() {
  struct timeval want_time;
  u_ns_ts_t timestamp;
  
  
  gettimeofday(&want_time, NULL);
    

  timestamp = (want_time.tv_sec - base_timestamp)*1000 + (want_time.tv_usec / 1000);


  return timestamp;
}
#endif

void free_retrieve_data(connection *cptr, u_ns_ts_t now)
{
  struct copy_buffer* old_buff = NULL;
  struct copy_buffer* new_buff = cptr->buf_head;

  RPR_DL(cptr, "Method called");
  while(new_buff != NULL)
  {
    RPR_DL(cptr, "Freeing linked list buffer of body. Buffer = %s", new_buff->buffer);
    old_buff = new_buff;
    new_buff = new_buff->next;
    FREE_AND_MAKE_NOT_NULL(old_buff, cptr, "old_buff"); // Must NOT do NULL as we are checking old_buff below
  }
  cptr->buf_head = NULL;

  // In case of connection closed due to error, cur_buf may point to the last
  // node in the linked list which is freed above. So check if cur_buf is same
  // as last node of the linked list, then do NOT free it
  if(cptr->cur_buf != (void *)old_buff)
  {
    char *buff = (char *)cptr->cur_buf;
    FREE_AND_MAKE_NULL(buff, cptr, "cptr->cur_buf; Freeing big buffer having complete body");
    cptr->cur_buf = NULL;
  }
  else
  {
    cptr->cur_buf = NULL; // Must make it NULL as it was pointing to last node
    RPR_DL(cptr, "Not freeing cptr->cur_buf as it is pointing to last node of the linked list");
  }
}

#if 0
/*
  remove element from from our timer array
  idx: the index of the element you want to remove
 */
void dis_timer_del(timer_type* tmr)
{ 
  int type = tmr->timer_type;
  RPR_DL(NULL, "STS:del tmr: type=%d, tmr=%p", type, tmr);
  
  assert(type >= 0);
  
  if (tmr->next)
    tmr->next->prev = tmr->prev;
  else
    ab_timers[type].prev = tmr->prev;
  if (tmr->prev)
    tmr->prev->next = tmr->next;
  else
    ab_timers[type].next = tmr->next;
  
  tmr->timer_type = -1;
  RPR_DL(NULL, "STS:del tmr: type=%d, tmr=%p DONE", type, tmr);
}
#endif

//Print stack strace.
void print_trace ()
{
  void *array[10];
  size_t size;
  char **strings;
  size_t i;

  size = backtrace (array, 10);
  strings = backtrace_symbols (array, size);

  for (i = 0; i < size; i++)
     rpr_error_log(_FLN_, NULL, "%s\n", strings[i]);
     //fprintf (fp, "%s\n", strings[i]);

  free (strings);
}

static inline void free_user_slot(connection *cptr, u_ns_ts_t now)
{
  //Note: if slot is already UNALLOC then don't ALLOC it.
  if(cptr->state == UNALLOC)
  {
    rpr_error_log(_FLN_, NULL, "NULL", "HPD Child(%d), free_user_slot() called for UNALLOC slot.\n", g_rpr_id);
    print_trace();
    return;
  }

  RPR_DL(cptr, "Method called. fd = %d, Free Count=%d cptr=%p at %lu, Busy Count = %d", cptr->fd, gFreeVuserCnt, cptr, now, gBusyVuserCnt);

  CHECK_BUF_FREE(cptr);

  cptr->state = UNALLOC;

  // TO add free_user_slot at tail in free pool.
  //cptr->free_next = (struct connection*)gFreeVuserHead;
  //gFreeVuserHead = cptr;
  gFreeVuserTail->free_next = cptr;
  gFreeVuserTail = cptr;
  gFreeVuserTail->free_next = NULL;
  gFreeVuserCnt++;
  gBusyVuserCnt--;

  if (cptr == gBusyVuserHead)
    gBusyVuserHead = (connection*) cptr->busy_next;

  if (cptr == gBusyVuserTail)
    gBusyVuserTail = (connection*) cptr->busy_prev;

  if (cptr->busy_next)
    ((connection*) cptr->busy_next)->busy_prev = cptr->busy_prev;

  if (cptr->busy_prev)
    ((connection*) cptr->busy_prev)->busy_next = cptr->busy_next;

  cptr->busy_next = cptr->busy_prev = NULL;
  //cptr->conn_link = NULL;
  cptr->fd = -1;

  //cptr->chunked_state = CHK_NO_CHUNKS; 
  RPR_DL(cptr, "Free Count=%d, Busy Count = %d", gFreeVuserCnt, gBusyVuserCnt);
}

static inline void close_fd_close_con(connection *cptr, u_ns_ts_t now)
{
  RPR_DL(cptr, "Method called.");

  SSL_CLEANUP

  //cptr->fd == -1, this may be the case if called from api hpd_reset_connection() to reset the connection  
  if (cptr->fd != -1) {
    remove_select(cptr->fd);
    close (cptr->fd);
  }

  free_user_slot(cptr, now);
}

void idle_connection( ClientData client_data, u_ns_ts_t now )
{
  connection* cptr;
  cptr = client_data.p;

  RPR_DL(cptr, "Method called. Now calling rpr_write_data_on_socket, cptr->type = %d", cptr->type);
  //ConfigurationSettings *conf_settings = NULL;
  //write data which is read till timeout has occured.

  if(cptr->type == REQUEST)
  {
    if(cptr->conf_settings->req_conf.socket_end_policy_mode == RPR_REQ_READ_TIMEOUT_SOCKET_EPM)
    {
      cptr->cptr_paired->bytes_remaining = cptr->bytes;
      rpr_dump_req_resp_in_file(cptr, cptr->conf_settings, (char *)cptr->cur_buf, cptr->bytes);
      rpr_write_data_on_socket(cptr->cptr_paired, now, cptr->cur_buf->buffer, cptr->conf_settings);
    }
    else
    {
      cptr->bytes_remaining = 30; 
      rpr_dump_req_resp_in_file(cptr, cptr->conf_settings, "Error: Request read timed out.", cptr->bytes_remaining);
      rpr_write_data_on_socket(cptr, now, "Error: Request read timed out.", cptr->conf_settings);
      CLOSE_FD_CLOSE_CONN(cptr);
    }
  }
  else
  {
    if(cptr->conf_settings->resp_conf.socket_end_policy_mode == RPR_REQ_READ_TIMEOUT_SOCKET_EPM)
    {
      cptr->cptr_paired->bytes_remaining = cptr->bytes;     
      rpr_dump_req_resp_in_file(cptr, cptr->conf_settings, (char *)cptr->cur_buf, cptr->bytes);
      rpr_write_data_on_socket(cptr->cptr_paired, now, cptr->cur_buf->buffer, cptr->conf_settings);
    }
    else
    {
      cptr->cptr_paired->bytes_remaining = 31;
      rpr_dump_req_resp_in_file(cptr, cptr->conf_settings, "Error: Response read timed out.", cptr->cptr_paired->bytes_remaining);
      rpr_write_data_on_socket(cptr->cptr_paired, now, "Error: Response read timed out.", cptr->conf_settings);
      CLOSE_FD_CLOSE_CONN(cptr);
    }
  }
 
}

static inline void start_idle_timer(connection *cptr, int units, u_ns_ts_t now)
{
  RPR_DL(cptr, "Method called, keep alive time out = %d", units);
  ClientData client_data;
  client_data.p = cptr;

  //ab_timers[AB_TIMEOUT_IDLE].timeout_val = units;
  cptr->timer_ptr->actual_timeout = units;

  dis_timer_add_ex(AB_TIMEOUT_IDLE, cptr->timer_ptr, now, idle_connection, client_data, 0, 0);
}

static inline void close_fd_reuse_con(connection *cptr, u_ns_ts_t now)
{
  RPR_DL(cptr, "close_fd_reuse_con Method called.");

  // to initialized the conection variables; this is neccassy when same connection is used for another request.
  //mod_select(cptr, cptr->fd, EPOLLIN);
  // We can come here in following states:
  //  - READ_REQUEST
  //  - THINK_REQUEST
  //  - PROCESS_STATIC_WRITE or PROCESS_STATIC_WRITE_CR or PROCESS_CGI_WRITE
  //  - PROCESS_SMTP_WRITE
  //  - PROCESS_REQUEST (For CGI) - Deleted

  // Anil - Do we need to check THINK_REQUEST ??
  //if(cptr->state != READ_REQUEST)
    mod_select(cptr, cptr->fd, EPOLLIN | EPOLLERR | EPOLLHUP);
  //switch state back to read or check_sid_owner.
  //TODO: check performance impact by this statement.
  cptr->state = READ_REQUEST;
  RPR_DL(cptr, "close_fd_reuse_con Method finish.");
}

void close_fd(connection *cptr, int finish, u_ns_ts_t now)
{
  RPR_DL(cptr, "close_fd called.");
  int timer_type;

  RPR_DL(cptr, "Method called. fd = %d, finish = %d, now = %lu",
                                                     cptr->fd, finish, now);

  /* We can not free req data in case of 100-Continue response sending*/
  // This will free req data (e.g. body,hdr) if stored and set other req related fields to default value
  free_retrieve_data(cptr, now); // free the body (buf_head and cur_buf)

  cptr->bytes = 0;
  cptr->bytes_remaining = 0;
  cptr->offset = 0;
  cptr->content_length = -1;
  cptr->is_data_complete = 0;
  
  if (finish) {
    RPR_DL(cptr, "finish is 1 hence closing connection.");

    timer_type = cptr->timer_ptr->timer_type;
    if ( timer_type >= 0 )
      dis_timer_del(cptr->timer_ptr);

    close_fd_close_con(cptr, now);
  } else {
      // Protocol is not http or https, always start timer
    RPR_DL(cptr, "Connection is to be reused.");
    close_fd_reuse_con(cptr, now);
  }
}


//-----------------------------------------------------------------------------
/*
void SetSockAcceptDefer(int fd, int proto)
{
  int value = RPR_DEFAULT_CONN_TIMEOUT;
  int flag;

  if (hpd_global_config->optimize_ether_flow > 0) {
    flag = 0;

    //printf ("Clear QuickAck\n");
    if (setsockopt( fd, IPPROTO_TCP, TCP_QUICKACK, (char *)&flag, sizeof(flag) ) < 0)
    {
      close(fd);
      HPD_EXIT(-1, _FLN_, "Error in setting  recv buffer size");
    }
  }

  if (hpd_global_config->optimize_ether_flow > 1) {
    if (hpd_global_config->keep_alive_timeout) {
    flag = 0;

    //printf ("Nodely \n");
    if (setsockopt( fd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag) ) < 0)
    {
      close(fd);
      HPD_EXIT(-1, _FLN_, "Error in setting  recv buffer size");
    }
    } else {
    flag = 1;

    //printf ("CORK \n");
    if (setsockopt( fd, IPPROTO_TCP, TCP_CORK, (char *)&flag, sizeof(flag) ) < 0)
    {
      close(fd);
      HPD_EXIT(-1, _FLN_, "Error in setting  recv buffer size");
    }
    }
  }
}
*/

/*Init epoll 
Create epoll file descriptor
This function uses epoll_create() to create a file descriptor to a new epoll instance given to us by the mighty kernel. While it doesn’t do anything with it quite yet we should still make sure to clean it up before the program terminates. Since it’s like any other Linux file descriptor we can just use close() for this.
*/
static inline void ns_epoll_init()
{
  total_max_epoll_fds = 4; /* IPV4(http/https) and IPV6(http/https) - total 4 Listner FDs */

  total_max_epoll_fds += MAX_CON_PER_CHILD;

  RPR_DL(NULL, "total_max_epoll_fds = %u\n", total_max_epoll_fds);

  if ((v_epoll_fd = epoll_create(total_max_epoll_fds)) == -1) {
    RPR_EXIT(-1, "epoll_create: err = %s", strerror(errno));
  }
  /*
  if(hpd_global_config->g_enable_hpd_listener_epoll)
  {
    if ((g_msg_com_listen_epfd = epoll_create(30)) == -1) {
      RPR_EXIT(-1, "epoll_create: err = %s", strerror(errno));
    }
  }
  */
}

inline void remove_select(int fd)
{
  struct epoll_event pfd;

  RPR_DL(NULL, "Method called. Removing %d from select", fd);

  bzero(&pfd, sizeof(struct epoll_event)); //Added after valgrind reported bug

  if (fd == -1) return;

  if (epoll_ctl(v_epoll_fd, EPOLL_CTL_DEL, fd, &pfd) == -1) {
    rpr_error_log(_FLN_, NULL, "epoll del: err = %s, FD =%d", strerror(errno), fd);
    //assert(0);
    //exit (-1);
  }
}

// Note - In this method, cptr can be fd also. So do not use cptr in debug and error log()
inline void add_select(void* ptr, int fd, int event)
{
  struct epoll_event pfd;

  RPR_DL(NULL, "Method called. Adding %d for event=%x", fd, event);

  bzero(&pfd, sizeof(struct epoll_event)); //Added after valgrind reported bug

  //pfd.events = EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLET;
  pfd.events = event;
  pfd.data.ptr = (void*) ptr;
  if (epoll_ctl(v_epoll_fd, EPOLL_CTL_ADD, fd, &pfd) == -1) {
    RPR_EXIT(-1, "epoll add: err = %s", strerror(errno));
  }
}

inline void mod_select(connection* cptr, int fd, int event)
{
  struct epoll_event pfd;

  RPR_DL(cptr, "Method called. Moding %d for event = %x", fd, event);

  bzero(&pfd, sizeof(struct epoll_event)); //Added after valgrind reported bug

  //pfd.events = EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLET;
  pfd.events = event;
  pfd.data.ptr = (void*) cptr;
  if (epoll_ctl(v_epoll_fd, EPOLL_CTL_MOD, fd, &pfd) == -1) {
    RPR_EXIT(-1, "epoll mod: err = %s", strerror(errno));
  }
}

static void create_listener_for_ipv4(char *ip_addr, ConfigurationSettings conf_settings)
{
  int g_listener_backlog = 100000;
  char err_msg[1024];

  RPR_DL(NULL, "Method called");

  if(conf_settings.socket_is_ssl)
  {
 
    RPR_DL(NULL, "In ssl TCP.");
    MY_MALLOC(socket_tcps_udp_lfd, sizeof(listener), NULL, "for child ssl listener");
    memset(socket_tcps_udp_lfd, 0, sizeof(listener));

    socket_tcps_udp_lfd[0].fd = nslb_Tcp_listen_ex(conf_settings.recording_port, g_listener_backlog, ip_addr, err_msg);
    if(socket_tcps_udp_lfd[0].fd < 0)
    {
      rpr_error_log(_FLN_, NULL, "%s", err_msg);
      RPR_EXIT(-1, "%s", err_msg);
    }
    RPR_DL(NULL, "Listening at SSL SOCKET TCP/UDP fd = %d", socket_tcps_udp_lfd[0].fd);
    nslb_Fcntl( socket_tcps_udp_lfd[0].fd, F_SETFL, O_NDELAY );
    socket_tcps_udp_lfd[0].protocol = SOCKET_TCPS_PROTOCOL;
    socket_tcps_udp_lfd[0].con_type = RPR_CON_TYPE_LISTENER;
  }
  else
  {
    RPR_DL(NULL, "In non ssl TCP.");
    //For Socket TCP/UDP Services
    MY_MALLOC(socket_tcp_udp_lfd, sizeof(listener), NULL, "for Child non ssl listener");
    memset(socket_tcp_udp_lfd, 0, sizeof(listener));
 
    socket_tcp_udp_lfd[0].fd = nslb_Tcp_listen_ex(conf_settings.recording_port, g_listener_backlog, ip_addr, err_msg);
    if(socket_tcp_udp_lfd[0].fd < 0)
    {
      rpr_error_log(_FLN_, NULL, "%s", err_msg);
      RPR_EXIT(-1, "%s", err_msg);
    }
    RPR_DL(NULL, "Listening at SOCKET TCP/UDP fd = %d", socket_tcp_udp_lfd[0].fd);
    nslb_Fcntl( socket_tcp_udp_lfd[0].fd, F_SETFL, O_NDELAY );
    socket_tcp_udp_lfd[0].protocol = SOCKET_TCP_PROTOCOL;
    socket_tcp_udp_lfd[0].con_type = RPR_CON_TYPE_LISTENER;
  }
  /*
  // FOR SSL TCP SOCKET 
  if(num_socket_sport_service_list){
    RPR_DL(NULL, "num_socket_sport_service_list = %d", num_socket_sport_service_list);
    MY_MALLOC(socket_tcps_udp_lfd, num_socket_sport_service_list * sizeof(listener), NULL, "number of listener ports for KOHLS IPV4");
    memset(socket_tcps_udp_lfd, 0, num_socket_sport_service_list * sizeof(listener));
  }
  for(i = 0; i < num_socket_sport_service_list; i++)
  {
    char err_msg[1024];
    RPR_DL(NULL, "Listening for SSL TCP SOCKET port %d", num_socket_sport_service_list);
    socket_tcps_udp_lfd[i].fd = nslb_Tcp_listen_ex(GET_SOCKET_SPORT_TABLE[i].port, g_listener_backlog, ip_addr, err_msg);
    if(socket_tcps_udp_lfd[i].fd < 0)
    {
      rpr_error_log(_FLN_, NULL, "listen_for_all_protocols", NULL, "%s", err_msg);
      RPR_EXIT(-1, "%s", err_msg);
    }
    RPR_DL(NULL, "Listening at SOCKET TCP/UDP fd = %d", socket_tcps_udp_lfd[i].fd);
    nslb_Fcntl( socket_tcps_udp_lfd[i].fd, F_SETFL, O_NDELAY );
    socket_tcps_udp_lfd[i].protocol = SOCKET_TCPS_PROTOCOL;
    socket_tcps_udp_lfd[i].con_type = RPR_CON_TYPE_LISTENER;
  }
  */
}

static void add_listner_port_in_epoll(char is_ssl)
{
  //int i;
    // For SOCKET_TCP
  if(is_ssl)
  {
    /*
    for (i = 0; i < child_info_shm[g_rpr_id - 1].num_ip_data_entries; i++) {
      //SetSockAcceptDefer(child_info_shm[g_rpr_id - 1].child_ip_data[i].socket_tcps_lfd[0].fd, SOCKET_TCPS_PROTOCOL);
      add_select((void *)(&((child_info_shm[g_rpr_id - 1].child_ip_data[i].socket_tcps_lfd[0]))),
               child_info_shm[g_rpr_id - 1].child_ip_data[i].socket_tcps_lfd[0].fd, EPOLLIN);
    }
    */
    add_select((void*)(&(socket_tcps_udp_lfd[0])), socket_tcps_udp_lfd[0].fd, EPOLLIN);
  
  }
  else
  {
    /*
    for (i = 0; i < child_info_shm[g_rpr_id - 1].num_ip_data_entries; i++) {
      //SetSockAcceptDefer(child_info_shm[g_rpr_id - 1].child_ip_data[i].socket_tcp_lfd[0].fd, SOCKET_TCP_PROTOCOL);
      add_select((void *)(&((child_info_shm[g_rpr_id - 1].child_ip_data[i].socket_tcp_lfd[0]))),
               child_info_shm[g_rpr_id - 1].child_ip_data[i].socket_tcp_lfd[0].fd, EPOLLIN);
    }
    */
    add_select((void*)(&(socket_tcp_udp_lfd[0])), socket_tcp_udp_lfd[0].fd, EPOLLIN);
  }
    /*
    // For SSL SOCKET_TCP
    for (i = 0; i < child_info_shm[g_rpr_id - 1].num_ip_data_entries; i++) {
      for(j = 0; j < num_socket_sport_service_list; j++) { //This loop for reading tcp ssl value
        SetSockAcceptDefer(child_info_shm[g_rpr_id - 1].child_ip_data[i].socket_tcps_lfd[j].fd, SOCKET_TCPS_PROTOCOL);
        add_select((void *)(&((child_info_shm[g_rpr_id - 1].child_ip_data[i].socket_tcps_lfd[j]))),
                 child_info_shm[g_rpr_id - 1].child_ip_data[i].socket_tcps_lfd[j].fd, EPOLLIN);
      }
    }
    */

    // For SOCKET_TCP
    //SetSockAcceptDefer(ipv4_smtp_lfd[i].fd);

    /*
    // For SSL SOCKET_TCP
    for(i = 0; i < num_socket_sport_service_list; i++) //Loop for reading TCP IPV4 ports
    {
      //SetSockAcceptDefer(ipv4_smtp_lfd[i].fd);
      add_select((void*)(&(socket_tcps_udp_lfd[i])), socket_tcps_udp_lfd[i].fd, EPOLLIN);
    }
    */
}

// This is called by child to close log fds opened by parent
void close_log_fds()
{ 
  RPR_DL(NULL, "Method called.");
  if(debug_fd > 0)
  {
    RPR_DL(NULL, "Closing debug log file of parent");
    close(debug_fd);
    debug_fd = -1;
  } 
  if(error_fd > 0)
  {
    RPR_DL(NULL, "Closing error log file of parent");
    close(error_fd);
    error_fd = -1;
  }
  nslb_close_log_fd();
}

static void set_random_seed()  /* Set Seed for random() */
{
  unsigned int seed;

  seed = (unsigned int)get_ms_stamp();
  seed ^= getpid();
  seed ^= getppid();

  srandom(seed);
}

static connection* allocate_user_tables(int conn_per_chunk) {
  connection* connection_chunk;
  timer_type *timer_chunk, *timer_ptr;
  int cnum;
  static int gTotalVuserSlots = 0;

  RPR_DL(NULL, "Method called. conn_per_chunk = %d", conn_per_chunk);

  gTotalVuserSlots += conn_per_chunk;

  MY_MALLOC(connection_chunk, conn_per_chunk * sizeof(connection), NULL, "conn_per_chunk * sizeof(connection)");

  MY_MALLOC(timer_chunk, (conn_per_chunk) * sizeof(timer_type), NULL, "(conn_per_chunk) * sizeof(timer_type)");
  timer_ptr = timer_chunk;

  memset(connection_chunk, 0, (conn_per_chunk * sizeof(connection)));

  for ( cnum = 0; cnum < conn_per_chunk; cnum++ ) {
    connection_chunk[cnum].free_next = (struct connection *) &connection_chunk[cnum+1];
    //connection_chunk[cnum].free_next = &connection_chunk[cnum+1];
    connection_chunk[cnum].busy_next = NULL;
    connection_chunk[cnum].busy_prev = NULL;
    connection_chunk[cnum].state = UNALLOC;
    connection_chunk[cnum].timer_ptr = timer_ptr;
    timer_ptr++;
    connection_chunk[cnum].timer_ptr->timer_type= -1;
    connection_chunk[cnum].timer_ptr->next = NULL;
    connection_chunk[cnum].timer_ptr->prev = NULL;
    //connection_chunk[cnum].uvtable = alloc_uvtable();
    //connection_chunk[cnum].error_url_hashcode = -1;
    //connection_chunk[cnum].url_data = NULL;
    //connection_chunk[cnum].wtgrpptr = NULL;
    //connection_chunk[cnum].order_table = &order_chunk[cnum * max_var_table_idx];
    /*
    if (hpd_global_config->pipelining_enabled) {
      MY_MALLOC(connection_chunk[cnum].pipe, sizeof(pipelining), NULL, "pipe");
      connection_chunk[cnum].pipe->pipe_buf = NULL;
      connection_chunk[cnum].pipe->pipe_buf_len = 0;
      connection_chunk[cnum].pipe->pipe_buf_offset = 0;
      connection_chunk[cnum].pipe->pipe_started = 0;
    }
    */
  }
  connection_chunk[cnum-1].free_next = NULL;
  gFreeVuserCnt += conn_per_chunk;
  gFreeVuserTail = &connection_chunk[cnum-1];  //Save Tail(last) address in gFreeVuserTail

  // Added for debugging of memory leak
  RPR_DL(NULL, "allocate_user_tables(%d) called."
                                   " gTotalVuserSlots = %d, gFreeVuserCnt = %d",
                                   conn_per_chunk, gTotalVuserSlots, gFreeVuserCnt);

  return connection_chunk;
}


connection* get_free_user_slot(int fd)
{
  connection *free, *next_free;

  RPR_DL(NULL, "Method called. fd = %d, Free Count = %d, Busy Count = %d", fd, gFreeVuserCnt, gBusyVuserCnt);

  if (gFreeVuserHead == NULL)
    gFreeVuserHead = allocate_user_tables(USER_CHUNK_SIZE);

  free = gFreeVuserHead;

  if (free) {
    CHECK_BUF_FREE(free);
    next_free = (connection*) free->free_next;
    free->free_next = NULL;
    //free->querry_str = NULL;
    gFreeVuserHead = next_free;
    gFreeVuserCnt--;
    gBusyVuserCnt++;
    //free->request_url = NULL;
    if (gFreeVuserMinCnt > gFreeVuserCnt) gFreeVuserMinCnt = gFreeVuserCnt;
  }

  if (gBusyVuserTail) {
    gBusyVuserTail->busy_next = (struct connection*) free;
    free->busy_prev = (struct connection*) gBusyVuserTail;
    gBusyVuserTail = free;
  } else {
    gBusyVuserHead = free;
    gBusyVuserTail = free;
  }

  RPR_DL(free, "Free Count=%d returns = %p, Busy Count = %d", gFreeVuserCnt, free, gBusyVuserCnt);
  /* Make conn_link null */
  //free->conn_link = NULL;
  //free->send_file = 0;
  //free->url_data = NULL;

  free->state = READ_REQUEST;
  free->con_type = RPR_CON_TYPE_CPTR;
  free->content_length = -1;
  //Note: this will be used to keep actual client address. Need to reset on new connection. 
  return free;
}

/* Function used to calculate request length.
 * 
 * ptr: pointer to the request read 
 * prefix_length: in case of fixed length, this is the length on which complete request length is mentioned.
 * 
 * Returns: lenght of request.
 * */

int socket_calc_req_length(char *ptr, int prefix_length)
{
  RPR_DL(NULL, "Method Called");
  char buf[prefix_length + 1];
  int req_len;

  memcpy(buf,ptr,prefix_length);
  buf[prefix_length] = '\0';
  req_len = atoi(buf) + prefix_length;
  return req_len;
}

#if 0
void rpr_forward_payload(connection *cptr, void *to_fp, int is_ssl, int type)
{
  int flag = 1;

  if(is_ssl)
    flag = 2;

  if (type == REQUEST)
  {
    if (datawrite(cptr->socket_tcp_request_buff, cptr->socket_tcp_request_buff_len, to_fp, flag) <= 0) {
      rpr_error_log(_FLN_, NULL, "Error in datawrite(). Segment number =%d. Error = %s\n", i, strerror(errno));
      break;
    }
  }
  else if (type == RESPONSE)
  {
    if (datawrite(cptr->socket_tcp_response_buff, cptr->socket_tcp_response_buff_len, to_fp, flag) <= 0) {
      rpr_error_log(_FLN_, NULL, "Error in datawrite(). Segment number =%d. Error = %s\n", i, strerror(errno));
      break;
    }
  }
}

void debug_log_socket_tcp_req_rep(connection *cptr, char *req, int req_len, char *type)
{
  char log_file[256 + 1];
  int log_fd;
  char buffer[MAX_REQ_SIZE + 1];
  //char file_start_name[128 + 1];

  RPR_DL(cptr, "Method called, req_len = %d, cptr->req_id = %d, g_req_id = %d", req_len, cptr->req_id, g_req_id);
  //if debug log flag is set or trace level is more than or equal to 2
  if(!debug_log)
    return;

  snprintf(log_file, 256, "%s/logs/%s_%s_%d_%u.dat", g_rpr_script_dir, type, g_rpr_id, cptr->req_id);

  if((log_fd = open(log_file, O_CREAT | O_WRONLY | O_APPEND | O_CLOEXEC, 00666)) < 0){
    rpr_error_log(_FLN_, NULL, "Error in opening file %s to dump socket tcp %s", log_file, type);
  } else {
    req_len = snprintf(buffer, MAX_REQ_SIZE, "\n%s|%s\n", rpr_get_cur_date_time(), req);
    write(log_fd, buffer, req_len);
    close(log_fd);
  }
}
#endif

#if 0
/* Function used to process the TCP request and send it to the NO core engine.
 * 
 * cptr: pointer to the connection structure
 * read_buf: pointer to the request read
 * byte_read: length of the read request. 
 * 
 * Returns: Nothing.
 * */

static void proc_socket_tcp_req(connection *cptr, u_ns_ts_t now, char *read_buf, int byte_read, void *dest, int is_ssl, int type)
{
  RPR_DL(cptr, "Method Called, read buf = %s", read_buf);

  RPR_DL(cptr, "cptr->request_url = %s", cptr->request_url);
  if(type == REQUEST)
  {
    MY_MALLOC(cptr->socket_tcp_request_buff, byte_read, cptr, "cptr->socket_tcp_request_buff");
    MY_MEMCPY(cptr->socket_tcp_request_buff, read_buf, byte_read);
    cptr->socket_tcp_request_buff_len = byte_read;
    debug_log_socket_tcp_req_rep(cptr, cptr->socket_tcp_request_buff, cptr->socket_tcp_request_buff_len, "request");
  }
  else if (type == RESPONSE)
  {
    MY_MALLOC(cptr->socket_tcp_response_buff, byte_read, cptr, "cptr->socket_tcp_response_buff");
    MY_MEMCPY(cptr->socket_tcp_response_buff, read_buf, byte_read);
    cptr->socket_tcp_response_buff_len = byte_read;
    debug_log_socket_tcp_req_rep(cptr, cptr->socket_tcp_request_buff, cptr->socket_tcp_request_buff_len, "response");
  }

  rpr_forward_payload(cptr, dest, is_ssl, type);
}
#endif

//total_size is the total_size saved so far prior to this call
//After call to this fucntion , source of total bytes must be incremted by length
void copy_retrieve_data( connection* cptr, char* buffer, int length, int total_size )
{
  int copy_offset;
  int copy_length;
  struct copy_buffer* new_buffer;
  int start_buf = 0;

  RPR_DL(cptr, "Method called. length = %d, total_size = %d", length, total_size);

  if(!total_size) 
    start_buf = 1; // setting start_buf flag
    
  while (length) {
    copy_offset = total_size % COPY_BUFFER_LENGTH;
    copy_length = COPY_BUFFER_LENGTH - copy_offset;
    RPR_DL(cptr, "copy_offset = %d, copy_length = %d, length = %d, total_size = %d",
                            copy_offset, copy_length, length, total_size);

    if (!copy_offset) {
      MY_MALLOC(new_buffer, sizeof(struct copy_buffer), NULL, "new copy buffer");
      if (new_buffer) {
        new_buffer->next = NULL;
        if (start_buf) {
          cptr->buf_head = cptr->cur_buf = new_buffer;
          start_buf = 0;
        }
        else {
          cptr->cur_buf->next = new_buffer;
          cptr->cur_buf = new_buffer;
        }
      }
    }
    if (length <= copy_length)
      copy_length = length;    // This sets the copy length if chunk recieved less than COPY_BUFFER_LENGTH

    memcpy(cptr->cur_buf->buffer+copy_offset, buffer, copy_length);
    total_size += copy_length;
    length -= copy_length;
    buffer += copy_length;
    RPR_DL(cptr, "After copy buffer of lengh %d, buffer = %s", copy_length, buffer);
  }
}

// Before calling this adjust cptr->total_len
void handle_partial_recv(connection* cptr, char *buffer, int length, int total_size)
{
  int copy_offset;
  int copy_length;
  struct copy_buffer* new_buffer; 
  int start_buf = 0;

  RPR_DL(cptr, "Method called, cptr = %p, buffer = %p, len = %d, total_len = %d", cptr, buffer, length, total_size);

  if(!total_size) 
    start_buf = 1; // setting start_buf flag

  while (length) {
    copy_offset = total_size % COPY_BUFFER_LENGTH; 
    copy_length = COPY_BUFFER_LENGTH - copy_offset;
    RPR_DL(cptr, "copy_offset = %d, copy_length = %d, length = %d, total_size = %d",
                            copy_offset, copy_length, length, total_size);

    if (!copy_offset) {
      MY_MALLOC(new_buffer, sizeof(struct copy_buffer), "new copy buffer", -1);
      if (new_buffer) {
        new_buffer->next = NULL;
        if (start_buf) {
          cptr->buf_head = cptr->cur_buf = new_buffer;
          start_buf = 0;
        }
        else {
          cptr->cur_buf->next = new_buffer;
          cptr->cur_buf = new_buffer;
        }
      }
    }
    if (length <= copy_length)
      copy_length = length;    // This sets the copy length if chunk recieved less than COPY_BUFFER_LENGTH
      
    memcpy(cptr->cur_buf->buffer+copy_offset, buffer, copy_length);
    total_size += copy_length; 
    length -= copy_length;     
    buffer += copy_length;    
    RPR_DL(cptr, "After copy buffer of lengh %d, buffer = %s", copy_length, buffer);
  } 
} 

// Copy linked list data into a big buffer
// This linked list has the request body
char *copy_linked_list_in_buf(connection *cptr, u_ns_ts_t now)
{
  long blen = cptr->bytes;
  int complete_buffers = blen / COPY_BUFFER_LENGTH;
  int incomplete_buf_size = blen % COPY_BUFFER_LENGTH;
  struct copy_buffer* buffer = cptr->buf_head;
  int i;
  char *tmp_buffer;

  RPR_DL(cptr, "Method called. blen = %ld, fd = %d, bytes = %ld", blen, cptr->fd, cptr->bytes);

  MY_MALLOC(tmp_buffer , blen + 1, cptr, "tmp_buffer");

  char *copy_cursor = tmp_buffer;

  for (i = 0; i < complete_buffers; i++)
  {
    memcpy(copy_cursor, buffer->buffer, COPY_BUFFER_LENGTH);
    copy_cursor += COPY_BUFFER_LENGTH;
    buffer = buffer->next;
  }
  if(incomplete_buf_size)
    memcpy(copy_cursor, buffer->buffer, incomplete_buf_size);

  RPR_DL(cptr, "tmp_buffer[blen] = [%c], tmp_buffer[blen-1] = [%c], tmp_buffer[blen-2] = [%c], tmp_buffer[blen-3] = [%c]",
                  tmp_buffer[blen], tmp_buffer[blen - 1], tmp_buffer[blen - 2], tmp_buffer[blen - 3]);
  tmp_buffer[blen] = '\0';
  RPR_DL(cptr, "blen = %ld, fd = %d", blen, cptr->fd);

  return tmp_buffer;
}

/*
  Read end policy and their priority -
    1. If no policy is given, read data till timeout
    2. If Message format is given then read data till provided length in message  
    3. If read bytes is given, read only data till read bytes
    4. If suffix is gievn, read till suffix not found
*/
int process_socket_recv_data(connection *cptr_in, char *buf, int bytes_read, int *read_offset, int msg_peek, ConfigurationSettings conf_settings, u_ns_ts_t now)
{
  int done = 0, err = 0, len = 0, len1 = 0;
  char payload_len[20] = {0};  //must be 0
  char dynamic_len_size[20] = {0};  //must be 0
  //char *payload_len_ptr = payload_len;
  //char *payload = buf;
  char *payload_ptr;
  char *tmp_ptr, *tmp_ptr1;
  connection *cptr = cptr_in;
  int remaning_length_bytes = 0;
  //u_ns_ts_t now = get_ms_stamp();
  SocketConfiguration *socket_conf = NULL;
  
  if(cptr->type == REQUEST)
    socket_conf = &conf_settings.req_conf;
  else if(cptr->type == RESPONSE) 
    socket_conf = &conf_settings.resp_conf;

  RPR_DL(cptr, "Method called, buf = %s, bytes_read = %d, "
                         "len-bytes = %d, len_type = %d, "
                         "cptr->bytes = %d, cptr->content_length = %d, now = %ld",
                          buf, bytes_read, socket_conf->socket_end_policy_value,
                          socket_conf->socket_end_policy_mode, cptr->bytes, cptr->content_length, now);
#if 0
#define RPR_TCP_SEGMENT_SOCKET_EPM           0
#define RPR_FIX_LEN_SOCKET_EPM           1
#define RPR_DELIMETER_SOCKET_EPM         2
#define RPR_DYNAMIC_LEN_SOCKET_EPM       3
#define RPR_REQ_READ_TIMEOUT_SOCKET_EPM           4
#endif

  switch(socket_conf->socket_end_policy_mode)
  {
    case RPR_DYNAMIC_LEN_SOCKET_EPM:
    {
      if((cptr->bytes + bytes_read) < socket_conf->socket_end_policy_value) // It means EGAIN comes
      {
        // Handle partial length bytes
        handle_partial_recv(cptr, buf, bytes_read, cptr->bytes);  // cptr->bytes must be 0
        cptr->bytes += bytes_read;
        RPR_DL(cptr, "RPR_DYNAMIC_LEN_SOCKET_EPM: partial Length_Bytes read, cptr->bytes = %d", cptr->bytes);
        return 0;
      }
      else if(cptr->content_length == -1)// Get content length first 
      {
        remaning_length_bytes = socket_conf->socket_end_policy_value - cptr->bytes;

        RPR_DL(cptr, "RPR_PROTOSOCKET_READ_ENDPOLICY_LENGTH_BYTES: remaning_length_bytes = %d, "
                               "cptr->bytes = %d", remaning_length_bytes, cptr->bytes);

        if(cptr->bytes) // If Length_Bytes is partially read
        {
          //sprintf(payload_len, "%s", (char *)cptr->cur_buf);
          //snprintf(payload_len+cptr->bytes, remaning_length_bytes + 1, "%s", buf);
          memcpy(payload_len, (char *)cptr->cur_buf, cptr->bytes);
          memcpy(payload_len+cptr->bytes, buf, remaning_length_bytes);
        }
        else
          memcpy(payload_len, buf, remaning_length_bytes);
          //snprintf(payload_len, remaning_length_bytes +  1, "%s", buf);

        //RPR_DL(cptr, "payload_len = %s", payload_len);
        int i;
        for(i = 0; i < remaning_length_bytes; i++)
          RPR_DL(NULL, "payload_len[%d] = 0x%x", i, payload_len[i]);

        // Read message length
        //cptr->body_offset += socket_req->rlen_bytes;
        RPR_DL(cptr, "socket_dynamic_len_type = %d", socket_conf->socket_dynamic_len_type);
        if(socket_conf->socket_dynamic_len_type == LEN_TYPE_TEXT)
        {
          //memcpy(payload_len_ptr, payload, socket_conf->socket_end_policy_value);
          if(!ns_is_numeric(payload_len))
          {
            RPR_DL(cptr, "Length-Byte '%s' is not numeric.", payload_len);
            err = 1;
            goto error;
          }
          cptr->content_length = atoi(payload_len) + socket_conf->socket_end_policy_value;
        }
        else
        {
          nslb_endianness((unsigned char*)dynamic_len_size, (unsigned char*)payload_len, socket_conf->socket_end_policy_value, socket_conf->len_endian);
          RPR_DL(cptr, "atoi(dynamic_len_size) = %d", atoi(dynamic_len_size));
          for(i = 0; i < socket_conf->socket_end_policy_value; i++)
            RPR_DL(NULL, "dynamic_len_size[%d] = 0x%x", i, dynamic_len_size[i]);

          memcpy(&(cptr->content_length), dynamic_len_size, socket_conf->socket_end_policy_value);
          cptr->content_length += socket_conf->socket_end_policy_value;
        }

        RPR_DL(cptr, "payload_len = %s, content length = %d", payload_len, cptr->content_length);
        if(cptr->content_length <= 0)
        {
          RPR_DL(cptr, "Content-Length cannot be zero.");
          err = 1;
          goto error;
        }

        // Now copy payload into cptr->cur_buf if exist
        //payload = buf + remaning_length_bytes; //Skip payload len
        //payload = buf;
        //bytes_read -= remaning_length_bytes;  //Substract length size  
        cptr->bytes = 0;
        #if 0
        if(cptr->bytes && bytes_read > 0)
        {
          //To reuse cptr->cur_buf->buffer fill at least one byte here 
          memcpy(cptr->cur_buf->buffer, payload, 1);
          cptr->bytes++;
          bytes_read--;
        }
  
        RPR_DL(cptr, "NS_PROTOSOCKET_READ_ENDPOLICY_LENGTH_BYTES: "
                               "get payload length, content_length = %d, payload = %p, cptr->bytes = %d",
                                cptr->content_length, payload, cptr->bytes);
        #endif
      }
      else
        RPR_DL(cptr, "Code shoud not come into this lag");

      RPR_DL(cptr, "cptr->content_length = %d, cptr->bytes = %d, bytes_read = %d", cptr->content_length, cptr->bytes, bytes_read);
      if(cptr->content_length <= (cptr->bytes + bytes_read))
        done = 1;

      break;
    }
    case RPR_FIX_LEN_SOCKET_EPM:
    {
      if(cptr->content_length == -1)
      {
        cptr->content_length = socket_conf->socket_end_policy_value;
      }

      if(cptr->content_length <= (cptr->bytes + bytes_read))
        done = 1;

      RPR_DL(cptr, "RPR_PROTOSOCKET_READ_ENDPOLICY_READ_BYTES get payload length, cptr->content_length = %d", cptr->content_length);
      break;
    }
    case RPR_DELIMETER_SOCKET_EPM:
    {
      /*=======================================================================
        [HINT: ReadEndPolicyDelimiter]

           1. If read is done by MSG_PEEK then,
              1.1. Check for delimiter existence,
              1.2. If delim found then 
                   calculate size which need to without MSG_PEEK
              1.3. If delim not found then, 
                   Check how many characters of delimiter found
                   1.3.1. If no characters found, JUST return to read without 
                          MSG_PEEK
                   1.3.2. If some characters found, then save OFFSET in delim
                          from where next time need to check for delimiter 
                            
            ==> OFFSET , use this variable in following way
                1. To save how many delimiter bytes found in read data
                   OFFSET = (+)ve
                2. To save Delimter found flag
                   OFFSET = -1
                3. Not check OR in progress 
                   OFFSET = 0 
            2. If read done without MSG_PEEK then,
               2.1. If offset

           Cases: 1. Payload > Delim, Payload contains complete Delim
                  2. Payload > Delim, Payload contains partial Delim
                  3. Payload > Delim, Payload doesn't contain Delim
                  4. Payload < Delim, Payload contains partial Delim
                  5. Payload < Delim, Payload doesn't contain Delim
      =======================================================================*/

      payload_ptr = buf;
      len1 = bytes_read;

      tmp_ptr = socket_conf->socket_delimeter;
      len = strlen(tmp_ptr);

      //IF read with flag MSG_PEEK
      if(msg_peek)
      {
        if(cptr->body_offset > 0)
        {
          len = len - cptr->body_offset;
          tmp_ptr += cptr->body_offset;
        }

        if((tmp_ptr1 = memmem(payload_ptr, len1, tmp_ptr, len)) != NULL) //Found
        {
          *read_offset = (tmp_ptr1 - payload_ptr) + len;
          cptr->body_offset = -1;  //Found
        }
        else  // Not Found
        {
          *read_offset = bytes_read;
          //Check how many bytes of delim found 
          if(len1 > len)
          {
            payload_ptr += len1 - len; //check from last only
            len1 = len;
          }
          else
          {
            len = len1;
          }

          tmp_ptr1 = payload_ptr;  //Delim start
          RPR_DL(cptr, "REQ_DELIMETER_SOCKET_EPM Peek else part,"
                        " len = %d, len1 = %d, tmp_ptr1 = %s, payload_ptr = %s", len, len1, tmp_ptr1, payload_ptr);

          for(int i = 0; i < len; i++)
          {
            if((tmp_ptr1 = memchr(tmp_ptr1, tmp_ptr[i], len1)) != NULL) //One byte matched   //tmp_ptr1 
            {
              if(cptr->body_offset == 0)
              {
                payload_ptr += len1 - 1;
                len = payload_ptr - tmp_ptr1 + 1; //Need to run for loop only for remaining bytes from where Delimiter first byte is found
              }
              cptr->body_offset++;
              tmp_ptr1++;
              len1 = 1;  //Now search for 1 char only
            }
            else
            {
              cptr->body_offset = 0;
              break;
            }
          }
        }
      }
      else
      {
        if (cptr->body_offset < 0)
        {
          cptr->body_offset = 0;         //MUST take care of this at the time of error case also
          done = 1;
        }
      }
      RPR_DL(cptr, "REQ_DELIMETER_SOCKET_EPM, done = %d, "
          "payload_ptr = %s, len1 = %d, tmp_ptr = %s, len = %d,"
          "msg_peek = %d, cptr->body_offset = %d",
           done, payload_ptr, len1, tmp_ptr, len, msg_peek, cptr->body_offset);

      if(msg_peek)
      {
        return 0;
      }

      break;
    }
    case RPR_REQ_CONTAINS_SOCKET_EPM:
    {
      payload_ptr = buf;
      len1 = bytes_read;

      tmp_ptr = socket_conf->socket_delimeter;
      len = strlen(tmp_ptr);

      RPR_DL(cptr, "payload_ptr = %s, len1 = %d, tmp_ptr = %s, len = %d", payload_ptr, len1, tmp_ptr, len);

      if((payload_ptr = memmem(payload_ptr, len1, tmp_ptr, len)) != NULL)
        done = 1;

      RPR_DL(cptr, "REQ_CONTAINS_SOCKET_EPM, done = %d, payload_ptr = %s, len1 = %d, tmp_ptr = %s, len = %d",
                 done, payload_ptr, len1, tmp_ptr, len);

      break;
    }
    case RPR_REQ_READ_TIMEOUT_SOCKET_EPM:
    {
      // Timeout case will be handle form timeout calback
      // Here just read and save data into cptr->buf_head
      break;
    }
    case RPR_CONN_CLOSE_SOCKET_EPM:
    {
      if(bytes_read == 0)
        done = 1;

      break;
    }
    default:  // No policy 
    {
      RPR_DL(cptr, "NO policy is given");
      done = 1;
      break;
    }
  }
  /*Calculate remaining bytes in last iteration*/
  if(cptr->content_length > 0)
    len = (cptr->content_length >= (cptr->bytes + bytes_read)) ? bytes_read:
                                   (bytes_read - ((cptr->bytes + bytes_read) - cptr->content_length));
  else
    len = bytes_read;

  RPR_DL(cptr, "Copy read data into cptr->buf_head, cptr = %p, "
                         "bytes_read = %d, cptr->bytes = %d, cptr->content_length = %d, len = %d",
                          cptr, bytes_read, cptr->bytes, cptr->content_length, len);

  copy_retrieve_data(cptr, buf, len, cptr->bytes);

  //rpr_copy_payload_to_cptr(cptr, buf, bytes_read);
  cptr->bytes += len;

error:
  if(done || err)
  {
    // Remove event EPOLLIN 
    //ns_socket_modify_epoll(cptr, NULL, EPOLLOUT);
    mod_select(cptr, cptr->fd, EPOLLERR | EPOLLHUP );

    if(done) // Data reading done
    {
      cptr->is_data_complete = done;
      // Delete timer
      if(cptr->timer_ptr->timer_type == AB_TIMEOUT_IDLE)
        dis_timer_del(cptr->timer_ptr);
      
      cptr->cur_buf = (struct copy_buffer *)copy_linked_list_in_buf(cptr, now);

      cptr->cptr_paired->bytes_remaining = cptr->bytes;
      rpr_dump_req_resp_in_file(cptr, &conf_settings, (char *)cptr->cur_buf, cptr->cptr_paired->bytes_remaining);

      if(cptr->bytes > 0)
        rpr_write_data_on_socket(cptr->cptr_paired, now, (char *)cptr->cur_buf, &conf_settings);
      //rpr_write_data_on_socket(cptr->cptr_paired, now, (char *)cptr->cur_buf, conf_settings.recording_name, conf_settings);
      //copy_url_resp(cptr);
    }

    if(err)
    {
      cptr->bytes = 0;
      //free_cptr_buf(cptr);
      //Close_connection(cptr, 0, now, NS_REQUEST_BADBYTES, NS_COMPLETION_BAD_BYTES);
    }
    /*
    if(url_resp_buff)
      url_resp_buff[0] = 0;
    */
  }

  RPR_DL(cptr, "done = %d.", done);

  return done;
}

void rpr_dump_req_resp_in_file(connection *cptr, ConfigurationSettings *conf_settings, char *input_data, int input_data_len)
{
  SocketConfiguration *socket_conf = NULL;
  char *uncompressed_data = NULL;
  int uncompressed_data_len = 0;
  char actual_dump_file_path[MAX_FILE_NAME_LEN + 1];
  char decoded_dump_file_path[MAX_FILE_NAME_LEN + 1];
  int actual_file_fd = -1;
  int decoded_file_fd = -1;
  char *write_buf = input_data;
  int write_buf_len = input_data_len;
  char decode_error_msg[1024 + 1];

  RPR_DL(cptr, "Method called");
  RPR_DL(cptr, "input_data = %s, input_data_len = %d", input_data, input_data_len);

  if((input_data == NULL) || (input_data_len <= 0))
  {
    RPR_DL(cptr, "input_data_len = %d", input_data_len);
    return;
  }

  if(cptr->type == REQUEST)
  {
    socket_conf = &conf_settings->req_conf;
    /*bug id: 101320: ToDo: TBD with DJA  g_rpr_ta_dir=$NS_WDIR/workspace/<user_name>/<profile_name>/cavisson*/
    sprintf(actual_dump_file_path, "%s/default/default/%s/%s/%s/request/request_%d_%d", GET_NS_TA_DIR(), "scripts", g_rpr_script_name, conf_settings->recording_name, g_rpr_id, g_req_count);
    sprintf(decoded_dump_file_path, "%s_decoded", actual_dump_file_path);
    g_req_count++;
  }
  else if(cptr->type == RESPONSE)
  {
    socket_conf = &conf_settings->resp_conf;
    sprintf(actual_dump_file_path, "%s/default/default/%s/%s/%s/response/response_%d_%d", GET_NS_TA_DIR(), "scripts", g_rpr_script_name, conf_settings->recording_name, g_rpr_id, g_resp_count);
    sprintf(decoded_dump_file_path, "%s_decoded", actual_dump_file_path);
    g_resp_count++;
  }

  RPR_DL(cptr, "actual_dump_file_path with NS_TA_DIR(%s) = %s", GET_NS_TA_DIR(),  actual_dump_file_path);
  if(socket_conf->socket_end_policy_mode == RPR_DYNAMIC_LEN_SOCKET_EPM)
  {
    write_buf = input_data + socket_conf->socket_end_policy_value;
    write_buf_len = input_data_len - socket_conf->socket_end_policy_value;
  }
  
  RPR_DL(cptr, "socket_conf->socket_decoding_type = %d", socket_conf->socket_decoding_type);
  if(socket_conf->socket_decoding_type)
  {
    switch(socket_conf->socket_decoding_type)
    {
      case ENC_BINARY:
	RPR_DL(cptr, "BINARY -------");
        //uncompressed_data = nslb_encode_bin_to_base64(write_buf, write_buf_len, &uncompressed_data_len);
        //uncompressed_data = nslb_decode_base64_to_text(uncompressed_data, uncompressed_data_len, &uncompressed_data_len);
	uncompressed_data = nslb_dec_bin_to_text(write_buf, write_buf_len, &uncompressed_data_len);
        //uncompressed_data_len = strlen(uncompressed_data);
        break;
      case ENC_HEX:
        uncompressed_data = nslb_decode_hex_to_text(write_buf, write_buf_len, &uncompressed_data_len);
        break;
      case ENC_BASE64:
	RPR_DL(cptr, "BASE64 ------");
        uncompressed_data = nslb_decode_base64_to_text(write_buf, write_buf_len, &uncompressed_data_len);
	//uncompressed_data = nslb_encode_base64_to_bin(write_buf, write_buf_len, &uncompressed_data_len);
        //uncompressed_data = nslb_dec_bin_to_text(write_buf, write_buf_len);
        //uncompressed_data_len = strlen(uncompressed_data);
        break;
      default:
        rpr_error_log(_FLN_, NULL, "Invalid encoding type");
        break;
    } 

    RPR_DL(cptr, "uncompressed_data = %s, uncompressed_data_len = %d", uncompressed_data, uncompressed_data_len);

    decoded_file_fd = open(decoded_dump_file_path, O_CREAT|O_WRONLY|O_TRUNC|O_CLOEXEC, 0666);
    if(decoded_file_fd < 0) {
      rpr_error_log(_FLN_, cptr, "Unable to open file (%s) for writing. Error = %s", decoded_dump_file_path, strerror(errno));
      return;
    }

    if((uncompressed_data_len > 0) && (uncompressed_data != NULL))
    {
      if(write (decoded_file_fd, uncompressed_data, uncompressed_data_len) < 0)
         rpr_error_log(_FLN_, cptr, "Unable to write in file (%s). Error = %s", decoded_dump_file_path, strerror(errno));
    }
    else
    {
      snprintf(decode_error_msg, 1024, "Error: Unable to decode message. Msg type = %d", socket_conf->socket_decoding_type);
      if(write (decoded_file_fd, decode_error_msg, strlen(decode_error_msg)) < 0) 
         rpr_error_log(_FLN_, cptr, "Unable to write in file (%s). Error = %s", decoded_dump_file_path, strerror(errno));
    } 
    close(decoded_file_fd);
  }

  actual_file_fd = open(actual_dump_file_path, O_CREAT|O_WRONLY|O_TRUNC|O_CLOEXEC, 0666);
  if(actual_file_fd < 0) {
    rpr_error_log(_FLN_, cptr, "Unable to open file (%s) for writing. Error = %s", actual_dump_file_path, strerror(errno));
    return;
  }

  if(write (actual_file_fd, write_buf, write_buf_len) < 0)
     rpr_error_log(_FLN_, cptr, "Unable to write in file (%s). Error = %s", actual_dump_file_path, strerror(errno));   

  close(actual_file_fd);
}

static void create_service_time(connection *cptr, int proto, char *recording_name)
{
  int ser_fd = -1;
  char file_name[4096] = "\0";
  char input[1024];

  RPR_DL(cptr, "Method called");

  if(!recording_name)
    return;

  svc_time = 0;
  req_time_in_ms = 0;

  svc_time = ((res_time.tv_usec + (res_time.tv_sec * 1000000)) - (req_time.tv_usec + (req_time.tv_sec *1000000)))/1000;

  RPR_DL(cptr, "tv_usec %ld tv_sec %ld, svc_time = %lf\n", req_time.tv_usec, req_time.tv_sec, svc_time);
  req_time_in_ms = (((long)req_time.tv_usec / 1000) + ((long)req_time.tv_sec * 1000));
  RPR_DL(cptr, "req_time_in_ms =  %lu \n", req_time_in_ms);

  if(svc_time < 0 || svc_time > 2147483647){
    svc_time = 0;
    RPR_DL(cptr, "svc_time is out of bound set to 0");
    rpr_error_log(_FLN_, cptr, "svc_time is out of bound , set to 0\n");
  }
  RPR_DL(cptr, "*********svc_time = %lf***********", svc_time);


  //sprintf(file_name, "%s/service_%d_%d_%d", resp_dir, my_child_id, my_sub_child_id, req_resp_id);
  //sprintf(file_name, "%s/response_%d_%d_%d", conf_dir, my_child_id, my_sub_child_id, req_resp_id);
  /*bug id: 101320: ToDo: TBD with DJA*/
  sprintf(file_name, "%s/default/default/%s/%s/%s/conf/service_%d_%d", GET_NS_TA_DIR(), "scripts", g_rpr_script_name, recording_name, g_rpr_id, g_resp_count);

  RPR_DL(cptr, "Opening service file [%s] for writing ...", file_name);
  ser_fd = open(file_name, O_CREAT|O_WRONLY|O_EXCL|O_CLOEXEC, 0666);
  if(ser_fd < 0)
  {
    RPR_EXIT(-1, "Unable to open for writing request.\n", g_resp_count);
    //exit(-1);
  }
  RPR_DL(cptr, "service time SVC_TIME 2 = %04d ", (int)svc_time);
  sprintf(input, "SVC_TIME 2 %04d\n", (int)svc_time);
  if(write(ser_fd, input, strlen(input)) < 0)
    rpr_error_log(_FLN_, cptr, "Unable to write for g_resp_count = %d", g_resp_count);

  if(proto == SOCKET_TCPS_PROTOCOL)
    sprintf(input, "RECORDING_PARAMETERS SSL TCP\n");
  else
    sprintf(input, "RECORDING_PARAMETERS TCP\n");
  if(write(ser_fd, input, strlen(input)) < 0)
    rpr_error_log(_FLN_, cptr, "Unable to write for g_resp_count = %d", g_resp_count);

  //REQ_TIMESTAMP <time stamp in milli-seconds>
  RPR_DL(cptr, "req time stamp REQ_TIMESTAMP = %lu ", req_time_in_ms);
  sprintf(input, "REQ_TIMESTAMP %lu\n", req_time_in_ms);
  if(write(ser_fd, input, strlen(input)) < 0)
    rpr_error_log(_FLN_, cptr, "Unable to write for g_resp_count = %d", g_resp_count);

  close(ser_fd);
}

void rpr_write_data_on_socket(connection *cptr, u_ns_ts_t now, char *data_to_send, ConfigurationSettings *conf_settings)
{
  char *ptr_ssl_buff;
  int bytes;
  int ssl_errno;
  int to_send = 0;

  RPR_DL(cptr, "Method called, now = %ld, cptr->fd = %d", now, cptr->fd);
  //TODO: check below assignment as cptr->bytes_remainin is being updated in CR_HANDLE_EAGAIN
  to_send = cptr->bytes_remaining;
  while (to_send > 0)
  {
    ptr_ssl_buff = data_to_send + cptr->offset;
    RPR_DL(cptr, "Write: offset=%ld, to_sent = %d cptr->proto_type=%d", cptr->offset, to_send, cptr->proto_type);

    if(cptr->proto_type == SOCKET_TCPS_PROTOCOL)
    {
      ERR_clear_error();
      bytes = SSL_write(cptr->ssl, ptr_ssl_buff, to_send);
      RPR_DL(cptr, "bytes=%d",bytes);
      if (bytes < 0)
      {
        if ((ssl_errno = SSL_get_error(cptr->ssl, bytes)) == SSL_ERROR_WANT_WRITE)
        { 
	  SOCKET_TCP_HANDLE_EAGAIN_NEW
        }
        else
        { 
          RPR_DL(cptr, "Error in sending. SSL_write: err = %d",ssl_errno);
          rpr_error_log(_FLN_, cptr, "Error in sending. SSL_write: err = %d", ssl_errno);
          CLOSE_FD_CLOSE_CONN(cptr);
        }
      }
      else
        cptr->offset += bytes;
    }
    else
    {
      bytes = write(cptr->fd, ptr_ssl_buff, to_send);
      if (bytes < 0)
      {
        if (errno == EAGAIN)
        {
	  SOCKET_TCP_HANDLE_EAGAIN_NEW
        }
        else
        {
          rpr_error_log(_FLN_, cptr, "Error in sending. err = %d", errno);
          CLOSE_FD_CLOSE_CONN(cptr);
        }
      }
      else
        cptr->offset += bytes;
    }

    RPR_DL(cptr, "After complete write: offset=%lu, bytes = %d", cptr->offset, bytes);
    to_send -= bytes;
  }
  cptr->state = READ_REQUEST;

  RPR_DL(cptr, "Cleaning the kernel read queue, cptr->fd = %d", cptr->cptr_paired->fd);
  char buf[MAX_HDR_SIZE + 1];
  int bytes_read = read(cptr->cptr_paired->fd, buf, MAX_HDR_SIZE);
  RPR_DL(cptr, "bytes_read = %d", bytes_read);

  RPR_DL(cptr, "cptr->type = %d", cptr->type);
  if(cptr->type == RESPONSE)
  {
    gettimeofday(&req_time, NULL);
    
    if(conf_settings)
      start_idle_timer(cptr, conf_settings->resp_conf.first_byte_recv_timeout, get_ms_stamp()); //first_byte_recv_timeout timer added for server //TODO
  }
  else
  {
    gettimeofday(&res_time, NULL);

    if(conf_settings)
      create_service_time(cptr, cptr->proto_type, conf_settings->recording_name);

    CLOSE_FD_REUSE_CONN(cptr);
    if(conf_settings)
      start_idle_timer(cptr, conf_settings->req_conf.first_byte_recv_timeout, get_ms_stamp()); //first_byte_recv_timeout start for client
  }

  cptr->read_start_timestamp = now;
}

// Return 0 : Success and -1: Failure
int reset_idle_connection_timer(connection *cptr, u_ns_ts_t now, SocketConfiguration *socket_conf, ConfigurationSettings conf_settings)
{
  int elaps_response_time = 0, next_idle_time = 0;
  int max_timeout, idle_timeout;

  RPR_DL(cptr, "Method called. cptr->bytes = %d, cptr->timer_ptr->timer_type = %d, now = %ld", cptr->bytes, cptr->timer_ptr->timer_type, now);

  if(cptr->timer_ptr->timer_type == AB_TIMEOUT_IDLE)
  {
    if(!cptr->bytes)
    {
      //max_timeout = cptr->url_num->proto.socket_req.recv.rttfb_timeout;
      max_timeout = socket_conf->first_byte_recv_timeout;
      idle_timeout = max_timeout;
    }
    else
    {
      //max_timeout = cptr->url_num->proto.socket_req.recv.rtimeout;
      max_timeout = socket_conf->max_read_timeout;
      idle_timeout = cptr->timer_ptr->actual_timeout = socket_conf->idle_timeout;
    }
    RPR_DL(cptr, "max_timeout = %d, idle_timeout = %d, cptr->read_start_timestamp = %ld", max_timeout, idle_timeout, cptr->read_start_timestamp);

    //TODO
    //When Response time is greater then response_timer than setting timeOut and closing connection
    elaps_response_time = now - cptr->read_start_timestamp;

    RPR_DL(cptr, "elaps_response_time = %d", elaps_response_time);
    if(elaps_response_time > max_timeout)
    {
      if(!cptr->bytes)
        return -1;
      else
      {
	cptr->cptr_paired->bytes_remaining = cptr->bytes;
        //write data which is read till timeout has occured.
        rpr_write_data_on_socket(cptr->cptr_paired, now, cptr->cur_buf->buffer, &conf_settings);
      }
    }

    //For Last sample;  if idle sec is 10 ms and response_timeout is 45 ms, 
    // then on 5th iteration we will make timer->actual_timeout as 5 ms

    next_idle_time = max_timeout - elaps_response_time;
    RPR_DL(cptr, "[CPTR_TIMER] next_idle_time %d", next_idle_time);

    if((idle_timeout > 0) && (next_idle_time < idle_timeout))
      cptr->timer_ptr->actual_timeout = next_idle_time ;

    dis_idle_timer_reset(get_ms_stamp(), cptr->timer_ptr);
  }

  return 0;
}

/*******************************************************************
    Calculating how many bytes to be read for different read policy
       1. Read fix length
       2. Fix Length
       3. Delemiter
*******************************************************************/
void rpr_socket_bytes_to_read(connection *cptr, int *bytes_read, int *msg_peek, SocketConfiguration *socket_conf)
{
  RPR_DL(cptr, "Method Called: Bytes read = %d, cptr->bytes = %d, "
      "socket_conf->socket_end_policy_value = %d, socket_conf->socket_end_policy_mode = %d, cptr->content_length = %d",
       *bytes_read, cptr->bytes, socket_conf->socket_end_policy_value, socket_conf->socket_end_policy_mode, cptr->content_length);

  switch(socket_conf->socket_end_policy_mode)
  {
    case RPR_DYNAMIC_LEN_SOCKET_EPM:
    {
      if(cptr->content_length == -1) //calculating length bytes
        *bytes_read = socket_conf->socket_end_policy_value - cptr->bytes;
      else
      {
        *bytes_read = cptr->content_length - cptr->bytes;
      }
      break;
    }
    case RPR_FIX_LEN_SOCKET_EPM:
    {
      *bytes_read = ((socket_conf->socket_end_policy_value - cptr->bytes) > *bytes_read) ?
                      *bytes_read : (socket_conf->socket_end_policy_value - cptr->bytes);
      break;
    }
    case RPR_DELIMETER_SOCKET_EPM:
    {
      *msg_peek = 1;
      break;
    }
  }
  RPR_DL(cptr, "Method Exit: Bytes read = %d", *bytes_read);
}

/* Function used to read the tcp data from the socket.
 * 
 * cptr: pointer to the connection structure
 * 
 * Returns: Nothing.
 * */

void rpr_read_data_from_socket(connection *cptr, u_ns_ts_t now, ConfigurationSettings conf_settings)
{
  int bytes_read = MAX_HDR_SIZE, total_read = 0;
  char buf[MAX_HDR_SIZE + 1]; 
  //int timer_type;
  int ret = 0;
  int msg_peek = 0;
  int read_offset = 0;
  int i;
  SocketConfiguration *socket_conf = NULL;

  if(cptr->type == REQUEST)
    socket_conf = &conf_settings.req_conf;
  else if(cptr->type == RESPONSE)
    socket_conf = &conf_settings.resp_conf;
 
  RPR_DL(cptr, "Method called. fd = %d", cptr->fd);

  ret = reset_idle_connection_timer(cptr, now, socket_conf, conf_settings);

  if(ret != 0)
   return;

  buf[0] = '\0';
  //Read till no more data available (wouldblock).
  while (1)
  {
    if(!msg_peek)
      rpr_socket_bytes_to_read(cptr, &bytes_read, &msg_peek, socket_conf);
    else
    {
      msg_peek = 0;
      bytes_read = read_offset;
    }

    if (cptr->proto_type == SOCKET_TCPS_PROTOCOL)
    {
      if(!msg_peek)
        bytes_read = SSL_read(cptr->ssl, buf, bytes_read);
      else
        bytes_read = SSL_peek(cptr->ssl, buf, bytes_read);

      //if (bytes_read <= 0)
      if (bytes_read < 0)
      {
        switch (SSL_get_error(cptr->ssl, bytes_read)) {
          case SSL_ERROR_ZERO_RETURN:  /* means that the connection closed from the server */
            RPR_DL(cptr, "Got SSL_ERROR_ZERO_RETURN event");
            CLOSE_FD_CLOSE_CONN(cptr);
          case SSL_ERROR_WANT_READ:
            RPR_DL(cptr, "Got SSL_ERROR_WANT_READ event");
            return;
            /* It can but isn't supposed to happen */
          case SSL_ERROR_WANT_WRITE:
            /*If we are reading data, then in SSL case we can also get SSL_ERROR_WANT_WRITE event.
            *So, removed close_fd and bread the loop*/
            RPR_DL(cptr, "SSL_read error: SSL_ERROR_WANT_WRITE\n");
            return;
          case SSL_ERROR_SYSCALL: //Some I/O error occurred
            if (errno == EAGAIN) // no more data available, return (it is like SSL_ERROR_WANT_READ)
            {
              RPR_DL(cptr, "SSL_read: No more data available, return");
              return;
            }

            if (errno == EINTR)
            {
              RPR_DL(cptr, "SSL_read interrupted. Continuing...");
              continue;
            }
            /* FALLTHRU */
          case SSL_ERROR_SSL: //A failure in the SSL library occurred, usually a protocol error
            /* FALLTHRU */
          default:
            ERR_print_errors_fp(stderr);
            CLOSE_FD_CLOSE_CONN(cptr);
        }
      }
    }
    else
    {
      if(!msg_peek)
         bytes_read = read(cptr->fd, buf, bytes_read);
       else
         bytes_read = recv(cptr->fd, buf, bytes_read, MSG_PEEK);

      for(i = 0; i < bytes_read; i++)
        RPR_DL(NULL, "buf[%d] = 0x%x", i, buf[i]);

      RPR_DL(cptr, "read(). Bytes reads = %d", bytes_read);
      
      if (bytes_read < 0)
      {
        if (errno == EAGAIN)
        {
          RPR_DL(cptr, "Read: Got EAGAIN");
          return;
        } else if (errno == EINTR) {   /* this means we were interrupted */
          continue;
        }
        else
        {
          RPR_DL(cptr, "Read: closing");
          fprintf(stderr, "Error in reading SOCKET TCP request. error = %s, FD = %d\n", strerror(errno), cptr->fd);
          CLOSE_FD_CLOSE_CONN(cptr);
        }
      }
    }
    //buf[bytes_read] = '\0';
    total_read += bytes_read;
    //RPR_DL(cptr, "Read %d bytes. msg=%s", bytes_read, buf);

    ret = process_socket_recv_data(cptr, buf, bytes_read,  &read_offset, msg_peek, conf_settings, now); 

    if(bytes_read == 0)
    {
      if(cptr->is_data_complete)
      {
        RPR_DL(cptr, "Returning as bytes_read = %d and cptr->is_data_complete = %d", bytes_read, cptr->is_data_complete);

        if(cptr->type == REQUEST)
          close_fd(cptr, 1, now);

        /*if(cptr->type == RESPONSE)
        {
          RPR_DL(cptr, "Cleaning the kernel read queue");
          bytes_read = read(cptr->fd, buf, MAX_HDR_SIZE);
          RPR_DL(cptr, "bytes_read = %d", bytes_read);
        } */
        return;
      }
      else      
        CLOSE_FD_CLOSE_CONN(cptr);
    }

    if(ret == 1)
    {/*
      if(cptr->type == RESPONSE)
      {
        RPR_DL(cptr, "Cleaning the kernel read queue");
        bytes_read = read(cptr->fd, buf, MAX_HDR_SIZE);
        RPR_DL(cptr, "bytes_read = %d", bytes_read);
      }*/
      if((cptr->type == RESPONSE) && (cptr->cptr_paired->fd < 0))
        close_fd(cptr, 1, now);
      
      RPR_DL(cptr, "Goto epoll again");
      return; // If data is incomplete then read and EGAIN found then read after sometime 
    }
    else
    {
      RPR_DL(cptr, "Continue to read...");
      continue; // If Complete data is not read, then read till EGAIN
    } 
   
    #if 0
    if (end_policy_mode == RPR_TCP_SEGMENT_SOCKET_EPM)
    {
      if(tmp_ptr = strchr(buf, '\n'))
      {
        *tmp_ptr = '\0';
        total_read = strlen(buf);
      }
    }
    else if (end_policy_mode == RPR_DYNAMIC_LEN_SOCKET_EPM)
    {
      if(total_read >= prefix_length) {
        if(cptr->data_buf == NULL)
          req_len = socket_calc_req_length(buf, prefix_length);
        else
          req_len = socket_calc_req_length(cptr->data_buf, prefix_length);
      }
    }

    if((tmp_ptr != NULL) || (total_read == req_len))
    {
      if(cptr->data_buf == NULL)
      {
        RPR_DL(cptr, "Total bytes reads = %d, byte reads = %d",
                   total_read, bytes_read);

        //TODO: make this function workable
        //add_req_stat_and_count(cptr, now);
        //Got \n in first request
        proc_socket_tcp_req(cptr, now, buf, total_read, req_url_ptr, dest, conf_settings.socket_is_ssl, type);
        return;
      }
      //TODO: make this function workable
      //handle_hdr(cptr, buf, bytes_read, MM_TCP);
      //Breaking here as we need only break while loop
      break;
    }
    //TODO: make this function workable
    //handle_hdr(cptr, buf, bytes_read, MM_TCP);
    #endif
    buf[0] = '\0';
  }//end of while

  RPR_DL(cptr, "Total bytes reads = %d, byte reads = %d", total_read, bytes_read);

  //TODO: make this function workable
  //add_req_stat_and_count(cptr, now);
  //We are here as we have read full data
  //Now process the data and send
  //proc_socket_tcp_req(cptr, now, cptr->data_buf, total_read, req_url_ptr, dest, conf_settings.socket_is_ssl, type);
}

static int make_server_connection(connection * cptr, char * cur_host_name , int port)
{
  //Time out value for connect and SSL_connect.
  //int timeout_val = 60;
  char err_msg[2048];
  int ret = 0;
  int loc_fd = -1;
  //int event;
  RPR_DL(NULL, "Connection to recorder, cur_host_name = %s, port = %d\n ", cur_host_name, port);
  if(nslb_fill_sockaddr_ex(&(cptr->server_addr), cur_host_name, port, err_msg) == 0)
    RPR_EXIT(-1, "This is not valid Host");

  //create non-blocking socket.
  loc_fd = nslb_nb_open_socket(AF_INET, err_msg);
  RPR_DL(cptr,"fd value : %d, cptr : %p", loc_fd, cptr); 

  if(loc_fd < 0) {
    RPR_EXIT(-1, "Client Socket Failure in creation");
    return -1;
  }
  add_select((void *)cptr, loc_fd, EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP | EPOLLET | EPOLLOUT);

  RPR_DL(cptr,"fd value : %d, cptr : %p", loc_fd, cptr); 

  ret = nslb_nb_connect_addr(loc_fd, cptr->server_addr, (int *)&(cptr->state), err_msg);
  if(ret < 0)  
    RPR_DL(cptr,"client connection refused to connect. Error msg is : %s", err_msg);
     
  cptr->fd = loc_fd; 
  RPR_DL(cptr,"ret value : %d, fd value : %d, cptr : %p", ret, cptr->fd, cptr); 
  return ret; //handle connection refused to connect.
  
/*  fd = nslb_tcp_client_ex(cur_host_name, port, timeout_val, err_msg);
  if(fd >= 0)
    sprintf(trace_msg, "Connection made to service endpoint at IP '%s'\n", cur_host_name);

  if(fd < 0) {
    RPR_EXIT(-1, "Error: Unable to make connection to %s:%d. Reason: %s", cur_host_name, port, err_msg);
    return -1;
  }
  return fd;
*/  
}

int rpr_handle_cptr_state(connection * cptr, u_ns_ts_t now, ConfigurationSettings conf_settings)
{
  char err_msg[1024];
  int ret = 0;
  int loc_fd = cptr->fd;
  RPR_DL(cptr, "Method called.");
  switch (cptr->state) {
    case READ_REQUEST:
      RPR_DL(cptr, "State is READ_REQUEST");
      switch(cptr->proto_type) {
        case SOCKET_TCP_PROTOCOL:
        case SOCKET_TCPS_PROTOCOL:
          rpr_read_data_from_socket(cptr, now, conf_settings);    //read request for SOCKET TCP API
          break;

        default:
          RPR_DL(cptr, "Invalid protocol (%d)", cptr->proto_type);
          rpr_error_log(_FLN_, cptr, "Invalid protocol (%d)", cptr->proto_type);
      }
      break;
    case CNST_CONNECTING:
      RPR_DL(cptr,"State = %d , fd = %d , cptr = %p , proto_type = %d , ret = %d",cptr->state,cptr->fd,cptr,cptr->proto_type,ret); 
      ret = nslb_nb_connect_addr(loc_fd, cptr->server_addr, (int *)&(cptr->state), err_msg);
      cptr->fd = loc_fd;
      if( ret < 0) 
      {
        RPR_DL(cptr,"client connection refused to connect. Error msg is : %s and ret value is %d",err_msg,ret); 
        return -1; 
      } 
      else if(ret == 1) 
      { 
        RPR_DL(cptr, " Continuing because state is connecting.");
        return 1;
      }   
      
      if(cptr->proto_type == SOCKET_TCP_PROTOCOL)
        break;

    case CNST_SSLCONNECTING:
      RPR_DL(cptr, "State is %d", cptr->state);
      ssl_set_clients_local(cptr, cptr->fd, conf_settings.server_ip, conf_settings.resp_conf.socket_ssl_cert);
      break;
    case SSL_ACCEPTING:
      RPR_DL(cptr, "Protocol[%d], State is SSL_ACCEPTING, starttls[%d]", cptr->proto_type, cptr->starttls);
      handle_ssl_accept( cptr, now );
      break;
    
    case PROCESS_SOCKET_TCP_WRITE:
      RPR_DL(cptr, "State is PROCESS_SOCKET_TCP_WRITE");
      rpr_write_data_on_socket(cptr, now, cptr->cptr_paired->cur_buf->buffer, &conf_settings);
      break;

	    default: /* case CNST_FREE */
	      RPR_DL(cptr, "Unexpected state. State is %d", cptr->state);
	      rpr_error_log(_FLN_, cptr, "Unexpected state. State is %d", cptr->state);
	      CLOSE_FD_CLOSE_CONN(cptr);
	  }
	}

        
/* CHILD PROCESS or non-parallel process */
static void rpr_child()
{
  // Changed to malloc as we need to allocated based on calculation on Jul 28, 11
  struct epoll_event *pfds;
  int ii;
  //int jj=0;
  timer_type *temp_tmr;
  u_ns_ts_t now;
  int temp_ms, r;
  connection *client_cptr;
  connection *server_cptr;
  connection *cptr;
  int fd = -1;
  char log_file[1024];
  char rpr_error_log_file[1024];
  char *port_num;
  char child_name[512];
  int file_on_index = -1;
  int ret = 0; 
  ConfigurationSettings conf_settings;
  
  RPR_DL(NULL, "Method called.");
  set_random_seed();

  g_rpr_id = atoi(getenv("CHILD_INDEX"));
  if(g_rpr_id < 0)
  {
    RPR_EXIT(-1, "CHILD_INDEX is not set: err = %s", strerror(errno));
  }

  RPR_DL(NULL, "g_rpr_id = %d.", g_rpr_id);
  // This must be called after g_rpr_id is set for child
  close_log_fds(); //this will close log fds that opened by parent
  
  //now = get_ms_stamp();
  
  if(g_rpr_num_childs > 1)
    file_on_index = g_rpr_id -1;
  else 
    file_on_index = g_rpr_id;

  //TODO: parse response based settings also
  rpr_parse_conf_file(file_on_index, &conf_settings);

  rpr_create_child_script_dir(conf_settings);

  create_listener_for_ipv4(NULL, conf_settings);  
  //create_listener_for_ipv4("10.10.40.18", conf_settings);  

  init_ssl();

  //listen_for_ipv6_protocols(); // This will be remove once we implement _ex for ipv6

  sprintf(log_file, "%s/logs/rpr_debug.%d.log", rpr_conf_dir, g_rpr_id);
  sprintf(error_log_file, "%s/logs/rpr_error.%d.log", rpr_conf_dir, g_rpr_id);
  sprintf(child_name, "child-%d", g_rpr_id);

  nslb_util_set_log_filename(log_file, rpr_error_log_file);

  RPR_DL(NULL, "Setting callback for SIGUSR2 in child");
  (void) signal( SIGUSR2, handle_sigusr2 );
  (void) signal( SIGCHLD, SIG_IGN );
  (void) signal( SIGPIPE, handle_sigpipe );
  (void) signal( SIGTERM, handle_sigterm );
  (void) signal( SIGINT, handle_sigint );
  (void) signal( SIGRTMIN+1, handle_sigrtmin1 );

  //  sleep(10);
  ns_epoll_init();
  MY_MALLOC(pfds, sizeof(struct epoll_event) * total_max_epoll_fds, NULL, "epoll event");
  //user_order_table_size = max_var_table_idx * sizeof(int);

  //TODO: add fd with EPOLLOUT in epoll
  add_listner_port_in_epoll(conf_settings.socket_is_ssl);

  printf("Ready to Record for port: %d\n", conf_settings.recording_port);
  fflush(NULL);

  for (;;)  // For ever loop
  {
    now = get_ms_stamp();
    temp_tmr = dis_timer_next( now );
    if (temp_tmr == NULL) {
      //printf("No timeout\n");
      temp_ms = -1;
    }  else {
    temp_ms = temp_tmr->timeout - now;
    if (temp_ms < 0)
      temp_ms = 0;
    }
    RPR_DL(NULL, "About to sleep for %d msec", temp_ms);
    
    int afd, is_listen_fd = 0, tmp;
    void *ptr;
    int proto_type_local;
    //int events;

    r = epoll_wait(v_epoll_fd, pfds, total_max_epoll_fds, temp_ms);

    // RPR_DL(NULL, "epoll_wait() returned %d", r);
    if ( r < 0 ) {
      if (errno != EINTR)
        rpr_error_log(_FLN_, NULL, "select/epoll_wait: err = %s", strerror(errno));
      continue;
    }
    //now = get_ms_stamp();

    RPR_DL(NULL, "Got (%d) events. now = %ld", r, now);

    for (ii = 0; ii < r; ii++)  // For all fds which are have data/con req/error etc
    {
      is_listen_fd = 0;
      proto_type_local = 0;
      ptr = (void *)pfds[ii].data.ptr;
      //events = pfds[ii].events;
      memcpy(&tmp, ptr, sizeof(int));
    
      RPR_DL(NULL, "Processing events %d, tmp = %d", ii, tmp);

      switch(*(char *) ptr)
      {
        case RPR_CON_TYPE_LISTENER: //New request came on child
          RPR_DL(NULL, "Event on listen fd = %d, protocol = %d ",
                             ((listener *)ptr)->fd, ((listener*)ptr)->protocol);
          is_listen_fd = 1;
          afd = ((listener *)ptr)->fd;
          proto_type_local = ((listener*)ptr)->protocol;
          break;

        case RPR_CON_TYPE_CPTR: //This is old request
          fd = ((listener *)ptr)->fd;
          cptr = ptr;
          /* We have a special case of FTP data listener connection. In
           * this case we will mark it listener and accept connection
           * on it. */
          proto_type_local = cptr->proto_type;
          RPR_DL(cptr, "fd= %d, proto_type_local = %d, cptr->state=%d, cptr = %p", cptr->fd, proto_type_local, cptr->state, cptr);
          break;
          
       default:
          fprintf(stderr, "Type of FD is not valid. It should not happen\n");
          continue;
      }

      if (is_listen_fd) {

        if(afd == -1)
          continue;
        RPR_DL(NULL, "Entering while loop");
        while ((fd = accept(afd, NULL, 0)) >= 0)
        {
          RPR_DL(NULL, "got fd from accept = %d, proto_type_local = %d", fd, proto_type_local);
          if ( fcntl( fd, F_SETFL, O_NDELAY ) < 0 ) {
            rpr_error_log(_FLN_, NULL, "fcntl failed: err = %s", strerror(errno));
            close(fd);
            break;
          }
           
          client_cptr = get_free_user_slot(fd);
          if (!client_cptr) {
            rpr_error_log(_FLN_, NULL, "unable to alloc user slot");
            close(fd);
          } else {
            cptr = client_cptr;

            start_idle_timer(client_cptr, conf_settings.req_conf.first_byte_recv_timeout, get_ms_stamp()); //first_byte_recv_timeout timer added
            client_cptr->read_start_timestamp = now; //Set it adjust timer
            client_cptr->timer_ptr->actual_timeout = conf_settings.req_conf.first_byte_recv_timeout;
           
            //client_cptr->state = READ_REQUEST; 
          
            //TODO: need to check this
            //start_init_idle_timer(cptr, 30000, now); /* hpd_global_config->first_req_timeout is used for both smtp and HTTP + all protocols*/
            client_cptr->fd = fd;
            client_cptr->conf_settings = &conf_settings;
 
            add_select(client_cptr, client_cptr->fd, EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP);

            server_cptr = get_free_user_slot(fd);
            server_cptr->conf_settings = &conf_settings;
            client_cptr->cptr_paired = (struct connection *)server_cptr;
            server_cptr->cptr_paired = (struct connection *)client_cptr;
            client_cptr->type = REQUEST; 
            server_cptr->type = RESPONSE;
	    // to make connection with endpoint server
            if((ret = make_server_connection(server_cptr, conf_settings.server_ip, conf_settings.socket_port)) < 0) {
	      rpr_error_log(_FLN_, NULL, "Error: Unable to make connection to [%s:%d]. cptr->fd = -1", conf_settings.server_ip, conf_settings.socket_port);
	      return;
  	    }

            switch(proto_type_local) {
              case SOCKET_TCP_PROTOCOL:
                RPR_DL(client_cptr, "New connection made for SOCKET TCP request");
                if((port_num = strstr(nslb_get_src_addr_ex(client_cptr->fd, 1), ":")) != NULL)
                {
                  RPR_DL(client_cptr, " nslb_get_src_addr_ex PASSED. ");
		  port_num++;
                  // take endpoint fd on cptr
                  client_cptr->proto_type = SOCKET_TCP_PROTOCOL;
                  server_cptr->proto_type = SOCKET_TCP_PROTOCOL;
		  if(ret == 1){
		  RPR_DL(client_cptr, " Continuing because state is connecting."); 
		    continue; 
		  }
                }
                break;
              case SOCKET_TCPS_PROTOCOL:
                RPR_DL(client_cptr, "New SSL connection mode for SOCKET_TCPS_PROTOCOL request");
                if((port_num = strstr(nslb_get_src_addr_ex(client_cptr->fd, 1), ":")) != NULL)
                {
                  RPR_DL(client_cptr, "port_num = %s, client_cptr->fd = %d", port_num, client_cptr->fd);
                  port_num++;
                  client_cptr->proto_type = SOCKET_TCPS_PROTOCOL;
                  server_cptr->proto_type = SOCKET_TCPS_PROTOCOL;
                  RPR_DL(client_cptr, "Calling set_ssl");
                  //To accept SSL connection from client.
                  set_ssl(client_cptr, now);
                  //To make SSL connection with endpoint server
                  RPR_DL(client_cptr, "Calling ssl_set_clients. server_cptr->fd = %d, conf_settings.server_ip = %s, conf_settings.resp_conf.socket_ssl_cert = %s", server_cptr->fd, conf_settings.server_ip, conf_settings.resp_conf.socket_ssl_cert);
		  if(ret == 1)
		   {	
		   RPR_DL(client_cptr, " Continuing because state is connecting."); 
		   continue; 
		   }
		  server_cptr->ssl = ssl_set_clients_local(server_cptr, server_cptr->fd, conf_settings.server_ip, conf_settings.resp_conf.socket_ssl_cert);
                }
                break;
            } // end of switch
          } // end of else 
        } // else of while accept loop

        RPR_DL(NULL, "Out of while accept");
        continue;
      } // end of is_listen_fd if condition

      if(cptr->fd == -1)
      {
        RPR_DL(NULL, "Continuing the loop as client fd is -1.");
        continue;
      }
/*
      if(events & (EPOLLRDHUP | EPOLLERR | EPOLLHUP)) 
      {
        RPR_DL(NULL, "Closing the connection as recorder got HUP/ERR event.");
        close_fd(cptr, 1, now);
        continue;
      }
*/
      ret = rpr_handle_cptr_state(cptr, now, conf_settings);
      if(ret < 0)
      {  
	RPR_DL(cptr,"client connection refused to connect.");
        return;
      }
      else if (ret == 1)
      { 
        RPR_DL(cptr, " Continuing because state is connecting.");
        continue;
      }
    } // End of for loop
  } // End of for(;;) loop
  dis_timer_run( now );
}

static int rpr_get_tokens(char *read_buf, char *fields[], char *token, int max_flds)
{
  int totalFlds = 0;
  char *ptr;
  char *token_ptr;

  ptr = read_buf;
  while((token_ptr = strtok(ptr, token)) != NULL)
  {
    ptr = NULL;
    totalFlds++;
    if(totalFlds > max_flds)
    {
      totalFlds = max_flds;
      break;  /* break from while */
    }
    fields[totalFlds - 1] = token_ptr;
  }
  return(totalFlds);
}


void rpr_parse_arguments(int argc, char* argv[])
{
  char opt;

  //argument parsing
  while((opt = getopt(argc, argv, "f:d:n:t:")) != -1)
  {
    switch(opt)
    {
      case 'f':
        conf_file_name_count = rpr_get_tokens(optarg, conf_file_name_list, ",", MAX_TOKEN);
        break;

      case 'd':
        debug_log = atoi(optarg);
        break;

      case 'n':  // Script Name
        g_rpr_script_name = strdup(optarg);
        break;

      case 't':  // Script Name
        g_rpr_timeout = atol(optarg);
        break;

      case '?':
        RPR_EXIT(-1, "Error: Invalid arguments");
        break;
    }
  }
  g_rpr_num_childs = conf_file_name_count;
  //RPR_DL(NULL, "Num child = %d", g_rpr_num_childs);
}

void rpr_init_settings(ConfigurationSettings *conf_settings)
{
  RPR_DL(NULL, "Method called.");

  conf_settings->recording_port = 21031; 
  strcpy(conf_settings->recording_name, "RPR_Default_Recording");

  conf_settings->req_conf.first_byte_recv_timeout 	= RPR_FIRST_BYTE_RECV_TIMEOUT;
  conf_settings->req_conf.idle_timeout 			= RPR_IDLE_TIMEOUT;
  conf_settings->req_conf.max_read_timeout 		= RPR_MAX_READ_TIMEOUT;

  conf_settings->req_conf.socket_decoding_type		= MSG_TYPE_TEXT;
  conf_settings->req_conf.socket_end_policy_mode 	= RPR_TCP_SEGMENT_SOCKET_EPM;
  conf_settings->req_conf.socket_end_policy_value	= RPR_TCP_SEGMENT_SOCKET_EPM;

  conf_settings->req_conf.socket_dynamic_len_type 	= LEN_TYPE_TEXT;
  //conf_settings->req_conf.socket_delimeter[MAX_DELIMETER_SIZE];
  conf_settings->req_conf.read_msg_max_len 		= 0;       /*Max msg len to be read*/
  conf_settings->req_conf.len_endian 			= SLITTLE_ENDIAN;

  //Response
  conf_settings->resp_conf.first_byte_recv_timeout  	= RPR_FIRST_BYTE_RECV_TIMEOUT;
  conf_settings->resp_conf.idle_timeout                 = RPR_IDLE_TIMEOUT;
  conf_settings->resp_conf.max_read_timeout             = RPR_MAX_READ_TIMEOUT;
                                                                                         
  conf_settings->resp_conf.socket_decoding_type         = MSG_TYPE_TEXT;
  conf_settings->resp_conf.socket_end_policy_mode  	= RPR_TCP_SEGMENT_SOCKET_EPM;
  conf_settings->resp_conf.socket_end_policy_value	= RPR_TCP_SEGMENT_SOCKET_EPM;

  conf_settings->resp_conf.socket_dynamic_len_type	= LEN_TYPE_TEXT;
  //conf_settings->resp_conf.socket_delimeter[MAX_DELIMETER_SIZE];
  conf_settings->resp_conf.read_msg_max_len 		= 0;       /*Max msg len to be read*/
  conf_settings->resp_conf.len_endian 			= SLITTLE_ENDIAN;

}

//TODO: parsing of response based settings is still remaining
void rpr_parse_conf_file(int child_id, ConfigurationSettings *conf_settings)
{
  FILE *fp_rpr_conf;
  char rpr_conf_file_path[MAX_LOCAL_BUFF_SIZE];
  int num;
  int is_port_provided = 0;
  int is_server_ip_provided = 0;

  char buff[MAX_LOCAL_BUFF_SIZE + 1];
  char keyword[MAX_LOCAL_BUFF_SIZE] ;
  char text1[MAX_LOCAL_BUFF_SIZE];
  char text2[MAX_LOCAL_BUFF_SIZE];
  char text3[MAX_LOCAL_BUFF_SIZE];
  char text4[MAX_LOCAL_BUFF_SIZE];

  fp_rpr_conf = NULL;
  num = 0;

  RPR_DL(NULL, "Method called for child_id %d", child_id);
  if(child_id < 0)
    RPR_EXIT(-1, "Error: Child id cannot be smaller than 0.");
    
  sprintf(rpr_conf_file_path, "%s/%s", rpr_conf_dir, conf_file_name_list[child_id]);

  RPR_DL(NULL, "Opening file to read configuration. File: %s", rpr_conf_file_path);
  if((fp_rpr_conf = fopen(rpr_conf_file_path, "r")) == NULL)
    RPR_EXIT(-1, "Error: Unable to open configuration file: [%s]", rpr_conf_file_path);

  rpr_init_settings(conf_settings);

  while(fgets(buff, MAX_LOCAL_BUFF_SIZE, fp_rpr_conf))
  {
    // Replace new line by Null 
    buff[strlen(buff) - 1] = '\0';

    //ignore comented and blank line
    if((buff[0] == '#') || buff[0] == '\0')
      continue;

    num = sscanf(buff, "%s %s %s %s %s", keyword, text1, text2, text3, text4);
    RPR_DL(NULL, "keyword = %s, text1 = %s, text2 = %s, text3= %s, text4 = %s, num = %d", keyword, text1, text2, text3, text4, num);
    
    //Common settings
    if(!strcmp(keyword, "SOCKET_PORT"))
    {
      if(num != 2)
        continue;
      conf_settings->socket_port = atoi(text1); 
      conf_settings->socket_is_ssl = 0;
      is_port_provided = 1;
    }
    else if(!strcmp(keyword, "SOCKET_SPORT"))
    {
      if(num != 2)
        continue;
      conf_settings->socket_port = atoi(text1); 
      conf_settings->socket_is_ssl = 1;
      is_port_provided = 1;
    }
    else if(!strcmp(keyword, "SOCKET_PROTOCOL"))
    {
      if(num != 2)
        continue;
      strcpy(conf_settings->socket_protocol, text1); 
    }
    else if(!strcmp(keyword, "RECORDING_NAME"))
    {
      if(num != 2)
        continue;
      strcpy(conf_settings->recording_name, text1);
    }
    else if(!strcmp(keyword, "RECORDING_PORT"))
    {
      if(num != 2)
        continue;
      conf_settings->recording_port = atoi(text1);
    }
    else if(!strcmp(keyword, "SERVER_IP"))
    {
      if(num != 2)
        continue;
      strcpy(conf_settings->server_ip, text1);
      is_server_ip_provided = 1;
    }
    // Timeout settings. It is common for request and response
    else if(!strcmp(keyword, "FIRST_BYTE_RECV_TIMEOUT"))
    {
      if(num != 2)
        continue;
      conf_settings->req_conf.first_byte_recv_timeout = atoi(text1);
    }
    else if(!strcmp(keyword, "FIRST_BYTE_SEND_TIMEOUT"))
    {
      if(num != 2)
        continue;
      conf_settings->resp_conf.first_byte_recv_timeout = atoi(text1);
    }
    else if(!strcmp(keyword, "REQ_IDLE_TIMEOUT")) 
    {
      if(num != 2)
        continue;
      conf_settings->req_conf.idle_timeout = atoi(text1);
    }
    else if(!strcmp(keyword, "RESP_IDLE_TIMEOUT")) 
    {
      if(num != 2)
        continue;
      conf_settings->resp_conf.idle_timeout = atoi(text1);
    }
    else if(!strcmp(keyword, "KEEP_ALIVE_TIMEOUT")) // Overall connection timeout
    {
      if(num != 2)
        continue;
      conf_settings->keep_alive_timeout = atoi(text1);
    }
    else if(!strcmp(keyword, "MAX_READ_TIMEOUT")) // Overall timeout
    {
      if(num != 2)
        continue;
      conf_settings->req_conf.max_read_timeout = atoi(text1);
    }
    else if(!strcmp(keyword, "MAX_WRITE_TIMEOUT")) // Overall timeout
    {
      if(num != 2)
        continue;
      conf_settings->resp_conf.max_read_timeout = atoi(text1);
    }
    // Request specific settings
    else if(!strcmp(keyword, "SOCKET_REQ_MSG_TYPE"))
    {
      if(num != 2)
        continue;

      if(!strcasecmp(text1, "text"))
        conf_settings->req_conf.socket_decoding_type = MSG_TYPE_TEXT;
      else if(!strcasecmp(text1, "binary"))
        conf_settings->req_conf.socket_decoding_type = MSG_TYPE_BINARY;
      else if(!strcasecmp(text1, "hex"))
        conf_settings->req_conf.socket_decoding_type = MSG_TYPE_HEX;
      else if(!strcasecmp(text1, "base64"))
        conf_settings->req_conf.socket_decoding_type = MSG_TYPE_BASE64;
      else
      {
        //rpr_error_log(_FLN_, NULL, "Invalid Message-Type value, it can be 'Text', 'Binary', 'Hex' or 'Base64'");
        RPR_EXIT(-1, "Error: Invalid Message-Type value, it can be 'Text', 'Binary', 'Hex' or 'Base64' in configuration file: [%s]", rpr_conf_file_path);
      }
    }
    else if(!strcmp(keyword, "SOCKET_REQ_END_POLICY"))
    {
      if(num < 2)
        continue;
      conf_settings->req_conf.socket_end_policy_mode = atoi(text1);

      if(conf_settings->req_conf.socket_end_policy_mode == RPR_FIX_LEN_SOCKET_EPM)
        conf_settings->req_conf.socket_end_policy_value = atol(text2);
      else if(conf_settings->req_conf.socket_end_policy_mode == RPR_DELIMETER_SOCKET_EPM)
        strncpy(conf_settings->req_conf.socket_delimeter, text2, MAX_DELIMETER_SIZE);
      else if(conf_settings->req_conf.socket_end_policy_mode == RPR_DYNAMIC_LEN_SOCKET_EPM)
      {
        conf_settings->req_conf.socket_end_policy_value = atol(text2);
        if(!strcasecmp(text3, "text"))
          conf_settings->req_conf.socket_dynamic_len_type = LEN_TYPE_TEXT;
        else if(!strcasecmp(text3, "binary"))
        {
          conf_settings->req_conf.socket_dynamic_len_type = LEN_TYPE_BINARY;
          if(!strcasecmp(text4, "L"))
            conf_settings->req_conf.len_endian = SLITTLE_ENDIAN;
          else if(!strcasecmp(text4, "B"))
            conf_settings->req_conf.len_endian = SBIG_ENDIAN;
          else
            RPR_EXIT(-1, "Invalid Length-Endian provided, it can be Little(L) or Big(B)");
        }
        else
        {
          //rpr_error_log(_FLN_, NULL, "Invalid Length-Type value, it can be 'Text' or 'Binary'");
          RPR_EXIT(-1, "Error: Invalid Length-Type value, it can be 'Text' or 'Binary' in configuration file: [%s]", rpr_conf_file_path);
        }
      }
      else if(conf_settings->req_conf.socket_end_policy_mode == RPR_REQ_READ_TIMEOUT_SOCKET_EPM)
        RPR_DL(NULL, "Req end policy is Timeout.");
      else if(conf_settings->req_conf.socket_end_policy_mode == RPR_REQ_CONTAINS_SOCKET_EPM)
        strncpy(conf_settings->req_conf.socket_delimeter, text2, MAX_DELIMETER_SIZE);
      else if(conf_settings->req_conf.socket_end_policy_mode == RPR_CONN_CLOSE_SOCKET_EPM)
        RPR_DL(NULL, "Req end policy is Close Connection.");
    }
    else if(!strcmp(keyword, "SOCKET_SSL_CLIENT_CERT"))
    {
      if(num != 2)
        continue;
      strcpy(conf_settings->req_conf.socket_ssl_cert, text1); 
    }
    // Response specific settings
    else if(!strcmp(keyword, "SOCKET_RESP_MSG_TYPE"))
    {
      if(num != 2)
        continue;

      if(!strcasecmp(text1, "text"))
        conf_settings->resp_conf.socket_decoding_type = MSG_TYPE_TEXT;
      else if(!strcasecmp(text1, "binary"))
        conf_settings->resp_conf.socket_decoding_type = MSG_TYPE_BINARY;
      else if(!strcasecmp(text1, "hex"))
        conf_settings->resp_conf.socket_decoding_type = MSG_TYPE_HEX;
      else if(!strcasecmp(text1, "base64"))
        conf_settings->resp_conf.socket_decoding_type = MSG_TYPE_BASE64;
      else
      {
        //rpr_error_log(_FLN_, NULL, "Invalid Message-Type value, it can be 'Text', 'Binary', 'Hex' or 'Base64'");
        RPR_EXIT(-1, "Error: Invalid Message-Type value, it can be 'Text', 'Binary', 'Hex' or 'Base64' in configuration file: [%s]", rpr_conf_file_path);
      }
    }
    else if(!strcmp(keyword, "SOCKET_RESP_END_POLICY"))
    {
      if(num < 2)
        continue;
      conf_settings->resp_conf.socket_end_policy_mode = atoi(text1);
      
      if(conf_settings->resp_conf.socket_end_policy_mode == RPR_FIX_LEN_SOCKET_EPM)
        conf_settings->resp_conf.socket_end_policy_value = atol(text2);
      else if(conf_settings->resp_conf.socket_end_policy_mode == RPR_DELIMETER_SOCKET_EPM)
        strncpy(conf_settings->resp_conf.socket_delimeter, text2, MAX_DELIMETER_SIZE); 
      else if(conf_settings->resp_conf.socket_end_policy_mode == RPR_DYNAMIC_LEN_SOCKET_EPM)
      {
        conf_settings->resp_conf.socket_end_policy_value = atol(text2);
        if(!strcasecmp(text3, "text"))
          conf_settings->resp_conf.socket_dynamic_len_type = LEN_TYPE_TEXT;
        else if(!strcasecmp(text3, "binary"))
        {
          conf_settings->resp_conf.socket_dynamic_len_type = LEN_TYPE_BINARY;
          if(!strcasecmp(text4, "L"))
            conf_settings->resp_conf.len_endian = SLITTLE_ENDIAN;
          else if(!strcasecmp(text4, "B"))
            conf_settings->resp_conf.len_endian = SBIG_ENDIAN;
          else
            RPR_EXIT(-1, "Invalid Length-Endian provided, it can be Little(L) or Big(B)");
        }
        else
        { 
          //rpr_error_log(_FLN_, NULL, "Invalid Length-Type value, it can be 'Text' or 'Binary'");
          RPR_EXIT(-1, "Error: Invalid Length-Type value, it can be 'Text' or 'Binary' in configuration file: [%s]", rpr_conf_file_path);
        }
      }
      else if(conf_settings->resp_conf.socket_end_policy_mode == RPR_REQ_READ_TIMEOUT_SOCKET_EPM)
        RPR_DL(NULL, "Response end policy is Timeout.");
      else if(conf_settings->resp_conf.socket_end_policy_mode == RPR_REQ_CONTAINS_SOCKET_EPM)
        strncpy(conf_settings->resp_conf.socket_delimeter, text2, MAX_DELIMETER_SIZE);
      else if(conf_settings->resp_conf.socket_end_policy_mode == RPR_CONN_CLOSE_SOCKET_EPM)
        RPR_DL(NULL, "Response end policy is Close Connection.");
    }
    else if(!strcmp(keyword, "SOCKET_SSL_SERVER_CERT"))
    {
      if(num != 2)
        continue;
      strcpy(conf_settings->resp_conf.socket_ssl_cert, text1); 
    }
    else
    {
      RPR_EXIT(-1, "Error: Invalid Keyword [%s] provided in configuration file: [%s].", keyword, rpr_conf_file_path);
    }
  }
  fclose(fp_rpr_conf);

  if(!is_port_provided)
    RPR_EXIT(-1, "Error: No SOCKET_PORT/SOCKET_SPORT is provided in configuration file: [%s].", rpr_conf_file_path);

  if(!is_server_ip_provided)
    RPR_EXIT(-1, "Error: No SERVER_IP is provided in configuration file: [%s].", rpr_conf_file_path);

  //After parsing complete data, set max_msglen value for request
  if(conf_settings->req_conf.socket_end_policy_mode == RPR_DYNAMIC_LEN_SOCKET_EPM)
  {
    if(conf_settings->req_conf.socket_dynamic_len_type == LEN_TYPE_BINARY)
    {
      RPR_DL(NULL, "conf_settings->req_conf.socket_end_policy_value = %d", conf_settings->req_conf.socket_end_policy_value);
 
      switch(conf_settings->req_conf.socket_end_policy_value)
      {
        case RPR_SOCKET_MSGLEN_BYTES_CHAR:
          conf_settings->req_conf.read_msg_max_len = UCHAR_MAX;             // UCHAR_MAX = 255
          break;
        case RPR_SOCKET_MSGLEN_BYTES_SHORT:
          conf_settings->req_conf.read_msg_max_len = USHRT_MAX;            // USHRT_MAX = 65535 = ~65K
          break;
        case RPR_SOCKET_MSGLEN_BYTES_INT:
          conf_settings->req_conf.read_msg_max_len = UINT_MAX;             // UINT_MAX = 4294967295 = ~4GB
          break;
        case RPR_SOCKET_MSGLEN_BYTES_LONG:
          conf_settings->req_conf.read_msg_max_len = ULONG_MAX;            // ULONG_MAX = 18446744073709551615 = ~18446744 TB
          break;
        default:
          RPR_EXIT(-1, "Error: Wrong Length-Bytes value passed for LEN_TYPE Binary, it can be 1,2,4 or 8 only in configuration file: [%s].", rpr_conf_file_path);
          //rpr_error_log(_FLN_, NULL, "Wrong Length-Bytes value passed for LEN_TYPE Binary, it can be 1,2,4 or 8 only");
      }
    }
    else if(conf_settings->req_conf.socket_dynamic_len_type == LEN_TYPE_TEXT)
    {
      // Max possible value in provided number of digits
      if(conf_settings->req_conf.socket_end_policy_value > 20)
        //rpr_error_log(_FLN_, NULL, "Maximum Length-Bytes value for LEN_TYPE Text can be 20");
        RPR_EXIT(-1, "Error: Maximum Length-Bytes value for LEN_TYPE Text can be 20 in configuration file: [%s].", rpr_conf_file_path);
 
      conf_settings->req_conf.read_msg_max_len = pow(10, conf_settings->req_conf.socket_end_policy_value) - 1;
    }
  }
  
  //After parsing complete data, set max_msglen value for response
  if(conf_settings->resp_conf.socket_end_policy_mode == RPR_DYNAMIC_LEN_SOCKET_EPM)
  {
    if(conf_settings->resp_conf.socket_dynamic_len_type == LEN_TYPE_BINARY)
    {
      RPR_DL(NULL, "conf_settings->resp_conf.socket_end_policy_value = %d", conf_settings->resp_conf.socket_end_policy_value);
 
      switch(conf_settings->resp_conf.socket_end_policy_value)
      {
        case RPR_SOCKET_MSGLEN_BYTES_CHAR:
          conf_settings->resp_conf.read_msg_max_len = UCHAR_MAX;             // UCHAR_MAX = 255
          break;
        case RPR_SOCKET_MSGLEN_BYTES_SHORT:
          conf_settings->resp_conf.read_msg_max_len = USHRT_MAX;            // USHRT_MAX = 65535 = ~65K
          break;
        case RPR_SOCKET_MSGLEN_BYTES_INT:
          conf_settings->resp_conf.read_msg_max_len = UINT_MAX;             // UINT_MAX = 4294967295 = ~4GB
          break;
        case RPR_SOCKET_MSGLEN_BYTES_LONG:
          conf_settings->resp_conf.read_msg_max_len = ULONG_MAX;            // ULONG_MAX = 18446744073709551615 = ~18446744 TB
          break;
        default:
          RPR_EXIT(-1, "Error: Wrong Length-Bytes value passed for LEN_TYPE Binary, it can be 1,2,4 or 8 only in configuration file: [%s].", rpr_conf_file_path);
          //rpr_error_log(_FLN_, NULL, "Wrong Length-Bytes value passed for LEN_TYPE Binary, it can be 1,2,4 or 8 only");
      }
    }
    else if(conf_settings->resp_conf.socket_dynamic_len_type == LEN_TYPE_TEXT)
    {
      // Max possible value in provided number of digits
      if(conf_settings->resp_conf.socket_end_policy_value > 20)
        RPR_EXIT(-1, "Error: Maximum Length-Bytes value for LEN_TYPE Text can be 20 in configuration file: [%s].", rpr_conf_file_path);
        //rpr_error_log(_FLN_, NULL, "Maximum Length-Bytes value for LEN_TYPE Text can be 20");
 
      conf_settings->req_conf.read_msg_max_len = pow(10, conf_settings->resp_conf.socket_end_policy_value) - 1;
    }
  }
}

//exclusive create a lock file
void check_uniq_instance() {
  static char buf[1024];
  //int fd;
  int pid;
  FILE *fp;

  RPR_DL(NULL, "Method Called, g_rpr_script_dir = [%s], g_rpr_home_dir = [%s]", g_rpr_script_dir, g_rpr_home_dir);
  sprintf (lock_fname, "%s/bin/.nsu_rpr.lock", g_rpr_home_dir);
  //Exclusive create lock file
  lock_fd = open(lock_fname, O_CREAT|O_EXCL|O_RDWR|O_CLOEXEC, 0666);
  RPR_DL(NULL, "Lock file %s fd=%d\n", lock_fname, lock_fd);
  if (lock_fd < 0) {
    //May be lock file exist but script recorder not running, left over lock
    //Get the pid from the lock file
    fp = fopen(lock_fname, "r");
    if (!fp) {
      rpr_error_log(_FLN_, NULL, "Unable to create nsu_rpr lock file\n");
      exit(1);
    }
    if (fgets(buf, 1024, fp) == NULL) {
      rpr_error_log(_FLN_, NULL, "Bad format of nsu_rpr lock file\n");
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
        rpr_error_log(_FLN_, NULL, "Unable to new create nsu_rpr lock file\n");
        exit(1);
      }
      sprintf(buf, "%d\n", getpid());
      write(lock_fd, buf, strlen(buf));
      close(lock_fd);
    } else {
      //pid exist. capture is running
      rpr_error_log(_FLN_, NULL, "Another instance (%d) of nsu_rpr is running\n", pid);
      exit(1);
    }
  } else {
    //Got the lock , put in pid
    sprintf(buf, "%d\n", getpid());
    RPR_DL(NULL, "buf = [%s]", buf);
    //printf("Adding pid %s\n", buf);
    rpr_write_all(lock_fd, buf, strlen(buf));
    close(lock_fd);
  }
}

void rpr_create_child_script_dir(ConfigurationSettings conf_settings)
{
  char recording_name_dir[MAX_FILE_NAME_LEN];
  char req_dir[MAX_FILE_NAME_LEN];
  char resp_dir[MAX_FILE_NAME_LEN];
  char conf_dir[MAX_FILE_NAME_LEN];
  
  RPR_DL(NULL, "Method called.");
  // this will contain all the request files
  sprintf(recording_name_dir, "%s/%s", g_rpr_script_dir, conf_settings.recording_name);
  RPR_DL(NULL, "Creating [%s] dir.", recording_name_dir);
  if (mkdir(recording_name_dir, 0777)) {
    unlink(lock_fname);
    RPR_EXIT(-1, "Can not create %s direcetory: %s", recording_name_dir, strerror(errno));
  }

  // this will contain all the request files
  sprintf(req_dir, "%s/request", recording_name_dir);
  RPR_DL(NULL, "Creating [%s] dir.", req_dir);
  if (mkdir(req_dir, 0777)) {
    unlink(lock_fname);
    rpr_error_log(_FLN_, NULL, "Can not create Script requests direcetory: %s", strerror(errno));
    exit(-1);
  }

  // this will contain all the response files
  sprintf(resp_dir, "%s/response", recording_name_dir);
  RPR_DL(NULL, "Creating [%s] dir.", resp_dir);
  if (mkdir(resp_dir, 0777))
  {
    unlink(lock_fname);
    rpr_error_log(_FLN_, NULL, "Can not create Script responses direcetory: %s", strerror(errno));
    exit(-1);
  }

  // this will contain all the configuration files
  sprintf(conf_dir, "%s/conf", recording_name_dir);
  RPR_DL(NULL, "Creating [%s] dir.", conf_dir);
  if (mkdir(conf_dir, 0777))
  {
    unlink(lock_fname);
    rpr_error_log(_FLN_, NULL, "Can not create Script configuration direcetory: %s", strerror(errno));
    exit(-1);
  }
}

static void rpr_create_script_dir(char *sname)
{
  int i;
  int len;
  char file_name[1024];
  char script_name[1024];
 
  if(!sname)
    sname = "_newSocketScript";

  len = strlen(sname);
  if (len > 31)
  {
    printf("Error: Script Name should be limited to 31 characters.\n");
    exit (1);
  }

  for (i=0; i<len;i++)
  {
    if (!(isalnum((sname[i])) || (sname[i] == '_')))
    {
      printf("Error: Script Name Can only have alphanumeric characters or _, in its name.\n");
      exit (1);
    }
  }
  /*bug id: 101320: ToDo: TBD with DJA  g_rpr_ta_dir=$NS_WDIR/workspace/<user_name>/<profile_name>/cavisson*/
  sprintf(script_name, "default/default/%s/%s", "scripts", sname);
  sprintf(g_rpr_script_dir, "%s/%s", GET_NS_TA_DIR(), script_name);

  //RPR_DL(NULL, "script_name = %s, g_rpr_script_dir = %s", script_name, g_rpr_script_dir);
  if (!strcmp (script_name, "default/default/scripts/_newSocketScript")) {
    sprintf (file_name, "rm -rf %s", g_rpr_script_dir);
    //RPR_DL(NULL, "Deleting %s", file_name);
    // Syatem Command can cause of SIGCHLD
    (void) signal (SIGCHLD, SIG_IGN);
    system(file_name);
    (void) signal( SIGCHLD, handle_sickchild );
  }

  //RPR_DL(NULL, "Creating %s", g_rpr_script_dir);

  if (mkdir(g_rpr_script_dir, 0775)) {
    printf("Error: Can not create Script with name '%s' : %s\n", script_name, strerror(errno));
    unlink(lock_fname);
    exit (1);
  }
  
  // this will contain all the logs files
  sprintf(g_rpr_logs_dir, "%s/logs", g_rpr_script_dir);
  //RPR_DL(NULL, "Creating %s", g_rpr_logs_dir);
  if (mkdir(g_rpr_logs_dir, 0777)) {
    printf("Error: Can not create Script logs direcetory with name '%s' : %s", g_rpr_logs_dir, strerror(errno));
    unlink(lock_fname);
    exit (1);
  }
  check_uniq_instance();
}

void rpr_get_home_dir()
{
  char *home_dir = NULL;

  //get user working dir
  home_dir = getenv("NS_WDIR");
  if(home_dir == NULL)
    home_dir = "/home/cavisson/work";

  sprintf(g_rpr_home_dir, "%s", home_dir);
  sprintf(rpr_conf_dir, "%s/webapps/netstorm/temp", home_dir);
}

int main(int argc, char *argv[])
{
  int i, child_pid, flag;
  int ports_per_child;
  parent_pid = getpid();
  //char lol_ip_addr[20] = "";

  rpr_get_home_dir();
  rpr_parse_arguments(argc, argv);
  /*bug id: 101320: ToDo: TBD with DJA*/
  /*set ns_ta_dir*/
  nslb_set_ta_dir_ex1(g_rpr_home_dir, GET_DEFAULT_WORKSPACE(), GET_DEFAULT_PROFILE());
  //TODO: complete this function with lock file concept
  rpr_create_script_dir(g_rpr_script_name);

  init_ssl_default(g_rpr_home_dir);
 
  signal(SIGALRM, rpr_recording_timout);
  alarm(g_rpr_timeout); 

  RPR_DL(NULL, "Alarm registered successfully for %ld seconds.", g_rpr_timeout);

  // SIGCHLD must be set after validate_server_addresses() as it used popen()
  (void) signal( SIGCHLD, handle_sickchild );
  (void) signal( SIGPIPE, handle_sigpipe);
  (void) signal( SIGTERM, handle_sigterm_parent);

  RPR_DL(NULL, "Signals registered successfully.");

  MY_MALLOC(child_table, sizeof(child_table_t) * g_rpr_num_childs, "NULL", "for child table");
  ports_per_child = 60*1024/g_rpr_num_childs - 16;
  for (i = 0; i < g_rpr_num_childs; i++)
  {   
    child_table[i].min_port = 1024 + i*ports_per_child;
    child_table[i].max_port = 1024 + (i+1)*ports_per_child - 1;
    child_table[i].cur_port = child_table[i].min_port;
  }

  char env_buf[32];

  sprintf(env_buf, "CHILD_INDEX=0");
  putenv(env_buf);

  RPR_DL(NULL, "Number of rpr childs = %d.", g_rpr_num_childs);
  if(g_rpr_num_childs > 1){
    for (i=0; i < g_rpr_num_childs; i++)
    {
      char *env_buf;
      MY_MALLOC(env_buf, 32, NULL, "for every child");
 
      RPR_DL(NULL, "PID after malloc of env_buf is %d", parent_pid);
      RPR_DL(NULL, "Setting enviorment variables - CHILD_INDEX=%d", i+1);
      sprintf(env_buf, "CHILD_INDEX=%d", i+1);
      putenv(env_buf);
 
      if ((child_pid = fork()) < 0)
      {
        kill_children();
        RPR_EXIT(1, "*** server:  Failed to create child process.  Aborting...");
      }
      if (child_pid > 0)
      {
        child_table[i].pid = child_pid;
        child_table[i].spawn_count = 1;
        child_table[i].last_spawn_time = get_ms_stamp();
 
        RPR_DL(NULL, "### server:  Created child process with pid = %d", child_pid);
      }
      else
      { //This is child part
        break;
      }
    } //end of for loop
    
    if (PARENT) {
        //set alarm to switch partition
      while (PARENT)
      {
        if (ns_sickchild_pending)
        {
          kill_children();
          RPR_EXIT(1, "ns_sickchild_pending = %d", ns_sickchild_pending);
        }
 
        RPR_DL(NULL, "Setting callback for SIGUSR2 in parent");
        (void) signal( SIGUSR2, handle_sigusr2 );
        ns_parent_started = 1;
        (void) signal( SIGINT, SIG_IGN );
        (void) signal( SIGUSR1, handle_sigusr1 );
 
        /* check if we have any childs left to pause for */
        flag = 1;
        for (i = 0; i < g_rpr_num_childs; i++) {
          if (child_table[i].pid != -1) {
            RPR_DL(NULL, "Not all children are killed. Wating.");
            flag = 0;
            break;
          }
        }
 
        if (flag) {
          rpr_error_log(_FLN_, NULL, "### server: No more children left. Exiting.");
          return 0;
        }
 
        pause(); 
      }
    }
    else
    {
      prctl(PR_SET_PDEATHSIG, SIGKILL);
      rpr_child(); // This is child which will return when stopped
    }
  }
  else
    rpr_child(); // This is child which will return when stopped
  
  return 0;
}
