#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libpq-fe.h"
#include <getopt.h>
#include <unistd.h>
#include <stdarg.h>
#include <pthread.h>

#include "../../libnscore/nslb_util.h"

#define HEADER "TestRun|SvcName|SvcIndex|Count|SignatureCount|CPSignatureCount|SuccessCount|FailCount|AvgTotalTime|AvgQueueWaitTime|AvgAppSelfTime|AvgSORRespTime|MinSORRespTime|MaxSORRespTime|VarianceSORRespTime\n"

typedef struct {
  char SvcName[1024];
  int SvcIndex;
  int Count;
  int SignatureCount;
  int CPSignatureCount;
  int SuccessCount;
  int FailCount;
  double AvgTotalTime;
  double AvgQueueWaitTime;
  double AvgAppSelfTime;
  double AvgSORRespTime;
  int MinSORRespTime;
  int MaxSORRespTime;
  double VarianceSORRespTime;
  char usedOrNot;
}svc_Data;


int debug = 0;
FILE *logfp = NULL;
int trnum, trnum2;
int status = 0;
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

static inline void fill_svc_data_structer(char *buf, svc_Data **svcData, int *max_alloc_index, int *used_index)
{
  int num_field;
  char *fields[32];

  if(!svcData)
    return;
  num_field = get_tokens_with_multi_delimiter(buf, fields, "|", 32);
  
  if(num_field < 14)
  {
    fprintf(stderr, "error:\n");
    exit (-1);
  }

  if(*used_index == *max_alloc_index)  {
    *max_alloc_index += 8;
    *svcData = (svc_Data*)realloc(*svcData, *max_alloc_index * sizeof(svc_Data)); 
  }

  strcpy((*svcData)[*used_index].SvcName, fields[0]);
  (*svcData)[*used_index].SvcIndex = atoi(fields[1]);
  (*svcData)[*used_index].Count = atoi(fields[2]);
  (*svcData)[*used_index].SignatureCount = atoi(fields[3]);
  (*svcData)[*used_index].CPSignatureCount = atoi(fields[4]);
  (*svcData)[*used_index].SuccessCount = atoi(fields[5]);
  (*svcData)[*used_index].FailCount = atoi(fields[6]);
  (*svcData)[*used_index].AvgTotalTime = atof(fields[7]);
  (*svcData)[*used_index].AvgQueueWaitTime = atof(fields[8]);
  (*svcData)[*used_index].AvgAppSelfTime = atof(fields[9]);
  (*svcData)[*used_index].AvgSORRespTime = atof(fields[10]);
  (*svcData)[*used_index].MinSORRespTime = atoi(fields[11]);
  (*svcData)[*used_index].MaxSORRespTime = atoi(fields[12]);
  (*svcData)[*used_index].VarianceSORRespTime = atof(fields[13]);
  (*svcData)[*used_index].usedOrNot = 1;

  (*used_index)++;
}
static inline void merge_and_print_svc_data(svc_Data *t1_svcData, int t1_max_alloc_index, 
                                              int t1_used_index, svc_Data * t2_svcData, int t2_max_alloc_index, int t2_used_index)
{
  int i, j;

  fprintf(stdout, "%s", HEADER);
  
  for(i = 0; i < t1_used_index; i++)
  {
    for(j = 0; j < t2_used_index; j++)
    {
      if(t2_svcData[j].usedOrNot)
        if(!strcmp(t1_svcData[i].SvcName, t2_svcData[j].SvcName))
          break;
    }
    //current instance is not found in another TR
    if(j == t2_used_index)
    {
      fprintf(stdout, "%d|%s|%d|%d|%d|%d|%d|%d|%.3f|%.3f|%.3f|%.3f|%d|%d|%.3f\n"
                      "%d|%c|%c|%c|%c|%c|%c|%c|%c|%c|%c|%c|%c|%c|%c\n", 
                      trnum, t1_svcData[i].SvcName, t1_svcData[i].SvcIndex, t1_svcData[i].Count, 
                      t1_svcData[i].SignatureCount, t1_svcData[i].CPSignatureCount, t1_svcData[i].SuccessCount, 
                      t1_svcData[i].FailCount, t1_svcData[i].AvgTotalTime, t1_svcData[i].AvgQueueWaitTime, 
                      t1_svcData[i].AvgAppSelfTime, t1_svcData[i].AvgSORRespTime, t1_svcData[i].MinSORRespTime, 
                      t1_svcData[i].MaxSORRespTime, t1_svcData[i].VarianceSORRespTime, 
                      trnum2, '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-');
      continue;
    }

    t2_svcData[j].usedOrNot = 0;
    fprintf(stdout, "%d|%s|%d|%d|%d|%d|%d|%d|%.3f|%.3f|%.3f|%.3f|%d|%d|%.3f\n"
                    "%d|%s|%d|%d|%d|%d|%d|%d|%.3f|%.3f|%.3f|%.3f|%d|%d|%.3f\n", 
                    trnum, t1_svcData[i].SvcName, t1_svcData[i].SvcIndex, t1_svcData[i].Count, 
                    t1_svcData[i].SignatureCount, t1_svcData[i].CPSignatureCount, t1_svcData[i].SuccessCount, 
                    t1_svcData[i].FailCount, t1_svcData[i].AvgTotalTime, t1_svcData[i].AvgQueueWaitTime, 
                    t1_svcData[i].AvgAppSelfTime, t1_svcData[i].AvgSORRespTime, t1_svcData[i].MinSORRespTime, 
                    t1_svcData[i].MaxSORRespTime, t1_svcData[i].VarianceSORRespTime, 
                    trnum2, t2_svcData[j].SvcName, t2_svcData[j].SvcIndex, t2_svcData[j].Count, 
                    t2_svcData[j].SignatureCount, t2_svcData[j].CPSignatureCount, t2_svcData[j].SuccessCount, 
                    t2_svcData[j].FailCount, t2_svcData[j].AvgTotalTime, t2_svcData[j].AvgQueueWaitTime, 
                    t2_svcData[j].AvgAppSelfTime, t2_svcData[j].AvgSORRespTime, t2_svcData[j].MinSORRespTime, 
                    t2_svcData[j].MaxSORRespTime, t2_svcData[j].VarianceSORRespTime);
  }
  for(i = 0; i < t2_used_index; i++)
  {
    if(t2_svcData[i].usedOrNot)
    {
      fprintf(stdout, "%d|%c|%c|%c|%c|%c|%c|%c|%c|%c|%c|%c|%c|%c|%c\n"
                      "%d|%s|%d|%d|%d|%d|%d|%d|%.3f|%.3f|%.3f|%.3f|%d|%d|%.3f\n",
                      trnum, '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-',
                      trnum2, t2_svcData[i].SvcName, t2_svcData[i].SvcIndex, t2_svcData[i].Count,
                      t2_svcData[i].SignatureCount, t2_svcData[i].CPSignatureCount, t2_svcData[i].SuccessCount,
                      t2_svcData[i].FailCount, t2_svcData[i].AvgTotalTime, t2_svcData[i].AvgQueueWaitTime,
                      t2_svcData[i].AvgAppSelfTime, t2_svcData[i].AvgSORRespTime, t2_svcData[i].MinSORRespTime,
                      t2_svcData[i].MaxSORRespTime, t2_svcData[i].VarianceSORRespTime);
    }
  }
}

