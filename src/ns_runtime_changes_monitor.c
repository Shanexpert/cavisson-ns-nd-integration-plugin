/************************************************************************************************************************
Program Name: ns_runtime_changes_monitor.c
Purpose     : This file contain all changes regarding to monitor at run time.
Flow        : 1. Function parse_runtime_changes() of file  ns_runtime_changes.c will read will read file runtime_changes.c              onf 

              2. File runtime_changes.conf containe all desired changes which will have to done on runtime.It contains fol              lowing keywords related to monitor
               (i)START_MONITOR <server_option> <group_id> <vector_name>
               (ii)STOP_MONITOR <server_option> <group_id> <vector_name>
               (iii)RESTART_MONITOR <server_option> <group_id> <vector_name> 
               (iv)ADD_MONITOR....
               (v)DEL_MONITOR....
            
              where server_option: (a)OneMonitorOfOneServer
             			   (b)OneMonitorVectorOfAllServers
                                   (c)OneMonitorGroupOfOneServer
                                   (d)OneMonitorGroupOfAllServers
				   (e)AllMonitorsOfOneServer  
				   (f)AllMonitorsOfAllServers  
                    group_id     : represent group id of that monitor ex: group id for cm_ps_data is 10015
                                   (a)Specified group id ex: 10015
                                   (b)ALL (means All groups)
                                   (c)NA (means Not Applicable)           
                    vector_name  : represent vector name of that monitor ex: NSVector
                                   (a)Specified vector name ex: NSVector
                                   (b)ALL (means All groups)
                                   (c)NA (means Not Applicable) 
              3. Parse the above keywords.All the parsing function is here.
              4. After parsing take appropriate action. 
Return      : 0 success
             -1 error       
Programed By: Manish Kumar Mishra
Date        : Wed Nov  9 10:19:27 IST 2011
Issues      :
*************************************************************************************************************************/

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include<netinet/in.h>
#include <regex.h>

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
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#include "ns_runtime_changes_monitor.h"
#include "ns_monitor_profiles.h"
#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "nslb_util.h"
#include "init_cav.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"

#include "nslb_cav_conf.h"
#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "util.h"
#include "tmr.h"
#include "timing.h"
#include "ns_gdf.h"
#include "nslb_sock.h"
#include "ns_msg_com_util.h"
#include "ns_log.h"
#include "ns_custom_monitor.h"
#include "ns_check_monitor.h"
#include "ns_pre_test_check.h"
#include "ns_alloc.h"
#include "ns_schedule_phases.h"
#include "ns_child_msg_com.h"
#include "ns_percentile.h"
#include "ns_dynamic_vector_monitor.h"
#include "ns_event_log.h"
#include "ns_server_admin_utils.h"
#include "ns_string.h"
#include "ns_event_id.h"
#include <sys/socket.h>
#include <errno.h>
#include "init_cav.h"
#include "ns_trace_level.h"
#include <v1/topolib_structures.h>
#include "nslb_get_norm_obj_id.h"
#include "ns_coherence_nid_table.h"
#include "ns_ndc.h"
#include "ns_ndc_outbound.h"
#include "netstorm.h"
#include "ns_trans.h"
#include "netomni/src/core/ni_scenario_distribution.h"
#include "nslb_log.h"
#include "nslb_mon_registration_con.h"
#include "ns_appliance_health_monitor.h"
#include "ns_monitor_metric_priority.h"
#include "ns_runtime_changes.h"
#include "ns_svr_ip_normalization.h"
#include "ns_trans_normalization.h"
#include "ns_http_status_codes.h"
#include "ns_error_msg.h"

#include "ns_socket.h"
#include "ns_monitor_init.h"


#define MAX_NAME_SIZE                 1024
#define CHECK_MONITOR_STOPPED         9 // This means check monitor is stopped at the end of test

extern int is_rtc_applied_for_dyn_objs();
void dvm_handle_err_case(DynamicVectorMonitorInfo *dynamic_vector_monitor_ptr);
extern int check_if_cm_vector(const void *cm1, const void *cm2);
extern void initialize_dyn_mon_vector_groups(CM_info *dyn_cm_start_ptr, int start_indx, int total_entries);
int create_diff_file();
int set_counters_for_vectors();
NormObjKey *specific_server_id_key;

int total_vectors_with_h_priority;
int total_vectors_with_m_priority;
int total_vectors_with_l_priority;

#define RTC_MONITOR_LOG_TOPO(keyword, server_opt, command, vector_name, server_ip, grp_name, rtc_msg_buf, brdcrmb, all_or_some) \
{ \
  char *rtc_msg_ptr; \
  int  len = strlen(rtc_msg_buf); \
  rtc_msg_ptr = rtc_msg_buf + len; \
  if(!strcasecmp(server_opt, "AllMon")) \
  { \
    sprintf(rtc_msg_ptr, "%s monitors at '%s' %s successfully.", all_or_some, brdcrmb, command); \
  } \
  else if(!strcasecmp(server_opt, "VectorMon")) \
  { \
    sprintf(rtc_msg_ptr, "%s monitors with vector name '%s' at '%s' %s successfully.", all_or_some, vector_name, brdcrmb, command); \
  } \
  else if(!strcasecmp(server_opt, "GroupMon")) \
  { \
    sprintf(rtc_msg_ptr, "%s monitors of group '%s' at '%s' %s successfully.", all_or_some, grp_name, brdcrmb, command); \
  } \
  NSDL3_MON(NULL, NULL, "%s", rtc_msg_ptr);\
}

//Added last argument "custom_mon_id" because we need this id in function send_msg_to_create_server_for_one_mon.
//Will be using this id as MON_ID in SendMsg buffer.
static int start_monitor(char *keyword, char *server_opt, CM_info *cus_mon_ptr, char *err_msg, int custom_mon_id)
{
  int ret;
  int RetValCntrlConn;

  NSDL3_MON(NULL, NULL, "Method Called. cus_mon_ptr->fd = %d", cus_mon_ptr->fd);

  if(cus_mon_ptr->fd < 0)
  {
    //make connection for one custom monitor
    cm_make_nb_conn(cus_mon_ptr, 0);

    //selecting fd
    //TM: this already added monitor so gp_info_index will already be set--done 
    if((ret = add_select_custom_monitor_one_mon(cus_mon_ptr)) < 0)
    {
      sprintf(err_msg, "Error in selecting fd for making connection with server '%s' for '%s' with vector name '%s'.", cus_mon_ptr->cs_ip, get_gdf_group_name(cus_mon_ptr->gp_info_index), cus_mon_ptr->monitor_name);

      return RUNTIME_ERROR; 
    }
  }
  else
  {
    if(!strcmp(keyword, "START_MONITOR"))
    {
      if(!(strcasecmp(server_opt, "OneMonitorOfOneServer")))
      {
        sprintf(err_msg, "Monitor of group '%s' with vector name '%s' on server '%s' is already running.", get_gdf_group_name(cus_mon_ptr->gp_info_index), cus_mon_ptr->monitor_name, cus_mon_ptr->cs_ip);
      }
      else if(!(strcasecmp(server_opt, "OneMonitorVectorOfAllServers")))
      {
        sprintf(err_msg, "All or some  monitors with vector name %s on all servers is already running.", cus_mon_ptr->monitor_name);
      }
      else if(!(strcasecmp(server_opt, "OneMonitorGroupOfOneServer")))
      {
        sprintf(err_msg, "All or some monitors of  group '%s' on server %s are already running.", get_gdf_group_name(cus_mon_ptr->gp_info_index), cus_mon_ptr->cs_ip);
      }
      else if(!(strcasecmp(server_opt, "OneMonitorGroupOfAllServers")))
      {
        sprintf(err_msg, "All or some monitors of group '%s' on all servers are already running.", get_gdf_group_name(cus_mon_ptr->gp_info_index));
      }
      else if(!(strcasecmp(server_opt, "AllMonitorsOfOneServer")))
      {
        sprintf(err_msg, "All or some monitors on server '%s' are already running.", cus_mon_ptr->cs_ip);
      }
      else
      {
        sprintf(err_msg, "All or some monitors on all servers are already running.");
      }

      NSDL3_MON(NULL, NULL, "err_msg = %s", err_msg);
 
      return RUNTIME_ERROR;
    }
  }

  NSDL3_MON(NULL, NULL, "Calling: check_and_open_nb_cntrl_connection()");

  /* To make control connection again(on start of monitor through gui) */
  RetValCntrlConn = check_and_open_nb_cntrl_connection(cus_mon_ptr, cus_mon_ptr->cs_ip, cus_mon_ptr->cs_port, NULL);
  if(RetValCntrlConn < 0)
    return RUNTIME_ERROR;

  NSDL3_MON(NULL, NULL, "Method End.");
  return RUNTIME_SUCCESS;
}


static int stop_monitor(char *keyword, char *server_opt, CM_info *cus_mon_ptr, char *err_msg, char *brdcrmb)
{
  int ret;
  char *msg = NULL;
  int reason;

  NSDL1_MON(NULL, NULL, "Method Called. cus_mon_ptr->fd = %d", cus_mon_ptr->fd);

  if(!(strcasecmp(keyword, "STOP_MONITOR")) || !(strcasecmp(keyword, "RESTART_MONITOR")))
  {
    if(cus_mon_ptr->conn_state == CM_STOPPED) {
      sprintf(err_msg, "All/Some monitors are already stopped");
      return MON_ALREADY_STOPPED;
    }
    msg = "stopped";
    reason = MON_STOP_ON_REQUEST;
  }
  else
  {
    msg = "deleted";
    reason = MON_DELETE_ON_REQUEST;
  }

  if(cus_mon_ptr->fd >= 0 || (cus_mon_ptr->flags & OUTBOUND_ENABLED))
  {
    if(cus_mon_ptr->any_server)
      mj_delete_specific_server_hash_entry(cus_mon_ptr);

    if(cus_mon_ptr->conn_state != CM_DELETED)
    {
      if((ret = stop_one_custom_monitor(cus_mon_ptr, reason)) < 0)
      {
        sprintf(err_msg, "Error in sending end_monitor message for %s with"
                         "vector name '%s' to cav mon server '%s'.", 
                          get_gdf_group_name(cus_mon_ptr->gp_info_index), cus_mon_ptr->monitor_name, cus_mon_ptr->cs_ip);      
        return RUNTIME_ERROR;
      }
    }
  }
  else if ((cus_mon_ptr->conn_state != CM_DELETED) && (!(strcasecmp(keyword, "DELETE_MONITOR"))))
  {
    //this check is for runtime changes when we use any keyword in specific-server
    if(cus_mon_ptr->any_server)
      mj_delete_specific_server_hash_entry(cus_mon_ptr);

    handle_monitor_stop(cus_mon_ptr, reason);    //calling here because of the issue mentioned in Bug 16229
  }
  else
  { 
    RTC_MONITOR_LOG_TOPO(keyword, server_opt, msg, cus_mon_ptr->monitor_name, cus_mon_ptr->cs_ip, get_gdf_group_name(cus_mon_ptr->gp_info_index), err_msg, brdcrmb, "All");
 
    NSDL3_MON(NULL, NULL, "err_msg = %s", err_msg);
    return RUNTIME_ERROR;
  }

  NSDL3_MON(NULL, NULL, "after stoping cus_mon_ptr->fd = %d", cus_mon_ptr->fd);
  MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Monitor stopped. Name = %s, gdf = %s", cus_mon_ptr->monitor_name, cus_mon_ptr->gdf_name);
  return RUNTIME_SUCCESS;
}


//static void get_server_ip_by_group_id_and_vector_name(char *group_id, char *vector_name, char *server_ip)
//Removing this function to remove warning. If anyone need this they can find it upto build of 4.1.6

#define COMPARE_VECTOR_OR_BREADCRUMB(name, breadcrumb_name, vector_name, is_mon_breadcrumb_set) \
{ \
  NSDL2_MON(NULL, NULL, "name %s, breadcrumb_name %s vector_name %s", name, breadcrumb_name, vector_name); \
  \
  if(is_mon_breadcrumb_set) \
  { \
    if(!strcasecmp(name, breadcrumb_name)) \
    { \
      return 1; \
    } \
  } \
  else \
  { \
    if(!strcasecmp(name, vector_name)) \
    { \
      return 1; \
    } \
  } \
}

//This will match the vector name of given monitor from exiting vector name
// Returns: 1  matched
//          0  not matched
static int match_vector_name(char *vector_name, CM_info *cus_mon_ptr, char *server_opt, int group_id_pass_on_runtime, int grp_id)
{
  int i;
  int num_dynamic_vector;
  CM_vector_info *vector_list = cus_mon_ptr->vector_list;

  NSDL2_MON(NULL, NULL, "Method called, vector_name = %s, cus_mon_ptr->monitor_name = %s, cus_mon_ptr = %p", vector_name, cus_mon_ptr->monitor_name, cus_mon_ptr);
 
  NSDL2_MON(NULL, NULL, "is_dynamic = %d", monitor_list_ptr[cus_mon_ptr->monitor_list_idx].is_dynamic);
  //if monitor is custom monitor
  if (!(monitor_list_ptr[cus_mon_ptr->monitor_list_idx].is_dynamic))
  {
    COMPARE_VECTOR_OR_BREADCRUMB(vector_name, cus_mon_ptr->vector_list[0].mon_breadcrumb, cus_mon_ptr->vector_list[0].vector_name, (cus_mon_ptr->vector_list[0].flags & MON_BREADCRUMB_SET))
  }
  else
  {
    //if monitor is dynamic monitor
    if((strcasecmp(server_opt, "GroupMon") == 0))
    {
      if(group_id_pass_on_runtime == grp_id)
      {
        if(!strcmp(cus_mon_ptr->monitor_name,vector_name))
        { 
          NSDL2_MON(NULL, NULL, "Successful return from match vector name function mon_breadcrumb = %s", vector_name);
          return 1;
        }
         else
        { 
          NSDL2_MON(NULL, NULL, "Unsuccessful return from match vector name function mon_breadcrumb = %s", vector_name);
          return 0;
        }
      }
    }
    else
    {
      num_dynamic_vector = cus_mon_ptr->total_vectors; 
      NSDL2_MON(NULL, NULL, "num_dynamic_vector = %d", num_dynamic_vector);
      for (i = 0; i < num_dynamic_vector; i++ )
      {
        NSDL2_MON(NULL, NULL, "i = %d, cus_mon_ptr->total_vectors = %d"
                               "vector_name = %s,vector_list[i].vector_name = %s"
                                " ", i, cus_mon_ptr->total_vectors, vector_name, vector_list[i].vector_name);
        
        COMPARE_VECTOR_OR_BREADCRUMB(vector_name, vector_list[i].mon_breadcrumb, vector_list[i].vector_name, (vector_list[i].flags & MON_BREADCRUMB_SET))
      }
    }
  }
  NSDL2_MON(NULL, NULL, "Method End.");
  return 0;
}


//This will search for passed breadcrumb in structure
// Returns: 1  found
//          0  not found
static int match_passed_breadcrumb(char *mon_breadcrumb, CM_info *cus_mon_ptr)
{
  int i = 0;
  int num_dynamic_vector = cus_mon_ptr->total_vectors;
  CM_vector_info *vector_list = cus_mon_ptr->vector_list;

  NSDL2_MON(NULL, NULL, "Method called, mon_breadcrumb = %s, cus_mon_ptr = %p, num_dynamic_vector = %d", 
                         mon_breadcrumb, cus_mon_ptr, num_dynamic_vector);
 
  for (i = 0; i < num_dynamic_vector; i++ )
  {
    NSDL2_MON(NULL, NULL, "i = %d, vector_list[i].mon_breadcrumb = %s", i, vector_list[i].mon_breadcrumb);

    if ((vector_list[i].flags & MON_BREADCRUMB_SET)) //if breadcrumb is set
    {
      if(!strcasecmp(mon_breadcrumb, vector_list[i].mon_breadcrumb)) //compare
        return 1; 
    }
  }

  NSDL2_MON(NULL, NULL, "Method End.");
  return 0;
}

/*  This functions handles DELETE_MONITOR in case of hierarchical view
 *  Syntax: DELETE_MONITOR <ServerOption> <MonitorGroupId> <VectorName> < BreadcrumbPath> <GdfName>
 *  ServerOption may have one of following values 
      1. AllMon - In case of All monitors to be deleted
      2. VectorMon - In case of Monitors having matching vector to be deleted
      3. GroupMon  - In case of Monitors having matching group to be deleted

 *  MonitorGroup is to be provided in case of GroupMon only, NA otherwise.
 *  Vector is to be provided in case of VectorMon only, NA otherwise.
 *  Breadcrumb may have one of following
      1. AllTier - if monitor to be removed from all tiers
      2. TierName -if monitor to be removed from given tier
      3. TierName>ServerName - if monitor to be removed from given server
      4. TierName>ServerName>InstanceName - if monitor to be removed from given instance
 *  Note - Here character '>' must be replaced with hierarichal view separator used in test run.
 *  GdfName id provided in case of DELETE_MONITOR only otherwise NA. 
 */

