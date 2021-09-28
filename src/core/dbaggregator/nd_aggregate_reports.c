/************************************************************************
Name     :    Aggregator Engine                                         *
Purpose  :    Aggregate the raw data                                    *
Author   :                                                              *
*************************************************************************/
#define _GNU_SOURCE 
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include "libpq-fe.h"
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
#include <sys/ipc.h>
#include <sys/shm.h>
#include <dlfcn.h>
#include <sys/prctl.h>
#include <ctype.h>
#include <assert.h>
#include "../../libnscore/nslb_partition.h"
#include "../../libnscore/nslb_db_util.h"
#include "../../libnscore/nslb_signal.h"
#include "../../libnscore/nslb_multi_thread_trace_log.h"
#include "../../libnscore/nslb_partition.h"
#include "../../libnscore/nslb_util.h"
#include "../../libnscore/nslb_sock.h"
#include "../../libnscore/nslb_get_norm_obj_id.h"
#include "../../libnscore/nslb_db_util.h"
#include "../../libnscore/nslb_comp_recovery.h"
#include "nd_aggregate_reports.h"
#include <sys/types.h>
#include <sys/wait.h>
#include "nslb_util.h"

#define ND_AGG_RESPAWN_RETRY_COUNT 5
#define ND_AGG_RESPAWN_RETRY_TOT_DURATION_SEC 10
#define ND_AGG_RESPAWN_RESTART_RETRIES_AFTER_SEC 900  //after 15min. 

#define ND_PARSER_CALLBACK_NAME "nd_agg_parser"
#define ND_MERGE_AGG_CALLBACK_NAME "nd_merge_agg_record"
#define ND_PARSE_KEYWORDS_CALLBACK "nd_parse_profile_keywords"

// ************ parser running mode *****************
#define OFFLINE 2
#define ONLINE 1
#define DISABLE 0

//*************** Test State ***********************
#define COMPLETED 0
#define ACTIVE 1
// ************* offline mode **********************
#define MAX_BUCKET_COUNT 10
#define MAX_SCHEDULAR_ENTRY 100

#define STANDALONE_MODE 1
#define MULTI_INSTANCE_MODE 2

#define PARENT() (getpid() == g_parent_pid) 
#define NV_NORMALIZE_TABLE_DEFAULT_SIZE 1024
#define DELTA_AGGREGATE_ENTRIES 32
#define MAX_READ_BUF 1024

#define FREE_AGG_RESULTSETS(resultSet, count) \
{  \
  if(count > 0) \
  { \
    int z; \
    for(z = 0; z < count; z++) \
    { \
      free_nv_result_set(&resultSet[z]); \
    } \
  }  \
}

#define NV_CHILD_SLEEP(time)\
{\
  int t = time; \
  while(t-- > 0)  \
  {\
    sleep(1);\
    if(g_parent_process_pid != getppid())\
    {\
      ND_LOG1("g_parent_process_pid %d is not same as %d, hence killing\n", g_parent_process_pid, getppid());\
      exit(-1);\
    }\
  }\
}

static short g_aggregator_running_mode = 1; //default running

static unsigned long g_latest_record_time = 0;
static unsigned long g_first_record_time = 0;
static unsigned long g_last_partitionid = -1;
unsigned long g_prev_latest_record_time;
unsigned long g_last_process_time = 0;
unsigned long g_cav_epoch_diff;
unsigned long long g_partitionid;
long g_start_time;
long g_end_time;

//keywords for db_server_name
PGconn *remote_db_connection = NULL;

aggregate_profile *g_aggregate_profile_ptr = NULL;

static int max_aggregate_entries = 0;
static int total_agggregate_entries = 0;
static int g_partition_duration = 8*3600;
int g_max_session_duration = 300;
int g_child_id;
int rnum;
int gTestRunNo = -1;
int g_parent_pid;
int g_parent_process_pid;
int gTotalOffBucket = 0;
int totAggTask = 0;
int g_rum_cav_epoch_year;
int maxActiveAggBucket;
int totalActiveAggBucket;
int g_trace_level = 1;
int g_trace_log_file_size = 10;//In mb.
int test_run_info_shm_id = -1;


char g_rum_base_dir[512];
static char *connecton_info = "dbname=test user=netstorm";
char docrootPrefix[1024] = "";
char g_lpt_file[512];
char dir[512];
//default is 8hr.
static char gnswdir[256] = "";
char gcronString[256] = "";
short consider_all_parser_in_offline_mode = 0;

CurPartInfo cur_part_info;
SchedularProfile gSchedularInfo[MAX_SCHEDULAR_ENTRY];
AggOfflineInfo gOfflineParserInfo[MAX_BUCKET_COUNT];

typedef struct {
  long timestamp;
  int id;
} SortHelperStruct;

//just for testing active case.. 
static NVActiveAggBucket *activeAggBucket;
//For Standalone.
char g_trace_file[512] = "";
char g_aggregate_conf_file[1024];
int g_processing_mode = MULTI_INSTANCE_MODE; 


//remove it
int gTotalComponent ;
/**********************************************************************
Fucntion to create aggregate table entry                              *
**********************************************************************/
int create_aggregate_table_entry(int* row_num) {
  if (total_agggregate_entries == max_aggregate_entries) {
    MY_REALLOC(g_aggregate_profile_ptr, (max_aggregate_entries + DELTA_AGGREGATE_ENTRIES) * sizeof(aggregate_profile),
      NULL, "aggregate_table");
    if (!g_aggregate_profile_ptr) {
      fprintf(stderr, "create_aggregate_table_entry(): Error allocating more memory for aggregate entries\n");
      ND_LOG4("create_aggregate_table_entry(): Error allocating more memory for aggregate entries\n");
      return (0);
    } else max_aggregate_entries += DELTA_AGGREGATE_ENTRIES;
  }
  *row_num = total_agggregate_entries++;
   ND_LOG3("create_aggregate_table_entry(): rownum =%d\n", *row_num);
  return (1);
}


/*  This function finds difference between cavmon epoch and unix epoch
 *  Input:  cav_epoch_year
 *  Output: difference between cavmon epoch and unix epoch in seconds.
 *
 *      01/01/70(IST)   01/01/70(GMT)                             01/01/14(IST)           01/01/14(GMT)
 *  --------|----------------|------------------------------------------|----------------|----------------
 *          <-----5.5hr------>                                          <-----5.5hr------>
 *
 *  We need to find out the difference between either 01/01/70(IST) and 01/01/14(IST);
 *  or between 01/01/70(GMT) and 01/01/14(GMT).
 *  Here we are calculating difference between 01/01/70(IST) and 01/01/14(IST).
 * 
 *  First we get difference between 01/01/70(IST) and 01/01/70(GMT)
 *  then difference between 01/01/70(GMT) and 01/01/14(IST).
 *  then we subtract second value from first one as 5.5 hrs will be negative in case of example shown.
 *
 */

/***********************************************************
function to get unix cav epoch diff                        *
***********************************************************/
long get_unix_cav_epoch_diff(int cav_epoch_year)
{
  int unix_epoch_year = 1970;
  struct tm t;
  long gmt_to_localtime_diff, cav_epoch_diff, unix_cav_epoch_diff;

  /*  Filling time 00:00:00 and date 01/01/YYYY */
  t.tm_isdst = -1;  //for day light saving
  t.tm_mon = 0;
  t.tm_mday = 1;
  t.tm_hour = 0;
  t.tm_min = 0;
  t.tm_sec = 0;

  /*  mktime() method takes year as year passed since 1900 */
  t.tm_year = unix_epoch_year - 1900;
  gmt_to_localtime_diff = (long)mktime(&t); //diff between localtime and gmt time.

  t.tm_year = cav_epoch_year - 1900;
  cav_epoch_diff = (long)mktime(&t);    //diff between cav_epoch(localtime) and unix_epoch(gmt)

  if(gmt_to_localtime_diff == -1 || cav_epoch_diff == -1)
  {
    fprintf(stderr, "Unable to get unix_cav_epoch_diff for cavmon year: %d\n", cav_epoch_year);
    exit(-1);
  }
  /*  diff between cav_epoch(localtime) and unix_epoch(localtime) */
  unix_cav_epoch_diff = cav_epoch_diff - gmt_to_localtime_diff;
  return unix_cav_epoch_diff;
}

/*****************************
set rum directory path       *
*****************************/
void set_rum_dir_path()
{
  ND_LOG1("Method Called\n");
  sprintf(g_rum_base_dir, "%s/logs/TR%d", gnswdir, gTestRunNo);
}

/****************************
filter files for aprof file *
****************************/
int file_filter(const struct dirent *a)
{
  int len = strlen(a->d_name) - 4;
  if(len < 0) len = 0;
  if((a->d_type == DT_REG) && (strstr(a->d_name, ".aprof")))
  {
    return 1;
  }
  return 0;
}

/************************************************************
parse and replace value (startime, endtime, testrunno       *
*************************************************************/
void parse_and_replace_value(char *in, char *out_buffer, int testrunno, long starttime, long endtime)
{
  char *ptr = in;
  char *tmp;
  char *out = out_buffer;
  char out1[10];
  ND_LOG4("Method called, input buffer - %s, out_buffer - %s, starttime - %ld, endtime - %ld, testrunno - %d", in, out_buffer, starttime, endtime, testrunno);
  while(ptr)
  {
    //copy from $ and then check if need to replace then replace by actual data and then move to next.
    tmp = strchr(ptr, '$');
    //copy data.
    if(tmp)
    {
      strncpy(out, ptr, (tmp - ptr));
      ND_LOG1("%s\n", out);
      out[tmp - ptr] = 0;
      //update out.
      out += (tmp - ptr);
    }
    else
    {
      strcpy(out, ptr);
      return;
    }
    ptr = tmp;
    if(tmp)
    {
      if(!strncasecmp(ptr, "$TRNO", 5))
      {
        sprintf(out1, "%d", testrunno);
        strcat(out,out1);
	out +=strlen(out1);
        ND_LOG4("OUT %s\n", out);
        ptr += 5;
        ND_LOG4("TESTRUNO %s\n", ptr);
      }
      else if(!strncasecmp(ptr, "$STARTTIME", 10))
      {
        sprintf(out1, "%ld", starttime);
        strcat(out,out1);
	out +=strlen(out1);
       // out += sprintf(out, "%ld", starttime);
        ND_LOG4("OUT %s\n", out);
        ptr += 10;
        ND_LOG4("STARATATATTIME %s\n", ptr);
      }
      else if(!strncasecmp(ptr, "$ENDTIME", 8))
      {
        sprintf(out1, "%ld", endtime);
        strcat(out,out1);
	out += strlen(out1);
       // out += sprintf(out, "%ld", endtime);
        ptr += 8;
      }
      else {
        out[0] = '$';
        out++;
        ptr ++;
      }
      *out = 0;
    }
  }

printf("value of out is \t %s", out);
fflush(stdout);
}

/*******************************
create aggregate table         *
*******************************/
void create_aggregate_table()
{
  char cmd[MAX_SQL_QUERY_SIZE] = "";
  //const char *conninfo;
  char table_name[512] = "";
  //int rnum = 0;
  int nrow = 0;
  int i = 0;
  //int j = 0;
  PGresult *res;
  //char *out;
  
  ND_LOG1("Method called");
  for(i = 0; i < g_aggregate_profile_ptr->numAggTable; i++)
  {
    sprintf(table_name,"%s_%d", g_aggregate_profile_ptr->table_name[i], gTestRunNo);
    ND_LOG1("Table name - %s", table_name);

    sprintf(cmd, "select * from information_schema.tables where table_name ~* \'^%s$\';", table_name);
    ND_LOG1("Query to check if table exist - %s", cmd);
    if(execute_db_query(cmd, &res))
    {
      exit(-1);
    }
    nrow = PQntuples(res); 
    //clear result.
    PQclear(res);
    //if nrow is 0.
    if(nrow == 0)
    {
      ND_LOG1("Aggregate table %s not exist creating table", table_name);
      //set parameters value and copy into cmd.
      ND_LOG1("table sql %s\n", g_aggregate_profile_ptr->create_table_sql[i]);
      parse_and_replace_value(g_aggregate_profile_ptr->create_table_sql[i], cmd, gTestRunNo, -1, -1); 
      ND_LOG1("Command for create table %s\n", cmd);

      ND_LOG4("Query to create aggreagta table(%s) - %s", table_name, cmd);
      if(execute_db_query(cmd, NULL))
      {
        exit(-1);
      }
    }
  }
}

#if 0
static long get_partition_idx()
{
  long    tloc;
  struct  tm *lt;
  char buff[128 + 1] = {0};
 
  /*  Getting current time  */ 
  (void)time(&tloc);
  lt = localtime(&tloc);

  if (lt == (struct tm *)NULL)
    return -1;
  else
  {
    /*  Getting time as formatted string  */
    /*  "%Y%m%d%H%MS" returns "YYYYMMDDHHMMSS" format  */
    if(strftime(buff, 128, "%Y%m%d%H%M%S", lt) == 0)
      return -1;
    else
    {
      return(atoll(buff));
    }
  }
  return -1;
}
#endif

/***************************************************
           parser running mode                     *
***************************************************/
static void kw_set_nv_aggregator_mode(char *buff)
{
  char keyword[512];
  char mode[32];
  char tmp[1024];
  int fields;

  char *usages = "Usages: NV_AGGREGATOR_RUN_MODE <mode>\n";

  fields = sscanf(buff, "%s %s %s", keyword, mode, tmp);

  if(fields < 2)
  {
    ND_ERROR("Invalid format of keyword NV_AGGREGATOR_RUN_MODE.\n %s\n", usages);
    return;
  }
  g_aggregator_running_mode = atoi(mode);
 
  ND_LOG1("g_aggregator_running_mode = %d", g_aggregator_running_mode);
}

/********************************************
       OFFLINE Parsing                      *
*********************************************/
void kw_set_nv_aggregate_run_at(char *buff)
{
  ND_LOG("Method Called \n"); 
  char keyword[512];
  char *parserName[32];
  char tmp_buf[1024];
  char c1[4], c2[4], c3[4], c4[4], c5[4], c6[4];
  char cronString[32];
  char tmp[1024];
  int fields, i, num_parser;

  char *usages = "Usages: NV_AGGREGATOR_RUN_AT <parsername> <cron-string>\n";

  fields = sscanf(buff, "%s %s %s %s %s %s %s %s %s", keyword, tmp_buf, c1, c2, c3, c4, c5, c6, tmp);

  if(fields < 7)
  {
    ND_LOG("Invalid format of keyword NV_AGGREGATOR_RUN_AT or cronstring missing-%s\n buff-%s\n fields-%d\n", usages, buff, fields);
    ND_ERROR("Invalid format of keyword NV_AGGREGATOR_RUN_AT or cronstring missing\n %s\n", usages);
    return;
  }

  if(gTotalOffBucket >= MAX_BUCKET_COUNT)
  {
    ND_LOG("Total buckets for offline mode exceed the limit..ignoring sample - %s %s %s\n", keyword, tmp_buf, cronString);  
    ND_ERROR("Total buckets for offline mode exceed the limit..ignoring sample - %s %s %s\n", keyword, tmp_buf, cronString);
    return;
  }

  //case when parsername and cronstring both given.
  if(fields == 8)
  {
     //make cron string.
     sprintf(cronString, "%s %s %s %s %s %s", c1, c2, c3, c4, c5, c6);
     if(strchr(tmp_buf, ',') == NULL)
     {
       ND_LOG("tmp_buf - %s, cronString - %s", tmp_buf, cronString);
       strcpy(gOfflineParserInfo[gTotalOffBucket].parserName, tmp_buf);
       strcpy(gOfflineParserInfo[gTotalOffBucket].cronString, cronString);
       ++gTotalOffBucket;
     }
     else
     {
        num_parser = get_tokens(tmp_buf, parserName, ",", MAX_BUCKET_COUNT);
        for(i = 0; i < num_parser; i++)
        {
          ND_LOG("parsername - %s, cronString - %s", parserName[i], cronString);
          strcpy(gOfflineParserInfo[gTotalOffBucket].parserName, parserName[i]);
          strcpy(gOfflineParserInfo[gTotalOffBucket].cronString, cronString);
          ++gTotalOffBucket;
        }
     }
   }
   //when parsername is not given
   if(fields == 7)
   {
     sprintf(gcronString, "%s %s %s %s %s %s", tmp_buf, c1, c2, c3, c4, c5);
     consider_all_parser_in_offline_mode = 1;
     ND_LOG("cronString - %s", gcronString);
   }
}

