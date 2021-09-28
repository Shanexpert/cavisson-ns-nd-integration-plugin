#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <semaphore.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include "ns_common.h"
#include "nslb_data_types.h"
#include "ns_global_settings.h"
#include "ns_write_rtg_data_in_db_or_csv.h"
#include "nslb_alloc.h"
#include "ns_trace_level.h"
#include "nslb_util.h"
#include "nslb_alloc.h"
#include "nslb_odbc_driver_connect.h"
#include "nslb_sock.h"

rtgDataMsgQueueAndTablesInfo rtgDataMsgQueueAndTablesInfo_obj;
extern long long g_partition_idx;
extern int testidx;

typedef struct
{
  char fieldName[1024];
  char fieldSep;
}transMeasurementFields;

transMeasurementFields transMeasurementFields_obj[] = {{"GeneratorName", ','}, {"TransactionName", ' '}, {"Avg(sec)", ','},
                                                      {"Min(sec)", ','}, {"Max(sec)", ','}, {"Completed", ','}, {"Success", ','},
                                                      {"Failure(%)", ','}, {"TPS", ','}, {"ThinkTime(sec)", '\n'}};

int open_csv_file(char *errorMsg, int errorMsgLen, int *fd, char *fileNameWithPath)
{ 
  *fd = open(fileNameWithPath, O_CREAT|O_RDWR, 0666);
  
  if(*fd == -1)
  { 
    snprintf(errorMsg, errorMsgLen, "ERROR[%s] in opening file[%s]", strerror(errno), fileNameWithPath);
    return -1;
  }
  
  return 0;
}

int write_rtg_data_in_csv(char *rtgDataBuffer, int rtgDataBufferLen, char *errorMsg, int errorMsgLen)
{
  if(rtgDataMsgQueueAndTablesInfo_obj.writeInCsvOrDb & (1 << DB_WRITE_MODE))
  {
    if(!rtgDataMsgQueueAndTablesInfo_obj.csv_fd)
    {
      sprintf(rtgDataMsgQueueAndTablesInfo_obj.csvFileNameWithPath, "%s/logs/TR%d/transactionRtgData.csv",
              g_ns_wdir, testidx); 
   
      if(open_csv_file(errorMsg, errorMsgLen, &(rtgDataMsgQueueAndTablesInfo_obj.csv_fd), 
                       rtgDataMsgQueueAndTablesInfo_obj.csvFileNameWithPath)) 
        return -1;
    }

    rtgDataBufferLen += sprintf(rtgDataBuffer + rtgDataBufferLen, ",");
  }
  else if(rtgDataMsgQueueAndTablesInfo_obj.writeInCsvOrDb & (1 << CSV_WRITE_MODE))
  {
    if(rtgDataMsgQueueAndTablesInfo_obj.curPartition < g_partition_idx)
    {
      if(rtgDataMsgQueueAndTablesInfo_obj.csv_fd) 
        close(rtgDataMsgQueueAndTablesInfo_obj.csv_fd);
      
      rtgDataMsgQueueAndTablesInfo_obj.curPartition = g_partition_idx;
    
      sprintf(rtgDataMsgQueueAndTablesInfo_obj.csvFileNameWithPath, "%s/logs/TR%d/%lld/reports/csv/transactionRtgData.csv",
              g_ns_wdir, testidx, g_partition_idx); 
    
      if(open_csv_file(errorMsg, errorMsgLen, &(rtgDataMsgQueueAndTablesInfo_obj.csv_fd), 
                       rtgDataMsgQueueAndTablesInfo_obj.csvFileNameWithPath)) 
        return -1;
    }
  }

  if(rtgDataMsgQueueAndTablesInfo_obj.csv_fd != -1)
  {
    if(write(rtgDataMsgQueueAndTablesInfo_obj.csv_fd, rtgDataBuffer, rtgDataBufferLen) == -1)
    {
      snprintf(errorMsg, errorMsgLen, "ERROR[%s] writing in  file[%s]", strerror(errno), 
               rtgDataMsgQueueAndTablesInfo_obj.csvFileNameWithPath);
      return -1;
    }
  }
  return 0;
}

/*
  NS_WRITE_RTG_DATA_IN_DB_OR_CSV ENABLE CSV_OR_DB MsgQueue Size  ConnString	                                                           
  NS_WRITE_RTG_DATA_IN_DB_OR_CSV 1       1        30             DSN=Postgres;Servername=localhost;Username=cavisson;Database=test1;Port=5432 
  
  CSV_OR_DB
  1 - CSV
  2 - DB
*/

