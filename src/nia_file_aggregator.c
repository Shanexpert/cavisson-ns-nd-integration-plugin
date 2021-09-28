#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <signal.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <assert.h>
#include <time.h>

#include "nia_file_aggregator.h"
#include "nslb_sock.h"
#include "util.h"
#include "ns_log.h"
#include "nslb_alloc.h" 
#include "nslb_log.h"
#include "nslb_multi_thread_trace_log.h"
#include "nslb_signal.h"
#include "nslb_common.h"
#include "nslb_comp_recovery.h"
#include "nslb_util.h"
#include "nslb_cmon_session.h"
#include "nslb_http_state_transition_init.h"

#define BLOCK_SIZE 8*1024
#define NIA_PARTITION 1
#define NIA_CONTINUE 0
#define NIA_STOP -2
#define NIA_ERROR -1

#define SLOG_THREAD 0 
#define DLOG_THREAD 1
#define RBU_ACCESS_LOG_THREAD 2
#define RBU_LIGHTHOUSE_THREAD 3 

int trace_log_size = 10;

UsedGen* usedGenTable = NULL;

static int online_mode = 0;
static int g_partition_mode = 0;
static int trace_level = 1;
static int dump_raw_data = 0;
static int is_test_running = 1;
static long long global_partition_idx = -1;
static long long global_start_partition_idx = -1;
CurPartInfo curPartInfo;
PartitionInfo part_info;
static int reader_run_mode = 0;
static int nlr_trace_level = 1;
static int logging_reader_pid = -1;
static int reader_write_time = 10;
static int dynamic_table_size = 256;
static int dynamic_tx_table_size = 5000;
static int generator_id = -1;
static int cav_epoch_year = 2014;
static int num_nvm = 0;
static int num_gen = 0;
static int resume_flag = 1;
int loader_opcode = -1;
int rbu_access_log;
char rbu_lighthouse_enabled = 0;
struct epoll_event *coll_fds;

int max_generator_entries = 0;
static int total_generator_entries;

char base_dir[512];
static int test_run_num = 0;
static int parent_pid = -1;
int  sigterm = 0;
int sigalarm = 0;
int partition_switch_sig = 0;
int trace_level_change_sig = 0;
int test_post_proc_sig = 0;

static int test_run_info_id;
TestRunInfoTable_Shr *testrun_info_table = NULL;
MTTraceLogKey *nfa_trace_log_key = NULL;

static void sigterm_handler(int sig);
static void partition_switch_sig_handler(int sig);
static void trace_level_change_sig_handler(int sig);
//static void test_post_proc_sig_handler(int sig);
static void sigalarm_handler(int sig);

char ns_wdir[512] = "/home/cavisson/work";
static int dlog_bufsize = 8192; //default dlog buf size.
static int rbu_acc_log_bufsize = 512; //default RBU access log buf size.

typedef struct {
  int cmon_interval;
  int cmon_count;
  int cmon_bufsize;
  int cmon_debug_level;
  int epoll_wait;
  int recovery_interval;
  int dlog_start_delay;
  int lr_start_delay;
}NIFASettings;

//This will have some settings related to cmon and other.
NIFASettings nifa_settings = {1, 10, 16*1024, 0, 2/*should be more than cmon_interval*/, 30, 5, 5};

#define CLEAR_SPACE(ptr) {while ((*ptr == ' ') || (*ptr == '\t')) ptr++;}

//there will be two threads for slog and dlog
//Changing to four threads slog, dlog, rbu_access_log and lighthouse
#define NUM_THREADS 4

static char *thread_name[] = {"Main-Thread", "slog-thread", "dlog-thread", "rbu-access-log-thread", "rbu-lighthouse-thread"};

#define SEQUENCE_IDX 0

static int sigterm_recieved = 0;
short epoll_timeout_cnt = 0;

void sigterm_handler(int sig)
{ 
  sigterm = 1;
}

void sigalarm_handler(int sig)
{ 
  sigalarm = 1;
}


void partition_switch_sig_handler(int sig) 
{
  partition_switch_sig = 1;
}

static int post_proc_sig = 0;

static void post_proc_sig_handler(int sig)
{
  post_proc_sig = 1;
}

static int sigchld_sig = 0;
static void sigchld_handler(int sig)
{
  sigchld_sig = 1;
}

void trace_level_change_sig_handler(int sig)
{
  trace_level_change_sig = 1;
}

#if 0
void test_post_proc_sig_handler(int sig)
{
  test_post_proc_sig = 1;
}
#endif

static int create_generator_table_entry(int* row_num) {

  if (total_generator_entries == max_generator_entries) {
    NSLB_REALLOC(usedGenTable, (max_generator_entries + DELTA_GENERATOR_ENTRIES) * sizeof(UsedGen), "used generator entries", -1, NULL);
    max_generator_entries += DELTA_GENERATOR_ENTRIES;
  }
  *row_num = total_generator_entries++;
  return (0);
}

#if 0
void init_gen_data_table()
{
  NSDL1_VARS(NULL, NULL, "Method called");
  int i;

  NSLB_MALLOC (genDataTable, total_generator_entries * sizeof(GenData), "genDataTable", -1, NULL);

  for(i = 0; i < total_generator_entries; i++){
    genDataTable[i].fd = -1;
    genDataTable[i].data_fd = -1;
    genDataTable[i].offset = 0;
    genDataTable[i].read_amount = 0;
  }
}
#endif

void write_offset_info_to_file(char *thread_str, int off_fd, DataOffsetInfo *data_offset_info){

  //first move to start.
  if(lseek(off_fd, SEEK_SET, 0) < 0)
  {
    NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_ERROR, "Error in lseek to start of offset file, Error- [%s]", nslb_strerror(errno));
    exit(1);
  }
  //write_partition_idx and offset of gen files.
  if(write(off_fd, (void*)&data_offset_info->partition_idx, sizeof(long long)) < 0)
  {
    NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_ERROR, "Error in write to offset file, Error- [%s]", nslb_strerror(errno));
    exit(1);  
  }
  //now write offset.
  //debug.
  //TODO: remove this.
  int j;
  for(j = 0; j < total_generator_entries; j++)
  {
     NSLB_TRACE_LOG4(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, "write offset: genidx:%d offset: %lld", j, data_offset_info->offset[j]);
  }
  
  if(write(off_fd, (void*)data_offset_info->offset, sizeof(long long) * total_generator_entries) < 0)
  {
    NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_ERROR, "Error in write to offset file, Error- [%s]", nslb_strerror(errno));
    exit(1);  
  }
}

void parse_and_fill_used_gen_struct(char *file_path)
{
  FILE *fp;
  char line[1024];
  char *start_ptr;
  char *end_ptr;
  int rnum;

  NSLB_TRACE_LOG3(nfa_trace_log_key, global_partition_idx, "Main-Thread", NSLB_TL_INFO, "Method called");
  //NETCLOUD_GENERATOR_TRUN 2798|MAC_67|192.168.1.67|7891|/home/cavisson/work|IPV4:192.168.1.67|1
  if((fp = fopen(file_path, "r")) != NULL)
  {
    while(fgets(line, 1024, fp) != NULL){

     // CLEAR_SPACE(line); //Removing white spaces from start
    
      if(*line == '#' || *line == '\n' || *line == '\0')  //Ignoring commented line
        continue;

      start_ptr = strchr(line, '\n'); //replacing newline with '\0'
      *start_ptr = '\0'; 

      start_ptr = strchr(line, ' ');
      if(start_ptr != NULL)
        start_ptr++;
      else
        goto err_exit;

      if (create_generator_table_entry(&rnum) != 0) {
        NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, "Main thread", NSLB_TL_ERROR, "Not enough memory. Could not create used generator table entry.");
        exit(1);
      }
      //TR number 
      usedGenTable[rnum].tr_num = atoi(start_ptr);
      
      start_ptr = strchr(start_ptr, '|');
      
      if(start_ptr != NULL)
        start_ptr++;   //pointing at the begning of generator name
      else
        goto err_exit;

      if((end_ptr = strchr(start_ptr, '|')) == NULL)
        goto err_exit;
     
      *end_ptr = '\0';
      //Gen name
      strcpy(usedGenTable[rnum].gen_name, start_ptr);

      start_ptr = ++end_ptr; //both pointers are pointing to the begining of generator ip

      if((end_ptr = strchr(start_ptr, '|')) == NULL)
        goto err_exit;
      
      *end_ptr = '\0';
      //Gen IP 
      strcpy(usedGenTable[rnum].gen_ip, start_ptr);

      start_ptr = ++end_ptr;
   
      //now check for port.
      if((end_ptr = strchr(start_ptr, '|')) == NULL)
        goto err_exit;
  
      *end_ptr = '\0';
      //save port.
      usedGenTable[rnum].cmon_port = atoi(start_ptr);

      start_ptr = ++end_ptr;

      if((start_ptr = strchr(start_ptr, '/')) == NULL) //pointing to the controller_name
        goto err_exit;
      
      if((end_ptr = strchr(start_ptr, '|')) == NULL)
        goto err_exit;
      
      *end_ptr = '\0';

      strcpy(usedGenTable[rnum].work_dir, start_ptr); 
    } 
  }else{
    NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, "Main thread", NSLB_TL_ERROR, "Error: Unable to open file %s, Error- %s", file_path, nslb_strerror(errno));
    exit(1);
  err_exit:
    NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, "Main thread", NSLB_TL_ERROR, "Error: %s file format is not correct", file_path);
    exit(1); 
  }

  int i;
  for(i = 0; i<total_generator_entries; i++)
     NSLB_TRACE_LOG4(nfa_trace_log_key, global_partition_idx, "Main thread", NSLB_TL_INFO, "tr_num = [%d], gen_name = [%s], gen_ip = [%s], work_dir = [%s], cmon_port = %d", usedGenTable[i].tr_num, usedGenTable[i].gen_name, usedGenTable[i].gen_ip, usedGenTable[i].work_dir, usedGenTable[i].cmon_port);
}

int  validate_file_name(char *proc_buf, int thread_type)
{
  char *colon_ptr;
  char file_name[50] = "";

  if(thread_type == DLOG_THREAD)
    strcpy(file_name, "dlog");
  else if(thread_type == SLOG_THREAD ) 
    strcpy(file_name, "slog");
  else if(thread_type == RBU_LIGHTHOUSE_THREAD)
    strcpy(file_name, "RBULightHouseRecord.csv");
  else
   strcpy(file_name, "rbu_access_log");

  if((colon_ptr = strchr(proc_buf, ':')) != NULL)
  {
    colon_ptr++;
    if(strcmp(colon_ptr, file_name) == 0)
      return 0;
    else
      return 1;
  }else{
    NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, "Main thread", NSLB_TL_INFO, "Response doesnt contains valid filename =[%s], response = %s", colon_ptr, proc_buf);
    return 1;
  }
  return 0;
}


