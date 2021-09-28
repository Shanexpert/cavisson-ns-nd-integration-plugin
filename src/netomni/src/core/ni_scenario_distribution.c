/********************************************************************************
 * File Name            : ni_scenario_distribution.c 
 * Author(s)            : Manpreet Kaur
 * Date                 : 19 March 2012
 * Copyright            : (c) Cavisson Systems
 * Purpose              : Contains parsing function for NS_GENERATOR_FILE, 
 *                        NS_GENERATOR keyword and other keywords,
 *                        add generator entries, distribute scenario among groups
 *                        wrt generator.
 * Modification History : <Author(s)>, <Date>, <Change Description/Location>
 ********************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <malloc.h>
#include <string.h>
#include <stdarg.h> 
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h> 
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <libgen.h>
#include <signal.h>
#include <pthread.h>

#include "nslb_sock.h"
#include "nslb_http_auth.h"
#include "../../../ns_master_agent.h"
#include "nslb_util.h"
#include "ni_user_distribution.h"
#include "ni_scenario_distribution.h"
#include "ni_schedule_phases_parse.h"
#include "ni_script_parse.h"
#include "../../../ns_exit.h"
#include "nslb_cav_conf.h"
#include "../../../ns_jmeter.h"
#include "../../../ns_test_init_stat.h"
#include "../../../ns_log.h"
#include "../../../ns_parent.h"
#include "../../../ns_kw_usage.h"
#include "../../../ns_error_msg.h"
#include "../../../ns_data_handler_thread.h"
#include "../../../ns_parse_netcloud_keyword.h"
#include "../../../ns_global_settings.h"
#include "../../../nslb_netcloud_util.h"

#define MAX_PHASE_NAME_LENGTH 512
int max_used_generator_entries; //Number of used generator
int total_gen_location_entries;  //Total number of used location
int max_gen_location_entries;
int g_gen_location_processed = 0;
int g_runtime_user_session_pct = 0;
int sgrp_used_genrator_entries; //Number of used generators per SGRP
int max_generator_for_rtc;
int num_users = 0; //NUM_USERS
int local_num_users = 0;//In case of FCU + PCT + NUM_USERS >0 then fill num_users.
int prof_pct_mode; //PROF_PCT_MODE
//FSR keyword
int mode;
int schedule_type;
int schedule_by;
int total_sessions = 0;
//Generator Entry struct
int total_generator_entries;
int max_generator_entries;
GeneratorEntry *generator_entry = NULL;
LocationTable *g_gen_location_table = NULL;
static int loc_user_or_session;  //User or session count calculated during location keyword  parsing 

//List of used generator(Currently values are not used)
static int total_used_generator_list_entries;
static int max_used_generator_list_entries;
GeneratorUsedList *gen_used_list = NULL;

//SGRP keyword parsing
int total_sgrp_entries;
static int max_sgrp_entries;
ScenGrpEntry* scen_grp_entry = NULL;
int total_used_sgrp_entries = 0;
//List of generator name
gen_name_quantity_list* gen_list = NULL;
//Global Vars:
static char scenario_file[FILE_PATH_SIZE];
static char scenario_settings_profile_file[FILE_PATH_SIZE];
static char new_conf_file[FILE_PATH_SIZE];
char work_dir[FILE_PATH_SIZE];
char controller_dir[FILE_PATH_SIZE];
static char scenario_file_name[FILE_PATH_SIZE];
static char internal_used_gen_file_name[FILE_PATH_SIZE]; //List of generators residing on internal enviornment
static char external_used_gen_file_name[FILE_PATH_SIZE]; //List of generators residing on cloud
char netomni_scenario_file[FILE_PATH_SIZE];
char netomni_proj_subproj_file[FILE_PATH_SIZE];
int test_run_num;
double vuser_rpm;  
int num_connections;
int ramp_up_all;
char controller_ns_build_ver[128];
int g_per_cvm_distribution;
typedef void (*sighandler_t)(int);
//Shell Inputs:
static char scen_file_for_shell[FILE_PATH_SIZE];
static char scen_proj_subproj_dir[FILE_PATH_SIZE];

//NETCLOUD_MODE
int netcloud_mode;
static char controller_type[9];

int continue_on_file_param_dis_err;

schedule *scenario_schedule_ptr = NULL;
schedule *group_schedule_ptr = NULL;
int ni_debug_level = 0;
FILE *debug_fp = NULL;

int scenario_settings_profile_file_flag = 0;
int scheduling_flag_enable;
int cluster_field_enable = 0;//CLUSTER_VARS enable
static int script_or_url;

//FCS variables
int enable_fcs_settings_mode;
int enable_fcs_settings_limit;
int enable_fcs_settings_queue_size;
perGenFCSTable *per_gen_fcs_table = NULL;
int progress_msecs;
int jmx_enabled_script = 0;

CheckGenHealth checkgenhealth;
gen_capacity_per_gen* gen_cap;
void check_generator_health();
char* get_version(char* component);
int read_and_process_gen_location();
extern int kw_set_enable_alert(char *buf, char *err_msg, int runtime_flag);
extern void reframe_alert_keyword(char *controller_dir, char *scenario_file_name, char *scen_proj_subproj_dir);
extern int kw_set_num_nvm(char *buf, Global_data *glob_set, int flag, char *err_msg);
#ifndef CAV_MAIN
extern char *script_name;
#else
extern __thread char *script_name;
#endif
void open_file_fd (FILE **fp)
{ 
  char file_name[512];
  char chk_controller_dir[FILE_PATH_SIZE];
   
  if (test_run_num != -1) {
    sprintf(file_name, "%s/logs/TR%d/NetCloud/distribution_tool.txt", work_dir, test_run_num);
  } else {
    sprintf(chk_controller_dir, "%s/.tmp/.controller", work_dir);
    if (mkdir(chk_controller_dir, 0775))
    {
      if(errno != EEXIST)
        NS_EXIT(-1, CAV_ERR_1000005, chk_controller_dir, errno, nslb_strerror(errno));
    }
    sprintf(file_name, "%s/.tmp/.controller/distribution_tool.txt", work_dir);
  }

  *fp = fopen(file_name, "a+"); 
  if(*fp == NULL)
  {
     NS_EXIT(-1, CAV_ERR_1000006,file_name, errno, nslb_strerror(errno));
  }
}

void ni_debug_logs(int log_level, char *filename, int line, char *fname, char *format, ...)
{
  char file[1024];
  char *ptr = NULL;
  va_list ap;
  char buffer[MAX_DEBUG_LOG_BUF_SIZE + 1] = "\0";
  char time_buf[100];
  int amt_written = 0, amt_written1=0;

  static int flag = 0;

  if (ni_debug_level < log_level) return;

  if(flag == 0) 
  {
    open_file_fd( &debug_fp);
    flag++;
  }

  ptr = strrchr(filename, '/'); 
  if(ptr != NULL)
    strcpy(file, ptr + 1);
  else
    strcpy(file, filename);

  amt_written1 = sprintf(buffer, "\n%s|%s|%d|%s|", nslb_get_cur_date_time(time_buf, 1), file, line, fname);

  va_start(ap, format);
  amt_written = vsnprintf(buffer + amt_written1 , MAX_DEBUG_LOG_BUF_SIZE - amt_written1, format, ap);

  va_end(ap);

  buffer[MAX_DEBUG_LOG_BUF_SIZE] = 0;

  if (amt_written < 0) {
    amt_written = strlen(buffer) - amt_written1;
  }

  if (amt_written > (MAX_DEBUG_LOG_BUF_SIZE - amt_written1)) {
    amt_written = (MAX_DEBUG_LOG_BUF_SIZE - amt_written1);
  }

  if(debug_fp != NULL)
    fprintf(debug_fp, "%s\n", buffer);
  else
    fprintf(stderr, "%s\n", buffer);
}
//Check whether Port is numeric or not
static int ni_is_numeric(char *str)
{
  int i;
  for(i = 0; i < strlen(str); i++) {
    if(!isdigit(str[i])) return 0;
  }
  return 1;
}

int get_tokens_(char *read_buf, char *fields[], char *token, int max_flds)
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
      //fprintf(stderr, "Total fields are more than max fields (%d), remaining fields are ignored\n", max_flds);
      totalFlds = max_flds;
      break;  /* break from while */
    }
    fields[totalFlds - 1] = token_ptr;
  }
  return(totalFlds);
}

/*********************************************************************************
 * Description          : Function used to malloc structures, and initialize
 *                        global variables.
 * Input-Parameters     : None
 * Output-Parameter     : None
 * Return               : None
 ********************************************************************************/

void init_generator_entry()
{
  NIDL (1, "Method called.");

  /*Initialize global vars*/
  total_generator_entries = 0;
  total_used_generator_list_entries = 0;
  total_sgrp_entries = 0;
  max_used_generator_entries = 0;
  sgrp_used_genrator_entries = 0;
  max_generator_for_rtc = 0; 
  /*Added global var for START phase, set flag if phase applied with "AFTER" option*/
  num_connections = 0;
  vuser_rpm = 0;
  ramp_up_all = 0; 
  /*Malloc generator entry*/
  //generator_entry = (GeneratorEntry *)malloc(INIT_GENERATOR_ENTRIES * sizeof(GeneratorEntry));
  //memset(generator_entry, 0, (INIT_GENERATOR_ENTRIES * sizeof(GeneratorEntry)));
  NSLB_MALLOC_AND_MEMSET(generator_entry, (INIT_GENERATOR_ENTRIES * sizeof(GeneratorEntry)), "generator table", -1, NULL);
  max_generator_entries = INIT_GENERATOR_ENTRIES;

  /*Malloc generator used entries*/
  //gen_used_list = (GeneratorUsedList *)malloc(INIT_USED_GENERATOR_ENTRIES * sizeof(GeneratorUsedList));
  NSLB_MALLOC(gen_used_list, (INIT_USED_GENERATOR_ENTRIES * sizeof(GeneratorUsedList)), "used generator list", -1, NULL);
  max_used_generator_list_entries = INIT_USED_GENERATOR_ENTRIES;

  /*Malloc generator used entries*/
  //scen_grp_entry = (ScenGrpEntry*)malloc(INIT_SGRP_ENTRIES * sizeof(ScenGrpEntry));
  NSLB_MALLOC(scen_grp_entry, (INIT_SGRP_ENTRIES * sizeof(ScenGrpEntry)), "scenario group table", -1, NULL);
  max_sgrp_entries = INIT_SGRP_ENTRIES;

  /*Malloc script table*/
  //script_table = (ScriptTable*)malloc(INIT_SCRIPT_ENTRIES * sizeof(ScriptTable));
  NSLB_MALLOC_AND_MEMSET(script_table, (INIT_SCRIPT_ENTRIES * sizeof(ScriptTable)), "script table", -1, NULL);
  max_script_entries = INIT_SCRIPT_ENTRIES;

  /*Malloc API table*/
  //api_table = (APITableEntry*)malloc(INIT_API_ENTRIES * sizeof(APITableEntry));
  NSLB_MALLOC_AND_MEMSET(api_table, (INIT_API_ENTRIES * sizeof(APITableEntry)), "api table", -1, NULL);
  max_api_entries = INIT_API_ENTRIES;

  /*Malloc Unique var table*/
  //total_unique_range_entries = 0;
  //uniquerangeTable = (UniqueRangeTableEntry*)malloc(INIT_UNIQUE_RANGE_ENTRIES * sizeof(UniqueRangeTableEntry));
  NSLB_MALLOC_AND_MEMSET(uniquerangeTable, (INIT_UNIQUE_RANGE_ENTRIES * sizeof(UniqueRangeTableEntry)), "unique var table", -1, NULL);
  max_unique_range_entries = INIT_UNIQUE_RANGE_ENTRIES;
}

/* 
 * Function used to set scenario file path
 * Setting WORK Directory
 * Tokenize file-name, create full path for file name
 * */
static void set_scenario_file_path(char *file_name)
{
  int total_flds;
  char *field[20];
  char org_file_name[FILE_PATH_SIZE];
  sprintf(org_file_name, "%s", file_name);
  
  NIDL (1, "Method called, org_file_name = %s", org_file_name);
  //<workspace>/<profile>/<proj>/<subproj>/<scenario file>
  //Tokenize file name  
  total_flds = get_tokens_(file_name, field, "/", 20);

  NIDL (3, "total_flds = %d", total_flds);
  if (total_flds == 1) { /*bug id: 101320: using default test assets dir */
    NIDL (1, "Setting scenario file path as %s/%s/default/default/scenarios/%s", work_dir, GET_NS_DTA_DIR(), field[0]);
    sprintf(scenario_file, "%s/%s/default/default/scenarios/%s", work_dir, GET_NS_DTA_DIR(), field[0]);
      
    //For Shell send scenario file without work env
    sprintf(scen_file_for_shell, "%s/%s/%s/%s/%s", GET_DEFAULT_WORKSPACE(), GET_DEFAULT_PROFILE(), "default", "default", field[0]);
    sprintf(scen_proj_subproj_dir, "default/default"); 
    //Save senario name
    sprintf(scenario_file_name, "%s", field[0]);
    return;
  }
  else if (total_flds == 5) {
    //set NS TA DIR
    nslb_set_ta_dir_ex1(work_dir, field[0], field[1] ); 
    NIDL (1, "Setting scenario file path as %s/%s/%s/scenarios/%s", GET_NS_TA_DIR(), field[2], field[3], field[4]); 
    sprintf(scenario_file, "%s/%s/%s/scenarios/%s", GET_NS_TA_DIR(), field[2], field[3], field[4]);
     
    //For Shell send scenario file without work env
    sprintf(scen_file_for_shell, "%s/%s/%s/%s/%s", field[0], field[1], field[2], field[3], field[4]);
    sprintf(scen_proj_subproj_dir, "%s/%s", field[2], field[3]);
     
    //Save senario name
    sprintf(scenario_file_name, "%s", field[4]);
    strcpy(netomni_proj_subproj_file, scen_proj_subproj_dir);
    strcpy(netomni_scenario_file, field[4]);
    return;
  }
  else if (total_flds == 11) {
    //set NS TA DIR
    nslb_set_ta_dir_ex1(work_dir, field[4], field[5] );
    NIDL (1, "Setting scenario file path as %s/%s/%s/scenarios/%s", GET_NS_TA_DIR(), field[7], field[8], field[10]);
    sprintf(scenario_file, "%s/%s/%s/scenarios/%s", GET_NS_TA_DIR(), field[7], field[8], field[10]);

    //For Shell send scenario file without work env
    sprintf(scen_file_for_shell, "%s/%s/%s/%s/%s/%s/%s", field[4], field[5], field[6], field[7], field[8], field[9], field[10]);
    sprintf(scen_proj_subproj_dir, "%s/%s", field[7], field[8]);

    //Save senario name
    sprintf(scenario_file_name, "%s", field[10]);
    strcpy(netomni_proj_subproj_file, scen_proj_subproj_dir);
    strcpy(netomni_scenario_file, field[10]);
    return;
  }
     
  NIDL (1, "ERROR!!! Invalid argument format[%s]. The format is <workspace>/<profile>/<proj>/<subproj>/<scenario file>", file_name);
}

/******************************************************************************* 
 * Description		: To set controller path to store generator related data
 *                        Tasks:
 *                        a) Set work directory
 *                        b) If testrun given then directory created at 
 *                           work-directory/logs/TR#num/.controller
 *                        c) Else directory created at following path
 *                           work-directory/.tmp/.controller   
 * Input-Parameter	: None
 * Output-Parameter	: None
 * Return		: None
 *******************************************************************************/ 

static void set_controller_dir_path()
{
  struct stat sb;
  char chk_controller_dir[FILE_PATH_SIZE]; 
  char create_cmd[MAX_COMMAND_SIZE];
  char tmpfs_log_dir[256];
  char tmpfs_controller_dir[FILE_PATH_SIZE];
  int cmd_write_idx = 0;

  sprintf(tmpfs_log_dir, "%s/%s/logs", memory_based_fs_mnt_path, basename(work_dir));
  NIDL(1, "Method called");
  /*Creating CONTROLLER directory*/
  if  (test_run_num != -1) {
    NIDL(2, "If testrun given hence storing in respective log directory");
    sprintf(chk_controller_dir, "%s/logs/TR%d", work_dir, test_run_num); 
    //Check testrun dir exists
    if (stat(chk_controller_dir, &sb) == -1) {
      NS_EXIT(-1, "Test run directory %s not found, please re-run the test", chk_controller_dir);
    }
    sprintf(controller_dir, "%s/logs/TR%d/.controller", work_dir, test_run_num);
    if(memory_based_fs_mode && !stat(memory_based_fs_mnt_path, &sb)) {
      //Bug 81367
      snprintf(tmpfs_controller_dir, FILE_PATH_SIZE, "%s/TR%d/.controller", tmpfs_log_dir, test_run_num);
      if(stat(tmpfs_controller_dir, &sb)) {
        cmd_write_idx = snprintf(create_cmd, MAX_COMMAND_SIZE, "mkdir -p %s;", tmpfs_controller_dir);
      }
      else {
        cmd_write_idx = snprintf(create_cmd, MAX_COMMAND_SIZE, "rm -rf %s/*;", tmpfs_controller_dir);
      }
      snprintf(create_cmd + cmd_write_idx, MAX_COMMAND_SIZE - cmd_write_idx, "rm -rf %s; ln -s %s %s", controller_dir,
                                                                              tmpfs_controller_dir, controller_dir);
      NIDL(2, "Running cmd: %s", create_cmd);
      if (system(create_cmd) < 0)
        NS_EXIT(-1, "Failed to create directory inside memory based file system (%s)", controller_dir);
    }
  } else {
    NIDL(2, "Storing data at path .tmp/.controller");
    sprintf(controller_dir, "%s/.tmp/.controller", work_dir);
    if (mkdir(controller_dir, 0755))
    {
      if(errno != EEXIST)
        NS_EXIT(-1, CAV_ERR_1000005, controller_dir, errno, nslb_strerror(errno));
    }
  }
}

/****************************************************************************
 * Description		: To sort scenario file, here we are using 
 *                        nsi_merge_sort_scen shell for sorting scenario 
 *                        configuration file
 * Input-Parameter      : None
 * Output-Parameter     : None
 * Return               : Return -1 if command fails else return 0 on success
 *****************************************************************************/
#ifdef TEST
static int sort_scenario_file(char *conf_file)
{
  char cmd[512];
  NIDL(1, "Method called scenario_file = %s", conf_file);
  sprintf(new_conf_file, "%s/sorted_conf_file", controller_dir);
  sprintf(cmd, "%s/bin/nsi_merge_sort_scen 0 %s %s", work_dir, conf_file, new_conf_file);

  if (system(cmd) != 0) {                  /* check result if successful if not exit*/
    fprintf(stderr, "\nUnable to sort. Executed cmd = [%s]\n", cmd);
    return FAILURE_EXIT;
  }
  return SUCCESS_EXIT;
}
#endif


/******************************************************
 *    FUNCTIONS FOR GENERATOR FILE
 *****************************************************/

//Print dump messages
static void generator_entry_dump(GeneratorEntry *generator_entry_debug)
{
  NIDL (2, "Method called, total_generator_entries = %d", total_generator_entries);
  int i;
  for (i = 0; i < total_generator_entries; i++)
  {
     NIDL (2, "Flag = %d, Generator_name = %s, IP = %s, Port = %s, Location = %s, Work = %s, Generator_type = %s, Comments = %s", generator_entry_debug[i].used_gen_flag, generator_entry_debug[i].gen_name, generator_entry_debug[i].IP, generator_entry_debug[i].agentport, generator_entry_debug[i].location, generator_entry_debug[i].work, generator_entry_debug[i].gen_type, generator_entry_debug[i].comments);
  }
}

/****************************************************************************
 * Description          : Create generator table
 * Input-Parameter      : 
 * row_num		: used for indexing
 * Output-Parameter     : Set total_generator_entries increment as per entries
 * Return               : Return -1 if allocation fails else return 0 on success
 *****************************************************************************/

int create_generator_table_entry(int* row_num)
{
  NIDL (1, "Method called.");
  if (total_generator_entries == max_generator_entries) {
    generator_entry = (GeneratorEntry *)realloc(generator_entry, (max_generator_entries + DELTA_GENERATOR_ENTRIES) * sizeof(GeneratorEntry));
    if (!generator_entry) {
      fprintf(stderr, "\ncreate_generator_table_entry(): Error allocating more memory for generator_entry entries\n");
      return FAILURE_EXIT;
    } else max_generator_entries += DELTA_GENERATOR_ENTRIES;
  }
  *row_num = total_generator_entries++; 
  return SUCCESS_EXIT;
}

