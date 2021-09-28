/********************************************************************************************************************
 * File Name      : ns_sql_var.c                                                                                    |
 |                                                                                                                  | 
 * Synopsis       : This file contain all the functions which take part in SQL Parameter                            | 
 |                                                                                                                  |
 * Author(s)      : Shibani/Ayush                                                                                   |
 |                  Shibani:                                                                                        |
 |                    (1) Add function for API nsl_sql_var() parsing                                                |
 |                    (2)                                                                                           |
 |                  Aysuh:                                                                                          |
 |                    (1) Add function for DataBase conncection                                                     |
 |                    (2) Add function for DataBase query                                                           |
 |                                                                                                                  |
 * Date           : Fri Aug  4 10:57:38 IST 2017                                                                    |
 |                                                                                                                  |
 * Copyright      : (c) Cavisson Systems                                                                            |
 |                                                                                                                  |
 * Mod. History   :                                                                                                 |
 *******************************************************************************************************************/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include<string.h>
#include <sql.h>
#include <sqlext.h>
#include<sys/socket.h>
#include<errno.h>
#include<netdb.h>
#include<arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <libgen.h>

#include "util.h"
#include "ns_log.h"
#include "ns_sql_vars.h"
#include "ns_alloc.h"
#include "ns_static_vars.h"
#include "ns_trace_level.h"
#include "nslb_sock.h"
#include "ns_parent.h"
#include "ns_exit.h"
#include "ns_script_parse.h"
#define FREE_SQL_HANDLES \
{\
  NSDL2_VARS(NULL, NULL, "Start freeing SQL handles.");\
  if((SQLHDBC) db.dbc != SQL_NULL_HDBC)\
  {\
    SQLDisconnect((SQLHDBC) db.dbc);\
    SQLFreeHandle(SQL_HANDLE_DBC, db.dbc);\
    db.dbc = (void *) SQL_NULL_HDBC;\
  }\
  if((SQLHENV) db.env != SQL_NULL_HENV)\
  {\
    SQLFreeHandle(SQL_HANDLE_ENV, db.env);\
    db.env = (void *) SQL_NULL_HENV;\
  }\
  if((SQLHSTMT) stmt != SQL_NULL_HSTMT){\
    SQLFreeHandle(SQL_HANDLE_STMT, (SQLHSTMT) stmt);\
    stmt = (void *) SQL_NULL_HSTMT;\
  }\
}

static char db_diag_msg[NS_SQL_PARAM_MAX_DB_DIAG_MSG_LEN+1] = {0};

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
  NSDL2_VARS(NULL, NULL, "Method Called, value = [%s]", value);
   
  if (*state == NS_ST_STAT_VAR_NAME) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "SQL");
  }
  
  if ((*done & SQL_PARAM_HOST_PARSING_DONE)) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012036_ID, CAV_ERR_1012036_MSG, "Host", "SQL");
  }

  if(value[0] == '\0'){
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, "Host", "SQL");
  }
  strcpy(host, value);
 
  *state = NS_ST_STAT_VAR_OPTIONS;
  *done |= SQL_PARAM_HOST_PARSING_DONE;

  return 0; 
}

static int set_buffer_for_sql_param_port(char* value, char* port, char* script_file_msg, int* state, short int* done)
{
  NSDL2_VARS(NULL, NULL, "Method Called, value = [%s]", value);

  if (*state == NS_ST_STAT_VAR_NAME) {
    NSTL1_OUT(NULL, NULL, "%s Expecting a variable as the first parameter\n", script_file_msg);
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "SQL");
  }

  if ((*done & SQL_PARAM_PORT_PARSING_DONE)) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012036_ID, CAV_ERR_1012036_MSG, "Port", "SQL");
  }

  if(value[0] == '\0'){
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, "Port", "SQL");
  }

  if(!is_numeric(value)){
    NSTL1_OUT(NULL, NULL, "%s Port must be numeric.\n", script_file_msg);
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012043_ID, CAV_ERR_1012043_MSG, "Port");
  }
  strcpy(port, value);

  *state = NS_ST_STAT_VAR_OPTIONS;
  *done |= SQL_PARAM_PORT_PARSING_DONE;

  return 0;
}

