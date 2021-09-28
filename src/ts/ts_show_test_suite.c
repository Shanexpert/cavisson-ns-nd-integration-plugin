/*----------------------------------------------------------------------
   Name    : ts_show_test_suite.c
   Author  : Narendra
   Purpose : This file is to show testsuites according project/sub-project details.
   Usage   : ts_show_test_suite [-A | -u <User Name> | -p <project/subproject>][-s <days>][-k <search keyword>]
   Format of Output:
             Project|Subproject|Test Suites|Modification Date|TestCase Count|Owner|Group|Permission
             Where Modification Date will be in "YYYY-MM-DD HH:MM:SS" (24 hour format)
   To Compile: gcc -o -g ts_show_test_suite ts_show_test_suite.c
----------------------------------------------------------------------*/
#define _XOPEN_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <grp.h>
#include <pwd.h>
#include <ctype.h>

// for get tokens
#include "../../base/libnscore/nslb_util.h"

#define MAX_STR_LENGTH 256

static int all = 0;
static int num_projs = 0;
char testsuites_dir[1024];
static char usr_project[1000][255 + 1]; //We need to make this dynamically
static char usr_sub_project[1000][255 + 1];

static int last_n_days = -1;
static char testsuites_search_key[1024] = "\0";

static char *search_keys[100];
static int num_search_key = 0;
gid_t usr_primary_grp_id;
gid_t usr_secondry_grp_id;
uid_t usr_id;


static void validate_user(char *user_name)
{
  char cmd[MAX_STR_LENGTH]="\0";

  sprintf(cmd, "nsu_check_user %s ts_show_test_suite", user_name);
  if(system(cmd) != 0)
  {
    exit (-1);
  }
  if(!all)
  {
    if(!strcmp(user_name, "admin") || !strcmp(user_name, "netstorm"))
    all = 1;
  }
}

// This method to give error Usage message and exit
void display_help_and_exit()
{
  printf("Usage: ts_show_test_suite [-A | -u <User Name>] [-p <project/subproject>] [-w <workspace_name>/<profile>] [-s <days>][-k <search keyword>]\n");
  printf("Where: \n");
  printf("  -A is used to show all test suites, project/sub-project details that present in test suites at default profile\n");
  printf("  -u is used to show test suites, <workspace>/<profile>/project/sub-project details of specified User Name\n");
  printf("  -p is used to show test suites of given <project>/<sub-project> of specified workspace/profile with -w  option\n");
  printf("  -w is used to show test suites of given <workspace>/<profile>\n");
  printf("  -s is used to show test suites of last s days modified\n");
  printf("  -k is used to show test suites which match with given keyword\n");
  printf("   At a perticular time one of the option from -A,-u or -p should be used \n");
  printf("  -s and -k options should be used with one of the option -A,-u or -p\n");
  exit(-1);
}

static void get_usr_uid_gid()
{
  FILE *fp;
  char cmd[MAX_STR_LENGTH]="\0";
  char buff[MAX_STR_LENGTH]="\0";
  struct passwd *pw;

  usr_id = getuid();
 
  pw = getpwuid(usr_id);

  sprintf(cmd, "id %s -G", pw->pw_name);
  
  fp = popen(cmd, "r");
  if(fp == NULL)
  {
    fprintf(stderr, "Error: Can not get groups id.\n"); 
    perror("popen");
  }

  if(fgets(buff, 1024, fp)!= NULL )
   sscanf(buff, "%d %d", &usr_primary_grp_id, &usr_secondry_grp_id);
}


int get_tokens1(char *read_buf, char *fields[], char *token, int max_flds)
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
      totalFlds = max_flds;
      break;  /* break from while */
    }
    fields[totalFlds - 1] = token_ptr;
  }
  return(totalFlds);
}


int ns_is_numeric1(char *str)
{
  int i;
  for(i = 0; i < strlen(str); i++) {
    if(!isdigit(str[i])) return 0;
  }
  return 1;
}




