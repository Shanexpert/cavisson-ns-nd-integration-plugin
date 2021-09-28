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
#include <ctype.h>
#include <assert.h>

#include "../../libnscore/nslb_partition.h"
#include "../../libnscore/nslb_db_util.h"
#include "../../libnscore/nslb_signal.h"
#include "../../libnscore/nslb_multi_thread_trace_log.h"
#include "../../libnscore/nslb_partition.h"
#include "../../libnscore/nslb_util.h"
//#include "hpd_log.h"
#include "../../libnscore/nslb_sock.h"
#include "../../libnscore/nslb_get_norm_obj_id.h"
#include "../../libnscore/nslb_db_util.h"
#include "../../libnscore/nslb_comp_recovery.h"
#include "nd_aggregate_reports.h"
#include <sys/types.h>
#include <sys/wait.h>

//Current partition Id.
//Partition will be created only 
unsigned long long g_partitionid;
int g_child_id;
//connection per child, to fetch records from db and other task.
PGconn *db_connection;
char g_client_id[512];
int g_db_pid;
int g_progress_interval;

//While parsing this kw check if directory not present then create it. 
char g_tmpfs_path[512] = "/home/cavisson/nd_agg";

int g_nv_filter_outlier = 0;
int g_nv_page_load_time_outlier = 120000;
int g_nv_dom_time_outlier = 120000;
int g_nv_dom_content_load_time_outlier = 120000;

int g_remote_db_pid;
char g_process_name[512] = "main";
//In case if we unable to attach test_run_info_shm_ptr then get the ccurrent partition. we can not do any thing.
long g_cur_partition = -1;

//trace log key.
TestRunInfoTable_Shr *test_run_info_shm_ptr;
MTTraceLogKey *g_trace_log_key = NULL;
int sigterm_flag = 0;

//Method will be used to malloc entry for any table. 
//Can be used in callbac to realloc columns.
int nv_allocate_table_entry(int *total_alloted, int used, int required, void **entry_ptr, int entry_size, int delta_entries)
{
  ND_LOG4("total_Alloted = %d, used = %d, required = %d, delta_entries = %d", *total_alloted, used, required, delta_entries);
  if(*total_alloted < used + required)
  {
    int new_count = NV_MAX(*total_alloted +  delta_entries, *total_alloted + required);
    ND_LOG3("new count = %d, total_Alloted = %d, used = %d, required = %d, delta_entries = %d", new_count, *total_alloted, used, required, delta_entries);
    MY_REALLOC(*entry_ptr, new_count * entry_size, NULL, "data record entries");
    *total_alloted = new_count;
  }
  return (0);
}


/***********NVresultset <---> DBResultSet Methods**********/

NVResultSet* dbRSToNVRS(PGresult *dbRS)
{
  if(dbRS == NULL) return NULL;
  if(!PQntuples(dbRS)) return NULL;
  
  int i, j;
  NVResultSet *nvrs;
  //Malloc NVResultSet and NVRow. 
  //TODO: replace by MY_MALLOC.
  MY_MALLOC(nvrs, sizeof(NVResultSet), NULL, "NVResultSet");   
  nvrs->curIdx = 0;
  nvrs->numRow = PQntuples(dbRS);
  nvrs->numColumn = PQnfields(dbRS); 
  //Malloc for NVColumn.
  MY_MALLOC(nvrs->column, sizeof(NVColumn)*(nvrs->numRow*nvrs->numColumn), NULL, "nvrs->column");
  //Malloc for columns.
  
  for(i = 0; i < nvrs->numRow; i++)
  {
    for(j = 0; j < nvrs->numColumn; j++)    
    {
      nvrs->column[i*nvrs->numColumn + j].value.str = PQgetvalue(dbRS, i, j); 
    }
  } 
  return nvrs;
}

NVColumn *nvGetValue(NVResultSet *nvrs, int row, int column)
{
  if(!nvrs) return NULL;

  if(row < 0 || row > nvrs->numRow || column < 0 || column > nvrs->numColumn)
    return NULL;
  return &(nvrs->column[row * nvrs->numColumn + column]);
}

