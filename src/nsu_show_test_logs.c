/******************************************************************
 * Name    : nsu_show_test_logs.c
 * Author  : Archana
 * Purpose : This file is to get list of all Test Runs summary.top file
             with info in the following format:

summary.top: Test Run|Scenario Name|Start Time|Report Summary|Page Dump|Report Progress|Report Detail|Report User|Report Fail|Report Page Break Down|WAN_ENV|REPORTING|Test Name|Test Mode|Run Time|Virtual Users

Output :     Project|Subproject|Test Run|Test Name|Scenario Name|Start Time|Run Time|Virtual Users|Report Summary|Report Progress|Report Detail|Report User|Report Fail|Report Page Break Down|WAN_ENV|REPORTING|Test Mode|Page Dump

 * Note:
   Changed Y to Available and N to Unavailable and changed name of all columns  
   Files from where we are calling nsu_show_test_logs.c:- 
   nsu_show_netstorm:
 
   nsu_show_netstorm -n -l 
     nsu_show_test_logs .RL
   nsu_show_netstorm -n
     nsu_show_test_logs -R -t

   gui/servlet/OperateServlet.java
     nsu_show_test_logs -A -r -l

   gui/analyze/index.jsp
     nsu_show_test_logs  -t testRun -r -l

   gui/analyze/rptTestRuns.jsp
     nsu_show_netstorm -n testRun
     nsu_show_test_logs  -A -r -l
     Or
     nsu_show_test_logs strProjSubP + " -d \"" + duration + "\" " + showUser + "" + strStarted + strScenario + strState + strKeyword + " -r -l";
     Or
     nsu_show_test_logs     " -u " + sesLoginName + "  -d \"00:30:00-NA\" -s 7 -r -l";

   gui/analyze/rptCmpUsingRtpl.jsp
     nsu_show_test_logs -A -r -l

   gui/analyze/rptTestDetails.jsp
     nsu_show_test_logs .p  proj/subproj  -s started .d duration -u user -r .l

   gui/bean/GenMsrInfo.java
     nsu_show_test_logs -r

   gui/bean/TestRunInfoUtils.java
     nsu_show_test_logs -A -r -l 

   gui/bean/FileBean.java
     nsu_show_test_logs -t    TRNUM  -rl
   
 * Modification History:
 * 23/09/08 - Initial Version
 * 02/10/09:3.2.3 Archana - To implement Project and Subprojects features
 * 24/01/13:3.9.0 Prachi  - Added option (g) to print "Total Filtered Records" and "Total Records".
 * 27/11/17:4.1.9/4.1.10 Abhay - Added "-u" option for gui user
*****************************************************************/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <pwd.h>
#include <time.h>
#include <sys/stat.h>
#include <libgen.h>
#include <errno.h>
#include <ctype.h>

#include "nslb_util.h" 
#include "nslb_get_norm_obj_id.h"
#include "ns_tr_duration.h"
#include "cav_data_ret/cdr_main.h"
#include "cav_data_ret/cdr_config.h"
#include "cav_data_ret/cdr_cache.h"
#include "cav_data_ret/cdr_log.h"
#include "cav_data_ret/cdr_cleanup.h"
#include "cav_data_ret/cdr_utils.h"
#define MAX_FILENAME_LENGTH 1024
#define MAX_SUMMARY_TOP_FIELDS 16

#define INIT_TEST_RUN_NUMBER 1000

#define DO_NOT_CONVERT 0
#define CONVERT 1

static char separator;
static int  Long_form;
static int  show_all = 0;
//static int  g_keyword_in_notes = 1;
static char g_mode[20];
static int  TRNum, raw_format, num_projs; 
static char proj_sub_proj[100] = "";
static char g_scenario[100];
static char g_notes[8192 + 1] = "NA";
//static char g_keyword[100];
static char *search_keys[100];
static int num_search_key;
static char project[100][100];
static char sub_project[100][100];
struct stat *buf;
static char test_type[64] ="";
char *ctrl_name = NULL;
/* Used  with -o option to save owner user id*/
static int given_owner_id = -1;

/* Used with -d option*/
static long long min_duration = -1;
static long long max_duration = -1;

/* Used with -x option*/
static long long start_time_sec = -1;
static long long end_time_sec = -1;

/* Used with -s option*/
static int last_n_days = -1;
static int cur_time = -1;

/**/
static int max_test_id = 0;
/* added in 3.7.7 for showing running test run only*/
static int Rflag = 0;
static int LLflag = 0;
static char g_log_path[MAXLENGTH + 1]; 
char tomcat_port[32] = {0};
char cmd_out[32] = {0};
//This method to open summary.top file
/*
static void is_netstorm_user()
{
  struct passwd *pw;
  pw = getpwuid(getuid());
  if(!strcmp(pw->pw_name, "netstorm"))
  {
    fprintf(stderr, "Error: This command must be run as 'netstorm' user only.\n");
    exit -1;
  }
}
*/
extern void cdr_get_cmt_partition_from_disk();
static char *search_map_table = NULL;
static inline void print_test_run_notes(char *test_run_path, char separator); 
int cdr_get_tr_list_from_disk(int value);
int tr_in_remove_range(struct cdr_cache_entry *entry);
static int Gflag = 0;
static int Dflag = 0;
static int total_num_records=0;
static int total_filtered_records=0;

//static unsigned long long file_size = 0;
static int zflag = 0;
static int Cflag = 0;
//Added for summary.top changes
TestRunList *test_run_info;
static int temp_summary_file_created = 0;
static char tmp_file_name[128]; 
char wdir[1024];
extern struct NormObjKey cache_entry_norm_table; // use tr_num to get the index of cache entry in cdr_cache_entry_list
int cdr_flag = 0;
char cdr_session_flag[2];
extern struct cdr_cache_entry *entry;
extern struct cdr_cache_entry *cdr_cache_entry_list;
extern struct cdr_cmt_cache_entry *cdr_cmt_cache_entry_list;
extern struct cdr_config_struct cdr_config; // global var that contains all the configuration settings

extern int total_cache_entry;
extern int max_cache_entry;
//replace Y to Available and N to Unavailable if found
void set_cleanup_policy();
inline void cdr_init();

static char str_time[1024+1];
char *format_time_ex(long long int time_stamp)
{
  str_time [0] = '\0';

  if (! time_stamp || time_stamp == -1)
    return "NA";

  struct tm  ts;
  ts = *localtime((time_t *)(&time_stamp));
  strftime(str_time, 1025, "%m/%d/%Y %H:%M:%S", &ts);
  
  return str_time;
}

static inline void convert_available_or_unavalable(char *str, char sep, int convert_flag)
{
  if(!str || !str[0])
  {
    printf("%cNA", sep);
    return;
  }

  if(convert_flag == DO_NOT_CONVERT)
  {
    printf("%c%s", sep, str);
    return;
  }
  
  if(!strcmp(str, "Y"))
    printf("%cAvailable", sep);
  else if(!strcmp(str, "N"))
    printf("%cUnavailable", sep);
  else
    printf("%c%s", sep, str);
}

#if 0
static int get_nxt_test_run_id() {
  char test_id_path[256 + 1];
  sprintf(test_id_path , "%s/etc/test_run_id", getenv("NS_WDIR"));

  FILE *fp = fopen(test_id_path, "r");
  char line[64 + 1];
 
  if(fp == NULL) {
    fprintf(stderr, "Error: Unable to read /etc/test_run_id.\n");
    exit(1);
  }

  fgets(line, 64, fp); 
  fclose(fp);
  return(atoi(line)); 
}
#endif

int get_biggest_test_run(TestRunList *test_run_info_ptr, int running_test_runs)
{
  int i = 0, big = 0;
  
  //initially make first TR biggest TR
  big = test_run_info[0].test_run_list;

  for(i = 1; i < running_test_runs; i++)
  {
    //if have next index, then compare if nxt index TR is greater then reset 'big'
    if(big < test_run_info[i].test_run_list)      
      big = test_run_info[i].test_run_list;
  }
return big;
}

/* Get the list of running test run
   if at least one test run is running than
   allcate a char array of size the next test run 
   from /etc/test_run_id + 256 (may possible during
   the execution of this program 256 test are fired)
   Set the flag to one in search_map which test run is running
   For optimization substract INIT_TEST_RUN_NUMBER  
*/
static void allocate_running_test_search_map() {
 
  int i, big_tr = 0, isnew = 1;
  char work_version[512 + 1];
  char *build, *digits[5];
  FILE *fp;

  sprintf(work_version, "%s/etc/version", wdir);
  if((fp = fopen(work_version, "r")))
  {
    fread(work_version, 1, 512, fp);
    fclose(fp);
    if((build = strchr(work_version, '\n')))
      *build = '\0';
    build += 7; //skipping BUILD\b 
    if(get_tokens(work_version+8, digits, ".", 5) != 3) //skipping VERSION\b
      return;
    if(((atoi(digits[0]) <= 4) && (atoi(digits[1]) < 2)) ||
       ((atoi(digits[0]) == 4) && (atoi(digits[1]) == 2) && !atoi(digits[2]) && build && (atoi(build) < 38)))
      isnew = 0; //old testrun
  }

  if(isnew)
    test_run_running = get_running_test_runs_ex(test_run_info, wdir); 
  else
    test_run_running = get_running_test_runs(test_run_info, wdir);  //bug 77079: get old test run
  //printf("\ntest _run_running=%d\n", test_run_running);
  if (Rflag) {
    return;
  }

  big_tr = get_biggest_test_run(test_run_info, test_run_running);
  if(big_tr == 0)
    return;

  max_test_id = big_tr - INIT_TEST_RUN_NUMBER + 256;  // +256 as somebody can run at max run at this time
  
  if(test_run_running) {
    search_map_table = (char*) malloc(max_test_id);

  if(search_map_table == NULL) {
    fprintf(stderr, "Error: Unable to allocate memory.\n"); 
    exit(1);
  }

    memset(search_map_table, 0, max_test_id);
  }

  for(i = 0; i < test_run_running; i++) {
    search_map_table[test_run_info[i].test_run_list - INIT_TEST_RUN_NUMBER] = 1;
  }
}

