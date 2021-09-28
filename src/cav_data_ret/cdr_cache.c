#include <string.h>
#include <stdio.h>
#include <libpq-fe.h>
#include <stdlib.h>
#include <errno.h>

#include "cdr_cache.h"
#include "cdr_log.h"
#include "cdr_mem.h"
#include "cdr_main.h"
#include "cdr_utils.h"
#include "cdr_components.h"
#include "cdr_dir_operation.h"
#include "cdr_cleanup.h"
#include "cdr_cmt_handler.h"
#include "cdr_nv_handler.h"
#include "nslb_get_norm_obj_id.h"
#include "nslb_util.h"


#define NUM_SUMMARY_TOP_FIELDS 16

// For TR cache 
struct NormObjKey cache_entry_norm_table; // use tr_num to get the index of cache entry in cdr_cache_entry_list
struct cdr_cache_entry *cdr_cache_entry_list = NULL;
int total_cache_entry = 0;
int max_cache_entry = 0;


//For CMT cache
struct NormObjKey cmt_cache_entry_norm_table; // use cmt partition num to get the index of cache entry in cdr_cache_entry_list
struct cdr_cmt_cache_entry *cdr_cmt_cache_entry_list = NULL;
int total_cmt_cache_entry = 0;
int max_cmt_cache_entry = 0;


//For NV cache
struct NormObjKey nv_cache_entry_norm_table; // use nv partition num to get the index of cache entry in cdr_cache_entry_list
struct cdr_nv_cache_entry *cdr_nv_cache_entry_list = NULL;
int total_nv_cache_entry = 0;
int max_nv_cache_entry = 0;

static long long get_tr_disk_size(int tr_num)
{
  CDRTL2("Method called");
  char path[CDR_FILE_PATH_SIZE] = "\0";

  sprintf(path, "%s/logs/TR%d/", ns_wdir, tr_num);
  long long int total_size = get_dir_size_ex(path);

  CDRTL2("Method Exit, TR '%d', sie '%lld'", tr_num, total_size);
  return total_size;
}

void close_dbConn(PGconn *dbConn)
{
  PQfinish(dbConn);
}

void clear_res_and_close_dbConn(PGconn *dbConn, PGresult *res)
{
  PQclear(res);
  close_dbConn(dbConn);
}
void partiton_tableSize_with_indexes_size(long long int partition_num, double *sz, double *sz_index)
{
  if (!partition_num) 
  {
    return;
  }
 
  char Query[8 * 1024];
  PGconn *dbConn;
  PGresult *res;
  char connectionString[] = ("user=cavisson dbname=test");

  /*make connection to POSTGRES db*/
  dbConn = PQconnectdb(connectionString); 

  if(PQstatus(dbConn) == CONNECTION_BAD)
  {
    CDRTL1("Error: [%s] while making connection to database with args[%s]", PQerrorMessage(dbConn), connectionString);
    
    close_dbConn(dbConn);

    return;
  }
  
  /*make full Query*/
  sprintf(Query,"select round(sum(innerr.table_Size)/1024,2) as table_Size_in_kb,\n" 
                "round(sum(innerr.table_Size_Including_Indexes)/1024,2) as table_Size_Including_Indexes_in_kb\n"
                "from\n"
                "(\n"
                   " select inn.table_name,\n"  
                   " pg_relation_size(inn.table_name) as table_Size,\n"
                   " pg_total_relation_size(inn.table_name) as table_Size_Including_Indexes\n" 
                   " from\n" 
                   " ( select\n" 
                      " table_name\n" 
                      " from\n" 
                      " information_schema.tables\n"
                      " where\n" 
                      " table_name ilike '%%_%lld_%%'"
                   " )as inn\n"
                ")as innerr;",partition_num );
  
 // printf("QUERY[%s]", Query);

  res = PQexec(dbConn, Query);

  if(PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    CDRTL1("Error: Query[%s] execution failed with ERROR[%s]", Query, PQerrorMessage(dbConn));
   
    clear_res_and_close_dbConn(dbConn, res);
    
    return;
  }

  /*ZERO rows is returned */
  if(!PQntuples(res))
  {
    CDRTL4("Query[%s] executed successfully but no row is returned", Query);
    
    clear_res_and_close_dbConn(dbConn, res);
   
    return;
  }  


  if(PQgetvalue(res, 0, 0))
  {
    *sz = atof(PQgetvalue(res, 0, 0));
  }
  
  if(PQgetvalue(res, 0, 1))
  {
    *sz_index = atof(PQgetvalue(res, 0, 1));
  }
  
  clear_res_and_close_dbConn(dbConn, res);
}

void testRun_tableSize_with_indexes_size(int testrun, double *sz, double *sz_index)
{
  if (!testrun) 
  {
//    *sz = 0;
  //  *sz_index = 0;
    return;
  }
 
#if 0
  if (!path)
  {
    printf ("Controller name is NULL; Hence returning \n");
    return NULL;
  }

  char *ctl=NULL;
  char *ptr = strchr (path+1, '/');
  if (ptr)
  {
    ptr = strchr (ptr+1, '/');
    if (ptr)
     *ptr = '\0';
 
    ctl = ptr+1;
    ptr =  strchr (ctl, '/');
    if (ptr)
     *ptr = '\0';
  }
 
  if (!ctl)
    return NULL;
#endif
  char Query[8 * 1024];
  PGconn *dbConn;
  PGresult *res;
  char connectionString[] = ("user=cavisson dbname=test");

  /*make connection to POSTGRES db*/
  dbConn = PQconnectdb(connectionString); 

  if(PQstatus(dbConn) == CONNECTION_BAD)
  {
    CDRTL1("Error: [%s] while making connection to database with args[%s]", PQerrorMessage(dbConn), connectionString);
    
    close_dbConn(dbConn);

    return;
  }
  
  /*make full Query*/
  sprintf(Query,"select round(sum(innerr.table_Size)/1024,2) as table_Size_in_kb,\n" 
                "round(sum(innerr.table_Size_Including_Indexes)/1024,2) as table_Size_Including_Indexes_in_kb\n"
                "from\n"
                "(\n"
                   " select inn.table_name,\n"  
                   " pg_relation_size(inn.table_name) as table_Size,\n"
                   " pg_total_relation_size(inn.table_name) as table_Size_Including_Indexes\n" 
                   " from\n" 
                   " ( select\n" 
                      " table_name\n" 
                      " from\n" 
                      " information_schema.tables\n"
                      " where\n" 
                      " table_name ilike '%%%d%%'"
                   " )as inn\n"
                ")as innerr;",testrun );
  
 // printf("QUERY[%s]", Query);

  res = PQexec(dbConn, Query);

  if(PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    CDRTL1("Error: Query[%s] execution failed with ERROR[%s]", Query, PQerrorMessage(dbConn));
   
    clear_res_and_close_dbConn(dbConn, res);
    
    return;
  }

  /*ZERO rows is returned */
  if(!PQntuples(res))
  {
    CDRTL4("Query[%s] executed successfully but no row is returned", Query);
    
    clear_res_and_close_dbConn(dbConn, res);
   
    return;
  }  


  if(PQgetvalue(res, 0, 0))
  {
    *sz = atof(PQgetvalue(res, 0, 0));
  }
  
  if(PQgetvalue(res, 0, 1))
  {
    *sz_index = atof(PQgetvalue(res, 0, 1));
  }
  
  clear_res_and_close_dbConn(dbConn, res);
}
#if 0
static int get_tr_db_table_size(int tr_num)
{
  
  return 0;
}

