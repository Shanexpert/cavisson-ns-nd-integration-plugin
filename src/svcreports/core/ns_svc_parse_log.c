/***********************************************************************************************
 *  File Name            : ns_svc_parse_log.c
 *  Author(s)            : Abhishek, Manmeet
 *  Date                 : 02/22/2013
 *  Copyright            : (c) Cavisson Systems
 *  Purpose              : Service Reports porcessing
 *                         
 *  Modification History :
 *               <Author(s)>, <Date>, <Change Description/Location>
 ************************************************************************************************/

#define _GNU_SOURCE /* defined for versionsort() used in scandir() */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <time.h>
#include <dirent.h>
#include <pthread.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>
#include <sys/socket.h>
#include <sys/time.h>


#include <libpq-fe.h>
#include <libgen.h>
#include <signal.h>


#include "../../libnscore/nslb_db_util.h"
#include "../../libnscore/nslb_util.h"
#include "../../libnscore/nslb_get_norm_obj_id.h"
#include "../../libnscore/nslb_buffer_pool.h"

#include "ns_svc_parse_log.h"
#ifndef BUILD_SVC_ALONE
#include "../../lps/lps_log_parser.h"
#include "../../lps/lps_log.h"
#else
#define MAX_LINE_LENGTH 128 * 1024
#endif

#define CLEAR_WHITE_SPACE(ptr) {while ((*ptr == ' ') || (*ptr == '\t')) ptr++;}

#define FREE_AND_MAKE_NULL(to_free, msg, index)  \
{                        \
  if (to_free)   \
  { \
    LPSDL1(NULL, "MY_FREE'ed (%s) done. Freeing ptr = $%p$ for index %d", msg, to_free, index); \
    free((void*)to_free);  \
    to_free = NULL; \
  } \
}

#define SVC_MALLOC(new, sz, msg, ptr) \
  { \
    if(sz >= 0) \
    { \
       new = (void *) malloc (sz); \
       SVC_DL2(ptr, threadIndex, "Malloced  $%p$ size %llu, msg=%s", new, (long long unsigned int) sz, msg); \
    } \
  }

#define SVC_REALLOC(new, old, sz, msg, ptr) \
  { \
    if(sz >= 0) \
    { \
       void *tmpnew, *tmpold = old; \
       tmpnew = (void *) realloc (old, sz); \
       if(!tmpold) \
       { \
         SVC_DL2(ptr, threadIndex, "Malloced  $%p$ size %llu, msg=%s", tmpnew, (long long unsigned int) sz, msg); \
       } \
       else if(tmpold != tmpnew) \
       { \
         SVC_DL2(ptr, threadIndex, "Freed  $%p$ size %llu, msg=%s", tmpold, (long long unsigned int) sz, msg); \
         SVC_DL2(ptr, threadIndex, "Realloced  $%p$ size %llu, msg=%s", tmpnew, (long long unsigned int) sz, msg); \
       } \
       new = tmpnew;\
    } \
  }

#define SVC_FREE(to_free, msg, ptr) \
  { \
    if(to_free) \
    { \
       free (to_free); \
       SVC_DL2(ptr, svc_info->threadIndex, "Freed pointer $%p$, msg=%s", to_free, msg); \
       to_free = NULL; \
    } \
  }

inline void svc_close(service_stats_info *ss_mon_info);

enum svc_keword {
  SERVICE_EXIT_SEARCH_PATTERN = 0,
  SERIVCE_STATUS,
  SERVICE_ELAPSED_TIME,
  COMPONENT_SOR_RESPONSE_TIME_SEARCH_PATTERN,
  COMPONENT_SOR_RESPONSE_TIME,
  COMPONENT_RESPONSE_TIME,
  COMPONENT_QUEUE_WAIT_TIME,
  COMPONENT_TOTAL_RESPONSE_TIME,
  COMPONENT_STATUS,
};

/* csv file names */
char svcMetaDataCSVFileName[] = "SvcTable.csv";
char svcRecordCSVFileName[] = "SvcRecord.csv";
char svcCompRecordCSVFileName[] = "SvcCompRecord.csv";
char svcErrorCodeCSVFileName[] = "SvcErrorCode.csv";
char svcSignatureCSVFileName[] = "SvcSignatureTable.csv";
char svcInstanceCSVFileName[] = "SvcInstanceTable.csv";

char svcMetaDataTableName[] = "SvcTable_";
char svcRecordTableName[] = "SvcRecord_";
char svcCompRecordTableName[] = "SvcCompRecord_";
char svcErrorCodeTableName[] = "SvcErrorCode_";
char svcSignatureTableName[] = "SvcSignatureTable_";
char svcInstanceTableName[] = "SvcInstanceTable_";


static FILE *my_fopen(char *filename, char *mode);

#define CREATE_FILE(threadIndex, fp, filename, buf, ptr) \
  fp = my_fopen(filename, "w"); \
  if(fp) {fclose(fp); fp = NULL;} \
  sprintf(buf, "chown %s:%s %s", ownername, grpname, filename); \
  system(buf);

#ifdef BUILD_SVC_ALONE
char ns_wdir[1024] = "/home/netstorm/work/";
char dbmode;
int testidx = 0;
int svc_loglevel = 0;
int logdir_created = 0;
int logfile_rollover_size = 1;
char logfilename[512] = "";
FILE *logfp = NULL;
service_stats_info *ss_mon_info;

static int my_printf(void *ptr, int threadIndex, char *format, ...)
{
  int ret = 0;
  va_list ap;
  struct stat st; 
  char cmdbuf[1024];
  //char tmpbuf[512];
  FILE **p_logfp = NULL;
  char *loc_logfilename = NULL;
  char *ownername = "netstorm";
  char *grpname = "netstorm";
  if(ptr == NULL)
    return -1;

  if(threadIndex == -1)
  {
    if (!((service_stats_info *)ptr)->logfp){
      va_start (ap, format);
        ret = vfprintf(stderr, format, ap);
      va_end(ap);
      return ret;
    }
    p_logfp = &((service_stats_info *)ptr)->logfp;
    loc_logfilename = ((service_stats_info *)ptr)->logfilename;
  }
  else
  {
    p_logfp = &((svc_info_t*) ptr)->logfp;
    loc_logfilename = ((svc_info_t*) ptr)->logfilename;
  }

  if(svc_loglevel)
  {
    va_start (ap, format);
      ret = vfprintf(*p_logfp, format, ap);
    va_end(ap);
    fprintf(*p_logfp, "\n");

    stat(loc_logfilename, &st); 

    if (st.st_size >= logfile_rollover_size * LOGFILE_ROLLOVER_SIZE_IN_MB * 1024 * 1024)
    {
      fclose (*p_logfp);
      *p_logfp = NULL;

      sprintf(cmdbuf, "mv %s/logs/TR%d/svc/logs/svc_debug%s%s.log "
                      "%s/logs/TR%d/svc/logs/svc_debug%s%s_prev.log",
                      ns_wdir, testidx, threadIndex>=0?"_":"",
                      threadIndex>=0?((svc_info_t*)ptr)->instance:"",
                      ns_wdir, testidx, threadIndex>=0?"_":"",
                      threadIndex>=0?((svc_info_t*)ptr)->instance:"");

      system(cmdbuf);

      CREATE_FILE(-1, *p_logfp, loc_logfilename, cmdbuf, NULL);
      *p_logfp = my_fopen(loc_logfilename, "w");
      if(*p_logfp == NULL)
      {
        svc_loglevel = 0;
        fprintf(stderr, "ERROR opening svc logfile for writing %s\n", loc_logfilename);
      }
    }else
      fflush(*p_logfp); 
  }
  return ret;
}



static char *get_cur_date_time()
{
  long    tloc;
  struct  tm *lt;
  static  char cur_date_time[100];

  (void)time(&tloc);
  if((lt = localtime(&tloc)) == (struct tm *)NULL)
    strcpy(cur_date_time, "Error|Error");
  else
    sprintf(cur_date_time, "%02d/%02d/%02d %02d:%02d:%02d",  lt->tm_mon + 1, lt->tm_mday, (1900 + lt->tm_year)%2000, lt->tm_hour, lt->tm_min, lt->tm_sec);
  return(cur_date_time);
}

int SVCEL(char *format, ...)
{
  int ret = 0;
  va_list ap;
  va_start (ap, format);
    ret += vfprintf(stderr, format, ap);
  va_end(ap);
  ret += fprintf(stderr, "\n");

  return ret;
}

#define SVC_LOG(ptr, threadIndex, ...) \
  { \
    if(threadIndex == -1) \
    { \
      if(logfp){ \
        fprintf(logfp, "%s|%s|%d|%s|%d|", get_cur_date_time(), basename(__FILE__), __LINE__, __FUNCTION__, threadIndex); \
        my_printf(ptr, -1, __VA_ARGS__); \
      } \
    } \
    else \
    { \
      if(((svc_info_t*) ptr)->logfp) \
      { \
        fprintf(((svc_info_t*) ptr)->logfp, "%s|%s|%d|%s|%d|", get_cur_date_time(), basename(__FILE__), __LINE__, __FUNCTION__, threadIndex); \
        my_printf(ptr, threadIndex, __VA_ARGS__); \
      } \
    } \
  }

#define SVC_DL1(ptr, threadIndex, ...) if(svc_loglevel >= 1) { SVC_LOG(ptr, threadIndex, __VA_ARGS__)}
#define SVC_DL2(ptr, threadIndex, ...) if(svc_loglevel >= 2) { SVC_LOG(ptr, threadIndex, __VA_ARGS__)}
#define SVC_DL3(ptr, threadIndex, ...) if(svc_loglevel >= 3) { SVC_LOG(ptr, threadIndex, __VA_ARGS__)}
#define SVC_DL4(ptr, threadIndex, ...) if(svc_loglevel >= 4) { SVC_LOG(ptr, threadIndex, __VA_ARGS__)}
#define LPSDL1(ptr, ...) 
#define LPSDL2(ptr, ...) 
#define LPSDL3(ptr, ...) 
#define LPSDL4(ptr, ...) 
#define LPSEL1(ptr, ...) 

#else

#define SVC_DL1(ptr, threadIndex, ...) { LPSDL1(NULL,  __VA_ARGS__);}
#define SVC_DL2(ptr, threadIndex, ...) { LPSDL2(NULL,  __VA_ARGS__);}
#define SVC_DL3(ptr, threadIndex, ...) { LPSDL3(NULL,  __VA_ARGS__);}
#define SVC_DL4(ptr, threadIndex, ...) { LPSDL4(NULL,  __VA_ARGS__);}
#define SVCEL(...) LPSEL4(NULL,  __VA_ARGS__)
#endif

/*#define SVC_DL1(threadIndex, ...) 
#define SVC_DL2(threadIndex, ...) 
#define SVC_DL3(threadIndex, ...) 
#define SVC_DL4(threadIndex, ...) */
static FILE *my_fopen(char *filename, char *mode)
{
  int fd;
  FILE *fp = NULL;
  int flags = O_CREAT|O_LARGEFILE;

  if(!strcmp(mode, "w") || !strcmp(mode, "w+"))
    flags |= O_TRUNC|O_WRONLY;

  else if(!strcmp(mode, "r"))
    flags |= O_RDONLY;

  else
    flags |= O_RDWR;
   
  fd = open(filename, flags, 00666);
  if(fd  == -1)
  {
    SVC_DL2(NULL, -1, "Error in opening file: %s", filename);
    perror("Error in opening file");

    return NULL;
  }

  fp = fdopen(fd, mode);

  if(fp == NULL)
  {
    SVC_DL2(NULL, -1, "Error in opening file: %s", filename);
    perror("Error in opening file");
    return NULL;
  }

  return fp;
}



static inline void read_owner_group(service_stats_info *ss_mon_info)
{
  char filename[256];
  struct stat s;
  struct passwd *pwd;
  struct group *grp;

  sprintf(filename, "%s/logs/TR%d", ss_mon_info->svc_ns_wdir, ss_mon_info->svc_testidx);
  stat(filename, &s);
  pwd = getpwuid(s.st_uid);
  if(pwd)
    strcpy(ss_mon_info->ownername, pwd->pw_name);
  grp = getgrgid(s.st_gid);
  if(grp)
    strcpy(ss_mon_info->grpname, grp->gr_name);
}

static inline void close_db_conn_and_clear_result_set(PGconn *conn, PGresult *res)
{
  if(conn)  {
    PQfinish(conn);
    conn = NULL;
  }
  if(res)  {
    PQclear(res);
    res = NULL;
  }
}

static inline int svc_copy_data_in_db(svc_info_t *svc_info, PGconn *conn,char *tablename, char *buf, int len)
{
  PGresult *res;
  int ret;
  char copy_cmd[1024];
  char tname[64];
  int testidx = ((service_stats_info*)(svc_info->ss_mon_info))->svc_testidx;

  sprintf(tname, "%s%d", tablename, testidx);

  if(!conn)
    return -1;

  sprintf(copy_cmd, "COPY %s FROM STDIN WITH DELIMITER ',' NULL AS ''", tname);
  res = PQexec(conn, copy_cmd);
  if (PQresultStatus(res) != PGRES_COPY_IN)
  {
    SVC_DL2(svc_info, svc_info->threadIndex, "COPY command for %s table failed: '%s'", tname, PQerrorMessage(conn));
    close_db_conn_and_clear_result_set(conn, res);
    conn = NULL;
    return -1;
  }
  
  ret = PQputCopyData(conn, buf, len);
  if(ret == -1)
  {
    SVC_DL2(svc_info, svc_info->threadIndex, "PQputCopyData command failed: '%s'", PQerrorMessage(conn));
    close_db_conn_and_clear_result_set(conn, res);
    conn = NULL;
    return -1;
  }

  /* Ends the COPY_IN operation successfully if errormsg is NULL.
   * If errormsg is not NULL then the COPY is forced to fail,
   * with the string pointed to by errormsg used as the error message. */
  if(PQputCopyEnd(conn, NULL) != 1)
  {
    SVC_DL2(svc_info, svc_info->threadIndex, "PQputCopyEnd() command for %s table failed: '%s'", tname, PQerrorMessage(conn));
    close_db_conn_and_clear_result_set(conn, NULL);
    conn = NULL;
  }
  PQclear(res);
  return 0; 
}

