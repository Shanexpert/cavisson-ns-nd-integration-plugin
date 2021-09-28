
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ns_string.h"

extern int db_log_result(char *, char *, char *, int);
void db_create_flow()
{
  void *stmt = NULL;
  char tablename[30], qstr[256];
  sprintf(tablename, "products_%04d%06d", ns_get_nvmid(), ns_get_userid());

  // Initialize db environement and variables
  if(ns_db_odbc_init() == -1)
  {
    ns_db_odbc_close();
    return;
  }

  ns_start_transaction("Connect_db_for_create");
  
  // Connect to database with DSN PostgresSQL, username netstorm and Database test
  if(ns_db_connect("DSN=PostgreSQL; Username=netstorm; Database=test") == -1)
  {
    db_log_result(tablename, "Connect to DB for create", "-,-,-,-,-", 1);
    ns_set_tx_status("Connect_db_for_create", 70);
    ns_db_odbc_close();
    return;
  }
  db_log_result(tablename, "Connect to DB for create", "-,-,-,-,-", 0);

  ns_end_transaction("Connect_db_for_create", NS_AUTO_STATUS); 

  // Allocate statement handle.
  if(ns_db_alloc_stmt_handle(&stmt) == -1)
  {
    db_log_result(tablename, "Allocate Statement Handle", "-,-,-,-,-", 1);
    ns_db_odbc_close();
    return;
  }
  db_log_result(tablename, "Allocate Statement Handle", "-,-,-,-,-", 0);

  ns_start_transaction("Run_create");
  
  // Executing drop table command
  sprintf(qstr, "drop table if exists %s;", tablename); 
  if(ns_db_execute_direct(stmt, qstr) == -1){
    ns_set_tx_status("Run_create", NS_TX_ERROR);
    fprintf(stderr, "Error in executing query: [%s]\n", qstr);
    db_log_result(tablename, "Drop Table", "-,-,-,-,-", 1);
  }
  else
    db_log_result(tablename, "Drop Table", "-,-,-,-,-", 0);

  // Executing create table command
  sprintf(qstr, "create table %s (product_id int, name varchar(50), category varchar(50), quantity int, unitprice int);", tablename); 

  if(ns_db_execute_direct(stmt, qstr) == -1){
    ns_set_tx_status("Run_create", NS_TX_ERROR);
    fprintf(stderr, "Error in executing query: [%s]\n", qstr);
    db_log_result(tablename, "Create Table", "-,-,-,-,-", 1);
  }
  
  else
    db_log_result(tablename, "Create Table", "-,-,-,-,-", 0);

  ns_end_transaction("Run_create", NS_AUTO_STATUS); 
  ns_page_think_time(1);

  ns_db_free_stmt(stmt);
  ns_db_odbc_close();

}