#define RESOLVE_GEN_IP() \
    NIDL (4, "Resolve flag = %d", generator_entry[i].resolve_flag);\
  if (!generator_entry[i].resolve_flag) { \
    if (!nslb_fill_sockaddr(&saddr, generator_entry[i].IP, 80)) { \
      NS_EXIT(-1, CAV_ERR_1014006, generator_entry[i].IP);\
    }\
    sprintf(tmp_ip, "IPV4:%s.", nslb_sockaddr_to_ip((struct sockaddr *)&saddr, 80));\
    if ((rem_port = strrchr(tmp_ip, ':')) != NULL) \
      *rem_port = '\0';\
    strcpy(generator_entry[i].resolved_IP, tmp_ip);\
    generator_entry[i].resolve_flag = 1; \
    NIDL (4, "Resolved ip = %s", generator_entry[i].resolved_IP);\
  } else \
    NIDL (4, "Already resolved ip = %s", generator_entry[i].resolved_IP);

//Print Usage for NS_GENERATOR_FILE keyword
static void usage_ns_generator_file(char *err_msg)
{
  NIDL (1, "Method called, err_msg = %s", err_msg);
  fprintf(stderr, "Error: Invalid value of NS_GENERATOR_FILE keyword: %s\n", err_msg);
  fprintf(stderr, "  Usage: NS_GENERATOR_FILE <file_name>\n");
  fprintf(stderr, "  Where file_name:\n");
  fprintf(stderr, "  All load generators are configured in given file_name with absolute path.\n");
}

/***************************************************************************
 * Description		: Function used to parse NS_GENERATOR_FILE keyword
 *                        Tasks:
 *                         a) Check whether file exists or not
 *                         b) Sort file for unique generator entries 
 *                         c) store sorted generator file at following path: 
 *                            controller-directory/sorted_gen_file.dat
 *                         d) Read file line by line, tokenize extract data 
 *                            and fill GeneratorEntry structure
 * Input-Parameter	:
 * buf			: buffer containing keyword
 * err_msg		: error message
 * Output-Parameter	: Set GeneratorEntry structure
 * Return		: Returns -1 on failure and 0 on success
 *****************************************************************************/

#define CLEAR_WHITE_SPACE(ptr) {while ((*ptr == ' ') || (*ptr == '\t')) ptr++;}
static int kw_set_ns_generator_file (char *buf, char *err_msg, int IS_NS_GEN_FILE)
{
  char keyword[MAX_DATA_LINE_LENGTH] = "\0";
  char tmp[MAX_DATA_LINE_LENGTH]; //This used to check if some extra field is given 
  char file_name[MAX_DATA_LINE_LENGTH] = "\0";
  char tmp_file_name[MAX_DATA_LINE_LENGTH] = "\0";
  int num, total_flds, rnum, line_num = 0;
  char line[2024];
  char msg_line[2024];
  char *field[20], *ptr;
  FILE* fp = NULL;
  char cmd[MAX_DATA_LINE_LENGTH];
  char sorted_gen_file[MAX_DATA_LINE_LENGTH];
  char port[512];
 
  NIDL (4, "Method called, buf = %s", buf);

  if(IS_NS_GEN_FILE) {
    if ((num = sscanf(buf, "%s %s %s", keyword, file_name, tmp)) > 2)
    {
      usage_ns_generator_file("Invalid number of arguments");
      NS_EXIT(-1, "NS_GENERATOR_FILE keyword have invalid number of arguments, please update keyword from usage");
    }  
  } else {
    sprintf(file_name, "%s/etc/.netcloud/generators.dat", g_ns_wdir);
  }

  //Check whether file exists
  if (access(file_name, F_OK))
  {
    NS_EXIT(-1, "Failed to open generator conf file (%s), error:%s, please correct file and re-run the test", file_name, nslb_strerror(errno));
  }

  sprintf(tmp_file_name, "%s/tmp_gen_file.dat", controller_dir);
  if(!IS_NS_GEN_FILE)  //default generator file
    sprintf(cmd, "nc_admin -o show >%s", tmp_file_name);
  else
    sprintf(cmd, "nc_admin -o show -f %s >%s", file_name, tmp_file_name);

  if (system(cmd) != 0) {                  /* check result if successful if not exit*/
    NS_EXIT(FAILURE_EXIT, "Unable to get controller specific generator conf file. Executed cmd = [%s]", cmd);
  }
  
  //sort generator file wrt generator name
  sprintf(cmd, "awk -F'|' '{ generator[$1] = $0 } END { for (l in generator) print generator[l] }' %s > %s/sorted_gen_file.dat; cp %s/sorted_gen_file.dat %s/logs/TR%d/NetCloud/.gen_file.dat", tmp_file_name, controller_dir, controller_dir, g_ns_wdir, test_run_num);
    
  sprintf(sorted_gen_file, "%s/sorted_gen_file.dat", controller_dir);
 
  NIDL(4, "sorted_gen_file = %s", sorted_gen_file);

  if (system(cmd) != 0) {                  /* check result if successful if not exit*/
    NS_EXIT(FAILURE_EXIT, "Unable to sort generator conf file. Executed cmd = [%s]", cmd);
  }

  //Read Sorted file  
  if ((fp = fopen(sorted_gen_file, "r")) == NULL)
  {
    NS_EXIT(-1, "Failed to open sorted scenario file %s", sorted_gen_file);
  }
  
  //Read file line-by line and fill structure as per file entries
  while (fgets(line, 2024, fp) != NULL)
  {
    //fputs(line, stdout);
    char *line_ptr = line;
    CLEAR_WHITE_SPACE(line_ptr);
    NIDL (3, "line_ptr = %s, line = %s", line_ptr, line);
    memmove(line, line_ptr, strlen(line_ptr));
    //Ignoring generator header and blank or commented lines
    if(line[0] == '\n' || line[0] == '#') {
      NIDL(4, "Ignoring generator header or blank/commented line from generator file, line number = %d", line_num);
      line_num++;
      continue;
    }
    if((ptr = strchr(line, '\n')) != NULL) //Replace newline with NULL char in each line before saving fields in structure
      *ptr = '\0';
    strcpy(msg_line, line); //Copying line for debugging purpose
    total_flds = get_tokens_(line, field, "|", 7);
    NIDL (3, "total_flds = %d, line_num = %d, line = %s", total_flds, line_num, msg_line);

    //Ignoring generator header and blank or commented lines
    if (!strcmp(field[1], "IP")) {
      NIDL(4, "Ignoring generator header line from generator file ");
      line_num++;
      continue;
    }
    //Validation check for CavMonAgent Port
    if (ni_is_numeric(field[2]) == 0) {
      NS_EXIT(-1, "Please input valid cavisson monitoring agent port number in generator"
                  " conf file %s at line %s and re-run the test", field[2], msg_line);
    }

    strcpy(port, field[2]);
    //Validation check for generator type, it should be Internal or External
    if (!((!strcasecmp(field[5], "Internal")) || (!strcasecmp(field[5], "External")))) { 
      NS_EXIT(-1, "Please input valid generator type either Internal or External in generator"
                           " conf file %s at line %s", field[5], msg_line);
    }

    if (create_generator_table_entry(&rnum) == FAILURE_EXIT)
    {
      NS_EXIT(-1, "Failed to create generator list entry");
    }
    
    //Copy value to struct 
    strcpy((char *)generator_entry[rnum].gen_name, field[0]);
    strcpy(generator_entry[rnum].IP, field[1]);
    strcpy(generator_entry[rnum].agentport, port);
    strcpy(generator_entry[rnum].location, field[3]);
    strcpy(generator_entry[rnum].work, field[4]);
    strcpy(generator_entry[rnum].gen_type, field[5]);
    strcpy(generator_entry[rnum].comments, field[6]);
    generator_entry[rnum].used_gen_flag = 0; //intially flag is disable
    generator_entry[rnum].num_groups = 0;
    generator_entry[rnum].mark_gen = 0;
    generator_entry[rnum].loc_idx = 0;
    nslb_md5(generator_entry[rnum].gen_name, generator_entry[rnum].token);
    //Increment line_num 
    line_num++;
  }   
  fclose(fp);
  
  return SUCCESS_EXIT;
}

/**********************************************************************
 *       Functions for GENERATOR LIST
 * *******************************************************************/

/*
 * Sort generator list with respect to used_gen_flag, 
 * In desending Order */
//static int sort_generator_list (const void *G1, const void *G2)
int sort_generator_list (const void *G1, const void *G2)
{
  NIDL (1, "Method called.");
  struct GeneratorEntry *g1, *g2;
  g1 = (struct GeneratorEntry *)G1;
  g2 = (struct GeneratorEntry *)G2;

  if (g1->used_gen_flag > g2->used_gen_flag)
    return -1;
  if (g1->used_gen_flag == g2->used_gen_flag)
    return 0;
  if (g1->used_gen_flag < g2->used_gen_flag)
    return 1;
 
  return SUCCESS_EXIT;
}


/*
 * Function check whether generator name exists in genrator list
 * Returns 
 * On success return 0 else -1 on generator name mismatch
 * */
static int check_gen_name_exist_list (char *gen_name, char *err_msg)
{
  int i;

  NIDL (2, "Method called, gen_name = %s", gen_name);

  for (i = 0; i < total_generator_entries; i++)
  {
    if (!(strcmp(gen_name, (char *)generator_entry[i].gen_name)))  
    {
      NIDL(3, "Generator name found at index = %d, enabling flag", i);
	generator_entry[i].used_gen_flag += 1;
      /*Bug#10479:In Kohls duplicate entries were found in NS_GENERATOR keyword, 
        due to which wrong generator entry was getting pick from generator file
        hence in order to resolve such issue we need to identify such errors and 
        terminate test because this might result into distribution issues
       */
      if (generator_entry[i].used_gen_flag > 1)
      {
        sprintf(err_msg, "Generator name '%s' used more than once in scenario, please remove duplicate entry"
                         , gen_name);
        return FAILURE_EXIT;
      } 
      max_used_generator_entries++;
      NIDL(3, "max_used_generator_entries = %d", max_used_generator_entries); 
      return SUCCESS_EXIT;
    } 
  }
  sprintf(err_msg, "Please add '%s' generator in generator file and re-run the test", gen_name); 
  return FAILURE_EXIT;
}

/****************************************************************************
 * Description          : Create used generator list
 * Input-Parameter      :
 * row_num              : used for indexing
 * Output-Parameter     : Set total_generator_list_entries increment as per entries
 * Return               : Return -1 if allocation fails else return 0 on success
 *****************************************************************************/

static int create_generator_list_table_entry(int* row_num, char *err_msg)
{
  NIDL (1, "Method called.");
  if (total_used_generator_list_entries == max_used_generator_list_entries) {
    gen_used_list = (GeneratorUsedList *)realloc(gen_used_list, (max_used_generator_list_entries + DELTA_USED_GENERATOR_ENTRIES) * sizeof(GeneratorUsedList));
    if (!gen_used_list) {
      sprintf(err_msg, "Error allocating more memory for generator_entry entries");
      return FAILURE_EXIT;
    } else max_used_generator_list_entries += DELTA_USED_GENERATOR_ENTRIES;
  }
  *row_num = total_used_generator_list_entries++;
  return SUCCESS_EXIT;
}

//Print usage for keyword NS_GENERATOR
/*static void ns_generator_list_usage(char *err_msg)
{
  NIDL(1, "Method called, err_msg = %s", err_msg);
  fprintf(stderr, "Error: Invalid value of NS_GENERATOR keyword: %s\n", err_msg);
  fprintf(stderr, "  Usage: NS_GENERATOR <generator_name>\n");
  fprintf(stderr, "  Where generator_name:\n");
  fprintf(stderr, "  Name of generator used in load balancing.\n");
  NS_EXIT(-1, "%s\nUsage: NS_GENERATOR <generator_name>", err_msg);
}
*/
static void generator_used_entry_dump(GeneratorUsedList *gen_used_list_debug)
{
  int i;
  NIDL (2, "Method called, total_used_generator_list_entries = %d", total_used_generator_list_entries);
  for (i = 0; i < total_used_generator_list_entries; i++)
     NIDL (2, "Generator_name = %s", gen_used_list_debug[i].generator_name);
}

/***************************************************************************
 * Description          : Function used to parse NS_GENERATOR keyword
 *                        Tasks:
 *                         a) Validate whether given generator name exists 
 *                            in file. 
 *                         b) Invalid entry, exit test
 *                         c) For valid entry, set used_gen_flag 
 *                         d) fill GeneratorUsedList struct
 *                         e) sort entries with enabled flag in descending order
 * Input-Parameter      :
 * buf                  : buffer containing keyword
 * err_msg              : error message
 * Output-Parameter     : Set used_gen_flag
 * Return               : Returns -1 on failure and 0 on success
 *****************************************************************************/

static int kw_set_ns_generator_list(char *buf, char *err_msg)
{
  char keyword[MAX_DATA_LINE_LENGTH] = "\0";
  char tmp[MAX_DATA_LINE_LENGTH]; //This used to check if some extra field is given
  char gen_name[MAX_DATA_LINE_LENGTH] = "\0";
  int num, rnum, ret;
  char error_msg[MAX_DATA_LINE_LENGTH];

  NIDL(4, "Method called, buf = %s", buf);
  num = sscanf(buf, "%s %s %s", keyword, gen_name, tmp);

  if (num > 2)
  {
    NS_KW_PARSING_ERR(buf, 0, err_msg, NS_GENERATOR_USAGE, CAV_ERR_1011168, CAV_ERR_MSG_1);
    //ns_generator_list_usage("Invalid number of arguments");
  }
  
  if ((ret = check_gen_name_exist_list(gen_name, error_msg)) == FAILURE_EXIT)
  {
    NS_KW_PARSING_ERR(buf, 0, err_msg, NS_GENERATOR_USAGE, CAV_ERR_1011168, error_msg);
  }
  // Sort generator list with regard to used_generator_flag
  if (create_generator_list_table_entry(&rnum, error_msg) == FAILURE_EXIT)
  {
    NS_EXIT(-1, error_msg);
    //NS_KW_PARSING_ERR(buf, 0, err_msg, NS_GENERATOR_USAGE, CAV_ERR_1011168, error_msg);
  }

  //Store generator name 
  strcpy(gen_used_list[rnum].generator_name, gen_name);

  //Sort generator list with respect to used list
  qsort(generator_entry, total_generator_entries, sizeof(struct GeneratorEntry), sort_generator_list);

  NIDL (3, "max_used_generator_entries = %d", max_used_generator_entries);
  return SUCCESS_EXIT;
}     

/****************************************************************************
 *           FUNCTION FOR OTHER KEYWORDS	
 * *************************************************************************/

#define add_generator_wrt_controller_type(gen_type) \
{ \
  NIDL(1, "Method called");\
  if(netcloud_mode == 2) {\
    if(!strcmp(controller_type, "External")) { \
      if(!strcmp(gen_type, "Internal")) {\
        NS_EXIT(-1, CAV_ERR_1014007, generator_entry[i].gen_name);\
      }\
    }\
  }\
} 

//Print SGRP usage
/*static void usage_kw_set_sgrp(char *err_msg, char *buf)
{
  NIDL(1, "Method called, err_msg = %s", err_msg);
  fprintf(stderr, "Error: Invalid value of SGRP: %s\n", err_msg);
  fprintf(stderr, "Keyword: %s", buf);
  fprintf(stderr, "  Usage: SGRP <GroupName> <GeneratorName> <ScenType> <user-profile> "
          "<type(0 for Script or 1 for URL)> <session-name/URL> <num-or-pct> <cluster_id>\n");
  NS_EXIT(-1, "%s\nUsage: SGRP <GroupName> <GeneratorName> <ScenType> <user-profile> "
          "<type(0 for Script or 1 for URL)> <session-name/URL> <num-or-pct> <cluster_id>", err_msg);
}*/

//Create SGRP entry
static int create_sgrp_entry(int* row_num, char *err_msg)
{
  NIDL (1, "Method called.");
  if (total_sgrp_entries == max_sgrp_entries) {
    scen_grp_entry = (ScenGrpEntry*)realloc(scen_grp_entry, (max_sgrp_entries + DELTA_SGRP_ENTRIES) * sizeof(ScenGrpEntry));
    if (!scen_grp_entry)  { //ToDo
      sprintf(err_msg, "Error allocating more memory for scen_grp_entry");
      return FAILURE_EXIT;
    } else max_sgrp_entries += DELTA_SGRP_ENTRIES;
  }
  *row_num = total_sgrp_entries++;
  return SUCCESS_EXIT;
}

