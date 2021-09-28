#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <libpq-fe.h>

#include "cdr_components.h"
#include "cdr_dir_operation.h"
#include "cdr_log.h"
#include "cdr_main.h"
#include "cdr_cache.h"
#include "cdr_nv_handler.h"
#include "cdr_config.h"
#include "cdr_cleanup.h"
#include "cdr_utils.h"
#include "cdr_cmt_handler.h"
#include "nslb_util.h"
#include "nslb_partition.h"
#include "cdr_drop_tables.h"

struct dirent **nv_partition_list = NULL;
int nv_partition_count = 0;


long long int get_nv_partition_disk_size(int norm_id)
{
  CDRTL2("Method called. NV Client ID '%s' cache_line: '%lld'", 
                        nv_client_id, cdr_nv_cache_entry_list[norm_id].partition_num);
  char path[CDR_FILE_PATH_SIZE] = "\0";

  sprintf(path, "%s/%lld/", hpd_root, 
                         cdr_nv_cache_entry_list[norm_id].partition_num);
  long long int total_size = get_dir_size_ex(path);
  
  CDRTL2("Method Exit, size '%lld'", total_size);
  return total_size;
}


long long int get_nv_partition_csv_size(int norm_id)
{
  CDRTL2("Method called. NV Client ID '%s' cache_line: '%lld'", 
                        nv_client_id, cdr_nv_cache_entry_list[norm_id].partition_num);

  char path[CDR_FILE_PATH_SIZE] = "\0";
  long long int total_size = 0; 
  int i;

  for(i = 1; i <= cdr_nv_cache_entry_list[norm_id].num_proc; i++)
  {
    sprintf(path, "%s/rum/%lld/%03d/db/", hpd_root,
                         cdr_nv_cache_entry_list[norm_id].partition_num, i);
    total_size += get_dir_size_ex(path);
  }
  
  CDRTL2("Method Exit, size '%lld'", total_size);
  return total_size;
}


long long int get_nv_partition_db_table_size(int norm_id)
{
  CDRTL2("Method called. NV Client ID '%s' cache_line: '%lld'", 
                        nv_client_id, cdr_nv_cache_entry_list[norm_id].partition_num);

  long long int total_size = 0; 

  CDRTL2("Method Exit, size '%lld'", total_size);
  return total_size;
}

long long int get_nv_partition_db_index_size(int norm_id)
{
  CDRTL2("Method called. NV Client ID '%s' cache_line: '%lld'", 
                        nv_client_id, cdr_nv_cache_entry_list[norm_id].partition_num);

  long long int total_size = 0; 

  CDRTL2("Method Exit, size '%lld'", total_size);
  return total_size;
}

long long int get_nv_logs_size(int norm_id)
{
  CDRTL2("Method called. NV Client ID '%s'", nv_client_id);

  char path[CDR_FILE_PATH_SIZE] = "\0";
  long long int total_size = 0;

  sprintf(path, "%s/rum/logs/", hpd_root);
  total_size += get_similar_file_size(path, log_or_trace_filter);

  CDRTL2("Method Exit, size '%lld'", total_size);
  return total_size;
}


long long int get_nv_partition_logs_size(int norm_id)
{
  CDRTL2("Method called. NV Client ID '%s' cache_line: '%lld'", 
                        nv_client_id, cdr_nv_cache_entry_list[norm_id].partition_num);

  char path[CDR_FILE_PATH_SIZE] = "\0";
  long long int total_size = 0;

  sprintf(path, "%s/rum/%lld/logs/", hpd_root,
                       cdr_nv_cache_entry_list[norm_id].partition_num);
  total_size += get_similar_file_size(path, log_or_trace_filter);

  sprintf(path, "%s/rum/%lld/", hpd_root,
                       cdr_nv_cache_entry_list[norm_id].partition_num);
  total_size += get_similar_file_size(path, log_or_trace_filter);

  sprintf(path, "%s/logs/", hpd_root);
  total_size += get_similar_file_size(path, log_or_trace_filter);
  
  sprintf(path, "%s/rum/%lld/mon_logs/", hpd_root,
                       cdr_nv_cache_entry_list[norm_id].partition_num);
  total_size += get_similar_file_size(path, log_or_trace_filter);

  CDRTL2("Method Exit, size '%lld'", total_size);
  return total_size;
}

