#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "cdr_cache.h"
#include "cdr_cleanup.h"
#include "cdr_main.h"
#include "cdr_utils.h"
#include "cdr_config.h"
#include "cdr_log.h"
#include "cdr_dir_operation.h"
#include "cdr_drop_tables.h"
#include "cdr_cmt_handler.h"
#include "nslb_util.h"
#include "nslb_partition.h"
#include "nslb_get_norm_obj_id.h"


struct cleanup_struct *g_cmt_cleanup_policy_ptr = NULL;
struct cleanup_struct *g_other_tr_cleanup_policy_ptr = NULL;

int cur_time_in_ngve_days()
{
  /* Example: tool start @ 2am daily and in negative days 5/22/2020 is given
     this will take cur time 1590093000 (5/22/2020 02:00:00) - check in negative days range
  */
  for(int i = 0; i < cdr_config.ngve_days.total_entry; ++i)
  {
    if(cur_time_stamp >=cdr_config.ngve_days.start_ts[i] && cur_time_stamp <= cdr_config.ngve_days.end_ts[i])
    {
      CDRTL3("Current date is in negative days range '%s' - '%s'.", 
                      cdr_config.ngve_days.start_dates[i],  cdr_config.ngve_days.end_dates[i]);
      return TRUE;
    }
  }

  return FALSE;
}

static int tr_in_ngve_tr(int tr_num)
{
  for(int i = 0; i < cdr_config.ngve_tr.total_entry; ++i)
  {
    if(tr_num == cdr_config.ngve_tr.tr[i])
      return TRUE;
  }

  return FALSE;
}


static inline void check_ngve_cleanup_days(long long int tr_time_stamp, struct ngve_cleanup_days_struct *days, long long int *flag)
{
  CDRTL2("Method called.");
  for(int i = 0; i < days->total_entry; i++)
  {
    if(tr_time_stamp >= days->start_ts[i] && tr_time_stamp <= days->end_ts[i]) // tr_time_stamp lies between start and end ts
      *flag = CONFIG_FALSE;
  }
  CDRTL2("Method exit.");
}


static void tr_in_ngve_cleanup_days(struct cdr_cache_entry *entry)
{
  CDRTL2("Method called.");
  struct ngve_cleanup_days_struct *days;

  // select policy according to tr_type
  if(entry->tr_type == ARCHIVED_TR)
    days = &(cdr_config.ngve_cleanup_days[ARCH_TR]);
  else if(entry->tr_type == DEBUG_TR)
    days = &(cdr_config.ngve_cleanup_days[DBG_TR]);
  else if(entry->tr_type == PERFORMANCE_TR)
    days = &(cdr_config.ngve_cleanup_days[PERF_TR]);
  else if(entry->tr_type == GENERATOR_TR)
    days = &(cdr_config.ngve_cleanup_days[GEN_TR]);
  else
    return;


  check_ngve_cleanup_days(entry->end_time_stamp, days, &(entry->remove_tr_f));

  days = &(cdr_config.ngve_cleanup_days[GRAPH_DATA]);
  check_ngve_cleanup_days(entry->end_time_stamp, days, &(entry->graph_data_remove_f));

  days = &(cdr_config.ngve_cleanup_days[CSV]);
  check_ngve_cleanup_days(entry->end_time_stamp, days, &(entry->csv_remove_f));

  days = &(cdr_config.ngve_cleanup_days[RAW_DATA]);
  check_ngve_cleanup_days(entry->end_time_stamp, days, &(entry->raw_file_remove_f));

  days = &(cdr_config.ngve_cleanup_days[DB]);
  check_ngve_cleanup_days(entry->end_time_stamp, days, &(entry->tr_db_remove_f));

  days = &(cdr_config.ngve_cleanup_days[HAR_FILE]);
  check_ngve_cleanup_days(entry->end_time_stamp, days, &(entry->har_file_remove_f));

  days = &(cdr_config.ngve_cleanup_days[PAGEDUMP]);
  check_ngve_cleanup_days(entry->end_time_stamp, days, &(entry->page_dump_remove_f));

  days = &(cdr_config.ngve_cleanup_days[LOGS]);
  check_ngve_cleanup_days(entry->end_time_stamp, days, &(entry->logs_remove_f));

  days = &(cdr_config.ngve_cleanup_days[TEST_DATA]);
  check_ngve_cleanup_days(entry->end_time_stamp, days, &(entry->test_data_remove_f));

  days = &(cdr_config.ngve_cleanup_days[REPORTS]);
  check_ngve_cleanup_days(entry->end_time_stamp, days, &(entry->reports_remove_f));

  CDRTL2("Method exit.");
}

