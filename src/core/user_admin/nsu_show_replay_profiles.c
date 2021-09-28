/*----------------------------------------------------------------------
   Name    : nsu_show_profiles.c
   Author  : Shalu 
   Purpose : This file is to show profiles according project/sub-project details.
   Usage   : nsu_show_profiles [-A | -u <User Name> | -p <proj1/sub_proj1> -p <proj2/sub_proj2> . . .]
   Format of Output:
             Project|Subproject|Profile|Owner|Group|Permission|Modification Date
             Where Modification Date will be in "YYYY-MM-DD HH:MM:SS" (24 hour format)
   To Compile: gcc -o -g nsu_show_profiles nsu_show_profiles.c
   Modification History:
             02/17/09:  ArunN - Initial Version
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

static char profile_name_list_file[MAX_STR_LENGTH];
static int all = 1;

static char scen_dir[MAX_LINE_LENGTH];
static char tr_dir[MAX_LINE_LENGTH];

static int num_projs = 0;
static char usr_project[1000][255 + 1];
static char usr_sub_project[1000][255 + 1];

// These two variables are used to store profiles from scenario file or test run
// It also uses usr_project and usr_sub_project array to keep the project/sub project name of scen or TR
static int profile_idx = 0; // index in usr_profile while parsing input and filing in usr_profile
static char usr_profile[1000][1000 + 1];


static gid_t usr_primary_grp_id;
static gid_t usr_secondry_grp_id;
static uid_t usr_id;

static int debug_level = 0;     // Debug log level
static FILE *debug_file = NULL; // Debug log file

/*bug id: 101320: Support new dir struc*/
static char g_wdir[1024];
#define GET_WDIR() g_wdir
#define SET_WDIR(ptr) sprintf(g_wdir, "%s", ptr);

static void usage(char *err_msg)
{

  fprintf(stderr, "%s\n", err_msg);
  fprintf(stderr, "Usage: \n");
  fprintf(stderr, "     : nsu_show_profiles [-A | -u <User Name> | -p <proj1/sub_proj1> | -w <workspace>/<profile> | -s <proj1/subproj1/scenario name | -t <TRNumber1>. . .] \n");
  fprintf(stderr, "Where: \n");
  fprintf(stderr, "     : -A is used to show all scenario, project/sub-project details that present in default profiles.\n");
  fprintf(stderr, "     : -u is used to show profiles specified User Name.\n");
  fprintf(stderr, "     : -p is used to show profiles of specified </proj/sub_proj>.\n");
  fprintf(stderr, "     : -s is used to show profiles used in specified scenario placed in <proj2/subproj2/scenarioname> \n");
  fprintf(stderr, "     : -t is used to show profiles used in specified test run.\n");
  fprintf(stderr, "     : -w is used to show all workspace profile releated replay_profiles\n");
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
  sprintf(tr_dir, "%s/logs", work_dir);

  NSLB_SET_W_OPTION("admin/system");
  nslb_init_wdir_wpdir();
  sprintf(profile_name_list_file, "/tmp/profile_list.%d", getpid());
}

static void open_debug_log_file(char *user_name)
{
char debug_file_path[4096];

  // Do not open if debug is off
  if(debug_level <= 0) return;

  // Create unique file for each user so that we do have permission issues
  sprintf(debug_file_path, "/tmp/%s_nsu_show_profiles_debug", user_name);

  if(!(debug_file = fopen(debug_file_path, "a")))
  {
    fprintf(stderr, "nsu_show_profiles: Error in opening debug file (%s). Error = %s\n", debug_file_path, strerror(errno));
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

  sprintf(cmd, "nsu_check_user %s nsu_show_profiles", usr);
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

//  printf("get_users_proj_sub_proj called.\n");

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
    //set test assets dir
    nslb_set_ta_dir_ex1(GET_WDIR(), temp[0], temp[1]);
    strcpy(usr_project[i], temp[2]);
    strcpy(usr_sub_project[i], temp[3]);
    //set_replay_profile_dir(GET_NS_TA_DIR(), usr_project[i], usr_sub_project[i]);
    LIB_DEBUG_LOG(1, debug_level, debug_file, "User project = %s, subproject = %s", usr_project[i], usr_sub_project[i]);
    i++; 
  }

  num_projs = i; 
  pclose(fp);
}

