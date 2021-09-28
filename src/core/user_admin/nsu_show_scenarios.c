/*----------------------------------------------------------------------
   Name    : nsu_show_scenarios.c
   Author  : Archana
   Purpose : This file is to show scenario according project/sub-project details.
   Usage   : nsu_show_scenarios [-A | -u <User Name> | -p <project/subproject>][-s <days>][-k <search keyword>]
   Format of Output:
             Project|Subproject|Scenario|Owner|Group|Permission|Modification Date
             Where Modification Date will be in "YYYY-MM-DD HH:MM:SS" (24 hour format)
   To Compile: gcc -o -g nsu_show_scenarios nsu_show_scenarios.c
   Modification History:
             02/17/09:  Archana - Initial Version
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

// for get tokens
#include "nslb_util.h"

#define MAX_STR_LENGTH 256

static int all = 0;
static int num_projs = 0;
char scenario_dir[1024];
static char usr_project[1000][255 + 1]; //We need to make this dynamically
static char usr_sub_project[1000][255 + 1];

static int last_n_days = -1;
static char scenario_search_key[1024] = "\0";

static char *search_keys[100];
static int num_search_key = 0;

static int total_records = 0;
static int total_filtered_records = 0;
static int gflag = 0;
static int sflag = 0;

gid_t usr_primary_grp_id;
gid_t usr_secondry_grp_id;
uid_t usr_id;

static void validate_user(char *user_name)
{
  char cmd[MAX_STR_LENGTH]="\0";

  sprintf(cmd, "nsu_check_user %s nsu_show_scenarios", user_name);
  if(system(cmd) != 0)
  {
    //fprintf(stderr, "Not a valid user : %s\n", usr);
    exit (-1);
  }
  if(!all)
  {
    if(!strcmp(user_name, "admin") || !strcmp(user_name, "cavisson"))
    all = 1;
  }
}

// This method to give error Usage message and exit
void display_help_and_exit()
{
  printf("Usage: nsu_show_scenarios [-A | -u <User Name>] [-p <project/subproject>] [-w <workspace_name>/<profile>] [-s <days>][-k <search keyword>]\n");
  printf("Where: \n");
  printf("  -A is used to show all scenario, project/sub-project details that present in default profile\n");
  printf("  -u is used to show scenario, project/sub-project details of specified User Name\n");
  printf("  -p is used to show scenarios of given <project>/<sub-project> of specified workspace/profile with -w  option\n");
  printf("  -w is used to show scenarios of given <workspace>/<profile>\n");
  printf("  -s is used to show scenarios of last s days modified\n");
  printf("  -k is used to show scenarios which match with given keyword\n");
  printf("   At a perticular time one of the option from -A,-u, -p, -w or -w AND -p should be used \n");
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
    get_tokens(buff, temp, "|", 10);
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

#if 0
//extract proj, subproj from scenario path
static void get_proj_sub_proj_name_from_scenario(char *path, char *project, char *sub_project, char *scenario_name)
{
  char *fields[30];
  char buf[MAX_STR_LENGTH]="\0";
  char conf_file[MAX_STR_LENGTH]="\0";
  char* fptr;

  strcpy(buf, path);
  
  get_tokens(buf, fields, "/", 10);
  strcpy(project, fields[4]);
  strcpy(sub_project, fields[5]);
  strcpy(conf_file, fields[6]);
  fptr = strrchr(conf_file, '.');
  if (!fptr || strcmp(fptr, ".conf")) 
  {
    fprintf(stderr, "Incorrect format of the conf file. must be <ScenarioName>.conf\n");
    exit(-1);
  }
  *fptr = '\0';
  if (strchr(conf_file, ',')) 
  {
     fprintf(stderr, "Scenario name can't have a comma in it\n");
     exit(-1);
  }
  strcpy(scenario_name, conf_file);
}
#endif

//get scenario owner if pw->pw_name is NULL set owner as UNKNOWN
static void get_scenario_owner(uid_t user_id, char *own)
{
  struct passwd *pw;
  pw = getpwuid(user_id);
  if(pw)
    strcpy(own, pw->pw_name);
  else
    strcpy(own, "UNKNOWN");
}

//get scenario group, if gp->gr_name is NULL set owner as UNKNOWN
static void get_scenario_group(gid_t group_id, char *grp)
{
  struct group *gp = getgrgid (group_id);
  if(gp)
    strcpy(grp, gp->gr_name);
  else
    strcpy(grp, "UNKNOWN");
}

//if running user & scenario owner is same then use permisson of owner
//if above condition fails then chek groups if same use permisson of group
//els use permisson of others
static void get_scenario_permission(uid_t scruid,  gid_t scr_gid, mode_t mode, char *perm)
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
static void get_scenario_modification_time(time_t *modification_time, char *time_format)
{
  struct tm *mod_time;
  mod_time = localtime(modification_time);
  //sprintf(time_format, "%d-%02d-%02d %02d:%02d:%02d", mod_time->tm_year + 1900, mod_time->tm_mon + 1, mod_time->tm_mday, mod_time->tm_hour, mod_time->tm_min, mod_time->tm_sec);
  //shows day in mm/dd/yyyy HH:MM:SS 
  sprintf(time_format, "%02d/%02d/%d %02d:%02d:%02d", mod_time->tm_mon + 1, mod_time->tm_mday, mod_time->tm_year + 1900, mod_time->tm_hour, mod_time->tm_min, mod_time->tm_sec);
}

static void get_scenario_stats(char *path)
{
  struct stat scenario_det;
  char owner[MAX_STR_LENGTH] = "\0";
  char group[MAX_STR_LENGTH] = "\0";
  char permission[MAX_STR_LENGTH] = "\0";
  char modification_time[MAX_STR_LENGTH] = "\0";

  if(stat(path, &scenario_det) != 0)
  {
     fprintf(stderr, "Error in getting stat of dir %s\n", path);
     return;
  }
  get_scenario_owner(scenario_det.st_uid, owner);
  get_scenario_group(scenario_det.st_gid, group);
  get_scenario_permission(scenario_det.st_uid, scenario_det.st_gid, scenario_det.st_mode, permission);
  get_scenario_modification_time(&scenario_det.st_mtime, modification_time);
  printf("%s|%s|%s|%s", owner, group, permission, modification_time);
  total_filtered_records++;
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
    printf("nsu_show_scenarios: Only one option can specify at a time.\n");
    display_help_and_exit();
  }
}

#if 0
static int convert_time_into_minutes(char *str)
 {
  int hh, mm, ss;
  int num, total_time;

  num = sscanf(str, "%d:%d:%d", &hh, &mm, &ss);
  if(num == 1){
    if(ss > 45)
      total_time=1;
  } else if(num == 2){
    total_time = mm; //if only one value given it is seconds
    if(ss > 45)
    total_time = total_time + 1;
  } else if(num == 3){
    total_time = (hh*60) + mm; //if HH::MM:SS given
    if(ss > 45)
    total_time = total_time + 1;
  } else {
      printf("time format incorrect");
      exit(-1);
  }
  return(total_time);
}
#endif
int get_total_scenarios(char *cmd)
{
  FILE *fp = NULL;
  char buf[2048 + 1];

  fp = popen(cmd, "r");
  if(fp == NULL) {
    perror("nsu_show_scenarios: popen"); //ERROR: popen failed
    exit(-1);
  } else {
    while(fgets(buf, 2048, fp)!= NULL ) {
      total_records++;
    }
  } 
  return 0; 
}

//creating shell command to fetch scenarios of given condition
static void create_scen_list_cmd(char *cmd) {
 int i;
 char create_path_buf[4096];
 char last_n_days_buf[32];
 static char search_key_buf[4096];
 static char buf[512];
 int search_depth;

 //for -w option, it is introduced
 int max_search_depth;
 cmd[0] = create_path_buf[0] = '\0';
 // /home/cavisson/work/workspace/<workspace_name>
 // /home/cavisson/work/workspace/<workspace_name>/<profile_name>
 ///home/cavisson/work/workspace/<workspace_name>/<profile_name>/cavisson/<proj>/<sub_proj>/scenarios
 // /home/cavisson/work/workspace/<workspace_name>/<profile_name>/cavisson/<proj>/<sub_proj>/scenarios/<scenario.conf> ==> depth =11
  //OR
 // /home/cavisson/work/workspace/<workspace_name>/<profile_name>/cavisson/<proj>/<sub_proj>/scenarios/<YML DIR>/yaml.conf --> depth = 12
 //
 if(all) {
   search_depth = 4;
   strcpy(create_path_buf, scenario_dir);
   max_search_depth = search_depth + 1;
 } else {
   search_depth = 1;
   if(num_projs) {
    max_search_depth = search_depth + 1;
    for(i=0; i < num_projs; i++)
       sprintf(create_path_buf,"%s/%s/%s/%s", scenario_dir, usr_project[i], usr_sub_project[i], "scenarios");
   }
   //in case only -w option provided, so num_projs would be zero
   else {
     strcpy(create_path_buf, scenario_dir);
     max_search_depth = 7;
   }
 }

 if(last_n_days > 0)
   sprintf(last_n_days_buf,"-mtime -%d", last_n_days);
 else
   last_n_days_buf[0] = '\0';

 if(scenario_search_key[0] == '\0')
   sprintf(search_key_buf, "-iname \"*.conf\" -o -iname \"*.yaml\" -o -iname \"*.wzrd\" -o -iname \"*.json\" -o -iname \"*.jmx\" | grep scenarios");
   else{
     strcat(search_key_buf,"\\( ");     
     for(i=0;i<num_search_key;i++){
       sprintf(buf, "-iname \"*%s*.conf\" -o -iname \"*%s*.yaml\" -o -iname \"*%s*.wzrd\"  -o -iname \"*%s*.json\" -o -iname \"*%s*.jmx\" -o ",
                    search_keys[i], search_keys[i], search_keys[i], search_keys[i]);
       strcat(search_key_buf,buf);
     }
     search_key_buf[strlen(search_key_buf)-3]='\0'; 
     strcat(search_key_buf,"\\)");
   }

 //printf("all = %d, last_n_days = %d, search_key_buf = %s, search_depth = %d", all, last_n_days, search_key_buf, search_depth);

 //Command has been changed, since we dont want to show hidden scenario files
 //Done to fix the bug 4931
 // Adding +1 in maxdepth to support .yaml file in directory
 //grep scenarios dir in order to avoid testsuites as it also conatins .conf file
 sprintf(cmd, "SCEN_LIST=`find -L %s -maxdepth %d -mindepth %d \\( ! -regex \'.*/\\..*\' \\) -type f %s %s | grep -w %s 2>/dev/null`;"
              "RET=$?;"
              "if [ \"X$SCEN_LIST\" != \"X\" ];then"
              "   ls -1t $SCEN_LIST;"
              "fi;" 
              "exit $RET",
              create_path_buf, max_search_depth, search_depth,
              last_n_days_buf, search_key_buf, "scenarios");

  char cmd1[4096];
  if(gflag)
  {
   sprintf(create_path_buf, ".");
   search_depth = 3;
   last_n_days_buf[0] = '\0';
   sprintf(search_key_buf, "-iname \"*.conf\"");
   sprintf(cmd1, "SCEN_LIST=`find -L %s -maxdepth %d -mindepth %d \\( ! -regex \'.*/\\..*\' \\) -type f  %s %s | grep -w %s 2>/dev/null`;"
              "RET=$?;"
              "if [ \"X$SCEN_LIST\" != \"X\" ];then"
              "   ls -1t $SCEN_LIST;"
              "fi;"
              "exit $RET",
              create_path_buf, search_depth + 1, search_depth,
              last_n_days_buf, search_key_buf, "scenarios");
   get_total_scenarios(cmd1);
  }

  //printf("cmd = %s\n", cmd);
}

