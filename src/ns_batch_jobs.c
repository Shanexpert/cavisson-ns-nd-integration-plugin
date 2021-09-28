/******************************************************************
 * Name    : ns_batch_jobs.c 
 * Author  : 
 * Purpose : This file contains methods related to
             parsing keyword, setup the connection with Create Server
             at run time for batch jobs
 * Note:
 * Modification History:
 * 30/05/2012 - Modification Version
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

#include "v1/topolib_structures.h"
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
#include "ns_user_monitor.h"
#include "nslb_cav_conf.h"
#include "ns_gdf.h"
#include "ns_server_admin_utils.h"
#include "ns_check_monitor.h"
#include "ns_mon_log.h"
#include "ns_pre_test_check.h"
#include "ns_string.h"
#include "ns_batch_jobs.h"
#include "ns_exit.h"

char keyword[MAX_DATA_LINE_LENGTH];

TopoInfo *topo_info;
static void batch_job_usage(char *error)
{
  NSDL2_MON(NULL, NULL, "Method called");
  fprintf(stderr, "\n%s", error);
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "BATCH_JOB|<Server Name>|<Batch Job Name>|<Program Name>|<Start Event>|<Start Phase Name>|<Repeat Option {Run Periodic (1) | Never (2)}>|<Periodicity (HH:MM:SS)>|<End Event>|[<Max Count> | <End Phase Name>]|<Success Criteria>|<Log File Name>|<Command Name>|<Search Pattern> \n");
  fprintf(stderr, "Where \n");
  fprintf(stderr, "Server Name:\n server name or server IP\n");
  fprintf(stderr, "Batch Job Name:\n job name\n");
  fprintf(stderr, "Program Name:\n name of program with or wihtout path along with its required arguments\n");
  fprintf(stderr, "Start Event:\n  1 - 'Before test is started'\n  2 - 'Start of Test'\n  3 - 'On Start of the Phase'\n  90 - 'After test is Over'\n");
  fprintf(stderr, "Start Phase Name:\n  If From Event 'On Start of the Phase (3)' then use phase name\n  Except 'On Start of the Phase (3)' use NA\n");
  fprintf(stderr, "Repeat Option:\n  NA - if Frequency 'Never (2)'\n  If Frequency 'Run Periodic (1)' then use periodicity in HH:MM:SS\n");
  fprintf(stderr, "End Event:\n  NA - if Frequency 'Never (2)'\n  If Frequency 'Run Periodic (1)' then use:\n    Till test completion (1)\n    Complete specified executions (2)\n    Till Completion of Phase (3)\n");
  fprintf(stderr, "Max Count:\n  NA - if Frequency 'Never (2)'\n  If Frequency 'Run Periodically (1)' then:\n    NA - if 'End Event 1'\n    Run for specified Max Count if 'End Event 2'\n    Run for specified End Phase Name if 'End Event 3'\n");
  fprintf(stderr, "Success Criteria:\n  1 - 'Exit status'\n  2 - 'Search some string in output'\n  3 - 'Check in log file'\n  4 - 'Execute another command to check status'\n");
  fprintf(stderr, "Log File Name:\n name of the log file to search any specific pattern\n");
  fprintf(stderr, "Command Name:\n another command to check the status\n");
  fprintf(stderr, "Search Pattern:\n pattern to be searched in Log File\n");
  fprintf(stderr, "Example:\n");
  fprintf(stderr, "BATCH_JOB|192.168.1.66|batchtest|/tmp/samplebatch.sh|1|NA|2|NA|NA|NA|1|NA|NA|NA|NA|NA|NA|NA|NA|NA\n\n");
  NS_EXIT(-1, "%s\nUsage: BATCH_JOB|<Server Name>|<Batch Job Name>|<Program Name>|<Start Event>|<Start Phase Name>|<Repeat Option {Run Periodic (1) | Never (2)}>|<Periodicity (HH:MM:SS)>|<End Event>|[<Max Count> | <End Phase Name>]|<Success Criteria>|<Log File Name>|<Command Name>|<Search Pattern>", error);
}

//Need to give only json supoort after auto_monitors enhancement
//While applying batch job via file, we need to mention Tier and Server

int parse_job_batch(char *buf, int runtime_flag, char *err_msg, char *delimiter, JSON_info *json_info_ptr)
{
  char key[MAX_LENGTH] = "";
  char tmp[MAX_LENGTH] = "";
  char batch_job_name[MAX_LENGTH] = "";
  int from_event = CHECK_MONITOR_EVENT_BEFORE_TEST_IS_STARTED;
  char start_phase_name[MAX_LENGTH] = "NA";
  int frequency = RUN_EVERY_TIME_OPTION;
  char origin_cmon_and_ip_port[MAX_LENGTH] = "NS";
  char server_name[MAX_LENGTH] = {0};
  char origin_cmon[MAX_LENGTH] = {0}; //For Heroku
  char periodicity[MAX_LENGTH] = "NA";
  int check_periodicity = 0;
  char end_event[MAX_LENGTH] = "NA";
  char max_count_or_phase_name[MAX_LENGTH] = "NA";
  char pgm_path[MAX_LENGTH] = "";
  char pgm_args[MAX_LENGTH] = "";
  int check_monitor_row = 0;
  char temp_usage_buf[MAX_DATA_LINE_LENGTH];
  char tmp_buf[MAX_DATA_LINE_LENGTH];
  int access = RUN_LOCAL_ACCESS;
  char rem_ip[1024] = "NA";
  char rem_username[256] = "NA";
  char rem_password[256] = "NA";
  int ret;
  int total_flds;
  char *field[20];
  int success_criteria;
  char log_file_name[MAX_LENGTH] = "NA";
  char cmd_name[MAX_LENGTH] = "NA";
  char search_pattern[MAX_LENGTH] = "NA";
  int server_index;
  int tier_idx;
  
  CheckMonitorInfo *check_monitor_ptr;
  
  NSDL2_MON(NULL, NULL, "Method called, keyword = %s, buf = %s", keyword, buf);


  // BATCH_JOB|ServerName|BatchJobName|ProgramName|StartEvent|StartPhaseName|RepeatOption|Periodicity|EndEvent|EndPhaseName|SuccessCriteria|LogFileName|CmdName|SearchPattern|NA|NA|NA|NA|NA|NA


  total_flds = get_tokens(buf, field, delimiter, 20);

  strcpy(batch_job_name, field[2]); // here to print batch_job_usage mentioned below properly

  if(total_flds < 20)
  {
    sprintf(temp_usage_buf, "Error: Too few arguments for Batch Job '%s'.\n", batch_job_name);
    batch_job_usage(temp_usage_buf);
  }

  NSDL2_MON(NULL, NULL, "total_flds = %d", total_flds);

  strcpy(key, field[0]);
  //strcpy(batch_job_name, field[2]);
  strcpy(origin_cmon_and_ip_port, field[1]);
  //sprintf(batch_job_name, "%s%c%s",origin_cmon_and_ip_port, global_settings->hierarchical_view_vector_separator,field[2]);
  sprintf(tmp, "%s", field[4]);
  from_event = atoi(tmp);

  strcpy(start_phase_name, field[5]);

  sprintf(tmp, "%s", field[6]);
  frequency = atoi(tmp);

  strcpy(periodicity, field[7]);
  strcpy(end_event, field[8]);
  strcpy(max_count_or_phase_name, field[9]);
  strcpy(pgm_path, field[3]);
  
  sprintf(tmp, "%s", field[10]);
  success_criteria = atoi(tmp);
 
  strcpy(log_file_name, field[11]);
  strcpy(cmd_name, field[12]);
  strcpy(search_pattern, field[13]); 
  if(json_info_ptr)
  {
    tier_idx=json_info_ptr->tier_idx;
    server_index=json_info_ptr->server_index;
  }
   
  else
  {
    if(find_tier_idx_and_server_idx(origin_cmon_and_ip_port, server_name, origin_cmon, global_settings->hierarchical_view_vector_separator, hpd_port, &server_index,&tier_idx) == -1)
    {
      NS_EXIT(-1, "Server (%S) not present in topolgy, for monitor (%s).", server_name, pgm_path);
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
    strcpy(rem_username, topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_index].server_ptr->username);
    strcpy(rem_password, topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_index].server_ptr->password);
  }
   
  
  //here we batch_job_name is like Tier>Server>batch_job_name for uniqueness

  sprintf(batch_job_name, "%s%c%s%c%s",topo_info[topo_idx].topo_tier_info[tier_idx].tier_name, global_settings->hierarchical_view_vector_separator,topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_index].server_disp_name,global_settings->hierarchical_view_vector_separator,field[2]);
  
  //when any monitor is applied with any set in specific server ,then it makes string and checks in hash table if monitor apllied on any server of a tier
  if(json_info_ptr && json_info_ptr->any_server)
  {
    if(json_info_ptr->instance_name == NULL)
       sprintf(tmp_buf, "%s%c%s", json_info_ptr->mon_name, global_settings->hierarchical_view_vector_separator, topo_info[topo_idx].topo_tier_info[tier_idx].tier_name);
    else
       sprintf(tmp_buf, "%s%c%s%c%s", json_info_ptr->mon_name, global_settings->hierarchical_view_vector_separator, 
                                      json_info_ptr->instance_name, global_settings->hierarchical_view_vector_separator, 
                                      topo_info[topo_idx].topo_tier_info[tier_idx].tier_name);

    NSDL1_MON(NULL, NULL, "check mon tmp_buf '%s' for making entry in hash table for server '%s'\n", tmp_buf, server_name);

    if(init_and_check_if_mon_applied(tmp_buf))
    {
      sprintf(err_msg, "Monitor '%s' can not be applied on server '%s' as it is already applied on any other sever on same tier '%s'", 
      json_info_ptr->mon_name, topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_index].server_disp_name, 
      topo_info[topo_idx].topo_tier_info[tier_idx].tier_name);

      NSDL1_MON(NULL, NULL, "%s\n", err_msg);
      return -1;
    }

  }

 if((from_event != CHECK_MONITOR_EVENT_BEFORE_TEST_IS_STARTED) && (from_event != CHECK_MONITOR_EVENT_START_OF_TEST) && (from_event != CHECK_MONITOR_EVENT_START_OF_PHASE) && (from_event != CHECK_MONITOR_EVENT_AFTER_TEST_IS_OVER))
  {
    sprintf(temp_usage_buf, "Error: Wrong From Event for Batch Job '%s'.", batch_job_name);
    batch_job_usage(temp_usage_buf);
    NS_EXIT(-1, "Error: Wrong From Event for Batch Job '%s'.", batch_job_name);
  }

  if(frequency != RUN_EVERY_TIME_OPTION && frequency != RUN_ONLY_ONCE_OPTION)
  {
    sprintf(temp_usage_buf, "Error: Wrong Frequency for Batch Job  '%s', should be Run Periodically(1) or Never(2)\n", batch_job_name);
    batch_job_usage(temp_usage_buf);
  }
    
  if(from_event != CHECK_MONITOR_EVENT_START_OF_PHASE)
  {
    if(strcmp(start_phase_name, "NA"))
    {
      sprintf(temp_usage_buf, "Error: Wrong Start Phase name for Btach Job '%s'\n", batch_job_name);
      batch_job_usage(temp_usage_buf);
    }
  }

  if(!strcmp(periodicity, "NA"))
    check_periodicity = -1;
  else
    check_periodicity = get_time_from_format(periodicity);

  if(from_event == CHECK_MONITOR_EVENT_BEFORE_TEST_IS_STARTED)
  {
    if(!(ret = check_duplicate_monitor_name(pre_test_check_info_ptr, total_pre_test_check_entries, batch_job_name, runtime_flag, err_msg,tier_idx,server_index)))
    {
      if(create_table_entry(&check_monitor_row, &total_pre_test_check_entries, &max_pre_test_check_entries, (char **)&pre_test_check_info_ptr, sizeof(CheckMonitorInfo), "Check Monitor Table") == -1)
      {
        NS_EXIT(-1, "Could not create table entry for Check Monitor Table");
      }

      NSDL1_MON(NULL, NULL, "check_monitor_row = %d, batch_job_name = %s, from_event = %d, start_phase_name = %s, frequency = %d, periodicity = %d, max_count_or_phase_name = %s, server_name = %s, rem_ip = %s, rem_username = %s, rem_password = %s, pgm_path = %s, pgm_args = %s, origin_cmon = %s", check_monitor_row, batch_job_name, from_event, start_phase_name, frequency, check_periodicity, max_count_or_phase_name, server_name, rem_ip, rem_username, rem_password, pgm_path, pgm_args, origin_cmon);
      check_monitor_ptr = pre_test_check_info_ptr + check_monitor_row;

    }
  }
  else
  {
    if(!(ret = check_duplicate_monitor_name(check_monitor_info_ptr, total_check_monitors_entries, batch_job_name, runtime_flag, err_msg,tier_idx,server_index)))
    {
      if(create_table_entry(&check_monitor_row, &total_check_monitors_entries, &max_check_monitors_entries, (char **)&check_monitor_info_ptr, sizeof(CheckMonitorInfo), "Check Monitor Table") == -1)
      {
        NS_EXIT(-1, "Could not create table entry for Check Monitor Table");
      }
      NSDL1_MON(NULL, NULL, "check_monitor_row = %d, batch_job_name = %s, from_event = %d, start_phase_name = %s, frequency = %d, periodicity = %d, max_count_or_phase_name = %s, server_name = %s, rem_ip = %s, rem_username = %s, rem_password = %s, pgm_path = %s, pgm_args = %s, origin_cmon = %s", check_monitor_row, batch_job_name, from_event, start_phase_name, frequency, check_periodicity, max_count_or_phase_name, server_name, rem_ip, rem_username, rem_password, pgm_path, pgm_args, origin_cmon);
      check_monitor_ptr = check_monitor_info_ptr + check_monitor_row;

    }
  }

  add_check_monitor(check_monitor_ptr, batch_job_name, from_event, start_phase_name, pgm_path, server_name, access, rem_ip, rem_username, rem_password, pgm_args, frequency, check_periodicity, end_event, max_count_or_phase_name, CHECK_MON_IS_BATCH_JOB, origin_cmon, 0, err_msg, server_index, json_info_ptr,tier_idx);

  check_monitor_ptr->bj_success_criteria = success_criteria;
 
  // Now we set batch jobs related fields
  switch(check_monitor_ptr->bj_success_criteria)
  {
    case 1:
      check_monitor_ptr->bj_success_criteria = success_criteria;
      break;
    case 2:
      check_monitor_ptr->bj_success_criteria = success_criteria;

      MY_MALLOC(check_monitor_ptr->bj_search_pattern, (strlen(search_pattern) + 1), "Search Pattern", -1);
      strcpy(check_monitor_ptr->bj_search_pattern, search_pattern);
      break;
    case 3:
      check_monitor_ptr->bj_success_criteria = success_criteria;
     
      MY_MALLOC(check_monitor_ptr->bj_log_file, (strlen(log_file_name) + 1), "Log File Name", -1);
      strcpy(check_monitor_ptr->bj_log_file, log_file_name);

      MY_MALLOC(check_monitor_ptr->bj_search_pattern, (strlen(search_pattern) + 1), "Search Pattern", -1);
      strcpy(check_monitor_ptr->bj_search_pattern, search_pattern);
      break;

    case 4:
      check_monitor_ptr->bj_success_criteria = success_criteria;
   
      MY_MALLOC(check_monitor_ptr->bj_check_cmd, (strlen(cmd_name) + 1), "Command Name", -1);
      strcpy(check_monitor_ptr->bj_check_cmd, cmd_name);
      break; 
   
  }
  return 0;
}

static void read_kw_from_batch_job_group(char *file_name, FILE* fp, int runtime_flag, char *err_msg)
{ 
  char buf[MAX_CONF_LINE_LENGTH + 1];

  NSDL2_SCHEDULE(NULL, NULL, "Method called, Batch Group Name = %s", file_name);
  while (nslb_fgets(buf, MAX_CONF_LINE_LENGTH, fp, 0) != NULL)
  {
    buf[strlen(buf)-1] = '\0';
    if((buf[0] == '#') || (buf[0] == '\0'))
      continue;
       if (strncmp(buf, "BATCH_JOB|", strlen("BATCH_JOB|")) != 0)
         continue;
       
       parse_job_batch(buf, runtime_flag, err_msg, "|", NULL);

  }//End while loop
}


// BATCH_JOB_GROUP  <batch group name>
void kw_set_batch_job_group(char *batch_group_name, int num, int runtime_flag, char *err_msg)
{ 
  FILE *fp;
  char wdir[1024];
  char file_name[2024];
  struct stat s;

  if (num != 2)
  {
    NS_EXIT(-1, "read_keywords(): Need ONE fields after key BATCH_JOB_GROUP");
  }

  if (getenv("NS_WDIR") != NULL)
  {
    strcpy(wdir, getenv("NS_WDIR"));

    /*  In case of hierarchical view, batch_job should be kept in work/batch_jobs/topology_name/ dir
     *  If batch_job is not found here, NS will find in work/batch_job dir  */
    sprintf(file_name, "%s/batch_jobs/%s/%s.bjobs", wdir, global_settings->hierarchical_view_topology_name, batch_group_name);
    if(stat(file_name, &s) != 0)
      sprintf(file_name, "%s/batch_jobs/%s.bjobs", wdir, batch_group_name);
  }
  else
  {
    NSDL2_SCHEDULE(NULL, NULL, "NS_WDIR env variable is not set. Setting it to default value /home/cavisson/work/");
    sprintf(file_name, "/home/cavisson/work/batch_jobs/%s.bjobs", batch_group_name);
  }
  NSDL3_SCHEDULE(NULL, NULL, "Reading keywords from %s file", file_name);

  fp = fopen(file_name, "r");
  if (fp == NULL)
  {
    error_log("Error in opening file %s", file_name);
    error_log("Error: BATCH_GROUP '%s.bjobs' is not present in %s/batch_jobs directory.", batch_group_name, wdir);
    NS_EXIT(-1, "Error: BATCH_GROUP '%s.bjobs' is not present in %s/batch_jobs directory.", batch_group_name, wdir);
  }
  read_kw_from_batch_job_group(file_name, fp, runtime_flag, err_msg);
  fclose(fp);
}



