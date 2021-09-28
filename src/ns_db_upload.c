#define _GNU_SOURCE
 
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <libpq-fe.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <regex.h>
#include <stdarg.h>
#include <pwd.h>
#include <grp.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/epoll.h>

#include "ns_dbu_monitor.h"
#include "nslb_db_util.h"
#include "nslb_util.h"
#include "logging.h"
#include "ns_common.h"
#include "ns_global_settings.h"
#include "tmr.h"
#include "ns_monitoring.h"
#include "nslb_db_upload_common.h"
#include "nslb_multi_thread_trace_log.h"
#include "nslb_signal.h"
#include "nslb_dbu_monitor_stats.h"
#include "nslb_mon_registration_con.h"
//#include "ndc_common_msg_com_util.h"

#define MAX_READ_LENGTH 128*1024
 char *ANALYSE_THRD = "ANALYZE-THREAD";

// For all global variables
db_upload ns_db_upload; 
ComponentRecoveryData db_analyze_recovery;
//variables for sending data to ns
int last_records_sent_time = 0;


//epoll fd
int v_epoll_fd = -1;

/*Bug#91417_RUNTIME_MONITORS_APPLIED*/
sigset_t set;
pthread_t *thread = NULL;
pthread_attr_t attr;
int *threadargs;
char createGenViewsAndMetaTables = 0;

#define NSDB_MAX_LINE_LENGTH 4096

#define PARTITION_NAME (ns_db_upload.partition_name[0] != '\0')?partition_idx:-1

#define NSDBU_TRACE_KW "NSDBU_TRACE_LEVEL"

inline void print_usages()
{
  fprintf(stderr, "nsu_db_upload --testrun <testrun number> [--chunk_size <chunk size>] "
                   "[--idle_time <idle_time>] "
                   "[--resume] [--ppid <pid of parent process>]  [--resume] "
                   "[--trace_level <trace_level>] [--partition_name <partition directory name>]\n"); 
}

void nsu_parse_args(int argc, char *argv[]){
  
  //NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "Main thread", INFO, "Method Called");
  char c;
  int t_flag = 0;

  // array for long argument support
  struct option longopts[] = {
                               {"testrun", 1, NULL, 't'},
                               {"running_mode", 1, NULL, 'm'},
                               {"chunk_size",  1, NULL, 's'},
                               {"idle_time",  1, NULL, 'T'},
                               {"resume",  1, NULL, 'r'},
                               {"ppid", 1, NULL, 'P'},
                               {"trace_level", 1, NULL, 'l'},
                               {"test_run_info_shm_key", 1, NULL, 'k'},
                               {"cav_epoch_diff", 1, NULL, 'e'},
                               {"num_cycles", 1, NULL, 'n'},
                               {"db_tmp_file_path", 1, NULL, 'x'},
                               {"usr_provided_partition", 1, NULL, 'p'},
                               {0, 0, 0, 0}
                             };


   while ((c = getopt_long(argc, argv, "t:m:s:T:r:P:c:e:k:l:n:x:p:", longopts, NULL)) != -1){
    switch (c){
      case 't':
        ns_db_upload.tr_num = atoi(optarg);
        t_flag = 1;
        break;
      case 's':
        ns_db_upload.chunk_size = atoll(optarg);
        break;        
      case 'T':
        ns_db_upload.idle_time = atoi(optarg);
        break;  
      case 'm':
        ns_db_upload.running_mode = atoi(optarg);
        break;  
      case 'r':
        ns_db_upload.resume_flag = atoi(optarg);
        break;
      case 'P':
        ns_db_upload.parent_pid = atoi(optarg);
        break;
      case 'l':
        ns_db_upload.trace_level = atoi(optarg);
        break;
      case 'e':
        ns_db_upload.cav_epoch_diff = atol(optarg);
        break;  
      case 'k':
        ns_db_upload.test_run_info_shm_id = atol(optarg);
        break;  
      case 'n':
        ns_db_upload.num_cycles = atoi(optarg);
        break;  
      case 'x':
        strcpy(ns_db_upload.dbTmpFilePath, optarg);
        break;
      case 'p':
        ns_db_upload.usr_provided_partition = atoll(optarg);
        break;
      case ':':
      case '?':
        fprintf(stderr, "Invalid option: %c\n", c); 
        print_usages();
        exit(-1); 
    }
  }
  if(!t_flag)
  {
    fprintf(stderr, "--testrun argument missing\n");
    print_usages();
    exit(-1);
  }
}

#define MAX_FILE_NAME_LEN 2048



/* We are pasrsing error message to recover for any error , 
 * for that we need full error message so we are taking too big buffer 
*/
#define ERROR_MSG_LEN 128 * 1024

/* This method will copy buffer to db and will also handle some basic error conditions.
 * it will upload remaing data and will leave error lines 
 * ... If db connection error will occure then this will return -2, caller need to handle this case
 */

void do_sigalarm() {
  if(ns_db_upload.is_test_running){
    if(ns_db_upload.process_mode == DB_CHILD_MODE) {
      if(ns_db_upload.parent_pid != -1 && nslb_check_pid_alive(ns_db_upload.parent_pid))
        ns_db_upload.is_test_running = 1;
      else {
        ns_db_upload.is_test_running = 0; 
        ns_db_upload.kill_upload_thread = 1;

        NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "Main Thread", NSLB_TL_INFO, "Logging Reader has been stopped, stop uploading to db.");
      }
    }
    else
      //check from shm
      ns_db_upload.is_test_running = nslb_chk_is_test_running(ns_db_upload.tr_num);
  }
}

#define ORL_END_MARKER_FILE ".orl_process_done.txt"


#define CUR_PART_IDX csv_table.data_file.partition_idx?csv_table.data_file.partition_idx:-1


#if NSU_DB_UPLOAD 
/*static char *thread_name[] = {"NS_DB_UPLOAD-Main"};
                              "NS_DB_UPLOAD Thread-0", 
                              "NS_DB_UPLOAD Thread-1",
                              "NS_DB_UPLOAD Thread-2",
                              "NS_DB_UPLOAD Thread-3",
                              "NS_DB_UPLOAD Thread-4",
                              "NS_DB_UPLOAD Thread-5",
                              "NS_DB_UPLOAD Thread-6",
                              "NS_DB_UPLOAD Thread-7",
                              "NS_DB_UPLOAD Thread-8",
                              "NS_DB_UPLOAD Thread-9",
                              "NS_DB_UPLOAD Thread-10",
                              "NS_DB_UPLOAD Thread-11",
                              "NS_DB_UPLOAD Thread-12",
                              "NS_DB_UPLOAD Thread-13",
                              "NS_DB_UPLOAD Thread-14",
                              "NS_DB_UPLOAD Thread-15",
                              "NS_DB_UPLOAD Thread-16",
                              "NS_DB_UPLOAD Thread-17",
                              "NS_DB_UPLOAD Thread-18",
                              "NS_DB_UPLOAD Thread-19",
                              "NS_DB_UPLOAD Thread-20",
                              "NS_DB_UPLOAD Thread-21",
                              "NS_DB_UPLOAD Thread-22",
                              "NS_DB_UPLOAD Thread-23",
                              "NS_DB_UPLOAD Thread-24",
                              "NS_DB_UPLOAD Thread-25",
                              "NS_DB_UPLOAD Thread-26",
                              "NS_DB_UPLOAD Thread-27",
                              "NS_DB_UPLOAD Thread-28",
                              "NS_DB_UPLOAD Thread-29",
                              "NS_DB_UPLOAD Thread-30"
};*/