long long int get_nv_partition_ocx_size(int norm_id)
{
  CDRTL2("Method called. NV Client ID '%s' cache_line: '%lld'", 
                        nv_client_id, cdr_nv_cache_entry_list[norm_id].partition_num);

  char path[CDR_FILE_PATH_SIZE] = "\0";
  long long int total_size = 0; 
  int i;

  for(i = 1; i <= cdr_nv_cache_entry_list[norm_id].num_proc; i++){
    sprintf(path, "%s/rum/%lld/%03d/snapshot/", hpd_root,
                       cdr_nv_cache_entry_list[norm_id].partition_num, i);
    total_size += get_dir_size_ex(path);
    
    sprintf(path, "%s/rum/%lld/%03d/useraction/", hpd_root,
                         cdr_nv_cache_entry_list[norm_id].partition_num, i);
    total_size += get_dir_size_ex(path);
    
    sprintf(path, "%s/rum/%lld/%03d/feedback/", hpd_root,
                         cdr_nv_cache_entry_list[norm_id].partition_num, i);
    total_size += get_dir_size_ex(path);
  }
  
  CDRTL2("Method Exit, size '%lld'", total_size);
  return total_size;
}

long long int get_nv_partition_na_traces_size(int norm_id)
{
  CDRTL2("Method called. NV Client ID '%s' cache_line: '%lld'", 
                        nv_client_id, cdr_nv_cache_entry_list[norm_id].partition_num);

  char path[CDR_FILE_PATH_SIZE] = "\0";
  long long int total_size = 0; 
  int i;

  for(i = 1; i <= cdr_nv_cache_entry_list[norm_id].num_proc; i++){
    sprintf(path, "%s/rum/%lld/%03d/traces/", hpd_root,
                         cdr_nv_cache_entry_list[norm_id].partition_num, i);
    total_size += get_dir_size_ex(path);
  }
  
  CDRTL2("Method Exit, size '%lld'", total_size);
  return total_size;
}

long long int get_nv_access_log_size(int norm_id)
{
  CDRTL2("Method called. NV Client ID '%s'", nv_client_id);

  char path[CDR_FILE_PATH_SIZE] = "\0";
  long long int total_size = 0; 

  sprintf(path, "%s/logs/access_log/", hpd_root);
  total_size += get_dir_size_ex(path);
  
  sprintf(path, "%s/rum/%lld/logs/access_log/", hpd_root,
                       cdr_nv_cache_entry_list[norm_id].partition_num);
  total_size += get_dir_size_ex(path); 
 
  CDRTL2("Method Exit, size '%lld'", total_size);
  return total_size;
}

long long int get_nv_partition_access_log_size(int norm_id)
{
  CDRTL2("Method called. NV Client ID '%s' cache_line: '%lld'", 
                        nv_client_id, cdr_nv_cache_entry_list[norm_id].partition_num);

  char path[CDR_FILE_PATH_SIZE] = "\0";
  long long int total_size = 0; 

  sprintf(path, "%s/rum/%lld/logs/access_log/", hpd_root, cdr_nv_cache_entry_list[norm_id].partition_num);
  total_size += get_dir_size_ex(path);
  
  CDRTL2("Method Exit, size '%lld'", total_size);
  return total_size;
}

int get_nv_partition_type(long long int partition_num)
{
  CDRTL2("Method called. NV Client ID '%s' cache_line: '%lld'", 
                        nv_client_id, partition_num);

  CDRTL2("Method Exit, size"); 
  return 0;
}



static void nv_check_ngve_cleanup_days(long long int partition_num, struct ngve_cleanup_days_struct *days, long long int *flag)
{
  CDRTL2("Method called,  partition num '%lld'", partition_num);
  for(int i = 0; i < days->total_entry; i++)
  {
    if(partition_num >= days->start_date_pf[i] && partition_num <= days->end_date_pf[i]) 
      *flag = CONFIG_FALSE;
  }
  CDRTL2("Method exit");
}



