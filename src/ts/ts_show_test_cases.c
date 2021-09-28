/*----------------------------------------------------------------------
   Name    : ts_show_test_cases.c
   Author  : Narendra
   Purpose : This file is to show scripts according project/sub-project details.
   Usage   : ts_show_test_cases [-A | -u <User Name> | -p <proj1/sub_proj1> -p <proj2/sub_proj2> . . .]
   Format of Output:
             Project|Subproject|Test Cases|Owner|Group|Permission|Modification Date
             Where Modification Date will be in "YYYY-MM-DD HH:MM:SS" (24 hour format)
   To Compile: gcc -o -g ts_show_test_cases ts_show_test_cases.c
----------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <grp.h>
#include <pwd.h>
#include <errno.h>
#include<dirent.h>

#include "nslb_log.h" 
#include "nslb_util.h" 

#define MAX_LINE_LENGTH 2048
#define MAX_STR_LENGTH 256

static char testcase_name_list_file[MAX_STR_LENGTH];
static int all = 1;

static char testcase_dir[MAX_LINE_LENGTH];
static char tsuites_dir[MAX_LINE_LENGTH];
//static char tr_dir[MAX_LINE_LENGTH];

static int num_projs = 0;
static char usr_project[1000][255 + 1];
static char usr_sub_project[1000][255 + 1];

// These two variables are used to store testcase from testsuite file or test run
// It also uses usr_project and usr_sub_project array to keep the project/sub project name of test suite or TR
static int testcase_idx = 0; // index in usr_testcase while parsing input and filing in usr_testcase
static char usr_testcase[1000][1000 + 1];


static gid_t usr_primary_grp_id;
static gid_t usr_secondry_grp_id;
static uid_t usr_id;

static int debug_level = 0;     // Debug log level
static FILE *debug_file = NULL; // Debug log file


static short min_depth;
static short max_depth;
#define GET_MIN_DEPTH() min_depth
#define GET_MAX_DEPTH() max_depth
#define SET_MIN_DEPTH(depth) \
{\
  min_depth=depth;\
}

#define SET_MAX_DEPTH(depth) \
{\
  max_depth=depth;\
}


#define TC_DIR "testcases"
#define SET_WDIR(ptr) sprintf(g_wdir, "%s", ptr);
//set ReplayProfileDir
#define GET_TESTCASES_DIR() testcase_dir 
#define GET_TESTSUITES_DIR() tsuites_dir 
inline static void set_testcases_dir(char *ta_dir, char *proj, char *sub_proj)
{
   if(!ta_dir || !proj || !sub_proj)
     return;
   sprintf(testcase_dir, "%s/%s/%s/%s", ta_dir, proj, sub_proj,  "testcases");
}

inline static void set_testsuites_dir(char *ta_dir, char *proj, char *sub_proj)
{
   if(!ta_dir || !proj || !sub_proj)
     return;
   sprintf(tsuites_dir, "%s/%s/%s/%s", GET_NS_TA_DIR(), proj, sub_proj,  "testsuites");
}


static void usage(char *err_msg)
{

  fprintf(stderr, "%s\n", err_msg);
  fprintf(stderr, "Usage: \n");
  fprintf(stderr, "     : ts_show_test_cases [-A | -u <User Name> | -p <proj1/sub_proj1> -p <proj2/sub_proj2> | -w <workspace>/<profile> | -t <proj2/subproj2/testsuite. . .] \n");
  fprintf(stderr, "Where: \n");
  fprintf(stderr, "     : -A is used to show all testsuite, that present in testcases directory inside admin's default profile.\n");
  fprintf(stderr, "     : -u is used to show testcase specified User Name.\n");
  fprintf(stderr, "     : -p is used to show testcase of specified proj/sub_proj. Always use along with -w option, otherwise default profile will be used.\n");
  fprintf(stderr, "     : -w is used to show testcase of specified <workspace>/<profile>.\n");
  fprintf(stderr, "     : -t is used to show testcase used in specified <proj2/subproj2/testsuites. Always use along with -w option, otherwise default profile will be used.\n");
  exit (-1);
}

static int get_tokens1(char *line, char *fields[], char *token )
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


static void open_debug_log_file(char *user_name)
{
char debug_file_path[4096];

  // Do not open if debug is off
  if(debug_level <= 0) return;

  // Create unique file for each user so that we do have permission issues
  sprintf(debug_file_path, "/tmp/%s_ts_show_test_cases_debug", user_name);

  if(!(debug_file = fopen(debug_file_path, "a")))
  {
    fprintf(stderr, "ts_show_test_cases: Error in opening debug file (%s). Error = %s\n", debug_file_path, strerror(errno));
    fprintf(stderr, "Debug log will be shown in console\n");
    debug_file = stdout;
  }
}

static void get_usr_uid_gid()
{
  FILE *fp;
  char cmd[MAX_STR_LENGTH]="\0";
  char buff[MAX_STR_LENGTH]="\0";
  struct passwd *pw;
  usr_id = getuid();
 
  pw = getpwuid(usr_id);

  // Open debug here so that we have the login name
  open_debug_log_file(pw->pw_name);
  sprintf(cmd, "id %s -G", pw->pw_name);
  
  fp = popen(cmd, "r");
  if(fp == NULL)
  {
    fprintf(stderr, "Error: Can not get groups id.\n"); 
    perror("popen");
  }

  if(fgets(buff, 1024, fp)!= NULL )
   sscanf(buff, "%d %d", &usr_primary_grp_id, &usr_secondry_grp_id);

  LIB_DEBUG_LOG(1, debug_level, debug_file, "usr_primary_grp_id = %d, usr_secondry_grp_id = %d", usr_primary_grp_id, usr_secondry_grp_id);
}

//is user ACTIVE
static void validate_user(char *usr)
{
  char cmd[MAX_STR_LENGTH]="\0";

  sprintf(cmd, "nsu_check_user %s ts_show_test_cases", usr);
  if(system(cmd) != 0)
  {
    //fprintf(stderr, "Not a valid user : %s\n", usr);
    LIB_DEBUG_LOG(1, debug_level, debug_file, "Error in running command = %s", cmd);;
    exit (-1);
  }

  //show all if user is admin or netstorm
  // TODO: we should remove it
  //
  if(!strcmp(usr, "admin") || !strcmp(usr, "netstorm"))
  {
    LIB_DEBUG_LOG(1, debug_level, debug_file, "Setting all for admin/netstorm user id");
    all = 1;
  }
}
 
//make array of proj subproj to match
static void get_users_proj_sub_proj(char *user)
{
  char cmd_name[128]="\0";  
  char buff[512]="\0";
  FILE *fp;
  char *temp[10];
  int i = 0;

  sprintf(cmd_name, "nsu_show_projects -u %s | grep -v \"^User Name\"", user);
   
  fp = popen(cmd_name, "r");

  if(fp == NULL)
  {
    printf("Error: 'nsu_show_project -u %s' failed.\n", user);
    perror("popen"); //ERROR: popen failed
    exit(-1);
  }

  while(fgets(buff, 1024, fp)!= NULL )
  {
    buff[strlen(buff) -1]='\0';
    get_tokens1(buff, temp, "|");
    if(!strcmp(temp[1], "All"))
    {
      all = 1;
      break; 
    }
     //<user_name>|<Profile>|<proj>|<subproj>
    //cavisson|default|pro1|subproj1
    char *default_ws = "admin";
    if(!strcmp(temp[0], "cavisson"))
       temp[0] = default_ws;
    //set test assets dir
    nslb_set_ta_dir_ex1(NSLB_GET_WDIR(), temp[0], temp[1]);
    set_testcases_dir(GET_NS_TA_DIR(), temp[0], temp[1]);
    set_testsuites_dir(GET_NS_TA_DIR(), temp[0], temp[1]);
 
    strcpy(usr_project[i], temp[2]);
    strcpy(usr_sub_project[i], temp[3]);
    LIB_DEBUG_LOG(1, debug_level, debug_file, "User project = %s, subproject = %s", usr_project[i], usr_sub_project[i]);
    i++; 
  }

  num_projs = i; 
  pclose(fp);
}

//make list of testcases
static void get_all_testcase_names()
{
  char cmd[MAX_STR_LENGTH]="\0";

  char create_path_buf[4096];
  create_path_buf[0] = '\0';
  // /home/cavisson/work/workspace/<workspace_name>
  // /home/cavisson/work/workspace/<workspace_name>/<profile_name>
  ///home/cavisson/work/workspace/<workspace_name>/<profile_name>/cavisson/<proj>/<sub_proj>/
  // /home/cavisson/work/workspace/<workspace_name>/<profile_name>/cavisson/<proj>/<sub_proj>/testcases/<dir>/<file> ==> depth =11
  strcpy(create_path_buf, GET_TESTCASES_DIR());
  if(num_projs) {
    for(int i=0; i < num_projs; i++)
       sprintf(create_path_buf,"%s/%s/%s", GET_TESTCASES_DIR(), usr_project[i], usr_sub_project[i]);
  }
  
  sprintf(cmd, "find -L %s -mindepth %d -maxdepth %d | grep %s | grep -v ^.*/[.] | sort -t '/' +4 -5 -k 5,6 -k 6,7 >%s", create_path_buf, GET_MIN_DEPTH(), GET_MAX_DEPTH(), TC_DIR, testcase_name_list_file);
 
  if(system(cmd) == -1)
  {
     fprintf(stderr, "Error: Can not find testcase names.\n");
     exit(-1);
  }
}