/********************************************
Trace level parsing                         *
********************************************/
void kw_set_nv_aggregate_reporting_trace_level(char *buff) 
{
  char keyword[32];
  char value[16];
  char value1[16];
  int num =0;
 
  #define USAGE_NV_TL "Usage : NV_AGG_REPORTING_TRACE_LEVEL <tracing level (1/2/3/4)> <file_size>\n" \

  num = sscanf(buff, "%s %s %s", keyword, value, value1);
  if(num != 3){
    ND_ERROR("There should be only one value after keyword %s", USAGE_NV_TL);
    exit(-1);
  }

  if(ns_is_numeric(value) == 0)
  {
    fprintf(stderr, "Error: Invalid value for keyword in line (%s) is not numeric\n %s", buff, USAGE_NV_TL);
    exit (-1);
  }
  g_trace_level = atoi(value);
  g_trace_log_file_size = atoi(value1);
  ND_LOG1("g_trace_level = %d, g_trace_log_file_size = %d", g_trace_level, g_trace_log_file_size);
}

/****************************************
Get aggregate table partitionid         *
****************************************/
unsigned long get_agg_partition_id(time_t current_time)
{
  ND_LOG1("Method called, current time - %ld", current_time);  
  time_t midnight_time; 
  time_t partition_time;
  struct tm *current_tm;
  struct tm *partition_tm;
  char buff[128];
  
  current_time += g_cav_epoch_diff;

  current_tm = localtime(&current_time); 
  
  //get midnight time.
  current_tm->tm_hour = 0;
  current_tm->tm_min = 0;
  current_tm->tm_sec = 0;
    
  midnight_time = mktime(current_tm);
  
  //get partition time.
  partition_time = (midnight_time + ((int)(current_time - midnight_time)/g_partition_duration)*g_partition_duration);

  //conver into tm structure.
  partition_tm = localtime(&partition_time);

  if(strftime(buff, 128, "%Y%m%d%H%M%S", partition_tm) == 0)
  {
    ND_LOG1("Nearest partition is %s\n", buff);
    return -1;
  }
  else 
  {
    ND_LOG1("Current Time - %lu, partition id - %s", current_time, buff);
    return atoll(buff);
  }

  //Tum kab aaoge.
  return 0;
}

/***************************************************************************
create partition table                                                     *
First time we may have to check if partition table already exist or not.   *
***************************************************************************/
void create_partition_table(time_t current_time, unsigned long partitionid, char check_if_exist)
{
  char cmd[4*1024];
  char partition_table_name[256];
  PGresult *res;
  int i, nrow;
  //char table_space[512] = "";
  //Below two for multi disk support

  //get partition id. 
  if(partitionid == 0)
    g_partitionid = get_agg_partition_id(current_time); 
  else 
    g_partitionid = partitionid;    
 
  ND_LOG1("Method called, partition id - %lu", g_partitionid);

  for(i = 0; i < g_aggregate_profile_ptr->numAggTable; i++)
  {
    sprintf(partition_table_name, "%s_%d_%lld", g_aggregate_profile_ptr->table_name[i], gTestRunNo, g_partitionid);  
    ND_LOG4("Partition table name - %s", partition_table_name); 
    if(check_if_exist)
    {
      sprintf(cmd, "select * from information_schema.tables where table_name ~* \'^%s$\';", partition_table_name);
      ND_LOG4("Command to check if partition table exist - %s", cmd);
      if(execute_db_query(cmd, &res))
      {
        exit(-1);
      } 
      nrow = PQntuples(res); 
      //clear result.
      PQclear(res);
      //if table already exist.
      if(nrow == 1)
      {
        ND_LOG4(" create_partition nrow = %d", nrow);
        //TODO: check if start time constaint added or not. If not then add that.
        continue;
      }
    }
 
    sprintf(cmd, "CREATE TABLE %s (LIKE %s_%d INCLUDING INDEXES) INHERITS (%s_%d)",
                 partition_table_name,
                 g_aggregate_profile_ptr->table_name[i], gTestRunNo, 
                 g_aggregate_profile_ptr->table_name[i], gTestRunNo);
    
    if(execute_db_query(cmd, NULL))
    {
      exit(-1);
    }
  
    //timestamp.
    char partition_str[64];  
    sprintf(partition_str, "%llu", g_partitionid);
    unsigned long timestamp = (unsigned long)nslb_get_time_in_secs(partition_str); 
    timestamp -= g_cav_epoch_diff; 
    
    //Note: each aggregate table should have timestamp column.
    //set starttime constraint.
    sprintf(cmd, "ALTER TABLE %s_%d_%lld ADD CONSTRAINT START_TIME_CONSTRAINT_%s_%d_%lld"
                 " CHECK (timestamp >= %ld)",
                 g_aggregate_profile_ptr->table_name[i], gTestRunNo, g_partitionid,
                 g_aggregate_profile_ptr->table_name[i], gTestRunNo, g_partitionid,
                 timestamp - (3600)/*This overlap time just to handle dst*/);
    if(execute_db_query(cmd, NULL))
    {
      ND_ERROR("Failed to set start constraint for partition table - %s_%d_%lld", 
                 g_aggregate_profile_ptr->table_name[i], gTestRunNo, g_partitionid);
      return; 
    }

    //set endtime constraint. 
    //Note: now the duration of partition are fixed. 
    sprintf(cmd, "ALTER TABLE %s_%d_%lld ADD CONSTRAINT END_TIME_CONSTRAINT_%s_%d_%lld"
                 " CHECK (timestamp <= %ld)",
                 g_aggregate_profile_ptr->table_name[i], gTestRunNo, g_partitionid, 
                 g_aggregate_profile_ptr->table_name[i], gTestRunNo, g_partitionid,
                 (timestamp + g_partition_duration));
    if(execute_db_query(cmd, NULL))
    {
      ND_ERROR("Failed to set start constraint for partition table - %s_%d_%lld", 
                 g_aggregate_profile_ptr->table_name[i], gTestRunNo, g_partitionid);
      return; 
    }
  }
}

/**************************************************
//this is time upto which we have processed.      *
//This will be latest record timestamp in db.     *
**************************************************/
inline void update_last_process_time_file()
{
  ND_LOG1("Method called");
  FILE *fp;
  if((fp = fopen(g_lpt_file, "w+")) == NULL)
  {
    ND_ERROR("%s: failed to open lpt file %s, error - %s\n", g_process_name, g_lpt_file, strerror(errno));
    exit(-1);
  }
  fprintf(fp, "%ld", g_last_process_time);
  fclose(fp);  
}

/*****************************************************
// Checking if  NV_TMP diectory exist else create it *
*****************************************************/
void kw_nv_tmp_file_path(char *buff)
{
  char keyword[1024] = "\0";
  char text1[1024] = "\0";
  int num = 0;
  ND_LOG1("Method called. buff = %s", buff);
  num = sscanf(buff, "%s %s", keyword, text1);
 
  if(num < 1 )
  {
    ND_ERROR("Format of DB_TMP_FILE_PATH keyword is not correct. %s", buff);
    ND_ERROR("Setting default values for DB_TMPS_PATH");
    return;
  }
  if(text1[0] == 0) /* Set default path */
  {
    //strcpy(g_client_id, g_hpd_controller_name);
  }
  else
  {
    strcpy(g_tmpfs_path, text1);
  }
  DIR* dir = opendir(g_tmpfs_path);
  if (dir)
  {
    ND_LOG1("g_tmpfs_path=%s\n exist", g_tmpfs_path);
    closedir(dir);
  }
  else
  {
    if(mkdir(g_tmpfs_path, 0775) != 0)  {
      ND_LOG1("Error in creating Directory", g_tmpfs_path);
      exit(-1);
    }
    else
      ND_LOG2("Directory Created successfully", g_tmpfs_path);
  }
}

void lower(char *str)
{
  char *iter=str;
  for (; *iter != '\0'; ++iter)
  {
    *iter = tolower(*iter);
  }
}

/**********************************
set client id                     *
**********************************/
void kw_set_client_id(char *buff)
{
  char keyword[1024] = "\0";
  char text1[1024] = "\0";
  int num=0;
  int i=0;
  int ascii;

  ND_LOG1("Method called. buff = %s", buff);
  num = sscanf(buff, "%s %s", keyword, text1);
  if(num < 1 )
  {
    ND_ERROR("Format of RUM_CLIENT_ID keyword is not correct. %s", buff);
    ND_ERROR("Setting default values for RUM_CLIENT_ID");
    return;
  }
  
  while (i != strlen(text1))  
  { 
    ascii = (int)(text1[i]); 
    if (((ascii >= 65) && (ascii <= 90))|| ((ascii >= 97) && (ascii <= 122)) || (ascii == 95)) 
    { 
      ND_LOG2("text1[%d] = %d", i, ascii);
    }
    else 
    { 
      /*****
      Other than alphabets, digits and _ are not allowed.
      As we are using the client id to put the table name in postgres 
      and postgress allows only alphabets, digits and _ for the name of a table. 
      ******/
      ND_ERROR("Format of RUM_CLIENT_ID keyword is not correct. %s", buff);
      ND_ERROR("Only alphabets, digits nd underscore(_) is allowed."); 
      exit(-1);
    }
    i++;  
  }  
 
  if(text1[0] == 0) /* Set default path */
  {
    //strcpy(g_client_id, g_hpd_controller_name);
  }
  else
  {
    strcpy(g_client_id, text1);
  }
  lower(g_client_id); 		//Converting the client id in lower case as postgres parses all the tables in lower case only. 
  ND_LOG2("Keyword g_client_id = %s", g_client_id);
}

/***************************************
create nd tmp file path                *
***************************************/
void create_nd_tmp_file_path()
{
  ND_LOG1("Method Called");
  DIR* dir = opendir(g_tmpfs_path);
  if (dir)
  {
    ND_LOG4("g_tmpfs_path=%s\n exist", g_tmpfs_path);
    closedir(dir);
  }
  else
  {
    if(mkdir(g_tmpfs_path, 0775) != 0)  {
      ND_LOG("Error in creating Directory", g_tmpfs_path);
    }
    else
      ND_LOG("Directory Created successfully", g_tmpfs_path);
  }
}

/************************************
method to get cav_epoch_difference  *
************************************/
long get_cav_epoch_diff()
{
   FILE *fp;
   char ch[512];
   char cav_epoch_diff_path[512];
   long epoch_diff;

   sprintf(cav_epoch_diff_path,"%s/.cav_epoch.diff", g_rum_base_dir);
   fp= fopen(cav_epoch_diff_path,"r");
   if(fp == NULL )
   {
    perror("Error in opening file .cav_epoch.diff");
    exit(0);
   }
   while(fgets(ch,1024,fp)){
     epoch_diff = atoi(ch);
   }
   fclose(fp);
   return epoch_diff;
}

/***************************************
// Parsing aggregate_scenario file     *
***************************************/
void parse_global_keywords()
{
  FILE *fp;
  char conf_file[512];
  char line[2048+1];
  //char error[4096];
  
  ND_LOG1("Method called");
  sprintf(conf_file, "%s/aggregate_scenario.conf", g_aggregate_conf_file);
  ND_LOG4("conf file = %s\n", conf_file);
  fp = fopen(conf_file, "r");
  if(fp == NULL)
  {
     fprintf(stderr, "Error: Unable to read %s file.", g_aggregate_conf_file);
     //exit(-1);
     return;
  }
  while(fgets(line, 2048, fp))
  {
    if(line[0] == '#' || line[0] == '\n' || line[0] == 0)
      continue;
    line[strlen(line) - 1] = 0;
    
    if(!strncmp(line, "DB_TMP_FILE_PATH", strlen("DB_TMP_FILE_PATH")))
      kw_nv_tmp_file_path(line);
    else
      create_nd_tmp_file_path();
    if(!strncmp(line, "ND_AGG_REPORTING_TRACE_LEVEL", strlen("ND_AGG_REPORTING_TRACE_LEVEL")))
      kw_set_nv_aggregate_reporting_trace_level(line);
    if(!strncmp(line, "ND_AGGREGATOR_RUN_AT", strlen("ND_AGGREGATOR_RUN_AT")))
      kw_set_nv_aggregate_run_at(line);
    if(!strncmp(line, "ND_AGGREGATOR_RUN_MODE", strlen("ND_AGGREGATOR_RUN_MODE")))
      kw_set_nv_aggregator_mode(line); 
  }
}

/*************************************************
Method to get bucket timestamp                   *
*************************************************/
long get_bucket_timestamp(time_t record_timestamp, int bucket_duration)
{
  time_t current_time = record_timestamp + g_cav_epoch_diff;
  time_t midnight_time; 
  time_t bucket_timestamp;
  struct tm *current_tm;

  current_tm = localtime(&current_time); 
  
  //get midnight time.
  current_tm->tm_hour = 0;
  current_tm->tm_min = 0;
  current_tm->tm_sec = 0;
    
  midnight_time = mktime(current_tm);
  
  //get partition time.
  bucket_timestamp = (midnight_time + ((int)(current_time - midnight_time)/bucket_duration)*bucket_duration);
  ND_LOG3("Bucket timetamp after get_bucket_timestamp %ld\n", bucket_timestamp);

  long tmpBucket =  bucket_timestamp - g_cav_epoch_diff; 
  if(tmpBucket < 0)
     tmpBucket = 0;

   return tmpBucket;
}

/**************************************************
get starttime from summary.top                    *
**************************************************/
static int get_summary_top_start_time_in_secs(char *date_str) {

  char buf[1024]; //2/24/09  11:50:44
  struct tm mtm;

  strptime(date_str, "%D %T", &mtm);
  strftime(buf, sizeof(buf), "%s", &mtm);
  printf("buf = %s\n", buf);
  return(atoi(buf));
}


/****************************************
read lpt from file                      *
****************************************/
static void get_last_process_time_from_lpt_file()
{
  ND_LOG1("Method Called\n");
  FILE *lpt_file;
  char ch[1024]; 
  if((lpt_file = fopen(g_lpt_file, "r")) == NULL)
  {
    ND_ERROR("Error: In opening file .lpt at path %s\n", g_lpt_file);
    exit(-1);
  }
  while(fgets(ch, 1024, lpt_file)){
     g_last_process_time = atoi(ch);
  }
  ND_LOG1("Last process time - %d", g_last_process_time); 
  fclose(lpt_file);
}

/************************************
set last process time               *
************************************/
static void set_last_process_time()
{
  //struct tm time_st;
  FILE *fp;
  char buff[1024];
  char filename[512];
  char *field[32];
  //int num;
  struct stat fstat;
 
  //set the lptfilename
  sprintf(g_lpt_file, "%s/aggregate/profiles/.%s_%d.lpt", gnswdir, g_aggregate_profile_ptr->table_name[0], gTestRunNo);
  //if we already have a lpt file regarding that testrun, then read lpt from file.
  if(!(stat(g_lpt_file, &fstat)))
  {
     get_last_process_time_from_lpt_file();
     return;
  }

  sprintf(filename, "%s/summary.top", g_rum_base_dir);

  fp = fopen(filename, "r");
  if(fp == NULL)
  {
     ND_ERROR("Unable to open file - %s\n", filename);
     exit(-1);
  }
  while(fgets(buff, 1024, fp))
  {
     get_tokens(buff, field, "|", 10);
     ND_LOG3("starttime - %s\n", field[2]);
  
     int time = get_summary_top_start_time_in_secs(field[2]);
     int tmp_time = time - g_cav_epoch_diff;
     if(tmp_time < 0)
       g_last_process_time = 0 ;
     else
       g_last_process_time = tmp_time - 3600;
     
  }
   //update lpt in a file
   update_last_process_time_file();
   ND_LOG3("last process time -  %d\n", g_last_process_time);
}

