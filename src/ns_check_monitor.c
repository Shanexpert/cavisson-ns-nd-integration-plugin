/******************************************************************
 * Name    : ns_check_monitor.c
 * Author  : Archana
 * Purpose : This file contains methods related to
             parsing keyword, setup the connection with Create Server
             at run time for check monitors
 * Note:
 * Modification History:
 * 02/04/09 - Initial Version
 * 05/06/09 - Modification Version
*****************************************************************/
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <libgen.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <regex.h>
#include <errno.h>
#include <v1/topolib_structures.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "nslb_util.h"
#include "nslb_sock.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"
#include "ns_check_monitor.h"
#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "ns_log.h"
#include "tmr.h"
#include "timing.h"
#include "ns_dynamic_vector_monitor.h"
#include "ns_custom_monitor.h"
#include "util.h"
#include "ns_event_log.h"
#include "ns_schedule_phases.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "ns_alloc.h"
#include "wait_forever.h"
#include "nslb_cav_conf.h"
#include "ns_gdf.h"
#include "ns_user_monitor.h"
#include "ns_server_admin_utils.h"
#include "ns_check_monitor.h"
#include "ns_mon_log.h"
#include "ns_pre_test_check.h"
#include "ns_string.h"
#include "ns_event_id.h"
#include "ns_batch_jobs.h"
#include "ns_parent.h"
#include "ns_ndc_outbound.h"
#include "ns_trace_level.h"
#include "ns_exit.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"
#include "nslb_alert.h"
#include "ns_appliance_health_monitor.h"
#include "ns_monitor_profiles.h"

#define FTP_BUFFER_SIZE 10240

#define SKIP_EPOLL 0
#define CHECK_MON_EPOLL 1
#define PARENT_MON_EPOLL 2
#define CHK_MON_START_MON_ID 50000

char pid_received_from_ndc = 0;

int total_check_monitors_entries = 0;  // Total Check monitor enteries
int max_check_monitors_entries = 0;    // Max Check monitor enteries
CheckMonitorInfo *check_monitor_info_ptr = NULL;
CheckMonitorInfo *pre_test_check_info_ptr;
int is_norm_table_init_done = 0;
int *chk_mon_id_map_table;
NormObjKey *mon_id_key;

/* ------------------ Global variable for Check monitor Disaster Recovery ---------------------- */

// This table will contain index of those custom monitor which are exited during the test due
static CheckMonitorInfo *chk_mon_dr_table[CHK_MON_DR_ARRAY_SIZE];      
// This is a temprary table, used to store exited custom monitor entry for next retry
static CheckMonitorInfo *chk_mon_dr_table_tmp[CHK_MON_DR_ARRAY_SIZE];    

// check monitor retry flag, will retry connection once closed or not
static int chk_mon_retry_flag = 0;                  
//check monitor retry count
static int max_chk_mon_retry_count = 0;             
int g_total_aborted_chk_mon_conn = 0; 
static int num_aborted_chk_mon_non_recoved = 0;
int chk_mon_send_msg(CheckMonitorInfo *chk_mon_ptr, int init_flag);
int chk_mon_send_msg_v2(CheckMonitorInfo *chk_mon_ptr, int init_flag);
/* ------------------ Global variable for Check monitor Disaster Recovery End ------------------ */


/* --- START: Method used during Parsing Time  ---*/

static void usage(char *error, int runtime_flag, char *err_msg)
{
  NSDL2_MON(NULL, NULL, "Method called");
  sprintf(err_msg, "\n%s", error);
  sprintf(err_msg, "Usage:\n");
  sprintf(err_msg, "CHECK_MONITOR <Server Name> <Check monitor Name> <From Event> <Phase Name> <Frequency {Run Periodic (1) | Never (2)}> <Periodicity (HH:MM:SS)> <End Event> [<Max Count> | <End Phase Name>] <Program Path> [Program Arguments] \n");
  sprintf(err_msg, "Where \n");
  sprintf(err_msg, "From Event:\n  1 - 'Before test is started'\n  2 - 'Start of Test'\n  3 - 'On Start of the Phase'\n  90 - 'After test is Over'\n");
  sprintf(err_msg, "Phase Name:\n  If From Event 'On Start of the Phase (3)' then use phase name\n  Except 'On Start of the Phase (3)' use NA\n");
  sprintf(err_msg, "Periodicity:\n  NA - if Frequency 'Never (2)'\n  If Frequency 'Run Periodic (1)' then use periodicity in HH:MM:SS\n");
  sprintf(err_msg, "End Event:\n  NA - if Frequency 'Never (2)'\n  If Frequency 'Run Periodic (1)' then use:\n    Till test completion (1)\n    Complete specified executions (2)\n    Till Completion of Phase (3)\n");
  sprintf(err_msg, "Max Count:\n  NA - if Frequency 'Never (2)'\n  If Frequency 'Run Periodically (1)' then:\n    NA - if 'End Event 1'\n    Run for specified Max Count if 'End Event 2'\n    Run for specified End Phase Name if 'End Event 3'\n");
  sprintf(err_msg, "Example:\n");
  sprintf(err_msg, "CHECK_MONITOR Server1 checkDBServer 1 NA 2 NA NA NA /home/tools/dbcheck.sh -c\n\n");
}

#ifdef NS_DEBUG_ON
// Use in pre test and check monitor and post test monitor files
char s_check_monitor_buf[MAX_CM_BUFFER_LENGTH];

//To convert CheckMonitorInfo to string for printing Check monitor Info
char *CheckMonitor_to_string(CheckMonitorInfo *check_monitor_ptr, char *buf)
{
  sprintf(buf, "Check monitor fd = %d, State = %d, Check Monitor Name = %s, From event = %d, Start Phase Name = %s, Option = %s, Max Count = %d, End Phase Name = %s, Create Server IP = %s:%d, Program Name (with args) = %s %s", check_monitor_ptr->fd, check_monitor_ptr->state, check_monitor_ptr->check_monitor_name, check_monitor_ptr->from_event, check_monitor_ptr->start_phase_name, (check_monitor_ptr->option == RUN_LOCAL_ACCESS)?"Run Periodic":"Never", check_monitor_ptr->max_count, check_monitor_ptr->end_phase_name, check_monitor_ptr->cs_ip, check_monitor_ptr->cs_port, check_monitor_ptr->pgm_path, check_monitor_ptr->pgm_args);

  return(buf);
}
#endif

static char *chk_mon_event_msg(CheckMonitorInfo *check_monitor_ptr)
{
  static char chk_mon_event_msg_buf[3 * 1024] = "\0";
  char src_add[1024] = "\0";

  strcpy(src_add, nslb_get_src_addr(check_monitor_ptr->fd));
  sprintf(chk_mon_event_msg_buf, "%s %s, source address '%s',  destination address '%s:%d'.",
                             (check_monitor_ptr->monitor_type == CHECK_MON_IS_SERVER_SIGNATURE)?
                             "Server Signature":"Check Monitor", check_monitor_ptr->check_monitor_name, 
                             (!strcmp(src_add, "Unknown or Not Connected") || (src_add == NULL))?g_machine:src_add, check_monitor_ptr->cs_ip,
                               check_monitor_ptr->cs_port);

  return(chk_mon_event_msg_buf);
}



//This method is to add check monitor info into the check monitor table.
//Signature Name should be unique 
//Check monitor name should be unique but do later
int add_check_monitor(CheckMonitorInfo *check_monitor_ptr, char *check_mon_name, int from_event, char *start_phase_name, char *pgm_path, char *cs_ip, int access, char *rem_ip, char *rem_username, char *rem_password, char *pgm_args, int option, int periodicity, char *end_event, char *max_count_or_end_phase_name, int monitor_type, char *origin_cmon, int runtime_flag, char *err_msg, int server_index, JSON_info *json_info_ptr,int tier_idx)
{
  char *ptr[2] ; // To split cs_ip and cs_port
  char temp_usage_buf[MAX_LENGTH];  //this is just for readable usage
  char *hv_ptr = NULL;
  char check_mon_name_buf[MAX_LENGTH];
  char *field[20]; //for tier name and server name storage

  int cs_port = g_cmon_port;
  int i;


  NSDL2_MON(NULL, NULL, "Method called. check_monitor_name = %s, from_event = %d, start_phase_name = %s, option = %d, periodicity = %d, end_event = %s, max_count_or_end_phase_name = %s, cs_ip = %s, access = %d, rem_ip = %s, rem_username = %s, rem_password = %s, pgm_path = %s, pgm_args = %s", check_mon_name, from_event, start_phase_name, option, periodicity, end_event, max_count_or_end_phase_name, cs_ip, access, rem_ip, rem_username, rem_password, pgm_path, pgm_args);

  memset(ptr, 0, sizeof(ptr));  // To initize the ptr with NULL.
  get_tokens(cs_ip, ptr, ":", 2);

 /* if(ptr[0])   
    strcpy(cs_ip, ptr[0]);   //create server IP
*/
  // We should get cs_port using server name - To be done later
  if(ptr[1])
    cs_port = atoi(ptr[1]);  //create server port

  memset((check_monitor_ptr), 0, sizeof(CheckMonitorInfo));
  check_monitor_ptr->con_type = NS_STRUCT_CHECK_MON;
  for (i = 0; i < total_pre_test_check_entries ; i++)
  {
    //SERVER SIGNATURE
    if((pre_test_check_info_ptr[i].monitor_type == CHECK_MON_IS_SERVER_SIGNATURE) || ( pre_test_check_info_ptr[i].monitor_type == INTERNAL_CHECK_MON))
    {
      //NSDL2_MON(NULL, NULL, "Checking if any Signature Name is not unique. check_monitor_ptr[%d].check_monitor_name = %s, check_mon_name = %s", i, pre_test_check_info_ptr[i].check_monitor_name, check_mon_name);
      if (!strcmp(pre_test_check_info_ptr[i].check_monitor_name, check_mon_name))
      {
        sprintf(err_msg, "Duplicate Signature Name (%s) for SERVER_SIGNATURE keyword. Signature Name should be unique.\n", check_mon_name);
        CHK_MON_RUNTIME_RETURN_OR_EXIT(runtime_flag, err_msg, 1);
      }
    }
  }
    
  MY_MALLOC(check_monitor_ptr->check_monitor_name, (strlen(check_mon_name) + 1), "Check monitor Name", -1); 
  strcpy(check_monitor_ptr->check_monitor_name, check_mon_name);
 
  strcpy(check_mon_name_buf,check_mon_name); 
  //here we fill tier name and server name of check monitor or batch job monitor
  if(strchr(check_mon_name_buf, '>'))
  {
    get_tokens(check_mon_name_buf, field, ">", 3);

    MALLOC_AND_COPY( field[0] ,check_monitor_ptr->tier_name, (strlen(field[0]) + 1), "Copying tier name", -1);
    MALLOC_AND_COPY( field[1] ,check_monitor_ptr->server_name, (strlen(field[1]) + 1), "Copying server name", -1);    
    check_monitor_ptr->tier_idx=tier_idx;
    check_monitor_ptr->server_index=server_index;    
  }

  check_monitor_ptr->any_server = false;

  if(json_info_ptr)
  { 
     if(json_info_ptr->instance_name)
     { 
       MY_MALLOC(check_monitor_ptr->instance_name, (strlen(json_info_ptr->instance_name) + 1), "Check monitor Name", -1); 
       strcpy(check_monitor_ptr->instance_name, json_info_ptr->instance_name);
     }
  
     if(json_info_ptr->mon_name)
     {
        MY_MALLOC(check_monitor_ptr->mon_name, (strlen(json_info_ptr->mon_name) + 1), "Check monitor Name", -1); 
        strcpy(check_monitor_ptr->mon_name, json_info_ptr->mon_name);
     }
  
     if(json_info_ptr->args)
     {
        MY_MALLOC(check_monitor_ptr->json_args, (strlen(json_info_ptr->args) + 1), "Check monitor Name", -1);
        strcpy(check_monitor_ptr->json_args, json_info_ptr->args);
     }

     if(json_info_ptr->g_mon_id)
     {
       MALLOC_AND_COPY(json_info_ptr->g_mon_id, check_monitor_ptr->g_mon_id, (strlen(json_info_ptr->g_mon_id) + 1),
                                                     "g_mon_id in Check monitor", 0);
     }
     else
        MALLOC_AND_COPY("-1", check_monitor_ptr->g_mon_id, 2, "g_mon_id in Check monitor", 0);

     check_monitor_ptr->any_server = json_info_ptr->any_server; 
     check_monitor_ptr->mon_info_index = json_info_ptr->mon_info_index; 
  }
  else
    MALLOC_AND_COPY("-1", check_monitor_ptr->g_mon_id, 2, "g_mon_id in Check monitor", 0);

  check_monitor_ptr->from_event = from_event;

  MY_MALLOC(check_monitor_ptr->start_phase_name, (strlen(start_phase_name) + 1), "Start Phase Name", -1); 
  if((check_monitor_ptr->from_event == CHECK_MONITOR_EVENT_BEFORE_TEST_IS_STARTED) || (check_monitor_ptr->from_event == CHECK_MONITOR_EVENT_AFTER_TEST_IS_OVER) || (check_monitor_ptr->from_event == CHECK_MONITOR_EVENT_START_OF_TEST))
    strcpy(check_monitor_ptr->start_phase_name, "NA");
  else
    strcpy(check_monitor_ptr->start_phase_name, start_phase_name);

  //Only support Never(2) option for "Before test is started"(1) and "After test is Over"(2)
  if((check_monitor_ptr->from_event == CHECK_MONITOR_EVENT_BEFORE_TEST_IS_STARTED) || (check_monitor_ptr->from_event == CHECK_MONITOR_EVENT_AFTER_TEST_IS_OVER))
  {
    if(option != RUN_ONLY_ONCE_OPTION)
    {
      sprintf(err_msg, "Error in Check Monitor '%s' keyword: Only supported Option 'Never (2)' with Start event 'Before test is started (1)' and 'After test is Over (2)'\n", check_mon_name);
      CHK_MON_RUNTIME_RETURN_OR_EXIT(runtime_flag, err_msg, 1);
    }
  }
  check_monitor_ptr->option = option;

  MY_MALLOC(check_monitor_ptr->end_phase_name, (strlen(max_count_or_end_phase_name) + 1), "End Phase Name", -1);
  if(check_monitor_ptr->option == RUN_ONLY_ONCE_OPTION)
  {
    if(!strcmp(end_event, "NA"))
    {
      if(!strcmp(max_count_or_end_phase_name, "NA"))
      {
        check_monitor_ptr->max_count = -1;
        strcpy(check_monitor_ptr->end_phase_name, "");
      }
      else
      {
        sprintf(temp_usage_buf, "Error: Wrong Max Count for Check Monitor '%s'\n", check_mon_name);
        usage(temp_usage_buf, runtime_flag, err_msg);
        CHK_MON_RUNTIME_RETURN_OR_EXIT(runtime_flag, err_msg, 1);
      }
    }
    else
    {
      sprintf(temp_usage_buf, "Error: Wrong End event for Check Monitor '%s'.\n", check_mon_name);
      usage(temp_usage_buf, runtime_flag, err_msg);
      CHK_MON_RUNTIME_RETURN_OR_EXIT(runtime_flag, err_msg, 1);
    }
  NSDL2_MON(NULL, NULL, "check_monitor_ptr->option = %d, end_event = %s, max_count_or_end_phase_name = %s, check_monitor_ptr->max_count = %d, check_monitor_ptr->end_phase_name = %s", check_monitor_ptr->option, end_event, max_count_or_end_phase_name, check_monitor_ptr->max_count, check_monitor_ptr->end_phase_name);
  }
  else
  {
    if(atoi(end_event) == TILL_TEST_COMPLETION)
    {
      if(!strcmp(max_count_or_end_phase_name, "NA"))
      {
        check_monitor_ptr->max_count = CHECK_COUNT_FOREVER;
        strcpy(check_monitor_ptr->end_phase_name, "");
      }
      else
      {
        sprintf(temp_usage_buf, "Error: Wrong Max Count for Check Monitor '%s'.\n", check_mon_name);
        usage(temp_usage_buf, runtime_flag, err_msg);
        CHK_MON_RUNTIME_RETURN_OR_EXIT(runtime_flag, err_msg, 1);
      }
    }
    else if(atoi(end_event) == COMPLETE_SPECIFIED_EXECUTIONS)
    {
      check_monitor_ptr->max_count = atoi(max_count_or_end_phase_name);
      strcpy(check_monitor_ptr->end_phase_name, "");
      if(check_monitor_ptr->max_count <= 0)
      {
        sprintf(temp_usage_buf, "Error: Wrong Max Count for Check Monitor '%s'.\n", check_mon_name);
        usage(temp_usage_buf, runtime_flag, err_msg);
        CHK_MON_RUNTIME_RETURN_OR_EXIT(runtime_flag, err_msg, 1);
      }
    }
    else if(atoi(end_event) == TILL_COMPLETION_OF_PHASE)
    {
      strcpy(check_monitor_ptr->end_phase_name, max_count_or_end_phase_name);
      check_monitor_ptr->max_count = -1;
    }
    else
    {
      sprintf(temp_usage_buf, "Error: Wrong End event for Check Monitor '%s'.\n", check_mon_name); 
      usage(temp_usage_buf, runtime_flag, err_msg);
      CHK_MON_RUNTIME_RETURN_OR_EXIT(runtime_flag, err_msg, 1);
    }
  }

  check_monitor_ptr->periodicity = periodicity;
 
  if(server_index >= 0)
  { 
    MY_MALLOC(check_monitor_ptr->cs_ip, 
             strlen(topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_index].server_ptr->topo_servers_list->server_ip) + 1, 
             "ServerIp in place of display name", -1);
    
    strcpy(check_monitor_ptr->cs_ip, 
           topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_index].server_ptr->topo_servers_list->server_ip);
    
    MY_MALLOC(check_monitor_ptr->server_name, 
              strlen(topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_index].server_disp_name) + 1, 
              "Custom Monitor cs_ip", 0);
    strcpy(check_monitor_ptr->server_name, topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_index].server_disp_name);
    check_monitor_ptr->server_index=server_index;
    check_monitor_ptr->tier_idx=tier_idx;
  }
  else
  {
    if((hv_ptr = strchr(cs_ip, global_settings->hierarchical_view_vector_separator)) != NULL) //hierarchical view
    {
      *hv_ptr = '\0';
      cs_ip = hv_ptr + 1;
    }

    MY_MALLOC(check_monitor_ptr->cs_ip, strlen(cs_ip) + 1, "Custom Monitor cs_ip", -1);
    strcpy(check_monitor_ptr->cs_ip, cs_ip);
  }

  check_monitor_ptr->cs_port = cs_port;
  check_monitor_ptr->access = access;
  check_monitor_ptr->state = CHECK_MON_COMMAND_STATE;
  check_monitor_ptr->server_index = server_index;
  // We are allocating below values even for local access option so that we dont have to validate these in send_msg_to_cs()
  MY_MALLOC(check_monitor_ptr->rem_ip, (strlen(rem_ip) + 1), "Remote access IP", -1); 
  strcpy(check_monitor_ptr->rem_ip, rem_ip);

  MY_MALLOC(check_monitor_ptr->rem_username, (strlen(rem_username) + 1), "Remote access User Name", -1); 
  strcpy(check_monitor_ptr->rem_username, rem_username);

  MY_MALLOC(check_monitor_ptr->rem_password, (strlen(rem_password) + 1), "Remote access password", -1); 
  strcpy(check_monitor_ptr->rem_password, rem_password);
  
  MY_MALLOC(check_monitor_ptr->pgm_path, (strlen(pgm_path) + 1), "Program Path", -1); 
  strcpy(check_monitor_ptr->pgm_path, pgm_path);

  MY_MALLOC(check_monitor_ptr->pgm_args, (strlen(pgm_args) + 1), "Program args", -1); 
  strcpy(check_monitor_ptr->pgm_args, pgm_args);

  check_monitor_ptr->origin_cmon = NULL;
  if(origin_cmon[0] != '\0')
  {
    //For Heroku
    MY_MALLOC(check_monitor_ptr->origin_cmon, (strlen(origin_cmon) + 1), "Origin Cmon", -1); 
    strcpy(check_monitor_ptr->origin_cmon, origin_cmon);
  }

  check_monitor_ptr->fd = -1;
  check_monitor_ptr->status = CHECK_MONITOR_NOT_STARTED;
  check_monitor_ptr->monitor_type = monitor_type;

  check_monitor_ptr->chk_mon_retry_attempts = max_chk_mon_retry_count +1;

  reset_chk_mon_values(check_monitor_ptr);
  //check_monitor_ptr->conn_state = CHK_MON_INIT;
  //check_monitor_ptr->send_offset = 0;

  check_monitor_ptr->ftp_file_size = -1;
  check_monitor_ptr->retry_count = -1;
  check_monitor_ptr->max_retry_count = -1;
  check_monitor_ptr->retry_time_threshold_in_sec = -1;
  check_monitor_ptr->recovery_time_threshold_in_sec = -1;
  check_monitor_ptr->last_retry_time_in_sec = -1;

  check_monitor_ptr->total_chunk_size = 0;
  check_monitor_ptr->mon_id = g_chk_mon_id;
  g_chk_mon_id++;

  NSDL3_MON(NULL, NULL, "Added Check monitor info. check_monitor_name = %s, from_event = %d, start_phase_name = %s, cs_ip = %s, cs_port = %d, access = %d, rem_ip = %s, rem_username = %s, rem_password = %s, pgm_path = %s, pgm_args = %s, option = %d, max_count = %d, end_phase_name = %s, monitor_type = %d, check_monitor_retry_attempts = %d, origin_cmon = %s", check_monitor_ptr->check_monitor_name, check_monitor_ptr->from_event, check_monitor_ptr->start_phase_name, check_monitor_ptr->cs_ip, check_monitor_ptr->cs_port, check_monitor_ptr->access, check_monitor_ptr->rem_ip, check_monitor_ptr->rem_username, check_monitor_ptr->rem_password, check_monitor_ptr->pgm_path, check_monitor_ptr->pgm_args, check_monitor_ptr->option, check_monitor_ptr->max_count, check_monitor_ptr->end_phase_name, check_monitor_ptr->monitor_type, check_monitor_ptr->chk_mon_retry_attempts, origin_cmon);