/*
VectorMon was used in previous design where the individual CMInfo has the vector. So if we want to remove the monitor
we need to match with mon breadcrumb of the CMInfo entry
*/
static int runtime_monitors_delete_topo(char *keyword, char *server_opt, char *mon_breadcrumb, char *group_idx, char *vector_name, char *gdf_name,char *err_msg)
{
  int mon_id, vector_idx = 0, ret = 0, group_id_pass_on_runtime= -1, grp_id;
  CM_info *cus_mon_ptr = NULL;
  CM_vector_info *vector_list = NULL;
  char command[MAX_DATA_LINE_LENGTH], server[MAX_DATA_LINE_LENGTH] = "NA", grp_name[MAX_DATA_LINE_LENGTH] = "NA";
  char server_ip[MAX_DATA_LINE_LENGTH] = "NA";
  int done = 0, execute_flag = 0, not_done = 0;
  char *ptr;
  char local_brdcrumb_buf[2*MAX_DATA_LINE_LENGTH + 1];
  local_brdcrumb_buf[0] = '\0';
  //char no_vectors;

  NSDL2_MON(NULL, NULL, "Method called, keyword = %s, server_opt = %s, mon_breadcrumb = %s, group_idx = %s, gdf_name = %s, vector_name = %s", keyword, server_opt, mon_breadcrumb, group_idx, gdf_name, vector_name);
  
  
  //set breadcrumb null; breadcrumb check will have no effect in this case
  if(strcasecmp(mon_breadcrumb, "AllTier") == 0)
    mon_breadcrumb[0] = '\0';

  //here we are looping monitor list ptr
  for(mon_id = 0; mon_id < total_monitor_list_entries; mon_id++)
  {
    execute_flag =0;
    cus_mon_ptr = monitor_list_ptr[mon_id].cm_info_mon_conn_ptr;
    //here we are continuing when there is no vector present in monitor
    if(cus_mon_ptr->total_vectors <= 0)
      continue;
      
    vector_list = cus_mon_ptr->vector_list;
    NSDL3_MON(NULL, NULL, "mon_id = %d, cus_mon_ptr = %p", mon_id, cus_mon_ptr);
    ret = 0;

    if((strcasecmp(server_opt, "GroupMon") == 0) || (strcasecmp(server_opt, "VectorMon") == 0))
    {
      //here we are checking group name if group name is not matched then continue
      group_id_pass_on_runtime = atoi(group_idx);
      NSDL2_MON(NULL, NULL, "gp_info_index = %d", cus_mon_ptr->gp_info_index);
      //Here we dont get NA_ gdf monitor so we dont handle it.
      if(cus_mon_ptr->gp_info_index < 0)
        continue;
      else
        grp_id = get_rpt_grp_id_by_gp_info_index(cus_mon_ptr->gp_info_index);
      //here we are matching gdf_id ,vector_name and server_ip
      RUNTIME_MON_CONDITION
      {
        execute_flag = 1;
      }
      else
      {
        //log error
        continue;
      }
    }
    else if(strcasecmp(server_opt, "AllMon") == 0)
    {
      //nothing to be done here 
      execute_flag = 1;
    }
    else
    {
      //log error
      continue;
    }
     
    if(!(vector_list[vector_idx].flags & MON_BREADCRUMB_SET)) //MON_BREADCRMB_SET is not set in custom monitor.It *maynot* be set for dynamic too
    {
      //this case is for custom monitor
      ptr = vector_list[vector_idx].vector_name;
      strncpy(local_brdcrumb_buf, ptr, 1024);
      int len = strlen(mon_breadcrumb);
      //breadcrumb matches
      if(strncasecmp(local_brdcrumb_buf, mon_breadcrumb, len) != 0)
        continue;
      else
        execute_flag = 1;
      
    }
    else
    {                                                      //dynamic monitor
      ptr = vector_list[vector_idx].mon_breadcrumb;
      //save location into local buffer because in structure we save complete breadcrumb eg: 'T>S>I' but here we only need 'T>S', to do comparison with passed breadcrumb 
      strncpy(local_brdcrumb_buf, ptr, 1024);
      int len = strlen(mon_breadcrumb);
    
      //if local_brdcrumb_buf and mon_breadcrumb  matches then go ahead
      if(strncasecmp(local_brdcrumb_buf, mon_breadcrumb, len) != 0)
      {
        ret = match_passed_breadcrumb(mon_breadcrumb, cus_mon_ptr);
        if (ret == 0)
          continue;
        else
          execute_flag = 1;
      }
      else
      {
        execute_flag = 1;
      }
    }

    if(execute_flag)
    {
      if(!(strcasecmp(keyword, "START_MONITOR")))
      {
        sprintf(server, "%s:%d", cus_mon_ptr->cs_ip, cus_mon_ptr->cs_port);
        strcpy(grp_name, get_gdf_group_name(cus_mon_ptr->gp_info_index));
        if (cus_mon_ptr->conn_state == CM_DELETED)
        {
          sprintf(err_msg, "%s", "Monitor cannot be started as its connection state is CM_DELETED.");
          strcpy(command, "cannot be started");
        }
        else
        {
          if(cus_mon_ptr->flags & OUTBOUND_ENABLED)
            ret = make_and_send_msg_to_start_monitor(cus_mon_ptr);
          else
            ret = start_monitor(keyword, server_opt, cus_mon_ptr, err_msg, mon_id);
          strcpy(command, "started");
        }
      }
      else if(!(strcasecmp(keyword, "STOP_MONITOR")))
      {
        strcpy(command, "stopped");
        sprintf(server, "%s:%d", cus_mon_ptr->cs_ip, cus_mon_ptr->cs_port);
        strcpy(grp_name, get_gdf_group_name(cus_mon_ptr->gp_info_index));
        ret = stop_monitor(keyword, server_opt, cus_mon_ptr, err_msg, mon_breadcrumb);
      }
      else if(!(strcasecmp(keyword, "RESTART_MONITOR")))
      {
        sprintf(server, "%s:%d", cus_mon_ptr->cs_ip, cus_mon_ptr->cs_port);
        strcpy(grp_name, get_gdf_group_name(cus_mon_ptr->gp_info_index));
        if((ret = stop_monitor(keyword, server_opt, cus_mon_ptr, err_msg, mon_breadcrumb)) < 0) {
          if(ret != MON_ALREADY_STOPPED)
            return ret;
        }
        //if error, do not do start
        if (cus_mon_ptr->conn_state == CM_DELETED)
        {
          sprintf(err_msg, "%s", "Monitor cannot be started as its connection state is CM_DELETED.\n");
          strcpy(command, "cannot be started");
        }
        else
        {
          if(cus_mon_ptr->flags & OUTBOUND_ENABLED)
            ret = make_and_send_msg_to_start_monitor(cus_mon_ptr);
          else
            ret = start_monitor(keyword, server_opt, cus_mon_ptr, err_msg, mon_id);
          strcpy(command, "restarted");
        }
      }
      else if(!(strcasecmp(keyword, "DELETE_MONITOR")))
      {
        strcpy(command, "deleted");
        sprintf(server, "%s:%d", cus_mon_ptr->cs_ip, cus_mon_ptr->cs_port);
        strcpy(grp_name, get_gdf_group_name(cus_mon_ptr->gp_info_index));
        ret = stop_monitor(keyword, server_opt, cus_mon_ptr, err_msg, mon_breadcrumb);
      }
      else
      {
        if(err_msg[0] == '\0')
          sprintf(err_msg, "Received invalid keyword '%s'. Valid keywords are 'START_MONITOR', 'STOP_MONITOR', 'RESTART_MONITOR' & 'DELETE_MONITOR'. ", keyword);
        return RUNTIME_ERROR;
      }
      execute_flag = 0;
    }

    if(ret < 0) 
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "%s\n", err_msg); 
      //err_msg[0] = '\0';
      not_done++;
    }
    else
      done++;
  }

  //Previously we saved '\0' in mon_breadcrumb if AllTier is received.
  //Because we won't need to put special handling for AllTier everywhere.
  //we use strncmp(str, mon_breadcrumb, strlen(mon_breadcrumb)) to match brdcrumb
  //When mon_breadcrumb is '\0', it matches with every string.
  //Now saving AllTier in tmp_breadcrumb to display in messages

  char tmp_breadcrumb[1024] = {0};
  if(mon_breadcrumb[0] == '\0')
    strcpy(tmp_breadcrumb, "AllTier");
  else
    strcpy(tmp_breadcrumb, mon_breadcrumb);

  if((done > 0) && (not_done == 0))  //all pass, no one fails
  {
    RTC_MONITOR_LOG_TOPO(keyword, server_opt ,command, vector_name, server, grp_name, err_msg, tmp_breadcrumb, "All"); 
  }
  else if ((done > 0) && (not_done > 0)) //some pass, some fail
  {
    RTC_MONITOR_LOG_TOPO(keyword, server_opt ,command, vector_name, server, grp_name, err_msg, tmp_breadcrumb, "Some"); 
    strcat(err_msg, " Some monitors fails.");
  }
  else 
  {
    if( err_msg[0] == '\0' )
      sprintf(err_msg, "Provided arguments is not correct for any monitor. Args - keyword = %s,  server option= %s, group index = %s, "
                           "vector name = %s", keyword, server_opt, group_idx, vector_name);
    return RUNTIME_ERROR;
  }

  return RUNTIME_SUCCESS;
}


//This function will parse keywords {START_MONITOR,STOP_MONITOR,RESTART_MONITOR,DELETE_MONITOR} <server_opt> <group_id> <verctor_name>
static int parse_keyword_runtime_monitor(char *buf, char *err_msg)
{
char keyword[MAX_DATA_LINE_LENGTH] = "NA";
char server_opt[MAX_DATA_LINE_LENGTH] =  "NA";
char group_id[MAX_DATA_LINE_LENGTH] = "NA";
char vector_name[MAX_NAME_SIZE] = "NA";
char gdf_name[MAX_NAME_SIZE] = "NA";
char mon_breadcrumb[MAX_NAME_SIZE] = "NA";
int grp_id = 0, num, ret;


  NSDL1_MON(NULL, NULL, "buf = %s, err_msg = %s", buf, err_msg); 
  NSDL3_MON(NULL, NULL, "Before parsing keyword = %s, server_opt = %s, group_id = %s, gdf_name = %s, vector_name = %s", keyword, server_opt, group_id, gdf_name, vector_name);

  num = sscanf(buf, "%s %s %s %s %s %s", keyword, server_opt, group_id, vector_name, mon_breadcrumb, gdf_name);
  if(num < 5)
  {
    strcpy(err_msg, "Invalid START_MONITOR entry\nSyntax: START_MONITOR <server_option> <group_id> <vector_name>\n");
    return RUNTIME_ERROR;
  }

  //in topology mode, breadcrumb cannot be NA
  if(strcmp(mon_breadcrumb, "NA") == 0)
  {
    strcpy(err_msg, "Invalid START_MONITOR entry\nSyntax: START_MONITOR <server_option> <group_id> <vector_name> <breadcrumb>\n");
    return RUNTIME_ERROR;
  }

  //if group_id is not 'NA' then only validate given group id
  if((strcmp(group_id, "NA") != 0) && ((grp_id = atoi(group_id)) < 10000))
  {
    sprintf(err_msg, "Invalid group id = %d", grp_id);
    return RUNTIME_ERROR;
  }

  NSDL3_MON(NULL, NULL, "After parsing keyword = %s, server_opt = %s, group_id = %s, gdf_name = %s,  vector_name = %s", keyword, server_opt, group_id, gdf_name, vector_name);


  if(!strcasecmp(keyword,"START_MONITOR")){
    ret = runtime_monitors_delete_topo(keyword, server_opt, mon_breadcrumb, group_id, vector_name, "NA", err_msg);
  }
  else if(!strcasecmp(keyword,"STOP_MONITOR"))
  {
    ret = runtime_monitors_delete_topo(keyword, server_opt, mon_breadcrumb, group_id, vector_name, "NA", err_msg);
  }
  else if(!strcasecmp(keyword,"RESTART_MONITOR"))
  {
    ret = runtime_monitors_delete_topo(keyword, server_opt, mon_breadcrumb, group_id, vector_name, "NA", err_msg);
  }
  //as of now when json is suppported the delete monitor is not called 
  else if(!strcasecmp(keyword,"DELETE_MONITOR"))
  {
    ret = runtime_monitors_delete_topo(keyword, server_opt, mon_breadcrumb, group_id, vector_name, gdf_name,err_msg);
  }
  else
  {
    sprintf(err_msg,"Unknow Keyword '%s'", keyword);
    return RUNTIME_ERROR;
  }

  NSDL1_MON(NULL, NULL, "Method Ends."); 
  return ret; 
}

//this function will parse the DELETE_CHECK_MON_TYPE or DELETE_BATCH_JOB_TYPE keyword on run time
int kw_set_runtime_delete_check_mon(char *check_mon_name)
{
  int i;
  CheckMonitorInfo *check_monitor_ptr = check_monitor_info_ptr;
  NSDL2_MON(NULL, NULL, "Method called.");
  for (i = 0; i < total_check_monitors_entries; i++, check_monitor_ptr++)
  {
    if((strcmp(check_monitor_ptr->check_monitor_name ,check_mon_name) ==0) && check_monitor_ptr->status != CHECK_MONITOR_STOPPED)
    {
      check_monitor_ptr->status = CHECK_MONITOR_STOPPED;
      if(check_monitor_ptr->fd >= 0)
      {
        end_check_monitor(check_monitor_ptr);
      }
      return 0;
    }
  }
  return 1;
}

//this is used for process diff json to delete the monitors
//For deleting monitor through diff
//Monitor should be unique (gdf + monitor name)
//While deleting monitor json diff we don’t know the vector information so each monitor in json should be uniquely identified.
//Some monitor of same gdf have 2 level of monitor name (t1>s1), and some having  3 level of monitor name (t1>s1>i1)
//if NA gdf need to be deleted group id 
int kw_set_runtime_process_deleted_monitors(char *buf, char *err_msg)
{
  char keyword[MAX_DATA_LINE_LENGTH] = "NA";
  char server_opt[MAX_DATA_LINE_LENGTH] =  "NA";
  char group_id[MAX_DATA_LINE_LENGTH] = "-1";
  char vector_name[MAX_NAME_SIZE] = "NA";
  char gdf_name[MAX_NAME_SIZE] = "NA";
  char mon_breadcrumb[MAX_NAME_SIZE] = "NA";
  char command[MAX_DATA_LINE_LENGTH];
  char server[MAX_DATA_LINE_LENGTH] = "NA"; 
  char grp_name[MAX_DATA_LINE_LENGTH] = "NA";
  
  int grp_id = 0, group_idx;
  int mon_id, ret = 0, group_id_pass_on_runtime= -1;
  
  CM_info *cus_mon_ptr = NULL;

  NSDL1_MON(NULL, NULL, "buf = %s, err_msg = %s", buf, err_msg);
  NSDL3_MON(NULL, NULL, "Before parsing keyword = %s, server_opt = %s, group_id = %s, gdf_name = %s, vector_name = %s, mon_breadcrumb = %s", keyword, server_opt, group_id, gdf_name, vector_name, mon_breadcrumb);

  sscanf(buf, "%s %s %s %s %s %s", keyword, server_opt, group_id, vector_name, mon_breadcrumb, gdf_name);

  //if group_id is not 'NA' then only validate given group id
  group_idx = atoi(group_id); 
  if( group_idx >= 0)
  {
    //if((grp_id = group_idx) < 10000)
    if(group_idx < 10000)
    {
      //sprintf(err_msg, "Invalid group id = %d", grp_id);
      sprintf(err_msg, "Invalid group id = %d", group_idx);
      return RUNTIME_ERROR;
    }
    group_id_pass_on_runtime = group_idx;
  }
  else
    group_id_pass_on_runtime =-1;

  NSDL3_MON(NULL, NULL, "After parsing keyword = %s, server_opt = %s, group_id = %s, gdf_name = %s,  vector_name = %s, mon_breadcrumb = %s", keyword, server_opt, group_id, gdf_name, vector_name, mon_breadcrumb);

  //here we are looping monitor list to delete the monitor 
  for(mon_id = 0; mon_id < total_monitor_list_entries; mon_id++)
  {
    cus_mon_ptr = monitor_list_ptr[mon_id].cm_info_mon_conn_ptr;

    NSDL3_MON(NULL, NULL, "mon_id = %d, cus_mon_ptr = %p", mon_id, cus_mon_ptr);
    
    NSDL2_MON(NULL, NULL, "gp_info_index = %d", cus_mon_ptr->gp_info_index);
    
    if (cus_mon_ptr->gp_info_index >= 0)
      grp_id = get_rpt_grp_id_by_gp_info_index(cus_mon_ptr->gp_info_index);
    else 
      grp_id = -1;
     
    if((group_id_pass_on_runtime == grp_id ) && (strcmp(cus_mon_ptr->monitor_name, mon_breadcrumb) == 0))
    {
      if( (group_id_pass_on_runtime == -1) && (strcmp(cus_mon_ptr->gdf_name, gdf_name) != 0))
      {
        continue;
      }
      strcpy(command, "deleted");
      sprintf(server, "%s:%d", cus_mon_ptr->cs_ip, cus_mon_ptr->cs_port);
      strcpy(grp_name, get_gdf_group_name(cus_mon_ptr->gp_info_index));
      ret = stop_monitor(keyword, server_opt, cus_mon_ptr, err_msg, mon_breadcrumb);
      if(ret < 0)
      {
        if( err_msg[0] == '\0' )
          sprintf(err_msg, "Provided arguments is not correct for any monitor. Args - keyword = %s,  server option= %s, group index = %s, "
                           "vector name = %s ,mon_breadcrumb = %s" , keyword, server_opt, group_id, vector_name, mon_breadcrumb);
        return RUNTIME_ERROR;
      }
      else
      {
        RTC_MONITOR_LOG_TOPO(keyword, server_opt ,command,"NA", server, grp_name, err_msg, mon_breadcrumb, "All");
        return RUNTIME_SUCCESS;
      }
    }
    //this case is for when we get no vector in cm_info ptr
    else if((cus_mon_ptr->total_vectors <= 0) && (strcmp(cus_mon_ptr->gdf_name_only,gdf_name) == 0) && 
            (strcmp(cus_mon_ptr->monitor_name, mon_breadcrumb) == 0))
    {
      strcpy(command, "deleted");
      sprintf(server, "%s:%d", cus_mon_ptr->cs_ip, cus_mon_ptr->cs_port);
      strcpy(grp_name, get_gdf_group_name(cus_mon_ptr->gp_info_index));
      ret = stop_monitor(keyword, server_opt, cus_mon_ptr, err_msg, mon_breadcrumb);
      if(ret < 0)
      {
        if( err_msg[0] == '\0' )
          sprintf(err_msg, "Provided arguments is not correct for any monitor. Args - keyword = %s,  server option= %s, group index = %s, "
                           "vector name = %s ,mon_breadcrumb = %s" , keyword, server_opt, group_id, vector_name, mon_breadcrumb);
        return RUNTIME_ERROR;
      }
      else
      {
        RTC_MONITOR_LOG_TOPO(keyword, server_opt ,command,"NA", server, grp_name, err_msg, mon_breadcrumb, "All");
        return RUNTIME_SUCCESS;
      }
    }
  }
  if( err_msg[0] == '\0' )
    sprintf(err_msg, "No monitor is deleted. Args - keyword = %s,  server option= %s, group index = %s, vector name = %s ,mon_breadcrumb = %s" , keyword, server_opt, group_id, vector_name, mon_breadcrumb);
  return RUNTIME_ERROR; 
}


//This function will parse START_MONITOR keyword on run time
int kw_set_runtime_monitors(char *buf, char *err_msg)
{
int ret;

  NSDL1_SCHEDULE(NULL, NULL, "Method Called. buf = %s, err_msg = %s", buf, err_msg);

  ret = parse_keyword_runtime_monitor(buf, err_msg);

  NSDL1_SCHEDULE(NULL, NULL, "Method End. ret = %d, err_msg = %s", ret, err_msg);
  return ret;
}


