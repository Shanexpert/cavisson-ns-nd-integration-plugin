/******************************************************************
 * Name    : ns_pre_test_check.c
 * Author  : Archana
 * Purpose : This file contains methods related to parsing keyword.
             and method for Starting events "Before test is 
             started" (1) for Check Monitors and server signatures
 * Note:
 * Modification History:
 * 15/10/08 - Initial Version
 * 02/04/09 - Last modification
*****************************************************************/
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include<netinet/in.h>
#include <regex.h>
#include <v1/topolib_structures.h>

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
#include "user_tables.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "util.h"
#include "tmr.h"
#include "timing.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "ns_log.h"
#include "ns_custom_monitor.h"
#include "ns_schedule_phases.h"
#include "ns_msg_com_util.h"
#include "ns_check_monitor.h"
#include "ns_pre_test_check.h"
#include "ns_child_msg_com.h"
#include "ns_alloc.h"
#include "wait_forever.h"
#include "nslb_cav_conf.h"
#include "ns_user_monitor.h"
#include "ns_gdf.h"
#include "ns_dynamic_vector_monitor.h"
#include "ns_mon_log.h"
#include "ns_server_admin_utils.h"
#include "ns_event_log.h"
#include "ns_string.h"
#include "ns_event_id.h"
#include "ns_trace_level.h"
#include "ns_parent.h"
#include "ns_ndc_outbound.h"
#include "../../base/topology/topolib_v1/topolib_structures.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"
#include "nslb_alert.h"

#ifdef TEST
/* depends */
void kill_all_children( ) { NSTL1_OUT(NULL, NULL, "kill_all_children() called\n"); exit(-1); }
Global_data global_settings;
int testidx = -1;
unsigned char my_port_index = 0; // For testing
FILE *console_fp = NULL;
int debug = 0;
char *argv0 = "netstorm.debug";
/* end depends */
#endif

int set_monitor = 0;
int total_pre_test_check_entries = 0;  // Total Check monitor enteries
int max_pre_test_check_entries = 0;    // Max Check monitor enteries
int pre_test_check_timeout = -1;        //default timeout for check monitors in seconds
CheckMonitorInfo *pre_test_check_info_ptr = NULL;

int num_pre_test_check;  //number of check monitors
static int pre_test_check_retry_count = 0;     //default retry count for check monitors
static int pre_test_check_retry_interval = 60; //default retry interval for check monitors in seconds
int epoll_fd; //this used by pre and post test check

char *failed_monitor;
//we have 8 type of errors macros defined in ns_pre_test_check.h
static char *error_msg[]={"Received failure from monitor/batch job", "Error in making connection to server", "Connection closed by other side", "Read error", "Send error", "Epoll error", "System error", "Timeout"};

/*********** FOR JAVA PROCESS SERVER SIGNATURE *********************/

#define MAX_NUM_LINES 1000 //DISCUSS
#define MAX_MSG_LENGTH 4*1024


int java_process_server_sig_enabled = 1; //flag to decide whether we need to add server sig on all servers of topology or not
int java_process_server_sig_start_indx = -1; //because now we cannot free pre test check mon structure, so using this we are cleaning this 
                                             //struct so that only java process server signature remain in pre test check mon struct
int max_retry_count = -1;
int retry_time_threshold_in_sec = -1;
int recovery_time_threshold_in_sec = -1;

/* --- START: Method used during Parsing Time  ---*/

//This method used to parse CHECK_MONITOR_TIMEOUT keyword.
int kw_set_pre_test_check_timeout(char *keyword, char *buf, char *kw_buf, char *err_msg, int runtime_flag)
{
  if(pre_test_check_timeout == -1)
  {
    NSDL2_MON(NULL, NULL, "Method called. Keyword = %s, Timeout (in second) = %s ", keyword, buf);
    pre_test_check_timeout = atoi(buf);
    if (pre_test_check_timeout <= 0) 
    { 
      NS_KW_PARSING_ERR(kw_buf, runtime_flag, err_msg, PRE_TEST_CHECK_TIMEOUT_USAGE, CAV_ERR_1011148, CAV_ERR_MSG_9);
    }
    NSDL3_MON(NULL, NULL, "pre_test_check_timeout = %d sec", pre_test_check_timeout);
  }
  return 0;
}

/*This method used to parse SERVER_SIGNATURE keyword. 
Since using CheckMonitorInfo structure so filling some fields default
This is similar as Start event 'Before test is started (1)' for Check Monitor
SERVER_SIGNATURE  <Server-Name> <Signature-Name>  <Signature-Type>  <Command or File>
Example:
(1)For File:--
  (i)Whitout double quote and without space--
     SERVER_SIGNATURE 192.168.1.227 ServerSigantureFile File C:\ServerSignature\file.txt
  (ii)With double quote but without space--
     SERVER_SIGNATURE 192.168.1.227 ServerSigantureFile File "C:\ServerSignature\file.txt"
  (iii)With double quote and with space--
     SERVER_SIGNATURE 192.168.1.227 ServerSigantureFile File "C:\Server Signature\manish.txt"
(2)For Command:--
  (i)Without double quote and without space--
     SERVER_SIGNATURE 192.168.1.227 ServerSigantureFile Command type C:\ServerSignature\file.txt
  (ii)With double quote but without space--
     SERVER_SIGNATURE 192.168.1.227 ServerSigantureFile Command type "C:\ServerSignature\file.txt"
  (iii)With double quote and with space--
     SERVER_SIGNATURE 192.168.1.227 ServerSigantureFile Command type "C:\Server Signature\manish.txt" 
     SERVER_SIGNATURE 192.168.1.227 ServerSigantureFile Command call "C:\Server Signature\cmdFile.bat" -a 
*/

int kw_set_server_signature(char *keyword, char *buf, int runtime_flag, char *err_msg, JSON_info *json_info_ptr)
{
  int num = 0;
  char key[MAX_LENGTH] = "";
  char server_signature_name[MAX_LENGTH] = "";
  char origin_cmon_and_ip_port[MAX_LENGTH] = "NS"; //default is NS
  char server_name[MAX_LENGTH] = {0};
  char origin_cmon[MAX_LENGTH] = {0}; //For Heruku
  char pgm_path[MAX_LENGTH] = "get_server_signature";  // Program name 
  char pgm_args[MAX_BUF_LENGTH] = "";      // Program arguments
  char command_or_file[MAX_LENGTH] = "";   // Command or file name 
  int server_signature_row = 0;  // this will contains the newly created row-index of table
  char signature_type[MAX_LENGTH] = ""; //Signature type - "Command or File"
  int access = RUN_LOCAL_ACCESS;
  char rem_ip[1024] = "NA";
  char rem_username[256] = "NA";
  char rem_password[256] = "NA";
  int server_index ;
  int tier_idx;
  char tmp_buf[MAX_DATA_LINE_LENGTH];

  
  NSDL2_MON(NULL, NULL, "Method called, keyword = %s, buf = %s", keyword, buf);

  num = sscanf(buf, "%s %s %s %s %[^\n]", key, origin_cmon_and_ip_port, server_signature_name, signature_type, command_or_file);

  if(json_info_ptr)
  {
    tier_idx=json_info_ptr->tier_idx;
    server_index=json_info_ptr->server_index;
  }
  else
  {
    if(find_tier_idx_and_server_idx(origin_cmon_and_ip_port,server_name, origin_cmon, global_settings->hierarchical_view_vector_separator, hpd_port, &server_index, &tier_idx) == -1)
    {
      sprintf(err_msg, CAV_ERR_1060059, origin_cmon_and_ip_port, pgm_path);
      CM_RUNTIME_RETURN_OR_EXIT(runtime_flag, err_msg, 0,-1,-1)
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
    strcpy(rem_ip, origin_cmon_and_ip_port);
    strcpy(server_name, g_cavinfo.NSAdminIP);
    strcpy(rem_username,topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_index].server_ptr->username);
    strcpy(rem_password,topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_index].server_ptr->password);
  }
  
    // Validation
  if(num < 5) // All fields except arguments are mandatory.
  {
    sprintf(err_msg, CAV_ERR_1060079,server_signature_name);
    CM_RUNTIME_RETURN_OR_EXIT(runtime_flag, err_msg, 1,tier_idx,server_index);
  }

  if(json_info_ptr && json_info_ptr->any_server)
  {
    if(json_info_ptr->instance_name == NULL)
       sprintf(tmp_buf, "%s%c%s", json_info_ptr->mon_name, global_settings->hierarchical_view_vector_separator, json_info_ptr->tier_name);
    else
       sprintf(tmp_buf, "%s%c%s%c%s", json_info_ptr->mon_name, global_settings->hierarchical_view_vector_separator, json_info_ptr->instance_name, global_settings->hierarchical_view_vector_separator,json_info_ptr->tier_name);
    if(init_and_check_if_mon_applied(tmp_buf))
    {
      sprintf(err_msg, "Monitor '%s' can not be applied on server %s as it is already applied on any other sever on same tier '%s'", json_info_ptr->mon_name, topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_index].server_ptr->server_name,topo_info[topo_idx].topo_tier_info[tier_idx].tier_name);
      return -1;
    }

  }

  //Here we set arguments depend on file or command
  if(!strcmp(signature_type, "Command"))
    sprintf(pgm_args, "%s -c %s ", server_signature_name, command_or_file);
  else if(!strcmp(signature_type, "File"))
    sprintf(pgm_args, "%s -f %s", server_signature_name, command_or_file);
  else
  {
    sprintf(err_msg, CAV_ERR_1060077, server_signature_name);
    CM_RUNTIME_RETURN_OR_EXIT(runtime_flag, err_msg, 1,tier_idx,server_index);
  }
  //Create table entry for server signatures 
  if(create_table_entry(&server_signature_row, &total_pre_test_check_entries, &max_pre_test_check_entries, (char **)&pre_test_check_info_ptr, sizeof(CheckMonitorInfo), "Server Signature Table") == -1)
  {
    sprintf(err_msg, CAV_ERR_1060078);
    CM_RUNTIME_RETURN_OR_EXIT(runtime_flag, err_msg, 1,tier_idx,server_index);
  }
  NSDL1_MON(NULL, NULL, "server_signature_row = %d, server_name = %s, rem_ip = %s, rem_username = %s, rem_password = %s, server_signature_name = %s, signature_type = %s, command_or_file = %s, origin_cmon = %s", server_signature_row, server_name, rem_ip, rem_username, rem_password, server_signature_name, signature_type, command_or_file, origin_cmon);

  //Set all default value since using same structure and methods as Start Event - "Before Test Run Starts" of check monitor.
  if(add_check_monitor(pre_test_check_info_ptr + server_signature_row, server_signature_name, CHECK_MONITOR_EVENT_BEFORE_TEST_IS_STARTED, "NA", pgm_path, server_name, access, rem_ip, rem_username, rem_password, pgm_args, RUN_ONLY_ONCE_OPTION, -1, "NA", "NA", CHECK_MON_IS_SERVER_SIGNATURE, origin_cmon, runtime_flag, err_msg, server_index, json_info_ptr,tier_idx) < 0)
    return -1;
  else
    return server_signature_row;
}