void kw_set_write_rtg_data_in_db_or_csv(char *buf)
{
  char keyword[2048];
  int enabled;
  int writeInCsvOrDb;
  char odbcConnString[2048];
  int msgQueueSize; 
  char num;
  
  //NSDL2_PARSING(NULL, NULL, "Method called, buf = %s", buf);
  
  num = sscanf(buf, "%s %d %d %d %s", keyword, &enabled, &writeInCsvOrDb, &msgQueueSize, odbcConnString);
 
  if (num < 5){
     NSTL1(NULL, NULL, "USAGE: \n"
                       "     	KEYWORD_NAME                   ENABLE  CSV_OR_DB MsgQueue_Size ConnString\n" 
                       "	NS_WRITE_RTG_DATA_IN_DB_OR_CSV 1       2         30            DSN=Postgres;Servername=localhost;Username=cavisson;Database=test1;Port=5432");
    //TODO
    //NS_EXIT(-1, "Invaid number of arguments %s", buf);
    return;
  }
  
  NSTL1(NULL, NULL, "NS_WRITE_RTG_DATA_IN_DB_OR_CSV[%s]", buf);
 
  if(enabled == 1)
  { 
    global_settings->write_rtg_data_in_db_or_csv = enabled;
    
    rtgDataMsgQueueAndTablesInfo_obj.writeInCsvOrDb = writeInCsvOrDb;
    
    if(msgQueueSize < 30)
    {
      msgQueueSize = 30;
    }
    
    rtgDataMsgQueueAndTablesInfo_obj.rtgDataMsgQueueInfo_obj.msgQueMaxSize = msgQueueSize;
    rtgDataMsgQueueAndTablesInfo_obj.rtgDataMsgQueueInfo_obj.msgQueInitSize = msgQueueSize/2;
    
    if(odbcConnString[0])
      strcpy(rtgDataMsgQueueAndTablesInfo_obj.odbcConnString, odbcConnString);
  }
}

void kw_set_write_rtg_data_in_influx_db(char *buf)
{
  char keyword[2048];
  char influxDbIP[128] = {0};
  int influxDbPort = 0;
  char dbName[1024] = {0};
  char userName[1024] = {0};
  char passwd[1024] = {0};

  char num = sscanf(buf, "%s %s %d %s %s %s", keyword, influxDbIP, &influxDbPort, dbName, userName, passwd);
  
  NSTL1(NULL, NULL, "NS_WRITE_RTG_DATA_IN_INFLUX_DB[%s]", buf);

  if(num != 6)
  { 
    NSTL1(NULL, NULL, "USAGE \n"
                      "NS_WRITE_RTG_DATA_IN_INFLUX_DB IP	PORT DB   USER     PWD\n"
                      "NS_WRITE_RTG_DATA_IN_INFLUX_DB 127.0.0.1 8086 Test cavisson cavisson");
    return;
  }
  
  if(influxDbIP[0]) strcpy(rtgDataMsgQueueAndTablesInfo_obj.influxDbInfo_obj.influxDbIP, influxDbIP);
  if(influxDbPort)  rtgDataMsgQueueAndTablesInfo_obj.influxDbInfo_obj.influxDbPort = influxDbPort;
  if(dbName[0])     strcpy(rtgDataMsgQueueAndTablesInfo_obj.influxDbInfo_obj.dbName, dbName);
  if(userName[0])   strcpy(rtgDataMsgQueueAndTablesInfo_obj.influxDbInfo_obj.userName, userName);
  if(passwd[0])     strcpy(rtgDataMsgQueueAndTablesInfo_obj.influxDbInfo_obj.passwd, passwd);
}

void msg_queue_creation_for_writing_rtg_data_in_db_or_csv()
{
  NSLB_MALLOC(rtgDataMsgQueueAndTablesInfo_obj.rtgDataMsgQueueInfo_obj.rtgDataMsgMpoolInfo, sizeof(nslb_mp_handler), 
              "nslb_mp_handler", -1, NULL);

  nslb_mp_init(rtgDataMsgQueueAndTablesInfo_obj.rtgDataMsgQueueInfo_obj.rtgDataMsgMpoolInfo, sizeof(rtgDataMsgInfo), 
               rtgDataMsgQueueAndTablesInfo_obj.rtgDataMsgQueueInfo_obj.msgQueInitSize, 1, MT_ENV);

  nslb_mp_set_max(rtgDataMsgQueueAndTablesInfo_obj.rtgDataMsgQueueInfo_obj.rtgDataMsgMpoolInfo, 
                  rtgDataMsgQueueAndTablesInfo_obj.rtgDataMsgQueueInfo_obj.msgQueMaxSize);
  
  if(nslb_mp_create(rtgDataMsgQueueAndTablesInfo_obj.rtgDataMsgQueueInfo_obj.rtgDataMsgMpoolInfo) < 0)
  {
    NSTL1(NULL, NULL, "Failed to create message queue");
    NSLB_FREE_AND_MAKE_NULL(rtgDataMsgQueueAndTablesInfo_obj.rtgDataMsgQueueInfo_obj.rtgDataMsgMpoolInfo, 
                            "rtgDataMsgMpoolInfo", -1, NULL);
    return;
  }
  NSTL1(NULL, NULL, "message queue is created");
}

#define CLEAR_WHITE_SPACE_AND_NEWLINE(ptr) {while ((*ptr == ' ') || (*ptr == '\t')|| (*ptr == '\n')) ptr++;}