int main(int argc, char **argv)
{
  char cmd[2048] = "";
  FILE *pipefp = NULL;
  svc_Data *t1_svcData = NULL;
  svc_Data *t2_svcData = NULL;
  int t1_max_alloc_index =0, t1_used_index = 0;
  int t2_max_alloc_index =0, t2_used_index = 0;
  int i = 0;



  static struct option long_options[] = {
    {"testrun",        required_argument, 0, 0},
    {"testrun2",       required_argument, 0, 0},
    {"status",         required_argument, 0, 0},
    {"get_count",      required_argument, 0, 0},
    {"svcindex",       required_argument, 0, 0},
    {"svcinstance",    required_argument, 0, 0},
    {"svcsignatureid", required_argument, 0, 0},
    {"starttime",      required_argument, 0, 0},
    {"endtime",        required_argument, 0, 0},
    {"resptimeqmode",  required_argument, 0, 0},
    {"responsetime",   required_argument, 0, 0},
    {"responsetime2",  required_argument, 0, 0},
    {"limit",          required_argument, 0, 0},
    {"offset",         required_argument, 0, 0},
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

          case 1: //test run number
            trnum2 = atoi(optarg); 
            if(trnum2 < 1000) 
              print_usage(argv[0], "ERROR: Invalid test run number given\n"); 
            break; 

          case 2: //svcinstance
            status = atoi(optarg); 
            break;

          case 3: //get_count
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

  sprintf(cmd, "nsi_db_svc_get_data");
  for (i = 1; i < argc; i++)
  {
    if(!strcmp(argv[i], "--testrun2"))
      i++;
    else
      sprintf(cmd, "%s %s", cmd, argv[i]);
  }

  pipefp = popen(cmd, "r");

  if(!pipefp)
  {
    printf("Could not execute '%s'\n", cmd);
    return 1;
  }
  char *ret;
  char tmpstr[2048];
  char first = 1; // use to ignor the header


  while(1)
  {
    ret = fgets(tmpstr, 2048, pipefp);
    if (ret == NULL) break;
    if(first)
    {
      first = 0;
      continue;
    }
    fill_svc_data_structer(tmpstr, &t1_svcData, &t1_max_alloc_index, &t1_used_index);
  }


  sprintf(cmd, "nsi_db_svc_get_data");
  for (i = 1; i < argc; i++)
  {
    if(!strcmp(argv[i], "--testrun"))
      i++;
    else if(!strcmp(argv[i], "--testrun2"))
      sprintf(cmd, "%s --testrun", cmd);
    else
      sprintf(cmd, "%s %s", cmd, argv[i]);
  }

  pipefp = popen(cmd, "r");

  if(!pipefp)
  {
    printf("Could not execute '%s'\n", cmd);
    return 1;
  }

  first = 1;
  while(1)
  {
    ret = fgets(tmpstr, 2048, pipefp);
    if (ret == NULL) break;
    if(first)
    {
      first = 0;
      continue;
    }

    fill_svc_data_structer(tmpstr, &t2_svcData, &t2_max_alloc_index, &t2_used_index);
  }


  merge_and_print_svc_data(t1_svcData, t1_max_alloc_index, t1_used_index, t2_svcData, t2_max_alloc_index, t2_used_index);
  return 0;
}