static int get_tr_db_index_size(int tr_num)
{
  return 0;
}
#endif
/**********************************************************************************************
 * Name: create_cache_entry_from_tr
 *    This function will print all the cache information
 *    of tr_num to cache_buff
 *    returns CDR_SUCCESS on success, CDR_EERROR on error
 *    cache format:
  <summary.top fields>|start time stamp|tr_type|tr du size|tr db table size|tr db index size|lmd summary.top|key files size|
  metric percentile data size|har file size|csv size|page dump size|db size|logs size|raw files size|scripts size|test data size
 **********************************************************************************************/
int create_cache_entry_from_tr(int tr_num, int norm_id, int cal_size)
{
  char summary_top[CDR_FILE_PATH_SIZE] = "\0";
  char summary_top_buff[CDR_BUFFER_SIZE] = "\0";
  char tmp_buff[CDR_BUFFER_SIZE] = "\0";
  FILE *fp;
  char *cache_fields[TOTAL_CACHE_FIELDS];
  int n_fields;
  char inc_entry = 0;

  CDRTL2("Method called");

  sprintf(summary_top, "%s/logs/TR%d/summary.top", ns_wdir, tr_num);
  if( (fp = fopen(summary_top, "r")) == NULL) {
    CDRTL1("Error: cannot open: %s, Removing TR, error: %s", summary_top, strerror(errno));
    if(cal_size)
      remove_tr_ex(tr_num);
    return CDR_ERROR;
  }
  nslb_fgets(summary_top_buff, CDR_BUFFER_SIZE, fp, 0);
  int len = strlen(summary_top_buff);
  if(summary_top_buff[len - 1] == '\n')
    summary_top_buff[len - 1] = '\0';
  strcpy(tmp_buff, summary_top_buff);
  fclose(fp);

  n_fields = get_tokens(summary_top_buff, cache_fields, "|", NUM_SUMMARY_TOP_FIELDS);

  // If summary top is not completed 
 // if(n_fields > 16 || n_fields < 15) {
  if(n_fields != 16) {
    CDRTL1("Error: Bad summary.top. n_fields: %d", n_fields);
    return CDR_ERROR;
  }
  if(tr_num != atoi(cache_fields[TR_NUM]))
  {
    CDRTL1("Error: TR number is diffrent in summary.top, TR number '%d', summary.top TR number '%s'", tr_num, cache_fields[TR_NUM]);
    return CDR_ERROR;
  }

  char tr_type = get_test_run_type(tr_num, cache_fields[TEST_MODE], cal_size);

  if(tr_type == BAD_TR && cal_size)
    return CDR_ERROR;

  if (norm_id < 0)
  {
    inc_entry = 1;  
    norm_id = nslb_get_or_gen_norm_id(&cache_entry_norm_table, cache_fields[TR_NUM], strlen(cache_fields[TR_NUM]), NULL);
    if(norm_id >= max_cache_entry) {
      int new_alloc_size = max_cache_entry + DELTA_REALLOC_SIZE;
      CDR_REALLOC(cdr_cache_entry_list, new_alloc_size * sizeof(struct cdr_cache_entry), "cdr_cache_entry_list");
 
      memset(&(cdr_cache_entry_list[max_cache_entry]), 0, DELTA_REALLOC_SIZE * sizeof(struct cdr_cache_entry));
      max_cache_entry = new_alloc_size;
    }
  }

  // summary.top fields
  cdr_cache_entry_list[norm_id].tr_num = tr_num;
  strncpy(cdr_cache_entry_list[norm_id].scenario_name, cache_fields[SCEN_NAME], CDR_FILE_PATH_SIZE);
  strncpy(cdr_cache_entry_list[norm_id].start_time, cache_fields[START_TIME], CDR_BUFFER_SIZE);
  
  cdr_cache_entry_list[norm_id].report_summary = cache_fields[REPORT_SUMMARY][0];
  cdr_cache_entry_list[norm_id].page_dump = cache_fields[PAGE_DUMP][0];

  if(n_fields == 16)
    strncpy(cdr_cache_entry_list[norm_id].report_progress, cache_fields[REPORT_PROGRESS], CDR_BUFFER_SIZE);
  else
    strncpy(cdr_cache_entry_list[norm_id].report_progress, "-", CDR_BUFFER_SIZE);
  cdr_cache_entry_list[norm_id].report_detail = cache_fields[REPORT_DETAIL][0];
  cdr_cache_entry_list[norm_id].report_user = cache_fields[REPORT_USER][0];
  cdr_cache_entry_list[norm_id].report_fail = cache_fields[REPORT_FAIL][0];
  cdr_cache_entry_list[norm_id].report_page_break_down = cache_fields[REPORT_PAGE_BREAK_DOWN][0];
  cdr_cache_entry_list[norm_id].wan_env = atoi(cache_fields[WAN_ENV]);
  cdr_cache_entry_list[norm_id].reporting = atoi(cache_fields[REPORTING]);
  strncpy(cdr_cache_entry_list[norm_id].test_name, cache_fields[TEST_NAME], CDR_BUFFER_SIZE);
  strncpy(cdr_cache_entry_list[norm_id].test_mode, cache_fields[TEST_MODE], 64);
  snprintf(cdr_cache_entry_list[norm_id].runtime, CDR_BUFFER_SIZE, "%s",cache_fields[RUNTIME]);
  cdr_cache_entry_list[norm_id].vusers = atoi(cache_fields[VUSERS]);

  // extra fields
  cdr_cache_entry_list[norm_id].end_time_stamp = summary_top_time_convert_to_ts(cache_fields[START_TIME]) + convert_to_secs(cache_fields[RUNTIME]);  
  cdr_cache_entry_list[norm_id].start_time_ts = summary_top_time_convert_to_ts(cache_fields[START_TIME]);

  cdr_cache_entry_list[norm_id].lmd_summary_top = get_lmd_ts(summary_top);

  cdr_cache_entry_list[norm_id].tr_type = tr_type; 
  if (cdr_cache_entry_list[norm_id].tr_type == CMT_TR)
    cmt_tr_cache_idx = norm_id; // to read the cache index

  if (tr_type != CMT_TR && cal_size == 1)
  {
    //set Partition list to calculate the size
    
    cdr_cache_entry_list[norm_id].partition_list = get_tr_partiton_list(tr_num, &(cdr_cache_entry_list[norm_id].count), NULL);


    cdr_cache_entry_list[norm_id].tr_disk_size = get_tr_disk_size(tr_num);
    cdr_cache_entry_list[norm_id].graph_data_size = get_greph_data_size(norm_id);
    cdr_cache_entry_list[norm_id].csv_size = get_csv_size(norm_id);
    cdr_cache_entry_list[norm_id].raw_file_size = get_raw_files_size(norm_id);

    double size = 0.0, size_with_index = 0.0;
    testRun_tableSize_with_indexes_size (tr_num, &size, &size_with_index);

    cdr_cache_entry_list[norm_id].tr_db_table_size = size;
    cdr_cache_entry_list[norm_id].tr_db_index_size = size_with_index;
    cdr_cache_entry_list[norm_id].tr_db_table_size += get_db_file_size(norm_id);
    //cdr_cache_entry_list[norm_id].tr_db_table_size = get_tr_db_table_size(norm_id);
    //cdr_cache_entry_list[norm_id].tr_db_index_size = get_tr_db_index_size(norm_id);
    cdr_cache_entry_list[norm_id].key_file_size = get_key_files_size(norm_id);
    cdr_cache_entry_list[norm_id].har_file_size = get_har_file_size(norm_id);
    cdr_cache_entry_list[norm_id].page_dump_size = get_page_dump_size(norm_id);
    cdr_cache_entry_list[norm_id].logs_size = get_logs_size(norm_id);
    cdr_cache_entry_list[norm_id].test_data_size = get_test_data_size(norm_id);  
    cdr_cache_entry_list[norm_id].reports_size = get_reports_size(norm_id);
    check_ngve_range(&(cdr_cache_entry_list[norm_id]));
  }


  if (inc_entry)
    total_cache_entry++;
  CDRTL3("Cache entry added for tr: %s", cache_fields[TR_NUM]);

  //Abhi Print complete cache on level 4
  CDRTL2("Method exit");
  return norm_id;
}