#define POINT_TO_KEYWORD_VALUE(a)                                                   \
{                                                                                   \
  valuePtr = schemaConfFileBufferLoc + a;                                           \
  CLEAR_WHITE_SPACE(valuePtr);                                                      \
  if(*valuePtr == ':') valuePtr += 1;                                               \
  CLEAR_WHITE_SPACE(valuePtr);                                                      \
  while(*valuePtr == '\n' || *valuePtr == '\t') valuePtr += 1;                      \
  schemaConfFileBufferLoc = schemaConfFileBuffer + charsRead - 1 ;                  \
  *schemaConfFileBufferLoc = '\0';                                                  \
  CLEAR_WHITE_SPACE(valuePtr);                                                      \
}

#define REALLOC_CHECK(ptr, size, curr_idx, total_idx, INCREMENT)\
{\
  if(curr_idx >= total_idx)\
  {\
    int old_size = curr_idx * size;\
     total_idx+=INCREMENT;\
    int new_size = total_idx * size;\
    NSLB_REALLOC_AND_MEMSET(ptr, new_size, old_size, "realloc strcts", -1, NULL);\
  }\
} 


int create_rtg_tables()
{
  char *queryBuff = NULL;
  char errorMsg[1024];

  int querySize = 0;

  int queryBuffMallocSize = 0;

  if(nslb_odbc_init_handles(&(rtgDataMsgQueueAndTablesInfo_obj.odbcVarInfo_obj), errorMsg, 1024))
  {
    NSTL1(NULL, NULL, "ERROR[%s] in nslb_odbc_init_handles", errorMsg);
    nslb_odbc_free_handles(&(rtgDataMsgQueueAndTablesInfo_obj.odbcVarInfo_obj), errorMsg, 1024);
    return -1;
  }

  char connRetryCount = 0;

  while(1)
  {  
    if(connRetryCount >= 5)
    {
      nslb_odbc_free_handles(&(rtgDataMsgQueueAndTablesInfo_obj.odbcVarInfo_obj), errorMsg, 1024);
      return -1;
    }

    if(nslb_odbc_driver_connect(&(rtgDataMsgQueueAndTablesInfo_obj.odbcVarInfo_obj), 
                                rtgDataMsgQueueAndTablesInfo_obj.odbcConnString, errorMsg, 1024))
    {
      NSTL1(NULL, NULL, "ERROR[%s] in nslb_odbc_driver_connect", errorMsg);
      
      /*database is down*/
      if(strcasestr(errorMsg, "Connection refused"))
      {
        sleep(2);
        connRetryCount++;
        continue;
      }
     
      /*error in arguments or different error*/
      nslb_odbc_free_handles(&(rtgDataMsgQueueAndTablesInfo_obj.odbcVarInfo_obj), errorMsg, 1024);
      return -1;
    }
    else
      break;
  }

  if(nslb_odbc_init_statement_handle(&(rtgDataMsgQueueAndTablesInfo_obj.odbcVarInfo_obj), errorMsg, 1024))
  {
    NSTL1(NULL, NULL, "ERROR[%s] in nslb_odbc_init_statement_handle", errorMsg);
    nslb_odbc_free_handles(&(rtgDataMsgQueueAndTablesInfo_obj.odbcVarInfo_obj), errorMsg, 1024);
    return -1;
  }

  for(int rtgTablesInfo_ptr_idx = 0; rtgTablesInfo_ptr_idx < rtgDataMsgQueueAndTablesInfo_obj.totalTables; 
      rtgTablesInfo_ptr_idx++)
  {
    querySize = strlen(rtgDataMsgQueueAndTablesInfo_obj.rtgTablesInfo_ptr[rtgTablesInfo_ptr_idx].schema) +
                strlen(rtgDataMsgQueueAndTablesInfo_obj.rtgTablesInfo_ptr[rtgTablesInfo_ptr_idx].tablename) + 64;

    REALLOC_CHECK(queryBuff, 1, querySize, queryBuffMallocSize, querySize); 

    sprintf(queryBuff, "create table %s (%s);", 
            rtgDataMsgQueueAndTablesInfo_obj.rtgTablesInfo_ptr[rtgTablesInfo_ptr_idx].tablename,
            rtgDataMsgQueueAndTablesInfo_obj.rtgTablesInfo_ptr[rtgTablesInfo_ptr_idx].schema);

    if(nslb_odbc_execute_query(&(rtgDataMsgQueueAndTablesInfo_obj.odbcVarInfo_obj), queryBuff, errorMsg, 1024))
    {
      NSTL1(NULL, NULL, "ERROR[%s] in nslb_odbc_execute_query", errorMsg);
      return -1;
    }
    else
    {
      NSTL1(NULL, NULL, "Query[%s] executed successfully", queryBuff);
    }
  }
 
  if(queryBuff)
    free(queryBuff);

  return 0;
}


