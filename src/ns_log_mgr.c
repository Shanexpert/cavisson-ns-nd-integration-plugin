/******************************************************************
 * Name                 : ns_log_mgr.c
 * Author               : Arun Nishad
 * Purpose              : A process invoked by netstorm parent which will filter_mode & log event to event log file.
 * Initial Version      : Fri Oct  1 13:26:43 IST 2010 
 * Modification History :

*****************************************************************/
//_GNU_SOURCE is defined for O_LARGE_FILE
#define _GNU_SOURCE

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <regex.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/shm.h>
#include<unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dlfcn.h>

#include "nslb_sock.h"
#include "ns_log_mgr.h"
#include "nslb_hash_code.h"
#include "nslb_util.h"
#include "ns_event_filter.h"
#include "ns_msg_def.h"
#include "nslb_partition.h"
#include "nslb_multi_thread_trace_log.h"
#include "nslb_signal.h"
#define NS_EXIT_VAR
#include "ns_exit.h"
#include "nslb_log.h"
#include "nslb_alloc.h"

#define TIMEOUT_FOR_ACCEPT 10
#define NS_STATE_READING        (1 << 0)
//#define EVENT_MSG             15
#define CONNECTION_TYPE_CHILD_OR_CLIENT         0
#define CONNECTION_TYPE_OTHER                   1

#define BIG_BUF_MEMORY_CONVERSION(offset) (big_buf_shr_mem + offset)

#define EL_MAX_EPOLL_FD 1024
#define EXTRA_CHUNK_EL_PROC_ENTRIES 100 

#define   _LF_ __LINE__, (char *)__FUNCTION__

int g_el_msg_com_epfd = -1;
static char* big_buf_shr_mem = NULL;
MTTraceLogKey *nlm_trace_log_key = NULL;
int el_lfd;
//int elog_fd = -1;
static int filter_mode;
static ELMsg_com_con *g_el_msg_com_con = NULL;
static char *src_type[] = {"Core", "Monitor", "Script", "API", "SyncPoint", "NDCollector"};
static char *severity_str[]={"Clear", "Debug", "Information", "Warning", "Minor", "Major", "Critical"};
static struct timeval kill_timeout;
static int test_run_num;
static long long partition_idx;

static int test_run_info_id;
TestRunInfoTable_Shr *testrun_info_table = NULL;

static char *g_ns_tmpdir;
static char *ns_wdir_lol;
static int total_el_proc_entries = 0;
static int max_el_proc_entries = 0;
static char debug_on = 0;
static int cur_conn_cnt = 0;

int trace_level = 1;
int trace_log_size = 10;
int trace_level_change_sig = 0;

#define MAX_FILE		255
#define MAX_FILE_NAME_LEN       512	
#define MAX_BUF_SIZE            2048
/* Total number of files*/
static unsigned char num_files = 0;


typedef struct {
  int data_fd;
  char file_name[MAX_FILE_NAME_LEN + 1];
} FileData;

FileData file_data[MAX_FILE];
//static char file_list[MAX_FILE][MAX_FILE_NAME_LEN];


// duplicate as netstorm has DEBUG LOG
#define MY_MALLOC(new, size, msg, index) {				\
    if (size < 0)							\
      {									\
	fprintf(stderr, "nsa_log_mgr: Trying to malloc a negative size (%d) for index %d\n", (int)size, index); \
	exit(-1);							\
      }									\
    else if (size == 0)							\
      {									\
	new = NULL;							\
      }									\
    else								\
      {									\
	new = (void *)malloc( size );					\
	if ( new == (void*) 0 )						\
	{								\
	  fprintf(stderr, "nsa_log_mgr: Out of Memory: %s for index %d\n", msg, index); \
	  exit(-1);						\
	}								\
      }									\
  }

#define MY_REALLOC(buf, size, msg, index)  \
{ \
    if (size <= 0) {  \
      fprintf(stderr, "nsa_log_mgr: Trying to realloc a negative or 0 size (%d) for index  %d\n", (int)size, index); \
      exit(-1);						\
    } else {  \
      buf = (void*)realloc(buf, size); \
      if ( buf == (void*) 0 )  \
      {  \
        fprintf(stderr, "nsa_log_mgr: Out of Memory: %s for index %d\n", msg, index); \
        exit(-1);						\
      }  \
    } \
  }

static void open_debug_trace_file_nc(int *fd)
{
  char log_file[1024];

  sprintf(log_file, "%s/logs/TR%d/debug_trace.log", ns_wdir_lol, test_run_num);

  if (*fd <= 0 ) //if fd is not open then open it
  {
    *fd = open (log_file, O_CREAT|O_WRONLY|O_APPEND, 00666);
    if (!*fd)
    {
      NS_EXIT(-1, "Error: Error in opening file '%s', Error = '%s'", log_file, nslb_strerror(errno));
    }
  }
}

/*Close all opened files*/
static void close_all_data_files() {
  int file_idx;

  NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, "Method Called, num_files = %d", num_files);

  for(file_idx = 0; file_idx < num_files; file_idx++) {
    if(file_data[file_idx].data_fd > 0)
     close(file_data[file_idx].data_fd);
     file_data[file_idx].data_fd = -1;
  }
}

void change_trace_level () {
  nslb_change_atb_mt_trace_log(nlm_trace_log_key, "NLM_TRACE_LEVEL");
  trace_level = nlm_trace_log_key->log_level;
  NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, "Trace Level changed to %d", trace_level);
  trace_level_change_sig = 0;
}

