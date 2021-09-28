#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <getopt.h>
#include <libgen.h>
#include <stdarg.h>

#ifdef COPY_CMD
#include "/usr/include/libpq-fe.h"
#else
#include "/usr/include/postgresql/libpq-fe.h"
#endif

#include "nslb_partition.h"
#include "nslb_util.h"
#include "nslb_db_util.h"
#include "nslb_db_upload_common.h"

#define UNKNOWN -1
#define NS       0
#define ND       1

db_upload fillUploaderInitValues;
const char *conninfo;
PGconn *dbconnection;
PGresult *res;
char machine_type[256];
char nsWdir[512]; 
char module = UNKNOWN;
char out_file[1024];
char error_file[1024];

static FILE *db_out_fp;
static FILE *db_error_fp; 

#define CLEAR_WHITESPACES_AND_NEWLINE(ptr, buffer, loc_buffer)	\
{						                \
  ptr = buffer;					                \
  while(ptr){					                \
    if(*ptr == ' ' || *ptr == '\t' || *ptr == '\n')	        \
      ptr++;					                \
    else{					                \
      strcpy(loc_buffer, ptr);			                \
      break;					                \
    }						                \
  }						                \
  ptr = (loc_buffer + strlen(loc_buffer)) - 1;	        \
  while(ptr){					                \
    if(*ptr == ' ' || *ptr == '\t' || *ptr == '\n')	        \
      *(ptr--) = '\0';				                \
    else   break;				                \
  }						                \
}

void inline write_db_log_file(FILE *fp, char *format, ...)
{
  char buffer[4096 + 1];
  char time_str[100];
  int amt_written, buf_len;
  va_list ap;

  amt_written = snprintf(buffer, 4096, "%s|", nslb_get_cur_date_time(time_str, 1));
  va_start (ap, format);
  buf_len = vsnprintf(buffer + amt_written, 4096 - amt_written, format, ap);
  va_end(ap);

  //format wrong then vsnprintf returns -1
  if(buf_len < 0)
    buf_len = strlen(buffer);
  else
    buf_len += amt_written;

  /* current time|message  */
  buffer[buf_len] = '\n';
  buffer[buf_len + 1] = '\0';
  fwrite(buffer, sizeof(char), buf_len + 1, fp);
}

static void usage(char *err) 
{
 write_db_log_file(db_out_fp, "Error in db_tbl_mgr");
 write_db_log_file(db_error_fp, "\nUsage:\n    db_tbl_mgr  <-t/--testrun TrNum> <-p/--partition> <-m/--module> <-c/--create_tables>\n"
                 "               <-i/--create_indexes> <-d/--drop_tables> <-M/--machine_type MachineType> <-w/--Ns_wdir NS_WDIR> <-O/--OutputFile>"
                 "               <-E/--ErrorFile>\n"
		 "where : \n"
 		 "TrNum :  It is the Test Run Number for which tables need to be dropped.\n"
 		 "         Partition    : If only tables of 1 partition need to be dropped.\n"
 		 "         Module	: Can have two values : NS/ND\n"
 		 "         Machine Type : It is CONFIG mentioned in /home/cavisson/etc/cav.conf (NDE/NS/NV). This is optional\n"
 		 "         Ns_wdir      : It is NS_WDIR path where shell to be run. This is optionali\n"
                 "         OutputFile   : It is out_file path/home/cavisson/logs/TR/ready_reports/TestInitStaus/4_databaseCreation.log.This is mandetory\n."
                 "         ErrorFile    : It is error_file path if partition index less then equal to 0 /home/cavisson/logs/TR/ns_logs/NS:ND.log"
                 "                                              else /home/cavisson/logs/TR/partition/ns_logs/NS:ND.log");
 exit(1);
}

char *nsi_upload_tmp_table [] = {"UserProfile", "ErrorCodes", "ActualServerTable", 
                                 "RecordedServerTable", "SessionProfile", "LogPhaseTable", ""};

int createDBConnection()
{
  conninfo = "dbname=test user=cavisson";
  /* make a connection to the database */
  write_db_log_file(db_out_fp, "Creating DB connection");
  dbconnection = PQconnectdb(conninfo);
  /* Check to see that the backend connection was successfully made */
  if (PQstatus(dbconnection) != CONNECTION_OK)
  {
    write_db_log_file(db_error_fp, "Connection to database failedi: %s", PQerrorMessage(dbconnection));
    write_db_log_file(db_out_fp, "Connection to database failed: %s", PQerrorMessage(dbconnection));
    if(dbconnection)
      PQfinish(dbconnection);
    dbconnection = NULL;
    return -1;
  }
  write_db_log_file(db_out_fp, "DB connection created successfully");
  return 0;
}