/*int delete_vector(CM_info *source_cm_ptr, int source_idx,int total_vectors, int master_vector_indx, CM_info *destination_cm_ptr)
{
  int dbuf_len = 0;

  NSDL2_MON(NULL, NULL, "Method Called, source_cm_ptr = %p, source_idx = %d,total_vectors = %d, master_vector_indx= %d",
                        source_cm_ptr, source_idx,total_vectors,master_vector_indx); 
  //is this a last vector
  if(total_vectors == 1) 
  {
    memcpy(&destination_cm_ptr[master_vector_indx], &source_cm_ptr[source_idx], sizeof(CM_info)); 

    NSDL2_MON(NULL, NULL, "destination_cm_ptr[%d].total_vectors = [%d], source_cm_ptr[%d].total_vectors = [%d] ", master_vector_indx, source_idx, destination_cm_ptr[master_vector_indx].total_vectors, source_cm_ptr[source_idx].total_vectors );

    //don't close the fd and also don't remove the entry
    //only set following 
    destination_cm_ptr[master_vector_indx].flags |= ALL_VECTOR_DELETED;
    destination_cm_ptr[master_vector_indx].flags |= DATA_FILLED;
    destination_cm_ptr[master_vector_indx].num_dynamic_filled = 0;
    destination_cm_ptr[master_vector_indx].dindex = 0;

    //clear vector name
    destination_cm_ptr[master_vector_indx].vector_name[0] = '\0'; 
    destination_cm_ptr[master_vector_indx].is_mon_breadcrumb_set = 0;
    destination_cm_ptr[master_vector_indx].group_vector_idx = 0;
    return 1;
  }
  else //other vectors are still present
  {
    // Vector deleted holds the fd. Assign this fd to next vector. Code will come here only if
    // there is more than 1 vector. Also assumption is that first vector holds fd
    if (source_cm_ptr[source_idx].fd >0)
    {
      source_cm_ptr[source_idx+1].fd = source_cm_ptr[source_idx].fd;
      source_cm_ptr[source_idx+1].is_dynamic = source_cm_ptr[source_idx].is_dynamic;
      source_cm_ptr[source_idx+1].num_dynamic_filled = 0;
      //source_cm_ptr[source_idx+1].dyn_num_vectors = --source_cm_ptr[source_idx].dyn_num_vectors;
      source_cm_ptr[source_idx+1].dyn_num_vectors = source_cm_ptr[source_idx].dyn_num_vectors - 1;
      source_cm_ptr[source_idx+1].is_group_vector = source_cm_ptr[source_idx].is_group_vector;
      source_cm_ptr[source_idx+1].group_vector_idx = source_cm_ptr[source_idx].group_vector_idx;

      //We set dbuf_len to a big size because there may come a big error message from custom monitor
      dbuf_len = MAX_MONITOR_BUFFER_SIZE + 1;
      MY_MALLOC(source_cm_ptr[source_idx+1].data_buf, dbuf_len, "Custom Monitor data buf", -1);
      memset(source_cm_ptr[source_idx+1].data_buf, 0, dbuf_len);
      memcpy(source_cm_ptr[source_idx+1].data_buf, source_cm_ptr[source_idx].data_buf, source_cm_ptr[source_idx].dindex);
      source_cm_ptr[source_idx+1].dindex = source_cm_ptr[source_idx].dindex;

      
      if(source_cm_ptr[source_idx].is_nd_vector)
      {
        source_cm_ptr[source_idx+1].instanceVectorIdxTable = source_cm_ptr[source_idx].instanceVectorIdxTable;
        source_cm_ptr[source_idx+1].tierNormVectorIdxTable = source_cm_ptr[source_idx].tierNormVectorIdxTable;
        source_cm_ptr[source_idx+1].cur_tierIdx_TierNormVectorIdxTable = source_cm_ptr[source_idx].cur_tierIdx_TierNormVectorIdxTable;
        source_cm_ptr[source_idx+1].cur_normVecIdx_TierNormVectorIdxTable = source_cm_ptr[source_idx].cur_normVecIdx_TierNormVectorIdxTable;
        source_cm_ptr[source_idx+1].cur_instIdx_InstanceVectorIdxTable = source_cm_ptr[source_idx].cur_instIdx_InstanceVectorIdxTable;
        source_cm_ptr[source_idx+1].cur_vecIdx_InstanceVectorIdxTable = source_cm_ptr[source_idx].cur_vecIdx_InstanceVectorIdxTable;
        source_cm_ptr[source_idx+1].instanceIdxMap = source_cm_ptr[source_idx].instanceIdxMap;
        source_cm_ptr[source_idx+1].tierIdxmap = source_cm_ptr[source_idx].tierIdxmap;
        source_cm_ptr[source_idx+1].ndVectorIdxmap = source_cm_ptr[source_idx].ndVectorIdxmap;
        memcpy(source_cm_ptr[source_idx+1].save_times_graph_idx, source_cm_ptr[source_idx].save_times_graph_idx, 128);

        memcpy(&(source_cm_ptr[source_idx+1].key), &(source_cm_ptr[source_idx].key), sizeof(source_cm_ptr[source_idx].key));

        source_cm_ptr[source_idx+1].is_norm_init_done = source_cm_ptr[source_idx].is_norm_init_done;
        source_cm_ptr[source_idx+1].dummy_data = source_cm_ptr[source_idx].dummy_data;
      }
      else if ((strstr(source_cm_ptr[source_idx].gdf_name, "cm_nv_") != NULL))
      {
        source_cm_ptr[source_idx+1].nv_map_data_tbl = source_cm_ptr[source_idx].nv_map_data_tbl;
        source_cm_ptr[source_idx+1].vectorIdxmap = source_cm_ptr[source_idx].vectorIdxmap;
      }
    }
    else
    {
      destination_cm_ptr[master_vector_indx].num_dynamic_filled = 0;
      destination_cm_ptr[master_vector_indx].dyn_num_vectors--;
    }
    return 0;  
  }
}*/

//This function is called when add vector is called and no vector is present only connection is mode.
//This happens when all vector existing monitor is deleted

inline void merge_special_into_runtime_cm_table(CM_info *source_cm_ptr, int start_source_idx, CM_info *destination_cm_ptr, int start_destination_idx, int num_source_cm_to_copy)
{


}

/************************************** RUNTIME ADD/DELETE MONITOR CODE STARTS *******************************************/

void search_deleted_mon_in_server_list(char *server_ip, int input_port, int all_vector_deleted_flag, int num_vector)
{
  int i,j = 0, ret = 0;

  //ServerInfo *servers_list = NULL;
  //int total_no_of_servers = 0;

  //servers_list = (ServerInfo *) topolib_get_server_info();
  //Total no. of servers in ServerInfo structure
  //total_no_of_servers = topolib_get_total_no_of_servers();

  //NSDL4_MON(NULL, NULL, "Method called. server_ip = %s, input_port = %d, num_vector = %d, total_no_of_servers = %d", server_ip,input_port, num_vector, total_no_of_servers);

  for(i = 0; i < topo_info[topo_idx].total_tier_count; i++)
  {
    for(j=0;j<topo_info[topo_idx].topo_tier_info[i].total_server;j++)
    { 
      if((topo_info[topo_idx].topo_tier_info[i].topo_server[j].used_row != -1) && (!topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->server_flag & DUMMY))
      { 
        ret = topolib_chk_server_ip_port(server_ip, input_port, topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->server_ip, NULL, NULL, 0, 0);
        NSDL4_MON(NULL, NULL, "i = %d, ret = %d", i, ret); 
   
        if(ret == 0) //Did not matched. Go for next IP in list
          continue;
        else 
        {
          if(num_vector == 0)
            topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->cmon_monitor_count--;
          else 
            topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->cmon_monitor_count = topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->cmon_monitor_count - num_vector;

        //no more monitor running on this server hence need to close control connection
      //Here We will be setting cmon_monitor_count to -1 only for monitors with "Warning: No vectors." in the vector list, because it will be again increased to 0 in custom config. And when the vectors come for the first time time this count again increase to 1 from custom config. 
      //But previously when we were talking about a monitor with one vector, we were having an issue because cmon_monitor_count got -1 here and it again increased to 0 from custom config. And due to this heart beat message was not been sent to cmon.
          if((topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->cmon_monitor_count == 0) && all_vector_deleted_flag)
          topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->cmon_monitor_count = -1;  //while sending next heartbeat msg, chk this flag and if -ve close connection

          return;
        }
      }
    }
    NSDL4_MON(NULL, NULL, "Method exited.");
  }
}

/* This is a common copy function to copy below defined into table cm_info_runtime_ptr:
 *                                       Normal CM 
 *                                       Normal DVM->CM
 *                                       Runtime DVM->CM
 */
/*inline void merge_into_runtime_cm_table(CM_info *source_cm_ptr, int start_source_idx, CM_info *destination_cm_ptr, int start_destination_idx, int num_source_cm_to_copy, int master_vector_indx, int *relative_dyn_idx)
{
  int i;
  int destination_idx = start_destination_idx;
  int source_idx = start_source_idx;
  
  NSDL2_MON(NULL, NULL, "Method Called, source_cm_ptr = %p, start_source_idx = %d, destination_cm_ptr = %p, start_destination_idx = %d, "
                        "num_source_cm_to_copy = %d, total_cm_runtime_entries = %d", 
                        source_cm_ptr, start_source_idx, destination_cm_ptr, start_destination_idx, num_source_cm_to_copy, 
                        total_cm_runtime_entries);
  NSTL2(NULL, NULL, "Method Called, source_cm_ptr = %p, start_source_idx = %d, destination_cm_ptr = %p, start_destination_idx = %d, "
                        "num_source_cm_to_copy = %d, total_cm_runtime_entries = %d",
                        source_cm_ptr, start_source_idx, destination_cm_ptr, start_destination_idx, num_source_cm_to_copy,
                        total_cm_runtime_entries);

  for(i = 0; i < num_source_cm_to_copy; i++)
  {
    NSDL4_MON(NULL, NULL, "i = %d, destination_idx = %d, source_idx = %d", i, destination_idx, source_idx);

   memcpy(&destination_cm_ptr[destination_idx], &source_cm_ptr[source_idx], sizeof(CM_info)); 

   NSDL2_MON(NULL, NULL, " CHECK ::: destination_cm_ptr[%d].dyn_num_vectors = [%d], source_cm_ptr[%d].dyn_num_vectors = [%d] ", destination_idx, source_idx, destination_cm_ptr[destination_idx].dyn_num_vectors, source_cm_ptr[source_idx].dyn_num_vectors );

   if((relative_dyn_idx != NULL) && (destination_cm_ptr[destination_idx].dvm_cm_mapping_tbl_row_idx != -1))//dynamic vector monitor
   {
     if(i == 0) //this is parent
     {
       //Update parent idx on merging
       destination_cm_ptr[destination_idx].parent_idx = destination_idx;
     }

     if(destination_cm_ptr[destination_idx].vectorIdx >= 0)
     {
       //reset is_data_filled of each vector in mapping table on every merging
       dvm_idx_mapping_ptr[destination_cm_ptr[destination_idx].dvm_cm_mapping_tbl_row_idx][destination_cm_ptr[destination_idx].vectorIdx].is_data_filled = -1; 

       //update relative index in mapping index
       dvm_idx_mapping_ptr[destination_cm_ptr[destination_idx].dvm_cm_mapping_tbl_row_idx][destination_cm_ptr[destination_idx].vectorIdx].relative_dyn_idx = *relative_dyn_idx;

     (*relative_dyn_idx)++;
     }
   }

    //Increment idx
    total_cm_runtime_entries++;
    destination_idx++;
    source_idx++;
  }
  
  NSDL2_MON(NULL, NULL, "After copied: total_cm_runtime_entries = %d, destination_idx = %d, source_idx = %d", 
                         total_cm_runtime_entries, destination_idx, source_idx++);
  NSTL2(NULL, NULL, "After copied: total_cm_runtime_entries = %d, destination_idx = %d, source_idx = %d",
                         total_cm_runtime_entries, destination_idx, source_idx++);
}*/

/* This is to copy all the normal CM from cm_info_ptr table to cm_info_runtime_ptr.
 * cm_info_runtime_ptr => already contains Runtime CM
 * After copying normal CM into this, it will contain all the "normal CM + Runtime CM"
 */
/*inline void merge_old_cm_into_runtime_cm_table()
{
  int start_old_cm_idx = 0; //always start from indx 0
  int num_old_cm_count = g_dyn_cm_start_idx; 

  NSDL2_MON(NULL, NULL, "Method Called, num_old_cm_count = %d, total_cm_runtime_entries = %d", num_old_cm_count, total_cm_runtime_entries); 
  NSTL1(NULL, NULL, "Method Called, num_old_cm_count = %d, total_cm_runtime_entries = %d", num_old_cm_count, total_cm_runtime_entries); 

  if(!(num_old_cm_count + total_cm_runtime_entries))
  {
    NSDL2_MON(NULL, NULL, "No CM entries found, hence returning..."); 
    return;
  }

  //Create realloc new to num_old_cm + num_new_cm

  MY_REALLOC(cm_info_runtime_ptr, (num_old_cm_count + total_cm_runtime_entries) * sizeof(CM_info), "Merging old and runtime custom moniotrs", -1);

  NSTL1(NULL, NULL, "Realloc cm_info_runtime_ptr structure with new size = %d, old size = %d", (num_old_cm_count + total_cm_runtime_entries), num_old_cm_count);
  merge_into_runtime_cm_table(cm_info_ptr, start_old_cm_idx, cm_info_runtime_ptr, total_cm_runtime_entries, num_old_cm_count, 0, NULL);

  if(total_cm_runtime_entries == num_old_cm_count) //not found any new entry, and old entry is already sorted hence no need to sort
  {
    NSDL2_MON(NULL, NULL, "Not found any new entry. Hence returning without sorting."); 
    NSTL1(NULL, NULL, "Not found any new entry. Hence returning without sorting.");
  }
  else
  {
    NSDL2_MON(NULL, NULL, "Found new entries, doing sorting"); 
    //total_cm_runtime_entries include both old and new (updated in merge_into_runtime_cm_table())
    qsort(cm_info_runtime_ptr, total_cm_runtime_entries, sizeof(CM_info), check_if_cm_vector);
    NSTL1(NULL, NULL, "Sorting done for custom monitor table with new entries");
  }

  return;
}*/


/* Merge 'old & new' DVM->CM into one CM_info table, table where CM's are already copied
 * Realloc (cm_info_runtime_ptr = total_cm_runtime_entries + total_cm_entries + total_dyn_cm_runtime_entries)
 * where,
 *     cm_info_runtime_ptr ==> is table where we put: (1) Runtime CM               ==> this is total_cm_runtime_entries
 *                                                    (2) Normal CM (not runtime)  ==> (2) + (3) is total_cm_entries
 *                                                    (3) Normal DVM -> CM
 *                                                    (4) Runtime DVM -> CM        ==> this is total_dyn_cm_runtime_entries
 *                                                     
 */