static  void close_el_msg_com_con(ELMsg_com_con* mccptr, int flag) {

  struct epoll_event pfd;

  NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, "Method called, cur_conn_cnt = %d, closing fd = %d, For Server = %s"                 ,cur_conn_cnt, mccptr->fd, nslb_get_dest_addr(mccptr->fd));

  if(mccptr->fd < 0)
  {
    NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_ERROR, "fd = %d is bad fd", mccptr->fd);
    return;
  }

  if (epoll_ctl(g_el_msg_com_epfd, EPOLL_CTL_DEL, mccptr->fd, &pfd) == -1) {
    NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_ERROR, "remove_select_nsa_log_mgr() - EPOLL_CTL_DEL: err = %s\n", nslb_strerror(errno));
    NS_EXIT(-1, "remove_select_nsa_log_mgr() - EPOLL_CTL_DEL: err = %s\n", nslb_strerror(errno));
  }
  if (close(mccptr->fd) < 0)
  NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_ERROR, "Error in closing fd error = %s", nslb_strerror(errno));
  
  mccptr->fd = -1;
  cur_conn_cnt--;

  if(mccptr->read_buf) {
    free(mccptr->read_buf);
  }
  mccptr->state = 0;

  NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, "cur_conn_cnt = %d, fd %d closed", cur_conn_cnt, mccptr->fd);

}

static void close_all_fd() {

  int idx;
  NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, "Method Called, total_el_proc_entries = %d", total_el_proc_entries);

  for(idx = 0; idx < total_el_proc_entries; idx++) {
    if(g_el_msg_com_con[idx].fd != -1) {
      close_el_msg_com_con(&g_el_msg_com_con[idx], 0); 
    }
  }

  close_all_data_files();
  NS_EXIT(0, "total_el_proc_entries = %d", total_el_proc_entries);
}

static inline void add_select_el_msg_com_con(int epfd, char* data_ptr, int fd, int event) {
  struct epoll_event pfd;

  NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, "Method called");

  bzero(&pfd, sizeof(struct epoll_event)); //Added after valgrind reported bug
      
  pfd.events = event;
  pfd.data.ptr = (void *) data_ptr;
    
  if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &pfd) == -1) {
      NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_ERROR, "Error: EPOLL_CTL_ADD: err = %s", nslb_strerror(errno));
      NS_EXIT(-1, "Error: EPOLL_CTL_ADD: err = %s", nslb_strerror(errno));
  }   
}   

static void init_el_msg_con_struct(ELMsg_com_con *mccptr, int fd, signed char type, char *ip) {

  //char *tmp;
  memset(mccptr, 0, sizeof(ELMsg_com_con));
  mccptr->fd = fd;
  mccptr->type = type;

  NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO,  "Method Called, ip = %s", ip);

  MY_MALLOC(mccptr->ip, strlen(ip) + 1, "mccptr->ip", -1);

  strcpy(mccptr->ip, ip);

  MY_MALLOC(mccptr->read_buf, sizeof(EventMsg), "mccptr->read_buf", -1); //TODO
}

static char *read_msg_ex(ELMsg_com_con *mccptr, int *sz) {
  int bytes_read;  // Bytes read in one read call
  //unsigned int size_tmp;
  int fd = mccptr->fd;

  if (fd == -1) {
    return NULL;  // Issue - this is misleading as it means read is not complete
  }

  // Method called for first time to read message
  if (!(mccptr->state & NS_STATE_READING)) {
    mccptr->read_offset = 0;
    mccptr->read_bytes_remaining = -1;
  }

  // Message length is not yet read
  if (mccptr->read_offset < sizeof(int)) {
    /* Reading Message length */
    while (1) {

      if ((bytes_read = read (fd, mccptr->read_buf + mccptr->read_offset, sizeof(int) - mccptr->read_offset)) < 0) {
        if (errno == EAGAIN) {
          mccptr->state |= NS_STATE_READING; // Set state to reading message
          return NULL;//NS_EAGAIN_RECEIVED;
        } else if (errno == EINTR) {   /* this means we were interrupted */
          continue;
        }
        else {
            NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_ERROR, "Error in reading message length from server = %s, error = %s\n",                             nslb_strerror(errno), nslb_get_dest_addr(fd));
            close_el_msg_com_con(mccptr, 0);
            return NULL; /* This is to handle closed connection from tools */
        }
      }
      if (bytes_read == 0) {
       NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, "Bytes read 0 from server = %s, closing connection ...",nslb_get_dest_addr(fd));
        close_el_msg_com_con(mccptr, 0);
        return NULL;
      }
      mccptr->read_offset += bytes_read;
      if (mccptr->read_offset == sizeof(int)) {
        mccptr->read_bytes_remaining = ((parent_child *)(mccptr->read_buf))->msg_len; 
 
      if(mccptr->read_bytes_remaining > sizeof(EventMsg)) {
       NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_ERROR, "Message size recieved by nsa_log_mgr is bigger than the limit of(%d)", sizeof(EventMsg));
          return NULL;
        }
        break;
      }
    }
  }

  /* Reading rest of bytes remaining in message length */
  while (mccptr->read_bytes_remaining) {

    if((bytes_read = read (fd, mccptr->read_buf + mccptr->read_offset, mccptr->read_bytes_remaining)) < 0) {
      if(errno == EAGAIN) {
        mccptr->state |= NS_STATE_READING; // Set state to reading message
        return NULL;  // NS_EAGAIN_RECEIVED | NS_READING;
      } else if (errno == EINTR) {   /* this means we were interrupted */
          continue;
      } else {
       NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_ERROR, "Error in reading remaining bytes from serevr = %s, error = %s\n",nslb_get_dest_addr(fd), nslb_strerror(errno));
        close_el_msg_com_con(mccptr, 0);
        return NULL; /* This is to handle closed connection from tools */
      }
    }
    if (bytes_read == 0) {
       NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, "Bytes read 0 from server = %s, closing connection ...",nslb_get_dest_addr(fd));
      close_el_msg_com_con(mccptr, 0);
      return NULL;
    }
    mccptr->read_offset += bytes_read;
    mccptr->read_bytes_remaining -= bytes_read;
  }

  mccptr->state &= ~NS_STATE_READING; // Clear state as reading message is complete
  *sz = mccptr->read_offset;
  return (mccptr->read_buf + UNSIGNED_INT); // Return index to opcode
}

