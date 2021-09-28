/******************************************************************
 * Name    : ns_monitor_profiles.c
 * Author  : Archana
 * Purpose : This file contains methods to read keywords from 
             monitor profile file for MONITOR_PROFILE
 * Note:
 * MONITOR_PROFILE <monitor profile name>
     This monitor profile may contains following keywords:
       - SERVER_STATS
       - SERVER_PERF_STATS
       - CUSTOM_MONITOR
       - STANDARD_MONITOR
       - CHECK_MONITOR
       - SERVER_SIGNATURE
       - MONITOR
       - NETOCEAN_MONITOR
       - USER_MONITOR
       - DYNAMIC_VECTOR_MONITOR
     This monitor profile should be in $NS_WDIR/scenarios/
 * Modification History:
 * 05/09/08 - Initial Version
 * 17/11/09 - Last modification date 
 * 28/03/2013 - Manish Kr. Mishra: support parsing of keywords ENABLE_NS_MONITORS, ENABLE_NO_MONITORS, DISABLE_NS_MONITORS, DISABLE_NO_MONITORS  
*****************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <regex.h>
#include <libgen.h>
#include "url.h"
#include "v1/topolib_structures.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"

#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "ns_monitor_profiles.h"
#include "ns_custom_monitor.h"
#include "smon.h"
#include "util.h"
#include "ns_gdf.h"
#include "ns_log.h"
#include "ns_user_monitor.h"
#include "ns_standard_monitors.h"
#include "ns_dynamic_vector_monitor.h"
#include "ns_alloc.h"
#include "ns_server_admin_utils.h"
#include "nslb_util.h"
#include "nslb_cav_conf.h"
#include "netomni/src/core/ni_scenario_distribution.h"
#include "wait_forever.h"
#include "ns_event_id.h"
#include "ns_event_log.h"
#include "ns_string.h"
#include "ns_runtime_changes.h"
#include "ns_trace_level.h"
#include "ns_runtime_changes_monitor.h"
#include "ns_appliance_health_monitor.h"
#include "ns_custom_monitor_RDnew.h"
#include "ns_parse_scen_conf.h"
#include "ns_runtime.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"
#include "ns_monitor_init.h"
#include <v1/topolib_structures.h>
#include "ns_check_monitor.h"
#include "ns_pre_test_check.h"
#include "ns_batch_jobs.h"
extern int get_max_report_level_from_non_shr_mem();
#define DELTA_AUTO_MON_SERVER_ENTRIES 50
#define DELTA_CMON_AGENT_SERVER_ENTRIES 100  //This macro is for ENABLE_CMON_AGENT

//Adding or Deleting Automonitor NUM_AUTO_MON macro should be increased or decreased6-
#define NUM_AUTO_MON 61 //number of monitors in auto_mon_list.
                       //Note: If add any monitor in auto_mon_list then set this macro otherwise ther may be core dump

#define HIERARCHICAL_VIEW_VECTOR_SEPARATOR_OFF '_'
#define HIERARCHICAL_VIEW_VECTOR_SEPARATOR_ON '>'

char g_java_mon_flag_cmon =0;
int g_auto_ns_mon_flag = 0, g_auto_no_mon_flag = 0, g_cmon_agent_flag = 0, g_auto_server_sig_flag = 0, g_disable_dvm_vector_list, g_enable_recreate_parent_epoll = 0, g_enable_iperf_monitoring = 0,  g_iperf_monitoring_port = 0;
int total_auto_mon_server_entries = 0, total_cmon_agent_server_entries = 0, max_auto_mon_server_entries = 0, max_cmon_agent_server_entries = 0;

char g_generator_process_monitoring_flag;
char g_tsdb_configuration_flag = 0;

int g_cmon_port ; //variable to store cmon port 
//int g_num_no_server = 0; //This will trace number of NOAppliances, If we have more than one NOAppliance then we need different vector names
                         //so we are taking this globle variable. 
AutoMonTable *g_auto_mon_table = NULL;
AutoMonSvrTable *g_auto_mon_server_table = NULL;
AutoMonSvrTable *g_cmon_agent_server_table = NULL;

const char *auto_mon_list[] = {"SystemStatsExtended", "TcpStatsRate", "NetworkBandwidth", "TcpStatesCountV3", "UpTimeEx", "SockStats", "MPStats", "DiskStats", "SimulatedServiceStats", "SimulatedProtocolStats","SERVER_SIGNATURE", "JavaProcessMonitoring", "CMON", "LPS", "DBUpload1", "NLR", "NLW", "NLM", "NDP", "DBUpload2", "NDC", "NCMain", "DeviceStats", "PostgreSQLExecutionStats","PostgreSQLBlockingStats","PostgreSQLTempDBStats","PostgreSQLLocksStats", "PostgreSQLSessionStats", "PostgreSQLConnectionStats", "PostgreSQLAccesslog", "NetworkDelay", "PostgreSQLDbConnectionStatistics","PostgreSQLBGWriterStats","PostgreSQLDBActivityStats","PostgreSQLQueryResponseTime","PostgreSQLDbTableHitRatio","PostgreSQLIOActivityStats","PostgreSQLLockModeStats" , "HPD", "TOMCAT", "NVDU", "EthStat", "ProcessCountByState", "MemStatsEx", "JMX_CMON", "PsIoStatNDC", "PsIoStatCMON", "PsIoStatLPS", "PsIoStatNDP", "PsIoStatTOMCAT", "PsIoStatNDEMain", "PsIoStatDBUpload1", "PsIoStatDBUpload2", "PsIoStatNLR", "PsIoStatNLW", "PsIoStatNLM", "PsIoStatHPD", "PsIoStatNVDU", "JMX_TOMCAT", "AccessLogStatsExtendedV6" , "DOCKER"};

static char *get_cur_time()
{
  time_t    tloc;
  struct  tm *lt, tm_struct;
  static  char cur_time[100];

  (void)time(&tloc);
  if((lt = nslb_localtime(&tloc, &tm_struct, 1)) == (struct tm *)NULL)
    strcpy(cur_time, "Error");
  else
    sprintf(cur_time, "%02d:%02d:%02d", lt->tm_hour, lt->tm_min, lt->tm_sec);
  return(cur_time);
}

// Following function is checking whether the gdf added during runtime is valid or not  
static int check_gdf_existence(char *buf, int flag, char *wdir_path)
{
  char sys_path[4*MAX_DATA_LINE_LENGTH] = {0};
  char keyword[MAX_DATA_LINE_LENGTH] = {0};
  char gdf_name[MAX_DATA_LINE_LENGTH] = {0};
  char server[MAX_DATA_LINE_LENGTH] = {0};
  char vector_name[MAX_DATA_LINE_LENGTH] = {0};
  
  if(flag == 0) // called for custom monitor
  sscanf(buf, "%s %s %s %s", keyword, server, gdf_name, vector_name);
  else // called for dynamic vector monitor
  sscanf(buf, "%s %s %s %s", keyword, server, vector_name, gdf_name);

  if(!strncmp(gdf_name, "NA_", 3))
    return 0;

  sprintf( sys_path, "%s/sys/%s", wdir_path, gdf_name);

  if(access( sys_path, F_OK ) == -1)
  {
    fprintf(stderr, "Error in opening %s gdf : %s for vector %s  \n", keyword, gdf_name, vector_name);
    return -1;
  }

  return 0;    
}

//Vector separator in hierarchical format

//Using is_auto_mon flag for DVM vector prefix while converting SM->DVM
inline int read_keywords_frm_monitor_profile(char *file_name, FILE* fp, int is_auto_mon, int runtime_flag, char *err_msg, int *mon_status, char *path)
{
  char buff[MAX_MONITOR_BUFFER_SIZE + 1] = {0};
  char text[MAX_DATA_LINE_LENGTH] = {0};
  char keyword[MAX_DATA_LINE_LENGTH] = {0};
  int num, ret, success_flag = 0, error_flag = 0;
  char *buf = NULL;
  
 
  NSDL2_SCHEDULE(NULL, NULL, "Method called, Monitor Profile Name = %s", file_name);
  while (nslb_fgets(buff, MAX_MONITOR_BUFFER_SIZE, fp, 0) != NULL)
  {
    ret = 0;
    memset(err_msg, 0, strlen(err_msg)); //REMOVE strlen
    buff[strlen(buff)-1] = '\0';
    buf = buff;
    CLEAR_WHITE_SPACE(buf);
    CLEAR_WHITE_SPACE_FROM_END(buf);

    if((buf[0] == '#') || (buf[0] == '\0'))
      continue;

    NSDL4_SCHEDULE(NULL, NULL, "buf = [%s]", buf);

    if ((num = sscanf(buf, "%s %s", keyword, text)) != 2)
    {
      NSDL4_SCHEDULE(NULL, NULL, "Monitor profile formate is wrong. At least two fields required <%s>", buf);
      error_log("At least two fields required <%s>", buf);
      continue;
    }
    else
    {
      NSDL3_SCHEDULE(NULL, NULL, "Read %s from %s", buf, file_name);

      if (strncasecmp(keyword, "SERVER_STATS", strlen("SERVER_STATS")) == 0) 
        get_server_perf_stats(buf); // Achint 03/01/2007 - Add this function to get all server IP Addresses
      else if (strcasecmp(keyword, "CUSTOM_MONITOR") == 0 || 
		strcasecmp(keyword, "SPECIAL_MONITOR") == 0 || 
		strcasecmp(keyword, "LOG_MONITOR") == 0) 
      {
        //on runtime first check whether gdf exists or not
        if(runtime_flag)
          ret = check_gdf_existence(buf, 0, path);
        //if gdf is valid then do further monitor parsing
        if(ret == 0) 
        {
          monitor_added_on_runtime = 1;
          ret = custom_monitor_config("CUSTOM_MONITOR", buf, NULL, 0, runtime_flag, err_msg, NULL, NULL, 0);

          if((ret >= 0) && (strcasecmp(keyword, "LOG_MONITOR") != 0))
            g_mon_id = get_next_mon_id();
        }
      }
      else if(strcasecmp(keyword, "STANDARD_MONITOR") == 0)
        ret = kw_set_standard_monitor(keyword, buf, runtime_flag, NULL, err_msg, NULL);
      else if (strcasecmp(keyword, "PRE_TEST_CHECK") == 0) 
        printf("Keyword 'PRE_TEST_CHECK' no Longer exist, it has been renamed as 'CHECK_MONITOR'\n");
      else if (strcasecmp(keyword, "CHECK_MONITOR") == 0)
        ret = kw_set_check_monitor(keyword, buf, runtime_flag, err_msg, NULL);
      else if (strcasecmp(keyword, "SERVER_SIGNATURE") == 0) 
      {
        if (runtime_flag)
        {
          strcat(err_msg, "SERVER_SIGNATURE cannot be added on runtime.");     
          ret = -1;          
        } 
        else
          ret = kw_set_server_signature(keyword, buf, runtime_flag, err_msg, NULL);
      }
      else if (strcasecmp(keyword, "USER_MONITOR") == 0)
        user_monitor_config(keyword, buf);
      else if (strcasecmp(keyword, "DYNAMIC_VECTOR_MONITOR") == 0)
      {
        //on runtime first check whether gdf exists or not
        if(runtime_flag)
          ret = check_gdf_existence(buf, 1, path);
        //if gdf is valid then do further monitor parsing
        if(ret == 0)
          ret = kw_set_dynamic_vector_monitor(keyword, buf, NULL, 0, runtime_flag, NULL, err_msg, NULL, NULL, 0);
      }
    }

    if(ret < 0)
    {
      error_flag = 1;
      if(runtime_flag)
      {
        NS_EL_2_ATTR(EID_RUNTIME_CHANGES_ERROR, -1,
                                -1, EVENT_CORE, EVENT_INFORMATION,
                                keyword,
                                buf,
                                "Error in applying runtime changes.\n%s", err_msg);
      }
      else
      {
        NS_EL_2_ATTR(EID_RUNTIME_CHANGES_ERROR, -1,
                                -1, EVENT_CORE, EVENT_INFORMATION,
                                keyword,
                                buf,
                                "Error in applying keyword\n%s", err_msg);

        fprintf(stderr, "%s: Error in applying Keyword = %s \"%s\"\n Error = %s\n", get_cur_time(), keyword, buf, err_msg);
      }
   }
   else
     success_flag = 1;

  }//End while loop

  NSDL2_SCHEDULE(NULL, NULL, "error_flag = %d, success_flag = %d", error_flag, success_flag);
  if(error_flag == 1 && success_flag == 1) //some mon passed some failed
  {
    if(mon_status != NULL)
      *mon_status = 1;
  }

  if(error_flag == 1 && success_flag == 0) //if all monitor failed
    return -1; 
  else                  //at least one mon passed
    return 0;
}

int kw_set_monitor_profile(char *profile_name, int num, char *err_msg, int runtime_flag, int *mon_status)
{
  FILE *fp;
  char wdir[1024];
  char file_name[2024];
  char path[1024] = {0};
  char cmd[4096]="\0";
  struct stat s;
  int ret = 0;
  
  if (num != 2)
  {
    NS_KW_PARSING_ERR("MONITOR_PROFILE", runtime_flag, err_msg, MONITOR_PROFILE_USAGE, CAV_ERR_1011359 ,"MONITOR_PROFILE");
  }

  char *ptr = NULL;
  if((ptr = strchr(profile_name, '\n')) != NULL)
    *ptr = '\0';  
  
  if (getenv("NS_WDIR") != NULL)
  {
    strcpy(wdir, getenv("NS_WDIR"));
    sprintf(path, "%s", wdir);

    /*  In case of hierarchical view, mprof should be kept in work/mprof/topology_name/ dir
     *  If mprof is not found here, NS will find in work/mprof dir  */ 
    sprintf(file_name, "%s/mprof/%s/%s.mprof", path, global_settings->hierarchical_view_topology_name, profile_name);
    if(stat(file_name, &s) != 0)
      sprintf(file_name, "%s/mprof/%s.mprof", path, profile_name);
  }
  else 
  {
    NSDL2_SCHEDULE(NULL, NULL, "NS_WDIR env variable is not set. Setting it to default value /home/cavisson/work/");
    sprintf(path, "/home/cavisson/work");
    sprintf(file_name, "%s/mprof/%s.mprof", path, profile_name);
  }
  NSDL3_SCHEDULE(NULL, NULL, "Reading keywords from %s file", file_name);
 
  fp = fopen(file_name, "r");
  if (fp == NULL)
  {
    error_log("Error in opening file %s", file_name);
    error_log("Error: MONITOR_PROFILE '%s.mprof' is not present in %s/mprof directory.", profile_name, wdir);
   
    if(runtime_flag)
      return -1;
    else
    {
      NS_KW_PARSING_ERR("MONITOR_PROFILE", runtime_flag, err_msg, MONITOR_PROFILE_USAGE, CAV_ERR_1060032, profile_name);
    }
  }

  //On runtime, copy mprof to testrun here only, becoz rght nw we are not saving mprof name anywhere
  if(runtime_flag)
  {
    sprintf(cmd, "cp %s %s/logs/%s/ns_files/runtime_%s.mprof 2>/dev/null", file_name, path, global_settings->tr_or_common_files, profile_name);
    nslb_system(cmd,1,err_msg);
  }

  ret = read_keywords_frm_monitor_profile(file_name, fp, 0, runtime_flag, err_msg, mon_status, path);
  CLOSE_FP(fp);
  return ret;
}

/***************************************************************************************************************
 * Name    : create_auto_mon_server_table
 *
 * Purpose : This function will create a server table for the servers for which we have to add monitors.
 *
 * Input   : It will take address of variable and at that address fill the server index.
 *
 * Output  : 0 - on success
 *          -1 - on failure
 **************************************************************************************************************/
static int create_auto_mon_server_table(int *row_num) 
{
  NSDL1_SCHEDULE(NULL, NULL, "Method called, "
                             "total_auto_mon_server_entries = %d, max_auto_mon_server_entries = %d", 
                              total_auto_mon_server_entries, max_auto_mon_server_entries);

  if (total_auto_mon_server_entries == max_auto_mon_server_entries) {
    MY_REALLOC_EX(g_auto_mon_server_table, (total_auto_mon_server_entries + DELTA_AUTO_MON_SERVER_ENTRIES) * sizeof(AutoMonSvrTable), max_auto_mon_server_entries * sizeof(AutoMonSvrTable), "auto mon", -1);
      
    max_auto_mon_server_entries += DELTA_AUTO_MON_SERVER_ENTRIES;
  }

  if(g_auto_mon_server_table == NULL)
  {
    NSTL1_OUT(NULL, NULL, "Error: Memory allocation failed for g_auto_mon_server_table.\n");
    return MON_FAILURE;
  }

  *row_num = total_auto_mon_server_entries++;

  NSDL2_SCHEDULE(NULL, NULL, "row = %d", *row_num);  

  return MON_SUCCESS; //success
}

static int create_cmon_agent_server_table(int *row_num) 
{
  NSDL1_SCHEDULE(NULL, NULL, "Method called, "
			     "total_cmon_agent_server_entries = %d, max_cmon_agent_server_entries = %d, g_cmon_agent_flag = %d", 
			      total_cmon_agent_server_entries, max_cmon_agent_server_entries, g_cmon_agent_flag);

  if (total_cmon_agent_server_entries == max_cmon_agent_server_entries) {
      MY_REALLOC_EX(g_cmon_agent_server_table, (total_cmon_agent_server_entries + DELTA_CMON_AGENT_SERVER_ENTRIES) * sizeof(AutoMonSvrTable), max_cmon_agent_server_entries * sizeof(AutoMonSvrTable), "cmon agent server table", -1);
      
    max_cmon_agent_server_entries += DELTA_CMON_AGENT_SERVER_ENTRIES;
  }

  if(g_cmon_agent_server_table == NULL)
  {
    NSTL1_OUT(NULL, NULL, "Error: Memory allocation failed for g_cmon_agent_server_table.\n");
    return MON_FAILURE;
  }

  *row_num = total_cmon_agent_server_entries++;

  NSDL2_SCHEDULE(NULL, NULL, "row = %d", *row_num);  

  return MON_SUCCESS; //success
}