/*************** JAVA PROCESS SERVER SIGNATURE SETUP CODE STARTS ************************************/

// To decide whether we have to do retry for particular server signature or not  
int recover_server_signature_or_not(CheckMonitorInfo *chk_mon_ptr)
{
  int ret_val = 1;
  long cur_time_in_sec;
  int threshold;

  //component will not be recovered in this case 
  if(chk_mon_ptr == NULL || chk_mon_ptr->max_retry_count <= 0 || chk_mon_ptr->recovery_time_threshold_in_sec <= 0 || chk_mon_ptr->retry_time_threshold_in_sec <= 0)
    return -1;

  if(chk_mon_ptr->retry_count > chk_mon_ptr->max_retry_count)
    threshold = chk_mon_ptr->recovery_time_threshold_in_sec;
  else
    threshold = chk_mon_ptr->retry_time_threshold_in_sec;
    
  cur_time_in_sec = time(NULL); 

  if ((cur_time_in_sec - chk_mon_ptr->last_retry_time_in_sec) >= threshold)
  {
    ret_val = 0;
    chk_mon_ptr->last_retry_time_in_sec = cur_time_in_sec;
    chk_mon_ptr->retry_count++;
  }

  return(ret_val);
}

//This is to process data present in topo server info structure
void process_data(CheckMonitorInfo *pre_test_check_ptr)
{
  int inst_indx = 0,line = 0, max_lines = 0,  search_count = 0, num_times_all_pattern_matched = 0;
  char *process_line[MAX_NUM_LINES] = {0};
  char *pipe_ptr = NULL;
  char pid_buf[10] = {0};       
  
  //if((pre_test_check_ptr->status != CHECK_MONITOR_DATA_PROCESSED) && (pre_test_check_ptr->monitor_type == INTERNAL_CHECK_MON) && (pre_test_check_ptr->ftp_file_size == 0)) //saved complete file
  if((pre_test_check_ptr->monitor_type == INTERNAL_CHECK_MON) && (pre_test_check_ptr->ftp_file_size == 0)) //saved complete file
  { 
    int tier_idx=pre_test_check_ptr->tier_idx;
    if(tier_idx != -1){
    int topo_server_indx = pre_test_check_ptr->server_index;
     if(topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[topo_server_indx].server_ptr->server_sig_output_buffer == NULL)
      return;
    max_lines = get_tokens_with_multi_delimiter(topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[topo_server_indx].server_ptr->server_sig_output_buffer, process_line, "\n", MAX_NUM_LINES);

    MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "max_lines = %d, check_monitor_name = %s, topo_server_indx = %d", max_lines, pre_test_check_ptr->check_monitor_name, topo_server_indx);

    if(topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[topo_server_indx].server_ptr->tot_inst == 0)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "No instances present for sever %s", topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[topo_server_indx].server_disp_name);
      //return;
    }

    //process saved data  
    //for each instance having this server id, process saved data & get pid & save pid in topo instance structure
    for (inst_indx = 0; inst_indx < topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[topo_server_indx].server_ptr->tot_inst; inst_indx++)
    {
      //topo_instance_index = topo_server_info[topo_server_indx].inst_indx[inst_indx];  
      TopoInstance *tmp_instance_ptr = topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[topo_server_indx].server_ptr->instance_head;
      search_count = 0; //reset
      num_times_all_pattern_matched = 0; //reset

      for(line = 1; line < max_lines; line++)
      {   
        search_count = 0; //reset
        //for(search_pattrn_indx = 0; search_pattrn_indx < topo_instance_info[topo_instance_index].num_search_pattern; search_pattrn_indx++)                
        while(tmp_instance_ptr != NULL)
        { 
          
          if(strncmp(*tmp_instance_ptr->search_pattern, "NA",2) == 0) 
          {
            MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "earch pattern not given. Ignoring %s", tmp_instance_ptr->search_pattern);
            tmp_instance_ptr=tmp_instance_ptr->next_instance;   
            continue;
          }

          if(strstr(process_line[line], *tmp_instance_ptr->search_pattern) == NULL) //not found
          {
            MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Search pattern %s not found in line %s.", tmp_instance_ptr->search_pattern, process_line[line]);
            tmp_instance_ptr=tmp_instance_ptr->next_instance;
            break;
          }
          else //found
          {
            MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,"Search pattern %s found in line %s. Incrementing search_count %d by one.", tmp_instance_ptr->search_pattern, process_line[line], search_count);
            search_count++;
          }
        }     
          
        if(search_count == tmp_instance_ptr->num_search_pattern)
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Found all the search pattern in line [%s]. search_count %d, tmp_instance_head->num_search_pattern %d num_times_all_pattern_matched %d", process_line[line], search_count, tmp_instance_ptr->num_search_pattern, num_times_all_pattern_matched);
          //now extract pid from matched line
          pipe_ptr = strchr(process_line[line], '|');
          if(pipe_ptr != NULL)  
          {
            strncpy(pid_buf, process_line[line], pipe_ptr - process_line[line]);
            pid_buf[pipe_ptr - process_line[line]] = '\0';
            //topo_instance_info[topo_instance_index].pid = atoi(pid_buf);  //save pid in structure
            //pre_test_check_ptr->status = CHECK_MONITOR_DATA_PROCESSED;
            //NSTL1(NULL, NULL, "Extracted pid %d for tier [%s] actual server [%s] server display name [%s] instance [%s] from line %s.", topo_instance_info[topo_instance_index].pid, topo_tier_info[topo_server_info[topo_server_indx].tierinfo_idx].TierName, topo_server_info[topo_server_indx].ServerName, topo_server_info[topo_server_indx].ServerDispName, topo_instance_info[topo_instance_index].displayName, process_line[line]);
          }  
          else
            NSTL1_OUT(NULL, NULL, "Invalid format %s. Not found pipe.\n", process_line[line]);

         
          num_times_all_pattern_matched++;

          //break;
        }
      } 
      if(num_times_all_pattern_matched == 1)
      {
       tmp_instance_ptr->pid = atoi(pid_buf);  //save pid in structure
        //pre_test_check_ptr->status = CHECK_MONITOR_DATA_PROCESSED;
        MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
             "Extracted pid %d for tier [%s] actual server [%s] server display name [%s] instance [%s]", tmp_instance_ptr->pid, topo_info[topo_idx].topo_tier_info[tier_idx].tier_name, topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[topo_server_indx].server_ptr->server_name, topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[topo_server_indx].server_disp_name, tmp_instance_ptr->display_name); 
      }
      if(num_times_all_pattern_matched > 1)
      {
        MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Found search pattern in more than one process for tier [%s] actual server [%s] server display name [%s] instance [%s]. Hence marking for retry.", topo_info[topo_idx].topo_tier_info[tier_idx].tier_name, topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[topo_server_indx].server_ptr->server_name, topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[topo_server_indx].server_disp_name, tmp_instance_ptr->display_name);
      }
      if(num_times_all_pattern_matched == 0)
      {
       MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Not found search pattern for tier [%s] actual server [%s] server display name [%s] instance [%s]. Hence marking for retry.", topo_info[topo_idx].topo_tier_info[tier_idx].tier_name, topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[topo_server_indx].server_ptr->server_name, topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[topo_server_indx].server_disp_name, tmp_instance_ptr->display_name);
      }
    }

   // if(pre_test_check_ptr->status != CHECK_MONITOR_DATA_PROCESSED)
    //{
   //   NSTL1(NULL, NULL, "Status is %d which is not CHECK_MONITOR_DATA_PROCESSED, hence setting this monitor for retry.", pre_test_check_ptr->status); 
    //  pre_test_check_ptr->status = CHECK_MONITOR_RETRY; 
    //  pre_test_check_ptr->from_event = CHECK_MONITOR_EVENT_RETRY_INTERNAL_MON;    
    //  pre_test_check_ptr->state = CHECK_MON_COMMAND_STATE;
 
      //free buffer once processed
      FREE_AND_MAKE_NULL(topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[topo_server_indx].server_ptr->server_sig_output_buffer, "topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[topo_server_indx].server_ptr->server_sig_output_buffer", -1);
  //  }
    //close_check_monitor_connection(pre_test_check_ptr);

    //if test started then only we need to remove from parent epoll
    if(ns_parent_state > NS_PARENT_ST_INIT)
      chk_mon_handle_err_case(pre_test_check_ptr, CM_REMOVE_FROM_EPOLL);
  }
 }
}