//Time Stamp | Event ID | Severity | User Session Data | Source | Host | Attributes Name | Attributes Value | Description
static void chk_n_log_event(EventMsg *em_ptr, char *ip) {

  int hash_code;
  char event_buf[MAX_BUF_LEN_FOR_EL + 1];
  char eid_str[32];
  int len;
  char filter_event = 0;
  
  NSLB_TRACE_LOG3(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, "Method Called, ip = %s, severity = %d", ip, em_ptr->severity);

  event_buf[0] = '\0';
  sprintf(eid_str, "%d", em_ptr->eid);  

  /* #define DO_NOT_LOG_EVENT         2 
   * #define LOG_FILTER_BASED_EVENT   1 
   * #define LOG_ALL_EVENT            0 
  */
  if(filter_mode == 1)
    filter_event = is_event_duplicate(em_ptr->ts, em_ptr->severity, "-", eid_str, em_ptr->attr, &hash_code, 0);
  else // it can not have 2 here 
    filter_event = is_event_duplicate(em_ptr->ts, em_ptr->severity, "-", eid_str, em_ptr->attr, &hash_code, -1);

  if(filter_event == 0) {
      len = sprintf(event_buf, "%02d:%02d:%02d", (em_ptr->ts/1000) / 3600,
						((em_ptr->ts/1000) % 3600) / 60,
						((em_ptr->ts/1000) % 3600) % 60);
      len = sprintf(event_buf, "%s|%d|%s|%s:%d:%d:%d|%s|%s|%s|%*.*s|%*.*s\n",
                                event_buf, em_ptr->eid, severity_str[em_ptr->severity],
				ip, em_ptr->nvm_id, em_ptr->uid, em_ptr->sid, src_type[em_ptr->src],
				em_ptr->host, BIG_BUF_MEMORY_CONVERSION(event_def_shr_mem[hash_code].attr_name),
				em_ptr->attr_len, em_ptr->attr_len, em_ptr->attr, em_ptr->msg_len,
			        em_ptr->msg_len, em_ptr->msg); 
    if(write(elog_fd, event_buf, len) < 0) {
      perror("nsa_log_mgr: writing event log");
      NS_EXIT(-1, "nsa_log_mgr: writing event log");
    }
  }
}

static void process_event_msg(char *buf, char *ip) {

  EventMsg em;
  char *buffer = buf;
  unsigned short msg_len;
  NSLB_TRACE_LOG3(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, "Method called, ip = %s", ip); 
  if(filter_mode == 2) { 
    NSLB_TRACE_LOG3(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_ERROR, "opcode for EVENT_MSG_LOG came while shuld not because all events are disabled, skipping");
    return;
  }
#if 0
    // Total Size
   memcpy(&em.total_size, buffer, UNSIGNED_INT); 
   buffer += UNSIGNED_INT;

   fprintf(stderr, "total_size = %lu\n", em.total_size);
#endif

  // Total OPCODE
  memcpy(&em.opcode, buffer, UNSIGNED_SHORT); 
  buffer += UNSIGNED_SHORT;

  // Time Stamp
  memcpy(&em.ts, buffer, UNSIGNED_INT); 
  buffer += UNSIGNED_INT;
  
  // Total NVM ID
  memcpy(&em.nvm_id, buffer, UNSIGNED_CHAR); 
  buffer += UNSIGNED_CHAR;

  // User ID
  memcpy(&em.uid, buffer, UNSIGNED_INT); 
  buffer += UNSIGNED_INT;
  
  // Session ID
  memcpy(&em.sid, buffer, UNSIGNED_INT); 
  buffer += UNSIGNED_INT;
  
  // Event ID
  memcpy(&em.eid, buffer, UNSIGNED_INT); 
  buffer += UNSIGNED_INT;
  
  // SRC ID
  memcpy(&em.src, buffer, UNSIGNED_CHAR); 
  buffer += UNSIGNED_CHAR;
  
  // Severity 
  memcpy(&em.severity, buffer, UNSIGNED_CHAR); 
  buffer += UNSIGNED_CHAR;
  
  //Host Len
  memcpy(&em.host_len, buffer, UNSIGNED_SHORT);    
  buffer += UNSIGNED_SHORT;

  //Host
  memcpy(em.host, buffer, em.host_len);
  em.host[em.host_len] = '\0';
  buffer += em.host_len;

  // Attribute Len
  memcpy(&em.attr_len, buffer, UNSIGNED_SHORT);    
  buffer += UNSIGNED_SHORT;

  // Attribute
  memcpy(em.attr, buffer, em.attr_len);
  em.attr[em.attr_len] = '\0';
  buffer += em.attr_len;

  // Message Len
  memcpy(&em.msg_len, buffer, UNSIGNED_SHORT);
  buffer += UNSIGNED_SHORT;
  msg_len = em.msg_len;
  if(em.msg_len > MAX_EVENT_DESC_SIZE)
  {
    //Code should not come into this lag
    NSLB_TRACE_LOG3(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO,"Message length = %d is greater then event log disk size = %d", em.msg_len, MAX_EVENT_DESC_SIZE);
    em.msg_len = MAX_EVENT_DESC_SIZE;
  }

  // Message
  memcpy(em.msg, buffer, em.msg_len);
  em.msg[em.msg_len] = '\0'; 
  buffer += msg_len;
  
  /*Chk/filter_mode & log events*/
  chk_n_log_event(&em, ip);  
}