//make array of proj subproj to match
static void get_users_proj_sub_proj(char *user)
{
  char cmd_name[128]="\0";  
  char buff[512]="\0";
  FILE *fp;
  char *temp[10];
  int i = 0;
  sprintf(cmd_name, "nsu_show_projects -u %s| grep -v \"^User Name\"", user);
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
    get_tokens1(buff, temp, "|", 10);
    if(!strcmp(temp[1], "All"))
    {
      all = 1; 
      return;
    }
    //<user_name>|<Profile>|<proj>|<subproj>
    //cavisson|default|pro1|subproj1
    char *default_ws = "admin";
    if(!strcmp(temp[0], "cavisson"))
       temp[0] = default_ws;
    nslb_set_ta_dir_ex1(NSLB_GET_WDIR(), temp[0], temp[1]);

    strcpy(usr_project[i], temp[2]);
    
    if(!strcmp(temp[3], "All")) {
      strcpy(usr_sub_project[i], "*");
    } else {
      strcpy(usr_sub_project[i], temp[3]);
    }
    i++;
  }
  num_projs = i; 
  pclose(fp);
}


//get testsuite owner if pw->pw_name is NULL set owner as UNKNOWN
static void get_testsuites_owner(uid_t user_id, char *own)
{
  struct passwd *pw;
  pw = getpwuid(user_id);
  if(pw)
    strcpy(own, pw->pw_name);
  else
    strcpy(own, "UNKNOWN");
}

//get testsuite group, if gp->gr_name is NULL set owner as UNKNOWN
static void get_testsuites_group(gid_t group_id, char *grp)
{
  struct group *gp = getgrgid (group_id);
  if(gp)
    strcpy(grp, gp->gr_name);
  else
    strcpy(grp, "UNKNOWN");
}