void nv_check_ngve_range(int norm_id, int cur_idx)
{
  CDRTL2("Method called. partition num: '%lld'", cdr_nv_cache_entry_list[norm_id].partition_num);

  struct ngve_cleanup_days_struct *days;
  long long int partition_num = cdr_nv_cache_entry_list[norm_id].partition_num;

  days = &(cdr_config.ngve_cleanup_days[DB]);
  nv_check_ngve_cleanup_days(partition_num, days, &(cdr_nv_cache_entry_list[norm_id].partition_db_remove_f));

  days = &(cdr_config.ngve_cleanup_days[CSV]);
  nv_check_ngve_cleanup_days(partition_num, days, &(cdr_nv_cache_entry_list[norm_id].partition_csv_remove_f));

  days = &(cdr_config.ngve_cleanup_days[OCX]);
  nv_check_ngve_cleanup_days(partition_num, days, &(cdr_nv_cache_entry_list[norm_id].partition_ocx_remove_f));

  days = &(cdr_config.ngve_cleanup_days[NA_TRACES]);
  nv_check_ngve_cleanup_days(partition_num, days, &(cdr_nv_cache_entry_list[norm_id].partition_na_traces_remove_f));

  days = &(cdr_config.ngve_cleanup_days[ACCESS_LOG]);
  nv_check_ngve_cleanup_days(partition_num, days, &(cdr_nv_cache_entry_list[norm_id].partition_access_log_remove_f));

  days = &(cdr_config.ngve_cleanup_days[LOGS]);
  nv_check_ngve_cleanup_days(partition_num, days, &(cdr_nv_cache_entry_list[norm_id].partition_logs_remove_f));

  CDRTL2("Method exit");
}

void cdr_dump_nv_cache_to_file()
{
  CDRTL2("Method called, total_cache_entry '%d'", total_nv_cache_entry);
  CDRTL3("Dumping cache to file: %s", nv_cache_file_path);

  FILE *fp;
  fp = fopen(nv_cache_file_path, "w");
  if(!fp) {
    CDRTL1("Error: cannot open nv cache file: %s, error: %s", cache_file_path, strerror(errno));
    return;
  }
  fprintf(fp, "#\n");

  for(int i = 0; i < total_nv_cache_entry; ++i) {
    if (cdr_nv_cache_entry_list[i].is_partition_present == CDR_TRUE)
    {
      fprintf(fp, "%lld|%d|%lld|%lld|%lld|%lld|%lld|" "%lld|%lld|%lld|" "%lld|%lld|%lld|%lld|" "%lld|%lld|%d\n",
            cdr_nv_cache_entry_list[i].partition_num,
            cdr_nv_cache_entry_list[i].partition_type,
            cdr_nv_cache_entry_list[i].partition_disk_size,
            cdr_nv_cache_entry_list[i].partition_db_table_size,
            cdr_nv_cache_entry_list[i].partition_db_index_size,
            cdr_nv_cache_entry_list[i].partition_csv_size, 
            cdr_nv_cache_entry_list[i].partition_ocx_size, 

            cdr_nv_cache_entry_list[i].partition_na_traces_size, 
            cdr_nv_cache_entry_list[i].partition_access_log_size, 
            cdr_nv_cache_entry_list[i].partition_logs_size, 
            cdr_nv_cache_entry_list[i].partition_db_remove_f, 
            cdr_nv_cache_entry_list[i].partition_csv_remove_f, 
            cdr_nv_cache_entry_list[i].partition_ocx_remove_f,
            cdr_nv_cache_entry_list[i].partition_na_traces_remove_f,

            cdr_nv_cache_entry_list[i].partition_access_log_remove_f, 
            cdr_nv_cache_entry_list[i].partition_logs_remove_f,
            cdr_nv_cache_entry_list[i].num_proc);

    }
  }
  CDRTL2("Method exit");
}