//Fill SGRP entires 
static int create_table_for_sgrp_keyword(char *scen_type, char *sg_name, char *uprof_name, int script_or_url, char *sess_or_url_name, double pct, char *cluster_id, int total_generator_used_in_grp, int sg_fields, int pct_flag_set, char *buf, char *err_msg)
{
  int i, rnum;
  //Added var for fsr  
  double fsr_pct_value;

  int num_field;
  char *fields[10];

  char sess_name_with_proj_subproj[2 * 1024];
  char temp_sess_or_url_name[MAX_DATA_LINE_LENGTH + 1];
  char proj_subproj_file[FILE_PATH_SIZE + 1];

  NIDL(2, "Method called, total_generator_used_in_grp = %d", total_generator_used_in_grp);

  NIDL(2, "scen_type = %s, sg_name = %s, uprof_name = %s, script_or_url = %d, sess_or_url_name = %s, pct = %d, cluster_id = %s, sg_fields = %d", scen_type, sg_name, uprof_name, script_or_url, sess_or_url_name, (int)pct, cluster_id, sg_fields);

  if(script_or_url == 100)
    jmx_enabled_script = 1;
  for (i = 0; i < total_generator_used_in_grp; i++)
  {
    if (create_sgrp_entry(&rnum, err_msg) == -1)
    {
      //NS_KW_PARSING_ERR(buf, 0, err_msg, SGRP_USAGE, CAV_ERR_1011170, CAV_ERR_MSG_1);
      NS_EXIT(-1, err_msg);
    }
    /* We construct basic phases, in case of by_group we need to add schedule for each group 
     * whereas in case of schedule_by_scenario schedule added once*/
    if (schedule_by == SCHEDULE_BY_SCENARIO) {
      if (scenario_schedule_ptr == NULL) {
        //scenario_schedule_ptr = (schedule*)malloc(sizeof(schedule));
        NSLB_MALLOC(scenario_schedule_ptr, (sizeof(schedule)), "scenario schedule", -1, NULL);
        NIDL(2, "Malloc'ed scenario_schedule_ptr = %p", scenario_schedule_ptr);
        initialize_schedule_struct(scenario_schedule_ptr, -1);
      }
    } else if (schedule_by == SCHEDULE_BY_GROUP) { 
      //group_schedule_ptr = (schedule*)realloc(group_schedule_ptr, (sizeof(schedule) * (rnum + 1)));
      NSLB_REALLOC(group_schedule_ptr, (sizeof(schedule) * (rnum + 1)), "group schedule", -1, NULL);
      NIDL(2, "Realloc'ed group_schedule_ptr = %p", group_schedule_ptr);
      initialize_schedule_struct(&group_schedule_ptr[rnum], rnum);
    }

    scen_grp_entry[rnum].group_num = rnum;

    //Add number of generator
    scen_grp_entry[rnum].num_generator = total_generator_used_in_grp;

    //Copy scen_type
    strcpy(scen_grp_entry[rnum].scen_type, scen_type);
    if (strcmp(scen_type, "NA") == 0) {
      if (mode == TC_FIX_CONCURRENT_USERS)
        scen_grp_entry[rnum].grp_type = TC_FIX_CONCURRENT_USERS;
      else
        scen_grp_entry[rnum].grp_type = TC_FIX_USER_RATE; 
    }
    else if (strcmp(scen_type, "FIX_CONCURRENT_USERS") == 0)
      scen_grp_entry[rnum].grp_type = TC_FIX_CONCURRENT_USERS;
    else if (strcmp(scen_type, "FIX_SESSION_RATE") == 0)
      scen_grp_entry[rnum].grp_type = TC_FIX_USER_RATE;

    //Add the scenrio group name
    strcpy(scen_grp_entry[rnum].scen_group_name, sg_name);
    
    //Add the generator name
    strcpy(scen_grp_entry[rnum].generator_name, gen_list[i].generator_name);
    NIDL(3, "Generator name, scen_grp_entry[%d].generator_name = %s", rnum, scen_grp_entry[rnum].generator_name);

    //Copy User Profile Name
    strcpy(scen_grp_entry[rnum].uprof_name, uprof_name);

    scen_grp_entry[rnum].script_or_url = script_or_url; 

    //sess_or_url_name : will may have full path of script , for example : 
    // 1) DEV/UT/HPD  2) DEV/DEV/HPD   3) Hpd
    // So need to check and process accordingly
    NIDL(3, "sess_or_url_name = %s, script_or_url = %d", sess_or_url_name, script_or_url);
    if (script_or_url != 1)
    {
      if(strchr(sess_or_url_name, '/'))
      {
         strcpy(temp_sess_or_url_name, sess_or_url_name);
         num_field =  get_tokens(temp_sess_or_url_name, fields, "/", 10);
         if(num_field != 3)
           NS_KW_PARSING_ERR(buf, 0, err_msg, SGRP_USAGE, CAV_ERR_1011335, temp_sess_or_url_name);

        strcpy(scen_grp_entry[rnum].proj_name, fields[0]);
        strcpy(scen_grp_entry[rnum].sub_proj_name, fields[1]);
        strcpy(scen_grp_entry[rnum].sess_name, fields[2]); 

	/*bug id: 101320:using GET_NS_TA_DIR() */	
        sprintf(temp_sess_or_url_name, "%s/%s/%s/scripts/%s", GET_NS_TA_DIR(), scen_grp_entry[rnum].proj_name, scen_grp_entry[rnum].sub_proj_name, scen_grp_entry[rnum].sess_name); //added code to check script exists
        if(access(temp_sess_or_url_name, F_OK) != 0)
        {
          NS_KW_PARSING_ERR(buf, 0, err_msg, SGRP_USAGE, CAV_ERR_1011335, temp_sess_or_url_name);
         //NS_EXIT(-1, "Script path (%s) doesn't exists, please provide valid script path", temp_sess_or_url_name);
        }
      }
      else
      {
        //netomni_proj_subproj_file will have project sub-project name stored at the time of scenario file name parsing
        //if nsu_start_test -n UT/UT/DEV.conf : proj/sub-proj will be default/default, so netomni_proj_subproj_file will have default/default
        //if nsu_start_test -n DEV.conf : proj/sub-proj will be UT/UT, so netomni_proj_subproj_file will have UT/UT
        //here, netomni_proj_subproj_file used in ns_master.c, so retaining local copy to find fields as get_tokens() update variable
        strcpy(proj_subproj_file, netomni_proj_subproj_file);
        if(strchr(proj_subproj_file, '/')){
          num_field =  get_tokens(proj_subproj_file, fields, "/", 10);
          if(num_field != 2)
          {
             NS_KW_PARSING_ERR(buf, 0, err_msg, SGRP_USAGE, CAV_ERR_1011334, proj_subproj_file);
           // NS_EXIT(-1, "Project and subproject are not in proper format (%s), please"
            //         " provide correct format and re-run the test", proj_subproj_file);
          }
          strcpy(scen_grp_entry[rnum].proj_name, fields[0]);
          strcpy(scen_grp_entry[rnum].sub_proj_name, fields[1]);
        }
        strcpy(scen_grp_entry[rnum].sess_name, sess_or_url_name);
      }

      //Find script idx with full path
      snprintf(sess_name_with_proj_subproj, 2048, "%s/%s/%s", 
                    scen_grp_entry[rnum].proj_name, scen_grp_entry[rnum].sub_proj_name, scen_grp_entry[rnum].sess_name);
      NIDL(3, "rnum = %d, sess_name_with_proj_subproj = %s, scen_grp_entry[rnum].proj_name = %s, scen_grp_entry[rnum].sub_proj_name = %s, "
               "sess_or_url_name = %s, netomni_proj_subproj_file = %s", rnum, sess_name_with_proj_subproj, scen_grp_entry[rnum].proj_name,
                  scen_grp_entry[rnum].sub_proj_name, sess_or_url_name, netomni_proj_subproj_file);

      if (i == 0)
      {
        //if (find_script_idx(scen_grp_entry[rnum].sess_name) == -1)
        if (find_script_idx(sess_name_with_proj_subproj) == -1)
        {
          //if(create_script_table(scen_grp_entry[rnum].sess_name) == -1)
          if(create_script_table(sess_name_with_proj_subproj, err_msg) == -1)
          {
            //NS_KW_PARSING_ERR(buf, 0, err_msg, SGRP_USAGE, CAV_ERR_1011170, CAV_ERR_MSG_1);
            NS_EXIT(-1, err_msg);
          }
        } 
      }
    }
    else
      strcpy(scen_grp_entry[rnum].sess_name, sess_or_url_name);

    if (prof_pct_mode == PCT_MODE_PCT) {
      scen_grp_entry[rnum].percentage = (pct * 100.0);
    } 
    else 
    {
      //Added check for FSR
      if (scen_grp_entry[rnum].grp_type == TC_FIX_USER_RATE) {
        /* Added Check for minimum limit: Minimum value for SGRP qty is 0.01
         * checking whether given qty is greater than total number of generators per grp * 0.01  
         * */
        if (pct < (total_generator_used_in_grp * 0.01)) {
          NIDL(3, "pct = %f, total_generator_used_in_grp = %d, total_generator_used_in_grp * 0.01 = %d", pct, 
                  total_generator_used_in_grp, (total_generator_used_in_grp * 0.01));

          NS_KW_PARSING_ERR(buf, 0, err_msg, SGRP_USAGE, CAV_ERR_1011333, "Sessions", pct, total_generator_used_in_grp, scen_grp_entry[rnum].scen_group_name, "Sessions");

         // NS_EXIT(-1, "Number of sessions (%.02f) cannot be less than total number of generators (%d) used in "
           //    "group (%s), please increase number of sessions and re-run the test", pct, total_generator_used_in_grp,
             //   scen_grp_entry[rnum].scen_group_name);
        }
        //Multiply qty with session multiplier 
        fsr_pct_value = pct * SESSION_RATE_MULTIPLIER;
        NIDL(3, "fsr_pct_value = %f", fsr_pct_value);
      }
      else {
        NIDL(3, "pct = %d", (int)pct);
        if (pct < total_generator_used_in_grp) {
          NIDL(3, "pct = %d, total_generator_used_in_grp = %d", (int)pct, total_generator_used_in_grp);
          NS_KW_PARSING_ERR(buf, 0, err_msg, SGRP_USAGE, CAV_ERR_1011333, "Users", pct, total_generator_used_in_grp, scen_grp_entry[rnum].scen_group_name, "Users");
        }
      }
      //array to get quantity divide wrt no of generators
      NIDL(4, "pct_flag_set = %d", pct_flag_set);
      //This pct_value will be used for as per generator capacity
      if (scen_grp_entry[rnum].grp_type == TC_FIX_USER_RATE) {
        scen_grp_entry[rnum].pct_value = fsr_pct_value;
        NIDL(4, "scen_grp_entry[%d].pct_value = %lf", rnum, scen_grp_entry[rnum].pct_value); 
      } else {
        scen_grp_entry[rnum].pct_value = pct;
        NIDL(4, "scen_grp_entry[%d].pct_value = %lf", rnum, scen_grp_entry[rnum].pct_value);
      }
      if (pct_flag_set == 0) {
          if (scen_grp_entry[rnum].grp_type == TC_FIX_USER_RATE)
            divide_usr_wrt_generator(total_generator_used_in_grp, fsr_pct_value, gen_list, buf, err_msg); 
          else  
            divide_usr_wrt_generator(total_generator_used_in_grp, pct, gen_list, buf, err_msg);
      } else {/*Distribute users/sess with respect to pct defined in SGRP for each generator*/
        NIDL(4, "Distribute percentage total_generator_used_in_grp = %d, sum =%d", total_generator_used_in_grp, ((mode == TC_FIX_USER_RATE)?fsr_pct_value:pct));
        pct_division_among_gen_per_grp(gen_list, ((mode == TC_FIX_USER_RATE)?fsr_pct_value:pct), total_generator_used_in_grp, rnum, buf, err_msg);             
      } 
    }

    NIDL(4, "gen_list[%d].qty_per_gen = %d", i, gen_list[i].qty_per_gen);
    if (scen_grp_entry[rnum].grp_type == TC_FIX_USER_RATE) {
      /* Update percentage in case of FSR instead of quantity, becoz double var was required
       * to store session rate*/
      if (prof_pct_mode != PCT_MODE_PCT) // We have already updated percentage above in case of pct
        scen_grp_entry[rnum].percentage = (double)gen_list[i].qty_per_gen;
      NIDL(2, "scen_grp_entry[%d].percentage = %0.2f", rnum, scen_grp_entry[rnum].percentage);
      /*Populate vuser_rpm with total session rate (sum all groups)*/
      vuser_rpm += scen_grp_entry[rnum].percentage;
      NIDL(4, "vuser_rpm = %f", vuser_rpm);
    } else {
      scen_grp_entry[rnum].quantity = gen_list[i].qty_per_gen;
      /*Populate num_connections with total SGRP quantity*/
      num_connections += scen_grp_entry[rnum].quantity;
    }
  
    //Copy cluster id
    strcpy(scen_grp_entry[rnum].cluster_id, cluster_id); 
    //Copy list of generator ids in first SGRP entry, later we will be copying the list in NS
    if (i == 0)
    { 
      int k; 
      NIDL(4, "List of generator id for scenario group %s", scen_grp_entry[rnum].scen_group_name);
      //scen_grp_entry[rnum].generator_name_list = malloc(total_generator_used_in_grp * sizeof(char *));
      NSLB_MALLOC(scen_grp_entry[rnum].generator_name_list, (total_generator_used_in_grp * sizeof(char *)), "generator name list", rnum, NULL);
      /*In case of PCT mode, user/sess distribution is done later therefore need to save pct values for each group corresponding to ids*/
      //scen_grp_entry[rnum].gen_pct_array =  malloc(total_generator_used_in_grp * sizeof(double));
      NSLB_MALLOC_AND_MEMSET(scen_grp_entry[rnum].gen_pct_array, (total_generator_used_in_grp * sizeof(double)), "generator pct array", rnum, NULL);
      for (k = 0; k < total_generator_used_in_grp; k++) {
        //scen_grp_entry[rnum].generator_name_list[k] = malloc(GENERATOR_NAME_LEN * sizeof(char));
        NSLB_MALLOC(scen_grp_entry[rnum].generator_name_list[k], (GENERATOR_NAME_LEN * sizeof(char)), "generaator name in list", k, NULL);
        strcpy(scen_grp_entry[rnum].generator_name_list[k], gen_list[k].generator_name);
        if (pct_flag_set) {
          NIDL(4, "gen_list[k].qty_per_gen = %d", gen_list[k].qty_per_gen);
          scen_grp_entry[rnum].pct_flag_set = pct_flag_set; //Set flag later used while distributing among generators 
          scen_grp_entry[rnum].gen_pct_array[k] = gen_list[k].pct_per_gen;
          NIDL(4, "scen_grp_entry[%d].gen_pct_array[%d] = %d", rnum, k, scen_grp_entry[rnum].gen_pct_array[k]);
        }
        NIDL(4, "scen_grp_entry[%d].generator_name_list[%d] = %s", rnum, k, scen_grp_entry[rnum].generator_name_list[k]);
      } 
    }
  }
  return SUCCESS_EXIT;
}

char *g_datadir[MAX_UNIX_FILE_NAME + 1];

static int get_grp_idx(char *grp_name)
{
  int i;
  for (i = 0; i < total_sgrp_entries; i++) {
    if (strcmp(grp_name, scen_grp_entry[i].scen_group_name) == 0) {
      return scen_grp_entry[i].group_num;
    }
  }
  return -1;
}

char **g_data_dir_table = NULL;

static int kw_set_datadir(char *buf, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];  //size=2048
  char grp[MAX_DATA_LINE_LENGTH];
  int mode;
  char kw_mode[MAX_DATA_LINE_LENGTH];
  char temp[MAX_DATA_LINE_LENGTH];
  int num;
  char data_dir[MAX_UNIX_FILE_NAME + 1]; //size=256
  char datadir_withpath[MAX_DATA_LINE_LENGTH];
  struct stat s;
  //static char g_data_dir[total_sgrp_entries][MAX_UNIX_FILE_NAME + 1];
  //char g_data_dir[total_sgrp_entries][MAX_UNIX_FILE_NAME + 1];
  int i;
  int grp_idx = -1;

  if(!g_data_dir_table){
    MY_MALLOC(g_data_dir_table, sizeof(char *) * total_sgrp_entries, "g_data_dir_table", -1);
    for(i = 0; i < total_sgrp_entries; i++)
      MY_MALLOC_AND_MEMSET(g_data_dir_table[i], MAX_UNIX_FILE_NAME + 1, "g_data_dir_table", -1);
  }

  //g_data_dir_table = &g_data_dir;
  num = sscanf(buf, "%s %s %s %s %s", keyword, grp, kw_mode, data_dir, temp);
  if(num>4) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_DATADIR_USAGE, CAV_ERR_1060085, CAV_ERR_MSG_1);
  }
  if(nslb_atoi(kw_mode, &mode) < 0) {  
  NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_DATADIR_USAGE, CAV_ERR_1060085, CAV_ERR_MSG_12);
  }
  if((mode < 0) || (mode > 1)) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_DATADIR_USAGE, CAV_ERR_1060085, CAV_ERR_MSG_12);
  }

  if(mode == USE_SPECIFIED_DATA_DIR) {
    if(!strcmp(grp, "ALL"))
    {
      if(validate_and_copy_datadir(0, data_dir, datadir_withpath, err_msg) < 0)
        NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_DATADIR_USAGE, CAV_ERR_1060085, err_msg);
 
      for(i =0; i < total_sgrp_entries; i++)
        strcpy(g_data_dir_table[i], datadir_withpath);
    }
    else
    {
      grp_idx = get_grp_idx(grp);
      if((grp_idx < 0) || (validate_and_copy_datadir(0, data_dir, g_data_dir_table[grp_idx], err_msg) < 0))
        NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_DATADIR_USAGE, CAV_ERR_1060085, err_msg);
    }
  }
} 

/*
 * Parse SGRP keyword
 * Validation checks: 
 * a) SGRP must support new format
 * b) Generator name with NA or in case of invalid entries
 * exit test
 * */                     
static int kw_set_sgrp(char *buf, char *err_msg, int sg_fields)
{
  double pct;
  char keyword[MAX_DATA_LINE_LENGTH];
  char sess_or_url_name[MAX_DATA_LINE_LENGTH]; //Change it may be script or url
  char uprof_name[MAX_DATA_LINE_LENGTH];
  char sg_name[MAX_DATA_LINE_LENGTH];
  char scen_type[MAX_DATA_LINE_LENGTH];
  char cluster_id[MAX_DATA_LINE_LENGTH];
  char generator_name[8096];
  int num;
  int total_generator_used_in_grp = 0, pct_flag_set = 0;
  //Resolve ip given in generator file
  struct sockaddr_in6 saddr;
  char tmp_ip[128], *rem_port;;
  cluster_id[0] = '\0';
  script_or_url = 0;//Default is script 
  NIDL(2, "Method called.");

  num = sscanf(buf, "%s %s %s %s %s %d %s %lf %s", keyword, sg_name, generator_name, scen_type,
                             uprof_name, &script_or_url, sess_or_url_name, &pct, cluster_id);
  NIDL(2, "num = %d, sg_fields = %d", num, sg_fields);

  if ((strcasecmp(scen_type, "NA") == 0) || (strcmp(scen_type, "FIX_CONCURRENT_USERS") == 0) || (strcmp(scen_type, "FIX_SESSION_RATE") == 0))
  {
    NIDL(2, "New SGRP format, hence validate with incremented field count");
    if (num < (sg_fields + 1 + 1 + 1)) //Added generator_name field
      NS_KW_PARSING_ERR(buf, 0, err_msg, SGRP_USAGE, CAV_ERR_1011300, CAV_ERR_MSG_1);
  }
  if (strcasecmp(sg_name, "ALL") == 0) {
    NS_KW_PARSING_ERR(buf, 0, err_msg, SGRP_USAGE, CAV_ERR_1011301, "Group name can never be ALL");
  }

  if ((strcmp(scen_type, "NA") == 0) || (strcmp(scen_type, "FIX_CONCURRENT_USERS") == 0) || (strcmp(scen_type, "FIX_SESSION_RATE") == 0))
   NIDL(2, "scen_type = %s", scen_type);
  else {
    NS_KW_PARSING_ERR(buf, 0, err_msg, SGRP_USAGE, CAV_ERR_1011332, scen_type);
  }   
  
  NIDL(2, "scen_type = %s", scen_type);
  
  if (mode != TC_MIXED_MODE) {
    if (strcmp(scen_type, "NA") != 0) {  
      NS_KW_PARSING_ERR(buf, 0, err_msg, SGRP_USAGE, CAV_ERR_1011331, scen_type);
    //  usage_kw_set_sgrp("ScenType has to be NA in SGRP", buf);
    }
  } else if (mode == TC_MIXED_MODE) {
    if ((strcmp(scen_type, "FIX_CONCURRENT_USERS") != 0) &&
        (strcmp(scen_type, "FIX_SESSION_RATE") != 0)) {
       NS_KW_PARSING_ERR(buf, 0, err_msg, SGRP_USAGE, CAV_ERR_1011303, "ScenType can be either FIX_CONCURRENT_USER or FIX_SESSION_RATE for Mixed type scenario");
     // usage_kw_set_sgrp("In case of MIX-MODE, ScenType should be either FIX_CONCURRENT_USERS or FIX_SESSION_RATE", buf);
    }
  }
 
  if (pct <= 0) {
      NS_KW_PARSING_ERR(buf, 0, err_msg, SGRP_USAGE, CAV_ERR_1011306, "User, Session or Percentage value must be greater then zero.");
   // NS_EXIT(FAILURE_EXIT, "Invalid value (%f) of pct or num user. Need to be greater than 0", pct);
  }

  if ((strcasecmp(generator_name, "NA") == 0) || (strcasecmp(generator_name, "NONE") == 0)) {
     NS_KW_PARSING_ERR(buf, 0, err_msg, SGRP_USAGE, CAV_ERR_1011302, "Generator name can never be NA or NONE");
    //usage_kw_set_sgrp("Invalid generator name in keyword, generator name cannot be NA or NONE", buf);
  } 
  else if ((strcasecmp(generator_name, "ALL") == 0)) {
    static int num_grp_count = 0;
    static int total_local_num_users_sessions = 0;
    static int remaining_local_num_users_sessions_leftover;
    static int remaining_local_num_users_sessions;
    if (!total_gen_location_entries || g_gen_location_processed)
    {
      //In ALL case, send maximum used generator entries
      NIDL(3, "Generator name is ALL, max_used_generator_entries = %d, pct = %d", max_used_generator_entries, (int)pct);
      int i;
      int pct_value = 0;
      int remaining_pct = 0;
      int tmp_num_users_sessions;
      total_generator_used_in_grp = max_used_generator_entries;
      if(g_gen_location_processed)
      {
        // In case of fcu if number of users are less then number of generators 
        // then users can't  be disctrbuted among all genrators so we have to reduce the number of generators as users
        // and recalcuate the percentage distribution of users.
        if (mode == TC_FIX_CONCURRENT_USERS)
        {
          // if mode is percantage with fix concurrent user then 
          //      calculate the number of users
          //      calculate the remaining undivied users and  distribute on groups
          if (prof_pct_mode == PCT_MODE_PCT)
          {
            tmp_num_users_sessions = (pct * local_num_users) / 100;
            tmp_num_users_sessions += (remaining_local_num_users_sessions / num_grp_count);
            if (remaining_local_num_users_sessions_leftover > 0)
            {
               tmp_num_users_sessions++;
               remaining_local_num_users_sessions_leftover--;
            }
          }
          else
          {
            tmp_num_users_sessions = (int)pct;
          }

          // if num of users are less then number of genrator
          //    reduce the number of generator to number of users
          //    calculate the new percentage value for each generator
          //    calculate the remaining values  
          if ((tmp_num_users_sessions) < total_generator_used_in_grp)
          {
            total_generator_used_in_grp = tmp_num_users_sessions;
            pct_value =  (100 * 100)/total_generator_used_in_grp;
            remaining_pct = (100 * 100)%total_generator_used_in_grp;
          }
        }
      }
      //Size of struct = max used generator entries * sizeof(gen_name_quantity_list)
      //gen_list = (gen_name_quantity_list *)malloc(total_generator_used_in_grp *  sizeof(gen_name_quantity_list));
      NSLB_MALLOC(gen_list, (total_generator_used_in_grp *  sizeof(gen_name_quantity_list)), "gen name quantity list", -1, NULL);
      static int nextgen = 0;
      int g;
      //Copy all used generator names
      for ( g = 0; g < total_generator_used_in_grp; g++)
      {
        i = nextgen++;
        nextgen %= max_used_generator_entries;
        add_generator_wrt_controller_type(generator_entry[i].gen_type)
        strcpy(gen_list[g].generator_name, (char *)generator_entry[i].gen_name);
        gen_list[g].gen_id = i;
        generator_entry[i].used_gen_flag = 2;
        if (g_gen_location_processed)
        {
          gen_list[g].pct_per_gen = (pct_value)?pct_value:generator_entry[i].pct_value;
          if (remaining_pct > 0)
          {
             remaining_pct--;
             gen_list[g].pct_per_gen++;
          }
          pct_flag_set = 1;
          NIDL(3, "Percentage provided for gen = %f", gen_list[g].pct_per_gen);
        }
        RESOLVE_GEN_IP() 
      }
    } 
    else 
    {
      int per_location_count;
      int remaining_count;
      static int next_location = 0;

      // In case of PCT data will be distributed later but here we have to calculate
      // how much data will be discrbuted among all group and how much will be left over 
      if (prof_pct_mode == PCT_MODE_PCT)
      {    
        if (mode == TC_FIX_CONCURRENT_USERS)
        {
          pct = (pct * local_num_users) / 100;
          total_local_num_users_sessions += (int)pct;
          num_grp_count++;
          remaining_local_num_users_sessions = local_num_users - total_local_num_users_sessions;
          remaining_local_num_users_sessions_leftover = remaining_local_num_users_sessions % num_grp_count;
        }
        return 0;
      }

      // In fsr mode pct must be multiplied by session rate multiplier to get proper distrution
      // till decimal poins
      if (mode == TC_FIX_USER_RATE)
        pct *= SESSION_RATE_MULTIPLIER; 
     
      // Distribute the users or session rate among all location as per coniguration
      remaining_count = (int)pct;
      NIDL(3, "Overall count = %d", remaining_count);
      for (int i = 0; i < total_gen_location_entries; i++) 
      {
        per_location_count = (int )(pct * g_gen_location_table[i].pct)/100;
        remaining_count -= per_location_count;
        g_gen_location_table[i].numUserOrSession += per_location_count;
      }
      NIDL(3, "Remaining count = %d", remaining_count);
      while (remaining_count > 0)
      { 
        g_gen_location_table[next_location++].numUserOrSession++;
        next_location %= total_gen_location_entries;
        remaining_count--;
      }
      return 0;  
    }
  } 
  else //Comma seperated generator name list 
  { 
    int total_fields; //Total number of generator names
    int i,j, error_flag = 0;
    double pct_value; 
    char *field[MAX_GENERATORS], *pct_ptr = NULL;
    
    NIDL(2, "Generator name = %s, max_used_generator_entries = %d", generator_name, max_used_generator_entries);  
    
    //Tokenize generator name list
    total_fields = get_tokens_(generator_name, field, ",", MAX_GENERATORS);
    NIDL(3, "total_fields = %d", total_fields);

    //Size of struct = total number of fields * max used generator entries * sizeof(gen_name_quantity_list)
    //gen_list = (gen_name_quantity_list *)malloc((max_used_generator_entries * total_fields) *  sizeof(gen_name_quantity_list));
    NSLB_MALLOC(gen_list, ((max_used_generator_entries * total_fields) * sizeof(gen_name_quantity_list)), "gen name quantity list", -1, NULL);

    //Check whether generator name exists in used generator list
    for (j = 0; j < total_fields; j++) {
      for (i = 0; i < max_used_generator_entries; i++) {
        NIDL(3, "field[%d] = %s, generator_entry[%d].gen_name = %s", j, field[j], i, generator_entry[i].gen_name);
        //Check whether generator name field has distribution percentage with colon separation
           
        if((pct_ptr = index (field[j], ':')) != NULL)
        {
          /*CASE: validation if generator provided without pct, 
            SGRP G1 gen1,gen2:80,gen3:20 NA Internet 0 Tours 100*/ 
          if((j != 0) && (pct_flag_set == 0))
          { 
            NS_KW_PARSING_ERR(buf, 0, err_msg, SGRP_USAGE, CAV_ERR_1011329, sg_name);
            //NS_EXIT(-1, "In group %s one or more generators are provided without distribution percentage",
                          // sg_name);
          } 
          /*Find percentage and save in var*/
          pct_ptr++; 
          pct_value = atof(pct_ptr) * 100;
          pct_flag_set = 1;/*Set flag*/
          error_flag = 1;/*Flag required for validation if generator provided without pct...SGRP G1 gen1:10,gen2:80,gen3 NA Internet 0 Tours 100*/
          /*Put NULL at : inorder to get generator name*/
          pct_ptr--;
          *pct_ptr = '\0';
          NIDL(3, "pct_value = %f", pct_value);
        }  
          
        if (!strcmp(field[j], (char *)generator_entry[i].gen_name)) {
          NIDL(3, "Generator name found in used list at index = %d", i);
          add_generator_wrt_controller_type(generator_entry[i].gen_type)
          strcpy(gen_list[total_generator_used_in_grp].generator_name, field[j]);
          gen_list[total_generator_used_in_grp].gen_id = i;
          RESOLVE_GEN_IP() 
          /*In case flag is set then save pct value in gen_list*/
          if (pct_flag_set == 1) 
          {
            if (error_flag) 
            {
              gen_list[total_generator_used_in_grp].pct_per_gen = pct_value;
              NIDL(3, "Percentage provided for gen = %f", gen_list[total_generator_used_in_grp].pct_per_gen);
              error_flag = 0;//Next field should have percentage
            } else { 
                NS_KW_PARSING_ERR(buf, 0, err_msg, SGRP_USAGE, CAV_ERR_1011329, sg_name);
             // NS_EXIT(-1, "In group %s one or more generators are provided without distribution percentage",
             // sg_name);
            }
          }
          total_generator_used_in_grp++;
          if (generator_entry[i].used_gen_flag != 2) {
            generator_entry[i].used_gen_flag = 2;
          } 
          break;
        }//Error case generator name does not exist in used generator list
        if (i == (max_used_generator_entries - 1)) {
          //fprintf(stderr, "\nInvalid generator name %s. Hence exiting...\n", field[j]);
          NS_KW_PARSING_ERR(buf, 0, err_msg, SGRP_USAGE, CAV_ERR_1011307, field[j]);
         // NS_EXIT(-1, "Generator name %s does not exist in used generator list. Please add "
           //           "this generator in used generator list and re-run the test", field[j]);
        }
      }
      //Dump generator list   
      for (i = 0; i < total_generator_used_in_grp; i++)
        NIDL(4, "gen_list[%d].generator_name = %s, gen_list[%d].gen_id = %d", i, gen_list[i].generator_name, i, gen_list[i].gen_id);
    }
  }  

  //Sort generator entries as per SGRP 
  qsort(generator_entry, total_generator_entries, sizeof(struct GeneratorEntry), sort_generator_list);

  //Create SGRP entry
  create_table_for_sgrp_keyword (scen_type, sg_name, uprof_name, script_or_url, sess_or_url_name, pct, cluster_id, total_generator_used_in_grp, sg_fields, pct_flag_set, buf, err_msg);
  total_used_sgrp_entries++;

  return SUCCESS_EXIT;
}