static int get_user_id_by_name(char *user_name) {
  struct passwd *pw = getpwnam(user_name);
  
  if(pw == NULL) {
    fprintf(stderr, "Error: Unable to get user id of user '%s'\n", user_name);
    exit(-1);
  }

  return(pw->pw_uid);
}

#if 0//This is to authenticate user is netstorm user or not
static void validate_user(char *user_name)
{
  char cmd[1024]="\0";
  char err_msg[1024]= "\0";
 
  sprintf(cmd, "nsu_check_user %s nsu_show_test_logs", user_name);
  if(nslb_system(cmd,1,err_msg) != 0)
  {
    //fprintf(stderr, "Not a valid user : %s\n", usr);
    exit (-1);
  }
}
#endif
static void usage(int flag)
{
   if(flag)
     printf("Only one option can be used at a time among -A, -u & -p\n");

   printf("Usage :\n");
   printf("   -A  : Show all test run log.\n");
   printf("   -r  : Show in raw format.\n");
   printf("   -l  : Show in Long form.\n");
   printf("   -T  : <ND> Show only ND test .\n");
   printf("   -o  <owner>: Owner.\n");
   printf("   -u  <user> : Show test run logs of this user only.\n");
   printf("   -d  <00:00:00-00:00:00>: Duration.\n");
   printf("   -s  <n>: last n days.\n");
   printf("   -c  <scenario name>: Show all test by scenario name given only.\n");
   printf("   -m  <state>: Show all test run by state provided.\n");
   printf("       valid states : locked ,unlocked, archived\n");
   printf("   -k  <keyword>: search by keyword in scenario ,test name and notes.\n");
   printf("   -p  <project/subproject> -p ..... : Show test run logs of these project/subproject only.\n");
   printf("   -t  <TRnum> Show test run log of given test run number only.\n");
   printf("   -R  Show test run log of all Running test only.\n");
   printf("   -L  Display header in Long list form.\n");
   printf("   -z  Displayes the locked TRs (R and AR mode only).It is used with 'm' mode having locked value.\n");
   printf("   -x  <\"MM/DD/YYYY-HH:MM:SS MM/DD/YYYY-HH:MM:SS\">\n");
   printf("       Takes input in quotes (\"\") as \"startdate-starttime enddate-endtime\" and shows tests running in between the interval.\n");
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

#if 0
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
#endif

/* Return 1 if matched else 0*/
static int match_test_run_with_proj_subproj(char *proj_name, char *subproj_name, int num_fields) {

  int i;

  if(num_projs > 0) {
    /* num_projs is filled with both option -u <user> or -p <Project/Subproject>
     * This is common for -u & -p option */
    for(i = 0; i < num_projs; i++) {
      if( num_fields >= 3) {
	  //printf("project[%d] = %s, sub_project[%d] = %s, proj_name = %s\n", i, project[i], i, sub_project[i], proj_name);
      //we will show that test run if proj & subproject is matched or proj is matched & subproj is All
        if( !strcasecmp(project[i], "All") || (!(strcasecmp(project[i], proj_name)) && 
            (!(strcasecmp(sub_project[i], subproj_name)) ||
             !(strcasecmp(sub_project[i], "All"))))) {
           return 1;
        }
      }
    }
  } else {
    return 1;
  }

  return 0;
}

static int search_text_in_scen_notes_tname(char *scen_name, char **fields) {
 int i ;
 if(num_search_key) {  // at least one key given
   for(i=0;i<num_search_key;i++){
     if(strcasestr(scen_name,search_keys[i]) ||        
        strcasestr(fields[0], search_keys[i]) ||    // test run number
        strcasestr(fields[12], search_keys[i]) ||  // tname
        strcasestr(g_notes, search_keys[i])) 
        return 1; 
   } 
     return 0;
 } else{
     return 1;
   }
}

/*
static void inline get_tr_size(char *dir_path)
{
  char run_cmd[2048];
  char temp_size[1024];
  FILE *fp = NULL;

  sprintf(run_cmd, "du -cb %s | tail -1 | cut  -f1", dir_path);

  fp = popen(run_cmd,"r");

  if(fp == NULL)
  {
    fprintf(stderr, "Error in executing command = %s.", run_cmd);
    exit(1);
  }
 
  if(fgets(temp_size, 1024, fp))
    file_size += atoi(temp_size);

}
*/




/* rneturn 1 means we Print else do not*/
static int show_test_logs(char *proj_name, char *subproj_name,
		                char *scen_name, char **summary_top_fields,
				struct stat *buf, int test_run_number, char* filename, int partition_flag, char *NSLogsPath) {
  int num_fields;
  long long test_run_duration;
  int test_strt_time;
  int keyword_matched;
  int state_matched ;
  char *proj_subproj_scen[100];
  int proj_sub_matched, owner_matched, duration_matched, last_n_days_matched, scenario_matched;
  char tmp[MAXLENGTH + 1]; 
  char chk_for_running = 1;
  long long tr_start_time_sec = 0;
  int relative_test_run_number = test_run_number - INIT_TEST_RUN_NUMBER; 

  // Test run must be >= 1000
  if(!Rflag && (relative_test_run_number > max_test_id || test_run_number < INIT_TEST_RUN_NUMBER)) {
      chk_for_running = 0;  // Do not chk as may possible test run iss copied from anywhere
      // return 0;  // We dont need this test run
  }

  proj_sub_matched = owner_matched = duration_matched = last_n_days_matched = 1;
  scenario_matched = keyword_matched = state_matched = 1;
  //printf("summary_top_fields = %s\n", summary_top_fields);
   
  num_fields = get_tokens(summary_top_fields[1], proj_subproj_scen, "/", 100);
  //printf("num_fields = %d, all = %d\n", num_fields, all);

  if(num_fields < 3) {
    strcpy(scen_name,  proj_subproj_scen[0]);
    strcpy(proj_name, "default");
    strcpy(subproj_name, "default");
  } else {
    strcpy(scen_name,  proj_subproj_scen[2]);
    strcpy(proj_name, proj_subproj_scen[0]);
    strcpy(subproj_name, proj_subproj_scen[1]);
  } 
  
  strcpy(tmp, filename);
  print_test_run_notes (dirname(tmp), separator);  // dirname places NULL
  keyword_matched = search_text_in_scen_notes_tname (scen_name, summary_top_fields);
  
  //filter  test runs by scenario name
  if (g_scenario[0] != '\0') {
    if (strcmp(scen_name, g_scenario) != 0) {
      scenario_matched = 0;
    }
    //If not matched then there is possibility 
    //that we have . in scenario name in summary.top file(when run from test suite)
    //We need to pass that one also so if we fail in matching then check with
    //adding . at starting 
    if(scenario_matched == 0)
    {
      char g_scen_tmp[100];
      strcpy(g_scen_tmp, ".");
      strcat(g_scen_tmp, g_scenario);
      if (strcmp(scen_name, g_scen_tmp) == 0)
        scenario_matched = 1;
    }
  }
  
  if(zflag)
  {
    char dir_path[1024];

    if (g_mode[0] != '\0') {
      if (strcmp(g_mode, "locked") == 0){ 
        if((strcmp(summary_top_fields[13], "R") == 0) || (strcmp(summary_top_fields[13], "AR") == 0))
        {
          sprintf(dir_path, "%s/logs/TR%d", getenv("NS_WDIR"), test_run_number);
          //get_tr_size(dir_path);
        }      
        else
        {
          state_matched = 0 ;
        }
      }
      else
      {
        fprintf(stderr, "When using z mode with m , m can take only locked value\n");
        usage(0);
      }
    }
  }
  else
  {
    if (g_mode[0] != '\0') {
      if (strcmp(g_mode, "locked") == 0) { 
        if (strcmp(summary_top_fields[13], "R" ) != 0) {
          state_matched = 0;   
        } 
      } else if (strcmp(g_mode,"unlocked") == 0) {
        if (strcmp(summary_top_fields[13], "W") != 0) {
          state_matched = 0;
        }
      } else if (strcmp(g_mode, "archived") == 0) {
          if ((strcmp(summary_top_fields[13], "AW") != 0) 
              && (strcmp(summary_top_fields[13], "AR") != 0)) {
            state_matched = 0;
          }
      } else {
        usage(1);
     }

    }
  } 

  total_num_records++;

  /* Show all running test even if filter is given*/
  if((search_map_table && (chk_for_running && search_map_table[relative_test_run_number])) || TRNum || show_all)  {
    total_filtered_records++;
    return 1;  //  Show
  } else {
    //printf("num_projs = %d\n", num_projs);
    //proj_sub_matched = match_test_run_with_proj_subproj(proj_name, subproj_name, num_fields);

    if(given_owner_id >= 0) {
      if(buf->st_uid != given_owner_id) {
        owner_matched = 0;
      }
    }
 
    if(min_duration >= 0 ) {    /*Check for duration*/
        test_run_duration = local_get_time_from_format(summary_top_fields[14]);
        if (test_run_duration == 0) {
          test_run_duration = local_get_time_from_format(calc_duration (test_run_number, g_log_path, filename, summary_top_fields[14], 0, 0, Rflag, 1));
        
        if(test_run_duration == 0){ //Bug42189
          if(partition_flag) //partition mode
          {
            char total_time_str[MAXLENGTH + 1] = {0};
            int ret = get_duration(test_run_number, NSLogsPath, total_time_str, Rflag, 1, NULL);
            if(!ret)
            {
              test_run_duration = local_get_time_from_format(total_time_str);
            } 
          }
         }
        }
        /*      
        printf("Checking for Duration, min_duration = %d, max_duration = %d, test_run_duration = %d\n",
			min_duration, max_duration, test_run_duration); 
        */
       
        if(!(test_run_duration >= min_duration && test_run_duration <= max_duration))
          duration_matched = 0;
    }
    if(last_n_days >= 0) {/*Check for last days*/
       test_strt_time = get_summary_top_start_time_in_secs(summary_top_fields[2]); 
       /*
       printf("Checking for last days, test_strt_time = %d, last_n_days = %d, cur_time = %d\n",
				test_strt_time, last_n_days, cur_time);
       */
       if(test_strt_time + last_n_days < cur_time ) 
         last_n_days_matched = 0; 
    }
       
    // Checking for -x option. 
    if(start_time_sec > -1 && end_time_sec > -1)
    {
      // getting seconds elapsed from epoch for time which is mentioned in summary.top
      //tr_start_time_sec = format_date_get_seconds_from_epoch(summary_top_fields[2], ' '); 
      tr_start_time_sec = get_summary_top_start_time_in_secs(summary_top_fields[2]);
     
      // Don't show the details if the time of test run doesn't lie between entered start and end time. 
      if(tr_start_time_sec < start_time_sec || tr_start_time_sec > end_time_sec)
        return 0;
    }
    
    if (proj_sub_matched && owner_matched && 
      duration_matched && last_n_days_matched && scenario_matched &&
      keyword_matched && state_matched) {
      total_filtered_records++;
      return 1; // Show
    } else {
      return 0;  // Do not Show 
    }
  }
}

//BugId - 24824 - Pause/Resume Log Issue:User field is showing root user when Pause/Resume test with netstorm from top pane of webdashboard.
static inline void print_test_owner(uid_t uid, char separator, char *NSLogsPath, int test_run) {

  struct passwd *pw;
  pw = getpwuid(uid);

  if(pw) 
    printf("%c%s", separator, pw->pw_name);
  else 
    printf("%cUnavailable", separator);

}
static inline void print_test_run_notes(char *test_run_path, char separator) {

  char test_notes_path[1024];
  //char line[8192 + 1] = "NA";
  strcpy(g_notes, "NA");
  FILE *fp;
  sprintf(test_notes_path, "%s/user_notes/test_notes.txt", test_run_path);
  fp = fopen(test_notes_path, "r"); 
 
  if(fp != NULL) {
    fgets(g_notes, 8192, fp);
    g_notes[strlen(g_notes) - 1] = '\0';
    fclose(fp);
  }
  //convert_available_or_unavalable(line, separator);  // test notes
}

static inline void create_test_run_notes(char *test_run_path, char *notes)
{
  char test_notes_path[1024];
  FILE *fp;
  sprintf(test_notes_path, "%s/user_notes", test_run_path);
  mkdir(test_notes_path, 0777);
  sprintf(test_notes_path, "%s/user_notes/test_notes.txt", test_run_path);
  fp = fopen(test_notes_path, "w+");

  if(fp != NULL) {
    fwrite(notes, strlen(notes), 1, fp);
    fclose(fp);
  }
}

static void remove_tmp_summary_file()
{
  if(unlink(tmp_file_name) < 0)
  {
    fprintf(stderr, "Error in deleting file %s. Error = %s", tmp_file_name, nslb_strerror(errno));
  }
}

static int create_tmp_summary_top(FILE *fp, struct stat *summary_stat_buf, char *line, int idx, int test_run_num) 
{
  char start_time[100]; 
  char run_time[100]; 
  
  FILE *fp_tid; 

  struct tm *lt;
  time_t now;
  int seconds; 
 
  //Create a temporary summary.top file which helps in parsing
  if (getenv("NS_WDIR") != NULL) 
    sprintf(tmp_file_name, "%s/.tmp/temp_summary_top.%d", getenv("NS_WDIR"), getpid()); 
  else 
    sprintf(tmp_file_name, "/home/cavisson/work/.tmp/temp_summary_top.%d", getpid());

  //Find the running TR in table, and its corresponding netstorm.tid file path  
  if((fp_tid = open_file (test_run_info[idx].tid_file_path, "r")) == NULL) { 
    fprintf(stderr, "Error in opening %s file.\n", test_run_info[idx].tid_file_path); 
    return 1; 
  }   
  if(fstat(fileno(fp_tid), summary_stat_buf) < 0) { 
      return 1; 
  } 
  fclose(fp_tid); 
  //Create temporary summary.top file
  if((fp = fopen(tmp_file_name, "w+")) == NULL) 
  {
    fprintf(stderr, "Error in opening %s file.\n", tmp_file_name); 
    return 1; 
  }
  //Calculate modify time of netstorm.tid to get start time
  lt = localtime(&(summary_stat_buf->st_mtime));
  sprintf(start_time, "%02d/%02d/%02d %02d:%02d:%02d",  lt->tm_mon + 1, lt->tm_mday, (1900 + lt->tm_year)%2000, lt->tm_hour, lt->tm_min, lt->tm_sec);
  time(&now); 
  //Find relative time in order to get run time, 
  //run time = current time - start time 
  seconds = (int) difftime(now, mktime(lt)); 
  //Format for run time HH:MM:SS
  strcpy(run_time, format_time(seconds)); 
  //Format: Test Run|Scenario Name|Start Time|Report Summary|Page Dump|Report Progress|Report Detail|Report User|Report Fail|Report Page Break Down|WAN_ENV|REPORTING|Test Name|Test Mode|Run Time|Virtual Users
  fprintf(fp, "%d|default/default/unknown|%s|N|N|N|N|N|N|N|0|0|summary.top file not found|RU|%s|0\n", test_run_num, start_time, run_time); 
  rewind(fp);
  if(fgets(line, 8192, fp) == NULL)
  {
      fclose(fp);
      return 1;  // Ignore as we did not find line in summary.top 
   }
   fclose(fp);
   temp_summary_file_created = 1;
   return 0; 
}


int Compare( const void *partition1, const void *partition2 )
{
  char *Partition1 = (char*)partition1;
  char *Partition2 = (char*)partition2;

  long long val1 = atoll(Partition1);
  long long val2 = atoll(Partition2);

  if(val2 < val1)
    return -1;
  else if (val1 == val2)
    return 0;
  else
    return 1;
}

/* Return 0 on failure if found character, 1 on success if all numeric
 * do following checking before considering passed dir partition
 * (1) length should be 14
 * (2) whole string should be numeric */
#if 0
static int is_partition(char *str)
{
  int i;

  // check length, if not 14 then return 0
  if(strlen(str) != PARTITION_LENGTH - 1)
    return 0;

  for(i = 0; i < strlen(str); i++) {
    if(!isdigit(str[i])) return 0;
  }
  return 1;
}
#endif

//Project|Subproject|Test Run|Test Name|Scenario Name|Start Time|Run Time|Virtual Users|Report Summary|Report Progress|Report Detail|Report User|Report Fail|Report Page Break Down|WAN_ENV|REPORTING|Test Mode|Page Dump|Notes|Started By| (type (T for normal  testrun and S for CMT session))TR| TR size | TR remove date|graph_data|size | remove date | csv | size | remove date | raw_file | size | remove date | db | size | remove date | key_file | size | remove date | har file |size | remove date | page dump | size | remove date | logs | size | remove date | test_data | size | remove date
#if 0
void print_v2_info (char *proj_name, char *subproj, char *scen_name, char *scen_name, char *sum_top_file, char *fields[], int test_run_no, int partition_flag, char *NSlogsPath) {

  printf ("%s|%s|%s|%s|%s|%s|", proj_name, subproj, fields [0], 
                            fields [12] ? fields[12] : "NA", 
                            scen_name,
                            fields [2] ? fields[2] : "NA",
                                                         
                         );
  
    duration = get_tm_from_format (fields[14]);
    if (duration == 0) {
      strcpy (str_duration, calc_duration (test_run_number, g_log_path, sum_top_file, fields[14], 0, 0, Rflag, 1));
      convert_available_or_unavalable(str_duration, separator, CONVERT);      //calculated Run Time
    } else {
       convert_available_or_unavalable(fields[14], separator, CONVERT);      //Run Time
    }
  convert_available_or_unavalable(fields[12], '|', DO_NOT_CONVERT);   //Test Name
}
#endif

static inline void print_fields(char *proj_name, char *subproj_name, char *scen_name, char *sum_top_file, 
                                char *fields[], struct stat summary_stat_buf, int test_run_number, int partition_flag, char *NSLogsPath)
{
  int duration;
  int ret;
  char str_duration[MAXLENGTH + 1];
  char total_time_str[MAXLENGTH + 1] = {0};
  char file_name[1024] = {0};

  if(Cflag)
    printf("%s%c", ctrl_name, separator);

  printf("%s%c", proj_name, separator);                     //Project
  printf("%s%c", subproj_name, separator);                  //Subproject
    
  printf("%s", fields[0]);                                  //Test Run

  convert_available_or_unavalable(fields[12], separator, DO_NOT_CONVERT);   //Test Name
  printf("%c%s", separator, scen_name);                     //Scenario Name

  convert_available_or_unavalable(fields[2], separator, CONVERT);    //Start Time
  
  if(partition_flag) //partition mode
  { 
    ret = get_duration(test_run_number, NSLogsPath, total_time_str, Rflag, 1, NULL);
    if(ret == -1)
    {
      printf("%c   NA   ", separator);
    }
    else if(ret == -2)
    {
      convert_available_or_unavalable(total_time_str, separator, CONVERT);
      sprintf(file_name, "%sTR%d", NSLogsPath, test_run_number);
      create_test_run_notes(file_name, "Duration may not be correct because testrun.gdf file is not in proper sequence.\n");
    }
    else
      convert_available_or_unavalable(total_time_str, separator, CONVERT);  
  }
  else // non partition mode
  { 
    //3.7.7 changes to calculate duration for aborted tests
    duration = get_tm_from_format (fields[14]);
    if (duration == 0) {
      strcpy (str_duration, calc_duration (test_run_number, g_log_path, sum_top_file, fields[14], 0, 0, Rflag, 1));
      convert_available_or_unavalable(str_duration, separator, CONVERT);      //calculated Run Time
    } else {
       convert_available_or_unavalable(fields[14], separator, CONVERT);      //Run Time
    }
    //convert_available_or_unavalable(fields[14], separator);   //Run Time
  }
 
  convert_available_or_unavalable(fields[15], separator, CONVERT);   //Virtual Users

  if(Long_form)
  {
    convert_available_or_unavalable(fields[3], separator, CONVERT);  //Report Summary
    if(!strcmp(fields[5],"Y") || !strcmp(fields[5],"N") || !strcmp(fields[5],"Available") || !strcmp(fields[5],"Unavailable"))
      convert_available_or_unavalable(fields[5], separator, CONVERT);  //Report Progress
    else
      convert_available_or_unavalable("Available", separator, DO_NOT_CONVERT);  //Report Progress
       
    convert_available_or_unavalable(fields[6], separator, CONVERT);  //Report Detail
    convert_available_or_unavalable(fields[7], separator, CONVERT);  //Report User
    convert_available_or_unavalable(fields[8], separator, CONVERT);  //Report Fail
    convert_available_or_unavalable(fields[9], separator, CONVERT);  //Report Page Break Down
    convert_available_or_unavalable(fields[10], separator, CONVERT); //WAN_ENV
    convert_available_or_unavalable(fields[11], separator, CONVERT); //REPORTING

    // Check Boundry COndtions
    if(((test_run_number - INIT_TEST_RUN_NUMBER) < max_test_id) && search_map_table && search_map_table[test_run_number - INIT_TEST_RUN_NUMBER])  {
      convert_available_or_unavalable("RU", separator, CONVERT);     //Test Mode
    } else {
      convert_available_or_unavalable(fields[13], separator, CONVERT); //Test Mode
    }

    convert_available_or_unavalable(fields[4], separator, CONVERT);  //Page Dump
    //print_test_run_notes(dirname(file_name), separator);
    convert_available_or_unavalable(g_notes, separator, DO_NOT_CONVERT);    // test notes
    
    if(!strcmp(fields[5],"Y") || !strcmp(fields[5],"N") || !strcmp(fields[5],"Available") || !strcmp(fields[5],"Unavailable"))
      print_test_owner(summary_stat_buf.st_uid, separator, NSLogsPath, test_run_number);
    else
      convert_available_or_unavalable(fields[5], separator, DO_NOT_CONVERT);  //Page Dump
      
  }
  if(Cflag) {
    printf("%c%s", separator, tomcat_port);
    printf("%c%s", separator, cmd_out);
  }
}

//This function will check whether Test is ND or not
int check_test_is_nd(char *NSLogsPath, int test_run_number)
{
  struct stat sorted_scenario;
  char scenario_files_path[100];
  char buf[128];
  char last_line_buf[128];
  int is_test_nd = 0;
  last_line_buf[0]= '\0';
  sprintf(scenario_files_path, "%s/TR%d/scenario", NSLogsPath, test_run_number);
  if(stat(scenario_files_path, &sorted_scenario) == 0)
  {
    sprintf(buf, "grep ^NET_DIAGNOSTICS_SERVER %s | cut -d ' ' -f2", scenario_files_path);
    nslb_run_cmd_and_get_last_line(buf, 128, last_line_buf);
    if(last_line_buf[0] != '\0')
    {
      is_test_nd = atoi(last_line_buf);
      if(is_test_nd != 0)
        return 0;
      else
        return 1;
    }
  }
  return 1;
}

//To read summary.top file and replace Y to Available and N to Unavailable if found
int read_summary_top(char *file_name, int test_run_num, char *NSLogsPath, char **user, int num_users, int uflag)
{
  FILE *fp;
  struct stat summary_stat_buf;
  struct stat summary_top_file;
  char common_files_path[2024] = {0};
  char line[8192];
  char *fields[500] = {0};
  int totalFlds;
  char proj_name[100]="\0";
  char subproj_name[100]="\0";
  char scen_name[100]="\0";
  int test_run_number;
  int duration;
  char str_duration[100];
  char sum_top_file[MAXLENGTH + 1]; 
  char total_time_str[MAXLENGTH + 1] = {0};
  static int RLheader_flag = 0;
  int ret, i = 0, flag_set = 0, partition_flag = 0;
  char *null_field = "NA";

  //printf("Reading %s\n", file_name);
  if((fp = open_file (file_name, "r")) == NULL) { 
    //In case of running TR, if summary.top not found then we will create a temporary file 
    //with default values of summary.top fields
    //Find the running TR in table, and its corresponding netstorm.tid file path  
    flag_set = is_testrun_running(test_run_num);

    if (flag_set) {
      if((ret = create_tmp_summary_top(fp, &summary_stat_buf, line, i, test_run_num)) == 1)
        return 1;
    } else
      return 1;  // Ignore this test run dont have summary.top
  } else { 
    strcpy (sum_top_file, file_name);
    if(fgets(line, 8192, fp) == NULL)
    {
      fclose(fp);
      return 1;  // Ignore as we did not find line in summary.top 
    }
    /* Not able to get stat 
     * The function fileno() examines the argument stream and
     *  returns its integer descriptor.
     */  
    if(fstat(fileno(fp), &summary_stat_buf) < 0) {
      return 1;   // Ignore as we did not find as something wrong with fp
    }
    fclose(fp);
  }
  //printf("summary line = %s\n", line);
  if (temp_summary_file_created)
    strcpy (sum_top_file, tmp_file_name);
  if(line[strlen(line) -1] == '\n')
    line[strlen(line) -1]='\0';

  totalFlds = get_tokens(line, fields, "|", 20);  

  /* summary.top has 16 pipe seperated fields,
     Test Run|Scenario Name|Start Time|Report Summary|Page Dump|Report Progress|Report Detail|Report User|Report Fail|Report Page Break Down|WAN_ENV|REPORTING|Test Name|Test Mode|Run Time|Virtual Users
     we are ignoring test runs those have less then 16 fields.
     Because some test run has summary.top with no data between pipe. 
     like :
     1888|automation_proj/auto_subproj/ut_arun_keyword_test|2/24/09  11:50:44|Y|Unavailable|Y|Unavailable|Unavailable|Unavailable|Unavailable|0|1|1888||00:00:14|10
     it has 15 fields, so core dump was occurring because it try to print 16th value i.e index 15  
  */

  /* in case of conitnuous monitoring due to any corruption in summary.top , nsu_show_netstorm doest not give output while test is running, 
     due to this two pid's was generated so commenting this code to avoid running of two test with same test run.
     Bug 75955: If fields are less than max possible fields then replaced with NA
  */
  if ((totalFlds < MAX_SUMMARY_TOP_FIELDS))
  {
    //return 1; // Less fields ignore this test run
    for(i = totalFlds ; i < MAX_SUMMARY_TOP_FIELDS; i++)
      fields[i] = null_field;
  }
  
  
  test_run_number = atoi(fields[0]);

  //Compare test_type is given ND by user or not
  if(strcasecmp(test_type, "ND") == 0)
  { 
    ret = check_test_is_nd(NSLogsPath, test_run_number);
    if(ret == 1)
      return 0; //Test is Non-ND 
  }

  //printf("fields[1] = %s\n", fields[1]);
  if(uflag != 0) {

    for ( i = 0; i < num_users; i++)
    {
      if (!strcmp(user[i], fields[5]))
      {
        break;
      }
    }
    if ( i == num_users)
     return 1;
  }

  //if partition mode then set partition_flag
  sprintf(common_files_path, "%s/TR%d/common_files", NSLogsPath, test_run_number);
  if(stat(common_files_path, &summary_top_file) == 0) //partition mode
    partition_flag = 1;
  if (show_test_logs(proj_name, subproj_name,
                          scen_name, fields, &summary_stat_buf,
                          test_run_number, file_name, partition_flag, NSLogsPath) == 0) {
    return 1;  
  }
  /*
  //if partition mode then set partition_flag
  sprintf(common_files_path, "%s/TR%d/common_files", NSLogsPath, test_run_number);
  if(stat(common_files_path, &summary_top_file) == 0) //partition mode
    partition_flag = 1;
  */
  if (Rflag == 1 && LLflag == 1) {

    if (RLheader_flag == 0) {
      separator = '\t';
      printf("TestRun\tStatus\tStart Date and Time\tElapsed Time\tOwner\t\tProject/Subproject/Scenario\n");
      RLheader_flag = 1;
    }

    printf("%s\t", fields[0]);                              //Test Run
    printf("Running");                                 //Status
    convert_available_or_unavalable(fields[2], '\t', CONVERT);       //Start Time

    if(partition_flag) //partition mode
    { 
      ret = get_duration(test_run_number, NSLogsPath, total_time_str, Rflag, 1, NULL);      
      if(ret == -2)
      {
        convert_available_or_unavalable(total_time_str, separator, CONVERT);
        create_test_run_notes(file_name, "Duration may not be correct because testrun.gdf file is not in proper sequence.\n");
      }
      if(ret == -1)
      {
        printf("%c   NA   ", separator);
      }
      else
        convert_available_or_unavalable(total_time_str, separator, CONVERT);  
    }
    else // non partition mode
    { 
      //marker ,3.7.7 change  
      duration = get_tm_from_format (fields[14]);
      if (duration == 0) {
        strcpy (str_duration, calc_duration (test_run_number, g_log_path, sum_top_file, fields[14], 0, 0, Rflag, 1));
        convert_available_or_unavalable(str_duration, '\t', CONVERT);      //calculated Run Time
      } else {
        convert_available_or_unavalable(fields[14], '\t', CONVERT);      //Run Time
      }
    }

    if(!strcmp(fields[5],"Y") || !strcmp(fields[5],"N") || !strcmp(fields[5],"Available") || !strcmp(fields[5],"Unavailable"))
      print_test_owner(summary_stat_buf.st_uid, separator, NSLogsPath, test_run_number);
    else
      convert_available_or_unavalable(fields[5], separator, DO_NOT_CONVERT);  //Page Dump
    printf("\t%s/%s/%s", proj_name, subproj_name, scen_name);                              //Project
    //printf("%s\t", subproj_name);                           //Subproject
    //printf("%s\t", scen_name);                              //Scenario Name
  } else {
    print_fields(proj_name, subproj_name, scen_name, sum_top_file, fields, summary_stat_buf, test_run_number, partition_flag, NSLogsPath);
  }
    
  printf("\n"); 
  return 0;
}


/*
 *Description: To filter directories starting with TR in scandir.
 *inputs : *a - DIR structure.
 *outputs : 1 on succes ,0 on failure.
 *error : none
 *algo : simple
*/
int only_tr(const struct dirent *a) 
{
  if ( (a->d_name[0] == 'T') && (a->d_name[1] == 'R') )
    return 1;
  else
    return 0;
}

/*
 *Description : Comparison function for scandir
 *inputs : **a, **b - pointers to DIR structure. 
 *outputs : 1 on succes ,0 on failure
 *error : none
 *algo : simple
*/
#if ( (Fedora && RELEASE >= 14) || (Ubuntu && RELEASE >= 1204) )
int my_alpha_sort(const struct dirent **aa, const struct dirent **bb) 
#else
int my_alpha_sort(const void *aa, const void *bb)
#endif
{
  const struct dirent **a = (const struct dirent **) aa;
  const struct dirent **b = (const struct dirent **) bb;
            
  //fprintf (stderr, "(a)->d_name  = %s, (b)->d_name = %s\n", (*a)->d_name, (*b)->d_name);
  if (atoi((*a)->d_name + 2) < atoi((*b)->d_name + 2))
    return 1;
  else
    return 0;
}
#if 0
int check_if_dir(char *name)
{
  struct stat info;
  if(!stat(name, &info) && S_ISDIR(info.st_mode))
    return 1;
  return 0;
}

#define IS_DIR(X)	(check_if_dir(X->d_name))
#endif

char *convert_char_field_to_actual_val (char val) 
{
  return val ? (val == 'Y' ? "Available"  : (val == 'W' ? "W" : "Unavailable" )): "NA";
}


static int ts_comp(const void *p1, const void *p2)
{
  return ((struct cdr_cache_entry *)p2)->start_time_ts - ((struct cdr_cache_entry *)p1)->start_time_ts;
}

static void convert_available_or_unavalable_cache(char *str, int convert_flag)
{
  if(!str || !str[0])
  {
    sprintf(str, "NA");
    return;
  }

  if(convert_flag == DO_NOT_CONVERT)
  {
    return;
  }

  if(!strcmp(str, "Y"))
    sprintf(str, "Available");
  else if(!strcmp(str, "N"))
    sprintf(str, "Unavailable");
}

static void update_duration(struct cdr_cache_entry *cdr_cache_entry_list)
{
  int duration;
  char sum_top_file[512];
  char common_files_path[512];
  int partition_flag = 0;
  struct stat st;
  int ret;
  //if partition mode then set partition_flag
  sprintf(common_files_path, "%s/logs/TR%d/common_files", ns_wdir, cdr_cache_entry_list->tr_num);
  if(stat(common_files_path, &st) == 0) //partition mode
    partition_flag = 1;

  sprintf(sum_top_file, "%s/logs/TR%d/summary.top", ns_wdir, cdr_cache_entry_list->tr_num);

  if(partition_flag)
  {
    sprintf(common_files_path, "%s/logs/", ns_wdir);
    ret = get_duration(cdr_cache_entry_list->tr_num, common_files_path, cdr_cache_entry_list->runtime, Rflag, 1, NULL);
    if(ret == -1)
    {
      sprintf(cdr_cache_entry_list->runtime, "   NA   ");
    }
    else if(ret == -2)
    {
      convert_available_or_unavalable_cache(cdr_cache_entry_list->runtime, CONVERT);
      sprintf(common_files_path, "%s/logs/TR%d", ns_wdir, cdr_cache_entry_list->tr_num);
      create_test_run_notes(common_files_path, "Duration may not be correct because testrun.gdf file is not in proper sequence.\n");
    }
    else
      convert_available_or_unavalable_cache(cdr_cache_entry_list->runtime, CONVERT);
    return;
  }
  
  duration = get_tm_from_format (cdr_cache_entry_list->runtime);

  if (duration == 0) {
    strcpy (cdr_cache_entry_list->runtime, 
              calc_duration (cdr_cache_entry_list->tr_num, g_log_path, sum_top_file, cdr_cache_entry_list->runtime, 0, 0, Rflag, 1));
    convert_available_or_unavalable_cache(cdr_cache_entry_list->runtime, CONVERT);      //calculated Run Time
  } 
  else {
    convert_available_or_unavalable_cache(cdr_cache_entry_list->runtime, CONVERT);      //Run Time
  }
}

int cdr_cache_appy_filter(struct cdr_cache_entry *cdr_cache_entry_list, char *project, char *subProject, char *scen_name)
{
  if((cdr_cache_entry_list->tr_type != RUNNING_TR && cdr_cache_entry_list->tr_type != CMT_TR))
  {
    // duration filter
    if (Dflag) 
    { 
      long long ts_temp = 0;
      ts_temp = (cdr_cache_entry_list->end_time_stamp - cdr_cache_entry_list->start_time_ts) * 1000;
      if(!(ts_temp >= min_duration && ts_temp <= max_duration))
        return 0;
    }

    //project, subproject filter
    if(!match_test_run_with_proj_subproj(project, subProject, 3))
      return 0;
  
    //last n days filter  
    if(last_n_days >= 0) {
      if(cdr_cache_entry_list->start_time_ts + last_n_days < cur_time )
        return 0;
    }

    //filter  test runs by scenario name
    if (g_scenario[0] != '\0') {
      int scenario_matched = 1;
      if (strcmp(scen_name, g_scenario) != 0) {
        scenario_matched = 0;
      }
      //If not matched then there is possibility
      //that we have . in scenario name in summary.top file(when run from test suite)
      //We need to pass that one also so if we fail in matching then check with
      //adding . at starting
      if(scenario_matched == 0)
      {
        char g_scen_tmp[100];
        strcpy(g_scen_tmp, ".");
        strcat(g_scen_tmp, g_scenario);
        if (strcmp(scen_name, g_scen_tmp) != 0)
          return 0;
      }
      return 1;
    }

     if (g_mode[0] != '\0') {
       if (strcmp(g_mode, "locked") == 0) {
         if (strcmp(cdr_cache_entry_list->test_mode, "R" ) != 0) {
           return 0;
         }
       } else if (strcmp(g_mode,"unlocked") == 0) {
         if (strcmp(cdr_cache_entry_list->test_mode, "W") != 0) {
           return 0;
         }
       } else if (strcmp(g_mode, "archived") == 0) {
           if ((strcmp(cdr_cache_entry_list->test_mode, "AW") != 0)
               && (strcmp(cdr_cache_entry_list->test_mode, "AR") != 0)) {
             return 0;
           }
       } else {
         usage(1);
      }
    }

    //any string to search
    int i ;
    if(num_search_key) {  // at least one key given
      char tr_num_str[16];
      sprintf(tr_num_str, "%d", cdr_cache_entry_list->tr_num);
      for(i=0;i<num_search_key;i++){
        if(strcasestr(scen_name, search_keys[i]) ||
           strcasestr(tr_num_str, search_keys[i]) ||    // test run number
           strcasestr(cdr_cache_entry_list->test_name, search_keys[i]) ||  // tname
           strcasestr(g_notes, search_keys[i]))
           return 1;
      }
        return 0;
    } 
  }
  return 1;
}

void support_for_new_options () 
{
  cdr_init(0);
 
  if(cdr_read_cache_file(0) == CDR_ERROR)
  {
    CDRTL1("Error: cache file read failed");
  }

  if (cdr_session_flag[0])
  {
    if (cdr_process_config_file() == CDR_ERROR) // processing of config.json
    {
      CDRTL1("Error: error in reading config.json file, exiting ...");
      exit(CDR_ERROR);
    }
    update_running_test_list();
    int id = create_cache_entry_from_tr(TRNum, -1, 1);

    if (id  < 0) 
    {
      printf ("TR%d is bad TR \n", TRNum);
      return;
    }


    if (strcmp (cdr_session_flag, "S") == 0)
    {
      if(cdr_read_cmt_cache_file(0) == CDR_ERROR) 
        CDRTL1("Error: cache file read failed");

      printf ("Partition_num|partition disk size|graph_data|size|remove date|csv|size|remove date|raw_file|size|remove date|"
              "db|size|remove date|har file|size|remove date|page dump|size|remove date|logs|size|remove date\n");

      cdr_get_cmt_partition_from_disk();

      for(int i = 0; i < total_cmt_cache_entry; ++i) 
      {
        if (cdr_cmt_cache_entry_list[i].partition_num)
        {
          printf ("%lld|%.2f", cdr_cmt_cache_entry_list[i].partition_num,
                               cdr_cmt_cache_entry_list[i].partition_disk_size/(1024.0*1024.0));

          printf ("|graph_data|%.2f|%s", cdr_cmt_cache_entry_list[i].partition_graph_data_size/(1024.0*1024.0), 
                                         format_time_ex(cdr_cmt_cache_entry_list[i].partition_graph_data_remove_f));


          printf ("|csv|%.2f|%s", cdr_cmt_cache_entry_list[i].partition_csv_size/(1024.0*1024.0), 
                                  format_time_ex(cdr_cmt_cache_entry_list[i].partition_csv_remove_f));
 
          printf ("|raw_file|%.2f|%s", cdr_cmt_cache_entry_list[i].partition_raw_file_size/(1024.0*1024.0), 
                                       format_time_ex(cdr_cmt_cache_entry_list[i].partition_raw_file_remove_f));

          printf ("|db|%.2f|%s", (cdr_cmt_cache_entry_list[i].partition_db_table_size + cdr_cmt_cache_entry_list[i].partition_db_index_size)/(1024.0*1024.0), 
                format_time_ex(cdr_cmt_cache_entry_list[i].partition_db_remove_f));
 
      
          printf ("|har_file|%.2f|%s", cdr_cmt_cache_entry_list[i].partition_har_file_size/(1024.0*1024.0), 
                                       format_time_ex(cdr_cmt_cache_entry_list[i].partition_har_file_remove_f));

          printf ("|page_dump|%.2f|%s", cdr_cmt_cache_entry_list[i].partition_page_dump_size/(1024.0*1024.0), 
                                        format_time_ex(cdr_cmt_cache_entry_list[i].partition_page_dump_remove_f));

          printf ("|logs|%.2f|%s\n", cdr_cmt_cache_entry_list[i].partition_logs_size/(1024.0*1024.0), 
                                   format_time_ex(cdr_cmt_cache_entry_list[i].partition_logs_remove_f));
        }
      }
    }
    else 
    {  
      char *ptr = strchr (cdr_cache_entry_list[id].scenario_name, '/');
      if (ptr) 
        *ptr = '\0';
      
      char *ptr1 = strchr(ptr + 1, '/');
      if (ptr1)
        *ptr1 = '\0';
      ++ ptr1;
      
      char buf [1024];
      sprintf (buf, "%s/logs/TR%d", wdir, cdr_cache_entry_list[id].tr_num);
      print_test_run_notes (buf, ' ');
     
      printf("%s|%s|%d|%s|%s|%s|%s|%d|%s", 
                                    cdr_cache_entry_list[id].scenario_name,
                                    ptr+1,
                                    cdr_cache_entry_list[id].tr_num,
                                    cdr_cache_entry_list[id].test_name,
                                    ptr1,  
                                    cdr_cache_entry_list[id].start_time,
                                    cdr_cache_entry_list[id].runtime,
                                    cdr_cache_entry_list[id].vusers,
                                    convert_char_field_to_actual_val (cdr_cache_entry_list[id].report_summary));

      if(!strcmp(cdr_cache_entry_list[id].report_progress, "Y") || !strcmp(cdr_cache_entry_list[id].report_progress, "N") || 
             !strcmp(cdr_cache_entry_list[id].report_progress , "Available") || !strcmp(cdr_cache_entry_list[id].report_progress, "Unavailable"))
        convert_available_or_unavalable(cdr_cache_entry_list[id].report_progress, '|', CONVERT);  //Report Progress
      else
        convert_available_or_unavalable("Available", '|', DO_NOT_CONVERT);

      printf ("|%s|%s|%s|%s|%d|%d",
                  convert_char_field_to_actual_val(cdr_cache_entry_list[id].report_detail),
                  convert_char_field_to_actual_val(cdr_cache_entry_list[id].report_user),
                  convert_char_field_to_actual_val(cdr_cache_entry_list[id].report_fail),
                  convert_char_field_to_actual_val(cdr_cache_entry_list[id].report_page_break_down),
                  cdr_cache_entry_list[id].wan_env,
                  cdr_cache_entry_list[id].reporting);

      if (cdr_cache_entry_list[id].tr_type == RUNNING_TR)
        printf ("|RU");
      else 
        printf("|%s", cdr_cache_entry_list[id].test_mode);
      
      printf ("|%s|%s|%s|%cTR",
                  convert_char_field_to_actual_val (cdr_cache_entry_list[id].page_dump),
                  g_notes,
                  cdr_cache_entry_list[id].report_progress ? ((cdr_cache_entry_list[id].report_progress[0] == 'Y' && cdr_cache_entry_list[id].report_progress[1] == '\0')? "Available" : 
                  (cdr_cache_entry_list[id].report_progress[0] == 'N' && cdr_cache_entry_list[id].report_progress[1] == '\0')? "Unavailable":  cdr_cache_entry_list[id].report_progress) : "NA",
                  cdr_cache_entry_list[id].tr_type == 1 ? 'S' : 'T');

      printf("|%.2f|%s", cdr_cache_entry_list[id].tr_disk_size == 0 ? -1 : cdr_cache_entry_list[id].tr_disk_size/(1024.0*1024.0),
                                    format_time_ex(cdr_cache_entry_list[id].remove_tr_f));

      printf("|graph_data|%.2f|%s", cdr_cache_entry_list[id].graph_data_size == 0 ? -1 : cdr_cache_entry_list[id].graph_data_size/(1024.0*1024.0),
                                    format_time_ex(cdr_cache_entry_list[id].graph_data_remove_f));

      printf("|csv|%.2f|%s", cdr_cache_entry_list[id].csv_size == 0 ? -1 : cdr_cache_entry_list[id].csv_size/(1024.0*1024.0),
                                    format_time_ex(cdr_cache_entry_list[id].csv_remove_f));

      printf("|raw_file|%.2f|%s", cdr_cache_entry_list[id].raw_file_size == 0 ? -1 : cdr_cache_entry_list[id].raw_file_size/(1024.0*1024.0),
                                    format_time_ex(cdr_cache_entry_list[id].raw_file_remove_f));

      printf("|db|%.2f|%s", cdr_cache_entry_list[id].tr_db_table_size == 0 ? -1 : cdr_cache_entry_list[id].tr_db_table_size/(1024.0*1024.0),
                                    format_time_ex(cdr_cache_entry_list[id].tr_db_remove_f));

      printf("|key_file|%.2f|%s", cdr_cache_entry_list[id].key_file_size == 0 ? -1 : cdr_cache_entry_list[id].key_file_size/(1024.0*1024.0),
                                    format_time_ex(cdr_cache_entry_list[id].key_file_remove_f));

      printf("|har_file|%.2f|%s", cdr_cache_entry_list[id].har_file_size == 0 ? -1 : cdr_cache_entry_list[id].har_file_size/(1024.0*1024.0),
         	     	            format_time_ex(cdr_cache_entry_list[id].har_file_remove_f));

      printf("|page_dump|%.2f|%s", cdr_cache_entry_list[id].page_dump_size == 0 ? -1 : cdr_cache_entry_list[id].page_dump_size/(1024.0*1024.0),
                                    format_time_ex(cdr_cache_entry_list[id].page_dump_remove_f));

      printf("|logs|%.2f|%s", cdr_cache_entry_list[id].logs_size == 0 ? -1 : cdr_cache_entry_list[id].logs_size/(1024.0*1024.0),
                                    format_time_ex(cdr_cache_entry_list[id].logs_remove_f));

      printf("|test_data|%.2f|%s\n", cdr_cache_entry_list[id].test_data_size == 0 ? -1 : cdr_cache_entry_list[id].test_data_size/(1024.0*1024.0),
                                    format_time_ex(cdr_cache_entry_list[id].test_data_remove_f));
      set_cleanup_policy(); 
      tr_in_remove_range(&(cdr_cache_entry_list[id]));
      component_in_remove_range(& (cdr_cache_entry_list[id]));
    }

   return;
  }

  cdr_get_tr_list_from_disk (0);

  qsort (cdr_cache_entry_list, total_cache_entry , sizeof (*cdr_cache_entry_list), ts_comp);

  int filtered = 0;

  for (int id = 0; id < total_cache_entry;  ++ id)
  {
   
    if (cdr_cache_entry_list[id].is_tr_present == CDR_TRUE)
    {
      char tmp_scenario_name [1024];
      sprintf(tmp_scenario_name, "%s", cdr_cache_entry_list[id].scenario_name);

      char *ptr = strchr (tmp_scenario_name, '/');
      if (ptr) 
        *ptr = '\0';
      
      char *ptr1 = strchr(ptr + 1, '/');
      if (ptr1)
        *ptr1 = '\0';
      ++ ptr1;

      char buf [1024];
      sprintf (buf, "%s/logs/TR%d", wdir, cdr_cache_entry_list[id].tr_num);
      print_test_run_notes (buf, ' ');

      //Apply filter
      if (!cdr_cache_appy_filter(&(cdr_cache_entry_list[id]), tmp_scenario_name, ptr + 1, ptr1))
        continue;
        
      filtered ++;

      update_duration(&(cdr_cache_entry_list[id]));
     
      printf("%s|%s|%d|%s|%s|%s|%s|%d|%s", 
                                    tmp_scenario_name,
                                    ptr+1,
                                    cdr_cache_entry_list[id].tr_num,
                                    cdr_cache_entry_list[id].test_name,
                                    ptr1,  
                                    cdr_cache_entry_list[id].start_time,
                                    cdr_cache_entry_list[id].runtime,
                                    cdr_cache_entry_list[id].vusers,
                                    convert_char_field_to_actual_val (cdr_cache_entry_list[id].report_summary));

      if(!strcmp(cdr_cache_entry_list[id].report_progress, "Y") || !strcmp(cdr_cache_entry_list[id].report_progress, "N") || 
             !strcmp(cdr_cache_entry_list[id].report_progress , "Available") || !strcmp(cdr_cache_entry_list[id].report_progress, "Unavailable"))
        convert_available_or_unavalable(cdr_cache_entry_list[id].report_progress, '|', CONVERT);  //Report Progress
      else
        convert_available_or_unavalable("Available", '|', DO_NOT_CONVERT);

      printf ("|%s|%s|%s|%s|%d|%d",
                  convert_char_field_to_actual_val(cdr_cache_entry_list[id].report_detail),
                  convert_char_field_to_actual_val(cdr_cache_entry_list[id].report_user),
                  convert_char_field_to_actual_val(cdr_cache_entry_list[id].report_fail),
                  convert_char_field_to_actual_val(cdr_cache_entry_list[id].report_page_break_down),
                  cdr_cache_entry_list[id].wan_env,
                  cdr_cache_entry_list[id].reporting);

      if (cdr_cache_entry_list[id].tr_type == RUNNING_TR)
        printf ("|RU");
      else if (cdr_cache_entry_list[id].tr_type == CMT_TR)
      {
        if(tr_is_running(cdr_cache_entry_list[id].tr_num))
          printf ("|RU");
        else
          printf("|%s", cdr_cache_entry_list[id].test_mode);

      }
      else 
        printf("|%s", cdr_cache_entry_list[id].test_mode);
      
      printf ("|%s|%s|%s|%cTR",
                  convert_char_field_to_actual_val (cdr_cache_entry_list[id].page_dump),
                  g_notes,
                  cdr_cache_entry_list[id].report_progress ? ((cdr_cache_entry_list[id].report_progress[0] == 'Y' && cdr_cache_entry_list[id].report_progress[1] == '\0')? "Available" : 
                  (cdr_cache_entry_list[id].report_progress[0] == 'N' && cdr_cache_entry_list[id].report_progress[1] == '\0')? "Unavailable":  cdr_cache_entry_list[id].report_progress) : "NA",
                  cdr_cache_entry_list[id].tr_type == 1 ? 'S' : 'T');

      printf("|%.2f|%s", cdr_cache_entry_list[id].tr_disk_size == 0 ? -1 : cdr_cache_entry_list[id].tr_disk_size/(1024.0*1024.0),
                                    format_time_ex(cdr_cache_entry_list[id].remove_tr_f));

      printf("|graph_data|%.2f|%s", cdr_cache_entry_list[id].graph_data_size == 0 ? -1 : cdr_cache_entry_list[id].graph_data_size/(1024.0*1024.0),
                                    format_time_ex(cdr_cache_entry_list[id].graph_data_remove_f));

      printf("|csv|%.2f|%s", cdr_cache_entry_list[id].csv_size == 0 ? -1 : cdr_cache_entry_list[id].csv_size/(1024.0*1024.0),
                                    format_time_ex(cdr_cache_entry_list[id].csv_remove_f));

      printf("|raw_file|%.2f|%s", cdr_cache_entry_list[id].raw_file_size == 0 ? -1 : cdr_cache_entry_list[id].raw_file_size/(1024.0*1024.0),
                                    format_time_ex(cdr_cache_entry_list[id].raw_file_remove_f));

      printf("|db|%.2f|%s", cdr_cache_entry_list[id].tr_db_table_size == 0 ? -1 : cdr_cache_entry_list[id].tr_db_table_size/(1024.0*1024.0),
                                    format_time_ex(cdr_cache_entry_list[id].tr_db_remove_f));

      printf("|key_file|%.2f|%s", cdr_cache_entry_list[id].key_file_size == 0 ? -1 : cdr_cache_entry_list[id].key_file_size/(1024.0*1024.0),
                                    format_time_ex(cdr_cache_entry_list[id].key_file_remove_f));

      printf("|har_file|%.2f|%s", cdr_cache_entry_list[id].har_file_size == 0 ? -1 : cdr_cache_entry_list[id].har_file_size/(1024.0*1024.0),
         	     	            format_time_ex(cdr_cache_entry_list[id].har_file_remove_f));

      printf("|page_dump|%.2f|%s", cdr_cache_entry_list[id].page_dump_size == 0 ? -1 : cdr_cache_entry_list[id].page_dump_size/(1024.0*1024.0),
                                    format_time_ex(cdr_cache_entry_list[id].page_dump_remove_f));

      printf("|logs|%.2f|%s", cdr_cache_entry_list[id].logs_size == 0 ? -1 : cdr_cache_entry_list[id].logs_size/(1024.0*1024.0),
                                    format_time_ex(cdr_cache_entry_list[id].logs_remove_f));

      printf("|test_data|%.2f|%s\n", cdr_cache_entry_list[id].test_data_size == 0 ? -1 : cdr_cache_entry_list[id].test_data_size/(1024.0*1024.0),
                                    format_time_ex(cdr_cache_entry_list[id].test_data_remove_f));
    }
  }
  if (Gflag)
    printf ("Total Filtered Records: %d, Total Records: %d\n", filtered, total_cache_entry);

}

void show_all_test_runs(char *NSLogsPath, char **user, int num_users, int uflag)
{
  char file_name[2024];
  struct dirent **namelist;
  DIR *dir_fp = opendir(NSLogsPath);
  int n;
  char *ptr = NULL;
  
  if (dir_fp == NULL) {
     fprintf(stderr, "Unable to open dir (%s) for reading.\n", nslb_strerror(errno));
     exit(1);
  }

  if (cdr_flag || cdr_session_flag[0])
  {
    support_for_new_options ();
    return; 
  } 
  
  //n = scandir (NSLogsPath, &namelist, only_tr, (int (*)(const struct dirent **, const struct dirent **))my_alpha_sort);
  n = scandir (NSLogsPath, &namelist, only_tr, my_alpha_sort);
  if (n < 0)
    perror ("scandir");
  else {
    while (n--) {
      if (namelist[n]->d_name[0] == 'T' && namelist[n]->d_name[1] == 'R' && nslb_get_file_type(NSLogsPath,namelist[n]) == DT_DIR) {
        sprintf(file_name, "%s/%s/summary.top", NSLogsPath, namelist[n]->d_name);
        ptr = namelist[n]->d_name;
        ptr = ptr + 2;
        if( ns_is_numeric (ptr) == 1)
        {
          read_summary_top(file_name, atoi(ptr), NSLogsPath, user, num_users, uflag);
          if (temp_summary_file_created) {
            remove_tmp_summary_file(); 
            temp_summary_file_created = 0;
          }
        }    
      }
      free(namelist[n]);
    }
    if(Gflag) 
    {
      printf("Total Filtered Records: %d, Total Records: %d\n",total_filtered_records, total_num_records);
    }
    free(namelist);
  }
  closedir(dir_fp);
}

void show_requested_test_run(char *NSLogsPath, char **user, int num_users, int uflag)
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
  if(read_summary_top(file_name, TRNum, NSLogsPath, user, num_users, uflag)== 1)
  {
    if (temp_summary_file_created) {
      remove_tmp_summary_file();
      temp_summary_file_created = 0;
    }  
    exit(-1);
  } else {
    if (temp_summary_file_created) {
      remove_tmp_summary_file(); 
      temp_summary_file_created = 0;
    }  
  }
}