return 0;
}

/*
 * Description           : check_duplicate_monitor_name() method is used to verify duplication of check monitor name.
 *                         Method can be used for both pre and post test run. If check monitor name duplicate 
 *                         then print error message and exit. (fix done for bug#1510 )
 * Input Parameter       
 *  check_mon_ptr        : Receives CheckMonitorInfo last pointer.
 *  total_monitor_entries: Provide total number of check monitor entries.
 *  check_mon_name       : Receives check monitor name to verify
 * 
 * Output Parameter      : None
 * Return                : Returns 0 on mis-match of name and 1 on match of name.
 * */
int check_duplicate_monitor_name(CheckMonitorInfo *check_mon_ptr, int total_monitor_entries, char *check_mon_name, int runtime_flag, char *err_msg,int tier_idx,int server_index) 
{
  int k;
  
  for(k = 0; k < total_monitor_entries; k++)
  {
    if (check_mon_ptr[k].status != CHECK_MONITOR_STOPPED)
    {
      if (!strcmp(check_mon_ptr[k].check_monitor_name, check_mon_name))
      {
        NSDL2_MON(NULL, NULL, "Checking if any Check Monitor Name is not unique. check_monitor_ptr[%d].check_monitor_name = %s, check_mon_name = %s", k, check_mon_ptr[k].check_monitor_name, check_mon_name);
        sprintf(err_msg, "Duplicate Check monitor name (%s) for CHECK_MONITOR keyword. Check monitor name should be unique.\n", check_mon_name);
        CHK_MON_RUNTIME_RETURN_OR_EXIT(runtime_flag, err_msg, 1);
      }
    }
  }
  return(0);
}

//This method used to parse CHECK_MONITOR keyword. All fields are mandatory except arguments.
int kw_set_check_monitor(char *keyword, char *buf, int runtime_flag, char *err_msg, JSON_info *json_info_ptr)
{
  int num = 0;
  char key[MAX_LENGTH] = "";
  char check_mon_name[MAX_LENGTH] = "";  
  int from_event = CHECK_MONITOR_EVENT_BEFORE_TEST_IS_STARTED; // default 
  char start_phase_name[MAX_LENGTH] = "NA";       
  int frequency = RUN_EVERY_TIME_OPTION;  //means run periodic - default
  char origin_cmon_and_ip_port[MAX_LENGTH] = "NS";       //default is NS
  char server_name[MAX_LENGTH] = {0};
  char origin_cmon[MAX_LENGTH] = {0}; //For Heroku
  char periodicity[MAX_LENGTH] = "NA";   // periodicity 
  int check_periodicity = 0;   // periodicity 
  char end_event[MAX_LENGTH] = "NA";   // End event 
  char max_count_or_phase_name[MAX_LENGTH] = "NA";   // Maximum count
  char pgm_path[MAX_LENGTH] = "";   // Program name with or without path
  char pgm_args[MAX_CM_BUFFER_LENGTH] = "";   // Program arguments if any (optional)
  char *token_arr[500];  // will contain the array of all arguments
  int i = 0, j = 0;               //just for storing the number of tokens
  char temp_buf[MAX_CM_BUFFER_LENGTH] = "";
  int check_monitor_row = 0;  // this will contains the newly created row-index of table
  char temp_usage_buf[MAX_BUF_LENGTH];  //this is just for readable usage

  int access = RUN_LOCAL_ACCESS;
  char rem_ip[1024] = "NA";
  char rem_username[256] = "NA";
  char rem_password[256] = "NA";
  int ret; 
  int server_index = -1;
  char tmp_buf[MAX_DATA_LINE_LENGTH];
  int tier_idx;
  
  NSDL2_MON(NULL, NULL, "Method called, keyword = %s, buf = %s", keyword, buf);

  // Last field temp_buf is used to detect is there are any args
  num = sscanf(buf, "%s %s %s %d %s %d %s %s %s %s %s", key, origin_cmon_and_ip_port, check_mon_name, &from_event, start_phase_name, &frequency, periodicity, end_event, max_count_or_phase_name, pgm_path, temp_buf);
  

  if(json_info_ptr)
  {
    tier_idx=json_info_ptr->tier_idx;
    server_index=json_info_ptr->server_index;
  }
   
  else
  { 
    if(find_tier_idx_and_server_idx(origin_cmon_and_ip_port,server_name, origin_cmon, global_settings->hierarchical_view_vector_separator, hpd_port, &server_index, &tier_idx) == -1)
    {
      sprintf(err_msg, "Server (%s) not present in topolgy, for the Monitor (%s).\n", server_name, pgm_path);
      CHK_MON_RUNTIME_RETURN_OR_EXIT(runtime_flag, err_msg, 0);
    }
  }
   

  if(topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_index].server_ptr->is_agentless[0] == 'N')
  {
        access = RUN_LOCAL_ACCESS;
        strcpy(rem_ip, "NA");
        strcpy(rem_username, "NA");
        strcpy(rem_password, "NA");
  } 
  else
  {
    access = RUN_REMOTE_ACCESS;
    strcpy(rem_ip, server_name);
    strcpy(server_name, g_cavinfo.NSAdminIP);
    strcpy(rem_username, topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_index].server_ptr->username);
    strcpy(rem_password, topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_index].server_ptr->password);

  }
 
  

  // Validation
  if(num < 10) // All fields except arguments are mandatory.
  {
    sprintf(temp_usage_buf, "Error: Too few arguments for Check Monitor '%s'.\n", check_mon_name);
    usage(temp_usage_buf, runtime_flag, err_msg);
    CHK_MON_RUNTIME_RETURN_OR_EXIT(runtime_flag, runtime_flag?err_msg:temp_usage_buf, 1); 
  }

  //here we need to get pgm_args only if user has given any pgm args
  if(num > 10)
  {
    num--; // decrement num as it has first argument also (temp_buf)
    i = get_tokens(buf, token_arr, " ", 500); //get the total number of tokens
    for(j = num; j < i; j++)
    {
      if(!strcmp(token_arr[j], "$OPTION"))
      {
        sprintf(temp_buf, "%d", frequency);
        NSDL2_MON(NULL, NULL, "Replacing '$OPTION' by %s", temp_buf);
        strcat(pgm_args, temp_buf);
      }
      else if(!strcmp(token_arr[j], "$INTERVAL"))
      {
        sprintf(temp_buf, "%d", global_settings->progress_secs);
        NSDL2_MON(NULL, NULL, "Replacing '$INTERVAL' by %s", temp_buf);
        strcat(pgm_args, temp_buf);
      }
      else
        strcat(pgm_args, token_arr[j]);

      strcat(pgm_args, " ");
    }
  }

  if((from_event != CHECK_MONITOR_EVENT_BEFORE_TEST_IS_STARTED) && (from_event != CHECK_MONITOR_EVENT_START_OF_TEST) && (from_event != CHECK_MONITOR_EVENT_START_OF_PHASE) && (from_event != CHECK_MONITOR_EVENT_AFTER_TEST_IS_OVER))
  {
    sprintf(temp_usage_buf, "Error: Wrong From Event for Check Monitor '%s'.\n", check_mon_name);
    usage(temp_usage_buf, runtime_flag, err_msg);
    CHK_MON_RUNTIME_RETURN_OR_EXIT(runtime_flag, runtime_flag?err_msg:temp_usage_buf, 1);
  }

  if(frequency != RUN_EVERY_TIME_OPTION && frequency != RUN_ONLY_ONCE_OPTION)
  {
    sprintf(temp_usage_buf, "Error: Wrong Frequency for Check Monitor '%s', should be Run Periodically(1) or Never(2)\n", check_mon_name);
    usage(temp_usage_buf, runtime_flag, err_msg);
    CHK_MON_RUNTIME_RETURN_OR_EXIT(runtime_flag, runtime_flag?err_msg:temp_usage_buf, 1);
  }

  if(from_event != CHECK_MONITOR_EVENT_START_OF_PHASE)
  {
    if(strcmp(start_phase_name, "NA"))
    {
      sprintf(temp_usage_buf, "Error: Wrong Start Phase name for Check Monitor '%s'\n", check_mon_name);
      usage(temp_usage_buf, runtime_flag, err_msg);
      CHK_MON_RUNTIME_RETURN_OR_EXIT(runtime_flag, runtime_flag?err_msg:temp_usage_buf, 1);
    }
  }
  
  if(!strcmp(periodicity, "NA"))
    check_periodicity = -1; 
  else
    check_periodicity = get_time_from_format(periodicity); //since periodicity in hh:mm:ss, so change in milisec, because create server need in msec 


  //when any monitor is applied with any set in specific server ,then it makes string and checks in hash table if monitor apllied on any server of a tier
  if(json_info_ptr && json_info_ptr->any_server)
  {
 
    if(json_info_ptr->instance_name == NULL)
       sprintf(tmp_buf, "%s%c%s", json_info_ptr->mon_name, global_settings->hierarchical_view_vector_separator, 
                                                                         topo_info[topo_idx].topo_tier_info[tier_idx].tier_name);
    else
       sprintf(tmp_buf, "%s%c%s%c%s", json_info_ptr->mon_name, global_settings->hierarchical_view_vector_separator, 
               json_info_ptr->instance_name, global_settings->hierarchical_view_vector_separator, 
               topo_info[topo_idx].topo_tier_info[tier_idx].tier_name);

    NSDL1_MON(NULL, NULL, "check mon tmp_buf '%s' for making entry in hash table for server '%s'\n", tmp_buf, server_name);

    if(init_and_check_if_mon_applied(tmp_buf))
    {
      sprintf(err_msg, "Monitor '%s' can not be applied on server '%s' as it is already applied on any other sever on same tier '%s'", 
              json_info_ptr->mon_name,  
              topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_index].server_disp_name, 
              topo_info[topo_idx].topo_tier_info[tier_idx].tier_name);
              NSDL1_MON(NULL, NULL, "%s\n", err_msg);
              return -1;
    }

  }

  if(from_event == CHECK_MONITOR_EVENT_BEFORE_TEST_IS_STARTED) //If start event is 1 i.e. "Before test is started" then separately create table entry for that as name pre_test_check_info_ptr
  {
    if(!(ret = check_duplicate_monitor_name(pre_test_check_info_ptr + check_monitor_row, total_pre_test_check_entries, check_mon_name, runtime_flag, err_msg,tier_idx,server_index))) // To check duplication of check monitor name
    {
      if(create_table_entry(&check_monitor_row, &total_pre_test_check_entries, &max_pre_test_check_entries, (char **)&pre_test_check_info_ptr, sizeof(CheckMonitorInfo), "Check Monitor Table") == -1)
      {
        printf("Could not create table entry for Check Monitor Table\n");
        CHK_MON_RUNTIME_RETURN_OR_EXIT(runtime_flag, err_msg, 1);
      }
      NSDL1_MON(NULL, NULL, "check_monitor_row = %d, check_monitor_name = %s, from_event = %d, start_phase_name = %s, frequency = %d, periodicity = %d, max_count_or_phase_name = %s, server_name = %s, rem_ip = %s, rem_username = %s, rem_password = %s, pgm_path = %s, pgm_args = %s, origin_cmon = %s", check_monitor_row, check_mon_name, from_event, start_phase_name, frequency, check_periodicity, max_count_or_phase_name, server_name, rem_ip, rem_username, rem_password, pgm_path, pgm_args, origin_cmon);

      add_check_monitor(pre_test_check_info_ptr + check_monitor_row, check_mon_name, from_event, start_phase_name, pgm_path, server_name, access, rem_ip, rem_username, rem_password, pgm_args, frequency, check_periodicity, end_event, max_count_or_phase_name, CHECK_MON_IS_CHECK_MONITOR, origin_cmon, runtime_flag, err_msg, server_index, json_info_ptr,tier_idx);
    }
  }
  else
  {
    if(!(ret = check_duplicate_monitor_name(check_monitor_info_ptr + check_monitor_row, total_check_monitors_entries, check_mon_name, runtime_flag, err_msg,tier_idx,server_index))) // To check duplication of check monitor name 
    {
      if(create_table_entry(&check_monitor_row, &total_check_monitors_entries, &max_check_monitors_entries, (char **)&check_monitor_info_ptr, sizeof(CheckMonitorInfo), "Check Monitor Table") == -1)
      {
        sprintf(err_msg, "Could not create table entry for Check Monitor Table\n");
        CHK_MON_RUNTIME_RETURN_OR_EXIT(runtime_flag, err_msg, 1);
      }
      NSDL1_MON(NULL, NULL, "check_monitor_row = %d, check_monitor_name = %s, from_event = %d, start_phase_name = %s, frequency = %d, periodicity = %d, max_count_or_phase_name = %s, server_name = %s, rem_ip = %s, rem_username = %s, rem_password = %s, pgm_path = %s, pgm_args = %s, origin_cmon = %s", check_monitor_row, check_mon_name, from_event, start_phase_name, frequency, check_periodicity, max_count_or_phase_name, server_name, rem_ip, rem_username, rem_password, pgm_path, pgm_args, origin_cmon);

      add_check_monitor(check_monitor_info_ptr + check_monitor_row, check_mon_name, from_event, start_phase_name, pgm_path, server_name, access, rem_ip, rem_username, rem_password, pgm_args, frequency, check_periodicity, end_event, max_count_or_phase_name, CHECK_MON_IS_CHECK_MONITOR, origin_cmon, runtime_flag, err_msg,server_index, json_info_ptr,tier_idx);
    }
  }
return 0;
}

/* --- End: Method used during Parsing Time  ---*/


/* --- START: Method used during Run Time  ---*/

inline void close_check_monitor_connection(CheckMonitorInfo *check_monitor_ptr)
{
  NSDL2_MON(NULL, NULL, "Method called. CheckMonitor => %s", CheckMonitor_to_string(check_monitor_ptr, s_check_monitor_buf));
  if(check_monitor_ptr->fd >= 0)
  {
    close(check_monitor_ptr->fd);
    check_monitor_ptr->fd = -1;
  }
}

//This method is to close all connections for check monitor 
static inline void close_all_check_monitor_connection()
{
  int i;
  CheckMonitorInfo *check_monitor_ptr = check_monitor_info_ptr;
  NSDL2_MON(NULL, NULL, "Method called.");
  for (i = 0; i < total_check_monitors_entries; i++, check_monitor_ptr++)
  {
    if(check_monitor_ptr->fd >= 0)
    {
      REMOVE_SELECT_MSG_COM_CON(check_monitor_ptr->fd, DATA_MODE);
      close_check_monitor_connection(check_monitor_ptr); 
    }
  }
}

//This method used to get exported environment variables
char* get_ftp_user_name()
{
  NSDL2_MON(NULL, NULL, "Method called.");
  if (getenv("NS_FTP_USER") != NULL)
    return (getenv("NS_FTP_USER"));
  else
  {
    NSDL2_MON(NULL, NULL, "NS_FTP_USER env variable is not set. Setting it to default value 'netstorm'");
    return ("netstorm");
  }
}

char* get_ftp_password()
{
  NSDL2_MON(NULL, NULL, "Method called.");
  if (getenv("NS_FTP_PASSWORD") != NULL)
    return (getenv("NS_FTP_PASSWORD"));
  else
  {
    NSDL2_MON(NULL, NULL, " NS_FTP_PASSWORD env variable is not set. Setting it to default value 'netstorm'");
    return ("netstorm");
  }
}