int cache_entry_add(char *cache_line_buff, char present_flag)
{
  /* Purpose: This function will take a cache line and add to cache_entry_list.
   */
  char *cache_fields[TOTAL_CACHE_FIELDS];
  int n_fields;
  int norm_id;

  CDRTL2("Method called. cache_line: %s", cache_line_buff);
  if(cache_line_buff[0] == '#')
    return 0;

  n_fields = get_tokens(cache_line_buff, cache_fields, "|", TOTAL_CACHE_FIELDS);
  if(n_fields != TOTAL_CACHE_FIELDS) { 
    CDRTL1("Error: Bad cache buff, n_fields: %d", n_fields);
      return CDR_ERROR;
  }

  int tr_num = atoi(cache_fields[TR_NUM]);
  /*char tr_type = get_test_run_type(tr_num, cache_fields[TEST_MODE]);

  if(tr_type == BAD_TR)
    return CDR_ERROR;*/

  norm_id = nslb_get_or_gen_norm_id(&cache_entry_norm_table, cache_fields[TR_NUM], strlen(cache_fields[TR_NUM]), NULL);
  if(norm_id >= max_cache_entry) {
    int new_alloc_size = max_cache_entry + DELTA_REALLOC_SIZE;
    CDR_REALLOC(cdr_cache_entry_list, new_alloc_size * sizeof(struct cdr_cache_entry), "cdr_cache_entry_list");

    memset(&(cdr_cache_entry_list[max_cache_entry]), 0, DELTA_REALLOC_SIZE * sizeof(struct cdr_cache_entry));
    max_cache_entry = new_alloc_size;
  }
  /*Abhi :
  cahnge - summary.top|extra_fields
  Test Run|Scenario Name|Start Time|start time TS|Report Summary|Page Dump|Report Progress|Report Detail|Report User|Report Fail|
  Report Page Break|start_ts| durationin sec|Tr disk| 
  devide this code into two parts summary.top and other fields 
  */

  // summary.top fields
  cdr_cache_entry_list[norm_id].start_time_ts = summary_top_time_convert_to_ts(cache_fields[START_TIME]);
  cdr_cache_entry_list[norm_id].tr_num = tr_num;
  strncpy(cdr_cache_entry_list[norm_id].scenario_name, cache_fields[SCEN_NAME], CDR_FILE_PATH_SIZE);
  strncpy(cdr_cache_entry_list[norm_id].start_time, cache_fields[START_TIME], CDR_BUFFER_SIZE);
  cdr_cache_entry_list[norm_id].report_summary = cache_fields[REPORT_SUMMARY][0];
  cdr_cache_entry_list[norm_id].page_dump = cache_fields[PAGE_DUMP][0];
  strncpy(cdr_cache_entry_list[norm_id].report_progress, cache_fields[REPORT_PROGRESS], CDR_BUFFER_SIZE);
  cdr_cache_entry_list[norm_id].report_detail = cache_fields[REPORT_DETAIL][0];
  cdr_cache_entry_list[norm_id].report_user = cache_fields[REPORT_USER][0];
  cdr_cache_entry_list[norm_id].report_fail = cache_fields[REPORT_FAIL][0];
  cdr_cache_entry_list[norm_id].report_page_break_down = cache_fields[REPORT_PAGE_BREAK_DOWN][0];
  cdr_cache_entry_list[norm_id].wan_env = atoi(cache_fields[WAN_ENV]);
  cdr_cache_entry_list[norm_id].reporting = atoi(cache_fields[REPORTING]);
  strncpy(cdr_cache_entry_list[norm_id].test_name, cache_fields[TEST_NAME], CDR_BUFFER_SIZE);
  strncpy(cdr_cache_entry_list[norm_id].test_mode, cache_fields[TEST_MODE], 64);
  //cdr_cache_entry_list[norm_id].runtime = convert_to_secs(cache_fields[RUNTIME]);
  snprintf(cdr_cache_entry_list[norm_id].runtime, CDR_BUFFER_SIZE, "%s",cache_fields[RUNTIME]);
  cdr_cache_entry_list[norm_id].vusers = atoi(cache_fields[VUSERS]);

  // extra fields
  cdr_cache_entry_list[norm_id].end_time_stamp = atoll(cache_fields[END_TIME_TS]);  
  cdr_cache_entry_list[norm_id].lmd_summary_top = atoll(cache_fields[LMD_SUMMARY_TOP]);

  cdr_cache_entry_list[norm_id].tr_type = atoi(cache_fields[TR_TYPE]);
  if (cdr_cache_entry_list[norm_id].tr_type == CMT_TR)
    cmt_tr_cache_idx = norm_id; // to read the cache index

  cdr_cache_entry_list[norm_id].tr_disk_size = atoll(cache_fields[TR_DISK_SIZE]);
  cdr_cache_entry_list[norm_id].graph_data_size = atoll(cache_fields[GRAPH_DATA_SIZE]);
  cdr_cache_entry_list[norm_id].csv_size = atoll(cache_fields[CSV_SIZE]);
  cdr_cache_entry_list[norm_id].raw_file_size = atoll(cache_fields[RAW_FILE_SIZE]);
  cdr_cache_entry_list[norm_id].tr_db_table_size = atoll(cache_fields[TR_DB_TABLE_SIZE]);
  cdr_cache_entry_list[norm_id].tr_db_index_size = atoll(cache_fields[TR_DB_INDEX_SIZE]);
  cdr_cache_entry_list[norm_id].key_file_size = atoll(cache_fields[KEY_FILE_SIZE]);
  cdr_cache_entry_list[norm_id].har_file_size = atoll(cache_fields[HAR_FILE_SIZE]);
  cdr_cache_entry_list[norm_id].page_dump_size = atoll(cache_fields[PAGE_DUMP_SIZE]);
  cdr_cache_entry_list[norm_id].logs_size = atoll(cache_fields[LOGS_SIZE]);
  cdr_cache_entry_list[norm_id].test_data_size = atoll(cache_fields[TEST_DATA_SIZE]);
  cdr_cache_entry_list[norm_id].reports_size = atoll(cache_fields[REPORTS_SIZE]);
  cdr_cache_entry_list[norm_id].configs_size = atoll(cache_fields[CONFIGS_SIZE]);

  cdr_cache_entry_list[norm_id].remove_tr_f = atoi(cache_fields[REMOVE_TR_F]);
  cdr_cache_entry_list[norm_id].graph_data_remove_f = atoi(cache_fields[GRAPH_DATA_REMOVE_F]);
  cdr_cache_entry_list[norm_id].csv_remove_f = atoi(cache_fields[CSV_REMOVE_F]);
  cdr_cache_entry_list[norm_id].raw_file_remove_f = atoi(cache_fields[RAW_FILE_REMOVE_F]);
  cdr_cache_entry_list[norm_id].tr_db_remove_f = atoi(cache_fields[TR_DB_REMOVE_F]);
  cdr_cache_entry_list[norm_id].key_file_remove_f = atoi(cache_fields[KEY_FILE_REMOVE_F]);
  cdr_cache_entry_list[norm_id].har_file_remove_f = atoi(cache_fields[HAR_FILE_REMOVE_F]);
  cdr_cache_entry_list[norm_id].page_dump_remove_f = atoi(cache_fields[PAGE_DUMP_REMOVE_F]);
  cdr_cache_entry_list[norm_id].logs_remove_f = atoi(cache_fields[LOGS_REMOVE_F]);
  cdr_cache_entry_list[norm_id].test_data_remove_f = atoi(cache_fields[TEST_DATA_REMOVE_F]);
  cdr_cache_entry_list[norm_id].reports_remove_f = atoi(cache_fields[REPORTS_REMOVE_F]);
  cdr_cache_entry_list[norm_id].configs_remove_f = atoi(cache_fields[CONFIGS_REMOVE_F]);

  if(present_flag)
    cdr_cache_entry_list[norm_id].is_tr_present = CDR_TRUE;
  else
    cdr_cache_entry_list[norm_id].is_tr_present = CDR_FALSE;

  total_cache_entry++;
  CDRTL3("Cache entry added for tr: %s", cache_fields[TR_NUM]);

  //Abhi Print complete cache on level 4
  CDRTL2("Method exit");
  return norm_id;
}