int parse_schema_for_rtg_tables(char *filename)
{
  FILE *schemaConfFileFp;
  char *schemaConfFileBuffer = NULL;
  char *schemaConfFileBufferLoc = NULL;
  char *schemaConfFileBufferLocPtr = NULL;
  char *valuePtr;
  size_t len = 0;
  ssize_t charsRead;
  char tablenameWithTR[2048];

  if((schemaConfFileFp = fopen(filename, "re")) == NULL)
  {
    NSTL1(NULL, NULL, "Error[%s] in opening file[%s]", nslb_strerror(errno), filename);
    return -1;
  }

  while(1)
  {
    schemaConfFileBufferLoc = NULL;
    schemaConfFileBufferLocPtr = NULL;

    if((charsRead = getdelim(&schemaConfFileBuffer, &len, ';', schemaConfFileFp)) == -1) 
    {
      NSTL1(NULL, NULL, "Error[%s] in getdelim function for file[%s]", nslb_strerror(errno), filename);
     /* if(schemaConfFileBuffer)
        free(schemaConfFileBuffer);
      
      if(schemaConfFileFp)
        fclose(schemaConfFileFp);
      return -1;*/
      break;
    }

    schemaConfFileBufferLoc = schemaConfFileBuffer;
    
    CLEAR_WHITE_SPACE_AND_NEWLINE(schemaConfFileBufferLoc);

    if((schemaConfFileBufferLocPtr = rindex(schemaConfFileBufferLoc, '#')))
    {
      while(*schemaConfFileBufferLocPtr != ';' && *schemaConfFileBufferLocPtr != '\n' && schemaConfFileBufferLocPtr)
        schemaConfFileBufferLocPtr += 1;
      

      if(*schemaConfFileBufferLocPtr == ';' || schemaConfFileBufferLocPtr == NULL)
      {
        schemaConfFileBufferLocPtr = NULL;
        continue;
      }

      schemaConfFileBufferLocPtr += 1;
      schemaConfFileBufferLoc = schemaConfFileBufferLocPtr;
    }

    CLEAR_WHITE_SPACE_AND_NEWLINE(schemaConfFileBufferLoc);

    if(schemaConfFileBufferLoc == NULL) continue;

    if(!strncasecmp(schemaConfFileBufferLoc, "TableName", 9))
    {
      POINT_TO_KEYWORD_VALUE(10);
      
      REALLOC_CHECK(rtgDataMsgQueueAndTablesInfo_obj.rtgTablesInfo_ptr, sizeof(rtgTablesInfo), 
                    rtgDataMsgQueueAndTablesInfo_obj.totalTables, rtgDataMsgQueueAndTablesInfo_obj.mallocTables, 5);
     
      sprintf(tablenameWithTR, "%s_%d", valuePtr, testidx);

      /*NSLB_MALLOC_AND_COPY(valuePtr, 
      rtgDataMsgQueueAndTablesInfo_obj.rtgTablesInfo_ptr[rtgDataMsgQueueAndTablesInfo_obj.totalTables].tablename, 
      strlen(valuePtr)+1, "tablename", -1, NULL);*/

      NSLB_MALLOC_AND_COPY(tablenameWithTR, 
      rtgDataMsgQueueAndTablesInfo_obj.rtgTablesInfo_ptr[rtgDataMsgQueueAndTablesInfo_obj.totalTables].tablename, 
      strlen(tablenameWithTR)+1, "tablename", -1, NULL);

      NSTL1(NULL, NULL, "tablename[%s]", 
      rtgDataMsgQueueAndTablesInfo_obj.rtgTablesInfo_ptr[rtgDataMsgQueueAndTablesInfo_obj.totalTables].tablename);
    }
    else if(!strncasecmp(schemaConfFileBufferLoc, "Schema", 6))
    {
      POINT_TO_KEYWORD_VALUE(7);
      
      NSLB_MALLOC_AND_COPY(valuePtr, 
      rtgDataMsgQueueAndTablesInfo_obj.rtgTablesInfo_ptr[rtgDataMsgQueueAndTablesInfo_obj.totalTables].schema, 
      strlen(valuePtr)+1, "Schema", -1, NULL);

      NSTL1(NULL, NULL, "schema[%s]", 
      rtgDataMsgQueueAndTablesInfo_obj.rtgTablesInfo_ptr[rtgDataMsgQueueAndTablesInfo_obj.totalTables].schema);
      
      rtgDataMsgQueueAndTablesInfo_obj.totalTables++;
    }
  }
 
  if(schemaConfFileBuffer)
    free(schemaConfFileBuffer);

  if(schemaConfFileFp)
    fclose(schemaConfFileFp);
 
  return 0; 
}

char *get_table_name_based_on_msgType(char msgType)
{
  switch(msgType)
  {
    case TRANSACTION_STATS:
      return rtgDataMsgQueueAndTablesInfo_obj.rtgTablesInfo_ptr[TRANSACTION_STATS].tablename; 
      break;
  }
  return NULL;
}