//extract proj, subproj from script path
//path would be like: /home/cavisson/work/workspace/<workspae_name>/<profile_name>/cavisson/<proj>/<sub_proj>/testcases/<test_case_dir>

static void get_proj_sub_proj_name(char *path, char* work_profile, char *project, char *sub_project, char *name, int tflag)
{
  char *fields[30];
  char buf[MAX_STR_LENGTH]="\0";
  
  LIB_DEBUG_LOG(1, debug_level, debug_file,"Method called, path = %s", path);
  strcpy(buf, path);
  get_tokens1(buf, fields, "/");
  
  strcpy(name, fields[10]);
  strcpy(work_profile, fields[5]);
 
  if(tflag)
  {
    strcpy(project, usr_project[num_projs - 1]);
    strcpy(sub_project, usr_sub_project[num_projs - 1]);
 }
  else{
    strcpy(project, fields[7]);
    strcpy(sub_project, fields[8]);
    }
}

//This method will add project/subproject/script in the array
static int add_testcase_in_array(char *testcase_name)
{
  char testcase_file_path[MAX_STR_LENGTH];
  int i;

  // Make absolute path
  
    LIB_DEBUG_LOG(1, debug_level, debug_file,"testcase_dir= %s/usr_project[%d] = %s/usr_sub_project[num_projs - 1] = %s/testcase_name = %s", testcase_dir, num_projs - 1, usr_project[num_projs - 1], usr_sub_project[num_projs - 1], testcase_name);
    sprintf(testcase_file_path, "%s/%s/%s/testcases/%s", testcase_dir, usr_project[num_projs - 1], usr_sub_project[num_projs - 1], testcase_name);
  
     LIB_DEBUG_LOG(1, debug_level, debug_file, "testcase_file_path", testcase_file_path);
    
  // Check if already in array or not
  for(i = 0; i < testcase_idx; i++)
  {
    if(strcmp(usr_testcase[i], testcase_file_path)== 0)
    {
      LIB_DEBUG_LOG(1, debug_level, debug_file, "Test case %s is already added in array", testcase_file_path);
      return 0;
    }
  }

  // Add in the array
  strcpy(usr_testcase[testcase_idx], testcase_file_path);
  //LIB_DEBUG_LOG(4, debug_level, debug_file, "usr_testcase[%d] = %s", testcase_idx, usr_testcase[script_idx]);
  testcase_idx++;
  return 0;
}