/***********************************************
set lpt in NV                                  *
***********************************************/
// Setting Last proc time
/*
static void set_last_process_time()
{
  //first check for .lpt file.
  FILE *lpt_file;
  int ret;
  //char partition_name[132];
  ND_LOG1("Method called");

  sprintf(g_lpt_file, "%s/aggregate/profiles/.%s.lpt", gnswdir, g_aggregate_profile_ptr->table_name[0]);
  ND_LOG1("Check for lpt file, %s\n",g_lpt_file);

  // IF lpt file does not exist then take the prco time from cur partition first partitio id
  struct stat fstat;
  if(stat(g_lpt_file, &fstat))
  {
    ND_LOG1("Unable to get the status of file %s\n", g_lpt_file);
    //sprintf(partition_name, "%lld", global_partition_idx);
    if(g_aggregate_profile_ptr->process_old_data)
    {
      ret = nslb_get_cur_partition_file_ex(&cur_part_info, g_rum_base_dir);
      if (ret == -1)
      {
        //throw what man

      }
      char partition_str[64];
      sprintf(partition_str, "%llu", cur_part_info.first_partition_idx);
      ND_LOG1("partitition str path %s\n", partition_str);
      unsigned long timestamp = nslb_get_time_in_secs(partition_str);
      g_last_process_time = (timestamp - g_cav_epoch_diff );
      g_last_process_time = get_bucket_timestamp(g_last_process_time, g_aggregate_profile_ptr->progress_interval);
    }
    else
    {
      unsigned long timestamp = time(NULL);
      g_last_process_time = (timestamp - g_cav_epoch_diff);
      g_last_process_time = get_bucket_timestamp(g_last_process_time, g_aggregate_profile_ptr->progress_interval);
      ND_LOG1("g_last pppp %ld\n", g_last_process_time);
    }
    update_last_process_time_file();
    ND_LOG1("g_last_process_time %ld\n", g_last_process_time);
  }
  //else read the file and update it
  else
  {
    if((lpt_file = fopen(g_lpt_file, "r")) == NULL)
    {
      ND_ERROR("Error: In opening file .lpt at path %s\n",g_lpt_file);
      exit(-1);
    }
    fscanf(lpt_file, "%ld", &g_last_process_time);
    fclose(lpt_file);
  }

  //sync to raat ke 12 baje because devils are waiting for us
  ND_LOG1("before sync up g_last_proc time %ld\n", g_last_process_time);
  g_last_process_time = get_bucket_timestamp(g_last_process_time, g_aggregate_profile_ptr->progress_interval);
  ND_LOG1("before sync up g_last_proc time %ld\n", g_last_process_time);
}*/


static inline void nv_db_analyze(char *table)
{
  char cmd[1024];
  PGresult   *res;
  sprintf(cmd, "VACUUM ANALYZE %s", table);
  if(execute_db_query_ex(cmd, &res, remote_db_connection, &g_remote_db_pid))
  {
     exit(-1);
  }
  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    ND_ERROR("ANALYZE failed: %s", PQerrorMessage(remote_db_connection));
    PQclear(res);
    return;
  }
  ND_LOG4("Sucessfully ANALYZED table (%s)", table);
}

/***********************************************************************
//will return -1 if any error else 0 if all records dumped.            *
//If partitionid is not given then calculate from bucket_timestamp     *
//Note: assumption is that aggNvRes will be equal to numAggTable       *
***********************************************************************/
int dump_aggregate_records_into_db(NVResultSet *aggNvResults, unsigned long bucket_timestamp, unsigned long partitionid)
{
  ND_LOG1("Method called");
  if(partitionid == 0)
  {
    partitionid = get_agg_partition_id(bucket_timestamp);
  }

  //check if partitionid is change then run vaccume analye on that. 
  if(g_last_partitionid != -1 && g_last_partitionid != partitionid)
  {
    char table_name[512];
    int k;
    for(k = 0; k < g_aggregate_profile_ptr->numAggTable; k++)
    {
      sprintf(table_name, "%s_%d_%ld", g_aggregate_profile_ptr->table_name[k], gTestRunNo, g_last_partitionid);
      ND_LOG1("table_name for analyze=%s\n", table_name);
      nv_db_analyze(table_name);
    }
  }
  g_last_partitionid = partitionid;

  //If there is only 
  char tmp_file_path[1024];
  FILE *fp = NULL;
  static char row[32*1024];
  int amt_written;
  NVResultSet *aggNvRes;
  NVColumn *column;
  char partition_table_name[1024];
  int i;
  
  for(i = 0; i < g_aggregate_profile_ptr->numAggTable; i++)
  { 
    aggNvRes = &aggNvResults[i];
    sprintf(tmp_file_path, "%s/.%s.csv", g_tmpfs_path, g_aggregate_profile_ptr->table_name[i]);
    ND_LOG4("tmp_file_path = %s\n", tmp_file_path);
    
    fp = fopen(tmp_file_path, "w");
    if(fp == NULL)
    {
       ND_ERROR("Failed to create file to dump into db, error - %s", strerror(errno));
       exit(-1);
    }
    
    //dump all the records. 
    int k, j;
    for(k = 0; k < aggNvRes->numRow; k++)
    {
      amt_written = 0;
      for(j = 0; j < aggNvRes->numColumn; j++)
      {
        column = nvGetValue(aggNvRes, k, j); 
        if(j == 0)
        {
          /*if(column->type == NV_STR)
            assert(atoi(column->value.str) > 0);
          else 
            assert(column->value.num > 0);*/
        }
        switch(column->type)
        {
          case NV_STR: 
            if(j == 0)
              amt_written += sprintf(&row[amt_written], "%s", column->value.str);
            else 
              amt_written += sprintf(&row[amt_written], ",%s", column->value.str);
            break;
          case NV_NUMBER: 
            if(j == 0)
              amt_written += sprintf(&row[amt_written], "%ld", column->value.num);
            else
              amt_written += sprintf(&row[amt_written], ",%ld", column->value.num);
            break;
          case NV_FLOAT:
            if(j == 0)
              amt_written += sprintf(&row[amt_written], "%.2f", column->value.float_num);
            else
              amt_written += sprintf(&row[amt_written], ",%.2f", column->value.float_num);
            break;
          default:
            break; 
            //TODO: 
        }
      }
      //dump this complete row into file.
      fprintf(fp, "%s\n", row);
    }
  
    //flush the data.
    fflush(fp);
    
    sprintf(partition_table_name, "%s_%d_%ld", g_aggregate_profile_ptr->table_name[i], gTestRunNo, g_last_partitionid);
    ND_LOG3("partition table name %s\n", partition_table_name);
    int ret;
    ret =  nslb_pg_bulkload(partition_table_name, tmp_file_path, "", ',');
    if(ret == 0)
      continue;
   
    //Check in case of 
    if(ret == -2)
    {
      ND_ERROR("Error in dumping data into db.");
      //now try to reconnect to db.
      ret = reconnect_to_db();
      if(ret == 0) {
        //should we upload the previous data again.
        //Note: this time if it will fail then ...
        ret = nslb_pg_bulkload(partition_table_name, tmp_file_path, "", ',');
        if(ret != 0)
        {
          fclose(fp);
          return ret;
        }
      }
      else {
        fclose(fp);
        return ret;
      }
    }
  }

  if(g_last_process_time < (bucket_timestamp + g_aggregate_profile_ptr->progress_interval))
  {
    g_last_process_time = bucket_timestamp + g_aggregate_profile_ptr->progress_interval; 
    ND_LOG3("After flushing, g_last_process_time = %lu", g_last_process_time);
    if(g_processing_mode == MULTI_INSTANCE_MODE)
      update_last_process_time_file();
  }
  
 fclose(fp);
 return 0; 
}

/***********************************************
Method to get timestamp of first record        *
***********************************************/
static inline long get_first_record_timestamp(unsigned long starttime, unsigned long endtime)
{
  ND_LOG1("Method called, starttime - %lu and endtime - %lu", starttime, endtime);
  PGresult *res;
  char cmd[4*1024];
  unsigned long first_record_time = 0;
  
  parse_and_replace_value(g_aggregate_profile_ptr->record_timestamp_sql, cmd, g_aggregate_profile_ptr->trno, starttime, endtime);
  ND_LOG3("Comamnd to last record timestamp %s\n", cmd); 
  
  if(execute_db_query_ex(cmd, &res, remote_db_connection, &g_remote_db_pid))
  {
     exit(-1);
  }  
  //If result set don't have 1 row then exit.
  if(PQntuples(res) != 1)
  {
    ND_LOG1("latest_record_query result is not valid, exiting.");
    exit(-1);
  }

  //In case if no new record present then it will having null.
  if(PQgetvalue(res, 0, 0) != NULL)
  {
    first_record_time = (unsigned long)atoll(PQgetvalue(res, 0, 0)); 
  }
  ND_LOG4("First record time is %lu in range (%lu - %lu)", first_record_time, starttime, endtime);
  return first_record_time;
}

/*****************************************
Get start and end time                   *
*****************************************/
inline void get_start_end_time(int *starttime, int *endtime,int *Break)
{
  //*starttime = g_last_process_time;
  if(*starttime+ (5*g_max_session_duration) > (time(NULL) - g_cav_epoch_diff)) 
  {
    *Break = 1;
    *endtime = time(NULL) - g_cav_epoch_diff;
  }
  else 
    *endtime = *starttime+ (5*g_max_session_duration);
}

int check_is_cmt_running()
{
   ND_LOG1("Method called\n");
   char cmd[256];
   char output[512];

   //check test is runnogn ot not. is not return 0 else 1 
   sprintf(cmd, "nsu_show_netstorm -n %d 2>/dev/null", gTestRunNo);
   FILE *f = popen(cmd, "r");
   while (fgets(output, 1024, f) != NULL)
   {
      if(!strncmp(output, "Active", strlen("Active")))
        return 1;
   }
   return 0;
}

/***********************************************
Get min, max in chunk                          *
***********************************************/
static inline void set_record_timestamp()
{
  ND_LOG1("Method called, last_process_time - %ld, cav_epoch_diff - %lu", g_last_process_time, g_cav_epoch_diff);
  PGresult *res;
  char cmd[4*1024];
  g_latest_record_time = 0;
  int max_session_duration = g_aggregate_profile_ptr->max_session_duration;
  long starttime, endtime;
  int Break = 0;
  int counter = 1;

  //Note: we will get min, max in chunk.
  starttime = g_last_process_time;
  if(starttime+ (5*max_session_duration) > (time(NULL) - g_cav_epoch_diff))
  {
    Break = 1;
    endtime = time(NULL) - g_cav_epoch_diff;
  }
  else
    endtime = starttime+ (5*max_session_duration);
 
  ND_LOG4(" Above while starttime - %d, endtime - %d\n", starttime, endtime);
  while(1)
  {
    parse_and_replace_value(g_aggregate_profile_ptr->record_timestamp_sql, cmd, g_aggregate_profile_ptr->trno, starttime, endtime);
    ND_LOG1("Comamnd to last record timestamp %s\n", cmd); 
  
    //Note: in case if remote db connection not given then it will use the local connection.
    if(execute_db_query_ex(cmd, &res, remote_db_connection, &g_remote_db_pid))
    {
       ND_LOG1("Failed to execute query - %s\n", cmd);
       exit(-1);
    }  
    //get first record.
    //If result set don't have 1 row then exit.
    if(PQntuples(res) != 1)
    {
      ND_LOG1("latest_record_query result is not valid, exiting..nrow=%s",PQntuples(res));
      exit(-1);
    }
    //In case if no new record present then it will having null.
    char *resultset = PQgetvalue(res, 0, 0);
    ND_LOG2("resultset=%s\n", PQgetvalue(res, 0, 0));
    if(resultset != NULL && resultset[0] != 0) 
    {
      g_first_record_time = (unsigned long)atoll(PQgetvalue(res, 0, 0)); 
      g_latest_record_time = (unsigned long)atoll(PQgetvalue(res, 0, 1)); 
      break;
    }
    else 
    {
      ND_LOG1("result is null \n");
      //Check if we have more time to check.
      if(Break) break;

      //set next start and end time.
      starttime = endtime;
      if(starttime+ (5*max_session_duration) > (time(NULL) - g_cav_epoch_diff))
      {
        Break = 1;
        endtime = time(NULL) - g_cav_epoch_diff;
      }
      else
        endtime = starttime+ (5*max_session_duration); 
    }
    ND_LOG1(" starttime=%d, endtime=%d, count\n", starttime, endtime, counter);
  }
  ND_LOG2("Latest record timestamp - %ld", g_latest_record_time);
}

/****************************************
Initialize active buckets               *
****************************************/
static inline void init_active_agg_buckets()
{
  totalActiveAggBucket = (int)ceil(g_max_session_duration / (double)g_aggregate_profile_ptr->progress_interval);
  ND_LOG1("totalActiveAggBucket = %d ", totalActiveAggBucket); 

  //FIXME: maxActiveAggBucket was not enough as per above formula. So for quick fix we have adde some more.
  maxActiveAggBucket = totalActiveAggBucket + 5;
  
  //now malloc for maxActiveAggBucket.
  activeAggBucket = (NVActiveAggBucket *)malloc(sizeof(NVActiveAggBucket) * maxActiveAggBucket);

  memset(activeAggBucket, 0, sizeof(NVActiveAggBucket) * maxActiveAggBucket);
   
  //malloc nvres.
  int i;
  for(i = 0; i < maxActiveAggBucket; i++)
  {
    //TODO: fix this. why we taking 1 exta.
    activeAggBucket[i].pendingNvres = (NVResultSet **)malloc(sizeof(NVResultSet *)*totalActiveAggBucket + 1);
    memset(activeAggBucket[i].pendingNvres, 0, sizeof(NVResultSet *)*totalActiveAggBucket + 1);
  }
  //Note: there is no need to get agg data from db. Because aggregate data will only be dumped when that will be completed.
}

/**************************************************************************************
//Note: assumption is that activeAggBucket should not be more than maxActiveAggBucket.*
**************************************************************************************/
static NVActiveAggBucket *get_nv_active_agg_bucket(time_t buckettimestamp)
{
  ND_LOG1("Method called");
  NVActiveAggBucket *freeActiveBucket = NULL;
  int i;
  for(i = 0; i < maxActiveAggBucket; i++)
  {
    if(activeAggBucket[i].bucketid == buckettimestamp)
    {
      return &activeAggBucket[i];
    }
    if(!freeActiveBucket && activeAggBucket[i].bucketid == 0)
      freeActiveBucket = &activeAggBucket[i];
  }
  //at this point we should have a freeActiveBucket.
  //TODO: if we don't have free entry than extend activeAggBucket.
  assert(freeActiveBucket != NULL);
  freeActiveBucket->bucketid = buckettimestamp;
  freeActiveBucket->partitionid = get_agg_partition_id(buckettimestamp);
  //TODO: this thing will not reset all the time.
  freeActiveBucket->numAggResultSet = g_aggregate_profile_ptr->numAggTable;
  freeActiveBucket->retryCount = 0;
  return freeActiveBucket;
}

/********************************************
sorting of db records                       *
********************************************/
static int sort_db_resultset(const void *a, const void *b)
{
   return (int)(((SortHelperStruct *)a)->timestamp - ((SortHelperStruct *)b)->timestamp);
}