void insert_data_into_csv_file(rtgDataMsgInfo *rtgDataMsgInfo_ptr)
{
  char errorMsg[1024];

  if(write_rtg_data_in_csv(rtgDataMsgInfo_ptr->msg, strlen(rtgDataMsgInfo_ptr->msg), errorMsg, 1024))
  {
    NSTL1(NULL, NULL, "ERROR[%s] while writing buffer[%s] in csv", rtgDataMsgInfo_ptr->msg);
  }
  else
  {  
    NSTL1(NULL, NULL, "Successfully write buffer[%s] in csv[%s]", rtgDataMsgInfo_ptr->msg, 
          rtgDataMsgQueueAndTablesInfo_obj.csvFileNameWithPath);
  }
}
 
void insert_data_into_tables(rtgDataMsgInfo *rtgDataMsgInfo_ptr)
{
  char errorMsg[1024];
  int queryBufferLen = 0;

  char *tablename = get_table_name_based_on_msgType(rtgDataMsgInfo_ptr->msgType);
  
  if(tablename)
  {
    REALLOC_CHECK(rtgDataMsgQueueAndTablesInfo_obj.queryBuffer, 1, rtgDataMsgInfo_ptr->msgMallocSize + 48, 
                  rtgDataMsgQueueAndTablesInfo_obj.queryBufferMallocSize, rtgDataMsgInfo_ptr->msgMallocSize + 48);
  
    queryBufferLen = sprintf(rtgDataMsgQueueAndTablesInfo_obj.queryBuffer, "insert into %s values %s", tablename, 
                             rtgDataMsgInfo_ptr->msg);

    if(nslb_odbc_execute_query((&rtgDataMsgQueueAndTablesInfo_obj.odbcVarInfo_obj), 
                               rtgDataMsgQueueAndTablesInfo_obj.queryBuffer, errorMsg, 1024))
    {
      NSTL1(NULL, NULL, "ERROR IN executing QUERY[%s]", rtgDataMsgQueueAndTablesInfo_obj.queryBuffer);

      queryBufferLen += sprintf(rtgDataMsgQueueAndTablesInfo_obj.queryBuffer + queryBufferLen, ";\n");
 
      if(rtgDataMsgQueueAndTablesInfo_obj.db_fd <= 0)
      {
        sprintf(rtgDataMsgQueueAndTablesInfo_obj.dbFileNameWithPath, "%s/logs/TR%d/transInsertQueriesData",
                g_ns_wdir, testidx);
        
        if(open_csv_file(errorMsg, 1024, &(rtgDataMsgQueueAndTablesInfo_obj.db_fd), 
                         rtgDataMsgQueueAndTablesInfo_obj.dbFileNameWithPath)) 
        {
          NSTL1(NULL, NULL, "%s", errorMsg);
          return;
        }
      }
     
      if(rtgDataMsgQueueAndTablesInfo_obj.db_fd != -1)
      {
        if(write(rtgDataMsgQueueAndTablesInfo_obj.db_fd, rtgDataMsgQueueAndTablesInfo_obj.queryBuffer, queryBufferLen) == -1)
        {
          NSTL1(NULL, NULL, "ERROR[%s] writing in file[%s]", strerror(errno), 
                rtgDataMsgQueueAndTablesInfo_obj.dbFileNameWithPath);

          return;
        }
      }
    }
    else
    {
      NSTL1(NULL, NULL, "QUERY[%s] executed successfully", rtgDataMsgQueueAndTablesInfo_obj.queryBuffer);
    }
  }
} 

void read_msg_from_msg_queue_and_insert_data_into_tables_or_csv()
{
  rtgDataMsgInfo *rtgDataMsgInfo_ptr = NULL;
  while(1)
  {
    rtgDataMsgInfo_ptr = nslb_mp_get_busy_head(rtgDataMsgQueueAndTablesInfo_obj.rtgDataMsgQueueInfo_obj.rtgDataMsgMpoolInfo);
    if(rtgDataMsgInfo_ptr)
    {
      if(rtgDataMsgQueueAndTablesInfo_obj.writeInCsvOrDb & (1 << DB_WRITE_MODE))
        insert_data_into_tables(rtgDataMsgInfo_ptr); 
      else if(rtgDataMsgQueueAndTablesInfo_obj.writeInCsvOrDb & (1 << CSV_WRITE_MODE))
        insert_data_into_csv_file(rtgDataMsgInfo_ptr);
 
      nslb_mp_free_busy_head(rtgDataMsgQueueAndTablesInfo_obj.rtgDataMsgQueueInfo_obj.rtgDataMsgMpoolInfo, 
                             (nslb_mp_handler *)rtgDataMsgInfo_ptr);
    }
    else
      break;
  }
}