/***************************************************************************************************************
 * Name    : create_auto_mon_table
 *
 * Purpose : This function will create monitor table and initialise that.
 *
 * Input   : NONE 
 *
 * Output  : 0 - on Success
 *          -1 - on Failure 
 **************************************************************************************************************/
static int create_auto_mon_table()
{
  int i;

  NSDL2_SCHEDULE(NULL, NULL, "Method Called."); 

  //Since this function is called from both ENABLE_NS_MONITORS and ENABLE_NO_MONITORS so we need to create monitor table only onces  
  if(g_auto_mon_table != NULL)
    return MON_SUCCESS; 

  MY_MALLOC(g_auto_mon_table, NUM_AUTO_MON * sizeof(AutoMonTable), "Auto Monitor Table", -1);
  
  if(g_auto_mon_table == NULL)
  {
    NSTL1_OUT(NULL, NULL, "Error: memory allocation failed for g_auto_mon_table.\n");
    return MON_FAILURE;
  }  

  memset(g_auto_mon_table, 0, NUM_AUTO_MON * sizeof(AutoMonTable));

  //Initializing monitor table 
  for(i = 0; i < NUM_AUTO_MON; i++)
  {
    NSDL2_SCHEDULE(NULL, NULL, "Copying monitor name '%s' in mon table on index %d", auto_mon_list[i], i); 
    strcpy(g_auto_mon_table[i].mon_name, auto_mon_list[i]); 
    g_auto_mon_table[i].ns_state = ACTIVATE; 
    g_auto_mon_table[i].no_state = ACTIVATE; 
  }
  
  //MON TABLE DUMP 
  NSDL2_SCHEDULE(NULL, NULL, "MON table DUMP---");
  for(i = 0; i < NUM_AUTO_MON; i++)
  {
    NSDL2_SCHEDULE(NULL, NULL, "g_auto_mon_table[%d].mon_name = [%s], g_auto_mon_table[%d].ns_state = [%d], g_auto_mon_table[%d].no_state = [%d]", 
                                i, g_auto_mon_table[i].mon_name, i, g_auto_mon_table[i].ns_state, g_auto_mon_table[i].no_state);
  }
  
  return MON_SUCCESS;
}

/***************************************************************************************************************
 * Name    : add_auto_mon_server 
 *
 * Purpose : This function will create server table by calling function create_auto_mon_server_table() and
 *           set their members. 
 *
 * Input   : NONE 
 *
 * Output  : 0 - on success
 *          -1 - on failure
 **************************************************************************************************************/
static int add_auto_mon_server(char *server_name, char *vname, int svr_type, char *controller_name)
{
  char *field[3];
  char buf[256];
  char *ptr;
  int row_num = -1;

  NSDL2_SCHEDULE(NULL, NULL, "Method Called, server_name = [%s], vname = [%s]", server_name, vname);

  if(create_auto_mon_server_table(&row_num) == MON_FAILURE)
    return MON_FAILURE;

  //Fill auto mon server list
  strcpy(g_auto_mon_server_table[row_num].server_name, server_name);
  strcpy(g_auto_mon_server_table[row_num].vector_name, vname);
  strcpy(buf, vname);

  //This is done as when enable store config keyword we will recieve "store!" in the vector name as we wnat only Cavisson><Appliance>
  if(enable_store_config == 1)
  {
    ptr=strchr(buf,'!');
    get_tokens(++ptr,field, &(global_settings->hierarchical_view_vector_separator), 3);
    sprintf(g_auto_mon_server_table[row_num].tier_server,"%s%c%s",field[0],global_settings->hierarchical_view_vector_separator,field[1]);
  }
  else
  {
    strcpy(g_auto_mon_server_table[row_num].tier_server, vname);
  }

  if(controller_name &&  (controller_name[0] != 0))
    strcpy(g_auto_mon_server_table[row_num].controller_name, controller_name);
  else
    strcpy(g_auto_mon_server_table[row_num].controller_name, "work");

  g_auto_mon_server_table[row_num].server_type = svr_type;

  NSDL2_SCHEDULE(NULL, NULL, "Search and Add Server into server list");
  add_server_in_server_list(server_name, g_auto_mon_server_table[row_num].tier_server,topo_idx);

  return MON_SUCCESS;
}

static int add_cmon_agent_server(char *server_name, char *vname, char *cavmon_home, int svr_type, char *server_disp_name)
{
  int row_num = -1;
  char *ptr = NULL;
  char tier_server[MAX_NAME_LEN] = {0};

  NSDL2_SCHEDULE(NULL, NULL, "Method Called, server_name = [%s], vname = [%s]", server_name, vname);

  if(create_cmon_agent_server_table(&row_num) == MON_FAILURE)
    return MON_FAILURE;

  //Fill cmon agent server list
  strcpy(g_cmon_agent_server_table[row_num].server_name, server_name);
  strcpy(g_cmon_agent_server_table[row_num].vector_name, vname);

  if(g_auto_server_sig_flag && !g_cmon_agent_flag)
  {
    strcpy(g_cmon_agent_server_table[row_num].tier_server,vname);
  }
  else
  {
    strcpy(tier_server, vname);
    ptr = strrchr(tier_server, global_settings->hierarchical_view_vector_separator);
    *ptr = '\0';
    strcpy(g_cmon_agent_server_table[row_num].tier_server, tier_server);
  }

  strcpy(g_cmon_agent_server_table[row_num].cavmon_home, cavmon_home);

  g_cmon_agent_server_table[row_num].server_type = svr_type;

  return MON_SUCCESS;
}

/***************************************************************************************************************
 * Name    : kw_set_enable_no_monitors 
 *
 * Purpose : 1) This function will Keyworld ENABLE_NO_MONITORS form scenario. 
 *              Syntax -
 *                ENABLE_NO_MONITORS  <mode> <server_name> <controller_name>
 *                  mode - 1 : active all the monitors [default]
 *                         0 : inactive all the monitors 
 *
 *           2) This function create two tables.
 *                  (i) AutoMonTable (if not exist)   (ii) AutoMonSvrTable
 *                  
 *                  g_auto_mon_table }               g_auto_mon_server_table
 *                   --------------- .                ---------------
 *                 0 |monitor name | .<-------------- |server_name  |
 *                   |ns_state     | .                |vector_name  |
 *                   |no_state     |                  |server_tpye  |
 *                   --------------- .                ---------------
 *                 1 |             | .<-------------- |             |
 *                   |             | .                |             |
 *                   --------------- .                ---------------
 *                 2 |             | .<-------------- |             |
 *                   |             | .                |             |
 *                   --------------- .                ---------------
 *                 3 |             | }                |             |
 *                   
 *          3) We will automate all the monitors which are in AutoMonTable for every server in AutoMonSvrTable
 *             Eg: 
 *              For Server1 -> mon1, mon2, mon3.... (all in AutoMonTable)
 *                  Server2 -> mon1, mon2, mon3.... (all in AutoMonTable)
 *
 * Input   : keyword - ENABLE_NO_MONITORS
 *           buf     - ENABLE_NO_MONITORS  <mode> <server_name> <controller_name>
 *
 * Output  : exit - on failure 
 **************************************************************************************************************/
int kw_set_enable_no_monitors(char *keyword, char *buf)
{
  char key[MAX_NAME_LEN];
  char state[MAX_NAME_LEN] = {0};
  char server_ip[MAX_NAME_LEN] = {0};
  char server_name[MAX_NAME_LEN] = {0};
  char controller_name[MAX_NAME_LEN] = {0};
  char vector_prefix[3*MAX_NAME_LEN] = {0};
  char tiername[MAX_NAME_LEN] = {0};
  char err_msg[MAX_NAME_LEN] = {0};
  int auto_mon_state = 1;
  int num = 0, port = 7891;
  static int loopBack_add_done = 0;
  char *ptr = NULL;

  NSDL2_SCHEDULE(NULL, NULL, "Method called, keyword = %s, buf = %s", keyword, buf);

  key[0] = 0, server_ip[0] = 0, state[0] = 0;
 
  num = sscanf(buf, "%s %s %s %s %s", key, state, server_ip, controller_name, tiername);

  if(tiername[0] == 0)
    strcpy(tiername , "Cavisson");

  //sprintf(vector_prefix, "%s%c%s", tiername, global_settings->hierarchical_view_vector_separator, server_ip);

  
  auto_mon_state = atoi(state);

  NSDL2_SCHEDULE(NULL, NULL, "auto_mon_state = %d", auto_mon_state);
  if(auto_mon_state != 0 && auto_mon_state != 1)
  { 
    NS_KW_PARSING_ERR(keyword, 0, err_msg, ENABLE_NO_MONITORS_USAGE, CAV_ERR_1060030, keyword)
  }

  // Validation
  if(auto_mon_state != 0 && (num < 3 || num > 5)) // Controller name is optional.
  {
    NS_KW_PARSING_ERR(keyword, 0, err_msg, ENABLE_NO_MONITORS_USAGE, CAV_ERR_1011359, keyword);
  }

  if(auto_mon_state == 0)
  {
    NSDL2_SCHEDULE(NULL, NULL, "Since auto_mon_state = %d so returning", auto_mon_state);
    return 0;
  }

  if((ptr = strchr(server_ip, ':')) != NULL)
  {
    *ptr = '\0';
    port = atoi(ptr + 1);
    if(port == 0)
    {
      NSTL1(NULL, NULL, "Wrong port (%s) set in ENABLE_NO_MONITOR. So setting it to 7891.", (ptr+1));
      port = 7891;
    }
  }
  
  g_auto_no_mon_flag = auto_mon_state;
  if(g_auto_no_mon_flag == INACTIVATE)
  {
    NSDL2_SCHEDULE(NULL, NULL, "Monitors are inactivate so returning.");
    return 0; 
  }

  //g_num_no_server++;
  //If topology name is not given and NO auto are configured on server other than 127.0.0.1
  if( (strcmp(global_settings->hierarchical_view_topology_name, "NA") == 0) && (strcmp(server_ip,"127.0.0.1")) )
  {
     topolib_do_default_entry_for_no(server_ip,topo_idx);
  }
  //Create monitor table
  NSDL2_SCHEDULE(NULL, NULL, "Creating auto monitor table");
  if(create_auto_mon_table() == MON_FAILURE)
    NS_EXIT(-1,CAV_ERR_1060033, server_ip);
     //sprintf(vname, "NOAppliance%d", g_num_no_server);
  //NSDL2_SCHEDULE(NULL, NULL, "Adding server '%s' in server list if not exist. And vector name '%s'", server_name, vname);
  
  NSDL2_SCHEDULE(NULL, NULL, "Adding server '%s' in server list if not exist. And vector name 'NOAppliance'", server_ip);
  
  //It loops for server_disp_name and server_ip in 4.6.0. 
  if(topolib_get_server_disp_name_from_server_ip(server_ip,server_name,err_msg,tiername, topo_idx) < 0) 
  {
    NS_EXIT(-1, CAV_ERR_1060034 ,server_ip);
  }
  
  sprintf(vector_prefix, "%s%c%s", tiername, global_settings->hierarchical_view_vector_separator, server_name);


  //If both NO and NS auto mon are configured on 127.0.0.1, in this case use SVR_TYPE_NS_NO
  if((strcmp(server_ip,"127.0.0.1") == 0) && g_auto_ns_mon_flag)
  {
    if(add_auto_mon_server(server_ip, vector_prefix, SVR_TYPE_NS_NO, controller_name) == MON_FAILURE)
    NS_EXIT(-1, CAV_ERR_1060035);

  }
  else{  
    //Add provied server in AutoMOnSvrTable
    if(add_auto_mon_server(server_ip, vector_prefix, SVR_TYPE_NO, controller_name) == MON_FAILURE)
      NS_EXIT(-1, CAV_ERR_1060036, server_ip);
  }

  // We are adding 127.0.0.1 in server list as we need this ip in the case of network delay monitor
  NSDL2_SCHEDULE(NULL, NULL, "Adding 127.0.0.1 into server list, loopBack_add_done = %d", loopBack_add_done);
  if(!loopBack_add_done)
  {
    add_server_in_server_list(LOOPBACK_IP_PORT, vector_prefix, topo_idx);
    loopBack_add_done = 1;
  }
  return 0;
}
/*Cavisson>Controller

Name                         Local               Remote                                  Purpose

Cmon_<GeneratorName>          ANY               <GeneratorIP>:7891                        Monitors

Control_<GeneratorName>       ANY:<NSPort>      <GeneratorIP>:AnyPort                     Control connection

Data_<GeneratorName>          ANY:<NSDataPort>  <GeneratorIP>:AnyPort                     Data connection


For Example, following vectors will come for generator VP-C-IL-Chicago-Quadranet-6
               Cavisson>Controller>Cmon_VP-C-IL-Chicago-Quadranet-6
               Cavisson>Controller>Control_VP-C-IL-Chicago-Quadranet-6
               Cavisson>Controller>Data_VP-C-IL-Chicago-Quadranet-6

               There will 4 vectors per generators. If you are running test with 100 generators, there will be 400 vectors from one monitor running on Controller
               Will monitor be able to handle or it will be busy?

               Cavisson><GeneratorName>
Name                          Local                 Remote                                 Purpose

CmonController                 ANY                  <ControllerIP>:7891                    Data Transfer

CmonGenerator                  7891                  <ControllerIP>                        Connection States Between Cmon and Controller

Control                        ANY                  <ControllerIP>: <NSPort>               Control connection
        
Data                           ANY                  <ControllerIP>:<NSDataPort>            Data connection

For Example, following vectors will come for generator VP-C-IL-Chicago-Quadrane
               Cavisson>VP-C-IL-Chicago-Quadranet-6>CmonController
               Cavisson>VP-C-IL-Chicago-Quadranet-6>CmonGenerator
               Cavisson>VP-C-IL-Chicago-Quadranet-6>Control
               Cavisson>VP-C-IL-Chicago-Quadranet-6>Data

               There will 4 per generator running on each generator.
               If you are running test with 100 generators, there will be 400 vectors from 100 monitors running on each generator

*/

void tcp_states_count_auto_mon(FILE *fp, int index, char *server)
{

  char buff[64000 + 1];
  int len=0;
  len += sprintf(buff + len , "STANDARD_MONITOR %s %s TcpStatesCountV3", server, g_auto_mon_server_table[index].vector_name);
    
  if((g_auto_mon_server_table[index].server_type == SVR_TYPE_CONTROLLER ) && (g_auto_ns_mon_flag == 2))
  {
    int k; 
    len += sprintf(buff + len, " -c ");
    for(k = 0 ; k < sgrp_used_genrator_entries ; k++)
    {
     len+= sprintf(buff + len , "Cmon_%s#ANY#%s:7891!Control_%s#%hu#%s!Data_%s#%hu#%s%c", generator_entry[k].gen_name, generator_entry[k].IP,generator_entry[k].gen_name,parent_port_number,generator_entry[k].IP ,generator_entry[k].gen_name, g_dh_listen_port,generator_entry[k].IP, (k < (sgrp_used_genrator_entries - 1))?'!':' ');
    }
    
   }
  else  if((g_auto_mon_server_table[index].server_type == SVR_TYPE_GEN) && (g_auto_ns_mon_flag == 2))
  { 
    len +=sprintf(buff + len, " -c " );
    len += sprintf(buff +  len , "CmonController#ANY#%s:7891!CmonGenerator#7891#%s!Control#ANY#%s:%hu!Data#ANY#%s:%hu", global_settings->ctrl_server_ip, global_settings->ctrl_server_ip, global_settings->ctrl_server_ip , parent_port_number , global_settings->ctrl_server_ip,  g_dh_listen_port);
  } 
    fprintf(fp, "%s\n", buff);
}

void enable_jmeter_monitoring_on_gen(FILE *fp, int index, char *server)
{
  char buff[4096];
  int len = 0;
  for(int i = 0; i<total_runprof_entries; i++)
  {
    //if(runprof_table_shr_mem[i].sess_ptr->script_type == SCRIPT_TYPE_JMETER)
    //Need to check script type 1 why??
    if(g_script_or_url == SCRIPT_TYPE_JMETER)
    {
      if((g_auto_mon_server_table[index].server_type) == SVR_TYPE_GEN)
      {
        len = sprintf(buff, "STANDARD_MONITOR %s %s%cJMeter_%s_%d JavaGCJMXSun8 -f %s/logs/TR%d/.pidfiles/.JMeter_%s_%d.pid\n", server, g_auto_mon_server_table[index].vector_name, global_settings->hierarchical_view_vector_separator, runprof_table_shr_mem[i].sess_ptr->sess_name, i, generator_entry[index].work, generator_entry[index].testidx, runprof_table_shr_mem[i].sess_ptr->sess_name, i);

        sprintf(buff + len, "LOG_MONITOR %s NA_get_log_file.gdf  %s%cget_log_file 2 cm_get_log_file  -f %s/logs/JMeter_%s_%d.log -t -s 10 -z %s/logs/TR%d/NetCloud/%s/JMeter_%s_%d.log", server, g_auto_mon_server_table[index].vector_name, global_settings->hierarchical_view_vector_separator, generator_entry[index].work, runprof_table_shr_mem[i].sess_ptr->sess_name, i,  getenv("NS_WDIR"), testidx, generator_entry[index].gen_name,  runprof_table_shr_mem[i].sess_ptr->sess_name, i);
       fprintf(fp, "%s\n", buff);
      }
      else if (g_auto_mon_server_table[index].server_type != SVR_TYPE_CONTROLLER)
      {
        sprintf(buff, "STANDARD_MONITOR %s %s%cJMeter_%s_%d JavaGCJMXSun8 -f %s/logs/TR%d/.pidfiles/.JMeter_%s_%d.pid\n", server, g_auto_mon_server_table[index].vector_name, global_settings->hierarchical_view_vector_separator,runprof_table_shr_mem[i].sess_ptr->sess_name, i, getenv("NS_WDIR"), testidx, runprof_table_shr_mem[i].sess_ptr->sess_name, i);
        fprintf(fp, "%s\n", buff);
      }
    }
  }
}

