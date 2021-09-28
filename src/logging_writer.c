#define _GNU_SOURCE 
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>
#include <semaphore.h>
#include <time.h>
#include <errno.h>

#include "logging.h"
#include "nslb_log.h"
#include "util.h"
#include <sys/wait.h>

#include "nslb_comp_recovery.h"
#include "nslb_multi_thread_trace_log.h"
#include "nslb_signal.h"
#include "nslb_partition.h"
#include "nslb_util.h"
#define NS_EXIT_VAR
#include "ns_exit.h"
#define MAX_RECOVERY_COUNT                5  //used to decide whether to recover component or not
#define RECOVERY_TIME_THRESHOLD_IN_SEC   10
#define RESTART_TIME_THRESHOLD_IN_SEC   900
#define MAX_FILENAME_LENGTH 1024

TestRunInfoTable_Shr *test_run_info_shr_tbl = NULL;
MTTraceLogKey *writer_tl_key = NULL;
char base_dir[MAX_FILENAME_LENGTH] = {0};

typedef struct 
{
  //int buffer_idx;
  unsigned int block_num;
} NVMInfo;

static void send_signal_to_reader();
void partition_switch_sig_to_reader();

int logging_reader_pid = -1;
shr_logging *g_shr_memory;
int *new_test_run_num;
int child_idx;
int dlog_fd;
int *test_run_number;
int logging_shr_blk_count = 0;
long long partition_idx = 0;
char partition_name[100] = "0";

key_t shm_base_mem = 0xca000000 * 100;
NVMInfo *nvm_info;

int num_children;
int netstorm_pid;
int sigterm = 0;
int log_shm_data_ready_sig = 0;
int sigalarm = 0;
int partition_switch_sig = 0;
int trace_level_change_sig = 0;
int test_post_proc_sig = 0;
int sigchild = 0;

int running_mode = 0;
int log_shr_buffer_size = 8192;

//Added field to hold total number of generators
int total_generator_entries = 0;

static void sigterm_handler(int sig);
static void log_shm_data_ready_sig_handler(int sig);
static void sigalarm_handler(int sig);
static void partition_switch_sig_handler(int sig);
static void trace_level_change_sig_handler(int sig);
static void test_post_proc_sig_handler(int sig);
static void sigchild_handler(int sig);

void do_log_shm_data_ready();
void do_sigterm();
void do_test_post_proc();
void do_sigalarm();

static int lw_trace_level = 1;     //writer trace level
static int lw_trace_log_size = 10; //default 10 MB

static int lr_trace_level = 1;     //reader trace level to pass NLR

int testidx;
int reader_write_time;
int DynamicTableSize;
int generator_id;  //Generator index
int cav_epoch_yr = 0;
int dyn_tx_tb_size;
char err_msg[100];
unsigned long test_run_info_shm_id = 0;//Added for new test run number

//used to decide whether to recover component or not 
ComponentRecoveryData ReaderRecoveryData;  