/*******************************************
process active data                        *
*******************************************/
static inline void process_active_agg_resultset(PGresult *res)
{
  ND_LOG1("Method called");
 //First column of this result set will be timestamp. using timestamp find it;s buckettimestamp. and then find if this record is already there.
  int numRecords = PQntuples(res);
  int i, j,k;
  unsigned long recTimestamp;
  long bucketTimestamp;
  //bucket timestamp will never be 0.
  long prevBucketTimestamp =  -1 ;
  NVActiveAggBucket *lactiveAggBucket;
  char *value;
  char primaryKey[2048]; //In case if primary column given then merge them by _ to create key.
  int newRecordFlag;

  int curRecordIndex = 0;  //this will point to the index we using in complete columns.
  NVResultSet nvres;
  SortHelperStruct *shs;

  //Sort these records by timestamp. First column.
  //Copy all these records and their index into a tmp table.
  //FIXME: currently we are using some internal method to create NVResult.
  int numRow = PQntuples(res);
  int numCol = PQnfields(res);
  ND_LOG4("Number of records found %d, numRow %d, numCol %d\n", numRecords, numRow, numCol);

  //Note: it will not happen.
  if(numRow == 0 || numCol == 0)
  {
    return;
  }

  //Malloc for number of db records.
  shs = malloc(sizeof(SortHelperStruct) * (numRow));

  for(i = 0; i < numRow; i++)
  {
    shs[i].timestamp = get_bucket_timestamp(atoll(PQgetvalue(res, i, 0)), g_aggregate_profile_ptr->progress_interval);
    shs[i].id = i;
  }

  qsort(shs, numRow, sizeof(SortHelperStruct), sort_db_resultset);

  nvres.column = (NVColumn *)malloc(numRow * numCol * sizeof(NVColumn));
  nvres.numColumn = numCol;
  ND_LOG1("nvres.numColumn = %d\n", nvres.numColumn);

  //now fill nvres as per order after sorting.
  for(i = 0; i < numRow; i++)
  {
    for(j = 0; j < numCol; j++)
    {
       nvres.column[i*numCol + j].type = NV_STR;
       nvres.column[i*numCol + j].free = 0;
       nvres.column[i*numCol + j].value.str = PQgetvalue(res, shs[i].id, j);
    }
  }
  nvres.numRow = numRow;

  //Run loop on NVResultSet.
  for(i = 0; i < numRow; i++)
  {
    //Assumption is that first field always be timestamp.
    recTimestamp = atol(nvGetValue(&nvres, i, 0)->value.str);
    //get timestamp id.
    bucketTimestamp = get_bucket_timestamp(recTimestamp, g_aggregate_profile_ptr->progress_interval);
           
    ND_LOG4("record timestamp - %lu, bucket timestamp - %lu", recTimestamp, bucketTimestamp);

    if(prevBucketTimestamp != bucketTimestamp)
    {
      //update activeAggBucket.
      lactiveAggBucket = get_nv_active_agg_bucket(bucketTimestamp);
      prevBucketTimestamp = bucketTimestamp;
    
      //if normalize table not initialzed then initalize it.
      if(lactiveAggBucket->normalizeTable.nt == NULL)
      {
        //If this is a first entry then set retryCount.
        lactiveAggBucket->retryCount = (g_last_process_time + g_max_session_duration - bucketTimestamp)/g_aggregate_profile_ptr->progress_interval - 1;
        ND_LOG4("retrycount=>%d ==>for bucketTimestamp=>%ld g_last_process_time=>%d g_max_session_duration=>%d progressinterval=>%d",                           lactiveAggBucket->retryCount, bucketTimestamp, g_last_process_time, g_max_session_duration,                                                    g_aggregate_profile_ptr->progress_interval);
        if(lactiveAggBucket->retryCount < 0)
          lactiveAggBucket->retryCount = 0;    

        nslb_init_norm_id_table(&lactiveAggBucket->normalizeTable, NV_NORMALIZE_TABLE_DEFAULT_SIZE);
      }
    }

      //now add current entry into this.
      for(j = 0; j < g_aggregate_profile_ptr->numPrimaryColumn; j++)
      {
        value = nvGetValue(&nvres, i, g_aggregate_profile_ptr->primaryColumnIdx[j])->value.str;
        //If value is null then assert.
        assert(value != NULL);
        if(j == 0)
          sprintf(primaryKey, "%s", value);
        else
          sprintf(primaryKey, "%s_%s", primaryKey, value);
      }
      
    //this method will add this entry in lactiveAggBucket->normalizeTable.
    ND_LOG2("Record primary key - %s for bucketid=%ld\n", primaryKey, bucketTimestamp);

    //Check this key present or not.
    nslb_get_or_gen_norm_id(&lactiveAggBucket->normalizeTable, primaryKey, strlen(primaryKey), &newRecordFlag);
    if(newRecordFlag == 0)
    {
      //Old record.
      ND_LOG1("Old record, skipping.");
      continue;
    }
    ND_LOG3("New Record found for bucket id - %lu", bucketTimestamp); 
    //New record them add them in separate NVResultSet.
    //Check if column is null then add current Index address.
    //If first record have been added then add column.
    if(lactiveAggBucket->nvres.column == NULL)
    {
       //update currentColumnIndex.
       curRecordIndex = i;
       lactiveAggBucket->nvres.column = &nvres.column[i*numCol];
       lactiveAggBucket->nvres.numColumn = numCol;
       //Will be updated when we fill new entry.
       lactiveAggBucket->nvres.numRow = 0;
    }
    else 
      curRecordIndex ++;

    //fill the entry.
    //If curRecord not equal to current one then we have to shift them.
    if(curRecordIndex != i)
    {
      memcpy(&nvres.column[curRecordIndex*numCol], &nvres.column[i*numCol], sizeof(NVColumn)*numCol);
    }
    lactiveAggBucket->nvres.numRow ++;
  }

  NVResultSet *bucketNVRes = NULL;
  int numBucketNVRes;
  //NVResultSet *mergedAggNVRes = NULL;
  //int numMergedAggRes; //need not to keep this.
  //Check and aggregate records.
  for(i = 0; i < maxActiveAggBucket; i++)
  {
    if(activeAggBucket[i].retryCount >= 0){
      activeAggBucket[i].retryCount++;
      ND_LOG1("retrycount=>%d for bucket=>%ld\n",activeAggBucket[i].retryCount,activeAggBucket[i].bucketid);
    }
    //if valid one then print the records.
    if(activeAggBucket[i].nvres.column)
    {
      ND_LOG1("Bucket Tiemstamp %ld, Total DB records - %d(with column %d)", (long)activeAggBucket[i].bucketid,
             activeAggBucket[i].nvres.numRow, activeAggBucket[i].nvres.numColumn);

      numBucketNVRes = 0;
      bucketNVRes = g_aggregate_profile_ptr->parser_callback(&activeAggBucket[i].nvres, &numBucketNVRes, (long)activeAggBucket[i].bucketid);
     

      // dump the data in db
      ND_LOG4("bucketNVRes ==>%d", bucketNVRes);
      if(numBucketNVRes == g_aggregate_profile_ptr->numAggTable)
      {
        //check if pendingnvrescount is reached to maximum limit
        assert(activeAggBucket[i].pendingNvresCount <= totalActiveAggBucket + 1);
        
        activeAggBucket[i].pendingNvres[activeAggBucket[i].pendingNvresCount++] = bucketNVRes;
        activeAggBucket[i].nvres.column = NULL;
      }
      else {
        FREE_AGG_RESULTSETS(bucketNVRes, numBucketNVRes);
        FREE_AND_MAKE_NULL(bucketNVRes, NULL, "bucketNVRes");
      }
    }
    //Check if any bucketid expired then mark that free.
    if(activeAggBucket[i].retryCount >totalActiveAggBucket)
    {
      for(k=0; k<activeAggBucket[i].pendingNvresCount; k++)
      {
        if(dump_aggregate_records_into_db(activeAggBucket[i].pendingNvres[k], activeAggBucket[i].bucketid, activeAggBucket[i].partitionid))
        {
           ND_ERROR("%s: Failed to dump aggregate records into DB. for bucket timestamp - %ld", g_process_name,                                                   (long)activeAggBucket[i].bucketid);
           exit(-1);
        }
        FREE_AGG_RESULTSETS(activeAggBucket[i].pendingNvres[k], numBucketNVRes);
        FREE_AND_MAKE_NULL(activeAggBucket[i].pendingNvres[k], NULL, "activeAggBucket[i].aggResultSet");
        activeAggBucket[i].pendingNvres[k] = NULL;
      }
      activeAggBucket[i].aggResultSet = NULL;
      activeAggBucket[i].partitionid = 0;
      activeAggBucket[i].bucketid = 0;
      activeAggBucket[i].retryCount = -1;
      activeAggBucket[i].pendingNvresCount =0;
      //Note: activeAggBucket[i].nvres being used for temporary purpose.
      //clean normalized table.
      if(activeAggBucket[i].normalizeTable.nt)
      {
        nslb_obj_hash_destroy(&activeAggBucket[i].normalizeTable);
      }
    }
  }

  //free shs.
  FREE_AND_MAKE_NULL(shs, NULL, "shs");

  //free nvres.
  FREE_AND_MAKE_NULL(nvres.column, NULL, "nvres.column");
}



/*static inline void flush_active_agg_rec()
{
  //now check all active sessions if they are much older then flush.
  int i;
  NVResultSet *aggResultSet;
  ND_LOG1("Method called");
  for(i = 0; i < maxActiveAggBucket; i++)
  {
    if(activeAggBucket[i].aggResultSet != NULL)
    {
      //TODO: check this condition. What is g_latest_record_time is 0.
      ND_LOG1("Inside aggResultSet condition, resetCount - %d\n", activeAggBucket[i].retryCount);
      if(activeAggBucket[i].retryCount > totalActiveAggBucket)
      {
        aggResultSet = activeAggBucket[i].aggResultSet;
        //Dump into db.
        if(dump_aggregate_records_into_db(activeAggBucket[i].aggResultSet, activeAggBucket[i].bucketid, activeAggBucket[i].partitionid))
        {
          ND_ERROR("%s: Failed to dump aggregate records into DB. for bucket timestamp - %ld", g_process_name, (long)activeAggBucket[i].bucketid);
          exit(-1);
        }
        //update g_last_process_time accordingly.

        if(activeAggBucket[i].bucketid + g_aggregate_profile_ptr->progress_interval > g_last_process_time)
          g_last_process_time = activeAggBucket[i].bucketid + g_aggregate_profile_ptr->progress_interval;


        //reset this bucket.
        //free resultset.
        ND_LOG1("Freeing activeAggBucket[i].aggResultSet, for index %d and ptr - %p", i, activeAggBucket[i].aggResultSet);
        //Note: Never pass arg like activeAggBucket[i].aggResultSet in macro. Because if there is any local variable with same name then that value will be used. 
        FREE_AGG_RESULTSETS(aggResultSet, g_aggregate_profile_ptr->numAggTable);
        FREE_AND_MAKE_NULL(activeAggBucket[i].aggResultSet, NULL, "activeAggBucket[i].aggResultSet");
        activeAggBucket[i].aggResultSet = NULL;
        activeAggBucket[i].partitionid = 0;
        activeAggBucket[i].bucketid = 0;
        activeAggBucket[i].retryCount = -1;
        //Note: activeAggBucket[i].nvres being used for temporary purpose.
        //clean normalized table.
        if(activeAggBucket[i].normalizeTable.nt)
        {
          nslb_obj_hash_destroy(&activeAggBucket[i].normalizeTable);
        }
      }
    }
  }
}*/

/*******************************************************
update retry count                                     *
*******************************************************/
static inline void update_active_agg_retry_count()
{
  int i = 0;
  ND_LOG1("Method called, maxActiveAggBucket = %d", maxActiveAggBucket);
  for(; i < maxActiveAggBucket; i++)
  {
    if(activeAggBucket[i].aggResultSet)
    {
      activeAggBucket[i].retryCount ++;
      ND_LOG2("retrycount=>%d for bucketid=>%d", activeAggBucket[i].retryCount, activeAggBucket[i].bucketid);
    }
  }
}

/*******************************************************
set end constraints                                    *
*******************************************************/
static inline void set_partition_end_constraint(unsigned long partitionid, int duration)
{
  ND_LOG1("Method called, partitionid - %lu, duration - %d", partitionid, duration);

  char cmd[4096];
  char partition_str[32];
  sprintf(partition_str, "%lu", partitionid);
  unsigned long timestamp = (unsigned long)nslb_get_time_in_secs(partition_str); 
  timestamp -= g_cav_epoch_diff; 
  
  //Note: each aggregate table should have timestamp column.
  //set starttime constraint.
  int i;
  for(i = 0; i < g_aggregate_profile_ptr->numAggTable; i++)
  {
    sprintf(cmd, "ALTER TABLE %s_%lu_%s ADD CONSTRAINT END_TIME_CONSTRAINT_%s_%lu_%s"
               " CHECK (timestamp <= %lu)",
               g_aggregate_profile_ptr->table_name[i], partitionid, g_client_id,
               g_aggregate_profile_ptr->table_name[i], partitionid, g_client_id,
               timestamp + duration);
    if(execute_db_query(cmd, NULL))
    {
      ND_ERROR("Failed to set start constraint for partition table - %s_%lld_%s", 
               g_aggregate_profile_ptr->table_name[i], partitionid, g_client_id);
      return; 
    }
  }
}

/*******************************
create partition table         * 
********************************/
static inline void check_and_create_partition_table(unsigned long max_record_timestamp)
{
  ND_LOG1("Method called, max_record_timestamp - %lu", max_record_timestamp);
  //first convert max_record_timestamp into bucketid format.
  max_record_timestamp = get_bucket_timestamp(max_record_timestamp, g_aggregate_profile_ptr->progress_interval); 
   
  unsigned long prev_partitionid = g_partitionid;
  //get partition id.
  g_partitionid = get_agg_partition_id(max_record_timestamp); 
  //if changed then first set end constraint to that partition and then create the new one. 
  //Note: partition duration will be fixed.
  if(g_partitionid > prev_partitionid)
  {
    //first set end constraint.
    //Note: now we have set end constrint while creating table.
    //set_partition_end_constraint(prev_partitionid, g_partition_duration);
  
    ND_LOG4("Partition Switched from %lu -> %lu", prev_partitionid, g_partitionid);
    create_partition_table(0, g_partitionid, 1); 
  }
  else 
    g_partitionid = prev_partitionid;
}

//This method will check for partition for a given time range.
static inline void check_and_create_partition_table_for_time_range(unsigned long starttime, unsigned long endtime)
{
  ND_LOG1("Method called"); 
  unsigned long bucket_time = starttime;
  ND_LOG3("Bucket_time = %lu, endtime = %lu", bucket_time, endtime);
  while(bucket_time < endtime)
  {
    check_and_create_partition_table(bucket_time);
    bucket_time += g_aggregate_profile_ptr->progress_interval;
  }
  ND_LOG4("Bucket_time after calculating range =%lu", bucket_time);
}



//method to read db server name
//SRC_DB_SERVER host portno dbname username password
void read_keyword_db_server_name(char *buff, char *conn_str)
{
  char kw[512];
  char value1[512];
  char value2[512];
  char value3[512];
  char value4[512];
  char value5[512];
  char tmp[512];
  int fields;
  
  //kw HOSTNAME  PORT DBNAME USERNAME PASSWORD(optional)
  fields = sscanf(buff, "%s %s %s %s %s %s %s", kw, value1, value2, value3, value4, value5, tmp);
  if(fields < 5)
  {
    ND_LOG1("Invalid format of keyword SRC_DB_SERVER");
    exit(-1);
  }

  sprintf(conn_str, "dbname=%s user=%s  hostaddr=%s port=%s", value3, value4, value1, value2);
  ND_LOG1("connection string = %s", conn_str);
  if(fields > 5)
   sprintf(conn_str, "%s password=%s", conn_str, value5);
}

void check_and_freeActiveBucket(long bucketid)
{
  int i,k;
  for(i = 0; i < maxActiveAggBucket; i++)
  {
    if(activeAggBucket[i].bucketid >0 && activeAggBucket[i].bucketid <= bucketid)
    {
      ND_LOG("Free active resultset for bucketid=>%ld",bucketid);
      for(k = 0; k < activeAggBucket[i].pendingNvresCount; k++)
      {
        //Narendra: why it was commented
        FREE_AGG_RESULTSETS(activeAggBucket[i].pendingNvres[k], g_aggregate_profile_ptr->numAggTable);
        FREE_AND_MAKE_NULL(activeAggBucket[i].pendingNvres[k], NULL, "activeAggBucket[i].aggResultSet");
        activeAggBucket[i].pendingNvres[k] = NULL;
      }
      activeAggBucket[i].aggResultSet = NULL;
      activeAggBucket[i].partitionid = 0;
      activeAggBucket[i].bucketid = 0;
      activeAggBucket[i].retryCount = -1;
      activeAggBucket[i].pendingNvresCount =0;
      //clean normalized table.
      if(activeAggBucket[i].normalizeTable.nt)
      {
        nslb_obj_hash_destroy(&activeAggBucket[i].normalizeTable);
      }
     }
   } 
}

