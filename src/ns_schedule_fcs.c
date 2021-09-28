/********************************************************************************
 * File Name            : ns_schedule_fcs.c 
 * Purpose              : Contains parsing function for ENABLE_FCS_SETTINGS keyword.
 *                        and code for dividing limit between NVM's. 
 *                        
 * Modification History : Gaurav|01/09/2017
 ********************************************************************************/

#include <regex.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
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
#include "timing.h"
#include "tmr.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "ns_trace_level.h"
#include "netstorm.h"
#include "ns_log.h"
#include "ns_schedule_duration.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "ns_schedule_phases_parse.h"
#include "ns_replay_access_logs.h"
#include "ns_session.h"
#include "ns_parent.h"
#include "wait_forever.h"
#include "ns_percentile.h"
#include "ns_connection_pool.h" 
#include "divide_users.h"
#include "ns_session_pacing.h"
#include "ns_alloc.h"
#include "ns_schedule_fcs.h"
#include "nslb_util.h"
#include "ns_exit.h"
#include "nslb_dashboard_alert.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"

//Alert_Info *alertInfo = NULL;

/*****************************************************************************
 * Description:  Parsigng function of ENABLE_FCS_SETTINGS keyword
 * Input      :  keyword buffer of scenario
 * Output     :  Mode - enable/disable FCS feature
                 Concurrent sess Limit - number of session ran concurrently
                 Pool Size - Size of memory pool i.e max blocked users
 *****************************************************************************/
int kw_set_enable_schedule_fcs(char *buf, int runtime_flag, char *err_msg)
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
  
  NSDL1_PARSING(NULL, NULL, "Method Called, buf = %s, args = %d, mode_str = %s, limit_str = %s, queue_size_ptr = %s",
                             buf, num, mode_str, limit_str, queue_size_ptr);
  
  if(num < 2 || num > 4)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, ENABLE_FCS_SETTINGS_USAGE, CAV_ERR_1011076, CAV_ERR_MSG_1);
  }
  
  if (ns_is_numeric(mode_str) == 0) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, ENABLE_FCS_SETTINGS_USAGE, CAV_ERR_1011076, CAV_ERR_MSG_2);
  }

  mode = atoi(mode_str);
  if(mode < 0 || mode > 1)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, ENABLE_FCS_SETTINGS_USAGE, CAV_ERR_1011076, CAV_ERR_MSG_3);
  }  

  if(limit_str[0] != '\0')
  {
    limit = atoi(limit_str);
    if(limit < 0)
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, ENABLE_FCS_SETTINGS_USAGE, CAV_ERR_1011076, CAV_ERR_MSG_8);
    if(limit == 0)
    {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, ENABLE_FCS_SETTINGS_USAGE, CAV_ERR_1011076, CAV_ERR_MSG_4);
    }

    if(ns_is_numeric(limit_str) == 0) {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, ENABLE_FCS_SETTINGS_USAGE, CAV_ERR_1011076, CAV_ERR_MSG_2);
    }
  }

  if(queue_size_ptr[0] != '\0')
  {
    queue_size = atoi(queue_size_ptr);
    if (queue_size < 0)
    {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, ENABLE_FCS_SETTINGS_USAGE, CAV_ERR_1011076, CAV_ERR_MSG_8);
    }

    if (ns_is_numeric(queue_size_ptr) == 0) {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, ENABLE_FCS_SETTINGS_USAGE, CAV_ERR_1011076, CAV_ERR_MSG_2);
    }
  }

  global_settings->concurrent_session_mode = mode;
  global_settings->concurrent_session_limit = limit;
  global_settings->concurrent_session_pool_size = queue_size;
 
  NSDL2_PARSING(NULL, NULL, "Method end: concurrent_session_mode = [%d], concurrent_session_limit = [%d], concurrent_session_pool_size = [%d]",
                             global_settings->concurrent_session_mode, global_settings->concurrent_session_limit,
                             global_settings->concurrent_session_pool_size); 

  return 0;
}

/************************************************************************************
 * Description :  Divides global session limit among NVMs
 *
 * Output      :  Limit of users that one NVM can process
 * Return      :  None
 ***********************************************************************************/

