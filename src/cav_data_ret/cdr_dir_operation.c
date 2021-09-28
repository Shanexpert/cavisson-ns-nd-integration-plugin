#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>

#include "cdr_main.h"
#include "cdr_log.h"
#include "cdr_utils.h"
#include "nslb_util.h"


static int my_filter(const struct dirent *dir_e)
{
  if((nslb_get_file_type(logsPath, dir_e) == DT_DIR) && 
        ((dir_e->d_name[0] == '.' && dir_e->d_name[1] == '\0') ||
        (dir_e->d_name[0] == '.' && dir_e->d_name[1] == '.' && dir_e->d_name[2] == '\0'))) 
    return 0;
  return 1;
}

long long int get_dir_size_ex(char *path)
{
  long long int total_size = 0;
  //DIR *dir;
  struct dirent **file_list;
  struct stat st;
  char file_path[CDR_FILE_PATH_SIZE];
  char resolved_path[CDR_FILE_PATH_SIZE];
  char *stat_file_ptr;
  int n, i, num_bytes;
  struct stat parent_path_st;
  char flag_cal_parent_stat = 0;

  /*dir = opendir(path);
  if(dir == NULL) {
    CDRTL4("Error: cannot open dir: %s, error: %s", path, strerror(errno));
    return 0; //CDR_ERROR;
  }*/

  logsPath = path;
  n = scandir(path, &file_list, my_filter, NULL);

  for (i = 0; i < n; i++){
    sprintf(file_path, "%s/%s", path, file_list[i]->d_name);
    if((nslb_get_file_type(path, file_list[i])) == DT_DIR)
    {
      total_size += get_dir_size_ex(file_path); 
    }
    else 
    { 
      stat_file_ptr = file_path;
      if (file_list[i]->d_type == DT_LNK)
      {
        //if((stat_file_ptr = realpath(file_path, resolved_path)) == NULL)
        if((num_bytes = readlink(file_path, resolved_path, CDR_FILE_PATH_SIZE)) == -1)
        {
          CDRTL4("In getting real path of link : '%s', error: %s\n", file_path, strerror(errno));
          continue;
        }
        resolved_path[num_bytes] = '\0';
        stat_file_ptr = resolved_path;
        if(!flag_cal_parent_stat) {
          if( (stat(path, &parent_path_st) != 0) ) {
            continue;
          }
          flag_cal_parent_stat = 1;
        }
      } 
      if( (stat(stat_file_ptr, &st) != 0) ) {
        CDRTL4("Error: stat error: '%s', file: '%s'\n", strerror(errno), stat_file_ptr);
        continue;
      }
      if (file_list[i]->d_type == DT_LNK)
      {
        if(parent_path_st.st_ino == st.st_ino)
        {
          continue;
        }
      }
      if(S_ISDIR(st.st_mode))
      {
        total_size += get_dir_size_ex(stat_file_ptr); 
        continue;
      }
      total_size += st.st_size;
    } 
  }
  return total_size;
}


static int is_partition(const struct dirent *dir_e)
{
  if(!(nslb_get_file_type(logsPath, dir_e) == DT_DIR) || strlen(dir_e->d_name) != 14) // partition_dir length is 14
    return 0;
  if (dir_e->d_name[0] != '2' && dir_e->d_name[1] != '0')
    return 0;

  if(!ns_is_numeric((char *)dir_e->d_name))
    return 0;

  return 1;
}

static int partition_comparator(const void *p1, const void *p2)
{
  return (atoll ((*(struct dirent **)p1)->d_name) - atoll ((*(struct dirent **)p2)->d_name));
}

struct dirent **get_tr_partiton_list(int tr_num, int *count, char *path)
{
  //DIR *dir;
  struct dirent **partition_list;
  char tr_path[CDR_FILE_PATH_SIZE];

  if(path == NULL)
    sprintf(tr_path, "%s/logs/TR%d/", ns_wdir, tr_num);
  else 
    sprintf(tr_path, "%s", path);

  /*dir = opendir(tr_path);
  if(dir == NULL) {
    CDRTL4("Error: cannot open dir: %s, error: %s", tr_path, strerror(errno));
    return 0; //CDR_ERROR;
  }*/

