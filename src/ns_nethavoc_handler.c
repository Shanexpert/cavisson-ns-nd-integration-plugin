/***********************************************************************************************************
 * FILE    : ns_nethavoc_handler.c                                                                         *
 * AUTHOR  : Abhishek Pratap Singh                                                                         *
 * PURPOSE : All the nethavoc related APIs in netstorm                                                     * 
 * HISTORY : DATE               NAME                     CHANGES                                           *
 *           16th June 2020     Abhishek Pratap Singh    NH_SCENARIO keyword Enhancement                   * 
 ***********************************************************************************************************/

#include <pthread.h>
#include <sys/types.h>
#include <string.h>

#include "ns_nethavoc_handler.h"
#include "ns_data_types.h"
#include "ns_common.h"
#include "tmr.h"
#include "timing.h"
#include "ns_global_settings.h"
#include "nslb_get_norm_obj_id.h"
#include "ns_schedule_phases_parse.h"
#include "ns_kw_usage.h"
#include "ns_trans_parse.h"
#include "ns_schedule_phases.h"
#include "ns_error_msg.h"
#include "nslb_alert.h"
#include "ns_alloc.h"
#include "nslb_cav_conf.h"
#include "ns_log.h"

static NormObjKey nh_scenaio_norm_tbl;
static Nh_global_settings nethavoc_global_settings;
static NetstormAlert *nethavoc_alert; 

// total_nethavoc_scenario : The variable which stores the total no of nh_scenario are configured 
//                           in the netstorm scenario.
static int total_nethavoc_scenario = 0;
static char ns_protocol = -1; 
static unsigned short ns_port = 0; 

//TODO: Check all the macros 
//TODO: Make Cav err for all the scenarios 

/*=============================================================================================+
| API         : nethavoc_cleanup                                                               |
| Arguments   : NONE                                                                           |
| Return Type : void                                                                           |
| Purpose     : To stop all the nethavoc scenarios that are running at the end of the          |
|               netstorm test                                                                  |
+=============================================================================================*/
void nethavoc_cleanup()
{
  NSDL1_NH(NULL, NULL, "Method called");

  if(!nethavoc_global_settings.is_nethavoc_enable)
  {
    NSDL1_NH(NULL, NULL, "Nethavoc is not enabled. So returning...");
    return ;
  }
  NSDL1_NH(NULL, NULL, "Going to stop all netHavoc scenario");

  for(int i = 0; i<total_nethavoc_scenario; i++)
  {
    NSDL2_NH(NULL, NULL, "Stop api called, phase name = [%s]", global_settings->nethavoc_scenario_array[i].ns_phase_name);
    global_settings->nethavoc_scenario_array[i].stop_scenario_at_phase_end = 1;
    nethavoc_send_api(global_settings->nethavoc_scenario_array[i].ns_phase_name, NS_PHASE_END); 
  }
}

/*================================================================================================+
| API         : nethavoc_make_api_body                                                            |
| Arguments   : buffer - buffer to fill the body                                                  |
|               phase_state - phase state to find whether to run/stop the NH scenario             |
|               arr_idx     - index of the phase to get the particular                            |
| Return Type : buffer_len - the length of the body                                               |
+================================================================================================*/
static int nethavoc_make_api_body(char *buffer, int phase_state, int arr_idx)
{
  NSDL1_NH(NULL, NULL, "Method called, phase_state = [%d], arr_idx = [%d]", phase_state, arr_idx);

  char scenario_operation[64];
  char time_mode[64];
  int  buffer_len;

  if(phase_state == NS_PHASE_END)  
    sprintf(scenario_operation, "STOP");
  else
    sprintf(scenario_operation, "RUN");

  if(global_settings->nethavoc_scenario_array[arr_idx].delay)
    sprintf(time_mode, "Specified");
  else
    sprintf(time_mode, "Current");
  
  NSDL2_NH(NULL, NULL, "global_settings->nethavoc_scenario_array[%d].nh_scenario_name = [%s], testidx = [%d], timeMode = [%s], NSDomainIP = [%s],"
                       "testRunOwner = [%s], netstormScenario = [%s], netstormPhases = [%s], scenarioDuration = [%d], NHScenarioOperation = [%s]", 
                       arr_idx, global_settings->nethavoc_scenario_array[arr_idx].nh_scenario_name, testidx, time_mode, g_cavinfo.NSAdminIP, g_ns_login_user,
                       g_scenario_name, global_settings->nethavoc_scenario_array[arr_idx].ns_phase_name,global_settings->nethavoc_scenario_array[arr_idx].delay,
                       scenario_operation);

  buffer_len = sprintf(buffer, "{\"scenarioName\":\"%s\",\"testRun\":\"%d\",\"timeMode\":\"%s\",\"nsDomainIP\":\"%s\",\"testRunOwner\":\"%s\","
                               "\"netstormScenario\":\"%s\",\"netstormPhases\":\"%s\",\"scenarioDuration\":\"%d\",\"nhScenarioOperation\":\"%s\","
                               "\"controllerProtocol\":\"%d\",\"controllerPort\":\"%hu\"}", 
                               global_settings->nethavoc_scenario_array[arr_idx].nh_scenario_name, testidx, time_mode, g_cavinfo.NSAdminIP, g_ns_login_user, 
                               g_scenario_name, global_settings->nethavoc_scenario_array[arr_idx].ns_phase_name,global_settings->nethavoc_scenario_array[arr_idx].delay
                               ,scenario_operation, ns_protocol, ns_port);
  return buffer_len;
}