void send_start_msg_to_cavmon(char *thread_str, DataOffsetInfo *offset_info, int thread_type, GenData* genDataTable)
{
  int written_amt;
  int total_amt = 0;
  int amt_to_write;
  int read_amount = 0;
  int ret;  
  int total_read = 0;
  char read_buf[512] = "";
  char *read_ptr = read_buf;
  char *nl_ptr;
  char proc_buf[512];
  char *write_ptr;
  char msg[512];
  //this msg will be less than 512.


      int gen_idx = genDataTable->gen_idx;
      NSLB_TRACE_LOG2(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, "Method called, gen id = [%d]", gen_idx);
      if(nifa_settings.cmon_debug_level)
      {
        sprintf(msg, "cm_get_file_req:MON_PGM_ARGS= -f %s -D -s %d -n %d -L %d -i -o %lld;MON_FREQUENCY=%d;MON_TEST_RUN=%d;MON_NS_WDIR=%s\n",
               genDataTable->data_file_path, nifa_settings.cmon_bufsize, nifa_settings.cmon_count, nifa_settings.cmon_debug_level,
               offset_info->offset[gen_idx], nifa_settings.cmon_interval, usedGenTable[gen_idx].tr_num, usedGenTable[gen_idx].work_dir);
      }
      else {
        sprintf(msg, "cm_get_file_req:MON_PGM_ARGS= -f %s -s %d -n %d -i -o %lld;MON_FREQUENCY=%d;MON_TEST_RUN=%d;MON_NS_WDIR=%s\n",
                    genDataTable->data_file_path, nifa_settings.cmon_bufsize, nifa_settings.cmon_count, offset_info->offset[gen_idx],
                    nifa_settings.cmon_interval, usedGenTable[gen_idx].tr_num, usedGenTable[gen_idx].work_dir);
      }
     
      write_ptr = msg; 
      amt_to_write = strlen(msg);
      NSLB_TRACE_LOG2(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, "CavMon msg = '%s'", msg);

      while(1){ //in loop untill we write all the data
        if(total_amt >= amt_to_write){
          break;
        }
        written_amt = write(genDataTable->fd, write_ptr, amt_to_write - total_amt); 
        if(written_amt < 0 )
        {
          NSLB_TRACE_LOG2(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_ERROR, "Error in write. Error- [%s]", nslb_strerror(errno));
          exit(1);
        }
        total_amt += written_amt;
        write_ptr += written_amt;
      }//end of while

   //start reading response
   total_read = 0;
    while(1){
      read_amount = read(genDataTable->fd, read_ptr, 512 - total_read);

      if(read_amount == 0){
        //connection closed from other side.
        genDataTable->fd = -1;
        //close fd
        break;
      }
      if(read_amount < 0){
        if(errno == EAGAIN)
        {
          sleep(1);
          continue;
        }
        if(errno == EINTR)
          continue;

        NSLB_TRACE_LOG2(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_ERROR, "Error in read cavmon fd=[%d], Error- [%s]", genDataTable->fd, nslb_strerror(errno));
        genDataTable->fd = -1;
        break;
      }

      total_read += read_amount;
      read_ptr+= read_amount; 

      if((nl_ptr = strchr(read_buf, '\n')) != NULL){
        strncpy(proc_buf, read_buf, nl_ptr-read_buf);
        proc_buf[(nl_ptr-read_buf)] = '\0';
        nl_ptr++;

        ret = validate_file_name(proc_buf, thread_type);

        if(ret){
         // fprintf(stderr, "Invalid response recieved from cmon(%s)\n", usedGenTable[gen_idx].gen_ip);
          NSLB_TRACE_LOG2(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_ERROR, "Invalid response received from cavmon, exiting.");
          exit(-1);
        }

        NSLB_TRACE_LOG2(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, "proc buffer = [%s]", proc_buf);

        //copy rest of data to partial buf.
        memcpy(genDataTable->partial_buf.buf, nl_ptr, ((total_read - (nl_ptr-read_buf))));

        //write to raw_date file.
        if(dump_raw_data && thread_type != SLOG_THREAD)
        {
          ret = write(genDataTable->raw_data_fd, nl_ptr, ((total_read - (nl_ptr-read_buf))));
          if(ret != (total_read - (nl_ptr-read_buf)))
          {
            NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_ERROR, "Write failed for raw_data");
            exit(-1);
          }
        }
 
        //Because this could be called in case of recovery in that case read_amount may contain some previous data.
        genDataTable->read_amount +=  (total_read - (nl_ptr-read_buf));
        genDataTable->offset +=  (total_read - (nl_ptr-read_buf));
        
        NSLB_TRACE_LOG2(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, "gen_read_amt = [%d]", genDataTable->read_amount);
        //Don't update offset_info->offset[i]. because this should be updated with complete block.
        break; 
      }
    }//end of while loop
}

inline void
add_select(GenData *ptr, int fd, int event, int *nfa_epoll_fd)
{ 
  struct epoll_event pfd;

  bzero(&pfd, sizeof(struct epoll_event)); //Added after valgrind reported bug
        
  pfd.events = event;
  pfd.data.ptr = (void*)ptr;
  if (epoll_ctl(*nfa_epoll_fd, EPOLL_CTL_ADD, fd, &pfd) == -1) {
    NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, "Main thread", NSLB_TL_ERROR, "Error in adding %d to nfa_epoll_fd epoll");
    exit (-1);             
  }
}

inline void remove_select(int fd, int epoll_fd)
{
  struct epoll_event pfd;

  bzero(&pfd, sizeof(struct epoll_event)); //Added after valgrind reported bug

  if (fd == -1) return;

  if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, &pfd) == -1) {
    NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, "Main thread", NSLB_TL_ERROR, "Error in removing fd %d from epoll, error = %s",
            fd, nslb_strerror(errno));
    exit(-1);
  }
}

//FIXME: in case of send_start_msg_to_cavmon, if some extra data comes then it copies to partial buf handle that case carefully.  
static inline void cmon_con_recovery(char *thread_str, int thread_type, GenData *genDataTable, DataOffsetInfo *offset_info, int nfa_epoll_fd, int bufsize)
{
  int i, fd;
  int gen_idx;
  char err_msg[1024];

  NSLB_TRACE_LOG2(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, "Method called");
  for(i=0; i < total_generator_entries; i++)
  {
    if(genDataTable[i].fd > 0)
      continue;
   
    //update offset.
    genDataTable[i].offset = offset_info->offset[i];
    genDataTable[i].read_amount = 0;

    //not connected.
    gen_idx = genDataTable[i].gen_idx;
    NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_ERROR, "genid %d: ip %s, port %d, i %d, offset = %lld", gen_idx, usedGenTable[gen_idx].gen_ip, usedGenTable[gen_idx].cmon_port, genDataTable[gen_idx].offset);
    if((fd = nslb_tcp_client_ex(usedGenTable[gen_idx].gen_ip, usedGenTable[gen_idx].cmon_port, 10, err_msg)) < 0){
      NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, "Failed to connet to cmon for generator %d, error %s"
         , gen_idx, err_msg);
      genDataTable[i].fd = -1;
      continue;
    }else{
      NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, "Generator id %d, recovered. Starting from offset %lld", gen_idx, genDataTable[gen_idx].offset);
      genDataTable[i].fd = fd;
      fcntl(genDataTable[i].fd, F_SETFD, FD_CLOEXEC);
      //now truncate raw_data file to offset.
      if(dump_raw_data && thread_type != SLOG_THREAD)
      {
        ftruncate(genDataTable[i].raw_data_fd, genDataTable[i].offset);
      }

      send_start_msg_to_cavmon(thread_str, offset_info, thread_type, &genDataTable[i]);

      if(genDataTable[i].fd != -1){ 
        if (fcntl(genDataTable[i].fd, F_SETFL, O_NONBLOCK))
        {
          NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_ERROR, "Could not make the socket non-blocking:");
          close(genDataTable[i].fd);
          exit(1);
        }
        add_select(&genDataTable[i], genDataTable[i].fd, EPOLLIN|EPOLLERR|EPOLLHUP, &nfa_epoll_fd);
      }
    }
  }
}
 
