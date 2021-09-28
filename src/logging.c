//_GNU_SOURCE is defined for O_LARGE_FILE
#define _GNU_SOURCE

#include <regex.h>
#include <semaphore.h>
#include <errno.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"
#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "user_tables.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "util.h"
#include "timing.h"
#include "tmr.h"

#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases_parse.h"
#include "ns_schedule_phases.h"

#include "nslb_sock.h"
#include "netstorm.h"
#include "ns_trans.h"
#include "ns_log.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "ns_alloc.h"
#include "ns_url_resp.h"
#include "ns_event_log.h"
#include "ns_event_id.h"
#include "ns_string.h"
#include "ns_page_dump.h"
#include "nslb_util.h"
#include "ns_monitoring.h"
#include "wait_forever.h"
#include "nslb_time_stamp.h"
#include <sys/wait.h>
#include "nslb_comp_recovery.h"
#include "netomni/src/core/ni_user_distribution.h"
#include "netomni/src/core/ni_scenario_distribution.h"
#include "ns_trace_level.h"
#include "nslb_encode_decode.h"
#include "nslb_multi_thread_trace_log.h"
#include "ns_exit.h"
#include "ns_error_msg.h"
#include "ns_test_monitor.h"
#include "ns_file_upload.h"
#include "ns_http_status_codes.h"

int nia_req_rep_uploader_pid = -2;
extern Msg_com_con ndc_mccptr;
/*This is done for optimization so that it can not do arithmatic operation at run time*/
static const int session_record_size	=  SESSION_RECORD_SIZE;
const int tx_pg_record_size      =  TX_PG_RECORD_SIZE;
const int tx_record_size		=  TX_RECORD_SIZE;
const int page_record_size	=  PAGE_RECORD_SIZE;
const int url_record_size	=  URL_RECORD_SIZE;
static const int data_record_hdr_size	=  DATA_RECORD_HDR_SIZE;
static const int msg_record_hdr_size	=  MSG_RECORD_HDR_SIZE;
static const int data_record_min_size	=  DATA_RECORD_MIN_SIZE;
static const int msg_record_min_size	=  MSG_RECORD_MIN_SIZE;
static const int page_dump_record_size  =  PAGE_DUMP_RECORD_SIZE;
static const int page_rbu_record_size   =  RBU_RECORD_SIZE;
static const int rbu_lh_record_size     =  RBU_LH_RECORD_SIZE;
//static const int host_record_size       =  HOST_RECORD_HDR_SIZE;

const int zero_int = 0;
const unsigned char zero_unsigned_char = 0;

static char vptr_to_str[MAX_LINE_LENGTH + 1];

shr_logging* logging_shr_mem;

#define MAX_RECOVERY_COUNT                5
#define RESTART_TIME_THRESHOLD_IN_SEC   900

ComponentRecoveryData ReaderRecoveryData;
ComponentRecoveryData ReqRepUploaderRecoveryData;

int writer_pid = -1;
int reader_pid  = -1;

int static_logging_fd = -1;
FILE *static_logging_fp = NULL;


int runtime_logging_fd = -1;
int session_status_fd = -1;
static unsigned int block_num = 1;  // Running counter (starting wit 1) to just give seq number to every block to be written

//extern g_ns_wdir;
int logging_shr_blk_count = 16;

// Linked list of local buffs which are freed and can be used again
local_buffer* free_local_bufs = NULL;

local_buffer* local_buf = NULL; // Head of the linked list
local_buffer* last_local_buf = NULL; // Tail of the linked list

int local_written_size = 0;

int total_free_local_bufs = 0;
int total_local_bufs = 0;

int log_shr_buffer_size = 8192;// Block size

long long total_url_record_size = 0;
long long total_page_dump_record_size = 0;
long long total_session_record_size = 0;
long long total_tx_pg_record_size = 0;
long long total_tx_record_size = 0;
long long total_page_record_size = 0;	
static long long total_waste_size = 0;	 // Part of block not used, so it is wasted size written in block and dlog
static long long total_page_rbu_record_size = 0;
static long long total_rbu_lh_record_size = 0;
static long long total_rbu_mark_mmeasure_record_size = 0;

unsigned int  total_url_records = 0;
unsigned int  total_page_dump_records = 0;
unsigned int  total_tx_pg_records = 0;
unsigned int  total_tx_records = 0;
unsigned int  total_session_records = 0;
unsigned int  total_page_records = 0;
static unsigned int  total_page_rbu_records = 0;
static unsigned int  total_rbu_lh_records = 0;
static unsigned int  total_rbu_mark_measure_records = 0; 

//Keeping shared memory key as global variable. In case of recovery we don't want to recreate the shared
//memory as those are already shared with NVM's. So this global key will be used while recovering

char shr_mem_key[64];
char shr_mem_new_tr_key[64];

//static void write_data_in_debug_logging ();

#define SEND_SIGNAL_TO_WRITER \
    if ((writer_pid > 0) && logging_shr_mem->write_flag) { \
      if (kill(writer_pid, SIGUSR1) == -1) { \
        if (errno != ESRCH) { \
          perror("Error in sending the logging_writer a signal"); \
	  NS_EXIT(-1, ""); \
        } \
      } \
    } 

#define SEND_SIGNAL_TO_WRITER_ALWAYS \
    if (writer_pid > 0) { \
      if (kill(writer_pid, SIGUSR1) == -1) { \
        if (errno != ESRCH) { \
          perror("Error in sending the logging_writer a signal"); \
	  NS_EXIT (-1, ""); \
        } \
      } \
    } 

// decrement the semaphore (Lock) 
#define NS_LOCK_LOGGING_SHR_BUFFER(shr_buffer) \
{ \
  NSDL1_LOGGING(NULL, NULL, "Start - Locking shared memory block %d using sem_wait", logging_shr_mem->current_buffer_idx); \
  /*time_t tm = time(NULL); \
  struct timespec abs_timeout;\
  abs_timeout.tv_sec = tm;\
  abs_timeout.tv_nsec = 1;\
  int sem_wait_ret = sem_timedwait(&(shr_buffer->sem), &abs_timeout); \
  if(sem_wait_ret < 0){ \
    NSDL1_LOGGING(NULL, NULL, "End - Fail locking shared memory block %d using sem_wait", logging_shr_mem->current_buffer_idx); \
    NS_EL_2_ATTR(EID_LOGGING_BUFFER,  -1, -1, EVENT_CORE, EVENT_WARNING, \
                               __FILE__, (char*)__FUNCTION__, \
                               "Error = %s, All shared memory blocks used for logging are full (sem_timedwait). Going to use local memory now. Total %d local buffers allocated. Currently %d local buffers are free. Block number is %u. disk_flag = %d memory_flag = %d", nslb_strerror(errno), total_local_bufs, total_free_local_bufs, shr_buffer->block_num, shr_buffer->disk_flag, shr_buffer->memory_flag); \
    return -1;\
  }\
  */\
  if (shr_buffer->memory_flag) { \
    /* Commenting code as we are incrementing bytes_written and updating with the size
     * logging_shr_mem->bytes_written = 0;*/ \
  } \
  /* Memory flag of the next block to be used is 0 which means it is still not written to disk */ \
  else { \
    NS_EL_2_ATTR(EID_LOGGING_BUFFER,  -1, -1, EVENT_CORE, EVENT_DEBUG, \
                               __FILE__, (char*)__FUNCTION__, \
                               "All shared memory blocks used for logging are full. Going to use local memory now. Total %d local buffers allocated. Currently %d local buffers are free. Block number is %u. disk_flag = %d memory_flag = %d", total_local_bufs, total_free_local_bufs, shr_buffer->block_num, shr_buffer->disk_flag, shr_buffer->memory_flag); \
    NS_UNLOCK_LOGGING_SHR_BUFFER(shr_buffer);\
    SEND_SIGNAL_TO_WRITER \
    return -1;\
  }\
  NSDL1_LOGGING(NULL, NULL, "End - Locking shared memory block %d using sem_wait", logging_shr_mem->current_buffer_idx); \
}





#define NS_UNLOCK_LOGGING_SHR_BUFFER(shr_buffer) \
{ \
  NSDL1_LOGGING(NULL, NULL, "Start - UnLocking shared memory block %d using sem_wait", logging_shr_mem->current_buffer_idx); \
  /*int sem_post_ret = sem_post(&(shr_buffer->sem)); \
  if(sem_post_ret < 0){ \
    NSDL1_LOGGING(NULL, NULL, "End - Fail unlocking shared memory block %d using sem_wait", logging_shr_mem->current_buffer_idx); \
    NS_EL_2_ATTR(EID_LOGGING_BUFFER,  -1, -1, EVENT_CORE, EVENT_WARNING, \
                               __FILE__, (char*)__FUNCTION__, \
                               "Error = %s, Sempost failed", nslb_strerror(errno));\
  }\
  */\
  NSDL1_LOGGING(NULL, NULL, "End - UnLocking shared memory block %d using sem_wait", logging_shr_mem->current_buffer_idx); \
}

#if 0  /*dump to dlog*/
FILE *mydlog;
void dump_mydlog(char *data, int len)
{
  fwrite(data, len, 1, mydlog);
  fflush(mydlog);
}
#endif

int init_nvm_logging_shr_mem()
{
  NSDL2_LOGGING(NULL, NULL, "Method called.");

  logging_shr_mem = v_port_entry->logging_mem;
  write_buffer* shr_buffer = &logging_shr_mem->buffer[logging_shr_mem->current_buffer_idx];
#if 0 /*Dump to dlog*/
   char file[512];
   sprintf(file, "/tmp/mydlog/dlog.%d", my_port_index);
   if((mydlog = fopen(file, "w")) == NULL)
   {
     perror("fopen");
     exit(-1);
   }
#endif

  NS_LOCK_LOGGING_SHR_BUFFER(shr_buffer);
  
  return 0;
}


static inline int inc_shr_buf() {

  write_buffer* shr_buffer = &logging_shr_mem->buffer[logging_shr_mem->current_buffer_idx];


  // This is a sefty check to see if any Logging Writer did not touch this block as it is in memory mode
  if (shr_buffer->memory_flag == 0) {
      NS_EL_2_ATTR(EID_LOGGING_BUFFER,  -1, -1, EVENT_CORE, EVENT_DEBUG,
                                __FILE__, (char*)__FUNCTION__,
                               "Shared memory block (idx = %d) used for logging is not in memory mode. Going to use local memory now", logging_shr_mem->current_buffer_idx);
    NS_UNLOCK_LOGGING_SHR_BUFFER(shr_buffer);
    return -1;
  }

  total_waste_size += (log_shr_buffer_size - logging_shr_mem->bytes_written);

  shr_buffer->block_num = block_num; // assign block number

  NSDL2_LOGGING(NULL, NULL, "Method called. block_num = %d, current_buffer_idx = %d, memory_flag = %d, disk_flag = %d, write_flag = %d, bytes_written = %d, waste_size = %d", shr_buffer->block_num, logging_shr_mem->current_buffer_idx, shr_buffer->memory_flag, shr_buffer->disk_flag, logging_shr_mem->write_flag, logging_shr_mem->bytes_written, (log_shr_buffer_size - logging_shr_mem->bytes_written));

  shr_buffer->memory_flag = 0;
  shr_buffer->disk_flag = 1;
  logging_shr_mem->prev_disk_timestamp = get_ms_stamp();
  //dump_mydlog(shr_buffer->data_buffer, log_shr_buffer_size); /*dump to dlog*/
  NS_UNLOCK_LOGGING_SHR_BUFFER(shr_buffer);

  SEND_SIGNAL_TO_WRITER;

  block_num++;  // Increament for next block

  logging_shr_mem->current_buffer_idx++;
  logging_shr_mem->current_buffer_idx %= logging_shr_blk_count;

  shr_buffer = &logging_shr_mem->buffer[logging_shr_mem->current_buffer_idx];


  NS_LOCK_LOGGING_SHR_BUFFER(shr_buffer);

  // If code come here, then we locked the buffer. If any error, it will return from above macro.
  return 0;

}

// Copy local buffs to shm if shm blocks are available
static inline int copy_local_buffers() {
  write_buffer* shr_buffer;
  local_buffer* freed_buf;

  NSDL2_LOGGING(NULL, NULL, "Method called. local_buf = %p, last_local_buf = %p, free_local_bufs = %p, total_free_local_bufs = %d, total_local_bufs = %d", local_buf, last_local_buf, free_local_bufs, total_free_local_bufs, total_local_bufs);

  // Check if the buffer is locked by logging writter or not. If locked, then return -1. Else lock this buffer
  shr_buffer = &logging_shr_mem->buffer[logging_shr_mem->current_buffer_idx];
  NS_LOCK_LOGGING_SHR_BUFFER(shr_buffer);

  while (local_buf) {
      NSDL3_LOGGING(NULL, NULL, "Copying local buffer to shm block at index = %d", logging_shr_mem->current_buffer_idx);
      memcpy(shr_buffer->data_buffer, local_buf->data_buffer, log_shr_buffer_size);
      logging_shr_mem->bytes_written = log_shr_buffer_size; // Set this to know if this buffer has data or not. (Used in flush_logging_buffere)
      NSDL3_LOGGING(NULL, NULL, "logging_shr_mem->bytes_written = %d", logging_shr_mem->bytes_written);

    freed_buf = local_buf;
    local_buf = local_buf->next;
    if (!local_buf)
      last_local_buf = NULL;

    if (free_local_bufs) { // Added freed local buf on the head of freed linked list
      NSDL3_LOGGING(NULL, NULL, "Adding freed local buffer in head of freed linked list");
      freed_buf->next = free_local_bufs;
      free_local_bufs = freed_buf;
      total_free_local_bufs++;
    } else {
      NSDL3_LOGGING(NULL, NULL, "Adding freed local buffer in head of freed linked list which is empty");
      freed_buf->next = NULL;
      free_local_bufs = freed_buf;
      total_free_local_bufs++;
    }
    if (inc_shr_buf() == -1)
    {
      NSDL3_LOGGING(NULL, NULL, "No more Shm block is available to copy local buf");
      return -1;
    }
    shr_buffer = &logging_shr_mem->buffer[logging_shr_mem->current_buffer_idx];
  }
  return 0;
}

static inline local_buffer* get_free_local_buffer() {
  local_buffer* return_mem;

  NSDL2_LOGGING(NULL, NULL, "Method called. local_buf = %p, last_local_buf = %p, free_local_bufs = %p, total_free_local_bufs = %d, total_local_bufs = %d", local_buf, last_local_buf, free_local_bufs, total_free_local_bufs, total_local_bufs);

  if (free_local_bufs) {
    return_mem = free_local_bufs;
    free_local_bufs = free_local_bufs->next;
    total_free_local_bufs--;
  } else {
    MY_MALLOC(return_mem , sizeof(local_buffer), "return_mem ", -1);
    //malloc data_buffer
    MY_MALLOC(return_mem->data_buffer, log_shr_buffer_size, "return_mem->data_buffer", -1); 
    total_local_bufs++;
    return_mem->next = NULL;
  }

  return_mem->next = NULL;

  if (local_buf) { // Linked list is not empty (Head is not NULL)
    last_local_buf->next = return_mem;
    return_mem->next = NULL;
    last_local_buf = return_mem;
  } else // Linked list is empty (Head is NULL)
    local_buf = last_local_buf = return_mem;

  return return_mem;
}

inline void fill_up_buffer(int size_written, char* buffer) {
  int size_left = log_shr_buffer_size - size_written;
  int var = -1;
  NSDL2_LOGGING(NULL, NULL, "Method called. size_written = %d, size_left = %d", size_written, size_left);
  NSDL4_LOGGING(NULL, NULL, "buffer = %s", buffer);

  if (size_left) {
  #if 0
    if (size_left >= 246) {
      NSTL1_OUT(NULL, NULL, "fill_up_buffer: size left is %d and this should not happen\n", size_left);
      exit(-1);
    }
    size_left += TOTAL_RECORDS;
  #endif
    memcpy(buffer + size_written, &var, 1);
  }
}

inline char* get_mem_ptr(int size) {
  char* return_buf;

  NSDL2_LOGGING(NULL, NULL, "Method called. logging_shr_mem->bytes_written = %d, local_written_size = %d, size = %d", logging_shr_mem->bytes_written, local_written_size, size);

  if (last_local_buf) { /* finish writing to local buffer */
    if ((local_written_size + size) <= log_shr_buffer_size) {
      NSDL3_LOGGING(NULL, NULL, "Enough space is available in the current local block");
      return_buf = last_local_buf->data_buffer + local_written_size;
      local_written_size += size;
      return return_buf;
    } else { /* try to write the local buffer to the shared buffer and get allocate a shared buffer */
      NSDL3_LOGGING(NULL, NULL, "Enough space is NOT available in the current local block");
      fill_up_buffer(local_written_size, last_local_buf->data_buffer);
      if (copy_local_buffers() == -1) {  /* could not copy all the local buffers to the shared buffers */
        NSDL3_LOGGING(NULL, NULL, "Could not copy all the local buffers to the shared buffers");
	local_written_size = size;
	return get_free_local_buffer()->data_buffer;
      } else {  /* copied all of the local buffers to the shared buffers */
        NSDL3_LOGGING(NULL, NULL, "Copied all of the local buffers to the shared buffers");
	write_buffer* shr_buffer = &logging_shr_mem->buffer[logging_shr_mem->current_buffer_idx];
	logging_shr_mem->bytes_written = size;
	return shr_buffer->data_buffer;
      }
    }
  } else { /* write to shared buffer */
    write_buffer* shr_buffer = &logging_shr_mem->buffer[logging_shr_mem->current_buffer_idx];
    if ((logging_shr_mem->bytes_written + size) <= log_shr_buffer_size) {
      NSDL3_LOGGING(NULL, NULL, "Enough space is available in the current shm block");
      return_buf = shr_buffer->data_buffer + logging_shr_mem->bytes_written;
      logging_shr_mem->bytes_written += size;    /* There is space to write into the current shared buffer */
      return return_buf;
    } else {
      NSDL3_LOGGING(NULL, NULL, "Enough space is NOT available in the current shm block");
      fill_up_buffer(logging_shr_mem->bytes_written, shr_buffer->data_buffer);    /* Need to fill up the current shared buffer and get a new buffer */
      if (inc_shr_buf() == -1) {
        NSDL3_LOGGING(NULL, NULL, "No more shm blocks avaialable");
	local_written_size = size;
	return get_free_local_buffer()->data_buffer;
      } else {
        NSDL3_LOGGING(NULL, NULL, "Shm block is avaialable at index = %d", logging_shr_mem->current_buffer_idx);
	write_buffer* shr_buffer = &logging_shr_mem->buffer[logging_shr_mem->current_buffer_idx];
	logging_shr_mem->bytes_written = size;
	return shr_buffer->data_buffer;
      }
    }
  }
}