//PROF_PCT_MODE <NUM or PCT or NUM_AUTO>
/*static void usage_kw_set_prof_pct_mode(char *err_msg)
{
  NIDL(1, "Method called, err_msg = %s", err_msg);
  fprintf(stderr, "Error: Invalid value of PROF_PCT_MODE: %s\n", err_msg);
  fprintf(stderr, "  Usage: PROF_PCT_MODE <NUM or PCT or NUM_AUTO>\n");
  NS_EXIT(-1, "Invalid value of PROF_PCT_MODE: %s", err_msg);
}*/

//Validate PROF_PCT_MODE keyword
static int kw_set_prof_pct_mode(char *buf, char *err_msg)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char value[MAX_DATA_LINE_LENGTH];
  int num;

  if((num = sscanf(buf, "%s %s", keyword, value)) != 2)
  NS_KW_PARSING_ERR(buf, 0, err_msg, PROF_PCT_MODE_USAGE, CAV_ERR_1011169, CAV_ERR_MSG_1);
   // usage_kw_set_prof_pct_mode("Atleast one field required for keyword: PROF_PCT_MODE"); 

  if (strcmp(value, "NUM") == 0) {
    prof_pct_mode = PCT_MODE_NUM;
  } else if (strcmp(value, "PCT") == 0) {
    /* Validations */
    if (mode == TC_MIXED_MODE) {
       NS_KW_PARSING_ERR(buf, 0, err_msg,PROF_PCT_MODE_USAGE , CAV_ERR_1011169, "For Mixed mode scenario, pct mode is not allowed");
      //usage_kw_set_prof_pct_mode("For Mixed mode scenarios, pct mode is not allowed.");
    } else if ((schedule_type == SCHEDULE_TYPE_ADVANCED) &&
               (schedule_by == SCHEDULE_BY_GROUP)) {
      NS_KW_PARSING_ERR(buf, 0, err_msg, PROF_PCT_MODE_USAGE , CAV_ERR_1011169, "In case of Schedule Type 'Advanced' and Schedule By 'Group' based scenario, pct mode is not allowed");
     // usage_kw_set_prof_pct_mode("For Advanced and Group based schedule, PCT mode is not allowed");
    } 
    prof_pct_mode = PCT_MODE_PCT;
  } else if (strcmp(value, "NUM_AUTO") == 0) {
      prof_pct_mode = PCT_MODE_NUM_AUTO;
  } else
     NS_KW_PARSING_ERR(buf, 0, err_msg, PROF_PCT_MODE_USAGE, CAV_ERR_1011169, CAV_ERR_MSG_3);
     //usage_kw_set_prof_pct_mode("Invalid argument");
  return 0; 
}

static int kw_set_stype(char *buf,char *err_msg)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char scen_mode[MAX_DATA_LINE_LENGTH];
  int num;
  NIDL(4, "Method called, buf =%s", buf);

  if ((num = sscanf(buf, "%s %s", keyword, scen_mode)) != 2) {
    NS_KW_PARSING_ERR(buf, 0, err_msg, STYPE_USAGE, CAV_ERR_1011172, CAV_ERR_MSG_1);
    //NS_EXIT(-1, "Atleast one field required for keyword: STYPE");
  }

  if (!strcmp(scen_mode, "FIX_CONCURRENT_USERS")) {
    mode = TC_FIX_CONCURRENT_USERS;
    prof_pct_mode = PCT_MODE_NUM; /* This gets overridden when keyword is defined later */
  } else if (!strcmp(scen_mode, "FIX_SESSION_RATE")) {
    mode = TC_FIX_USER_RATE;
    prof_pct_mode = PCT_MODE_PCT; /* This gets overridden when keyword is defined later */
  } else if (!strcmp(scen_mode, "MIXED_MODE")) {  
    mode = TC_MIXED_MODE;
/*  else if (!strcmp(mode, "REPLAY_ACCESS_LOGS"))
  {
    testCase.mode = TC_FIX_USER_RATE;
    global_settings->replay_mode = 1;
    global_settings->use_prof_pct = PCT_MODE_PCT; // This gets overridden when keyword is defined later 
  }
  else if (!strcmp(mode, "FIX_HIT_RATE")) {
    testCase.mode = TC_FIX_HIT_RATE;
    global_settings->use_prof_pct = PCT_MODE_PCT;
  } else if (!strcmp(mode, "FIX_PAGE_RATE")) {
    testCase.mode = TC_FIX_PAGE_RATE;
    global_settings->use_prof_pct = PCT_MODE_PCT;
  } else if (!strcmp(mode, "FIX_TX_RATE")) {
    testCase.mode = TC_FIX_TX_RATE;
    global_settings->use_prof_pct = PCT_MODE_PCT;
  } else if (!strcmp(mode, "MEET_SLA")) {
    testCase.mode = TC_MEET_SLA;
    global_settings->use_prof_pct = PCT_MODE_PCT;
  } else if (!strcmp(mode, "MEET_SERVER_LOAD")) {    
    testCase.mode = TC_MEET_SERVER_LOAD;
    global_settings->use_prof_pct = PCT_MODE_PCT;
  } else if (!strcmp(mode, "FIX_MEAN_USERS")) {
    testCase.mode = TC_FIX_MEAN_USERS;
    global_settings->use_prof_pct = PCT_MODE_PCT;
  */
  } else {
          NS_KW_PARSING_ERR(buf, 0, err_msg, STYPE_USAGE, CAV_ERR_1011241, scen_mode);
        // NS_EXIT(-1, "Unknown scenario type provided (%s)", scen_mode);
  }
  return 0;
}

static int kw_set_target_rate(char *buf, char *err_msg)
{
  float time_min;
  char keyword[MAX_DATA_LINE_LENGTH];
  char option = 'M';
  int num;

  NIDL(2, "Method called, buf = %s", buf);

  if ((num = sscanf(buf, "%s %f %c", keyword, &time_min, &option)) < 2)
  {
    NS_KW_PARSING_ERR(buf, 0, err_msg, TARGET_RATE_USAGE, CAV_ERR_1011173, CAV_ERR_MSG_1);
    //NS_EXIT(-1, "Invalid format of keyword: %s", keyword);
  }
  
  if (mode == TC_FIX_USER_RATE) {
    if (prof_pct_mode == PCT_MODE_NUM) {
      NIDL(2, "TARGET_RATE ignored. in NUM mode");
    } else if (prof_pct_mode == PCT_MODE_PCT && schedule_type == SCHEDULE_TYPE_ADVANCED) {
      NIDL(2, "Ignoring TARGET_RATE in SCHEDULE_TYPE_ADVANCED and PCT mode schedule.");
    } else {
      total_sessions = (int)(time_min * SESSION_RATE_MULTIPLIER);    
      vuser_rpm = total_sessions;
      NIDL(2, "Total session user hit = %d", total_sessions);
    }
  }  
  return 0;
}

static void kw_netcloud_mode(char *buf, char *err_msg)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH];
  char nc_mode[MAX_DATA_LINE_LENGTH];
  int num, mode;
  mode = 0;

  NIDL(2, "Method called, buf = %s", buf);

  if ((num = sscanf(buf, "%s %s %s", keyword, nc_mode, tmp)) < 2)
  {
   
   NS_EXIT(-1, "Invalid  format of the keyword %s", keyword);
  }

  mode = atoi(nc_mode);
  if ((mode < 0) || (mode > 2))
  {
   
    NS_EXIT(-1, "Invalid value of netcloud mode");
  }
  netcloud_mode = mode;
}

static int validate_file_name (char *file_name, char *buf, char *err_msg) 
{ 
  int total_flds;
  char *field[20];
  char validate_file[FILE_PATH_SIZE]; 
  struct stat sb;

  NIDL (1, "Method called, file_name = %s", file_name);
  //Tokenize file name  
  total_flds = get_tokens_(file_name, field, "/", 20);
  NIDL (3, "Tokenize, total_flds = %d", total_flds);
  //If only file name provided then scenario_settings_profile file resides in current scenario's project-subproject
  if (total_flds == 1) 
  {
    NIDL (1, "Scenario setting profile file path not provided, hence using current scenario path [%s]", scen_proj_subproj_dir);
    sprintf(scenario_settings_profile_file, "%s/%s/scenario_profiles/%s", work_dir, GET_NS_TA_DIR(), scen_proj_subproj_dir, field[0]);
  }
  //Else file name given with project subproject
  else if (total_flds == 3) 
  {
    NIDL (1, "Setting scenario setting profile file path %s/%s/%s", field[0], field[1], field[2]); 
    sprintf(scenario_settings_profile_file, "%s/%s/%s/scenario_profiles/%s", GET_NS_TA_DIR(), field[0], field[1], field[2]);
  } 
  else //Invalid file format
  {
   NS_KW_PARSING_ERR(buf, 0, err_msg, SCENARIO_SETTING_PROFILE_USAGE, CAV_ERR_1011177, CAV_ERR_MSG_3);
   // NS_EXIT(-1, "Invalid format for scenario settings profile file %s", buf);
  }
  //Validate whether file exists 
  NIDL (1, "Verify file =%s", scenario_settings_profile_file);
  if (stat(scenario_settings_profile_file, &sb) == 0) {
    NIDL (1, "File exists at path =%s", scenario_settings_profile_file);
    scenario_settings_profile_file_flag = 1;
  } else {
   NS_KW_PARSING_ERR(buf, 0, err_msg, SCENARIO_SETTING_PROFILE_USAGE, CAV_ERR_1011174, validate_file);
   // NS_EXIT(-1, "Error: File does not exists SCENARIO_SETTINGS_PROFILE %s", validate_file);
  }
  return 0;
}

static int kw_scenario_settings_profile(char *buf, char *err_msg)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH];
  char file_name[MAX_DATA_LINE_LENGTH];
  int num;

  NIDL(2, "Method called, buf = %s", buf);

  if ((num = sscanf(buf, "%s %s %s", keyword, file_name, tmp)) != 2)
  {
   NS_KW_PARSING_ERR(buf, 0, err_msg, SCENARIO_SETTING_PROFILE_USAGE, CAV_ERR_1011177, CAV_ERR_MSG_1);
   // NS_EXIT(-1, "Invalid number of arguments for %s", keyword);
  }
  validate_file_name(file_name, buf, err_msg);
  return 0;
}

static void read_kw_rtc(char *rtc_kw, char *err_msg)
{
  int num;
  char text[MAX_DATA_LINE_LENGTH];
  char keyword[MAX_DATA_LINE_LENGTH];
  NIDL(2, "Method called, rtc_kw = %s", rtc_kw);
  if ((num = sscanf(rtc_kw, "%s %s", keyword, text)) < 2) {
    fprintf(stderr, "\nread_kw_rtc(): At least two fields required  <%s>\n", rtc_kw);
    return;
  } else if (strcasecmp(keyword, "SCHEDULE") == 0) {
    kw_schedule(rtc_kw, 1, err_msg);
    scheduling_flag_enable = 1;
  }
}

static void kw_cont_on_file_param_dis_err (char *buf, char *err_msg)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH];
  char cont_on_err[MAX_DATA_LINE_LENGTH];
  int num, mode;
  mode = STOP_TEST;

  NIDL(2, "Method called, buf = %s", buf);

  if ((num = sscanf(buf, "%s %s %s", keyword, cont_on_err, tmp)) < 2)
  {
    NS_EXIT(-1, "Invalid format of the keyword %s", keyword);
  }

  mode = atoi(cont_on_err);
  if ((mode < 0) || (mode > 1))
  {
    NS_EXIT(-1, "Invalid value for keyword CONTINUE_ON_FILE_PARAM_DISTRIBUTION_ERR");
  }
  continue_on_file_param_dis_err = mode;
}

void remove_tmp_file(char *file)
{
  NIDL(2, "Method called, file to remove is %s", file);
  unlink(file);
}

void set_work_dir()
{
  if (getenv("NS_WDIR") != NULL) {
    strcpy(work_dir, getenv("NS_WDIR"));
  } else {
    NS_DUMP_WARNING("NS_WDIR env variable is not set. Setting it to default value /home/cavisson/work/");
    strcpy(work_dir, "/home/cavisson/work");
  }
}

static void kw_nc_dependency(char *buf, char *err_msg)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char tmp[MAX_DATA_LINE_LENGTH];
  char additional_files_or_dir[1024] = {0};
  int num;

  NIDL(2, "Method called, buf = %s", buf);

  num = sscanf(buf, "%s %s %s", keyword, additional_files_or_dir, tmp);

  if(num < 2 || num >= 3){
    NS_EXIT(-1, "Insufficient number of arguments passed. Usage like: NC_DEPENDENCY <file name/directory with full path>");
  }
  if((additional_files_or_dir[0] == '\0') || (additional_files_or_dir[0] != (char) 47))
  {
    NS_EXIT(-1, "Invalid path [%s] for Keyword NC_DEPENDENCY. Usage: NC_DEPENDENCY <file name/directory with absolute path>",
                additional_files_or_dir);
  }
  NIDL(2, "additional_files_or_dir = %s", additional_files_or_dir);
  //make_nc_dependency_directories(additional_files_or_dir);
  //make tar for NC_DEPENDENCY keyword
  //make_tar_for_nc_dependency(additional_files_or_dir);
}