  logsPath = tr_path;
  // get the list of partitions
  int n = scandir(tr_path, &partition_list, is_partition, NULL);
  if(n <= 0) {
    CDRTL4("Error: No partition directory present in: %s", tr_path);
    return 0; //CDR_ERROR;
  }

  *count = n;
  qsort(partition_list, n, sizeof(*partition_list), partition_comparator);
  return partition_list;
}

int rtg_filter(const struct dirent *dir_e)
{
  int d_type = nslb_get_file_type(logsPath, dir_e);
  if((d_type == DT_REG || d_type == DT_LNK) &&
        !(strncmp(dir_e->d_name, "rtgMessage.dat", 14 /*strlet("rtgMessage.dat")*/)))
    return 1;
  return 0;
}


int pct_filter(const struct dirent *dir_e)
{
  int d_type = nslb_get_file_type(logsPath, dir_e);
  if((d_type == DT_REG || d_type == DT_LNK) &&
        !(strncmp(dir_e->d_name, "pctMessage.dat", 14 /*strlet("pctMessage.dat")*/)))
    return 1;
  return 0;
}

int testrun_filter(const struct dirent *dir_e)
{
  int d_type = nslb_get_file_type(logsPath, dir_e);
  if((d_type == DT_REG || d_type == DT_LNK) &&
        !(strncmp(dir_e->d_name, "testrun.gdf", 11 /*strlet("testRun.gdf")*/)))
    return 1;
  return 0;
}

int testrunouput_log_filter(const struct dirent *dir_e)
{
  int d_type = nslb_get_file_type(logsPath, dir_e);
  if((d_type == DT_REG || d_type == DT_LNK) &&
        !(strncmp(dir_e->d_name, "TestRunOutput.log", 17 )))
    return 1;
  return 0;
}

int log_or_trace_filter(const struct dirent *dir_e)
{
  int d_type = nslb_get_file_type(logsPath, dir_e);
  if((d_type == DT_REG || d_type == DT_LNK) &&
        (strstr(dir_e->d_name, ".trace") || strstr(dir_e->d_name, ".log")))
    return 1;
  return 0;
}

int json_filter(const struct dirent *dir_e)
{
  int d_type = nslb_get_file_type(logsPath, dir_e);
  if((d_type == DT_REG || d_type == DT_LNK) &&
        (strstr(dir_e->d_name, ".json")))
    return 1;
  return 0;
}

long long int get_similar_file_size(char *path, int (*filter_fun_ptr)(const struct dirent *))
{
  long long int total_size = 0;
  //DIR *dir;
  struct dirent **file_list;
  struct stat st;
  char file_path[1024];
  int n, i;

  /*dir = opendir(path);
  if(dir == NULL) {
    CDRTL4("Error: cannot open dir: %s, error: %s", path, strerror(errno));
    return 0; //CDR_ERROR;
  }*/
  logsPath = path;
  n = scandir(path, &file_list, filter_fun_ptr, NULL);

  for (i = 0; i < n; i++){
    sprintf(file_path, "%s/%s", path, file_list[i]->d_name);
      if( (stat(file_path, &st) != 0) ) {
        CDRTL4("Error: stat error: '%s', file: '%s'\n", strerror(errno), file_path);
        continue;
      }
      //CDRTL1("------%s - %ld\n", stat_file_ptr, st.st_size);
      total_size += st.st_size;
  }
  //CDRTL2("Method Exit, path '%s' szie '%lld'", path, total_size);
  return total_size;
}


void remove_file(char *path)
{
  CDRTL2("Method called, path '%s'", path);
  unlink(path);
  CDRTTL("'%s'", path); 
  CDRTL2("Method exit"); 
}


void remove_dir(char *path)
{
  CDRTL2("Method called, path '%s'", path);
  rmdir(path);
  CDRTTL("'%s'", path); 
  CDRTL2("Method exit"); 
}