//g_cavinfo.NSAdminIP wrong in cav.conf
//If ns alert are disabled then nh alert should be send 
/*================================================================================================+
| API         : nethavoc_send_api                                                                 |
| Arguments   : phase_name  - ns phase for which send api is called                               | 
|               phase_state - starting/completion of the phase(PHASE_START = 1, NS_PHASE_END = 2)    |
| Return Type : 0                                                                                 |
+================================================================================================*/
int nethavoc_send_api(char *phase_name, int phase_state)
{
  NSDL1_NH(NULL, NULL, "Method called, phase_name = [%s], phase_state = [%d]", phase_name, phase_state);

  int arr_idx;
  int len;

  if(!nethavoc_global_settings.is_nethavoc_enable)
  {
    NSDL1_NH(NULL, NULL, "Nethavoc is not enabled. So returning...");
    return 0;
  }

  arr_idx = nslb_get_norm_id(&nh_scenaio_norm_tbl, phase_name, strlen(phase_name)); 

  if(arr_idx < 0)
  {
    NSDL1_NH(NULL, NULL, "NH scenario is not configured or stopped already for the phase [%s]", phase_name);
    return 0;
  }

  NSDL2_NH(NULL, NULL, "stop_scenario_at_phase_end = [%d]", global_settings->nethavoc_scenario_array[arr_idx].stop_scenario_at_phase_end);


  if((phase_state == NS_PHASE_END) && (!(global_settings->nethavoc_scenario_array[arr_idx].stop_scenario_at_phase_end)))
  {
    NSDL1_NH(NULL, NULL, "stop_scenario_at_phase_end is disabled for the phase [%s]", phase_name);
    return 0;
  }

  if(phase_state == NS_PHASE_END)
  {
    NSDL1_NH(NULL, NULL, "Deleting norm id for phase [%s], arr_idx = [%d]", phase_name, arr_idx);
    nslb_delete_norm_id_ex(&nh_scenaio_norm_tbl, phase_name, strlen(phase_name), &arr_idx);
  }

  NSDL2_NH(NULL, NULL, "arr_idx = [%d], phase name = [%s]", arr_idx, global_settings->nethavoc_scenario_array[arr_idx].ns_phase_name);
  char buffer[1024];


  len = nethavoc_make_api_body(buffer, phase_state, arr_idx); 

  NSDL3_NH(NULL, NULL, "Body length = [%d], Body = [%s]", len, buffer);
  NSDL2_NH(NULL, NULL, "Going to send alert to nethavoc engine");
  
  nslb_alert_send(nethavoc_alert, POST_REQUEST, APPLICATION_JSON, buffer, len);

  return 0;
}

