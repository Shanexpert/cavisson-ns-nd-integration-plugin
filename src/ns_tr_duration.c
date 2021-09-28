#define _GNU_SOURCE
//#define _XOPEN_SOURCE       /* See feature_test_macros(7) */
/* 
 * Output will be :
 * ----------------------------------------------------------- 
    TR|Duration|OrigDuration|Operation|Status (header line)
    1234|00:10:10|00:00:00|get or update|OK or NotOK or NotZero
*/

/* summary.top has 16 pipe seperated fields,
   Test Run|Scenario Name|Start Time|Report Summary|Page Dump|Report Progress|Report Detail|
   Report User|Report Fail|Report|Page Break Down|WAN_ENV|REPORTING|Test Name|Test Mode
   |Run Time|Virtual Users
*/

#include <time.h>
#include <errno.h>
#include <strings.h>
#include <stdlib.h>
#include "nslb_util.h"
#include "ns_tr_duration.h"

int test_run_running = 0;
TestRunList *test_run_info = NULL;

char *trpath(int trnum, char *ns_wdir_path) 
{
  char workpath[MAXLENGTH + 1];
  static char trpath[MAXLENGTH + 1];
  
  //strcpy (workpath, getenv("NS_WDIR"));  
  strcpy (workpath, ns_wdir_path);  
  sprintf(trpath,"%s/logs/TR%d", workpath, trnum);
  return trpath;
}

/*calculating duration by finidng the block size from testrun.gdf and size of rgtMessage.dat .
* tot_block = (size of rgtMessage.dat)/ block size 
* duration = tot_block * progress interval
* progress interval is also taken from testrun.gdf 
*/
/* Design change for rtg file.
   End packet is written in rtg on test stop and new rtg file is created after runtime change.
   Hence start and end packet will be present in last rtg only.
   Hence skip start packet only if next rtg is present or test is running or test was stopped forcefully.
   Also do while loop takes care of multiple rtg in partition
*/
int get_actual_duration(char *summary_top, char *rtg_msg_dat, char *test_run_gdf, int tr_running_flag)
{
  struct stat stat_rtg_msg_dat;
  FILE *fp_test_run;
  char frst_line[8192 + 1];
  char buf[1024 + 1] = {0};
  char *fields[100];
  char str_orig_duration[20];
  int ret_val;
  int tm_interval;
  int tot_block = 0;
  int blocks_in_rtg = 0;
  int trnum;
  unsigned int rtg_size = 0;
  unsigned int next_rtg_size = 0;
  unsigned int blk_sz;
  int orig_duration;
  int calc_duration;
  int rtg_file_count = 0;
  int next_rtg_file_present = 0;


  if (stat (rtg_msg_dat, &stat_rtg_msg_dat) != -1)
    next_rtg_size = (unsigned int) stat_rtg_msg_dat.st_size;
  else
    return 0; //assuming that core dump happened and rtgMessage.dat is not created.so duration is 0.
  
  /* If runtime change happens, then new rtg file is created in partition with new version
     eg rtgMessage.dat rtgMessage.dat.1 rtgMessage.dat.2
     In non partition mode, runtime changes are not applicable, hence no new rtg file is created.
     But this do while loop handles multiple rtg in both partition and non partition mode.
  */

  do 
  {
    rtg_size = next_rtg_size;

    //check if next rtg is present
    //no need to check this in non partition mode
    sprintf(buf, "%s.%d", rtg_msg_dat, rtg_file_count + 1); 
    if(stat(buf, &stat_rtg_msg_dat) != -1)
    {
      next_rtg_file_present = 1;
      next_rtg_size = (unsigned int) stat_rtg_msg_dat.st_size;
    }
    else
    {
      next_rtg_file_present = 0;
    }

    if(rtg_file_count == 0)
      fp_test_run = fopen (test_run_gdf, "r");
    else
    {
      sprintf(buf, "%s.%d", test_run_gdf, rtg_file_count); 
      fp_test_run = fopen (buf, "r");
    }
     
    if (fp_test_run == NULL) 
    {
      //Bug By Passing testrun when testrun.gdf and rtgMessage.dat is not in sequence  
      //perror ("fopen");
      //exit(-1);
      return -1;
    }
    //In testrun.gdf now we have increase count to 9 as we assume partition index 
    //will be added in case of continues monitoring 
    //default -1
    // Modification:changes token index from 9 to 15 
    if (nslb_fgets(frst_line, 8192, fp_test_run, 0) != NULL)
    {
      ret_val  = get_tokens (frst_line, fields, "|", 15);
      if (ret_val == -1){
        fprintf (stderr, "get_tokens failed !!");  
        return -1;  
      }   
    } 
    else 
    {
      perror ("fgets");
      return -1;
    }

    if(!fields[5])
      return 0;
  
    strcpy (str_orig_duration, get_orig_duration (summary_top, &trnum)); 
    orig_duration = get_tm_from_format (str_orig_duration);  
   
    blk_sz = atoi (fields[5]); //block size from testrun.gdf
    //printf("blk_size %d\n",blk_sz);
    tm_interval = (atoi (fields[6])) / 1000; //progess interval from testrun.gdf in TR directory
    //printf ("duration=%ld\n",orig_duration);
    if(!blk_sz)
      return 0;
    blocks_in_rtg = (rtg_size / blk_sz); 
  
    //if only start block or no block is present in rtg
    if (blocks_in_rtg <= 1)
      blocks_in_rtg = 0; 
    //To skip start block ,if next rtg us present or test ends abnormally or forcely or test is running
    else if (next_rtg_file_present == 1 || orig_duration == 0 || tr_running_flag == 1)
      blocks_in_rtg = blocks_in_rtg - 1; 
    else
      blocks_in_rtg = blocks_in_rtg - 2; //To skip start and end block in last rtg.

    tot_block += blocks_in_rtg;
    rtg_file_count++;
  } while(next_rtg_file_present);

  calc_duration = tot_block * tm_interval ;

  return calc_duration; 
}