//This will malloc a new entry for NVResultSet.
NVResultSet *getNVResultSet(int numColumn, int deltaRow)
{
  NVResultSet *nvrs;
  MY_MALLOC(nvrs, sizeof(NVResultSet), NULL, "NVResultSet-API");
  //initialize other things.
  memset(nvrs, 0, sizeof(NVResultSet));
  if(deltaRow <= 0)
    deltaRow = 16;
  nvrs->deltaTuples = numColumn * deltaRow;
  nvrs->numColumn = numColumn;
  return nvrs;
}

//If successfully added then current row number else -1.
int nvAddRow(NVResultSet *nvrs, NVColumn *columns)
{
  ND_LOG3("Method called");
  //Validation.
  if(nvrs == NULL || columns == NULL)
    return -1;
  //Add new entry
  nv_allocate_table_entry(&nvrs->maxTuples, nvrs->numRow * nvrs->numColumn, nvrs->numColumn * 1/*as we are adding one row.*/, (void **)&nvrs->column, sizeof(NVColumn), nvrs->deltaTuples);
  memcpy(&nvrs->column[nvrs->numRow * nvrs->numColumn], columns, sizeof(NVColumn) * nvrs->numColumn);
  nvrs->numRow++;
  return (nvrs->numRow - 1);
}

//These helpers to set value.
inline void setNVColumnString(NVColumn *column, char *value, char free)
{
  column->value.str = value;
  column->free = (short)free;
  column->type = NV_STR;
}

inline void setNVColumnFloat(NVColumn *column, double value)
{
  column->value.float_num = value;
  column->free = 0;
  column->type = NV_FLOAT;
}

inline void setNVColumnNumber(NVColumn *column, long value)
{
  column->value.num = value;
  column->free = 0;
  column->type = NV_NUMBER;
}

//if equal then it will return 0.else bhagwaan jaane.
int compareNVColumn(NVColumn *column1, NVColumn *column2)
{
  //note: both should have same type.
  //Or any of them can have string type
  if(column1->type == column2->type)
  {
    switch(column1->type)
    {
      case NV_STR: return strcmp(column1->value.str, column2->value.str);
      case NV_NUMBER: return !(column1->value.num == column2->value.num); 
      case NV_FLOAT: return !(column1->value.float_num == column2->value.float_num); 
    }
  }
  else if(column1->type == NV_STR)
  {
    switch(column2->type)
    {
      case NV_NUMBER: return !(atol(column1->value.str) == column2->value.num); 
      case NV_FLOAT: return !(atof(column1->value.str) == column2->value.float_num); 
    }
  }
  else if(column2->type == NV_STR)
  {
    switch(column1->type)
    {
      case NV_NUMBER: return !(column1->value.num == atol(column2->value.str)); 
      case NV_FLOAT: return !(column1->value.float_num == atof(column2->value.str)); 
    }
  }
  return -1; 
}

inline void free_nv_result_set(NVResultSet *nvrs)
{
  int i = 0;
  int totalRecords = nvrs->numColumn * nvrs->numRow;

  for(i = 0; i < totalRecords; i++)
  {
    if(nvrs->column[i].type == NV_STR && nvrs->column[i].free)
    {
      FREE_AND_MAKE_NULL(nvrs->column[i].value.str, NULL, "nvrs->column[i].value.str");
    }
  }  
  if(nvrs->column)
  {
    FREE_AND_MAKE_NULL(nvrs->column, NULL, "nvrs->column");
  }
  //reset other things
  nvrs->numRow = nvrs->numColumn = nvrs->maxTuples = nvrs->deltaTuples = 0;
}