/*process completed java process server sig data stored in topology ServerInfo structure

 Format is: We have to process 'Arguments' field

Pid|Owner|StartTime|CPUTime|TDUsingPID|TDUsingJMX|UsingCmd|F1|F2|F3|Instance|Arguments|LogFileName

1200|root|09:47|00:00:02|No|No|No|NA|NA|NA|-|java -DNS_WDIR=/home/cavisson/work -Xmx1024m -Xloggc:/home/cavisson/work/webapps/netstorm/logs/gc_nsServer_work.log -XX:+PrintGCDetails -verbose:gc -XX:+PrintGCTimeStamps -XX:+PrintHeapAtGC -XX:+UseParNewGC com.cavisson.gui.server.Server|-
2273|root|09:48|00:00:16|No|No|No|NA|NA|NA|apps|/apps/java/jdk1.6.0_24/bin/java -DNS_WDIR=/home/cavisson/work -server -Djava.awt.headless=true -DHPD_ROOT=/var/www/hpd -DHPD_CMD=hpd -Xmx1024m -Xloggc:/home/cavisson/work/webapps/netstorm/logs/gc_tomcat_work.log -XX:+PrintGCDetails -verbose:gc -XX:+PrintGCTimeStamps -XX:+PrintHeapAtGC -XX:+UseParNewGC -Djava.util.logging.manager=org.apache.juli.ClassLoaderLogManager -Djava.util.logging.config.file=/apps/apache-tomcat-6.0.16/conf/logging.properties -Djava.endorsed.dirs=/apps/apache-tomcat-6.0.16/endorsed -classpath :/apps/apache-tomcat-6.0.16/bin/bootstrap.jar -Dcatalina.base=/apps/apache-tomcat-6.0.16 -Dcatalina.home=/apps/apache-tomcat-6.0.16 -Djava.io.tmpdir=/apps/apache-tomcat-6.0.16/temp org.apache.catalina.startup.Bootstrap start|/apps/apache-tomcat-6.0.16/logs/catalina.out 

Note:
   Here multiple pid matching case for a particular instance does not occur because once all specified patterns matched, will extract pid and move on to the next instance.
*/
void process_java_server_sig_data_and_save_in_instance_struct()
{
  int i = 0;

  //if "java_process_server_sig_start_indx = -1", it means that no server has been autoscaled where monitor can be appied. Either auto_scale mode on those server is < 2 (i.e. autoscaled from BCI) or its a windows server or server status is 0 (i.e Inactive server)
  if(java_process_server_sig_start_indx < 0)
    return;

  for(i = java_process_server_sig_start_indx; i < total_pre_test_check_entries; i++)
  {
    process_data(&pre_test_check_info_ptr[i]);
  }
}

//THis is for pre test failed java process server signature
void make_connections_for_java_process_server_sig()
{
  int i = 0;
  
  //if "java_process_server_sig_start_indx = -1", it means that no server has been autoscaled where monitor can be appied. Either auto_scale mode on those server is < 2 (i.e. autoscaled from BCI) or its a windows server or server status is 0 (i.e Inactive server)
  if(java_process_server_sig_start_indx < 0)
    return;

  for(i = java_process_server_sig_start_indx; i < total_pre_test_check_entries; i++)
  {
    //for partial & error make nb connection again
    //if((pre_test_check_info_ptr[i].ftp_file_size > 0) || ((pre_test_check_info_ptr[i].status != CHECK_MONITOR_PASS) && (pre_test_check_info_ptr[i].status != CHECK_MONITOR_STOPPED) && (pre_test_check_info_ptr[i].status != CHECK_MONITOR_DATA_PROCESSED))) 
    if((pre_test_check_info_ptr[i].ftp_file_size > 0) || ((pre_test_check_info_ptr[i].status != CHECK_MONITOR_PASS) && (pre_test_check_info_ptr[i].status != CHECK_MONITOR_STOPPED))) 
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Making connection for %s from make_connections_for_java_process_server_sig().", pre_test_check_info_ptr[i].check_monitor_name); 
      if(chk_mon_make_nb_conn(&pre_test_check_info_ptr[i]) < 0)
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Error in making connection for %s, Closing fd & setting status for retry.", pre_test_check_info_ptr[i].check_monitor_name);
        if(pre_test_check_info_ptr[i].fd >= 0)
        {
          close(pre_test_check_info_ptr[i].fd);
          pre_test_check_info_ptr[i].fd = -1;
        }
        pre_test_check_info_ptr[i].status = CHECK_MONITOR_RETRY;
      }
    }
  } 
}

//This is to start server signature on runtime
void start_server_sig(CheckMonitorInfo *pre_test_check_ptr)
{
  if((pre_test_check_ptr->status == CHECK_MONITOR_RETRY) && (pre_test_check_ptr->fd < 0))
  {
    if(recover_server_signature_or_not(pre_test_check_ptr) == 0)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Recovering monitor %s. Retry_count = %d, max_retry_count = %d, recovery_time_threshold_in_sec = %d, "
                        "last_retry_time_in_sec = %ld, retry_time_threshold_in_sec = %d", pre_test_check_ptr->check_monitor_name,
                     pre_test_check_ptr->retry_count, pre_test_check_ptr->max_retry_count, pre_test_check_ptr->recovery_time_threshold_in_sec,
                     pre_test_check_ptr->last_retry_time_in_sec, pre_test_check_ptr->retry_time_threshold_in_sec);
      if(chk_mon_make_nb_conn(pre_test_check_ptr) < 0)
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error in making connection for %s, Closing fd.", pre_test_check_ptr->check_monitor_name);
        if(pre_test_check_ptr->fd >= 0)
        {
          close(pre_test_check_ptr->fd);
          pre_test_check_ptr->fd = -1;
        }
      }
      else
      {
        if(pre_test_check_ptr->conn_state == CHK_MON_CONNECTED)
        {
          pre_test_check_ptr->status = CHECK_MONITOR_PASS;
          pre_test_check_ptr->retry_count = 0;
          pre_test_check_ptr->last_retry_time_in_sec = 0;
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Recovered monitor %s successfully.", pre_test_check_ptr->check_monitor_name);
        }
        else
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Recovery of monitor %s is in progress.", pre_test_check_ptr->check_monitor_name);
      }
    }
    else
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Not Recovering monitor %s.", pre_test_check_ptr->check_monitor_name); 
    }
  }
  else
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Monitor %s status is %d not retry,", pre_test_check_ptr->check_monitor_name, pre_test_check_ptr->status);
  }
}

//Recovery from deliver_report.c
void retry_connections_for_java_process_server_sig()
{
  int i = 0;

  //if "java_process_server_sig_start_indx = -1", it means that no server has been autoscaled where monitor can be appied. Either auto_scale mode on those server is < 2 (i.e. autoscaled from BCI) or its a windows server or server status is 0 (i.e Inactive server)
  if(java_process_server_sig_start_indx < 0)
    return;

  for(i = java_process_server_sig_start_indx; i < total_pre_test_check_entries; i++)
  {
    start_server_sig(&pre_test_check_info_ptr[i]);
  } 
}

// ENABLE_JAVA_PROCESS_SERVER_SIGNATURE <enable/disable> <max retry count> <retry time threshold in sec> <recovery time threshold in sec>
int kw_set_java_process_server_signature(char *keyword, char *buf, int runtime_flag, char *err_msg)
{
  char key[MAX_LENGTH] = {0};

  if((sscanf(buf, "%s %d %d %d %d", key, &java_process_server_sig_enabled, &max_retry_count, &retry_time_threshold_in_sec, &recovery_time_threshold_in_sec)) < 2)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, ENABLE_JAVA_PROCESS_SERVER_SIGNATURE_USAGE, CAV_ERR_1011154, CAV_ERR_MSG_1);
  }

  if(java_process_server_sig_enabled < 0 || java_process_server_sig_enabled > 2)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, ENABLE_JAVA_PROCESS_SERVER_SIGNATURE_USAGE, CAV_ERR_1011154, CAV_ERR_MSG_3);
  }
  return 0;
}

//This is to setup server signature on all the servers of topology, to get java instances using shell 'nsi_get_java_instances'
void setup_server_sig_get_java_process()
{
  char buf[MAX_MONITOR_BUFFER_SIZE] = {0};
  char err_msg[MAX_DATA_LINE_LENGTH] = {0};
  int i,j = 0, ret = -1;
  //ServerInfo *servers_list = NULL;

  NSDL1_MON(NULL, NULL, "Method Called.");

  //servers_list = (ServerInfo *) topolib_get_server_info();
  
  for(i = 0; i<topo_info[topo_idx].total_tier_count; i++)
  {
    for(j=0;j<topo_info[topo_idx].topo_tier_info[i].total_server;j++)
    {
    //Modified Check to apply check monitor for java process only for Server discovered from Cmon not from BCI as it could lead to duplicate entries and only for those servers which are active.
      if((topo_info[topo_idx].topo_tier_info[i].topo_server[j].used_row != -1) && (strncmp(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->machine_type, "Windows", 7) == 0) || (topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->status == 0) || topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->auto_scale < 2)    
  //ServerInfo *servers_list = NULL;
      {
        continue;
      }

      sprintf(buf, "SERVER_SIGNATURE %s%c%s %s_JavaInstanceList Command nsi_get_java_instances", topo_info[topo_idx].topo_tier_info[i].tier_name,global_settings ->hierarchical_view_vector_separator,topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->server_disp_name, topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_disp_name);

      //NSTL1(NULL, NULL, "Configuring monitor %s.", buf);

      ret = kw_set_server_signature("SERVER_SIGNATURE", buf, 0, err_msg, NULL); //TODO: validation after creating table...fix???
      if(ret < 0)
        printf("%s", err_msg);
      else
      {
        if(java_process_server_sig_start_indx == -1)
          java_process_server_sig_start_indx = ret;

        //save server signature index in topo_server_info structure, topo_server_info index in server signature structure
        topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->server_sig_indx = ret;
        pre_test_check_info_ptr[ret].topo_server_info_idx = i;
        pre_test_check_info_ptr[ret].monitor_type = INTERNAL_CHECK_MON; //change mon type

        //pre_test_check_info_ptr[ret].retry_count = -1;
        pre_test_check_info_ptr[ret].max_retry_count = max_retry_count;
        pre_test_check_info_ptr[ret].retry_time_threshold_in_sec = retry_time_threshold_in_sec;
        pre_test_check_info_ptr[ret].recovery_time_threshold_in_sec = recovery_time_threshold_in_sec;
        //pre_test_check_info_ptr[ret].last_retry_time_in_sec = -1;
        pre_test_check_info_ptr[ret].status = CHECK_MONITOR_RETRY;

        //NSTL1(NULL, NULL, "Monitor %s configured successfully. topo server info index is %d & check monitor index is %d.", buf, topo_server_info[i].server_sig_indx, pre_test_check_info_ptr[ret].topo_server_info_idx);
      }
    }
  }
}

/*************** JAVA PROCESS SERVER SIGNATURE SETUP CODE ENDS ************************************/


//This method used to set pre_test_check_retry_count
void set_pre_test_check_retry_count(char *value)
{
  NSDL2_MON(NULL, NULL, "Method called. Retry count = %s", value);
  pre_test_check_retry_count = atoi(value);
  if (pre_test_check_retry_count <= 0) 
  { 
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Retry count should be greater than 0\n");
    NS_EXIT(-1, CAV_ERR_1031006);
  }
  NSDL3_MON(NULL, NULL, "pre_test_check_retry_count = %d", pre_test_check_retry_count);
}