// For preparing and sending msg to CS.
int send_msg_to_cs(CheckMonitorInfo *check_monitor_ptr)
{
  char buffer[4 * ALERT_MSG_SIZE];
  char alert_msg[ALERT_MSG_SIZE + 1];
  buffer[0] = '\0';
  alert_msg[0] = '\0';
  char ChkMon_BatchJob[50];
  int len;
  int mon_time_out;
  char SendMsg[MAX_CHECK_MONITOR_MSG_SIZE];
  char BatchBuffer[MAX_LENGTH] = "";
  int testidx = start_testidx; //storing global start_testidx in local testidx for test monitoring purpose.
   
   if(check_monitor_ptr->bj_success_criteria > 0)
   {
     strcpy(ChkMon_BatchJob, "Batch Job");
   }
   else
   {
     strcpy(ChkMon_BatchJob, "Check Monitor");
   }
  //This done because create server gives error if < 0 frequency
  if(check_monitor_ptr->periodicity < 0) check_monitor_ptr->periodicity = global_settings->progress_secs;

  NSDL2_MON(NULL, NULL, "Method called. Frequency = %d, CheckMonitor => %s", check_monitor_ptr->periodicity, CheckMonitor_to_string(check_monitor_ptr, s_check_monitor_buf));
  if(check_monitor_ptr->from_event == CHECK_MONITOR_EVENT_BEFORE_TEST_IS_STARTED)
    mon_time_out = pre_test_check_timeout;
  else if(check_monitor_ptr->from_event == CHECK_MONITOR_EVENT_AFTER_TEST_IS_OVER)
    mon_time_out = post_test_check_timeout;
  else
    mon_time_out = -1;

  // Send start session msg to ndc and set state to CHK_MON_SESS_START_SENDING
  if(is_outbound_connection_enabled && check_monitor_ptr->conn_state == CHK_MON_SESS_START_SENDING){
    sprintf(SendMsg, "nsu_server_admin_req;Msg=START_SESSION;Server=%s;\n", check_monitor_ptr->cs_ip);
    NSDL2_MON(NULL, NULL, "msg_buf = %s", SendMsg);
  }
 
  else { 
    switch(check_monitor_ptr->bj_success_criteria)
    {
      case NS_BJ_USE_EXIT_STATUS:
        sprintf(BatchBuffer, ";BJ_SUCCESS_CRITERIA=%d", check_monitor_ptr->bj_success_criteria);
        break;

      case NS_BJ_SEARCH_IN_OUTPUT:
        sprintf(BatchBuffer, ";BJ_SUCCESS_CRITERIA=%d;BJ_SEARCH_PATTERN=%s", check_monitor_ptr->bj_success_criteria, check_monitor_ptr->bj_search_pattern); 
        break;
 
      case NS_BJ_CHECK_IN_LOG_FILE:
        sprintf(BatchBuffer, ";BJ_SUCCESS_CRITERIA=%d;BJ_LOG_FILE=%s;BJ_SEARCH_PATTERN=%s", check_monitor_ptr->bj_success_criteria, check_monitor_ptr->bj_log_file, check_monitor_ptr->bj_search_pattern);
        break;

      case NS_BJ_RUN_CMD:
        sprintf(BatchBuffer, ";BJ_SUCCESS_CRITERIA=%d;BJ_CHECK_CMD=%s", check_monitor_ptr->bj_success_criteria, check_monitor_ptr->bj_check_cmd);
        break;
    } 

    if(check_monitor_ptr->origin_cmon == NULL)
    {
      sprintf(SendMsg, "init_check_monitor:MON_NAME=%s;MON_ID=%d;MON_START_EVENT=%d;MON_PGM_NAME=%s;MON_PGM_ARGS=%s;MON_OPTION=%d;"
                       "MON_ACCESS=%d;MON_REMOTE_IP=%s;MON_REMOTE_USER_NAME=%s;MON_REMOTE_PASSWD=%s;MON_FREQUENCY=%d;G_MON_ID=%s;"                                      "MON_TEST_RUN=%d;MON_NS_SERVER_NAME=%s;MON_CAVMON_SERVER_NAME=%s;MON_NS_FTP_USER=%s;"                                       "MON_NS_FTP_PASSWORD=%s;MON_TIMEOUT=%d;MON_NS_WDIR=%s;MON_CHECK_COUNT=%d%s;MON_NS_VER=%s;MON_VECTOR_SEPARATOR=%c;MON_PARTITION_IDX=%lld;"
                     "CUR_PARTITION_IDX=%lld\n",
                     check_monitor_ptr->check_monitor_name, check_monitor_ptr->mon_id, check_monitor_ptr->from_event, 
                     check_monitor_ptr->pgm_path, check_monitor_ptr->pgm_args, check_monitor_ptr->option, check_monitor_ptr->access, 
                     check_monitor_ptr->rem_ip, check_monitor_ptr->rem_username, check_monitor_ptr->rem_password,
                     check_monitor_ptr->periodicity, check_monitor_ptr->g_mon_id, testidx, g_cavinfo.NSAdminIP, check_monitor_ptr->cs_ip, 
                     get_ftp_user_name(), get_ftp_password(), mon_time_out, g_ns_wdir, check_monitor_ptr->max_count,
                     BatchBuffer, ns_version, global_settings->hierarchical_view_vector_separator, g_start_partition_idx, g_partition_idx);
    }
    else
    {
      sprintf(SendMsg, "init_check_monitor:MON_NAME=%s;MON_ID=%d;MON_START_EVENT=%d;MON_PGM_NAME=%s;MON_PGM_ARGS=%s;MON_OPTION=%d;"
                       "MON_ACCESS=%d;MON_REMOTE_IP=%s;MON_REMOTE_USER_NAME=%s;MON_REMOTE_PASSWD=%s;MON_FREQUENCY=%d;G_MON_ID=%s;"                            "MON_TEST_RUN=%d;MON_NS_SERVER_NAME=%s;MON_CAVMON_SERVER_NAME=%s;MON_NS_FTP_USER=%s;"                                      "MON_NS_FTP_PASSWORD=%s;MON_TIMEOUT=%d;MON_NS_WDIR=%s;MON_CHECK_COUNT=%d%s;MON_NS_VER=%s;ORIGIN_CMON=%s;MON_VECTOR_SEPARATOR=%c;MON_PARTITION_IDX=%lld;"
                     "CUR_PARTITION_IDX=%lld\n",
                     check_monitor_ptr->check_monitor_name, check_monitor_ptr->mon_id, check_monitor_ptr->from_event,
                     check_monitor_ptr->pgm_path, check_monitor_ptr->pgm_args, check_monitor_ptr->option, check_monitor_ptr->access, 
                     check_monitor_ptr->rem_ip, check_monitor_ptr->rem_username, check_monitor_ptr->rem_password,
                     check_monitor_ptr->periodicity,check_monitor_ptr->g_mon_id, testidx, g_cavinfo.NSAdminIP, check_monitor_ptr->cs_ip, 
                     get_ftp_user_name(), get_ftp_password(), mon_time_out, g_ns_wdir, check_monitor_ptr->max_count,
                     BatchBuffer, ns_version, check_monitor_ptr->origin_cmon, global_settings->hierarchical_view_vector_separator, g_start_partition_idx, g_partition_idx);
    }
  }
  NSDL3_MON(NULL, NULL, "Sending message to cs_ip %s on cs_port %d. CheckMonitor fd is %d. Sent msg buffer is %s", check_monitor_ptr->cs_ip, check_monitor_ptr->cs_port, check_monitor_ptr->fd, SendMsg);

  if (send(check_monitor_ptr->fd, SendMsg, strlen(SendMsg), 0) != strlen(SendMsg))
  {
    ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
				EID_CHKMON_ERROR, EVENT_CRITICAL,
				"Error in sending check monitor init message to cav mon server '%s'",
				 check_monitor_ptr->cs_ip);
    return (FAILURE);
  }

  sprintf(alert_msg, ALERT_MSG_1017011, ChkMon_BatchJob, check_monitor_ptr->check_monitor_name, check_monitor_ptr->server_name, check_monitor_ptr->tier_name);

    if((len = nslb_make_cavisson_alert_body(buffer, ALERT_MSG_BUF_SIZE, testidx, alert_msg, ALERT_INFO, check_monitor_ptr->check_monitor_name, check_monitor_ptr->tier_name, check_monitor_ptr->server_name, NULL, global_settings->hierarchical_view_topology_name, global_settings->alert_info->policy, global_settings->alert_info->protocol, g_cavinfo.NSAdminIP, parent_port_number, ALERT_PASS_VALUE, global_settings->progress_secs)) < 0)
    {
       NSTL1(NULL, NULL, "Failed to make POST body, testidx = %d, alert_type = %d, alert_policy = %s, alert_msg = %s",
                         testidx, ALERT_INFO, global_settings->alert_info->policy, alert_msg);
    }

    NSTL1(NULL, NULL, "Going to send alert msg, tomcat port = %d, testid = %d", g_tomcat_port, testidx);

    if (ns_send_alert_ex(ALERT_INFO, HTTP_METHOD_POST, NS_CONTENT_TYPE_JSON, buffer, len) < 0)
       NSTL1(NULL, NULL, "Failed to send alert msg to Dashboard");
  // Here assumed that START_SESSION is completely written, set conn state to CHK_MON_SESS_START_RECEIVING, as SESSION_STARTED is expected here
  if(is_outbound_connection_enabled)
    check_monitor_ptr->conn_state = CHK_MON_SESS_START_RECEIVING; 
  return (SUCCESS);
}

// setting up the connection for check monitor with CS (Create Server).
static int check_monitor_connection_setup(CheckMonitorInfo *check_monitor_ptr, int check_mon_idx)
{
  char err_msg[1024]="\0";
  char *server_ip;
  short server_port;
  char buffer[4 * ALERT_MSG_SIZE];
  char alert_msg[ALERT_MSG_SIZE + 1];
  buffer[0] = '\0';
  alert_msg[0] = '\0';
  char ChkMon_BatchJob[50];
  int len;

  NSDL2_MON(NULL, NULL, "Method called, CheckMonitor => %s", check_monitor_ptr->check_monitor_name);

  if(check_monitor_ptr->bj_success_criteria > 0)
  {
    strcpy(ChkMon_BatchJob, "Batch Job");
    ns_check_monitor_log(EL_DF, DM_EXECUTION, MM_MON, _FLN_, check_monitor_ptr,
                               EID_CHKMON_GENERAL, EVENT_INFO,
                               "Starting Batch Job - %s",
                               check_monitor_ptr->check_monitor_name);
    #ifdef NS_DEBUG_ON
      printf("Starting Batch Job - %s\n", check_monitor_ptr->check_monitor_name);
    #endif
  }
  else
  {
    strcpy(ChkMon_BatchJob, "Check Monitor");
    ns_check_monitor_log(EL_DF, DM_EXECUTION, MM_MON, _FLN_, check_monitor_ptr,
			       EID_CHKMON_GENERAL, EVENT_INFO,
			       "Starting Check Monitor - %s",
			       check_monitor_ptr->check_monitor_name);

    #ifdef NS_DEBUG_ON
      printf("Starting Check Monitor - %s\n", check_monitor_ptr->check_monitor_name);
    #endif
  }
 
  sprintf(alert_msg, ALERT_MSG_1017009, ChkMon_BatchJob, check_monitor_ptr->check_monitor_name, check_monitor_ptr->server_name, check_monitor_ptr->tier_name);

    if((len = nslb_make_cavisson_alert_body(buffer, ALERT_MSG_BUF_SIZE, testidx, alert_msg, ALERT_INFO, check_monitor_ptr->check_monitor_name, check_monitor_ptr->tier_name, check_monitor_ptr->server_name, NULL, global_settings->hierarchical_view_topology_name, global_settings->alert_info->policy, global_settings->alert_info->protocol, g_cavinfo.NSAdminIP, parent_port_number, ALERT_PASS_VALUE, global_settings->progress_secs)) < 0)
    {
       NSTL1(NULL, NULL, "Failed to make POST body, testidx = %d, alert_type = %d, alert_policy = %s, alert_msg = %s",
                         testidx, ALERT_INFO, global_settings->alert_info->policy, alert_msg);
    }

    NSTL1(NULL, NULL, "Going to send alert msg, tomcat port = %d, testid = %d", g_tomcat_port, testidx);

    if (ns_send_alert_ex(ALERT_INFO, HTTP_METHOD_POST, NS_CONTENT_TYPE_JSON, buffer, len) < 0)
       NSTL1(NULL, NULL, "Failed to send alert msg to Dashboard");


  NSDL2_MON(NULL, NULL, "Making connection for Check Monitor '%s'", check_monitor_ptr->check_monitor_name);

  if(is_outbound_connection_enabled)
  {
    server_ip = global_settings->net_diagnostics_server; 
    server_port = global_settings->net_diagnostics_port;
    NSDL2_MON(NULL, NULL, "OutBound Enabled, server_ip = %s, server_port = %d", server_ip, server_port);
   /*
    sprintf(msg_buf, "nd_data_req:action=chk_mon_config;server=%s;mon_id=%d;msg=", check_monitor_ptr->cs_ip, check_monitor_ptr->mon_id);
    chk_mon_make_send_msg(check_monitor_ptr, msg_buf, &msg_len);
    if(write_msg_for_mon(msg_buf, strlen(msg_buf), 1) < 0)
    {
      mark_check_monitor_fail_for_outbound(check_monitor_ptr, CHECK_MONITOR_SEND_FAIL, &num_pre_test_check, &num_post_test_check);

      NSDL1_MON(NULL, NULL, "Error in sending mon config msg for check monitor %s", check_monitor_ptr->check_monitor_name);
      return FAILURE;
    }
   */
  } else {
    server_ip = check_monitor_ptr->cs_ip; 
    server_port = check_monitor_ptr->cs_port;
    NSDL2_MON(NULL, NULL, "OutBound Disabled, server_ip = %s, server_port = %d", server_ip, server_port);
  }
  
  {
    if ((check_monitor_ptr->fd = nslb_tcp_client_ex(server_ip, server_port, conn_timeout, err_msg)) < 0)
    {
      ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
          			EID_CHKMON_ERROR, EVENT_MAJOR,
          			"%s",
          			err_msg);  
      mark_check_monitor_fail(check_monitor_ptr, 0, CHECK_MONITOR_CONN_FAIL);
      chk_mon_update_dr_table(check_monitor_ptr);
      return FAILURE;
    }
    if (fcntl(check_monitor_ptr->fd, F_SETFL, O_NONBLOCK) < 0)
    {
      ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
          			EID_CHKMON_ERROR, EVENT_CRITICAL,
          			"Error in making connection non blocking.");
      mark_check_monitor_fail(check_monitor_ptr, 0, CHECK_MONITOR_SYSERR);
      return FAILURE;
    }
    if(is_outbound_connection_enabled) {
      NSDL2_MON(NULL, NULL, "Outbound is enabled");
      check_monitor_ptr->conn_state = CHK_MON_SESS_START_SENDING; 
    }
 
    NSDL2_MON(NULL, NULL, "Connection made successfully. Sending request message");
    if (send_msg_to_cs(check_monitor_ptr) == FAILURE)
    {
      mark_check_monitor_fail(check_monitor_ptr, 0, CHECK_MONITOR_SEND_FAIL);
      return FAILURE;
    }
    NSDL3_MON(NULL, NULL, "Request message sent successfully for Check Monitor '%s'", check_monitor_ptr->check_monitor_name);
    ADD_SELECT_MSG_COM_CON((char *)check_monitor_ptr, check_monitor_ptr->fd, EPOLLIN | EPOLLERR , DATA_MODE);
  }
  
  return SUCCESS;
}

//This method is to close ftp file fd and reset state
void close_ftp_file(CheckMonitorInfo *check_monitor_ptr)
{
  NSDL2_MON(NULL, NULL, "Method called. CheckMonitor = %s, ftp_fp = %p", check_monitor_ptr->check_monitor_name, check_monitor_ptr->ftp_fp);
  if(check_monitor_ptr->ftp_fp != NULL)
  {
    fclose(check_monitor_ptr->ftp_fp);
    check_monitor_ptr->ftp_fp = NULL;
  }
  check_monitor_ptr->ftp_file_size = 0;
  check_monitor_ptr->state = CHECK_MON_COMMAND_STATE;
}



//This method is to read more ftp file message if state found, and write to file
static int ftp_file_more(CheckMonitorInfo *check_monitor_ptr)
{
  char buffer[FTP_BUFFER_SIZE + 1];
  int  bytes_to_read;
  int bytes_read = 0;
  NSDL2_MON(NULL, NULL, "Method called. CheckMonitor = %s, bytes_remaining = %d", check_monitor_ptr->check_monitor_name, check_monitor_ptr->ftp_file_size);

  while(check_monitor_ptr->ftp_file_size)
  {
    // We need to read only the ftp content. Not more than that
    if(check_monitor_ptr->ftp_file_size < FTP_BUFFER_SIZE)
      bytes_to_read = check_monitor_ptr->ftp_file_size;
    else
      bytes_to_read = FTP_BUFFER_SIZE;
 
    bytes_read = read(check_monitor_ptr->fd, buffer, bytes_to_read);

    NSDL2_MON(NULL, NULL, "CheckMonitor = %s, bytes_to_read = %d, bytes_read = %d, bytes_remaining (before read) = %d", check_monitor_ptr->check_monitor_name, bytes_to_read, bytes_read, check_monitor_ptr->ftp_file_size); 

    if(bytes_read < 0)
    {
      if (errno == EAGAIN) 
      {
        //for FTP in check monitor we need to read the file all at once.
        NSDL2_MON(NULL, NULL, "CheckMonitor = %s, Complete ftp contents is not available to read", check_monitor_ptr->check_monitor_name);
        return SUCCESS;
      }
      else if (errno == EINTR)
      {
        NSDL2_MON(NULL, NULL, "CheckMonitor = %s, FTP read interrupted. Continuing...", check_monitor_ptr->check_monitor_name);
        continue;
      }
      else
      {
        ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
				    EID_CHKMON_ERROR, EVENT_MAJOR,	
				    "Error in reading ftp contents of check monitor"
				    " message for monitor '%s'. Error = %s",
				    check_monitor_ptr->check_monitor_name,
				    nslb_strerror(errno));
        mark_check_monitor_fail(check_monitor_ptr, 1, CHECK_MONITOR_SYSERR);
        return FAILURE;
      }
    }
    if (bytes_read == 0)
    {
      ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
				  EID_CHKMON_ERROR, EVENT_MAJOR,
				  "Connection closed by CavMonAgent for Check Monitor '%s'.",
				  check_monitor_ptr->check_monitor_name);
      mark_check_monitor_fail(check_monitor_ptr, 1, CHECK_MONITOR_CONN_CLOSED);
      return FAILURE;
    }
    buffer[bytes_read] = '\0'; // Null terminate for debug log only
    NSDL4_MON(NULL, NULL, "CheckMonitor = %s, FTP contents [%d] = %s", check_monitor_ptr->check_monitor_name, bytes_read, buffer);

    if(check_monitor_ptr->monitor_type == INTERNAL_CHECK_MON)
    { 
      NSDL4_MON(NULL, NULL, "BEFORE structure buffer size [%d], local buffer size = [%d], bytes_read = [%d] , now ftp file size = [%d] ",           strlen(topo_info[topo_idx].topo_tier_info[check_monitor_ptr->tier_idx].topo_server[check_monitor_ptr->server_index].server_ptr->server_sig_output_buffer), strlen(buffer), bytes_read, check_monitor_ptr->ftp_file_size);

      strncat(topo_info[topo_idx].topo_tier_info[check_monitor_ptr->tier_idx].topo_server[check_monitor_ptr->server_index].server_ptr->server_sig_output_buffer, buffer, bytes_read);

      NSDL4_MON(NULL, NULL, "AFTER structure buffer size [%d], local buffer size = [%d], bytes_read = [%d]  , now ftp file size = [%d]", strlen(topo_info[topo_idx].topo_tier_info[check_monitor_ptr->tier_idx].topo_server[check_monitor_ptr->server_index].server_ptr->server_sig_output_buffer), strlen(buffer), bytes_read, check_monitor_ptr->ftp_file_size);
    }
    else
    {
      if(fwrite(buffer, sizeof(char), bytes_read, check_monitor_ptr->ftp_fp) != bytes_read)  
      {
        ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
			  	  EID_CHKMON_ERROR, EVENT_MAJOR,
				    "CheckMonitor = '%s', Error: Can not write to ftp file.",
  				  check_monitor_ptr->check_monitor_name);
        mark_check_monitor_fail(check_monitor_ptr, 1, CHECK_MONITOR_SYSERR);
        return FAILURE;
      }
    }
    check_monitor_ptr->ftp_file_size -= bytes_read;
      NSDL4_MON(NULL, NULL, "AFTER DECREMENTING FTP FILE SIZE : local buffer size = [%d], bytes_read = [%d] ,now ftp file size [%d] ", strlen(buffer), bytes_read, check_monitor_ptr->ftp_file_size);
  }

  NSDL2_MON(NULL, NULL, "CheckMonitor = %s, FTP done", check_monitor_ptr->check_monitor_name);

  //process nsi_get_java_instances output for coherence monitor to extract pid
  if(check_monitor_ptr->monitor_type == INTERNAL_CHECK_MON)
    process_data(check_monitor_ptr);

  close_ftp_file(check_monitor_ptr);
  return SUCCESS;
}