int executeCmd(char *cmd)
{
  /* If connection to DB is not present then create it. */
  if(!dbconnection)
    createDBConnection();

  write_db_log_file(db_out_fp, "Executing DB command \n%s", cmd);
  res = PQexec(dbconnection, cmd);

  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    write_db_log_file(db_error_fp, "Query '%s' failed %s", cmd, PQerrorMessage(dbconnection));
    write_db_log_file(db_out_fp, "DB command failed \n%s", cmd);
    PQclear(res);
    return -1;
  }
  write_db_log_file(db_out_fp, "DB command executed successfully");
  return 0;
}

int createTable(int trNum, int totalEntries, db_table_info_t *createTableInitStruct)
{
  char createTableCmd[10*1024] = "";
  int idx = 0;

  while(idx < totalEntries){
    sprintf(createTableCmd, "CREATE TABLE %s_%d (%s) %s", createTableInitStruct[idx].csvtable, trNum,
                             createTableInitStruct[idx].dbSchemaStruct.schema, createTableInitStruct[idx].tableSpaceCmd);
    executeCmd(createTableCmd);
    idx++;
  }
  return 0;
}

int createIndex(int trNum, int totalEntries, db_table_info_t *createTableInitStruct)
{
  char createIndexCmd[10*1024] = "";
  char *indexFields[128];
  int i, idx = 0;
  int numFields = 0;
  int tokens = 0;
  char *fields[3] = {0};
  char *ptr;
  char indexBuffer[1024];
  char columnName[1024];
  char aliasName[1024];
  char moduleName[1024];   //NS/NDE/NV

  while(idx < totalEntries)
  {
    /* If the index is null */
    if(!createTableInitStruct[idx].dbSchemaStruct.index) return -1;

    if((numFields = get_tokens_with_multi_delimiter(createTableInitStruct[idx].dbSchemaStruct.index, indexFields, "|", 128)) > 128)
    {
      write_db_log_file(db_error_fp, "Error : CSV Configuration file cannot have fields greater than 128.");
      return -1;
    }

    for(i = 0; i < numFields; i++)
    {
      /* Now the particular indexFields can contain ' ', '\t' and '\n'. So we need to remove these. */
      //ptr = indexFields[i];
      CLEAR_WHITESPACES_AND_NEWLINE(ptr, indexFields[i], indexBuffer);
    
      if((tokens = get_tokens_with_multi_delimiter(indexBuffer, fields, ">", 3)) < 2)
      {
        write_db_log_file(db_error_fp, "Error : Bad index format for '%s'.", indexBuffer);
        continue;
      }

      /* Clear the whitespaces and newline from the fields */
      CLEAR_WHITESPACES_AND_NEWLINE(ptr, fields[0], columnName);
      CLEAR_WHITESPACES_AND_NEWLINE(ptr, fields[1], aliasName);

      //if modulename is provided in file nd_db_schema.conf
      if(tokens > 2)
      {
        CLEAR_WHITESPACES_AND_NEWLINE(ptr, fields[2], moduleName);
      }
      else
      {
        moduleName[0] = '\0';
      }

      /* NOTE :
         The case when composite indexes are used with spaces between them are not handled.
         So while writing composite indexes in the schema conf file, composite fields should
         be separated by ',' only. No whitespaces should be used.
      */

      /* Cases when index will be created
       *   1.  modulename is not given in conf file.
       *   2.  We could not get machine type.
       *   3.  modulename is not ALL and machine type is not matched with modulename
       *
       * Note: moduleName is list of machine types on which user doesn't want to create index.
       *       If modulename is ALL then index will not be created on any machine type.
       *       If modulename is NDE then index will not be created on NDE machine type.
       *       If modulename is NS,NDE then index will not be created on NS and NDE machine type.
       */
      if(!moduleName[0] || !machine_type[0] || ((strcasecmp(moduleName, "!ALL") != 0) && (strstr(moduleName, machine_type) == NULL)))
      {
        sprintf(createIndexCmd, "CREATE INDEX %s_%s_%d ON %s_%d (%s) %s;", 
                               createTableInitStruct[idx].csvtable, aliasName, trNum, 
                               createTableInitStruct[idx].csvtable, trNum, columnName, 
                               createTableInitStruct[idx].tableSpaceCmd);
        executeCmd(createIndexCmd);
      }
    }

    idx++;
  }
  return 0;
}