//This method used to set pre_test_check_retry_interval 
void set_pre_test_check_retry_interval(char *value)
{
  NSDL2_MON(NULL, NULL, "Method called. Retry interval (in second) = %s", value);
  pre_test_check_retry_interval = atoi(value);
  if (pre_test_check_retry_interval <= 0)
  { 
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Retry Interval should be greater than 0\n");
    NS_EXIT(-1, CAV_ERR_1031007);
  }
  NSDL3_MON(NULL, NULL, "pre_test_check_retry_interval = %d sec", pre_test_check_retry_interval);
}

//This method used to set check_monitor_timeout
int set_pre_test_check_timeout(char *value, char *err_msg)
{
  if(pre_test_check_timeout == -1)
  {
    //NSDL2_MON(NULL, NULL, "Method called. Timeout (in second) = %s", value);
    pre_test_check_timeout = atoi(value);
    if (pre_test_check_timeout <= 0) 
    { 
      //NSTL1(NULL, NULL, "Timeout should be greater than 0\n");
      //NS_EXIT(-1, "Timeout should be greater than 0");
      sprintf(err_msg, CAV_ERR_1031008);
      return -1;
    }
    //NSDL3_MON(NULL, NULL, "check_monitor_timeout = %d sec", pre_test_check_timeout);
  }
  return 0;
}

//This method used to set test name when pass -N option as argument
void set_tname(char *buf)
{
  NSDL2_MON(NULL, NULL, "Method called. Test Name = \"%s\"", buf);

  if(global_settings->testname[0] == '\0') 
  {     
    strncpy(global_settings->testname, buf, MAX_TNAME_LENGTH);
    global_settings->testname[MAX_TNAME_LENGTH] = '\0';
  }       

  NSDL3_MON(NULL, NULL, "Test Name \"%s\"", global_settings->testname);
}


/* --- End: Method used during Parsing Time  ---*/


/* --- START: Method used during Run Time  ---*/

static void free_pre_test_check(int total_entries, CheckMonitorInfo *check_monitor_ptr, int do_table_free)
{
  int i;
  //CheckMonitorInfo *check_monitor_ptr = pre_test_check_info_ptr; 
  NSDL2_MON(NULL, NULL, "Method called. total_entries = %d, do_table_free = %d", total_entries, do_table_free);

  for (i = 0; i < total_entries; i++, check_monitor_ptr++)
  {
    FREE_AND_MAKE_NOT_NULL(check_monitor_ptr->check_monitor_name, "check_monitor_ptr->check_monitor_name", -1);
    FREE_AND_MAKE_NOT_NULL(check_monitor_ptr->cs_ip, "check_monitor_ptr->cs_ip", -1);
    FREE_AND_MAKE_NOT_NULL(check_monitor_ptr->rem_ip, "check_monitor_ptr->rem_ip", -1);
    FREE_AND_MAKE_NOT_NULL(check_monitor_ptr->rem_username, "check_monitor_ptr->rem_username", -1);
    FREE_AND_MAKE_NOT_NULL(check_monitor_ptr->rem_password, "check_monitor_ptr->rem_password", -1);
    FREE_AND_MAKE_NOT_NULL(check_monitor_ptr->pgm_path, "check_monitor_ptr->pgm_path", -1);
    FREE_AND_MAKE_NOT_NULL(check_monitor_ptr->pgm_args, "check_monitor_ptr->pgm_args", -1);
    FREE_AND_MAKE_NOT_NULL(check_monitor_ptr->partial_buffer, "check_monitor_ptr->partial_buffer", -1);
    FREE_AND_MAKE_NOT_NULL(check_monitor_ptr->origin_cmon, "check_monitor_ptr->origin_cmon", -1);
    FREE_AND_MAKE_NOT_NULL(check_monitor_ptr->g_mon_id, "check_monitor_ptr->g_mon_id", -1);
    FREE_AND_MAKE_NOT_NULL(check_monitor_ptr->tier_name, "check_monitor_ptr->tier_name", -1);
    FREE_AND_MAKE_NOT_NULL(check_monitor_ptr->server_name, "check_monitor_ptr->server_name", -1);
    FREE_AND_MAKE_NOT_NULL(check_monitor_ptr->instance_name, "check_monitor_ptr->instance_name", -1);
    FREE_AND_MAKE_NOT_NULL(check_monitor_ptr->mon_name, "check_monitor_ptr->mon_name", -1);
    FREE_AND_MAKE_NOT_NULL(check_monitor_ptr->end_phase_name, "check_monitor_ptr->end_phase_name", -1);
    FREE_AND_MAKE_NOT_NULL(check_monitor_ptr->start_phase_name, "check_monitor_ptr->start_phase_name", -1);
    FREE_AND_MAKE_NOT_NULL(check_monitor_ptr->json_args, "check_monitor_ptr->json_args", -1);
    FREE_AND_MAKE_NOT_NULL(check_monitor_ptr->partial_buf, "check_monitor_ptr->partial_buf", -1);
    FREE_AND_MAKE_NOT_NULL(check_monitor_ptr->origin_cmon, "check_monitor_ptr->origin_cmon", -1);
  }
  if(do_table_free)
  {
    FREE_AND_MAKE_NULL(pre_test_check_info_ptr, "pre_test_check_info_ptr", -1);
    total_pre_test_check_entries = 0;
    max_pre_test_check_entries = 0;
  }

  FREE_AND_MAKE_NULL(failed_monitor, "failed monitor", -1);

  if(epoll_fd > 0) close(epoll_fd);
}

/* --- START: Method used for epoll  ---*/

//This method is to close all connections for server signature and Start Event - "Before Test Run Starts" of check monitor
static inline void close_connection_all_pre_test_check()
{
  int i;
  CheckMonitorInfo *check_monitor_ptr = pre_test_check_info_ptr;
  NSDL2_MON(NULL, NULL, "Method called.");
  for (i = 0; i < total_pre_test_check_entries; i++, check_monitor_ptr++)
    close_check_monitor_connection(check_monitor_ptr);
}

//This method is abort pre test check and exit
static inline void abort_pre_test_check_and_exit(char *err_msg)
{
  NSDL2_MON(NULL, NULL, "Method called. Error Message is '%s'", err_msg);
  NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_MAJOR, __FILE__, (char*)__FUNCTION__,
                                       "%s", err_msg);
  MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Method called. Error Message is '%s'", err_msg);
  if(!is_outbound_connection_enabled)
    close_connection_all_pre_test_check();

  NS_EXIT(-1, "Method called. Error Message is '%s'", err_msg);
}

static inline void epoll_init()
{
  NSDL2_MON(NULL, NULL, "Method called");

  if ((epoll_fd = epoll_create(total_pre_test_check_entries)) == -1) 
  {
    char err_msg[MAX_LENGTH];
    sprintf(err_msg, "epoll_init() - Failed to create epoll for Check Monitor/Batch job. Error is '%s'. Exiting ...", nslb_strerror(errno));
    abort_pre_test_check_and_exit(err_msg);
  }
}


inline void add_select_pre_test_check(char* data_ptr, int fd, int event, int check_mon_state)
{
  struct epoll_event pfd;

  NSDL2_MON(NULL, NULL, "Method called. Adding fd = %d for event = %x", fd, event);
  bzero(&pfd, sizeof(struct epoll_event)); //Added after valgrind reported bug

  pfd.events = event;
  pfd.data.ptr = (void *) data_ptr;

  //epoll_fd = epoll created for pre test check monitors. Can we create main epoll here ?
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &pfd) == -1)
  {
    char err_msg[MAX_LENGTH];
    sprintf(err_msg, "add_select_pre_test_check() - Add epoll failed. Error is '%s'. Exiting ...", nslb_strerror(errno));
    if(check_mon_state == CHECK_MONITOR_EVENT_AFTER_TEST_IS_OVER)
      abort_post_test_check_and_exit(err_msg);
    else abort_pre_test_check_and_exit(err_msg);
  }
}

inline void delete_select_pre_test_check(int fd)
{
  struct epoll_event pfd;

  NSDL2_MON(NULL, NULL, "Method called, Removing %d from select", fd);
  bzero(&pfd, sizeof(struct epoll_event)); //Added after valgrind reported bug
 
  if(fd != -1)
  { 
    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, &pfd) == -1) 
    {
      char err_msg[MAX_LENGTH];
      sprintf(err_msg, "delete_select_pre_test_check() - Failed to delete epoll. Error is '%s'. Exiting ...", nslb_strerror(errno));
      abort_pre_test_check_and_exit(err_msg);
    }
  }
}

inline void mod_select_pre_test_check(char* data_ptr, int fd, int event) 
{
  struct epoll_event pfd;
        
  NSDL2_MON(NULL, NULL, "Method called. Moding %d for event=%x, my_port_index = %d", fd, event, my_port_index);
  bzero(&pfd, sizeof(struct epoll_event)); 
        
  pfd.events = event;
  pfd.data.ptr = (void*) data_ptr;
  if(fd != -1)
  {
    if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &pfd) == -1) 
    {
      NSDL2_MON(NULL, NULL, "Error occured in mod_select_pre_test_check(), epoll mod: err = %s", nslb_strerror(errno));
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "\nError occured in mod_select_pre_test_check(), epoll mod: err = %s\n", nslb_strerror(errno));
      NS_EXIT (-1, "\nError occured in mod_select_pre_test_check(), epoll mod: err = %s\n", nslb_strerror(errno));
    } 
  }
} 

/* --- END: Method used for epoll  ---*/