/*fuction to get duration from summary.top
 * it is the 15 field in summary.top 
 */

/*Bug 4283: We are not able to reproduce this issue. But earlier we were returning NULL which may cause problem in strcpy that's why
 * we have done some changes instead of returning NULL in get_orig_duration(), now we are returning DEFAULT_ORIG_DURATION.
 */

#define DEFAULT_ORIG_DURATION "00:00:00"

char *get_orig_duration(char *filepath, int *trnum)
{
  FILE *fp;
  struct stat summary_stat_buf;
  char line[8192];
  char *fields[100];
  int totalflds;
  static char orig_duration[20];
  
  if ((fp = fopen (filepath,"r")) == NULL)
    return DEFAULT_ORIG_DURATION;  // Ignore this test run dont have summary.top

  if (nslb_fgets (line, 8192, fp, 0) == NULL) {
    fclose (fp);
    return DEFAULT_ORIG_DURATION;  // Ignore as we did not find line in summary.top 
  }
  
  if (fstat (fileno (fp), &summary_stat_buf) < 0) {
    return DEFAULT_ORIG_DURATION;   // Ignore as we did not find as something wrong with fp
  }
  fclose (fp);
  
  if (line[strlen (line) - 1] == '\n')
    line[strlen (line) - 1] = '\0';
  totalflds = get_tokens (line, fields, "|", 20);  
  if (totalflds < MAX_SUMMARY_TOP_FIELDS) {
    return DEFAULT_ORIG_DURATION; // Less fields ignore this test run
  }

  *trnum = atoi(fields[0]);
  strcpy (orig_duration, fields[14]);      //Run Time (duration)
  //num_duration = get_tm_from_format (orig_duration);
  return orig_duration;
  //printf ("--------->>>%s\n",orig_duration);
  exit(0);
}
                                                 
/*
 *To convert XX:XX:XX format to secs
 */