static int kw_enable_fcs_settings(char *buf, char *err_msg)
{
  char keyword[MAX_DATA_LINE_LENGTH] = {0};
  char mode_str[32] = {0};
  char limit_str[32] = {0};
  char queue_size_ptr[32] = {0};
  char tmp[MAX_DATA_LINE_LENGTH] = {0};
  int mode = 0;
  int limit = 8;
  int queue_size = 0;
  int num;
 
  num = sscanf(buf, "%s %s %s %s %s", keyword, mode_str, limit_str, queue_size_ptr, tmp);

  NIDL(1, "Method called, buf = %s, args = %d, mode_str = %s, limit_str = %s, queue_size = %s", buf, num, mode_str, limit_str, queue_size_ptr);
  if(num < 2 || num > 4)
  {
      NS_KW_PARSING_ERR(buf, 0, err_msg, ENABLE_FCS_SETTING_USAGE, CAV_ERR_1011178, CAV_ERR_MSG_1);
   // NS_EXIT(-1, "[ENABLE_FCS_SETTINGS]: Insufficient number of arguments, USAGE: ENABLE_FCS_SETTINGS <mode> <session limit> <queue size>");
  }

  if (ns_is_numeric(mode_str) == 0) {
      NS_KW_PARSING_ERR(buf, 0, err_msg, ENABLE_FCS_SETTING_USAGE, CAV_ERR_1011178, CAV_ERR_MSG_2);
   // NS_EXIT(-1, "ENABLE_FCS_SETTINGS mode is not numeric");
  }

  mode = atoi(mode_str);
  if (mode < 0 || mode > 1)
  {
      NS_KW_PARSING_ERR(buf, 0, err_msg, ENABLE_FCS_SETTING_USAGE, CAV_ERR_1011178, CAV_ERR_MSG_3);
    //NS_EXIT(-1, "ENABLE_FCS_SETTINGS mode is not valid");
  }

  if(limit_str[0] != '\0')
  {
    if (ns_is_numeric(limit_str) == 0) {
        NS_KW_PARSING_ERR(buf, 0, err_msg, ENABLE_FCS_SETTING_USAGE, CAV_ERR_1011178, CAV_ERR_MSG_2);
      //NS_EXIT(-1, "ENABLE_FCS_SETTINGS concurrent sessions limit is not numeric");
    }
 
    limit = atoi(limit_str);
    if (limit < 0)
    {
      NS_KW_PARSING_ERR(buf, 0, err_msg, ENABLE_FCS_SETTING_USAGE, CAV_ERR_1011178, "Limit can not be less than zero");
     // NS_EXIT(-1, "ENABLE_FCS_SETTINGS limit is not valid");
    }
  }
  
  if(queue_size_ptr[0] != '\0')
  {
    if (ns_is_numeric(queue_size_ptr) == 0) {
       NS_KW_PARSING_ERR(buf, 0, err_msg, ENABLE_FCS_SETTING_USAGE, CAV_ERR_1011178, CAV_ERR_MSG_2);
      //NS_EXIT(-1, "ENABLE_FCS_SETTINGS concurrent sessions queue size is not numeric");
    }
 
    queue_size = atoi(queue_size_ptr);
    if (queue_size < 0)
    {
      NS_KW_PARSING_ERR(buf, 0, err_msg, ENABLE_FCS_SETTING_USAGE, CAV_ERR_1011178, "Queue size can not be less than zero");
      //NS_EXIT(-1, "ENABLE_FCS_SETTINGS queue size is not valid");
    }
  }
  enable_fcs_settings_mode = mode;
  enable_fcs_settings_limit = limit;
  enable_fcs_settings_queue_size = queue_size;
  NIDL(1, "Method exit(), enable_fcs_settings_mode = %d, enable_fcs_settings_limit = %d, enable_fcs_settings_queue_size = %d",
           enable_fcs_settings_mode, enable_fcs_settings_limit, enable_fcs_settings_queue_size);
  return 0;
}

/* Parsing of Keyword PROGRESS_MSECS
 * Making default progress msec for individual products
 * NC = 30s, NDE = 60s, NVSM = 60s  
 * Setting above default values if keyword PROGRESS_MSEC not specified
 */

static int kw_set_progress_msecs(char* text, char *buf, char *err_msg)
{
  if(ns_is_numeric(text) == 0)
  {
      NS_KW_PARSING_ERR(buf, 0, err_msg, PROGRESS_MSECS_USAGE, CAV_ERR_1011021, CAV_ERR_MSG_2);
    //NS_EXIT(-1, "Error: PROGRESS_MSECS - Value is not numeric");
  }
  progress_msecs = atoi(text);
  if ( progress_msecs == 0 ) {
    NIDL(1, "Progress report time interval is %d.\nSetting PROGRESS_MSECS According to Product.\n", progress_msecs);
    if (!strcmp (g_cavinfo.config, "NDE")) {
      if (!strcmp (g_cavinfo.SUB_CONFIG, "NVSM"))
        progress_msecs = PROGRESS_INTERVAL_NDE_NVSM;
      else
        progress_msecs = PROGRESS_INTERVAL_NDE;
    } else {
      progress_msecs = PROGRESS_INTERVAL_NC;
    }
    NIDL(1, "Progress report time interval is %d According to Product type.\n", progress_msecs);
  }
  return 0;
}

static int kw_check_generator_health(char *buf, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char mode_str[32 + 1] = {0};
  char disk_str[32 + 1] = {0};
  char cpu_str[32 + 1] = {0};
  char mem_str[32 + 1] = {0};
  char bw_str[32 + 1] = {0};
  char tmp[MAX_DATA_LINE_LENGTH]; //This used to check if some extra field is given
  int num;
  int mode = 0;
  int diskAvail = 0;
  int CPUAvail = 0;
  int memAvail = 0;
  int BWAvail = 0;

  num = sscanf(buf, "%s %s %s %s %s %s %s", keyword, mode_str, disk_str, cpu_str, mem_str, bw_str, tmp);

  NSDL2_PARSING(NULL, NULL, "Method called, buf = %s, num = %d, key=[%s], mode=[%s], disk=[%s], cpu=[%s], mem=[%s], BW=[%s]",
                             buf, num, keyword, mode_str, disk_str, cpu_str, mem_str, bw_str);

  if(num < 2 || num > 6){
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, CHECK_GENERATOR_HEALTH_USAGE, CAV_ERR_1011247, CAV_ERR_MSG_1);
  }

  if(ns_is_numeric(mode_str) == 0){
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, CHECK_GENERATOR_HEALTH_USAGE, CAV_ERR_1011247, CAV_ERR_MSG_2);
  }

  mode = atoi(mode_str);
  if(mode < 0 || mode > 1){
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, CHECK_GENERATOR_HEALTH_USAGE, CAV_ERR_1011247, CAV_ERR_MSG_3);
  }

  checkgenhealth.check_generator_health = mode;

  //Bug 42407: If mode is disabled then don't parse remaining arguments
  if(checkgenhealth.check_generator_health)
  {
    if(disk_str[0] != '\0')
    {
      if(ns_is_numeric(disk_str) == 0){
         NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, CHECK_GENERATOR_HEALTH_USAGE, CAV_ERR_1011247, "minDiskAvailability is not numeric");
      } 
  
      diskAvail = atoi(disk_str);
      if(diskAvail < 0 || diskAvail > 1024){
        NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, CHECK_GENERATOR_HEALTH_USAGE, CAV_ERR_1011247, 
                          "minDiskAvailability is not valid. It should be between [0-1024]");
      }

      checkgenhealth.minDiskAvailability = diskAvail;
      if(!checkgenhealth.minDiskAvailability)
        NS_DUMP_WARNING("[Check Generator Health] Disk check is disabled, test may be affected");
    }

    if(cpu_str[0] != '\0')
    {
      if(ns_is_numeric(cpu_str) == 0){
          NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, CHECK_GENERATOR_HEALTH_USAGE, CAV_ERR_1011247, "minCpuAvailability is not numeric");         }

      CPUAvail = atoi(cpu_str);
      if(CPUAvail < 0){
        NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, CHECK_GENERATOR_HEALTH_USAGE, CAV_ERR_1011247, 
                          "minCpuAvailability is not valid. It should be between [0-INT_MAX]");
      }

      checkgenhealth.minCpuAvailability = CPUAvail;
      if(!checkgenhealth.minCpuAvailability)
        NS_DUMP_WARNING("[Check Generator Health] CPU check is disabled, test may be affected");
    }

    if(mem_str[0] != '\0')
    {
      if(ns_is_numeric(mem_str) == 0){
        NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, CHECK_GENERATOR_HEALTH_USAGE, CAV_ERR_1011247, "minMemAvailability is not numeric");
      }

      memAvail = atoi(mem_str);
      if(memAvail < 0 || memAvail > 1024){
        NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, CHECK_GENERATOR_HEALTH_USAGE, CAV_ERR_1011247, 
                          "minMemAvailability is not valid. It should be between [0-1024]");
      }
 
      checkgenhealth.minMemAvailability = memAvail;
      if(!checkgenhealth.minMemAvailability)
        NS_DUMP_WARNING("[Check Generator Health] Memory check is disabled, test may be affected");
    }

    if(bw_str[0] != '\0')
    {
      if(ns_is_numeric(bw_str) == 0){
        NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, CHECK_GENERATOR_HEALTH_USAGE, CAV_ERR_1011247, "minBandwidthAvailability is not numeric");
      }
 
      BWAvail = atoi(bw_str);
      if(BWAvail < 0){
        NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, CHECK_GENERATOR_HEALTH_USAGE, CAV_ERR_1011247, 
                          "minBandwidthAvailability is not valid. It should be between [0-1024]");
      } 
      checkgenhealth.minBandwidthAvailability = BWAvail;
    }
  }
  else
  {
    NS_DUMP_WARNING("[Check Generator Health] Mode is disabled, this may affect health of running test. Checking cmon connectivity, build version match and previous test running on generator will be ignored");
  }
  NIDL(3, "Method exit, mode = %c, disk = %d, cpu = %d, mem = %d, BW = %d", checkgenhealth.check_generator_health,
                    checkgenhealth.minDiskAvailability, checkgenhealth.minCpuAvailability, checkgenhealth.minMemAvailability,
                    checkgenhealth.minBandwidthAvailability);
  return 0;
}

static void kw_set_cvm_based_load_distribution(char* buf, char *err_msg) 
{
  char keyword[MAX_DATA_LINE_LENGTH] = "";
  char distribution_mode[MAX_DATA_LINE_LENGTH] = "";
  int num, d_mode;

  NIDL(2, "Method called, buf = %s", buf);

  if ((num = sscanf(buf, "%s %s", keyword, distribution_mode)) < 2)
  {
    NS_EXIT(-1, "Invalid  format of the keyword %s", keyword);
  }

  nslb_atoi(distribution_mode, &d_mode);
  if ((d_mode < 0) || (d_mode > 2))
  {
    NS_EXIT(-1, "Invalid value of netcloud mode");
  }
  g_per_cvm_distribution = d_mode;
  if(mode == TC_MIXED_MODE || prof_pct_mode == PCT_MODE_PCT)
    g_per_cvm_distribution = 0; 
}

/* 
 * Read keywords related scenario distribution:
 * NS_GENERATOR_FILE
 * NS_GENERATOR
 * Netstorm keywords:
 * SGRP
 * PROF_PCT_MODE
 * NUM_USER
 * */
static int read_scen_keywords(int default_gen_file, char *err_msg)
{
  int num;
  FILE *fp;
  char buf[MAX_CONF_LINE_LENGTH+1];
  char text[MAX_DATA_LINE_LENGTH];
  char keyword[MAX_DATA_LINE_LENGTH];
  int  line_num = 0;
  int sg_fields = 5;
  NIDL(2, "Method called");
  
  if ((fp = fopen(new_conf_file, "r")) == NULL) {
    //NS_EXIT(-1, "Failed to open scenario file %s, error = %s", new_conf_file, nslb_strerror(errno));
    NS_EXIT(-1, CAV_ERR_1000006, new_conf_file, errno, nslb_strerror(errno));
  }

  if (default_gen_file) {
    kw_set_ns_generator_file(NULL, err_msg, 0);
  } 
  while (fgets(buf, MAX_CONF_LINE_LENGTH, fp) != NULL)
  {
    line_num++;
    NIDL(2, "Buffer = %s", buf);

    if ((num = sscanf(buf, "%s %s", keyword, text)) < 2) {
      fprintf(stderr, "\nread_scen_keywords(): At least two fields required  <%s>\n", buf);
      continue;
    } else if (strcasecmp(keyword, "NS_GENERATOR_FILE") == 0) {
      kw_set_ns_generator_file(buf, err_msg, 1);
    } else if (strcasecmp(keyword, "NS_GENERATOR") == 0) {
      kw_set_ns_generator_list(buf, err_msg);
    } else if (strcasecmp(keyword, "NUM_USERS") == 0) {
      local_num_users = atoi(text); //Fill local_num_users variable with the value mention in keyword.
    } else if (strcasecmp(keyword, "PROF_PCT_MODE") == 0) {
      kw_set_prof_pct_mode(buf, err_msg);
    } else if (!strcasecmp(buf, "CLUSTER_VARS")) {
      sg_fields = 6;
    } else if (!strcasecmp(buf, "USE_CLUSTERS")) {
      if (atoi(text) == 1)
      {
        cluster_field_enable = 1; 
        sg_fields = 6; //increment for cluster_id
      }
    } else if (strcasecmp(keyword, "SGRP") == 0) {
      kw_set_sgrp(buf, err_msg, sg_fields);
    } else if (strcasecmp(keyword, "STYPE") == 0) {
      kw_set_stype(buf, err_msg);
    } else if (strcasecmp(keyword, "TARGET_RATE") == 0) {
      kw_set_target_rate(buf, err_msg);
    } /*SCHEDULE Keywords*/
      else if (strcasecmp(keyword, "SCHEDULE") == 0) {
      kw_schedule(buf, 0, err_msg);
    } else if (strcasecmp(keyword, "SCHEDULE_TYPE") == 0) {
      kw_schedule_type(buf,err_msg);
    } else if (strcasecmp(keyword, "SCHEDULE_BY") == 0) {
      kw_schedule_by(buf, err_msg);
    } else if (strcasecmp(keyword, "NETCLOUD_MODE") == 0) {
      kw_netcloud_mode(buf, err_msg);
    } else if (strcasecmp(keyword, "SCENARIO_SETTINGS_PROFILE") == 0) {
      kw_scenario_settings_profile(buf, err_msg);
    } else if (strcasecmp(keyword, "CONTINUE_ON_FILE_PARAM_DISTRIBUTION_ERR") == 0) {
      kw_cont_on_file_param_dis_err(buf, err_msg);
    } else if(strcasecmp(keyword, "NC_DEPENDENCY") == 0 && (g_do_not_ship_test_assets == SHIP_TEST_ASSETS)) {   
      kw_nc_dependency(buf,err_msg);
    } else if(strcasecmp(keyword, "ENABLE_FCS_SETTINGS") == 0) {
      kw_enable_fcs_settings(buf, err_msg);
    } else if(strcasecmp(keyword, "PROGRESS_MSECS") == 0) {
      kw_set_progress_msecs(text, buf, err_msg);
    } else if (strcasecmp(keyword, "CHECK_GENERATOR_HEALTH") == 0) {
      kw_check_generator_health(buf, err_msg, 0);
      check_generator_health();
    } else if (strcasecmp(keyword, "CONTINUE_TEST_ON_GEN_FAILURE") == 0) {
      kw_set_ns_gen_fail(buf, err_msg, 0);
    } else if (strcasecmp(keyword, "ENABLE_ALERT") == 0) {
      kw_set_enable_alert(buf, err_msg, 0);
    } else if (strcasecmp(keyword, "CVM_BASED_LOAD_DISTRIBUTION") == 0) {
      kw_set_cvm_based_load_distribution(buf, err_msg);
    } else if (strcasecmp(keyword, "NUM_NVM") == 0) {
      kw_set_num_nvm(buf, global_settings, 0, err_msg);
    } else if (strcasecmp(keyword, "G_DATADIR") == 0) {
      kw_set_datadir(buf, 0, err_msg);
    } else if (strcasecmp(keyword, "JMETER_VUSERS_SPLIT") == 0) {
      kw_set_jmeter_vusers_split(buf, err_msg);
    } else if (strcasecmp(keyword, "JMETER_CSV_DATA_SET_SPLIT") == 0) {
      kw_set_jmeter_csv_files_split_mode(buf, err_msg);
    } 
  }
  return 0;
}


static int verify_ext_gen_list_in_nc_mode_2()
{
  int i, external_gen_found, internal_gen_found;
  external_gen_found = 0; //external generator not found
  NIDL (1, "Method called, total_used_generator_list_entries = %d", total_used_generator_list_entries);
  if(netcloud_mode == 2)
  {
    if(!strcmp(controller_type, "Internal"))
    {
      for(i = 0; i < total_generator_entries; i++)
      {
        if((generator_entry[i].used_gen_flag == 2) && (!strcasecmp(generator_entry[i].gen_type, "External"))) {
          NIDL (3, "External generator define, hence setting flag");
          external_gen_found = 1;
        }
        if((generator_entry[i].used_gen_flag == 2) && (!strcasecmp(generator_entry[i].gen_type, "Internal"))) {
          NIDL (3, "Internal generator define, hence setting flag");
          internal_gen_found = 1;
        }
      }
    }
    //No external generator define, hence terminating test
    if(!external_gen_found)
    {
      NIDL (3, "Error: In case of netcloud mode 2, external generatos are not defined, please select external generators in scenario group. Hence exiting...");
      NS_EXIT(-1, CAV_ERR_1014021);
    }
    //No internal generator define, hence terminating test
    if(!internal_gen_found)
    {
      NIDL (3, "Error: In case of netcloud mode 2, internal generatos are not defined, please select internal generators in scenario group. Hence exiting...");
      NS_EXIT(-1, CAV_ERR_1014022);
    }
  }
  return 0;
}

/*Calculate used generator list*/
static void calculate_used_gen_list()
{
  int j;
  NIDL(2, "Method called, sgrp_used_genrator_entries = %d", sgrp_used_genrator_entries);

  for (j = 0; j < total_generator_entries; j++) 
  {
    if ((generator_entry[j].used_gen_flag == 2) && (!strcasecmp(generator_entry[j].gen_type, "Internal")) && (sgrp_used_genrator_entries == 0))    {
      sgrp_used_genrator_entries ++; 
    }
    else if ((generator_entry[j].used_gen_flag == 2) && (!strcasecmp(generator_entry[j].gen_type, "External")) && (sgrp_used_genrator_entries == 0)) 
    {
      sgrp_used_genrator_entries ++;
    }    
    else {
      NIDL(2, "generator_entry[%d].gen_name = %s", j, generator_entry[j].gen_name);
      break;
    }
  }
  NIDL(2, "sgrp_used_genrator_entries = %d", sgrp_used_genrator_entries);
}


/*Function used to write used generator list in file*/
static void create_used_gen_list_file()
{
  FILE *fp_int, *fp_ext;
  int j;
  int type_internal = 0, type_external = 0; 
  internal_used_gen_file_name[0] = '\0';
  external_used_gen_file_name[0] = '\0';
  NIDL(2, "Method called, sgrp_used_genrator_entries = %d", sgrp_used_genrator_entries);

  //Create used generator list according to generator type
  for (j = 0; j < total_generator_entries; j++)
  {
    if(!type_internal && ((generator_entry[j].used_gen_flag == 2) && (!strcasecmp(generator_entry[j].gen_type, "Internal"))))
    {
      sprintf(internal_used_gen_file_name, "%s/internal_used_generator_list.dat", controller_dir);
      type_internal = 1; //Internal type found
    }

    if(!type_external && ((generator_entry[j].used_gen_flag == 2) && (!strcasecmp(generator_entry[j].gen_type, "External"))))
    {
      sprintf(external_used_gen_file_name, "%s/external_used_generator_list.dat", controller_dir);
      type_external = 1; //External type found
    }
  } 
  //sprintf(used_gen_file_name, "%s/used_generator_list.dat", controller_dir);
  
  if(type_internal) 
  {
    if((fp_int = fopen(internal_used_gen_file_name, "a")) == NULL)
    {
      NS_EXIT(-1, CAV_ERR_1000006, internal_used_gen_file_name, errno, nslb_strerror(errno));
    }
  }

  if(type_external)
  {
    if((fp_ext = fopen(external_used_gen_file_name, "a")) == NULL)
    {
      NS_EXIT(-1, CAV_ERR_1000006, external_used_gen_file_name, errno, nslb_strerror(errno));
    }
  }

  /*Format: GENERATOR_NAME|IP|WORK|PORT*/
  for (j = 0; j < total_generator_entries; j++) 
  {
    if (generator_entry[j].mark_gen == 1)
      continue;
    if (type_internal && (generator_entry[j].used_gen_flag == 2) && (!strcasecmp(generator_entry[j].gen_type, "Internal"))) {
      fprintf(fp_int, "%s|%s|%s|%s|%d|%d\n", generator_entry[j].gen_name, generator_entry[j].IP, generator_entry[j].work, generator_entry[j].agentport, j, generator_entry[j].loc_idx);
    }
    else if (type_external && (generator_entry[j].used_gen_flag == 2) && (!strcasecmp(generator_entry[j].gen_type, "External"))) 
    {
      fprintf(fp_ext, "%s|%s|%s|%s|%d|%d\n", generator_entry[j].gen_name, generator_entry[j].IP, generator_entry[j].work, generator_entry[j].agentport, j, generator_entry[j].loc_idx);
    }    
    else {
      NIDL(2, "generator_entry[%d].gen_name = %s", j, generator_entry[j].gen_name);
      break;
    }
  }

  if (type_internal)
    fclose(fp_int);

  if (type_external)
    fclose(fp_ext);
}