int tr_in_remove_range(struct cdr_cache_entry *entry)
{
  CDRTL2("Method called, TR '%d' type '%d'", entry->tr_num, entry->tr_type);
  struct cleanup_struct *cleanup_ptr = g_other_tr_cleanup_policy_ptr;
  int retention_time;
  int tr_days;
  if (entry->remove_tr_f == CONFIG_FALSE)
    return FALSE;

  if(entry->tr_type == ARCHIVED_TR)
    retention_time = cleanup_ptr[ARCH_TR].retention_time;
  else if(entry->tr_type == DEBUG_TR)
    retention_time = cleanup_ptr[DBG_TR].retention_time;
  else if(entry->tr_type == PERFORMANCE_TR)
    retention_time = cleanup_ptr[PERF_TR].retention_time;
  else if(entry->tr_type == GENERATOR_TR)
    retention_time = cleanup_ptr[GEN_TR].retention_time;
  else
  {
    
    CDRTL1("Error : Tr type is no set for tr num '%d' type '%d'.", entry->tr_num, entry->tr_type);
    return FALSE;
  }
  
  if (retention_time == -1)
   return FALSE;

  // calculate days for tr
  tr_days = (cur_time_stamp - entry->end_time_stamp) / (ONE_DAY_IN_SEC);

  CDRTL3("tr_days '%d', retention_time = '%d', cur_time_stamp '%lld', end_time_stamp '%lld'", 
                             tr_days, retention_time, cur_time_stamp, entry->end_time_stamp);
  if(tr_days >= retention_time)
  {
    return TRUE;
  }
  entry->remove_tr_f = ((long long int)(retention_time - tr_days) * (ONE_DAY_IN_SEC)) + cur_time_stamp_with_no_hr;

  return FALSE;
}

void remove_tr_ex(int tr_num)
{
  CDRTL2("Method Called, tr_num '%d'.", tr_num);
  
  char cmd_buf[CDR_FILE_PATH_SIZE];
  char out_buf[CDR_FILE_PATH_SIZE] = "\0";

  long long int total_time = get_ts_in_ms();
 
  snprintf(cmd_buf, CDR_FILE_PATH_SIZE, "%s/bin/nsu_rm_trun -f -n %d -R 'Data Retention'", ns_wdir, tr_num);
  nslb_run_cmd_and_get_last_line(cmd_buf, CDR_FILE_PATH_SIZE, out_buf);
  
  total_time = get_ts_in_ms() - total_time;
  
  CDRAL(0, tr_num, 0, "TR", 0, "Delete", "Delete due to retention time as bad TR.", total_time, "Retention Manager");
  CDRTL2("Method exit, cmd_buf '%s', out_buf '%s'.", cmd_buf, out_buf);
}

void remove_tr(struct cdr_cache_entry *entry)
{
  CDRTL2("Method Called, tr_num '%d'.", entry->tr_num);
  
  char cmd_buf[CDR_FILE_PATH_SIZE];
  char out_buf[CDR_FILE_PATH_SIZE] = "\0";

  long long int total_time = get_ts_in_ms();
 
  snprintf(cmd_buf, CDR_FILE_PATH_SIZE, "%s/bin/nsu_rm_trun -f -n %d -R 'Data Retention'", ns_wdir, entry->tr_num);
  nslb_run_cmd_and_get_last_line(cmd_buf, CDR_FILE_PATH_SIZE, out_buf);
  
  total_time = get_ts_in_ms() - total_time;
  
  CDRAL(0, entry->tr_num, 0, "TR", entry->tr_disk_size, "Delete", "Delete due to retention time", total_time, "Retention Manager");

  CDRTL2("Method exit, cmd_buf '%s', out_buf '%s'.", cmd_buf, out_buf);
}

void remove_raw_data(struct cdr_cache_entry *entry)
{
  CDRTL2("Method called");
  long long int total_time = get_ts_in_ms();

  char path[CDR_FILE_PATH_SIZE] = "\0";
  int i;
  for(i=0 ; i < entry->count; i++)
  {
    sprintf(path, "%s/logs/TR%d/%s/nd/raw_data/", ns_wdir,
                                entry->tr_num, entry->partition_list[i]->d_name);
    remove_dir_file_ex(path);
   
    sprintf(path, "%s/logs/TR%d/%s/reports/raw_data/", ns_wdir,
                                entry->tr_num, entry->partition_list[i]->d_name);
    remove_dir_file_ex(path);
  }

  total_time = get_ts_in_ms() - total_time;
  CDRAL(0, entry->tr_num, 0, "raw_data", entry->raw_file_size, "Delete", "Delete due to retention time", total_time, "Retention Manager");

  CDRTL2("Method exit");
}

