/******************************************************************
 * Name    : nsu_show_partition_detail.c
 * Author  : Kushal
 * Purpose : This file is to get list of all partitions of TestRun provided with other details from summary.top file
 *           in the following format:
 *           PARTITION NAME|STATE|DURATION|START DATE TIME|PARTITION SIZE|DB SIZE|NOTES(will add this in future)
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ftw.h>
#include <ctype.h>
#include "nslb_util.h"
#include "nslb_partition.h"
#include <dirent.h>
#include "ns_tr_duration.h"
#include <libpq-fe.h>
#include <time.h>

static long long total = 0;

static int TRNum;
static int partition_duration = 0;
static char mode[20] = {0};
static char notes[8192 + 1] = "NA";
static char is_start_partition[3 + 1] = "NA";
static char *search_keywords[100];
static int num_search_keywords = 0;
static long long min_duration = -1;
static long long max_duration = -1;
static int last_n_days = -1;
static int cur_time = -1;

long long dbsize;
const char *conninfo;
PGconn *dbconnection;

#define MAX_LINE_LENGTH 2048

static void Usage()
{
  printf("Usage :\n");
  printf("   -t  <TRnum> Show test run log of given test run number only.\n");
  printf("   -m  <state> Show all partition by state provided.\n");
  printf("       valid states : locked ,unlocked, archived\n");
  printf("   -k  <keyword>: search by keyword partition name or notes.\n");
  printf("   -d  <00:00:00-00:00:00>: Duration.\n");
  printf("   -s  <n>: last n days.\n");
  exit (-1);
}

static int check_mode(char *fields)
{
  if (strcmp(mode, "locked") == 0) {
    if (strcmp(fields, "R" ) == 0) {
      return 1;
    }
  } else if (strcmp(mode,"unlocked") == 0) {
    if (strcmp(fields, "W") == 0) {
      return 1;
    }
  } else if (strcmp(mode, "archived") == 0) {
    if ((strcmp(fields, "AW") == 0)
      || (strcmp(fields, "AR") == 0)) {
      return 1;
    }
  } else {
      Usage();
   }
  return 0;
}

static int search_text(char *partition) 
{
  int i = 0;
  
  for(i=0;i<num_search_keywords;i++)
  {
    if(strcasestr(partition, search_keywords[i]) || strcasestr(notes, search_keywords[i]))
      return 1;
  }
  return 0;
}

static inline void print_partition_notes(char *path) 
{
  char notes_path[1024] = {0};
  FILE *fp;

  strcpy(notes, "NA");
  sprintf(notes_path, "%s/user_notes/test_notes.txt", path);
  fp = fopen(notes_path, "r");

  if(fp != NULL) 
  {
    fgets(notes, 8192, fp);
    notes[strlen(notes) - 1] = '\0';
    fclose(fp);
  }
}

//this func will read summary.top present at path passed as argument, and fills the buffer passed in arguments accordingly.
int get_duration_from_summary_top(char *start_date_time, char *state, char *duration, char *path)
{
  char summary_top_content[MAX_LINE_LENGTH] = {0};
  char summary_top_path[1024] = {0};
  char rtg_path[1024] = {0};
  char testrun_gdf_path[1024] = {0};
  char *fields[16];
  int total_fields = 0;
  int test_strt_time = 0;
  FILE *fp = NULL;

  memset(fields, 0, sizeof(fields));

  sprintf(summary_top_path, "%s/summary.top", path);
  fp = fopen(summary_top_path, "r");
  if(!fp)
  {
    //printf("Error: Unable to open file %s, error is %s\n", summary_top_path, nslb_strerror(errno));
    strcpy(start_date_time, "NA");
    strcpy(state, "NA");
    strcpy(duration, "NA");
    return -1; //not able to open summary.top
  }

  if(fgets(summary_top_content, MAX_LINE_LENGTH, fp) != NULL) 
  {
    //summary.top format: Test Run|Scenario Name|Start Time|Report Summary|Page Dump|Report Progress|Report Detail|Report User|Report Fail|Report Page Break Down|WAN_ENV|REPORTING|Test Name|Test Mode|Run Time|Virtual Users
    total_fields = get_tokens_with_multi_delimiter(summary_top_content, fields, "|", 16); 
  }
 
  fclose(fp);
  fp = NULL;

  //check mode
  if(mode[0] != '\0')
  {
    if(check_mode(fields[13]) != 1) //if mode not matched then return
      return 1;
  }

  if(last_n_days >= 0) {/*Check for last days*/
    test_strt_time = get_summary_top_start_time_in_secs(fields[2]);
    if(test_strt_time + last_n_days < cur_time )
      return 1;
  }

  if(total_fields > 14)
  {
    strcpy(start_date_time, fields[2]);
    strcpy(state, fields[13]);
    strcpy(duration, fields[14]);
    partition_duration = local_get_time_from_format(fields[14]);
  
    if(strcmp(duration, "00:00:00") == 0)
    {
      sprintf(rtg_path, "%s/rtgMessage.dat", path);     
      sprintf(testrun_gdf_path, "%s/testrun.gdf", path);     
      partition_duration = get_actual_duration(summary_top_path, rtg_path, testrun_gdf_path, 0);
      strcpy(duration, format_time(partition_duration));
    }
    return 0;
  }
  else  //summary.top is wrong
  {
    strcpy(start_date_time, "NA");
    strcpy(state, "NA");
    strcpy(duration, "NA");
    return -1;
  }
  //printf("Error: Incorrect %s summary.top . Found '%d' fields in summary.top %s, expected fields is 16.\n", 
  //                 path, total_fields, summary_top_content);
  //return -1;  //summary.top is wrong
}