/*Search & get if already opened else open & save*/
static int get_file_fd(char *file_name, int mode) {
  int file_idx;
  int fd = -1; 
  
  NSLB_TRACE_LOG3(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, "Method Called, file_name = %s, mode = [%d]", file_name, mode);

  for(file_idx = 0; file_idx < num_files; file_idx++) {
    if(strcmp(file_name, file_data[file_idx].file_name) == 0) {
      NSLB_TRACE_LOG3(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_ERROR, "File = %s found as already exists", file_name);
      return file_data[file_idx].data_fd;
    }
  }
 
  if(num_files >= MAX_FILE) {
    NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_ERROR, "Error: Too many files used in save data API. Maximum limit is %d. Data is not saved in the file %s", MAX_FILE, file_name);
    return -1;
  }

  if((mode == 0) || (mode == 2))
    fd = open(file_name,  O_CREAT | O_WRONLY | O_CLOEXEC | O_TRUNC | O_LARGEFILE, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
  else
    fd = open(file_name,  O_CREAT | O_WRONLY | O_CLOEXEC | O_APPEND | O_LARGEFILE, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);

  if(fd <= 0) {
  NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_ERROR, "Error: Error in opening file used in save data API. File name is '%s'. Error = %s\n",
           file_name, nslb_strerror(errno));
    return -1; 
  }

  if(mode != 2){
    strcpy(file_data[num_files].file_name, file_name);
    file_data[num_files].data_fd = fd; 
    num_files++;
  }

  NSLB_TRACE_LOG3(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, "File = %s not found, hence opening & adding to list", file_name);
  return fd;
}

/* Write to file*/
static void write_data_to_file(char *file_name, char *msg, int msglen, int mode) {

  int fd = -1;
  NSLB_TRACE_LOG3(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, "Method Called, file_name = %s, msg = %*.*s, msglen = %d",
				  file_name, msglen, msglen, msg, msglen);
  fd = get_file_fd(file_name,  mode); 
  
  if(fd < 0) {
    return;
  }

  if(write(fd, msg, msglen) == -1) {
   fprintf(stderr, "Error: Error in writing data for save data API. File name = %s. Error = %s\n",
		    file_name, nslb_strerror(errno));
  }
}

// To convert long long data from network to host format.
// this function will take Long pointer of data and invertly join them to make long long to convert them network to host format.
 unsigned long long ntohll(unsigned int in[2]) 
 { 
   unsigned long long ll_value; 
   unsigned long long l_value; 
   unsigned long long u_value;

   l_value = (unsigned long long )ntohl(in[0]); 
   u_value = (unsigned long long )ntohl(in[1]);  

   ll_value = ((u_value << 32) + l_value); 
   return(ll_value); 
 } 

static void process_partition_switch_msg(char* msg){
  unsigned short opcode;
  //unsigned long partition_idx;
  char event_log_file[1024];
  
  NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, "Method Called");
  memcpy(&opcode, msg, UNSIGNED_SHORT);
  msg += UNSIGNED_SHORT;
 
  NSLB_TRACE_LOG2(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, "opcode = %hu", opcode);
  
  memcpy(&test_run_num, msg, UNSIGNED_INT);
  msg += UNSIGNED_INT;

  NSLB_TRACE_LOG2(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, "test_run_num = %d", test_run_num);

  memcpy(&partition_idx, msg, UNSIGNED_LONG);
  msg += UNSIGNED_LONG;

  NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, " Switched from previous partition partition_idx = %d", partition_idx);

 
  //closing event.log file  
  close(elog_fd);
  elog_fd = -1;
  sprintf(event_log_file, "%s/logs/TR%d/%llu/event.log", ns_wdir_lol, test_run_num, partition_idx);  //TODO TEST IN NON NDE MODE 

  open_event_log_file(event_log_file, O_CREAT | O_WRONLY | O_CLOEXEC | O_APPEND | O_LARGEFILE, 1);  
}

static void process_debug_trace_msg(char *buf_ptr, int len) {
  static int trace_log_fd_nc = -1;

  NSLB_TRACE_LOG3(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, "Method Called");
  len -= (UNSIGNED_SHORT + UNSIGNED_INT); // Minus the bytes of tot_dt_buf_size and opcode
  buf_ptr += UNSIGNED_SHORT; // Increase as we do not need of opcode
  buf_ptr[len] = '\0';
  NSLB_TRACE_LOG3(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, "Method Called msg = %s, length = %d", buf_ptr, len);
  open_debug_trace_file_nc(&trace_log_fd_nc);
  write(trace_log_fd_nc, buf_ptr, len);
}

