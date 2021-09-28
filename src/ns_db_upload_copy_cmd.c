#define _GNU_SOURCE
 
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <libpq-fe.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <regex.h>
#include <stdarg.h>
#include <pwd.h>
#include <grp.h>
#include "ns_exit.h"

#include "nslb_db_util.h"
#include "nslb_util.h"

static int test_run_info_shm_key;
static long long chunk_size = 128 * 1024 * 1024;
static int ideal_time = 5;
static int tr_num;
static int running_mode = 0;
static char ns_wdir[512] = "/home/cavisson/work";
#ifdef DB_DEBUG_ON
static int debug_level = 2;
#else
static int debug_level = 0;
#endif
static FILE *debug_fp = NULL;
static FILE *error_fp = NULL;
static int summary_flag = 0;
static FILE *commentry_fp = NULL;
static int resume_flag = 0;
static int flag_create_table = 1;
static int parent_pid = 0;
static int commentry_interval = 0;
static int ownerid, grpid;
static int is_test_running = 1;
static  long long tr_start_timestamp_in_secs = 0;
static char partition_name[1024] = "";
static char nde_md_directory[1024] = "";
static int is_first_test_run = 0;
static long long overall_start_time_in_secs = 0;
static long long end_time_in_secs = 0;
static long long cav_epoch_diff = 0;
static int create_partition_table = 1;
static int wait_interval_in_secs = 60;
static int csv_complete_flag = 0;

#define STAND_ALONE_MODE 0
#define DB_CHILD_MODE 1
#define MAX_LINE_LENGTH 4096

static int process_mode = STAND_ALONE_MODE;

inline void print_usages()
{
  fprintf(stderr, "nsu_db_upload --testrun <testrun number> [--chunk_size <chunk size>] "
                   "[--ideal_time <ideal_time>] [--commentry <commentry interval>]"
                   "[--summary] [--resume] [--ppid <pid of parent process>]  [--resume] "
                   "[--create_table <0/1>] [--debug <debug_level>]" 
                   "[--partition_name <partition directory name>]"
                   "[--is_first_test_run <0/1>] [--create_partition_table [0/1]\n"); 
}
#ifdef DB_DEBUG_ON

#define DB_UPLOAD_DEBUG1(thread_id, ... ) if(debug_level >= 1) debug_log(thread_id, __FUNCTION__, __VA_ARGS__); 
#define DB_UPLOAD_DEBUG2(thread_id, ... ) if(debug_level >= 2) debug_log(thread_id, __FUNCTION__, __VA_ARGS__); 
#define DB_UPLOAD_DEBUG3(thread_id, ... ) if(debug_level >= 3) debug_log(thread_id, __FUNCTION__, __VA_ARGS__); 
#define DB_UPLOAD_DEBUG4(thread_id, ... ) if(debug_level >= 4) debug_log(thread_id, __FUNCTION__, __VA_ARGS__);

#else 
#define DB_UPLOAD_DEBUG1(thread_id, ... )   
#define DB_UPLOAD_DEBUG2(thread_id, ... )   
#define DB_UPLOAD_DEBUG3(thread_id, ... ) 
#define DB_UPLOAD_DEBUG4(thread_id, ... )  
#endif

#define LOG_COMMENTRY(...) if(commentry_interval && commentry_fp) fprintf(commentry_fp, __VA_ARGS__);

#define DB_UPLOAD_ERROR(thread_id, ...) error_log(thread_id, __FUNCTION__, __VA_ARGS__);


static void error_log(int thread_id, const char *fname, char *format, ...)
{
  va_list ap;
  char buffer[MAX_LINE_LENGTH + 1];
  char date_string[100];

  va_start(ap, format);
  vsnprintf(buffer, MAX_LINE_LENGTH, format, ap);
  va_end(ap);
  buffer[MAX_LINE_LENGTH] = 0;
  if(error_fp){
    fprintf(error_fp, "%s|%s|%d|%s\n", nslb_get_cur_date_time(date_string, 0), fname, thread_id, buffer);
    fflush(error_fp);
    //set commentry
    LOG_COMMENTRY("Thread[%d]:%s\n", thread_id, buffer);
  }
  else
    fprintf(stderr, "%s|%s|%d|%s\n", nslb_get_cur_date_time(date_string, 0), fname, thread_id, buffer);
}

#ifdef DB_DEBUG_ON
static void debug_log(int thread_id, const char *fname, char *format, ...)
{
  va_list ap;
  char buffer[MAX_LINE_LENGTH + 1];
  char date_string[100];

  va_start(ap, format);
  vsnprintf(buffer, MAX_LINE_LENGTH, format, ap);
  va_end(ap);
  buffer[MAX_LINE_LENGTH] = 0;
  if(debug_fp){
    fprintf(debug_fp, "%s|%s|%d|%s\n", nslb_get_cur_date_time(date_string, 0), fname, thread_id, buffer);
    fflush(debug_fp);
  }
}
#endif

static inline void read_owner_group()
{
  char filename[256];
  struct stat s;
  struct passwd *pwd;
  struct group *grp;

  snprintf(filename, 256, "%s/logs/TR%d", ns_wdir, tr_num);
  stat(filename, &s);
  pwd = getpwuid(s.st_uid);
  if(pwd)
    ownerid = pwd->pw_uid;
  grp = getgrgid(s.st_gid);
  if(grp)
    grpid = pwd->pw_gid;
}
void nsu_parse_args(int argc, char *argv[]){
  
  DB_UPLOAD_DEBUG3(0, "Method Called");
  char c;
  int t_flag = 0;
  // array for long argument support
  struct option longopts[] = {
                               {"testrun", 1, NULL, 't'},
                               {"test_run_info_shm_key", 1, NULL, 'k'},
                               {"running_mode", 1, NULL, 'm'},
                               {"chunk_size",  1, NULL, 's'},
                               {"ideal_time",  1, NULL, 'T'},
                               {"debug",  1, NULL, 'd'},
                               {"resume",  0, NULL, 'r'},
                               {"ppid", 1, NULL, 'P'},
                               {"commentry", 1, NULL, 'c'},
                               {"create_table", 1, NULL, 'C'},
                               {"summary", 0, NULL, 'S'},
                               //{"tr_start_timestamp_in_msecs", 0, NULL, 'R'},
                               {"partition_name", 1, NULL, 'p'},
                               {"is_first_test_run", 1, NULL, 'f'},
                               {"create_partition_table", 1, NULL, 'F'},
                               {"cav_epoch_diff", 1, NULL, 'e'},
                               {"wait_interval_in_secs", 1, NULL, 'w'},
                               {0, 0, 0,0}
                             };


   while ((c = getopt_long(argc, argv, "t:k:m:s:T:d:rP:c:C:S:R:f:p:F:e:w:", longopts, NULL)) != -1){
    switch (c){
      case 't':
        tr_num = atoi(optarg);
        t_flag = 1;
        break;
       case 'k':
         test_run_info_shm_key = atoi(optarg);
         break;
      case 's':
        chunk_size = atoll(optarg);
        break;        
      case 'T':
        ideal_time = atoi(optarg);
        break;  
      case 'm':
        running_mode = atoi(optarg);
        break;  
      case 'd':
        debug_level = atoi(optarg);
        break;
      case 'r':
        resume_flag = 1;
        break;
      case 'P':
        parent_pid = atoi(optarg);
        break;
      case 'c':
        /*commentry interval should be greater than 0 */
        commentry_interval = atoi(optarg)>0?atoi(optarg):0;
        if(commentry_interval) summary_flag = 1;
        break;
      case 'C':
        /* By default, this flag is set, means that if user does not pass -C, it 
         * creates db tables For disabling, -C 0 has to be passed. 
         * NS sends -C 0 option. 
         */
        flag_create_table = atoi(optarg);
        break;
      case 'S':
        summary_flag = 1;
        break;
      case 'p':
        //checking if partition mode is on. if partition mode is off then partition id is -1
        if(strcmp(optarg, "-1"))
        {
          strcpy(partition_name, optarg); 
          strcpy(nde_md_directory, "common_files");
        }
        break;  
      case 'R':
        tr_start_timestamp_in_secs = (atoll(optarg) * (long long)1000);
        break;  
      case 'f':
        is_first_test_run = atoi(optarg);
        break;  
      case 'F':
        create_partition_table = atoi(optarg);
        break;  
      case 'e':
        cav_epoch_diff = atoll(optarg);
        break;  
      case 'w':
        wait_interval_in_secs = atoi(optarg);
        break;  
      case ':':
      case '?':
        fprintf(stderr, "Invalid option: %c\n", c); 
        print_usages();
        exit(-1); 
    }
  }
  if(!t_flag)
  {
    fprintf(stderr, "--testrun argument missing\n");
    print_usages();
    exit(-1);
  }
}


#define MAX_FILE_NAME 2048
typedef struct csv_file_info {

  char file_path[MAX_FILE_NAME];
  FILE *fp;
  char *partial_buf;
  int partial_buf_len;
  int max_partial_buf_len;
  FILE *off_fp;
  int comm_count;
  int wait_interval;

} csv_file_info;