static void *loaderThreadStartRoutine(void *args){

  char dynamicThreadName[255] = "";
  int thread_id =(*(int *)args);
  
  switch (thread_id){
    case 1:
      if(ns_db_upload.usr_provided_partition < 0)
      {
        NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "Metadata_thread", NSLB_TL_INFO, 
                                                                    "Thread started to process MetaData CSV Files");
        staticCsvDataLoader("Metadata_thread", 0, &ns_db_upload, 1, "common_files"); 
      }
      break;
    default :
      sprintf(dynamicThreadName, "%s_thread", ns_db_upload.dbUploaderDynamicCSVInitStruct[thread_id - 2].csvtable); 
      NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, dynamicThreadName, NSLB_TL_INFO, 
                      "Thread started to process '%s'", ns_db_upload.dbUploaderDynamicCSVInitStruct[thread_id - 2].csvName);
      dynamicCsvDataLoader(dynamicThreadName, ns_db_upload.dbUploaderDynamicCSVInitStruct[thread_id - 2].csvName, 0, (thread_id - 2), &ns_db_upload, NULL);  
  }
  return NULL; 
}
#endif

int test_post_proc_sig = 0;
int sigalarm = 0;
int sigterm = 0;
int rt_trace = 0;
void test_post_proc_sig_handler(int sig) {
  test_post_proc_sig = 1;

}
void sigalarm_handler (int sig) {
    sigalarm = 1;
}

void sigterm_handler(int sig) {
  sigterm = 1;

}

#if NSU_DB_UPLOAD 
static void rt_trace_change(int sig){
  rt_trace = 1;
}

static void partition_switch(int sig)
{
  NSLB_TRACE_LOG2(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "Main Thread", NSLB_TL_INFO, "Partition Switch Signal"
               " SIGRTMIN+2 Received from Logging reader.");
}

/*
static void fork_db_analyze()
{
  char db_analyze_path[512];
  char interval_buf[64];
  char testrun_num[64];
  char db_upload_pid[64];
  if ((db_analyze_pid = fork()) ==  0 )
  {
    sprintf(testrun_num, "%d", ns_db_upload.tr_num);
    sprintf(interval_buf, "%d", ns_db_upload.db_analyze_interval);
    sprintf(db_upload_pid, "%d", ns_db_upload.pid);
    sprintf(db_analyze_path, "%s/bin/nsi_db_analyze", ns_db_upload.ns_wdir);

    NSLB_TRACE_LOG2(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "Main Thread", NSLB_TL_INFO, "Initializing DB ANALYZE: db_analyze_path = [%s]", db_analyze_path);

    if (execlp(db_analyze_path, "nsi_db_analyze",
      "--testrun", testrun_num,
      "--interval", interval_buf,
      "--ppid", db_upload_pid,
      NULL) == -1)
    {
      NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "Main Thread", NSLB_TL_ERROR, "Initializing DB ANALYZE: error in execl.");
      fprintf(stderr, "Initializing DB ANALYZE: error in execlp\n");
      perror("execl");
      exit(-1);
    }
  }
  else
  {
    if (db_analyze_pid < 0) {
      fprintf(stderr, "error in forking the DB Analyze process.\n");
    }
  }
}
*/

void setDbuDebugLogFile(db_upload *dbUploadInit)
{ 
  FILE *fp = NULL;
  
  //Open file to log copy command error 
  sprintf(dbUploadInit->error_file_name,"%s/logs/TR%d/db_upload_error.log", dbUploadInit->ns_wdir , dbUploadInit->tr_num);
  
  fp = fopen(dbUploadInit->error_file_name, "a+e");
  
  if(!fp)
  { 
    NSLB_TRACE_LOG1(dbUploadInit->trace_log_key, dbUploadInit->partition_idx, "MAIN_THREAD", NSLB_TL_INFO, "Error in opening file '%s'", dbUploadInit->error_file_name);
  }
  else
    chown(dbUploadInit->error_file_name, dbUploadInit->ownerid, dbUploadInit->grpid);
  if(fp)
   fclose(fp);
  
  fp = NULL;
  
  //open file to log table creation log 
  sprintf(dbUploadInit->db_table_log_file_name, "%s/logs/TR%d/DBUploadTable.log", dbUploadInit->ns_wdir, dbUploadInit->tr_num);
  
  fp = fopen(dbUploadInit->db_table_log_file_name, "a+e");
  if(!fp)
  { 
    NSLB_TRACE_LOG1(dbUploadInit->trace_log_key, dbUploadInit->partition_idx, "MAIN_THREAD", NSLB_TL_INFO, "Error in opening file '%s'", dbUploadInit->db_table_log_file_name);
  }
  else
    chown(dbUploadInit->db_table_log_file_name, dbUploadInit->ownerid, dbUploadInit->grpid);

  if(fp)
   fclose(fp);
}

/*#define ORACLE_STATS_MON_FILE "oracle_sql_report"
#define MSSQL_STATS_MON_FILE "mssql_report"
#define GENERIC_DB_STATS_MON_FILE "genericDb_report"*/

/*Bug#91417_RUNTIME_MONITORS_APPLIED*/
void check_generic_db_monitors_files(db_upload *ns_db_upload, char *pathName)
{
  char retCheck;

  for(int monitorsInfoPtrIdx = 0; monitorsInfoPtrIdx < ns_db_upload->totalMonitorsInfoPtrIdx; monitorsInfoPtrIdx++)
  {
    retCheck = check_for_stats_mon(ns_db_upload, ns_db_upload->monitorsInfoPtr[monitorsInfoPtrIdx].monitorsAppliedFileName,
                                   pathName, "Main Thread");
    if(retCheck)
    {
      NSLB_TRACE_LOG1(ns_db_upload->trace_log_key, ns_db_upload->partition_idx, "Main Thread", NSLB_TL_INFO,
                      "monitorsAppliedFileName[%s] exist", 
                       ns_db_upload->monitorsInfoPtr[monitorsInfoPtrIdx].monitorsAppliedFileName);

      if(ns_db_upload->monitorsInfoPtr[monitorsInfoPtrIdx].fileType == ORCL_MON_DYN_CSV_FILE_TYPE)
      {
        if(!ns_db_upload->enable_oracle_stats_reports_threads)
        {
          ns_db_upload->enable_oracle_stats_reports_threads = 1;
          NSLB_TRACE_LOG1(ns_db_upload->trace_log_key, ns_db_upload->partition_idx, "Main Thread", NSLB_TL_INFO,
                          "enable_oracle_stats_reports_threads[%d]", ns_db_upload->enable_oracle_stats_reports_threads);
          sprintf(ns_db_upload->orlEndMarkerFile, ".orl_process_done.txt");
        }
      }
      else
      {
        if(!(ns_db_upload->enable_generic_db_monitors_threads & 1<<ns_db_upload->monitorsInfoPtr[monitorsInfoPtrIdx].fileType))
        {
          NSLB_TRACE_LOG1(ns_db_upload->trace_log_key, ns_db_upload->partition_idx, "Main Thread", NSLB_TL_INFO,
                          "enable_generic_db_monitors_threads[%d]", ns_db_upload->enable_generic_db_monitors_threads);
          ns_db_upload->enable_generic_db_monitors_threads |= 1<<ns_db_upload->monitorsInfoPtr[monitorsInfoPtrIdx].fileType;
        }
      }
    }
    else
    {
      NSLB_TRACE_LOG1(ns_db_upload->trace_log_key, ns_db_upload->partition_idx, "Main Thread", NSLB_TL_INFO,
                      "monitorsAppliedFileName[%s] doesnt exist", 
                       ns_db_upload->monitorsInfoPtr[monitorsInfoPtrIdx].monitorsAppliedFileName);
    }
  }

  if(ns_db_upload->enable_generic_db_monitors_threads)
  {
    if(!(ns_db_upload->enable_generic_db_monitors_threads & 1<<GEN_MON_PARENT_CSV_FILE_TYPE))
    {
      ns_db_upload->enable_generic_db_monitors_threads |= 1<<GEN_MON_PARENT_CSV_FILE_TYPE;
      sprintf(ns_db_upload->genericDbMonEndMarkerFile, ".genericDb_process_done.txt");
    }
  }
}

