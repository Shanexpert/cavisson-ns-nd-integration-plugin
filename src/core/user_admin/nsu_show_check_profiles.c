/*----------------------------------------------------------------------
   Name    : nsu_show_check_profiles.c
   Author  : Naveen Raina 
   Purpose : This file is to show check profiles according to project/sub-project details.
   Usage   : nsu_show_check_profiles [-A | -u <User Name> | -p <proj1/sub_proj1> ]
   Format of Output:
             Project|Subproject|Script|Owner|Group|Permission|Modification Date
             Where Modification Date will be in "YYYY-MM-DD HH:MM:SS" (24 hour format)
   To Compile: gcc -o -g nsu_show_check_profiles nsu_show_check_profiles.c
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

#include "../../libnscore/nslb_log.h" 
#include "../../libnscore/nslb_util.h" 

#define MAX_LINE_LENGTH 2048
#define MAX_STR_LENGTH 256

static char chk_prof_list_file[MAX_STR_LENGTH];
static int all = 1;

static char chk_prof_dir[MAX_LINE_LENGTH];

static int num_projs = 0;
static char usr_project[1000][255 + 1];
static char usr_sub_project[1000][255 + 1];

static gid_t usr_primary_grp_id;
static gid_t usr_secondry_grp_id;
static uid_t usr_id;

static int debug_level = 0;     // Debug log level
static FILE *debug_file = NULL; // Debug log file

static char g_wdir[1024];
#define SET_WDIR(ptr) strcpy(g_wdir, ptr);
#define GET_WDIR() g_wdir

static void usage(char *err_msg)
{

  fprintf(stderr, "%s\n", err_msg);
  fprintf(stderr, "Usage: \n");
  fprintf(stderr, "     : nsu_show_check_profiles [-A | -u <User Name> | -p <workspace>/<profile>/<proj1/sub_proj1> -p <workspace>/<profile>/<proj2/sub_proj2> . .] \n");
  fprintf(stderr, "Where: \n");
  fprintf(stderr, "     : -A is used to show all scenario, project/sub-project details that present in default checkprofile.\n");
  fprintf(stderr, "     : -u is used to show checkprofiles specified User Name.\n");
  fprintf(stderr, "     : -p is used to show checkprofiles of specified <workspace>/<profile>/proj/sub_proj.\n");
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

static void init()
{
  char *work_dir = "/home/cavisson/work";

  if (getenv("NS_WDIR") != NULL)
    work_dir = (getenv("NS_WDIR"));
  SET_WDIR(work_dir)
  sprintf(chk_prof_dir, "%s/", work_dir);

  sprintf(chk_prof_list_file, "/tmp/chk_prof_list.%d", getpid());
  LIB_DEBUG_LOG(1, debug_level, debug_file, "chk_prof_dir = %s", chk_prof_dir);

}

static void open_debug_log_file(char *user_name)
{
char debug_file_path[4096];

  // Do not open if debug is off
  if(debug_level <= 0) return;

  // Create unique file for each user so that we do have permission issues
  sprintf(debug_file_path, "/tmp/%s_nsu_show_check_profiles_debug", user_name);

  if(!(debug_file = fopen(debug_file_path, "a")))
  {
    fprintf(stderr, "nsu_show_check_profiles: Error in opening debug file (%s). Error = %s\n", debug_file_path, strerror(errno));
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

  sprintf(cmd, "nsu_check_user %s nsu_show_check_profiles", usr);
  if(system(cmd) != 0)
  {
    //fprintf(stderr, "Not a valid user : %s\n", usr);
    LIB_DEBUG_LOG(1, debug_level, debug_file, "Error in running command = %s", cmd);;
    exit (-1);
  }

  //show all if user is admin or netstorm
  // TODO: we should remove it
  //
  if(!strcmp(usr, "admin") || !strcmp(usr, "cavisson"))
  {
    LIB_DEBUG_LOG(1, debug_level, debug_file, "Setting all for admin/cavisson user id");
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

/*  nsu_show_projects output -->
    User Name|Project|Subproject
    NA|a|aaaa
    NA|abc|xyz
    NA|default1|default1
    NA|default|default
    NA|gaurav_project|gaurav_sub_project1
*/
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
    //printf("i = %d, buff = %s\n", i, buff);
    buff[strlen(buff) -1]='\0';
    get_tokens1(buff, temp, "|");
    if(!strcmp(temp[1], "All"))
    {
     // LIB_DEBUG_LOG(1, debug_level, stdout, "User project is ALL");
      all = 1;
      break; 
    }
    //<user_name>|<Profile>|<proj>|<subproj>
    //cavisson|default|pro1|subproj1
    char *default_ws = "admin";
    if(!strcmp(temp[0], "cavisson"))
       temp[0] = default_ws;
    nslb_set_ta_dir_ex1(GET_WDIR(), temp[0], temp[1]);
    strcpy(usr_project[i], temp[2]);
    strcpy(usr_sub_project[i], temp[3]);
    LIB_DEBUG_LOG(1, debug_level, debug_file, "User project = %s, subproject = %s", usr_project[i], usr_sub_project[i]);
    i++; 
  }

  num_projs = i; 
  pclose(fp);
}