/*This method will create table and fill db_info structure */
static int ns_db_create_db_con (db_conn_table *db_con_info, char *csv_file_name)
{
  int i;
  char *local_csv = csv_file_name;

  db_con_info->conn = PQconnectdb("dbname=test user=netstorm");
  DB_UPLOAD_DEBUG3(0, "Creating db connection for csv file = %s\n", local_csv);

  if (PQstatus(db_con_info->conn) != CONNECTION_OK){
    DB_UPLOAD_ERROR(0, "Connection to database failed: %s", PQerrorMessage(db_con_info->conn));
    return -1;
  }
  for(i = 0; i < NUM_DB_TABLES; i++)
  {
    //if(strstr(local_csv, db_file_table[i].csv_file) != NULL)
    if(!strcmp(local_csv, db_file_table[i].csv_file))
    {
      if(((!strcasecmp(local_csv, "urc.csv"))|| (!strcasecmp(local_csv, "prc.csv")) || (!strcasecmp(local_csv, "trc.csv")) || (!strcasecmp(local_csv, "tprc.csv")) || (!strcasecmp(local_csv, "src.csv"))) && partition_name[0] != 0)
      {
        sprintf(db_con_info->table, "%s%d_%s", db_file_table[i].table, tr_num, partition_name);
        db_con_info->csv_file_idx = i;
      }
      else
      {
        sprintf(db_con_info->table, "%s%d", db_file_table[i].table, tr_num);
        db_con_info->csv_file_idx = i;
      }
      sprintf(db_con_info->file_path, "%s", db_file_table[i].file_path);
      db_con_info->nd_type = db_file_table[i].nd_type;
      break;
    }
  }
  if(i == NUM_DB_TABLES)
  {
    DB_UPLOAD_ERROR(0, "No table matched for csv file \'%s\'", local_csv);
    return -1;
  }
  DB_UPLOAD_DEBUG3(0, "DB connection created for csv file = %s\n", local_csv);
  return 0;
}

//Method to fill the buffer.
//This will read the file 
static int read_file_and_fill_buf (FILE *file_ptr, char *buff_to_fill, long bytes_to_fill)
{
  int read_bytes = 0;
   
  //fprintf(stdout, "Method Called: file_ptr = %p, bytes_to_fill = %u\n", file_ptr, bytes_to_fill);
  read_bytes = fread(buff_to_fill , 1, bytes_to_fill, file_ptr);
  if(read_bytes < 0){
      //error_log(_LF_, "Error: Error in reading file.(%s)\n", nslb_strerror(errno));
      DB_UPLOAD_ERROR(0 ,"Error: Error in reading file.(%s)", nslb_strerror(errno));
      NS_EXIT(-1, "Error: Error in reading file.(%s)", nslb_strerror(errno));
  }
  buff_to_fill[read_bytes] = '\0';
  //fprintf(stdout, "read_bytes = %d, buff_to_fill = %s\n", read_bytes, buff_to_fill);
  //If file have no data to read then return 0
    return read_bytes;

}

static long get_row_count_of_table(PGconn *db_con, char *table)
{
  char query[1024];
  PGresult *res = NULL;
  long total_rows;

  sprintf(query, "select count(*) from %s", table);
  res = PQexec(db_con, query);

  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    printf("Failed to get row count from table %s\n", table);
    return -1;
  }

  total_rows = atol(PQgetvalue(res, 0, 0));
  return total_rows; 
}

//this will return complete line bytes.
inline void get_complete_bytes(char *buf, int size, int *complete_line_size)
{
  int tmp_line_length = size;  
  int i;

  if(buf)
  {
    for(i = size; i > 0; i--){ 
      if(buf[i - 1] != '\n')
      {
        tmp_line_length --;
      }
      else
        break;
    }
    *complete_line_size = tmp_line_length;
  }
  else
    *complete_line_size = 0;
}

/*This function will update offset file for instance 0 to cur instance */
inline void update_offset_files(int thread_id, csv_file_info *csv_data, int cur_partial_buf_len)
{
  long offset;
  if(csv_data->fp && csv_data->off_fp)
  {
    offset = ftell(csv_data->fp);
    //write at starting of offset file
    fseek(csv_data->off_fp, 0, SEEK_SET);
    fprintf(csv_data->off_fp, "%ld;", (offset - cur_partial_buf_len));
    fflush(csv_data->off_fp);
    DB_UPLOAD_DEBUG3(thread_id, "updating offset file for csv file = %s", csv_data->file_path);
  }
  else
    DB_UPLOAD_ERROR(thread_id, "offset file not open for csv file = %s, failed to save current offset", csv_data->file_path);
}

#define CLEAR_WHITE_SPACE(ptr) {while ((*ptr == ' ') || (*ptr == '\t')) ptr++;}

#define DB_ERROR1 "missing data for column"
#define DB_ERROR2 "invalid input syntax"
#define DB_ERROR3 "extra data after last expected column"
#define DB_ERROR4 "duplicate key value violates unique constraint"


/* We are pasrsing error message to recover for any error , 
 * for that we need full error message so we are taking too big buffer 
*/
#define ERROR_MSG_LEN 128 * 1024

/* This method will copy buffer to db and will also handle some basic error conditions.
 * it will upload remaing data and will leave error lines 
 * ... If db connection error will occure then this will return -2, caller need to handle this case
 */
static int upload_buf_to_db(int thread_id, db_conn_table *db_con, char *buf, int len, int db_pid)
{
  char errorMsg[ERROR_MSG_LEN] = "";
  int next_line_num; 
  char *next_line_ptr = NULL;
  char *copy_buf = buf;
  int copy_len = len;
  char save_char;
 
  DB_UPLOAD_DEBUG2(thread_id, "Method called, buf len = %d, table name = %s", len, db_con->table);
  while(1)  {
    *errorMsg = 0;
    if(copy_len <= 0)
      return 0;

    if(db_copy_into_db_ex(db_con, copy_buf, copy_len, errorMsg, ERROR_MSG_LEN) == -1)
    {
      /*This is the case where one or more rows don't have complete columns.
	This is to handle hotspot case where we are writing \n in csv */
      DB_UPLOAD_ERROR(thread_id, "Failed to upload to db, error = %s", errorMsg);
      //Check for possible errors
      if(strstr(errorMsg, DB_ERROR1) || strstr(errorMsg, DB_ERROR2)
                                     || strstr(errorMsg, DB_ERROR3) || strstr(errorMsg, DB_ERROR4))
      {
        DB_UPLOAD_DEBUG2(thread_id, "Failed to copy buffer to db, one or more row have incomplete column, copying data before error line");
	char *tmp;
	tmp = strcasestr(errorMsg, " line ");
	if(tmp) {
          tmp += 6;
	  CLEAR_WHITE_SPACE(tmp);
	  next_line_num = atoi(tmp);
          DB_UPLOAD_DEBUG3(thread_id, "Error found at line number = %d", next_line_num);
	  next_line_ptr = copy_buf; 
          /*In case if error comes at first line, then just skip this line and continue */
          if(next_line_num <= 1)
          {
	    //update copy_buf and copy_len for next iteration.
            copy_buf = strchr(next_line_ptr, '\n') + 1;
	    DB_UPLOAD_ERROR(thread_id, "Invalid line, Table: [%s], Line: %*.*s", db_con->table, (copy_buf - next_line_ptr),
					       (copy_buf - next_line_ptr), next_line_ptr);
            copy_len = len - (copy_buf - buf);
            continue;
          }
	  //search for new line
	  int line_no = 0;
	  while(*next_line_ptr){
	    if(*next_line_ptr == '\n')
	    {
	      line_no += 1;
	      if(line_no == (next_line_num - 1)) 
		break;
	    }
	    next_line_ptr ++;
	  }
          DB_UPLOAD_DEBUG3(thread_id, "Copying data till %d line", line_no);
          copy_len = (next_line_ptr - copy_buf - 1);
          //set null at end of buffer to copy.(we are assuming that \n will be at then end of buffer)
          save_char = *(next_line_ptr + 1);
          *(next_line_ptr + 1) =  0; 
          /*Copy data before error line(we are not handling any kind of error in this case)*/
          if(db_copy_into_db_ex(db_con, copy_buf, copy_len, errorMsg, ERROR_MSG_LEN) == -1)
          {
            *(next_line_ptr + 1) = save_char;
            /*Check for db connection  */
            /*if(PQping("dbname=test user=netstorm") != PQPING_OK) */
            if(!nslb_check_pid_alive(db_pid))
            {
	      DB_UPLOAD_ERROR(thread_id, "There is some probleam in db connection, trying to reconnect");
	      return -2;
            }
            DB_UPLOAD_ERROR(thread_id, "Some unexpected error occure while copying data to db");
            break;
          }
          //update copy_buf and copy_len for next iteration.
          *(next_line_ptr + 1) = save_char;
          copy_buf = strchr(next_line_ptr + 1, '\n') + 1;
          DB_UPLOAD_ERROR(thread_id, "Invalid line, Table: [%s], Line: %*.*s", db_con->table, (copy_buf - next_line_ptr - 1),
                                             (copy_buf - next_line_ptr - 1), next_line_ptr + 1);
          copy_len = len - (copy_buf - buf);
	} 
	else  //don't got line num ?
	{
          break;
	}   
      }
      /*else if(PQping("dbname=test user=netstorm") != PQPING_OK)  Check for connection */
      else if(!nslb_check_pid_alive(db_pid))
      {      
        DB_UPLOAD_ERROR(thread_id, "There is some probleam in db connection, trying to reconnect");
        return -2;
      }
      else {  /*In any other error case , we will return */
        break;
      }
    }
    else
      return 0;
  }
  DB_UPLOAD_ERROR(thread_id, "Failed to copy csv data to db");
  return -1;
}