void apply_docker_based_monitors_on_gen(FILE *fp, int index, char *server_name)
{
  int k ;
  char buff[64000 + 1];
  char common_buff[2048];
  int total_docker_mon = 4;
  int len = 0;
  char *docker_mon_list[] = {"DockerCpuStatsEx", "DockerMemoryStatsEx","DockerNetworkStatsEx", "DockerIOStatsEx"}; 
  sprintf(common_buff, "STANDARD_MONITOR %s %s", server_name, g_auto_mon_server_table[index].vector_name);
  
  for(int i = 0; i< total_docker_mon; i++)
  {
    len+= sprintf(buff + len, "%s %s --container self\n", common_buff, docker_mon_list[i]); 
  }
  fprintf(fp, "%s", buff); 
}

//This funtction start the Iperf Server by Executing cm_iperf_start_server shell for Iperf monitoring on NetCloud Test
int start_iperf_server_for_netcloud_test(char *path, int *port)
{
  char buf[MAX_NAME_LEN]; 
  FILE *fp = NULL;
  char iperf_path[MAX_NAME_LEN];
  sprintf(iperf_path ,  "%s/bin/cm_iperf_start_server -t %d", path, testidx);
  fp = popen(iperf_path, "r");
  if (fp == NULL)
  {
    fprintf(stderr, "Error in executing shell %s (Error: '%s')", iperf_path, strerror(errno));
    *port= -1;
    NSDL1_MON(NULL, NULL, "Error in executing shell %s (Error : '%s') So port is %d", iperf_path, strerror(errno) , *port);
    return -1;
  }
  nslb_fgets(buf, MAX_NAME_LEN, fp, 0);
  *port = atoi(buf);
  pclose(fp);
  NSDL1_MON(NULL, NULL, "port return by shell %d", iperf_path, strerror(errno) , *port);
  return 0;
}
   

/***************************************************************************************************************
 * Name    : kw_set_enable_ns_monitors 
 *
 * Purpose : 1) This function will Keyworld ENABLE_NS_MONITORS form scenario. 
 *              Syntax -
 *                ENABLE_NS_MONITORS  <mode> 
 *                  mode - 1 : active all the monitors [default]
 *                         0 : inactive all the monitors 
 *
 *           2) This function create two tables.
 *                  (i) AutoMonTable (if not exist)    (ii) AutoMonSvrTable
 *                  
 *                  g_auto_mon_table }               g_auto_mon_server_table
 *                   --------------- .                ---------------
 *                 0 |monitor name | .<-------------- |server_name  |
 *                   |ns_state     | .                |vector_name  |
 *                   |no_state     | .                |server_type  |
 *                   --------------- .                ---------------
 *                 1 |             | .<-------------- |             |
 *                   |             | .                |             |
 *                   --------------- .                ---------------
 *                 2 |             | .<-------------- |             |
 *                   |             | .                |             |
 *                   --------------- .                ---------------
 *                 3 |             | }                |             |
 *                   
 *          3) We will automate all the monitors which are in AutoMonTable for every server in AutoMonSvrTable
 *             Eg: 
 *              For Server1 -> mon1, mon2, mon3.... (all in AutoMonTable)
 *                  Server2 -> mon1, mon2, mon3.... (all in AutoMonTable)
 *
 *          4) If Controller mode is not enable then we will add server '127.0.0.1:7891' in AutoMonSvrTable
 *
 *          5) If Controller mode is enable then we will add server '127.0.0.1:7891' and all generators host name in AutoMonSvrTable
 *
 * Input   : keyword - ENABLE_NS_MONITORS
 *           buf     - ENABLE_NS_MONITORS  <mode>
 *
 * Output  : exit - on failure 
 **************************************************************************************************************/
int kw_set_enable_ns_monitors(char *keyword, char *buf)
{
  char key[MAX_NAME_LEN];
  char state[MAX_NAME_LEN];
  int value = 0;
  char server_ip_port[MAX_NAME_LEN];
  char vector_prefix[MAX_NAME_LEN] = {0};
  char err_msg[MAX_NAME_LEN] = {0};
  int auto_mon_state = 1;
  int auto_iperf_state=0;
  int num = 0, i = 0;

  NSDL2_MON(NULL, NULL, "Method called, keyword = %s, buf = %s", keyword, buf);
   
  num = sscanf(buf, "%s %s %d", key, state, &value);
  auto_mon_state = atoi(state);
  auto_iperf_state = value;
  if (auto_iperf_state != 1 || (strcmp(g_cavinfo.config, "NC")))
  {
    auto_iperf_state = 0;
    NSDL1_MON(NULL, NULL, "Either Machine type is not NC or  iperf_monitoring value is not 0 or 1 set to 0(Bydefault)");
  }

  if(num != 2 && num != 3 )
  {
    NS_KW_PARSING_ERR(keyword, 0, err_msg, ENABLE_NS_MONITORS_USAGE, CAV_ERR_1011359, keyword);
  }


  NSDL1_MON(NULL, NULL, "auto_mon_state = %d", auto_mon_state);

  if(auto_mon_state != 0 && auto_mon_state != 1 && auto_mon_state != 2)
  { 
    NS_KW_PARSING_ERR(keyword, 0, err_msg, ENABLE_NS_MONITORS_USAGE, CAV_ERR_1060030, keyword);
  }

  g_auto_ns_mon_flag = auto_mon_state;
  g_enable_iperf_monitoring = auto_iperf_state;

  if(g_auto_ns_mon_flag == INACTIVATE)
  {
    NSDL2_SCHEDULE(NULL, NULL, "Monitors are inactivate so returning.");
    return 0;
  }

  //Creating monitor table
  NSDL2_MON(NULL, NULL, "Creating monitor table");
  if(create_auto_mon_table() == MON_FAILURE)
    NS_EXIT(-1,CAV_ERR_1060037);
             
  // Add server in AutoMonSvrTable
  // Test will be either in NS_MODE(-1) or CONTROLLER_MODE(0, 1, 2)
  NSDL2_MON(NULL, NULL, "Controller mode = %d", loader_opcode);
  if(loader_opcode == STAND_ALONE) //Netstorm standalone mode
  {
    NSDL2_MON(NULL, NULL, "Configure auto monitor for standalone mode(netstorm mode).");
    if(enable_store_config)
      sprintf(vector_prefix,"%s!Cavisson%c%s",g_store_machine,global_settings->hierarchical_view_vector_separator,g_machine);
    else
      sprintf(vector_prefix,"Cavisson%c%s",global_settings->hierarchical_view_vector_separator,g_machine);
       //TODO TIER Name Cavisson
    if(add_auto_mon_server(LOOPBACK_IP_PORT, vector_prefix, SVR_TYPE_NS, NULL) == MON_FAILURE)
      NS_EXIT(-1, CAV_ERR_1060038, LOOPBACK_IP_PORT);
  }
  else //controller mode
  {
    //TODO: find out the controller type and also their ip
     NSDL2_MON(NULL, NULL, "Configure auto monitor for Controller mode(NetCloud).");

    if(topolib_generate_vector_auto_monitor(IS_CONTROLLER, NULL, NULL, vector_prefix, global_settings->hierarchical_view_vector_separator, err_msg) == -1)
    {
      NS_EXIT(-1, CAV_ERR_1060039);
    }
   //TODO TIER_NAME Cavisson
    if(add_auto_mon_server(LOOPBACK_IP_PORT, vector_prefix, SVR_TYPE_CONTROLLER, NULL) == MON_FAILURE)
      NS_EXIT(-1, CAV_ERR_1060038, LOOPBACK_IP_PORT);

   //Add all generators in AutoMonSvrTable
    NSDL2_MON(NULL, NULL, "sgrp_used_genrator_entries = %d", sgrp_used_genrator_entries);
    for (i = 0; i < sgrp_used_genrator_entries; i++)
    {
      NSDL3_MON(NULL, NULL, "i = %d, generator_entry[%d].IP = [%s], generator_entry[%d].agentport = [%s]", 
	                                     i, i, generator_entry[i].IP, i, generator_entry[i].agentport);

      //sprintf(server_ip_port, "%s:%s", generator_entry[i].IP, generator_entry[i].agentport);
      sprintf(server_ip_port, "%s", generator_entry[i].IP);
       
      NSDL3_MON(NULL, NULL, "server_ip_port = [%s]", server_ip_port);
 
      if(topolib_generate_vector_auto_monitor(IS_GENERATOR, (char *)generator_entry[i].gen_name, NULL, vector_prefix, global_settings->hierarchical_view_vector_separator, err_msg) == -1)
    {
      NS_EXIT(-1,CAV_ERR_1060039);
    }

    if(add_auto_mon_server(server_ip_port, vector_prefix, SVR_TYPE_GEN, NULL) == MON_FAILURE)
      NS_EXIT(-1, CAV_ERR_1060038,server_ip_port);
    }
  }
  return 0;
}

//This function is used set for TSDB configuration
int kw_set_tsdb_configuration(char *buff)
{
  char keyword[1024];
  int value;
  char err_msg[MAX_NAME_LEN] = {0};

  if(sscanf(buff, "%s %d", keyword, &value)< 2)
  {
    NS_KW_PARSING_ERR(keyword, 0, err_msg, TSDB_SERVER_USAGE, CAV_ERR_1060082, keyword);
  }

  if(value == 0)
     g_tsdb_configuration_flag |= RTG_MODE;
  else if(value == 1)
       {
          g_tsdb_configuration_flag |= TSDB_MODE;
	   //Allocate only if TSDB mode is on .Don't Allocate for RTG_MODE & RTG_TSDB_MODE is On
	  if(g_gdf_hash == NULL)
	  {
	    MY_MALLOC(g_gdf_hash, (1024 * sizeof(NormObjKey)), "Memory allocation to Norm gdf_name table", -1);
	    nslb_init_norm_id_table(g_gdf_hash, 1024);
	  }
      }

  else if(value == 2)
     g_tsdb_configuration_flag |= RTG_TSDB_MODE;
  else
  {
    NSTL1(NULL, NULL, "Invalid value to the keyword %s. Value = %d. Hence setting it to 0 by default.", keyword, value);
    g_tsdb_configuration_flag |= RTG_MODE;
    return 0;
  }
  return 0;
}



void kw_set_enable_generator_process_monitoring(char *buff)
{     
  int value;
  char key[1024];
         
  if(sscanf(buff, "%s %d", key, &value) != 2)
  {
    NSTL1(NULL, NULL, "Wrong input to the keyword %s. Hence setting its value to 0", key);
    g_generator_process_monitoring_flag = 0;
    return; 
  }      
      
  if(value == 0)
    g_generator_process_monitoring_flag = 0;
  else if(value == 1)
    g_generator_process_monitoring_flag = 1;
  else   
  {   
    NSTL1(NULL, NULL, "Invalid value to the keyword %s. Value = %d. Hence setting it to 0 by default.", key, value);
    g_generator_process_monitoring_flag = 0;
  }      
  return;
}  

int kw_set_enable_cavmon_inbound_port(char *keyword ,char *buff)
{
  int port = 0;
  char key[MAX_NAME_LEN];
  char err_msg[MAX_AUTO_MON_BUF_SIZE];
  if(sscanf(buff, "%s %d", key, &port) != 2)
  {
    NS_KW_PARSING_ERR(keyword, 0, err_msg, CAVMON_INBOUND_PORT_USAGE, CAV_ERR_1011359, keyword);
  }
  if(port < 1)
  {
    NS_KW_PARSING_ERR(keyword, 0, err_msg, CAVMON_INBOUND_PORT_USAGE, CAV_ERR_1060082);
  }
  else
  {
    g_cmon_port = port;
    set_cmon_port(g_cmon_port);
    NSTL1(NULL, NULL, "Cmon Port given for CAVMON_INBOUND_PORT is %d",port);
  }
  return 0;
}

int kw_set_enable_store_config(char *keyword,char *buff)
{
  char key[MAX_DATA_LINE_LENGTH];
  char err_msg[MAX_AUTO_MON_BUF_SIZE];
  int option;
  int len =0;

  if(sscanf(buff, "%s %d %s", key, &option,g_hierarchy_prefix) < 2)
  {     
     NS_KW_PARSING_ERR(keyword, 0, err_msg, GROUP_HIERARCHY_CONFIG_USAGE, CAV_ERR_1011359, keyword);
  } 

 
  if( option == 1 || option == 0)
  {
     if(strcmp(global_settings->hierarchical_view_topology_name,"NA"))
       enable_store_config=option;
     else
     {
       NSTL1(NULL, NULL, "Error: topology name is mandatory to give to set this keyword %s",key);
       return 0; 
     }
     
     if('\0' == g_hierarchy_prefix[0])
       strcpy(g_hierarchy_prefix,"Store!");
     else
     {
       len = strlen(g_hierarchy_prefix);
       sprintf(g_hierarchy_prefix + len,"!");
     }
  }
  else
  {
    NS_KW_PARSING_ERR(keyword, 0, err_msg, GROUP_HIERARCHY_CONFIG_USAGE, CAV_ERR_1060040, keyword);
  }
   
  if(enable_store_config)
  {
    if(!strncmp(g_cavinfo.config, "NS", 2))
      sprintf(g_store_machine, "NS");
    else if(!strncmp(g_cavinfo.config, "NDE", 3))
      sprintf(g_store_machine, "NDE");
    else if(!strncmp(g_cavinfo.config, "NV", 2))
      sprintf(g_store_machine, "NV");
    else if(!strncmp(g_cavinfo.config, "NO", 2))
      sprintf(g_store_machine, "NO");
    else if(!strncmp(g_cavinfo.config, "NC", 2))
      sprintf(g_store_machine, "NC");
  }
  return 0;
}

void kw_set_enable_auto_json_monitor_assist(char *file_path, int runtime_flag, int process_diff_json)
{
  char *file_ptr = NULL;
  char file_timestamp[128];
  char link_file[1024];
  char original_file[1024];
  long tloc;
  struct tm *lt, tm_struct;
  FILE *fp;
  long lSize;
  char cmd[4096];
  char err[4096];
 
  NSDL2_MON(NULL, NULL, "Method called, process_diff_json = %d, runtime_flag = %d", process_diff_json, runtime_flag);

  strcpy(file_timestamp, "default");

  /*  Getting current time  */
  (void)time(&tloc);
  lt = nslb_localtime(&tloc, &tm_struct, 1);

  if(lt != (struct tm *)NULL)
    strftime(file_timestamp, 128, "%Y%m%d%H%M%S", lt);

  if(process_diff_json)
    strcpy(global_settings->auto_json_monitors_diff_filepath, file_path);
  else
    strcpy(global_settings->auto_json_monitors_filepath, file_path);
 
  NSDL2_MON(NULL, NULL, "file_path = %s", file_path); 
  fp = fopen (file_path , "r" );
  if( !fp )
  {
    sprintf(err, "Error: Unable to read file [%s] for keyword ENABLE_AUTO_JSON_MONITOR.\n", file_path);
    goto err_case;
  }
 
  fseek( fp , 0L , SEEK_END);
  lSize = ftell( fp );
  rewind( fp );

  MY_REALLOC(file_ptr, (lSize + 1), "Allocating auto_json_monitors_diff_ptr", -1)
  if( !file_ptr )
  {
    CLOSE_FP(fp);
    sprintf(err,"Error: memory alloc fails for Keyword ENABLE_AUTO_JSON_MONITOR");
    goto err_case;
  }

  if(process_diff_json)
    auto_json_monitors_diff_ptr = file_ptr;
  else
    auto_json_monitors_ptr = file_ptr;
 
  /* copy the file into the buffer */
  if( 1!=fread(file_ptr , lSize, 1 , fp) )
  {
    CLOSE_FP(fp);
    free(file_ptr);
    sprintf(err,"Error: entire read fails for file[%s] for keyword ENABLE_AUTO_JSON_MONITOR",file_path);
    goto err_case;
  }
 
  CLOSE_FP(fp);
  
  sprintf(original_file, "%s/logs/%s/%s_%s", g_ns_wdir, global_settings->tr_or_partition, basename(file_path), file_timestamp);
 
  sprintf(cmd, "cp %s %s 2>/dev/null", file_path, original_file);
 
  nslb_system(cmd,1,err);

  if(!process_diff_json)
  {
    sprintf(link_file, "%s/logs/%s/ns_files/monitor.json", g_ns_wdir, global_settings->tr_or_common_files);

    if((link(original_file, link_file)) == -1)
    {
      if(errno != EEXIST)
      {
        NSTL1(NULL, NULL, "Could not create link %s to %s", link_file, original_file);
      }
      else
      {
        if(unlink(link_file) < 0)
        {
          NSTL1(NULL, NULL, "Could not able to remove link %s.", link_file);
        }
        if((link(original_file, link_file)) < 0)
        {
          if((symlink(original_file, link_file)) == -1)
          {
            NSTL1(NULL, NULL, "Error: Unable to create link %s, err = %s", link_file, nslb_strerror(errno));
          }
        }
      }
    }
    NSDL2_SCHEDULE(NULL, NULL, "Created link of %s in %s", original_file, link_file);

    //only call here to get global elements from JSON
    read_json_and_apply_monitor(process_diff_json, 1);
  }
  else
  {
    read_json_and_apply_monitor(process_diff_json, 0);
    if(auto_json_monitors_diff_ptr)
      free(auto_json_monitors_diff_ptr);
    global_settings->auto_json_monitors_diff_filepath[0] = '\0';
  }

  NSTL1(NULL, NULL, "JSON file has been added. JSON file name: %s", file_path);
  return;

  err_case :
    if(runtime_flag){
      NSTL1(NULL, NULL, "%s", err);
      return;
    }
    else
      NS_EXIT(-1,"%s", err);
}