static int set_sql_param_driver_name(char* value, SQLVarTableEntry* sqlVarTable, char* script_file_msg, int* state, short int* done)
{
  NSDL2_VARS(NULL, NULL, "Method Called, value = [%s]", value);
  
  if (*state == NS_ST_STAT_VAR_NAME) {
    NSTL1_OUT(NULL, NULL, "%s Expecting a variable as the first parameter\n", script_file_msg);
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "SQL");
  }
  
  if ((*done & SQL_PARAM_DRIVER_PARSING_DONE)) {
    NSTL1_OUT(NULL, NULL, "%s  'Driver' name can be given only once.\n", script_file_msg);
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012036_ID, CAV_ERR_1012036_MSG, "Driver", "SQL");
  }
  
  if(value[0] == '\0'){
    NSTL1_OUT(NULL, NULL, "%s  Value for field 'Driver' must be given.\n", script_file_msg);
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, "Driver", "SQL");
  }

  char driver[256] = {0};
  strcpy(driver, value);
  if ((sqlVarTable->driver = copy_into_big_buf(driver, 0)) == -1) {
    NSTL1(NULL, NULL, "%s: Failed to copy driver name into big buffer\n", script_file_msg);
    NS_EXIT(-1, CAV_ERR_1000018, driver);
  }
 
  NSDL3_VARS(NULL, NULL, "Driver name = %s", RETRIEVE_BUFFER_DATA(sqlVarTable->driver)); 

  *state = NS_ST_STAT_VAR_OPTIONS;   
  *done |= SQL_PARAM_DRIVER_PARSING_DONE;
 
  return 0;  
}

static int set_sql_param_user_name(char* value, SQLVarTableEntry* sqlVarTable, char* script_file_msg, int* state, short int* done)
{
  NSDL2_VARS(NULL, NULL, "Method Called, value = [%s]", value);

  if (*state == NS_ST_STAT_VAR_NAME) {
    NSTL1_OUT(NULL, NULL, "%s Expecting a variable as the first parameter\n", script_file_msg);
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "SQL");
  }  

  if ((*done & SQL_PARAM_USER_PARSING_DONE)) {
    NSTL1_OUT(NULL, NULL, "%s  'User' name can be given only once.\n", script_file_msg);
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012036_ID, CAV_ERR_1012036_MSG, "User", "SQL");
  }

  if(value[0] == '\0'){
    NSTL1_OUT(NULL, NULL, "%s  Value for field 'User' must be given.\n", script_file_msg);
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, "User", "SQL");
  }

  char user[265] = {0};
  strcpy(user, value);
  if ((sqlVarTable->user = copy_into_big_buf(user, 0)) == -1) {
    NSTL1(NULL, NULL, "%s: Failed to copy user name into big buffer\n", script_file_msg);
    NS_EXIT(-1, CAV_ERR_1000018, user);
  }

  NSDL3_VARS(NULL, NULL, "User name = %s", RETRIEVE_BUFFER_DATA(sqlVarTable->user));

  *state = NS_ST_STAT_VAR_OPTIONS;
  *done |= SQL_PARAM_USER_PARSING_DONE;  

  return 0;
}

static int set_sql_param_passwd(char* value, SQLVarTableEntry* sqlVarTable, char* script_file_msg, int* state, short int* done)
{
  NSDL2_VARS(NULL, NULL, "Method Called, value = [%s]", value);

  if (*state == NS_ST_STAT_VAR_NAME) {
    NSTL1_OUT(NULL, NULL, "%s Expecting a variable as the first parameter\n", script_file_msg);
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "SQL");
  }

  if ((*done & SQL_PARAM_PASSWD_PARSING_DONE)) {
    NSTL1_OUT(NULL, NULL, "%s  'PassWord' can be given only once.\n", script_file_msg);
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012036_ID, CAV_ERR_1012036_MSG, "PassWord", "SQL");
  }
 
  if(value[0] == '\0'){
    NSTL1_OUT(NULL, NULL, "%s  Value for field 'PassWord' must be given.\n", script_file_msg);
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, "PassWord", "SQL");
  }

  char passwd[256] = {0};
  strcpy(passwd, value);
  if ((sqlVarTable->passwd = copy_into_big_buf(passwd, 0)) == -1) {
    NSTL1(NULL, NULL, "%s: Failed to copy password into big buffer\n", script_file_msg);
    NS_EXIT(-1, CAV_ERR_1000018, passwd);
  }
  
  NSDL3_VARS(NULL, NULL, "Password = %s", RETRIEVE_BUFFER_DATA(sqlVarTable->passwd));

  *state = NS_ST_STAT_VAR_OPTIONS;
  *done |= SQL_PARAM_PASSWD_PARSING_DONE;

  return 0; 
}