void * ns_upload_rtg_data_in_db_or_csv_thread()
{
  if(rtgDataMsgQueueAndTablesInfo_obj.writeInCsvOrDb & (1 << DB_WRITE_MODE))
  { 
    char filename[1024];
    sprintf(filename, "%s/etc/dbconf/ns_rtg_db_schema.conf", getenv("NS_WDIR"));
    
    //TODO never exit
    parse_schema_for_rtg_tables(filename);
      //goto thread_exit;
    
    create_rtg_tables();
      //goto thread_exit;
  }

  while(1)
  {
    if(sem_wait(&(rtgDataMsgQueueAndTablesInfo_obj.mutex)) == -1)
    {
      NSTL1(NULL, NULL, "ERROR[%s] in sem_wait", strerror(errno));
      continue;
    }

    read_msg_from_msg_queue_and_insert_data_into_tables_or_csv();
  }

  //TODO
  /*thread_exit:
    NSTL1(NULL, NULL, "ns_upload_rtg_data_in_db_or_csv_thread is exiting");
    global_settings->write_rtg_data_in_db_or_csv = 0;*/
    //TODO take lock
    /*nslb_mp_destroy(rtgDataMsgQueueAndTablesInfo_obj.rtgDataMsgQueueInfo_obj.rtgDataMsgMpoolInfo);
    NSLB_FREE_AND_MAKE_NULL(rtgDataMsgQueueAndTablesInfo_obj.rtgDataMsgQueueInfo_obj.rtgDataMsgMpoolInfo, 
                            "rtgDataMsgMpoolInfo", -1, NULL);*/
  return NULL;
}

void ns_upload_rtg_data_in_db_or_csv_thread_creation()
{
  pthread_t thread_id;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
 
  if(sem_init(&(rtgDataMsgQueueAndTablesInfo_obj.mutex), 0, 0) == -1)
  {
    nslb_mp_destroy(rtgDataMsgQueueAndTablesInfo_obj.rtgDataMsgQueueInfo_obj.rtgDataMsgMpoolInfo);
    NSLB_FREE_AND_MAKE_NULL(rtgDataMsgQueueAndTablesInfo_obj.rtgDataMsgQueueInfo_obj.rtgDataMsgMpoolInfo, 
                            "rtgDataMsgMpoolInfo", -1, NULL);
    global_settings->write_rtg_data_in_db_or_csv = 0;
    goto exit;
  }

  if(pthread_create(&thread_id, &attr, ns_upload_rtg_data_in_db_or_csv_thread, NULL) != 0)
  {
    nslb_mp_destroy(rtgDataMsgQueueAndTablesInfo_obj.rtgDataMsgQueueInfo_obj.rtgDataMsgMpoolInfo);
    NSTL1(NULL, NULL, "Failed to create ns_upload_rtg_data_in_db_or_csv_thread");
    NSLB_FREE_AND_MAKE_NULL(rtgDataMsgQueueAndTablesInfo_obj.rtgDataMsgQueueInfo_obj.rtgDataMsgMpoolInfo, 
                            "rtgDataMsgMpoolInfo", -1, NULL);
    global_settings->write_rtg_data_in_db_or_csv = 0;
  }
   
  NSTL1(NULL, NULL, "ns_upload_rtg_data_in_db_or_csv_thread created successfully");

  exit:
    pthread_attr_destroy(&attr);
}

void write_transaction_aggregate_data_in_csv()
{
  char errorMsg[2048];
  char tran_rtg_csv[1024];
  char queryBuff[2048];
   
  sprintf(tran_rtg_csv, "%s/logs/TR%d/transAggRtgData.csv", g_ns_wdir, testidx); 
  
  FILE *fptr = fopen(tran_rtg_csv, "w");
 
  if(!fptr)
  {
    NSTL1(NULL, NULL, "Error[%s] in opening file[%s]", nslb_strerror(errno), tran_rtg_csv);
    return;
  }

  sprintf(queryBuff, 
            "SELECT\n" 
         
            "  GeneratorName as \"Generator Name\", \n"
            
            "  transactionname as \"Transaction Name\", \n"
            
            "  CASE WHEN \n"
            "    sum(TxnTime_Success) > 0 \n"
            "  THEN \n"
            "    round((sum(TxnTime_Avg *TxnTime_Success)/sum(TxnTime_Success))::numeric/1000,3) \n"
            "  ELSE \n"
            "    0 \n"
            "  END as \"Avg\", \n"
       
            "  CASE WHEN \n"
            "    min(txntime_min) > 0 \n"
            "  THEN \n"
            "    round(min(txntime_min)::numeric/1000, 3) \n"
            "  ELSE \n"
            "    0 \n"
            "  END as \"Min\", \n"
              
            "  CASE WHEN \n"
            "    max(txntime_max) > 0 \n"
            "  THEN \n"
            "    round(max(txntime_max)::numeric/1000, 3) \n"
            "  ELSE \n"
            "    0 \n"
            "  END as \"Max\", \n"
            
            "  sum(txntime_success) as \"Completed\", \n"
            
            "  sum(TxnSuccTime_Success) as \"Success\", \n"
            
            "  CASE WHEN  \n"
            "    sum(txntime_success) > 0  \n"
            "  THEN\n" 
            "    round(((sum(txntime_success) - sum(TxnSuccTime_Success))/sum(txntime_success))::numeric*100, 3) \n"
            "  ELSE \n"
            "    0 \n"
            "  END as \"Failure(%%)\", \n"
           
            "  round(avg(TxnCompPerSec)::numeric,3) as \"TPS\", \n"
           
            "  CASE WHEN \n"
            "    sum(TxnThinkTime_Success) > 0 \n"
            "  THEN \n"
            "    round((sum(TxnThinkTime_Avg * TxnThinkTime_Success)/sum(TxnThinkTime_Success))::numeric/1000,3) \n"
            "  ELSE \n"
            "    0 \n"
            "  END as \"Think Time\" \n"
  
            "FROM \n"
            "  TransactionStats_%d \n"

            "GROUP BY \n"
            "  GeneratorName,transactionname \n"

            "ORDER BY \n"
            "  GeneratorName,transactionname;", testidx);

  NSTL1(NULL, NULL, "executing Query[%s]", queryBuff);

  if(nslb_odbc_execute_query((&rtgDataMsgQueueAndTablesInfo_obj.odbcVarInfo_obj), queryBuff, errorMsg, 2048))
  {
     NSTL1(NULL, NULL, "errorMsg[%s]", errorMsg);
     if(fptr)
       fclose(fptr);
    
     return;
  }

  NSTL1(NULL, NULL, "SUCCESS in executing Query");
  
  fprintf(fptr, "#Generator Name,Transaction Name,Avg(sec),Min(sec),Max(sec),Completed,Success,Failure(%%),TPS,Think Time(sec)\n");

  if(nslb_odbc_write_query_data_into_file((&rtgDataMsgQueueAndTablesInfo_obj.odbcVarInfo_obj), fptr, errorMsg, 2048, 1))
  {
    NSTL1(NULL, NULL, "errorMsg[%s]", errorMsg);
  }
  else
  {
    NSTL1(NULL, NULL, "SUCCESS in writing Query data in file[%s]", tran_rtg_csv);
  }

  if(fptr)
    fclose(fptr);
}