#define NUM_EOF_CHARS 5

#define URC_CSV_BIT	0x00000001 
#define SRC_CSV_BIT	0x00000002
#define TPRC_CSV_BIT	0x00000004
#define TRC_CSV_BIT	0x00000008
#define PRC_CSV_BIT	0x00000010

#define NS_CSV_BIT_MASK 0x0000001F


static void inline set_csv_bits(char *csv_filename)
{
  if(!strcmp(csv_filename, "urc.csv"))                                  
  {                                                                                                                  
    csv_complete_flag |= URC_CSV_BIT;                     
  }                                                                            
  else if(!strcmp(csv_filename, "src.csv"))                               
  {                                                                            
    csv_complete_flag |= SRC_CSV_BIT;                       
  }                                                                           
  else if(!strcmp(csv_filename, "tprc.csv"))                            
  {                                                                            
    csv_complete_flag |= TPRC_CSV_BIT;                    
  }                                                                            
  else if(!strcmp(csv_filename, "trc.csv"))                            
  {                                                                            
    csv_complete_flag |= TRC_CSV_BIT;                    
  }                                                                            
  else if(!strcmp(csv_filename, "prc.csv"))                              
  {                                                                             
    csv_complete_flag |= PRC_CSV_BIT;                      
  }                                                                            
  else                                                                        
    return;                                                    
}

#define DECREASE_OFFSET_IF_MASK_BIT_NOT_SET  \
  if(csv_complete_flag != NS_CSV_BIT_MASK)   \
  {                                          \
    *update_offset += 5;                     \
  }

#define DECREASE_OFFSET_IF_MASK_BIT_NOT_SET  \
  if(csv_complete_flag != NS_CSV_BIT_MASK)   \
  {                                          \
    *update_offset += 5;                     \
  }

#define DECREASE_BYTES_TO_COPY \
   *bytes_to_copy -= 4;

static void check_eof_for_csv_files_and_set_bits(char *buf, int *bytes_to_copy, char *csv_name, int *update_offset)
{
  char *tmp_ptr = buf;

  tmp_ptr += (*bytes_to_copy - NUM_EOF_CHARS);

  if(!strncmp(tmp_ptr, "\nEOF\n", NUM_EOF_CHARS))
  {
    set_csv_bits(csv_name);
    DECREASE_OFFSET_IF_MASK_BIT_NOT_SET
    DECREASE_BYTES_TO_COPY
  }
  else if(!strncmp(buf, "EOF\n", 4))
  {
    set_csv_bits(csv_name);
    DECREASE_OFFSET_IF_MASK_BIT_NOT_SET
    DECREASE_BYTES_TO_COPY
  }
  else
    return;

}


/***************************************************************************** 
 * read_and_copy_into_db()
 *
 *     This method reads csv file and uploads data to db.
 *
 *     csv_data is passed so that we can modify offset file at time of db upload. 
 *****************************************************************************/
static int read_and_copy_into_db(int thread_id, db_conn_table *db_con, csv_file_info *csv_data, 
                                 char *buf, long long *used_len, long long *max_len, 
                                 char *csv_name, int db_pid)
{
  long long max_copy_len = ((*max_len * 80)/100);
  int bytes_to_copy;
  int read_bytes;
  char tmpChar;
  char date_string[100] = "";
  long readen_bytes = 0;
  int local_partial_buf_len = 0;
  int conn_status = 0;

  DB_UPLOAD_DEBUG2(thread_id, "Method called, table name = %s, used_len = %lld, max_copy_len = %lld", 
                              db_con->table, *used_len, max_copy_len);   

  //Check if partial len and used buf len is greater than max len then 
  //first copy to db and then copy partial len to buf.
  if(csv_data->partial_buf_len) {
    if((*used_len + csv_data->partial_buf_len) > *max_len)
    {
      tmpChar = buf[*used_len]; /* Saved character */
      buf[*used_len] = 0;

      /* Need to handle error conditions. */
      check_eof_for_csv_files_and_set_bits(buf, &bytes_to_copy, csv_name, &csv_data->partial_buf_len);
      if(upload_buf_to_db(thread_id, db_con, buf, *used_len, db_pid) == -2)
      {
        /* DB Connection is closed */
        buf[*used_len] = tmpChar; /* Restore the saved char */
        return -2; 
      }

      buf[*used_len] = tmpChar; /* Restore the saved char */

#ifdef DB_DEBUG_ON
      {
        long nrows = get_row_count_of_table(db_con->conn, db_con->table);
        DB_UPLOAD_DEBUG3(thread_id, "csv data upload to table %s, Total rows in db = %d", 
                         db_con->table, nrows);
      }
#endif
      //update offset files
      update_offset_files(thread_id, csv_data, csv_data->partial_buf_len); 
      *used_len = 0;
    }
    //Copy last partial data to buf and update used_len.
    DB_UPLOAD_DEBUG3(thread_id, "Copying last partial data to read buffer");
    memcpy(buf + *used_len, csv_data->partial_buf, csv_data->partial_buf_len);
    *used_len += csv_data->partial_buf_len;
    //No need to free partial buf, as we will reuse it.
    local_partial_buf_len = csv_data->partial_buf_len;
    csv_data->partial_buf_len = 0;
  }

  while(1)
  {
    read_bytes = read_file_and_fill_buf((csv_data->fp), (buf+*used_len), (*max_len - *used_len));
    DB_UPLOAD_DEBUG3(thread_id, "Csv file [%s] Total read bytes = %d\n", csv_data->file_path, read_bytes);
    /*For commentry*/
    if(commentry_interval)
    {
      if(csv_data->comm_count == commentry_interval)
      {
        readen_bytes = ftell(csv_data->fp);
        if(readen_bytes > 0) { 
          LOG_COMMENTRY("Thread[%d]:csv=%s,time=%s,Total bytes read=%ld\n", 
                                      thread_id, csv_name, nslb_get_cur_date_time(date_string, 0), readen_bytes);
        }
        csv_data->comm_count = 0;
      }
      csv_data->comm_count++;
    }
    //If no data available then break loop.
    if(!read_bytes) {
      /*reset used_len because it will be used for next instance, and for that we need to remove partial data */
      *used_len -= local_partial_buf_len;
      bytes_to_copy =*used_len;
      break;
    }
    get_complete_bytes(buf, (*used_len + read_bytes), &bytes_to_copy);
    local_partial_buf_len = (*used_len + read_bytes - bytes_to_copy);

    DB_UPLOAD_DEBUG3(thread_id, "CSV file [%s] Total complete bytes = %d, Partial bytes length = %d\n", 
                     csv_data->file_path, bytes_to_copy, local_partial_buf_len);

    /*? Do we need to update used_len here because it may create problem in case of failed to copy in db */
    /*Check if bytes to copy is greater than 80 % of max then copy to db
      if last_instance_flag is set then it is mandatory to copy data*/

    //if(bytes_to_copy >= max_copy_len)
    /*first we were checking complete lines, now we checking total read data*/
    if(*used_len + read_bytes >= max_copy_len)
    {
      tmpChar = buf[bytes_to_copy];
      buf[bytes_to_copy] = 0;
      check_eof_for_csv_files_and_set_bits(buf, &bytes_to_copy, csv_name, &local_partial_buf_len);
      if(upload_buf_to_db(thread_id, db_con, buf, bytes_to_copy, db_pid) == -2)
      {
        buf[bytes_to_copy] = tmpChar;
        *used_len = bytes_to_copy;
        conn_status = -2;
        break;
      }
      buf[bytes_to_copy] = tmpChar;

#ifdef DB_DEBUG_ON
      {
        long nrows = get_row_count_of_table(db_con->conn, db_con->table);
        DB_UPLOAD_DEBUG3(thread_id, "csv data upload to table %s\n", db_con->table);
        DB_UPLOAD_DEBUG3(thread_id, "Total rows in db = %d\n", nrows);
      }
#endif

      update_offset_files(thread_id, csv_data, local_partial_buf_len);
      //move partial data to starting and update used_len
      if(local_partial_buf_len)
        memmove(buf, (buf + bytes_to_copy), local_partial_buf_len);
      *used_len = local_partial_buf_len;
    }
    else
    {
      tmpChar = buf[bytes_to_copy];
      buf[bytes_to_copy] = 0;
      check_eof_for_csv_files_and_set_bits(buf, &bytes_to_copy, csv_name, &local_partial_buf_len);
      if(upload_buf_to_db(thread_id, db_con, buf, bytes_to_copy, db_pid) == -2)
      {
        buf[bytes_to_copy] = tmpChar;
        *used_len = bytes_to_copy;
        conn_status = -2;
        break; 
      }
      buf[bytes_to_copy] = tmpChar;

#ifdef DB_DEBUG_ON
      {
        long nrows = get_row_count_of_table(db_con->conn, db_con->table);
        DB_UPLOAD_DEBUG3(thread_id, "csv data upload to table %s\n", db_con->table);
        DB_UPLOAD_DEBUG3(thread_id, "Total rows in db = %d\n", nrows);
      }
#endif

      update_offset_files(thread_id, csv_data, local_partial_buf_len);
      *used_len = 0;
      break;
    }
  }
  //Copy remained local partial buffer into partial buffer to that instance.
  if(local_partial_buf_len)
  {
    DB_UPLOAD_DEBUG3(thread_id, "CSV file[%s], partial buffer length = %d copying to structure\n", csv_data->file_path, local_partial_buf_len);
    //if partial buf len is greater then max partial buf len then realloc the buffer and set max partial buf length
    if(local_partial_buf_len > csv_data->max_partial_buf_len)
    {
      csv_data->partial_buf = realloc(csv_data->partial_buf, local_partial_buf_len);
      csv_data->max_partial_buf_len = local_partial_buf_len;
    }
    memcpy(csv_data->partial_buf, (buf + bytes_to_copy), local_partial_buf_len);
    csv_data->partial_buf_len = local_partial_buf_len; 
  }
  return conn_status;
}
/*******************************************************************************
 * check_and_set_offset()
 *
 *            Checks if offset file is present, and if resuming, seek the file 
 *            pointer to saved offset.
 *
 *            Else open the file for writing the offset.
 *******************************************************************************/