int get_tm_from_format(char *str)
{
  int hr, min, sec, msec;
  int num, total_time;
  
  num = sscanf (str, "%d:%d:%d.%d", &hr, &min, &sec, &msec);
  if(num == 1)
    total_time = hr*1000; //if only one value given it is seconds
  else if(num == 3)
    total_time = ((hr*60*60) + (min*60) + sec)*1000; //if HH::MM:SS given
  else if(num == 4)
    total_time = ((hr*60*60) + (min*60) + sec)*1000 + msec; //if HH::MM:SS.MSECS given
  else if(num == 2)
    total_time = ((hr*60) + min)*1000; //if MM:SS given
  else 
    total_time = 60*1000;  //default

  return(total_time);//returns in msecs 
}

/*
 *To convert secs to  XX:XX:XX format
 */
//char *format_time(int tm)
char *format_time(int tm)
{
  int hr,min,sec;
  static char str_time[MAXLENGTH +1];
  
  hr = tm / 3600; 
  min = (tm % 3600) / 60; 
  sec = (tm  % 3600) % 60 ;
  sprintf (str_time, "%02d:%02d:%02d", hr, min, sec); 

  return str_time;
}

/*
 *To write calculated duration to summary.top .
 */
int write_summary(char *duartion, char *summary_top)
{
  FILE *fp;
  int fpf_ret;
  char line[8192];
  int count = 0;
  char *line_ptr = line;

  if ((fp = fopen (summary_top,"r")) == NULL)
    return 1;  // Ignore this test run dont have summary.top

  if (nslb_fgets (line, 8192, fp, 0) == NULL) {
    fclose (fp);
    return 1;  // Ignore as we did not find line in summary.top 
  }
  else
    fclose(fp);


  if (line[strlen (line) - 1] == '\n')
    line[strlen (line) - 1] = '\0';

  if ((fp = fopen (summary_top, "w")) == NULL)
    return 1;

  while(*line_ptr)
  {
    if(*line_ptr == '|')
      count++;

    if((count == 14) && (*line_ptr == '|'))
    {
      *line_ptr = 0;
      fpf_ret = fprintf (fp, "%s|%s|", line, duartion);
      if (fpf_ret < 0)
        return 1;

      line_ptr++;
    }
    else if(count == 15)
    {
      fpf_ret = fprintf (fp, "%s\n", ++line_ptr);
      if (fpf_ret < 0)
        return 1;
      return 0; //abhay :- this function will be returned from here as we have updated the last field of summary.top(bug 10016)
    }
    else
      line_ptr++;
  }

  fclose (fp);
  return 0;
}

/*TR|Duration|OrigDuration|Operation|Status (header line)
 *1234|00:10:10|00:00:00|get or update|OK or NotOK or NotZero
 * **********************************************************/
void print_result(int trnum,char *duration, char *orig_duration, char *operation, char *status, int hflag)
{
  char result_line[MAXLENGTH + 1];
  
  if (hflag) {
    printf("TestRun|Duration|OriginalDuration|Operation|Status\n"); 
  }
  sprintf (result_line, "%d|%s|%s|%s|%s", trnum, duration, orig_duration, operation, status);
  printf ("%s\n", result_line);
}