/*Bug#91417_RUNTIME_MONITORS_APPLIED*/
void init_inotify_and_add_watch(db_upload *ns_db_upload)
{
  char errMsg[1024];
  char path[1024];

  ns_db_upload ->inotify_info_ptr = nslb_inotify_init(errMsg, 1024);

  if(!ns_db_upload ->inotify_info_ptr)
  {
    NSLB_TRACE_LOG1(ns_db_upload->trace_log_key, ns_db_upload->partition_idx, "Main Thread", NSLB_TL_INFO,
                    "Error [%s] in nslb_inotify_init", errMsg);

    nslb_db_upload_exit(-1, "Error in nslb_inotify_init", ns_db_upload);
  }

  NSLB_TRACE_LOG1(ns_db_upload->trace_log_key, ns_db_upload->partition_idx, "Main Thread", NSLB_TL_INFO,
                  "Successfully init inotify_info_ptr with inotify_fd[%d]", ns_db_upload ->inotify_info_ptr->inotify_fd);

  sprintf(path, "%s/common_files", ns_db_upload->base_dir);

  ns_db_upload->commonFilesDir_wd = nslb_inotify_add_watch(ns_db_upload->inotify_info_ptr->inotify_fd, path, IN_CREATE,                                                                    errMsg, 1024);

  if(ns_db_upload->commonFilesDir_wd == -1)
  {
    NSLB_TRACE_LOG1(ns_db_upload->trace_log_key, ns_db_upload->partition_idx, "Main Thread", NSLB_TL_INFO,
                    "Error [%s] in nslb_inotify_add_watch for path[%s] and event[IN_CREATE]", errMsg, path);

    nslb_db_upload_exit(-1, "Error in nslb_inotify_add_watch", ns_db_upload);
  }
  
  NSLB_TRACE_LOG1(ns_db_upload->trace_log_key, ns_db_upload->partition_idx, "Main Thread", NSLB_TL_INFO,
                  "Successfully added path[%s] with event[IN_CREATE] for commonFilesDir_wd[%d]", path, 
                  ns_db_upload->commonFilesDir_wd);
}

/*Bug#91417_RUNTIME_MONITORS_APPLIED*/
#define SECS_IN_ONE_DAY 86400
void checking_genericDb_monitors_hardlink_in_partitions()
{
  char currPartition[512];
  char prevPartition[512];
  char pathName[1024];
  int retCheck;
  long long currPartitionInSecs;

  strcpy(currPartition, ns_db_upload.partition_name);
  currPartitionInSecs = nslb_get_time_in_secs(currPartition);

  while(1)
  {
    NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "Main Thread",
                    NSLB_TL_INFO, "currPartition[%s]", currPartition);
  
    retCheck = nslb_get_prev_partition(ns_db_upload.base_dir, currPartition, prevPartition);
    if(!retCheck)
    {
      NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "Main Thread",
                      NSLB_TL_INFO, "checking hardLink in prevPartition[%s]", prevPartition);

      sprintf(pathName, "%s/%s/reports/csv", ns_db_upload.base_dir, prevPartition);
      check_generic_db_monitors_files(&ns_db_upload, pathName);
     
      if(ns_db_upload.enable_generic_db_monitors_threads == ns_db_upload.allGenericMonFileTypes &&
         ns_db_upload.enable_oracle_stats_reports_threads == 1)
      {
        NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "Main Thread",
                        NSLB_TL_INFO, "All hardLinks are found for genericDb as well as oracle moniotrs");
        break;
      }

      if(currPartitionInSecs - nslb_get_time_in_secs(prevPartition) >=                                                                   ns_db_upload.daysToCheckMonHardlink * SECS_IN_ONE_DAY)
      {
        NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "Main Thread",
                        NSLB_TL_INFO, "Time difference between NSDBU currPartition[%s] and currPartition [%s] "
                        "is greater than [%d] days", ns_db_upload.partition_name, prevPartition, 
                        ns_db_upload.daysToCheckMonHardlink);
        break;
      } 
    }
    else
    {
      NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "Main Thread",
                      NSLB_TL_INFO, "Error in getting prevPartition for currPartition[%s] "
                      "nslb_get_prev_partition returned[%d] Value 1 means reach first partition", currPartition, 
                      retCheck);
      break;
    }
    strcpy(currPartition, prevPartition);
  }
}


/*Bug#91417_RUNTIME_MONITORS_APPLIED*/
int threads_creation_for_oracle_or_genericDb_monitors(int dynCsvIdx)
{
  if(ns_db_upload.dbUploaderDynamicCSVInitStruct[dynCsvIdx].fileType == ORCL_MON_DYN_CSV_FILE_TYPE ||
     (ns_db_upload.allGenericMonFileTypes & 1 << ns_db_upload.dbUploaderDynamicCSVInitStruct[dynCsvIdx].fileType))
  {
    pthread_mutex_init(&(ns_db_upload.dbUploaderDynamicCSVInitStruct[dynCsvIdx].threadLock), NULL);

    if(ns_db_upload.dbUploaderDynamicCSVInitStruct[dynCsvIdx].fileType == ORCL_MON_DYN_CSV_FILE_TYPE)
    {
      if(ns_db_upload.enable_oracle_stats_reports_threads)
      {
        ns_db_upload.dbUploaderDynamicCSVInitStruct[dynCsvIdx].threadIsRunning = 1;
        //createMainTable(&ns_db_upload.dbUploaderDynamicCSVInitStruct[dynCsvIdx], &ns_db_upload);
        return 0;
      }
      else
      {
        return 1;
      }
    }
    else
    {
      if(ns_db_upload.enable_generic_db_monitors_threads &
         1 << ns_db_upload.dbUploaderDynamicCSVInitStruct[dynCsvIdx].fileType)
      {
        ns_db_upload.dbUploaderDynamicCSVInitStruct[dynCsvIdx].threadIsRunning = 1;
        createMainTable(&ns_db_upload.dbUploaderDynamicCSVInitStruct[dynCsvIdx], &ns_db_upload, "Main Thread");
        createGenViewsAndMetaTables=1;

        return 0;
      }
      else
      {
        return 1;
      }
    }
  }
  return 0;
}