int set_ns_sql_odbc_con_attr(SQLVarTableEntry* sqlVarTable, SQLConAttr* sqlAttr){
  char file_name[256]={0};
  FILE *sql_fp;
  sprintf(file_name,"/home/cavisson/thirdparty/sqldriver/%s",RETRIEVE_BUFFER_DATA(sqlVarTable->driver));
  NSDL2_VARS(NULL, NULL, "Opening database '%s' ODBC attributes file '%s'", RETRIEVE_BUFFER_DATA(sqlVarTable->driver), file_name);
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
      NSDL2_VARS(NULL, NULL, "DSN attribute name = %s, HOST attribute name = %s, USER attribute name = %s, PORT attribute name = %s, PASSWORD attribute name = %s, DATABASE attribute name = %s", sqlAttr->dsn, sqlAttr->host, sqlAttr->user, sqlAttr->port, sqlAttr->password, sqlAttr->database);
    }
    fclose(sql_fp);
  }
  else{
    NSDL2_VARS(NULL, NULL, "Error in opening driver file '%s'.",file_name);
    NSTL1_OUT(NULL,NULL,"Driver name '%s' does't not exist.",RETRIEVE_BUFFER_DATA(sqlVarTable->driver));
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012083_ID, CAV_ERR_1012083_MSG, RETRIEVE_BUFFER_DATA(sqlVarTable->driver));
  }
return 0;
}

static int set_sql_param_db_name(char* value, SQLVarTableEntry* sqlVarTable, char* script_file_msg, int* state, short int* done)
{
  NSDL2_VARS(NULL, NULL, "Method Called, value = [%s]", value);

  char database[256] = {0};

  if (*state == NS_ST_STAT_VAR_NAME) {
    NSTL1_OUT(NULL, NULL, "%s Expecting a variable as the first parameter\n", script_file_msg);
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "SQL");
  }

  if ((*done & SQL_PARAM_DATABASE_PARSING_DONE)) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012036_ID, CAV_ERR_1012036_MSG, "DataBase", "SQL");
  }

  if(value[0] == '\0'){
    NSTL1_OUT(NULL, NULL, "%s  Value for field 'Database' must be given.\n", script_file_msg);
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, "DataBase", "SQL");
  }

  strcpy(database, value);
  if ((sqlVarTable->db_name = copy_into_big_buf(database, 0)) == -1) {
    NSTL1(NULL, NULL, "%s: Failed to copy database name into big buffer\n", script_file_msg);
    NS_EXIT(-1, CAV_ERR_1000018, database);
  }

  NSDL3_VARS(NULL, NULL, "Database name = %s", RETRIEVE_BUFFER_DATA(sqlVarTable->db_name)); 
   
  *state = NS_ST_STAT_VAR_OPTIONS;
  *done |= SQL_PARAM_DATABASE_PARSING_DONE;

  return 0;
}

static int set_sql_param_query(char* value, SQLVarTableEntry* sqlVarTable, char* script_file_msg, int* state, short int* done)
{
  NSDL2_VARS(NULL, NULL, "Method Called, value = [%s]", value);
  
  char buffer[512] = {0};
  
  if (*state == NS_ST_STAT_VAR_NAME) {
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "SQL");
  }

  if ((*done & SQL_PARAM_QUERY_PARSING_DONE)) {
    NSTL1_OUT(NULL, NULL, "%s 'Query' can be given only once.\n", script_file_msg);
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012036_ID, CAV_ERR_1012036_MSG, "Query", "SQL");
  }

  if(value[0] == '\0'){
    NSTL1_OUT(NULL, NULL, "%s Value for field 'Query' must be given.\n", script_file_msg);
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012132_ID, CAV_ERR_1012132_MSG, "Query", "SQL");
  }

  strcpy(buffer, value);
  if ((sqlVarTable->query = copy_into_big_buf(buffer, 0)) == -1) {
    NSTL1(NULL, NULL, "%s: Failed to copy query into big buffer\n", script_file_msg);
    NS_EXIT(-1, CAV_ERR_1000018, buffer);
  }
  
  NSDL3_VARS(NULL, NULL, "Query =  %s", RETRIEVE_BUFFER_DATA(sqlVarTable->query)); 
   
  *state = NS_ST_STAT_VAR_OPTIONS;
  *done |= SQL_PARAM_QUERY_PARSING_DONE;

  return 0;
}