//make list of profiles
static void get_all_profile_names()
{
  char cmd[MAX_STR_LENGTH]="\0";
  
  
  // Now we will not search for profile.capture we will just show the script name as new script design dont have any script.capture

  // Run command to get list of all profiles in ascending order of project/subproject 
  // and within same subproject, ascending order of profile name
  // Also remove any hidden project or profile names (starting with .)
  // find /home/netstorm/work/profiles/ -mindepth 3 -maxdepth 3 -type d | grep -v ^.*/[.] | sort -t '/' +4 -5 -k 5,6 -k 6,7
  //
    /*bug id: 101320 : use profile dir path as per tes assets dir*/

  sprintf(cmd, "find -L %s -mindepth 4 -maxdepth 4 -type d | grep -v ^.*/[.] | awk -F '/' '($11==\"replay_profiles\")' | sort -t '/' >%s", GET_NS_TA_DIR(), profile_name_list_file);
 
  if(system(cmd) == -1)
  {
     fprintf(stderr, "Error: Can not find profile names.\n");
     exit(-1);
  }
}

//extract proj, subproj from profile path
//path would be like: /home/cavisson/work/workspace/<workspae_name>/<profile_name>/cavisson/<proj>/<sub_proj>/replay_profiles/<ral_log_test>
static void get_proj_sub_proj_name(char *path, char *work_profile, char *project, char *sub_project, char *name, int tflag)
{
  char *fields[30];
  char buf[MAX_STR_LENGTH]="\0";
  
  LIB_DEBUG_LOG(1, debug_level, debug_file,"Method called, path = %s", path);
  strcpy(buf, path);
  get_tokens1(buf, fields, "/");

  if(tflag)
  {
    strcpy(project, usr_project[num_projs - 1]);
    strcpy(sub_project, usr_sub_project[num_projs - 1]);
    strcpy(name, fields[6]);
  }
  else{
    strcpy(work_profile, fields[5]);
    strcpy(project, fields[7]);
    strcpy(sub_project, fields[8]);
    strcpy(name, fields[10]);
    }
}


//This method will add project/subproject/profile in the array
static int add_profile_in_array(char *profile_name, int tflag, int tr_num)
{
  char profile_file_path[MAX_STR_LENGTH];
  int i;

  // Make absolute path
  
    LIB_DEBUG_LOG(1, debug_level, debug_file," usr_project[%d] = %s usr_sub_project[%d] = %s profile_name = %s",  num_projs - 1, usr_project[num_projs - 1], num_projs - 1, usr_sub_project[num_projs - 1], profile_name);
    if(tflag)
      sprintf(profile_file_path, "%s/TR%d/replay_data/%s", tr_dir, tr_num, profile_name);
    else
      sprintf(profile_file_path, "%s/%s/%s/replay_profiles/%s", GET_NS_TA_DIR(), usr_project[num_projs - 1], usr_sub_project[num_projs - 1], profile_name);
  
     LIB_DEBUG_LOG(1, debug_level, debug_file, "profile_file_path", profile_file_path);
    
  // Check if already in array or not
  for(i = 0; i < profile_idx; i++)
  {
    if(strcmp(usr_profile[i], profile_file_path)== 0)
    {
      LIB_DEBUG_LOG(1, debug_level, debug_file, "Profile %s is already added in array", profile_file_path);
      return 0;
    }
  }

  // Add in the array
  strcpy(usr_profile[profile_idx], profile_file_path);
  LIB_DEBUG_LOG(4, debug_level, debug_file, "usr_profile[%d] = %s", profile_idx, usr_profile[profile_idx]);
  profile_idx++;
  return 0;
}

//This method will extarct project/subproject from scenario name
int extract_proj_subproj_name(char *optarg, char **field, char *usr_scenario)
{
  int ret;
  ret=get_tokens1(optarg, field, "/");
  if(ret==5)
  {
    //<workspace>/<profile>/<proj>/<sub_pro>/<scenario>
    //set TA dir
    nslb_set_ta_dir_ex1(GET_WDIR(), field[0], field[1]);
    strcpy(usr_project[num_projs], field[2]);
    strcpy(usr_sub_project[num_projs], field[3]);
    strcpy(usr_scenario, field[4]);
  }
  else if(ret==3)
  {
    strcpy(usr_project[num_projs], field[0]); 
    strcpy(usr_sub_project[num_projs], field[1]);
    strcpy(usr_scenario, field[2]);
  }
  else 
  {
    return -1;
  }
  

  num_projs++;

  LIB_DEBUG_LOG(1, debug_level, debug_file, "ret = %d", ret);
  return ret;
}