int setup_gentable_make_conn(char *thread_str, int thread_type, int partition_mode, GenData *genDataTable, DataOffsetInfo *offset_info, int nfa_epoll_fd, int bufsize)
{
  int i;
  int fd = -1;
  char err_msg[1024];
  char raw_data_file[1024 + 64] = "";

  NSLB_TRACE_LOG2(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, "Method called");
  for(i=0; i < total_generator_entries; i++)
  {
    //set generator idx.
    genDataTable[i].gen_idx = i;

    if(thread_type == DLOG_THREAD){
      if(partition_mode)
      {
        sprintf(genDataTable[i].data_file_path, "%s/logs/TR%d/%lld/reports/raw_data/dlog", usedGenTable[i].work_dir, usedGenTable[i].tr_num, offset_info->partition_idx);
        if(dump_raw_data)
          sprintf(raw_data_file, "%s/%lld/reports/raw_data/dlog.%s", base_dir, offset_info->partition_idx, usedGenTable[i].gen_name);
      }
      else
      {
        sprintf(genDataTable[i].data_file_path, "%s/logs/TR%d/reports/raw_data/dlog", usedGenTable[i].work_dir, usedGenTable[i].tr_num);
        if(dump_raw_data)
          sprintf(raw_data_file, "%s/reports/raw_data/dlog.%s", base_dir, usedGenTable[i].gen_name);
      }
    }
    else if(thread_type == SLOG_THREAD) { //SLOG_THREAD
      if(g_partition_mode) //we will check for g_partition_mode because partition_mode will be 0 for this thread.
      {
        sprintf(genDataTable[i].data_file_path, "%s/logs/TR%d/%lld/reports/raw_data/slog", usedGenTable[i].work_dir, usedGenTable[i].tr_num, global_start_partition_idx);
      }
      else
        sprintf(genDataTable[i].data_file_path, "%s/logs/TR%d/reports/raw_data/slog", usedGenTable[i].work_dir, usedGenTable[i].tr_num);
    }
    else if(thread_type == RBU_ACCESS_LOG_THREAD){  //RBU_ACCESS_LOG_THREAD
      if(partition_mode)
      {
        sprintf(genDataTable[i].data_file_path, "%s/logs/TR%d/%lld/rbu_logs/rbu_access_log", usedGenTable[i].work_dir, usedGenTable[i].tr_num, offset_info->partition_idx);
        if(dump_raw_data)
          sprintf(raw_data_file, "%s/%lld/rbu_logs/rbu_access_log.%s", base_dir, offset_info->partition_idx, usedGenTable[i].gen_name);
      }
      else
      {
        sprintf(genDataTable[i].data_file_path, "%s/logs/TR%d/rbu_logs/rbu_access_log", usedGenTable[i].work_dir, usedGenTable[i].tr_num);
        if(dump_raw_data)
          sprintf(raw_data_file, "%s/rbu_logs/rbu_access_log.%s", base_dir, usedGenTable[i].gen_name);
      }
    }
    else if(thread_type == RBU_LIGHTHOUSE_THREAD){
      if(partition_mode)
      {
        sprintf(genDataTable[i].data_file_path, "%s/logs/TR%d/%lld/reports/csv/RBULightHouseRecord.csv",
                 usedGenTable[i].work_dir, usedGenTable[i].tr_num, offset_info->partition_idx);
        if(dump_raw_data)
          sprintf(raw_data_file, "%s/%lld/reports/csv/RBULightHouseRecord.csv.%s", base_dir, offset_info->partition_idx,
                   usedGenTable[i].gen_name);
      }
      else
      {
        sprintf(genDataTable[i].data_file_path, "%s/logs/TR%d/reports/csv/RBULightHouseRecord.csv",
                 usedGenTable[i].work_dir, usedGenTable[i].tr_num);
        if(dump_raw_data)
          sprintf(raw_data_file, "%s/reports/csv/RBULightHouseRecord.csv.%s", base_dir, usedGenTable[i].gen_name);
      }
    }

    //init partial buffer.
    if(!genDataTable[i].partial_buf.buf)
      NSLB_MALLOC(genDataTable[i].partial_buf.buf, bufsize, "genDataTable[i].partial_buf.buf", -1, NULL);

    //set offset from offset_info.
    genDataTable[i].offset = offset_info->offset[i];

    //create a raw data file just to print whatever we will get from cmon.
    //only for dlog and RBU access log.
    if(dump_raw_data && thread_type != SLOG_THREAD)
    {
      genDataTable[i].raw_data_fd = open(raw_data_file, O_CREAT|O_WRONLY |O_LARGEFILE| O_APPEND|O_CLOEXEC, 0666); 
      NSLB_TRACE_LOG2(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, "Opening file %s", raw_data_file);
      if(genDataTable[i].raw_data_fd <  0)
      {
       NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_ERROR, "genid %d: failed to create raw data file %s", i, raw_data_file);
       exit(-1); 
      }
      //truncate to offset.
      ftruncate(genDataTable[i].raw_data_fd, genDataTable[i].offset);
    }
 
    NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_ERROR, "genid %d: ip %s, port %d", i, usedGenTable[i].gen_ip, usedGenTable[i].cmon_port);
    if((fd = nslb_tcp_client_ex(usedGenTable[i].gen_ip, usedGenTable[i].cmon_port, 10, err_msg)) < 0){
      NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, "%s", err_msg);
      genDataTable[i].fd = -1;
      continue;
    }else{
       genDataTable[i].fd = fd;
       fcntl(genDataTable[i].fd, F_SETFD, FD_CLOEXEC);

       send_start_msg_to_cavmon(thread_str, offset_info, thread_type, &genDataTable[i]);

       if(genDataTable[i].fd != -1){ 
         if (fcntl(genDataTable[i].fd, F_SETFL, O_NONBLOCK))
         {
           NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_ERROR, "Could not make the socket non-blocking:");
           close(genDataTable[i].fd);
           exit(1);//Do we need to exit
         }
         add_select(&genDataTable[i], genDataTable[i].fd, EPOLLIN|EPOLLERR|EPOLLHUP, &nfa_epoll_fd);
       }
     }
  }
  NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, "all added to epoll");
  return 0;
}


#define DLOG_OFFSET_FILE ".dlog.offset"
#define SLOG_OFFSET_FILE ".slog.offset"
#define RBU_ACC_LOG_OFFSET_FILE ".rbu_acc_log.offset"
#define RBU_LIGHTHOUSE_RECORD_OFFSET_FILE ".RBULightHouseRecord.csv.offset"

//This file will read offset file if exist if not then it will open that.
static inline void check_and_set_offset_file(char *thread_str, int thread_type, int *off_fd, DataOffsetInfo *data_offset_info)
{
  char off_file[512 + 32];
    
  NSLB_TRACE_LOG2(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, "Method called");

  //TODO: check the location of dlog files.
  if(thread_type == DLOG_THREAD)
  {
    sprintf(off_file, "%s/%s", base_dir, DLOG_OFFSET_FILE);
  }
  else if(thread_type == SLOG_THREAD){  //slog thread
    sprintf(off_file, "%s/%s", base_dir, SLOG_OFFSET_FILE); 
  }
  else if(thread_type == RBU_ACCESS_LOG_THREAD)//RBU Access Log thread
    sprintf(off_file, "%s/%s", base_dir, RBU_ACC_LOG_OFFSET_FILE); 
  else if(thread_type == RBU_LIGHTHOUSE_THREAD)
    sprintf(off_file, "%s/%s", base_dir, RBU_LIGHTHOUSE_RECORD_OFFSET_FILE); 

  NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, "check_and_set_offset_file = %s", off_file);
  //open file.
  *off_fd = open(off_file, O_CREAT|O_RDWR|O_CLOEXEC, 0666);
  if(*off_fd < 0)
  {
    NSLB_TRACE_LOG2(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_ERROR, "Unable to open offset file = [%s], Error- [%s]", off_file, nslb_strerror(errno));
    exit(1);
  }
  
  //read data.
  //check if content is already exist.
  struct stat st;
  int size = sizeof(long long) + total_generator_entries * sizeof(long long);
  char buf[1024];
  
  if(stat(off_file, &st) < 0){
    NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_ERROR, "Cant stat offset file=[%s], Error- [%s]", off_file, nslb_strerror(errno));
    exit(1);
  }

  NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, "offset_file size = [%d], size = [%d]", st.st_size, size);
  //if resume flag is not enable then we will not check for .offset file.
  if(resume_flag && st.st_size > 0){
    if(st.st_size != size){
      //TODO: error. file exist but invalid data. What to do?? 
    }
    if(read(*off_fd, buf, size) < 0)
    {
      NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_ERROR, "Read error, Error- [%s]", nslb_strerror(errno));
    }
    //copy first 8 bytes to idx.
    memcpy(&data_offset_info->partition_idx, buf, sizeof(long long));
    //copy offset.
    memcpy(data_offset_info->offset, buf+sizeof(long long), size - sizeof(long long));
  }
  else {
    //need to initialize offset file.
    //this will be done from where it is called.
  }

  //print offset details.
  int i;
  NSLB_TRACE_LOG4(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, "offset file: prev partition = %lld", data_offset_info->partition_idx);
  for(i = 0; i < total_generator_entries; i++)
  {
    NSLB_TRACE_LOG4(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, "genidx: %d, offset = %lld Parent_pid= %d", i, data_offset_info->offset[i], parent_pid);
  }
}

long long get_start_partition(char *thread_str)
{
  NSLB_TRACE_LOG2(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, "Method called");
  //read partition info file of global_partition_idx./
  int ret;
  char partition_name[32];
  
  sprintf(partition_name, "%lld", global_partition_idx);
  if((ret = nslb_get_partition_info(base_dir, &part_info, partition_name)) != -1){
    return(part_info.start_partition);
  }else{ //error in reading partition info file
    NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_ERROR, "Error in reading start partition from partition info file");
    exit(-1);
  }
}


static inline void  nia_write_to_file(char *thread_str, GenData *ptr, char *data, int size, int fd, char *data_file)
{
  int ret = write(fd, data, size);
  if(ret != size)
  {
    NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_ERROR, "Failed to write to data file %s", data_file);
    exit(-1);
  }
}


//run cmd and get last line of output.
int run_cmd_and_get_last_line (char *thread_str, char *cmd, int length, char *out)
{
  FILE *app = NULL;
  char temp[length];

  app = popen(cmd, "r");

  if(app == NULL)
  {
    NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_ERROR, "popen failed for %s. Error:%s", cmd, nslb_strerror(errno));
    return -1;
  }

  while(fgets(temp, length, app))
  {
    strcpy(out, temp); // Out will have the last line of command output
  }

  if(pclose(app) == -1)
    NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_ERROR, "pclose() failed for %s", cmd);

  return 0;
}

#if 0
//this will run command to check data file size on generator and offset on controller if both are same that means partition done.
//FIXME: it is very heavey command we shoudl handle it in a different way.
//return -1: error
//				0: partition not done.
//				1: partition done.
static int is_partition_done(char *thread_str, GenData *genDataTable)
{
  int i;
  char cmd[1024 * 4];
  char out[4096];
  char data_file[512];
  long long gen_offset;  //offset of files on generator.
  int ret;

  for(i=0; i < total_generator_entries; i++) 
  {
    int gen_idx = genDataTable[i].gen_idx;
    sprintf(cmd, "%s/bin/nsu_server_admin -i -s %s -c 'wc -c %s'", ns_wdir, usedGenTable[gen_idx].gen_ip, genDataTable[i].data_file_path); 
    NSLB_TRACE_LOG3(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, "Running command %s", cmd);
    if(run_cmd_and_get_last_line(thread_str, cmd, 1024, out))
    {
      return -1;
    }
    //now check for response.
    //should be in format %d %s[offset filename.]
    ret = sscanf(out, "%lld %s", &gen_offset, data_file);
    if(ret != 2)
      return -1;

    if(gen_offset != genDataTable[i].offset)
    {
      NSLB_TRACE_LOG3(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, "Offset (%lld) not matched from file size(%lld) on generator, file name = %s", genDataTable[i].offset, gen_offset, data_file);
      return 0;
    }
  }
      NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, 
                      "Session established with CavMonAgent of Generator '%s'", usedGenTable[gen_idx].gen_name);
  
  //if file size equal to offset then it's done.
  return 1;
}
#endif

