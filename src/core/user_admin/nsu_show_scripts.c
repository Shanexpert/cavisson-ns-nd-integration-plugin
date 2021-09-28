/*----------------------------------------------------------------------
   Name    : nsu_show_scripts.c
   Author  : ArunN
   Purpose : This file is to show scripts according project/sub-project details.
   Usage   : nsu_show_scripts [-A | -u <User Name> | -p <proj1/sub_proj1> -p <proj2/sub_proj2> . . .]
   Format of Output:
             Project|Subproject|Script|Owner|Group|Permission|Modification Date
             Where Modification Date will be in "YYYY-MM-DD HH:MM:SS" (24 hour format)
   To Compile: gcc -o -g nsu_show_scripts nsu_show_scripts.c
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
#include <libgen.h>

#include "../../libnscore/nslb_log.h" 
#include "../../libnscore/nslb_util.h" 

#define MAX_LINE_LENGTH 2048
#define MAX_STR_LENGTH 256

static char script_name_list_file[MAX_STR_LENGTH];
static char script_error[MAX_STR_LENGTH];

static int all = 1;
static int jflag = 0;
static char script_dir[MAX_LINE_LENGTH];
static char scen_dir[MAX_LINE_LENGTH];
static char tr_dir[MAX_LINE_LENGTH];
static int num_projs = 0;

static short min_depth;
static short max_depth;
#define SET_MIN_DEPTH(depth) \
{\
  min_depth=depth;\
}

#define SET_MAX_DEPTH(depth) \
{\
  max_depth=depth;\
}

#define GET_MIN_DEPTH() min_depth
#define GET_MAX_DEPTH() max_depth
static char usr_project[1000][255 + 1];
static char usr_sub_project[1000][255 + 1];

// These two variables are used to store scripts from scenario file or test run
// It also uses usr_project and usr_sub_project array to keep the project/sub project name of scen or TR
static int script_idx = 0; // index in usr_script while parsing input and filing in usr_script
static char usr_script[1000][1000 + 1];


static gid_t usr_primary_grp_id;
static gid_t usr_secondry_grp_id;
static uid_t usr_id;

static int debug_level = 0;     // Debug log level
static FILE *debug_file = NULL; // Debug log file

int extract_proj_subproj_name(char *optarg, char **field, char *usr_scenario);

int script_list_flag = 0;         //0 : by Default , 1: When script.list is used for project/sub-porject
/*bug   : ns_ta_dir*/
static char g_wdir[1024];
#define SET_WDIR(ptr) strcpy(g_wdir, ptr);
#define GET_WDIR() g_wdir

static void usage(char *err_msg)
{

  fprintf(stderr, "%s\n", err_msg);
  fprintf(stderr, "Usage: \n");
  fprintf(stderr, "     : nsu_show_scripts [-A | -u <User Name> | -p <workspace>/<profile>/<proj1/sub_proj1> -p <workspace>/<profile>/<proj2/sub_proj2> | -s <workspace>/<profile>/<proj1/subproj1/scenario name -s <workspace>/<profile>/<proj2/subproj2/scenario| -t <TRNumber> -t <TRNumber1> -j <JMeter Script>. . .] \n");
  fprintf(stderr, "Where: \n");
  fprintf(stderr, "     : -A is used to show all scenario, project/sub-project details that present in default profile.\n");
  fprintf(stderr, "     : -u is used to show scripts specified User Name.\n");
  fprintf(stderr, "     : -p is used to show scripts of specified proj/sub_proj.\n");
  fprintf(stderr, "     : -s is used to show scripts used in specified scenario.\n");
  fprintf(stderr, "     : -t is used to show scripts used in specified test run.\n");
  fprintf(stderr, "     : -j is used to show all jmeter scripts.\n");
  fprintf(stderr, "     : -w is used to show all workspace name releated scripts\n");
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
  sprintf(script_dir, "%s", work_dir);
  sprintf(scen_dir, "%s", work_dir);
  sprintf(tr_dir, "%s/logs", work_dir);

  sprintf(script_name_list_file, "/tmp/script_list.%d", getpid());
  nslb_set_ta_dir_ex1(GET_WDIR(), GET_DEFAULT_WORKSPACE(), GET_DEFAULT_PROFILE());
  LIB_DEBUG_LOG(1, debug_level, debug_file, "script_dir = %s", script_dir);

}