static void  process_save_data_api_msg(char *msg) {
  unsigned short opcode;
  unsigned short file_name_len = 0;
  unsigned int buf_len = 0;
  unsigned int mode = 0; 
  char file_name[MAX_FILE_NAME_LEN + MAX_FILE_NAME_LEN];
  char file_name_new[MAX_FILE_NAME_LEN + MAX_FILE_NAME_LEN];

  NSLB_TRACE_LOG3(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, "Method Called");
  // Total OPCODE
  memcpy(&opcode, msg, UNSIGNED_SHORT); 
  msg += UNSIGNED_SHORT;

  // file name Len
  memcpy(&file_name_len, msg, UNSIGNED_SHORT);    
  msg += UNSIGNED_SHORT;
  NSLB_TRACE_LOG2(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, "file_name_len = %d", file_name_len);

  // file name
  memcpy(&file_name, msg, file_name_len);    
  msg += file_name_len;
  file_name[file_name_len] = '\0';

  // File open mode 
  memcpy(&mode, msg, UNSIGNED_INT);    
  msg += UNSIGNED_INT;
  NSLB_TRACE_LOG2(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, "mode = %d", mode);

  // buf name Len
  memcpy(&buf_len, msg, UNSIGNED_INT);    
  msg += UNSIGNED_INT;

  if(file_name[0] == '/') {
     strcpy(file_name_new, file_name);
     mkdir_ex(file_name_new);
  } else {
     sprintf(file_name_new, "%s/logs/TR%d/%s", ns_wdir_lol, test_run_num, file_name);
     file_name_len = strlen(file_name_new);
  }

  if(file_name_len <= MAX_FILE_NAME_LEN) {
    write_data_to_file(file_name_new, msg, buf_len, mode);
  } else {
    fprintf(stderr, "Error: File name length (%d) used in save data API is more"
		    " than maximum file name length (%d) supported. File name is '%s'."
		    " Data is not saved.\n", file_name_len, MAX_FILE_NAME_LEN, file_name_new);
    NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_ERROR, "Error: File name length (%d) used in save data API is more"
		       " than maximum file name length (%d) supported. File name is '%s'."
		       " Data is not saved.\n", file_name_len, MAX_FILE_NAME_LEN, file_name_new);
    return;
  }
  NSLB_TRACE_LOG3(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, "File name = %s\n", file_name_new);
}

/* Returns:
 * -1 on failure 
 *  0 on success
 *  1 on  Reallocation performed*/
static int create_el_proc_table_entry(int *row_num, int delta) {
  
  int ret = 0;

  NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, "Method Called, row_num = %d, delta = %d", row_num, delta);

  if (total_el_proc_entries == max_el_proc_entries) {
    MY_REALLOC (g_el_msg_com_con, ((max_el_proc_entries + delta) * sizeof(ELMsg_com_con)), "g_el_msg_com_con", -1);
    NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, "Reallocating, total_el_proc_entries = %d, max_el_proc_entries = %d",
				   total_el_proc_entries, max_el_proc_entries);
    ret = 1; // Reallocation DONE
    if (!g_el_msg_com_con) {
      NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, "Error allocating more memory for el proc entries");
      return(-1);
    } else { 
      max_el_proc_entries += delta;
    }
  }
  *row_num = total_el_proc_entries++;
  return (ret);
}

static void  rearrange_el_msg_com_con() {

  int i;
  struct epoll_event pfd;

  NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, "Method Called, total_el_proc_entries = %d", total_el_proc_entries);

  for(i = 0; i < total_el_proc_entries; i++) {
     if(g_el_msg_com_con[i].fd < 0)
       continue;
     else if (epoll_ctl(g_el_msg_com_epfd, EPOLL_CTL_DEL, g_el_msg_com_con[i].fd, &pfd) == -1) {
       NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_ERROR, "nsa_log_mgr - EPOLL_CTL_DEL: err = %s\n",
                       nslb_strerror(errno));
       fprintf(stderr, "nsa_log_mgr - EPOLL_CTL_DEL: fd = %d, ip = %s, err = %s\n", g_el_msg_com_con[i].fd,
                        g_el_msg_com_con[i].ip?g_el_msg_com_con[i].ip:NULL, nslb_strerror(errno));
     }
     add_select_el_msg_com_con(g_el_msg_com_epfd, (char *)&g_el_msg_com_con[i],
			  g_el_msg_com_con[i].fd, EPOLLIN | EPOLLERR | EPOLLHUP);
  }
}