static int set_sql_param_save_into_file(char* value, SQLVarTableEntry* sqlVarTable, char* script_file_msg, int* state, short int* done, int group_idx)
{
  NSDL2_VARS(NULL, NULL, "Method Called, value = [%s]", value);

  if (*state == NS_ST_STAT_VAR_NAME) {
    NSTL1_OUT(NULL, NULL, "%s Expecting a variable as the first parameter\n", script_file_msg);
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012053_ID, CAV_ERR_1012053_MSG, "SQL");
  }
 
  if ((*done & SQL_PARAM_SAVEINTOFILE_PARSING_DONE)) {
    NSTL1_OUT(NULL, NULL, "%s  'IsSaveIntoFile' can be given only once.\n", script_file_msg);
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012036_ID, CAV_ERR_1012036_MSG, "IsSaveIntoFile", "SQL");
  }

  char is_save_into_file[16] = {0};
  if(value[0] != '\0')
    strcpy(is_save_into_file, value);
  else
    strcpy(is_save_into_file, "NO");

  if((strcasecmp(is_save_into_file, "Yes")) && (strcasecmp(is_save_into_file, "NO")))
  {
    NSTL1_OUT(NULL, NULL, "%s  Value of filed 'IsSaveIntoFile' is '%s'. It's value can be either Yes or No.\n", script_file_msg, is_save_into_file);
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012027_ID, CAV_ERR_1012027_MSG, is_save_into_file, "IsSaveIntoFile", "File");
  }

  if ((sqlVarTable->is_save_into_file = copy_into_big_buf(is_save_into_file, 0)) == -1) {
    NSTL1(NULL, NULL, "%s: Failed to copy into big buffer\n", script_file_msg);
    NS_EXIT(-1, CAV_ERR_1000018, is_save_into_file);
  }
 
  NSDL3_VARS(NULL, NULL, "IsSaveIntoFile =  %s", RETRIEVE_BUFFER_DATA(sqlVarTable->is_save_into_file));  

  *state = NS_ST_STAT_VAR_OPTIONS;
  if((!strcasecmp(is_save_into_file, "Yes"))){
    *done |= SQL_PARAM_SAVEINTOFILE_PARSING_DONE;
    groupTable[group_idx].is_save_into_file_flag = 1;    
  }
  return 0;
}



int parse_sql_params(char* keyword, char* value, SQLVarTableEntry* sqlVarTable, int* state, char* script_file_msg, char* host, char* port, short int *done, char* staticvar_buf, int* group_idx, int* var_start, int sess_idx, int* rw_data, int* create_group, int* var_idx, int* column_flag, unsigned short* reltv_var_idx) 
{
  int exitStatus;

  NSDL1_VARS(NULL, NULL, "Method called");

  if(!strcasecmp(keyword, "Host"))
  {
    set_buffer_for_sql_param_host(value, host, script_file_msg, state, done);
  }
  else if(!strcasecmp(keyword, "Port"))
  {
    set_buffer_for_sql_param_port(value, port, script_file_msg, state, done);
  }
  else if(!strcasecmp(keyword, "Driver"))
  {
    set_sql_param_driver_name(value, sqlVarTable, script_file_msg, state, done);
  }
  else if(!strcasecmp(keyword, "User"))
  {
    set_sql_param_user_name(value, sqlVarTable, script_file_msg, state, done);
  }
  else if(!strcasecmp(keyword, "PassWord"))
  {
    set_sql_param_passwd(value, sqlVarTable, script_file_msg, state, done);
  }
  else if(!strcasecmp(keyword, "DataBase"))
  {
    set_sql_param_db_name(value, sqlVarTable, script_file_msg, state, done);
  }
  else if(!strcasecmp(keyword, "Query"))
  {
    set_sql_param_query(value, sqlVarTable, script_file_msg, state, done);
  }
  else if(!strcasecmp(keyword, "IsSaveIntoFile"))
  {
    set_sql_param_save_into_file(value, sqlVarTable, script_file_msg, state, done, *group_idx);
  }
  else
  {
    exitStatus = set_last_option(value, keyword, staticvar_buf, group_idx, var_start, sess_idx, script_file_msg, state, rw_data, create_group, var_idx, column_flag, *reltv_var_idx);
    *reltv_var_idx = *reltv_var_idx + 1;
    if (exitStatus == -1)
      return exitStatus;
  }
  return 0; 
}