//make list of check profiles 
static void get_all_checkprofile_names()
{
  char cmd[MAX_STR_LENGTH]="\0";

  // Run command to get list of all checkprofiles in ascending order of project/subproject 
  // and within same subproject, ascending order of script name
  // Also remove any hidden project or script names (starting with .)
  // find /home/netstorm/work/checkprofiles/ -mindepth 3 -maxdepth 3 -type d | grep -v ^.*/[.] | sort -t '/' +4 -5 -k 5,6 -k 6,7
  //
  sprintf(cmd, "find -L %s -mindepth 4 -maxdepth 4 -name *.cprof | grep -v ^.*/[.] | sort -t '/' +4 -5 -k 5,6 -k 6,7 >%s", GET_NS_TA_DIR(), chk_prof_list_file);
 
  if(system(cmd) == -1)
  {
     fprintf(stderr, "Error: Can not find script names.\n");
     exit(-1);
  }
}

//extract proj, subproj from script path
// path is full path like /home/netstorm/work/workspace/<workspace_name>/<profile>/cavisson/proj/subproj/checkprofile/name

static void get_proj_sub_proj_name(char *path, char *profile, char *project, char *sub_project, char *name, int tflag)
{
  char *fields[30];
  char buf[MAX_STR_LENGTH]="\0";
  
  LIB_DEBUG_LOG(1, debug_level, debug_file,"Method called, path = %s", path);
  strcpy(buf, path);
  get_tokens1(buf, fields, "/");

  strcpy(profile, fields[5]);
  strcpy(project, fields[7]);
  strcpy(sub_project, fields[8]);
  strcpy(name, fields[10]);
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


//This method will get the project/subproject/scenario name from suumary.top file and fill it in field
int nslib_read_summary_top(char *file_name, char *fields[])
{
  FILE *fp;
  int totalFlds;
  char line_buf[8192];
 
  if((fp = nslib_open_file (file_name, "r")) == NULL)
  {
    fprintf(stderr, "Error in opening %s\n", file_name);
    exit(-1);
  }

  if(fgets(line_buf, 8192, fp) == NULL)
  {
    //printf("Error: file is empty\n");
    fclose(fp);
    return -1; 
  }
  fclose(fp);

  totalFlds = get_tokens1(line_buf, fields, "|");  
  if (totalFlds == 0)
  {
    //printf("Error: %s file is empty\n");
    return -1;
  }
  //if (totalFlds < 15)
  //{
    //printf("Error: Number of fields are not correct in %s file\n");
   // return 1;
  //}
  return 0;
}


int nslib_is_valid_test_run(int TRNum, char *NSLogsPath)
{
  char file_name[2024];
  DIR *dir;

  sprintf(file_name, "%s/TR%d/", NSLogsPath, TRNum);
  if((dir = opendir(file_name)) == NULL)
  {
    return 0; // Not valid
  }
  closedir(dir);

  return 1; //valid test run
}

//get script owner if pw->pw_name is NULL set owner as UNKNOWN
static void get_checkprofile_owner(uid_t user_id, char *own)
{
  struct passwd *pw;
  pw = getpwuid(user_id);
  if(pw)
    strcpy(own, pw->pw_name);
  else
    strcpy(own, "UNKNOWN");
}

//get script group, if gp->gr_name is NULL set owner as UNKNOWN
static void get_checkprofile_group(gid_t group_id, char *grp)
{
  struct group *gp = getgrgid (group_id);
  if(gp)
    strcpy(grp, gp->gr_name);
  else
    strcpy(grp, "UNKNOWN");
}

//if running user & script owner is same then use permisson of owner
//if above condition fails then chek groups(all groups) with groups of script, if same use permisson of group
//els use permisson of others
static void get_checkprofile_permission(uid_t scruid,  gid_t scr_gid, mode_t mode, char *perm)
{
  //if user is root or admin we should display RWD
  if(!usr_id)
  {
    strcpy(perm, "RWD");
    return;
  }

  if(scruid == usr_id)
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

//modification time when contents of dir are updated, & change time is that when permission has changed
//we have used  modification time (mtime)
static void get_checkprofile_modification_time(time_t *modification_time, char *time_format)
{
  struct tm *mod_time;
  mod_time = localtime(modification_time);
  //sprintf(time_format, "%d-%02d-%02d %02d:%02d:%02d", mod_time->tm_year + 1900, mod_time->tm_mon + 1, mod_time->tm_mday, mod_time->tm_hour, mod_time->tm_min, mod_time->tm_sec);
  //show date & time in format mm/dd/yy HH:MM:SS
  sprintf(time_format, "%02d/%02d/%d %02d:%02d:%02d", mod_time->tm_mon + 1,  mod_time->tm_mday, mod_time->tm_year + 1900, mod_time->tm_hour, mod_time->tm_min, mod_time->tm_sec);
}

static void get_checkprofile_stats(char *path)
{
  struct stat script_det;
  char owner[MAX_STR_LENGTH] = "\0";
  char group[MAX_STR_LENGTH] = "\0";
  char permission[MAX_STR_LENGTH] = "\0";
  char modification_time[MAX_STR_LENGTH] = "\0";

  LIB_DEBUG_LOG(1, debug_level, debug_file, "path = %s", path);
  if(stat(path, &script_det) != 0)
  {
     fprintf(stderr, "Error in getting stat of dir %s\n", path);
     return;
  }
  get_checkprofile_owner(script_det.st_uid, owner);
  get_checkprofile_group(script_det.st_gid, group);
  get_checkprofile_permission(script_det.st_uid, script_det.st_gid, script_det.st_mode, permission);
  get_checkprofile_modification_time(&script_det.st_mtime, modification_time);
  printf("%s|%s|%s|%s\n", owner, group, permission, modification_time);
}

static int is_checkprofile_to_show(char *project, char *sub_project, char *script)
{
  int i;
   
  for(i = 0; i < num_projs; i++)
  {
    if(!strcasecmp(project, usr_project[i]) && (!strcasecmp(usr_sub_project[i], "All") || !strcasecmp(sub_project, usr_sub_project[i])))
        return 0; //show
  }
 
  return 1;
}

static void  show_checkprofile_using_file_list()
{
  FILE *chk_prof_list;
  char chk_prof_path[MAX_LINE_LENGTH] = "\0";
  char project[MAX_STR_LENGTH] = "\0";
  char sub_project[MAX_STR_LENGTH] = "\0";
  char chk_prof_name[MAX_LINE_LENGTH] = "\0";
  char profile[MAX_STR_LENGTH] = "\0";
  chk_prof_list = fopen(chk_prof_list_file, "r");
  if(chk_prof_list == NULL)
  {
    fprintf(stderr, "Unable to open %s file.\n", chk_prof_list_file);
    exit(-1);
  }
  while(fgets(chk_prof_path, MAX_LINE_LENGTH, chk_prof_list)) 
  {
    chk_prof_path[strlen(chk_prof_path) - 1]='\0';
    get_proj_sub_proj_name(chk_prof_path, profile, project, sub_project, chk_prof_name, 0);
    if(!all) 
    {
       if(is_checkprofile_to_show(project, sub_project, chk_prof_name))
       {
         continue;
       }
    }
    printf("%s|%s|%s|%s|", profile, project, sub_project, chk_prof_name);
    get_checkprofile_stats(chk_prof_path);
  }
}


int main(int argc, char *argv[])
{
  char option;
  char user[128];
  char *field[10];
  int pflag = 0;
  int uflag = 0;
  int aflag = 0;

  init();
  while ((option = getopt(argc, argv, "Au:p:")) != -1)
  {
    switch (option)
    {
      case 'A': // All checkprofiles
        if(pflag || uflag || aflag)
          usage("-A cannot be used with options or cannot be given more than once");

        all = 1;
        aflag++;
        nslb_set_ta_dir_ex1(GET_WDIR(), GET_DEFAULT_WORKSPACE(), GET_DEFAULT_PROFILE());
        break;

      case 'u': // checkprofiles of all project/subproject assigned for the user
        if(pflag || uflag || aflag)
          usage("-u cannot be used with options or cannot be given more than once");
          
        all = 0; 
        strcpy(user, optarg);
        validate_user(user);
        get_users_proj_sub_proj(user);
        uflag++;
        break;

      case 'p': // checkprofiles of project/subproject passed
        if(uflag || aflag)
          usage("-p option cannot be used with other options");

	//if all is already set to 1 by previous -p All/All, then we need not to check nxt -p option 
	if(all && pflag)
	 break;

        if(get_tokens1(optarg, field, "/") != 4)
          usage("with -p the format should be <workspace>/<profile>/project/subproject");
        nslb_set_ta_dir_ex1(GET_WDIR(), field[0], field[1]);
        strcpy(usr_project[pflag], field[2]);
        strcpy(usr_sub_project[pflag], field[3]);
	//if proj is all show all 
	if(!strcmp(usr_project[pflag], "All"))
	  all = 1;
	else
          all = 0;
        pflag++;
        num_projs = pflag;
        break;

      case ':':
      case '?':
        usage(0);
    }
  }

  printf("%s|Project|Subproject|CheckProfile|Owner|Group|Permission|Modification Date\n", WORK_PROFILE); 
 
  get_usr_uid_gid();

  get_all_checkprofile_names();
  show_checkprofile_using_file_list();

  remove(chk_prof_list_file);
  return 0;
}