static inline void check_and_set_offset(int thread_id, char *csv_name, csv_file_info *csv_table, char partition_md_flag)
{
  char *csv_file_name = csv_table->file_path;
  char offset_file[MAX_FILE_NAME];
  struct stat fpstat;
  long offset = 0;
  FILE *fp;
 
  if(!csv_file_name) 
    return;
  sprintf(offset_file, "%s/webapps/logs/TR%d/%s/reports/csv/.%s.offset" , ns_wdir, tr_num, 
                        (partition_md_flag)?nde_md_directory:partition_name, csv_name);
  /*Check if file exist then read offset from that file and set to file.
   * else create file and dump 0 as offset
   */
  if(!stat(offset_file, &fpstat)) 
  {
    DB_UPLOAD_DEBUG3(thread_id, "Offset file %s found setting file pointer to given offset", offset_file);
    if(resume_flag && create_partition_table && !partition_md_flag)
    {
      int fd=open(offset_file, O_RDONLY | O_WRONLY | O_TRUNC | O_CLOEXEC);
      if(fd == -1)
      {
        NS_EXIT(-1, "Could not open file = [%s]", offset_file);
      }
      else
      {
        fp = fdopen(fd, "w");
        fprintf(fp, "%s", "0;");
        fclose(fp);
      }
    }

    fp = fopen(offset_file, "r+");
    csv_table->off_fp = fp; 
    if(!fp){
      DB_UPLOAD_ERROR(thread_id, "Failed to open offset file %s", offset_file);
      return;
    }
    //if resume flag is not set then no need to seek to offset.
    if(!resume_flag) 
      return;

    char off_string[64+1] = "";
    if(fgets(off_string, 64, fp))  {
      offset = atol(off_string); 
 
      //Setting offset
      if(fseek(csv_table->fp, offset, SEEK_SET))
      {
        DB_UPLOAD_ERROR(thread_id, "Failed to set file offset to %lu", offset);
        return;
      }
      DB_UPLOAD_DEBUG3(thread_id, "File will be processed from %lu", offset);
    }
  }
  else
  {
    DB_UPLOAD_DEBUG3(thread_id, "Offset file not found for csv file %s, creating offset file", csv_file_name);
    fp = fopen(offset_file, "w");
    csv_table->off_fp = fp; 
    if(!fp) {
      DB_UPLOAD_ERROR(thread_id, "Failed to create offset file %s\n", offset_file); 
      return;
    }
    chown(offset_file, ownerid, grpid);
    DB_UPLOAD_DEBUG3(thread_id, "Setting offset 0 to offset file %s for csv file %s", offset_file, csv_file_name); 
    fprintf(fp, "%s", "0;");
    fflush(fp);
  }
}

#define RECONNECT_DB(con, conn_status, db_pid)  {\
  PQreset(con);  	\
  if(PQstatus(con) != CONNECTION_OK) {\
    DB_UPLOAD_ERROR(thread_id, "Failed to reconnect to db, will try after %d sec", ideal_time);    \
    conn_status = -2;  	\
  }   \
  else   {\
    conn_status = 0; 	\
    db_pid = PQbackendPID(con); \
  }  \
}

static int check_csv_bit_for_retry(char *csv_file)
{                                     
  if(!strcmp(csv_file, "urc.csv"))                           
  {
    if(csv_complete_flag & URC_CSV_BIT)                  
      return 1;
    else
      return 0;
  }
  else if(!strcmp(csv_file, "src.csv"))                        
  {
    if(csv_complete_flag & SRC_CSV_BIT)
      return 1;
    else
      return 0;
  }
  else if(!strcmp(csv_file, "tprc.csv"))                     
  {
    if(csv_complete_flag & TPRC_CSV_BIT)
      return 1;
    else
      return 0;
  }
  else if(!strcmp(csv_file, "trc.csv"))                     
  {
    if(csv_complete_flag & TRC_CSV_BIT)           
      return 1;
    else
      return 0;
  }
  else if(!strcmp(csv_file, "prc.csv"))                       
  {
    if(csv_complete_flag & PRC_CSV_BIT)                   
      return 1;
    else
      return 0;
  }
  else if(!strcmp(csv_file, "urt.csv"))                       
  {
    return 0;
  }
  else
    return 2;
}

#define URC_RESET_BIT	0x80000000
#define SRC_RESET_BIT	0x40000000
#define TPRC_RESET_BIT	0x20000000
#define TRC_RESET_BIT	0x10000000
#define PRC_RESET_BIT	0x08000000
//#define URT_RESET_BIT	0x04000000

static int check_lsb_csv_bit(char *csv_file)
{                                     
  if(!strcmp(csv_file, "urc.csv"))                           
  {
    if(csv_complete_flag & URC_RESET_BIT)                  
      return 1;
    else
      return 0;
  }
  else if(!strcmp(csv_file, "src.csv"))                        
  {
    if(csv_complete_flag & SRC_RESET_BIT)
      return 1;
    else
      return 0;
  }
  else if(!strcmp(csv_file, "tprc.csv"))                     
  {
    if(csv_complete_flag & TPRC_RESET_BIT)
      return 1;
    else
      return 0;
  }
  else if(!strcmp(csv_file, "trc.csv"))                     
  {
    if(csv_complete_flag & TRC_RESET_BIT)           
      return 1;
    else
      return 0;
  }
  else if(!strcmp(csv_file, "prc.csv"))                       
  {
    if(csv_complete_flag & PRC_RESET_BIT)                   
      return 1;
    else
      return 0;
  }
  else
    return 0;
}