inline void mark_check_monitor_fail(CheckMonitorInfo *check_monitor_ptr, int deselect, int status)
{
  char ChkMon_BatchJob[50];
  char alert_msg[ALERT_MSG_SIZE + 1];
  char buffer[4 * ALERT_MSG_SIZE];
  int len;

  NSDL2_MON(NULL, NULL, "Method called. CheckMonitor => %s, check_monitor_ptr->fd = %d, deselect = %d, status = %d", CheckMonitor_to_string(check_monitor_ptr, s_check_monitor_buf), check_monitor_ptr->fd, deselect, status);

  if(check_monitor_ptr->bj_success_criteria > 0) 
    strcpy(ChkMon_BatchJob, "Batch Job");
  else
    strcpy(ChkMon_BatchJob, "Check Monitor");
  
  NSDL2_MON(NULL, NULL, "ChkMon_BatchJob = %s", ChkMon_BatchJob);

  if(deselect)
  {
    if(check_monitor_ptr->from_event == CHECK_MONITOR_EVENT_BEFORE_TEST_IS_STARTED) 
    {
      delete_select_pre_test_check(check_monitor_ptr->fd);
      --num_pre_test_check;
    }
    else if(check_monitor_ptr->from_event == CHECK_MONITOR_EVENT_AFTER_TEST_IS_OVER)
    {
      delete_select_pre_test_check(check_monitor_ptr->fd);
      --num_post_test_check;
    }
    else
      remove_select_msg_com_con_ex(__FILE__, __LINE__, (char *)__FUNCTION__,check_monitor_ptr->fd);
  }
  close_check_monitor_connection(check_monitor_ptr);
  check_monitor_ptr->status = status;
  
  ns_check_monitor_log(EL_DF, DM_EXECUTION, MM_MON, _FLN_, check_monitor_ptr,
                               EID_CHKMON_ERROR, EVENT_MAJOR,
                               "%s failed - %s: %s",
                               ChkMon_BatchJob, check_monitor_ptr->check_monitor_name, error_msg[status]);
  sprintf(alert_msg, ALERT_MSG_1017007, ChkMon_BatchJob, check_monitor_ptr->check_monitor_name, check_monitor_ptr->server_name, check_monitor_ptr->tier_name,error_msg[status]);

  if((len = nslb_make_cavisson_alert_body(buffer, ALERT_MSG_BUF_SIZE, testidx, alert_msg, ALERT_CRITICAL, ChkMon_BatchJob, check_monitor_ptr->tier_name, check_monitor_ptr->server_name, NULL, global_settings->hierarchical_view_topology_name, global_settings->alert_info->policy, global_settings->alert_info->protocol, g_cavinfo.NSAdminIP, parent_port_number, ALERT_FAIL_VALUE, global_settings->progress_secs)) < 0)
  {
    NSTL1(NULL, NULL, "Failed to make POST body, testidx = %d, alert_type = %d, alert_policy = %s, alert_msg = %s",
                         testidx, ALERT_CRITICAL, global_settings->alert_info->policy, alert_msg);
  }
 
  NSTL1(NULL, NULL, "Going to send alert msg, tomcat port = %d, testid = %d", g_tomcat_port, testidx);
  if (ns_send_alert_ex(ALERT_CRITICAL, HTTP_METHOD_POST, NS_CONTENT_TYPE_JSON, buffer, len) < 0)
    NSTL1(NULL, NULL, "Failed to send alert msg to Dashboard.");

}


inline void mark_check_monitor_fail_v2(CheckMonitorInfo *check_monitor_ptr, int deselect, int status)
{
  char ChkMon_BatchJob[50];
  char alert_msg[ALERT_MSG_SIZE + 1];
  char buffer[4 * ALERT_MSG_SIZE];
  int len;

  NSDL2_MON(NULL, NULL, "Method called. CheckMonitor => %s, check_monitor_ptr->fd = %d, deselect = %d, status = %d", CheckMonitor_to_string(check_monitor_ptr, s_check_monitor_buf), check_monitor_ptr->fd, deselect, status);

  if(check_monitor_ptr->bj_success_criteria > 0) 
    strcpy(ChkMon_BatchJob, "Batch Job");
  else
    strcpy(ChkMon_BatchJob, "Check Monitor");
  
  NSDL2_MON(NULL, NULL, "ChkMon_BatchJob = %s", ChkMon_BatchJob);

  if(deselect)
  {
    if(check_monitor_ptr->from_event == CHECK_MONITOR_EVENT_BEFORE_TEST_IS_STARTED) 
    {
      delete_select_pre_test_check(check_monitor_ptr->fd);
      --num_pre_test_check;
    }
    else if(check_monitor_ptr->from_event == CHECK_MONITOR_EVENT_AFTER_TEST_IS_OVER)
    {
      delete_select_pre_test_check(check_monitor_ptr->fd);
      --num_post_test_check;
    }
    else
      REMOVE_SELECT_MSG_COM_CON(check_monitor_ptr->fd, DATA_MODE);
  }
  close_check_monitor_connection(check_monitor_ptr);
  check_monitor_ptr->status = status;
  
  ns_check_monitor_log(EL_DF, DM_EXECUTION, MM_MON, _FLN_, check_monitor_ptr,
                               EID_CHKMON_ERROR, EVENT_MAJOR,
                               "%s failed - %s: %s",
                               ChkMon_BatchJob, check_monitor_ptr->check_monitor_name, error_msg[status]);
  sprintf(alert_msg, ALERT_MSG_1017007, ChkMon_BatchJob, check_monitor_ptr->check_monitor_name, check_monitor_ptr->server_name, check_monitor_ptr->tier_name,error_msg[status]);

  if((len  = nslb_make_cavisson_alert_body(buffer, ALERT_MSG_BUF_SIZE, testidx, alert_msg, ALERT_CRITICAL, ChkMon_BatchJob, check_monitor_ptr->tier_name, check_monitor_ptr->server_name, NULL, global_settings->hierarchical_view_topology_name, global_settings->alert_info->policy, global_settings->alert_info->protocol, g_cavinfo.NSAdminIP, parent_port_number, ALERT_FAIL_VALUE, global_settings->progress_secs)) < 0)
  {
     NSTL1(NULL, NULL, "Failed to make POST body, testidx = %d, alert_type = %d, alert_policy = %s, alert_msg = %s",
                         testidx, ALERT_CRITICAL, global_settings->alert_info->policy, alert_msg);
  }
  
  NSTL1(NULL, NULL, "Going to send alert msg, tomcat port = %d, testid = %d", g_tomcat_port, testidx);
  if (ns_send_alert_ex(ALERT_CRITICAL, HTTP_METHOD_POST, NS_CONTENT_TYPE_JSON, buffer, len) < 0)
     NSTL1(NULL, NULL, "Failed to send alert msg to Dashboard.");

}


inline void mark_check_monitor_pass(CheckMonitorInfo *check_monitor_ptr, int deselect, int status)
{
  char ChkMon_BatchJob[50];
  char alert_msg[ALERT_MSG_SIZE + 1];
  char buffer[4 * ALERT_MSG_SIZE];
  int len;

  NSDL2_MON(NULL, NULL, "Method called. CheckMonitor => %s, check_monitor_ptr->fd = %d, deselect = %d, status = %d", CheckMonitor_to_string(check_monitor_ptr, s_check_monitor_buf), check_monitor_ptr->fd, deselect, status);

  if(check_monitor_ptr->bj_success_criteria > 0)
    strcpy(ChkMon_BatchJob, "Batch Job");
  else
    strcpy(ChkMon_BatchJob, "Check Monitor");

  NSDL2_MON(NULL, NULL, "ChkMon_BatchJob = %s", ChkMon_BatchJob);

  check_monitor_ptr->status = status;

  ns_check_monitor_log(EL_DF, DM_EXECUTION, MM_MON, _FLN_, check_monitor_ptr,
                                             EID_CHKMON_GENERAL, EVENT_INFO,
                                             "%s passed - %s",
                                             ChkMon_BatchJob, check_monitor_ptr->check_monitor_name);
  sprintf(alert_msg, ALERT_MSG_1017008, ChkMon_BatchJob, check_monitor_ptr->check_monitor_name, check_monitor_ptr->server_name, check_monitor_ptr->tier_name);

  if((len = nslb_make_cavisson_alert_body(buffer, ALERT_MSG_BUF_SIZE, testidx, alert_msg, ALERT_INFO, ChkMon_BatchJob, check_monitor_ptr->tier_name, check_monitor_ptr->server_name, NULL, global_settings->hierarchical_view_topology_name, global_settings->alert_info->policy, global_settings->alert_info->protocol, g_cavinfo.NSAdminIP, parent_port_number, ALERT_PASS_VALUE, global_settings->progress_secs)) < 0)
  {
     NSTL1(NULL, NULL, "Failed to make POST body, testidx = %d, alert_type = %d, alert_policy = %s, alert_msg = %s",
                         testidx, ALERT_INFO, global_settings->alert_info->policy, alert_msg);
  }  
 
  NSTL1(NULL, NULL, "Going to send alert msg, tomcat port = %d, testid = %d", g_tomcat_port, testidx);
  if (ns_send_alert_ex(ALERT_INFO, HTTP_METHOD_POST, NS_CONTENT_TYPE_JSON, buffer, len) < 0)
    NSTL1(NULL, NULL, "Failed to send alert msg to Dashboard.");
  
  #ifdef NS_DEBUG_ON
    //printf("%s passed - %s\n", ChkMon_BatchJob, check_monitor_ptr->check_monitor_name);
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "%s passed - %s", ChkMon_BatchJob, check_monitor_ptr->check_monitor_name);
   #endif

  if(check_monitor_ptr->option == RUN_EVERY_TIME_OPTION) //Run periodic
  {
    if(check_monitor_ptr->max_count == -1)
    {
      if(strcmp(check_monitor_ptr->end_phase_name, "")==0)
      {
        ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
				  EID_CHKMON_GENERAL, EVENT_INFO,
				  "Since %s '%s' is running periodically till test is over,"
				  " so connection not closed. Returning...",
				  ChkMon_BatchJob, check_monitor_ptr->check_monitor_name);
      }
      else
      {
        ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
                                  EID_CHKMON_GENERAL, EVENT_INFO,
                                  "Since %s '%s' is running periodically till %s phase is over,"
                                  " so connection not closed. Returning...",
                                  ChkMon_BatchJob, check_monitor_ptr->check_monitor_name,check_monitor_ptr->end_phase_name);
      }
      return;
    }
    check_monitor_ptr->max_count--;
    if(check_monitor_ptr->max_count > 0)
    {
      ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
				  EID_CHKMON_GENERAL, EVENT_INFO,
				  "Since %s '%s' is running periodically and"
				  " repeat count left (%d) is not 0, so connection not closed. Returning...",
				  ChkMon_BatchJob, check_monitor_ptr->check_monitor_name, check_monitor_ptr->max_count);
      return;
    }
    ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
					  EID_CHKMON_GENERAL, EVENT_INFO,
					  "%s '%s' repeat count left 0, so connection is closed.",
					  ChkMon_BatchJob, check_monitor_ptr->check_monitor_name, check_monitor_ptr->max_count);
  }

  if(deselect)
  {
    if(check_monitor_ptr->from_event == CHECK_MONITOR_EVENT_BEFORE_TEST_IS_STARTED)
    {
      delete_select_pre_test_check(check_monitor_ptr->fd);
      --num_pre_test_check;
    }
    else if(check_monitor_ptr->from_event == CHECK_MONITOR_EVENT_AFTER_TEST_IS_OVER)
    {
      delete_select_pre_test_check(check_monitor_ptr->fd);
      --num_post_test_check;
    }
    else
      remove_select_msg_com_con_ex(__FILE__, __LINE__, (char *)__FUNCTION__,check_monitor_ptr->fd);
  }
  close_check_monitor_connection(check_monitor_ptr);
}

