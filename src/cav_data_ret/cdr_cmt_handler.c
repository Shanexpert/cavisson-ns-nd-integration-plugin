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
#include "cdr_cmt_handler.h"
#include "cdr_config.h"
#include "cdr_cleanup.h"
#include "cdr_utils.h"
#include "cdr_drop_tables.h"
#include "nslb_util.h"
#include "nslb_partition.h"


long long int get_partition_disk_size(int norm_id)
{
  CDRTL2("Method called. CMT TR '%d' cache_line: '%lld'", 
                        cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);
  char path[CDR_FILE_PATH_SIZE] = "\0";

  sprintf(path, "%s/logs/TR%d/%lld/", ns_wdir, 
                        cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);
  long long int total_size = get_dir_size_ex(path);
  
  CDRTL2("Method Exit, size '%lld'", total_size);
  return total_size;
}

long long int get_partition_graph_data_size(int norm_id)
{
  CDRTL2("Method called. CMT TR '%d' cache_line: '%lld'", 
                        cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);

  char path[CDR_FILE_PATH_SIZE] = "\0";
  long long int total_size = 0; 

  sprintf(path, "%s/logs/TR%d/%lld/", ns_wdir, 
                       cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);

  total_size += get_similar_file_size(path, rtg_filter);
  total_size += get_similar_file_size(path, pct_filter);
  total_size += get_similar_file_size(path, testrun_filter);

  sprintf(path, "%s/logs/TR%d/%lld/percentile/", ns_wdir, 
                       cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);
  total_size += get_dir_size_ex(path);

  sprintf(path, "%s/logs/TR%d/%lld/transposed/", ns_wdir,
                       cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);
  total_size += get_dir_size_ex(path);
  
  CDRTL2("Method Exit, size '%lld'", total_size);
  return total_size;
}

long long int get_partition_csv_size(int norm_id)
{
  CDRTL2("Method called. CMT TR '%d' cache_line: '%lld'", 
                        cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);

  char path[CDR_FILE_PATH_SIZE] = "\0";
  long long int total_size = 0; 

  sprintf(path, "%s/logs/TR%d/%lld/reports/csv/", ns_wdir,
                       cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);
  total_size += get_dir_size_ex(path);
  
  sprintf(path, "%s/logs/TR%d/%lld/nd/csv/", ns_wdir,
                       cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);
  total_size += get_dir_size_ex(path);

  CDRTL2("Method Exit, size '%lld'", total_size);
  return total_size;
}

long long int get_partition_raw_files_size(int norm_id)
{
  CDRTL2("Method called. CMT TR '%d' cache_line: '%lld'", 
                        cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);

  char path[CDR_FILE_PATH_SIZE] = "\0";
  long long int total_size = 0; 

  sprintf(path, "%s/logs/TR%d/%lld/nd/raw_data/", ns_wdir,
                       cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);
  total_size += get_dir_size_ex(path);

  sprintf(path, "%s/logs/TR%d/%lld/reports/raw_data/", ns_wdir,
                       cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);
  total_size += get_dir_size_ex(path);

  CDRTL2("Method Exit, size '%lld'", total_size);
  return total_size;
}

long long int get_partition_db_file_size(int norm_id)
{
  CDRTL2("Method called. CMT TR '%d' cache_line: '%lld'", 
                        cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);

  char path[CDR_FILE_PATH_SIZE] = "\0";
  long long int total_size = 0; 

  sprintf(path, "%s/logs/TR%d/%lld/nd/sqb", ns_wdir,
                       cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);
  total_size += get_dir_size_ex(path);

  sprintf(path, "%s/logs/TR%d/%lld/nd/dat", ns_wdir,
                       cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);
  total_size += get_dir_size_ex(path);

  sprintf(path, "%s/logs/TR%d/%lld/nd/hs", ns_wdir,
                       cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);
  total_size += get_dir_size_ex(path);


  CDRTL2("Method Exit, size '%lld'", total_size);
  return total_size;

}

/*long long int get_partition_db_table_size(int norm_id)
{
  CDRTL2("Method called. CMT TR '%d' cache_line: '%lld'", 
                        cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);

  long long int total_size = 0; 

  CDRTL2("Method Exit, size '%lld'", total_size);
  return total_size;
}

long long int get_partition_db_index_size(int norm_id)
{
  CDRTL2("Method called. CMT TR '%d' cache_line: '%lld'", 
                        cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);

  long long int total_size = 0; 

  CDRTL2("Method Exit, size '%lld'", total_size);
  return total_size;
}
*/

long long int get_partition_har_file_size(int norm_id)
{
  CDRTL2("Method called. CMT TR '%d' cache_line: '%lld'", 
                        cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);

  char path[CDR_FILE_PATH_SIZE] = "\0";
  long long int total_size = 0;

  sprintf(path, "%s/logs/TR%d/%lld/rbu_logs/harp_files/", ns_wdir,
                        cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);

  total_size += get_dir_size_ex(path);
 
  sprintf(path, "%s/logs/TR%d/%lld/rbu_logs/snap_shots/", ns_wdir,
                       cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);
 
  total_size += get_dir_size_ex(path);
 
  sprintf(path, "%s/logs/TR%d/%lld/rbu_logs/screen_shot/", ns_wdir,
                       cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);

  total_size += get_dir_size_ex(path);

  CDRTL2("Method Exit, size '%lld'", total_size);
  return total_size;
}