//this will run command to check data file size on generator and offset on controller if both are same that means partition done.
//return -1: error
//				0: partition not done.
//				1: partition done.
static int is_data_completed(char *thread_str, GenData *genDataTable)
{
  int i;
  char err_msg[1024] = "";
  int gen_idx;
  long long gen_offset;
  for(i=0; i < total_generator_entries; i++) 
  {
    gen_idx = genDataTable[i].gen_idx; 
    //data is completed for generator skipping
    if(genDataTable[i].cmon_session_fd == -2)
      continue;
    //check if cmon_session_fd is connected if not then connect.
    if(genDataTable[i].cmon_session_fd <= 0)
    {
      //start session.
      genDataTable[i].cmon_session_fd = nslb_cmon_start_session(usedGenTable[gen_idx].gen_ip, usedGenTable[gen_idx].cmon_port, err_msg);
      //if failed to initialize then just return with error code.
      if(genDataTable[i].cmon_session_fd <= 0)
      {
        NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_ERROR, "nslb_cmon_start_session() failed for cmon(%s:%d), error = %s", usedGenTable[gen_idx].gen_ip, usedGenTable[gen_idx].cmon_port, err_msg);
        //return 0;
        //If not able to make connction to a generator even then continue with other generators
        continue;
      }
    }

    //now get file size.
    gen_offset = nslb_cmon_get_file_size(usedGenTable[gen_idx].gen_ip, genDataTable[i].cmon_session_fd, genDataTable[i].data_file_path, err_msg);
    //check if failed.
    if(gen_offset < 0)
    {
      NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_ERROR, 
                      "Closing session with CavMonAgnet(IP: %s, Port: %d) because command nslb_cmon_get_file_size() failed for file = %s. "
                      "%s", 
                       usedGenTable[gen_idx].gen_ip, usedGenTable[gen_idx].cmon_port, genDataTable[i].data_file_path, err_msg);
      //if command will fail then it will close the connection internally.
      /* Here we cannot set cmon_session_fd with -1 because if any generator is killed then it make more connections continously.
      */

      genDataTable[i].cmon_session_fd = -1;
      //return 0;
      continue;
    }

    //Even if single generator has some data then continue reading
    if(gen_offset > genDataTable[i].offset)
    {
      NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, "Offset (%lld) is less than file size(%lld) on generator, file name = %s", genDataTable[i].offset, gen_offset, genDataTable[i].data_file_path);
      return 0;
    }
    else /*closing fd as file download completed or offset is greater than filesize*/
    {
      NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, "Offset (%lld) matched from file size(%lld) on generator, file name = %s. Data is completed", genDataTable[i].offset, gen_offset, genDataTable[i].data_file_path);
      close(genDataTable[i].cmon_session_fd);
      //data is completed for generator
      genDataTable[i].cmon_session_fd = -2;
    }
  }
  return 1;
}
/*In Offline Partition mode we need to update partition index with current partition*/
static void update_test_run_info_shm(char *next_partition)
{
  testrun_info_table->partition_idx = atoll(next_partition);
  strcpy(testrun_info_table->partition_name, next_partition);
}

static int nia_get_next_task (char *thread_str, int partition_flag, long long cur_partition_idx, char *next_partition, GenData *genDataTable)
{
  int ret;
  char cur_partition_str[32];

  NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, "Method called");

  if(partition_flag)
    sprintf(cur_partition_str, "%lld", cur_partition_idx);

  //init next_partition buffer.
  if(next_partition) next_partition[0] = 0;

  //First we will check if data size is same as on generator.
  //if not then we will continue to read the same file.
  ret = is_data_completed(thread_str, genDataTable);
  if(ret == -1)
  {
    NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_ERROR, "is_data_completed() failed");
    return NIA_ERROR;
  }
  //current partition not done. 
  if(ret == 0)
  {
    NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, "%s: Complete data not received from all the generators"
                   " , continue processing.", online_mode?"Online mode":"Offline mode");
    return NIA_CONTINUE;
  }
      
  //Offline mode.
  if(!online_mode)
  {
    if(partition_flag) {
      //Check for next partition.
      ret = nslb_get_next_partition_ex(base_dir, cur_partition_str, next_partition);
      //Note this function can return 0, 1(no next partition), -1(error)
      if(ret < 0) {
        NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, "nslb_get_next_partition() Failed");
        return NIA_ERROR; 
      }
      if(next_partition[0]) 
      { 
        NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, "Offline mode: Partition switched from %s to %s", cur_partition_str, next_partition);
        update_test_run_info_shm(next_partition);//Set current partition in shared memory 
        return NIA_PARTITION;
      }  
      else {  //No next partition.
          NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, "Offline mode: No more partition remained, last partition was %s", cur_partition_str);
        return NIA_STOP;
      }   
    }
    else {
      NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, "Offline mode: No more data remain in dlog file");
      return NIA_STOP;
    }
  }
  else {  //Online mode.
    //If test stop then stop thread.
    if(is_test_running == 0) {
      NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, "Online mode: test run stopped, stopping thread.");
      return NIA_STOP;
    }
   
    if(partition_flag) {
      //If partition change in shared memory then check for next partition.
      if(cur_partition_idx != testrun_info_table->partition_idx)
      {
        //we will check for next partition only if current partition have been completed.
        //Issue: what if current partition is bad partition. 
        //Note this function can return 0, 1(no next partition), -1(error)
        ret = nslb_get_next_partition_ex(base_dir, cur_partition_str, next_partition);
        if(ret < 0) {
          NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_ERROR, "nslb_get_next_partition() Failed\n");
          return NIA_ERROR; 
        }
        if(next_partition[0]) 
        {
          NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, "Online mode: Partition switched from %s to %s", cur_partition_str, next_partition);
          return NIA_PARTITION;
        }  
        else {  //No next partition.
          //TODO: check if this case can happen.
          NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, "Online mode: No more partition remained last partition was %s", cur_partition_str);
          return NIA_CONTINUE;
        }   
      }
      //Not changed, continue.
      else {
        NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, "Online mode: partition not switched and test is also running, waiting for data");
        return NIA_CONTINUE;  
      }
    }
    //Non partition flag.
    else {
      NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, "Online mode: waiting for data to come in ");
      return NIA_CONTINUE;
    }
  } 
  return NIA_ERROR;
}

void reset_gentable_and_close_conn(int thread_mode, GenData *genDataTable, int block_size, int *epoll_fd, int free_partial_buffer)
{
  int i;
  char err_msg[1024] = "";
   
  for( i=0; i<total_generator_entries; i++)
  {
    remove_select(genDataTable[i].fd, *epoll_fd); 
    if(genDataTable[i].fd != -1) 
      close(genDataTable[i].fd);

    genDataTable[i].offset = 0;
    genDataTable[i].read_amount = 0;
    genDataTable[i].fd = -1;

    if(free_partial_buffer) {
      free(genDataTable[i].partial_buf.buf);
      genDataTable[i].partial_buf.buf = NULL;
    }
    else
      memset(genDataTable[i].partial_buf.buf, 0, block_size);

    //close all cmon_session.fd
    if(genDataTable[i].cmon_session_fd > 0)
    {
      nslb_cmon_stop_session(usedGenTable[genDataTable[i].gen_idx].gen_ip, genDataTable[i].cmon_session_fd, err_msg);
      genDataTable[i].cmon_session_fd = -1;
    }
    if(dump_raw_data && thread_mode != SLOG_THREAD)
    {
      close(genDataTable[i].raw_data_fd);
    }
  }
}

//Note: if append_mode is not set then file will not be open in append mode.
static inline void open_controller_data_file(char *thread_str, int partition_mode, char *controller_data_file, int *controller_data_fd, int thread_mode, long long partition_idx, int append_mode, long long block_size)
{
   NSLB_TRACE_LOG2(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, "Method called");

   if(thread_mode == DLOG_THREAD){
     if(partition_mode)
       sprintf(controller_data_file, "%s/%lld/reports/raw_data/dlog", base_dir, partition_idx);
     else 
       sprintf(controller_data_file, "%s/reports/raw_data/dlog", base_dir);
   }
   else if(thread_mode == SLOG_THREAD){  //the last one slog
     if(g_partition_mode) //because this thread will run in non partition mode.
     {
       sprintf(controller_data_file, "%s/%lld/reports/raw_data/slog", base_dir, global_start_partition_idx);
     }
     else
       sprintf(controller_data_file, "%s/reports/raw_data/slog", base_dir);
   }
   else if(thread_mode == RBU_LIGHTHOUSE_THREAD){
     if(partition_mode)
       sprintf(controller_data_file, "%s/%lld/reports/csv/RBULightHouseRecord.csv", base_dir, partition_idx);
     else
       sprintf(controller_data_file, "%s/reports/csv/RBULightHouseRecord.csv", base_dir);
   }
   else {  //For RBU access Log
     if(partition_mode)
       sprintf(controller_data_file, "%s/%lld/rbu_logs/rbu_access_log", base_dir, partition_idx);
     else
       sprintf(controller_data_file, "%s/rbu_logs/rbu_access_log", base_dir);
   }

    //open data file on controller.
    int mode = O_CREAT|O_WRONLY |O_LARGEFILE|O_CLOEXEC;
    if(append_mode > 0)
      mode |= O_APPEND;
     
    if((*controller_data_fd = open(controller_data_file, mode, 00666)) == -1){
      NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_ERROR, "Error in opening data file [%s], Error- [%s]", controller_data_file, nslb_strerror(errno));
      exit(1); 
    }
    
    if(thread_mode != SLOG_THREAD && append_mode)
    {
      struct stat fdstat;
      //check if we have complete no of blocks.
      //get file size.
      if(fstat(*controller_data_fd, &fdstat))
      {
        NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_ERROR, "Error in lstat() data file [%s], Error- [%s]", controller_data_file, nslb_strerror(errno));
        exit(1);
      }
      //check if we have complete bytes.
      if((fdstat.st_size % block_size)) 
      {
        long long complete_block_size = ((fdstat.st_size / block_size) * (block_size));
        NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, "data file [%s] don't have complete number of blocks(bs-%lld), truncating file upto complete blocks(%lld)", controller_data_file, block_size, complete_block_size);
        //truncate to complete blocks.
        if(ftruncate(*controller_data_fd, complete_block_size))
        {
          NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_ERROR, "ftruncate() failed for data file [%s], Error - [%s]", controller_data_file, nslb_strerror(errno));
          exit(1);
        }
      }
    } 
}

//this will return complete line bytes.
static inline void get_complete_line_bytes(char *buf, int size, int *complete_line_size)
{
  int tmp_line_length = size;  
  int i;

  if(buf)
  {
    for(i = size; i > 0; i--){ 
      if(buf[i - 1] != '\n')
      {
        tmp_line_length --;
      }
      else
        break;
    }
    *complete_line_size = tmp_line_length;
  }
  else
    *complete_line_size = 0;
}

