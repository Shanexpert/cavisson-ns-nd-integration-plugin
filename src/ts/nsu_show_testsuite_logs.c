/******************************************************************
 * Name    : nsu_show_testsuite_logs.c
 * Author  : Anuj Sharma
 * Usage   : To See the Usage check the usage function below
 * Purpose : This file is to get list of all Test suites summary.report file
             with info in the following format:
*****************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <pwd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <fcntl.h>
#include <libgen.h>
#include <errno.h>

#define MAX_FILENAME_LENGTH 1024
#define MAX_DATA_LINE_LENGTH 1024

static int TSR_FLAG = 0;
//static int CYCLE_NUMBER;
static char *CYCLE_NUMBER;
int errno;

static void usage()
{
   printf("Usage :\n");
   printf("   -a  <No Argument>       : Show Logs for all Test Cycles Numbers\n");
   printf("   -n  <Test Cycle Number> : Show All Logs for this Test Cycle Number \n");
   printf("   -f  <Test Cycle Number> : Show Failed Case Logs for this Test Cycle Number\n");
   printf("   -p  <Test Cycle Number> : Show Passes Case Logs for this Test Cycle Number\n");
   exit (-1);
}

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

static int get_tokens(char *read_buf, char *fields[], char *token, int max_flds)
{
  int totalFlds = 0;
  char *ptr;
  char *token_ptr;
    
  ptr = read_buf;
  while((token_ptr = strtok(ptr, token)) != NULL)
  { 
    ptr = NULL;
    totalFlds++;
    if(totalFlds > max_flds)
    {
      fprintf(stderr, "Total fields are more than max fields (%d), remaining fields are ignored\n", max_flds);
      totalFlds = max_flds;
      break;  /* break from while */
    }
    fields[totalFlds - 1] = token_ptr;
  }
  return(totalFlds);
}

static void get_log_dir_name(char *log_dir, char *log_dir_name)
{
 int num_tokens;
 char *temp;
 char *log_fields[100];
 char *log_fields_one[100];
 char log_dir_tok_two[8192];
 //int log_tokens;
 get_tokens(log_dir, log_fields, "<", 100);

 strcpy(log_dir_name, log_fields[0]);
 if(log_fields[1] != NULL)
   strcpy(log_dir_tok_two, log_fields[1]);
  num_tokens = get_tokens(log_dir_tok_two, log_fields_one, "-", 100);
  if(num_tokens)
  {
     sprintf(log_dir_name, "%s_%s_%s", log_dir_name, log_fields_one[0], log_fields_one[1]);
     temp = rindex(log_dir_name, '>');
     if(temp)
       *temp=0;
  }
}

//This function is to check the size of a file if(file_size == 0 print Unavailable)
int check_file_size(char *dir, char *file_name_with_path)
{
  struct stat fileStats;
  int file_size = 0;

  if (stat(file_name_with_path, &fileStats) == 0) 
  {
    file_size = (int)fileStats.st_size;
     if(file_size == 0)
       printf("Unavailable");
     else
       printf("%s/%s", dir, basename(file_name_with_path)); //pre_stat_setup.log
  }
  else if(errno == ENOENT)
  {
     //fprintf(stderr, "File %s not present.\n", file_name);
     printf("Unavailable");
     return(1);
  }
  return(0);
}