void remove_har_file(struct cdr_cache_entry *entry)
{
  CDRTL2("Method called");
  long long int total_time = get_ts_in_ms();

  char path[CDR_FILE_PATH_SIZE] = "\0";
  int i;
  for(i=0 ; i < entry->count; i++)
  {
    sprintf(path, "%s/logs/TR%d/%s/rbu_logs/harp_files/", ns_wdir,
                                entry->tr_num, entry->partition_list[i]->d_name);
    remove_dir_file_ex(path);  
   
    sprintf(path, "%s/logs/TR%d/%s/rbu_logs/snap_shots/", ns_wdir,
                                entry->tr_num, entry->partition_list[i]->d_name);
    remove_dir_file_ex(path);

    sprintf(path, "%s/logs/TR%d/%s/rbu_logs/screen_shot/", ns_wdir,
                                entry->tr_num, entry->partition_list[i]->d_name);
    remove_dir_file_ex(path);
 }

  total_time = get_ts_in_ms() - total_time;
  CDRAL(0, entry->tr_num, 0, "har_file", entry->har_file_size, "Delete", "Delete due to retention time", total_time, "Retention Manager");

  CDRTL2("Method exit");
}

static void remove_db_file(struct cdr_cache_entry *entry)
{
  CDRTL2("Method called");
  char path[CDR_FILE_PATH_SIZE] = "\0";
  int i;
  for(i=0 ; i < entry->count; i++)
  {
    sprintf(path, "%s/logs/TR%d/%s/nd/sqb", ns_wdir, entry->tr_num, entry->partition_list[i]->d_name);
    remove_dir_file_ex(path);

    sprintf(path, "%s/logs/TR%d/%s/nd/dat", ns_wdir, entry->tr_num, entry->partition_list[i]->d_name);
    remove_dir_file_ex(path);

    sprintf(path, "%s/logs/TR%d/%s/nd/hs", ns_wdir, entry->tr_num, entry->partition_list[i]->d_name);
    remove_dir_file_ex(path);
  } 

  CDRTL2("Method exit");
}


void remove_pagedump(struct cdr_cache_entry *entry)
{
  CDRTL2("Method called");
  long long int total_time = get_ts_in_ms();

  char path[CDR_FILE_PATH_SIZE] = "\0";
  int i;
  for(i=0 ; i < entry->count; i++)
  {
    sprintf(path, "%s/logs/TR%d/%s/page_dump/", ns_wdir,
                                entry->tr_num, entry->partition_list[i]->d_name);
    remove_dir_file_ex(path);
  }

  total_time = get_ts_in_ms() - total_time;
  CDRAL(0, entry->tr_num, 0, "pagedump", entry->page_dump_size, "Delete", "Delete due to retention time", total_time, "Retention Manager");

  CDRTL2("Method exit");
}

void remove_test_data(struct cdr_cache_entry *entry)
{
  CDRTL2("Method called");
  long long int total_time = get_ts_in_ms();

  char path[CDR_FILE_PATH_SIZE] = "\0";
  sprintf(path, "%s/logs/TR%d/scripts", ns_wdir, entry->tr_num);
  remove_dir_file_ex(path);

  for(int i=0 ; i < entry->count; i++)
  {
    sprintf(path, "%s/logs/TR%d/%s/server_logs/", ns_wdir,
                                entry->tr_num, entry->partition_list[i]->d_name);
    remove_dir_file_ex(path);
  }

  total_time = get_ts_in_ms() - total_time;
  CDRAL(0, entry->tr_num, 0, "test_data", entry->test_data_size, "Delete", "Delete due to retention time", total_time, "Retention Manager");

  CDRTL2("Method exit");
}

void remove_csv(struct cdr_cache_entry *entry)
{
  CDRTL2("Method called");

  char path[CDR_FILE_PATH_SIZE] = "\0";
  int i;
  long long int total_time = get_ts_in_ms();
  
  for(i=0 ; i < entry->count; i++)
  {
    sprintf(path, "%s/logs/TR%d/%s/reports/csv/", ns_wdir,
                                entry->tr_num, entry->partition_list[i]->d_name);
    remove_dir_file_ex(path);

    sprintf(path, "%s/logs/TR%d/%s/nd/csv/", ns_wdir,
                                entry->tr_num, entry->partition_list[i]->d_name);
    remove_dir_file_ex(path);
  }

  sprintf(path, "%s/logs/TR%d/common_files/reports/csv/", ns_wdir, entry->tr_num);
  remove_dir_file_ex(path);

  sprintf(path, "%s/logs/TR%d/common_files/nd/csv/", ns_wdir, entry->tr_num);
  remove_dir_file_ex(path);
  
  total_time = get_ts_in_ms() - total_time;
  CDRAL(0, entry->tr_num, 0, "CSV", entry->csv_size, "Delete", "Delete due to retention time", total_time, "Retention Manager");

  CDRTL2("Method exit");
}

