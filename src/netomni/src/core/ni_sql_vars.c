/********************************************************************************************************************
 * File Name      : ni_sql_var.c                                                                                    |
 |                                                                                                                  | 
 * Synopsis       : This file contain all the functions which take part in SQL Parameter                            | 
 |                                                                                                                  |
 * Author(s)      : Shibani                                                                                         |
 |                  Shibani:                                                                                        |
 |                    (1) Add function for API nsl_sql_var() parsing                                                |
 |                    (2) Add function for DataBase conncection                                                     |
 |                    (3) Add function for DataBase query execute                                                   |
 |                                                                                                                  |
 * Date           : Wed Sep 27 13:16:58 IST 2017                                                                    |
 |                                                                                                                  |
 * Copyright      : (c) Cavisson Systems                                                                            |
 |                                                                                                                  |
 * Mod. History   :                                                                                                 |
 *******************************************************************************************************************/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sql.h>
#include <sqlext.h>
#include <sys/socket.h>
#include <errno.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <libgen.h>
#include <ctype.h>

#include "ni_sql_vars.h"
#include "ni_script_parse.h"
#include "nslb_sock.h"
#include "ni_scenario_distribution.h"
#include "../../../ns_exit.h"
#include "../../../ns_error_msg.h"

static char db_diag_msg[NI_SQL_PARAM_MAX_DB_DIAG_MSG_LEN];

static int is_numeric(char *str)
{
  int i;
  for(i = 0; i < strlen(str); i++) {
    if(!isdigit(str[i])) 
      return 0;
  }
  return 1;
}

static int set_buffer_for_sql_param_host(char* value, char* host, char* script_file_msg, int* state, short int* done)
{
  NIDL(1, "Method Called, value = [%s]", value);
   
  if (*state == NI_ST_STAT_VAR_NAME) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "SQL");
  }
  
  if ((*done & NI_SQL_PARAM_HOST_PARSING_DONE)) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012036_ID, CAV_ERR_1012036_MSG, "Host", "SQL");
  }

  if(value[0] == '\0'){
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, "Host", "SQL");
  }
  strcpy(host, value);
 
  *state = NI_ST_STAT_VAR_OPTIONS;
  *done |= NI_SQL_PARAM_HOST_PARSING_DONE;

  return 0; 
}

static int set_buffer_for_sql_param_port(char* value, char* port, char* script_file_msg, int* state, short int* done)
{
  NIDL(1, "Method Called, value = [%s]", value);

  if (*state == NI_ST_STAT_VAR_NAME) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "SQL");
  }

  if ((*done & NI_SQL_PARAM_PORT_PARSING_DONE)) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012036_ID, CAV_ERR_1012036_MSG, "Port", "SQL");
  }

  if(value[0] == '\0'){
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, "Port", "SQL");
  }

  if(!is_numeric(value)){
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012043_ID, CAV_ERR_1012043_MSG, "Port");
  }
  strcpy(port, value);

  *state = NI_ST_STAT_VAR_OPTIONS;
  *done |= NI_SQL_PARAM_PORT_PARSING_DONE;

  return 0;
}

static int set_sql_param_driver_name(char* value, NI_SQLVarTableEntry* sqlVarTable, char* script_file_msg, int* state, short int* done)
{
  NIDL(1, "Method Called, value = [%s]", value);
  
  if (*state == NI_ST_STAT_VAR_NAME) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "SQL");
  }
  
  if ((*done & NI_SQL_PARAM_DRIVER_PARSING_DONE)) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012036_ID, CAV_ERR_1012036_MSG, "Driver", "SQL");
  }
  
  if(value[0] == '\0'){
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, "Driver", "SQL");
  }

  strcpy(sqlVarTable->driver, value); 
  NIDL (1, "Driver name = %s", sqlVarTable->driver);

  *state = NI_ST_STAT_VAR_OPTIONS;   
  *done |= NI_SQL_PARAM_DRIVER_PARSING_DONE;
 
  return 0;  
}