static inline int mkdir_p(char *path, CheckMonitorInfo *check_monitor_ptr)
{
  char *fields[30];
  char tmp_buf1[MAX_LENGTH] = "";
  char save_path[8000];
  int i, j = 0;

  NSDL2_MON(NULL, NULL, "Method called.CheckMonitor = %s, FTP file path = %s", check_monitor_ptr->check_monitor_name, path);
  strcpy(save_path, path);
  i = get_tokens(save_path, fields, "/", 30);
  while(j < i)
  {
    strcat(tmp_buf1, "/");
    strcat(tmp_buf1, fields[j]);
    //sprintf(tmp_buf1,"%s%s",tmp_buf1, fields[j]);
    if (mkdir(tmp_buf1, 0777) != 0)
    {
      if(errno != EEXIST)
      {
        ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
				    EID_CHKMON_ERROR, EVENT_MAJOR,
				    "CheckMonitor = '%s', Error in creating directory for ftp."
				    " Path: '%s'. Error = '%s'",
				    check_monitor_ptr->check_monitor_name, path, nslb_strerror(errno));
        return FAILURE;
      }
    }
    j++;
  }
  return SUCCESS;
}

// This method to extract file name and file size from command line
// and create sub dir in the Test Run dir and then create file with extracted file size and save the contents.
// TRXXX/server_logs/<server_name>/<Monitor name>/<FTPFile relative path>/<FTP filename>
// cmd_line has
//   FTPFile:<file name with path>:<size>:[file open mode]
//   File open mode is mode given in fopen (w, w+, a, a+). Default is w+
// e.g
// FTPFile:ps.log:100
// FTPFile:instance1/ps.log:100
// FTPFile:/tmp/ps.log:100   -- now support with absolute path also
// FTPFile:x:-1 (Error case)
// FTPFile: :100 (Error case)
// FTPFile:instance1/ps.log/:100 (Error case)
int extract_ftpfilename_size_createfile_with_size(char *cmd_line, CheckMonitorInfo *check_monitor_ptr)
{
  char temp_cmd_line[2014] = "";
  int num_field = 0;
  char *fields[10];
  char tmp_buf1[MAX_LENGTH] = "";
  char tmp_buf2[MAX_LENGTH] = "";
  char *ftp_file_path; // relative path of the ftp file 
  char *ftp_file_name; // ftp file name 
  char ftp_full_path[MAX_LENGTH] = "";
  char ftp_file_name_with_full_path[MAX_LENGTH] = "";
  char symlink_buf[1024 + 1] = "";
  char symlink_buf_with_file_name[1024 + 1] = "";
  int len_of_file;
  char mode[2 + 1] = "w+";
  int testidx = start_testidx;
  NSDL2_MON(NULL, NULL, "Method called. cmd_line = %s", cmd_line);
  //int tier_id=check_monitor_ptr->tier_idx;
  strcpy(temp_cmd_line, cmd_line);
  num_field = get_tokens(temp_cmd_line, fields, ":", 10);
  // ----- Start - Validation of FTPFile format, file name, file size and relative path ------
  if((num_field != 3) && (num_field != 4))
  {
    ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
				EID_CHKMON_ERROR, EVENT_MAJOR,
				"CheckMonitor = '%s', FTP Command is not in proper format."
				" It should be in 'FTPFile:<FileName with relative path>:FileSize' format."
				" Command = %s", check_monitor_ptr->check_monitor_name, cmd_line);
    return FAILURE;
  }

  if(num_field == 4)
  {
    if((!strcmp(fields[3], "w")) || (!strcmp(fields[3], "w+")) || (!strcmp(fields[3], "a")) || (!strcmp(fields[3], "a+"))) 
    {
      strcpy(mode, fields[3]);
      mode[2] = '\0';
    }
    else
    {
      ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
        EID_CHKMON_ERROR, EVENT_MAJOR,
        "CheckMonitor = '%s', File open mode %s is not correct. Hence setting file open mode to default %s.",
        check_monitor_ptr->check_monitor_name, fields[3], mode);
    }
  }

  /*if(cmd_line[strlen(fields[0]) + 1] == '/')
  {
    ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr, EVENT_MAJOR, "CheckMonitor = '%s', Error: FTP File name can have relative path only. Command = %s", check_monitor_ptr->check_monitor_name, cmd_line);
    return FAILURE;
  }*/
 
  if(cmd_line[strlen(cmd_line) - strlen(fields[2]) - 2] == '/')
  {
    ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
				EID_CHKMON_ERROR, EVENT_MAJOR,
				"CheckMonitor = '%s', Error: FTP File name not in proper format."
				" Command = %s", check_monitor_ptr->check_monitor_name, cmd_line);
    return FAILURE;
  }

  // Parse file name
  strcpy(tmp_buf1, fields[1]);
  ftp_file_path = dirname(tmp_buf1); // It will Null teminate after the path
  strcpy(tmp_buf2, fields[1]);
  ftp_file_name = basename(tmp_buf2);

  len_of_file = strlen(ftp_file_name);
  if((len_of_file == 0) || (ftp_file_name[0] == '.') || (ftp_file_name[0] == '/'))
  {
    ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
				EID_CHKMON_ERROR, EVENT_MAJOR,
				"CheckMonitor = '%s', FTP file name is not in proper format."
				" Ftp file name with path = %s", check_monitor_ptr->check_monitor_name, fields[1]);
    return FAILURE;
  }
  if (len_of_file > 255)
  {
    ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
				EID_CHKMON_ERROR, EVENT_MAJOR,
				"CheckMonitor = '%s', Error: FTP File can not be more than 255 characters",
				check_monitor_ptr->check_monitor_name);
    return FAILURE;
  }
  // Parse size of the file
  //Fixed bug: Mantis bug id- 0000047
  //check_monitor_ptr->ftp_file_size = atoi(fields[2]);
  check_monitor_ptr->ftp_file_size = atol(fields[2]);
  if(check_monitor_ptr->ftp_file_size < 0)
  {
    ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
				EID_CHKMON_ERROR, EVENT_MAJOR,
				"CheckMonitor = '%s', Invalid File size. File size (%s) can not be negative",
				check_monitor_ptr->check_monitor_name, fields[2]);
    return FAILURE;
  }
  else
  {
    if(check_monitor_ptr->monitor_type == INTERNAL_CHECK_MON)
    {
      if(topo_info[topo_idx].topo_tier_info[check_monitor_ptr->tier_idx].topo_server[check_monitor_ptr->server_index].server_ptr->server_sig_output_buffer == NULL)
      {
        MY_MALLOC_AND_MEMSET(topo_info[topo_idx].topo_tier_info[check_monitor_ptr->tier_idx].topo_server[check_monitor_ptr->server_index].server_ptr->server_sig_output_buffer, check_monitor_ptr->ftp_file_size  + 1, "Server signature java process output buffer", -1);

      }
    }
  }

  // ----- End - Validation of FTPFile format, file name, file size and relative path ------

  //for this server signature will be saving data in topo server info structure, not in file, hence returning
  if(check_monitor_ptr->monitor_type == INTERNAL_CHECK_MON) //SERVER SIGNATURE for java process
    return SUCCESS;

  //if file size is >= 0
  if(check_monitor_ptr->monitor_type == CHECK_MON_IS_CHECK_MONITOR)  //Check Monitor
  {
    // Make directory TRXXX/server_logs/<server_name>/<Monitor name>/<ftp relative path>
    sprintf(ftp_full_path, "%s/logs/TR%d/server_logs/%s/%s/%s", g_ns_wdir, testidx, check_monitor_ptr->cs_ip, check_monitor_ptr->check_monitor_name, ftp_file_path);
  }
  else if(check_monitor_ptr->monitor_type == CHECK_MON_IS_SERVER_SIGNATURE) //SERVER_SIGNATURE
  {
    // Make directory TRXXXX/server_signatures/<Signature-Name> 
    sprintf(ftp_full_path, "%s/logs/TR%d/%lld/server_signatures/%s", g_ns_wdir, testidx, g_partition_idx, ftp_file_path);
  }
  if(cmd_line[strlen(fields[0]) + 1] == '/') //check if ftp file with absolute path then create dir as it is
    sprintf(ftp_full_path, "%s", ftp_file_path); //to make dir
  NSDL2_MON(NULL, NULL, "ftp_full_path = %s, ftp_file_path = %s, ftp_file_name = %s", ftp_full_path, ftp_file_path,  ftp_file_name);

  if (mkdir_p(ftp_full_path, check_monitor_ptr) != SUCCESS)
    return FAILURE;
   
  sprintf(symlink_buf, "%s/logs/TR%d/server_signatures", g_ns_wdir, testidx);
  
  if (mkdir_p(symlink_buf, check_monitor_ptr) != SUCCESS)
    return FAILURE;
  
  // Create ftp file
  sprintf(ftp_file_name_with_full_path, "%s/%s", ftp_full_path, ftp_file_name);
  NSDL2_MON(NULL, NULL, "ftp_full_path = %s, ftp_file_name = %s", ftp_full_path, ftp_file_name);
  check_monitor_ptr->ftp_fp = fopen(ftp_file_name_with_full_path, mode);
  if (!check_monitor_ptr->ftp_fp)
  {
    ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
				EID_CHKMON_ERROR, EVENT_MAJOR,
				"Error in creating ftp file '%s' for check monitor '%s'. Error = '%s'",
				ftp_file_name_with_full_path, check_monitor_ptr->check_monitor_name, nslb_strerror(errno));
    close_ftp_file(check_monitor_ptr); // Call to reset variables
    return FAILURE;
  }
  sprintf(symlink_buf_with_file_name, "%s/%s" ,symlink_buf, ftp_file_name);
  
  if((remove(symlink_buf_with_file_name) < 0) && (errno != ENOENT)) //removing existing link
    ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
				EID_CHKMON_ERROR, EVENT_MAJOR,
			        "Could not remove symbolic link '%s'",
				symlink_buf_with_file_name);
    
  if((symlink(ftp_file_name_with_full_path, symlink_buf_with_file_name)) < 0)
    ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
                                          EID_CHKMON_ERROR, EVENT_MAJOR,
                                           "Could not create symbolic link %s to %s with errno %d\n",
                                         symlink_buf, ftp_full_path, strerror(errno));
  return SUCCESS;
}

#define REMAINING_FTP_FILE_SIZE 1024

// This method is called only once
// Return:
//  FAILURE or bytes consumed
static int start_ftp_file(CheckMonitorInfo *check_monitor_ptr, char *msg_buf,  int bytes_avail)
{
  int bytes_consumed;
  int size_to_write;

  NSDL2_MON(NULL, NULL, "Method called. FTP Start. Msg = %s, CheckMonitor = '%s', bytes_avail = %d", msg_buf, check_monitor_ptr->check_monitor_name, bytes_avail);

  if(extract_ftpfilename_size_createfile_with_size(msg_buf, check_monitor_ptr) == FAILURE)
  {
    close_ftp_file(check_monitor_ptr);
    return FAILURE;
  }
 
  bytes_consumed = strlen(msg_buf) + 1; // add 1 for NULL termination
  // Decreament bytes_avail by the size of FTPFile command line
  bytes_avail = bytes_avail - bytes_consumed;
  
  NSDL2_MON(NULL, NULL, "CheckMonitor = %s, bytes_avail = %d, bytes_consumed = %d, bytes_remaining = %d", check_monitor_ptr->check_monitor_name, bytes_avail, bytes_consumed, check_monitor_ptr->ftp_file_size);
  // If size of ftp_file is 0, then close the file
  if(check_monitor_ptr->ftp_file_size == 0)
  {
    NSDL2_MON(NULL, NULL, "CheckMonitor = %s. FTP done. FTP file size is 0.", check_monitor_ptr->check_monitor_name);
    close_ftp_file(check_monitor_ptr);
    return bytes_consumed;
  }

  msg_buf = msg_buf + bytes_consumed; 
  NSDL4_MON(NULL, NULL, "CheckMonitor = %s, FTP contents [%d] = %s", check_monitor_ptr->check_monitor_name, bytes_avail, msg_buf);

  if(check_monitor_ptr->ftp_file_size <= bytes_avail) 
    size_to_write = check_monitor_ptr->ftp_file_size;
  else
    size_to_write = bytes_avail;
  
  NSDL3_MON(NULL, NULL, "CheckMonitor = %s, Writing to FTP file. FTP size_to_write = %d", check_monitor_ptr->check_monitor_name, size_to_write);

  if(size_to_write > 0) // Do not write if there is nothing to write
  {
    if(check_monitor_ptr->monitor_type == INTERNAL_CHECK_MON)
    {
      NSDL4_MON(NULL, NULL, "BEFORE  structure buffer size [%d], local buffer size = [%d], bytes_read = [%d] ,now ftp file size [%d] ", 
      strlen(topo_info[topo_idx].topo_tier_info[check_monitor_ptr->tier_idx].topo_server[check_monitor_ptr->server_index].server_ptr->server_sig_output_buffer), 
      strlen(msg_buf), size_to_write, check_monitor_ptr->ftp_file_size);
      
      strncat(topo_info[topo_idx].topo_tier_info[check_monitor_ptr->tier_idx].topo_server[check_monitor_ptr->server_index].server_ptr->server_sig_output_buffer, msg_buf, size_to_write);
      
      NSDL4_MON(NULL, NULL, "AFTER  structure buffer size [%d], local buffer size = [%d], bytes_read = [%d] ,now ftp file size [%d] ", 
      strlen(topo_info[topo_idx].topo_tier_info[check_monitor_ptr->tier_idx].topo_server[check_monitor_ptr->server_index].server_ptr->server_sig_output_buffer), strlen(msg_buf), size_to_write, check_monitor_ptr->ftp_file_size);
 

    }
    else
    {
      if(fwrite(msg_buf, sizeof(char), size_to_write, check_monitor_ptr->ftp_fp) != size_to_write) 
      {
        ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
			  	  EID_CHKMON_ERROR, EVENT_MAJOR,
				    "Error in writing FTP file for Check Monitor '%s'. Error = %s",
				    check_monitor_ptr->check_monitor_name, nslb_strerror(errno));
        return FAILURE;
      }
    }
  }

  // Adjust vars and ftp_file_size so that it has remaining size
  bytes_avail = bytes_avail - size_to_write;
  bytes_consumed += size_to_write;
  check_monitor_ptr->ftp_file_size -= size_to_write;
      NSDL4_MON(NULL, NULL, "AFTER DECREMENT FTP FILE SIZE :  local buffer size = [%d], bytes_read = [%d] ,now ftp file size [%d] ",  strlen(msg_buf), size_to_write, check_monitor_ptr->ftp_file_size);
  NSDL3_MON(NULL, NULL, "CheckMonitor = %s, bytes_avail = %d, bytes_consumed = %d, bytes_remaining = %d", check_monitor_ptr->check_monitor_name, bytes_avail, bytes_consumed, check_monitor_ptr->ftp_file_size);
  // Complete size already recieved
  if(check_monitor_ptr->ftp_file_size == 0)
  {
    NSDL2_MON(NULL, NULL, "CheckMonitor = %s, FTP done. Complete size has been recieved. bytes_consumed = %d", check_monitor_ptr->check_monitor_name, bytes_consumed); 

    //process nsi_get_java_instances output for coherence monitor to extract pid
    if(check_monitor_ptr->monitor_type == INTERNAL_CHECK_MON)
    {
      process_data(check_monitor_ptr);
    }

    close_ftp_file(check_monitor_ptr);
    return(bytes_consumed);
  }

  // Since whole ftp contents is not read, set state to CHECK_MON_FTP_FILE_STATE
  check_monitor_ptr->state = CHECK_MON_FTP_FILE_STATE;

  // Now call ftp_file_more to see if more data is available on socket
  if(ftp_file_more(check_monitor_ptr) == FAILURE) 
  {
    close_ftp_file(check_monitor_ptr);
    return FAILURE;
  }

  // At this point there are two cases - either FTP is complete or not
  return bytes_consumed; // Bytes consumed is not required, if FTP is not done
}





//This method is to handle to read message that comes from create server.
//It will handle to read all partial messages
// read message and check status, if Event status found then call eventlog method 
// If CheckMonitorStatus:Fail status found that means check monitor failed then stop check monitor.
// If FTPFile status found then write one new method to Read file contents.
//This will retun SUCCESS if all OK else returns FAILURE

