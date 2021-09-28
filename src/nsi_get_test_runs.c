/******************************************************************
 * Name    : ns_monitor_profiles.c
 * Author  : Archana
 * Purpose : This file is to get list of all Test Runs summary.top file
             with info in the following format:
Test Run|Scenario Name|Start Time|Report Summary|Page Dump|Report Progress|Report Detail|Report User|Report Fail|Report Page Break Down|WAN_ENV|REPORTING|Test Name|Test Mode|Run Time|Virtual Users

 * Note:
   Changed Y to Available and N to Unavailable and changed name of all columns
 * Modification History:
 * 23/09/08 - Initial Version
*****************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

#define MAX_FILENAME_LENGTH 1024

//This method to open summary.top file
static FILE * open_file(char *file_name, char *mode)
{
  FILE *fp;

  //printf("open_file() - file_name = %s\n", file_name);

  if((fp = fopen(file_name, mode)) == NULL)
  {
      //fprintf(stderr, "Error in opening %s file. Ignoring this...\n", file_name);
      return(fp);
  }
  //printf("File %s opened successfully.\n", file_name, fp);
  return(fp);
}

//Tokanize the line with token '|' and store in fields 
static int get_tokens(char *line, char *fields[], char *token )
{
  int totalFlds = 0;
  char *ptr;

  ptr = line;
  while((fields[totalFlds] = strtok(ptr, token)) != NULL)
  {
    ptr = NULL;
    totalFlds++;
  }
  return(totalFlds);
}

//To read summary.top file and replace Y to Available and N to Unavailable if found
int read_summary_top(char *file_name)
{
  FILE *fp;
  char line[8192];
  char *fields[500];
  int totalFlds, i = 0;
 
  if((fp = open_file (file_name, "r")) == NULL)
    return 1;

  if(fgets(line, 8192, fp) == NULL)
  {
    //printf("Error: file is empty\n");
    fclose(fp);
    return 1; 
  }
  fclose(fp);

  totalFlds = get_tokens(line, fields, "|");  
  if (totalFlds == 0)
  {
    //printf("Error: %s file is empty\n");
    return 1;
  }
  //if (totalFlds < 15)
  //{
    //printf("Error: Number of fields are not correct in %s file\n");
   // return 1;
  //}
  if(i == 0) // First field
    printf("%s", fields[i]);

  if(i+1 < totalFlds)  printf("|");

  for(i = 1; i < totalFlds; i++)
  {
    if(i < totalFlds -1)
    {
      if(!strcmp(fields[i], "Y"))
        printf("%s|", "Available");
      else if(!strcmp(fields[i], "N"))
        printf("%s|", "Unavailable");
      else printf("%s|", fields[i]);
    }
    else
      printf("%s", fields[i]);
  }
  //printf("\n");
  return 0;
}

void show_all_test_runs(char *NSLogsPath)
{
  FILE *fp;
  char TRNumPtr[1024];
  char file_name[2024];

  fp = popen("ls -d TR* | cut -c3- | sort -n", "r");
  if(fp == NULL)
  {
    perror("popen"); //ERROR: popen failed
    exit(-1);
  }

  while(fgets(TRNumPtr, 1024, fp)!= NULL )
  {
    TRNumPtr[strlen(TRNumPtr) - 1] = '\0';  //// Replacing new line by null
    sprintf(file_name, "%s/TR%s/summary.top", NSLogsPath, TRNumPtr);
    //printf("Reading %s file\n", file_name);
    read_summary_top(file_name);
  }
  pclose(fp);
}

void show_requested_test_run(int TRNum, char *NSLogsPath)
{
  char file_name[2024];
  DIR *dir;

  sprintf(file_name, "%s/TR%d/", NSLogsPath, TRNum);
  if((dir = opendir(file_name)) == NULL)
  {
    printf("Test number not found !\n");
    exit(-1);
  }
  closedir(dir);

  sprintf(file_name, "%s/TR%d/summary.top", NSLogsPath, TRNum);
  //printf("Reading %s file\n", file_name);
  if (read_summary_top(file_name) == 1)
  {
    exit(-1);
  }
}


int main(int argc, char *argv[])
{
  char wdir[1024];
  char NSLogsPath[2024];
  int TRNum; 

  if (getenv("NS_WDIR") != NULL)
  {
    strcpy(wdir, getenv("NS_WDIR"));
    sprintf(NSLogsPath, "%s/logs", wdir);
    //sprintf(NSLogsPath, "%s/logs/", wdir);
  }
  else
  {
    printf("NS_WDIR env variable is not set. Setting it to default value /home/cavisson/work/");
    //sprintf(NSLogsPath, "/home/cavisson/work/logs/");
    sprintf(NSLogsPath, "/home/cavisson/work/logs");
  }

  //printf("Setting directory to %s\n", NSLogsPath);
  chdir(NSLogsPath);

  if(argc > 2)
  {
    printf("Usage: %s <TRNum>\n", argv[0]); 
    exit(-1);
  }
  printf("Test Run|Scenario Name|Start Time|Report Summary|Page Dump|Report Progress|Report Detail|Report User|Report Fail|Report Page Break Down|WAN_ENV|REPORTING|Test Name|Test Mode|Run Time|Virtual Users\n");
  if (argc <= 1)
  {
    show_all_test_runs(NSLogsPath);
  }
  else
  {
    TRNum = atoi(argv[1]);
    show_requested_test_run(TRNum, NSLogsPath);
  }
  return 0;
}