// Args:
//   scen_name with <workspace>/<profile>/proj/sub-proj e.g. <workspace>/<profile>/default/default/my_scen.conf (.conf is optional)
static void get_profile_from_scenario(char *scen_name)
{
  char *field[10];
  char sce_file_path[MAX_STR_LENGTH + 1];
  DIR *prof_file;
  char lol_array[1024 + 1];
  char usr_scenario[255+1];
  char *tmp;

  strcpy(lol_array, scen_name);
  if(extract_proj_subproj_name(lol_array, field, usr_scenario) > 2)
  {
  if((tmp = strstr(scen_name, ".conf")) != NULL){
    *tmp = '\0';    
    sprintf(sce_file_path, "%s/%s/%s/scenarios/%s", GET_NS_TA_DIR(), usr_project[(num_projs - 1)], usr_sub_project[(num_projs - 1)], usr_scenario);
    *tmp = '.';
  }
  else
    sprintf(sce_file_path, "%s/%s/%s/scenarios/%s", GET_NS_TA_DIR(), usr_project[(num_projs - 1)], usr_sub_project[(num_projs - 1)], usr_scenario); 
  }
  // Check if specified file is present or not
  if ((prof_file= opendir(sce_file_path)) == NULL)
  {
    fprintf(stderr, "Error in opening dir %s . Error = %s\n", sce_file_path, strerror(errno));
    fprintf(stderr, "Scenario file is not correct. Scenario file is %s\n", sce_file_path);
    return;
  }

  LIB_DEBUG_LOG(1, debug_level, debug_file, "field[0] = %s, field[1] = %s",field[0], field[1]);
  add_profile_in_array(field[2], 0, 0);
  closedir(prof_file);
  return;
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

/*
 *Deprofileion : Comparison function for scandir
 *inputs : **a, **b - pointers to DIR structure.
 *outputs : 1 on succes ,0 on failure
 *error : none
 *algo : simple
*/
#if 0
#ifdef NS_KER_FC14
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
 *Deprofileion: To filter directories starting with TR in scandir.
 *inputs : *a - DIR structure.
 *outputs : 1 on succes ,0 on failure.
 *error : none
 *algo : simple
*/
int only_profile_dir(const struct dirent *a) 
{
    return 1;
}
//This method will get the no of profiles and add it in the array
static int get_tr_profile_list(int tr_num, char *tr_dir, char *proj, char *subproj, int tflag)
{
  char tr_dir_name[1024];

  //char file_name[2024];

  sprintf(tr_dir_name, "%s/TR%d/replay_data/profile", tr_dir, tr_num);

  DIR *dir_fp = opendir(tr_dir_name);

  if (dir_fp == NULL) {
    fprintf(stderr, "Unable to open dir (%s) for reading. TR%d do nor have any profile\n", strerror(errno), tr_num);
    exit(-1);
  }

  add_profile_in_array("profile", tflag, tr_num);
  closedir(dir_fp);
  return 0;
}

//This method will start the process of fetching project/subproject/profile from TR by initially reading the file from summary.top 
static void get_profile_from_tr(char *tr_num, int tflag)
{
  char *field[20];
  char buf[MAX_STR_LENGTH];
  char summary_top_path[MAX_STR_LENGTH];
  char proj_subproj[MAX_STR_LENGTH];
  char tr_prof_path[MAX_STR_LENGTH];
  int tr_num_value;
  DIR *dir;


  // Check if TR is present or not

  if(strstr(tr_num, "TR")){
    tr_num_value = atoi(tr_num + 2);
  }else
    tr_num_value = atoi(tr_num);

  if(nslib_is_valid_test_run(tr_num_value, tr_dir) != 1)
  {
    fprintf(stderr, "Unable to open test run %d for reading (%s).\n", tr_num_value, strerror(errno));
    exit(1);
  }

  if(strstr(optarg,"TR"))
    sprintf(tr_prof_path, "%s/%s/replay_data/profile", tr_dir, tr_num);
  else
    sprintf(tr_prof_path, "%s/TR%s/replay_data/profile", tr_dir, tr_num);

  if((dir = opendir(tr_prof_path)) == NULL)
  {
    fprintf(stderr, "%s tesrun do not have replay profile. tr_prof_path = %s\n", optarg, tr_prof_path);//if summary.top does not contain anything it will genrate error and exit
    // return;
    exit(-1);
  }

  if(strstr(optarg,"TR"))
    sprintf(summary_top_path, "%s/%s/summary.top", tr_dir, tr_num);
  else
    sprintf(summary_top_path, "%s/TR%s/summary.top", tr_dir, tr_num);

  if(nslib_read_summary_top(summary_top_path, field) == -1)
  {
    fprintf(stderr, "summary.top is empty\n");//if summary.top does not contain anything it will genrate error and exit
    exit(-1);
  }

  strcpy(proj_subproj, field[1]);
  //snprintf(proj_subproj, MAX_STR_LENGTH, "%s/%s/%s", GET_DEFAULT_WORKSPACE(), GET_DEFAULT_PROFILE(), field[1]);
  extract_proj_subproj_name(proj_subproj, field, buf);
  get_tr_profile_list(tr_num_value, tr_dir, field[2], field[3], tflag);
  closedir(dir);

}


//get profile owner if pw->pw_name is NULL set owner as UNKNOWN
static void get_profile_owner(uid_t user_id, char *own)
{
  //printf("get_profile_owner called\n");
  struct passwd *pw;
  pw = getpwuid(user_id);
  if(pw)
    strcpy(own, pw->pw_name);
  else
    strcpy(own, "UNKNOWN");
}

//get profile group, if gp->gr_name is NULL set owner as UNKNOWN
static void get_profile_group(gid_t group_id, char *grp)
{
  struct group *gp = getgrgid (group_id);
  if(gp)
    strcpy(grp, gp->gr_name);
  else
    strcpy(grp, "UNKNOWN");
}

//if running user & profile owner is same then use permisson of owner
//if above condition fails then chek groups(all groups) with groups of profile, if same use permisson of group
//els use permisson of others
static void get_profile_permission(uid_t scruid,  gid_t scr_gid, mode_t mode, char *perm)
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
static void get_profile_modification_time(time_t *modification_time, char *time_format)
{
  //printf("get_profile_modification_time called\n");
  struct tm *mod_time;
  mod_time = localtime(modification_time);
  //sprintf(time_format, "%d-%02d-%02d %02d:%02d:%02d", mod_time->tm_year + 1900, mod_time->tm_mon + 1, mod_time->tm_mday, mod_time->tm_hour, mod_time->tm_min, mod_time->tm_sec);
  //show date & time in format mm/dd/yy HH:MM:SS
  sprintf(time_format, "%02d/%02d/%d %02d:%02d:%02d", mod_time->tm_mon + 1,  mod_time->tm_mday, mod_time->tm_year + 1900, mod_time->tm_hour, mod_time->tm_min, mod_time->tm_sec);
}

static void get_profile_stats(char *path)
{
  //printf("get_profile_stats called path = %s\n", path);
  struct stat profile_det;
  char owner[MAX_STR_LENGTH] = "\0";
  char group[MAX_STR_LENGTH] = "\0";
  char permission[MAX_STR_LENGTH] = "\0";
  char modification_time[MAX_STR_LENGTH] = "\0";

  LIB_DEBUG_LOG(1, debug_level, debug_file, "path = %s", path);
  if(stat(path, &profile_det) != 0)
  {
     fprintf(stderr, "Error in getting stat of dir %s\n", path);
     return;
  }
  get_profile_owner(profile_det.st_uid, owner);
  get_profile_group(profile_det.st_gid, group);
  get_profile_permission(profile_det.st_uid, profile_det.st_gid, profile_det.st_mode, permission);
  get_profile_modification_time(&profile_det.st_mtime, modification_time);
  printf("%s|%s|%s|%s\n", owner, group, permission, modification_time);
}

static int is_profile_to_show(char *project, char *sub_project, char *profile)
{
  int i;
   
  for(i = 0; i < num_projs; i++)
  {
    if(!strcasecmp(project, usr_project[i]) && (!strcasecmp(usr_sub_project[i], "All") || !strcasecmp(sub_project, usr_sub_project[i])))
        return 0; //show
  }
 
  return 1;
}

static void  show_profiles_using_file_list()
{
  FILE *profile_list;
  char profile_path[MAX_LINE_LENGTH] = "\0";
  char project[MAX_STR_LENGTH] = "\0";
  char sub_project[MAX_STR_LENGTH] = "\0";
  char profile_name[MAX_LINE_LENGTH] = "\0";
  char work_profile[MAX_LINE_LENGTH] = "\0";

  profile_list = fopen(profile_name_list_file, "r");
  if(profile_list == NULL)
  {
    fprintf(stderr, "Unable to open %s file.\n", profile_name_list_file);
    exit(-1);
  }
  while(fgets(profile_path, MAX_LINE_LENGTH, profile_list)) 
  {
    profile_path[strlen(profile_path) - 1]='\0';
    get_proj_sub_proj_name(profile_path, work_profile, project, sub_project, profile_name, 0);
    if(!all) 
    {
       if(is_profile_to_show(project, sub_project, profile_name))
       {
         continue;
       }
    }
    printf("%s|%s|%s|%s|", work_profile, project, sub_project, profile_name);
    get_profile_stats(profile_path);
  }
}
//This method will display the <workspace>/<workprofile>/project/subproject/profile
static void  show_profiles_using_array(int tflag)
{
  char project[MAX_STR_LENGTH] = "\0";
  char sub_project[MAX_STR_LENGTH] = "\0";
  char profile_name[MAX_LINE_LENGTH] = "\0";
  int idx;
  char work_profile[MAX_LINE_LENGTH] = "\0";
  

  LIB_DEBUG_LOG(1, debug_level, debug_file, "profile_idx  = %d", profile_idx);
  
  for(idx = 0; idx < profile_idx; idx++)
  {
    LIB_DEBUG_LOG(1, debug_level, debug_file, "usr_profile[%d] = %s", idx, usr_profile[idx]); 
    get_proj_sub_proj_name(usr_profile[idx], work_profile, project, sub_project, profile_name, tflag);
    printf("%s|%s|%s|%s|", work_profile, project, sub_project, profile_name);
    get_profile_stats(usr_profile[idx]);
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
  int tflag = 0;
  int sflag = 0;
  int wflag = 0;
  char s_name[1024];
  init();
  while ((option = getopt(argc, argv, "Au:w:p:s:t:D:")) != -1)
  {
    switch (option)
    {
      case 'A': // All profiles
        if(pflag || uflag || aflag || sflag || tflag || wflag)
          usage("-A cannot be used with options or cannot be given more than once");

        all = 1;
        aflag++;
        break;

      case 'u': // profiles of all project/subproject assigned for the user
        if(pflag || uflag || aflag || sflag || tflag)
          usage("-u cannot be used with options or cannot be given more than once");
          
        all = 0; // Why?
        strcpy(user, optarg);
        //validate_user(user);
        //get_users_proj_sub_proj(user);
        uflag++;
        break;

      case 'p': // profiles of project/subproject passed
        if(uflag || aflag | sflag || tflag)
          usage("-p option cannot be used with other options");

	//if all is already set to 1 by previous -p All/All, then we need not to check nxt -p option 
	if(all && pflag)
	 break;

        if(get_tokens1(optarg, field, "/") != 2)
          usage("with -p the format should be project/subproject");
        strcpy(usr_project[pflag], field[0]);
        strcpy(usr_sub_project[pflag], field[1]);
        //if proj is all show all
	if(!strcmp(usr_project[pflag], "All"))
	  all = 1;
	else
          all = 0;
        pflag++;
        num_projs = pflag;
        break;

      case 's': // Profile used in scenario
        if(pflag || aflag || tflag || uflag)
          usage("-s cannot be used with options or cannot be given more than once");
        strcpy(s_name, optarg);
        sflag++;
        break;

      case 't':
        if(pflag || uflag || aflag || sflag || wflag)
          usage("-t cannot be used with options or cannot be given more than once");
        get_profile_from_tr(optarg, ++tflag);
        
        break;

      case 'D':
        debug_level = atoi(optarg);
        break;
      case 'w':
        NSLB_SET_W_OPTION(optarg);
        wflag++;
        break;
      case ':':
      case '?':
        usage(0);
    }
  }


  printf("%s|Project|Subproject|Profile|Owner|Group|Permission|Modification Date\n", WORK_PROFILE); 
 
  get_usr_uid_gid();
  nslb_check_and_set_ta_dir( aflag, wflag, pflag,  NSLB_GET_W_OPTION() );
  if(uflag)
  {
    get_users_proj_sub_proj(user); 
  }
  if(sflag)
  {
    get_profile_from_scenario(s_name);
  }
  if(sflag || tflag)
  {
    show_profiles_using_array(tflag);
  }
  else
  {
    get_all_profile_names();
    show_profiles_using_file_list();
  }

  remove(profile_name_list_file);
  return 0;
}