void *nfa_thread_callback(void *thread_data)
{
  int off_fd, i;
  char controller_data_file[512];
  int controller_data_fd;
  DataOffsetInfo data_offset_info;
  int thread_idx = *(int *)thread_data;
  char *thread_str = thread_name[thread_idx + 1];
  int thread_mode;
  int nfa_epoll_fd;
  GenData *genDataTable;
  int block_size;
  int l_partition_mode = g_partition_mode; 
  time_t last_recovery_time = 0; 

  NSLB_TRACE_LOG2(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, "Method called, thread_idx = %d", thread_idx);

  if(thread_idx  == SLOG_THREAD){
    //this thread will also work in non partition mode because we will read slog from the same location till then end.
    l_partition_mode = 0;
    thread_mode = SLOG_THREAD;
    block_size = 1024*1024;  //TODO: make it configurable.
  }
  else if(thread_idx  == DLOG_THREAD){ 
    thread_mode = DLOG_THREAD;
    block_size = dlog_bufsize;
  }
  else if(thread_idx  == RBU_ACCESS_LOG_THREAD){
    thread_mode = RBU_ACCESS_LOG_THREAD;
    block_size = rbu_acc_log_bufsize; 
  }
  else if(thread_idx  == RBU_LIGHTHOUSE_THREAD){
    thread_mode = RBU_LIGHTHOUSE_THREAD;
    block_size = rbu_acc_log_bufsize; 
  }

  data_offset_info.partition_idx = -1;
  data_offset_info.offset = (long long*)malloc(total_generator_entries * sizeof(long long));
  memset(data_offset_info.offset, 0, total_generator_entries * sizeof(long long));

  //read offset file and fill in structure DataOffsetInfo.
  check_and_set_offset_file(thread_str, thread_mode, &off_fd, &data_offset_info);

  //check if partition found or not.
  if(l_partition_mode && data_offset_info.partition_idx <= 0)
  {
    //now get first partition.
    data_offset_info.partition_idx = get_start_partition(thread_str);
    //update offset file.
  }
  write_offset_info_to_file(thread_str, off_fd, &data_offset_info);

  
  nfa_epoll_fd = epoll_create(total_generator_entries); //creating epoll for collector
  if(nfa_epoll_fd == -1)
  {
    NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_ERROR, "Error in creating epoll fd, Error- [%s]", nslb_strerror(errno));
    exit(1);
  }

  //malloc genData.
  NSLB_MALLOC (genDataTable, total_generator_entries * sizeof(GenData), "genDataTable", -1, NULL);
  memset(genDataTable, 0, total_generator_entries * sizeof(GenData));

  //Note: data file on controller should be open first.
  open_controller_data_file(thread_str, l_partition_mode, controller_data_file, &controller_data_fd, thread_mode, data_offset_info.partition_idx, 1, block_size); 
   
  data_offset_info.partition_idx = global_partition_idx; 
  //open data files and create connection to cavmon.
  setup_gentable_make_conn(thread_str, thread_mode, l_partition_mode, genDataTable, &data_offset_info, nfa_epoll_fd, block_size);

  int events;
  int read_amt;
  int ret;
  char *read_buffer = NULL;
  char *read_ptr;
  struct epoll_event *nfa_fds;
  char next_partition[32] = "";
  int switch_partition = 0;
  int ret_event;

  /*If epoll count is equal to 10 mins then max epoll timeout has been reached*/ 
  short epoll_timeout_max_cnt = (10 * 60 * 1000)/(nifa_settings.epoll_wait * 1000) ;
  //allocate read_buffer.
  //only if thread_mode is slog.
  if(thread_mode == SLOG_THREAD)
  {
    NSLB_MALLOC(read_buffer, block_size, "read_buffer", -1, NULL);
    read_ptr = read_buffer;
  }
  last_recovery_time = time(NULL);  

  int rbu_acc_log_hdr_found = 0;  
  NSLB_MALLOC(nfa_fds, sizeof(struct epoll_event) * total_generator_entries, "nfa_event", -1, NULL); //malloc for events
  while(1){ //infinite loop waiting for events
    if(switch_partition)
    {
      NSLB_TRACE_LOG1(nfa_trace_log_key, atoll(next_partition), thread_str, NSLB_TL_INFO, "Switching partition from %lld to %s",
                     data_offset_info.partition_idx, next_partition);
      //close all the previous connection fo cavmon, remove fds from epoll.
      reset_gentable_and_close_conn(thread_mode, genDataTable, block_size, &nfa_epoll_fd, 0);
     
      //close previous controller_data_file.
      close(controller_data_fd);
     
      //set data_offset_info.
      data_offset_info.partition_idx = atoll(next_partition);
      memset(data_offset_info.offset, 0, sizeof(long long)* total_generator_entries);

      setup_gentable_make_conn(thread_str, thread_mode, l_partition_mode, genDataTable, &data_offset_info, nfa_epoll_fd, block_size);
      open_controller_data_file(thread_str, l_partition_mode, controller_data_file, &controller_data_fd, thread_mode, data_offset_info.partition_idx, 0, block_size);

      switch_partition = 0; 
    }

    //check for recovery of cmon connection.
    if((time(NULL) - last_recovery_time) >= nifa_settings.recovery_interval)
    {
      cmon_con_recovery(thread_str, thread_mode, genDataTable, &data_offset_info, nfa_epoll_fd, block_size);    
      //update last_recovery_time.
      last_recovery_time = time(NULL);
    }

    NSLB_TRACE_LOG3(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, "going for epoll wait for time = %d(sec)", nifa_settings.epoll_wait);

    ret_event = epoll_wait(nfa_epoll_fd, nfa_fds, total_generator_entries, nifa_settings.epoll_wait * 1000); //waiting indefinetly....what timout should be given
    NSLB_TRACE_LOG2(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, "return event = [%d]", ret_event);
    if(ret_event < 0){
      NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, "Unable to get event. Error- [%s]", nslb_strerror(errno));
      continue; //or exit
    }
    if(ret_event == 0)
    {
      epoll_timeout_cnt++;
      NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_ERROR, "epoll_timeout_cnt = %d", epoll_timeout_cnt);
      ret = nia_get_next_task (thread_str, l_partition_mode, data_offset_info.partition_idx, next_partition, genDataTable);  
      if(ret == NIA_ERROR)
       exit(1);
      else if(ret == NIA_CONTINUE) 
      {
      /*OFFLINE mode: Introduce max epoll timeout, NIFA might stuck in epoll wait and then process continues to run for indefinite time*/   
        if (online_mode == 0) { 
          if(epoll_timeout_cnt <= epoll_timeout_max_cnt) {
            continue;
          } else {
            NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_ERROR, "epoll_wait timeout cur count reached to its max limit = %d", epoll_timeout_max_cnt);
            fprintf(stderr, "Error: NIFA Epoll timeout, epoll_wait timeout cur count reached to its max limit = %d\n", epoll_timeout_max_cnt);
            break;  
          }
        } else //In Online mode, timeout not required
          continue;
      } else if(ret == NIA_STOP) {
        break;
      }
      else if (ret == NIA_PARTITION)
      {
        switch_partition = 1;
      }
    }
    epoll_timeout_cnt = 0;/* Reset epoll timeout count to 0 as we has to track only continues timeout*/
    for(i = 0; i<ret_event; i++){

      GenData *ptr = (GenData *)nfa_fds[i].data.ptr;
      events = nfa_fds[i].events;
      if(events == EPOLLIN){

       while(1){
        //In case of dlog we will use partial_buf assigned to each gen.
        //And in case of slog we will use read_buffer.
        NSLB_TRACE_LOG3(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, 
                        "thread_mode = %d, ptr->read_amount = %d", thread_mode, ptr->read_amount);

        if(thread_mode == DLOG_THREAD)
          read_ptr = ptr->partial_buf.buf + ptr->read_amount;
        else if(thread_mode == SLOG_THREAD) {
          read_ptr = read_buffer;
          //check if partial data exist then first copy this partial data to buffer.
          if(ptr->read_amount)
          {
            memcpy(read_ptr, ptr->partial_buf.buf, ptr->read_amount);
            read_ptr += ptr->read_amount;
          }
        }
        else if(thread_mode == RBU_ACCESS_LOG_THREAD || thread_mode == RBU_LIGHTHOUSE_THREAD)
          read_ptr = ptr->partial_buf.buf + ptr->read_amount;
 
        //FIXME:we are controlling read bytes from ptr->read_amount. 
        NSLB_TRACE_LOG3(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, 
                         "Going to read data from gen id = %d, previous ptr->read_amount = %d, available space = %d, read_ptr = %p",
                          ptr->gen_idx, ptr->read_amount, block_size - ptr->read_amount, read_ptr); 

        read_amt = read(ptr->fd, read_ptr, block_size - ptr->read_amount);

        NSLB_TRACE_LOG3(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, 
                        "Read data: gen_id = %d, fd = %d, read_amt = [%d], ptr_read_amt = [%d], block_size = %d, offset = %lld, data = %s", 
                         ptr->gen_idx, ptr->fd, read_amt, ptr->read_amount, block_size, ptr->offset, read_ptr);

        if(read_amt == 0){
          NSLB_TRACE_LOG2(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, "Warning: Connection closed for fd = %d", ptr->fd);

           //don't kill me i can recovered.
           remove_select(ptr->fd, nfa_epoll_fd); 
           close(ptr->fd);
           //remove from epoll.
           ptr->fd = -1;
           break; 
        }else if(read_amt < 0){
          NSLB_TRACE_LOG3(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, "errno = %d", errno);
          if(errno == EAGAIN)
          {
            break;
          }
          else if(errno == EINTR)
            continue;
        
          NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, 
                          "Warning: read failed for fd =%d, strerror = %s", ptr->fd, nslb_strerror(errno));
          //remove from epoll.
          remove_select(ptr->fd, nfa_epoll_fd); 
          close(ptr->fd);
          ptr->fd = -1;
          break; 
        }
        
        ptr->read_amount += read_amt;
        //update offset on each read.
        ptr->offset += read_amt;
 
        //write this to raw_data_fd.
        if(dump_raw_data && thread_mode != SLOG_THREAD)
        {
          int ret = write(ptr->raw_data_fd, read_ptr, read_amt);
          if(ret != read_amt)
          {
            NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_ERROR, "write failed for raw_data fd");
            exit(-1);
          }
        }

/*****process data**********/
        if(thread_mode == DLOG_THREAD)
        {
          if(ptr->read_amount == block_size)
          {
             NSLB_TRACE_LOG4(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, "Complete block received.");
             nia_write_to_file(thread_str, ptr, ptr->partial_buf.buf, ptr->read_amount, controller_data_fd, controller_data_file);
             ptr->read_amount = 0;
             //update offset files.
             //TODO: do we need to write it in each block read.
             data_offset_info.offset[ptr->gen_idx] += block_size;         
             write_offset_info_to_file(thread_str, off_fd, &data_offset_info);      
          } 
        }
        else if(thread_mode == SLOG_THREAD)
        {
          //just write complete lines to slog files.
          //first get complete lines.
          int complete_line_size;
          get_complete_line_bytes(read_buffer, ptr->read_amount, &complete_line_size);
          //now just copy complete bytes to data files and update offset.
          nia_write_to_file(thread_str, ptr, read_buffer, complete_line_size, controller_data_fd, controller_data_file);
          //copy rest of data to partial buf.
          memcpy(ptr->partial_buf.buf, read_buffer + complete_line_size, (ptr->read_amount - complete_line_size));
          ptr->read_amount = (ptr->read_amount - complete_line_size);
          //update offset file
          data_offset_info.offset[ptr->gen_idx] += complete_line_size; 
          write_offset_info_to_file(thread_str, off_fd, &data_offset_info);
        }
        else if(thread_mode == RBU_ACCESS_LOG_THREAD)
        {
          char *line = NULL;
          int lsize = 0;
          line = ptr->partial_buf.buf;

          //Initially : rbu_acc_log_hdr_found = 0
          //If rbu_acc_log_hdr_found is unset : compares ptr->partial_buf.buf with Header intial, if header found, sets rbu_acc_log_hdr_found
          //If rbu_acc_log_hdr_found is set : compares ptr->partial_buf.buf with Header intial,if header found 
          //            checks for new line, increment pointer to next bit, 
          //            calculate size which need to be reduced, and decrease the same amount from ptr->read_amount
          NSLB_TRACE_LOG4(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, 
                           "Complete block received., rbu_acc_log_hdr_found = %d", 
                            rbu_acc_log_hdr_found);
          
          if(!rbu_acc_log_hdr_found)
          {
            if(!strncmp(ptr->partial_buf.buf, "#ClientIp", 9)) //sizeof(#ClientIp) = 9 
            {
              NSLB_TRACE_LOG4(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, "RBU Access log header found");
              rbu_acc_log_hdr_found = 1;
            }
          }
          else
          {
            if(!strncmp(ptr->partial_buf.buf, "#ClientIp", 9)) //sizeof(#ClientIp) = 9 
            {
              NSLB_TRACE_LOG4(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, 
                               "RBU Access log header found for gen id = %d,  ptr->read_amount = %d",   
                                ptr->gen_idx, ptr->read_amount);
              if((line = strchr(ptr->partial_buf.buf, '\n')) != NULL)
              {
                line++;
                lsize = line - ptr->partial_buf.buf;
                ptr->read_amount -= lsize;
              } 

              NSLB_TRACE_LOG4(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, 
                              "lsize = %d, ptr->read_amount = %d", lsize, ptr->read_amount);
            }
          }

           NSLB_TRACE_LOG4(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, 
                           "Dump RBU Access log data for gen id %d of size %d into file", 
                            ptr->gen_idx, ptr->gen_idx, ptr->read_amount);

          nia_write_to_file(thread_str, ptr, line, ptr->read_amount, controller_data_fd, controller_data_file);
          ptr->read_amount = 0;
          //update offset files.
          //TODO: do we need to write it in each block read.
          data_offset_info.offset[ptr->gen_idx] += block_size;
          write_offset_info_to_file(thread_str, off_fd, &data_offset_info);
        }
        else if(thread_mode == RBU_LIGHTHOUSE_THREAD)
        {
           NSLB_TRACE_LOG4(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, "Complete block received.");
           nia_write_to_file(thread_str, ptr, ptr->partial_buf.buf, ptr->read_amount, controller_data_fd, controller_data_file);
           ptr->read_amount = 0;
           data_offset_info.offset[ptr->gen_idx] += block_size;         
           write_offset_info_to_file(thread_str, off_fd, &data_offset_info);
        }
       }
     }
   }//end of for loop for all events
   if(sigterm_recieved)
   {
     NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_str, NSLB_TL_INFO, "SIGTERM Recieved, exiting from this thread");
     break;
   }
 }// end of while loop 
 reset_gentable_and_close_conn(thread_mode, genDataTable, block_size, &nfa_epoll_fd, 1);
 //close epoll fd.
 close(nfa_epoll_fd);
 //TODO: free allocated memory.
 return NULL;
}

