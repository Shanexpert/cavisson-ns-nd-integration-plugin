#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "cdr_components.h"
#include "cdr_dir_operation.h"
#include "cdr_log.h"
#include "cdr_main.h"
#include "cdr_cache.h"
#include "nslb_util.h"

/*************************************************************************************************
 * get the size of all the contents in directory dir_name
 * returns CDR_ERROR if failed
 *************************************************************************************************/
long long get_dir_size(const char *dir_path)
{
  long long total_size = 0;
  DIR *dir;
  struct dirent *dir_e;
  struct stat st;
  char file_name[CDR_FILE_PATH_SIZE];

  dir = opendir(dir_path);
  if(dir == NULL) {
    CDRTL1("Error: Cannot open dir: %s, error: %s", dir_path, strerror(errno));
    return 0; //CDR_ERROR;
  }

  // read the files and subdirectories one by one
  while( (dir_e = readdir(dir)) != NULL) {
    // filter . and ..
    if( (strcmp(dir_e->d_name, ".") == 0) || (strcmp(dir_e->d_name, "..") == 0))
      continue;

    sprintf(file_name, "%s/%s", dir_path, dir_e->d_name);

    if( (stat(file_name, &st) != 0) ) {
      // CDRTL1("Error: stat error: %s    file: %s\n", strerror(errno), file_name);
      continue;
    }

    total_size += st.st_size;

    // if sub-dir get the size of its contents
    if(S_ISDIR(st.st_mode)) {
      total_size += get_dir_size(file_name);
    }
  }

  closedir(dir);
  return total_size;
}

#if 0
static int is_partition(const struct dirent *dir_e)
{
  /* Purpose: to check wether a directory is partition or not
   * TODO: Improve function
   */
  const char *dir_path;
  
  if(dir_e == NULL)
    return CDR_FALSE;
    
  dir_path = dir_e->d_name;

  if(!(dir_e->d_type == DT_DIR) && strlen(dir_path) != 14) // partition_dir length is 14
    return CDR_FALSE;
  
  // whole name should be numeric
  if(!ns_is_numeric((char *)dir_e->d_name))
    return CDR_FALSE;
  return CDR_TRUE;
}

/*********************************************************************************************
 *  get the total file size of a particular file 'file_name' in all partitions
 *  e.g.  rtgMessage.dat, .partition_info.txt
 *  if file_name is a dir, size of dir is returned
 *  if failed CDR_ERROR is return
 *  file_name should be relative to partition
 *********************************************************************************************/
static long long get_total_size_in_all_partitions(int tr_num, char *file_name)
{
  long long total_size = 0;
  DIR *dir;
  struct dirent **partition_list;
  struct stat st;
  char tr_path[CDR_FILE_PATH_SIZE];
  char name_buff[CDR_FILE_PATH_SIZE];

  sprintf(tr_path, "%s/logs/TR%d/", ns_wdir, tr_num);

  dir = opendir(tr_path);
  if(dir == NULL) {
    CDRTL1("Error: cannot open dir: %s, error: %s", tr_path, strerror(errno));
    return 0; //CDR_ERROR;
  }

  // get the list of partitions
  int n = scandir(tr_path, &partition_list, is_partition, NULL);
  if(n <= 0) {
    CDRTL1("Error: No partition directory present in: %s", tr_path);
    return 0; //CDR_ERROR;
  }

  while(n--) {
    // make file_name: <tr>/<partition>/<file_name>
    sprintf(name_buff, "%s/%s/%s", tr_path, partition_list[n]->d_name, file_name);

    if( (stat(name_buff, &st) != 0) ) {
      // CDRTL1("Error: stat error: %s, file: %s\n", strerror(errno), name_buff);
      continue;
    }
    
    if(S_ISDIR(st.st_mode))
      total_size += get_dir_size(name_buff);
    else
      total_size += st.st_size;
      //total_size += get_file_size(name_buff);
  }
  
  return total_size;
}

#endif

/* funtions to get the file size of component files */

long long get_key_files_size(int norm_id)
{
  /* get the total size of key files:
   *    1) summary.top
   *    2) sorted_scenario.conf
   *    3) <partition>/.partition_info.txt
   */
  long long total_size = 0;
  char path[CDR_FILE_PATH_SIZE];
  int i;

  sprintf(path, "%s/logs/TR%d/summary.top", ns_wdir, cdr_cache_entry_list[norm_id].tr_num);
  total_size += get_file_size(path);

  sprintf(path, "%s/logs/TR%d/sorted_scenario.conf", ns_wdir, cdr_cache_entry_list[norm_id].tr_num);
  total_size += get_file_size(path);
  
  for(i=0 ; i < cdr_cache_entry_list[norm_id].count; i++)
  {
    sprintf(path, "%s/logs/TR%d/%s/.partition_info.txt",
                      ns_wdir, cdr_cache_entry_list[norm_id].tr_num, cdr_cache_entry_list[norm_id].partition_list[i]->d_name);
    total_size += get_file_size(path);
  }

  return total_size;
}

