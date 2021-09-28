/******************************************************************
 * Name    : ns_post_test_check.c
 * Author  : Archana
 * Purpose : This file contains methods related to make connection 
             for from event "After test is Over" (90)
             for Check Monitors
 * Note:
 * Modification History:
 * 02/04/09 - Initial Version
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
#include "ns_log.h"
#include "ns_custom_monitor.h"
#include "util.h"
#include "tmr.h"
#include "timing.h"
#include "ns_schedule_phases.h"
#include "ns_check_monitor.h"
#include "ns_pre_test_check.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "ns_alloc.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"
#include "nslb_alert.h"
#include "nslb_cav_conf.h"

#include "netstorm.h"
#include "wait_forever.h"
#include "init_cav.h"
#include "ns_gdf.h"
#include "ns_user_monitor.h"
#include "ns_dynamic_vector_monitor.h"
#include "ns_mon_log.h"
#include "ns_event_log.h"
#include "ns_string.h"
#include "ns_event_id.h"
#include "ns_parent.h"
#include "ns_ndc_outbound.h"
#include "ns_trace_level.h" 
#include "ns_error_msg.h"
#include "ns_kw_usage.h"
int total_post_test_check_entries = 0;  // Total Check monitor enteries
int post_test_check_timeout = -1;        

int num_post_test_check;  //number of check monitors

/* --- START: Method used during Parsing Time  ---*/

//This method used to parse POST_TEST_CHECK_TIMEOUT keyword.
//This post_test_check_timeout use to get timeout for running From event "After test is Over (90)" of check monitor in seconds.
int kw_set_post_test_check_timeout(char *keyword, char *buf, char *kw_buf, char *err_msg, int runtime_flag)
{
  if(post_test_check_timeout == -1)
  {
    NSDL2_MON(NULL, NULL, "Method called. Keyword = %s, Timeout (in second) = %s ", keyword, buf);
    post_test_check_timeout = atoi(buf);
    if (post_test_check_timeout <= 0) 
    { 
      NS_KW_PARSING_ERR(kw_buf, runtime_flag, err_msg, POST_TEST_CHECK_TIMEOUT_USAGE, CAV_ERR_1011149, CAV_ERR_MSG_9);
    }
    NSDL3_MON(NULL, NULL, "post_test_check_timeout = %d sec", post_test_check_timeout);
  }
  else
    post_test_check_timeout = POST_TEST_CHECK_DEFAULT_TIMEOUT; //default timeout is 5 min for From event "After test is Over" of check monitor
  return 0;
}

/* --- End: Method used during Parsing Time  ---*/


/* --- START: Method used during Run Time  ---*/

/* --- START: Method used for epoll  ---*/

//This method is to close all connections for From event "After test is Over (90)" of check monitor 
static inline void close_connection_all_post_test_check()
{
  int i;
  CheckMonitorInfo *check_monitor_ptr = check_monitor_info_ptr;
  NSDL2_MON(NULL, NULL, "Method called.");
  for (i = 0; i < total_check_monitors_entries; i++, check_monitor_ptr++)
    close_check_monitor_connection(check_monitor_ptr);
}

//This method is abort post test check and exit
inline void abort_post_test_check_and_exit(char *err_msg)
{
  NSDL2_MON(NULL, NULL, "Method called. Error Message is '%s'", err_msg);
  NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_MAJOR,
                              	    __FILE__, (char*)__FUNCTION__,
				    "%s", err_msg);

  if(!is_outbound_connection_enabled)
    close_connection_all_post_test_check();
  /* Bug 2048 Fixed */
  /*exit(-1);*/
}

static inline void epoll_init()
{
  NSDL2_MON(NULL, NULL, "Method called");

  if((epoll_fd = epoll_create(total_check_monitors_entries)) == -1) 
  {
    char err_msg[MAX_LENGTH];
    sprintf(err_msg, "epoll_init() - Failed to create epoll for check monitors. Error is '%s'. Exiting ...", nslb_strerror(errno));
    abort_post_test_check_and_exit(err_msg);
  }
}

/* --- END: Method used for epoll  ---*/