/*inline void merge_old_and_new_dyn_cm()
{
  int old_dyn_cm_idx, num_old_dyn_cm;
  int dyn_idx = 0, i, relative_dyn_idx = 0; //relative_dyn_idx is to fill dyn_idx (relative index) in cm_info table for each vector
  int old_cm_dyn_vector_count = 0;
  int dbuf_len = 0;
  int ret = 0;
  
  old_dyn_cm_idx = g_dyn_cm_start_idx;
  num_old_dyn_cm = total_cm_entries - g_dyn_cm_start_idx; //number of dynamic monitors

  NSDL2_MON(NULL, NULL, "Method Called, total_cm_entries = %d, g_dyn_cm_start_idx = %d, num_old_dyn_cm = %d, total_dyn_cm_runtime_entries = %d, old_dyn_cm_idx = %d", 
                         total_cm_entries, g_dyn_cm_start_idx, num_old_dyn_cm, total_dyn_cm_runtime_entries, old_dyn_cm_idx); 

  NSTL1(NULL, NULL, "Method Called, total_cm_entries = %d, g_dyn_cm_start_idx = %d, num_old_dyn_cm = %d, total_dyn_cm_runtime_entries = %d, old_dyn_cm_idx = %d", total_cm_entries, g_dyn_cm_start_idx, num_old_dyn_cm, total_dyn_cm_runtime_entries, old_dyn_cm_idx);

  //reset g_dyn_cm_start_idx 
  g_dyn_cm_start_idx = total_cm_runtime_entries;

  NSDL2_MON(NULL, NULL, "g_dyn_cm_start_idx = %d, (total_cm_runtime_entries + num_old_dyn_cm + total_dyn_cm_runtime_entries) = %d", g_dyn_cm_start_idx, (total_cm_runtime_entries + num_old_dyn_cm + total_dyn_cm_runtime_entries)); 

  if(!(total_cm_runtime_entries + num_old_dyn_cm + total_dyn_cm_runtime_entries))
  {
    NSDL2_MON(NULL, NULL, "No DVM->CM entries found, hence returning..."); 
    return;
  }

  MY_REALLOC(cm_info_runtime_ptr, ((total_cm_runtime_entries + num_old_dyn_cm + total_dyn_cm_runtime_entries) * sizeof(CM_info)), "Merging old and new dynamic custom moniotrs into runtime cm table", -1); // DELETED MONITOR will also get allocated but not merged

  int new_vector_count = 0;

  while(old_dyn_cm_idx < total_cm_entries)
  {
    ret = 0;
    relative_dyn_idx = 0; //reset here, this is to fill dyn_idx (relative index) in cm_info table for each vector  

    NSDL4_MON(NULL, NULL, "old_dyn_cm_idx %d, total_cm_entries %d, cm_info_ptr[%d].conn_state %d, vector name %s.", old_dyn_cm_idx, 
                           total_cm_entries, old_dyn_cm_idx, cm_info_ptr[old_dyn_cm_idx].conn_state, cm_info_ptr[old_dyn_cm_idx].vector_name);

    old_cm_dyn_vector_count = cm_info_ptr[old_dyn_cm_idx].dyn_num_vectors;

    NSDL2_MON(NULL, NULL, "old_dyn_cm_idx = %d, num_old_dyn_cm = %d, total_cm_runtime_entries = %d, cm_info_ptr[old_dyn_cm_idx].dyn_num_vectors = %d", old_dyn_cm_idx, num_old_dyn_cm, total_cm_runtime_entries, cm_info_ptr[old_dyn_cm_idx].dyn_num_vectors);

    if (cm_info_ptr[old_dyn_cm_idx].all_vector_deleted == 1)
    {
      if((cm_info_ptr[old_dyn_cm_idx].vector_name[0] == '\0'))
      {
        if(total_reused_vectors)
        {
          for(i = 0; i < total_reused_vectors; i++)
          {
	    if((((reused_vector[i]->mon_id == cm_info_ptr[old_dyn_cm_idx].mon_id)) && (!(strcmp(reused_vector[i]->gdf_name, cm_info_ptr[old_dyn_cm_idx].gdf_name)))) || (cm_info_ptr[old_dyn_cm_idx].runtime_and_copy_flag & DO_NOT_MERGE))
            {
              ret=1;
              if(cm_info_ptr[old_dyn_cm_idx].fd > 0)
              {
                NSDL2_MON(NULL, NULL, "Remove select fd  = [%d]", cm_info_ptr[old_dyn_cm_idx].fd);
                NSTL1(NULL, NULL, "Skipping  merging of dummy entry[ %p ] and removing fd[ %d ] from the epoll as it is marked as DO_NOT_MERGE",cm_info_ptr[old_dyn_cm_idx], cm_info_ptr[old_dyn_cm_idx].fd);
                //remove select to remove fd with old address of cm_info_ptr
                //remove_select_msg_com_con_ex(__FILE__, __LINE__, (char *)__FUNCTION__, cm_info_ptr[old_dyn_cm_idx].fd);
                close_custom_monitor_connection(&cm_info_ptr[old_dyn_cm_idx]);
              }
              break;
            }
          }
        }
        if(ret == 1)
        {
          old_dyn_cm_idx = old_dyn_cm_idx + 1;
          continue;
        }
      }
      merge_into_runtime_cm_table(cm_info_ptr, old_dyn_cm_idx, cm_info_runtime_ptr, total_cm_runtime_entries,1, 0, &relative_dyn_idx);
    }
    else
      merge_into_runtime_cm_table(cm_info_ptr, old_dyn_cm_idx, cm_info_runtime_ptr, total_cm_runtime_entries, cm_info_ptr[old_dyn_cm_idx].dyn_num_vectors, 0, &relative_dyn_idx);


     // update global variable when merging is done.
     if(cm_info_ptr[old_dyn_cm_idx].cs_port == -1)
     {
       nc_ip_data_mon_idx = (total_cm_runtime_entries - (cm_info_ptr[old_dyn_cm_idx].dyn_num_vectors - get_deleted_vector_count(&cm_info_ptr[old_dyn_cm_idx])));

       if(cm_info_runtime_ptr[old_dyn_cm_idx].all_vector_deleted == 1)
       {
         if(get_deleted_vector_count(&cm_info_ptr[old_dyn_cm_idx]) > 0)
           nc_ip_data_mon_idx -= cm_info_runtime_ptr[old_dyn_cm_idx].all_vector_deleted; 
       }
     }

    //THIS LOOP IS FOR NEW VECTOR ONLY
    for(dyn_idx = 0; dyn_idx < total_dyn_cm_runtime_entries;)
    {
      NSDL2_MON(NULL, NULL, "total_dyn_cm_runtime_entries = %d, new_vector_count = %d, cm_info_ptr[old_dyn_cm_idx].gdf_name [%s], "
                            "dyn_cm_info_runtime_ptr[dyn_idx].gdf_name = [%s], cm_info_ptr[old_dyn_cm_idx].fd = [%d] , "
                            "dyn_cm_info_runtime_ptr[dyn_idx].fd = [%d], dyn_idx = %d, total_dyn_cm_runtime_entries = %d", 
                             total_dyn_cm_runtime_entries, new_vector_count, cm_info_ptr[old_dyn_cm_idx].gdf_name, 
                             dyn_cm_info_runtime_ptr[dyn_idx].gdf_name, dyn_cm_info_runtime_ptr[dyn_idx].fd, 
                             dyn_cm_info_runtime_ptr[dyn_idx].fd, dyn_idx, total_dyn_cm_runtime_entries);

      //IF both matched & fd is not -ve then new monitor is a explicit vector(either received new vector/through add vector msg) & add this vector in new runtime CM table & set its copy flag in its own table.

      if ((strcmp(cm_info_ptr[old_dyn_cm_idx].gdf_name, dyn_cm_info_runtime_ptr[dyn_idx].gdf_name) == 0) &&
	 ((cm_info_ptr[old_dyn_cm_idx].mon_id == dyn_cm_info_runtime_ptr[dyn_idx].mon_id) && ((cm_info_ptr[old_dyn_cm_idx].mon_id >= 0) || (cm_info_ptr[old_dyn_cm_idx].cs_port == -1)))) 
      {
        NSDL2_MON(NULL, NULL, "Matched. total_dyn_cm_runtime_entries %d, new_vector_count %d", total_dyn_cm_runtime_entries, new_vector_count);
        //Additional vector got at runtime
        if(dyn_cm_info_runtime_ptr[dyn_idx].dyn_num_vectors == 0)
        {
          NSDL2_MON(NULL, NULL, "dyn_cm_info_runtime_ptr[dyn_idx].dyn_num_vectors is 0. new_vector_count %d", new_vector_count);
        
          //getting parent index, who has made connection 
          int master_vector_indx = -1; 
          
          //Fix for Bug-15318. Core dump was coming when all vectors of any monitor were deleted and then vector of the same parent was added. So, the newly added vector was getting wrong parent index.   

          //When all vectors of a monitor get deleted then .all_vector_deleted flag is set. So, to get the correct parent index  we are subtracting .all_vector_deleted flag from  "(total_cm_runtime_entries - (cm_info_ptr[old_dyn_cm_idx].dyn_num_vectors - get_deleted_vector_count(&cm_info_ptr[old_dyn_cm_idx])))".

         //And "cm_info_runtime_ptr[old_dyn_cm_idx].all_vector_deleted" can only be 0 or 1.


//MARCH 16
          if(new_vector_count == 0)
            master_vector_indx = total_cm_runtime_entries - cm_info_ptr[old_dyn_cm_idx].dyn_num_vectors;
           //master_vector_indx = (total_cm_runtime_entries - (cm_info_ptr[old_dyn_cm_idx].dyn_num_vectors - get_deleted_vector_count(&cm_info_ptr[old_dyn_cm_idx])));
          else
            master_vector_indx = ((total_cm_runtime_entries - cm_info_ptr[old_dyn_cm_idx].dyn_num_vectors) - new_vector_count);
            //master_vector_indx = ((total_cm_runtime_entries - (cm_info_ptr[old_dyn_cm_idx].dyn_num_vectors - get_deleted_vector_count(&cm_info_ptr[old_dyn_cm_idx]))) - new_vector_count);


          NSDL2_MON(NULL, NULL, "BEFORE: master_vector_indx = [%d], cm_info_runtime_ptr[master_vector_indx].dyn_num_vectors = [%d], cm_info_runtime_ptr[master_vector_indx].num_dynamic_filled = [%d], cm_info_runtime_ptr[total_cm_runtime_entries].group_vector_idx = [%d], cm_info_runtime_ptr[master_vector_indx].group_num_vectors = [%d], flag %d", master_vector_indx, cm_info_runtime_ptr[master_vector_indx].dyn_num_vectors, cm_info_runtime_ptr[master_vector_indx].num_dynamic_filled, cm_info_runtime_ptr[total_cm_runtime_entries].group_vector_idx, cm_info_runtime_ptr[master_vector_indx].group_num_vectors, new_vector_count);
;
  
         //CHECK if data then only increment 'num_dynamic_filled' .... 
          if(dyn_cm_info_runtime_ptr[dyn_idx].data != NULL)
            cm_info_runtime_ptr[master_vector_indx].num_dynamic_filled++;

          if (cm_info_runtime_ptr[master_vector_indx].all_vector_deleted == 1)
          {
            if(cm_info_runtime_ptr[master_vector_indx].is_nd_vector)
            {
              dyn_cm_info_runtime_ptr[dyn_idx].instanceVectorIdxTable = cm_info_runtime_ptr[master_vector_indx].instanceVectorIdxTable;  
              dyn_cm_info_runtime_ptr[dyn_idx].tierNormVectorIdxTable = cm_info_runtime_ptr[master_vector_indx].tierNormVectorIdxTable;  
              dyn_cm_info_runtime_ptr[dyn_idx].cur_tierIdx_TierNormVectorIdxTable = cm_info_runtime_ptr[master_vector_indx].cur_tierIdx_TierNormVectorIdxTable;  
              dyn_cm_info_runtime_ptr[dyn_idx].cur_normVecIdx_TierNormVectorIdxTable = cm_info_runtime_ptr[master_vector_indx].cur_normVecIdx_TierNormVectorIdxTable;  
              dyn_cm_info_runtime_ptr[dyn_idx].cur_instIdx_InstanceVectorIdxTable = cm_info_runtime_ptr[master_vector_indx].cur_instIdx_InstanceVectorIdxTable;  
              dyn_cm_info_runtime_ptr[dyn_idx].cur_vecIdx_InstanceVectorIdxTable = cm_info_runtime_ptr[master_vector_indx].cur_vecIdx_InstanceVectorIdxTable;  
              dyn_cm_info_runtime_ptr[dyn_idx].instanceIdxMap = cm_info_runtime_ptr[master_vector_indx].instanceIdxMap;  
              dyn_cm_info_runtime_ptr[dyn_idx].tierIdxmap = cm_info_runtime_ptr[master_vector_indx].tierIdxmap;  
              dyn_cm_info_runtime_ptr[dyn_idx].ndVectorIdxmap = cm_info_runtime_ptr[master_vector_indx].ndVectorIdxmap;  
              memcpy(dyn_cm_info_runtime_ptr[dyn_idx].save_times_graph_idx, cm_info_runtime_ptr[master_vector_indx].save_times_graph_idx, 128);

              memcpy(&(dyn_cm_info_runtime_ptr[dyn_idx].key), &(cm_info_runtime_ptr[master_vector_indx].key), sizeof(cm_info_runtime_ptr[master_vector_indx].key));

              dyn_cm_info_runtime_ptr[dyn_idx].is_norm_init_done = cm_info_runtime_ptr[master_vector_indx].is_norm_init_done;
              dyn_cm_info_runtime_ptr[dyn_idx].dummy_data = cm_info_runtime_ptr[master_vector_indx].dummy_data;
              dyn_cm_info_runtime_ptr[dyn_idx].no_log_OR_vector_format = cm_info_runtime_ptr[master_vector_indx].no_log_OR_vector_format;
            }
            else if(strstr(cm_info_runtime_ptr[master_vector_indx].gdf_name, "cm_nv") != NULL)
            {
              dyn_cm_info_runtime_ptr[dyn_idx].vectorIdxmap = cm_info_runtime_ptr[master_vector_indx].vectorIdxmap;
              dyn_cm_info_runtime_ptr[dyn_idx].nv_map_data_tbl = cm_info_runtime_ptr[master_vector_indx].nv_map_data_tbl;
            }
            else
            {
              dyn_cm_info_runtime_ptr[dyn_idx].skip_breadcrumb_creation = cm_info_runtime_ptr[master_vector_indx].skip_breadcrumb_creation;
            }

            //dyn_cm_info_runtime_ptr[dyn_idx].group_element_size = cm_info_runtime_ptr[master_vector_indx].group_element_size;
            memcpy(&dyn_cm_info_runtime_ptr[dyn_idx].group_element_size, &cm_info_runtime_ptr[master_vector_indx].group_element_size, sizeof(unsigned short) * (MAX_METRIC_PRIORITY_LEVELS + 1));

            //else if(cm_info_runtime_ptr[master_vector_indx].cs_port == -1)    //Only incase of NetCloud monitor.
            //{
              ///dyn_cm_info_runtime_ptr[dyn_idx].genVectorPtr = cm_info_runtime_ptr[master_vector_indx].genVectorPtr;
           //}


            //preserving max_mapping_tbl_vector_entries
            if(cm_info_runtime_ptr[master_vector_indx].max_mapping_tbl_vector_entries > 0)
            {
              dyn_cm_info_runtime_ptr[dyn_idx].max_mapping_tbl_vector_entries = cm_info_runtime_ptr[master_vector_indx].max_mapping_tbl_vector_entries;  
              dyn_cm_info_runtime_ptr[dyn_idx].parent_idx = cm_info_runtime_ptr[master_vector_indx].parent_idx;

              //reset is_data_filled of each vector in mapping table on every merging
              dvm_idx_mapping_ptr[dyn_cm_info_runtime_ptr[dyn_idx].dvm_cm_mapping_tbl_row_idx][dyn_cm_info_runtime_ptr[dyn_idx].vectorIdx].is_data_filled = -1;

              dvm_idx_mapping_ptr[dyn_cm_info_runtime_ptr[dyn_idx].dvm_cm_mapping_tbl_row_idx][dyn_cm_info_runtime_ptr[dyn_idx].vectorIdx].relative_dyn_idx = relative_dyn_idx;

              relative_dyn_idx++;
            }


            //We set dbuf_len to a big size because there may come a big error message from custom monitor
            dbuf_len = MAX_MONITOR_BUFFER_SIZE + 1;
            //free
            //free_cm_tbl_row(&cm_info_runtime_ptr[master_vector_indx], -1);

            //this DVM has no vector, so don't create new entry for vector just update existing entry
            memcpy(&cm_info_runtime_ptr[master_vector_indx], &dyn_cm_info_runtime_ptr[dyn_idx], sizeof(CM_info));
            MY_MALLOC(cm_info_runtime_ptr[master_vector_indx].data_buf, dbuf_len, "Custom Monitor data buf", -1);
            memset(cm_info_runtime_ptr[master_vector_indx].data_buf, 0, dbuf_len);
            cm_info_runtime_ptr[master_vector_indx].is_dynamic = 1;
            cm_info_runtime_ptr[master_vector_indx].all_vector_deleted = 0;
            cm_info_runtime_ptr[master_vector_indx].is_group_vector = 1;
            cm_info_runtime_ptr[master_vector_indx].dyn_num_vectors = 1;

            //set netcloud monitor data ptrs
            if(cm_info_runtime_ptr[master_vector_indx].cs_port == -1)
            {
              cm_info_runtime_ptr[master_vector_indx].data = genVectorPtr[cm_info_runtime_ptr[master_vector_indx].generator_id][cm_info_runtime_ptr[master_vector_indx].vectorIdx].data;
              cm_info_runtime_ptr[master_vector_indx].is_data_filled = 1; 
            }
          }
          else
          {
            merge_into_runtime_cm_table(dyn_cm_info_runtime_ptr, dyn_idx, cm_info_runtime_ptr, total_cm_runtime_entries, 1, master_vector_indx, &relative_dyn_idx);
            new_vector_count++;
            cm_info_runtime_ptr[total_cm_runtime_entries - 1].fd = -1;
            cm_info_runtime_ptr[master_vector_indx].dyn_num_vectors++;

            //set netcloud monitor data ptrs
            if(cm_info_runtime_ptr[master_vector_indx].cs_port == -1)
            {
              cm_info_runtime_ptr[total_cm_runtime_entries - 1].data = genVectorPtr[cm_info_runtime_ptr[total_cm_runtime_entries - 1].generator_id][cm_info_runtime_ptr[total_cm_runtime_entries - 1].vectorIdx].data;
              cm_info_runtime_ptr[total_cm_runtime_entries - 1].is_data_filled = 1;
            }
          }


          //cm_info_runtimeptr[total_cm_runtime_entries - 1].group_vector_idx = cm_info_runtime_ptr[master_vector_indx].group_num_vectors; 
          //cm_info_runtime_ptr[master_vector_indx].group_num_vectors++;
          //reset fd

          NSDL2_MON(NULL, NULL, "AFTER: master_vector_indx = [%d], cm_info_runtime_ptr[master_vector_indx].dyn_num_vectors = [%d], cm_info_runtime_ptr[master_vector_indx].num_dynamic_filled = [%d], cm_info_runtime_ptr[master_vector_indx].group_num_vectors = [%d], flag %d", master_vector_indx, cm_info_runtime_ptr[master_vector_indx].dyn_num_vectors, cm_info_runtime_ptr[master_vector_indx].num_dynamic_filled, cm_info_runtime_ptr[master_vector_indx].group_num_vectors, new_vector_count);
          NSTL3(NULL, NULL, "AFTER: master_vector_indx = [%d], cm_info_runtime_ptr[master_vector_indx].dyn_num_vectors = [%d], cm_info_runtime_ptr[master_vector_indx].num_dynamic_filled = [%d], cm_info_runtime_ptr[master_vector_indx].group_num_vectors = [%d], flag %d", master_vector_indx, cm_info_runtime_ptr[master_vector_indx].dyn_num_vectors, cm_info_runtime_ptr[master_vector_indx].num_dynamic_filled, cm_info_runtime_ptr[master_vector_indx].group_num_vectors, new_vector_count);
        }

        //set copy flag
        dyn_cm_info_runtime_ptr[dyn_idx].runtime_and_copy_flag |= RUNTIME_DVM_CM_MERGED;

        NSDL2_MON(NULL, NULL, "Copied, dyn_cm_info_runtime_ptr[dyn_idx].runtime_and_copy_flag [%d], dyn_cm_info_runtime_ptr[dyn_idx].fd = [%d]. , flag %d", dyn_cm_info_runtime_ptr[dyn_idx].runtime_and_copy_flag, dyn_cm_info_runtime_ptr[dyn_idx].fd, new_vector_count);
      } //strcmp end
 
      NSDL2_MON(NULL, NULL, "increment dyn cm table master_vector_indx.");

      //increment dyn cm table master_vector_indx
      if(dyn_cm_info_runtime_ptr[dyn_idx].dyn_num_vectors == 0)
        dyn_idx = dyn_idx + 1;
      else
        dyn_idx = dyn_idx + dyn_cm_info_runtime_ptr[dyn_idx].dyn_num_vectors;
 
      NSDL2_MON(NULL, NULL, "dyn_idx = %d, flag = %d", dyn_idx, new_vector_count);
     } //NEW VECTOR for loop end

    //INCREMENT OLD TABLE INDEX
    if(cm_info_ptr[old_dyn_cm_idx].dyn_num_vectors == 0)
      old_dyn_cm_idx = old_dyn_cm_idx + 1;
    else
      old_dyn_cm_idx = old_dyn_cm_idx + old_cm_dyn_vector_count;

    //reset 
    new_vector_count = 0;

  } //while loop end

  // COPY LEFT RUNTIME DVM->CM MONITORS
  //this will copy newly added dynamic vector monitor converted into custom monitors
  for(dyn_idx = 0; dyn_idx < total_dyn_cm_runtime_entries;)
  {
    relative_dyn_idx = 0;
    NSTL2(NULL, NULL, "Going to copy DVM converted to CM into runtime structure. dyn_idx = %d, dyn_cm_info_runtime_ptr[dyn_idx].runtime_and_copy_flag = %d, dyn_cm_info_runtime_ptr[dyn_idx].vector_name = %s", dyn_idx, dyn_cm_info_runtime_ptr[dyn_idx].runtime_and_copy_flag, dyn_cm_info_runtime_ptr[dyn_idx].vector_name);

    NSDL2_MON(NULL, NULL, "For loop starts. dyn_idx = %d, total_dyn_cm_runtime_entries = %d", dyn_idx, total_dyn_cm_runtime_entries);
    if(!(dyn_cm_info_runtime_ptr[dyn_idx].runtime_and_copy_flag & RUNTIME_DVM_CM_MERGED)) //not copied yet
    {
      NSDL2_MON(NULL, NULL, "Merging...dyn_cm_info_runtime_ptr[dyn_idx].runtime_and_copy_flag %d.", dyn_cm_info_runtime_ptr[dyn_idx].runtime_and_copy_flag);

      merge_into_runtime_cm_table(dyn_cm_info_runtime_ptr, dyn_idx, cm_info_runtime_ptr, total_cm_runtime_entries, dyn_cm_info_runtime_ptr[dyn_idx].dyn_num_vectors, 0, &relative_dyn_idx);

      dyn_cm_info_runtime_ptr[dyn_idx].runtime_and_copy_flag |= RUNTIME_DVM_CM_MERGED;
      NSDL2_MON(NULL, NULL, "Merging end & dyn_cm_info_runtime_ptr[dyn_idx].runtime_and_copy_flag %d.", dyn_cm_info_runtime_ptr[dyn_idx].runtime_and_copy_flag);
    }

   NSDL2_MON(NULL, NULL, " Incrementing dyn_idx %d", dyn_idx);

    //increment dyn cm table master_vector_indx
    if(dyn_cm_info_runtime_ptr[dyn_idx].dyn_num_vectors == 0)
      dyn_idx = dyn_idx + 1;
    else
      dyn_idx = dyn_idx + dyn_cm_info_runtime_ptr[dyn_idx].dyn_num_vectors;

   NSDL2_MON(NULL, NULL, " After Incrementing dyn_idx %d", dyn_idx);
  }

  if(is_dyn_mon_added_on_runtime)
  {
    qsort(&cm_info_runtime_ptr[g_dyn_cm_start_idx], total_cm_runtime_entries - g_dyn_cm_start_idx, sizeof(CM_info), dyn_cm_mon_sort_by_gdf_name);
  }

  initialize_dyn_mon_vector_groups(&cm_info_runtime_ptr[g_dyn_cm_start_idx], g_dyn_cm_start_idx, total_cm_runtime_entries);

  //once qsort done we need to reset netcloud monitor index in global variable, and qsort only executes on addition of new DVM
  if(is_dyn_mon_added_on_runtime)
  {
    //reset netcloud monitor cm_info table index in global variable
    for(i = g_dyn_cm_start_idx; i < total_cm_runtime_entries; i += cm_info_runtime_ptr[i].group_num_vectors)
    {
      if(cm_info_runtime_ptr[i].cs_port == -1)
      {
        nc_ip_data_mon_idx = i;
        break;
      } 
    }
  }

  //reset
  is_dyn_mon_added_on_runtime = 0;

  NSDL2_MON(NULL, NULL, "Method exited");
}*/

