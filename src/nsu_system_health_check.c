#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <malloc.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <math.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <libgen.h>
#include "nslb_multi_thread_trace_log.h"
#include "nslb_trace_log.h"
#include "nslb_util.h"
#include "nslb_log.h"
#include "libpq-fe.h"
#include <sys/time.h>
#include <pwd.h>
#include <grp.h>
#include <ctype.h>
#include "nslb_partition.h"
#include <dirent.h>
#include <stdbool.h>

#define MAX_READ_LINE_LENGTH 128 * 1024
#define MAX_MAIL_BUFFER_SIZE 16*1024
#define MAX_SIZE 1024
#define NUM_THREADS 21
#define TL_INFO "info"
#define TL_ERROR "error"
#define MAX_DATA_LENGTH 128

#define DELTA_FOUND_CORES_NUM 16

#define N_ROWS 8

  //gcc -Wall -o program new_prashant.c ../libnscore/nslb_multi_thread_trace_log.c ../libnscore/nslb_util.c -lm -pthread
 
struct MTTraceLogKey *trace_log_key;
int total_controller = 0;
char *controller_name[MAX_SIZE];
char ns_wdir[512];
char sub_of_mail[MAX_SIZE];
char bt_path[MAX_SIZE];
char csv_path[MAX_SIZE];
char *mail_recipient_list = NULL;
char *mail_cc_list = NULL;
char msg[MAX_READ_LINE_LENGTH]={0};
int start_thread[NUM_THREADS] = {1,-1,1,1,-1,-1,-1,-1,1,1,-1,-1,1,1,1,1,1,1,1,-1,-1};
pthread_t tid[NUM_THREADS];
char my_controller[50];

 //declaring index of start thread array 
enum flag{
  CORE_FLAG = 0,
  TMPFS_FLAG = 1,
  REL_FLAG = 2,
  MANDATORY_PROCESS_FLAG = 3,
  UNWANTED_PROCESS_FLAG = 4,
  CHECK_UNEXPETED_FILE_IN_DIR_FLAG = 5,
  CHECK_NDP_DBU_PROGRESS_FLAG = 6,
  CONSTRAINT_FLAG = 7,
  TEST_RUN_STATUS_FLAG = 8,
  RTG_PACKET_SIZE_FLAG = 9,
  METADATA_SIZE_CHECK_FLAG = 10,
  DB_DUP_FILE_CHECK = 11,
  READ_CSV_FILE = 12,
  CHECK_ULIMIT_FLAG = 13,
  CHECK_SHARED_MEM_BUFF =14,
  CHECK_DISK_FREE = 15,
  CHECK_HEAP_DUMP_FLAG = 16,
  POSTGRES_FLAG = 17,
  MAX_FILE_SIZE_FLAG = 18,
  CLEANUP_STATUS_FLAG = 19,
  FILE_LMD_FLAG = 20
};

//default enable health monitoir
int enable_health_monitor = 1;

//default value for keyword enabling tmp directory
int enable_tmp_dir_scanning = 1;

//default enable system level health monitor	
int enable_system_level_health = 1;

//default status of test running
int is_test_running = 0;

//flag for test run no.
int test_run_no_flag=0;

//default size for cshm_log_file_size
int cshm_log_file_size = 100;

//default delay time and size

long long int core_delay_time = 300;
long long int filesystem_delay_time = 4 * 60 * 60; // 4 hours
long long int postgresql_delay_time = 1 * 60 * 60; //1 hour
long long int rel_dir_delay_time = 24 * 60 * 60; // 24 hours
long long int mandatory_process_delay_time = 300; // 5 min
long long int unwanted_process_delay_time = 300;  
//long long int average_load_delay_time = 60 * 60; //1 hour
long long int monitor_path_delay_time = 300; 
long long int missing_constraint_delay_time = 60 * 60; // 1 hour
long long int max_file_size_delay_time = 300;
long long int unexpected_files_in_dir_delay_time = 24 * 60 * 60; //24 Hours
long long int ndp_dbu_progress_delay_time = 300;
long long int test_run_delay_time = 300;
long long int rtg_delay_time = 8 * 60 * 60; //8 hours
long long int sleep_time = 10;
int dump_bt_flag = 0;
bool send_mail = false;
bool send_mail_cc = false;
bool core_dir_exist = false;
int size_to_realloc = 0;
int total_core_binary = 0;
int total_rel_path = 0;
int total_unexp_files_path = 0;
int total_mandatory_process_to_monitor = 0;
int total_unwanted_process_to_monitor = 0;
int health_monitor_trace_level = 1;
int new_core = 0;
int process_alert_frequency = 0;
//double average_load_threshold = 10;
int no_of_files_threshold = 1; //default 1 file
long long int query_exec_threshold_value = 2; //value in seconds
int num_connection_threshold_value = 80;
//int testidx;
//int test_no;
long long int bytes_remaining_threshold = 1 * 1024 * 1024 * 1024; // 1 GB
//char test_path[512]; // test path where test should run
int test_run_no;
//char rtg_test_path[512]; // test path where test should run
//int rtg_test;
long long int thres_packet_size = 5 * 1024 * 1024; //5MB
int gdf_threshold_count = 120; //120 files
FILE *bt_fp = NULL;
static pthread_mutex_t thread_mutex = PTHREAD_MUTEX_INITIALIZER;
//int len; 
int header_msg_length;
int *fs_threshold;
int *fs_threshold_for_mail;
int *filesystem_mounted_flag;
char **fs_mounted_name;
char **file_system_name;
int process_flag  = 0;
int no_of_fs_keyword = 0;
char conf_file_path[512];
int count_of_path_to_monitor = 0;
//char seq_no=65;
char machine_name[1024] = {0};
char host_name[1024*1024];
char machine_mode[1024] = {0};
long long int max_size_of_file_in_tmp = 1 * 1024 * 1024;
long long int threshold_timestamp_diff;

char ndc_path[MAX_SIZE];
char hpd_path[MAX_SIZE];
char lps_path[MAX_SIZE];
char tomcat_path[MAX_SIZE];
char postgresql_path[MAX_SIZE];
char cmon_path[MAX_SIZE];
char ndp_path[MAX_SIZE];
char nsu_db_upload_path[MAX_SIZE];
char ndu_db_upload_path[MAX_SIZE];
char log_rd_path[MAX_SIZE];
char log_wr_path[MAX_SIZE];
char event_log_path[MAX_SIZE];
char netstorm_path[MAX_SIZE];

char ndc_pid_path[MAX_SIZE];
char hpd_pid_path[MAX_SIZE];
char lps_pid_path[MAX_SIZE];
char tomcat_pid_path[MAX_SIZE];
char postgresql_pid_path[MAX_SIZE];
char cmon_pid_path[MAX_SIZE];
char ndp_pid_path[MAX_SIZE];
char nsu_db_upload_pid_path[MAX_SIZE];
char ndu_db_upload_pid_path[MAX_SIZE];
char log_rd_pid_path[MAX_SIZE];
char log_wr_pid_path[MAX_SIZE];
char event_log_pid_path[MAX_SIZE];


int total_no_of_rows = 0;
int max_heap_dump = 1;
int max_days_heap_dump = 15;
long long int max_file_size = 1073741824; // 1 GB    
long long int max_sleep_time = 24*60*60;     // 1 day
char meta_max_cont_path[512];                    
long long int metadata_max_mail_time = 24*60*60;

char db_dup_cont_path[512];
long long int db_dup_sleep_time = 5*60*60; // 5 hour
long long int db_dup_mail_time = 5*60*60;
long long int dis_cshm_mail_time = 4*60*60; // 4 hour
int system_level_monitor_sleep_time = 5*60; // 5 min
int idle_cpu_threshold = 20;
int load_avg_over_5min_threshold = 40;
int cache_memory_threshold = 20;
int disk_free_space_threshold = 80;
int tmp_threshold = 80;
int cshm_thread_delay = 5*60;
int builds_threshold = 5;
int threshold_size_of_core_dir = 200*1024;  // 200 GB
int threshold_timestamp_diff_days = 15;  // 15 days
long int heap_dump_sleep_time = 4*60*60; // 4 hour

//default values for IOTOP_CAPTURE_SETTING

bool show_iotop_output = true;
bool show_iotop_thread_output = false;
int iotop_log_interval = 3;
 
//structures declared 

typedef struct {
  char    core_path[MAX_SIZE];
  char    binary_name[MAX_SIZE];
  char    bin_base_name[MAX_SIZE];
  int     index;
  int     core_counter;
  int     max_core_size;
  int     max_cores_allowed;
}Core_Info;
Core_Info *core_info = NULL;

typedef struct coreNames 
{
  char     name[MAX_SIZE];
  int      bin_idx;
}coreNames;

typedef struct {
  coreNames   *core_names;
  int         core_names_num;
  long long   last_reporting_time;
}foundCoreList;

foundCoreList found_core_list;

//This structure is used to hold information of files which are changed during runtime
typedef struct
{
  char file_name[MAX_SIZE];
  long long file_size;
  long long lmd;
}file_changes_count;
file_changes_count *file_change_struct = NULL;

int file_count = 0;


typedef struct {
  char path_name[MAX_SIZE];
  char owner_name[MAX_SIZE];
  char group_name[MAX_SIZE];
  int permission;
  char permission_flag;
  char owner_flag;
  char group_flag;
}Monitor_path;
Monitor_path *monitor_path = NULL;


typedef struct
{
  char process_name[MAX_SIZE];
  char only_mandatory_process_name[MAX_SIZE];
  char process_pid_path[MAX_SIZE];
}Mandatory_Process;
Mandatory_Process *mandatory_process = NULL;

typedef struct
{
  char process_name[MAX_SIZE];
  char only_unwanted_process_name[MAX_SIZE];
}Unwanted_Process;
Unwanted_Process *unwanted_process = NULL;

typedef struct
{
  long long int  inode;
  char file_name[256];
}largeFileInfo;

typedef struct
{
  char dir_path[MAX_SIZE];
  long long int dir_size_threshold;
  int num_files_threshold;
  int include_sub_dir;
}Dir_Info;
Dir_Info *dir_info = NULL;

typedef struct
{
  char file_name[256];
  int file_flag;
}FileInfo;

static int find_binary_in_coreinfo(char *binary_name, int index, int end_index);
void send_mail_to_user(char *sub_of_mail, char *msg, char *core_file_path);

#define MONITOR_TRACE_LOG1(thread_name, severity, ...) \
{ \
  if((health_monitor_trace_level & 0x000000FF)) \
  nslb_mt_trace_log(trace_log_key, 0, thread_name, severity, __FILE__, __LINE__, (char*)__FUNCTION__, __VA_ARGS__); \
}                                

#define MONITOR_TRACE_LOG2(thread_name, severity, ...) \
{ \
  if((health_monitor_trace_level & 0x0000FF00)) \
  nslb_mt_trace_log(trace_log_key, 0, thread_name, severity, __FILE__, __LINE__, (char*)__FUNCTION__, __VA_ARGS__); \
}

#define MONITOR_TRACE_LOG3(thread_name, severity, ...) \
{ \
  if((health_monitor_trace_level & 0x00FF0000)) \
  nslb_mt_trace_log(trace_log_key, 0, thread_name, severity, __FILE__, __LINE__, (char*)__FUNCTION__, __VA_ARGS__); \
}

#define MONITOR_TRACE_LOG4(thread_name, severity, ...) \
{ \
  if((health_monitor_trace_level & 0xFF000000)) \
  nslb_mt_trace_log(trace_log_key, 0, thread_name, severity, __FILE__, __LINE__, (char*)__FUNCTION__, __VA_ARGS__); \
}


//function to execute a command
int run_cmd(FILE **pipe_fp, char *cmd)
{
  *pipe_fp = popen(cmd, "r");

  if(*pipe_fp == NULL)
  {  
    fprintf(stderr, "Error in executing command = '%s' (Error: '%s')", cmd, nslb_strerror(errno));
    return -1;
  }
  return 0;
}

#define MY_REALLOC(buf, size, msg, index)  \
{ \
  if (size <= 0) {  \
    fprintf(stderr, "Trying to realloc a negative or 0 size (%d) for index  %d\n", (int )(size), index); \
  } else {  \
    buf = (void*)realloc(buf, size); \
    if ( buf == (void*) 0 )  \
    {  \
      fprintf(stderr, "Out of Memory (size = %d): %s for index %d\n", (int )(size), msg, (int )index); \
    }  \
  } \
}

#define MY_REALLOC_AND_MEMSET(buf, size, old_size, msg, index)  \
{ \
  if (size <= 0) {  \
    fprintf(stderr, "Trying to realloc a negative or 0 size (%d) for index  %d\n", (int )(size), index); \
  } else {  \
    buf = (void*)realloc(buf, size); \
    if ( buf == (void*) 0 )  \
    {  \
      fprintf(stderr, "Out of Memory (size = %d): %s for index %d\n", (int )(size), msg, (int )index); \
    }  \
    memset(((char *)buf) + old_size, 0, size - old_size);                                           \
  } \
}

//Sort stucture on the base on core path
int sort_core_path(const void *core_path1, const void *core_path2)
{
  Core_Info *a = (Core_Info *)core_path1;
  Core_Info *b = (Core_Info *)core_path2;
  return((strcmp(a->core_path, b->core_path)));
}


//Save Matching index
void match_binary_path()
{
  int i, j;
  for(i= 0; i < total_core_binary; i++)
  {
    core_info[i].index++;
    for(j = i+1; j < total_core_binary; j++)
    {
      if(!strcmp(core_info[i].core_path, core_info[j].core_path))
      {
	core_info[i].index++;
      }
      else
      {
	break;
      }
    }
  }
}


int move_core(char *core_name)
{
  int ret=0;
  char buf[MAX_SIZE];
  char dir[50]="/home/cavisson/fail_to_delete";

  if(!core_dir_exist)
  {
    mkdir(dir, 0777);
    core_dir_exist = true;
    MONITOR_TRACE_LOG3("move_core", TL_INFO, "Directory '%s' created for moving the cores which are unable to delete\n", dir);
  }

  sprintf(buf, "mv /home/cavisson/core_files/%s %s/%s", core_name, dir, core_name);

  ret = system(buf);

  if(ret == -1)
  {
    MONITOR_TRACE_LOG1("move_core", TL_INFO, "Unable to move the core '%s' to the directory '%s' \n", core_name, dir);
    return -1;
  }
  else
  {
    pthread_mutex_lock(&thread_mutex);

    MONITOR_TRACE_LOG2("move_core", TL_INFO, "Core '%s' moved to the directory which was unable to delete\n",core_name);
    sprintf(sub_of_mail,"Alert | %s | CSHM | %s | %s | Core Moved", machine_mode,machine_name,my_controller);
    snprintf(msg, MAX_READ_LINE_LENGTH, "Attention: This core was not getting deleted so it is moved to the directory '%s'.\n\n Core   :   %s", dir, core_name);
    MONITOR_TRACE_LOG2("Msg send for move_core", TL_INFO, " %s \n", msg);
    send_mail_to_user(sub_of_mail, msg, NULL);
    
    pthread_mutex_unlock(&thread_mutex);
  }

  return ret;
}

//Remove cores
void remove_core(char *core_file, char *binary_name, int core_name_idx)
{
  if(!(unlink(core_file)))
  {
    MONITOR_TRACE_LOG2("find_core_and_remove_core", TL_INFO, "Removed core file '%s' whose binary is '%s'\n", core_file, binary_name);
  }
  else
  {
    MONITOR_TRACE_LOG1("find_core_and_remove_core", TL_ERROR, "Unable to remove core file '%s' whose binary is '%s'\n", core_file, binary_name);
  }

  found_core_list.core_names[core_name_idx].name[0] = '\0';
  found_core_list.core_names[core_name_idx].bin_idx = -1;
}

int dump_data_in_file(int fd, char *msg, int len)
{   
  // Here the length of the message is stored in a variable write_bytes_remaining.
  int write_bytes_remaining = len;
  int write_offset = 0;
  int bytes_sent = 0;
    
  if(fd == -1)
    return -1;

  // Now the loop will be executed here and will continue till all the bytes are send.
  while(write_bytes_remaining)
  {
    if ((bytes_sent = write(fd, msg + write_offset, write_bytes_remaining)) < 0)//writing the message
    {
      if(errno == EINTR)
        continue;
      MONITOR_TRACE_LOG1("dump BT cores in file", TL_ERROR, "Error: Failed to dump data in core file. Error = '%s'\n", strerror(errno));
      return -1;
    }
    else
    {
      write_offset += bytes_sent;
      write_bytes_remaining -= bytes_sent;
    }
  }
  return len;
}

//Save backtrace in Backtrace.log and in mail buffer
void save_backtrace(char *core_file, char *binary_name)
{
  FILE *fp = NULL;
  char command[1024], bt_buf[1024], info[1024], mail_buf[16*1024] = "", core_buf[1024] = "";
  int mail_buf_size = 16 * 1024;
  int ret, mail_buf_length = 0, length = 0;
  mail_buf[0]='\0';

  sprintf(command, "gdb --batch --quiet -ex \"bt\" %s %s 2>/dev/null |tail -n+5", binary_name, core_file);
  ret = run_cmd(&fp, command);

  if(ret == -1)
  {
    MONITOR_TRACE_LOG1("find_core_and_remove_core", TL_ERROR, "Unable to get back trace of core file '%s' "
	"whose binary name is '%s' \n", core_file, binary_name);
    return;
  } 

  MONITOR_TRACE_LOG2("find_core_and_remove_core", TL_INFO, "New core found '%s' whose binary name is '%s'\n", core_file, binary_name);

  new_core++;

  //save_backtrace in Backtrace.log
  if(dump_bt_flag && new_core == 1)
  {
    time_t t;
    time(&t);
    fprintf(bt_fp, "%s---->\n", ctime(&t));
    fflush(bt_fp);
  }

  if(dump_bt_flag) 
  {
    sprintf(info, "%d. This back trace is found from core file '%s' and its binary name is '%s' \n", new_core, core_file, binary_name);

    fwrite(info, 1, strlen(info), bt_fp);
    fflush(bt_fp);   
  }

  if(dump_bt_flag && send_mail)
  { 
    while((fgets(bt_buf, 1024, fp)) != NULL)
    { 
      fwrite(bt_buf, 1, strlen(bt_buf), bt_fp);  

      if(mail_buf_length < mail_buf_size) 
	mail_buf_length += snprintf(mail_buf + mail_buf_length, mail_buf_size - mail_buf_length, "%s", bt_buf); 

      fflush(bt_fp); 
    }
  }
  else if(dump_bt_flag)
  { 
    while((fgets(bt_buf, 1024, fp)) != NULL)
    { 
      fwrite(bt_buf, 1, strlen(bt_buf), bt_fp);   
      fflush(bt_fp); 
    }
  }
  else
  { 
    while((fgets(bt_buf, 1024, fp)) != NULL)
    { 
      if(mail_buf_length < mail_buf_size) 
	mail_buf_length += snprintf(mail_buf + mail_buf_length, mail_buf_size - mail_buf_length, "%s", bt_buf); 
    }
  }
  //close previous fp
  if(fp)
    pclose(fp);


  //Write mail message in buffer
  if(mail_buf[0] != '\0' && mail_buf[0] != '\n')
  {
    /* Run command to list information of core file*/ 
    sprintf(command, "ls -l %s", core_file);
    ret = run_cmd(&fp, command);

    if(ret == -1)
    {
      MONITOR_TRACE_LOG1("find_core_and_remove_core", TL_ERROR, "Unable to list information of core file '%s' \n", core_file);
      return;
    }

    //Get output of command in core_buf
    fgets(core_buf, 1024, fp);

    //close fp 
    if(fp)
      pclose(fp);
     
    if(send_mail)
    {      
      int  core_fd = -1;
      char temp_core_file[1024] = "" ;
      char buffer[26];
      char core_msg[MAX_READ_LINE_LENGTH];
      time_t mail_dt;
      struct tm* tm_info, tm_struct;

      time(&mail_dt);
      tm_info = nslb_localtime(&mail_dt, &tm_struct, 1);
      strftime(buffer, 26, "%Y/%m/%d %H:%M:%S %Z", tm_info);

      snprintf(core_msg, MAX_READ_LINE_LENGTH, "Attention: Segmentation fault detected for binaries in the monitoring list. The information regarding the gdb backtrace and saved core file paths is appended below. \n\n Binary    : %s \n  Core      : %s \n  InFo      : %s \n  Backtrace : \n%s\n\n", binary_name, core_file, core_buf, mail_buf);

      length = snprintf(msg , MAX_READ_LINE_LENGTH , "\n***This is a system generated email. Please do not respond to this email.****\n\n\nAlert Time: %s\n\n%s\nThis is for your information and take necessary action if required.\n\n\n\n\n\nFor any feedback or query on health monitoring tool, please send an email to client-support@cavisson.com with \"Cavisson System Health Monitor - Feedback\" in the subject line.\n+", buffer, core_msg);

      MONITOR_TRACE_LOG2("Msg send for find_core_and_remove_core", TL_INFO, " %s \n", msg);
      strcpy(temp_core_file, "/tmp/coreFile_XXXXXX");

      if((core_fd = mkstemp(temp_core_file)) < 1)
      {
        MONITOR_TRACE_LOG1("find_core_and_remove_core", TL_ERROR, "Unable to create of core file in /tmp directory \n");
        return ;
      }
      
      //dump core BT in file referred by core_fd
      if(dump_data_in_file(core_fd, msg, length) == -1)
      {
        MONITOR_TRACE_LOG1("find_core_and_remove_core", TL_ERROR, "Unable to write BT of core file in /tmp/%s file \n", temp_core_file);
        unlink(temp_core_file);
        return ;
      }

      sprintf(sub_of_mail,"Alert | %s | CSHM | %s | %s | Segmentation Fault Detected", machine_mode, machine_name, my_controller);

      send_mail_to_user(sub_of_mail, msg, temp_core_file);
      close(core_fd);
      unlink(temp_core_file);
    }
    else
    {
      MONITOR_TRACE_LOG2("find_core_and_remove_core", TL_INFO, "Unable to send mail as the mail list is empty \n");
      MONITOR_TRACE_LOG2("find_core_and_remove_core", TL_INFO, "Binary  : %s \nCore  : %s \nInFo  : %s \nBacktrace : \n%s\n\n", binary_name, core_file, core_buf, mail_buf);
    }
  }
  else
  {
    MONITOR_TRACE_LOG2("find_core_and_remove_core", TL_INFO, "Unable to send mail as the backtrace is empty. \n Binary : %s \n  Core : %s \n  InFo : %s \n  Backtrace : \n%s\n\n", binary_name, core_file, core_buf, mail_buf);
  }
}

static void get_binary_name_of_core(char *core_file, char *binary_name)
{
  FILE *fp = NULL;
  char *newline_ptr = NULL;
  char command[MAX_SIZE] = "";
  char bin_ctrl_name[100]="";
  char buff[100]="";
  int ret;

  sprintf(command, "echo %s | cut -d '!' -f4", core_file);

  ret = run_cmd(&fp, command);

  if(ret == -1)
  {
    MONITOR_TRACE_LOG1("find_core_and_remove_core", TL_ERROR, "Unable to get contoller name in '%s'"
        "                       core file through command\n", core_file);
    return;
  }
  
  fgets(buff, 100, fp);
   
  if(fp)
    pclose(fp);

  sscanf(buff,"%s",bin_ctrl_name);
  
  if(strcmp(my_controller, bin_ctrl_name) != 0)
  {
    MONITOR_TRACE_LOG3("find_core_and_remove_core", TL_ERROR,"Contorller name not mached for core : '%s' having controller name : '%s'\n", core_file, bin_ctrl_name); 
    return;
  }
 
  sprintf(command, "file '%s' | cut -d \"'\" -f2 | cut -d \" \" -f1", core_file);

  ret = run_cmd(&fp, command);

  if(ret == -1)
  {
    MONITOR_TRACE_LOG1("find_core_and_remove_core", TL_ERROR, "Unable to get binary name of '%s'" 
	"                       core file through file command\n", core_file);
    return;
  }

  fgets(binary_name, MAX_SIZE, fp);

  if(fp)
    pclose(fp);


  if((newline_ptr = strchr(binary_name, '\n')))
    *newline_ptr = '\0';
}

int find_core_in_list_and_check_binary_name_of_core(char *core_path_with_name, char *binary_name, char *core_name, int index, int end_index)
{
  int i, bin_index;

  for(i = 0; i < found_core_list.core_names_num; i++)
  {
    if(strncmp(found_core_list.core_names[i].name, core_name, strlen(core_name)) == 0)
      return found_core_list.core_names[i].bin_idx;
  }

  get_binary_name_of_core(core_path_with_name, binary_name);  
  bin_index = find_binary_in_coreinfo(binary_name, index, end_index);

  return bin_index;
}

int add_core_to_list(char *core_name, int bin_index)
{
  int i;

  /* Get free slot to save new core */
  for(i = 0; i < found_core_list.core_names_num; i++)
  {
    if(found_core_list.core_names[i].name[0] == '\0')
      break;
  }

  /* Realloc list if no slot is available */
  if(i == found_core_list.core_names_num)
  {
    found_core_list.core_names = realloc(found_core_list.core_names, 
	sizeof(coreNames) * (found_core_list.core_names_num + DELTA_FOUND_CORES_NUM));
    memset(found_core_list.core_names + found_core_list.core_names_num, 0, sizeof(coreNames) * DELTA_FOUND_CORES_NUM);
    found_core_list.core_names_num += DELTA_FOUND_CORES_NUM;
  }

  // Save core name and corresponding binary index
  snprintf(found_core_list.core_names[i].name, strlen(core_name), "%s", core_name);
  found_core_list.core_names[i].bin_idx = bin_index;
  return i;
}

static int find_binary_in_coreinfo(char *binary_name, int index, int end_index)
{
  int i;
  char tmp_binary_name[1024] = "";
  struct stat fileStat;
  struct stat fileSt;
  char *not_op_found = NULL;

  //Loop for all binary files configured for this path.
  //For example for '/home/cavisson/work' 'nsu_db_upload' and 'netstorm' binaries are configured.
  for(i = index; i < end_index; i++)
  {
    /* Using 'file' command, we can get binary name in following 3 forms - 
       1. Absolute path - /home/cavisson/bin/nsu_db_upload
       2. Relative path - ../bin/nsu_db_upload
       3. Only bin name - nsu_db_upload

       We have to match this binary name with binary names provided by user.
       In case 1 and 2, we are mathcing inodes. In case 3 we are just matching names. */

    //In case file command is not able to give binary name ang give core info like: core.!home!cavisson!work!bin!netstorm.12020:
    if((not_op_found = strchr(binary_name, '!')))
    {
      char *dot_found = NULL;
      char *ptr = NULL;

      dot_found = strchr(not_op_found, '.');

      if(dot_found)
	*dot_found = '\0';

      while((ptr = strchr(not_op_found, '!')))
      {
	*ptr = '/';
	not_op_found++;
      }

      if((not_op_found = strchr(binary_name, '.')))
      {
	not_op_found++;
	sprintf(binary_name, "%s", not_op_found);
      }

      if(stat(binary_name, &fileStat) < 0)
	continue;

      if(stat(core_info[i].binary_name, &fileSt) < 0)
	continue;

      //Binaries matched
      if(fileStat.st_ino == fileSt.st_ino)
	break;
    } 
    //In case of absolute or relative path
    else if(strchr(binary_name, '/'))
    {
      //Absolute path
      if(binary_name[0] == '/')
	strcpy(tmp_binary_name, binary_name);
      //Relative path
      else
	sprintf(tmp_binary_name, "%s/%s" , ns_wdir, binary_name);

      if(stat(tmp_binary_name, &fileStat) < 0)    
	continue;

      if(stat(core_info[i].binary_name, &fileSt) < 0)    
	continue;

      //Binaries matched
      if(fileStat.st_ino == fileSt.st_ino)
      {
        strcpy(binary_name, core_info[i].binary_name); 
	break;
      }

    }
    //In case if only binary name is available
    else
    {
      char *ptr = NULL;
      ptr = basename(binary_name);
      if(strcmp(core_info[i].bin_base_name, ptr) == 0)
	break;
    }
  } 

  if(i < end_index)
  {
    return i;
  }
  else
  {
    return -1;
  }
}

void report_core_dump(char *core_file, char *binary_name)
{
  pthread_mutex_lock(&thread_mutex);
    save_backtrace(core_file, binary_name);
  pthread_mutex_unlock(&thread_mutex);
}

void process_core_file(char *core_path, char *core_name, long long timestamp, int index, int end_index, long int core_size)
{
  char binary_name[1024];
  int core_name_idx, bin_index;
  char core_path_with_name[1024] = "";
  long long int timestamp_diff=0;

  snprintf(core_path_with_name, 1024, "%s/%s", core_path, core_name);
  if(core_path_with_name[0] == '\0')
    return;

  //New core found which was not reported earlier
  if(timestamp > found_core_list.last_reporting_time)
  {
    //function to get binary name of that particular core file
    get_binary_name_of_core(core_path_with_name, binary_name);

    //this function will return binary index for that binary name
    bin_index = find_binary_in_coreinfo(binary_name, index, end_index);

    // if bin_index is less than 0 means binary name is not matched
    if(bin_index < 0)
    {
      MONITOR_TRACE_LOG2("find_core_and_remove_core", TL_INFO, "New core found '%s' but not reporting it as this core is not configured\n", core_path_with_name);
      return;
    }
   
    //add that core file in core_names structure, this function will return core_name index  
    core_name_idx = add_core_to_list(core_name, bin_index);

    //this function will save backtrace of core name in backtrace file and also report core file in mail
    report_core_dump(core_path_with_name, binary_name);
  }
  //core has alredy been reported and its time stamp is less than monitor timestamp
  else
  {
    /* This function will check core name in core_names structure, 
       if core name found in structure then it will return binary index means index where binary name of that core exists, 
       if core name is not found in structure it will get binary name of that strutcure and  it will return binary index of that core name */
    bin_index = find_core_in_list_and_check_binary_name_of_core(core_path_with_name, binary_name, core_name, index, end_index);

    //if binary index is less than 0 means binary name not found then return from this function
    if(bin_index < 0)
      return;

    //Now that core is not added yet so we need to add core name in core_names structure
    core_name_idx = add_core_to_list(core_name, bin_index);
  }

  //Increment core counter every time when new core found
  core_info[bin_index].core_counter++;
  
  if(core_size > core_info[bin_index].max_core_size)
     core_info[bin_index].max_cores_allowed = 1;
  

  //If core counter is more than threshold value of maximum core limit in that case it will remove extra cores
  if(core_info[bin_index].core_counter > core_info[bin_index].max_cores_allowed || (timestamp_diff=found_core_list.last_reporting_time-timestamp) > threshold_timestamp_diff)
  {
    //it will remove core and also set null values in core_names structure
    remove_core(core_path_with_name, binary_name, core_name_idx); 
  }
}