int read_output_from_cs(CheckMonitorInfo *check_monitor_ptr)
{
  char check_monitor_msg[MAX_CHECK_MON_CMD_LENGTH + 1];
  int bytes_read = 0, total_read = 0;

  NSDL2_MON(NULL, NULL, "Method called. CheckMonitor => %s", CheckMonitor_to_string(check_monitor_ptr, s_check_monitor_buf));

  check_monitor_msg[0] = '\0';
  if(check_monitor_ptr->fd == -1) // This should never happen. Just for safety
  {
    ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
				EID_CHKMON_ERROR, EVENT_MAJOR, "fd is -1. returning.");
    return SUCCESS;
  }
  //Check state of read message
  if(check_monitor_ptr->state == CHECK_MON_FTP_FILE_STATE)
  {
    if(ftp_file_more(check_monitor_ptr) == FAILURE)
    {
      close_ftp_file(check_monitor_ptr);
      return FAILURE;
    }
  }
  if(check_monitor_ptr->state == CHECK_MON_FTP_FILE_STATE) // Still in FTP
    return SUCCESS;

  if(check_monitor_ptr->state == CHECK_MON_COMMAND_STATE)
  {
    if (check_monitor_ptr->partial_buffer)
    {
      NSDL2_MON(NULL, NULL, "Earlier partial message was received for Check Monitor '%s'. Copying in local buffer. Message = %s ", check_monitor_ptr->check_monitor_name, check_monitor_ptr->partial_buffer);
      strcpy(check_monitor_msg, check_monitor_ptr->partial_buffer);
      total_read = strlen(check_monitor_msg); // Must set
      NSDL2_MON(NULL, NULL, "Freeing partial_buffer = %p", check_monitor_ptr->partial_buffer);
      FREE_AND_MAKE_NULL_EX(check_monitor_ptr->partial_buffer, total_read + 1, "check_monitor_ptr->partial_buffer", -1);//partial message length.
    }
  }
  else
  {
    ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
				EID_CHKMON_ERROR, EVENT_CRITICAL,
				"Invalid state (%d) of check monitor '%s'",
				check_monitor_ptr->state, check_monitor_ptr->check_monitor_name);
    return FAILURE;
  }

  int bytes_consumed;
  char *ptr, *cmd_line;
  // Read message till EAGAIN or error or pass/fail
  while(1)
  {
    NSDL3_MON(NULL, NULL, "Going to read for Check Monitor '%s'", check_monitor_ptr->check_monitor_name);
    if((bytes_read = read (check_monitor_ptr->fd, check_monitor_msg + total_read, MAX_CHECK_MON_CMD_LENGTH - total_read)) < 0)
    {
      if(errno == EAGAIN)
      {
        NSDL2_MON(NULL, NULL, "No more check monitor message is available to read for Check Monitor '%s'", check_monitor_ptr->check_monitor_name);
        if (total_read)
        {
          NSDL2_MON(NULL, NULL, "Allocating buffer to save the partial message. Message = %s", check_monitor_msg);
          MALLOC_AND_COPY(check_monitor_msg, check_monitor_ptr->partial_buffer, strlen(check_monitor_msg) + 1, "check_monitor_ptr->partial_buffer", -1);
        }
        return SUCCESS;
      }
      else if (errno == EINTR) 
      {   /* this means we were interrupted */
        NSDL2_MON(NULL, NULL, "Interrupted. Continuing...");
        continue;
      }
      else /* if (bytes_read < 0  && errno != EAGAIN) */
      {
        ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
				    EID_CHKMON_ERROR, EVENT_MAJOR,
				    "Error in reading check monitor message for monitor '%s'. Error = %s",
				    check_monitor_ptr->check_monitor_name, nslb_strerror(errno));
        //mark_check_monitor_fail(check_monitor_ptr, 1, CHECK_MONITOR_SYSERR);
        mark_check_monitor_fail_v2(check_monitor_ptr, 1, CHECK_MONITOR_SYSERR);
        return FAILURE;
      }
    }
    if (bytes_read == 0)
    {
      //checking since case of run periodic create server close connection for check monitor.
      // Since netstorm 
      if(check_monitor_ptr->option == RUN_EVERY_TIME_OPTION) //Run periodically
      {
        // If it is repeat till test is over or repeat is not complete
        // then it is error as create server closed connection
        if((check_monitor_ptr->max_count == -1) || (check_monitor_ptr->max_count != 0))
        {
          ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
				      EID_CHKMON_ERROR, EVENT_CRITICAL,
				      "Connection closed for %s",
				      chk_mon_event_msg(check_monitor_ptr));
          //mark_check_monitor_fail(check_monitor_ptr, 1, CHECK_MONITOR_CONN_CLOSED);
          mark_check_monitor_fail_v2(check_monitor_ptr, 1, CHECK_MONITOR_CONN_CLOSED);
          return FAILURE;
        }
        // It should never come here as we will close connection once max_count becomes 0
        ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
				    EID_CHKMON_ERROR, EVENT_CRITICAL,
				    "Connection closed for %s",
				    "which is already complete.",
                                    chk_mon_event_msg(check_monitor_ptr));
        //mark_check_monitor_fail(check_monitor_ptr, 1, CHECK_MONITOR_CONN_CLOSED);
        mark_check_monitor_fail_v2(check_monitor_ptr, 1, CHECK_MONITOR_CONN_CLOSED);
        return FAILURE;
      }
      else
      {
        ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
				    EID_CHKMON_ERROR, EVENT_CRITICAL,
				    "Connection closed for %s",
				    " Error = %s",
                                    chk_mon_event_msg(check_monitor_ptr), nslb_strerror(errno));
        //mark_check_monitor_fail(check_monitor_ptr, 1, CHECK_MONITOR_CONN_CLOSED);
        mark_check_monitor_fail_v2(check_monitor_ptr, 1, CHECK_MONITOR_CONN_CLOSED);
        return FAILURE;
      }
    }

    total_read += bytes_read;
    check_monitor_msg[total_read] = '\0';

    ptr = check_monitor_msg;
    // Now process each command line. There can be 0 or more cmd lines
    // Also cmd line may not be complete
    char *tmp_ptr;
    while ((tmp_ptr = strstr(ptr, "\n"))) //Extracting cmd line 
    {
      *tmp_ptr = 0; // NULL terminate 
      cmd_line = ptr;

      NSDL3_MON(NULL, NULL, "Received line from %s = %s", check_monitor_ptr->check_monitor_name, cmd_line);
      NSDL3_MON(NULL, NULL, "total_read = %d", total_read);

      if(!strncmp(cmd_line, CHECK_MONITOR_FTP_FILE, strlen(CHECK_MONITOR_FTP_FILE)))
      {
        bytes_consumed = start_ftp_file(check_monitor_ptr, cmd_line, total_read);
        NSDL3_MON(NULL, NULL, "After start_ftp_file() - bytes_consumed = %d total_read = %d", bytes_consumed, total_read);

        if(bytes_consumed == FAILURE) // Error
        {  
           //mark_check_monitor_fail(check_monitor_ptr, 1, CHECK_MONITOR_READ_FAIL);
           mark_check_monitor_fail_v2(check_monitor_ptr, 1, CHECK_MONITOR_READ_FAIL);
           return FAILURE;
        }
        if(check_monitor_ptr->state == CHECK_MON_FTP_FILE_STATE) // FTP is not complete 
          return SUCCESS;

        total_read -= bytes_consumed;
        if(total_read == 0) break; // No more bytes left to read in the buffer

        ptr = ptr + bytes_consumed;
        continue;
      }
      else if(!(strncmp(cmd_line, CHECK_MONITOR_EVENT, strlen(CHECK_MONITOR_EVENT))))
      {
        NSDL3_MON(NULL, NULL, "%s found for CheckMonitor '%s'", CHECK_MONITOR_EVENT, check_monitor_ptr->check_monitor_name);
        ns_check_monitor_event_command(check_monitor_ptr, cmd_line, _FLN_);
      }
      else if(!(strncmp(cmd_line, CHECK_MONITOR_PASSED_LINE, strlen(CHECK_MONITOR_PASSED_LINE))))
      {
        NSDL3_MON(NULL, NULL, "%s found for CheckMonitor '%s'", CHECK_MONITOR_PASSED_LINE, check_monitor_ptr->check_monitor_name);
        mark_check_monitor_pass_v2(check_monitor_ptr, 1, CHECK_MONITOR_PASS);
        return SUCCESS;
      }
      else if(!(strncmp(cmd_line, CHECK_MONITOR_FAILED_LINE, strlen(CHECK_MONITOR_FAILED_LINE))))
      {
        NSDL3_MON(NULL, NULL, "%s found for CheckMonitor '%s'", CHECK_MONITOR_FAILED_LINE, check_monitor_ptr->check_monitor_name);
        ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
				    EID_CHKMON_ERROR, EVENT_INFO,
				    "Check Monitor Failed", "%s found for CheckMonitor '%s'",
				    CHECK_MONITOR_FAILED_LINE, check_monitor_ptr->check_monitor_name);
        //mark_check_monitor_fail(check_monitor_ptr, 1, CHECK_MONITOR_FAIL);
        mark_check_monitor_fail_v2(check_monitor_ptr, 1, CHECK_MONITOR_FAIL);
        return FAILURE;
      } else if((is_outbound_connection_enabled && check_monitor_ptr->conn_state == CHK_MON_SESS_START_RECEIVING) && !(strncmp(cmd_line, CHECK_MONITOR_START_SESSION_LINE, strlen(CHECK_MONITOR_START_SESSION_LINE)))){
        NSDL3_MON(NULL, NULL, "%s found for CheckMonitor '%s'", CHECK_MONITOR_START_SESSION_LINE, check_monitor_ptr->check_monitor_name);
        check_monitor_ptr->conn_state = CHK_MON_SENDING; 
        if(chk_mon_send_msg_v2(check_monitor_ptr, SKIP_EPOLL) == -1)
        return -1;

      }
      else // Invalid command
        ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr, EID_CHKMON_GENERAL, EVENT_INFO, "%s", cmd_line); 

      ptr = tmp_ptr + 1;
      total_read -= strlen(cmd_line) + 1; // update total_read with bytes left
    }
   
    // At this point total_read will have any bytes not processed 
    // Check if parital line not processed is of MAX size
    if(total_read >= MAX_CHECK_MON_CMD_LENGTH)
    {
      ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr, 
				  EID_CHKMON_ERROR, EVENT_MAJOR,
				  "Command line is more than max size from check monitor '%s'.",
				  check_monitor_ptr->check_monitor_name);
      //mark_check_monitor_fail(check_monitor_ptr, 1, CHECK_MONITOR_READ_FAIL);
      mark_check_monitor_fail_v2(check_monitor_ptr, 1, CHECK_MONITOR_READ_FAIL);
      return FAILURE;
    }

    // This is left over bytes if any which are not terminated by \n
    bcopy(ptr, check_monitor_msg, total_read + 1); // Must be NULL terminated
  }
}

//This method is to handle to read message that comes from create server.
//It will handle to read all partial messages
// read message and check status, if Event status found then call eventlog method 
// If CheckMonitorStatus:Fail status found that means check monitor failed then stop check monitor.
// If FTPFile status found then write one new method to Read file contents.
//This will retun SUCCESS if all OK else returns FAILURE

int read_output_from_cs_v2(CheckMonitorInfo *check_monitor_ptr)
{
  char check_monitor_msg[MAX_CHECK_MON_CMD_LENGTH + 1];
  int bytes_read = 0, total_read = 0;

  NSDL2_MON(NULL, NULL, "Method called. CheckMonitor => %s", CheckMonitor_to_string(check_monitor_ptr, s_check_monitor_buf));

  check_monitor_msg[0] = '\0';
  if(check_monitor_ptr->fd == -1) // This should never happen. Just for safety
  {
    ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
				EID_CHKMON_ERROR, EVENT_MAJOR, "fd is -1. returning.");
    return SUCCESS;
  }
  //Check state of read message
  if(check_monitor_ptr->state == CHECK_MON_FTP_FILE_STATE)
  {
    if(ftp_file_more(check_monitor_ptr) == FAILURE)
    {
      close_ftp_file(check_monitor_ptr);
      return FAILURE;
    }
  }
  if(check_monitor_ptr->state == CHECK_MON_FTP_FILE_STATE) // Still in FTP
    return SUCCESS;

  if(check_monitor_ptr->state == CHECK_MON_COMMAND_STATE)
  {
    if (check_monitor_ptr->partial_buffer)
    {
      NSDL2_MON(NULL, NULL, "Earlier partial message was received for Check Monitor '%s'. Copying in local buffer. Message = %s ", check_monitor_ptr->check_monitor_name, check_monitor_ptr->partial_buffer);
      strcpy(check_monitor_msg, check_monitor_ptr->partial_buffer);
      total_read = strlen(check_monitor_msg); // Must set
      NSDL2_MON(NULL, NULL, "Freeing partial_buffer = %p", check_monitor_ptr->partial_buffer);
      FREE_AND_MAKE_NULL_EX(check_monitor_ptr->partial_buffer, total_read + 1, "check_monitor_ptr->partial_buffer", -1);//partial message length.
    }
  }
  else
  {
    ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
				EID_CHKMON_ERROR, EVENT_CRITICAL,
				"Invalid state (%d) of check monitor '%s'",
				check_monitor_ptr->state, check_monitor_ptr->check_monitor_name);
    return FAILURE;
  }

  int bytes_consumed;
  char *ptr, *cmd_line;
  // Read message till EAGAIN or error or pass/fail
  while(1)
  {
    NSDL3_MON(NULL, NULL, "Going to read for Check Monitor '%s'", check_monitor_ptr->check_monitor_name);
    if((bytes_read = read (check_monitor_ptr->fd, check_monitor_msg + total_read, MAX_CHECK_MON_CMD_LENGTH - total_read)) < 0)
    {
      if(errno == EAGAIN)
      {
        NSDL2_MON(NULL, NULL, "No more check monitor message is available to read for Check Monitor '%s'", check_monitor_ptr->check_monitor_name);
        if (total_read)
        {
          NSDL2_MON(NULL, NULL, "Allocating buffer to save the partial message. Message = %s", check_monitor_msg);
          MALLOC_AND_COPY(check_monitor_msg, check_monitor_ptr->partial_buffer, strlen(check_monitor_msg) + 1, "check_monitor_ptr->partial_buffer", -1);
        }
        return SUCCESS;
      }
      else if (errno == EINTR) 
      {   /* this means we were interrupted */
        NSDL2_MON(NULL, NULL, "Interrupted. Continuing...");
        continue;
      }
      else /* if (bytes_read < 0  && errno != EAGAIN) */
      {
        ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
				    EID_CHKMON_ERROR, EVENT_MAJOR,
				    "Error in reading check monitor message for monitor '%s'. Error = %s",
				    check_monitor_ptr->check_monitor_name, nslb_strerror(errno));
        //mark_check_monitor_fail(check_monitor_ptr, 1, CHECK_MONITOR_SYSERR);
        mark_check_monitor_fail_v2(check_monitor_ptr, 1, CHECK_MONITOR_SYSERR);
        return FAILURE;
      }
    }
    if (bytes_read == 0)
    {
      //checking since case of run periodic create server close connection for check monitor.
      // Since netstorm 
      if(check_monitor_ptr->option == RUN_EVERY_TIME_OPTION) //Run periodically
      {
        // If it is repeat till test is over or repeat is not complete
        // then it is error as create server closed connection
        if((check_monitor_ptr->max_count == -1) || (check_monitor_ptr->max_count != 0))
        {
          ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
				      EID_CHKMON_ERROR, EVENT_CRITICAL,
				      "Connection closed for %s",
				      chk_mon_event_msg(check_monitor_ptr));
          //mark_check_monitor_fail(check_monitor_ptr, 1, CHECK_MONITOR_CONN_CLOSED);
          mark_check_monitor_fail_v2(check_monitor_ptr, 1, CHECK_MONITOR_CONN_CLOSED);
          return FAILURE;
        }
        // It should never come here as we will close connection once max_count becomes 0
        ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
				    EID_CHKMON_ERROR, EVENT_CRITICAL,
				    "Connection closed for %s",
				    "which is already complete.",
                                    chk_mon_event_msg(check_monitor_ptr));
        //mark_check_monitor_fail(check_monitor_ptr, 1, CHECK_MONITOR_CONN_CLOSED);
        mark_check_monitor_fail_v2(check_monitor_ptr, 1, CHECK_MONITOR_CONN_CLOSED);
        return FAILURE;
      }
      else
      {
        ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
				    EID_CHKMON_ERROR, EVENT_CRITICAL,
				    "Connection closed for %s",
				    " Error = %s",
                                    chk_mon_event_msg(check_monitor_ptr), nslb_strerror(errno));
        //mark_check_monitor_fail(check_monitor_ptr, 1, CHECK_MONITOR_CONN_CLOSED);
        mark_check_monitor_fail_v2(check_monitor_ptr, 1, CHECK_MONITOR_CONN_CLOSED);
        return FAILURE;
      }
    }

    total_read += bytes_read;
    check_monitor_msg[total_read] = '\0';

    ptr = check_monitor_msg;
    // Now process each command line. There can be 0 or more cmd lines
    // Also cmd line may not be complete
    char *tmp_ptr;
    while ((tmp_ptr = strstr(ptr, "\n"))) //Extracting cmd line 
    {
      *tmp_ptr = 0; // NULL terminate 
      cmd_line = ptr;

      NSDL3_MON(NULL, NULL, "Received line from %s = %s", check_monitor_ptr->check_monitor_name, cmd_line);
      NSDL3_MON(NULL, NULL, "total_read = %d", total_read);

      if(!strncmp(cmd_line, CHECK_MONITOR_FTP_FILE, strlen(CHECK_MONITOR_FTP_FILE)))
      {
        bytes_consumed = start_ftp_file(check_monitor_ptr, cmd_line, total_read);
        NSDL3_MON(NULL, NULL, "After start_ftp_file() - bytes_consumed = %d total_read = %d", bytes_consumed, total_read);

        if(bytes_consumed == FAILURE) // Error
        {  
           //mark_check_monitor_fail(check_monitor_ptr, 1, CHECK_MONITOR_READ_FAIL);
           mark_check_monitor_fail_v2(check_monitor_ptr, 1, CHECK_MONITOR_READ_FAIL);
           return FAILURE;
        }
        if(check_monitor_ptr->state == CHECK_MON_FTP_FILE_STATE) // FTP is not complete 
          return SUCCESS;

        total_read -= bytes_consumed;
        if(total_read == 0) break; // No more bytes left to read in the buffer

        ptr = ptr + bytes_consumed;
        continue;
      }
      else if(!(strncmp(cmd_line, CHECK_MONITOR_EVENT, strlen(CHECK_MONITOR_EVENT))))
      {
        NSDL3_MON(NULL, NULL, "%s found for CheckMonitor '%s'", CHECK_MONITOR_EVENT, check_monitor_ptr->check_monitor_name);
        ns_check_monitor_event_command(check_monitor_ptr, cmd_line, _FLN_);
      }
      else if(!(strncmp(cmd_line, CHECK_MONITOR_PASSED_LINE, strlen(CHECK_MONITOR_PASSED_LINE))))
      {
        NSDL3_MON(NULL, NULL, "%s found for CheckMonitor '%s'", CHECK_MONITOR_PASSED_LINE, check_monitor_ptr->check_monitor_name);
        mark_check_monitor_pass_v2(check_monitor_ptr, 1, CHECK_MONITOR_PASS);
        return SUCCESS;
      }
      else if(!(strncmp(cmd_line, CHECK_MONITOR_FAILED_LINE, strlen(CHECK_MONITOR_FAILED_LINE))))
      {
        NSDL3_MON(NULL, NULL, "%s found for CheckMonitor '%s'", CHECK_MONITOR_FAILED_LINE, check_monitor_ptr->check_monitor_name);
        ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
				    EID_CHKMON_ERROR, EVENT_INFO,
				    "Check Monitor Failed", "%s found for CheckMonitor '%s'",
				    CHECK_MONITOR_FAILED_LINE, check_monitor_ptr->check_monitor_name);
        //mark_check_monitor_fail(check_monitor_ptr, 1, CHECK_MONITOR_FAIL);
        mark_check_monitor_fail_v2(check_monitor_ptr, 1, CHECK_MONITOR_FAIL);
        return FAILURE;
      } else if((is_outbound_connection_enabled && check_monitor_ptr->conn_state == CHK_MON_SESS_START_RECEIVING) && !(strncmp(cmd_line, CHECK_MONITOR_START_SESSION_LINE, strlen(CHECK_MONITOR_START_SESSION_LINE)))){
        NSDL3_MON(NULL, NULL, "%s found for CheckMonitor '%s'", CHECK_MONITOR_START_SESSION_LINE, check_monitor_ptr->check_monitor_name);
        check_monitor_ptr->conn_state = CHK_MON_SENDING; 
        if(chk_mon_send_msg_v2(check_monitor_ptr, SKIP_EPOLL) == -1)
        return -1;

      }
      else // Invalid command
        ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr, EID_CHKMON_GENERAL, EVENT_INFO, "%s", cmd_line); 

      ptr = tmp_ptr + 1;
      total_read -= strlen(cmd_line) + 1; // update total_read with bytes left
    }
   
    // At this point total_read will have any bytes not processed 
    // Check if parital line not processed is of MAX size
    if(total_read >= MAX_CHECK_MON_CMD_LENGTH)
    {
      ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr, 
				  EID_CHKMON_ERROR, EVENT_MAJOR,
				  "Command line is more than max size from check monitor '%s'.",
				  check_monitor_ptr->check_monitor_name);
      //mark_check_monitor_fail(check_monitor_ptr, 1, CHECK_MONITOR_READ_FAIL);
      mark_check_monitor_fail_v2(check_monitor_ptr, 1, CHECK_MONITOR_READ_FAIL);
      return FAILURE;
    }

    // This is left over bytes if any which are not terminated by \n
    bcopy(ptr, check_monitor_msg, total_read + 1); // Must be NULL terminated
  }
}

// This method is to set status of Test Run 
// This status will save in test_run.status file in logs/TRXXXX dir
/* Before Thu Jan  6 15:17:38 IST 2011: we were using open sytem call to open file
 * But we were getting a 0 fd (its a STANDARD INPUT STREAM), to avoid that issue we 
 * have a workaround by fopening it.
 */
void save_status_of_test_run(char *test_run_status)
{
  char test_run_status_file[MAX_LENGTH];
  NSDL3_MON(NULL, NULL, "Method called");
  FILE *status_fp = NULL;  //fp of test run file

  sprintf(test_run_status_file, "%s/logs/TR%d/test_run.status", g_ns_wdir, testidx);
  if (status_fp == NULL) //if fd is not open then open it
  {
    status_fp = fopen(test_run_status_file, "a"); 
    if (status_fp == NULL)
    {
      print_core_events((char*)__FUNCTION__, __FILE__, 
                        "Error in opening file '%s', Error = %s",
                         test_run_status_file, nslb_strerror(errno));
      NSTL1(NULL, NULL, "Error in opening file '%s', Error = %s", test_run_status_file, nslb_strerror(errno));
      NS_EXIT(-1, CAV_ERR_1000006, test_run_status_file, errno, nslb_strerror(errno)); //TODO: CHK THIS
    }
  }

  fprintf(status_fp, "%s\n", test_run_status);
  NSDL3_MON(NULL, NULL, "Saved testrun status '%s' in file '%s'", test_run_status, test_run_status_file);
  fclose(status_fp);
}