static char wdir[256 + 1];
int start_logging_reader (int testidx, int debug_log_value, int reader_run_mode, int reader_write_time, int DynamicTableSize, int generator_id, int cav_epoch_yr, long test_run_info_shm_id, int is_recovery, int dyn_tx_tb_size)
{
    char shr_mem_new_tr_key[64];

    sprintf(shr_mem_new_tr_key, "%ld", test_run_info_shm_id);
    NSLB_TRACE_LOG1(writer_tl_key, partition_idx, "Main thread", NSLB_TL_INFO, "%s logging reader", is_recovery?"Recovering":"Starting");
    //TODO call nslb_debug_log_ex
   
    if ((logging_reader_pid = fork()) ==  0 ) 
    {
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
      char total_nvms_str[8];
      char total_generator_entries_str[8];
      int ret = 0;
      int ret1 = 0;
      sprintf(trace_level_reader, "%d", debug_log_value);
      sprintf(deliminator_reader, ",");
      sprintf(test_run_num, "%d", testidx);
      sprintf(reader_running_mode, "%d", reader_run_mode);
      sprintf(reader_write_time_str, "%d", reader_write_time);
      sprintf(dynamic_t_size, "%d", DynamicTableSize); 
      sprintf(dynamic_tx_tb_size, "%d", dyn_tx_tb_size); 
      sprintf(generator_id_buf, "%d", generator_id);
      sprintf(cav_epoch_year, "%d", cav_epoch_yr);
      sprintf(shr_buff_size_str, "%d", log_shr_buffer_size);
      sprintf(total_nvms_str, "%d", num_children);
      sprintf(total_generator_entries_str, "%d", total_generator_entries);

      sprintf(lr_bin_path, "%s/bin/nsu_logging_reader", wdir);

      //write logging_reader pid in get_all_process_pid file.
      ret = nslb_write_all_process_pid(getpid(), "logging reader's pid", wdir, testidx, "a");
      if( ret == -1 )
      {
        fprintf(stderr, "failed to open the logging reader's pid file\n");
        exit(-1);
      }
      ret1 = nslb_write_process_pid(getpid(), "logging reader's  pid", wdir, testidx, "w","NLR.pid",err_msg); 
      if(ret1 == -1)
      {
        fprintf(stderr, "failed to open the logging reader's pid file\n");
      }

      ret = nslb_write_all_process_pid(getppid(), "logging reader's parent's pid", wdir, testidx, "a");
      if( ret == -1 )
      {
        fprintf(stderr, "failed to open the logging reader's parent's pid file\n");
        exit(-1);
      }
/*      ret1 = nslb_write_process_pid(getppid(), "logging reader's  parent pid", wdir, testidx, "w","nlr.ppid",err_msg);       
      if(ret1 == -1)        
      {
        fprintf(stderr, "failed to open the logging reader's parent's pid file\n");
      }
  */ 
      //if (execlp("nsu_logging_reader", "nsu_logging_reader", "-l", test_run_num, "-D", trace_level_reader, "-W", wdir, "-m", reader_running_mode, "-Z", dynamic_t_size, "-k", shr_mem_new_tr_key, "-g", generator_id_buf, "-f", "1", "-e", cav_epoch_year, NULL) == -1) 
      if (execlp(lr_bin_path, "nsu_logging_reader", "-l", test_run_num, "-T", trace_level_reader, "-W", wdir, "-m", reader_running_mode, "-Z", dynamic_t_size, "-k", shr_mem_new_tr_key, "-g", generator_id_buf,  "-S" , shr_buff_size_str, "-n", total_nvms_str, "-G", total_generator_entries_str, "-X", dynamic_tx_tb_size, NULL) == -1)  //TODO: KRISHNA..NEED TO PAAS TRACE
      {
        perror("execl");
        NS_EXIT(-1, "Initializing logging reader: error in execl");
      }
    } 
    else 
    {
      if (logging_reader_pid < 0) 
      {
        NSLB_TRACE_LOG1(writer_tl_key, partition_idx, "Main thread", NSLB_TL_ERROR, "Unable to start Logging reader due to error in execl");
        fprintf(stderr, "error in forking the logging_reader process\n");
        //in case of recovery, dont exit if fork() fails
        if(is_recovery)
          return -1;
        else
          NS_EXIT(-1, "value of 'is_recovery' not found");
      }
      else
      {
        NSLB_TRACE_LOG1(writer_tl_key, partition_idx, "Main thread", NSLB_TL_INFO, "Logging reader process created(fork) with pid %d", logging_reader_pid);
      }
    }

  test_run_info_shr_tbl->ns_lr_pid = logging_reader_pid;
  return 0;
}

static void check_and_start_logging_reader()
{
  if(nslb_recover_component_or_not(&ReaderRecoveryData) == 0)
  { 
    NSLB_TRACE_LOG1(writer_tl_key, partition_idx, "Main thread", NSLB_TL_INFO, "Logging reader not running. Recovering logging reader.");

    if(start_logging_reader(testidx, lr_trace_level, running_mode, reader_write_time, DynamicTableSize, generator_id, cav_epoch_yr, test_run_info_shm_id, 1, dyn_tx_tb_size) == 0)
    {
      NSLB_TRACE_LOG1(writer_tl_key, partition_idx, "Main thread", NSLB_TL_INFO, "Logging reader recovered, Pid = %d.", logging_reader_pid);
    }
    else
    {
      NSLB_TRACE_LOG1(writer_tl_key, partition_idx, "Main thread", NSLB_TL_ERROR, "Logging reader recovery failed");
    }
  }
  else
  {
    NSLB_TRACE_LOG1(writer_tl_key, partition_idx, "Main thread", NSLB_TL_INFO, "Logging Reader max restart count is over. Cannot recover LR"
          " Retry count = %d, Max Retry count = %d", ReaderRecoveryData.retry_count, ReaderRecoveryData.max_retry_count);
  }
}

#define  DO_WRITER_WORK  \
  log_shm_data_ready_sig = 0;\
  alarm(0);\
  do_log_shm_data_ready();\
  if(test_run_info_shr_tbl->loader_opcode != 2){\
    if(running_mode){\
      if(logging_reader_pid < 0)/*In case of generator logging reader will not spwan*/\
        check_and_start_logging_reader();\
      else\
        send_signal_to_reader(); \
    }\
  }

void do_handle_sigchld()
{
  int status;
  int ret;

  NSLB_TRACE_LOG1(writer_tl_key, partition_idx, "Main thread", NSLB_TL_INFO, "Signal SIG_CHLD received");

  if(running_mode != 0) 
  {
    ret = waitpid(-1, &status, 0);

    if(ret == logging_reader_pid)
    {
      test_run_info_shr_tbl->ns_lr_pid = -1;
      logging_reader_pid = -1;

      NSLB_TRACE_LOG1(writer_tl_key, partition_idx, "Main thread", NSLB_TL_INFO, "SIG_CHLD received from Logging Reader");
      check_and_start_logging_reader();
    }
  }
  else
  {
    NSLB_TRACE_LOG1(writer_tl_key, partition_idx, "Main thread", NSLB_TL_INFO, "SIG_CHLD received in offline mode. This should not happen.");
  }

  sigchild = 0;
}