long long int check_sleep_interval_in_min_sec_or_hr(char *value)
{
  long long int time = 0;
  char time_unit = '\0';

  sscanf(value, "%lld%c", &time, &time_unit);

  if(time_unit == '\0' || time <= 0)
    return -1;

  if(time_unit == 's' || time_unit == 'S')
  {
    return time;
  }

  else if(time_unit == 'm' || time_unit == 'M')
  {
    return (time * 60);
  }

  else if(time_unit == 'h' || time_unit == 'H')
  {
    return (time * 60 * 60);
  }
  else
    return -1;
}

long long int convert_units_in_bytes(char *value)
{
  long long int size = 0;
  char unit = '\0';

  sscanf(value, "%lld%c", &size, &unit);

  if(unit == '\0' || size <= 0)
    return -1;

  if(unit == 'b' || unit == 'B')
  {
    return(size);
  }

  if(unit == 'k' || unit == 'K')
  {
    return(size * 1024);
  }

  else if(unit == 'm' || unit == 'M')
  {
    return(size * 1024 * 1024);
  }

  else if(unit == 'g' || unit == 'G')
  {
    return(size * 1024 * 1024 * 1024);
  }
  else
    return -1;
}

//calculate time in seconds
long long int my_gettimeofday()
{
  struct timeval want_time;
  long long int timestamp;

  gettimeofday(&want_time, NULL);
  timestamp = want_time.tv_sec;
  return timestamp;
}

void* check_heap_dump_monitor()
{
  FILE *fp = NULL; 
  int ret = 0;
  int no_of_heap_dump = 0;
  char command[MAX_DATA_LENGTH], buff[1024];

  MONITOR_TRACE_LOG2("check_heap_dump_monitor", TL_INFO, "check_heap_dump_monitor thread started\n ");

  while(1)
  {
    sprintf(command, "ls -t /home/cavisson/HeapDump | wc -l");

    ret = run_cmd(&fp, command);

    if(ret == -1)
    {
      MONITOR_TRACE_LOG1("check_heap_dump_monitor", TL_ERROR, "Unable to run command : '%s'\n", command);
      sleep(300);
      continue;
    }

    fgets(buff, 3, fp);

    MONITOR_TRACE_LOG3("check_heap_dump_monitor", TL_INFO, "Command : '%s' output: %s\n", command, buff);
    
    no_of_heap_dump = atoi(buff);

    memset(&buff[0], 0, MAX_DATA_LENGTH);
    memset(&command[0], 0, MAX_DATA_LENGTH);

    if(fp)
      pclose(fp);    
   
    if(no_of_heap_dump > max_heap_dump)
    {
      MONITOR_TRACE_LOG2("check_heap_dump_monitor", TL_INFO, "No. of heap dump files crossed threshold : %d \n", no_of_heap_dump);

      sprintf(command,"ls -td /home/cavisson/HeapDump/* | tail -n +%d | xargs rm -- ; echo $?", max_heap_dump+1);

      ret = run_cmd(&fp, command);

      if(ret == -1)
      {
        MONITOR_TRACE_LOG1("check_heap_dump_monitor", TL_ERROR, "Unable to run command : '%s'\n", command);
        sleep(300);
        continue;
      }

      fgets(buff, 3, fp);
   
      MONITOR_TRACE_LOG2("check_heap_dump_monitor", TL_INFO, "command :'%s' output : c1: [%c] '\n", command, buff[0]);

      if(fp)
        pclose(fp);
    
      if(buff[0] == '0')
      {
        pthread_mutex_lock(&thread_mutex);
          sprintf(sub_of_mail,"INFO | %s | CSHM | %s | %s | Heap dump files deleted", machine_mode,machine_name,my_controller);
          sprintf(msg, "Number of heap dump files in /home/cavisson/HeapDump folder exceeded the threshold value. Deleted %d heap dump files\n", no_of_heap_dump-max_heap_dump);
          MONITOR_TRACE_LOG2("Msg send for check_heap_dump_monitor",TL_INFO, "%s", msg);
          send_mail_to_user(sub_of_mail, msg, NULL);
        pthread_mutex_unlock(&thread_mutex);

      }
    }
    else
    {
      MONITOR_TRACE_LOG2("check_heap_dump_monitor",TL_INFO, "Heap dump files are less than the threshold given\n", msg);
    }
    
    memset(&buff[0], 0, MAX_DATA_LENGTH);
    memset(&command[0], 0, MAX_DATA_LENGTH);
    no_of_heap_dump = 0;
    
    sprintf(command, "find /home/cavisson/HeapDump/ -mtime +%d -print | wc -l", max_days_heap_dump);

    ret = run_cmd(&fp, command);

    if(ret == -1)
    { 
      MONITOR_TRACE_LOG1("check_heap_dump_monitor", TL_ERROR, "Unable to run command : '%s'\n", command);
      sleep(300);
      continue;
    }

    fgets(buff, 3, fp);

    MONITOR_TRACE_LOG2("check_heap_dump_monitor", TL_INFO, "command :'%s' output : '%s'\n", command, buff);

    no_of_heap_dump = atoi(buff);

    memset(&buff[0], 0, MAX_DATA_LENGTH);
    memset(&command[0], 0, MAX_DATA_LENGTH);

    if(fp)
    pclose(fp);

    if(no_of_heap_dump > 0)
    {
      MONITOR_TRACE_LOG2("check_heap_dump_monitor", TL_INFO, "No. of heap dump files older than threshold : '%d'\n", no_of_heap_dump);    
 
      sprintf(command,"find /home/cavisson/HeapDump/ -mtime +%d -type f -print | xargs rm -- ; echo $?", max_days_heap_dump);
     
      ret = run_cmd(&fp, command);

      if(ret == -1)
      {
        MONITOR_TRACE_LOG1("check_heap_dump_monitor", TL_ERROR, "Unable to run command : '%s'\n", command);
        sleep(300);
        continue;
      }

      fgets(buff, 3, fp);
 
      MONITOR_TRACE_LOG2("check_heap_dump_monitor", TL_INFO, "command: '%s' output : c1: [%c]\n", command, buff[0]);
 
      if(fp)
        pclose(fp);

      if(buff[0] == '0')
      {
        pthread_mutex_lock(&thread_mutex);
          sprintf(sub_of_mail,"INFO | %s | CSHM | %s | %s | Heap dump files deleted", machine_mode,machine_name,my_controller);
          sprintf(msg, "Found heap dump files older than the threshold date in /home/cavisson/HeadDump folder. Deleted %d heap dump files\n", no_of_heap_dump);
          MONITOR_TRACE_LOG2("Msg send for ",TL_INFO, "%s", msg);
          send_mail_to_user(sub_of_mail, msg, NULL);
        pthread_mutex_unlock(&thread_mutex);

      }
      else
      {
        MONITOR_TRACE_LOG2("check_heap_dump_monitor",TL_INFO, "No heap dump files are older than threshold");
      }

      memset(&buff[0], 0, MAX_DATA_LENGTH);
      memset(&command[0], 0, MAX_DATA_LENGTH);
      no_of_heap_dump = 0;
    }
    sleep(heap_dump_sleep_time);
  }
  return NULL;
}

void* check_postgresql_monitor()
{
  /* database variables */
  const char *conninfo;
  PGconn *dbconnection;

  /* to store result */
  PGresult *res;

  long long int start_time, end_time, query_exec_time;
  int pg_mail_flag = 0;
  int num_connections;
  //char *ptr;
  int query_time_mail_send = 0;
  int connection_mail_send = 0;
  int postgres_port=0;
  char command[512], buff[64];
  int ret;
  FILE *fp = NULL;
  char pg_msg_buf[MAX_READ_LINE_LENGTH];
  int index = 0;
  int pg_len = 0;

  MONITOR_TRACE_LOG2("check_postgresql_monitor", TL_INFO, "check_postgresql_monitor thread started\n");  
    
  while(postgres_port == 0)
  {
    sprintf(command,"sudo netstat -plunt | grep postgres | grep LISTEN | cut -d ':' -f2 | cut -d ' ' -f1");
    ret = run_cmd(&fp, command);

    if(ret == -1)
    {
      MONITOR_TRACE_LOG1("check_postgresql_monitor", TL_ERROR, "Unable to run command : '%s'\n", command);
      sleep(300);
      continue;
    }

    fgets(buff, 64, fp);

    if(fp)
     pclose(fp);
   
    if(atoi(buff) > 0)
    {
      postgres_port = atoi(buff);
    }
    else
    {
      MONITOR_TRACE_LOG2("check_postgresql_monitor", TL_INFO, "Postgres port not found \n ");
      sleep(postgresql_delay_time);
      continue; 
    }
    
    MONITOR_TRACE_LOG2("check_postgresql_monitor", TL_INFO, "Postgres port found : '%d'\n", postgres_port);
  }
   
  while(1)
  {
    index = 1;
    conninfo = "dbname=test user=cavisson";
    /* make a connection to the database */
    dbconnection = PQconnectdb(conninfo);
    /* Check to see that the backend connection was successfully made */
    if(PQstatus(dbconnection) != CONNECTION_OK)
    {
      MONITOR_TRACE_LOG1("check_postgresql_monitor", TL_ERROR, "Connection to database failed: %s\n", PQerrorMessage(dbconnection));
      pg_mail_flag = 1;

      if(dbconnection)
      {
	PQfinish(dbconnection);
	dbconnection = NULL;
      }
    }

    /* time in seconds before executing query*/
    start_time = my_gettimeofday();

    //We can also use following query to get number of connections
    //select sum(numbackends) from pg_stat_database;
    res = PQexec(dbconnection, "SELECT count(*) from pg_stat_activity;");

    /* time in seconds after executing query*/
    end_time = my_gettimeofday();

    if(PQresultStatus(res) != PGRES_TUPLES_OK)
    {
      MONITOR_TRACE_LOG1("check_postgresql_monitor", TL_ERROR, "Query is not execute: %s", PQerrorMessage(dbconnection));

      if(pg_mail_flag)
      { 
	MONITOR_TRACE_LOG2("Msg send for check_postgresql_monitor", TL_ERROR, "Connection to database failed: %s\n", PQerrorMessage(dbconnection));
        pg_len += snprintf(pg_msg_buf + pg_len, MAX_READ_LINE_LENGTH - pg_len, "%d) Connection to database failed: %s\n\n", index, PQerrorMessage(dbconnection));
        index++;
      }
      else
      {
	MONITOR_TRACE_LOG2("Msg send for check_postgresql_monitor", TL_ERROR, "Query is not execute: %s\n", PQerrorMessage(dbconnection));
        pg_len += snprintf(pg_msg_buf + pg_len, MAX_READ_LINE_LENGTH - pg_len, "%d) Connection to database failed: %s\n\n", index, PQerrorMessage(dbconnection));
        index++;
      }

      PQclear(res);
      if(dbconnection)
      {
	PQfinish(dbconnection);
	dbconnection = NULL;
      }
    }

    query_exec_time = end_time - start_time;

    MONITOR_TRACE_LOG2("check_postgresql_monitor", TL_INFO, "Postgresql takes %lld seconds in executing query\n", query_exec_time);

    if(query_exec_time > query_exec_threshold_value)
    {
      if(query_time_mail_send == 0)
      {
      MONITOR_TRACE_LOG2("check_postgresql_monitor",TL_ERROR," Query execution Time:'%lld' seconds is more than query_exec_threshold_value:'%lld' seconds\n", query_exec_time, query_exec_threshold_value);
      pg_len += snprintf(pg_msg_buf + pg_len, MAX_READ_LINE_LENGTH - pg_len, "%d) Query execution Time:'%lld' seconds is more than query_exec_threshold_value:'%lld' seconds\n\n", index, query_exec_time, query_exec_threshold_value);
      query_time_mail_send = 1;
      index++;
      }
      else
      {
         MONITOR_TRACE_LOG2("check_postgresql_monitor",TL_ERROR," Msg already send for Query execution Time:'%lld' threshold_value:'%lld' seconds\n", query_exec_time, query_exec_threshold_value);
      }
    }
    else if(query_time_mail_send == 1)
    {
      MONITOR_TRACE_LOG2("check_postgresql_monitor",TL_ERROR," Query execution Time:'%lld' seconds is under threshold_value:'%lld' seconds\n", query_exec_time, query_exec_threshold_value);      
      query_time_mail_send = 0;
    }
    
    sprintf(command,"ss | awk '{print $6}' | grep -w %d | wc -l ", postgres_port);
    ret = run_cmd(&fp, command);

    if(ret == -1)
    {
      MONITOR_TRACE_LOG1("check_postgresql_monitor", TL_ERROR, "Unable to run command : '%s'\n", command);
      sleep(300);
      continue;
    }

    fgets(buff, 64, fp);

    if(atoi(buff) > 0)
      num_connections = atoi(buff);
    else
    {
      MONITOR_TRACE_LOG1("check_postgresql_monitor", TL_ERROR, "Could not get number of connections value\n");
      num_connections = -1;
    }
    
    if(fp)
      pclose(fp);
   
    MONITOR_TRACE_LOG2("check_postgresql_monitor", TL_INFO, "No.of connections made : '%d'\n", num_connections);

    if(num_connections > num_connection_threshold_value)
    {
      if(connection_mail_send == 0)
      {
      MONITOR_TRACE_LOG2("check_postgresql_monitor",TL_ERROR, "Number of DB (PostgreSQL) connections:'%d' are more than "
                         "threshold value: '%d'\n", num_connections, num_connection_threshold_value);
      pg_len += snprintf(pg_msg_buf + pg_len, MAX_READ_LINE_LENGTH - pg_len, "%d) Number of DB (PostgreSQL) connections:'%d' are more than "
                                 "threshold value: '%d'\n\n", index, num_connections, num_connection_threshold_value);
      connection_mail_send = 1;
      index++;
      }
      else
      {
         MONITOR_TRACE_LOG2("check_postgresql_monitor",TL_ERROR, "Msg already send once for number of DB (PostgreSQL) connections:'%d' are more than "
                         "threshold value: '%d'\n", num_connections, num_connection_threshold_value);
      }
    }
    else if(connection_mail_send == 1)
    {
      MONITOR_TRACE_LOG2("check_postgresql_monitor",TL_ERROR, "Number of DB (PostgreSQL) connections:'%d' are under "
                         "threshold value: '%d'\n", num_connections, num_connection_threshold_value);
      connection_mail_send = 0;
    }
    
    if(send_mail)
    {
      if(pg_msg_buf[0] != '\0')
      {
        pthread_mutex_lock(&thread_mutex);
          sprintf(sub_of_mail,"Alert | %s | CSHM | %s | %s | Postgresql Monitor", machine_mode,machine_name,my_controller);
          snprintf(msg, MAX_READ_LINE_LENGTH, "Below are the alerts generated on postgresql:\n\n%s\n", pg_msg_buf);
          MONITOR_TRACE_LOG2("Msg send for check_postgresql_monitor",TL_INFO, "%s", msg);
          send_mail_to_user(sub_of_mail, msg, NULL);
        pthread_mutex_unlock(&thread_mutex);
      }
    }
    else if(pg_msg_buf[0] != '\0')
    {
      MONITOR_TRACE_LOG1("Msg not send for check_postgresql_monitor",TL_ERROR, "Below are the alerts generated on postgresql:\n\n%s\n", pg_msg_buf);
    }
     
    pg_msg_buf[0] = '\0';
    pg_len=0;

    /* close the connection to the database and cleanup */
    PQclear(res);
    if(dbconnection)
    {
      PQfinish(dbconnection);
      dbconnection = NULL;
    }

    sleep(postgresql_delay_time);
  }
  return NULL;
}

void* check_file_content()
{
  MONITOR_TRACE_LOG2("check_file_content", TL_INFO, "check_file_content thread started \n" );

  int idx = 0;
  struct stat st;
  char mail_buf[MAX_MAIL_BUFFER_SIZE];
  char err_buf[MAX_MAIL_BUFFER_SIZE];
  char sub_of_mail[MAX_MAIL_BUFFER_SIZE];
  int mail_buf_len = 0;
  
  sprintf(sub_of_mail, "Critical | %s | CSHM | %s | %s | files have been modified", machine_mode, machine_name, my_controller);

  while(1)
  {
    mail_buf[0] = '\0';
    mail_buf_len = 0;
    for(idx = 0; idx < file_count; idx++)
    {
      if(stat(file_change_struct[idx].file_name, &st) == -1) 
      {
        sprintf(err_buf, "Error in Reading file '%s'. Error:'%s'\n", file_change_struct[idx].file_name, strerror(errno));
        mail_buf_len += snprintf(mail_buf + mail_buf_len, MAX_MAIL_BUFFER_SIZE - mail_buf_len, "%s\n", err_buf); 
        MONITOR_TRACE_LOG1("check file size and lmd", TL_ERROR, "%s", err_buf );
      }
      //Due to Permission issues or if file size is 0 then st.st_size eqauls 0
      else if(!st.st_size)
      {
        sprintf(err_buf, "Error: File '%s' has either 0 size or having permission issue\n", file_change_struct[idx].file_name);
        mail_buf_len += snprintf(mail_buf + mail_buf_len, MAX_MAIL_BUFFER_SIZE - mail_buf_len, "%s\n", err_buf); 
        MONITOR_TRACE_LOG1("check file size and lmd", TL_ERROR,"%s", err_buf );
      }
      else if(S_ISREG(st.st_mode) == 0)
      {
        sprintf(err_buf, "Error: File '%s' is not regular file now.\n", file_change_struct[idx].file_name);
        mail_buf_len += snprintf(mail_buf + mail_buf_len, MAX_MAIL_BUFFER_SIZE - mail_buf_len, "%s\n", err_buf); 
        MONITOR_TRACE_LOG1("check file size and lmd", TL_ERROR, "%s", err_buf);
      }
      else
      {
        if(file_change_struct[idx].lmd != st.st_mtime || file_change_struct[idx].file_size != st.st_size)
        {
          sprintf(err_buf, "File '%s' has been modified \n", file_change_struct[idx].file_name);
          mail_buf_len += snprintf(mail_buf + mail_buf_len, MAX_MAIL_BUFFER_SIZE - mail_buf_len, "%s\n", err_buf); 
          MONITOR_TRACE_LOG1("check file size and lmd", TL_ERROR, "%s", err_buf);
        }
      }
    }

    if(mail_buf[0] != '\0')
    {
      pthread_mutex_lock(&thread_mutex);

      if(send_mail)
      {
        MONITOR_TRACE_LOG2("check_file_content", TL_INFO, "Message to be mailed is:\n%s", msg);
        send_mail_to_user(sub_of_mail, mail_buf, NULL);      
      }
      else
      {
        MONITOR_TRACE_LOG1("Msg not send for check file content", TL_ERROR, "%s", msg);
      }

      pthread_mutex_unlock(&thread_mutex);
    }

    sleep(300);
  }
  return NULL;
}

void* remove_builds_from_rel_dir()
{
  FILE *fp = NULL;
  char command[1024], buff[64];
  int ret, no_of_builds, no_of_build_to_remove;

  MONITOR_TRACE_LOG2("remove_builds_from_rel_dir", TL_INFO, "remove_builds_from_rel_dir thread started \n");

  while(1)
  {
      sprintf(command, "ls -f %s/.rel/*.bin | wc -l", ns_wdir);
      ret = run_cmd(&fp, command);

      if(ret == -1)
      {
	MONITOR_TRACE_LOG1("remove_builds_from_rel_dir", TL_ERROR, "Unable to run command : '%s'\n", command);
	sleep(300);
	continue;
      }

      fgets(buff, 64, fp);
      no_of_builds = atoi(buff);

      if(fp)
	pclose(fp);

      if(no_of_builds > builds_threshold)
      {
	no_of_build_to_remove = no_of_builds - builds_threshold; 

	sprintf(command, "list=`ls -tr %s/.rel/*.bin | head -%d`; rm $list;", ns_wdir, no_of_build_to_remove);

	ret = system(command);

	if(ret == -1)
	{
	  MONITOR_TRACE_LOG1("remove_builds_from_rel_dir", TL_ERROR, "Unable to run command : '%s'\n", command);
	  sleep(300);
	  continue;
	}

	pthread_mutex_lock(&thread_mutex);

	MONITOR_TRACE_LOG2("remove_builds_from_rel_dir", TL_INFO, "Total number of builds '%d' in .rel directory is greater than threshold limit '%d' so removing '%d' builds from '%s/.rel'\n", no_of_builds, builds_threshold, no_of_build_to_remove, ns_wdir);    
	sprintf(sub_of_mail,"INFO | %s | CSHM | %s | %s | Removed builds from .rel directory", machine_mode,machine_name,my_controller);
	snprintf(msg, MAX_READ_LINE_LENGTH, "Total number of builds '%d' in .rel directory is greater than threshold limit '%d' so removing '%d' builds from '%s/.rel'\n", no_of_builds, builds_threshold, no_of_build_to_remove, ns_wdir);
      
        if(send_mail)
        {
	  MONITOR_TRACE_LOG2("Msg send for remove_builds_from_rel_dir", TL_INFO, "%s",msg);
	  send_mail_to_user(sub_of_mail, msg, NULL);
        }
        else
        {
	  MONITOR_TRACE_LOG1("Msg not send for remove_builds_from_rel_dir", TL_ERROR, "%s",msg);
        }

	pthread_mutex_unlock(&thread_mutex);
      }
    sleep(rel_dir_delay_time);
  }
  return NULL;
}

int check_mandatory_process_by_ps(int i)
{
  FILE *fp = NULL;
  char command[1024] = "", buff[512] = "";
  int ret;
  int pid = 0;
  int flag_to_check_process_is_running = 0;
  int exe = 0;

  MONITOR_TRACE_LOG2("check_mandatory_process_by_ps", TL_INFO, "Entered the check_mandatory_process_by_ps for the process = '%s'\n",mandatory_process[i].process_name);

  while(exe == 0)
  {
    sprintf(command, "ps -ef | grep -w %s | grep -v \"grep\" | awk '{print $2}'", mandatory_process[i].process_name);

    ret = run_cmd(&fp, command);

    if(ret == -1)
    {
      MONITOR_TRACE_LOG1("check_mandatory_process_by_ps", TL_ERROR, "Unable to run command : '%s'\n", command);
      sleep(30);
    }
    else
    {
      exe = 1;
      while(fgets(buff, 512, fp) != NULL)
      {
	pid = atoi(buff);


	MONITOR_TRACE_LOG3("check_mandatory_process_by_ps", TL_INFO, "pid found by ps command for '%s' = '%d' \n", mandatory_process[i].process_name, pid);

	if(pid > 0)
	{	
	  flag_to_check_process_is_running = 1;
	  break; 
	  buff[0] = '\0';
	}
	else
        {
	  MONITOR_TRACE_LOG2("check_mandatory_process_by_ps", TL_ERROR, "wrong pid found for '%s' = '%d' \n", mandatory_process[i].process_name, pid);
        }
      }
      
      if(pid > 0)
      { 
	MONITOR_TRACE_LOG3("check_mandatory_process_by_ps", TL_INFO, "status for '%s' = '1' as the pid found = '%d' \n",  mandatory_process[i].process_name, pid);
      }
      else
      { 
        MONITOR_TRACE_LOG2("check_mandatory_process_by_ps", TL_ERROR, "Pid not found for '%s' \n", mandatory_process[i].process_name);
      }
       
      if(fp)
        pclose(fp);
    }
  }
  return flag_to_check_process_is_running; 
}


void* check_mandatory_process()
{
  FILE *fp = NULL;
  char command[1024] = "";
  char buff[512] = "";
  char mail_buf_run[16*1024] = "";
  char mail_buf_not_run[16*1024] = "";
  char first_time_mail_send = 1;
  int ret, i, pid = 0;
  int flag_to_check_process_is_running[total_mandatory_process_to_monitor];
  int flag_to_send_mail[total_mandatory_process_to_monitor] ;
  int mail_buf_run_length = 0;
  int mail_buf_not_run_length = 0;
  int mail_buf_size = 16 * 1024;
  struct stat pidfile; 

  MONITOR_TRACE_LOG2("check_mandatory_process", TL_INFO, "check_mandatory_process thread started\n");

  while(1)
  {
    for(i = 0; i < total_mandatory_process_to_monitor; i++)
    {
      if(strcmp(mandatory_process[i].process_name,ndp_path) != is_test_running && strcmp(mandatory_process[i].process_name,nsu_db_upload_path) != is_test_running && strcmp(mandatory_process[i].process_name,ndu_db_upload_path) != is_test_running)
      {
        if(strcmp(mandatory_process[i].process_name,log_rd_path) != is_test_running && strcmp(mandatory_process[i].process_name,log_wr_path) != is_test_running && strcmp(mandatory_process[i].process_name,event_log_path) != is_test_running)
        {
          flag_to_check_process_is_running[i] = 0;

          buff[0]='\0';

          if(stat(mandatory_process[i].process_pid_path, &pidfile))
          {
	    fprintf(stderr, "File '%s' does not exists\n" , mandatory_process[i].process_pid_path);

	    if(strcmp(mandatory_process[i].process_pid_path,"NULL") != 0)
	      MONITOR_TRACE_LOG2("check_mandatory_process", TL_INFO, "File '%s' does not exists\n", mandatory_process[i].process_pid_path);

	    if(check_mandatory_process_by_ps(i))
	      flag_to_check_process_is_running[i] = 1;
          }
          else
          {
	    MONITOR_TRACE_LOG3("check_mandatory_process", TL_INFO, "File '%s' exists\n", mandatory_process[i].process_pid_path);
      
            fp = fopen(mandatory_process[i].process_pid_path, "r");

            if(fp == NULL)
            {
              MONITOR_TRACE_LOG2("check_mandatory_process", TL_ERROR, "Could not open file '%s'\n", mandatory_process[i].process_pid_path);
            }
            else
            {
              if(fgets(buff, 1024, fp) == NULL)
              {
                MONITOR_TRACE_LOG1("check_mandatory_process", TL_INFO, "Cannot read pid from file\n");
                fclose(fp);
              }
              else
              {
                fclose(fp);

                pid = atoi(buff);

	        MONITOR_TRACE_LOG3("check_mandatory_process", TL_INFO, "pid of '%s' = '%d' exists\n", mandatory_process[i].only_mandatory_process_name, pid);

	        if(pid > 0)
	        { 
	          sprintf(command, "ps -o 'cmd' %d | grep %s | wc -l",pid,mandatory_process[i].only_mandatory_process_name);

	          ret = run_cmd(&fp, command);

	          if(ret == -1)
	          {
	            MONITOR_TRACE_LOG1("check_mandatory_process", TL_ERROR, "Unable to run command : '%s'\n", command);
	            sleep(300);
	            continue;
	          }

	          fgets(buff, 512, fp);

	          if(fp)
	            pclose(fp);

	          ret = atoi(buff);

	          MONITOR_TRACE_LOG3("check_mandatory_process", TL_INFO, "Status of process '%s' = '%d' \n", mandatory_process[i].only_mandatory_process_name, ret);

	          if(ret == 1)
	            flag_to_check_process_is_running[i] = 1;
	        }  
	        else
	        {
	          if(check_mandatory_process_by_ps(i))
	            flag_to_check_process_is_running[i] = 1;
	        }
              }
            }
          }

          if(buff[0] == '\0')
          {
	    pthread_mutex_lock(&thread_mutex);
	    pthread_mutex_unlock(&thread_mutex);
          }  
          
          /* check send_mail is enable and mail_buf_length must be less than buffer size and check flags*/
          if((mail_buf_not_run_length < mail_buf_size))
          {
            if(flag_to_check_process_is_running[i] == 0)
            {
	      flag_to_send_mail[i] = 1;

              if(strcmp(mandatory_process[i].process_name, postgresql_path) == 0)
                mail_buf_not_run_length += snprintf(mail_buf_not_run + mail_buf_not_run_length, mail_buf_size - mail_buf_not_run_length, "       Process name : 'Postgres'\n");
              
	      else if(strcmp(mandatory_process[i].process_name, cmon_path) == 0)
                mail_buf_not_run_length += snprintf(mail_buf_not_run + mail_buf_not_run_length, mail_buf_size - mail_buf_not_run_length, "       Process name : 'Cmon'\n");

              else
   	        mail_buf_not_run_length += snprintf(mail_buf_not_run + mail_buf_not_run_length, mail_buf_size - mail_buf_not_run_length, "       Process name : '%s'\n", mandatory_process[i].process_name);
            }

            if(flag_to_check_process_is_running[i] == 1 && flag_to_send_mail[i] == 1)
            { 
              if(strcmp(mandatory_process[i].process_name, postgresql_path) == 0)
                 mail_buf_run_length += snprintf(mail_buf_run + mail_buf_run_length, mail_buf_size - mail_buf_run_length, "       Process name : 'Postgres'\n");

              else if(strcmp(mandatory_process[i].process_name, cmon_path) == 0)
                mail_buf_run_length += snprintf(mail_buf_run + mail_buf_run_length, mail_buf_size - mail_buf_run_length, "       Process name : 'Cmon'\n");
              else
                mail_buf_run_length += snprintf(mail_buf_run + mail_buf_run_length, mail_buf_size - mail_buf_run_length, "       Process name : '%s'\n", mandatory_process[i].process_name); 

              flag_to_send_mail[i] = 0;
	      first_time_mail_send = 1;
            }
          }
          else
          {
            MONITOR_TRACE_LOG1("check_mandatory_process", TL_ERROR, "unable to write in buffer due to less memory");
          }  
        }
      }
    }

    if(send_mail)
    {
      /* if mail buffer contain some data in that case mail is sent */
      if(mail_buf_not_run[0] != '\0')
      {
        pthread_mutex_lock(&thread_mutex);

	MONITOR_TRACE_LOG1("check_mandatory_process", TL_ERROR, "Alert: Below are the process names which are not running:\n '%s' \n", mail_buf_not_run);
	if(first_time_mail_send == 1)
	{
	  sprintf(sub_of_mail,"Critical | %s | CSHM | %s | %s | Mandatory Processes Not Running", machine_mode,machine_name,my_controller);
	  snprintf(msg, MAX_READ_LINE_LENGTH, "Alert: Below are the process names which are not running:\n\n%s", mail_buf_not_run);
	  MONITOR_TRACE_LOG2("Msg send for check_mandatory_process", TL_INFO, "%s \n",msg);
	  send_mail_to_user(sub_of_mail, msg, NULL);
        }
        pthread_mutex_unlock(&thread_mutex);

        mail_buf_not_run[0] = '\0';
        mail_buf_not_run_length = 0;
        
	//Keyword is disable hence it will send only one time mail
	if(! process_alert_frequency)
	  first_time_mail_send = 0;
      }
      if(mail_buf_run[0] != '\0')
      {
        pthread_mutex_lock(&thread_mutex);

        MONITOR_TRACE_LOG2("check_mandatory_process", TL_INFO, "Below are the process names which are now running:\n%s", mail_buf_run);
        sprintf(sub_of_mail,"Alert Cleared | %s | CSHM | %s | %s | Mandatory Processes Now Running", machine_mode,machine_name,my_controller);
        snprintf(msg, MAX_READ_LINE_LENGTH, "Below are the process names which are now running:\n\n%s", mail_buf_run);
        MONITOR_TRACE_LOG2("Msg send for check_mandatory_process", TL_INFO, "%s\n", msg);
        send_mail_to_user(sub_of_mail, msg, NULL);

        pthread_mutex_unlock(&thread_mutex);

        mail_buf_run[0] = '\0';
        mail_buf_run_length = 0;
      }
    }
    else
    {
      if(mail_buf_not_run[0] != '\0')
      {
        MONITOR_TRACE_LOG1("Msg not send for check_mandatory_process", TL_ERROR, "Alert: Below are the process names which are not running:\n%s", mail_buf_not_run);

        mail_buf_not_run[0] = '\0';
        mail_buf_not_run_length = 0;
      }
      if(mail_buf_run[0] != '\0')
      {
        MONITOR_TRACE_LOG1("Msg not send for check_mandatory_process", TL_ERROR, "Below are the mandatory process names which are now running:\n%s", mail_buf_run);

        mail_buf_run[0] = '\0';
        mail_buf_run_length = 0;
      }
    }
    sleep(mandatory_process_delay_time);
  }
  return NULL;
}