int handle_if_check_monitor_fd(void *ptr)
{
  CheckMonitorInfo *check_monitor_ptr = (CheckMonitorInfo *) ptr;
  //Commented as it fills debug log file.
  //NSDL3_MON(NULL, NULL, "Method called, with fd = %d", fd);

  NSDL3_MON(NULL, NULL, "check_monitor_ptr->fd = %d, CheckMonitor => %s", check_monitor_ptr->fd, CheckMonitor_to_string(check_monitor_ptr, s_check_monitor_buf));
  if (read_output_from_cs_v2((check_monitor_ptr)) == FAILURE)
  {
    ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
    			    EID_CHKMON_ERROR, EVENT_MAJOR,
    			    "Error in reading data from server for check monitor."
    			    " Server Name = %s, Check Monitor Name = %s",
    			    check_monitor_ptr->cs_ip, check_monitor_ptr->check_monitor_name);

    if(check_monitor_ptr->monitor_type == INTERNAL_CHECK_MON)
      check_monitor_ptr->status = CHECK_MONITOR_RETRY;    
    else
        chk_mon_update_dr_table(check_monitor_ptr);

    /*
    close_all_check_monitor_connection(); // This is to close other monitors
    save_status_of_test_run(TEST_RUN_STOPPED_DUE_TO_FAILURE_OF_CHECK_MONITOR);
    ns_check_monitor_log(EL_CDF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr, EVENT_MAJOR, "Test Run Canceled");
    kill_all_children();
    CHK_MON_RUNTIME_RETURN_OR_EXIT(runtime_flag, err_msg);
    */
  }
  return 1;
}

// This method is for all check monitor if event matched then start check monitor
// make connection to create server.
// Send request message to server
// add select parent for check monitor
// If from_event is not 'Start of the Phase (3)' then phase name would be "NA" 
// If phase name matched for specified from event then start that check monitor
void start_check_monitor(int from_event, char *phase_name)
{


  if (loader_opcode == CLIENT_LOADER) // Check Monitor only will handled by standalone parent or master only
    return;

  NSDL2_MON(NULL, NULL, "Method called, From Event = %d, Start Phase Name = %s, total_check_monitors_entries = %d", from_event, phase_name, total_check_monitors_entries);

  //Set default timeout, Only to avoid sending -1 for timeout if not given by user
  if(pre_test_check_timeout <= 0)
    pre_test_check_timeout = PRE_TEST_CHECK_DEFAULT_TIMEOUT;

  if(from_event == CHECK_MONITOR_EVENT_BEFORE_TEST_IS_STARTED)
  {
    if(total_pre_test_check_entries == 0)
      return;
    if(run_pre_test_check() != 0)
    {  
      if (!(global_settings->continue_on_pre_test_check_mon_failure))
      {
        NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_CRITICAL,
                                 __FILE__, (char*)__FUNCTION__,
                                 "Start check monitor/batch job failed, Test Run Canceled.");
        NS_EXIT(-1, "Start Check Monitor/Batch Job failed. Test Run Canceled."); //TODO: DISCUSS 
      }
      else
      {
        NSTL1(NULL,NULL, "\nStart Check Monitor/Batch Job failed.\nFailed Pre Test Check monitors are ignored as continue on Pre Test Check monitor error is enabled.\n");
        NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_CRITICAL,
                                 __FILE__, (char*)__FUNCTION__,
                                 "Start check monitor/batch job failed , Failed Pre Test Check monitors are ignored as continue on Pre Test Check monitor error is enabled.");
      }
    }
  }
  else
  {
    if(from_event == CHECK_MONITOR_EVENT_AFTER_TEST_IS_OVER && total_check_monitors_entries) {
      run_post_test_check();
      /*Return as ONLY CHECK_MONITOR_EVENT_AFTER_TEST_IS_OVER were left*/
      return;
    }

    int check_monitor_id;  
    CheckMonitorInfo *check_monitor_ptr = check_monitor_info_ptr;
    for (check_monitor_id = 0; check_monitor_id < total_check_monitors_entries; check_monitor_id++, check_monitor_ptr++)
    {
      if(topo_info[topo_idx].topo_tier_info[check_monitor_ptr->tier_idx].topo_server[check_monitor_ptr->server_index].server_ptr->topo_servers_list->cntrl_conn_state & CTRL_CONN_ERROR)
      {
        ns_check_monitor_log(EL_DF, DM_EXECUTION, MM_MON, _FLN_,  check_monitor_ptr,
                                 EID_CHKMON_GENERAL, EVENT_INFO,
                                 "Got Unknown server error from gethostbyname for - %s. So skipping connection making for it.",
                                 check_monitor_ptr->check_monitor_name);
        continue;
      }
      else if(check_monitor_ptr->status == CHECK_MONITOR_STOPPED)
      {
        NSDL1_MON(NULL, NULL, "check_monitor_ptr '%s' not started as its status is CHECK_MONITOR_STOPPED", check_monitor_ptr->mon_name);
        continue;
      }

      NSDL2_MON(NULL, NULL, "check_monitor_ptr[%d].from_event = %d, phase_name = %s", check_monitor_id, from_event, phase_name);
      if(check_monitor_ptr->from_event == from_event)
      {
        if(from_event == CHECK_MONITOR_EVENT_START_OF_TEST)
        {
          if(check_monitor_connection_setup(check_monitor_ptr, check_monitor_id) == FAILURE)
          {/*
             close_all_check_monitor_connection(); // This is to close other monitors
             save_status_of_test_run(TEST_RUN_STOPPED_DUE_TO_FAILURE_OF_CHECK_MONITOR);
             ns_check_monitor_log(EL_CDF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr, EVENT_MAJOR, "Test Run Canceled");
             kill_all_children();
             CHK_MON_RUNTIME_RETURN_OR_EXIT(runtime_flag, err_msg);*/
             continue;
          }
        }
        else if(from_event == CHECK_MONITOR_EVENT_START_OF_PHASE)
        {
          if(!strcmp(check_monitor_ptr->start_phase_name, phase_name))
            if(check_monitor_connection_setup(check_monitor_ptr, check_monitor_id) == FAILURE)
              continue;
            
        }
      }
    }
  }
}

//To close connection of check monitor send 'end_monitor' message to create server(cav mon server)
void end_check_monitor(CheckMonitorInfo *check_monitor_ptr)
{
  char *buffer="end_monitor\n";

  NSDL2_MON(NULL, NULL, "Method called. CheckMonitor => %s", CheckMonitor_to_string(check_monitor_ptr, s_check_monitor_buf));

  if(check_monitor_ptr->fd != -1)
  {
    if (send(check_monitor_ptr->fd, buffer, strlen(buffer), 0) != strlen(buffer))
      ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
				  EID_CHKMON_ERROR, EVENT_MAJOR,
				  "Error in sending end_monitor message for check monitor '%s'"
				  " to cav mon server '%s'",
				  check_monitor_ptr->check_monitor_name, check_monitor_ptr->cs_ip);

    close_check_monitor_connection(check_monitor_ptr);
  }
}

//To stop of check monitor for specified End event 'Till completion of phase (3)'
//and close connection of check monitor for matched end phase name from create server
void stop_check_monitor(int end_event, char *end_phase_name)
{
  int check_monitor_id;  
  CheckMonitorInfo *check_monitor_ptr = check_monitor_info_ptr;
  NSDL2_MON(NULL, NULL, "Method called, End Phase Name = %s, total_check_monitors_entries = %d", end_phase_name, total_check_monitors_entries);
 
  for (check_monitor_id = 0; check_monitor_id < total_check_monitors_entries; check_monitor_id++, check_monitor_ptr++)
  { 
    if(end_event == TILL_COMPLETION_OF_PHASE)
    {
      if(!strcmp(check_monitor_ptr->end_phase_name, end_phase_name))
        end_check_monitor(check_monitor_ptr);
    }
  }
}

//This method is to stop all check monitor at end of test run
inline void stop_all_check_monitors()
{
  int i;
  CheckMonitorInfo *check_monitor_ptr = check_monitor_info_ptr;
  NSDL2_MON(NULL, NULL, "Method called.");
  for (i = 0; i < total_check_monitors_entries; i++, check_monitor_ptr++)
  {
    if(check_monitor_ptr->fd >= 0)
    {
      end_check_monitor(check_monitor_ptr);
      check_monitor_ptr->status = CHECK_MONITOR_STOPPED;
    }
  }
}
/* --- END: Method used during Run Time  --- */

/* --- START: Make and send of message --- */
void make_check_mon_batchjob_msg(CheckMonitorInfo *check_monitor_ptr, char *msg_buf, int *msg_len, char BatchBuffer[], int mon_time_out){

  char init_str_buff[64] = "0";

  NSDL2_MON(NULL, NULL, "is_outbound_connection_enabled = %d, check_monitor_ptr->conn_state = %d",is_outbound_connection_enabled, check_monitor_ptr->conn_state);

 if(is_outbound_connection_enabled && check_monitor_ptr->conn_state == CHK_MON_SENDING){
    // Check Monitor case, with outbound case
    if(BatchBuffer[0] == '\0'){
      NSDL2_MON(NULL, NULL, "is_outbound_connection_enabled and chk_mon_ptr->conn_state = %d", check_monitor_ptr->conn_state);
      // RUN_CMD_EX:INitialDelay(ms):RepeatCount:RepeatInterval:TR:PartitionID:NA:NA:NA:commandname:cmd args
      //BUG:75138, Earlier testidx was passed in the third arguement, but now "NA" is passed instead. This change was done because server signature's request was sent to CMON before NDC could send testrun information to NDC. Due to this when CMON searched for that TR in running_test dir/, it could not find and hence server signature fails. So we are sending "NA" in place of TR. So that cmon will pass it right away. "NA" TR is sent from nsu_server_admin, hence CMON passes it.
      *msg_len = sprintf(msg_buf, "RUN_CMD_EX:0:%d:%d:NA:%lld:NA:NA:NA:%s:%s\n", check_monitor_ptr->max_count, check_monitor_ptr->periodicity, g_start_partition_idx, check_monitor_ptr->pgm_path, check_monitor_ptr->pgm_args);
      NSDL2_MON(NULL, NULL, "*msg_len = %d, msg_buf = %s", *msg_len, msg_buf);
      return;
    } else { // Batch jobs with outbound case
      NSDL2_MON(NULL, NULL, "Batch job with out bound");
      strcpy(init_str_buff, "RUN_BATCH_JOB"); 
    }
  } else {
    NSDL2_MON(NULL, NULL, "Inbound Check Mon/Batch job");
    strcpy(init_str_buff, "init_check_monitor"); 
  } 

  // Out bound disabled or batch job with outbound case
  *msg_len = sprintf(msg_buf, "%s:MON_NAME=%s;MON_ID=%d;MON_START_EVENT=%d;MON_PGM_NAME=%s;MON_PGM_ARGS=%s;"                                       "MON_OPTION=%d;MON_ACCESS=%d;MON_REMOTE_IP=%s;MON_REMOTE_USER_NAME=%s;MON_REMOTE_PASSWD=%s;"
                            "MON_FREQUENCY=%d;MON_TEST_RUN=%d;MON_NS_SERVER_NAME=%s;MON_CAVMON_SERVER_NAME=%s;"
                            "MON_NS_FTP_USER=%s;MON_NS_FTP_PASSWORD=%s;MON_TIMEOUT=%d;MON_NS_WDIR=%s;"
                            "MON_CHECK_COUNT=%d%s;MON_NS_VER=%s;MON_VECTOR_SEPARATOR=%c;MON_PARTITION_IDX=%lld\n",
                             init_str_buff, check_monitor_ptr->check_monitor_name, check_monitor_ptr->mon_id, check_monitor_ptr->from_event,
                             check_monitor_ptr->pgm_path, check_monitor_ptr->pgm_args, check_monitor_ptr->option,
                             check_monitor_ptr->access, check_monitor_ptr->rem_ip, check_monitor_ptr->rem_username, 
                             check_monitor_ptr->rem_password, check_monitor_ptr->periodicity, testidx, 
                             g_cavinfo.NSAdminIP, check_monitor_ptr->cs_ip, get_ftp_user_name(), 
                             get_ftp_password(), mon_time_out, g_ns_wdir, check_monitor_ptr->max_count, BatchBuffer, 
                             ns_version, global_settings->hierarchical_view_vector_separator, g_start_partition_idx);
 
}

inline void chk_mon_make_send_msg(CheckMonitorInfo *check_monitor_ptr, char *msg_buf, int *msg_len)
{
  int mon_time_out;
  char BatchBuffer[MAX_CHECK_MONITOR_MSG_SIZE] = "";

  //This done because create server gives error if < 0 frequency
  if(check_monitor_ptr->periodicity < 0) check_monitor_ptr->periodicity = global_settings->progress_secs;

  NSDL2_MON(NULL, NULL, "Method called. Frequency = %d, CheckMonitor => %s, Message buffer = %s", check_monitor_ptr->periodicity, check_monitor_ptr->check_monitor_name, msg_buf);
  if(check_monitor_ptr->from_event == CHECK_MONITOR_EVENT_BEFORE_TEST_IS_STARTED)
    mon_time_out = pre_test_check_timeout;
  else if(check_monitor_ptr->from_event == CHECK_MONITOR_EVENT_AFTER_TEST_IS_OVER)
    mon_time_out = post_test_check_timeout;
  else
    mon_time_out = -1;
  
  // Send start session msg to ndc and set state to CHK_MON_SESS_START_SENDING, 
  // Start session Msg format is: nsu_server_admin_req;Msg=START_SESSION;Server=<ServerIp>;\n
  if(is_outbound_connection_enabled && check_monitor_ptr->conn_state == CHK_MON_SESS_START_SENDING){
    *msg_len = sprintf(msg_buf, "nsu_server_admin_req;Msg=START_SESSION;Server=%s;\n", check_monitor_ptr->cs_ip);
    //check_monitor_ptr->conn_state = CHK_MON_SESS_START_SENDING;
    NSDL2_MON(NULL, NULL, "msg_buf = %s", msg_buf);
    return;
  }  

   
  switch(check_monitor_ptr->bj_success_criteria)
  {
    case NS_BJ_USE_EXIT_STATUS:
      sprintf(BatchBuffer, ";BJ_SUCCESS_CRITERIA=%d", check_monitor_ptr->bj_success_criteria);
      break;

    case NS_BJ_SEARCH_IN_OUTPUT:
      sprintf(BatchBuffer, ";BJ_SUCCESS_CRITERIA=%d;BJ_SEARCH_PATTERN=%s", check_monitor_ptr->bj_success_criteria, check_monitor_ptr->bj_search_pattern); 

//      sprintf(BatchBuffer, "BJ_PASS_OPTION=%d;BJ_CHECK_CHK_MOND=%s;BJ_LOG_FILE=%s;BJ_SEARCH_PATTERN=%s\n", check_monitor_ptr->batch_job_pass_option, check_monitor_ptr->cmd, check_monitor_ptr->file_name, check_monitor_ptr->search_pattern);
      break;
 
    case NS_BJ_CHECK_IN_LOG_FILE:
      sprintf(BatchBuffer, ";BJ_SUCCESS_CRITERIA=%d;BJ_LOG_FILE=%s;BJ_SEARCH_PATTERN=%s", check_monitor_ptr->bj_success_criteria, check_monitor_ptr->bj_log_file, check_monitor_ptr->bj_search_pattern);
      break;

    case NS_BJ_RUN_CMD:
      sprintf(BatchBuffer, ";BJ_SUCCESS_CRITERIA=%d;BJ_CHECK_CMD=%s", check_monitor_ptr->bj_success_criteria, check_monitor_ptr->bj_check_cmd);
      break;
 }

  make_check_mon_batchjob_msg(check_monitor_ptr, msg_buf, msg_len, BatchBuffer, mon_time_out);
 
}

static inline void chk_mon_free_partial_buf(CheckMonitorInfo *chk_mon_ptr)
{
  NSDL2_MON(NULL, NULL, "Method called, chk_mon_ptr = %p partial_buffer %p",
                         chk_mon_ptr, chk_mon_ptr->partial_buffer);

  FREE_AND_MAKE_NULL(chk_mon_ptr->partial_buffer, "chk_mon_ptr->partial_buffer", -1);
  chk_mon_ptr->send_offset = 0;
  chk_mon_ptr->bytes_remaining = 0;

}