int check_if_file_exist_and_in_json_format(char *tmp_file_path, char *file_path, char *err, char diff_flag)
{
  NSDL2_MON(NULL, NULL, "Method called");

  if(tmp_file_path[0] != '\0')
  {
    if((tmp_file_path[0] == '/') && diff_flag)
    {
      strcpy(file_path, tmp_file_path);
    }
    else
    {
      if(strchr(tmp_file_path, '/') != NULL)
      {
        if(!diff_flag)
        {
          sprintf(err, "Absolute or relative path for primary json is not supported anymore. "
                       "Please move your json [%s] to %s/mprof/%s/json/ directory and give only json name in scenario.\n", 
                        tmp_file_path, g_ns_wdir, global_settings->hierarchical_view_topology_name);
          return -1;
        }
        else
        {
          sprintf(err, "Relative path for diff json is not supported anymore. Please give absolute path for json [%s].\n", tmp_file_path);
          return -1;
        }
      }
      else
      {
        if(!diff_flag)
          sprintf(file_path, "%s/mprof/%s/json/%s", g_ns_wdir,global_settings->hierarchical_view_topology_name, tmp_file_path);
        else
        {
          sprintf(err, "Diff file need to be given with absolute path. Please give absolute path for json [%s].\n", tmp_file_path);
          return -1;
        }
      }
    }

    if(check_json_format(file_path) != 0)
    {
      sprintf(err, "Error: Either filename [%s] does not exist or there are one or more error in json [%s]. Check it in json editor.\n", 
                    tmp_file_path, tmp_file_path);
      return -1;
    }
  }
  else
  {
    sprintf(err, "Error: Either filename [%s] does not exist or there are one or more error in json [%s]. Check it in json editor.\n", 
                  file_path, file_path);
    return -1;
  }
  return 0;
}

/* ENABLE_AUTO_JSON_MONITOR <MODE> <FILE_NAME> <DIFF_FILE>
 * ENABLE_AUTO_JSON_MONITOR <MODE> <DIRECTORY_NAME>
 * MODE is optional field. Default value is 0.
 * Mode 0 -> Disable
 * Mode 1 -> Enable
     ->At start test
       ENABLE_AUTO_JSON_MONITOR 1 <FILE_NAME> 
     ->At json diff 
       ENABLE_AUTO_JSON_MONITOR 1 <FILE_NAME> <DIFF_FILE_PATH>
 * Mode 2 -> Enable  ENABLE_AUTO_JSON_MONITOR 2 <DIRECTORY_NAME> 
     -> DIRECTORY_NAME is optional 
*/
int kw_set_enable_auto_json_monitor(char *keyword, char *buf, char runtime_flag, char *err)
{
  char key[1024];
  int value;
  char tmp_file_path[1024];
  char tmp_diff_file_path[1024];
  char file_path[1024];
  char diff_file_path[1024];
  char temp[1024];
  int args;

  tmp_file_path[0] = '\0';
  NSDL2_MON(NULL, NULL, "Method called, keyword = %s, buf = %s, global_settings->net_diagnostics_mode=%d",
                                                     keyword, buf, global_settings->net_diagnostics_mode);

  args = sscanf(buf, "%s %d %s %s %s", key, &value, tmp_file_path, tmp_diff_file_path, temp);

  //when mode is 2 
  if(value == 2)
  {
    if(args > 3 || args < 2)
    {
      NS_KW_PARSING_ERR(keyword, runtime_flag, err, ENABLE_AUTO_JSON_MONITOR_USAGE, CAV_ERR_1011359, keyword);
    }    
    //tmp_file_path stores the directory name of json files.
    global_settings->json_mode = 2;

    // As mode is 2 we are never going to use global_settings->auto_json_monitors_filepath this var memory. So assigning this to
    // global_settings->json_files_directory_path var
    global_settings->json_files_directory_path = global_settings->auto_json_monitors_filepath;
    if(!strncmp(g_cavinfo.config, "SM", 2))
    {
      sprintf(global_settings->json_files_directory_path, "%s/smconfig/default/default", g_ns_wdir);
    }
    else
    {
      if(tmp_file_path[0] != '\0')    //directory name is given
        sprintf(global_settings->json_files_directory_path, "%s/%s", g_ns_wdir, tmp_file_path);
      else                            //directory name is not given so store default path
        sprintf(global_settings->json_files_directory_path, "%s/mprof/%s/monitor", g_ns_wdir, global_settings->hierarchical_view_topology_name);
    }
    sprintf(file_path, "%s/global.json", global_settings->json_files_directory_path);
    mj_read_global_json(file_path);
  }
  else
  {
    if(value == 0)
    {
      return 0;
    }

    if(value != 1)
    {
      NS_KW_PARSING_ERR(keyword, runtime_flag, err, ENABLE_AUTO_JSON_MONITOR_USAGE, CAV_ERR_1011359, keyword);
    }
    
    if(args > 4 || args < 2)
    {
      NS_KW_PARSING_ERR(keyword, runtime_flag, err, ENABLE_AUTO_JSON_MONITOR_USAGE, CAV_ERR_1011359, keyword);
    }

    if(check_if_file_exist_and_in_json_format(tmp_file_path, file_path, err, PROCESS_PRIMARY_JSON) < 0)
    {
      NS_KW_PARSING_ERR(keyword, runtime_flag, err, ENABLE_AUTO_JSON_MONITOR_USAGE, CAV_ERR_1060049, file_path);
    }

    if(runtime_flag)
    {
      if(check_if_file_exist_and_in_json_format(tmp_diff_file_path, diff_file_path, err, PROCESS_DIFF_JSON) < 0)
      {
        NS_KW_PARSING_ERR(keyword, runtime_flag, err, ENABLE_AUTO_JSON_MONITOR_USAGE, CAV_ERR_1060049, diff_file_path);
      }
    }

    global_settings->json_mode = 1;

    kw_set_enable_auto_json_monitor_assist(file_path, runtime_flag, PROCESS_PRIMARY_JSON);

    //Call on only runtime to process diff json
    if(runtime_flag) 
      kw_set_enable_auto_json_monitor_assist(diff_file_path, runtime_flag, PROCESS_DIFF_JSON);
  }

  return 0;
}

/***************************************************************************************************************
 * Name    : kw_set_disable_ns_no_monitors 
 *
 * Purpose : 1) This function will Keyworld DISABLE_NS_MONITORS and DISABLE_NO_MONITORS form scenario. 
 *              Syntax -
 *               DISABLE_NS_MONITORS <mon1> <mon2> <mon3> ... 
 *               DISABLE_NO_MONITORS <mon1> <mon2> <mon3> ... 
 *                   
 *           2) Set state on monitors in AutoMonTable to 0 
 *
 * Input   : keyword - DISABLE_NS_MONITORS
 *           buf     - DISABLE_NS_MONITORS <space seperated monitor list>
 *
 * Output  : exit - on failure 
 **************************************************************************************************************/
int kw_set_disable_ns_no_monitors(char *keyword, char *buf)
{
  char dis_ns_mon_list[MAX_AUTO_MON_BUF_SIZE];
  char *dis_ns_mon_list_arr[500];
  char *buf_ptr = NULL;
  char err_msg[MAX_AUTO_MON_BUF_SIZE]; 
  int i = 0, j = 0, total_mons = 0;

  dis_ns_mon_list[0] = 0;

  NSDL2_SCHEDULE(NULL, NULL, "Method called, keyword = %s, buf = %s", keyword, buf);

  if(g_auto_ns_mon_flag == INACTIVATE && g_auto_no_mon_flag == INACTIVATE)
  {
    NS_DUMP_WARNING("Ignoring scenario setting for %s as it cannot be used without enabling any %s Monitor.",
                     keyword, (!strcmp(keyword, "DISABLE_NS_MONITORS"))? "NS" : "NO");
    return 0;
  }
 
  buf_ptr = strpbrk(buf, "/t ");
  if(buf_ptr == NULL)
  {
    NS_KW_PARSING_ERR(keyword, 0, err_msg, DISABLE_NS_NO_MONITORS_USAGE, CAV_ERR_1060044, keyword);
  } 
  else
    buf_ptr += strlen(keyword);
 
  CLEAR_WHITE_SPACE(buf_ptr);
  
  NSDL2_SCHEDULE(NULL, NULL,"buf_ptr = [%s]", buf_ptr);
 
  strcpy(dis_ns_mon_list, buf_ptr); 
  CLEAR_WHITE_SPACE_FROM_END(dis_ns_mon_list);

  total_mons = get_tokens(buf, dis_ns_mon_list_arr, " ", 500);

  //Set state of monitors in AutoMonTable  
  NSDL3_SCHEDULE(NULL, NULL, "Number of disable monitors, total_mons = %d", total_mons);
  for(i = 0; i < total_mons; i++)
  {
    NSDL3_SCHEDULE(NULL, NULL, "i = %d", i);
    for(j = 0; j < NUM_AUTO_MON; j++)
    {
      NSDL3_SCHEDULE(NULL, NULL, "j = %d, dis_ns_mon_list_arr[%d] = [%s], g_auto_mon_table[%d].mon_name = [%s]", 
                                  j, i, dis_ns_mon_list_arr[i], j, g_auto_mon_table[j].mon_name);
      if((strcmp(dis_ns_mon_list_arr[i], g_auto_mon_table[j].mon_name)) == 0)
      {
        if(!strcmp(keyword, "DISABLE_NS_MONITORS"))
          g_auto_mon_table[j].ns_state = INACTIVATE;
        else
          g_auto_mon_table[j].no_state = INACTIVATE;

        break; //Break internal loop  
      }
    }
  }
  return 0;
}

void get_working_dir(char *wdir)
{
  NSDL2_SCHEDULE(NULL, NULL, "Method Called");

  if (getenv("NS_WDIR") != NULL)
  {
    strcpy(wdir, getenv("NS_WDIR"));
  }
  else 
  {
    NSDL2_SCHEDULE(NULL, NULL, "NS_WDIR env variable is not set. Setting it to default value /home/cavisson/work/");
    strcpy(wdir, "/home/cavisson/work/");
  }
}

int get_controller_name(char *con_name)
{
  FILE *fp;
  char wdir[MAX_NAME_LEN];  
  char cmd[2*MAX_NAME_LEN];  

  NSDL2_SCHEDULE(NULL, NULL, "Method Called, con_name = %p", con_name);

  get_working_dir(wdir);

  sprintf(cmd, "basename %s", wdir);
  
  NSDL2_SCHEDULE(NULL, NULL, "cmd = [%s]", cmd);

  fp = popen(cmd, "r");
  if(fp == NULL)
  {
    perror("Error: Error in running command to get basename");
    return MON_FAILURE;
  }
  if (!nslb_fgets(con_name, MAX_NAME_LEN, fp, 0))
  {
    perror("Error: Error in getting basename.");
    return MON_FAILURE;
  }
  pclose(fp);
 
  return MON_SUCCESS; 
}


void kw_set_disable_dvm_vector_list(char *keyword, char *buf)
{
  char key[DYNAMIC_VECTOR_MON_MAX_LEN] = "";
  int value;

  NSDL2_MON(NULL, NULL, "Method called, keyword = %s, buf = %s", keyword, buf);

  sscanf(buf, "%s %d", key, &value);

  if(value)
  {
    g_disable_dvm_vector_list = 1;
    NSTL1(NULL, NULL, "g_disable_dvm_vector_list is set when value of %s is %d", key, value);
  }
  else
  {
    g_disable_dvm_vector_list = 0;
    NSTL1(NULL, NULL, "g_disable_dvm_vector_list is not set when value of %s is %d", key, value);
  }
}

int kw_set_enable_cmon_agent(char *keyword, char *buffer)
{
  char key[MAX_NAME_LEN];
  //char input_server_ip[MAX_NAME_LEN + 1] = {0};
  char temp_server_name[MAX_NAME_LEN] = {0};
  char display_server_name[2*MAX_NAME_LEN + 2] = {0};
  char tiername[MAX_NAME_LEN + 1] = {0};
  char vector_prefix[4*MAX_NAME_LEN + 1] = {0};
  //char *input_tmp_arr[2];
  char state[MAX_NAME_LEN];
  char value[MAX_NAME_LEN];
  int enable_cmon_agent = 0;   //0 - desable ,   1 - enable
  int enable_java_mon = 0; 
  int i,num_field=0;
  //ServerInfo *servers_list = NULL;
  //int total_no_of_servers = 0;
  int machine_type = 1;
  char *colon_ptr = NULL;
  char *fields[3];
  char err_msg[MAX_AUTO_MON_BUF_SIZE]; 

  NSDL2_SCHEDULE(NULL, NULL, "Method called, keyword = %s, buffer = %s", keyword, buffer);
  num_field = get_tokens(buffer, fields, " ", 3);
  if(num_field == 2) 
  {
    strcpy(key,fields[0]);
    strcpy(state,fields[1]);
    g_java_mon_flag_cmon = 1;
  }
  else if(num_field == 3)
  {
    strcpy(key,fields[0]);
    strcpy(state,fields[1]);
    strcpy(value,fields[2]); 
    enable_java_mon=atoi(value);
    g_java_mon_flag_cmon = enable_java_mon; 
  }
  else
  {
    NS_KW_PARSING_ERR(keyword, 0, err_msg, ENABLE_CMON_AGENT_USAGE, CAV_ERR_1011359, key);
  }
  enable_cmon_agent = atoi(state);

  NSDL2_SCHEDULE(NULL, NULL, "enable_cmon_agent = %d", enable_cmon_agent);

  if(num_field == 2)
  {
    if(enable_cmon_agent != 0 && enable_cmon_agent != 1)
    {
      NS_KW_PARSING_ERR(keyword, 0, err_msg, ENABLE_CMON_AGENT_USAGE, CAV_ERR_1060030, key);
    }
  }
  else
  {
    if(enable_cmon_agent != 0 && enable_cmon_agent != 1 && g_java_mon_flag_cmon !=0 && g_java_mon_flag_cmon !=1)
    {
      NS_KW_PARSING_ERR(keyword, 0, err_msg, ENABLE_CMON_AGENT_USAGE, CAV_ERR_1060030, key);
    }
  }

  g_cmon_agent_flag = enable_cmon_agent;

  if(g_cmon_agent_flag == INACTIVATE)
  {
    NSDL2_SCHEDULE(NULL, NULL, "Monitors for cmon agent are inactivate so returning.");
    return 0;
  }

  //servers_list = (ServerInfo *) topolib_get_server_info();
  //Total no. of servers in ServerInfo structure
  //total_no_of_servers = topolib_get_total_no_of_servers();
  
  for(i=0; i < topo_info[topo_idx].total_tier_count; i++)
  {
   for(int j=0;j<topo_info[topo_idx].topo_tier_info[i].total_server;j++)
   {
     if(topo_info[topo_idx].topo_tier_info[i].topo_server[j].used_row != -1)
     {
        NSDL2_SCHEDULE(NULL, NULL, "topo_info[topo_idx].topo_tier_info[%d].topo_server[%d].server_ptr->topo_servers_list->server_ip = %s ,topo_info[topo_idx].topo_tier_info[%d].topo_server[%d].server_ptr->cav_mon_home = %s",
    i,j, topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->server_ip,i,j, topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->cav_mon_home);
      //In case of lps,nde we make server entry directly in structure (not from file). In that case intall_dir will be NA, 
      //so ignoring these entries
      if(!strcasecmp(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->cav_mon_home, "NA"))
        continue;

    //strcpy(input_server_ip, servers_list[i].server_ip);

 /*
    if(strstr(input_server_ip, ":"))
    {
      get_tokens(input_server_ip, input_tmp_arr, ":", 2);
      if(input_tmp_arr[0])
        strcpy(input_server_ip, input_tmp_arr[0]);
    }
*/
    //if(servers_list[i].topo_server_idx >= 0) 
      if(!topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->server_flag & DUMMY) 
      {
        if(!((topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->auto_scale & 0X02) || (!strcmp(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->server_ip, "127.0.0.1"))))
          continue;
        strcpy(tiername, topo_info[topo_idx].topo_tier_info[i].tier_name);
        topolib_get_server_display_and_tier_for_auto_mon(j, tiername, temp_server_name,i,topo_idx);
      }
      else
        continue;
    

      snprintf(vector_prefix, 4*MAX_NAME_LEN, "%s%c%s%cCMON", tiername, global_settings->hierarchical_view_vector_separator, temp_server_name, global_settings->hierarchical_view_vector_separator);
      sprintf(display_server_name, "%s_%s", tiername, temp_server_name);

      //reset ip & port
      //memset(input_server_ip, 0, MAX_NAME_LEN);

      //we cannot have colon in signature name because check monitor uses colon as token
      if((colon_ptr = strchr(display_server_name, ':')))
         *colon_ptr = '_';

        if( !strcasecmp(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->machine_type,"Linux") || 
                           !strcasecmp(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->machine_type,"LinuxEx") )
          machine_type = LINUX;
        else if(!strncasecmp(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->machine_type,"AIX",3))
          machine_type = AIX;
        else if( !strcasecmp(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->machine_type,"Solaris") || 
                            !strcasecmp(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->machine_type,"SunOS") )
          machine_type = SOLARIS;
        else if( !strncasecmp(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->machine_type,"HPUX",4) )
          machine_type = HPUX_MACHINE;
        else if( !strcasecmp(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->machine_type,"Windows") )
          machine_type = WIN_MACHINE;
        else
          machine_type = OTHERS;

      if(add_cmon_agent_server(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->server_ip, vector_prefix, topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->cav_mon_home, machine_type, display_server_name) == MON_FAILURE)
      NS_EXIT(-1, CAV_ERR_1060031, topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->server_ip); 
     }
   }
  }
  return 0;
}

/***************************************************************************************************************
 * Name    : make_ns_auto_mprof 
 *
 * Purpose : 1) This function will make a hidden file on TRxx/... of name '.auto.mprof'
 *           2) This .auto.mprof will contains all the monitors which are to be automate for NS, Controller(internal, external) and NO 
 *
 * Input   : NONE 
 *
 * Output  : 0 - on Success
 *          -1 - on Failure 
 **************************************************************************************************************/