static inline void  create_csv_files(svc_info_t *svc_info)
{
  char csvfilename[1024] = "", tmpbuf[1024] = "";
  FILE *fp = NULL;
  char *ns_wdir = ((service_stats_info*)(svc_info->ss_mon_info))->svc_ns_wdir;
  int testidx = ((service_stats_info*)(svc_info->ss_mon_info))->svc_testidx;
  char *ownername = ((service_stats_info*)(svc_info->ss_mon_info))->ownername;
  char *grpname = ((service_stats_info*)(svc_info->ss_mon_info))->grpname;
  char *dbmode = &((service_stats_info*)(svc_info->ss_mon_info))->dbmode;

  SVC_DL2(svc_info, svc_info->threadIndex, "Method Called");

  /* Meta Data */
  sprintf(csvfilename, "%s/logs/TR%d/svc/csv/%s/%s", 
                        ns_wdir, 
                        testidx, 
                        svc_info->instance, 
                        svcMetaDataCSVFileName);

  /* Create the directory if not exists */
  mkdir_ex(csvfilename);
  sprintf(csvfilename, "chown -R %s:%s %s/logs/TR%d/svc/", ownername, grpname,
                        ns_wdir, 
                        testidx);
  system(csvfilename);

  sprintf(csvfilename, "%s/logs/TR%d/svc/csv/%s/%s", 
                        ns_wdir, 
                        testidx, 
                        svc_info->instance, 
                        svcMetaDataCSVFileName);

  CREATE_FILE(svc_info->threadIndex, fp, csvfilename, tmpbuf, svc_info);

  /* Service Record */
  sprintf(csvfilename, "%s/logs/TR%d/svc/csv/%s/%s", 
                        ns_wdir, 
                        testidx, 
                        svc_info->instance, 
                        svcRecordCSVFileName);
  CREATE_FILE(svc_info->threadIndex, fp, csvfilename, tmpbuf, svc_info);

  /* Service Component Record */
  sprintf(csvfilename, "%s/logs/TR%d/svc/csv/%s/%s", 
                        ns_wdir, 
                        testidx, 
                        svc_info->instance, 
                        svcCompRecordCSVFileName);
  CREATE_FILE(svc_info->threadIndex, fp, csvfilename, tmpbuf, svc_info);

  /* Error Code */
  sprintf(csvfilename, "%s/logs/TR%d/svc/csv/%s/%s", 
                        ns_wdir, 
                        testidx, 
                        svc_info->instance, 
                        svcErrorCodeCSVFileName);
  CREATE_FILE(svc_info->threadIndex, fp, csvfilename, tmpbuf, svc_info);

  /* Signature Table */
  sprintf(csvfilename, "%s/logs/TR%d/svc/csv/%s/%s", 
                        ns_wdir, 
                        testidx, 
                        svc_info->instance, 
                        svcSignatureCSVFileName);
  CREATE_FILE(svc_info->threadIndex, fp, csvfilename, tmpbuf, svc_info);

  /* Instance Table */
  if(svc_info->threadIndex == 0)
  {
    sprintf(csvfilename, "%s/logs/TR%d/svc/csv/%s", 
                          ns_wdir, 
                          testidx, 
                          svcInstanceCSVFileName);
    CREATE_FILE(svc_info->threadIndex, fp, csvfilename, tmpbuf, svc_info);
    LPSDL1(NULL, "((service_stats_info*)(svc_info->ss_mon_info))->svc_instance_fp = %p", 
                   ((service_stats_info*)(svc_info->ss_mon_info))->svc_instance_fp);

    if(!((service_stats_info*)(svc_info->ss_mon_info))->svc_instance_fp)
    {
      LPSDL1(NULL, "opening file = '%s'", csvfilename);
      SVC_DL2(svc_info, svc_info->threadIndex, "opening file = '%s'", csvfilename);
      ((service_stats_info*)(svc_info->ss_mon_info))->svc_instance_fp = my_fopen(csvfilename, "w");
 
      if(((service_stats_info*)(svc_info->ss_mon_info))->svc_instance_fp)
      {
        fprintf(((service_stats_info*)(svc_info->ss_mon_info))->svc_instance_fp, "%s\n", SVC_INSTANCE_HEADER); 
        int len, i;
        for(i = 0; i < ((service_stats_info*)(svc_info->ss_mon_info))->total_svc_info; i++)
        {
          len = sprintf(csvfilename, "%d,%s\n", 
                        ((service_stats_info*)(svc_info->ss_mon_info))->svc_info[i].threadIndex, 
                        ((service_stats_info*)(svc_info->ss_mon_info))->svc_info[i].instance);
          fprintf(((service_stats_info*)(svc_info->ss_mon_info))->svc_instance_fp, "%s", csvfilename);
          if(*dbmode >= 1)
          {
            if(svc_copy_data_in_db(svc_info, svc_info->svc_db_con, svcInstanceTableName, csvfilename, len) == -1)
              *dbmode = 0;
          }
        }  
        fclose(((service_stats_info*)(svc_info->ss_mon_info))->svc_instance_fp);
      }
    }
  }


  if(!svc_info->svc_record_fp)
  {
    sprintf(csvfilename, "%s/logs/TR%d/svc/csv/%s/%s", ns_wdir, testidx, 
                         svc_info->instance, svcRecordCSVFileName);

    SVC_DL2(svc_info, svc_info->threadIndex, "opening file = '%s'", csvfilename);
    svc_info->svc_record_fp = my_fopen(csvfilename, "w");

    if(svc_info->svc_record_fp)
      fprintf(svc_info->svc_record_fp, "%s\n", SVC_RECORD_HEADER); 
  }

  if(!svc_info->svc_comp_record_fp)
  {
    sprintf(csvfilename, "%s/logs/TR%d/svc/csv/%s/%s", ns_wdir, testidx, 
                         svc_info->instance, svcCompRecordCSVFileName);

    SVC_DL2(svc_info, svc_info->threadIndex, "opening file = '%s'", csvfilename);
    svc_info->svc_comp_record_fp = my_fopen(csvfilename, "w");
    if(svc_info->svc_comp_record_fp)
      fprintf(svc_info->svc_comp_record_fp, "%s\n", SVC_COMP_RECORD_HEADER); 
  }

  if(!svc_info->svc_signature_table_fp)
  {
    sprintf(csvfilename, "%s/logs/TR%d/svc/csv/%s/%s", ns_wdir, testidx, 
                          svc_info->instance, svcSignatureCSVFileName);

    SVC_DL2(svc_info, svc_info->threadIndex, "opening file = '%s'", csvfilename);
    svc_info->svc_signature_table_fp = my_fopen(csvfilename, "w");
    if(svc_info->svc_signature_table_fp)
      fprintf(svc_info->svc_signature_table_fp, "%s\n", SVC_SIGNATURE_HEADER); 
  }

  if(!svc_info->svc_error_code_fp)
  {
    sprintf(csvfilename, "%s/logs/TR%d/svc/csv/%s/%s", ns_wdir, testidx, 
                          svc_info->instance, svcErrorCodeCSVFileName);

    SVC_DL2(svc_info, svc_info->threadIndex, "opening file = '%s'", csvfilename);
    svc_info->svc_error_code_fp = my_fopen(csvfilename, "w");
    if(svc_info->svc_error_code_fp)
      fprintf(svc_info->svc_error_code_fp, "%s\n", SVC_ERRCODE_HEADER);
  }

  if(!svc_info->svc_table_fp)
  {
    sprintf(csvfilename, "%s/logs/TR%d/svc/csv/%s/%s", ns_wdir, testidx, 
                          svc_info->instance, svcMetaDataCSVFileName);

    SVC_DL2(svc_info, svc_info->threadIndex, "opening file = '%s'", csvfilename);
    svc_info->svc_table_fp = my_fopen(csvfilename, "w");
    if(svc_info->svc_table_fp)
      fprintf(svc_info->svc_table_fp, "%s\n", SVC_TABLE_HEADER);
  }
}


static inline void svc_process_svc_error_code(svc_info_t *svc_info, char *status, int statusId, char force_dump) 
{

  char *dbmode = &((service_stats_info*)(svc_info->ss_mon_info))->dbmode;
  if(svc_info->svc_error_code_fp)
  {
    fprintf(svc_info->svc_error_code_fp, "%d,%s\n", statusId, status);
    fflush(svc_info->svc_error_code_fp);
  }
  
  if(*dbmode >= 1)
  {
    char buf[ 16 + strlen(status)];
    int len;
    
    len = sprintf(buf, "%d,%s\n", statusId, status);
    if(svc_copy_data_in_db(svc_info, svc_info->svc_db_con, svcErrorCodeTableName, buf, len) == -1) 
      *dbmode = 0;
  }
}

static inline void svc_process_svc_signature_table_data(svc_info_t *svc_info, char *svcSigName, int svcSigId, char force_dump)    
{
  char *dbmode = &((service_stats_info*)(svc_info->ss_mon_info))->dbmode;
  if(svc_info->svc_signature_table_fp)
  {
    fprintf(svc_info->svc_signature_table_fp, "%d,%s\n", svcSigId, svcSigName);
    fflush(svc_info->svc_signature_table_fp);
  }

  if(*dbmode >= 1)
  {
    char buf[ 16 + strlen(svcSigName)];
    int len;
    
    len = sprintf(buf, "%d,%s\n", svcSigId, svcSigName);
    if(svc_copy_data_in_db(svc_info, svc_info->svc_db_con, svcSignatureTableName, buf, len) == -1) 
      *dbmode = 0;
  }
}

inline void svc_instance_init(svc_info_t *svc_info)
{
  int threadIndex = svc_info->threadIndex;
  char *dbmode = &((service_stats_info*)(svc_info->ss_mon_info))->dbmode;
  LPSDL1(NULL, "Method Called");
  svc_info->bufpool_key = nslb_bufpool_init(sizeof(sess_map_table_t), 
                                                        INIT_NUM_SESS_MAP_TABLE_ENTRIES, 
                                                        DELTA_NUM_SESS_MAP_TABLE_ENTRIES);
  if(svc_info->bufpool_key == NULL)
  {
    SVC_DL2(svc_info, threadIndex, "Error in create map table for thread = '%d'", threadIndex);
    exit(-1);
  }

  if(((service_stats_info*)(svc_info->ss_mon_info))->ddr)  
  {
    PGconn **svc_db_con = &svc_info->svc_db_con;
    if(*dbmode)
    {
      *svc_db_con = PQconnectdb("dbname=test user=netstorm");
      if (PQstatus(*svc_db_con) != CONNECTION_OK)
      {
        SVC_DL2(svc_info, threadIndex, "Connection to database failed: %s\n",
                         PQerrorMessage(*svc_db_con));
 
        *svc_db_con = NULL;
        *dbmode = 0;
      }
    }
    LPSDL1(NULL, "&(((service_stats_info*)(svc_info->ss_mon_info))->key_svc_error_code)= %p", 
                   &(((service_stats_info*)(svc_info->ss_mon_info))->key_svc_error_code));
    create_csv_files(svc_info);
  }
  int norm_svc_error_code_id; 
  int flag_new_svc_error_code;
  char *strStatus = "SUCCESS";
  norm_svc_error_code_id = nslb_get_or_gen_norm_id(&(((service_stats_info*)(svc_info->ss_mon_info))->key_svc_error_code), strStatus,
                                                      strlen(strStatus), &flag_new_svc_error_code);
  LPSDL1(NULL, "flag_new_svc_error_code = %d", flag_new_svc_error_code);
  if(flag_new_svc_error_code){
     /* it is meta data so we always dump it time*/
    svc_process_svc_error_code(svc_info, strStatus, norm_svc_error_code_id, 1);
  }

  SVC_MALLOC(svc_info->svcRecordBuf, SVC_RECORD_MAX_BUFFER_SIZE, "log buffer", svc_info);
  SVC_MALLOC(svc_info->svcCompRecordBuf, SVC_RECORD_MAX_BUFFER_SIZE, "log buffer", svc_info);

  svc_info->allocated_num_svc_sessions = INIT_NUM_PARALLEL_SESSIONS_PER_THREAD;

  if(threadIndex == 0)
    svc_process_svc_signature_table_data(svc_info, "NA", -1, 1);

}

//For add the other service for all instances
static void svc_add_default_service(service_stats_info *ss_mon_info)
{
  int i;

  LPSDL1(NULL,  "Methos Called");
  LPSDL1(NULL,  "ss_mon_info->total_service = %d", ss_mon_info->total_service);
#ifdef BUILD_SVC_ALONE
  int threadIndex = -1;
#endif
  for(i = 0; i < ss_mon_info->total_svc_info; i++)
  {
    SVC_REALLOC(ss_mon_info->svc_info[i].svc_table, ss_mon_info->svc_info[i].svc_table, 
                 (ss_mon_info->total_service + 1) * sizeof(service_table), "svc table", ss_mon_info);
    memset(&ss_mon_info->svc_info[i].svc_table[ss_mon_info->total_service], 0, sizeof(service_table));

    SVC_MALLOC(ss_mon_info->svc_info[i].svc_table[ss_mon_info->total_service].name, 
               strlen("Other") + 1, "svc_name", ss_mon_info);
    strcpy(ss_mon_info->svc_info[i].svc_table[ss_mon_info->total_service].name, "Other");
    ss_mon_info->svc_info[i].svc_table[ss_mon_info->total_service].namelen = 5 /*strlen("Other")*/;  

    SVC_MALLOC(ss_mon_info->svc_info[i].svc_table[ss_mon_info->total_service].disp_name, 
                strlen("Other") + 1, "svc_name", ss_mon_info);
    strcpy(ss_mon_info->svc_info[i].svc_table[ss_mon_info->total_service].disp_name, "Other");
    ss_mon_info->svc_info[i].svc_table[ss_mon_info->total_service].numComp = 1;
 

    ss_mon_info->svc_info[i].svc_table[ss_mon_info->total_service].numComp = 1;
    SVC_MALLOC(ss_mon_info->svc_info[i].svc_table[ss_mon_info->total_service].comp_table, 
                ss_mon_info->svc_info[i].svc_table[ss_mon_info->total_service].numComp * sizeof(component_table), 
                "component_table", ss_mon_info);
    memset(ss_mon_info->svc_info[i].svc_table[ss_mon_info->total_service].comp_table, 0, sizeof(component_table));

    SVC_MALLOC(ss_mon_info->svc_info[i].svc_table[ss_mon_info->total_service].comp_table[0].name, 
               strlen("Other") + 1, "svc_name", ss_mon_info);
    strcpy(ss_mon_info->svc_info[i].svc_table[ss_mon_info->total_service].comp_table[0].name, "Other");
    ss_mon_info->svc_info[i].svc_table[ss_mon_info->total_service].comp_table[0].namelen = 5 /*strlen("other")*/;
    
    SVC_MALLOC(ss_mon_info->svc_info[i].svc_table[ss_mon_info->total_service].comp_table[0].disp_name, 
                strlen("Other") + 1, "svc_name", ss_mon_info);
    strcpy(ss_mon_info->svc_info[i].svc_table[ss_mon_info->total_service].comp_table[0].disp_name, "Other");
  }
  ss_mon_info->total_service += 1;
}

static void copy_svc_entry(service_table *src, service_table *dest)
{
  int i;
  //Copy service name and disp_name
  dest->name = strdup(src->name);
  dest->disp_name = strdup(src->disp_name);
  dest->namelen = src->namelen; //For First instance svc.conf is passed and for other its added here
#ifdef BUILD_SVC_ALONE
  int threadIndex = -1;
#endif
  
  //Copy component.
  SVC_MALLOC(dest->comp_table, src->numComp * sizeof(component_table), "Component_table", NULL);
  memset(dest->comp_table, 0, src->numComp * sizeof(component_table));

  //Copy component name and disp name
  for(i = 0; i < src->numComp; i++)
  {
    SVC_MALLOC(dest->comp_table[i].name, strlen(src->comp_table[i].name) + 1, "svc_name", NULL);
    strcpy(dest->comp_table[i].name, src->comp_table[i].name);
    dest->comp_table[i].namelen = src->comp_table[i].namelen;
 
    SVC_MALLOC(dest->comp_table[i].disp_name, strlen(src->comp_table[i].disp_name) + 1, "svc_name", NULL);
    strcpy(dest->comp_table[i].disp_name, src->comp_table[i].disp_name);
  }
  dest->numComp = src->numComp;
}