int start_req_rep_uploader(int is_recovery, int reader_run_mode)
{
  /*"-n <Gen TestRunNum> "        testidx
    "-P <Gen Partition_Idx> "   g_partition_idx
    "-W <Gen Work dir> "       getenv("NS_WDIR")
    "-d <Gen tmp dir> "     need to create path
    "-k <Shared memory key> "   key would be one send to logging writer
    "-r (recovery mode) "     
    "-s <Controller IP Address> "   master_ip 
    "-p <Controller Port> "   7891  
    "-g <Gen Name> "      global_settings->event_generating_host
    "-w <Controller wdir> "   getenv("NS_CONTROLLER_WDIR")
    "-N <Controller tr>      TR - getenv("NS_CONTROLLER_TEST_RUN") 
    "-H <rbu_lighthouse_mode>"
    "-j <rbu_performance_trace(JSProfiler)>"
 */
  NSDL1_LOGGING(NULL, NULL, "Starting nia_req_rep_uploader");
  if ((nia_req_rep_uploader_pid = fork()) ==  0 ) 
  {
    char partition_name[64]; 
    char req_rep_uploader_bin_path[1024];
    char gen_trun[64];
    char ctrl_port[10];
    //char ns_user_name[512];
    //char *ptr;
    char isRecovery[10];
    char readerRunMode[10];
    char trace_level_str[16];
    char rbuEnabled[10] = "0";
    char ws_enabled[10] = "0";
    char g_tracing_mode[10] = "0";
    char captureClipEnabled[10] = "0";
    char rbuLighthouseEnabled[2] = "0";
    char rbuPerformanceTraceEnabled[2] = "0";  //JS Profiler
    int grp_idx, ret;

    sprintf(isRecovery, "%d", is_recovery);
    sprintf(readerRunMode, "%d", reader_run_mode);
    sprintf(partition_name, "%lld", g_start_partition_idx);

    /*getting gen user name
    strcpy(ns_user_name, netstorm_usr_and_grp_name);
    ptr = strchr(ns_user_name, ':');
    if(ptr != NULL)
      *ptr = '\0';
    */

    sprintf(req_rep_uploader_bin_path, "%s/bin/nia_req_rep_uploader", g_ns_wdir); 
    sprintf(gen_trun, "%d", testidx);
    sprintf(ctrl_port, "%d", 7891);
    sprintf(trace_level_str, "%d", global_settings->nirru_trace_level);
   
    CLOSE_INHERITED_FD_FROM_CHILD // close control and data conn epoll fd on child 
    //if RBU is enabled
    if(global_settings->protocol_enabled & RBU_API_USED) {
      NSDL2_LOGGING(NULL, NULL, "enable_capture_clip = %d, total_runprof_entries", group_default_settings->rbu_gset.enable_capture_clip, total_runprof_entries);
      for(grp_idx = 0; grp_idx < total_runprof_entries; grp_idx++) {
        NSDL1_LOGGING(NULL, NULL, "gset.rbu_gset.enable_capture_clip = %d", runprof_table_shr_mem[grp_idx].gset.rbu_gset.enable_capture_clip); 
        if(runprof_table_shr_mem[grp_idx].gset.rbu_gset.enable_capture_clip) {
          strcpy(captureClipEnabled, "1");
        }
        if(runprof_table_shr_mem[grp_idx].gset.rbu_gset.lighthouse_mode) {
          strcpy(rbuLighthouseEnabled, "1");
        }
        if(runprof_table_shr_mem[grp_idx].gset.rbu_gset.performance_trace_mode) {
          strcpy(rbuPerformanceTraceEnabled, "1");
        }
      }
      strcpy(rbuEnabled, "1");
    }

    //If WebSocket enabled
    if(global_settings->protocol_enabled & WS_PROTOCOL_ENABLED) {
      NSDL2_LOGGING(NULL, NULL, "WebSocket is enabled");
      strcpy(ws_enabled, "1");   
    } 
    //if G_TRACING is enabled
    if ((get_max_tracing_level() > TRACE_DISABLE) && (get_max_trace_dest() > 0)) {
      strcpy(g_tracing_mode, "1");
    }

    //write req_rep_uploader_pid pid in get_all_process_pid file.
   ret = nslb_write_all_process_pid(getpid(), "req rep uploader's pid", g_ns_wdir, testidx, "a");
    if( ret == -1 )
    {
      NSTL1_OUT(NULL, NULL, "failed to open the req rep uploader's pid file\n");
      END_TEST_RUN;
    }
    ret = nslb_write_all_process_pid(getppid(), "req rep uploader's parent's pid", g_ns_wdir, testidx, "a");
   if(ret == -1)
    {
      NSTL1_OUT(NULL, NULL, "failed to open the req rep uploader's pid file\n");
      END_TEST_RUN;
    }
    NSDL1_LOGGING(NULL, NULL, "Arguments are gen_trun= %s, partition_name = %s, g_ns_wdir = %s, "
			      "shr_mem_new_tr_key = %s, master_ip = %s, "
			      "global_settings->event_generating_host = %s, isRecovery = %s, readerRunMode = %s, "
                              "req_rep_uploader_bin_path = %s,trace_level = %s, rbuEnabled = %s, g_tracing_mode =  %s, "
                              "captureClipEnabled = %s, rbuLighthouseEnabled = %s, rbuPerformanceTraceEnabled = %s, ws_enabled = %s", 
                              gen_trun, partition_name, g_ns_wdir, shr_mem_new_tr_key, master_ip, 
                              global_settings->event_generating_host, isRecovery, readerRunMode, req_rep_uploader_bin_path, trace_level_str, 
                              rbuEnabled, g_tracing_mode, captureClipEnabled, rbuLighthouseEnabled, rbuPerformanceTraceEnabled, ws_enabled);

    if (execlp(req_rep_uploader_bin_path, "nia_req_rep_uploader", 
			 "-r", isRecovery, "-o", readerRunMode,
       "-n", gen_trun, "-P", partition_name,
       "-W", g_ns_wdir, 
       "-k", shr_mem_new_tr_key,
       "-s", master_ip, "-p", ctrl_port, 
       "-g", global_settings->event_generating_host, 
       "-w", getenv("NS_CONTROLLER_WDIR"),
       "-N", getenv("NS_CONTROLLER_TEST_RUN"),
       "-R", rbuEnabled,
       "-M", g_tracing_mode,
       "-C", captureClipEnabled,
       "-H", rbuLighthouseEnabled,
       "-j", rbuPerformanceTraceEnabled,
       "-l", trace_level_str,  
       "-S", ws_enabled, NULL) == -1) 
        {
          NSDL1_LOGGING(NULL, NULL, "Initializing nia_req_rep_uploader: error in execl");
          NSTL1(NULL, NULL, "Initializing nia_req_rep_uploader: error in execl\n");
          perror("execl");
          NS_EXIT(-1, "Initializing nia_req_rep_uploader: error in execl");
        }
  }
  else 
  {
    if (nia_req_rep_uploader_pid < 0) {
      NSTL1(NULL, NULL, "Error in forking the nia_req_rep_uploader process\n");
      if(is_recovery == 0) {
        NS_EXIT(-1, "Error in forking the nia_req_rep_uploader process");
      }
      else {
        return -1;
      }
    }
  } 
  return 0;
}

/* Function is called from both parent and NVM to open dlog file in append mode.
   Parent will call from initial and partition switch time via start_logging()
   NVM will call at parition switch time, on receiving next partition message. 
   Returns 0 on success and -1 in error case
 */
int open_dlog_file_in_append_mode (int init_or_partition_call) 
{
  char file_buf[MAX_FILE_NAME];

  NSDL2_LOGGING(NULL, NULL, "Method called, init_or_partition_call = %d", init_or_partition_call);

  NSTL1(NULL, NULL, "Opening dlog file by %s at partition switch time.", (init_or_partition_call == 0)?"parent at init or":"NVM");

  sprintf(file_buf, "logs/%s/reports/raw_data/dlog", global_settings->tr_or_partition);
  if ((runtime_logging_fd = open(file_buf, O_CREAT | O_CLOEXEC | O_APPEND | O_RDWR | O_NONBLOCK | O_LARGEFILE, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)) == -1) {
    NSTL1_OUT(NULL, NULL, "Error in opening logging file %s\n", file_buf);
    return -1;
  }
  return 0;
}

int start_logging(const Global_data* gdata, int testidx, int create_slog) {
  NSDL2_LOGGING(NULL, NULL, "Method called, testidx = %d, g_partition_idx = %ld, g_prev_partition_idx = %lld",
                             testidx, g_partition_idx, g_prev_partition_idx);
  char file_buf[MAX_FILE_NAME];
  static long long slog_partition_idx = -1;
  char file_link_buf[MAX_FILE_NAME];
  struct stat slog_s;

  if(create_slog) //create only one time while calling from netstorm.c, do not create during switching 
  {
    //slog will be created on test start/restart as slog.partition_name in case of partition_creation_mode on.
    //A hard link to slog is created in partition.
    //While switching we need to keep track of slog as many slog's can be present in common files.
    //Hence saving slog_partition_idx variable. This will hold partition_id of current slog.
    slog_partition_idx = g_partition_idx;
    sprintf(file_buf, "%s/logs/TR%d/%lld/reports/raw_data/slog", g_ns_wdir, testidx, slog_partition_idx);

    if ((static_logging_fd = open(file_buf, O_CREAT | O_CLOEXEC | O_APPEND | O_RDWR,  S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)) == -1)
    {
      NSTL1_OUT(NULL, NULL, "start_logging: Error in opening logging file %s\n", file_buf);
      return -1;
    }
    if((static_logging_fp = fdopen(static_logging_fd, "a")) == NULL) 
    {
      NSTL1_OUT(NULL, NULL, "start_logging: Error in opening logging file %s using fdopen\n", file_buf);
      return -1;
    } 
    /*In release 3.9.7: Page dump has been redesign therefore log and sess_status_log files wont be created*/
#if 0
    /* Initialize session status recording file. */
    sprintf(file_buf, "logs/TR%d/sess_status_log", testidx);
    if ((session_status_fd = open(file_buf, O_CREAT | O_APPEND | O_RDWR | O_TRUNC | O_NONBLOCK | O_LARGEFILE, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)) == -1) {
      NSTL1_OUT(NULL, NULL, "Unable to open file %s\n", file_buf);
      return -1;
    }
#endif 
  }
 
  //creating hard link of slog in partition
  if(global_settings->partition_creation_mode)
  {
    sprintf(file_buf, "%s/logs/TR%d/%lld/reports/raw_data/slog", g_ns_wdir, testidx, create_slog?slog_partition_idx:g_prev_partition_idx);
    sprintf(file_link_buf, "%s/logs/%s/reports/raw_data/slog", g_ns_wdir, global_settings->tr_or_partition);
    //int ret = link(file_buf, file_link_buf);  //TODO Krishna Error handling
    //if(ret < 0) {
    if(stat(file_link_buf, &slog_s) && ((link(file_buf, file_link_buf)) == -1))
    {
      if((symlink(file_buf, file_link_buf)) == -1)  //Manmeet
        NSTL1(NULL, NULL, "Error: Unable to create link %s from %s, err = %s", file_link_buf, file_buf, nslb_strerror(errno));
    }
    NSDL2_LOGGING(NULL, NULL, "Created link of %s in %s", file_buf, file_link_buf);
  }

  /*open dlog file*/
  if ((open_dlog_file_in_append_mode(0)) == -1)
    return -1;

  /* Initialize session status recording file. */
  /*sprintf(file_buf, "logs/%s/sess_status_log", global_settings->tr_or_partition);
  if ((session_status_fd = open(file_buf, O_CREAT | O_APPEND | O_RDWR | O_TRUNC | O_NONBLOCK | O_LARGEFILE, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)) == -1) {
    NSTL1_OUT(NULL, NULL, "Unable to open file %s\n", file_buf);
    return -1;
  }*/

  return 0;
}

static void send_nsa_logger_pid_change_message()
{
  int i;
  parent_child nsa_logger_msg;

  NSDL2_LOGGING(NULL, NULL, "Method Called. writer_pid = %d", writer_pid);

  nsa_logger_msg.opcode = NSA_LOGGER_PID_CHANGE_MESSAGE;
  nsa_logger_msg.nsa_logger_pid = writer_pid; 

  //send to NVM
  for(i = 0; i < global_settings->num_process; i++)
  {
    //if NVM is over then no need to send msg to NVM
    if (g_msg_com_con[i].fd != -1)
    {
      NSDL3_LOGGING(NULL, NULL, "Sending msg to NVM id = %d %s", i,
              msg_com_con_to_str(&g_msg_com_con[i]));
      nsa_logger_msg.msg_len = sizeof(parent_child) - sizeof(int);
      write_msg(&g_msg_com_con[i], (char *)&nsa_logger_msg, sizeof(parent_child), 0, CONTROL_MODE);
    }
  }
}

//TODO:we need to set pid -1 (if recovery to be done) & 2 (if no recovery to be done)
static int start_nsa_logger(int num_children, int recovery_flag)
{
  if ((writer_pid = fork()) ==  0 ) {
    char lw_bin_path[512 + 1] = "";
    char num_children_str[8];
    char logging_file[MAX_FILE_NAME];
    char debug_level_writer[8];
    char test_idx_str[1024];
    char reader_write_time_str[1024];
    char reader_running_mode[8];
    char size_of_dynamic_table[16];
    char size_of_dynamic_tx_table[16];
    char log_wrt_shr_blk_num[8];
    char generator_id_buf[125];
    //char g_partition_idx_str[64];
    char cav_epoch_year_str[8];
    char log_shr_buffer_size_str[16];
    char total_generator_entries_str[8];
    int ret = 0;
    int ret1=0;
    char err_msg[100];
    sprintf(num_children_str, "%d", num_children);
    sprintf(debug_level_writer, "%d", global_settings->logging_writer_debug);
    //sprintf(logging_file, "logs/TR%d/reports/raw_data/dlog", testidx);
    sprintf(logging_file, "logs/%s/reports/raw_data/dlog", global_settings->tr_or_partition);
    sprintf(reader_running_mode, "%d", global_settings->reader_run_mode);
    sprintf(test_idx_str, "%d", testidx);
    sprintf(reader_write_time_str, "%d", global_settings->reader_csv_write_time);
    // Dynamic URL table size passed as it is used of URL ID Normalization
    sprintf(size_of_dynamic_table, "%d", global_settings->url_hash.dynamic_url_table_size);
    sprintf(size_of_dynamic_tx_table, "%d", global_settings->dyn_tx_norm_table_size);
    sprintf(log_wrt_shr_blk_num, "%d", logging_shr_blk_count);
    sprintf(generator_id_buf, "%d", g_generator_idx);
    //sprintf(g_partition_idx_str, "%ld", g_partition_idx);
    sprintf(cav_epoch_year_str, "%d", global_settings->cav_epoch_year);
    sprintf(log_shr_buffer_size_str, "%d", log_shr_buffer_size);
    sprintf(total_generator_entries_str, "%d", sgrp_used_genrator_entries);

    CLOSE_INHERITED_EPOLL_FROM_CHILD // close control and data conn epoll fd on child
   // close(ndc_mccptr.fd);
    nslb_close_all_open_files(-1, 0, NULL);
   
    //write logger pid in get_all_process_pid file.
    ret = nslb_write_all_process_pid(getpid(), "logging writer's pid", g_ns_wdir, testidx, "a");
    if( ret == -1 )
    {
      NSTL1_OUT(NULL, NULL, "failed to open the logging writer's pid file\n");
      END_TEST_RUN;
    }
    
    ret1= nslb_write_process_pid(getpid(), "logging writer's pid", g_ns_wdir, testidx,"w","NLW.pid",err_msg);
    if(ret1 ==-1)
    {
      NSTL1_OUT(NULL, NULL, "%s",err_msg);
    }
     
    ret = nslb_write_all_process_pid(getppid(), "logging writer's parent's pid", g_ns_wdir, testidx, "a");
    if( ret == -1 )
    {
      NSTL1_OUT(NULL, NULL, "failed to open the logging writer's parent's pid file\n");
      END_TEST_RUN;
    }
  /*  ret1 = nslb_write_process_pid(getppid(), "logging writer's parent's pid", g_ns_wdir, testidx, "w","nlw.ppid",err_msg);
    if( ret1 == -1)
    {
      NSTL1_OUT(NULL, NULL, "%s",err_msg);
    }
*/

    sprintf(lw_bin_path, "%s/bin/nsa_logger", g_ns_wdir);
    NSDL2_LOGGING(NULL, NULL, "NDE MODE .... cav_epoch_year_str = %s, test_idx_str = %s, shr_mem_new_tr_key = %s, reader_running_mode = %s \n", cav_epoch_year_str, test_idx_str, shr_mem_new_tr_key, reader_running_mode);

    if (execlp(lw_bin_path, "nsa_logger", num_children_str, logging_file, shr_mem_key, reader_running_mode, test_idx_str, reader_write_time_str, size_of_dynamic_table, log_wrt_shr_blk_num, generator_id_buf, cav_epoch_year_str, shr_mem_new_tr_key, log_shr_buffer_size_str, total_generator_entries_str, size_of_dynamic_tx_table, NULL) == -1) {
      NSTL1(NULL, NULL, "initialize_logging_memory: error in execl");
      perror("execl");
      NS_EXIT (-1, "initialize_logging_memory: error in execl");
      //return -1;
    }
  } else{
    if (writer_pid < 0) {
      NSTL1(NULL, NULL, "error in forking the logging_writer process\n");
      if(recovery_flag == 0)
        NS_EXIT (-1, "error in forking the logging_writer process");
    }
  }
  /* This is to debug the page_dump issue: All pages are not coming in Page dump 
  sleep(2);
  NSTL1_OUT(NULL, NULL, "nsa_logger started\n");
  init_nvm_logging_shr_mem_test(buffers);
  write_data_in_debug_logging();
  */
 
  set_test_run_info_writer_pid(writer_pid); 
  return 0;
}

//This function is called when the test starts and not while in recovery. So this function has
//exit call on failure.
shr_logging* initialize_logging_memory(int num_children, const Global_data* gdata, int testidx, int g_generator_idx) {
  int log_mem_fd;
  shr_logging* buffers;
  shr_logging* logging_memory;
  int i, j;
  //char shr_mem_key[64];
  //char shr_mem_new_tr_key[64];
  
  NSDL2_LOGGING(NULL, NULL, "Method called, num_children = %d, testidx = %d", num_children, testidx);
  //sprintf(shr_mem_key, "0x%x", shm_base + total_num_shared_segs);
  log_shr_buffer_size = gdata->log_shr_buffer_size;
  //if ((log_mem_fd = shmget(shm_base + total_num_shared_segs, sizeof(shr_logging) * num_children, IPC_CREAT | IPC_EXCL | 0666)) == -1)
  NSDL2_LOGGING(NULL, NULL, "logging_shr_blk_count =%d, log_shr_buffer_size = %d",logging_shr_blk_count, log_shr_buffer_size);
  int mem_size = (LOGGING_PER_CHILD_SHR_MEM_SIZE(log_shr_buffer_size, logging_shr_blk_count)) * num_children;
  if ((log_mem_fd = shmget(shm_base, mem_size, IPC_CREAT | IPC_EXCL | 0666)) == -1)
  {
     check_shared_mem(mem_size);
    perror("error in allocating shared memory for the logging memory");
    return NULL;
  }
  //total_num_shared_segs++;

  //Rather than passing, key, pass fd. because parent might (mark)delete, before child does shmget
  sprintf(shr_mem_key, "%d", log_mem_fd);
  if ((logging_memory = (shr_logging*) shmat(log_mem_fd, NULL, 0)) == NULL) {
    perror("error in getting shared memory for the logging_memory");
    return NULL;
  }
  if (shmctl (log_mem_fd, IPC_RMID, NULL)) {
	printf ("ERROR: unable to mark shm removal for 'logging' of size=%lu err=%s\n",
                 (u_ns_ptr_t)(sizeof(shr_logging) + ((logging_shr_blk_count - 1) * sizeof(write_buffer))) * num_children, nslb_strerror(errno));
        return NULL;
   }
  NSDL2_LOGGING(NULL, NULL, "logging_shr_blk_count before loop =%d",logging_shr_blk_count);
  char *ptr;
  ptr =(char *) logging_memory;

  for (i = 0; i < num_children; i++) {
    buffers = (shr_logging *) (ptr + (i * (LOGGING_PER_CHILD_SHR_MEM_SIZE(log_shr_buffer_size, logging_shr_blk_count))));
    for (j = 0; j < logging_shr_blk_count; j++) {
      buffers->buffer[j].memory_flag = 1;
      buffers->buffer[j].disk_flag = 0;
      //Note: This data_buffer should not be used in logging writer. because it will have different address space.
      //So we should use LOGGING_DATA_BUFFER_PTR macro to get address.
      buffers->buffer[j].data_buffer = LOGGING_DATA_BUFFER_PTR(ptr, i, j, log_shr_buffer_size, logging_shr_blk_count);
      if(sem_init(&(buffers->buffer[j].sem), 1, 1) == -1)
      {
        //perror("sem_init");
        NS_EXIT(-1, CAV_ERR_1000042, errno, nslb_strerror(errno));
      } 
    }
    buffers->current_buffer_idx = 0;
    buffers->cur_lw_idx = 0;
    buffers->bytes_written = 0;
    buffers->write_flag = 0;
  }

  buffers->write_flag = 1;
#if 0
    //For debug purpose.
    FILE *log_fp = fopen("/tmp/mydata", "w");
    for(i = 0; i < num_children; i++) 
    {
      buffers = (shr_logging *) (ptr + (i * (LOGGING_PER_CHILD_SHR_MEM_SIZE(log_shr_buffer_size, logging_shr_blk_count))));
      fprintf(log_fp, "%p:%d", buffers, mem_size);
      for(j = 0; j < logging_shr_blk_count; j++)
      {
        fprintf(log_fp, "\t%p", buffers->buffer[j].data_buffer);
      }
      fprintf(log_fp, "\n");
    }
    fclose(log_fp);
#endif

  //Initialize component recovery data
  if(nslb_init_component_recovery_data(&ReaderRecoveryData, MAX_RECOVERY_COUNT, (global_settings->progress_secs/1000 + 5), RESTART_TIME_THRESHOLD_IN_SEC) == 0)
  {
     NSDL2_LOGGING(NULL, NULL, "Main thread Recovery data initialized with"
                      "MAX_RECOVERY_COUNT = %d, RESTART_TIME_THRESHOLD_IN_SEC = %d",
                             global_settings->progress_secs , RESTART_TIME_THRESHOLD_IN_SEC);
  }
  else
  {
     NSDL2_LOGGING(NULL, NULL, "Method Called. Component recovery could not be initialized");
  }
 
  start_nsa_logger(num_children, 0); //TODO: NEED TO HANDLE RETURN VALUES

	//start req rep uploader
	//first argument(0) is recovery mode(is_recovery);
  NSDL2_LOGGING(NULL, NULL, "loader_opcode =%d, global_settings->reader_run_mode = %d", 
                             loader_opcode, global_settings->reader_run_mode);
  
  if((loader_opcode == CLIENT_LOADER) && (global_settings->reader_run_mode == 1))
  {
    if(nslb_init_component_recovery_data(&ReqRepUploaderRecoveryData, MAX_RECOVERY_COUNT, (global_settings->progress_secs/1000 + 5), RESTART_TIME_THRESHOLD_IN_SEC) == 0)
    {
       NSDL1_LOGGING(NULL, NULL, "Initialzation done for ReqRepUploaderRecoveryData"
                      "max_recovery_count = %d, restart_time_threshold_in_sec = %d",
                             MAX_RECOVERY_COUNT, RESTART_TIME_THRESHOLD_IN_SEC);
    }
    else
    {
      NSDL1_LOGGING(NULL, NULL, "Component recovery could not be initialized for ReqRepUploaderRecoveryData");
    }
    start_req_rep_uploader(0,  global_settings->reader_run_mode);
  }
  return logging_memory;
}