inline void mark_check_monitor_pass_v2(CheckMonitorInfo *check_monitor_ptr, int deselect, int status)
{
  char ChkMon_BatchJob[50];
  char alert_msg[ALERT_MSG_SIZE + 1];
  char buffer[4 * ALERT_MSG_SIZE];
  int len;

  NSDL2_MON(NULL, NULL, "Method called. CheckMonitor => %s, check_monitor_ptr->fd = %d, deselect = %d, status = %d", CheckMonitor_to_string(check_monitor_ptr, s_check_monitor_buf), check_monitor_ptr->fd, deselect, status);

  if(check_monitor_ptr->bj_success_criteria > 0)
    strcpy(ChkMon_BatchJob, "Batch Job");
  else
    strcpy(ChkMon_BatchJob, "Check Monitor");

  NSDL2_MON(NULL, NULL, "ChkMon_BatchJob = %s", ChkMon_BatchJob);

  check_monitor_ptr->status = status;

  ns_check_monitor_log(EL_DF, DM_EXECUTION, MM_MON, _FLN_, check_monitor_ptr,
                                             EID_CHKMON_GENERAL, EVENT_INFO,
                                             "%s passed - %s",
                                             ChkMon_BatchJob, check_monitor_ptr->check_monitor_name);

  sprintf(alert_msg, ALERT_MSG_1017008, ChkMon_BatchJob, check_monitor_ptr->check_monitor_name, check_monitor_ptr->server_name, check_monitor_ptr->tier_name);

  if((len = nslb_make_cavisson_alert_body(buffer, ALERT_MSG_BUF_SIZE, testidx, alert_msg, ALERT_INFO, ChkMon_BatchJob, check_monitor_ptr->tier_name, check_monitor_ptr->server_name, NULL, global_settings->hierarchical_view_topology_name, global_settings->alert_info->policy, global_settings->alert_info->protocol, g_cavinfo.NSAdminIP, parent_port_number, ALERT_PASS_VALUE, global_settings->progress_secs)) < 0)
  {
    NSTL1(NULL, NULL, "Failed to make POST body, testidx = %d, alert_type = %d, alert_policy = %s, alert_msg = %s",
                         testidx, ALERT_INFO, global_settings->alert_info->policy, alert_msg);
  }
 
  NSTL1(NULL, NULL, "Going to send alert msg, tomcat port = %d, testid = %d", g_tomcat_port, testidx);
  if (ns_send_alert_ex(ALERT_INFO, HTTP_METHOD_POST, NS_CONTENT_TYPE_JSON, buffer, len) < 0)
     NSTL1(NULL, NULL, "Failed to send alert msg to Dashboard.");
  

  #ifdef NS_DEBUG_ON
    //printf("%s passed - %s\n", ChkMon_BatchJob, check_monitor_ptr->check_monitor_name);
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "%s passed - %s", ChkMon_BatchJob, check_monitor_ptr->check_monitor_name);
  #endif

  if(check_monitor_ptr->option == RUN_EVERY_TIME_OPTION) //Run periodic
  {
    if(check_monitor_ptr->max_count == -1)
    {
      if(strcmp(check_monitor_ptr->end_phase_name, "")==0)
      {
        ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
				  EID_CHKMON_GENERAL, EVENT_INFO,
				  "Since %s '%s' is running periodically till test is over,"
				  " so connection not closed. Returning...",
				  ChkMon_BatchJob, check_monitor_ptr->check_monitor_name);
      }
      else
      {
        ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
                                  EID_CHKMON_GENERAL, EVENT_INFO,
                                  "Since %s '%s' is running periodically till %s phase is over,"
                                  " so connection not closed. Returning...",
                                  ChkMon_BatchJob, check_monitor_ptr->check_monitor_name,check_monitor_ptr->end_phase_name);
      }
      return;
    }
    check_monitor_ptr->max_count--;
    if(check_monitor_ptr->max_count > 0)
    {
      ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
				  EID_CHKMON_GENERAL, EVENT_INFO,
				  "Since %s '%s' is running periodically and"
				  " repeat count left (%d) is not 0, so connection not closed. Returning...",
				  ChkMon_BatchJob, check_monitor_ptr->check_monitor_name, check_monitor_ptr->max_count);
      return;
    }
    ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
					  EID_CHKMON_GENERAL, EVENT_INFO,
					  "%s '%s' repeat count left 0, so connection is closed.",
					  ChkMon_BatchJob, check_monitor_ptr->check_monitor_name, check_monitor_ptr->max_count);
  }

  if(deselect)
  {
    if(check_monitor_ptr->from_event == CHECK_MONITOR_EVENT_BEFORE_TEST_IS_STARTED)
    {
      delete_select_pre_test_check(check_monitor_ptr->fd);
      --num_pre_test_check;
    }
    else if(check_monitor_ptr->from_event == CHECK_MONITOR_EVENT_AFTER_TEST_IS_OVER)
    {
      delete_select_pre_test_check(check_monitor_ptr->fd);
      --num_post_test_check;
    }
    else
      REMOVE_SELECT_MSG_COM_CON(check_monitor_ptr->fd, DATA_MODE);
  }
  close_check_monitor_connection(check_monitor_ptr);
}

/*
   This method to setup the connection with Create Server
   Make connections to all create servers where check monitor is to be executed
   If any check monitor was fail earlier, then make connection again as Long as all check monitors not pass or retry count is not 0
   Retry count and retry interval are for all Check monitor with Start Event 'Before test is started' until all Check monitor pass.
   All Check monitor for Start Event 'Before test is started' should be run again based on retry count.
*/

static int make_connections()
{
  
  int pre_test_check_id;
  CheckMonitorInfo *check_monitor_ptr = pre_test_check_info_ptr; 
  char ChkMon_BatchJob[50];
  char alert_msg[ALERT_MSG_SIZE + 1];
  char buffer[4 * ALERT_MSG_SIZE];
  buffer[0] = '\0';
  alert_msg[0] = '\0'; 
  int len;
  int tier_idx=pre_test_check_info_ptr->tier_idx;
  if(tier_idx >= 0)
  {
    int server_idx=pre_test_check_info_ptr->server_index;
 
  //ServerInfo *servers_list = NULL;
  //servers_list = (ServerInfo *) topolib_get_server_info();
 
  NSDL2_MON(NULL, NULL, "Method called, total_pre_test_check_entries = %d", total_pre_test_check_entries);

  //printf("Starting Check monitors/Batch Jobs\n");
  MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Starting Check monitors/Batch Jobs");
  for (pre_test_check_id = 0; pre_test_check_id < total_pre_test_check_entries; pre_test_check_id++, check_monitor_ptr++)
  {
    if((pid_received_from_ndc == 1) && (check_monitor_ptr->monitor_type == INTERNAL_CHECK_MON))
    {
      continue;
    }

    if(check_monitor_ptr->bj_success_criteria > 0)
      strcpy(ChkMon_BatchJob, "Batch Job");
    else
      strcpy(ChkMon_BatchJob, "Check Monitor");

    if(topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->topo_servers_list->cntrl_conn_state & CTRL_CONN_ERROR)
    {
      ns_check_monitor_log(EL_DF, DM_EXECUTION, MM_MON, _FLN_,  check_monitor_ptr,
                                 EID_CHKMON_GENERAL, EVENT_INFO,
                                 "Got Unknown server error from gethostbyname for %s - %s. So skipping connection making for it.",
                                 ChkMon_BatchJob, check_monitor_ptr->check_monitor_name);
      sprintf(alert_msg, ALERT_MSG_1017010, ChkMon_BatchJob, check_monitor_ptr->check_monitor_name, check_monitor_ptr->server_name, check_monitor_ptr->tier_name, "Got Unknown server error from gethostbyname");

      if((len = nslb_make_cavisson_alert_body(buffer, ALERT_MSG_BUF_SIZE, testidx, alert_msg, ALERT_CRITICAL, ChkMon_BatchJob, check_monitor_ptr->tier_name, check_monitor_ptr->server_name, NULL, global_settings->hierarchical_view_topology_name, global_settings->alert_info->policy, global_settings->alert_info->protocol, g_cavinfo.NSAdminIP, parent_port_number, ALERT_FAIL_VALUE, global_settings->progress_secs)) < 0)
      {    
        NSTL1(NULL, NULL, "Failed to make POST body, testidx = %d, alert_type = %d, alert_policy = %s, alert_msg = %s",
                         testidx, ALERT_CRITICAL, global_settings->alert_info->policy, alert_msg);
      }
   
      NSTL1(NULL, NULL, "Going to send alert msg, tomcat port = %d, testid = %d", g_tomcat_port, testidx);
      if (ns_send_alert_ex(ALERT_CRITICAL, HTTP_METHOD_POST, NS_CONTENT_TYPE_JSON, buffer, len) < 0)
        NSTL1(NULL, NULL, "Failed to send alert msg to Dashboard");
      
      continue;
    }
    buffer[0] = '\0';
    alert_msg[0] = '\0';
    
    ns_check_monitor_log(EL_DF, DM_EXECUTION, MM_MON, _FLN_,  check_monitor_ptr,
                                 EID_CHKMON_GENERAL, EVENT_INFO,
                                 "Starting %s - %s",
                                 ChkMon_BatchJob, check_monitor_ptr->check_monitor_name);
    #ifdef NS_DEBUG_ON
      //printf("Starting %s - %s\n", ChkMon_BatchJob, check_monitor_ptr->check_monitor_name);
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Starting %s - %s", ChkMon_BatchJob, check_monitor_ptr->check_monitor_name);  
    #endif
    sprintf(alert_msg, ALERT_MSG_1017009, ChkMon_BatchJob, check_monitor_ptr->check_monitor_name, check_monitor_ptr->server_name, check_monitor_ptr->tier_name);

    if((len = nslb_make_cavisson_alert_body(buffer, ALERT_MSG_BUF_SIZE, testidx, alert_msg, ALERT_INFO, check_monitor_ptr->check_monitor_name, check_monitor_ptr->tier_name, check_monitor_ptr->server_name, NULL, global_settings->hierarchical_view_topology_name, global_settings->alert_info->policy, global_settings->alert_info->protocol, g_cavinfo.NSAdminIP, parent_port_number, ALERT_PASS_VALUE, global_settings->progress_secs)) < 0)
    {
       NSTL1(NULL, NULL, "Failed to make POST body, testidx = %d, alert_type = %d, alert_policy = %s, alert_msg = %s",
                         testidx, ALERT_INFO, global_settings->alert_info->policy, alert_msg);
    }

    NSTL1(NULL, NULL, "Going to send alert msg, tomcat port = %d, testid = %d", g_tomcat_port, testidx);
      
    if (ns_send_alert_ex(ALERT_INFO, HTTP_METHOD_POST, NS_CONTENT_TYPE_JSON, buffer, len) < 0)
       NSTL1(NULL, NULL, "Failed to send alert msg to Dashboard");
    

    NSDL3_MON(NULL, NULL, "%s => %s", ChkMon_BatchJob, CheckMonitor_to_string(check_monitor_ptr, s_check_monitor_buf));
  
    NSDL2_MON(NULL, NULL, "Making connection for %s '%s'", ChkMon_BatchJob, check_monitor_ptr->check_monitor_name);
    //check_monitor_ptr->fd = nslb_tcp_client(check_monitor_ptr->cs_ip, check_monitor_ptr->cs_port);
int ret =  chk_mon_make_nb_conn(check_monitor_ptr);
    if (ret>=0)
    num_pre_test_check++;
  }
   
  //We need to add fd in epoll as we will sned message for pre test check monitor on a different epoll. And data will come on this epoll only. Once pretest check monitors are finished, we will remove this epoll. And for rest of the monitors data will be coming in the main epoll created in wait_forever.c
  /*
  if((total_pre_test_check_entries > 0) && is_outbound_connection_enabled)
    add_select_pre_test_check((char *)&ndc_data_mccptr, ndc_data_mccptr.fd, EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP, CHECK_MONITOR_EVENT_BEFORE_TEST_IS_STARTED);
  */    
  return num_pre_test_check;
  }
return 0;
}