void * threadStartRoutine(void *arg)
{
  char dynamicThreadName[256];

  int dynamicCsvIdx = *((int *)(arg));
  
  sprintf(dynamicThreadName, "%s_thread", ns_db_upload.dbUploaderDynamicCSVInitStruct[dynamicCsvIdx].csvtable); 
  
  NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, dynamicThreadName, NSLB_TL_INFO, 
                  "Thread started to process '%s'", ns_db_upload.dbUploaderDynamicCSVInitStruct[dynamicCsvIdx].csvName);

  dynamicCsvDataLoader(dynamicThreadName, ns_db_upload.dbUploaderDynamicCSVInitStruct[dynamicCsvIdx].csvName, 0, dynamicCsvIdx,                        &ns_db_upload, NULL);  
  
  return NULL;
}


void create_threads_for_fileType(int fileType)
{
  int retCheck;
  for(int dynamicCsvIdx = 0; dynamicCsvIdx < ns_db_upload.totalCsvEntry; dynamicCsvIdx++)
  {
    if(fileType == ns_db_upload.dbUploaderDynamicCSVInitStruct[dynamicCsvIdx].fileType)
    {
      threadargs[dynamicCsvIdx+1] = dynamicCsvIdx;
      pthread_mutex_lock(&ns_db_upload.dbUploaderDynamicCSVInitStruct[dynamicCsvIdx].threadLock);
        if(!ns_db_upload.dbUploaderDynamicCSVInitStruct[dynamicCsvIdx].threadIsRunning)
        {
          ns_db_upload.dbUploaderDynamicCSVInitStruct[dynamicCsvIdx].threadIsRunning = 1; 
          createMainTable(&ns_db_upload.dbUploaderDynamicCSVInitStruct[dynamicCsvIdx], &ns_db_upload, "Main Thread");
          createGenViewsAndMetaTables=1;

          NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "Main Thread", NSLB_TL_INFO,
                          "Creating thread for CSVFILE[%s]", 
                          ns_db_upload.dbUploaderDynamicCSVInitStruct[dynamicCsvIdx].csvName);

          retCheck = pthread_create(&thread[dynamicCsvIdx+1], &attr, threadStartRoutine, &threadargs[dynamicCsvIdx+1]);
          if (retCheck)
          {
            NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "Main Thread", NSLB_TL_ERROR, 
                            "pthread_create() failed, Error = %s", nslb_strerror(errno));
            nslb_db_upload_exit(-1, "Internal Error(pthread_error)", &ns_db_upload);
          }
         
          char threadName[16];
          snprintf(threadName, 16, "%s_thread", ns_db_upload.dbUploaderDynamicCSVInitStruct[dynamicCsvIdx].csvtable);
          
          if(pthread_setname_np(thread[dynamicCsvIdx+1], threadName))
            NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "Main Thread", NSLB_TL_ERROR,                                           "ERROR[%s] in pthread_setname_np for threadName[%s]", strerror(errno), threadName);
        }
        else
        {
          NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "Main Thread", NSLB_TL_INFO,
                          "threadIsRunning for CSVFILE[%s]", 
                          ns_db_upload.dbUploaderDynamicCSVInitStruct[dynamicCsvIdx].csvName);
        }
      pthread_mutex_unlock(&ns_db_upload.dbUploaderDynamicCSVInitStruct[dynamicCsvIdx].threadLock);
    }
  }
}


void match_file_name_with_monitorsAppliedFileName_and_start_threads(db_upload *dbUploadInit, char *file_name)
{
  for(int monitorsInfoPtrIdx = 0; monitorsInfoPtrIdx < dbUploadInit->totalMonitorsInfoPtrIdx; monitorsInfoPtrIdx++)
  {
    if(!strcmp(file_name, dbUploadInit->monitorsInfoPtr[monitorsInfoPtrIdx].monitorsAppliedFileName))
    {
      NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "Main Thread", NSLB_TL_INFO,
                      "create_threads_for_fileType[%d]", dbUploadInit->monitorsInfoPtr[monitorsInfoPtrIdx].fileType);

      create_threads_for_fileType(dbUploadInit->monitorsInfoPtr[monitorsInfoPtrIdx].fileType);

      if(dbUploadInit->monitorsInfoPtr[monitorsInfoPtrIdx].parentFileType)
      {
        NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "Main Thread", NSLB_TL_INFO,
                        "create_threads_for_parentFileType[%d]", 
                        dbUploadInit->monitorsInfoPtr[monitorsInfoPtrIdx].parentFileType);

        create_threads_for_fileType(dbUploadInit->monitorsInfoPtr[monitorsInfoPtrIdx].parentFileType);
      }
      break;
    }
  }
}
    
/*Bug#91417_RUNTIME_MONITORS_APPLIED*/
void inotify_event_handling(inotify_info *inotify_info_ptr, db_upload *dbUploadInit)
{
  char errMsg[1024];
  
  int retCheck = nslb_read_from_inotify_fd_and_fill_into_inotify_wd_events_info_ptr(inotify_info_ptr, errMsg, 1024);
  if(retCheck < 0)
  {
    NSLB_TRACE_LOG1(dbUploadInit->trace_log_key, dbUploadInit->partition_idx, "Main Thread", NSLB_TL_INFO, 
                    "ERROR[%s] while reading from inotify_fd[%d]", inotify_info_ptr->inotify_fd, errMsg);
    //TODO
    switch(retCheck)
    { 
      case E_AGAIN:                                                                               
      case ERROR_IN_ARGS:                                                                      
      case READ_ERROR:
      case ZERO_BYTES:
        break;
    }
  }
  else
  {
    NSLB_TRACE_LOG1(dbUploadInit->trace_log_key, dbUploadInit->partition_idx, "Main Thread", NSLB_TL_INFO,
                    "GOT [%d] watch_descriptors events", inotify_info_ptr->wd_events_count);
  
    for(int inotify_wd_events_info_ptr_idx = 0; inotify_wd_events_info_ptr_idx < inotify_info_ptr->wd_events_count;
        inotify_wd_events_info_ptr_idx++)
    {
      NSLB_TRACE_LOG1(dbUploadInit->trace_log_key, dbUploadInit->partition_idx, "Main Thread", NSLB_TL_INFO,
                      "EVENT for watch_descriptor[%d] and file[%s]", 
                      inotify_info_ptr->inotify_wd_events_info_ptr[inotify_wd_events_info_ptr_idx].watch_descriptor,
                      inotify_info_ptr->inotify_wd_events_info_ptr[inotify_wd_events_info_ptr_idx].file_name); 
      
      if(inotify_info_ptr->inotify_wd_events_info_ptr[inotify_wd_events_info_ptr_idx].watch_descriptor == 
         dbUploadInit->commonFilesDir_wd) 
      {
        if(inotify_info_ptr->inotify_wd_events_info_ptr[inotify_wd_events_info_ptr_idx].file_name[0])
        {
          match_file_name_with_monitorsAppliedFileName_and_start_threads(dbUploadInit, 
          inotify_info_ptr->inotify_wd_events_info_ptr[inotify_wd_events_info_ptr_idx].file_name);
        }
      }
    }
  }
}
  