/* Handle partial write*/
/*
init_flag:
 0 for skipping the epoll work
 1 for work on the check monitor's epoll
 2 for work on the parents epoll 
*/
//int chk_mon_send_msg(CheckMonitorInfo *chk_mon_ptr, int init_flag)
int chk_mon_send_msg(CheckMonitorInfo *chk_mon_ptr, int init_flag)
{
  char *buf_ptr;
  int bytes_sent;
  char SendMsg[MAX_MONITOR_BUFFER_SIZE];
  char buffer[4 * ALERT_MSG_SIZE];
  char alert_msg[ALERT_MSG_SIZE + 1];
  buffer[0] = '\0';
  alert_msg[0] = '\0';
  char ChkMon_BatchJob[50];
  int len;

  NSDL2_MON(NULL, NULL, "Method called, chk_mon_ptr = %p, partial_buf = %p", 
                        chk_mon_ptr,chk_mon_ptr->partial_buf);
  if(chk_mon_ptr->bj_success_criteria > 0)
      strcpy(ChkMon_BatchJob, "Batch Job");
  else
      strcpy(ChkMon_BatchJob, "Check Monitor");

  if(chk_mon_ptr->partial_buf == NULL) //First time
  {
    buf_ptr = SendMsg;
    chk_mon_make_send_msg(chk_mon_ptr,buf_ptr, &chk_mon_ptr->bytes_remaining);
  }
  else //If there is partial send
  {
    buf_ptr = chk_mon_ptr->partial_buf;
  }

  // Send MSG to CMON
  while(chk_mon_ptr->bytes_remaining)
  {
    NSDL2_MON(NULL, NULL, "Send CHK MSG: chk_mon_ptr->fd = %d, remaining_bytes = %d,  send_offse = %d, buf = [%s]", 
                               chk_mon_ptr->fd, chk_mon_ptr->bytes_remaining,
                               chk_mon_ptr->send_offset, buf_ptr+chk_mon_ptr->send_offset);

    if((bytes_sent = send(chk_mon_ptr->fd, buf_ptr + chk_mon_ptr->send_offset, chk_mon_ptr->bytes_remaining, 0)) < 0)
    {
      NSDL2_MON(NULL, NULL, "bytes_sent = %d, errno = %d, error = %s", bytes_sent, errno, nslb_strerror(errno));
      if(errno == EAGAIN) //If message send partially
      {
        if(chk_mon_ptr->partial_buf == NULL)
        {
          MY_MALLOC(chk_mon_ptr->partial_buf, (chk_mon_ptr->bytes_remaining + 1), "Malloc buffer for partial send", -1);
          strcpy(chk_mon_ptr->partial_buf, buf_ptr + chk_mon_ptr->send_offset);
          chk_mon_ptr->send_offset=0;
        }
        // In case of outbound connection, start session message will also go  
        NSDL3_MON(NULL, NULL, "After Partial Send: chk_mon_ptr->fd = %d, remaining_bytes = %d, send_offset = %d, remaning buf = [%s]", 
                               chk_mon_ptr->fd, chk_mon_ptr->bytes_remaining, chk_mon_ptr->send_offset, 
                              chk_mon_ptr->partial_buf + chk_mon_ptr->send_offset);
       
        return 0;
      }
      if(errno == EINTR) //If any intrept occurr
      {
        NSDL2_MON(NULL, NULL, "Interrupted. Continuing...");
        //chk_mon_ptr->fd = -1; //Do not set fd to -1 else no data will be sent
        continue;
      }
      else 
      {
       //sending msg failed for this monitor, close fd & send for nxt monitor
        ns_check_monitor_log(EL_F, 0, 0, _FLN_, chk_mon_ptr, EID_DATAMON_ERROR, EVENT_MAJOR,
                       "cm_init_monitor send failure with server %s", chk_mon_ptr->cs_ip);
        if(init_flag == PARENT_MON_EPOLL)
          chk_mon_handle_err_case(chk_mon_ptr, CM_REMOVE_FROM_EPOLL);
        else if(init_flag == CHECK_MON_EPOLL)
          delete_select_pre_test_check(chk_mon_ptr->fd);  
         
        chk_mon_free_partial_buf(chk_mon_ptr); 
        return -1;
      } 
    }

    NSDL2_MON(NULL, NULL, "bytes_sent = %d", bytes_sent);

    if(bytes_sent == 0)
    {
      ns_check_monitor_log(EL_F, 0, 0, _FLN_, chk_mon_ptr, EID_DATAMON_ERROR, EVENT_MAJOR,
                       "cm_init_monitor send failure with server %s", chk_mon_ptr->cs_ip);
      if(init_flag == PARENT_MON_EPOLL)
        chk_mon_handle_err_case(chk_mon_ptr, CM_REMOVE_FROM_EPOLL);
      else if(init_flag == CHECK_MON_EPOLL)
        delete_select_pre_test_check(chk_mon_ptr->fd);   
   
      chk_mon_free_partial_buf(chk_mon_ptr);
      return -1;
    }

    // START_SESSTION Msg written completely, now wait for SESSION_STARTED msg
    if((chk_mon_ptr->bytes_remaining == 0) && is_outbound_connection_enabled && (chk_mon_ptr->conn_state == CHK_MON_SESS_START_SENDING)){
      NSDL2_MON(NULL, NULL, "Outbound is enabled and start session msg is completely written, hence setting connection state to"
					" CHK_MON_SESS_START_RECEIVING", bytes_sent);
      chk_mon_ptr->conn_state = CHK_MON_SESS_START_RECEIVING;
    }
    chk_mon_ptr->bytes_remaining -= bytes_sent;
    chk_mon_ptr->send_offset += bytes_sent;
  } //End while Loop 

  //No need to check the remaining bytes as you will reach here only in case bytes remaining is 0
  //if(remaining_bytes == 0)
  //{
    NSDL2_MON(NULL, NULL, "CM INIT MSG sent successfully for %s.", chk_mon_ptr->check_monitor_name);
    ns_check_monitor_log(EL_F, 0, 0, _FLN_, chk_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFO,
		                         "Messgae 'cm_init_monitor' send succefully for %s, source address = %s",
		                          chk_mon_ptr->check_monitor_name, nslb_get_src_addr(chk_mon_ptr->fd)); 

    sprintf(alert_msg, ALERT_MSG_1017011, ChkMon_BatchJob, chk_mon_ptr->check_monitor_name, chk_mon_ptr->server_name, chk_mon_ptr->tier_name);

    if((len = nslb_make_cavisson_alert_body(buffer, ALERT_MSG_BUF_SIZE, testidx, alert_msg, ALERT_INFO, ChkMon_BatchJob, chk_mon_ptr->tier_name, chk_mon_ptr->server_name, NULL, global_settings->hierarchical_view_topology_name, global_settings->alert_info->policy, global_settings->alert_info->protocol, g_cavinfo.NSAdminIP, parent_port_number, ALERT_PASS_VALUE, global_settings->progress_secs)) < 0)
    {
       NSTL1(NULL, NULL, "Failed to make POST body, testidx = %d, alert_type = %d, alert_policy = %s, alert_msg = %s",
                          testidx, ALERT_INFO, global_settings->alert_info->policy, alert_msg);
    }

    NSTL1(NULL, NULL, "Going to send alert msg, tomcat port = %d, testid = %d", g_tomcat_port, testidx);
   
    if (ns_send_alert_ex(ALERT_INFO, HTTP_METHOD_POST, NS_CONTENT_TYPE_JSON, buffer, len) < 0)
      NSTL1(NULL, NULL, "Failed to send alert msg to Dashboard");
   
    chk_mon_free_partial_buf(chk_mon_ptr);   
    if(is_outbound_connection_enabled && chk_mon_ptr->conn_state == CHK_MON_SESS_START_SENDING){
      chk_mon_ptr->conn_state = CHK_MON_SESS_START_RECEIVING;
    } else {   
      chk_mon_ptr->conn_state = CM_RUNNING;
    }
    if(init_flag == PARENT_MON_EPOLL)
    {
      MOD_SELECT_MSG_COM_CON((char *)chk_mon_ptr, chk_mon_ptr->fd, EPOLLIN | EPOLLERR | EPOLLHUP, DATA_MODE);
    }
    else if(init_flag == CHECK_MON_EPOLL)
      mod_select_pre_test_check((char *)chk_mon_ptr, chk_mon_ptr->fd, EPOLLIN | EPOLLERR | EPOLLHUP);   

  //}

  return 0;
}

/* Handle partial write*/
/*
init_flag:
 0 for skipping the epoll work
 1 for work on the check monitor's epoll
 2 for work on the parents epoll 
*/
int chk_mon_send_msg_v2(CheckMonitorInfo *chk_mon_ptr, int init_flag)
{
  char *buf_ptr;
  int bytes_sent;
  char SendMsg[MAX_MONITOR_BUFFER_SIZE];
  char buffer[4 * ALERT_MSG_SIZE];
  char alert_msg[ALERT_MSG_SIZE + 1];
  buffer[0] = '\0';
  alert_msg[0] = '\0';
  char ChkMon_BatchJob[50];
  int len;

  if(chk_mon_ptr->bj_success_criteria > 0)
      strcpy(ChkMon_BatchJob, "Batch Job");
  else
      strcpy(ChkMon_BatchJob, "Check Monitor");

  NSDL2_MON(NULL, NULL, "Method called, chk_mon_ptr = %p, partial_buf = %p", 
                        chk_mon_ptr,chk_mon_ptr->partial_buf);
  if(chk_mon_ptr->partial_buf == NULL) //First time
  {
    buf_ptr = SendMsg;
    chk_mon_make_send_msg(chk_mon_ptr,buf_ptr, &chk_mon_ptr->bytes_remaining);
  }
  else //If there is partial send
  {
    buf_ptr = chk_mon_ptr->partial_buf;
  }

  // Send MSG to CMON
  while(chk_mon_ptr->bytes_remaining)
  {
    NSDL2_MON(NULL, NULL, "Send CHK MSG: chk_mon_ptr->fd = %d, remaining_bytes = %d,  send_offse = %d, buf = [%s]", 
                               chk_mon_ptr->fd, chk_mon_ptr->bytes_remaining,
                               chk_mon_ptr->send_offset, buf_ptr+chk_mon_ptr->send_offset);

    if((bytes_sent = send(chk_mon_ptr->fd, buf_ptr + chk_mon_ptr->send_offset, chk_mon_ptr->bytes_remaining, 0)) < 0)
    {
      NSDL2_MON(NULL, NULL, "bytes_sent = %d, errno = %d, error = %s", bytes_sent, errno, nslb_strerror(errno));
      if(errno == EAGAIN) //If message send partially
      {
        if(chk_mon_ptr->partial_buf == NULL)
        {
          MY_MALLOC(chk_mon_ptr->partial_buf, (chk_mon_ptr->bytes_remaining + 1), "Malloc buffer for partial send", -1);
          strcpy(chk_mon_ptr->partial_buf, buf_ptr + chk_mon_ptr->send_offset);
          chk_mon_ptr->send_offset=0;
        }
        // In case of outbound connection, start session message will also go  
        NSDL3_MON(NULL, NULL, "After Partial Send: chk_mon_ptr->fd = %d, remaining_bytes = %d, send_offset = %d, remaning buf = [%s]", 
                               chk_mon_ptr->fd, chk_mon_ptr->bytes_remaining, chk_mon_ptr->send_offset, 
                              chk_mon_ptr->partial_buf + chk_mon_ptr->send_offset);
       
        return 0;
      }
      if(errno == EINTR) //If any intrept occurr
      {
        NSDL2_MON(NULL, NULL, "Interrupted. Continuing...");
        //chk_mon_ptr->fd = -1; //Do not set fd to -1 else no data will be sent
        continue;
      }
      else 
      {
       //sending msg failed for this monitor, close fd & send for nxt monitor
        ns_check_monitor_log(EL_F, 0, 0, _FLN_, chk_mon_ptr, EID_DATAMON_ERROR, EVENT_MAJOR,
                       "cm_init_monitor send failure with server %s", chk_mon_ptr->cs_ip);
        if(init_flag == PARENT_MON_EPOLL)
          chk_mon_handle_err_case(chk_mon_ptr, CM_REMOVE_FROM_EPOLL);
        else if(init_flag == CHECK_MON_EPOLL)
          delete_select_pre_test_check(chk_mon_ptr->fd);  
         
        chk_mon_free_partial_buf(chk_mon_ptr); 
        return -1;
      } 
    }

    NSDL2_MON(NULL, NULL, "bytes_sent = %d", bytes_sent);

    if(bytes_sent == 0)
    {
      ns_check_monitor_log(EL_F, 0, 0, _FLN_, chk_mon_ptr, EID_DATAMON_ERROR, EVENT_MAJOR,
                       "cm_init_monitor send failure with server %s", chk_mon_ptr->cs_ip);
      if(init_flag == PARENT_MON_EPOLL)
        chk_mon_handle_err_case(chk_mon_ptr, CM_REMOVE_FROM_EPOLL);
      else if(init_flag == CHECK_MON_EPOLL)
        delete_select_pre_test_check(chk_mon_ptr->fd);   
   
      chk_mon_free_partial_buf(chk_mon_ptr);
      return -1;
    }

    // START_SESSTION Msg written completely, now wait for SESSION_STARTED msg
    if((chk_mon_ptr->bytes_remaining == 0) && is_outbound_connection_enabled && (chk_mon_ptr->conn_state == CHK_MON_SESS_START_SENDING)){
      NSDL2_MON(NULL, NULL, "Outbound is enabled and start session msg is completely written, hence setting connection state to"
					" CHK_MON_SESS_START_RECEIVING", bytes_sent);
      chk_mon_ptr->conn_state = CHK_MON_SESS_START_RECEIVING;
    }
    chk_mon_ptr->bytes_remaining -= bytes_sent;
    chk_mon_ptr->send_offset += bytes_sent;
  } //End while Loop 

  //No need to check the remaining bytes as you will reach here only in case bytes remaining is 0
  //if(remaining_bytes == 0)
  //{
    NSDL2_MON(NULL, NULL, "CM INIT MSG sent successfully for %s.", chk_mon_ptr->check_monitor_name);
    ns_check_monitor_log(EL_F, 0, 0, _FLN_, chk_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFO,
		                         "Messgae 'cm_init_monitor' send succefully for %s, source address = %s",
		                          chk_mon_ptr->check_monitor_name, nslb_get_src_addr(chk_mon_ptr->fd)); 

    chk_mon_free_partial_buf(chk_mon_ptr);   
    sprintf(alert_msg, ALERT_MSG_1017011, ChkMon_BatchJob, chk_mon_ptr->check_monitor_name, chk_mon_ptr->server_name, chk_mon_ptr->tier_name);

    if((len = nslb_make_cavisson_alert_body(buffer, ALERT_MSG_BUF_SIZE, testidx, alert_msg, ALERT_INFO, ChkMon_BatchJob, chk_mon_ptr->tier_name, chk_mon_ptr->server_name, NULL, global_settings->hierarchical_view_topology_name, global_settings->alert_info->policy, global_settings->alert_info->protocol, g_cavinfo.NSAdminIP, parent_port_number, ALERT_PASS_VALUE, global_settings->progress_secs)) < 0)
    {
       NSTL1(NULL, NULL, "Failed to make POST body, testidx = %d, alert_type = %d, alert_policy = %s, alert_msg = %s",
                          testidx, ALERT_INFO, global_settings->alert_info->policy, alert_msg);
    }   

    NSTL1(NULL, NULL, "Going to send alert msg, tomcat port = %d, testid = %d", g_tomcat_port, testidx);

    if (ns_send_alert_ex(ALERT_INFO, HTTP_METHOD_POST, NS_CONTENT_TYPE_JSON, buffer, len) < 0)
      NSTL1(NULL, NULL, "Failed to send alert msg to Dashboard");

    if(is_outbound_connection_enabled && chk_mon_ptr->conn_state == CHK_MON_SESS_START_SENDING){
      chk_mon_ptr->conn_state = CHK_MON_SESS_START_RECEIVING;
    } else {   
      chk_mon_ptr->conn_state = CM_RUNNING;
    }
    if(init_flag == PARENT_MON_EPOLL){
      MOD_SELECT_MSG_COM_CON((char *)chk_mon_ptr, chk_mon_ptr->fd, EPOLLIN | EPOLLERR | EPOLLHUP, DATA_MODE);
    }
    else if(init_flag == CHECK_MON_EPOLL)
      mod_select_pre_test_check((char *)chk_mon_ptr, chk_mon_ptr->fd, EPOLLIN | EPOLLERR | EPOLLHUP);   

  //}

  return 0;
}

/* --- END: Make and send of the message --- */
/*---------------- | Functions for Check Monitor Disaster Recovery | --------------------*/ 

/* Open a socket 
 * Connect that socket  
 */
int chk_mon_make_nb_conn(CheckMonitorInfo *chk_mon_ptr)
{
  //int err_num, err;
  int con_state;
  char err_msg[1024] = "\0";
  //socklen_t errlen;
  char*  server_ip; 
  short  server_port;
  char buffer[4 * ALERT_MSG_SIZE];
  char alert_msg[2 * ALERT_MSG_SIZE];
  buffer[0] = '\0';
  alert_msg[0] = '\0';
  char ChkMon_BatchJob[50];
  int len;

  if(chk_mon_ptr->bj_success_criteria > 0)
      strcpy(ChkMon_BatchJob, "Batch Job");
  else
      strcpy(ChkMon_BatchJob, "Check Monitor");
    
  NSDL2_MON(NULL, NULL, "Method Called, chk_mon_ptr = %p", chk_mon_ptr);
  if(is_outbound_connection_enabled){
    NSDL2_MON(NULL, NULL, "Outbound connection is enabled");
    server_ip = global_settings->net_diagnostics_server; 
    server_port = global_settings->net_diagnostics_port;
  } else {
    NSDL2_MON(NULL, NULL, "Outbound connection is disabled");
    server_ip = chk_mon_ptr->cs_ip; 
    server_port = chk_mon_ptr->cs_port;
  }

  if((chk_mon_ptr->fd = nslb_nb_open_socket((AF_INET), err_msg)) < 0)
  {
    NSDL3_MON(NULL, NULL, "Error: problem in opening socket for %s", chk_mon_ptr->check_monitor_name);
    return -1;
  }
  else
  { 
    NSDL3_MON(NULL, NULL, "Socket opened successfully so making connection for fd %d to server ip =%s, port = %d", 
                           chk_mon_ptr->fd, server_ip, server_port);

    int con_ret = nslb_nb_connect(chk_mon_ptr->fd, server_ip, server_port, &con_state, err_msg);
    NSDL3_MON(NULL, NULL, " con_ret = %d", con_ret);
    if(con_ret < 0)
    {
      NSDL3_MON(NULL, NULL, "%s", err_msg);
      ns_check_monitor_log(EL_F, 0, 0, _FLN_, chk_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFO,
                                                 "Connection failed for %s. %s", chk_mon_event_msg(chk_mon_ptr), err_msg);
      //if((strstr(err_msg, "gethostbyname")) && (strstr(err_msg, "Unknown server error")))
      //  chk_mon_ptr->conn_state = CHK_MON_GETHOSTBYNAME_ERROR; 
      sprintf(alert_msg, ALERT_MSG_1017010, ChkMon_BatchJob, chk_mon_ptr->check_monitor_name, chk_mon_ptr->server_name, chk_mon_ptr->tier_name, err_msg);

      if((len = nslb_make_cavisson_alert_body(buffer, ALERT_MSG_BUF_SIZE, testidx, alert_msg, ALERT_CRITICAL, ChkMon_BatchJob, chk_mon_ptr->tier_name, chk_mon_ptr->server_name, NULL, global_settings->hierarchical_view_topology_name, global_settings->alert_info->policy, global_settings->alert_info->protocol, g_cavinfo.NSAdminIP, parent_port_number, ALERT_FAIL_VALUE, global_settings->progress_secs)) < 0)
      {
         NSTL1(NULL, NULL, "Failed to make POST body, testidx = %d, alert_type = %d, alert_policy = %s, alert_msg = %s",
                          testidx, ALERT_CRITICAL, global_settings->alert_info->policy, alert_msg);
      }     

      NSTL1(NULL, NULL, "Going to send alert msg, tomcat port = %d, testid = %d", g_tomcat_port, testidx);
      if (ns_send_alert_ex(ALERT_CRITICAL, HTTP_METHOD_POST, NS_CONTENT_TYPE_JSON, buffer, len) > 0)
        NSTL1(NULL, NULL, "Failed to send alert msg to Dashboard. Error Msg = %s", err_msg);
      		
      return -1;
    }
    else if(con_ret == 0)
    {
      chk_mon_ptr->conn_state = CHK_MON_CONNECTED;
      ns_check_monitor_log(EL_F, 0, 0, _FLN_, chk_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFO,
                                                 "Connection established for %s.", chk_mon_event_msg(chk_mon_ptr));
    }
    else if(con_ret > 0)
    {
      if(con_state == NSLB_CON_CONNECTED){
        chk_mon_ptr->conn_state = CHK_MON_CONNECTED;
        // If outbound is enabled then first START_SESSION request is send why, so connection state is set to CHK_MON_SESS_START_SENDING
        if(is_outbound_connection_enabled){
          NSDL3_MON(NULL, NULL, "Outound is enabled, setting connection state to CHK_MON_SESS_START_SENDING");
          chk_mon_ptr->conn_state = CHK_MON_SESS_START_SENDING;
        }
      }

      if(con_state == NSLB_CON_CONNECTING)
        chk_mon_ptr->conn_state = CHK_MON_CONNECTING;
    }
    
    if(g_msg_com_epfd <= 0)
    {
      if(chk_mon_ptr->conn_state == CHK_MON_CONNECTED)
      {
	//chk_mon_make_send_msg(chk_mon_ptr, SendMsg, &send_msg_len);
	chk_mon_send_msg(chk_mon_ptr, SKIP_EPOLL);
      }

      if(chk_mon_ptr->conn_state == CHK_MON_CONNECTING || chk_mon_ptr->conn_state == CHK_MON_SENDING || chk_mon_ptr->conn_state == CHK_MON_SESS_START_SENDING)
      {
        add_select_pre_test_check((char *)chk_mon_ptr, chk_mon_ptr->fd, EPOLLOUT | EPOLLERR, CHECK_MONITOR_EVENT_BEFORE_TEST_IS_STARTED);
      }
      else
      {
        add_select_pre_test_check((char *)chk_mon_ptr, chk_mon_ptr->fd, EPOLLIN | EPOLLERR, CHECK_MONITOR_EVENT_BEFORE_TEST_IS_STARTED);
      }
    }    
    else 				//Add this socket fd in epoll and wait for an event
      ADD_SELECT_MSG_COM_CON((char *)chk_mon_ptr, chk_mon_ptr->fd, EPOLLOUT | EPOLLERR | EPOLLHUP, DATA_MODE); //If EPOLLOUT event comes then send messages
  }

  return 0;
}

inline void chk_mon_handle_err_case(CheckMonitorInfo *chk_mon_ptr, int remove_from_epoll)
{
   NSDL2_MON(NULL, NULL, "Method Called, chk_mon_ptr->fd = %d, chk_mon_ptr->conn_state = %d, remove_from_epoll = %d", 
                          chk_mon_ptr->fd, chk_mon_ptr->conn_state, remove_from_epoll);
   if(chk_mon_ptr->fd < 0)
     return;
					
   if(remove_from_epoll)
     REMOVE_SELECT_MSG_COM_CON(chk_mon_ptr->fd, DATA_MODE);

   chk_mon_ptr->conn_state = CHK_MON_INIT;
   close(chk_mon_ptr->fd);
   chk_mon_ptr->fd = -1;
}

