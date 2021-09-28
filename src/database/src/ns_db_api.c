
/*******************************************************************
 *
 * File: ns_db_api.c
 * Contains API's for database
 * This file included in ns_string_api.c with #include directive
 *
 * Author     : Manmeet Singh
 * Created on : 8 May 2012
 *
 * Copyright 2012
 *
 ******************************************************************/
#include "ns_db_api.h"
#include "../../ns_log.h"
#include <sql.h>
#include <stdlib.h>
#include <string.h>
#include <sqlext.h>
#include "../../ns_global_settings.h"
#include "../../ns_tls_utils.h"


//static char db_diag_msg[MAX_DB_DIAG_MSG_LEN];

static inline void cleanup(VUser* vptr)
{
  /* free up allocated handles */
  if((SQLHDBC) vptr->httpData->db.dbc != SQL_NULL_HDBC){
    SQLDisconnect((SQLHDBC)vptr->httpData->db.dbc);
    SQLFreeHandle(SQL_HANDLE_DBC, vptr->httpData->db.dbc);
    vptr->httpData->db.dbc = (void *) SQL_NULL_HDBC;
  }
  if((SQLHENV)vptr->httpData->db.env != SQL_NULL_HENV){
    SQLFreeHandle(SQL_HANDLE_ENV, vptr->httpData->db.env);
    vptr->httpData->db.env = (void *) SQL_NULL_HENV;
  }
  //exit (-1);
}

#define log_error(vptr, msg) \
{ \
    NSDL2_API(vptr, NULL, msg); \
    NSEL_CRI(vptr, NULL, ERROR_ID, ERROR_ATTR, msg); \
}

static char *extract_error(char *fn, SQLHANDLE handle, SQLSMALLINT type, char *db_diag_msg)
{
  SQLINTEGER i = 0, native;
  SQLCHAR state[7], text[256];
  SQLSMALLINT len;
  SQLRETURN ret;
  int num_written = 0;

  num_written = sprintf(db_diag_msg, "ODBC driver reported following "
                                      "diagnostics whilst running '%s': ", fn);

  do
  {
    ret = SQLGetDiagRec(type, handle, ++i, state, &native, text,
                          sizeof(text), &len );

    if (SQL_SUCCEEDED(ret))
    {
      num_written = sprintf((char *) db_diag_msg, "%s [state:%s; msg#:%ld; native:%ld; message:", 
                              (char *) db_diag_msg, (char *) state, (long) i, (long)native);

      if((num_written + len) < (MAX_ERR2_LEN - 1))
      {
        num_written += len;
        strcat(db_diag_msg, (char *)text);
      }
      else 
        break;
    }
  }
  while( ret == SQL_SUCCESS );
  return db_diag_msg;
}

/*****************************************************************
 * ns_db_odbc_init()
 *
 * Synopsis: This API initializes the ODBC library. 
 *           Internally it allocates environment handle, 
 *           sets ODBC version to 3, and allocates a connection 
 *           handle.
 *
 * Inputs  : None
 * Returns : 0 on Success, -1 on error
 *
 *****************************************************************/
int ns_db_odbc_init(void)
{
  VUser *vptr = TLS_GET_VPTR();
  
  SQLRETURN ret; /* ODBC API return status */

  NSDL2_API(vptr, NULL, "Method Called");

  vptr->httpData->db.dbc = SQL_NULL_HDBC;
  vptr->httpData->db.env = SQL_NULL_HENV;
 /* Allocate an environment handle */
  ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, (SQLHENV *) &vptr->httpData->db.env);
  if((ret != SQL_SUCCESS_WITH_INFO) && (ret != SQL_SUCCESS)) {
    log_error(vptr, "NS_DB_ODBC_INIT: SQLAllocHandle(Env) Failed");
    //cleanup();
    return -1;
  }
  NSDL2_API(vptr, NULL, "SQLAllocHandle(Env) succeeded");
  /* We want ODBC 3 support */
  ret = SQLSetEnvAttr((SQLHENV) vptr->httpData->db.env, SQL_ATTR_ODBC_VERSION, (void *) SQL_OV_ODBC3, 0);
  if ( (ret != SQL_SUCCESS_WITH_INFO) && (ret != SQL_SUCCESS)) {
    log_error(vptr, "NSQLSetEnvAttr(ODBC version) Failed");
    //cleanup();
    return -1;
  }
  NSDL2_API(vptr, NULL, "SQLSetEnvAttr(ODBC version) succeeded");

  /* Allocate a connection handle */
  ret = SQLAllocHandle(SQL_HANDLE_DBC, (SQLHENV) vptr->httpData->db.env, (SQLHDBC *) &vptr->httpData->db.dbc);
  if ( (ret != SQL_SUCCESS_WITH_INFO) && (ret != SQL_SUCCESS)) {
    log_error(vptr, "SQLAllocHandle(dbc) Failed");
    //cleanup();
    return -1;
  }
  NSDL2_API(vptr, NULL, "SQLAllocHandle(dbc) succeeded");
  return 0;
}