void remove_logs(struct cdr_cache_entry *entry)
{
  CDRTL2("Method called");

  char path[CDR_FILE_PATH_SIZE] = "\0";
  int i;
  long long int total_time = get_ts_in_ms();

  sprintf(path, "%s/logs/TR%d/error.log", ns_wdir, entry->tr_num);
  remove_dir_file_ex(path);

  sprintf(path, "%s/logs/TR%d/event.log", ns_wdir, entry->tr_num);
  remove_dir_file_ex(path);

  sprintf(path, "%s/logs/TR%d/ready_reports/testinit_status/testInit.log", ns_wdir, entry->tr_num);
  remove_dir_file_ex(path);

  sprintf(path, "%s/logs/TR%d/debug.log", ns_wdir, entry->tr_num);
  remove_dir_file_ex(path);

  sprintf(path, "%s/logs/TR%d/TestRunOutput.log", ns_wdir, entry->tr_num);
  remove_dir_file_ex(path);

  for(i=0 ; i < entry->count; i++)
  {
    sprintf(path, "%s/logs/TR%d/%s/ns_logs/", ns_wdir, entry->tr_num, entry->partition_list[i]->d_name);
    remove_dir_file_ex(path);

    sprintf(path, "%s/logs/TR%d/%s/nd/logs/", ns_wdir, entry->tr_num, entry->partition_list[i]->d_name);
    remove_dir_file_ex(path);

    sprintf(path, "%s/logs/TR%d/%s/monitor.log", ns_wdir, entry->tr_num, entry->partition_list[i]->d_name);
    remove_dir_file_ex(path);

    sprintf(path, "%s/logs/TR%d/%s/error.log", ns_wdir, entry->tr_num, entry->partition_list[i]->d_name);
    remove_dir_file_ex(path);

    sprintf(path, "%s/logs/TR%d/%s/event.log", ns_wdir, entry->tr_num, entry->partition_list[i]->d_name);
    remove_dir_file_ex(path);

    sprintf(path, "%s/logs/TR%d/%s/debug.log", ns_wdir, entry->tr_num, entry->partition_list[i]->d_name);
    remove_dir_file_ex(path);

    sprintf(path, "%s/logs/TR%d/%s/rbu_logs/", ns_wdir, entry->tr_num, entry->partition_list[i]->d_name);
    remove_dir_file_ex(path);
  }

  total_time = get_ts_in_ms() - total_time;
  CDRAL(0, entry->tr_num, 0, "logs", entry->logs_size, "Delete", "Delete due to retention time", total_time, "Retention Manager");
  CDRTL2("Method exit");
}

void remove_reports(struct cdr_cache_entry *entry)
{
  CDRTL2("Method called");
  long long int total_time = get_ts_in_ms();

  char path[CDR_FILE_PATH_SIZE] = "\0";
  int i;
  for(i=0 ; i < entry->count; i++)
  {
    sprintf(path, "%s/logs/TR%d/%s/rbu_logs/performance_trace/", ns_wdir,
                                entry->tr_num, entry->partition_list[i]->d_name);
    remove_dir_file_ex(path);

    sprintf(path, "%s/logs/TR%d/%s/rbu_logs/lighthouse/", ns_wdir,
                                entry->tr_num, entry->partition_list[i]->d_name);
    remove_dir_file_ex(path);
 }

  total_time = get_ts_in_ms() - total_time;
  CDRAL(0, entry->tr_num, 0, "reports", entry->reports_size, "Delete", "Delete due to retention time", total_time, "Retention Manager");

  CDRTL2("Method exit");
}

void remove_db(struct cdr_cache_entry *entry)
{
  long long int total_time = get_ts_in_ms();
  make_query_and_drop_tables(entry->tr_num, 0); 
  remove_db_file(entry);
  
  total_time = get_ts_in_ms() - total_time;
  CDRAL(0, entry->tr_num, 0, "DB", entry->tr_db_table_size + entry->tr_db_index_size, "Delete", "Delete due to retention time", total_time, "Retention Manager");
}


static int check_cleanup_range(int tr_days, int retention_time, long long int *flag)
{
  CDRTL2("Method called");
  if (*flag != CONFIG_FALSE  && *flag != TRUE && retention_time >= 0)
  {
    if(cdr_config.data_remove_flag == TRUE && tr_days > retention_time)
    {
      *flag = TRUE;
      return TRUE;
    }
    *flag= ((long long int)(retention_time - tr_days) * (ONE_DAY_IN_SEC)) + cur_time_stamp_with_no_hr;
  }
  return FALSE;
  CDRTL2("Method exit");
}