void* check_unwanted_process()
{
  FILE *fp = NULL;
  char command[1024] = "", buff[512] = "", mail_buf_run[16*1024] = "", mail_buf_not_run[16*1024] = "";
  int ret, i, pid = 0;
  char flag_to_check_process_is_not_running[total_unwanted_process_to_monitor];
  char flag_to_send_mail[total_unwanted_process_to_monitor];
  int mail_buf_length = 0;
  int mail_buf_size = 16 * 1024;
  int flag_if_pid_not_matched = 0;

  MONITOR_TRACE_LOG2("check_unwanted_process", TL_INFO, "check_unwanted_process thread started: \n");

  while(1)
  {
    for(i=0; i < total_unwanted_process_to_monitor; i++)
    {
      if(flag_to_check_process_is_not_running[i] == 1)
	flag_to_check_process_is_not_running[i] = 0;

      buff[0] = '\0';
      sprintf(command, "ps -ef | grep -w \"%s\" | grep -v \"grep\" | awk '{print $2}'", unwanted_process[i].process_name);

      ret = run_cmd(&fp, command);

      if(ret == -1)
      {
	MONITOR_TRACE_LOG1("check_unwanted_process", TL_ERROR, "Unable to run command : '%s'\n", command);
	sleep(300);
	continue;
      }

      while(fgets(buff, 512, fp) != NULL)
      {
	pid = atoi(buff);

	MONITOR_TRACE_LOG2("check_unwanted_process", TL_INFO, "Pid for unwanted process '%s' found = '%d'\n", unwanted_process[i].process_name, pid);

	if(pid > 0)
	{
	  flag_to_check_process_is_not_running[i] = 1;
	  flag_if_pid_not_matched = 0;
	  break;
	}
	else
	{
	  flag_if_pid_not_matched = 1;
	}
      }

      if(fp)
	pclose(fp);

      if(buff[0] == '\0' || flag_if_pid_not_matched == 1)
      {
	if(flag_to_check_process_is_not_running[i] == 1)
	  flag_to_check_process_is_not_running[i] = 0;

	//if(flag_to_send_mail[i] == 1)
	  //flag_to_send_mail[i] = 0;

      }

      /* check send_mail is enable and mail_buf_length must be less than buffer size and check flags*/
      if((mail_buf_length < mail_buf_size) && flag_to_check_process_is_not_running[i] == 1 && flag_to_send_mail[i] == 0)
      {
	mail_buf_length += snprintf(mail_buf_run + mail_buf_length, mail_buf_size - mail_buf_length, "Process name : '%s'\n",
	                            unwanted_process[i].process_name);
        flag_to_send_mail[i] = 1;
      }
      if((mail_buf_length < mail_buf_size) && flag_to_check_process_is_not_running[i] == 0 && flag_to_send_mail[i] == 1)
      {
        mail_buf_length += snprintf(mail_buf_not_run + mail_buf_length, mail_buf_size - mail_buf_length, "Process name : '%s'\n",
                                    unwanted_process[i].process_name);
        flag_to_send_mail[i] = 0;     
      }

    }

    /* if mail buffer contain some data in that case mail is sent */
    if(mail_buf_run[0] != '\0')
    {
      pthread_mutex_lock(&thread_mutex);

      MONITOR_TRACE_LOG2("check_unwanted_process", TL_ERROR, "Alert: Below unwanted process are running:\n%s", mail_buf_run); 
      sprintf(sub_of_mail,"Alert | %s | CSHM | %s | %s | Unwanted Processes Running", machine_mode,machine_name,my_controller);
      snprintf(msg, MAX_READ_LINE_LENGTH, "Alert: Below unwanted process are running:\n%s", mail_buf_run);

      if (send_mail)
      {
        MONITOR_TRACE_LOG2("Msg send for check_unwanted_process", TL_INFO, "%s",msg);
        send_mail_to_user(sub_of_mail, msg, NULL);
      }
      else
      {
        MONITOR_TRACE_LOG1("Msg not send for check_unwanted_process", TL_ERROR, "%s",msg);
      }

      pthread_mutex_unlock(&thread_mutex);

      mail_buf_run[0] = '\0';
      mail_buf_length = 0;
    }
    if(mail_buf_not_run[0] != '\0')
    {
      pthread_mutex_lock(&thread_mutex);

      MONITOR_TRACE_LOG2("check_unwanted_process", TL_INFO, "Below unwanted process are not running now:\n%s", mail_buf_not_run); 
      sprintf(sub_of_mail,"Alert Cleared | %s | CSHM | %s | %s | Unwanted Processes Running", machine_mode,machine_name,my_controller);
      snprintf(msg, MAX_READ_LINE_LENGTH, "Below unwanted process are not running now :\n%s", mail_buf_not_run);

      if (send_mail)
      {
        MONITOR_TRACE_LOG2("Msg send for check_unwanted_process", TL_INFO, "%s",msg);
        send_mail_to_user(sub_of_mail, msg, NULL);
      }
      else
      {
        MONITOR_TRACE_LOG1("Msg not send for check_unwanted_process", TL_ERROR, "%s",msg);
      }

      pthread_mutex_unlock(&thread_mutex);
      
      mail_buf_not_run[0] = '\0';
      mail_buf_length = 0;
    }
    sleep(unwanted_process_delay_time);
  }
  return NULL;
}


void* filesystem_threshold_monitor()
{
  char file_system_buf[1024 * 16];
  char mail_buf_fs[16*1024] = "", mail_buf_not_mount[16*1024] = "";
  int len_fs= 0;
  int len_mount=0;
  int max_length=16*1024;
  char buf[1024];
  // new variable defined for storing value of df -h command
  char file_system[1024] = "";
  char size[8]="";
  char used[8]="";
  char avail[8]="";
  char use_percent[8]="";
  char mounted_point[1024]="";

  MONITOR_TRACE_LOG2("filesystem_threshold_monitor", TL_INFO, "filesystem_threshold_monitor thread started \n");

  while(1)
  { 
    FILE *fp = NULL;
    char buff[128];
    int i; //fs_counter = 0;

    sprintf(buff, "df -h");
    if (run_cmd(&fp, buff) == -1) 
    {
      MONITOR_TRACE_LOG1("filesystem_threshold_monitor", TL_ERROR, "Unable to run df -h command\n");
      sleep(filesystem_delay_time);
      continue;
    }

    setvbuf(fp, file_system_buf, _IOFBF, 1024*16);
    fgets(buf, 1024, fp); //skipping first line

    while(fgets(buf, 1024, fp))
    {
      sscanf(buf, "%s%s%s%s%s%s", file_system, size, used, avail, use_percent, mounted_point );
      for(i = 0; i < no_of_fs_keyword; i++)
      {
	if (!strcmp(mounted_point, fs_mounted_name[i]) && !strcmp(file_system,  file_system_name[i]))
	{
	  filesystem_mounted_flag[i] = 1; // flag is activated
	  if(atoi(use_percent) > fs_threshold_for_mail[i])
	  {
	  fs_threshold_for_mail[i] = fs_threshold_for_mail[i] + 5;
	  MONITOR_TRACE_LOG2("filesystem_threshold_monitor", TL_INFO, "Used space '%s' of filesystem '%s' mounted on '%s' crossed"
	      " it's threshold limit %d.\n", use_percent, file_system_name[i], fs_mounted_name[i], fs_threshold[i]);
          len_fs += snprintf(mail_buf_fs + len_fs, max_length - len_fs, "Used space '%s' of filesystem '%s' mounted on '%s' crossed "
                                                               "it's threshold limit '%d%%'.\n\n", use_percent,
                                                               file_system_name[i], fs_mounted_name[i], fs_threshold[i]);
          }
        }
      }
    }

    for(i=0; i< no_of_fs_keyword; i++)
    {
      if(filesystem_mounted_flag[i] == 0)
      {
        MONITOR_TRACE_LOG2("filesystem_threshold_monitor", TL_INFO, "Filesystem '%s' configured in '%s' is not mounted on '%s'.\n", 
                                                                    file_system_name[i], conf_file_path, fs_mounted_name[i]);
        len_mount += snprintf(mail_buf_not_mount + len_mount, max_length - len_mount, "Filesystem '%s' configured in '%s' is not mounted on '%s'.\n\n", file_system_name[i], conf_file_path, fs_mounted_name[i]);
      }
      else
        filesystem_mounted_flag[i] = 0; //reset all flag for next iteration
    }


    if(send_mail)
    {
      if(mail_buf_fs[0] != '\0' )
      {
        pthread_mutex_lock(&thread_mutex);
          sprintf(sub_of_mail,"Critical | %s | CSHM | %s | %s | Disk Running Out of Space", machine_mode,machine_name,my_controller);
          snprintf(msg, MAX_READ_LINE_LENGTH, "%s", mail_buf_fs);
          MONITOR_TRACE_LOG2("Msg send  for filesystem_threshold_monitor", TL_INFO, "%s", msg);
          send_mail_to_user(sub_of_mail, msg, NULL);
        pthread_mutex_unlock(&thread_mutex);
        len_fs = 0;
        mail_buf_fs[0] = '\0';
      }
      if(mail_buf_not_mount[0] != '\0')
      {
        pthread_mutex_lock(&thread_mutex);
          sprintf(sub_of_mail,"Critical | %s | CSHM | %s | %s | Filesystem Not Mounted", machine_mode,machine_name,my_controller);
          snprintf(msg, MAX_READ_LINE_LENGTH, "%s", mail_buf_not_mount);
          MONITOR_TRACE_LOG2("Msg send  for filesystem_threshold_monitor", TL_INFO, "%s", msg);
          send_mail_to_user(sub_of_mail, msg, NULL);
        pthread_mutex_unlock(&thread_mutex);
        len_mount = 0;
        mail_buf_not_mount[0] = '\0';    
      }
    }
    else
    {
      if(mail_buf_fs[0] != '\0' )
      { 
        MONITOR_TRACE_LOG1("Msg not send for filesystem_threshold_monitor", TL_ERROR, "%s", mail_buf_fs);
      }
      if(mail_buf_not_mount[0] != '\0')
      {
        MONITOR_TRACE_LOG1("Msg not send for filesystem_threshold_monitor", TL_ERROR, "%s", mail_buf_not_mount);
      }
      len_fs = 0;
      len_mount = 0;
      mail_buf_fs[0] = '\0';
      mail_buf_not_mount[0] = '\0';
    } 
    file_system[0] = '\0'; 
    size[0] = '\0';
    used[0] = '\0';
    avail[0] = '\0';
    use_percent[0] = '\0';
    mounted_point[0] = '\0';

    sleep(filesystem_delay_time);

    if(fp)
      pclose(fp);

  }
  return NULL;
}