void send_partition_switch_sig_to_reader()
{
  if (kill(logging_reader_pid, PARTITION_SWITCH_SIG) == -1)
  {
    NSLB_TRACE_LOG1(writer_tl_key, partition_idx, "Main thread", NSLB_TL_ERROR, "Unable to send partition switch signal to Logging Reader");
    if (errno != ESRCH)
      NS_EXIT(-1, "errno != ESRCH");  //TODO DISCUSS
  }
  NSLB_TRACE_LOG2(writer_tl_key, partition_idx, "Main thread", NSLB_TL_ERROR, "Partition switch signal sent to Logging Reader");
}

static void flush_writer_buffer()
{
  write_buffer* buf_to_write;
  shr_logging* shr_memory;
  int i,j;
  void *ptr;
  int buf_to_write_idx = 0;
  char *data_buffer;
  ptr = (char *) g_shr_memory;

  for (i = 0; i < num_children; i++) 
  {
    shr_memory = (shr_logging*)(ptr + (i * (LOGGING_PER_CHILD_SHR_MEM_SIZE(log_shr_buffer_size, logging_shr_blk_count))));
    buf_to_write_idx = shr_memory->cur_lw_idx;

    for (j = 0; j < logging_shr_blk_count; j++)
    {
      buf_to_write = &shr_memory->buffer[buf_to_write_idx];
      //Note: we can not use buf_to_write->data_buffer because that have address which is pointing to different address space.
      data_buffer = LOGGING_DATA_BUFFER_PTR(ptr, i, buf_to_write_idx, log_shr_buffer_size, logging_shr_blk_count);
      if (buf_to_write->disk_flag) 
      {
        NSLB_TRACE_LOG2(writer_tl_key, partition_idx, "Main thread", NSLB_TL_INFO, "DATA in logs/TRXXX/reports/raw_data/dlog [%s] ", data_buffer);
        if (write(dlog_fd, data_buffer, log_shr_buffer_size) != log_shr_buffer_size) 
        {
          NSLB_TRACE_LOG1(writer_tl_key, partition_idx, "Main thread", NSLB_TL_ERROR, "Error in writing to the logging file: %s", nslb_strerror(errno));
        }
        buf_to_write->disk_flag = 0;
        buf_to_write->memory_flag = 1;
      }
      else{
        //Save the idx where last buffer flushed 
        //break the loop
      	shr_memory->cur_lw_idx = buf_to_write_idx;
        break; //TODO: AA What about sig term case?? do we need to process all buffers
      }
      
      buf_to_write_idx++;
      buf_to_write_idx %= logging_shr_blk_count;
    }
  }
}

void do_partition_switch()
{
  char next_partition[100] = {0};
  char file_path[1024];
  //Update partition-run number
  if(nslb_get_next_partition(wdir, testidx, partition_name, next_partition) < 0) 
  {
    NSLB_TRACE_LOG1(writer_tl_key, partition_idx, "Main thread", NSLB_TL_ERROR, "Error: Cannot find next partition");
    partition_switch_sig = 0;
    return;
  }
  //While creating new partition we were flushing data in dlog file, in case of offline when logging reader was reading dlog
  //following error was found in testing:
  //logging_reader: Error occured while matching msg number 11029,with existing message count 11030 for child 2, partition index = 20140424182255
  //Therefore following code was commented and tested(it seems to resolve issue), but need to work on this piece of code 
  //flush_writer_buffer();
  //Close logging file fd and open file in append mode
  if(close(dlog_fd) <0) 
  {
    NSLB_TRACE_LOG1(writer_tl_key, partition_idx, "Main thread", NSLB_TL_ERROR, "Error in closing dlog fd");
  }
  else
  {
    NSLB_TRACE_LOG1(writer_tl_key, partition_idx, "Main thread", NSLB_TL_INFO, "dlog fd closed");
    dlog_fd = -1;
  }
  strcpy(partition_name, next_partition);
  partition_idx = atoll(next_partition);

  sprintf(file_path, "logs/TR%d/%s/reports/raw_data/dlog", testidx, partition_name);
  NSLB_TRACE_LOG1(writer_tl_key, partition_idx, "Main thread", NSLB_TL_INFO, "Switching to partition %lld. New dlog file = %s", partition_idx, file_path);

  if ((dlog_fd = open(file_path, O_APPEND | O_RDWR | O_LARGEFILE | O_CLOEXEC, S_IRWXU | S_IRGRP | S_IROTH)) == -1) 
  {
    NSLB_TRACE_LOG1(writer_tl_key, partition_idx, "Main thread", NSLB_TL_ERROR, "Error in opening logging file %s", file_path);
    perror("Error is ");
    NS_EXIT(-1, "logging_writer: Error in opening logging file %s", file_path);
  }
  NSLB_TRACE_LOG1(writer_tl_key, partition_idx, "Main thread", NSLB_TL_INFO, "dlog fd opened %s", file_path);
      
  //Send signal to reader 
  if(running_mode > 0 && (logging_reader_pid > 0))
    send_partition_switch_sig_to_reader();

  partition_switch_sig = 0;
}