void createViewsAndMetaDataTablesForGenericMonitors()
{
  NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "Main Thread", NSLB_TL_INFO,                                           "CREATION OF GENERIC META DATA TABLES");

  for(int csvIdx = 0; csvIdx < ns_db_upload.totalStaticCsvEntry; csvIdx++) 
  {
    if(ns_db_upload.dbUploaderStaticCSVInitStruct[csvIdx].fileType == GEN_MON_META_CSV_FILE_TYPE) 
      createMainTable(&ns_db_upload.dbUploaderStaticCSVInitStruct[csvIdx], &ns_db_upload, "Main Thread");
  }
  
  char cmd[512];
  char err_msg[1024] = "\0";
 
  sprintf(cmd, "%s/bin/generic_db_mon_views_creation %d", ns_db_upload.ns_wdir, ns_db_upload.tr_num);
  
  if(nslb_system(cmd, 1, err_msg) != 0)
  {
    NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "Main Thread", NSLB_TL_INFO,                                           "Error[%s] while executing cmd[%s] checks logs in FILE[%s/logs/TR%d/.genericDBMonitorViewsLogs]",                                err_msg, cmd, ns_db_upload.ns_wdir, ns_db_upload.tr_num);
  }
  else
  {
    NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "Main Thread", NSLB_TL_INFO,                                           "SUCCESS IN VIEW CREATION");
  }

  createGenViewsAndMetaTables=0;
}


#endif



#if NSU_DB_UPLOAD 
int main(int argc, char *argv[])
{
  char cur_time_buf[100];
  int ret ;
   
  //Sigchild signal for handling defunct process
  signal (SIGCHLD, sigchild_handle);
  
  sigemptyset(&set);
  sigaddset(&set, SIGALRM);
  sigaddset(&set, SIGTERM);
  sigaddset(&set, SIGRTMIN+2);
  sigaddset(&set, TEST_POST_PROC_SIG);
  sigaddset(&set, TRACE_LEVEL_CHANGE_SIG);

  struct sigaction sa;
  sigset_t sigset;

  bzero (&sa, sizeof(struct sigaction));
  sa.sa_handler = sigalarm_handler;
  sigaction(SIGALRM, &sa, NULL);

  bzero (&sa, sizeof(struct sigaction));
  sa.sa_handler = test_post_proc_sig_handler;
  sigaction(TEST_POST_PROC_SIG, &sa, NULL);

  bzero(&sa, sizeof(struct sigaction));
  sa.sa_handler = partition_switch;
  sigaction(SIGRTMIN+2, &sa, NULL);

  bzero (&sa, sizeof(struct sigaction));
  sa.sa_handler = sigterm_handler;
  sigaction(SIGTERM, &sa, NULL);

  bzero (&sa, sizeof(struct sigaction));
  sa.sa_handler = rt_trace_change;
  sigaction(TRACE_LEVEL_CHANGE_SIG, &sa, NULL);

  alarm(1);
  sigfillset (&sigset);
  sigprocmask (SIG_SETMASK, &sigset, NULL);

 // Init ns_db_upload
  nslb_init_db_upload_before_parsing_args(&ns_db_upload, 0);

  /* Parse command line argumnets */
  nsu_parse_args(argc, argv);

  sprintf(ns_db_upload.base_dir, "%s/logs/TR%d", ns_db_upload.ns_wdir, ns_db_upload.tr_num);
  sprintf(ns_db_upload.CSVConfigFilePath, "%s/etc/dbconf/ns_db_csv.conf", ns_db_upload.ns_wdir);
  sprintf(ns_db_upload.schemaConfFilePath, "%s/etc/dbconf/ns_db_schema.conf", ns_db_upload.ns_wdir);

  // This should not move to lib
  nslb_set_cur_partition_shm(&ns_db_upload);

  // Init trace log
  ns_db_upload.trace_log_key = nslb_init_mt_trace_log(ns_db_upload.ns_wdir, ns_db_upload.tr_num, 
                                                      ns_db_upload.partition_idx?ns_db_upload.partition_idx:-1, "ns_logs/ns_db_trace.log", 
                                                      ns_db_upload.trace_level, 10);

  //Init table error trace log.
  ns_db_upload.copyError_trace_log_key = nslb_init_mt_trace_log(ns_db_upload.ns_wdir, ns_db_upload.tr_num, 
                                                      ns_db_upload.partition_idx?ns_db_upload.partition_idx:-1,                                                                                      "ns_logs/nsu_db_upload_copyError.trace", ns_db_upload.trace_level, 10);

  sprintf(ns_db_upload.endMarkerFile, ".nlr_process_done.txt");

  // Check for the presence of Oracle Stats Report Monitor by checking for the presence of .oracle_stats_mon_enabled 
  /*ret = check_for_stats_mon(&ns_db_upload, ORACLE_STATS_MON_FILE);
  if(ret)
  {
    ns_db_upload.enable_oracle_stats_reports_threads = 1;
    ns_db_upload.num_tables = 30;
    sprintf(ns_db_upload.orlEndMarkerFile, ".orl_process_done.txt");
  }*/

  // Check for the presence of MSSQL Stats Report Monitor by checking for the presence of .ms_sql_stats_mon_enabled 
  //ret = check_for_stats_mon(&ns_db_upload, MSSQL_STATS_MON_FILE);
  
  /*BUG 84397
  ret = check_for_stats_mon(&ns_db_upload, GENERIC_DB_STATS_MON_FILE);
  if(ret)
  {
    ns_db_upload.enable_mssql_stats_reports_threads = 1;
    ns_db_upload.num_tables += 2;
    //sprintf(ns_db_upload.msSqlEndMarkerFile, ".mssql_process_done.txt");
    sprintf(ns_db_upload.genericDbMonEndMarkerFile, ".genericDb_process_done.txt");
  }*/
  
  /*Bug#91417_RUNTIME_MONITORS_APPLIED*/

  /*Parse file for generic and oracle monitors and fill into monitorsInfo strct*/
  char monInfoFilePath[1024];
  sprintf(monInfoFilePath, "%s/etc/dbconf/ns_db_monitorsInfo.conf", ns_db_upload.ns_wdir);

  if(monInfoConfigParser(&ns_db_upload, monInfoFilePath))
  {
    fprintf(stderr, "Error occurred while parsing monitorsInfo  configuration file (%s). Error : %s\n", 
                     monInfoFilePath, ns_db_upload.error);
    exit(EXIT_FAILURE);
  }
 
  /*If Online mode- init inotify and add common_files dir for watch(IN_CREATE)*/ 
  if(ns_db_upload.running_mode)
  {
    init_inotify_and_add_watch(&ns_db_upload);
  }

  /*settings vars on the basis of mon running hidden files exist in common_files dir*/
  char pathName[1024];
  sprintf(pathName, "%s/common_files", ns_db_upload.base_dir);
  check_generic_db_monitors_files(&ns_db_upload, pathName);
 
  //getting controller name  
  ns_db_upload.controller = basename(ns_db_upload.ns_wdir);
  
  /*check machine type*/
  check_machine_type_is_sm(&ns_db_upload);


  /* Parsing Configuration NSDBSchema.conf and populating structures. */
  if(CSVConfigParser(&ns_db_upload)){
    fprintf(stderr, "Error occurred while parsing CSV configuration file (%s). Error : %s\n", 
                     ns_db_upload.CSVConfigFilePath, ns_db_upload.error);
    exit(EXIT_FAILURE);
  }

  /* Parsing Configuration NSCSVFiles.conf and populating structures. */
  if(schemaFileParser(&ns_db_upload)){
    fprintf(stderr, "Error occurred while parsing schema configuration file (%s). Error : %s\n", 
                                         ns_db_upload.schemaConfFilePath, ns_db_upload.error);
    exit(EXIT_FAILURE);
  }
  
  //so if trace level will be greater or equal to 3 then only we will enable pg_bulkload_debug_flag.
  if(ns_db_upload.trace_level >= 3)
  {
    ns_db_upload.pg_bulkload_debug_flag = 1;
    dumpConfigFilePopulatedValues(&ns_db_upload);
  }

  
  //getting controller name 
  ns_db_upload.controller = basename(ns_db_upload.ns_wdir);

  /*Bug#96763*/
  nslb_get_days_to_check_offset_file(&ns_db_upload);
 
  /*Bug#91417_RUNTIME_MONITORS_APPLIED*/
  nslb_get_days_to_check_genericDb_monitors_hardlink(&ns_db_upload);

  nslb_get_dbu_monitor_set(&ns_db_upload);

  //waiting for process done before switching
  //nslb_get_process_done_creation_wait_time(&ns_db_upload);

  //getting no of iterations to wait for process done
  nslb_get_iterations_to_wait_before_partiton_switch(&ns_db_upload);

 //set anlayze thread for main tables  
  nslb_get_main_table_analyze_thread_set(&ns_db_upload);

  //set csv_sleep_interval and csv_retry_count 
  dbu_retry_and_check_csv(&ns_db_upload);
  
  //Get database id
  nslb_get_database_id(&ns_db_upload);

  //Get Partition switch delay time
  nslb_get_partition_switch_delay_time(&ns_db_upload);
  
  //Get DB ANALYZE Interval
  nslb_get_db_analyze_interval_count(&ns_db_upload);
  
  get_dbu_enabled_filter_csv_data(&ns_db_upload);

  /* Calling this function before fork_db_analyze, 
     because nsdbu pid is set in this function which is used by fork_db_analyze */
  nslb_init_after_parseArgs(&ns_db_upload);
 
  //create new directory 
  sprintf(ns_db_upload.DbuPostgresPidsDir, "%s/logs/TR%d/.pidfiles", ns_db_upload.ns_wdir, ns_db_upload.tr_num); 

  struct stat st;

  //check directory exists or not if not exists then create
  if(stat(ns_db_upload.DbuPostgresPidsDir, &st) == -1)
  {
    ret = mkdir (ns_db_upload.DbuPostgresPidsDir, 0777);
    if(ret != 0)
    {
      fprintf(stderr, "Error [%s] while creating Dir [%s]", nslb_strerror(errno), ns_db_upload.DbuPostgresPidsDir);
      
      NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "Main Thread", NSLB_TL_INFO,
                      "Error [%s] while creating Dir [%s]", nslb_strerror(errno), ns_db_upload.DbuPostgresPidsDir);
      exit(EXIT_FAILURE);
    
    }
    else//change ownership of file
    {
      if(chown(ns_db_upload.DbuPostgresPidsDir, ns_db_upload.ownerid, ns_db_upload.grpid)) 
      {
        NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "Main Thread", NSLB_TL_ERROR,                                                     "Failed to set owner of file [%s], Error  [%s]", ns_db_upload.DbuPostgresPidsDir, nslb_strerror(errno));
      
        fprintf(stderr, "Error [%s] while changing ownership of Dir [%s]", nslb_strerror(errno), ns_db_upload.DbuPostgresPidsDir);
       
        exit(EXIT_FAILURE);
      }
    }
  }

  setDbuDebugLogFile(&ns_db_upload);

  /* Fill nsi_db_analyze shell recovery structure */
  nslb_init_component_recovery_data(&db_analyze_recovery, 5, 10, 900);

  //Forking ANALYZE Shell
  /* 
  if(ns_db_upload.db_analyze_interval && ns_db_upload.running_mode && (ns_db_upload.partition_name[0] != '\0'))
  {
    fork_db_analyze();
  }
  */

  //Set db upload tool
  nslb_get_db_upload_tool(&ns_db_upload);
  //set validate_csv_buffer
  nslb_get_validate_csv_buffer(&ns_db_upload);

  nslb_get_csv_mode(&ns_db_upload);

  //get progress interval
  nslb_get_progress_interval(&ns_db_upload);

  //nslb_init_after_parse_args(&ns_db_upload);

  /* Check for DB_TMP_FILE_PATH path either this directory path does not exites or does not Write permissions.
   * Reset the value so that it will take default path TRXXX/.tmp/.
   */