//This functin is to get the data from the summary.report file in the following format
//TEST_DESCRIPTION|TR_NUM|STATUS|pre_file_path|post_file_path|check_file_path|test_run_file_path 
int read_summary_report_for_given_testsuite(char *file_name, char *CYCLE_NUMBER, char *TCLogsPath, char *TSRNumPtr, char *line_tmp)
{
 char test_description[8192];
 char log_dir[8192];
 char log_dir_name[8192];
 char webapps_logs_dir[8192];
 char *fields[100];
 int tokens;
 char pre_stat_file[8192];
 char post_stat_file[8192];
 char check_stat_file[8192];
 char run_data_stat_file[8192];
 char test_run_file[8192];
 char pre_file_name[1024];
 char post_file_name[1024];
 char check_file_name[1024];
 char test_file_name[1024];
 char run_data_file_name[1024];

 strcpy(pre_file_name, "pre_test_setup.report"); 
 strcpy(post_file_name, "post_test_setup.report");
 strcpy(check_file_name, "check_status.report");
 strcpy(test_file_name, "test_run.report");
 strcpy(run_data_file_name, "run_data.report");

 test_description[0] = '\0';

 tokens = get_tokens(line_tmp, fields, " ", 100);
 int i;
 printf("%s|",TSRNumPtr);  //TSR_NUMBER  
 printf("%s|", fields[0]); //TEST_CASE_ID

 for(i=7; i<tokens; i++)
   sprintf(test_description, "%s %s", test_description, fields[i]);

 test_description[strlen(test_description) -1] ='\0';

 if (test_description[0] != '\0')
   printf("%s|", test_description + 1); //TEST_DESCRIPTION
 else
   printf("Unavailable|");
  
 if (fields[1] != NULL)
   printf("%s|", fields[1]); //TR_NUM
 else
   printf("Unavailable|");

 if (fields[6] != NULL) 
   printf("%s|", fields[6]); //STATUS
 else
   printf("Unavailable|");

 strcpy(log_dir, fields[0]);
 get_log_dir_name(log_dir, log_dir_name);
 sprintf(webapps_logs_dir, "logs/tsr/%s/%s/logs/%s", CYCLE_NUMBER, TSRNumPtr, log_dir_name);
 sprintf(pre_stat_file, "%s/%s/logs/%s/%s", TCLogsPath, TSRNumPtr, log_dir_name, pre_file_name);
 sprintf(post_stat_file, "%s/%s/logs/%s/%s", TCLogsPath, TSRNumPtr, log_dir_name, post_file_name);
 sprintf(check_stat_file, "%s/%s/logs/%s/%s", TCLogsPath, TSRNumPtr, log_dir_name, check_file_name);
 sprintf(test_run_file, "%s/%s/logs/%s/%s", TCLogsPath, TSRNumPtr, log_dir_name, test_file_name);
 sprintf(run_data_stat_file, "%s/%s/logs/%s/%s", TCLogsPath, TSRNumPtr, log_dir_name, run_data_file_name);
 //printf("pre_stat_file = %s\n, post_stat_file = %s\n, check_stat_file = %s\n, test_run_file = %s\n, webapps_logs_dir = %s\n", pre_stat_file, post_stat_file, check_stat_file, test_run_file, webapps_logs_dir);
 //printf("run_data_file_name = %s\n", run_data_file_name);

 check_file_size(webapps_logs_dir, pre_stat_file); //pre_test_setup.report
 printf("|");
 check_file_size(webapps_logs_dir, post_stat_file); //post_test_setup.report 
 printf("|");
 check_file_size(webapps_logs_dir, check_stat_file); //check_test_setup.report
 printf("|");
 check_file_size(webapps_logs_dir, test_run_file); //"test_run.report" 
 printf("|");
 check_file_size(webapps_logs_dir, run_data_stat_file); //"run_data.report" 
 printf("\n");

 return 0;
}

int tokenize_summary_report_for_given_testsuite(char *file_name, char *CYCLE_NUMBER, char *TCLogsPath, char *TSRNumPtr, char *FLAG_SHOW_CASE)
{
 FILE *fp;
 char line[8192];
 char token_line[8192];
 int num_line = 0;
 char *fields[100];
 //int tokens;
 if((fp = open_file (file_name, "r")) == NULL)
    return 1;
 while (fgets(line, 1024, fp))
 {
   num_line++;

   if(num_line < 7)
     continue;
   strcpy(token_line, line);
   get_tokens(token_line, fields, " ", 100);

   if (strncmp(FLAG_SHOW_CASE, "ALL", strlen("ALL")) == 0) //This is if want to see all the case
     read_summary_report_for_given_testsuite(file_name, CYCLE_NUMBER, TCLogsPath, TSRNumPtr, line);

   if (strncmp(FLAG_SHOW_CASE, "FAIL", strlen("FAIL")) == 0)
   {
     if (strncmp(fields[6], "Pass", strlen("Pass")) != 0) //This is if want to see all the Failed cases  
       read_summary_report_for_given_testsuite(file_name, CYCLE_NUMBER, TCLogsPath, TSRNumPtr, line);
     else
       continue;
   }
 
   if (strncmp(FLAG_SHOW_CASE, "PASS", strlen("PASS")) == 0)
   {
     if (strncmp(fields[6], "Pass", strlen("Pass")) == 0) //This is if want to see all the Passes cases
       read_summary_report_for_given_testsuite(file_name, CYCLE_NUMBER, TCLogsPath, TSRNumPtr, line);
     else
       continue;
   }
 
 }
 if(num_line < 7)
   printf("Unavailable|Unavailable|Unavailable|Unavailable|Unavailable|Unavailable|Unavailable|Unavailable|Unavailable|Unavailable\n");

 return 0;
}