//This method will extarct project/subproject/<tsuites>.conf from testsuites name
int extract_proj_subproj_name(char *optarg, char **field, char *usr_testsuites)
{
  int ret;

  if((ret = get_tokens1(optarg, field, "/")) < 3)
    return -1;
  
  strcpy(usr_project[num_projs], field[0]);
  strcpy(usr_sub_project[num_projs], field[1]);
  strcpy(usr_testsuites, field[2]);
  num_projs++;

  LIB_DEBUG_LOG(1, debug_level, debug_file, "ret = %d", ret);
  return ret;
}

// Args:
//   scen_name with proj/sub-proj e.g. default/default/my_scen.conf (.conf is optional)
static void get_testcase_from_testsuites(char *tsuites_name)
{
  char *field[10];
  char buf[MAX_STR_LENGTH + 1]="\0";
  char tsuites_file_path[MAX_STR_LENGTH + 1];
  FILE *tsuites_file;
  //int num_script = 0;
  char lol_array[1024 + 1];
  char usr_testsuites[255+1];

  strcpy(lol_array, tsuites_name);
  if(extract_proj_subproj_name(lol_array, field, usr_testsuites) < 2)
   return;
  
  if(strstr(tsuites_name, ".conf"))
    sprintf(tsuites_file_path, "%s/%s/%s/testsuites/%s", GET_TESTSUITES_DIR(), usr_project[num_projs -1], usr_sub_project[num_projs - 1], usr_testsuites);
  else
    sprintf(tsuites_file_path, "%s/%s/%s/testsuites/%s.conf", GET_TESTSUITES_DIR(), usr_project[num_projs - 1], usr_sub_project[num_projs - 1], usr_testsuites);
  
  //DL

  // Check if specified file is present or not
  if ((tsuites_file = fopen(tsuites_file_path, "r")) == NULL)
  {
    fprintf(stderr, "Error in opening file %s . Error = %s", tsuites_file_path, strerror(errno));
    fprintf(stderr, "Test suites file is not correct. Test suites file is %s ", tsuites_file_path);
    exit(-1);
  }

  while(fgets(buf, MAX_STR_LENGTH, tsuites_file))
  {
    LIB_DEBUG_LOG(1, debug_level, debug_file, "buf = %s",buf);
    if(!(strncmp(buf, "TEST_CASE_NAME", 4)))
    {
      get_tokens1(buf, field, " ");
      //We may have some condition for that.
      add_testcase_in_array(field[1]);
    }
  }
}