long long int get_partition_page_dump_size(int norm_id)
{
  CDRTL2("Method called. CMT TR '%d' cache_line: '%lld'", 
                        cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);

  char path[CDR_FILE_PATH_SIZE] = "\0";
  long long int total_size = 0;

  sprintf(path, "%s/logs/TR%d/%lld/page_dump/", ns_wdir,
                        cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);

  total_size += get_dir_size_ex(path);

  CDRTL2("Method Exit, size '%lld'", total_size);
  return total_size;
}

long long int get_partition_logs_size(int norm_id)
{
  CDRTL2("Method called. CMT TR '%d' cache_line: '%lld'", 
                        cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);

  char path[CDR_FILE_PATH_SIZE] = "\0";
  long long int total_size = 0;

  sprintf(path, "%s/logs/TR%d/%lld/ns_logs/", ns_wdir,
                        cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);
  total_size += get_dir_size_ex(path);

  sprintf(path, "%s/logs/TR%d/%lld/nd/logs/", ns_wdir,
                        cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);
  total_size += get_dir_size_ex(path);

  sprintf(path, "%s/logs/TR%d/%lld/monitor.log", ns_wdir,
                        cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);
  total_size += get_file_size(path);

  sprintf(path, "%s/logs/TR%d/%lld/error.log", ns_wdir,
                        cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);
  total_size += get_file_size(path);

  sprintf(path, "%s/logs/TR%d/%lld/event.log", ns_wdir,
                        cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);
  total_size += get_file_size(path);

  sprintf(path, "%s/logs/TR%d/%lld/debug.log", ns_wdir,
                        cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);
  total_size += get_file_size(path);

  sprintf(path, "%s/logs/TR%d/%lld/rbu_logs/", ns_wdir,
                        cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);
  total_size += get_dir_size_ex(path);

  CDRTL2("Method Exit, size '%lld'", total_size);
  return total_size;
}

long long int get_partition_reports_size(int norm_id)
{
  CDRTL2("Method called. CMT TR '%d' cache_line: '%lld'",
                        cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);

  char path[CDR_FILE_PATH_SIZE] = "\0";
  long long int total_size = 0;

  sprintf(path, "%s/logs/TR%d/%lld/rbu_logs/lighthouse/", ns_wdir,
                        cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);

  total_size += get_dir_size_ex(path);

  sprintf(path, "%s/logs/TR%d/%lld/rbu_logs/performance_trace/", ns_wdir,
                       cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);
					   
  total_size += get_dir_size_ex(path);

  total_size += get_similar_file_size(path, testrunouput_log_filter);

  CDRTL2("Method Exit, size '%lld'", total_size);
  return total_size;
}

int partition_is_running(int tr_num, long long int partition_num)
{
  FILE *fp = NULL;
  char file_path[CDR_FILE_PATH_SIZE] = "\0";
  char buff[CDR_BUFFER_SIZE] = "\0";
  long long int cur_partition = 0;

  CDRTL2("Method called. CMT TR '%d' cache_line: '%lld'", tr_num, partition_num);
  sprintf(file_path, "%s/logs/TR%d/.curPartition", ns_wdir, tr_num);
  
  fp = fopen(file_path, "r") ;
  if(!fp) {
    CDRTL1("Error: cannot open: %s", file_path);
    return CDR_ERROR;
  }

  while(nslb_fgets(buff, CDR_BUFFER_SIZE, fp, 0) != NULL)
  {
    if(strncmp(buff, "CurPartitionIdx=", 16) == 0) {
      cur_partition = atoll((buff + 16));
    }
  }
  CDRTL4("CurPartitionIdx='%lld', partition_num='%lld'", cur_partition, partition_num);

  if(partition_num == cur_partition)
    return CDR_TRUE;
  
  CDRTL2("Method exit");
  return CDR_FALSE;
}

static int partition_is_bad(int tr_num, long long int partition_num)
{
  CDRTL2("Method called. CMT TR '%d' cache_line: '%lld'", tr_num, partition_num);

  struct stat st;
  char key_files[4][CDR_FILE_PATH_SIZE];

  sprintf(key_files[0], "%s/logs/TR%d/%lld/rtgMessage.dat", ns_wdir, tr_num, partition_num);
  //sprintf(key_files[1], "%s/logs/TR%d/%lld/pctMessage.dat", ns_wdir, tr_num, partition_num);
  sprintf(key_files[1], "%s/logs/TR%d/%lld/testrun.gdf", ns_wdir, tr_num, partition_num);
  sprintf(key_files[2], "%s/logs/TR%d/%lld/.partition_info.txt", ns_wdir, tr_num, partition_num);

  for(int i = 0; i < 3; ++i) {
    if(stat(key_files[i], &st) != 0) {
      CDRTL1("Error: bad partition '%lld' file '%s', error: %s\n", partition_num, key_files[i], strerror(errno));
      return CDR_TRUE;
    }
  }

  CDRTL2("Method exit");
  return CDR_FALSE;
}