inline void abort_pre_test_checks(int epoll_check, char *err)
{
  if(epoll_check == 0)
  {
    NSDL1_MON(NULL, NULL, "Timeout while waiting for Check Monitor/Batch Job results. Number of Check Monitor/Batch Job not complete = %d", num_pre_test_check);
    //to print on progress report and console
    NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_MAJOR, __FILE__, (char*)__FUNCTION__,
                                         "Timeout while waiting for Check Monitor/Batch Job results."
					 " Number of Check Monitor/Batch Job not complete = %d.",
					 num_pre_test_check);

    //NSTL1_OUT(NULL, NULL, "\nTimeout while waiting for Check Monitor/Batch Job results. Number of Check Monitor/Batch Job not completed due to timeout = %d.", num_pre_test_check);
     MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Timeout while waiting for Check Monitor/Batch Job results. Number of Check Monitor/Batch Job not completed due to timeout = %d.", num_pre_test_check);
    num_pre_test_check = 0; // We need to abort this cycle
    return;
  }
  char err_msg[MAX_LENGTH];
  NSDL1_MON(NULL, NULL, "Error while waiting for Check Monitor/Batch Job results. Number of Check Monitor/Batch Job not complete = %d.", num_pre_test_check);
  sprintf(err_msg, "Error : while waiting for Check Monitor/Batch Job results. So %s failed for From Event 'Before test is started'. Exiting ...", err);
  if(!(global_settings->continue_on_pre_test_check_mon_failure))
  {
    NSDL1_MON(NULL, NULL, "Since CONTINUE_ON_MONITOR_ERROR flag for Pre Test Check monitors is off so aborting and exitting.");
    abort_pre_test_check_and_exit(err_msg);
  }
}

static void wait_for_results()
{
  int cnt, i;
  int epoll_time_out;
  struct epoll_event *epev = NULL;
  CheckMonitorInfo *check_monitor_ptr = pre_test_check_info_ptr;
  char ChkMon_BatchJob[50];
  char alert_msg[ALERT_MSG_SIZE + 1];
  char buffer[4 * ALERT_MSG_SIZE];
  buffer[0] = '\0';
  alert_msg[0] = '\0';
  int len;


  epoll_time_out = pre_test_check_timeout;

  NSDL2_MON(NULL, NULL, "Method called. num_pre_test_check = %d, epoll_timeout in sec = %d", num_pre_test_check, epoll_time_out);

  MY_MALLOC(epev, sizeof(struct epoll_event) * total_pre_test_check_entries, "CheckMonitor epev", -1);

  while(1)
  {
    if(num_pre_test_check == 0)
    {
      NSDL2_MON(NULL, NULL, "Check Monitor/Batch Job done.");
      break;
    }
    memset(epev, 0, sizeof(struct epoll_event) * total_pre_test_check_entries);
    cnt = epoll_wait(epoll_fd, epev, num_pre_test_check, epoll_time_out * 1000);
    NSDL4_MON(NULL, NULL, "epoll wait return value = %d", cnt);
   
    if(check_monitor_ptr->bj_success_criteria > 0)
      strcpy(ChkMon_BatchJob, "Batch Job");
    else
      strcpy(ChkMon_BatchJob, "Check Monitor");

    if (cnt > 0)
    {
      for (i = 0; i < cnt; i++)
      {
        check_monitor_ptr = (CheckMonitorInfo *)epev[i].data.ptr;
        if (epev[i].events & EPOLLERR) 
          mark_check_monitor_fail(check_monitor_ptr, 1, CHECK_MONITOR_EPOLLERR);
        else if (epev[i].events & EPOLLIN)
          read_output_from_cs(check_monitor_ptr);
        else if (epev[i].events & EPOLLOUT)    //for NoN-Blocking Code
          pre_chk_mon_send_msg_to_cmon(check_monitor_ptr);
        else
        {
          ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
				      EID_CHKMON_ERROR, EVENT_MAJOR,
				      "Event failed for Check Monitor/Batch Job '%s'. epev[i].events = %d err = %s",
				       check_monitor_ptr->check_monitor_name, epev[i].events, nslb_strerror(errno));
          sprintf(alert_msg, ALERT_MSG_1017010, ChkMon_BatchJob, check_monitor_ptr->check_monitor_name, check_monitor_ptr->server_name, check_monitor_ptr->tier_name, nslb_strerror(errno));

          if((len = nslb_make_cavisson_alert_body(buffer, ALERT_MSG_BUF_SIZE, testidx, alert_msg, ALERT_CRITICAL, ChkMon_BatchJob, check_monitor_ptr->tier_name, check_monitor_ptr->server_name, NULL, global_settings->hierarchical_view_topology_name, global_settings->alert_info->policy, global_settings->alert_info->protocol, g_cavinfo.NSAdminIP, parent_port_number, ALERT_FAIL_VALUE, global_settings->progress_secs)) < 0)
          {
             NSTL1(NULL, NULL, "Failed to make POST body, testidx = %d, alert_type = %d, alert_policy = %s, alert_msg = %s",
                         testidx, ALERT_CRITICAL, global_settings->alert_info->policy, alert_msg);
          }    
    
          NSTL1(NULL, NULL, "Going to send alert msg, tomcat port = %d, testid = %d", g_tomcat_port, testidx);
          if (ns_send_alert_ex(ALERT_CRITICAL, HTTP_METHOD_POST, NS_CONTENT_TYPE_JSON, buffer, len) > 0)
             NSTL1(NULL, NULL, "Failed to send alert msg to Dashboard");
          
          mark_check_monitor_fail(check_monitor_ptr, 1, CHECK_MONITOR_SYSERR);
        }
      }
    } 
    else if(flag_run_time_changes_called)
    {
      flag_run_time_changes_called = 0;  
      NSDL2_MON(NULL, NULL, "Got event on epoll, but it was for runtime changes.Continuing..");
      continue;
    }
    else //Timeout while waiting for check monitor results
    {
      if(errno != EINTR )
      {
        abort_pre_test_checks(cnt, nslb_strerror(errno));
        break;
      }
    }
  }
  if (rfp) fflush(rfp);
  FREE_AND_MAKE_NULL(epev, "epev", -1);
}