// MOVE TO LIBRARY

//This method to open summary.top file
FILE * nslib_open_file(char *file_name, char *mode)
{
FILE *fp;


  if((fp = fopen(file_name, mode)) == NULL)
  {
    //fprintf(stderr, "Error in opening %s file. Ignoring this...\n", file_name);
    return(fp);
  }
  //printf("File %s opened successfully.\n", file_name, fp);
  return(fp);
}

/*
 *Description : Comparison function for scandir
 *inputs : **a, **b - pointers to DIR structure.
 *outputs : 1 on succes ,0 on failure
 *error : none
 *algo : simple
*/
#if 0
#if ( (Fedora && RELEASE >= 14) || (Ubuntu && RELEASE >= 1204) )
static int my_alpha_sort(const struct dirent **aa, const struct dirent **bb)
#else
static int my_alpha_sort(const void *aa, const void *bb)
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
#endif
/*
 *Description: To filter directories starting with TR in scandir.
 *inputs : *a - DIR structure.
 *outputs : 1 on succes ,0 on failure.
 *error : none
 *algo : simple
*/
int only_testcase_dir(const struct dirent *a) 
{
    return 1;
}

//get testcase owner if pw->pw_name is NULL set owner as UNKNOWN
static void get_testcase_owner(uid_t user_id, char *own)
{
  //printf("get_script_owner called\n");
  struct passwd *pw;
  pw = getpwuid(user_id);
  if(pw)
    strcpy(own, pw->pw_name);
  else
    strcpy(own, "UNKNOWN");
}

//get testcase group, if gp->gr_name is NULL set owner as UNKNOWN
static void get_testcase_group(gid_t group_id, char *grp)
{
  struct group *gp = getgrgid (group_id);
  if(gp)
    strcpy(grp, gp->gr_name);
  else
    strcpy(grp, "UNKNOWN");
}