void do_change_trace_level()
{
  nslb_change_atb_mt_trace_log(writer_tl_key, "NLW_TRACE_LEVEL"); 
  lw_trace_level = writer_tl_key->log_level;
  NSLB_TRACE_LOG1(writer_tl_key, partition_idx, "Main thread", NSLB_TL_INFO, "Trace Level changed to %d", lw_trace_level); 
  trace_level_change_sig = 0;
}
//*******************************
// 1st arg = num_childs,
// 2  arg = loggiging file
// 3  arg = shared memory pointer
//
int main(int argc, char** argv)
{
  int log_mem_fd, ret;
  //key_t shr_mem_key;
  struct sigaction sa;
  sigset_t sigset;
 
  num_children = atoi(argv[1]);
  if ((dlog_fd = open(argv[2], O_APPEND | O_RDWR | O_LARGEFILE |O_CLOEXEC, S_IRUSR | S_IWUSR | S_IRGRP |  S_IWGRP| S_IROTH)) == -1) 
  {
    fprintf(stderr, "logging_writer: Error in opening logging file %s. Err = %s\n", argv[2], nslb_strerror(errno));
    return -1;
  }
  //shr_mem_key = strtoul(argv[3], NULL, 16);
  log_mem_fd = strtoul(argv[3], NULL, 10);
  running_mode = atoi(argv[4]);
  testidx  = atoi(argv[5]);
  reader_write_time  = atoi(argv[6]);
  DynamicTableSize = atoi(argv[7]);
  logging_shr_blk_count = atoi(argv[8]);  
  generator_id = atoi(argv[9]); //Generator index
  //Need to create shared memory if test run shared memory pointer passed 
  test_run_info_shm_id = strtoul(argv[11], NULL, 10);
  cav_epoch_yr = atoi(argv[10]);
  log_shr_buffer_size = atoi(argv[12]);
  total_generator_entries = atoi(argv[13]);
  dyn_tx_tb_size = atoi(argv[14]);

  if (getenv("NS_WDIR") != NULL)
    strcpy(wdir, getenv("NS_WDIR"));
  else{
    printf("NS_WDIR env variable is not set. Setting it to default value /home/cavisson/work/");
    strcpy(wdir, "/home/cavisson/work/");
  }
  sprintf(base_dir, "%s/logs/TR%d", wdir, testidx);

  //This shm is for getting partition idx when NS is running in switching mode
  test_run_info_shr_tbl = (TestRunInfoTable_Shr *) shmat(test_run_info_shm_id, NULL, 0);
  if (test_run_info_shr_tbl == (void *) -1) {
    perror("logging_writer: error in attaching shared memory");
    return -1;
  }
  //if partition creation mode is off and normal test is running.
  //By default partition name will be -1.
  if(test_run_info_shr_tbl->partition_idx > 0)
  {
    // Copy current partition from shared memory and start saving in this partition
    // Since LW takes data from SHM, there is not concept of going back to prev partitions
    partition_idx = test_run_info_shr_tbl->partition_idx;
    strcpy(partition_name, test_run_info_shr_tbl->partition_name);
  }

  //always get trace level from shared memory becoz writer always run in online mode
  lw_trace_level = test_run_info_shr_tbl->nlw_trace_level;
  lr_trace_level = test_run_info_shr_tbl->nlr_trace_level;

  writer_tl_key = nslb_init_mt_trace_log(wdir, testidx, partition_idx, "ns_logs/nlw_trace.log",
                                  lw_trace_level, lw_trace_log_size);  


  NSLB_TRACE_LOG1(writer_tl_key, partition_idx, "Main thread", NSLB_TL_INFO, "Stared logging writer. TR num %d, NLW trace level %d, NLR trace level %d", testidx, lw_trace_level, lr_trace_level);

  NSLB_TRACE_LOG1(writer_tl_key, partition_idx, "Main thread", NSLB_TL_INFO, "argv[0] = %s argv[1](total netstorm child) = %s argv[2](file to write) = %s argv[3] = %s argv[4] = %s argv[5] = %s argv[6] = %s argv[7] = %s debuglogvalue = %d logging_shr_blk_count = %d, generator_id = %d, log_shared_buffer_size = %d, total_generator_entries = %d, loader_opcode = %d",argv[0], argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], argv[7], lw_trace_level, logging_shr_blk_count, generator_id, log_shr_buffer_size, total_generator_entries, test_run_info_shr_tbl->loader_opcode);

  //if previous logging writer is already running after writer restarts, then first kill it
  ret = nslb_save_pid_ex(base_dir, "nlw", "nsa_logger");
  if(ret == 1)
  {
    NSLB_TRACE_LOG1(writer_tl_key, partition_idx, "Main thread", NSLB_TL_INFO, "Prev Logging Writer killed, pid saved");
  }
  else if(ret == 0)
  {
    NSLB_TRACE_LOG1(writer_tl_key, partition_idx, "Main thread", NSLB_TL_INFO, "Prev Logging Writer was not running, pid saved");
  }
  else
  {
    NSLB_TRACE_LOG1(writer_tl_key, partition_idx, "Main thread", NSLB_TL_ERROR, "Error in saving Logging Writer pid");
  }

  nvm_info = (void *) malloc(num_children * sizeof(NVMInfo));
  memset(nvm_info, 0, num_children * sizeof(NVMInfo));
  
  //This shm is for reading data from NetStorm
  if ((g_shr_memory = (shr_logging*) shmat(log_mem_fd, NULL, 0)) == (void *) -1) {
    NSLB_TRACE_LOG1(writer_tl_key, partition_idx, "Main thread", NSLB_TL_ERROR, "Error in attaching to shared memory");
    perror("logging_writer: error in attaching shared memory");
    NS_EXIT(-1, "Error in attaching to shared memory");
  }

  child_idx = num_children - 1;
  netstorm_pid = getppid();

  NSLB_TRACE_LOG1(writer_tl_key, partition_idx, "Main thread", NSLB_TL_INFO, "Setting signal handlers.");
  bzero (&sa, sizeof(struct sigaction));
  sa.sa_handler = SIG_IGN;
  sigaction(SIGINT, &sa, NULL);

  bzero (&sa, sizeof(struct sigaction));
  sa.sa_handler = SIG_IGN;
  sigaction(SIGHUP, &sa, NULL);

  bzero (&sa, sizeof(struct sigaction));
  sa.sa_handler = log_shm_data_ready_sig_handler;
  sigaction(LOG_SHM_DATA_READY_SIG, &sa, NULL);

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
  sa.sa_handler = test_post_proc_sig_handler;
  sigaction(TEST_POST_PROC_SIG, &sa, NULL);

  bzero (&sa, sizeof(struct sigaction));
  sa.sa_handler = sigchild_handler;
  sigaction(SIGCHLD, &sa, NULL);


  sigfillset (&sigset);
  sigprocmask (SIG_SETMASK, &sigset, NULL);
  //signal(LOG_SHM_DATA_READY_SIG, log_shm_data_ready_sig_handler);
  //signal(SIGTERM, sigterm_handler);
 
  /*  If logging reader running mode is 0, then dont run lgging reader  
   *  NetCloud: In case of generator we are not spawning logging reader*/
  if((running_mode != 0) && (test_run_info_shr_tbl->loader_opcode != 2))
  {
    start_logging_reader(testidx, lr_trace_level, running_mode, reader_write_time, DynamicTableSize, generator_id, cav_epoch_yr, test_run_info_shm_id, 0, dyn_tx_tb_size);
  }

  /*  We need to avoid a situation when a component is recovered again and again,
   *  if component continues to die because of some reason.
   *  Hence we have a counter and a threshold timer;
   *  Component can be recovered limited number of times.
   *  If component doesn't die again within threshold time, then the counter is reset to 0.
   */
  if(nslb_init_component_recovery_data(&ReaderRecoveryData, MAX_RECOVERY_COUNT, RECOVERY_TIME_THRESHOLD_IN_SEC, RESTART_TIME_THRESHOLD_IN_SEC) == 0)
  {
    NSLB_TRACE_LOG1(writer_tl_key, partition_idx, "Main thread", NSLB_TL_INFO, "Recovery data initialized with"
                          "MAX_RECOVERY_COUNT = %d, RECOVERY_TIME_THRESHOLD_IN_SEC = %d, RESTART_TIME_THRESHOLD_IN_SEC = %d",
                              MAX_RECOVERY_COUNT, RECOVERY_TIME_THRESHOLD_IN_SEC, RESTART_TIME_THRESHOLD_IN_SEC);
  }
  else
  {
    NSLB_TRACE_LOG1(writer_tl_key, partition_idx, "Main thread", NSLB_TL_ERROR, "Component recovery could not be initialized");
  }
    
  while (1)
  {
    if (sigterm)  
    {
	      do_sigterm();
	      sigterm = 0;
    }

    if(sigchild)
      do_handle_sigchld();

    if (partition_switch_sig)
      do_partition_switch();

    if(trace_level_change_sig)
      do_change_trace_level();

    if(test_post_proc_sig)
      do_test_post_proc();

    //Added while loop because we are sending signal without checking the flag
    //So, once log_shm_data_ready_sig is set then call function
    while(log_shm_data_ready_sig)
    {  
      NSLB_TRACE_LOG2(writer_tl_key, partition_idx, "Main thread", NSLB_TL_INFO, "Calling DO_WRITER_WORK");
      DO_WRITER_WORK
    }

    if (sigalarm)  
    {
	      do_sigalarm();
	      sigalarm = 0;
        //Added in alarm for following case:
        //If all NVMs done and only one NVM is working and that NVM dont have
        //flag enable to send signal then dlog will never get updated
        do
        {
          NSLB_TRACE_LOG2(writer_tl_key, partition_idx, "Main thread", NSLB_TL_INFO, "Calling DO_WRITER_WORK");
          DO_WRITER_WORK
        }while(log_shm_data_ready_sig);
    }
    alarm(10);

    //if partition id is diff in shared mem, that means partition has switched.
    if(partition_idx > 0 && partition_idx != test_run_info_shr_tbl->partition_idx)
    {
      NSLB_TRACE_LOG1(writer_tl_key, partition_idx, "Main thread", NSLB_TL_INFO, "Partition_idx %lld in shared memory is different"
              " from cur partition %lld in LW, but partition switch signal has not been recieved yet, hence switching partition.",
                      test_run_info_shr_tbl->partition_idx, partition_idx);
      partition_switch_sig = 1; 
    }

    sigemptyset(&sigset);
    sigsuspend(&sigset);
  }
}