void divide_on_fcs_mode()
{
  int i;
  int session_limit_all, leftover_session;
  int total_users = 0, leftover;

  NSDL1_SCHEDULE(NULL, NULL, "Method called"); 

  for (i = 0 ; i < global_settings->num_process; i++) {
    total_users += v_port_table[i].num_vusers;
  }

  leftover = total_users - global_settings->concurrent_session_limit;
  if(leftover < 0)
  {
    NS_EXIT(1, CAV_ERR_1031053, total_users, global_settings->concurrent_session_limit);
  }
  NSDL4_SCHEDULE(NULL, NULL, "Session limit Distribution among NVMs:");
  NSDL4_SCHEDULE(NULL, NULL, "TotalUsers=%d\n", global_settings->num_connections);

  int proc = 0;
  session_limit_all = (global_settings->concurrent_session_limit) / global_settings->num_process;
  leftover_session = (global_settings->concurrent_session_limit) % global_settings->num_process;
  NSDL4_SCHEDULE(NULL, NULL, "session_limit_all = %d  leftover_session = %d", session_limit_all, leftover_session);

  for (i = 0; i < global_settings->num_process; i++) {
    v_port_table[i].limit_per_nvm = session_limit_all;
  }

  for (i = 0; i < leftover_session; i++) {
    v_port_table[proc].limit_per_nvm++;
    proc++;
    NSDL4_SCHEDULE(NULL, NULL, "proc = %d  limit_per_nvm = %d", proc , v_port_table[i].limit_per_nvm);
    if (proc >= global_settings->num_process) proc = 0; // This ensures that we fill left over for
  }

  for (i = 0; i < global_settings->num_process; i++)
    NSDL4_SCHEDULE(NULL, NULL, "NVM=%d:  Sess limit=%d", i, v_port_table[i].limit_per_nvm);
}

/*
  FCS logic in STANDALONE and CLIENT_LOADER to
  optimize pool size when not given in keyword
  HLD:
    blocked_user = total users - session limit
    blocked_user_per_nvm = blocked_user/total NVM + increment
    total users:   Sum all users in SGRP
    session limit: Minimum number of session running concurrently
    increment:     If (blocked_user % total) > 0 then increment = 2 else 1
 */
void check_fcs_user_and_pool_size()
{
  int exp_block_users = 0;
  int all;
  int leftover;
  int increment = 1;
  int per_nvm_estimated_pool_size = 0;
  
  NSDL2_SCHEDULE(NULL, NULL, "Method called");

  //Check limit should be less then number of users
  if ( global_settings->num_connections < global_settings->concurrent_session_limit ) {
    NSTL1(NULL, NULL, "Error: Number of users (%d) cannot be less than Concurrent Session Limit (%d)",
              global_settings->num_connections, global_settings->concurrent_session_limit);
    NS_EXIT(-1, "Error: Number of users (%d) cannot be less than Concurrent Session Limit (%d)", global_settings->num_connections, global_settings->concurrent_session_limit);
  }

  exp_block_users = global_settings->num_connections - global_settings->concurrent_session_limit;
  all = exp_block_users / global_settings->num_process;
  leftover = exp_block_users % global_settings->num_process;
  if(leftover)
    increment = 2;

  per_nvm_estimated_pool_size = all + increment;
  NSDL3_SCHEDULE(NULL,NULL, "all = %d, leftover = %d, global_settings->concurrent_session_pool_size = %d,"
                            " per_nvm_estimated_pool_size = %d", all, leftover, global_settings->concurrent_session_pool_size,
                             per_nvm_estimated_pool_size); 

  //update pool size with estimated pool size if not given
  if (!global_settings->concurrent_session_pool_size)
  {
    global_settings->concurrent_session_pool_size = per_nvm_estimated_pool_size;
    NSTL1(NULL, NULL, "Updating pool size with estimated pool size = %d", global_settings->concurrent_session_pool_size);
  }
  else 
  {
    //Whether given pool size is less than estimated pool
    if (per_nvm_estimated_pool_size > global_settings->concurrent_session_pool_size)
    {
      NSTL1(NULL, NULL, "Error: Concurrent Session Pool Size (%d) should be more than Number of blocked users (%d)",
           global_settings->concurrent_session_pool_size, exp_block_users);
      NS_EXIT(-1, "Error: Concurrent Session Pool Size (%d) should be more than Number of blocked users(%d)",
        	     global_settings->concurrent_session_pool_size, exp_block_users);
    }
  }
}