static int set_sql_param_user_name(char* value, NI_SQLVarTableEntry* sqlVarTable, char* script_file_msg, int* state, short int* done)
{
  NIDL(1, "Method Called, value = [%s]", value);  

  if (*state == NI_ST_STAT_VAR_NAME) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "SQL");
  }  

  if ((*done & NI_SQL_PARAM_USER_PARSING_DONE)) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012036_ID, CAV_ERR_1012036_MSG, "User", "SQL");
  }

  if(value[0] == '\0'){
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, "User", "SQL");
  }

  strcpy(sqlVarTable->user, value);
  NIDL (1, "User name = %s", sqlVarTable->user);

  *state = NI_ST_STAT_VAR_OPTIONS;
  *done |= NI_SQL_PARAM_USER_PARSING_DONE;  

  return 0;
}

static int set_sql_param_passwd(char* value, NI_SQLVarTableEntry* sqlVarTable, char* script_file_msg, int* state, short int* done)
{
  NIDL(1, "Method Called, value = [%s]", value);

  if (*state == NI_ST_STAT_VAR_NAME) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "SQL");
  }

  if ((*done & NI_SQL_PARAM_PASSWD_PARSING_DONE)) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012036_ID, CAV_ERR_1012036_MSG, "PassWord", "SQL");
  }
 
  if(value[0] == '\0'){
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, "PassWord", "SQL");
  }

  strcpy(sqlVarTable->passwd, value); 
 
  NIDL(1, "Password = %s", sqlVarTable->passwd);

  *state = NI_ST_STAT_VAR_OPTIONS;
  *done |= NI_SQL_PARAM_PASSWD_PARSING_DONE;

  return 0; 
}

int ni_set_sql_odbc_con_attr(NI_SQLVarTableEntry* sqlVarTable, NI_SQLConAttr* sqlAttr){
  char file_name[256]={0};
  FILE *sql_fp;
  sprintf(file_name,"/home/cavisson/thirdparty/sqldriver/%s",sqlVarTable->driver);
  NIDL (2, "Opening database '%s' ODBC attributes file '%s'", sqlVarTable->driver, file_name);
  sql_fp = fopen(file_name,"r");
  if (sql_fp){
    int len=0;
    char attr_buf[128]={0};
    while (nslb_fgets(attr_buf,128,sql_fp, 0)!=NULL ){
      len = strlen(attr_buf);
      attr_buf[len-1]='\0';
      if ((!strncmp(attr_buf,"DSN",3))){
        sprintf(sqlAttr->dsn,"%s", attr_buf+4);
      }
      else if((!strncmp(attr_buf,"HOST",4))){
        sprintf(sqlAttr->host,"%s", attr_buf+5);
      }
      else if((!strncmp(attr_buf,"USER",4))){
        sprintf(sqlAttr->user,"%s", attr_buf+5);
      }
      else if((!strncmp(attr_buf,"PORT",4))){
        sprintf(sqlAttr->port,"%s", attr_buf+5);
      }
      else if((!strncmp(attr_buf,"PASSWORD",8))){
        sprintf(sqlAttr->password,"%s", attr_buf+9);
      }
      else if((!strncmp(attr_buf,"DATABASE",8))){
        sprintf(sqlAttr->database,"%s", attr_buf+9);
      }
      NIDL (2, "DSN attribute name = %s, HOST attribute name = %s, USER attribute name = %s, PORT attribute name = %s, PASSWORD attribute name = %s, DATABASE attribute name = %s", sqlAttr->dsn, sqlAttr->host, sqlAttr->user, sqlAttr->port, sqlAttr->password, sqlAttr->database);
    }
    fclose(sql_fp);
  }
  else{
    NIDL (2, "Error in opening driver file '%s'.",file_name);
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012083_ID, CAV_ERR_1012083_MSG, sqlVarTable->driver);
  }
return 0;
}

static int set_sql_param_db_name(char* value, NI_SQLVarTableEntry* sqlVarTable, char* script_file_msg, int* state, short int* done)
{
  NIDL (1, "Method Called, value = [%s]", value);

  if (*state == NI_ST_STAT_VAR_NAME) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "SQL");
  }

  if ((*done & NI_SQL_PARAM_DATABASE_PARSING_DONE)) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012036_ID, CAV_ERR_1012036_MSG, "DataBase", "SQL");
  }

  if(value[0] == '\0'){
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, "DataBase", "SQL");
  }

  strcpy(sqlVarTable->db_name, value);
  NIDL (1, "Database name = %s", sqlVarTable->db_name);
   
  *state = NI_ST_STAT_VAR_OPTIONS;
  *done |= NI_SQL_PARAM_DATABASE_PARSING_DONE;

  return 0;
}