int get_partition_type(int tr_num, long long int partition_num)
{
  CDRTL2("Method called. CMT TR '%d' cache_line: '%lld'", tr_num, partition_num);

  if(partition_is_running(tr_num, partition_num) == CDR_TRUE)
    return RUNNING_PARTITION;

  if(partition_is_bad(tr_num, partition_num))
  {
    remove_partition(tr_num, partition_num);
    return BAD_PARTITION;
  }

  return NORMAL_PARTITION;
}

void update_partition_info(int tr_num, long long int partition_num)
{
  //TODO: partition_info 
  CDRTL2("Method called. CMT TR '%d' partition num: '%lld'", tr_num, partition_num);

  PartitionInfo partInfo;
  PartitionInfo prev_partInfo;
  PartitionInfo next_partInfo;
  char base_dir[CDR_FILE_PATH_SIZE] = "\0";
  char *multidisk_paths = NULL;
  

  // Check in nslb_partition.c base_dir path till TR e.g. /home/cavisson/work/logs/TR1111/
  sprintf(base_dir, "%s/%d/", ns_wdir, tr_num);

  
  //get the info of partiiton to be removed
  char partition_name[64 + 1] = "";
  sprintf(partition_name, "%lld", partition_num);
  if(nslb_get_partition_info(base_dir, &partInfo, partition_name) < 0)
  {
    CDRTL1("Error: error in getting partition info. CMT TR '%d' partition num: '%lld'", tr_num, partition_num);
    return ;
  }
  
  //get the infor of next partition  
  if (partInfo.next_partition != 0) // this should always true for data retention case, last partition should not remove
  {
    sprintf(partition_name, "%lld", partInfo.next_partition);
    if(nslb_get_partition_info(base_dir, &next_partInfo, partition_name) < 0)
    {
      CDRTL1("Error: error in getting partition info. CMT TR '%d' partition num: '%lld'", tr_num, partition_num);
      return ;
    }
    next_partInfo.prev_partition = partInfo.prev_partition;

    nslb_save_partition_info_ex(base_dir, next_partInfo.cur_partition, &next_partInfo, multidisk_paths);
  }

  //get the infor of prev partition  
  if (partInfo.prev_partition != 0) // if this statement is fase that means this is first partition, should update .curPartiton in TR
  {
    sprintf(partition_name, "%lld", partInfo.prev_partition);
    if(nslb_get_partition_info(base_dir, &prev_partInfo, partition_name) < 0)
    {
      CDRTL1("Error: error in getting partition info. CMT TR '%d' partition num: '%lld'", tr_num, partition_num);
      return ;
    }
    prev_partInfo.next_partition = partInfo.next_partition;
    nslb_save_partition_info_ex(base_dir, prev_partInfo.cur_partition, &prev_partInfo, multidisk_paths);
  }
  else 
  {
    // update cujr partition in TR
  }

  CDRTL2("Method exit");
}

void remove_partition(int tr_num, long long int partition_num)
{
  CDRTL2("Method called. CMT TR '%d' cache_line: '%lld'", tr_num, partition_num);
  CDRTL3("Bad partition: [%lld]. Removing...", partition_num);

  char path[CDR_FILE_PATH_SIZE] = "\0";
  long long int total_time = get_ts_in_ms();

  //update_partition_info(tr_num, partition_num);
  sprintf(path, "%s/logs/TR%d/%lld/", ns_wdir, tr_num, partition_num);
  remove_dir_file_ex(path);
  total_time = get_ts_in_ms() - total_time;

    CDRAL(0, tr_num, partition_num, "Partition",  0, "Delete", "Delete due bad partition", total_time, "Retention Manager");


  CDRTL2("Method exit");
}

static void cmt_check_ngve_cleanup_days(long long int partition_num, struct ngve_cleanup_days_struct *days, long long int *flag)
{
  CDRTL2("Method called,  partition num '%lld'", partition_num);
  for(int i = 0; i < days->total_entry; i++)
  {
    if(partition_num >= days->start_date_pf[i] && partition_num <= days->end_date_pf[i]) 
      *flag = CONFIG_FALSE;
  }
  CDRTL2("Method exit");
}