/*================================================================================================+
| API         : nethavoc_init                                                                     |
| Arguments   : None                                                                              |
| Return Type : Void                                                                              |
| Usage       : It is use to initialise and configure nethavoc process, it is one time only       |
+================================================================================================*/
void nethavoc_init()
{
  NSDL1_NH(NULL, NULL, "Method called");
  
  if(!nethavoc_global_settings.is_nethavoc_enable)
  {
    NSDL1_NH(NULL, NULL, "Nethavoc is not enabled. So returning...");
    return ;
  }

  char url[1024];
  char trace_file[1024];

  sprintf(trace_file, "%s/logs/TR%d/nh_api_trace", getenv("NS_WDIR"), testidx);

  int  trace_fd = open(trace_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);

  NSDL3_NH(NULL, NULL, "trace_file = [%s], trace_fd = [%d]", trace_file, trace_fd);
  NSDL2_NH(NULL, NULL, "Going to init alert library for netHavoc");

  nethavoc_alert = nslb_alert_init(1, 1, 5, 20); 
 
  if(!nethavoc_alert)
    return; 

  sprintf(url, "/DashboardServer/netHavoc/NHScenarioDataService/operateNHScenario?token=%s", nethavoc_global_settings.token); 

  ns_port = global_settings->alert_info->enable_alert ? global_settings->alert_info->server_port : 0; 
  ns_protocol = global_settings->alert_info->enable_alert ? global_settings->alert_info->protocol : -1;

  NSDL2_NH(NULL, NULL, "Going to config alert library for netHavoc");
  NSDL3_NH(NULL, NULL, "nethavoc_global_settings.server_ip = [%s], nethavoc_global_settings.server_port = [%s], nethavoc_global_settings.protocol = [%d], url = [%s]", nethavoc_global_settings.server_ip, nethavoc_global_settings.server_port, nethavoc_global_settings.protocol, url);

  nslb_alert_config(nethavoc_alert, nethavoc_global_settings.server_ip, atoi(nethavoc_global_settings.server_port), nethavoc_global_settings.protocol, url, 5, 30, 300,trace_fd);
}

/*================================================================================================+
| API         : create_nethavoc_table_entry                                                       |
| Arguments   : entry_num     - the no of row which is added in the table                         |
|               total_entries - the total of entries that has been added in the table             |
|               max_entries   - maximum No of entries for which memory is malloced                |
|               ptr           - buffer pointer for memory reallocing and entry data               |
|               buffer_size   - size of the single buffer                                         |
|               name          - name of the pointer buffer for debug log                          |
| Return Type :  0  - success (memory is realloc/entry_num is incremented)                        |
|               -1 - failure (unable to realloc)                                                  |
+================================================================================================*/

static int create_nethavoc_table_entry(int *entry_num, int *total_entries, int *max_entries, char **ptr, int buffer_size, char *name)
{ 
  NSDL1_NH(NULL, NULL, "Method called, entry_num = [%d], total_entries = [%d], max_entries = [%d]", entry_num, *total_entries, *max_entries);

  if(*total_entries == *max_entries)
  { 
    MY_REALLOC_AND_MEMSET(*ptr, ((*max_entries + DELTA_ENTRIES) * buffer_size), (*max_entries * buffer_size), "*ptr", -1);
    if (!*ptr)
    { 
      NSDL1_NH(NULL, NULL, "Unable to realloc memory");
      return -1;
    }
    else
      *max_entries += DELTA_ENTRIES;
  }  
  *entry_num = (*total_entries)++;
  NSDL2_NH(NULL, NULL, "entry_num = [%d], total_entries = [%d], max_entries = [%d]", entry_num, *total_entries, *max_entries);

  return 0;
}

/*================================================================================================+
| API         : nh_scenario_add_key_in_hash_map                                                   |
| Arguments   : key - key for hash map (here phase name is the key)                               |
|               len - length of the key                                                           | 
|               scenario_name - nethavoc scenario name                                            |
|               is_new_key    - 1 if key doesn't exists                                           |
|                               0 if key exists already                                           |
| Return Type : index of the hash table                                                           | 
+================================================================================================*/
static int nh_scenario_add_key_in_hash_map(char *key, int len, char *scenario_name, int *is_new_key)
{ 
  NSDL1_NH(NULL, NULL, "Method called");

  static int init_done = 0;

  //init_done is the variable used to initialise the hash table for the first time 
  if(!init_done)
  { 
    NSDL2_NH(NULL, NULL, "Going to init hash table");
    nslb_init_norm_id_table_ex(&nh_scenaio_norm_tbl, NH_HASH_TABLE_SIZE);
    init_done = 1;
  }

  // is_new_key is the int variable which is used to distinguish if the key is newly added or not
  // is_new_key = 1 : key is newly added
  // is_new_key = 0 : key is already present
  NSDL2_NH(NULL, NULL, "key = [%s], len = [%d], scenario_name = [%s]", key, len, scenario_name);
  return(nslb_get_or_set_norm_id(&nh_scenaio_norm_tbl, key, len, is_new_key)); 
}