void* max_file_size_in_tmp()
{
  if(! enable_tmp_dir_scanning)
  {
      MONITOR_TRACE_LOG1("max_file_size_in_tmp", TL_INFO, "max_file_size_in_tmp thread will not started as keyword 'ENABLE_TMP_DIR_SCAN' is disable\n");
      return NULL;
  }

  FILE *fp;
  struct dirent **entry_list;
  struct stat filestat;
  int i, j, num_files, ret, mail_buf_length = 0; 
  int large_file_info_len = 0, large_file_info_max_len = 0;
  long long int file_size;
  int mail_buf_size = 16 * 1024;
  char cmd[1024] = "", buf[1024] = "", file_path[1024] = "", mail_buf[16*1024] = "";
  int max_size_of_file_in_tmp_in_mb = max_size_of_file_in_tmp/(1024*1024); 
  char file_size_str[50];
  char path_of_file[256];

  largeFileInfo *large_file_info = NULL;

  MONITOR_TRACE_LOG2("max_file_size_in_tmp", TL_INFO, "max_file_size_in_tmp thread started\n");

  while(1)
  {
    num_files = scandir("/tmp", &entry_list, 0, alphasort);
    /* If any error occurs. Eg Directory does not exist etc */
    if(num_files < 0)
    {
      MONITOR_TRACE_LOG1("max_file_size_in_tmp", TL_ERROR, "Unable to get files and directories from /tmp from scandir()");
      sleep(300);
      continue;
    }

    for(i = 0; i < num_files; i++)
    {
      struct dirent *entry;
      entry = entry_list[i];

      /* Skip . and .. directories */
      if(!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
	continue;

      /* In case of regular file, using stat function to get file size.
       * In case of directory, links etc, using du command to get size */
      if(nslb_get_file_type("/tmp", entry) == DT_REG)
      {
	sprintf(file_path, "/tmp/%s", entry->d_name);
	/* In case of error in stat function, skip that file */
	if(stat(file_path, &filestat) != 0)
	{
	  MONITOR_TRACE_LOG3("max_file_size_in_tmp", TL_ERROR, "Not able to stat file '%s', "
	      "Error = '%s' \n", file_path, nslb_strerror(errno));
	  continue;
	}

	file_size = filestat.st_size;
	/* if file size is not exceeding threshold, then skip file */
	if(file_size < max_size_of_file_in_tmp)
	  continue;
	else
	{
	  pthread_mutex_lock(&thread_mutex);
	  pthread_mutex_unlock(&thread_mutex);
	}    
      }
      else
      {
	sprintf(cmd, "du -sbD /tmp/%s", entry->d_name);

	ret = run_cmd(&fp, cmd);
	if(ret == -1)
	{
	  MONITOR_TRACE_LOG1("max_file_size_in_tmp", TL_ERROR, "command '%s' failed", cmd);
	  sleep(300);
	  continue;
	}

	fgets(buf, 1024, fp);

	if(fp)
	  pclose(fp);
     
	file_size = atoll(buf);

  
	buf[0] = '\0';

	/* if file size is not exceeding threshold, then skip file */
	if(file_size < max_size_of_file_in_tmp)
	  continue;
	else
	{
	  pthread_mutex_lock(&thread_mutex);
	  pthread_mutex_unlock(&thread_mutex);
	}    
      }
 
      /* Check if mail has already been sent for this file */
      for(j = 0; j < large_file_info_len; j++)
      {
	if((large_file_info[j].inode == entry->d_ino) && !strcmp(large_file_info[j].file_name, entry->d_name))
	  break;
      }

      /* Mail has already been sent for this file, so continue */
      if(j < large_file_info_len)
	continue;

      sprintf(cmd, "du -shD /tmp/%s", entry->d_name);
        
      ret = run_cmd(&fp, cmd);
      if(ret == -1)
      { 
        MONITOR_TRACE_LOG1("max_file_size_in_tmp", TL_ERROR, "command '%s' failed", cmd);
        sleep(300);
        continue;
      }
        
      fgets(buf, 1024, fp);
   
      MONITOR_TRACE_LOG3("max_file_size_in_tmp", TL_INFO, "Size : %s", buf);
        
      sscanf(buf,"%s %s", file_size_str, path_of_file); 

      MONITOR_TRACE_LOG2("max_file_size_in_tmp", TL_INFO, "Size : %s ,  File : %s\n", file_size_str, path_of_file);
        
      if(fp)
        pclose(fp);
      
      buf[0] = '\0';

      

      /* Realloc list of files, for which mail has already been sent */
      if(large_file_info_len == large_file_info_max_len)
      {
	large_file_info_max_len += 10;
	MY_REALLOC(large_file_info, (sizeof(largeFileInfo) * large_file_info_max_len), "LargeFileInfo", -1);
      }

      /* Add file name and inode to list */
      large_file_info[large_file_info_len].inode = entry->d_ino;
      snprintf(large_file_info[large_file_info_len].file_name, 256, "%s", entry->d_name);

      MONITOR_TRACE_LOG2("max_file_size_in_tmp", TL_INFO, "Size of file name '%s' inode '%lld' is '%s' "
	  "is greater than threshold '%lld' bytes \n", entry->d_name, 
	  large_file_info[large_file_info_len].inode, file_size_str, max_size_of_file_in_tmp);

      large_file_info_len++;
      
      /* Add file name and size to buffer. This buffer will be sent in mail */

      if((mail_buf_length < mail_buf_size))
	mail_buf_length += snprintf(mail_buf + mail_buf_length, mail_buf_size - mail_buf_length, "      %s : %s\n", file_size_str, entry->d_name);
      free(entry);
    }
    
    free(entry_list);

    if(mail_buf[0] != '\0')
    {
      pthread_mutex_lock(&thread_mutex);
      MONITOR_TRACE_LOG2("max_file_size_in_tmp", TL_INFO, "Alert: Size of following files or directories in /tmp dir exceeds threshold '%d' Mb. Please Note, buffer size of alert is 16KB. Data exceeding this size is discarded.\n%s", max_size_of_file_in_tmp_in_mb, mail_buf);
      sprintf(sub_of_mail,"Critical | %s | CSHM | %s | %s | Max File Size in /tmp Threshold Crossed", machine_mode,machine_name,my_controller);
      //mail_buf can handle approx. 60 files that have max value than threshold value because it is 16kb
      snprintf(msg, MAX_READ_LINE_LENGTH, "Alert: Size of following files or directories "
	  "in /tmp dir exceeds threshold '%d' Mb. Please Note, buffer size of alert is 16KB. Data exceeding this size is discarded.\n\n%s", max_size_of_file_in_tmp_in_mb, mail_buf);
      
      if(send_mail)
      {
        MONITOR_TRACE_LOG2("Msg send for max_file_size_in_tmp", TL_INFO, "%s\n", msg);
        send_mail_to_user(sub_of_mail, msg, NULL);
      }
      else
      {
        MONITOR_TRACE_LOG1("Msg not send for max_file_size_in_tmp", TL_ERROR, "%s\n", msg);
      }
  
      pthread_mutex_unlock(&thread_mutex);

      mail_buf[0] = '\0';
      mail_buf_length = 0;
    }

    sleep(max_file_size_delay_time);
  }
  return NULL;
}


void* check_cleanup_policy_status()
{ 
  FILE *fp = NULL;
  char command[1024];
  long long int epoch_of_cleanup;
  long long int curr_epoch;
  int difference_of_epoch;
  char buff[1024];
  int ret;
  int send_mail_for_cleanup = 0;

  MONITOR_TRACE_LOG2("check_cleanup_policy_status", TL_INFO, "check_cleanup_policy_status thread started \n");

  while(1)
  {
    sprintf(command, "ls -l --time-style='+%%m/%%d/%%Y  %%H:%%M:%%S' /home/cavisson/%s/NDEPurge/log/cleanup.log | awk '{print $6 \" \" $7}'", my_controller);

    ret = run_cmd(&fp, command);

    if(ret == -1)
    {
      MONITOR_TRACE_LOG1("check_cleanup_policy_status", TL_ERROR, "Unable to run command : '%s'\n", command);
      sleep(300);
      continue;
    }
    else
    { 
      fgets(buff, 1024, fp);

      if(fp)
	pclose(fp); 

      MONITOR_TRACE_LOG2("check_cleanup_policy_status", TL_INFO, "date and time of cleanup.log = '%s'", buff);

      sprintf(command, "date --date=\"%s\" +%%s", buff);

      ret = run_cmd(&fp, command);

      if(ret == -1)
      {
	MONITOR_TRACE_LOG1("check_cleanup_policy_status", TL_ERROR, "Unable to run command : '%s'\n", command);
	sleep(300);
	continue;
      } 
      else
      { 
	fgets(buff, 1024, fp);

	if(fp)
	  pclose(fp);

	epoch_of_cleanup=atoll(buff);

	MONITOR_TRACE_LOG2("check_cleanup_policy_status", TL_INFO, "epoch time of cleanup.log = '%lld' \n", epoch_of_cleanup);

	strcpy(command, "date +%s");

	ret = run_cmd(&fp, command);

	if(ret == -1)
	{
	  MONITOR_TRACE_LOG1("check_cleanup_policy_status", TL_ERROR, "Unable to run command : '%s' \n", command);
	  sleep(300);
	  continue;
	}
	else
	{
	  fgets(buff, 1024, fp);

	  if(fp)
	    pclose(fp);

	  curr_epoch=atoll(buff);

	  MONITOR_TRACE_LOG2("check_cleanup_policy_status", TL_INFO, "current epoch time =  '%lld' \n", curr_epoch);

	  difference_of_epoch=curr_epoch - epoch_of_cleanup;

	  if( difference_of_epoch > 86500 && send_mail_for_cleanup != 1)
	  {
	    pthread_mutex_lock(&thread_mutex);

	    MONITOR_TRACE_LOG2("check_cleanup_policy_status", TL_ERROR, "Raising alert for Cleanup Policy Not Running");
	    sprintf(sub_of_mail,"Critical | %s | CSHM | %s | %s | Cleanup Policy Not Running", machine_mode,machine_name,my_controller);
	    sprintf(msg, "Alert: Cleanup Policy Nde purge not updating cleanup.log\n");
	    send_mail_to_user(sub_of_mail, msg, NULL);
	    MONITOR_TRACE_LOG1("Msg send for check_cleanup_policy_status", TL_ERROR, "Raising alert for Cleanup Policy Not Running");

	    send_mail_for_cleanup=1;

	    pthread_mutex_unlock(&thread_mutex);
	  }
	  else if(send_mail_for_cleanup == 1)
	  {
	    pthread_mutex_lock(&thread_mutex);

	    MONITOR_TRACE_LOG2("check_cleanup_policy_status", TL_INFO, "Alert cleared : Cleanup Policy is Running now");
	    sprintf(sub_of_mail,"Alert Cleared | %s | CSHM | %s | %s | Cleanup Policy Not Running", machine_mode,machine_name,my_controller);
	    sprintf(msg,"Cleanup Policy Nde purge updating cleanup.log");
	    send_mail_to_user(sub_of_mail, msg, NULL);
	    MONITOR_TRACE_LOG1("Msg send for check_cleanup_policy_status", TL_INFO, "Alert cleared : Cleanup Policy is Running now");

	    pthread_mutex_unlock(&thread_mutex);
	    send_mail_for_cleanup = 0;   
	  }
	} 
      }
    }
    sleep(86400);
  }
  return NULL;
}


void* check_unexpected_files_in_dir()
{
  FILE *fp;
  int i, total_files, total_files_prev[total_unexp_files_path], mail_buf_length = 0;
  long long int dir_size, dir_size_prev[total_unexp_files_path];
  int mail_buf_size = 16 * 1024;
  char buf[2048] = "", mail_buf[16*1024] = "";
  struct stat s;

  MONITOR_TRACE_LOG2("check_unexpected_files_in_dir", TL_INFO, "check_unexpected_files_in_dir thread started \n" );
  
  while(1)
  {
    for(i = 0; i < total_unexp_files_path; i++)
    {
      /* Check if dir exists */
      if(stat(dir_info[i].dir_path, &s) != 0)
      {
	MONITOR_TRACE_LOG1("check_unexpected_files_in_dir", TL_ERROR, "Dir '%s' does not exist\n", dir_info[i].dir_path);
	continue;
      }

      /* Get size of dir and number of files in dir */
      /* -S option is used with du to exclude size of subdirectories*/
      if(dir_info[i].include_sub_dir == 1)
	sprintf(buf, "du -sb %s; ls -A %s | wc -l", dir_info[i].dir_path, dir_info[i].dir_path);
      else
	sprintf(buf, "du -Ssb %s; ls -A %s | wc -l", dir_info[i].dir_path, dir_info[i].dir_path);

      if(run_cmd(&fp, buf) == -1)
      {
	MONITOR_TRACE_LOG1("check_unexpected_files_in_dir", TL_ERROR, "Unable to run command : '%s'\n", buf);
	sleep(300);
	continue;
      }

      /* Get size of dir */
      fgets(buf, 2048, fp);
      dir_size = atoll(buf);

      /* Get number of files in dir */
      fgets(buf, 2048, fp);
      total_files = atoi(buf); // total number of files in configured directory including hidden files

      if(fp)
	pclose(fp);

      if(dir_size > dir_info[i].dir_size_threshold)
      {
	pthread_mutex_lock(&thread_mutex);
	pthread_mutex_unlock(&thread_mutex);
      }


      if(total_files > dir_info[i].num_files_threshold)
      {
	pthread_mutex_lock(&thread_mutex);
	pthread_mutex_unlock(&thread_mutex);
      }

      /* check send_mail is enable and mail_buf_length must be less than buffer size */
      if((mail_buf_length < mail_buf_size))
      {
	/* Add size to buffer. This buffer will be sent in mail */
	if(dir_size > dir_info[i].dir_size_threshold && dir_size_prev[i] != dir_size)
	{
          dir_size_prev[i] = dir_size;
	  mail_buf_length += snprintf(mail_buf + mail_buf_length, mail_buf_size - mail_buf_length, "Size of '%s' dir : '%lld' bytes "
	      "exceeds threshold size : '%lld'\n", dir_info[i].dir_path,
	      dir_size, dir_info[i].dir_size_threshold);
	}

	/* Add total number of files in configured directory to buffer. This buffer will be sent in mail */
	if(total_files > dir_info[i].num_files_threshold && total_files_prev[i] != total_files)
	{
          total_files_prev[i] = total_files;
	  mail_buf_length += snprintf(mail_buf + mail_buf_length, mail_buf_size - mail_buf_length, "Number of files in '%s' dir : '%d' "
	      "exceeds threshold num files limit : '%d'\n", dir_info[i].dir_path,
	      total_files, dir_info[i].num_files_threshold);
	}
      }
    }

    /* if mail buffer contain some data in that case mail is sent */
    if(mail_buf[0] != '\0')
    {
      pthread_mutex_lock(&thread_mutex);
      sprintf(sub_of_mail,"Critical | %s | CSHM | %s | %s | Unwanted Files in Directory", machine_mode,machine_name,my_controller);
      snprintf(msg, MAX_READ_LINE_LENGTH, "Alert: Size and/or number of files in configured "
	  "directory is exceeding threshold values as below:\n%s", mail_buf);
      send_mail_to_user(sub_of_mail, msg, NULL);
      pthread_mutex_unlock(&thread_mutex);

      mail_buf[0] = '\0';
      mail_buf_length = 0;
    }
    sleep(unexpected_files_in_dir_delay_time);
  }
  return NULL;
}


void* check_ndp_dbu_progress()
{
  FILE *fp = NULL;
  char *fields[10];
  int i = 0, first_iteration = 1;
  int ret = 0, num_fields, mail_buf_length = 0;
  int empty_slot = -1;
  char command[1024] = "", buf[512] = "";
  int mail_buf_size = 16 * 1024;
  char mail_buf[16*1024] = "";
  long long int bytes_remaining;
  long int bytes_remaining_mb;
  int file_info_len = 0, file_info_max_len = 0;
  int bytes_remaining_threshold_mb = bytes_remaining_threshold/1048576 ; 

  FileInfo *file_info = NULL;

  sprintf(command, "%s/bin/neu_check_progress -n %d", ns_wdir, test_run_no);

  MONITOR_TRACE_LOG2("check_ndp_dbu_progress", TL_INFO, "check_ndp_dbu_progress thread started \n" );

  while(1)
  {
    if(is_test_running)
    {
      /* run shell which will give how much ndp or dbu is delayed in bytes*/
      ret = run_cmd(&fp, command);

      if(ret == -1)
      {
	MONITOR_TRACE_LOG1("check_ndp_dbu_progress", TL_ERROR, "Unable to run shell : '%s'\n", command);
	sleep(300);
	continue;
      }

      while(fgets(buf, 1024, fp) != NULL)
      {
	/* Skip headers */
	if(!strncasecmp(buf, "CSV NAME", 8) || !strncasecmp(buf, "RAW FILE NAME", 13))
	  continue;

	num_fields = get_tokens_with_multi_delimiter(buf, fields, ",", 10);

	if(num_fields != 4) 
	{
	  MONITOR_TRACE_LOG1("check_ndp_dbu_progress", TL_ERROR, "Total Fields:'%d' are less than 4\n", num_fields);
	  continue;
	}

	/* Get the value in bytes means how much ndp or dbu is running delayed*/
	bytes_remaining = atoll(fields[1]);
        
        bytes_remaining_mb = bytes_remaining/1048576;  

	/* if value is less than threshold, then continue */
	if(bytes_remaining < bytes_remaining_threshold)
	  continue;
	else
	{
	  pthread_mutex_lock(&thread_mutex);
	  pthread_mutex_unlock(&thread_mutex);
	}

	/*Get the file name and compare it Is in list? otherwise send it in mail*/
	for(i = 0; i < file_info_len; i++)
	{
	  if(strcmp(file_info[i].file_name, fields[0]) == 0)
	  {
	    file_info[i].file_flag = 1;
	    break;
	  }
	}

	/* Mail has already been sent for this file, so continue */
	if(i < file_info_len)
	  continue;

	/* Find empty slot in list for new file name */
	empty_slot = -1;
	for(i = 0; i < file_info_len; i++)
	{
	  if(file_info[i].file_name[0] == '\0')
	  {
	    empty_slot = i;
	    break;
	  }
	}

	if(empty_slot == -1 && file_info_len < file_info_max_len)
	  empty_slot = file_info_len;

	/* Realloc list of files, for which mail has already been sent */
	if(empty_slot == -1 && file_info_len == file_info_max_len)
	{
	  file_info_max_len += 10;
	  MY_REALLOC_AND_MEMSET(file_info, (sizeof(FileInfo) * file_info_max_len), (sizeof(FileInfo) * file_info_len), "FileInfo", -1);
	  empty_slot = file_info_len;
	}

	/* Get the file name and set the file flag to 1*/
	strcpy(file_info[empty_slot].file_name, fields[0]);
	file_info[empty_slot].file_flag = 1;

	if((mail_buf_length < mail_buf_size))
	{
	  mail_buf_length += snprintf(mail_buf + mail_buf_length, mail_buf_size - mail_buf_length, "FILE NAME: '%s', DELAY SIZE(in Mb): '%ld'\n", file_info[empty_slot].file_name, bytes_remaining_mb);
	}
	file_info_len++;
      }

      if(fp)
	pclose(fp);

      if(first_iteration)
      {
	for(i = 0; i < file_info_len; i++)
	{
	  if(file_info[i].file_flag == 0)
	    file_info[i].file_name[0] = '\0';
	  else
	    file_info[i].file_flag = 0; 
	}
      }

      first_iteration = 0;


      /* if mail buffer contain some data in that case mail is sent */
      if(mail_buf[0] != '\0' && send_mail == true)
      {
	pthread_mutex_lock(&thread_mutex);

	sprintf(sub_of_mail,"Critical | %s | CSHM | %s | %s | NDP/DBU Threads are Running Behind", machine_mode,machine_name,my_controller);
	snprintf(msg, MAX_READ_LINE_LENGTH, "Alert: Following NDP/DBU threads are lagging behind by more than the threshold of '%d' Mb.\n%s", bytes_remaining_threshold_mb, mail_buf);
        MONITOR_TRACE_LOG2("Msg send for check_ndp_dbu_progress", TL_INFO, "%s", msg);
	send_mail_to_user(sub_of_mail, msg, NULL);
	pthread_mutex_unlock(&thread_mutex);

	mail_buf[0] = '\0';
	mail_buf_length = 0;
      }
      else if(mail_buf[0] != '\0')
      {
        MONITOR_TRACE_LOG1("Msg not send for check_ndp_dbu_progress", TL_ERROR, "Alert: Following NDP/DBU threads are lagging behind by more than the threshold of '%d' Mb.\n%s", bytes_remaining_threshold_mb, mail_buf);
 
        mail_buf[0] = '\0';
        mail_buf_length = 0;
      }
    }
    sleep(ndp_dbu_progress_delay_time);
  }
  return NULL;
}

void* check_test_run_status()
{
  //TODO: Get Session number from config.ini. This alert is valid for NDE, NV, NO CMT only - look at the requirement document
  FILE *fp = NULL;
  int ret = 0;
  int test_run_status;
  int mail_send_flag = 0;
  char buff[64] = "", command[1024];
  int test_run_mail_send = 0;

  MONITOR_TRACE_LOG2("check_test_run_status", TL_INFO, "check_test_run_status thread started\n");

  while(1)
  {
    sprintf(command, "nsu_show_all_netstorm | grep -w %d 1>/dev/null 2>/dev/null; echo $?", test_run_no);

    ret = run_cmd(&fp, command);

    if(ret == -1)
    {
      MONITOR_TRACE_LOG1("check_test_run_status", TL_ERROR, "Unable to run command : '%s'\n", command);
      sleep(300);
      continue;
    }

    fgets(buff, 64, fp);
    test_run_status = atoi(buff);

    MONITOR_TRACE_LOG3("check_test_run_status", TL_INFO, "Session '%d' return code status = '%d' \n", test_run_no, test_run_status);

    if(fp)
      pclose(fp);

    if(test_run_status != 0)
    {
      MONITOR_TRACE_LOG2("check_test_run_status", TL_INFO, "Session '%d' is not running as the return code status = '%d' \n", test_run_no, test_run_status);
      is_test_running = 0;
      /* mail_send_flag 1 means mail must be send as test is not running*/
      if(mail_send_flag == 0)
	mail_send_flag = 1;
    }
    else if(test_run_status == 0)
    {
      MONITOR_TRACE_LOG2("check_test_run_status", TL_INFO, "Session '%d' is running as the return code status = '%d' \n", test_run_no, test_run_status);
      is_test_running = 1;
      /*Reset the mail_send_flag to 0 as test start or running*/
      if(mail_send_flag == 2)
	mail_send_flag = 0;
    }

    if(mail_send_flag == 1)
    {
      pthread_mutex_lock(&thread_mutex);
      sprintf(sub_of_mail,"Critical | %s | CSHM | %s | %s | Session is not running", machine_mode,machine_name,my_controller);
      sprintf(msg, "Session '%d' is not running.\n", test_run_no);
 
      if(send_mail)
      {
        MONITOR_TRACE_LOG2("Msg send for check_test_run_status", TL_INFO,"%s", msg);
        send_mail_to_user(sub_of_mail, msg, NULL);
      }
      else
      {
        MONITOR_TRACE_LOG1("Msg not send for check_test_run_status", TL_ERROR,"%s", msg);
      } 

      /*Set the mail_send_flag to 2 as mail must be send one time */
      mail_send_flag = 2;

      pthread_mutex_unlock(&thread_mutex);
      test_run_mail_send = 1;
    }
    else if(test_run_mail_send == 1 && test_run_status == 0)
    {
      pthread_mutex_lock(&thread_mutex);
      sprintf(sub_of_mail,"Alert Cleared | %s | CSHM | %s | %s | Session is not running", machine_mode,machine_name,my_controller);
      sprintf(msg, "Test Run Number '%d' is now running.\n", test_run_no);
      if(send_mail)
      {         
        MONITOR_TRACE_LOG2("Msg send for check_test_run_status", TL_INFO,"%s", msg);
        send_mail_to_user(sub_of_mail, msg, NULL);
      }
      else    
      {       
        MONITOR_TRACE_LOG1("Msg not send for check_test_run_status", TL_ERROR,"%s", msg);
      }

      pthread_mutex_unlock(&thread_mutex);
      test_run_mail_send = 0;
    }

    sleep(test_run_delay_time);
  }
  return NULL;
}

int check_testrun_gdf_file(char *base_dir, char *partition, int file_idx, int gdf_count, char *gdf_file_name)
{
  char tmp_file_name[128];
  char path[512];
  struct stat fstat;

  if(file_idx == 0)
    strcpy(tmp_file_name, "testrun.gdf");
  else
    sprintf(tmp_file_name, "testrun.gdf.%d", file_idx);

  snprintf(gdf_file_name, 128, "%s", tmp_file_name);
  snprintf(path, 512, "%s/%s/%s", base_dir, partition, tmp_file_name);

  while(stat(path, &fstat) == 0)
  {
    snprintf(gdf_file_name, 128, "%s", tmp_file_name);
    file_idx = file_idx + 1;
    sprintf(tmp_file_name, "testrun.gdf.%d", file_idx);
    snprintf(path, 512, "%s/%s/%s", base_dir, partition, tmp_file_name);
    gdf_count = gdf_count + 1;
  }
  return gdf_count;
}

void* check_rtg_packet_size()
{
  FILE *fp = NULL;
  long long int packet_size;
  char path[1024], buff[1024], cur_partition_name[16], prev_partition_name[16], next_partition_name[16];
  char base_dir[128], mail_buf[16*1024] = "";
  long long cur_partition_idx;
  char *fields[8];
  int total_fields = 0, ret = 0, mail_buf_length = 0;
  int mail_buf_size = 16 * 1024;
  int count, gdf_count;
  int file_idx = 0;
  char gdf_file_name[128]; //testrun gdf file name
  int thres_packet_size_mb = thres_packet_size/1048576; 
  int packet_size_mb=0;

  sprintf(base_dir, "%s/logs/TR%d", ns_wdir, test_run_no);

  /*get Current Partition*/
  cur_partition_idx = nslb_get_cur_partition(base_dir);
  sprintf(cur_partition_name, "%lld", cur_partition_idx);

  /*get previous partition*/
  ret = nslb_get_prev_partition(base_dir, cur_partition_name, prev_partition_name);

  /* If previous partition exists then copy it to cur_partition_name*/  
  if(ret == 0)
    sprintf(cur_partition_name, "%s", prev_partition_name);

  MONITOR_TRACE_LOG2("check_rtg_packet_size", TL_INFO, "check_rtg_packet_size thread started \n");   

  while(1)
  {
    while(1)
    { 
      if(is_test_running)
      {
	/*get next partition*/
	ret = nslb_get_next_partition(ns_wdir, test_run_no, cur_partition_name, next_partition_name);

	/*If next partition gets then break from loop otherwise sleep for configured time*/ 
	if(ret == 0)
	  break;
      }
      sleep(rtg_delay_time);
    }

    file_idx = 0;
    count = 0;

    /*function that will give count of total testrun gdf files in partition and also find the max testrun gdf file*/
    gdf_count = check_testrun_gdf_file(base_dir, cur_partition_name, file_idx, count, gdf_file_name);

    sprintf(path, "%s/%s/%s", base_dir, cur_partition_name, gdf_file_name);

    fp = fopen(path, "r");

    if(fp == NULL)
    {  

      MONITOR_TRACE_LOG1("check_rtg_packet_size", TL_ERROR, "Error in opening file = '%s' (Error: '%s')\n", path, nslb_strerror(errno));
      sleep(300);
      sprintf(cur_partition_name, "%s", next_partition_name);
      continue;
    }
    fgets(buff, 1024, fp);

    if(fp)
      fclose(fp);

    total_fields = get_tokens_with_multi_delimiter(buff, fields, "|", 8);

    if(total_fields < 8)
    {
      MONITOR_TRACE_LOG1("check_rtg_packet_size", TL_ERROR, "Total Fields:'%d' are less than 8\n", total_fields);
      sleep(300);
      sprintf(cur_partition_name, "%s", next_partition_name);
      continue;
    }

    packet_size = atoll(fields[5]);

    if(packet_size > thres_packet_size)
    {
      pthread_mutex_lock(&thread_mutex);
      pthread_mutex_unlock(&thread_mutex);
    }


    if(gdf_count > gdf_threshold_count)
    {
      pthread_mutex_lock(&thread_mutex);
      pthread_mutex_unlock(&thread_mutex);
    }

    /* check send_mail is enable and mail_buf_length must be less than buffer size */
    if((mail_buf_length < mail_buf_size))
    {
      /* add total count of gdf files to buffer. this buffer will be sent in mail */
      if(gdf_count > gdf_threshold_count)
      {
	mail_buf_length += snprintf(mail_buf + mail_buf_length, mail_buf_size - mail_buf_length, "Number of testrun.gdf files are '%d' " 
	    "exceeds threshold gdf file count '%d' for partition '%s'\n", gdf_count, gdf_threshold_count, 
	    cur_partition_name);
      }

      /* add packet size to buffer. this buffer will be sent in mail */
      if(packet_size > thres_packet_size)
      {
        packet_size_mb = packet_size/1048576;
	mail_buf_length += snprintf(mail_buf + mail_buf_length, mail_buf_size - mail_buf_length, "Packet size '%d' Mb exceeds threshold "                                     "packet size '%d' Mb\n", packet_size_mb, thres_packet_size_mb);
      }
    }

    /* if mail buffer contain some data in that case mail is sent */
    if(mail_buf[0] != '\0')
    {
      pthread_mutex_lock(&thread_mutex);

      sprintf(sub_of_mail,"Critical | %s | CSHM | %s | %s | RTG Packet Size or GDF Version Count has Crossed Threshold", machine_mode,machine_name,my_controller);
      snprintf(msg, MAX_READ_LINE_LENGTH, "Number of testrun.gdf files or packet size of testrun.gdf file "
	  "exceeding threshold values as below:\n\n%s", mail_buf);

      if(send_mail)
      {
        MONITOR_TRACE_LOG2("Msg send for check_rtg_packet_size", TL_INFO, "%s", msg);
        send_mail_to_user(sub_of_mail, msg, NULL);      
      }
      else
      {
        MONITOR_TRACE_LOG1("Msg not send for check_rtg_packet_size", TL_ERROR, "%s", msg);
      }

      pthread_mutex_unlock(&thread_mutex);

      mail_buf[0] = '\0';
      mail_buf_length = 0;
    }
    sprintf(cur_partition_name, "%s", next_partition_name);
  }
  return NULL;
}


int filter_csv_file(const struct dirent *file)
{
  if((nslb_get_file_type(".", file) == DT_REG) && strstr(file->d_name, ".csv"))
    return 1;

  return 0;
}

void* check_metadata_file_size()
{
  int i,mail_buf_length = 0,  num_files = -1;
  char path_buffer[1024] = "";
  char abs_csv_path[1024] = "";
  char mail_buff[MAX_MAIL_BUFFER_SIZE] = "";
  struct stat fstat;
  struct dirent **flist = NULL;
  long long int last_mail_send = 0;
  DIR *dirp;
  snprintf(path_buffer, 1024, "%s/logs/TR%d/nd/csv/", ns_wdir, test_run_no);
  dirp = opendir(path_buffer);
  MONITOR_TRACE_LOG2("check_metadata_file_size", TL_INFO, "check_metadata_file_size thread started\n");

  while(1)
  {
    if(is_test_running)
    {
    if((num_files = scandir(path_buffer, &flist, filter_csv_file, NULL)) == -1){
      MONITOR_TRACE_LOG1("check_metadata_file_size", TL_ERROR, "Directory Error :'%s'\n", path_buffer);
    }
    else
    {
      for(i = 0; i<num_files; i++)
      {
	snprintf(abs_csv_path, 1024, "%s/%s", path_buffer, flist[i]->d_name);      
	if(stat(abs_csv_path, &fstat) == 0)
	{
	  if(fstat.st_size > max_file_size)   
	    if(mail_buf_length < MAX_MAIL_BUFFER_SIZE) 
	      mail_buf_length += snprintf(mail_buff + mail_buf_length, MAX_MAIL_BUFFER_SIZE - mail_buf_length, "%s\n", flist[i]->d_name);           
	}
      }

      /* if mail buffer contain some data in that case mail is sent */
      if(mail_buff[0] != '\0' && ( last_mail_send == 0 || (last_mail_send > metadata_max_mail_time) ))
      {
	pthread_mutex_lock(&thread_mutex);
        MONITOR_TRACE_LOG2("check_metadata_file_size", TL_INFO, "Alert: Below files Size exceed threshold values :\n%s", mail_buff);
	sprintf(sub_of_mail,"Critical | %s | CSHM | %s | %s | Metadata File Crossed The Threshold", machine_mode,machine_name,my_controller);
	snprintf(msg, MAX_READ_LINE_LENGTH, "Alert: Below files Size exceed threshold values :\n%s", mail_buff);

        if(send_mail)
        {
	  MONITOR_TRACE_LOG2("Msg send for check_metadata_file_size", TL_INFO, "%s",msg);
	  send_mail_to_user(sub_of_mail, msg, NULL);
        }
        else
        {
          MONITOR_TRACE_LOG1("Msg not send for check_metadata_file_size", TL_ERROR, "%s",msg);
        }

	pthread_mutex_unlock(&thread_mutex);
	last_mail_send = 0;
      }
    }

    mail_buff[0] = '\0';
    mail_buf_length = 0;
    last_mail_send = last_mail_send + max_sleep_time;
    }
    sleep(max_sleep_time);
  }
  closedir(dirp);
  return NULL;   
}

void* db_dup_check_file()
{
  int mail_buf_length = 0 ;
  char ch, path_buffer[1024] = "", textBuff[1024] = "";
  char mail_buff[MAX_MAIL_BUFFER_SIZE] = "";
  FILE *fd = NULL;
  int count = 0;
  long off = 0;
  long long int last_mail_send = 0;

  snprintf(path_buffer, 1024, "%s/logs/TR%d/db_upload_error_log", ns_wdir, test_run_no);           // File location

  MONITOR_TRACE_LOG2("check_db_dup_file", TL_INFO, "check_db_dup_file thread started \n");

  while(1)
  { 
    if(is_test_running)
    { 
      if( (fd = fopen(path_buffer, "r")) == NULL )
      {
        MONITOR_TRACE_LOG1("check_db_dup_file", TL_ERROR, "FIle not found Error :'%s'\n", path_buffer);
      }
      else
      {
      // MONITOR_TRACE_LOG1("check_db_dup_file", TL_ERROR, "affset'%d'\n", fd->_offset);
        fseek(fd, -1, SEEK_END);

        off = ftell(fd);

        while(off != -1)
        {
	  fseek(fd, off, SEEK_SET);

	  ch = fgetc(fd);

	  if(ch == '\n')
	    count++;

	  if(count == 10)
	    break;

	  off-=1;
        }

        while (fgets(textBuff, 1024, fd))
        {
	  if(mail_buf_length < MAX_MAIL_BUFFER_SIZE)
	    mail_buf_length += snprintf(mail_buff + mail_buf_length, MAX_MAIL_BUFFER_SIZE - mail_buf_length, "%s",textBuff );        
        }

        fclose(fd);
      }

      if(mail_buff[0] != '\0' && ( last_mail_send == 0 || (last_mail_send > db_dup_mail_time) ))
      {
        pthread_mutex_lock(&thread_mutex);

        MONITOR_TRACE_LOG2("db_dup_check_file", TL_INFO, "Alert: Below are the duplicate metadata entries refused by DB upload :\n%s", mail_buff);
        sprintf(sub_of_mail,"Critical | %s | CSHM | %s | %s | Duplicate Or Error Message Found", machine_mode,machine_name,my_controller);
        snprintf(msg, MAX_READ_LINE_LENGTH, "Alert: Below are the duplicate metadata entries refused by DB upload :\n%s", mail_buff);
     
        if(send_mail)
        {
          MONITOR_TRACE_LOG2("Msg send for db_dup_check_file", TL_INFO, "%s",msg);
          send_mail_to_user(sub_of_mail, msg, NULL); 
        }
        else
        {
          MONITOR_TRACE_LOG1("Msg not send for db_dup_check_file", TL_ERROR, "%s",msg);
        }

        pthread_mutex_unlock(&thread_mutex);
        last_mail_send = 0;
      } 

      count = 0;  
      mail_buff[0] = '\0';
      mail_buf_length = 0;
      last_mail_send = last_mail_send + db_dup_sleep_time;
    }
    sleep(db_dup_sleep_time);
  }
  return NULL; 
}



void* check_missing_constraint()
{
  FILE *fp = NULL;
  long long cur_partition_idx, tmp_partition = 0;
  char base_dir[128], cur_partition_name[16], prev_partition_name[16], command[128], buff[128*1024]= "", line_buf[1024];
  int ret, partition_switch_flag = 0;

  MONITOR_TRACE_LOG2("check_missing_constraint", TL_INFO, "check_missing_constraint thread started\n");

  sprintf(base_dir, "%s/logs/TR%d/", ns_wdir, test_run_no);
  cur_partition_idx = nslb_get_cur_partition(base_dir);
  if(cur_partition_idx < 0)
  {
    MONITOR_TRACE_LOG1("check_missing_constraint", TL_ERROR, "current partition does not exist\n");
    return NULL;
  }
  sprintf(cur_partition_name, "%lld", cur_partition_idx);

  MONITOR_TRACE_LOG2("check_missing_constraint", TL_INFO, "Curr partition: '%lld'\n", cur_partition_idx);

  ret = nslb_get_prev_partition(base_dir, cur_partition_name, prev_partition_name);
  if(ret == 0)
  {
    //Start from previous partition
    sprintf(cur_partition_name, "%s", prev_partition_name);
  }
  else if(ret < 0) 
  {
    MONITOR_TRACE_LOG1("check_missing_constraint", TL_ERROR, "Previous partition does not exist, %s is the current partition\n", cur_partition_name);
  }


  while(1)
  {
    if(is_test_running)
    {
      MONITOR_TRACE_LOG2("check_missing_constraint", TL_INFO, "Checking for partiton: '%s' , cur partition: '%lld' \n", cur_partition_name, tmp_partition);
        
      sprintf(command, "%s/bin/neu_get_missing_constraints -n %d -p %s", ns_wdir, test_run_no, cur_partition_name);
      ret = run_cmd(&fp, command);
      if(ret == -1)
      {
	MONITOR_TRACE_LOG1("check_missing_constraint", TL_ERROR, "Unable to run shell : '%s'\n", command);
	sleep(300);
	continue;
      }

      while(fgets(line_buf, 1024, fp) != NULL)
      {
	if(strncasecmp(line_buf, "No Missing Constraints", 22) == 0)
	  continue;
	else
	  strcat(buff, line_buf);
      }

      if(fp)
	pclose(fp);

      if(buff[0] != '\0')
      {
        if(send_mail)
        {
	  pthread_mutex_lock(&thread_mutex);

            MONITOR_TRACE_LOG2("check_missing_constraint", TL_INFO, "Attention: Missing Constraints are \n\n%s\n\n", buff);
	    sprintf(sub_of_mail,"Critical | %s | CSHM | %s | %s | Missing Constraints Found", machine_mode,machine_name,my_controller); 
	    snprintf(msg, MAX_READ_LINE_LENGTH, "Attention: Missing Constraints are \n\n%s\n\n", buff);
            MONITOR_TRACE_LOG2("Msg send for check_missing_constraint", TL_INFO, "%s\n", msg);
	    send_mail_to_user(sub_of_mail, msg, NULL);
   
	  pthread_mutex_unlock(&thread_mutex);
	  buff[0] = '\0';
        }
        else
        {
          MONITOR_TRACE_LOG1("Msg not send for check_missing_constraint", TL_ERROR, "Attention: Missing Constraints are \n%s\n\n", buff);
	  buff[0] = '\0';
        }
      }

      /*if partition_switch_flag is 1 means parition is switched so we need to update cur_partition_idx */ 
      if(partition_switch_flag)
      {
	cur_partition_idx = tmp_partition;
	partition_switch_flag = 0;
      }

      while((tmp_partition = nslb_get_cur_partition(base_dir)) <= cur_partition_idx)
      {
	if(tmp_partition < 0)
	{
	  MONITOR_TRACE_LOG1("check_missing_constraint", TL_ERROR, "Error in getting current partition.");
	}
	sleep(missing_constraint_delay_time);
      }

      //cur_partition_idx = tmp_partition;
      sprintf(cur_partition_name, "%lld", cur_partition_idx);
      partition_switch_flag = 1;
    }
    sleep(900);
  }
  return NULL;
}

//reading productUI csv file for cpu, load average and available memory
void* read_product_ui_csv_file()
{
  FILE *csvfile = NULL;
  FILE *fp = NULL;
  char command[1024];
  char y_m_d[30] = "date +%Y%m%d";
  char epoch_cmd[30] = "date +%s%3N"; 
  char top_cmd[50] = "top -b -o +%CPU|head -n 12"; //changes for bug 86783
  char iotop_process_cmd[50] = "sudo iotop -btoP -n 5 -d 1";
  char iotop_thread_cmd[50] = "sudo iotop -bto -n 5 -d 1";
  char buff[1024];
  char read_buff[1024];
  char readbuff[1024];
  int num;
  int ret;
  long int current_epoch;
  int diff_of_epoch;
  long int epoch_time;
  float usr_cpu;
  float sys_cpu;
  float nice_val;
  float idle_cpu;
  float IO_wait;
  float harware_interuppt;
  float software_interuppt;
  float steal_time;
  long int total_memory;
  long int used_memory;
  long int free_memory;
  long int buffer_memory;
  long int total_swap_memory;
  long int used_swap_memory;
  long int free_swap_memory;
  long int cached_memory;
  float load_avg_over_1min;
  float load_avg_over_5min;
  float load_avg_over_15min;
  bool cpu_mail_send = false;
  int load_avg_mail_send = 0;
  int date;
  long int cache_memory;
  long int cache_memory_mb=0;
  int cache_memory_pct;
  struct stat fcsv_path;
  int csv_not_running_mail_send = 0;
  bool csv_not_updating_mail_send = false;
  int msg_len = 0;


  MONITOR_TRACE_LOG2("read_product_ui_csv_file", TL_INFO, "read product UI CSV thread started \n");
  while(1)
  {
    sprintf(command, "%s", y_m_d);

    ret = run_cmd(&fp, command);

    if(ret == -1)
    {
      MONITOR_TRACE_LOG1("read_product_ui_csv_file", TL_ERROR, "Unable to run command : '%s'\n", command);
      sleep(300);
      continue;
    }
    else
    { 
      fgets(buff, 1024, fp);
      date = atoi(buff);

      if(fp)
	pclose(fp);

      snprintf(csv_path, 512, "/home/cavisson/ProductUIData/ApplianceData/productData_%d.csv",date);

      if(stat(csv_path, &fcsv_path))
      {	
	if(csv_not_running_mail_send == 1)
	{
	  pthread_mutex_lock(&thread_mutex);

	  MONITOR_TRACE_LOG1("read_product_ui_csv_file", TL_ERROR, "System Health Data Collector is not running as the csv file '[%s]' does not exist \n",csv_path);
	  sprintf(sub_of_mail,"Critical | %s | CSHM | %s | %s | System Health Data Collector Script Not Running", machine_mode,machine_name,my_controller);
	  snprintf(msg, MAX_READ_LINE_LENGTH, "System Health Data Collector is not running as the CSV file [%s] does not exist.\n", csv_path);
  
          if(send_mail) 
          {
	    MONITOR_TRACE_LOG2("Msg send for read_product_ui_csv_file", TL_INFO, "%s",msg);
	    send_mail_to_user(sub_of_mail, msg, NULL);
          }
          else
          {  
	    MONITOR_TRACE_LOG1("Msg not send for read_product_ui_csv_file", TL_ERROR, "%s", msg);
          }    
	  csv_not_running_mail_send = 2;

	  pthread_mutex_unlock(&thread_mutex);
	}
	else if(csv_not_running_mail_send == 0)
	{
          csv_not_running_mail_send = 1;
	  MONITOR_TRACE_LOG2("read_product_ui_csv_file", TL_INFO, "System Health Data Collector is not running has been noticed Ist time \n");
	}
        else
        {
           MONITOR_TRACE_LOG2("read_product_ui_csv_file", TL_INFO, "System Health Data Collector is not running has been notified \n");
        }
      }
      else
      {
	csv_not_running_mail_send = 0;
	MONITOR_TRACE_LOG2("read_product_ui_csv_file", TL_INFO, "csv file [%s] exists\n",csv_path);
	csvfile = fopen(csv_path, "r");

	if(csvfile == NULL)
	{  
	  MONITOR_TRACE_LOG1("read_product_ui_csv_file", TL_ERROR, "Error in opening file = '%s' (Error: '%s')\n", csv_path, nslb_strerror(errno));                          
	  sleep(300);
	  continue;
	}
	else
	{
	  while(!feof(csvfile))
	    fgets(read_buff, MAX_READ_LINE_LENGTH, csvfile);

          fclose(csvfile);

	  if((num = sscanf(read_buff, "%ld, %f, %f, %f, %f, %f, %f, %f, %f, %ld, %ld, %ld, %ld, %ld, %ld, %ld, %ld, %f, %f, %f", &epoch_time, &usr_cpu, &sys_cpu, &nice_val, &idle_cpu, &IO_wait, &harware_interuppt, &software_interuppt, &steal_time, &total_memory, &used_memory, &free_memory, &buffer_memory, &total_swap_memory, &used_swap_memory, &free_swap_memory, &cached_memory, &load_avg_over_1min, &load_avg_over_5min, &load_avg_over_15min)) < 20)
	  {
	    MONITOR_TRACE_LOG1("read_product_ui_csv_file", TL_ERROR, "Less no. of data at epoch time  = %ld,  text = %s\n" , epoch_time, read_buff);
	  }
	  else  
	  {       

	    //take out present epoch time  
	    sprintf(command, "%s", epoch_cmd);

	    ret = run_cmd(&fp, command);

	    if(ret == -1)
	    {
	      MONITOR_TRACE_LOG1("read_product_ui_csv_file", TL_ERROR, "Unable to run command : '%s'\n", command);
	      sleep(300);
	      continue;
	    }
	    else
	    {
	      fgets(buff, 1024, fp);

	      if(fp)
		pclose(fp);  

	      current_epoch = atol(buff); 

	      diff_of_epoch = current_epoch - epoch_time;

	      MONITOR_TRACE_LOG2("read_product_ui_csv_file", TL_INFO, "current epoch = %ld ,epoch time = %ld \n", current_epoch, epoch_time);

	      if( diff_of_epoch > 121000 && csv_not_updating_mail_send != true)
	      {
		pthread_mutex_lock(&thread_mutex);

		MONITOR_TRACE_LOG2("read_product_ui_csv_file", TL_ERROR, "System Health Data Collector is not updating system stats as the last epoch time difference is more two minutes\n");
		sprintf(sub_of_mail,"Critical | %s | CSHM | %s | %s | System Health Data Collector Script Not Updating Stats", machine_mode,machine_name,my_controller);
		snprintf(msg, MAX_READ_LINE_LENGTH,"System Health Data Collector is not updating system stats. File [%s] has not been updated in the last two minutes\n", csv_path);
		MONITOR_TRACE_LOG1("Msg send for read_product_ui_csv_file", TL_INFO, "%s",msg);
		send_mail_to_user(sub_of_mail, msg, NULL);

		csv_not_updating_mail_send = true;

		pthread_mutex_unlock(&thread_mutex);
	      } 
	      else
	      {  
		csv_not_updating_mail_send = false;

		if( idle_cpu < idle_cpu_threshold )
		{
		  sprintf(command, "%s",top_cmd);

		  ret = run_cmd(&fp, command);

		  if(ret == -1)
		  {
		    MONITOR_TRACE_LOG1("read_product_ui_csv_file", TL_ERROR, "Unable to run command : '%s'\n", command);
		  }
		  else
		  {
		    MONITOR_TRACE_LOG2("read_product_ui_csv_file", TL_ERROR, "Idle cpu=%f, Threshold cpu=%d. Raising Alert \n", idle_cpu, idle_cpu_threshold);
		    pthread_mutex_lock(&thread_mutex);

		    sprintf(sub_of_mail,"Critical | %s | CSHM | %s | %s | CPU Usage Crossed Threshold", machine_mode,machine_name,my_controller);

		    msg_len = 0; 

		    msg_len = sprintf(msg,"CPU usage has crossed the threshold (%d%%).\n\nFollowing are the top five processes consuming the cpu : \n\n", (100-idle_cpu_threshold) );

		    while(fgets(readbuff, 1024, fp) != NULL)
		    {
		      msg_len += snprintf(msg+msg_len, MAX_READ_LINE_LENGTH  - msg_len, "%s", readbuff);
		    }

                    if(send_mail)
                    {
		      MONITOR_TRACE_LOG2("Msg send for read_product_ui_csv_file", TL_INFO, "%s", msg);  
                      send_mail_to_user(sub_of_mail, msg, NULL);
                    }
                    else
                    {
		      MONITOR_TRACE_LOG1("Msg not send for read_product_ui_csv_file", TL_INFO, "%s", msg);  
                    }

		    pthread_mutex_unlock(&thread_mutex);
		    cpu_mail_send=true;
		    if(fp)
		      pclose(fp);
		  }  
		}
		else if(cpu_mail_send)
		{ 
		  MONITOR_TRACE_LOG2("read_product_ui_csv_file", TL_INFO, "Cpu usage is now normal as per the threshold cpu \n");
		  pthread_mutex_lock(&thread_mutex);

		  sprintf(sub_of_mail,"Alert Cleared | %s | CSHM | %s | %s | CPU Usage Crossed Threshold", machine_mode,machine_name,my_controller);
		  sprintf(msg,"Cpu usage is now below the threshold (%d%%).\n", idle_cpu_threshold);
		  MONITOR_TRACE_LOG2("Msg send for read_product_ui_csv_file", TL_INFO,"%s", msg);
		  send_mail_to_user(sub_of_mail, msg, NULL);

		  pthread_mutex_unlock(&thread_mutex);
		  cpu_mail_send = false;
		}   

		if( load_avg_over_5min > load_avg_over_5min_threshold )
		{ 
                  if( load_avg_mail_send == 0)
                  {

                    if(show_iotop_thread_output)
                      sprintf(command, "%s",iotop_thread_cmd);
                    else
                      sprintf(command, "%s",iotop_process_cmd);
                   
                    if(show_iotop_output)
                      ret = run_cmd(&fp, command);
                    else
                      ret = 1;
                  
                    if(ret == -1)
                    {
                      MONITOR_TRACE_LOG1("read_product_ui_csv_file", TL_ERROR, "Unable to run command : '%s'\n", command);
                    }
                    else
                    {
                  
		      MONITOR_TRACE_LOG2("read_product_ui_csv_file", TL_INFO, "Load average=%.2f, Threshold load average=%d. Raising Alert \n", load_avg_over_5min, load_avg_over_5min_threshold);

		      pthread_mutex_lock(&thread_mutex);
                  
                      sprintf(sub_of_mail,"Critical | %s | CSHM | %s | %s | Load Average Crossed Threshold", machine_mode,machine_name,my_controller);

                      if(!show_iotop_output)
		         sprintf(msg, "Load averge (%.2f) has exceeded the threshold (%d).\n", load_avg_over_5min, load_avg_over_5min_threshold);
                      else
                      {
                        msg_len = 0;
                  
                        msg_len = sprintf(msg,"Load averge (%.2f) has exceeded the threshold (%d).\n\nFollowing is the output of iotop command : \n\n", load_avg_over_5min, load_avg_over_5min_threshold);
                  
                        while(fgets(readbuff, 1024, fp) != NULL)
                        {
                          msg_len += snprintf(msg + msg_len, MAX_READ_LINE_LENGTH  - msg_len,"%s", readbuff);
                        }
                      }

                      if(send_mail)
                      {
		        MONITOR_TRACE_LOG2("Msg send for read_product_ui_csv_file", TL_INFO, "%s", msg);
                        send_mail_to_user(sub_of_mail, msg, NULL);
                      }
                      else
                      {
		        MONITOR_TRACE_LOG1("Msg not send for read_product_ui_csv_file", TL_INFO, "%s", msg);
                      }
                  
		      pthread_mutex_unlock(&thread_mutex);

		      load_avg_mail_send++;

                      if(show_iotop_output)
                      {
                        if(fp)
                        pclose(fp);
                      }
                    }
                  }
                  else if(load_avg_mail_send < iotop_log_interval)
                    load_avg_mail_send++;

                  if(load_avg_mail_send == iotop_log_interval)
                    load_avg_mail_send=0;

		}
		else if(load_avg_mail_send != 0)
		{
		  pthread_mutex_lock(&thread_mutex);

		  MONITOR_TRACE_LOG2("read_product_ui_csv_file", TL_INFO, "Load avg is  now normal\n");
		  sprintf(sub_of_mail,"Alert Cleared | %s | CSHM | %s | %s | Load Average Crossed Threshold", machine_mode,machine_name,my_controller);
		  sprintf(msg,"Load avg is now below the threshold (%d).\n", load_avg_over_5min_threshold);
		  MONITOR_TRACE_LOG2("Msg send for read_product_ui_csv_file", TL_INFO, "%s", msg);
		  send_mail_to_user(sub_of_mail, msg, NULL);

		  pthread_mutex_unlock(&thread_mutex);
		  load_avg_mail_send = 0;
		}
     
	        sprintf(command, "free -k | grep Mem | tr -s ' ' | cut -d ' ' -f6");

	        ret = run_cmd(&fp, command);

	        if(ret == -1)
	        {
	          MONITOR_TRACE_LOG1("read_product_ui_csv_file", TL_ERROR, "Unable to run command : '%s'\n", command);
	          sleep(300);
	          continue;
	        }
	        else
	        {
	          fgets(buff, 1024, fp);
     
	          cache_memory = atol(buff);

	          if(fp)
	            pclose(fp);

	          cache_memory_pct = (cache_memory * 100) / total_memory;

	          MONITOR_TRACE_LOG2("read_product_ui_csv_file", TL_INFO, "Cache memory pct=%d, Cache memory=%ld Threshold cache memory=%d\n", cache_memory_pct, cache_memory, cache_memory_threshold);

	          if( cache_memory_pct > cache_memory_threshold )
	          {

                    cache_memory_mb = cache_memory/1024;               

	            MONITOR_TRACE_LOG2("read_product_ui_csv_file", TL_INFO, "Cache memory=%d, Threshold cache memory=%d. Raising Alert \n", cache_memory_pct, cache_memory_threshold);
    		    sprintf(command, "sudo /home/cavisson/bin/cav_service free_cache");

                    //Before we were using popen to run command but changed to system for running the command 
              
	            ret = system(command);       

	            if(ret == 1)
	            {
	              pthread_mutex_lock(&thread_mutex);

	    	      MONITOR_TRACE_LOG2("read_product_ui_csv_file", TL_INFO, "Unable to run free cache command to free cache memory as cache memory=%d and threshold cache memory=%d. Raising Alert \n", cache_memory, cache_memory_threshold);
		      sprintf(sub_of_mail,"Critical | %s | CSHM | %s | %s | Cache Memory Crossed Threshold", machine_mode,machine_name,my_controller);
		      sprintf(msg, "Cache memory is %d%% (%ld MB) while threshold is %d%%.\nUnable to run free cache command.\n", cache_memory_pct, cache_memory_mb, cache_memory_threshold);
		      MONITOR_TRACE_LOG2("Msg send for read_product_ui_csv_file", TL_INFO, "%s", msg);
		      send_mail_to_user(sub_of_mail, msg, NULL);

		      pthread_mutex_unlock(&thread_mutex);
    	            }
	            else if(ret == 0)
	            { 
                      sprintf(command, "free -lm");

                      ret = run_cmd(&fp, command);

                      if(ret == -1)
                      {
                        MONITOR_TRACE_LOG1("read_product_ui_csv_file", TL_ERROR, "Unable to run command : '%s'\n", command);
		        pthread_mutex_lock(&thread_mutex);

		        MONITOR_TRACE_LOG2("read_product_ui_csv_file", TL_INFO, "Able to run free cache cammand : '%s' \n", command);
		        sprintf(sub_of_mail,"INFO | %s | CSHM | %s | %s | Memory Usage Crossed Threshold", machine_mode,machine_name,my_controller);
		        sprintf(msg, "Cache memory is %d%% (%ld MB) while threshold is %d%%. Successfully executed the free cache command.\n", cache_memory_pct, cache_memory_mb, cache_memory_threshold);
		        MONITOR_TRACE_LOG2("Msg send for read_product_ui_csv_file", TL_INFO, "%s", msg);
		        send_mail_to_user(sub_of_mail, msg, NULL);

		        pthread_mutex_unlock(&thread_mutex);
                        sleep(300);
                        continue;
                      }
                      else
                      {
                        pthread_mutex_lock(&thread_mutex);

                        sprintf(sub_of_mail,"INFO | %s | CSHM | %s | %s | Memory Usage Crossed Threshold", machine_mode,machine_name,my_controller);

                        msg_len = 0;

                        msg_len = sprintf(msg,"Cache memory is %d%% (%ld MB) while threshold is %d%%. Successfully executed the free cache command.\n\nFollowing are the stats (free -lm) after executing the free cache command:\n\n", cache_memory_pct, cache_memory_mb, cache_memory_threshold);

                        while(fgets(readbuff, 1024, fp) != NULL)
                        {
                          msg_len += snprintf(msg+msg_len, MAX_READ_LINE_LENGTH  - msg_len, "%s", readbuff);
                        }
            
	                if(fp)
	                  pclose(fp);
                         
                        if(send_mail)
                        {
                          MONITOR_TRACE_LOG2("Msg send for read_product_ui_csv_file", TL_INFO, "%s", msg);
                          send_mail_to_user(sub_of_mail, msg, NULL);
                        }
                        else
                        {
                          MONITOR_TRACE_LOG1("Msg not send for read_product_ui_csv_file", TL_INFO, "%s", msg);
                        }

                        pthread_mutex_unlock(&thread_mutex);
                      }
	            }
	          }
	        }
              }
            }
	  }
	}
      }
    }
    sleep(system_level_monitor_sleep_time); 
  }
  return NULL;
}

void* run_and_read_ulimit_command()
{
  FILE *fp = NULL;
  char command[1024]="ulimit -a | awk '{print $1, $2, $NF}'";
  char first_name[MAX_READ_LINE_LENGTH];
  char second_name[MAX_READ_LINE_LENGTH];
  char value[MAX_READ_LINE_LENGTH] = "";
  char buff[1024];
  int ret;
  int int_value;
  int core_ulimit_mail_send = 0;
  int data_ulimit_mail_send = 0;
  int file_size_ulimit_mail_send = 0;
  int max_mem_ulimit_mail_send = 0;
  int cpu_ulimit_mail_send = 0;
  int virtual_ulimit_mail_send = 0;
  int file_locks_ulimit_mail_send = 0;
  int open_ulimit_mail_send = 0;
  int stack_ulimit_mail_send = 0;

  MONITOR_TRACE_LOG2("run_and_read_ulimit_command", TL_INFO, "ulimit function thread started \n");

  while(1)
  {
    ret = run_cmd(&fp, command);

    if(ret == -1)
    {   
      MONITOR_TRACE_LOG1("run_and_read_ulimit_command", TL_ERROR, "Unable to run command : '%s'\n", command);
      sleep(300);
      continue;
    }
    else
    {  
      while(fgets(buff, 1024, fp)!=NULL)
      { 
	sscanf(buff, "%s %s %s", first_name, second_name, value);

	if(!strcmp(first_name, "core"))
	{
	  if(!strcmp(value, "unlimited"))
	  {
	    if(core_ulimit_mail_send == 1)
	    { 
	      pthread_mutex_lock(&thread_mutex);

	      MONITOR_TRACE_LOG2("run_and_read_ulimit_command", TL_INFO,"core file size is now set to unlimited");
	      sprintf(sub_of_mail, "Alert Cleared | %s | CSHM | %s | %s | Core File Size Not Set To Unlimited", machine_mode,machine_name,my_controller);
	      sprintf(msg,"core file size is now set to unlimited");
	      MONITOR_TRACE_LOG2("Msg send for run_and_read_ulimit_command", TL_INFO,"core file size is now set to unlimited");
	      send_mail_to_user(sub_of_mail, msg, NULL);

	      pthread_mutex_unlock(&thread_mutex);
	      core_ulimit_mail_send = 0;
	    }
	  }
	  else if(core_ulimit_mail_send == 0)
	  {
	    //send core not unlimited send alert
	    pthread_mutex_lock(&thread_mutex);

	    MONITOR_TRACE_LOG2("run_and_read_ulimit_command", TL_ERROR,"core file size is set to %s but it should be unlimited",value);
	    sprintf(sub_of_mail, "Critical | %s | CSHM | %s | %s | Core File Size Not Set To Unlimited", machine_mode,machine_name,my_controller);
	    snprintf(msg, MAX_READ_LINE_LENGTH,"core file size is not set to unlimited, it is set to %s", value);
	    MONITOR_TRACE_LOG2("Msg send for run_and_read_ulimit_command", TL_INFO,"core file size is set to %s but it should be unlimited",value);
	    send_mail_to_user(sub_of_mail, msg, NULL);

	    pthread_mutex_unlock(&thread_mutex);
	    core_ulimit_mail_send = 1;
	  }  
	}
	else if(!strcmp(first_name, "data"))
	{
	  if(!strcmp(value, "unlimited"))
	  {
	    if(data_ulimit_mail_send == 1)
	    {
	      pthread_mutex_lock(&thread_mutex);

	      MONITOR_TRACE_LOG2("run_and_read_ulimit_command", TL_INFO,"data seg size is now set to unlimited");
	      sprintf(sub_of_mail, "Alert Cleared | %s | CSHM | %s | %s | Data Seg Size Set To Unlimited", machine_mode,machine_name,my_controller);
	      sprintf(msg,"data seg size is now set to unlimited");
	      MONITOR_TRACE_LOG2("Msg send for run_and_read_ulimit_command", TL_INFO,"data seg size is now set to unlimited");
	      send_mail_to_user(sub_of_mail, msg, NULL);

	      pthread_mutex_unlock(&thread_mutex);
	      data_ulimit_mail_send = 0;
	    }
	  }
	  else if(data_ulimit_mail_send == 0)
	  {
	    //send data not unlimited send alert
	    pthread_mutex_lock(&thread_mutex);

	    MONITOR_TRACE_LOG2("run_and_read_ulimit_command", TL_INFO,"data seg size is set to %s but it should be unlimited",value);
	    sprintf(sub_of_mail, "Critical | %s | CSHM | %s | %s | Data Seg Size Not Set To Unlimited", machine_mode,machine_name,my_controller);
	    snprintf(msg, MAX_READ_LINE_LENGTH,"data seg size is not set to unlimited, it is set to %s", value);
	    MONITOR_TRACE_LOG2("Msg send for run_and_read_ulimit_command", TL_INFO,"data seg size is set to %s but it should be unlimited",value);
	    send_mail_to_user(sub_of_mail, msg, NULL);

	    pthread_mutex_unlock(&thread_mutex);
	    data_ulimit_mail_send = 1;
	  }
	}
	else if(!strcmp(first_name, "file") && !strcmp(second_name, "size"))
	{
	  if(!strcmp(value, "unlimited"))
	  {
	    if(file_size_ulimit_mail_send == 1)
	    {
	      pthread_mutex_lock(&thread_mutex);

	      MONITOR_TRACE_LOG2("run_and_read_ulimit_command", TL_INFO,"file size is now set to unlimited");
	      sprintf(sub_of_mail, "Alert Cleared | %s | CSHM | %s | %s | File Size Set To Unlimited", machine_mode,machine_name,my_controller);
	      sprintf(msg,"file size is now set to unlimited");
	      MONITOR_TRACE_LOG2("Msg send for run_and_read_ulimit_command", TL_INFO,"file size is now set to unlimited");
	      send_mail_to_user(sub_of_mail, msg, NULL);

	      pthread_mutex_unlock(&thread_mutex);
	      file_size_ulimit_mail_send = 0;
	    }
	  }
	  else if(file_size_ulimit_mail_send == 0) 
	  {
	    //send max memory not unlimited send alert
	    pthread_mutex_lock(&thread_mutex);

	    MONITOR_TRACE_LOG2("run_and_read_ulimit_command", TL_INFO,"file size is set to %s but it should be unlimited",value);
	    sprintf(sub_of_mail, "Critical | %s | CSHM | %s | %s | File Size Not Set To Unlimited", machine_mode,machine_name,my_controller);
	    snprintf(msg, MAX_READ_LINE_LENGTH, "file size is not to unlimited, it is set to %s", value);
	    MONITOR_TRACE_LOG2("Msg send for run_and_read_ulimit_command", TL_INFO,"file size is set to %s but it should be unlimited",value);
	    send_mail_to_user(sub_of_mail, msg, NULL);

	    pthread_mutex_unlock(&thread_mutex);
	    file_size_ulimit_mail_send = 1;
	  }
	}
	else if(!strcmp(first_name, "max") && !strcmp(second_name, "memory"))
	{
	  if(!strcmp(value, "unlimited"))
	  {
	    if(max_mem_ulimit_mail_send == 1)
	    {
	      pthread_mutex_lock(&thread_mutex);

	      MONITOR_TRACE_LOG2("run_and_read_ulimit_command", TL_INFO,"max mamory size is now set to unlimited");
	      sprintf(sub_of_mail, "Alert Cleared | %s | CSHM | %s | %s | Max Memory Size Set To Unlimited", machine_mode,machine_name,my_controller);
	      sprintf(msg,"max memory size is now set to unlimited");
	      MONITOR_TRACE_LOG2("Msg send for run_and_read_ulimit_command", TL_INFO,"max mamory size is now set to unlimited");
	      send_mail_to_user(sub_of_mail, msg, NULL);

	      pthread_mutex_unlock(&thread_mutex);
	      max_mem_ulimit_mail_send = 0;
	    }
	  }
	  else if(max_mem_ulimit_mail_send == 0)
	  {
	    //send max memory not unlimited send alert
	    pthread_mutex_lock(&thread_mutex);

	    MONITOR_TRACE_LOG2("run_and_read_ulimit_command", TL_INFO,"max memory size is set to %s but it should be unlimited",value);      
	    sprintf(sub_of_mail, "Critical | %s | CSHM | %s | %s | Max Memory Size Not Set To Unlimited", machine_mode,machine_name,my_controller);
	    snprintf(msg, MAX_READ_LINE_LENGTH,"max memory size is not set to unlimited, it is set to %s", value);
	    MONITOR_TRACE_LOG2("Msg send for run_and_read_ulimit_command", TL_INFO,"max memory size is set to %s but it should be unlimited",value);      
	    send_mail_to_user(sub_of_mail, msg, NULL);

	    pthread_mutex_unlock(&thread_mutex);
	    max_mem_ulimit_mail_send = 1;
	  }
	}
	else if(!strcmp(first_name, "cpu"))
	{
	  if(!strcmp(value, "unlimited"))
	  {
	    if(cpu_ulimit_mail_send == 1)
	    { 
	      pthread_mutex_lock(&thread_mutex);

	      MONITOR_TRACE_LOG2("run_and_read_ulimit_command", TL_INFO,"cpu time is now set to unlimited"); 
	      sprintf(sub_of_mail, "Alert Cleared | %s | CSHM | %s | %s | Cpu Time Set To Unlimited", machine_mode,machine_name,my_controller);
	      sprintf(msg,"cpu time is now set to unlimited");
	      MONITOR_TRACE_LOG2("Msg send for run_and_read_ulimit_command", TL_INFO,"cpu time is now set to unlimited"); 
	      send_mail_to_user(sub_of_mail, msg, NULL);

	      pthread_mutex_unlock(&thread_mutex);
	      cpu_ulimit_mail_send = 0;
	    }
	  }
	  else if(cpu_ulimit_mail_send == 0)
	  {
	    //send cpu time not unlimited send alert
	    pthread_mutex_lock(&thread_mutex);

	    MONITOR_TRACE_LOG2("run_and_read_ulimit_command", TL_INFO,"cpu time is set to %s but it should be unlimited",value); 
	    sprintf(sub_of_mail, "Critical | %s | CSHM | %s | %s | Cpu Time Not Set To Unlimited", machine_mode,machine_name,my_controller);
	    snprintf(msg, MAX_READ_LINE_LENGTH, "cpu time is not set to unlimited, it is set to %s", value);
	    MONITOR_TRACE_LOG2("Msg send for run_and_read_ulimit_command", TL_INFO,"cpu time is set to %s but it should be unlimited",value); 
	    send_mail_to_user(sub_of_mail, msg, NULL);

	    pthread_mutex_unlock(&thread_mutex);
	    cpu_ulimit_mail_send = 1;
	  }
	}
	else if(!strcmp(first_name, "virtual"))
	{
	  if(!strcmp(value, "unlimited"))
	  {
	    if(virtual_ulimit_mail_send == 1)
	    {
	      pthread_mutex_lock(&thread_mutex);

	      MONITOR_TRACE_LOG2("run_and_read_ulimit_command", TL_INFO,"virtual memory is now set to unlimited");
	      sprintf(sub_of_mail, "Alert Cleared | %s | CSHM | %s | %s | Virtual Memory Set To Unlimited", machine_mode,machine_name,my_controller);
	      sprintf(msg,"virtual memory is now set to unlimited");
	      MONITOR_TRACE_LOG2("Msg send for run_and_read_ulimit_command", TL_INFO,"virtual memory is now set to unlimited");
	      send_mail_to_user(sub_of_mail, msg, NULL);

	      pthread_mutex_unlock(&thread_mutex);
	      virtual_ulimit_mail_send = 0;
	    }
	  }
	  else if(virtual_ulimit_mail_send == 0)
	  {
	    //send virtual memory not unlimited send alert
	    pthread_mutex_lock(&thread_mutex);

	    MONITOR_TRACE_LOG2("run_and_read_ulimit_command", TL_INFO,"virtual memory is set to %s but it should be unlimited",value);
	    sprintf(sub_of_mail, "Critical | %s | CSHM | %s | %s | Virtual Memory Not Set To Unlimited", machine_mode,machine_name,my_controller);
	    snprintf(msg, MAX_READ_LINE_LENGTH, "virtual memory is not set to unlimited, it is set to %s", value);
	    MONITOR_TRACE_LOG2("Msg send for run_and_read_ulimit_command", TL_INFO,"virtual memory is set to %s but it should be unlimited",value);
	    send_mail_to_user(sub_of_mail, msg, NULL);        

	    pthread_mutex_unlock(&thread_mutex);
	    virtual_ulimit_mail_send = 1;
	  }
	}
	else if(!strcmp(first_name, "file") && !strcmp(second_name, "locks"))
	{
	  if(!strcmp(value, "unlimited"))
	  {
	    if(file_locks_ulimit_mail_send == 1)
	    {
	      pthread_mutex_lock(&thread_mutex);

	      MONITOR_TRACE_LOG2("run_and_read_ulimit_command", TL_INFO,"file locks is now set to unlimited"); 
	      sprintf(sub_of_mail, "Alert Cleared | %s | CSHM | %s | %s | File Locks Set To Unlimited", machine_mode,machine_name,my_controller);
	      sprintf(msg,"file locks is now set to unlimited");
	      MONITOR_TRACE_LOG2("Msg send for run_and_read_ulimit_command", TL_INFO,"file locks is now set to unlimited"); 
	      send_mail_to_user(sub_of_mail, msg, NULL);

	      pthread_mutex_unlock(&thread_mutex);
	      file_locks_ulimit_mail_send = 0;
	    }
	  }
	  else if(file_locks_ulimit_mail_send == 0)
	  {
	    //send file locks not unlimited send alert
	    pthread_mutex_lock(&thread_mutex);

	    MONITOR_TRACE_LOG2("run_and_read_ulimit_command", TL_INFO,"file locks is set to %s but it should be unlimited",value);
	    sprintf(sub_of_mail, "Critical | %s | CSHM | %s | %s | File Locks Not Set To Unlimited", machine_mode,machine_name,my_controller);
	    snprintf(msg, MAX_READ_LINE_LENGTH, "file locks is not set to unlimited, it is set to %s", value);
	    MONITOR_TRACE_LOG2("Msg send for run_and_read_ulimit_command", TL_INFO,"file locks is set to %s but it should be unlimited",value);
	    send_mail_to_user(sub_of_mail, msg, NULL);

	    pthread_mutex_unlock(&thread_mutex);
	    file_locks_ulimit_mail_send = 1;
	  }
	}
	else if(!strcmp(first_name, "open"))
	{
	  int_value = atoi(value);
	  if( int_value > 262144 && open_ulimit_mail_send == 0)
	  {
	    //send open files are greater send alert
	    pthread_mutex_lock(&thread_mutex);

	    MONITOR_TRACE_LOG2("run_and_read_ulimit_command", TL_INFO,"open files is now set to %s which is more than threshold : 262144",value); 
	    sprintf(sub_of_mail, "Critical | %s | CSHM | %s | %s | Open Files Crossed The Threshold", machine_mode,machine_name,my_controller);
	    snprintf(msg, MAX_READ_LINE_LENGTH, "open files is not less than the threshold : 262144, it is set to %s", value);
	    MONITOR_TRACE_LOG2("Msg send for run_and_read_ulimit_command", TL_INFO,"open files is now set to %s which is more than threshold : 262144",value); 
	    send_mail_to_user(sub_of_mail, msg, NULL);       

	    pthread_mutex_unlock(&thread_mutex);
	    open_ulimit_mail_send = 1;
	  }
	  else
	  {
	    if(open_ulimit_mail_send == 1)
	    {
	      pthread_mutex_lock(&thread_mutex);

	      MONITOR_TRACE_LOG2("run_and_read_ulimit_command", TL_INFO,"open files is under the threshold limit");
	      sprintf(sub_of_mail, "Alert Cleared | %s | CSHM | %s | %s | Open Files Crossed The Threshold", machine_mode,machine_name,my_controller);
	      sprintf(msg,"open files is now ok");
	      MONITOR_TRACE_LOG2("Msg send for run_and_read_ulimit_command", TL_INFO,"open files is under the threshold limit");
	      send_mail_to_user(sub_of_mail, msg, NULL);

	      pthread_mutex_unlock(&thread_mutex);
	      open_ulimit_mail_send = 0;
	    }
	  }
	}
	else if(!strcmp(first_name, "stack"))
	{
	  int_value = atoi(value);
	  if( int_value > 8192 && stack_ulimit_mail_send == 0)
	  {
	    //send stack size are greater send alert
	    pthread_mutex_lock(&thread_mutex);

	    MONITOR_TRACE_LOG2("run_and_read_ulimit_command", TL_INFO,"stack size is now set to %s which is more than threshold : 8192 \n",value);
	    sprintf(sub_of_mail, "Critical | %s | CSHM | %s | %s | Stack Size Crossed The Threshold", machine_mode,machine_name,my_controller);
	    snprintf(msg, MAX_READ_LINE_LENGTH, "stack size is not less than threshold : 8192 , it is set to %s \n", value);
	    MONITOR_TRACE_LOG2("Msg send for run_and_read_ulimit_command", TL_INFO,"stack size is now set to %s which is more than threshold : 8192 \n",value);
	    send_mail_to_user(sub_of_mail, msg, NULL);    

	    pthread_mutex_unlock(&thread_mutex);
	    stack_ulimit_mail_send = 1;
	  }
	  else
	  {
	    if(stack_ulimit_mail_send == 1)
	    {
	      pthread_mutex_lock(&thread_mutex);

	      MONITOR_TRACE_LOG2("run_and_read_ulimit_command", TL_INFO,"stack size is under the threshold : 8192");
	      sprintf(sub_of_mail, "Alert Cleared | %s | CSHM | %s | %s | Stack Size Crossed The Threshold",                                                                                                                           machine_mode,machine_name,my_controller);
	      sprintf(msg,"stack size is now ok");
	      MONITOR_TRACE_LOG2("Msg send for run_and_read_ulimit_command", TL_INFO,"stack size is under the threshold : 8192");
	      send_mail_to_user(sub_of_mail, msg, NULL);

	      pthread_mutex_unlock(&thread_mutex);
	      stack_ulimit_mail_send = 0;
	    }
	  } 
	}
      }
      if(fp)
        pclose(fp);
    }
    sleep(system_level_monitor_sleep_time);
  }
  return NULL;
}


void* check_shared_mem_buff()
{ 
  FILE *fp = NULL;
  char command[1024];
  long long int shmmax;
  long long int shared_buff;
  int total_mem;
  int shmmax_threshold;
  int shm_max;
  int shared_buff_threshold;
  char unit[10];
  char buff[1024];  
  int ret;
  int shared_buffer_mail_send = 0; 
  int shmmax_mail_send = 0;

  MONITOR_TRACE_LOG2("check_shared_mem_buff", TL_INFO, "check shared mem buff thread started \n"); 

  while(1)
  {
    sprintf(command, "free -g | grep 'Mem' | tr -s ' ' | cut -d ' ' -f2");

    ret = run_cmd(&fp, command);

    if(ret == -1)
    { 
      MONITOR_TRACE_LOG1("check_shared_mem_buff", TL_ERROR, "Unable to run command : '%s'\n", command);
      sleep(300);
      continue;
    }
    else
    {
      fgets(buff, 1024, fp);

      if(fp)
	pclose(fp);

      sscanf(buff, "%d", &total_mem);

      sprintf(command, "cat /etc/postgresql/9.5/main/postgresql.conf | grep '^shared_buffers' | awk '{print $3}' | sed 's/^[0-9]*/& /'");

      ret = run_cmd(&fp, command);

      if(ret == -1)
      {
	MONITOR_TRACE_LOG1("check_shared_mem_buff", TL_ERROR, "Unable to run command : '%s'\n", command);
	sleep(300);
	continue;
      }
      else
      {
	fgets(buff, 1024, fp);

	if(fp)
	  pclose(fp);

	sscanf(buff, "%lld %s", &shared_buff, unit );
        
        if((total_mem/4) > 20)
          shared_buff_threshold = 20;
        else
          shared_buff_threshold = total_mem/4;

	if(!strcmp(unit, "GB"))
	{
	  if(shared_buff < shared_buff_threshold)
	  {
	    if(shared_buffer_mail_send != 1)
	    {
	      //send mail shared buff is less than preferred by pgtune
	      pthread_mutex_lock(&thread_mutex);

	      MONITOR_TRACE_LOG2("check_shared_mem_buff", TL_INFO,"shared buffers must be %d but it is set to %lld \n", shared_buff_threshold ,shared_buff);
	      sprintf(sub_of_mail, "Critical | %s | CSHM | %s | %s | PostGres Shared Buffer Value not Optimal", machine_mode,machine_name,my_controller);
	      sprintf(msg, "Recommended value of PostGres shared buffers is %d GB. However, it is set to %lld GB. Consider changing the value.\n", shared_buff_threshold ,shared_buff);
              
              if(send_mail)
              {
	        MONITOR_TRACE_LOG2("Msg send for check_shared_mem_buff", TL_INFO,"%s", msg);
	        send_mail_to_user(sub_of_mail, msg, NULL);
              }
              else
              {
                MONITOR_TRACE_LOG1("Msg not send for check_shared_mem_buff", TL_ERROR,"%s", msg);
              }
                 
	      pthread_mutex_unlock(&thread_mutex);
	      shared_buffer_mail_send = 1;
	    }
	  }
	  else
	  {
	    if(shared_buffer_mail_send == 1 && shared_buff >= shared_buff_threshold)
	    {

	      MONITOR_TRACE_LOG2("check_shared_mem_buff", TL_INFO, "shared buffers is now ok");
	      shared_buffer_mail_send = 0;
	    }
	  } 
	}
      } 
      sprintf(command, "cat /proc/sys/kernel/shmmax");

      ret = run_cmd(&fp, command);

      if(ret == -1)
      {
	MONITOR_TRACE_LOG1("check_shared_mem_buff", TL_ERROR, "Unable to run command : '%s'\n", command);
      }
      else
      { 
	fgets(buff, 1024, fp);

	if(fp)
	  pclose(fp);

	sscanf(buff, "%lld", &shmmax);
      
        shm_max = shmmax/1073741824;
        
        if((total_mem/2) > 30)
          shmmax_threshold = 30;
        else
          shmmax_threshold = total_mem/2;

	if(shm_max < shmmax_threshold)
	{
	  if(shmmax_mail_send != 1)
	  {
	    //send alert shmamx value is less than preffered by pgtune
	    pthread_mutex_lock(&thread_mutex);

	    MONITOR_TRACE_LOG2("check_shared_mem_buff", TL_INFO,"shared memory must be %d but it is set to %d \n", shmmax_threshold, shm_max); 
	    sprintf(sub_of_mail, "Critical | %s | CSHM | %s | %s | Shared Memory Value not Optimal", machine_mode,machine_name,my_controller);
	    sprintf(msg, "Recommended setting of shared memory is %d GB. However, it is set to %d GB. Consider changing the value\n", shmmax_threshold, shm_max);
      
            if(send_mail)
            {
	      MONITOR_TRACE_LOG2("Msg send for check_shared_mem_buff", TL_INFO,"%s",msg); 
              send_mail_to_user(sub_of_mail, msg, NULL);
            }
            else
            {
              MONITOR_TRACE_LOG1("Msg not send for check_shared_mem_buff", TL_ERROR,"%s", msg);
            }

	    pthread_mutex_unlock(&thread_mutex);
	    shmmax_mail_send = 1;
	  }
	}
	else
	{
	  if(shmmax_mail_send == 1 && shm_max >= shmmax_threshold)
	  {
	    MONITOR_TRACE_LOG2("check_shared_mem_buff", TL_INFO, "shared memory is now ok");
	    shmmax_mail_send = 0;
	  }
	}
      }
    }
    sleep(system_level_monitor_sleep_time);
  }
  return NULL;
}


void* check_disk_free_space()
{ 
  FILE *fp = NULL;
  char command[1024] = {0};
  char readbuff[1024] = {0};  
  char buff[1024] = {0};
  int ret = 0;
  int free_space_mail_send = 0;
  int value = 0;
  int suid_dumpable_mail_send=0;
  FILE *fr = NULL;
  char* filename = "/proc/sys/fs/suid_dumpable";
  struct stat attr;
  int disk_msg_len = 0;

  MONITOR_TRACE_LOG2("check_disk_free_space", TL_INFO, "check_disk_free_space thread started \n");

  while(1) 
  {
    sprintf(command, "df -h | awk '0+$5 > %d {print}'",disk_free_space_threshold);

    ret = run_cmd(&fp, command);

    if(ret == -1)
    { 
      MONITOR_TRACE_LOG1("check_disk_free_space", TL_ERROR, "Unable to run command : '%s'\n", command);
      sleep(300);
      continue;
    }
    else
    {
      if(fgets(buff, 1024, fp) != NULL)
      {
	if(buff[0] != '\0' && free_space_mail_send != 1 && buff[0] != '\n')
	{
	  pthread_mutex_lock(&thread_mutex);

	  sprintf(sub_of_mail, "Critical | %s | CSHM | %s | %s | Free Disk Space Crossed The Threshold",                                                                                                                                          machine_mode,machine_name,my_controller);
	  disk_msg_len = 0;
	  disk_msg_len = sprintf(msg,"Free Disk space has crossed the threshold (%d%%) for following partitions\n\n", disk_free_space_threshold);

          disk_msg_len += snprintf(msg+disk_msg_len, MAX_READ_LINE_LENGTH - disk_msg_len,"%s\n", buff);

	  while(fgets(readbuff, 1024, fp) != NULL)
	  {
	    disk_msg_len += snprintf(msg+disk_msg_len, MAX_READ_LINE_LENGTH - disk_msg_len,"%s\n", readbuff);
	  }
	  MONITOR_TRACE_LOG2("check_disk_free_space", TL_INFO, "Free space less than %d%\n", disk_free_space_threshold);
         
          if(send_mail)
          {
	    MONITOR_TRACE_LOG2("Msg send for check_disk_free_space", TL_INFO, "Free space stats are:\n%s", msg);
	    send_mail_to_user(sub_of_mail, msg, NULL);      
          }
          else
          {
            MONITOR_TRACE_LOG1("Msg not send for check_disk_free_space", TL_ERROR, "Free space stats are:\n%s", msg);
          }

	  pthread_mutex_unlock(&thread_mutex);
	  free_space_mail_send = 1;
	}
      }
      else if(free_space_mail_send == 1) 
      {
	MONITOR_TRACE_LOG2("check_disk_free_space", TL_INFO, "Free space is now ok");
	free_space_mail_send = 0;
      }
      if(fp)
        pclose(fp);
    }

    fr = fopen(filename, "r");

    if(fr == NULL)
    {
      MONITOR_TRACE_LOG1("check_suid_dumpable_value", TL_ERROR, "Could not open file '%s'\n",filename);
    }
    else
    {
      if(fgets(buff, 1024, fr) == NULL)
      {
	MONITOR_TRACE_LOG1("check_suid_dumpable_value", TL_ERROR, "Cannot read Suid dumpable file and value : %d \n", value);
	fclose(fr);
      }
      else
      {
	fclose(fr);

	value = atoi(buff);

	MONITOR_TRACE_LOG3("check_suid_dumpable_value", TL_INFO, "Suid dumpable file value : '%d'\n", value);

	if(value != 1 && suid_dumpable_mail_send != 1)
	{

	  MONITOR_TRACE_LOG2("check_suid_dumpable_value", TL_INFO, "Suid dumpable value : '%d'\n", value);

	  if(stat(filename, &attr))
	  {
	    MONITOR_TRACE_LOG1("check_suid_dumpable_value", TL_ERROR, "Could not find file '%s'", filename);
	  }
	  else
	  {

	    strcpy(buff,ctime(&attr.st_mtime));          

	    MONITOR_TRACE_LOG2("check_suid_dumpable_value", TL_INFO, "Last modified date and time of suid dumpable file = '%s'", buff);

	    if(suid_dumpable_mail_send != 1)
	    {
	      pthread_mutex_lock(&thread_mutex);

	      sprintf(sub_of_mail, "Critical | %s | CSHM | %s | %s| Suid Dumpable Value Is Not Equal To 1",                                                                                                                                   machine_mode,machine_name,my_controller);
	      snprintf(msg, MAX_READ_LINE_LENGTH, "Suid Dumpable file value = %d .Last modified date and time = %s ", value, buff);
	      MONITOR_TRACE_LOG2("Msg send for check_suid_dumpable_value", TL_INFO, " %s ", msg);
	      send_mail_to_user(sub_of_mail, msg, NULL);      

	      pthread_mutex_unlock(&thread_mutex);
	      suid_dumpable_mail_send = 1;
	    }
	  }
	}
	else if(value == 1 && suid_dumpable_mail_send == 1)
	{
          MONITOR_TRACE_LOG2("check_suid_dumpable_value", TL_INFO, "Value of suid dumpable file : '%d'", value);
	  suid_dumpable_mail_send = 0;
	}
      }
    }
    sleep(system_level_monitor_sleep_time);
  }
  return NULL;		
}


void* find_core_and_remove_core()
{
  int i, end_index, ret;
  FILE *fp = NULL;
  char buf[MAX_SIZE];
  buf[0] = '\n';
  char *newline_ptr = NULL;
  char core_name[MAX_SIZE];
  core_name[0]='\n';
  char buff[MAX_SIZE];
  buff[0]='\n';
  char oldest_file[MAX_SIZE];
  oldest_file[0]='\n';
  int size_of_core_dir = 0;
  long long int core_size; 
  long long timestamp;
  long int core_size_mb;
  threshold_timestamp_diff = threshold_timestamp_diff_days*24*60*60;   // seconds = days*24*60*60

  /* Monitor should not report core dumps which were created before monitor started,
   * That is why initialize last reporting time here */
  found_core_list.last_reporting_time = time(NULL);

  //To sort all the core_info array on the basis of core path name
  qsort(core_info, total_core_binary, sizeof(Core_Info), sort_core_path);
  match_binary_path();


  MONITOR_TRACE_LOG2("find_core_and_remove_core", TL_INFO, "find_core_and_remove_core thread started \n" );
  while(1)
  {
    sprintf(buf, "du -sm /home/cavisson/core_files/");

    ret = run_cmd(&fp, buf);

    if(ret == -1)
    {
      MONITOR_TRACE_LOG1("find_core_and_remove_core", TL_ERROR, "Unable to run command : '%s'\n", buf);
      sleep(300);
      continue;
    }
    else
    {
      if(fgets(buff, 1024, fp) == NULL)
      {
        MONITOR_TRACE_LOG1("find_core_and_remove_core", TL_ERROR, "Unable to get size of core directory");
        if(fp)
         pclose(fp);
      }
      else
      {
        if(fp)
         pclose(fp);

        size_of_core_dir = atoi(buff);
        
        if( size_of_core_dir > threshold_size_of_core_dir )
        {
          sprintf(buf, "ls -t /home/cavisson/core_files/ | grep \"^core.\" | tail -1");
    
          ret = run_cmd(&fp, buf);

          if(ret == -1)
          {
            MONITOR_TRACE_LOG1("find_core_and_remove_core", TL_ERROR, "Unable to run command : '%s'\n", buf);
            sleep(300);
            continue;
          }
          else
          {
            if(fgets(buff, 1024, fp) == NULL)
            {
              MONITOR_TRACE_LOG1("find_core_and_remove_core", TL_ERROR, "Unable to get the file name of core to be deleted");
              if(fp)
                pclose(fp);
            }
            else
            {
              if(fp)
                pclose(fp);

              sscanf(buff, "%s", core_name); 
              
              sprintf(oldest_file, "/home/cavisson/core_files/%s", core_name);
             
              if(!(unlink(oldest_file)))
              {
                MONITOR_TRACE_LOG2("find_core_and_remove_core", TL_INFO, "Removed core file '%s' due to size of core path crossed\n", oldest_file);
              }
              else
              {
                MONITOR_TRACE_LOG1("find_core_and_remove_core", TL_ERROR, "Unable to remove core file '%s'\n", oldest_file);
                move_core(core_name);
              }
            }
          }
        }

        i = 0;
        while(i < total_core_binary)
        {
          /* In order to get all core files sorted by time, we are trying to execute following command - 
           * ls -l --time-style=+"%s" | grep "^-" | awk '{print $7 " " $6}' | grep "^core.[0-9]*"
           * Following are the meaning of each command used - 
           * ls -lt --time-style=+"%s"  <List all files, sort by time, and print time in seconds form>
           * grep "^-"                  <Considering regular files only>
           * awk '{print $7 " " $6}'    <Printing timestamp and file name>
           * grep "^core"               <Checking if file name starts with "core" keyword   */
          sprintf(buf, "ls -lt --time-style=+\'%%s' %s | grep \"^-\" | awk '{print $7 \" \" $6 \" \" $5}' | grep \"^core\"", core_info[i].core_path);
          if(run_cmd(&fp, buf) == -1)
          {
            i++;
            continue;
          }

          end_index = i + core_info[i].index;

          /* Read names of all cores one by one. Latest core will come first */
          while((fgets(buf, MAX_SIZE, fp))!= NULL)
          {
            if((newline_ptr = strchr(buf, '\n')))
            *newline_ptr = '\0';

            sscanf(buf, "%s %lld %lld", core_name, &timestamp, &core_size);
            
            core_size_mb = core_size/1048576;
 
            /*This function will process core file on the basis of timestamp and add core name in structure*/
            process_core_file(core_info[i].core_path, core_name, timestamp, i, end_index, core_size_mb);
          }

          i = end_index;
          if(fp)
          	pclose(fp);
        }

        //set 0 for core counter for all core binaries 
        for(i = 0; i < total_core_binary; i++)
          core_info[i].core_counter = 0;

        //update last_reporting_time variable every time when its going to sleep
        found_core_list.last_reporting_time = time(NULL);
        sleep(core_delay_time);
      }
    }
  }
  return NULL;
}

//This flag will set we need to dump the Backtrace of core in file or not default 0
void kw_set_bt_flag(char *value)
{
  if(ns_is_numeric(value) != 0)
  {
    dump_bt_flag = atoi(value);
  }

  MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "DUMP_BT_IN_FILE Keyword found value = '%d'\n" , dump_bt_flag);
}