/*********************************************************************************************
 * Name: cdr_read_file
 *    This function will read the cache file line by line and
 *    Build the cdr_cache_entry_list
 *    returns the number of entry present in cache file
 *    returns 0 if cache file is empty
 *    returns -1 if fail
 *********************************************************************************************/
int cdr_read_cache_file(char present_flag)
{
  char ret_val;
  char flag = 1;

  CDRTL2("Method called");
  if(rebuild_cache == CDR_TRUE)
    return 0;
  
  ret_val=read_file(cache_file_path, flag, present_flag);
  
  if(ret_val != 0)
  {
    CDRTL1("Error: Error in reading cache file");
    return CDR_ERROR; 
  }
  else
  {
    CDRTL3("cdr_cache_entry_list created");
    CDRTL2("Method exit");
    return CDR_SUCCESS; 
  }
}

/********************************************************************************************
 * Name: get_cache_entry
 *    get the cache entry of tr_num
 *    if tr is not present in cdr_cache_entry_list, it will return NULL
 *    if present, return the pointer to the location of cache entry in cdr_cache_entry_list
 ********************************************************************************************/
struct cdr_cache_entry *get_cache_entry(int tr_num)
{
  int len;
  int norm_id;
  char tr_num_str[16];

  CDRTL2("Method called. tr_num: %d", tr_num);
  
  len = snprintf(tr_num_str, 16, "%d", tr_num);