/*================================================================================================+
| API         : check_phase_name_validation                                                       | 
| Arguments   : phase_name - phase name that has to be validate                                   |
| Return Type :  0 - if phase name is scheduled in SCHEDULE keyword                               |
|               -1 - if phase name is not scheduled in SCHEDULE keyword                           |
+================================================================================================*/
static int check_phase_name_validation(char *phase_name)
{
  NSDL1_NH(NULL, NULL, "Method called, phase_name = [%s]", phase_name);

  int    i,j;
  Schedule *schedule;

  NSDL2_NH(NULL, NULL, "schedule_by = [%d]", global_settings->schedule_by);

  //check if scenario is scheduled by scenario/group
  if(global_settings->schedule_by == SCHEDULE_BY_SCENARIO) 
  {
    schedule = scenario_schedule;
    for(i=0; i<schedule->num_phases; i++)
    {
      if((!strcmp(phase_name, schedule->phase_array[i].phase_name)))
      {
        NSDL3_NH(NULL, NULL, "Phase[%s] is found in global settings", phase_name);
        return 0;
      }
    }
  } 
  else 
  { 
    schedule = &(group_schedule[0]);
    for(j=0; j<total_runprof_entries; j++)
    {
      for(i=0; i<group_schedule[j].num_phases; i++)
      {
        if((!strcmp(phase_name, group_schedule[j].phase_array[i].phase_name)))
        {
          NSDL3_NH(NULL, NULL, "Phase[%s] is found in global settings", phase_name);
          return 0;
        }
      }
    }
  }  
  NSDL1_NH(NULL, NULL, "Phase[%s] Not found in global settings", phase_name);
  return INVALID_PHASE_NAME;
}

/*===================================================================================================+
| API         : parse_nh_scenario_keyword                                                            |
| Arguments   : buf                           - the keyword buffer is passed                         |
|               err_msg                       - err_msg is generated at special cases                |
|               nh_scenario_name              - nethavoc scenario name                               |
|               phase_name                    - NS phase name                                        |
|               delay                         - delay in nh scenario execution                       |
|               is_stop_scenario_at_phase_end - whether to stop the nh scenario at the end or not    |
| Return Type : exit or 0                                                                            |
+===================================================================================================*/
static int parse_nh_scenario_keyword(char *buf, char *err_msg, char *nh_scenario_name, char *phase_name, int delay, int is_stop_scenario_at_phase_end)
{ 
  NSDL1_NH(NULL, NULL, "Method called, nh_scenario_name = [%s], phase_name = [%s], delay = [%d], is_stop_scenario_at_phase_end = [%d]", 
                        nh_scenario_name, phase_name, delay, is_stop_scenario_at_phase_end);

  int arr_idx;
  int is_new_key;
  static int row_num;
  static int total_row;
  static int max_row;

  if(create_nethavoc_table_entry(&row_num, &total_row, &max_row,(char **) &(global_settings->nethavoc_scenario_array), sizeof(Nh_scenario_settings), phase_name) == -1)
  { 
    NSDL1_NH(NULL, NULL, "Error in allocating more memory for phase '%s'.", phase_name);
    NS_KW_PARSING_ERR(buf, 0, err_msg, NH_SCENARIO_USAGE, CAV_ERR_1011296, "Error in allocating more memory for phase '%s'.", phase_name);
  }
  
  //adding hash to the hash table: key is phase name
  arr_idx = nh_scenario_add_key_in_hash_map(phase_name, strlen(phase_name), nh_scenario_name, &is_new_key);
  NSDL2_NH(NULL, NULL, "arr_idx = [%d]", arr_idx);
   
  //filling the data to Nh_settings for the every NH_SCENARIO Keyword 
  strncpy(global_settings->nethavoc_scenario_array[arr_idx].nh_scenario_name, nh_scenario_name, 64);
  strncpy(global_settings->nethavoc_scenario_array[arr_idx].ns_phase_name, phase_name, 64);
  global_settings->nethavoc_scenario_array[arr_idx].delay = delay;
  global_settings->nethavoc_scenario_array[arr_idx].stop_scenario_at_phase_end = is_stop_scenario_at_phase_end;

  //incrementing the count of nethavoc scenario
  total_nethavoc_scenario++;

  NSDL2_NH(NULL, NULL, "nh_scenario_name = [%s], ns_phase_name = [%s], delay = [%d], stop_scenario_at_phase_end = [%d]", 
           global_settings->nethavoc_scenario_array[arr_idx].nh_scenario_name, global_settings->nethavoc_scenario_array[arr_idx].ns_phase_name, 
           global_settings->nethavoc_scenario_array[arr_idx].delay, global_settings->nethavoc_scenario_array[arr_idx].stop_scenario_at_phase_end);
  return 0;
}