//used to decide whether to recover component or not 
ComponentRecoveryData ReaderRecoveryData; 
#define MAX_RECOVERY_COUNT                5  //used to decide whether to recover component or not
#define RECOVERY_TIME_THRESHOLD_IN_SEC   10
#define RESTART_TIME_THRESHOLD_IN_SEC   900
#define MAX_FILENAME_LENGTH 1024
 
int start_logging_reader (int testidx, int debug_log_value, int reader_run_mode, int reader_write_time, int DynamicTableSize, int generator_id, int cav_epoch_yr, long test_run_info_shm_id, int is_recovery, int num_nvm, int num_gen, int loader_opcode, int dyn_tx_tb_size)
{
    char shr_mem_new_tr_key[64];

    sprintf(shr_mem_new_tr_key, "%d", test_run_info_id);
    NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_name[0], NSLB_TL_INFO, "%s logging reader", is_recovery?"Recovering":"Starting");
    //TODO call nslb_debug_log_ex
   
    if ((logging_reader_pid = fork()) ==  0 ) 
    {
      nslb_close_all_open_files(-1, 0, NULL);
      char lr_bin_path[512 + 1] = "";
      char trace_level_reader[8];
      char deliminator_reader[8];
      char test_run_num[1024];
      char reader_write_time_str[1024]; //no use
      char reader_running_mode[8];
      char dynamic_t_size[16];
      char dynamic_tx_tb_size[16];
      char generator_id_buf[125];
      char cav_epoch_year[125];
      char shr_buff_size_str[32];
      char num_nvm_str[16];
      char num_gen_str[16];
      char loader_opcode_val[5];
      sprintf(trace_level_reader, "%d", debug_log_value);
      sprintf(deliminator_reader, ",");
      sprintf(test_run_num, "%d", testidx);
      sprintf(reader_running_mode, "%d", reader_run_mode);
      sprintf(reader_write_time_str, "%d", reader_write_time);
      sprintf(dynamic_t_size, "%d", DynamicTableSize);
      sprintf(dynamic_tx_tb_size, "%d", dyn_tx_tb_size); 
      sprintf(generator_id_buf, "%d", generator_id);
      sprintf(cav_epoch_year, "%d", cav_epoch_yr);
      sprintf(shr_buff_size_str, "%d", dlog_bufsize);
      sprintf(num_nvm_str, "%d", num_nvm);
      sprintf(num_gen_str, "%d", num_gen);
      sprintf(loader_opcode_val, "%d", loader_opcode);

      sprintf(lr_bin_path, "%s/bin/nsu_logging_reader", ns_wdir);
 
      //if (execlp("nsu_logging_reader", "nsu_logging_reader", "-l", test_run_num, "-D", trace_level_reader, "-W", wdir, "-m", reader_running_mode, "-Z", dynamic_t_size, "-k", shr_mem_new_tr_key, "-g", generator_id_buf, "-f", "1", "-e", cav_epoch_year, NULL) == -1) 
      if (execlp(lr_bin_path, "nsu_logging_reader", "-l", test_run_num, "-T", trace_level_reader, "-W", ns_wdir, "-m", reader_running_mode, "-Z", dynamic_t_size, "-k", shr_mem_new_tr_key, "-g", generator_id_buf,  "-S" , shr_buff_size_str, "-n", num_nvm_str, "-G", num_gen_str, "-o", loader_opcode_val, "-X", dynamic_tx_tb_size, NULL) == -1)  //TODO: KRISHNA..NEED TO PAAS TRACE
      {
        fprintf(stderr, "Initializing logging reader: error in execl\n");
        perror("execl");
        exit(-1);
      }
    } 
    else 
    {
      if (logging_reader_pid < 0) 
      {
        NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_name[0], NSLB_TL_ERROR, "Unable to start Logging reader due to error in execl");
        fprintf(stderr, "error in forking the logging_reader process\n");
        //in case of recovery, dont exit if fork() fails
        if(is_recovery)
          return -1;
        else
          exit(-1);
      }
      else
      {
        NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_name[0], NSLB_TL_INFO, "Logging reader process created(fork) with pid %d", logging_reader_pid);
      }
    }

  if(online_mode)
    testrun_info_table->ns_lr_pid = logging_reader_pid;
  return 0;
}

static void check_and_start_logging_reader()
{
  if(nslb_recover_component_or_not(&ReaderRecoveryData) == 0)
  { 
    NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_name[0], NSLB_TL_INFO, "Logging reader not running. Recovering logging reader.");

    //start logging reader in online mode irrespective of reader_run_mode 
    if(start_logging_reader(test_run_num, nlr_trace_level, 1/*reader_run_mode*/, reader_write_time, dynamic_table_size, generator_id, cav_epoch_year, test_run_info_id, 1, num_nvm, num_gen, loader_opcode, dynamic_tx_table_size) == 0) 
    {
      NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_name[0], NSLB_TL_INFO, "Logging reader recovered, Pid = %d.", logging_reader_pid);
    }
    else
    {
      NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_name[0], NSLB_TL_ERROR, "Logging reader recovery failed");
    }
  }
  else
  {
    NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_name[0], NSLB_TL_INFO, "Logging Reader max restart count is over. "
     "Cannot recover LR Retry count = %d, Max Retry count = %d", ReaderRecoveryData.retry_count, ReaderRecoveryData.max_retry_count);
  }
}

void do_handle_sigchld()
{
  int status;
  int ret;

  NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_name[0], NSLB_TL_INFO, "Signal SIG_CHLD received");

  if(online_mode) 
  {
    //Issue: actually it was hanging no reason..
    ret = waitpid(-1, &status, WNOHANG);

    if(ret == logging_reader_pid)
    {
      testrun_info_table->ns_lr_pid = -1;
      logging_reader_pid = -1;

      NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_name[0], NSLB_TL_INFO, "SIG_CHLD received from Logging Reader");
      check_and_start_logging_reader();
    }
  }
  else
  {
    NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_name[0], NSLB_TL_INFO, "SIG_CHLD received in offline mode. This should not happen.");
  }

  sigchld_sig = 0;
}