static void reset_lsb_bit(char *csv_file)
{
  if(!strcmp(csv_file, "urc.csv"))                           
  {
    csv_complete_flag &= ~URC_RESET_BIT;                  
  }
  else if(!strcmp(csv_file, "src.csv"))                        
  {
    csv_complete_flag &= ~SRC_RESET_BIT;
  }
  else if(!strcmp(csv_file, "tprc.csv"))                     
  {
    csv_complete_flag &= ~TPRC_RESET_BIT;
  }
  else if(!strcmp(csv_file, "trc.csv"))                     
  {
    csv_complete_flag &= ~TRC_RESET_BIT;       
  }
  else if(!strcmp(csv_file, "prc.csv"))                       
  {
    csv_complete_flag &= ~PRC_RESET_BIT;                   
  }
  else
    return;

}

 
static inline void upload_data(int thread_id, char *csv_file)
{
  int file_fd;
  db_conn_table db_con = {NULL, "\0", "\0", -1}; 
  int ret;
  char *read_buff = NULL;
  long long  used_len = 0;
  int conn_status = 0;
  int db_pid;
  /*as is_test_running will set to 0 we need to test input file for 1 more time, so we using this flag, 
    it will be set when is_test_running will set 0 for first time */
  char last_iteration =  0;
  int return_val = 0;

  DB_UPLOAD_DEBUG2(thread_id, "Method called,  csv file = %s", csv_file);

  csv_file_info csv_table;
  memset(&csv_table, 0, sizeof(csv_file_info));
  
  LOG_COMMENTRY("Thread[%d] Opening csv files and database connection\n", thread_id);

  DB_UPLOAD_DEBUG3(thread_id, "Creating db connection");

  ret = ns_db_create_db_con (&db_con, csv_file);

  if(ret != 0){ // failed to create connection
    DB_UPLOAD_ERROR(thread_id, "Failed to create db connection");
    NS_EXIT(-1, "Failed to create db connection");
  }

  DB_UPLOAD_DEBUG3(thread_id, "db connection created");

  //set db_pid.
  db_pid = PQbackendPID(db_con.conn);

  sprintf(csv_table.file_path, "%s/webapps/logs/TR%d/%s/reports/csv/%s" , ns_wdir, tr_num, 
                                (db_file_table[db_con.csv_file_idx].partition_md_flag)?nde_md_directory:partition_name, 
                                csv_file);

  file_fd = open(csv_table.file_path, O_RDONLY | O_LARGEFILE | O_CLOEXEC);

  if(file_fd == -1)
  {
    DB_UPLOAD_ERROR(thread_id, "Unable to open file %s. ERROR: %s", 
                    csv_table.file_path, nslb_strerror(errno));
    csv_table.fp = NULL; 
  }
  else
  {
    csv_table.fp = fdopen(file_fd, "r");
    if(csv_table.fp == NULL){
      DB_UPLOAD_ERROR(thread_id, "Filed in fdopen file %s. ERROR: %s", 
                      csv_table.file_path, nslb_strerror(errno));
    }
    else
    { 
      check_and_set_offset(thread_id, csv_file, &csv_table, db_file_table[db_con.csv_file_idx].partition_md_flag);
      csv_table.comm_count = commentry_interval;
    }
  }
 
  // Alloctae read buff(one extra for null character) 
  read_buff = malloc(chunk_size + 1);

  if(!read_buff && chunk_size > 10 * 1024 * 1024)
  {
    chunk_size = 10 * 1024 * 1024;
    read_buff = malloc(chunk_size + 1);
  }

  if(!read_buff)
  {
    DB_UPLOAD_ERROR(thread_id, "Failed to malloc memory chunk [size = %lld], try again with less chunk size using --chunk_size option ", chunk_size);
    NS_EXIT(-1, "ThreadID:%d Failed to malloc memory chunk [size = %lld], try again with less chunk size using --chunk_size option \n", thread_id, chunk_size);
  }
  
  time_t last_processing_time = 0;
  time_t start_time; 
  int csv_processing_flag = 0;

  while(1){

    if(partition_name[0] != '\0')
    {
      return_val = check_lsb_csv_bit(csv_file);
      if(return_val)
      {
        //Close old file pointer
        if(csv_table.fp)
          fclose(csv_table.fp);
        if(csv_table.partial_buf)
          csv_table.partial_buf = NULL;

        if(csv_table.partial_buf_len)
          csv_table.partial_buf_len = 0;

        if(csv_table.max_partial_buf_len)
          csv_table.max_partial_buf_len = 0;

        /* Opening new file as bit is set so we will have the new partition to process. 
         * Once all the bits are set this will open file for new partition. */
        ret = ns_db_create_db_con (&db_con, csv_file);

        if(ret != 0){ // failed to create connection
          DB_UPLOAD_ERROR(thread_id, "Failed to create db connection");
          NS_EXIT(-1, "Failed to create db connection");
        }

        DB_UPLOAD_DEBUG3(thread_id, "db connection created");
        //set db_pid.
        db_pid = PQbackendPID(db_con.conn);

        sprintf(csv_table.file_path, "%s/webapps/logs/TR%d/%s/reports/csv/%s" , ns_wdir, tr_num, 
                                    (db_file_table[db_con.csv_file_idx].partition_md_flag)?nde_md_directory:partition_name, csv_file);

        file_fd = open(csv_table.file_path, O_RDONLY | O_LARGEFILE | O_CLOEXEC);

        if(file_fd == -1)
        {
          DB_UPLOAD_ERROR(thread_id, "Unable to open file %s. ERROR: %s", 
                         csv_table.file_path, nslb_strerror(errno));
          csv_table.fp = NULL; 
          NS_EXIT(-1, "Unable to open file %s. ERROR: %s",
                         csv_table.file_path, nslb_strerror(errno));
        }
        else
        {
          csv_table.fp = fdopen(file_fd, "r");
          if(csv_table.fp == NULL)
          {
            DB_UPLOAD_ERROR(thread_id, "Failed in fdopen file %s. ERROR: %s", 
                           csv_table.file_path, nslb_strerror(errno));
            NS_EXIT(-1, "Failed in fdopen file %s. ERROR: %s",
                           csv_table.file_path, nslb_strerror(errno));
          }
          else
          { 
            if(csv_table.off_fp)
            {
              fclose(csv_table.off_fp);
            }

            check_and_set_offset(thread_id, csv_file, &csv_table, db_file_table[db_con.csv_file_idx].partition_md_flag);
          }
        }
        reset_lsb_bit(csv_file);
      } 
    }

    start_time = time(NULL);

    if(conn_status == 0)
    { 
      if(csv_table.fp)
      { 
        if(partition_name[0] != '\0')
        {
          return_val = check_csv_bit_for_retry(csv_file);
          if(!return_val)
          {
	    conn_status = read_and_copy_into_db(thread_id, &db_con, &csv_table,
                                                read_buff, &used_len, 
                                                &chunk_size, csv_file, db_pid);
          }
        }
        else
        {
	  conn_status = read_and_copy_into_db(thread_id, &db_con, &csv_table,
                                              read_buff, &used_len, 
                                              &chunk_size, csv_file, db_pid);

	  LOG_COMMENTRY("Thread[%d]:File=%s ,%ld Bytes data processed\n", thread_id, 
                        csv_table.file_path, (ftell(csv_table.fp) - csv_table.partial_buf_len));
        }
      }
    }
  
    last_processing_time = time(NULL) - start_time;
    DB_UPLOAD_DEBUG3(thread_id, "Processing time = %ld", last_processing_time);

   if(ideal_time > last_processing_time) 
     sleep(ideal_time - last_processing_time);

    if(is_test_running == 0 && last_iteration)
    {
      LOG_COMMENTRY("Thread[%d] Parent process stopped, closing upload thread\n", thread_id); 
      DB_UPLOAD_DEBUG2(thread_id, "Test run stopped, stop upload csv data");

      if(strcmp(csv_file, "urt.csv"))
      {
        return_val = check_csv_bit_for_retry(csv_file);
        if(!return_val)
        {
          csv_processing_flag = 1;
          csv_table.wait_interval += 5; 
          if(csv_table.wait_interval >= wait_interval_in_secs)
          {
            set_csv_bits(csv_file);
            csv_processing_flag = 0;
          }
        }
      }

      if(csv_processing_flag)
        csv_processing_flag = 0;
      else
      {
        break;
      }
    }
    if(is_test_running == 0) last_iteration = 1;
    if(conn_status == -2)
      RECONNECT_DB(db_con.conn, conn_status, db_pid);
  }


  /* Print summary report */
  if(summary_flag)
  {
    struct stat file_stat;
    long file_size = 0;
    char perc_string[32] = "";
    if(stat(csv_table.file_path, &file_stat))
    {
      DB_UPLOAD_ERROR(thread_id, "Failed to get file size of csv file %s", csv_table.file_path);
      file_size = 0;
      *perc_string = 0;
    }
    else
    {
      file_size = file_stat.st_size;
      if(file_size)
        sprintf(perc_string, "[%.2f%%]", ((double)((double)ftell(csv_table.fp) * 100) / (double)file_size));
      else
        sprintf(perc_string, "[100.00%%]");
    }      
     
    LOG_COMMENTRY("Thread[%d]: csv file = %s, total bytes processed = %ld%s\n", thread_id, 
          csv_file, ftell(csv_table.fp), perc_string);
    LOG_COMMENTRY("Thread[%d]:Table name = %s Rows copied = %ld\n",thread_id, db_con.table, 
                  get_row_count_of_table(db_con.conn, db_con.table));    
  }
  //? What's about partial data
  free(read_buff);
  read_buff = NULL;
  char offset_file_path[MAX_FILE_NAME] = ""; 
  //set offset to 0.
  if(csv_table.off_fp)
  {
    //remove offset files.
    sprintf(offset_file_path, "%s/webapps/logs/TR%d/%s/reports/csv/.%s.offset" , ns_wdir, tr_num, 
                               (db_file_table[db_con.csv_file_idx].partition_md_flag)?nde_md_directory:partition_name, 
                               csv_file);

    if(partition_name[0] == 0)
      unlink(offset_file_path);
    fclose(csv_table.off_fp);
  }
  if(csv_table.fp)
    fclose(csv_table.fp);
  if(csv_table.partial_buf)
    free(csv_table.partial_buf);
  //disconncet db connection
  if(db_con.conn){
    DB_UPLOAD_DEBUG2(thread_id, "Closing db connection");
    PQfinish(db_con.conn);
    db_con.conn = NULL;
  }
}

#define SINGLE_THREADED_CSV_COUNT 11
typedef struct STCsvInfo
{
  char *csv_name;
  char partition_md_flag;
  char *table_name;
}STCsvInfo;
static STCsvInfo st_csv_info[SINGLE_THREADED_CSV_COUNT] = {  {"ect.csv" , 1, "ErrorCodes_"},
                                                      {"hat.csv", 1, "ActualServerTable_"},
                                                      {"hrt.csv", 1, "RecordedServerTable_"},
                                                      {"log_phase_table.csv", 1, "LogPhaseTable_"},
                                                      {"pgt.csv", 1, "PageTable_"},
                                                      {"rpf.csv", 1, "RunProfile_"},
                                                      {"spf.csv", 1, "SessionProfile_"},
                                                      {"sst.csv", 1, "SessionTable_"},
                                                      {"trt.csv", 1, "TransactionTable_"},
                                                      {"upf.csv", 1, "UserProfile_"},
                                                      {"generator_table.csv", 1, "GeneratorTable_"} }; 