#define MAX_NUM_COMPONENT 256
static int svc_add_service(service_stats_info *ss_mon_info, char *line)
{
  char *tmp;
  char *svc_name = NULL;
  char *svc_display_name = NULL;
  char *comp_str = NULL;
  int comp_count = 0;
  char *fields[MAX_NUM_COMPONENT];
  int i = 0;
  char *comp_display_name = NULL;
  char *comp_name = NULL;
#ifdef BUILD_SVC_ALONE
  int threadIndex = -1;
#endif
  char *ptr;
  if((ptr = strchr(line, '\n')))
    *ptr = '\0';
  
  CLEAR_WHITE_SPACE(line);
 
  if((tmp = strchr(line, ' ')) != NULL){
    *tmp = 0;
    comp_str = tmp + 1;
    CLEAR_WHITE_SPACE(comp_str);
  }

  svc_display_name = svc_name = line;

  if((tmp = strchr(line, ',')) != NULL) {
    *tmp = 0;
    svc_name = tmp + 1;
  }

  LPSDL1(NULL,  "Methos Called, ss_mon_info->total_svc_info = %d", ss_mon_info->total_svc_info);
  for(i = 0; i < ss_mon_info->total_svc_info; i++)
  {
    SVC_REALLOC(ss_mon_info->svc_info[i].svc_table, ss_mon_info->svc_info[i].svc_table, 
                 (ss_mon_info->total_service + 1) * sizeof(service_table), "svc table", ss_mon_info);
    memset(&ss_mon_info->svc_info[i].svc_table[ss_mon_info->total_service], 0, sizeof(service_table));
  }

  SVC_MALLOC(ss_mon_info->svc_info[0].svc_table[ss_mon_info->total_service].name, strlen(svc_name) + 1, "svc_name", ss_mon_info);
  strcpy(ss_mon_info->svc_info[0].svc_table[ss_mon_info->total_service].name, svc_name);
  ss_mon_info->svc_info[0].svc_table[ss_mon_info->total_service].namelen = strlen(svc_name);

  SVC_MALLOC(ss_mon_info->svc_info[0].svc_table[ss_mon_info->total_service].disp_name, 
             strlen(svc_display_name) + 1, "svc_name", ss_mon_info);
  strcpy(ss_mon_info->svc_info[0].svc_table[ss_mon_info->total_service].disp_name, svc_display_name);

  i = 0;
  //Now check for component.
  if(comp_str && *comp_str)
  {
    LPSDL1(NULL,  "comp_str = %s", comp_str);
    comp_count = get_tokens_with_multi_delimiter(comp_str, fields, ";\n", 256); 

    //If comp_count is 0 then melloc 1 for other
    SVC_MALLOC(ss_mon_info->svc_info[0].svc_table[ss_mon_info->total_service].comp_table, 
                (comp_count + 1) * sizeof(component_table) , "component_table", ss_mon_info);
    memset(ss_mon_info->svc_info[0].svc_table[ss_mon_info->total_service].comp_table, 0, (comp_count + 1)* sizeof(component_table));

    LPSDL1(NULL,  "comp_count = %d", comp_count);

    for(i = 0; i < comp_count; i++)
    {
      comp_display_name = comp_name = fields[i];
      if((tmp = strchr(fields[i], ',')) != NULL){ 
        *tmp = 0;
        comp_name = tmp + 1;
      }
      
      SVC_MALLOC(ss_mon_info->svc_info[0].svc_table[ss_mon_info->total_service].comp_table[i].name, 
                 strlen(comp_name) + 1, "svc_name", ss_mon_info);
      strcpy(ss_mon_info->svc_info[0].svc_table[ss_mon_info->total_service].comp_table[i].name, comp_name);
      ss_mon_info->svc_info[0].svc_table[ss_mon_info->total_service].comp_table[i].namelen = strlen(comp_name);
  
      SVC_MALLOC(ss_mon_info->svc_info[0].svc_table[ss_mon_info->total_service].comp_table[i].disp_name, 
                  strlen(comp_display_name) + 1, "svc_name", ss_mon_info);
      strcpy(ss_mon_info->svc_info[0].svc_table[ss_mon_info->total_service].comp_table[i].disp_name, comp_display_name);
    }
  }

  //at then end add default component.
  if(comp_count == 0)
  {
    SVC_MALLOC(ss_mon_info->svc_info[0].svc_table[ss_mon_info->total_service].comp_table, 
                (comp_count + 1)* sizeof(component_table) , "component_table", ss_mon_info);
    memset(ss_mon_info->svc_info[0].svc_table[ss_mon_info->total_service].comp_table, 0, (comp_count + 1)* sizeof(component_table));
  }

  SVC_MALLOC(ss_mon_info->svc_info[0].svc_table[ss_mon_info->total_service].comp_table[i].name, 
               strlen("Other") + 1, "svc_name", ss_mon_info);
  strcpy(ss_mon_info->svc_info[0].svc_table[ss_mon_info->total_service].comp_table[i].name, "Other");
  ss_mon_info->svc_info[0].svc_table[ss_mon_info->total_service].comp_table[i].namelen = 5 /*strlen("Other") */;
  
  SVC_MALLOC(ss_mon_info->svc_info[0].svc_table[ss_mon_info->total_service].comp_table[i].disp_name, 
              strlen("Other") + 1, "svc_name", ss_mon_info);
  strcpy(ss_mon_info->svc_info[0].svc_table[ss_mon_info->total_service].comp_table[i].disp_name, "Other");

  ss_mon_info->svc_info[0].svc_table[ss_mon_info->total_service].numComp = comp_count + 1;
  LPSDL1(NULL,  "ss_mon_info->svc_info[0].svc_table[ss_mon_info->total_service].numComp = %d", 
                 ss_mon_info->svc_info[0].svc_table[ss_mon_info->total_service].numComp);
  
  //Now copy to other instances.
  for(i = 1; i < ss_mon_info->total_svc_info; i++)
    copy_svc_entry(&(ss_mon_info->svc_info[0].svc_table[ss_mon_info->total_service]), &(ss_mon_info->svc_info[i].svc_table[ss_mon_info->total_service]));    

  ss_mon_info->total_service += 1;
  LPSDL1(NULL,  "ss_mon_info->total_service = %d", ss_mon_info->total_service);
  return 0;
}
/* parse_svc_conf()
 * pase the svc.conf file and and copy into TR directory
 */
static inline int parse_svc_conf(service_stats_info *ss_mon_info)
{
  char buf[16 * 1024] = "";
  char cmd_out[1024] = "";
  char *filename = buf;
  char *ptr = buf;
  FILE *fp;
  char *field[8];
  int num_field;
  char f_field_seperator = 0;
  char f_component_session_id_field = 0;
  char file_name[1024] =  "";
  int ret;
#ifdef BUILD_SVC_ALONE
  int threadIndex = -1;
#endif

  keyword_value_t *svc_keywords = ss_mon_info->svc_keywords;

  SVC_DL2(ss_mon_info, -1, "Method Called, ns_wdir = %s, testidx = %d, conf_file = %s", 
               ss_mon_info->svc_ns_wdir, ss_mon_info->svc_testidx, ss_mon_info->conf_file);

  if(ss_mon_info->conf_file[0] == '/')
    sprintf(file_name, "%s", ss_mon_info->conf_file);
  else
    sprintf(file_name, "%s/sys/%s", ss_mon_info->svc_ns_wdir, ss_mon_info->conf_file); 

  SVC_DL2(ss_mon_info, -1, "file_name = '%s'", file_name);
  
 
  //copy Svc.conf in TR
  if(ss_mon_info->vector_flag) {
    sprintf(ptr, "mkdir -p %s/logs/TR%d/svc/conf/; cp %s %s/logs/TR%d/svc/conf/; chown -R %s:%s %s/logs/TR%d/svc/",
                     ss_mon_info->svc_ns_wdir, ss_mon_info->svc_testidx, file_name, ss_mon_info->svc_ns_wdir,  
                     ss_mon_info->svc_testidx, 
                     ss_mon_info->ownername, ss_mon_info->grpname, ss_mon_info->svc_ns_wdir, ss_mon_info->svc_testidx);
    ret = nslb_run_cmd_and_get_last_line(ptr, 1024, cmd_out);
 
    if(ret) {
      SVCEL("Fail: Error in run Command, cmd = '%s'" , ptr);
      return -1;
    }
  }
  memset(svc_keywords, 0, sizeof(keyword_value_t) * TOTAL_SVC_KEYWORD);
  
  sprintf(filename, "%s/sys/svc.conf", ss_mon_info->svc_ns_wdir);
  fp = my_fopen(filename, "r");

  if(!fp)
  {
    SVCEL("Error: error in openning file '%s'. Error : '%s'", filename, strerror(errno));
    return -1;
  }

  while(fgets(ptr, 16 * 1024, fp))
  {
    while(*ptr == ' ' || *ptr == '\t' )
      ptr++;

    if(*ptr == '#' || *ptr == '\n')
      continue;

    if(!strncmp(buf , "SERVICE_INFO", 12))
    {
      svc_add_service(ss_mon_info, buf + 12);
      continue;
    }
    num_field = get_tokens_with_multi_delimiter(buf, field, "|\n", 8);    
    if(num_field < 2)
      continue;

      
    if(!strcmp(field[0], "FIELD_SEPERATOR")) {
      if(*(field[1] + 1) != '\0') {
        SVCEL("Error: FIELD_SEPERATOR should have only one charcter, but define '%s' in svc.conf", field[1]);
        return -1;
      }
      if(f_field_seperator) {
        SVCEL("Error: FIELD_SEPERATOR is define more once in svc.conf");
        return -1;
      }
      ss_mon_info->field_seperator = *field[1]; 
      f_field_seperator = 1;
    }
    else if(!strcmp(field[0], "SERVICE_EXIT_SEARCH_PATTERN")) {
      keyword_value_t *service_exit_pttern = &svc_keywords[SERVICE_EXIT_SEARCH_PATTERN];

      service_exit_pttern->count += 1;
      /* after finding the keyword realloc array size and 
       * alloc for new pattern */
      SVC_REALLOC(service_exit_pttern->value, service_exit_pttern->value, 
                  service_exit_pttern->count * sizeof (char **), "count", ss_mon_info);
      SVC_MALLOC(service_exit_pttern->value[service_exit_pttern->count -1], 
                 strlen(field[1]) + 1, "SERVICE_EXIT_SEARCH_PATTERN", ss_mon_info); 
      strcpy(service_exit_pttern->value[service_exit_pttern->count -1], field[1]);
    }
    else if(!strcmp(field[0], "SERIVCE_STATUS")) {
      keyword_value_t *service_status = &svc_keywords[SERIVCE_STATUS];

      service_status->count += 1;
      SVC_REALLOC(service_status->value, service_status->value, service_status->count * sizeof (char **), "count", ss_mon_info);
      SVC_MALLOC(service_status->value[service_status->count - 1], strlen(field[1]) + 1, "SERIVCE_STATUS", ss_mon_info); 
      strcpy(service_status->value[service_status->count - 1], field[1]);
    }
    else if(!strcmp(field[0], "SERVICE_ELAPSED_TIME")) {
      keyword_value_t *service_elapsed_time = &svc_keywords[SERVICE_ELAPSED_TIME];

      service_elapsed_time->count += 1;
      SVC_REALLOC(service_elapsed_time->value, service_elapsed_time->value, 
                  service_elapsed_time->count * sizeof (char **), "count", ss_mon_info);
      SVC_MALLOC(service_elapsed_time->value[service_elapsed_time->count - 1], 
                  strlen(field[1]) + 1, "SERVICE_ELAPSED_TIME", ss_mon_info); 
      strcpy(service_elapsed_time->value[service_elapsed_time->count - 1], field[1]);
    }
    else if(!strcmp(field[0], "COMPONENT_SOR_RESPONSE_TIME_SEARCH_PATTERN")) {
      keyword_value_t *component_sor_response_time_search_pattern = &svc_keywords[COMPONENT_SOR_RESPONSE_TIME_SEARCH_PATTERN];

      component_sor_response_time_search_pattern->count += 1;
      SVC_REALLOC(component_sor_response_time_search_pattern->value, 
                   component_sor_response_time_search_pattern->value, 
                   component_sor_response_time_search_pattern->count * sizeof (char **), "count", ss_mon_info);
      SVC_MALLOC(component_sor_response_time_search_pattern->value[component_sor_response_time_search_pattern->count - 1], 
                  strlen(field[1]) + 1, "COMPONENT_SOR_RESPONSE_TIME_SEARCH_PATTERN", ss_mon_info); 
      strcpy(component_sor_response_time_search_pattern->value[component_sor_response_time_search_pattern->count - 1], field[1]);
    }
    else if(!strcmp(field[0], "COMPONENT_SOR_RESPONSE_TIME")) {
      keyword_value_t *component_sor_response_time = &svc_keywords[COMPONENT_SOR_RESPONSE_TIME];

      component_sor_response_time->count += 1;
      SVC_REALLOC(component_sor_response_time->value, component_sor_response_time->value, 
                   component_sor_response_time->count * sizeof (char **), "count", ss_mon_info);
      SVC_MALLOC(component_sor_response_time->value[component_sor_response_time->count - 1], 
                   strlen(field[1]) + 1, "COMPONENT_SOR_RESPONSE_TIME", ss_mon_info); 
      strcpy(component_sor_response_time->value[component_sor_response_time->count - 1], field[1]);
    }
    else if(!strcmp(field[0], "COMPONENT_RESPONSE_TIME")) {
      keyword_value_t *component_response_time = &svc_keywords[COMPONENT_RESPONSE_TIME];

      component_response_time->count += 1;
      SVC_REALLOC(component_response_time->value, component_response_time->value, 
                  component_response_time->count * sizeof (char **), "count", ss_mon_info);
      SVC_MALLOC(component_response_time->value[component_response_time->count - 1], 
                   strlen(field[1]) + 1, "COMPONENT_RESPONSE_TIME", ss_mon_info); 
      strcpy(component_response_time->value[component_response_time->count - 1], field[1]);
    }
    else if(!strcmp(field[0], "COMPONENT_QUEUE_WAIT_TIME")) {
      keyword_value_t *component_queue_wait_time = &svc_keywords[COMPONENT_QUEUE_WAIT_TIME];

      component_queue_wait_time->count += 1;
      SVC_REALLOC(component_queue_wait_time->value, component_queue_wait_time->value, 
                  component_queue_wait_time->count * sizeof (char **), "count", ss_mon_info);
      SVC_MALLOC(component_queue_wait_time->value[component_queue_wait_time->count - 1],  
                  strlen(field[1]) + 1, "COMPONENT_QUEUE_WAIT_TIME", ss_mon_info); 
      strcpy(component_queue_wait_time->value[component_queue_wait_time->count - 1], field[1]);
    }
    else if(!strcmp(field[0], "COMPONENT_TOTAL_RESPONSE_TIME")) {
      keyword_value_t *component_total_response_time = &svc_keywords[COMPONENT_TOTAL_RESPONSE_TIME];

      component_total_response_time->count += 1;
      SVC_REALLOC(component_total_response_time->value, component_total_response_time->value, 
                   component_total_response_time->count * sizeof (char **), "count", ss_mon_info);
      SVC_MALLOC(component_total_response_time->value[component_total_response_time->count - 1], 
                   strlen(field[1]) + 1, "COMPONENT_TOTAL_RESPONSE_TIME", ss_mon_info); 
      strcpy(component_total_response_time->value[component_total_response_time->count - 1], field[1]);
    }
    else if(!strcmp(field[0], "COMPONENT_STATUS")) {
      keyword_value_t *component_status = &svc_keywords[COMPONENT_STATUS];

      component_status->count += 1;
      SVC_REALLOC(component_status->value, component_status->value, component_status->count * sizeof (char **), "count", ss_mon_info);
      SVC_MALLOC(component_status->value[component_status->count - 1], strlen(field[1]) + 1, "COMPONENT_STATUS", ss_mon_info); 
      strcpy(component_status->value[component_status->count - 1], field[1]);
    }
    else if(!strcmp(field[0], "COMPONENT_SESSION_ID_FIELD")) {
      if(!ns_is_numeric(field[1])) {
        SVCEL("Error: value of keyword 'COMPONENT_SESSION_ID_FIELD', '%s' is not  numeric in svc.conf", field[1]);
        return -1;
      }
      if(f_component_session_id_field) {
        SVCEL("Error: COMPONENT_SESSION_ID_FIELD is define more once in svc.conf");
        return -1;
      }
     
      ss_mon_info->component_session_id_field = atoi(field[1]) - 1;

      if(ss_mon_info->component_session_id_field < 0) {
        SVCEL("Error: Invalid value of COMPONENT_SESSION_ID_FIELD defined in svc.conf file. This value must be a positive integer.");
        return -1;
      }
    }

    ptr = buf;
  }
  fclose(fp);

  //adding other service 
  svc_add_default_service(ss_mon_info);

  // set default values if not given svc.conf
  if(!svc_keywords[SERVICE_EXIT_SEARCH_PATTERN].value) {
    keyword_value_t *service_exit_pttern = &svc_keywords[SERVICE_EXIT_SEARCH_PATTERN];

    service_exit_pttern->count += 2;
    /* after finding the keyword realloc array size and 
     * alloc for new pattern */
    SVC_REALLOC(service_exit_pttern->value, service_exit_pttern->value, 
                 service_exit_pttern->count * sizeof (char **), "count", ss_mon_info);
    SVC_MALLOC(service_exit_pttern->value[0], strlen("com.wellsfargo.mws.distribution.MessageDispatcher") + 1, 
                 "SERVICE_EXIT_SEARCH_PATTERN", ss_mon_info); 
    strcpy(service_exit_pttern->value[0], "com.wellsfargo.mws.distribution.MessageDispatcher");
    SVC_MALLOC(service_exit_pttern->value[1], strlen("com.wellsfargo.module.handlers.ExitMessageHandler") + 1, 
                 "SERVICE_EXIT_SEARCH_PATTERN", ss_mon_info); 
    strcpy(service_exit_pttern->value[1], "com.wellsfargo.module.handlers.ExitMessageHandler");
  }
  if(!svc_keywords[SERIVCE_STATUS].value) {
    keyword_value_t *service_status = &svc_keywords[SERIVCE_STATUS];

    service_status->count += 1;
    SVC_REALLOC(service_status->value, service_status->value, service_status->count * sizeof (char **), "count", ss_mon_info);
    SVC_MALLOC(service_status->value[service_status->count - 1], 
                strlen("Exit message STATUS:") + 1, "SERIVCE_STATUS", ss_mon_info); 
    strcpy(service_status->value[service_status->count - 1],"Exit message STATUS:" );
  }
  if(!svc_keywords[SERVICE_ELAPSED_TIME].value) {
    keyword_value_t *service_elapsed_time = &svc_keywords[SERVICE_ELAPSED_TIME];

    service_elapsed_time->count += 1;
    SVC_REALLOC(service_elapsed_time->value, service_elapsed_time->value, 
                service_elapsed_time->count * sizeof (char **), "count", ss_mon_info);
    SVC_MALLOC(service_elapsed_time->value[service_elapsed_time->count - 1], 
                strlen(" elapsed (ms):") + 1, "SERVICE_ELAPSED_TIME", ss_mon_info); 
    strcpy(service_elapsed_time->value[service_elapsed_time->count - 1], " elapsed (ms):");
  }

  if(!svc_keywords[COMPONENT_SOR_RESPONSE_TIME_SEARCH_PATTERN].value) {
    keyword_value_t *component_sor_response_time_search_pattern = &svc_keywords[COMPONENT_SOR_RESPONSE_TIME_SEARCH_PATTERN];

    component_sor_response_time_search_pattern->count += 1;
    SVC_REALLOC(component_sor_response_time_search_pattern->value, 
                 component_sor_response_time_search_pattern->value, 
                 component_sor_response_time_search_pattern->count * sizeof (char **), "count", ss_mon_info);
    SVC_MALLOC(component_sor_response_time_search_pattern->value[component_sor_response_time_search_pattern->count - 1], 
                strlen("com.wellsfargo.mws.servant.framework.sorwrapper.SORLogHelper") + 1, 
                "COMPONENT_SOR_RESPONSE_TIME_SEARCH_PATTERN", ss_mon_info); 
    strcpy(component_sor_response_time_search_pattern->value[component_sor_response_time_search_pattern->count - 1], 
            "com.wellsfargo.mws.servant.framework.sorwrapper.SORLogHelper");
  }

  if(!svc_keywords[COMPONENT_SOR_RESPONSE_TIME].value) {
    keyword_value_t *component_sor_response_time = &svc_keywords[COMPONENT_SOR_RESPONSE_TIME];

    component_sor_response_time->count += 1;
    SVC_REALLOC(component_sor_response_time->value, component_sor_response_time->value, 
                 component_sor_response_time->count * sizeof (char **), "count", ss_mon_info);
    SVC_MALLOC(component_sor_response_time->value[component_sor_response_time->count - 1], 
                 strlen(", SOR response time (ms):") + 1, "COMPONENT_SOR_RESPONSE_TIME", ss_mon_info); 
    strcpy(component_sor_response_time->value[component_sor_response_time->count - 1], ", SOR response time (ms):");
  }
  if(!svc_keywords[COMPONENT_RESPONSE_TIME].value) {
    keyword_value_t *component_response_time = &svc_keywords[COMPONENT_RESPONSE_TIME];

    component_response_time->count += 1;
    SVC_REALLOC(component_response_time->value, component_response_time->value, 
                 component_response_time->count * sizeof (char **), "count", ss_mon_info);
    SVC_MALLOC(component_response_time->value[component_response_time->count - 1], 
                 strlen(", response time (ms):") + 1, "COMPONENT_RESPONSE_TIME", ss_mon_info); 
    strcpy(component_response_time->value[component_response_time->count - 1], ", response time (ms):");
  }
  if(!svc_keywords[COMPONENT_QUEUE_WAIT_TIME].value) {
    keyword_value_t *component_queue_wait_time = &svc_keywords[COMPONENT_QUEUE_WAIT_TIME];

    component_queue_wait_time->count += 1;
    SVC_REALLOC(component_queue_wait_time->value, component_queue_wait_time->value, 
                 component_queue_wait_time->count * sizeof (char **), "count", ss_mon_info);
    SVC_MALLOC(component_queue_wait_time->value[component_queue_wait_time->count - 1],  
                strlen(", QUEUE wait time (ms):") + 1, "COMPONENT_QUEUE_WAIT_TIME", ss_mon_info); 
    strcpy(component_queue_wait_time->value[component_queue_wait_time->count - 1], ", QUEUE wait time (ms):");
  }
  if(!svc_keywords[COMPONENT_TOTAL_RESPONSE_TIME].value) {
    keyword_value_t *component_total_response_time = &svc_keywords[COMPONENT_TOTAL_RESPONSE_TIME];

    component_total_response_time->count += 1;
    SVC_REALLOC(component_total_response_time->value, component_total_response_time->value, 
                 component_total_response_time->count * sizeof (char **), "count", ss_mon_info);
    SVC_MALLOC(component_total_response_time->value[component_total_response_time->count - 1], 
                 strlen(", Total response time (ms):") + 1, "COMPONENT_TOTAL_RESPONSE_TIME", ss_mon_info); 
    strcpy(component_total_response_time->value[component_total_response_time->count - 1], ", Total response time (ms):");
  }
  if(!svc_keywords[COMPONENT_STATUS].value) {
    keyword_value_t *component_status = &svc_keywords[COMPONENT_STATUS];

    component_status->count += 1;
    SVC_REALLOC(component_status->value, component_status->value, component_status->count * sizeof (char **), "count", ss_mon_info);
    SVC_MALLOC(component_status->value[component_status->count - 1], strlen("STATUS:") + 1, "COMPONENT_STATUS", ss_mon_info); 
    strcpy(component_status->value[component_status->count - 1], "STATUS:");
  }

  /*int i, j;
  for(i = 0; i < TOTAL_SVC_KEYWORD; i++)
  {
    for(j = 0 ; j < svc_keywords[i].count; j++)
    {
      fprintf(stdout, "%s|%d|%s\n", __FUNCTION__, __LINE__, svc_keywords[i].value[j]);
    }
  }*/
  return 0;
}