/*****************************************************************
 * ns_db_connect()
 *
 * Synopsis: This API makes a connection to the destination database
 *           using the connection string passed as argument.
 *           Please note that the connection handle is allocated
 *           in ns_db_odbc_init() API itself.
 *           It is mandatory to call the ns_db_odbc_init() before
 *           calling this API.
 *
 * Inputs  : char *conn_str - This is the connection string that is
 *                            used to connect the Database Source
 *                            using ODBC.
 *
 * Returns : 0 on Success, -1 on error
 *
 * Example :
 *
 * if (ns_db_connect("DSN=PostgreSQL; Username=cavisson; Database=test") == -1)
 * {
 *   handle_error();
 * }
 *
 *****************************************************************/
int ns_db_connect(char *conn_str)
{
  SQLCHAR outstr[1024];
  SQLRETURN ret; /* ODBC API return status */
  SQLSMALLINT outstrlen;
  VUser *vptr = TLS_GET_VPTR();  
  
  char errormsg[MAX_ERR_LEN], errormsg2[MAX_ERR2_LEN];

  NSDL2_API(vptr, NULL, "Method Called");

   if((SQLHDBC) vptr->httpData->db.dbc == SQL_NULL_HDBC || (SQLHENV) vptr->httpData->db.env == SQL_NULL_HENV)
  {
    log_error(vptr, "Error: ns_db_odbc_init() not called prior to ns_db_connect()");
    //cleanup();
    return -1;
  }
  
  ret = SQLDriverConnect((SQLHDBC) vptr->httpData->db.dbc, (void *) 1, 
                           (SQLCHAR *)conn_str, SQL_NTS, outstr, 
                           sizeof(outstr), &outstrlen,
                           SQL_DRIVER_COMPLETE);

  if (SQL_SUCCEEDED(ret)) {
    NSDL2_API(vptr, NULL, "Connected to database. returned strring was: [%s]", outstr);
    if (ret == SQL_SUCCESS_WITH_INFO) {
      NSDL2_API(vptr, NULL, "Driver reported the following diagnostics: [%s]",
                extract_error("SQLDriverConnect", (SQLHDBC) vptr->httpData->db.dbc, SQL_HANDLE_DBC, errormsg2));
    }
  } else {
    snprintf(errormsg, MAX_ERR_LEN, "Failed to connect: [%s]", extract_error("SQLDriverConnect", (SQLHDBC) vptr->httpData->db.dbc, SQL_HANDLE_DBC, errormsg2));
    log_error(vptr, errormsg);
    //cleanup();
    return -1;
  }
  NSDL2_API(vptr, NULL, "SQLDriverConnect(dbc) succeeded");
  return 0;
}

/*****************************************************************
 * ns_db_alloc_stmt_handle()
 *
 * Synopsis: This API allocates a handle for executing an SQL statement.
 *           Thi API must be called before making a call to ns_db_execute().
 *
 * Arguments  : void **p_stmt - The API fills the address statement handle
 *                              in this argument.
 *
 * Returns : 0 on Success, -1 on error
 *
 * Example :
 *
 * void *stmt = NULL;
 * if (ns_db_alloc_stmt_handle(&stmt) == -1)
 * {
 *   handle_error();
 * }
 *
 *****************************************************************/