static int set_sql_param_query(char* value, NI_SQLVarTableEntry* sqlVarTable, char* script_file_msg, int* state, short int* done)
{
  NIDL (1, "Method Called, value = [%s]", value);
  
  if (*state == NI_ST_STAT_VAR_NAME) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "SQL");
  }

  if ((*done & NI_SQL_PARAM_QUERY_PARSING_DONE)) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012036_ID, CAV_ERR_1012036_MSG, "Query", "SQL");
  }

  if(value[0] == '\0'){
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, "Query", "SQL");
  }

  //sqlVarTable->query = (char*) malloc(strlen(value) + 1);
  NSLB_MALLOC(sqlVarTable->query, (strlen(value) + 1), "sql var entry query", -1, NULL);
  strcpy(sqlVarTable->query, value);
 
  NIDL (1, "Query =  %s", sqlVarTable->query); 
   
  *state = NI_ST_STAT_VAR_OPTIONS;
  *done |= NI_SQL_PARAM_QUERY_PARSING_DONE;

  return 0;
}

static int set_sql_param_save_into_file(char* value, NI_SQLVarTableEntry* sqlVarTable, char* script_file_msg, int* state, short int* done)
{
  NIDL (1, "Method Called, value = [%s]", value);

  if (*state == NI_ST_STAT_VAR_NAME) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "SQL");
  }
 
  if ((*done & NI_SQL_PARAM_SAVEINTOFILE_PARSING_DONE)) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012036_ID, CAV_ERR_1012036_MSG, "IsSaveIntoFile", "SQL");
  }

  if(value[0] != '\0')
    strcpy(sqlVarTable->is_save_into_file, value);
  else
    strcpy(sqlVarTable->is_save_into_file, "NO");

  if((strcasecmp(sqlVarTable->is_save_into_file, "Yes")) && (strcasecmp(sqlVarTable->is_save_into_file, "NO")))
  {
    NIDL (1, "%s  Value of filed 'IsSaveIntoFile' is '%s'. It's value can be either Yes or No.\n", sqlVarTable->is_save_into_file);
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012027_ID, CAV_ERR_1012027_MSG, sqlVarTable->is_save_into_file, 
                               "IsSaveIntoFile", "SQL");
  }

  NIDL(1, "IsSaveIntoFile =  %s", sqlVarTable->is_save_into_file);  

  *state = NI_ST_STAT_VAR_OPTIONS;
  if((!strcasecmp(sqlVarTable->is_save_into_file, "Yes")))
    *done |= NI_SQL_PARAM_SAVEINTOFILE_PARSING_DONE;

  return 0;
}


int ni_parse_sql_params(char* line_tok, char* value, NI_SQLVarTableEntry* sqlVarTable, int* state, char* script_file_msg, char* host, char* port, short int* done, int* api_idx, int* create_group) 
{
  int exitStatus;

  NIDL(1, "Method called");

  if(!strcasecmp(line_tok, "Host"))
  {
    set_buffer_for_sql_param_host(value, host, script_file_msg, state, done);
  }
  else if(!strcasecmp(line_tok, "Port"))
  {
    set_buffer_for_sql_param_port(value, port, script_file_msg, state, done);
  }
  else if(!strcasecmp(line_tok, "Driver"))
  {
    set_sql_param_driver_name(value, sqlVarTable, script_file_msg, state, done);
  }
  else if(!strcasecmp(line_tok, "User"))
  {
    set_sql_param_user_name(value, sqlVarTable, script_file_msg, state, done);
  }
  else if(!strcasecmp(line_tok, "PassWord"))
  {
    set_sql_param_passwd(value, sqlVarTable, script_file_msg, state, done);
  }
  else if(!strcasecmp(line_tok, "DataBase"))
  {
    set_sql_param_db_name(value, sqlVarTable, script_file_msg, state, done);
  }
  else if(!strcasecmp(line_tok, "Query"))
  {
    set_sql_param_query(value, sqlVarTable, script_file_msg, state, done);
  }
  else if(!strcasecmp(line_tok, "IsSaveIntoFile"))
  {
    set_sql_param_save_into_file(value, sqlVarTable, script_file_msg, state, done);
  }
  else
  {
    if ((exitStatus = ni_set_last_option(api_idx, state, create_group,  script_file_msg, line_tok)) == -1)
      return exitStatus;
  }
  return 0; 
}