static u_ns_ts_t base_timestamp = 946684800;   /* approx time to substract from 1/1/70 */

static u_ns_ts_t get_ms_stamp() {
  struct timeval want_time;
  u_ns_ts_t timestamp;


  gettimeofday(&want_time, NULL);


  timestamp = (want_time.tv_sec - base_timestamp)*1000 + (want_time.tv_usec / 1000);


  return timestamp;
}

inline void svc_init(service_stats_info *ss_mon_info, char *t_ns_wdir, int t_testidx)
{
  char cmdbuf[2048];
  char cmd_out[1024] = "";
  int ret;
  sprintf(ss_mon_info->svc_ns_wdir, "%s", t_ns_wdir);
  ss_mon_info->svc_testidx = t_testidx;

  SVC_DL2(ss_mon_info, -1, "Method Called, ns_wdir = %s, dbmode = %d, testidx = %d", 
                      t_ns_wdir, ss_mon_info->dbmode, t_testidx);
  
  if(!ss_mon_info->ddr)
    ss_mon_info->dbmode = 0;

  
  SVC_DL2(ss_mon_info, -1, "ss_mon_info->ddr = %d, ss_mon_info->dbmode = %d", ss_mon_info->ddr, ss_mon_info->dbmode); 
  read_owner_group(ss_mon_info);

  if(parse_svc_conf(ss_mon_info) == -1)
    return;

  sprintf(cmdbuf, "%s/logs/TR%d/summary.top | cut -d '|' -f 3", 
                      ss_mon_info->svc_ns_wdir, ss_mon_info->svc_testidx);
  ret = nslb_run_cmd_and_get_last_line(cmdbuf, 1024, cmd_out);
  ss_mon_info->svc_ns_time_stamp =  get_ms_stamp();

  
  nslb_init_norm_id_table(&ss_mon_info->key_svc_name, 16*1024);
  nslb_init_norm_id_table(&ss_mon_info->key_svc_error_code, 16*1024);
  nslb_init_norm_id_table(&ss_mon_info->key_svc_signature, 16*1024);

  if(ss_mon_info->dbmode && !ss_mon_info->vector_flag) 
  {
    sprintf(cmdbuf, "%s/bin/nsu_svc_create_table %d %s > /dev/null 2>&1", 
                      ss_mon_info->svc_ns_wdir, ss_mon_info->svc_testidx, ss_mon_info->svc_ns_wdir);
    ret = nslb_run_cmd_and_get_last_line(cmdbuf, 1024, cmd_out);
    if(ret) {
      SVCEL("ERROR executing command: '%s'\n"
            "setting database mode to 0, data will be written to csv files only\n",
            cmdbuf);

      SVC_DL2(ss_mon_info, -1, "ERROR executing command: '%s', "
                          "setting database mode to 0, data will be written to csv files only",
                          cmdbuf);
    }
  }
  return;
}

int get_all_tokens(char *read_buf, char *fields[], char token, int max_flds)
{
  int totalFlds = 0;
  char *ptr;

  ptr = read_buf;
  fields[totalFlds] = ptr;
  totalFlds++; 
  while(*ptr)
  {
    if(*ptr == token)
    {
      if(ptr == (fields[totalFlds -1]))
      {
        fields[totalFlds -1] = NULL;
      }
      *ptr = '\0';
      fields[totalFlds] = ptr + 1;
      totalFlds++;
    }
    ptr++;
  }
  return(totalFlds);
}
/******************************************************************************
 * subtract_ms_in_formatted_time()
 *
 * This function subtracts provided milliseconds from a provided time.
 * 
 *
 * The time can either be provided as formatted string in first 
 * argument (currently the only format supported is %Y.%m.%d-%H:%M:%S.
 *
 * Alternatively the time can be supplied as struct tm as fourth argument..
 *
 * Either first or fourth arg is required. If provided both, first argument
 * (formatted time string) takes precedence.
 ******************************************************************************/
static inline void subtract_ms_in_formatted_time(int threadIndex, char *input, char *output, int msecs_to_be_subtracted, struct tm *time_ptr)
{
  struct tm time;
  long long int ct;

  if(time_ptr)
    memcpy(&time, time_ptr, sizeof(struct tm));
    //time = *time_ptr;
  if(input)
    strptime(input, "%Y.%m.%d-%H:%M:%S", &time);

  time_t loctime = mktime(&time);
  char buf[512];

  ct = (long long int) loctime;

  int ms = 0;
  char *ptr, *ptr2;
  if(input)
  {
    ptr = strrchr(input, '.');
    if(ptr) /* There are milliseconds in the log */
    {
      ptr++;
      ptr2 = strchr(ptr, '(');
      if(ptr2) /* Timezone text - (PST) */
        *ptr2 = '\0';
      ms = atoi(ptr);
      if(ptr2)
        *ptr2 = '(';
    }
  }

  //convert into ms
  ct *= 1000;
  ct += ms;

  ct -= msecs_to_be_subtracted; /* Milliseconds to be subtracted, passed as argument */

  
  sprintf(output, "%lld", ct);
  return;

  ms = ct % 1000;
  ct /= 1000;

  loctime = (time_t) ct;
  struct tm *tmp;
  struct tm l_tm;
  tmp = localtime(&loctime);
  memcpy(&l_tm, tmp, sizeof(struct tm));
  strftime(buf, sizeof(buf), "%Y.%m.%d-%H:%M:%S", &l_tm);
  sprintf(output, "%s.%d%s", buf, ms, ptr2);
  SVC_DL2(NULL, threadIndex, "output = %s", output);
}


static inline void svc_process_svc_table_data(svc_info_t *svc_info, char *svcName, int svcId, char force_dump)    
{
  char *dbmode = &((service_stats_info*)(svc_info->ss_mon_info))->dbmode;
  if(svc_info->svc_table_fp)
  {
    fprintf(svc_info->svc_table_fp, "%d,%s\n", svcId, svcName);
    fflush(svc_info->svc_table_fp);
  }
  if(*dbmode >= 1)
  {
    char buf[ 16 + strlen(svcName)];
    int len;
    
    len = sprintf(buf, "%d,%s\n", svcId, svcName);
    if(svc_copy_data_in_db(svc_info, svc_info->svc_db_con, svcMetaDataTableName, buf, len) == -1) 
      *dbmode = 0;
  }
}

/**********************************************************************************
 * get_full_line()
 *
 *   This function retrieves the characters from line_begin ptr to line_end ptr
 *   upto a max len chars (including null) , if it finds a \'0' character before
 *   line end it replaces it with the delimiter character. 
 *
 *   It copies the string in buf and returns its pointer.
 *
 *   The buffer buf with len number of bytes must be supplied by the caller.
 ***********************************************************************************/

static char *get_full_line(char *line_begin, char *line_end, char delimiter, char *buf, int len)
{
  int i;

  if(!line_begin || !line_end || !buf || len==0 || line_begin[0]=='\0')
    return buf; 

  if(line_end <= line_begin)
  {
    buf[0] = '\0';
    return buf;
  }

  if(line_end - line_begin + 1 < len)
    len = line_end - line_begin + 1;

  buf[len - 1] = '\0';

  for(i = 0; i < len - 2; i++)
  {
    buf[i] = line_begin[i];
    if(!buf[i])
      buf[i] = delimiter; 
  }
  return buf;
}

/**********************************************************************
 * compare_starttime()
 * 
 * Synopsis
 *     Comparison method for qsort on the basis of start time 
 * 
 **********************************************************************/
static int compare_starttime(const void *p1, const void *p2)
{
  int diff = (int) (((svc_instance_t *)p1)->svcstarttime - ((svc_instance_t *)p2)->svcstarttime);

  //if(((svc_instance_t *)p1)->svcstarttime != ((svc_instance_t *)p2)->svcstarttime)
    //return (int) (((svc_instance_t *)p1)->svcstarttime - ((svc_instance_t *)p2)->svcstarttime);

  //if(diff > 25 || diff < -25)
    return diff;

  //return (int) (strcmp(((svc_instance_t *)p1)->svcname, ((svc_instance_t *)p2)->svcname));
}

/**********************************************************************
 * compare_alpha()
 * 
 * Synopsis
 *     Comparison method for qsort on the basis of start time 
 * 
 **********************************************************************/
static int compare_alpha (const void *p1, const void *p2)
{
  return (int) (strcmp(((svc_instance_t *)p1)->svcname, ((svc_instance_t *)p2)->svcname));
}

/**********************************************************************
 * sort_session_components_on_start_time()
 * 
 * Synopsis
 *     Sorts the array on the basis of start time
 * 
 **********************************************************************/

static inline void sort_session_components_on_start_time(sess_map_table_t *sess_map_table)
{
  int n = sess_map_table->num_components;

  if (n <= 1)
    return;

  qsort( (void * )(&(sess_map_table->component_data[1])), n - 1, sizeof(svc_instance_t), compare_starttime);
}
 
/***************************************************************************** 
 * compute_signature()
 *
 *      This function 
 *        a) computes the signature string, 
 *        b) finds the signature ID using normalised function and
 *        c) dumps the signature meta data to svcSignatureTable_xxxx
 *
 *****************************************************************************/