int make_ns_auto_mprof()
{
  int i = 0, j = 0;
  FILE *fauto_mprof;

 // char flag_java_process;
  char auto_mprof[2*MAX_NAME_LEN];
  char auto_mon_cmd[8*MAX_NAME_LEN];  
  char wdir[MAX_NAME_LEN];  
  char hv_sep[2];
  char *ptr = NULL, *hpd_ptr = NULL, *controller_name = NULL;
  char tier_name[MAX_NAME_LEN + 1] = {0};
  char vector_name[MAX_NAME_LEN + 1] = {0};
  char ns_or_controller[20 + 1] = {0};
  char err_msg[64000] = {0};
//  int is_nv = 0;
  char server_name[128];
  char tomcat_dir[1024] = {0};
  char access_log_file[2*1024];
  char tsdb_access_log_file[2*1024];
  char file_path[1024];
  //auto_mon_cmd[0] = 0, hpd_controller_name[0] = 0, auto_mprof[0] = 0;
  auto_mon_cmd[0] = 0, auto_mprof[0] = 0;
  

  //Commenting following as we are supporting only default work for HPD Traffic stats 
  //In future if required then we will support controller name also
  #if 0
  strcpy(hpd_controller_name, "work");
  //If ENABLE_NO_MONITORS is used then get controller name for HPD Traffic stat monitor
  if(g_auto_no_mon_flag)
    if(get_controller_name(hpd_controller_name) == MON_FAILURE)
      return MON_FAILURE;

  //Removing new line from end
  if((new_line_ptr = strstr(hpd_controller_name, "\n")) != NULL)
    *new_line_ptr = 0;

  NSDL2_SCHEDULE(NULL, NULL, "hpd_controller_name = [%s]", hpd_controller_name);
  #endif

  get_working_dir(wdir); 
  
  controller_name = strrchr(wdir, '/');
  controller_name++;
  if(!strncmp(controller_name, "work", 4))
    controller_name = "apps";

  if(strcmp(g_cavinfo.config, "NV") == 0)
  {
//    is_nv = 1;    //netvision mode

    hpd_ptr = getenv ("HPD_ROOT");
    if(hpd_ptr == NULL)
      hpd_ptr = "/var/www/hpd";

    /*controller_name = strrchr(wdir, '/');
    controller_name++;
    if(!strncmp(controller_name, "work", 4)) 
      controller_name = "apps";*/
  }
   
  if((g_tsdb_configuration_flag & TSDB_MODE) ||(g_tsdb_configuration_flag & RTG_TSDB_MODE))
    sprintf(tsdb_access_log_file, "%s/logs/TR%d/tsdb/logs/al/access.log*",getenv("NS_WDIR"), testidx); 
  
  sprintf(auto_mprof, "%s/logs/%s/ns_files/ns_auto.mprof", wdir, global_settings->tr_or_common_files);
 
  NSDL2_SCHEDULE(NULL, NULL, "Method Called, Opening file '%s' on write mode", auto_mprof);

  if ((fauto_mprof = fopen(auto_mprof, "w+")) == NULL) {
    fprintf(stderr, "error in creating auto mprof file\n");
    perror("fopen");
    return MON_FAILURE;
  }

  NSDL2_SCHEDULE(NULL, NULL, "Making command for monitors each server, total_auto_mon_server_entries = %d", total_auto_mon_server_entries);

  //for each server make monitor structure 
  fprintf(fauto_mprof, "#This is Netstorm generated mprof to automate following monitors...\n\n");
  for(i = 0; i < total_auto_mon_server_entries; i++)
  {
    NSDL3_SCHEDULE(NULL, NULL, "server idx = %d", i);
    fprintf(fauto_mprof, "##### Monitors for server '%s' #####\n", g_auto_mon_server_table[i].server_name);
    for(j = 0; j < NUM_AUTO_MON; j++)
    {
   //   flag_java_process = 0;
      //skip those monitors which are listed for disable
      NSDL3_SCHEDULE(NULL, NULL, "i = %d, j = %d, g_auto_mon_table[%d].ns_state = %d, g_auto_mon_table[%d].no_state = %d, "
                                 "g_auto_mon_server_table[%d].server_type = %d", 
                                  i, j, j, g_auto_mon_table[j].ns_state, j, g_auto_mon_table[j].no_state, i, g_auto_mon_server_table[i].server_type);
      if((g_auto_mon_table[j].ns_state == 0 && 
         (g_auto_mon_server_table[i].server_type == SVR_TYPE_NS || g_auto_mon_server_table[i].server_type == SVR_TYPE_CONTROLLER || 
          g_auto_mon_server_table[i].server_type == SVR_TYPE_GEN)) || 
          (g_auto_mon_table[j].no_state == 0 && g_auto_mon_server_table[i].server_type == SVR_TYPE_NO))
        continue;
       
      //strcpy(server_name, g_auto_mon_server_table[i].server_name);
      //This done for autoscaling of monitor as we are passing cavisson>NDAppliance as a servername          
      strcpy(server_name,g_auto_mon_server_table[i].tier_server);

      NSDL3_SCHEDULE(NULL, NULL, "mon_name = [%s]", g_auto_mon_table[j].mon_name); 
      if(g_auto_mon_table[j].no_state != INACTIVATE && g_auto_mon_server_table[i].server_type == SVR_TYPE_NS_NO)
      {
        if(!strcmp(g_auto_mon_table[j].mon_name, "SimulatedServiceStats") && ((g_auto_mon_table[j].no_state == ACTIVATE)))
        {
          sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s%cSimulatedServiceStats SimulatedServiceStats -c %s",
                              server_name, g_auto_mon_server_table[i].vector_name, global_settings->hierarchical_view_vector_separator, g_auto_mon_server_table[i].controller_name);
          fprintf(fauto_mprof, "%s\n", auto_mon_cmd);
        }
	else if(!strcmp(g_auto_mon_table[j].mon_name, "SimulatedProtocolStats") && (g_auto_mon_table[j].no_state == ACTIVATE))
        {
          sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s%cSimulatedProtocolStats SimulatedProtocolStats -c %s",
                              server_name, g_auto_mon_server_table[i].vector_name, global_settings->hierarchical_view_vector_separator, g_auto_mon_server_table[i].controller_name);
          fprintf(fauto_mprof, "%s\n", auto_mon_cmd);
        }
        else if(!strcmp(g_auto_mon_table[j].mon_name, "SERVER_SIGNATURE"))
        {
          sprintf(auto_mon_cmd, "SERVER_SIGNATURE %s %s File /var/www/hpd/conf/hpd.conf", g_auto_mon_server_table[i].vector_name,server_name);
          fprintf(fauto_mprof, "%s\n", auto_mon_cmd);
        }
        continue;
 
      }
      if(!strcmp(g_auto_mon_table[j].mon_name, "MPStats") && (g_auto_mon_table[j].ns_state == ACTIVATE))
      { 
        sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s%cMPStats MPStat ",
                              server_name, g_auto_mon_server_table[i].vector_name, global_settings->hierarchical_view_vector_separator); 
      }
      else if(!strcmp(g_auto_mon_table[j].mon_name, "MemStatsEx") && (g_auto_mon_table[j].ns_state == ACTIVATE))
      {
        sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s MemStatsEx ",
                              server_name, g_auto_mon_server_table[i].vector_name);
      }
      else if(!strcmp(g_auto_mon_table[j].mon_name, "DiskStats") && (g_auto_mon_table[j].ns_state == ACTIVATE))
      {
        sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s%cDiskStats FileSystemStats ",
                      server_name, g_auto_mon_server_table[i].vector_name, global_settings->hierarchical_view_vector_separator); 
      }
      else if(!strcmp(g_auto_mon_table[j].mon_name, "EthStat") && (g_auto_mon_table[j].ns_state == ACTIVATE)) 
      {
        sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s%cEthStat EthStat ",
                              server_name, g_auto_mon_server_table[i].vector_name, global_settings->hierarchical_view_vector_separator); 
      }
      else if(!strcmp(g_auto_mon_table[j].mon_name, "ProcessCountByState") && (g_auto_mon_table[j].ns_state == ACTIVATE))
      {
        sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s ProcessCountByState ",
                              server_name, g_auto_mon_server_table[i].vector_name); 
      }
      else if(!strcmp(g_auto_mon_table[j].mon_name, "DeviceStats") && (g_auto_mon_table[j].ns_state == ACTIVATE))
      {
        sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s%cDeviceStats DeviceStats ",
                              server_name, g_auto_mon_server_table[i].vector_name, global_settings->hierarchical_view_vector_separator);
      }
      else if(!strcmp(g_auto_mon_table[j].mon_name, "AccessLogStatsExtendedV6") && ((g_tsdb_configuration_flag & TSDB_MODE) ||(g_tsdb_configuration_flag & RTG_TSDB_MODE)) && (g_auto_mon_table[j].ns_state == ACTIVATE))
      {
        sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s%cTSDB_ACCESSLOG AccessLogStatsExtendedV6 -f %s -N 1 -y  V2 -F request:5,respTime:7,statusCode:6,respSize:-1 -S \"space\"  -h 1 -u nsec -A  Overall -R 1 -X TSDB_ACCESSLOG",
                              server_name, g_auto_mon_server_table[i].vector_name, global_settings->hierarchical_view_vector_separator,tsdb_access_log_file);
      }
      else if(g_auto_mon_server_table[i].server_type == SVR_TYPE_NO && !strcmp(g_auto_mon_table[j].mon_name, "SimulatedServiceStats") && (g_auto_mon_table[j].no_state == ACTIVATE))
      {
        sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s%cSimulatedServiceStats SimulatedServiceStats -c %s",
                              server_name, g_auto_mon_server_table[i].vector_name, global_settings->hierarchical_view_vector_separator, g_auto_mon_server_table[i].controller_name);
      }
      else if(g_auto_mon_server_table[i].server_type == SVR_TYPE_NO && !strcmp(g_auto_mon_table[j].mon_name, "SimulatedProtocolStats") && (g_auto_mon_table[j].no_state == ACTIVATE))
      {
        sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s%cSimulatedProtocolStats SimulatedProtocolStats -c %s",
                              server_name, g_auto_mon_server_table[i].vector_name, global_settings->hierarchical_view_vector_separator, g_auto_mon_server_table[i].controller_name);
      }
      else if(g_auto_mon_server_table[i].server_type == SVR_TYPE_NO && !strcmp(g_auto_mon_table[j].mon_name, "SERVER_SIGNATURE"))
      {
        sprintf(auto_mon_cmd, "SERVER_SIGNATURE %s %s File /var/www/hpd/conf/hpd.conf", g_auto_mon_server_table[i].vector_name,server_name);
      }
      /*g_enable_iperf_monitoring set 1 for Netcloud Test for Iperf monitoring 
	ENABLE_NS_MONITOR <mode> <iperf monitoring>
	Apply monitor only on Generators */
      else if(!strcmp(g_auto_mon_table[j].mon_name, "NetworkBandwidth") && g_enable_iperf_monitoring && (g_auto_mon_server_table[i].server_type == SVR_TYPE_GEN) && (g_auto_mon_table[j].ns_state == ACTIVATE))

      {
	sprintf(auto_mon_cmd , "STANDARD_MONITOR %s %s NetworkBandwidth -c %s -p %d", server_name, g_auto_mon_server_table[i].vector_name, global_settings->ctrl_server_ip , g_iperf_monitoring_port);
	fprintf(fauto_mprof, "%s\n", auto_mon_cmd);
	continue;
      }
      else if(!strcmp(g_auto_mon_table[j].mon_name, "TcpStatesCountV3") && (g_auto_mon_table[j].ns_state == ACTIVATE))
      {
        tcp_states_count_auto_mon(fauto_mprof, i , server_name);
        continue;
      }
      else if(!strcmp(g_auto_mon_table[j].mon_name, "DOCKER") && (g_auto_mon_server_table[i].server_type == SVR_TYPE_GEN) && (g_auto_mon_table[j].ns_state == ACTIVATE))
      {
        apply_docker_based_monitors_on_gen(fauto_mprof, i, server_name);
        continue;
      }
      else if(g_auto_mon_server_table[i].server_type == SVR_TYPE_NS && !strcmp(g_auto_mon_table[j].mon_name, "PostgreSQLDbConnectionStatistics") && (get_max_report_level_from_non_shr_mem() == 2) && (g_auto_mon_table[j].ns_state == ACTIVATE))
      { //monitoring test because test is a default database
        sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s%ctest PostgreSQLDbConnectionStatistics -R ${TIER}  -H ${SERVER}  -U cavisson -P  pXHClQV9rFIlke7UZuaS0A==  -h localhost -p 5432 -a postgres -f %s/MBean/XML/PostgreSQLDBConnectionStatistics.json",
                          server_name, g_auto_mon_server_table[i].vector_name, global_settings->hierarchical_view_vector_separator, g_ns_wdir);
        fprintf(fauto_mprof, "%s\n", auto_mon_cmd);
        continue;
      }
      else if(g_auto_mon_server_table[i].server_type == SVR_TYPE_NS && !strcmp(g_auto_mon_table[j].mon_name, "PostgreSQLBGWriterStats") && (get_max_report_level_from_non_shr_mem() == 2) && (g_auto_mon_table[j].ns_state == ACTIVATE))
      { //monitoring test because test is a default database
        sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s%ctest PostgreSQLBGWriterStats -R ${TIER}  -H ${SERVER}  -U cavisson -P  pXHClQV9rFIlke7UZuaS0A==  -h localhost -p 5432 -a postgres -f %s/MBean/XML/PostgreSQLBgwriterStats.json",
                          server_name, g_auto_mon_server_table[i].vector_name, global_settings->hierarchical_view_vector_separator, g_ns_wdir);
        fprintf(fauto_mprof, "%s\n", auto_mon_cmd);
        continue;
      }
      else if(g_auto_mon_server_table[i].server_type == SVR_TYPE_NS && !strcmp(g_auto_mon_table[j].mon_name, "PostgreSQLDBActivityStats") && (get_max_report_level_from_non_shr_mem() == 2) && (g_auto_mon_table[j].ns_state == ACTIVATE))
      { //monitoring test because test is a default database
        sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s%ctest PostgreSQLDBActivityStats -R ${TIER}  -H ${SERVER}  -U cavisson -P  pXHClQV9rFIlke7UZuaS0A==  -h localhost -p 5432 -a postgres -f %s/MBean/XML/PostgreSQLDatabaseStats.json",
                          server_name, g_auto_mon_server_table[i].vector_name, global_settings->hierarchical_view_vector_separator, g_ns_wdir);
        fprintf(fauto_mprof, "%s\n", auto_mon_cmd);
        continue;
      }
      else if(g_auto_mon_server_table[i].server_type == SVR_TYPE_NS && !strcmp(g_auto_mon_table[j].mon_name, "PostgreSQLQueryResponseTime") && (get_max_report_level_from_non_shr_mem() == 2) && (g_auto_mon_table[j].ns_state == ACTIVATE))
      { //monitoring test because test is a default database
        sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s%ctest PostgreSQLQueryResponseTime -R ${TIER}  -H ${SERVER}  -U cavisson -P  pXHClQV9rFIlke7UZuaS0A==  -h localhost -p 5432 -a test -f %s/MBean/XML/PostgreSQLDbQueryResponseTime.json",
                          server_name, g_auto_mon_server_table[i].vector_name, global_settings->hierarchical_view_vector_separator, g_ns_wdir);
        fprintf(fauto_mprof, "%s\n", auto_mon_cmd);
        continue;
      } 
      else if(g_auto_mon_server_table[i].server_type == SVR_TYPE_NS && !strcmp(g_auto_mon_table[j].mon_name, "PostgreSQLDbTableHitRatio") && (get_max_report_level_from_non_shr_mem() == 2) && (g_auto_mon_table[j].ns_state == ACTIVATE)) 
      { //monitoring test because test is a default database
        sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s%ctest PostgreSQLDbTableHitRatio -R ${TIER}  -H ${SERVER}  -U cavisson -P  pXHClQV9rFIlke7UZuaS0A==  -h localhost -p 5432 -a test -f %s/MBean/XML/PostgreSQLDbTableHeapHitRatio.json",
                          server_name, g_auto_mon_server_table[i].vector_name, global_settings->hierarchical_view_vector_separator, g_ns_wdir);
        fprintf(fauto_mprof, "%s\n", auto_mon_cmd);
        continue;
      }
      else if(g_auto_mon_server_table[i].server_type == SVR_TYPE_NS && !strcmp(g_auto_mon_table[j].mon_name, "PostgreSQLIOActivityStats") && (get_max_report_level_from_non_shr_mem() == 2) && (g_auto_mon_table[j].ns_state == ACTIVATE))
      { //monitoring test because test is a default database
        sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s%ctest PostgreSQLIOActivityStats -R ${TIER}  -H ${SERVER}  -U cavisson -P  pXHClQV9rFIlke7UZuaS0A==  -h localhost -p 5432 -a test -f %s/MBean/XML/PostgreSQLIOActivityStats.json",
                          server_name, g_auto_mon_server_table[i].vector_name, global_settings->hierarchical_view_vector_separator, g_ns_wdir);
        fprintf(fauto_mprof, "%s\n", auto_mon_cmd);
        continue;
      }
      else if(g_auto_mon_server_table[i].server_type == SVR_TYPE_NS && !strcmp(g_auto_mon_table[j].mon_name, "PostgreSQLExecutionStats") && (get_max_report_level_from_non_shr_mem() == 2) && (g_auto_mon_table[j].ns_state == ACTIVATE))
      { //monitoring test because test is a default database
        sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s%ctest PostgreSQLExecutionStats --tier ${TIER} --server ${SERVER} --user cavisson --pwd pXHClQV9rFIlke7UZuaS0A== --maxRetryCount 60 --executeTime 24 --loadFactor 20 --host localhost --port 5432 --isEncryptPwd --dbName test",
                          server_name, g_auto_mon_server_table[i].vector_name, global_settings->hierarchical_view_vector_separator);
        fprintf(fauto_mprof, "%s\n", auto_mon_cmd);
        continue;
      }
      /*else if(g_auto_mon_server_table[i].server_type == SVR_TYPE_NS && !strcmp(g_auto_mon_table[j].mon_name, "PostgreSQLDetailIOActivityStats") && (get_max_report_level_from_non_shr_mem() == 2))
      { //monitoring test because test is a default database
        sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s%ctest PostgreSQLDetailIOActivityStats --tier ${TIER} --server ${SERVER} --user cavisson --pwd pXHClQV9rFIlke7UZuaS0A== --maxRetryCount 60 --host localhost --port 5432 --isEncryptPwd --dbName test",
                          server_name, g_auto_mon_server_table[i].vector_name, global_settings->hierarchical_view_vector_separator);
        fprintf(fauto_mprof, "%s\n", auto_mon_cmd);
        continue;
      }*/
      else if(g_auto_mon_server_table[i].server_type == SVR_TYPE_NS && !strcmp(g_auto_mon_table[j].mon_name, "PostgreSQLBlockingStats") && (get_max_report_level_from_non_shr_mem() == 2) && (g_auto_mon_table[j].ns_state == ACTIVATE))
      { //monitoring test because test is a default database
        sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s%ctest PostgreSQLBlockingStats --tier ${TIER} --server ${SERVER} --user cavisson --pwd pXHClQV9rFIlke7UZuaS0A== --vecPersistCount 5 --maxRetryCount 60 --host localhost --port 5432 --isEncryptPwd --dbName test",
                          server_name, g_auto_mon_server_table[i].vector_name, global_settings->hierarchical_view_vector_separator);
        fprintf(fauto_mprof, "%s\n", auto_mon_cmd);
        continue;
      }
      else if(g_auto_mon_server_table[i].server_type == SVR_TYPE_NS && !strcmp(g_auto_mon_table[j].mon_name, "PostgreSQLTempDBStats") && (get_max_report_level_from_non_shr_mem() == 2) && (g_auto_mon_table[j].ns_state == ACTIVATE))
      { //monitoring test because test is a default database
        sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s%ctest PostgreSQLTempDBStats --tier ${TIER} --server ${SERVER} --user cavisson --pwd pXHClQV9rFIlke7UZuaS0A== --vecPersistCount 5 --maxRetryCount 60 --host localhost --port 5432 --isEncryptPwd --dbName test",
                          server_name, g_auto_mon_server_table[i].vector_name, global_settings->hierarchical_view_vector_separator);
        fprintf(fauto_mprof, "%s\n", auto_mon_cmd);
        continue;
      }
      else if(g_auto_mon_server_table[i].server_type == SVR_TYPE_NS && !strcmp(g_auto_mon_table[j].mon_name, "PostgreSQLLocksStats") && (get_max_report_level_from_non_shr_mem() == 2) && (g_auto_mon_table[j].ns_state == ACTIVATE)) 
      { //monitoring test because test is a default database
        sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s%ctest PostgreSQLLocksStats --tier ${TIER} --server ${SERVER} --user cavisson --pwd pXHClQV9rFIlke7UZuaS0A== --vecPersistCount 5 --maxRetryCount 60 --host localhost --port 5432 --isEncryptPwd --dbName test",
                          server_name, g_auto_mon_server_table[i].vector_name, global_settings->hierarchical_view_vector_separator);
        fprintf(fauto_mprof, "%s\n", auto_mon_cmd);
        continue;
      }
      else if(g_auto_mon_server_table[i].server_type == SVR_TYPE_NS && !strcmp(g_auto_mon_table[j].mon_name, "PostgreSQLSessionStats") && (get_max_report_level_from_non_shr_mem() == 2) && (g_auto_mon_table[j].ns_state == ACTIVATE))
      { //monitoring test because test is a default database
        sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s%ctest PostgreSQLSessionStats --tier ${TIER} --server ${SERVER} --user cavisson --pwd pXHClQV9rFIlke7UZuaS0A== --vecPersistCount 5 --maxRetryCount 60 --host localhost --port 5432 --isEncryptPwd --dbName test",
                          server_name, g_auto_mon_server_table[i].vector_name, global_settings->hierarchical_view_vector_separator);
        fprintf(fauto_mprof, "%s\n", auto_mon_cmd);
        continue;
      }
      else if(g_auto_mon_server_table[i].server_type == SVR_TYPE_NS && !strcmp(g_auto_mon_table[j].mon_name, "PostgreSQLConnectionStats") && (get_max_report_level_from_non_shr_mem() == 2) && (g_auto_mon_table[j].ns_state == ACTIVATE))
      { //monitoring test because test is a default database
        sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s%ctest PostgreSQLConnectionStats --tier ${TIER} --server ${SERVER} --user cavisson --pwd pXHClQV9rFIlke7UZuaS0A== --vecPersistCount 5 --maxRetryCount 60 --host localhost --port 5432 --isEncryptPwd --dbName test",
                          server_name, g_auto_mon_server_table[i].vector_name, global_settings->hierarchical_view_vector_separator);
        fprintf(fauto_mprof, "%s\n", auto_mon_cmd);
        continue;
      }
      else if(g_auto_mon_server_table[i].server_type == SVR_TYPE_NS && !strcmp(g_auto_mon_table[j].mon_name, "PostgreSQLAccesslog") && (get_max_report_level_from_non_shr_mem() == 2) && (g_auto_mon_table[j].ns_state == ACTIVATE))
      { //monitoring test because test is a default database
        sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s%ctest PostgreSQLAccesslog --tier ${TIER} --server ${SERVER} --logFile /home/cavisson/work/webapps/logs/pg_log/pos* --fileCleanTime 60 --fileCount 5",
                          server_name, g_auto_mon_server_table[i].vector_name, global_settings->hierarchical_view_vector_separator);
        fprintf(fauto_mprof, "%s\n", auto_mon_cmd);
        continue;
      }
      else if(g_auto_mon_server_table[i].server_type == SVR_TYPE_NS && !strcmp(g_auto_mon_table[j].mon_name, "PostgreSQLLockModeStats") && (get_max_report_level_from_non_shr_mem() == 2) && (g_auto_mon_table[j].ns_state == ACTIVATE))
      { //monitoring test because test is a default database
        sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s%ctest PostgreSQLLockModeStats -R ${TIER}  -H ${SERVER}  -U cavisson -P  pXHClQV9rFIlke7UZuaS0A==  -h localhost -p 5432 -a postgres -f %s/MBean/XML/PostgreSQLLockModeStats.json",
                          server_name, g_auto_mon_server_table[i].vector_name, global_settings->hierarchical_view_vector_separator, g_ns_wdir);
        fprintf(fauto_mprof, "%s\n", auto_mon_cmd);
        continue;
      }
      else if(!strcmp(g_auto_mon_table[j].mon_name, "NetworkDelay") && (g_auto_mon_table[j].ns_state == ACTIVATE))
      {
        if (strcmp(g_auto_mon_server_table[i].server_name, LOOPBACK_IP_PORT))    
        { 
          //extract tier name from vector name
          //strcpy(hv_sep, &global_settings->hierarchical_view_vector_separator);
          hv_sep[0] = global_settings->hierarchical_view_vector_separator;
          hv_sep[1] = '\0';

          if((ptr = strstr(g_auto_mon_server_table[i].vector_name, hv_sep)) != NULL) 
            strncpy(tier_name, g_auto_mon_server_table[i].vector_name, ptr - g_auto_mon_server_table[i].vector_name);
          else
          {
            fprintf(stderr, "Error: Vector name format '%s' not correct. In hierarchical mode vector name format must be " 
                            "'Tier<vector separator>Server'.\n", g_auto_mon_server_table[i].vector_name);
            continue;
          }

         /*set machine name (NS -> NSAppliance & Netcloud -> Controller) and vector name 
          *For ENABLE_NO_MONITOR (in NS & Netcloud both) : vector name will be server name given in scenario with keyword ENABLE_NO_MONITOR
          *For Netcloud (with ENABLE_NS_MONITOR & ENABLE_NO_MONITOR both) : vector name will be generator name  */
          if((loader_opcode == STAND_ALONE) || (strstr(g_auto_mon_server_table[i].vector_name, "NOAppliance") != NULL))
          {
            strcpy(ns_or_controller, "NSAppliance");
            strncpy(vector_name, g_auto_mon_server_table[i].server_name, MAX_NAME_LEN);
          }
          else
          {
            strcpy(ns_or_controller, "Controller");

            strcpy(vector_name, ptr + 1);
          }

          sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s%c%s%c%s NetworkDelay -s %s ",
                                   server_name, tier_name, global_settings->hierarchical_view_vector_separator, ns_or_controller,
                                   global_settings->hierarchical_view_vector_separator, vector_name, g_auto_mon_server_table[i].server_name); 
        }
        else
         continue;
      }//We don't want any monitor on generator.Hence,skipping.
      else if((!strcmp(g_auto_mon_table[j].mon_name, "JMX_TOMCAT")) && ((g_auto_mon_server_table[i].server_type)!= SVR_TYPE_GEN) && (g_auto_mon_table[j].ns_state == ACTIVATE))
      {
        if (getenv("TOMCAT_DIR") != NULL)
          strcpy(tomcat_dir, getenv("TOMCAT_DIR"));
        else
        {
          NSTL1_OUT(NULL, NULL, "Tomcat directory not found\n");
          NS_EXIT(-1, CAV_ERR_1000027, "TOMCAT_DIR");
        }

        if(tomcat_dir[0] != '\0')
          sprintf(access_log_file,"%s/logs/localhost_access_log.*",tomcat_dir);

        sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s%cOverall AccessLogV6 -A TOMCAT%cOverall -p max_resp_time:100000 -F request:5,statusCode:6,respTime:7,respSize:8 -S \"space\" -o -h 0 -M 1 -u mls -X TOMCAT -f %s -U \"M:5,P:'ProductUI',V:TOMCAT%cProductUI\" -U \"M:5,P:'ACL',V:TOMCAT%cACL\" -U \"M:5,P:'getAccessControl',V:TOMCAT%cGetAccessControl\" -U \"M:5,P:'DashboardDataService/data',V:TOMCAT%cDashboardData\" -U \"M:5,P:'newDataPacket',V:TOMCAT%cGetFavPanelData\" -U \"M:5,P:'alertCounter',V:TOMCAT%cAlertCounter\" -U \"M:5,P:'DashBoardServerServlet',V:TOMCAT%cDashBoardServerServlet\" -U \"M:5,P:'loadFavData',V:TOMCAT%cLoadFavData\" -U \"M:5,P:'graphTimeService',V:TOMCAT%cGraphTime\" -U \"M:5,P:'zoomChart',V:TOMCAT%cZoomChart\" -U \"M:5,P:'downloadLowerPanelData',V:TOMCAT%cLowerPane\" -U \"M:5,P:'changeGraphColor',V:TOMCAT%cChangeGraphColor\" -U \"M:5,P:'convertChart',V:TOMCAT%cConvertChart\" -U \"M:5,P:'generateDerivedGraph',V:TOMCAT%cDerivedGraph\" -U \"M:5,P:'initTree',V:TOMCAT%cTreeOperations\" -U \"M:5,P:'dropTreeData',V:TOMCAT%cDashboardDropMetricFromTree\" -U \"M:5,P:'treeOperations',V:TOMCAT%cDashboardOpenMergeTreeOperation\" -U \"M:5,P:'getActiveAlerts',V:TOMCAT%cAlertGetActiveAlerts\" -U \"M:5,P:'getAlertHistoryData',V:TOMCAT%cAlertGetAlertHistoryData\" -U \"M:5,P:'getMaintenanceSettings',V:TOMCAT%cAlertGetMaintenanceSetting\" -U \"M:5,P:'getAlertSettings',V:TOMCAT%cAlertGetAlertSettings\" -U \"M:5,P:'alertHistoryDataForOverlay',V:TOMCAT%cAlertHistoryDataForOverlay\" -U \"M:5,P:'runCommandInfo',V:TOMCAT%cRunCommand\" -U \"M:5,P:'runCommandOnServer',V:TOMCAT%cRunCommandOnServer\" -U \"M:5,P:'takeThreadDump',V:TOMCAT%cDashboardTakeThreadDump\" -U \"M:5,P:'viewThreadDump',V:TOMCAT%cDashboardViewThreadDumps\" -U \"M:5,P:'parseThreadDumpFile',V:TOMCAT%cDashboardThreadDumps\" -U \"M:5,P:'getColorRules',V:TOMCAT%cGetColorRules\" -U \"M:5,P:'colorManagement',V:TOMCAT%cColorManagement\" -U \"M:5,P:'saveFavData',V:TOMCAT%cCreateFavorite\"",
                          server_name, g_auto_mon_server_table[i].vector_name, global_settings->hierarchical_view_vector_separator,global_settings->hierarchical_view_vector_separator, access_log_file, global_settings->hierarchical_view_vector_separator, global_settings->hierarchical_view_vector_separator, global_settings->hierarchical_view_vector_separator, global_settings->hierarchical_view_vector_separator, global_settings->hierarchical_view_vector_separator, global_settings->hierarchical_view_vector_separator, global_settings->hierarchical_view_vector_separator, global_settings->hierarchical_view_vector_separator, global_settings->hierarchical_view_vector_separator, global_settings->hierarchical_view_vector_separator, global_settings->hierarchical_view_vector_separator, global_settings->hierarchical_view_vector_separator, global_settings->hierarchical_view_vector_separator, global_settings->hierarchical_view_vector_separator, global_settings->hierarchical_view_vector_separator, global_settings->hierarchical_view_vector_separator, global_settings->hierarchical_view_vector_separator, global_settings->hierarchical_view_vector_separator, global_settings->hierarchical_view_vector_separator, global_settings->hierarchical_view_vector_separator, global_settings->hierarchical_view_vector_separator, global_settings->hierarchical_view_vector_separator, global_settings->hierarchical_view_vector_separator, global_settings->hierarchical_view_vector_separator, global_settings->hierarchical_view_vector_separator, global_settings->hierarchical_view_vector_separator, global_settings->hierarchical_view_vector_separator, global_settings->hierarchical_view_vector_separator, global_settings->hierarchical_view_vector_separator, global_settings->hierarchical_view_vector_separator);

        fprintf(fauto_mprof, "%s\n", auto_mon_cmd);

        if(tomcat_dir[0] != '\0')
        {
          //Reusing buffer access_log_file
          //sprintf(access_log_file, "%s/logs/tomcat.pid",tomcat_dir);    
          sprintf(access_log_file, "%s/.pidfiles/.TOMCAT.pid",g_ns_wdir);        
    
          sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s%cTOMCAT TomcatCacheStats -X TOMCAT -f %s", server_name, g_auto_mon_server_table[i].vector_name, global_settings->hierarchical_view_vector_separator,access_log_file);
          fprintf(fauto_mprof, "%s\n", auto_mon_cmd);
          sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s%cTOMCAT TomcatThreadPoolStats -X TOMCAT -f %s", server_name, g_auto_mon_server_table[i].vector_name, global_settings->hierarchical_view_vector_separator,access_log_file);
          fprintf(fauto_mprof, "%s\n", auto_mon_cmd);
          sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s%cTOMCAT TomcatSessionManagerStats -X TOMCAT -f %s", server_name, g_auto_mon_server_table[i].vector_name, global_settings->hierarchical_view_vector_separator,access_log_file);
          fprintf(fauto_mprof, "%s\n", auto_mon_cmd);
          sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s%cTOMCAT TomcatServletStats -X TOMCAT -f %s", server_name, g_auto_mon_server_table[i].vector_name, global_settings->hierarchical_view_vector_separator,access_log_file);
          fprintf(fauto_mprof, "%s\n", auto_mon_cmd);
          sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s%cTOMCAT TomcatServiceStats -X TOMCAT -f %s", server_name, g_auto_mon_server_table[i].vector_name, global_settings->hierarchical_view_vector_separator,access_log_file);
          fprintf(fauto_mprof, "%s\n", auto_mon_cmd);
          sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s%cTOMCAT JavaThreadingStats -f %s", server_name, g_auto_mon_server_table[i].vector_name, global_settings->hierarchical_view_vector_separator,access_log_file);
          fprintf(fauto_mprof, "%s\n", auto_mon_cmd);
          sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s%cTOMCAT JavaOperatingSystemStats -f %s",
                          server_name, g_auto_mon_server_table[i].vector_name, global_settings->hierarchical_view_vector_separator,access_log_file);
          fprintf(fauto_mprof, "%s\n", auto_mon_cmd);  
          sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s%cTOMCAT JavaGCJMXSun8 -f %s", server_name, g_auto_mon_server_table[i].vector_name, global_settings->hierarchical_view_vector_separator,access_log_file);
          fprintf(fauto_mprof, "%s\n", auto_mon_cmd);
        }
        else
          NSTL1(NULL, NULL, "TOMCAT_DIR is not available, Couldnt apply TOMCAT monitors.");

        continue;
      }
      else if((!strcmp(g_auto_mon_table[j].mon_name, "JMX_CMON")) && ((g_auto_mon_server_table[i].server_type)!= SVR_TYPE_GEN) && !(g_java_mon_flag_cmon) && (g_auto_mon_table[j].ns_state == ACTIVATE))
      {
        sprintf(file_path, "/home/cavisson/monitors/sys/cmon.pid");
         
        sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s%cCMON JavaThreadingStats -f %s", 
      		  server_name, g_auto_mon_server_table[i].vector_name, global_settings->hierarchical_view_vector_separator,file_path);
        fprintf(fauto_mprof, "%s\n", auto_mon_cmd);
        sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s%cCMON JavaOperatingSystemStats -f %s",
                        server_name, g_auto_mon_server_table[i].vector_name, global_settings->hierarchical_view_vector_separator,file_path);
        fprintf(fauto_mprof, "%s\n", auto_mon_cmd);
        sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s%cCMON JavaGCJMXSun8 -f %s", server_name, g_auto_mon_server_table[i].vector_name, global_settings->hierarchical_view_vector_separator,file_path);
        fprintf(fauto_mprof, "%s\n", auto_mon_cmd);

        continue;
      }