int ni_sql_param_validate_keywords(short int* sql_args_done_flag, char* script_file_msg)
{
  if(!(*sql_args_done_flag & NI_SQL_PARAM_HOST_PARSING_DONE))
  {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012055_ID, CAV_ERR_1012055_MSG, "Host", "SQL");
  }
  if(!(*sql_args_done_flag & NI_SQL_PARAM_PORT_PARSING_DONE))
  {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012055_ID, CAV_ERR_1012055_MSG, "Port", "SQL");
  }
  if(!(*sql_args_done_flag & NI_SQL_PARAM_DRIVER_PARSING_DONE))
  {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012055_ID, CAV_ERR_1012055_MSG, "Driver", "SQL");
  }
  if(!(*sql_args_done_flag & NI_SQL_PARAM_USER_PARSING_DONE))
  {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012055_ID, CAV_ERR_1012055_MSG, "UserName", "SQL");
  }
  if(!(*sql_args_done_flag & NI_SQL_PARAM_PASSWD_PARSING_DONE))
  {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012055_ID, CAV_ERR_1012055_MSG, "Password", "SQL");
  }
  if(!(*sql_args_done_flag & NI_SQL_PARAM_DATABASE_PARSING_DONE))
  {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012055_ID, CAV_ERR_1012055_MSG, "DataBase", "SQL");
  }
  if(!(*sql_args_done_flag & NI_SQL_PARAM_QUERY_PARSING_DONE))
  {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012055_ID, CAV_ERR_1012055_MSG, "Query", "SQL");
  }
  return 0;
}

int ni_sql_param_validate_host_and_port(char* host, char* port, char* script_file_msg, NI_SQLVarTableEntry* sqlVarTable)
{
  char host_with_port[512] = {0}; 
  char tmp_host_with_port[512] = {0};
  char *loc_ptr1 = NULL;
  struct sockaddr_in6 saddr;  
 
  sprintf(host_with_port, "%s:%s", host, port);
  NIDL(1, "host_with_port = %s", host_with_port);

  strcpy(tmp_host_with_port, host_with_port);
  if((nslb_fill_sockaddr(&saddr, tmp_host_with_port, 0)) == 0)
  {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012112_ID, CAV_ERR_1012112_MSG);
  }
  loc_ptr1 = nslb_sockaddr_to_ip((struct sockaddr *)&saddr, 1);
  NIDL(2, "loc_ptr1 = %s", loc_ptr1);

  if((strstr(loc_ptr1, "BadIP")) || (strstr(loc_ptr1, "Bad Family")))
  {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012113_ID, CAV_ERR_1012113_MSG);
  }
  char *loc_ptr2 = NULL;

  loc_ptr2 = strrchr(loc_ptr1, ':');
    
  if(loc_ptr2)
  {
    *loc_ptr2 = '\0';
     loc_ptr2++;
     strcpy(host, loc_ptr1);
     strcpy(port, loc_ptr2);
     NIDL(2, "After parsing host = %s, port = %s", host, port);

     strcpy(sqlVarTable->host, host);
     strcpy(sqlVarTable->port, port); 
     NIDL(3, "Host = %s, port = %s", sqlVarTable->host, sqlVarTable->port);
   }
   else
   {
     NIDL(1, "Port not found");
     SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012084_ID, CAV_ERR_1012084_MSG);
   }
   return 0;
} 