  norm_id = nslb_get_norm_id(&cache_entry_norm_table, tr_num_str, len);
  if(norm_id < 0) {
    CDRTL3("TR not present in cdr_cache_entry_list. tr_num: %d", tr_num);
    return NULL;
  }

  CDRTL3("Found TR: [%d] at index: [%d]", tr_num, norm_id);
  CDRTL2("Method exit");
  return &(cdr_cache_entry_list[norm_id]);
}


void cdr_dump_cache_to_file()
{
  FILE *fp;

  CDRTL2("Method called, total_cache_entry '%d'", total_cache_entry);
  CDRTL3("Dumping cache to file: %s", cache_file_path);

  // Opening file in truncagte mode
  fp = fopen(cache_file_path, "w");
  if(!fp) {
    CDRTL1("Error: cannot open cache file: %s, error: %s", cache_file_path, strerror(errno));
    return;
  }
  fprintf(fp, "#tr_num| scenario_name| start_time| report_summary| page_dump| report_progress| "
              "report_detail| report_user| report_fail| report_page_break_down| wan_env| reporting| "
              "test_name| test_mode| runtime| vusers| end_time_stamp| lmd_summary_top| tr_type| "
              "tr_disk_size| graph_data_size| csv_size| raw_file_size| tr_db_table_size| tr_db_index_size| "
              "key_file_size| har_file_size| page_dump_size| logs_size| test_data_size| reports_size| config_size| remove_tr_f| "
              "graph_data_remove_f| csv_remove_f| raw_file_remove_f| tr_db_remove_f| key_file_remove_f| "
              "har_file_remove_f| page_dump_remove_f| logs_remove_f| test_data_remove_f| reports_remove_f| config_remove_f\n");

  for(int i = 0; i < total_cache_entry; ++i) {
    if(cdr_cache_entry_list[i].is_tr_present == CDR_TRUE){
      fprintf(fp, "%d|%s|%s|%c|%c|"  "%s|%c|%c|%c|%c|" "%d|%d|%s|%s|%s|" "%d|%lld|%lld|%d|%lld|"  
                  "%lld|%lld|%lld|%lld|%lld|" "%lld|%lld|%lld|%lld|%lld|%lld|%lld|"  "%lld|%lld|%lld|%lld|%lld|" "%lld|%lld|%lld|%lld|%lld|%lld|%lld\n",
        cdr_cache_entry_list[i].tr_num,
        cdr_cache_entry_list[i].scenario_name,
        cdr_cache_entry_list[i].start_time,
        cdr_cache_entry_list[i].report_summary,
        cdr_cache_entry_list[i].page_dump,

        cdr_cache_entry_list[i].report_progress,
        cdr_cache_entry_list[i].report_detail,
        cdr_cache_entry_list[i].report_user,
        cdr_cache_entry_list[i].report_fail,
        cdr_cache_entry_list[i].report_page_break_down,

        cdr_cache_entry_list[i].wan_env,
        cdr_cache_entry_list[i].reporting,
        cdr_cache_entry_list[i].test_name,
        cdr_cache_entry_list[i].test_mode,
        cdr_cache_entry_list[i].runtime,

        cdr_cache_entry_list[i].vusers,
        cdr_cache_entry_list[i].end_time_stamp,
        cdr_cache_entry_list[i].lmd_summary_top,
        cdr_cache_entry_list[i].tr_type,
        cdr_cache_entry_list[i].tr_disk_size,

        cdr_cache_entry_list[i].graph_data_size,
        cdr_cache_entry_list[i].csv_size,
        cdr_cache_entry_list[i].raw_file_size,
        cdr_cache_entry_list[i].tr_db_table_size,
        cdr_cache_entry_list[i].tr_db_index_size,
   
        cdr_cache_entry_list[i].key_file_size,
        cdr_cache_entry_list[i].har_file_size,
        cdr_cache_entry_list[i].page_dump_size,
        cdr_cache_entry_list[i].logs_size,
        cdr_cache_entry_list[i].test_data_size,
        cdr_cache_entry_list[i].reports_size,  
        cdr_cache_entry_list[i].configs_size,
  
        cdr_cache_entry_list[i].remove_tr_f,
        cdr_cache_entry_list[i].graph_data_remove_f,
        cdr_cache_entry_list[i].csv_remove_f,
        cdr_cache_entry_list[i].raw_file_remove_f,
        cdr_cache_entry_list[i].tr_db_remove_f,

        cdr_cache_entry_list[i].key_file_remove_f,
        cdr_cache_entry_list[i].har_file_remove_f,
        cdr_cache_entry_list[i].page_dump_remove_f,
        cdr_cache_entry_list[i].logs_remove_f,
        cdr_cache_entry_list[i].reports_remove_f,
        cdr_cache_entry_list[i].test_data_remove_f,
        cdr_cache_entry_list[i].configs_remove_f);
      }
  }
  fclose(fp);
  CDRTL2("Method exit");
}