void upload_meta_data(thread_id)
{
  int i;
  csv_file_info csv_table[SINGLE_THREADED_CSV_COUNT];
  char *read_buff[SINGLE_THREADED_CSV_COUNT] = {NULL};
  long long used_len[SINGLE_THREADED_CSV_COUNT] = {0}; 
  int file_fd;
  int ret;
  int conn_status = 0;
  int db_pid;
  int metadata_alloc_size;
  /*as is_test_running will set to 0(test will close) we need to test input file for 1 more time, so we using this flag, 
    it will be set when is_test_running will set 0 for first time */
  char last_iteration = 0;
  db_conn_table db_con = {NULL, "\0", "\0", -1}; 
  
  DB_UPLOAD_DEBUG2(thread_id, "Method called");
  LOG_COMMENTRY("Thread[%d]:Opening csv files and making data connection\n", thread_id);

  DB_UPLOAD_DEBUG2(thread_id, "Creating db connection");
  //create db connection.
  ret = ns_db_create_db_con (&db_con, st_csv_info[0].csv_name);
  if(ret != 0){ // failed to create connection
    DB_UPLOAD_ERROR(thread_id, "Failed to create db connection");
    NS_EXIT(-1, "Failed to create db connection");
  }
  DB_UPLOAD_DEBUG2(thread_id, "db connection created");
  
  //set db_pid.
  db_pid = PQbackendPID(db_con.conn);

  //Fill csv_table
  for(i = 0; i < SINGLE_THREADED_CSV_COUNT; i++)
  {
    memset(&csv_table[i], 0, sizeof(csv_file_info)); 
    /*Open csv file*/ 
    sprintf(csv_table[i].file_path, "%s/webapps/logs/TR%d/%s/reports/csv/%s" , ns_wdir, tr_num, 
                                     (st_csv_info[i].partition_md_flag)?nde_md_directory:partition_name, 
                                     st_csv_info[i].csv_name);

    DB_UPLOAD_DEBUG3(thread_id, "csv file = %s, csv file path = %s", st_csv_info[i].csv_name, 
                                csv_table[i].file_path);

    file_fd = open(csv_table[i].file_path, O_RDONLY | O_LARGEFILE | O_CLOEXEC);

    if(file_fd == -1){
      DB_UPLOAD_ERROR(thread_id, "Unable to open file %s. ERROR: %s", csv_table[i].file_path, 
                                 nslb_strerror(errno));

      csv_table[i].fp = NULL;
    }
    else
    {
    csv_table[i].fp = fdopen(file_fd, "r");
    if(csv_table[i].fp == NULL){
      DB_UPLOAD_ERROR(thread_id, "Failed in fdopen file %s. ERROR: %s", csv_table[i].file_path, 
                      nslb_strerror(errno));
    }
    check_and_set_offset(thread_id, st_csv_info[i].csv_name, &csv_table[i], st_csv_info[i].partition_md_flag);
    csv_table[i].comm_count = commentry_interval; 
  //Allocate read buf.
    }
    if(chunk_size > 1024 * 1024)
      metadata_alloc_size = 1024 * 1024;
    else 
      metadata_alloc_size = chunk_size;

    //one extra for null character
    read_buff[i] = malloc(metadata_alloc_size + 1);

    if(!read_buff[i])
    {
      DB_UPLOAD_ERROR(thread_id, "Failed to malloc memory chunk for metadata table");
      NS_EXIT(-1, "ThreadID:%d Failed to malloc memory chunk for metadata table", thread_id);
    }
  }
 
  //Now start process to read csv and upload to database.
  time_t last_processing_time = 0;
  time_t start_time;
  DB_UPLOAD_DEBUG2(thread_id, "Going in loop to read and upload csv data"); 
  while(1)
  {
    start_time = time(NULL);
    
    //Upload csv data to process.
    if(conn_status == 0) {
      for(i = 0; i < SINGLE_THREADED_CSV_COUNT; i++)
      {
	DB_UPLOAD_DEBUG3(thread_id, "Checking csv = %s", st_csv_info[i].csv_name);
	//Change table name.
        sprintf(db_con.table, "%s%d", st_csv_info[i].table_name, tr_num);
        if(csv_table[i].fp)
        {
          DB_UPLOAD_DEBUG3(thread_id, "Checking csv file = %s", st_csv_info[i].csv_name);
          conn_status = read_and_copy_into_db(thread_id, &db_con, &csv_table[i], read_buff[i], 
                                              &used_len[i], &chunk_size,
                                              st_csv_info[i].csv_name, db_pid);
          if(conn_status == -2)
            break;

          LOG_COMMENTRY("Thread[%d]:File=%s, %ld Bytes data processed\n", thread_id, 
                          csv_table[i].file_path, 
                          (ftell(csv_table[i].fp) - csv_table[i].partial_buf_len));
        }  
      }
    }
    
    last_processing_time = time(NULL) - start_time;
    DB_UPLOAD_DEBUG3(thread_id, "Processing time = %ld", last_processing_time);

    if(ideal_time > last_processing_time) 
      sleep(ideal_time - last_processing_time);

    if(is_test_running == 0 && last_iteration) {
      LOG_COMMENTRY("Thread[%d]:Parent process stopped, closing data upload thread\n", thread_id); 
      DB_UPLOAD_DEBUG2(thread_id, "Test run stopped, stop upload csv data");
      break; 
    }
    if(is_test_running == 0) last_iteration = 1; 
    //Check for connection, If close then try to reconnect.
    if(conn_status == -2)
      RECONNECT_DB(db_con.conn, conn_status, db_pid); 
  }
  /* Print summary report */
  if(summary_flag)
  {
    struct stat file_stat;
    long file_size;
    char perc_string[32]  = "";
    for(i = 0; i < SINGLE_THREADED_CSV_COUNT; i++) {
      sprintf(db_con.table, "%s%d", st_csv_info[i].table_name, tr_num);
      if(stat(csv_table[i].file_path, &file_stat))
      {
        DB_UPLOAD_ERROR(thread_id, "Failed to get file size of csv file %s", csv_table[i].file_path);
        file_size = 0;
        *perc_string = 0;
      }
      else
      {
        file_size = file_stat.st_size;
        if(file_size)
          sprintf(perc_string, "[%.2f%%]", ((double)((double)ftell(csv_table[i].fp) * 100) / (double)file_size));
        else
          sprintf(perc_string, "[100.00%%]");
      }      
      LOG_COMMENTRY("Thread[%d]:csv file = %s, total bytes processed = %ld%s\n", thread_id, 
                    st_csv_info[i].csv_name,
                    ftell(csv_table[i].fp), perc_string); 
    }
    LOG_COMMENTRY("Thread[%d]:Table name =%s Rows copied =%ld\n", thread_id, db_con.table, 
                  get_row_count_of_table(db_con.conn, db_con.table));    
  }

  char offset_file_path[MAX_FILE_NAME] = "";
  //Free malloced data.
  for(i = 0; i < SINGLE_THREADED_CSV_COUNT; i++)
  {
    //reset offset file.
    if(csv_table[i].off_fp)
    {
      sprintf(offset_file_path, "%s/webapps/logs/TR%d/%s/reports/csv/.%s.offset" , ns_wdir, tr_num, 
                                 (st_csv_info[i].partition_md_flag)?nde_md_directory:partition_name, 
                                 st_csv_info[i].csv_name);

      if(partition_name[0] == 0)
        unlink(offset_file_path);
      fclose(csv_table[i].off_fp);
    }

    if(csv_table[i].fp)
      fclose(csv_table[i].fp);
    if(csv_table[i].partial_buf)
      free(csv_table[i].partial_buf);

    free(read_buff[i]);
    read_buff[i] = NULL;
  }
  //disconncet db connection
  if(db_con.conn){
    DB_UPLOAD_DEBUG2(thread_id, "Closing db connection");
    PQfinish(db_con.conn);
    db_con.conn = NULL;
  }
}

static void *ns_upload_db(void *args)
{
  int thread_id =(*(int *)args);
  DB_UPLOAD_DEBUG2(thread_id, "method called");

  switch (thread_id){
    case 0:
//              break;
      LOG_COMMENTRY("Thread[%d] started for csv files: ect.csv, hat.csv, hrt.csv, log_phase_table.csv, pgt.csv, rpf.csv, spf.csv, sst.csv, trt.csv, upf.csv\n", thread_id);
      upload_meta_data(thread_id); 
      break;
    case 1:
      LOG_COMMENTRY("Thread[%d] started for urc.csv\n", thread_id);
      upload_data(thread_id, "urc.csv");
      break;
    case 2:
      LOG_COMMENTRY("Thread[%d] started for src.csv\n", thread_id);
      upload_data(thread_id, "src.csv");
      break;
    case 3:
      LOG_COMMENTRY("Thread[%d] started for tprc.csv\n", thread_id);
      upload_data(thread_id, "tprc.csv");
      break;
    case 4:
      LOG_COMMENTRY("Thread[%d] started for trc.csv\n", thread_id);
      upload_data(thread_id, "trc.csv");
      break;
    case 5:
      LOG_COMMENTRY("Thread[%d] started for prc.csv\n", thread_id);
      upload_data(thread_id, "prc.csv");
      break;
    case 6:
      LOG_COMMENTRY("Thread[%d] started for urt.csv\n", thread_id);
      upload_data(thread_id, "urt.csv");
      break;
    default:
      DB_UPLOAD_ERROR(thread_id, "Some error occured!");
      break;
  }
  return NULL; 
}


#define NUM_THREADS 7

int sigrtmin2 = 0;
int sigalarm = 0;
void sigrtmin2_handler(int sig) {
  sigrtmin2 = 1;
}
void sigalarm_handler (int sig) {
    sigalarm = 1;
}

void do_sigalarm() {
  if(is_test_running){
    if(process_mode == DB_CHILD_MODE) {
      if(parent_pid != -1 && nslb_check_pid_alive(parent_pid))
        is_test_running = 1;
      else {
        is_test_running = 0; 
        DB_UPLOAD_DEBUG2(0, "ndu_process_data have been stoped, stop uploading to db");
      }
    }
    else
      is_test_running = nslb_chk_is_test_running(tr_num);
  }
}