static int compute_signature(sess_map_table_t *sess_map_table, svc_info_t *svc_info)
{
  int n = sess_map_table->num_components, i;

  if (n <= 1)
    return -1;

  char svc_signature_str[(130 * n) + 1];
  char tmp_svc_crit_signature_str[(130 * n) + 1];
  char *svc_crit_signature_str = tmp_svc_crit_signature_str;
  svc_signature_str[0] = '\0';
  tmp_svc_crit_signature_str[0] = '\0';
  int len;

  for(i = 1; i < n; i++)
  {
    sprintf(svc_signature_str, "%s%s%s", svc_signature_str, (i <= n - 1 && i > 1)?"&#44;":"", sess_map_table->component_data[i].svcname);
    if(sess_map_table->component_data[i].svccritpathflag == 1 && n > 1){
      len = sprintf(svc_crit_signature_str, "%s&#44;%s", 
                                      svc_crit_signature_str, 
                                      sess_map_table->component_data[i].svcname);
    }
  }

  int flag_new_svc_signature = 0;

  int normalised_svc_signature_index = nslb_get_or_gen_norm_id(&((service_stats_info*)(svc_info->ss_mon_info))->key_svc_signature, 
                                             svc_signature_str,
                                             strlen(svc_signature_str), 
                                             &flag_new_svc_signature);

  if(flag_new_svc_signature){
     /* It is meta data so we always flush the file stream */
    svc_process_svc_signature_table_data(svc_info, svc_signature_str, normalised_svc_signature_index, 1);
  }
 
  if(tmp_svc_crit_signature_str[0] != '\0') 
  {
    if(!strncmp(svc_crit_signature_str, "&#44;", 5))
    {
      svc_crit_signature_str +=5;
    }

    if(len > 5 && !strncmp(tmp_svc_crit_signature_str + len - 5, "&#44;", 5))
    {
      *(tmp_svc_crit_signature_str + len - 5) = '\0';
    }

    flag_new_svc_signature = 0;
    int normalised_svc_cp_signature_index = nslb_get_or_gen_norm_id(&((service_stats_info*)(svc_info->ss_mon_info))->key_svc_signature,
                                               svc_crit_signature_str,
                                               strlen(svc_crit_signature_str),
                                               &flag_new_svc_signature);
    if(flag_new_svc_signature){
       /* It is meta data so we always flush the file stream */
      svc_process_svc_signature_table_data(svc_info, svc_crit_signature_str, normalised_svc_cp_signature_index, 1);
    }
    sess_map_table->component_data[0].svccpsignatureindex = normalised_svc_cp_signature_index;
  }
  return normalised_svc_signature_index;
}

/***************************************************************************** 
 * set_signature_in_components ()
 *
 *      This function scans through all the components of the sessIndex and sets the 
 *      signatureID field.
 *
 *****************************************************************************/
static inline void set_signature_in_components(sess_map_table_t *sess_map_table, int signatureID)
{
  int i;

  if(sess_map_table->num_components <= 0) 
    return;

  for(i = 0; i < sess_map_table->num_components; i++)
  {
    sess_map_table->component_data[i].svcsignatureindex = signatureID;
  }
}


static inline void dump_rec_and_comp_rec_buffers(char *buf, FILE *fp, int *p_index, 
                                                 char *tablename, svc_info_t *svc_info)
{
  if(!buf || !fp || !p_index || !tablename || *p_index == 0)
    return;

  char *dbmode = &((service_stats_info*)(svc_info->ss_mon_info))->dbmode;
  SVC_DL2(svc_info, svc_info->threadIndex, "Method called, threadIndex = %d, buf = %p, p_index = %p, tablename = %s", 
                       svc_info->threadIndex, buf, p_index, tablename);

  fprintf(fp, "%s", buf);
  fflush(fp);

  if(*dbmode >= 1)
  {
    if(svc_copy_data_in_db(svc_info, svc_info->svc_db_con, tablename, buf, *p_index) == -1) 
      *dbmode = 0;
  }

  *p_index = 0;
}

/***************************************************************************** 
 * dump_session_to_csv_and_db ()
 *
 *      This function scans through all the components of the sessIndex and 
 *      saves all the entries in csv files and database.
 *
 *****************************************************************************/
static inline void dump_session_to_csv_and_db(sess_map_table_t *sess_map_table, svc_info_t *svc_info)
{
  int i, j, k;
  char *buf;
  FILE *fp;
  int *p_index = NULL, max_buffer_size;
  char tablename[64];

  if(!sess_map_table) /* This is the case when buffer need to be force dumped at svc_close */
  {
    /* Dump the service (main tx) record data buffer */
    buf = svc_info->svcRecordBuf;
    fp = svc_info->svc_record_fp;
    sprintf(tablename, "%s", svcRecordTableName);
    p_index = &svc_info->svcRecordIndex;

    dump_rec_and_comp_rec_buffers(buf, fp, p_index, tablename, svc_info);

    /* Now dump the service components record data buffer */
    buf = svc_info->svcCompRecordBuf;
    fp = svc_info->svc_comp_record_fp;
    sprintf(tablename, "%s", svcCompRecordTableName);
    p_index = &svc_info->svcCompRecordIndex;

    dump_rec_and_comp_rec_buffers(buf, fp, p_index, tablename, svc_info);

    return;
  }
 
  if(sess_map_table->num_components <= 0) 
    return;

  svc_instance_t *pSvcComp = NULL;
  for(i = 0; i < sess_map_table->num_components; i++)
  {
    pSvcComp = &(sess_map_table->component_data[i]);
    if(pSvcComp->svctype == SVC_TYPE_MAIN)
    {
      buf = svc_info->svcRecordBuf;
      fp = svc_info->svc_record_fp;
      p_index = &svc_info->svcRecordIndex;
      max_buffer_size = 0;
      sprintf(tablename, "%s", svcRecordTableName);
      /*#define SVC_RECORD_HEADER "InstanceID,SvcInstance,SvcIndex,SvcSessionIndex,SvcSignatureIndex,SvcStatusIndex,SvcStartTime,SvcRespTime,SvcSORRespTime,SvcQueueWaitTime,SvcAppTime,SvcTotalTime,SvcElapsedTime,SvcNSRelTime"*/
      *p_index += sprintf(buf + *p_index, "%d,%d,%d,%s,%d,%d,%d,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld\n", 
                svc_info->threadIndex, 
                pSvcComp->svcinstance, 
                pSvcComp->svcindex, 
                pSvcComp->svcsessionindex, 
                pSvcComp->svcsignatureindex, 
                pSvcComp->svccpsignatureindex, 
                pSvcComp->svcstatusindex, 
                pSvcComp->svcstarttime, 
                pSvcComp->svcresptime, 
                pSvcComp->svcsorresptime, 
                pSvcComp->svcqueuewaittime, 
                pSvcComp->svcapptime, 
                //pSvcComp->svctotaltime, 
                pSvcComp->svcelapsedtime,
                pSvcComp->svcelapsedtime,
                pSvcComp->svcstarttime - svc_info->time_offset - ((service_stats_info *) svc_info->ss_mon_info)->svc_ns_time_stamp); 

      /* Run the loop for total service -1 
       * -1 for 'other' bucket 
       *  if name not match with any service 
       *  'other' counter will increment*/

      for(j = 0; j < (((service_stats_info *)(svc_info->ss_mon_info))->total_service) - 1; j++)
      {
        if(pSvcComp->svcnamelen == svc_info->svc_table[j].namelen)
          if(!strcmp(pSvcComp->svcname, svc_info->svc_table[j].name))
            break;
      }
      pthread_mutex_lock(&((service_stats_info *)(svc_info->ss_mon_info))->svc_mon_data_mutex);
        svc_info->svc_table[j].cumRequest += 1;
        svc_info->svc_table[j].numRequest += 1;
        svc_info->svc_table[j].cumDBRespTime += pSvcComp->svcsorresptime;
        svc_info->svc_table[j].cumQWTime += pSvcComp->svcqueuewaittime;
        svc_info->svc_table[j].cumAppTime += pSvcComp->svcapptime;
        if(!pSvcComp->svcstatusindex)
          svc_info->svc_table[j].numSuccessReq += 1;
        else
          svc_info->svc_table[j].numFailReq += 1;
      pthread_mutex_unlock(&((service_stats_info *)(svc_info->ss_mon_info))->svc_mon_data_mutex);    
    }
    else
    {
      buf = svc_info->svcCompRecordBuf;
      fp = svc_info->svc_comp_record_fp;
      p_index = &svc_info->svcCompRecordIndex;
      max_buffer_size = SVC_RECORD_MAX_BUFFER_SIZE;
      sprintf(tablename, "%s", svcCompRecordTableName);
      *p_index += sprintf(buf + *p_index, "%d,%d,%d,%d,%d,%s,%d,%d,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%d,%lld\n", 
                svc_info->threadIndex, 
                pSvcComp->svcparentinstance, 
                pSvcComp->svcparentindex, 
                pSvcComp->svcinstance, 
                pSvcComp->svcindex, 
                pSvcComp->svcsessionindex, 
                pSvcComp->svcsignatureindex, 
                pSvcComp->svcstatusindex, 
                pSvcComp->svcstarttime, 
                pSvcComp->svcparentstarttime, 
                pSvcComp->svcresptime, 
                pSvcComp->svcsorresptime, 
                pSvcComp->svctotaltime, 
                pSvcComp->svcqueuewaittime, 
                pSvcComp->svcelapsedtime, 
                pSvcComp->svccritpathflag,
                pSvcComp->svcstarttime - svc_info->time_offset - ((service_stats_info *) svc_info->ss_mon_info)->svc_ns_time_stamp); 

      for(k = 0; k < svc_info->svc_table[j].numComp - 1; k++)
      {
        if(pSvcComp->svcnamelen == svc_info->svc_table[j].comp_table[k].namelen)
          if(!strcmp(pSvcComp->svcname, svc_info->svc_table[j].comp_table[k].name))
            break;
      }
      pthread_mutex_lock(&((service_stats_info *)(svc_info->ss_mon_info))->svc_mon_data_mutex);
        svc_info->svc_table[j].comp_table[k].cumRequest += 1;
        svc_info->svc_table[j].comp_table[k].numRequest += 1;
        svc_info->svc_table[j].comp_table[k].cumDBRespTime += pSvcComp->svcsorresptime;
        svc_info->svc_table[j].comp_table[k].cumQWTime += pSvcComp->svcqueuewaittime;
        svc_info->svc_table[j].comp_table[k].cumAppTime += pSvcComp->svcapptime;
        if(!pSvcComp->svcstatusindex)
          svc_info->svc_table[j].comp_table[k].numSuccessReq += 1;
        else
          svc_info->svc_table[j].comp_table[k].numFailReq += 1;
      pthread_mutex_unlock(&((service_stats_info *)(svc_info->ss_mon_info))->svc_mon_data_mutex);
    }

    if(*p_index > ((max_buffer_size * 8) / 10) - 200 /* Rough max record size*/ )
    {
      dump_rec_and_comp_rec_buffers(buf, fp, p_index, tablename, svc_info);
    }
  }
}

/***************************************************************************** 
 * compute_app_time_and_mark_critical_path ()
 *
 *      This function computes the apptime and marks the component transactions 
 *      that come in critical path
 *
 *****************************************************************************/
static inline void compute_app_time_and_mark_critical_path(sess_map_table_t *sess_map_table)
{
  int n = sess_map_table->num_components, i;
  int max_endtime_comp_index;
  long long int max_endtime = 0, apptime = 0, queuewaittime = 0;
  svc_instance_t *comp_arr = sess_map_table->component_data; 
  int grp_start_index = 1;

  if (n <= 0)
    return;

  /* Start from 1, as 0 is for parent transaction */
  max_endtime = comp_arr[1].svcendtime;
  apptime = comp_arr[1].svcstarttime - comp_arr[0].svcstarttime;
  max_endtime_comp_index = 1;

  for(i = 2; i < n; i++)
  {
    if(comp_arr[i].svcstarttime > max_endtime) /* New component group starts */
    {
      /* Difference will add to the app time */
      apptime += comp_arr[i].svcstarttime - max_endtime;
      comp_arr[max_endtime_comp_index].svccritpathflag = 1;

      /* Now sort the previous group on the alphabetic basis of svcname */
      if(i - 1 > grp_start_index)
      {
        qsort((&(sess_map_table->component_data[grp_start_index])), 
               (i - grp_start_index), 
               sizeof(svc_instance_t), 
               compare_alpha);
      }

      grp_start_index = i;
    }

    if(comp_arr[i].svcendtime >= max_endtime)
    {
      max_endtime_comp_index = i;
      max_endtime = comp_arr[i].svcendtime;
    }
  }

  /* Last max end time component is definitely in critical path */  
  comp_arr[max_endtime_comp_index].svccritpathflag = 1;

  /* Now sort the last group on the alphabetic basis of svcname */
  if(n - 1 > grp_start_index)
  {
    qsort((&(sess_map_table->component_data[grp_start_index])), 
           (n - grp_start_index), 
           sizeof(svc_instance_t), 
           compare_alpha);
  }

  if(comp_arr[max_endtime_comp_index].svcendtime < comp_arr[0].svcstarttime + comp_arr[0].svcelapsedtime)
  {
    /* The end time of the parent txn minus the last max end time is part of app self time */
    apptime += comp_arr[0].svcstarttime + 
               comp_arr[0].svcelapsedtime - 
               comp_arr[max_endtime_comp_index].svcendtime;
  }
  
  for(i = 1; i < n; i++) {
    if(comp_arr[i].svccritpathflag == 1)
    {
      queuewaittime += comp_arr[i].svcqueuewaittime;
    }
  }

  comp_arr[0].svcapptime = apptime;
  comp_arr[0].svcqueuewaittime = queuewaittime;
  comp_arr[0].svcsorresptime = comp_arr[0].svcelapsedtime - comp_arr[0].svcapptime - comp_arr[0].svcqueuewaittime;
}

/***************************************************************************** 
 * free_components ()
 *
 *      This function frees the allocated memory for components belonging to this 
 *      session
 *
 *****************************************************************************/
static inline void free_components(svc_info_t *svc_info, int index, sess_map_table_t *sess_map_table)
{
  if(sess_map_table->component_data)
    SVC_FREE(sess_map_table->component_data, "component_data", svc_info);

  nslb_bufpool_return_slot(svc_info->bufpool_key, index);
  
}

#define INIT_LOCAL_VARIABLES \
    strcpy(strSessionID, ""); \
    startTime = 0; \
    sprintf(strStartTime, "0"); \
    parentStartTime = 0; \
    sprintf(strParentStartTime, "0"); \
    endTime = 0; \
    sprintf(svcName, "NA"); \
    svcType = SVC_TYPE_MAIN; \
    respTime = 0; \
    sorRespTime = 0; \
    qWaitTime = 0; \
    totalRespTime = 0; \
    elapsedTime = 0; \
    sprintf(statusMsg, "NA"); \
    isMainSvc = 0;