int ns_sql_param_validate_keywords(short int* sql_args_done_flag, char* script_file_msg)
{
  if(!(*sql_args_done_flag & SQL_PARAM_HOST_PARSING_DONE))
  {
    NSTL1_OUT(NULL, NULL, "%s Host name is not given. It is mandatory field for this API.\n", script_file_msg);
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012055_ID, CAV_ERR_1012055_MSG, "Host", "SQL");
  }
  if(!(*sql_args_done_flag & SQL_PARAM_PORT_PARSING_DONE))
  {
    NSTL1_OUT(NULL, NULL, "%s Port is not given. It is mandatory field for this API.\n", script_file_msg);
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012055_ID, CAV_ERR_1012055_MSG, "Port", "SQL");
  }
  if(!(*sql_args_done_flag & SQL_PARAM_DRIVER_PARSING_DONE))
  {
    NSTL1_OUT(NULL, NULL, "%s Driver name is not given. It is mandatory field for this API.\n", script_file_msg);
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012055_ID, CAV_ERR_1012055_MSG, "Driver", "SQL");
  }
  if(!(*sql_args_done_flag & SQL_PARAM_USER_PARSING_DONE))
  {
    NSTL1_OUT(NULL, NULL, "%s User name is not given. It is mandatory field for this API.\n", script_file_msg);
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012055_ID, CAV_ERR_1012055_MSG, "UserName", "SQL");
  }
  if(!(*sql_args_done_flag & SQL_PARAM_PASSWD_PARSING_DONE))
  {
    NSTL1_OUT(NULL, NULL, "%s PassWord is not given. It is mandatory field for this API.\n", script_file_msg);
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012055_ID, CAV_ERR_1012055_MSG, "Password", "SQL");
  }
  if(!(*sql_args_done_flag & SQL_PARAM_DATABASE_PARSING_DONE))
  {
    NSTL1_OUT(NULL, NULL, "%s DataBase name is not given. It is mandatory field for this API.\n", script_file_msg);
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012055_ID, CAV_ERR_1012055_MSG, "DataBase", "SQL");
  }
  if(!(*sql_args_done_flag & SQL_PARAM_QUERY_PARSING_DONE))
  {
    NSTL1_OUT(NULL, NULL, "%s SQL Query is not given. It is mandatory field for this API.\n", script_file_msg);
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012055_ID, CAV_ERR_1012055_MSG, "Query", "SQL");
  }
  return 0;
}

int ns_sql_param_validate_host_and_port(char* host, char* port, char* script_file_msg, SQLVarTableEntry* sqlVarTable)
{
  char host_with_port[512] = {0}; 
  char tmp_host_with_port[512] = {0};
  char *loc_ptr1 = NULL;
  struct sockaddr_in6 saddr;  
 
  sprintf(host_with_port, "%s:%s", host, port);
  NSDL2_VARS(NULL, NULL, "host_with_port = %s", host_with_port);

  strcpy(tmp_host_with_port, host_with_port);
  if((nslb_fill_sockaddr(&saddr, tmp_host_with_port, 0)) == 0)
  {
    NSTL1_OUT(NULL, NULL, "%s Format of 'Host' or 'Port' is invalid.\n", script_file_msg);
    SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012112_ID, CAV_ERR_1012112_MSG);
  }
  loc_ptr1 = nslb_sockaddr_to_ip((struct sockaddr *)&saddr, 1);
  NSDL2_VARS(NULL, NULL, "loc_ptr1 = %s", loc_ptr1);

  if((strstr(loc_ptr1, "BadIP")) || (strstr(loc_ptr1, "Bad Family")))
  {
    NSTL1_OUT(NULL, NULL, "%s Bad Family or Bad IP found.\n", script_file_msg);
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
     NSDL2_VARS(NULL, NULL, "Host and port after parsing = %s and %s", host, port);

     if ((sqlVarTable->host = copy_into_big_buf(host, 0)) == -1)
     {
       NSTL1(NULL, NULL, "%s: Failed to copy host name into big buffer\n", script_file_msg);
       NS_EXIT(-1, CAV_ERR_1000018, host);
     }
    
     if ((sqlVarTable->port = copy_into_big_buf(port, 0)) == -1)
     {
       NSTL1(NULL, NULL, "%s: Failed to copy port into big buffer\n", script_file_msg);
       NS_EXIT(-1, CAV_ERR_1000018, port);
     }

     NSDL2_VARS(NULL, NULL, "Host = %s, port = %s", RETRIEVE_BUFFER_DATA(sqlVarTable->host), RETRIEVE_BUFFER_DATA(sqlVarTable->port));
   }
   else
   {
     NSDL2_VARS(NULL, NULL, "Port not found");
     SCRIPT_PARSE_ERROR_EXIT_EX(NULL, CAV_ERR_1012084_ID, CAV_ERR_1012084_MSG);
   }
   return 0;
} 