int cmt_cache_entry_add(char *cache_line_buff, char present_flag)
{
  /* Purpose: This function will take a cache line and add to cache_entry_list.
   */
  char *cache_fields[TOTAL_CACHE_FIELDS];
  int n_fields;
  int norm_id;

  CDRTL2("Method called. cache_line: %s", cache_line_buff);

  if(cache_line_buff[0] == '#')
    return 0;

  n_fields = get_tokens(cache_line_buff, cache_fields, "|", TOTAL_CACHE_FIELDS);
  if(n_fields != TOTAL_CMT_CACHE_FIELDS) { 
    CDRTL1("Error: Bad cache buff, n_fields: %d", n_fields);
      return CDR_ERROR;
  }

  norm_id = nslb_get_or_gen_norm_id(&cmt_cache_entry_norm_table, cache_fields[CMT_PARTITION_NUM], 
                                      14 /*strlen(cache_fields[PARTITION_NUM], 20200610163802)*/, NULL);
  if(norm_id >= max_cmt_cache_entry) {
    int new_alloc_size = max_cmt_cache_entry + DELTA_REALLOC_SIZE;
    CDR_REALLOC(cdr_cmt_cache_entry_list, new_alloc_size * sizeof(struct cdr_cmt_cache_entry), "cdr_cmt_cache_entry_list");
    memset(&(cdr_cmt_cache_entry_list[max_cmt_cache_entry]), 0, DELTA_REALLOC_SIZE * sizeof(struct cdr_cmt_cache_entry));
    max_cmt_cache_entry = new_alloc_size;
  }

  cdr_cmt_cache_entry_list[norm_id].partition_num = atoll(cache_fields[CMT_PARTITION_NUM]);
  cdr_cmt_cache_entry_list[norm_id].partition_type = atoll(cache_fields[CMT_PARTITION_NUM]);

  cdr_cmt_cache_entry_list[norm_id].partition_disk_size = atoll(cache_fields[CMT_PARTITION_DISK_SIZE]);
  cdr_cmt_cache_entry_list[norm_id].partition_graph_data_size = atoll(cache_fields[CMT_PARTITION_GRAPH_DATA_SIZE]);
  cdr_cmt_cache_entry_list[norm_id].partition_csv_size = atoll(cache_fields[CMT_PARTITION_CSV_SIZE]);
  cdr_cmt_cache_entry_list[norm_id].partition_raw_file_size = atoll(cache_fields[CMT_PARTITION_RAW_FILE_SIZE]);
  cdr_cmt_cache_entry_list[norm_id].partition_db_table_size = atoll(cache_fields[CMT_PARTITION_DB_TABLE_SIZE]);
  cdr_cmt_cache_entry_list[norm_id].partition_db_index_size = atoll(cache_fields[CMT_PARTITION_DB_INDEX_SIZE]);
  cdr_cmt_cache_entry_list[norm_id].partition_har_file_size = atoll(cache_fields[CMT_PARTITION_HAR_FILE_SIZE]);
  cdr_cmt_cache_entry_list[norm_id].partition_page_dump_size = atoll(cache_fields[CMT_PARTITION_PAGE_DUMP_SIZE]);
  cdr_cmt_cache_entry_list[norm_id].partition_logs_size = atoll(cache_fields[CMT_PARTITION_LOGS_SIZE]);
  cdr_cmt_cache_entry_list[norm_id].partition_reports_size = atoll(cache_fields[CMT_PARTITION_REPORTS_SIZE]);  

  cdr_cmt_cache_entry_list[norm_id].partition_graph_data_remove_f = atoi(cache_fields[CMT_PARTITION_GRAPH_DATA_REMOVE_F]);
  cdr_cmt_cache_entry_list[norm_id].partition_csv_remove_f = atoi(cache_fields[CMT_PARTITION_CSV_REMOVE_F]);
  cdr_cmt_cache_entry_list[norm_id].partition_raw_file_remove_f = atoi(cache_fields[CMT_PARTITION_RAW_FILE_REMOVE_F]);
  cdr_cmt_cache_entry_list[norm_id].partition_db_remove_f = atoi(cache_fields[CMT_PARTITION_DB_REMOVE_F]);
  cdr_cmt_cache_entry_list[norm_id].partition_har_file_remove_f = atoi(cache_fields[CMT_PARTITION_HAR_FILE_REMOVE_F]);
  cdr_cmt_cache_entry_list[norm_id].partition_page_dump_remove_f = atoi(cache_fields[CMT_PARTITION_PAGE_DUMP_REMOVE_F]);
  cdr_cmt_cache_entry_list[norm_id].partition_logs_remove_f = atoi(cache_fields[CMT_PARTITION_LOGS_REMOVE_F]);
  cdr_cmt_cache_entry_list[norm_id].partition_reports_remove_f = atoi(cache_fields[CMT_PARTITION_REPORTS_REMOVE_F]);  

  if(present_flag)
    cdr_cmt_cache_entry_list[norm_id].is_partition_present = CDR_TRUE;
  else
    cdr_cmt_cache_entry_list[norm_id].is_partition_present = CDR_FALSE;
  
  total_cmt_cache_entry++;
  CDRTL3("Cache entry added for Partition: '%s'", cache_fields[CMT_PARTITION_NUM]);

  //Abhi Print complete cache on level 4
  CDRTL2("Method exit");
  return norm_id;
}

/*********************************************************************************************
 * Name: ccdr_read_cmt_tr_cache_file
 *    This function will read the cmt tr cache file line by line and
 *    Build the cdr_cache_entry_list
 *    returns the number of entry present in cache file
 *    returns 0 if cache file is empty
 *    returns -1 if fail
 *********************************************************************************************/
int cdr_read_cmt_cache_file(char present_flag)
{
  char ret_val;   
  char flag = 2;  
  CDRTL2("Method called");
  if(rebuild_cache == CDR_TRUE)
    return 0;
 
  ret_val = read_file(cmt_cache_file_path,flag, present_flag);
 
  if(ret_val != 0)
  {
    CDRTL1("Error: Error in reading cache file");
    return CDR_ERROR;
  }
  else
  {
    CDRTL3("cdr_cache_entry_list created");
    CDRTL2("Method exit");
    return CDR_SUCCESS;
  }
}