long long get_greph_data_size(int norm_id)
{
  /* get the total size of metric data and percentile data. Files:
   *    1) <partition>/rtgMessage.dat
   *    2) <partition>/pctMessage.dat
   *    3) <partition>/all testRun.*
   */

  CDRTL2("Method called");
  char path[CDR_FILE_PATH_SIZE] = "\0";
  long long int total_size = 0; 
  int i;

  for(i=0 ; i < cdr_cache_entry_list[norm_id].count; i++)
  {
    sprintf(path, "%s/logs/TR%d/%s/", ns_wdir, cdr_cache_entry_list[norm_id].tr_num, cdr_cache_entry_list[norm_id].partition_list[i]->d_name);

    total_size += get_similar_file_size(path, rtg_filter);
    total_size += get_similar_file_size(path, pct_filter);
    total_size += get_similar_file_size(path, testrun_filter);
    sprintf(path, "%s/logs/TR%d/%s/percentile/", ns_wdir, 
                                 cdr_cache_entry_list[norm_id].tr_num, cdr_cache_entry_list[norm_id].partition_list[i]->d_name);
    total_size += get_dir_size_ex(path);

    sprintf(path, "%s/logs/TR%d/%s/transposed/", ns_wdir,
                                 cdr_cache_entry_list[norm_id].tr_num, cdr_cache_entry_list[norm_id].partition_list[i]->d_name);
    total_size += get_dir_size_ex(path);
  }
  CDRTL2("Method Exit, size '%lld'", total_size);
  return total_size;
}

long long get_har_file_size(int norm_id)
{
  /* Path: <partition>/rbu_logs/hars/
   */
  CDRTL2("Method called");
  char path[CDR_FILE_PATH_SIZE] = "\0";
  long long int total_size = 0; 
  int i;

  for(i=0 ; i < cdr_cache_entry_list[norm_id].count; i++)
  {
    sprintf(path, "%s/logs/TR%d/%s/rbu_logs/harp_files/", ns_wdir, 
                         cdr_cache_entry_list[norm_id].tr_num, cdr_cache_entry_list[norm_id].partition_list[i]->d_name);

    total_size += get_dir_size_ex(path);
    sprintf(path, "%s/logs/TR%d/%s/rbu_logs/snap_shots/", ns_wdir,
                         cdr_cache_entry_list[norm_id].tr_num, cdr_cache_entry_list[norm_id].partition_list[i]->d_name);

    total_size += get_dir_size_ex(path);
    sprintf(path, "%s/logs/TR%d/%s/rbu_logs/screen_shot/", ns_wdir,
                         cdr_cache_entry_list[norm_id].tr_num, cdr_cache_entry_list[norm_id].partition_list[i]->d_name);

    total_size += get_dir_size_ex(path);
  }
  CDRTL2("Method Exit, size '%lld'", total_size);
  return total_size;
}

long long get_db_file_size(int norm_id)
{
  /* Path: <partition>/rbu_logs/hars/
   */
  CDRTL2("Method called");
  char path[CDR_FILE_PATH_SIZE] = "\0";
  long long int total_size = 0;
  int i;

  for(i=0 ; i < cdr_cache_entry_list[norm_id].count; i++)
  { 
    sprintf(path, "%s/logs/TR%d/%s/nd/sqb", ns_wdir,
                          cdr_cache_entry_list[norm_id].tr_num, cdr_cache_entry_list[norm_id].partition_list[i]->d_name);
 
    total_size += get_dir_size_ex(path);
   
    sprintf(path, "%s/logs/TR%d/%s/nd/dat", ns_wdir,
                          cdr_cache_entry_list[norm_id].tr_num, cdr_cache_entry_list[norm_id].partition_list[i]->d_name);
 
    total_size += get_dir_size_ex(path);
   
    sprintf(path, "%s/logs/TR%d/%s/nd/hs", ns_wdir,
                          cdr_cache_entry_list[norm_id].tr_num, cdr_cache_entry_list[norm_id].partition_list[i]->d_name);
    total_size += get_dir_size_ex(path);
  }

  CDRTL2("Method Exit, size '%lld'", total_size);
  return total_size;
}