#define INCREMENT_SIZE 16 * 1024 * 1024

void insert_data_into_influx_db(int sockFd)
{
  char headerBuff[2048];
  
  char *bodyBuff = NULL;
  int bodyBuffCurrLoc = 0;
  int bodyBuffMallocSize = 0;
 
  char resultBuff[2048];
  
  int lineNumFields;
  char *lineFields[rtgDataMsgQueueAndTablesInfo_obj.odbcVarInfo_obj.linesCount + 1];
 
  int numFields;
  int transMeasurementFields_objCount = sizeof(transMeasurementFields_obj)/sizeof(transMeasurementFields_obj[0]);
  char *fields[transMeasurementFields_objCount + 1];
 
  char tmpBuff[2048];
  int tmpBuffLen = 0;

  int tableLen = strlen(rtgDataMsgQueueAndTablesInfo_obj.rtgTablesInfo_ptr[TRANSACTION_STATS].tablename); 
       
  /**************************************************************************************************************************
    InfluxDB line protocol is a text-based format for writing points to InfluxDB

    LINE PROTOCOL SYNTAX

    <measurement>[,<tag_key>=<tag_value>[,<tag_key>=<tag_value>]] <field_key>=<field_value>[,<field_key>=<field_value>] [<timestamp>]

   *************************************************************************************************************************/

  
  lineNumFields = get_tokens_with_multi_delimiter(rtgDataMsgQueueAndTablesInfo_obj.odbcVarInfo_obj.bufferPtr, lineFields, 
                                                  "\n", rtgDataMsgQueueAndTablesInfo_obj.odbcVarInfo_obj.linesCount + 1);
  if(lineNumFields == rtgDataMsgQueueAndTablesInfo_obj.odbcVarInfo_obj.linesCount)
  {
    NSTL1(NULL, NULL, "lineNumFields[%d] == rtgDataMsgQueueAndTablesInfo_obj.odbcVarInfo_obj.linesCount[%d] ", lineNumFields,
          rtgDataMsgQueueAndTablesInfo_obj.odbcVarInfo_obj.linesCount);
    
    for(int lineFieldsIdx = 0; lineFieldsIdx < lineNumFields; lineFieldsIdx++)
    {
     
      numFields = get_tokens_with_multi_delimiter(lineFields[lineFieldsIdx], fields, 
                                                  ",", transMeasurementFields_objCount + 1);
      if(numFields == transMeasurementFields_objCount)
      { 
        NSTL1(NULL, NULL, "numFields[%d] == transMeasurementFields_objCount[%d]", numFields, transMeasurementFields_objCount);
       
        REALLOC_CHECK(bodyBuff, 1, bodyBuffCurrLoc + tableLen, bodyBuffMallocSize, INCREMENT_SIZE);
        bodyBuffCurrLoc += sprintf(bodyBuff + bodyBuffCurrLoc, "%s,", 
                                   rtgDataMsgQueueAndTablesInfo_obj.rtgTablesInfo_ptr[TRANSACTION_STATS].tablename);
       
        for(int transMeasurementFields_objIdx = 0; transMeasurementFields_objIdx < transMeasurementFields_objCount; 
            transMeasurementFields_objIdx++)
        {
          tmpBuffLen = sprintf(tmpBuff, "%s=%s%c", transMeasurementFields_obj[transMeasurementFields_objIdx].fieldName, 
                               fields[transMeasurementFields_objIdx], 
                               transMeasurementFields_obj[transMeasurementFields_objIdx].fieldSep);
          
          REALLOC_CHECK(bodyBuff, 1, bodyBuffCurrLoc + tmpBuffLen, bodyBuffMallocSize, INCREMENT_SIZE);
          bodyBuffCurrLoc += sprintf(bodyBuff + bodyBuffCurrLoc, "%s", tmpBuff);
        }
      }
      else
      {
        NSTL1(NULL, NULL, "numFields[%d] != transMeasurementFields_objCount[%d]", numFields, transMeasurementFields_objCount);
      }
    }
  }
  else
  {
    NSTL1(NULL, NULL, "lineNumFields[%d] != rtgDataMsgQueueAndTablesInfo_obj.odbcVarInfo_obj.linesCount[%d] ", lineNumFields,
          rtgDataMsgQueueAndTablesInfo_obj.odbcVarInfo_obj.linesCount);
    return;
  }

  if(bodyBuff)
  {
    sprintf(headerBuff,
            "POST /write?db=%s&u=%s&p=%s HTTP/1.1\r\nHost: %s:%d\r\nContent-Length: %d\r\n\r\n",
             rtgDataMsgQueueAndTablesInfo_obj.influxDbInfo_obj.dbName, 
             rtgDataMsgQueueAndTablesInfo_obj.influxDbInfo_obj.userName, 
             rtgDataMsgQueueAndTablesInfo_obj.influxDbInfo_obj.passwd, 
             rtgDataMsgQueueAndTablesInfo_obj.influxDbInfo_obj.influxDbIP, 
             rtgDataMsgQueueAndTablesInfo_obj.influxDbInfo_obj.influxDbPort,
             bodyBuffCurrLoc);
    
    NSTL1(NULL, NULL, "Writing headerBuff[%s] to influxDb", headerBuff);
    int retVal = write(sockFd, headerBuff, strlen(headerBuff));
    if (retVal < 0)
    {
      NSTL1(NULL, NULL, "ERROR[%s] while writing headerBuff request to InfluxDB", nslb_strerror(errno));
      return;
    }

    NSTL1(NULL, NULL, "Writing BODY_BUFF[%s] to influxDb", bodyBuff);
    retVal = write(sockFd, bodyBuff, bodyBuffCurrLoc);
    if (retVal < 0)
    {
      NSTL1(NULL, NULL, "ERROR[%s] while writing BODY_BUFF request to InfluxDB", nslb_strerror(errno));
      return;
    }

    /* Get back the acknwledgement from InfluxDB */
    /* It worked if you get "HTTP/1.1 204 No Content"*/
    retVal = read(sockFd, resultBuff, sizeof(resultBuff));
    if (retVal < 0)
    {
      NSTL1(NULL, NULL, "ERROR[%s] while reading response from InfluxDB", nslb_strerror(errno));
      return;
    }

    resultBuff[retVal] = 0; /* terminate string */
    NSTL1(NULL, NULL, "Response[%s] from InfluxDB ****Note:204 is Success****", resultBuff);
  }
}