//#ifdef PG_BULKLOAD
  //Get DB Tmp File Path from sorted_scenario
  char tmp_path[1024] = "";
 
  if(g_db_upload_command == PG_BULKLOAD_TOOL)
  {
    if(ns_db_upload.dbTmpFilePath[0] == '\0')
      nslb_get_tmp_file_path(&ns_db_upload);
 
    if(ns_db_upload.dbTmpFilePath[0] != '\0')
    { 
      char cmd[512], cmd_out[1024] = "", *ptr;
    
      sprintf(cmd, "stat -c '%%a' %s 2>/dev/null", ns_db_upload.dbTmpFilePath);
      nslb_run_cmd_and_get_last_line(cmd, 1024, cmd_out);
    
      ptr = cmd_out;
      if(strlen(cmd_out) > 3) //777
        ptr += strlen(cmd_out) - 3;
    
      if(strcmp(ptr, "777")) //Permissions not present
      {
        NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "Main Thread", NSLB_TL_INFO,
                         "Keyword DB_TMP_FILE_PATH is present in scenario with path '%s', "
                         "but either this directory path does not exist or does not have 777 "
                         "permissions. So temp file will be created in '%s/logs/TR%d/.tmp/' directory. "
                         "Current permission = '%s'.", ns_db_upload.dbTmpFilePath, ns_db_upload.ns_wdir,
                          ns_db_upload.tr_num, ptr[0]?ptr:"Directory not present");
        ns_db_upload.dbTmpFilePath[0] = '\0';
 
      }
    }
 
    if(ns_db_upload.dbTmpFilePath[0] != '\0')
      sprintf(tmp_path, "%s", ns_db_upload.dbTmpFilePath);
    else
      sprintf(tmp_path, "%s/webapps/logs/TR%d/.tmp/" , ns_db_upload.ns_wdir, ns_db_upload.tr_num);
 
    // Calling the clean file function
    fork_pg_bulkload_tmp_file_cleanup(&ns_db_upload, tmp_path);
 
    NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "Main Thread", NSLB_TL_INFO, 
                     "Final dbTmpFilePath = %s", tmp_path);
  }