int ns_db_alloc_stmt_handle(void **p_stmt)
{
  SQLRETURN ret; /* ODBC API return status */

  VUser *vptr = TLS_GET_VPTR();
  
  
  NSDL2_API(vptr, NULL, "Method Called");

  if((SQLHDBC) vptr->httpData->db.dbc == SQL_NULL_HDBC || (SQLHENV) vptr->httpData->db.env == SQL_NULL_HENV)
  {
    log_error(vptr, "Error: ns_db_odbc_init() not called prior to ns_db_connect()");
    //cleanup();
    return -1;
  }
  ret = SQLAllocHandle(SQL_HANDLE_STMT, (SQLHDBC) vptr->httpData->db.dbc, (SQLHSTMT *) p_stmt);
  if(SQL_SUCCEEDED(ret))
  {
    NSDL2_API(vptr, NULL, "Successfully allocated stmt handle = %p", *p_stmt);
  }
  else
  {
    log_error(vptr, "Failed to allocate stmt handle");
    //cleanup();
    return -1;
  }
  NSDL2_API(vptr, NULL, "SQLAllocHandle(stmt) succeeded");
  return 0;
}

/*****************************************************************
 * ns_db_execute_direct()
 *
 * Synopsis: This API is used to execute an SQL statement. 
 *           Please note that ns_db_odbc_init(), ns_db_connect() and
 *           ns_db_alloc_stmt_handle() API's must be called in this 
 *           sequence before calling this API.
 *
 * Arguments: void *in_stmt - Statement handle returned by ns_db_alloc_stmt_handle()
 *            char *qstr    - SQL statement to be executed.
 *
 * Returns : 0 on Success, -1 on error
 *
 * Example :
 *
 * if(ns_db_execute_direct(stmt, "update products set quantity = 4 where name = 'bedsheet';") == -1)
 * {
 *   handle_error();
 * }
 *
 *****************************************************************/
int ns_db_execute_direct(void *in_stmt, char *qstr)
{
  RETCODE ret;
  char errormsg[MAX_ERR_LEN + 1];
  SQLHSTMT stmt = (SQLHSTMT) in_stmt;
  VUser *vptr = TLS_GET_VPTR();
  

  NSDL2_API(vptr, NULL, "Method Called");

  if(!qstr || qstr[0] == '\0')
  {
    log_error(vptr, "Error: Empty query string");
    //cleanup();
    return -1;
  }
  if((SQLHDBC) vptr->httpData->db.dbc == SQL_NULL_HDBC || (SQLHENV) vptr->httpData->db.env == SQL_NULL_HENV)
  {
    log_error(vptr, "Error: ns_db_odbc_init() not called prior to ns_db_execute_direct()");
    //cleanup();
    return -1;
  }
  if(stmt == SQL_NULL_HSTMT)
  {
    log_error(vptr, "Error: stmt is NULL");
    //cleanup();
    return -1;
  }

  ret = SQLExecDirect(stmt, (SQLCHAR *) qstr, SQL_NTS);
  if ( (ret != SQL_SUCCESS) && (ret != SQL_SUCCESS_WITH_INFO) ) {
    snprintf(errormsg, 1024, "Error in executing the query: [%s]", qstr);
    errormsg[1024] = '\0';
    log_error(vptr, errormsg);
    //cleanup();
    return -1;
  }

  NSDL2_API(vptr, NULL, "SQLExecDirect(qstr) succeeded");
  return 0;
}

/*****************************************************************
 * ns_db_get_value()
 *
 * Synopsis: This API is used to retrieve the output of the last
 *           SQL command executed using nd_db_execute() API.
 *
 * Arguments: void *in_stmt - Statement handle returned by ns_db_alloc_stmt_handle()
 *            char *retbuf  - This is the buffer in which the output of the query is saved.
 *                            The memory for this buffer can be supplied by the user, in which
 *                            case the 3rd argument should be the size of buffer.
 *                            In case the 3rd argument is zero, a static buffer is populated 
 *                            by the API, which should be copied by the user of this API.
 *            int retbuflen - Length of the buffer if the memory is allocated by the caller. 
 *                            If 0, the return string will be written to a static buffer, which 
 *                            the caller must copy.
 *
 * Returns : 0 on Success, -1 on error
 *
 * Example :
 * char buf[2048];
 * ...
 * ...
 * if(ns_db_get_value(stmt, buf, 2048) == -1)
 * {
 *   handle_error();
 * }
 * printf("Query result: %s\n", buf);
 *
 *****************************************************************/