// Below two func to get dir size
int find_total_size(const char *fpath, const struct stat *sb, int typeflag) 
{
  total += sb->st_size;
  return 0;
}

int get_partition_size(char *path, char *partition_size)
{
  total = 0;
  //calculate size of partition
  if (ftw(path, &find_total_size, 1))
  {
    //perror("ftw");
    strcpy(partition_size, "-1");
    return -1;
  }

  sprintf(partition_size, "%lld", total);
  return 0;
}
/* Function to create database connection */
int createDBConnection()
{
  conninfo = "dbname=test user=cavisson";
  /* make a connection to the database */
  dbconnection = PQconnectdb(conninfo);
  /* Check to see that the backend connection was successfully made */
  if (PQstatus(dbconnection) != CONNECTION_OK)
  {
    //fprintf(stderr, "Connection to database failed: %s\n", PQerrorMessage(dbconnection));
    if(dbconnection)
    PQfinish(dbconnection);
    dbconnection = NULL;
    return -1;
  }
  return 0;
}

/*Function to calculate database size */
long long calculate_db_size(const PGresult *res)
{
  long long table_size, total = 0;
  int no_of_rows;  //to store total no. resulted from query2
  int i = 0;
  char *ele;

  /*To get total number of rows from the result of query */
  no_of_rows = PQntuples(res);

  /*To get get result one by one per row and sum all of them*/
  for(i = 0; i < no_of_rows; i++)
  {
    ele = PQgetvalue(res, i, 0);
    table_size = atoll(ele);

    if( table_size < 0)
      return -1;

    total = total + table_size;
  }

  return total;
}

/*Function to execute query */
PGresult *executeCmd(char *query)
{
  PGresult *res;
  const char *paramValues[1];
  int paramLengths[1];
  int paramFormats[1];

  /* If connection to DB is not present then create it. */
  if(!dbconnection)
   createDBConnection();

  //To initialsize length and format of parameter which has to be given to function PQexecParams 
  paramLengths[0] = 0;
  paramFormats[0] = 0;

  //Query to get block size
  res = PQexecParams(dbconnection, query, 0, NULL, paramValues, paramLengths, paramFormats, 0);
  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    PQclear(res);
    return NULL;
  }

  return res;
}
   