/************************************
load callback                       *
************************************/
int load_callback(aggregate_profile *ap)
{
  ND_LOG1("Method called load callback, parser file - %s", ap->parser_callback_src);

  //check for callback file.
  //char buff[MAX_LINE_LENGTH];
  char aggre_c_fname[MAX_LINE_LENGTH + 1];
  char aggre_so_fname[MAX_LINE_LENGTH + 1];
  //int ret;

  aggre_c_fname[0] = '\0';

  ND_LOG("parser_callback === %s\n", ap->parser_callback_src);
  if(!ap->parser_callback_src)
    return -1;

  if(ap->parser_callback_src[0] == '/')
    sprintf(aggre_c_fname, "%s", ap->parser_callback_src);
  else
    sprintf(aggre_c_fname, "%s/aggregate/profiles/%s", gnswdir, ap->parser_callback_src);

  sprintf(aggre_so_fname, "%s/.tmp/%s_aggregate_param.so", gnswdir, ap->table_name[0]);

  struct stat st;
  //check if file present.
  if(stat(aggre_c_fname, &st))
  {
    ND_LOG1("Callback file %s, not accessible, error - %s", aggre_c_fname, strerror(errno));
    return -1;
  }

  //now parse this file and create shared library.
  char cmd[MAX_LINE_LENGTH];
  void *so_handle = so_handle;

  char command_log_file[MAX_LINE_LENGTH];
  sprintf(command_log_file, "/tmp/nd_parser_compile.%d.log", getpid());
  ND_LOG1("C FILE NAME = %s", aggre_c_fname);
  //create gcc command.
  sprintf(cmd, "gcc  -ggdb -m%d -fgnu89-inline -fpic -shared -I %s/include -I /usr/include/postgresql/ -o %s %s %s/bin/nd_aggregator_api.so -DNV_CALLBACK -Wall %s 2>%s", 64, gnswdir, aggre_so_fname, aggre_c_fname, gnswdir, ap->gcc_args, command_log_file);
  ND_LOG("Command to compile callback file - %s ", cmd);
 

  int status = system(cmd);
  ND_LOG("Cmd - %s, return value -%d ",cmd, status);
  status = WEXITSTATUS(status);
  if(status != 0)
  {
    ND_ERROR("Failed to compile parser file(%s), check '%s' for error exit status is =%d", aggre_c_fname, command_log_file, status);
    return -1;
  }
  else {
    unlink(command_log_file);
  }

   //Load so file .
   so_handle = dlopen (aggre_so_fname, RTLD_NOW);
   //store that handle in aggregate profile
   ap->so_handle = so_handle;

   if(!so_handle)
   {
     //Failed to load so.
     ND_LOG( "Failed to load so file - %s, error - %s", aggre_so_fname, dlerror());
     return -1;
   }
   //Note: can be identify this method automatically.
   ap->parser_callback = dlsym(so_handle, "nd_agg_parser");
   if(!ap->parser_callback)
   {
     ND_ERROR("dlsym failed to load 'nd_agg_parser' callback from shared lib %s, error - %s", aggre_so_fname, strerror(errno));
     return -1;
   }

   //Check for nd_merge_agg_record callback.
   ap->merge_agg_callback = dlsym(so_handle, "nd_merge_agg_record");
   if (ap->merge_agg_callback == NULL) {
     ND_ERROR("dlsym failed to load 'nd_merge_agg_record' callback from shared lib %s, error - %s", aggre_so_fname, strerror(errno));
   }
  
  //Load parse keywords callback.
  ap->parse_keywords_callback = dlsym(so_handle, ND_PARSE_KEYWORDS_CALLBACK);
  //Note: this is optional.
  if(ap->parse_keywords_callback == NULL)
  {
    ND_LOG("%s callback not present, for aggregate profile %s", ND_PARSE_KEYWORDS_CALLBACK, ap->table_name[0]);
  }
  return 0;
}

static void sigchld_ignore(int sig)
{
  //pid_t pid;
  int status;
  waitpid(-1, &status, WNOHANG);
  return;
}

/*******************************
parsers                        *
*******************************/
void aggregate_child()
{
  ND_LOG1("Method Called");
  static char cmd[MAX_SQL_QUERY_SIZE];
  char aggregate_profile_parsing_error[4*1024];
  PGresult *res;
  NVResultSet *dbRes; 
  NVResultSet *aggNvRes;
  unsigned long starttime;
  unsigned long endtime;
  int progress_interval;
  int max_session_duration;
  int numResultSet;
  unsigned long first_record_time;
  char aprofpath[1024];
 
  //int prevNumDBRecords;
  unsigned long prev_last_process_time; 
  int break_flag;
  int first_record_check_flag = 0;
 
  (void) signal( SIGCHLD, sigchld_ignore);

  g_parent_process_pid = getppid();
  ND_LOG1("g_parent_process_pid = %d, getppid = %d\n", g_parent_process_pid, getppid());

  if(g_processing_mode == STANDALONE_MODE)
  {
    g_child_id = 0;
  }
  else
  {
    g_child_id = atoi(getenv("CHILD_INDEX"));
    g_aggregate_profile_ptr = &g_aggregate_profile_ptr[g_child_id - 1];
    //CLose the previous trace file and open new one.
    nslb_clean_mt_trace_log(g_trace_log_key); 
    //trace file will be prefixed with name of first aggregate table.
    char *trace_file = cmd;
    sprintf(trace_file, "nd_aggregate_reporter_%s.trace", g_aggregate_profile_ptr->table_name[0]);
    g_trace_log_key = nslb_init_mt_trace_log_ex(g_rum_base_dir, CURRENT_PATITION, trace_file, g_trace_level,                 g_trace_log_file_size); 
  }

  //set g_process_name.
  sprintf(g_process_name, "Aggregate_%s_%d", g_aggregate_profile_ptr->table_name[0], gTestRunNo);

  //set progress interval and max session duration   
  g_progress_interval= g_aggregate_profile_ptr->progress_interval;
  progress_interval = g_aggregate_profile_ptr->progress_interval;
  max_session_duration = g_aggregate_profile_ptr->max_session_duration;
  ND_LOG1("g_process_name %s, progress_interval %d\n", g_process_name, progress_interval);

  //load callback.
  if(load_callback(g_aggregate_profile_ptr) != 0)
  {
    ND_LOG("Failed to load callback for aggregate child");
    exit(-1) ;
  }

  //set aprofpath on the basis of parser mode  
  if(g_processing_mode == STANDALONE_MODE)
    sprintf(aprofpath, "%s", g_aggregate_profile_ptr->aprofname);
  else
    sprintf(aprofpath, "%s/aggregate/profiles/%s", gnswdir, g_aggregate_profile_ptr->aprofname);

  //parse profile specific keywords.
  if(g_aggregate_profile_ptr->parse_keywords_callback)
  {
    if(g_aggregate_profile_ptr->parse_keywords_callback(aprofpath, aggregate_profile_parsing_error))
    {
      ND_ERROR("Invalid profile %s, failed to parse profile specific keywords, Error - %s",                                    g_aggregate_profile_ptr->aprofname, aggregate_profile_parsing_error);
      exit(-1);
    }
  }

  //init db connection.
  db_connection = PQconnectdb(connecton_info);
  if(PQstatus(db_connection) != CONNECTION_OK)
  {
    ND_ERROR("Failed to create db connection.Error - %s\n", PQerrorMessage(db_connection));
    PQfinish(db_connection); 
    //These childs should not be respawned.
    exit(-1);
  }
  //We will check for this pid on db error.
  g_db_pid = PQbackendPID(db_connection);

  //case when we want data from remote machine 
 //remote db connection
  if(g_aggregate_profile_ptr->src_db_server_conn_string[0])
  {
    remote_db_connection = PQconnectdb(g_aggregate_profile_ptr->src_db_server_conn_string);
    if(PQstatus(remote_db_connection) != CONNECTION_OK)
    {
      ND_ERROR("Failed to create db connection.Error - %s\n", PQerrorMessage(remote_db_connection));       
      PQfinish(remote_db_connection);
      exit(-1);
    }
    g_remote_db_pid = PQbackendPID(remote_db_connection);
  }
  
  //check and create aggregate table.
  create_aggregate_table();

  if(g_processing_mode != STANDALONE_MODE)
  {
     set_last_process_time();
  }
  else 
  {
    //g_last_process_time = get_bucket_timestamp(g_start_time, g_aggregate_profile_ptr->progress_interval);
    g_last_process_time = g_start_time;
    ND_LOG1("standalone mode g_last_process_time %ld\n", g_last_process_time);
  }

  //create partition table
  create_partition_table(g_last_process_time, 0, 1);
  
  //get partition from g_last_process time and set as  g_last_partition_id.
  if(g_last_process_time > 0)
    g_last_partitionid = get_agg_partition_id(g_last_process_time);

  //init_active_agg_buckets();
  
  //aggregate parsing.
  while(1)
  {
    //check test status
    int test_state = check_is_cmt_running();
        
    //check if parnt is alive.
    if(g_parent_process_pid != getppid())
    {
      ND_LOG1("g_parent_process_pid %d is not same as %d, hence killing\n", g_parent_process_pid, getppid());
      exit(-1);
    }
    // check if we need to updaet latest_record_time.
    if(g_latest_record_time == 0 || (int)(g_latest_record_time - g_last_process_time) < max_session_duration + g_aggregate_profile_ptr->progress_interval)
    {
      ND_LOG1("g_latest_record_time = %d", g_latest_record_time);
      g_prev_latest_record_time = g_latest_record_time;
      set_record_timestamp();

      ND_LOG1("g_latest_record_time = %d, g_last_process_time = %d, g_max_session_duration -%d\n", g_latest_record_time, g_last_process_time, max_session_duration);
    }
    else if(first_record_check_flag)
    {
      prev_last_process_time = g_last_process_time;
      int tmp_process_time = 0;
      //check for first record in next maxActiveAggBucket * g_aggregate_profile_ptr->progress_interval window.
      while(g_latest_record_time - g_last_process_time > max_session_duration) 
      {
        break_flag = 0;
        tmp_process_time = g_last_process_time;
        first_record_time = get_first_record_timestamp(g_last_process_time, g_last_process_time + max_session_duration);  
        //update last_process_time.
        if(first_record_time)
        {
          break_flag=1;
        }
        if(!first_record_time)
          first_record_time = NV_MIN(g_latest_record_time, (g_last_process_time + max_session_duration));
        ND_LOG1("g_latest_record_time =%ld, g_max_session_duration =%ld, first_record_time =%ld", g_latest_record_time, max_session_duration,          first_record_time);
        g_last_process_time = NV_MIN((g_latest_record_time - max_session_duration), first_record_time);
        g_last_process_time = get_bucket_timestamp(g_last_process_time, g_aggregate_profile_ptr->progress_interval);

        ND_LOG1("updated last_process_time=  %ld", g_last_process_time);

        //Update that last process time into lpt file. 
        if(g_processing_mode == MULTI_INSTANCE_MODE)
          update_last_process_time_file();
        if(break_flag || tmp_process_time == g_last_process_time)
          break;
      }
      if(prev_last_process_time != g_last_process_time)
      {
        ND_LOG1("Updated last_process_time from %ld to %ld", prev_last_process_time, g_last_process_time);
        //create partition table.
        check_and_create_partition_table_for_time_range(g_last_process_time, NV_MIN(g_latest_record_time, g_last_process_time + max_session_duration));
      }
    }
   
    //check if we have latest record or not.
    if(g_latest_record_time == 0)
    {
      ND_LOG1("There is no record in db for starttime - %ld and endtime - %ld, sleeping.",                                             g_last_process_time, g_latest_record_time);
 
      //If there is no data present and parser is running in offline mode then stop the parser.
      if(g_aggregator_running_mode == OFFLINE)
      {
        //In case of offline if we do not have data then just stop the parser.
        exit(0);
      }
      else
      {
        //check carefully 
        //g_last_process_time = g_last_process_time + g_aggregate_profile_ptr->progress_interval;
        NV_CHILD_SLEEP(g_aggregate_profile_ptr->progress_interval);
        continue;
      }
    }
    ND_LOG1("g_last_process_time - %lu, g_first_record_time - %lu, g_latest_record_time - %lu",                                      g_last_process_time, g_first_record_time, g_latest_record_time);
    first_record_check_flag = 0;

    //NO need to process data in active case in ND.
    /*
    //flush active aggregate sessions.
    if((int)(g_latest_record_time - g_last_process_time) < (g_max_session_duration + g_aggregate_profile_ptr->progress_interval))
    {
       //If OFFLINE mode then exit. 
       if(g_aggregator_running_mode == OFFLINE)
       {
         exit(0);
       }
       starttime = g_last_process_time;
       ND_LOG1("Difference between both time in mode 1%d\n", (int)(g_latest_record_time - g_last_process_time));
       endtime = g_last_process_time + g_max_session_duration;

       //Check to terminate standalone mode.
       if(g_processing_mode == STANDALONE_MODE && starttime >= g_end_time)
       {
         ND_LOG1("Stand alone mode, Data processed till - %ld", g_end_time);
         break;
       }
       if(g_end_time > 0 && endtime > g_end_time) endtime = g_end_time;

       ND_LOG1("Data Collection mode 1, starttime - %ld, endtime - %ld", starttime, endtime);
       ND_LOG1("g_prev_latest_record_time = %lu, g_latest_record_time = %lu", g_prev_latest_record_time, g_latest_record_time);
       //Check and create partition table if partition changed 
       check_and_create_partition_table(endtime);
       //Fetch the records.
       ND_LOG1("startime - %ld, endtime - %ld\n", starttime, endtime);
       parse_and_replace_value(g_aggregate_profile_ptr->data_collection_sql, cmd, g_aggregate_profile_ptr->trno, starttime, endtime);
       ND_LOG1("Command to data collection %s\n", cmd);
       if(execute_db_query_ex(cmd, &res, remote_db_connection, &g_remote_db_pid))
       {
         exit(-1);
       }
       
       //if total rows are zero then continue.
       prevNumDBRecords = PQntuples(res);
       ND_LOG1("prevNumDBRecords = %d\n", prevNumDBRecords);
       if(prevNumDBRecords == 0)
       {
         ND_LOG1("prevNumDBRecords is 0 hence continue");
         //update retry count.
         //Note: In case if we have not received any new record, we have to increase retry count for already existing active records.
         update_active_agg_retry_count();
       }
       else
        {
          process_active_agg_resultset(res);
        }
        
       //clear db result set.
       PQclear(res);
       //TODO://ashish(2july) ...may be in active case we have some data in min,max query but not in data collection query at that time we            continue to process without updating the lpt
       //g_last_process_time += g_aggregate_profile_ptr->progress_interval; 
       //ND_LOG1("After flushing, g_last_process_time = %lu", g_last_process_time);
       //if(g_processing_mode == MULTI_INSTANCE_MODE)
       //  update_last_process_time_file();

       //sleep for next interval.
       NV_CHILD_SLEEP(g_aggregate_profile_ptr->progress_interval);
       continue;
    }*/   
       
      //If we have complete records then just take them and dump into db. No need to keep them into Active records.
      //run query to get records.     
     //if((int)(g_latest_record_time - g_last_process_time) > (max_session_duration))
    if(((int)(g_latest_record_time - g_last_process_time) > g_aggregate_profile_ptr->progress_interval) || !test_state)
    { 
      starttime = g_last_process_time;
      endtime = starttime + progress_interval;
        
      //Check to terminate standalone mode.
      if(g_processing_mode == STANDALONE_MODE && starttime >= g_end_time)
      break;
      if(g_end_time > 0 && endtime > g_end_time) endtime = g_end_time;

      ND_LOG1("Difference between both time in mode 2%d\n", (int)(g_latest_record_time - g_last_process_time));

      //check and create partition table.
      //Note: this will set end constraint on previous table.
      check_and_create_partition_table(endtime);
      ND_LOG1("Data collection mode 2, starttime - %ld, endtime - %ld", starttime, endtime);
 
      //for ND directly dumped a lpt as bucketid
      long bucketid = get_bucket_timestamp(starttime, g_aggregate_profile_ptr->progress_interval) + g_aggregate_profile_ptr->progress_interval;
      //long bucketid = g_last_process_time + g_aggregate_profile_ptr->progress_interval;
      ND_LOG1("Bucket id = %ld\n", bucketid);

      //check this bucket id (less than equal) in active buckets. and clear that bucketid.
      //check_and_freeActiveBucket(bucketid);

      parse_and_replace_value(g_aggregate_profile_ptr->data_collection_sql, cmd, g_aggregate_profile_ptr->trno, starttime, endtime);
      ND_LOG1("Command to data collection %s\n", cmd); 
      if(execute_db_query_ex(cmd, &res, remote_db_connection, &g_remote_db_pid))
      {
         exit(-1);
      }
      //check if there is no record then just continue.
      if(!PQntuples(res))
      {
        PQclear(res);
        //set flag to check first_record_timestamp.
        //for ND no need to set first_record_check_flag
        first_record_check_flag = 0;
        g_last_process_time += g_aggregate_profile_ptr->progress_interval;
        ND_LOG1("No record found for start time - %ld and endtime - %ld", starttime, endtime);
        continue; 
      }
      //convert db resultset into nvresultset.
      dbRes = dbRSToNVRS(res);

      //run callback and get aggregate NVRS.
      aggNvRes = g_aggregate_profile_ptr->parser_callback(dbRes, &numResultSet, bucketid);
      ND_LOG1("Total Aggregate records - %d", aggNvRes->numRow);

      //clear DATABASE Resultset.
      PQclear(res);

      //If numResultSet are not equal to total aggregate table then just ignore these records.
      if(numResultSet != g_aggregate_profile_ptr->numAggTable)   
      {
        ND_ERROR("%s : parser return resultset (%d) not equal to expected (%d), ignoring.\n", g_process_name,                              numResultSet, g_aggregate_profile_ptr->numAggTable);
      }
      else 
      {
        //if we get valid resultset then dump into db. 
        if(dump_aggregate_records_into_db(aggNvRes, g_last_process_time, 0)) 
        {
          ND_ERROR("%s: Failed to dump aggregate records into DB.", g_process_name);
          exit(-1);
        }
      }
      //FIXME:handle case when we can have multiple resultset. 
      //Need to provide an api to get multiple resultset.
      FREE_AGG_RESULTSETS(aggNvRes, numResultSet);
      FREE_AND_MAKE_NULL(aggNvRes, NULL, "aggNvRes");
      continue;  
    }
         
    //else sleep for idle time
    /*else
    {
      set_record_timestamp();
      if(g_latest_record_time == 0)
      {
        ND_LOG1("sleep g_latest_record_time - %d", g_latest_record_time);
        NV_CHILD_SLEEP(g_aggregate_profile_ptr->progress_interval);
      }
      //g_last_process_time += g_aggregate_profile_ptr->progress_interval;
      continue; 
    }*/
  }
}

