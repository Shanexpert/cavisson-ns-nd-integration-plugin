#define _XOPEN_SOURCE 
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "cdr_cache.h"
#include "cdr_utils.h"
#include "cdr_main.h"
#include "cdr_log.h"
#include "cdr_cleanup.h"
#include "nslb_util.h"

#define MAX_RUNNING_TEST_LIST 20
#define MaxValue 2048 * 10

int running_test_list[1024 * 1024];
int total_running_test = 0;
char rebuild_cache = CDR_FALSE;

void change_ts_partition_name_format(long long int time_stamp, char *outbuff, int outbuff_size)
{
 
  CDRTL2("Method called");

  struct tm  ts;
  ts = *localtime((time_t *)(&time_stamp));
  strftime(outbuff, outbuff_size, "%Y%m%d%H%M%S", &ts);

  CDRTL2("Method exit");
}

long long format_date_convert_to_ts(char * buffer)
{
  char tmp_buf[1024];
  struct tm tm;
 
  CDRTL2("Method called");

  memset(&tm, 0, sizeof(struct tm));
  //Format of Date  : 2017/04/02
  tm.tm_isdst = -1;   //auto find daylight saving
  strptime(buffer, "%Y/%m/%d %H:%M:%S", &tm);
  strftime(tmp_buf, 1024, "%s", &tm);

  CDRTL2("Method exit");
  return atoll(tmp_buf);
}

long long partition_format_date_convert_to_ts(char * buffer)
{
  char tmp_buf[1024];
  struct tm tm;
 
  CDRTL2("Method called");

  memset(&tm, 0, sizeof(struct tm));
  //Format of Date  : 2017/04/02
  tm.tm_isdst = -1;   //auto find daylight saving
  strptime(buffer, "%Y%m%d%H%M%S", &tm);
  strftime(tmp_buf, 1024, "%s", &tm);

  CDRTL2("Method exit");
  return atoll(tmp_buf);
}

long long summary_top_time_convert_to_ts(char * buffer)
{
  char tmp_buf[1024];
  struct tm tm;
 
  CDRTL2("Method called");

  memset(&tm, 0, sizeof(struct tm));
  //Format of Date  : 06/16/20 12:24:42
  tm.tm_isdst = -1;   //auto find daylight saving
  strptime(buffer, "%m/%d/%y  %H:%M:%S", &tm);
  strftime(tmp_buf, 1024, "%s", &tm);

  CDRTL2("Method exit");
  return atoll(tmp_buf);
}

int convert_to_secs(char *str)
{
  int field1, field2, field3, total_time;
  int num;
      
  num = sscanf(str, "%d:%d:%d", &field1, &field2, &field3);
  if(num == 3)
    total_time = ((field1*60*60) + (field2*60) + field3); //if HH::MM:SS given
  else 
  {
    CDRTL1("Error: Invalid format of time '%s'\n", str);
    return 0;
  }       
    
  return total_time;
}

long long int get_lmd_ts(char *path) {
  struct stat attr;

  CDRTL2("Method called, '%s'", path);
  if(stat(path, &attr) != 0)
  {
    CDRTL1("Error: Configuration file not present '%s', error : '%s'.", path, strerror(errno));
    return CDR_ERROR;
  }
  CDRTL2("Method exit, ts =%lld", attr.st_mtime);
  return attr.st_mtime;
}

int update_running_test_list()
{
  char find_cmd[CDR_FILE_PATH_SIZE];
  char tid_file_name[CDR_FILE_PATH_SIZE];
  FILE *find_cmd_fp;

  CDRTL2("Method called");
  sprintf(find_cmd, "%s/bin/nsu_show_test_logs -RL | awk '!/TestRun/{printf(\"%%-10s%%-10s%%-20s%%-s\\n\",$1,$2,$6,$7)}' | cut -d ' ' -f 1", 
                     ns_wdir);

  /* Run find */
  find_cmd_fp = popen(find_cmd, "r");

  if(!find_cmd_fp) {
    CDRTL1("Error: Unable to get netstorm.tid file");
    return CDR_ERROR;
  }

  /* Read find output*/
  while(nslb_fgets(tid_file_name, CDR_FILE_PATH_SIZE, find_cmd_fp, 0)) {
     /* We have testrun number here*/ 
     running_test_list[total_running_test++] = atoi(tid_file_name);
  }

  pclose(find_cmd_fp);

  return CDR_SUCCESS;
}