void component_in_remove_range(struct cdr_cache_entry *entry)
{
  CDRTL2("Method called");
  if (entry->remove_tr_f == CONFIG_FALSE)
    return;

  struct cleanup_struct *cleanup_policy = g_other_tr_cleanup_policy_ptr;

  int tr_days = (cur_time_stamp - entry->end_time_stamp) / (ONE_DAY_IN_SEC);


  if (check_cleanup_range(tr_days, cleanup_policy[GRAPH_DATA].retention_time, &(entry->graph_data_remove_f)) == TRUE)
  {
    remove_tr(entry);
    entry->is_tr_present = CDR_FALSE;
    return;
  }

  if (check_cleanup_range(tr_days, cleanup_policy[CSV].retention_time, &(entry->csv_remove_f)) == TRUE)
  {
    remove_csv(entry);
    CDRTL3("CSV removed for tr: %d, size: %d bytes", entry->tr_num, entry->csv_size);
  }

  if (check_cleanup_range(tr_days, cleanup_policy[RAW_DATA].retention_time, &(entry->raw_file_remove_f)) == TRUE)
  {
    remove_raw_data(entry);
    CDRTL3("RAW_DATA removed for tr: %d, size: %d bytes", entry->tr_num, entry->raw_file_size);
  }

  if (check_cleanup_range(tr_days, cleanup_policy[DB].retention_time, &(entry->tr_db_remove_f)) == TRUE)
  {
    remove_db(entry);
    CDRTL3("DB removed for tr: %d, bytes", entry->tr_num );
  }

  if (check_cleanup_range(tr_days, cleanup_policy[HAR_FILE].retention_time, &(entry->har_file_remove_f)) == TRUE)
  {
    remove_har_file(entry);
    CDRTL3("RAW_DATA removed for tr: %d, size: %d bytes", entry->tr_num, entry->raw_file_size);
  }
  
  if (check_cleanup_range(tr_days, cleanup_policy[PAGEDUMP].retention_time, &(entry->page_dump_remove_f)) == TRUE)
  {
    remove_pagedump(entry);
    CDRTL3("RAW_DATA removed for tr: %d, size: %d bytes", entry->tr_num, entry->raw_file_size);
  }

  if (check_cleanup_range(tr_days, cleanup_policy[LOGS].retention_time, &(entry->logs_remove_f)) == TRUE)
  {
    remove_logs(entry);
    CDRTL3("LOGS removed for tr: %d, size: %d bytes", entry->tr_num, entry->logs_size);
  }

  if (check_cleanup_range(tr_days, cleanup_policy[TEST_DATA].retention_time, &(entry->test_data_remove_f)) == TRUE)
  {
    remove_test_data(entry);
    CDRTL3("RAW_DATA removed for tr: %d, size: %d bytes", entry->tr_num, entry->raw_file_size);
  }

  if (check_cleanup_range(tr_days, cleanup_policy[REPORTS].retention_time, &(entry->reports_remove_f)) == TRUE)
  {
    remove_reports(entry);
    CDRTL3("REPORTS removed for tr: %d, size: %d bytes", entry->tr_num, entry->reports_size);
  }

  CDRTL2("Method exit");
}

void set_cleanup_policy()
{
  CDRTL2("Methid Called");

  if(cmt_tr_num != 0 && cdr_config.tr_num != cmt_tr_num) 
  {
    CDRTL3("Config tr '%d' number is not same as config.ini tr_num: '%d', Taking config.ini '%d' TR as CMT", 
       cdr_config.tr_num, cmt_tr_num, cmt_tr_num);
    cdr_config.tr_num = cmt_tr_num;
  }
  else 
  {
    cmt_tr_num = cdr_config.tr_num; 
  }

  if(cdr_config.tr_num != 0) //TEST_RUN is not present with test number 
  {

    g_cmt_cleanup_policy_ptr = cdr_config.cleanup;
    if(cdr_config.cleanup_all_tr_flag == CDR_FALSE) // cleanup_all_tr not present
    {
      g_other_tr_cleanup_policy_ptr = cdr_config.cleanup;
      //CDRTL1("Cleanup policy is set only for CMT TR.");
    }
    else{
      g_other_tr_cleanup_policy_ptr = cdr_config.cleanup_all_tr; // cleanup_all_tr present
      //CDRTL1("Cleanup policy is diffrent for CMT TR and other TRs.");
    }
  }
  else  // TR not present
  {
    g_cmt_cleanup_policy_ptr = cdr_config.cleanup;
    if(cdr_config.cleanup_all_tr_flag == CDR_FALSE) // tr not present and cleanup_all_tr not present
    {
      g_other_tr_cleanup_policy_ptr = cdr_config.cleanup;
      //CDRTL1("Cleanup policy is same for CMT TR and other TRs.");
    }
    else // tr not present and cleanup_all_tr present
    {
      g_other_tr_cleanup_policy_ptr = cdr_config.cleanup_all_tr;
      //CDRTL1("Cleanup policy is diffrent for CMT TR and other TRs.");
    }
  }
  CDRTL2("Methid Exit");
}

void check_ngve_range(struct cdr_cache_entry *entry)
{
  CDRTL2("Method called.");
  // check if tr is in negative tr
  if(tr_in_ngve_tr(entry->tr_num) == TRUE)
  {
    entry->remove_tr_f = CONFIG_FALSE;
    CDRTL3("tr_num: [%d] is in negative tr list", entry->tr_num);
    return;
  }

  tr_in_ngve_cleanup_days(entry);
  CDRTL2("Method exit.");
}