/***************************************
//Note: --start <--- start marker      *
//      --end   <--- end marker        *
***************************************/
static char *get_multiline_kw_value(FILE *fp, char *buffer, int max_length, char *kw)
{
  static char buff[MAX_LINE_LENGTH];
  char line[4096];
  char *outbuffer;
  int buff_copied = 0;
  ND_LOG1("Method called");

  if(fp == NULL) return buffer;

  //if buffer given then use that else static one.
  if(buffer != NULL)
  {
    outbuffer = buffer;
  }
  else {
    outbuffer = buff;
    max_length = MAX_LINE_LENGTH;
  }

  outbuffer[0] = 0;
  int length;

  ND_LOG1("Method called for kw - %s", kw);

  while(fgets(line, 4096, fp))
    {
      if(line[0] == '\n' || line [0] == '#')
        continue;

      //Ignore start marker.
      if(!strncasecmp("--start", line, strlen("--start")))
        continue;

      if(!strncasecmp("--end", line, strlen("--end")))
      {
        ND_LOG1("Complete Value of kw - %s", outbuffer);
        return outbuffer;
      }
      //copy this value into buffer.
      length = strlen(line);
      if(buff_copied + length < max_length)
      {
        strcat(outbuffer, line);
        buff_copied += strlen(line);
      }
      else {
        ND_LOG1("Max buffer limit reached for keyword - %s", kw);
        strncat(outbuffer, line, (max_length - buff_copied) - 1);
        outbuffer[max_length - 1] = 0;
        return outbuffer;
      }
    }
  ND_LOG1("Complete file read, --end marker missing, Kw value - %s", outbuffer);
  return outbuffer;
}

/****************************************************************
//This method will attach shared memory and set g_cur_partition.*
****************************************************************/
inline void attach_test_run_info_shm_ptr_and_set_cur_partition()
{
  //set test_run_info_shm_ptr.
  if(test_run_info_shm_id != -1) 
  {
    //attach the shard memory.
    test_run_info_shm_ptr = shmat(test_run_info_shm_id, NULL, 0);
    if(test_run_info_shm_ptr == (void *)-1 )
    {
      //failed to attach shared memory.
      test_run_info_shm_ptr = NULL;
    }
    else {
      g_cur_partition = test_run_info_shm_ptr->partition_idx; 
      return;
    }
  }
  //get current partition. 
  g_cur_partition = nslb_get_cur_partition(g_rum_base_dir); 
}

#if 0
char *min_(char *delp, char *par_start,char *par_end)
{
   int temp, small;
  
   temp = (delp < par_start) ? delp : par_start;
   small =  (par_end < temp) ? par_end: temp;
   if(small== delp)
     return delp; 
   if(small == par_start)
     return par_start;
   if(small == par_end)
     return par_end;
return NULL;
}
#endif

char *nv_query_strstr(char *input, char *del, int ignorecase)
{
   /*Note: it will keep count for ( and )*/
  //char *parstart;
  //char *parend;
  //char *delptr;
  char *ptr = input;
  int openbraces = 0;
  int (*mystrncmp)(const char *, const char *, size_t);
  mystrncmp = strncmp;
  if(ignorecase)
    mystrncmp = strncasecmp;

  //Search character by character.
  while(ptr)
  {
    if(*ptr == 0) break;
    if(*ptr == '(') {
      openbraces++;
    }
    else if(*ptr == ')')
    {
      openbraces--;
    }
    else if(!openbraces)
    {
      if(!mystrncmp(ptr, del, strlen(del)))
        return ptr;
    }
    ptr++;
  }
  return NULL;
}


static inline void get_record_timestamp_query_from_data_collection_sql(aggregate_profile *ap)
{
  ND_LOG1("Method called");
  char timestamp_column[512]; 
  //If query is in format of 
  //select timestamp, col1, col2 
  // then just take timestamp with the appropriate aggregate function.
  //else   //TODO: currently we assuming that query will alwasy be in first format.
  // select agg_function(timestamp) from (data_collection_query);
  char *ptr = ap->data_collection_sql; 
  CLEAR_WHITE_SPACE(ptr);
  if(!strncasecmp(ptr, "SELECT ", 7))
  {
    //get first column name. which is timestamp column.
    ptr += 7;/*strlen("SELECT ")*/
    CLEAR_WHITE_SPACE(ptr);
    //column can be given inside a function.Eg. round(timestamp, 2) Handle that case.
    memcpy(timestamp_column, ptr, (strchr(ptr, ',') - ptr)); 
    timestamp_column[strchr(ptr, ',') - ptr] = 0; 
    ND_LOG1("Timestamp column - %s", timestamp_column);

    //check for first from.
    //ptr = strcasestr(ptr, "FROM ");
    ptr = nv_query_strstr(ptr,"FROM", 1);
    if(ptr == NULL)
    {
      ND_ERROR("Invalid format of data_collection_query - '%s'", ap->data_collection_sql);
      return;
    }
    sprintf(ap->record_timestamp_sql, "SELECT min(%s), max(%s) %s", timestamp_column, timestamp_column, ptr);
    ND_LOG1("record_timestamp_query - %s", ap->record_timestamp_sql);
  }
}

/*****************************************************************************************************
//this method will return 0 if there is no error in aggregate profile. else will fill all the errors.*
*****************************************************************************************************/
static int validate_aggregate_profile(aggregate_profile *ap, char *agg_profile_file_name, char *errorString)
{
  ND_LOG1("Method called, aggregate profile name - %s", agg_profile_file_name);

  int i = 0;
  //check if create query is given for all tables.
  for(i = 0; i < ap->numAggTable; i++)
  {
    if(ap->create_table_sql[i] == 0)
    {
      sprintf(errorString, "%s aggregate table don't have create sql\n", ap->table_name[i]);
      return -1;
    }
  }
  if(ap->parser_callback_src[0] == 0)
  {
    sprintf(errorString, "CALLBACK_FILE not given in profile\n");
    return -1;
  }
  //TODO: validate queries.
  if(ap->parser_callback == NULL)
  {
    sprintf(errorString, "parser callback %s not given in parser file %s", ND_PARSER_CALLBACK_NAME, ap->parser_callback_src);
    return -1;
  }

  if(ap->merge_agg_callback == NULL)
  {
    sprintf(errorString, "%s callback missing from parser file - %s", ND_MERGE_AGG_CALLBACK_NAME, ap->parser_callback_src);
    return -1;
  }

  if(ap->numPrimaryColumn == 0)
  {
    sprintf(errorString, "No primary column specified. Primary column is mandatory");
    return -1;
  }
  if(ap->data_collection_sql[0] == 0)
  {
    sprintf(errorString, "Date collection query is missing.");
    return -1;
  }
  if(ap->record_timestamp_sql[0] == 0) 
  {
    //create last record query from data collection sql.
    get_record_timestamp_query_from_data_collection_sql(ap);
    if(ap->record_timestamp_sql[0] == 0)
    {
      sprintf(errorString, "Can not detect record_timestamp_query from data_collection_sql");
      return -1;
    }
  }
  //if remote client id not given then take g_client id.
  if(!ap->src_client_id[0])
    strcpy(ap->src_client_id, g_client_id);

  return 0;
}

static int parent_started = 0;

static int pending_sigchld = 0;

static void nv_agg_handle_sigpipe(int sig)
{
  return;
}


static void nv_agg_log_child_exit_status(int status)
{
  //int status;

  ND_LOG1("Method called");

  if(WIFEXITED(status))
  {
    ND_LOG1("Child terminated normally. Exit status = %d", WEXITSTATUS(status));
  }
  else if(WIFSIGNALED(status))
  {
    ND_LOG1("Child process terminated because of a signal which was not caught. Signal = %d", WTERMSIG(status));
  }
  else if(WIFSTOPPED(status))
  {
    ND_LOG1("Child process which caused the return is currently stopped.   = %d", WSTOPSIG(status));
  }
  else if(WCOREDUMP(status))
  {
    ND_LOG1("Child process core dump is generated");
  }
}

static void nv_agg_handle_sickchild(int sig)
{
  pid_t pid;
  int status;
  int i;

  if(!parent_started)
  {
    pending_sigchld = 1;
    return;
  }

  //now check for the failed one.
  pid = waitpid(-1, &status, WNOHANG);
  if (pid <= 0) return;

  ND_LOG1("Received sigchild on process %d", pid);
  
  nv_agg_log_child_exit_status(status);
  //TODO: remove other condition.
  if(!WIFEXITED(status) || !WCOREDUMP(status)) {      /* we dont spawn it if child has exited gracefully. */
    for (i = 0; i < total_agggregate_entries; i++) {
      if(g_aggregate_profile_ptr[i].process_id == pid)
      {
        //This will be respawn by parent.
        g_aggregate_profile_ptr[i].process_id = -1;
        break;
      }
    }
  } else {
    for (i = 0; i < total_agggregate_entries; i++) {
      if(g_aggregate_profile_ptr[i].process_id == pid)
      {
        //These processes will not be respawned.
        memset(&g_aggregate_profile_ptr[i], 0, sizeof(aggregate_profile));
        break;
      }
    }
  }
}

static inline void recover_aggregate_child(char **argv)
{
  int i;
  char *env_buf;
  for(i = 0; i < total_agggregate_entries; i++)
  {
    if(g_aggregate_profile_ptr[i].process_id == -1)
    {
      if(!nslb_recover_component_or_not(&g_aggregate_profile_ptr[i].recovery))
      {
        //set environment variable for childid.
        MY_MALLOC(env_buf, 32, NULL, "for every child");
        ND_LOG1("Setting enviorment variables - CHILD_INDEX=%d", i+1);
        sprintf(env_buf, "CHILD_INDEX=%d", i+1);
        putenv(env_buf);

        g_aggregate_profile_ptr[i].process_id = fork();
        if(g_aggregate_profile_ptr[i].process_id == -1)
        {
          ND_ERROR("Failed to fork child for aggregate report %s\n", g_aggregate_profile_ptr[i].process_id);
          //Note: we will try again.
          continue;
        }
        //child came here.
        if(!g_aggregate_profile_ptr[i].process_id)
        {
          sprintf(argv[0], "nd_aggregator->[%s]      ",g_aggregate_profile_ptr[i].aprofname);
          aggregate_child();
          exit(0);
        }
        else 
        {
          ND_LOG1("Successfully respawned  aggregate_process(%s) in recovery with new process id - %d", 
                     g_aggregate_profile_ptr[i].table_name[0], g_aggregate_profile_ptr[i].process_id);    
        }
      } 
      else {
        ND_LOG1("Could not recover aggregate report process(%s), Tried %d times in %d seconds, shell try again in %d seconds.",
                         g_aggregate_profile_ptr[i].table_name[0],
                         ND_AGG_RESPAWN_RETRY_COUNT, ND_AGG_RESPAWN_RETRY_TOT_DURATION_SEC, ND_AGG_RESPAWN_RESTART_RETRIES_AFTER_SEC);
      }
    }
  }
}
static void nv_agg_handle_sigterm(int event)
{
  ND_LOG1("%s, SIGTERM received.", g_process_name);
  fprintf(stderr, "%s, SIGTERM received.", g_process_name);
  
  //If parent pid then send kill signal to all it's child process. 
  if(PARENT())
  {
    int i;
    for(i = 0; i <  total_agggregate_entries; i++)
    {
      if(g_aggregate_profile_ptr[i].process_id > 0){
        kill(g_aggregate_profile_ptr[i].process_id, SIGTERM);
      }
    }
  }
  exit(0); 
}

static void nv_agg_handle_sigint(int event)
{
  ND_LOG1("%s, SIGINT received.", g_process_name);
  fprintf(stderr, "%s, SIGINT received.", g_process_name);
  exit(0);
}

static int g_sigalrm_signal = 0;
static void nv_agg_handle_sigalrm(int event)
{
  ND_LOG1("%s, SIGALRM received.", g_process_name);
  g_sigalrm_signal = 1;
}

static inline void nv_agg_kill_children()
{
  ND_LOG1("Method called");
  int i;
  char process_desc[64];
  for(i = 0; i < total_agggregate_entries; i++)
  {
    //printf ("killing... %d\n", v_port_table[i].pid);
    if (g_aggregate_profile_ptr[i].process_id > 0) {
      sprintf(process_desc, "nv_agg child process [%d]", i);
      nslb_kill_and_wait_for_pid_ex(g_aggregate_profile_ptr[i].process_id, process_desc, 1, 10);
      //kill(g_aggregate_profile_ptr[i].process_id, SIGTERM);
      //printf ("waiting... %d\n", v_port_table[i].pid);
      //waitpid(g_aggregate_profile_ptr[i].process_id, &status, 0);
    }
  }
  exit(1);
}