char tr_is_running(int tr_num)
{
  CDRTL2("Method called");

  for(int i = 0; i < total_running_test; ++i)
    if(running_test_list[i] == tr_num)
      return CDR_TRUE;
  CDRTL2("Method exited");
  return CDR_FALSE;
}

static int tr_is_old_cmt_test(int tr_num)
{
  /* TR is other cmt test if first_partition and start_partition is not similar in the current partition 
   * read .curPartition info and get cur partion 
   * read .partition_info.txt in cur patiation and if 
   * FirstPartition not equal StartPartition, it is OTHER_CMT_TEST
   * these to fields values are not same mark as other_CMT_test
   */
  FILE *fp = NULL;
  char buff[CDR_BUFFER_SIZE] = "\0";
  char cur_partition[CDR_BUFFER_SIZE] = "\0";
  char first_partition[CDR_BUFFER_SIZE] = "\0";
  char start_partition[CDR_BUFFER_SIZE] = "\0";
  char file_path[CDR_FILE_PATH_SIZE] = "\0";

  CDRTL2("Method exit");

  sprintf(file_path, "%s/logs/TR%d/.curPartition", ns_wdir, tr_num);

  fp = fopen(file_path, "r");
  if(!fp) {
    CDRTL1("Error: cannot open: %s", file_path);
    return CDR_FALSE;
  }

  while(nslb_fgets(buff, CDR_BUFFER_SIZE, fp, 0) != NULL)
  {
    if(strncmp(buff, "CurPartitionIdx=", 16) == 0)
    {
      strncpy(cur_partition, buff + 16, 14); // length of partition name is 14
      break;
    }
  }
  fclose(fp);

  sprintf(file_path, "%s/logs/TR%d/%s/.partition_info.txt", ns_wdir, tr_num, cur_partition);
  fp = fopen(file_path, "r");
  if(!fp) {
    CDRTL1("Error: cannot open: %s", file_path);
    return CDR_ERROR;
  }

  while(nslb_fgets(buff, CDR_BUFFER_SIZE, fp, 0) != NULL)
  {
    if(strncmp(buff, "FirstPartition=", 15) == 0) {
      strcpy(first_partition, buff + 15);
    }
    else if(strncmp(buff, "StartPartition=", 15) == 0) {
      strcpy(start_partition, buff + 15);
    }

    if (start_partition[0] != '\0' && first_partition[0] != '\0')
      break;
  }
  fclose(fp);

  CDRTL2("Method exit");

  if(strcmp(first_partition, start_partition) != 0)
    return CDR_TRUE;
  return CDR_FALSE;
}