static void accept_and_timeout_for_el(int *row_num, int delta) {
  socklen_t len = sizeof(struct sockaddr_in);
  struct sockaddr_in their_addr;
  int fd;
  int ret;
  int num_event = 0;
  static int timeout = 0;
  static int epoll_create_done = 0;
  static int nsa_log_mgr_epfd = 0;
  struct epoll_event nsa_log_mgr_event;
  static struct epoll_event *events = NULL;

  NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, "Method Called, delta = %d", delta);

  if((ret = create_el_proc_table_entry(row_num, delta)) == -1) {
    NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, "Unable to create table entry for g_el_msg_com_con");
    /* Unable to create table entry for g_el_msg_com_con*/
    NS_EXIT(-1, "Unable to create table entry for g_el_msg_com_con");
  }

  //Bug 35717 - NS | "nsa_log_mgr: select(): Bad file descriptor" is error is coming while starting the test. 
  NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, "epoll_create_done = %d", epoll_create_done);
  if(!epoll_create_done)
  {
    if((nsa_log_mgr_epfd = epoll_create(1)) == -1)
    {
      NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO,
                      "Error: accept_and_timeout_for_el(): epoll_create() failed, err = %s", nslb_strerror(errno));
      NS_EXIT(-1,  "Error: accept_and_timeout_for_el(): epoll_create() failed, err = %s", nslb_strerror(errno));
    }

    timeout = TIMEOUT_FOR_ACCEPT * 1000;

    MY_MALLOC(events, sizeof(struct epoll_event), "epoll event", -1);
    if(events == NULL)
    {
      NS_EXIT(-1, "Error: accept_and_timeout_for_el(): Unable to malloc events = [%p], hence exiting", events);
    }

    nsa_log_mgr_event.events = EPOLLIN;
    nsa_log_mgr_event.data.fd = el_lfd;

    if (epoll_ctl(nsa_log_mgr_epfd, EPOLL_CTL_ADD, el_lfd, &nsa_log_mgr_event) == -1)
    {
      NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, "Could not add parent fd in EPOLL. err = %s",
                       nslb_strerror(errno));
      NS_EXIT(-1, "Error: accept_and_timeout_for_el(): epoll_ctl failed, err = %s", nslb_strerror(errno));
    }

    epoll_create_done = 1;
    NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, "Epoll creation is done, epoll_create_done = %d",
                    epoll_create_done);
  }

  memset(events, 0, sizeof(struct epoll_event));

  /* Wait for new connection and Accept the connections */
  num_event = epoll_wait(nsa_log_mgr_epfd, events, 1, timeout);

  if(num_event < 0)
  {
    NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO,
                    "Error: accept_and_timeout_for_el() - epoll_wait failed, err = %s", nslb_strerror(errno));
    NS_EXIT(-1, "Error: accept_and_timeout_for_el() - epoll_wait failed, err = %s", nslb_strerror(errno));
  }
  else if(num_event > 0)
  {
    fd = accept(el_lfd, (struct sockaddr *)&their_addr, (socklen_t *)&len);
    if (fd < 0)
    {
      NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO,
                      "Error: accept_and_timeout_for_el() - accept failed, err = %s", nslb_strerror(errno));
      NS_EXIT(-1, "Error: accept_and_timeout_for_el() - accept failed, err = %s", nslb_strerror(errno));
    }
  }
  else { /* no connection found */
    NS_EXIT(-1, "nsa_log_mgr: Timeout(%d Sec) while waiting for connections from client."
                    "Number of connections accepted so far = %d" ,
                     TIMEOUT_FOR_ACCEPT, cur_conn_cnt);
  }

  fcntl(fd, F_SETFL, O_NONBLOCK);

  init_el_msg_con_struct(&g_el_msg_com_con[*row_num], fd, CONNECTION_TYPE_CHILD_OR_CLIENT,
                          nslb_sockaddr_to_ip((const struct sockaddr *)&their_addr, 0));

  add_select_el_msg_com_con(g_el_msg_com_epfd, (char *)&g_el_msg_com_con[*row_num],
                          fd, EPOLLIN | EPOLLERR | EPOLLHUP);
  cur_conn_cnt++;

  NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO,
                  "Got connection from  %s, current number of connection = %d",
                   nslb_sock_ntop((const struct sockaddr *)&their_addr), cur_conn_cnt);

  if(ret == 1) { // It means rellalloation done hence we will not have an correct pointer in pfd
    rearrange_el_msg_com_con();
  }
}

static void add_select(void* ptr, int fd, int event) {
  struct epoll_event pfd;

  bzero(&pfd, sizeof(struct epoll_event));

  pfd.events = event;
  pfd.data.ptr = ptr;
  if (epoll_ctl(g_el_msg_com_epfd, EPOLL_CTL_ADD, fd, &pfd) == -1) {
    NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_ERROR, "add_select for nsa_log_mgr epoll add: err = %s", nslb_strerror(errno));
    NS_EXIT(-1, "add_select for nsa_log_mgr epoll add: err = %s", nslb_strerror(errno));
  }
}