#define MAX_KEYWORD_LINE_LEN 2048
static int check_and_set_partition(char *ns_wdir, int trnum)
{
  char partition_dir[MAX_KEYWORD_LINE_LEN];
  FILE *fp = NULL;
  int num_fields = 0;
  char tmp_buf[MAX_KEYWORD_LINE_LEN];
  char *fields[5];
  char db_complete_path[MAX_KEYWORD_LINE_LEN];
  struct stat buf_stats;

  while(1)
  {
    sprintf(db_complete_path, "%s/logs/TR%d/%s/reports/csv/.nsu_db_complete", 
                                   ns_wdir, tr_num, partition_name);

    if((stat(db_complete_path, &buf_stats)) == 0)
      break;

    
    sprintf(partition_dir, "%s/logs/TR%d/%s/.partition_info.txt", ns_wdir, trnum, partition_name);

    fp = fopen(partition_dir, "r");

    if(!fp) {
      fprintf(stderr, "\nUnable to open file = [%s]. ERROR : %s\n", partition_dir, nslb_strerror(errno));
      break;
    }

    while(fgets(tmp_buf, MAX_KEYWORD_LINE_LEN, fp))
      if(tmp_buf[0] != '#')
        break;

    num_fields = get_tokens_with_multi_delimiter(tmp_buf, fields, ",", 5);
    if(num_fields < 2)
      break;

    if(*fields[1] != '0')     
      strcpy(partition_name, fields[1]);  
    else
      break;
  }
}