static int create_gen_dir(int j, int k, int flag, int grp_idx)
{
  struct stat sb; //Check whether directory exists
  FILE *fp;
  char create_cmd[MAX_COMMAND_SIZE];
  char generator_dir[FILE_PATH_SIZE];
  char new_scen_file_name[FILE_PATH_SIZE];
  char script_name[MAX_DATA_LINE_LENGTH + 1];

  NIDL(2, "Method called generator_entry index = %d", j);

  sprintf(generator_dir, "%s/%s/rel/%s/scenarios", controller_dir, generator_entry[j].gen_name,
                         scen_proj_subproj_dir);
  sprintf(new_scen_file_name, "%s/%s_%s", generator_dir, generator_entry[j].gen_name, scenario_file_name);
  NIDL(2, "generator_name = %s, new_scen_file_name = %s", generator_dir, new_scen_file_name);

  /* Check whether generator directory exists */
  if (stat(new_scen_file_name, &sb) == -1) {
    NIDL(2, "File Status:%s.", nslb_strerror(errno));
    NIDL(2, "Create directory for generator_entry[%d].generator_name = %s", j, generator_entry[j].gen_name);
    sprintf(create_cmd, "mkdir -p %s", generator_dir);

    if (system(create_cmd) != 0) {
      NS_EXIT(FAILURE_EXIT, "Failed to create scenario directory for generator %s", generator_entry[j].gen_name);
    }
  }
  if ((fp = fopen(new_scen_file_name, "a")) == NULL)
  {
    NS_EXIT(-1, "Error in opening %s file.", new_scen_file_name);
  }
  
  if(scen_grp_entry[k].script_or_url != 1)
  {
    snprintf(script_name, MAX_DATA_LINE_LENGTH, "%s/%s/%s", 
               scen_grp_entry[k].proj_name, scen_grp_entry[k].sub_proj_name, scen_grp_entry[k].sess_name);
  }
  else
  {
    strcpy(script_name, scen_grp_entry[k].sess_name);
  }

  NIDL(2, "script_name = %s, script_or_url = %d ", script_name, scen_grp_entry[k].script_or_url);

  if (flag == 1) {
    /*SGRP <GroupName> <ScenType> <user-profile> <type(0 for Script or 1 for URL)> <session-name/URL> <num-or-pct> <cluster_id> */
    if (scen_grp_entry[grp_idx].grp_type == TC_FIX_USER_RATE) {
      if (schedule_by == SCHEDULE_BY_SCENARIO) 
        fprintf(fp, "SGRP %s %d %s %s %d %s %0.3f %s\n", scen_grp_entry[k].scen_group_name, sgrp_used_genrator_entries, scen_grp_entry[k].scen_type, scen_grp_entry[k].uprof_name, scen_grp_entry[k].script_or_url, script_name, scen_grp_entry[k].percentage/SESSION_RATE_MULTIPLIER, (cluster_field_enable)?scen_grp_entry[k].cluster_id:"");
      else
         fprintf(fp, "SGRP %s %d %s %s %d %s %0.3f %s\n", scen_grp_entry[k].scen_group_name, scen_grp_entry[k].num_generator, scen_grp_entry[k].scen_type, scen_grp_entry[k].uprof_name, scen_grp_entry[k].script_or_url, script_name, scen_grp_entry[k].percentage/SESSION_RATE_MULTIPLIER, (cluster_field_enable)?scen_grp_entry[k].cluster_id:"");
    } else {  
      if (schedule_by == SCHEDULE_BY_SCENARIO)
        fprintf(fp, "SGRP %s %d %s %s %d %s %d %s\n", scen_grp_entry[k].scen_group_name, sgrp_used_genrator_entries, scen_grp_entry[k].scen_type, scen_grp_entry[k].uprof_name, scen_grp_entry[k].script_or_url, script_name, scen_grp_entry[k].quantity, (cluster_field_enable)?scen_grp_entry[k].cluster_id:"");
      else 
        fprintf(fp, "SGRP %s %d %s %s %d %s %d %s\n", scen_grp_entry[k].scen_group_name, scen_grp_entry[k].num_generator, scen_grp_entry[k].scen_type, scen_grp_entry[k].uprof_name, scen_grp_entry[k].script_or_url, script_name,scen_grp_entry[k].quantity, (cluster_field_enable)?scen_grp_entry[k].cluster_id:"");
    }         
  } else {
    /*SGRP <GroupName> <ScenType> <user-profile> <type(0 for Script or 1 for URL)> <session-name/URL> <num-or-pct> <cluster_id> */
    if (schedule_by == SCHEDULE_BY_SCENARIO)
      fprintf(fp, "SGRP %s %d %s %s %d %s %d %s\n", scen_grp_entry[k].scen_group_name, sgrp_used_genrator_entries, scen_grp_entry[k].scen_type, scen_grp_entry[k].uprof_name, scen_grp_entry[k].script_or_url, script_name, 0, (cluster_field_enable)?scen_grp_entry[k].cluster_id:"");
    else 
      fprintf(fp, "SGRP %s %d %s %s %d %s %d %s\n", scen_grp_entry[k].scen_group_name, scen_grp_entry[k].num_generator, scen_grp_entry[k].scen_type, scen_grp_entry[k].uprof_name, scen_grp_entry[k].script_or_url, script_name, 0, (cluster_field_enable)?scen_grp_entry[k].cluster_id:"");
  }  
  fclose(fp);
  return 0;
}

/* Create generator directory and save respective scenario configuration file 
 * To obtain equal set of transactions from all generators, we need to ship 
 * all groups with unique scripts to each generator. Therefore we are passing
 * those SGRP entries with quantity 0. 
 * */

static void create_gen_dir_store_scen_file()
{
  int i,j,k = 0;
  int gen_found_flag = 0; //Generator name not match
  int sg_idx;
 
  NIDL(1, "Method called sgrp_used_genrator_entries = %d, total_sgrp_entries = %d", sgrp_used_genrator_entries, total_sgrp_entries);

  /*Total SGRP groups*/
  while (k < total_sgrp_entries) 
  {
    /*Run loop with total generators uniquely used in SGRP.*/
    for (j = 0; j < sgrp_used_genrator_entries; j++)
    {
      NIDL(2, "scen_grp_entry[%d].num_generator = %d", k, scen_grp_entry[k].num_generator);
      /*Total generators used in a single group*/
      for (i = 0; i < scen_grp_entry[k].num_generator; i++)  
      {
        sg_idx = k + i; /*Scenario group index need to be incremented with respect to number of generator*/ 
        if (!strcmp(scen_grp_entry[sg_idx].generator_name, (char *)generator_entry[j].gen_name))
        {
          NIDL(2, "Generator name match setting flag, scen_grp_entry[%d].generator_name = %s", 
                   sg_idx, scen_grp_entry[sg_idx].generator_name);
          gen_found_flag = 1; //Generator name found
          break;
        } 
        else 
        {
          NIDL(2, "Generator name mismatch,hence reset flag");
          gen_found_flag = 0; //Generator name not found
        }
      }
      if (generator_entry[j].mark_gen == 0) 
      {
        if (gen_found_flag == 0) 
        { 
          /*Add group in generator with quantity 0*/
          NIDL(2, "generator index j = %d, generator count i = %d, group sg_idx =%d", j, i, sg_idx); 
          create_gen_dir(j, sg_idx, gen_found_flag, k); //gen_found_flag send to hard code quantity "0" for that grp
        }
        else 
        {
          /* Create generator directory */
          NIDL(2, "generator index j = %d, generator count i = %d, group sg_idx =%d", j, i, sg_idx); 
          create_gen_dir(j, sg_idx, gen_found_flag, k); //Here gen_found_flag must be 1
        }  
      }
    }
    /* We need to increment for unique group entries
     * Eg. If group idx 0 has 2 generators then next unique entry wud be group idx 2 
     * Go to next group*/
    k = k + scen_grp_entry[k].num_generator;
  }
}

#ifdef TEST
//Remove .tmp/.controller directory 
static void remove_existing_controller_dir()
{
  char create_cmd[MAX_COMMAND_SIZE];
  char controller_dir[FILE_PATH_SIZE];
  struct stat sb;

  sprintf(controller_dir, "%s/.tmp/.controller", work_dir);

  if (stat(controller_dir, &sb) == -1) {
  } else {
    sprintf(create_cmd, "rm -rf %s/.tmp/.controller", work_dir);
    if (system(create_cmd) != 0)
    {
      NS_EXIT(-1, "Error in creating controller directory");
    }
  }
}

//Print usage
static void help(void)
{
  fprintf(stderr, "Usage: ni_scenario_distribution_tool -h [-n scenario file OR <workspace>/<profile>/<proj>/<subproj>/scenario file] [-d debug-level] [-t testrun-number]\n");
  fprintf(stderr, "\t-h\t Prints usage of ni_scenario_distribution_tool\n");
  fprintf(stderr, "\t-n\t scenario file  <workspace>/<profile>/<proj>/<subproj>/scenario file\n");
  fprintf(stderr, "\t-t\t test-run number\n");
  fprintf(stderr, "\t-d\t debug level\n");
  NS_EXIT(-1, "Usage: ni_scenario_distribution_tool -h [-n scenario file] [-d debug-level] [-t testrun-number]");
}
#endif

static void free_malloc_ptr()
{
  NIDL(1, "Method called");

  //Free and make null gen_used_list and scen_grp_entry ptr
  NIDL(3, "Free and make NULL pointer gen_used_list = %p, scen_grp_entry = %p", gen_used_list, scen_grp_entry);
  free(gen_used_list);
  gen_used_list = NULL;
  //free(scen_grp_entry);
  //scen_grp_entry = NULL;

  NIDL(3, "Free and make NULL pointer gen_list = %p", gen_list);
  free(gen_list);
  gen_list = NULL;

}

static void validate_num_user_and_pct_mode()
{
  NIDL(1, "Method called");
  NIDL(4, "prof_pct_mode = %d, num_users = %d", prof_pct_mode, num_users);
  if (mode == TC_FIX_CONCURRENT_USERS && local_num_users > 0) 
  {
    if (prof_pct_mode == PCT_MODE_NUM){
      NIDL(2, "Ignoring NUM_USERS in NUM mode.");
    }
    else if (prof_pct_mode == PCT_MODE_PCT && schedule_type == SCHEDULE_TYPE_ADVANCED){
      NIDL(2, "Ignoring NUM_USERS in SCHEDULE_TYPE_ADVANCED and PCT mode schedule.");
    }
    else {//In valid case fill num_users
      num_users = local_num_users;
      num_connections = num_users;
    }
  }
}

static void fill_generator_id_list()
{
   int k = 0, i, j;
   
   NIDL(1, "Method called");
   /*Total SGRP groups*/
   while (k < total_sgrp_entries)
   {
     // Malloc generator id  list
     NIDL(4, "List of generator id for scenario group %s", scen_grp_entry[k].scen_group_name);
     //scen_grp_entry[k].generator_id_list = (int *)malloc(scen_grp_entry[k].num_generator *  sizeof(int));
     //memset(scen_grp_entry[k].generator_id_list, -1, sizeof(int) * scen_grp_entry[k].num_generator);
     NSLB_MALLOC_AND_MEMSET_WITH_MINUS_ONE(scen_grp_entry[k].generator_id_list, (scen_grp_entry[k].num_generator *  sizeof(int)), "gen id list", k, NULL);
     
     //Find generator id for corresponding generator list
     for (i = 0; i < scen_grp_entry[k].num_generator; i++)
     {
        for (j = 0; j < sgrp_used_genrator_entries; j++)
        {
           if (!strcmp(scen_grp_entry[k].generator_name_list[i], (char *)generator_entry[j].gen_name))
           {
             scen_grp_entry[k].generator_id_list[i] = j;
             NIDL(4, "scen_grp_entry[%d].generator_id_list[%d] = %d", k, i, scen_grp_entry[k].generator_id_list[i]);   
             break;
           }   
        } 
     }
     //Unique group
     k = k + scen_grp_entry[k].num_generator;
   }
}

/*This function is used to find number of groups and group ids going to a particular generator*/
static void find_num_grp_per_gen()
{
  int k = 0, i, j;
  NIDL(1, "Method called");
  /*Total SGRP groups*/
  while(k < total_sgrp_entries)
  {
    for (i = 0; i < scen_grp_entry[k].num_generator; i++)
    {
      for (j = 0; j < sgrp_used_genrator_entries; j++)
      {
         if (!strcmp(scen_grp_entry[k].generator_name_list[i], (char *)generator_entry[j].gen_name))
         {
           NIDL(4, "In group id list at index %d, saved group INDEX %d", generator_entry[j].num_groups, (i + k));
           //Save consequetive group index, 
           //for example Group1 distributed across gen1 and gen2, then in scen_grp_entry table for Group1 we will have two enteries (0 and 1 index)
           generator_entry[j].group_id_list[generator_entry[j].num_groups] = i + k;
           generator_entry[j].num_groups++;//Total number of groups per generator also used for indexing
           NIDL(4, "Total number of groups going for generator %s is %d", generator_entry[j].gen_name, generator_entry[j].num_groups);
           break; 
         }
      }
    }
    k += scen_grp_entry[k].num_generator; 
  }
  //In case of 0 session or user going for one/all groups for particular generator then mark generator  
  int set_flag = 0;
  for (j = 0; j < sgrp_used_genrator_entries; j++)
  {
    for (k = 0; k < generator_entry[j].num_groups; k++)
    {
      NIDL(1, "Set flag %d, num_grps = %d", set_flag, generator_entry[j].num_groups);
      set_flag = 0;
      if (scen_grp_entry[generator_entry[j].group_id_list[k]].grp_type == TC_FIX_USER_RATE) 
      {
        NIDL(4, "Set flag for generator name %s, percentage = %f", generator_entry[j].gen_name, scen_grp_entry[generator_entry[j].group_id_list[k]].percentage);
        if ((scen_grp_entry[generator_entry[j].group_id_list[k]].percentage/SESSION_RATE_MULTIPLIER) == 0.0) 
          set_flag = 1; 
        else 
          break;//If one group found with non zero quantity, check next generator in list 
      } else {
        NIDL(4, "Set flag for generator name %s, quantity = %d", generator_entry[j].gen_name, scen_grp_entry[generator_entry[j].group_id_list[k]].quantity);
        if(scen_grp_entry[generator_entry[j].group_id_list[k]].quantity == 0) 
           set_flag = 1;   
        else 
          break;//If one group found with non zero quantity, check next generator in list
       }
    }
    if (set_flag == 1) 
    {
      NIDL(1, "All quantity/percentage provided for generator is 0, hence marking generator %s", generator_entry[j].gen_name);
      fprintf(stderr, "Warning: Load distribution for generator %s is 0 therefore in current session this generator will not participate...\n", generator_entry[j].gen_name);
      NS_DUMP_WARNING("Load distribution for generator %s is 0 therefore in current session this generator will not participate",
                       generator_entry[j].gen_name);
      generator_entry[j].mark_gen = 1;
      sgrp_used_genrator_entries--;//Decrement count of used generators.
    }
  }
}

/*Ignoring all killed generators from generator table if*/
/*getting failed in CHECK_GENERATOR_HEALTH mode.*/
static void update_gen_table()
{
  int i;
  for(i = 0; i < sgrp_used_genrator_entries; i++)
  {
    NIDL(1, "i = %d, generator name = %s", i,generator_entry[i].gen_name);
    if(generator_entry[i].flags & IS_GEN_KILLED)
    {
      memmove(generator_entry + i, generator_entry + i + 1, (sgrp_used_genrator_entries - (i + 1)) * sizeof(GeneratorEntry));
      memset(generator_entry + (sgrp_used_genrator_entries - 1), 0, sizeof(GeneratorEntry));
      sgrp_used_genrator_entries--;
      i--;
    }
  }

  max_used_generator_entries = sgrp_used_genrator_entries;
  total_generator_entries = sgrp_used_genrator_entries;
  g_data_control_var.total_killed_gen = 0;
  NIDL(1, "sgrp_used_genrator_entries = %d, max_used_generator_entries = %d, total_generator_entries = %d", 
           sgrp_used_genrator_entries, max_used_generator_entries, total_generator_entries);

  for(i = 0; i < sgrp_used_genrator_entries; i++)
    NIDL(1, "i = %d, generator name = %s", i,generator_entry[i].gen_name);
  generator_entry_dump(generator_entry);
}

void handle_child_ignore1()
{
}

void check_generator_health()
{
  char controller_build_ver[128] = {0};
  char cmd[1024] = {0};
  
  long long time1;
  long long diff;

  time1=time(NULL);

  NIDL(1, "Method called, generator health check start time = %lld", time1);
  
  if(checkgenhealth.check_generator_health)
  {
    write_log_file(NS_GEN_VALIDATION, "Checking generator health w.r.t (cmon,build version,memory,disk,bandwidth)");
    char *version_ptr = get_version("NetStorm");
    strcpy(controller_build_ver, version_ptr);
    sprintf(cmd, "echo \"%s\" | awk -F ' ' '{print $2 $3 $4}' | sed 's/GUI:Version//g'", controller_build_ver);
    if (nslb_run_cmd_and_get_last_line(cmd, 1024, controller_ns_build_ver) != 0) {
      NIDL(1, "Error in running cmd = %s, exiting !", cmd);
      NS_EXIT(-1, "Unable to get product version, please upgrade valid netstorm image");
    }

    sighandler_t child_handler1;
    child_handler1 = signal(SIGCHLD, handle_child_ignore1);
    run_command_in_thread(CHECK_GENERATOR_HEALTH, 0);
    (void) signal(SIGCHLD, child_handler1);
  }

  diff = time(NULL) - time1;
  NIDL(1, "Checking of generator health took %lld seconds", diff);
  if (!total_gen_location_entries)
    update_gen_table();
  else
    generator_entry_dump(generator_entry);
}

void ni_default_ramp_down_phase(schedule *schedule_ptr, int difference_of_ramp_up_down, char* grp_phase_name)
{
  ni_Phases *ph;
  int phase_idx;
  NIDL(1, "Method called");

  phase_idx = schedule_ptr->num_phases;
  schedule_ptr->num_phases = (schedule_ptr->num_phases + 1);
  MY_REALLOC(schedule_ptr->phase_array, sizeof(ni_Phases) * schedule_ptr->num_phases, "phase_array", schedule_ptr->num_phases);
  ph = &(schedule_ptr->phase_array[phase_idx]);
  memset(ph, 0, sizeof(ni_Phases));
  if (schedule_by == SCHEDULE_BY_SCENARIO) {                                      //Fill phase name
    strncpy(ph->phase_name, "Default_RampDown", sizeof(ph->phase_name) - 1);
  } 
  else {
    strncpy(ph->phase_name, grp_phase_name, sizeof(ph->phase_name) - 1); 
  }
  ph->phase_type = SCHEDULE_PHASE_RAMP_DOWN;                                      //Fill Phase type ramp_down
  ph->phase_cmd.ramp_down_phase.ramp_down_mode = RAMP_DOWN_MODE_IMMEDIATE;        //Fill ramp_donw mode immediate
  ph->phase_cmd.ramp_down_phase.ramp_down_pattern = RAMP_DOWN_PATTERN_LINEAR;     //Fill ramp_down_pattern Linearly
  ph->phase_cmd.ramp_down_phase.num_vusers_or_sess = difference_of_ramp_up_down;  //Fill users on group
  NIDL(1, "phase_name = %s, ramp_down_mode = %d, ramp_down_pattern = %d, num_vusers_or_sess = %d, phase_type = %d, "
          "phase_idx = %d, num_phases = %d", ph->phase_name, ph->phase_cmd.ramp_down_phase.ramp_down_mode,
           ph->phase_cmd.ramp_down_phase.ramp_down_pattern, ph->phase_cmd.ramp_down_phase.num_vusers_or_sess,
           ph->phase_type, phase_idx, schedule_ptr->num_phases);
}

int ni_find_total_ramp_up_down_users(schedule *schedule_ptr, int quantity)
{
  ni_Phases *ph;
  int difference_of_ramp_up_down = 0, phase_idx;
  int total_ramp_up_users_gen = 0;
  int total_ramp_down_users_gen = 0;
  NIDL(1, "Method called");
  
  //Fill total ramp_up and ramp_down users
  for(phase_idx = 0; phase_idx < schedule_ptr->num_phases; phase_idx++) {
    ph = &(schedule_ptr->phase_array[phase_idx]);
    if(ph->phase_type == SCHEDULE_PHASE_RAMP_UP) {
      NIDL(1, "num_vusers_or_sess = %d, quantity = %d", ph->phase_cmd.ramp_up_phase.num_vusers_or_sess, quantity);
      if(ph->phase_cmd.ramp_up_phase.num_vusers_or_sess != -1) {
       total_ramp_up_users_gen += ph->phase_cmd.ramp_up_phase.num_vusers_or_sess;
      }
      else { 
       total_ramp_up_users_gen += quantity;
      }
    }
    else if (ph->phase_type == SCHEDULE_PHASE_RAMP_DOWN) {
      NIDL(1, "num_vusers_or_sess = %d, quantity = %d", ph->phase_cmd.ramp_down_phase.num_vusers_or_sess, quantity);
      if(ph->phase_cmd.ramp_up_phase.num_vusers_or_sess != -1) { 
        total_ramp_down_users_gen += ph->phase_cmd.ramp_down_phase.num_vusers_or_sess;
      }
      else { 
        total_ramp_down_users_gen += quantity;
      }
    }
  }

  //Calculating difference of ramp_up and ramp_down users
  difference_of_ramp_up_down = total_ramp_up_users_gen - total_ramp_down_users_gen;
  NIDL(1, "total_ramp_up_users_gen = %d, total_ramp_down_users_gen = %d, difference_of_ramp_up_down = %d",
           total_ramp_up_users_gen, total_ramp_down_users_gen, difference_of_ramp_up_down);
  
  return difference_of_ramp_up_down;
}

