/******************************************************************
 * Name    : ns_dynamic_vector_monitor.c 
 * Author  : Archana
 * Purpose : This file contains methods related to parsing dynamic 
             vector monitor keyword, and method to get vector list from cmon
 * Note:
 * Modification History:
 * 04/11/09 - Initial Version
*****************************************************************/

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <string.h>
#include <ctype.h>
#include <curl/curl.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <regex.h>
#include <v1/topolib_structures.h>

#include "ns_get_log_file_monitor.h"
#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "nslb_util.h"
#include "nslb_sock.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"
#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "util.h"
#include "ns_msg_def.h"
#include "ns_log.h"
#include "tmr.h"
#include "timing.h"
#include "ns_custom_monitor.h"
#include "ns_dynamic_vector_monitor.h"
#include "ns_schedule_phases.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "ns_alloc.h"
#include "wait_forever.h"
#include "nslb_cav_conf.h"
#include "ns_server_admin_utils.h"
#include "ns_user_monitor.h"
#include "ns_batch_jobs.h" 
#include "ns_check_monitor.h"
#include "ns_event_log.h"
#include "ns_mon_log.h"
#include "ns_string.h"
#include "ns_event_id.h"
#include "init_cav.h"
#include "nslb_msg_com.h"
#include "ns_coherence_nid_table.h"
#include "ns_trace_level.h"
#include "ns_nv_tbl.h"
#include "ns_monitor_profiles.h"
#include "ns_monitor_2d_table.h" 
#include "ns_standard_monitors.h"
#include "ns_ndc_outbound.h"
#include "ns_ndc.h"
#include "ns_gdf.h"
#include "ns_runtime_changes_monitor.h"
#include "ns_exit.h"
#include "ns_appliance_health_monitor.h"
#include "ns_monitor_metric_priority.h"
#include "ns_custom_monitor_RDnew.h"
#include "ns_lps.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"
#include "ns_monitor_init.h"
#include "nslb_get_norm_obj_id.h"
// This is the max line length of vector list
// So if avg size of vector name is 20, then we can handle upto MAX_DYNAMIC_VECTORS vectors
#define DYNAMIC_VECTOR_MON_MAX_MSG_LENGTH    (20*MAX_DYNAMIC_VECTORS)

//function to set dvm of custom-gdf-mon
int total_nid_table_row_entries = 0;
int max_nid_table_row_entries = 0;

NormObjKey *specific_server_id_key;

int max_dest_tier_entries; //to store the the destination tier name entries in table
int total_dest_tier;  //this will store for total destination tier names

int g_dyn_cm_start_idx_runtime; // reset on every dynamic monitor runtime addition, start index of runtime dynamic vector monitor in runtime table

//static int max_dynamic_vector_mon_entries = 0;    // Max dynamic vector monitor enteries

//static int num_dynamic_vector_mon;                 //number of dynamic vector monitors

//static int dyn_mon_send_msg(DynamicVectorMonitorInfo *dynamic_vector_monitor_ptr);
int dyn_mon_make_nb_conn(DynamicVectorMonitorInfo *dynamic_vector_monitor_ptr, char *svr_ip, int svr_port);
int check_if_tier_exist(char *tier, int total_mp_enteries, JSON_MP *json_mp_ptr);

int dynamic_vector_monitor_retry_count    = 0;  //default retry count for dynamic vector monitors
int dynamic_vector_monitor_timeout = 15;   // Default timeout for dynamic vector monitors in seconds

//static int dvm_epoll_fd; //this used by dynamic vector monitors

char* failed_dynamic_vector_monitor;
char is_dyn_mon_added_on_runtime = 0;   //this is used to check wheather a dynamic vector monitor is added on runtime.


int hpd_port = 0; //saving hpd port here to avoid reading from file again and again 
int ret_value = 0;
int g_dyn_cm_start_idx;

extern void add_delete_kubernetes_monitor_on_process_diff(char *appname, char *tier_name, char *mon_name, int delete_flag);
extern void search_deleted_mon_in_server_list(char *server_ip, int input_port, int all_vector_deleted_flag,  int num_vector);
extern int java_process_server_sig_enabled;

//we have 8 type of errors macros defined in ns_dynamic_vector_monitor.h 
char *error_msgs[]={"Recieved failure from monitor", "Error in making connection to server", "Connection closed by other side", "Read error", "Send error", "Epoll error", "System error", "Monitor time out"};


//This is to find out whether vector list processing of all coherence cluster monitor done or not
int total_no_of_coherence_cluster_mon = 0;

int dyn_mon_send_msg_to_cmon(void *ptr);
int create_mapping_table_col_entries(int, int, int);
int create_mapping_table_row_entries();
int read_json_and_pass_file_content(char *file, char **buff);

int g_dvm_malloc_delta_entries = 10;

/* --- START: Method used during Parsing Time  ---*/
//This method used to parse DYNAMIC_VECTOR_TIMEOUT keyword.
int kw_set_dynamic_vector_timeout(char *keyword, char *buf, char *kw_buf, char *err_msg, int runtime_flag)
{
  NSDL2_MON(NULL, NULL, "Method called. Keyword = %s, Timeout (in second) = %s ", keyword, buf);
  dynamic_vector_monitor_timeout = atoi(buf);
  if (dynamic_vector_monitor_timeout <= 0)
  {
    NS_KW_PARSING_ERR(kw_buf, runtime_flag, err_msg, DYNAMIC_VECTOR_TIMEOUT_USAGE, CAV_ERR_1011155, CAV_ERR_MSG_9);
  }
  NSDL3_MON(NULL, NULL, "pre_test_check_timeout = %d sec", dynamic_vector_monitor_timeout);
  return 0;
}

//This method used to parse DYNAMIC_VECTOR_TIMEOUT keyword.
int kw_set_dynamic_vector_monitor_retry_count(char *keyword, char *buf, char *kw_buf, char *err_msg, int runtime_flag)
{
  {
    NSDL2_MON(NULL, NULL, "Method called. Keyword = %s, retry count = %s ", keyword, buf);
    dynamic_vector_monitor_retry_count = atoi(buf);
    if (dynamic_vector_monitor_retry_count < 0)
     {
      NS_KW_PARSING_ERR(kw_buf, runtime_flag, err_msg, DYNAMIC_VECTOR_MONITOR_RETRY_COUNT_USAGE, CAV_ERR_1011156, CAV_ERR_MSG_8);
     }
    NSDL3_MON(NULL, NULL, "dynamic_vector_monitor_retry_count = %d", dynamic_vector_monitor_retry_count);
  }
  return 0;
}

/*------------------------------------------------------------------------------------------------------------- 
 * Purpose   : This function will parse keyword DYNAMIC_CM_RT_TABLE_SIZE <MODE> <SIZE>
 *        
 * Input     : buf          : DYNAMIC_CM_RT_TABLE_SIZE <MODE> <SIZE>
 *                            MODE : 0/1
 *                            SIZE : 0-1000
 *      
 * Build_ver : 4.1.9#
 *------------------------------------------------------------------------------------------------------------*/

void kw_set_dynamic_cm_rt_table_settings(char *buf)
{
  int num_args = 0; 
  char mode[MAX_STRING_LENGTH] = "";
  char keyword[MAX_STRING_LENGTH] = "";
  char size[MAX_STRING_LENGTH] = "";
  char tmp_str[MAX_STRING_LENGTH] = "";
  char usages[2*MAX_STRING_LENGTH] = "";
  char imode;
  int isize = 0;

  NSDL3_MON(NULL, NULL, "Method called, buf = %s", buf);

  sprintf(usages, "Usages:\n"
                  "Syntax - DYNAMIC_CM_RT_TABLE_SIZE <MODE> <SIZE> \n"
                  "Where:\n"
                  "MODE          - 0 Disable (Default) \n"
                  "              - 1 Enable Hash Table Searching\n"
                  "              - 2 Enable Hash table Searching as well as Skip Search for duplicate vectors in cm_info_ptr\n"
                  "SIZE          - Must be greater than 0. \n");

  num_args = sscanf(buf, "%s %s %s %s", keyword, mode, size, tmp_str);

  NSDL3_MON(NULL, NULL, "num_args = %d, keyword = %s, mode = %s, size = %s, tmp_str = %s", num_args, keyword, mode, size, tmp_str);

  if(num_args > 3)
  { 
    NSTL1_OUT(NULL, NULL, "Error: provided number of argument (%d) is wrong. Setting default value.%s\n", num_args, usages);
    imode = 0;
    isize = 10000;
  }

  NSDL3_MON(NULL, NULL, "mode = %s, size = %s, tmp_str = %s", mode, size, tmp_str);
 
  imode = (char)atoi(mode);
  isize = atoi(size);

  if(imode < 0 && imode > 2)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Error: Mode = %d, Invalid mode is provided. Setting mode for keyword (%s) to 0 by default.\n", imode, keyword);
    imode = 0;
  }

  if(isize < 0)
  {
    NSTL1_OUT(NULL, NULL, "Error: Size = %d, Invalid size is provided for dynamic_cm_hash_table. Setting size for keyword (%s) to 10000\n", isize, keyword);
    isize = 10000;
  }

  global_settings->dynamic_cm_rt_table_mode = imode;
  global_settings->dynamic_cm_rt_table_size = isize;

  NSDL3_MON(NULL, NULL, "Method ends, Keyword (%s) parsed. mode = %d, size = %d", keyword,
                         global_settings->dynamic_cm_rt_table_mode, global_settings->dynamic_cm_rt_table_size);
  MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Method ends, Keyword (%s) parsed. mode = %d, size = %d", keyword,
                         global_settings->dynamic_cm_rt_table_mode, global_settings->dynamic_cm_rt_table_size); 
}

//#ifdef NS_DEBUG_ON
char *dvm_to_str(DynamicVectorMonitorInfo *dynamic_vector_monitor_ptr)
{
  static char dvm_buf[MAX_MONITOR_BUFFER_SIZE];
  sprintf(dvm_buf, "cs_ip = %s, cs_port = %d, Program name to get vector list = %s, Program args to get vector list(with args) = %s, ProgramName (with args) = %s %s, gdf = %s, fd = %d", dynamic_vector_monitor_ptr->cs_ip, dynamic_vector_monitor_ptr->cs_port, dynamic_vector_monitor_ptr->pgm_name_for_vectors, dynamic_vector_monitor_ptr->pgm_args_for_vectors, dynamic_vector_monitor_ptr->pgm_path, dynamic_vector_monitor_ptr->pgm_args, dynamic_vector_monitor_ptr->gdf_name, dynamic_vector_monitor_ptr->fd);
  return(dvm_buf);
}
//#endif

/*static void free_dynamic_vector_monitor_particular_indx(int i)
{
  DynamicVectorMonitorInfo *dynamic_vector_monitor_ptr = dynamic_vector_monitor_info_ptr; 
  NSDL2_MON(NULL, NULL, "dynamic_vector_monitor_ptr %p, i %d", dynamic_vector_monitor_ptr, i);

  FREE_AND_MAKE_NULL(dynamic_vector_monitor_ptr[i].monitor_name, "dynamic_vector_monitor_ptr[i].monitor_name", -1);
  FREE_AND_MAKE_NULL(dynamic_vector_monitor_ptr[i].cs_ip, "dynamic_vector_monitor_ptr[i].cs_ip", -1);
  FREE_AND_MAKE_NULL(dynamic_vector_monitor_ptr[i].cavmon_ip, "dynamic_vector_monitor_ptr[i].cavmon_ip", -1);
  FREE_AND_MAKE_NULL(dynamic_vector_monitor_ptr[i].gdf_name, "dynamic_vector_monitor_ptr[i].gdf_name", -1);
  FREE_AND_MAKE_NULL(dynamic_vector_monitor_ptr[i].vector_list, "dynamic_vector_monitor_ptr[i].vector_list", -1);
  FREE_AND_MAKE_NULL(dynamic_vector_monitor_ptr[i].pgm_path, "dynamic_vector_monitor_ptr[i].pgm_path", -1);
  FREE_AND_MAKE_NULL(dynamic_vector_monitor_ptr[i].pgm_args, "dynamic_vector_monitor_ptr[i].pgm_args", -1);
  FREE_AND_MAKE_NULL(dynamic_vector_monitor_ptr[i].pgm_name_for_vectors, "dynamic_vector_monitor_ptr[i].pgm_name_for_vectors", -1);
  FREE_AND_MAKE_NULL(dynamic_vector_monitor_ptr[i].pgm_args_for_vectors, "dynamic_vector_monitor_ptr[i].pgm_args_for_vectors", -1);
  FREE_AND_MAKE_NULL(dynamic_vector_monitor_ptr[i].rem_ip, "dynamic_vector_monitor_ptr[i].rem_ip", -1);
  FREE_AND_MAKE_NULL(dynamic_vector_monitor_ptr[i].rem_username, "dynamic_vector_monitor_ptr[i].rem_username", -1);
  FREE_AND_MAKE_NULL(dynamic_vector_monitor_ptr[i].rem_password, "dynamic_vector_monitor_ptr[i].rem_password", -1);
  FREE_AND_MAKE_NULL(dynamic_vector_monitor_ptr[i].origin_cmon, "dynamic_vector_monitor_ptr[i].origin_cmon", -1);
  FREE_AND_MAKE_NULL(dynamic_vector_monitor_ptr[i].init_vector_list, "dynamic_vector_monitor_ptr[i].init_vector_list", -1);
  FREE_AND_MAKE_NULL(dynamic_vector_monitor_ptr[i].appname_pattern, "dynamic_vector_monitor_ptr[i].appname_pattern", -1);
  FREE_AND_MAKE_NULL(dynamic_vector_monitor_ptr[i].tier_name, "dynamic_vector_monitor_ptr[i].tier_name", -1);
  FREE_AND_MAKE_NULL(dynamic_vector_monitor_ptr[i].server_name, "dynamic_vector_monitor_ptr[i].server_name", -1);
  FREE_AND_MAKE_NULL(dynamic_vector_monitor_ptr[i].pod_name, "dynamic_vector_monitor_ptr[i].pod_name", -1);
}*/

static int dynamic_vector_monitor_usage(char *error, int runtime_flag, char *err_msg)
{
  NSDL2_MON(NULL, NULL, "Method called");
  if(!runtime_flag)
  {
    NS_KW_PARSING_ERR("DYNAMIC_VECTOR_MONITOR", runtime_flag, err_msg, DYNAMIC_VECTOR_MONITOR_USAGE, CAV_ERR_1060013);  
    return 1;
  }
  else
    return -1;
}


int create_table_entry_dvm(int *row_num, int *total, int *max, char **ptr, int size, char *name)
{
  MLTL3(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "row_num = %d, total = %d, max = %d, ptr = %p, size = %d, name = %s\n",
                        *row_num, *total, *max, *ptr, size, name);
  if (*total == *max)
  {
    MY_REALLOC_AND_MEMSET(*ptr, (*max + g_dvm_malloc_delta_entries) * size, (*max) * size, name, -1);
    *max += g_dvm_malloc_delta_entries;
  }
  *row_num = (*total)++;
   MLTL3(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "row_num = %d, total = %d, max = %d, ptr = %p, size = %d, name = %s\n",
                        *row_num, *total, *max, *ptr, size, name);

   return 0;
}


void set_coherence_cache_format_type()
{
  int i = 0, j = 0, num_cluster = 0;
  CM_info *cm_ptr = NULL;
  char *cluster_server_array[500];

  for (i = 0; i < total_monitor_list_entries; i++)
  {
    cm_ptr = monitor_list_ptr[i].cm_info_mon_conn_ptr;
    if(cm_ptr->flags & NEW_FORMAT) //coherence cluster monitor
    {
      cluster_server_array[num_cluster] = cm_ptr->cs_ip;
      num_cluster++;
    }
  }
  for (i = 0; i < total_monitor_list_entries; i++)
  {
    cm_ptr = monitor_list_ptr[i].cm_info_mon_conn_ptr;
    if(strstr(cm_ptr->gdf_name, "cm_coherence_cache") != NULL) //found
    {
      for(j = 0; j < num_cluster; j++)
      {
        if(strcmp(cm_ptr->cs_ip, cluster_server_array[j]) == 0) //match
          cm_ptr->flags |= NEW_FORMAT;
      }
    }
  }              
}

int kw_set_enable_alert_rule_monitor(char *keyword, char *buf, char *err_msg)
{         
  char key[DYNAMIC_VECTOR_MON_MAX_LEN]; 
  char SendBuffer[10 * DYNAMIC_VECTOR_MON_MAX_LEN]; 
  int value;
  char file_path[DYNAMIC_VECTOR_MON_MAX_LEN];
  char temp[DYNAMIC_VECTOR_MON_MAX_LEN];
  int args;
        
  NSDL2_MON(NULL, NULL, "Method called, keyword = %s, buf = %s, global_settings->continuous_monitoring_mode = %d", 
                                                       keyword, buf, global_settings->continuous_monitoring_mode);
          
  args = sscanf(buf, "%s %d %s %s", key, &value, file_path, temp);
  
  if(args > 3 || args < 2)
  {
    NS_KW_PARSING_ERR(keyword, 0, err_msg, ENABLE_ALERT_LOG_MONITOR_USAGE, CAV_ERR_1011359, key);       
  }

  if(value > 2)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Wrong input (%d) provided for the keyword (%s). Expected options are: 0->Disable, 1->AutoMode (enable only if continuous mode), 2->Enable", value, key);
    return 0;
  }

  if(((value == 1) && (global_settings->continuous_monitoring_mode)) || (value == 2))
  {
    //DYNAMIC_VECTOR_MONITOR Alert>NS_Alert Alert>Cavisson>Rule1 cm_alert_log_stats.gdf 2 /apps/java/jdk1.7.0_71/bin/java -cp /opt/cavisson/monitors/bin:/opt/cavisson/monitors/lib/java-getopt-1.0.9.jar:/opt/cavisson/monitors/lib/CmonLib.jar cm_alert_log_stats -s "$#$" -I 2  -d '/home/cavisson/Controller_3/webapps/logs' -f 'activealert.log' -L data -D EOC /apps/java/jdk1.7.0_71/bin/java -cp /opt/cavisson/monitors/bin:/opt/cavisson/monitors/lib/java-getopt-1.0.9.jar:/opt/cavisson/monitors/lib/CmonLib.jar cm_alert_log_stats -s "$#$" -I 2 -d '/home/cavisson/Controller_3/webapps/logs' -f 'activealert.log' -D -L header
    sprintf(SendBuffer, "DYNAMIC_VECTOR_MONITOR 127.0.0.1 Alert>Cavisson>Rule1 cm_alert_log_stats.gdf 2 java -cp /opt/cavisson/monitors/bin:/opt/cavisson/monitors/lib/java-getopt-1.0.9.jar:/opt/cavisson/monitors/lib/CmonLib.jar cm_alert_log_stats -s \"$#$\" -d '%s/webapps/logs' -f 'activealert.log' -L data -D EOC java -cp /opt/cavisson/monitors/bin:/opt/cavisson/monitors/lib/java-getopt-1.0.9.jar:/opt/cavisson/monitors/lib/CmonLib.jar cm_alert_log_stats -s \"$#$\" -d '%s/webapps/logs' -f 'activealert.log' -D -L header", g_ns_wdir , g_ns_wdir);

    NSDL2_MON(NULL, NULL, "Adding %s", SendBuffer);

    kw_set_dynamic_vector_monitor("DYNAMIC_VECTOR_MONITOR", SendBuffer, NULL, 0, 0, NULL, err_msg, file_path, NULL, 0);
  }
  return 0;
}

//This method used to parse DYNAMIC_VECTOR_MONITOR keyword. 
//DYNAMIC_VECTOR_MONITOR <Server Name> <Dynamic Vector Monitor Name> <GDF Name> <Run Option> <Command or Program Name with arguments for getting data> EOC <Program Name with arguments for getting Vector List>
int kw_set_dynamic_vector_monitor(char *keyword, char *buf, char* server_name, int use_lps_flag, int runtime_flag, char *pod_name, char *err_msg, char *init_vectors_file, JSON_info *json_info_ptr, char skip_breadcrumb_creation)
{         
  int num = 0;
  int i = 0, j = 0;               //just for storing the number of tokens
  char key[DYNAMIC_VECTOR_MON_MAX_LEN] = "";
  char new_buf[MAX_MONITOR_BUFFER_SIZE] = "";  //buffer line before EOC
  char monitor_name[DYNAMIC_VECTOR_MON_MAX_LEN] = "";
  char origin_cmon_and_ip_port[DYNAMIC_VECTOR_MON_MAX_LEN] = "NO"; //default is NO
  char cs_ip[DYNAMIC_VECTOR_MON_MAX_LEN];
  char tier_name[DYNAMIC_VECTOR_MON_MAX_LEN];
  int cs_port = g_cmon_port;
  char *cs_ptr[2] ; // To split cs_ip and cs_port
  char origin_cmon[DYNAMIC_VECTOR_MON_MAX_LEN]; //For Heroku
  char gdf_name[DYNAMIC_VECTOR_MON_MAX_LEN] = "";
  int option = RUN_EVERY_TIME_OPTION;
  char pgm_path[DYNAMIC_VECTOR_MON_MAX_LEN] = "";  // Program name with or without path
  char pgm_args[MAX_CONF_LINE_LENGTH] = "";      // Program arguments if any
  char pgm_name_for_vectors[DYNAMIC_VECTOR_MON_MAX_LEN*4] = "";      // Program name for vectors if any
  char pgm_args_for_vectors[MAX_MONITOR_BUFFER_SIZE] = "";      // Program arguments for vectors if any
  char prg_to_get_vector_list[MAX_MONITOR_BUFFER_SIZE] = ""; // program to get vector list 
  char temp_buff[MAX_MONITOR_BUFFER_SIZE];
  char app_search_pattern[MAX_MONITOR_BUFFER_SIZE];
  //char rem_username[256] = "NA";

  char temp_buf[2*MAX_MONITOR_BUFFER_SIZE] = "";
  char *token_arr[2000];  // will contain the array of all arguments
  char *ptr_to_vector_arg = NULL;
  char *ptr;
  int kubernetes_NA_gdf_flag=0;
  //ServerCptr server_mon_ptr;
  char *hv_ptr;
  char cm_line[MAX_MONITOR_BUFFER_SIZE];
  tier_name[0] = 0;
  origin_cmon[0] = '\0';

  NSDL2_MON(NULL, NULL, "Method called, keyword = %s, buf = %s, server_name = %s", keyword, buf, server_name);

  // Last field temp_buf is used to detect is there are any args
  num = sscanf(buf, "%s %s %s %s %d %s %s", key, origin_cmon_and_ip_port, monitor_name, gdf_name, &option, pgm_path, temp_buf);
  NSDL2_MON(NULL, NULL, "Method called, keyword = %s, buf = %s", keyword, buf);
  // Validation
  if(num < 6) // All fields except arguments are mandatory.
  {
    sprintf(err_msg, "Monitor buffer: %s", buf);
    return(dynamic_vector_monitor_usage("Error: Too few arguments for Dynamic Vector Monitor\n", runtime_flag, err_msg));
  }

  if(option != RUN_EVERY_TIME_OPTION && option != RUN_ONLY_ONCE_OPTION)
  {
    sprintf(err_msg, "Monitor buffer: %s", buf);
    return(dynamic_vector_monitor_usage("Error: Wrong Option for Dynamic Vector Monitor, should be Run Periodically(1) or Never(2)\n", runtime_flag, err_msg));
  }

  if((ptr = strstr(buf, "EOC")) == NULL)
  {
    sprintf(err_msg, "Monitor buffer: %s", buf);
    return(dynamic_vector_monitor_usage("Error: Too few arguments for Dynamic Vector Monitor.\nSyntax Error: EOC missing in DYNAMIC_VECTOR_MONITOR keyword.\n", runtime_flag, err_msg));
  }

  //Need to check whether option for applying access log on kubernetes container is given or not.
  if (!(strcmp(gdf_name,"NA_KUBER")))
    kubernetes_NA_gdf_flag=1;
//TODO check json_info once



  //extract line before EOC 
  strncpy(new_buf, buf, (ptr - buf));

  //extract line after EOC 
  ptr += strlen("EOC ");  //to increment pointer to get vector-program name
  NSDL2_MON(NULL, NULL, "Program to get vector list from create server '%s'", ptr);
  if(strlen(ptr) == 0)
    return(dynamic_vector_monitor_usage("Error: Too few arguments for Dynamic Vector Monitor.\nMissing Program Name for getting Vector List.\n", runtime_flag, err_msg));

  CLEAR_WHITE_SPACE(ptr);
  strcpy(prg_to_get_vector_list, ptr);

  /*NOTE: For all the other monitors "pgm_name_for_vectors" will be prog name only eg: cm_mpstat
   *      and "pgm_args_for_vectors" will be prog arg only eg: -v NS_
   *
   *But in case of java based standard monitors for eg: "AppDynamics":
   *standard_monitor.dat entry:
   *AppDynamics|NA|2|java cm_app_dynamics_stats|java|NA|All|DVM|AppDynamics monitor 
   *while executing this as dynamic monitor, we get "java" in "pgm_name_for_vectors" 
   *and "'cm_app_dynamics_stats' with arguments" in "pgm_args_for_vectors"
   *but it does not creates any issue because CavMonAgent always execute prog name along with its arg hence not handling right now. 
   */

  //Extracting name and arguments from prg_to_get_vector_list
  ptr_to_vector_arg = strstr(prg_to_get_vector_list, " ");

  if(ptr_to_vector_arg != NULL) // it means we have both prog name and prog argument
  {
    //prog name to get vector list
    strncpy(pgm_name_for_vectors, prg_to_get_vector_list, ptr_to_vector_arg - prg_to_get_vector_list);
    //prog arguments to get vector list
    strcpy(temp_buff, ptr_to_vector_arg + 1);
    if(kubernetes_NA_gdf_flag && (strstr(temp_buff, " ")))
    {
      i=get_tokens(temp_buff, token_arr, " ", 500);
      for(j=0;j<i;j++)
      {
        if(strcmp(token_arr[j],"-appname"))
        {
          strcat(pgm_args_for_vectors,token_arr[j]);
          strcat(pgm_args_for_vectors," ");
        }
        else
        {
          j++;
          strcpy(app_search_pattern,token_arr[j]);
        }
      }
    }
    else
      strcpy(pgm_args_for_vectors,temp_buff);
  }
  else // it means we only have program name
  {
    strcpy(pgm_name_for_vectors, prg_to_get_vector_list);
  }

  //Here we need to get pgm_args only if user has given any pgm args
  if(!(g_tsdb_configuration_flag & TSDB_MODE) && (num > 6))
  {
    num--; // decrement num as it has first argument also (temp_buf)
    i = get_tokens(new_buf, token_arr, " ", 500); //get the total number of tokens
    for(j = num; j < i; j++)
    {
      if(!strcmp(token_arr[j], "$OPTION"))
      {
        sprintf(temp_buf, "%d", option);
        NSDL2_MON(NULL, NULL, "Replacing '$OPTION' by %s", temp_buf);
        strcat(pgm_args, temp_buf);
      }
      else if(!strcmp(token_arr[j], "$INTERVAL"))
      {
        sprintf(temp_buf, "%d", global_settings->progress_secs);
        NSDL2_MON(NULL, NULL, "Replacing '$INTERVAL' by %s", temp_buf);
        strcat(pgm_args, temp_buf);
      }
      else if(kubernetes_NA_gdf_flag && (strstr(token_arr[j], "-appname")))
      {
        j++;
        strcpy(app_search_pattern,token_arr[j]);
      }
      else
        strcat(pgm_args, token_arr[j]);

      strcat(pgm_args, " ");
    }
  }
  else if((g_tsdb_configuration_flag & TSDB_MODE) && (num > 6))
  {
    extract_prgms_args(new_buf,pgm_args, 5);
  }


  //if ENABLE_JAVA_PROCESS_SERVER_SIGNATURE kw is in auto mode
  if((java_process_server_sig_enabled == 2) && (strstr(gdf_name, "cm_coherence") != NULL))
    java_process_server_sig_enabled = 1;


  if((hv_ptr = strchr(origin_cmon_and_ip_port, global_settings->hierarchical_view_vector_separator)))
  {
    *hv_ptr = '\0';
    hv_ptr ++;
    strcpy(tier_name,origin_cmon_and_ip_port);
    memmove(origin_cmon_and_ip_port, hv_ptr, strlen(hv_ptr) + 1);

  }

  memset(cs_ptr, 0, sizeof(cs_ptr));  // To initize the ptr with NULL.
  i = get_tokens(origin_cmon_and_ip_port, cs_ptr, ":", 2);
  strcpy(cs_ip, cs_ptr[0]);
  if(cs_ptr[1])
    cs_port = atoi(cs_ptr[1]);

  if(origin_cmon[0] != '\0') //Heroku monitor
    sprintf(cm_line, "CUSTOM_MONITOR %s@%s:%d %s %s %d %s %s", origin_cmon, cs_ip, cs_port, gdf_name, monitor_name, option, pgm_path, pgm_args);
  else
  {
    if(tier_name[0] != '\0')
    {
      sprintf(cm_line, "CUSTOM_MONITOR %s%c%s:%d %s %s %d %s %s", tier_name, global_settings->hierarchical_view_vector_separator, cs_ip, cs_port, gdf_name, monitor_name, option, pgm_path, pgm_args);
    }
    else
    {
      sprintf(cm_line, "CUSTOM_MONITOR %s:%d %s %s %d %s %s", cs_ip, cs_port, gdf_name, monitor_name, option, pgm_path, pgm_args);
    }
  }

 
  int config_ret = custom_monitor_config("CUSTOM_MONITOR", cm_line, cs_ip, 1, runtime_flag, err_msg, pod_name, json_info_ptr,
    skip_breadcrumb_creation);

  if(config_ret < 0)
  {
    return -1;
    //TODO MSR: Error handling 
  }

  if(app_search_pattern[0] != '\0')
  {
    int no_of_appname_tokens, aj;
    char *appname_token_arr[MAX_NO_OF_APP];

    no_of_appname_tokens = get_tokens(app_search_pattern, appname_token_arr, ",", MAX_NO_OF_APP);

    CM_info *cm_ptr = monitor_list_ptr[total_monitor_list_entries - 1].cm_info_mon_conn_ptr;
    MY_MALLOC(cm_ptr->appname_pattern,((no_of_appname_tokens+1) * sizeof(char *)), "Allocating for appname pattern row", (total_monitor_list_entries - 1));

    for(aj=0;aj<no_of_appname_tokens;aj++)
    {
      MY_MALLOC(cm_ptr->appname_pattern[aj],strlen(appname_token_arr[aj])+1, "Allocating for appname pattern", aj);
      strcpy(cm_ptr->appname_pattern[aj], appname_token_arr[aj]);
    }
    cm_ptr->total_appname_pattern=no_of_appname_tokens;
  }

  if (is_outbound_connection_enabled &&
      (json_info_ptr && !(json_info_ptr->agent_type & CONNECT_TO_NDC)))
    g_mon_id = get_next_mon_id();

  return 0;
}

// ND_ENABLE_HOT_SPOT_MONITOR <0/1>
void kw_set_nd_enable_hot_spot_monitor(char *keyword, char *buf, char *err_msg)
{
  char key[DYNAMIC_VECTOR_MON_MAX_LEN] = "";
  char SendBuffer[DYNAMIC_VECTOR_MON_MAX_LEN];
  int value;

  NSDL2_MON(NULL, NULL, "Method called, keyword = %s, buf = %s, global_settings->net_diagnostics_mode=%d", keyword, buf, global_settings->net_diagnostics_mode);

  sscanf(buf, "%s %d", key, &value);

  if(!(global_settings->net_diagnostics_mode))
    return;

  // If ND HotSpot monitor is enabled, then we need to add dynamic vector monitor 
  // This monitor talks to ND Collector (LPS) to get vectors and data
  if(value)// Currently this value is not saved any where
  {
    sprintf(SendBuffer, "DYNAMIC_VECTOR_MONITOR %s:%d NDHotSpotThread cm_nd_thread_hot_spot_data.gdf 2 cm_nd_thread_hot_spot_data EOC cm_nd_get_app_instances", global_settings->net_diagnostics_server, global_settings->net_diagnostics_port);

    NSDL2_MON(NULL, NULL, "Adding %s", SendBuffer);

    kw_set_dynamic_vector_monitor("DYNAMIC_VECTOR_MONITOR", SendBuffer, NULL, 0, 0, NULL, err_msg, NULL, NULL, 0);   
  }
/*  else if(value)
  {
    NSTL1_OUT(NULL, NULL, "Warning: Ignoring scenario setting for Hotspot threads monitoring as it cannot be enabled without enabling NetDiagnostics for at least one scenario group.\n");
  }*/
}

void kw_set_nd_enable_nodejs_server_monitor(char *keyword, char *buf, char *err_msg)
{
  char key[DYNAMIC_VECTOR_MON_MAX_LEN] = "";
  char SendBuffer[DYNAMIC_VECTOR_MON_MAX_LEN];
  int value;
        
  NSDL2_MON(NULL, NULL, "Method called, keyword = %s, buf = %s, global_settings->net_diagnostics_mode=%d", keyword, buf, global_settings->net_diagnostics_mode);
        
  sscanf(buf, "%s %d", key, &value);
        
  if(!(global_settings->net_diagnostics_mode))
    return;
          
  // This monitor talks to ND Collector to get vectors and data
  if(value)// Currently this value is not saved any where
  {
    sprintf(SendBuffer, "DYNAMIC_VECTOR_MONITOR %s:%d NDNodejsServer cm_nd_nodejs_server_monitor.gdf 2 cm_nd_nodejs_server_data EOC cm_nd_nodejs_server_mon", global_settings->net_diagnostics_server, global_settings->net_diagnostics_port);

    NSDL2_MON(NULL, NULL, "Adding %s", SendBuffer);

    kw_set_dynamic_vector_monitor("DYNAMIC_VECTOR_MONITOR", SendBuffer, NULL, 0, 0, NULL, err_msg, NULL, NULL, 0);
  }
}


void kw_set_nd_enable_nodejs_async_event_monitor(char *keyword, char *buf, char *err_msg)
{
  char key[DYNAMIC_VECTOR_MON_MAX_LEN] = "";
  char SendBuffer[DYNAMIC_VECTOR_MON_MAX_LEN];
  int value;

  NSDL2_MON(NULL, NULL, "Method called, keyword = %s, buf = %s, global_settings->net_diagnostics_mode=%d", keyword, buf, global_settings->net_diagnostics_mode);

  sscanf(buf, "%s %d", key, &value);

  if(!(global_settings->net_diagnostics_mode))
    return;

  if(value)// Currently this value is not saved any where
  {
    sprintf(SendBuffer, "DYNAMIC_VECTOR_MONITOR %s:%d NDNodejsAsyncEvent cm_nd_nodejs_async_event.gdf 2 cm_nd_nodejs_async_event_data EOC cm_nd_nodejs_async_event_mon", global_settings->net_diagnostics_server, global_settings->net_diagnostics_port);

    NSDL2_MON(NULL, NULL, "Adding %s", SendBuffer);
  
    kw_set_dynamic_vector_monitor("DYNAMIC_VECTOR_MONITOR", SendBuffer, NULL, 0, 0, NULL, err_msg, NULL, NULL, 0);
  }
}

// ND_ENABLE_METHOD_MONITOR_EX <Enable/Disable>
int kw_set_nd_enable_method_monitor_ex(char *keyword, char *buf, char *err_msg)
{
  char key[DYNAMIC_VECTOR_MON_MAX_LEN]; //TODO: DISCUSS THIS BUFF SIZE
  char enable_method_mon[32];
  char SendBuffer[10 * DYNAMIC_VECTOR_MON_MAX_LEN];
  //int value;

  NSDL2_MON(NULL, NULL, "Method called, keyword = %s, buf = %s, global_settings->net_diagnostics_mode=%d", keyword, buf, global_settings->net_diagnostics_mode);

  if(sscanf(buf, "%s %s", key, enable_method_mon) != 2) 
  {
    NS_KW_PARSING_ERR(keyword, 0, err_msg, ND_ENABLE_METHOD_MONITOR_EX_USAGE, CAV_ERR_1011359, key);
  }

  if(!(global_settings->net_diagnostics_mode))
    return 0;

  // If ND Method monitor is enabled, then we need to add dynamic vector monitor 
  // This monitor talks to ND Collector to get vectors and data

  if(atoi(enable_method_mon) == 1)
  {
    sprintf(SendBuffer, "DYNAMIC_VECTOR_MONITOR %s:%d NDMethodMon cm_nd_method_stats.gdf 2 cm_nd_method_mon_data_ex EOC cm_nd_method_mon_ex", global_settings->net_diagnostics_server, global_settings->net_diagnostics_port);

    NSDL2_MON(NULL, NULL, "Adding %s", SendBuffer);

    kw_set_dynamic_vector_monitor("DYNAMIC_VECTOR_MONITOR", SendBuffer, NULL, 0, 0, NULL, err_msg, NULL, NULL, 0);   
  }
  return 0;
}
/* --- End: Method used during Parsing Time  ---*/

// ND_ENABLE_HTTP_HEADER_CAPTURE_MONITOR <Enable/Disable>
int kw_set_nd_enable_http_header_capture_monitor(char *keyword, char *buf, char *err_msg)
{
  char key[DYNAMIC_VECTOR_MON_MAX_LEN]; 
  char enable_http_header_capture_mon[32];
  char SendBuffer[10 * DYNAMIC_VECTOR_MON_MAX_LEN];

  NSDL2_MON(NULL, NULL, "Method called, keyword = %s, buf = %s, global_settings->net_diagnostics_mode=%d", keyword, buf, global_settings->net_diagnostics_mode);

  if(sscanf(buf, "%s %s", key, enable_http_header_capture_mon) != 2) 
  {
    NS_KW_PARSING_ERR(keyword, 0, err_msg, ND_ENABLE_HTTP_HEADER_CAPTURE_MONITOR_USAGE, CAV_ERR_1011359, key);
  }

  if(!(global_settings->net_diagnostics_mode))
    return 0;

  // If ND HTTP Capture Monitor is enabled, then we need to add dynamic vector monitor 
  // This monitor talks to ND Collector to get vectors and data

  if(atoi(enable_http_header_capture_mon) == 1)
  {
    sprintf(SendBuffer, "DYNAMIC_VECTOR_MONITOR %s:%d NDHTTPHeaderCapture cm_nd_http_header_stats.gdf 2 cm_nd_http_header_capture_data EOC cm_nd_http_header_capture_mon", global_settings->net_diagnostics_server, global_settings->net_diagnostics_port);

    NSDL2_MON(NULL, NULL, "Adding %s", SendBuffer);

    kw_set_dynamic_vector_monitor("DYNAMIC_VECTOR_MONITOR", SendBuffer, NULL, 0, 0, NULL, err_msg, NULL, NULL, 0);   
  }
  return 0;
}

// ND_ENABLE_EXCEPTIONS_MONITOR <Enable/Disable>
int kw_set_nd_enable_exceptions_monitor(char *keyword, char *buf, char *err_msg)
{
  char key[DYNAMIC_VECTOR_MON_MAX_LEN]; 
  char enable_exceptions_mon[32];
  char SendBuffer[10 * DYNAMIC_VECTOR_MON_MAX_LEN];

  NSDL2_MON(NULL, NULL, "Method called, keyword = %s, buf = %s, global_settings->net_diagnostics_mode=%d", keyword, buf, global_settings->net_diagnostics_mode);

  if(sscanf(buf, "%s %s", key, enable_exceptions_mon) != 2) 
  {
    NS_KW_PARSING_ERR(keyword, 0, err_msg, ND_ENABLE_EXCEPTIONS_MONITOR_USAGE, CAV_ERR_1011359, key);
  }

  if(!(global_settings->net_diagnostics_mode))
    return 0;

  // If ND EXCEPTIONS Monitor is enabled, then we need to add dynamic vector monitor 
  // This monitor talks to ND Collector to get vectors and data

  if(atoi(enable_exceptions_mon) == 1)
  {
    sprintf(SendBuffer, "DYNAMIC_VECTOR_MONITOR %s:%d NDExceptionsMon cm_nd_exception_stats.gdf 2 cm_nd_exceptions_mon_data EOC cm_nd_exceptions_mon", global_settings->net_diagnostics_server, global_settings->net_diagnostics_port);

    NSDL2_MON(NULL, NULL, "Adding %s", SendBuffer);

    kw_set_dynamic_vector_monitor("DYNAMIC_VECTOR_MONITOR", SendBuffer, NULL, 0, 0, NULL, err_msg, NULL, NULL, 0);   
  }
  return 0;
}

//ND_ENABLE_ENTRY_POINT_MONITOR <0/1>
int kw_set_nd_enable_entry_point_monitor(char *keyword, char *buf, char *err_msg)
{
  char key[DYNAMIC_VECTOR_MON_MAX_LEN]; 
  char SendBuffer[10 * DYNAMIC_VECTOR_MON_MAX_LEN];
  int value;

  NSDL2_MON(NULL, NULL, "Method called, keyword = %s, buf = %s, global_settings->net_diagnostics_mode=%d", 
                                                     keyword, buf, global_settings->net_diagnostics_mode);

  if(sscanf(buf, "%s %d", key, &value) != 2) 
  {
    NS_KW_PARSING_ERR(keyword, 0, err_msg, ND_ENABLE_ENTRY_POINT_MONITOR_USAGE, CAV_ERR_1011359, key);
  }

  if(!(global_settings->net_diagnostics_mode))
    return 0;

  // If ND Entry Point Monitor is enabled, then we need to add dynamic vector monitor 
  // This monitor talks to ND Collector to get vectors and data

  if(value == 1)
  {
    sprintf(SendBuffer, "DYNAMIC_VECTOR_MONITOR %s:%d NDEntryPointMon cm_nd_entry_point_stats.gdf 2 cm_nd_entry_point_mon_data EOC "
                        "cm_nd_entry_point_mon", global_settings->net_diagnostics_server, global_settings->net_diagnostics_port);

    NSDL2_MON(NULL, NULL, "Adding %s", SendBuffer);

    kw_set_dynamic_vector_monitor("DYNAMIC_VECTOR_MONITOR", SendBuffer, NULL, 0, 0, NULL, err_msg, NULL, NULL, 0);   
  }
  return 0;
}

//ND_ENABLE_BACKEND_CALL_MONITOR <0/1>
int kw_set_nd_enable_backend_call_monitor(char *keyword, char *buf, char *err_msg)
{
  char key[DYNAMIC_VECTOR_MON_MAX_LEN]; 
  char SendBuffer[10 * DYNAMIC_VECTOR_MON_MAX_LEN];
  int value;
  char file_path[DYNAMIC_VECTOR_MON_MAX_LEN];
  char temp[DYNAMIC_VECTOR_MON_MAX_LEN];
  int args;

  NSDL2_MON(NULL, NULL, "Method called, keyword = %s, buf = %s, global_settings->net_diagnostics_mode=%d", 
                                                     keyword, buf, global_settings->net_diagnostics_mode);

  args = sscanf(buf, "%s %d %s %s", key, &value, file_path, temp);
  
  if(args > 3 || args < 2)
  {
    NS_KW_PARSING_ERR(keyword, 0, err_msg, ND_ENABLE_BACKEND_CALL_MONITOR_USAGE, CAV_ERR_1011359, key);
  }

  if(!(global_settings->net_diagnostics_mode))
    return 0;

  // If ND DB Call Monitor is enabled, then we need to add dynamic vector monitor 
  // This monitor talks to ND Collector to get vectors and data
  
  if(value == 1)
  {

    sprintf(SendBuffer, "DYNAMIC_VECTOR_MONITOR %s:%d NDEnableBackendCallMon cm_nd_backend_call_stats.gdf 2 cm_nd_backend_call_mon_data EOC "
                       "cm_nd_backend_call_mon", global_settings->net_diagnostics_server, global_settings->net_diagnostics_port);

    NSDL2_MON(NULL, NULL, "Adding %s", SendBuffer);

    kw_set_dynamic_vector_monitor("DYNAMIC_VECTOR_MONITOR", SendBuffer, NULL, 0, 0, NULL, err_msg, file_path, NULL, 0);   
  }
  return 0;
}

//ND_ENABLE_NODE_GC_MONITOR <0/1>
int kw_set_nd_enable_node_gc_monitor(char *keyword, char *buf)
{
  char key[DYNAMIC_VECTOR_MON_MAX_LEN]; 
  char SendBuffer[10 * DYNAMIC_VECTOR_MON_MAX_LEN];
  int value;
  char file_path[DYNAMIC_VECTOR_MON_MAX_LEN];
  char temp[DYNAMIC_VECTOR_MON_MAX_LEN];
  char err_msg[MAX_AUTO_MON_BUF_SIZE];
  int args;

  NSDL2_MON(NULL, NULL, "Method called, keyword = %s, buf = %s, global_settings->net_diagnostics_mode=%d", 
                                                     keyword, buf, global_settings->net_diagnostics_mode);

  args = sscanf(buf, "%s %d %s %s", key, &value, file_path, temp);
  
  if(args > 3 || args < 2)
  {
    NS_KW_PARSING_ERR(keyword, 0, err_msg, ND_ENABLE_NODE_GC_MONITOR_USAGE, CAV_ERR_1011359, key);
  }

  if(!(global_settings->net_diagnostics_mode))
    return 0;

  if(value == 1)
  {
    sprintf(SendBuffer, "DYNAMIC_VECTOR_MONITOR %s:%d NDEnableNodeGCMon cm_nd_nodejs_gc.gdf 2 cm_nd_node_gc_data EOC "
                       "cm_nd_node_gc_mon", global_settings->net_diagnostics_server, global_settings->net_diagnostics_port);

    NSDL2_MON(NULL, NULL, "Adding %s", SendBuffer);

    kw_set_dynamic_vector_monitor("DYNAMIC_VECTOR_MONITOR", SendBuffer, NULL, 0, 0, NULL, NULL, file_path, NULL, 0);   
  }
  return 0;
}

//ND_ENABLE_EVENT_LOOP_MONITOR <0/1>
int kw_set_nd_enable_event_loop_monitor(char *keyword, char *buf)
{
  char key[DYNAMIC_VECTOR_MON_MAX_LEN]; 
  char SendBuffer[10 * DYNAMIC_VECTOR_MON_MAX_LEN];
  int value;
  char file_path[DYNAMIC_VECTOR_MON_MAX_LEN];
  char temp[DYNAMIC_VECTOR_MON_MAX_LEN];
  char err_msg[MAX_AUTO_MON_BUF_SIZE];
  int args;

  NSDL2_MON(NULL, NULL, "Method called, keyword = %s, buf = %s, global_settings->net_diagnostics_mode=%d", 
                                                     keyword, buf, global_settings->net_diagnostics_mode);

  args = sscanf(buf, "%s %d %s %s", key, &value, file_path, temp);
  
  if(args > 3 || args < 2)
  {
    NS_KW_PARSING_ERR(keyword, 0, err_msg, ND_ENABLE_EVENT_LOOP_MONITOR_USAGE, CAV_ERR_1011359, key);
  }

  if(!(global_settings->net_diagnostics_mode))
    return 0;

  if(value == 1)
  {
    sprintf(SendBuffer, "DYNAMIC_VECTOR_MONITOR %s:%d NDEnableEventLoopMon cm_nd_nodejs_event_loop.gdf 2 cm_nd_event_loop_data EOC "
                       "cm_nd_event_loop_mon", global_settings->net_diagnostics_server, global_settings->net_diagnostics_port);

    NSDL2_MON(NULL, NULL, "Adding %s", SendBuffer);

    kw_set_dynamic_vector_monitor("DYNAMIC_VECTOR_MONITOR", SendBuffer, NULL, 0, 0, NULL, NULL, file_path, NULL, 0);   
  }
  return 0;
}

//ND_ENABLE_FP_STATS_MONITOR_EX <0/1>
int kw_set_nd_enable_fp_stats_monitor_ex(char *keyword, char *buf, char *err_msg)
{
  char key[DYNAMIC_VECTOR_MON_MAX_LEN]; 
  char SendBuffer[10 * DYNAMIC_VECTOR_MON_MAX_LEN];
  int value;

  NSDL2_MON(NULL, NULL, "Method called, keyword = %s, buf = %s, global_settings->net_diagnostics_mode=%d", 
                                                     keyword, buf, global_settings->net_diagnostics_mode);

  if(sscanf(buf, "%s %d", key, &value) != 2) 
  {
    NS_KW_PARSING_ERR(keyword, 0, err_msg, ND_ENABLE_FP_STATS_MONITOR_EX_USAGE, CAV_ERR_1011359, key);
  }

  if(!(global_settings->net_diagnostics_mode))
    return 0;

  // If ND DB Call Monitor is enabled, then we need to add dynamic vector monitor 
  // This monitor talks to ND Collector to get vectors and data
  if(value == 1)
  {
    sprintf(SendBuffer, "DYNAMIC_VECTOR_MONITOR %s:%d NDEnableFPStatsMonEx cm_nd_bt.gdf 2 cm_nd_fp_stats_mon_ex_data EOC "
                        "cm_nd_fp_stats_mon_ex", global_settings->net_diagnostics_server, global_settings->net_diagnostics_port);

    NSDL2_MON(NULL, NULL, "Adding %s", SendBuffer);

    kw_set_dynamic_vector_monitor("DYNAMIC_VECTOR_MONITOR", SendBuffer, NULL, 0, 0, NULL, err_msg, NULL, NULL, 0);   
  }
  return 0;
}

//ND_ENABLE_BUSINESS_TRANS_STATS_MONITOR <0/1>
int kw_set_nd_enable_business_trans_stats_monitor(char *keyword, char *buf, char *err_msg)
{
  char key[DYNAMIC_VECTOR_MON_MAX_LEN]; 
  char SendBuffer[10 * DYNAMIC_VECTOR_MON_MAX_LEN];
  int value;

  NSDL2_MON(NULL, NULL, "Method called, keyword = %s, buf = %s, global_settings->net_diagnostics_mode=%d", 
                                                     keyword, buf, global_settings->net_diagnostics_mode);

  if(sscanf(buf, "%s %d", key, &value) != 2) 
  {
    NS_KW_PARSING_ERR(keyword, 0, err_msg, ND_ENABLE_BUSINESS_TRANS_STATS_MONITOR_USAGE, CAV_ERR_1011359, key);
  }

  if(!(global_settings->net_diagnostics_mode))
    return 0;

  // If ND DB Call Monitor is enabled, then we need to add dynamic vector monitor 
  // This monitor talks to ND Collector to get vectors and data
  if(value == 1)
  {
    sprintf(SendBuffer, "DYNAMIC_VECTOR_MONITOR %s:%d NDEnableBusinessTransStatsMon cm_nd_bt_ip.gdf 2 cm_nd_business_trans_stats_mon_data EOC "
                        "cm_nd_business_trans_stats_mon", global_settings->net_diagnostics_server, global_settings->net_diagnostics_port);

    NSDL2_MON(NULL, NULL, "Adding %s", SendBuffer);

    kw_set_dynamic_vector_monitor("DYNAMIC_VECTOR_MONITOR", SendBuffer, NULL, 0, 0, NULL, err_msg, NULL, NULL, 0);   
  }
  return 0;
}

int kw_set_nd_enable_cpu_by_thread_monitor(char *keyword, char *buf, char *err_msg)
{
  char key[DYNAMIC_VECTOR_MON_MAX_LEN]; 
  char SendBuffer[10 * DYNAMIC_VECTOR_MON_MAX_LEN];
  int value;

  NSDL2_MON(NULL, NULL, "Method called, keyword = %s, buf = %s, global_settings->net_diagnostics_mode=%d", 
                                                     keyword, buf, global_settings->net_diagnostics_mode);

  if(sscanf(buf, "%s %d", key, &value) != 2) 
  {
    NS_KW_PARSING_ERR(keyword, 0, err_msg, ND_ENABLE_JVM_THREAD_MONITOR_USAGE, CAV_ERR_1011359, key);
  }

  if(!(global_settings->net_diagnostics_mode))
    return 0;

  // If ND DB Call Monitor is enabled, then we need to add dynamic vector monitor 
  // This monitor talks to ND Collector to get vectors and data
  if(value == 1)
  {
    sprintf(SendBuffer, "DYNAMIC_VECTOR_MONITOR %s:%d ND_ENABLE_CPU_BY_THREAD cm_nd_jvm_thread_monitor.gdf 2 " 
                        "cm_nd_jvm_thread_monitor_mon_data EOC cm_nd_jvm_thread_monitor_mon", global_settings->net_diagnostics_server, 
                         global_settings->net_diagnostics_port);

    NSDL2_MON(NULL, NULL, "Adding %s", SendBuffer);

   kw_set_dynamic_vector_monitor("DYNAMIC_VECTOR_MONITOR", SendBuffer, NULL, 0, 0, NULL, err_msg, NULL, NULL, 0);   
  }
  return 0;
}

//ND_ENABLE_DB_CALL_MONITOR <0/1>
int kw_set_nd_enable_db_call_monitor(char *keyword, char *buf, char *err_msg)
{
  char key[DYNAMIC_VECTOR_MON_MAX_LEN]; 
  char SendBuffer[10 * DYNAMIC_VECTOR_MON_MAX_LEN];
  int value;

  NSDL2_MON(NULL, NULL, "Method called, keyword = %s, buf = %s, global_settings->net_diagnostics_mode=%d", 
                                                     keyword, buf, global_settings->net_diagnostics_mode);

  if(sscanf(buf, "%s %d", key, &value) != 2) 
  {
    NS_KW_PARSING_ERR(keyword, 0, err_msg, ND_ENABLE_DB_CALL_MONITOR_USAGE, CAV_ERR_1011359, key);
  }

  if(!(global_settings->net_diagnostics_mode))
    return 0;

  // If ND DB Call Monitor is enabled, then we need to add dynamic vector monitor 
  // This monitor talks to ND Collector to get vectors and data
  if(value == 1)
  {
    sprintf(SendBuffer, "DYNAMIC_VECTOR_MONITOR %s:%d NDEnableDBCallMon cm_nd_db_call_stats.gdf 2 cm_nd_db_call_mon_data EOC "
                        "cm_nd_db_call_mon", global_settings->net_diagnostics_server, global_settings->net_diagnostics_port);

    NSDL2_MON(NULL, NULL, "Adding %s", SendBuffer);

    kw_set_dynamic_vector_monitor("DYNAMIC_VECTOR_MONITOR", SendBuffer, NULL, 0, 0, NULL, err_msg, NULL, NULL, 0);   
  }
  return 0;
}


//ND_ENABLE_BT_MONITOR <0/1>
int kw_set_nd_enable_bt_monitor(char *keyword, char *buf, char *err_msg)
{
  char key[DYNAMIC_VECTOR_MON_MAX_LEN] = "";
  char SendBuffer[DYNAMIC_VECTOR_MON_MAX_LEN];
  char temp[DYNAMIC_VECTOR_MON_MAX_LEN];
  temp[0] = '\0';
  int value;
  int args = 0;

  NSDL2_MON(NULL, NULL, "Method called, keyword = %s, buf = %s, global_settings->net_diagnostics_mode=%d", keyword, buf, global_settings->net_diagnostics_mode);
  
  args = sscanf(buf, "%s %d %s", key, &value, temp);

  if(temp[0] != '\0')
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL,EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Usage of this keyword is ND_ENABLE_BT_MONITOR <0/1>, anything else passed with this keyword %s is not supported now. Please remove it from scenario.",temp);
  }

  
  if(args > 3 || args < 2)                      //keeping this for backward compatibility, earlier, we were using file_path as third arguement. 
  {
    NS_KW_PARSING_ERR(keyword, 0, err_msg, ND_ENABLE_BT_MONITOR_USAGE, CAV_ERR_1011359, key); 
  }

  if(!(global_settings->net_diagnostics_mode))  
    return 0;

  if(value == 1)
  {

    sprintf(SendBuffer, "DYNAMIC_VECTOR_MONITOR %s:%d NDBTMON cm_nd_bt.gdf 2 cm_nd_bt_mon_data EOC " 
                        "cm_nd_bt_mon", global_settings->net_diagnostics_server, global_settings->net_diagnostics_port);

    NSDL2_MON(NULL, NULL, "Adding %s", SendBuffer);

    kw_set_dynamic_vector_monitor("DYNAMIC_VECTOR_MONITOR", SendBuffer, NULL, 0, 0, NULL, err_msg, NULL, NULL, 0);   
  }
  return 0; 
}


//ND_ENABLE_BT_IP_MONITOR <0/1>
int kw_set_nd_enable_bt_ip_monitor(char *keyword, char *buf, char *err_msg)
{
  char key[DYNAMIC_VECTOR_MON_MAX_LEN]; 
  char SendBuffer[10 * DYNAMIC_VECTOR_MON_MAX_LEN];
  int value;
  char temp[DYNAMIC_VECTOR_MON_MAX_LEN];
  temp[0] = '\0';
  int args=0;

  NSDL2_MON(NULL, NULL, "Method called, keyword = %s, buf = %s, global_settings->net_diagnostics_mode=%d", 
                                                     keyword, buf, global_settings->net_diagnostics_mode);

  args = sscanf(buf, "%s %d %s", key, &value, temp);

  if(temp[0] != '\0')
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL,EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Usage of this keyword is ND_ENABLE_BT_IP_MONITOR <0/1>, anything else passed with this keyword %s is not supported now. Please remove it from scenario.",temp);
  }
 

  if(args > 3 || args < 2)              //keeping this for backward compatibility, earlier, we were using file_path as third arguement.
  {
    NS_KW_PARSING_ERR(keyword, 0, err_msg, ND_ENABLE_BT_IP_MONITOR_USAGE, CAV_ERR_1011359, key);
  }

  if(!(global_settings->net_diagnostics_mode)) 
   return 0;

  // If ND BT IP Monitor is enabled, then we need to add dynamic vector monitor 
  // This monitor talks to ND Collector to get vectors and data

  
  if(value == 1)
  {

    sprintf(SendBuffer, "DYNAMIC_VECTOR_MONITOR %s:%d NDBTIPMON cm_nd_bt_ip.gdf 2 cm_nd_bt_ip_mon_data EOC "
                        "cm_nd_bt_ip_mon", global_settings->net_diagnostics_server, global_settings->net_diagnostics_port);
  
    NSDL2_MON(NULL, NULL, "Adding %s", SendBuffer);

    kw_set_dynamic_vector_monitor("DYNAMIC_VECTOR_MONITOR", SendBuffer, NULL, 0, 0, NULL, err_msg, NULL, NULL, 0);
  }
  return 0;
}

/*******************GDF name to Vector Name************************************/
void make_vector_name_from_gdf(char *gdf_name,char *vector_name)
{
  char temp_gdf_name[DYNAMIC_VECTOR_MON_MAX_LEN];
  char *ptr = NULL;

  int i=0,j=0;

  strcpy(temp_gdf_name, gdf_name); 
  ptr=strstr(temp_gdf_name,".gdf");
  if(!ptr)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                       "Wrong gdf_name is passed %s. Hence skipping making of vector_name.", gdf_name);
    return;
  }
  *ptr='\0';
  
  for(i=0; temp_gdf_name[i]!='\0'; i++)
  {
    if(temp_gdf_name[i] == '_')
    {
      continue;
    }
    else
    {
      vector_name[j] = toupper(temp_gdf_name[i]);           //vector name is program name only in upper case
    }
    j++;
  }
  return; 
}


/******************Parsing of ENABLE_VECTOR_HASH***********************/
//This keyword is for create hash vector table for given gdf monitor
//if Same gdf comes twice we consider the first gdf setting.
/*
 ENABLE_VECTOR_HASH <0/1> <gdf_name> <count>

 Description -> This keyword is used for create hash vector table for given gdf monitor and max count is 2400.

 -> This keyword is used for DVM monitors not for CM. If user mention this keyword for CM then we neglect it.

 -> The max count is 2400 if User give more than this then we will use 2400 count.

 ->  <count> is optional Default is 64*/
void kw_set_enable_vector_hash(char *keyword, char *buf, char *err_msg)
{
  char key[DYNAMIC_VECTOR_MON_MAX_LEN];
  char gdf_name[DYNAMIC_VECTOR_MON_MAX_LEN];
  int args=0;
  int value;
  int vector_gdf_hash_row;
  int key_size =-1;

  NSDL2_MON(NULL, NULL, "Method called, keyword = %s, buf = %s", keyword, buf);

  args = sscanf(buf, "%s %d %s %d", key, &value, gdf_name, &key_size);

  if(value ==0)
    return;

  if(args < 3)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
                                "Wrong usage of keyword %s. Buffer = %s. USAGE (ENABLE_VECTOR_HASH <0/1> <gdf_name> <key_size>. and max count is 2400", keyword, buf);
    return;
  }

  //reallocation of destination tier info structure
  if(create_table_entry_ex(&vector_gdf_hash_row, &total_vector_gdf_hash_entries, &max_vector_gdf_hash_entries, (char **)&vector_gdf_hash, sizeof(VectorGdfHash), "Vector GDF Hash Table") == -1)
  {
    sprintf(err_msg, "Could not create table entry for Vector GDF Hash Table\n");
    return;
  }
 

  MALLOC_AND_COPY( gdf_name, vector_gdf_hash[total_vector_gdf_hash_entries].gdf_name, (strlen(gdf_name) + 1), "Copy GDF name", -1);

  if(key_size > 2400)
   vector_gdf_hash[total_vector_gdf_hash_entries].key_size = 2400;
  if(key_size > 0)
    vector_gdf_hash[total_vector_gdf_hash_entries].key_size = key_size;
  else
    vector_gdf_hash[total_vector_gdf_hash_entries].key_size = 64;

  total_vector_gdf_hash_entries++;
  return;
}





/*******************Parsing of ND_ENABLE_MONITOR keyword************************/

void kw_set_nd_enable_monitor(char *keyword, char *buf, char *err_msg)
{
  char key[DYNAMIC_VECTOR_MON_MAX_LEN];
  char SendBuffer[10 * DYNAMIC_VECTOR_MON_MAX_LEN];
  int value;
  char prgm_name[DYNAMIC_VECTOR_MON_MAX_LEN];
  char gdf_name[DYNAMIC_VECTOR_MON_MAX_LEN];
  char vector_name[DYNAMIC_VECTOR_MON_MAX_LEN];
  int args=0;
  
  NSDL2_MON(NULL, NULL, "Method called, keyword = %s, buf = %s, global_settings->net_diagnostics_mode=%d",
                                                     keyword, buf, global_settings->net_diagnostics_mode);

  if(!(global_settings->net_diagnostics_mode))
    return;

  args = sscanf(buf, "%s %d %s %s", key, &value, gdf_name, prgm_name);
  
  if((args !=4) || ((value != 0) && (value != 1)))
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Wrong usage of keyword %s. Buffer = %s. USAGE (ND_ENABLE_MONITOR <0/1> <gdf_name> <prgm_name>.", keyword, buf);
    return;
  }

  
  if(value == 1) 
  {
    make_vector_name_from_gdf(gdf_name, vector_name);
 
    if(vector_name[0] == '\0')
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                       "Enable to make vector_name from gdf %s. Hence skipping processing of ND_ENABLE_MONITOR keyword %s. Buffer = %s.", keyword, buf);
      return;
    }
    sprintf(SendBuffer, "DYNAMIC_VECTOR_MONITOR %s:%d %s %s 2 %s EOC %s", global_settings->net_diagnostics_server, global_settings->net_diagnostics_port, vector_name, gdf_name, prgm_name, prgm_name);

    NSDL2_MON(NULL, NULL, "Adding %s", SendBuffer);

    kw_set_dynamic_vector_monitor("DYNAMIC_VECTOR_MONITOR", SendBuffer, NULL, 0, 0, NULL, err_msg, NULL, NULL, 0);
  }
}


/*****************************************************************************************************/

/*static inline void dvm_remove_select_and_close(DynamicVectorMonitorInfo *dynamic_vector_monitor_ptr)
{

  NSDL2_MON(NULL, NULL, "Method called. dynamic_vector_monitor_ptr->fd = %d", dynamic_vector_monitor_ptr->fd);

  if(dynamic_vector_monitor_ptr->fd <= 0)
     return;

  //remove_select_msg_com_con_ex(__FILE__, __LINE__, (char *)__FUNCTION__,dynamic_vector_monitor_ptr->fd);

  if(close(dynamic_vector_monitor_ptr->fd) < 0)
  {
    NSTL1(NULL, NULL, "Erorr in closing socket. fd = %d, Error = %s", dynamic_vector_monitor_ptr->fd, strerror(errno));
  }

  dynamic_vector_monitor_ptr->fd = -1;
  NSDL2_MON(NULL, NULL, "Method Exit");
}*/



/* --- START: Method used for epoll  ---*/

static inline void close_dynamic_vector_monitor_connection(DynamicVectorMonitorInfo *dynamic_vector_monitor_ptr)
{
  NSDL2_MON(NULL, NULL, "Method called. DynamicVectorMonitor => %s", dvm_to_str(dynamic_vector_monitor_ptr)); 
 
    CLOSE_FD(dynamic_vector_monitor_ptr->fd);
  
}


/* This method sets group_num_vectors for the first occurence of custom monitor of a dynamic vector monitor having same GDF
 * This is done so that in testrun.gdf file, we get one entry for Group for all dynamic vector monitors which have same GDF.
 * For example, for following Dyn Vec Mon:
 * DYNAMIC_VECTOR_MONITOR NSAppliance NSApplianceDF cm_df.gdf 2 cm_df -i 10 EOC cm_df -v NS
 * DYNAMIC_VECTOR_MONITOR NOAppliance NOApplianceDF cm_df.gdf 2 cm_df -i 10 EOC cm_df -v NO
 * 
 * we will have group_num_vectors for first cm_info_ptr set to total of all vectors for both these dyn vec mon so that in 
 * testrun.gdf, we have on Group line with all vectors for both these mon.
 * This is done so that Execution GUI can do Open All members type of feature for all vecotor across all mon with same GDF
 */

void initialize_dyn_mon_vector_groups(Monitor_list *dyn_cm_start_ptr, int start_indx, int total_entries)
{
  int mon_idx, local_mon_idx;
  CM_info *cm_ptr = NULL;
  CM_info *local_cm_ptr = NULL;

  NSDL2_MON(NULL, NULL, "Method called.");

  for (mon_idx = start_indx; mon_idx < total_entries; mon_idx++)
  {
    cm_ptr = dyn_cm_start_ptr[mon_idx].cm_info_mon_conn_ptr;
    NSDL2_MON(NULL, NULL, "mon_idx %d, GDF Name %s, monitor %s, is_dynamic %d", mon_idx, cm_ptr->gdf_name, 
                           cm_ptr->monitor_name, dyn_cm_start_ptr[mon_idx].is_dynamic);

    while(local_mon_idx < total_entries)
    {
      local_cm_ptr = dyn_cm_start_ptr[local_mon_idx].cm_info_mon_conn_ptr;

      if(!strcmp(cm_ptr->gdf_name, local_cm_ptr->gdf_name))
      {
        NSDL2_MON(NULL, NULL, "Dynamic vector monitor at index %d found with same GDF Name = %s", mon_idx, cm_ptr->gdf_name);
        dyn_cm_start_ptr[mon_idx].no_of_monitors++;
      } 
      else
      {
        mon_idx = local_mon_idx;
        break;
      }
      local_mon_idx++;
    }
  }
}


//This method will return the vector count from the space separated vector list.
int get_vector_count(char *buf)
{
  int vec_count = 1;
  while(*buf != 0) 
  {
    if(*buf == ' ') vec_count++;
    buf++;
  }
  return vec_count;

}

int init_and_check_if_mon_applied(char *tmp_buf)
{
  int specific_server_new_flag = 0;
  if(specific_server_id_key == NULL)
  {
    MY_MALLOC(specific_server_id_key, (16*1024 * sizeof(NormObjKey)), "Memory allocation to Norm specific server table", -1);
    nslb_init_norm_id_table(specific_server_id_key, 16*1024);
  }


  nslb_get_set_or_gen_norm_id_ex(specific_server_id_key, tmp_buf, strlen(tmp_buf), &specific_server_new_flag);

  if(!specific_server_new_flag)
  {
    NSDL1_MON(NULL, NULL, "String tmp_buf '%s' already exist in hash table", tmp_buf);

    return 1;
  }
  
  NSDL1_MON(NULL, NULL, "String tmp_buf '%s' is new entry in hash table", tmp_buf);

  return 0;
}

//free json info ptr
void free_json_info_ptr(JSON_info *json_info_ptr)
{
  FREE_AND_MAKE_NULL(json_info_ptr->vectorReplaceFrom, "json_info_ptr->vectorReplaceFrom", -1);
  FREE_AND_MAKE_NULL(json_info_ptr->vectorReplaceTo, "json_info_ptr->vectorReplaceTo", -1);
  FREE_AND_MAKE_NULL(json_info_ptr->javaClassPath, "json_info_ptr->javaClassPath", -1);
  FREE_AND_MAKE_NULL(json_info_ptr->javaHome, "json_info_ptr->javaHome", -1);
  FREE_AND_MAKE_NULL(json_info_ptr->init_vector_file, "json_info_ptr->init_vector_file", -1);
  FREE_AND_MAKE_NULL(json_info_ptr->mon_name, "json_info_ptr->mon_name", -1);
  FREE_AND_MAKE_NULL(json_info_ptr->config_json, "json_info_ptr->config_json", -1);
  FREE_AND_MAKE_NULL(json_info_ptr->use_agent, "json_info_ptr->use_agent", -1);
}

//copy source json info ptr to destination src_json_info_ptr
void fill_json_info_ptr_struct(JSON_info *dest_json_info_ptr, JSON_info *src_json_info_ptr)
{
  if(src_json_info_ptr->vectorReplaceTo)
  {
    MALLOC_AND_COPY(src_json_info_ptr->vectorReplaceTo ,dest_json_info_ptr->vectorReplaceTo, (strlen(src_json_info_ptr->vectorReplaceTo) + 1), "JSON Info Ptr VectorReplaceTo", -1);
    //strcpy(dest_json_info_ptr->vectorReplaceTo, src_json_info_ptr->vectorReplaceTo);
  }

  if(src_json_info_ptr->vectorReplaceFrom)
  {
    MALLOC_AND_COPY(src_json_info_ptr->vectorReplaceFrom ,dest_json_info_ptr->vectorReplaceFrom, (strlen(src_json_info_ptr->vectorReplaceFrom)+1), "JSON Info Ptr VectorReplaceFrom", -1);
    //strcpy(dest_json_info_ptr->vectorReplaceFrom, src_json_info_ptr->vectorReplaceFrom);
  }

  dest_json_info_ptr->tier_server_mapping_type = src_json_info_ptr->tier_server_mapping_type;
  dest_json_info_ptr->metric_priority = src_json_info_ptr->metric_priority;

  if(src_json_info_ptr->javaHome)     
  {
    MALLOC_AND_COPY(src_json_info_ptr->javaHome ,dest_json_info_ptr->javaHome, (strlen(src_json_info_ptr->javaHome) + 1), "JSON Info Ptr JavaHome", -1);
    //strcpy(dest_json_info_ptr->javaHome, src_json_info_ptr->javaHome);
  }

  if(src_json_info_ptr->javaClassPath)
  {
    MALLOC_AND_COPY(src_json_info_ptr->javaClassPath ,dest_json_info_ptr->javaClassPath, (strlen(src_json_info_ptr->javaClassPath) + 1), "JSON Info Ptr javaClassPath", -1);
    //strcpy(dest_json_info_ptr->javaClassPath, src_json_info_ptr->javaClassPath);
  }

  if(src_json_info_ptr->init_vector_file)     
  {
    MALLOC_AND_COPY(src_json_info_ptr->init_vector_file ,dest_json_info_ptr->init_vector_file,(strlen(src_json_info_ptr->init_vector_file) +1),"JSON Info Ptr init vector file", -1);
    //strcpy(dest_json_info_ptr->init_vector_file, src_json_info_ptr->init_vector_file);
  }
  if(src_json_info_ptr->config_json)         
  {
    MALLOC_AND_COPY(src_json_info_ptr->config_json, dest_json_info_ptr->config_json, strlen(src_json_info_ptr->config_json) + 1, "JSON Info config json", -1);
  }

  if(src_json_info_ptr->mon_name)
  {
    MALLOC_AND_COPY(src_json_info_ptr->mon_name, dest_json_info_ptr->mon_name, strlen(src_json_info_ptr->mon_name) + 1, "JSON Info config json", -1);
  }
  dest_json_info_ptr->agent_type = src_json_info_ptr->agent_type;
  dest_json_info_ptr->any_server = src_json_info_ptr->any_server;
  dest_json_info_ptr->dest_any_server_flag = src_json_info_ptr->dest_any_server_flag; //for destination tier

}

//this function is to free the mon list info linklist
void free_mon_list_info(Mon_List_Info *mon_list) 
{
  FREE_AND_MAKE_NULL( mon_list->prgrm_args ,"Freeing program args of mon list", -1);
  FREE_AND_MAKE_NULL( mon_list->mon_name ,"Freeing mon name of mon list", -1);
  FREE_AND_MAKE_NULL( mon_list->gdf_name ,"Freeing gdf name of mon list", -1);
  FREE_AND_MAKE_NULL( mon_list->monitor_name ,"Freeing monitor name of mon list", -1);
  FREE_AND_MAKE_NULL( mon_list->source_server_ip ,"Freeing source server ip of mon list", -1);
  free_json_info_ptr(mon_list->json_info_ptr);
  FREE_AND_MAKE_NULL( mon_list->json_info_ptr ,"Freeing JSON info ptr of mon list", -1);
  
}

//this function is for searching of mon name from mon list info linklist
Mon_List_Info *search_mon_name_in_mon_list_info(char *mon_name, int dest_tier_name_norm_id, int source_tier_idx, char *server_ip,
                                                char *monitor_name)
{
  Mon_List_Info *mon_list;
  mon_list = dest_tier_info[dest_tier_name_norm_id].source_tier_info[source_tier_idx].mon_list_info_pool.busy_head;

  while (mon_list != NULL)
  {
    //This case will happen when server is not present in tier and user going to delete the monitor through UI.
    if( server_ip == NULL )
    {
      if(strcmp(mon_list->monitor_name, monitor_name) == 0)
        return mon_list;
    }
    else
    {
      if(strcmp(mon_name, mon_list->mon_name) == 0) //if mon name matched
        return mon_list;
    }
    mon_list = nslb_next(mon_list);
  }
  return NULL;
}


//This is used for make delete mon buf for tier-for-any-server and also delete entry of monitor from destination tier monitor structure
int delete_entry_from_dest_tier_info_struct(JSON_info *json_info_ptr, int server_index, int tier_id, char tmp_buf[MAX_DATA_LINE_LENGTH],
                                            char (*monitor_buf)[32*MAX_DATA_LINE_LENGTH], char group_id[8], char fname[MAX_LINE_LENGTH],
                                            char *monitor_name)
{
  
  char mon_list_name[MAX_DATA_LINE_LENGTH]; //this is used for making the mon name for monitor
  
  int mon_buf_id =0;
  int i ; //for looping purpose
  int dest_tier_name_norm_id; //for destination tier name normalized table 
  int source_tier_idx;  
  
  //Dest_Tier_Info *dest_tier_info;
  Mon_List_Info *mon_list; //this pointer is used for monitor list info  
  
  for(i=0; i<json_info_ptr->no_of_dest_any_server_elements; i++)
  {
    if(dest_tier_name_id_key != NULL)
    {
    //get destination tier normalized id
    dest_tier_name_norm_id =nslb_get_norm_id(dest_tier_name_id_key, json_info_ptr->dest_any_server_arr[i],
                                             strlen(json_info_ptr->dest_any_server_arr[i]));
    if( dest_tier_name_norm_id != -2 )
    {
      sprintf(fname, "%s/sys/%s", g_ns_wdir, tmp_buf);
      get_group_id_from_gdf_name(fname, group_id);

      sprintf(monitor_buf[mon_buf_id],"DELETE_MONITOR GroupMon %s NA %s%c%s%c%s %s",group_id,topo_info[topo_idx].topo_tier_info[tier_id].tier_name,global_settings->hierarchical_view_vector_separator,topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_disp_name,global_settings->hierarchical_view_vector_separator,dest_tier_info[dest_tier_name_norm_id].dest_server_ip ,tmp_buf);
      //for checking mon_name present in mon list 

      sprintf(mon_list_name, "%s%c%s%c%s" ,topo_info[topo_idx].topo_tier_info[tier_id].tier_name ,global_settings->hierarchical_view_vector_separator,topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_disp_name, global_settings->hierarchical_view_vector_separator,dest_tier_info[dest_tier_name_norm_id].dest_server_ip);

      mon_buf_id += 1;
      MLTL4(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Delete monitor of destination tier whose monitor buf is %s",monitor_buf[mon_buf_id]);

      //get source tier normalized id
      for(source_tier_idx =0; source_tier_idx < dest_tier_info[dest_tier_name_norm_id].total_source_tier ;source_tier_idx++ )
      {  
        if( dest_tier_info[dest_tier_name_norm_id].source_tier_info[source_tier_idx].source_tier_id == tier_id )
        {
          //search mon name from mon_list
          mon_list = search_mon_name_in_mon_list_info(mon_list_name, dest_tier_name_norm_id, source_tier_idx, 
                                                      dest_tier_info[dest_tier_name_norm_id].dest_server_ip, monitor_name);
      
          if(mon_list == NULL)
          {
            MLTL1(EL_DF, 0, 0, _FLN_, NULL , EID_DATAMON_ERROR, EVENT_INFORMATION,"Invalid mon name (%s) deletion request", mon_list_name);
            continue;
          }
          // free mon_list and return slot to mp pool
          free_mon_list_info(mon_list);
          nslb_mp_free_slot(&(dest_tier_info[dest_tier_name_norm_id].source_tier_info[source_tier_idx].mon_list_info_pool), mon_list);
          break;
        }
      }
    }
    }
  }
  return mon_buf_id;  
}


//make monitor buf and populate the mon list with monitors
void make_monitor_list(JSON_info *json_info_ptr ,int tier_id, int server_idx, char *tmp_prgrm_args, char *prgrm_args, char (*monitor_buf)[32*MAX_DATA_LINE_LENGTH] ,char *gdf_name ,char *mon_name ,int dest_tier_name_norm_id ,int mon_buf_id )
{
  char tmp_buf[MAX_DATA_LINE_LENGTH];
  char mon_list_name[MAX_DATA_LINE_LENGTH]; //this is used for making the mon name for monitor

  int source_tier_idx = 0;
  int source_tier_name_new_flag = 1;
  int source_tier_id;
 
  Mon_List_Info *mon_list;
  
  //looping existing source tier in source tier info 
  for(source_tier_idx=0; source_tier_idx < dest_tier_info[dest_tier_name_norm_id].total_source_tier ;source_tier_idx++ )
  {
    if(dest_tier_info[dest_tier_name_norm_id].source_tier_info[source_tier_idx].source_tier_id == tier_id)
    {
      source_tier_name_new_flag =0; 
      source_tier_id =source_tier_idx;
    }
  }

  //get new source tier name
  if(source_tier_name_new_flag)
  {
    source_tier_id = dest_tier_info[dest_tier_name_norm_id].total_source_tier;
    //filling source tier info structure
    MALLOC_AND_COPY( topo_info[topo_idx].topo_tier_info[tier_id].tier_name ,dest_tier_info[dest_tier_name_norm_id].source_tier_info[source_tier_id].source_tier_name,(strlen(topo_info[topo_idx].topo_tier_info[tier_id].tier_name) + 1),"Copying source tier name", -1); //copying source tier name 
    

    dest_tier_info[dest_tier_name_norm_id].source_tier_info[source_tier_id].source_tier_id = tier_id; //store source tier id
    dest_tier_info[dest_tier_name_norm_id].total_source_tier += 1;  //store total no. of source tier
  }

  //make monitor list linklist
  mon_list =(Mon_List_Info*)nslb_mp_get_slot(&(dest_tier_info[dest_tier_name_norm_id].source_tier_info[source_tier_id].mon_list_info_pool));

  //making monitor buf array
  if ( dest_tier_info[dest_tier_name_norm_id].dest_server_ip_active ) //active destination server ip
  {
    if(enable_store_config && !strcmp(topo_info[topo_idx].topo_tier_info[tier_id].tier_name, "Cavisson"))
      sprintf(tmp_buf, "%s!%s", g_store_machine, topo_info[topo_idx].topo_tier_info[tier_id].tier_name);
    else
      sprintf(tmp_buf, "%s", topo_info[topo_idx].topo_tier_info[tier_id].tier_name);

    //here if we found %cav_tier_any_server% in instance then we make hierachy Source_tier>Source_server_name>Destination_server_name
    sprintf(monitor_buf[mon_buf_id],"STANDARD_MONITOR %s%c%s %s%c%s%c%s %s %s",topo_info[topo_idx].topo_tier_info[tier_id].tier_name,global_settings->hierarchical_view_vector_separator, topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_idx].server_disp_name,tmp_buf, global_settings->hierarchical_view_vector_separator, topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_idx].server_disp_name,global_settings->hierarchical_view_vector_separator,dest_tier_info[dest_tier_name_norm_id].dest_server_ip , mon_name,prgrm_args);

    sprintf(mon_list_name , "%s%c%s%c%s" ,topo_info[topo_idx].topo_tier_info[tier_id].tier_name , global_settings->hierarchical_view_vector_separator, topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_idx].server_disp_name , global_settings->hierarchical_view_vector_separator , dest_tier_info[dest_tier_name_norm_id].dest_server_ip); //this buffer is used for mon_name in mon list  
  
    
   MALLOC_AND_COPY( mon_list_name ,mon_list->mon_name, (strlen(mon_list_name) + 1), "Copying monitor name",-1); //store mon name for deletion purpose 
    MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"Apply json auto monitor on destination %s tier Monitor name %s and mon_name breadcrumb is  %s",dest_tier_info[dest_tier_name_norm_id].dest_server_ip , mon_name , mon_list_name);
  }

  //populate monitor list with monitor info
  MALLOC_AND_COPY(tmp_prgrm_args, mon_list->prgrm_args, (strlen(tmp_prgrm_args) + 1), "Copying progrms args of monitor", -1);
  MALLOC_AND_COPY(mon_name, mon_list->monitor_name, (strlen(mon_name) + 1), "Copying monitor name of monitor", -1);
  MALLOC_AND_COPY(gdf_name, mon_list->gdf_name, (strlen(gdf_name) + 1), "Copying gdf name of monitor", -1);
  MALLOC_AND_COPY(topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_idx].server_disp_name ,mon_list->source_server_ip , (strlen(topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_idx].server_disp_name) + 1), "Copying source server ip", -1 );//store src ip
  
  MY_MALLOC_AND_MEMSET(mon_list->json_info_ptr, sizeof(JSON_info), "Monitor JSON Info Ptr", -1);
  fill_json_info_ptr_struct(mon_list->json_info_ptr ,json_info_ptr);  //copy json_info_ptr
  
}


/*************Destination Tier Monitor Buffer*******************/
int create_mon_buf_arr(JSON_info *json_info_ptr,int tier_id, int source_server_idx ,char *tmp_prgrm_args ,char (*monitor_buf)[32*MAX_DATA_LINE_LENGTH] ,char *mon_name ,char *gdf_name, char (*instance_name)[64] )
{
  char err_msg[MAX_DATA_LINE_LENGTH];
  char prgrm_args[32*MAX_DATA_LINE_LENGTH];
  char leftover_arguments[MAX_DATA_LINE_LENGTH];
  char *variable_ptr = NULL;  
 
  int i, k; //for looping purpose
  int count; //for looping
  int dest_tier_name_norm_id; //for destination tier name normalized table 
  int dest_tier_id;  //destination tier name id
  int dest_tier_name_new_flag;
  int dest_tier_info_row;
  int max_source_tier_entries;

  int mon_buf_id =0; //for monitor buffer id

  StdMonitor *std_mon_ptr = NULL;
  //this is done to get the gdf name if gdf name is null
  if(gdf_name == NULL)
  { 
    if((std_mon_ptr = get_standard_mon_entry(mon_name, topo_info[topo_idx].topo_tier_info[tier_id].topo_server[source_server_idx].server_ptr->machine_type,0, err_msg)) == NULL)
    {
      sprintf(err_msg, "Error: Standard Monitor Name '%s' or server type '%s' is incorrect\n", 
                        mon_name,topo_info[topo_idx].topo_tier_info[tier_id].topo_server[source_server_idx].server_ptr->machine_type);
      strcat(err_msg, "Please provide valid standard monitor name\n");
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,"Error %s",err_msg);
      err_msg[0]='\0';
      return 0;
    }
    gdf_name = std_mon_ptr->gdf_name;
  }

  //here we make destination tier name normalized table  
  if(dest_tier_name_id_key == NULL)
  {
    MY_MALLOC(dest_tier_name_id_key, (16*1024 * sizeof(NormObjKey)), "Memory allocation to Norm destination tier name table", -1);
    nslb_init_norm_id_table(dest_tier_name_id_key, 16*1024);
  }
 
  //here we are looping destination tier name which is present in json tag 
  for ( i=0; i<json_info_ptr->no_of_dest_any_server_elements; i++)
  {
    dest_tier_name_norm_id = nslb_get_set_or_gen_norm_id_ex(dest_tier_name_id_key, json_info_ptr->dest_any_server_arr[i] , strlen(json_info_ptr->dest_any_server_arr[i]), &dest_tier_name_new_flag);
    
    //if new destination tier comes increase dest tier count by 1
    if(dest_tier_name_new_flag) //for new - value will be 1
    {
      total_dest_tier +=1;  //this will store for total destination tier names
    } 
    //reallocation of destination tier info structure
    if(create_table_entry_ex(&dest_tier_info_row, &dest_tier_name_norm_id, &max_dest_tier_entries, (char **)&dest_tier_info, sizeof(Dest_Tier_Info), "Destination Tier Info Table") == -1)
    {
      sprintf(err_msg, "Could not create table entry for Destination Tier Info\n");
    }

    //reallocation of source tier info structure
    max_source_tier_entries = dest_tier_info[dest_tier_name_norm_id].max_source_tier_entries;
    if ( dest_tier_info[dest_tier_name_norm_id].total_source_tier >= max_source_tier_entries )
    {
      NSLB_REALLOC_AND_MEMSET( dest_tier_info[dest_tier_name_norm_id].source_tier_info , ( max_source_tier_entries + DELTA_MON_ID_ENTRIES) * sizeof(Source_Tier_Info), max_source_tier_entries * sizeof(Source_Tier_Info), "Reallocating destination source  tier info struct ", -1, 0);
      
      count = dest_tier_info[dest_tier_name_norm_id].max_source_tier_entries;
      dest_tier_info[dest_tier_name_norm_id].max_source_tier_entries += DELTA_MON_ID_ENTRIES;
      
      // init memory pool for newly added entries
      for ( ; count < dest_tier_info[dest_tier_name_norm_id].max_source_tier_entries ; count++) 
      {
        nslb_mp_init(&(dest_tier_info[dest_tier_name_norm_id].source_tier_info[count].mon_list_info_pool), sizeof(Mon_List_Info), DELTA_MON_ID_ENTRIES , DELTA_MON_ID_ENTRIES , NON_MT_ENV);
        nslb_mp_create(&(dest_tier_info[dest_tier_name_norm_id].source_tier_info[count].mon_list_info_pool));
      }
    }
    
    dest_tier_id = topolib_get_tier_id_from_tier_name( json_info_ptr->dest_any_server_arr[i], topo_idx );  // destination tier name id
    
    //checking if tier name is not present in topology
    if( dest_tier_id == -1)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"Failed to apply json auto monitor on %s tier.This tier is not present in topology",json_info_ptr->dest_any_server_arr[i]);
    }
      //here we are looping of destionation tier total servers
    for(k = 0;k < topo_info[topo_idx].topo_tier_info[dest_tier_id].total_server ; k++)
    {
      if(topo_info[topo_idx].topo_tier_info[dest_tier_id].topo_server[k].used_row != -1)
      {        //Active server found in destination tier
        if(topo_info[topo_idx].topo_tier_info[dest_tier_id].topo_server[k].server_ptr->status == 1)
        {
          //replace %cav_tier_any_server% with server ip
          variable_ptr = NULL;
          strcpy(prgrm_args, tmp_prgrm_args);
          variable_ptr=strstr(prgrm_args, "%cav_tier_any_server%");
          strcpy(leftover_arguments, variable_ptr + 21);
          strcpy(variable_ptr,topo_info[topo_idx].topo_tier_info[dest_tier_id].topo_server[k].server_ptr->server_name);
          strcpy(variable_ptr+ strlen(topo_info[topo_idx].topo_tier_info[dest_tier_id].topo_server[k].server_ptr->server_name),leftover_arguments);
          dest_tier_info[dest_tier_name_norm_id].dest_server_ip_active = true; // for checking destination server ip active or not
          MALLOC_AND_COPY( topo_info[topo_idx].topo_tier_info[dest_tier_id].tier_name, 
                         dest_tier_info[dest_tier_name_norm_id].dest_tier_name, 
                         (strlen(topo_info[topo_idx].topo_tier_info[dest_tier_id].tier_name) + 1), 
                          "Copying destination tier name", -1); //copying destination tier name
        
          MALLOC_AND_COPY(topo_info[topo_idx].topo_tier_info[dest_tier_id].topo_server[k].server_disp_name ,
                        dest_tier_info[dest_tier_name_norm_id].dest_server_ip, 
                        (strlen(topo_info[topo_idx].topo_tier_info[dest_tier_id].topo_server[k].server_disp_name) +1), 
                         "Copying destination server ip" , -1);  // copying destination server ip

          strcpy(instance_name[mon_buf_id] ,dest_tier_info[dest_tier_name_norm_id].dest_server_ip);//copying instance name  
          //make monitor buf and populate the mon list with monitors
          make_monitor_list(json_info_ptr ,tier_id, source_server_idx ,tmp_prgrm_args ,prgrm_args, monitor_buf ,gdf_name ,mon_name ,dest_tier_name_norm_id ,mon_buf_id); //here k is destination source server id and j is source server id
           
          MLTL4(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
            "Add monitor of destination tier whose monitor buf is %s",monitor_buf[mon_buf_id]);
        
          MLTL4(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
              "Apply json auto monitor on destination tier [%s] whose source server is [%s].", 
                 dest_tier_info[dest_tier_name_norm_id].dest_tier_name,
                 topo_info[topo_idx].topo_tier_info[tier_id].topo_server[source_server_idx].server_disp_name);
          mon_buf_id += 1; //for monitor buf array
          break;  //found active server on destination tier
        }   
      }
    }      // No active server found
    if(! dest_tier_info[dest_tier_name_norm_id].dest_server_ip_active ) //this handling is for destination tier server is not active so here we populate the structure for future purpose when destination tier server is auto scaled
    {
      //filling destination tier info structure
      MALLOC_AND_COPY(json_info_ptr->dest_any_server_arr[i], dest_tier_info[dest_tier_name_norm_id].dest_tier_name, (strlen(json_info_ptr->dest_any_server_arr[i]) + 1), "Copying destination tier name", 0);

      //only populate mon list with monitors and don not make monitor buf arr
      make_monitor_list(json_info_ptr ,tier_id, source_server_idx ,tmp_prgrm_args, prgrm_args, monitor_buf ,gdf_name ,mon_name ,dest_tier_name_norm_id ,mon_buf_id );

      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"Failed to apply json auto monitor due to %s tier servers is not active.", dest_tier_info[dest_tier_name_norm_id].dest_tier_name);    
    }
  }
   return mon_buf_id; 
}

int copy_program_args_based_on_cmon_version(int tier_idx,int server_index, char *prgrm_args, JSON_info *json_info_ptr, char *tmp_prgrm_args)
{
  if(json_info_ptr && server_index >= 0)
  {
    if(topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_index].server_ptr->cmon_option_flag == 1)
    {
      if(json_info_ptr->options)
        strcpy(prgrm_args, json_info_ptr->options);
      else
        return -1;
    }
    else
    {
      if(json_info_ptr->old_options)
        strcpy(prgrm_args, json_info_ptr->old_options);
    }
  }
  return 0;
}

//this function will call for one server and make monitor buff of it
int create_mon_buf_wrapper(char *exclude_server_arr[], int no_of_exclude_server_elements, char *specific_server_arr[],
                           int no_of_specific_server_elements, char *mon_name, char *tmp_prgrm_args, char *instance_name, 
                           JSON_info *json_info_ptr, int process_diff_json, char mon_type, int tier_id, char *gdf_name, 
                           JSON_MP *json_mp_ptr, int server_index, bool any_found, char *kub_vector_name, char *app_name)
{

  int i=0,j=0, k;
  int mon_buf_id = 0;
  int mon_buf_count = 0; //for monitor buffer looping
  int ret=0, et=1, mon_ret=-1 ;
  char monitor_buf[32*MAX_DATA_LINE_LENGTH];
  int mon_row = 0;
  char temp_monitor_buf[32*MAX_DATA_LINE_LENGTH];
  char err_msg[MAX_DATA_LINE_LENGTH];
  char pod_name[MAX_DATA_LINE_LENGTH];
  char group_id[8];
  char vector_name[256];
  char dyn_mon_buf[64*MAX_DATA_LINE_LENGTH] = {0};
  char dvm_data_arg_buf[] = "-L data";
  char dvm_vector_arg_buf[] = "-L header";
  char monitor_type_buf[20] = "Check Monitor";   //this is used for check monitor and batch job monitor deletion Default buf is Check Monitor
  char prgrm_args[32*MAX_DATA_LINE_LENGTH];
  char tmp_buf[MAX_DATA_LINE_LENGTH];
  char java_args[16*MAX_DATA_LINE_LENGTH] = {0};
  char java_home_and_classpath[1024]={0};
  char kub_instance_name[MAX_DATA_LINE_LENGTH];
  NSDL2_MON(NULL, NULL, "Method called, mon_name = %s, tier_id = %d", mon_name, tier_id);
  
  strncpy(prgrm_args, tmp_prgrm_args, 32*MAX_DATA_LINE_LENGTH);

  if(server_index < 0)
    return -1;
  
  MALLOC_AND_COPY(topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_disp_name ,json_info_ptr->server_name,(strlen(topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_disp_name) + 1), "Copy server name", -1);
  json_info_ptr->server_index=server_index;
  //In case of outbound, if the server is down, we still needs to create the request. When server comes up, it travers through monitor list server_info_structure.  
  if((topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_ptr->status == 1) || is_outbound_connection_enabled)
  {
    if(no_of_exclude_server_elements)
    {
      for(et = 0; et<no_of_exclude_server_elements; et++)
      {
        if(!nslb_regex_match(topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_ptr->server_name, exclude_server_arr[et]) ||  !nslb_regex_match(topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_disp_name, exclude_server_arr[et]))
        {
          ret = 1;
          break;
        }
      }
      if(ret == 1)
      {
        ret = 0;
        return -1;
      }
    }
    // 
    if(no_of_specific_server_elements  && !any_found)//if "Any" found in the specific server then do not loop
    {
      for(et = 0; et < no_of_specific_server_elements; et++)
      {
        if(nslb_regex_match(topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_disp_name, specific_server_arr[et])!=0)
        {
          ret = 1;
        }
        else
        {
          ret = 0;
          break;
        }
      }
      if(ret == 1)
      {
        ret = 0;
        return -1;
      }
    }

    if((copy_program_args_based_on_cmon_version(tier_id,server_index, prgrm_args, json_info_ptr, tmp_prgrm_args)) == -1)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"Required mandatory options tag for monitor but options tag is not mentioned in JSON. So skipping this monitor %s", mon_name);
      return -1;
    }
    //strcpy(prgrm_args, tmp_prgrm_args);
    if(kub_vector_name)
    {
      mj_replace_char_and_update_pod_name(kub_vector_name, prgrm_args, pod_name, app_name, mon_name, kub_instance_name); 
    }
    else
    {
      kub_instance_name[0] = '\0';
    }

    if(mon_type == CHECK_MON_TYPE)
    {
      NSDL2_MON(NULL, NULL, "check: ");
      
      sprintf(monitor_buf,"CHECK_MONITOR %s%c%s %s%c%s%c%s %s",topo_info[topo_idx].topo_tier_info[tier_id].tier_name,global_settings->hierarchical_view_vector_separator,topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_ptr->server_disp_name,topo_info[topo_idx].topo_tier_info[tier_id].tier_name,global_settings->hierarchical_view_vector_separator,topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_disp_name,global_settings->hierarchical_view_vector_separator,mon_name, prgrm_args);

      if(process_diff_json)
      {
        //This will allocate from start everytime once it get free 
        create_table_entry_dvm(&mon_row, &total_json_monitors, &max_json_monitors, (char **)&json_monitors_ptr, sizeof(Mon_from_json), "Check Monitors to be Applied");

        MY_MALLOC(json_monitors_ptr[mon_row].mon_buff, strlen(monitor_buf) + 1, "Check monitor buffer", mon_row);
        strcpy(json_monitors_ptr[mon_row].mon_buff, monitor_buf);
        json_monitors_ptr[mon_row].mon_type = CHECK_MONITOR;
        MY_MALLOC_AND_MEMSET(json_monitors_ptr[mon_row].json_info_ptr, sizeof(JSON_info), "JSON Info Ptr", -1);
        if(any_found)
        {

          if(json_info_ptr->instance_name)
          {
            MY_MALLOC_AND_MEMSET(json_monitors_ptr[mon_row].json_info_ptr->instance_name, strlen(instance_name), "JSON Info Ptr", -1);
            strcpy(json_monitors_ptr[mon_row].json_info_ptr->instance_name, instance_name);
          }
          // this is for monconfig struct
          if(json_info_ptr->g_mon_id)
          { 
            MALLOC_AND_COPY(json_info_ptr->g_mon_id, json_monitors_ptr[mon_row].json_info_ptr->g_mon_id, strlen(json_info_ptr->g_mon_id) + 1,
                                        "JSON INFO ptr g_mon_id", -1);
          }
          json_monitors_ptr[mon_row].json_info_ptr->mon_info_index = json_info_ptr->mon_info_index;

          MY_MALLOC_AND_MEMSET(json_monitors_ptr[mon_row].json_info_ptr->mon_name, strlen(mon_name), "JSON Info Ptr", -1);
          strcpy(json_monitors_ptr[mon_row].json_info_ptr->mon_name, mon_name);

          MY_MALLOC_AND_MEMSET(json_monitors_ptr[mon_row].json_info_ptr->args, strlen(prgrm_args), "JSON Info Ptr", -1);
          strcpy(json_monitors_ptr[mon_row].json_info_ptr->args, prgrm_args);

          json_monitors_ptr[mon_row].json_info_ptr->any_server=true;
        }
      MALLOC_AND_COPY(json_info_ptr->tier_name, json_monitors_ptr[mon_row].json_info_ptr->tier_name, strlen(json_info_ptr->tier_name) + 1,                                        "JSON INFO ptr g_mon_id", -1);  
      json_monitors_ptr[mon_row].json_info_ptr->tier_idx=json_info_ptr->tier_idx;
      
      MALLOC_AND_COPY(topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_disp_name ,
                                                   json_monitors_ptr[mon_row].json_info_ptr->server_name,(strlen(topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_disp_name) + 1), "Copy tier name", -1);
  
      json_monitors_ptr[mon_row].json_info_ptr->server_index=server_index;


     }     
     else
     {
        strcpy(temp_monitor_buf,monitor_buf);

        if(json_info_ptr && tmp_prgrm_args)
        {
          MY_MALLOC_AND_MEMSET(json_info_ptr->args, strlen(tmp_prgrm_args), "JSON Info Ptr", -1);
          strcpy(json_info_ptr->args, tmp_prgrm_args);
        }

        if((mon_ret = kw_set_check_monitor("CHECK_MONITOR", monitor_buf, process_diff_json, err_msg, json_info_ptr)) < 0 )
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                                       "Failed to apply json auto monitor with buffer: %s . Error: %s", temp_monitor_buf, err_msg);
      }
      if(topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_ptr->status == 1)
         topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_ptr->auto_monitor_applied = 1;
    }

    if(mon_type == BATCH_JOB_TYPE)
    {
      NSDL2_MON(NULL, NULL, "batch job: ");
      //here we pass mon_name withoout appending tier server SO that old design is not changed of batch jon monitors in which 
      //we pass mon_name only without appending tier server
      sprintf(monitor_buf,"BATCH_JOB %s%c%s %s %s",topo_info[topo_idx].topo_tier_info[tier_id].tier_name,global_settings->hierarchical_view_vector_separator,topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_ptr->server_name,mon_name,prgrm_args);

      if(process_diff_json)
      {
        //This will allocate from start everytime once it get free 
        create_table_entry_dvm(&mon_row, &total_json_monitors, &max_json_monitors, (char **)&json_monitors_ptr, sizeof(Mon_from_json), 
                               "Batch Job Monitors to be Applied");

        MY_MALLOC(json_monitors_ptr[mon_row].mon_buff, strlen(monitor_buf) + 1, "Batch Job monitor buffer", mon_row);
        strcpy(json_monitors_ptr[mon_row].mon_buff, monitor_buf);
        json_monitors_ptr[mon_row].mon_type = BATCH_JOB;
        MY_MALLOC_AND_MEMSET(json_monitors_ptr[mon_row].json_info_ptr, sizeof(JSON_info), "JSON Info Ptr", -1);
        if(any_found)
        {

          if(json_info_ptr->instance_name)
          {
            MY_MALLOC_AND_MEMSET(json_monitors_ptr[mon_row].json_info_ptr->instance_name, strlen(instance_name), "JSON Info Ptr", -1);
            strcpy(json_monitors_ptr[mon_row].json_info_ptr->instance_name, instance_name);
          }

          // this is for monconfig struct
          if(json_info_ptr->g_mon_id)
          {
            MALLOC_AND_COPY(json_info_ptr->g_mon_id, json_monitors_ptr[mon_row].json_info_ptr->g_mon_id, strlen(json_info_ptr->g_mon_id) + 1,
                                        "JSON INFO ptr g_mon_id", -1);
          }
          json_monitors_ptr[mon_row].json_info_ptr->mon_info_index = json_info_ptr->mon_info_index;

          MY_MALLOC_AND_MEMSET(json_monitors_ptr[mon_row].json_info_ptr->mon_name, strlen(mon_name), "JSON Info Ptr", -1);
          strcpy(json_monitors_ptr[mon_row].json_info_ptr->mon_name, mon_name);

          MY_MALLOC_AND_MEMSET(json_monitors_ptr[mon_row].json_info_ptr->args, strlen(prgrm_args), "JSON Info Ptr", -1);
          strcpy(json_monitors_ptr[mon_row].json_info_ptr->args, prgrm_args);

          json_monitors_ptr[mon_row].json_info_ptr->any_server=true;
        }
        MALLOC_AND_COPY(json_info_ptr->tier_name, json_monitors_ptr[mon_row].json_info_ptr->tier_name, strlen(json_info_ptr->tier_name) + 1,
                                                                                                    "JSON INFO ptr g_mon_id", -1);
        json_monitors_ptr[mon_row].json_info_ptr->tier_idx=json_info_ptr->tier_idx;
      
        MALLOC_AND_COPY(topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_disp_name ,
                       json_monitors_ptr[mon_row].json_info_ptr->server_name,
                       (strlen(topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_disp_name) + 1), 
                       "Copy tier name", -1);

        json_monitors_ptr[mon_row].json_info_ptr->server_index=server_index;
      }
      else
      {
        strcpy(temp_monitor_buf,monitor_buf);

        if(json_info_ptr && tmp_prgrm_args)
        {
          MY_MALLOC_AND_MEMSET(json_info_ptr->args, strlen(tmp_prgrm_args), "JSON Info Ptr", -1);
          strcpy(json_info_ptr->args, tmp_prgrm_args);
        }

        if((mon_ret = parse_job_batch(temp_monitor_buf, 0,err_msg, " ", json_info_ptr)) < 0 )
            MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                                       "Failed to apply json auto monitor with buffer: %s . Error: %s", temp_monitor_buf, err_msg);
      }
      if(topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_ptr->status ==1)
        topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_ptr->auto_monitor_applied = 1;
    }

    else if(mon_type == SERVER_SIGNATURE_MON_TYPE)
    {
      if((instance_name) && (instance_name[0] != '\0'))
      {  
        sprintf(monitor_buf,"SERVER_SIGNATURE %s%c%s %s%c%s%c%s%c%s %s",
                                            topo_info[topo_idx].topo_tier_info[tier_id].tier_name,
                                            global_settings->hierarchical_view_vector_separator,
                                            topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_disp_name,
                                            topo_info[topo_idx].topo_tier_info[tier_id].tier_name,
                                            global_settings->hierarchical_view_vector_separator,
                                            topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_disp_name,
                                            global_settings->hierarchical_view_vector_separator,
                                            instance_name,
                                            global_settings->hierarchical_view_vector_separator,
                                            mon_name,prgrm_args);
      }
      else
      {
        sprintf(monitor_buf,"SERVER_SIGNATURE %s%c%s %s%c%s%c%s %s",
                                      topo_info[topo_idx].topo_tier_info[tier_id].tier_name,
                                      global_settings->hierarchical_view_vector_separator,
                                      topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_ptr->server_disp_name,
                                      topo_info[topo_idx].topo_tier_info[tier_id].tier_name,
                                      global_settings->hierarchical_view_vector_separator,
                                      topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_ptr->server_disp_name,
                                      global_settings->hierarchical_view_vector_separator,mon_name,prgrm_args);
      }
      if(process_diff_json)
      {
        //This will allocate from start everytime once it get free 
        create_table_entry_dvm(&mon_row, &total_json_monitors, &max_json_monitors, (char **)&json_monitors_ptr, sizeof(Mon_from_json),
                               "Server Signature to be Applied");

        MY_MALLOC_AND_MEMSET(json_monitors_ptr[mon_row].json_info_ptr, sizeof(JSON_info), "JSON Info Ptr", -1);
        MY_MALLOC(json_monitors_ptr[mon_row].mon_buff, strlen(monitor_buf) + 1, "Server Signature buffer", mon_row);
        strcpy(json_monitors_ptr[mon_row].mon_buff, monitor_buf);
        json_monitors_ptr[mon_row].mon_type = SERVER_SIGNATURE;
        MY_MALLOC_AND_MEMSET(json_monitors_ptr[mon_row].json_info_ptr->instance_name, strlen(instance_name), "JSON Info Ptr", -1);
        if(any_found)
        {
          //MY_MALLOC_AND_MEMSET(json_monitors_ptr[mon_row].json_info_ptr, sizeof(JSON_info), "JSON Info Ptr", -1);
          if(instance_name)
          {
            strcpy(json_monitors_ptr[mon_row].json_info_ptr->instance_name, instance_name);
          }

          // this is for monconfig struct
          if(json_info_ptr->g_mon_id)
          {
            MALLOC_AND_COPY(json_info_ptr->g_mon_id, json_monitors_ptr[mon_row].json_info_ptr->g_mon_id, strlen(json_info_ptr->g_mon_id) + 1,
                                        "JSON INFO ptr g_mon_id", -1);
          }
          json_monitors_ptr[mon_row].json_info_ptr->mon_info_index = json_info_ptr->mon_info_index;

          MY_MALLOC_AND_MEMSET(json_monitors_ptr[mon_row].json_info_ptr->mon_name, strlen(mon_name), "JSON Info Ptr", -1);
          strcpy(json_monitors_ptr[mon_row].json_info_ptr->mon_name, mon_name);

          MY_MALLOC_AND_MEMSET(json_monitors_ptr[mon_row].json_info_ptr->args, strlen(prgrm_args), "JSON Info Ptr", -1);
          strcpy(json_monitors_ptr[mon_row].json_info_ptr->args, prgrm_args);

          json_monitors_ptr[mon_row].json_info_ptr->any_server=true;
        }
        MALLOC_AND_COPY(json_info_ptr->tier_name, json_monitors_ptr[mon_row].json_info_ptr->tier_name, strlen(json_info_ptr->tier_name) + 1,
                                                                                                    "JSON INFO ptr g_mon_id", -1);
        json_monitors_ptr[mon_row].json_info_ptr->tier_idx=json_info_ptr->tier_idx;
      
        MALLOC_AND_COPY(topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_disp_name ,json_monitors_ptr[mon_row].json_info_ptr->server_name,(strlen(topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_disp_name) + 1), "Copy tier name", -1);

        json_monitors_ptr[mon_row].json_info_ptr->server_index=server_index;
      }
      else
      {
        strcpy(temp_monitor_buf,monitor_buf);
        if(json_info_ptr && tmp_prgrm_args)
        {
          MY_MALLOC_AND_MEMSET(json_info_ptr->args, strlen(tmp_prgrm_args), "JSON Info Ptr", -1);
          strcpy(json_info_ptr->args, tmp_prgrm_args);
        }
        if((mon_ret = kw_set_server_signature("SERVER_SIGNATURE", monitor_buf, process_diff_json, err_msg, json_info_ptr)) < 0 )
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                                        "Failed to apply json auto monitor with buffer: %s . Error: %s", temp_monitor_buf, err_msg);
      }
      if(topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_ptr->status == 1)
        topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_ptr->auto_monitor_applied = 1;
    }

    else if (mon_type == LOG_MON_TYPE)
    {
      if((instance_name) && (instance_name[0] != '\0'))
        sprintf(monitor_buf,"LOG_MONITOR %s%c%s %s %s%c%s%c%s %s",topo_info[topo_idx].topo_tier_info[tier_id].tier_name,global_settings->hierarchical_view_vector_separator, topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_ptr->server_disp_name, gdf_name,topo_info[topo_idx].topo_tier_info[tier_id].tier_name, global_settings->hierarchical_view_vector_separator,topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_disp_name,global_settings->hierarchical_view_vector_separator,instance_name, prgrm_args);
      else
        sprintf(monitor_buf,"LOG_MONITOR %s%c%s %s %s%c%s%c%s %s",topo_info[topo_idx].topo_tier_info[tier_id].tier_name,global_settings->hierarchical_view_vector_separator,topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_ptr->server_disp_name, gdf_name, topo_info[topo_idx].topo_tier_info[tier_id].tier_name ,global_settings->hierarchical_view_vector_separator, topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_disp_name,global_settings->hierarchical_view_vector_separator,mon_name, prgrm_args);

      if(process_diff_json)
      {
        //This will allocate from start everytime once it get free 
        create_table_entry_dvm(&mon_row, &total_json_monitors, &max_json_monitors, (char **)&json_monitors_ptr, sizeof(Mon_from_json),
                               "log monitor to be Applied");

        MY_MALLOC(json_monitors_ptr[mon_row].mon_buff, strlen(monitor_buf) + 1, "log monitor buffer", mon_row);
        strcpy(json_monitors_ptr[mon_row].mon_buff, monitor_buf);
        json_monitors_ptr[mon_row].mon_type = LOG_MONITOR;
        MY_MALLOC_AND_MEMSET(json_monitors_ptr[mon_row].json_info_ptr, sizeof(JSON_info), "JSON Info Ptr", -1);
        if(any_found)
        {
          //MY_MALLOC_AND_MEMSET(json_monitors_ptr[mon_row].json_info_ptr, sizeof(JSON_info), "JSON Info Ptr", -1);
          json_monitors_ptr[mon_row].json_info_ptr->any_server=true;
        }
        json_monitors_ptr[mon_row].json_info_ptr->frequency = json_info_ptr->frequency;
        json_monitors_ptr[mon_row].json_info_ptr->sm_mode = json_info_ptr->sm_mode;
        json_monitors_ptr[mon_row].json_info_ptr->lps_enable = json_info_ptr->lps_enable;
        // this is for monconfig struct
        if(json_info_ptr->g_mon_id)
        {
          MALLOC_AND_COPY(json_info_ptr->g_mon_id, json_monitors_ptr[mon_row].json_info_ptr->g_mon_id, strlen(json_info_ptr->g_mon_id) + 1,
                                        "JSON INFO ptr g_mon_id", -1);
        }
        json_monitors_ptr[mon_row].json_info_ptr->mon_info_index = json_info_ptr->mon_info_index;
        MALLOC_AND_COPY(json_info_ptr->tier_name, json_monitors_ptr[mon_row].json_info_ptr->tier_name, strlen(json_info_ptr->tier_name) + 1,
                                                                                                    "JSON INFO ptr g_mon_id", -1);
        json_monitors_ptr[mon_row].json_info_ptr->tier_idx=json_info_ptr->tier_idx;
      
        MALLOC_AND_COPY(topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_disp_name ,json_monitors_ptr[mon_row].json_info_ptr->server_name,(strlen(topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_disp_name) + 1), "Copy tier name", -1);

        json_monitors_ptr[mon_row].json_info_ptr->server_index=server_index;

      }
      else
      {
        strcpy(temp_monitor_buf,monitor_buf);

        if((mon_ret = custom_monitor_config("LOG_MONITOR",monitor_buf, NULL, 0, process_diff_json, err_msg, NULL, json_info_ptr, 0)) >= 0)
        {
          g_mon_id = get_next_mon_id();
          monitor_added_on_runtime = 1;
        }
      }
      if(topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_ptr->status ==1)
        topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_ptr->auto_monitor_applied = 1;
    }
   
    else if((mon_type == CUSTOM_GDF_MON_TYPE))
    {
      if(strchr(prgrm_args, '%'))
      {
        char *variable_ptr = NULL;
        char leftover_arguments[MAX_DATA_LINE_LENGTH];
 
        if((variable_ptr=strstr(prgrm_args, "%server_ip%")))
        {
          strcpy(leftover_arguments, variable_ptr + 11);
          strcpy(variable_ptr,topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_ptr->server_name);
          strcpy(variable_ptr + strlen(topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_ptr->server_name), leftover_arguments);
        }
        if((variable_ptr=strstr(prgrm_args, "%server_name%")))
        {
          strcpy(leftover_arguments, variable_ptr + 13);
          strcpy(variable_ptr,topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_ptr->server_disp_name);
          strcpy(variable_ptr + strlen(topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_ptr->server_disp_name), leftover_arguments);
        }
      }
      //This check is for supporting os-type Linux over LinuxEx, as we are comparing we are comapring machine_type with LinuxEx as default.   
      if((!strncasecmp(topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_ptr->machine_type,json_info_ptr->os_type,5))          || (strcasecmp(json_info_ptr->os_type, "ALL") == 0)) {
        //checking java path path only when the program type is java  
        if(!strcasecmp(json_info_ptr->pgm_type, "JAVA") )
        {
          if(json_info_ptr && json_info_ptr->javaClassPath && json_info_ptr->javaClassPath[0] != '\0')
          {
            if(json_info_ptr->javaHome && json_info_ptr->javaHome[0] != '\0')
            {
              if(json_info_ptr->javaHome[strlen(json_info_ptr->javaHome) - 1] == '/' || 
                 json_info_ptr->javaHome[strlen(json_info_ptr->javaHome) - 1] == '\\')
              {
                json_info_ptr->javaHome[strlen(json_info_ptr->javaHome) - 1] = '\0';
              }

              if(strcasecmp(topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_ptr->machine_type, "Windows"))
              {
                sprintf(java_home_and_classpath,"%s/bin/java -cp %s",json_info_ptr->javaHome, json_info_ptr->javaClassPath);
              }
              else
                sprintf(java_home_and_classpath,"%s\\bin\\java -cp %s", json_info_ptr->javaHome, json_info_ptr->javaClassPath);
            }
            else
              sprintf(java_home_and_classpath,"java -cp %s", json_info_ptr->javaClassPath);
          }
          else
            strcpy(java_home_and_classpath, "java");
        }
        NSDL2_MON(NULL, NULL, "java_home_and_classpath=[%s] ",java_home_and_classpath);

        if((instance_name) && (instance_name[0] != '\0'))
        {
          sprintf(vector_name,"%s%c%s%c%s",topo_info[topo_idx].topo_tier_info[tier_id].tier_name,global_settings->hierarchical_view_vector_separator,topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_ptr->server_disp_name,global_settings->hierarchical_view_vector_separator, instance_name);
        }
        else
        {
          sprintf(vector_name,"%s%c%s",topo_info[topo_idx].topo_tier_info[tier_id].tier_name,global_settings->hierarchical_view_vector_separator,topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_disp_name);
        }
        sprintf(java_args, "%s -DCAV_MON_HOME=%s -DMON_TEST_RUN=%d -DVECTOR_NAME=%s ",java_home_and_classpath,
                                              topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_ptr->cav_mon_home, 
                                              testidx, 
                                              vector_name);

        if(strcasecmp(json_info_ptr->pgm_type, "java") == 0)
        {
          if ((json_info_ptr->use_agent) && strcasecmp(json_info_ptr->use_agent, "local") == 0)
          {
            sprintf(dyn_mon_buf, "DYNAMIC_VECTOR_MONITOR Cavisson%c%s %s %s %d %s %s %s %s EOC %s %s %s %s", global_settings->hierarchical_view_vector_separator, g_machine, vector_name, gdf_name,json_info_ptr->run_opt, java_args, json_info_ptr->mon_name, prgrm_args, dvm_data_arg_buf, java_args, mon_name,json_info_ptr->args, dvm_data_arg_buf );
          }
          else
          {
            sprintf(dyn_mon_buf, "DYNAMIC_VECTOR_MONITOR %s%c%s %s %s %d %s %s %s %s EOC %s %s %s %s",topo_info[topo_idx].topo_tier_info[tier_id].tier_name,global_settings->hierarchical_view_vector_separator,topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_ptr->server_disp_name,vector_name ,gdf_name,json_info_ptr->run_opt, java_args, json_info_ptr->mon_name,prgrm_args,dvm_data_arg_buf, java_args, mon_name,json_info_ptr->args, dvm_data_arg_buf);
          }
        }
        else
        {
          if ((json_info_ptr->use_agent) && strcasecmp(json_info_ptr->use_agent, "local") == 0)
          {
            sprintf(dyn_mon_buf, "DYNAMIC_VECTOR_MONITOR Cavisson%c%s %s %s %d %s %s %s EOC %s %s", global_settings->hierarchical_view_vector_separator, g_machine,vector_name,gdf_name, json_info_ptr->run_opt,json_info_ptr->mon_name,prgrm_args, dvm_data_arg_buf, mon_name, dvm_vector_arg_buf);
          }
          else
          {
            sprintf(dyn_mon_buf, "DYNAMIC_VECTOR_MONITOR %s%c%s %s %s %d %s %s %s EOC %s %s",
             topo_info[topo_idx].topo_tier_info[tier_id].tier_name,global_settings->hierarchical_view_vector_separator,topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_ptr->server_disp_name,vector_name, gdf_name, json_info_ptr->run_opt, json_info_ptr->mon_name,prgrm_args, dvm_data_arg_buf, mon_name, dvm_vector_arg_buf);
          }
        }
 
        NSDL2_MON(NULL, NULL, "dyn_mon_buf= %s, server_name = %s", dyn_mon_buf, 
                                         topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_ptr->server_name);
 
        if(process_diff_json)
        {
          //This will allocate from start everytime once it get free 
          create_table_entry_dvm(&mon_row, &total_json_monitors, &max_json_monitors, (char **)&json_monitors_ptr, sizeof(Mon_from_json),
                                 "log monitor to be Applied");
  
          MY_MALLOC(json_monitors_ptr[mon_row].mon_buff, strlen(dyn_mon_buf) + 1, "log monitor buffer", mon_row);
          strcpy(json_monitors_ptr[mon_row].mon_buff, dyn_mon_buf);
          json_monitors_ptr[mon_row].mon_type = mon_type;
          //json_monitors_ptr[mon_row].json_info_ptr = json_info_ptr; 
          MY_MALLOC_AND_MEMSET(json_monitors_ptr[mon_row].json_info_ptr, sizeof(JSON_info), "JSON Info Ptr", -1);
          json_monitors_ptr[mon_row].json_info_ptr->frequency = json_info_ptr->frequency;
          json_monitors_ptr[mon_row].json_info_ptr->is_process = json_info_ptr->is_process;
          json_monitors_ptr[mon_row].json_info_ptr->sm_mode = json_info_ptr->sm_mode;
          if(any_found)
          {
            json_monitors_ptr[mon_row].json_info_ptr->any_server=true;
          }
          if(json_info_ptr)
          {
            json_monitors_ptr[mon_row].json_info_ptr->agent_type = json_info_ptr->agent_type;

            if(json_info_ptr->javaHome)
            {
              MALLOC_AND_COPY(json_info_ptr->javaHome ,json_monitors_ptr[mon_row].json_info_ptr->javaHome ,(strlen(json_info_ptr->javaHome)+ 1),                               "JSON INFO ptr java home", -1);   
            }
            if(json_info_ptr->javaClassPath)
            {
              MALLOC_AND_COPY(json_info_ptr->javaClassPath , json_monitors_ptr[mon_row].json_info_ptr->javaClassPath ,
                              (strlen(json_info_ptr->javaClassPath)+ 1) , "JSON INFO ptr java class path", -1); 
            }
            if (json_info_ptr->init_vector_file)
            {
              MY_MALLOC_AND_MEMSET(json_monitors_ptr[mon_row].json_info_ptr->init_vector_file, (strlen(json_info_ptr->init_vector_file)+ 1),
                                   "JSON Info Ptr init vector file", -1);
              strcpy(json_monitors_ptr[mon_row].json_info_ptr->init_vector_file, json_info_ptr->init_vector_file);
            }
            if(json_info_ptr->use_agent)
            {
              MALLOC_AND_COPY(json_info_ptr->use_agent ,json_monitors_ptr[mon_row].json_info_ptr->use_agent ,
                              (strlen(json_info_ptr->use_agent)+ 1) , "JSON INFO ptr use_agent", -1);
            }
            if(json_info_ptr->pgm_type)
            {
              MALLOC_AND_COPY(json_info_ptr->pgm_type ,json_monitors_ptr[mon_row].json_info_ptr->pgm_type ,(strlen(json_info_ptr->pgm_type)+ 1) ,                              "JSON INFO ptr program type", -1);
            }
            if(json_info_ptr->app_name)
            {
              MALLOC_AND_COPY(json_info_ptr->app_name ,json_monitors_ptr[mon_row].json_info_ptr->app_name ,(strlen(json_info_ptr->app_name)+ 1) ,                              "JSON INFO ptr appname", -1);
            }
            if(json_info_ptr->config_json)
            {
              MALLOC_AND_COPY(json_info_ptr->config_json , json_monitors_ptr[mon_row].json_info_ptr->config_json ,
              (strlen(json_info_ptr->config_json)+ 1) , "JSON INFO ptr cfg", -1);
            }
            if(json_info_ptr->os_type)
            {
              MALLOC_AND_COPY(json_info_ptr->os_type , json_monitors_ptr[mon_row].json_info_ptr->os_type ,(strlen(json_info_ptr->os_type)+ 1) , 
                              "JSON INFO ptr os_type", -1);
            }
            if(json_info_ptr->mon_name)
            {
              MALLOC_AND_COPY(json_info_ptr->mon_name ,json_monitors_ptr[mon_row].json_info_ptr->mon_name ,(strlen(json_info_ptr->mon_name)+ 1),
                              "JSON INFO ptr mon_name", -1);
            }
            if(json_info_ptr->args)
            {
              MALLOC_AND_COPY(json_info_ptr->args , json_monitors_ptr[mon_row].json_info_ptr->args ,(strlen(json_info_ptr->args)+ 1) , 
              "JSON INFO ptr arguments", -1);
            }
            json_monitors_ptr[mon_row].json_info_ptr->skip_breadcrumb_creation = json_info_ptr->skip_breadcrumb_creation;
          
            json_monitors_ptr[mon_row].json_info_ptr->run_opt = json_info_ptr->run_opt;

            // this is for monconfig struct
            if(json_info_ptr->g_mon_id)
            {
              MALLOC_AND_COPY(json_info_ptr->g_mon_id, json_monitors_ptr[mon_row].json_info_ptr->g_mon_id, strlen(json_info_ptr->g_mon_id) + 1,
                                        "JSON INFO ptr g_mon_id", -1);
            }
            json_monitors_ptr[mon_row].json_info_ptr->mon_info_index = json_info_ptr->mon_info_index;
            MALLOC_AND_COPY(json_info_ptr->tier_name, json_monitors_ptr[mon_row].json_info_ptr->tier_name, 
                                                                           strlen(json_info_ptr->tier_name)+ 1,"JSON INFO ptr g_mon_id", -1);
            json_monitors_ptr[mon_row].json_info_ptr->tier_idx=json_info_ptr->tier_idx;
           
            MALLOC_AND_COPY(topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_disp_name ,
                                            json_monitors_ptr[mon_row].json_info_ptr->server_name,(strlen(topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_disp_name) + 1), "Copy tier name", -1);

            json_monitors_ptr[mon_row].json_info_ptr->server_index=server_index;


          }

        }
         else
         {
           if(json_info_ptr && json_info_ptr->init_vector_file && json_info_ptr->init_vector_file[0] != '\0')
           {
             kw_set_dynamic_vector_monitor("DYNAMIC_VECTOR_MONITOR", dyn_mon_buf, topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_ptr->server_name, 0,process_diff_json, NULL, err_msg, json_info_ptr->init_vector_file, json_info_ptr,json_info_ptr->skip_breadcrumb_creation);
           } 
           else
           {
             kw_set_dynamic_vector_monitor("DYNAMIC_VECTOR_MONITOR", 
                              dyn_mon_buf, topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_ptr->server_name, 0,
                              process_diff_json, NULL, err_msg, NULL, json_info_ptr, json_info_ptr->skip_breadcrumb_creation);
           }
           monitor_added_on_runtime = 1;
         }
       }
       else
       {
         MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                               "Failed to apply json CUSTOM MONITOR as %s and %s doesn't match.",
                     topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_ptr->machine_type,
                     json_info_ptr->os_type);
       }
     
     if(topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_ptr->status ==1)
        topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_ptr->auto_monitor_applied = 1;

     } 
     
     else if (mon_type == STD_MON_TYPE)
     {
       //this is done for all type of monitor
       int ret =0;
       char (*monitor_buf_arr)[32*MAX_DATA_LINE_LENGTH];
       char (*instance_name_arr)[64] =NULL;

       if(strchr(prgrm_args, '%'))
       {
         char *variable_ptr = NULL;
         char leftover_arguments[MAX_DATA_LINE_LENGTH];

         if((variable_ptr=strstr(prgrm_args, "%server_ip%")))
         {
           strcpy(leftover_arguments, variable_ptr + 11);
           strcpy(variable_ptr,topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_ptr->server_name);
           strcpy(variable_ptr + strlen(topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_ptr->server_name), leftover_arguments);
         }
         if((variable_ptr=strstr(prgrm_args, "%server_name%")))
         {
           strcpy(leftover_arguments, variable_ptr + 13);
           strcpy(variable_ptr,topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_disp_name);
           strcpy(variable_ptr + strlen(topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_disp_name), leftover_arguments);
         }
         if(json_info_ptr->dest_any_server_flag)
         { 
           MY_MALLOC(monitor_buf_arr, sizeof(*monitor_buf_arr) * json_info_ptr->no_of_dest_any_server_elements, "malloc monitor buffer", 0);
           MY_MALLOC(instance_name_arr, sizeof(*instance_name_arr) * json_info_ptr->no_of_dest_any_server_elements, 
                     "malloc instance name array", 0);

           //here j is server idx of source ip 
           mon_buf_count = create_mon_buf_arr(json_info_ptr, tier_id, server_index, prgrm_args, monitor_buf_arr, mon_name, gdf_name,
                                              instance_name_arr );
           ret = 1;
         }
       }
       if(ret == 0)
       { 
         MY_MALLOC( monitor_buf_arr , sizeof(*monitor_buf_arr) * 1, "malloc monitor buffer", -1);
         mon_buf_count=1;
         if(enable_store_config && !strcmp(topo_info[topo_idx].topo_tier_info[tier_id].tier_name, "Cavisson"))
           sprintf(tmp_buf, "%s!%s", g_store_machine, topo_info[topo_idx].topo_tier_info[tier_id].tier_name);
         else
           sprintf(tmp_buf, "%s", topo_info[topo_idx].topo_tier_info[tier_id].tier_name);
         // This check is for those in vector name is comes i.e kubernetes monitors
         if(kub_instance_name[0] == '\0')
         {
           if((instance_name) && (instance_name[0] != '\0'))
             sprintf(monitor_buf_arr[0],"STANDARD_MONITOR %s%c%s %s%c%s%c%s %s %s",
             topo_info[topo_idx].topo_tier_info[tier_id].tier_name,global_settings->hierarchical_view_vector_separator,topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_disp_name, 
             tmp_buf,global_settings->hierarchical_view_vector_separator, topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_ptr->server_disp_name,global_settings->hierarchical_view_vector_separator, instance_name, 
             mon_name, prgrm_args);
           else
             sprintf(monitor_buf_arr[0],"STANDARD_MONITOR %s%c%s %s%c%s %s %s", topo_info[topo_idx].topo_tier_info[tier_id].tier_name,global_settings->hierarchical_view_vector_separator, topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_disp_name, tmp_buf,global_settings->hierarchical_view_vector_separator, topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_disp_name, mon_name, prgrm_args);
         }
         else
         {
           if((instance_name) && (instance_name[0] != '\0'))
             sprintf(monitor_buf_arr[0],"STANDARD_MONITOR %s%c%s %s%c%s%c%s%c%s %s %s", topo_info[topo_idx].topo_tier_info[tier_id].tier_name,
global_settings->hierarchical_view_vector_separator, topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_disp_name, 
             tmp_buf,global_settings->hierarchical_view_vector_separator, topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_ptr->server_disp_name,global_settings->hierarchical_view_vector_separator, kub_instance_name,global_settings->hierarchical_view_vector_separator, instance_name, 
             mon_name, prgrm_args);
           else
             sprintf(monitor_buf_arr[0],"STANDARD_MONITOR %s%c%s %s%c%s%c%s %s %s",topo_info[topo_idx].topo_tier_info[tier_id].tier_name,global_settings->hierarchical_view_vector_separator, topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_disp_name, tmp_buf,global_settings->hierarchical_view_vector_separator, topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_disp_name,
global_settings->hierarchical_view_vector_separator, kub_instance_name, mon_name, prgrm_args);
           
         }
           
          //pointer_monitor_buf with monitor buffer array used in  destination tier 
          //monitor_buf = STANDARD_MONITOR TIER>Server_72 TIER>Server_72 UpTime
        }

        if(json_info_ptr)
        {
          if(json_info_ptr->json_mp_ptr)
          {
            for(i=0; i< json_info_ptr->total_mp_enteries; i++)
            {
              if(!strcmp(topo_info[topo_idx].topo_tier_info[tier_id].tier_name, json_info_ptr->json_mp_ptr[i].tier))
              {
                if(json_info_ptr->json_mp_ptr[i].server_mp_ptr == NULL)
                  json_info_ptr->metric_priority = json_mp_ptr[i].mp;
                else
                {
                  for(k=0 ; k<json_info_ptr->json_mp_ptr[i].total_server_entries ; k++)
                  {
                    if(!strcmp(topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_disp_name,
                               json_info_ptr->json_mp_ptr[i].server_mp_ptr[k].server))
                    {
                      json_info_ptr->metric_priority = json_mp_ptr[i].server_mp_ptr[k].mp;
                    }
                  }  
                }
              }
            }
          } 
          else
            json_info_ptr->metric_priority = g_metric_priority;
        }
        for (mon_buf_id=0; mon_buf_id < mon_buf_count ; mon_buf_id++)
        {
          //monitor buf array loop
          if(process_diff_json)
          {
            //This will allocate from start everytime once it get free 
            create_table_entry_dvm(&mon_row, &total_json_monitors, &max_json_monitors, (char **)&json_monitors_ptr, sizeof(Mon_from_json), 
                                   "Standard Monitors to be Applied");
            MY_MALLOC(json_monitors_ptr[mon_row].mon_buff, strlen(monitor_buf_arr[mon_buf_id]) + 1, "Standard monitor buffer", mon_row);
            strcpy(json_monitors_ptr[mon_row].mon_buff, monitor_buf_arr[mon_buf_id]);
            json_monitors_ptr[mon_row].mon_type = STANDARD_MONITOR;
            MY_MALLOC(json_monitors_ptr[mon_row].pod_name, strlen(pod_name) + 1, "Pod Name", mon_row);
            strcpy(json_monitors_ptr[mon_row].pod_name, pod_name);
            MY_MALLOC_AND_MEMSET(json_monitors_ptr[mon_row].json_info_ptr, sizeof(JSON_info), "JSON Info Ptr", -1);
            json_monitors_ptr[mon_row].json_info_ptr->is_process = json_info_ptr->is_process;
            json_monitors_ptr[mon_row].json_info_ptr->frequency = json_info_ptr->frequency;
            json_monitors_ptr[mon_row].json_info_ptr->sm_mode = json_info_ptr->sm_mode;
            if(json_info_ptr)
            {
              // this is for monconfig struct
              if(json_info_ptr->g_mon_id)
              {
                MALLOC_AND_COPY(json_info_ptr->g_mon_id, json_monitors_ptr[mon_row].json_info_ptr->g_mon_id, strlen(json_info_ptr->g_mon_id) + 1,
                                        "JSON INFO ptr g_mon_id", -1);
              }
              json_monitors_ptr[mon_row].json_info_ptr->mon_info_index = json_info_ptr->mon_info_index;

              if(json_info_ptr->vectorReplaceTo)
              {
                MY_MALLOC_AND_MEMSET(json_monitors_ptr[mon_row].json_info_ptr->vectorReplaceTo, (strlen(json_info_ptr->vectorReplaceTo)+ 1), 
                                     "JSON Info Ptr VectorReplaceTo", -1);
                strcpy(json_monitors_ptr[mon_row].json_info_ptr->vectorReplaceTo, json_info_ptr->vectorReplaceTo);
              }
              if(json_info_ptr->vectorReplaceFrom)
              {
                MY_MALLOC_AND_MEMSET(json_monitors_ptr[mon_row].json_info_ptr->vectorReplaceFrom, (strlen(json_info_ptr->vectorReplaceFrom)+ 1),                                      "JSON Info Ptr VectorReplaceFrom", -1);
  
                strcpy(json_monitors_ptr[mon_row].json_info_ptr->vectorReplaceFrom, json_info_ptr->vectorReplaceFrom);
              }
  
              json_monitors_ptr[mon_row].json_info_ptr->tier_server_mapping_type = json_info_ptr->tier_server_mapping_type;
              json_monitors_ptr[mon_row].json_info_ptr->metric_priority = json_info_ptr->metric_priority;
  
              if(json_info_ptr->javaHome)
              {
                MY_MALLOC_AND_MEMSET(json_monitors_ptr[mon_row].json_info_ptr->javaHome, (strlen(json_info_ptr->javaHome)+ 1), 
                                     "JSON Info Ptr JavaHome", -1);
                strcpy(json_monitors_ptr[mon_row].json_info_ptr->javaHome, json_info_ptr->javaHome);
              }
              if(json_info_ptr->javaClassPath)
              {
                MY_MALLOC_AND_MEMSET(json_monitors_ptr[mon_row].json_info_ptr->javaClassPath, (strlen(json_info_ptr->javaClassPath)+ 1), 
                                     "JSON Info Ptr javaClassPath", -1);
  
                strcpy(json_monitors_ptr[mon_row].json_info_ptr->javaClassPath, json_info_ptr->javaClassPath);
              }
              if(json_info_ptr->init_vector_file)
              {
                MY_MALLOC_AND_MEMSET(json_monitors_ptr[mon_row].json_info_ptr->init_vector_file, (strlen(json_info_ptr->init_vector_file)+ 1),
                                     "JSON Info Ptr init vector file", -1);
                strcpy(json_monitors_ptr[mon_row].json_info_ptr->init_vector_file, json_info_ptr->init_vector_file);
              }
              if(json_info_ptr->config_json)
              {
                MALLOC_AND_COPY(json_info_ptr->config_json, json_monitors_ptr[mon_row].json_info_ptr->config_json, 
                                strlen(json_info_ptr->config_json)+1, "JSON Info config json", -1);
              }
              if(json_info_ptr->mon_name)
              {
                MALLOC_AND_COPY(json_info_ptr->mon_name, json_monitors_ptr[mon_row].json_info_ptr->mon_name, 
                                strlen(json_info_ptr->mon_name)+1, "JSON Info config json", -1);
              }
              json_monitors_ptr[mon_row].json_info_ptr->agent_type = json_info_ptr->agent_type;
              json_monitors_ptr[mon_row].json_info_ptr->any_server = json_info_ptr->any_server;
              json_monitors_ptr[mon_row].json_info_ptr->dest_any_server_flag = json_info_ptr->dest_any_server_flag;  //for destination tier 
              
              if(json_info_ptr->json_mp_ptr)
              {
                MY_MALLOC_AND_MEMSET(json_monitors_ptr[mon_row].json_info_ptr->json_mp_ptr, 
                                     json_info_ptr->total_mp_enteries * sizeof(JSON_MP), "JSON Info Ptr init json_mp_ptr", -1);
                for(i=0; i< json_info_ptr->total_mp_enteries; i++)
                { 
                  MALLOC_AND_COPY(json_info_ptr->json_mp_ptr[i].tier, json_monitors_ptr[mon_row].json_info_ptr->json_mp_ptr[i].tier,
                                  strlen(json_info_ptr->json_mp_ptr[i].tier), "copying tier from overrride MP", -1);
                  MALLOC_AND_COPY(json_info_ptr->json_mp_ptr[i].server_mp_ptr[j].server, 
                                  json_monitors_ptr[mon_row].json_info_ptr->json_mp_ptr[i].server_mp_ptr[j].server, 
                                  strlen(json_info_ptr->json_mp_ptr[i].server_mp_ptr[j].server), "copying sever from overrride MP", -1);

                  json_monitors_ptr[mon_row].json_info_ptr->json_mp_ptr[i].mp = json_info_ptr->json_mp_ptr[i].mp;
                }
              }
             
            MALLOC_AND_COPY(json_info_ptr->tier_name, json_monitors_ptr[mon_row].json_info_ptr->tier_name, strlen(json_info_ptr->tier_name)+1,                                        "JSON INFO ptr g_mon_id", -1);
            json_monitors_ptr[mon_row].json_info_ptr->tier_idx=json_info_ptr->tier_idx;
            
            MALLOC_AND_COPY(json_info_ptr->server_name ,json_monitors_ptr[mon_row].json_info_ptr->server_name,(strlen(json_info_ptr->server_name) + 1), "Copy tier name", -1);

            json_monitors_ptr[mon_row].json_info_ptr->server_index=server_index;

            }
          
          }
          else
          {
            strcpy(temp_monitor_buf,monitor_buf_arr[mon_buf_id]);
            //this is done for destination tier name for multiple instances name
            if ( ret == 1 )
            {
              REALLOC_AND_COPY(instance_name_arr[mon_buf_id] ,json_info_ptr->instance_name,strlen(instance_name_arr[mon_buf_id]) + 1,
                               "Copying destination server ip for instance",0);
            }
            if((mon_ret = kw_set_standard_monitor("STANDARD_MONITOR", monitor_buf_arr[mon_buf_id], process_diff_json, pod_name, err_msg,
                                                  json_info_ptr)) < 0 )
            {
              MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                                 "Failed to apply json auto monitor with buffer: %s . Error: %s", temp_monitor_buf, err_msg);
            }
          }
          if(topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_ptr->status == 1)
            topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_ptr->auto_monitor_applied = 1;
        }
        ret =0;
        FREE_AND_MAKE_NULL( monitor_buf_arr ,"Freeing monitor buf arr", -1); //free 2d array   
        FREE_AND_MAKE_NULL( instance_name_arr ,"Freeing instance name buf arr", -1); //free 2d array
      }

      else if((mon_type == DELETE_STD_MON_TYPE) || (mon_type == DELETE_CUSTOM_GDF))
      {
        int ret = 0;
        err_msg[0] = '\0';
        StdMonitor *std_mon_ptr = NULL;
        char fname[MAX_LINE_LENGTH];
        char (*monitor_buf_arr)[32*MAX_DATA_LINE_LENGTH];
        
        //DELETE_MONITOR GroupMon 10108 NA Cavisson>NDAppliance>Instance1
        if(mon_type == DELETE_STD_MON_TYPE)
        {
          if((std_mon_ptr = get_standard_mon_entry(mon_name, topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_ptr->machine_type, process_diff_json, err_msg)) == NULL)
          {
            sprintf(err_msg, "Error: Standard Monitor Name '%s' or server type '%s' is incorrect\n", mon_name, 
                             topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_ptr->machine_type);
            strcat(err_msg, "Please provide valid standard monitor name\n");
            strcat(err_msg, "Error in deleting monitor due to above error.\n");
            MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error %s",err_msg);
            err_msg[0]='\0';
            return -1;
          }
          strcpy(tmp_buf, std_mon_ptr->gdf_name);
        }
        else
          strcpy(tmp_buf, gdf_name);

        if(strncmp(tmp_buf,"NA_",3) != 0)
        {
          sprintf(fname, "%s/sys/%s", g_ns_wdir, tmp_buf);
          get_group_id_from_gdf_name(fname, group_id);
        }
        else
        {
          strcpy(group_id, "-1");
        }     
        
        if(json_info_ptr->dest_any_server_flag && (total_dest_tier >0))
        {  
          MY_MALLOC( monitor_buf_arr , sizeof(*monitor_buf_arr) *json_info_ptr->no_of_dest_any_server_elements , "malloc monitor buffer", -1);

          mon_buf_count =delete_entry_from_dest_tier_info_struct(json_info_ptr, server_index,tier_id ,tmp_buf ,monitor_buf_arr, group_id,fname, mon_name);
          ret = 1;
        }

        if (ret ==0)
        {
          MY_MALLOC( monitor_buf_arr , sizeof(*monitor_buf_arr) * 1, "malloc monitor buffer", -1);
          mon_buf_count = 1;
     
          if((instance_name) && (instance_name[0] != '\0'))
            sprintf(monitor_buf_arr[0],"DELETE_MONITOR GroupMon %s NA %s%c%s%c%s %s", group_id, topo_info[topo_idx].topo_tier_info[tier_id].tier_name,global_settings->hierarchical_view_vector_separator, topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_disp_name,global_settings->hierarchical_view_vector_separator, instance_name, tmp_buf);
          else
            sprintf(monitor_buf_arr[0],"DELETE_MONITOR GroupMon %s NA %s%c%s %s", group_id, topo_info[topo_idx].topo_tier_info[tier_id].tier_name,global_settings->hierarchical_view_vector_separator, topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_disp_name, tmp_buf);           
          //point monitor buf to monitor_buf_arr
        }

        for(mon_buf_id=0;mon_buf_id < mon_buf_count; mon_buf_id++)
        {     
          ret = kw_set_runtime_process_deleted_monitors(monitor_buf_arr[mon_buf_id], err_msg);
     
          if(ret == 0) //atleast one monitor parsing successfull
          {
            monitor_runtime_changes_applied = 1;  //set in both the cases 'ADD/DELETE'
            monitor_deleted_on_runtime = 2;  //set in both the cases 'ADD/DELETE'
          }
     
          if(err_msg != '\0')
            MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error %s",err_msg);
        }
        FREE_AND_MAKE_NULL( monitor_buf_arr ,"Freeing monitor buf arr", -1); //free 2d array
      }
      
      //here we are deleting check monitors and batch job monitors 
      else if((mon_type == DELETE_CHECK_MON_TYPE) || (mon_type == DELETE_BATCH_JOB_TYPE))
      {
        int ret =0;

        sprintf(monitor_buf,"%s%c%s%c%s", topo_info[topo_idx].topo_tier_info[tier_id].tier_name, global_settings->hierarchical_view_vector_separator,topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_index].server_disp_name, global_settings->hierarchical_view_vector_separator, mon_name);

        ret = kw_set_runtime_delete_check_mon(monitor_buf);
        
        if (mon_type ==DELETE_BATCH_JOB_TYPE)
          strcpy(monitor_type_buf,"Batch Job Monitor");

        if(ret ==0)
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "%s with vector name '%s' deleted successfully.",monitor_type_buf,monitor_buf);
        }
        else
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "%s with vector name '%s' not deleted successfully",monitor_type_buf,monitor_buf);
        }
     }
  }
  return mon_ret;
}

/****************CREATE_MON_BUF*******************************/
/*
 *Below function will make buffer for STANDARD_MONITOR, CHECK_MONITOR, SERVER_SIGNATURE and DELETE_MONITOR
 *At start of the test it will apply monitor from here only but at runtime buffers will be saved and applied later.
 *Delete Monitor will be called from here only irrespective of runtime
*/

int create_mon_buf(char *exclude_server_arr[], int no_of_exclude_server_elements, char *specific_server_arr[], 
                   int no_of_specific_server_elements, char *mon_name, char *tmp_prgrm_args, char *instance_name, JSON_info *json_info_ptr,
                   int process_diff_json, char mon_type, int tier_id, char *gdf_name, JSON_MP *json_mp_ptr, int server_idx, 
                   char *kub_vector_name, char *app_name)
{
  int j=0;
  bool any_found=false; 
  bool is_json_malloc=false;
  bool server_found=false;
  int  mon_ret=-1;
  NSDL2_MON(NULL, NULL, "Method called, mon_name = %s, tier_id = %d", mon_name, tier_id);
 
  //checking if the specific server have "Any" for any first server which is active 
  if(no_of_specific_server_elements && !strcasecmp(specific_server_arr[0],"Any"))
  {   
    any_found = true;                      //if found "Any" in specific server

    if(json_info_ptr != NULL) 
      json_info_ptr->any_server=true;
    else if(!process_diff_json)
    {
      MY_MALLOC_AND_MEMSET(json_info_ptr, sizeof(JSON_info), "JSON Info Ptr", -1);
      is_json_malloc = true;              //setting if malloc done here to free it in last
      json_info_ptr->any_server = true; 
    } 
  }
  else
  {
     if(json_info_ptr != NULL)
       json_info_ptr->any_server=false;
  }  
 
  if(json_info_ptr)
  {
    if((instance_name) && (instance_name[0] != '\0'))
    {
      MY_MALLOC_AND_MEMSET(json_info_ptr->instance_name, strlen(instance_name), "JSON Info Ptr", -1);
      strcpy(json_info_ptr->instance_name, instance_name);
    }
  }
 
  // This is the case when server is auto scaled in case of ENABLE_AUTO_JSON_MONITOR mode 2 is on.
  if((server_idx != -1) && (total_mon_config_list_entries >0))
  {
    create_mon_buf_wrapper(exclude_server_arr, no_of_exclude_server_elements, specific_server_arr, no_of_specific_server_elements, mon_name,
tmp_prgrm_args, instance_name, json_info_ptr, process_diff_json, mon_type, tier_id, gdf_name, json_mp_ptr, server_idx,any_found, kub_vector_name, app_name);
  }
  else
  {
    for(j=0; j < topo_info[topo_idx].topo_tier_info[tier_id].total_server; j++)
    {
      if(topo_info[topo_idx].topo_tier_info[tier_id].topo_server[j].used_row != -1)
      {
        if(server_found) //break the loop if we run the monitor on any server
        break;

        mon_ret =create_mon_buf_wrapper(exclude_server_arr, no_of_exclude_server_elements, specific_server_arr, no_of_specific_server_elements,
                                      mon_name, tmp_prgrm_args, instance_name, json_info_ptr, process_diff_json, mon_type, tier_id, gdf_name,
                                      json_mp_ptr, j , any_found, kub_vector_name, app_name);

        if(any_found) //set the server_found if "Any" found
        { 
          if(mon_ret >= 0 && process_diff_json == 0)
            server_found = true;
          else if(process_diff_json)
            server_found = true;
        }
      }
    }
  }
   
  if(is_json_malloc)
  {
    FREE_AND_MAKE_NULL(json_info_ptr->vectorReplaceFrom, "json_info_ptr->vectorReplaceFrom", j);
    FREE_AND_MAKE_NULL(json_info_ptr->vectorReplaceTo, "json_info_ptr->vectorReplaceTo", j);
    FREE_AND_MAKE_NULL(json_info_ptr->javaClassPath, "json_info_ptr->javaClassPath", j);
    FREE_AND_MAKE_NULL(json_info_ptr->javaHome, "json_info_ptr->javaHome", j);
    FREE_AND_MAKE_NULL(json_info_ptr->init_vector_file, "json_info_ptr->init_vector_file", j);
    FREE_AND_MAKE_NULL(json_info_ptr->instance_name, "json_info_ptr->instance_name", j);
    FREE_AND_MAKE_NULL(json_info_ptr->mon_name, "json_info_ptr->mon_name", j);
    FREE_AND_MAKE_NULL(json_info_ptr->args, "json_info_ptr->args", j);
    FREE_AND_MAKE_NULL(json_info_ptr->config_json, "json_info_ptr->config_json", j);
    FREE_AND_MAKE_NULL(json_info_ptr->os_type, "json_info_ptr->os_type", j);
    FREE_AND_MAKE_NULL(json_info_ptr->app_name, "json_info_ptr->app_name", j);
    FREE_AND_MAKE_NULL(json_info_ptr->pgm_type, "json_info_ptr->pgm_type", j);
    FREE_AND_MAKE_NULL(json_info_ptr->use_agent, "json_info_ptr->use_agent", j);
    FREE_AND_MAKE_NULL(json_info_ptr->tier_name,"json_info_ptr->tier_name",j);
    FREE_AND_MAKE_NULL(json_info_ptr->server_name, "json_info_ptr->server_name",j);
    FREE_AND_MAKE_NULL(json_info_ptr, "json_info_ptr", j);
    
  }
  return 0;
}


/************************LOOP MACRO*****************************/

int check_if_tier_excluded(int no_of_exclude_tier_elements, char *exclude_tier_arr[], char *tier_name)
{
  int et, ret=0;
  for(et=0;et<no_of_exclude_tier_elements;et++)
  {
    if(!strncmp(exclude_tier_arr[et], "$p:", 3))
    {
     if(!nslb_regex_match(tier_name, exclude_tier_arr[et]+3))
      {
        ret = 1;
        break;
      }
    }
    else if((strchr(exclude_tier_arr[et], '*')) || (strchr(exclude_tier_arr[et], '.')) || (strchr(exclude_tier_arr[et], '?')))
    {
      if(!nslb_regex_match(tier_name, exclude_tier_arr[et]))
      {
        ret = 1;
        break;
      }
    }
    else if(!strcmp(tier_name, exclude_tier_arr[et]))
    {
      ret = 1;
      break;
    }
  }
  return ret;
}


void delete_add_monitor_from_json(char *exclude_tier, char *exclude_server, char *specific_server, char *temp_tiername, char *mon_name, char *prgrm_args, char *instance_name, JSON_info *json_info_ptr, int process_diff_json, char mon_type, char *gdf_name, char tier_type, int tier_group_index, JSON_MP *json_mp_ptr)
{
  int no_of_tier_elements=0;
  int no_of_exclude_server_elements = 0;
  char *exclude_server_arr[MAX_NO_OF_APP];
  int no_of_specific_server_elements = 0;
  char *specific_server_arr[MAX_NO_OF_APP];
  int no_of_exclude_tier_elements = 0;
  char *exclude_tier_arr[MAX_NO_OF_APP];
  char *tier_arr[MAX_NO_OF_APP];
  int tier_id;
  int given_tier_id;
  int tid = -1;
  char dummy_ptr[MAX_DATA_LINE_LENGTH];
  char err_msg[MAX_DATA_LINE_LENGTH];    
  int ret = 0;
  StdMonitor *std_mon_ptr = NULL; // added std_mon_ptr in JSON_info can be remove

  NSDL2_MON(NULL, NULL, "Method called, mon_name = %s, temp_tier_name = %s", mon_name, temp_tiername);
      
  no_of_exclude_tier_elements = get_tokens(exclude_tier, exclude_tier_arr, ",", MAX_NO_OF_APP);
  no_of_exclude_server_elements = get_tokens(exclude_server, exclude_server_arr, ",", MAX_NO_OF_APP);
  no_of_specific_server_elements = get_tokens(specific_server, specific_server_arr, ",", MAX_NO_OF_APP);
  

  /* For non MBean monitors, get_standard_mon_entry may return NULL because of "ALL" machine_type 
  and will call get_standard_mon_entry again in kw_set_standard_monitor with correct machine_type.
  So, for non MBean monitors get_standard_mon_entry is called for 1 extra time. Will fix later
  */
  if (mon_type == STD_MON_TYPE || mon_type == DELETE_STD_MON_TYPE)
  {
    if((std_mon_ptr = get_standard_mon_entry(mon_name, "ALL", process_diff_json, err_msg)) == NULL)
    {
      /***
      Here we need std_mon_ptr for MBean type monitors

      But 'get_standard_mon_entry' will be called everytime a standard monitor is applied or deleted.
      Here it is called with "ALL" machine type but there are standard monitors whoose machine type is not "ALL".
      So get_standard_mon_entry will return NULL and get_standard_mon_entry will be called again in kw_set_standard_monitor
      function with correct machine type.

      So commenting the below log message if get_standard_mon_entry returns NULL.
      Fix in future: put proper log message
      ***/

      // sprintf(err_msg, "Warning/Error: Standard Monitor Name '%s' or server type 'ALL' is incorrect\n", mon_name);
      // MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION, "Warning/Error %s",err_msg);
      err_msg[0]='\0';
      // return;
    } 
    else 
    {
      //first check is for to check whether agent type is mentioned in standard_monitor.dat file or not
      if((std_mon_ptr->agent_type & CONNECT_TO_BOTH_AGENT))
      {
        if((set_agent_type(json_info_ptr, std_mon_ptr, mon_name)) == -1)
           return; 
      }
      json_info_ptr->std_mon_ptr = std_mon_ptr;
      gdf_name = std_mon_ptr->gdf_name;
    }
  }

 
  if(!strcasecmp(temp_tiername, "AllTier"))
  {
    int ndc_mon_applied = 0; // to avoid multiple entries of AllTier for ndc monitors

    for(tier_id=0; tier_id < topo_info[topo_idx].total_tier_count; tier_id++) 
    {
      if(no_of_exclude_tier_elements)
      {
        ret = check_if_tier_excluded(no_of_exclude_tier_elements, exclude_tier_arr, topo_info[topo_idx].topo_tier_info[tier_id].tier_name);

        if(ret == 1)
        {
          ret = 0;
          continue;
        }
      }
 
      
      if(json_info_ptr != NULL)
      {
        //topo_idx is 0 as we have only one topology in the case of NS.
        MALLOC_AND_COPY(topo_info[topo_idx].topo_tier_info[tier_id].tier_name,json_info_ptr->tier_name , 
                                             (strlen(topo_info[topo_idx].topo_tier_info[tier_id].tier_name) + 1), "Copy tier name", -1);
        json_info_ptr->tier_idx=tier_id;
        json_info_ptr->metric_priority = g_metric_priority;
      }
      
      if (json_info_ptr && (json_info_ptr->agent_type & CONNECT_TO_NDC) && !ndc_mon_applied)
      {
        ndc_mon_applied = 1;

        strcpy(dummy_ptr, temp_tiername);
        no_of_tier_elements = get_tokens(dummy_ptr, tier_arr, ",", MAX_NO_OF_APP);
        //Make and send message for delete monitor to NDC.
        if(mon_type == DELETE_STD_MON_TYPE || mon_type == DELETE_CUSTOM_GDF)
        {
          make_and_send_del_msg_to_NDC(tier_arr, no_of_tier_elements, exclude_tier_arr, no_of_exclude_tier_elements,
            specific_server_arr, no_of_specific_server_elements, exclude_server_arr, no_of_exclude_server_elements,
            tier_group_index, instance_name, gdf_name);
        }
        else
        {
          add_entry_for_mbean_mon(mon_name, tier_arr, no_of_tier_elements, exclude_tier_arr, no_of_exclude_tier_elements,
            specific_server_arr, no_of_specific_server_elements, exclude_server_arr, no_of_exclude_server_elements,
            tier_group_index, gdf_name, process_diff_json);
          mbean_mon_rtc_applied = 1;
        }
      } 
      
      if (json_info_ptr && (json_info_ptr->agent_type & CONNECT_TO_CMON))
      {
        if (std_mon_ptr && std_mon_ptr->config_json)
        {  // for MBean monitor
          MALLOC_AND_COPY(std_mon_ptr->config_json, json_info_ptr->config_json, strlen(std_mon_ptr->config_json)+1, "config_json", -1);
          MALLOC_AND_COPY(std_mon_ptr->monitor_name, json_info_ptr->mon_name, strlen(std_mon_ptr->monitor_name)+1, "monitor_name", -1);
        }
        create_mon_buf(exclude_server_arr, no_of_exclude_server_elements, specific_server_arr, no_of_specific_server_elements, mon_name,
        prgrm_args, instance_name, json_info_ptr, process_diff_json, mon_type, tier_id, gdf_name, json_mp_ptr, -1, NULL, NULL);  
      }
    }

    ndc_mon_applied = 0;
  }
  else
  {  // not AllTier
    strcpy(dummy_ptr, temp_tiername);
    no_of_tier_elements = get_tokens(dummy_ptr, tier_arr, ",", MAX_NO_OF_APP);

    if (json_info_ptr && (json_info_ptr->agent_type & CONNECT_TO_NDC))
    {
      //Make and send message for delete monitor to NDC.
      if(mon_type == DELETE_STD_MON_TYPE || mon_type == DELETE_CUSTOM_GDF){
        make_and_send_del_msg_to_NDC(tier_arr, no_of_tier_elements, exclude_tier_arr, no_of_exclude_tier_elements, specific_server_arr,
        no_of_specific_server_elements, exclude_server_arr, no_of_exclude_server_elements, tier_group_index, instance_name, gdf_name);
        //return;
      }
      else{
        add_entry_for_mbean_mon(mon_name, tier_arr, no_of_tier_elements, exclude_tier_arr, no_of_exclude_tier_elements, specific_server_arr, 
        no_of_specific_server_elements, exclude_server_arr, no_of_exclude_server_elements, tier_group_index, gdf_name, process_diff_json);

        mbean_mon_rtc_applied = 1;
      }
      //return;
    } 
    
    if (json_info_ptr && (json_info_ptr->agent_type & CONNECT_TO_CMON))
    {
      if (std_mon_ptr && std_mon_ptr->config_json){  // for MBean monitor
        MALLOC_AND_COPY(std_mon_ptr->config_json, json_info_ptr->config_json, strlen(std_mon_ptr->config_json)+1, "config_json", -1);
        MALLOC_AND_COPY(std_mon_ptr->monitor_name, json_info_ptr->mon_name, strlen(std_mon_ptr->monitor_name)+1, "monitor_name", -1);
      }
      /*
      Regex support for "tier_name" and "list" in case of TierGroup has been removed. 
      User will have to give the complete tier_name. 
      Regex will only be supported for "pattern" in case of TierGroup.
      BUG_ID: 67602 
      */
      for(given_tier_id = 0; given_tier_id < no_of_tier_elements; given_tier_id++) 
      {
        if(tier_type & 0x02)
        {
          for(tier_id=0;tier_id<topo_info[topo_idx].total_tier_count;tier_id++)
          {
            //Using precomiled regex pattern if tier group is used and we are having tier_group_index
            if((tier_group_index != -1) ? nslb_pattern_regexec(topo_info[topo_idx].topo_tier_info[tier_id].tier_name, 
             &(topo_info[topo_idx].topo_tier_group[tier_group_index].preg)) : nslb_regex_match(topo_info[topo_idx].topo_tier_info[tier_id].tier_name, tier_arr[given_tier_id]))
            {
              continue;
            }

            if(no_of_exclude_tier_elements){
              ret = check_if_tier_excluded(no_of_exclude_tier_elements, exclude_tier_arr, topo_info[topo_idx].topo_tier_info[tier_id].tier_name);
        
              if(ret == 1)
              {
                ret = 0;
                continue;
              }
            }
            MALLOC_AND_COPY( topo_info[topo_idx].topo_tier_info[tier_id].tier_name,json_info_ptr->tier_name ,
                                               (strlen(topo_info[topo_idx].topo_tier_info[tier_id].tier_name) + 1), 
                                               "Copy tier name", -1);
            json_info_ptr->tier_idx=tier_id;
            tid= topolib_get_tier_id_from_tier_name(tier_arr[given_tier_id],topo_idx);
            create_mon_buf(exclude_server_arr, no_of_exclude_server_elements, specific_server_arr, no_of_specific_server_elements,
            mon_name, prgrm_args, instance_name, json_info_ptr, process_diff_json, mon_type, tier_id, gdf_name, json_mp_ptr, -1, NULL, NULL);

          }
        }
        else{
          if(no_of_exclude_tier_elements){
            ret = check_if_tier_excluded(no_of_exclude_tier_elements, exclude_tier_arr, tier_arr[given_tier_id]);

            if(ret == 1)
            {
              ret = 0;
              continue;
            }
          }

          tid= topolib_get_tier_id_from_tier_name(tier_arr[given_tier_id],topo_idx);
          if( tid == -1 ){
            MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
                "Tier [%s] does not exist in topology so skipping monitor application for this tier.", tier_arr[given_tier_id]);
            continue;
          }
           //topo_idx is 0 as we have only one topology in the case of NS.
            MALLOC_AND_COPY( topo_info[topo_idx].topo_tier_info[tid].tier_name,json_info_ptr->tier_name ,
                                              (strlen(topo_info[topo_idx].topo_tier_info[tid].tier_name) + 1), "Copy tier name", -1);
            json_info_ptr->tier_idx=tid;
          create_mon_buf(exclude_server_arr, no_of_exclude_server_elements, specific_server_arr, no_of_specific_server_elements,
          mon_name, prgrm_args, instance_name, json_info_ptr, process_diff_json, mon_type, tid, gdf_name, json_mp_ptr, -1, NULL, NULL);
        }
      }
    }
  }
}


int process_custom_gdf_monitors(nslb_json_t *json, int delete_flag, int process_custom_gdf_mon, int process_diff_json, char *temp_tiername, int tier_group_index, char tier_type, JSON_MP *json_mp_ptr, char *json_monitors_filepath)
{
  char prgrm_args[32*MAX_DATA_LINE_LENGTH];
  char exclude_tier[MAX_DATA_LINE_LENGTH];
  char exclude_server[MAX_DATA_LINE_LENGTH];
  char specific_server[MAX_DATA_LINE_LENGTH];
  char instance[MAX_DATA_LINE_LENGTH];
  char gdf_name[MAX_DATA_LINE_LENGTH];
  char *dummy_ptr=NULL;
  int len, ret = 0, run_option;
  char cfile[512];
  //char temp_appname[MAX_DATA_LINE_LENGTH];
  //char tier_group_name[MAX_DATA_LINE_LENGTH];

  JSON_info *json_info_ptr=NULL;

  NSDL2_MON(NULL, NULL, "Method called process_custom_gdf_monitors ");  
  if(!json_info_ptr)
    MY_MALLOC_AND_MEMSET(json_info_ptr, sizeof(JSON_info), "JSON Info Ptr", -1);   
  while(process_custom_gdf_mon)
  {
    ret=0;
    exclude_tier[0] = '\0';
    exclude_server[0] = '\0';
    specific_server[0] = '\0';

    GOTO_NEXT_ARRAY_ITEM_OF_MONITORS_JSON(json);
    OPEN_ELEMENT_OF_MONITORS_JSON(json);

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "enabled",0);
    {
      if(ret!=-1)
      {
        GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
        if( !strcasecmp(dummy_ptr,"false"))
        {
          CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
          continue;
        }
      }
      else
        ret=0;
    }

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "exclude-tier",0);
    {
      if(ret != -1)
      {
        GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
        strcpy(exclude_tier,dummy_ptr);
      }
      else
        ret=0;
     }

     GOTO_ELEMENT_OF_MONITORS_JSON(json, "exclude-server",0);
     {
       if(ret != -1)
       {
         GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
         strcpy(exclude_server,dummy_ptr);
       }
       else
         ret=0;
     }

     GOTO_ELEMENT_OF_MONITORS_JSON(json, "specific-server",0);
     {
       if(ret != -1)
       {
         GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
         strcpy(specific_server,dummy_ptr);
       }
       else
         ret=0;
     }
     
     GOTO_ELEMENT_OF_MONITORS_JSON(json, "instance",0);
     {
       if(ret != -1)
       {
         GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
         strcpy(instance,dummy_ptr);
       }
       else
         ret=0;
     }

     //here frequency is monitor interval
     GOTO_ELEMENT_OF_MONITORS_JSON(json, "frequency",0);
     {
       if(ret != -1)
       {
         GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
         json_info_ptr->frequency = atoi(dummy_ptr) *1000;
       }
       else
         ret=0;
     }

     GOTO_ELEMENT_OF_MONITORS_JSON(json, "program-name", 0);
     if(ret != 1)
     {
       GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
       MALLOC_AND_COPY(dummy_ptr ,json_info_ptr->mon_name, (len + 1) , "JSON Info Ptr mon_name", -1);
     }
     else
     {
       MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"not provided argument: program-name of custom-gdf-mon %s for tier: %s", json_info_ptr->mon_name, temp_tiername);
       CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
       continue;
     }

     GOTO_ELEMENT_OF_MONITORS_JSON(json, "gdf-name", 1);
     if(ret != 1)
     {
       GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
       strcpy(gdf_name, dummy_ptr);
     }
     else
     {
       MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"not provided argument: program-name of custom-gdf-mon %s for tier: %s", json_info_ptr->mon_name, temp_tiername);
       CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
       continue;
     }

     GOTO_ELEMENT_OF_MONITORS_JSON(json, "run-option", 0);
     if(ret != -1)
     {
       GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
       run_option = atoi(dummy_ptr);
       json_info_ptr->run_opt=run_option; 
       if((run_option != 1) && (run_option != 2))
       {
         MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"run_option provided isnot correct. Hence setting it to 2: program-name of custom-gdf-mon %s for tier: %s", json_info_ptr->mon_name, temp_tiername);
         run_option = 2;
         json_info_ptr->run_opt=run_option; 
       }
     }
     else
     {
       run_option = 2;
       json_info_ptr->run_opt=run_option; 
       ret=0;
     }

//Added enhencement for change in the json format for the custom monitor from Monitor UI. 
     
     GOTO_ELEMENT_OF_MONITORS_JSON(json, "cfg", 0);
     if(ret!=-1)
     {
       GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
       sprintf(cfile,"%s/mprof/.custom/%s", g_ns_wdir,dummy_ptr); 
       MALLOC_AND_COPY(cfile,json_info_ptr->config_json,strlen(cfile) + 1 , "JSON Info Ptr cfg", -1); 
     }
     else
     { 
       ret=0; 
       MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"not provided argument: cfg of custom-gdf-mon %s for tier: %s", json_info_ptr->mon_name, temp_tiername);
     }

     GOTO_ELEMENT_OF_MONITORS_JSON(json, "app-name", 1);
     if(ret!=-1)
     {
       GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
       MALLOC_AND_COPY(dummy_ptr,json_info_ptr->app_name,(len + 1) , "JSON Info Ptr app_name", -1);
     }
     else
     {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"not provided argument: app-name of custom-gdf-mon %s for tier: %s", json_info_ptr->mon_name, temp_tiername);
        CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
        continue;
     }
  
     GOTO_ELEMENT_OF_MONITORS_JSON(json, "os-type", 0);
     if(ret!=-1)
     {
       GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
       MALLOC_AND_COPY(dummy_ptr,json_info_ptr->os_type,(len + 1) , "JSON Info Ptr os_type", -1);
     }
     else
     {
       MALLOC_AND_COPY("ALL",json_info_ptr->os_type, 4 ,"JSON Info Ptr os_type", -1); 
       MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"not provided argument: OS-type of custom-gdf-mon %s for tier: %s. Hence setting default os-type to LinuxEx", json_info_ptr->mon_name, temp_tiername);
       ret =0;
     }
 
     GOTO_ELEMENT_OF_MONITORS_JSON(json, "options",1);
     if(ret!=-1)
     {
       GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
       MALLOC_AND_COPY(dummy_ptr, json_info_ptr->options, len + 1, "copy monitor name", -1);
     }
     else
     {
       MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"not provided argument: options of custom-gdf-mon %s for tier: %s", json_info_ptr->mon_name, temp_tiername);
       ret=0;
     }

     GOTO_ELEMENT_OF_MONITORS_JSON(json, "old-options",0);
     if(ret!=-1)
     {
        GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
        MALLOC_AND_COPY(dummy_ptr, json_info_ptr->old_options, len + 1, "copy monitor name", -1);
      }
      else
      {
        if(json_info_ptr->options)
        {
          MALLOC_AND_COPY(json_info_ptr->options, json_info_ptr->old_options, strlen(json_info_ptr->options) + 1, "copy monitor name", -1);
        }
        MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"not provided argument: options of custom-gdf-mon %s for tier: %s", json_info_ptr->mon_name, temp_tiername);
       ret=0;
      }
  
     /*GOTO_ELEMENT_OF_MONITORS_JSON(json, "options", 0);
     if(ret!=-1)
     {
       GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
       MY_MALLOC_AND_MEMSET(json_info_ptr->args,(len + 1), "JSON Info Ptr args", -1);
       strcpy(json_info_ptr->args, dummy_ptr);
       strcpy(prgrm_args, dummy_ptr);
     }
     else
     {
       MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"not provided argument: options of custom-gdf-mon %s for tier: %s", json_info_ptr->mon_name, temp_tiername);
       ret=0;
     }*/

     GOTO_ELEMENT_OF_MONITORS_JSON(json, "mhp", 0);
     if(ret != -1)
     {
       GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
       json_info_ptr->skip_breadcrumb_creation = atoi(dummy_ptr);
       if((json_info_ptr->skip_breadcrumb_creation != 1) && (json_info_ptr->skip_breadcrumb_creation != 0))
       { 
         MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"Metric Hierarchy prefix provided is not correct. Hence setting it to 1: program-name of custom-gdf-mon %s for tier: %s", json_info_ptr->mon_name, temp_tiername);
         json_info_ptr->skip_breadcrumb_creation = 1;
       }
     }
     else
     {
       json_info_ptr->skip_breadcrumb_creation = 1;
       ret=0;
     } 

     GOTO_ELEMENT_OF_MONITORS_JSON(json, "pgm-type", 1);
     if(ret!=-1)
     {
       GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
       MALLOC_AND_COPY(dummy_ptr,json_info_ptr->pgm_type,(len + 1) , "JSON Info Ptr pgm_type", -1);
     }
     else
     {
       MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"not provided argument: pgm-type of custom-gdf-mon %s for tier: %s", json_info_ptr->mon_name, temp_tiername);
       CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
       continue;
     }    
 
     GOTO_ELEMENT_OF_MONITORS_JSON(json, "java-home", 0);
     if(ret!=-1)
     {
       GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
       MALLOC_AND_COPY(dummy_ptr,json_info_ptr->javaHome,(len + 1) , "JSON Info Ptr javaHome", -1);
     }
     else
     {
       MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"not provided argument: java-home of custom-gdf-mon %s for tier: %s", json_info_ptr->mon_name, temp_tiername);
       ret =0;
     }

     GOTO_ELEMENT_OF_MONITORS_JSON(json, "java-classpath", 0);
     if(ret!=-1)
     {
       GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
       MALLOC_AND_COPY(dummy_ptr,json_info_ptr->javaClassPath,(len + 1) , "JSON Info Ptr javaClassPath", -1);
       NSDL2_MON(NULL, NULL, "javaClassPath  %s",json_info_ptr->javaClassPath);
     }
     else
     {
       MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"not provided argument: java-classpath of custom-gdf-mon %s for tier: %s", json_info_ptr->mon_name, temp_tiername);
       ret =0;
     }

     GOTO_ELEMENT_OF_MONITORS_JSON(json, "run-as-process",0);
     if(ret !=-1)
     {
       GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
       if( !strcasecmp(dummy_ptr,"true"))
       {
	 json_info_ptr->is_process = 1;
       }
     }
     else
     {                         
       MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"not provided argument: run-as-process of custom-gdf-mon %s for tier: %s", json_info_ptr->mon_name, temp_tiername);
       ret =0;
     }



    
     GOTO_ELEMENT_OF_MONITORS_JSON(json, "use-agent", 0);
     if(ret!=-1)
     {
       GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
       MALLOC_AND_COPY(dummy_ptr,json_info_ptr->use_agent,(len + 1) , "JSON Info Ptr use_agent", -1);
     }
     else
     {
       MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"not provided argument: use-agent of custom-gdf-mon %s for tier: %s", json_info_ptr->mon_name, temp_tiername);
      ret =0; 
     }
      
     json_info_ptr->agent_type |= CONNECT_TO_CMON;
     GOTO_ELEMENT_OF_MONITORS_JSON(json, "agent-type",0);
     {
       if(ret != -1)
       {
         GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
         if(!strcasecmp(dummy_ptr,"BCI"))
           {
             //here unset cmon 
             json_info_ptr->agent_type |= CONNECT_TO_NDC;
             json_info_ptr->agent_type &= ~CONNECT_TO_CMON;
           }
          // We do not want it to start NDC Mbean monitor to be applied at runtime. 
         else if(!strcasecmp(dummy_ptr,"ALL"))
         {
           json_info_ptr->agent_type |= CONNECT_TO_BOTH_AGENT;
         }
       }
       else
         ret=0;
     }
    
   // sprintf(program_name, "%d %s", run_option, mon_name);
     if(delete_flag)
       delete_add_monitor_from_json(exclude_tier, exclude_server, specific_server, temp_tiername, json_info_ptr->mon_name, prgrm_args, instance, json_info_ptr, process_diff_json, DELETE_CUSTOM_GDF, gdf_name, tier_type, tier_group_index, json_mp_ptr);
     else
       delete_add_monitor_from_json(exclude_tier, exclude_server, specific_server, temp_tiername, json_info_ptr->mon_name, prgrm_args, instance, json_info_ptr, process_diff_json, CUSTOM_GDF_MON_TYPE, gdf_name, tier_type, tier_group_index, json_mp_ptr);

    CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
  }
  return ret;
}


int process_log_monitors(nslb_json_t *json, int delete_flag, int process_log_mon, int process_diff_json, char *temp_tiername, int tier_group_index, char tier_type, JSON_MP *json_mp_ptr, char *json_monitors_filepath)
{
  char prgrm_args[32*MAX_DATA_LINE_LENGTH];
  char old_options[32*MAX_DATA_LINE_LENGTH];
  char new_options[32*MAX_DATA_LINE_LENGTH];
  char exclude_tier[MAX_DATA_LINE_LENGTH];
  char exclude_server[MAX_DATA_LINE_LENGTH];
  char specific_server[MAX_DATA_LINE_LENGTH];
  char instance[MAX_DATA_LINE_LENGTH];
  char mon_name[MAX_DATA_LINE_LENGTH];
  char gdf_name[MAX_DATA_LINE_LENGTH];
  char *dummy_ptr=NULL;
  int len, ret=0;
  int frequency;
  //char temp_appname[MAX_DATA_LINE_LENGTH];
  //char tier_group_name[MAX_DATA_LINE_LENGTH];

  JSON_info *json_info_ptr=NULL;

  while (process_log_mon)
  {
    frequency=0;
    ret=0;
    exclude_tier[0] = '\0';
    exclude_server[0] = '\0';
    specific_server[0] = '\0';
    instance[0]='\0';
    prgrm_args[0]='\0';
    gdf_name[0]='\0';
    GOTO_NEXT_ARRAY_ITEM_OF_MONITORS_JSON(json);
    OPEN_ELEMENT_OF_MONITORS_JSON(json);

    if(!delete_flag)
    {
      GOTO_ELEMENT_OF_MONITORS_JSON(json, "enabled",0);
      {
        if(ret!=-1)
        {
          GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
          if( !strcasecmp(dummy_ptr,"false"))
          {
            CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
            continue;
          }
        }
        else
          ret=0;
      }
    }

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "log-mon-name",1);
    if(ret!=-1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      strcpy(mon_name,dummy_ptr);
      GOTO_ELEMENT_OF_MONITORS_JSON(json, "exclude-tier",0);
      {
        if(ret != -1)
        {
          GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
          strcpy(exclude_tier,dummy_ptr);
        }
        else
          ret=0;
      }

      GOTO_ELEMENT_OF_MONITORS_JSON(json, "exclude-server",0);
      {
        if(ret != -1)
        {
          GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
          strcpy(exclude_server,dummy_ptr);
        }
        else
          ret=0;
     }

      GOTO_ELEMENT_OF_MONITORS_JSON(json, "specific-server",0);
      {
        if(ret != -1)
        {
          GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
          strcpy(specific_server,dummy_ptr);
        }
        else
          ret=0;
      }
      //here frequency is monitor interval
      GOTO_ELEMENT_OF_MONITORS_JSON(json, "frequency",0);
      {
        if(ret != -1)
        {
          GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
          frequency = atoi(dummy_ptr) *1000;
        }
        else
          ret=0;
      }
 
      GOTO_ELEMENT_OF_MONITORS_JSON(json, "instance",0);
      {
        if(ret != -1)
        {
          GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
          strcpy(instance,dummy_ptr);
        }
        else
        {
          strcpy(instance, mon_name);
          ret=0;
        }
      }
    }
    else
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"not provided argument: log-mon-name for tier: %s ",temp_tiername);
      CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
      continue;
    }

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "gdf-name",1);
    if(ret!=-1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      strcpy(gdf_name,dummy_ptr); 
    }
    else
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"not provided argument: gdf-name of log-mon-name %s for tier: %s",mon_name,temp_tiername);
      CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
      continue;
    }
    GOTO_ELEMENT_OF_MONITORS_JSON(json, "run-option",0);
    if(ret!=-1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      strcat(prgrm_args,dummy_ptr);
      strcat(prgrm_args," ");
    }
    else
    {
      strcat(prgrm_args,"2");
      strcat(prgrm_args," ");
      ret=0;
    }

    if(process_log_mon == 1 )
      strcat(prgrm_args,"java cm_log_parser");
    else if(process_log_mon == 2)
      strcat(prgrm_args,"cm_get_log_file");
    else if(process_log_mon == 3)
      strcat(prgrm_args,"java cm_file");

    strcat(prgrm_args," ");
    /*GOTO_ELEMENT_OF_MONITORS_JSON(json, "options",1);
    if(ret!=-1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      strcat(prgrm_args,dummy_ptr);
    }
     else
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"not provided argument: options of log-mon-name %s for tier: %s ",mon_name,temp_tiername);
      CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
      continue;
    }*/

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "options",1);
    if(ret!=-1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      strcpy(new_options, prgrm_args);
      strcat(new_options, dummy_ptr);
    }
    else
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"not provided argument: options of log-mon-name %s for tier: %s ",mon_name,temp_tiername);
      CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
      continue;
    }

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "old-options",0);
    if(ret!=-1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      strcpy(old_options, prgrm_args);
      strcat(old_options, dummy_ptr);
    }
    else
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"not provided argument: options of log-mon-name %s for tier: %s ",mon_name,temp_tiername);
      ret=0;
    }
 
    MY_MALLOC_AND_MEMSET(json_info_ptr, sizeof(JSON_info), "JSON Info Ptr", -1);
    json_info_ptr->frequency = frequency *1000;
    json_info_ptr->lps_enable = 1;
   
    MALLOC_AND_COPY(new_options, json_info_ptr->options, strlen(new_options) + 1, "copy monitor name", -1);
    if(old_options[0] != '\0')
    {
      MALLOC_AND_COPY(old_options, json_info_ptr->old_options, strlen(old_options) + 1, "copy monitor name", -1);
    }
    else
    {
      MALLOC_AND_COPY(json_info_ptr->options, json_info_ptr->old_options, strlen(json_info_ptr->options) + 1, "copy monitor name", -1);
    }

    json_info_ptr->agent_type |= CONNECT_TO_CMON;
    if(delete_flag)
      delete_add_monitor_from_json(exclude_tier, exclude_server, specific_server, temp_tiername, mon_name, prgrm_args, instance, json_info_ptr, process_diff_json, DELETE_CUSTOM_GDF, gdf_name, tier_type, tier_group_index, json_mp_ptr);
    else
      delete_add_monitor_from_json(exclude_tier, exclude_server, specific_server, temp_tiername, mon_name, prgrm_args, instance, json_info_ptr, process_diff_json, LOG_MON_TYPE, gdf_name, tier_type, tier_group_index, json_mp_ptr);

    CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
  }
  return ret;
}

int process_batch_job_and_check_monitors(nslb_json_t *json ,int process_diff_json,int delete_flag, char *temp_tiername, int tier_group_index, char tier_type, JSON_MP *json_mp_ptr ,int process_check_mon, int process_batch_job, char *json_monitors_filepath)
{
  char prgrm_args[32*MAX_DATA_LINE_LENGTH];
  char old_options[32*MAX_DATA_LINE_LENGTH];
  char exclude_tier[MAX_DATA_LINE_LENGTH];
  char exclude_server[MAX_DATA_LINE_LENGTH];
  char specific_server[MAX_DATA_LINE_LENGTH];
  char instance[MAX_DATA_LINE_LENGTH];
  char mon_name[MAX_DATA_LINE_LENGTH];
  char *dummy_ptr=NULL;
  int len, ret=0;
    
  JSON_info *json_info_ptr=NULL;
  
  while (process_check_mon || process_batch_job)
  {
    ret=0;
    old_options[0]= '\0';
    exclude_tier[0] = '\0';
    exclude_server[0] = '\0';
    specific_server[0] = '\0';
    prgrm_args[0]='\0';
    instance[0]='\0';
    GOTO_NEXT_ARRAY_ITEM_OF_MONITORS_JSON(json);
    OPEN_ELEMENT_OF_MONITORS_JSON(json);

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "enabled",0);
    {
      if(ret!=-1)
      {
        GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
        if( !strcasecmp(dummy_ptr,"false"))
        {
          CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
          continue;
        }
      }
      else
        ret=0;
    }

    if(process_batch_job == 0)
    {
      GOTO_ELEMENT_OF_MONITORS_JSON(json, "check-mon-name",1);
    }
    else
    {
      GOTO_ELEMENT_OF_MONITORS_JSON(json, "batch-job-mon",1);
    }

    if(ret!=-1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      strcpy(mon_name,dummy_ptr);

      GOTO_ELEMENT_OF_MONITORS_JSON(json, "exclude-tier",0);
      {
        if(ret != -1)
        { 
          GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
          strcpy(exclude_tier,dummy_ptr);
        }
        else
          ret=0;
      }

      GOTO_ELEMENT_OF_MONITORS_JSON(json, "exclude-server",0);
      {
        if(ret != -1)
        {
          GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
          strcpy(exclude_server,dummy_ptr);
        }
        else
          ret=0;
      }

      GOTO_ELEMENT_OF_MONITORS_JSON(json, "specific-server",0);
      {
        if(ret != -1)
        {
          GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
          strcpy(specific_server,dummy_ptr);
        }
        else
          ret=0;
      }
      GOTO_ELEMENT_OF_MONITORS_JSON(json, "instance",0);
      {
        if(ret != -1)
        {
          GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
          strcpy(instance,dummy_ptr);
        }
        else
          ret=0;
      }
    }
    else
    {
      if (process_batch_job ==0)
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"not provided argument: check-mon-name for tier :%s",temp_tiername);
      }
      else
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"not provided argument: batch-job-mon for tier :%s",temp_tiername);
      }
      CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
      continue;
    }
    ret=0;

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "options",1);
    if(ret!=-1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      strcpy(prgrm_args, dummy_ptr);
    }
    else
    {
      MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"not provided argument: options of custom-gdf-mon %s for tier: %s", mon_name, temp_tiername);
      ret=0;
    }

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "old-options",0);
    if(ret!=-1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      strcpy(old_options, dummy_ptr);
    }
    else
    {
      MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"not provided argument: options of custom-gdf-mon %s for tier: %s", mon_name, temp_tiername);
      ret=0;
    }

    MY_MALLOC_AND_MEMSET(json_info_ptr, sizeof(JSON_info), "JSON Info Ptr", -1);
    json_info_ptr->agent_type |= CONNECT_TO_CMON;

    MALLOC_AND_COPY(mon_name, json_info_ptr->mon_name, strlen(mon_name)+1, "copy monitor name", -1); 
    MALLOC_AND_COPY(prgrm_args, json_info_ptr->options, strlen(prgrm_args)+1, "copy monitor name", -1);
    if(old_options[0] != '\0')
    {
      MALLOC_AND_COPY(old_options, json_info_ptr->old_options, strlen(old_options) + 1, "copy monitor name", -1);
    }
    else
    {
      MALLOC_AND_COPY(json_info_ptr->options, json_info_ptr->old_options, strlen(json_info_ptr->options) + 1, "copy monitor name", -1);
    }

    if(!delete_flag)
    {
      if (process_batch_job == 0)
        delete_add_monitor_from_json(exclude_tier, exclude_server, specific_server, temp_tiername, mon_name, prgrm_args, instance, json_info_ptr, process_diff_json, CHECK_MON_TYPE, NULL, tier_type, tier_group_index, json_mp_ptr);
      else
        delete_add_monitor_from_json(exclude_tier, exclude_server, specific_server, temp_tiername, mon_name, prgrm_args, instance, json_info_ptr, process_diff_json, BATCH_JOB_TYPE, NULL, tier_type, tier_group_index, json_mp_ptr);
    }
    else
    {
      if (process_batch_job == 0)
        delete_add_monitor_from_json(exclude_tier, exclude_server, specific_server, temp_tiername, mon_name, prgrm_args, instance, json_info_ptr, process_diff_json, DELETE_CHECK_MON_TYPE, NULL, tier_type, tier_group_index, json_mp_ptr);
      else
        delete_add_monitor_from_json(exclude_tier, exclude_server, specific_server, temp_tiername, mon_name, prgrm_args, instance, json_info_ptr, process_diff_json,DELETE_BATCH_JOB_TYPE, NULL, tier_type, tier_group_index, json_mp_ptr);
    }

    FREE_AND_MAKE_NULL(json_info_ptr, "Freeing Json Element", -1);
    
    CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
  }
  return ret;
}

//Called in test start
int add_json_monitors_ex(int process_diff_json, nslb_json_t *json, int delete_flag)
{
  NSDL2_MON(NULL, NULL, "Method called for MetricPriority at tier level ");
  char prgrm_args[32*MAX_DATA_LINE_LENGTH];
  char exclude_tier[MAX_DATA_LINE_LENGTH];
  char exclude_server[MAX_DATA_LINE_LENGTH];
  char specific_server[MAX_DATA_LINE_LENGTH];
  char dest_any_server[MAX_DATA_LINE_LENGTH];
  char instance[MAX_DATA_LINE_LENGTH];
  char mon_name[MAX_DATA_LINE_LENGTH];
  char *dummy_ptr=NULL;
  int len, ret=0, i, j;
  char *buf[50];
  int total_mp_enteries = 0;
  int total_enteries =0;
  int max_mp_entries = 0;
  int mp_row_num;
  char *field[50];
  char temp_appname[MAX_DATA_LINE_LENGTH];
  char temp_tiername[MAX_DATA_LINE_LENGTH];
  char tier_group_name[MAX_DATA_LINE_LENGTH];
  char json_monitors_filepath[MAX_DATA_LINE_LENGTH];
  int process_check_mon, process_server_sig, process_std_mon, process_log_mon, tier_group_index, process_custom_gdf_mon, process_batch_job;
  int is_monitor_applied=0;
  char tier_type = 0;
  //char agent_type;
  int new_pod_entry=0;

  NSDL2_MON(NULL, NULL, "Method called. process_diff_json = %d  delete_flag = %d", process_diff_json, delete_flag);
 
  JSON_MP *json_mp_ptr = NULL;
  JSON_info *json_info_ptr=NULL;
  
  strcpy(json_monitors_filepath,global_settings->auto_json_monitors_filepath);

  GOTO_ELEMENT_OF_MONITORS_JSON(json, "Tier", 1);
  OPEN_ELEMENT_OF_MONITORS_JSON(json);

  if(ret == -1)
  {
    if(((int)json->error)!=6)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                               "Can't able to open Tier element in json [%s] while applying json auto monitors due to error: %s.", 
                                global_settings->auto_json_monitors_filepath, nslb_json_strerror(json));
      return -1;
    }
  }

  while (1)
  {
    process_check_mon=0;
    process_batch_job=0;
    process_server_sig=0;
    process_std_mon=0;  
    process_log_mon=0;
    tier_group_index=-1;
    ret = 0;
    tier_type = 0;
    
    GOTO_NEXT_ARRAY_ITEM_OF_MONITORS_JSON(json);
    OPEN_ELEMENT_OF_MONITORS_JSON(json);

    ret=0;
    GOTO_ELEMENT_OF_MONITORS_JSON(json, "MetricPriority", 0);
    if(ret != -1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON_EX(json, dummy_ptr, &len, 0, 1);
      total_enteries = get_tokens_with_multi_delimiter(dummy_ptr, buf, ",", 50);
      if(total_mp_enteries >= max_mp_entries)
      {
        MY_REALLOC_AND_MEMSET(json_mp_ptr, (5+max_mp_entries)*sizeof(JSON_MP), (max_mp_entries)*sizeof(JSON_MP), "json mp ptr allocation", -1);
        max_mp_entries += 5;
      }

      for(i = 0; i < total_enteries; i++) 
      {
        get_tokens_with_multi_delimiter(buf[i], field, ":", 2);
        if((mp_row_num = check_if_tier_exist(field[0], total_mp_enteries, json_mp_ptr)) < 0)
        {
          mp_row_num = total_mp_enteries;
          MALLOC_AND_COPY(field[0], json_mp_ptr[mp_row_num].tier, strlen(field[0])+1, "copying tier in overide MP", -1);
          total_mp_enteries ++;

          NSDL3_MON(NULL, NULL, "Structure Entry: Metric Priority at Tier level: index=%d, Tier=%s, MP=%s", mp_row_num, json_mp_ptr[mp_row_num].tier, field[1]);
        }
       
        GET_MP(field[1], json_mp_ptr[mp_row_num].mp) 
      }
      NSDL2_MON(NULL, NULL, "Method called for MetricPriority at tier level ");
    }
    else
      ret = 0;

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "OverrideMetricsPriority", 0);
    if(ret != -1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      total_enteries = get_tokens_with_multi_delimiter(dummy_ptr, buf, ",", 50);
      if(total_mp_enteries >= max_mp_entries)
      {
        MY_REALLOC_AND_MEMSET(json_mp_ptr, (5+max_mp_entries)*sizeof(JSON_MP), max_mp_entries*sizeof(JSON_MP), "josn mp ptr allocation", -1);
        max_mp_entries += 5;
      }
      for(i = 0; i < total_enteries; i++)
      { 
        get_tokens_with_multi_delimiter(buf[i], field, ">:", 3);
        mp_row_num = check_if_tier_exist(field[0], total_mp_enteries, json_mp_ptr);
        if(mp_row_num < 0)
        {
          mp_row_num = total_mp_enteries;
          MALLOC_AND_COPY(field[0], json_mp_ptr[mp_row_num].tier, strlen(field[0])+1, "copying tier in overide MP", -1);
        }
        if(json_mp_ptr[mp_row_num].total_server_entries >= json_mp_ptr[mp_row_num].max_server_entries)
        {
          MY_REALLOC_AND_MEMSET(json_mp_ptr[mp_row_num].server_mp_ptr, (json_mp_ptr[mp_row_num].max_server_entries+5)*sizeof(SERVER_MP), json_mp_ptr[mp_row_num].max_server_entries*sizeof(SERVER_MP), "Server MP structure", -1);
          json_mp_ptr[mp_row_num].max_server_entries += 5;
        }
        MALLOC_AND_COPY(field[1], json_mp_ptr[mp_row_num].server_mp_ptr[json_mp_ptr[mp_row_num].total_server_entries].server, strlen(field[1])+1, "copying server in Server MP", -1); 
        GET_MP(field[2], json_mp_ptr[mp_row_num].server_mp_ptr[json_mp_ptr[mp_row_num].total_server_entries].mp); 
        json_mp_ptr[mp_row_num].total_server_entries ++;        

        NSDL3_MON(NULL, NULL, "Structure Entry: Override Metric Priority at Tier level: index=%d, Tier=%s, TierMP=%d, ServerIndex=%d, Server=%s, ServerMP=%s", mp_row_num, json_mp_ptr[mp_row_num].tier, json_mp_ptr[mp_row_num].mp, (json_mp_ptr[mp_row_num].total_server_entries - 1), field[1], field[2]);
      }                      
    }
    else
      ret = 0;

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "tier_group",0);
    if(ret != -1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      strcpy(tier_group_name, dummy_ptr);

      if(tier_group_name[0] != '\0')
      {
        for(i=0;i<topo_info[topo_idx].total_tier_group_entries;i++)
        {
          if(!strcmp(topo_info[topo_idx].topo_tier_group[i].GrpName, tier_group_name))
          {
            tier_group_index = i;
            break;
          }
        }
      
        if(tier_group_index != -1)
        {
          if(!strcasecmp(topo_info[topo_idx].topo_tier_group[i].type, "list"))
          {
            strcpy(temp_tiername, topo_info[topo_idx].topo_tier_group[i].allTierList);
            tier_type |= 0x01;
          }
          else if(!strcasecmp(topo_info[topo_idx].topo_tier_group[i].type, "pattern"))
          {
            strcpy(temp_tiername, topo_info[topo_idx].topo_tier_group[i].GrpDefination);
            tier_type |= 0x02;
          }
        }
        else
        {
          CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "ERROR:Tier Group %s is not found in tier group structure.", tier_group_name);
          continue;
        }
      }
      else
      {
        CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "ERROR:Tier Group entered is null.");
        continue;
      }
    }
    else if(ret == -1)
    {
      ret=0;

      GOTO_ELEMENT_OF_MONITORS_JSON(json, "name",1);

      if(ret==-1)
      {
        if(((int)json->error)!=6)
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                         "Can't able to open name element in json [%s] due to error: %s.", global_settings->auto_json_monitors_filepath, nslb_json_strerror(json));
          return -1;
        }
        ret=0;
      }

      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      strcpy(temp_tiername, dummy_ptr);

      if(ret==-1)
      {
        if(((int)json->error)!=6)
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                         "Can't able to open name element in json [%s] due to error: %s.", global_settings->auto_json_monitors_filepath, nslb_json_strerror(json));
          return -1;
        }
        ret=0;
      }
    }

//check monitor
    ret=0;
    process_check_mon=0;
    GOTO_ELEMENT_OF_MONITORS_JSON(json,"check-monitor",0);
    if(ret != -1)
    {
      is_monitor_applied=1;
      OPEN_ELEMENT_OF_MONITORS_JSON(json);
      if(ret == -1)
      {
        if(((int)json->error)!=6)
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                         "Can't able to open gdf element in json [%s] while applying monitors due to error: %s.", global_settings->auto_json_monitors_filepath, nslb_json_strerror(json));
          return -1;
        }
        ret=0;
      }
      else
        process_check_mon = 1;
    }
    
    ret =process_batch_job_and_check_monitors(json, process_diff_json,delete_flag, temp_tiername, tier_group_index, tier_type, json_mp_ptr,process_check_mon, 0, json_monitors_filepath);    
    if(ret == -1)
    {
      if(((int)json->error)!=6)
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                         "Can't able to apply monitors on Tier [%s] from json [%s] due to error: %s.",temp_tiername , global_settings->auto_json_monitors_filepath, nslb_json_strerror(json));
        //continue;
      }
      else
      {
        CLOSE_ELEMENT_ARR_OF_MONITORS_JSON(json);
        //CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
        //continue;
        ret=0;
      }
    }

// batch job monitors
    ret=0;
    process_batch_job = 0;
    GOTO_ELEMENT_OF_MONITORS_JSON(json,"batch-job",0);
    if(ret != -1)
    {
      is_monitor_applied=1;
      OPEN_ELEMENT_OF_MONITORS_JSON(json);
      if(ret == -1)
      {
        if(((int)json->error)!=6)
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                       "Can't able to open gdf element in json [%s] while applying monitors due to error: %s.", global_settings->auto_json_monitors_filepath, nslb_json_strerror(json));
          return -1;
        }
        ret=0;
      }
      else
        process_batch_job = 1;
    }
    
    ret =process_batch_job_and_check_monitors(json ,process_diff_json ,delete_flag,temp_tiername, tier_group_index, tier_type, json_mp_ptr ,0 ,process_batch_job, json_monitors_filepath);
    
    if(ret == -1)
    {
      if(((int)json->error)!=6)
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                         "Can't able to apply monitors on Tier [%s] from json [%s] due to error: %s.",temp_tiername , global_settings->auto_json_monitors_filepath, nslb_json_strerror(json));
        //continue;
      }
      else
      {
        CLOSE_ELEMENT_ARR_OF_MONITORS_JSON(json);
        //CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
        //continue;
        ret=0;
      }
    }
       
// server signature array
    if(!delete_flag)
    {
      GOTO_ELEMENT_OF_MONITORS_JSON(json,"server-signature",0);
      if(ret != -1)
      {
        is_monitor_applied=1;
        OPEN_ELEMENT_OF_MONITORS_JSON(json);
        if(ret == -1)
        {
          if(((int)json->error)!=6)
          {
            MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                         "Can't able to open gdf element in json [%s] while applying monitors due to error: %s.", global_settings->auto_json_monitors_filepath, nslb_json_strerror(json));
            return -1;
          }
          ret=0;
        }
        else
          process_server_sig = 1;
      }
      else
        ret = 0;
    }

    while (process_server_sig)
    {
      ret=0;
      exclude_tier[0] = '\0';
      exclude_server[0] = '\0';
      specific_server[0] = '\0';
      prgrm_args[0]='\0';
      instance[0]='\0';
      GOTO_NEXT_ARRAY_ITEM_OF_MONITORS_JSON(json);
      OPEN_ELEMENT_OF_MONITORS_JSON(json);

      GOTO_ELEMENT_OF_MONITORS_JSON(json, "enabled",0);
      {
        if(ret!=-1)
        {
          GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
          if( !strcasecmp(dummy_ptr,"false"))
   {
     CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
            continue;
   }
        }
        else
          ret=0;
      }

      GOTO_ELEMENT_OF_MONITORS_JSON(json, "signature-name",1);
      if(ret!=-1)
      {
        GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
        strcpy(mon_name,dummy_ptr);

        GOTO_ELEMENT_OF_MONITORS_JSON(json, "exclude-tier",0);
        {
          if(ret != -1)
          {
            GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
            strcpy(exclude_tier,dummy_ptr);
          }
          else
            ret=0;
        }

        GOTO_ELEMENT_OF_MONITORS_JSON(json, "exclude-server",0);
        {
          if(ret != -1)
          {
            GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
            strcpy(exclude_server,dummy_ptr);
          }
          else
            ret=0;
        }

        GOTO_ELEMENT_OF_MONITORS_JSON(json, "specific-server",0);
        {
          if(ret != -1)
          {
            GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
            strcpy(specific_server,dummy_ptr);
          }
          else
            ret=0;
        }
        
        GOTO_ELEMENT_OF_MONITORS_JSON(json, "instance",0);
        {
          if(ret != -1)
          {
            GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
            strcpy(instance,dummy_ptr);
          }
          else
            ret=0;
        }
      }
      else
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"not provided argument: signature-name for tier: %s ",temp_tiername);
        CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
        continue;
      }

      GOTO_ELEMENT_OF_MONITORS_JSON(json, "signature-type",0);
      if(ret!=-1)
      {
        GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
        strcat(prgrm_args,dummy_ptr);
        strcat(prgrm_args," ");
        GOTO_ELEMENT_OF_MONITORS_JSON(json, "command/file-name",0);
        if(ret!=-1)
        {
          GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
          strcat(prgrm_args,dummy_ptr);
        }
        else
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"not provided argument:command/file-name of signature-name %s for tier: %s ",mon_name,temp_tiername);
          CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
          continue;
        }
      } 
      else
      {
        ret = 0;
        /*GOTO_ELEMENT_OF_MONITORS_JSON(json, "options",1);
        if(ret!=-1)
        {
          GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
          strcpy(prgrm_args,dummy_ptr);
        }
        else
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"not provided argument: signature-type and options of signature-name %s for tier: %s",mon_name,temp_tiername);
          CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
          continue;
        }*/
        
        GOTO_ELEMENT_OF_MONITORS_JSON(json, "options",1);
        if(ret!=-1)
        {
          GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
          strcpy(prgrm_args,dummy_ptr);
        }
        else
        {
          MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"not provided argument: options of server_signature name %s for tier: %s", mon_name, temp_tiername);
          ret=0;
        }
      }

      MY_MALLOC_AND_MEMSET(json_info_ptr, sizeof(JSON_info), "JSON Info Ptr", -1);
      MALLOC_AND_COPY(prgrm_args, json_info_ptr->options, strlen(prgrm_args) + 1, "copy monitor name", -1);

      MALLOC_AND_COPY(json_info_ptr->options, json_info_ptr->old_options, strlen(json_info_ptr->options) + 1, "copy monitor name", -1);

      json_info_ptr->agent_type |= CONNECT_TO_CMON;
      
      if(strstr(prgrm_args, "cm_service_stats") != NULL)
	json_info_ptr->gdf_flag = SERVICE_STATS; 

      
      delete_add_monitor_from_json(exclude_tier, exclude_server, specific_server, temp_tiername, mon_name, prgrm_args, instance, json_info_ptr, process_diff_json, SERVER_SIGNATURE_MON_TYPE, NULL, tier_type, tier_group_index, json_mp_ptr);
      FREE_AND_MAKE_NULL(json_info_ptr, "Freeing Json Element", -1);

      CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);

    }

    if(ret == -1)
    {
      if(((int)json->error)!=6)
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                         "Can't able to apply monitors on Tier [%s] from json [%s] due to error: %s.",temp_tiername, global_settings->auto_json_monitors_filepath, nslb_json_strerror(json));
       // return -1;
      }
      else
      {
        CLOSE_ELEMENT_ARR_OF_MONITORS_JSON(json);
       // CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
       // continue;
      }
    }

// LOG MONITOR

    ret=0;
    process_log_mon = 0;
    GOTO_ELEMENT_OF_MONITORS_JSON(json,"log-pattern-mon",0);
    if(ret != -1)
    {
      is_monitor_applied=1;
      OPEN_ELEMENT_OF_MONITORS_JSON(json);
      if(ret == -1)
      {
        if(((int)json->error)!=6)
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                       "Can't able to open gdf element in json [%s] while applying monitors due to error: %s.", global_settings->auto_json_monitors_filepath, nslb_json_strerror(json));
          return -1;
        }
        ret=0;
      }
      else
        process_log_mon = 1;
    }
    ret = process_log_monitors(json, delete_flag, process_log_mon, process_diff_json, temp_tiername, tier_group_index, tier_type, json_mp_ptr, json_monitors_filepath);
    if(ret == -1)
    {
      if(((int)json->error)!=6)
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                         "Can't able to apply monitors on Tier [%s] from json [%s] due to error: %s.",temp_tiername, global_settings->auto_json_monitors_filepath, nslb_json_strerror(json));
        // return -1;
      }
      else
      {
        CLOSE_ELEMENT_ARR_OF_MONITORS_JSON(json);
        // CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
        // continue;
      }
    }

    //call func
    
    ret=0;
    process_log_mon = 0;
    GOTO_ELEMENT_OF_MONITORS_JSON(json,"get-log-file",0);
    if(ret != -1)
    { 
      is_monitor_applied=1;
      OPEN_ELEMENT_OF_MONITORS_JSON(json);
      if(ret == -1)
      {
        if(((int)json->error)!=6)
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                         "Can't able to open log monitor element in json [%s] while applying monitors due to error: %s.", global_settings->auto_json_monitors_filepath, nslb_json_strerror(json));
          return -1;
        }
        ret=0;
      }
      else
        process_log_mon = 2;
    }
    //func call
    ret = process_log_monitors(json, delete_flag, process_log_mon, process_diff_json, temp_tiername, tier_group_index, tier_type, json_mp_ptr, json_monitors_filepath);
    if(ret == -1)
    {
      if(((int)json->error)!=6)
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                         "Can't able to apply monitors on Tier [%s] from json [%s] due to error: %s.",temp_tiername, global_settings->auto_json_monitors_filepath, nslb_json_strerror(json));
        // return -1;
      }
      else
      {
        CLOSE_ELEMENT_ARR_OF_MONITORS_JSON(json);
        // CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
        // continue;
      }
    }
    
    ret=0;
    process_log_mon = 0;
    GOTO_ELEMENT_OF_MONITORS_JSON(json,"log-data-mon",0);
    if(ret != -1)
    {
      is_monitor_applied=1;
      OPEN_ELEMENT_OF_MONITORS_JSON(json);
      if(ret == -1)
      {
        if(((int)json->error)!=6)
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                         "Can't able to open log monitor element in json [%s] while applying monitors due to error: %s.", global_settings->auto_json_monitors_filepath, nslb_json_strerror(json));
          return -1;
        }
        ret=0;
      }
      else
        process_log_mon = 3;
    }
    ret = process_log_monitors(json, delete_flag, process_log_mon, process_diff_json, temp_tiername, tier_group_index, tier_type, json_mp_ptr, json_monitors_filepath);

    if(ret == -1)
    {
      if(((int)json->error)!=6)
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                         "Can't able to apply monitors on Tier [%s] from json [%s] due to error: %s.",temp_tiername, global_settings->auto_json_monitors_filepath, nslb_json_strerror(json));
       // return -1;
      }
      else
      {
        CLOSE_ELEMENT_ARR_OF_MONITORS_JSON(json);
       // CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
       // continue;
      }
    }

    //custom_gdf_mon
    ret=0;
    process_custom_gdf_mon = 0;
    GOTO_ELEMENT_OF_MONITORS_JSON(json,"custom-gdf-mon",0);
    if(ret != -1)
    {
      is_monitor_applied=1;
      OPEN_ELEMENT_OF_MONITORS_JSON(json);
      if(ret == -1)
      {
        if(((int)json->error)!=6)
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                       "Can't able to open gdf element in json [%s] while applying monitors due to error: %s.", global_settings->auto_json_monitors_filepath, nslb_json_strerror(json));
          return -1;
        }
        ret=0;
      }
      else
        process_custom_gdf_mon = 1;
    }
 
    if(process_custom_gdf_mon)
    {
      ret = process_custom_gdf_monitors(json, delete_flag, process_custom_gdf_mon, process_diff_json, temp_tiername, tier_group_index, tier_type, json_mp_ptr, json_monitors_filepath);
 
      if(ret == -1)
      {
        if(((int)json->error)!=6)
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                           "Can't able to apply monitors on Tier [%s] from json [%s] due to error: %s.",temp_tiername, global_settings->auto_json_monitors_filepath, nslb_json_strerror(json));
         // return -1;
        }
        else
        {
          CLOSE_ELEMENT_ARR_OF_MONITORS_JSON(json);
         // CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
         // continue;
        }
      }
    }
    
// gdf/std-mon array
    ret=0;
    GOTO_ELEMENT_OF_MONITORS_JSON(json, "gdf",0);
    if(ret != -1)
    {
      OPEN_ELEMENT_OF_MONITORS_JSON(json);
    }
    else   //If gdf is not found, it means no more array brackets present. So we need to close the obj element, hence closing it. 
    { 
      CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
      continue;
    }

    if(ret == -1)
    {
      ret=0;

      if(is_monitor_applied == 1)
      {
        GOTO_ELEMENT_OF_MONITORS_JSON(json, "std-mon",0);
        if(ret == -1)
          return -1;
      }
      else
      {
        GOTO_ELEMENT_OF_MONITORS_JSON(json, "std-mon",1);
      }
      OPEN_ELEMENT_OF_MONITORS_JSON(json);
      if(ret==-1)
      {
        if(((int)json->error)!=6)
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                         "Can't able to open gdf element in json [%s] due to error: %s.", global_settings->auto_json_monitors_filepath, nslb_json_strerror(json));
          return -1;
        }
        ret=0;

      }
      else
        process_std_mon=1;
    }
    else
      process_std_mon=1;

    while (process_std_mon)
    {
      ret=0;
      exclude_tier[0] = '\0';
      exclude_server[0] = '\0';
      specific_server[0] = '\0';
      dest_any_server[0] = '\0';
      instance[0]='\0';
      //Memory allocation for json_info_ptr
      if(!json_info_ptr)
      {
        MY_MALLOC_AND_MEMSET(json_info_ptr, sizeof(JSON_info), "JSON Info Ptr", -1);
        json_info_ptr->json_mp_ptr = json_mp_ptr;
        json_info_ptr->total_mp_enteries = total_mp_enteries;
        json_info_ptr->metric_priority = g_metric_priority;
      }

      GOTO_NEXT_ARRAY_ITEM_OF_MONITORS_JSON(json);
      OPEN_ELEMENT_OF_MONITORS_JSON(json);

      if(!delete_flag)
      {
        GOTO_ELEMENT_OF_MONITORS_JSON(json, "enabled",0);
        {
          if(ret!=-1)
          {
            GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
            if( !strcasecmp(dummy_ptr,"false"))
	          {
	            CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
              continue;
	          }
          }
          else
            ret=0;
        }
      }

      GOTO_ELEMENT_OF_MONITORS_JSON(json, "std-mon-name",1);
      if(ret!=-1)
      {
        GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
        strcpy(mon_name,dummy_ptr);

        GOTO_ELEMENT_OF_MONITORS_JSON(json, "exclude-tier",0);
        {
          if(ret != -1)
          {
            GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
            strcpy(exclude_tier,dummy_ptr);
          }
          else
            ret=0;
        }

        GOTO_ELEMENT_OF_MONITORS_JSON(json, "exclude-server",0);
        {
          if(ret != -1)
          {
            GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
            strcpy(exclude_server,dummy_ptr);
          }
          else
            ret=0;
        }

        GOTO_ELEMENT_OF_MONITORS_JSON(json, "specific-server",0);
        {
          if(ret != -1)
          {
            GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
            strcpy(specific_server,dummy_ptr);
          }
          else
            ret=0;
        }
        
        GOTO_ELEMENT_OF_MONITORS_JSON(json, "tier-for-any-server",0);
        {
          if(ret != -1)
          {
            GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
            strcpy(dest_any_server,dummy_ptr);
          }
          else
            ret=0;
        }

        GOTO_ELEMENT_OF_MONITORS_JSON(json, "instance",0);
        {
          if(ret != -1)
          {
            GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
            strcpy(instance,dummy_ptr);
          }
          else
            ret=0;
        }
        //here frequency is monitor interval
        GOTO_ELEMENT_OF_MONITORS_JSON(json, "frequency",0);
        {
          if(ret != -1)
          {
            GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
            json_info_ptr->frequency = atoi(dummy_ptr) *1000;
          }
          else
            ret=0;
        }
        GOTO_ELEMENT_OF_MONITORS_JSON(json, "init-vector-file",0);
        {
          if(ret != -1)
          {
            GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
            MY_MALLOC_AND_MEMSET(json_info_ptr->init_vector_file, (len + 1), "JSON Info Ptr initVectorFile", -1);
            strcpy(json_info_ptr->init_vector_file, dummy_ptr);
            if(json_info_ptr->init_vector_file[0] == '\0')
            {
              MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                         "ERROR:Mandatory field missing. file_name is required with key init_vector_file.");
            }
          }
          else
            ret=0;
        }
     
        /*
        Commenting below block as std_monitor.dat already has entry for agent-type for MBean monitors
        Old monitors in std_monitor.dat doesn't have entry for agent-type field, so will set default agent-type
        to CONNECT_TO_CMON. This will be done when we called function 'get_std_mon_entry' in 'delete_add_monitor_from_json'
        */
        
        json_info_ptr->agent_type |= CONNECT_TO_CMON;
        
        //json_info_ptr->agent_type |= CONNECT_TO_NDC;
        GOTO_ELEMENT_OF_MONITORS_JSON(json, "agent-type",0);
        {
          if(ret != -1)
          {
            GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
            //KUSHAL: this block will execute if agent-type found
            if(!strcasecmp(dummy_ptr, "BCI"))
            {
              //here unset cmon 
              json_info_ptr->agent_type |= CONNECT_TO_NDC;
              json_info_ptr->agent_type &= ~CONNECT_TO_CMON; 
            }
            else if(!strcasecmp(dummy_ptr,"ALL"))
            {
              json_info_ptr->agent_type |= CONNECT_TO_BOTH_AGENT;
            }
          }
          else
            ret=0;
        }

      }

      GOTO_ELEMENT_OF_MONITORS_JSON(json, "app",1);
      OPEN_ELEMENT_OF_MONITORS_JSON(json);

      while(1)
      {
        ret=0;
        prgrm_args[0]='\0';
        new_pod_entry = 0;

        GOTO_NEXT_ARRAY_ITEM_OF_MONITORS_JSON(json);
        OPEN_ELEMENT_OF_MONITORS_JSON(json);
        GOTO_ELEMENT_OF_MONITORS_JSON(json, "app-name",0);
        if(ret != -1)
        {
          GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
          strcpy(temp_appname, dummy_ptr);
          //this check is for json diff when we change the app-name in app-name tag
          if(process_diff_json == 0)
          {
            if(strcasecmp(dummy_ptr,"default"))
            {
              CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
              continue;
            }
          }
          else
          {
            if (strcmp(temp_appname,"default") != 0)
              new_pod_entry = 1;
          }
        }
        else
          ret=0;
 
        GOTO_ELEMENT_OF_MONITORS_JSON(json, "cmon-pod-pattern",0);
        if(ret !=-1)
        {
          GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
          MY_MALLOC_AND_MEMSET(json_info_ptr->cmon_pod_pattern, (len + 1), "JSON Info Ptr cmon-pod-pattern", -1);
          strcpy(json_info_ptr->cmon_pod_pattern, dummy_ptr);
        }
        else
          ret=0;

        GOTO_ELEMENT_OF_MONITORS_JSON(json, "java-home",0);
        if(ret !=-1)
        {
          GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
          MY_MALLOC_AND_MEMSET(json_info_ptr->javaHome, (len + 1), "JSON Info Ptr JavaClasspath", -1);
          strcpy(json_info_ptr->javaHome, dummy_ptr);
        }
        else
          ret=0;

        GOTO_ELEMENT_OF_MONITORS_JSON(json, "java-classpath",0);
        if(ret !=-1)
        {
          GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
          MY_MALLOC_AND_MEMSET(json_info_ptr->javaClassPath, (len + 1), "JSON Info Ptr JavaClasspath", -1);
          strcpy(json_info_ptr->javaClassPath, dummy_ptr);
        }
        else
          ret=0;

        GOTO_ELEMENT_OF_MONITORS_JSON(json, "TierServerMappingType",0);
        if(ret !=-1)
        {
          GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
          if(!strncasecmp(dummy_ptr, "standard", 8))
            json_info_ptr->tier_server_mapping_type |= STANDARD_TYPE;
          else if(!strncasecmp(dummy_ptr, "custom", 6))
            json_info_ptr->tier_server_mapping_type |= CUSTOM_TYPE;
        }
        else
          ret=0;


        GOTO_ELEMENT_OF_MONITORS_JSON(json, "VectorReplaceFrom",0);
        if(ret != -1)
        {
          GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
          MY_MALLOC_AND_MEMSET(json_info_ptr->vectorReplaceFrom, (len + 1), "JSON Info Ptr VectorReplaceFrom", -1);
          strcpy(json_info_ptr->vectorReplaceFrom, dummy_ptr);
        }
        else
          ret = 0;

	GOTO_ELEMENT_OF_MONITORS_JSON(json, "run-as-process",0);
	if(ret !=-1)
	{
	  GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
	  if( !strcasecmp(dummy_ptr,"true"))
	  {
	    json_info_ptr->is_process = 1;
	  }
	}
	else
	  ret=0;


        GOTO_ELEMENT_OF_MONITORS_JSON(json, "VectorReplaceTo",0);
        if(ret != -1)
        {
          GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
          MY_MALLOC_AND_MEMSET(json_info_ptr->vectorReplaceTo, (len + 1), "JSON Info Ptr VectorReplaceTo", -1);
          strcpy(json_info_ptr->vectorReplaceTo, dummy_ptr);
        }
        else
          ret=0;

        /*GOTO_ELEMENT_OF_MONITORS_JSON(json, "options",1);
        GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
        strcpy (prgrm_args, dummy_ptr);*/
  
        GOTO_ELEMENT_OF_MONITORS_JSON(json, "options",1);
        if(ret!=-1)
        {
          GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
          MALLOC_AND_COPY(dummy_ptr, json_info_ptr->options, len + 1, "copy monitor name", -1);
        }
        else
        {
          MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"not provided argument: options of std-mon %s for tier: %s", json_info_ptr->mon_name, temp_tiername);
          ret=0;
        }

        GOTO_ELEMENT_OF_MONITORS_JSON(json, "old-options",0);
        if(ret!=-1)
        {
          GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
          MALLOC_AND_COPY(dummy_ptr, json_info_ptr->old_options, len + 1, "copy monitor name", -1);
        }
        else
        {
          if(json_info_ptr->options != NULL)
          {
            MALLOC_AND_COPY(json_info_ptr->options, json_info_ptr->old_options, strlen(json_info_ptr->options) + 1, "copy monitor name", -1);
          }
          else 
          {
            MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"not provided argument: old-options of std-mon %s for tier: %s", json_info_ptr->mon_name, temp_tiername);
          }
         ret=0;
        }
    
        if(dest_any_server[0] != '\0')
        {
          json_info_ptr->dest_any_server_flag = true;
          json_info_ptr->no_of_dest_any_server_elements=get_tokens(dest_any_server, json_info_ptr->dest_any_server_arr, "," , MAX_NO_OF_APP);
        }
         
        //appname must not be default
        if(new_pod_entry == 1)
          //this function is for kubernetes for apply monitor on pod name
          add_delete_kubernetes_monitor_on_process_diff(temp_appname, temp_tiername, mon_name, delete_flag);
        else if(delete_flag)
          //make function to remove entry from dest_tier structure of monitor.
          delete_add_monitor_from_json(exclude_tier, exclude_server, specific_server, temp_tiername, mon_name, prgrm_args, instance, json_info_ptr, process_diff_json, DELETE_STD_MON_TYPE, NULL, tier_type, tier_group_index, json_mp_ptr);
        else
          delete_add_monitor_from_json(exclude_tier, exclude_server, specific_server, temp_tiername, mon_name, prgrm_args, instance, json_info_ptr, process_diff_json, STD_MON_TYPE, NULL, tier_type, tier_group_index, json_mp_ptr);
        
        
        FREE_AND_MAKE_NULL(json_info_ptr->vectorReplaceTo, "vectorReplaceTO in JSON", -1);
        FREE_AND_MAKE_NULL(json_info_ptr->vectorReplaceFrom, "Freeing vectorReplaceFrom", -1);
        FREE_AND_MAKE_NULL(json_info_ptr->init_vector_file, "Freeing initVectorFile", -1);
        FREE_AND_MAKE_NULL(json_info_ptr->javaHome, "Freeing javaHome", -1);
        FREE_AND_MAKE_NULL(json_info_ptr->javaClassPath, "Freeing javaClasspath", -1);
        FREE_AND_MAKE_NULL(json_info_ptr->cmon_pod_pattern, "Freeing cmonpodpattern", -1);
        CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);

      }

      FREE_AND_MAKE_NULL(json_info_ptr, "Freeing Json Element", -1);

      if(ret == -1)
      {
        if(((int)json->error)!=6)
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                         "Can't able to apply monitors on Tier [%s] from json [%s] due to error: %s.", temp_tiername, global_settings->auto_json_monitors_filepath, nslb_json_strerror(json));
          return -1;
        }
        else
        {
          CLOSE_ELEMENT_ARR_OF_MONITORS_JSON(json);
          CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
          continue;
        }
      }
      CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json)
    }

   CLOSE_ELEMENT_ARR_OF_MONITORS_JSON(json);
   CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
  }
  
  if(ret == -1)
  {
    if(((int)json->error)!=6)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                       "Can't able to apply monitors on Tier [%s] from json [%s] due to error: %s.", temp_tiername, global_settings->auto_json_monitors_filepath, nslb_json_strerror(json));
      return -1;
    }
    else
      return 0;
  }

  if(json_mp_ptr)
  {
    for(i = 0; i < total_enteries; i++)
    {
      FREE_AND_MAKE_NULL(json_mp_ptr[i].tier,"Freeing tier", -1 );
      if(json_mp_ptr[i].server_mp_ptr != NULL )
      {
       for(j=0 ; j<json_mp_ptr[i].total_server_entries ; j++ )
       {
        FREE_AND_MAKE_NULL(json_mp_ptr[i].server_mp_ptr[j].server, "Freeing server", -1);
       }
      }    
    }
    FREE_AND_MAKE_NULL(json_mp_ptr[i].server_mp_ptr, "Freeing server ptr", -1);
    FREE_AND_MAKE_NULL(json_mp_ptr, "Freeing json_mp_ptr", -1);
  }

  return 0;
}

int read_json_and_apply_monitor(int process_diff_json, char process_global_vars)
{
  nslb_json_t *json = NULL;
  nslb_json_error err;
  int delete_flag = 0;
  char *dummy_ptr=NULL;
  int len=0,ret=0;
  char json_monitors_filepath[MAX_DATA_LINE_LENGTH];

  NSDL2_MON(NULL, NULL, "Method called, process_diff_json = %d, process_global_vars = %d", process_diff_json, process_global_vars);

  if(process_diff_json)
  {
    json = nslb_json_init_buffer(auto_json_monitors_diff_ptr, 0, 0, &err);
    strcpy(json_monitors_filepath, global_settings->auto_json_monitors_filepath);
  }
  else
  {
    json = nslb_json_init_buffer(auto_json_monitors_ptr, 0, 0, &err);
    strcpy(json_monitors_filepath, global_settings->auto_json_monitors_filepath);
  }

  MLTL4(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "JSON =%p",json); 
  if(json == NULL)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                       "Unable to convert json content of file [%s] to json structure, due to: %s.", 
                        global_settings->auto_json_monitors_filepath, nslb_json_strerror(json));
    return -1;
  }


  if(process_diff_json)
  {
    OPEN_ELEMENT_OF_MONITORS_JSON(json);

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "MetricPriority", 0);

    if(ret != -1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON_EX(json, dummy_ptr, &len, 0, 1);
      if(len > 0)
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Found field 'MetricPriority' in file %s. This field is not supported in Runtime Changes.");
        //set_g_metric_priority(dummy_ptr);
      }
    }
    else
      ret = 0;

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "runtime", 1);
    if(ret == -1)
        return -1;

    OPEN_ELEMENT_OF_MONITORS_JSON(json);
    if(ret == -1)
    {
      if(((int)json->error)!=6)
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                       "Can't able to open runtime element in json [%s] while applying json auto monitors due to error: %s.", global_settings->auto_json_monitors_diff_filepath, nslb_json_strerror(json));
        return -1;
      }
    }
    while(1)
    {  
      delete_flag = 0;
      GOTO_NEXT_ARRAY_ITEM_OF_MONITORS_JSON(json);
      OPEN_ELEMENT_OF_MONITORS_JSON(json);
      //type is add / delete. If no type specified process as add
      GOTO_ELEMENT_OF_MONITORS_JSON(json, "type", 1);
      {
        if(ret!=-1)
        {
          GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
          if( !strcasecmp(dummy_ptr,"deleted"))
             delete_flag=1; 
        }
        else
        {
          ret=0;
          CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
          continue;
        }
      }
      add_json_monitors_ex(process_diff_json, json, delete_flag);
      if(json)
      {
        CLOSE_ELEMENT_ARR_OF_MONITORS_JSON(json);
        CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
      }
      else
        break;
    }
  }
  else //Handle non-runtime
  {
    OPEN_ELEMENT_OF_MONITORS_JSON(json);

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "MetricPriority", 0);
    if(ret != -1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON_EX(json, dummy_ptr, &len, 0, 1);
      if(len > 0)
        g_metric_priority = get_metric_priority_id(dummy_ptr, 0);
    }
    else
      ret = 0;
  
    //If process_global_vars flag is set ,it means we do not need to apply monitors from JSON. We will set global vars only
    if(process_global_vars)
    {
      CLOSE_JSON_ELEMENT(json);
      return 0;
    }

    add_json_monitors_ex(process_diff_json, json, delete_flag);    
  }

  NSDL2_MON(NULL, NULL, "g_metric_priority = %d", g_metric_priority);

  return 0;
}


//}
/* add_json_monitors(char *vector_name, int server_idx, int tier_idx, char *app_name, int runtime_flag)
 * This function is used to apply monitor as the json configured for the monitors on the basis of the tier.
 * vector_name and app_name are passed in the case of the kubernetes.
 * If vector_name and app_name are null that means it is the case of normal VM.
 * If it is the case of kubernetes then we will extract different part of vector_name to get names , IPs, creations time of nodes and pods and container ID using function parse_kubernetes_vector_format.
 * We apply monitor on node we extracted from vector in case of kubernetes instead of the Server.
 */

int runtime_process_log_monitors(nslb_json_t *json, char *vector_name, int server_idx, int tier_idx, int runtime_flag, int process_log_mon, int no_of_token_elements, char **token_arr, int et, char *node_name, char *node_ip, char *pod_name, int mon_row, char *json_monitors_filepath)
{
  char err_msg[MAX_DATA_LINE_LENGTH];
  char prgrm_args[16*MAX_DATA_LINE_LENGTH];
  char monitor_buf[32*MAX_DATA_LINE_LENGTH];
  char temp_monitor_buf[32*MAX_DATA_LINE_LENGTH];
  char mon_name[MAX_DATA_LINE_LENGTH];
  char gdf_name[MAX_DATA_LINE_LENGTH];
  char *dummy_ptr=NULL;
  int len,ret = 0;
  char instance_name[MAX_DATA_LINE_LENGTH];
  JSON_info *json_info_ptr=NULL;
 
  sprintf(instance_name, " ");
  while (process_log_mon)
  {
    ret=0;
    prgrm_args[0]='\0';
    gdf_name[0] = '\0';
    GOTO_NEXT_ARRAY_ITEM_OF_MONITORS_JSON(json);
    OPEN_ELEMENT_OF_MONITORS_JSON(json);

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "enabled",0);
    {
      if(ret!=-1)
      {
        GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
        if( !strcasecmp(dummy_ptr,"false"))
        {
          CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
          continue;
        }
      }
      else
        ret=0;
    }

    MY_MALLOC_AND_MEMSET(json_info_ptr, sizeof(JSON_info), "JSON Info Ptr", -1);

    CHECK_OPTIONAL_FEATURES_IN_JSON(json,ret,instance_name,tier_idx);
   
    //here frequency is monitor interval
    GOTO_ELEMENT_OF_MONITORS_JSON(json, "frequency",0);
    {
      if(ret != -1)
      {
        GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
        json_info_ptr->frequency = atoi(dummy_ptr) *1000;
      }
      else
         ret=0;
    }

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "log-mon-name",1);
    if(ret!=-1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      strcpy(mon_name,dummy_ptr);
    }
    else
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"not provided argument: log-mon-name for tier: %s ",topo_info[topo_idx].topo_tier_info[tier_idx].tier_name);
      CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
      continue;
    }

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "gdf-name",1);
    if(ret!=-1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      strcpy(gdf_name,dummy_ptr);
      //strcat(prgrm_args,dummy_ptr);
      //strcat(prgrm_args," ");
    }
    else
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"not provided argument: gdf-name of log-mon-name %s for tier: %s",mon_name,topo_info[topo_idx].topo_tier_info[tier_idx].tier_name);
      CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
      continue;
    }
    GOTO_ELEMENT_OF_MONITORS_JSON(json, "run-option",0);
    if(ret!=-1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      strcat(prgrm_args,dummy_ptr);
      strcat(prgrm_args," ");
    }
    else
    {
      strcat(prgrm_args,"2");
      strcat(prgrm_args," ");
      //MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"not provided argument: run-option of log-mon-name %s for tier: %s ",mon_name,topo_tier_info[tier_idx].TierName);
      //CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
      //continue;
      ret=0;
    }
    if(process_log_mon == 1 )
      strcat(prgrm_args,"java cm_log_parser");
    else if(process_log_mon == 2)
      strcat(prgrm_args,"cm_get_log_file");
    else if(process_log_mon == 3)
      strcat(prgrm_args,"java cm_file");

    strcat(prgrm_args," ");
    /*GOTO_ELEMENT_OF_MONITORS_JSON(json, "options",1);
    if(ret!=-1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      strcat(prgrm_args,dummy_ptr);
    }
    else
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"not provided argument: options of log-mon-name %s for tier: %s",mon_name,topo_tier_info[tier_idx].TierName);
      CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
      continue;
    }*/

    //TODO check cmon verion if cmon version is greater than default value then go for new-option otherwise go for old-option
    if(topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->cmon_option_flag == 0)
    { 
      GOTO_ELEMENT_OF_MONITORS_JSON(json, "old-options",0);
      if(ret!=-1)
      {
        GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
        strcat(prgrm_args, dummy_ptr);
      }
      else
      {
        GOTO_ELEMENT_OF_MONITORS_JSON(json, "options",1);
        if(ret!=-1)
        {
          GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
          strcat(prgrm_args, dummy_ptr);
        }
      }
    }
    else
    {
      GOTO_ELEMENT_OF_MONITORS_JSON(json, "options",1);
      if(ret!=-1)
      {
        GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
        strcat(prgrm_args, dummy_ptr);
      }
      else
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"not provided argument: options of log-mon-name %s for tier: %s ",mon_name, topo_info[topo_idx].topo_tier_info[tier_idx].tier_name);
        CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
        continue;
      }
    }

    if(instance_name[0] == '\0')
      sprintf(monitor_buf,"LOG_MONITOR %s%c%s %s %s%c%s%s %s",topo_info[topo_idx].topo_tier_info[tier_idx].tier_name, global_settings->hierarchical_view_vector_separator, topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->server_disp_name, gdf_name, topo_info[topo_idx].topo_tier_info[tier_idx].tier_name, global_settings->hierarchical_view_vector_separator, topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_disp_name, instance_name, prgrm_args);
    else
      sprintf(monitor_buf,"LOG_MONITOR %s%c%s %s %s%c%s%c%s %s",topo_info[topo_idx].topo_tier_info[tier_idx].tier_name, global_settings->hierarchical_view_vector_separator, topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->server_disp_name, gdf_name, topo_info[topo_idx].topo_tier_info[tier_idx].tier_name, global_settings->hierarchical_view_vector_separator,  topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_disp_name, global_settings->hierarchical_view_vector_separator, mon_name, prgrm_args);


    if(runtime_flag)
    {
      //This will allocate from start everytime once it get free 
      create_table_entry_dvm(&mon_row, &total_json_monitors, &max_json_monitors, (char **)&json_monitors_ptr, sizeof(Mon_from_json), "Server Signature to be Applied");

      MY_MALLOC(json_monitors_ptr[mon_row].mon_buff, strlen(monitor_buf) + 1, "log monitor buffer", mon_row);
      strcpy(json_monitors_ptr[mon_row].mon_buff, monitor_buf);
      json_monitors_ptr[mon_row].mon_type = LOG_MONITOR;
      MY_MALLOC(json_monitors_ptr[mon_row].pod_name, strlen(pod_name) + 1, "Pod Name", mon_row);
      strcpy(json_monitors_ptr[mon_row].pod_name, pod_name);
      MY_MALLOC_AND_MEMSET(json_monitors_ptr[mon_row].json_info_ptr, sizeof(JSON_info), "JSON Info Ptr", -1);
      if(json_info_ptr->any_server)
      {
       // MY_MALLOC_AND_MEMSET(json_monitors_ptr[mon_row].json_info_ptr, sizeof(JSON_info), "JSON Info Ptr", -1);

        json_monitors_ptr[mon_row].json_info_ptr->any_server=true;
      }
      json_monitors_ptr[mon_row].json_info_ptr->frequency = json_info_ptr->frequency;
      json_monitors_ptr[mon_row].json_info_ptr->lps_enable = 1;
      MALLOC_AND_COPY(json_info_ptr->tier_name, json_monitors_ptr[mon_row].json_info_ptr->tier_name, strlen(json_info_ptr->tier_name) + 1,
                                                                                                 "JSON INFO ptr g_mon_id", -1);
      json_monitors_ptr[mon_row].json_info_ptr->tier_idx=json_info_ptr->tier_idx;
    }
    else
    {
      strcpy(temp_monitor_buf,monitor_buf);
      if(custom_monitor_config("LOG_MONITOR",monitor_buf, NULL, 0, 1, err_msg, NULL, json_info_ptr, 0) >= 0)
      {
        g_mon_id = get_next_mon_id();
        monitor_added_on_runtime = 1;
      }
      else
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                                    "Failed to apply json auto monitor with buffer: %s . Error: %s", temp_monitor_buf, err_msg);


    }

    topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->auto_monitor_applied = 1;

    CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);

  }

  FREE_AND_MAKE_NULL(json_info_ptr, "Freeing json_info_ptr", -1);  

  return ret;
}

//stripping > bracket from instance name and store in json_info_ptr 
void copy_instance_name_in_json_info_ptr(char *instance, JSON_info *json_info_ptr)
{
  char instance_name[MAX_DATA_LINE_LENGTH];
  char strip_instance_name[MAX_DATA_LINE_LENGTH];

  char *ptr=NULL;

  strcpy(instance_name, instance);

  if((ptr=strstr(instance_name, ">")))
  {
    strcpy(strip_instance_name, ptr + 1);
    MALLOC_AND_COPY(strip_instance_name, json_info_ptr->instance_name, strlen(strip_instance_name)+1, "instance name", -1);
  }
}

//This is an array element in JSON, which is used to provide custom gdf. User can provide customised gdf and program name and that when provides data will be shown on dashboard.

int runtime_process_custom_gdf_mon(nslb_json_t *json, char *vector_name, int server_idx, int tier_idx, int runtime_flag, int process_custom_gdf_mon, int no_of_token_elements, char **token_arr, int et, char *node_name, char *node_ip, char *pod_name, int mon_row, char *json_monitors_filepath)
{
  char gdf_name[MAX_DATA_LINE_LENGTH];
  char *dummy_ptr=NULL;
  int variable_replaced = 0;
  char cfile[512];
  int len,ret = 0 ;
  char instance_name[1024]="\0";
  char java_home_and_classpath[1024]={0};
  char tmp_vector_name[MAX_DATA_LINE_LENGTH];
  char dyn_mon_buf[64*MAX_DATA_LINE_LENGTH];
  char dvm_data_arg_buf[] = "-L data";
  char dvm_vector_arg_buf[] = "-L header";
  char java_args[16*MAX_BUF_SIZE_FOR_STD_MON] = {0};
  JSON_info *json_info_ptr=NULL;
  char prgrm_args[16*MAX_DATA_LINE_LENGTH];
  prgrm_args[0] = '\0';
   
  /*if(!json_info_ptr)
    MY_MALLOC_AND_MEMSET(json_info_ptr, sizeof(JSON_info), "JSON Info Ptr", -1);
  */

//  sprintf(instance_name, " ");

  while (process_custom_gdf_mon)
  {
    ret=0;
    gdf_name[0] = '\0';
    GOTO_NEXT_ARRAY_ITEM_OF_MONITORS_JSON(json);
    OPEN_ELEMENT_OF_MONITORS_JSON(json);

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "enabled",0);
    {
     
      if(ret!=-1)
      {
        GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
        if( !strcasecmp(dummy_ptr,"false"))
        {
          CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
          continue;
        }
      }
      else
        ret=0;
    }

    MY_MALLOC_AND_MEMSET(json_info_ptr, sizeof(JSON_info), "JSON Info Ptr", -1);

    CHECK_OPTIONAL_FEATURES_IN_JSON(json,ret,instance_name,tier_idx);

    //here frequency is monitor interval
    GOTO_ELEMENT_OF_MONITORS_JSON(json, "frequency",0);
    {
      if(ret != -1)
      {
        GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
        json_info_ptr->frequency = atoi(dummy_ptr) *1000;
      }
      else
         ret=0;
    }

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "program-name", 1);
    if(ret!=-1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      MALLOC_AND_COPY(dummy_ptr,json_info_ptr->mon_name,(len + 1) , "JSON Info Ptr pgm_type", -1);
    }
    else
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"not provided argument: program-name for custom_gdf_mon for tier: %s ", topo_info[topo_idx].topo_tier_info[tier_idx].tier_name);
      CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
      continue;
    }

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "gdf-name", 1);
    if(ret!=-1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      strcpy(gdf_name, dummy_ptr);
    }
    else
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
             "not provided argument: gdf-name of custom_gdf_mon %s for tier: %s",
              json_info_ptr->mon_name,topo_info[topo_idx].topo_tier_info[tier_idx].tier_name); 
      CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
      continue;
    }
    GOTO_ELEMENT_OF_MONITORS_JSON(json, "run-option",0);
    if(ret!=-1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      json_info_ptr->run_opt = atoi(dummy_ptr);
      if((json_info_ptr->run_opt != 1) && (json_info_ptr->run_opt != 2))
      {
        MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
              "run_option provided isnot correct. Hence setting it to 2: program-name of custom-gdf-mon %s for tier: %s", 
               json_info_ptr->mon_name, topo_info[topo_idx].topo_tier_info[tier_idx].tier_name);
        json_info_ptr->run_opt = 2;
      }
    }
    else
    {
      json_info_ptr->run_opt = 2;
      ret=0;
    }

    //TODO check cmon verion if cmon version is greater than default value then go for new-option otherwise go for old-option
    if(topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->cmon_option_flag == 0)
    {
      GOTO_ELEMENT_OF_MONITORS_JSON(json, "old-options",0);
      if(ret == -1)
      {
        GOTO_ELEMENT_OF_MONITORS_JSON(json, "options",1);
      }

    }
    else
    {
      GOTO_ELEMENT_OF_MONITORS_JSON(json, "options",1);
    }

    //GOTO_ELEMENT_OF_MONITORS_JSON(json, "options",0);
    if(ret!=-1)
    {
      variable_replaced |= 0x10;
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      MALLOC_AND_COPY(dummy_ptr,json_info_ptr->args,(len + 1) , "JSON Info Ptr options", -1);
      while((variable_replaced & 0x10) && strchr(dummy_ptr,'%'))
      {
         variable_replaced &= 0x01;
         char *variable_ptr = NULL;
         char leftover_arguments[MAX_DATA_LINE_LENGTH];

         if((variable_ptr=strstr(dummy_ptr,"%server_ip%")))
         {
           strcpy(leftover_arguments, variable_ptr + 11);
           strcpy(variable_ptr,topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->server_name);
           strcpy(variable_ptr + strlen(topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->server_name), leftover_arguments);
           variable_replaced |= 0x11;
         }
         if((variable_ptr=strstr(dummy_ptr,"%server_name%")))
         {
           strcpy(leftover_arguments, variable_ptr + 13);
           strcpy(variable_ptr,topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_disp_name);
           strcpy(variable_ptr + strlen(topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_disp_name), leftover_arguments);
           variable_replaced |= 0x11;
         }
       }
       variable_replaced = 0;
       strcpy(json_info_ptr->args, dummy_ptr);
       strcpy(prgrm_args, dummy_ptr);
    }
    else
    {
      MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
           "option provided isnot correct. Hence setting it to 2: program-name of custom-gdf-mon %s for tier: %s", 
            json_info_ptr->mon_name, topo_info[topo_idx].topo_tier_info[tier_idx].tier_name);
      ret=0;
    }
   
    GOTO_ELEMENT_OF_MONITORS_JSON(json, "cfg", 1);
    if(ret!=-1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      sprintf(cfile,"%s/mprof/.custom/%s", g_ns_wdir,dummy_ptr);
      MALLOC_AND_COPY(cfile,json_info_ptr->config_json,strlen(cfile) + 1 , "JSON Info Ptr cfg", -1);
    }
    else
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
            "not provided argument: config-file-path of custom-gdf-mon %s for tier: %s", 
             json_info_ptr->mon_name,topo_info[topo_idx].topo_tier_info[tier_idx].tier_name);
      CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
      continue;
    }

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "mhp", 0);
    if(ret != -1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      json_info_ptr->skip_breadcrumb_creation = atoi(dummy_ptr);
      if((json_info_ptr->skip_breadcrumb_creation != 1) && (json_info_ptr->skip_breadcrumb_creation != 0))
      {
        MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
              "Metric Hierarchy prefix provided is not correct. Hence setting it to 1: program-name of custom-gdf-mon %s for tier: %s", 
               json_info_ptr->mon_name, 
               topo_info[topo_idx].topo_tier_info[tier_idx].tier_name);
        json_info_ptr->skip_breadcrumb_creation = 0;
      }
    }
    else
    {
      json_info_ptr->skip_breadcrumb_creation = 0;
      ret=0;
    }

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "app-name",1);
    if(ret!=-1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      MY_MALLOC_AND_MEMSET(json_info_ptr->app_name, (len + 1), "JSON Info Ptr app_name", -1);
      strcpy(json_info_ptr->app_name, dummy_ptr);
    }
    else
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
            "not provided argument: app-name %s for tier: %s",
             json_info_ptr->mon_name,topo_info[topo_idx].topo_tier_info[tier_idx].tier_name);
      CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
      continue;
    }
     
    GOTO_ELEMENT_OF_MONITORS_JSON(json, "pgm-type", 1);
    if(ret!=-1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      MALLOC_AND_COPY(dummy_ptr,json_info_ptr->pgm_type,(len + 1) , "JSON Info Ptr pgm_type", -1);
    }
    else
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"not provided argument: pgm-type of custom-gdf-mon %s for tier: %s", json_info_ptr->mon_name, topo_info[topo_idx].topo_tier_info[tier_idx].tier_name);
      CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
      continue; 
    }
 
    GOTO_ELEMENT_OF_MONITORS_JSON(json, "os-type",0);
    if(ret!=-1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      MALLOC_AND_COPY(dummy_ptr,json_info_ptr->os_type,(len + 1) , "JSON Info Ptr os_type", -1);
    }
    else
    {
      MALLOC_AND_COPY("ALL",json_info_ptr->os_type, 4 ,"JSON Info Ptr os_type", -1);
      MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"not provided argument: OS-type of custom-gdf-mon %s for tier: %s. Hence setting default os-type to(LinuxEx)", json_info_ptr->mon_name,topo_info[topo_idx].topo_tier_info[tier_idx].tier_name);
      ret =0;
    }

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "run-as-process",0);
    if(ret !=-1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      if( !strcasecmp(dummy_ptr,"true"))
      {
	json_info_ptr->is_process = 1;
      }
    }
    else
    {
      MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"not provided argument: run-as-process of custom-gdf-mon %s for tier: %s. Hence setting default 0", json_info_ptr->mon_name,topo_info[topo_idx].topo_tier_info[tier_idx].tier_name);
      ret=0;
    }
 
    GOTO_ELEMENT_OF_MONITORS_JSON(json, "java-home",0);
    if(ret!=-1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      MY_MALLOC_AND_MEMSET(json_info_ptr->javaHome, (len + 1), "JSON Info Ptr javaHome", -1);
      strcpy(json_info_ptr->javaHome, dummy_ptr);
    }
    else
    {
      MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"not provided argument: java-home %s for tier: %s",json_info_ptr->mon_name,topo_info[topo_idx].topo_tier_info[tier_idx].tier_name);
      ret=0;
    }

     GOTO_ELEMENT_OF_MONITORS_JSON(json, "java-classpath",0);
    if(ret!=-1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      MY_MALLOC_AND_MEMSET(json_info_ptr->javaClassPath, (len + 1), "JSON Info Ptr java_classPath", -1);
      strcpy(json_info_ptr->javaClassPath, dummy_ptr);
    }
    else
    {
      MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"not provided argument: java-classpath %s for tier: %s",json_info_ptr->mon_name,topo_info[topo_idx].topo_tier_info[tier_idx].tier_name);
      ret=0;
    }

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "use-agent",0);
    if(ret!=-1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      MY_MALLOC_AND_MEMSET(json_info_ptr->use_agent, (len + 1), "JSON Info Ptr use_agent", -1);
      strcpy(json_info_ptr->use_agent, dummy_ptr);
    }
    else
    {
      MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"not provided argument: use-agent %s for tier: %s",json_info_ptr->mon_name,topo_info[topo_idx].topo_tier_info[tier_idx].tier_name);
      ret =0;
      //CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
      //continue;
    }

    json_info_ptr->agent_type |= CONNECT_TO_CMON;
    GOTO_ELEMENT_OF_MONITORS_JSON(json, "agent-type",0);
    {
      if(ret != -1)
      {
        GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
        if(!strcasecmp(dummy_ptr,"BCI"))
        {
          //here unset cmon 
          json_info_ptr->agent_type |= CONNECT_TO_NDC;
          json_info_ptr->agent_type &= ~CONNECT_TO_CMON;
        }
        // We do not want ito start NDC Mbean monitor to be applied at runtime. 
        else if(!strcasecmp(dummy_ptr,"ALL"))
        {
          json_info_ptr->agent_type |= CONNECT_TO_BOTH_AGENT;
        }
      }
      else
        ret=0;
    } 
    
 
   //All check of machine type 
   if(!strcasecmp(topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->machine_type , json_info_ptr->os_type))
   { 
     if(!strcasecmp(json_info_ptr->pgm_type, "JAVA") )
     {
       if(json_info_ptr && json_info_ptr->javaClassPath && json_info_ptr->javaClassPath[0] != '\0')
       {
         if(json_info_ptr->javaHome && json_info_ptr->javaHome[0] != '\0')
         {
           if(json_info_ptr->javaHome[strlen(json_info_ptr->javaHome) - 1] == '/' || json_info_ptr->javaHome[strlen(json_info_ptr->javaHome) - 1] == '\\')
             json_info_ptr->javaHome[strlen(json_info_ptr->javaHome) - 1] = '\0';
  
           if(strcasecmp(topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->machine_type, "Windows"))
           { 
             sprintf(java_home_and_classpath,"%s/bin/java -cp %s",json_info_ptr->javaHome, json_info_ptr->javaClassPath);
           }
           else
             sprintf(java_home_and_classpath,"%s\\bin\\java -cp %s", json_info_ptr->javaHome, json_info_ptr->javaClassPath);
         }
         else
           sprintf(java_home_and_classpath,"java -cp %s", json_info_ptr->javaClassPath);
       }
       else
         strcpy(java_home_and_classpath, "java");
       
       if(instance_name[0]) 
       {
         copy_instance_name_in_json_info_ptr(instance_name, json_info_ptr);         
         sprintf(tmp_vector_name,"%s%c%s%s",topo_info[topo_idx].topo_tier_info[tier_idx].tier_name, global_settings->hierarchical_view_vector_separator,topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_disp_name,instance_name);
       }
       else
       {
         sprintf(tmp_vector_name,"%s%c%s",topo_info[topo_idx].topo_tier_info[tier_idx].tier_name, global_settings->hierarchical_view_vector_separator,topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_disp_name); 
       } 
       sprintf(java_args, "%s -DCAV_MON_HOME=%s -DMON_TEST_RUN=%d -DVECTOR_NAME=%s ",
               java_home_and_classpath,
               topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->cav_mon_home, testidx, tmp_vector_name); 
       if((json_info_ptr->use_agent) && strcasecmp(json_info_ptr->use_agent, "local") == 0)
       {
         sprintf(dyn_mon_buf,"DYNAMIC_VECTOR_MONITOR 127.0.0.1 %s %s %d %s %s %s %s EOC %s %s %s %s",
                           tmp_vector_name, gdf_name, json_info_ptr->run_opt,java_args, json_info_ptr->mon_name, prgrm_args, dvm_data_arg_buf, java_args, json_info_ptr->mon_name, prgrm_args, dvm_data_arg_buf);
       }
       else
       {
         sprintf(dyn_mon_buf,"DYNAMIC_VECTOR_MONITOR %s%c%s %s %s %d %s %s %s %s EOC %s %s %s %s",topo_info[topo_idx].topo_tier_info[tier_idx].tier_name, global_settings->hierarchical_view_vector_separator, topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->server_disp_name,tmp_vector_name, gdf_name, json_info_ptr->run_opt, java_args,json_info_ptr->mon_name,prgrm_args,dvm_data_arg_buf,java_args,json_info_ptr->mon_name, prgrm_args, dvm_data_arg_buf);          
       }
     }  
      // This Buffer is made without program-type "java" and it is dvm
     else 
     {
       sprintf(dyn_mon_buf,"DYNAMIC_VECTOR_MONITOR %s%c%s %s %s %d %s %s %s EOC %s %s ",topo_info[topo_idx].topo_tier_info[tier_idx].tier_name, global_settings->hierarchical_view_vector_separator,topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->server_disp_name,tmp_vector_name, gdf_name,json_info_ptr->run_opt,json_info_ptr->mon_name,json_info_ptr->args, dvm_data_arg_buf,json_info_ptr->mon_name, dvm_vector_arg_buf);
     }
     
     if(runtime_flag)
     {
       //This will allocate from start everytime once it get free 
       create_table_entry_dvm(&mon_row, &total_json_monitors, &max_json_monitors, (char **)&json_monitors_ptr, sizeof(Mon_from_json), "Server Signature to be Applied");

       MALLOC_AND_COPY(dyn_mon_buf,json_monitors_ptr[mon_row].mon_buff, strlen(dyn_mon_buf) + 1 ,"dynamic mon buf", -1);
       json_monitors_ptr[mon_row].mon_type = CUSTOM_GDF_MON;
       MY_MALLOC(json_monitors_ptr[mon_row].pod_name, strlen(pod_name) + 1, "Pod Name", mon_row);
       json_monitors_ptr[mon_row].json_info_ptr = json_info_ptr;
       //json_monitors_ptr[mon_row].json_info_ptr->frequency = json_info_ptr->frequency;
       strcpy(json_monitors_ptr[mon_row].pod_name, pod_name);
     }
     else
     {
       MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                          "this log will come in case of some error. This funcion is called at the time of node_discovery. And it is a case of runtime processing. This log has been written for initial code execution");
     }
      topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->auto_monitor_applied = 1;

      CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
    }
  }
  //FREE_AND_MAKE_NULL(json_info_ptr, "Freeing json_info_ptr", -1);
  
  return ret;
}


/*
MACRO NAME: DECODE_FILE_NAME(input)

->input: "-f %2Fhome%2Fcavisson%2Fwork%2F%25pod_name%25%2FFILE1 -N 1 -F request:3,respTime:7,statusCode:5,respSize:6 -S \"space\"  -j access_log.v1ready -h 0 -u mls -A TVS>OverAll -X tvs"

->input is parsed for "-f" argument as it consist of file path which is needed to be decoded. If "-f" argument is not found, then decoding willn not be done.

-> ptr is pointed to the value passed with -f argument, which is passed to the function decode_file_name and its output is collected in output buffer. Then, whole decoded string is gathered in temp buffer to which input will be pointing at the end of MACRO. 

-> input after decoding has been done: "-f /home/cavisson/work/%pod_name%/FILE1 -N 1 -F request:3,respTime:7,statusCode:5,respSize:6 -S \"space\"  -j access_log.v1ready -h 0 -u mls -A TVS>OverAll -X tvs" 
*/
#define DECODE_FILE_NAME(input)\
{\
  char *p1, *ptr;\
  char temp[32*MAX_DATA_LINE_LENGTH];\
  char output[MAX_DATA_LINE_LENGTH];\
  if(ptr = strstr(input, "-f")){\
    ptr=ptr+2;\
    while(strchr(ptr, ' ')){\
      ptr++;  /*to skip all the blank spaces in between "-f" argument and the value passed to it.*/\
      if(*ptr != ' ')\
        break;\
    }\
    p1 = strchr(ptr, ' ');\
    if (p1) /*-f is used in the middle of arguements*/\
      *p1='\0';\
    if(decode_file_name(ptr, output) != 0)     /*if p1 is NULL it means -f is used at end.*/\
    {\
      NSTL2(NULL, NULL, "Unable decode file name: %s", ptr);\
      continue;\
    }\
    if(p1) \
      *p1 = ' ';\
    snprintf(temp, (ptr - input), "%s", input); \
    if(p1)\
      sprintf(temp, "%s %s %s", temp, output, p1);\
    else\
      sprintf(temp, "%s %s", temp, output);\
    input = temp;\
  }\
}

int check_if_tier_exist(char *tier, int total_mp_enteries, JSON_MP *json_mp_ptr)
{
  int i;

  for(i=0; i<total_mp_enteries; i++)
  {
    if(!strcmp(json_mp_ptr[i].tier, tier))
      return i;
  }
  return -1;
}


/*
function name: copy_vector_data_in_string
Description: This function is used to copy field array entrires to input array.
Which consist node_ip, node_name, pod_ip, pod_name, container_name.
*/

kub_ip_mon_vector_fields *copy_vector_data_in_string(char *field[10])
{
  char* ptr = NULL;
  kub_ip_mon_vector_fields *vector_fields_ptr;

  vector_fields_ptr = (kub_ip_mon_vector_fields *)malloc(NUM_KUBE_VECTOR_FIELDS * sizeof(kub_ip_mon_vector_fields));
  strcpy(vector_fields_ptr[0].input, field[1]);
  strcpy(vector_fields_ptr[0].pattern,"node_ip");
  strcpy(vector_fields_ptr[1].input, field[2]);
  strcpy(vector_fields_ptr[1].pattern,"node_name");
  strcpy(vector_fields_ptr[2].input, field[4]);
  strcpy(vector_fields_ptr[2].pattern, "pod_ip");
  strcpy(vector_fields_ptr[3].input, field[5]);
  strcpy(vector_fields_ptr[3].pattern, "pod_name");
  strcpy(vector_fields_ptr[4].input, field[6]);
  strcpy(vector_fields_ptr[4].pattern, "container_id");
  if(field[8] != NULL)
  {
    strcpy(vector_fields_ptr[6].input, field[8]);
    strcpy(vector_fields_ptr[6].pattern, "node_region");
  }
  if(field[9] != NULL)
  {
    strcpy(vector_fields_ptr[7].input, field[9]);
    strcpy(vector_fields_ptr[7].pattern, "node_zone");
  }

  // part_pod_name
  strcpy(vector_fields_ptr[5].input, field[5]);
  ptr=strrchr(vector_fields_ptr[5].input,'-');
  if(ptr)
    *ptr='\0';
  strcpy(vector_fields_ptr[5].pattern, "part_pod_name");

  return vector_fields_ptr;
}

/*
function name: search_replace_chars
Arguments:
            we have to send input data buffer as argument.
            out_buf is to save encode string, initally out_buf is null.
            input_patter consist string through, which we have to encode the dummy_ptr.
            pattern consist the string node_name, node_ip, pod_name pod_ip, container_id.
 Example:
          -N 1 -F request:3,respTime:7,statusCode:5,respSize:6 -S \"space\" -j access_log.v1ready -h 0 -u mls -A TVS>OverAll -X tvs -f %2Fhome%2Fca              visson%2Fwork%2F%25node_ip%25%2F%node_name%%2F%25pod_name%25%2FFILE1
Return:
          1 - successfully parsed the input string ,  0 - for error.
 Description: In this function we are supplying input buffer. which consists of substring
              like pod_ip, pod_name, node_ip, node_name, server_ip, server_name, container_id.
              If matched then we will replace with the actual names.
 Assumptions :
               There is only one delimiter, which consist two sequence "%" or "%25".
*/


int search_replace_chars(char **dummy_ptr , char *delim , char *out_buf, kub_ip_mon_vector_fields *vector_fields_ptr)
{
  //tmp_ptr is the pointer, which search for pair of "%"
  char *ptr=*dummy_ptr,*temp_ptr=NULL;
  int delim_len = strlen(delim);
  MLTL3(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "dummy_ptr befor decoding = %s", *dummy_ptr);
  MLTL3(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "value of i in for loop %d", NUM_KUBE_VECTOR_FIELDS);
  //out_buf will consist, the traversed string and decoded part also.
  int  out_idx=0; // must start with 0
  int p,offset=1, type=0;

  //This while loop will traversed the Whole string till NULL
  while(*ptr != '\0') 
  {
    out_buf[out_idx]=*ptr;

    //Searching for "%"
    if(*ptr == delim[0])
    {
      //Searching for "2" && "5"
      if(*(ptr+1) == delim[1] && *(ptr+2) == delim[2])
      {
        type = 1;
        offset = delim_len  -1;
        ptr = ptr + delim_len;
        temp_ptr = ptr;
      }
      //else is for "%" and other combinations
      else
      {
        type = 2; 
        offset = 1 ;
        ptr++;
        temp_ptr = ptr;
      }
      // loop until we get '%' or null
      while(*temp_ptr != delim[0]) 
      {
        if(*temp_ptr == '\0') 
        {
          type=0;
          out_buf[++out_idx]=*ptr;
          break;//return remaing buff 
        }
        temp_ptr++;
      }
      if((type == 1 && *(temp_ptr+1) == delim[1] && *(temp_ptr+2) == delim[2]) || type == 2)
      {
        *temp_ptr = '\0';
        for(p = 0; p < NUM_KUBE_VECTOR_FIELDS; p++)
        {
          if(!strcmp(ptr, vector_fields_ptr[p].pattern))
          {
            out_buf[out_idx]='\0';
            strcat(out_buf, vector_fields_ptr[p].input);
            out_idx = strlen(out_buf) - 1;
            if(type == 1)
              ptr= temp_ptr + offset;
            if(type == 2)
              ptr= temp_ptr;
            break;
          }
        }
        if(p == NUM_KUBE_VECTOR_FIELDS)
        {
          out_buf[++out_idx]=*ptr;
        }
        *temp_ptr = delim[0];
      }
    }
    out_idx++;
    ptr++;
  }
  out_buf[out_idx]='\0';
  MLTL3(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "dummy_ptr after decode in out_buf= %s",out_buf);
  if(!(strcmp(*dummy_ptr,out_buf)))
  {
    MLTL3(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "dummy_ptr, if no changes are their in dummy_ptr= %s",*dummy_ptr);
    return 0;
  }
  else
  {  
    *dummy_ptr = out_buf;
    MLTL3(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "dummy_ptr after change, now out_buf is pointing to dummy_ptr = %s",*dummy_ptr); 
    return 1;
  }
}



int runtime_process_batch_job_and_check_mon(nslb_json_t *json, int server_idx, int tier_idx, int runtime_flag, int process_check_mon ,int process_batch_job, int no_of_token_elements, char **token_arr, int et, char *pod_name, int mon_row, char *json_monitors_filepath)
{
  char *dummy_ptr=NULL;
  int len,ret = 0 ;
  char instance_name[MAX_DATA_LINE_LENGTH];
  char monitor_buf[32*MAX_DATA_LINE_LENGTH];
  char temp_monitor_buf[32*MAX_DATA_LINE_LENGTH];
  char mon_name[MAX_DATA_LINE_LENGTH];
  char err_msg[MAX_DATA_LINE_LENGTH];

  JSON_info *json_info_ptr=NULL;
  char prgrm_args[16*MAX_DATA_LINE_LENGTH];
  prgrm_args[0] = '\0';

  sprintf(instance_name, " ");

  while (process_check_mon || process_batch_job)
  {
    ret=0;
    prgrm_args[0]='\0';
    GOTO_NEXT_ARRAY_ITEM_OF_MONITORS_JSON(json);
    OPEN_ELEMENT_OF_MONITORS_JSON(json);

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "enabled",0);
    {
      if(ret!=-1)
      {
        GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
        if( !strcasecmp(dummy_ptr,"false"))
        {
          CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
          continue;
        }
      }
      else
       ret=0;
    }

    MY_MALLOC_AND_MEMSET(json_info_ptr, sizeof(JSON_info), "JSON Info Ptr", -1);

    //here we get instance_name with > bracket i.e ">instance"
    CHECK_OPTIONAL_FEATURES_IN_JSON(json,ret,instance_name,tier_idx);

    if(process_batch_job == 0)
    {
      GOTO_ELEMENT_OF_MONITORS_JSON(json, "check-mon-name",1);
    }
    else
    {
      GOTO_ELEMENT_OF_MONITORS_JSON(json, "batch-job-mon",1);
    }

    if(ret!=-1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      strcpy(mon_name,dummy_ptr);
    }
    else
    {
      if (process_batch_job == 0)
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"not provided argument: check-mon-name for tier :%s",
              topo_info[topo_idx].topo_tier_info[tier_idx].tier_name);
      }
      else
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"not provided argument: batch-job-mon for tier :%s",
              topo_info[topo_idx].topo_tier_info[tier_idx].tier_name);
      }
      CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
      continue;
    }

    ret=0;
    //TODO check cmon verion if cmon version is greater than default value then go for new-option otherwise go for old-option
    if(topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->cmon_option_flag == 0)
    {
      GOTO_ELEMENT_OF_MONITORS_JSON(json, "old-options",0);
      if(ret == -1)
      {
        GOTO_ELEMENT_OF_MONITORS_JSON(json, "options",1);
      }
    }
    else
    {
      GOTO_ELEMENT_OF_MONITORS_JSON(json, "options",1);
    }

    GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
    strcpy(prgrm_args, dummy_ptr);

    if(process_batch_job ==0)
    {
      sprintf(monitor_buf,"CHECK_MONITOR %s %s%c%s%c%s %s",
             topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->server_name ,
             topo_info[topo_idx].topo_tier_info[tier_idx].tier_name ,
             global_settings->hierarchical_view_vector_separator ,
             topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_disp_name , 
             global_settings->hierarchical_view_vector_separator ,mon_name ,prgrm_args);
    }
    else
    {
      sprintf(monitor_buf,"BATCH_JOB %s%c%s %s %s",
              topo_info[topo_idx].topo_tier_info[tier_idx].tier_name, 
              global_settings->hierarchical_view_vector_separator ,
              topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->server_name ,mon_name ,prgrm_args);    
    }
   
    if(json_info_ptr)
    {
      if(instance_name[0] != '\0')
      {
        //instance_name come with > bracket i.e ">instance" so we store instance_name in json_info_ptr without > bracket
        //we use json_info_ptr->instance_name in any_server case.
        copy_instance_name_in_json_info_ptr(instance_name, json_info_ptr);
      }

      if(mon_name[0] != '\0')
      {
        MY_MALLOC_AND_MEMSET(json_info_ptr->mon_name, strlen(mon_name), "JSON Info Ptr", -1);
        strcpy(json_info_ptr->mon_name, mon_name);
      }

      if(prgrm_args[0] != '\0')
      {
        MY_MALLOC_AND_MEMSET(json_info_ptr->args, strlen(prgrm_args), "JSON Info Ptr", -1);
        strcpy(json_info_ptr->args, prgrm_args);
      }

    }

    if(runtime_flag)
    {
      //This will allocate from start everytime once it get free 
      create_table_entry_dvm(&mon_row, &total_json_monitors, &max_json_monitors, (char **)&json_monitors_ptr, sizeof(Mon_from_json), "Check Monitors to be Applied");

      MY_MALLOC(json_monitors_ptr[mon_row].mon_buff, strlen(monitor_buf) + 1, "Check monitor buffer", mon_row);
      strcpy(json_monitors_ptr[mon_row].mon_buff, monitor_buf);
      if (process_batch_job == 0)
        json_monitors_ptr[mon_row].mon_type = CHECK_MONITOR;
      else
        json_monitors_ptr[mon_row].mon_type = BATCH_JOB;

      MY_MALLOC(json_monitors_ptr[mon_row].pod_name, strlen(pod_name) + 1, "Pod Name", mon_row);
      strcpy(json_monitors_ptr[mon_row].pod_name, pod_name);
      MY_MALLOC_AND_MEMSET(json_monitors_ptr[mon_row].json_info_ptr, sizeof(JSON_info), "JSON Info Ptr", -1); 
      if(json_info_ptr->instance_name)
      {
        MY_MALLOC_AND_MEMSET(json_monitors_ptr[mon_row].json_info_ptr->instance_name, strlen(json_info_ptr->instance_name), "JSON Info Ptr", -1);
        strcpy(json_monitors_ptr[mon_row].json_info_ptr->instance_name, json_info_ptr->instance_name);
      }
      MY_MALLOC_AND_MEMSET(json_monitors_ptr[mon_row].json_info_ptr->mon_name, strlen(mon_name), "JSON Info Ptr", -1);
      strcpy(json_monitors_ptr[mon_row].json_info_ptr->mon_name, mon_name);

      MY_MALLOC_AND_MEMSET(json_monitors_ptr[mon_row].json_info_ptr->args, strlen(prgrm_args), "JSON Info Ptr", -1);
      strcpy(json_monitors_ptr[mon_row].json_info_ptr->args, prgrm_args);

      if(json_info_ptr->any_server)
      {
        json_monitors_ptr[mon_row].json_info_ptr->any_server=true;
      }
      MALLOC_AND_COPY(topo_info[topo_idx].topo_tier_info[tier_idx].tier_name, json_monitors_ptr[mon_row].json_info_ptr->tier_name, strlen(topo_info[topo_idx].topo_tier_info[tier_idx].tier_name),"JSON INFO ptr g_mon_id", -1);
    json_monitors_ptr[mon_row].json_info_ptr->tier_idx=tier_idx;
    MALLOC_AND_COPY(topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_disp_name, json_monitors_ptr[mon_row].json_info_ptr->server_name, strlen(topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_disp_name) + 1,"server", -1);
    json_monitors_ptr[mon_row].json_info_ptr->server_index=server_idx;

     }
    else
    {
      strcpy(temp_monitor_buf,monitor_buf);
      if(kw_set_check_monitor("CHECK_MONITOR", monitor_buf, runtime_flag, err_msg, json_info_ptr) < 0 )
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                                      "Failed to apply json auto monitor with buffer: %s . Error: %s", temp_monitor_buf, err_msg);
    }
    topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->auto_monitor_applied = 1;

    CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);

    FREE_AND_MAKE_NULL(json_info_ptr->instance_name, "json_info_ptr->instance_name", -1);
    FREE_AND_MAKE_NULL(json_info_ptr->mon_name, "json_info_ptr->mon_name", -1);
    FREE_AND_MAKE_NULL(json_info_ptr->args, "json_info_ptr->args", -1);
    FREE_AND_MAKE_NULL(json_info_ptr, "json_info_ptr", -1);
  }
  return ret;
}


//Here we will set the cmon_option_flag is 1 if we get new-options in json So that we will send the options in 
int add_json_monitors(char *vector_name, int server_idx, int tier_idx, char *app_name, int runtime_flag, int ndc_any_server_check, int lps_based_monitors)
{
  char err_msg[MAX_DATA_LINE_LENGTH];
  char path[4*MAX_DATA_LINE_LENGTH];
  char overall_vector[MAX_DATA_LINE_LENGTH];
  char temp_buff[32*MAX_DATA_LINE_LENGTH];
  char part_pod_name[MAX_DATA_LINE_LENGTH];
  char prgrm_args[16*MAX_DATA_LINE_LENGTH];
  char monitor_buf[32*MAX_DATA_LINE_LENGTH];
  char dest_any_server[MAX_DATA_LINE_LENGTH];
  char (*monitor_buf_arr)[32*MAX_DATA_LINE_LENGTH];
  char (*instance_name_arr)[64]=NULL;
  char arr_ret=0;
  int mon_buf_count = 0;  //monitor buffer count
  int mon_buf_id=0;  //for monitor buf loop
  char temp_monitor_buf[32*MAX_DATA_LINE_LENGTH];
  char mon_name[MAX_DATA_LINE_LENGTH];
  char *gdf_name;
  char *dummy_ptr=NULL;
  int len, ret=0,et=-1,i,tier_group_found = 0;
  char *ptr=NULL;
  char *field[10];
  //char **replace_value_with[8];
  char node_ip[MAX_DATA_LINE_LENGTH];
  char node_name[MAX_DATA_LINE_LENGTH];
  char pod_ip[MAX_DATA_LINE_LENGTH];
  char pod_name[1024];
  char container_id[MAX_DATA_LINE_LENGTH];
  //char null[2];
  char temp_appname[MAX_DATA_LINE_LENGTH];
  char temp_tiername[MAX_DATA_LINE_LENGTH];
  char tier_group_name[MAX_DATA_LINE_LENGTH];
  char tmp_tier_name[MAX_DATA_LINE_LENGTH];
  char *token_arr[MAX_NO_OF_APP];
  int no_of_token_elements = 0;
  int variable_replaced = 0;
  char instance_name[1024];
  char tmp_instance_name[MAX_DATA_LINE_LENGTH*2];
  char node_region[MAX_DATA_LINE_LENGTH];
  char node_zone[MAX_DATA_LINE_LENGTH];
  sprintf(instance_name, " ");
  int mon_row = 0;
  int process_check_mon,process_server_sig,flag, process_log_mon, process_custom_gdf_mon, process_batch_job;
  int is_monitor_applied=0; 
  char out_buf[16*MAX_DATA_LINE_LENGTH];
  char seq[]="%25";
  char tier_type = 0;
  char namespace[MAX_DATA_LINE_LENGTH];
  char json_monitors_filepath[MAX_DATA_LINE_LENGTH];
  int num_field;
  kub_ip_mon_vector_fields *vector_fields_ptr = NULL;
  #define JSON_INFO_PTR 1 
  JSON_info *json_info_ptr=NULL;
  nslb_json_t *json = NULL;
  nslb_json_error err;
  MLTL3(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, "vector_name=%s server_idx=%d tier_idx=%d appname=%s runtime_flag=%d",vector_name,server_idx, tier_idx,app_name,runtime_flag);


  namespace[0] = '\0'; 
  json = nslb_json_init_buffer(auto_json_monitors_ptr, 0, 0, &err);
  MLTL4(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "JSON = %p", json);
  if(json == NULL)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                       "Unable to convert json content of file [%s] to json structure, due to: %s.", global_settings->auto_json_monitors_filepath, nslb_json_strerror(json));
    return -1; 
  }
  
  strcpy(json_monitors_filepath,global_settings->auto_json_monitors_filepath);

  if(vector_name != NULL)
  {
    strcpy(temp_buff, vector_name);
    
    num_field = parse_kubernetes_vector_format(temp_buff, field);
     if(num_field < 0)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                       "Received Invalid vector name. vector name = %s", vector_name);
      return -1;
    }
       
    
    /*
    function name: copy_vector_data_in_string
    Description: This function is used to copy field array entrires to input array.
    Which consist node_ip, node_name, pod_ip, pod_name, container_name.
    */
    vector_fields_ptr = copy_vector_data_in_string(field);
    strcpy(node_ip, field[1]);
    strcpy(node_name, field[2]);
    strcpy(pod_ip, field[4]);
    strcpy(pod_name, field[5]);
    strcpy(container_id, field[6]);
     if(num_field == 8)
      strcpy(namespace, field[7]);
    if(num_field == 10)
    {
      strcpy(namespace, field[7]);
      strcpy(node_region, field[8]);
      strcpy(node_zone, field[9]);
    }

  }

  strcpy(part_pod_name,pod_name);
  ptr=strrchr(part_pod_name,'-');
  if(ptr)
    *ptr='\0';
  
  if(!(topo_info[topo_idx].topo_tier_info[tier_idx].tier_name))
  {
     MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                       "Invalid TierId [%d] while applying json auto monitors from json [%s]", tier_idx, global_settings->auto_json_monitors_filepath);
     return -1;
  }

  NSDL2_MON(NULL,NULL,"add_json_monitors is called : server_id = %d , tier_idx = %d max mon entries = %d",server_idx, tier_idx,topo_info[topo_idx].topo_tier_info[tier_idx].max_mon_entries);

  OPEN_ELEMENT_OF_MONITORS_JSON(json);
  GOTO_ELEMENT_OF_MONITORS_JSON(json, "Tier", 1);
  OPEN_ELEMENT_OF_MONITORS_JSON(json);
  if(ret == -1)
  {
    if(((int)json->error)!=6)
    {   
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                       "Can't able to open Tier element in json [%s] while applying json auto monitors due to error: %s.", global_settings->auto_json_monitors_filepath, nslb_json_strerror(json));
      return -1;
    }
  }
  
  while (1)
  {
    process_check_mon=0;
    process_batch_job=0;
    process_server_sig=0;
    process_log_mon=0;
    flag=0;
    ret = 0;
    tier_type = 0;

    GOTO_NEXT_ARRAY_ITEM_OF_MONITORS_JSON(json);
    OPEN_ELEMENT_OF_MONITORS_JSON(json);

    ret=0;

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "tier_group",0);
    if(ret != -1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      strcpy(tier_group_name, dummy_ptr);

      if(tier_group_name[0] != '\0')
      {
        for(i=0;i<topo_info[topo_idx].total_tier_group_entries;i++)
        {
          if(!strcmp(topo_info[topo_idx].topo_tier_group[i].GrpName, tier_group_name))
          {
            tier_group_found = 1;
            break;
          }
        }

        if(tier_group_found)
        {
          if(!strcasecmp(topo_info[topo_idx].topo_tier_group[i].type, "list"))
          {
            strcpy(temp_tiername,topo_info[topo_idx].topo_tier_group[i].allTierList);
            tier_type |= 0x01;
          }
          else if(!strcasecmp(topo_info[topo_idx].topo_tier_group[i].type, "pattern"))
          {
            strcpy(temp_tiername, topo_info[topo_idx].topo_tier_group[i].GrpDefination);
            tier_type |= 0x02;
          }
        }
        else
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Info: tier_group Name '%s' is not found in provided topology.", tier_group_name);
          continue;
        }
      }
      else
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Info: value for keyword 'tier_group' is missing."); 
        continue;
      }
    }

    if(ret == -1)
    {
      ret=0;

      GOTO_ELEMENT_OF_MONITORS_JSON(json, "name",1);
      if(ret==-1)
      {
        if(((int)json->error)!=6)
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                         "Can't able to open name element in json [%s] due to error: %s.", global_settings->auto_json_monitors_filepath, nslb_json_strerror(json));
          return -1;
        }
        ret=0;
      }

    GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
    strcpy(temp_tiername, dummy_ptr);

      if(ret==-1)
      {
        if(((int)json->error)!=6)
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                         "Can't able to open name element in json [%s] due to error: %s.", global_settings->auto_json_monitors_filepath, nslb_json_strerror(json));
          return -1;
        }
        ret=0;
      }
    }
    
    no_of_token_elements = get_tokens(temp_tiername, token_arr, ",", MAX_NO_OF_APP);

    /*Regex support for "tier_name" and "list" in case of TierGroup has been removed. 
      User will have to give the complete tier_name. 
      Regex will only be supported for "pattern" in case of TierGroup.
      BUG_ID: 67602 
    */

    ret=1;        //assuming not matched by default
    for(et=0;et<no_of_token_elements;et++)
    {
      if(tier_type & 0x02)
      {
        if(!nslb_regex_match(topo_info[topo_idx].topo_tier_info[tier_idx].tier_name,token_arr[et]))   //matched
        {
          ret=0;
          break;
        }
      }
      else
      {
        if(!strcmp(topo_info[topo_idx].topo_tier_info[tier_idx].tier_name,token_arr[et]) || !strcasecmp(token_arr[et],"AllTier"))   //matched
        {
          ret=0;
          break;
        }
      }
    }
    if(ret == 1)
    {
      CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
      continue;
    }

    if(ret == -1)
    {
      if(((int)json->error)!=6)
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                         "Error while processing Tier[%s] entry in json [%s] while applying json auto monitors due to error: %s.", 
               topo_info[topo_idx].topo_tier_info[tier_idx].tier_name, 
               global_settings->auto_json_monitors_filepath, nslb_json_strerror(json));
        return -1;
      }
      else
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                         "Can't able to find Tier[%s] entry in json [%s] while applying json auto monitors due to error: %s.", topo_info[topo_idx].topo_tier_info[tier_idx].tier_name, global_settings->auto_json_monitors_filepath, nslb_json_strerror(json));
        return 0;
      }
    }

// log monitor array in json
    ret=0;
    process_log_mon = 0;
    GOTO_ELEMENT_OF_MONITORS_JSON(json,"log-pattern-mon",0);
    if(ret != -1)
    {
      is_monitor_applied=1; 
      OPEN_ELEMENT_OF_MONITORS_JSON(json);
      if(ret == -1)
      {
        if(((int)json->error)!=6)
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                         "Can't able to open log monitor element in json [%s] while applying monitors due to error: %s.", global_settings->auto_json_monitors_filepath, nslb_json_strerror(json));
          return -1;
        }
        ret=0;
      }
      else
        process_log_mon = 1;
    }
    //func call
    ret = runtime_process_log_monitors(json, vector_name, server_idx, tier_idx, runtime_flag, process_log_mon, no_of_token_elements, token_arr, et, node_name, node_ip, pod_name, mon_row, json_monitors_filepath);
    if(ret == -1)
    {
      if(((int)json->error)!=6)
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                         "Can't able to apply monitors on Tier [%s] from json [%s] due to error: %s.", 
                         topo_info[topo_idx].topo_tier_info[tier_idx].tier_name, 
                         global_settings->auto_json_monitors_filepath, nslb_json_strerror(json));
       // return -1;
      }
      else
      {
        CLOSE_ELEMENT_ARR_OF_MONITORS_JSON(json);
       // CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
       // continue;
      }
    }
 
    ret=0;
    process_log_mon = 0;
    GOTO_ELEMENT_OF_MONITORS_JSON(json,"get-log-file",0);
    if(ret != -1)
    {
      is_monitor_applied=1;
      OPEN_ELEMENT_OF_MONITORS_JSON(json);
      if(ret == -1)
      {
        if(((int)json->error)!=6)
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                         "Can't able to open log monitor element in json [%s] while applying monitors due to error: %s.", global_settings->auto_json_monitors_filepath, nslb_json_strerror(json));
          return -1;
        }
        ret=0;
      }
      else
        process_log_mon = 2;
    }
    //func call
    ret = runtime_process_log_monitors(json, vector_name, server_idx, tier_idx, runtime_flag, process_log_mon, no_of_token_elements, token_arr, et, node_name, node_ip, pod_name, mon_row, json_monitors_filepath);
    if(ret == -1)
    {
      if(((int)json->error)!=6)
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                         "Can't able to apply monitors on Tier [%s] from json [%s] due to error: %s.",
                          topo_info[topo_idx].topo_tier_info[tier_idx].tier_name ,
                          global_settings->auto_json_monitors_filepath, nslb_json_strerror(json));
       // return -1;
      }
      else
      {
        CLOSE_ELEMENT_ARR_OF_MONITORS_JSON(json);
       // CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
       // continue;
      }
    }

    ret=0; 
    process_log_mon = 0;
    GOTO_ELEMENT_OF_MONITORS_JSON(json,"log-data-mon",0);
    if(ret != -1)
    {
      is_monitor_applied=1;
      OPEN_ELEMENT_OF_MONITORS_JSON(json);
      if(ret == -1)
      {
        if(((int)json->error)!=6)
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                         "Can't able to open log monitor element in json [%s] while applying monitors due to error: %s.", global_settings->auto_json_monitors_filepath, nslb_json_strerror(json));
          return -1;
        }
        ret=0;
      }
      else
        process_log_mon = 3;
    }
    //func call
    ret = runtime_process_log_monitors(json, vector_name, server_idx, tier_idx, runtime_flag, process_log_mon, no_of_token_elements, token_arr, et, node_name, node_ip, pod_name, mon_row, json_monitors_filepath);
    if(ret == -1)
    {
      if(((int)json->error)!=6)
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                         "Can't able to apply monitors on Tier [%s] from json [%s] due to error: %s.", topo_info[topo_idx].topo_tier_info[tier_idx].tier_name, global_settings->auto_json_monitors_filepath, nslb_json_strerror(json));
       // return -1;
      }
      else
      {
        CLOSE_ELEMENT_ARR_OF_MONITORS_JSON(json);
       // CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
       // continue;
      }
    }
   
    if(!lps_based_monitors)
    {
       ret=0;
       process_custom_gdf_mon = 0;
       GOTO_ELEMENT_OF_MONITORS_JSON(json, "custom-gdf-mon",0);
       if(ret != -1)
       {
         is_monitor_applied=1;
         OPEN_ELEMENT_OF_MONITORS_JSON(json);
         if(ret == -1)
         {
           if(((int)json->error)!=6)
           {
             MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                            "Can't able to open custom-gdf-mon element in json [%s] while applying monitors due to error: %s.", global_settings->auto_json_monitors_filepath, nslb_json_strerror(json));
             return -1;
           }
           ret=0;
         }
         else
           process_custom_gdf_mon = 1;
       }
       //func call
       ret = runtime_process_custom_gdf_mon(json, vector_name, server_idx, tier_idx, runtime_flag, process_custom_gdf_mon, no_of_token_elements, token_arr, et, node_name, node_ip, pod_name, mon_row, json_monitors_filepath);
       if(ret == -1)
       {
         if(((int)json->error)!=6)
         {
           MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                    "Can't able to apply monitors for custom-gdf-mon on Tier [%s] from json [%s] due to error: %s.", 
                     topo_info[topo_idx].topo_tier_info[tier_idx].tier_name, 
                     global_settings->auto_json_monitors_filepath, nslb_json_strerror(json));
         }
   
         else   // We need to close array element first.
         {
           CLOSE_ELEMENT_ARR_OF_MONITORS_JSON(json);
         }
       }
     } 


// check monitor array in json
    if(!lps_based_monitors)
    {
       ret=0;
       process_check_mon=0;
       GOTO_ELEMENT_OF_MONITORS_JSON(json,"check-monitor",0);
       if(ret != -1)
       {  
         is_monitor_applied=1;
         OPEN_ELEMENT_OF_MONITORS_JSON(json);
         if(ret == -1)
         {
           if(((int)json->error)!=6)
           {
             MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                            "Can't able to open gdf element in json [%s] while applying monitors due to error: %s.", global_settings->auto_json_monitors_filepath, nslb_json_strerror(json));
             return -1;
           }
           ret=0;
         }
         else
           process_check_mon = 1;
       }
    
       ret = runtime_process_batch_job_and_check_mon(json, server_idx, tier_idx, runtime_flag, process_check_mon ,0 ,no_of_token_elements, token_arr, et,pod_name, mon_row, json_monitors_filepath);
          
       if(ret == -1)
       {
         if(((int)json->error)!=6)
         {
           MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                            "Can't able to apply monitors on Tier [%s] from json [%s] due to error: %s.",topo_info[topo_idx].topo_tier_info[tier_idx].tier_name, global_settings->auto_json_monitors_filepath, nslb_json_strerror(json));
           //continue;
         }
         else
         {
           CLOSE_ELEMENT_ARR_OF_MONITORS_JSON(json);
           //CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
           //continue;
            ret=0;
         } 
       }
    } 

// batch job array
    if(!lps_based_monitors)
    {
      ret = 0;
      process_batch_job=0;
      GOTO_ELEMENT_OF_MONITORS_JSON(json,"batch-job",0);
      if(ret != -1)
      {
        is_monitor_applied=1;
        OPEN_ELEMENT_OF_MONITORS_JSON(json);
        if(ret == -1)
        {
          if(((int)json->error)!=6)
          {
            MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                     "Can't able to open gdf element in json [%s] while applying monitors due to error: %s.", global_settings->auto_json_monitors_filepath, nslb_json_strerror(json));
            return -1;
          }
          ret=0;
        }
        else
          process_batch_job = 1;
      }
  
      ret = runtime_process_batch_job_and_check_mon(json ,server_idx, tier_idx, runtime_flag, 0,process_batch_job,no_of_token_elements, token_arr, et, pod_name, mon_row, json_monitors_filepath);
  
      if(ret == -1)
      {
        if(((int)json->error)!=6)
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                           "Can't able to apply monitors on Tier [%s] from json [%s] due to error: %s.", topo_info[topo_idx].topo_tier_info[tier_idx].tier_name, global_settings->auto_json_monitors_filepath, nslb_json_strerror(json));
          //continue;
        }
        else
        {
          CLOSE_ELEMENT_ARR_OF_MONITORS_JSON(json);
          //CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
          //continue;
   ret=0;
        } 
      }
    }

// server signature array
   if(!lps_based_monitors)
   {
      GOTO_ELEMENT_OF_MONITORS_JSON(json,"server-signature",0);
      if(ret != -1)
      {
        is_monitor_applied=1; 
        OPEN_ELEMENT_OF_MONITORS_JSON(json);
        if(ret == -1)
        {
          if(((int)json->error)!=6)
          {
            MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                           "Can't able to open gdf element in json [%s] while applying monitors due to error: %s.", global_settings->auto_json_monitors_filepath, nslb_json_strerror(json));
            return -1;
          }
          ret=0;
        }
        else
          process_server_sig = 1;
      }
      else
        ret = 0;
 
      while (process_server_sig)
      {
        ret=0;
        prgrm_args[0]='\0';
        GOTO_NEXT_ARRAY_ITEM_OF_MONITORS_JSON(json);
        OPEN_ELEMENT_OF_MONITORS_JSON(json);
 
        GOTO_ELEMENT_OF_MONITORS_JSON(json, "enabled",0);
        {
          if(ret!=-1)
          {
            GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
            if( !strcasecmp(dummy_ptr,"false"))
            {
              CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
              continue;
            }
          }
          else
            ret=0;
        }
 
        MY_MALLOC_AND_MEMSET(json_info_ptr, sizeof(JSON_info), "JSON Info Ptr", -1);
 
        CHECK_OPTIONAL_FEATURES_IN_JSON(json,ret,instance_name,tier_idx);
       
        GOTO_ELEMENT_OF_MONITORS_JSON(json, "signature-name",1);
        if(ret!=-1)
        {
          GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
          strcpy(mon_name,dummy_ptr);
        }
        else
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"not provided argument: signature-name for tier: %s ",topo_info[topo_idx].topo_tier_info[tier_idx].tier_name);
          CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
          continue;
        }
 
        GOTO_ELEMENT_OF_MONITORS_JSON(json, "signature-type",0);
        if(ret!=-1)
        {
          GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
          strcat(prgrm_args,dummy_ptr);
          strcat(prgrm_args," ");
 
          GOTO_ELEMENT_OF_MONITORS_JSON(json, "command/file-name",0);
          if(ret!=-1)
          {
            GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
            strcat(prgrm_args,dummy_ptr);
          }
          else
          {
            MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"not provided argument:command/file-name of signature-name %s for tier: %s ",mon_name,
            topo_info[topo_idx].topo_tier_info[tier_idx].tier_name);
            CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
            continue;
          }
        }
        else
        {
          ret=0;
          if(topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->cmon_option_flag == 0)
          {
            GOTO_ELEMENT_OF_MONITORS_JSON(json, "old-options",0);
            if(ret == -1)
            {
              GOTO_ELEMENT_OF_MONITORS_JSON(json, "options",1);
            }
          }
          else
          {
            GOTO_ELEMENT_OF_MONITORS_JSON(json, "options",1);
          }
 
          //GOTO_ELEMENT_OF_MONITORS_JSON(json, "options",1);
          if(ret!=-1)
          {
            GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
            strcpy(prgrm_args,dummy_ptr);
          }
          else
          {
            MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"not provided argument: signature-type and options of signature-name %s for tier: %s",
                   mon_name,topo_info[topo_idx].topo_tier_info[tier_idx].tier_name);
            CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
            continue;
          }
        }
 
        sprintf(monitor_buf,"SERVER_SIGNATURE %s %s_%s %s",
               topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->server_name, 
               topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_disp_name, mon_name, prgrm_args);
 
        if(json_info_ptr)
        {
          if(instance_name[0] != '\0')
          {
            //instance_name come with > bracket i.e ">instance" so we store instance_name in json_info_ptr without > bracket
            //we use json_info_ptr->instance_name in any_server case.
            copy_instance_name_in_json_info_ptr(instance_name, json_info_ptr);
          }
 
          if(mon_name[0] != '\0')
          {
            MY_MALLOC_AND_MEMSET(json_info_ptr->mon_name, strlen(mon_name), "JSON Info Ptr", -1);
            strcpy(json_info_ptr->mon_name, mon_name);
          }
 
          if(prgrm_args[0] != '\0')
          {
            MY_MALLOC_AND_MEMSET(json_info_ptr->args, strlen(prgrm_args), "JSON Info Ptr", -1);
            strcpy(json_info_ptr->args, prgrm_args);
          }
 
        }
 
 
        if(runtime_flag)
        {
          //This will allocate from start everytime once it get free 
          create_table_entry_dvm(&mon_row, &total_json_monitors, &max_json_monitors, (char **)&json_monitors_ptr, sizeof(Mon_from_json), "Server Signature to be Applied");
 
          MY_MALLOC(json_monitors_ptr[mon_row].mon_buff, strlen(monitor_buf) + 1, "Server Signature buffer", mon_row);
          strcpy(json_monitors_ptr[mon_row].mon_buff, monitor_buf);
          json_monitors_ptr[mon_row].mon_type = SERVER_SIGNATURE;
          MY_MALLOC(json_monitors_ptr[mon_row].pod_name, strlen(pod_name) + 1, "Pod Name", mon_row);
          strcpy(json_monitors_ptr[mon_row].pod_name, pod_name);
          MY_MALLOC_AND_MEMSET(json_monitors_ptr[mon_row].json_info_ptr, sizeof(JSON_info), "JSON Info Ptr", -1);
          if(json_info_ptr->any_server)
          {
 
            MY_MALLOC_AND_MEMSET(json_monitors_ptr[mon_row].json_info_ptr->instance_name, strlen(instance_name), "JSON Info Ptr", -1);
            strcpy(json_monitors_ptr[mon_row].json_info_ptr->instance_name, instance_name);
           
            MY_MALLOC_AND_MEMSET(json_monitors_ptr[mon_row].json_info_ptr->mon_name, strlen(mon_name), "JSON Info Ptr", -1);
            strcpy(json_monitors_ptr[mon_row].json_info_ptr->mon_name, mon_name);
 
            MY_MALLOC_AND_MEMSET(json_monitors_ptr[mon_row].json_info_ptr->args, strlen(prgrm_args), "JSON Info Ptr", -1);
            strcpy(json_monitors_ptr[mon_row].json_info_ptr->args, prgrm_args);
 
            json_monitors_ptr[mon_row].json_info_ptr->any_server=true;
          }
          MALLOC_AND_COPY(topo_info[topo_idx].topo_tier_info[tier_idx].tier_name, json_monitors_ptr[mon_row].json_info_ptr->tier_name, strlen(topo_info[topo_idx].topo_tier_info[tier_idx].tier_name) + 1,"JSON INFO ptr g_mon_id", -1);
          json_monitors_ptr[mon_row].json_info_ptr->tier_idx=tier_idx; 
          
          MALLOC_AND_COPY(topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_disp_name, json_monitors_ptr[mon_row].json_info_ptr->server_name, strlen(topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_disp_name) + 1,"server", -1);
          json_monitors_ptr[mon_row].json_info_ptr->server_index=server_idx;
 
 
 
  
        }
        else
        {
          strcpy(temp_monitor_buf,monitor_buf);
          if(kw_set_server_signature("SERVER_SIGNATURE", monitor_buf, runtime_flag, err_msg, json_info_ptr) < 0 )
            MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                                        "Failed to apply json auto monitor with buffer: %s . Error: %s", temp_monitor_buf, err_msg);
 
 
        }
 
        topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->auto_monitor_applied = 1;
 
        CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
     
        FREE_AND_MAKE_NULL(json_info_ptr->instance_name, "json_info_ptr->instance_name", -1);
        FREE_AND_MAKE_NULL(json_info_ptr->mon_name, "json_info_ptr->mon_name", -1);
        FREE_AND_MAKE_NULL(json_info_ptr->args, "json_info_ptr->args", -1);
        FREE_AND_MAKE_NULL(json_info_ptr, "json_info_ptr", -1);
 
      }
      if(ret == -1)
      {
        if(((int)json->error)!=6)
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                "Can't able to apply monitors on Tier [%s] from json [%s] due to error: %s.",
                topo_info[topo_idx].topo_tier_info[tier_idx].tier_name, global_settings->auto_json_monitors_filepath, nslb_json_strerror(json));
         // return -1;
        }
        else
        {
          CLOSE_ELEMENT_ARR_OF_MONITORS_JSON(json);
         // CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
         // continue;
        }
      }
    }

// gdf/std-mon array
    ret=0;
    GOTO_ELEMENT_OF_MONITORS_JSON(json, "gdf",0);
    if(ret != -1)
    {
      OPEN_ELEMENT_OF_MONITORS_JSON(json);
    }
    else   //If gdf is not found, it means no more array brackets present. So we need to close the obj element, hence closing it. 
    {
      CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
      continue;
    }
    if(ret == -1)
    {
      ret=0;

      if(is_monitor_applied == 1)
      {
        GOTO_ELEMENT_OF_MONITORS_JSON(json, "std-mon",0);
        if(ret == -1)
          return -1;
      }
      else
      {
        GOTO_ELEMENT_OF_MONITORS_JSON(json, "std-mon",1);
      }
      OPEN_ELEMENT_OF_MONITORS_JSON(json);
      if(ret==-1)
      {
        if(((int)json->error)!=6)
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                         "Can't able to open gdf element in json [%s] due to error: %s.", global_settings->auto_json_monitors_filepath, nslb_json_strerror(json));
          return -1;
        }
        ret=0;

      }
      else
 flag=1;
    }
    else
      flag=1;
    while (flag)
    {
      ret=0; 
      GOTO_NEXT_ARRAY_ITEM_OF_MONITORS_JSON(json);
      OPEN_ELEMENT_OF_MONITORS_JSON(json);

      GOTO_ELEMENT_OF_MONITORS_JSON(json, "enabled",0);
      {
        if(ret!=-1)
        {
          GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
          if( !strcasecmp(dummy_ptr,"false"))
          {
            CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
            continue;
          }
        }
        else
          ret=0;
      }
  
      MY_MALLOC_AND_MEMSET(json_info_ptr, sizeof(JSON_info), "JSON Info Ptr", -1);

      //here we get instance_name with > bracket i.e ">instance"
      CHECK_OPTIONAL_FEATURES_IN_JSON(json,ret,instance_name,tier_idx);
  
      //here frequency is monitor interval
      GOTO_ELEMENT_OF_MONITORS_JSON(json, "frequency",0);
      {
        if(ret != -1)
        {
          GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
          json_info_ptr->frequency = atoi(dummy_ptr) *1000;
        }
        else
          ret=0;
      }
   
      //this check is used to skip all type of monitor those are not using any_server flag while ndc sends instance_up msg
      if(ndc_any_server_check == 1 && !json_info_ptr->any_server)
      {
        CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
        continue;
      }
      if(instance_name[0] != '\0')
      {
        //instance_name come with > bracket i.e ">instance" so we store instance_name in json_info_ptr without > bracket
        //we use json_info_ptr->instance_name in any_server case.
        copy_instance_name_in_json_info_ptr(instance_name, json_info_ptr);
      }
      GOTO_ELEMENT_OF_MONITORS_JSON(json, "std-mon-name", 1);
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      strcpy(mon_name, dummy_ptr);

      //parse tier-for-any-server
      GOTO_ELEMENT_OF_MONITORS_JSON(json, "tier-for-any-server",0);
      {
        if(ret != -1)
        {
          GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
          strcpy(dest_any_server,dummy_ptr);
          json_info_ptr->dest_any_server_flag = true;
          json_info_ptr->no_of_dest_any_server_elements=get_tokens(dest_any_server, json_info_ptr->dest_any_server_arr, "," , MAX_NO_OF_APP);
        }
        else
          ret=0;
      }
      json_info_ptr->agent_type |= CONNECT_TO_CMON;
      GOTO_ELEMENT_OF_MONITORS_JSON(json, "agent-type",0);
      {
        if(ret != -1)
        {
          GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
          if(!strcasecmp(dummy_ptr, "BCI"))
          {
            //here unset CMON
            json_info_ptr->agent_type &= ~CONNECT_TO_CMON;
            json_info_ptr->agent_type |= CONNECT_TO_NDC;
          }
          // We do not want to start NDC Mbean monitor to be applied at runtime. 
          else if(!strcasecmp(dummy_ptr,"ALL"))
          {
            json_info_ptr->agent_type |= CONNECT_TO_BOTH_AGENT;  
          }
        }
        else
          ret=0;
          
      }
      
      //We donot need to apply rtc at time of node discovery for MBean type monitor.
      if(json_info_ptr->agent_type & CONNECT_TO_NDC && !(json_info_ptr->agent_type & CONNECT_TO_CMON))
      {
        CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
        continue;
      }

      StdMonitor *std_mon_ptr;
      if((std_mon_ptr = get_standard_mon_entry(mon_name, "ALL", 1, err_msg)) == NULL)
      {
        /***
        Here we need std_mon_ptr for MBean type monitors

        But 'get_standard_mon_entry' will be called everytime a standard monitor is applied or deleted.
        Here it is called with "ALL" machine type but there are standard monitors whoose machine type is not "ALL".
        So get_standard_mon_entry will return NULL and get_standard_mon_entry will be called again in kw_set_standard_monitor
        function with correct machine type.

        So commenting the below log message if get_standard_mon_entry returns NULL.
        Fix in future: put proper log message
        ***/
        if(lps_based_monitors) 
        {
           CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
           continue;
        }
        //sprintf(err_msg, "Warning/Error: Standard Monitor Name '%s' or server type \"ALL\" is incorrect\n", dummy_ptr);
        //MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION, "Warning/Error %s",err_msg);
        err_msg[0]='\0';
      }
      else
      {
        if(lps_based_monitors && !(std_mon_ptr->use_lps))
        {
          CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
          continue;
        }
        if (std_mon_ptr->config_json)
        {
          MALLOC_AND_COPY(std_mon_ptr->config_json, json_info_ptr->config_json, strlen(std_mon_ptr->config_json)+1, "config_json", -1);
          MALLOC_AND_COPY(std_mon_ptr->monitor_name, json_info_ptr->mon_name, strlen(std_mon_ptr->monitor_name)+1, "monitor_name", -1);
          MALLOC_AND_COPY(std_mon_ptr->gdf_name, gdf_name, strlen(std_mon_ptr->gdf_name)+1, "monitor gdf name", -1);
        }
        if((std_mon_ptr->agent_type & CONNECT_TO_BOTH_AGENT))
        {
          if((set_agent_type(json_info_ptr, std_mon_ptr, mon_name)) == -1)
          {
            CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
            continue;
          }
        }
      }
      
      GOTO_ELEMENT_OF_MONITORS_JSON(json, "init-vector-file",0);
      {
        if(ret != -1)
        {
          GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
          MY_MALLOC_AND_MEMSET(json_info_ptr->init_vector_file, (len + 1), "JSON Info Ptr initVectorFile", -1);
          strcpy(json_info_ptr->init_vector_file, dummy_ptr);
        }
        else
          ret=0;
      }
 
      GOTO_ELEMENT_OF_MONITORS_JSON(json, "app",1);
      OPEN_ELEMENT_OF_MONITORS_JSON(json);
      while(1)
      {
        ret=0;
        prgrm_args[0]='\0'; 
      
        GOTO_NEXT_ARRAY_ITEM_OF_MONITORS_JSON(json);
        OPEN_ELEMENT_OF_MONITORS_JSON(json);
        GOTO_ELEMENT_OF_MONITORS_JSON(json, "app-name",1);
        if(app_name)
        {
          strcpy(temp_appname, app_name);
          if(ret == -1)
          {
            CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
            continue;
          }
          else
   {
            GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
            if(strcasecmp(dummy_ptr,app_name))
            {
              CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
              continue;
            }
          }

          GOTO_ELEMENT_OF_MONITORS_JSON(json, "exclude-app",0);
          {
            if(ret != -1)
            {
              GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
              no_of_token_elements = get_tokens(dummy_ptr, token_arr, ",", MAX_NO_OF_APP);
              for(et=0;et<no_of_token_elements;et++)
              {
                if(strstr(pod_name, token_arr[et]))
                {
                  CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
                  ret=1;
                  break;
                }
              }
              if(ret == 1)
                continue;
            }
            else
              ret=0;
          }

          variable_replaced |= 0x10;
          
         
          GOTO_ELEMENT_OF_MONITORS_JSON(json, "java-home",0);
          if(ret !=-1)
          {
            GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
            MY_MALLOC_AND_MEMSET(json_info_ptr->javaHome, (len + 1), "JSON Info Ptr JavaClasspath", -1);
            strcpy(json_info_ptr->javaHome, dummy_ptr);
          }
          else
            ret=0;
    
          GOTO_ELEMENT_OF_MONITORS_JSON(json, "java-classpath",0);
          if(ret !=-1)
          {
            GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
            MY_MALLOC_AND_MEMSET(json_info_ptr->javaClassPath, (len + 1), "JSON Info Ptr JavaClasspath", -1);
            strcpy(json_info_ptr->javaClassPath, dummy_ptr);
          }
          else
            ret=0;

	  GOTO_ELEMENT_OF_MONITORS_JSON(json, "run-as-process",0);
	  if(ret !=-1)
	  {
	    GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
	     if( !strcasecmp(dummy_ptr,"true"))
	     {
	       json_info_ptr->is_process = 1;
             }
	  }
	  else
	    ret=0;


          //GOTO_ELEMENT_OF_MONITORS_JSON(json, "options",1);
          //GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);

          //TODO check cmon verion if cmon version is greater than default value then go for new-option otherwise go for old-option
          if(topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->cmon_option_flag == 0)
          {
            GOTO_ELEMENT_OF_MONITORS_JSON(json, "old-options",0);
            if(ret == -1)
            {
              GOTO_ELEMENT_OF_MONITORS_JSON(json, "options",1);
            }

          }
          else
          {
            GOTO_ELEMENT_OF_MONITORS_JSON(json, "options",1);
          } 

          GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
          /*
          function name: search_replace_chars
          Arguments:
            we have to send input data buffer as argument.
            out_buf is to save encode string, initally out_buf is null.
            input_patter consist string through, which we have to encode the dummy_ptr.
            pattern consist the string node_name, node_ip, pod_name pod_ip, container_id.

          Description: In this function we are supplying input buffer. which consists of substring
            like pod_ip, pod_name, node_ip, node_name, server_ip, server_name, container_id.
            If matched then we will replace with the actual names.
          */

          ret_value = search_replace_chars(&dummy_ptr, seq, out_buf, vector_fields_ptr);

          MLTL3(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "dummy_ptr after function call is = %s",dummy_ptr);

          MLTL3(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              " ret value after function \"search_replace_chars\" is  = %d", ret_value);

          //It will enter into in this, only when string is passed as it is.
          if( ret_value == 0 )
          {
            if(((!strncasecmp(mon_name,"AccessLogStats",14)) || (!strncasecmp(mon_name,"AccessLogStatusStats",20))) && vector_name != NULL)
            {
              sprintf(path,"-f /var/log/containers/*%s*%s*.log", part_pod_name,container_id);
              sprintf(overall_vector, "-A %s%cOverall", pod_name, global_settings->hierarchical_view_vector_separator);
              sprintf(prgrm_args,"%s %s ", path, overall_vector);
              GOTO_ELEMENT_OF_MONITORS_JSON(json, "url",0);
              if(ret != -1)
              {
                OPEN_ELEMENT_OF_MONITORS_JSON(json);
                while (1)
                {
                  ret=0;
                  GOTO_NEXT_ARRAY_ITEM_OF_MONITORS_JSON(json);
                  OPEN_ELEMENT_OF_MONITORS_JSON(json);
                  GOTO_ELEMENT_OF_MONITORS_JSON(json, "M",1);
                  GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
                  sprintf(temp_buff, "-U \"M:%s,", dummy_ptr);
                  strcat(prgrm_args, temp_buff);
                  GOTO_ELEMENT_OF_MONITORS_JSON(json, "P",1);
                  GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
                  sprintf(temp_buff, "P:'%s',", dummy_ptr);
                  strcat(prgrm_args, temp_buff);
                  GOTO_ELEMENT_OF_MONITORS_JSON(json, "V",1);
                  GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
                  sprintf(temp_buff, "V:%s%c%s\" ", pod_name, global_settings->hierarchical_view_vector_separator, dummy_ptr);
                  strcat(prgrm_args, temp_buff);
                  CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
                }
                CLOSE_ELEMENT_ARR_OF_MONITORS_JSON(json);
              }
              else
                ret=0;

              path[0]='\0';
              overall_vector[0]='\0';
            }
            else if(!strncasecmp(mon_name,"JavaGCJMX",9) && vector_name !=NULL)
            {
              sprintf(prgrm_args,"--host %s ",pod_ip);
            }
            else if(!strncasecmp(mon_name,"SpringBoot",10) && vector_name !=NULL)
            {
              sprintf(prgrm_args, "-U \"http://%s:", pod_ip);
              GOTO_ELEMENT_OF_MONITORS_JSON(json, "port",1);
              GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
              strcat(prgrm_args, dummy_ptr);
              GOTO_ELEMENT_OF_MONITORS_JSON(json, "url",1);
              GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
              strcat(prgrm_args, dummy_ptr);
              strcat(prgrm_args, "\" ");
              if(!strncasecmp(mon_name,"SpringBootServiceStats",22))
              {
                strcat(prgrm_args," -X ");
                strcat(prgrm_args, pod_name);
                strcat(prgrm_args, " ");
              }
            }
            else if(!strncasecmp(mon_name,"NginxResourceStats",18) && vector_name != NULL)
            {
              sprintf(prgrm_args, "-U \"http://%s:", pod_ip);
              GOTO_ELEMENT_OF_MONITORS_JSON(json, "url",1);
              GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
              strcat(prgrm_args, dummy_ptr);
              strcat(prgrm_args, "\" ");
            }
            GOTO_ELEMENT_OF_MONITORS_JSON(json, "options",1);
            GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
          }
          variable_replaced = 0;
        } 
        else
        {
          if(ret != -1)
          {
            GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
            strcpy(temp_appname, dummy_ptr);
            if(strcasecmp(dummy_ptr,"default"))
            {
              CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
              continue;
            }
          }

          GOTO_ELEMENT_OF_MONITORS_JSON(json, "java-home",0);
          if(ret !=-1)
          {
            GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
            MY_MALLOC_AND_MEMSET(json_info_ptr->javaHome, (len + 1), "JSON Info Ptr JavaClasspath", -1);
            strcpy(json_info_ptr->javaHome, dummy_ptr);
          }
          else
            ret=0;

          GOTO_ELEMENT_OF_MONITORS_JSON(json, "java-classpath",0);
          if(ret !=-1)
          {
            GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
            MY_MALLOC_AND_MEMSET(json_info_ptr->javaClassPath, (len + 1), "JSON Info Ptr JavaClasspath", -1);
            strcpy(json_info_ptr->javaClassPath, dummy_ptr);
          }
          else
            ret=0;

	 GOTO_ELEMENT_OF_MONITORS_JSON(json, "run-as-process",0);
	 if(ret !=-1)
	 {
	   GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
	   if( !strcasecmp(dummy_ptr,"true"))
	   {
	     json_info_ptr->is_process = 1;
	   }
	 }
	 else
	   ret=0;

  
          GOTO_ELEMENT_OF_MONITORS_JSON(json, "TierServerMappingType",0);
          if(ret !=-1)
          {
            GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
            if(!strncasecmp(dummy_ptr, "standard", 8))
              json_info_ptr->tier_server_mapping_type |= STANDARD_TYPE;
            else if(!strncasecmp(dummy_ptr, "custom", 6))
              json_info_ptr->tier_server_mapping_type |= CUSTOM_TYPE;
          }
          else
            ret=0;


          GOTO_ELEMENT_OF_MONITORS_JSON(json, "VectorReplaceFrom",0);
          if(ret != -1)
          {
            GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
            MY_MALLOC_AND_MEMSET(json_info_ptr->vectorReplaceFrom, (len + 1), "JSON Info Ptr VectorReplaceFrom", -1);
            strcpy(json_info_ptr->vectorReplaceFrom, dummy_ptr);
          }
          else
            ret = 0;


          GOTO_ELEMENT_OF_MONITORS_JSON(json, "VectorReplaceTo",0);
          if(ret != -1)
          {
            GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
            MY_MALLOC_AND_MEMSET(json_info_ptr->vectorReplaceTo, (len + 1), "JSON Info Ptr VectorReplaceTo", -1);
            strcpy(json_info_ptr->vectorReplaceTo, dummy_ptr);
          }
          else
            ret=0;

   variable_replaced |= 0x10;

          GOTO_ELEMENT_OF_MONITORS_JSON(json, "options",1);
          GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);

          while((variable_replaced & 0x10) && strchr(dummy_ptr,'%'))
          {
     variable_replaced &= 0x01;
            char *variable_ptr = NULL;
            char leftover_arguments[MAX_DATA_LINE_LENGTH];
 
            if((variable_ptr=strstr(dummy_ptr,"%server_ip%")))
            {
              strcpy(leftover_arguments, variable_ptr + 11);
              strcpy(variable_ptr,topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->server_name);
              strcpy(variable_ptr + strlen(topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->server_name), leftover_arguments);
              variable_replaced |= 0x11;
            }
            if((variable_ptr=strstr(dummy_ptr,"%server_name%")))
            {
              strcpy(leftover_arguments, variable_ptr + 13);
              strcpy(variable_ptr,topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->server_disp_name);
              strcpy(variable_ptr + strlen(topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->server_disp_name), leftover_arguments);
             variable_replaced |= 0x11;
            }
            if(json_info_ptr->dest_any_server_flag)
            {
              MY_MALLOC(monitor_buf_arr, sizeof(*monitor_buf_arr) * json_info_ptr->no_of_dest_any_server_elements, "malloc monitor buffer", 0);
              MY_MALLOC(instance_name_arr, sizeof(*instance_name_arr) * json_info_ptr->no_of_dest_any_server_elements, "malloc instance name", 0);
              mon_buf_count=create_mon_buf_arr(json_info_ptr, tier_idx, server_idx ,dummy_ptr ,monitor_buf_arr,mon_name ,gdf_name, instance_name_arr);
              arr_ret = 1;
            }
          }
          variable_replaced = 0;
        }
        
        if (arr_ret == 0)
        {
          strcat(prgrm_args, dummy_ptr);
          MY_MALLOC(monitor_buf_arr, sizeof(*monitor_buf_arr) * 1, "malloc monitor buffer", -1);
          mon_buf_count =1; //for monitor buf loop
          
          if(enable_store_config && (!strcmp(topo_info[topo_idx].topo_tier_info[tier_idx].tier_name, "Cavisson")))
            sprintf(tmp_tier_name, "%s!%s", g_store_machine, topo_info[topo_idx].topo_tier_info[tier_idx].tier_name);
          else
            sprintf(tmp_tier_name, "%s", topo_info[topo_idx].topo_tier_info[tier_idx].tier_name);
     
          //TODO: Discuss below check 
          if(vector_name==NULL)
          {
            sprintf(monitor_buf_arr[0],"STANDARD_MONITOR %s%c%s %s%c%s%s %s %s", topo_info[topo_idx].topo_tier_info[tier_idx].tier_name, global_settings->hierarchical_view_vector_separator, topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->server_disp_name, tmp_tier_name, global_settings->hierarchical_view_vector_separator, topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_disp_name, instance_name, mon_name, prgrm_args);
          }
          else
          {
            if((!strcmp(mon_name, "RedisSlaveStatsEx")) || (!strcmp(mon_name, "RedisActivityStatsV2Ex")) || (!strcmp(mon_name, "RedisCacheStatsV2Ex")) || (!strcmp(mon_name, "RedisperformanceStatsV2Ex")) || (!strcmp(mon_name, "RedisSystemStatsEx")))
            {
              if(namespace[0] != '\0')
                sprintf(tmp_instance_name, "%s%c%s%s", namespace, global_settings->hierarchical_view_vector_separator, pod_name, instance_name);
              else
                sprintf(tmp_instance_name, "default%c%s%s", global_settings->hierarchical_view_vector_separator, pod_name, instance_name);
            }
            else
              sprintf(tmp_instance_name, "%s%s", pod_name, instance_name);

            sprintf(monitor_buf_arr[0],"STANDARD_MONITOR %s%c%s %s%c%s%c%s %s %s", topo_info[topo_idx].topo_tier_info[tier_idx].tier_name, global_settings->hierarchical_view_vector_separator,topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->server_disp_name, tmp_tier_name, global_settings->hierarchical_view_vector_separator, topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_disp_name, global_settings->hierarchical_view_vector_separator, tmp_instance_name, mon_name, prgrm_args);
          }
        }

        for(mon_buf_id = 0; mon_buf_id < mon_buf_count ;mon_buf_id++ )
        {
	  NSDL2_MON(NULL, NULL, "Method called, monitor_buf = %s", monitor_buf_arr[mon_buf_id]);
          if(runtime_flag)
          {
            //This will allocate from start everytime once it get free 
            create_table_entry_dvm(&mon_row, &total_json_monitors, &max_json_monitors, (char **)&json_monitors_ptr, sizeof(Mon_from_json), "Standard Monitors to be Applied");
            MY_MALLOC(json_monitors_ptr[mon_row].mon_buff, strlen(monitor_buf_arr[mon_buf_id]) + 1, "Standard monitor buffer", mon_row);
            strcpy(json_monitors_ptr[mon_row].mon_buff, monitor_buf_arr[mon_buf_id]);
            json_monitors_ptr[mon_row].mon_type = STANDARD_MONITOR;
            MY_MALLOC(json_monitors_ptr[mon_row].pod_name, strlen(pod_name) + 1, "Pod Name", mon_row);
            strcpy(json_monitors_ptr[mon_row].pod_name, pod_name);
            MY_MALLOC_AND_MEMSET(json_monitors_ptr[mon_row].json_info_ptr, sizeof(JSON_info), "JSON Info Ptr", -1);

            //TODO:These malloc and copies should be done in optimized way, will remove this in upcoming builds.            
            if(json_info_ptr)
            {
              if(json_info_ptr->vectorReplaceTo)
              {
                MY_MALLOC_AND_MEMSET(json_monitors_ptr[mon_row].json_info_ptr->vectorReplaceTo, (strlen(json_info_ptr->vectorReplaceTo)+ 1), "JSON Info Ptr VectorReplaceTo", -1);
                strcpy(json_monitors_ptr[mon_row].json_info_ptr->vectorReplaceTo, json_info_ptr->vectorReplaceTo);
              }
              if(json_info_ptr->vectorReplaceFrom)
              {
                MY_MALLOC_AND_MEMSET(json_monitors_ptr[mon_row].json_info_ptr->vectorReplaceFrom, (strlen(json_info_ptr->vectorReplaceFrom)+ 1), "JSON Info Ptr VectorReplaceFrom", -1);

                strcpy(json_monitors_ptr[mon_row].json_info_ptr->vectorReplaceFrom, json_info_ptr->vectorReplaceFrom);
              }
 
              json_monitors_ptr[mon_row].json_info_ptr->tier_server_mapping_type = json_info_ptr->tier_server_mapping_type;
              json_monitors_ptr[mon_row].json_info_ptr->connect_mode = json_info_ptr->connect_mode;
              json_monitors_ptr[mon_row].json_info_ptr->agent_type = json_info_ptr->agent_type;
              json_monitors_ptr[mon_row].json_info_ptr->metric_priority = json_info_ptr->metric_priority;
              json_monitors_ptr[mon_row].json_info_ptr->any_server = json_info_ptr->any_server;
              json_monitors_ptr[mon_row].json_info_ptr->dest_any_server_flag = json_info_ptr->dest_any_server_flag;
              json_monitors_ptr[mon_row].json_info_ptr->frequency = json_info_ptr->frequency;
	      json_monitors_ptr[mon_row].json_info_ptr->is_process = json_info_ptr->is_process;
              MALLOC_AND_COPY(topo_info[topo_idx].topo_tier_info[tier_idx].tier_name, json_monitors_ptr[mon_row].json_info_ptr->tier_name, 
                                           strlen(topo_info[topo_idx].topo_tier_info[tier_idx].tier_name) + 1, "JSON INFO ptr g_mon_id", -1);
              json_monitors_ptr[mon_row].json_info_ptr->tier_idx=tier_idx;
              MALLOC_AND_COPY(topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_disp_name, json_monitors_ptr[mon_row].json_info_ptr->server_name, strlen(topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_disp_name) + 1,"server", -1);
              json_monitors_ptr[mon_row].json_info_ptr->server_index=server_idx;


              if(json_info_ptr->javaHome)
              {
                MY_MALLOC_AND_MEMSET(json_monitors_ptr[mon_row].json_info_ptr->javaHome, (strlen(json_info_ptr->javaHome)+ 1), "JSON Info Ptr JavaHome", -1);
                strcpy(json_monitors_ptr[mon_row].json_info_ptr->javaHome, json_info_ptr->javaHome);
              }
              if(json_info_ptr->javaClassPath)
              {
                MY_MALLOC_AND_MEMSET(json_monitors_ptr[mon_row].json_info_ptr->javaClassPath, (strlen(json_info_ptr->javaClassPath)+ 1), "JSON Info Ptr javaClassPath", -1);

                strcpy(json_monitors_ptr[mon_row].json_info_ptr->javaClassPath, json_info_ptr->javaClassPath);
              }
              if(json_info_ptr->config_json)
              {
                MALLOC_AND_COPY(json_info_ptr->config_json, json_monitors_ptr[mon_row].json_info_ptr->config_json, strlen(json_info_ptr->config_json)+1, "JSON Info config json", -1);
              }
              if(json_info_ptr->mon_name)
              {
                MALLOC_AND_COPY(json_info_ptr->mon_name, json_monitors_ptr[mon_row].json_info_ptr->mon_name, strlen(json_info_ptr->mon_name)+1, "JSON Info mon name", -1);
              }
              if(json_info_ptr->init_vector_file)
              {
                MALLOC_AND_COPY(json_info_ptr->init_vector_file, json_monitors_ptr[mon_row].json_info_ptr->init_vector_file, strlen(json_info_ptr->init_vector_file)+1, "JSON Info init vector file", -1);
              }
              if (arr_ret == 1) //this case is for destination any server 
              {
                REALLOC_AND_COPY(instance_name_arr[mon_buf_id] ,json_monitors_ptr[mon_row].json_info_ptr->instance_name,strlen(instance_name_arr[mon_buf_id]) + 1,"Copying destination server ip for instance",0);
              }
              else
              {
                if(json_info_ptr->instance_name)
                {
                  MALLOC_AND_COPY(json_info_ptr->instance_name, json_monitors_ptr[mon_row].json_info_ptr->instance_name, strlen(json_info_ptr->instance_name)+1, "JSON Info mon name", -1);
                }
              }
              if(namespace[0] != '\0')
              {
                MALLOC_AND_COPY(namespace, json_monitors_ptr[mon_row].json_info_ptr->namespace, strlen(namespace)+1, "JSON Info namespace", -1);
              }
            //MALLOC_AND_COPY(json_info_ptr->tier_name, json_monitors_ptr[mon_row].json_info_ptr->tier_name, strlen(json_info_ptr->tier_name),"JSON INFO ptr g_mon_id", -1);
            //json_monitors_ptr[mon_row].json_info_ptr->tier_idx=json_info_ptr->tier_idx;
            }
          }
          else
          {
            strcpy(temp_monitor_buf,monitor_buf_arr[mon_buf_id]);
            //this is done for destination tier name for multiple instances name
            if (arr_ret == 1)
            {
              REALLOC_AND_COPY(instance_name_arr[mon_buf_id] ,json_info_ptr->instance_name,strlen(instance_name_arr[mon_buf_id]) + 1,"Copying destination server ip for instance",0);
            }
            if(kw_set_standard_monitor("STANDARD_MONITOR", monitor_buf_arr[mon_buf_id], runtime_flag, pod_name, err_msg,json_info_ptr) < 0 )
              MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                                    "Failed to apply json auto monitor with buffer: %s . Error: %s", temp_monitor_buf, err_msg);
          }
        }
        arr_ret = 0;
        topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->auto_monitor_applied = 1;

        FREE_AND_MAKE_NULL( monitor_buf_arr ,"Freeing monitor buf arr", -1); //free 2d array
        FREE_AND_MAKE_NULL( instance_name_arr ,"Freeing instance name arr", -1); //free 2d array

        FREE_AND_MAKE_NULL(json_info_ptr->vectorReplaceTo, "vectorReplaceTO in JSON", -1);
        FREE_AND_MAKE_NULL(json_info_ptr->vectorReplaceFrom, "Freeing vectorReplaceFrom", -1);
        FREE_AND_MAKE_NULL(json_info_ptr->init_vector_file, "Freeing initVectorFile", -1);
        FREE_AND_MAKE_NULL(json_info_ptr->javaHome, "Freeing javaHome", -1);
        FREE_AND_MAKE_NULL(json_info_ptr->javaClassPath, "Freeing javaClasspath", -1);
        FREE_AND_MAKE_NULL(json_info_ptr->config_json, "Freeing configJson", -1);
        FREE_AND_MAKE_NULL(json_info_ptr->mon_name, "Freeing json_info_ptr->mon_name", -1);
        FREE_AND_MAKE_NULL(json_info_ptr->tier_name, "Freeing json_info_ptr->tier_name", -1);
        FREE_AND_MAKE_NULL(json_info_ptr->server_name, "Freeing json_info_ptr->server_name", -1); 
        CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
      }
      if(ret == -1)
      { 
        if(((int)json->error)!=6)
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                         "Can't able to apply monitors on Tier [%s] from json [%s] due to error: %s.",topo_info[topo_idx].topo_tier_info[tier_idx].tier_name, global_settings->auto_json_monitors_filepath, nslb_json_strerror(json));
            return -1;
        }
        else
        {
          CLOSE_ELEMENT_ARR_OF_MONITORS_JSON(json);
          CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
          continue;
        }
      }
      CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json)
    }
    CLOSE_ELEMENT_ARR_OF_MONITORS_JSON(json);
    CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
  }
  if(ret == -1)
  {
    if(((int)json->error)!=6)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                       "Can't able to apply monitors on Tier [%s] from json [%s] due to error: %s.", topo_info[topo_idx].topo_tier_info[tier_idx].tier_name, global_settings->auto_json_monitors_filepath, nslb_json_strerror(json));
      return -1;
    }
    else
      return 0;
  }
  topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->auto_monitor_applied = 1;

  return 0;
}

//this function is used for kubernetes monitors in which we add monitor on pod other than default app-name
void add_delete_kubernetes_monitor_on_process_diff(char *appname, char *tier_name, char *mon_name, int delete_flag)
{
  int mon_id;
  int vector_id;
  int optimize_check = 0;  //ip_mon_check
  int server_info_index = -1;
  int appname_len=-1;

  char gdf_name[MAX_DATA_LINE_LENGTH];
  char err_msg[MAX_DATA_LINE_LENGTH];
  char fname[MAX_LINE_LENGTH];

  CM_info *cus_mon_ptr = NULL;
  CM_vector_info *vector_list = NULL;
  StdMonitor *std_mon_ptr = NULL;

  appname_len = strlen(appname);

  if(delete_flag == 0)
  {
    //we are loop reverse as we know that NA_KUBER.gdf monitor found last index of cm info ptr.
    for(mon_id = total_monitor_list_entries -1; mon_id >= 0; mon_id--)
    {
      cus_mon_ptr = monitor_list_ptr[mon_id].cm_info_mon_conn_ptr;
      //this check is used for ND monitor because for ND monitor will set tier_name is NULL
      if (cus_mon_ptr->tier_name != NULL )
      {
        //here we check conn state tier anme and gdf name of ip monitor if all are true then we apply monitor otherwise not.
        if((cus_mon_ptr->conn_state != CM_DELETED) && (strcmp(cus_mon_ptr->tier_name, tier_name) == 0) && (cus_mon_ptr->flags & NA_KUBER)) // tier_name and deleted 
        {
          vector_list = cus_mon_ptr->vector_list;
          for(vector_id =0; vector_id < cus_mon_ptr->total_vectors; vector_id++)
          {
            //here we check vector state and appname if matches then apply monitor
            if((vector_list[vector_id].kube_info->server_idx != -1) && (vector_list[vector_id].vector_state != CM_DELETED) && (strncmp(appname,vector_list[vector_id].kube_info->app_name,appname_len) == 0))  //vector_deleted is not
            {
              server_info_index = vector_list[vector_id].kube_info->server_idx;

              if(total_mon_config_list_entries > 0)
              {
                mj_apply_monitors_on_autoscaled_server(vector_list[vector_id].vector_name, server_info_index,cus_mon_ptr->tier_index, appname);
              }
              else
              {
                add_json_monitors(vector_list[vector_id].vector_name, server_info_index, cus_mon_ptr->tier_index,appname, 1, 0, 0);
              }
              MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Calling add_json_monitors_function for single vector name is %s",vector_list[vector_id].vector_name);
              optimize_check=1;
            }
          }
        }
        else
        {
          //we know that all multiple entries of same monitor are together so when we found one monitor name then its means all same type of monitor are present next to previous one and when we get another type monitor name then we skip all other monitor so that we dont have to loop all monitor
          if(optimize_check == 1)
            break;
        }
      }
    }
  }
  else
//else
  {
    //by default we are checking for LinuxEx
    if((std_mon_ptr = get_standard_mon_entry(mon_name ,"LinuxEx",1, err_msg)) == NULL)
    {
      sprintf(err_msg, "Error: Standard Monitor Name '%s' or server type LinuxEx is incorrect\n", mon_name);
      strcat(err_msg, "We are assuming that Kubernates monitor type is Linux\n");
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION, "Error %s",err_msg);
      err_msg[0]='\0';
    }
    else
    {
      strcpy(gdf_name,std_mon_ptr->gdf_name);
      sprintf(fname, "%s/sys/%s", g_ns_wdir, gdf_name); //store complete path because in cus mon ptr gdf name save with full path
      //deleting all monitors applied on deleted pod
      for(mon_id = 0; mon_id < total_monitor_list_entries; mon_id++)
      {
        cus_mon_ptr = monitor_list_ptr[mon_id].cm_info_mon_conn_ptr;

        if((strcmp(cus_mon_ptr->gdf_name,fname) == 0) && (strcmp(cus_mon_ptr->tier_name, tier_name) == 0) && (strncmp(appname,cus_mon_ptr->pod_name,appname_len) == 0))
        {
          ns_cm_monitor_log(EL_F, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_INV_DATA, EVENT_MINOR,
          "Going to delete '%s', as this was applied on pod '%s' and this pod is deleted or monitor is deleted from this pod.", cus_mon_ptr->monitor_name, cus_mon_ptr->pod_name);

          //add this for any server we have to remove also entry from hash table of any_server
          if(cus_mon_ptr->any_server)
          {
            mj_delete_specific_server_hash_entry(cus_mon_ptr);
            /*
            //This is the case when ENABLE_AUTO_JSON_MONITOR mode 2 is on.
            if(total_mon_config_list_entries > 0)
            {
              if(cus_mon_ptr->instance_name == NULL)
                sprintf(tmp_buf, "%s%c%s%c%s", cus_mon_ptr->gdf_name_only, global_settings->hierarchical_view_vector_separator, cus_mon_ptr->tier_name, global_settings->hierarchical_view_vector_separator, cus_mon_ptr->g_mon_id);
else
               sprintf(tmp_buf, "%s%c%s%c%s%c%s", cus_mon_ptr->gdf_name_only, global_settings->hierarchical_view_vector_separator, cus_mon_ptr->instance_name, global_settings->hierarchical_view_vector_separator, cus_mon_ptr->tier_name, global_settings->hierarchical_view_vector_separator, cus_mon_ptr->g_mon_id);
            }
            else
            {
              if(cus_mon_ptr->instance_name == NULL)
                sprintf(tmp_buf, "%s%c%s", cus_mon_ptr->gdf_name_only, global_settings->hierarchical_view_vector_separator, cus_mon_ptr->tier_name);
              else
                sprintf(tmp_buf, "%s%c%s%c%s", cus_mon_ptr->gdf_name_only, global_settings->hierarchical_view_vector_separator, cus_mon_ptr->instance_name, global_settings->hierarchical_view_vector_separator, cus_mon_ptr->tier_name);
            }

            nslb_delete_norm_id_ex(specific_server_id_key, tmp_buf, strlen(tmp_buf), &norm_id);

            NSDL1_MON(NULL, NULL, " Monitor tmp_buf '%s' deleted from hash table for server '%s'\n", tmp_buf, cus_mon_ptr->server_name);
            */
          }

          handle_monitor_stop(cus_mon_ptr, MON_DELETE_ON_REQUEST);
          monitor_runtime_changes_applied=1;
          monitor_deleted_on_runtime = 2;
        }
      }
    }
  }
}




int kw_set_continue_on_monitor_error(char *buf, char *err_msg, int runtime_flag)
{
  char key[DYNAMIC_VECTOR_MON_MAX_LEN] = "";
  char custom_or_standard_mon_mode[32];
  char dyn_mon_mode[32];
  char pre_test_check_mon_mode[32];
  int num = 0;
  int cust_mon_mode = 0;
  int dynamic_mode = 0;
  int pre_test_mon_mode = 0;

  NSDL2_MON(NULL, NULL, "Method called, buf = %s", buf);

  num = sscanf(buf, "%s %s %s %s", key, custom_or_standard_mon_mode, dyn_mon_mode, pre_test_check_mon_mode);

  NSDL2_MON(NULL, NULL, "key = %s, custom_or_standard_mon_mode = %s, dyn_mon_mode = %s, pre_test_check_mon_mode = %s", key, custom_or_standard_mon_mode, dyn_mon_mode, pre_test_check_mon_mode);

  if (num != 4){
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, CONTINUE_ON_MONITOR_ERROR_USAGE, CAV_ERR_1011146, CAV_ERR_MSG_1);
  }
  
  if(ns_is_numeric(custom_or_standard_mon_mode) == 0) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, CONTINUE_ON_MONITOR_ERROR_USAGE, CAV_ERR_1011146, CAV_ERR_MSG_2);
  }

  if(ns_is_numeric(dyn_mon_mode) == 0) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, CONTINUE_ON_MONITOR_ERROR_USAGE, CAV_ERR_1011146, CAV_ERR_MSG_2);
  }

  if(ns_is_numeric(pre_test_check_mon_mode) == 0) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, CONTINUE_ON_MONITOR_ERROR_USAGE, CAV_ERR_1011146, CAV_ERR_MSG_2);
  }

  cust_mon_mode = atoi(custom_or_standard_mon_mode);

  if((cust_mon_mode < 0) || (cust_mon_mode > 1))
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, CONTINUE_ON_MONITOR_ERROR_USAGE, CAV_ERR_1011146, CAV_ERR_MSG_3);
  }

  global_settings->continue_on_mon_failure = cust_mon_mode;
  
  dynamic_mode = atoi(dyn_mon_mode);

  if((dynamic_mode < 0) || (dynamic_mode > 1))
  {
   NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, CONTINUE_ON_MONITOR_ERROR_USAGE, CAV_ERR_1011146, CAV_ERR_MSG_3);
  }

  global_settings->continue_on_dyn_vector_mon_failure = dynamic_mode;
  NSDL2_PARSING(NULL, NULL, "Dynamic monitor Mode = %d", global_settings->continue_on_dyn_vector_mon_failure);

  pre_test_mon_mode = atoi(pre_test_check_mon_mode);

  if((pre_test_mon_mode < 0) || (pre_test_mon_mode > 1))
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, CONTINUE_ON_MONITOR_ERROR_USAGE, CAV_ERR_1011146, CAV_ERR_MSG_3);
  }

  global_settings->continue_on_pre_test_check_mon_failure = pre_test_mon_mode;

  NSDL2_PARSING(NULL, NULL, "Pre Test Check Monitor Mode = %d", global_settings->continue_on_pre_test_check_mon_failure);
  return 0;
}

/********************* NV MONITORS CODE STARTS ***************************************/
//setting HPD port
void get_and_set_hpd_port()
{
  FILE *port_fp;
  char port[6];
  char hpd_file[2*DYNAMIC_VECTOR_MON_MAX_LEN];
  char hpd_root[DYNAMIC_VECTOR_MON_MAX_LEN];
  
  if (getenv("HPD_ROOT") != NULL)
    strcpy(hpd_root, getenv("HPD_ROOT"));
  else
  {
    hpd_port = -1;
    return;
  }

  sprintf(hpd_file, "%s/.tmp/.HPD_MON_PORT", hpd_root);
  
  port_fp = fopen(hpd_file, "r");

  if(port_fp)
  {
    if (fgets(port, MAX_LINE_LENGTH, port_fp) != NULL)
      hpd_port = atoi(port);

    CLOSE_FP(port_fp);
  }
  else
  {
    NSTL1(NULL, NULL, "Error in opening File: %s", hpd_file);
    hpd_port = -1;
    return;
  }
}

//NV monitors are enabled by default.
//If machine is NV then only monitor setup will be done, else will ignore the keyword
int kw_set_nv_enable_monitor(char *keyword, char *buf, char *nv_mon_gdf_name, char *err_msg)
{
  char key[DYNAMIC_VECTOR_MON_MAX_LEN];
  char SendBuffer[10 * DYNAMIC_VECTOR_MON_MAX_LEN];
  int value;

  if(sscanf(buf, "%s %d", key, &value) != 2)
  {
    NS_KW_PARSING_ERR(keyword, 0, err_msg, NV_MONITOR_USAGE, CAV_ERR_1011359, key);
  }

  if(value != 1) //keyword disabled
    return 0;
  
  if((!strncmp(g_cavinfo.config, "NV", 2)) || (strstr(g_cavinfo.SUB_CONFIG, "NV") != NULL)  || (!strncmp(g_cavinfo.SUB_CONFIG, "ALL", 3)))
  {
    if(!hpd_port) //check if hpd port is already obtained or not, if not first get hpd port from file
    {
      get_and_set_hpd_port();
    }
    if (hpd_port == -1)
      return 0;
 
    // TESTING TODO
    //sprintf(SendBuffer, "DYNAMIC_VECTOR_MONITOR %s:%d %s %s 2 cm_rum_stats EOC cm_rum_stats", LOOPBACK_IP_PORT, hpd_port, nv_mon_gdf_name, nv_mon_gdf_name);
    sprintf(SendBuffer, "DYNAMIC_VECTOR_MONITOR Cavisson%c%s %s %s 2 cm_rum_stats EOC cm_rum_stats", global_settings ->hierarchical_view_vector_separator,g_machine, nv_mon_gdf_name, nv_mon_gdf_name);

    NSDL2_MON(NULL, NULL, "Adding %s", SendBuffer);

    kw_set_dynamic_vector_monitor("DYNAMIC_VECTOR_MONITOR", SendBuffer, NULL, 0, 0, NULL, err_msg, NULL, NULL, 0);
  }
  return 0;
}

 
/********************* NV MONITORS CODE ENDS ***************************************/


/* --- END: Method used during Run Time  --- */
/* --- START: NoN Blocking Code for Dynamic Vector Monitor --- */
/*static inline void dyn_mon_make_msg(DynamicVectorMonitorInfo *dynamic_vector_monitor_ptr, char *msg_buf, int *msg_len)
{
  int mon_time_out;
  char owner_name[MAX_STRING_LENGTH];
  char group_name[MAX_STRING_LENGTH];
  int testidx = start_testidx; //storing start_testidx in local variable testidx for test monitoring purpose

  //This done because create server gives error if < 0 frequency
  int frequency = global_settings->progress_secs;

  NSDL2_MON(NULL, NULL, "Method called. Frequency = %d", frequency);
  mon_time_out = dynamic_vector_monitor_timeout;

   In SendMsg buffer, changes cus_mon_ptr->cs_ip to cus_mon_ptr->cavmon_ip in order to pass actual cavmon ip to server instead of passing ip to which connection is established.
  


   *msg_len = sprintf(msg_buf, "init_check_monitor:OWNER=%s;GROUP=%s;MON_NAME=%s;MON_PGM_NAME=%s;MON_PGM_ARGS=%s;MON_OPTION=%d;"                          "MON_ACCESS=%d;MON_TIMEOUT=%d;MON_REMOTE_IP=%s;MON_REMOTE_USER_NAME=%s;MON_REMOTE_PASSWD=%s;"
                     "MON_FREQUENCY=%d;MON_TEST_RUN=%d;MON_NS_SERVER_NAME=%s;MON_CAVMON_SERVER_NAME=%s;MON_NS_FTP_USER=%s;"
                     "MON_NS_FTP_PASSWORD=%s;MON_NS_WDIR=%s;MON_NS_VER=%s;MON_VECTOR_SEPARATOR=%c;MON_PARTITION_IDX=%lld;"
                     "CUR_PARTITION_IDX=%lld;NUM_TX=%d\n",
                     nslb_get_owner(owner_name), nslb_get_group(group_name), dynamic_vector_monitor_ptr->monitor_name,
                     dynamic_vector_monitor_ptr->pgm_name_for_vectors, dynamic_vector_monitor_ptr->pgm_args_for_vectors,
                     RUN_ONLY_ONCE_OPTION, dynamic_vector_monitor_ptr->access, mon_time_out,
                     dynamic_vector_monitor_ptr->rem_ip, dynamic_vector_monitor_ptr->rem_username,
                     dynamic_vector_monitor_ptr->rem_password, frequency, testidx, g_cavinfo.NSAdminIP,
                     dynamic_vector_monitor_ptr->cavmon_ip, get_ftp_user_name(), get_ftp_password(),
                     g_ns_wdir, ns_version, global_settings->hierarchical_view_vector_separator,
                     g_start_partition_idx, g_partition_idx, total_tx_entries);

}
*/
/*static inline void dyn_mon_free_partial_buf(DynamicVectorMonitorInfo *dynamic_vector_monitor_ptr)
{

  NSDL2_MON(NULL, NULL, "Method called, dynamic_vector_monitor_ptr = %p partial_buffer %p",
                         dynamic_vector_monitor_ptr, dynamic_vector_monitor_ptr->partial_buffer);
  dynamic_vector_monitor_ptr->send_offset = 0;
  dynamic_vector_monitor_ptr->bytes_remaining = 0;

}*/

void kw_set_dvm_malloc_delta_size(char *buff)
{
  int size = 0, num;
  char keyword[MAX_DATA_LINE_LENGTH];
  num = sscanf(buff, "%s %d", keyword, &size);
  if((num < 2) || (size <= 0))
    g_dvm_malloc_delta_entries = 10;
  else
    g_dvm_malloc_delta_entries = size;

  MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "g_dvm_malloc_delta_entries = %d", g_dvm_malloc_delta_entries); 
}

void kw_set_server_health(char *keyword, char *buf, char *err_msg)
{
  int num, j=0, flag = 0;
  char *token_arr[15];
  char server_name[MAX_LENGTH];
  char file_name[MAX_LENGTH];
  char instance[MAX_LENGTH];
  char stat_type[MAX_LENGTH];
  char tier_name[MAX_LENGTH];
  char data_name[MAX_LENGTH];
  char program_name[50];
  char SendBuffer[MAX_MONITOR_BUFFER_SIZE];

  num = get_tokens_with_multi_delimiter(buf, token_arr, " ", 15);

  while(j < num)
  {
    if((strcmp(token_arr[j],"-S"))==0)
      sprintf(server_name,"%s",token_arr[j+1]);

    else if((strcmp(token_arr[j],"-f"))==0)
      sprintf(file_name,"%s",token_arr[j+1]);

    else if((strcmp(token_arr[j],"-i"))==0)
      sprintf(instance,"%s",token_arr[j+1]);

    else if((strcmp(token_arr[j],"-s"))==0)
      sprintf(stat_type,"%s",token_arr[j+1]);

    else if((strcmp(token_arr[j],"-t"))==0)
      sprintf(tier_name,"%s",token_arr[j+1]);

    else if((strcmp(token_arr[j],"-D"))==0)
      sprintf(data_name,"%s",token_arr[j+1]);
    j++;
  }

 //Checking Mandatory arguments 
  if(server_name[0] == '\0')
  {
    strcat(err_msg,"\nMandatory argument missing. Please give server name with -S argument");
    flag=1;
  }
  if(file_name[0] == '\0')
  { 
    strcat(err_msg,"\nMandatory argument missing. Please give Configuration file name with -f argument");
    flag=1;
  }
  if(instance[0] == '\0')
  {
    strcat(err_msg,"\nMandatory argument missing. Please give Instance name with -i argument");
    flag=1;
  }
  if(tier_name[0] == '\0')
  {
    strcat(err_msg,"\nMandatory argument missing. Please give Tier name with -t argument");
    flag=1;
  }
  if(stat_type[0] == '\0')
  {
    strcat(err_msg,"\nMandatory argument missing. Please give stat type  with -s argument");
    flag=1;
  }
  NSTL1_OUT(NULL, NULL, "%s\n",err_msg);

  if(flag==1)
    return; 
  

  if((strcmp(stat_type,"server"))==0)
    sprintf(program_name,"ServerHealthStats");
  else if((strcmp(stat_type,"overall"))==0)
    sprintf(program_name,"OverallServerHealthStats"); 
 
  sprintf(SendBuffer, "STANDARD_MONITOR %s%c%s %s%c%s%c%s %s -f %s -s %s -D %s", tier_name,global_settings->hierarchical_view_vector_separator, server_name, tier_name, global_settings->hierarchical_view_vector_separator, server_name, global_settings->hierarchical_view_vector_separator, instance, program_name, file_name, stat_type,data_name);

  NSDL2_MON(NULL, NULL, "Adding %s", SendBuffer);

  kw_set_standard_monitor("STANDARD_MONITOR", SendBuffer, 0, 0,err_msg, NULL);

}

/* --- END: NoN Blocking Code for Dynamic Vector Monitor --- */



/*--------------- BEGIN: NetCloud IP Monitor -----------------

void kw_enable_netcloud_ip_monitor(char *keyword, char *buf)
{
  char key[DYNAMIC_VECTOR_MON_MAX_LEN] = "";
  char SendBuffer[DYNAMIC_VECTOR_MON_MAX_LEN];
  int value;

  if(sscanf(buf, "%s %d", key, &value) != 2)
  {
    NSTL1_OUT(NULL, NULL, "Error: Too few/more arguments for %s keywords.", key);
    exit (-1);
  }

  if(value != 1)  //keyword disabled. No need to process further.
    return;

  //ip/port does not make any sense here, because this monitor will not make any connection. This monitor data will come on parent-generator connection.
  sprintf(SendBuffer, "DYNAMIC_VECTOR_MONITOR %s:-1 NetCloudIPData ns_ip_data.gdf 2 ns_ip_data EOC ns_ip_data", LOOPBACK_IP_PORT );

  NSDL2_MON(NULL, NULL, "Adding %s", SendBuffer);

  kw_set_dynamic_vector_monitor("DYNAMIC_VECTOR_MONITOR", SendBuffer, NULL, 0, 0, NULL, NULL, 0); 
}

-------------- END: NetCloud IP Monitor ---------------------*/