static int AllocBuffer(SQLUINTEGER buffSize, char** ptr, SQLLEN *buffer_len) 
{
  NSDL2_VARS(NULL, NULL, "Method called, buffSize = %d, ptr = %p, buffer_len = %p", (SQLUINTEGER) buffSize, (SQLPOINTER) ptr, (SQLLEN) buffer_len);

  MY_MALLOC_AND_MEMSET(*ptr, buffSize, "Allocate memory for ColPtrArray array", -1); 
  
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
    ret = SQLGetDiagRec(type, handle, ++i, state, &native, text, sizeof(text), &len);

    if (SQL_SUCCEEDED(ret))
    {
      NSDL1_VARS(NULL, NULL, "state = %s, record number = %d, native = %d, message = %s", (char *) state, (int) i, (int)native, (char*) text);
      num_written += snprintf(&db_diag_msg[num_written], 1024, "%s", (char *)text);
    }
  }
  while( ret == SQL_SUCCESS );
  return db_diag_msg;
}

static int fill_query_result_buffer(SQLVarTableEntry* sqlVarTable, char *buff , SQLLEN buff_len, char* delimeter)
{
  NSDL2_VARS(NULL, NULL, "Method called. sqlVarTable = %p, buff = %s, buff_len = %d, delimeter = %s", sqlVarTable, buff, (int) buff_len, delimeter);
  
  if(!buff || (buff[0] == '\0')){
   NSDL3_VARS(NULL, NULL, "Empty buffer found");
   //return -1; 
  }

  buff_len = strlen(delimeter) + buff_len + 1; 
 
  if(sqlVarTable->query_result_size + buff_len >= sqlVarTable->query_result_buff_size)
  {
    MY_REALLOC_EX(sqlVarTable->query_result, sqlVarTable->query_result_buff_size + NS_SQL_PARAM_DELTA_COL_SIZE, sqlVarTable->query_result_buff_size, "Reallocate memory for sqlVarTable->query_result", -1);
    sqlVarTable->query_result_buff_size += NS_SQL_PARAM_DELTA_COL_SIZE;
  }

  sqlVarTable->query_result_size += sprintf(&(sqlVarTable->query_result[sqlVarTable->query_result_size]), "%s%s", buff, delimeter);

  return 0;
}