static int AllocBuffer(SQLUINTEGER buffSize, char** ptr, SQLLEN *buffer_len) 
{
  NIDL(1, "Method called, buffSize = %d, ptr = %p, buffer_len = %p", (SQLUINTEGER) buffSize, (SQLPOINTER) ptr, (SQLLEN) buffer_len);

  //*ptr = malloc(buffSize);
  NSLB_MALLOC_AND_MEMSET(*ptr, buffSize, "SQL allocated buffer", -1, NULL);
  if (buffer_len != NULL) {
    *buffer_len = buffSize;
  }
  return 0;
}

static char *extract_error(char *fn, SQLHANDLE handle, SQLSMALLINT type)
{
  SQLINTEGER i = 0, native;
  SQLCHAR state[7], text[1024];
  SQLSMALLINT len;
  SQLRETURN ret;
  int num_written = 0;

  do
  {
    ret = SQLGetDiagRec(type, handle, ++i, state, &native, text,
                          sizeof(text), &len );

    if (SQL_SUCCEEDED(ret))
    {
       NIDL(1, "state = %s, record number = %d, native = %d, message = %s", (char *) state, (int) i, (int)native, (char*) text);
       num_written += snprintf(&db_diag_msg[num_written], 1024, "%s", (char *)text);
    }
  }
  while( ret == SQL_SUCCESS );
  return db_diag_msg;
}

static int fill_query_result_buffer(NI_SQLVarTableEntry* sqlVarTable, char *buff , SQLLEN buff_len, char* delimeter)
{
  NIDL(1, "Method called. sqlVarTable = %p, buff = %s, buff_len = %d, delimeter = %s", sqlVarTable, buff, (int) buff_len, delimeter);
  
  if(!buff || (buff[0] == '\0')){
   NIDL(2, "Empty buffer found");
  }

  buff_len = strlen(delimeter) + buff_len + 1; 
 
  if(sqlVarTable->query_result_size + buff_len >= sqlVarTable->query_result_buff_size)
  {
    sqlVarTable->query_result = realloc(sqlVarTable->query_result, sqlVarTable->query_result_buff_size + NI_SQL_PARAM_DELTA_COL_SIZE); 
    sqlVarTable->query_result_buff_size += NI_SQL_PARAM_DELTA_COL_SIZE;
  }

  sqlVarTable->query_result_size += sprintf(&(sqlVarTable->query_result[sqlVarTable->query_result_size]), "%s%s", buff, delimeter);

  return 0;
}