void ni_add_default_ramp_down_phase()
{
  int difference_of_ramp_up_down = 0, i, j, total_users = 0;
  char grp_phase_name[MAX_PHASE_NAME_LENGTH];
  NIDL(1, "Method called");

  if (schedule_by == SCHEDULE_BY_SCENARIO) {
    for (i = 0; i < total_sgrp_entries; i++) {
      if(get_grp_mode(i) != TC_FIX_USER_RATE)
        total_users += scen_grp_entry[i].quantity;
      else
        total_users += scen_grp_entry[i].percentage; 
    }
    //Find the difference of ramp_down and ramp_up user 
    difference_of_ramp_up_down = ni_find_total_ramp_up_down_users(scenario_schedule_ptr, total_users);
      
    //Add default ramp_down phase on phase_array structure according to difference
    if(difference_of_ramp_up_down > 0) {
      ni_default_ramp_down_phase(scenario_schedule_ptr, difference_of_ramp_up_down, grp_phase_name); 
    }
  } 
  else if (schedule_by == SCHEDULE_BY_GROUP) {
    for (i = 0; i < total_sgrp_entries;) {
      //Find the difference of ramp_down and ramp_up users per group
      total_users = 0;
      for(j = i; j < (i + scen_grp_entry[i].num_generator); j++) {
        if(get_grp_mode(j) != TC_FIX_USER_RATE)
          total_users += scen_grp_entry[j].quantity;
        else
          total_users += scen_grp_entry[j].percentage;
      }
      difference_of_ramp_up_down = ni_find_total_ramp_up_down_users(&group_schedule_ptr[i], total_users);
     
      //Add default ramp_down phase on phase_array structure according to difference on per group
      if(difference_of_ramp_up_down > 0) {
        snprintf(grp_phase_name, MAX_PHASE_NAME_LENGTH, "%sDefault_RampDown", scen_grp_entry[i].scen_group_name);
        for(j = i; j < (i + scen_grp_entry[i].num_generator); j++) {
          ni_default_ramp_down_phase(&group_schedule_ptr[j], difference_of_ramp_up_down, grp_phase_name);
        }
      }
      i += scen_grp_entry[i].num_generator; 
    }
  }
}

void init_generator_capacity_and_distribution() 
{
  int rnum, i, remaning_capacity = 0, total_cvms = 0, total_capacity = 0, gen_idx, per_gen_cap = 0;
  NIDL(4, "Method Called, total_sgrp_entries = %d, total_used_sgrp_entries = %d, g_per_cvm_distribution = %d, total_generator_entries = %d", 
           total_sgrp_entries, total_used_sgrp_entries, g_per_cvm_distribution, total_generator_entries);

  if(!g_per_cvm_distribution) { 
    return; 
  }
  //Reseting the num_connection and vuser_rpm variable at the time of g_per_cvm_distribution is on 
  num_connections = 0;
  vuser_rpm = 0; 
  //malloc data structure of filled per generator capacity
  NSLB_MALLOC_AND_MEMSET(gen_cap, (sizeof(int) * total_generator_entries), "gen_capacity_per_gen", -1, NULL);
 
  //Find total users or session in secenario 
  for(rnum = 0; rnum < total_sgrp_entries;) {
    NIDL(4, "scen_grp_entry[%d].pct_value = %lf", rnum, scen_grp_entry[rnum].pct_value);
    total_capacity += scen_grp_entry[rnum].pct_value;
    rnum += scen_grp_entry[rnum].num_generator;
  }
  NIDL(4, "Total_capacity = %d", total_capacity);

  //Find total cvms on generator and capacity of generator
  for(i = 0; i < total_generator_entries; i++) { 
    total_cvms += generator_entry[i].num_cvms;
    gen_cap[i].per_gen_quantity_distributed = 0;
  }
  NIDL(4, "Total_cvms = %d", total_cvms);

  //Calculate per generator capacity
  remaning_capacity = total_capacity;
  for(i = 0; i < total_generator_entries; i++) {
    gen_cap[i].cap_per_gen = (total_capacity * generator_entry[i].num_cvms) / total_cvms;
    remaning_capacity -= gen_cap[i].cap_per_gen;
  }
  NIDL(4, "remaning_capacity = %d", remaning_capacity);
  
  //Find and distribute remaining capacity 
  per_gen_cap = remaning_capacity / total_generator_entries;
  if(!per_gen_cap)
    per_gen_cap = 1;
  while(remaning_capacity > 0) {
    for(i = 0; i < total_generator_entries && remaning_capacity > 0; i++) {
      gen_cap[i].cap_per_gen += per_gen_cap;
      remaning_capacity -= per_gen_cap;
    }
    per_gen_cap = 1;
  }
  NIDL (2, "total_capacity = %d, per_gen_cap = %d", total_capacity, per_gen_cap);

//for debug purpose  
#ifdef NS_DEBUG_ON
  for(i = 0; i < total_generator_entries; i++) 
    NIDL(2, "gen_cap[%d].cap_per_gen = %d, gen_cap[%d].per_gen_quantity_distributed = %d",
             i, gen_cap[i].cap_per_gen, i, gen_cap[i].per_gen_quantity_distributed);
#endif
  
  //In case of pct mode data is already distributed among some generators so set that count as distributed capacity
  for(rnum = 0; rnum < total_sgrp_entries; rnum += scen_grp_entry[rnum].num_generator) {
    if(scen_grp_entry[rnum].pct_flag_set) {
      for(i = 0; i < scen_grp_entry[rnum].num_generator; i++) {
        gen_idx =  scen_grp_entry[rnum].generator_id_list[i];
        if (scen_grp_entry[rnum].grp_type == TC_FIX_USER_RATE) {
          gen_cap[gen_idx].per_gen_quantity_distributed += scen_grp_entry[rnum + i].percentage;
          NIDL(4, "gen_idx= %d, gen_cap[%d].per_gen_quantity_distributed = %d, scen_grp_entry[rnum + i].percentage = %lf", gen_idx, gen_idx, 
                   gen_cap[gen_idx].per_gen_quantity_distributed, rnum + i, scen_grp_entry[rnum + i].percentage);
        } else {
          gen_cap[gen_idx].per_gen_quantity_distributed += scen_grp_entry[rnum + i].quantity;
          NIDL(4, "gen_idx= %d, gen_cap[%d].per_gen_quantity_distributed = %d, scen_grp_entry[%d].quantity = %d", gen_idx, gen_idx, 
                   gen_cap[gen_idx].per_gen_quantity_distributed, rnum + i, scen_grp_entry[rnum + i].quantity);
        }
      }
    }
  }

  //1. Distribute single generator entry first
  for(rnum = 0; rnum < total_sgrp_entries; rnum += scen_grp_entry[rnum].num_generator) {
    if(!scen_grp_entry[rnum].pct_flag_set && scen_grp_entry[rnum].num_generator == 1)
      divide_usr_wrt_generator_capacity(rnum, gen_cap);
  }
  //2. Distribute multiple generator entry
  for(rnum = 0; rnum < total_sgrp_entries; rnum += scen_grp_entry[rnum].num_generator) {
    if(!scen_grp_entry[rnum].pct_flag_set && scen_grp_entry[rnum].num_generator > 1 && scen_grp_entry[rnum].num_generator < total_generator_entries)
      divide_usr_wrt_generator_capacity(rnum, gen_cap);
  } 
  //3. Distribute all generator entry
  for(rnum = 0; rnum < total_sgrp_entries; rnum += scen_grp_entry[rnum].num_generator) {
    if(!scen_grp_entry[rnum].pct_flag_set && scen_grp_entry[rnum].num_generator > 1 && scen_grp_entry[rnum].num_generator == total_generator_entries)
      divide_usr_wrt_generator_capacity(rnum, gen_cap);
  }
  
  //Free data structure of generator capacity
  FREE_AND_MAKE_NULL_EX(gen_cap, sizeof(gen_capacity_per_gen) * total_generator_entries, "gen_capacity_per_gen", -1);
}

/* Arguments needed:
 *   -n Scenario name
 *   -t Test run number
 *   -d Debug level */
int init_scenario_distribution_tool(char *scen_name, int debug_levl, int test_num, int tool_call_at_init_rtc, int default_gen_file, int for_quantity_rtc, char *err_msg)
{
  char create_cmd[MAX_COMMAND_SIZE];
  char scen_file_name[FILE_PATH_SIZE];
  struct stat sb; 
  char script_path[4096], script_version[64];
  int script_type = -1, i, ret;

  //Copy value in global variables
  test_run_num = test_num;
  ni_debug_level = debug_levl;
  time_t now;

  set_work_dir();
  NIDL(1, "Method called, scen_name = %s, debug_levl = %d, test_num = %d, tool_call_at_init_rtc = %d, for_quantity_rtc = %d", 
                          scen_name, debug_levl, test_num, tool_call_at_init_rtc, for_quantity_rtc);

  if (!tool_call_at_init_rtc)
  { 
    strcpy(scen_file_name, scen_name);

    //Remove controller directory
    //remove_existing_controller_dir();

    //Intialize structures
    init_generator_entry();
  
    //Set scenario path
    set_scenario_file_path(scen_file_name);
  
    //Set controller dirctory path
    set_controller_dir_path();
  
    //Sort scenario file
    //sort_scenario_file(scenario_file);
    strcpy(new_conf_file, g_sorted_conf_file);
   
    //Find controller type
    find_controller_type(controller_type);

    read_and_process_gen_location();
    //Read keywords from scenario file
    read_scen_keywords(default_gen_file, err_msg);
    
    /*Fill generator id in scenario group*/
    fill_generator_id_list();

    //Distribution of users according to generator capacity
    init_generator_capacity_and_distribution();   

    //validate NUM_USERS and PROF_PCT_MODE
    validate_num_user_and_pct_mode(); 
    //Debug Purpose
    generator_used_entry_dump(gen_used_list);
    generator_entry_dump(generator_entry);  
    NIDL(2, "Total number of scenario group = %d", total_sgrp_entries);
    if(total_sgrp_entries == 0)
    { 
      NS_EXIT(-1, CAV_ERR_1011300, "Please add atleast one scenario group to run test");
    }

    //In case of NETCLOUD_MODE 2, verify whether external generator list given or not
    verify_ext_gen_list_in_nc_mode_2();

    //Create used generator list file
    //create_used_gen_list_file();
    calculate_used_gen_list();
    if(sgrp_used_genrator_entries == 0)
    {
      NS_EXIT(-1, CAV_ERR_1011168, "Please add atleast one generator in scenario to run Netcloud test");
    }

    if ((prof_pct_mode == PCT_MODE_PCT) && (mode == TC_FIX_USER_RATE) && (schedule_type != SCHEDULE_TYPE_ADVANCED))
    {
      if (total_sessions <= 0) 
      {
        NIDL(2, "total_sessions = %d", total_sessions);
        NS_EXIT(-1, CAV_ERR_1011173, "Please input valid target session rate to run NetCloud test.");
      }     
    }
   
    //Check whether PROF_PCT_MODE = PCT
    if ((prof_pct_mode == PCT_MODE_PCT))
    {
      if (schedule_type == SCHEDULE_TYPE_ADVANCED) 
      {
        int tot_qty = calculate_total_user_or_session();
        if (total_gen_location_entries && (loc_user_or_session != tot_qty))
        {
          //In case of location mode, configued user/session in keyword NUM_USER/TARGET_RATE
          NS_EXIT(-1, "Total users/sessions are not configured as per schedule in scenario");
        }

        if (mode == TC_FIX_USER_RATE)
          vuser_rpm = total_sessions = tot_qty;
        else
          num_connections = num_users = tot_qty;
      }
      if (g_do_not_ship_test_assets == SHIP_TEST_ASSETS)
      {
        if (mode == TC_FIX_USER_RATE) 
          distribute_pct_among_grps(scen_grp_entry, total_sessions);
        else {     
          NIDL(3, "Entered number of users...........NUM_USERS = %d\n", num_users);
          distribute_pct_among_grps(scen_grp_entry, num_users);
        }
      }
    }
  
    /*Check rampdown phase in scenario if not exist add default rampdown phase of mode immadiatly on generators*/ 
    if (schedule_type == SCHEDULE_TYPE_ADVANCED)
      ni_add_default_ramp_down_phase();

    if ((prof_pct_mode == PCT_MODE_NUM) && (mode == TC_FIX_USER_RATE) && (schedule_type == SCHEDULE_TYPE_ADVANCED))
    {    
      NIDL(3, "In advance scenario, FSR number mode vuser_rpm = %f", vuser_rpm);
    }
    for (i = 0; i < total_script_entries; i++)
    {
      sprintf(script_path, "%s/%s/%s/scripts/%s", GET_NS_TA_DIR(), script_table[i].proj_name, script_table[i].sub_proj_name, 
                                                  script_table[i].script_name);
      script_name = script_path + 9 + strlen(work_dir); 
      NIDL (1, "Parsing Script <%s>\n", script_table[i].script_name);
      write_log_file(NS_GEN_VALIDATION, "Parsing script %s for generators", script_table[i].script_name);
      /*Find script type used in scenario group*/
      if ((ret = ni_get_script_type(script_path, &script_type, script_version)) == FAILURE_EXIT)
      { //No need to replace error msg 
        NS_EXIT(FAILURE_EXIT, "Failed to fetch script type of script %s", script_path);
      }  
      /* In case of C type script we need to parse registration.spec file */ 
      //if (script_type == SCRIPT_TYPE_C) 
      {
        time(&now);
        NIDL (1, "Start time for parsing registration.spec file =%s", ctime(&now));
        write_log_file(NS_GEN_VALIDATION, "Parsing script parameters of %s for generators", script_table[i].script_name);
        if ((ni_parse_registration_file(script_path, i) == FAILURE_EXIT)) {
          NS_EXIT(FAILURE_EXIT, "Failed to parse script variables of script %s", script_path);
        }
        time(&now);
        NIDL (1, "Time after parsing registration.spec file = %s", ctime(&now));
      }     
    }
  } else {
    open_file_fd( &debug_fp);
    read_kw_rtc(scen_name, err_msg);
  }

  /*Validate schedule phase of controller scenario*/
  if (!tool_call_at_init_rtc) {
    ni_validate_phases();

    /*Memcpy controller schedule pointer per generator*/  
    update_schedule_structure_per_gen();
  }
  if ((!tool_call_at_init_rtc) || (tool_call_at_init_rtc && scheduling_flag_enable)) {
    /*Distribute schedule settings among generators*/
    if ((mode == TC_FIX_CONCURRENT_USERS) || (mode == TC_MIXED_MODE))
      distribute_schedule_among_gen();
  }

  /*Advance scenario/group*/
  if (!tool_call_at_init_rtc) {
    if (schedule_type == SCHEDULE_TYPE_ADVANCED)
      distribute_vuser_or_sess_among_gen();

    find_num_grp_per_gen();
    /*Create generator directories and store scenario files*/
    if (g_do_not_ship_test_assets == SHIP_TEST_ASSETS)
      create_gen_dir_store_scen_file();
    /*Fill generator id in scenario group*/
    //fill_generator_id_list();
    //Create used generator list file
    create_used_gen_list_file();
    /* create jmeter scripts for generators*/
    if(jmx_enabled_script && (g_do_not_ship_test_assets == SHIP_TEST_ASSETS))
      ns_copy_jmeter_scripts_to_generator();

    time(&now);
    NIDL(1, "Start time for copying undivided data files from datadir =%s", ctime(&now));
    copy_data_dir_files(); //undivided
    time(&now);
    NIDL(1, "Time after copying undivided data files from datadir =%s", ctime(&now));

    /*Distribute file parameter users*/
    time(&now);
    NIDL(1, "Start time for dividing data files =%s", ctime(&now));
    ni_divide_values_per_generator(0);
    time(&now);
    NIDL(1, "Time after dividing data files =%s", ctime(&now));
    /*Creating script wise registrations.spec file for generators*/
    time(&now);
    NIDL(1, "Start time for creating registrations.spec files = %s", ctime(&now));
    ni_divide_values_unique_var_per_generator(0);
    time(&now);
    NIDL(1, "Time after creating registrations.spec files = %s", ctime(&now));
    /*Create control files for file parameter*/
    create_ctrl_files();
    /*Fill default progress interval in generators*/
    if (g_do_not_ship_test_assets == SHIP_TEST_ASSETS) 
    {
      reframe_progress_msecs(controller_dir, scenario_file_name, scen_proj_subproj_dir);
      reframe_alert_keyword(controller_dir, scenario_file_name, scen_proj_subproj_dir);
    }
  } 
  if (((!tool_call_at_init_rtc) || (tool_call_at_init_rtc && scheduling_flag_enable)) && (g_do_not_ship_test_assets == SHIP_TEST_ASSETS)) {
    /*Function used to reconstruct schedule phase statement*/
    reframe_schedule_keyword(controller_dir, scenario_file_name, scen_proj_subproj_dir, tool_call_at_init_rtc, for_quantity_rtc); 
  }
   //FCS mode
   if((!tool_call_at_init_rtc)) {
    if(enable_fcs_settings_mode) {
      if(mode == TC_FIX_CONCURRENT_USERS) {
        if (g_do_not_ship_test_assets == SHIP_TEST_ASSETS) {
          divide_limit_wrt_generators(sgrp_used_genrator_entries, enable_fcs_settings_limit, enable_fcs_settings_queue_size);
          reframe_fcs_keyword(controller_dir, scenario_file_name, scen_proj_subproj_dir);
        }
      }
      else
      {
        NS_EXIT(-1, "Fix concurrent sessions (FCS) is only supported in Fix concurrent users"
                       " (FCU), please set STYPE keyword as FIX_CONCURRENT_USERS and re-run the test");
      }
    }
  }

  if (!tool_call_at_init_rtc) {
    /*Free gen_sch_setting ptr*/
    if (schedule_by == SCHEDULE_BY_SCENARIO) {
      NIDL(3, "Free and Make null pointer gen_sch_setting = %p", gen_sch_setting);
      free(gen_sch_setting);
      gen_sch_setting = NULL;
    }
  }
   
  if (!tool_call_at_init_rtc) 
  {
    /*Free and make NULL malloced pointers*/
    free_malloc_ptr();

    //Generator Shell
    NIDL(3, "Providing data to shell work_dir = %s", work_dir);
    NIDL(3, "Scenario file %s", scen_file_for_shell);

    if (test_run_num != -1) 
    {
      if (g_do_not_ship_test_assets == SHIP_TEST_ASSETS)
      {
        write_log_file(NS_GEN_VALIDATION, "Creating scenario and script for generators in .controller directory");
        if (stat(internal_used_gen_file_name, &sb) == 0) 
        {
          if (scenario_settings_profile_file_flag)
            sprintf(create_cmd, "%s/bin/nii_create_generator_data -g %s -s %s -t %d -D %d -p %s -i %s", work_dir, internal_used_gen_file_name, scen_file_for_shell, test_run_num, ni_debug_level?1:0, scenario_settings_profile_file, g_cavinfo.NSAdminIP);
          else  
            sprintf(create_cmd, "%s/bin/nii_create_generator_data -g %s -s %s -t %d -D %d -i %s", work_dir, internal_used_gen_file_name, scen_file_for_shell, test_run_num, ni_debug_level?1:0, g_cavinfo.NSAdminIP);
          if (system(create_cmd) != 0)
          {
            NS_EXIT(-1, "Failed to create generator data.");
          } 
        } 
     
        if (stat(external_used_gen_file_name, &sb) == 0) 
        {
          if (scenario_settings_profile_file_flag)
            sprintf(create_cmd, "%s/bin/nii_create_generator_data -g %s -s %s -t %d -D %d -T external -p %s -i %s", work_dir, external_used_gen_file_name, scen_file_for_shell, test_run_num, ni_debug_level?1:0, scenario_settings_profile_file, g_cavinfo.NSAdminIP);
          else
            sprintf(create_cmd, "%s/bin/nii_create_generator_data -g %s -s %s -t %d -D %d -T external, -i %s", work_dir, external_used_gen_file_name, scen_file_for_shell, test_run_num, ni_debug_level?1:0, g_cavinfo.NSAdminIP);
          if (system(create_cmd) != 0)
          {
            NS_EXIT(-1, "Failed to create generator data.");
          }
        } 
        NIDL(3, "create_cmd = %s", create_cmd);
      }
    }
    else 
    {
      if (stat(internal_used_gen_file_name, &sb) == 0) 
      {
        if (scenario_settings_profile_file_flag)
          sprintf(create_cmd, "%s/bin/nii_create_generator_data -g %s -s %s -D %d -p %s -i %s", work_dir, internal_used_gen_file_name, scen_file_for_shell, ni_debug_level?1:0, scenario_settings_profile_file, g_cavinfo.NSAdminIP);
        else 
          sprintf(create_cmd, "%s/bin/nii_create_generator_data -g %s -s %s -D %d -i %s", work_dir, internal_used_gen_file_name, scen_file_for_shell, ni_debug_level?1:0, g_cavinfo.NSAdminIP);
        if (system(create_cmd) != 0)
        {
          NS_EXIT(-1, "Failed to create generator data.");
        }
      }  
    
      if (stat(external_used_gen_file_name, &sb) == 0) 
      {
        if (scenario_settings_profile_file_flag)
          sprintf(create_cmd, "%s/bin/nii_create_generator_data -g %s -s %s -D %d -T external -p %s -i %s", work_dir, external_used_gen_file_name, scen_file_for_shell, ni_debug_level?1:0, scenario_settings_profile_file, g_cavinfo.NSAdminIP);
        else
          sprintf(create_cmd, "%s/bin/nii_create_generator_data -g %s -s %s -D %d -T external -i %s", work_dir, external_used_gen_file_name, scen_file_for_shell, ni_debug_level?1:0, g_cavinfo.NSAdminIP);
        if (system(create_cmd) != 0)
        {
          NS_EXIT(-1, "Failed to create generator data.");
        }
      }
    } 
  }
  /*if (debug_levl) {
    fclose(debug_fp);
    debug_fp = NULL;
  }*/ 
  return SUCCESS_EXIT;
}