//This Keyword will set the time to check the time in which we will check core default 300 Seconds
void kw_set_core_delay_time(char *value)
{
  long long int ret;

  ret = check_sleep_interval_in_min_sec_or_hr(value);

  if(ret != -1)
  {
    core_delay_time = ret;
  }
  MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "CORE_DELAY_TIME Keyword found value = '%lld' seconds\n" , core_delay_time);
}


//This Keyword will set the time to check the time in which we will check file system default 300 Seconds
void kw_set_filesystem_delay_time(char *value)
{
  long long int ret;

  ret = check_sleep_interval_in_min_sec_or_hr(value);

  if(ret != -1)
  {
    filesystem_delay_time = ret;
  }
  MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "FILE_SYSTEM_DELAY_TIME Keyword found value = '%lld' seconds\n", filesystem_delay_time);
}


//This Keyword will set the time to check the time in which we will check postgresql, default 1 hour
void kw_set_postgresql_delay_time(char *value)
{
  long long int ret;

  ret = check_sleep_interval_in_min_sec_or_hr(value);

  if(ret != -1)
  {
    postgresql_delay_time = ret;
  }
  MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "POSTGRESQL_DELAY_TIME Keyword found value = '%lld' seconds\n" , postgresql_delay_time);
}


//This Keyword will set the time to check the time in which we will check .rel directory, default 24 hours
void kw_set_rel_dir_delay_time(char *value)
{
  long long int ret;

  ret = check_sleep_interval_in_min_sec_or_hr(value);

  if(ret != -1)
  {
    rel_dir_delay_time = ret;
  }
  MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "REL_DIR_DELAY_TIME Keyword found value = '%lld' seconds\n" , rel_dir_delay_time);
}