int ns_sql_get_data_from_db(SQLVarTableEntry* sqlVarTable, SQLConAttr* sqlAttr, char* column_delimiter, char *err_msg)
{
  NSDL3_VARS(NULL, NULL, "Method Called, sqlVarTable = %p", sqlVarTable);   

  DataBase_T db; 
  SQLRETURN ret;
  char *conn_str = NULL;
  SQLCHAR outstr[1024];
  SQLSMALLINT outstrlen;
  void *in_stmt = SQL_NULL_HSTMT;
  void* stmt = SQL_NULL_HSTMT;

  db.env = SQL_NULL_HENV;
  db.dbc = SQL_NULL_HDBC;

  /* Allocate an environment handle */
  ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, (SQLHENV *) &db.env);     
  if ((ret != SQL_SUCCESS_WITH_INFO) && (ret != SQL_SUCCESS)) {
    sprintf(err_msg, "NS_DB_ODBC_INIT: SQLAllocHandle(Env) Failed");
    return -1;
  }
  NSDL2_VARS(NULL, NULL, "SQLAllocHandle(Env) succeeded");
  
  /* We want ODBC 3 support */
  ret = SQLSetEnvAttr((SQLHENV) db.env, SQL_ATTR_ODBC_VERSION, (void *) SQL_OV_ODBC3, 0);
  if ( (ret != SQL_SUCCESS_WITH_INFO) && (ret != SQL_SUCCESS)) {
    sprintf(err_msg, "NSQLSetEnvAttr(ODBC version) Failed");
    return -1;
  }
  NSDL2_VARS(NULL, NULL, "SQLSetEnvAttr(ODBC version) succeeded"); 

  /* Allocate a connection handle */
  ret = SQLAllocHandle(SQL_HANDLE_DBC, (SQLHENV) db.env, (SQLHDBC *) &db.dbc);
  if ( (ret != SQL_SUCCESS_WITH_INFO) && (ret != SQL_SUCCESS)) {
    sprintf(err_msg, "SQLAllocHandle(dbc) Failed");
    return -1;
  }
  NSDL2_VARS(NULL, NULL, "SQLAllocHandle(dbc) succeeded");
   
  MY_MALLOC_AND_MEMSET(conn_str, 1024, "Allocate memory for conn_str", -1);
  sprintf(conn_str, "%s=%s;%s=%s;%s=%s;%s=%s;%s=%s;%s=%s", sqlAttr->dsn, RETRIEVE_BUFFER_DATA(sqlVarTable->driver), sqlAttr->user, RETRIEVE_BUFFER_DATA(sqlVarTable->user), sqlAttr->database, RETRIEVE_BUFFER_DATA(sqlVarTable->db_name), sqlAttr->host, RETRIEVE_BUFFER_DATA(sqlVarTable->host), sqlAttr->port, RETRIEVE_BUFFER_DATA(sqlVarTable->port), sqlAttr->password, RETRIEVE_BUFFER_DATA(sqlVarTable->passwd));
 
 /* sprintf(conn_str, "DSN=%s;Server=%s;Port=%s;User=%s;Password=%s;Database=%s", RETRIEVE_BUFFER_DATA(sqlVarTable->driver),                                         RETRIEVE_BUFFER_DATA(sqlVarTable->host), RETRIEVE_BUFFER_DATA(sqlVarTable->port), RETRIEVE_BUFFER_DATA(sqlVarTable->user),                     RETRIEVE_BUFFER_DATA(sqlVarTable->passwd), RETRIEVE_BUFFER_DATA(sqlVarTable->db_name));  
 */

  NSDL2_VARS(NULL, NULL, "conn_str = %s", conn_str);
  ret = SQLDriverConnect((SQLHDBC) db.dbc, (void *) 1,
                           (SQLCHAR *)conn_str, SQL_NTS, outstr,
                           sizeof(outstr), &outstrlen,
                           SQL_DRIVER_NOPROMPT);

  if (SQL_SUCCEEDED(ret)) {
    NSDL2_VARS(NULL, NULL, "Connected to database. returned strring was: [%s]", outstr);
    if (ret == SQL_SUCCESS_WITH_INFO) {
      NSDL2_VARS(NULL, NULL, "Driver reported the following diagnostics: [%s]",
                extract_error("SQLDriverConnect", (SQLHDBC) db.dbc, SQL_HANDLE_DBC));
    }
  } else {
    sprintf(err_msg, "Failed to connect: %s", extract_error("SQLDriverConnect", (SQLHDBC) db.dbc, SQL_HANDLE_DBC));
    return -1;
  }
  //Resolve Bug 34799 - SQL_Parameter| Performance issue when we are providing 244 MB of data in SQL Parameter.
  if((run_mode_option & RUN_MODE_OPTION_COMPILE)){
    FREE_SQL_HANDLES;
    return 0; 
  }

  /* Allocate a handle for executing SQL statements */ 
  ret = SQLAllocHandle(SQL_HANDLE_STMT, (SQLHDBC) db.dbc, (SQLHSTMT *) &in_stmt);
  if(SQL_SUCCEEDED(ret))
  {
    NSDL2_VARS(NULL, NULL, "Successfully allocated stmt handle = %p", in_stmt);
  }
  else
  {
    sprintf(err_msg, "Failed to allocate stmt handle");
    return -1;
  }

  stmt = (SQLHSTMT) in_stmt;
  if(stmt == SQL_NULL_HSTMT)
  {
    sprintf(err_msg, "Error: stmt is NULL");
    return -1;
  }

  /* Execute SQL statement */
  ret = SQLExecDirect(stmt, (SQLCHAR *) RETRIEVE_BUFFER_DATA(sqlVarTable->query), SQL_NTS);
  if ( (ret != SQL_SUCCESS) && (ret != SQL_SUCCESS_WITH_INFO) ) {
    sprintf(err_msg, "Error in executing the query: [%s]", RETRIEVE_BUFFER_DATA(sqlVarTable->query));
    return -1;
  }

  NSDL2_VARS(NULL, NULL, "SQLExecDirect(qstr) succeeded");

  MY_MALLOC_AND_MEMSET(sqlVarTable->query_result, NS_SQL_PARAM_MAX_QUERY_OUTPUT_LEN, "Allocate memory for sqlVarTable->query_result", -1);
  sqlVarTable->query_result_buff_size = NS_SQL_PARAM_MAX_QUERY_OUTPUT_LEN;

  NSDL3_VARS(NULL, NULL, "sqlVarTable->query_result_buff_size = %ld", sqlVarTable->query_result_buff_size);
 
  SQLSMALLINT columns;
  /* Find the total no of columns in result row */
  ret = SQLNumResultCols(stmt, &columns);
  if ((ret != SQL_SUCCESS) && (ret != SQL_SUCCESS_WITH_INFO)) {
   sprintf(err_msg, "SQLNumResultCols() Failed");
   return -1;
  }

  NSDL2_VARS(NULL, NULL, "num of columns = %d\n", columns);

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

    NSDL2_VARS(NULL, NULL, "Data Type : %i, ColName : %s, DecimalDigits : %i, Nullable %i, ColumnSize = %d, ColNameLen = %i",
                   (int)DataType, ColName, (int)DecimalDigits, (int)Nullable, (int)ColumnSize, (int)ColNameLen);
    
    if (DataType == SQL_LONGVARCHAR){
      if((AllocBuffer(NS_SQL_PARAM_MAX_COL_SIZE, &ColPtrArray[i], &ColBufferLenArray[i])) == -1)
        return -1;
    }
    else{
      if((AllocBuffer(ColumnSize+1, &ColPtrArray[i], &ColBufferLenArray[i])) == -1)
        return -1;
    }

    NSDL2_VARS(NULL, NULL, "i = %d, Col Buffer Ptr = %p, Col Buffer Len = %i, DataType = %d", i, ColPtrArray[i], (int)ColBufferLenArray[i], DataType);

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
        NSDL2_VARS(NULL, NULL, "SQL_NULL_DATA found , ColPtrArray[%d] = %s, ColLenOrIndArray[%d] = %d", i, ColPtrArray[i], i, ColLenOrIndArray[i]);
        //After last column, column delimiter is not placed.
        if((i < (columns - 1)))
          fill_query_result_buffer(sqlVarTable, "NULL" , 4, column_delimiter);
        else
          //After last column new line('\n') will be the delimiter as it is end of a row.
          fill_query_result_buffer(sqlVarTable, "NULL" , 4, "\n");
      }
      else 
      {
        NSDL2_VARS(NULL, NULL, "ColPtrArray[%d] = %s, ColLenOrIndArray[%d] = %d", i, ColPtrArray[i], i, ColLenOrIndArray[i]);
        if((i < (columns - 1)))
          fill_query_result_buffer(sqlVarTable, ColPtrArray[i] , ColLenOrIndArray[i], column_delimiter);
        else
          fill_query_result_buffer(sqlVarTable, ColPtrArray[i] , ColLenOrIndArray[i], "\n");
      }
    }
  }
  NSDL2_VARS(NULL, NULL,"sqlVarTable->query_result_size = %ld", sqlVarTable->query_result_size);
  NSDL2_VARS(NULL, NULL, "Query Result = %s", sqlVarTable->query_result);

  /* Free up allocated handles */
  FREE_SQL_HANDLES;
   
  return 0;
}