int cmt_cache_entry_add_from_partition(long long int partition_num, char *partition_num_str, int norm_id, int cur_idx)
{
  /* Purpose: This function will take a cache line and add to cache_entry_list.
   */

  CDRTL2("Method called. partition num '%lld'", partition_num);

  char partition_type = get_partition_type(cdr_cache_entry_list[cmt_tr_cache_idx].tr_num, partition_num);

  if(partition_type == BAD_PARTITION)
    return CDR_ERROR;  

  if (norm_id < 0)
  {
    norm_id = nslb_get_or_gen_norm_id(&cmt_cache_entry_norm_table, partition_num_str, strlen(partition_num_str), NULL);
    if(norm_id >= max_cmt_cache_entry) {
      int new_alloc_size = max_cmt_cache_entry + DELTA_REALLOC_SIZE;
      CDR_REALLOC(cdr_cmt_cache_entry_list, new_alloc_size * sizeof(struct cdr_cmt_cache_entry), "cdr_cmt_cache_entry_list");
      memset(&(cdr_cmt_cache_entry_list[max_cmt_cache_entry]), 0, DELTA_REALLOC_SIZE * sizeof(struct cdr_cmt_cache_entry));
      max_cmt_cache_entry = new_alloc_size;
    }
  }

  /* Fomrmat CMT cache file
  Partion Number | Partition Disk size | Partition DB table size | Partition DB index size | Partion Metric Percentile Size | Partition har file size| Partition csv size | Partition page dump size | Partiton db size | Partiton logs_size | Partiton raw_file_size
  */
  
  CDRTL3("Cache entry added for Partitoin num: '%s'", partition_num_str);

  cdr_cmt_cache_entry_list[norm_id].partition_num = partition_num;

  cdr_cmt_cache_entry_list[norm_id].partition_disk_size = get_partition_disk_size(norm_id);
  cdr_cmt_cache_entry_list[norm_id].partition_graph_data_size = get_partition_graph_data_size(norm_id); 
  cdr_cmt_cache_entry_list[norm_id].partition_csv_size = get_partition_csv_size(norm_id); 

  double size = 0.0, size_with_index = 0.0;
  partiton_tableSize_with_indexes_size (cdr_cmt_cache_entry_list[norm_id].partition_num, &size, &size_with_index);

  cdr_cmt_cache_entry_list[norm_id].partition_db_table_size = size;
  cdr_cmt_cache_entry_list[norm_id].partition_db_index_size = size_with_index; 
  cdr_cmt_cache_entry_list[norm_id].partition_db_table_size += get_partition_db_file_size(norm_id);
  cdr_cmt_cache_entry_list[norm_id].partition_raw_file_size = get_partition_raw_files_size(norm_id);
  cdr_cmt_cache_entry_list[norm_id].partition_har_file_size = get_partition_har_file_size(norm_id);
  cdr_cmt_cache_entry_list[norm_id].partition_page_dump_size = get_partition_page_dump_size(norm_id); 
  cdr_cmt_cache_entry_list[norm_id].partition_logs_size = get_partition_logs_size(norm_id); 
  cdr_cmt_cache_entry_list[norm_id].partition_reports_size = get_partition_reports_size(norm_id);
  cdr_cmt_cache_entry_list[norm_id].partition_type = partition_type; 


  cmt_check_ngve_range(norm_id, cur_idx);

  total_cmt_cache_entry++;
  //Abhi Print complete cache on level 4
  CDRTL2("Method exit");
  return norm_id;
}



int nv_cache_entry_add(char *cache_line_buff, char present_flag)
{
  /* Purpose: This function will take a cache line and add to cache_entry_list.
   */
  char *cache_fields[TOTAL_CACHE_FIELDS];
  int n_fields;
  int norm_id;

  CDRTL2("Method called. cache_line: %s", cache_line_buff);

  n_fields = get_tokens(cache_line_buff, cache_fields, "|", TOTAL_CACHE_FIELDS);
  if(n_fields != TOTAL_CACHE_FIELDS) { 
    CDRTL1("Error: Bad cache buff, n_fields: %d", n_fields);
      return CDR_ERROR;
  }

  norm_id = nslb_get_or_gen_norm_id(&nv_cache_entry_norm_table, cache_fields[NV_PARTITION_NUM], 
                                      14 /*strlen(cache_fields[PARTITION_NUM], 20200610163802)*/, NULL);
  if(norm_id >= max_nv_cache_entry) {
    int new_alloc_size =  max_nv_cache_entry + DELTA_REALLOC_SIZE;
    CDR_REALLOC(cdr_nv_cache_entry_list, new_alloc_size * sizeof(struct cdr_nv_cache_entry), "cdr_nv_cache_entry_list");
    memset(&cdr_nv_cache_entry_list[max_nv_cache_entry], 0, DELTA_REALLOC_SIZE * sizeof(struct cdr_nv_cache_entry));
    max_nv_cache_entry = new_alloc_size;
  }


  cdr_nv_cache_entry_list[norm_id].partition_num = atoi(cache_fields[NV_PARTITION_NUM]);
  cdr_nv_cache_entry_list[norm_id].partition_type = atoi(cache_fields[NV_PARTITION_TYPE]);

  cdr_nv_cache_entry_list[norm_id].partition_disk_size = atoi(cache_fields[NV_PARTITION_DISK_SIZE]);
  cdr_nv_cache_entry_list[norm_id].partition_db_table_size = atoll(cache_fields[NV_PARTITION_DB_TABLE_SIZE]);
  cdr_nv_cache_entry_list[norm_id].partition_db_index_size = atoll(cache_fields[NV_PARTITION_DB_INDEX_SIZE]);
  cdr_nv_cache_entry_list[norm_id].partition_csv_size = atoll(cache_fields[NV_PARTITION_CSV_SIZE]);
  cdr_nv_cache_entry_list[norm_id].partition_ocx_size = atoll(cache_fields[NV_PARTITION_OCX_SIZE]);
  cdr_nv_cache_entry_list[norm_id].partition_na_traces_size = atoll(cache_fields[NV_PARTITION_NA_TRACES_SIZE]);
  cdr_nv_cache_entry_list[norm_id].partition_access_log_size = atoll(cache_fields[NV_PARTITION_ACCESS_LOG_SIZE]); // TODO :not on partion level, how to handle it?
  cdr_nv_cache_entry_list[norm_id].partition_logs_size = atoll(cache_fields[NV_PARTITION_LOGS_SIZE]); // TODO :At multiple level how to handle it 
  cdr_nv_cache_entry_list[norm_id].partition_db_remove_f = atoi(cache_fields[NV_PARTITION_DB_REMOVE_F]);
  cdr_nv_cache_entry_list[norm_id].partition_csv_remove_f = atoi(cache_fields[NV_PARTITION_CSV_REMOVE_F]);
  cdr_nv_cache_entry_list[norm_id].partition_ocx_remove_f = atoi(cache_fields[NV_PARTITION_OCX_REMOVE_F]);
  cdr_nv_cache_entry_list[norm_id].partition_na_traces_remove_f = atoi(cache_fields[NV_PARTITION_NA_TRACES_REMOVE_F]);
  cdr_nv_cache_entry_list[norm_id].partition_access_log_remove_f = atoi(cache_fields[NV_PARTITION_ACCESS_LOG_REMOVE_F]);
  cdr_nv_cache_entry_list[norm_id].partition_logs_remove_f = atoi(cache_fields[NV_PARTITION_LOGS_REMOVE_F]);
  cdr_nv_cache_entry_list[norm_id].num_proc = atoi(cache_fields[NV_PARTITION_NUM_PROC]);


  if(present_flag)
    cdr_nv_cache_entry_list[norm_id].is_partition_present = CDR_TRUE;
  else
    cdr_nv_cache_entry_list[norm_id].is_partition_present = CDR_FALSE;
  
  total_nv_cache_entry++;
  CDRTL3("Cache entry added for NV Partition: %s", cache_fields[NV_PARTITION_NUM]);

  //Abhi Print complete cache on level 4
  CDRTL2("Method exit");
  return norm_id;
}