//if running user & testcase owner is same then use permisson of owner
//if above condition fails then chek groups(all groups) with groups of script, if same use permisson of group
//else use permisson of others
static void get_testcase_permission(uid_t tcaseuid,  gid_t tcase_gid, mode_t mode, char *perm)
{
  //if user is root or admin we should display RWD
  if(!usr_id)
  {
    strcpy(perm, "RWD");
    return;
  }

  if(tcaseuid == usr_id)
  {
    //    permisson of owner
    if(S_IRUSR & mode)
      strcpy(perm, "R");
    else
      strcpy(perm, "-");

    if(S_IWUSR & mode)
     strcat(perm, "WD");
    else
     strcat(perm, "--");
  }
  //currently only 2 groups are matched
  else if (tcase_gid == usr_primary_grp_id || tcase_gid == usr_secondry_grp_id)
  {
    //permisson of group
    if(S_IRGRP & mode)
      strcpy(perm, "R");
    else
      strcpy(perm, "-");
     if(S_IWUSR & mode)
       strcat(perm, "WD");
     else
       strcat(perm, "--");
  }
  else
  {
    //permisson of other
    if(S_IROTH & mode)
      strcpy(perm, "R");
    else
      strcpy(perm, "-");

     if(S_IWOTH & mode)
       strcat(perm, "WD");
     else
       strcat(perm, "--");
  }
}

//modification time when contents of dir are updated, & change time is that when permission has changed
//we have used  modification time (mtime)
static void get_testcase_modification_time(time_t *modification_time, char *time_format)
{
  struct tm *mod_time;
  mod_time = localtime(modification_time);
  sprintf(time_format, "%02d/%02d/%d %02d:%02d:%02d", mod_time->tm_mon + 1,  mod_time->tm_mday, mod_time->tm_year + 1900, mod_time->tm_hour, mod_time->tm_min, mod_time->tm_sec);
}

static void get_additional_info(char *path)
{
  struct stat testcase_det;
  FILE *conf_fp;
  char conf_file[1024] = "\0";
  char buf[4 * 1024 + 1] = "\0"; 
  char desc[4 * 1024] = "\0";
  char scenario[1024] = "\0";
  char *temp[10]; 
  sprintf(conf_file, "%s/testcase.conf", path);
  //From this file we will extract scenario name and description.  
  if(stat(conf_file, &testcase_det) != 0)
  {
     fprintf(stderr, "Error in getting stat of dir %s\n", conf_file);
     return;
  }
  if((conf_fp = fopen(conf_file, "r"))  == NULL)
  {
    fprintf(stderr, "Error in opening testcase configuration %s", conf_file);
    return;
  } 
  while(fgets(buf, 4096, conf_fp) != NULL)
  {  
     if(buf[strlen(buf) - 1] == '\n')
       buf[strlen(buf) - 1] = '\0';
     if(strncmp(buf, "SCENARIO_NAME", 13) == 0)
     {
      get_tokens1(buf, temp, " "); 
      sprintf(scenario, "%s", temp[1]); 
     }
     else if(strncmp(buf, "DESCRIPTION", 11) == 0)
     {
       sprintf(desc, "%s", &(buf[11 + 1]));
     }
  }
  fclose(conf_fp);
  printf("%s|%s|", scenario, desc);

}

static void get_testcase_stats(char *path)
{
  struct stat testcase_det;
  char owner[MAX_STR_LENGTH] = "\0";
  char group[MAX_STR_LENGTH] = "\0";
  char permission[MAX_STR_LENGTH] = "\0";
  char modification_time[MAX_STR_LENGTH] = "\0";

  LIB_DEBUG_LOG(1, debug_level, debug_file, "path = %s", path);
  if(stat(path, &testcase_det) != 0)
  {
     fprintf(stderr, "Error in getting stat of dir %s\n", path);
     return;
  }
  get_testcase_owner(testcase_det.st_uid, owner);
  get_testcase_group(testcase_det.st_gid, group);
  get_testcase_permission(testcase_det.st_uid, testcase_det.st_gid, testcase_det.st_mode, permission);
  get_testcase_modification_time(&testcase_det.st_mtime, modification_time);
  printf("%s|%s|%s|%s\n", owner, group, permission, modification_time);
}

static int is_testcase_valid(char *project, char *sub_project, char *testcase)
{
  char file_path[1024] = "\0";
  struct stat fp_stat;
  sprintf(file_path, "%s/%s/%s/%s/%s/testcase.conf", testcase_dir, project, sub_project, TC_DIR, testcase);
  if(stat(file_path, &fp_stat) != 0) 
    return 1;

  sprintf(file_path, "%s/%s/%s/%s/%s/pre_test_setup", testcase_dir, project, sub_project, TC_DIR, testcase);
  if(stat(file_path, &fp_stat) != 0)
    return 1;

  // comment this code as check_status file is not present in new design
  /*sprintf(file_path, "%s/%s/%s/%s/check_status", testcase_dir, project, sub_project, testcase);
  if(stat(file_path, &fp_stat) != 0)
    return 1;*/
  return 0;
}


