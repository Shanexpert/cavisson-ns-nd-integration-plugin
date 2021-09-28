#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libpq-fe.h"
#include <getopt.h>
#include <unistd.h>
#include <stdarg.h>
#include <pthread.h>

int debug = 0;
FILE *logfp = NULL;
int trnum;
int svcinst = 0;
int getcount = 0;

#define debug_log(...) \
{ \
  if(debug && logfp)\
  { \
    fprintf(logfp, "%s|%d|%s|",__FILE__, __LINE__, __FUNCTION__); \
    fprintf(logfp, __VA_ARGS__); \
  } \
}

inline void print_usage(char *progname, char *msg)
{
  printf("%s\n", msg);
  printf("Usage: %s --testrun <test run number> [--signature <flowpath signature>] \n"
         "       [--urlidx <number>] [--phaseidx <index>] \n"
         "       [--starttime <time in ms>] [--endtime <time in ms>] \n"
         "       [--location <name>] [--access <name>] [--browser <name>] \n"
         "       [--script <name>] [--trans <name>] [--page <name>] \n"
         "       [--responsetimeqmode <1 - greater than, 2 - less than, 3 - range>] \n"
         "       [--responsetime <time in ms>] \n"
         "       [--responsetime2 <time in ms (only in case responsetimeqmode is 3>] \n"
         "       [--walltime_threshold <time in ms>] \n"
         "\n"
         "Notes: --testrun, --urlidx and --signature are mandatory arguments\n"
         "       -D if given, will generate logs in /tmp/%s.nnn\n"
         "       where nnn is process id\n", progname, progname);
  exit(1);

}

int main(int argc, char **argv)
{
  char cmd[2048] = "";
  FILE *pipefp = NULL;
  const char *conninfo;
  PGconn *dbconnection;

  int nrows = 0;
  PGresult *res;

  conninfo = "dbname=test user=netstorm";
  /* make a connection to the database */
  dbconnection = PQconnectdb(conninfo);
  /* Check to see that the backend connection was successfully made */
  if (PQstatus(dbconnection) != CONNECTION_OK)
  {
    fprintf(stderr, "Connection to database failed: %s\n", PQerrorMessage(dbconnection));
    if(dbconnection)
      PQfinish(dbconnection);
    dbconnection = NULL;
    return -1;
  }

  static struct option long_options[] = {
    {"testrun",        required_argument, 0, 0},
    {"svcindex",       required_argument, 0, 0},
    {"svcinstance",    required_argument, 0, 0},
    {"svcsignatureid", required_argument, 0, 0},
    {"starttime",      required_argument, 0, 0},
    {"endtime",        required_argument, 0, 0},
    {"status",         required_argument, 0, 0},
    {"resptimeqmode",  required_argument, 0, 0},
    {"responsetime",   required_argument, 0, 0},
    {"responsetime2",  required_argument, 0, 0},
    {"limit",          required_argument, 0, 0},
    {"offset",         required_argument, 0, 0},
    {"get_count",      required_argument, 0, 0},
    {"instancename",   required_argument, 0, 0},
    {"instanceid",     required_argument, 0, 0},
    {0,                0,                 0, 0}
  };

  while (1)
  {
    int option_index = 0;
    char c;
    c = getopt_long(argc, argv, "", long_options, &option_index);

    if(c == -1) 
      break;

    switch (c) 
    {
      case 0:
        switch(option_index)
        {
          case 0: //test run number
            trnum = atoi(optarg); 
            if(trnum < 1000) 
              print_usage(argv[0], "ERROR: Invalid test run number given\n"); 
            break; 

          case 2: //svcinstance
            svcinst = atoi(optarg); 
            break;

          case 12: //get_count
            getcount = atoi(optarg); 
            break;

          default:
            break;
        }
        break;
      case '?':
        break;
    }
  }

  sprintf(cmd, "nsi_db_svc_get_comp_timing_details_sh");
  if(!svcinst && !getcount)
    sprintf(cmd, "%s --only_query 1", cmd);

  int i; 
  for (i = 1; i < argc; i++)
    sprintf(cmd, "%s %s", cmd, argv[i]);

  pipefp = popen(cmd, "r");

  if(!pipefp)
  {
    printf("Could not execute '%s'\n", cmd);
    return 1;
  }
  char *ret;
  char tmpstr[2048];

  if(svcinst || getcount)
  {
    while(1)
    {
      ret = fgets(tmpstr, 2048, pipefp);
      if (ret == NULL) break;
      if(tmpstr[0] != '\n') printf("%s", tmpstr);
    }
    return 0;
  }

  cmd[0] = '\0';
  //char tmpstr[2048] = "";
  //char *ret;
  while(1)
  {
    ret = fgets(tmpstr, 2048, pipefp);
    if (ret == NULL) break;
    sprintf(cmd, "%s %s", cmd, tmpstr);
  }
  if(!strstr(cmd, "SELECT"))
  {
    fprintf(stderr, "Error: Could not create SQL query string\n%s\n", cmd);
    return 1;
  }

  strcat(cmd, ";");

  res = PQexec(dbconnection, cmd);
  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    PQclear(res);
    if(dbconnection)
      PQfinish(dbconnection);
    dbconnection = NULL;
    return 1;
  }
  nrows = PQntuples(res);

  if (nrows <= 0)
    return 0;

  debug_log("Num of rows = %d\n", nrows);

  struct component_t{
    char* SvcCompName;
    char* StartTime;
    char* QueueWaitTime; 
    char* SORRespTime;
    char* StatusIndex;
    char* Status;
    char* CPFlag;
  } comp_arr[nrows];

  for(i = 0; i < nrows; i++)
  {
    comp_arr[i].SvcCompName = PQgetvalue(res, i, 0); 
    comp_arr[i].StartTime = PQgetvalue(res, i, 1); 
    comp_arr[i].QueueWaitTime = PQgetvalue(res, i, 2); 
    comp_arr[i].SORRespTime = PQgetvalue(res, i, 3); 
    comp_arr[i].StatusIndex = PQgetvalue(res, i, 4); 
    comp_arr[i].Status = PQgetvalue(res, i, 5); 
    comp_arr[i].CPFlag = PQgetvalue(res, i, 6); 
  }

  printf("SvcCompName|StartTime|QueueWaitTime|SORRespTime|StatusIndex|Status|CPFlag\n");
  int max_endtime = (atoi(comp_arr[0].StartTime) + 
                     atof(comp_arr[0].QueueWaitTime) + 
                     atof(comp_arr[0].SORRespTime));

  int max_endtime_comp_index = 0;
  
  for(i = 1; i < nrows; i++)
  {
    if(atoi(comp_arr[i].StartTime) > max_endtime) /* New component group starts */
    {
      /* Difference will add to the app time */
      comp_arr[max_endtime_comp_index].CPFlag[0] = '1';
    }

    int endtime = (atoi(comp_arr[i].StartTime) +
                   atof(comp_arr[i].QueueWaitTime) + 
                   atof(comp_arr[i].SORRespTime));

    if(endtime >= max_endtime)
    {
      max_endtime_comp_index = i;
      max_endtime = endtime;
    }
  }
  comp_arr[max_endtime_comp_index].CPFlag[0] = '1';

  for(i = 0; i < nrows; i++)
  {
    printf("%s|%s|%s|%s|%s|%s|%s\n", 
      comp_arr[i].SvcCompName,
      comp_arr[i].StartTime,
      comp_arr[i].QueueWaitTime,
      comp_arr[i].SORRespTime,
      comp_arr[i].StatusIndex,
      comp_arr[i].Status,
      comp_arr[i].CPFlag);
  }

  return 0;
}