void cmt_check_ngve_range(int norm_id, int cur_idx)
{
  CDRTL2("Method called. partition num: '%lld'", cdr_cmt_cache_entry_list[norm_id].partition_num);
  if (cur_idx == (cdr_cache_entry_list[cmt_tr_cache_idx].count -1)) //current partition or should be running partition
  {
    CDRTL1("Info: Partition '%lld' is current partition. Ignoring", cdr_cmt_cache_entry_list[norm_id].partition_num);
    return; 
  } 

  struct ngve_cleanup_days_struct *days;
  long long int partition_num = cdr_cmt_cache_entry_list[norm_id].partition_num;

  days = &(cdr_config.ngve_cleanup_days[GRAPH_DATA]);
  cmt_check_ngve_cleanup_days(partition_num, days, &(cdr_cmt_cache_entry_list[norm_id].partition_graph_data_remove_f));

  days = &(cdr_config.ngve_cleanup_days[CSV]);
  cmt_check_ngve_cleanup_days(partition_num, days, &(cdr_cmt_cache_entry_list[norm_id].partition_csv_remove_f));

  days = &(cdr_config.ngve_cleanup_days[RAW_DATA]);
  cmt_check_ngve_cleanup_days(partition_num, days, &(cdr_cmt_cache_entry_list[norm_id].partition_raw_file_remove_f));

  days = &(cdr_config.ngve_cleanup_days[DB]);
  cmt_check_ngve_cleanup_days(partition_num, days, &(cdr_cmt_cache_entry_list[norm_id].partition_db_remove_f));

  days = &(cdr_config.ngve_cleanup_days[HAR_FILE]);
  cmt_check_ngve_cleanup_days(partition_num, days, &(cdr_cmt_cache_entry_list[norm_id].partition_har_file_remove_f));

  days = &(cdr_config.ngve_cleanup_days[PAGEDUMP]);
  cmt_check_ngve_cleanup_days(partition_num, days, &(cdr_cmt_cache_entry_list[norm_id].partition_page_dump_remove_f));

  days = &(cdr_config.ngve_cleanup_days[LOGS]);
  cmt_check_ngve_cleanup_days(partition_num, days, &(cdr_cmt_cache_entry_list[norm_id].partition_logs_remove_f));

  days = &(cdr_config.ngve_cleanup_days[REPORTS]);
  cmt_check_ngve_cleanup_days(partition_num, days, &(cdr_cmt_cache_entry_list[norm_id].partition_reports_remove_f));

  CDRTL2("Method exit");
}

void remove_partition_csv(int norm_id)
{
  CDRTL2("Method called. CMT TR '%d' partition_num: '%lld'",
                        cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);

  char path[CDR_FILE_PATH_SIZE] = "\0";
  long long int total_time = get_ts_in_ms();

  sprintf(path, "%s/logs/TR%d/%lld/reports/csv/", ns_wdir,
                       cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num); 
  remove_dir_file_ex(path);
  
  sprintf(path, "%s/logs/TR%d/%lld/nd/csv/", ns_wdir,
                       cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);
  remove_dir_file_ex(path);

  total_time = get_ts_in_ms() - total_time;

  CDRAL(0, cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num, "Partition-CSV",  cdr_cmt_cache_entry_list[norm_id].partition_csv_size, "Delete", "Delete due to retention time", total_time, "Retention Manager");


  CDRTL2("Method Exit");
}

void remove_partition_raw_data(int norm_id)
{
  CDRTL2("Method called. CMT TR '%d' partition_num: '%lld'",
                        cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);

  char path[CDR_FILE_PATH_SIZE] = "\0";
  long long int total_time = get_ts_in_ms();

  sprintf(path, "%s/logs/TR%d/%lld/nd/raw_data/", ns_wdir,
                       cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);
  remove_dir_file_ex(path);

  sprintf(path, "%s/logs/TR%d/%lld/reports/raw_data/", ns_wdir,
                       cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);
  remove_dir_file_ex(path);

  total_time = get_ts_in_ms() - total_time;

  CDRAL(0, cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num, "Partition-RAW_DATA",  cdr_cmt_cache_entry_list[norm_id].partition_raw_file_size, "Delete", "Delete due to retention time", total_time, "Retention Manager");

  CDRTL2("Method Exit");
}

static void remove_partition_db_file(int norm_id)
{

  CDRTL2("Method called. CMT TR '%d' partition_num: '%lld'",
                        cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);

  char path[CDR_FILE_PATH_SIZE] = "\0";

  sprintf(path, "%s/logs/TR%d/%lld/nd/sqb", ns_wdir, 
                       cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);
  remove_dir_file_ex(path);

  sprintf(path, "%s/logs/TR%d/%lld/nd/dat", ns_wdir, 
                       cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);
  remove_dir_file_ex(path);

  sprintf(path, "%s/logs/TR%d/%lld/nd/hs", ns_wdir, 
                       cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);
  remove_dir_file_ex(path);


  CDRTL2("Method Exit");
}

void remove_partition_db(int norm_id)
{
  CDRTL2("Method called. CMT TR '%d' partition_num: '%lld'",
                        cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);

  long long int total_time = get_ts_in_ms();

  make_query_and_drop_tables(cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);
  remove_partition_db_file(norm_id);

  total_time = get_ts_in_ms() - total_time;

  CDRAL(0, cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num, "Partition-DB",  cdr_cmt_cache_entry_list[norm_id].partition_db_index_size+cdr_cmt_cache_entry_list[norm_id].partition_db_table_size, "Delete", "Delete due to retention time", total_time, "Retention Manager");

  CDRTL2("Method Exit");
}