static inline void send_signal_to_reader()
{
  if (kill(logging_reader_pid, LOG_SHM_DATA_READY_SIG) == -1) 
  {
    NSLB_TRACE_LOG1(writer_tl_key, partition_idx, "Main thread", NSLB_TL_ERROR, "Error in sending the logging reader");
    if (errno != ESRCH)
    {
      perror("Error in sending the logging reader LOG_SHM_DATA_READY_SIG signal");
      NS_EXIT(-1, "Error in sending the logging reader LOG_SHM_DATA_READY_SIG signal");
    }
  }
  else
  {
    NSLB_TRACE_LOG2(writer_tl_key, partition_idx, "Main thread", NSLB_TL_ERROR, "LOG_SHM_DATA_READY_SIG sent to logging reader");
  }
}

void send_sigterm_signal_to_reader()
{
  NSLB_TRACE_LOG1(writer_tl_key, partition_idx, "Main thread", NSLB_TL_INFO, "Received SIGTERM signal.");
  if (kill(logging_reader_pid, SIGTERM) == -1) 
  {
    NSLB_TRACE_LOG1(writer_tl_key, partition_idx, "Main thread", NSLB_TL_ERROR, "Error in sending the logging reader sigterm signal");
    if (errno != ESRCH) 
    {
      perror("Error in sending the logging reader sigterm signal");
      NS_EXIT(-1, "Error in sending the logging reader sigterm signal");
    }
  }
  else
  {
    NSLB_TRACE_LOG2(writer_tl_key, partition_idx, "Main thread", NSLB_TL_ERROR, "SIG_TERM signal sent to logging reader");
  }
}