/*================================================================================================+
| API         : nh_get_home_dir                                                                   |
| Arguments   : home_dir - buffer to fill the home directory                                      |
| Return Type : void                                                                              |
| Usage       : to fill the home directory to the buffer                                          |
+================================================================================================*/
static void nh_get_home_dir(char *home_dir)
{
  NSDL1_NH(NULL, NULL, "Method called");

  //get user working dir
  strcpy(home_dir, getenv("NS_WDIR"));
  if(home_dir == NULL)
    strncpy(home_dir, "/home/cavisson/work", 128);

  NSDL2_NH(NULL, NULL, "home_dir= [%s]", home_dir);
}

/*================================================================================================+
| API         : parse_nh_conf_file                                                                | 
| Arguments   : buf     - the keyword buffer is passed                                            |
|               err_msg - the buffer for err_msg                                                  |
| Return Type : 0 - disable nethavoc                                                              |
|               1 - enable nethavoc                                                               |
+================================================================================================*/
static int parse_nh_conf_file(char *buf, char *err_msg)
{
  NSDL1_NH(NULL, NULL, "Method called");
  FILE *fp_nh_conf;
  char nh_conf_file_path[MAX_LOCAL_BUFF_SIZE];
  int  num;

  char buff[MAX_BUFF_SIZE + 1]            ;
  char keyword[MAX_LOCAL_BUFF_SIZE]       ;
  char server_ip[MAX_LOCAL_BUFF_SIZE]     ;
  char server_port[MAX_LOCAL_BUFF_SIZE]   ;
  char token[TOKEN_BUFF_SIZE + 1]         ;  
  char topology[TOKEN_BUFF_SIZE + 1]      ;  
  char pattern[TOKEN_BUFF_SIZE + 1]       ;  
  char nh_home_dir[MAX_LOCAL_BUFF_SIZE]   ;  
  char future_use;
  int protocol;

  nh_get_home_dir(nh_home_dir);

  sprintf(nh_conf_file_path, "%s/netHavoc/conf/netHavoc.conf", nh_home_dir);

  if((fp_nh_conf = fopen(nh_conf_file_path, "r")) == NULL)
  {
    NSDL2_NH(NULL, NULL,"nh_conf_file_path = [%s], nh_home_dir = [%s]", nh_conf_file_path, nh_home_dir); 
    return DISABLE_NETHAVOC;
  }

  while(nslb_fgets(buff, MAX_BUFF_SIZE, fp_nh_conf, 0))
  {
    // Replace new line by Null 
    buff[strlen(buff) - 1] = '\0'; 

    //ignore comented and blank line
    if((buff[0] == '#') || buff[0] == '\0')
      continue; 
 
    NSDL4_NH(NULL, NULL,"buff = [%s]", buff); 

    //KEYWORD: NH_SCENARIO_INTEGRATION <IP> <PORT> <PROTOCOL> <TOKEN> <TOPOLOGY> <PATTERN>
    num = sscanf(buff, "%s %s %s %d %s %s %s %c", keyword, server_ip, server_port, &protocol, token, topology, pattern, &future_use);
    
    if(!strcmp(keyword, "NH_SCENARIO_INTEGRATION"))
    {
      NSDL2_NH(NULL, NULL,"keyword = [%s], server_ip = [%s], server_port = [%s], protocol = [%d], token =[%s]", keyword, server_ip, server_port, protocol,  token); 
      if(num < 8)
      {
        NS_KW_PARSING_ERR(buf, 0, err_msg, NH_SCENARIO_INTEGRATION_USAGE, CAV_ERR_1011292, 
                          "Wrong NH_SCENARIO_INTEGRATION entry in %s" , nh_conf_file_path);
      }
      snprintf(nethavoc_global_settings.server_ip,  MAX_LOCAL_BUFF_SIZE, "%s", server_ip);
      snprintf(nethavoc_global_settings.server_port, MAX_LOCAL_BUFF_SIZE, "%s", server_port); 
      snprintf(nethavoc_global_settings.token, TOKEN_BUFF_SIZE, "%s", token); 
      nethavoc_global_settings.protocol = protocol;
     
      NSDL2_NH(NULL, NULL,"server_ip = [%s], server_port = [%s], protocol = [%d], token =[%s]", nethavoc_global_settings.server_ip, nethavoc_global_settings.server_port, nethavoc_global_settings.protocol, nethavoc_global_settings.token); 
      return ENABLE_NETHAVOC;
    }
  } 
  fclose(fp_nh_conf);
  NSDL2_NH(NULL, NULL,"Nethavoc is disbled"); 
  return DISABLE_NETHAVOC;
}