int tr_is_bad(int tr_num)
{
  /* TR is bad if all of the following files are not present:
   * 1) sumary.top
   * 2) sorted_scenario.conf
   * 3) .curPartition
   * 4) <cur_partition>/rtgMessage.dat
   * 5) <cur_partition>/pctMessage.dat
   * 6) <cur_partition>/testrun.gdf
   */
  FILE *fp;
  struct stat st;
  char *cur_partition;
  char buff[CDR_BUFFER_SIZE];
  char key_files[6][CDR_FILE_PATH_SIZE];

  CDRTL2("Method called");

  sprintf(key_files[0], "%s/logs/TR%d/summary.top", ns_wdir, tr_num);
  sprintf(key_files[1], "%s/logs/TR%d/sorted_scenario.conf", ns_wdir, tr_num);
  sprintf(key_files[2], "%s/logs/TR%d/.curPartition", ns_wdir, tr_num);

  fp = fopen(key_files[2], "r"); // open .curPartition and get current partition
  if(!fp)
    return CDR_TRUE;

  while(nslb_fgets(buff, CDR_BUFFER_SIZE, fp, 0) != NULL)
  {
    if(strncmp(buff, "CurPartitionIdx=", 16) == 0)
      cur_partition = buff + 16; // skip to partition name
      int len = strlen(buff);
      if(buff[len - 1] == '\n')
        buff[len - 1] = '\0';
  }
  fclose(fp);

  sprintf(key_files[3], "%s/logs/TR%d/%s/rtgMessage.dat", ns_wdir, tr_num, cur_partition);
  //sprintf(key_files[4], "%s/logs/TR%d/%s/pctMessage.dat", ns_wdir, tr_num, cur_partition);
  sprintf(key_files[4], "%s/logs/TR%d/%s/testrun.gdf", ns_wdir, tr_num, cur_partition);

  for(int i = 0; i < 5; ++i) {
    if(stat(key_files[i], &st) != 0) {
      CDRTL4("file %s, error: %s\n", key_files[i], strerror(errno));
      return CDR_TRUE;
    }
  }

  CDRTL2("Method exit");
  return CDR_FALSE;
}

int get_cmt_tr_number()
{
  char file_name[CDR_FILE_PATH_SIZE];
  char data[CDR_FILE_PATH_SIZE];
  FILE *fp;
  int cmtTestNumber = 0;

  CDRTL2("Method called");

  sprintf(file_name, "%s/webapps/sys/config.ini", ns_wdir);
  fp = fopen(file_name, "r");

  if(!fp) {
    CDRTL1("Error: Unable to open '%s', seeting CTM TR number '%d', error : '%s'.", file_name, cmtTestNumber, strerror(errno)); 
    return cmtTestNumber;
  }
  int len = 15; //strlen("nde.testRunNum=");
  while(nslb_fgets(data, CDR_FILE_PATH_SIZE, fp, 0))
  {
    CDRTL4("Data from file : '%s'.", data); 
    if(data[0] == '#') // ignoring comment lines
      continue;

    if(!strncmp(data, "nde.testRunNum=", len))
    {
      char *ptr = data + len; 
      cmtTestNumber = atoi(ptr);
    }
    else if(!strncmp(data, "nde.testRunNum = ", len +2))
    {
      char *ptr = data + len + 2;
      cmtTestNumber = atoi(ptr);
    }
  }
  fclose(fp);
  CDRTL2("Method exit, CMT TR number '%d'", cmtTestNumber);
  return cmtTestNumber;
}

/***********************************************************************************
 * Name: get_test_run_mode
 * This function will return the status of a test run
 *    1)Bad 2)CMT 3)Running 4)Old CMT 5)Debug 6)Archived 7)Locked 8)Performance
 * If 13 field in summary.top is:
 *      AR: Archive,    R: Locked,    W: Unlocked
 ***********************************************************************************/
char get_test_run_type(int tr_num, char *test_mod_buf, int remove_tr)
{
  char f_path[CDR_FILE_PATH_SIZE];
  struct stat st;

  CDRTL2("Method called");

  if (tr_num == cmt_tr_num)
    return CMT_TR;

//  update_running_test_list(); // get the currently running test list

  if(tr_is_running(tr_num))
    return RUNNING_TR;

  if(strcmp(test_mod_buf, "AW") == 0)
    return ARCHIVED_TR;
  else if(strcmp(test_mod_buf, "R") == 0 || strcmp(test_mod_buf, "AR") == 0)
    return LOCKED_TR;

  if(tr_is_bad(tr_num)){
    if(remove_tr)
      remove_tr_ex(tr_num);
    //Audit Log
    return BAD_TR;
  }

  if(tr_is_old_cmt_test(tr_num))
    return OLD_CMT_TR;
  
  sprintf(f_path, "%s/logs/TR%d/ready_reports/TestRunNumber", ns_wdir, tr_num);
  if (stat (f_path, &st) == 0) // debug file present
    return GENERATOR_TR;

  sprintf(f_path, "%s/logs/TR%d/debug.log", ns_wdir, tr_num);
  if (stat (f_path, &st) == 0) // debug file present
    return DEBUG_TR;


  return PERFORMANCE_TR;
}