//This functin is to get the data from the cycle_summary.report file in the following format
//TEST_CYCLE_NAME|START_TIME|END_TIME|NUM_TSR_IN_CYCLE
//If No testcase executed then END_TIME = START_TIME
int read_cycle_summary_report_for_all_cycle(char *file_name)
{
  FILE *fp;
  //int fd;
  char line[8192];
  char tmp_lin[8192];
  char *tmp_line_val;
  char *tmp_cyc_value;
  char *tmp_suit_value;
  char *tmp_end_date_value;
  char *tmp_mode_value;
  char start_date_time[8192];
  char cycle_number[8192];
  char cycle_mode;
  char testsuite_name[8192];
  char end_date_time[8192] = "\0";
  int total_cases_per_cycle = 0;
  int cases_pass_per_cycle = 0;
  int cases_failed_per_cycle = 0;
  int read_ahead = 0;
  char *fields[100];
  //int size;
  struct stat st;
  stat(file_name, &st);
  //size = st.st_size;
  
  //char tmp_buf[size];
  //int n = 0; 

/*
  if((fd = open(file_name, O_APPEND|O_RDWR)) == -1){
    printf("open error = %s", strerror(errno));
    return 1;
  }
  else
   {
     if((n = read(fd, tmp_buf, size)) < 0)
       printf("read_error = %s", strerror(errno));

     if(strstr(tmp_buf, "TestCycleMode: ") == NULL){
       if((n = write(fd, "TestCycleMode: W", 16)) < 0 )
         printf("write_error = %s", strerror(errno));
       } 
   }
   close(fd);
 */

  if((fp = open_file (file_name, "r")) == NULL)
    return 1;
 
   
  //while (feof(fp) == 0)  //Commented because feof() repeates last line
  while(fgets(line, 8192, fp) != NULL)
  {
   // fgets(line, 8192, fp);
   // total_cases_per_cycle++;
    if (line[0] == '#' || line[0]== '\n') continue; 
    strcpy(tmp_lin, line); 

    if ((tmp_line_val = strstr(tmp_lin, "Start Date/Time: "))) {
      *tmp_line_val = 0;
      strcpy(start_date_time, strlen("Start Date/Time: ") + tmp_line_val);
      start_date_time[strlen(start_date_time) - 1] = '\0'; 
    } else if ((tmp_cyc_value = strstr(tmp_lin, "Test Cycle Name: "))) {
      *tmp_cyc_value = 0;
      strcpy(cycle_number, strlen("Test Cycle Name: ") + tmp_cyc_value);
      cycle_number[strlen(cycle_number) - 1] = '\0'; //Test Cycle Number
    } else if ((tmp_suit_value = strstr(tmp_lin, "Test Suite Name: "))) {
      *tmp_suit_value = 0;
      strcpy(testsuite_name, strlen("Test Suite Name: ") + tmp_suit_value);
      testsuite_name[strlen(testsuite_name) - 1] = '\0'; //Test Suite name
    } else if ((tmp_end_date_value = strstr(tmp_lin, "End Date/Time: "))) {
      *tmp_end_date_value = 0;
       strcpy(end_date_time, strlen("End Date/Time: ") + tmp_end_date_value);
       end_date_time[strlen(end_date_time) - 1] = '\0';
    } else if ((tmp_mode_value = strstr(tmp_lin, "TestCycleMode: "))) {
      *tmp_mode_value = 0;
      cycle_mode = *(tmp_mode_value + 15 /*strlen("TESTCYCLEMODE")*/);
      //strcpy(cycle_mode, strlen("TESTCYCLEMODE: ") + tmp_mode_value);
      //cycle_code[strlen(cycle_code) - 1] = '\0'; 
    } else  if (strstr(line, "ERROR:") != NULL) {
        strcpy(end_date_time, "Unavailable");
    }
    else
    {
      total_cases_per_cycle++;
      if (strncmp(tmp_lin, " ---------------------", strlen(" ---------------------")) == 0)
         read_ahead = 1;

      if(read_ahead == 0)
       continue;
      else
      {
        get_tokens(tmp_lin, fields, " ", 100);
        //fields[7] contains the status of a case Pass, Fail, etc...
        if (fields[7] != NULL)
 	 if (strncmp(fields[7], "Pass", strlen("Pass")) == 0)
 	 {
	    //if Pass then increment cases_pass_per_cycle
	    cases_pass_per_cycle++;
	    fields[7]="";
	 }
      }
    }
  }
    if (end_date_time[0] == '\0')
      strcpy(end_date_time, "In Progress");

    total_cases_per_cycle = total_cases_per_cycle - 2; //As there are 2 extra lines

  /* Bug Id : 70300
   * Issue  : In case of "In Progress" cases_pass_per_cycle and cases_pass_per_cycle is not updated.
   */

  //if (cases_pass_per_cycle < 1 || (strcmp(end_date_time,"In Progress") == 0))
  //  cases_pass_per_cycle = 0;

  cases_failed_per_cycle = total_cases_per_cycle - cases_pass_per_cycle;

  //if (cases_failed_per_cycle < 1 || (strcmp(end_date_time,"In Progress") == 0))
  //  cases_failed_per_cycle = 0; 

  //printing the pipe seperated values in the following sequence
  //TEST_CYCLE_NUM|TESTSUITE_NAME|START_TIME|END_TIME|TOTAL_CASES|TOTAL_PASS_CASES|TOTAL_FAIL_CASES|TESTSUITEMODE
  printf("%s|%s|%s|%s|%d|%d|%d|%c\n", cycle_number, testsuite_name, start_date_time, end_date_time, total_cases_per_cycle, cases_pass_per_cycle, cases_failed_per_cycle, cycle_mode);

  if(fp)
    fclose(fp);

  return 0;
}