long long get_csv_size(int norm_id)
{
  /* Files:
   *    1) Url.csv
   *    2) session.csv
   *    3) page.csv
   *    4) transaction.csv
   *    5) <tr>/report/csv/alertHistory.csv
   *    6) <tr>/common_files/reports/
   *    7) <tr>/<partition>/reports/csv
   *    8) <tr>/<partition>/reports
   *    need more info for: NS DDR CSV, ND CSV
   */

  CDRTL2("Method called");
  char path[CDR_FILE_PATH_SIZE] = "\0";
  long long int total_size = 0; 
  int i;

  for(i=0 ; i < cdr_cache_entry_list[norm_id].count; i++)
  {
    sprintf(path, "%s/logs/TR%d/%s/reports/csv/", ns_wdir,  
                                cdr_cache_entry_list[norm_id].tr_num, cdr_cache_entry_list[norm_id].partition_list[i]->d_name);
    total_size += get_dir_size_ex(path);

    sprintf(path, "%s/logs/TR%d/%s/nd/csv/", ns_wdir,  
                                cdr_cache_entry_list[norm_id].tr_num, cdr_cache_entry_list[norm_id].partition_list[i]->d_name);
    total_size += get_dir_size_ex(path);
  }

  sprintf(path, "%s/logs/TR%d/common_files/reports/csv/", ns_wdir,  
                              cdr_cache_entry_list[norm_id].tr_num);
  total_size += get_dir_size_ex(path);

  sprintf(path, "%s/logs/TR%d/common_files/nd/csv/", ns_wdir,  
                              cdr_cache_entry_list[norm_id].tr_num);
  total_size += get_dir_size_ex(path);

  CDRTL2("Method Exit, size '%lld'", total_size);
  return total_size;

}

long long get_page_dump_size(int norm_id)
{
  /* Path: <tr>/<partition>/page_dump
   */
  CDRTL2("Method called");
  char path[CDR_FILE_PATH_SIZE] = "\0";
  long long int total_size = 0; 
  int i;

  for(i=0 ; i < cdr_cache_entry_list[norm_id].count; i++)
  {
    sprintf(path, "%s/logs/TR%d/%s/page_dump/", ns_wdir, 
                         cdr_cache_entry_list[norm_id].tr_num, cdr_cache_entry_list[norm_id].partition_list[i]->d_name);

    total_size += get_dir_size_ex(path);
  }
  CDRTL2("Method Exit, size '%lld'", total_size);
  return total_size;
}

long long get_logs_size(int norm_id)
{
  /*Files:
   *  1) error.log
   *  2) event.log
   *  3) <tr>/ready_reports/testinit_status/testInit.log
   *  4) <tr>/debug.log
   *  5) <tr>/<partition>/ns_logs
   *  6) <tr>/<partition>/monitor.log
   *  7) <tr>/<partition>nd/logs
   *  8) Need more info for Debug trace
   */

  CDRTL2("Method called");
  char path[CDR_FILE_PATH_SIZE] = "\0";
  long long int total_size = 0; 
  int i;

  sprintf(path, "%s/logs/TR%d/error.log", ns_wdir, cdr_cache_entry_list[norm_id].tr_num);
  total_size += get_file_size(path);

  sprintf(path, "%s/logs/TR%d/event.log", ns_wdir, cdr_cache_entry_list[norm_id].tr_num);
  total_size += get_file_size(path);

  sprintf(path, "%s/logs/TR%d/ready_reports/testinit_status/testInit.log", ns_wdir, cdr_cache_entry_list[norm_id].tr_num);
  total_size += get_file_size(path);

  sprintf(path, "%s/logs/TR%d/debug.log", ns_wdir, cdr_cache_entry_list[norm_id].tr_num);
  total_size += get_file_size(path);

  sprintf(path, "%s/logs/TR%d/TestRunOutput.log", ns_wdir, cdr_cache_entry_list[norm_id].tr_num);
  total_size += get_file_size(path);

  for(i=0 ; i < cdr_cache_entry_list[norm_id].count; i++)
  {
    sprintf(path, "%s/logs/TR%d/%s/ns_logs/", ns_wdir, 
                         cdr_cache_entry_list[norm_id].tr_num, cdr_cache_entry_list[norm_id].partition_list[i]->d_name);
    total_size += get_dir_size_ex(path);

    sprintf(path, "%s/logs/TR%d/%s/nd/logs/", ns_wdir, 
                         cdr_cache_entry_list[norm_id].tr_num, cdr_cache_entry_list[norm_id].partition_list[i]->d_name);
    total_size += get_dir_size_ex(path);

    sprintf(path, "%s/logs/TR%d/%s/monitor.log", ns_wdir, 
                         cdr_cache_entry_list[norm_id].tr_num, cdr_cache_entry_list[norm_id].partition_list[i]->d_name);
    total_size += get_file_size(path);

    sprintf(path, "%s/logs/TR%d/%s/error.log", ns_wdir, 
                         cdr_cache_entry_list[norm_id].tr_num, cdr_cache_entry_list[norm_id].partition_list[i]->d_name);
    total_size += get_file_size(path);

    sprintf(path, "%s/logs/TR%d/%s/event.log", ns_wdir, 
                         cdr_cache_entry_list[norm_id].tr_num, cdr_cache_entry_list[norm_id].partition_list[i]->d_name);
    total_size += get_file_size(path);

    sprintf(path, "%s/logs/TR%d/%s/debug.log", ns_wdir, 
                         cdr_cache_entry_list[norm_id].tr_num, cdr_cache_entry_list[norm_id].partition_list[i]->d_name);
    total_size += get_file_size(path);

    sprintf(path, "%s/logs/TR%d/%s/rbu_logs/", ns_wdir, 
                         cdr_cache_entry_list[norm_id].tr_num, cdr_cache_entry_list[norm_id].partition_list[i]->d_name);
    total_size += get_dir_size_ex(path);


  }
  CDRTL2("Method Exit, size '%lld'", total_size);
  return total_size;
}