#define CHK_AND_INCR_LEN(vptr, delta) \
{ \
  if(copied_len + delta > len) \
  { \
    sprintf(errormsg, "Buffer size (%d) is insufficient to hold the query output string. " \
                      "Copying only %d characters\n", len, len); \
    log_error(vptr, errormsg); \
    retbuf = buff; \
    return 0; \
  } \
  else \
  { \
    copied_len += delta; \
  } \
}
int ns_db_get_value(void *in_stmt, char *retbuf, int retbuf_len)
{
  SQLSMALLINT columns;
  RETCODE ret;
  SQLCHAR buf[128][1024];
  SQLINTEGER indicator[128];
  SQLHSTMT stmt = (SQLHSTMT) in_stmt;
 
  VUser *vptr = TLS_GET_VPTR();
  

  char errormsg[MAX_ERR_LEN + 1];
  int i = 0;
  //static char static_buffer[1024] = "";
  int len = 1024, copied_len;
  //char *buff = static_buffer;
  char *buff = NULL;
  
  NSDL2_API(vptr, NULL, "retbuf_len = %d", retbuf_len);

  if((SQLHDBC) vptr->httpData->db.dbc == SQL_NULL_HDBC || (SQLHENV) vptr->httpData->db.env == SQL_NULL_HENV)
  {
    log_error(vptr, "Error: ns_db_odbc_init() not called prior to ns_db_execute_direct()");
    return -1;
  }
  if(stmt == SQL_NULL_HSTMT)
  {
    log_error(vptr, "Error: stmt is NULL");
    return -1;
  }

  if(retbuf_len) /* If buffer is provided by user, use it, else use static buffer */
  {
    len = retbuf_len;
    buff = retbuf;
  }
  else
  {
    fprintf(stderr, "Error: ns_db_get_value() - Output buffer and buffer size must be provided.\n");
    return -1;
  }

  // Find the total no of columns in result row
  ret = SQLNumResultCols(stmt, &columns);
  if ( (ret != SQL_SUCCESS) && (ret != SQL_SUCCESS_WITH_INFO) ) {
    log_error(vptr, "SQLNumResultCols() Failed");
    return -1;
  }
  NSDL2_API(vptr, NULL, "num of columns = %d\n", columns);
  if(columns > 128)
  {
    sprintf(errormsg, "Number of Columns return by the query was large (%d). "
                      "Truncated to 128 columns.\n", columns);
    log_error(vptr, errormsg);

    columns = 128;
  }
  for (i = 0; i < columns; i++) {
    ret = SQLBindCol(stmt, i + 1, SQL_C_CHAR, buf[i], sizeof(buf[i]), (SQLLEN *)&indicator[i]);
    if ((ret != SQL_SUCCESS) && (ret != SQL_SUCCESS_WITH_INFO)) {
      log_error(vptr, "SQLNumResultCols() Failed\n\n");
    return -1;
    }
  }

  buff[0] = '\0';
  copied_len = 1;
  while (SQL_SUCCEEDED(SQLFetch(stmt))) {
    if(buff[0] != '\0')
    {
      CHK_AND_INCR_LEN(vptr, 1);
      strcat(buff, "\n");
    }
    for (i = 0; i < columns; i++) {
      if (indicator[i] == SQL_NULL_DATA) {
        CHK_AND_INCR_LEN(vptr, 5/*strlen("NULL,")*/);
        strcat(buff, "NULL,");
      }
      else {
        CHK_AND_INCR_LEN(vptr, strlen((char *) buf[i]) + 1 /* "," */);
        strcat(buff,(char *) buf[i]);
        strcat(buff, ",");
      }
    }
    if(buff[strlen(buff) -1] == ',')
      buff[strlen(buff) -1] = '\0';
  }
  retbuf = buff;
  return 0;
}