void parse_args(int argc, char **argv){

  int c;
  int t_flag = 0, k_flag = 0,  m_flag = 0, num_nvm_flag = 0;

  if(argc < 2){
    fprintf(stderr,  "Missing Mandatorey arguments. Usage nia_file_aggregator -t [Test Run Number] -k [Shared Memory Key] -p [percentile mode]\n");
    exit(1);
  }

  struct option longopts[] = {
                               {"testrun", 1, NULL, 't'},
                               {"shmkey", 1, NULL, 'k'},
                               {"readermode", 1, NULL, 'm'},
                               {"ppid", 1, NULL, 'P'},
                               {"dlog_bufsize", 1, NULL, 'b'},
                               {"trace_level", 1, NULL, 'l'},
                               {"lr_trave_level", 1, NULL, 'L'},
                               {"reader_write_time", 1, NULL, 'T'},
                               {"dynamic_table_size", 1, NULL, 'D'},
                               {"gen_id", 1, NULL, 'g'},
                               {"cav_epoch_year", 1, NULL, 'c'},
                               {"num_nvm", 1, NULL, 'n'},
                               {"num_gen", 1, NULL, 'G'},
                               {"resume", 1, NULL, 'r'},
                               {"loader_opcode", 1, NULL, 'o'},
                               {"rbu_access_log", 1, NULL, 'R'},
                               {"dynamic_tx_table_size", 1, NULL, 'X'},
                               {0, 0, 0, 0}
                             };

  while ((c = getopt_long(argc, argv, "t:k:m:p:P:B:b:l:L:T:D:g:c:n:G:r:o:R:H:X:", longopts, NULL)) != -1)
  {
    switch(c){
      case 't':
        test_run_num = atoi(optarg); 
        t_flag = 1;
        break;
      case 'l':
        trace_level = atoi(optarg);
        break;
      case 'L':
        nlr_trace_level = atoi(optarg);
        break;
      case 'T':
        reader_write_time = atoi(optarg);
        break;
      case 'D': 
        dynamic_table_size = atoi(optarg);
        break;
      case 'g':
        generator_id = atoi(optarg);
        break;
      case 'c':
        cav_epoch_year = atoi(optarg);
        break;
      case 'n':
        num_nvm = atoi(optarg);
        num_nvm_flag  = 1;
        break;
      case 'G':
        num_gen = atoi(optarg);
        break;
      case 'k':
        test_run_info_id = atoi(optarg);
        k_flag = 1;
        break;
      case 'm':
        reader_run_mode = atoi(optarg);
        if(reader_run_mode > 0)
          online_mode = 1;
        m_flag = 1;
        break;
      case 'P':
        parent_pid = atoi(optarg);
        break;
      case 'b':
        dlog_bufsize = atoi(optarg);
        break;
      case 'r':
        resume_flag = atoi(optarg);
        break;
      case 'o':
        loader_opcode = atoi(optarg);
        break;
      case 'R':
        rbu_access_log = atoi(optarg);
        break;
      case 'H':
        rbu_lighthouse_enabled = atoi(optarg);
        break;
      case 'X':
        dynamic_tx_table_size = atoi(optarg);
        break;

      case '?':
        fprintf(stderr, "Usage: ./nia_file_aggregator -t [Test Run Number]  -k [Shared Memory Key] -m [Reader Run Mode -p [percentile mode]\n");
        exit(-1);
    }
  }
  
  if(!(t_flag && k_flag && m_flag && num_nvm_flag)){
    fprintf(stderr, "All arguments are mandatory. Usage: nia_file_aggregator -t [Test Run Number]  -m [Reader Run Mode] -n [num nvm]\n");
    exit(1);
  }
  
  //if online mode then need to specify parent pid.
  if(online_mode && (parent_pid == -1)) 
  {
    fprintf(stderr, "Error: nia_file_aggregator, --ppid argument missing.(required in case of readermode 1)\n");
    exit(1);
  }
}

/*At init time fill partition_idx with first partition index*/
static void init_test_run_info_shm()
{
  if (g_partition_mode > 0) {
    testrun_info_table->partition_idx = curPartInfo.first_partition_idx;
    sprintf(testrun_info_table->partition_name, "%lld", testrun_info_table->partition_idx);
  }
  testrun_info_table->reader_run_mode = online_mode;//Actual reader run mode
}

static int create_test_run_info()
{
  int share_mem_fd;

  if ((share_mem_fd = shmget(IPC_PRIVATE , sizeof(TestRunInfoTable_Shr), IPC_CREAT | IPC_EXCL | 0666)) == -1) {
    perror("error in allocating shared memory for NS parent and logging writer");
    return -1;
  }
  testrun_info_table = shmat(share_mem_fd, NULL, 0);
  if (testrun_info_table == NULL) {
    perror("error in getting shared memory for NS parent and logging writer");
    return -1;
  }

  if (shmctl(share_mem_fd, IPC_RMID, NULL)) {
    printf ("ERROR: unable to mark shm removal for 'NS parent and logging writer' of size=%lu err=%s\n",
                (u_ns_ptr_t)sizeof(int), nslb_strerror(errno));
    return -1;
  }
  return share_mem_fd; //success case
}


//This will set partition mode.
void get_set_partition_idx(char *base_dir)
{

  int ret;

  if(online_mode)
  {
    testrun_info_table = (TestRunInfoTable_Shr *)shmat(test_run_info_id, NULL, 0);
    if (testrun_info_table == (void *)-1) 
    {
      perror("nia_file_aggregator: error in attaching shared memory");
      exit (-1);
    }
    //set trace level.
    if(testrun_info_table->partition_idx > 0)
    { 
      global_partition_idx = testrun_info_table->partition_idx;
      g_partition_mode = 1;
    }
    trace_level = testrun_info_table->nifa_trace_level;
  }
  else {
    //get current partition.
    //if cur_partition_info not present then it means it is not partition mode.
    //read partition info of that partition.
    if((ret = nslb_get_cur_partition_file_ex(&curPartInfo, base_dir)) != -1){ //If cur part file present we are treating test as in partition mode
      g_partition_mode = 1;
  
      global_partition_idx = curPartInfo.cur_partition_idx;
    }
    else{ //error in reading current partition file
      fprintf(stderr, "Error reading Current partition file, treating test as non partition mode\n");
    } 
    /*In Offline(Partition/Non-Partition) test NIFA keeps merging dlog and slog data consequently therefore 
    logging reader needs to run online now it requires TestRunInfo shared memory segment, hence we need to 
    create shared memory segment and pass key.*/
    //testrun_info_table = nslb_do_shmget_with_id((sizeof(TestRunInfoTable_Shr) * 1), "nia_file_aggregator", &test_run_info_id);
    test_run_info_id = create_test_run_info();
    if (test_run_info_id == -1) {
      perror("error in getting shared memory for nia_file_aggregator");
      exit (-1);
    }
    init_test_run_info_shm();
  }
}

void do_change_trace_level()
{
  nslb_change_atb_mt_trace_log(nfa_trace_log_key, "NFA_TRACE_LEVEL"); 
  trace_level = nfa_trace_log_key->log_level;
  NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, "Main thread", NSLB_TL_INFO, "Trace Level changed to %d", trace_level); 
  trace_level_change_sig = 0;
}

void do_sigalarm()
{
  if((parent_pid == -1) || !nslb_check_pid_alive(parent_pid)){
    NSLB_TRACE_LOG4(nfa_trace_log_key, global_partition_idx, "Main thread", NSLB_TL_ERROR, "Parent got killed");
    is_test_running = 0;
  }else {
    NSLB_TRACE_LOG4(nfa_trace_log_key, global_partition_idx, "Main thread", NSLB_TL_ERROR, "Parent is alive");
    is_test_running = 1;
  }
}

static inline void nia_save_pid() 
{
  int ret;
  ret = nslb_save_pid_ex(base_dir, "nifa", "nia_file_aggregator"); //save pid to file

  if(ret == 1){
     NSLB_TRACE_LOG2(nfa_trace_log_key, global_partition_idx, thread_name[0], NSLB_TL_INFO, "Prev nia_file_aggregator killed, pid saved");
  }
  else if(ret == 0){
     NSLB_TRACE_LOG2(nfa_trace_log_key, global_partition_idx, thread_name[0], NSLB_TL_INFO, "Prev file_aggregator was not running, pid saved");
  }
  else{
     NSLB_TRACE_LOG2(nfa_trace_log_key, global_partition_idx, thread_name[0], NSLB_TL_ERROR, "Error in saving nia_file_aggregator pid");
  }
}


//This will read sorted_scenario file and will read this kw.
static inline void get_nifa_settings()
{
  FILE *ss_fp = NULL;
  char ss_file[512];
  char buf[1024];
  char kw[512];
  int ret;
  /*settings*/
  int cmon_interval = 1;
  int cmon_count = 10;
  int cmon_bufsize = 16*1024;
  int cmon_debug_level = 0;
  int epoll_wait = 2;
  int recovery_interval = 30;
  int dlog_start_delay = 5;
  int lr_start_delay = 5;
   
  
  sprintf(ss_file, "%s/sorted_scenario.conf", base_dir);
  ss_fp = fopen(ss_file, "r");
  if(ss_fp == NULL)
  {
    NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_name[0], NSLB_TL_ERROR, "Failed to open sorted scenario file(%s), error = %s,  Exiting", ss_file, nslb_strerror(errno));
    exit(-1);
  }
  while(fgets(buf, 1024, ss_fp))
  {
    if(!strncmp(buf, "NIFA_CMON_SETTINGS", strlen("NIFA_CMON_SETTINGS")))
    {
      ret = sscanf(buf, "%s %d %d %d %d %d %d %d %d", kw, &cmon_interval, &cmon_count, &cmon_bufsize, &cmon_debug_level, &epoll_wait,
            &recovery_interval, &dlog_start_delay, &lr_start_delay);
      //It will set kws which are given rest will be set to defaults.
      if(ret >= 2)
      {
        //validate some fields.
        if((epoll_wait * 1000) <= cmon_interval)
        {
          NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_name[0], NSLB_TL_INFO, "cmon_interval(%d) can not be more than "
             "epoll_wait(%d), setting defalt values.", cmon_interval, epoll_wait);
          epoll_wait = 2;
          cmon_interval = 100;
        }
      }
    }
  }

  fclose(ss_fp);
  //setting values.
  nifa_settings.cmon_interval = cmon_interval;
  nifa_settings.cmon_count = cmon_count;
  nifa_settings.cmon_bufsize = cmon_bufsize;
  nifa_settings.cmon_debug_level = cmon_debug_level;
  nifa_settings.epoll_wait = epoll_wait;
  nifa_settings.recovery_interval = recovery_interval;
  nifa_settings.dlog_start_delay = dlog_start_delay;
  nifa_settings.lr_start_delay = lr_start_delay;
      
  NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_name[0], NSLB_TL_INFO, 
        "nifa_settings.cmon_interval = %d "
        "nifa_settings.cmon_count = %d "
        "nifa_settings.cmon_bufsize = %d "
        "nifa_settings.cmon_debug_level = %d "
        "nifa_settings.epoll_wait = %d "
        "nifa_settings.recovery_interval = %d "
        "nifa_settings.dlog_start_delay  = %d "
        "nifa_settings.lr_start_delay = %d ",
         nifa_settings.cmon_interval, nifa_settings.cmon_count, nifa_settings.cmon_bufsize, nifa_settings.cmon_debug_level,
         nifa_settings.epoll_wait, nifa_settings.recovery_interval, nifa_settings.dlog_start_delay, nifa_settings.lr_start_delay);
}