int process_svc_data(char *buf, int len, svc_info_t *svc_info)
{
  char *cursor = buf;
  char delimiter = ((service_stats_info *)(svc_info->ss_mon_info))->field_seperator;
  char *line_end = NULL, *line_begin = NULL;
  int threadIndex = svc_info->threadIndex;
  int component_session_id_field = ((service_stats_info *)(svc_info->ss_mon_info))->component_session_id_field;
  
  char *fields[128];
  int num_fields;
  int svc_session_index = -1, svc_comp_index = -1;
 
  char strStartTime[64], strParentStartTime[64], strRespTime[64];
  char strSorRespTime[64], strQWaitTime[64], strTotalRespTime[64], strElapsedTime[64];
  char svcName[MAX_SVC_NAME_LEN], strSessionID[64];
  int svcNameLen = 0;

  int signatureID;
  long long int startTime, endTime, parentStartTime, respTime, sorRespTime, qWaitTime, totalRespTime, elapsedTime;

  char strStatus[MAX_STATUS_STRLEN] = "", statusMsg[512], isMainSvc, svcType;
  int normalised_svcIndex;
  int norm_svc_error_code_id;
  int flag_new_svc_error_code = 0;
  int i;
  keyword_value_t *svc_keywords = ((service_stats_info *)(svc_info->ss_mon_info))->svc_keywords;
 
  INIT_LOCAL_VARIABLES

  static int line = 0;

  SVC_DL2(svc_info, threadIndex, "len = %d\n", len);
  while(1)
  {
    SVC_DL2(svc_info, threadIndex, "svc_session_index = %d", svc_session_index);
    if(*cursor == '\0')
      break;
    line ++;
    memset(fields, 0, sizeof(char *) * 128); 
    
    INIT_LOCAL_VARIABLES

    line_begin = cursor;

    /* First find the end of current line */
    line_end = strchr(cursor, '\n');
    if(!line_end)
      return 0;
    *line_end = '\0';

    /* Ignoring blank lines in raw file */
    if(line_end == cursor)
    { 
      cursor++;
      continue;
    }

    SVC_DL2(svc_info, threadIndex, "line = %d, cursor = %s", line, cursor);
    //SVC_DL2(threadIndex, "%s", buf);
    num_fields = get_all_tokens(cursor, fields, delimiter, 128);
    if(!line_end)
      cursor = buf + len + 1;
    else
      cursor = line_end + 1;

    char *ptr, *ptr2;
    if(num_fields < 14)
    {
      SVC_DL2(svc_info, threadIndex, "Number of fields are less from 14");
      continue;
    }
    SVC_DL2(svc_info, threadIndex, "line = %d", line); 

    /* --- Find sessionId --- */

    /* If second field is NULL, there are chances that this is a main transaction
     * with XML information. If so find the details of main (parent) transaction.
     */
    if(!fields[component_session_id_field])
    {
      ptr = strstr(fields[14], "sessionId>");
      if(!ptr)
      {
        char tmpbuf[1024] = "";
        SVC_DL2(svc_info, threadIndex, "Though second field is blank, but There is no SessionID. "
                        "Hence ignoring this line: '%s'", 
                        get_full_line(line_begin, line_end, delimiter, tmpbuf, 1024));
        continue;
      }

      ptr = ptr + 10; /*strlen("sessionId>")*/
      ptr2 = strchr(ptr, '<');
      *ptr2 = '\0';
      strcpy(strSessionID, ptr);
      //sessionID = atoi(strSessionID);
      

      *ptr2 = '<';
 
      ptr = strstr(fields[14], "Body>");
      if(ptr)
      {
        ptr2 = strchr(ptr, ':');
        ptr2++;
        ptr = svcName;
        while(*ptr2 != ' ')
        {
          *ptr = *ptr2;
          ptr++;
          ptr2++;
        }
        *ptr = '\0';
      }
      isMainSvc = 1;
    }
    else
      strcpy(strSessionID, fields[1]);
      //sessionID = atoi(fields[1]);

    if(isMainSvc)
    {
      subtract_ms_in_formatted_time(svc_info->threadIndex, fields[0], strStartTime, 0, NULL);
      SVC_DL2(svc_info, threadIndex, "strStartTime = '%s'", strStartTime);
      startTime = strtoll(strStartTime, NULL, 10);
      if(!svc_info->first_line_flag)
      {
        svc_info->time_offset = startTime  - get_ms_stamp();
        //svc_info->time_offset = startTime  - get_ms_stamp() - ((service_stats_info *) svc_info->ss_mon_info)->svc_ns_time_stamp;
        svc_info->first_line_flag = 1;
      }
    }
    else if(fields[11])
    {
      char match = 0;
      for(i = 0; i < svc_keywords[COMPONENT_SOR_RESPONSE_TIME_SEARCH_PATTERN].count; i++) {
        if(!strcmp(fields[11], svc_keywords[COMPONENT_SOR_RESPONSE_TIME_SEARCH_PATTERN].value[i])) {
          match = 1;
          break;
        }
      }
      if(!match){
        for(i = 0; i < svc_keywords[SERVICE_EXIT_SEARCH_PATTERN].count; i++) {
          if(!strcmp(fields[11], svc_keywords[SERVICE_EXIT_SEARCH_PATTERN].value[i])) {
            match = 1;
            break;
          }
        }
      }

      if(match) 
      {
        //Find SOR Resp Time
        SVC_DL2(svc_info, threadIndex, "svcType == %d", svcType);
        for(i = 0; i < svc_keywords[COMPONENT_SOR_RESPONSE_TIME].count; i++) {
          if((ptr = strstr(fields[14], svc_keywords[COMPONENT_SOR_RESPONSE_TIME].value[i])))
          {
            //find trans name 
            svcType = SVC_TYPE_COMPONENT;
            char *ptr3; 
            *ptr = '\0';
            ptr2 = strrchr(fields[14], ',');
            if(ptr2)
            {
              ptr3 = strrchr(fields[14], ':');
              if(ptr3)
              {
                if(ptr2 > ptr3)
                  ptr3 = ptr2;
              }
              else 
                ptr3 = ptr2;
              ptr3++;
              if(!strncmp(ptr3, "//MWS/", 6))
                ptr3 += 5;
              strcpy(svcName, ptr3);
            } 
            *ptr = ',';
         
            ptr2 = strSorRespTime;
            ptr = ptr + 26; /*strlen(", SOR response time (ms): ");*/
            while(isdigit(*ptr))
            {
              *ptr2 = *ptr;
              ptr++;
              ptr2++;
            }
            *ptr2 = '\0';
            //sorRespTime = strtoll(strSorRespTime, NULL, 10);
            sorRespTime = atoi(strSorRespTime);
            break;
          }
        }
        if(i >= svc_keywords[COMPONENT_SOR_RESPONSE_TIME].count)
        {
          for(i = 0; i < svc_keywords[SERIVCE_STATUS].count; i++) {
            if((ptr = strstr(fields[14], svc_keywords[SERIVCE_STATUS].value[i])))
            {
              int index;
              sess_map_table_t sess_map_table;
              memset(&sess_map_table, 0, sizeof(sess_map_table_t));
           
              index = nslb_get_svc_index_from_data(svc_info->bufpool_key, strSessionID, &sess_map_table);
           
              if(index < 0)
              {
                SVC_DL2(svc_info, threadIndex, "Found 'Exit message STATUS' text for invalid sessionID = %s",
                                             strSessionID);
                continue;
              }
           
              svc_session_index = index;
           
              if(ptr)
              {
                ptr += 20/* strlen("Exit message STATUS:") */;
           
                ptr2 = strStatus;
                while(*ptr != ' ' && *ptr != ',')
                {
                  *ptr2 = *ptr;
                  ptr++;
                  ptr2++;
                  if(ptr2 - &strStatus[0] >= MAX_STATUS_STRLEN - 1)
                    break;
                }
                *ptr2 = '\0';
              
           
                norm_svc_error_code_id = nslb_get_or_gen_norm_id(&((service_stats_info*)(svc_info->ss_mon_info))->key_svc_error_code, strStatus,
                                                            strlen(strStatus), &flag_new_svc_error_code);
                if(flag_new_svc_error_code){
                   /* it is meta data so we always dump it time*/
                  svc_process_svc_error_code(svc_info, strStatus, norm_svc_error_code_id, 1);
                }
           
                sess_map_table.component_data[0].svcstatusindex = norm_svc_error_code_id;
              }
           
              for(i = 0; i < svc_keywords[SERVICE_ELAPSED_TIME].count; i++) {
                if((ptr = strstr(fields[14], svc_keywords[SERVICE_ELAPSED_TIME].value[i])))
                {
                  /* Copy the elapsed time to parent svc txn */
                  ptr2 = strElapsedTime;
                  ptr = ptr + 15; /*strlen(" elapsed (ms): ");*/
                  while(isdigit(*ptr))
                  {
                    *ptr2 = *ptr;
                    ptr++;
                    ptr2++;
                  }
                  *ptr2 = '\0';
                  elapsedTime = atoi(strElapsedTime);
                  sess_map_table.component_data[0].svcelapsedtime = elapsedTime;
                  break;
                }
              }
              /* Current session is complete. */ 
              SVC_DL1(svc_info, threadIndex, "session is complete; threadIndex = %d, "
                                   "sessionId = %s, "
                                   "parentInstance = %d, "
                                   "parentIndex = %d, "
                                   "parentStartTime = %lld, "
                                   "num_components = %d, "
                                   "num_allocated_components = %d",
                                   threadIndex,
                                   sess_map_table.sessionId,
                                   sess_map_table.parentInstance,
                                   sess_map_table.parentIndex,
                                   sess_map_table.parentStartTime,
                                   sess_map_table.num_components,
                                   sess_map_table.num_allocated_components);
           
              sort_session_components_on_start_time(&sess_map_table); 
              compute_app_time_and_mark_critical_path(&sess_map_table);
              signatureID = compute_signature(&sess_map_table, svc_info);
              set_signature_in_components(&sess_map_table, signatureID);
              dump_session_to_csv_and_db(&sess_map_table, svc_info);
              free_components(svc_info, index, &sess_map_table);
            }
            break;
          }
          continue;
        }
     
        //Find resp Time
        for(i = 0; i < svc_keywords[COMPONENT_RESPONSE_TIME].count; i++){
          if((ptr = strstr(fields[14], svc_keywords[COMPONENT_RESPONSE_TIME].value[i])))
          {
          /************************************************************
           *  Now find Service Name
           ************************************************************/
            char *ptr3;
            *ptr = '\0';
            ptr2 = strrchr(fields[14], ',');
            if(ptr2)
            {
              ptr3 = strrchr(fields[14], '/');
              if(ptr3)
              {
                if(ptr2 > ptr3)
                  ptr3 = ptr2;
              }
              else 
                ptr3 = ptr2;
              ptr3++;
              strcpy(svcName, ptr3);
            }
            *ptr = ',';
         
            ptr2 = strRespTime;
            ptr = ptr + 22; /*strlen(", response time (ms): ");*/
            while(isdigit(*ptr))
            {
              *ptr2 = *ptr;
              ptr++;
              ptr2++;
            }
            *ptr2 = '\0';
            respTime = atoi(strRespTime);
            SVC_DL2(svc_info, threadIndex, "strRespTime = '%s'", strRespTime);
            break;
          }
        }
   
        /************************************************************
         *  Now find Queue Wait Time
         ************************************************************/
        for(i = 0; i < svc_keywords[COMPONENT_QUEUE_WAIT_TIME].count; i++) {
          if((ptr = strstr(fields[14], svc_keywords[COMPONENT_QUEUE_WAIT_TIME].value[i])))
          {
            ptr2 = strQWaitTime;
            ptr = ptr + 24; /*strlen(", QUEUE wait time (ms): ");*/
            while(isdigit(*ptr))
            {
              *ptr2 = *ptr;
              ptr++;
              ptr2++;
            }
            *ptr2 = '\0';
            qWaitTime = atoi(strQWaitTime);
            break;
          }
        }
        
        /************************************************************
         *  Now find Total response time
         ************************************************************/
        for(i = 0; i < svc_keywords[COMPONENT_TOTAL_RESPONSE_TIME].count; i++) {
          if((ptr = strstr(fields[14], svc_keywords[COMPONENT_TOTAL_RESPONSE_TIME].value[i])))
          {
            ptr2 = strTotalRespTime;
            ptr = ptr + 28; /*strlen(", Total response time (ms): ");*/
            while(isdigit(*ptr))
            {
              *ptr2 = *ptr;
              ptr++;
              ptr2++;
            }
            *ptr2 = '\0';
            totalRespTime = atoi(strTotalRespTime);
            break;
          }
        }
        subtract_ms_in_formatted_time(svc_info->threadIndex, fields[0], strStartTime, totalRespTime, NULL);
        startTime = strtoll(strStartTime, NULL, 10);
        endTime = startTime + totalRespTime;
     
        /************************************************************
         *  Now find Elapsed time
         ************************************************************/
        ptr = strstr(fields[14], " elapsed (ms): ");
        if(ptr)
        {
          ptr2 = strElapsedTime;
          ptr = ptr + 15; /*strlen(" elapsed (ms): ");*/
          while(isdigit(*ptr))
          {
            *ptr2 = *ptr;
            ptr++;
            ptr2++;
          }
          *ptr2 = '\0';
          elapsedTime = atoi(strElapsedTime);
        }
   
        /************************************************************
         *  Now find status and status message
         ************************************************************/
        for(i = 0; i < svc_keywords[COMPONENT_STATUS].count; i++) {
          if((ptr = strstr(fields[14], svc_keywords[COMPONENT_STATUS].value[i])))
          {
            strncpy(statusMsg, fields[14], ptr - fields[14]);
            statusMsg[ptr - fields[14]] = '\0';
            ptr2 = statusMsg + (ptr - fields[14] - 1);
            while(*ptr2 == ' ' || *ptr2 == '\t' || *ptr2 == ',')
              ptr2--;
            *(ptr2+1) = '\0';
            ptr2 = strStatus;
            ptr = ptr + 7; /*strlen("STATUS:");*/
            while(*ptr != ' ' && *ptr != ',')
            {
              *ptr2 = *ptr;
              ptr++;
              ptr2++;
              if(ptr2 - &strStatus[0] >= MAX_STATUS_STRLEN - 1)
                break;
            }
            *ptr2 = '\0';
            break  ;
          }
        }
      }
      else 
        continue;
    }

    static int static_instance = -1; /* Shared among threads. Must be updated throgh mutex lock */
    int parentInst;
    int parentIndex;
    int local_instance;

    pthread_mutex_lock(&((service_stats_info *)(svc_info->ss_mon_info))->instance_count_mutex);
      static_instance++;
      local_instance = static_instance;
    pthread_mutex_unlock(&((service_stats_info *)(svc_info->ss_mon_info))->instance_count_mutex);    
 
    sess_map_table_t sess_map_table;
    memset(&sess_map_table, 0, sizeof(sess_map_table_t));
    int index;

    /* Block for Svc Table - BEGIN */
    int flag_new_svc_name = 0;

    //TODO: we can find svcName len, need to optimize it
    svcNameLen = strlen(svcName);
    normalised_svcIndex = nslb_get_or_gen_norm_id(&((service_stats_info*)(svc_info->ss_mon_info))->key_svc_name, 
                                               svcName,svcNameLen,
                                               &flag_new_svc_name);

    if(svcType == SVC_TYPE_MAIN)
    {
      index = nslb_bufpool_get_free_slot(&(svc_info->bufpool_key));
      strcpy(sess_map_table.sessionId, strSessionID);
      sess_map_table.parentInstance = local_instance;
      sess_map_table.parentIndex = normalised_svcIndex;
      sess_map_table.parentStartTime =  startTime;
        
      parentInst = local_instance; 
      parentIndex = normalised_svcIndex;
      parentStartTime =  startTime;
    }
    else
    {
      index = nslb_get_svc_index_from_data(svc_info->bufpool_key, strSessionID, &sess_map_table);
      if(index < 0)
      {
        /* Current Svc is Component transaction and session ID is new */
        SVC_DL2(svc_info, threadIndex, "ParentId not found for session Id '%s'", strSessionID); 
        continue; 
      }

      /* Current Svc is Component transaction and session ID found earlier */
      svc_session_index = index;

      parentInst = sess_map_table.parentInstance;
      parentIndex = sess_map_table.parentIndex;
      parentStartTime = sess_map_table.parentStartTime;
    }

    if(flag_new_svc_name){
       /* It is meta data so we always flush the file stream */
      svc_process_svc_table_data(svc_info, svcName, normalised_svcIndex, 1);
    }
    /* Block for Svc Table - END */

    /* Block for SvcErrorCode*/
    if(strStatus[0] != '\0')
    { 
      //TODO: we can find status len, need to optimize it
      norm_svc_error_code_id = nslb_get_or_gen_norm_id(&((service_stats_info*)(svc_info->ss_mon_info))->key_svc_error_code, strStatus,
                                                    strlen(strStatus), &flag_new_svc_error_code);
      if(flag_new_svc_error_code)
        svc_process_svc_error_code(svc_info, strStatus, norm_svc_error_code_id, 1); /* it is meta data so we always dump it time*/
    }
 
    SVC_DL2(svc_info, threadIndex, "sess_map_table.component_data == %p", sess_map_table.component_data);
    if(!sess_map_table.component_data){
    /* New Session, allocate memory for components */
      SVC_MALLOC(sess_map_table.component_data,
              INIT_NUM_COMPONENT_TRANSACTIONS_PER_SESSION * sizeof(svc_instance_t),
              "component_data", ss_mon_info); 
      memset(sess_map_table.component_data, 0,
             INIT_NUM_COMPONENT_TRANSACTIONS_PER_SESSION * sizeof(svc_instance_t));
      sess_map_table.num_allocated_components = INIT_NUM_COMPONENT_TRANSACTIONS_PER_SESSION;
      sess_map_table.num_components = 1;
      svc_comp_index=0;
    }
    else 
    {
      SVC_DL2(svc_info, threadIndex, "sess_map_table.num_components == %d", sess_map_table.num_components);
      sess_map_table.num_components++;
      svc_comp_index = sess_map_table.num_components - 1;
      SVC_DL2(svc_info, threadIndex, "sess_map_table.num_components == %d", sess_map_table.num_components);

      if (sess_map_table.num_components >= sess_map_table.num_allocated_components){
        SVC_REALLOC(sess_map_table.component_data,
                   sess_map_table.component_data,
                   (sess_map_table.num_allocated_components + DELTA_NUM_COMPONENT_TRANSACTIONS_PER_SESSION) * sizeof(svc_instance_t),
                "component_data", svc_info); 
 
        memset(sess_map_table.component_data + sess_map_table.num_allocated_components, 0,
               DELTA_NUM_COMPONENT_TRANSACTIONS_PER_SESSION * sizeof(svc_instance_t));
        sess_map_table.num_allocated_components += DELTA_NUM_COMPONENT_TRANSACTIONS_PER_SESSION;
      }
    }

    if(nslb_bufpool_set_user_data(svc_info->bufpool_key, index, &sess_map_table) == -1)
    {
      SVC_DL2(svc_info, threadIndex, "Error: nslb_bufpool_set_user_data()");
    }
    svc_instance_t *pSvcComp = &sess_map_table.component_data[svc_comp_index];

    pSvcComp->svcparentinstance = parentInst;
    pSvcComp->svcparentindex = parentIndex;
    pSvcComp->svcinstance = local_instance;
    pSvcComp->svcindex = normalised_svcIndex;

    if(svcNameLen > MAX_SVC_NAME_LEN)
      svcNameLen = MAX_SVC_NAME_LEN;
    strncpy(pSvcComp->svcname, svcName, svcNameLen);
    pSvcComp->svcname[svcNameLen] = '\0';
    pSvcComp->svcnamelen = svcNameLen;

    pSvcComp->svctype = svcType;
    strcpy(pSvcComp->svcsessionindex, strSessionID);
    //pSvcComp->svcsignatureindex;
    pSvcComp->svcstatusindex = norm_svc_error_code_id;
    pSvcComp->svcstarttime = startTime;
    pSvcComp->svcendtime = endTime;
    pSvcComp->svcparentstarttime = parentStartTime;
    pSvcComp->svcresptime = respTime;
    pSvcComp->svcsorresptime = sorRespTime;
    pSvcComp->svctotaltime = totalRespTime;
    pSvcComp->svcqueuewaittime = qWaitTime;
    pSvcComp->svcelapsedtime = elapsedTime;

  }

/***** CLEANUP - begin *****/
  //svc_info[threadIndex].allocated_num_svc_sessions = 0;
  //svc_info[threadIndex].num_svc_sessions = 0;
/***** CLEANUP - end *****/
 
  return 0;
}