void log_shm_data_ready_sig_handler(int sig)
{
	log_shm_data_ready_sig = 1;
}
#if 0
// decrement the semaphore (Lock) 
#define NS_LOCK_LOGGING_SHR_BUFFER(shr_buffer, buf_to_write_idx, nvm_id) \
{ \
  NSLB_TRACE_LOG2(writer_tl_key, partition_idx, "Main thread", NSLB_TL_INFO, "Start - Locking shared memory block %d using sem_wait for NVM = %d", buf_to_write_idx, nvm_id); \
  /*time_t tm = time(NULL); \
  struct timespec abs_timeout;\
  abs_timeout.tv_sec = tm;\
  abs_timeout.tv_nsec = 1;\
  int sem_wait_ret = sem_timedwait(&(shr_buffer->sem), &abs_timeout); \
  if(sem_wait_ret < 0){ \
    NSLB_TRACE_LOG2(writer_tl_key, partition_idx, "Main thread", NSLB_TL_INFO, "End - Fail locking shared memory block %d using sem_wait for NVM = %d, disk_flag = %d, memory_flag = %d", buf_to_write_idx, nvm_id, shr_buffer->disk_flag, shr_buffer->memory_flag); \
    nvm_info[child_num].buffer_idx = buf_to_write_idx;\
    break;\
  }\
  */\
  NSLB_TRACE_LOG2(writer_tl_key, partition_idx, "Main thread", NSLB_TL_INFO, "End - Locking shared memory block %d using sem_wait for NVM = %d", buf_to_write_idx, nvm_id); \
}