//This method will return 0 - success,1 - failure.
int parse_aggregate_profile(char *aprof_file, aggregate_profile *ap, char aprofName[])
{
  struct stat report_stat;
  char buf[4*1024 + 1];
  int line_no;
  char aggregate_profile_parsing_error[4*1024];
  char *temp_ptr;
  int j;
  FILE *report_fp;
  char *text[100];
  //char error[4096];
  int num_fields;

    if(stat(aprof_file, &report_stat))
    {
      fprintf(stderr, "stat() failed for file %s, error = %s\n", aprof_file, strerror(errno));
      ND_LOG("stat() failed for file %s, error = %s\n", aprof_file, strerror(errno));
      //free(file_dirent);
      return -1;
    }
    if((report_fp = fopen(aprof_file,"r")) == NULL)
    {
      //hpd_error_log(0, 0, _FL_, "read_keywords", NULL, "unable to open conf file %s for NV", g_nv_conf_file);
      //TODO: replace return from exit
      //return;
      ND_LOG("error in opening aprof file");
      return -1;
    }

    line_no = 0;
    //memset complete ap.
    //printf("file name with path = %s\n", aprof_file);
    
    //copy aprof name
    strcpy(ap->aprofname, aprofName);

    while(fgets(buf, 4*1024, report_fp))
    {
      line_no++;

      //ignore emtpy lines.
      if((buf[0] == '\n')||(buf[0] == '#'))
        continue;
     
      buf[strlen(buf) - 1] = '\0'; //Removing new lines.

      if(!strncmp(buf, "TABLE_NAME", strlen("TABLE_NAME")))
      {
        temp_ptr = (char *)buf + strlen("TABLE_NAME");
        CLEAR_WHITE_SPACE(temp_ptr);
        //Note: there will be only 8 table name.
        num_fields = get_tokens(temp_ptr, text, ",", MAX_AGG_TABLE); 
        //tokenize these record and fill in a tableget_tokens(temp_ptr, text, ",", MAX_AGG_TABLE);
        for(j=0;j<num_fields;j++) {
          strcpy(ap->table_name[j], text[j]);
          ap->numAggTable++;
        }
      }
      //This is multi line keyword.
      //Format:
      //CREATE_TABLE_SQL <TABLE_NAME>
      else if(!strncmp(buf, "CREATE_TABLE_SQL", strlen("CREATE_TABLE_SQL")))
      {
        temp_ptr = (char *)buf + strlen("CREATE_TABLE_SQL");
        ND_LOG1("Inside CREATE TABLE SQL %s\n", temp_ptr);
        CLEAR_WHITE_SPACE(temp_ptr);
        ND_LOG1("After CREATE TABLE SQL %s\n", temp_ptr);
        //Now temp_ptr is point to table name.
        //search this table name in ap.tables.
        for(j = 0; j < ap->numAggTable; j++)
        {
          if(!strcasecmp(temp_ptr, ap->table_name[j]))
          {
            get_multiline_kw_value(report_fp, ap->create_table_sql[j], MAX_SQL_QUERY_SIZE, buf);
            break;
          }
        }
        if(j == ap->numAggTable)
        {
          ND_ERROR("kw - %s, no table found.", buf);
        }
      }
      else if(!strncmp(buf, "DATA_COLLECTION_SQL", strlen("DATA_COLLECTION_SQL")))
      {
         get_multiline_kw_value(report_fp, ap->data_collection_sql, MAX_SQL_QUERY_SIZE, buf);
      }
      else if(!strncmp(buf, "RECORD_TIMESTAMP_SQL", strlen("RECORD_TIMESTAMP_SQL")))
      {
         get_multiline_kw_value(report_fp, ap->record_timestamp_sql, MAX_SQL_QUERY_SIZE, buf);
      } 
      else if(!strncmp(buf, "PROGRESS_INTERVAL", strlen("PROGRESS_INTERVAL")))
      {
        temp_ptr = (char *)buf + strlen("PROGRESS_INTERVAL");
        CLEAR_WHITE_SPACE(temp_ptr);
        ap->progress_interval = atoi(temp_ptr);
      }
      else if(!strncmp(buf, "TEST_STATE", strlen("TEST_STATE")))
      {
        temp_ptr = (char *)buf + strlen("TEST_STATE");
        CLEAR_WHITE_SPACE(temp_ptr);
        ap->test_state = atoi(temp_ptr);
      }
      else if(!strncmp(buf, "G_MAX_SESSION_DURATION", strlen("G_MAX_SESSION_DURATION")))
      {
        temp_ptr = (char *)buf + strlen("G_MAX_SESSION_DURATION");
        CLEAR_WHITE_SPACE(temp_ptr);
        ap->max_session_duration = atoi(temp_ptr);
      }
       
      else if(!strncmp(buf, "GCCARGS", strlen("GCCARGS")))
      {
        temp_ptr = (char *)buf + strlen("GCCARGS");
        CLEAR_WHITE_SPACE(temp_ptr);
        strcpy(ap->gcc_args, temp_ptr);
      }
      else if(!strncmp(buf, "CALLBACK_FILE", strlen("CALLBACK_FILE")))
      {
        temp_ptr = (char *)buf + strlen("CALLBACK_FILE");
        CLEAR_WHITE_SPACE(temp_ptr);
        strcpy(ap->parser_callback_src, temp_ptr);
        ND_LOG1("parser_callback_src %s", ap->parser_callback_src);
      }
      else if(!strncmp(buf, "PROCESS_OLD_DATA", strlen("PROCESS_OLD_DATA")))
      {
        temp_ptr = (char *)buf + strlen("PROCESS_OLD_DATA");
        ap->process_old_data = atoi(temp_ptr);
      }
      else if(!strncasecmp(buf, "SRC_CLIENT_ID",strlen("SRC_CLIENT_ID")))
      {
        temp_ptr = (char *)buf + strlen("SRC_CLIENT_ID");
        CLEAR_WHITE_SPACE(temp_ptr);
        strcpy(ap->src_client_id, temp_ptr);
      }
      else if(!strncasecmp(buf, "SRC_DB_SERVER",strlen("SRC_DB_SERVER")))
      {
        read_keyword_db_server_name(buf, ap->src_db_server_conn_string);
      }
      else if(!strncmp(buf, "PRIMARY_KEY_COLUMN", strlen("PRIMARY_KEY_COLUMN")))
      {
         temp_ptr = (char *)buf + strlen("PRIMARY_KEY_COLUMN");
         num_fields = get_tokens(temp_ptr, text, ",", 32);
         for(j = 0; j < num_fields; j++)
         {
           ap->primaryColumnIdx[j] = atoi(text[j]);
           ap->numPrimaryColumn++;
         }
       }
        
      //TODO: parse LAST_RECORD_TIMESTAMP_SQL 
      //Note: this query should always have start timestamp condition.
    }
    //set dummy signal.
     
    //load callback.
    sighandler_t oldhandler = signal(SIGCHLD, sigchld_ignore);
    //load_callback(ap);
    if(load_callback(ap) != 0)
    {
      ND_LOG("Failed to load callback");
      exit(-1) ;
    }

    //set that back.
    signal(SIGCHLD, oldhandler);

    //parse profile specific keywords.
    if(ap->parse_keywords_callback)
    {
      if(ap->parse_keywords_callback(aprof_file, aggregate_profile_parsing_error))
      {
        ND_ERROR("Invalid profile %s, failed to parse profile specific keywords, Error - %s", aprof_file, aggregate_profile_parsing_error);
        return -1;
      }
    }
    aggregate_profile_parsing_error[0] = 0;
    //validate mandatory options.
    if(validate_aggregate_profile(ap, aprof_file, aggregate_profile_parsing_error))
    {
      ND_ERROR("Invalid profile %s, please correct following error - %s", aprof_file, aggregate_profile_parsing_error);
      return -1;
    }

    //fill Trno in aggregate_profile
    ap->trno = gTestRunNo; 
  
  //we have validated. Now close the open library. That will be open in aggregate_child again.
 if(g_processing_mode == MULTI_INSTANCE_MODE)
 {
   dlclose(ap->so_handle);
   ap->parse_keywords_callback = NULL;
   ap->parser_callback = NULL;
   ap->merge_agg_callback = NULL;
 }
 return 0;
}

static void usage(char *err_msg)
{

  fprintf(stderr, "%s\n", err_msg);
  fprintf(stderr, "Usage: \n");
  fprintf(stderr, "     : nd_aggregate_reports -f <aprof file name>  -s <starttime> -e <endtime> | -s <proj1/subproj1/scenario name -s\n");
  fprintf(stderr, "Where: \n");
  fprintf(stderr, "     : -f is used to provide aggregate profile name.\n");
  fprintf(stderr, "     : -s is used to give start time from where we need to get data.\n");
  fprintf(stderr, "     : -e is used to give end time from where we need to get data.\n");
  exit (-1);
}

//set parent pid
void set_parent_pid()
{ 
  FILE *fp;
  char filepath[1024];
  sprintf(filepath, "%s/.tmp/.aggregator_pid", gnswdir);
  fp = fopen(filepath, "w");
  if(fp == NULL)
  {
    ND_LOG("unable to open file %s", filepath);
    exit(-1);
  }
 
  fprintf(fp,"%d", g_parent_pid);
  fclose(fp);
}

void parse_aggregator_rtc_file(char aprof_name[], char action[])
{
  FILE *fp;
  char *ptr;
  char buf[4*1024+1];
  char filepath[1024];
  
  sprintf(filepath, "%s/.tmp/nv_aggregate.rtc", gnswdir);
  if((fp = fopen(filepath, "r")) == NULL)
  { 
    ND_LOG("unable to open aggregator rtc file");
    return;
  }
  while(fgets(buf, 1024, fp)) {
    buf[strlen(buf) - 1] = '\0'; //Removing new lines.
    //ignore emtpy lines.
    if((buf[0] == '\n')||(buf[0] == '#'))
     continue;
    
    //tokenize profilename and action
     char *tmp = buf;
     if((ptr = strchr(tmp, ' ')) != NULL)
     {
       *ptr = 0;
       strcpy(aprof_name, tmp);
       strcpy(action, ptr + 1);
       ND_LOG("profname=%s and action=%s", aprof_name, action);
     }
  }
  fclose(fp);
  unlink("/tmp/nv_aggregator.rtc");
}

aggregate_profile *get_agg_profile_entry(char *prof_name)
{
  ND_LOG("get_agg_profile_entry called");
  int i;
  for(i=0; i< total_agggregate_entries; i++)
  {
    //we are not stored aprof name in aggregate_profile ,so we kill the process on the basis of c callback file
    if(!strcmp(g_aggregate_profile_ptr[i].aprofname, prof_name) && g_aggregate_profile_ptr[i].process_id != 0)
    { 
      ND_LOG("Match Found\n"); 
      return &g_aggregate_profile_ptr[i];
    }
  }
  return NULL;
}

inline void print_status(char *aprof_name, char *status)
{
  ND_LOG("Method Called\n");
  char status_file[512];
  sprintf(status_file, "%s/.tmp/.%s.%d.status", gnswdir, aprof_name, getpid());

  FILE *fp;
  fp = fopen(status_file, "w");

  if(fp == NULL)
  {
    ND_ERROR("Failed to create status file - %s, error - %s", status_file, strerror(errno));
    return;
  } 
  ND_LOG("file successfully created with status = %s\n", status);
  fprintf(fp, "%s\n", status);
  fclose(fp);
}


//This function stop the parser
inline void stop_parser(char aprof_name[], char aprof_path[], aggregate_profile *ap)
{
  ND_LOG("stop parser called\n");
  if(ap == NULL)
    ap = get_agg_profile_entry(aprof_name);
 
  if(ap && ap->process_id > 0)
  {
    kill(ap->process_id, SIGTERM);
    //when we stop the parser ,profname and other entries remain in structure that create problem when we starting a killed parser
    memset(ap, 0, sizeof(aggregate_profile));
    ND_LOG("Parser %s stoped by sending stop signal", aprof_name);
    print_status(aprof_name, "SUCCESS");
    return;
  }
  print_status(aprof_name, "FAILED <Parser is not running");
  ND_LOG("parser %s  not running\n", aprof_name);
  fprintf(stderr, "Parser %s, not running.\n", aprof_name);
}


//This function is used to  start the parser, whose aprof file is passes as a argument in shell
//If Parser already running we stop it first then start the parser
inline void start_parser(char aprofname[], char aprof_path[], char **argv)
{
  ND_LOG("Start Parser called\n");
  aggregate_profile *ap = get_agg_profile_entry(aprofname); 
  //TODO:reuse the unused entries of g_aggregate_profile_ptr table.
  int row=0;
  int child_pid;
  char *env_buf;

  if(ap)
  {
    print_status(aprofname, "FAILED <Already Running>");
    //Note: if it is already running then just return from here. 
    ND_LOG("Parser %s already running.", aprofname);
    fprintf(stderr, "Parser %s already running.\n",aprofname);
    return;
  }

  {
    ND_LOG("create aggregate table entry\n");
    create_aggregate_table_entry(&row);
    ap = &g_aggregate_profile_ptr[row];
    memset(ap, 0, sizeof(aggregate_profile));
  }
  if(!parse_aggregate_profile(aprof_path, ap, aprofname))
  {
   //set environment variable for childid.
    MY_MALLOC(env_buf, 32, NULL, "for every child");
    ND_LOG3("Setting enviorment variables - CHILD_INDEX=%d", row+1);
    sprintf(env_buf, "CHILD_INDEX=%d", row+1);
    putenv(env_buf);
    child_pid = fork();
    
    ND_LOG("child pid=%d", child_pid);
    if(child_pid == -1)
    {
      print_status(aprofname, "FAILED");
      ND_ERROR("Failed to fork child for aggregate report %s\n", g_aggregate_profile_ptr[row]);
      fprintf(stderr, "Failed to fork child for aggregate report when we sent START signal = %s\n", strerror(errno));
    }

    if(!child_pid)
    {
      //child
      ND_LOG("aggregate child called");
      sprintf(argv[0], "nd_aggregator->[%s]      ",aprofname);
      aggregate_child();
      return ;
    }
    else {
      //parent.
      print_status(aprofname, "SUCCESS");
      ap->process_id = child_pid;
    }
  }
}

inline void check_status(char *aprof_name)
{
  //int status = 0; //Not running.
  aggregate_profile *ap = get_agg_profile_entry(aprof_name);

  if(ap && ap->process_id != 0)
  {
    //status = 1;
    print_status(aprof_name, "RUNNING");
    return;
  }
  print_status(aprof_name, "NOT RUNNING");
}

//function that perform runtime changes in aggregate parsers
void apply_rtc(char **argv)
{
  char aprof_name[512] = "";
  char action[512] = "";
  char aprof_path[1024];
 
  //parse file /docroot_prefix/.tmp/nv_aggregator.rtc
  parse_aggregator_rtc_file(aprof_name, action);
  ND_LOG("APROF NAME=%s, ACTION=%s", aprof_name, action);
 
  if(aprof_name[0] == 0 || action[0] == 0)
  {
    ND_ERROR("No profile found for applying rtc");
    return;
  }
  //set aprof path
  sprintf(aprof_path,"%s/aggregate/profiles/%s", gnswdir, aprof_name);

  //stop 
  if(!strcmp(action,"stop"))
    stop_parser(aprof_name, aprof_path, NULL);
  
  //start
  if(!strcmp(action, "start"))
    start_parser(aprof_name, aprof_path, argv);
 
  //restart
  if(!strcmp(action, "restart"))
  {
    stop_parser(aprof_name, aprof_path, NULL);
    start_parser(aprof_name, aprof_path, argv);
  } 

  if(!strcmp(action, "status"))
  {
    check_status(aprof_name);
  }
}

/*AggOfflineInfo *get_parser_cron_string(char *parsername)
{
  int i;

  //check if we have same cron for all parser NV_AGGREGATE_RUN_AT *******
  if(consider_all_parser_in_offline_mode == 1)
   return &gOfflineParserInfo[0].cronString;

  //check each parser one by one 
  for(i = 0; i < gTotalOffBucket; i++)
  {
    if(!strcmp(parsername, gOfflineParserInfo[i].parserName))
    {
       return &gOfflineParserInfo[i].cronString;
    } 
  } 
}*/
 
void fill_task_entry_in_table()
{
  ND_LOG("Method Called $NS_WDIR - %s\n", gnswdir);
  //int num_rec;
  char output[1024];
  char cmd[1024];
  //char out[1025];
  char *rec[32];
  
  
  //check if we reached to maximum limit 
  if(totAggTask >= MAX_SCHEDULAR_ENTRY)
  {
    ND_ERROR("Total schedular task exceed the limit - %d", totAggTask);
    return;
  }

  sprintf(cmd, "%s/tools/nsi_schedular_admin -o show 2>/dev/null", gnswdir);
  ND_LOG("cmd- %s\n", cmd);
 
  FILE *f = popen(cmd, "r");
  while (fgets(output, 1024, f) != NULL) 
  {
    //check for no records  
    if(!strcmp(output, "No task found"))
    {
       ND_LOG("NO records found\n");
       return;
    }
    //filter record of type aggParser
    if(strstr(output, "aggParser") != NULL)
    {
      ND_LOG("Record - %s", output); 
      //here we got the record of type aggParser
      //tokenize these record and fill in a table
      get_tokens(output, rec, "|", 32);

      ND_LOG("rec1 -%s, rec2-%s, rec3-%s, rec4-%s,", rec[0], rec[1], rec[2], rec[3]); 
      gSchedularInfo[totAggTask].taskid = atoi(rec[0]);
      strcpy(gSchedularInfo[totAggTask].taskname, rec[1]);
      strcpy(gSchedularInfo[totAggTask].type, rec[2]);
      strcpy(gSchedularInfo[totAggTask].command, rec[3]);
      strcpy(gSchedularInfo[totAggTask].cronstring, rec[4]);
      gSchedularInfo[totAggTask].expirytime = atoll(rec[5]); 
      strcpy(gSchedularInfo[totAggTask].disable, rec[6]);
      strcpy(gSchedularInfo[totAggTask].schedule, rec[7]);
      ND_LOG("taskname - %s\n", gSchedularInfo[totAggTask].taskname);

      ++totAggTask; 
    }
   }
   ND_LOG("Total task - %d", totAggTask);
   pclose(f);
}

int is_scheduled_or_not(char *name, int *taskid)
{
   ND_LOG("Method Called\n");
   int i;
   for(i = 0; i < totAggTask; i++)
   {
      if(!strcmp(gSchedularInfo[i].taskname, name))
      {
         *taskid = gSchedularInfo[i].taskid;
         ND_LOG("taskid - %d\n", gSchedularInfo[i].taskid);
         return 1;
      }
   }
   return 0;
}