int req_rep_uploader_recovery()
{
  NSDL2_LOGGING(NULL, NULL, "Method called nia_req_rep_uploader_pid = %d", nia_req_rep_uploader_pid);
  if (nia_req_rep_uploader_pid > 0)
    return -1;

  if(nslb_recover_component_or_not(&ReqRepUploaderRecoveryData) == 0)
  {
    NSDL2_LOGGING(NULL, NULL, "Main thread, Req Rep Uploader not running. Recovering it");

		//first argument(1) is recovery mode(is_recovery);
    if(start_req_rep_uploader(1, global_settings->reader_run_mode) == 0)
    { 
      NSDL2_LOGGING(NULL, NULL, "Main thread, Req Rep Uploader recovered, Pid = %d.", nia_req_rep_uploader_pid);
    }
    else
    {
      NSDL2_LOGGING(NULL, NULL, "Main thread, Req Rep Uploader recovery failed");
      return -1;
    }
  }
  else
  {
    NSDL2_LOGGING(NULL, NULL, "Main thread, Req Rep Uploader max restart count is over. Cannot recover"
          " Retry count = %d, Max Retry count = %d", 
          ReqRepUploaderRecoveryData.retry_count, ReqRepUploaderRecoveryData.max_retry_count);
    return -1;
  }

  return 0;
}

int nsa_logger_recovery()
{
  NSDL2_LOGGING(NULL, NULL, "Method called");
  if (writer_pid != -1)
    return -1;

  if(nslb_recover_component_or_not(&ReaderRecoveryData) == 0)
  {
    NSDL2_LOGGING(NULL, NULL, "Main thread, Logging writer not running. Recovering logging writer.");

    if(start_nsa_logger(global_settings->num_process, 1) == 0)
    { 
      NSDL2_LOGGING(NULL, NULL, "Main thread, Logging writer recovered, Pid = %d.", writer_pid);
      //send new writer pid to NVM's
      send_nsa_logger_pid_change_message();
    }
    else
    {
      NSDL2_LOGGING(NULL, NULL, "Main thread, Logging writer recovery failed");
      return -1;
    }
  }
  else
  {
    NSDL2_LOGGING(NULL, NULL, "Main thread, Logging writer max restart count is over. Cannot recover LW"
          " Retry count = %d, Max Retry count = %d", ReaderRecoveryData.retry_count, ReaderRecoveryData.max_retry_count);
    return -1;
  }

  return 0;
}