/******************************* PROCESS MONITOR STARTS *******************************************/
      else if(!strcmp(g_auto_mon_table[j].mon_name, "JavaProcessMonitoring") && ((g_auto_mon_server_table[i].server_type)!= SVR_TYPE_GEN) && (g_auto_mon_table[j].ns_state == ACTIVATE)) 

      {
        if(g_auto_ns_mon_flag)
        {
          char controller_name[128];
          if(get_controller_name(controller_name) == MON_FAILURE)
            return MON_FAILURE;
          if(enable_store_config  && (!strcmp(server_name, "127.0.0.1")))
          {
            sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s%cAutoMon ProcessDataExV3 -T TR%d -C %s -v 10000",
                             server_name, g_auto_mon_server_table[i].vector_name, global_settings->hierarchical_view_vector_separator,testidx, controller_name);
          }
          else   
          { 
            sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s%cAutoMon ProcessDataExV3 -T TR%d -C %s -v 10000",
                server_name, g_auto_mon_server_table[i].vector_name,global_settings->hierarchical_view_vector_separator,testidx, controller_name);
          }
          fprintf(fauto_mprof, "%s\n", auto_mon_cmd);
        }
        continue;
      }

      else 
      {
        if(!strcmp(g_auto_mon_table[j].mon_name, "SimulatedServiceStats") || 
           !strcmp(g_auto_mon_table[j].mon_name, "SimulatedProtocolStats") || 
           !strcmp(g_auto_mon_table[j].mon_name, "SERVER_SIGNATURE") || 
           !strcmp(g_auto_mon_table[j].mon_name, "LPS") ||
           !strcmp(g_auto_mon_table[j].mon_name, "DBUpload1") ||
           !strcmp(g_auto_mon_table[j].mon_name, "NLR") ||
           !strcmp(g_auto_mon_table[j].mon_name, "NLW") ||
           !strcmp(g_auto_mon_table[j].mon_name, "NLM") ||
           !strcmp(g_auto_mon_table[j].mon_name, "NDP") ||
           !strcmp(g_auto_mon_table[j].mon_name, "NDC") ||
           !strcmp(g_auto_mon_table[j].mon_name, "DBUpload2") ||
           !strcmp(g_auto_mon_table[j].mon_name, "PostgreSQLDbConnectionStatistics") ||
           !strcmp(g_auto_mon_table[j].mon_name, "PostgreSQLBGWriterStats") ||
           !strcmp(g_auto_mon_table[j].mon_name, "PostgreSQLDBActivityStats") ||
           !strcmp(g_auto_mon_table[j].mon_name, "PostgreSQLQueryResponseTime") ||
           !strcmp(g_auto_mon_table[j].mon_name, "PostgreSQLDbTableHitRatio") ||
           !strcmp(g_auto_mon_table[j].mon_name, "PostgreSQLIOActivityStats") ||
           !strcmp(g_auto_mon_table[j].mon_name, "PostgreSQLLockModeStats") ||
           !strcmp(g_auto_mon_table[j].mon_name, "PostgreSQLExecutionStats") ||
           !strcmp(g_auto_mon_table[j].mon_name, "PostgreSQLBlockingStats") ||
 //          !strcmp(g_auto_mon_table[j].mon_name, "PostgreSQLDetailIOActivityStats") ||
           !strcmp(g_auto_mon_table[j].mon_name, "PostgreSQLTempDBStats") ||
           !strcmp(g_auto_mon_table[j].mon_name, "PostgreSQLLocksStats") ||
           !strcmp(g_auto_mon_table[j].mon_name, "PostgreSQLSessionStats") ||
           !strcmp(g_auto_mon_table[j].mon_name, "PostgreSQLConnectionStats") ||
           !strcmp(g_auto_mon_table[j].mon_name, "PostgreSQLAccesslog") ||
           !strcmp(g_auto_mon_table[j].mon_name, "HPD") ||
           !strcmp(g_auto_mon_table[j].mon_name, "TOMCAT") ||
           !strcmp(g_auto_mon_table[j].mon_name, "NVDU") ||
           !strcmp(g_auto_mon_table[j].mon_name, "PsIoStatDBUpload2") ||
           !strcmp(g_auto_mon_table[j].mon_name, "PsIoStatNDC") ||
           !strcmp(g_auto_mon_table[j].mon_name, "PsIoStatCMON") ||
           !strcmp(g_auto_mon_table[j].mon_name, "PsIoStatLPS") ||
           !strcmp(g_auto_mon_table[j].mon_name, "PsIoStatNDP") ||
           !strcmp(g_auto_mon_table[j].mon_name, "PsIoStatTOMCAT") ||
           !strcmp(g_auto_mon_table[j].mon_name, "PsIoStatNDEMain") ||
           !strcmp(g_auto_mon_table[j].mon_name, "PsIoStatDBUpload1") ||
           !strcmp(g_auto_mon_table[j].mon_name, "PsIoStatNLR") ||
           !strcmp(g_auto_mon_table[j].mon_name, "PsIoStatNLW") ||
           !strcmp(g_auto_mon_table[j].mon_name, "PsIoStatNLM") ||
           !strcmp(g_auto_mon_table[j].mon_name, "PsIoStatHPD") ||
           !strcmp(g_auto_mon_table[j].mon_name, "PsIoStatNVDU") ||
           !strcmp(g_auto_mon_table[j].mon_name, "CMON") || //if ENABLE_CMON_AGENT is 1 then do not add process data for cmon through ENABLE_NS_MONITOR
           !strcmp(g_auto_mon_table[j].mon_name, "JMX_TOMCAT") || //We don't want any monitor on generator.Hence,skipping.
           !strcmp(g_auto_mon_table[j].mon_name, "JMX_CMON") ||
           !strcmp(g_auto_mon_table[j].mon_name, "NCMain") ||
	   !strcmp(g_auto_mon_table[j].mon_name, "JavaProcessMonitoring") ||
	   !strcmp(g_auto_mon_table[j].mon_name, "NetworkBandwidth") ||
	   !strcmp(g_auto_mon_table[j].mon_name, "DOCKER") ||
	   !strcmp(g_auto_mon_table[j].mon_name, "AccessLogStatsExtendedV6"))

           
        continue;

        sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s %s ", 
                               server_name, g_auto_mon_server_table[i].vector_name, g_auto_mon_table[j].mon_name);
      }

      NSDL3_SCHEDULE(NULL, NULL, "Adding monitor: [%s]", auto_mon_cmd);
      //write monitor syntax into .auto.mprof file 
      fprintf(fauto_mprof, "%s\n", auto_mon_cmd);
    }
    fprintf(fauto_mprof, "\n");
  } 
 
  //Add cmon monitors and auto server signature for all server
  NSDL3_SCHEDULE(NULL, NULL, "g_cmon_agent_flag = %d, g_auto_server_sig_flag = %d", g_cmon_agent_flag, g_auto_server_sig_flag);
  if(g_cmon_agent_flag || g_auto_server_sig_flag)
  {
    fprintf(fauto_mprof, "##### Cmon Process Data monitors & Auto Server Signatures #####\n");

    for(i = 0; i < total_cmon_agent_server_entries; i++)
    {
      strcpy(server_name, g_cmon_agent_server_table[i].tier_server); 
      if(g_cmon_agent_flag) //Cmon Process Data monitors
      {
        strcpy(server_name, g_cmon_agent_server_table[i].tier_server);

        NSDL3_SCHEDULE(NULL, NULL, "cmon agent server idx = %d", i);
        if( (g_cmon_agent_server_table[i].server_type == AIX) || (g_cmon_agent_server_table[i].server_type == HPUX_MACHINE) )
        {
          //process data monitor with "-f" option taking pid from cmon.pid
          if(enable_store_config  && (!strcmp(g_cmon_agent_server_table[i].server_name, "127.0.0.1")))
          { 
            sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s!%s ProcessData -f %s/sys/cmon.pid\n",
                             server_name,g_store_machine,g_cmon_agent_server_table[i].vector_name,
                             g_cmon_agent_server_table[i].cavmon_home);
          }
          else
          { 
            sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s ProcessData -f %s/sys/cmon.pid\n",
                             server_name, g_cmon_agent_server_table[i].vector_name,
                             g_cmon_agent_server_table[i].cavmon_home);
          }
        }
        else if(g_cmon_agent_server_table[i].server_type == WIN_MACHINE)
        {
           continue;
        }
        else 
        {
          //process data monitor with "-f" option taking pid from cmon.pid
          if(enable_store_config && (!strcmp(g_cmon_agent_server_table[i].server_name, "127.0.0.1")))
          {
            sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s!%s ProcessDataExV2 -f %s/sys/cmon.pid\n",
                             server_name,g_store_machine,g_cmon_agent_server_table[i].vector_name,
                             g_cmon_agent_server_table[i].cavmon_home);
          }
          else
          {
            sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s ProcessDataExV2 -f %s/sys/cmon.pid\n", 
                             server_name, g_cmon_agent_server_table[i].vector_name,
                             g_cmon_agent_server_table[i].cavmon_home);
          }
        }
          //write monitor syntax into .auto.mprof file
          fprintf(fauto_mprof, "%s\n", auto_mon_cmd);
      }
   
      if(g_cmon_agent_flag && g_java_mon_flag_cmon)
      {
        if(enable_store_config && (!strcmp(g_cmon_agent_server_table[i].server_name, "127.0.0.1")))
        { 
          sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s!%s JavaThreadingStats -f %s/sys/cmon.pid\n",
                             server_name ,g_store_machine,g_cmon_agent_server_table[i].vector_name,
                             g_cmon_agent_server_table[i].cavmon_home);
          
          fprintf(fauto_mprof, "%s\n", auto_mon_cmd);
     
          //code for applying JVM JavaGCJMXSun8
          sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s!%s JavaGCJMXSun8 -f %s/sys/cmon.pid\n",
                             server_name,g_store_machine,g_cmon_agent_server_table[i].vector_name,
                             g_cmon_agent_server_table[i].cavmon_home);
 
          fprintf(fauto_mprof, "%s\n", auto_mon_cmd);
        }
        else 
        {
           sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s JavaThreadingStats -f %s/sys/cmon.pid\n",
                             server_name, g_cmon_agent_server_table[i].vector_name,
                             g_cmon_agent_server_table[i].cavmon_home);

          fprintf(fauto_mprof, "%s\n", auto_mon_cmd);

          //code for applying JVM JavaGCJMXSun8
          sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s JavaGCJMXSun8 -f %s/sys/cmon.pid\n",
                             server_name, g_cmon_agent_server_table[i].vector_name,
                             g_cmon_agent_server_table[i].cavmon_home);

          fprintf(fauto_mprof, "%s\n", auto_mon_cmd);          
        }  
      }
      if(g_auto_server_sig_flag) //Auto Server Signatures
      {
        if( g_cmon_agent_server_table[i].server_type == LINUX )
        {
          sprintf(auto_mon_cmd, "SERVER_SIGNATURE %s %s>ps_auto Command %s",server_name, g_cmon_agent_server_table[i].vector_name,PS_LINUX);
          fprintf(fauto_mprof, "%s\n", auto_mon_cmd); // for ps command output

          sprintf(auto_mon_cmd, "SERVER_SIGNATURE %s %s>df_auto Command %s",server_name, g_cmon_agent_server_table[i].vector_name,DF_LINUX);
          fprintf(fauto_mprof, "%s\n", auto_mon_cmd); // for df command output
        }
        else if( g_cmon_agent_server_table[i].server_type == AIX )
        {
          sprintf(auto_mon_cmd, "SERVER_SIGNATURE %s %s>ps_auto Command %s",server_name, g_cmon_agent_server_table[i].vector_name,PS_LINUX);
          fprintf(fauto_mprof, "%s\n", auto_mon_cmd); // for ps command output

          sprintf(auto_mon_cmd, "SERVER_SIGNATURE %s %s>df_auto Command %s",server_name, g_cmon_agent_server_table[i].vector_name,DF_AIX);
          fprintf(fauto_mprof, "%s\n", auto_mon_cmd); // for df command output
        }
        else if( g_cmon_agent_server_table[i].server_type == SOLARIS )
        {
          sprintf(auto_mon_cmd, "SERVER_SIGNATURE %s %s>ps_auto Command %s",server_name, g_cmon_agent_server_table[i].vector_name,PS_SOLARIS);
          fprintf(fauto_mprof, "%s\n", auto_mon_cmd); // for ps command output

          sprintf(auto_mon_cmd, "SERVER_SIGNATURE %s %s>df_auto Command %s",server_name, g_cmon_agent_server_table[i].vector_name,DF_SOLARIS);
          fprintf(fauto_mprof, "%s\n", auto_mon_cmd); // for df command output
        }
        if (g_cmon_agent_server_table[i].server_type != WIN_MACHINE && g_cmon_agent_server_table[i].server_type != HPUX_MACHINE )
        {
          sprintf(auto_mon_cmd, "SERVER_SIGNATURE %s %s>meminfo_auto Command %s",server_name, g_cmon_agent_server_table[i].vector_name,MEMINFO);
          fprintf(fauto_mprof, "%s\n", auto_mon_cmd); // for vmstat command output
        }
      } 
      fprintf(fauto_mprof, "\n"); 
    }
    fprintf(fauto_mprof, "\n");
  }  
  pclose(fauto_mprof);

  //Parse .auto.mprof file
  NSDL3_SCHEDULE(NULL, NULL, "Opening '%s' in read mode", auto_mprof);
  FILE *fp = fopen(auto_mprof, "r");
  if (fp == NULL)
  {
    error_log("Error in opening file %s", auto_mprof);
    return -1;
  }

  NSDL3_SCHEDULE(NULL, NULL, "Parsing mprof '%s'", auto_mprof);
  read_keywords_frm_monitor_profile(auto_mprof, fp, 1, 0, err_msg, NULL, wdir);
  CLOSE_FP(fp);

  //Free memory
  NSDL3_SCHEDULE(NULL, NULL, "Freeing tables AutoMonTable, and AutoMonSvrTable.");
  FREE_AND_MAKE_NULL(g_auto_mon_table, "Freeing AutoMonTable.", -1);
  FREE_AND_MAKE_NULL(g_auto_mon_server_table, "Freeing AutoMonSvrTable.", -1);
  FREE_AND_MAKE_NULL(g_cmon_agent_server_table, "Freeing Cmon Agent server Array", -1);

  return 0;
}