long long get_raw_files_size(int norm_id)
{
  CDRTL2("Method called");
  char path[CDR_FILE_PATH_SIZE] = "\0";
  long long int total_size = 0; 
  int i;

  for(i=0 ; i < cdr_cache_entry_list[norm_id].count; i++)
  {
    sprintf(path, "%s/logs/TR%d/%s/nd/raw_data/", ns_wdir,  
                                cdr_cache_entry_list[norm_id].tr_num, cdr_cache_entry_list[norm_id].partition_list[i]->d_name);
    total_size += get_dir_size_ex(path);
    
    sprintf(path, "%s/logs/TR%d/%s/reports/raw_data/", ns_wdir,
                                cdr_cache_entry_list[norm_id].tr_num, cdr_cache_entry_list[norm_id].partition_list[i]->d_name);
    total_size += get_dir_size_ex(path);
  }

  CDRTL2("Method Exit, size '%lld'", total_size);
  return total_size;
}

long long get_test_data_size(int norm_id)
{
  CDRTL2("Method called");
  char path[CDR_FILE_PATH_SIZE] = "\0";
  long long int total_size = 0; 
  sprintf(path , "%s/logs/TR%d/scripts/", ns_wdir, cdr_cache_entry_list[norm_id].tr_num);

  total_size += get_dir_size_ex(path);

  for(int i=0 ; i < cdr_cache_entry_list[norm_id].count; i++)
  {
    sprintf(path, "%s/logs/TR%d/%s/server_logs/", ns_wdir,  
                                cdr_cache_entry_list[norm_id].tr_num, cdr_cache_entry_list[norm_id].partition_list[i]->d_name);
    total_size += get_dir_size_ex(path);
  }

  CDRTL2("Method Exit, size '%lld'", total_size);
  return total_size;
}

long long get_reports_size(int norm_id)
{
  /* Path: <partition>/rbu_logs/hars/
   */
  CDRTL2("Method called");
  char path[CDR_FILE_PATH_SIZE] = "\0";
  long long int total_size = 0;
  int i;

  for(i=0 ; i < cdr_cache_entry_list[norm_id].count; i++)
  {
    sprintf(path, "%s/logs/TR%d/%s/rbu_logs/lighthouse/", ns_wdir,
                         cdr_cache_entry_list[norm_id].tr_num, cdr_cache_entry_list[norm_id].partition_list[i]->d_name);

    total_size += get_dir_size_ex(path);
    sprintf(path, "%s/logs/TR%d/%s/rbu_logs/performance_trace/", ns_wdir,
                         cdr_cache_entry_list[norm_id].tr_num, cdr_cache_entry_list[norm_id].partition_list[i]->d_name);

    total_size += get_dir_size_ex(path);
  }
  CDRTL2("Method Exit, size '%lld'", total_size);
  return total_size;
}