int ns_sql_save_query_result_into_file(SQLVarTableEntry* sqlVarTable, char* file_name, GroupTableEntry* groupTable, int group_idx)
{
  int fd;
  char tmp_file_name[512] = {0};
  char *path = NULL;
  char cmd[1024] = {0};
  char err_msg[1024]= "\0";

  NSDL2_VARS(NULL, NULL, "Method called file_name = %s, groupTable = %p, group_idx = %d", file_name, groupTable, group_idx);

  if(groupTable[group_idx].absolute_path_flag != 1)
  {
    strcpy(tmp_file_name, file_name);
    path = dirname(tmp_file_name);
    if(!path)
    {
      NSDL2_VARS(NULL, NULL, "Unable to get dirname of path = %s", file_name);
      return -1;
    }
    NSDL2_VARS(NULL, NULL, "path = %s", path);
    sprintf(cmd, "mkdir -p %s; chmod 0775 %s", path, path);
    
    if(nslb_system(cmd,1,err_msg) != 0) {
       NSTL1_OUT(NULL, NULL, "Unable to create directory %s due to error: %s\n", path, nslb_strerror(errno));
    }

  }
 
  if((fd = open(file_name, O_WRONLY|O_APPEND|O_CREAT|O_CLOEXEC, 0666)) == -1)
  {
     NSTL1_OUT(NULL, NULL, "Unable to open file %s, due to error: %s\n", file_name, nslb_strerror(errno));
     return -1;
  }

  if(write(fd, sqlVarTable->query_result, sqlVarTable->query_result_size) == -1)
  {
    NSTL1_OUT(NULL, NULL, "Unable to save query result into file %s, due to error: %s\n", file_name, nslb_strerror(errno));
    return -1;
  }

  write(fd, "\n\n",2); 
  return 0;
}

 
