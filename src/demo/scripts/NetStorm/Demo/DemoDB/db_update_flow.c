
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ns_string.h"

extern int db_log_result(char *, char *, char *, int);
void db_update_flow()
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

  ns_start_transaction("Connect_db_for_update"); 
  
  // Connect to database with DSN PostgresSQL, username netstorm and password test123
  if(ns_db_connect("DSN=PostgreSQL; Username=netstorm; Datbase=test") == -1)
  {
    db_log_result(tablename, "Connect to DB for update", "-,-,-,-,-", 1);
    ns_set_tx_status("Connect_db_for_update", 70);
    ns_db_odbc_close();
    return;
  }
  db_log_result(tablename, "Connect to DB for update", "-,-,-,-,-", 0);
  
  ns_end_transaction("Connect_db_for_update", NS_AUTO_STATUS); 

  // Allocate statement handle.
  if(ns_db_alloc_stmt_handle(&stmt) == -1)
  {
    db_log_result(tablename, "Allocate Statement Handle", "-,-,-,-,-", 1);
    ns_db_odbc_close();
    return;
  }
  db_log_result(tablename, "Allocate Statement Handle", "-,-,-,-,-", 0);
  
  ns_start_transaction("Run_update");
  
  // Executing select query
  sprintf(qstr, "update %s set quantity = 26 where name = 'bedsheet';", tablename); 

  if(ns_db_execute_direct(stmt, qstr) == -1){
    ns_set_tx_status("Run_update", NS_TX_ERROR);
    fprintf(stderr, "Error in executing query: [%s]\n", qstr);
    db_log_result(tablename, "ExecuteDirect", "-,-,-,-,-", 1);
  }
  else
    db_log_result(tablename, "ExecuteDirect", "-,-,-,-,-", 0);
  
  ns_end_transaction("Run_update", NS_AUTO_STATUS); 
  ns_page_think_time(1);

  ns_db_free_stmt(stmt);
  ns_db_odbc_close();

}