static void cdr_drop_aggregate_tables_older_than_given_date(long long date)
{
  char Query[1024];
  PGconn *dbConn;
  PGresult *res;
  char connectionString[] = ("user=cavisson dbname=test");
  int nrows = 0;
  char *partition;

  /*make connection to POSTGRES db*/
  dbConn = PQconnectdb(connectionString);

  if(PQstatus(dbConn) == CONNECTION_BAD)
  {
    CDRTL1("Error: ERROR[%s] while making connection to database with args[%s]", PQerrorMessage(dbConn), connectionString);
    PQfinish(dbConn);
    return;
  }

  char *ptr = strrchr (ns_wdir, '/');
  if (!ptr)
  {
    CDRTL1("Error: NS_WDIR is not set. Please set NS_WDIR\n");
    return;
  }
  partition = ptr + 9;
  
 
  sprintf (Query, "select table_name from information_schema.tables where "
                   "(table_name ilike '%%NVPageAggregateTable%%' OR table_name ilike '%%NVSessionAggregateTable%%')"
                   "AND table_name ilike '%%%s%%';", partition);

  res = PQexec(dbConn, Query);
  if(PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    CDRTL1("Error: Query[%s] execution failed with ERROR[%s]", Query, PQerrorMessage(dbConn));
    PQclear(res);
    PQfinish(dbConn);
    return;
  }

  nrows = PQntuples(res);
  if(nrows <= 0)
  {
    CDRTL3("Query[%s] executed successfully but no row is returned", Query);
    PQclear(res);
    PQfinish(dbConn);
    return;
  }

  else
  {
    char *ptr, *deleteTableQuery = NULL;
    long val;
    int deleteTableQuery_currOff = 0, malloced_size = 2048;

    deleteTableQuery = malloc(malloced_size);
    deleteTableQuery_currOff += sprintf(deleteTableQuery + deleteTableQuery_currOff, "drop table if exists ");
    for (int i = 0; i < nrows; ++ i)
    {
      ptr = strchr (PQgetvalue(res, i, 0), '_');
      if (ptr)
      {
        val = atol (ptr+1);
        if (date - val > 0) {
          if(malloced_size - deleteTableQuery_currOff <= strlen(PQgetvalue(res, i, 0)) + 1) {
            malloced_size += malloced_size;
            deleteTableQuery = realloc(deleteTableQuery, malloced_size);
            if(!deleteTableQuery) {
              CDRTL1("Error: Realloc fails exit");
              exit(-1);
            }
          }
          deleteTableQuery_currOff += sprintf(deleteTableQuery + deleteTableQuery_currOff, "%s,", PQgetvalue(res, i, 0));
        }
      }
    }

    if(deleteTableQuery_currOff)
      deleteTableQuery[deleteTableQuery_currOff - 1] = '\0';

    PQclear(res);
    res = PQexec(dbConn, deleteTableQuery);

    if(PQresultStatus(res) != PGRES_COMMAND_OK)
      CDRTL1("Error: ERROR[%s] while executing Query\n", PQerrorMessage(dbConn));
    /*else
      CDRTL1("Query executed successfully\n");*/

    PQclear(res);
    PQfinish(dbConn);
    free (deleteTableQuery);
    deleteTableQuery = NULL;
  }
}

void remove_nv_partition_db(int norm_id)
{
  CDRTL2("Method called. NV Client ID '%s' cache_line: '%lld'",
                        nv_client_id, cdr_nv_cache_entry_list[norm_id].partition_num);
 
  long long int total_time = get_ts_in_ms();
  make_query_and_drop_tables(0, cdr_nv_cache_entry_list[norm_id].partition_num);
    
  total_time = get_ts_in_ms() - total_time;
 
  CDRNVAL(0, nv_client_id, cdr_nv_cache_entry_list[norm_id].partition_num, "NV-Partition-DB",  cdr_nv_cache_entry_list[norm_id].partition_csv_size, "Delete", "Delete due to retention time", total_time, "Retention Manager");

  CDRTL2("Method Exit");
}