/*--------------------
format_date_get_seconds_from_epoch takes time and date in time_str and delimeter in ch.
It then calculates the total seconds passed from epoch time and returns it as long long integer variable.

Note : This time calculation is only for year >= 2000.
----------------------*/
long long format_date_get_seconds_from_epoch(char *time_str, char ch)
{
  struct tm t;
  time_t t_of_day;

  char *date_ptr = time_str;
  char *time_ptr;
  char *tmp;
  int num, num1;
  long long field1, field2, field3, field4, field5, field6;

  tmp = index(date_ptr, ch);
  
  if(tmp)
  { 
    time_ptr = tmp + 1;
    *tmp = 0;
  }

  num = sscanf(date_ptr, "%lld/%lld/%lld", &field1, &field2, &field3); 		// MM/DD/YYYY
  num1 = sscanf(time_ptr, "%lld:%lld:%lld", &field4, &field5, &field6);		// HH:MM:SS

  if((num < 3 || num > 3) || (num1 <3 || num1 > 3))
  {
    printf("ERROR : Entered time & date in case of -x is not in correct format.\n");
    exit(-1);
  }
 
  if(field1 > 12 || field2 > 31)
  {
    printf("ERROR : Date [%s] is invalid. \n", date_ptr);
    exit(-1);
  }

  if(field4 > 60 || field5 > 60 || field6 > 60)
  {
    printf("ERROR : Time [%s] is invalid.\n", time_ptr);
    exit(-1);
  }

  if(field3 < 2000)
    field3 += 2000;

  t.tm_year = field3 - 1900;
  t.tm_mon = field1 - 1;           // Month, 0 - jan
  t.tm_mday = field2;          // Day of the month
  t.tm_hour = field4;
  t.tm_min = field5;
  t.tm_sec = field6;
  t.tm_isdst = -1;        // Is DST on? 1 = yes, 0 = no, -1 = unknown
  t_of_day = mktime(&t);

  //printf("Seconds since the Epoch: %ld\n", (long) t_of_day);
  return(t_of_day);
}

/*--------------------
logging_start_end_date_time takes time in specific format (refer usage) and set global variables sec_for_start and sec_for_end
----------------------*/
void logging_start_end_date_time(char *time_date, long long *sec_for_start, long long *sec_for_end)
{
  char *tmp;
  char *start_format = time_date;
  char *end_format;

  tmp = index(start_format, ' ');

  if(tmp)
  {
    end_format = tmp + 1;
    *tmp = 0;
  }

  *sec_for_start = format_date_get_seconds_from_epoch(start_format, '-');
  *sec_for_end = format_date_get_seconds_from_epoch(end_format, '-');

}

//common for both utilities : nsu_show_test_logs & nsu_show_partition_detail
void fill_min_max_duration(char *time_string, long long *max_duration, long long *min_duration) 
{
  char *tmp;
  char *min_time_str = time_string;
  char *max_time_str;
 
  tmp = index(time_string, '-');

  if(tmp) 
  {
    max_time_str = tmp + 1;
    *tmp = 0;
  }

  *min_duration = local_get_time_from_format(min_time_str);  // mintime have time is millsec

  if(max_time_str) 
  {
    if(strcmp(max_time_str, "NA") != 0)
      *max_duration = local_get_time_from_format(max_time_str);  // mintime have time is millsec
    else
      *max_duration = 9223372036854775807;  // LONG LONG MAX
  } 
  else
    *max_duration = *min_duration; 
}

/*BUG 9141: 
 Time duration in summary.top file was 700:12:01 while converting into millisec the value overflow integer range 
 hence making all time stamps in long long format.*/
long long local_get_time_from_format(char *str)
{   
  long long field1, field2, field3, field4, total_time;
  int num;
      
  num = sscanf(str, "%lld:%lld:%lld.%lld", &field1, &field2, &field3, &field4);
  if(num == 1)
    total_time = field1*1000; //if only one value given it is seconds
  else if(num == 3)
    total_time = ((field1*60*60) + (field2*60) + field3)*1000; //if HH::MM:SS given
  else if(num == 4)
    total_time = ((field1*60*60) + (field2*60) + field3)*1000 + field4; //if HH::MM:SS.MSECS given
  else if(num == 2)
    total_time = ((field1*60) + field2)*1000; //if MM:SS given
  else 
  {
    fprintf(stderr, "Invalid format of time '%s'\n", str);
    exit(-1);
  }       
    
  return(total_time);
}   

int is_testrun_running(int test_run_num)
{
  int i = 0;

  for(i = 0; i < test_run_running; i++) 
  {
    if (test_run_num == test_run_info[i].test_run_list) 
      return 1; //Test run found
  } 

  return 0;
}