int log_session_record(VUser* vptr, u_ns_ts_t now) {
  char* copy_location;
  unsigned char record_num = SESSION_RECORD;
  unsigned char is_run_phase = 0;
  int rec_index;
  u_ns_ts_t abs_time_in_milisec = 0;
  

  NSDL1_LOGGING(vptr, NULL, "Method called, child_idx = %hd, now = %u", child_idx, now);

  // For Jmeter we need 1VUser which in end of session do record logging.
  // We are logging session record separtely for JMETER. So, its not required here
  if(vptr->sess_ptr->jmeter_sess_name != NULL)
  {
    NSTL1(vptr, NULL, "We are logging session record separtely for JMETER. So, ignoring here");
    return;
  }
  
  if ((gRunPhase == NS_RUN_PHASE_EXECUTE) && (vptr->started_at >= global_settings->test_runphase_start_time))
    is_run_phase = 1;

  if ((copy_location = get_mem_ptr(session_record_size))) {
    
    total_session_records++;
    total_session_record_size += session_record_size;
    memcpy(copy_location, &record_num, UNSIGNED_CHAR_SIZE);
    copy_location += UNSIGNED_CHAR_SIZE;

    // Session Record Session Index
    memcpy(copy_location, &vptr->sess_ptr->sess_norm_id, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    // Session Record Session Instance
    memcpy(copy_location, &vptr->sess_inst, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    // Session Record User IndeX
    memcpy(copy_location, &vptr->user_index, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    // Session Record Group Num
    memcpy(copy_location, &(runprof_table_shr_mem[vptr->group_num].grp_norm_id), UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    // Session Record Child id
    memcpy(copy_location, &child_idx, SHORT_SIZE);
    copy_location += SHORT_SIZE;

    // Session Record Is Run Phase
    memcpy(copy_location, &is_run_phase, UNSIGNED_CHAR_SIZE);
    copy_location += UNSIGNED_CHAR_SIZE;

    // Session Record Access
    // Calculting the index by taking the difference of shared memory pointers
    rec_index = vptr->access - accattr_table_shr_mem;  
    memcpy(copy_location, &rec_index, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    // Session Record Location
    rec_index = vptr->location - locattr_table_shr_mem;
    memcpy(copy_location, &rec_index, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    // Session Record Browser
    rec_index = vptr->browser - browattr_table_shr_mem;
    memcpy(copy_location, &rec_index, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    // Session Record Freq
    // rec_index = vptr->freq - freqattr_table_shr_mem;
    rec_index = 0; // Not using freq starting 3.9.4 release
    memcpy(copy_location, &rec_index, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    // Session Record Machine Attribute
    // rec_index = vptr->machine - machattr_table_shr_mem;
    rec_index = 0; // Not using machine starting 3.9.4 release
    memcpy(copy_location, &rec_index, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    // Session Record Started at time
    abs_time_in_milisec = g_time_diff_bw_cav_epoch_and_ns_start_milisec + vptr->started_at;
    memcpy(copy_location, &abs_time_in_milisec, NS_TIME_DATA_SIZE);
    copy_location += NS_TIME_DATA_SIZE;

    // Session Record End Time
    if (vptr->started_at > now) { /* Safety net to saveguard against start time > now */
      abs_time_in_milisec = g_time_diff_bw_cav_epoch_and_ns_start_milisec + vptr->started_at;
      error_log("Start time is more than now (%u). vptr=> %s", now,
                vptr_to_string(vptr, vptr_to_str, MAX_LINE_LENGTH));
    } else {
      abs_time_in_milisec = g_time_diff_bw_cav_epoch_and_ns_start_milisec + now;
    }
    memcpy(copy_location, &abs_time_in_milisec, NS_TIME_DATA_SIZE);
    copy_location += NS_TIME_DATA_SIZE;

    // Session Record Think Duration
    memcpy(copy_location, &vptr->sess_think_duration, NS_TIME_DATA_SIZE);
    copy_location += NS_TIME_DATA_SIZE;

    // Session Record Session Status
    memcpy(copy_location, &vptr->sess_status, UNSIGNED_CHAR_SIZE);
    copy_location += UNSIGNED_CHAR_SIZE;

    // Session Record Session Status
    short  phase_num;
    SET_PHASE_NUM(phase_num);
    memcpy(copy_location, &phase_num, SHORT_SIZE);
    //copy_location += SHORT_SIZE;
  } else {
    return -1;
   }

  return 0;
}

static int log_tx_pg_record_v2(VUser* vptr, u_ns_ts_t start_time, u_ns_ts_t end_time, TxInfo *node_ptr) {
  char* copy_location;
  unsigned char record_num = TX_PG_RECORD_V2;
  int i;

  NSDL2_LOGGING(vptr, NULL, "Method called. node_ptr->num_pages = %d, end_time = %u", node_ptr->num_pages, end_time);

  // In a loop, log all page records which are part of this transaction
  for(i = 0; i < node_ptr->num_pages; i++)
  {
    
    if ((copy_location = get_mem_ptr(tx_pg_record_size))) {

    total_tx_pg_records++;
    total_tx_pg_record_size += tx_pg_record_size;

    // Record type
    memcpy(copy_location, &record_num, UNSIGNED_CHAR_SIZE);
    copy_location += UNSIGNED_CHAR_SIZE;

    // Record Child Id
    memcpy(copy_location, &child_idx, SHORT_SIZE);
    copy_location += SHORT_SIZE;

    // TransactionIndex
    memcpy(copy_location, &node_ptr->hash_code, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    // SessionIndex
    memcpy(copy_location, &vptr->sess_ptr->sess_norm_id, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //Session Instance
    memcpy(copy_location, &vptr->sess_inst, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;


    // TxInstance
    memcpy(copy_location, &node_ptr->instance, SHORT_SIZE);
    copy_location += SHORT_SIZE;

    //Page instance
    memcpy(copy_location, &node_ptr->page_instance[i], SHORT_SIZE);
    copy_location += SHORT_SIZE;
 
    // StartTime
    memcpy(copy_location, &start_time, NS_TIME_DATA_SIZE);
    copy_location += NS_TIME_DATA_SIZE;

    // EndTime
    memcpy(copy_location, &end_time, NS_TIME_DATA_SIZE);
    copy_location += NS_TIME_DATA_SIZE;

    // Tx Status
    memcpy(copy_location, &node_ptr->status, UNSIGNED_CHAR_SIZE);
    copy_location += UNSIGNED_CHAR_SIZE;

    // Note - RespTime is not in dlog. It will be calculated by logging_reader when generating CSV file from dlog
    
    // Phase id
    short phase_num;
    SET_PHASE_NUM(phase_num);
    memcpy(copy_location, &phase_num, SHORT_SIZE);
    NSDL2_LOGGING(vptr, NULL, "total_tx_pg_record_size = %lld, session instance = %u, user id = %u", total_tx_pg_record_size, vptr->sess_inst, vptr->user_index );
    } 
    else 
     return -1;
  }
  return 0;
}
// Record format:
// RecordType, TransactionIndex, ChildIndex, PageInstance, SessionInstance, UserId
//
// RecordType, TransactionIndex, SessionIndex, SessionInstance, ChildIndex, TxInstance, PageInstance, StartTime, EndTime, Status, PhaseIndex, 

// Start and End time is already in abs format
#if 0
static int log_tx_pg_record(VUser* vptr, u_ns_ts_t start_time, u_ns_ts_t end_time, TxInfo *node_ptr) {
  char* copy_location;
  unsigned char record_num = TX_PG_RECORD;
  int i;

  NSDL2_LOGGING(vptr, NULL, "Method called. node_ptr->num_pages = %d, end_time = %u", node_ptr->num_pages, end_time);

  // In a loop, log all page records which are part of this transaction
  for(i = 0; i < node_ptr->num_pages; i++)
  {
    
    if ((copy_location = get_mem_ptr(tx_pg_record_size))) {

    total_tx_pg_records++;
    total_tx_pg_record_size += tx_pg_record_size;

    // Record type
    memcpy(copy_location, &record_num, UNSIGNED_CHAR_SIZE);
    copy_location += UNSIGNED_CHAR_SIZE;

    // TransactionIndex
    memcpy(copy_location, &node_ptr->hash_code, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    // SessionIndex
    memcpy(copy_location, &vptr->sess_ptr->sess_norm_id, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //Session Instance
    memcpy(copy_location, &vptr->sess_inst, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    // Record Child Id
    memcpy(copy_location, &child_idx, SHORT_SIZE);
    copy_location += SHORT_SIZE;

    // TxInstance
    memcpy(copy_location, &node_ptr->instance, SHORT_SIZE);
    copy_location += SHORT_SIZE;

    //Page instance
    memcpy(copy_location, &node_ptr->page_instance[i], SHORT_SIZE);
    copy_location += SHORT_SIZE;
 
    // StartTime
    memcpy(copy_location, &start_time, NS_TIME_DATA_SIZE);
    copy_location += NS_TIME_DATA_SIZE;

    // EndTime
    memcpy(copy_location, &end_time, NS_TIME_DATA_SIZE);
    copy_location += NS_TIME_DATA_SIZE;

    // Tx Status
    memcpy(copy_location, &node_ptr->status, UNSIGNED_CHAR_SIZE);
    copy_location += UNSIGNED_CHAR_SIZE;

    // Note - RespTime is not in dlog. It will be calculated by logging_reader when generating CSV file from dlog
    
    // Phase id
    short phase_num;
    SET_PHASE_NUM(phase_num);
    memcpy(copy_location, &phase_num, SHORT_SIZE);
    NSDL2_LOGGING(vptr, NULL, "total_tx_pg_record_size = %lld, session instance = %u, user id = %u", total_tx_pg_record_size, vptr->sess_inst, vptr->user_index );
    } 
    else 
     return -1;
  }
  return 0;
}
#endif
int log_tx_record_v2(VUser* vptr, u_ns_ts_t now, TxInfo *node_ptr ) {
  char* copy_location;
  unsigned char record_num = TX_RECORD_V2;
  u_ns_ts_t end_time;
  u_ns_ts_t start_time = 0;

  NSDL2_LOGGING(vptr, NULL, "Method called");
  if ((copy_location = get_mem_ptr(tx_record_size))) {
   
    total_tx_records++;
    total_tx_record_size += tx_record_size;
    memcpy(copy_location, &record_num, UNSIGNED_CHAR_SIZE);
    copy_location += UNSIGNED_CHAR_SIZE;
  
    //  Tx Record Child Id
    memcpy(copy_location, &child_idx, SHORT_SIZE);
    copy_location += SHORT_SIZE;

    // Tx Record hash code or tx index
    memcpy(copy_location, &node_ptr->hash_code, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;
 
    //  Tx Record Session Index
    memcpy(copy_location, &vptr->sess_ptr->sess_norm_id, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //  Tx Record Session Instance
    memcpy(copy_location, &vptr->sess_inst, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;
 
    //  Tx Record Tx Instance
    memcpy(copy_location, &node_ptr->instance, SHORT_SIZE);
    copy_location += SHORT_SIZE;

    //  Tx Record Tx begin at
    start_time = g_time_diff_bw_cav_epoch_and_ns_start_milisec + node_ptr->begin_at;
    memcpy(copy_location, &start_time, NS_TIME_DATA_SIZE);
    copy_location += NS_TIME_DATA_SIZE;

    //  Tx Record Tx end at
    if(!runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_rbu) // Non RBU
    {
      if (node_ptr->begin_at > now) { /* Safety net to saveguard against start time > now */
        end_time = g_time_diff_bw_cav_epoch_and_ns_start_milisec + node_ptr->begin_at;
        error_log("Transaction start time is more than now (%u). vptr=> %s", now, vptr_to_string(vptr, vptr_to_str, MAX_LINE_LENGTH));
      } else {
          end_time = g_time_diff_bw_cav_epoch_and_ns_start_milisec + now;
      }
    }
    else // RBU
    {
      // In case of RBU, we cannot use end_time as there is time spent in waiting for har file
      // So derive end time - start time + tx time + think time
      NSDL2_LOGGING(vptr, NULL, "RBU- g_time_diff_bw_cav_epoch_and_ns_start_milisec = %d, begin_at = %ld, think_duration = %ld", 
                                      g_time_diff_bw_cav_epoch_and_ns_start_milisec, node_ptr->rbu_tx_time, node_ptr->think_duration);
      end_time = g_time_diff_bw_cav_epoch_and_ns_start_milisec + node_ptr->begin_at + node_ptr->rbu_tx_time + node_ptr->think_duration;
      NSDL2_LOGGING(vptr, NULL, "RBU- transaction end_time = %ld", end_time);
    }

    memcpy(copy_location, &end_time, NS_TIME_DATA_SIZE);
    copy_location += NS_TIME_DATA_SIZE;

    //  Tx Record Tx Think Time
    memcpy(copy_location, &node_ptr->think_duration, NS_TIME_DATA_SIZE);
    copy_location += NS_TIME_DATA_SIZE;

    //  Tx Record Tx Status
    memcpy(copy_location, &node_ptr->status, UNSIGNED_CHAR_SIZE);
    copy_location += UNSIGNED_CHAR_SIZE;

    // Tx Record phase id
    short phase_num;
    SET_PHASE_NUM(phase_num);
    memcpy(copy_location, &phase_num, SHORT_SIZE);
    
  } else
    return -1;

  log_tx_pg_record_v2(vptr, start_time, end_time, node_ptr);
  return 0;
}

#if 0
int log_tx_record(VUser* vptr, u_ns_ts_t now, TxInfo *node_ptr ) {
  char* copy_location;
  unsigned char record_num = TX_RECORD;
  u_ns_ts_t end_time;
  u_ns_ts_t start_time = 0;

  NSDL2_LOGGING(vptr, NULL, "Method called");
  if ((copy_location = get_mem_ptr(tx_record_size))) {
   
    total_tx_records++;
    total_tx_record_size += tx_record_size;
    memcpy(copy_location, &record_num, UNSIGNED_CHAR_SIZE);
    copy_location += UNSIGNED_CHAR_SIZE;
  
    // Tx Record hash code or tx index
    memcpy(copy_location, &node_ptr->hash_code, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;
 
    //  Tx Record Session Index
    memcpy(copy_location, &vptr->sess_ptr->sess_norm_id, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //  Tx Record Session Instance
    memcpy(copy_location, &vptr->sess_inst, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;
 
    //  Tx Record Child Id
    memcpy(copy_location, &child_idx, SHORT_SIZE);
    copy_location += SHORT_SIZE;

    //  Tx Record Tx Instance
    memcpy(copy_location, &node_ptr->instance, SHORT_SIZE);
    copy_location += SHORT_SIZE;

    //  Tx Record Tx begin at
    start_time = g_time_diff_bw_cav_epoch_and_ns_start_milisec + node_ptr->begin_at;
    memcpy(copy_location, &start_time, NS_TIME_DATA_SIZE);
    copy_location += NS_TIME_DATA_SIZE;

    //  Tx Record Tx end at
    if(!runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_rbu) // Non RBU
    {
      if (node_ptr->begin_at > now) { /* Safety net to saveguard against start time > now */
        end_time = g_time_diff_bw_cav_epoch_and_ns_start_milisec + node_ptr->begin_at;
        error_log("Transaction start time is more than now (%u). vptr=> %s", now, vptr_to_string(vptr, vptr_to_str, MAX_LINE_LENGTH));
      } else {
          end_time = g_time_diff_bw_cav_epoch_and_ns_start_milisec + now;
      }
    }
    else // RBU
    {
      // In case of RBU, we cannot use end_time as there is time spent in waiting for har file
      // So derive end time - start time + tx time + think time
      NSDL2_LOGGING(vptr, NULL, "RBU- g_time_diff_bw_cav_epoch_and_ns_start_milisec = %d, begin_at = %ld, think_duration = %ld", 
                                      g_time_diff_bw_cav_epoch_and_ns_start_milisec, node_ptr->rbu_tx_time, node_ptr->think_duration);
      end_time = g_time_diff_bw_cav_epoch_and_ns_start_milisec + node_ptr->begin_at + node_ptr->rbu_tx_time + node_ptr->think_duration;
      NSDL2_LOGGING(vptr, NULL, "RBU- transaction end_time = %ld", end_time);
    }

    memcpy(copy_location, &end_time, NS_TIME_DATA_SIZE);
    copy_location += NS_TIME_DATA_SIZE;

    //  Tx Record Tx Think Time
    memcpy(copy_location, &node_ptr->think_duration, NS_TIME_DATA_SIZE);
    copy_location += NS_TIME_DATA_SIZE;

    //  Tx Record Tx Status
    memcpy(copy_location, &node_ptr->status, UNSIGNED_CHAR_SIZE);
    copy_location += UNSIGNED_CHAR_SIZE;

    // Tx Record phase id
    short phase_num;
    SET_PHASE_NUM(phase_num);
    memcpy(copy_location, &phase_num, SHORT_SIZE);
    
  } else
    return -1;

  log_tx_pg_record_v2(vptr, start_time, end_time, node_ptr);
  return 0;
}
#endif

int log_page_record_v2(VUser* vptr, u_ns_ts_t now) {
  char* copy_location;
  unsigned char record_num = PAGE_RECORD_V2;
  int cur_tx = -1;
  u_ns_ts_t abs_time_in_milisec = 0;

  NSDL1_LOGGING(vptr, NULL, "Method called, child_idx = %hd, now = %u, page_record_size = %d", child_idx, now, page_record_size);
  if ((copy_location = get_mem_ptr(page_record_size))) {

    total_page_records++;
    total_page_record_size += page_record_size;

    memcpy(copy_location, &record_num, UNSIGNED_CHAR_SIZE);
    copy_location += UNSIGNED_CHAR_SIZE;

    // Page Record Child Id
    memcpy(copy_location, &child_idx, SHORT_SIZE);
    copy_location += SHORT_SIZE;

    // Page Record Current Page
    memcpy(copy_location, &vptr->cur_page->page_norm_id, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    // Page Record Session Index
    memcpy(copy_location, &vptr->sess_ptr->sess_norm_id, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    // Page Record Session Instance
    memcpy(copy_location, &vptr->sess_inst, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    // Page Record Current Tx
    // This will work for single concurrent Transaction only
    cur_tx = tx_get_cur_tx_hash_code(vptr);     
    memcpy(copy_location, &cur_tx, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    // Page Record Tx Instance
    memcpy(copy_location, &vptr->tx_instance, SHORT_SIZE);
    copy_location += SHORT_SIZE;

    // Page Record Page Instance
    memcpy(copy_location, &vptr->page_instance, SHORT_SIZE);
    copy_location += SHORT_SIZE;

    // Page Record Page Begin at
    //Here were are converting this into abs miliseconds as we need download in miliseconds
    abs_time_in_milisec = g_time_diff_bw_cav_epoch_and_ns_start_milisec + vptr->pg_begin_at;
    memcpy(copy_location, &abs_time_in_milisec, NS_TIME_DATA_SIZE);
    copy_location += NS_TIME_DATA_SIZE;

    // Page Record Page end at
    if(!runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_rbu) // Non RBU
    {
      if (vptr->pg_begin_at > now) { /* Safety net to saveguard against start time > now */
        abs_time_in_milisec = g_time_diff_bw_cav_epoch_and_ns_start_milisec + vptr->pg_begin_at;
        error_log("Start time is more than now (%u). vptr=> %s", now,
                  vptr_to_string(vptr, vptr_to_str, MAX_LINE_LENGTH));
      } else {
        abs_time_in_milisec = g_time_diff_bw_cav_epoch_and_ns_start_milisec + now; 
      }
    }
    else
    {
      abs_time_in_milisec = ((g_time_diff_bw_cav_epoch_and_ns_start_milisec + vptr->pg_begin_at) + ((vptr->httpData->rbu_resp_attr->on_load_time != -1)?vptr->httpData->rbu_resp_attr->on_load_time:0));
    }

    memcpy(copy_location, &abs_time_in_milisec, NS_TIME_DATA_SIZE);
    copy_location += NS_TIME_DATA_SIZE;

    // Page Record Page Status
    memcpy(copy_location, &vptr->page_status, UNSIGNED_CHAR_SIZE);
    copy_location += UNSIGNED_CHAR_SIZE;
    // Session Record Session Status
    short phase_num;
    SET_PHASE_NUM(phase_num);
    memcpy(copy_location, &phase_num, SHORT_SIZE);
    //copy_location += SHORT_SIZE;
  } else
    return -1;
  return 0;
}

#if 0
int log_page_record(VUser* vptr, u_ns_ts_t now) {
  char* copy_location;
  unsigned char record_num = PAGE_RECORD;
  int cur_tx = -1;
  u_ns_ts_t abs_time_in_milisec = 0;

  NSDL1_LOGGING(vptr, NULL, "Method called, child_idx = %hd, now = %u, page_record_size = %d", child_idx, now, page_record_size);
  if ((copy_location = get_mem_ptr(page_record_size))) {

    total_page_records++;
    total_page_record_size += page_record_size;
    memcpy(copy_location, &record_num, UNSIGNED_CHAR_SIZE);
    copy_location += UNSIGNED_CHAR_SIZE;

    // Page Record Current Page
    memcpy(copy_location, &vptr->cur_page->page_norm_id, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    // Page Record Session Index
    memcpy(copy_location, &vptr->sess_ptr->sess_norm_id, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    // Page Record Session Instance
    memcpy(copy_location, &vptr->sess_inst, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    // Page Record Current Tx
    // Changed by Anuj: 25/10/07
    // This will work for single concurrent Transaction only
    cur_tx = tx_get_cur_tx_hash_code(vptr);     //Anuj 28/11/07
    memcpy(copy_location, &cur_tx, UNSIGNED_INT_SIZE); // Anil - How to get cur_tx here?
    copy_location += UNSIGNED_INT_SIZE;

    // Page Record Child Id
    memcpy(copy_location, &child_idx, SHORT_SIZE);
    copy_location += SHORT_SIZE;

    // Page Record Tx Instance
    memcpy(copy_location, &vptr->tx_instance, SHORT_SIZE);
    copy_location += SHORT_SIZE;

    // Page Record Page Instance
    memcpy(copy_location, &vptr->page_instance, SHORT_SIZE);
    copy_location += SHORT_SIZE;

    // Page Record Page Begin at
    //Here were are converting this into abs miliseconds as we need download in miliseconds
    abs_time_in_milisec = g_time_diff_bw_cav_epoch_and_ns_start_milisec + vptr->pg_begin_at;
    memcpy(copy_location, &abs_time_in_milisec, NS_TIME_DATA_SIZE);
    copy_location += NS_TIME_DATA_SIZE;

    // Page Record Page end at
    if(!runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_rbu) // Non RBU
    {
      if (vptr->pg_begin_at > now) { /* Safety net to saveguard against start time > now */
        abs_time_in_milisec = g_time_diff_bw_cav_epoch_and_ns_start_milisec + vptr->pg_begin_at;
        error_log("Start time is more than now (%u). vptr=> %s", now,
                  vptr_to_string(vptr, vptr_to_str, MAX_LINE_LENGTH));
      } else {
        abs_time_in_milisec = g_time_diff_bw_cav_epoch_and_ns_start_milisec + now; 
      }
    }
    else
    {
      abs_time_in_milisec = ((g_time_diff_bw_cav_epoch_and_ns_start_milisec + vptr->pg_begin_at) + ((vptr->httpData->rbu_resp_attr->on_load_time != -1)?vptr->httpData->rbu_resp_attr->on_load_time:0));
    }

    memcpy(copy_location, &abs_time_in_milisec, NS_TIME_DATA_SIZE);
    copy_location += NS_TIME_DATA_SIZE;

    // Page Record Page Status
    memcpy(copy_location, &vptr->page_status, UNSIGNED_CHAR_SIZE);
    copy_location += UNSIGNED_CHAR_SIZE;
    // Session Record Session Status
    short phase_num;
    SET_PHASE_NUM(phase_num);
    memcpy(copy_location, &phase_num, SHORT_SIZE);
    //copy_location += SHORT_SIZE;
  } else
    return -1;
  return 0;
}

#endif

// This function record the page dump log
int log_page_dump_record(VUser* vptr, int child_index, u_ns_ts_t page_start_time, u_ns_ts_t page_end_time, int sess_instance, int sess_index, int page_index, int page_instance, char* parameter, short parameter_size, int page_status, char *flow_name, int log_file_sfx_size, char *log_file, int res_body_orig_name_size, char *res_body_orig_name, char *page_name, int page_response_time, int future1, int txName_len, char *txName, int fetched_param_len, char *fetched_param)
{
  char* copy_location;
  char cv_fail_msg_buf[4096+1]; //Buffer used to store CVfail checkpoint msg.
  char *cv_fail_encode_err_msg = 0;
  int nvm_id, gen_id = -1;
  unsigned char record_num = PAGE_DUMP_RECORD;
  int flow_name_size = strlen(flow_name);
  int cv_fail_size = 0;
  PageTableEntry_Shr* cur_page = &page_table_shr_mem[page_index];
  PerPageChkPtTableEntry_Shr *check_pt_fail_ptr = NULL;//Is used to access check point table.

  NSDL1_LOGGING(vptr, NULL, "Method called, page_status = %d, page_name = %s", page_status, page_name);
  if(page_status == NS_REQUEST_CV_FAILURE) //condition for CVfail.
  {
    if(vptr->httpData->check_pt_fail_start >= 0)
    {
      NSDL1_LOGGING(vptr, NULL, "first checkpoint have CVFail condition at index = %hi for page = %s ",
                                 vptr->httpData->check_pt_fail_start, page_name);
      check_pt_fail_ptr = cur_page->first_checkpoint_ptr + vptr->httpData->check_pt_fail_start;
    } 
  
    if(check_pt_fail_ptr)
    {
      /*Bug 36321: As parameter is already using ns_nvm_scratch_buf hence taking local buffer checkpoint_msg*/
      char checkpoint_msg[16*1024] = {0};
      /* Read checkpoint which is applied in registration.spec and stores it's value in scratch buffer. */
      checkpoint_to_str(check_pt_fail_ptr->checkpoint_ptr, checkpoint_msg);
      cv_fail_size = snprintf(cv_fail_msg_buf, 4096+1, "%s Page failed with status %s due to checkpoint (%s) failure.", checkpoint_msg,
                              get_error_code_name(NS_REQUEST_CV_FAILURE), ns_eval_string(check_pt_fail_ptr->checkpoint_ptr->id));
      if(cv_fail_size > 4096)
        cv_fail_size = 4096;
    
      //Bug 35799: Issue with dbupload, so encoding to html format, browser automatic decode the same
      cv_fail_encode_err_msg = (char *)nslb_encode_html(cv_fail_msg_buf, cv_fail_size, NULL);
      cv_fail_size = strlen(cv_fail_encode_err_msg);
      cv_fail_encode_err_msg[cv_fail_size] = 0;
      NSDL2_LOGGING(NULL, NULL, "cv_fail_msg_buf = %s, cv_fail_size = %d, cv_fail_encode_err_msg = %s",
                                 cv_fail_msg_buf, cv_fail_size, cv_fail_encode_err_msg);
    }
  } 
  
  int req_size = page_dump_record_size + parameter_size + flow_name_size + log_file_sfx_size + res_body_orig_name_size + txName_len + fetched_param_len + cv_fail_size;
  u_ns_ts_t abs_time_in_milisec = 0;
  RunProfTableEntry_Shr *rpf_ptr = &(runprof_table_shr_mem[vptr->group_num]);

  NSDL1_LOGGING(vptr, NULL, "page dump header size = %d, page dump with parameters size = %d, flow_name_size = %d, log_file_sfx_size = %d, res_body_orig_name_size = %d, page_response_time = %u, txName_len = %d, fetched_param_len = %d, cv_fail_size = %d", page_dump_record_size, parameter_size, flow_name_size, log_file_sfx_size, res_body_orig_name_size, page_response_time, txName_len, fetched_param_len, cv_fail_size);
  NSDL1_LOGGING(vptr, NULL, "total_page_dump_records = %d,  total_page_dump_record_size = %d, page_start_time = %u, page_end_time = %u", total_page_dump_records, total_page_dump_record_size, page_start_time, page_end_time);

  if (req_size > log_shr_buffer_size) 
  { 
    NSDL1_LOGGING(NULL, NULL, "Requested size = %d for page dump is more than shared memory block size = %d. Data will be lost", req_size, log_shr_buffer_size);
    NS_EL_2_ATTR(EID_LOGGING_BUFFER,  -1, -1, EVENT_CORE, EVENT_MAJOR,
                   __FILE__, (char*)__FUNCTION__,
                   "Requested size = %d is more than shared memory block size = %d. Hence ignoring this page %s:%s from logging", req_size, log_shr_buffer_size, page_name, flow_name);
    return 0;
  }
  #ifndef CAV_MAIN
  if ((copy_location = get_mem_ptr(req_size))) 
  {
    total_page_dump_records++;
    total_page_dump_record_size += req_size;

    memcpy(copy_location, &record_num, UNSIGNED_CHAR_SIZE);
    copy_location += UNSIGNED_CHAR_SIZE;

    //page dump url record start time
    abs_time_in_milisec = g_time_diff_bw_cav_epoch_and_ns_start_milisec + page_start_time;
    memcpy(copy_location, &abs_time_in_milisec, NS_TIME_DATA_SIZE);
    copy_location += NS_TIME_DATA_SIZE;
  
    //page dump url record page end time
    abs_time_in_milisec = g_time_diff_bw_cav_epoch_and_ns_start_milisec + page_end_time; 
    memcpy(copy_location, &abs_time_in_milisec, NS_TIME_DATA_SIZE);
    copy_location += NS_TIME_DATA_SIZE;

    //page dump url record Generator Id.
    if (send_events_to_master == 1) //In case of NC, calculate generator id whereas in case of standalone send -1
      gen_id = (int)((child_index & 0xFF00) >> 8);
    memcpy(copy_location, &gen_id, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;
    
    //page dump url record NVM Id
    nvm_id = (int)(child_index & 0x00FF);
    memcpy(copy_location, &nvm_id, SHORT_SIZE);
    copy_location += SHORT_SIZE;

    //page dump url record User Id.
    memcpy(copy_location, &vptr->user_index, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //page dump url record Session Inst
    memcpy(copy_location, &sess_instance, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //page dump url record Page Id
    memcpy(copy_location, &page_index, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //page dump url record Page Inst.
    memcpy(copy_location, &page_instance, SHORT_SIZE);
    copy_location += SHORT_SIZE;

    //page dump url record Page Status
    memcpy(copy_location, &page_status, SHORT_SIZE);
    copy_location += SHORT_SIZE;

    //page dump url record Page response time
    memcpy(copy_location, &page_response_time, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //page dump url record Group Id
    memcpy(copy_location, &(rpf_ptr->grp_norm_id), UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //page dump url record Session Id
    memcpy(copy_location, &sess_index, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;
  
    //page dump url record Partition.
    memcpy(copy_location, &vptr->partition_idx, UNSIGNED_LONG_SIZE);
    copy_location += UNSIGNED_LONG_SIZE;

    //page dump url record Trace Level.
    memcpy(copy_location, &(rpf_ptr->gset.trace_level), SHORT_SIZE);
    copy_location += SHORT_SIZE;

    //Insert three future field 1
    memcpy(copy_location, &future1, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //TxName 
    memcpy(copy_location, &txName_len, SHORT_SIZE);
    copy_location += SHORT_SIZE;

    //Insert TxName
    if(txName_len > 0){
      memcpy(copy_location, txName, txName_len);
      copy_location += txName_len;
    }

    //Insert fetched param
    memcpy(copy_location, &fetched_param_len, SHORT_SIZE);
    copy_location += SHORT_SIZE;

    //Insert fetched param
    if (fetched_param_len > 0) {
      memcpy(copy_location, fetched_param, fetched_param_len);
      copy_location += fetched_param_len;
    }

    //page dump url record flow name Size.
    memcpy(copy_location, &flow_name_size, UNSIGNED_CHAR_SIZE);
    copy_location += UNSIGNED_CHAR_SIZE;
 
    //page dump flow name
    if (flow_name_size > 0) {
      memcpy(copy_location, flow_name, flow_name_size);
      copy_location += flow_name_size;
    } 

    //page dump url log file size.
    memcpy(copy_location, &log_file_sfx_size, SHORT_SIZE);
    copy_location += SHORT_SIZE;

    //page dump url log file.
    if (log_file_sfx_size > 0) {
      memcpy(copy_location, log_file, log_file_sfx_size);
      copy_location += log_file_sfx_size;
    }

    //page dump url res body orig name size.
    memcpy(copy_location, &res_body_orig_name_size, SHORT_SIZE);
    copy_location += SHORT_SIZE;

    //page dump url response body orig name.
    if (res_body_orig_name_size > 0) {
      memcpy(copy_location, res_body_orig_name, res_body_orig_name_size);
      copy_location += res_body_orig_name_size;
    }

    //page dump url record Parameter Size.
    memcpy(copy_location, &parameter_size, SHORT_SIZE);
    copy_location += SHORT_SIZE;
 
    //page dump url record Parameter.
    if (parameter_size > 0) {
      memcpy(copy_location, parameter, parameter_size);
      copy_location += parameter_size;
    }

    //page dump CVfail check point.
    memcpy(copy_location, &cv_fail_size, SHORT_SIZE);
    copy_location += SHORT_SIZE;

    //page dump record CVfail msg buff.
    if(cv_fail_size > 0) {
      memcpy(copy_location, cv_fail_encode_err_msg, cv_fail_size);
      copy_location += cv_fail_size;
    }
  }
  #endif 
  else
    return -1;
  //Set flag to indicate at least one page dump is done of this session
  vptr->flags |= NS_VPTR_FLAGS_PAGE_DUMP_DONE;
  return 0; 
}

/*
  StartTime bigint,  Seconds

  DnsStartTime bigint,
  DnsEndTime bigint,
          DNSTime

  ConnectStartTime bigint,
  ConnectDoneTime bigint,
          ConnectTime

  SSLHandshakeDone bigint,
          SSLTime

  WriteCompleTime bigint,
          WriteTime

  FirstByteRcdTime bigint,
          FirstByteRcdTime

  RequestCompletedTime bigint,
          DownloadTime

  RenderingTime bigint, - Not used

  EndTime bigint,  Seconds
*/

int log_url_record(VUser* vptr, connection* cptr, unsigned char status, u_ns_ts_t now, int is_redirect, const int con_num, ns_8B_t flow_path_instance, int url_id) {
  char* copy_location;
  unsigned char record_num = URL_RECORD;
  short resp_code = cptr->req_code;
  unsigned char comp_mode = cptr->req_ok;
  unsigned char conn_type = cptr->connection_type;
  unsigned char bit_mask = 0;
  unsigned int zero_unsigned_long = 0;
  u_ns_ts_t abs_time_in_milisec = 0;

  /* 3.9.0 Changes:
   * Before connection pool was implemented, we were using cptr index as con_num to report in drill down 
   * Now we do not have cptr index as we are using pool. So we will use cptr->conn_fd
  */
  // const unsigned char con_num = (unsigned char) (cptr - (vptr->first_cptr));

  int cur_tx = -1; // Anuj: 25/10/07

  NSDL2_LOGGING(vptr, cptr, "Method called, child_idx = %hd, status = %d, now = %u, url_id=%d, cptr->dns_lookup_time=%d", child_idx, status, now, url_id, cptr->dns_lookup_time);
  //"Failed to copy to db, Error = ERROR:  value "4294967295" is out of range for type integer" - This error was coming in case url_id = -1,        hence adding a safety check and not logging data in db.
  if(url_id < 0)
  { 
    NSTL1(vptr, cptr, "Not logging url record as url_id = %d", url_id);
    // Not making url log on level 1 as url may contain special characters resulting in core dump
    NSTL2(vptr, cptr, "Url = %s", cptr->url);
    return -1;
  }

  int req_size = url_record_size + ((cptr->x_dynaTrace_data)?(cptr->x_dynaTrace_data->use_len):0);

  if(global_settings->monitor_type == HTTP_API)
  {
    char buffer[req_size + 1024 + 1];
    int length = 0;
    char cavtest_csv_file[512];
    int total_time = 0;
    int resolve_time = 0;
    int connection_time = 0;
    int ssl_handshake_time = 0;
    int send_time = 0;
    int first_byte_rcv_time = 0;
    int download_time = 0;
    int response_time = 0;
    int redirect_time = 0;
    int req_header_size = 0;
    int req_body_size = 0;
    int req_size = 0;
    int rep_header_size = 0;
    int rep_body_size = 0;
    int rep_size = 0;
    char page_error[32] = "";
    if(!vptr->page_status)
      snprintf(page_error, 32, "Success");
    else
      snprintf(page_error, 32, "%s", get_error_code_name(vptr->page_status));

    resolve_time = (cptr->dns_lookup_time > 0) ? cptr->dns_lookup_time : 0;
    connection_time = (cptr->connect_time > 0) ? cptr->connect_time : 0;
    ssl_handshake_time = (cptr->ssl_handshake_time > 0) ? cptr->ssl_handshake_time : 0;
    send_time = (cptr->write_complete_time > 0) ? cptr->write_complete_time : 0;
    first_byte_rcv_time = (cptr->first_byte_rcv_time > 0) ? cptr->first_byte_rcv_time : 0;
    download_time = (cptr->bytes > 0) ? cptr->request_complete_time : 0;
    if(vptr->page_status == NS_REQUEST_TIMEOUT)
      download_time = cptr->request_complete_time;

    total_time = resolve_time + connection_time + ssl_handshake_time + send_time + first_byte_rcv_time + download_time;

    NSDL2_LOGGING(vptr, cptr, "cptr->dns_lookup_time = %d, cptr->connect_time = %d, cptr->ssl_handshake_time = %d "
                              "cptr->write_complete_time = %d, cptr->first_byte_rcv_time = %d, DownloadTime = %d, "
                              "cptr->request_complete_time = %d, total_time = %d",
                               cptr->dns_lookup_time, cptr->connect_time, cptr->ssl_handshake_time,
                               cptr->write_complete_time, cptr->first_byte_rcv_time, download_time,
                               cptr->request_complete_time, total_time);

    req_size = (cptr->tcp_bytes_sent > 0) ? cptr->tcp_bytes_sent : 0;
    req_body_size = (cptr->http_payload_sent > 0) ? cptr->http_payload_sent : 0;
    req_header_size = req_size - req_body_size;
    rep_size = (cptr->tcp_bytes_recv > 0) ? cptr->tcp_bytes_recv : 0;
    rep_header_size = (cptr->bytes > 0) ? cptr->bytes : 0;
    rep_body_size = rep_size - rep_header_size;

    if(!runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_rbu)
      response_time = cptr->ns_component_start_time_stamp - cptr->started_at;
    else
      response_time = (vptr->httpData->rbu_resp_attr->on_load_time != -1)?vptr->httpData->rbu_resp_attr->on_load_time:0;

    abs_time_in_milisec = g_time_diff_bw_cav_epoch_and_ns_start_milisec + vptr->started_at;
    //MonitorId(text),Tier(text),Server(text),MonitorName(text),Type(text),StartTimeStamp,PartitionId(number),PageName(text),PageStatus(smallInt),RemoteServer(IP),UrlName(text),UrlStatus(number),URLStatusString(text),TotalTime(ms),ResolveTime(ms),ConnectionTime(ms),SSLHandShakeTime(ms),SendTime(ms),FirstByteTime(ms),DownloadTime(ms),ResponseTime(ms),RedirectTime(ms),RequestHeaderSize(byte),RequestBodySize(byte),RequestSize(byte),ResponseHeaderSize(byte),ResponseBodySize(byte),ResponseSize(byte),PageErrorMsg(text)
    length = snprintf(buffer, req_size + 1024, "%s,%s,%s,%s,httpService,%lld,%lld,%s,%d,%s,%s,%d,%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                                               "%d,%d,%d,%s\n",
               vptr->sess_ptr->sess_name, global_settings->cavtest_tier,global_settings->cavtest_server, global_settings->monitor_name,
               abs_time_in_milisec, global_settings->cavtest_partition_idx, vptr->cur_page->page_name, !vptr->page_status,
               nslb_sockaddr_to_ip((struct sockaddr *)&cptr->cur_server, 1), cptr->url, cptr->req_code, get_http_status_text(cptr->req_code), 
               total_time, resolve_time, connection_time, ssl_handshake_time, send_time, first_byte_rcv_time, download_time, response_time,
               redirect_time, req_header_size, req_body_size, req_size, rep_header_size, rep_body_size, rep_size, page_error);

    NSDL2_LOGGING(vptr, cptr, "HTTP API DATA: %s", buffer);

    snprintf(cavtest_csv_file, 512, "TR%s/%lld/reports/csv/%s", g_controller_testrun, global_settings->cavtest_partition_idx, SM_HTTP_API_CSV);
    ns_file_upload(cavtest_csv_file, buffer, length);
  }
  
  #ifndef CAV_MAIN
  if ((copy_location = get_mem_ptr(req_size))) {
    total_url_records++;
    total_url_record_size += url_record_size;
    memcpy(copy_location, &record_num, UNSIGNED_CHAR_SIZE);
    copy_location += UNSIGNED_CHAR_SIZE;

    //X-dynatrace header sting
    int size = cptr->x_dynaTrace_data?cptr->x_dynaTrace_data->use_len:0; 
    memcpy(copy_location, &size, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    if(cptr->x_dynaTrace_data)
    {
      if(cptr->x_dynaTrace_data->use_len > 0){
        memcpy(copy_location, cptr->x_dynaTrace_data->buffer, cptr->x_dynaTrace_data->use_len);
        copy_location += cptr->x_dynaTrace_data->use_len;
      }
      FREE_AND_MAKE_NULL_EX(cptr->x_dynaTrace_data->buffer, cptr->x_dynaTrace_data->len, "Freeing x_dynaTrace_data buffer", -1);
      cptr->x_dynaTrace_data->len = 0;
      FREE_AND_MAKE_NULL_EX(cptr->x_dynaTrace_data, sizeof(x_dynaTrace_data_t), "Freeing x_dynaTrace_data", -1);
    }

    // Url Record Child Id
    memcpy(copy_location, &child_idx, SHORT_SIZE);
    copy_location += SHORT_SIZE;
    // Url Record Url num index
    //
    //memcpy(copy_location, &cptr->url_num->proto.http.url_index, UNSIGNED_INT_SIZE);
    memcpy(copy_location, &url_id, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    // Url Record Session index
    memcpy(copy_location, &vptr->sess_ptr->sess_norm_id, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;
    NSDL2_LOGGING(vptr, cptr, "vptr->sess_ptr->sess_norm_id = %u", vptr->sess_ptr->sess_norm_id);

    // Url Record Session Instance
    memcpy(copy_location, &vptr->sess_inst, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    // Url Record Current Tx
    cur_tx = tx_get_cur_tx_hash_code(vptr);   //Anuj 28/11/07
    memcpy(copy_location, &cur_tx, UNSIGNED_INT_SIZE); // Anil - How do we get cur_tx???
    copy_location += UNSIGNED_INT_SIZE;

    // Url Record Current Page
    memcpy(copy_location, &vptr->cur_page->page_norm_id, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;
    NSDL2_LOGGING(vptr, cptr, "vptr->cur_page->page_norm_id = %u", vptr->cur_page->page_norm_id);


    // Url Record Tx Instance
    memcpy(copy_location, &vptr->tx_instance, SHORT_SIZE);
    copy_location += SHORT_SIZE;

    // Url Record Page Instance
    memcpy(copy_location, &vptr->page_instance, SHORT_SIZE);
    copy_location += SHORT_SIZE;

    // Bits
    //   0 - Not Used
    //   1 - Not Used
    //   2 - SSL Reused
    //   3 - Connection Resued
    //   4 - Main Url
    //   5 - Embed Url
    //       If both bit 4 and 5 are set, then it is redirect URL
    //   6 - Not Used
    //   7 - Not Used
    //
    
    //NSDL4_LOGGING(NULL, NULL, " value of cur_ssl_reuse = %d", cptr->cur_ssl_reuse);

    if(cptr->flags & NS_CPTR_FLAGS_SSL_REUSED) // SSL reused
      bit_mask |= 0x04;
     
    // Connection reuse (takes up 4th bit). Use it as conn reuse
    if (cptr->con_init_time != cptr->started_at)
        bit_mask |= 0x08;     //TODO: AA -> Now how to get this ??
    
    // URL type (takes up 5th and 6th bit)
    /* Here we are logging embedded redirected url  as embedded url
       We are logging only main redirected url as redirected url
       We are doing this because in page component  detail page embedded redirected   URL comes  twice, so the average response time of page is not correct */

    if ((is_redirect) && (cptr->url_num->proto.http.type == MAIN_URL)) {
      bit_mask |= 0x30; 
    } else {
      switch (cptr->url_num->proto.http.type) {
      case MAIN_URL:
        bit_mask |= 0x10;
        break;
      case EMBEDDED_URL:
        bit_mask |= 0x20;
        break;
      case REDIRECT_URL: // This is in case of manual redirection and is always Main redirect URL
        bit_mask |= 0x30;
        break;
      }
    }

    memcpy(copy_location, &bit_mask, UNSIGNED_CHAR_SIZE);
    copy_location += UNSIGNED_CHAR_SIZE;

    // Url Record Url Started at
    abs_time_in_milisec = g_time_diff_bw_cav_epoch_and_ns_start_milisec + cptr->started_at;
    memcpy(copy_location, &abs_time_in_milisec, NS_TIME_DATA_SIZE);
    copy_location += NS_TIME_DATA_SIZE;

    NSDL2_LOGGING(vptr, cptr, "abs_time_in_milisec = %llu, g_time_diff_bw_cav_epoch_and_ns_start_milisec = %llu, started_at = %llu", abs_time_in_milisec, (u_ns_ts_t)g_time_diff_bw_cav_epoch_and_ns_start_milisec, cptr->started_at);

    // DNS Resolution time
    memcpy(copy_location, &cptr->dns_lookup_time, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    // Url Record Connect time
    memcpy(copy_location, &cptr->connect_time, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    // Url Record SSL Handshake done time
    memcpy(copy_location, &cptr->ssl_handshake_time, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    // Url Record Write Complete time
    memcpy(copy_location, &cptr->write_complete_time, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    // Url Record First byte recieve time
    memcpy(copy_location, &cptr->first_byte_rcv_time, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    // Url Record Request complete time (Download time)
    // Bug : 36489 | NS DDR - In case of connection fail, time is shown in content download time
    // In this, for failure case also, total time taken by NS is added to request_complete_time, 
    // Due to which in DDR content download has some value in failure also.
    NSDL2_LOGGING(vptr, cptr, "cptr->bytes = %d", cptr->bytes);
    if(cptr->bytes > 0) {
      NSDL2_LOGGING(vptr, cptr, "cptr->request_complete_time = %d", cptr->request_complete_time);
      memcpy(copy_location, &cptr->request_complete_time,  UNSIGNED_INT_SIZE);
      copy_location +=  UNSIGNED_INT_SIZE;
    } else {
      NSDL2_LOGGING(vptr, cptr, "cptr->request_complete_time set as 0");
      memcpy(copy_location, &zero_unsigned_long, UNSIGNED_INT_SIZE);
      copy_location += UNSIGNED_INT_SIZE;
    }
    
    // Url Record this zero copy is for Rendering time
    memcpy(copy_location, &zero_unsigned_long, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    // Url End time
    if (cptr->started_at > cptr->ns_component_start_time_stamp) { /* Safety net to saveguard against start time > now */
      abs_time_in_milisec = g_time_diff_bw_cav_epoch_and_ns_start_milisec + cptr->started_at;
      error_log("Start time is more than End url time (%llu). vptr=> %s", cptr->ns_component_start_time_stamp, vptr_to_string(vptr, vptr_to_str, MAX_LINE_LENGTH));
    } else {
      abs_time_in_milisec = g_time_diff_bw_cav_epoch_and_ns_start_milisec + cptr->ns_component_start_time_stamp;
    }
    memcpy(copy_location, &abs_time_in_milisec, NS_TIME_DATA_SIZE);
    copy_location += NS_TIME_DATA_SIZE;

    // Url Record Response code
    memcpy(copy_location, &resp_code, SHORT_SIZE);
    copy_location += SHORT_SIZE;

    // Url Record Http Payload sent
    memcpy(copy_location, &cptr->http_payload_sent, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    // Url Record Tcp bytes sent
    memcpy(copy_location, &cptr->tcp_bytes_sent, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    // Url Record this zero copy is for ethernet bytes sent
    memcpy(copy_location, &zero_int, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    // Url Record cptr bytes
    memcpy(copy_location, &cptr->bytes, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    // Url Record Tcp bytes recieve
    memcpy(copy_location, &cptr->tcp_bytes_recv, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    // Url Record this zero copy is for ethernet bytes recv
    memcpy(copy_location, &zero_int, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    // Url Record Compression mode
    memcpy(copy_location, &comp_mode, UNSIGNED_CHAR_SIZE);
    copy_location += UNSIGNED_CHAR_SIZE;

    // Url Record Status 
    memcpy(copy_location, &status, UNSIGNED_CHAR_SIZE);
    copy_location += UNSIGNED_CHAR_SIZE;

    // Url Record Num connection
    // this zero copy is for content verification code
    // Use this place for connection number
    memcpy(copy_location, &con_num, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    // Url Record Connection type
    memcpy(copy_location, &conn_type, UNSIGNED_CHAR_SIZE);
    copy_location += UNSIGNED_CHAR_SIZE;

    // Url Record this zero copy is for retries
    memcpy(copy_location, &zero_unsigned_char, UNSIGNED_CHAR_SIZE);
    copy_location += UNSIGNED_CHAR_SIZE;

    // Url Record flow path instance
    memcpy(copy_location, &flow_path_instance, UNSIGNED_LONG_SIZE);
    copy_location += UNSIGNED_LONG_SIZE;
    // Session Record Session Status
    short phase_num;
    SET_PHASE_NUM(phase_num);
    memcpy(copy_location, &phase_num, SHORT_SIZE);
    copy_location += SHORT_SIZE;
    
  }
  else
    return -1;
  #endif

  return 0;
}

inline int log_page_rbu_detail_record(VUser* vptr, char *har_file_name, u_ns_ts_t har_file_date_time, int speed_index, char *cookies_cav_nv , RBU_RespAttr *rbu_resp_attr, char *profile_name)
{
  unsigned char record_num = RBU_PAGE_DETAIL_RECORD;
  //char group_name[256] = ""; 
  int nvm_id, gen_id = -1; 
  //int group_num = -1; 
  int screen_height = -1; 
  int screen_weidth = -1; 
  int dom_content_time = -1; 
  int on_load_time = -1; 
  int page_load_time = -1; 
  int start_render_time = -1; 
  int time_to_interact = -1; 
  int visually_complete_time = -1; 
  int req_server_count = -1; 
  int req_browser_cache_count = -1; 
  int req_text_type_count = -1; 
  int req_text_type_size = -1; 
  int req_other_type_count = -1;
  int req_other_type_cum_size = -1;
  int req_count_before_dom_content = -1; 
  int req_count_before_on_load = -1; 
  int req_count_before_start_rendering = -1;
  int browser_cached_req_count_before_dom_content = -1; 
  int browser_cached_req_count_before_on_load = -1; 
  int browser_cached_req_count_before_start_rendering = -1; 
  int req_js_type_count = -1; 
  int req_js_type_cum_size = -1; 
  int req_css_type_count = -1; 
  int req_css_type_cum_size = -1; 
  int req_image_type_count = -1; 
  int req_image_type_cum_size = -1; 
  int browser_type = -1; 
  int bytes_recieved = -1; 
  int bytes_sent = -1; 
  int host_id = -1; 
  u_ns_ts_t partition_id = 0; 
  u_ns_ts_t main_url_start_date_time = 0; 
  char* copy_location;
  PerHostSvrTableEntry_Shr *svr_entry;
  int pg_status = -1; 
  unsigned int sess_inst = 0; 
  short int performance_trace_mode = 0; 
  int dom_element = -1; 
  int main_url_resp_time = -1; 
  int byte_rcvd_before_dom_content = -1;
  int byte_rcvd_before_on_load = -1;
  unsigned int page_norm_id = 0;
  RunProfTableEntry_Shr *rpf_ptr = &(runprof_table_shr_mem[vptr->group_num]);
  int first_paint;
  int first_contentful_paint;
  int largest_contentful_paint;
  int total_blocking_time;
  char cumulative_layout_shift[8];

  if(rbu_resp_attr == NULL)
  {
    NSDL2_RBU(NULL, NULL,"G_RBU Enabled");
    rbu_resp_attr = vptr->httpData->rbu_resp_attr;
    page_norm_id = vptr->cur_page->page_norm_id;
  }
  else
    page_norm_id = rbu_resp_attr->page_norm_id;

  NSDL2_RBU(NULL, NULL,"vptr->partition_idx = %lld", vptr->partition_idx);

  if(vptr != NULL)
  {
    partition_id = (vptr->partition_idx ? vptr->partition_idx : g_partition_idx );
    //group_num = vptr->group_num; 
    screen_height = vptr->screen_size->height;
    screen_weidth = vptr->screen_size->width;
    dom_content_time = rbu_resp_attr->on_content_load_time;
    on_load_time = rbu_resp_attr->on_load_time;
    page_load_time = rbu_resp_attr->page_load_time;
    start_render_time = rbu_resp_attr->_cav_start_render_time;
    time_to_interact = rbu_resp_attr->_tti_time;
    visually_complete_time = rbu_resp_attr->_cav_end_render_time;
    req_server_count = rbu_resp_attr->request_without_cache;
    req_browser_cache_count = rbu_resp_attr->request_from_cache;
 
    req_text_type_count = rbu_resp_attr->resp_html_count;
    req_text_type_size = rbu_resp_attr->resp_html_size; 
    req_other_type_count = rbu_resp_attr->resp_other_count;
    req_other_type_cum_size = rbu_resp_attr->resp_other_size;
    main_url_start_date_time = rbu_resp_attr->main_url_start_date_time;
    req_count_before_dom_content = rbu_resp_attr->req_bfr_DOM;
    req_count_before_on_load = rbu_resp_attr->req_bfr_OnLoad;
    req_count_before_start_rendering = rbu_resp_attr->req_bfr_Start_render;
    browser_cached_req_count_before_dom_content = rbu_resp_attr->req_frm_browser_cache_bfr_DOM;
    browser_cached_req_count_before_on_load = rbu_resp_attr->req_frm_browser_cache_bfr_OnLoad;
    browser_cached_req_count_before_start_rendering = rbu_resp_attr->req_frm_browser_cache_bfr_Start_render; 

    req_js_type_count = rbu_resp_attr->resp_js_count;
    req_js_type_cum_size = rbu_resp_attr->resp_js_size;
    req_css_type_count = rbu_resp_attr->resp_css_count;
    req_css_type_cum_size = rbu_resp_attr->resp_css_size;
    req_image_type_count = rbu_resp_attr->resp_img_count;
    req_image_type_cum_size = rbu_resp_attr->resp_img_size;

    bytes_recieved = rbu_resp_attr->byte_rcvd;
    bytes_sent = rbu_resp_attr->byte_send;
    browser_type = rpf_ptr->gset.rbu_gset.browser_mode;

    svr_entry = get_svr_entry(vptr, vptr->last_cptr->url_num->index.svr_ptr);
    host_id = svr_entry->host_id;

    //Setting page status according to 'rbu_resp_attr->cv_status' which will be of same convention as that of Netstorm Status/Error Code
    pg_status = rbu_resp_attr->cv_status;         

    sess_inst = vptr->sess_inst; 
    //strcpy(group_name, runprof_table_shr_mem[vptr->group_num].scen_group_name);

    performance_trace_mode = rbu_resp_attr->performance_trace_flag;
    dom_element = rbu_resp_attr->dom_element;
    main_url_resp_time = rbu_resp_attr->main_url_resp_time;
    byte_rcvd_before_dom_content = rbu_resp_attr->byte_rcvd_bfr_DOM;
    byte_rcvd_before_on_load = rbu_resp_attr->byte_rcvd_bfr_OnLoad;

    first_paint = rbu_resp_attr->first_paint;
    first_contentful_paint = rbu_resp_attr->first_contentful_paint;
    largest_contentful_paint = rbu_resp_attr->largest_contentful_paint;
    total_blocking_time = rbu_resp_attr->total_blocking_time;
    snprintf(cumulative_layout_shift, 8, "%.3f", rbu_resp_attr->cum_layout_shift);

    NSDL3_LOGGING(NULL, NULL, " server_name = %s, host_id is = %d", svr_entry->server_name, host_id);
  }

  NSDL1_LOGGING(NULL, NULL, "Method Called, child_idx = %hd, partition_id = %lld, group_num = %d, "
                            "screen_height = %d, screen_weidth = %d, dom_content_time = %d, on_load_time = %d, page_load_time = %d, "
                            "start_render_time = %d, time_to_interact = %d, visually_complete_time = %d, req_server_count = %d, "
                            "req_browser_cache_count = %d, bytes_recieved = %d, bytes_sent = %d, browser_type = %d, "
                            "req_js_type_count = %d, req_js_type_cum_size = %d, req_css_type_count = %d, req_css_type_cum_size = %d, "
                            "req_image_type_count = %d, req_image_type_cum_size = %d, main_url_start_date_time = %lld, "
                            "req_count_before_dom_content = %d, req_count_before_on_load = %d, "
                            "browser_cached_req_count_before_dom_content = %d, browser_cached_req_count_before_on_load = %d, "
                            "req_text_type_count = %d, req_text_type_size %d, req_other_type_count = %d, req_other_type_cum_size = %d, "
                            "host_id = %d, req_count_before_start_rendering = %d, browser_cached_req_count_before_start_rendering = %d, "
                            "har_file_date_time = %lld, speed_index = %d, cookies_cav_nv = %s, har_file_name = %s, group_name = %s, "
                            "profile_name = %s, page_norm_id = %u, page_status = %d, sess_inst = %u, device_info = %s, "
                            "performance_trace_mode = %d, dom_element = %d, main_url_resp_time =%d, byte_rcvd_before_dom_content = %d, "
                            "byte_rcvd_before_on_load = %d, first_paint = %d, first_contentful_paint = %d, largest_contentful_paint =%d, "
                            "total_blocking_time = %d, cumulative_layout_shift = %s",
                            child_idx, partition_id, rpf_ptr->grp_norm_id, screen_height, screen_weidth, dom_content_time, 
                            on_load_time, page_load_time, start_render_time, time_to_interact, visually_complete_time, req_server_count, 
                            req_browser_cache_count, bytes_recieved, bytes_sent, browser_type, req_js_type_count, req_js_type_cum_size, 
                            req_css_type_count, req_css_type_cum_size, req_image_type_count, req_image_type_cum_size, 
                            main_url_start_date_time, req_count_before_dom_content, req_count_before_on_load, 
                            browser_cached_req_count_before_dom_content, browser_cached_req_count_before_on_load, req_text_type_count,
                            req_text_type_size, req_other_type_count, req_other_type_cum_size, host_id, req_count_before_start_rendering,
                            browser_cached_req_count_before_start_rendering, har_file_date_time, speed_index, cookies_cav_nv, har_file_name,
                            rpf_ptr->scen_group_name, profile_name, page_norm_id, pg_status, sess_inst, rbu_resp_attr->dvc_info,
                            performance_trace_mode, dom_element, main_url_resp_time, byte_rcvd_before_dom_content, byte_rcvd_before_on_load,
                            first_paint, first_contentful_paint, largest_contentful_paint, total_blocking_time, cumulative_layout_shift);
 
  int har_file_name_size = strlen(har_file_name);
  int cookies_cav_nv_size = strlen(cookies_cav_nv);
  int group_name_size = strlen(rpf_ptr->scen_group_name);
  int profile_name_size = strlen(profile_name);
  int dvc_info_size = strlen(rbu_resp_attr->dvc_info);
  int cls_size = strlen(cumulative_layout_shift);
  int req_size = page_rbu_record_size + har_file_name_size + cookies_cav_nv_size + group_name_size + profile_name_size + dvc_info_size +
                 cls_size;

  if (req_size > log_shr_buffer_size) 
  { 
    NSDL1_LOGGING(NULL, NULL, "Requested size = %d for rbu page detail record is more than shared memory block size = %d. Data will be lost", req_size, log_shr_buffer_size);
    return 0;
  }
 
  if(global_settings->monitor_type == WEB_PAGE_AUDIT)
  {
    char buffer[1024 + 1];
    int length = 0;
    char cavtest_csv_file[512];
    int total_time = 0;
    int resolve_time = 0;
    int connection_time = 0;
    int ssl_handshake_time = 0; 
    int send_time = 0;
    int first_byte_rcv_time = 0;
    int download_time = 0;
    int response_time = 0;
    int redirect_time = 0;
    int req_header_size = 0;
    int req_body_size = 0;
    int req_size = 0;
    int rep_header_size = 0;
    int rep_body_size = 0;
    int rep_size = 0;
    int page_weight = 0;
    int jss_size = 0;
    int css_size = 0;
    int image_weight = 0;
    char page_error[32] = "";
    u_ns_ts_t abs_time_in_milisec = 0;

    if(!vptr->page_status)
      snprintf(page_error, 32, "Success");
    else
      snprintf(page_error, 32, "%s", get_error_code_name(vptr->page_status));

    resolve_time = (rbu_resp_attr->dns_time > 0) ? rbu_resp_attr->dns_time : 0;
    connection_time = (rbu_resp_attr->connect_time > 0) ? rbu_resp_attr->connect_time : 0;
    ssl_handshake_time = (rbu_resp_attr->ssl_time > 0) ? rbu_resp_attr->ssl_time : 0; 
    download_time = (rbu_resp_attr->rcv_time > 0) ? rbu_resp_attr->rcv_time : 0;
    response_time = (rbu_resp_attr->url_resp_time > 0) ? rbu_resp_attr->url_resp_time : 0;

    total_time = resolve_time + connection_time + ssl_handshake_time + send_time + first_byte_rcv_time + download_time;

    NSDL2_LOGGING(NULL, NULL, "total_time = %d, dns_time = %f, connect_time = %f, ssl_time = %f, rcv_time = %f",
                               total_time, rbu_resp_attr->dns_time, rbu_resp_attr->connect_time, rbu_resp_attr->ssl_time,
                               rbu_resp_attr->rcv_time);

    req_size = ((rbu_resp_attr->byte_send > 0) ? rbu_resp_attr->byte_send : 0) * 1024;
    req_body_size = ((rbu_resp_attr->req_body_size > 0) ? rbu_resp_attr->req_body_size : 0) * 1024;
    req_header_size = req_size - req_body_size;
    rep_size = (vptr->last_cptr->tcp_bytes_recv > 0) ? vptr->last_cptr->tcp_bytes_recv : 0;
    rep_body_size = ((rbu_resp_attr->byte_rcvd > 0) ? rbu_resp_attr->byte_rcvd : 0) * 1024;
    rep_header_size = rep_size - rep_body_size; //total response size - response body size
    page_weight = ((rbu_resp_attr->pg_wgt > 0) ? rbu_resp_attr->pg_wgt : 0) * 1024;
    jss_size = ((rbu_resp_attr->resp_js_size > 0) ? rbu_resp_attr->resp_js_size : 0) * 1024;
    css_size = ((rbu_resp_attr->resp_css_size > 0) ? rbu_resp_attr->resp_css_size : 0) * 1024;
    image_weight = ((rbu_resp_attr->resp_img_size > 0) ? rbu_resp_attr->resp_img_size : 0) * 1024;

    abs_time_in_milisec = g_time_diff_bw_cav_epoch_and_ns_start_milisec + vptr->started_at;

    //MonitorId(text),Tier(text),Server(text),MonitorName(text),Type(text),StartTimeStamp,PartitionId(number),PageName(text),PageStatus(smallInt),RemoteServer(IP),UrlName(text),UrlStatus(number),URLStatusString(text),TotalTime(ms),ResolveTime(ms),ConnectionTime(ms),SSLHandShakeTime(ms),SendTime(ms),FirstByteTime(ms),DownloadTime(ms),ResponseTime(ms),RedirectTime(ms),domTime(ms),onLoadTime(ms),overallTime(ms),timeToInteract(ms),startRenderTime(ms),visualCompleteTime(ms),requestHeaderSize(byte),RequestBodySize(byte),RequestSize(byte),ResponseHeaderSize(byte),ResponseBodySize(byte),ResponseSize(byte),pageWeight(byte),JSSize(byte),CSSSize(byte),ImageWeight(byte),DomElement(number),PageSpeedScore(number),PageErrorMsg(text)
    length = snprintf(buffer, 1024, "%s,%s,%s,%s,webpageAudit,%llu,%lld,%s,%d,%s,%s,%d,%s,"
             "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%s\n",
             vptr->sess_ptr->sess_name, global_settings->cavtest_tier, global_settings->cavtest_server, global_settings->monitor_name,
             abs_time_in_milisec, global_settings->cavtest_partition_idx, vptr->cur_page->page_name,
             !vptr->page_status, rbu_resp_attr->server_ip_add, rbu_resp_attr->url, rbu_resp_attr->status_code, rbu_resp_attr->status_text,
             total_time, resolve_time, connection_time, ssl_handshake_time, send_time, first_byte_rcv_time, download_time, response_time,
             redirect_time, dom_content_time, on_load_time, page_load_time, time_to_interact, start_render_time, visually_complete_time,
             req_header_size, req_body_size, req_size, rep_header_size, rep_body_size, rep_size,
             page_weight, jss_size, css_size, image_weight, dom_element, rbu_resp_attr->pg_speed, page_error); 

    NSDL2_LOGGING(vptr, NULL, "WEB PAGE AUDIT DATA: %s", buffer);

    snprintf(cavtest_csv_file, 512, "TR%s/%lld/reports/csv/%s", g_controller_testrun, global_settings->cavtest_partition_idx,
                                                                SM_WEB_PAGE_AUDIT_CSV);
    ns_file_upload(cavtest_csv_file, buffer, length);

    if(runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.enable_capture_clip == 1)
    {
      char path_snap_shot[256 + 1] = "";
      char dest_path[256 + 1] = "";
      char buf[128 + 1] = "";
      char cmd[512 + 1] = "";
      char *start_ptr = NULL;
      
      #ifndef CAV_MAIN
      snprintf(path_snap_shot, 256, "%s/logs/TR%d/%lld/rbu_logs/snap_shots/", g_ns_wdir, testidx, vptr->partition_idx);
      #else
      snprintf(path_snap_shot, 256, "%s/logs/TR%d/%s/rbu_logs/snap_shots/", g_ns_wdir, testidx, vptr->sess_ptr->sess_name);
      #endif
      snprintf(dest_path, 256, "TR%s/%lld/rbu_logs/snap_shots/%s", g_controller_testrun, global_settings->cavtest_partition_idx,
                                                                   vptr->sess_ptr->sess_name);

      snprintf(cmd, 512, "echo \"%s\" | awk -F '+' '{print $6}'", rbu_resp_attr->har_name);
      if(nslb_run_cmd_and_get_last_line(cmd, 128, buf) != 0) {
        NSTL1_OUT(NULL, NULL, "Error in running cmd = %s, exiting !\n", cmd);
        return -1;
      }
      NSDL1_RBU(vptr, NULL, "rbu_resp_attr->har_name = %s, cmd = %s, buf = %s", rbu_resp_attr->har_name, cmd, buf);
      if((start_ptr = strchr(buf, '.')) != NULL)
      {
        *start_ptr = '\0';
      } else {
        NSDL1_RBU(vptr, NULL, "Not found [.] in %s", rbu_resp_attr->har_name);
        return -1;
      }
      ns_upload_clips(path_snap_shot, dest_path, buf);
    } 
  }

  #ifndef CAV_MAIN
  if ((copy_location = get_mem_ptr(req_size)))
  {
    total_page_rbu_records++;
    total_page_rbu_record_size += req_size;

    memcpy(copy_location, &record_num, UNSIGNED_CHAR_SIZE);
    copy_location += UNSIGNED_CHAR_SIZE;

    //page rbu detail record partition_id
    memcpy(copy_location, &partition_id, UNSIGNED_LONG_SIZE);
    copy_location += UNSIGNED_LONG_SIZE;

    //page rbu detail record har_file_name
    memcpy(copy_location, &har_file_name_size, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    if(har_file_name_size > 0)
    {
      memcpy(copy_location, har_file_name, har_file_name_size);
      copy_location += har_file_name_size;
    }
 
    //page rbu detail record page norm id
    //memcpy(copy_location, &vptr->cur_page->page_norm_id, UNSIGNED_INT_SIZE);
    memcpy(copy_location, &page_norm_id, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE; 

    //page rbu detail record &sess_idx
    memcpy(copy_location, &vptr->sess_ptr->sess_norm_id, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;  

    //page rbu detail record nvm_id
    nvm_id = (int)(child_idx & 0x00FF);
    memcpy(copy_location, &nvm_id, SHORT_SIZE);
    copy_location += SHORT_SIZE;
 
    //page rbu detail record gen_id
    if (send_events_to_master == 1) //In case of NC, calculate generator id whereas in case of standalone send -1
      gen_id = (int)((child_idx & 0xFF00) >> 8);
    memcpy(copy_location, &gen_id, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //page rbu detail record group_num
    memcpy(copy_location, &(rpf_ptr->grp_norm_id), SHORT_SIZE);
    copy_location += SHORT_SIZE;

    //page rbu detail record host_id
    memcpy(copy_location, &host_id, SHORT_SIZE);
    copy_location += SHORT_SIZE;

    //page rbu detail record browser_type
    memcpy(copy_location, &browser_type, SHORT_SIZE);
    copy_location += SHORT_SIZE;

    //page rbu detail record screen_height
    memcpy(copy_location, &screen_height, SHORT_SIZE);
    copy_location += SHORT_SIZE;
 
    //page rbu detail record screen_weidth
    memcpy(copy_location, &screen_weidth, SHORT_SIZE);
    copy_location += SHORT_SIZE;
 
    //page rbu detail record dom_content_time
    memcpy(copy_location, &dom_content_time, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //page rbu detail record on_load_time
    memcpy(copy_location, &on_load_time, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE; 

    //page rbu detail record page_load_time
    memcpy(copy_location, &page_load_time, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //page rbu detail record start_render_time
    memcpy(copy_location, &start_render_time, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;
 
    //page rbu detail record time_to_interact
    memcpy(copy_location, &time_to_interact, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //page rbu detail record visually_complete_time
    memcpy(copy_location, &visually_complete_time, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //page rbu detail record req_server_count
    memcpy(copy_location, &req_server_count, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;
 
    //page rbu detail record req_browser_cache_count
    memcpy(copy_location, &req_browser_cache_count, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //page rbu detail record req_text_type_count
    memcpy(copy_location, &req_text_type_count, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //page rbu detail record req_text_type_size
    memcpy(copy_location, &req_text_type_size, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //page rbu detail record req_js_type_count
    memcpy(copy_location, &req_js_type_count, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //page rbu detail record &req_js_type_cum_size
    memcpy(copy_location, &req_js_type_cum_size, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //page rbu detail record req_css_type_count
    memcpy(copy_location, &req_css_type_count, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //page rbu detail record req_css_type_cum_size
    memcpy(copy_location, &req_css_type_cum_size, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;
   
    //page rbu detail record req_image_type_count
    memcpy(copy_location, &req_image_type_count, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //page rbu detail record req_image_type_cum_size
    memcpy(copy_location, &req_image_type_cum_size, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;
 
    //page rbu detail record req_other_type_count
    memcpy(copy_location, &req_other_type_count, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //page rbu detail record req_other_type_cum_size
    memcpy(copy_location, &req_other_type_cum_size, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //page rbu detail record bytes_recieved
    memcpy(copy_location, &bytes_recieved, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //page rbu detail record bytes_sent
    memcpy(copy_location, &bytes_sent, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //page rbu detail record speed_index
    memcpy(copy_location, &speed_index, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //page rbu detail record main_url_start_date_time
    memcpy(copy_location, &main_url_start_date_time, UNSIGNED_LONG_SIZE);
    copy_location += UNSIGNED_LONG_SIZE; 

    //page rbu detail record har_file_date_time
    memcpy(copy_location, &har_file_date_time, UNSIGNED_LONG_SIZE);
    copy_location += UNSIGNED_LONG_SIZE;
 
    //page rbu detail record req_count_before_dom_content
    memcpy(copy_location, &req_count_before_dom_content, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //page rbu detail record req_count_before_on_load
    memcpy(copy_location, &req_count_before_on_load, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //page rbu detail record req_count_before_start_rendering
    memcpy(copy_location, &req_count_before_start_rendering, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //page rbu detail record browser_cached_req_count_before_dom_content
    memcpy(copy_location, &browser_cached_req_count_before_dom_content, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //page rbu detail record browser_cached_req_count_before_on_load
    memcpy(copy_location, &browser_cached_req_count_before_on_load, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //page rbu detail record browser_cached_req_count_before_start_rendering
    memcpy(copy_location, &browser_cached_req_count_before_start_rendering, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //page rbu detail record cookies_cav_nv
    memcpy(copy_location, &cookies_cav_nv_size, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;
  
    if(cookies_cav_nv_size > 0)
    {
      memcpy(copy_location, cookies_cav_nv, cookies_cav_nv_size);
      copy_location += cookies_cav_nv_size;
    }
   
    //page rbu detail record group_name
    memcpy(copy_location, &group_name_size, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;
    if(group_name_size > 0)
    {
      memcpy(copy_location, rpf_ptr->scen_group_name, group_name_size);
      copy_location += group_name_size;
    }

    //page rbu detail record profile_name
    memcpy(copy_location, &profile_name_size, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;
    if(profile_name_size > 0)
    {
      memcpy(copy_location, profile_name, profile_name_size);
      copy_location += profile_name_size;
      //copy_location += profile_name_size;
    }

    //rbu page status
    memcpy(copy_location, &pg_status, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //session instance
    memcpy(copy_location, &sess_inst, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //page rbu detail record Device Information
    memcpy(copy_location, &dvc_info_size, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;
    if(dvc_info_size > 0)
    {
      memcpy(copy_location, rbu_resp_attr->dvc_info, dvc_info_size);
      copy_location += dvc_info_size;
    }

    //performance trace dump
    memcpy(copy_location, &performance_trace_mode, SHORT_SIZE);
    copy_location += SHORT_SIZE;  

    //DOM Elements
    memcpy(copy_location, &dom_element, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //Backend Response Time
    memcpy(copy_location, &main_url_resp_time, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //Byte Received Before DomContent Event Fired
    memcpy(copy_location, &byte_rcvd_before_dom_content, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //Byte Received Before OnLoad Event Fired
    memcpy(copy_location, &byte_rcvd_before_on_load, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //First Paint Time
    memcpy(copy_location, &first_paint, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //First Contentful Paint Time
    memcpy(copy_location, &first_contentful_paint, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //Largest Contentful Paint Time
    memcpy(copy_location, &largest_contentful_paint, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //Cumulative Layout Shift
    memcpy(copy_location, &cls_size, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;
    if(cls_size > 0)
    {
      memcpy(copy_location, cumulative_layout_shift, cls_size);
      copy_location += cls_size;
    }
 
    //Total Blocking Time
    memcpy(copy_location, &total_blocking_time, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

  } else
    return -1;
  #endif

  return 0;
}

//logging CSV for RBU mark/measure.
inline void log_rbu_mark_and_measure_record(VUser *vptr, RBU_RespAttr *rbu_resp_attr)
{
  unsigned char record_num = RBU_MARK_MEASURE_RECORD;
  int host_id = -1, i, mark_measures_count = rbu_resp_attr->total_mark_and_measures; 
  u_ns_ts_t absStartTime = 0;
  int nvm_id, gen_id = -1; 
  int record_size;
  short name_len;
  unsigned int sess_norm_id = 0; 
  unsigned int page_norm_id = 0;
  PerHostSvrTableEntry_Shr *svr_entry;
  RunProfTableEntry_Shr *rpf_ptr = &(runprof_table_shr_mem[vptr->group_num]);
  RBU_MarkMeasure *mark_and_measures = rbu_resp_attr->mark_and_measures;
  char *copy_location;
 
  page_norm_id = vptr->cur_page->page_norm_id;

  sess_norm_id = vptr->sess_ptr->sess_norm_id;

  nvm_id = (int)(child_idx & 0x00FF);

  if (send_events_to_master == 1) //In case of NC, calculate generator id whereas in case of standalone send -1
    gen_id = (int)((child_idx & 0xFF00) >> 8);

  svr_entry = get_svr_entry(vptr, vptr->last_cptr->url_num->index.svr_ptr);
  host_id = svr_entry->host_id;


  // Multiple records will be logged corresponding to one single page. 
  for (i = 0; i < mark_measures_count; i++) {
    // It will have both starttime, relativestarttime
    absStartTime = rbu_resp_attr->har_date_and_time + mark_and_measures[i].startTime; 
    
    name_len = (short) strlen(mark_and_measures[i].name);

    record_size = MARK_MEASURE_RECORD_SIZE + name_len; 

    if ((copy_location = get_mem_ptr(record_size))) {
      // It must be for logging.
      total_rbu_mark_measure_records ++;
      total_rbu_mark_mmeasure_record_size += record_size;

      memcpy(copy_location, &record_num, UNSIGNED_CHAR_SIZE);
      copy_location += UNSIGNED_CHAR_SIZE;

      memcpy(copy_location, &page_norm_id, UNSIGNED_INT_SIZE);
      copy_location += UNSIGNED_INT_SIZE; 

      memcpy(copy_location, &sess_norm_id, UNSIGNED_INT_SIZE);
      copy_location += UNSIGNED_INT_SIZE;  

      memcpy(copy_location, &nvm_id, SHORT_SIZE);
      copy_location += SHORT_SIZE;
      
      memcpy(copy_location, &gen_id, UNSIGNED_INT_SIZE);
      copy_location += UNSIGNED_INT_SIZE;

      //page rbu detail record group_num
      memcpy(copy_location, &(rpf_ptr->grp_norm_id), SHORT_SIZE);
      copy_location += SHORT_SIZE;

      memcpy(copy_location, &host_id, SHORT_SIZE);
      copy_location += SHORT_SIZE;

      //absStartTime
      memcpy(copy_location, &absStartTime, UNSIGNED_LONG_SIZE);
      copy_location += UNSIGNED_LONG_SIZE; 

      //name size. 
      memcpy(copy_location, &name_len, SHORT_SIZE);
      copy_location += SHORT_SIZE;

      //copy name. 
      memcpy(copy_location, mark_and_measures[i].name, name_len);
      copy_location += name_len;

      memcpy(copy_location, &mark_and_measures[i].type, UNSIGNED_CHAR_SIZE);
      copy_location += UNSIGNED_CHAR_SIZE;

      //start time
      memcpy(copy_location, &mark_and_measures[i].startTime, UNSIGNED_INT_SIZE);
      copy_location += UNSIGNED_INT_SIZE;

      //duration.
      memcpy(copy_location, &mark_and_measures[i].duration, UNSIGNED_INT_SIZE);
      copy_location += UNSIGNED_INT_SIZE;

      memcpy(copy_location, &vptr->sess_inst, UNSIGNED_INT_SIZE);
      copy_location += UNSIGNED_INT_SIZE;

      memcpy(copy_location, &vptr->page_instance, SHORT_SIZE);
      copy_location += SHORT_SIZE; 
    }
  } 
}

//Logging CSV of RBU LightHouse Datail Record
inline int log_rbu_light_house_detail_record(VUser *vptr, RBU_LightHouse *rbu_light_house)
{
  NSDL2_RBU(NULL, NULL,"Method Called");
  unsigned char record_num = RBU_LIGHTHOUSE_RECORD;
  char *lighthouse_filename = rbu_light_house->lighthouse_filename;
  int nvm_id, gen_id = -1; 
  int host_id = -1; 
  int browser_type = -1;
  int performance_score = -1;
  int pwa_score = -1;
  int accessibility_score = -1;
  int best_practice_score = -1;
  int seo_score = -1;
  int first_contentful_paint_time = -1;
  int first_meaningful_paint_time = -1;  
  int speed_index = -1;
  int first_CPU_idle = -1;
  int time_to_interact = -1;
  int input_latency = -1;
  int largest_contentful_paint;
  int total_blocking_time;
  char cumulative_layout_shift[8];
  u_ns_ts_t partition_id = 0; 
  u_ns_ts_t lh_file_date_time = 0; 

  char* copy_location;

  PerHostSvrTableEntry_Shr *svr_entry;
  RunProfTableEntry_Shr *rpf_ptr = &(runprof_table_shr_mem[vptr->group_num]);

  NSDL2_RBU(NULL, NULL,"vptr->partition_idx = %lld", vptr->partition_idx);

  if(vptr)
  {
    partition_id = (vptr->partition_idx ? vptr->partition_idx : g_partition_idx );
 
    if( vptr->last_cptr)
    {
      svr_entry = get_svr_entry(vptr, vptr->last_cptr->url_num->index.svr_ptr);
      host_id = svr_entry->host_id;
 
      NSDL3_LOGGING(NULL, NULL, " server_name = %s, host_id is = %d", svr_entry->server_name, host_id);
    }
 
    lh_file_date_time = rbu_light_house->lh_file_date_time;
    performance_score = rbu_light_house->performance_score;
    pwa_score = rbu_light_house->pwa_score;
    accessibility_score = rbu_light_house->accessibility_score;
    best_practice_score = rbu_light_house->best_practice_score;
    seo_score = rbu_light_house->seo_score;
    first_contentful_paint_time = rbu_light_house->first_contentful_paint_time;
    first_meaningful_paint_time = rbu_light_house->first_meaningful_paint_time;  
    speed_index = rbu_light_house->speed_index;
    first_CPU_idle = rbu_light_house->first_CPU_idle;
    time_to_interact = rbu_light_house->time_to_interact;
    input_latency = rbu_light_house->input_latency;
    largest_contentful_paint = rbu_light_house->largest_contentful_paint;
    total_blocking_time = rbu_light_house->total_blocking_time;
    snprintf(cumulative_layout_shift, 8, "%.3f", rbu_light_house->cum_layout_shift);

    browser_type = rpf_ptr->gset.rbu_gset.browser_mode;
  }

  NSDL1_LOGGING(NULL, NULL, "Method Called, child_idx = %hd, partition_id = %lld, group_num = %d, "
                            "performance_score = %d, pwa_score = %d, accessibility_score = %d, best_practice_score = %d, seo_score = %d, "
                            "first_contentful_paint = %d, first_meaningful_paint = %d, speed_index = %d, first_CPU_idle = %d, "
                            "time_to_interact = %d, input_latency = %d, largest_contentful_paint =%d, total_blocking_time = %d, "
                            "cumulative_layout_shift = %s",
                             child_idx, partition_id, rpf_ptr->grp_norm_id, performance_score, pwa_score, accessibility_score,
                             best_practice_score, seo_score, first_contentful_paint_time,
                             first_meaningful_paint_time, speed_index, first_CPU_idle, time_to_interact, input_latency,
                             largest_contentful_paint, total_blocking_time, cumulative_layout_shift);

  int lh_file_name_size = strlen(lighthouse_filename);
  int group_name_size = strlen(rpf_ptr->scen_group_name);
  int cls_size = strlen(cumulative_layout_shift);
  int req_size = rbu_lh_record_size + lh_file_name_size + group_name_size + cls_size;

  if (req_size > log_shr_buffer_size) 
  { 
    NSDL1_LOGGING(NULL, NULL, "Requested size = %d for rbu light-house detail record is more than shared memory block size = %d. Data will be lost", req_size, log_shr_buffer_size);
    return 0;
  }
 
  if ((copy_location = get_mem_ptr(req_size)))
  {
    total_rbu_lh_records++;
    total_rbu_lh_record_size += req_size;

    memcpy(copy_location, &record_num, UNSIGNED_CHAR_SIZE);
    copy_location += UNSIGNED_CHAR_SIZE;

    //rbu light-house record partition_id
    memcpy(copy_location, &partition_id, UNSIGNED_LONG_SIZE);
    copy_location += UNSIGNED_LONG_SIZE;

    //rbu light-house record LighthouseReportFileName
    memcpy(copy_location, &lh_file_name_size, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    if(lh_file_name_size > 0)
    {
      memcpy(copy_location, lighthouse_filename, lh_file_name_size);
      copy_location += lh_file_name_size;
    }
 
    //rbu light-house record page norm id
    memcpy(copy_location, &vptr->cur_page->page_norm_id, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE; 

    //rbu light-house record &sess_idx
    memcpy(copy_location, &vptr->sess_ptr->sess_norm_id, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;  

    //rbu light-house record nvm_id
    nvm_id = (int)(child_idx & 0x00FF);
    memcpy(copy_location, &nvm_id, SHORT_SIZE);
    copy_location += SHORT_SIZE;
 
    //rbu light-house record gen_id
    if (send_events_to_master == 1) //In case of NC, calculate generator id whereas in case of standalone send -1
      gen_id = (int)((child_idx & 0xFF00) >> 8);
    memcpy(copy_location, &gen_id, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //rbu light-house record group_num
    memcpy(copy_location, &(rpf_ptr->grp_norm_id), SHORT_SIZE);
    copy_location += SHORT_SIZE;

    //rbu light-house record host_id
    memcpy(copy_location, &host_id, SHORT_SIZE);
    copy_location += SHORT_SIZE;

    //rbu light-house record browser_type
    memcpy(copy_location, &browser_type, SHORT_SIZE);
    copy_location += SHORT_SIZE;

    //rbu light-house record group_name
    memcpy(copy_location, &group_name_size, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;
    if(group_name_size > 0)
    {
      memcpy(copy_location, rpf_ptr->scen_group_name, group_name_size);
      copy_location += group_name_size;
    }

    //rbu light-house record lh_file_date_time 
    memcpy(copy_location, &lh_file_date_time, UNSIGNED_LONG_SIZE);
    copy_location += UNSIGNED_LONG_SIZE;

    //rbu light-house record performance_score 
    memcpy(copy_location, &performance_score, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //rbu light-house record pwa_score 
    memcpy(copy_location, &pwa_score, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //rbu light-house record accessibility_score 
    memcpy(copy_location, &accessibility_score, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;
 
    //rbu light-house record best_practice_score 
    memcpy(copy_location, &best_practice_score, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;
 
    //rbu light-house record seo_score
    memcpy(copy_location, &seo_score, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;
 
    //rbu light-house record first_contentful_paint_time 
    memcpy(copy_location, &first_contentful_paint_time, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //rbu light-house record first_meaningful_paint_time
    memcpy(copy_location, &first_meaningful_paint_time, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //rbu light-house record speed_index
    memcpy(copy_location, &speed_index, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //rbu light-house record first_CPU_idle 
    memcpy(copy_location, &first_CPU_idle, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //rbu light-house record time_to_interact
    memcpy(copy_location, &time_to_interact, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //rbu light-house record input_latency 
    memcpy(copy_location, &input_latency, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //rbu light-house record Largest Contentful Paint Time
    memcpy(copy_location, &largest_contentful_paint, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

    //rbu light-house record Cumulative Layout Shift
    memcpy(copy_location, &cls_size, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;
    if(cls_size > 0)
    {
      memcpy(copy_location, cumulative_layout_shift, cls_size);
      copy_location += cls_size;
    }

    //rbu light-house record Total Blocking Time
    memcpy(copy_location, &total_blocking_time, UNSIGNED_INT_SIZE);
    copy_location += UNSIGNED_INT_SIZE;

  } else
    return -1;

  return 0;
}

int log_data_record(unsigned int sess_inst, http_request_Shr* url, char* buf, int buf_length) {
  int bytes_left_in_buf;
  int bytes_to_request;
  unsigned int data_copy_length;
  int last_chunk;
  char* copy_location;
  unsigned char record_num;

  NSDL2_LOGGING(NULL, NULL, "Method called, child_idx = %d, "
                            "sess_inst = %u, buf = %s, buf_length = %d", 
                            child_idx, sess_inst, buf, buf_length);

  while (buf_length) {
    if (last_local_buf)
      bytes_left_in_buf = log_shr_buffer_size - local_written_size;
    else
      bytes_left_in_buf = log_shr_buffer_size - logging_shr_mem->bytes_written;
    if (bytes_left_in_buf < data_record_min_size) {
      bytes_left_in_buf = log_shr_buffer_size;
    }

    if (bytes_left_in_buf < (buf_length + data_record_hdr_size)) {
      bytes_to_request = bytes_left_in_buf;
      last_chunk = 0;
    } else {
      bytes_to_request = buf_length + data_record_hdr_size;
      last_chunk = 1;
    }

    data_copy_length = bytes_to_request - data_record_hdr_size;

    if ((copy_location = get_mem_ptr(bytes_to_request))) {
      if (last_chunk) {
	record_num = DATA_LAST_RECORD;
	memcpy(copy_location, &record_num, UNSIGNED_CHAR_SIZE);
      } else {
	record_num = DATA_RECORD;
	memcpy(copy_location, &record_num, UNSIGNED_CHAR_SIZE);
      }
      copy_location += UNSIGNED_CHAR_SIZE;
      memcpy(copy_location, &data_copy_length, UNSIGNED_INT_SIZE);
      copy_location += UNSIGNED_INT_SIZE;
      memcpy(copy_location, &child_idx, SHORT_SIZE);
      copy_location += SHORT_SIZE;
      memcpy(copy_location, &sess_inst, UNSIGNED_INT_SIZE);
      copy_location += UNSIGNED_INT_SIZE;
      memcpy(copy_location, &url, INDEX_SIZE);
      copy_location += INDEX_SIZE;
      memcpy(copy_location, buf, data_copy_length);
    } else
      return -1;

    buf += data_copy_length;
    buf_length -= data_copy_length;
  }

  return 0;
}

int log_message_record(unsigned int msg_num, u_ns_ts_t now, char* buf, int buf_length) {
  int bytes_left_in_buf;
  int bytes_to_request;
  unsigned int data_copy_length;
  int last_chunk;
  char* copy_location;
  unsigned char record_num;

  NSDL2_LOGGING(NULL, NULL, "Method called, child_idx = %d, msg_num = %u, now = %u, buf = %s, buf_length = %d", child_idx, msg_num, now, buf, buf_length);
  while (buf_length) {
    if (last_local_buf)
      bytes_left_in_buf = log_shr_buffer_size - local_written_size;
    else
      bytes_left_in_buf = log_shr_buffer_size - logging_shr_mem->bytes_written;
    if (bytes_left_in_buf < msg_record_min_size) {
      bytes_left_in_buf = log_shr_buffer_size;
    }

    if (bytes_left_in_buf < (buf_length + msg_record_hdr_size)) {
      bytes_to_request = bytes_left_in_buf;
      last_chunk = 0;
    } else {
      bytes_to_request = buf_length + msg_record_hdr_size;
      last_chunk = 1;
    }

    data_copy_length = bytes_to_request - msg_record_hdr_size;

    if ((copy_location = get_mem_ptr(bytes_to_request))) {
      if (last_chunk) {
	record_num = MSG_LAST_RECORD;
	memcpy(copy_location, &record_num, UNSIGNED_CHAR_SIZE);
      } else {
	record_num = MSG_RECORD;
	memcpy(copy_location, &record_num, UNSIGNED_CHAR_SIZE);
      }
      copy_location += UNSIGNED_CHAR_SIZE;
      memcpy(copy_location, &data_copy_length, UNSIGNED_INT_SIZE);
      copy_location += UNSIGNED_INT_SIZE;
      memcpy(copy_location, &child_idx, SHORT_SIZE);
      copy_location += SHORT_SIZE;
      memcpy(copy_location, &msg_num, UNSIGNED_INT_SIZE);
      copy_location += UNSIGNED_INT_SIZE;
      memcpy(copy_location, &now, NS_TIME_DATA_SIZE);
      copy_location += NS_TIME_DATA_SIZE;
      memcpy(copy_location, buf, data_copy_length);
    } else
      return -1;

    buf += data_copy_length;
    buf_length -= data_copy_length;
  }

  return 0;
}

int log_message_record2(unsigned int msg_num, u_ns_ts_t now, char* buf, int buf_length, char* buf2, int len2) {
  int bytes_left_in_buf;
  int bytes_to_request;
  unsigned int data_copy_length;
  int last_chunk;
  char* copy_location;
  unsigned char record_num;
  //Total message size
  unsigned int total_size = buf_length + len2;
  
  NSDL1_LOGGING(NULL, NULL, "Method called, child_idx = %d, msg_num = %d, now = %u, buf_length = %d, len2 = %d", child_idx, msg_num, now, buf_length, len2);
  NSDL4_LOGGING(NULL, NULL, "buf = %s, buf2 = %s, ", buf, buf2);

  while (buf_length + len2) {
    NSDL4_LOGGING(NULL, NULL, "At starting of loop, buf_length = %d, len2 = %d", buf_length, len2);
    if (last_local_buf)
      bytes_left_in_buf = log_shr_buffer_size - local_written_size;
    else
      bytes_left_in_buf = log_shr_buffer_size - logging_shr_mem->bytes_written;
    if (bytes_left_in_buf < msg_record_min_size) {
      bytes_left_in_buf = log_shr_buffer_size;
    }

    if (bytes_left_in_buf < (buf_length +len2 + msg_record_hdr_size)) {
      bytes_to_request = bytes_left_in_buf;
      last_chunk = 0;
    } else {
      bytes_to_request = buf_length +len2 + msg_record_hdr_size;
      last_chunk = 1;
    }

    NSDL2_LOGGING(NULL, NULL, "bytes_left_in_buf = %d, logging_shr_mem->bytes_written = %d, msg_record_min_size = %d, last_chunk = %d", bytes_left_in_buf, logging_shr_mem->bytes_written, msg_record_min_size, last_chunk);

    data_copy_length = bytes_to_request - msg_record_hdr_size;

    NSDL2_LOGGING(NULL, NULL, "bytes_to_request = %d, data_copy_length = %d", bytes_to_request, data_copy_length);
    if ((copy_location = get_mem_ptr(bytes_to_request))) {
      if (last_chunk) {
	record_num = MSG_LAST_RECORD;
      } else {
	record_num = MSG_RECORD;
      }
      NSDL2_LOGGING(NULL, NULL, "bytes_to_request = %d, data_copy_length = %d, record_num = %d", bytes_to_request, data_copy_length, record_num);

      memcpy(copy_location, &record_num, UNSIGNED_CHAR_SIZE);
      copy_location += UNSIGNED_CHAR_SIZE;
      //Copy total size
      memcpy(copy_location, &total_size, UNSIGNED_INT_SIZE);
      copy_location += UNSIGNED_INT_SIZE;
      memcpy(copy_location, &data_copy_length, UNSIGNED_INT_SIZE);
      copy_location += UNSIGNED_INT_SIZE;
      memcpy(copy_location, &child_idx, SHORT_SIZE);
      copy_location += SHORT_SIZE;
      memcpy(copy_location, &msg_num, UNSIGNED_INT_SIZE);
      copy_location += UNSIGNED_INT_SIZE;
      memcpy(copy_location, &now, NS_TIME_DATA_SIZE);
      copy_location += NS_TIME_DATA_SIZE;
      if (buf_length) { //Hdr length
          NSDL2_LOGGING(NULL, NULL, "Writting headers buffer");
	  if (buf_length >= data_copy_length) {
              memcpy(copy_location, buf, data_copy_length);
    	      buf += data_copy_length;
    	      buf_length -= data_copy_length;
	  } else {
              memcpy(copy_location, buf, buf_length);
	      data_copy_length -= buf_length;
      	      copy_location += buf_length;
    	      buf_length = 0;
              if(buf2 != NULL){
                memcpy(copy_location, buf2, data_copy_length);
    	        buf2 += data_copy_length;
    	        len2 -= data_copy_length;
              }
	  }
      } else if (len2) { //Response length
          NSDL2_LOGGING(NULL, NULL, "Writting response buffer");
          memcpy(copy_location, buf2, data_copy_length);
    	  buf2 += data_copy_length;
    	  len2 -= data_copy_length;
      }
    } else
      return -1;
    NSDL4_LOGGING(NULL, NULL, "At end of loop, buf_length = %d, len2 = %d", buf_length, len2);
  }

  return 0;
}

void dump_debug_data (int count)
{
  /*NSTL1_OUT(NULL, NULL, "Total url record size = %lld, total url records = %d\n", total_url_record_size, total_url_records);
  NSTL1_OUT(NULL, NULL, "Total session record size = %lld, total session records = %d\n", total_session_record_size, total_session_records);
  NSTL1_OUT(NULL, NULL, "Total tx record size = %lld, total tx records = %d\n", total_tx_record_size, total_tx_records);
  NSTL1_OUT(NULL, NULL, "Total page record size = %lld, total page records = %d\n", total_page_record_size, total_page_records);
  NSTL1_OUT(NULL, NULL, "Total page waste size = %lld\n", total_waste_size);
  NSTL1_OUT(NULL, NULL, "Total size of all blocks = %lld\n", 
		    total_url_record_size + total_session_record_size + total_tx_record_size + total_page_record_size + total_waste_size);
  */
  FILE *db_records_fp = NULL;
  char db_records_file[1024];
  char buf[1024] = {0};
  sprintf(db_records_file, "logs/TR%d/ready_reports/db_record.dat", testidx);
  db_records_fp = fopen(db_records_file, "a+");
  if (!db_records_fp)
  {
    NSDL1_LOGGING(NULL, NULL,"error in opening");
    return;
  }
  sprintf(buf, "NVM %d,%d,%d,%d,%d,%d,%lld,%lld,%lld,%lld,%lld,%lld,%d,%d,%d,%d\n", my_port_index, total_url_records, total_page_records, total_tx_pg_records, total_tx_records, total_session_records, total_url_record_size, total_page_record_size, total_tx_pg_record_size, total_tx_record_size, total_session_record_size, total_waste_size, block_num, count, total_free_local_bufs, total_local_bufs);
  fprintf(db_records_fp, "%s", buf);
  fclose(db_records_fp);
}

void flush_logging_buffers(void) {
  int max_count = 5;
  int sleep_duration = 1000000; // Sleep time in microseconds which 1 seconds)
  int count = 1;
  write_buffer *wb;
  int flag, i;

  if (writer_pid <= 0)
    return;

  // Following cases are possible
  //  - Last buffer has no data. In this case, nothing is to be done. In this case, disk_flag should remain 0. So logging_writer will not use this buffer
  //    This will happen in only one case:
  //      - No data is logged by this NVM at any time. 
  //  - Last buffer has data and was locked. In this case, disk_flag is to be set to 1 so that logging_writer will use this buffer
  //  - Last buffer was already unlocked and data is NOT in local buffer - This should not happen
  //  - Last buffer was already unlocked and data was in local buffer
  //
  //  Data in       Data in (w/o disk flag)
  //  LocalBuffer   CurrentBufferInShm       Locked  Action
  //     No             No                    No    Not a valid condition as NVM locks the first buffer on start
  //     No             No                    Yes   Check using bytes_written is 0. Unlock this buffer so that logging writer do not get stuck 
  //     No             Yes                   No    Not a valid condition as NVM locks the buffer which is used
  //     No             Yes                   Yes   Set disk_flag and Unlock
  //     Yes            No                    No    Write local buffers in dlog file
  //     Yes            Yes                   *     Not a valid condition as local buffer used means that no shm buffer was free
  //
 
  write_buffer* shr_buffer = &logging_shr_mem->buffer[logging_shr_mem->current_buffer_idx];
  //NS_UNLOCK_LOGGING_SHR_BUFFER(shr_buffer);

  NSDL2_LOGGING(NULL, NULL, "Method called. logging_shr_mem->write_flag = %d, local_buf = %p", logging_shr_mem->write_flag, local_buf);

  NSDL1_LOGGING(NULL, NULL, "Logging stats: Total number of blocks generated by this NVM = %d, current number of local buffers = %d, current number of free local buffers = %d", block_num, total_local_bufs, total_free_local_bufs);

  if(local_buf == NULL) // No data in local buffers
  {
    // Check if any data was written in the shared memory. It will be 0 only in one case when there was data generated by NVM 
    if(logging_shr_mem->bytes_written)
    {
      NSDL2_LOGGING(NULL, NULL, "There is no data in local buffers but current shared memory buffer has data of size = %d", logging_shr_mem->bytes_written);
      shr_buffer->memory_flag = 0;
      shr_buffer->disk_flag = 1;
      if(logging_shr_mem->bytes_written < log_shr_buffer_size){
        char size_left = -1;
        memcpy(shr_buffer->data_buffer + logging_shr_mem->bytes_written, &size_left, 1);
      }
      //dump_mydlog(shr_buffer->data_buffer, log_shr_buffer_size);/*Dump to dlog*/
      //We are assiging block number here. There are 2 cases possible :
      //Case1: Only one shared block is assigned then block num will not get assigned to this block.
      //Case2: Its last block and no local buffers are there then this block will not get assigned block number
      shr_buffer->block_num = block_num; // assign block number
    }
    else
      NSDL2_LOGGING(NULL, NULL, "There is no data in local buffers and no data in current shared memory buffer");

    // Always unlock
    NS_UNLOCK_LOGGING_SHR_BUFFER(shr_buffer);
  }

  //Without checking just send signal to writer 
  SEND_SIGNAL_TO_WRITER_ALWAYS;

  // Allow some time for logging writter to take data frm shared memory and write to dlog
  while (logging_shr_mem->write_flag == 0) {

    if (count > max_count) {
      if (local_buf) {
        NS_EL_2_ATTR(EID_LOGGING_BUFFER,  -1, -1, EVENT_CORE, EVENT_DEBUG,
                                __FILE__, (char*)__FUNCTION__,
                               "Log data in shared memory is not written in dlog file. Data will be lost");
      }
      break;
    }

    NSDL2_LOGGING(NULL, NULL, "Sleeping for logging writter to flush data. Iteration = %d", count);
    
    usleep(sleep_duration);
    count++;
  }
  
  if (logging_shr_mem->write_flag == 1) { /* Irrespective of local_buf. We should send signal so that 
                                           * shared mem from other NVMs also gets flushed */
    NS_EL_2_ATTR(EID_LOGGING_BUFFER,  -1, -1, EVENT_CORE, EVENT_DEBUG,
                              __FILE__, (char*)__FUNCTION__,
                             "Sending SIGUSR1 to logging writer");
    SEND_SIGNAL_TO_WRITER;
  }

  /* Wait for nsa_logger to write the shr data completely. and then flush local_buf. */
  count = 1;
  wb = logging_shr_mem->buffer;
  while(1)
  {
    flag = 0;

    for (i = 0; i < logging_shr_blk_count; i++) {
      if (wb[i].disk_flag == 1)
        flag = 1;
    }

    if(flag == 0)
      break;

    if (count > max_count) {
      NS_EL_2_ATTR(EID_LOGGING_BUFFER,  -1, -1, EVENT_CORE, EVENT_CRITICAL,
                   __FILE__, (char*)__FUNCTION__,
                   "Log data in shared memory is not written in dlog file. Data will be lost");
      break;
    }

    NSDL2_LOGGING(NULL, NULL, "flag = 1, data not completely written. sleeping");
    SEND_SIGNAL_TO_WRITER;
    usleep(sleep_duration);
    count++;
  }

  count = 1;
  // Data left in local buffer is written directly in the dlog file by NVM
  while (local_buf) {
    NSDL1_LOGGING(NULL, NULL, "Writing data from local buffer. Local buffer block number = %d, block_num = %d", count, block_num);
    //NSTL1_OUT(NULL, NULL, "Writing data from local buffer. Local buffer block number = %d, block_num = %d\n", count, block_num);

    if (local_buf->next == NULL) {  /* The last local buf in the list */
      char size_left = -1; // Indicates end of data in each block
      memcpy(local_buf->data_buffer + local_written_size, &size_left, 1);
    }
    if(write(runtime_logging_fd, local_buf->data_buffer, log_shr_buffer_size) != log_shr_buffer_size)
    {
      NSTL1_OUT(NULL, NULL, "Error in writing data in dlog file, Ignored. Error = %s\n", nslb_strerror(errno));
    }
    // TODO:Free local buffers. Do we need to do or not?
    local_buf = local_buf->next;
    count++;
    block_num++; // Increament global block number
  }
  
  dump_debug_data(count - 1);

}


void stop_logging(void) {
  NSDL2_LOGGING(NULL, NULL, "Method called");
  if(static_logging_fp != NULL)
    fclose(static_logging_fp);
  // if (static_logging_fd != -1) // fdopen also closed fd
    // close(static_logging_fd);
  
  static_logging_fd = -1;
  static_logging_fp = NULL;
  
  if (runtime_logging_fd != -1)
    close(runtime_logging_fd);
  runtime_logging_fd = -1;
}

/* logs session status to sess_status_log file */
inline void log_session_status(VUser *vptr)
{
  char status[0xff];

  NSDL1_LOGGING(vptr, NULL, "Method called, Tracing level = %d," 
                     "trace_on_fail = %d, page_status = %d\n", 
                    runprof_table_shr_mem[vptr->group_num].gset.trace_level,
                    runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail, vptr->page_status);

  /* Added check for trace-on-failure option equal to 2*/
  /*if ((runprof_table_shr_mem[vptr->group_num].gset.trace_level > TRACE_LEVEL_1) && 
      ((runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == 0) || 
       ((runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail > 0) && 
         (vptr->page_status != NS_REQUEST_OK))))*/ 
  {
    NSDL3_LOGGING(vptr, NULL,"Logging data in session status, child_index = %d, vptr->sess_inst = %u, sess_status = %s",  child_idx, vptr->sess_inst, get_session_error_code_name( vptr->sess_status));
    snprintf(status, sizeof(status) - 1, "%hd:%u|%s\n", child_idx, vptr->sess_inst,
             get_session_error_code_name( vptr->sess_status));
    write(session_status_fd, status, strlen(status));
  }
}


void log_usage (char *msg, char *value){
  NSTL1_OUT(NULL, NULL, "%s\n", value);
  NSTL1_OUT(NULL, NULL, "%s\n", msg);
  NSTL1_OUT(NULL, NULL, "Usage: DEBUG_LOGGING_WRITER [0/1]");
  exit(1);
}


void kw_logging_writer_debug(char *buf)
{
  int num;
  char keyword[1024];
  char tmp[1024];
  char value[1024];

  num = sscanf(buf, "%s %s %s", keyword, value, tmp);

  if( num != 2 )
  {
    log_usage("Only one field needed after keyword DEBUG_LOGGING_WRITER", buf);
  }

  if(!ns_is_numeric(value))
  {
    log_usage("Value of argument is not valid, it should be numeric only", buf);
  }

  if((atoi(value)) < 0 || (atoi(value)) > 4) 
  {
    log_usage("Value of argument is not valid", buf);
  }

  global_settings->logging_writer_debug = atoi(value);
}

int kw_set_nirru_trace_level(char *buf, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH];
  int level;
  int size = 0;
  int num;

  NSDL2_PARSING(NULL, NULL, "Method called, buf = %s", buf);
        
  num = sscanf(buf, "%s %d %d %s", keyword, &level, &size, tmp);
 
  if (num < 2){
    NSTL1(NULL, NULL, "Invaid number of arguments in line %s.", buf);
    NS_EXIT(-1, "Invaid number of arguments in line %s.", buf);
  }
  
  if (level < 0 || level > 4)
  {
    //NSTL1_OUT(NULL, NULL, "Invaid file aggregator level %d, hence setting to default level 1\n", level);
    NSTL1(NULL, NULL, "Invaid file aggregator level %d, hence setting to default level 1", level);
    level = 1;
  }

  if (size <= 0){
    if(num >= 3) //checking if size is provided by user or not, if yes then show warning
    //NSTL1_OUT(NULL, NULL, "Invalid file aggregator file size %d, hence setting to default size 10 MB;\n", size);
    NSTL1(NULL, NULL, "Invalid file aggregator file size %d, hence setting to default size 10 MB;", size);
    size = 10;
  }

  global_settings->nirru_trace_level = level;
  NSDL2_PARSING(NULL, NULL, "trace_level = %d", level);
  return 0;
}

/********************************************************************************************************
 *Description         : kw_set_ns_logging_writer_arg() method usage 
 *********************************************************************************************************/
void ns_logging_writer_arg_usage(char *err_msg, char *buf)
{
  NSTL1_OUT(NULL, NULL, "Error: Invalid value of NS_LOGGING_WRITER_ARG keyword: %s\n", err_msg);
  NSTL1_OUT(NULL, NULL, "       Line %s\n", buf);
  NSTL1_OUT(NULL, NULL, "  Usage: NS_LOGGING_WRITER_ARG <No of blocks>\n");
  NSTL1_OUT(NULL, NULL, "  Where:\n");
  NSTL1_OUT(NULL, NULL, "         No of blocks : Is used to specify total no of blocks we want to set. Default value is 16\n");
  exit(-1);
}
 
/********************************************************************************************************
 *Description         : kw_set_ns_logging_writer_arg() method is used to parse LOGGING_WRITER_ARG keyword
 *********************************************************************************************************/
int kw_set_ns_logging_writer_arg(char *buf)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char tmp_data[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH];//Extra args
  int num;

  NSDL1_PARSING(NULL, NULL, "Method called. buf = %s", buf);

  num = sscanf(buf, "%s %s %s", keyword, tmp_data, tmp);

  if (num != 2)
  {
    ns_logging_writer_arg_usage("Invaid number of arguments", buf);
  }
 
  if(ns_is_numeric(tmp_data) == 0) {
    ns_logging_writer_arg_usage("Logging writer arg can have only integer value", buf);
  }

  logging_shr_blk_count = atoi(tmp_data);

  if(logging_shr_blk_count < 0)
    ns_logging_writer_arg_usage("Logging writer arg can have only positive integer value", buf);

  return 0;
}

/*  This function reads keyword MAX_LOGGING_BUFS.
 *  This keyword provides number of max bufs a NVM can use if shared memory is full
 *  By default it's 10000.
 *
 *  Usage: MAX_LOGGING_BUFS <Number of logging bufs>
 */
void kw_set_max_logging_bufs(char *buf)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  int max_logging_bufs = 10000;
  
  if(sscanf(buf, "%s %d", keyword, &max_logging_bufs) < 2)
  {
    NSTL1_OUT(NULL, NULL, "Error: Not enough arguments for MAX_LOGGING_BUFS keyword\n");
    NSTL1_OUT(NULL, NULL, "Usage: MAX_LOGGING_BUFS <Number of logging bufs>");
    exit(-1);
  }

  if(max_logging_bufs <= 0)
  {
    //NSTL1_OUT(NULL, NULL, "Warning: Argument to MAX_LOGGING_BUFS keyword is not valid, hence setting to default 10000\n");
    NSTL1(NULL, NULL, "Warning: Argument to MAX_LOGGING_BUFS keyword is not valid, hence setting to default 10000");
    NS_DUMP_WARNING("For MAX_LOGGING_BUFS keyword, number of logging buffers can not '%d'. Setting number of logging buffers to 1000.", max_logging_bufs);
    max_logging_bufs = 10000;
  }
  global_settings->max_logging_bufs = max_logging_bufs;
  NSDL3_PARSING(NULL, NULL, "global_settings->max_logging_bufs = %d", global_settings->max_logging_bufs);
}


//flush buffer to writer
inline void flush_shm_buffer()
{
  NSDL1_LOGGING(NULL, NULL, "Method called");
  //return;  
  copy_local_buffers();//If any local buffers are available then copy in share buffe
  write_buffer* shr_buffer = &logging_shr_mem->buffer[logging_shr_mem->current_buffer_idx];
  NSDL2_LOGGING(NULL, NULL, "Current buffer index = %d, Memory flag = %d, Bytes Written = %d", logging_shr_mem->current_buffer_idx, shr_buffer->memory_flag, logging_shr_mem->bytes_written);
  //If block have data then fill buffer
  if ((shr_buffer->memory_flag == 1) && (logging_shr_mem->bytes_written)) {
    if(logging_shr_mem->bytes_written < log_shr_buffer_size){
      char size_left = -1;
      memcpy(shr_buffer->data_buffer + logging_shr_mem->bytes_written, &size_left, 1);
    }

    //fill_up_buffer(logging_shr_mem->bytes_written, shr_buffer->buffer);
    NSDL1_LOGGING(NULL, NULL, "Before setting value of  memory flag = %d, disk_flag = %d", shr_buffer->memory_flag, shr_buffer->disk_flag);
    inc_shr_buf();
    logging_shr_mem->bytes_written = 0;
#if 0
    shr_buffer->memory_flag = 0;
    shr_buffer->disk_flag = 1;
    //There is condition when this can be called again but there is no data is written in next block so nake bytes_written to 0
    //Case: When progress msescs is 1 sec and session paching is 10 sec then data will come after 10 secs but this function will be called on every 1 sec
    logging_shr_mem->bytes_written = 0;
    logging_shr_mem->current_buffer_idx++;
    logging_shr_mem->current_buffer_idx %= logging_shr_blk_count;

    shr_buffer->block_num = block_num; // assign block number
    block_num++;

    NSDL2_LOGGING(NULL, NULL, "After assinging block number, shr_buffer->block_num = %d, block_num = %d", shr_buffer->block_num, block_num);
    NSDL1_LOGGING(NULL, NULL, "After setting value of  memory flag = %d, disk_flag = %d", shr_buffer->memory_flag, shr_buffer->disk_flag);
    //Without checking just send signal to writer 
    NSDL1_LOGGING(NULL, NULL, "Send signal to writer without checking");
    SEND_SIGNAL_TO_WRITER_ALWAYS;
#endif
  }
}

/*
 *
 * This code is to debug the page dump issue
int init_nvm_logging_shr_mem_test(shr_logging *logging_memory)
{
  NSDL2_LOGGING(NULL, NULL, "Method called.");
  NSTL1_OUT(NULL, NULL, "%s Method called.\n", (char*)__FUNCTION__);

  logging_shr_mem = &logging_memory[0];
  write_buffer* shr_buffer = &logging_shr_mem->buffer[logging_shr_mem->current_buffer_idx];

  NS_LOCK_LOGGING_SHR_BUFFER(shr_buffer);
  
  return 0;
}

static void write_data_in_debug_logging ()
{
  int i;
  static int my_num = 1;
  int total_written = 0;
  char *copy_location;
  int count = 1000000;

  my_buf = read_file_and_fill_buffer(&my_buf_size);
  char *buf = "SessionInstance=0; UserId=0; Group=G1; Script=DemoTours:HPD.c; Page=Home; PageStatus=Success; PageInstance=0; Size=2975; Level=4; FileSuffix=0_0_0_0_0_0_0_0_0.dat; URLStatus=Success; SessionStatus=NF;";
  int buf_length = strlen(buf);

  ThinkProfTableEntry_Shr *think_ptr = (ThinkProfTableEntry_Shr *)runprof_table_shr_mem[0].page_think_table[0];
 
  while(count){ //Infinite loop
    total_written = 0;
    if ((copy_location = get_mem_ptr(log_shr_buffer_size))) {
      //Runnig loop 2047 times as (8192/4) = 2047 - 1; -1 for last record as we will
      //inset a new line at the end of block
      //for(i = 0; i < 2047; i++){
      for(i = 0; i < 1364; i++){
        total_written += sprintf(copy_location + total_written, "%06d", my_num);
      }
      copy_location += total_written;
      //sprintf(copy_location, "000\n");
      sprintf(copy_location, "00000\n");
      //NSTL1_OUT(NULL, NULL, "PTT = %d\n", think_ptr->avg_time);
      if(think_ptr->avg_time > 0){
        //NSTL1_OUT(NULL, NULL, "Going to sleep\n");
        usleep(think_ptr->avg_time);
        //NSTL1_OUT(NULL, NULL, "After sleep\n");
      }
      //NSTL1_OUT(NULL, NULL, "my_num = %d\n", my_num);
      //NSTL1_OUT(NULL, NULL, "count = %d,\n", count);
    }
    
    log_message_record2(0, my_num, 8172, buf, buf_length, my_buf, my_buf_size);
    my_num++;
    count--;
  }
  NSTL1_OUT(NULL, NULL, "my_num = %d\n", my_num);

  sleep(100000);
}

file_to_buffer(char **fbuf, char *file, int size)
{
  FILE *fp;
  MY_MALLOC(*fbuf, size, "fbuf", -1);
  fp = fopen(file, "r");
  fread(*fbuf, size, 1, fp);
  fclose(fp);
}

*/