/*
   This method to setup the connection with Create Server
   This will make connection only for From event "After test is Over (90)" of check monitor to all create servers where check monitor is to be executed
*/
static int make_connections_for_post_test_check()
{
  int post_test_check_id;
  CheckMonitorInfo *check_monitor_ptr = check_monitor_info_ptr; 
  //ServerInfo *servers_list = NULL;
  //servers_list = (ServerInfo *) topolib_get_server_info();
  char *server_ip;
  short server_port;
  char ChkMon_BatchJob[50];
  char alert_msg[ALERT_MSG_SIZE + 1];
  char buffer[4 * ALERT_MSG_SIZE];
  alert_msg[0] = '\0';
  buffer[0] = '\0';
  int len;
  if(check_monitor_ptr->bj_success_criteria > 0)
    strcpy(ChkMon_BatchJob, "Batch Job");
  else
    strcpy(ChkMon_BatchJob, "Check Monitor");


  NSDL2_MON(NULL, NULL, "Method called");

  if(is_outbound_connection_enabled)
  {
    NSDL2_MON(NULL, NULL, "Outbound connection is enabled");
    server_ip = global_settings->net_diagnostics_server; 
    server_port = global_settings->net_diagnostics_port;
  } /*else {
    NSDL2_MON(NULL, NULL, "Outbound connection is disabled");
    server_ip = check_monitor_ptr->cs_ip; 
    server_port = check_monitor_ptr->cs_port;
  }*/
  printf("Starting Check Monitors/Batch Jobs\n");
  for (post_test_check_id = 0; post_test_check_id < total_check_monitors_entries; post_test_check_id++, check_monitor_ptr++)
  {
    if(check_monitor_ptr->from_event != CHECK_MONITOR_EVENT_AFTER_TEST_IS_OVER)
      continue;
 
    if(topo_info[topo_idx].topo_tier_info[check_monitor_info_ptr->tier_idx].topo_server[check_monitor_info_ptr->server_index].server_ptr->topo_servers_list->cntrl_conn_state & CTRL_CONN_ERROR)
    {
      ns_check_monitor_log(EL_DF, DM_EXECUTION, MM_MON, _FLN_,  check_monitor_ptr,
                                 EID_CHKMON_GENERAL, EVENT_INFO,
                                 "Got Unknown server error from gethostbyname for - %s. So skipping connection making for it.",
                                 check_monitor_ptr->check_monitor_name);

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
    else if(check_monitor_ptr->status == CHECK_MONITOR_STOPPED)
    {
      NSDL1_MON(NULL, NULL, "check_monitor_ptr '%s' not started as its status is CHECK_MONITOR_STOPPED", check_monitor_ptr->mon_name);
      continue;
    }

    if(!is_outbound_connection_enabled)
    {
      NSDL2_MON(NULL, NULL, "Outbound connection is disabled");
      server_ip = check_monitor_ptr->cs_ip;
      server_port = check_monitor_ptr->cs_port;
    }

    if(check_monitor_ptr->bj_success_criteria > 0)
    {
      ns_check_monitor_log(EL_CDF, DM_EXECUTION, MM_MON, _FLN_,  check_monitor_ptr,
                                   EID_CHKMON_GENERAL, EVENT_INFO,
                                   "Starting Batch Job - %s",
                                   check_monitor_ptr->check_monitor_name);
      #ifdef NS_DEBUG_ON
        printf("Starting Batch Job - %s\n", check_monitor_ptr->check_monitor_name); 
      #endif
    }
    else
    {
      ns_check_monitor_log(EL_DF, DM_EXECUTION, MM_MON, _FLN_, check_monitor_ptr,
                              EID_CHKMON_GENERAL, EVENT_INFO,
                              "Starting Check Monitor - %s", check_monitor_ptr->check_monitor_name);
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


    NSDL3_MON(NULL, NULL, "CheckMonitor => %s", CheckMonitor_to_string(check_monitor_ptr, s_check_monitor_buf));
    NSDL2_MON(NULL, NULL, "Making connection for Check Monitor '%s'", check_monitor_ptr->check_monitor_name);
      check_monitor_ptr->fd = nslb_tcp_client(server_ip, server_port);
    if (check_monitor_ptr->fd < 0)
    {
      ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
        			  EID_CHKMON_ERROR, EVENT_MAJOR,
        			  "Error in making connection to server %s.",
        			  check_monitor_ptr->cs_ip);
 
      sprintf(alert_msg, ALERT_MSG_1017010, ChkMon_BatchJob, check_monitor_ptr->check_monitor_name, check_monitor_ptr->server_name, check_monitor_ptr->tier_name, "Error in making connection to server");

          if((len = nslb_make_cavisson_alert_body(buffer, ALERT_MSG_BUF_SIZE, testidx, alert_msg, ALERT_CRITICAL, ChkMon_BatchJob, check_monitor_ptr->tier_name, check_monitor_ptr->server_name, NULL, global_settings->hierarchical_view_topology_name, global_settings->alert_info->policy, global_settings->alert_info->protocol, g_cavinfo.NSAdminIP, parent_port_number, ALERT_FAIL_VALUE, global_settings->progress_secs)) < 0)
          {
             NSTL1(NULL, NULL, "Failed to make POST body, testidx = %d, alert_type = %d, alert_policy = %s, alert_msg = %s",
                         testidx, ALERT_CRITICAL, global_settings->alert_info->policy, alert_msg);
          }

          NSTL1(NULL, NULL, "Going to send alert msg, tomcat port = %d, testid = %d", g_tomcat_port, testidx);
          if (ns_send_alert_ex(ALERT_CRITICAL, HTTP_METHOD_POST, NS_CONTENT_TYPE_JSON, buffer, len) > 0)
             NSTL1(NULL, NULL, "Failed to send alert msg to Dashboard");

      mark_check_monitor_fail(check_monitor_ptr, 0, CHECK_MONITOR_CONN_FAIL);
      continue;
    }
  
    if (fcntl(check_monitor_ptr->fd, F_SETFL, O_NONBLOCK) < 0)
    {
      ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
        			  EID_CHKMON_ERROR, EVENT_CRITICAL,
        			  "Error in making connection non blocking.");
      char err_msg[MAX_LENGTH];
      sprintf(alert_msg, ALERT_MSG_1017010, ChkMon_BatchJob, check_monitor_ptr->check_monitor_name, check_monitor_ptr->server_name, check_monitor_ptr->tier_name, "Error in making connection non blocking.");

          if((len = nslb_make_cavisson_alert_body(buffer, ALERT_MSG_BUF_SIZE, testidx, alert_msg, ALERT_CRITICAL, ChkMon_BatchJob, check_monitor_ptr->tier_name, check_monitor_ptr->server_name, NULL, global_settings->hierarchical_view_topology_name, global_settings->alert_info->policy, global_settings->alert_info->protocol, g_cavinfo.NSAdminIP, parent_port_number, ALERT_FAIL_VALUE, global_settings->progress_secs)) < 0)
          {
             NSTL1(NULL, NULL, "Failed to make POST body, testidx = %d, alert_type = %d, alert_policy = %s, alert_msg = %s",
                         testidx, ALERT_CRITICAL, global_settings->alert_info->policy, alert_msg);
          }

          NSTL1(NULL, NULL, "Going to send alert msg, tomcat port = %d, testid = %d", g_tomcat_port, testidx);
          if (ns_send_alert_ex(ALERT_CRITICAL, HTTP_METHOD_POST, NS_CONTENT_TYPE_JSON, buffer, len) > 0)
             NSTL1(NULL, NULL, "Failed to send alert msg to Dashboard");

      sprintf(err_msg, "Error in making connection non blocking for Check Monitor '%s'. Exiting ...", check_monitor_ptr->check_monitor_name); 
      abort_post_test_check_and_exit(err_msg);
    }
    // Out bound is enables, START_SESSION request will be send first
    if(is_outbound_connection_enabled) {
      NSDL2_MON(NULL, NULL, "Outbound is enabled");
      check_monitor_ptr->conn_state = CHK_MON_SESS_START_SENDING; 
    }
    NSDL2_MON(NULL, NULL, "Connection made successfully. Sending request message");
    if (send_msg_to_cs(check_monitor_ptr) == FAILURE)
    {
      mark_check_monitor_fail(check_monitor_ptr, 0, CHECK_MONITOR_SEND_FAIL);
      continue;
    }
    NSDL3_MON(NULL, NULL, "Request message sent successfully for Check Monitor '%s'", check_monitor_ptr->check_monitor_name);
    add_select_pre_test_check((char *)check_monitor_ptr, check_monitor_ptr->fd, EPOLLIN | EPOLLERR, CHECK_MONITOR_EVENT_AFTER_TEST_IS_OVER);

    num_post_test_check++;
    total_post_test_check_entries++;
    
  }
  return num_post_test_check;
}