void remove_similar_file(char *path, int (*filter_fun_ptr)(const struct dirent *), int retention_time)
{
  struct dirent **file_list;
  char file_path[1024];
  int n, i;

  /*dir = opendir(path);
  if(dir == NULL) {
    CDRTL4("Error: cannot open dir: %s, error: %s", path, strerror(errno));
    return 0; //CDR_ERROR;
  }*/

  logsPath = path;
  n = scandir(path, &file_list, filter_fun_ptr, NULL);

  for (i = 0; i < n; i++){
    sprintf(file_path, "%s/%s", path, file_list[i]->d_name);
    if(retention_time != -1)
    {
      struct stat st;
      if( (stat(file_path, &st) != 0) ) {
        CDRTL4("Error: stat error: '%s', file: '%s'\n", strerror(errno), file_path);
        return;
      }
      int days = (cur_time_stamp - st.st_mtime) / (ONE_DAY_IN_SEC);
      if(days < retention_time)
        return;
    }
    remove_file(file_path);
  }
  //CDRTL2("Method Exit, path '%s' szie '%lld'", path, total_size);
}
static void remove_dir_file(char *path)
{
  //DIR *dir;
  struct dirent **file_list;
  struct stat st;
  char file_path[CDR_FILE_PATH_SIZE];
  char resolved_path[CDR_FILE_PATH_SIZE];
  char *stat_file_ptr;
  int n, i, num_bytes;
  struct stat parent_path_st;
  char flag_cal_parent_stat = 0;
  /*
  dir = opendir(path);
  if(dir == NULL) {
    CDRTL4("Error: cannot open dir: '%s', error: %s\n", path, strerror(errno));
    return; //CDR_ERROR;
  }*/

  logsPath = path;
  n = scandir(path, &file_list, my_filter, NULL);

  //fprintf(stderr, "get size '%s' '%d'\n", path, n);

  for (i = 0; i < n; i++){
    sprintf(file_path, "%s/%s", path, file_list[i]->d_name);
    if((nslb_get_file_type(path, file_list[i])) == DT_DIR)
    {
      remove_dir_file(file_path);
      remove_dir(file_path);
    }
    else 
    { 
      stat_file_ptr = file_path;
      if (file_list[i]->d_type == DT_LNK)
      {
        if((num_bytes = readlink(file_path, resolved_path, CDR_FILE_PATH_SIZE)) == -1)
        {
          CDRTL4("Error: In getting real path of link : '%s', error: %s\n", file_path, strerror(errno));
          continue;
        }
        resolved_path[num_bytes] = '\0';
        stat_file_ptr = resolved_path;
        if(!flag_cal_parent_stat) {
          if( (stat(path, &parent_path_st) != 0) ) {
            remove_file (file_path);
            continue;
          }
          flag_cal_parent_stat = 1;
        }
      }
      if( (stat(stat_file_ptr, &st) != 0) ) {
        CDRTL4("Error: stat error: '%s', file: '%s'\n", strerror(errno), stat_file_ptr);
        remove_file (file_path);
        continue;
      }
      if (file_list[i]->d_type == DT_LNK)
      {
        if(parent_path_st.st_ino == st.st_ino)
        {
          remove_file (file_path);
          continue;
        }
      }
      if(S_ISDIR(st.st_mode))
      {
        remove_dir_file(stat_file_ptr);
        remove_file(stat_file_ptr);
        remove_file (file_path);
        continue;
      }
      remove_file(stat_file_ptr);
      remove_file (file_path);
    } 
  }
  remove_dir(path);
}

void remove_dir_file_ex(char *path)
{
  //Check if pass path is link
  char resolved_path[CDR_FILE_PATH_SIZE];
  int num_bytes;
  struct stat st;
  if( (stat(path, &st) != 0) ) {
    CDRTL4("Error: stat error: '%s', file: '%s'\n", strerror(errno), path);
    return;
  }
  if(S_ISLNK(st.st_mode))
  {
    //get the actual path
    if((num_bytes = readlink(path, resolved_path, CDR_FILE_PATH_SIZE)) == -1)
    {
      CDRTL4("Error: In getting real path of link : '%s', error: %s\n", path, strerror(errno));
      return;
    }
    resolved_path[num_bytes] = '\0';
    remove_dir_file(resolved_path);
  }
  else if(S_ISDIR(st.st_mode))
  {
    remove_dir_file(path);
  }
  else
    remove_file(path);
}

/*************************************************************************************************
 * get the file size in bytes
 * returns CDR_ERROR if failed
 * if file_path is a directory, the size without the contents if the dir is return
 *************************************************************************************************/