inline void svc_close(service_stats_info *ss_mon_info)
{
  int i;
  //For close all monitors data csv file
  for(i = 0; i < ss_mon_info->total_svc_info; i++)
  {  
    /* Dump any undumped in-memory buffers */
    dump_session_to_csv_and_db(NULL, &ss_mon_info->svc_info[i]);
   
    /* Close csv file pointers */ 
    if(ss_mon_info->svc_info[i].svc_record_fp)
    {
      fclose(ss_mon_info->svc_info[i].svc_record_fp);
      ss_mon_info->svc_info[i].svc_record_fp = NULL;
    }

    if(ss_mon_info->svc_info[i].svc_comp_record_fp)
    {
      fclose(ss_mon_info->svc_info[i].svc_comp_record_fp);
      ss_mon_info->svc_info[i].svc_comp_record_fp = NULL;
    }

    if(ss_mon_info->svc_info[i].svc_table_fp)
    {
      fclose(ss_mon_info->svc_info[i].svc_table_fp);
      ss_mon_info->svc_info[i].svc_table_fp = NULL;
    }

    if(ss_mon_info->svc_info[i].svc_signature_table_fp)
    {
      fclose(ss_mon_info->svc_info[i].svc_signature_table_fp);
      ss_mon_info->svc_info[i].svc_signature_table_fp = NULL;
    }

    if(ss_mon_info->svc_info[i].svc_error_code_fp)
    {
      fclose(ss_mon_info->svc_info[i].svc_error_code_fp);
      ss_mon_info->svc_info[i].svc_error_code_fp = NULL;
    }

    /* Destroy the buffer pool key */
    nslb_bufpool_destroy(&ss_mon_info->svc_info[i].bufpool_key);
  }
  nslb_obj_hash_destroy(&ss_mon_info->key_svc_name);
  nslb_obj_hash_destroy(&ss_mon_info->key_svc_error_code);
  nslb_obj_hash_destroy(&ss_mon_info->key_svc_signature);
}

/* This function will take three arguments:
 * Ip address, server port and the message that is to be send.
 */
static int send_message(int fd, char *msg, int len)
{
  /* Here the length of the message is stored in a variable write_bytes_remaining.
   */
  int write_bytes_remaining = len;
  int write_offset = 0;
  int bytes_sent = 0;
  int data = len;
  if(fd == -1)
    return -1;

  LPSDL1(NULL,  "Method Called");
  signal( SIGCHLD, SIG_IGN);
  signal( SIGPIPE, SIG_IGN);
  /* Now the loop will be executed here and will continue till all the bytes are send.
   */
  while(write_bytes_remaining) {

    if(data > write_bytes_remaining)
       data = write_bytes_remaining;

    LPSDL1(NULL,  "fd = %d", fd);
    if ((bytes_sent = send (fd, msg + write_offset, data, 0)) < 0)//Sending the message
    {
      LPSDL1(NULL,  "errno = %d", errno);
      if(errno == EINTR)
        continue;
      else
        LPSEL1(NULL, "\nError: Failed to send message. Error = '%s'\n", strerror(errno));
        return -1; 
    }
    LPSDL1(NULL,  "bytes_sent = %d", bytes_sent);
    write_offset += bytes_sent;
    write_bytes_remaining -= bytes_sent;
  }
  return len;
}
void svc_send_vector_to_ns(service_stats_info *ss_mon_info)
{
  int amt_written = 0;
  char vec_str[8 * 1024] = "";
  int i, j, k;
  LPSDL1(NULL,  "Methos Called");

  signal( SIGCHLD, SIG_IGN);
  LPSDL1(NULL,  "ss_mon_info->total_service = %d", ss_mon_info->total_service);
  for(i = 0; i < ss_mon_info->total_service; i++)
  {
    vec_str[0] = '\0';
    //amt_written = sprintf(vec_str, "%s%s_Aggregated ", vec_str, ss_mon_info->svc_info[0].svc_table[i].disp_name);
    if(ss_mon_info->agg_graph)
      amt_written = sprintf(vec_str, "%s_Aggregated ", ss_mon_info->svc_info[0].svc_table[i].disp_name);

    LPSDL1(NULL,  "ss_mon_info->total_svc_info = %d", ss_mon_info->total_svc_info);
    for(j = 0; j < ss_mon_info->total_svc_info; j++)
    {
      if(ss_mon_info->instance_graph){
        amt_written = sprintf(vec_str, "%s%s_%s ", vec_str, ss_mon_info->svc_info[0].svc_table[i].disp_name, 
                               ss_mon_info->svc_info[j].instance);
      }
    }

    LPSDL1(NULL,  "ss_mon_info->svc_info[0].svc_table[%d].numComp = %d", i, ss_mon_info->svc_info[0].svc_table[i].numComp);
    for(j = 0; j < ss_mon_info->svc_info[0].svc_table[i].numComp; j++)
    {
      if(ss_mon_info->agg_graph) {
        amt_written = sprintf(vec_str, "%s%s_%s_Aggregated ", vec_str, ss_mon_info->svc_info[0].svc_table[i].disp_name, 
                               ss_mon_info->svc_info[0].svc_table[i].comp_table[j].disp_name);
      }
      for(k = 0; k < ss_mon_info->total_svc_info; k++)
      {
        if(ss_mon_info->comp_graph) {
          amt_written = sprintf(vec_str, "%s%s_%s_%s ", vec_str, ss_mon_info->svc_info[0].svc_table[i].disp_name, 
                               ss_mon_info->svc_info[0].svc_table[i].comp_table[j].disp_name, 
                               ss_mon_info->svc_info[k].instance);
        }
      }
    }
    if(send_message(ss_mon_info->svc_data_mon_fd, vec_str, amt_written) != amt_written)
    {
      LPSEL1(NULL, "Error in sending vector to NS");
      return;
    }
    LPSDL1(NULL,  "%s", vec_str);
  }
  if(send_message(ss_mon_info->svc_data_mon_fd, "\n", strlen("\n")) != strlen("\n"))
  {
    LPSEL1(NULL, "Error in sending vector to NS");
    return;
  }
  LPSDL1(NULL,  "Method Exiting");
}

#define RESET_ALL_VALUE() {     \
  cumRequest = 0;               \
  numRequest = 0;               \
  cumDBRespTime = 0;            \
  cumQWTime = 0;                \
  cumAppTime = 0;               \
  numSuccessReq = 0;            \
  numFailReq = 0;               \
}