//This Keyword will set the time to check the permission, owner name and group name of a path, default is 300 Seconds 
void kw_set_permission_and_ownership_check_delay_time(char *value)
{
  long long int ret;

  ret = check_sleep_interval_in_min_sec_or_hr(value);

  if(ret != -1)
  {
    monitor_path_delay_time = ret;
  }
  MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "PERMISSION_AND_OWNERSHIP_CHECK_DELAY_TIME keyword found value = '%lld' seconds\n", monitor_path_delay_time);

}

//This Keyword will set the time to check the time in which we will check constraints are applied on ND & NS tables, default 300 Seconds
void kw_set_missing_constraint_delay_time(char *value)
{
  long long int ret;

  ret = check_sleep_interval_in_min_sec_or_hr(value);

  if(ret != -1)
  {
    missing_constraint_delay_time = ret;
  }
  MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "MISSING_CONSTRAINT_DELAY_TIME Keyword found value = '%lld' seconds\n", missing_constraint_delay_time);
}

//This Keyword will set the time to check the time in which we check max file size in tmp directory, default 300 Seconds
void kw_set_max_file_size_delay_time(char *value)
{
  long long int ret;

  ret = check_sleep_interval_in_min_sec_or_hr(value);

  if(ret != -1)
  {
    max_file_size_delay_time = ret;
  }
  MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "MAX_FILE_SIZE_DELAY_TIME Keyword found value = '%lld' seconds\n", max_file_size_delay_time);
}

//This Keyword will set the time to check the time in which we check size in configured directory, default 24 hours
void kw_set_unexpected_files_in_dir_delay_time(char *value)
{
  long long int ret;

  ret = check_sleep_interval_in_min_sec_or_hr(value);

  if(ret != -1)
  {
    unexpected_files_in_dir_delay_time = ret;
  }
  MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "UNEXPECTED_FILES_IN_DIR_DELAY_TIME Keyword found value = '%lld' seconds\n", 
      unexpected_files_in_dir_delay_time);
}


//This Keyword will set the time to check the time in which we check ndp and dbu progress, default 300 Seconds
void kw_set_ndp_dbu_progress_delay_time(char *value)
{
  long long int ret;

  /* This function will convert units(minutes and hours) in seconds*/ 
  ret = check_sleep_interval_in_min_sec_or_hr(value);

  if(ret != -1)
  {
    ndp_dbu_progress_delay_time = ret;
  }

  MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "NDP_DBU_PROGRESS_DELAY_TIME Keyword found value = '%lld' seconds\n",
      ndp_dbu_progress_delay_time);
}


//This Keyword will set the time to check the time in which we check test run is running or not, default 300 Seconds
void kw_set_test_run_delay_time(char *value)
{
  long long int ret;

  /* This function will convert units(minutes and hours) in seconds*/
  ret = check_sleep_interval_in_min_sec_or_hr(value);

  if(ret != -1)
  {
    test_run_delay_time = ret;
  }

  MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "TEST_RUN_DELAY_TIME Keyword found value = '%lld' seconds\n", test_run_delay_time);
}

//This Keyword will set the time to check the time in which we check packet size of testrun.gdf file, default 300 Seconds
void kw_set_rtg_delay_time(char *value)
{
  long long int ret;

  /* This function will convert units(minutes and hours) in seconds*/
  ret = check_sleep_interval_in_min_sec_or_hr(value);

  if(ret != -1)
  {
    rtg_delay_time = ret;
  }

  MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "RTG_DELAY_TIME Keyword found value = '%lld' seconds\n", rtg_delay_time);
}

void kw_set_mandatory_process_alert_frequency(char *value)
{
  if(!strcmp(value, "1")) 
    process_alert_frequency = 1;   
  return ;
  MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "MANDATORY_PROCESS_ALERT_FREQUENCY Keyword found value = '%s'. Setting keyword with %d \n",                                                                                                                          process_alert_frequency);
}

//This Keyword will set the time to check the time in which we will check mandatory process status, default 300 Seconds
void kw_set_mandatory_process_delay_time(char *value)
{
  long long int ret;

  ret = check_sleep_interval_in_min_sec_or_hr(value);

  if(ret != -1)
  {
    mandatory_process_delay_time = ret;
  }
  MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "MANDATORY_PROCESS_DELAY_TIME Keyword found value = '%lld' seconds\n" , mandatory_process_delay_time);
}

//This Keyword will set the time to check the time in which we will check unwanted process status, default 300 Seconds
void kw_set_unwanted_process_delay_time(char *value)
{
  long long int ret;

  ret = check_sleep_interval_in_min_sec_or_hr(value);

  if(ret != -1)
  {
    unwanted_process_delay_time = ret;
  }
  MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "UNWANTED_PROCESS_DELAY_TIME Keyword found value = '%lld' seconds\n" , unwanted_process_delay_time);
}



//This Keyword will set the time to check the time in which we will check core default 300 Seconds
void kw_set_mainthread_sleep_time(char *value)
{
  long long int ret;

  ret = check_sleep_interval_in_min_sec_or_hr(value);

  if(ret != -1)
  {
    sleep_time = ret;
  }
  MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "SLEEP_TIME Keyword found value = '%lld' seconds\n" , sleep_time);
}


void check_size_exceeds(int size, char *buf)
{
  if(size > MAX_SIZE)
    MONITOR_TRACE_LOG2("Main_Thread", TL_ERROR, "Size of string is more than %d Max Size. so it being truncated\n truncated string:'%s'", MAX_SIZE, buf);
}

//This Keyword will save the tmpfs partition space threshold value
void kw_set_fs_threshold(char *filesys_name, char *mounted_name, char *value)
{
  int size_to_realloc = 0;

  /*If no of rows is equal to no of strings means now need to realloc by 8*/
  /*It means realloc by 8, when more than 8 keywords come*/
  if(total_no_of_rows == no_of_fs_keyword)
  {
    size_to_realloc = ((total_no_of_rows + N_ROWS) * sizeof(char*));

    MY_REALLOC(file_system_name, size_to_realloc, "File System Name", -1);
    MY_REALLOC(fs_mounted_name, size_to_realloc, "File System Mounted Path", -1);
    MY_REALLOC(fs_threshold, ((total_no_of_rows + N_ROWS) * sizeof(int)), "File System threshold value", -1);
    MY_REALLOC(fs_threshold_for_mail, ((total_no_of_rows + N_ROWS) * sizeof(int)), "File System threshold value for mail", -1);
    MY_REALLOC(filesystem_mounted_flag, ((total_no_of_rows + N_ROWS) * sizeof(int)), "File System Mounted Flag", -1);

    // Increase total_no_of_rows varible by 8
    total_no_of_rows += 8;   
  }

  /*Malloc each index of file_system_name, fs_mounted_name with length of string passed in function*/
  file_system_name[no_of_fs_keyword] = malloc(sizeof(char) * (strlen(filesys_name) + 1));
  fs_mounted_name[no_of_fs_keyword]= malloc(sizeof(char) * (strlen(mounted_name) + 1));

  /*copy string passed in function in variables*/
  strcpy(file_system_name[no_of_fs_keyword], filesys_name);
  strcpy(fs_mounted_name[no_of_fs_keyword], mounted_name);

  /*If user give some negative value or non numeric number for threshold value so we set default value to 80% otherwise if give 0 then take 0*/
  if(value[0] != '0')
  {
    fs_threshold[no_of_fs_keyword] = atoi(value);
    fs_threshold_for_mail[no_of_fs_keyword] = atoi(value);
    filesystem_mounted_flag[no_of_fs_keyword] = 0;

    if(fs_threshold[no_of_fs_keyword] <= 0 || fs_threshold[no_of_fs_keyword] > 100)
    {
      fs_threshold[no_of_fs_keyword] = 80;
      fs_threshold_for_mail[no_of_fs_keyword] = 80;
    }
  }
  else
  {
    fs_threshold[no_of_fs_keyword] = atoi(value);
    fs_threshold_for_mail[no_of_fs_keyword] = atoi(value);
    filesystem_mounted_flag[no_of_fs_keyword] = 0;
  }

  MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "FILE_SYSTEM_THRESHOLD Keyword found filesystem name = '%s' mounted name = '%s' threshold = '%d%' \n" , file_system_name[no_of_fs_keyword], fs_mounted_name[no_of_fs_keyword], fs_threshold[no_of_fs_keyword]);

  no_of_fs_keyword++;
}

//This Keyword will remove builds from .rel directory as specified by user
void kw_set_num_builds_threshold_in_rel_dir(char *value)
{
  //char path[512] = "";
  //int ret = 0, i;
  int build_threshold_value;
  //int num_dirs;

  MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "NUM_BUILDS_THRESHOLD_IN_REL_DIR Keyword found threshold = '%s' \n" , value);

  //snprintf(path, 512, "%s/.rel", ns_wdir);

  /* If user give some negative value or non numeric number for build_threshold_value so we set default value to 25 files otherwise if give 0 then take 0 value*/
  if(value[0] != '0')
  {
    build_threshold_value = atoi(value);

    if(build_threshold_value <= 0)
      build_threshold_value = 5;
  }
  else
    build_threshold_value = atoi(value);

  /* User can provide NS_WDIR to check builds in rel directory on all controllers */
  //if(!strncmp(path, "NS_WDIR", 7))
  // num_dirs = total_controller;
  //else
  //num_dirs = 1;

  /* total_rel_path holds value of previously malloced structures */
  //size_to_realloc = sizeof(RelDir_Info) * (total_rel_path + num_dirs);
  //MY_REALLOC(rel_info, size_to_realloc, "Rel Dir Path Table", -1);

  /* Fill info of all dirs in structure */
  //for(i = 0; i < num_dirs; i++)
  //{
    /* if complete path was provided */
    // if(num_dirs == 1)
    //ret = snprintf(rel_info[total_rel_path].rel_dir_path, MAX_SIZE, "%s", path);
    /* if NS_WDIR was provided */
    // else
    // ret = snprintf(rel_info[total_rel_path].rel_dir_path, MAX_SIZE, "%s%s", controller_name[i], path+7);

    /* Log error if snprintf truncated something */
    //check_size_exceeds(ret, rel_info[total_rel_path].rel_dir_path);

    /* Save threshold values */
    builds_threshold = build_threshold_value;
    //MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "Full Rel dir path = '%s' Threshold = '%d'\n, rel_info[total_rel_path].rel_dir_path, rel_info[total_rel_path].builds_threshold);
    MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "Build threshold in .rel dir = '%d'\n", builds_threshold);

    //total_rel_path++;
  //}
}


//This Keyword is used to check postgres is running or not and its performance
void kw_check_postgresql_monitor(char *value, char *value1, char *value2)
{
  long long int ret;

  if(atoi(value) == 1)
  {

    ret = check_sleep_interval_in_min_sec_or_hr(value1);

    if(ret != -1)
    {
      query_exec_threshold_value = ret;
    }

    num_connection_threshold_value = atoi(value2);
    if(num_connection_threshold_value <= 0)
      num_connection_threshold_value = 80;

    MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "CHECK_POSTGRESQL_MONITOR keyword is enabled. "
	"Query time threshold = %lld seconds, Num connections threshold = %d \n", 
	query_exec_threshold_value, num_connection_threshold_value);
  }
}


void kw_enable_system_level_health(int enable )
{
  if(!enable)
  {
    start_thread[READ_CSV_FILE] = 0;
    start_thread[CHECK_ULIMIT_FLAG] = 0;
    start_thread[CHECK_SHARED_MEM_BUFF] = 0;
    start_thread[CHECK_DISK_FREE] = 0;
    start_thread[CHECK_HEAP_DUMP_FLAG] = 0;
    start_thread[POSTGRES_FLAG] = 0;
    start_thread[MAX_FILE_SIZE_FLAG] = 0;
  }
  return;
}

//This Keyword is used to set name of the machine
void kw_check_machine_name(char *buf)
{
  sprintf(machine_name, "%s", buf);
  MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "SET_MACHINE_NAME keyword is enabled and machine name = '%s' \n", buf);
}


//Save core path and binary name
void kw_set_core_info(char *core_path, char *binary_name, char *max_cores_allowed, char *max_core_size)
{
  int i, ret = 0;
  int num_dirs;
  char *ptr = NULL;

  MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "CORE_INFO Keyword found path = '%s' binary name = '%s' max_cores_allowed = '%s' max_core_size = '%s'\n", core_path, 
      binary_name, max_cores_allowed, max_core_size);

  int j;

  for(j = 0; j < total_core_binary; j++)
  {
    if(!strcmp(core_info[j].binary_name, binary_name))
      break;
  }

  if(j < total_core_binary)
    return;

  /* User can provide NS_WDIR/core_name to check core path on all controllers */
  //if(!strncmp(core_path, "NS_WDIR", 7))
  //num_dirs = total_controller;
  //else
  num_dirs = 1;

  /* total_core_binary holds value of previously malloced structures */
  size_to_realloc = sizeof(Core_Info) * (total_core_binary + num_dirs);
  MY_REALLOC(core_info, size_to_realloc, "Binary name Table", -1);

  /* Fill info of all dirs in structure */
  for(i = 0; i < num_dirs; i++)
  {
    /* if complete path was provided */
    //if(num_dirs == 1)
    //{
    ret = snprintf(core_info[total_core_binary].core_path, MAX_SIZE, "%s", core_path);
    ret = snprintf(core_info[total_core_binary].binary_name, MAX_SIZE, "%s", binary_name);
    // }  
    /* if NS_WDIR was provided */
    // else
    //{
    //ret = snprintf(core_info[total_core_binary].core_path, MAX_SIZE, "%s%s", controller_name[i], core_path+7);
    //ret = snprintf(core_info[total_core_binary].binary_name, MAX_SIZE, "%s%s", controller_name[i], binary_name+7);
    // }

    core_info[total_core_binary].max_cores_allowed = atoi(max_cores_allowed);
    if(core_info[total_core_binary].max_cores_allowed <= 0)
      core_info[total_core_binary].max_cores_allowed = 3;

    core_info[total_core_binary].max_core_size = atoi(max_core_size);
    if(core_info[total_core_binary].max_core_size <= 0)
      core_info[total_core_binary].max_core_size = 20480;

    core_info[total_core_binary].index = 0;

    /* Log error if snprintf truncated something */
    check_size_exceeds(ret, core_info[total_core_binary].core_path);
    check_size_exceeds(ret, core_info[total_core_binary].binary_name);

    /* get the basename like binary name from whole path */
    ptr = basename(core_info[total_core_binary].binary_name);

    /* Save threshold values */
    snprintf(core_info[total_core_binary].bin_base_name, MAX_SIZE, "%s", ptr);

    MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "core path = '%s' binary name = '%s' max_cores_allowed = '%d' max_core_size = '%d'\n", 
	core_info[total_core_binary].core_path, core_info[total_core_binary].binary_name,  
	core_info[total_core_binary].max_cores_allowed, core_info[total_core_binary].max_core_size);
    total_core_binary++;
  }
}

// change trace level of health monitor
void kw_set_health_monitor_trace_level(char *value)
{
  if(atoi(value) != 0 && atoi(value) >= 1 && atoi(value) <= 4)
  {
    health_monitor_trace_level = atoi(value);
    if(health_monitor_trace_level == 1)
      health_monitor_trace_level = 0X000000FF;
    else if(health_monitor_trace_level == 2)
      health_monitor_trace_level = 0X0000FFFF;
    else if(health_monitor_trace_level == 3)
      health_monitor_trace_level = 0X00FFFFFF;
    else if(health_monitor_trace_level == 4)
      health_monitor_trace_level = 0XFFFFFFFF;
    else 
      health_monitor_trace_level = 0X000000FF;
  }
  MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "HEALTH_MONITOR_TRACE_LEVEL Keyword found trace level = '%d'\n", health_monitor_trace_level);
}

//Check size of files in /tmp directory
void kw_check_max_file_size_in_tmp(char *value)
{
  long long int ret;

  ret = convert_units_in_bytes(value);

  if(ret != -1)
  {
    max_size_of_file_in_tmp = ret;
  }

  MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "MAX_FILE_SIZE_IN_TMP keyword is found threshold max size of file in tmp = '%lld' bytes \n", max_size_of_file_in_tmp);
}