static inline void chk_mon_retry_conn()
{
  int i;
  int tmp_failed_chk_mon_id = 0;
  //int total_aborted_chk_mon_conn = g_total_aborted_chk_mon_conn;

  NSDL2_MON(NULL, NULL, "Method Called. g_total_aborted_chk_mon_conn = %d", g_total_aborted_chk_mon_conn);

  //For aborted check monitoirs only
  for(i=0; i < g_total_aborted_chk_mon_conn; i++)
  {
    NSDL2_MON(NULL, NULL, "i = %d, chk_mon_dr_table[%d] = %p, chk_mon_retry_attempts = %d, check_monitor_info_ptr = %p,"
                          "chk_mon_dr_table[i]->check_monitor_name, s_check_monitor_buf) = %s",
                           i, i, chk_mon_dr_table[i], chk_mon_dr_table[i]->chk_mon_retry_attempts, check_monitor_info_ptr, 
                           chk_mon_dr_table[i]->check_monitor_name);

    //If particular monitor has complete their all attemps then it will not retry for next 
    if(chk_mon_dr_table[i]->chk_mon_retry_attempts == 0)
    {
      //If all retry attemps has complete and still monitor is not starting then remove it from list 
      NSDL3_MON(NULL, NULL, "All retry count has been completed for %s.", chk_mon_dr_table[i]->check_monitor_name); 

      ns_check_monitor_log(EL_F, DM_WARN1, MM_MON, _FLN_, chk_mon_dr_table[i], EID_CHKMON_GENERAL, EVENT_CRITICAL,
                                 "All retry count has been completed for (%s)",
                                 chk_mon_event_msg(chk_mon_dr_table[i]));
      continue;
    }

    if(topo_info[topo_idx].topo_tier_info[chk_mon_dr_table[i]->tier_idx].topo_server[chk_mon_dr_table[i]->server_index].server_ptr->topo_servers_list->cntrl_conn_state & CTRL_CONN_ERROR)
    {
      ns_check_monitor_log(EL_F, DM_WARN1, MM_MON, _FLN_, chk_mon_dr_table[i], EID_CHKMON_GENERAL, EVENT_CRITICAL,"Got Unknown server error from gethostbyname for (%s). So changing retry count to 0 and state to stopped, as we don't need to retry for it anymore.", chk_mon_event_msg(chk_mon_dr_table[i]));
      chk_mon_dr_table[i]->chk_mon_retry_attempts = 0;
      num_aborted_chk_mon_non_recoved++;
      chk_mon_dr_table[i]->conn_state = CHK_MON_STOPPED;
      continue;
    } 

    ns_check_monitor_log(EL_F, DM_WARN1, MM_MON, _FLN_, chk_mon_dr_table[i],  EID_CHKMON_GENERAL, EVENT_WARNING,
                          "Starting %s for retry attempt %d.",
                           chk_mon_event_msg(chk_mon_dr_table[i]),
                          ((max_chk_mon_retry_count - chk_mon_dr_table[i]->chk_mon_retry_attempts) == 0)?
                          1:(max_chk_mon_retry_count - chk_mon_dr_table[i]->chk_mon_retry_attempts) + 1);

    int ret = chk_mon_make_nb_conn(chk_mon_dr_table[i]);

    if(ret == -1)
    {
      NSDL3_MON(NULL, NULL, "Retry connection failed, so add into cm_dr table");

      ns_check_monitor_log(EL_F, DM_WARN1, MM_MON, _FLN_, chk_mon_dr_table[i],  EID_CHKMON_GENERAL, EVENT_CRITICAL,
                          "Retry connection failed for %s for retry attempt %d.",
                           chk_mon_event_msg(chk_mon_dr_table[i]),
                          ((max_chk_mon_retry_count - chk_mon_dr_table[i]->chk_mon_retry_attempts) == 0)?
                          1:(max_chk_mon_retry_count - chk_mon_dr_table[i]->chk_mon_retry_attempts) + 1);


      if(tmp_failed_chk_mon_id > CHK_MON_DR_ARRAY_SIZE)
      {
        fprintf(stderr, "Warning: Number of failed check monitors %d exceeding maximum failed chk monitors limit %d."
                        "We will not do further retry for this vector %s.\n",
                        tmp_failed_chk_mon_id, CHK_MON_DR_ARRAY_SIZE, chk_mon_dr_table[i]->check_monitor_name);
        NS_DUMP_WARNING("Number of failed check monitors %d exceeding maximum failed check monitors limit %d."
                        "We will not do further retry for this vector %s.",
                        tmp_failed_chk_mon_id, CHK_MON_DR_ARRAY_SIZE, chk_mon_dr_table[i]->check_monitor_name);
      }
      chk_mon_dr_table_tmp[tmp_failed_chk_mon_id] = chk_mon_dr_table[i]; 
      tmp_failed_chk_mon_id++;
      chk_mon_handle_err_case(chk_mon_dr_table[i], CHK_MON_NOT_REMOVE_FROM_EPOLL);
      NSDL3_MON(NULL, NULL, "chk_mon_dr_table_tmp[%d] = %d, tmp_failed_chk_mon_id = %d",
                             tmp_failed_chk_mon_id, chk_mon_dr_table_tmp[tmp_failed_chk_mon_id], tmp_failed_chk_mon_id);

      //Reduce per monitor retry count
      if(chk_mon_dr_table[i]->chk_mon_retry_attempts > 0)
      {
        chk_mon_dr_table[i]->chk_mon_retry_attempts--;

        if(chk_mon_dr_table[i]->chk_mon_retry_attempts == 0)
        {
          num_aborted_chk_mon_non_recoved++;
          chk_mon_dr_table[i]->conn_state = CHK_MON_STOPPED;
        }
      } 
      NSDL3_MON(NULL, NULL, "Left retry counts : chk_mon_dr_table[i]->chk_mon_retry_attempts = %d",
                             chk_mon_dr_table[i]->chk_mon_retry_attempts);
    }

    //total_aborted_chk_mon_conn--;
      if(chk_mon_dr_table[i]->chk_mon_retry_attempts == 0)
      {
        num_aborted_chk_mon_non_recoved++;
        chk_mon_dr_table[i]->conn_state = CHK_MON_STOPPED;
      }

  } //End for loop

  //g_total_aborted_chk_mon_conn = tmp_failed_chk_mon_id;

  //for next sample of progress report
  NSDL3_MON(NULL, NULL, "tmp_failed_chk_mon_id = %d", tmp_failed_chk_mon_id);
  if(tmp_failed_chk_mon_id)
  {
    //reset array of failed monitors
    memset(chk_mon_dr_table, 0, sizeof(chk_mon_dr_table));
    //copy monitors failed after retry from temporary array to array of failed monitors
    memcpy(chk_mon_dr_table, chk_mon_dr_table_tmp, sizeof(void *) * tmp_failed_chk_mon_id);
    //reset temporary array of failed monitors
    memset(chk_mon_dr_table_tmp, 0, sizeof(chk_mon_dr_table_tmp));
    g_total_aborted_chk_mon_conn = tmp_failed_chk_mon_id;
  }
  else
  {
    memset(chk_mon_dr_table, 0, sizeof(chk_mon_dr_table));
    g_total_aborted_chk_mon_conn = 0;
  }
  

  NSDL3_MON(NULL, NULL, "End retry: g_total_aborted_chk_mon_conn = %d", g_total_aborted_chk_mon_conn);
}

//Calling this from deliver_report.c for aborted monitors recovery
inline void handle_chk_mon_disaster_recovery()
{
  //Handling Check Monitor retry 
  NSDL2_MESSAGES(NULL, NULL, "Method called. g_total_aborted_chk_mon_conn = %d, num_aborted_chk_mon_non_recoved = %d",
                             g_total_aborted_chk_mon_conn, num_aborted_chk_mon_non_recoved);

  if(g_total_aborted_chk_mon_conn > 0 && num_aborted_chk_mon_non_recoved != g_total_aborted_chk_mon_conn)
  {
    NSDL3_MESSAGES(NULL, NULL, "There are check monitors to be restarted.");
    chk_mon_retry_conn();  
  }
  else
    NSDL3_MESSAGES(NULL, NULL, "There are no check monitors to be restarted.");
  
  NSDL3_MESSAGES(NULL, NULL, "Function end. DR done.");
} 


int pre_chk_mon_send_msg_to_cmon(void *ptr)
{
  int con_state;
  char err_msg[1024] = "\0";
  char *server_ip;
  short server_port;


  CheckMonitorInfo *chk_mon_ptr = (CheckMonitorInfo *) ptr;

  NSDL2_MON(NULL, NULL, "Method called, chk_mon_ptr = %p, chk_mon_ptr->fd = %d, chk_mon_ptr->conn_state = %d",
                        chk_mon_ptr, chk_mon_ptr->fd, chk_mon_ptr->conn_state);
  //Check State first 
  //1. If is in CM_CONNECTED then send message and change state to SENDING
  //2. If is in CM_CONNECTING then again try to connect if connect then change state to CM_CONNECTED other wise into CM_CONNICTING 
  if(chk_mon_ptr->fd < 0)
  {
    return 0;
  }
  if(is_outbound_connection_enabled){
    NSDL2_MON(NULL, NULL, "Outbound connection is enabled");
    server_ip = global_settings->net_diagnostics_server; 
    server_port = global_settings->net_diagnostics_port;
  } else {
    NSDL2_MON(NULL, NULL, "Outbound connection is disabled");
    server_ip = chk_mon_ptr->cs_ip; 
    server_port = chk_mon_ptr->cs_port;
  }

  if(chk_mon_ptr->conn_state == CM_CONNECTING)
  {
    //Again send connect request
    NSDL3_MON(NULL, NULL, "Since connection is in CM_CONNECTING so try to Reconnect for fd %d and to server ip = %s, port = %d", chk_mon_ptr->fd, server_ip, server_port);

    if( nslb_nb_connect(chk_mon_ptr->fd, server_ip, server_port, &con_state, err_msg) != 0 &&
      con_state != NSLB_CON_CONNECTED)
    {
      NSDL3_MON(NULL, NULL, "err_msg = %s", err_msg);
      mark_check_monitor_fail(chk_mon_ptr, 1, CHECK_MONITOR_CONN_FAIL); 
      ns_check_monitor_log(EL_F, DM_WARN1, MM_MON, _FLN_, chk_mon_ptr, EID_CHKMON_GENERAL, EVENT_WARNING,
                          "Connection failed for %s.",
                           chk_mon_event_msg(chk_mon_ptr));
      //if((strstr(err_msg, "gethostbyname")) && (strstr(err_msg, "Unknown server error")))
      //  chk_mon_ptr->conn_state = CHK_MON_GETHOSTBYNAME_ERROR;
      return -1;
    }

    // If outbound is enabled then first START_SESSION request is send why, so connection state is set to CHK_MON_SESS_START_SENDING
    chk_mon_ptr->conn_state = CM_CONNECTED;
    if(is_outbound_connection_enabled){
      NSDL3_MON(NULL, NULL, "Outound is enabled, setting connection state to CHK_MON_SESS_START_SENDING");
      chk_mon_ptr->conn_state = CHK_MON_SESS_START_SENDING;
    }
    ns_check_monitor_log(EL_F, 0, 0, _FLN_, chk_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFO,
                                                  "Connection established %s, source address = %s",
                                                  chk_mon_ptr->check_monitor_name, nslb_get_src_addr(chk_mon_ptr->fd));
  }

  // Make msg only first time 
  // If connection is in CM_SENDING state then we will make send msg
  //if(chk_mon_ptr->conn_state == CM_CONNECTED || chk_mon_ptr->conn_state == CM_CONNECTING)
  //if(chk_mon_ptr->conn_state == CM_CONNECTED)
  //{ 
    //NSDL3_MON(NULL, NULL, "Making custom monitor init message as state is %d", chk_mon_ptr->conn_state);

    //chk_mon_make_send_msg(chk_mon_ptr, SendMsg, &send_msg_len);

    //NSDL3_MON(NULL, NULL, "After making send message, SendMsg = [%s], send_msg_len = %d", SendMsg, send_msg_len);
  //}

  if(chk_mon_send_msg(chk_mon_ptr, CHECK_MON_EPOLL) == -1)
    return -1;

  return 0;
}

int chk_mon_send_msg_to_cmon(void *ptr)
{
  int con_state;
  char err_msg[1024] = "\0";

  CheckMonitorInfo *chk_mon_ptr = (CheckMonitorInfo *) ptr;

  NSDL2_MON(NULL, NULL, "Method called, chk_mon_ptr = %p, chk_mon_ptr->fd = %d, chk_mon_ptr->conn_state = %d",
                        chk_mon_ptr, chk_mon_ptr->fd, chk_mon_ptr->conn_state);
  //Check State first 
  //1. If is in CM_CONNECTED then send message and change state to SENDING
  //2. If is in CM_CONNECTING then again try to connect if connect then change state to CM_CONNECTED other wise into CM_CONNICTING 
  if(chk_mon_ptr->fd < 0)
  {
    return 0;
  }

  if(chk_mon_ptr->conn_state == CM_CONNECTING)
  {
    //Again send connect request
    NSDL3_MON(NULL, NULL, "Since connection is in CM_CONNECTING so try to Reconnect for fd %d and to server ip = %s, port = %d", chk_mon_ptr->fd, chk_mon_ptr->cs_ip, chk_mon_ptr->cs_port);

    if( nslb_nb_connect(chk_mon_ptr->fd, chk_mon_ptr->cs_ip, chk_mon_ptr->cs_port, &con_state, err_msg) != 0 &&
      con_state != NSLB_CON_CONNECTED)
    {
      NSDL3_MON(NULL, NULL, "err_msg = %s", err_msg);
      ns_check_monitor_log(EL_F, DM_WARN1, MM_MON, _FLN_, chk_mon_ptr, EID_CHKMON_GENERAL, EVENT_WARNING,
                          "Retry connection failed for %s for retry attempt %d.",
                           chk_mon_event_msg(chk_mon_ptr),
                          ((max_chk_mon_retry_count - chk_mon_ptr->chk_mon_retry_attempts) == 0)?
                          1:(max_chk_mon_retry_count - chk_mon_ptr->chk_mon_retry_attempts) + 1);
      
      //if((strstr(err_msg, "gethostbyname")) && (strstr(err_msg, "Unknown server error")))
      //  chk_mon_ptr->conn_state = CHK_MON_GETHOSTBYNAME_ERROR;

      chk_mon_handle_err_case(chk_mon_ptr, CM_REMOVE_FROM_EPOLL);
      if(chk_mon_ptr->monitor_type != INTERNAL_CHECK_MON)
        chk_mon_update_dr_table(chk_mon_ptr);
      return -1;
    }

    chk_mon_ptr->conn_state = CM_CONNECTED;
    ns_check_monitor_log(EL_F, 0, 0, _FLN_, chk_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFO,
                                                  "Connection established %s, source address = %s",
                                                  chk_mon_ptr->check_monitor_name, nslb_get_src_addr(chk_mon_ptr->fd));
  }
  
  if(chk_mon_ptr->conn_state == CM_CONNECTED && chk_mon_ptr->monitor_type == INTERNAL_CHECK_MON)
  {
    chk_mon_ptr->status = CHECK_MONITOR_PASS;
    chk_mon_ptr->retry_count = 0;
    chk_mon_ptr->last_retry_time_in_sec = 0;
    ns_check_monitor_log(EL_F, 0, 0, _FLN_, chk_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFO, "Recovered monitor %s successfully.", chk_mon_ptr->check_monitor_name);
  }

  // Make msg only first time 
  // If connection is in CM_SENDING state then we will make send msg
  //if(chk_mon_ptr->conn_state == CM_CONNECTED || chk_mon_ptr->conn_state == CM_CONNECTING)
  //if(chk_mon_ptr->conn_state == CM_CONNECTED)
  //{ 
    //NSDL3_MON(NULL, NULL, "Making custom monitor init message as state is %d", chk_mon_ptr->conn_state);

    //chk_mon_make_send_msg(chk_mon_ptr, SendMsg, &send_msg_len);

    //NSDL3_MON(NULL, NULL, "After making send message, SendMsg = [%s], send_msg_len = %d", SendMsg, send_msg_len);
  //}

  if(chk_mon_send_msg_v2(chk_mon_ptr, PARENT_MON_EPOLL) == -1)
    return -1;

  return 0;
}

void reset_chk_mon_values(CheckMonitorInfo *chk_mon_ptr)
{
  chk_mon_ptr->conn_state = CHK_MON_INIT;
  chk_mon_ptr->bytes_remaining = 0;
  chk_mon_ptr->send_offset = 0;
}

/* This function will add entry of exited check monitor into chk_mon_dr_table
 * And also init all members which are included for disaster recovery   
 */
inline void chk_mon_update_dr_table(CheckMonitorInfo *chk_mon_ptr)
{
  NSDL2_MON(NULL, NULL, "Method called. Updating chk_mon_dr_table: g_total_aborted_chk_mon_conn = %d," 
                        "total_check_monitors_entries = %d, chk_mon_retry_flag = %d, max_chk_mon_retry_count = %d", 
                         g_total_aborted_chk_mon_conn, total_check_monitors_entries, chk_mon_retry_flag, 
                         max_chk_mon_retry_count);

  if((chk_mon_retry_flag) && (max_chk_mon_retry_count > 0))
  {
    NSDL3_MON(NULL, NULL, "Adding check monitor '%s'", chk_mon_ptr->check_monitor_name, g_total_aborted_chk_mon_conn); 

    //Saving check monitor pointer into chk_mon_dr_table
    chk_mon_dr_table[g_total_aborted_chk_mon_conn] = chk_mon_ptr;
    g_total_aborted_chk_mon_conn++;

    NSDL3_MON(NULL, NULL, "chk mon dr members: chk_mon_retry_attempts = %d", chk_mon_ptr->chk_mon_retry_attempts); 
    //Init CHECK MONITOR DR members     
    reset_chk_mon_values(chk_mon_ptr);
    //chk_mon_ptr->bytes_remaining = 0;
    //chk_mon_ptr->send_offset = 0;

    chk_mon_ptr->chk_mon_retry_attempts--;


    if(chk_mon_ptr->partial_buf != NULL)
      FREE_AND_MAKE_NULL(chk_mon_ptr->partial_buf, "chk_mon_ptr->partial_buf", -1);

      NSDL3_MON(NULL, NULL, "chk_mon_ptr->check_monitor_name = %s, chk_mon_ptr = %p,"
                            "check_monitor_info_ptr = %p, chk_mon_ptr->chk_mon_retry_attempts = %d", chk_mon_ptr->check_monitor_name,
                             chk_mon_ptr, check_monitor_info_ptr, chk_mon_ptr->chk_mon_retry_attempts);
  }
  NSDL2_MON(NULL, NULL, "g_total_aborted_chk_mon_conn = %d", g_total_aborted_chk_mon_conn);
}

/* this is to parse keyword:
 * ENABLE_CHECK_MONITOR_DR <0/1> <retry_count>  
 */
int kw_set_enable_chk_monitor_dr(char *keyword, char *buf, int runtime_flag, char *err_msg)
{
  char key[1024];
  int retry_flag;
  int retry_count;

  NSDL2_MON(NULL, NULL, "method called, keyword = %s, buf = %s", keyword, buf);

  if(sscanf(buf, "%s %d %d", key, &retry_flag, &retry_count) != 3)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, ENABLE_CHECK_MONITOR_DR_USAGE, CAV_ERR_1011153, CAV_ERR_MSG_1)
  }

  chk_mon_retry_flag = retry_flag;
  if (global_settings->continuous_monitoring_mode) 
  {     
    max_chk_mon_retry_count = 0xfffffff;
  }     
  else    
  {     
    max_chk_mon_retry_count = retry_count; 
  }         

  NSDL3_MON(NULL, NULL, "chk_mon_retry_flag = %d, max_chk_mon_retry_count = %d", 
                          chk_mon_retry_flag, max_chk_mon_retry_count);
  return 0;
}