void schedule_task()
{
   ND_LOG("Method Called\n");
   int  i, ret;
   int taskid = 0;
   char cmd[1024];
   char out[1025];
   //char prevConString[256];

   //fill task in table
   fill_task_entry_in_table();

   //check if parser already scheduled , if yes then check having a same time or not 
   //if time is different then remove the old entry and make new one
   ND_LOG("gTotalOffBucket - %d\n", gTotalOffBucket);
   for(i = 0; i < gTotalOffBucket; i++)
   {
     //check that we already have an entry.is yes then delete it
     //if return 1 means already scheduled
     ret = is_scheduled_or_not(gOfflineParserInfo[i].parserName, &taskid);
     ND_LOG("parser schedule - %d\n", ret);
     if(ret)
     {
        sprintf(cmd, "%s/tools/nsi_schedular_admin -o delete -i %d", gnswdir, taskid);
        //execute that command
        ret = nslb_run_cmd_and_get_last_line (cmd, 1024, out);
        ND_LOG("cmd - %s , status - %d", cmd, ret);
        if(ret)
        {
          ND_ERROR("Failed to delete task, command - %s, taskid - %d", cmd, taskid);
          return;
        }
     }
     //now add the task
     sprintf(cmd, "%s/tools/nsi_schedular_admin -o add --name %s --type aggParser --time \'%s\' --command \'%s/bin/nv_rtc_aggregate %s start\'      ", gnswdir, gOfflineParserInfo[i].parserName, gOfflineParserInfo[i].cronString, docrootPrefix, gOfflineParserInfo[i].parserName);

     //execute that command
     ret = nslb_run_cmd_and_get_last_line (cmd, 1024, out);
     ND_LOG("command to add task - %s, return value - %d\n", cmd, ret);
     if(ret)
     {
        ND_ERROR("Failed to add  task, command - %s", cmd);
        continue;
     }
   }
   //FIXME:needed or not.. update the gSchedularInfo table
   //gTotalOffBucket = 0;
   //fill_task_entry_in_table();
}

//function to get shared memory key that we used to get current partition info.
inline void get_shm_key()
{
  char *ptr;
  FILE *fp;
  char filepath[512];
  char line[10240];

  g_cur_partition = nslb_get_cur_partition(g_rum_base_dir);

  sprintf(filepath, "%s/%ld/.partition_info.txt", g_rum_base_dir, g_cur_partition);
  fp = fopen(filepath, "r");
  if(fp == NULL)
  {
     fprintf(stderr, "Error: Unable to read %s file. and g_rum_base_dir - %s", filepath, g_rum_base_dir);
     exit(-1);
  }
  while(fgets(line, 2048, fp))
  {
    if(line[0] == '#' || line[0] == '\n' || line[0] == 0)
      continue;
    line[strlen(line) - 1] = 0;
    if(!strncmp(line, "TRInfoShmKey=",strlen("TRInfoShmKey=")))
    {
       ptr = (char *)line + strlen("TRInfoShmKey=");
       test_run_info_shm_id = atoi(ptr);
    }
  }
}

void validate_offline_parser_profile(int *ignoreProfile, char *parserName)
{
  ND_LOG("Method Called");
  int i, j;
  for(i = 0; i < total_agggregate_entries; i++)
  {
     //check if already have entry for this parser
     for(j = 0; j < gTotalOffBucket; j++)
     {
        ND_LOG1("parsername - %s\n", gOfflineParserInfo[j].parserName);
        //here check for duplicate parser entry. 
        if(!strcmp(g_aggregate_profile_ptr[i].aprofname, gOfflineParserInfo[j].parserName))
        {
           *ignoreProfile = 1;
           return;
        }
     }
     //here check for valid parser name in keyword NV_AGGREGATE_RUN_AT
     if(!strcmp(g_aggregate_profile_ptr[i].aprofname, parserName))
     {
        *ignoreProfile = 0;
        return;
     }
   }
   //otherwise mark as invalid
   *ignoreProfile = 1;
}


//Initialize sigrtmin signal
int sigrtmin_recv = 0;

//handle sigrtmin+3
static void nv_agg_handle_test_post_proc_signal()
{
  ND_LOG1("%s, SIGRTMIN+3 received.", g_process_name);
  fprintf(stderr, "%s, SIGRTMIN+3 received.", g_process_name);
  
  //If parent pid then send kill signal to all it's child process. 
  if(PARENT())
  {
    int i;
    for(i = 0; i <  total_agggregate_entries; i++)
    {
      if(g_aggregate_profile_ptr[i].process_id > 0){
        kill(g_aggregate_profile_ptr[i].process_id, SIGTERM);
      }
    }
  }
  exit(0); 
}


//sigrtmin handler
static void nv_agg_handle_sigrtmin(int sig)
{
  ND_LOG("Received sigrtmin");
  sigrtmin_recv = 1;
}

/**************************************************************
//nde.testRunNum from $NS_WDIR/webapps/sys/config.ini file    *
**************************************************************/
int get_test_run_no()
{
  FILE *fp;
  char buf[1024*1024 + 1];
  char *ptr;
  char filename[256];

  if(getenv("NS_WDIR"))
    strcpy(gnswdir, getenv("NS_WDIR"));

  sprintf(filename, "%s/webapps/sys/config.ini", gnswdir);
  if((fp = fopen(filename, "r")) == NULL)
  {
    ND_LOG("Error in opening file - %s", filename);
    return -1;
  }

  while(fgets(buf, 1024*1024, fp))
  {
    buf[strlen(buf) - 1] = '\0'; //Removing new lines.
    //ignore emtpy lines.
    if((buf[0] == '\n')||(buf[0] == '#'))
      continue;

    if(!strncmp(buf, "nde.testRunNum = ", strlen("nde.testRunNum = "))){
      ptr = (char *)buf + strlen("nde.testRunNum = ");
      CLEAR_WHITE_SPACE(ptr);
      gTestRunNo =  atoi(ptr);
    }
  }
  fclose(fp);
  return 0;
}

static void create_default_tmpfs_dir()
{
  struct stat sb;
  if (stat(g_tmpfs_path, &sb) == 0 && S_ISDIR(sb.st_mode))
  {
     ND_LOG("Tmpfs directory already exist - %s\n", g_tmpfs_path);
     return;
  }
  else
  {
    if(mkdir(g_tmpfs_path, 0775) != 0)  
    {
      ND_ERROR("Error in creating Directory -%s\n", g_tmpfs_path);
      exit(-1);
    }
    else
     ND_LOG("Directory Created successfully - %s\n", g_tmpfs_path);
  }
}

/*********************
MAIN                 *
*********************/
int main(int argc, char *argv[])
{
  struct dirent **file_dirent;
  int num_files;
  char *ptr;
  int i;
  int ignoreProfile = 0;
  aggregate_profile ap;
  char c; 
  char agg_file_name_with_path[1024];
  //char sqlQuery[1024];
  ptr = getenv ("HPD_ROOT");
  if (ptr == NULL)
    ptr = "/var/www/hpd";
  strcpy(docrootPrefix, ptr);

  //set $NS_WDIR
  if(getenv("NS_WDIR"))
    strcpy(gnswdir, getenv("NS_WDIR"));
  else
    strcpy(gnswdir, "/home/netstorm/work");

/****************Standalone Mode Start**********/
  char aprof_filename[512] = "";
  while ((c = getopt(argc, argv, "c:t:f:s:e:l:")) != -1) {
  switch(c) {
    case 'c':
      strcpy(g_aggregate_conf_file, optarg);
      break;
    case 'f':
       //Check if file name is relative then convert into complete path.
       strcpy(aprof_filename, optarg);
       break;
    case 's':
       g_start_time =  atol(optarg);
       break;
    case 't':
       gTestRunNo = atoi(optarg);
       break;
    case 'e':
       g_end_time  = atol(optarg);
       break;
    case 'l': 
       strcpy(g_trace_file, optarg);
       break;
    case '?':
     default:
       usage(0);
    }
 } 

  //set deafult tr for testing
  set_rum_dir_path();

  //get testrunno
  //get_test_run_no();

  //set parent_pid.
  g_parent_pid = getpid();

 
  //read .partition_info_file and set key
  get_shm_key();

  attach_test_run_info_shm_ptr_and_set_cur_partition();

  //initialize trace log key for parent.
  g_trace_log_key = nslb_init_mt_trace_log_ex(g_rum_base_dir, CURRENT_PATITION, g_trace_file[0]?g_trace_file:"nd_aggregate_reporter.trace", g_trace_level, g_trace_log_file_size); 

  //create default tmpfs path
  create_default_tmpfs_dir();
 
  //get cav_epoch_diff
  g_cav_epoch_diff = get_cav_epoch_diff();

  //aggregate_child
  parse_global_keywords();

 //If any argument is given then we will run it standalone.
 if(aprof_filename[0])
 {
   //Validate the arguments. 
   if((g_start_time < 0) || (g_end_time < 0))
   {
     usage("Start time / end time not given in proper format\n");
   }
   //parse given profile.
   //check if complete path is given.
   if(aprof_filename[0] != '/')
     sprintf(agg_file_name_with_path, "%s/aggregate/profiles/%s", gnswdir, aprof_filename); 
   else
     strcpy(agg_file_name_with_path, aprof_filename);
  
   //set processing mode
   g_processing_mode = STANDALONE_MODE;
   
   memset(&ap, 0, sizeof(aggregate_profile));
   if(parse_aggregate_profile(agg_file_name_with_path, &ap, aprof_filename))
   {
     ND_LOG("Error in parsing profile %s", agg_file_name_with_path);
     exit(-1);
   }
   //malloc g_aggregate_profile_ptr and memset ap.
   MY_MALLOC(g_aggregate_profile_ptr, sizeof(aggregate_profile), NULL, "g_aggregate_profile_ptr");
   memcpy(g_aggregate_profile_ptr, &ap, sizeof(aggregate_profile));

   //set processing mode.

   (void) signal(SIGPIPE, nv_agg_handle_sigpipe);
   (void) signal(SIGTERM, nv_agg_handle_sigterm);
   (void) signal(SIGCHLD, nv_agg_handle_sickchild);
   (void) signal(SIGINT, nv_agg_handle_sigint);

   //start processing data.
   aggregate_child();
   return 0;
 }
/***************StandAlone Mode End************/

/************Multi Instance Mode *********/

  //set parentpid in file /tmp/.aggregator_pid
  set_parent_pid();

  sprintf(dir, "%s/aggregate/profiles/", gnswdir);

  num_files = scandir(dir, &file_dirent, file_filter, NULL);

  //Error.
  if(num_files == -1)
  {
    fprintf(stderr, "scandir() failed, error = %s\n", strerror(errno));
    return -1;
  }
  //No file found. 
  if(num_files == 0)
  {
    free(file_dirent);
    return -2;
  }
  char file_name_with_path[1024];
  
  //First we will parse all the keywords then validate them.
  for(i = 0; i < num_files; i++)
  {
    sprintf(file_name_with_path, "%s/%s", dir, file_dirent[i]->d_name);
   
    memset(&ap, 0, sizeof(aggregate_profile));
    if(parse_aggregate_profile(file_name_with_path, &ap, file_dirent[i]->d_name))
    {
      free(file_dirent[i]);
      continue;
    }
    free(file_dirent[i]);
    //now add this aggreagte profile in g_aggregate_table.
    if (create_aggregate_table_entry(&rnum) != 1)
    {
      fprintf(stderr, "Not enough emeory. Could not created serach var table entry.\n");
      exit(-1);
    }
    memcpy(&g_aggregate_profile_ptr[rnum], &ap, sizeof(aggregate_profile));
  }

  if(total_agggregate_entries == 0)
  {
    ND_ERROR("No profile found to process. Exiting.");
    exit(0);
  }

  //In offline mode, start the launcher but launcher will not start any parsers. 
  if(g_aggregator_running_mode == OFFLINE)
  {
    ND_LOG("OFFLINE MODE is Enabled\n");
    if(consider_all_parser_in_offline_mode)
    {
      for(i = 0; i < total_agggregate_entries; i++)
      {
         validate_offline_parser_profile(&ignoreProfile, g_aggregate_profile_ptr[i].aprofname);
         if(!ignoreProfile) 
         {
            ND_LOG("parsername - %s, cron string - %s\n", g_aggregate_profile_ptr[i].aprofname, gcronString);
            strcpy(gOfflineParserInfo[++gTotalOffBucket].parserName, g_aggregate_profile_ptr[i].aprofname);
            strcpy(gOfflineParserInfo[++gTotalOffBucket].cronString, gcronString);
         }
      }
      ND_LOG("Total offline task - %d\n", gTotalOffBucket);
    }

    //check if we have no cronstring keyword and offline mode is enable,then set default cronstring(12 AM)
    if(gTotalOffBucket == 0)
    {
       for(i = 0; i < total_agggregate_entries; i++)
       {
          ND_LOG("parsername - %s, cron string - %s\n", g_aggregate_profile_ptr[i].aprofname, gcronString);
          strcpy(gOfflineParserInfo[gTotalOffBucket].parserName, g_aggregate_profile_ptr[i].aprofname);
          strcpy(gOfflineParserInfo[gTotalOffBucket].cronString, "0 0 12 * * ?");
          gTotalOffBucket++;
       }
    }
 
     //schedule the task (add/delete)
     schedule_task();
  }

   if(g_aggregator_running_mode == ONLINE)
   {
     //now fork all the childs.
     int child_pid;
     char *env_buf;
     for(i = 0; i < total_agggregate_entries; i++)
     {    
       //set environment variable for childid.
       MY_MALLOC(env_buf, 32, NULL, "for every child");
       ND_LOG1("Setting enviorment variables - CHILD_INDEX=%d", i+1);
       sprintf(env_buf, "CHILD_INDEX=%d", i+1);
       putenv(env_buf);
       child_pid = fork();
    
       if(child_pid == -1)
       {
         ND_ERROR("Failed to fork child for aggregate report %s\n", g_aggregate_profile_ptr[i]);
         continue;
       }

       if(!child_pid)
       {
         //child
         //prctl(PR_SET_NAME, "ashish"); 
         //space to override previous name 
         //TODO:how to put null in argv[0]
         sprintf(argv[0], "nd_aggregator->[%s]      ",g_aggregate_profile_ptr[i].aprofname);
         aggregate_child();
         return 0;
       }
       else {
        //parent.
        g_aggregate_profile_ptr[i].process_id = child_pid;
        //init recovery.
        if(nslb_init_component_recovery_data(&g_aggregate_profile_ptr[i].recovery, ND_AGG_RESPAWN_RETRY_COUNT,ND_AGG_RESPAWN_RETRY_TOT_DURATION_SEC, ND_AGG_RESPAWN_RESTART_RETRIES_AFTER_SEC) == -1) {
          ND_ERROR("Failed to start recovery module for aggregate profile - %s", g_aggregate_profile_ptr[i].table_name[0]);
          //TODO: replace by kill all children.
          exit(-1);
        }
      }
    }
   }

  //now set the signals.
  //Note: setting sigchld handler before forking childrens because if any profile failed in begining then don't restart the aggregator.
  (void) signal( SIGCHLD, nv_agg_handle_sickchild );
  (void) signal( SIGPIPE, nv_agg_handle_sigpipe);
  (void) signal(SIGTERM, nv_agg_handle_sigterm);
  (void) signal(SIGINT, nv_agg_handle_sigint);
  (void) signal(SIGALRM, nv_agg_handle_sigalrm);
  //handle SIGRTMIN	
  (void)signal(SIGRTMIN, nv_agg_handle_sigrtmin);
  (void)signal(SIGRTMIN+3, nv_agg_handle_test_post_proc_signal); //received when netstorm test is completed/exit

  if(g_parent_pid == getpid())
  {
    parent_started = 1;

    if(pending_sigchld)
    {
      nv_agg_kill_children();
      return 0;
    }

    alarm(1);

    while(1)
    {
      if(g_sigalrm_signal)
      {
        recover_aggregate_child(argv);
        g_sigalrm_signal = 0;
      }
     
      //check for parent pid
      if(!nslb_check_pid_alive(getppid()))
      {
        ND_LOG1("parentpid = %d\n", getppid());
        nv_agg_handle_test_post_proc_signal(); 
      }
  
      if(sigrtmin_recv == 1)
      {
        apply_rtc(argv);
        sigrtmin_recv = 0;
      }

      alarm(1);

      //wait for the signal
      pause();
    }
  }

  //If parent then goto sleep.
  return 0;
}