int cleanup_process()
{
  struct cdr_cache_entry *entry;

  CDRTL2("Method called.");

  if (cdr_config.cleanup_flag == CDR_FALSE && cdr_config.cleanup_all_tr_flag == CDR_FALSE)
  {
    CDRTL3("Clean policy is not define. Exiting");
    return CDR_SUCCESS;
  }

  // Check cur time is in negative days
  long int lol_time = cur_time_stamp;
  if(cur_time_in_ngve_days() == TRUE)
  {
    // Only house keeing work - get the new TRs and size of the comonents
    CDRTL3("Current date: [%s] is in negative days.", ctime(&(lol_time)));
    cdr_config.data_remove_flag = FALSE;
  }

  if(cdr_config.data_remove_flag == TRUE)
    set_cleanup_policy();

  for(int i = 0; i < total_cache_entry; ++i)
  {
    entry = &(cdr_cache_entry_list[i]);

    if(entry->is_tr_present == CDR_FALSE)
    {
      // TR is not present in disk
      CDRTL3("TR: '%d' was present earlier with size: '%lld', now not present in disk, So ignoring.", entry->tr_num, entry->tr_disk_size);
      // TODO: audit log
      continue;
    }

    CDRTL4("TR: %d, tr_type: '%d', g_other_tr_cleanup_policy_ptr: '%p'", entry->tr_num, entry->tr_type, g_other_tr_cleanup_policy_ptr);

    if(entry->tr_type != CMT_TR && g_other_tr_cleanup_policy_ptr != NULL)
    {

      if(entry->tr_type == RUNNING_TR || entry->tr_type == OLD_CMT_TR || entry->tr_type == LOCKED_TR)
      {
        CDRTL3("TR: [%d] is running/old_cmt/locked. tr_type: %d. Ignoring...", entry->tr_num, entry->tr_type);
        continue;
      }

      if(tr_in_remove_range(entry) == CDR_TRUE)
      {
        remove_tr(entry);
        entry->is_tr_present = CDR_FALSE;
        continue;
      }

      // Remove components that are in range of retention time
      if(entry->partition_list == NULL)
        entry->partition_list = get_tr_partiton_list(entry->tr_num, &(entry->count), NULL);
      component_in_remove_range(entry);

    }
    else if(entry->tr_type == CMT_TR)
    {
      if(entry->partition_list == NULL) {
        entry->partition_list = get_tr_partiton_list(entry->tr_num, &(entry->count), NULL);  

        struct dirent **partition_list = entry->partition_list;
        char *partition_ptr = NULL;
        long long partition_num = 0;
        int norm_id; 
        
        for(int n = 0; n < entry->count; n++)
        {
          partition_ptr = partition_list[n]->d_name; 
          norm_id = -2;
          
          if(rebuild_cache != CDR_TRUE) // no need to get norm id  
            norm_id = nslb_get_norm_id(&cmt_cache_entry_norm_table, partition_ptr, strlen(partition_ptr));
        
          if(norm_id < 0) {
            if(norm_id == -2) {
              // -2 means tr is not present
              partition_num = atoll(partition_ptr);
        
              if((norm_id = cmt_cache_entry_add_from_partition(partition_num, partition_ptr, norm_id, n)) == CDR_ERROR) // add to cache_entry_list
                continue;
              
              CDRTL3("partition: [%lld] not present in cache, added to cdr_cmt__cache_entry_list", partition_num);
            }
            else {
              CDRTL1("Error: nslb_get_norm_id");
              return CDR_ERROR;
            }
          }
          else if (cdr_cmt_cache_entry_list[norm_id].partition_type == RUNNING_PARTITION)
          {
            if((norm_id = cmt_cache_entry_add_from_partition(partition_num, partition_ptr, norm_id, n)) == CDR_ERROR) // add to cache_entry_list
              continue;
          }
        
          cdr_cmt_cache_entry_list[norm_id].is_partition_present = CDR_TRUE;
        }
      }                                                                                      
    }                                                                                        
  }                                                                                          
                                                                                             
  CDRTL2("Method exit");                                                                     
  return CDR_SUCCESS;                                                                        
}                                                                                            