void remove_nv_partition_csv(int norm_id)
{
  CDRTL2("Method called. NV Client ID '%s' cache_line: '%lld'",
                        nv_client_id, cdr_nv_cache_entry_list[norm_id].partition_num);

  char path[CDR_FILE_PATH_SIZE] = "\0";
  int i;
  long long int total_time = get_ts_in_ms();

  for(i = 1; i <= cdr_nv_cache_entry_list[norm_id].num_proc; i++)
  {
    sprintf(path, "%s/rum/%lld/%03d/db/", hpd_root,
                         cdr_nv_cache_entry_list[norm_id].partition_num, i);
    remove_dir_file_ex(path);
  }

  total_time = get_ts_in_ms() - total_time;
  CDRNVAL(0, nv_client_id, cdr_nv_cache_entry_list[norm_id].partition_num, "NV-Partition-CSV",  cdr_nv_cache_entry_list[norm_id].partition_csv_size, "Delete", "Delete due to retention time", total_time, "Retention Manager");

  CDRTL2("Method Exit");
}
void remove_nv_partition_ocx(int norm_id)
{
  CDRTL2("Method called. NV Client ID '%s' cache_line: '%lld'",
                        nv_client_id, cdr_nv_cache_entry_list[norm_id].partition_num);

  char path[CDR_FILE_PATH_SIZE] = "\0";
  int i;
  long long int total_time = get_ts_in_ms();

  for(i = 1; i <= cdr_nv_cache_entry_list[norm_id].num_proc; i++){
    sprintf(path, "%s/rum/%lld/%03d/snapshot/", hpd_root,
                       cdr_nv_cache_entry_list[norm_id].partition_num, i);
    remove_dir_file_ex(path);

    sprintf(path, "%s/rum/%lld/%03d/useraction/", hpd_root,
                         cdr_nv_cache_entry_list[norm_id].partition_num, i);
    remove_dir_file_ex(path);

    sprintf(path, "%s/rum/%lld/%03d/feedback/", hpd_root,
                         cdr_nv_cache_entry_list[norm_id].partition_num, i);
    remove_dir_file_ex(path);
  }

  total_time = get_ts_in_ms() - total_time;
  CDRNVAL(0, nv_client_id, cdr_nv_cache_entry_list[norm_id].partition_num, "NV-Partition-OCX",  cdr_nv_cache_entry_list[norm_id].partition_ocx_size, "Delete", "Delete due to retention time", total_time, "Retention Manager");

  CDRTL2("Method Exit");
}

void remove_nv_partition_na_traces(int norm_id)
{
  CDRTL2("Method called. NV Client ID '%s' cache_line: '%lld'",
                        nv_client_id, cdr_nv_cache_entry_list[norm_id].partition_num);

  char path[CDR_FILE_PATH_SIZE] = "\0";
  int i;
  long long int total_time = get_ts_in_ms();

  for(i = 1; i <= cdr_nv_cache_entry_list[norm_id].num_proc; i++){
    sprintf(path, "%s/rum/%lld/%03d/traces/", hpd_root,
                         cdr_nv_cache_entry_list[norm_id].partition_num, i);
    remove_dir_file_ex(path);
  }

  total_time = get_ts_in_ms() - total_time;
  CDRNVAL(0, nv_client_id, cdr_nv_cache_entry_list[norm_id].partition_num, "NV-Partition-NA_TRACES",  cdr_nv_cache_entry_list[norm_id].partition_na_traces_size, "Delete", "Delete due to retention time", total_time, "Retention Manager");

  CDRTL2("Method Exit");
}

void remove_nv_partition_access_log(int norm_id)
{
  CDRTL2("Method called. NV Client ID '%s' cache_line: '%lld'",
                        nv_client_id, cdr_nv_cache_entry_list[norm_id].partition_num);

  char path[CDR_FILE_PATH_SIZE] = "\0";
  long long int total_time = get_ts_in_ms();

  sprintf(path, "%s/logs/access_log/", hpd_root);
  remove_dir_file_ex(path);

  sprintf(path, "%s/rum/%lld/logs/access_log/", hpd_root, cdr_nv_cache_entry_list[norm_id].partition_num);
  remove_dir_file_ex(path);
  
  total_time = get_ts_in_ms() - total_time;
  CDRNVAL(0, nv_client_id, cdr_nv_cache_entry_list[norm_id].partition_num, "NV-Partition-ACCESS_LOG",  cdr_nv_cache_entry_list[norm_id].partition_access_log_size, "Delete", "Delete due to retention time", total_time, "Retention Manager");

  CDRTL2("Method Exit");
}