/*********************************************************************************************
 * Name: ccdr_read_cmt_tr_cache_file
 *    This function will read the cmt tr cache file line by line and
 *    Build the cdr_cache_entry_list
 *    returns the number of entry present in cache file
 *    returns 0 if cache file is empty
 *    returns -1 if fail
 *********************************************************************************************/
int cdr_read_nv_cache_file(char present_flag)
{
  char ret_val;
  char flag = 3;
  CDRTL2("Method called");
  if(rebuild_cache == CDR_TRUE)
    return 0;
  
  ret_val = read_file(nv_cache_file_path,flag, present_flag);

  if(ret_val != 0)
  {
    CDRTL1("Error: Error in reading cache file");
    return CDR_ERROR;
  }
  else
  {
    CDRTL3("cdr_cache_entry_list created");
    CDRTL2("Method exit");
    return CDR_SUCCESS;
  }
}

int nv_cache_entry_add_overall(char *nv_client_id)
{
  CDRTL2("Method called. nv_client_id '%d'", nv_client_id);
  char partition_num_str[16] = "overall";
  int norm_id = nslb_get_or_gen_norm_id(&nv_cache_entry_norm_table, partition_num_str, strlen(partition_num_str), NULL);
  if(norm_id >= max_nv_cache_entry) {
    int new_alloc_size = max_nv_cache_entry + DELTA_REALLOC_SIZE;
    CDR_REALLOC(cdr_nv_cache_entry_list, new_alloc_size * sizeof(struct cdr_nv_cache_entry), "cdr_nv_cache_entry_list");
    memset(&(cdr_nv_cache_entry_list[max_nv_cache_entry]), 0, DELTA_REALLOC_SIZE * sizeof(struct cdr_nv_cache_entry));
    max_nv_cache_entry = new_alloc_size;
  }
  
  if (!cdr_nv_cache_entry_list[norm_id].nv_client_id) {
    CDR_MALLOC(cdr_nv_cache_entry_list[norm_id].nv_client_id, strlen(nv_client_id) + 1, "nv_client_id");
    sprintf(cdr_nv_cache_entry_list[norm_id].nv_client_id, "%s", nv_client_id);
  }

  
  cdr_nv_cache_entry_list[norm_id].partition_type = 0; 

  cdr_nv_cache_entry_list[norm_id].partition_disk_size = 0; 
  cdr_nv_cache_entry_list[norm_id].partition_db_table_size = 0; 
  cdr_nv_cache_entry_list[norm_id].partition_db_index_size = 0; 
  cdr_nv_cache_entry_list[norm_id].partition_csv_size = 0; 
  cdr_nv_cache_entry_list[norm_id].partition_ocx_size = 0; 
  cdr_nv_cache_entry_list[norm_id].partition_na_traces_size = 0; 
  cdr_nv_cache_entry_list[norm_id].partition_access_log_size = get_nv_access_log_size(norm_id);; 
  cdr_nv_cache_entry_list[norm_id].partition_logs_size = get_nv_logs_size(norm_id);; 
  cdr_nv_cache_entry_list[norm_id].num_proc = 0; 
  return TRUE;
}

int nv_cache_entry_add_from_partition(long long int partition_num, char *partition_num_str, int norm_id, int cur_idx)
{
  /* Purpose: This function will take a cache line and add to cache_entry_list.
   */

  CDRTL2("Method called. partition num '%lld'", partition_num);

  char partition_type = get_nv_partition_type(partition_num);

  if(partition_type == BAD_PARTITION)
    return CDR_ERROR;


  if (norm_id < 0)
  {
    norm_id = nslb_get_or_gen_norm_id(&nv_cache_entry_norm_table, partition_num_str, strlen(partition_num_str), NULL);
    if(norm_id >= max_nv_cache_entry) {
      int new_alloc_size = max_nv_cache_entry + DELTA_REALLOC_SIZE;
      CDR_REALLOC(cdr_nv_cache_entry_list, new_alloc_size * sizeof(struct cdr_nv_cache_entry), "cdr_nv_cache_entry_list");
      memset(&(cdr_nv_cache_entry_list[max_nv_cache_entry]), 0, DELTA_REALLOC_SIZE * sizeof(struct cdr_nv_cache_entry));
      max_nv_cache_entry = new_alloc_size;
    }
  }

  /* Fomrmat CMT cache file
  Partion Number | Partition Disk size | Partition DB table size | Partition DB index size | Partion Metric Percentile Size | Partition har file size| Partition csv size | Partition page dump size | Partiton db size | Partiton logs_size | Partiton raw_file_size
  */
  
  CDRTL3("Cache entry added for NV Partitoin num: '%s'", partition_num_str);

  cdr_nv_cache_entry_list[norm_id].partition_num = partition_num;
  cdr_nv_cache_entry_list[norm_id].partition_type = partition_type; 

  double size = 0.0, size_with_index = 0.0;
  partiton_tableSize_with_indexes_size (cdr_nv_cache_entry_list[norm_id].partition_num, &size, &size_with_index);

  cdr_nv_cache_entry_list[norm_id].partition_disk_size = size; 
  cdr_nv_cache_entry_list[norm_id].partition_db_table_size = size_with_index; 
  cdr_nv_cache_entry_list[norm_id].partition_db_index_size = get_nv_partition_db_table_size(norm_id); 
  cdr_nv_cache_entry_list[norm_id].partition_csv_size = get_nv_partition_csv_size(norm_id); 
  cdr_nv_cache_entry_list[norm_id].partition_ocx_size = get_nv_partition_ocx_size(norm_id); 
  cdr_nv_cache_entry_list[norm_id].partition_na_traces_size = get_nv_partition_na_traces_size(norm_id);
  cdr_nv_cache_entry_list[norm_id].partition_access_log_size = get_nv_partition_access_log_size(norm_id);
  cdr_nv_cache_entry_list[norm_id].partition_logs_size = get_nv_partition_logs_size(norm_id);
  cdr_nv_cache_entry_list[norm_id].num_proc = get_nv_partition_num_proc(partition_num);

  nv_check_ngve_range(norm_id, cur_idx);

  total_nv_cache_entry++;
  //Abhi Print complete cache on level 4
  CDRTL2("Method exit");
  return norm_id;
}