//fill proj, subproj when given with -p option
//proj_sub_proj, is in format : proj1/sub_proj1 proj2/sub_proj2 proj3/sub_proj3 ...
static void get_proj_sub_proj()
{
   int i;
   char *proj_subproj_with_slash[100];
   char *temp[100];
  
  num_projs = get_tokens(proj_sub_proj, proj_subproj_with_slash, " ", 100);

  for (i = 0; i<num_projs; i++)
  {
    int num;
    num = get_tokens(proj_subproj_with_slash[i], temp, "/", 100);

    strcpy(project[i], temp[0]);
    if(num == 1)
      strcpy(sub_project[i], "All");
    else
      strcpy(sub_project[i], temp[1]);
  }
}

//fill proj, subproj when given with -u option
static void get_users_proj_sub_proj(char *user)
{
  char cmd_name[128 + 1]="\0";  
  char buff[1024 + 1]="\0";
  FILE *fp;
  char *temp[10];
  int i = 0;

//  printf("get_users_proj_sub_proj called.\n");

/*  nsu_show_projects output -->
    User Name|Project|Subproject
    NA|a|aaaa
    NA|abc|xyz
    NA|default1|default1
    NA|default|default
    NA|gaurav_project|gaurav_sub_project1
*/
  sprintf(cmd_name, "nsu_show_projects -u cavisson | grep -v \"^User Name\"");
   
  fp = popen(cmd_name, "r");

  if(fp == NULL)
  {
    printf("Error: 'nsu_show_project -u cavisson' failed.\n");
    perror("popen"); //ERROR: popen failed
    exit(-1);
  }

  while(fgets(buff, 1024, fp)!= NULL )
  {
    // printf("i = %d, buff = %s\n", i, buff);
    buff[strlen(buff) -1]='\0';
    get_tokens(buff, temp, "|", 10);
    strcpy(project[i], temp[1]);
    strcpy(sub_project[i], temp[2]);
    i++; 
  }
  num_projs = i; 
  pclose(fp);
}