void remove_partition_har_file(int norm_id)
{
CDRTL2("Method called. CMT TR '%d' partition_num: '%lld'",
                        cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);

  char path[CDR_FILE_PATH_SIZE] = "\0";

  long long int total_time = get_ts_in_ms();

  sprintf(path, "%s/logs/TR%d/%lld/rbu_logs/harp_files/", ns_wdir,
                       cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);
  remove_dir_file_ex(path);
 
  sprintf(path, "%s/logs/TR%d/%lld/rbu_logs/snap_shots/", ns_wdir,
                       cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);
  remove_dir_file_ex(path);
  
  sprintf(path, "%s/logs/TR%d/%lld/rbu_logs/screen_shot/", ns_wdir,
                       cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);
  remove_dir_file_ex(path);

  total_time = get_ts_in_ms() - total_time;

  CDRAL(0, cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num, "Partition-HAR_FILE",  cdr_cmt_cache_entry_list[norm_id].partition_har_file_size, "Delete", "Delete due to retention time", total_time, "Retention Manager");


  CDRTL2("Method Exit");

}

void remove_partition_page_dump(int norm_id)
{
  CDRTL2("Method called. CMT TR '%d' partition_num: '%lld'",
                        cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);

  char path[CDR_FILE_PATH_SIZE] = "\0";

  long long int total_time = get_ts_in_ms();


  sprintf(path, "%s/logs/TR%d/%lld/page_dump/", ns_wdir,
                       cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);
  remove_dir_file_ex(path);

  total_time = get_ts_in_ms() - total_time;

CDRAL(0, cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num, "Partition-PAGE_DUMP",  cdr_cmt_cache_entry_list[norm_id].partition_page_dump_size, "Delete", "Delete due to retention time", total_time, "Retention Manager");

  CDRTL2("Method Exit");

}

void remove_partition_logs(int norm_id)
{
  CDRTL2("Method called. CMT TR '%d' partition_num: '%lld'",
                        cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);

  struct cleanup_struct *cleanup_policy = g_cmt_cleanup_policy_ptr;
 
  char path[CDR_FILE_PATH_SIZE] = "\0";

  long long int total_time = get_ts_in_ms();

  sprintf(path, "%s/logs/TR%d/%lld/ns_logs/", ns_wdir,
                       cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);
  remove_dir_file_ex(path);

  sprintf(path, "%s/logs/TR%d/%lld/nd/logs/", ns_wdir,
                       cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);
  remove_dir_file_ex(path);

  sprintf(path, "%s/logs/TR%d/%lld/monitor.log", ns_wdir,
                       cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);
  remove_dir_file_ex(path);

  sprintf(path, "%s/logs/TR%d/%lld/error.log", ns_wdir,
                       cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);
  remove_dir_file_ex(path);

  sprintf(path, "%s/logs/TR%d/%lld/event.log", ns_wdir,
                       cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);
  remove_dir_file_ex(path);

  sprintf(path, "%s/logs/TR%d/%lld/debug.log", ns_wdir,
                       cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);
  remove_dir_file_ex(path);

  sprintf(path, "%s/logs/TR%d/%lld/rbu_logs/", ns_wdir,
                       cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);
  remove_dir_file_ex(path);

  sprintf(path, "%s/logs/TR%d/", ns_wdir,cdr_cache_entry_list[cmt_tr_cache_idx].tr_num);
  remove_similar_file(path, testrunouput_log_filter, cleanup_policy[LOGS].retention_time);

  total_time = get_ts_in_ms() - total_time;

  CDRAL(0, cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num, "Partition-LOGS",  cdr_cmt_cache_entry_list[norm_id].partition_logs_size, "Delete", "Delete due to retention time", total_time, "Retention Manager");

  CDRTL2("Method Exit");

}

void remove_partition_reports(int norm_id)
{
  CDRTL2("Method called. CMT TR '%d' partition_num: '%lld'",
                        cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);

  char path[CDR_FILE_PATH_SIZE] = "\0";

  long long int total_time = get_ts_in_ms();

  sprintf(path, "%s/logs/TR%d/%lld/rbu_logs/lighthouse/", ns_wdir,
                       cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);
  remove_dir_file_ex(path);

  sprintf(path, "%s/logs/TR%d/%lld/rbu_logs/performance_trace/", ns_wdir,
                       cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num);
  remove_dir_file_ex(path);

  total_time = get_ts_in_ms() - total_time;

  CDRAL(0, cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cmt_cache_entry_list[norm_id].partition_num, "Partition-REPORTS",  cdr_cmt_cache_entry_list[norm_id].partition_reports_size, "Delete", "Delete due to retention time", total_time, "Retention Manager");

  CDRTL2("Method Exit");
}