void remove_nv_partition_logs(int norm_id)
{
  struct cleanup_struct *cleanup_policy = g_other_tr_cleanup_policy_ptr;

  CDRTL2("Method called. NV Client ID '%s' cache_line: '%lld'",
                        nv_client_id, cdr_nv_cache_entry_list[norm_id].partition_num);

  char path[CDR_FILE_PATH_SIZE] = "\0";
  long long int total_size = 0;
  long long int total_time = 0;  
  
  sprintf(path, "%s/rum/%lld/logs/", hpd_root, cdr_nv_cache_entry_list[norm_id].partition_num);
  remove_similar_file(path, log_or_trace_filter, cleanup_policy[LOGS].retention_time); 
 
  sprintf(path, "%s/rum/%lld/", hpd_root, cdr_nv_cache_entry_list[norm_id].partition_num);
  remove_similar_file(path, log_or_trace_filter, cleanup_policy[LOGS].retention_time);
 
  sprintf(path, "%s/logs/", hpd_root);
  remove_similar_file(path, log_or_trace_filter, cleanup_policy[LOGS].retention_time);  

  sprintf(path, "%s/rum/%lld/mon_logs/", hpd_root, cdr_nv_cache_entry_list[norm_id].partition_num);
  remove_dir_file_ex(path);

  total_time = get_ts_in_ms() - total_time;
 
  CDRNVAL(0, nv_client_id, cdr_nv_cache_entry_list[norm_id].partition_num, "NV-Partition-LOGS",  cdr_nv_cache_entry_list[norm_id].partition_logs_size, "Delete", "Delete due to retention time", total_time, "Retention Manager");

  CDRTL2("Method Exit, size '%lld'", total_size);
}

static void cdr_delete_access_log_files_older_than_given_date (long date) 
{
  struct dirent *de;
  char *ptr;
  long val = 0;
  char filename [1024];

  char directory[1024];
  sprintf (directory, "%s/logs/access_log/", hpd_root);
  DIR *dr = opendir(directory); 

  if (!dr)
  {
    CDRTL1("Error: Could not open current directory err = '%s", strerror(errno));
    return ; 
  } 

  while ((de = readdir(dr)) != NULL ) 
  {
    if (de->d_type == DT_REG && de->d_name[0] != '.') 
    {
      ptr = strrchr (de->d_name, '_');
      if (ptr)
      {
        val = atol (ptr+1);
        if (date - val >= 0)
        {
          sprintf (filename, "%s/%s", directory, de->d_name);
          remove_dir_file_ex(filename);
        }
      }
    }
  }
  closedir(dr);
}

void cdr_cleanup_overall_data()
{
  CDRTL2("Method called"); 
  struct cleanup_struct *cleanup_policy = g_other_tr_cleanup_policy_ptr;
  long long int timestamp = cur_time_stamp - (cleanup_policy[ACCESS_LOG].retention_time * (ONE_DAY_IN_SEC));
  char outbuf [24];
  long date;

  change_ts_partition_name_format(timestamp, outbuf, 24);

  if (cleanup_policy[DB_AGG].retention_time >= -1 && cdr_config.data_remove_flag == TRUE)
      cdr_drop_aggregate_tables_older_than_given_date(atol(outbuf));

  outbuf[8] = '\0';
  date = atol(outbuf);
  
  if (cleanup_policy[ACCESS_LOG].retention_time >= -1 && cdr_config.data_remove_flag == TRUE)
      cdr_delete_access_log_files_older_than_given_date(date);

  CDRTL2("Method Exit");
}