int send_buffer(char *buffer, int bytes_read, int *partial_buf_len , char flag)
{
  char *tmp_ptr; 
  char *data_ptr = buffer;

  while((tmp_ptr = strchr(data_ptr, '\n'))!= NULL)
  {
    *tmp_ptr = '\0';
    if(flag == 1)
    {
      cache_entry_add(data_ptr, 0);  
    }
    else if(flag == 2)
    {
      cmt_cache_entry_add(data_ptr, 0);
    }
    else if (flag == 3)
    {
      nv_cache_entry_add(data_ptr, 0);
    }
    data_ptr = tmp_ptr + 1;
  }
  if (data_ptr != buffer)
  {
    *partial_buf_len = bytes_read - (data_ptr - buffer);
    memcpy(buffer, data_ptr, *partial_buf_len + 1);
  }
  return 0;
}

int read_file(char *file_path, char flag, char present_flag)
{
  int fd ;
  int bytes_read = 0;
  int partial_buf_len = 0; 
  char RecvMsg[MaxValue + 1];
 
  fd = open(file_path, O_RDONLY);
  if (fd < 0)
  { 
    return 1;        
  } 
  while(1)
  {
    while((bytes_read = read(fd, RecvMsg + partial_buf_len, MaxValue - partial_buf_len)) > 0)      //read max size from a file
    {
      bytes_read = bytes_read + partial_buf_len;
      RecvMsg[bytes_read + 1] = '\0';                                     		 //setting last element of a buffer to NULL       
      partial_buf_len = 0;
      send_buffer(RecvMsg, bytes_read, &partial_buf_len, flag);                  
      bytes_read = 0;
    }
    if(partial_buf_len)
    {
      if(flag == 1)
      {
        cache_entry_add(RecvMsg, present_flag);
      }
      else if(flag == 2)
      {  
        cmt_cache_entry_add(RecvMsg, present_flag);
      }
      else if (flag == 3)
      { 
        nv_cache_entry_add(RecvMsg, present_flag);
      }
    }
    return 0;  
  }
  return 0;
}