//This function is get all the testsuite numbers present is a particular automation test cycle 
void show_all_test_suite_numbers (char *TSLogsPath, char *CYCLE_NUMBER, char *FLAG_SHOW_CASE)
{
  //printf("show_all_test_suite_numbers : Function Called\n");
  FILE *fp;
  char TSRNumPtr[1024];
  char file_name[2024];
  char TCLogsPath[2024];

  sprintf(TCLogsPath, "%s/%s", TSLogsPath, CYCLE_NUMBER);
  chdir(TCLogsPath);
  //printf("The TCLogsPath logs path is %s\n", TCLogsPath);

  fp = popen("ls -d *|cut -c1-|sort -n", "r");
  if(fp == NULL)
  {
    perror("popen"); //ERROR: popen failed
    exit(-1);
  }

  while(fgets(TSRNumPtr, 1024, fp)!= NULL )
  {
    TSRNumPtr[strlen(TSRNumPtr) - 1] = '\0';    // Replacing new line by null
    sprintf(file_name, "%s/%s/summary.report", TCLogsPath, TSRNumPtr);
    tokenize_summary_report_for_given_testsuite(file_name, CYCLE_NUMBER, TCLogsPath, TSRNumPtr, FLAG_SHOW_CASE);
  }
  pclose(fp);
}