static int is_testcase_to_show(char *project, char *sub_project, char *testcase)
{
  int i;
   
  for(i = 0; i < num_projs; i++)
  {
    if(!strcasecmp(project, usr_project[i]) && (!strcasecmp(usr_sub_project[i], "All") || !strcasecmp(sub_project, usr_sub_project[i])))
        return 0; //show
  }
 
  return 1;
}

static void  show_testcase_using_file_list()
{
  FILE *testcase_list;
  char testcase_path[MAX_LINE_LENGTH] = "\0";
  char project[MAX_STR_LENGTH] = "\0";
  char sub_project[MAX_STR_LENGTH] = "\0";
  char testcase_name[MAX_LINE_LENGTH] = "\0";
  char work_profile[MAX_LINE_LENGTH] = "\0";
  testcase_list = fopen(testcase_name_list_file, "r");
  if(testcase_list == NULL)
  {
    fprintf(stderr, "Unable to open %s file.\n", testcase_name_list_file);
    exit(-1);
  }
  while(fgets(testcase_path, MAX_LINE_LENGTH, testcase_list)) 
  {
    // /home/cavisson/work/workspace/<workspae_name>/<profile_name>/cavisson/<proj>/<sub_proj>/testcases/<test_case_dir>
    testcase_path[strlen(testcase_path) - 1]='\0';
    get_proj_sub_proj_name(testcase_path, work_profile, project, sub_project, testcase_name, 0);
    if(!all) 
    {
       if( num_projs && is_testcase_to_show(project, sub_project, testcase_name))
       {
         continue;
       }
    }
    if(is_testcase_valid(project, sub_project, testcase_name))
    {
      continue;
    }
    
    printf("%s|%s|%s|%s|", work_profile, project, sub_project, testcase_name);
    get_additional_info(testcase_path);
    get_testcase_stats(testcase_path);
  }
}
//This method will display the project/subproject/script
static void  show_testcase_using_array(int tflag)
{
  char project[MAX_STR_LENGTH] = "\0";
  char sub_project[MAX_STR_LENGTH] = "\0";
  char testcase_name[MAX_LINE_LENGTH] = "\0";
  int idx;
  char work_profile[MAX_LINE_LENGTH] = "\0"; 

  LIB_DEBUG_LOG(1, debug_level, debug_file, "testcase_idx  = %d", testcase_idx);
  
  for(idx = 0; idx < testcase_idx; idx++)
  {
    LIB_DEBUG_LOG(1, debug_level, debug_file, "usr_testcase[%d] = %s", idx, usr_testcase[idx]); 
      get_proj_sub_proj_name(usr_testcase[idx], work_profile, project, sub_project, testcase_name, tflag);
   // get_proj_sub_proj_name(usr_script[idx], project, sub_project, script_name);
  
    if(is_testcase_valid(project, sub_project, testcase_name))
    {
      continue;
    }
    printf("%s|%s|%s|%s|", work_profile, project, sub_project, testcase_name);
    get_additional_info(usr_testcase[idx]);
    get_testcase_stats(usr_testcase[idx]);
  }
}

/*#define SET_TS_TC_DIR( proj, sub_proj )\
{\
    nslb_set_ta_dir_ex1(NSLB_GET_WDIR(), GET_NS_WORKSPACE(), GET_NS_PROFILE()); \
    set_testcases_dir(GET_NS_TA_DIR(), proj, sub_proj); \
    set_testsuites_dir(GET_NS_TA_DIR(), proj, sub_proj); \
}*/