static void receive_forever(int num_processes) {

  int cnt, i, rcv_amt;
  struct epoll_event *epev = NULL;
  unsigned int epoll_timeout;
  //char epoll_timeout_cnt = 0;
  char *msg = NULL;
  unsigned short opcode;
  int row_num = 0;
  //int is_listen_fd = 0;
  int delta_el_proc = num_processes + EXTRA_CHUNK_EL_PROC_ENTRIES; 
  int netstorm_pid = getppid();
 
  NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, "Method Called, num_processes = %d", num_processes);
 
  /* The size is not the maximum size of the backing store but just a hint
   * to the kernel about how to dimension internal structures.*/
  if ((g_el_msg_com_epfd = epoll_create(num_processes)) == -1) {
    NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_ERROR, "Unable to epoll_create . exiting.");
    NS_EXIT(-1, "Unable to epoll_create . exiting.");
  }

  /*Allocate space for epoll_event structure*/
  MY_MALLOC(epev, sizeof(struct epoll_event) * EL_MAX_EPOLL_FD, "epoll event", -1);

  /*We pass NULL so that we can identify that its a event for listen fd*/
  add_select(NULL, el_lfd, EPOLLIN);

  if (epev == NULL) {
    NS_EXIT(-1, "%s:%d Malloc failed. Exiting.", __FUNCTION__, __LINE__);
  }

  kill_timeout.tv_sec  = -1;  // infinite
  kill_timeout.tv_usec = -1;
 
  epoll_timeout = 1000 * 60;   // One minute timeout

  while(1) {

    //is_listen_fd = 0;
    memset(epev, 0, sizeof(struct epoll_event) * EL_MAX_EPOLL_FD);

    if(trace_level_change_sig) {
      change_trace_level();
    }

    cnt = epoll_wait(g_el_msg_com_epfd, epev, EL_MAX_EPOLL_FD, epoll_timeout);
    if (cnt > 0) {
      /* Reset epoll timeout count to 0 as we has to track only continuesly timeout*/
     // epoll_timeout_cnt = 0;
      for (i = 0; i < cnt; i++) {
        ELMsg_com_con *mccptr = NULL;

        int *fd_mon = NULL;

        fd_mon = epev[i].data.ptr;

        /* For listen fd fd_mon will be NULL as we have fill NULL through add_select*/
        if(fd_mon == NULL) {
          /*This is a listen fd*/
          accept_and_timeout_for_el(&row_num, delta_el_proc);
	  continue;
        }

        mccptr = (ELMsg_com_con *)epev[i].data.ptr;

        if (epev[i].events & EPOLLERR) {
          close_el_msg_com_con(mccptr, 0 );
          continue;
        }

        if (epev[i].events & EPOLLHUP) {
          close_el_msg_com_con(mccptr, 0);
          continue;
        }

        msg = NULL;
        if (epev[i].events & EPOLLIN)
            msg = read_msg_ex(mccptr, &rcv_amt);
        /* data we are reading is not complete, wait for anoter poll */

        if (msg == NULL) {
           if(cur_conn_cnt != 0) {
             continue;
           } else {
             NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_ERROR, "No connection left hence exiting ...");
             close_all_data_files();
             NS_EXIT(1, "No connection left hence exiting ...");
           }
        }
       
        memcpy(&opcode, msg, UNSIGNED_SHORT);

        NSLB_TRACE_LOG3(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, "Opcode Recieved = %d", opcode);

        switch (opcode) {
          case EVENT_MSG_LOG:
            process_event_msg(msg, mccptr->ip);
            break;
          case SAVE_DATA_MSG:
            process_save_data_api_msg(msg);
            break;
          case NEW_TEST_RUN: // Parition switch
            process_partition_switch_msg(msg);
            break;
          case DEBUG_TRACE_MSG:
            process_debug_trace_msg(msg, rcv_amt);
            break;
          default:
            break;
        } // End of switch
      }  // End of for()
    } else if (cnt == 0) {  // Time Out
      if(is_process_alive(netstorm_pid, NULL)) {
         continue;  // Continue as we got timeout and NetStorm process is still running 
      } else {
         NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, "NetStorm process got killed. Exiting ...");
         close_all_fd();
      }
    } else if (errno == EBADF) {
      NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_ERROR, "Error: %s, Exiting ...", nslb_strerror(errno));
      close_all_fd();
    } else if (errno == EFAULT) {
      NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_ERROR, "Error: %s, Exiting ...", nslb_strerror(errno));
      close_all_fd();
      NS_EXIT(-1, "Error: %s, Exiting ...", nslb_strerror(errno));
    } else if (errno == EINVAL) {
      NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_ERROR, "Error: %s, Exiting ...", nslb_strerror(errno));
      close_all_fd();
    } else {
      if (errno != EINTR)  {
        NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_ERROR, "Error: %s, Exiting ...", nslb_strerror(errno));
        close_all_fd();
      }  else;
    }
  }

  NSLB_FREE_AND_MAKE_NOT_NULL(epev, "epev", -1, NULL);
  close_all_data_files();
  return;
}

static void handle_el_sigpipe(int sig) {
  NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, "Method Called, sig = %d", sig);
}

static void handle_el_sigint(int sig) {
  NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, "Method Called, sig = %d", sig); 
}

static void handle_el_sigterm(int sig) {
  NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, "Method Called, sig = %d", sig);

  close_all_data_files();

  NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, "Exiting ...");


  NS_EXIT(0, "Exiting ...");
}

static void handle_el_sigusr2(int sig) {
  NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, "Method Called, sig = %d", sig);
}

static void handle_trace_level_change_sig(int sig)
{
  trace_level_change_sig = 1;
}

static void get_hash_code_func(Str_to_hash_code_ex *str_to_hash_code_fun, char *hash_fun_name , char *so_file_name) {

  void *handle;
  char *error;

  NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, "Method Called, hash_fun_name = %s, so_file_name = %s", hash_fun_name, so_file_name);

  handle = dlopen (so_file_name, RTLD_LAZY);

  if ((error = dlerror())) {
    /* If so, print the error message and exit. */
    NS_EXIT(-1, "%s", error);
  }

  //var_hash_func = dlsym(handle, "hash_variables");
  *str_to_hash_code_fun = dlsym(handle, hash_fun_name);

  if ((error = dlerror())) {
    /* If so, print the error message and exit. */
    NS_EXIT(-1, "%s", error);
  }
} 