//if running user & test suite owner is same then use permisson of owner
//if above condition fails then chek groups if same use permisson of group
//els use permisson of others
static void get_testsuites_permission(uid_t scruid,  gid_t scr_gid, mode_t mode, char *perm)
{
  //if user is root or admin we should display RWD
  if(!usr_id)
  {
    strcpy(perm, "RWD");
    return;
  }

  if(scruid == usr_id)
  {
    //permisson of owner
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
  else if (scr_gid == usr_primary_grp_id || scr_gid == usr_secondry_grp_id)
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

//Modification time when contents of dir are updated, & change time is that when permission has changed
//we have used  modification time (mtime)
static void get_testsuites_modification_time(time_t *modification_time, char *time_format)
{
  struct tm *mod_time;
  mod_time = localtime(modification_time);
  //sprintf(time_format, "%d-%02d-%02d %02d:%02d:%02d", mod_time->tm_year + 1900, mod_time->tm_mon + 1, mod_time->tm_mday, mod_time->tm_hour, mod_time->tm_min, mod_time->tm_sec);
  //shows day in mm/dd/yyyy HH:MM:SS 
  sprintf(time_format, "%02d/%02d/%d %02d:%02d:%02d", mod_time->tm_mon + 1, mod_time->tm_mday, mod_time->tm_year + 1900, mod_time->tm_hour, mod_time->tm_min, mod_time->tm_sec);
}

static void get_testcase_count_and_testsuites_stats(char *path)
{
  struct stat testsuites_det;
  char owner[MAX_STR_LENGTH] = "\0";
  char group[MAX_STR_LENGTH] = "\0";
  char permission[MAX_STR_LENGTH] = "\0";
  char modification_time[MAX_STR_LENGTH] = "\0";
  char buf[2048 + 1] = "\0";
  FILE *fp = NULL;
  int testcase_count = 0;
  if(stat(path, &testsuites_det) != 0)
  {
     fprintf(stderr, "Error in getting stat of dir %s\n", path);
     return;
  }
  if((fp = fopen(path, "r")) == NULL)
  {
     fprintf(stderr, "Error in opening file = %s\n", path);
     return;
  }
  while(fgets(buf, 2048, fp) != NULL)
  {
    if(strncmp(buf, "TEST_CASE_NAME", 14) == 0)
      testcase_count ++;
  }
  fclose(fp);
  get_testsuites_owner(testsuites_det.st_uid, owner);
  get_testsuites_group(testsuites_det.st_gid, group);
  get_testsuites_permission(testsuites_det.st_uid, testsuites_det.st_gid, testsuites_det.st_mode, permission);
  get_testsuites_modification_time(&testsuites_det.st_mtime, modification_time);
  printf("%s|%d|%s|%s|%s\n", modification_time, testcase_count, owner, group, permission);
}

#if 0
static int is_scenario_to_show(char *project, char *sub_project)
{
  int i;
  for(i = 0; i < num_projs; i++)
    if(!strcasecmp(project, usr_project[i]) && ( !strcasecmp(usr_sub_project[i], "All") || !strcasecmp(sub_project, usr_sub_project[i])))
      return 0; //show
 
  return 1;
}
#endif

//This method to check options if used more
void check_if_more_option(int uflag, int Aflag, int pflag)
{
  if ((uflag == 1) || (Aflag == 1) || (pflag == 1))
  {
    printf("ts_show_test_suite: Only one option can specify at a time.\n");
    display_help_and_exit();
  }
}


//creating shell command to fetch scenarios of given condition
static void create_tsuites_list_cmd(char *cmd) {
 int i;
 char create_path_buf[4096];
 char last_n_days_buf[32];
 static char search_key_buf[4096];
 static char buf[512];
 int search_depth;

 cmd[0] = create_path_buf[0] = '\0';

 //for -w option, it is introduced
 int max_search_depth;
 // /home/cavisson/work/workspace/<workspace_name>
 // /home/cavisson/work/workspace/<workspace_name>/<profile_name>
 ///home/cavisson/work/workspace/<workspace_name>/<profile_name>/cavisson/<proj>/<sub_proj>/testsuites
 // /home/cavisson/work/workspace/<workspace_name>/<profile_name>/cavisson/<proj>/<sub_proj>/testsuites/<testconfig.conf> ==> depth =11

 if(all) {
   search_depth = 4;
   strcpy(create_path_buf, testsuites_dir);
   max_search_depth = search_depth + 1;
 } else {
   search_depth = 1;
   if(num_projs) {
    max_search_depth = search_depth + 1;
    for(i=0; i < num_projs; i++)
       sprintf(create_path_buf,"%s/%s/%s/%s", testsuites_dir, usr_project[i], usr_sub_project[i], "testsuites");
   }
   //in case only -w option provided, so num_projs would be zero
   else {
     strcpy(create_path_buf, testsuites_dir);
     max_search_depth = 6;
   }
 }

 if(last_n_days > 0)
   sprintf(last_n_days_buf,"-mtime -%d", last_n_days);
 else
   last_n_days_buf[0] = '\0';

 if(testsuites_search_key[0] == '\0')
   sprintf(search_key_buf, "-iname \"*.conf\"");
   else{
     strcat(search_key_buf,"\\( ");     
     for(i=0;i<num_search_key;i++){
       sprintf(buf,"-iname \"*%s*.conf\" -o ",search_keys[i]);
       strcat(search_key_buf,buf);
     }
     search_key_buf[strlen(search_key_buf)-3]='\0'; 
     strcat(search_key_buf,"\\)");
   }
 sprintf(cmd, "TSUITS_LIST=`find -L %s -maxdepth %d -mindepth %d %s %s | grep %s 2>/dev/null`;"
              "RET=$?;"
              "if [ \"X$TSUITS_LIST\" != \"X\" ];then"
              "   ls -1t $TSUITS_LIST;"
              "fi;" 
              "exit $RET",
              create_path_buf, max_search_depth, search_depth,
              last_n_days_buf, search_key_buf, "testsuites");
}

static void print_testsuites(char *cmd) {
  FILE *fp;
  //char testsuites_name[2024];
  char *fields[12];
  char buf[2048 + 1]="\0";
  char testsuites_path[2048 + 1]="\0";
  //char conf_file[MAX_STR_LENGTH]="\0";
  //char* fptr;
  int num_fields;
  int tsuites_len;

  fp = popen(cmd, "r");
  if(fp == NULL) {
    perror("ts_show_test_suite: popen"); //ERROR: popen failed
    exit(-1);
  } else {
      while(fgets(buf, 2048, fp)!= NULL ) {
        buf[strlen(buf) - 1] = '\0';  //// Replacing new line by null
        strcpy(testsuites_path, buf);
        num_fields = get_tokens1(buf, fields, "/",  12);
        if(num_fields < 11)
          continue;

       // /home/cavisson/work/workspace/<workspace_name>/<profile_name>/cavisson/<proj>/<sub_proj>/testsuites/<testconfig.conf> ==> depth =11
        char profile_name[NS_PROFILE_SIZE];
        nslb_get_profile_name(fields[5], profile_name);
 
        tsuites_len = strlen(fields[10]) - 5; // Not including .conf
        printf("%s|%s|%s|%*.*s|", profile_name, fields[7], fields[8], tsuites_len, tsuites_len, fields[10]);
        get_testcase_count_and_testsuites_stats(testsuites_path);
      }
  }
  pclose(fp);
}


int main(int argc, char *argv[]) {
  char option;
  char arg_buf[1024];
  char user[10];
  int num_fields,i;
  char copy_key[50];
  char *fields[4];
  int uflag = 0, Aflag = 0, sflag = 0, kflag = 0, pflag = 0, wflag = 0;

  if(argc < 2) {
    fprintf(stderr," Error: provide at least one argument according to following condition\n");
    display_help_and_exit();
  }
  /*bug id: 101320: set ns_wdir and default workspace and profile*/
  nslb_init_wdir_wpdir();
  while ((option = getopt(argc, argv, "Au:p:w:s:k:")) != -1)
  {
    switch (option)
    {
      case 'A':
        if (Aflag)
        {
          printf("ts_show_test_suite: -A option cannot be specified more than once.\n");
          display_help_and_exit();
        }
        check_if_more_option(uflag, Aflag, pflag);
        NSLB_CHECK_IF_W_OPTION(wflag, argv[0])
        Aflag++;
        all = 1;
        break;

      case 'u':
        if (uflag)
        {
          printf("ts_show_test_suite: -u option cannot be specified more than once.\n");
          display_help_and_exit();
        }
        check_if_more_option(uflag, Aflag, pflag);
        NSLB_CHECK_IF_W_OPTION(wflag, argv[0])
        uflag++;
        all = 0;
        strcpy(user, optarg);
        validate_user(user);
        get_users_proj_sub_proj(user);
        break;

      case 'p':
        if(pflag)
        {
          printf("nsu_show_scenarios: -p option cannot be specified more than once.\n");
          display_help_and_exit();
        }
        check_if_more_option(uflag, Aflag, pflag);
        pflag++;
        all = 0;
        strcpy(arg_buf, optarg);
        num_fields = get_tokens(arg_buf, fields, "/", 3);
        //<proj>/<sub_proj>
        if(num_fields == 0)
          display_help_and_exit();

        /* if sub project is not given we set * as it will help in find to put path*/
        if(num_fields == 1)
          strcpy(usr_sub_project[0], "*");
        else
          strcpy(usr_sub_project[0], fields[1]);

        strcpy(usr_project[0], fields[0]);
        num_projs = 1;
        break;

    case 'w':
        if(wflag)
        {
          printf("nsu_show_scenarios: -w option cannot be specified more than once.\n");
          display_help_and_exit();
        }
        check_if_more_option(uflag, Aflag, wflag);
        wflag++;
        strcpy(arg_buf, optarg);
        NSLB_SET_W_OPTION(arg_buf);
        break;

      case 's':
        if(sflag)
        {
         printf("ts_show_test_suite: -s option cannot be specified more than once.\n");
         display_help_and_exit();
        }
        sflag++;
        if(ns_is_numeric1(optarg) == 0) {
          fprintf(stderr, "Error: No of days can have only integer value\n");
          exit(-1);
        }
        last_n_days = atoi(optarg);
        break;

      case 'k':
        if(kflag)
        {
          printf("ts_show_test_suite: -k option cannot be specified more than once.\n");
          display_help_and_exit();
        }
        kflag++;
        strcpy(testsuites_search_key, optarg);
        num_search_key = get_tokens1(testsuites_search_key, search_keys, " ", 100);
    	
//To search multiple keyword

	if(num_search_key > 3)
          num_search_key=3;
        for(i=0;i<num_search_key;i++){
	  if(strlen(search_keys[i])>32){
	    strncpy(copy_key,search_keys[i],32);
	    strcpy(search_keys[i],copy_key);
          }
	}

        break;
      case ':':
      case '?':
        display_help_and_exit();
    }
  }
  //-s or -k option should not be given without -u, -p or -A
  if(sflag || kflag){
    if(uflag || pflag || Aflag || wflag){}
    else{
      fprintf(stderr,"Error: -s or -k option should not be given independently.\n ");
      exit(1);
    }
  }
    
  get_usr_uid_gid();
  //set ta path
  nslb_check_and_set_ta_dir( Aflag, wflag, pflag,  NSLB_GET_W_OPTION() );
  strcpy(testsuites_dir, GET_NS_TA_DIR());

  if(chdir(testsuites_dir)) {
    fprintf(stderr, "Error: ts_show_test_suite: Unable to chdir on %s\n", testsuites_dir); 
    exit(1);
  }

  printf("%s|Project|Subproject|Test Suites|Modification Date|TestCase Count|Owner|Group|Permission\n", WORK_PROFILE);
  char cmd[4096];
  create_tsuites_list_cmd(cmd);
  print_testsuites(cmd);

  return 0;
}