void nv_cleanup_process()
{
  CDRTL2("Method called");

  if(nv_partition_list == NULL)
  {
    CDRTL1("Error: No Partition for NV client id '%s'.", nv_client_id);
    return;
  }

  CDRTL3("Partition_list is set for NV client id '%s', total number of partition '%d'.",
            nv_client_id, nv_partition_count);

  long int lol_time = cur_time_stamp;
  if(cur_time_in_ngve_days() == TRUE)
  {
    // Only house keeing work - get the new TRs and size of the comonents
    CDRTL3("Current date: [%s] is in negative days.", ctime(&(lol_time)));
    cdr_config.data_remove_flag = FALSE;
  }

  if(cdr_config.cleanup_flag == CDR_FALSE)
  {
    CDRTL1("Error: Cleanup policy is not set for CMT TR");
    return;
  }

  int i;
  struct cdr_nv_cache_entry *entry;

  char partition_db_remove_f = FALSE;
  char partition_csv_remove_f = FALSE;
  char partition_ocx_remove_f = FALSE;
  char partition_na_traces_remove_f = FALSE;
  char partition_access_log_remove_f = FALSE; 
  char partition_logs_remove_f = FALSE;

  struct cleanup_struct *cleanup_policy = g_other_tr_cleanup_policy_ptr;
  char partition_num_buf[16];
  long long int partition_ts;
  int partition_days;
  
  cdr_cleanup_overall_data();

  /*Partition are save in increment order
    starting loop from last to first */
  for(i = nv_partition_count -1; i >= 0 ; i--)
  {
    entry = &(cdr_nv_cache_entry_list[i]);

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
    if( i == nv_partition_count - 1)
    {
      CDRTL4("Partition %lld is running partiton", cdr_nv_cache_entry_list[i].partition_num);
      entry->partition_type = RUNNING_PARTITION;
      continue;
    }

    // we are assuming partition end time is next partion start time 
    snprintf(partition_num_buf, 16, "%lld", cdr_nv_cache_entry_list[i+1].partition_num);
    partition_ts = partition_format_date_convert_to_ts(partition_num_buf);

    // Add logic for partition days
    partition_days = (cur_time_stamp - partition_ts) / (ONE_DAY_IN_SEC);

    if (partition_db_remove_f == FALSE && cleanup_policy[DB].retention_time > -1)
    {
      if (entry->partition_db_remove_f == TRUE)
      {
        partition_db_remove_f = TRUE;
      }
      else if(cdr_config.data_remove_flag == TRUE && 
               entry->partition_db_remove_f != CONFIG_FALSE && partition_days > cleanup_policy[DB].retention_time)
      {
        entry->partition_db_remove_f = TRUE;
        remove_nv_partition_db(i);
        CDRTL3("DB removed for NV Client ID: %s, partition = '%lld', size: '%lld' bytes", 
                nv_client_id, entry->partition_num, 
                (entry->partition_db_table_size + entry->partition_db_index_size));
      }
      else 
        entry->partition_db_remove_f = ((long long int)(cleanup_policy[DB].retention_time - partition_days) 
                                                * (ONE_DAY_IN_SEC)) + cur_time_stamp_with_no_hr;
    }
 
 
    if (partition_csv_remove_f == FALSE && cleanup_policy[CSV].retention_time > -1)
    {
      if (entry->partition_csv_remove_f == TRUE)
      {
        partition_csv_remove_f = TRUE;
      }
      else if(cdr_config.data_remove_flag == TRUE 
                && entry->partition_csv_remove_f != CONFIG_FALSE && partition_days > cleanup_policy[CSV].retention_time)
      {
        entry->partition_csv_remove_f = TRUE;
        remove_nv_partition_csv(i);
        CDRTL3("DB removed for NV Client ID '%s', partition = '%lld', size: '%lld' bytes", 
                nv_client_id, entry->partition_num, entry->partition_csv_size); 
      }
      else 
        entry->partition_csv_remove_f = ((long long int)(cleanup_policy[CSV].retention_time - partition_days) 
                                                * (ONE_DAY_IN_SEC)) + cur_time_stamp_with_no_hr;
    }
 
    if (partition_ocx_remove_f == FALSE && cleanup_policy[OCX].retention_time > -1)
    {
      if (entry->partition_ocx_remove_f == TRUE)
      {
        partition_ocx_remove_f = TRUE;
      }
      else if(cdr_config.data_remove_flag == TRUE 
                && entry->partition_ocx_remove_f != CONFIG_FALSE && partition_days > cleanup_policy[OCX].retention_time)
      {
        entry->partition_ocx_remove_f = TRUE;
        remove_nv_partition_ocx(i);
        CDRTL3("DB removed for NV Client ID '%s', partition = '%lld', size: '%lld' bytes", 
                nv_client_id, entry->partition_num, entry->partition_ocx_size); 
      }
      else 
        entry->partition_ocx_remove_f = ((long long int)(cleanup_policy[OCX].retention_time - partition_days) 
                                                * (ONE_DAY_IN_SEC)) + cur_time_stamp_with_no_hr;
    }
 
    if (partition_na_traces_remove_f == FALSE && cleanup_policy[NA_TRACES].retention_time > -1)
    {
      if (entry->partition_na_traces_remove_f == TRUE)
      {
        partition_na_traces_remove_f = TRUE;
      }
      else if(cdr_config.data_remove_flag == TRUE 
                && entry->partition_na_traces_remove_f != CONFIG_FALSE && partition_days > cleanup_policy[NA_TRACES].retention_time)
      {
        entry->partition_na_traces_remove_f = TRUE;
        remove_nv_partition_na_traces(i);
        CDRTL3("DB removed for NV Client ID '%s', partition = '%lld', size: '%lld' bytes", 
                nv_client_id, entry->partition_num, entry->partition_na_traces_size); 
      }
      else
        entry->partition_na_traces_remove_f = ((long long int)(cleanup_policy[NA_TRACES].retention_time - partition_days) 
                                                * (ONE_DAY_IN_SEC)) + cur_time_stamp_with_no_hr;
    }
 
    if (partition_access_log_remove_f == FALSE && cleanup_policy[ACCESS_LOG].retention_time > -1)
    {
      if (entry->partition_access_log_remove_f == TRUE)
      {
        partition_access_log_remove_f = TRUE;
      }
      else if(cdr_config.data_remove_flag == TRUE 
                && entry->partition_access_log_remove_f != CONFIG_FALSE && partition_days > cleanup_policy[ACCESS_LOG].retention_time)
      {
        entry->partition_access_log_remove_f = TRUE;
        remove_nv_partition_access_log(i);
        CDRTL3("DB removed for NV Client ID '%s', partition = '%lld', size: '%lld' bytes", 
                nv_client_id, entry->partition_num, entry->partition_access_log_size); 
      }
      else
        entry->partition_access_log_remove_f = ((long long int)(cleanup_policy[ACCESS_LOG].retention_time - partition_days) 
                                                * (ONE_DAY_IN_SEC)) + cur_time_stamp_with_no_hr;
    }
 
    if (partition_logs_remove_f == FALSE && cleanup_policy[LOGS].retention_time > -1)
    {
      if (entry->partition_logs_remove_f == TRUE)
      {
        partition_logs_remove_f = TRUE;
      }
      else if(cdr_config.data_remove_flag == TRUE && 
               entry->partition_logs_remove_f != CONFIG_FALSE && partition_days > cleanup_policy[LOGS].retention_time)
      {
        entry->partition_logs_remove_f = TRUE;
        remove_nv_partition_logs(i);
        CDRTL3("DB removed for NV Client ID '%s', partition = '%lld', size: '%lld' bytes", 
                nv_client_id, entry->partition_num, entry->partition_logs_size);
      }
      else
        entry->partition_logs_remove_f = ((long long int)(cleanup_policy[LOGS].retention_time - partition_days) 
                                                * (ONE_DAY_IN_SEC)) + cur_time_stamp_with_no_hr;
    }
  }

  CDRTL2("Method exit");
}