char *calc_duration(int trnum, char *ns_log_path, char *sum_top_file, char *str_orig_duration, int tr_or_partition, long long partition_idx, int Rflag, int get_is_test_running_flag)
{
  char tr_dir_path[MAXLENGTH + 1];
  char rtg_msg_dat_path[MAXLENGTH + 1];
  char test_run_gdf_path[MAXLENGTH + 1];
  static char str_calc_duration[MAXLENGTH + 1];
  struct stat stat_buf;
  int calc_duration;
  int no_sum_top = 0;
  int no_rtg_msg = 0;
  int tr_gdf_flag = 0;
  int tr_running_flag = 0;

  if(tr_or_partition)
    sprintf (tr_dir_path, "%s/TR%d/%lld", ns_log_path, trnum, partition_idx); 
  else
    sprintf (tr_dir_path, "%s/TR%d", ns_log_path, trnum); 
  
  
  strcpy (rtg_msg_dat_path, tr_dir_path);
  strcat (rtg_msg_dat_path, "/rtgMessage.dat");
  strcpy (test_run_gdf_path, tr_dir_path);
  strcat (test_run_gdf_path, "/testrun.gdf");
  
  if (stat (sum_top_file ,&stat_buf) == -1) {
    no_sum_top = 1;
  }
  if (stat (rtg_msg_dat_path ,&stat_buf) == -1) {
    no_rtg_msg = 1;
  }
          
  if (stat (test_run_gdf_path ,&stat_buf) == -1) {
    tr_gdf_flag = 1; 
  }

  if(get_is_test_running_flag == 1)
  {
    //check if testrun running  
    tr_running_flag = is_testrun_running(trnum);
  }

  if (no_sum_top == 0 &&  no_rtg_msg == 0 && tr_gdf_flag == 0){
    calc_duration = get_actual_duration (sum_top_file, rtg_msg_dat_path, test_run_gdf_path, tr_running_flag); 
    if(calc_duration == -1) {
      sprintf(str_calc_duration, "%d", calc_duration);
      return str_calc_duration;
    }
    if (Rflag)
      calc_duration = (calc_duration > 10) ? calc_duration - 10 : 0;   //because last block not added in running test ,so decrementing time taken by it.
  } else  {
    calc_duration = 0;
  }
  
  strcpy (str_calc_duration, format_time (calc_duration));

  return str_calc_duration;
}

//convert partition name into msecs from epoch
long long convert_partition_name_into_msec(char *partition_name)
{
  char *ptr = NULL;
  char year_buf[5] = {0};
  char month_buf[3] = {0};
  char date_buf[3] = {0};
  char hour_buf[3] = {0};
  char min_buf[3] = {0};
  char sec_buf[3] = {0};
  struct tm t;
  long long time_in_sec = 0;

  ptr = partition_name;

  /* partition_name format is '20140523223419' which means '2014  05    23    22    34      19 '
   *                                                         |    |     |     |     |       |
   *                                                         v    v     v     v     v       v
   *                                                        year  month date  hour  minute  second  */
  strncpy(year_buf, ptr, 4);
  ptr = ptr + 4;

  strncpy(month_buf, ptr, 2);
  ptr = ptr + 2;

  strncpy(date_buf, ptr, 2);
  ptr = ptr + 2;

  strncpy(hour_buf, ptr, 2);
  ptr = ptr + 2;

  strncpy(min_buf, ptr, 2);
  ptr = ptr + 2;

  strncpy(sec_buf, ptr, 2);
  ptr = ptr + 2;

  t.tm_isdst = -1;
  t.tm_year = atoi(year_buf) - 1900; //tm_mon    The number of months since January, in the range 0 to 11.
  t.tm_mon = atoi(month_buf) - 1;
  t.tm_mday = atoi(date_buf);
  t.tm_hour = atoi(hour_buf);
  t.tm_min = atoi(min_buf);
  t.tm_sec = atoi(sec_buf);

  time_in_sec = (long long)mktime(&t);
  return(time_in_sec*1000); //convert into msec
}

