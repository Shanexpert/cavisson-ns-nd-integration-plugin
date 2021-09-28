#include <stdio.h>
#include <libpq-fe.h>
#include <stdlib.h>
#include <string.h>
#include "cdr_log.h"

typedef struct table_size
{
  float tableSize;
  float tableSize_including_indexes;
}testRun_tableSize;

static void close_dbConn(PGconn *dbConn)
{
  PQfinish(dbConn);
}

static void clear_res_and_close_dbConn(PGconn *dbConn, PGresult *res)
{
  PQclear(res);
  close_dbConn(dbConn);
}

void make_query_and_drop_tables(int test_run, long partition)
{
  char Query[1024];
  char tablename[256];
  PGconn *dbConn;
  PGresult *res;
  int malloced_size = 2048;
  char connectionString[] = ("user=cavisson dbname=test");

  /*make connection to POSTGRES db*/
  dbConn = PQconnectdb(connectionString); 

  if(PQstatus(dbConn) == CONNECTION_BAD)
  {
    printf("ERROR[%s] while making connection to database with args[%s]", PQerrorMessage(dbConn), connectionString);
    
    close_dbConn(dbConn);

    return ;
  }
  
  /*if(!test_run)
  {
    printf("please provide TEST RUN");
    close_dbConn(dbConn);
    return ;
  }*/
 
  if(!partition)
  {
    sprintf(tablename, "\\_%d", test_run);
  }
  else if (!test_run)
  { 
    sprintf(tablename, "\\_%ld_", partition);
  }
  else 
  {
    sprintf(tablename, "\\_%d_%ld", test_run, partition);
  }

  sprintf(Query, "select table_name from information_schema.tables where table_name ilike  '%%%s%%';", tablename);
  res = PQexec(dbConn, Query);

  if(PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    printf("Query[%s] execution failed with ERROR[%s]", Query, PQerrorMessage(dbConn));
   
    clear_res_and_close_dbConn(dbConn, res);
    
    return ;
  }

  /*ZERO rows is returned */
  int rows = PQntuples (res);
 
  if(rows <= 0)
  {
    CDRTL4("Query[%s] executed successfully but no row is returned", Query);
    
    clear_res_and_close_dbConn(dbConn, res);
   
    return ;
  }  

  int deleteTableQuery_currOff = 0;
  char *deleteTableQuery = NULL;

  deleteTableQuery = malloc(malloced_size);
    
  deleteTableQuery_currOff += sprintf(deleteTableQuery + deleteTableQuery_currOff, "drop table if exists ");

  for(int rowIdx = 0; rowIdx < rows; rowIdx++)
  {
    if(malloced_size - deleteTableQuery_currOff <= strlen(PQgetvalue(res, rowIdx, 0)) + 1)
    //if(deleteTableQuery_currOff >= malloced_size)
    {
      malloced_size += malloced_size;
      deleteTableQuery = realloc(deleteTableQuery, malloced_size);
      if(!deleteTableQuery)
      {
        printf("Realloc fails exit");
        exit(-1);
      }
    }

    deleteTableQuery_currOff += sprintf(deleteTableQuery + deleteTableQuery_currOff, "%s,", PQgetvalue(res, rowIdx, 0));
  }

  if(deleteTableQuery_currOff)
  {
    deleteTableQuery[deleteTableQuery_currOff - 1] = '\0';
  }

  //printf("executing deleteTableQuery[%s]\n", deleteTableQuery);

  PQclear(res);
  res = PQexec(dbConn, deleteTableQuery);

  if(PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    printf("ERROR[%s] while executing Query\n", PQerrorMessage(dbConn)); 
  }
  PQclear(res);
}

#if 0
testRun_tableSize * testRun_tableSize_with_indexes_size(int test_run, long partition)
{
  char Query[1024];
  char tablename[256];
  PGconn *dbConn;
  PGresult *res;
  char connectionString[] = ("user=cavisson dbname=test");

  /*make connection to POSTGRES db*/
  dbConn = PQconnectdb(connectionString); 

  if(PQstatus(dbConn) == CONNECTION_BAD)
  {
    printf("ERROR[%s] while making connection to database with args[%s]", PQerrorMessage(dbConn), connectionString);
    
    close_dbConn(dbConn);

    return NULL;
  }
  
  if(!test_run)
  {
    printf("please provide TEST RUN");
    close_dbConn(dbConn);
    return NULL;
  }
 
  if(!partition)
  {
    sprintf(tablename, "\\_%d", test_run);
  }
  else
  { 
    sprintf(tablename, "\\_%d_%ld", test_run, partition);
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
                      " table_name ilike '%%%s%%'\n"
                   " )as inn\n"
                ")as innerr;", tablename);
  
  printf("QUERY[%s]", Query);

  res = PQexec(dbConn, Query);

  if(PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    printf("Query[%s] execution failed with ERROR[%s]", Query, PQerrorMessage(dbConn));
   
    clear_res_and_close_dbConn(dbConn, res);
    
    return NULL;
  }

  /*ZERO rows is returned */
  if(!PQntuples(res))
  {
    CDRTL4("Query[%s] executed successfully but no row is returned", Query);
    
    clear_res_and_close_dbConn(dbConn, res);
   
    return NULL;
  }  

  testRun_tableSize *size = malloc(sizeof(testRun_tableSize));

  if(!size)
  {
    printf("Error in malloc");
    return NULL; 
  }

  if(PQgetvalue(res, 0, 0))
  {
    size -> tableSize = atof(PQgetvalue(res, 0, 0));
  }
  
  if(PQgetvalue(res, 0, 0))
  {
    size -> tableSize_including_indexes = atof(PQgetvalue(res, 0, 1));
  }
  
  clear_res_and_close_dbConn(dbConn, res);
  
  return size;
}

int main()
{

 /*testRun_tableSize *test_run_size = testRun_tableSize_with_indexes_size(1938, 20200619125509);

 if(test_run_size)
 { 
   printf("table_size[%.2f]", test_run_size ->tableSize);
   printf("table_size_including_indexes_in_kb[%.2f]", test_run_size ->tableSize_including_indexes);
  
   free(test_run_size);
 }*/


 make_query_and_drop_tables (1875, 20181220130204);
 //make_query_and_drop_tables (1875, 0);
 return 0;
}
#endif