static void print_header()
{
#if 0  
  if (LLflag == 1 && test_run_running > 0 && Rflag == 1)
  { 
    separator = '\t';
     printf("TestRun\tStatus\tStart Date\t\tDuration\tOwner\t\tProject/Subproject/Scenario\n");
  } else
#endif 
    
  if (!LLflag){
    if(raw_format)
    { 
      separator = '|';
      if(Long_form)
        printf("Project|Subproject|Test Run|Test Name|Scenario Name|Start Time|Run Time|Virtual Users|Report Summary|Report Progress|Report Detail|Report User|Report Fail|Report Page Break Down|WAN_ENV|REPORTING|Test Mode|Page Dump|Notes|Started By\n");

      else
        printf("Project|Subproject|Test Run|Test Name|Scenario Name|Start Time|Run Time|Virtual Users\n");
        //printf("Test Run|Test Name|Scenario Name|Start Time|Run Time|Virtual Users\n");
    }
    else
    {
      separator = '\t';
      if(Long_form)
        printf("Project\tSubproject\tTest Run\tTest Name\tScenario Name\tStart Time\tRun Time\tVirtual Users\tReport Summary\tReport Progress\tReport Detail\tReport User\tReport Fail\tReport Page Break Down\tWAN_ENV\tREPORTING\tTest Mode\tPage Dump\tNotes\nStarted By\n");
      else
        printf("Project\tSubproject\tTest Run\tTest Name\tScenario Name\tStart Time\tRun Time\tVirtual Users\n");
    } 
  }
}