int main(int argc, char *argv[]){

  time_t start_time, end_time;
  char cur_time_buf[100];

  if (getenv("NS_WDIR") != NULL)
    strcpy(ns_wdir, getenv("NS_WDIR"));
  else
  {
    NS_EXIT(-1, "NS_WDIR env variable is not set.");
  }

  /* Parse command line argumnets */
  nsu_parse_args(argc, argv);
  DB_UPLOAD_DEBUG1(0, "tr_num %d, ideal_time = %d, chunk_size = %d, parent_pid = %d", 
                           tr_num, ideal_time, chunk_size, parent_pid);
 
  read_owner_group(); 
 
  if(partition_name[0] != '\0' && resume_flag)
  {
    check_and_set_partition(ns_wdir, tr_num);
  }

  /*Set error_fp*/
  char file_path[1024] = "";
  sprintf(file_path, "%s/logs/TR%d/%s/nsu_db.error", getenv("NS_WDIR"), tr_num, partition_name);
  error_fp = fopen(file_path, "w");
  if(error_fp)
    chown(file_path, ownerid, grpid);

#if DB_DEBUG_ON
  /*Set debug file */
  if(debug_level > 0) {
    sprintf(file_path, "%s/logs/TR%d/%s/nsu_db.log", getenv("NS_WDIR"), tr_num, partition_name);
    if((debug_fp = fopen(file_path, "w")) == NULL)
    {
      debug_level = 0;   
    }
    else
      chown(file_path, ownerid, grpid);
  }
#endif

  /*Open commentry file  */
  if(commentry_interval)
  {
    sprintf(file_path, "%s/logs/TR%d/%s/nsu_db_prgs.rpt", ns_wdir, tr_num, partition_name);
    if((commentry_fp = fopen(file_path, "w")) == NULL)
    {
      DB_UPLOAD_ERROR(0, "Failed to open commentry file, commentry(progress report) will be disabled");
      commentry_interval = 0;
      summary_flag = 0;
    }
    else
      chown(file_path, ownerid, grpid);
  }

  /* If parent pid in non zero, it means process has been spawned from NS reader */
  if(parent_pid)
  {
    process_mode = DB_CHILD_MODE;  
  }
  else
    summary_flag = 1; 
  if( -1 == chk_postgresql_is_running())
  {
    DB_UPLOAD_ERROR(0, "Database is not running, data can not be uploaded to db\n");
    NS_EXIT(-1, "Postgresql is not running, please start postgresql and re-run the test.\nDatabase is not running, data can not be uploaded to db");
  }
 
  char cmdbuf[256] = "\0";
  int ret ;
  char cmd_out[1024] = "";
  long long overlap_time = 0;
  char partition_dir_name_loc[1024] = "";
  long long start_time_in_secs = 0;

  if(running_mode >= 1){

    //Handling Logging reader recovery
    if(is_first_test_run == 0)
    {
      PGresult *res;
      char cmd[1024];
      const char *conninfo;
      int num_tables = 0;
      PGconn *dbconnection;

      conninfo = "dbname=test user=netstorm";

      dbconnection = PQconnectdb(conninfo);
      if (PQstatus(dbconnection) != CONNECTION_OK)
      {
        fprintf(stderr, "\nConnection to database failed: %s\n", PQerrorMessage(dbconnection));
        if(dbconnection)
         PQfinish(dbconnection);
        dbconnection = NULL;
        return -1;
      }

      sprintf(cmd, "select * from information_schema.tables where  table_name ~ \'urlrecord_%d$\';", tr_num);

      res = PQexec(dbconnection, cmd);
      if (PQresultStatus(res) != PGRES_TUPLES_OK)
      {
        PQclear(res);
        if(dbconnection)
          PQfinish(dbconnection);
        dbconnection = NULL;
        return 1;
      }

      num_tables = PQntuples(res);
      if(num_tables == 0)
      {
        is_first_test_run = 1;
      }
    }

    /* If db upload is resuming then no need to create tables */
    if((!resume_flag && flag_create_table) || (is_first_test_run && partition_name[0] != 0)) {
      sprintf(cmdbuf, "%s/bin/nsu_create_table %d > /dev/null 2>&1", ns_wdir, tr_num);
      ret = nslb_run_cmd_and_get_last_line(cmdbuf, 1024, cmd_out);
      if(ret)
      {
        DB_UPLOAD_ERROR(0, "Failed to create tables, ERROR executing command: '%s'\n", cmdbuf);
        NS_EXIT(-1, "Failed to create tables, ERROR executing command: '%s'\n", cmdbuf);
      }
      DB_UPLOAD_DEBUG2(0, "NS Tables successfully created");
    }
 
    if(!resume_flag || (is_first_test_run && partition_name[0] != 0))
    {
      sprintf(cmdbuf, "%s/bin/nsu_create_index %d > /dev/null 2>&1", ns_wdir, tr_num);
      ret = nslb_run_cmd_and_get_last_line(cmdbuf, 1024, cmd_out);
      if(ret)
      {
        DB_UPLOAD_ERROR(0, "Failed to create indexes, ERROR executing command: '%s'\n", cmdbuf);
        NS_EXIT(-1, "Failed to create indexes, ERROR executing command: '%s'\n", cmdbuf);
      }
      DB_UPLOAD_DEBUG2(0, "NS index successfully created");
 
    }

    if(partition_name[0] != 0 && create_partition_table)
    {
      if(debug_level > 0) {
        sprintf(cmdbuf, "%s/bin/neu_ns_create_partition_table %d %s >> %s 2>&1", ns_wdir, tr_num, partition_name, file_path);
      }
      else
      {
        sprintf(cmdbuf, "%s/bin/neu_ns_create_partition_table %d %s >/dev/null 2>&1", ns_wdir, tr_num, partition_name);
      }

      ret = nslb_run_cmd_and_get_last_line(cmdbuf, 1024, cmd_out);
      if(ret)
      {
        DB_UPLOAD_ERROR(0, "Failed to create partition tables,ERROR executing command: '%s'\n", cmdbuf);
        NS_EXIT(-1, "Failed to create partition tables,ERROR executing command: '%s'\n", cmdbuf);
      }
      else
      {
        DB_UPLOAD_DEBUG2(0, "NS Partition Tables successfully created. Now going to add check constraint on it.");
      
        sprintf(partition_dir_name_loc, "%s00", partition_name);
        start_time_in_secs = nslb_get_time_in_secs(partition_dir_name_loc);

        if(cav_epoch_diff)
        {
          start_time_in_secs = start_time_in_secs - cav_epoch_diff;
        }

        overlap_time = get_overlap_time_in_secs(ns_wdir, tr_num);
        overall_start_time_in_secs = start_time_in_secs - overlap_time; 
        if(overall_start_time_in_secs < 0)
          overall_start_time_in_secs = 0;

        if(debug_level > 0) {
          sprintf(cmdbuf, "%s/bin/neu_ns_create_check_constraint -TR %d -PT %s -ST %lld >> %s 2>&1", ns_wdir, tr_num, partition_name, overall_start_time_in_secs, file_path);
        }
        else
        {
          sprintf(cmdbuf, "%s/bin/neu_ns_create_check_constraint -TR %d -PT %s -ST %lld >/dev/null 2>&1", ns_wdir, tr_num, partition_name, overall_start_time_in_secs);
        }

        ret = nslb_run_cmd_and_get_last_line(cmdbuf, 1024, cmd_out);
        if(ret)
        {
          DB_UPLOAD_ERROR(0, "Failed to create partition tables,ERROR executing command: '%s'\n", cmdbuf);
          NS_EXIT(-1, "Failed to create partition tables,ERROR executing command: '%s'\n", cmdbuf);
        }
      }
    }
  }

  start_time = time(NULL); 
  /*Add commentry header*/
  if(commentry_interval || summary_flag)
  {
    LOG_COMMENTRY("DataBase uploading start at %s\n", nslb_get_cur_date_time(cur_time_buf, 0));
  }

  DB_UPLOAD_DEBUG1(0, "Starting db threads");
  pthread_t *thread = NULL;
  pthread_attr_t attr;
  int threadargs[NUM_THREADS] = {0};
  int t;
  thread = (pthread_t *) malloc( NUM_THREADS * sizeof(pthread_t));
  pthread_attr_init(&attr);

  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIGALRM);
  sigaddset(&set, SIGRTMIN+2);
  pthread_sigmask(SIG_BLOCK, &set, NULL);
  for(t = 0; t < NUM_THREADS; t++){
    threadargs[t] = t;
    DB_UPLOAD_DEBUG2(0, "Creating thread %d", threadargs[t]);
    ret = pthread_create(&thread[t], &attr, ns_upload_db, (void *)&threadargs[t]);      
    if (ret)
    {
      DB_UPLOAD_ERROR(0, "ERROR; return code from pthread_create() is %d", ret);
      NS_EXIT(-1, "ERROR; return code from pthread_create() is %d", ret);
    }
  }
  pthread_attr_destroy(&attr);

  struct sigaction sa;
  sigset_t sigset;

  bzero (&sa, sizeof(struct sigaction));
  sa.sa_handler = sigalarm_handler;
  sigaction(SIGALRM, &sa, NULL);

  bzero (&sa, sizeof(struct sigaction));
  sa.sa_handler = sigrtmin2_handler;
  sigaction(SIGRTMIN+2, &sa, NULL);

  alarm(1);
  sigfillset (&sigset);
  sigprocmask (SIG_SETMASK, &sigset, NULL);

  char db_complete_path_buf[2048]="";
  FILE *db_complete_fp = NULL;
  struct stat buf_stats;
  char partition_dir[2048];
  char tmp_buf[2048];
  int num_fields = 0;
  char *fields[5];

  while(1){
     
    if (sigalarm)  {
      do_sigalarm();
      sigalarm = 0;
      alarm(1);
    }

    if (sigrtmin2) {
      sighandler_t prev_handler;
      prev_handler = signal(SIGRTMIN+2, SIG_IGN);
      //is_test_running = 0;
      (void) signal(SIGRTMIN+2, prev_handler);
      sigrtmin2 = 0;
    }
  
    if(partition_name[0] != '\0' && resume_flag)
    {
      if(csv_complete_flag == NS_CSV_BIT_MASK)
      {
        sprintf(db_complete_path_buf, "%s/logs/TR%d/%s/reports/csv/.nsu_db_complete", 
                                       ns_wdir, tr_num, partition_name);

        db_complete_fp = fopen(db_complete_path_buf, "w"); 
        if(db_complete_fp == NULL)
        {
          NS_EXIT(-1, "Unable to open file = %s. ERROR : %s", db_complete_path_buf, nslb_strerror(errno));
        }
        else
        {
          chown(db_complete_path_buf, ownerid, grpid);
          fclose(db_complete_fp);
        }

        if(get_and_set_endtime_constraint_on_partition_table(ns_wdir, tr_num, partition_name, 1, &end_time_in_secs, start_time_in_secs, overlap_time, debug_level, file_path) == -1)
        {
          DB_UPLOAD_ERROR(0, "Error : Failed to add the end time check constraint on the partition table.");
          NS_EXIT(-1, "Error : Failed to add the end time check constraint on the partition table.");
        }

        sprintf(partition_dir, "%s/logs/TR%d/%s/.partition_info.txt", ns_wdir, tr_num, partition_name);
  
        FILE *fp = fopen(partition_dir, "r");
        if(!fp)
        {
          NS_EXIT(-1, "Unable to open file = [%s]. ERROR : %s", partition_dir, nslb_strerror(errno));
        }
  
        while(fgets(tmp_buf, MAX_KEYWORD_LINE_LEN, fp))
        {
          if(tmp_buf[0] == '#')
            continue;
          
          num_fields = get_tokens_with_multi_delimiter(tmp_buf, fields, ",", 5);
          if(num_fields == 3)
          {
            strcpy(partition_name, fields[2]);  
            sprintf(file_path, "%s/logs/TR%d/%s/nsu_db.error", getenv("NS_WDIR"), tr_num, partition_name);
            error_fp = fopen(file_path, "w");
            if(error_fp)
              chown(file_path, ownerid, grpid);

            #if DB_DEBUG_ON
            /*Set debug file */
            if(debug_level > 0) {
              sprintf(file_path, "%s/logs/TR%d/%s/nsu_db.log", getenv("NS_WDIR"), tr_num, partition_name);
              if((debug_fp = fopen(file_path, "w")) == NULL)
              {
                debug_level = 0;   
              }
              else
                chown(file_path, ownerid, grpid);
            }
            #endif


            if(partition_name[0] != 0 && create_partition_table)
            {
              if(debug_level > 0) {
                sprintf(cmdbuf, "%s/bin/neu_ns_create_partition_table %d %s >> %s 2>&1", ns_wdir, tr_num, partition_name, file_path);
              }
              else
              {
                sprintf(cmdbuf, "%s/bin/neu_ns_create_partition_table %d %s >/dev/null 2>&1", ns_wdir, tr_num, partition_name);
              }

              ret = nslb_run_cmd_and_get_last_line(cmdbuf, 1024, cmd_out);
              if(ret)
              {
                DB_UPLOAD_ERROR(0, "Failed to create partition tables,ERROR executing command: '%s'\n", cmdbuf);
                NS_EXIT(-1, "Failed to create partition tables,ERROR executing command: '%s'\n", cmdbuf);
              }
              else
              {
                DB_UPLOAD_DEBUG2(0, "NS Partition Tables successfully created. Now going to add check constraint on it.");
      
                sprintf(partition_dir_name_loc, "%s00", partition_name);
                start_time_in_secs = nslb_get_time_in_secs(partition_dir_name_loc);

                if(cav_epoch_diff)
                {
                  start_time_in_secs = start_time_in_secs - cav_epoch_diff;
                }

                overlap_time = get_overlap_time_in_secs(ns_wdir, tr_num);
                overall_start_time_in_secs = start_time_in_secs - overlap_time; 
                if(overall_start_time_in_secs < 0)
                  overall_start_time_in_secs = 0;

                if(debug_level > 0) {
                  sprintf(cmdbuf, "%s/bin/neu_ns_create_check_constraint -TR %d -PT %s -ST %lld >> %s 2>&1", ns_wdir, tr_num, partition_name, overall_start_time_in_secs, file_path);
                }
                else
                {
                  sprintf(cmdbuf, "%s/bin/neu_ns_create_check_constraint -TR %d -PT %s -ST %lld >/dev/null 2>&1", ns_wdir, tr_num, partition_name, overall_start_time_in_secs);
                } 

                ret = nslb_run_cmd_and_get_last_line(cmdbuf, 1024, cmd_out);
                if(ret)
                {
                  DB_UPLOAD_ERROR(0, "Failed to create partition tables,ERROR executing command: '%s'\n", cmdbuf);
                  NS_EXIT(-1, "Failed to create partition tables,ERROR executing command: '%s'\n", cmdbuf);
                }
              }
            }
            csv_complete_flag = 0xF8000000;   
          }
        }
      }
    }

    if(!is_test_running)
      break;

    sigemptyset(&sigset);
    sigsuspend(&sigset);
  }
  
  for(t = 0; t < NUM_THREADS; t++)
  {
    ret = pthread_join(thread[t], NULL);
    if(ret)
    {
      DB_UPLOAD_ERROR(0, "Error code from pthread join for thread id[%d] = \'%d\'", t, ret);
      NS_EXIT(-1, "Error code from pthread join for thread id[%d] = \'%d\'", t, ret); 
    }
    DB_UPLOAD_DEBUG2(0, "Thread[%d] successfully joined", t);
  }

  if(partition_name[0] != '\0') 
  {
    if(csv_complete_flag == NS_CSV_BIT_MASK)
    {
      sprintf(db_complete_path_buf, "%s/logs/TR%d/%s/reports/csv/.nsu_db_complete", 
                                     ns_wdir, tr_num, partition_name);

      if((stat(db_complete_path_buf, &buf_stats)) != 0)
      {
        db_complete_fp = fopen(db_complete_path_buf, "w"); 
        if(db_complete_fp == NULL)
        {
          NS_EXIT(-1, "Unable to open file = (%s). ERROR : %s", db_complete_path_buf, nslb_strerror(errno));
        }
        else
          chown(db_complete_path_buf, ownerid, grpid);
      }
    }
  }

/*
  if(partition_name[0] != 0)
  {
    if(get_and_set_endtime_constraint_on_partition_table(ns_wdir, tr_num, partition_name, 1, &end_time_in_secs, start_time_in_secs, overlap_time, debug_level, file_path) == -1)
    {
      DB_UPLOAD_ERROR(0, "Error : Failed to add the end time check constraint on the partition table.");
      exit(-1);
    }
  }
*/

  end_time = time(NULL);
  if(commentry_interval || summary_flag)
  {
    LOG_COMMENTRY("Database uploading finished at %s\n", nslb_get_cur_date_time(cur_time_buf, 0));
    LOG_COMMENTRY("Total time taken = %ld(sec)\n", (end_time - start_time));
  }
  DB_UPLOAD_DEBUG1(0, "CSV Data successfully uploaded to tables"); 
  if(debug_fp)
    fclose(debug_fp);
  if(commentry_fp)
    fclose(commentry_fp);
  NS_EXIT(0, "Exiting after uploading data to CVS tables.");
}