long long get_file_size(const char *file_path)
{
  struct stat st;

  if( (stat(file_path, &st) != 0) ) {
    // CDRTL1("Error: %s, file: %s\n", strerror(errno), file_path);
    return 0; //CDR_ERROR;
  }

  return st.st_size;
}


static int cdr_check_core_file(char *file_name)
{
  char tmp_file_name[1024] ;
  sprintf(tmp_file_name, "%s", file_name);
  
  char *home_ptr;
  char *cav_ptr;
  char *con_ptr;
  char *ptr;

  CDRTL2("Method called");
  if(!(ptr = strstr(tmp_file_name, "core.!")))
    return 1;

  home_ptr = ptr + 6; 

  if(strncmp(home_ptr, "home!", 5))
    return 1;

  cav_ptr = home_ptr + 4;
  *cav_ptr = '\0';
  cav_ptr++;

  if(strncmp(cav_ptr, "cavisson!", 9))
    return 1;

  con_ptr = cav_ptr + 8;
  *con_ptr = '\0';
  con_ptr++;

  ptr = strchr(con_ptr, '!');

  if(!ptr)
    return 1;

  *ptr = '\0';

  CDRTL4("home_ptr='%s', cav_ptr= '%s', con_ptr='%s'\n", home_ptr, cav_ptr, con_ptr);

  char lol_ns_wdir[128];
  sprintf(lol_ns_wdir, "/%s/%s/%s", home_ptr, cav_ptr,con_ptr);

  CDRTL4("ns_wdir = '%s', lol_ns_wdir = '%s'\n", ns_wdir, lol_ns_wdir);
  if(!strcmp(ns_wdir, lol_ns_wdir))
    return 1;

  return 0;
}


static void remove_dir_with_retention_time(char *path, int retention_time, int check_core_file_flag)
{
  DIR *dir;
  struct stat st;
  struct dirent *de;
  int days;
  char file_path[1024];
  long long int total_time;

  dir = opendir(path);

  if(dir == NULL) {
    CDRTL4("Error: cannot open dir: '%s', error: %s\n", path, strerror(errno));
    return;
  }

  while ((de = readdir(dir)) != NULL ) 
  {
    sprintf (file_path, "%s/%s", path, de->d_name);
    if (check_core_file_flag)
    {
       if(!cdr_check_core_file(file_path))
         continue;
    }

    if (stat (file_path, &st))
    {
      CDRTL4("Error: cannot find file_path : '%s', error: %s\n", file_path, strerror(errno));
    }
    
    CDRTL4("file_path '%s'- %lld - %lld / %lld", file_path, cur_time_stamp, st.st_mtime, ONE_DAY_IN_SEC);

    days = (cur_time_stamp - st.st_mtime) / (ONE_DAY_IN_SEC);

    if(days < retention_time)
      continue;

    else 
    {
      //if (de->d_type != DT_DIR){
      if (!S_ISDIR(st.st_mode)){
        total_time = get_ts_in_ms();
        remove_file(file_path);
        total_time = get_ts_in_ms() - total_time;
        CDRAL(0, 0, 0, path, st.st_size, "Delete", "Delete due to retention time", total_time, "Retention Manager");
      }
    }
  }
  closedir(dir);
}


void remove_dir_file_with_retention_time_ex(char *path, int retention_time, int check_core_file_flag)
{
  //Check if pass path is link
  struct stat st;
  if( (stat(path, &st) != 0) ) {
    CDRTL4("Error: stat error: '%s', file: '%s'\n", strerror(errno), path);
    return;
  }
  else if(S_ISDIR(st.st_mode))
  {
    remove_dir_with_retention_time(path, retention_time, check_core_file_flag); 
  }
  else 
  {
    int days = (cur_time_stamp - st.st_mtime) / (ONE_DAY_IN_SEC);
    if(days < retention_time)
      return;
    
    long long int total_time = get_ts_in_ms();
    remove_file(path);
    total_time = get_ts_in_ms() - total_time;
    CDRAL(0, 0, 0, path, st.st_size, "Delete", "Delete due to retention time", total_time, "Retention Manager");
  }
}