void make_conn_and_insert_data_into_influx_db()
{
  char errMsg[1024];
  int sockFd = nslb_tcp_client_ex_r(rtgDataMsgQueueAndTablesInfo_obj.influxDbInfo_obj.influxDbIP, 
                                    rtgDataMsgQueueAndTablesInfo_obj.influxDbInfo_obj.influxDbPort, 0, errMsg);

  if(sockFd == -1)
  {
    NSTL1(NULL, NULL, "ERROR[%s] while making connection with IP[%s] and PORT[%d]", errMsg, 
                       rtgDataMsgQueueAndTablesInfo_obj.influxDbInfo_obj.influxDbIP, 
                       rtgDataMsgQueueAndTablesInfo_obj.influxDbInfo_obj.influxDbPort);
    return;
  }
    
  NSTL1(NULL, NULL, "INFLUX DB :- Connection made Successfully with IP[%s] and PORT[%d] for sockFd[%d]",
                     rtgDataMsgQueueAndTablesInfo_obj.influxDbInfo_obj.influxDbIP, 
                     rtgDataMsgQueueAndTablesInfo_obj.influxDbInfo_obj.influxDbPort, sockFd);

  NSTL1(NULL, NULL, "LINES[%d] present in transAggRtgData.csv", rtgDataMsgQueueAndTablesInfo_obj.odbcVarInfo_obj.linesCount);

  if(rtgDataMsgQueueAndTablesInfo_obj.odbcVarInfo_obj.linesCount)
    insert_data_into_influx_db(sockFd);
}