int ns_db_get_value_in_file(void *in_stmt, char *file)
{
  
  SQLSMALLINT columns;
  RETCODE ret;
  SQLCHAR buf[128][1024];
  SQLINTEGER indicator[128];
  SQLHSTMT stmt = (SQLHSTMT) in_stmt;
  
  VUser *vptr = TLS_GET_VPTR();
  
  //char ch;
  char errormsg[MAX_ERR_LEN + 1];
  char file_path[1024];
  int i = 0; 
  //int length = 0;
  //int len = 1024, copied_len;
  FILE *fp;

  NSDL2_API(vptr, NULL, "Method Called");

  if((SQLHDBC) vptr->httpData->db.dbc == SQL_NULL_HDBC || (SQLHENV) vptr->httpData->db.env == SQL_NULL_HENV)
  {
    log_error(vptr, "Error: ns_db_odbc_init() not called prior to ns_db_execute_direct()");
    return -1;
  }
  if(stmt == SQL_NULL_HSTMT)
  {
    log_error(vptr, "Error: stmt is NULL");
    return -1;
  }

  if(file[0] != '/')
    sprintf(file_path, "%s/logs/%s/ns_logs/%s", getenv("NS_WDIR"), global_settings->tr_or_partition, file);
  else
    strcpy(file_path, file);

  NSDL2_API(vptr, NULL, "File to be written in %s", file_path);
    
  fp = fopen(file_path, "a+");
  if(fp == NULL)
  {
    log_error(vptr, "Cannot open file");
    NSDL2_API(vptr, NULL, "Cannot Open file = %s", file_path);
    return -1;
  }
  
  // Find the total no of columns in result row
  ret = SQLNumResultCols(stmt, &columns);
  if ( (ret != SQL_SUCCESS) && (ret != SQL_SUCCESS_WITH_INFO) ) {
    log_error(vptr, "SQLNumResultCols() Failed");
    return -1;
  }
  NSDL2_API(vptr, NULL, "num of columns = %d\n", columns);
  if(columns > 128)
  {
    snprintf(errormsg, MAX_ERR_LEN, "Number of Columns return by the query was large (%d). "
                      "Truncated to 128 columns.\n", columns);
    log_error(vptr, errormsg);

    columns = 128;
  }
  for (i = 0; i < columns; i++) {
    ret = SQLBindCol(stmt, i + 1, SQL_C_CHAR, buf[i], sizeof(buf[i]), (SQLLEN *)&indicator[i]);
    if ((ret != SQL_SUCCESS) && (ret != SQL_SUCCESS_WITH_INFO)) {
      log_error(vptr, "SQLNumResultCols() Failed\n\n");
    return -1;
    }
  }

  //copied_len = 1;
  while (SQL_SUCCEEDED(SQLFetch(stmt))) {

    for (i = 0; i < columns; i++) {
      if (indicator[i] == SQL_NULL_DATA) {
        if(i != (columns -1))
          fprintf(fp, "NULL,");
        else
          fprintf(fp, "NULL");
      }
      else {
       fprintf(fp ,"%s", (char *) buf[i]);
        if(i != (columns -1))
          fprintf(fp, ",");
      }
    }
   fprintf(fp, "\n");
  }

  fclose(fp);
  return 0; 
}


/*****************************************************************
 * ns_db_free_stmt()
 *
 * Synopsis: This API is used to free the statement handle allocated using
 *           ns_db_alloc_stmt_handle() API.
 *
 * Arguments: void *stmt - Statement handle returned by ns_db_alloc_stmt_handle()
 *
 * Returns : 0 on Success, -1 on error
 *
 * Example :
 *
 * if(ns_db_free_stmt(stmt) == -1)
 * {
 *   handle_error();
 * }
 *
 *****************************************************************/
int ns_db_free_stmt(void *stmt)
{
 if((SQLHSTMT) stmt != SQL_NULL_HSTMT){
    SQLFreeHandle(SQL_HANDLE_STMT, (SQLHSTMT) stmt);
    stmt = (void *) SQL_NULL_HSTMT;
  }
  return 0;
}

/*****************************************************************
 * ns_db_odbc_close()
 *
 * Synopsis: This API is used to free the environment handle and database
 *           connection handle, which are allocated internally at the 
 *           time of ns_db_odbc_init() API.
 *
 * Arguments: None
 *
 * Returns : 0 on Success, -1 on error
 *
 * Example :
 *
 * if(ns_db_odbc_close() == -1)
 * {
 *   handle_error();
 * }
 *
 *****************************************************************/