//This function is get the list of all the Automation test cycles present in the $NS_WDIR/logs/tsr directory
void show_all_test_cycles(char *TSLogsPath)
{
  FILE *fp;
  char TSRNumPtr[1024];
  char file_name[2024];

  fp = popen("ls -d *|cut -c1-|sort -n", "r");
  if(fp == NULL)
  {
    perror("popen"); //ERROR: popen failed
    exit(-1);
  }
  
  while(fgets(TSRNumPtr, 1024, fp)!= NULL )
  {
    TSRNumPtr[strlen(TSRNumPtr) - 1] = '\0';    // Replacing new line by null
    sprintf(file_name, "%s/%s/cycle_summary.report", TSLogsPath, TSRNumPtr);
    //printf("The file name is %s\n", file_name);
    read_cycle_summary_report_for_all_cycle(file_name);
  }
  pclose(fp);
  //exit(0);
}

/**************************************************************************/

void show_test_cycle(char *TSLogsPath)
{
    char file_name[2024];
    sprintf(file_name, "%s/%s/cycle_summary.report", TSLogsPath, CYCLE_NUMBER);
    read_cycle_summary_report_for_all_cycle(file_name);
}


/**************************************************************************/


int main(int argc, char *argv[])
{
  char wdir[1024];
  char TSLogsPath[2024];
  char option;
  extern char *optarg;
  char *FLAG_SHOW_CASE;
  char *ptr;
  int FLAG_SHOW_MODE = 0;

  if (argc < 2)
    usage();

  ptr = getenv("NS_WDIR");
  if (ptr != NULL)
  {
    strcpy(wdir, ptr);
    sprintf(TSLogsPath, "%s/logs/tsr", wdir);
  }
  else
  {
    printf("NS_WDIR env variable is not set. Setting it to default value /home/cavisson/work/");
    sprintf(TSLogsPath, "/home/cavisson/work/logs/tsr");
  }

  //printf("Setting directory to %s\n", NSLogsPath);
  chdir(TSLogsPath);

  while ((option = getopt(argc, argv, "m:n:f:p:a")) != -1) 
  {
    switch (option) 
    {
      case 'a':
        TSR_FLAG = 1;
        break;
      case 'n':
        CYCLE_NUMBER = (optarg);
        FLAG_SHOW_CASE = "ALL"; 
        break;   
      case 'f':
        CYCLE_NUMBER = (optarg);
        FLAG_SHOW_CASE = "FAIL"; 
        break;   
      case 'p':
        CYCLE_NUMBER = (optarg);
        FLAG_SHOW_CASE = "PASS"; 
        break;  
      case 'm':
        CYCLE_NUMBER = (optarg);
        FLAG_SHOW_MODE = 1;
        break; 
      case ':':
      case '?':
        usage();
      default:
        usage();
    }
  }
  
  //This is called with tsi_show_tsr -a option. It will give the pipe seperated output in the format as given below
  //TEST_CYCLE_NUM|TESTSUITE_NAME|START_TIME|END_TIME|TOTAL_CASES|TOTAL_PASS_CASES|TOTAL_FAIL_CASES
  if (TSR_FLAG == 1)
  {
    printf("TEST_CYCLE_NUM|TESTSUITE_NAME|START_TIME|END_TIME|TOTAL_CASES|TOTAL_PASS_CASES|TOTAL_FAIL_CASES|TESTSUITE_MODE\n");
    show_all_test_cycles(TSLogsPath); 
  }
  if (FLAG_SHOW_MODE == 1 && CYCLE_NUMBER)
  {
    printf("TEST_CYCLE_NUM|TESTSUITE_NAME|START_TIME|END_TIME|TOTAL_CASES|TOTAL_PASS_CASES|TOTAL_FAIL_CASES|TESTSUITE_MODE\n");
    show_test_cycle(TSLogsPath);
  }

  //This is called with tsi_show_tsr -n <TEST_CYCLE_NUM> option.It will give the pipe seperated output in the format as follo
  //TSR_NUM|TESTCASE_ID|TEST_DESC|TR_NUM|PRE_LOG|POST_LOG|CHECK_LOG|TEST_RUN_LOG
  else if (CYCLE_NUMBER)
  {
    printf("TSR_NUM|TESTCASE_ID|TEST_DESC|TR_NUM|STATUS|PRE_LOG|POST_LOG|CHECK_LOG|TEST_RUN_LOG|RUN_DATA_LOG\n");
    show_all_test_suite_numbers(TSLogsPath, CYCLE_NUMBER, FLAG_SHOW_CASE);  
  }
  
  exit(0);
}