void svc_send_vector_data_to_ns(service_stats_info *ss_mon_info)
{
  int amt_written = 0;
  char vec_str[8 * 1024] = "";

  int i, j, k;
  int cumRequest = 0;
  int numRequest = 0;
  int cumDBRespTime = 0;
  int cumQWTime = 0;
  int cumAppTime = 0;
  int numSuccessReq = 0;
  int numFailReq = 0;
  int timeout = 10;
  LPSDL1(NULL,  "Methos Called");

  signal(SIGCHLD, SIG_IGN);
  pthread_mutex_lock(&ss_mon_info->svc_mon_data_mutex);
  LPSDL1(NULL,  "ss_mon_info->total_service = %d", ss_mon_info->total_service);
  for(i = 0; i < ss_mon_info->total_service; i++)
  {
    vec_str[0] = '\0';
    RESET_ALL_VALUE();
    //amt_written = sprintf(vec_str, "%s%s_Aggregated ", vec_str, ss_mon_info->svc_info[0].svc_table[i].disp_name);
    if(ss_mon_info->agg_graph) {
      for(j = 0; j < ss_mon_info->total_svc_info; j++)
      {
        cumRequest += ss_mon_info->svc_info[j].svc_table[i].cumRequest;
        numRequest += ss_mon_info->svc_info[j].svc_table[i].numRequest;
        cumDBRespTime += ss_mon_info->svc_info[j].svc_table[i].cumDBRespTime;
        cumQWTime += ss_mon_info->svc_info[j].svc_table[i].cumQWTime;
        cumAppTime += ss_mon_info->svc_info[j].svc_table[i].cumAppTime;
        numSuccessReq += ss_mon_info->svc_info[j].svc_table[i].numSuccessReq;
        numFailReq += ss_mon_info->svc_info[j].svc_table[i].numFailReq;
      }
      if(numRequest)
      {
        amt_written = sprintf(vec_str, "%d %d %.3f %.3f %.3f %.3f %.3f %.3f\n",
                                         numRequest, 
                                         cumRequest, 
                                         numRequest/(double)timeout, 
                                         cumDBRespTime/(double)numRequest,
                                         cumAppTime/(double)numRequest,
                                         cumQWTime/(double)numRequest,
                                         numSuccessReq/(double)numRequest,
                                         numFailReq/(double)numRequest);
       }
       else
         amt_written = sprintf(vec_str, "%d %d %.3f %.3f %.3f %.3f %.3f %.3f\n",
                                         0, cumRequest, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    }


    LPSDL1(NULL, "ss_mon_info->total_svc_info = %d", ss_mon_info->total_svc_info);
    for(j = 0; j < ss_mon_info->total_svc_info; j++)
    {
      LPSDL1(NULL, "ss_mon_info->svc_info[%d].svc_table[%d].numRequest = %d", 
                     j, i, ss_mon_info->svc_info[j].svc_table[i].numRequest);
      if(ss_mon_info->instance_graph){
        if(ss_mon_info->svc_info[j].svc_table[i].numRequest)
        {
          amt_written = sprintf(vec_str, "%s%d %d %.3f %.3f %.3f %.3f %.3f %.3f\n", vec_str,
                        ss_mon_info->svc_info[j].svc_table[i].numRequest, 
                        ss_mon_info->svc_info[j].svc_table[i].cumRequest, 
                        ss_mon_info->svc_info[j].svc_table[i].numRequest/(double)timeout, 
                        ss_mon_info->svc_info[j].svc_table[i].cumDBRespTime/(double)ss_mon_info->svc_info[j].svc_table[i].numRequest,
                        ss_mon_info->svc_info[j].svc_table[i].cumAppTime/(double)ss_mon_info->svc_info[j].svc_table[i].numRequest,
                        ss_mon_info->svc_info[j].svc_table[i].cumQWTime/(double)ss_mon_info->svc_info[j].svc_table[i].numRequest,
                        ss_mon_info->svc_info[j].svc_table[i].numSuccessReq/(double)ss_mon_info->svc_info[j].svc_table[i].numRequest,
                        ss_mon_info->svc_info[j].svc_table[i].numFailReq/(double)ss_mon_info->svc_info[j].svc_table[i].numRequest);
        }
        else
          amt_written = sprintf(vec_str, "%s%d %d %.3f %.3f %.3f %.3f %.3f %.3f\n",
                                         vec_str, 0, ss_mon_info->svc_info[j].svc_table[i].cumRequest, 
                                         0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
      }
      ss_mon_info->svc_info[j].svc_table[i].numRequest = 0;
      ss_mon_info->svc_info[j].svc_table[i].cumDBRespTime = 0;
      ss_mon_info->svc_info[j].svc_table[i].cumAppTime = 0;
      ss_mon_info->svc_info[j].svc_table[i].cumQWTime = 0;
      ss_mon_info->svc_info[j].svc_table[i].numSuccessReq = 0;
      ss_mon_info->svc_info[j].svc_table[i].numFailReq = 0; 
    }

    LPSDL1(NULL,  "ss_mon_info->svc_info[0].svc_table[%d].numComp = %d", i, ss_mon_info->svc_info[0].svc_table[i].numComp);
    for(j = 0; j < ss_mon_info->svc_info[0].svc_table[i].numComp; j++)
    {
      RESET_ALL_VALUE();
      if(ss_mon_info->agg_graph) {
        for(k = 0; k < ss_mon_info->total_svc_info; k++)
        {
          cumRequest += ss_mon_info->svc_info[k].svc_table[i].comp_table[j].cumRequest;
          numRequest += ss_mon_info->svc_info[k].svc_table[i].comp_table[j].numRequest;
          cumDBRespTime += ss_mon_info->svc_info[k].svc_table[i].comp_table[j].cumDBRespTime;
          cumQWTime += ss_mon_info->svc_info[k].svc_table[i].comp_table[j].cumQWTime;
          cumAppTime += ss_mon_info->svc_info[k].svc_table[i].comp_table[j].cumAppTime;
          numSuccessReq += ss_mon_info->svc_info[k].svc_table[i].comp_table[j].numSuccessReq;
          numFailReq += ss_mon_info->svc_info[k].svc_table[i].comp_table[j].numFailReq;
        }
        if(numRequest)
        {
          amt_written = sprintf(vec_str, "%s%d %d %.3f %.3f %.3f %.3f %.3f %.3f\n", 
                                           vec_str, numRequest, cumRequest, 
                                           numRequest/(double)timeout, 
                                           cumDBRespTime/(double)numRequest,
                                           cumAppTime/(double)numRequest,
                                           cumQWTime/(double)numRequest,
                                           numSuccessReq/(double)numRequest,
                                           numFailReq/(double)numRequest);
        }
        else
          amt_written = sprintf(vec_str, "%s%d %d %.3f %.3f %.3f %.3f %.3f %.3f\n",
                                         vec_str, 0, cumRequest, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
      }
      for(k = 0; k < ss_mon_info->total_svc_info; k++)
      { 
        if(ss_mon_info->comp_graph)
        {
          if(ss_mon_info->svc_info[k].svc_table[i].comp_table[j].numRequest)
          {
            amt_written = sprintf(vec_str, "%s%d %d %.3f %.3f %.3f %.3f %.3f %.3f\n", vec_str,
                        ss_mon_info->svc_info[k].svc_table[i].comp_table[j].numRequest, 
                        ss_mon_info->svc_info[k].svc_table[i].comp_table[j].cumRequest, 
                        ss_mon_info->svc_info[k].svc_table[i].comp_table[j].numRequest/(double)timeout, 
                        ss_mon_info->svc_info[k].svc_table[i].comp_table[j].cumDBRespTime/
                        (double)ss_mon_info->svc_info[k].svc_table[i].comp_table[j].numRequest,
                        ss_mon_info->svc_info[k].svc_table[i].comp_table[j].cumAppTime/
                        (double)ss_mon_info->svc_info[k].svc_table[i].comp_table[j].numRequest,
                        ss_mon_info->svc_info[k].svc_table[i].comp_table[j].cumQWTime/
                        (double)ss_mon_info->svc_info[k].svc_table[i].comp_table[j].numRequest,
                        ss_mon_info->svc_info[k].svc_table[i].comp_table[j].numSuccessReq/
                        (double)ss_mon_info->svc_info[k].svc_table[i].comp_table[j].numRequest,
                        ss_mon_info->svc_info[k].svc_table[i].comp_table[j].numFailReq/
                        (double)ss_mon_info->svc_info[k].svc_table[i].comp_table[j].numRequest);
          }
          else
            amt_written = sprintf(vec_str, "%s%d %d %.3f %.3f %.3f %.3f %.3f %.3f\n",
                                         vec_str, 0, 
                                         ss_mon_info->svc_info[k].svc_table[i].comp_table[j].cumRequest, 
                                         0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
        }
        ss_mon_info->svc_info[k].svc_table[i].comp_table[j].numRequest = 0;
        ss_mon_info->svc_info[k].svc_table[i].comp_table[j].cumDBRespTime = 0;
        ss_mon_info->svc_info[k].svc_table[i].comp_table[j].cumAppTime = 0;
        ss_mon_info->svc_info[k].svc_table[i].comp_table[j].cumQWTime = 0;
        ss_mon_info->svc_info[k].svc_table[i].comp_table[j].numSuccessReq = 0;
        ss_mon_info->svc_info[k].svc_table[i].comp_table[j].numFailReq = 0;
      }
    }
    if(send_message(ss_mon_info->svc_data_mon_fd, vec_str, amt_written) != amt_written)
    {
      LPSEL1(NULL,  "Error: in vector data");
      return;
    }
    LPSDL1(NULL,  "%s", vec_str);
  }  
  pthread_mutex_unlock(&ss_mon_info->svc_mon_data_mutex);
  LPSDL1(NULL,  "Method Exiting");
}

void free_service_table(service_stats_info *ss_mon_info)
{
  int i, j, k;
  for(k = 0; k < ss_mon_info->total_svc_info; k++) {
    for(i = 0; i < ss_mon_info->total_service; i++)
    { 
      //Free name
      FREE_AND_MAKE_NULL(ss_mon_info->svc_info[k].svc_table[i].name, "name", -1);
      FREE_AND_MAKE_NULL(ss_mon_info->svc_info[k].svc_table[i].disp_name, "disp_name", -1);
    
      //Free component
      for(j = 0; j < ss_mon_info->svc_info[k].svc_table[i].numComp; j++)
      {
        FREE_AND_MAKE_NULL(ss_mon_info->svc_info[k].svc_table[i].comp_table[j].name, "name", -1);
        FREE_AND_MAKE_NULL(ss_mon_info->svc_info[k].svc_table[i].comp_table[j].disp_name, "disp_name", -1);
      }
      FREE_AND_MAKE_NULL(ss_mon_info->svc_info[k].svc_table[i].comp_table, "comp_table", -1);
    }
    FREE_AND_MAKE_NULL(ss_mon_info->svc_info[k].svc_table, "svc_table", -1);
  }
}


#ifdef BUILD_SVC_ALONE
void print_usage_svc(char *prog_name, char * message)
{
  fprintf(stderr, "%s %s\n", prog_name, message);
  exit(-1);
}

static inline void svc_parse_args(int argcount, char **argvector)
{
  char c;
  int tflag=0;
  char msg[64] = "";

  while ((c = getopt(argcount, argvector, "m:t:d:W:")) != -1)
  {
    switch (c)
    {

      case 'm':
        dbmode = atoi(optarg);
        break;

      case 't':
        tflag++;
        testidx = atoi(optarg);
        break;

      case 'd':
        svc_loglevel = atoi(optarg);
        break;

      case 'W':
        strcpy(ns_wdir, optarg);
        break;

      case ':':
      case '?':
        sprintf(msg, "Invalid argument %c", c);
        print_usage_svc(argvector[0], msg);
        exit(1);
    }
  }
  if (tflag != 1)
  {
    print_usage_svc(argvector[0], "Testrun number not specified");
    exit(1);
  }
}

static int read_from_file (svc_info_t *svc_info, FILE *fp, char *read_buffer, unsigned int num_bytes)
{
  int read_bytes = 0;

  read_bytes = fread(read_buffer , 1, num_bytes, fp);

  if(read_bytes < 0){
    SVC_DL2(svc_info, svc_info->threadIndex, "Error: Error in reading file. (%s)", strerror(errno));
    exit(-1);
  }
  read_buffer[read_bytes] = '\0';
  return read_bytes;
}

static char *get_last_newline_ptr(char *buf, int size)
{
  char *ptr = buf + size - 1; /* Points to last byte */

  while(1)
  {
    if (*ptr == '\n')
      break;

    ptr--;

    if (ptr == buf)
      return NULL;
  }

  return ptr;
}

void *process_svc_raw_data(void *args)
{
  char raw_filename[1024];
  char raw_buffer[MAX_LINE_LENGTH];
  FILE *fp = NULL;
  svc_info_t *svc_info = (svc_info_t *) args;
  int threadIndex = ((svc_info_t *) args)->threadIndex;
  int is_test_running;
  int num_unproc_bytes = 0;
  int num_bytes_read;
  char *ptr;
  char c;
  char *ownername = ((service_stats_info*) svc_info->ss_mon_info)->ownername;
  char *grpname = ((service_stats_info*) svc_info->ss_mon_info)->grpname;

  svc_instance_init(svc_info);

  sprintf(raw_filename, "%s/logs/TR%d/svc/raw_data/%s", ns_wdir, testidx, 
                        svc_info[threadIndex].instance);

  fp = my_fopen(raw_filename, "r");

  if(!fp)
  {
    SVC_DL2(NULL, -1, "Error in opening file '%s', error = %s\n", raw_filename, strerror(errno));
    return NULL;
  }

  /* Allocate memory for svc log file name for this thread and open logfile */
  char logfilename[1024] = "";
  char cmdbuf[1024] = "";

  if(logdir_created > 0)
  {
    char tmpfilename[1024];
    strcpy(tmpfilename, raw_filename);
    
    sprintf(logfilename, "%s/logs/TR%d/svc/logs/svc_debug_%s.log",
                          ns_wdir, testidx, basename(tmpfilename));

    svc_info[threadIndex].logfilename = malloc(strlen(logfilename) + 1);
    strcpy(svc_info[threadIndex].logfilename, logfilename);

    CREATE_FILE(threadIndex, svc_info[threadIndex].logfp, svc_info[threadIndex].logfilename, cmdbuf, svc_info);
    svc_info[threadIndex].logfp = my_fopen(svc_info[threadIndex].logfilename, "a");

    if(svc_info[threadIndex].logfp == NULL)
      SVC_DL2(NULL, -1, "ERROR opening logfile for writing %s\n", svc_info[threadIndex].logfilename);
  }

  SVC_DL2(svc_info, threadIndex, "Service Reports - Processing started for raw file %s", raw_filename);
  SVC_DL2(svc_info, threadIndex, "Service Reports - Processing started for raw file %s", raw_filename);


  while(1)
  {
    is_test_running = nslb_chk_is_test_running(testidx);
    while(1)
    {
      num_bytes_read = read_from_file(svc_info, fp, raw_buffer + num_unproc_bytes,
                                      MAX_LINE_LENGTH - num_unproc_bytes);
   
      if(num_bytes_read == 0) //Data is over
      {
        /* If there is leftover data from previous read, process it */
        if(num_unproc_bytes > 0)
        {
          if(process_svc_data(raw_buffer, num_unproc_bytes, svc_info) == -1)
            SVC_DL2(svc_info, threadIndex, "Error: in process_svc_data()");
        }
        break;
      }
   
      ptr = get_last_newline_ptr(raw_buffer, (num_bytes_read + num_unproc_bytes));
   
      if(ptr == NULL)
      {
        /* Newline not found in current read cycle */
        num_unproc_bytes = num_bytes_read + num_unproc_bytes;
        continue;
      }
      num_unproc_bytes = (num_unproc_bytes + num_bytes_read) - (ptr - raw_buffer) - 1;
   
      c = *(ptr + 1); /* Save the byte after last \n in c */
      *(ptr + 1) = '\0';
   
      if(process_svc_data(raw_buffer, (ptr - raw_buffer) + 1, svc_info) == -1)
      {
        SVC_DL2(svc_info, threadIndex, "Error: in process_svc_data");
        break;
      }
   
      /* Replace the saved character */
      *(ptr + 1) = c;
   
      /* Move the leftover bytes to the beginning of the buffer */
      if(num_unproc_bytes)
        memmove(raw_buffer, ptr + 1, num_unproc_bytes);
    }
   
    if(is_test_running <= 0)
    {
/***** CLEANUP - begin *****/
      svc_info[threadIndex].allocated_num_svc_sessions = 0;
      svc_info[threadIndex].num_svc_sessions = 0;
/***** CLEANUP - end *****/
      break;
    }
   
    /* Sleep for ~ 300 msecs */
    usleep(300*1024);
  }

  if(svc_info[threadIndex].svcCompRecordIndex != 0) 
  {
    if(svc_info[threadIndex].svc_comp_record_fp)
    {
      fprintf(svc_info[threadIndex].svc_comp_record_fp, "%s", svc_info[threadIndex].svcCompRecordBuf);
      fflush(svc_info[threadIndex].svc_comp_record_fp);
    }
  }


  return NULL;
}



int main(int argc,char **argv)
{
  //char is_db_mode = 1;

  char t_buf[2048];
  char *raw_data_dirname = t_buf;//, *cmd = t_buf;

  struct stat st;
  int threadIndex = -1;

  svc_parse_args(argc, argv);

  //to set the environment variable
  if(!ns_wdir[0])
  {
    if (getenv("NS_WDIR") != NULL)
      strcpy(ns_wdir, getenv("NS_WDIR"));
    else
    {
      fprintf(stderr, "NS_WDIR env variable is not set.");
      exit(-1);
    }
  }
  SVC_MALLOC(ss_mon_info, sizeof(service_stats_info), "service_stats_info", NULL);
  memset(ss_mon_info, 0, sizeof(service_stats_info));
  ss_mon_info->ddr = 1;
  ss_mon_info->dbmode = 1;
  strcpy(ss_mon_info->ownername, "netstorm");                     \
  strcpy(ss_mon_info->grpname, "netstorm");                       \
  ss_mon_info->svc_ns_wdir = ns_wdir;

  char *ownername = ss_mon_info->ownername;
  char *grpname  = ss_mon_info->grpname;

  if(svc_loglevel > 0)
  {
    /* Create Directory for debug logs */
    char log_dirname[1024];
 
    sprintf(log_dirname, "%s/logs/TR%d/svc/logs/tmp", ns_wdir, testidx);
 
    if(mkdir_ex(log_dirname) == 0){ 
      SVCEL("ERROR creating svc log directory %s", log_dirname);
    }
 
    else
    {
      char cmdbuf[1024] = "";
      logdir_created = 1;
      sprintf(cmdbuf, "chown %s:%s %s/logs/TR%d/svc/logs", ownername, grpname, ns_wdir, testidx);
      system(cmdbuf);

      sprintf(logfilename, "%s/logs/TR%d/svc/logs/svc_debug.log", ns_wdir, testidx);
 
      CREATE_FILE(-1, logfp, logfilename, cmdbuf, ss_mon_info);
      logfp = my_fopen(logfilename, "a");
 
      if(logfp == NULL)
        fprintf(stderr, "ERROR opening logfile for writing %s\n", logfilename);
    }
  }

  // reading path of directory
  sprintf(raw_data_dirname, "%s/logs/TR%d/svc/raw_data/", ns_wdir, testidx);

  if (stat(raw_data_dirname, &st) < 0)
  {
    SVC_DL2(ss_mon_info, -1, "ERROR: The directory does not exists: %s\n", raw_data_dirname);
    return -1;
  }

  DIR * dirp;
  struct dirent * entry;

  pthread_t *thread = NULL;
  pthread_attr_t attr;
  int t, ret;

  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

  int max_file = 0;
  int num_file = 0;

  dirp = opendir(raw_data_dirname); /* There should be error handling after this */


  if(dirp)
  {
 
    while ((entry = readdir(dirp)) != NULL) {
      if (entry->d_type == DT_REG) { /* If the entry is a regular file */
        if(num_file == max_file)
        {
           thread = (pthread_t *) realloc(thread, 8 * sizeof(pthread_t));
           ss_mon_info->svc_info =  (svc_info_t *)realloc(ss_mon_info->svc_info, 8 * sizeof(svc_info_t));
           memset(&ss_mon_info->svc_info[max_file], 0, sizeof(svc_info_t) * 8);
           max_file += 8;
        }
        num_file++;
    
        strcpy(ss_mon_info->svc_info[num_file - 1].instance, entry->d_name);
        ss_mon_info->svc_info[num_file - 1].threadIndex = num_file - 1;
      }
    }
  }
  if(num_file)
  {
    ss_mon_info->total_svc_info = num_file;
    svc_init(ss_mon_info, ns_wdir, testidx);
    for(t = 0; t < num_file; t++) {
      ss_mon_info->svc_info[t].ss_mon_info = ss_mon_info;
      ret = pthread_create(&thread[t], &attr, process_svc_raw_data, (void *)(&ss_mon_info->svc_info[t]));
      if (ret)
      {
        exit(-1);
      }
    }
  }
  closedir(dirp);

  if(num_file == 0) 
  {
    SVC_DL2(ss_mon_info, -1, "There is no file to process.");
    return -1;
  }

  pthread_attr_destroy(&attr);
  for(t = 0; t < num_file; t++)
  {
    ret = pthread_join(thread[t], NULL);
    if(ret)
    {
      exit(-1);
    }
  }
  svc_close(ss_mon_info);
  SVC_DL2(ss_mon_info, -1, "Method Exiting\n");
  return 0;
}
#endif