int ns_db_odbc_close(void)
{
  VUser *vptr = TLS_GET_VPTR();
  
  NSDL2_API(vptr, NULL, "Method called.");
  cleanup(vptr);
  return 0;
}

/*****************************************************************
 * ns_db_prepare()
 *
 * Synopsis: This API is used to prepare an executable SQL statement. 
 *           Please note that ns_db_odbc_init(), ns_db_connect() and
 *           ns_db_alloc_stmt_handle() API's must be called in this 
 *           sequence before calling this API.
 *
 * Arguments: void *in_stmt - Statement handle returned by ns_db_alloc_stmt_handle()
 *            char *qstr    - SQL statement to be executed.
 *
 * Returns : 0 on Success, -1 on error
 *
 * Example :
 *
 * if(ns_db_prepare(stmt, "update products set quantity = 4 where name = ? ;") == -1)
 * {
 *   handle_error();
 * }
 *
 *****************************************************************/
int ns_db_prepare(void *in_stmt, char *qstr)
{
  char errormsg[MAX_ERR_LEN + 1], errormsg2[MAX_ERR2_LEN + 1];
  SQLHSTMT stmt = (SQLHSTMT) in_stmt;
  SQLRETURN   rc;
  VUser *vptr = TLS_GET_VPTR();
  

  NSDL2_API(vptr, NULL, "Method Called");

  if(!qstr || qstr[0] == '\0')
  {
    log_error(vptr, "Error: Empty query string");
    return -1;
  }

  if((SQLHDBC) vptr->httpData->db.dbc == SQL_NULL_HDBC || (SQLHENV) vptr->httpData->db.env == SQL_NULL_HENV)
  {
    log_error(vptr, "Error: ns_db_odbc_init() not called prior to ns_db_prepare()");
     return -1;
  }

  if(stmt == SQL_NULL_HSTMT)
  {
    log_error(vptr, "Error: stmt is NULL");
    return -1;
  }

  /* prepare statement for multiple use */
  rc = SQLPrepare(stmt, (SQLCHAR *) qstr, SQL_NTS);
  if ((rc != SQL_SUCCESS) && (rc != SQL_SUCCESS_WITH_INFO))
  {
    snprintf(errormsg, MAX_ERR_LEN, "Error in Preparing the query, msg = %s", 
            extract_error("SQLPrepare", (SQLHDBC) vptr->httpData->db.dbc, SQL_HANDLE_DBC, errormsg2));
    log_error(vptr, errormsg);
    return -1;
  }

  NSDL2_API(vptr, NULL, "SQLPrepare succeeded");
  return 0;

}

/*****************************************************************
 * ns_db_execute()
 *
 * Synopsis: This API is used to execute the SQL statement 
 *           which has been prepared by ns_db_prepare() and ns_db_bindparameter().
 *           Through this api, the SQL statement will be compiled only once and 
 *           can be executed again and again hence minimizing the execution time 
 *           required by the same-looking queries. 
 *           Please note that ns_db_odbc_init(), ns_db_connect(), 
 *           ns_db_alloc_stmt_handle(), ns_db_prepare() and ns_db_bindparameter() 
 *            API's must be called in this sequence before calling this API.
 *
 * Arguments: void *in_stmt - Statement handle returned by ns_db_alloc_stmt_handle()
 *
 * Returns : 0 on Success, -1 on error
 *
 * Example :
 *
 * if(ns_db_prepare(stmt == -1)
 * {
 *   handle_error();
 * }
 *
 *****************************************************************/