inline void abort_post_test_checks(int epoll_check, char *err)
{
  NSDL2_MON(NULL, NULL, "Method called.");

  if(epoll_check == 0)
  {
    NSDL1_MON(NULL, NULL, "Timeout while waiting for check monitor results. Number of check monitor not complete = %d", num_post_test_check);
    //to print on progress report and console
    NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_MAJOR,
                              	    __FILE__, (char*)__FUNCTION__,
				    "Timeout while waiting for check monitor results."
				    " Number of check monitor not complete = %d",
				    num_post_test_check);
    num_post_test_check = 0; // We need to abort this cycle
    return;
  }
  char err_msg[MAX_LENGTH];
  NSDL1_MON(NULL, NULL, "Error while waiting for check monitor results. Number of check monitor not complete = %d.", num_post_test_check);
  sprintf(err_msg, "Error '%s' comming while waiting for check monitor results. So Check monitors failed for From Event 'After test is Over'. Exiting ...", err);
  abort_post_test_check_and_exit(err_msg);
}

static void free_post_test_check()
{
  int i;
  CheckMonitorInfo *check_monitor_ptr = check_monitor_info_ptr;
  NSDL2_MON(NULL, NULL, "Method called. total_check_monitors_entries = %d", total_check_monitors_entries);

  for (i = 0; i < total_check_monitors_entries; i++, check_monitor_ptr++)
  {  
    FREE_AND_MAKE_NOT_NULL(check_monitor_ptr->check_monitor_name, "check_monitor_ptr->check_monitor_name", -1);
    FREE_AND_MAKE_NOT_NULL(check_monitor_ptr->cs_ip, "check_monitor_ptr->cs_ip", -1);
    FREE_AND_MAKE_NOT_NULL(check_monitor_ptr->rem_ip, "check_monitor_ptr->rem_ip", -1);
    FREE_AND_MAKE_NOT_NULL(check_monitor_ptr->rem_username, "check_monitor_ptr->rem_username", -1);
    FREE_AND_MAKE_NOT_NULL(check_monitor_ptr->rem_password, "check_monitor_ptr->rem_password", -1);
    FREE_AND_MAKE_NOT_NULL(check_monitor_ptr->pgm_path, "check_monitor_ptr->pgm_path", -1);
    FREE_AND_MAKE_NOT_NULL(check_monitor_ptr->pgm_args, "check_monitor_ptr->pgm_args", -1);
    FREE_AND_MAKE_NOT_NULL(check_monitor_ptr->partial_buffer, "check_monitor_ptr->partial_buffer", -1);
    FREE_AND_MAKE_NOT_NULL(check_monitor_ptr->tier_name, "check_monitor_ptr->tier_name", -1);
    FREE_AND_MAKE_NOT_NULL(check_monitor_ptr->g_mon_id, "check_monitor_ptr->g_mon_id", -1);
    FREE_AND_MAKE_NOT_NULL(check_monitor_ptr->server_name, "check_monitor_ptr->server_name", -1);
    FREE_AND_MAKE_NOT_NULL(check_monitor_ptr->instance_name, "check_monitor_ptr->instance_name", -1);
    FREE_AND_MAKE_NOT_NULL(check_monitor_ptr->mon_name, "check_monitor_ptr->mon_name", -1);
    FREE_AND_MAKE_NOT_NULL(check_monitor_ptr->end_phase_name, "check_monitor_ptr->end_phase_name", -1);
    FREE_AND_MAKE_NOT_NULL(check_monitor_ptr->start_phase_name, "check_monitor_ptr->start_phase_name", -1);
    FREE_AND_MAKE_NOT_NULL(check_monitor_ptr->json_args, "check_monitor_ptr->json_args", -1);
    FREE_AND_MAKE_NOT_NULL(check_monitor_ptr->partial_buf, "check_monitor_ptr->partial_buf", -1);
    FREE_AND_MAKE_NOT_NULL(check_monitor_ptr->origin_cmon, "check_monitor_ptr->origin_cmon", -1);
    FREE_AND_MAKE_NOT_NULL(check_monitor_ptr->bj_search_pattern, "check_monitor_ptr->bj_search_pattern,", -1);
  }
  FREE_AND_MAKE_NULL(check_monitor_info_ptr, "check_monitor_info_ptr", -1);
  FREE_AND_MAKE_NULL(failed_monitor, "failed monitor", -1);

  total_check_monitors_entries = 0;
  max_check_monitors_entries = 0;
  if(epoll_fd > 0) close(epoll_fd);
}