static void get_basename(char *wdir)
{
  if(wdir != NULL) {
    ctrl_name = basename(wdir);
  }
  
  char file[1024] = {0}; 
  char line[1024] = {0}; 
  char *ptr = NULL;
  struct stat s;

  strcpy(tomcat_port, "80");
  sprintf(file, "%s/webapps/.tomcat/tomcat_port", wdir);
  
  if(stat(file, &s) != 0)
    return;

  if(s.st_size == 0)
    return;

  FILE *fp = fopen(file, "r");

  if(fp == NULL) {
    return;
  }

  fgets(line, 1024, fp);

  if((ptr = strchr(line, '\n')) != NULL)
    *ptr = '\0';

  if((ptr = strchr(line, ' ')) != NULL)
    *ptr = '\0';
  
  sprintf(tomcat_port, "%s", line);
  fclose(fp);
}

static void get_product_name(char *wdir)
{
  char cmd_buff[128] = {0};

  sprintf(cmd_buff, "%s/bin/nsi_show_config", wdir);
  nslb_run_cmd_and_get_last_line(cmd_buff, 32, cmd_out);
  if(cmd_out[0] == '\0')
  {
    cmd_out[0] = '-';
  }
}

int main(int argc, char *argv[])
{
  char NSLogsPath[2024];
  char option;
  extern char *optarg;
  int uflag = 0;
  int aflag = 0;
  int pflag = 0;
  int i = 0;
  char search_arg[1024];
  char copy_key[50];
  char user_arg[1024];
  char *user[100];
  int num_users;
  //is_cavisson_user();


  if (getenv("NS_WDIR") != NULL)
  {
    strcpy(wdir, getenv("NS_WDIR"));
    sprintf(NSLogsPath, "%s/logs/", wdir);
  }
  else
  {
    printf("NS_WDIR env variable is not set. Setting it to default value /home/cavisson/work/");
    sprintf(NSLogsPath, "/home/cavisson/work/logs/");
  }

  strcpy (g_log_path, NSLogsPath); //3.7.7 change , used in calc_duration function
  
  //printf("Setting directory to %s\n", NSLogsPath);
  chdir(NSLogsPath);
  
  while ((option = getopt(argc, argv, "Au:p:T:t:o:d:c:s:k:m:x:w:S:lrRLgzCO")) != -1) 
  {
    switch (option) 
    {
      case 'R':
        Rflag = 1;
        break;
      case 'L':
        LLflag = 1;
        break;
      case 'A':
	    if(uflag || aflag || pflag)
          usage(1);
        aflag++;
        show_all = 1;
        break;
       case 'T' :
        strncpy(test_type, optarg, 64);
        test_type[63] = '\0';
        break;
      case 'u':
	    if(uflag || aflag || pflag)
          usage(1);
 	    uflag++;
	strcpy(user_arg,optarg);
        num_users = get_tokens(user_arg, user, ",", 100);
        //validate_user(user);
        //get_users_proj_sub_proj(user[0]); //get proj/sub_proj by nsu_show_projects -u user
        break;
      case 'p':
	    if(uflag || aflag || pflag)
          usage(1);
 	      pflag++;
        strcat(proj_sub_proj, optarg);
        strcat(proj_sub_proj, " ");
        get_proj_sub_proj();
        break;
      case 'c':
        strcat(g_scenario, optarg);
        break;
      case 'k':
	strcpy(search_arg,optarg);
	num_search_key = get_tokens(search_arg, search_keys, " ", 100);
        if(num_search_key > 3)
          num_search_key=3;
        for(i=0;i<num_search_key;i++){
          if(strlen(search_keys[i])>32){
            strncpy(copy_key,search_keys[i],32);
            strcpy(search_keys[i],copy_key);
          }
        }

        break;
      case 'm':
        strcat(g_mode, optarg);
        break;        
      case 't':
        TRNum = atoi(optarg);
        break;
      case 'l':
        Long_form = 1;
        break;
      case 'r':
        raw_format = 1;
        break;
      case 'o':  // Can not have default value:
        given_owner_id = get_user_id_by_name(optarg); 
        break;
      case 'O':
        cdr_flag = 1;
        cdr_config.log_file_size = 10 * 1024 * 1024;
        cdr_config.audit_log_file_size = 10 * 1024 * 1024;

        break;
      case 'S':
        strncpy (cdr_session_flag, optarg, 2);
        break;
      case 'd':  // Can not have default value
        fill_min_max_duration(optarg, &max_duration, &min_duration);
        Dflag = 1; 
        //printf("min time = %d, max_time = %d\n", min_duration, max_duration);
        break;
      case 's':  // Can not have default value
        cur_time = time(NULL);
        last_n_days = atoi(optarg) * 24 * 60 * 60; // In Secs
        break;
      case 'g':   //To print Total Filtered Records and Total Records.
        Gflag = 1;
        break;
      case 'z':
        zflag = 1;
        break;
      case 'C':
        Cflag = 1;
        separator = '|';
        break;
      case 'x':
        logging_start_end_date_time(optarg, &start_time_sec, &end_time_sec); 
        break;
      case ':':
      case '?':
        usage(0);
    }
  }
  //Malloc running test run table
  test_run_info = (TestRunList *)malloc(256 * sizeof(TestRunList));
  memset(test_run_info, 0, (256 * sizeof(TestRunList)));
  allocate_running_test_search_map();
  if(!Cflag && ! cdr_flag && !cdr_session_flag[0])
    print_header();

  else if (cdr_flag || (strcmp (cdr_session_flag, "T") == 0))
     printf ("Project|Subproject|Test Run|Test Name|Scenario Name|Start Time|Run Time|Virtual Users|Report Summary|Report Progress|Report Detail|Report User|Report Fail|Report Page Break Down|WAN_ENV|REPORTING|Test Mode|Page Dump|Notes|Started By| (type (T for normal  testrun and S for CMT session))TR| TR size | TR remove date|graph_data|size | remove date | csv | size | remove date | raw_file | size | remove date | db | size | remove date | key_file | size | remove date | har file |size | remove date | page dump | size | remove date | logs | size | remove date | test_data | size | remove date\n");


  if(Cflag) {
    get_basename(wdir);
    get_product_name(wdir);
  }
/* In regard to bug_id#3181: 
 * Below code was added, when executing nsu_show_netstorm shell:
 * Syntax: nsu_show_netstorm -n TestNumber
 * which further executes nsu_show_test_logs -R -t TestNumber.
 * Here we check whether given testrun number is running, search TestNumber in test_run_list.
 * Return 0 -success(TestNumber running)
 * Return 1 -failure(TestNumber not running)    
 * */ 
  if (!cdr_session_flag[0] && Rflag == 1) {
    if(TRNum) // For specified TR, -L and other flags are ignored
    { 
      for (i = 0; i < test_run_running; i++) 
      {      
        if (TRNum == test_run_info[i].test_run_list)
        { 
          // Given test run is in the list of running test. so return 0 (running)
          return 0;
        }
      }
      // Given test run is not in the list of running test. so return 1 (Not running)
      return 1;
    }
    else
    {
      for (i = 0; i < test_run_running; i++) {
        TRNum = test_run_info[i].test_run_list;
        show_requested_test_run (NSLogsPath, user, num_users, uflag);
      }
      if(Cflag && (test_run_running == 0))
        printf("%s|-|-|-|-|-|-|-|-|-|-|-|-|-|-|-|-|-|-|-|-|%s|%s\n", ctrl_name, tomcat_port, cmd_out);
    }
  } else {       
    if (cdr_session_flag[0] && TRNum)
      support_for_new_options(); 
    else if (TRNum)
      show_requested_test_run(NSLogsPath, user, num_users, uflag);
    else
      show_all_test_runs(NSLogsPath, user, num_users, uflag);
  }

  // Checking for -x option 
  if(start_time_sec > -1 && end_time_sec > -1)
  {
    // If no test run data is available to show, just print the - for that controller.
    if(total_filtered_records == 0) 
    {
      get_basename(wdir);
      get_product_name(wdir);
      printf("%s|-|-|-|-|-|-|-|-|-|-|-|-|-|-|-|-|-|-|-|-|%s|%s\n", ctrl_name, tomcat_port, cmd_out);
    }
  }
  //if(zflag && (!strcmp(g_mode, "locked")))
  //  printf("\nTotal %d bytes\n", file_size);
  //Free and make NULL test_run_info pointer
  free(test_run_info);
  test_run_info = NULL;
  return 0;
}