/*static void do_remove_add_select_and_reset_mon_gdf_tables()
{
  NSDL2_MON(NULL, NULL, "Method Called.");
  NSTL1(NULL, NULL, "Method Called.");

  int i;
  CM_info *local_cm_info_runtime_ptr = cm_info_runtime_ptr;
  for(i = 0; i < total_cm_runtime_entries; i++, local_cm_info_runtime_ptr++)
  {
    NSDL2_MON(NULL, NULL, "i = [%d], total_cm_runtime_entries  = [%d], local_cm_info_runtime_ptr->fd = [%d]", i, total_cm_runtime_entries, local_cm_info_runtime_ptr->fd);

    if(local_cm_info_runtime_ptr->fd > 0)  //fd positive
    {
      NSDL2_MON(NULL, NULL, "Remove select fd  = [%d]", local_cm_info_runtime_ptr->fd);
      //remove select to remove fd with old address of cm_info_ptr
      //remove_select_msg_com_con_ex(__FILE__, __LINE__, (char *)__FUNCTION__,local_cm_info_runtime_ptr->fd);

      NSDL2_MON(NULL, NULL, "Add select fd  = [%d]", local_cm_info_runtime_ptr->fd);
      //add select with new address of cm_info_ptr
      if (local_cm_info_runtime_ptr->conn_state == CM_CONNECTING)
        //add_select_msg_com_con_ex(__FILE__, __LINE__, (char *)__FUNCTION__,(char *)local_cm_info_runtime_ptr, local_cm_info_runtime_ptr->fd, EPOLLOUT | EPOLLIN | EPOLLERR | EPOLLHUP);
      else
        //add_select_msg_com_con_ex(__FILE__, __LINE__, (char *)__FUNCTION__,(char *)local_cm_info_runtime_ptr, local_cm_info_runtime_ptr->fd, EPOLLIN | EPOLLERR | EPOLLHUP);



      NSDL2_MON(NULL, NULL, "Done remove select & add select of fd  = [%d]", local_cm_info_runtime_ptr->fd);
    }
  }

  FREE_AND_MAKE_NULL(cm_info_ptr, "cm_info_ptr", -1);

  cm_info_ptr = cm_info_runtime_ptr; 

  total_cm_entries = total_cm_runtime_entries;   // total custom monitor enteries
  max_cm_entries = total_cm_runtime_entries;   // total custom monitor enteries

  cm_info_runtime_ptr = NULL;
  total_cm_runtime_entries = 0;                  // total custom monitor entries -> for runtime
  max_cm_runtime_entries = 0;                    // max custom monitor entries -> for runtime

  dyn_cm_info_runtime_ptr = NULL;
  total_dyn_cm_runtime_entries = 0;              // total dynamic -> custom monitor entries 
  max_dyn_cm_runtime_entries = 0;                // max dynamic -> custom monitor entries

  //free_gdf_tables(); //free gdf tables

  save_cm_info_runtime_ptr = NULL;
  total_save_cm_runtime_entries = 0;
  warning_no_vectors = 0;
  NSTL1(NULL, NULL, "Method End");
}*/