// This method to check result for check monitor:
// If all check monitor pass then it will pass,
// If all check monitor fail or some check monitors passed and some failed then should make connection again for all check monitor based on retry count.
inline int check_results(int check_mon_state)
{
  int test_check_id;
  int fail = 0, pass = 0;
  int total_check_mon;
  char ChkMon_BatchJob[50] = "Batch Job";
  failed_monitor[0] = '\0';
  CheckMonitorInfo *check_monitor_ptr = NULL;

  if(check_mon_state == CHECK_MONITOR_EVENT_BEFORE_TEST_IS_STARTED)
  {
    check_monitor_ptr = pre_test_check_info_ptr; 
    total_check_mon = total_pre_test_check_entries;
  }
  else if(check_mon_state == CHECK_MONITOR_EVENT_AFTER_TEST_IS_OVER)
  {
    check_monitor_ptr = check_monitor_info_ptr; 
    total_check_mon = total_check_monitors_entries;
  }
  NSDL2_MON(NULL, NULL, "Method called."); 

  for (test_check_id = 0; test_check_id < total_check_mon; test_check_id++, check_monitor_ptr++)
  {
    NSDL3_MON(NULL, NULL, "%s => %s", ChkMon_BatchJob, CheckMonitor_to_string(check_monitor_ptr, s_check_monitor_buf));
   
    //Auto server signature & java process server signature should not stop the test if failed, hence marking them passed explicitly 
    if((check_monitor_ptr->monitor_type == INTERNAL_CHECK_MON) || ((pre_test_check_retry_count == 0) && (strstr(check_monitor_ptr->check_monitor_name, "_auto"))))
    {
      pass++;
      continue;
    }

    if(check_monitor_ptr->bj_success_criteria > 0)
      strcpy(ChkMon_BatchJob, "Batch Job");
    else
      strcpy(ChkMon_BatchJob, "Check Monitor");

    if(check_monitor_ptr->status == CHECK_MONITOR_PASS)
    {
      NSDL4_MON(NULL, NULL, "%s '%s' passed", ChkMon_BatchJob, check_monitor_ptr->check_monitor_name);
      pass++;
    }
    else if(check_monitor_ptr->status == CHECK_MONITOR_STOPPED)
    {
      NSDL4_MON(NULL, NULL, "%s '%s' was stopped by netstorm and treating as pass.", ChkMon_BatchJob, check_monitor_ptr->check_monitor_name);
      pass++;
    }

    else    
    {
      char tmp[256 + 256]="\0";
      NSDL4_MON(NULL, NULL, "%s '%s' failed", ChkMon_BatchJob, check_monitor_ptr->check_monitor_name);
      sprintf(tmp, "    %s %s: %s, \n", ChkMon_BatchJob , check_monitor_ptr->check_monitor_name, error_msg[check_monitor_ptr->status]);
      strcat(failed_monitor, tmp);
      fail++;
    }
  }

  NSDL2_MON(NULL, NULL, "Total Passed = %d, Total Failed = %d", pass, fail);

  if (pass == total_check_mon) 
  {
    NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_INFO, __FILE__, (char*)__FUNCTION__,
                                         "All Check Monitor/Batch Job passed");
    //NSTL1_OUT(NULL, NULL, "All Check Monitor/Batch Job passed.\n");
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "All Check Monitor/Batch Job passed.");
    return CHECK_MONITOR_DONE_WITH_ALL_PASS;
  }
  else if(fail == total_check_mon) 
  {
    NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_CRITICAL, __FILE__, (char*)__FUNCTION__,
                                         "All Check Monitor/Batch Job failed.");
    //NSTL1_OUT(NULL, NULL, "All Check Monitor/Batch Job failed.\n");
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "All Check Monitor/Batch Job failed.");
    return CHECK_MONITOR_DONE_WITH_ALL_FAIL;
  }
 
  NSDL2_MON(NULL, NULL, "failed_monitor = %s", failed_monitor);  

  if(failed_monitor)
  { 
    char *comma_index;
    if((comma_index=rindex(failed_monitor, ',')) !=  NULL) 
        *comma_index=0;
  }

  NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_CRITICAL, __FILE__, (char*)__FUNCTION__,
                                         "Following Check Monitor/Batch Job failed: %s\n",
					  failed_monitor);
  NSTL1_OUT(NULL, NULL, "\n\nFollowing Check Monitor/Batch Job failed:\n    %s\n", failed_monitor);

  return CHECK_MONITOR_DONE_WITH_SOME_FAIL;
}

//This is main method to control Start Event 'Before test is started' and server signature
//To print on console and progress report used fprint2f
int run_pre_test_check()
{
  int ret;
  
  NSDL2_MON(NULL, NULL, "Method called");

  NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_INFO, __FILE__, (char*)__FUNCTION__,
				     "Starting before test run check monitor/batch job/server signatures ...");
  epoll_init();

  //we are allocating buffer for failed monitor as we will fill failed monitor with detail in check_result
  MY_MALLOC(failed_monitor, (sizeof(char) * total_pre_test_check_entries * (256 + 256)), "failed_monitor", -1);

  int retry_count = 1; 
  while(1)
  {
    num_pre_test_check = make_connections();
    if(num_pre_test_check > 0) //At least one connection made
    {
      NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_INFO, __FILE__, (char*)__FUNCTION__,
					 "check monitor/batch job/server signatures started. Waiting for results");
      wait_for_results();
      close_connection_all_pre_test_check();
   }

    ret = check_results(CHECK_MONITOR_EVENT_BEFORE_TEST_IS_STARTED);
    retry_count++;
    if(ret == CHECK_MONITOR_DONE_WITH_ALL_PASS) break;
    // At this point, at least once monitor failed
    if(pre_test_check_retry_count == 0)
    {
      break;
    }
    if(pre_test_check_retry_count > 0) pre_test_check_retry_count--;
    //Sleep based on retry interval. It will come here if any check monitor failed and retry count not 0

    NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_MAJOR, __FILE__, (char*)__FUNCTION__,
					 "check monitor/batch job failed. Sleeping for retry interval (%d seconds) ...",
					  pre_test_check_retry_interval);
    NSTL1_OUT(NULL, NULL, "check monitor/batch job failed. Sleeping for retry interval (%d seconds) ...\n",
                     pre_test_check_retry_interval);
    sleep(pre_test_check_retry_interval);
    NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_INFO, __FILE__, (char*)__FUNCTION__,
					 "Starting next try %d. Retry attempts left = %d",
					 retry_count, pre_test_check_retry_count);
   
    NSTL1_OUT(NULL, NULL, "Starting next try %d. Retry attempts left = %d\n",
			retry_count, pre_test_check_retry_count);
  }

  if((java_process_server_sig_enabled == 1))
  {
    free_pre_test_check(java_process_server_sig_start_indx, &pre_test_check_info_ptr[0], 0);
    //memmove and reset total entries and do not free these java process entries but free other entries
    //memmove(pre_test_check_info_ptr, pre_test_check_info_ptr + java_process_server_sig_start_indx, sizeof(CheckMonitorInfo)*(total_pre_test_check_entries - java_process_server_sig_start_indx));    
    //total_pre_test_check_entries = total_pre_test_check_entries - java_process_server_sig_start_indx; 
    //free_pre_test_check(java_process_server_sig_start_indx, &pre_test_check_info_ptr[total_pre_test_check_entries], 0);
  }
  else
    free_pre_test_check(total_pre_test_check_entries, pre_test_check_info_ptr, 1);

  if(ret == CHECK_MONITOR_DONE_WITH_ALL_PASS)
    return 0;
  save_status_of_test_run(TEST_RUN_STOPPED_DUE_TO_FAILURE_OF_CHECK_MONITOR);
  NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_WARNING, __FILE__, (char*)__FUNCTION__,
					 "No more retry attempts left for check monitor/batch job/server signatures.");
  NSTL1_OUT(NULL, NULL, "No more retry attempts left for check monitor/batch job/server signatures.\n");
  return -1;
}

/* --- END: Method used during Run Time  --- */

#ifdef TEST
/* --- START: Method uesed only for standalone testing --- */
//To compile:
//gcc -DNS_DEBUG_ON -o ns_pre_test_check ns_log.c ns_pre_test_check.c err_exit.c init_cav.c sock.c -ggdb  -I./thirdparty/libs/libxml2-2.6.30/include -D TEST -I../ -ldl -ggdb

FILE *rfp;

static void create_report_file() 
{
  char buf[1024];

  NSDL2_MON(NULL, NULL, "Method called.");

  sprintf(buf, "progress.report");
  NSDL2_MON(NULL, NULL, "Creating Report file %s", buf);

  fflush(NULL);
  if ((rfp = fopen(buf, "w")) == NULL) 
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "create_report_file() - Error in creating the report file\n");
    perror("create_report_file");
    NS_EXIT(1, "create_report_file() - Error in creating the report file");
  }
}

void fprint2f(FILE *fp1, FILE* fp2,char* format, ...) {
  va_list ap;
  int amt_written = 0;
  char buffer[4096];

  NSDL2_MISC(vptr, cptr, "Method called");
  va_start(ap, format);
  amt_written = vsnprintf(buffer, 4095, format, ap);
  va_end(ap);

  buffer[amt_written] = 0;

  if (fp1)
     fprintf(fp1, buffer);
  if (fp2)
     fprintf(fp2, buffer);
}

static void init()
{
  char buf[1024];
  char err_msg[4098];
  strcpy(buf, "MODULEMASK MON");
  if (kw_set_modulemask(buf) != 0)
  {
    NSTL1_OUT(NULL, NULL, "Invalid modulemask found");
    exit(-1);
  }

  init_ms_stamp();
  create_report_file();
}

static void read_test_file(char *file_name)
{
  char buf[MAX_LENGTH];
  char key[MAX_LENGTH] = "";
  char text[MAX_LENGTH] = "";
  FILE *fp;
  char err_msg[1024] = {0};

  fp = fopen(file_name, "r");
  while (!feof(fp)) 
  {
    if (fgets(buf, MAX_BUF_LENGTH, fp) != NULL) 
    {
       buf[strlen(buf) - 1] = '\0';  // Replace new line by Null
      if(strchr(buf, '#') || buf[0] == '\0')
        continue;
      sscanf(buf, "%s %s", key, text);

      if(strcasecmp(key, "DEBUG") == 0)
        kw_set_debug(text);
      if(strcasecmp(key, "CHECK_MONITOR") == 0)
        kw_set_check_monitor(key, buf, 0, err_msg);  //util.c, ns_monitor_profiles.c
      if(strcasecmp(key, "CHECK_MONITOR_TIMEOUT") == 0) 
        kw_set_check_monitor(key, text, 0, err_msg);  //util.c
      /*else
      { 
        NSTL1_OUT(NULL, NULL, "Error: Keyword CHECK_MONITOR not found in %s file\n", file_name);
        exit(-1);
      }*/
    }
  }
  fclose(fp);
}

// Usage pre_test_check <test file> [count] [delay]
int main(int argc, char** argv)
{
  int  count = 1;  //default count is 1
  int delay = 1;   //default delay time is 1
  int i;

  global_settings->progress_secs = 10000;

  if((argc < 2) || (argc > 4)) 
  {
    NSTL1_OUT(NULL, NULL, "Error: Check arguments for %s\n", argv[0]);
    printf("Usage: %s <test_file> [count] [delay]\n", argv[0]);
    exit(-1);
  }

  init();
  
  count = atoi(argv[2]);
  delay = atoi(argv[3]);

  for(i = 0; i < count; i++)
  {
   //printf("Starting check monitor. Iteration count = %d\n", i + 1);
   MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Starting check monitor. Iteration count = %d", i + 1);
    //Parse keywords and add Check Monitor info into table
    read_test_file(argv[1]);  

    if(total_pre_test_check_entries != 0)
    {
      if(run_pre_test_check() == 0)      //ns_parent.c
        //printf("Check Monitor passed\n");
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Check Monitor passed");
      else
        //printf("Check Monitor failed\n");
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Check Monitor failed");
    }
    if(delay)
      sleep(delay);
  }
  //printf("\nEnd of check monitoring\n");
  MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "End of check monitoring");
  if (rfp) fclose(rfp);
}

/* --- END: Method uesed only for standalone testing --- */
#endif