//Check unexpected files in configured directory
void kw_check_unexpected_files_in_dir(char *path, char *value1, char *value2, char *value3)
{
  int i, ret;
  long long int result;
  long long int dir_size_threshold;
  int num_files_threshold;
  int num_dirs;
  int include_sub_dir = 0;
  
  /* Convert file size threshold from 12K/M/G format to bytes */
  result = convert_units_in_bytes(value1);

  /* If user give some unexpected value for dir_size like 10 instead of 10g/m so we set default value to 10MB */
  if(result == -1)
    dir_size_threshold = 10 * 1024 * 1024;
  else
    dir_size_threshold = result;

  /* If user give some negative value or non numeric number for num_files so we set default value to 25 files otherwise if give 0 then take 0*/
  if(value2[0] != '0')
  {
    num_files_threshold = atoi(value2);

    if(num_files_threshold <= 0)
      num_files_threshold = 25;
  }
  else
    num_files_threshold = atoi(value2);

  /* If user give 0/1 value in value3 then store this in include_sub_dir variable otherwise print warning in logs*/
  if(value3[0] != '\0')
  {
    int tmp = atoi(value3);
    if(tmp == 0 || tmp == 1)
      include_sub_dir = tmp;
    else
      MONITOR_TRACE_LOG2("Main_Thread", TL_ERROR, "In CHECK_UNEXPECTED_FILES_IN_DIR keyword User has given value of fourth field '%d' instead "
	                 "of 0/1", tmp);
  }

  /* User can provide NS_WDIR to check unexpected files in directory provided by user on all controllers */
  if(!strncmp(path, "NS_WDIR", 7))
    num_dirs = total_controller;
  else
    num_dirs = 1;

  /* total_unexp_files_path holds value of previously malloced structures */
  size_to_realloc = sizeof(Dir_Info) * (total_unexp_files_path + num_dirs);
  MY_REALLOC(dir_info, size_to_realloc, "Unexpected File Path Table", -1);

  /* Fill info of all dirs in structure */
  for(i = 0; i < num_dirs; i++)
  {
    /* if complete path was provided */
    if(num_dirs == 1)
      ret = snprintf(dir_info[total_unexp_files_path].dir_path, MAX_SIZE, "%s", path);
    /* if NS_WDIR was provided */
    else
      ret = snprintf(dir_info[total_unexp_files_path].dir_path, MAX_SIZE, "%s%s", controller_name[i], path+7);

    /* Log error if snprintf truncated something */
    check_size_exceeds(ret, dir_info[total_unexp_files_path].dir_path);

    /* Save threshold values */
    dir_info[total_unexp_files_path].dir_size_threshold = dir_size_threshold;
    dir_info[total_unexp_files_path].num_files_threshold = num_files_threshold;
    dir_info[total_unexp_files_path].include_sub_dir = include_sub_dir;
    MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "CHECK_UNEXPECTED_FILES_IN_DIR keyword is found, Configured dir path = '%s' threshold "
                       "size of Configured directory='%lld' bytes and max number of files in Configured directory = '%d' and include "
                       "sub directory varibale value is = '%d'", dir_info[total_unexp_files_path].dir_path,
                       dir_info[total_unexp_files_path].dir_size_threshold, dir_info[total_unexp_files_path].num_files_threshold,
                       dir_info[total_unexp_files_path].include_sub_dir);
    total_unexp_files_path++;
  }
}

//Check permission and owner name and group name of a path
void kw_check_permission_and_ownership(char *path, char *permission, char *owner_name, char *grp_name)
{
  //start_thread[MONITOR_PATH] = enable;
  int i;

  //if atleast one value is provided by this keyword then varibles are set otherwise not 
  if(((permission[0] == '\0')||(!strcmp(permission, "NA"))) && ((owner_name[0] =='\0')||(!strcmp(owner_name,"NA"))) && ((grp_name[0] == '\0') || (!strcmp(grp_name, "NA"))))
  {
    return;
  }

  if(!strncmp(path, "NS_WDIR", 7))
  {
    path = path + 7;

    size_to_realloc = (sizeof(Monitor_path) * count_of_path_to_monitor) + (sizeof(Monitor_path) * total_controller);
    MY_REALLOC(monitor_path, size_to_realloc, "Path to monitor", -1);

    for(i=0; i< total_controller; i++)
    {
      sprintf(monitor_path[count_of_path_to_monitor].path_name, "%s%s", controller_name[i], path);
      if(strcmp(permission, "NA"))
      {
	monitor_path[count_of_path_to_monitor].permission = atoi(permission);
	monitor_path[count_of_path_to_monitor].permission_flag = 1;
      }  
      if(strcmp(owner_name, "NA"))
      {
	sprintf(monitor_path[count_of_path_to_monitor].owner_name, "%s", owner_name);
	monitor_path[count_of_path_to_monitor].owner_flag = 1;
      }
      if(strcmp(grp_name, "NA"))
      {
	sprintf(monitor_path[count_of_path_to_monitor].group_name, "%s", grp_name);
	monitor_path[count_of_path_to_monitor].group_flag = 1;
      }

      MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "CHECK_PERMISSON_AND_OWNERSHIP keyword found PATH = '%s' Permission = '%d' Owner = '%s' Group = '%s'\n",  monitor_path[count_of_path_to_monitor].path_name, monitor_path[count_of_path_to_monitor].permission, 
	  monitor_path[count_of_path_to_monitor].owner_name, monitor_path[count_of_path_to_monitor].group_name);
      count_of_path_to_monitor++;
    }   
  }
  else
  {
    size_to_realloc = (sizeof(Monitor_path) * count_of_path_to_monitor) + (sizeof(Monitor_path));
    MY_REALLOC(monitor_path, size_to_realloc, "Path to monitor", -1);

    sprintf(monitor_path[count_of_path_to_monitor].path_name, "%s", path);

    if(strcmp(permission, "NA"))
    {
      monitor_path[count_of_path_to_monitor].permission = atoi(permission);
      monitor_path[count_of_path_to_monitor].permission_flag = 1;
    }  
    if(strcmp(owner_name, "NA"))
    {
      sprintf(monitor_path[count_of_path_to_monitor].owner_name, "%s", owner_name);
      monitor_path[count_of_path_to_monitor].owner_flag = 1;
    }
    if(strcmp(grp_name, "NA"))
    {
      sprintf(monitor_path[count_of_path_to_monitor].group_name, "%s", grp_name);
      monitor_path[count_of_path_to_monitor].group_flag = 1;
    }

    MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "CHECK_PERMISSON_AND_OWNERSHIP keyword found PATH = '%s' Permission = '%d' Owner = '%s' Group = '%s'\n"                                         ,monitor_path[count_of_path_to_monitor].path_name, monitor_path[count_of_path_to_monitor].permission, 
	monitor_path[count_of_path_to_monitor].owner_name, monitor_path[count_of_path_to_monitor].group_name);
    count_of_path_to_monitor++;
  }        
}

//Check Process is running
void kw_check_mandatory_process(char *process_name, char *process_pid_path)
{
  int i, ret = 0;
  int num_dirs;
  char *ptr = NULL;

  /* User can provide NS_WDIR/process_name to check process is running on all controllers */
  //if(!strncmp(process_name, "NS_WDIR", 7))
  //	num_dirs = total_controller;
  //else
  num_dirs = 1;

  /* total_mandatory_process_to_monitor holds value of previously malloced structures */
  size_to_realloc = sizeof(Mandatory_Process) * (total_mandatory_process_to_monitor + num_dirs);
  MY_REALLOC(mandatory_process, size_to_realloc, "Mandatory Process table", -1);

  /* Fill info of all dirs in structure */
  for(i = 0; i < num_dirs; i++)
  {
    /* if complete path was provided */
    if(num_dirs == 1)
      ret = snprintf(mandatory_process[total_mandatory_process_to_monitor].process_name, MAX_SIZE, "%s", process_name);
    /* if NS_WDIR was provided */
    else
      ret = snprintf(mandatory_process[total_mandatory_process_to_monitor].process_name, MAX_SIZE, "%s%s", controller_name[i], process_name+7);

    /* Log error if snprintf truncated something */
    check_size_exceeds(ret, mandatory_process[total_mandatory_process_to_monitor].process_name);

    /*get the basename or last name of process like: /home/cavisson/work/bin/netstorm  get netstorm from this */
    ptr = basename(mandatory_process[total_mandatory_process_to_monitor].process_name);

    snprintf(mandatory_process[total_mandatory_process_to_monitor].process_pid_path, MAX_SIZE, "%s", process_pid_path);

    /* Save threshold values */
    snprintf(mandatory_process[total_mandatory_process_to_monitor].only_mandatory_process_name, MAX_SIZE, "%s", ptr);

    MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "Process name = '%s' \n", mandatory_process[total_mandatory_process_to_monitor].process_name);
    total_mandatory_process_to_monitor++;
  }
}

//CHECK_FILE_CONTENT_CHANGES <0/1> file_name
void kw_file_content_check(char *file_name)
{
  struct stat st;

  if(stat(file_name, &st) == -1)
  {
    MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "Unable to stat file '%s', Error = %s\n", file_name, strerror(errno));
    return ;
  }
  else if(S_ISREG(st.st_mode) == 0)
  {
    MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "Error: File Name '%s' must be regular file.", file_name);
    return;
  }

  //Due to Permission issues or if file size is 0 then st.st_size eqauls 0
  if(!st.st_size)
  {
    MONITOR_TRACE_LOG2("Main_Thread", TL_INFO,  "Error: File Name '%s' has file size either 0 or having permission issue", file_name);
    return;
  }

  long long size_to_realloc;

  /* total_unexp_files_path holds value of previously malloced structures */
  size_to_realloc = sizeof(file_changes_count) * (file_count + 1);
  MY_REALLOC(file_change_struct, size_to_realloc, "file changes count", -1);


  sprintf(file_change_struct[file_count].file_name, "%s", file_name);
  file_change_struct[file_count].file_size = st.st_mtime;
  file_change_struct[file_count].lmd = st.st_size;

  MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "CHECK_FILE_CONTENT_CHANGES file: '%s'", file_name);

  file_count ++;
  start_thread[FILE_LMD_FLAG] = 1;
}

//Check Process is not running
void kw_check_unwanted_process(char *process_name)
{
  int i, ret = 0;
  int num_dirs;
  char *ptr = NULL;

  /* User can provide NS_WDIR/process_name to check process is running or not on all controllers */
  //if(!strncmp(process_name, "NS_WDIR", 7))
  //	num_dirs = total_controller;
  //else
  num_dirs = 1;

  /* total_unwanted_process_to_monitor holds value of previously malloced structures */
  size_to_realloc = sizeof(Unwanted_Process) * (total_unwanted_process_to_monitor + num_dirs);
  MY_REALLOC(unwanted_process, size_to_realloc, "Unwanted Process table", -1);

  /* Fill info of all dirs in structure */
  for(i = 0; i < num_dirs; i++)
  {
    /* if complete path was provided */
    if(num_dirs == 1)
      ret = snprintf(unwanted_process[total_unwanted_process_to_monitor].process_name, MAX_SIZE, "%s", process_name);
    /* if NS_WDIR was provided */
    else
      ret = snprintf(unwanted_process[total_unwanted_process_to_monitor].process_name, MAX_SIZE, "%s%s", controller_name[i], process_name+7);

    /* Log error if snprintf truncated something */
    check_size_exceeds(ret, unwanted_process[total_unwanted_process_to_monitor].process_name);

    /*get the basename or last name of process like: /home/cavisson/work/bin/netstorm  get netstorm from this */
    ptr = basename(unwanted_process[total_unwanted_process_to_monitor].process_name);

    /* Save threshold values */
    snprintf(unwanted_process[total_unwanted_process_to_monitor].only_unwanted_process_name, MAX_SIZE, "%s", ptr);

    MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "Process name = '%s' \n", unwanted_process[total_unwanted_process_to_monitor].process_name);
    total_unwanted_process_to_monitor++;
  }
}

//Check ndp and nddbu progress
void kw_check_ndp_dbu_progress(char *value)
{
  long long int ret;

  /* copy the path to test path variable */
  //snprintf(test_path, 512, "%s", path);

  /* If user give some negative value or zero for test number then we add logs in monitor log*/
  /*if(atoi(test_number) > 0)
  {
    test_no = atoi(test_number);
  }
  else
  {
    MONITOR_TRACE_LOG2("Main_Thread", TL_ERROR, "User give no value in test_number variable, test_number = '%s' \n", test_number);
    return;
  }*/

  /* Convert bytes_remaining_threshold from 12K/M/G format to bytes */
  ret = convert_units_in_bytes(value);

  /* If user give some unexpected value for bytes_remaining_threshold like 10 instead of 10g/m so we set default value to 1GB */
  if(ret != -1)
  {
    bytes_remaining_threshold = ret;
  }

  MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "CHECK_NDP_DBU_PROGRESS Keyword found threshold value = '%lld' bytes \n", bytes_remaining_threshold);
}


//Check packet size of testrun.gdf file
void kw_check_rtg_packet_size(char *value, char *value1)
{
  long long int result;

  /* copy the path to rtg_test_path variable */
  //snprintf(rtg_test_path, 512, "%s", path);

  /* If user give some negative value or zero for test number then we add logs in monitor log*/
  /*if(atoi(test_run) > 0)
    {
    rtg_test = atoi(test_run);
    }
    else
    {
    MONITOR_TRACE_LOG2("Main_Thread", TL_ERROR, "User give no value in test_run variable, test_run = '%s' \n", test_run);
    return;
    }*/

  /* Convert thres_packet_size from 12K/M/G format to bytes */
  result = convert_units_in_bytes(value);

  /* If user give some unexpected value for thres_packet_size like 10 instead of 10g/m so we set default value to 3MB */
  if(result != -1)
    thres_packet_size = result;

  /* If user give some negative value or non numeric number for gdf_threshold_count so we set default value to 2 files otherwise if give 0 then take 0*/
  if(value1[0] != '0')
  {
    gdf_threshold_count = atoi(value1);

    if(gdf_threshold_count <= 0)
      gdf_threshold_count = 120;
  }
  else
    gdf_threshold_count = atoi(value1);


  MONITOR_TRACE_LOG2("Main_Thread", TL_INFO,"CHECK_RTG_PACKET_SIZE Keyword found threshold packet size = '%lld', theshold gdf files count = '%d'\n", thres_packet_size, gdf_threshold_count);
}


void kw_check_metadata_file_size(char *buff3, char *buff4, char *buff5)
{
  if(buff3[0]!=0)
    max_file_size = convert_units_in_bytes(buff3); 

  if(buff4[0]!=0)
    max_sleep_time = check_sleep_interval_in_min_sec_or_hr(buff4);

  if(buff5[0]!=0)
    metadata_max_mail_time = check_sleep_interval_in_min_sec_or_hr(buff5);  

  MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, 
      "CHECK_METADATA_FILE_SIZE keyword found File size check : '%lld' sleep time : '%lld' mail send time : '%lld' \n",
      max_file_size, max_sleep_time, metadata_max_mail_time);
}


void kw_db_dup_file_check(char *buff3, char *buff4)
{
  if(buff3[0]!=0)
    db_dup_sleep_time = check_sleep_interval_in_min_sec_or_hr(buff3);

  if(buff4[0]!=0)
    db_dup_mail_time = check_sleep_interval_in_min_sec_or_hr(buff4);

  MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "CHECK_DB_DUPLICATE_FILE keyword found sleep time: '%lld' mail time: '%lld' \n",
      db_dup_sleep_time, db_dup_mail_time);
}


//CHECK_HEAP_DUMP <0/1> <max_file> <max_old_days> <mail_time[s/m/h]>
void kw_heap_dump_check(char *buff2, char *buff3, char *buff4)
{
  if(atoi(buff2) > 0)
    max_heap_dump = atoi(buff2);

  if(atoi(buff3) > 0)
    max_days_heap_dump = atoi(buff3);

  if(buff4[0]!=0)
    heap_dump_sleep_time = check_sleep_interval_in_min_sec_or_hr(buff4);

  if(heap_dump_sleep_time <= 0)
     heap_dump_sleep_time = 4*60*60;

  MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "CHECK_HEAP_DUMP keyword found max file: '%d' max old days: '%d' mail_time: '%ld'\n",
      max_heap_dump, max_days_heap_dump, heap_dump_sleep_time);
}


void kw_set_mail_recipent_list(char *buff)
{
  send_mail = true;
  mail_recipient_list = (char*) malloc(strlen(buff) + 1);
  strcpy(mail_recipient_list, buff);
  MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "MAIL_RECIPIENT_LIST Keyword found list = '%s' \n" , buff);
}

void kw_set_mail_cc_list(char *buff)
{
  send_mail_cc = true;
  send_mail = true;
  mail_cc_list = (char*) malloc(strlen(buff) + 1);
  strcpy(mail_cc_list, buff);
  MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "MAIL_CC_LIST Keyword found list = '%s' \n" , buff);
}

void disable_for_no_test()
{
  start_thread[CONSTRAINT_FLAG] = 0;
  start_thread[CHECK_NDP_DBU_PROGRESS_FLAG] = 0;
  start_thread[RTG_PACKET_SIZE_FLAG] = 0; 
  start_thread[TEST_RUN_STATUS_FLAG] = 0; 
  start_thread[METADATA_SIZE_CHECK_FLAG] = 0; 
  start_thread[DB_DUP_FILE_CHECK] = 0; 
}

void read_config_ini_file()
{
  FILE *fp = NULL;
  char command[1024];
  char config_ini_path[1024];
  char buff[1024];
  int ret;
  struct stat s;

  MONITOR_TRACE_LOG2("read_config_ini_file", TL_INFO, "Entered reading config.ini file \n");

  sprintf(config_ini_path,"%s/webapps/sys/config.ini",ns_wdir);

  if(stat(config_ini_path, &s))
  { 
    MONITOR_TRACE_LOG1("read_config_ini_file", TL_ERROR, "File '%s' does not exists\n" , config_ini_path);
    disable_for_no_test();
  }
  else
  {
    sprintf(command, "cat %s | grep testRunNum | cut -d '=' -f2", config_ini_path);

    ret = run_cmd(&fp, command);

    if(ret == -1)
    {
      MONITOR_TRACE_LOG2("read_config_ini_file", TL_ERROR, "Unable to run command : '%s'\n", command);
      disable_for_no_test();
    }
    else
    {
      if(fgets(buff, 1024, fp) != NULL)
      {

	sscanf(buff, "%d", &test_run_no);

	if( test_run_no > 0 )
	{ 
	  MONITOR_TRACE_LOG2("read_config_ini_file", TL_INFO, "Test run no. = '%d' in file '%s' \n", test_run_no, config_ini_path);
	  test_run_no_flag = 1;
	  sprintf(ndp_pid_path, "%s/logs/TR%d/.ndp.pid", ns_wdir, test_run_no);
	  sprintf(nsu_db_upload_pid_path, "%s/logs/TR%d/.nsu_db_upload.pid", ns_wdir, test_run_no);
	  sprintf(ndu_db_upload_pid_path, "%s/logs/TR%d/.ndu_db_upload.pid", ns_wdir, test_run_no);
	  sprintf(log_rd_pid_path, "%s/logs/TR%d/.nlr.pid", ns_wdir, test_run_no);
	  sprintf(log_wr_pid_path, "%s/logs/TR%d/.nlw.pid", ns_wdir, test_run_no);
	  sprintf(event_log_pid_path, "%s/logs/TR%d/.nlm.pid", ns_wdir, test_run_no);

	}
	else
	{
	  MONITOR_TRACE_LOG2("read_config_ini_file", TL_ERROR, "Test run no. = '%d' written the config.ini file is invalid" ,test_run_no);
	  disable_for_no_test();
	}
      }
      else
      {
	disable_for_no_test();

	pthread_mutex_lock(&thread_mutex);

	sprintf(sub_of_mail, "Critical | %s | CSHM | %s | %s | Session Number not Defined in config.ini", machine_mode,machine_name,my_controller);
	sprintf(msg, "Session number is not defined in config.ini.\nController level checks will not be performed by Cavisson System Health Monitor (CSHM). You should restart the CSHM in case you write the session no. in the file");
	MONITOR_TRACE_LOG1("Msg send for read_config_ini_file", TL_ERROR, "Session is not defined in config.ini as there is no test run no. in the file.You should restart the health check monitor in case you write the test run no. in the file \n");
	send_mail_to_user(sub_of_mail, msg, NULL);

	pthread_mutex_unlock(&thread_mutex);
      }
    }
    if(fp)
      pclose(fp);
  }
}


//Read cav.conf file
void read_cav_conf_file()
{
  FILE *conf = NULL;
  char read_buf[1024], *fields[3];
  int total_fields = 0;
  char cav_conf_path[512];

  if(!strcmp(my_controller, "work"))
  {
    strcpy(cav_conf_path, "/home/cavisson/etc/cav.conf");
  }
  else
  {
    sprintf(cav_conf_path, "/home/cavisson/etc/cav_%s.conf",my_controller);
  }

  MONITOR_TRACE_LOG2("read_cav_conf_file", TL_INFO, "my controller name = '%s' \n ",my_controller);
  MONITOR_TRACE_LOG2("read_cav_conf_file", TL_INFO, "my cav conf path = '%s' \n ",cav_conf_path);

  conf = fopen (cav_conf_path, "r");

  if(!conf)
  {
    MONITOR_TRACE_LOG1("Main_Thread", TL_ERROR, "Unable to open '/home/cavisson/etc/cav.conf' file error = %s\n" , nslb_strerror(errno));
    exit(-1);
  }

  while (fgets(read_buf, 1024, conf))
  {
    if (*read_buf == '#' ||  *read_buf == ' ' || *read_buf == '\t' || *read_buf == '\n' || 
	*read_buf == '\r')
    {
      continue;
    }

    if((read_buf[0] != '\0') && (!strncmp(read_buf, "CONFIG", 6)))
    {
      total_fields = get_tokens_with_multi_delimiter(read_buf, fields, " ", 3);

      if(total_fields < 2)
      {
	MONITOR_TRACE_LOG2("Main_Thread", TL_ERROR, "Fields are less than 2 in '/etc/cav.conf'\n");
	exit(-1);
      }
      sprintf(machine_mode, "%s", fields[1]);
      machine_mode[strlen(machine_mode) - 1] = '\0';
      MONITOR_TRACE_LOG1("Main_Thread", TL_INFO, "Machine Mode = '%s'\n", machine_mode);
      break;
    }
    else 
      continue;
  }
}