int ni_sql_get_data_from_db(NI_SQLVarTableEntry* sqlVarTable, NI_SQLConAttr* sqlAttr, char* column_delimiter, char *err_msg)
{
  NIDL(1, "Method Called, sqlVarTable = %p", sqlVarTable);   

  NI_DataBase_T db; 
  SQLRETURN ret;
  char *conn_str = NULL;
  SQLCHAR outstr[1024];
  SQLSMALLINT outstrlen;
  void *in_stmt = NULL;

  db.env = SQL_NULL_HENV;
  db.dbc = SQL_NULL_HDBC;

  /* Allocate an environment handle */
  ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, (SQLHENV *) &db.env);     
  if ((ret != SQL_SUCCESS_WITH_INFO) && (ret != SQL_SUCCESS)) {
    sprintf(err_msg, "NS_DB_ODBC_INIT: SQLAllocHandle(Env) Failed");
    return -1;
  }
  NIDL(1, "SQLAllocHandle(Env) succeeded");
  
  /* We want ODBC 3 support */
  ret = SQLSetEnvAttr((SQLHENV) db.env, SQL_ATTR_ODBC_VERSION, (void *) SQL_OV_ODBC3, 0);
  if ( (ret != SQL_SUCCESS_WITH_INFO) && (ret != SQL_SUCCESS)) {
    sprintf(err_msg, "NSQLSetEnvAttr(ODBC version) Failed");
    return -1;
  }
  NIDL(1, "SQLSetEnvAttr(ODBC version) succeeded"); 

  /* Allocate a connection handle */
  ret = SQLAllocHandle(SQL_HANDLE_DBC, (SQLHENV) db.env, (SQLHDBC *) &db.dbc);
  if ( (ret != SQL_SUCCESS_WITH_INFO) && (ret != SQL_SUCCESS)) {
    sprintf(err_msg, "SQLAllocHandle(dbc) Failed");
    return -1;
  }
  NIDL(1, "SQLAllocHandle(dbc) succeeded");
   
  //conn_str = (char*) malloc(1024);
  NSLB_MALLOC(conn_str, (1024 * sizeof(char)), "connection string", -1, NULL);

  sprintf(conn_str, "%s=%s;%s=%s;%s=%s;%s=%s;%s=%s;%s=%s", sqlAttr->dsn, sqlVarTable->driver, sqlAttr->user, sqlVarTable->user, sqlAttr->database, sqlVarTable->db_name,  sqlAttr->host, sqlVarTable->host, sqlAttr->port, sqlVarTable->port, sqlAttr->password, sqlVarTable->passwd);

  NIDL(1, "conn_str = %s", conn_str);
  ret = SQLDriverConnect((SQLHDBC) db.dbc, (void *) 1,
                           (SQLCHAR *)conn_str, SQL_NTS, outstr,
                           sizeof(outstr), &outstrlen,
                           SQL_DRIVER_NOPROMPT);

  if (SQL_SUCCEEDED(ret)) {
     NIDL(1, "Connected to database. returned strring was: [%s]", outstr);
    if (ret == SQL_SUCCESS_WITH_INFO) {
       NIDL(1, "Driver reported the following diagnostics: [%s]",
                extract_error("SQLDriverConnect", (SQLHDBC) db.dbc, SQL_HANDLE_DBC));
    }
  } else {
    sprintf(err_msg, "Failed to connect: [%s]", extract_error("SQLDriverConnect", (SQLHDBC) db.dbc, SQL_HANDLE_DBC));
    return -1;
  }

  /* Allocate a handle for executing SQL statements */ 
  ret = SQLAllocHandle(SQL_HANDLE_STMT, (SQLHDBC) db.dbc, (SQLHSTMT *) &in_stmt);
  if(SQL_SUCCEEDED(ret))
  {
    NIDL(1, "Successfully allocated stmt handle = %p", in_stmt);
  }
  else
  {
    sprintf(err_msg, "Failed to allocate stmt handle");
    return -1;
  }

  void* stmt = (SQLHSTMT) in_stmt;
  if(stmt == SQL_NULL_HSTMT)
  {
    sprintf(err_msg, "Error: stmt is NULL");
    return -1;
  }

  /* Execute SQL statement */
  ret = SQLExecDirect(stmt, (SQLCHAR *) sqlVarTable->query, SQL_NTS);
  if ( (ret != SQL_SUCCESS) && (ret != SQL_SUCCESS_WITH_INFO) ) {
    sprintf(err_msg, "Error in executing the query: [%s]", sqlVarTable->query);
    return -1;
  }

  NIDL(1, "SQLExecDirect(qstr) succeeded");

  //MY_MALLOC_AND_MEMSET(sqlVarTable->query_result, NS_SQL_PARAM_MAX_QUERY_OUTPUT_LEN, "Allocate memory for sqlVarTable->query_result", -1);
  //sqlVarTable->query_result = malloc(NI_SQL_PARAM_MAX_QUERY_OUTPUT_LEN);
  NSLB_MALLOC(sqlVarTable->query_result, NI_SQL_PARAM_MAX_QUERY_OUTPUT_LEN, "sql var query result", -1, NULL);
  
  sqlVarTable->query_result_buff_size = NI_SQL_PARAM_MAX_QUERY_OUTPUT_LEN;

  NIDL(3, "sqlVarTable->query_result_buff_size = %ld", sqlVarTable->query_result_buff_size);
 
  SQLSMALLINT columns;
  /* Find the total no of columns in result row */
  ret = SQLNumResultCols(stmt, &columns);
  if ((ret != SQL_SUCCESS) && (ret != SQL_SUCCESS_WITH_INFO)) {
   sprintf(err_msg, "SQLNumResultCols() Failed");
   return -1;
  }

  NIDL(1, "num of columns = %d\n", columns);

  SQLCHAR  ColName[255];
  SQLSMALLINT  ColNameLen, DataType, DecimalDigits, Nullable;
  char*  ColPtrArray[columns];
  SQLLEN  ColBufferLenArray[columns];
  SQLLEN  ColLenOrIndArray[columns];
  SQLULEN  ColumnSize;
  int i = 0;
  
  for (i = 0; i < columns; i++) {
    // Describe the parameter.
    ret = SQLDescribeCol(stmt,
                         i+1,
                         ColName, 255,
                         &ColNameLen,
                         &DataType,
                         &ColumnSize,
                         &DecimalDigits,
                         &Nullable);

    NIDL(2, "Data Type : %i, ColName : %s, DecimalDigits : %i, Nullable %i, ColumnSize = %d, ColNameLen = %i",
                   (int)DataType, ColName, (int)DecimalDigits, (int)Nullable, (int)ColumnSize, (int)ColNameLen);
    
    if (DataType == SQL_LONGVARCHAR){
      if((AllocBuffer(NI_SQL_PARAM_MAX_COL_SIZE, &ColPtrArray[i], &ColBufferLenArray[i])) == -1)
        return -1;
    }
    else{
      if((AllocBuffer(ColumnSize+1, &ColPtrArray[i], &ColBufferLenArray[i])) == -1)
        return -1;
    }

    NIDL(2, "i = %d, Col Buffer Ptr = %p, Col Buffer Len = %i, DataType = %d", i, ColPtrArray[i], (int)ColBufferLenArray[i], DataType);

    // Bind the memory to the parameter.
    ret = SQLBindCol(stmt,                   // Statment Handle
                     i+1,                    // Column Number
                     SQL_C_CHAR,             // C Type
                     ColPtrArray[i],         // Column value Pointer
                     ColBufferLenArray[i],   // Buffer Length 
                     &ColLenOrIndArray[i]);  //Len or Indicator
 
    if ((ret != SQL_SUCCESS) && (ret != SQL_SUCCESS_WITH_INFO)) 
    {
      sprintf(err_msg, "SQLNumResultCols() Failed");
      return -1;
    }
  }

  while (SQL_SUCCEEDED(SQLFetch(stmt))) 
  {
    for(i = 0; i < columns; i++) 
    {
      if (ColLenOrIndArray[i] == SQL_NULL_DATA)
      { 
        NIDL(2, "SQL_NULL_DATA found , ColPtrArray[%d] = %s, ColLenOrIndArray[%d] = %d", i, ColPtrArray[i], i, ColLenOrIndArray[i]);

        if((i < (columns - 1)))
          fill_query_result_buffer(sqlVarTable, "NULL" , 4, column_delimiter);
        else
          //After last column new line('\n') will be the delimiter as it is end of a row.
          fill_query_result_buffer(sqlVarTable, "NULL" , 4, "\n");
      }
      else 
      {
        NIDL(2, "ColPtrArray[%d] = %s, ColLenOrIndArray[%d] = %d", i, ColPtrArray[i], i, ColLenOrIndArray[i]);
        if((i < (columns - 1)))
          fill_query_result_buffer(sqlVarTable, ColPtrArray[i] , ColLenOrIndArray[i], column_delimiter);
        else
          fill_query_result_buffer(sqlVarTable, ColPtrArray[i] , ColLenOrIndArray[i], "\n");
      }
    }
  }
  NIDL(3,"sqlVarTable->query_result_size = %ld", sqlVarTable->query_result_size);
  NIDL(2, "Query Result = %s", sqlVarTable->query_result);

  /* Free up allocated handles */
  if((SQLHDBC) db.dbc != SQL_NULL_HDBC)
  {
    SQLDisconnect((SQLHDBC) db.dbc);
    SQLFreeHandle(SQL_HANDLE_DBC, db.dbc);
    db.dbc = (void *) SQL_NULL_HDBC;
  }
  if((SQLHENV) db.env != SQL_NULL_HENV)
  {
    SQLFreeHandle(SQL_HANDLE_ENV, db.env);
    db.env = (void *) SQL_NULL_HENV;
  }
  if((SQLHSTMT) stmt != SQL_NULL_HSTMT){
    SQLFreeHandle(SQL_HANDLE_STMT, (SQLHSTMT) stmt);
    stmt = (void *) SQL_NULL_HSTMT;
  }

  return 0;
}

 