void cmt_cleanup_process()
{
  CDRTL2("Method called");

  if(cmt_tr_cache_idx == -1)
      return;
  if(cdr_cache_entry_list[cmt_tr_cache_idx].partition_list == NULL)
  { 
    CDRTL3("Info: No Partition in CMT tr '%d'.", cdr_cache_entry_list[cmt_tr_cache_idx].tr_num);
    return;
  }

  CDRTL3("Partition_list is set for CMT tr '%d', total number of partition '%d'.",
            cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, cdr_cache_entry_list[cmt_tr_cache_idx].count);

  long int lol_time = cur_time_stamp;
  if(cur_time_in_ngve_days() == TRUE)
  {
    // Only house keeing work - get the new TRs and size of the comonents
    CDRTL3("Current date: [%s] is in negative days.", ctime(&(lol_time)));
    cdr_config.data_remove_flag = FALSE;
  }

  if(cdr_config.cleanup_flag == CDR_FALSE)
  {
    CDRTL1("Info: Cleanup policy is not set for CMT TR");
    return;
  }

  int i;
  struct cdr_cmt_cache_entry *entry;

  /*Partion are save in increment format
    starting loop from last to first */
  char partition_graph_data_remove_f = FALSE;
  char partition_csv_remove_f = FALSE;
  char partition_raw_file_remove_f = FALSE;
  char partition_db_remove_f = FALSE;
  char partition_har_file_remove_f = FALSE;
  char partition_page_dump_remove_f = FALSE;
  char partition_logs_remove_f = FALSE;
  char partition_reports_remove_f = FALSE;
 
  struct cleanup_struct *cleanup_policy = g_cmt_cleanup_policy_ptr;
  char partition_num_buf[16];
  long long int partition_ts;
  int partition_days;

  for(i = total_cmt_cache_entry -1; i >= 0 ; i--)
  {
    entry = &(cdr_cmt_cache_entry_list[i]);

    if(entry->is_partition_present == CDR_FALSE)
    {
       //Partition is not present in disk
       CDRTL4("Partition: '%lld' was present earlier with size: '%lld', now not present in disk, So ignoring.", 
                         entry->partition_num, entry->partition_disk_size);
       // TODO: audit log
       continue;
    }

    if(entry->partition_type == RUNNING_PARTITION)
    {
      CDRTL3("Partition: [%lld] is running partition. Ignoring...", entry->partition_num);
      continue;
    }
  
    //If controll read here for last partion and not in running stat ignore that parttition 
    if( i == (total_cmt_cache_entry -1)) 
    {
      CDRTL1("Error: This condition should not come, last partition should we running partition");
      continue;
    }

    // we are assuming partition end time is next partion start time 
    snprintf(partition_num_buf, 16, "%lld", cdr_cmt_cache_entry_list[i+1].partition_num);
    partition_ts = partition_format_date_convert_to_ts(partition_num_buf);

    // Add logic for partition days
    partition_days = (cur_time_stamp - partition_ts) / (ONE_DAY_IN_SEC);

    if (partition_graph_data_remove_f == FALSE && cleanup_policy[GRAPH_DATA].retention_time >= -1)
    {
      if (entry->partition_graph_data_remove_f == TRUE)
      {
        partition_graph_data_remove_f = TRUE;
      }
      else if(cdr_config.data_remove_flag == TRUE && 
               entry->partition_graph_data_remove_f != CONFIG_FALSE && 
               (cleanup_policy[GRAPH_DATA].retention_time != -1 && partition_days > cleanup_policy[GRAPH_DATA].retention_time))
      {
        entry->partition_graph_data_remove_f = TRUE;
        remove_partition(cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, entry->partition_num);
        entry->is_partition_present = CDR_FALSE;
        CDRTL3("Partiton removed for tr: %d, partition = '%lld', size: '%lld' bytes", 
                cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, entry->partition_num, entry->partition_disk_size);
        continue;
      }
      if(entry->partition_graph_data_remove_f != CONFIG_FALSE)
        entry->partition_graph_data_remove_f = ((long long int)(cleanup_policy[GRAPH_DATA].retention_time - partition_days) 
                                                * (ONE_DAY_IN_SEC)) + cur_time_stamp_with_no_hr;
    }
 
    if (partition_csv_remove_f == FALSE && cleanup_policy[CSV].retention_time >= -1)
    {
      if (entry->partition_csv_remove_f == TRUE)
      {
        partition_csv_remove_f = TRUE;
      }
      else if(cdr_config.data_remove_flag == TRUE 
                && entry->partition_csv_remove_f != CONFIG_FALSE && 
                (cleanup_policy[CSV].retention_time != -1 && partition_days > cleanup_policy[CSV].retention_time))
      {
        entry->partition_csv_remove_f = TRUE;
        remove_partition_csv(i);
        CDRTL3("CSV removed for tr: %d, partition = '%lld', size: '%lld' bytes", 
                cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, entry->partition_num, entry->partition_csv_size);
      }
      if(entry->partition_csv_remove_f != CONFIG_FALSE)
        entry->partition_csv_remove_f = ((long long int)(cleanup_policy[CSV].retention_time - partition_days) 
                                                * (ONE_DAY_IN_SEC)) + cur_time_stamp_with_no_hr;
    }
 
    if (partition_raw_file_remove_f == FALSE && cleanup_policy[RAW_DATA].retention_time >= -1)
    {
      if (entry->partition_raw_file_remove_f == TRUE)
      {
        partition_raw_file_remove_f = TRUE;
      }
      else if(cdr_config.data_remove_flag == TRUE && 
               entry->partition_raw_file_remove_f != CONFIG_FALSE && 
               (cleanup_policy[RAW_DATA].retention_time != -1 && partition_days > cleanup_policy[RAW_DATA].retention_time))
      {
        entry->partition_raw_file_remove_f = TRUE;
        remove_partition_raw_data(i);
        CDRTL3("RAW_DATA removed for tr: %d, partition = '%lld', size: '%lld' bytes", 
                cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, entry->partition_num, entry->partition_raw_file_size);
      }
      if(entry->partition_raw_file_remove_f != CONFIG_FALSE)
        entry->partition_raw_file_remove_f = ((long long int)(cleanup_policy[RAW_DATA].retention_time - partition_days) 
                                                * (ONE_DAY_IN_SEC)) + cur_time_stamp_with_no_hr;
    }
 
    if (partition_db_remove_f == FALSE && cleanup_policy[DB].retention_time >= -1)
    {
      if (entry->partition_db_remove_f == TRUE)
      {
        partition_db_remove_f = TRUE;
      }
      else if(cdr_config.data_remove_flag == TRUE && 
               entry->partition_db_remove_f != CONFIG_FALSE && 
              (cleanup_policy[DB].retention_time != -1 && partition_days > cleanup_policy[DB].retention_time))
      {
        entry->partition_db_remove_f = TRUE;
        remove_partition_db(i);
        CDRTL3("DB removed for tr: %d, partition = '%lld', size: '%lld' bytes", 
                cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, entry->partition_num, 
                (entry->partition_db_table_size + entry->partition_db_index_size));
      }
      if(entry->partition_db_remove_f != CONFIG_FALSE)
        entry->partition_db_remove_f = ((long long int)(cleanup_policy[DB].retention_time - partition_days) 
                                                * (ONE_DAY_IN_SEC)) + cur_time_stamp_with_no_hr;
    }
 
    if (partition_har_file_remove_f == FALSE && cleanup_policy[HAR_FILE].retention_time >= -1)
    {
      if (entry->partition_har_file_remove_f == TRUE)
      {
        partition_har_file_remove_f = TRUE;
      }
      else if(cdr_config.data_remove_flag == TRUE && 
               entry->partition_har_file_remove_f != CONFIG_FALSE && 
               (cleanup_policy[HAR_FILE].retention_time != -1 && partition_days > cleanup_policy[HAR_FILE].retention_time))
      {
        entry->partition_har_file_remove_f = TRUE;
        remove_partition_har_file(i);
        CDRTL3("HAR_FILE removed for tr: %d, partition = '%lld', size: '%lld' bytes", 
                cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, entry->partition_num, entry->partition_har_file_size);
      }
      if(entry->partition_har_file_remove_f != CONFIG_FALSE)
        entry->partition_har_file_remove_f = ((long long int)(cleanup_policy[HAR_FILE].retention_time - partition_days) 
                                                * (ONE_DAY_IN_SEC)) + cur_time_stamp_with_no_hr;
    }
 
    if (partition_page_dump_remove_f == FALSE && cleanup_policy[PAGEDUMP].retention_time >= -1)
    {
      if (entry->partition_page_dump_remove_f == TRUE)
      {
        partition_page_dump_remove_f = TRUE;
      }
      else if(cdr_config.data_remove_flag == TRUE && 
               entry->partition_page_dump_remove_f != CONFIG_FALSE && 
               (cleanup_policy[PAGEDUMP].retention_time != -1 && partition_days > cleanup_policy[PAGEDUMP].retention_time))
      {
        entry->partition_page_dump_remove_f = TRUE;
        remove_partition_page_dump(i);
        CDRTL3("PAGEDUMP removed for tr: %d, partition = '%lld', size: '%lld' bytes", 
                cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, entry->partition_num, entry->partition_page_dump_size);
      }
      if(entry->partition_page_dump_remove_f != CONFIG_FALSE)
        entry->partition_page_dump_remove_f = ((long long int)(cleanup_policy[PAGEDUMP].retention_time - partition_days) 
                                                * (ONE_DAY_IN_SEC)) + cur_time_stamp_with_no_hr;
    }
 
    if (partition_logs_remove_f == FALSE && cleanup_policy[LOGS].retention_time >= -1)
    {
      if (entry->partition_logs_remove_f == TRUE)
      {
        partition_logs_remove_f = TRUE;
      }
      else if(cdr_config.data_remove_flag == TRUE && 
               entry->partition_logs_remove_f != CONFIG_FALSE && 
               cleanup_policy[LOGS].retention_time != -1 && partition_days > cleanup_policy[LOGS].retention_time)
      {
        entry->partition_logs_remove_f = TRUE;
        remove_partition_logs(i);
        CDRTL3("LOGS removed for tr: %d, partition = '%lld', size: '%lld' bytes", 
                cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, entry->partition_num, entry->partition_logs_size);
      }
      if(entry->partition_logs_remove_f != CONFIG_FALSE)
        entry->partition_logs_remove_f = ((long long int)(cleanup_policy[LOGS].retention_time - partition_days) 
                                                * (ONE_DAY_IN_SEC)) + cur_time_stamp_with_no_hr;
    }
    
    if (partition_reports_remove_f == FALSE && cleanup_policy[REPORTS].retention_time >= -1)
    {
      if (entry->partition_reports_remove_f == TRUE)
      {
        partition_reports_remove_f = TRUE;
      }
      else if(cdr_config.data_remove_flag == TRUE &&
               entry->partition_reports_remove_f != CONFIG_FALSE &&
               cleanup_policy[REPORTS].retention_time != -1 && partition_days > cleanup_policy[LOGS].retention_time)
      {
        entry->partition_reports_remove_f = TRUE;
        remove_partition_reports(i);
        CDRTL3("REPORTS removed for tr: %d, partition = '%lld', size: '%lld' bytes",
                cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, entry->partition_num, entry->partition_reports_size);
      }
      if(entry->partition_reports_remove_f != CONFIG_FALSE)
        entry->partition_reports_remove_f = ((long long int)(cleanup_policy[REPORTS].retention_time - partition_days)
                                                * (ONE_DAY_IN_SEC)) + cur_time_stamp_with_no_hr;
    }
  }

  char path[512];
  sprintf(path, "%s/logs/TR%d/", ns_wdir,
                       cdr_cache_entry_list[cmt_tr_cache_idx].tr_num);
  remove_similar_file(path, testrunouput_log_filter, cleanup_policy[LOGS].retention_time);

  CDRTL2("Method exit");
}