//Read configuration file
void read_conf_file()
{
  FILE *configfile = NULL;
  char keyword [MAX_READ_LINE_LENGTH];
  char buff[MAX_READ_LINE_LENGTH];
  char buff2[MAX_READ_LINE_LENGTH];
  char buff3[MAX_READ_LINE_LENGTH];
  char buff4[MAX_READ_LINE_LENGTH];
  char buff5[MAX_READ_LINE_LENGTH];
  char buff6[MAX_READ_LINE_LENGTH];
  char read_buffer[MAX_READ_LINE_LENGTH] = "";
  char *read_buf = read_buffer;
  int num;

  configfile = fopen(conf_file_path, "r");

  if(configfile == NULL)
  {  
    MONITOR_TRACE_LOG1("Main_Thread", TL_ERROR, "Unable to open conf file '%s' at following path error = %s\n" , conf_file_path, 
	nslb_strerror(errno));
    exit(-1);
  }

  while((fgets(read_buf, MAX_READ_LINE_LENGTH, configfile)) != NULL)
  {
    keyword [0] = '\0';
    buff[0] = '\0';
    buff2[0] = '\0';
    buff3[0] = '\0';
    buff4[0] = '\0';
    buff5[0] = '\0';
    buff6[0] = '\0';



    //moves pointer ahead of spaces or tab 
    CLEAR_WHITE_SPACE(read_buf);



    // Ignore commented lines, spaces, tabs, carriage returns and newline

    if (*read_buf == '#' ||  *read_buf == ' ' || *read_buf == '\t' || *read_buf == '\n' || *read_buf == '\r')
      continue;
    MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "Conf Line read: '%s'", read_buf);

    if((num = sscanf(read_buf, "%s %s %s %s %s %s %s", keyword, buff, buff2, buff3, buff4, buff5, buff6)) < 2)
    {
      MONITOR_TRACE_LOG1("Main_Thread", TL_ERROR, "Error in  keyword = '%s' and text after it = '%s' \n", keyword, buff);
      continue;
    }

    if(!strcmp(keyword, "ENABLE_TMP_DIR_SCAN"))
    {
      if(ns_is_numeric(buff) != 0)
      {
        if(atoi(buff) == 1 || atoi(buff) == 0)
        {
           enable_tmp_dir_scanning = atoi(buff);
           MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "Keyword 'ENABLE_TMP_DIR_SCAN' found value = '%d'\n", enable_tmp_dir_scanning);
        }
        else
        {
          MONITOR_TRACE_LOG2("Main_Thread", TL_ERROR, "wrong value '%s' for Keyword 'ENABLE_TMP_DIR_SCAN' value must either 0 or 1.Setting it with default value %d \n",  buff, enable_tmp_dir_scanning);
        }
      }
    }

    //ENABLE_HEALTH_MONITOR <0/1> <time in sec>
    if(!strcmp(keyword, "ENABLE_HEALTH_MONITOR"))
    {
      if(ns_is_numeric(buff) != 0)
      {
        if(atoi(buff) == 1 || atoi(buff) == 0)
        {
	  enable_health_monitor = atoi(buff);
          if(ns_is_numeric(buff2) != 0 && atoi(buff2) > 0)
            dis_cshm_mail_time = atoi(buff2);
          MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "ENABLE_HEALTH_MONITOR keyword found enable = '%d' , mail time = '%d' \n", enable_health_monitor, dis_cshm_mail_time);
        }
      }
    }



    //ENABLE_SYSTEM_LEVEL_HEALTH <0/1> <idle_cpu_threshold[0-100]> <load_avg_threshold[0-100]> <cache_threshold[0-100]> <disk_threshold[0-100]> <time in sec>
    if(!strcmp(keyword, "ENABLE_SYSTEM_LEVEL_HEALTH"))
    {
      if(ns_is_numeric(buff) != 0)
      {
        if(atoi(buff) == 1 || atoi(buff) == 0)
        {
          enable_system_level_health = atoi(buff);
          if(ns_is_numeric(buff2) != 0 && atoi(buff2) >= 0 && atoi(buff2) <= 100)
            idle_cpu_threshold = atoi(buff2);
          if(ns_is_numeric(buff3) != 0 && atoi(buff3) >= 0 && atoi(buff3) <= 100)
            load_avg_over_5min_threshold = atoi(buff3);
          if(ns_is_numeric(buff4) != 0 && atoi(buff4) >= 0 && atoi(buff4) <= 100)
            cache_memory_threshold = atoi(buff4);
          if(ns_is_numeric(buff5) != 0 && atoi(buff5) >= 0 && atoi(buff5) <= 100)
            disk_free_space_threshold = atoi(buff5);
          if(ns_is_numeric(buff2) != 0 && atoi(buff2) >= 0)
            system_level_monitor_sleep_time = atoi(buff6);
          kw_enable_system_level_health(enable_system_level_health);
          MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "ENABLE_SYSTEM_LEVEL_HEALTH keyword found enable = %d, cpu = %d, load = %d, mem =%d, disk = %d, sleep = %d \n" , enable_system_level_health, idle_cpu_threshold, load_avg_over_5min_threshold,cache_memory_threshold, disk_free_space_threshold, system_level_monitor_sleep_time);
        }
      }
    }


    if(!strcmp(keyword, "SET_MACHINE_NAME"))
    {
      kw_check_machine_name(buff);
    }

    //COREINFO <0/1> <core_path> <binary_name> <max_cores_allowed> <max_core_size[in mb]>
    if(!strcmp(keyword, "COREINFO"))
    {
      if(ns_is_numeric(buff) != 0)
      {
        if(atoi(buff) == 1 || atoi(buff) == 0)
        {
          start_thread[CORE_FLAG] = atoi(buff);
          if(atoi(buff) == 1 && strcmp(buff2,"1") != 0)
	    kw_set_core_info(buff2, buff3, buff4, buff5);
        }
      }
    }

    if(!strcmp(keyword, "HEALTH_MONITOR_TRACE_LEVEL"))
    {
      kw_set_health_monitor_trace_level(buff);
    }

    else if(!strcmp(keyword, "MAIL_RECIPIENT_LIST"))
    {
      kw_set_mail_recipent_list(buff);
    }

    else if(!strcmp(keyword, "MAIL_CC_LIST"))
    {
      kw_set_mail_cc_list(buff);
    }

    else if(!strcmp(keyword, "DUMP_BT_IN_FILE"))
    {
      kw_set_bt_flag(buff);
    }

    else if(!strcmp(keyword, "CORE_DELAY_TIME"))
    {
      kw_set_core_delay_time(buff);
    }

    else if(!strcmp(keyword, "CSHM_THREAD_DELAY_TIME"))
    {
      if(ns_is_numeric(buff) != 0)
      {
        if(atoi(buff) >= 0)
          cshm_thread_delay = atoi(buff);
        MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "CSHM_THREAD_DELAY_TIME keyword found delay = '%d' \n", cshm_thread_delay);
      }
    }

    //FILE_SYSTEM_DELAY_TIME values[s/m/h]
    else if(!strcmp(keyword, "FILE_SYSTEM_DELAY_TIME"))
    {
      kw_set_filesystem_delay_time(buff);
    }

    else if(!strcmp(keyword, "REL_DIR_DELAY_TIME"))
    {
      kw_set_rel_dir_delay_time(buff);
    }

    else if(!strcmp(keyword, "MANDATORY_PROCESS_DELAY_TIME"))
    {
      kw_set_mandatory_process_delay_time(buff);
    }

    else if(!strcmp(keyword, "MANDATORY_PROCESS_ALERT_FREQUENCY"))
    {
      kw_set_mandatory_process_alert_frequency(buff);
    }

    else if(!strcmp(keyword, "UNWANTED_PROCESS_DELAY_TIME"))
    {
      kw_set_unwanted_process_delay_time(buff);
    }

    else if(!strcmp(keyword, "POSTGRESQL_DELAY_TIME"))
    {
      kw_set_postgresql_delay_time(buff);
    }

    else if(!strcmp(keyword, "PERMISSION_AND_OWNERSHIP_CHECK_DELAY_TIME")) 
    {
      kw_set_permission_and_ownership_check_delay_time(buff);
    }

    else if(!strcmp(keyword, "MISSING_CONSTRAINT_DELAY_TIME"))
    {
      kw_set_missing_constraint_delay_time(buff);
    }

    else if(!strcmp(keyword, "MAX_FILE_SIZE_DELAY_TIME"))
    {
      kw_set_max_file_size_delay_time(buff);
    }

    else if(!strcmp(keyword, "UNEXPECTED_FILES_IN_DIR_DELAY_TIME"))
    {
      kw_set_unexpected_files_in_dir_delay_time(buff);
    }

    else if(!strcmp(keyword, "NDP_DBU_PROGRESS_DELAY_TIME"))
    {
      kw_set_ndp_dbu_progress_delay_time(buff);
    }

    else if(!strcmp(keyword, "TEST_RUN_DELAY_TIME"))
    {
      kw_set_test_run_delay_time(buff);
    }

    else if(!strcmp(keyword, "RTG_DELAY_TIME"))
    {
      kw_set_rtg_delay_time(buff);
    }

    else if(!strcmp(keyword, "SLEEP_TIME"))
    {
      kw_set_mainthread_sleep_time(buff);
    }


    else if(!strcmp(keyword, "FILE_SYSTEM_THRESHOLD"))
    {
      if(ns_is_numeric(buff) != 0)
      {
        if(atoi(buff) == 1 || atoi(buff) == 0)
        {
	  start_thread[TMPFS_FLAG] = atoi(buff);
          if(atoi(buff) == 1 && strcmp(buff2,"1") != 0)
	    kw_set_fs_threshold(buff2, buff3, buff4);
        }
      }
    }

    else if(!strncmp(keyword, "CHECK_FILE_CONTENT_CHANGES", strlen("CHECK_FILE_CONTENT_CHANGES")))
    {
      if(ns_is_numeric(buff) != 0)
      {
        if(atoi(buff) == 1 || atoi(buff) == 0)
        {
          if(atoi(buff) == 1)
            kw_file_content_check(buff2);
        }
      }
    }

    else if(!strcmp(keyword, "NUM_BUILDS_THRESHOLD_IN_REL_DIR"))
    {
      if(ns_is_numeric(buff) != 0)
      {
        if(atoi(buff) == 1 || atoi(buff) == 0)
        {
	  start_thread[REL_FLAG] = atoi(buff);
          if(atoi(buff) == 1)
	    kw_set_num_builds_threshold_in_rel_dir(buff2);
        }
      }
    }

    else if(!strcmp(keyword, "CHECK_POSTGRESQL_MONITOR"))
    {
      if(ns_is_numeric(buff) != 0)
      {
        if(atoi(buff) == 1 || atoi(buff) == 0)
        {
	  start_thread[POSTGRES_FLAG] = atoi(buff);
          if(atoi(buff) == 1)
	    kw_check_postgresql_monitor(buff, buff2, buff3);
        }
      }
    }

    else if(!strcmp(keyword, "CHECK_MANDATORY_PROCESS"))
    { 
      if(ns_is_numeric(buff) != 0)
      {
        if(atoi(buff) == 1 || atoi(buff) == 0)
        {
	  start_thread[MANDATORY_PROCESS_FLAG] = atoi(buff);
          if(atoi(buff) == 1)
          {
            if(strcmp(buff2,ndc_path) != 0 && strcmp(buff2,ndp_path) != 0 && strcmp(buff2,nsu_db_upload_path) != 0 && strcmp(buff2,ndu_db_upload_path) !=0  && strcmp(buff2,hpd_path) != 0 && strcmp(buff2,lps_path) !=0 && strcmp(buff2,netstorm_path) != 0)
            {
              if(strcmp(buff2,tomcat_path) != 0 && strcmp(buff2,postgresql_path) != 0 && strcmp(buff2,log_rd_path) != 0 && strcmp(buff2,log_wr_path) != 0 && strcmp(buff2,event_log_path) != 0 && strcmp(buff2,cmon_path) != 0 && strcmp(buff2,"1") != 0)    
	        kw_check_mandatory_process(buff2, buff3);
            }
          }
        }
      }
    }    

    else if(!strcmp(keyword, "CHECK_UNWANTED_PROCESS"))
    {
      if(ns_is_numeric(buff) != 0)
      {
        if(atoi(buff) == 1 || atoi(buff) == 0)
        {
	  start_thread[UNWANTED_PROCESS_FLAG] = atoi(buff);
          if(atoi(buff) == 1 && strcmp(buff2,"1") != 0)
	    kw_check_unwanted_process(buff2);
        }
      }
    }

    else if(!strcmp(keyword, "CHECK_MISSING_CONSTRAINT"))
    {
      if(ns_is_numeric(buff) != 0)
      {
        if(atoi(buff) == 1 || atoi(buff) == 0)
        {
	  start_thread[CONSTRAINT_FLAG] = atoi(buff);
          MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "CHECK_MISSING_CONSTRAINT keyword found enable = '%d' \n", start_thread[CONSTRAINT_FLAG]);
        }
      }
    }
   
    else if(!strcmp(keyword, "CHECK_CLEANUP_STATUS"))
    {
      if(ns_is_numeric(buff) != 0)
      {
        if(atoi(buff) == 1 || atoi(buff) == 0)
          start_thread[CLEANUP_STATUS_FLAG] = atoi(buff);
        MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "CHECK_MISSING_CONSTRAINT keyword found enable = '%d' \n", start_thread[CLEANUP_STATUS_FLAG]);
      }
    }

    else if(!strcmp(keyword, "CORE_DIR_MAX_SIZE"))
    { 
      if(ns_is_numeric(buff) != 0 && ns_is_numeric(buff2) != 0)
      { 
        if(atoi(buff) > 0)
          threshold_size_of_core_dir = atoi(buff);
        if(atoi(buff2) > 0)
          threshold_timestamp_diff_days = atoi(buff2);
        MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "CORE_DIR_MAX_SIZE keyword found threshold size of core directory = '%d' Mb\n", threshold_size_of_core_dir);
      }
    }

    else if(!strcmp(keyword, "MAX_FILE_SIZE_IN_TMP"))
    {
      if(ns_is_numeric(buff) != 0)
      {
        if(atoi(buff) == 1 || atoi(buff) == 0)
        {
	  start_thread[MAX_FILE_SIZE_FLAG] = atoi(buff);
          if(atoi(buff) == 1 && strcmp(buff2,"1") != 0)
	    kw_check_max_file_size_in_tmp(buff2);
        }
      }
    }

    else if(!strcmp(keyword, "CHECK_UNEXPECTED_FILES_IN_DIR"))
    {
      if(ns_is_numeric(buff) != 0)
      {
        if(atoi(buff) == 1 || atoi(buff) == 0)
        {
	  start_thread[CHECK_UNEXPETED_FILE_IN_DIR_FLAG] = atoi(buff);
          if(atoi(buff) == 1)
	    kw_check_unexpected_files_in_dir(buff2, buff3, buff4, buff5);
        }
      }
    }

    else if(!strcmp(keyword, "CHECK_NDP_DBU_PROGRESS"))
    {
      if(ns_is_numeric(buff) != 0)
      {
        if(atoi(buff) == 1 || atoi(buff) == 0)
        {
	  start_thread[CHECK_NDP_DBU_PROGRESS_FLAG] = atoi(buff);
          if(atoi(buff) == 1 && strcmp(buff2,"1") != 0)
	    kw_check_ndp_dbu_progress(buff2);
        }
      }
    }
    
    //CHECK_RTG_PACKET_SIZE <0/1> <packet size threshold[b/k/m/g]> <threshold of testrun.gdf files>
    else if(!strcmp(keyword, "CHECK_RTG_PACKET_SIZE"))
    { 
      if(ns_is_numeric(buff) != 0)
      {
        if(atoi(buff) == 1 || atoi(buff) == 0)
        {
	  start_thread[RTG_PACKET_SIZE_FLAG] = atoi(buff);    
          if(atoi(buff) == 1)  
	    kw_check_rtg_packet_size(buff2, buff3);
        }
      }
    }

    else if(!strcmp(keyword, "CHECK_METADATA_FILE_SIZE"))
    {
      if(ns_is_numeric(buff) != 0)
      {
        if(atoi(buff) == 1 || atoi(buff) == 0)
        {
 	  start_thread[METADATA_SIZE_CHECK_FLAG] = atoi(buff);
          if(atoi(buff) == 1)
	    kw_check_metadata_file_size(buff2, buff3, buff4);
        }
      }
    }
    else if(!strcmp(keyword, "CHECK_DB_DUPLICATE_FILE"))
    {
      if(ns_is_numeric(buff) != 0)
      {
        if(atoi(buff) == 1 || atoi(buff) == 0)
        {
	  start_thread[DB_DUP_FILE_CHECK] = atoi(buff);
          if(atoi(buff) == 1)
	    kw_db_dup_file_check(buff2, buff3);
        }
      }
    }
    
    //CHECK_HEAP_DUMP <0/1> <max_file> <max_old_days> <mail_time[s/m/h]>
    else if(!strcmp(keyword, "CHECK_HEAP_DUMP"))
    {
      if(ns_is_numeric(buff) != 0)
      {
        if(atoi(buff) == 1 || atoi(buff) == 0)
        {
          start_thread[CHECK_HEAP_DUMP_FLAG] = atoi(buff);
          if(atoi(buff) == 1)
            kw_heap_dump_check(buff2, buff3, buff4);
        }
      }
    }
    
    //IOTOP_CAPTURE_SETTING <0/1> <thread level> <log interval count>
    else if(!strcmp(keyword, "IOTOP_CAPTURE_SETTING"))
    {
      if(atoi(buff) == 0)
        show_iotop_output = false;
      
      if(atoi(buff2) == 1)
        show_iotop_thread_output = true;
      
      if(ns_is_numeric(buff3) != 0)
        iotop_log_interval = atoi(buff3);
    }

  }
}

//This function will save all the control_name from /etc/cav_controller.conf
void find_all_controller_name()
{
  FILE *conf = NULL;
  char read_buf[1024], *fields[7];
  int total_fields = 0;

  conf = fopen ("/home/cavisson/etc/cav_controller.conf", "r");

  if (!conf)
  {
    MONITOR_TRACE_LOG1("Main_Thread", TL_ERROR, "Unable to open '/etc/cav_controller.conf' file error = %s\n" , nslb_strerror(errno));
    exit(-1);
  }

  while (fgets(read_buf, 1024, conf))
  {
    if (*read_buf == '#' ||  *read_buf == ' ' || *read_buf == '\t' || *read_buf == '\n' || 
	*read_buf == '\r' || !strncmp(read_buf, "NAME|", 5))
    {
      continue;
    }

    total_fields = get_tokens_with_multi_delimiter(read_buf, fields, "|", 7);
    if(total_fields < 3)
    {
      MONITOR_TRACE_LOG2("Main_Thread", TL_ERROR, "Fields are less than 3 in '/etc/cav_controller.conf'\n");
      exit(-1);
    }
    controller_name[total_controller] = (char*) malloc(strlen(fields[2]) + 1);
    strcpy(controller_name[total_controller], fields[2]);
    MONITOR_TRACE_LOG1("Main_Thread", TL_INFO, "Controller name = '%s'\n", controller_name[total_controller]);
    total_controller++;
  }
  MONITOR_TRACE_LOG1("Main_Thread", TL_INFO, "Total Controller = '%d'\n", total_controller);
}

void send_mail_to_user(char *sub_of_mail, char *msg, char *core_file)
{
  FILE *fp = NULL;
  char buf[MAX_READ_LINE_LENGTH];
  char temp_buf[MAX_DATA_LENGTH];
  char buffer[26];
  int ret;
  int ret_val;
  time_t mail_dt;
  struct tm* tm_info, tm_struct;
  time(&mail_dt);
  tm_info = nslb_localtime(&mail_dt, &tm_struct, 1);
  strftime(buffer, 26, "%Y/%m/%d %H:%M:%S %Z", tm_info);

  sprintf(temp_buf, "lsb_release -r | cut -f2");

  ret_val = run_cmd(&fp, temp_buf);

  if(ret_val == -1)
  {
    MONITOR_TRACE_LOG1("ubuntu_release", TL_ERROR, "Unable to get ubuntu release\n");
    return;
  }

  fgets(temp_buf, MAX_DATA_LENGTH, fp);

  if(!strncmp(temp_buf, "12.", 3))
  {
    if(send_mail_cc)
    {
      if(core_file)
        sprintf(buf, "mail -s '%s' -c '%s' '%s' < %s", sub_of_mail, mail_cc_list, mail_recipient_list, core_file);
      else
        sprintf(buf, "mail -s '%s' -c '%s' '%s' << +\n****This is a system generated email. Please do not respond to this email.****\n\n\nAlert Time: %s\n\n%s\nThis is for your information and take necessary action if required.\n\n\n\n\n\nFor any feedback or query on health monitoring tool, please send an email to client-support@cavisson.com with \"Cavisson System Health Monitor - Feedback\" in the subject line.\n+", sub_of_mail, mail_cc_list, mail_recipient_list, buffer, msg);
    }
    else
    {
      if(core_file)
        sprintf(buf, "mail -s '%s' '%s' < %s", sub_of_mail, mail_recipient_list, core_file);
      else
        sprintf(buf, "mail -s '%s' '%s' << +\n****This is a system generated email. Please do not respond to this email.****\n\n\nAlert Time: %s\n\n%s\nThis is for your information and take necessary action if required.\n\n\n\n\n\nFor any feedback or query on health monitoring tool, please send an email to client-support@cavisson.com with \"Cavisson System Health Monitor - Feedback\" in the subject line.\n+", sub_of_mail, mail_recipient_list, buffer, msg);
    }
  }
  else if(!strncmp(temp_buf, "16.", 3))
  {
    if(send_mail_cc)
    {
      if(core_file)
        sprintf(buf, "mail -s '%s' -a 'Cc:%s' '%s' < %s", sub_of_mail, mail_cc_list, mail_recipient_list, core_file);
      else
        sprintf(buf, "mail -s '%s' -a 'Cc:%s' '%s' << +\n****This is a system generated email. Please do not respond to this email.****\n\n\nAlert Time: %s\n\n%s\nThis is for your information and take necessary action if required.\n\n\n\n\n\nFor any feedback or query on health monitoring tool, please send an email to client-support@cavisson.com with \"Cavisson System Health Monitor - Feedback\" in the subject line.\n+", sub_of_mail, mail_cc_list, mail_recipient_list, buffer, msg);
   }
    else
    {
      if(core_file)
        sprintf(buf, "mail -s '%s' '%s' < %s", sub_of_mail, mail_recipient_list, core_file);
      else
        sprintf(buf, "mail -s '%s' '%s' << +\n****This is a system generated email. Please do not respond to this email.****\n\n\nAlert Time: %s\n\n%s\nThis is for your information and take necessary action if required.\n\n\n\n\n\nFor any feedback or query on health monitoring tool, please send an email to client-support@cavisson.com with \"Cavisson System Health Monitor - Feedback\" in the subject line.\n+", sub_of_mail, mail_recipient_list, buffer, msg);
    }
  }
  else 
    MONITOR_TRACE_LOG1("Ubuntu Version Failure ", TL_ERROR, "Unable to match any of the ubuntu version & release i.e., 12.04 and 16.04\n");

  if(fp)
    pclose(fp);
  
  if(send_mail)
  {
    ret = run_cmd(&fp, buf);

    if(ret == 0)
      MONITOR_TRACE_LOG2("send_mail", TL_INFO, "Mail with subject [%s] send successfully\n", sub_of_mail);

    if(ret == -1)
    {
      MONITOR_TRACE_LOG1("send_mail", TL_ERROR, "Unable to send mail with subject [%s]\n", sub_of_mail);
      return;
    }

    if(fp)
      pclose(fp);
  }
  else
  {
    MONITOR_TRACE_LOG2("send_mail", TL_ERROR, "Unable to send mail with subject '[%s]' as the e-mail list is empty \n", sub_of_mail);
  }

  memset(&msg[0], 0, MAX_READ_LINE_LENGTH);
  memset(&sub_of_mail[0], 0, MAX_SIZE);
   
}

int main(int argc, char **argv)
{
  int i, ret, conf_file = 0;
  char full_path[MAX_DATA_LENGTH]; //128
  char full_buff[MAX_DATA_LENGTH]; //128
  char command[MAX_DATA_LENGTH];   //128
  char tomcat_wdir[512];
  char buff[1024];
  FILE *fp = NULL;
  char c;
  struct stat s;
  struct stat ts;
  char cwd[100];

 //checking conf file path in arguments
  while ((c = getopt(argc, argv, "c:")) != -1) 
  {
    switch (c) 
    {
      case 'c':
        snprintf(conf_file_path, 512, "%s", optarg);
        conf_file = 1;
        break;
      case ':':
      case '?':
        fprintf(stderr, "Usage: ./nsu_system_health_check -c [conf_file_path] \n");
        exit(0);
     }
  }

  //taking NS_WDIR into global variable ns_wdir
  if(getenv("NS_WDIR") != NULL)
  {
    strncpy(ns_wdir, getenv("NS_WDIR"), 511);
    ns_wdir[511] = '\0';
    sprintf(command, "basename %s", ns_wdir);
    ret = run_cmd(&fp, command);
    if(ret == -1)
    {
      fprintf(stderr, "Unable to find controller name by command : '%s'\n", command);
      exit(-1);
    }

    fgets(buff, 1024, fp);
    if(fp)
      pclose(fp);
    sscanf(buff,"%s",my_controller);
  }
  else
  {
    fprintf(stderr, "NS_WDIR env variable is not set.\n");
    exit(-1);
  }
			        
  //taking TOMCAT_WDIR into local variable 
  if(getenv("TOMCAT_DIR") != NULL)
  {
    strncpy(tomcat_wdir, getenv("TOMCAT_DIR"), 511);
    tomcat_wdir[511] = '\0';
  }
  else
  {
    fprintf(stderr, "TOMCAT_DIR env variable is not set.\n");
    exit(-1);
  }

  if(machine_name[0] == '\0')
  {
    sprintf(command, "hostname -I | awk '{print $1}'");
    ret = run_cmd(&fp, command);
    if(ret == -1)
    {
      MONITOR_TRACE_LOG1("Main_Thread", TL_ERROR, "Unable to find hostname of machine, command : '%s'\n", command);
      exit(-1);
    }

    fgets(buff, 1024, fp);
    if(fp)
      pclose(fp);

    char *ptr = NULL;
    
    if((ptr = strchr(buff, '\n'))) 
      *ptr = '\0';
    snprintf(machine_name, 1024, "%s", buff);    
  }


  //default path of conf file if user not provide it
  if(conf_file == 0)
  {
    snprintf(conf_file_path, 512, "%s/system_health_monitor/conf/system_health_monitor.conf",ns_wdir);
  }

  //checking conf file exist on the path or not
  if(stat(conf_file_path, &s))
  {
    fprintf(stderr, "File '%s' does not exists\n" , conf_file_path);
    exit(-1);
  }
					  
  chdir(ns_wdir);
  //writing the cshm pid in /pid/.cshm.pid 
 
  //array of monitor function
  void* (*fun[])(void *)={find_core_and_remove_core, filesystem_threshold_monitor, remove_builds_from_rel_dir, check_mandatory_process, check_unwanted_process, check_unexpected_files_in_dir, check_ndp_dbu_progress, check_missing_constraint, check_test_run_status, check_rtg_packet_size, check_metadata_file_size, db_dup_check_file, read_product_ui_csv_file, run_and_read_ulimit_command, check_shared_mem_buff, check_disk_free_space, check_heap_dump_monitor, check_postgresql_monitor, max_file_size_in_tmp, check_cleanup_policy_status, check_file_content};

  char base_dir[1024] = "";
  sprintf(full_path, "%s/system_health_monitor/logs",ns_wdir);

  trace_log_key = nslb_init_mt_trace_log_ex(full_path, 0, "System_health_monitor_trace.log", health_monitor_trace_level, cshm_log_file_size);
						   
  if(getcwd(cwd, sizeof(cwd)) != NULL) 
  {
    MONITOR_TRACE_LOG1("Main_Thread", TL_INFO, "Current working dir: %s\n", cwd);
  }

  if (getenv("NS_WDIR") != NULL)
    strcpy(base_dir, getenv("NS_WDIR"));
  else
  {
    //NS_WDIR env variable is not set. Setting it to default value /home/cavisson/work
    sprintf(base_dir, "/home/cavisson/work");
  }
  nslb_write_process_pid_ns_wdir(base_dir,"CSHM",getpid());

  sprintf(tomcat_path, "%s",tomcat_wdir);
  sprintf(tomcat_pid_path, "%s/.pidfiles/.TOMCAT.pid",ns_wdir);

  if(stat(tomcat_pid_path, &ts))
  {
    MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "Unable to find tomcat , So tomcat_path : '%s' and tomcat_pid_path  : '%s'\n", tomcat_path, tomcat_pid_path);
  }
  else
  {
    MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "tomcat_path : '%s' and tomcat_pid_path  : '%s'\n", tomcat_path, tomcat_pid_path);
  }

  //mandatory process path
  sprintf(ndc_path, "%s/ndc/bin/ndcollector",ns_wdir);
  sprintf(ndp_path, "%s/bin/ndp",ns_wdir);
  sprintf(nsu_db_upload_path, "%s/bin/nsu_db_upload",ns_wdir);
  sprintf(ndu_db_upload_path, "%s/bin/ndu_db_upload",ns_wdir);
  sprintf(hpd_path, "%s/hpd/bin/nsu_hpd",ns_wdir);
  sprintf(lps_path, "%s/lps/bin/nsu_lps",ns_wdir);
  sprintf(postgresql_path, "/usr/lib/postgresql/*/bin/postgres");
  sprintf(log_rd_path, "%s/bin/nsu_logging_reader",ns_wdir);
  sprintf(log_wr_path, "%s/bin/nsa_logger",ns_wdir);
  sprintf(event_log_path, "%s/bin/nsa_log_mgr",ns_wdir);
  sprintf(cmon_path, "CAV_MON_HOME");
  sprintf(netstorm_path, "%s/bin/netstorm",ns_wdir);

  //mandatory process pid path 
  sprintf(ndc_pid_path, "%s/ndc/sys/ndc.pid",ns_wdir);
  //sprintf(ndp_pid_path, "%s/logs/TR%d/.ndp.pid",ns_wdir,test_run_no);
  //sprintf(nsu_db_upload_pid_path, "%s/logs/TR%d/.nsu_db_upload.pid",ns_wdir,test_run_no);
  //sprintf(ndu_db_upload_pid_path, "%s/logs/TR%d/.ndu_db_upload.pid",ns_wdir,test_run_no);
  sprintf(hpd_pid_path, "%s/hpd/.tmp/hpd.pid",ns_wdir);
  sprintf(lps_pid_path, "%s/lps/sys/lps.pid",ns_wdir);
  sprintf(postgresql_pid_path, "/var/run/postgresql/9.5-main.pid");
  //sprintf(log_rd_pid_path, "%s/bin/nsu_logging_reader",ns_wdir);
  //sprintf(log_wr_pid_path, "%s/bin/nsa_logger",ns_wdir);
  //sprintf(event_log_pid_path, "%s/bin/nsa_log_mgr",ns_wdir);
  sprintf(cmon_pid_path, "/home/cavisson/monitors/sys/cmon.pid");
  //keyword given
  kw_set_num_builds_threshold_in_rel_dir("5"); 
  kw_check_mandatory_process(postgresql_path, postgresql_pid_path);
  find_all_controller_name();
  read_conf_file();
  sleep(cshm_thread_delay);

  read_cav_conf_file();
								         
  //Set default processes binary information in core info structure
  kw_set_core_info("/home/cavisson/core_files", lps_path, "3", "20480");
  kw_set_core_info("/home/cavisson/core_files", ndc_path, "3", "20480");
  kw_set_core_info("/home/cavisson/core_files", ndp_path, "3", "20480");
  kw_set_core_info("/home/cavisson/core_files", ndu_db_upload_path, "3", "20480");
  kw_set_core_info("/home/cavisson/core_files", nsu_db_upload_path, "3", "20480");
  kw_set_core_info("/home/cavisson/core_files", netstorm_path, "3", "20480");

  if(!strcmp(machine_mode, "NDE"))
  {
    read_config_ini_file();
    kw_check_mandatory_process(ndc_path, ndc_pid_path);
    kw_check_mandatory_process(lps_path, lps_pid_path);
    kw_check_mandatory_process(tomcat_path, tomcat_pid_path);
    kw_check_mandatory_process(cmon_path, cmon_pid_path);

    if(test_run_no_flag)
    {
      kw_check_mandatory_process(ndp_path, ndp_pid_path);
      kw_check_mandatory_process(nsu_db_upload_path, nsu_db_upload_pid_path);
      kw_check_mandatory_process(ndu_db_upload_path, ndu_db_upload_pid_path);
      kw_check_mandatory_process(log_rd_path, log_rd_pid_path);
      kw_check_mandatory_process(log_wr_path, log_wr_pid_path);
      kw_check_mandatory_process(event_log_path, event_log_pid_path);

      if(start_thread[CHECK_NDP_DBU_PROGRESS_FLAG] != 0)
        start_thread[CHECK_NDP_DBU_PROGRESS_FLAG] = 1;
	
      if(start_thread[CONSTRAINT_FLAG] != 0)
        start_thread[CONSTRAINT_FLAG] = 1;
	
      if(start_thread[METADATA_SIZE_CHECK_FLAG] != 0)
        start_thread[METADATA_SIZE_CHECK_FLAG] = 1;

      if(start_thread[DB_DUP_FILE_CHECK] != 0)
        start_thread[DB_DUP_FILE_CHECK] = 1;
     }
  
     if(start_thread[CLEANUP_STATUS_FLAG] !=0)
       start_thread[CLEANUP_STATUS_FLAG] = 1;
  }
  else if(!strcmp(machine_mode, "NF") || !strcmp(machine_mode, "NO"))
  {
    if(!strcmp(machine_mode, "NO"))
      read_config_ini_file();

    kw_check_mandatory_process(hpd_path, hpd_pid_path);
    kw_check_mandatory_process(tomcat_path, tomcat_pid_path);
    kw_check_mandatory_process(cmon_path, cmon_pid_path);
    if(test_run_no_flag)
    {
      kw_check_mandatory_process(log_rd_path, log_rd_pid_path);
      kw_check_mandatory_process(log_wr_path, log_wr_pid_path);
      kw_check_mandatory_process(event_log_path, event_log_pid_path);
    }
  }
  
  else if(!strcmp(machine_mode, "NV"))
  {
    read_config_ini_file();
    kw_check_mandatory_process(hpd_path, hpd_pid_path);
    kw_check_mandatory_process(tomcat_path, tomcat_pid_path);
    kw_check_mandatory_process(cmon_path, cmon_pid_path);
 
    if(test_run_no_flag)
    {
      kw_check_mandatory_process(nsu_db_upload_path, nsu_db_upload_pid_path);
      kw_check_mandatory_process(ndu_db_upload_path, ndu_db_upload_pid_path);
      kw_check_mandatory_process(log_rd_path, log_rd_pid_path);
      kw_check_mandatory_process(log_wr_path, log_wr_pid_path);
      kw_check_mandatory_process(event_log_path, event_log_pid_path);
    }
  }

  else if(!strcmp(machine_mode, "NS") || !strcmp(machine_mode, "NC"))
  {
    disable_for_no_test();
    kw_check_mandatory_process(ndc_path, ndc_pid_path);
    kw_check_mandatory_process(lps_path, lps_pid_path);
    kw_check_mandatory_process(tomcat_path, tomcat_pid_path);
    kw_check_mandatory_process(cmon_path, cmon_pid_path);

    if(test_run_no_flag)
    {
      kw_check_mandatory_process(ndp_path, ndp_pid_path);
      kw_check_mandatory_process(nsu_db_upload_path, nsu_db_upload_pid_path);
      kw_check_mandatory_process(log_rd_path, log_rd_pid_path);
      kw_check_mandatory_process(log_wr_path, log_wr_pid_path);
      kw_check_mandatory_process(event_log_path, event_log_pid_path);
    }
  }
 
  else 
  {
    disable_for_no_test();  
  } 

  if(start_thread[CORE_FLAG] == 1)
  {
    MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "Total core Binaries to monitor = '%d'\n", total_core_binary);
  }

  if(dump_bt_flag)
  {
    sprintf(bt_path, "%s/system_health_monitor/logs/Backtrace.log",ns_wdir);
    bt_fp = fopen(bt_path, "w"); 
 
    if(bt_fp == NULL)
    {
      MONITOR_TRACE_LOG1("Main_Thread", TL_ERROR, "Unable to open backtrace file '%s'\n", bt_path);
    }
  }


  sprintf(full_buff, "chown -R cavisson:cavisson %s", full_path);
  ret = run_cmd(&fp, full_buff);

  if(ret != -1 && fp)
    pclose(fp);

  MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "Enable health monitor = '%d', Enable system level health = '%d', Num of threads = '%d' \n", enable_health_monitor, enable_system_level_health, NUM_THREADS);

  if(enable_health_monitor)
  {
    for (i = 0; i < NUM_THREADS; i++)
    {
      if(start_thread[i] == 1)
      {
        pthread_create(&tid[i], NULL, fun[i], NULL);
      }
    }
  }
  else
  {
    while(1)
    {
      pthread_mutex_lock(&thread_mutex);
  
      MONITOR_TRACE_LOG2("Main_Thread", TL_INFO, "Cavisson System Health Monitor is Disabled by the user\n");
      sprintf(sub_of_mail,"INFO | %s | CSHM | %s | %s | Cavisson System Health Monitor is Disabled", machine_mode,machine_name,my_controller);
      sprintf(msg, "System health monitor is disabled by the user");
      MONITOR_TRACE_LOG2("Msg send for Main_Thread", TL_INFO, "Cavisson System Health Monitor is Disabled by the user\n");
      send_mail_to_user(sub_of_mail, msg, NULL);

      pthread_mutex_unlock(&thread_mutex);
      
      sleep(dis_cshm_mail_time);
    }     
  }

  if(enable_system_level_health == 0)
  {
    while(1)
    { 
      pthread_mutex_lock(&thread_mutex);

      MONITOR_TRACE_LOG2("disabled system level health", TL_INFO, "system level health is disabled \n");
      sprintf(sub_of_mail,"INFO | %s | CSHM | %s | %s | Cavisson System Health Monitor is Disabled", machine_mode,machine_name,my_controller);
      sprintf(msg, "System level health monitoring of Cavisson System Health Monitor is disabled by the user");
      send_mail_to_user(sub_of_mail, msg, NULL);
      
      pthread_mutex_unlock(&thread_mutex);
      
      sleep(dis_cshm_mail_time);
    }
  }

																	         MONITOR_TRACE_LOG3("Main_Thread", TL_INFO, "main thread going in infinite sleep \n");
   while(1)
   {
     sleep(100);
   } 
  return 0;
}