int ns_db_execute(void *in_stmt)
{
  RETCODE ret;
  char errormsg[MAX_ERR_LEN + 1], errormsg2[MAX_ERR2_LEN + 1];
  SQLHSTMT stmt = (SQLHSTMT) in_stmt;

  VUser *vptr = TLS_GET_VPTR();
  

  NSDL2_API(vptr, NULL, "Method Called");

  if((SQLHDBC) vptr->httpData->db.dbc == SQL_NULL_HDBC || (SQLHENV) vptr->httpData->db.env == SQL_NULL_HENV)
  {
    log_error(vptr, "Error: ns_db_odbc_init() not called prior to ns_db_execute()");
    return -1;
  }
  if(stmt == SQL_NULL_HSTMT)
  {
    log_error(vptr, "Error: stmt is NULL");
    return -1;
  }

  // Execute the prepared statement from ns_db_prepare()
  ret = SQLExecute(stmt);
  if ( (ret != SQL_SUCCESS) && (ret != SQL_SUCCESS_WITH_INFO) ) {
    snprintf(errormsg, MAX_ERR_LEN, "Error in executing the query, msg = %s", 
            extract_error("SQLExecute", (SQLHDBC) vptr->httpData->db.dbc, SQL_HANDLE_DBC, errormsg2));
    log_error(vptr, errormsg);
    return -1;
  }

  NSDL2_API(vptr, NULL, "SQLExecute succeeded");
  return 0;
}


// Execute the prepared statement from ns_db_prepare

/*****************************************************************
 * ns_db_bindparameter()
 *
 * Synopsis: This API is binds a buffer to a parameter marker 
 *           in an SQL statement.
 *           Please note that ns_db_odbc_init(), ns_db_connect(), 
 *           ns_db_alloc_stmt_handle() and ns_db_prepare() API's 
 *           must be called in this sequence before calling this API.
 *
 * Arguments: void *in_stmt            - Statement handle returned by ns_db_alloc_stmt_handle()
 *            unsigned short p_no      - Parameter number, ordered sequentially in increasing 
 *            signed short int io_type - The type of the parameter
 *            signed short int v_type  - The C data type of the parameter. 
 *            signed short int p_type  - The SQL data type of the parameter. 
 *            unsigned long col_size   - The size of the column or expression of the corresponding
 *                                       parameter marker.      
 *            signed short int d_digit - The decimal digits of the column or expression of the 
 *                                       corresponding parameter marker. 
 *            void *p_value_ptr        - A pointer to a buffer for the parameter's data
 *            long buf_len             - Length of the p_value_ptr buffer in bytes. 
 *            long *strlen_indptr      - A pointer to a buffer for the parameter's length
 *
 * Returns : 0 on Success, -1 on error
 *
 * Example :
 *
 * If(ns_db_bindprepare(hstmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR, 10, 0, department, 0, &plength) == -1)
 *   {
 *    handle_error();
 *   }
 *
 *****************************************************************/

int ns_db_bindparameter(void             *in_stmt, 
                        unsigned short   p_no, 
                        signed short int io_type, 
                        signed short int v_type, 
                        signed short int p_type, 
                        unsigned long    col_size, 
                        signed short int d_digit, 
                        void             *p_value_ptr, 
                        long             buf_len, 
                        long             *strlen_indptr)
{
  char errormsg[MAX_ERR_LEN + 1], errormsg2[MAX_ERR2_LEN + 1];
  SQLHSTMT stmt = (SQLHSTMT) in_stmt;
  SQLRETURN   rc;
 
  VUser *vptr = TLS_GET_VPTR();
  
    
  NSDL2_API(vptr, NULL, "Method Called");

  if((SQLHDBC) vptr->httpData->db.dbc == SQL_NULL_HDBC || (SQLHENV) vptr->httpData->db.env == SQL_NULL_HENV)
  {
    log_error(vptr, "Error: ns_db_odbc_init() not called prior to ns_db_bindparameter()");
     return -1;
  }
  if(stmt == SQL_NULL_HSTMT)
  {
    log_error(vptr, "Error: stmt is NULL");
    return -1;
  }

  /* Bind the parameters here after preparing the query using nd_db_prepare() */
  rc = SQLBindParameter(in_stmt, p_no, io_type, v_type, 
                        p_type, col_size, d_digit, p_value_ptr, 
                        buf_len, strlen_indptr);

  if ((rc != SQL_SUCCESS) && (rc != SQL_SUCCESS_WITH_INFO))
  {
    snprintf(errormsg, MAX_ERR_LEN, "Error: SQLBindParameter failed, message = %s", 
            extract_error("SQLBindParameter", (SQLHDBC) vptr->httpData->db.dbc, SQL_HANDLE_DBC, errormsg2));
    log_error(vptr, errormsg);
    return -1;
  }

  NSDL2_API(vptr, NULL, "SQLBindParameter succeeded");
  return 0;

}