//#endif

  //Get chunk size from sorted_scenario, 
  //this function will set default chunk size 128 MB if fails to set from scenario
  //always call this function after nslb_get_tmp_file_path, 
  //because variable tmpfs_size_in_mb is set in nslb_get_tmp_file_path and used by nslb_get_dbu_chunk_size.
  if(ns_db_upload.chunk_size <= 0)
    nslb_get_dbu_chunk_size(&ns_db_upload);

  NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "Main Thread", NSLB_TL_INFO, 
                  "Final ns_db_upload.chunk_size = '%lld'", ns_db_upload.chunk_size);

  //Validate some imp dbu/postgresql variable value
  nslb_analyze_system_variable(&ns_db_upload);

 
  // offline mode 
  if(!ns_db_upload.running_mode)
    ns_db_upload.is_test_running = 0;

  int numThreads = ns_db_upload.numThreads;
  //one more thread for nsi_db_analyze so increment ns_db_upload.numThreads by 1 
   if(ns_db_upload.usr_provided_partition < 0 && ns_db_upload.dbu_analyze_thread_main_table_on_off)
     ns_db_upload.numThreads = ns_db_upload.numThreads + 1;
 
  if(ns_db_upload.running_mode && ns_db_upload.usr_provided_partition < 0)
  {
    if ((v_epoll_fd = epoll_create(1)) == -1)
    { 
      nslb_db_upload_exit(-1, "epoll_init, epoll_create: err =\n", &ns_db_upload);
    }

    /*Bug#91417_RUNTIME_MONITORS_APPLIED*/
    /*add inotify_fd in epoll*/
    if(ns_db_upload.inotify_info_ptr->inotify_fd)
    {
      NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "Main Thread", NSLB_TL_INFO,
                      "ADDED inotify_fd[%d] in v_epoll_fd[%d]", ns_db_upload.inotify_info_ptr->inotify_fd, v_epoll_fd);

      nslb_mon_reg_add_select(v_epoll_fd, ns_db_upload.inotify_info_ptr->inotify_fd, EPOLLIN|EPOLLHUP|EPOLLERR);
    }

    int threadCount;

    if(ns_db_upload.dbu_analyze_thread_main_table_on_off)
      threadCount = ns_db_upload.numThreads - 1;
    else
      threadCount = ns_db_upload.numThreads;


    //allocate dbu monitor stat log
    if(ns_db_upload.dbu_monitor_on_off)
    {
      NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "Main Thread", NSLB_TL_INFO,
                                                                   "Going to allocate dbu monitor stat structure.");
      if(nslb_init_dbu_monitor_stats(&(ns_db_upload.dbuMonitorStat), threadCount) == -1)
        nslb_db_upload_exit(-1, "Malloc Failed of dbu_monitor_stat_struct", &ns_db_upload);

      NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "Main Thread",
                    NSLB_TL_INFO, "Going to init_dbu_monitor");
      //initialise dbu monitor structure only if dbu is in online mode 
      init_dbu_monitor();
    }
  }

  /*Bug#91417_RUNTIME_MONITORS_APPLIED*/
  /*check monitors hardlink in previous partitions in (ONLINE + OFFLINE) modes*/ 
  if(ns_db_upload.enable_generic_db_monitors_threads != ns_db_upload.allGenericMonFileTypes ||
     ns_db_upload.enable_oracle_stats_reports_threads != 1)
  {
    checking_genericDb_monitors_hardlink_in_partitions();
  }
 
  NSLB_TRACE_LOG2(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "Main Thread", NSLB_TL_INFO, 
                  "DataBase uploading start at %s", nslb_get_cur_date_time(cur_time_buf, 0));

  NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "Main Thread", NSLB_TL_INFO, "Starting db threads.");

  threadargs = malloc(ns_db_upload.numThreads * sizeof(int));
  int t;
  
  thread = (pthread_t *) malloc(ns_db_upload.numThreads * sizeof(pthread_t));
  pthread_attr_init(&attr);

  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  //pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  pthread_sigmask(SIG_BLOCK, &set, NULL);

  char threadName[16];

  for(t = 0; t < numThreads; t++)
  {
    threadargs[t] = t+1;
    if(!ns_db_upload.totalStaticCsvEntry && !t)
    { 
      continue;
    }

    NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "Main Thread", NSLB_TL_INFO,                                            "Creating thread %d.", threadargs[t]);

    if(threadargs[t] != 1)
    {
      /*Bug#91417_RUNTIME_MONITORS_APPLIED*/
      /*func will first check if file type is for oracle or generic monitors and
        then bit is set for that monitors or not*/ 
      if(threads_creation_for_oracle_or_genericDb_monitors(threadargs[t]-2))
      { 
        continue;
      }
    }

    ret = pthread_create(&thread[t], &attr, loaderThreadStartRoutine, (void *)&threadargs[t]);
    if(ret)
    {
      NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "Main Thread", NSLB_TL_ERROR,                                           "ERROR: Return code from pthread_create() is %d", threadargs[t]);
      nslb_db_upload_exit(-1, "Error in creating thread", &ns_db_upload);
    }
   
    t ? snprintf(threadName, 16, "%s_thread", ns_db_upload.dbUploaderDynamicCSVInitStruct[t - 1].csvtable) :                            snprintf(threadName, 16, "Metadata_thread");

    if(pthread_setname_np(thread[t], threadName))
      NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "Main Thread", NSLB_TL_ERROR,                                           "ERROR[%s] in pthread_setname_np for threadName[%s]", strerror(errno), threadName);
  }

  if(createGenViewsAndMetaTables)
    createViewsAndMetaDataTablesForGenericMonitors();
  
  
  if(ns_db_upload.usr_provided_partition < 0 && ns_db_upload.dbu_analyze_thread_main_table_on_off)
  {
    NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, ANALYSE_THRD, NSLB_TL_INFO, "Creating thread - %s", ANALYSE_THRD);
      
    ret = pthread_create(&thread[t], &attr, analyzeThreadStartRoutine, (void *)&ns_db_upload); 
    if (ret)
    {
      NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, ANALYSE_THRD, NSLB_TL_ERROR, "pthread_create() failed, "
      "Error = %s", nslb_strerror(errno));
      nslb_db_upload_exit(-1, "Internal Error(pthread_error)", &ns_db_upload);
    }
  }

  //pthread_attr_destroy(&attr);
  pthread_sigmask(SIG_UNBLOCK, &set, NULL);

  int pfds_idx;
  int event_count = -1;
  char read_buf[MAX_READ_LENGTH + 1];
  char err_msg[BUFFER_LEN + 1] = "";
  char *partial_buf = NULL;
  int partial_buf_len, bytes_read = 0;


  //For Communication with NS
  static struct epoll_event pfds[2];
  //last_recorded_time set
  int time_diff = 0;
  int curr_time;
  last_records_sent_time = time(NULL);

  while(1){
    err_msg[0] = '\0';
    if(ns_db_upload.running_mode && ns_db_upload.usr_provided_partition < 0) 
    {
      //check offline mode here
      if(ns_db_upload.dbu_monitor_on_off)
      {
        curr_time = time(NULL);
        time_diff = (curr_time - last_records_sent_time) * 1000;
       
        //NSLB_TRACE_LOG3(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "Main Thread", NSLB_TL_INFO, "time_diff - %d", time_diff);
        // interval retrieved from scenario
      
        if(time_diff >= ns_db_upload.progress_interval)
        {
          NSLB_TRACE_LOG2(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "Main Thread", NSLB_TL_INFO, "time_diff - %d > prgrs_intrval -%d"      , time_diff, ns_db_upload.progress_interval);
          if(ns_db_upload.usr_provided_partition == -1 && ns_db_upload.running_mode)
          {
            send_monitor_data_to_ns(err_msg);
            last_records_sent_time = time(NULL);
          }
        }
      }
      //TODO Handling if fd is not added in epoll or connection not established with NDC
      event_count = epoll_wait(v_epoll_fd, pfds, 2, 1000);
    
      if(event_count > 0)
      { 
        for(pfds_idx = 0; pfds_idx < event_count; pfds_idx++)
        {
          if(mrcptr && mrcptr->mon_reg_fd == pfds[pfds_idx].data.fd)
          {
              //MON : EPOLLERR/HUP - destory mrcptr, close connection and  call init_dbu_monitor
              //      EPOLLIN - not handled bcz ns does not send data
            if(pfds[pfds_idx].events & EPOLLOUT)
            {
              if(mrcptr->flags & NSLB_MON_REG_CONNECTING)
              {
                NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "Main Thread", NSLB_TL_INFO, "init_dbu_monitor(0) called"
                                                                        "from epoll");
                init_dbu_monitor();
              }
              else if(mrcptr->flags & NSLB_MON_REG_WRITING)
              {
                NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "Main Thread", NSLB_TL_INFO, "Partial data from epoll");
                //Sending NULL buffer to sending partail data
                int ret = nslb_mon_reg_send(mrcptr, NULL, 0, err_msg);
         
                //Incase of errro in writing 
                if(ret < 0)
                {
                  NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "MAIN_THREAD", NSLB_TL_ERROR, "Error : %s", err_msg);
                  remove_fd_and_close_monitor_connection();
                  destroy_monitor_structure(err_msg);
                }
                else if(ret == NSLB_MON_REG_DATA_SENT)
                {
                  NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "MAIN_THREAD", NSLB_TL_INFO,
                                                                  "Data sent completely.");
                  //nslb_mon_reg_remove_select(v_epoll_fd, mrcptr->mon_reg_fd);//data sent completely and goes in while loop and comes in case of
                  nslb_mon_reg_mod_select(v_epoll_fd, mrcptr->mon_reg_fd, EPOLLIN|EPOLLHUP|EPOLLERR);
                }
              }
            }
            else if(pfds[pfds_idx].events & EPOLLERR || pfds[pfds_idx].events & EPOLLHUP || pfds[pfds_idx].events & EPOLLIN)
            {
              if(pfds[pfds_idx].events & EPOLLIN)
              {
                bytes_read = nslb_mon_reg_read_line(mrcptr->mon_reg_fd, read_buf, &partial_buf, &partial_buf_len);
                
                NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "Main Thread", NSLB_TL_INFO, 
                                                  "Received Msg from NS = [%s] So closing connection and try again and bytes_read :- %d "
                , read_buf, bytes_read);
              }
              //add proper trace log 
              remove_fd_and_close_monitor_connection();
              destroy_monitor_structure(err_msg);
            }
          }
          /*Bug#91417_RUNTIME_MONITORS_APPLIED*/
          else if(pfds[pfds_idx].data.fd == ns_db_upload.inotify_info_ptr->inotify_fd)
          {
            pthread_sigmask(SIG_BLOCK, &set, NULL);
              NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "Main Thread", NSLB_TL_INFO,
                              "GOT EVENT FOR inotify_fd[%d]", ns_db_upload.inotify_info_ptr->inotify_fd);
              inotify_event_handling(ns_db_upload.inotify_info_ptr, &ns_db_upload);
            pthread_sigmask(SIG_UNBLOCK, &set, NULL);
           
            if(createGenViewsAndMetaTables)
              createViewsAndMetaDataTablesForGenericMonitors();
          }
        }
      }
    }