//TODO: configure the timeout for this.
int reconnect_to_db()
{
  int status;
  while(1) {
    PQreset(db_connection);
    status = PQstatus(db_connection);
    if(status == CONNECTION_OK)
      break;
    if(status == CONNECTION_BAD)
    {
      ND_ERROR("Failed to reconnect to db(Connection Probleam), will try after 60 sec");
      sleep(60);
      //Check if we have recieved SIGTERM Signal.
      if(sigterm_flag)
        return -1; 
    }
    else {
      //TODO: Specify error.
      ND_ERROR("Failed to reconnect to db");
      return -1;
    }
  }
  g_db_pid = PQbackendPID(db_connection);
  return 0;
}

//reconnect in case of remote connection
int reconnect_to_db_ex(PGconn *connection,  int *db_con_pid)
{
  ND_LOG("reconnect_to_db_ex method called");
  int status;
  while(1) {
    PQreset(connection);
    status = PQstatus(connection);
    if(status == CONNECTION_OK)
      break;
    if(status == CONNECTION_BAD)
    {
      ND_ERROR("Failed to reconnect to db(Connection Probleam), will try after 60 sec");
      sleep(60);
      //Check if we have recieved SIGTERM Signal.
      if(sigterm_flag)
        return -1; 
    }
    else {
      //TODO: Specify error.
      ND_ERROR("Failed to reconnect to db");
      return -1;
    }
  }
  *db_con_pid = PQbackendPID(connection);
  return 0;
}

/*static int nslbashish_check_pid_alive(pid_t pid)
{
  //first we will check using kill if it will fail with permisson then we will check using /proc/<pid> directory.
  if(!kill(pid, 0)) {
    //successfull means pid is running.
    return 1;
  }
  else {
    //check if error is EPERM then check for proc file.
    if(errno == EPERM) {
      struct stat proc_dir_stat;
      char proc_dir_name[64] = "";

      //Check for /proc/pid dir if exist then pid is runnig 
      //else process not running 
      sprintf(proc_dir_name, "/proc/%d", pid);

      if(!stat(proc_dir_name, &proc_dir_stat))
      {
        return 1;
      }
    }
  }
  return 0;
}*/


//execute db query for remote connection
int execute_db_query_ex(char *query, PGresult **res, PGconn *connection, int *db_con_pid)
{
  int status;
  PGresult *localRes; 
  char clearFlag = 0;
  
  ND_LOG2("Method called, query - %s", query);

  //check if connection is null then use db_conection.
  if(!connection)
  {
    connection = db_connection;
    *db_con_pid = g_db_pid;
  }

  if(res == NULL)
  {
    res = &localRes;
    clearFlag = 1;
  }

  *res = PQexec(connection, query);
  

  status = PQresultStatus(*res);

  if(clearFlag)
    PQclear(*res);

  //Note: in case if res was not given then we have to clear this result.
  if(status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK)
  {
    ND_ERROR("Failed to execute Query, error - %s", PQerrorMessage(connection));
    ND_LOG1("db_con_pid = %d\n", *db_con_pid);
    //check for pid.
    if(!nslb_check_pid_alive(*db_con_pid))
    {
      //move in recovery.
      ND_LOG1("going in recovery");
      status = reconnect_to_db_ex(connection, db_con_pid);
      if(status != 0)
      {
        ND_LOG1("Failed to reconnect.. status =%d", status);
        exit(-1);
      }
      if(status == 0)
      {
         *res = PQexec(connection, query);
         status = PQresultStatus(*res);
         if(status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK)
         {  
           ND_ERROR("Failed to execute Query, error - %s", PQerrorMessage(connection));
           //Note: not closing the connection.
           return -1;
         }
      }
    } 
    else {
      //In other error case we will not close the connection. It's upto the caller.
      /*
      PQfinish(db_connection);
      db_connection = NULL;
      g_db_pid = -1;
      */
      return -1;
    }
  }
  return 0;
}


//will return res else NULL.
//Should be use by child only.
//FIXME: after recovery it should try for query again.
int execute_db_query(char *query, PGresult **res)
{
  return execute_db_query_ex(query, res, db_connection, &g_db_pid);
}