//Update dr table with new pointers, make new monitor nb connections & nb control connection on new servers if any
void make_connection_on_runtime()
{
  int i, ret;

  //ServerInfo *servers_list = NULL;
  //servers_list = (ServerInfo *) topolib_get_server_info();

  CM_info *local_cm_info_ptr =  NULL;
  NSDL2_MON(NULL, NULL, "Method called. g_total_aborted_cm_conn = [%d], total_monitor_list_entries = [%d]", g_total_aborted_cm_conn, total_monitor_list_entries);
  MLTL4(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Method called. g_total_aborted_cm_conn = [%d], total_monitor_list_entries = [%d]", g_total_aborted_cm_conn, total_monitor_list_entries);
  //reset g_total_aborted_cm_conn
  g_total_aborted_cm_conn = 0;
  
  /*FD not negative do (a) remove select (b) add select
    FD negative  :
     (a) Add monitor for disaster recovery (DR). Update complete DR from indx 0.
          Note: If old monitor recovery count exceeds then do not add this monitor in DR         
     (b) Make sure if already some monitor in DR then old monitor should not replace.
       b.1> Reset 'g_total_aborted_cm_conn' & update complete dr from indx 0.
       b.2> Because DR monitor should retry immediately instead of next deliver report hence Make connection for monitors whose runtime flag is set, if connection established successfully then remove this moinitor from DR & reset g_total_aborted_cm_conn accordingly.
     (c) After above step, DR will have old failed monitor + runtime failed monitor.  */
 
  for(i = 0; i < total_monitor_list_entries; i++)
  {
    local_cm_info_ptr = monitor_list_ptr[i].cm_info_mon_conn_ptr;
    if(local_cm_info_ptr->cs_port == -1)  //For NetCloud Monitor port will be -1
      continue;  

    if(local_cm_info_ptr->is_monitor_registered == MONITOR_REGISTRATION) 
      continue;

    NSDL2_MON(NULL, NULL, "i = [%d], total_monitor_list_entries  = [%d], local_cm_info_ptr->fd = [%d]", i, total_monitor_list_entries, local_cm_info_ptr->fd);

    if(local_cm_info_ptr->fd >= 0)  //fd positive
    {
      NSDL3_MON(NULL, NULL, "DO NOTHING...  fd  = [%d]", local_cm_info_ptr->fd);
    }
    else //fd negative
    {
      NSDL2_MON(NULL, NULL, "local_cm_info_ptr->cm_retry_attempts = [%d]", local_cm_info_ptr->cm_retry_attempts);

      //Check if old monitor recovery count exceeds or not, if not then add for recovery
      //Runtime added monitor & old monitor not added for dr => both will have cm_retry_attempts '-1'.
      //Only old monitors added for recovery will have cm_retry_attempts greater than 0.
      if((local_cm_info_ptr->cm_retry_attempts > 0)) //old monitors
      {
        if(g_total_aborted_cm_conn >= max_size_for_dr_table)
        {
          MLTL3(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Reached maximum limit of DR table entry. So cannot allocate any more memory. Hence returning. cm_info->gdf_name = %s, CM_info->monitor_name = %s.", local_cm_info_ptr->gdf_name, local_cm_info_ptr->monitor_name);
          continue;
        }

        if(g_total_aborted_cm_conn >= max_dr_table_entries)
        {
          MY_REALLOC_AND_MEMSET(cm_dr_table, ((max_dr_table_entries + delta_size_for_dr_table) * sizeof(CM_info *)), (max_dr_table_entries * sizeof(CM_info *)), "Reallocation of DR table", -1);
          max_dr_table_entries += delta_size_for_dr_table;
        
          MLTL3(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "DR table has been reallocated by DELTA(%d) entries. Now new size will %d", delta_size_for_dr_table, max_dr_table_entries);
        }

        cm_dr_table[g_total_aborted_cm_conn] = local_cm_info_ptr;
        g_total_aborted_cm_conn++;
 
        //If any monitor's conn_state is CM_DELETED and (retry_attempt > 0), then it means it is reused monitor.
        if(local_cm_info_ptr->conn_state == CM_DELETED)
          local_cm_info_ptr->conn_state = CM_INIT;
      }
      else if ((local_cm_info_ptr->cm_retry_attempts == -1) && (local_cm_info_ptr->flags & RUNTIME_ADDED_MONITOR)) //runtime monitors first time
      {
        if(topo_info[topo_idx].topo_tier_info[local_cm_info_ptr->tier_index].topo_server[local_cm_info_ptr->server_index].used_row != -1)
        {
          if(topo_info[topo_idx].topo_tier_info[local_cm_info_ptr->tier_index].topo_server[local_cm_info_ptr->server_index].server_ptr->topo_servers_list->cntrl_conn_state & CTRL_CONN_ERROR)
          {
            NSDL2_MON(NULL,NULL, "Skipping making connection on Server (%s) for monitor gdf (%s) as there was an error occured while making control connection.", topo_info[topo_idx].topo_tier_info[local_cm_info_ptr->tier_index].topo_server[local_cm_info_ptr->server_index].server_disp_name, local_cm_info_ptr->gdf_name);
            continue;
          }

          //ret = cm_make_nb_conn(cm_dr_table[i], 1);
          ret = cm_make_nb_conn(local_cm_info_ptr, 1);
          if(ret == -1)
          {
            cm_handle_err_case(local_cm_info_ptr, CM_NOT_REMOVE_FROM_EPOLL);
            //cm_handle_err_case(cm_dr_table[i], CM_NOT_REMOVE_FROM_EPOLL);
            cm_update_dr_table(local_cm_info_ptr);
          } 
        } 
      }
    }
  }//end for loop
}

int set_counters_for_vectors()
{
  CM_info *cmptr = NULL;
  int i = 0;
  for(i=0; i< total_monitor_list_entries; i++)
  {
    cmptr=monitor_list_ptr[i].cm_info_mon_conn_ptr;
    //CM_vector_info *vector_list=cmptr->vector_list;
    if(!(cmptr->flags & ALL_VECTOR_DELETED))
    {
      if(cmptr->new_vector_first_index >= 0)
	cmptr->new_vector_first_index = -1;

      cmptr->total_deleted_vectors = 0;
    }
  }
  new_monitor_first_idx = -1;
  return 0;
}



int set_dvm_mapping_table()
{
  int i;   // k;   , relative_dyn_idx = 0;
  CM_info *cm_ptr;

  for(i = 0; i < total_monitor_list_entries; i++)
  {
    cm_ptr = monitor_list_ptr[i].cm_info_mon_conn_ptr;

    MLTL4(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Gdf name for i = %d is = %s", i, cm_ptr->gdf_name);
    MLTL4(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Monitor regis for i = %d is = %d", i, cm_ptr->is_monitor_registered);
    
    if(cm_ptr->is_monitor_registered == MONITOR_REGISTRATION)  
    { 
      if(strstr(cm_ptr->gdf_name,"cm_hm_ns_tier_server_count.gdf") != NULL)
      {
        g_tier_server_count_index=i;
        MLTL3(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Index for cm_hm_ns_tier_server_count.gdf = %d", g_tier_server_count_index); 
      }
    } 

    if(monitor_list_ptr[i].is_dynamic)
    {
      if(cm_ptr->cs_port == -1)
        nc_ip_data_mon_idx = i;
    }

    //Setting index for mon_id_map_table. This table is used in case of Outbound. 
    //Since CM_info pointer will not be changed throughout the test now, we will be saving its pointer for reverse mapping with mon_id_map_table. This change will be done in 4.1.14.
    if(is_outbound_connection_enabled)
    {
      if(cm_ptr->mon_id >= 0)
        mon_id_map_table[cm_ptr->mon_id].mon_index = i;
    }

    //resetting elements
    if(!g_delete_vec_freq_cntr)
    {
      cm_ptr->total_deleted_vectors = 0;
    }

    cm_ptr->total_reused_vectors = 0;
  }
  return 0;
}

/*This func will de following task:
 *free DVM monitor struct if all monitors processed
 * merge all the CM's tables
 * do remove/select for all the monitors whose fd is +ve
 * free all the tables related to monitor & gdf, reset all the global counters
 * set cm_info_ptr
 * parse all the gdf's
 * initialize & set all the tables related to gdf again, set all the global counters
 * switch partition, if partition enabled
 * reset complete dr table again because tables merging changes all the pointers,
 * make nb connections for runtime added monitors if fail add in dr table*/
static void handle_runtime_cm_conn()
{
  int ret = -1;


  // need to call this function once all runtime addition deletion done for this particular interval, else will miss DVM->CM vectors because running DVM just above
  // skipping merging of mon tables
  if(!(g_tsdb_configuration_flag & TSDB_MODE))
  {
    qsort(monitor_list_ptr, total_monitor_list_entries, sizeof(Monitor_list), check_if_cm_vector);
    set_no_of_monitors();
    MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                "Creating diff file. total_deleted_vectors = %d",
                       total_deleted_vectors); 

  //This is the case when we get NA_gdf flag on in which RTC is done in NA type gdf monitor.
  //and only when we return 1 if we get g_monitor_runtime_changes off i.e rtc is not applied on other monitor which have no NA_type gdf.
    if((g_monitor_runtime_changes_NA_gdf == 0) && (g_monitor_runtime_changes == 0) && (g_vector_runtime_changes == 0) && (monitor_deleted_on_runtime < 2))
    {
      MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Creating Diff file in case of deletion");
      create_diff_file();

      if(g_rtg_rewrite)
        return;

      process_gdf_wrapper(1); // TODO THINK TO AVOID GDF PROCESSING IN CASE DELETE / REUSE

      // make_conn_for_reused_monitors(1);
 
      return;
    }

    if(!g_monitor_runtime_changes_NA_gdf && !g_vector_runtime_changes && !g_monitor_runtime_changes && (monitor_deleted_on_runtime != 2) && (!is_rtc_applied_for_dyn_objs()))
    {
      NSDL2_MON(NULL, NULL, "Total runtime monitor count is zero. monitor_deleted_on_runtime [%d]. Returning...", monitor_deleted_on_runtime);

      MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                "Total runtime monitor count is zero. monitor_deleted_on_runtime [%d]. Returning...", monitor_deleted_on_runtime);
      return;
    }


    ret = create_diff_file();
  }
  else
  {
    ret = set_counters_for_vectors();
  }

  //TODO: MSR -> Reset new_vector_first index and rest new monitors index qand sort

  //make_conn_for_reused_monitors(0);
  /**********************/

  //to handle issue if parent restarted for some reason after adding runtime vectors in runtime table then parent & new vectors will have different fd, & merging is done if gdf name & fd is same , hence we are saving parent ptr when new vector received & updating new vector fd from parent fd just before merging
  //update_runtime_vectors_fd();




  if(ret == 0) //in case of only 'Warning: no vectors' do not create new testrun.gdf
  {
    //Parse all the gdf & fill respective structures & set respective global variables
    if(((g_tsdb_configuration_flag & TSDB_MODE) && (g_gdf_processing_flag == PROCESS_REQUIRED)))
    {
      MLTL4(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
	                "g_gdf_processing_flag %d before process_gdf ", g_gdf_processing_flag);
      qsort(monitor_list_ptr, total_monitor_list_entries, sizeof(Monitor_list), check_if_cm_vector);
      set_no_of_monitors();
      process_gdf_wrapper(1);

      g_gdf_processing_flag = -1;
      MLTL4(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
	                "g_gdf_processing_flag %d after process_gdf ", g_gdf_processing_flag);
     // g_gdf_processing_flag = COMPLETED_PROCESSED;
    }
    else if (!(g_tsdb_configuration_flag & TSDB_MODE))
    {
      process_gdf_wrapper(1);
    }
    if(monitor_added_on_runtime)  //This only happen when a new monitor is added in structure.
    {
      check_dup_grp_id_in_CM_and_DVM();
      monitor_added_on_runtime = 0;
    }
  }

  set_dvm_mapping_table();
  if(is_outbound_connection_enabled)
  {
    make_mon_msg_and_send_to_NDC(1, 0, 1);
  }
  else
  {
    make_connection_on_runtime();
  }

  MLTL4(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Method exit, Handle Runtime Monitors/Vectors done!!, New RTG Packet Size = %ld", msg_data_size);
}

static inline void dvm_make_msg(DynamicVectorMonitorInfo *dynamic_vector_monitor_ptr, char *msg_buf, int frequency, int *msg_len)
{
  char owner_name[MAX_STRING_LENGTH];
  char group_name[MAX_STRING_LENGTH];
  int testidx = start_testidx;

  if(dynamic_vector_monitor_ptr->origin_cmon == NULL)
  {
     *msg_len = sprintf(msg_buf, "init_check_monitor:OWNER=%s;GROUP=%s;MON_NAME=%s;MON_PGM_NAME=%s;MON_PGM_ARGS=%s;MON_OPTION=%d;"                          "MON_ACCESS=%d;MON_TIMEOUT=%d;MON_REMOTE_IP=%s;MON_REMOTE_USER_NAME=%s;MON_REMOTE_PASSWD=%s;"
                       "MON_FREQUENCY=%d;MON_TEST_RUN=%d;MON_NS_SERVER_NAME=%s;MON_CAVMON_SERVER_NAME=%s;MON_NS_FTP_USER=%s;"
                       "MON_NS_FTP_PASSWORD=%s;MON_NS_WDIR=%s;MON_NS_VER=%s;MON_VECTOR_SEPARATOR=%c;MON_PARTITION_IDX=%lld;NUM_TX=%d\n",
                        nslb_get_owner(owner_name), nslb_get_group(group_name), dynamic_vector_monitor_ptr->monitor_name,
                       dynamic_vector_monitor_ptr->pgm_name_for_vectors, dynamic_vector_monitor_ptr->pgm_args_for_vectors,
                       RUN_ONLY_ONCE_OPTION, dynamic_vector_monitor_ptr->access, dynamic_vector_monitor_timeout,
                       dynamic_vector_monitor_ptr->rem_ip, dynamic_vector_monitor_ptr->rem_username,
                       dynamic_vector_monitor_ptr->rem_password, dynamic_vector_monitor_ptr->frequency, testidx, g_cavinfo.NSAdminIP,
                       dynamic_vector_monitor_ptr->cavmon_ip, get_ftp_user_name(), get_ftp_password(),
                       g_ns_wdir, ns_version, global_settings->hierarchical_view_vector_separator, g_start_partition_idx, total_tx_entries);
  }
  else
  {
    *msg_len = sprintf(msg_buf, "init_check_monitor:OWNER=%s;GROUP=%s;MON_NAME=%s;MON_PGM_NAME=%s;MON_PGM_ARGS=%s;MON_OPTION=%d;"                          "MON_ACCESS=%d;MON_TIMEOUT=%d;MON_REMOTE_IP=%s;MON_REMOTE_USER_NAME=%s;MON_REMOTE_PASSWD=%s;"
                     "MON_FREQUENCY=%d;MON_TEST_RUN=%d;MON_NS_SERVER_NAME=%s;MON_CAVMON_SERVER_NAME=%s;MON_NS_FTP_USER=%s;"
                     "MON_NS_FTP_PASSWORD=%s;MON_NS_WDIR=%s;MON_NS_VER=%s;ORIGIN_CMON=%s;MON_VECTOR_SEPARATOR=%c;MON_PARTITION_IDX=%lld;NUM_TX=%d\n",
                     nslb_get_owner(owner_name), nslb_get_group(group_name), dynamic_vector_monitor_ptr->monitor_name,
                     dynamic_vector_monitor_ptr->pgm_name_for_vectors, dynamic_vector_monitor_ptr->pgm_args_for_vectors,
                     RUN_ONLY_ONCE_OPTION, dynamic_vector_monitor_ptr->access, dynamic_vector_monitor_timeout,
                     dynamic_vector_monitor_ptr->rem_ip, dynamic_vector_monitor_ptr->rem_username,
                     dynamic_vector_monitor_ptr->rem_password, dynamic_vector_monitor_ptr->frequency, testidx, g_cavinfo.NSAdminIP,
                     dynamic_vector_monitor_ptr->cavmon_ip, get_ftp_user_name(), get_ftp_password(),
                     g_ns_wdir, ns_version, dynamic_vector_monitor_ptr->origin_cmon,
                     global_settings->hierarchical_view_vector_separator, g_start_partition_idx, total_tx_entries);
  }
}

static inline void dvm_free_partial_buf(DynamicVectorMonitorInfo *dynamic_vector_monitor_ptr)
{
  NSDL2_MON(NULL, NULL, "Method called, dynamic_vector_monitor_ptr = %p partial_buffer %p", 
                         dynamic_vector_monitor_ptr, dynamic_vector_monitor_ptr->partial_buffer);

  FREE_AND_MAKE_NULL(dynamic_vector_monitor_ptr->partial_buffer, "dynamic_vector_monitor_ptr->partial_buffer", -1);
  dynamic_vector_monitor_ptr->send_offset = 0;
  dynamic_vector_monitor_ptr->bytes_remaining = 0;
  
}
/* Handle first time & partial write*/
/*static int dvm_send_msg(DynamicVectorMonitorInfo *dynamic_vector_monitor_ptr)
{
  char *buf_ptr;
  int bytes_sent;
  char SendMsg[MAX_MONITOR_BUFFER_SIZE];

  NSDL2_MON(NULL, NULL, "Method called, dynamic_vector_monitor_ptr = %p partial_buffer %p", 
                         dynamic_vector_monitor_ptr, dynamic_vector_monitor_ptr->partial_buffer);

  if(dynamic_vector_monitor_ptr->partial_buffer == NULL) //First time 
  {
    buf_ptr = SendMsg;
    dvm_make_msg(dynamic_vector_monitor_ptr, buf_ptr, global_settings->progress_secs, &dynamic_vector_monitor_ptr->bytes_remaining); 
  }
  else //If there is partial send
  {
    buf_ptr = dynamic_vector_monitor_ptr->partial_buffer;
  }

  // Send MSG to CMON
  while(dynamic_vector_monitor_ptr->bytes_remaining)
  {
    NSDL2_MON(NULL, NULL, "Sending DVM MSG: dynamic_vector_monitor_ptr->fd = %d, remaining_bytes = %d,  send_offset = %d, buf = [%s]", 
                           dynamic_vector_monitor_ptr->fd, dynamic_vector_monitor_ptr->bytes_remaining,
                           dynamic_vector_monitor_ptr->send_offset,
                           buf_ptr + dynamic_vector_monitor_ptr->send_offset);

    if((bytes_sent = send(dynamic_vector_monitor_ptr->fd, buf_ptr + dynamic_vector_monitor_ptr->send_offset, dynamic_vector_monitor_ptr->bytes_remaining, 0)) < 0)
    {
      NSDL2_MON(NULL, NULL, "bytes_sent = %d, errno = %d, error = %s", bytes_sent, errno, strerror(errno));
      if(errno == EAGAIN) //If message send partially
      {
        if(dynamic_vector_monitor_ptr->partial_buffer == NULL)
        {
          MY_MALLOC(dynamic_vector_monitor_ptr->partial_buffer, (dynamic_vector_monitor_ptr->bytes_remaining + 1), "DVM Partial Send", -1);
        strcpy(dynamic_vector_monitor_ptr->partial_buffer, buf_ptr + dynamic_vector_monitor_ptr->send_offset);
          dynamic_vector_monitor_ptr->send_offset = 0;
        }
        dynamic_vector_monitor_ptr->conn_state = DVM_SENDING;

        NSDL1_MON(NULL, NULL, "After Partial Send: dynamic_vector_monitor_ptr->fd = %d, remaining_bytes = %d, send_offset = %d, remaining buf = [%s]",
                               dynamic_vector_monitor_ptr->fd, dynamic_vector_monitor_ptr->bytes_remaining, 
                               dynamic_vector_monitor_ptr->send_offset, 
                               dynamic_vector_monitor_ptr->partial_buffer + dynamic_vector_monitor_ptr->send_offset);
        return 0;
      }
      if(errno == EINTR) //If any intrept occurr
      {
        NSDL1_MON(NULL, NULL, "Interrupted. Continuing...");
        continue;
      }
      else 
      {
        //sending msg failed for this monitor, close fd & send for nxt monitor
        ns_dynamic_vector_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, dynamic_vector_monitor_ptr,
                                           EID_DATAMON_ERROR, EVENT_MAJOR,
                                           "Sending DVM msg 'init_check_monitor' failed for %s. Error: send(): errno %d (%s)",
                                            dvm_to_str(dynamic_vector_monitor_ptr), errno, strerror(errno));
        dvm_handle_err_case(dynamic_vector_monitor_ptr);
        return -1;
      } 
    }

    NSDL2_MON(NULL, NULL, "bytes_sent = %d", bytes_sent);

    dynamic_vector_monitor_ptr->bytes_remaining -= bytes_sent;
    dynamic_vector_monitor_ptr->send_offset += bytes_sent; 
    
    // send should never return 0. even if it returns 0 it is not a error
  } //End while Loop 

  //At this pt messages sent completely
    NSDL2_MON(NULL, NULL, "DVM INIT MSG sent succefully for %s.", dvm_to_str(dynamic_vector_monitor_ptr));
    ns_dynamic_vector_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, dynamic_vector_monitor_ptr,
                                       EID_DATAMON_GENERAL, EVENT_INFO,
                                       "Sending DVM msg 'cm_init_monitor' successfull for %s.", dvm_to_str(dynamic_vector_monitor_ptr));

  //Free partial message buffer and reset send offset & bytes_remaining in dynamic_vector_monitor_ptr
  dvm_free_partial_buf(dynamic_vector_monitor_ptr);

  dynamic_vector_monitor_ptr->conn_state = DVM_WAITING_FOR_VECTORS;
    //mod_select_msg_com_con_ex(__FILE__, __LINE__, (char *)__FUNCTION__,(char *)dynamic_vector_monitor_ptr, dynamic_vector_monitor_ptr->fd, EPOLLIN | EPOLLERR | EPOLLHUP);

  return 0;
}*/


/*int dvm_make_nb_conn(DynamicVectorMonitorInfo *dynamic_vector_monitor_ptr)
{
  int con_state, s_port = 7891;
  char err_msg[1024] = "\0";
  char s_ip[512] = "\0";
 
  NSDL2_MON(NULL, NULL, "Method Called, dynamic_vector_monitor_ptr = %p", dynamic_vector_monitor_ptr);
  if((dynamic_vector_monitor_ptr->fd = nslb_nb_open_socket((AF_INET), err_msg)) < 0)
  {
    NSDL3_MON(NULL, NULL, "Error: problem in opening socket for %s", dvm_to_str(dynamic_vector_monitor_ptr));
    return -1;
  }

  //set timestamp
  dynamic_vector_monitor_ptr->timestamp = time(NULL);
  NSDL3_MON(NULL, NULL, "Set dvm monitor start timestamp [%d]", dynamic_vector_monitor_ptr->timestamp);

  //Adding here beacuse on any error we are removing fd blindly in dvm_handle_err_case()
  //Add this socket fd in epoll and wait for an event
  //If EPOLLOUT event comes then send messages
  //TODO: add comments for EPOLLET
  //add_select_msg_com_con_ex(__FILE__, __LINE__, (char *)__FUNCTION__,(char *)dynamic_vector_monitor_ptr, dynamic_vector_monitor_ptr->fd, EPOLLOUT | EPOLLERR | EPOLLHUP);
  
  DVM_SET_AGENT_FOR_VECTORS(dynamic_vector_monitor_ptr, s_ip, s_port);
  NSDL3_MON(NULL, NULL, "Socket opened successfully so making connection for fd %d to server ip =%s, port = %d", 
                         dynamic_vector_monitor_ptr->fd, s_ip, s_port);
  //Socket opened successfully so making connection
  int con_ret = nslb_nb_connect(dynamic_vector_monitor_ptr->fd, s_ip, s_port, &con_state, err_msg);
  NSDL3_MON(NULL, NULL, " con_ret = %d", con_ret);
  if(con_ret < 0)
  {
    NSDL3_MON(NULL, NULL, "%s", err_msg);
    ns_dynamic_vector_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, dynamic_vector_monitor_ptr,
                                         EID_DATAMON_GENERAL, EVENT_INFO,
                                         "Connection failed for %s. %s", dvm_to_str(dynamic_vector_monitor_ptr), err_msg);
    //if((strstr(err_msg, "gethostbyname")) && (strstr(err_msg, "Unknown server error")))
    //  dynamic_vector_monitor_ptr->conn_state = DVM_GETHOSTBYNAME_ERROR;  
    
    dvm_handle_err_case(dynamic_vector_monitor_ptr);
    return -1;
  }
  else if(con_ret == 0)
  {
    dynamic_vector_monitor_ptr->conn_state = DVM_CONNECTED;
    ns_dynamic_vector_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, dynamic_vector_monitor_ptr,
                                         EID_DATAMON_GENERAL, EVENT_INFO,
                                         "Connection established for %s.", dvm_to_str(dynamic_vector_monitor_ptr));
  }
  else if(con_ret > 0) 
  { 
    if(con_state == NSLB_CON_CONNECTED)
      dynamic_vector_monitor_ptr->conn_state = DVM_CONNECTED;
   
    if(con_state == NSLB_CON_CONNECTING)
      dynamic_vector_monitor_ptr->conn_state = DVM_CONNECTING;

    NSDL3_MON(NULL, NULL, " con_ret = %d, con_state = [%d], dynamic_vector_monitor_ptr->conn_state = [%d]", con_ret, con_state, dynamic_vector_monitor_ptr->conn_state);

  }

  //If CONNECTED -> send msg, add in epollin, else already in epollout
  if(con_state == NSLB_CON_CONNECTED)
  {
  
    NSDL3_MON(NULL, NULL, "Build & Send Message");

    if(dvm_send_msg(dynamic_vector_monitor_ptr) == -1)
      return -1; 
  }
  else
  {
    NSDL3_MON(NULL, NULL, " STATE IS CONNECTING.");
  }
  return 0;
}*/


//This func is called from deliver report
void runtime_change_mon_dr()
{
  NSDL2_MON(NULL, NULL, "Method Called"); 

  if(g_enable_delete_vec_freq)
    ns_cm_monitor_log(EL_F, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, "g_delete_vec_freq_cntr = %d , total_waiting_deleted_vectors = %d\n", g_delete_vec_freq_cntr, total_waiting_deleted_vectors);

  //Handling custom monitor
  //Starting custom monitors and dynamic vector monitors if vectors are received
  //if(cm_info_runtime_ptr != NULL || dynamic_vector_monitor_info_ptr != NULL || dyn_cm_info_runtime_ptr != NULL || monitor_deleted_on_runtime == 1)
  if(g_vector_runtime_changes || g_monitor_runtime_changes_NA_gdf || g_monitor_runtime_changes || monitor_deleted_on_runtime == 1 || is_rtc_applied_for_dyn_objs() || total_deleted_vectors > 0)
  {
    MLTL1(EL_F, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, "There are some runtime changed custom monitors, g_vector_runtime_changes %d, g_monitor_runtime_changes %d, g_monitor_runtime_changes_NA_gdf %d, monitor_deleted_on_runtime %d, max_mapping_tbl_row_entries %d, total_mapping_tbl_row_entries %d, total_dummy_dvm_mapping_tbl_row_entries %d, g_mon_id = %d, max_mon_id_entries = %d, total_deleted_vectors = %d", g_vector_runtime_changes, g_monitor_runtime_changes, g_monitor_runtime_changes_NA_gdf, monitor_deleted_on_runtime, max_mapping_tbl_row_entries, total_mapping_tbl_row_entries, total_dummy_dvm_mapping_tbl_row_entries, g_mon_id, max_mon_id_entries, total_deleted_vectors);

    if(global_settings->enable_health_monitor)
    {
      hm_data->deleted_vectors = total_deleted_vectors;
      //hm_data->reused_vectors = total_reused_vectors;
    }

    handle_runtime_cm_conn();

    monitor_deleted_on_runtime = 0;
    total_deleted_vectors = 0;

    if(global_settings->dynamic_cm_rt_table_mode)
      nslb_obj_hash_reset(dyn_cm_rt_hash_key);
  }

  NSDL3_MON(NULL, NULL, "Method Exited."); 
} 

/************************************** RUNTIME ADD/DELETE CUSTOM MONITOR CODE ENDS *******************************************/


/************************************** RUNTIME ADD/DELETE DVM MONITOR CODE STARTS *******************************************/

/* Open a socket 
 * Connect that socket  
 */

/*void dvm_handle_err_case(DynamicVectorMonitorInfo *dynamic_vector_monitor_ptr)
{
  //Free partial message buffer 
  dvm_free_partial_buf(dynamic_vector_monitor_ptr);
  
  if(dynamic_vector_monitor_ptr->fd < 0)
    return;

  //remove_select_msg_com_con_ex(__FILE__, __LINE__, (char *)__FUNCTION__,dynamic_vector_monitor_ptr->fd);

  CLOSE_FD(dynamic_vector_monitor_ptr->fd);


  dynamic_vector_monitor_ptr->conn_state = DVM_INIT;
}*/

//This method is called in following cases:
// 1. if connection was partial
// 2. if send was partial
/*int dvm_chk_conn_send_msg_to_cmon(void *ptr) 
{
  int s_port = 7891, con_state;
  //int send_msg_len = 0;
  //char SendMsg[MAX_MONITOR_BUFFER_SIZE] = "\0";
  char err_msg[1024] = "\0";
  char s_ip[512] = "\0";

  DynamicVectorMonitorInfo *dynamic_vector_monitor_ptr = (DynamicVectorMonitorInfo *) ptr;

  NSDL2_MON(NULL, NULL, "Method called, dynamic_vector_monitor_ptr = %p, dynamic_vector_monitor_ptr->fd = %d, dynamic_vector_monitor_ptr->conn_state = %d", 
                        dynamic_vector_monitor_ptr, dynamic_vector_monitor_ptr->fd, dynamic_vector_monitor_ptr->conn_state);

  //Check State first 
  //1. If is in DVM_CONNECTED then send message and change state to SENDING
  //2. If is in DVM_CONNECTING then again try to connect if connect then change state to DVM_CONNECTED other wise into DVM_CONNICTING 

  if(dynamic_vector_monitor_ptr->conn_state == DVM_CONNECTING)
  {
    //Again send connect request
    DVM_SET_AGENT_FOR_VECTORS(dynamic_vector_monitor_ptr, s_ip, s_port);
    NSDL3_MON(NULL, NULL, "Since connection is in DVM_CONNECTING so try to Reconnect for fd %d and to server ip = %s, port = %d", 
                           dynamic_vector_monitor_ptr->fd, s_ip, s_port);

    if(nslb_nb_connect(dynamic_vector_monitor_ptr->fd, s_ip, s_port, &con_state, err_msg) != 0 && 
      con_state != NSLB_CON_CONNECTED)
    {
      NSDL3_MON(NULL, NULL, "err_msg = %s", err_msg);
      ns_dynamic_vector_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, dynamic_vector_monitor_ptr,
                                       EID_DATAMON_GENERAL, EVENT_CRITICAL,
                                      "Retry connection failed for %s for retry attempt %d.", dvm_to_str(dynamic_vector_monitor_ptr), 
                                       dynamic_vector_monitor_ptr->dvm_retry_attempts);
 
      if(dynamic_vector_monitor_ptr->dvm_retry_attempts == dynamic_vector_monitor_retry_count)
      {
        NSDL3_MON(NULL, NULL, "Retry Count %d reached the max limit. Incrementing failed monitor counter.", dynamic_vector_monitor_retry_count - dynamic_vector_monitor_ptr->dvm_retry_attempts);
        num_failed_runtime_dyn_mon++;
      }
      dvm_handle_err_case(dynamic_vector_monitor_ptr);
      return -1;
    }
    dynamic_vector_monitor_ptr->conn_state = DVM_CONNECTED;
    ns_dynamic_vector_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, dynamic_vector_monitor_ptr, EID_DATAMON_GENERAL, EVENT_INFO,
                                  "Connection established for %s.", dvm_to_str(dynamic_vector_monitor_ptr));
   }

  NSDL3_MON(NULL, NULL, "Build & Send Message");

  if(dvm_send_msg(dynamic_vector_monitor_ptr) == -1)
    return -1;

  return 0;
}*/


/*static inline int check_results_of_dvm()
{
  int i;
  DynamicVectorMonitorInfo *dynamic_vector_monitor_ptr = dynamic_vector_monitor_info_ptr;
  
  failed_dynamic_vector_monitor[0] = '\0';
 
  for (i = 0; i < total_dynamic_vector_mon_entries; i++, dynamic_vector_monitor_ptr++)
  {
    NSDL3_MON(NULL, NULL, "DynamicVectorMonitor => %s", dvm_to_str(dynamic_vector_monitor_ptr));
    if(dynamic_vector_monitor_ptr->status == DYNAMIC_VECTOR_MONITOR_PASS)
    {
      NSDL4_MON(NULL, NULL, "Dynamic Vector Monitor '%s' passed", dynamic_vector_monitor_ptr->monitor_name);
      num_passed_runtime_dyn_mon++;
    }
    else    
    {
      char tmp[256 + 256]="\0";
      NSDL4_MON(NULL, NULL, "Dynamic Vector Monitor '%s' failed", dynamic_vector_monitor_ptr->monitor_name);
      sprintf(tmp, "%s: %s, ", dynamic_vector_monitor_ptr->monitor_name, error_msgs[dynamic_vector_monitor_ptr->status]);
      strcat(failed_dynamic_vector_monitor, tmp);
      //num_failed_runtime_dyn_mon++;
    }
  }

  NSDL2_MON(NULL, NULL, "Total Passed = %d, Total Failed = %d", num_passed_runtime_dyn_mon, num_failed_runtime_dyn_mon);

  if (num_passed_runtime_dyn_mon == total_dynamic_vector_mon_entries) 
  {
    NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_INFO,
		  	       __FILE__, (char*)__FUNCTION__,
			       "All Dynamic Vector monitors passed.");
    NSTL1_OUT(NULL, NULL, "All Dynamic Vector monitors passed.\n");
    return DYNAMIC_VECTOR_MONITOR_DONE_WITH_ALL_PASS;
  }
  else if(num_failed_runtime_dyn_mon == total_dynamic_vector_mon_entries) 
  {
    NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_MAJOR,
		  	       __FILE__, (char*)__FUNCTION__,
    			      "All Dynamic Vector monitors failed.");

    NSTL1_OUT(NULL, NULL, "All Dynamic Vector monitors failed.\n");
    return DYNAMIC_VECTOR_MONITOR_DONE_WITH_ALL_FAIL;
  }

  if(failed_dynamic_vector_monitor)
  { 
    char *comma_index;
    if((comma_index=rindex(failed_dynamic_vector_monitor, ',')) !=  NULL) 
        *comma_index=0;
  }

  NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_MAJOR,
		  	       __FILE__, (char*)__FUNCTION__,
			       "Following Dynamic Vector monitors failed %s",
			       failed_dynamic_vector_monitor);
  NSTL1_OUT(NULL, NULL, "\n\nFollowing Dynamic Vector monitors failed %s\n", failed_dynamic_vector_monitor); 
  return DYNAMIC_VECTOR_MONITOR_DONE_WITH_SOME_FAIL;
}*/


/*void handle_if_dvm_fd(void *ptr)
{             
  NSDL2_MON(NULL, NULL, "Method called. handle_if_dvm_fd()."); 

  DynamicVectorMonitorInfo *dynamic_vector_monitor_ptr = (DynamicVectorMonitorInfo *) ptr;
                
  read_output_from_create_server_to_get_vector_list(dynamic_vector_monitor_ptr, 1);

  //check_results_of_dvm();                
}*/           


/************************************** RUNTIME ADD/DELETE DVM MONITOR CODE ENDS *******************************************/




/************************************** RUNTIME ADD/DELETE MONITOR CODE ENDS *******************************************/




/************************************** MONITOR DUMP CODE STARTS *******************************************/

/*void global_variable_dump(FILE *fp)
{

  NSDL3_MON(NULL, NULL, " Method Called.");
  fprintf(fp, "\n----------------- Global variables Dump STARTS:----------------\n");

  fprintf(fp, "total_cm_entries = %d\ntotal_cm_runtime_entries = %d\ntotal_dyn_cm_runtime_entries = %d\n", total_cm_entries, total_cm_runtime_entries, total_dyn_cm_runtime_entries);
  fprintf(fp, "nv_vector_format = %d\ng_total_aborted_cm_conn = %d\ng_dyn_cm_start_idx = %d\nhpd_port = %d\npid_received_from_ndc = %d\ntotal_check_monitors_entries = %d", nv_vector_format, g_total_aborted_cm_conn, g_dyn_cm_start_idx, hpd_port, pid_received_from_ndc, total_check_monitors_entries);
 
  fprintf(fp, "\n----------------- Global variables Dump END:----------------\n");
  

}*/



void dump_monitor_table()
{
  FILE *fp = NULL;
  char filename[256] = {0};
  char date_string[100];

  NSDL3_MON(NULL, NULL, " Method Called.");

  //TODO MSR -> Recreate functions to dump cm_info as well as cm_vector_info 

  sprintf(filename, "%s/logs/%s/ns_logs/topology_structure_dump_file", g_ns_wdir, global_settings->tr_or_common_files);

  fp = fopen(filename, "a+");

  if(fp)
  {
    fprintf(fp, "%s\n", nslb_get_cur_date_time(date_string, 0));    

    tier_structure_dump(fp,topo_idx);      //Dumping of Tier.conf
 
    server_structure_dump(fp,topo_idx);    //Dumping of Server.conf file
  
    instance_structure_dump(fp,topo_idx);  //Dumping of Instance.conf file
 
    CLOSE_FP(fp);
  }
 
}

/************************************** MONITOR DUMP CODE ENDS *******************************************/





/************************ DIFF FILE CREATION ON RUNTIME ***************************/


//In case of HML this function also write the vector line in the hml_metrics structure.
//so we need not have to loop again in create_diff_file
static int get_vector_count_for_diff(CM_info *cm_ptr)
{
  int new_vector = 0, i = cm_ptr->monitor_list_idx,j=0;
  CM_info *local_cm_ptr = NULL;
  CM_vector_info *local_cm_vector_info = NULL;

  NSDL2_GDF(NULL, NULL, "Method called");
  while(1)
  {

    if(i >= total_monitor_list_entries)
     break;

    local_cm_ptr = monitor_list_ptr[i].cm_info_mon_conn_ptr;

    if(local_cm_ptr->flags & ALL_VECTOR_DELETED)
    {
      i = i + 1;
      continue;
    }

    //if((local_cm_ptr) && (!strcmp(local_cm_ptr->gdf_name, cm_ptr->gdf_name)) && (monitor_list_ptr[i].is_dynamic))
    if((local_cm_ptr) && (!strcmp(local_cm_ptr->gdf_name, cm_ptr->gdf_name)))
    {
      //new vector entry
      if(local_cm_ptr->new_vector_first_index != -1)
      {
        new_vector += (local_cm_ptr->total_vectors - local_cm_ptr->new_vector_first_index);
        if(global_settings->enable_hml_group_in_testrun_gdf)
        {
          for(j = local_cm_ptr->new_vector_first_index ; j < local_cm_ptr->total_vectors ; j++)
          {  
             local_cm_vector_info = &(local_cm_ptr->vector_list[j]);
             FILL_HML_VECTOR_LINE(local_cm_vector_info, -1, local_cm_ptr->metric_priority);
          } 
        }
      }
 
      //deleted vector
      if((g_delete_vec_freq_cntr == 0) || !(g_enable_delete_vec_freq))
      {
        new_vector += local_cm_ptr->total_deleted_vectors;
        if(global_settings->enable_hml_group_in_testrun_gdf)
        {
          for(j = 0 ; j < local_cm_ptr->total_deleted_vectors ; j++)
          {
            local_cm_vector_info = &(local_cm_ptr->vector_list[local_cm_ptr->deleted_vector[j]]);
            FILL_HML_VECTOR_LINE(local_cm_vector_info , -1, local_cm_ptr->metric_priority);
          }
        }
      }
      //reused_vector
      new_vector += local_cm_ptr->total_reused_vectors;
      if(global_settings->enable_hml_group_in_testrun_gdf)
      {
        for(j = 0 ; j < local_cm_ptr->total_reused_vectors ; j++)
        {
          local_cm_vector_info = &(local_cm_ptr->vector_list[local_cm_ptr->reused_vector[j]]);
          FILL_HML_VECTOR_LINE(local_cm_vector_info, -1, local_cm_ptr->metric_priority);
        }
      }
    }
    else
      break;

    i = i + 1;
  }

  return new_vector;
}
 
FILE *write_group_line(CM_info *cmptr, FILE *fp, int *index)
{
  int found = 0, n = -1, i;
  char printLine[MAX_LINE_LENGTH];
  char line[MAX_LINE_LENGTH];
  FILE *gdf_fp = NULL;
  Group_Info *local_grp_ptr = NULL;
  char *buffer[GDF_MAX_FIELDS];
  CM_info *ptr = NULL;
  int actual_graph_count;
  int num_actual_data;
  int tot_num_vectors = 0; // provide tot_reused_vec + tot_del_vec + tot_new_vec 
  int num_hml_graphs[MAX_METRIC_PRIORITY_LEVELS + 1] = {0};
  
  //search gdf in total_cm_runtime_entries
  for(n = cmptr->monitor_list_idx; n < total_monitor_list_entries; n++)
  {
    ptr = monitor_list_ptr[n].cm_info_mon_conn_ptr;

    if(!strcmp(cmptr->gdf_name, ptr->gdf_name))
    {
      if(ptr->gp_info_index >= 0)
      {
        local_grp_ptr = (group_data_ptr + ptr->gp_info_index);

        tot_num_vectors = get_vector_count_for_diff(ptr);

        if(tot_num_vectors == 0)
          return NULL; 
  
        if(global_settings->enable_hml_group_in_testrun_gdf)
        {
          FILL_HML_GROUP_LINE(local_grp_ptr);
        }
        else
        {
          sprintf(printLine,"%s|%s|%d|%s|%d|%d|%s|%s%s|%s",
          "Group", local_grp_ptr->group_name, local_grp_ptr->rpt_grp_id, "vector", local_grp_ptr->num_actual_graphs[MAX_METRIC_PRIORITY_LEVELS], tot_num_vectors, local_grp_ptr->groupMetric, STORE_PREFIX, local_grp_ptr->Hierarchy, local_grp_ptr->group_description);
        }

        found = 1;
        *index = n;
      }
      //If found in structure but gdf processing is not done, we will write in gdf file after opening gdf file.
      break;
    }
  }
  
   // read gdf file
  if((gdf_fp = fopen(cmptr->gdf_name, "r")) == NULL)
  {
    NSTL1_OUT(NULL, NULL, "Error in opening gdf file %s for monitor %s\n", cmptr->gdf_name, cmptr->monitor_name);
    NSTL1_OUT(NULL, NULL, "Dumping structure and generating core dump");
    dump_monitor_table();
    abort();
    perror("fopen");
    NS_EXIT(-1,CAV_ERR_1060018,cmptr->gdf_name, cmptr->monitor_name);
  }


  NSDL2_GDF(NULL, NULL, "found = %d", found);
  // If Group already not exist then read GDF file for this group and get Group line
  if(found != 1)
  {
    actual_graph_count = get_actual_no_of_graphs(cmptr->gdf_name, NULL, &num_actual_data, cmptr, num_hml_graphs);

    while (fgets(line, MAX_LINE_LENGTH, gdf_fp) != NULL)
    {
      line[strlen(line) - 1] = '\0';
      if(!(strncasecmp(line, "group|", strlen("group|"))))
      {
        get_tokens(line, buffer, "|", GDF_MAX_FIELDS);
         break;
      }
      else 
        continue;
    }
 
    tot_num_vectors = get_vector_count_for_diff(cmptr);
  
    if(global_settings->enable_hml_group_in_testrun_gdf)
    {
      if(hml_metrics) 
      {
        for(i = 0; i < MAX_METRIC_PRIORITY_LEVELS; i++)
        {
          NSDL3_GDF(NULL, NULL, "Fill HML Group Line (Runtime): num_vectors[%d] = %d, num_hml_graphs[%d] = %d",
                                 i, hml_metrics[i].num_vectors, i, num_hml_graphs[i]);
          if(hml_metrics[i].num_vectors)
            sprintf(hml_metrics[i].group_line, "Group|%s|%s:%c|%s|%d|%d|%s|%s%s|%s",
                   buffer[1], buffer[2], get_metric_priority_by_id(i), "vector",
                   num_hml_graphs[i], hml_metrics[i].num_vectors,
                   buffer[6], STORE_PREFIX, buffer[7], buffer[8]);
        }
      }
    }
    else
    {
      sprintf(printLine,"%s|%s|%d|%s|%d|%d|%s|%s%s|%s",
      "Group", buffer[1], atoi(buffer[2]), "vector", actual_graph_count, tot_num_vectors, buffer[6], STORE_PREFIX, buffer[7], buffer[8]);
    }
  }

  if(!global_settings->enable_hml_group_in_testrun_gdf)
    fprintf(fp, "%s\n", printLine);
  
  return gdf_fp;
}

/*void write_left_new_monitors(FILE *fp)
{
  int mon_idx, vec_idx;
  CM_info *cm_ptr = NULL;
  CM_vector_info *vector_list = NULL;

  for(mon_idx = new_monitor_first_idx; mon_idx < total_monitor_list_entries; mon_idx++)
  {
    cm_ptr = monitor_list_ptr[mon_idx].cm_info_mon_conn_ptr;
    vector_list = cm_ptr->vector_list;
    for(vec_idx=0; vec_idx<cm_ptr->total_vectors; vec_idx++)
    {
      if(vector_list[vec_idx].flags & WRITTEN)
        continue;
      if(vector_list[vec_idx].flags & MON_BREADCRUMB_SET)
        fprintf(fp, "%s %ld\n", vector_list[vec_idx].mon_breadcrumb, vector_list[vec_idx].rtg_index);
      else
        fprintf(fp, "%s %ld\n", vector_list[vec_idx].vector_name, vector_list[vec_idx].rtg_index);
      vector_list[vec_idx].flags |= WRITTEN;
    }
  }
  return; 
}

void write_new_monitors_of_same_group(char *gdf_name, FILE *fp)
{
  int mon_idx, vec_idx;
  CM_info *cm_ptr = NULL;
  CM_vector_info *vector_list = NULL;
  
  for(mon_idx = new_monitor_first_idx; mon_idx < total_monitor_list_entries; mon_idx++)
  {
    cm_ptr = monitor_list_ptr[mon_idx].cm_info_mon_conn_ptr;
    if((strcmp(cm_ptr->gdf_name, gdf_name)))
      continue;
    vector_list = cm_ptr->vector_list;
    for(vec_idx=0; vec_idx<cm_ptr->total_vectors; vec_idx++)
    {
      if(vector_list[vec_idx].flags & MON_BREADCRUMB_SET)
        fprintf(fp, "%s %ld\n", vector_list[vec_idx].mon_breadcrumb, vector_list[vec_idx].rtg_index);
      else
        fprintf(fp, "%s %ld\n", vector_list[vec_idx].vector_name, vector_list[vec_idx].rtg_index);
      vector_list[vec_idx].flags |= WRITTEN;
    }
  }
  return;
}*/


static int get_actual_vectors_num(int idx)
{
  int actual_num_vectors = 0, i; 
  
  NSDL2_GDF(NULL, NULL, "Method called, dynobj_idx = %d", idx);

  if(idx == NEW_OBJECT_DISCOVERY_TX || idx == NEW_OBJECT_DISCOVERY_TX_CUM){
    for(i = 0; i < sgrp_used_genrator_entries; i++)
      actual_num_vectors += g_tx_loc2norm_table[i].dyn_total_entries[idx-1];
    actual_num_vectors = actual_num_vectors + dynObjForGdf[idx].total;
  }
  else if(idx == NEW_OBJECT_DISCOVERY_SVR_IP){
    for(i = 0; i < sgrp_used_genrator_entries; i++)
      actual_num_vectors += g_svr_ip_loc2norm_table[i].dyn_total_entries;
    actual_num_vectors = actual_num_vectors + dynObjForGdf[idx].total;
  }
  else if(idx == NEW_OBJECT_DISCOVERY_STATUS_CODE){
    for(i = 0; i < sgrp_used_genrator_entries; i++)
      actual_num_vectors += g_http_status_code_loc2norm_table[i].dyn_total_entries;
    actual_num_vectors = actual_num_vectors + dynObjForGdf[idx].total;
  }
  else if(idx == NEW_OBJECT_DISCOVERY_TCP_CLIENT_FAILURES){
    for(i = 0; i < sgrp_used_genrator_entries; i++)
      actual_num_vectors += g_tcp_clinet_errs_loc2normtbl[i].tot_dyn_entries[idx - 1];
    actual_num_vectors = actual_num_vectors + dynObjForGdf[idx].total;
  }
  else
    actual_num_vectors = (sgrp_used_genrator_entries+1) * dynObjForGdf[idx].total;
  
  NSDL2_GDF(NULL, NULL, "actual_num_vectors = %d", actual_num_vectors);  
  return actual_num_vectors;
}

void write_group(FILE *fp, int idx, int gp_info_idx)
{
  char printLine[2*MAX_LINE_LENGTH];
  char line[MAX_LINE_LENGTH];
  char *buffer[GDF_MAX_FIELDS];
  Group_Info *local_grp_ptr = NULL;
  FILE *gdf_fp = NULL;
  char *dyn_obj_name = NULL;
  char groupMetric[MAX_LINE_LENGTH];
  int actual_num_vectors = 0;
 
  NSDL2_GDF(NULL, NULL, "Method called, dynobj_idx = %d, gp_info_idx = %d", idx, gp_info_idx);

  actual_num_vectors = get_actual_vectors_num(idx);
  
  if(gp_info_idx > 0)
  {
    local_grp_ptr = (group_data_ptr + gp_info_idx);

    sprintf(printLine,"%s|%s|%d|%s|%d|%d|%s|%s%s|%s",
    "Group", local_grp_ptr->group_name, local_grp_ptr->rpt_grp_id, "vector", local_grp_ptr->num_graphs, actual_num_vectors, local_grp_ptr->groupMetric, STORE_PREFIX, local_grp_ptr->Hierarchy, local_grp_ptr->group_description);
  }
  else
  {
    gdf_fp = open_gdf(dynObjForGdf[idx].gdf_name);
    /*In case of NC,  pure dynamic transaction or dynamic server ip, hierarchy was not correct in diff file and in online mode it is read from    testrun.gdf.diff*/     
    if(idx == NEW_OBJECT_DISCOVERY_TX || idx == NEW_OBJECT_DISCOVERY_TX_CUM)
      dyn_obj_name = "Transaction";
    else if(idx == NEW_OBJECT_DISCOVERY_SVR_IP)
      dyn_obj_name = "HOSTNAME>IP";
    else if(idx == NEW_OBJECT_DISCOVERY_RBU_DOMAIN)
      dyn_obj_name = "Group>Page>Domain";
    else if(idx == NEW_OBJECT_DISCOVERY_STATUS_CODE)
      dyn_obj_name = "StatusCode";
    else if(idx == NEW_OBJECT_DISCOVERY_TCP_CLIENT_FAILURES)
      dyn_obj_name = "TCPClientFailures";

    while (fgets(line, MAX_LINE_LENGTH, gdf_fp) != NULL)
    {
      line[strlen(line) - 1] = '\0';
      if(!(strncasecmp(line, "group|", strlen("group|"))))
      {
        get_tokens(line, buffer, "|", GDF_MAX_FIELDS);
        if(loader_opcode == MASTER_LOADER && dyn_obj_name){
          if ((atoi(buffer[2]) == 109) && (strcasecmp(g_cavinfo.SUB_CONFIG, "NVSM") == 0))  //Domain Stat
            strcpy(groupMetric, "NetVision SM Metrics");
          else
            strcpy(groupMetric, buffer[6]);
          
          sprintf(printLine,"%s|%s|%d|%s|%d|%d|%s|%sController>Generator>%s|%s",
          "Group", buffer[1], atoi(buffer[2]), "vector", atoi(buffer[4]), actual_num_vectors,
          groupMetric, STORE_PREFIX, dyn_obj_name, buffer[8]);
        }
        else{
          if ((atoi(buffer[2]) == 109) && (strcasecmp(g_cavinfo.SUB_CONFIG, "NVSM") == 0))  //Domain Stat
            strcpy(groupMetric, "NetVision SM Metrics");
          else
            strcpy(groupMetric, buffer[6]);
         
          sprintf(printLine,"%s|%s|%d|%s|%d|%d|%s|%s%s|%s",
          "Group", buffer[1], atoi(buffer[2]), "vector", atoi(buffer[4]), actual_num_vectors,
          groupMetric, STORE_PREFIX, buffer[7], buffer[8]);
        }
 
        break; //expecting one group in each monitor gdf
      }
      else
        continue;
    }
    close_gdf(gdf_fp);
  }

  fprintf(fp, "%s\n", printLine);
  NSDL2_GDF(NULL, NULL, "printLine = %s", printLine);
}

void write_graph(FILE *fp, int idx, int gp_info_idx)
{
  int j;
  char line[MAX_LINE_LENGTH];
  Graph_Info *local_graph_data_ptr = NULL;
  FILE *gdf_fp;

  NSDL2_GDF(NULL, NULL, "Method called, dynobj_idx = %d, gp_info_idx = %d", idx, gp_info_idx);

  if(gp_info_idx > 0)
  {
    local_graph_data_ptr = graph_data_ptr + (group_data_ptr + gp_info_idx)->graph_info_index;

    for(j = 0; j < (group_data_ptr + gp_info_idx)->num_graphs; j++, local_graph_data_ptr++)
    {
      fprintf(fp, "%s\n", local_graph_data_ptr->gline);
    }
  }
  else
  {
    gdf_fp = open_gdf(dynObjForGdf[idx].gdf_name);

    while (fgets(line, MAX_LINE_LENGTH, gdf_fp) != NULL)
    {
      line[strlen(line) - 1] = '\0';
      if(!(strncasecmp(line, "graph|", strlen("graph|"))))
      {
        fprintf(fp, "%s\n", line);
        continue;
      }
      else
        continue;
    }
    close_gdf(gdf_fp);
  }
}


int write_runtime_changes_for_dyn_objs(FILE *fp)
{
  int j, genId, index;
  int endId, curId;
  char *name;
  char prefix[1024];

  NSDL2_GDF(NULL, NULL, "Method called, sgrp_used_genrator_entries = %d", sgrp_used_genrator_entries);

  for(j = 1; j < MAX_DYN_OBJS; j++)
  {
    NSDL2_GDF(NULL, NULL, "j = %d, total = %d", j, dynObjForGdf[j].total);
    if(dynObjForGdf[j].total <= 0)
      continue;

    endId = dynObjForGdf[j].startId + dynObjForGdf[j].total;
 
    NSDL2_GDF(NULL, NULL, "j = %d, dynObjForGdf[j].gp_info_idx = %d", j, dynObjForGdf[j].gp_info_idx);

    write_group(fp, j, dynObjForGdf[j].gp_info_idx);
      
    for(genId = 0; genId < sgrp_used_genrator_entries + 1; genId++)  
    {
      curId = dynObjForGdf[j].startId; 

      if(genId > 0){
        if(j == NEW_OBJECT_DISCOVERY_STATUS_CODE){
          curId = g_http_status_code_loc2norm_table[genId-1].last_gen_local_norm_id;
        }
        else if(j == NEW_OBJECT_DISCOVERY_TX || j == NEW_OBJECT_DISCOVERY_TX_CUM){
          curId = g_tx_loc2norm_table[genId-1].last_gen_local_norm_id[j-1];
        }
        else if(j == NEW_OBJECT_DISCOVERY_SVR_IP){
          curId = g_svr_ip_loc2norm_table[genId-1].last_gen_local_norm_id;
        }
        else if(j == NEW_OBJECT_DISCOVERY_TCP_CLIENT_FAILURES){
          curId = g_tcp_clinet_errs_loc2normtbl[genId-1].last_gen_local_norm_id[j - 1];  // RE-Check it
        }
      }

      getNCPrefix(prefix, genId-1, -1, ">", 0);

      NSTL1(NULL, NULL, "DynObjIdx = %d, genId= %d, startId = %d, total = %d, curId = %d, endId = %d", 
                         j, genId, dynObjForGdf[j].startId, dynObjForGdf[j].total, curId, endId); 
    
      for( ; curId < endId; curId++)
      {
        if(genId == 0 || j == NEW_OBJECT_DISCOVERY_RBU_DOMAIN) {
          index = curId; // Bug 79363: index will be garbeg when genId = 0
          name = nslb_get_norm_table_data(dynObjForGdf[j].normTable, index);
        }
        else {
          if(j == NEW_OBJECT_DISCOVERY_TX || j == NEW_OBJECT_DISCOVERY_TX_CUM){
            index = g_tx_loc2norm_table[genId-1].nvm_tx_loc2norm_table[curId];
            if(index != -1){
              g_tx_loc2norm_table[genId-1].last_gen_local_norm_id[j-1]++;
              name = nslb_get_norm_table_data(dynObjForGdf[j].normTable, index);              
            }
          }
          if(j == NEW_OBJECT_DISCOVERY_SVR_IP){
            index = g_svr_ip_loc2norm_table[genId-1].nvm_svr_ip_loc2norm_table[curId];
            if(index != -1){
              g_svr_ip_loc2norm_table[genId-1].last_gen_local_norm_id++;
              name = nslb_get_norm_table_data(dynObjForGdf[j].normTable, index);
            }
          }
          if(j == NEW_OBJECT_DISCOVERY_STATUS_CODE){
            index = g_http_status_code_loc2norm_table[genId-1].nvm_http_status_code_loc2norm_table[curId];
            if(index != -1){
              g_http_status_code_loc2norm_table[genId-1].last_gen_local_norm_id++;
              name = nslb_get_norm_table_data(dynObjForGdf[j].normTable, index);
            }
          }
          if(j == NEW_OBJECT_DISCOVERY_TCP_CLIENT_FAILURES){   // Need to rethink again 
            index = g_tcp_clinet_errs_loc2normtbl[genId-1].loc2norm[curId];
            if(index != -1){
              g_tcp_clinet_errs_loc2normtbl[genId-1].last_gen_local_norm_id[j - 1]++;
              name = nslb_get_norm_table_data(dynObjForGdf[j].normTable, index);
            }
          }
        } 
        if(index != -1)
          fprintf(fp, "%s%s %d\n", prefix, name, dynObjForGdf[j].rtg_index_tbl[genId][index]);
 
        NSDL2_GDF(NULL, NULL, "Goint to write in testrun.gdf.diff. GenId = %d, vector_name = %s%s", genId, prefix, name);
      }
    }
    write_graph(fp, j, dynObjForGdf[j].gp_info_idx);
  }
  return 0;
}
    
//create diff file using only one table 'dyn_cm_info_runtime_ptr'
int create_diff_file()
{
  FILE *fp = NULL;
  FILE *gdf_fp = NULL;
  char filename[1024];
  char line[MAX_LINE_LENGTH];
  char print_line[MAX_LINE_LENGTH];
  int j = 0, i = 0;
  CM_vector_info *vector_list = NULL;
  int last_entry = 0;
  int index = -1;
  int group_written = 0; 
  char printLine[MAX_LINE_LENGTH];
  char *field[14];
  //int ret;

  Graph_Info *local_graph_data_ptr = NULL;

  CM_info *cmptr = NULL;
  CM_info *cmptr_next = NULL;
  CM_info *cmptr_prev = NULL;

  NSDL2_GDF(NULL, NULL, "Method called");

  //if only monitor with vector list 'Warning: No vector" added then no need to create diff file because we will not add this monitor in testrun.gdf file
  if(! is_rtc_applied_for_dyn_objs())

  //Deleted vectors will not be written in diff file for some time, that is delete_freq. Default value will be 5. delete_freq will be increased and will be reset at the time when deleted vectors will be written in diff. 
  {
    if((!g_vector_runtime_changes) && (total_deleted_vectors == 0))
      return 1;
  }
  else
    set_rtg_index_for_dyn_objs_discovered();

  if(g_rtg_rewrite)
    return 1;

  if(global_settings->enable_hml_group_in_testrun_gdf)
  {

    MLTL3(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "HML Group feature is enable, max_entry = 50");
    CREATE_HML_METRIC_ARR(4);
  }

  //open diff file
  sprintf(filename, "%s/logs/TR%d/%lld/testrun.gdf.diff.%d", g_ns_wdir, testidx, g_partition_idx,test_run_gdf_count + 1);

  if((fp = fopen(filename, "w+")) == NULL)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Error in opening diff file %s\n", filename);
    perror("fopen");
    NS_EXIT(-1, CAV_ERR_1060017, filename);
  }

  // write info line
  sprintf(printLine,"Info|%s|-|%d|%u|%ld|%d|-",
                     version, (unsigned int)sizeof(Msg_data_hdr),
                     testidx, tmp_msg_data_size,
                     global_settings->progress_secs);

  NSDL3_GDF(NULL, NULL, "printLine = %s", printLine);

  fprintf(fp, "%s\n", printLine);

  if(is_rtc_applied_for_dyn_objs()) 
    write_runtime_changes_for_dyn_objs(fp);

  for(i=0; i< total_monitor_list_entries; i++)
  {
    cmptr=monitor_list_ptr[i].cm_info_mon_conn_ptr;
   
    vector_list=cmptr->vector_list;
    //         Monitors with NA gdf               New Vectors                            Deleted Vectors                      Reused vectors
    if(!strncmp(cmptr->gdf_name, "NA", 2) || (((cmptr->new_vector_first_index == -1) && (cmptr->total_deleted_vectors <= 0) && (cmptr->total_reused_vectors == 0)) && group_written == 0))
       continue;

    if(!(cmptr->flags & ALL_VECTOR_DELETED))
    {
      //writing Group line
      if((cmptr_prev == NULL) || (strcmp(cmptr->gdf_name, cmptr_prev->gdf_name) != 0)|| (group_written == 0)) //not same
      {
        index = -1;
        gdf_fp = write_group_line(cmptr, fp, &index);
 
        if(gdf_fp == NULL) // if total vectors to write are zero 
          continue;

        group_written=1;
      }

      //New vector entry
      if(cmptr->new_vector_first_index >= 0)
      {
        for(j=cmptr->new_vector_first_index; j < cmptr->total_vectors; j++)
        {
        //writing vectors
          if(!global_settings->enable_hml_group_in_testrun_gdf)
          {
            int mp = cmptr->metric_priority;      
            if(vector_list[j].flags & MON_BREADCRUMB_SET)
              fprintf(fp, "%s %ld\n", vector_list[j].mon_breadcrumb, vector_list[j].rtg_index[mp]);
            else
              fprintf(fp, "%s %ld\n", vector_list[j].vector_name, vector_list[j].rtg_index[mp]);
          }
        }
        cmptr->new_vector_first_index = -1;
      }

      //checking if vectors reused
      if(cmptr->total_reused_vectors)
      {
        int index;
        for(j=0; j < cmptr->total_reused_vectors; j++)
        {
          if(!global_settings->enable_hml_group_in_testrun_gdf)
          { 
            int mp = cmptr->metric_priority;
            index = cmptr->reused_vector[j];
            if(vector_list[index].flags & MON_BREADCRUMB_SET)
              fprintf(fp, "%s %ld\n", vector_list[index].mon_breadcrumb, vector_list[index].rtg_index[mp]);
            else
              fprintf(fp, "%s %ld\n", vector_list[index].vector_name, vector_list[index].rtg_index[mp]);
          }
        }
      }

      //Deleted vectors will be shown at delete frequency i.e. 5 freq
        //Vector deleted
      if((g_delete_vec_freq_cntr == 0) || !(g_enable_delete_vec_freq))
      {
        if(cmptr->total_deleted_vectors > 0)
        {
          int index;
          for(j=0; j<cmptr->total_deleted_vectors; j++)
          {
            int mp = cmptr->metric_priority;
            index = cmptr->deleted_vector[j];
            if(vector_list[index].flags & MON_BREADCRUMB_SET)
              fprintf(fp, "#-%s %ld\n", vector_list[index].mon_breadcrumb, vector_list[index].rtg_index[mp]);
            else
              fprintf(fp, "#-%s %ld\n", vector_list[index].vector_name, vector_list[index].rtg_index[mp]);

            if(g_enable_delete_vec_freq)
            {
              vector_list[index].flags |= RUNTIME_DELETED_VECTOR;
              vector_list[index].flags &= ~WAITING_DELETED_VECTOR;
              vector_list[index].vector_state = CM_DELETED;
              total_waiting_deleted_vectors--;
            }
          }
        }
      }
    }

    //set previous
    cmptr_prev = cmptr;
    last_entry = 0;
    
    if(i+1 == total_monitor_list_entries)
      last_entry = 1;
    else
      cmptr_next = monitor_list_ptr[i+1].cm_info_mon_conn_ptr;

    if(((last_entry == 1) || ((strcmp(cmptr->gdf_name, cmptr_next->gdf_name) != 0))) && (group_written == 1)) //not same write graph lines
    {
      if((index >= 0) && (monitor_list_ptr[index].cm_info_mon_conn_ptr->gp_info_index >= 0))
      {
        local_graph_data_ptr=graph_data_ptr + (group_data_ptr + monitor_list_ptr[index].cm_info_mon_conn_ptr->gp_info_index)->graph_info_index;

        if((group_data_ptr + monitor_list_ptr[index].cm_info_mon_conn_ptr->gp_info_index)->num_graphs <= 0)
        {
          MLTL3(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                       "cm_info_ptr[%d].gdf_name = %s, group_data_ptr[%d]->num_graphs = %d",
                              index, monitor_list_ptr[index].gdf_name, monitor_list_ptr[index].cm_info_mon_conn_ptr->gp_info_index,
                              (group_data_ptr + monitor_list_ptr[index].cm_info_mon_conn_ptr->gp_info_index)->num_graphs);
        }
        
        if(global_settings->enable_hml_group_in_testrun_gdf)
        {
          FILL_HML_GRAPH_LINE((group_data_ptr +  monitor_list_ptr[index].cm_info_mon_conn_ptr->gp_info_index), local_graph_data_ptr);
        }
        else
        { 
          for(j=0; j < (group_data_ptr + monitor_list_ptr[index].cm_info_mon_conn_ptr->gp_info_index)->num_graphs; j++, local_graph_data_ptr++)
          {
            if(local_graph_data_ptr->graph_state == GRAPH_NOT_EXCLUDED)
            {
              MLTL4(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                     "Dyn Mon: already parsed - gdf_name = %s, priniting graph line = %s",
                                 monitor_list_ptr[index].gdf_name, local_graph_data_ptr->gline);
              fprintf(fp, "%s\n", local_graph_data_ptr->gline);
            }
          }
        }
      }
      else
      {
        if(gdf_fp) 
        {
          while (fgets(line, MAX_LINE_LENGTH, gdf_fp) != NULL)
          {
            line[strlen(line) - 1] = '\0'; 
            strcpy(print_line, line);
            if(!(strncasecmp(line, "graph|", strlen("graph|"))))
            {
              get_tokens(line, field, "|", 14);
              if(!global_settings->enable_hml_group_in_testrun_gdf)
              {
                /* Write Graph line into testrun.gdf.diff if it is not excluded or come into criteria of H/M/L */
                if(is_graph_excluded(field[11], field[8], -1) == GRAPH_NOT_EXCLUDED)
                {
                  MLTL4(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                         "Dyn Mon: gdf_name = %s, priniting graph line = %s", cmptr->gdf_name, print_line);  
                  fprintf(fp, "%s\n", print_line);
                }
              }
              else
              {
                int mp = 0;
                int wbytes = 0;
                mp = get_metric_priority_id(field[11], 0);

                NSDL3_GDF(NULL, NULL, "mp = %d, graph_lines_write_index = %d, num_vectors = %d",
                                       mp, hml_metrics[mp].graph_lines_write_index, hml_metrics[mp].num_vectors);
                if(hml_metrics[mp].num_vectors && (is_graph_excluded(field[11], field[8], -1) != GRAPH_EXCLUDED))
                { 
                  if((hml_metrics[mp].graph_lines_size - hml_metrics[mp].graph_lines_write_index) < (strlen(print_line) + 1))
                  { 
                    MY_REALLOC(hml_metrics[mp].graph_lines, hml_metrics[mp].graph_lines_size + HML_METRIC_DELTA_SIZE,
                                   "Realloc HMLMetric Vector List", -1); 
                    hml_metrics[mp].graph_lines_size += HML_METRIC_DELTA_SIZE;
                  }
                  wbytes = sprintf(hml_metrics[mp].graph_lines + hml_metrics[mp].graph_lines_write_index, "%s\n", print_line);
                  hml_metrics[mp].graph_lines_write_index += wbytes; 
                  NSDL3_GDF(NULL, NULL, "wbytes = %d, graph_lines_write_index = %d", wbytes, hml_metrics[mp].graph_lines_write_index);
                }
              }
            }
          }
        }
        CLOSE_FP(gdf_fp);
      }
      group_written=0;
    
      WRITE_HML_IN_TESTRUN_GDF(fp);
      
      //Reset all members of array hml_metric[] to reuse
      RESET_HML_METRIC_ARR
    }
  }
 
  new_monitor_first_idx = -1;
  
  CLOSE_FP(fp);

  FREE_HML_METRIC_ARR

  return 0;
}

//ENABLE_MONITOR_DELETE_FREQUNCY <enable/disable> <frequency>
//This keyword is used to create diff file  
void kw_set_enable_monitor_delete_frequency(char *keyword, char *buffer)
{
  int value1 = 0;
  int value2 = 0;
 
  if(sscanf(buffer, "%*s %d %d", &value1, &value2) < 1)
  {
    NSTL1(NULL, NULL, "Invalid Usage of Keyword ENABLE_MONITOR_DELETE_FREQUNECY. %s. DISABLED by default.", buffer);
    return;
  }

  NSTL2(NULL, NULL, "ENABLE_MONITOR_DELETE_FREQUENCY keyword found with buff %s and  value1 =  %d and value = %d", buffer, value1, value2);

  //if feature is enable setting it to 0 else -1
  if(value1 != 0)
    g_enable_delete_vec_freq = true;
  else
    return;
  
  if((value2 > 1) && (value2 < 50))
    g_delete_vec_freq = value2;

  NSTL2(NULL, NULL, "ENABLE_MONITOR_DELETE_FREQUENCY keyword found. g_delete_vec_freq =  %d", g_delete_vec_freq); 
  return;
}