void cdr_get_nv_client_id()
{
  char file_name[CDR_FILE_PATH_SIZE];
  char data[CDR_FILE_PATH_SIZE];
  FILE *fp;

  CDRTL2("Method called");

  sprintf(file_name, "%s/rum/config/rum.conf", hpd_root);
  fp = fopen(file_name, "r");

  if(!fp) {
    CDRTL1("Error: Unable to open '%s', error : '%s'.", file_name, strerror(errno));
    return ;
  }
  int len = 14; //strlen("RUM_CLIENT_ID ");
  while(fgets(data, CDR_FILE_PATH_SIZE, fp))
  {
    CDRTL4("Data from file : '%s'.", data);
    if(data[0] == '#') // ignoring comment lines
      continue;

    if(!strncmp(data, "RUM_CLIENT_ID ", len))
    {
      char *ptr = data + len;
      snprintf(nv_client_id, CDR_FILE_PATH_SIZE, "%s", ptr);
    }
  }

  CDRTL2("Method exit, nv_client_id '%s'", nv_client_id);
}

int get_nv_partition_num_proc(long long int partition_num)
{
  CDRTL2("Method called, NV Partition num '%lld'", partition_num);
  char file_name[CDR_FILE_PATH_SIZE];
  char data[CDR_FILE_PATH_SIZE] = "\0";
  FILE *fp;

  sprintf(file_name, "%s/rum/%lld/.num_process", hpd_root, partition_num);
  fp = fopen(file_name, "r");

  if(!fp) {
    CDRTL1("Error: Unable to open '%s', error : '%s'.", file_name, strerror(errno));
    return 0;
  }
  fgets(data, CDR_FILE_PATH_SIZE, fp);
  int num_proc = atoi(data);
  CDRTL2("Method exit, num process '%d'", num_proc);
  return num_proc;
}
 