void *nsi_get_gen_instances(void *arg)
{
    char cmd[1024];
    int loc_idx;
    int *loc_idx_ptr;
    FILE *fp = NULL;
    char tmp[1024+1];
    int count = 0;
    char *field[3];
    char *work;
    char work_path[512]="/home/cavisson/work";

    loc_idx_ptr = (int *)arg;
    loc_idx = *loc_idx_ptr;
    FREE_AND_MAKE_NOT_NULL(loc_idx_ptr, "loc_idx_ptr", -1);

    NIDL(2, "Method called. loc_idx = %d", loc_idx); 
    sprintf(cmd, "nsi_allocate_gen_instances %d '%s' %d", test_run_num, g_gen_location_table[loc_idx].name, g_gen_location_table[loc_idx].genRequired); 

    NIDL(2, "Method called. cmd = %s", cmd);
    if((fp =  popen(cmd, "r")) == NULL)
    {
      NIDL(2, "Error in executing command [%s]. Error = %s", cmd , nslb_strerror(errno));
      return NULL;
    }
    while(fgets(tmp, 1024, fp) != NULL)
    {
      NIDL(2, "generator line = %s", tmp);
      int len = strlen(tmp);
      if(tmp[len - 1] == '\n')
        tmp[len -1]='\0';
      int total_flds = get_tokens_(tmp, field, "|", 3);
      if (total_flds != 2)
      {
        fprintf(stderr, "%s\n", tmp);
        return NULL; 
      }
      if((work = strrchr(field[0], ':')) != NULL)
      {
         sprintf(work_path, "/home/cavisson/%s", work + 1);
         *work = '\0';
      }
      //fill generators information in generator entry table
      strcpy((char *)generator_entry[g_gen_location_table[loc_idx].startIndexGenTbl + count].gen_name, field[0]);
      strcpy(generator_entry[g_gen_location_table[loc_idx].startIndexGenTbl + count].IP, field[1]);
      strcpy(generator_entry[g_gen_location_table[loc_idx].startIndexGenTbl + count].work, work_path);
      strcpy(generator_entry[g_gen_location_table[loc_idx].startIndexGenTbl + count].location, g_gen_location_table[loc_idx].name);
      generator_entry[g_gen_location_table[loc_idx].startIndexGenTbl + count].loc_idx = loc_idx;
      count++;
    }
 
    pclose(fp);

    return NULL;  
}

int calculate_gen_and_fill_table()
{
  int i=0;
  int total_gen = 0;
  int ret/*, total_gen*/;
  static pthread_t *thread = NULL;
  pthread_attr_t attr;
  
  NIDL(2, "Method called. total_gen_location_entries = %d", total_gen_location_entries);
  //calculate the total number of generators required
  for(i = 0; i< total_gen_location_entries; i++)
  {
     int per_gen_cap = g_gen_location_table[i].genCapacity;
     int total_user_or_sessions = g_gen_location_table[i].numUserOrSession;
     if (g_runtime_user_session_pct)
     {
       total_user_or_sessions += (total_user_or_sessions * g_runtime_user_session_pct) / 100;
     }
     int num_gen_req = (total_user_or_sessions/per_gen_cap) + ((total_user_or_sessions%per_gen_cap)?1:0);
     g_gen_location_table[i].startIndexGenTbl = total_gen; 
     g_gen_location_table[i].genRequired = num_gen_req;
     total_gen += num_gen_req;
     NIDL(2, "per_gen_cap = %d, total_user_or_sessions = %d, num_gen_req = %d", per_gen_cap, total_user_or_sessions, num_gen_req);
     
  }
  
  write_log_file(NS_GEN_VALIDATION, "Total generator required %d", total_gen);
  if (total_gen > 255)
  {
    write_log_file(NS_GEN_VALIDATION, "Total generator count is more than 255");
    NS_EXIT(-1, "Total generator count is more than 255. Exiting..."); 
  }
  max_used_generator_entries = total_gen; 
  total_generator_entries = total_gen; 
  MY_MALLOC_AND_MEMSET(generator_entry, total_gen * sizeof(GeneratorEntry), "generator_entry", -1);
  
  MY_MALLOC (thread, total_gen_location_entries * sizeof(pthread_t), "thread pthread_t", -1);

  pthread_attr_init(&attr);
  //pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  write_log_file(NS_GEN_VALIDATION, "Creating generators");
  char cmd[1024+1];
  sprintf(cmd, "nohup nsi_check_and_stop_gen_instances %d >> %s/logs/TR%d/NetCloud/gen_inst.netstorm.log 2>&1 &", test_run_num, 
                g_ns_wdir, test_run_num);
  system(cmd);

  //create thread to create number of generator instancges as per location requirement
  for(i = 0; i < total_gen_location_entries; i++)
  {
    int *loc_idx;
    MY_MALLOC(loc_idx, sizeof(int), "loc_idx", -1);
    *loc_idx = i;
    write_log_file(NS_GEN_VALIDATION, "Creating %d generators for location '%s'", g_gen_location_table[i].genRequired, 
                   g_gen_location_table[i].name);
    ret = pthread_create(&thread[i], &attr, nsi_get_gen_instances, loc_idx);
    if (ret)
    {
      write_log_file(NS_GEN_VALIDATION, "Failed to create generators for location '%s'", g_gen_location_table[i].name);
      NS_EXIT(-1, CAV_ERR_1014014); 
    }
  }

  pthread_attr_destroy(&attr);
  
  //wait of all threads
  for(i = 0; i < total_gen_location_entries; i++)
  {
    ret = pthread_join(thread[i], NULL);
    if(ret)
    {
      NS_EXIT(-1, "Error code from pthread join for thread id[%d] = \'%d\'", i, ret);
    }
  }

  FREE_AND_MAKE_NULL(thread, "Freeing Thread pointer", -1);
 
  //initalize the generator entry table
  for (i = 0; i < total_gen; i++)
  {
    if (!generator_entry[i].IP[0])
    {
      NS_EXIT(-1, "Test failed due to unavailablity of generators"); 
    }
    strcpy(generator_entry[i].agentport, "7891");
    strcpy(generator_entry[i].gen_type, "Internal");
    strcpy(generator_entry[i].comments, "NA");
    generator_entry[i].used_gen_flag = 0; //intially flag is disable
    generator_entry[i].resolve_flag = 0; //intially flag is disable
    generator_entry[i].num_groups = 0;
    generator_entry[i].mark_gen = 0;
    nslb_md5(generator_entry[i].gen_name, generator_entry[i].token);
    write_log_file(NS_GEN_VALIDATION, "Generator[%s:%s] created for location '%s'", generator_entry[i].gen_name, generator_entry[i].IP, g_gen_location_table[generator_entry[i].loc_idx].name);
  }

  //calculate and distribute users or session among all generators as per location configuration
  for(i = 0; i < total_gen_location_entries; i++)
  {
    int pct_value = (g_gen_location_table[i].pct * 100)/g_gen_location_table[i].genRequired;
    int remaining_pct = (g_gen_location_table[i].pct * 100) % g_gen_location_table[i].genRequired;
    for(int j = 0; j< g_gen_location_table[i].genRequired; j++)
    {
      generator_entry[g_gen_location_table[i].startIndexGenTbl + j].pct_value = pct_value;
      if (remaining_pct > 0)
      {
        generator_entry[g_gen_location_table[i].startIndexGenTbl + j].pct_value++;
        remaining_pct--;
      } 
      NIDL(2, "Method called. total_pct_per_gen = %d", generator_entry[g_gen_location_table[i].startIndexGenTbl + j].pct_value);
    }
  }
  return 0;
}

static int g_total_loc_pct = 0;
int add_location_to_list(char *buf, char *err_msg)
{
  char name[512];
  int pct;

  NIDL(2, "Location = %s, total_gen_location_entries = %d, max_gen_location_entries = %d", buf, 
           total_gen_location_entries, max_gen_location_entries);

  sscanf(buf, "%s %d", name, &pct);
  if (pct <= 0)  //TODO: need to handle for SRCP pct distribution
    return 0; 
  if(total_gen_location_entries == max_gen_location_entries)
  {
    max_gen_location_entries += DELTA_GENERATOR_ENTRIES;
    NSLB_REALLOC_AND_MEMSET(g_gen_location_table, (max_gen_location_entries * sizeof(LocationTable)), (total_gen_location_entries * sizeof(LocationTable)), "LocationTable malloc", -1, NULL);
  }
  strcpy(g_gen_location_table[total_gen_location_entries].name, name);
  g_gen_location_table[total_gen_location_entries].pct = pct;
  g_total_loc_pct += pct;
  total_gen_location_entries++;
  NIDL(2, "Total location entries = %d, Location name = %s, PCT = %d",total_gen_location_entries, 
           g_gen_location_table[total_gen_location_entries].name,
           g_gen_location_table[total_gen_location_entries].pct);

  return 0;
}

int read_and_process_gen_location()
{
  FILE *fp;
  char err_msg[1024];
  char buf[2048+1];
  char text[MAX_DATA_LINE_LENGTH];
  char keyword[MAX_DATA_LINE_LENGTH];
  int num;
  char line[2024];
  NIDL(2, "Method called. generator entry = %d, scen file = %s", used_generator_entries, new_conf_file);

  //IN case of NS_GENERATOR Keyword this code will not be executed.
  if (used_generator_entries)
  {
     NIDL(2, "NS_GENERATOR keywork is used in scenario");
     return 0;
  }

  if ((fp = fopen(new_conf_file, "r")) == NULL) {
    NS_EXIT(-1, CAV_ERR_1000006, new_conf_file, errno, nslb_strerror(errno));
  }
   
  while (fgets(buf, 2048, fp) != NULL)
  {
    NIDL(2, "Buffer = %s", buf);

    if ((num = sscanf(buf, "%s %s", keyword, text)) < 2) {
      fprintf(stderr, "\nread_scen_keywords(): At least two fields required  <%s>\n", buf);
      continue;
    }
    if (strcasecmp(keyword, "STYPE") == 0) // Parsing it here as we need mode for filling num_user/session_rate for gen_location
      kw_set_stype(buf, err_msg);
    else if (strcasecmp(keyword, "GEN_LOCATION") == 0)
      add_location_to_list(buf + sizeof("GEN_LOCATION"), err_msg);
    else if (strcasecmp(keyword, "NUM_USERS") == 0)
      local_num_users = atoi(text); //Fill local_num_users variable with the value mention in keyword.
    else if (strcasecmp(keyword, "PROF_PCT_MODE") == 0)
      kw_set_prof_pct_mode(buf, err_msg);
    else if (strcasecmp(keyword, "TARGET_RATE") == 0) 
      kw_set_target_rate(buf, err_msg);
    else if (strcasecmp(keyword, "RUNTIME_USER_SESSION_PCT") == 0)
    {
      if ((nslb_atoi(text, &g_runtime_user_session_pct) < 0) || g_runtime_user_session_pct < 0)
         NS_EXIT(-1, "Runtime percentage value must be numeric and greater than zero.");
    }
  }
  fclose (fp);

  if(mode == TC_MIXED_MODE)
   NS_EXIT(-1, "User and Session Rate both are not supported together");

  if (g_total_loc_pct != 100)
    NS_EXIT(-1, "Total location pct is not equal to 100.");
 
  if (prof_pct_mode == PCT_MODE_PCT)
  {
    if (((mode == TC_FIX_CONCURRENT_USERS) && (local_num_users == 0)) || ((mode == TC_FIX_USER_RATE) && (total_sessions == 0)))
    {
      NS_EXIT(-1, "In case of PROF_MODE_PCT user or session rate can not be zero.");
    }
  }
  
  char location_file_buf[1024];
  sprintf(location_file_buf, "%s/etc/.netcloud/gen_server.conf", g_ns_wdir);
  if ((fp = fopen(location_file_buf, "r")) == NULL) {
    NS_EXIT(-1, CAV_ERR_1000006, location_file_buf, errno, nslb_strerror(errno));
  }
  
  int user_capacity;
  int sessionrate_capacity;
  //Read file line-by line and fill structure as per gen_server.conf file entries 
  while (fgets(line, 2048, fp) != NULL)
  {
    char *line_ptr = line;
    char msg_line[2024];
    char *field[7];
    char *ptr;
    int line_num;

    CLEAR_WHITE_SPACE(line_ptr);
    NIDL (3, "line_ptr = %s, line = %s", line_ptr, line);
    memmove(line, line_ptr, strlen(line_ptr));
    //Ignoring generator header and blank or commented lines
    if(line[0] == '\n' || line[0] == '#') {
      NIDL(4, "Ignoring generator header or blank/commented line from generator file, line number = %d", line_num);
      line_num++;
      continue;
    }
    if((ptr = strchr(line, '\n')) != NULL) //Replace newline with NULL char in each line before saving fields in structure
      *ptr = '\0';
    strcpy(msg_line, line); //Copying line for debugging purpose
    int total_flds = get_tokens_(line, field, "=", 7);
    NIDL (3, "total_flds = %d, line_num = %d, line = %s", total_flds, line_num, msg_line);
    
    if (total_flds < 2)
    {
      NS_EXIT(-1, "Failed to parse gen_server.conf as minimun number of parameter should be atleast 2");
    }

    if (!strcmp("maxVuserCapacity", field[0]))
    {
      if (nslb_atoi(field[1], &user_capacity) < 0)
        NS_EXIT(-1, "Failed to parse gen_server.conf as user capacity is not numeric.");
    }
   
    if (!strcmp("maxSessionRateCapacity", field[0]))
    {
      if (nslb_atoi(field[1], &sessionrate_capacity) < 0)
        NS_EXIT(-1, "Failed to parse gen_server.conf as session rate is not numeric.");
    }
 
  }
  fclose(fp);
  for (int i = 0; i< total_gen_location_entries; i++)
  {
    if(mode == TC_FIX_USER_RATE)
      g_gen_location_table[i].genCapacity = sessionrate_capacity * SESSION_RATE_MULTIPLIER;
    else
      g_gen_location_table[i].genCapacity = user_capacity;
     NIDL (2, "capacity = %d", g_gen_location_table[i].genCapacity);
  } 

  //Distribute user or session per location in case of NUM mode during SGRP processing  
  //but in case of PCT need to do after SGRP processing but during SGCP 
  //we will calcucate the total numbers or session rate distributed among all groups and  how much is remaining 
  //as part of calculation that has to be adjusted later. 
  
  if ((fp = fopen(new_conf_file, "r")) == NULL) {
    NS_EXIT(-1, CAV_ERR_1000006, new_conf_file, errno, nslb_strerror(errno));
  }

  while (fgets(buf, 2048, fp) != NULL) 
  {
    NIDL(2, "Buffer = %s", buf);

    if ((num = sscanf(buf, "%s %s", keyword, text)) < 2)
    {
      fprintf(stderr, "\nread_scen_keywords(): At least two fields required  <%s>\n", buf);
      continue;
    }
    if (strcasecmp(keyword, "SGRP") == 0)
    {
        kw_set_sgrp(buf, err_msg, 1);
    }
  }
  fclose(fp);

  //Distribute user or session per location in case of PCT mode 
  if (prof_pct_mode == PCT_MODE_PCT)
  {
    int remaining_count, per_location_count, next_location;
    if (mode == TC_FIX_USER_RATE)
    {
      loc_user_or_session = total_sessions;  
    }
    else
    {
      loc_user_or_session = local_num_users;
    }
    remaining_count = loc_user_or_session;

    for (int i = 0; i < total_gen_location_entries; i++) 
    {
      per_location_count = (int )(loc_user_or_session * g_gen_location_table[i].pct)/100;
      remaining_count -= per_location_count;
      g_gen_location_table[i].numUserOrSession += per_location_count;
    }
    NIDL(3, "Remaining count = %d", remaining_count);
    while (remaining_count > 0)
    { 
      g_gen_location_table[next_location++].numUserOrSession++;
      next_location %= total_gen_location_entries;
      remaining_count--;
    }
    vuser_rpm = 0;
    total_sessions = 0;
  }
  
  //calculate per generator distribution and fill generator table
  calculate_gen_and_fill_table();
  g_gen_location_processed = 1;
  return 0;
}

//gcc -DTOOL_TEST keyword_parsing.c user_distribution.c -o keyword_parsing
#ifdef TEST
int main(int argc, char *argv[])
{
  int opt;
  extern int optopt;
  int scen_flag = 0;
  char file_name[FILE_PATH_SIZE];
  char create_cmd[MAX_COMMAND_SIZE];
  char err_msg[1024];

  set_work_dir();
  NIDL(1, "Method called");
  while ((opt = getopt(argc, argv, "hn:d:t:")) != -1)
  {
    switch (opt) {
      case 'h':
         help();
         break;
      case 'n':
        strcpy(file_name, optarg);
        scen_flag = 1;
        break;
      case 'd':
        ni_debug_level = atoi(optarg);
        break;
      case 't':
        test_run_num = atoi(optarg);
        break;
      default:
        fprintf(stderr, "\nInvalid option %c\n", optopt);
        help();
    }
  }
  /Scenario file is a mandatory option
  if (!scen_flag) {
    fprintf(stderr, "\nScenario file not given.Hence exiting...\n");
    help();
  }
  //Intialize structures
  init_generator_entry();
  
  //Set scenario path
  set_scenario_file_path(file_name);
  
  //Remove controller directory
  remove_existing_controller_dir();
  
  //Set controller dirctory path
  set_controller_dir_path();
  
  //Sort scenario file
  sort_scenario_file(scenario_file);
  
  //Read keywords from scenario file
  read_scen_keywords(default_gen_file, err_msg);
 
  //Create used generator list file
  create_used_gen_list_file();
  if(sgrp_used_genrator_entries == 0)
  {
    NS_EXIT(1, "Test is running in master mode, but no generators are provided to use.");
  }
  
  //Check whether PROF_PCT_MODE = PCT
  if (prof_pct_mode == PCT_MODE_PCT)
    distribute_pct_among_grps(scen_grp_entry, num_users);
  
  //Create generator directories and store scenario files
  create_gen_dir_store_scen_file(); 

  //Generator Shell
  NIDL(3, "Providing data to shell work_dir = %s", work_dir);
  NIDL(3, "Scenario file %s", scen_file_for_shell);
  NIDL(3, "Used generator file %s", used_gen_file_name);

  if  (test_run_num != -1)
    sprintf(create_cmd, "%s/bin/nii_create_generator_data -g %s -s %s -t %d -D %d -i %s", work_dir, used_gen_file_name, scen_file_for_shell, test_run_num, ni_debug_level?1:0, g_cavinfo.NSAdminIP);
  else
    sprintf(create_cmd, "%s/bin/nii_create_generator_data -g %s -s %s -D %d -i %s", work_dir, used_gen_file_name, scen_file_for_shell, ni_debug_level?1:0, g_cavinfo.NSAdminIP);

  if (system(create_cmd) != 0)
  {
    NS_EXIT(-1, "Generator shell command failed");
  }
 
  return SUCCESS_EXIT;
}
#endif 