int dropTable(int trNum, long long partition_idx, int totalEntries, db_table_info_t *dropTableInitStruct)
{
  char dropTableCmd[10*1024] = "";
  int idx = 0;

  while(idx < totalEntries)
  {
    dropTableCmd[0] = '\0';

    /* Drop main tables */
    if(partition_idx <= 0)
      sprintf(dropTableCmd, "DROP TABLE IF EXISTS %s_%d CASCADE", dropTableInitStruct[idx].csvtable, trNum);
    /* Drop partition tables. Only dynamic tables are created in partition */
    else if(dropTableInitStruct[idx].fileType == 1 || dropTableInitStruct[idx].fileType == 3)
      sprintf(dropTableCmd, "DROP TABLE IF EXISTS %s_%d_%lld CASCADE", dropTableInitStruct[idx].csvtable, trNum, partition_idx);

    if(dropTableCmd[0] != '\0')
      executeCmd(dropTableCmd);

    idx++;
  }

  if(partition_idx > 0 || module == ND)
    return 0;

  /* Few NS main tables are created and uploaded by shell script nsi_upload_tmp_table.
   * Dropping these tables */
  idx = 0;
  while(nsi_upload_tmp_table[idx][0] != '\0')
  {
    sprintf(dropTableCmd, "DROP TABLE IF EXISTS %s_%d CASCADE", nsi_upload_tmp_table[idx], trNum);    
    executeCmd(dropTableCmd);
    idx++;
  } 
  return 0;
}

//get machine type from /etc/cav_controller.conf if exists
//else get from /home/cavisson/etc/cav.conf
void get_machine_type(char *machine_type)
{
  FILE *fp_conf = NULL;
  char read_buf[1024 + 1] = {0};
  char temp1[512] = {0};
  char temp2[512] = {0};
  char controller_name[256] = {0};

  //basename may modify it's input string, hence copying to temp1
  strcpy(temp1, nsWdir);
  strcpy(controller_name, basename(temp1));

  sprintf(temp1, "/home/cavisson/etc/cav_%s.conf", controller_name);
  fp_conf = fopen(temp1, "r");

  if(fp_conf == NULL)
  {
    fp_conf = fopen("/home/cavisson/etc/cav.conf", "r");
    if(fp_conf == NULL)
      return;
  }

  while(fgets(read_buf, 1024, fp_conf))
  {
    if (!strncmp(read_buf, "CONFIG", 6))
      sscanf(read_buf,"%s %s", temp1, temp2);
  }

  //We need to compare !NDE/!NS etc, hence saving in this format
  sprintf(machine_type, "!%s", temp2); 

  if(fp_conf)
    fclose(fp_conf);
}