static void open_debug_log_file(char *user_name)
{
char debug_file_path[4096];

  // Do not open if debug is off
  if(debug_level <= 0) return;

  // Create unique file for each user so that we do have permission issues
  sprintf(debug_file_path, "/tmp/%s_nsu_show_scripts_debug", user_name);

  if(!(debug_file = fopen(debug_file_path, "a")))
  {
    fprintf(stderr, "nsu_show_scripts: Error in opening debug file (%s). Error = %s\n", debug_file_path, strerror(errno));
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

  sprintf(cmd, "nsu_check_user %s nsu_show_scripts", usr);
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
  snprintf(cmd_name, 128, "nsu_show_projects -u %s | grep -v \"^User Name\"", user);
   
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

//make list of scripts
static void get_all_script_names(char *path)
{
  char cmd[MAX_LINE_LENGTH * 2]="\0";
  struct stat s;
  
  //sprintf(cmd, "find %s -mindepth 4 -maxdepth 4 -name \"script.capture\" | awk -F\"script.capture\" \'{print $1}\' | grep -v ^.*/[.] | sort -t '/' +4 -5 -k 5,6 -k 6,7 >%s", script_dir, script_name_list_file);
  // Now we will not search for script.capture we will just show the script name as new script design dont have any script.capture

  // Run command to get list of all scripts in ascending order of project/subproject 
  // and within same subproject, ascending order of script name
  // Also remove any hidden project or script names (starting with .)
  // find /home/netstorm/work/scripts/ -mindepth 3 -maxdepth 3 -type d | grep -v ^.*/[.] | sort -t '/' +4 -5 -k 5,6 -k 6,7
  
  sprintf(script_error,"/tmp/script_error.txt.%d", getpid());

  if (!jflag)
  {
    // We can have multiple files with runlogic.c(or .java) due to which nsu_show_scripts show multiple scripts. Specific check has been added.
    //find /home/cavisson/MultiUserWorkSpace/workspace/admin/system/cavisson -mindepth 5 --type d -name 'runlogic' | grep -v ^.*/[.] |sort -t '/' -u -k 9,9 -k 10,10 -k 12,12
    sprintf(cmd, "find %s -mindepth %d -maxdepth %d -type f -name 'runlogic.c' -o -name 'runlogic.java'  -o -type d -name 'runlogic' 2>>%s| grep -v ^.*/[.] |sort -t '/' -u -k 9,9 -k 10,10 -k 12,12 1>%s 2>>%s", path, GET_MIN_DEPTH(), GET_MAX_DEPTH(), script_error, script_name_list_file, script_error);
  }
  else
  {
    sprintf(cmd, "find %s -mindepth %d -maxdepth %d -type f -name '*.jmx' 2>>%s| grep -v ^.*/[.] | sort -u -t '/' +10 -8 -k 8,9 -k 10,11 -k 11,12 1>%s 2>>%s", path, GET_MIN_DEPTH(), GET_MAX_DEPTH(), script_error, script_name_list_file, script_error);
  }

  if(system(cmd) == -1)
  {
     fprintf(stderr, "Error: Can not find script names.\n");
     exit(-1);
  }
  
  stat(script_error, &s);
  if(!s.st_size)
  {
    if(remove(script_error) == -1)
    {
      fprintf(stderr, "Failed to remove file %s, error:%s\n", script_error, strerror(errno));
    }  
  }
}

//extract proj, subproj from script path
// path is full path like /home/netstorm/work/script/proj/subproj/name

static void get_profile_proj_sub_proj_name(char *path, char* profile, char *project, char *sub_project, char *name, int tflag, int script_list_flag)
{
  char *fields[30];
  char buf[MAX_LINE_LENGTH]="\0";
  
  LIB_DEBUG_LOG(1, debug_level, debug_file,"Method called, path = %s", path);
  strcpy(buf, path);
  int num_fileds = get_tokens1(buf, fields, "/");
  
  if(num_fileds < 6)
    return;
  
  if(tflag && !script_list_flag)
  {
    strcpy(project, usr_project[num_projs - 1]);
    strcpy(sub_project, usr_sub_project[num_projs - 1]);
    strcpy(name, fields[6]);
  }
  else if(tflag && script_list_flag) {
    ///home/cavisson/work/logs/TR5327/scripts/default/default/scripts/hpd_tours_new
    strcpy(profile, "NA");
    strcpy(project, fields[6]);
    strcpy(sub_project, fields[7]);
    if(num_fileds > 9)
      strcpy(name, fields[9]);
    else
      strcpy(name, fields[8]);
  }
  else{
    ///home/cavisson/work/workspace/admin/default//cavisson/pro1/subproj1/scripts/script_dir1/runlogic.c
    strcpy(profile, fields[5]);
    strcpy(project, fields[7]);
    strcpy(sub_project, fields[8]);
    strcpy(name, fields[10]);
  }
}

//This method will add project/subproject/script in the array
static int add_script_in_array(char *script_name, int tflag, int tr_num)
{
  char script_file_path[MAX_STR_LENGTH];
  int i;
  char script_name_with_proj[MAX_LINE_LENGTH]; 

  if(!tflag){
    if(!strchr(script_name, '/')) 
    {
      sprintf(script_name_with_proj, "%s/%s/%s/%s", usr_project[num_projs - 1], usr_sub_project[num_projs - 1], "scripts", script_name);
    }
    else
      strcpy(script_name_with_proj, script_name);
  } 
  // Make absolute path
  
    LIB_DEBUG_LOG(1, debug_level, debug_file,"script_dir= %s/usr_project[%d] = %s/usr_sub_project[num_projs - 1] = %s/script_name = %s", script_dir, num_projs - 1, usr_project[num_projs - 1], usr_sub_project[num_projs - 1], script_name);

     if(tflag)
       sprintf(script_file_path, "%s/TR%d/scripts/%s", tr_dir, tr_num, script_name);
     else
     {
       char* script_name1 = basename(script_name_with_proj);
       char* proj_subproj_name = dirname(script_name_with_proj);
       sprintf(script_file_path, "%s/%s/scripts/%s", GET_NS_TA_DIR(), proj_subproj_name, script_name1);
     }

     LIB_DEBUG_LOG(1, debug_level, debug_file, "script_file_path", script_file_path);

  // Check if already in array or not
  for(i = 0; i < script_idx; i++)
  {
    if(strcmp(usr_script[i], script_file_path)== 0)
    {
      LIB_DEBUG_LOG(1, debug_level, debug_file, "Script %s is already added in array", script_file_path);
      return 0;
    }
  }

  // Add in the array
  strcpy(usr_script[script_idx], script_file_path);
  LIB_DEBUG_LOG(4, debug_level, debug_file, "usr_script[%d] = %s", script_idx, usr_script[script_idx]);
  script_idx++;
  return 0;
}

// Args:
//   scen_name with proj/sub-proj e.g. default/default/my_scen.conf (.conf is optional)
static void get_script_from_scenario(char *scen_name)
{
  char *field[10];
  char buf[MAX_STR_LENGTH + 1]="\0";
  char sce_file_path[MAX_STR_LENGTH + 1];
  FILE *sce_file;
  char lol_array[1024 + 1];
  char usr_scenario[255+1];
  struct stat scen_dt;
  int i = 0, slash_count = 0;


  //Hanlding case of providing proj/sub-proj without scenario name
  for(i = 0; i< strlen(scen_name); i++){
    if(scen_name[i] == '/'){
      ++slash_count;
    } 
    if(slash_count == 4){            //When two slashes encountered
      if(scen_name[i + 1] == '\0'){  //If after second slash nothing is provided i.e., proj/sub-proj/
        slash_count = 1;             //ignoring second slash in case of no scenario_file name provided i.e., 'RBU_Chrome/RBU_Chrome/'
      }
      break;
    }    
  }
  
  //When two slashes are not provided
  if(slash_count != 4)
  {
    fprintf(stderr, "Please Provide correct Path in format <workspace>/<profile>/<project>/<sub-project>/<scenario_name>\n");
    exit(-1);   
  }
  strcpy(lol_array, scen_name); 
  if(extract_proj_subproj_name(lol_array, field, usr_scenario) > 2)
  {
    if(strstr(scen_name, ".conf"))
      sprintf(sce_file_path, "%s/%s/%s/%s/%s", GET_NS_TA_DIR(), usr_project[(num_projs - 1)], usr_sub_project[(num_projs - 1)], "scenarios", usr_scenario );
    else
      sprintf(sce_file_path, "%s/%s/%s/%s/%s.conf", GET_NS_TA_DIR(), usr_project[(num_projs - 1)], usr_sub_project[(num_projs - 1)], "scenarios", usr_scenario);
  }
  stat(sce_file_path, &scen_dt);

  if(S_ISREG(scen_dt.st_mode) != 0)
  {
    // Check if specified file is present or not
    if ((sce_file = fopen(sce_file_path, "r")) == NULL)
    {
      fprintf(stderr, "Error in opening file %s . Error = %s", sce_file_path, strerror(errno));
      fprintf(stderr, "Scenario file is not correct. Scenario file is %s ", sce_file_path);
      exit(-1);
    }

    while(fgets(buf, MAX_STR_LENGTH, sce_file))
    {
      LIB_DEBUG_LOG(1, debug_level, debug_file, "buf = %s",buf);
      if(!(strncmp(buf, "SGRP", 4)))
      {
        int num_fields = get_tokens1(buf, field, " ");
        //Changes for netomni as one extra field added in SGRP keyword 
        if(num_fields == 8)
        {
          // Check if URL is used in SGRP
          if(atoi(field[5]) == 0) // script, not URL
          {
            LIB_DEBUG_LOG(1, debug_level, debug_file, "field[5] = %s, field[6] = %s",field[5], field[6]);
            add_script_in_array(field[6], 0, 0);
          }
          else if (atoi(field[5]) == 100)
          {
            jflag = 1;
            LIB_DEBUG_LOG(1, debug_level, debug_file, "field[5] = %s, field[6] = %s",field[5], field[6]);
            add_script_in_array(field[6], 0, 0);
          }
        }
        //Old changes
        else if(num_fields == 7)
        {
          if(atoi(field[4]) == 0) // script, not URL
          {
            LIB_DEBUG_LOG(1, debug_level, debug_file, "field[4] = %s, field[5] = %s",field[4], field[5]);
            add_script_in_array(field[5], 0, 0);
          }
          else if (atoi(field[4]) == 100)
          {
            jflag = 1;
            LIB_DEBUG_LOG(1, debug_level, debug_file, "field[4] = %s, field[5] = %s",field[4], field[5]);
            add_script_in_array(field[5], 0, 0);
          }
        }
       
      }
    }
  }
  else
    fprintf(stderr, "Error in opening file %s . Error = %s", sce_file_path, strerror(errno));
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
 *Description : Comparison function for scandir
 *inputs : **a, **b - pointers to DIR structure.
 *outputs : 1 on succes ,0 on failure
 *error : none
 *algo : simple
*/
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

/*
 *Description: To filter directories starting with TR in scandir.
 *inputs : *a - DIR structure.
 *outputs : 1 on succes ,0 on failure.
 *error : none
 *algo : simple
*/
int only_script_dir(const struct dirent *a) 
{
    return 1;
}
//This method will get the no of scripts and add it in the array
static int get_tr_script_list(int tr_num, char *tr_dir, char *proj, char *subproj, int tflag)
{
  struct dirent **namelist;
  char tr_dir_name[1024];

  //char file_name[2024];

  sprintf(tr_dir_name, "%s/TR%d/scripts", tr_dir, tr_num);

  DIR *dir_fp = opendir(tr_dir_name);
  int n;

  if (dir_fp == NULL) {
     fprintf(stderr, "Unable to open dir (%s) for reading.\n", strerror(errno));
     exit(1);
  }

  n = scandir (tr_dir_name, &namelist, only_script_dir, my_alpha_sort);
  if (n < 0)
    perror ("scandir");
  else {
    while (n--) {
      if (namelist[n]->d_name[0] != '.' )
      {

        //sprintf(file_name, "%s/%s/scripts", tr_dir, namelist[n]->d_name);
        add_script_in_array(namelist[n]->d_name, tflag, tr_num);
      }
      free(namelist[n]);
    }
    free(namelist);
  }
  closedir(dir_fp);
  return 0;
}

//Extract project sub-project from scripts.list file
static void get_tr_script_list_ext(char *script_list_file, int tr_num, char *tr_dir, int tflag)
{
  FILE *fd;

  char file_buf[2024];
  char *end_ptr = NULL;

  //Need to code here for scripts.list
  //if fopen for scripts.list is successfull.
  //read name of proj and subproj from file and add script name in array
  //else (for old design), do as previous

  if((fd = fopen(script_list_file, "r"))!= NULL){
    while(fgets(file_buf, 2048, fd) != NULL){
      if((end_ptr = strrchr(file_buf, '\n')) != NULL) //Handle Linux format to terminate new line
      {
        *end_ptr = 0;
        if((end_ptr = strrchr(file_buf, '\r')) != NULL) //Handle DOS format to terminate new line
          *end_ptr = 0;
      }
      add_script_in_array(file_buf, tflag , tr_num);
      script_list_flag = 1;
    }
    if(fclose(fd) != 0) {
      fprintf(stderr, "Unable to close scripts.list with Error - [%s]\n", strerror(errno));
      exit(-1);
    }   
  }
  else 
  {
      fprintf(stderr, "Unable to open scripts.list with Error - [%s]\n", strerror(errno));
      exit(-1);
  }
}

//This method will start the process of fetching project/subproject/script from TR by initially reading the file from summary.top 
static void get_script_from_tr(char *tr_num, int tflag)
{
  char *field[20];
  char buf[MAX_STR_LENGTH];
  char proj_subproj[MAX_STR_LENGTH];
  char summary_top_path[MAX_STR_LENGTH];
  char script_list_file[2024];
  int tr_num_value;
  struct stat scrpt_list;

  // Check if TR is present or not

  if(strstr(tr_num, "TR")){
    tr_num_value = atoi(tr_num + 2);
  }else
    tr_num_value = atoi(tr_num);

  if(nslib_is_valid_test_run(tr_num_value, tr_dir) != 1)
  {
    fprintf(stderr, "Unable to open test run %d for reading. %s\n", tr_num_value, strerror(errno));
    exit(1);
  }

  sprintf(script_list_file, "%s/TR%d/scripts/scripts.list", tr_dir, tr_num_value);

  stat(script_list_file, &scrpt_list);

  if(S_ISREG(scrpt_list.st_mode) != 0)
  {
    get_tr_script_list_ext(script_list_file, tr_num_value, tr_dir, tflag);
  }
  else
  {
    if(strstr(optarg,"TR"))
      sprintf(summary_top_path, "%s/%s/summary.top", tr_dir, tr_num);
    else
      sprintf(summary_top_path, "%s/TR%s/summary.top", tr_dir, tr_num);

    if(nslib_read_summary_top(summary_top_path, field) == -1)
    {
      fprintf(stderr, "summary.top is empty\n");//if summary.top does not contain anything it will genrate error and exit
      exit(-1);
    }

    //strcpy(proj_subproj, field[1]);
    snprintf(proj_subproj, MAX_STR_LENGTH, "%s/%s/%s", GET_DEFAULT_WORKSPACE(), GET_DEFAULT_PROFILE(), field[1]);
    extract_proj_subproj_name(proj_subproj, field, buf);
    get_tr_script_list(tr_num_value, tr_dir, field[2], field[3], tflag);
  }

}


//get script owner if pw->pw_name is NULL set owner as UNKNOWN
static void get_script_owner(uid_t user_id, char *own)
{
  //printf("get_script_owner called\n");
  struct passwd *pw;
  pw = getpwuid(user_id);
  if(pw)
    strcpy(own, pw->pw_name);
  else
    strcpy(own, "UNKNOWN");
}

//get script group, if gp->gr_name is NULL set owner as UNKNOWN
static void get_script_group(gid_t group_id, char *grp)
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
static void get_script_permission(uid_t scruid,  gid_t scr_gid, mode_t mode, char *perm)
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
static void get_script_modification_time(time_t *modification_time, char *time_format)
{
  //printf("get_script_modification_time called\n");
  struct tm *mod_time;
  mod_time = localtime(modification_time);
  //sprintf(time_format, "%d-%02d-%02d %02d:%02d:%02d", mod_time->tm_year + 1900, mod_time->tm_mon + 1, mod_time->tm_mday, mod_time->tm_hour, mod_time->tm_min, mod_time->tm_sec);
  //show date & time in format mm/dd/yy HH:MM:SS
  sprintf(time_format, "%02d/%02d/%d %02d:%02d:%02d", mod_time->tm_mon + 1,  mod_time->tm_mday, mod_time->tm_year + 1900, mod_time->tm_hour, mod_time->tm_min, mod_time->tm_sec);
}

static void get_script_stats(char *path)
{
  //printf("get_script_stats called path = %s\n", path);
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
  get_script_owner(script_det.st_uid, owner);
  get_script_group(script_det.st_gid, group);
  get_script_permission(script_det.st_uid, script_det.st_gid, script_det.st_mode, permission);
  get_script_modification_time(&script_det.st_mtime, modification_time);
  printf("%s|%s|%s|%s\n", owner, group, permission, modification_time);
}

static int is_script_to_show(char *project, char *sub_project, char *script)
{
  int i;
   
  for(i = 0; i < num_projs; i++)
  {
    if(!strcmp(project, usr_project[i]) && (!strcasecmp(usr_sub_project[i], "All") || !strcmp(sub_project, usr_sub_project[i])))
        return 0; //show
  }
 
  return 1;
}

static void  show_scripts_using_file_list()
{
  FILE *script_list;
  char script_path[MAX_LINE_LENGTH] = "\0";
  char project[MAX_STR_LENGTH] = "\0";
  char sub_project[MAX_STR_LENGTH] = "\0";
  char script_name[MAX_LINE_LENGTH] = "\0";
  char profile_name[MAX_LINE_LENGTH] = "\0";
  script_list = fopen(script_name_list_file, "r");
  if(script_list == NULL)
  {
    fprintf(stderr, "Unable to open %s file.\n", script_name_list_file);
    exit(-1);
  }
  while(fgets(script_path, MAX_LINE_LENGTH, script_list)) 
  {
    //printf("\nscript_path = %s\n", script_path);
    script_path[strlen(script_path) - 1]='\0';
    get_profile_proj_sub_proj_name(script_path, profile_name, project, sub_project, script_name, 0, script_list_flag);
    if(!all) 
    {
       if(is_script_to_show(project, sub_project, script_name))
       {
         continue;
       }
    }
    //if(((!strcmp(profile_name, "system")) || (!strcmp(profile_name, "repo")))) {
     // strcpy(profile_name, "default");}
    
    printf("%s|%s|%s|%s|", profile_name, project, sub_project, script_name);
    get_script_stats(script_path);
  }
}
//This method will display the project/subproject/script
static void  show_scripts_using_array(int tflag)
{
  char project[MAX_STR_LENGTH] = "\0";
  char sub_project[MAX_STR_LENGTH] = "\0";
  char script_name[MAX_LINE_LENGTH] = "\0";
  char profile_name[MAX_LINE_LENGTH] = "\0";
  int idx;
  

  LIB_DEBUG_LOG(1, debug_level, debug_file, "script_idx  = %d", script_idx);
  
  for(idx = 0; idx < script_idx; idx++)
  {
    LIB_DEBUG_LOG(1, debug_level, debug_file, "usr_script[%d] = %s", idx, usr_script[idx]); 
    get_profile_proj_sub_proj_name(usr_script[idx], profile_name, project, sub_project, script_name, tflag, script_list_flag);
   // get_profile_proj_sub_proj_name(usr_script[idx], project, sub_project, script_name);
    printf("%s|%s|%s|%s|", profile_name, project, sub_project, script_name);
    get_script_stats(usr_script[idx]);
  }
}
//This method will extarct project/subproject from scenario name
int extract_proj_subproj_name(char *optarg, char **field, char *usr_scenario)
{
int ret;

  ret = get_tokens1(optarg, field, "/");
  nslb_set_ta_dir_ex1(GET_WDIR(), field[0], field[1]);
  strcpy(usr_project[num_projs], field[2]);
  strcpy(usr_sub_project[num_projs], field[3]);
  strcpy(usr_scenario, field[4]);
  num_projs++;

  LIB_DEBUG_LOG(1, debug_level, debug_file, "ret = %d", ret);
  return ret;
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
  char path[MAX_LINE_LENGTH] = "";
  init();

  if(argc < 2) {
    usage("Error: provide at least one argument according to following condition");
  }
  
  while ((option = getopt(argc, argv, "Au:p:w:s:t:D:j")) != -1)
  {
    switch (option)
    {
      case 'A': // All scripts
        if(pflag || uflag || aflag || sflag || tflag || jflag)
          usage("-A cannot be used with options or cannot be given more than once");

        //all = 1;
        aflag++;
        snprintf(path, MAX_LINE_LENGTH, "%s", GET_NS_TA_DIR());
        SET_MIN_DEPTH(5)
        SET_MAX_DEPTH(5)
        //nslb_set_ta_dir_ex1(GET_WDIR(), GET_DEFAULT_WORKSPACE(), GET_DEFAULT_PROFILE());
        break;

      case 'u': // scripts of all project/subproject assigned for the user
        if(pflag || uflag || aflag || sflag || tflag || jflag)
          usage("-u cannot be used with options or cannot be given more than once");
          
        all = 0; // Why?
        strcpy(user, optarg);
        validate_user(user);
        get_users_proj_sub_proj(user);
        uflag++;
        snprintf(path, MAX_LINE_LENGTH, "%s", GET_NS_TA_DIR());
        SET_MIN_DEPTH(5)
        SET_MAX_DEPTH(5)
        break;

      case 'p': // scripts of project/subproject passed
        if(uflag || aflag | sflag || tflag)
          usage("-p option cannot be used with other options");

	//if all is already set to 1 by previous -p All/All, then we need not to check nxt -p option 
	if(all && pflag)
	 break;

        if(get_tokens1(optarg, field, "/") != 4)
          usage("with -p the format should be <workspace>/<profile>/project/subproject");
        nslb_set_ta_dir_ex1(GET_WDIR(), field[0], field[1]);
        strcpy(usr_project[pflag], field[2]);
        strcpy(usr_sub_project[pflag]/*field[0]*/, field[3]);
	//if proj is all show all 
	if(!strcmp(usr_project[pflag], "All"))
	  all = 1;
	else
          all = 0;
        pflag++;
        num_projs = pflag;
        snprintf(path, MAX_LINE_LENGTH, "%s", GET_NS_TA_DIR());
        SET_MIN_DEPTH(5)
        SET_MAX_DEPTH(5)
        break;

      case 's': // Scripts used in scenario
        if(pflag || aflag || tflag || uflag || jflag)
          usage("-s cannot be used with options or cannot be given more than once");

        get_script_from_scenario(optarg);
        sflag++;
        break;

      case 't':
        if(pflag || uflag || aflag || sflag || jflag)
          usage("-t cannot be used with options or cannot be given more than once");
        get_script_from_tr(optarg, ++tflag);
        break;

      case 'D':
        debug_level = atoi(optarg);
        break;
     
      case 'j':
        if(uflag || aflag || sflag || tflag || jflag)
           usage("-jmeter cannot be used with options or cannot be given more than once");

        jflag++;
        break;
      case 'w': // All scripts
        if( wflag || pflag || uflag || aflag || sflag || tflag || jflag)
          usage("-w cannot be used with options or cannot be given more than once");
        snprintf(path, MAX_LINE_LENGTH, "%s/workspace/%s/cavisson/", GET_WDIR(), optarg);
        SET_MIN_DEPTH(5)
        SET_MAX_DEPTH(5)
        break;
      case ':':
      case '?':
        usage(0);
    }
  }

  /*bug id 101320: WorkProfile added*/
  printf("%s|Project|Subproject|Script|Owner|Group|Permission|Modification Date\n", WORK_PROFILE); 
 
  get_usr_uid_gid();


  if(sflag || tflag)
  {
    show_scripts_using_array(tflag);
  }
  else
  {
    get_all_script_names(path);
    show_scripts_using_file_list();
  }

  remove(script_name_list_file);
  return 0;
}