void cdr_dump_cmt_cache_to_file()
{
  CDRTL2("Method called, total_cache_entry '%d'", total_cache_entry);
  CDRTL3("Dumping cache to file: %s", cmt_cache_file_path);

  FILE *fp;
  fp = fopen(cmt_cache_file_path, "w");
  if(!fp) {
    CDRTL1("Error: cannot open cmt cache file: %s, error: %s", cmt_cache_file_path, strerror(errno));
    return;
  }
  fprintf(fp, "#partition_num| partition_type| partition_disk_size| partition_graph_data_size| partition_csv_size| partition_raw_file_size| partition_db_table_size|  partition_db_index_size| partition_har_file_size| partition_page_dump_size| partition_logs_size| partition_reports_size  partition_graph_data_remove_f| partition_csv_remove_f| partition_raw_file_remove_f| partition_db_remove_f|  partition_har_file_remove_f| partition_page_dump_remove_f| partition_logs_remove_f| partition_reports_remove_f\n");

  for(int i = 0; i < total_cmt_cache_entry; ++i) {
    if (cdr_cmt_cache_entry_list[i].is_partition_present == CDR_TRUE)
    {
      fprintf(fp, "%lld|%d|%lld|%lld|%lld|%lld|%lld|" "%lld|%lld|%lld|%lld|%lld|" "%lld|%lld|%lld|%lld|" "%lld|%lld|%lld|%lld\n",
            cdr_cmt_cache_entry_list[i].partition_num,
            cdr_cmt_cache_entry_list[i].partition_type,
            cdr_cmt_cache_entry_list[i].partition_disk_size,
            cdr_cmt_cache_entry_list[i].partition_graph_data_size, 
            cdr_cmt_cache_entry_list[i].partition_csv_size, 
            cdr_cmt_cache_entry_list[i].partition_raw_file_size, 
            cdr_cmt_cache_entry_list[i].partition_db_table_size, 

            cdr_cmt_cache_entry_list[i].partition_db_index_size, 
            cdr_cmt_cache_entry_list[i].partition_har_file_size, 
            cdr_cmt_cache_entry_list[i].partition_page_dump_size, 
            cdr_cmt_cache_entry_list[i].partition_logs_size, 
            cdr_cmt_cache_entry_list[i].partition_reports_size,            

            cdr_cmt_cache_entry_list[i].partition_graph_data_remove_f, 
            cdr_cmt_cache_entry_list[i].partition_csv_remove_f, 
            cdr_cmt_cache_entry_list[i].partition_raw_file_remove_f, 
            cdr_cmt_cache_entry_list[i].partition_db_remove_f, 

            cdr_cmt_cache_entry_list[i].partition_har_file_remove_f, 
            cdr_cmt_cache_entry_list[i].partition_page_dump_remove_f, 
            cdr_cmt_cache_entry_list[i].partition_logs_remove_f,
            cdr_cmt_cache_entry_list[i].partition_reports_remove_f);

    }
  }

  fclose(fp);
  CDRTL2("Method exit");
}