int get_first_and_last_partition(char *NSLogsPath, int test_run_number, char partition_name[][PARTITION_LENGTH])
{
  char file_path[2024] = {0};
  char path[2024] = {0};
  char line_buff[2024] = {0};
  FILE *fp = NULL;
  int ret = 0;
  //'.curPartition' File format:
  //FirstPartitionIdx=20140917162543
  //CurPartitionIdx=20140917162543
  sprintf(path, "%s/TR%d", NSLogsPath, test_run_number);
  sprintf(file_path, "%s/TR%d/.curPartition", NSLogsPath, test_run_number);

  fp = fopen(file_path, "r");
  if(fp == NULL)
  {
    ret = -1;    
  }
  else
  {
    //save 'FirstPartitionIdx'
    if(nslb_fgets(line_buff, 2024, fp, 0) != NULL)
    {
      strcpy(partition_name[0], line_buff + FirstPartitionIdx_LENGTH);
      partition_name[0][14] = '\0'; //replace newline by null
    }

    //save 'CurPartitionIdx'
    if(nslb_fgets(line_buff, 2024, fp, 0) != NULL)
    {
      strcpy(partition_name[1], line_buff + CurPartitionIdx_LENGTH);
      partition_name[1][14] = '\0'; //replace newline by null
    }
    fclose(fp);
    fp = NULL;
  }
  return ret;
}

/*This function is to give 'Elapsed Time' in case of partition mode.
 * Steps:
 * (1) Make array of all the partitions present in TR dir.
 * (2) qsort partition array
 * (3) calculate time diff in msecs between last and first partition
 * (4) calculate last partition duration from its rtgMessage.dat using formula described in func get_actual_duration()
 * (5) Add (3) output & (4) output => it will give TestRun duration in msecs
 * (6) convert obtained duration in 'hh:mm:ss' format
 * */
long long get_duration(int test_run_number, char *NSLogsPath, char *total_time_str, int Rflag, int get_is_test_running_flag, char *partition_duration)
{
  char sum_top_file[2024] = {0};
  char partition_name[PARTITION_ARRAY_SIZE][PARTITION_LENGTH] = {{0}};
  char last_partition_duration_in_msec[100] = {0};
  //char total_time_str[1024] = {0};
  //DIR *tr_dir;
  //struct dirent *dir;
  //int index = 0;
  long long partition_duration_diff = 0;
  int ret = 0; 
  
  ret = get_first_and_last_partition(NSLogsPath, test_run_number, partition_name);

  if(ret == -1)
    return -1;

  if(partition_name[1][0] == '0')
    partition_duration_diff = 0;
  else
     //time diff in msecs between last and first partition
    partition_duration_diff = convert_partition_name_into_msec(partition_name[1]) - convert_partition_name_into_msec(partition_name[0]); 

  //calculate last partition duration from rtgMessage.dat
  sprintf(sum_top_file, "%s/TR%d/%s/summary.top", NSLogsPath, test_run_number, partition_name[1]);
  strcpy (last_partition_duration_in_msec, calc_duration (test_run_number, NSLogsPath, sum_top_file, NULL, 1, atoll(partition_name[1]), Rflag, get_is_test_running_flag));
  //strcpy (last_partition_duration_in_msec, calc_duration (test_run_number, NSLogsPath, sum_top_file, NULL, 1, atoll(partition_name[1]), Rflag, calc_duration));
  if(atoll(last_partition_duration_in_msec) == -1) {
    if(total_time_str)
      convert_to_hh_mm_ss(partition_duration_diff, total_time_str);
    return -2;
  }

  //add (diff between last and first partition) and (last partition rtgMessage.dat duration) to get TestRun duration
  partition_duration_diff = partition_duration_diff + local_get_time_from_format(last_partition_duration_in_msec);

  //convert in format hh_mm_ss
  if(total_time_str)
    convert_to_hh_mm_ss(partition_duration_diff, total_time_str);

  if(partition_duration)
    strcpy(partition_duration, last_partition_duration_in_msec);

  return partition_duration_diff;
}


int get_summary_top_start_time_in_secs(char *date_str) {

  char buf[1024]; //2/24/09  11:50:44
  struct tm mtm;

  strptime(date_str, "%D %T", &mtm);
  strftime(buf, sizeof(buf), "%s", &mtm);
  return(atoi(buf));
}