int main(int argc, char *argv[])
{
  char option;
  char user[128];
  char *field[10];
  int pflag = 0;
  int uflag = 0;
  int aflag = 0;
  int tflag = 0;
  int wflag = 0;
  //char usr_testsuites[255 + 1];
  char tsuites_name[1024 + 1];
  char arg_buf[1024];
 
  sprintf(testcase_name_list_file, "/tmp/testcase_list.%d", getpid());
  if(argc < 2) {
    usage("Error: provide at least one argument according to following condition\n");
  }
 
  /*bug id: 101320: set ns_wdir and default workspace and profile*/
  nslb_init_wdir_wpdir();
 
  while ((option = getopt(argc, argv, "Au:p:t:D:w:")) != -1)
  {
    switch (option)
    {
      case 'A': // All testcases
        if(pflag || uflag || aflag || tflag || wflag)
          usage("-A cannot be used with options or cannot be given more than once");
        all = 1;
        aflag++;
        //default path ===> ~/work/workspace/admin/default/cavisson
        //complete path===> <default pat>/proj1/subporj/tescases/tescase_dir/files
        SET_MIN_DEPTH(4)
        SET_MAX_DEPTH(4)
       break;

      case 'u': // testcases of all project/subproject assigned for the user
        if(pflag || uflag || aflag || tflag)
          usage("-u cannot be used with options or cannot be given more than once");
          
        all = 0; // Why?
        strcpy(user, optarg);
        validate_user(user);
        get_users_proj_sub_proj(user);
        uflag++;
        break;

      case 'p': // testcases of project/subproject passed
        if(uflag || aflag || tflag)
          usage("-p option cannot be used with other options");

	//if all is already set to 1 by previous -p All/All, then we need not to check nxt -p option 
	if(all && pflag)
	 break;

        if(get_tokens1(optarg, field, "/") != 2)
          usage("with -p the format should be project/subproject");

        strcpy(usr_project[pflag], field[0]);
        strcpy(usr_sub_project[pflag], field[1]);
	//if proj is all show all 
	if(!strcmp(usr_project[pflag], "All")){
        // path ==> /home/cavisson/work/workspace/<workspace_name>/<profile_name>/cavisson
        //           /<proj>/<sub_proj>/testcases/<dir>/<file> ==>depth = 5
        SET_MIN_DEPTH(4)
        SET_MAX_DEPTH(4)
	all = 1;
        }
	else
          all = 0;
        pflag++;
        num_projs = pflag;
        // path ==> /home/cavisson/work/workspace/<workspace_name>/<profile_name>/cavisson/<proj>/<sub_proj>
        //                 /testcases/<dir>/<file>
        SET_MIN_DEPTH(2)
        SET_MAX_DEPTH(2)
        break;

    case 'w':
        if(wflag)
        {
          printf("nsu_show_scenarios: -w option cannot be specified more than once.\n");
          usage("nsu_show_scenarios: -w option cannot be specified more than once.\n");
        }
        //check_if_more_option(uflag, aflag, wflag);
        if(uflag || aflag )
          usage("-w option cannot be used with other options");
        wflag++;
        all = 0;
        strcpy(arg_buf, optarg);
        NSLB_SET_W_OPTION(arg_buf);
        //set search depth level
        switch(get_tokens1(optarg, field, "/"))
        {
          case 1:
          // ie. -w workspace/ only
          SET_MIN_DEPTH(6)
          SET_MAX_DEPTH(6)
          break;
 
          // ie. -w workspace/profile o
          default:
          SET_MIN_DEPTH(4)
          SET_MAX_DEPTH(4)
        }
        break;

      case 't': // Testsuit used in testcase
        if(pflag || aflag || uflag)
          usage("-t cannot be used with options  other than  -w or cannot be given more than once");
        strcpy(tsuites_name, optarg); 
        tflag++;
        break;


      case 'D':
        debug_level = atoi(optarg);
        break;

      case ':':
      case '?':
        usage(0);
    }
  }



  printf("%s|Project|Subproject|Test Case|Scenario|Description|Owner|Group|Permission|Modification Date\n", WORK_PROFILE); 
 
  get_usr_uid_gid();

 //set ta path
  nslb_check_and_set_ta_dir( aflag, wflag, pflag,  NSLB_GET_W_OPTION() );
  sprintf(testcase_dir, "%s", GET_NS_TA_DIR());
  //case when to show form testsuite file.
  if(tflag)
  {
    sprintf(tsuites_dir, "%s", GET_NS_TA_DIR());
    get_testcase_from_testsuites(tsuites_name);
    show_testcase_using_array(tflag);
  }
  else
  {  
    get_all_testcase_names();
    show_testcase_using_file_list();
  }

  remove(testcase_name_list_file);
  return 0;
}