static void print_scenarios(char *cmd) {
  FILE *fp;
  char scenario_name[2024];
  char *fields[13];
  char *fields1[3];
  char buf[2048 + 1]="\0";
  char scenario_path[2048 + 1]="\0";
  //char conf_file[MAX_STR_LENGTH]="\0";
  //char* fptr;
  int num_fields;
  //int num_fields1;
  int scen_len;
  char type[10];
 
  fp = popen(cmd, "r");
  if(fp == NULL) {
    perror("nsu_show_scenarios: popen"); //ERROR: popen failed
    exit(-1);
  } else {
      while(fgets(buf, 2048, fp)!= NULL ) {
        buf[strlen(buf) - 1] = '\0';  //// Replacing new line by null
        strcpy(scenario_path, buf);
        num_fields = get_tokens(buf, fields, "/",  13);
        // -w output ==> ./system/cavisson/default/default/scenarios/scenario_HTTP_Proxy.conf 
        //printf("num_fields = %d\n", num_fields);
        if( (num_fields != 11 ) && (num_fields != 12 ) )
          continue;

        // /home/cavisson/work/workspace/admin/system/cavisson/default/default/scenarios/scenario_HTTP_Proxy.conf ==> 11
        // OR
        ///home/cavisson/work/workspace/admin/system/cavisson/default/default/scenarios/hpd_tours/scenario.yaml ==>  12
        if(num_fields == 12)  //   ./SocketAPI/SocketAPI/scenarios/hpd_tours/scenario.yaml
        {
          strcpy(scenario_name, fields[num_fields - 1]);
          if(!strcmp(scenario_name, "scenario.yaml"))
            strcpy(type, "YAML");
          else if(!strcmp(scenario_name, "scenario.wzrd"))
            strcpy(type, "WZRD");
          else if (!strcmp(scenario_name, "scenario.json"))
            strcpy(type, "JSON");
          else if (!strcmp(scenario_name, "scenario.jmx"))
            strcpy(type, "JMX");
          else
            continue;
        }
         
        strcpy(scenario_name, fields[10]);
        scen_len = strlen(fields[10]);
        if(num_fields == 11) {
         get_tokens(scenario_name, fields1, ".", 3); // scenario.yaml
         if(strcmp((fields1[1]), "conf"))
           continue;  //Ignore test.yaml.conf

         strcpy(type, "CONF");
         scen_len -= 5; // Not including .conf
        }
        
        char profile_name[NS_PROFILE_SIZE];
        nslb_get_profile_name(fields[5], profile_name);

        printf("%s|%s|%s|%*.*s|", profile_name, fields[7], fields[8], scen_len, scen_len, fields[10]);
        get_scenario_stats(scenario_path);
        printf("|%s\n", type);
      }
  }

  if(gflag) {
    printf("Total Filtered Records = %d, Total Records = %d\n", total_filtered_records, total_records);
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
  int uflag = 0, Aflag = 0, kflag = 0, pflag = 0, wflag = 0;

  if(argc < 2) {
    fprintf(stderr," Error: provide at least one argument according to following condition\n");
    display_help_and_exit();
  }
  /*bug id: 101320: set ns_wdir and default workspace and profile*/
  nslb_init_wdir_wpdir();
  while ((option = getopt(argc, argv, "Agu:p:w:s:k:")) != -1)
  {
    switch (option)
    {
      case 'A':
        if (Aflag)
        {
          printf("nsu_show_scenarios: -A option cannot be specified more than once.\n");
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
          printf("nsu_show_scenarios: -u option cannot be specified more than once.\n");
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
         printf("nsu_show_scenarios: -s option cannot be specified more than once.\n");
         display_help_and_exit();
        }
        sflag++;
        if(ns_is_numeric(optarg) == 0) {
          fprintf(stderr, "Error: No of days can have only integer value\n");
          exit(-1);
        }
        last_n_days = atoi(optarg);
        break;

      case 'k':
        if(kflag)
        {
          printf("nsu_show_scenarios: -k option cannot be specified more than once.\n");
          display_help_and_exit();
        }
        kflag++;
        strcpy(scenario_search_key, optarg);
        num_search_key = get_tokens(scenario_search_key, search_keys, " ", 100);
    	
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
      case 'g':
        if(gflag)
        {
          printf("nsu_show_scenarios: -g option cannot be specified more than once.\n");
          display_help_and_exit();
        }
        gflag++;
        break;
      case ':':
      case '?':
        display_help_and_exit();
    }
  }
  //-s or -k option should not be given without -w, -u, -p or -A
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

  sprintf(scenario_dir, "%s", GET_NS_TA_DIR());
  if(chdir(scenario_dir)) {
    fprintf(stderr, "Error: nsu_show_scenarios: Unable to chdir on %s\n", scenario_dir); 
    exit(1);
  }

  printf("%s|Project|Subproject|Scenario|Owner|Group|Permission|Modification Date|Type\n", WORK_PROFILE);
  char cmd[4096];
  create_scen_list_cmd(cmd);
  print_scenarios(cmd);

  return 0;
}