/*
    if (sigalarm)  {
      do_sigalarm();
      sigalarm = 0;
      alarm(1);
    }
*/
    do_sigalarm();

    if (test_post_proc_sig) {
      sighandler_t prev_handler;
      prev_handler = signal(TEST_POST_PROC_SIG, SIG_IGN);
      NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "Main Thread", NSLB_TL_INFO, "Received TEST_POST_PROC_SIG. Switching to offline mode.");
      ns_db_upload.running_mode = 0;
      ns_db_upload.is_test_running = 0;
      (void) signal(TEST_POST_PROC_SIG, prev_handler);
     
      //Send stop signal to nsi_db_analyze shell
      /*if(db_analyze_pid > 0)
        kill(db_analyze_pid, TEST_POST_PROC_SIG);
      */

      test_post_proc_sig = 0;
    }
                     
    if (sigterm) {
      sighandler_t prv_handler;
      prv_handler = signal(SIGTERM, SIG_IGN);
      NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "Main Thread", NSLB_TL_INFO, "Received SIGTERM.");
      //is_test_running = 0;
      ns_db_upload.kill_upload_thread = 1;
      (void) signal(SIGTERM, prv_handler);

      //Send stop signal to nsi_db_analyze shell
      /*if(db_analyze_pid > 0)
        kill(db_analyze_pid, SIGTERM);
      */

      sigterm = 0;
    }

    if(rt_trace) {
      sighandler_t prv_handler;
      prv_handler = signal(TRACE_LEVEL_CHANGE_SIG, SIG_IGN);
      NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "Main Thread", NSLB_TL_INFO, "TRACE_LEVEL_CHANGE_SIG signal"
                "received, changing trace level.");
      nslb_change_atb_mt_trace_log(ns_db_upload.trace_log_key, NSDBU_TRACE_KW);
      //now check if trace level is 3 or more then enable pg_bulkload_debug_flag.
      if(ns_db_upload.trace_log_key->log_level & 0x00FF0000)
        ns_db_upload.pg_bulkload_debug_flag = 1;

      (void) signal(TRACE_LEVEL_CHANGE_SIG, prv_handler);
      rt_trace = 0;
    }
   
//    #ifdef PG_BULKLOAD
  if(g_db_upload_command == PG_BULKLOAD_TOOL)
  {
    if((tmp_file_cleanup_pid < 0) && (nslb_recover_component_or_not(&tmp_csv_cleanup_recovery) == 0))
    {
      NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "Main Thread", NSLB_TL_INFO, "Going to recover tmp csv cleanup shell.");
      fork_pg_bulkload_tmp_file_cleanup(&ns_db_upload, tmp_path);
    }
  }
 //   #endif
                      
  /*if((db_analyze_pid < 0) && (nslb_recover_component_or_not(&db_analyze_recovery) == 0))
    {
      NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "Main Thread", 
                                            NSLB_TL_INFO, "Going to recover nsi_db_analyze shell");
      fork_db_analyze();
    }
  */

    if(!(ns_db_upload.is_test_running) || ns_db_upload.kill_upload_thread)
      break;

    //sigemptyset(&sigset);
    //sigsuspend(&sigset);
     if(ns_db_upload.dbu_monitor_on_off == 0)
      sleep(1);
  }

  /*Bug#91417_RUNTIME_MONITORS_APPLIED*/
  NSLB_TRACE_LOG1(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "Main Thread", NSLB_TL_INFO, 
                  "THREAD EXITING");
  pthread_exit(NULL);

  /*for(t = 0; t < ns_db_upload.numThreads; t++)
  {
    if(!ns_db_upload.totalStaticCsvEntry && !t)
      continue;

    ret = pthread_join(thread[t], NULL);
    if(ret)
    {
      NSLB_TRACE_LOG2(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "Main Thread", NSLB_TL_ERROR, "Error code from pthread join for thread id[%d] = \'%d\'", t, ret);
      nslb_db_upload_exit(-1, "Error code from pthread join", &ns_db_upload);
    }
    NSLB_TRACE_LOG2(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "Main Thread", NSLB_TL_INFO, "Thread[%d] successfully joined", t);
  }

  if(thread)
  {
    free(thread); 
  }*/

  /*if(db_analyze_pid > 0)
    kill(db_analyze_pid, SIGTERM);
  */

  /*
  end_time = time(NULL);
  if(ns_db_upload.trace_level)
  {
    NSLB_TRACE_LOG2(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "Main Thread", NSLB_TL_INFO, "Database uploading finished at %s", nslb_get_cur_date_time(cur_time_buf, 0));
    NSLB_TRACE_LOG2(ns_db_upload.trace_log_key, ns_db_upload.partition_idx, "Main Thread", NSLB_TL_INFO, "Total time taken = %ld(sec)", (end_time - start_time));
  }
  
  nslb_db_upload_exit(0, "CSV Data successfully uploaded to tables", &ns_db_upload);
  return 0;*/
}
#endif
