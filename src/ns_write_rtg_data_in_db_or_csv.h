#ifndef NS_WRITE_RTG_DATA_IN_DB_OR_CSV_H
#define NS_WRITE_RTG_DATA_IN_DB_OR_CSV_H
 
#include "nslb_mem_pool.h"
#include "nslb_odbc_driver_connect.h"

/* rtgDataMsgMpoolInfo is a linked list of rtgDataMsgInfo structs */
typedef struct{
  NSLB_MP_COMMON
  int msgMallocSize;
  int msgLen;
  char *msg;
  char msgType;
} rtgDataMsgInfo;

/*msg queue info*/
typedef struct{
  int msgQueInitSize;
  int msgQueMaxSize;
  nslb_mp_handler *rtgDataMsgMpoolInfo;
} rtgDataMsgQueueInfo;

/*tables info*/
typedef struct {
  char *tablename;
  char *schema;
} rtgTablesInfo;
  
  /*influxDb handling*/
typedef struct{
  
  char influxDbIP[128];
  int influxDbPort;
  char dbName[1024];
  char userName[1024];
  char passwd[1024];
}influxDbInfo;

typedef struct {

  /*buffer to make query and insert into tables*/
  char *queryBuffer;
  int queryBufferMallocSize;

  /*tables info*/
  int totalTables;
  int mallocTables;
  rtgTablesInfo *rtgTablesInfo_ptr;

  /*odbc conn info*/
  odbcVarInfo odbcVarInfo_obj;
  char odbcConnString[512];

  /*msg queue info*/
  rtgDataMsgQueueInfo rtgDataMsgQueueInfo_obj;

  sem_t mutex;

  /*buffer to write msg in queue*/
  char *rtgDataBuffer;
  int rtgDataBufferMallocSize;

  /*csv info which is to be used if msg queue is full*/
  int csv_fd;
  long long curPartition;
  char csvFileNameWithPath[1024];

  /*contains insert queries if not able to write data in db*/
  int db_fd;
  char dbFileNameWithPath[1024];

  char writeInCsvOrDb;  /*1 means write in CSV and 2 means in DB*/

  influxDbInfo influxDbInfo_obj;

} rtgDataMsgQueueAndTablesInfo;

enum rtgTableNameMappingWithSchema
{
  TRANSACTION_STATS=0
};

enum writeInCsvOrDb
{
  CSV_WRITE_MODE=0,
  DB_WRITE_MODE
};

extern void msg_queue_creation_for_writing_rtg_data_in_db_or_csv();
extern void ns_upload_rtg_data_in_db_or_csv_thread_creation();
extern rtgDataMsgQueueAndTablesInfo rtgDataMsgQueueAndTablesInfo_obj;
extern int write_rtg_data_in_csv(char *rtgDataBuffer, int rtgDataBufferLen, char *errorMsg, int errorMsgLen);
extern void write_transaction_aggregate_data_in_csv();
extern void make_conn_and_insert_data_into_influx_db();

#endif