int apply_jmeter_monitors()
{
  char err_msg[1024];  
  char auto_mprof[MAX_NAME_LEN]; 
  FILE *fauto_mprof = NULL;
  char server_name[1024];
   
  char buff[4096];
  sprintf(auto_mprof, "%s/logs/TR%d/common_files/ns_files/ns_auto.mprof", g_ns_wdir, testidx);
  
  if ((fauto_mprof = fopen(auto_mprof, "a+")) == NULL)
  { 
    fprintf(stderr, "error in creating auto mprof file\n");
    perror("fopen");
    return MON_FAILURE;
  }
  
  int switch_flag = 0;
  for (int j = 0; j < total_runprof_entries; j++)
  {
    if(g_script_or_url == SCRIPT_TYPE_JMETER)
    {
      if(loader_opcode == MASTER_LOADER)
      {
        for(int i = 0; i<sgrp_used_genrator_entries; i++)
        {
          sprintf(server_name,"Cavisson%c%s",global_settings->hierarchical_view_vector_separator, generator_entry[i].gen_name);
             
          sprintf(buff, "STANDARD_MONITOR %s %s%cJMeter_%s_%d JavaGCJMXSun8 -f %s/logs/TR%d/.pidfiles/.JMeter_%s_%d.pid", server_name, server_name, global_settings->hierarchical_view_vector_separator, runprof_table_shr_mem[j].sess_ptr->sess_name, j, generator_entry[i].work, generator_entry[i].testidx, runprof_table_shr_mem[j].sess_ptr->sess_name, j);
          
          fprintf(fauto_mprof, "%s\n", buff);
          fflush(fauto_mprof);
          if(kw_set_standard_monitor("STANDARD_MONITOR",buff, 1, NULL, err_msg, NULL) < 0)
             NSDL2_MON(NULL, NULL,"Error in applying JavaGC monitor for Generator %s. Error: %s\n", generator_entry[i].gen_name, err_msg);
          sprintf(buff, "LOG_MONITOR %s NA_get_log_file.gdf  %s%cget_log_file 2 cm_get_log_file  -f %s/logs/TR%d/ns_logs/JMeter_%s_%d.log -t -s 10 -z %s/logs/TR%d/NetCloud/%s/JMeter_%s_%d.log -B 0 -y v2", server_name, server_name, global_settings->hierarchical_view_vector_separator, generator_entry[i].work, generator_entry[i].testidx, runprof_table_shr_mem[j].sess_ptr->sess_name, j,  getenv("NS_WDIR"), testidx, generator_entry[i].gen_name,  runprof_table_shr_mem[j].sess_ptr->sess_name, j);
  
          fprintf(fauto_mprof, "%s\n", buff);
          fflush(fauto_mprof);
            
          if(custom_monitor_config("CUSTOM_MONITOR", buff , NULL, 0, 1, err_msg, NULL, NULL, 0) < 0) 
           NSDL2_MON(NULL, NULL,"Error in applying LOG monitor monitor for Generator %s. Error: %s\n", generator_entry[i].gen_name, err_msg);
     
          if(switch_flag < sgrp_used_genrator_entries)
          {
            sprintf(buff, "LOG_MONITOR %s NA_get_log_file.gdf  %s%c%s_%d 2 cm_get_log_file  -f %s/logs/TR%d/TestRunOutput.log -t -s 10 -z %s/logs/TR%d/NetCloud/%s/TestRunOutput.log -B 0 -y v2", server_name, server_name, global_settings->hierarchical_view_vector_separator,generator_entry[i].gen_name, i,  generator_entry[i].work, generator_entry[i].testidx, getenv("NS_WDIR"), testidx, generator_entry[i].gen_name);
            fprintf(fauto_mprof, "%s\n", buff);
            fflush(fauto_mprof);
            switch_flag++;
     
            if(custom_monitor_config("CUSTOM_MONITOR", buff , NULL, 0, 1, err_msg, NULL, NULL, 0) < 0) 
              NSDL2_MON(NULL, NULL,"Error in applying LOG monitor monitor for Generator %s. Error: %s\n", generator_entry[i].gen_name, err_msg);
          }
        }
      }
      else if (loader_opcode == STAND_ALONE)
      {
         sprintf(server_name,"Cavisson%c%s",global_settings->hierarchical_view_vector_separator, g_machine);
         sprintf(buff, "STANDARD_MONITOR %s %s%cJMeter_%s_%d JavaGCJMXSun8 -f %s/logs/TR%d/.pidfiles/.JMeter_%s_%d.pid", server_name, server_name, global_settings->hierarchical_view_vector_separator,runprof_table_shr_mem[j].sess_ptr->sess_name, j, getenv("NS_WDIR"), testidx, runprof_table_shr_mem[j].sess_ptr->sess_name, j);
        
        fprintf(fauto_mprof, "%s\n", buff);
        fflush(fauto_mprof);
        if(kw_set_standard_monitor("STANDARD_MONITOR",buff, 1, NULL, err_msg, NULL) < 0)
          NSDL2_MON(NULL, NULL,"Error in applying JavaGC monitor for %s. Error: %s\n", server_name, err_msg);
      }
    }
  }   
  fclose(fauto_mprof);
  return 0;
}


                                                                                                               