void remove_recyclebin_testrun(char *path, int retention_time) 
{
  DIR *dir;
  struct stat st;
  struct dirent *de;
  int days;
  char file_path[1024];
  long long int total_time;

  dir = opendir(path);

  if(dir == NULL) {
    CDRTL1("Error: cannot open dir: '%s', error: %s\n", path, strerror(errno));
    return;
  }

  while ((de = readdir(dir)) != NULL ) 
  {
    sprintf (file_path, "%s/%s", path, de->d_name);
    total_time = get_ts_in_ms();

    if (stat (file_path, &st))
    {
      CDRTL1("Error: cannot find file_path : '%s', error: %s\n", file_path, strerror(errno));
    }
    
    CDRTL4("file_path '%s'- %lld - %lld / %lld", file_path, cur_time_stamp, st.st_mtime, ONE_DAY_IN_SEC);

    days = (cur_time_stamp - st.st_mtime) / (ONE_DAY_IN_SEC);

    if(days < retention_time)
      continue;

    char cmd_buf[CDR_FILE_PATH_SIZE] = "\0";
    char out_buf[CDR_FILE_PATH_SIZE] = "\0";

    snprintf(cmd_buf, CDR_FILE_PATH_SIZE, "%s/bin/nsu_rm_trun -f -n %d -k 1", ns_wdir, atoi(de->d_name+2)); 

    nslb_run_cmd_and_get_last_line(cmd_buf, CDR_FILE_PATH_SIZE, out_buf);

    total_time = get_ts_in_ms() - total_time;
  
    CDRAL(0, atoi(de->d_name+2), 0, "TR", 0, "Delete", "Delete due to Recyclebin retention time", total_time, "Retention Manager");
  
    CDRTL2("Method exit, cmd_buf '%s', out_buf '%s'.", cmd_buf, out_buf);
  }
}
                                                                                             
void cdr_recyclebin_cleanup() 
{
  CDRTL2("Method Called, cdr_config.recyclebin_time '%d'.", cdr_config.total_custom_cleanup);
  char path[CDR_FILE_PATH_SIZE] = "\0";
  sprintf (path, "%s/.recyclebin", ns_wdir);
  remove_recyclebin_testrun(path, cdr_config.recyclebin_cleanup);
                                                 
  CDRTL2("Method Exit");
}
                                                                                             
void cdr_handle_custom_cleanup()                                                             
{                                                                                            
  CDRTL2("Method Called, cdr_config.total_custom_cleanup '%d'.", cdr_config.total_custom_cleanup);
                                                                                             
  int i;                                                                                     
  for(i = 0; i< cdr_config.total_custom_cleanup; i++)                                        
  {                                                                                          
    CDRTL3("Custom Path '%s'.", cdr_config.custom_cleanup[i].path);                           
    remove_dir_file_with_retention_time_ex(cdr_config.custom_cleanup[i].path, cdr_config.custom_cleanup[i].retention_time, 0);
                                                                                             
  }                                                                                          
  CDRTL2("Method exit");                                                                     
}                                                                                            
                                                                                             