/**/
int main(int argc, char *argv[]) {

  if(argc != 12) {
   fprintf(stderr, "%s: Invalid number of arugments to nsa_event_logger.\n", (char*)__FUNCTION__);
   return -1; 
  }

  int j, port, num_process, el_shmid, big_buf_shmid;
  char so_file_name[1024], event_log_file[1024];
  //int lfd;

  //int test_run_info_shm_id;
  char base_dir[1024];
  char partition_name[1024] = "";
  int ret;
 
  /* Port on which this process will listen*/
  port		     = atoi(argv[1]);
  /* total number of process on including parent */
  num_process        = atoi(argv[2]);
  /* listen fd of this process*/
  el_lfd	     = atoi(argv[3]);
  /* filter_mode flag to decide filter events or not*/
  filter_mode	     = atoi(argv[4]);
  /* Shared memory id for event structure */
  el_shmid	     = strtoul(argv[5], NULL, 10);
  /* Total number of hashes of events*/ 
  num_total_event_id = atoi(argv[6]);
  /* Shared memory id for big buf*/
  big_buf_shmid      = strtoul(argv[7], NULL, 10);
  /* *.so dir path so that we can get *.so files*/
  g_ns_tmpdir        = argv[8];
  /* Test run number for which this process is running*/
  test_run_num       = atoi(argv[9]);
  
  debug_on           = atoi(argv[10]);
  test_run_info_id = atoi(argv[11]); 
  /*Get present NetStorm working directory*/
  ns_wdir_lol = getenv("NS_WDIR");

  if(ns_wdir_lol == NULL) {
     ns_wdir_lol = "/home/cavisson/work";
     fprintf(stderr, "nsa_event_logger unable to get NS_WDIR setting %s\n", ns_wdir_lol); 
  }
  sprintf(base_dir, "%s/logs/TR%d", ns_wdir_lol, test_run_num);


  testrun_info_table = (TestRunInfoTable_Shr*)shmat(test_run_info_id, NULL, 0);
  if (testrun_info_table == NULL) 
  {
    perror("event logger: error in attaching shared memory");
    NS_EXIT(-1, "testrun_info_table is NULL");
  }
  partition_idx = testrun_info_table->partition_idx;
  strcpy(partition_name, testrun_info_table->partition_name);

  trace_level = testrun_info_table->nlm_trace_level;

  nlm_trace_log_key = nslb_init_mt_trace_log(ns_wdir_lol, test_run_num, partition_idx, "ns_logs/nlm_trace.log", trace_level, trace_log_size);

  ret = nslb_save_pid_ex(base_dir, "nlm", "nsa_log_mgr");

  if(ret == 1)
  {
     NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, "Prev Logging Writer killed, pid saved");
  }
  else if(ret == 0)
  {
     NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, "Prev Logging Writer was not running, pid saved");
  }
  else
  {
     NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_ERROR, "Error in saving Logging Writer pid");
  }

  if(filter_mode < 0 || filter_mode > 2) {
     NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_ERROR, "filter_mode = %d, which must be [0-2].", filter_mode);
     NS_EXIT(-1, "filter_mode = %d, which must be [0-2].", filter_mode);
  }

  NSLB_TRACE_LOG1(nlm_trace_log_key, partition_idx, "Main thread", NSLB_TL_INFO, "Starting NetStorm log manager (nsa_log_mgr): Port = %d, Num proc to handle = %d, Listen fd = %d, Filterm mode = %d, Shm id = %d, Event Hashes = %d, Big Buf Shm id = %d, Debug = %d, TestRunInfoShmKey = %d, trace_level = %d", port, num_process, el_lfd, filter_mode, el_shmid, num_total_event_id, big_buf_shmid, debug_on, test_run_info_id, trace_level);

  if(partition_idx <= 0)
    sprintf(event_log_file, "%s/logs/TR%d/event.log", ns_wdir_lol, test_run_num);
  else
    sprintf(event_log_file, "%s/logs/TR%d/%lld/event.log", ns_wdir_lol, test_run_num, partition_idx);
    
  sprintf(so_file_name, "%s/%s/event_id_hash_fun_write.so", ns_wdir_lol, g_ns_tmpdir);
  open_event_log_file(event_log_file, O_CREAT | O_CLOEXEC | O_WRONLY | O_APPEND | O_LARGEFILE, 0);

  void *ptr;
  /* We should not attach shared memory as we have to filter all events*/
  if(filter_mode != 2)  {
    /*Attach big_buf_shr_mem of Netstorm*/
    ptr = shmat(big_buf_shmid, NULL, 0);
    if ((ns_ptr_t)ptr == -1) {
      perror("ns_event_logger: error in attaching shared memory");
      NS_EXIT(-1, "(ns_ptr_t)ptr = -1");
    }
    big_buf_shr_mem = (char*) ptr;

    /*Attach event definition shrm mem*/
    ptr = shmat(el_shmid, NULL, 0);
    if ((ns_ptr_t)ptr == -1) {
      perror("ns_event_logger: error in attaching shared memory");
      NS_EXIT(-1, "(ns_ptr_t)ptr = -1");
    }
    event_def_shr_mem = (EventDefinitionShr*) ptr;

    /*Malloc for 1 as all event will be recieved by only this process*/
    for(j = 0; j <  num_total_event_id; j++) {
       MY_MALLOC(event_def_shr_mem[j].event_head, sizeof(Events*) * 1,
                                                "Event Definition table", num_total_event_id);
       memset(event_def_shr_mem[j].event_head, 0, sizeof(Events*) * 1);
    } 

    /*We also need to open so file(created by Netstorm parent) to create hash func*/
    get_hash_code_func(&var_event_hash_func, "event_id_hash_fun" , so_file_name);
  }

  (void) signal( SIGPIPE, handle_el_sigpipe );
  (void) signal( SIGTERM, handle_el_sigterm );
  (void) signal( SIGINT, handle_el_sigint );
  (void) signal( SIGUSR2, handle_el_sigusr2);
  (void) signal( TRACE_LEVEL_CHANGE_SIG, handle_trace_level_change_sig);

  receive_forever(num_process);

  return 0;
}