/*================================================================================================+
| API         : kw_set_nh_scenario                                                                |
| Arguments   : buf     - the keyword buffer is passed                                            |
|               err_msg - the buffer for err_msg                                                  |
| Return Type : void                                                                              | 
+================================================================================================*/
int kw_set_nh_scenario(char *buf, char *err_msg)
{
  NSDL1_NH(NULL, NULL, "Method called");
  int  num;
  char keyword[NH_PHASE_NAME_SIZE + 1];
  char nh_scenario_name[MAX_DATA_LINE_LENGTH];
  char phase_name[NH_PHASE_NAME_SIZE + 1];
  char phase_type[NH_PHASE_NAME_SIZE + 1];
  int  delay;
  int is_stop_scenario_at_phase_end;
 
  if(!nethavoc_global_settings.is_nethavoc_enable)
  {
    NSDL2_NH(NULL, NULL,"going to parse nh conf file"); 

    if(parse_nh_conf_file(buf, err_msg) == DISABLE_NETHAVOC)
    {
      NSDL2_NH(NULL, NULL, "nethavoc is disable"); 
      return 0;
    }

    nethavoc_global_settings.is_nethavoc_enable = ENABLE_NETHAVOC;
    NSDL2_NH(NULL, NULL, "nethavoc is enable"); 
  }
 
  num = sscanf(buf, "%s %s %s %d %d %s", keyword, nh_scenario_name, phase_name, &delay, &is_stop_scenario_at_phase_end, phase_type);

  if(num < NUM_MANDATORY_VALUES)
  {
    NS_KW_PARSING_ERR(buf, 0, err_msg, NH_SCENARIO_USAGE, CAV_ERR_1011296, CAV_ERR_MSG_1);
  }

  NSDL2_NH(NULL, NULL, "keyword = [%s], nh_scenario_name = [%s],phase_name = [%s], delay = [%d], is_stop_scenario_at_phase_end =[%d], ", keyword, nh_scenario_name,phase_name, delay, is_stop_scenario_at_phase_end);

  if(check_phase_name_validation(phase_name) == -1)
  {
    NS_KW_PARSING_ERR(buf, 0, err_msg, NH_SCENARIO_USAGE, CAV_ERR_1011296,
                      "Provided phase name is not scheduled with SCHEDULE keyword.");
  }

  if(strlen(phase_name) > NH_PHASE_NAME_SIZE)
  {
    NS_KW_PARSING_ERR(buf, 0, err_msg, NH_SCENARIO_USAGE, CAV_ERR_1011296,
                      "Provided phase name is invalid. Length can be greater than 48 characters.");
  }

  if (match_pattern(phase_name, "^[a-zA-Z][a-zA-Z0-9_]*$") == 0)
  {
    NS_KW_PARSING_ERR(buf, 0, err_msg, NH_SCENARIO_USAGE, CAV_ERR_1011296, "Phase name '%s' is invalid. Phase name should contain "
                      "only alphanumeric character, and first character must be alpha, other characters can be alpha, numeric or underscore", phase_name);
  }

  parse_nh_scenario_keyword(buf, err_msg, nh_scenario_name, phase_name, delay, is_stop_scenario_at_phase_end);
 
  return 0;
}