static void wait_for_post_check_results()
{
  int cnt, i;
  int epoll_time_out;
  struct epoll_event *epev = NULL;
  CheckMonitorInfo *check_monitor_ptr = NULL;
  
  epoll_time_out = post_test_check_timeout;

  NSDL2_MON(NULL, NULL, "Method called. num_post_test_check = %d, epoll_timeout in sec = %d", num_post_test_check, epoll_time_out);

  MY_MALLOC(epev, sizeof(struct epoll_event) * total_post_test_check_entries, "CheckMonitor epev", -1);

  while(1)
  {
    if(num_post_test_check == 0)
    {
      NSDL2_MON(NULL, NULL, "Check monitor done.");
      break;
    }
    memset(epev, 0, sizeof(struct epoll_event) * total_post_test_check_entries);
    NSDL2_MON(NULL, NULL, "Wait for an I/O event on an epoll file descriptor epoll_fd = %d, num_post_test_check = %d, epoll_time_out = %d", epoll_fd, num_post_test_check, epoll_time_out);

    cnt = epoll_wait(epoll_fd, epev, num_post_test_check, epoll_time_out * 1000);
    NSDL4_MON(NULL, NULL, "epoll wait return value = %d", cnt);

    if (cnt > 0)
    {
      for (i = 0; i < cnt; i++)
      {
        check_monitor_ptr = (CheckMonitorInfo *)epev[i].data.ptr;
        if (epev[i].events & EPOLLERR) 
          mark_check_monitor_fail(check_monitor_ptr, 1, CHECK_MONITOR_EPOLLERR);
        else if (epev[i].events & EPOLLIN)
          read_output_from_cs(check_monitor_ptr);
        else
        {
          ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
				      EID_CHKMON_ERROR, EVENT_MAJOR,
				      "Event failed for check monitor '%s'. epev[i].events = %d err = %s",
				      check_monitor_ptr->check_monitor_name, epev[i].events, nslb_strerror(errno));
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
      abort_post_test_checks(cnt, nslb_strerror(errno));
      break;
    }
  }
  if (rfp) fflush(rfp);
  FREE_AND_MAKE_NULL(epev, "epev", -1);
}

//This is main method to control From Event 'After test is Over'
void run_post_test_check()
{
  NSDL2_MON(NULL, NULL, "Method called for From Event 'After test is Over'."); 
 
  NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_INFO,
                              	    __FILE__, (char*)__FUNCTION__,
				    "Starting after test run over check monitors ...");
  epoll_init();

  //we are allocating buffer for failed monitor as we will fill failed monitor with detail in check_result
  MY_MALLOC(failed_monitor, (sizeof(char) * total_check_monitors_entries * (256 + 256)), "failed_monitor", -1);

  //No retry for From Event 'After test is Over', so no need of while(1)
  num_post_test_check = make_connections_for_post_test_check();
  if(num_post_test_check > 0) //At least one connection made
  {
    wait_for_post_check_results(); //this will come out when check monitor done or time out come.
    close_connection_all_post_test_check();
  }
  free_post_test_check();

/* Bug 2048 - Post processing is not done if Check Monitors started after test is over fails 
 * So do not exit only RETURN
 * if(ret != CHECK_MONITOR_DONE_WITH_ALL_PASS)
 * {
 *   save_status_of_test_run(TEST_RUN_STOPPED_DUE_TO_FAILURE_OF_CHECK_MONITOR);
 *   exit (-1);
 * }
 */
}

/* --- END: Method used during Run Time  --- */