/*Check if cav data retenetion managder is already running*/
int check_and_set_cdr_pid()
{
  FILE *pid_fp;
  char pid_file_path[CDR_FILE_PATH_SIZE], buff[CDR_BUFFER_SIZE] = "\0";
  int pid = 0;
  CDRTL2("MEthod called");
  struct stat st;

  sprintf(pid_file_path, "%s/logs/data_retention/.cavDataRetention.pid", ns_wdir); 

  if(!(pid_fp = fopen(pid_file_path, "r")))
  {
    CDRTL1("Error: cannot open: %s", pid_file_path);
  }
  else 
  {

    //read pid from file
    nslb_fgets(buff, CDR_BUFFER_SIZE, pid_fp, 0);
    pid = atoi (buff);
    fclose(pid_fp);
    
    if(pid > 0) // pid should be greater then 0
    {
      char pid_dir[CDR_FILE_PATH_SIZE];
      sprintf(pid_dir, "/proc/%d/comm", pid); 
      CDRTL1("PID in pid file '%d'", pid);
 
      if(!stat(pid_dir, &st)) // Check PID is running or not 
      {
        CDRTL1("PID directory present '%s'", pid_dir);
        
        //Check PID is running with same process or not 
 
        FILE *proc_name_fp;
 
        if(!(proc_name_fp = fopen(pid_dir, "r")))
        {
          CDRTL1("Error: cannot open: %s, error : '%s'.", pid_dir, strerror(errno));
          return CDR_FALSE;
        }
 
        //read process name from file
        buff[0] = '\0';
        nslb_fgets(buff, CDR_BUFFER_SIZE, proc_name_fp, 0); // Reuse the buff
        fclose(proc_name_fp);
 
        //replace \n with NULL;
        char *ptr = strchr (buff, '\n');
        if (ptr)
          ptr[0] = '\0';
 
        CDRTL3("Process name '%s', with pid '%d'.", buff, pid);

        if (!strcmp(buff, "cav_data_ret_ma"))
        {
          CDRTL1("Error: Cavisson data retention manager is alrady running with '%d' pid.", pid);
          exit (CDR_ERROR);
        }
        CDRTL1("Error: Pid '%d' is present in pid file and running , but with another process name '%s'", pid, buff);
      }
      else
        CDRTL1("Error: Pid '%d' is present in pid file but not running.", pid);
    }
  }

  //Write new pid in pid file
  pid = getpid();
  
  CDRTL1("Cureent pid Pid '%d'", pid);

  //open file in truncate mode
  if(!(pid_fp = fopen(pid_file_path, "w")))
  {
    CDRTL1("Error: cannot open: '%s' in write mode, eorro: '%s'", pid_file_path, strerror(errno));
    return CDR_ERROR;
  }

  fprintf(pid_fp, "%d\n", pid);
  fclose(pid_fp);

  CDRTL2("MEthod exit");
  return CDR_SUCCESS;
}

int check_and_set_lmd_config_file()
{
  FILE *lmd_fp;
  char lmd_file_path[CDR_FILE_PATH_SIZE], buff[CDR_BUFFER_SIZE] = "\0";
  long long int lmd = 0;
  long long int config_file_lmd_ts;

  CDRTL2("Method Called");
  sprintf(lmd_file_path, "%s/logs/data_retention/.lmd", ns_wdir); 

  //Check if foile is exists

  if(!(lmd_fp = fopen(lmd_file_path, "r")))
  {
    CDRTL1("Error: cannot open: %s", lmd_file_path);
  }
  else
  {

    //read pid from file
    nslb_fgets(buff, CDR_BUFFER_SIZE, lmd_fp, 0);
    lmd = atoi (buff);
    fclose(lmd_fp);
    CDRTL3("LMD for file '%s' in file is '%lld'.", lmd_file_path, lmd);
  }

  if ((config_file_lmd_ts = get_lmd_ts(config_file_path)) == CDR_ERROR)
    return CDR_ERROR; 

  CDRTL3("Current LMD for file '%s' is '%lld' and in LMD file '%lld'.", lmd_file_path, config_file_lmd_ts, lmd);

  if(config_file_lmd_ts != lmd) //config file updated
  {
    CDRTL1("Found Config file modified. Setting Rebuilding cache flag...");
    rebuild_cache = CDR_TRUE;

    // Write new LMD in LMD file
    if(!(lmd_fp = fopen(lmd_file_path, "w")))
    {
      CDRTL1("Error: cannot open: '%s' in write mode, eorro: '%s'", lmd_file_path, strerror(errno));
      return CDR_ERROR;
    }
    fprintf(lmd_fp, "%lld\n", config_file_lmd_ts);
    fclose(lmd_fp);
  }
  else 
    CDRTL1("Error: Config file '%s' is not updated, Going to use old cache...", lmd_file_path);
  CDRTL2("Method Exit");
  return CDR_SUCCESS;
}

long long int get_ts_in_ms()
{
  struct timeval te;
  gettimeofday(&te, NULL); // get current time
  long long milliseconds = te.tv_sec*1000LL + te.tv_usec/1000; // calculate milliseconds
    // printf("milliseconds: %lld\n", milliseconds);
  return milliseconds;
}


#ifdef MAIN
int main ()
{
  char ptr[256] = "2018/02/15";
  printf("\nTs=%lld\n\n", convert_to_ts(ptr));
}
#endif