void get_partition_db_size(int TRNum, char *partition, char *db_size)
{
  char *blck_size;
  char cmd[1024] = {0};
  PGresult *res = NULL;

  sprintf(db_size, "%d", -1);

  //Query to get block size
  sprintf(cmd, "show block_size");
  res = executeCmd(cmd);
  if(res == NULL)
    return;

  //Save block size
  blck_size = PQgetvalue(res, 0, 0);
  if(atoi(blck_size) < 0)
    return;

  //Query to get disk usage
  sprintf(cmd, "SELECT relpages FROM pg_class WHERE relname like '%%_%d_%s'", TRNum, partition);

  //Get disk usage of all tables 
  res = executeCmd(cmd);
  if(res == NULL)
    return;

  //Calling function which would calculate total size and return total size in blocks unit
  dbsize = calculate_db_size(res);
  if(dbsize < 0)
    return;

  dbsize = (dbsize * atoll(blck_size));
  sprintf(db_size, "%lld", dbsize);
}

void display_partition_info(int TRNum)
{
  char start_date_time[100] = "";
  char state[5] = "";
  char duration[1024 + 1] = "";
  char db_size[100 + 1] = {0};
  char partition_size[100 + 1] = {0};
  char ns_wdir[512] = {0};
  char path[2024] = {0};
  struct dirent **namelist;
  PartitionInfo part_info;
  int n = 0, total_partition = 0, filtered_partition = 0;

  if (getenv("NS_WDIR") != NULL)
    strcpy(ns_wdir, getenv("NS_WDIR"));

  sprintf(path, "%s/logs/TR%d", ns_wdir, TRNum);

  n = scandir(path, &namelist, 0, mysort);

  if (n <= 0)
  {
    fprintf(stderr, "Error in reading TR directory, Error is %s\n", nslb_strerror(errno));
    return;
  }

  while(n--)
  {
    if(is_partition(namelist[n]->d_name) != 0) //found partition
    {
      total_partition++; //count of total partitions in TR 

      sprintf(path, "%s/logs/TR%d/%s/", ns_wdir, TRNum, namelist[n]->d_name);

      //notes
      print_partition_notes(path);

      //search keywords
      if(num_search_keywords) 
      {
        if(search_text(namelist[n]->d_name) != 1)
          continue;
      }

      //partition duration & match mode
      if(get_duration_from_summary_top(start_date_time, state, duration, path) == 1)
        continue;

      //apply duration filter
      if(max_duration != -1)
      {
        if(!(partition_duration >= min_duration && partition_duration <= max_duration))
          continue;
      }
   
      //partiton db size
      get_partition_db_size(TRNum, namelist[n]->d_name, db_size);

      //partition size
      get_partition_size(path, partition_size);

      if(check_if_link_of_script_exists(ns_wdir, TRNum, atoll(namelist[n]->d_name), &part_info) == 1)
        strcpy(is_start_partition, "YES");
      else
        strcpy(is_start_partition, "NO");

      printf("%s|%s|%s|%s|%s|%s|%s|%s\n", namelist[n]->d_name, state, duration, start_date_time, partition_size, db_size, notes, is_start_partition);
      fflush(stdout);

      filtered_partition++; //count of filtered partitions

      free(namelist[n]);
    }
  }
  printf("Filter selected %d out of %d partitions.\n", filtered_partition, total_partition);
  fflush(stdout);
  free(namelist);
}


int main(int argc, char *argv[])
{
  char option;
  char keywords[1024] = {0};

  while ((option = getopt(argc, argv, "t:m:k:d:s:")) != -1)
  {
    switch (option)
    {
      case 't':
        TRNum = atoi(optarg);
        break;
      case 'm' :
        strcpy(mode, optarg);
        break;
      case 'k' :
        strcpy(keywords, optarg);
        num_search_keywords = get_tokens(keywords, search_keywords, " ", 3);
        break; 
      case 'd':  
        fill_min_max_duration(optarg, &max_duration, &min_duration);
        break;
     case 's':  
        cur_time = time(NULL);
        last_n_days = atoi(optarg) * 24 * 60 * 60; // In Secs
        break;
      case ':':
      case '?':
        Usage();
    }
  }

  if(TRNum > 0)
  {
    display_partition_info(TRNum);
  }
  else
  {
    fprintf(stderr, "Error: Mandatory argument 'TestRun Number' missing.\n");
    Usage();
  }

  return 0;
}