//code for adding java process data monitor for each generator..  
 
int apply_java_process_monitor_for_generators()
{
  int i=0;
  char err_msg[1024] = "";
  char auto_mprof[MAX_NAME_LEN];
  FILE *fauto_mprof;
  char auto_mon_cmd[2*MAX_NAME_LEN];
  char copy_buffer[2*MAX_NAME_LEN];
  char *tmp_ptr;
  char de_li='/';
  auto_mon_cmd[0] = 0, auto_mprof[0] = 0;

  sprintf(auto_mprof, "%s/logs/TR%d/common_files/ns_files/ns_auto.mprof", g_ns_wdir, testidx);
  if ((fauto_mprof = fopen(auto_mprof, "a+")) == NULL)
  {
    fprintf(stderr, "error in creating auto mprof file\n");
    perror("fopen");
    return MON_FAILURE;
  }
  fprintf(fauto_mprof, "############# Java Process Monitor for Generators ################\n");
  for (i = 0; i < sgrp_used_genrator_entries; i++) 
  {
   
    tmp_ptr = strrchr((generator_entry[i].work), de_li);  //this contains the absolute path of a controller. we only need controller name.
    sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s ProcessDataExV3 -T TR%d -C %s -v 10000",
                 generator_entry[i].IP, generator_entry[i].gen_name, generator_entry[i].testidx, tmp_ptr+1);
    strcpy(copy_buffer,auto_mon_cmd); 
    if(kw_set_standard_monitor("STANDARD_MONITOR",copy_buffer, 1, NULL, err_msg, NULL) < 0)
    {
      NSDL2_MON(NULL, NULL,"Error in applying ProcessDataExV2 monitor for NVM_%d. Error: %s", (i + 1), err_msg);
    }

    NSDL2_MON(NULL, NULL, "Java stdmon entry %s",auto_mon_cmd); 

    fprintf(fauto_mprof, "%s\n", auto_mon_cmd);
  }
  fflush(fauto_mprof);
  fclose(fauto_mprof);
  return 0; 
} 
 //	:: end  apply ps data for gen


/***************************** CMON SETTINGS ********************************************/

/* This is to parse CMON_SETTINGS keyword and fill cmon_settings buffer in servers_list structure
 * in order to send these settings to server while sending control messages(test_run_starts and test_run_running).
 * Calling this from ns_parse_scen_conf.c to parse keyword CMON_SETTINGS.
 * Format:
 * CMON_SETTINGS  <Server>  HB_MISS_COUNT=6;CAVMON_MON_TMP_DIR=/tmp;CAVMON_DEBUG=on;CAVMON_DEBUG_LEVEL=1 
 * buf: Input buffer
 * run_time_flag: 0 - Normal parsing
 *                1-  Runtime parsing */

//void kw_set_cmon_settings(char *keyword, char *buf)
int kw_set_cmon_settings(char *buf, char *err_msg, int run_time_flag)
{
  char key[MAX_NAME_LEN];
  char server_name[MAX_NAME_LEN];
  char cmon_settings_buffer[MAX_NAME_LEN];
  char ip_port[2*MAX_NAME_LEN];
  int num = 0, i= 0;
  int j=0;

  //int total_no_of_servers = 0;

  NSDL2_SCHEDULE(NULL, NULL, "Method called, buf = %s, err_msg = %s, run_time_flag = %d", buf, err_msg, run_time_flag);

  key[0] = 0, server_name[0] = 0;
 
  num = sscanf(buf, "%s %s %s", key, server_name, cmon_settings_buffer);

  // Validation
  if(num != 3) // All fields mandatory.
  {
    sprintf(err_msg, "Error: Too few/more arguments for %s keywords.", key);
    strcat(err_msg,  "Usage: CMON_SETTINGS <Server> HB_MISS_COUNT=6;CAVMON_MON_TMP_DIR=/tmp;CAVMON_DEBUG=on;");
    strcat(err_msg,  "CAVMON_DEBUG_LEVEL=1\n"); 
    //fprintf(stderr,"%s", err_msg); 
    /*fprintf(stderr, "Error: Too few/more arguments for %s keywords.\n"
                    "Usage: CMON_SETTINGS <Server> HB_MISS_COUNT=6;CAVMON_MON_TMP_DIR=/tmp;CAVMON_DEBUG=on;"
                    "CAVMON_DEBUG_LEVEL=1\n",key);*/
    if(!run_time_flag)
    {
      NS_EXIT(-1,CAV_ERR_1060041);
    }
    else
    {
      NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_MAJOR,
                   __FILE__, (char*)__FUNCTION__,
                   "%s", err_msg);
      return -1;
    }
  }
  NSDL2_SCHEDULE(NULL, NULL, "num = [%d], key = [%s], server_name = [%s], cmon_settings_buffer = [%s]", num, key, server_name, cmon_settings_buffer);

  //if first time -> malloc and save
  //else free existing and malloc and save (considering latest cmon settings, ignoring previous)
  if(!strcasecmp(server_name, "ALL"))
  {
    NSDL2_SCHEDULE(NULL, NULL, "ALL CASE");
    //save cmon settings in global buffer 
    if(cmon_settings == NULL)
    {
      MY_MALLOC_AND_MEMSET(cmon_settings, strlen(cmon_settings_buffer) + 1, "cmon settings", -1);
      strcpy(cmon_settings, cmon_settings_buffer);
    }
    else
    {
      NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_MAJOR,
                   __FILE__, (char*)__FUNCTION__,
                   "Received duplicate cmon settings for server %s."
                   "Ignoring previous settings considering %s settings",
                   server_name, cmon_settings_buffer);

      FREE_AND_MAKE_NULL(cmon_settings, "cmon settings", -1);
      MY_MALLOC_AND_MEMSET(cmon_settings, strlen(cmon_settings_buffer) + 1, "cmon settings", -1);
      strcpy(cmon_settings, cmon_settings_buffer);
    }
    NSDL2_SCHEDULE(NULL, NULL, "cmon_settings = [%s]", cmon_settings);
  }
  else //for specefic server
  {
    NSDL2_SCHEDULE(NULL, NULL, "SPECIFIC SERVER CASE");
    //for(i=0; i < total_no_of_servers; i++)
    for(i=0;i<topo_info[topo_idx].total_tier_count;i++)
    {
      for(j=0;j<topo_info[topo_idx].topo_tier_info[i].total_server;j++)
      {
        if((topo_info[topo_idx].topo_tier_info[i].topo_server[j].used_row != -1) && (!topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->server_flag & DUMMY))
        {
          ip_port[0] = '\0'; //reset
          NSDL2_SCHEDULE(NULL, NULL, "i = %d,topo_tier_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->cmon_monitor_count = %d,topo_info[topo_idx].topo_tier_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->server_ip = %s"
               ,i,topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->cmon_monitor_count, 
                topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->server_ip);
      
          if(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->cmon_monitor_count) 
          {
            // In case we are using ip only (assuming default cavmon agent port 7891 will be applied internally),
            // but in server.dat we have defined like this ip:7891
            // to handle this, creating buffer ip_port
            sprintf(ip_port, "%s:7891", server_name);
            NSDL2_SCHEDULE(NULL, NULL, "ip_port = %s, server_name = %s,topo_info[topo_idx].topo_tier_info[%d].topo_server[%d].server_ptr->topo_servers_list->server_ip = %s",
            ip_port, server_name, i,j, topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->server_ip);

           //if server found in servers_list structure then save its cmon_settings_buffer in servers_list structure                     else do not do anything
           //if(strcmp(server_name, servers_list[i].server_ip) == 0)   
            if(!strcmp(server_name, topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->server_ip) || 
                !strcmp(ip_port, topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->server_ip))   
            {
              if(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->cmon_settings == NULL)
              {
                MY_MALLOC_AND_MEMSET(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->cmon_settings, strlen(cmon_settings_buffer) + 1, "cmon settings", -1);
               strcpy(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->cmon_settings, cmon_settings_buffer);
              }
              else
              {
                NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_MAJOR,
                          __FILE__, (char*)__FUNCTION__,
                         "Received duplicate cmon settings for server %s."
                         "Ignoring previous settings considering %s settings",
                         server_name, cmon_settings_buffer);

                FREE_AND_MAKE_NULL(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->cmon_settings, "server list cmon settings", -1);
                MY_MALLOC_AND_MEMSET(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->cmon_settings, strlen(cmon_settings_buffer) + 1, "cmon settings", -1);
                strcpy(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->cmon_settings, cmon_settings_buffer);
              }
              NSDL2_SCHEDULE(NULL, NULL, 
                    "cmon_settings_buffer = [%s],topo_info[topo_idx].topo_tier_info[%d].topo_server[%d]->server_ptr->topo_servers_list->cmon_settings = [%s]", 
              cmon_settings_buffer, i,j, topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->cmon_settings);
         
              return 0; 
            }
          }
        }
      }
    }
    if(!run_time_flag)
    {
      NS_EXIT(-1, CAV_ERR_1060042, server_name);
    }
    else
    {
      fprintf(stderr, "Error: No monitor is running on server %s\n", server_name);
      return -1;
    }
  }
  NSDL2_SCHEDULE(NULL, NULL, "End of CMON_SETTINGS parsing.");
  return 0;
}


/***** HIERARCHICAL_VIEW keyword parsing starts ****/

// HIERARCHICAL_VIEW <0/1> <ConfigurationFileName> <VectorSeparator>
int kw_enable_hierarchical_view(char *keyword, char *buffer)
{
  char key[MAX_NAME_LEN] = {0}, topology_name[MAX_NAME_LEN + 1] = {0};
  char vector_separator = '\0';
  char err_msg[MAX_AUTO_MON_BUF_SIZE];
  char owner_name[128];
  char group_name[128];
  int hierarchical_mode;

  if(sscanf(buffer, "%s %d %s %c", key, &hierarchical_mode, topology_name, &vector_separator) < 2)
    NS_KW_PARSING_ERR(keyword, 0, err_msg, HIERARCHICAL_VIEW_USAGE, CAV_ERR_1011359, keyword);

  global_settings->hierarchical_view = hierarchical_mode;

  if(hierarchical_mode != 1)  //Hierarchical mode is off
    NS_KW_PARSING_ERR(keyword, 0, err_msg, HIERARCHICAL_VIEW_USAGE, CAV_ERR_1060043, keyword);

  if('\0' == vector_separator)     //vector_separator has not been passed
    vector_separator = HIERARCHICAL_VIEW_VECTOR_SEPARATOR_ON;

  global_settings->hierarchical_view_vector_separator = vector_separator;
  /* Topology name priorty order is: (1) Scenario.conf (2) site.env 
   * By default topology name is NA in KeywordDefinition.dat -> in this case take topology name from site.env 
   * */
  if('\0' == topology_name[0])
  {
    if (getenv("DEFAULT_TOPOLOGY") != NULL)
      strcpy(global_settings->hierarchical_view_topology_name, getenv("DEFAULT_TOPOLOGY"));
    else
      strcpy(global_settings->hierarchical_view_topology_name, "NA");
  }
  else
    strcpy(global_settings->hierarchical_view_topology_name, topology_name);

  NSDL2_SCHEDULE(NULL, NULL, "Hierarchical view mode set to [%d], conf file name is [%s], vector_separator is [%c].",                                                        global_settings->hierarchical_view, global_settings->hierarchical_view_topology_name, 
                              global_settings->hierarchical_view_vector_separator);
   //Moving read_topology here from parse files as NetDiagnostic keyword has to add server from topology. Earlier netdiagnostic server was not added in the topology file but in the servers_list structures.  
   topo_idx = topolib_read_topology_and_init_method(getenv("NS_WDIR"), global_settings->hierarchical_view_topology_name, global_settings->hierarchical_view_vector_separator, err_msg,loader_opcode, testidx, nslb_get_owner(owner_name), nslb_get_group(group_name), g_partition_idx, "ns_logs/topo_debug.log", debug_log_value, max_trace_level_file_size, topo_idx);
  if(err_msg[0] != '\0')
  {
    NS_EXIT(-1, "Topology is incorrect, please correct topology and re-run the test, error: %s", err_msg);
  }
 
  return 0;
}

/***** HIERARCHICAL_VIEW keyword parsing ends ****/


/********************SETUP AUTO SERVER SIGNATURE********************************************/
//ENABLE_AUTO_SERVER_SIG <0/1>     (default value is 1)
/******************************************************************************************/
int kw_set_enable_auto_server_sig(char *keyword, char *buffer)
{
  char key[MAX_NAME_LEN];
  char input_server_ip[MAX_NAME_LEN] = {0};
  char temp_server_name[MAX_NAME_LEN] = {0};
  char display_server_name[2*MAX_NAME_LEN + 1] = {0};
  char tiername[MAX_NAME_LEN] = {0};
  char vector_prefix[4*MAX_NAME_LEN + 1] = {0};
  char *input_tmp_arr[2];
  char state[MAX_NAME_LEN];
  int enable_auto_server_sig = 0;   //0 - desable ,   1 - enable
  int num = 0, i,j;
  //ServerInfo *servers_list = NULL;
  //int total_no_of_servers = 0;
  int machine_type;
  char *colon_ptr = NULL;
  char err_msg[MAX_AUTO_MON_BUF_SIZE];

  NSDL2_SCHEDULE(NULL, NULL, "Method called, keyword = %s, buffer = %s", keyword, buffer);

  num = sscanf(buffer, "%s %s", key, state);

  // Validation
  if(num != 2) // All fields are mandatory.
  {
    NS_KW_PARSING_ERR(keyword, 0, err_msg, ENABLE_AUTO_SERVER_SIGNATURE_USAGE, CAV_ERR_1011359, keyword);
  }

  enable_auto_server_sig = atoi(state);

  NSDL2_SCHEDULE(NULL, NULL, "enable_cmon_agent = %d", enable_auto_server_sig);

  if(enable_auto_server_sig != 0 && enable_auto_server_sig != 1)
  {
    NS_KW_PARSING_ERR(keyword, 0, err_msg, ENABLE_AUTO_SERVER_SIGNATURE_USAGE, CAV_ERR_1060030, keyword); 
  }

  g_auto_server_sig_flag = enable_auto_server_sig;

  if(g_auto_server_sig_flag == INACTIVATE)
  {
    NSDL2_SCHEDULE(NULL, NULL, "Auto server signature monitors are inactivate so returning.");
    return 0;
  }


  //both ENABLE_CMON_AGENT and ENABLE_AUTO_SERVER_SIG use same server list (i.e. g_cmon_agent_server_table ) to set monitors at different server
  //Therefore if ENABLE_CMON_AGENT is 0 then we have to fill g_cmon_agent_server_table
  if(g_cmon_agent_flag == INACTIVATE)
  {
    //servers_list = (ServerInfo *) topolib_get_server_info();
    //Total no. of servers in ServerInfo structure
    //total_no_of_servers = topolib_get_total_no_of_servers();

    for(i=0; i < topo_info[topo_idx].total_tier_count; i++)
    {
      for(j=0;j<topo_info[topo_idx].topo_tier_info[i].total_server;j++)
      {
        if(topo_info[topo_idx].topo_tier_info[i].topo_server[j].used_row != -1)
        {
          NSDL2_SCHEDULE(NULL, NULL, "topo_info[topo_idx].topo_tier_info[%d].topo_server[%d].server_ptr->topo_servers_list->server_ip = %s, [%d].cav_mon_home = %s",
             i, j,  topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->server_ip, j, topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->cav_mon_home);

        //In case of lps,nde we make server entry directly in structure (not from file). In that case intall_dir will be NA, 
        //so ignoring these entries
          if(!strcasecmp(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->cav_mon_home, "NA"))
            continue;

          strcpy(input_server_ip, topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->server_ip);

          if(strstr(input_server_ip, ":"))
          {
            get_tokens(input_server_ip, input_tmp_arr, ":", 2);
            if(input_tmp_arr[0])
              strcpy(input_server_ip, input_tmp_arr[0]);
          }

          if(!topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->server_flag & DUMMY) 
          {  
            if(!((topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->auto_scale & 0X02) || (!strcmp(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->server_ip, "127.0.0.1"))))
              continue;
            topolib_get_server_display_and_tier_for_auto_mon(j, tiername, temp_server_name,i,topo_idx);
          }
   
  

         else
           continue;
   
          snprintf(vector_prefix, 2*MAX_NAME_LEN, "%s%c%s", tiername, global_settings->hierarchical_view_vector_separator, temp_server_name);
          sprintf(display_server_name, "%s>%s", tiername, temp_server_name);

          //reset ip
          memset(input_server_ip, 0, MAX_NAME_LEN);

          //we cannot have colon in signature name because check monitor uses colon as token
          if((colon_ptr = strchr(display_server_name, ':')))
            *colon_ptr = '_';

          if( !strcasecmp(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->machine_type,"Linux") ||
                                    !strcasecmp(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->machine_type,"LinuxEx") )
            machine_type = LINUX;
          else if(!strncasecmp(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->machine_type,"AIX",3))
            machine_type = AIX;
          else if( !strcasecmp(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->machine_type,"Solaris") 
                                  || !strcasecmp(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->machine_type,"SunOS") )
            machine_type = SOLARIS;
          else if(!strncasecmp(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->machine_type,"HPUX",4))
            machine_type = HPUX_MACHINE;
          else if(!strcasecmp(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->machine_type,"Windows"))
            machine_type = WIN_MACHINE; 
          else
            machine_type = OTHERS;

          if(add_cmon_agent_server(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->server_ip, vector_prefix,              topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->cav_mon_home, machine_type, display_server_name) == MON_FAILURE)
          {
            NS_EXIT(-1, CAV_ERR_1060031, topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->server_ip);
          }  
        }
      } 
    }
  } 
  return 0;
}