#define NS_UNLOCK_LOGGING_SHR_BUFFER(shr_buffer, buf_to_write_idx, nvm_id) \
{ \
  NSLB_TRACE_LOG2(writer_tl_key, partition_idx, "Main thread", NSLB_TL_INFO, "Start - UnLocking shared memory block %d using sem_wait for NVM = %d", buf_to_write_idx, nvm_id); \
  /*int sem_post_ret = sem_post(&(shr_buffer->sem)); \
  if(sem_post_ret < 0){\
    fprintf(stderr, "Error in calling sem_post\n");\
    NSLB_TRACE_LOG2(writer_tl_key, partition_idx, "Main thread", NSLB_TL_INFO, "Error in unLocking shared memory block %d using sem_wait for NVM = %d, disk_flag = %d, memory_flag = %d", buf_to_write_idx, nvm_id, shr_buffer->disk_flag, shr_buffer->memory_flag); \
  }\
*/\
  NSLB_TRACE_LOG2(writer_tl_key, partition_idx, "Main thread", NSLB_TL_INFO, "End - UnLocking shared memory block %d using sem_wait for NVM = %d", buf_to_write_idx, nvm_id); \
}
#endif

void do_log_shm_data_ready()
{
  write_buffer* buf_to_write;
  int buf_to_write_idx;
  int start_child_idx = child_idx;
  int child_num = child_idx;
  int offset = 0;
  int bytes_left = 0; 
  int bytes_wrtn = 0;
  int i = 0;
  char *data_buffer;
  shr_logging* shr_memory;
  void *ptr;
  ptr = (char *) g_shr_memory;

  shr_memory = (shr_logging*)(ptr + (child_idx * (LOGGING_PER_CHILD_SHR_MEM_SIZE(log_shr_buffer_size, logging_shr_blk_count))));

  shr_memory->write_flag = 0;
  NSLB_TRACE_LOG3(writer_tl_key, partition_idx, "Main thread", NSLB_TL_INFO, "Method called child_idx = %d, logging_shr_blk_count = %d", child_idx, logging_shr_blk_count);

  do {
    shr_memory = (shr_logging*)(ptr + (child_num * (LOGGING_PER_CHILD_SHR_MEM_SIZE(log_shr_buffer_size, logging_shr_blk_count))));
    start_child_idx = buf_to_write_idx = shr_memory->cur_lw_idx;
    do {
      buf_to_write = &shr_memory->buffer[buf_to_write_idx];
      //Note: we can not use buf_to_write->data_buffer.
      data_buffer = LOGGING_DATA_BUFFER_PTR(ptr, child_num, buf_to_write_idx, log_shr_buffer_size, logging_shr_blk_count);
      NSLB_TRACE_LOG3(writer_tl_key, partition_idx, "Main thread", NSLB_TL_ERROR, "start_child_idx = %d, buf_to_write->disk_flag = %d", start_child_idx, buf_to_write->disk_flag);

      //decrement the semaphore (Lock)
      //NS_LOCK_LOGGING_SHR_BUFFER(buf_to_write, buf_to_write_idx, child_num);

      if (buf_to_write->disk_flag) {
        // Initial block number in nvm info is 0 and we expect block number from NVM to start from 1
        if(buf_to_write->block_num != (nvm_info[child_num].block_num + 1))
        {
          NSLB_TRACE_LOG1(writer_tl_key, partition_idx, "Main thread", NSLB_TL_ERROR, "Error: Incorrect block number of NVM %d. NVM block number is %d, expected block number is %d\n", child_num, buf_to_write->block_num, (nvm_info[child_num].block_num + 1));
        }

        NSLB_TRACE_LOG2(writer_tl_key, partition_idx, "Main thread", NSLB_TL_INFO, "Writing NVM %d, block number %d, ptr = %p", child_num, buf_to_write->block_num, data_buffer);
        bytes_left = log_shr_buffer_size;
        offset = 0;
        while(1) {
	        bytes_wrtn = write(dlog_fd, data_buffer + offset, bytes_left);
          if(bytes_wrtn == bytes_left)
            break;
          if(bytes_wrtn == 0)
          {
            NSLB_TRACE_LOG2(writer_tl_key, partition_idx, "Main thread", NSLB_TL_ERROR, "logging_writer: "
                "nothing was written to the logging file, bytes written = "
                "%d, offset = %d, bytes left = %d. Retrying.. Error is: %s, address = %p", bytes_wrtn, offset, bytes_left, nslb_strerror(errno), data_buffer);
          }
          else if(bytes_wrtn < 0) 
          {
            switch(errno)
            {
              case ENOSPC:
                /* In case of no disk space available, we need to terminate the test  
                 * but after delay of 100 sec*/
                sleep(10);
                if (i == 9)
                {
                  NSLB_TRACE_LOG1(writer_tl_key, partition_idx, "Main thread", NSLB_TL_ERROR, "Error in writting to the logging file"
                    " bytes written = %d, offset = %d, bytes left = %d. Hence terminating logging writer, Error is %s, adddress = %p", 
                          bytes_wrtn, offset, bytes_left, nslb_strerror(errno), data_buffer);
                  NS_EXIT (-1, "Error in writting to the logging file"
                    " bytes written = %d, offset = %d, bytes left = %d. Hence terminating logging writer, Error is %s, adddress = %p",
                          bytes_wrtn, offset, bytes_left, nslb_strerror(errno), data_buffer); 
                } 
                i++;
                continue;

              default:
                /* In case of write fail, we need to continue without modifying offset and bytes_left*/
                NSLB_TRACE_LOG1(writer_tl_key, partition_idx, "Main thread", NSLB_TL_ERROR, "Error in writting to the logging file, "
                    "bytes written = %d, offset = %d, bytes left = %d. Retrying.. Error is %s, address = %p, base_address = %p",
                    bytes_wrtn, offset, bytes_left, nslb_strerror(errno), data_buffer + offset, g_shr_memory);
                continue; 
            } 
          }
          offset += bytes_wrtn;
          bytes_left -= bytes_wrtn;
        }

	      buf_to_write->disk_flag = 0;
	      buf_to_write->memory_flag = 1;
        nvm_info[child_num].block_num = buf_to_write->block_num; // Save for cross checking
        NSLB_TRACE_LOG3(writer_tl_key, partition_idx, "Main thread", NSLB_TL_ERROR, "Set flag buf_to_write->memory_flag =%d, buf_to_write->disk_flag = %d, nvm_info[%d].block_num = %d", buf_to_write->memory_flag, buf_to_write->disk_flag, child_num, nvm_info[child_num].block_num);
        //NS_UNLOCK_LOGGING_SHR_BUFFER(buf_to_write, buf_to_write_idx, child_num);
      }
      else 
      {
        NSLB_TRACE_LOG4(writer_tl_key, partition_idx, "Main thread", NSLB_TL_INFO, "disk_flag is not set for block %d", buf_to_write_idx);
      	shr_memory->cur_lw_idx = buf_to_write_idx;
        //NS_UNLOCK_LOGGING_SHR_BUFFER(buf_to_write, buf_to_write_idx, child_num);
	      break;
      }
      buf_to_write_idx++;
      buf_to_write_idx %= logging_shr_blk_count;
    } while ( buf_to_write_idx != start_child_idx);
    child_num++;
    child_num %= num_children;
  } while (child_num != child_idx);

  child_idx--;
  if (child_idx < 0)
    child_idx = num_children - 1;

  shr_memory = (shr_logging*)(ptr + (child_idx * (LOGGING_PER_CHILD_SHR_MEM_SIZE(log_shr_buffer_size, logging_shr_blk_count))));

  shr_memory->write_flag = 1;
}