void cdr_handle_other_cleanup()                                                              
{                                                                                            
  CDRTL2("Method Called, cdr_config.total_custom_cleanup '%d'.", cdr_config.total_custom_cleanup);
                                                                                             
  if (!g_other_tr_cleanup_policy_ptr)                                                        
    return;                                                                                  
                                                                                             
  char path[CDR_FILE_PATH_SIZE] = "\0";                                                      
  if(g_other_tr_cleanup_policy_ptr[LOGS].retention_time == -1)
    return;
                                                                                             
  sprintf(path, "%s/webapps/netstorm/logs/", ns_wdir);                                       
  remove_dir_file_with_retention_time_ex(path, g_other_tr_cleanup_policy_ptr[LOGS].retention_time, 0);
  
  sprintf(path, "%s/apps/apache-tomcat-7.0.91/logs/", ns_wdir);
  remove_dir_file_with_retention_time_ex(path, g_other_tr_cleanup_policy_ptr[LOGS].retention_time, 0);

  sprintf(path, "%s/apps/apache-tomcat-7.0.104/logs/", ns_wdir);
  remove_dir_file_with_retention_time_ex(path, g_other_tr_cleanup_policy_ptr[LOGS].retention_time, 0);
  
  sprintf(path, "%s/apps/apache-tomcat-7.0.99/logs/", ns_wdir);
  remove_dir_file_with_retention_time_ex(path, g_other_tr_cleanup_policy_ptr[LOGS].retention_time, 0);

  sprintf(path, "%s/apps/apache-tomcat-7.0.105/logs/", ns_wdir);
  remove_dir_file_with_retention_time_ex(path, g_other_tr_cleanup_policy_ptr[LOGS].retention_time, 0);

  /* Removing for bug # 99509
  sprintf(path, "%s/webapps/logs/", ns_wdir);
  remove_dir_file_with_retention_time_ex(path, g_other_tr_cleanup_policy_ptr[LOGS].retention_time); */
  
  sprintf(path, "/home/cavisson/monitors/logs/");
  remove_dir_file_with_retention_time_ex(path, g_other_tr_cleanup_policy_ptr[LOGS].retention_time, 0);
  
  sprintf(path, "%s/.rel/", ns_wdir);
  remove_dir_file_with_retention_time_ex(path, g_other_tr_cleanup_policy_ptr[LOGS].retention_time, 0);
  
  sprintf(path, "/home/cavisson/core_files/");
  remove_dir_file_with_retention_time_ex(path, g_other_tr_cleanup_policy_ptr[LOGS].retention_time, 1);

  sprintf(path, "/tmp");
  remove_dir_file_with_retention_time_ex(path, g_other_tr_cleanup_policy_ptr[LOGS].retention_time, 0);
 
  sprintf(path, "%s/ndc/logs/", ns_wdir);
  remove_dir_file_with_retention_time_ex(path, g_other_tr_cleanup_policy_ptr[LOGS].retention_time, 0);
 
  sprintf(path, "%s/lps/logs/", ns_wdir);
  remove_dir_file_with_retention_time_ex(path, g_other_tr_cleanup_policy_ptr[LOGS].retention_time, 0);

  sprintf(path, "%s/.productLogs/", ns_wdir);
  remove_dir_file_with_retention_time_ex(path, g_other_tr_cleanup_policy_ptr[LOGS].retention_time, 0);

  sprintf(path, "%s/.productLogs/.scriptConverterLogs/", ns_wdir);
  remove_dir_file_with_retention_time_ex(path, g_other_tr_cleanup_policy_ptr[LOGS].retention_time, 0);

  sprintf(path, "%s/.productLogs/.auditLogs/", ns_wdir);
  remove_dir_file_with_retention_time_ex(path, g_other_tr_cleanup_policy_ptr[LOGS].retention_time, 0);

  sprintf(path, "%s/.productLogs/.restservices/", ns_wdir);
  remove_dir_file_with_retention_time_ex(path, g_other_tr_cleanup_policy_ptr[LOGS].retention_time, 0);

  sprintf(path, "%s/.productLogs/.configUILogs/", ns_wdir);
  remove_dir_file_with_retention_time_ex(path, g_other_tr_cleanup_policy_ptr[LOGS].retention_time, 0);

  sprintf(path, "%s/.productLogs/.tsdb/", ns_wdir);
  remove_dir_file_with_retention_time_ex(path, g_other_tr_cleanup_policy_ptr[LOGS].retention_time, 0);

  sprintf(path, "%s/.productLogs/.alertLogs/", ns_wdir);
  remove_dir_file_with_retention_time_ex(path, g_other_tr_cleanup_policy_ptr[LOGS].retention_time, 0);

  sprintf(path, "%s/.productLogs/.kpiLogs/", ns_wdir);
  remove_dir_file_with_retention_time_ex(path, g_other_tr_cleanup_policy_ptr[LOGS].retention_time, 0);

  sprintf(path, "%s/.productLogs/.ddrLogs/", ns_wdir);
  remove_dir_file_with_retention_time_ex(path, g_other_tr_cleanup_policy_ptr[LOGS].retention_time, 0);

  sprintf(path, "%s/netHavoc/logs/", ns_wdir);
  remove_dir_file_with_retention_time_ex(path, g_other_tr_cleanup_policy_ptr[LOGS].retention_time, 0);
  
  sprintf(path, "%s/webapps/netstorm/logs/nethavoc.log", ns_wdir);
  remove_dir_file_with_retention_time_ex(path, g_other_tr_cleanup_policy_ptr[LOGS].retention_time, 0);

  CDRTL2("Method exit");
}

void cdr_remove_sm_data()
{
  struct dirent **file_list;
  int n, i;
  char path[CDR_FILE_PATH_SIZE] = "\0";
  char file_path[1024];
  char *ptr;
  int retention_time = g_other_tr_cleanup_policy_ptr[CONFIGS].retention_time;

  if(retention_time == -1)
    return;

  sprintf(path, "%s/smconfig/default/default/", ns_wdir);
  remove_dir_file_with_retention_time_ex(path, g_other_tr_cleanup_policy_ptr[LOGS].retention_time, 0); 

  sprintf(path, "%s/", ns_wdir);
  logsPath = path;
  n = scandir(path, &file_list, json_filter, NULL);

  for (i = 0; i < n; i++){
    sprintf(file_path, "%s/%s", path, file_list[i]->d_name);
    struct stat st;
    if( (stat(file_path, &st) != 0) ) {
      CDRTL4("Error: stat error: '%s', file: '%s'\n", strerror(errno), file_path);
      return;
    }
    int days = (cur_time_stamp - st.st_mtime) / (ONE_DAY_IN_SEC);
    if(days < retention_time)
      continue;
    ptr = strchr(file_list[i]->d_name, '.');
    if (ptr)
    {
      remove_file(file_path);
      *ptr = '\0';
      sprintf(file_path, "%s/scripts/default/default/%s", path, file_list[i]->d_name);
      remove_dir_file_ex(file_path);
      sprintf(file_path, "%s/scenarios/default/default/%s.conf", path, file_list[i]->d_name);
      remove_file(file_path);
    }
  }
}

