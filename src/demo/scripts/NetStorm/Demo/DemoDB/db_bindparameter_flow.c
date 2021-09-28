#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ns_string.h"
#include <sqltypes.h>
#include <sqlext.h>


extern int db_log_result(char *, char *, char *, int);

void db_bindparameter_flow()
{
  void *stmt = NULL;
  char tablename[30], qstr[256];
  char buf[2048];
 
  // parameters for query: "insert into products_0000000000 values(?,?,?,?,?) ;"
  //
  // test=> select * from products_0000000000 ;
  //  product_id |   name    |  category  | quantity | unitprice
  // ------------+-----------+------------+----------+-----------
  //           2 | pillo     | home       |        4 |        20
  //           3 | bed       | home       |        6 |        30
  //           4 | laptop    | appliances |        8 |        40
  //           5 | ludo      | toys       |       10 |        50
  //           6 | maskara   | beauty     |       12 |        60
  //           7 | nailpaint | beauty     |       14 |         5
  //           8 | mobile    | appliances |       10 |     20000
  //           1 | bedsheet  | home       |       26 |        10
 
  long int temp = SQL_NTS;
  long int var_1 = 8; // product_id
  unsigned char var_2[] = "mobile"; //name
  unsigned char var_3[] = "appliances"; // category
  long int var_4 = 10; // quantity
  long int var_5 = 20000; // unitprice

  sprintf(tablename, "products_%04d%06d", ns_get_nvmid(), ns_get_userid());

  // Initialize db environement and variables
  if(ns_db_odbc_init() == -1)
  {
    ns_db_odbc_close();
    return;
  }

  ns_start_transaction("Connect_db_for_bindparameter"); 
  
  // Connect to database with DSN PostgresSQL, username netstorm and password test123
  if(ns_db_connect("DSN=PostgreSQL; Username=netstorm; Datbase=test") == -1)
  {
    db_log_result(tablename, "Connect to DB for bind parameter", "-,-,-,-,-", 1);
    ns_set_tx_status("Connect_db_for_bindparameter", 70);
    ns_db_odbc_close();
    return;
  }
  db_log_result(tablename, "Connect to DB for bind parameter", "-,-,-,-,-", 0);
  
  ns_end_transaction("Connect_db_for_bindparameter", NS_AUTO_STATUS); 

  // Allocate statement handle.
  if(ns_db_alloc_stmt_handle(&stmt) == -1)
  {
    db_log_result(tablename, "Allocate Statement Handle", "-,-,-,-,-", 1);
    ns_db_odbc_close();
    return;
  }
  db_log_result(tablename, "Allocate Statement Handle", "-,-,-,-,-", 0);
  
  ns_start_transaction("Prepare");
  
  // Prepare SQL statement
  sprintf(qstr, "insert into %s values(?,?,?,?,?) ;", tablename); 

  if(ns_db_prepare(stmt, qstr) == -1){
    ns_set_tx_status("Prepare", NS_TX_ERROR);
    fprintf(stderr, "Error in preparing SQL statement: [%s]\n", qstr);
    db_log_result(tablename, "Prepare", "-,-,-,-,-", 1);
  }
  else
    db_log_result(tablename, "Prepare", "-,-,-,-,-", 0);
  
  ns_end_transaction("Prepare", NS_AUTO_STATUS); 


//--------------------------------------------------------
// Prepare SQLBindParameter statement - START
//-------------------------------------------------------
  ns_start_transaction("Bind_parameter");
  
  // Bind 1st Parameter
  if(ns_db_bindparameter(stmt, (unsigned short)1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, (unsigned long)NULL, 0, &var_1, sizeof(var_1), 0) == -1)
  {
    ns_set_tx_status("Bind_parameter", NS_TX_ERROR);
    fprintf(stderr, "Error in binding SQL statement: [%s]\n", qstr);
    db_log_result(tablename, "Bind", "-,-,-,-,-", 1);
  }
  else
  {
    db_log_result(tablename, "Bind", "-,-,-,-,-", 0);
  }

  // Bind 2nd Parameter
  if(ns_db_bindparameter(stmt, (unsigned short)2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR, 50, 0, &var_2, sizeof(var_2), &temp) == -1)
  {
    ns_set_tx_status("Bind_parameter", NS_TX_ERROR);
    fprintf(stderr, "Error in binding SQL statement: [%s]\n", qstr);
    db_log_result(tablename, "Bind", "-,-,-,-,-", 1);
  }
  else
  {
    db_log_result(tablename, "Bind", "-,-,-,-,-", 0);
  }

  // Bind 3rd Parameter
  if(ns_db_bindparameter(stmt, (unsigned short)3, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR, 50, 0, &var_3, sizeof(var_3), &temp) == -1)
  {
    ns_set_tx_status("Bind_parameter", NS_TX_ERROR);
    fprintf(stderr, "Error in binding SQL statement: [%s]\n", qstr);
    db_log_result(tablename, "Bind", "-,-,-,-,-", 1);
  }
  else
  {
    db_log_result(tablename, "Bind", "-,-,-,-,-", 0);
  }

  // Bind 4th Parameter
  if(ns_db_bindparameter(stmt, (unsigned short)4, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, (unsigned long)NULL, 0, &var_4, sizeof(var_4), 0) == -1)
  {
    ns_set_tx_status("Bind_parameter", NS_TX_ERROR);
    fprintf(stderr, "Error in binding SQL statement: [%s]\n", qstr);
    db_log_result(tablename, "Bind", "-,-,-,-,-", 1);
  }
  else
  {
    db_log_result(tablename, "Bind", "-,-,-,-,-", 0);
  }

  // Bind 5th Parameter
  if(ns_db_bindparameter(stmt, (unsigned short)5, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, (unsigned long)NULL, 0, &var_5, sizeof(var_5), 0) == -1)
  {
    ns_set_tx_status("Bind_parameter", NS_TX_ERROR);
    fprintf(stderr, "Error in binding SQL statement: [%s]\n", qstr);
    db_log_result(tablename, "Bind", "-,-,-,-,-", 1);
  }
  else
  {
    db_log_result(tablename, "Bind", "-,-,-,-,-", 0);
  }

  ns_end_transaction("Bind_parameter", NS_AUTO_STATUS); 
  

//-------------------------------------------------------
// Prepare SQLBindParameter statement - END	
//-------------------------------------------------------
  ns_start_transaction("Execute"); 

  // Executing prepared query
  if(ns_db_execute(stmt) == -1)
  {
    ns_set_tx_status("Execute", NS_TX_ERROR);
    fprintf(stderr, "Error in executing query: [%s]\n", qstr);
    db_log_result(tablename, "Execute", "-,-,-,-,-", 1);
  }
  else
  {
    db_log_result(tablename, "Execute", "-,-,-,-,-", 0);
  }



//---------------------------------------------------------------------------------------------//
//			SELECT AFTER INSERT
//---------------------------------------------------------------------------------------------//

 // Executing Select after inserting using BindParameter
  ns_start_transaction("Run_select");

  sprintf(qstr, "select * from %s where product_id = 8;", tablename);

  if(ns_db_execute_direct(stmt, qstr) == -1)
  {
    ns_set_tx_status("Run_select", NS_TX_ERROR);
    fprintf(stderr, "Error in executing query: [%s]\n", qstr);
    db_log_result(tablename, "Select", "-,-,-,-,-", 1);
  }
  else
  {
    printf("NIVE : ns_db_execute_direct for : %s : SUCCESS\n",qstr);
    db_log_result(tablename, "Select", "-,-,-,-,-", 0);
  }


  ns_end_transaction("Run_select", NS_AUTO_STATUS);
  ns_page_think_time(1);
  if(ns_db_get_value(stmt, buf, 2048) == -1){
    fprintf(stderr, "Error in executing query: [%s]\n", qstr);
    db_log_result(tablename, "Get Value after select", "-,-,-,-,-", 1);
  }

  else
    db_log_result(tablename, "Data after Select", buf, 0);




  ns_end_transaction("Execute", NS_AUTO_STATUS); 
  
  ns_page_think_time(1);

  ns_db_free_stmt(stmt);
  ns_db_odbc_close();
}