int main(int argc, char **argv)
{
  int ret;
  char gen_data_path[512] = "";
  struct sigaction sa;
  sigset_t sigset;
  pthread_t fa_thread[3];

  parse_args(argc, argv); //parsing arguements

  if(getenv("NS_WDIR") != NULL)
    sprintf(ns_wdir, "%s", getenv("NS_WDIR"));
  else
    sprintf(ns_wdir, "%s", "/home/cavisson/work/");

  sprintf(gen_data_path, "%s/logs/TR%d/NetCloud/NetCloud.data", ns_wdir, test_run_num);
  sprintf(base_dir, "%s/logs/TR%d", ns_wdir, test_run_num);


  get_set_partition_idx(base_dir); //get current partition idx

  //Now we are introducing 5 new debug level.(5-8) which will be set to enable raw_data duming.
  if(trace_level > 4)
  {
    dump_raw_data = 1;
    trace_level -= 4;
  }  

  //initialize trace log for nia_file_aggregator
  nfa_trace_log_key = nslb_init_mt_trace_log(ns_wdir, test_run_num, global_partition_idx, "ns_logs/nifa_trace.log", trace_level, trace_log_size);

  //set start partition idx for slog in partition mode
  //because in partition mode slog will be read from common_files/reports/raw_data/.
  //and file name will be slog.<start_partition>
  if(g_partition_mode)
    global_start_partition_idx = get_start_partition(thread_name[0]);

  //dump all the arguments.
  NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_name[0], NSLB_TL_INFO, "Arguments passed to program: testrun=%d, "
         "shmkey = %d, reader run mode = %d ppid = %d, dlog_bufsize = %d, resume_flag = %d, global_partition_idx = %lld, "
         "global_start_partition_idx = %lld, trace_level = %d, dump_raw_data = %d,  rbu_access_log = %d, rbu_lighthouse_enabled = %d",
         test_run_num, test_run_info_id, online_mode, parent_pid, dlog_bufsize, resume_flag,
         global_partition_idx, global_start_partition_idx, trace_level, dump_raw_data, rbu_access_log, rbu_lighthouse_enabled);

  //load nifa_settings.
  get_nifa_settings();
  
  //save pid.
  nia_save_pid();

  parse_and_fill_used_gen_struct(gen_data_path); //Filled the used generator list andtheir corresponding values

  //creating thread
  pthread_attr_t attr;
  pthread_attr_init(&attr);

  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIGALRM);
  sigaddset(&set, SIGTERM);
  sigaddset(&set, SIGINT);
  sigaddset(&set, TRACE_LEVEL_CHANGE_SIG);
  sigaddset(&set, TEST_POST_PROC_SIG);
  sigaddset(&set, SIGCHLD);
  pthread_sigmask(SIG_BLOCK, &set, NULL);
  
  int thread_id[NUM_THREADS];
  int i;

  for(i = 0; i < NUM_THREADS; i++)
  {
    //if dlog thread then wait for some delay.
    if(i == 1)
    {
      NSLB_TRACE_LOG2(nfa_trace_log_key, global_partition_idx, thread_name[0], NSLB_TL_INFO, "Sleeping for %d(sec) before starting %s", nifa_settings.dlog_start_delay, thread_name[i+1]);
      sleep(nifa_settings.dlog_start_delay);
    }

    //If tool is not called for RBU Module, then don't create RBU_ACCESS_LOG_THREAD
    if(i == RBU_ACCESS_LOG_THREAD)
    {
      if(!(rbu_access_log)){
        NSLB_TRACE_LOG2(nfa_trace_log_key, global_partition_idx, thread_name[0], NSLB_TL_INFO, 
                                "RBU_ACCESS_LOG_THREAD and rbu_access_log = %d", 
                                rbu_access_log);
        break;
      }
    } 

    //If lighthouse is not enabled, then don't create RBU_LIGHTHOUSE_THREAD
    if(i == RBU_LIGHTHOUSE_THREAD)
    {
      if(!(rbu_lighthouse_enabled)){
        NSLB_TRACE_LOG2(nfa_trace_log_key, global_partition_idx, thread_name[0], NSLB_TL_INFO, 
                                "RBU_LIGHTHOUSE_THREAD and rbu_lighthouse_enabled = %d", rbu_lighthouse_enabled);
        break;
      }
    }

    thread_id[i] = i;
    if(pthread_create(&fa_thread[i], &attr, nfa_thread_callback, (void*) &thread_id[i])){
       NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_name[0], NSLB_TL_ERROR, "Error in creating thread; %s", nslb_strerror(errno));
       exit(1);
    }
    NSLB_TRACE_LOG2(nfa_trace_log_key, global_partition_idx, thread_name[0], NSLB_TL_INFO, "Thread %s started successfully", thread_name[i+1]);
  }
  
  NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_name[0], NSLB_TL_INFO, "All thread started.");

  pthread_attr_destroy(&attr);

  //setting signal handler
  bzero (&sa, sizeof(struct sigaction));
  sa.sa_handler = SIG_IGN;
  sigaction(SIGINT, &sa, NULL);

  bzero (&sa, sizeof(struct sigaction));
  sa.sa_handler = sigterm_handler;
  sigaction(SIGTERM, &sa, NULL);

  bzero (&sa, sizeof(struct sigaction));
  sa.sa_handler = sigalarm_handler;
  sigaction(SIGALRM, &sa, NULL);

  bzero (&sa, sizeof(struct sigaction));
  sa.sa_handler = partition_switch_sig_handler;
  sigaction(PARTITION_SWITCH_SIG, &sa, NULL);

  bzero (&sa, sizeof(struct sigaction));
  sa.sa_handler = trace_level_change_sig_handler;
  sigaction(TRACE_LEVEL_CHANGE_SIG, &sa, NULL);

  bzero (&sa, sizeof(struct sigaction));
  sa.sa_handler = post_proc_sig_handler;
  sigaction(TEST_POST_PROC_SIG, &sa, NULL);

  bzero (&sa, sizeof(struct sigaction));
  sa.sa_handler = sigchld_handler;
  sigaction(SIGCHLD, &sa, NULL);

  alarm(1);
  sigfillset (&sigset);
  sigprocmask (SIG_SETMASK, &sigset, NULL);

  //if offline mode then need not to poll test run.
  if(!online_mode) is_test_running = 0;
  
  //start logging reader.
  //give some time so that threads can create dlog and slog file.
  //otherwise logging reader will fail.
  NSLB_TRACE_LOG2(nfa_trace_log_key, global_partition_idx, thread_name[0], NSLB_TL_INFO, "Sleeping for %d(sec) before starting nsu_logging_reader", nifa_settings.lr_start_delay);
  sleep(nifa_settings.lr_start_delay);
  //start logging reader in online mode irrespective of reader_run_mode 
  start_logging_reader(test_run_num, nlr_trace_level, 1/*reader_run_mode*/, reader_write_time, dynamic_table_size, generator_id, cav_epoch_year, test_run_info_id, 0, num_nvm, num_gen, loader_opcode, dynamic_tx_table_size); 
  
  //start recovery only if online mode.
  if(online_mode)
  {
    if(nslb_init_component_recovery_data(&ReaderRecoveryData, MAX_RECOVERY_COUNT, RECOVERY_TIME_THRESHOLD_IN_SEC, RESTART_TIME_THRESHOLD_IN_SEC) == 0)
    {
      NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_name[0], NSLB_TL_INFO, "Recovery data initialized with"
                          "MAX_RECOVERY_COUNT = %d, RECOVERY_TIME_THRESHOLD_IN_SEC = %d, RESTART_TIME_THRESHOLD_IN_SEC = %d",
                              MAX_RECOVERY_COUNT, RECOVERY_TIME_THRESHOLD_IN_SEC, RESTART_TIME_THRESHOLD_IN_SEC);
    }
    else
    {
      NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_name[0], NSLB_TL_ERROR, "Component recovery could not be initialized");
      exit(-1);
    }
  } 
  

  while(1){
    if (sigalarm)  {
      do_sigalarm(); //what to do in this function
      sigalarm = 0;
      if(logging_reader_pid == -1)
        check_and_start_logging_reader();
      alarm(1);
    }
    //BugId 47002: process will suspend before alarm(1) will expire.
    sigemptyset(&sigset);
    sigsuspend(&sigset);

    if (sigterm) {
      sighandler_t prv_handler;
      prv_handler = signal(SIGTERM, SIG_IGN);
      sigterm_recieved = 1;
      (void) signal(SIGTERM, prv_handler);
      sigterm = 0;
      break;
    }

    if(sigchld_sig)
      do_handle_sigchld();      

    if(trace_level_change_sig)
       do_change_trace_level();

    //post proc signal received.
    if(post_proc_sig == 1)
    {
      sighandler_t prv_handler;
      prv_handler = signal(SIGTERM, SIG_IGN);
      NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_name[0], NSLB_TL_ERROR, "Post proc signal recieved");
      //switch to offline mode.
      online_mode = 0;
      //TODO: check if we need to set test_running = 0.
      is_test_running = 0;
      (void) signal(SIGTERM, prv_handler);
      post_proc_sig = 0;
      epoll_timeout_cnt = 0;/*Reset epoll timeout counter*/
    }

    //if test is over then break.
    if(is_test_running == 0)
      break;
  }

  for(i=0 ; i<NUM_THREADS; i++){

    NSLB_TRACE_LOG2(nfa_trace_log_key, global_partition_idx, thread_name[0], NSLB_TL_INFO, "Thread Join");

    if(i == RBU_ACCESS_LOG_THREAD)
    {
      if(!(rbu_access_log)){
        NSLB_TRACE_LOG2(nfa_trace_log_key, global_partition_idx, thread_name[0], NSLB_TL_INFO,
                                "RBU_ACCESS_LOG_THREAD and rbu_access_log = %d",
                                rbu_access_log);
         break;
      }
    }

    if(i == RBU_LIGHTHOUSE_THREAD)
    {
      if(!(rbu_lighthouse_enabled)){
        NSLB_TRACE_LOG2(nfa_trace_log_key, global_partition_idx, thread_name[0], NSLB_TL_INFO,
                                "RBU_LIGHTHOUSE_THREAD and rbu_lighthouse_enabled = %d", rbu_lighthouse_enabled);
        break;
      }
    }

    ret = pthread_join(fa_thread[i], NULL);
    if(ret){
      NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_name[0], NSLB_TL_ERROR, "Error in joining threads, Error- [%s]", nslb_strerror(errno));
      exit(1);
    }
    NSLB_TRACE_LOG2(nfa_trace_log_key, global_partition_idx, thread_name[0], NSLB_TL_INFO, "Thread %s joined successfully", thread_name[i+1]);
  }
  
  //all threads have done their tasks now just send POST_PROC signal to logging_reader.
  if(logging_reader_pid > 0)
  {
    int status;
    if (kill(logging_reader_pid, TEST_POST_PROC_SIG) == -1)
    {
      NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_name[0], NSLB_TL_ERROR, "Error in sending signal TEST_POST_PROC_SIG to logging reader");
      exit(0);
    }
  
    NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_name[0], NSLB_TL_INFO, "Waiting for Logging Reader to exit.");
    waitpid(logging_reader_pid, &status, 0);
    NSLB_TRACE_LOG1(nfa_trace_log_key, global_partition_idx, thread_name[0], NSLB_TL_INFO, "Logging Reader exited. Going to exit");
  }
  
  exit(0);
}