void sigchild_handler (int sig) 
{
    sigchild = 1;
}

void sigterm_handler(int sig) 
{
    sigterm = 1;
}

void partition_switch_sig_handler(int sig) 
{
  partition_switch_sig = 1;
}

void trace_level_change_sig_handler(int sig)
{
  trace_level_change_sig = 1;
}

void test_post_proc_sig_handler(int sig)
{
  test_post_proc_sig = 1;
}


void do_test_post_proc()
{
  int status;
  NSLB_TRACE_LOG1(writer_tl_key, partition_idx, "Main thread", NSLB_TL_INFO, "Logging Writer: Recieved signal TEST_POST_PROC_SIG.");
  flush_writer_buffer();
  close(dlog_fd);
  if(running_mode && (logging_reader_pid > 0))
  {
    if (kill(logging_reader_pid, TEST_POST_PROC_SIG) == -1)
    {
      NSLB_TRACE_LOG1(writer_tl_key, partition_idx, "Main thread", NSLB_TL_ERROR, "Error in sending signal TEST_POST_PROC_SIG to logging reader");
      NS_EXIT(0, "Error in sending signal TEST_POST_PROC_SIG to logging reader");
    }
  
    //Wait for logging reader pid. Logging writer will exit only when logging reader exits.
    NSLB_TRACE_LOG1(writer_tl_key, partition_idx, "Main thread", NSLB_TL_INFO, "Logging Writer: Waiting for Logging Reader to exit.");
    //TODO Error handling if required
    waitpid(logging_reader_pid, &status, 0);
  } 
  NSLB_TRACE_LOG1(writer_tl_key, partition_idx, "Main thread", NSLB_TL_INFO, "Logging Writer: Logging Reader exited. Going to exit");
  exit(0);
}

void do_sigterm()
{
  flush_writer_buffer();
  close(dlog_fd);
  if(running_mode && (logging_reader_pid > 0))
    send_sigterm_signal_to_reader();
  NSLB_TRACE_LOG1(writer_tl_key, partition_idx, "Main thread", NSLB_TL_INFO, "send_sigterm_signal_to_reader() is called");
  exit(0);
}

void sigalarm_handler (int sig) 
{
    sigalarm = 1;
}

void do_sigalarm() 
{
  alarm(10);
  //Check if parrent is alive, if not exit
  if ((kill (netstorm_pid, 0) == -1) && ((errno == ESRCH) || (errno == EPERM))) 
  {
    NSLB_TRACE_LOG1(writer_tl_key, partition_idx, "Main thread", NSLB_TL_ERROR, "Logging: NetStorm master process got killed");
    exit(1);
  }
}