int main(int argc, char *argv[])
{
  /*  Creating Table,  Creating Indexes, Dropping table */
  int create_table = 0;
  int drop_table = 0;
  int create_index = 0;
  int nsWdir_flag = 0;

  int trNum = 0;
  long long partition_idx = -1;
  char oldTRMigrationHiddenFile[1024];
  struct stat oldTRHiddenFileBuf;
  /*char oracle_stats_mon_flag_file[1024];
  char sql_stats_mon_flag_file[1024];
  struct stat fstat;*/
  char c;
  db_error_fp=stderr;
  db_out_fp=stdout; 
  struct option longopts[] = {
                               {"create_tables", 0, NULL, 'c'},
                               {"drop_tables",  0, NULL, 'd'},
                               {"create_indexes",  0, NULL, 'i'},
                               {"testrun",  1, NULL, 't'},
                               {"partition",  1, NULL, 'p'},
                               {"module",  1, NULL, 'm'},
                               {"machine_type",  1, NULL, 'M'},
                               {"Ns_wdir",  1, NULL, 'w'},
                               {0, 0, 0,0}
                             };


  while ((c = getopt_long(argc, argv, "cdit:p:m:M:w:O:E:", longopts, NULL)) != -1)
  {
    switch (c)
    {
      case 'c': create_table = 1;
                break;
      case 'd': drop_table = 1;
                break;
      case 'i': create_index = 1;
                break;
      case 't': trNum = atoi(optarg);
                break;
      case 'p': partition_idx = atoll(optarg);
                break;
      case 'm': if(strcasecmp(optarg, "NS") == 0) module = NS;
                else if(strcasecmp(optarg, "ND") == 0) module = ND;
                else usage("Please provide correct module name");
                break;
      case 'M': strcpy(machine_type, optarg);
                break;
      case 'w': strcpy(nsWdir, optarg);
                nsWdir_flag = 1;
                break;
      case 'O': strncpy(out_file, optarg, 1024);
                break;
      case 'E': strncpy(error_file, optarg, 1024);
                break;
      case '?': fprintf(db_error_fp, "Error : Invalid option to getopt.\n");
                exit(-1);
    }
  }
  if(out_file[0])
  {
    db_out_fp = fopen(out_file, "a");
    if(!db_out_fp)
      db_out_fp = stdout;
  }
  if(error_file[0])
  {
    db_error_fp = fopen(error_file, "a");
    if(!db_error_fp)
      db_error_fp = stderr;
  }


  //TRxxx/nd/logs/.ndp_version
  //fprintf(db_error_fp, "argc = %d\n", argc);
  //for(i = 0; i < argc; i++)
  //  fprintf(db_error_fp, "argv[%d] = %s\n", i, argv[i]);

  if(!trNum)
    usage("Please provide testrun number");

  if(module == UNKNOWN)
    usage("Please provide module");

  if(!create_table && !drop_table && !create_index)
    usage("Please provide any table operation, createTable/createIndex/dropTable");

  if(drop_table && (create_table || create_index))
    usage("Drop table cannot be given with another operation");

  if((partition_idx > 0) && (create_table || create_index))
    usage("Partition is supported in drop table operation only");

  if(!nsWdir_flag)
  {
    if (getenv("NS_WDIR") != NULL)
      sprintf(nsWdir, "%s", getenv("NS_WDIR"));
    else
    {
      usage("Please provide NS_WDIR Path as it is not set");
    }
  }

  //if machine type is passed then save it; otherwise get from /home/cavisson/etc/cav.conf
  if(machine_type[0] == '\0')
    get_machine_type(machine_type);

  if(module == NS)
  {
    fillUploaderInitValues.module=-1;
    sprintf(fillUploaderInitValues.CSVConfigFilePath, "%s/etc/dbconf/ns_db_csv.conf", nsWdir);
    sprintf(fillUploaderInitValues.schemaConfFilePath, "%s/etc/dbconf/ns_db_schema.conf", nsWdir);
  }
  else if (module == ND)
  {
    sprintf(fillUploaderInitValues.CSVConfigFilePath, "%s/etc/dbconf/nd_db_csv.conf", nsWdir);

    //Checking a version file created by NDP so ato provide migration for NDDBU
    sprintf(oldTRMigrationHiddenFile, "%s/logs/TR%d/nd/logs/.ndp_version", nsWdir, trNum);
    if(stat(oldTRMigrationHiddenFile, &oldTRHiddenFileBuf) != 0)
      sprintf(fillUploaderInitValues.schemaConfFilePath, "%s/etc/dbconf/nd_db_schema_old_tr.conf", nsWdir);
    else 
      sprintf(fillUploaderInitValues.schemaConfFilePath, "%s/etc/dbconf/nd_db_schema.conf", nsWdir);
  }
  
  sprintf(fillUploaderInitValues.base_dir, "%s/logs/TR%d", nsWdir, trNum);

  /*if(module == NS)
  {
    sprintf(oracle_stats_mon_flag_file, "%s/logs/TR%d/.oracle_sql_report", nsWdir, trNum);
 
    NSLB_TRACE_LOG1(fillUploaderInitValues.trace_log_key, fillUploaderInitValues.partition_idx, "Main Thread", NSLB_TL_INFO, 
                    "Checking For presence of Oracle Monitor file [%s]", oracle_stats_mon_flag_file);
 
    if(stat(oracle_stats_mon_flag_file, &fstat) != 0)  
    {
      sprintf(oracle_stats_mon_flag_file, "%s/logs/TR%d/common_files/.oracle_sql_report", nsWdir, trNum);
      if(stat(oracle_stats_mon_flag_file, &fstat) == 0)  
      {
        fillUploaderInitValues.enable_oracle_stats_reports_threads = 1;
      }
    }

    //sprintf(sql_stats_mon_flag_file, "%s/logs/TR%d/.mssql_report", nsWdir, trNum);
    //BUG 84397
    sprintf(sql_stats_mon_flag_file, "%s/logs/TR%d/.genericDb_report", nsWdir, trNum);
    NSLB_TRACE_LOG1(fillUploaderInitValues.trace_log_key, fillUploaderInitValues.partition_idx, "Main Thread", NSLB_TL_INFO,
                    "Checking For presence of SQL Monitor file [%s]", sql_stats_mon_flag_file);

    if(stat(sql_stats_mon_flag_file, &fstat) != 0)
    {
      //sprintf(sql_stats_mon_flag_file, "%s/logs/TR%d/common_files/.mssql_report", nsWdir, trNum);
      //BUG 84397
      sprintf(sql_stats_mon_flag_file, "%s/logs/TR%d/common_files/.genericDb_report", nsWdir, trNum);
      if(stat(sql_stats_mon_flag_file, &fstat) == 0)
      {
        fillUploaderInitValues.enable_mssql_stats_reports_threads = 1;
      }
    }
  }*/

  fillUploaderInitValues.controller = basename(nsWdir);
  check_machine_type_is_sm(&fillUploaderInitValues);

  /* Parsing Configuration NDDBSchema.conf and populating structures. */
  if(CSVConfigParser(&fillUploaderInitValues))
  {
    write_db_log_file(db_error_fp, "Error : %s", fillUploaderInitValues.error);
    write_db_log_file(db_out_fp, "CSVConfigParser Error : %s", fillUploaderInitValues.error);
    exit(EXIT_FAILURE);
  }

  /* Parsing Configuration NDCSVFiles.conf and populating structures. */
  if(schemaFileParser(&fillUploaderInitValues))
  {
    write_db_log_file(db_error_fp, "Error : %s", fillUploaderInitValues.error);
    write_db_log_file(db_out_fp, "schemaFileParser Error : %s", fillUploaderInitValues.error);
    exit(EXIT_FAILURE);
  }
  

  if(create_table == 1 || create_index == 1)
  {
    /* Parse and fill table space info in structure */
    if(parseTableSpaceInfo(&fillUploaderInitValues) < 0)
    {
      exit(-2);
    }
  }

  if(create_table == 1)
  {
    /* Creating Static CSV tables in DB. */
    createTable(trNum, fillUploaderInitValues.totalStaticCsvEntry, fillUploaderInitValues.dbUploaderStaticCSVInitStruct);
    /* Creating Dynamic CSV tables in DB. */
    createTable(trNum, fillUploaderInitValues.totalCsvEntry, fillUploaderInitValues.dbUploaderDynamicCSVInitStruct);
  }
  
  if(create_index == 1)
  {
    /* Creating Static CSV indexes in DB. */
    createIndex(trNum, fillUploaderInitValues.totalStaticCsvEntry, fillUploaderInitValues.dbUploaderStaticCSVInitStruct);
    /* Creating Dynamic CSV indexes in DB. */
    createIndex(trNum, fillUploaderInitValues.totalCsvEntry, fillUploaderInitValues.dbUploaderDynamicCSVInitStruct);
    
    /*generic db create views*/
    /*if(fillUploaderInitValues.enable_mssql_stats_reports_threads == 1)
    {
      char cmd[512];
      char err_msg[1024] = "\0";
 
      sprintf(cmd, "%s/bin/generic_db_mon_views_creation %d", nsWdir, trNum);
     
      if(nslb_system(cmd,1,err_msg) != 0)
        write_db_log_file(stderr, "Error while executing cmd[%s] checks logs in FILE[%s/logs/TR%d/.genericDBMonitorViewsLogs]",                           cmd, nsWdir, trNum);
    }*/
  }
 
  if(drop_table == 1)
  { 
    /* Dropping Static CSV tables in DB. */
    dropTable(trNum, partition_idx, fillUploaderInitValues.totalStaticCsvEntry, fillUploaderInitValues.dbUploaderStaticCSVInitStruct);   
    /* Dropping Dynamic CSV tables in DB. */
    dropTable(trNum, partition_idx, fillUploaderInitValues.totalCsvEntry, fillUploaderInitValues.dbUploaderDynamicCSVInitStruct);
  }

  /* Close the db connection */
  if(dbconnection)
  {
    PQfinish(dbconnection);
    dbconnection = NULL;
  }
  return 0;
}
