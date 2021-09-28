/******************************************************************
 * Name    : ns_monitor_server_ctrl_conn.c
 * Author  : Prachi
 * Purpose : This file contains methods related to sending control message on server
 *              
 * Note:
 * Modification History:
 * 18/12/13 - Initial Version
*****************************************************************/

/* High level design:
 * send_testrun_starts_msg_to_all_cavmonserver_used() → 
 *                                         called from ns_parent.c only first time in the beginning of the test
 *                                         after parsing arguments etc to send start heart beat message. 
 * send_testrun_running_msg_to_all_cavmonserver_used() →                                                                                        *                                         called from deliver_report.c throughout the test to send control message.
 * send_end_msg_to_all_cavmonserver_used()  → 
 *                                         called from deliver_report.c at the end of the test to send end heart beat message.
 *
 *
 * All the 3 functions mentioned above works in following ways:
 * 1. For all the servers present in server.dat, check if monitor is applied on any server. 
 *    1.1> if applied : 
 *                make connection and send control message.
 *    1.2> if not applied then do not send control message.
 *                                       
 *   
 * Steps to create connection and send message:
 * If connection is made first time then it will be blocking else non-blocking. 
 *
 * a> blocking control connection
 *        a.1.  either connected → if created successfully then send message immediately
 *        a.2.  or  not connected → will retry in next progress interval only if retry flag
 *   After retry non-blocking connection will be made.
 * b> non-blocking control connection
 *        b.1 if gets connected then
 *            b.1.1> send message
 *            b.1.2> reset cntrl_conn_retry_attempts for that particular server
 *        b.2 if not connected
 *            b.2.1> decrement cntrl_conn_retry_attempts for that particular server.
 *            b.2.2> retry again in next progress interval (default 10 secs)
 *        b.3 if connecting / in progress
 *            b.3.1> do add_select for events (EPOLLOUT | EPOLLERR | EPOLLHUP)                              
 *                   On EPOLLOUT:
 *                     1. Try to connect again
 *                        1.1 if fail:
 *                            remove from epoll 
 *                            close fd
 *                            it will retry again on next progress interval.
 *                        1.2 if pass:
 *                            make message
 *                            send message : it can be pass/fail/partial
 *                   On EPOLLERR and in other cases:                                             
 *                      remove from epoll_wait only. 
 *
 *
 *
 *  Here all the functions are static except following: 
 *
 *  called from ns_runtime_changes_monitor.c:
 *    check_and_open_nb_cntrl_connection(CM_info *cus_mon_ptr, char *ip, int port, char *msg);
 *
 *  called from wait_forever.c:
 *    handle_server_ctrl_conn_event(struct epoll_event *pfds, int i, void *ptr);
 *
 *  //to send heart beat/control message
 *  send_testrun_starts_msg_to_all_cavmonserver_used();
 *  send_testrun_running_msg_to_all_cavmonserver_used(int seq_num);
 *  send_end_msg_to_all_cavmonserver_used();
 *
 *  */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <regex.h>
#include <errno.h>
#include "v1/topolib_structures.h"
#include "nslb_util.h"
#include "nslb_cav_conf.h"
#include "ns_msg_def.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "util.h"
#include "tmr.h"
#include "timing.h"
#include "ns_gdf.h"
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
#include "ns_user_monitor.h"
#include "ns_mon_log.h"
#include "ns_event_log.h"
#include "ns_server_admin_utils.h"
#include "ns_string.h"
#include "ns_event_id.h"
#include "nslb_sock.h"
#include "ns_trace_level.h"
#include "ns_ndc.h"
#include "ns_appliance_health_monitor.h"
#include "nslb_mon_registration_con.h"
#include "wait_forever.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"
//#define topo_idx 0
#define DEFAULT_CMON_PORT 7891

#define SEND_END_MSG_AND_CLOSE_CONN 1
#define SEND_CTRL_MSG_AND_DO_NOT_CLOSE_CONN 0

//this will keep incrementing on each progress report, once it reach the g_count_to_send_hb then will send heartbeat on monitors data connection
int g_last_hb_send = 1;
//this is counter to know after how many progress report we have to send the heartbeat
int g_count_to_send_hb = 0;

//to identify if monitors data conn heartbeat is enable or disable after parsing keyword 'DATA_CONN_HEART_BEAT'
int g_enable_mon_data_conn_hb = 0;
//to store monitors data conn heartbeat interval in secs after parsing keyword 'DATA_CONN_HEART_BEAT'
//NS will send heartbeat after this interval
int g_data_conn_hb_interval_in_secs = 0;


#define DATA_CONN_HEART_BEAT_INTERVAL 900  //15 mins

//DATA_CONN_HEART_BEAT <0/1> <interval in secs>
int kw_set_enable_data_conn_hb(char *keyword, char *buffer, char *err_msg, int runtime_flag)
{
  char key[1024] = {0};
  int data_conn_hb = 0;
  int data_conn_hb_interval_in_secs = DATA_CONN_HEART_BEAT_INTERVAL;

  if(sscanf(buffer, "%s %d %d", key, &data_conn_hb, &data_conn_hb_interval_in_secs) < 2)
    NS_KW_PARSING_ERR(buffer, runtime_flag, err_msg, ENABLE_DATA_CONN_HB_USAGE, CAV_ERR_1011151, CAV_ERR_MSG_1);

  g_enable_mon_data_conn_hb = data_conn_hb;
  g_data_conn_hb_interval_in_secs = data_conn_hb_interval_in_secs;

  //at every this counter will send hb
  //global_settings->progress_secs is in msecs converting into secs
  g_count_to_send_hb = g_data_conn_hb_interval_in_secs / (global_settings->progress_secs / 1000); 
  return 0;
}

#define DATA_CONN_HEART_BEAT_MSG "test_run_running\n"
#define DATA_CONN_HEART_BEAT_MSG_LEN 17 //16 + 1 for new line 

static int send_hb_on_nb_data_conn(CM_info *cm_info)
{
  char *buf_ptr;
  int bytes_sent;

  NSDL2_MON(NULL, NULL, "Method called.");

  //First time 
  if(cm_info->hb_partial_buf == NULL) //First time 
  {
    buf_ptr = DATA_CONN_HEART_BEAT_MSG;
    cm_info->hb_bytes_remaining = DATA_CONN_HEART_BEAT_MSG_LEN;
  }
  else //If there is partial send
  {
    buf_ptr = cm_info->hb_partial_buf;
  }

  // Send MSG to CMON
  while(cm_info->hb_bytes_remaining)
  {
    NSDL2_MON(NULL, NULL, "Send control MSG: cm_info->fd = %d, remaining_bytes = %d,  send_offset = %d buf = %s", 
                           cm_info->fd, 
                           cm_info->hb_bytes_remaining, 
                           cm_info->hb_send_offset, 
                           buf_ptr + cm_info->hb_send_offset);

    bytes_sent = send(cm_info->fd, buf_ptr + cm_info->hb_send_offset, cm_info->hb_bytes_remaining, 0);

    if(bytes_sent < 0)
    {
      NSDL2_MON(NULL, NULL, "bytes_sent = %d, errno = %d, error = %s", bytes_sent, errno, nslb_strerror(errno));
      if(errno == EAGAIN) //If message send partially
      {
        if(cm_info->hb_partial_buf == NULL)
        {
          MY_MALLOC(cm_info->hb_partial_buf, (cm_info->hb_bytes_remaining + 1), "Malloc buffer for heart beat partial send", -1);
          strcpy(cm_info->hb_partial_buf, buf_ptr + cm_info->hb_send_offset);
          cm_info->hb_send_offset = 0;
        }
        cm_info->hb_conn_state = HB_DATA_CON_SENDING;

        NSDL3_MON(NULL, NULL, "Partial Send control msg: cm_info->fd = %d, remaining_bytes = %d, send_offset = %d," 
                              "remaning buf = [%s]", 
                               cm_info->fd, 
                               cm_info->hb_bytes_remaining, 
                               cm_info->hb_send_offset, 
                               cm_info->hb_partial_buf + cm_info->hb_send_offset);
       
        return 0;
      }
      if(errno == EINTR) //If any intrept occurr
      {
        NSDL2_MON(NULL, NULL, "Interrupted. Continuing...");
        continue;
      }
      else 
      {
        //sending msg failed for this monitor
        NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_MAJOR,
                                __FILE__, (char*)__FUNCTION__,
                       "HeartBeat send failure for monitor %s on ip %s", cm_info->monitor_name, cm_info->cs_ip);

        return -1;
      } 
    }

    NSDL2_MON(NULL, NULL, "bytes_sent = %d", bytes_sent);

    if(bytes_sent == 0)
    {
      NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_MAJOR,
                                __FILE__, (char*)__FUNCTION__,
                       "HeartBeat send failure with server %s for monitor %s", cm_info->cs_ip, cm_info->monitor_name);
      return -1;
    }
    cm_info->hb_bytes_remaining -= bytes_sent;
    cm_info->hb_send_offset += bytes_sent; 
  } //End while Loop 

  //if(remaining_bytes == 0)
  //{
    cm_info->hb_conn_state = HB_DATA_CON_INIT;
    cm_info->hb_bytes_remaining = 0;
    cm_info->hb_send_offset = 0;
    if(cm_info->hb_partial_buf != NULL)
      FREE_AND_MAKE_NULL(cm_info->hb_partial_buf, "cm_info->hb_partial_buf", -1);
  //}
  return 0;
}

void send_hb_on_data_conn()
{
  int mon_id = 0;
  CM_info *cm_ptr = NULL;

  MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Sending HeartBeat on monitors data connection.");
  for(mon_id = 0; mon_id < total_monitor_list_entries; mon_id++)
  {
    cm_ptr = monitor_list_ptr[mon_id].cm_info_mon_conn_ptr;
    //Not sending heartbeat for Monitor registerd with NS.
     if(cm_ptr->is_monitor_registered == MONITOR_REGISTRATION)
       continue;
    
    //not sending heartbeat on lps monitors data connection because mostly lps and NS present in same machine 
    //cmon was not able to handle heartbeat on monitors with run everytime option, so we will be sending heartbeat for monitors with run only once option. we are converting every monitor in run once type, and cmon will also handle these cases in 4.1.7. Hence this is only committed in 4.1.6 and not in 4.1.7
    //BUG 68317: When monitor is added at runtime, it makes connection in deliver report and if state is connecting,it means we have not sent cm_init_monitor message to cmon on data connection. So we donot need to send HB message on it. if we send Hb on this connection, cmon treats it as control connection. And We send cm_init_monitor req when connection is in connected state it was ignored by cmon, as they have considered this connection as control connection. And they only expect HB messages like test_run_starts, test_run_running, and test_run_ends.
    if(!(cm_ptr->flags & USE_LPS) && (cm_ptr->fd > 0) && (cm_ptr->conn_state != CM_CONNECTING) && (cm_ptr->option == RUN_ONLY_ONCE_OPTION))
    {
      NSDL2_MON(NULL, NULL, "Sending heartbeat msg for monitor %s on server %s. Run option = %d, fd = %d, use_lps = %d", 
                                                     cm_ptr->monitor_name, 
                                                     cm_ptr->cs_ip, 
                                                     cm_ptr->option, 
                                                     cm_ptr->fd, (cm_ptr->flags & USE_LPS));
      if(send_hb_on_nb_data_conn(cm_ptr) < 0)
      {
        MLTL1(EL_F, 0, 0, _FLN_, cm_ptr, EID_DATAMON_GENERAL, EVENT_INFO,
                        "Heart beat sending failed on server '%s', hence closing the monitor connection from that server for monitor '%s'",
                         cm_ptr->cs_ip, 
                         cm_ptr->monitor_name);

        handle_monitor_stop(cm_ptr, MON_STOP_ON_RECEIVE_ERROR);
      }
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Heart beat sent for monitor %s on server %s. Run option = %d, fd = %d, use_lps = %d", 
               cm_ptr->monitor_name, 
               cm_ptr->cs_ip, 
               cm_ptr->option, 
               cm_ptr->fd, 
               (cm_ptr->flags & USE_LPS));
    }
    else
    {
      NSDL2_MON(NULL, NULL, "Not sending heartbeat msg for monitor %s on server %s. Run option = %d, fd = %d, use_lps = %d", 
                  cm_ptr->monitor_name, 
                  cm_ptr->cs_ip, 
                  cm_ptr->option, 
                  cm_ptr->fd, (cm_ptr->flags & USE_LPS));
    } 
  }
}

/* TODO
 * nslb_get_src_addr() and nslb_get_dest_addr() methods return static buffer.
 * This can overwrite buffer before using it.
 * Need to change API; API should take buffer as argument to store the result.
 */

/*This function will create non-blocking connection.
 * if connection created successfully -> then reset connection retry attempts to max retry count
 * if not -> then close fd and return -1
 * if partial -> then add in epoll_wait */
static int make_nb_cntrl_connection(TopoServerInfo *topo_server, int do_not_add_fd_in_epoll) 
{
  int con_state;
  char err_msg[2 * 1024] = "\0";
  char msg_buf[1024];     //This buf is used to print control connection msg
  //char SendMsg[HEART_BEAT_CON_SEND_MSG_BUF_SIZE] = "\0";
  //int send_msg_len = 0;
  //socklen_t errlen;
   
  con_state = -1;

  NSDL2_MON(NULL, NULL, "Method Called, svr_list = %p, svr_list->control_fd = %d, svr_list->con_state = %d",topo_server,topo_server->topo_servers_list->control_fd,topo_server->topo_servers_list->con_state);

  //Go back form here if state is in CONNNECTING and decrement retry attemps if state is CONNECTING 
  //If all retry attemps have been completed then remove form epoll and change state to STOPPED
  if(topo_server->topo_servers_list->con_state == HEART_BEAT_CON_CONNECTING || topo_server->topo_servers_list->con_state == HEART_BEAT_CON_SENDING || topo_server->topo_servers_list->con_state == HEART_BEAT_CON_CONNECTED)
  { 
    NSDL2_MON(NULL, NULL, "Returning because heart beat conn state is either connecting, sending or connected.");
    return 0;
  }

  if((topo_server->topo_servers_list->control_fd = nslb_nb_open_socket((AF_INET), err_msg)) < 0)
  {
    //Error case
    NSDL3_MON(NULL, NULL, "Error: problem in opening socket for %s", topo_server->topo_servers_list->server_ip);
    NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_INFO,
                   __FILE__, (char*)__FUNCTION__,
                   "%s for server %s", err_msg, topo_server->topo_servers_list->server_ip); 
    return -1;
  }
  else
  { 
    //Socket opened successfully so making connection
    NSDL3_MON(NULL, NULL, "Socket opened successfully so making Control connection for fd %d to server ip =%s, port = %d", 
                        topo_server->topo_servers_list->control_fd, 
                        topo_server->topo_servers_list->server_ip, DEFAULT_CMON_PORT);
    int con_ret = nslb_nb_connect(topo_server->topo_servers_list->control_fd, topo_server->topo_servers_list->server_ip, DEFAULT_CMON_PORT, &con_state, err_msg);

    NSDL3_MON(NULL, NULL, "con_ret = %d", con_ret);
    if(con_ret == 0)
    {
      topo_server->topo_servers_list->con_state = HEART_BEAT_CON_CONNECTED; 
      NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE,  EVENT_MAJOR,
                     __FILE__, (char*)__FUNCTION__,
                   "Control connection established for fd %d on server %s. %s", 
                                                     topo_server->topo_servers_list->control_fd, 
                                                     topo_server->topo_servers_list->server_ip, 
                                                     err_msg);

      sprintf(msg_buf, "source address = %s, ", nslb_get_src_addr(topo_server->topo_servers_list->control_fd)); 
      sprintf(msg_buf+strlen(msg_buf), "destination address = %s", nslb_get_dest_addr(topo_server->topo_servers_list->control_fd)); 
      
      if (monitor_debug_level > 0)
      {  
        MLTL1(EL_F, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFO,
                          "Control connection made. fd = %d, %s",
                           topo_server->topo_servers_list->control_fd, 
                           msg_buf);
      }
    }
    else if(con_ret > 0) 
    { 
      // Add in if elseif
      if(con_state == NSLB_CON_CONNECTED)
      {
        topo_server->topo_servers_list->con_state = HEART_BEAT_CON_CONNECTED; 
        topo_server->topo_servers_list->cntrl_conn_retry_attempts = max_cm_retry_count; 
      }
      else if(con_state == NSLB_CON_CONNECTING)
      {
        topo_server->topo_servers_list->con_state = HEART_BEAT_CON_CONNECTING; 
        //Add this socket fd in epoll and wait for an event
        //If EPOLLOUT event comes then send messages
        if(do_not_add_fd_in_epoll == 0)
          add_select_msg_com_con((char *)topo_server->topo_servers_list, topo_server->topo_servers_list->control_fd, EPOLLOUT | EPOLLERR | EPOLLHUP);
      }
    }
    else //(con_ret < 0)
    {
      if((strstr(err_msg, "gethostbyname")) && ((strstr(err_msg, "Unknown server error")) || (strstr(err_msg, "Unknown host")) || (strstr(err_msg, "Host name lookup failure")) || (strstr(err_msg, "No address associated with name"))))
      {
        topo_server->topo_servers_list->cntrl_conn_state |= CTRL_CONN_ERROR;
        update_health_monitor_sample_data(&(hm_data->num_server_error_get_host_by_name));
      }
    
      NSDL3_MON(NULL, NULL, "%s", err_msg);
      NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_CRITICAL,
                   __FILE__, (char*)__FUNCTION__,
                   "Control connection failed for fd %d on server %s. %s", 
                                         topo_server->topo_servers_list->control_fd,
                                         topo_server->topo_servers_list->server_ip, 
                                         err_msg); 

      MLTL1(EL_F, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFO,
                      "Control connection failed for fd = %d, on server %s, source address = %s, destination address = %s. %s",
                       topo_server->topo_servers_list->control_fd, 
                       topo_server->topo_servers_list->server_ip, 
                       nslb_get_src_addr(topo_server->topo_servers_list->control_fd), 
                       nslb_get_dest_addr(topo_server->topo_servers_list->control_fd), err_msg);

      update_health_monitor_sample_data(&hm_data->num_control_conn_failure);
      close(topo_server->topo_servers_list->control_fd);
      topo_server->topo_servers_list->control_fd = -1;
      return -1;
    }

    NSDL3_MON(NULL, NULL, "Adding fd %d into epoll wait for Out event. topo_server->topo_servers_list->con_type = %d, topo_server->server_ptr->topo_servers_list->con_state = %d" 
                                " topo_server->topo_servers_list->cntrl_conn_state = %c", 
                                  topo_server->topo_servers_list->control_fd, 
                                  topo_server->topo_servers_list->con_type, 
                                  topo_server->topo_servers_list->con_state, 
                                  topo_server->topo_servers_list->cntrl_conn_state);
    }
  return 0;
}

inline void add_select_cntrl_conn()
{
  int i,j;
  for(i=0;i<topo_info[topo_idx].total_tier_count;i++)
  {
    for(j=0;j<topo_info[topo_idx].topo_tier_info[i].total_server;j++)
    {
      if((topo_info[topo_idx].topo_tier_info[i].topo_server[i].used_row != -1) && !(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->server_flag & DUMMY))
      {
        if(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->control_fd > 0 && 
                 topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->con_state == HEART_BEAT_CON_CONNECTING)
        {
          add_select_msg_com_con((char *)topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list, 
                   topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->control_fd, EPOLLOUT | EPOLLERR | EPOLLHUP);
        } 
      }
    }
  }  
}


/* This will remove fd from epoll if remove_from_epoll flag is set else not and
 * it will close the connection */ 
static inline void close_ctrl_conn(ServerInfo *svr_list, int remove_from_epoll) //EDIT
{
   NSDL2_MON(NULL, NULL, "Method Called, svr_list->control_fd = %d, svr_list->con_state = %d, remove_from_epoll = %d", 
                          svr_list->control_fd, svr_list->con_state, remove_from_epoll);
   if(svr_list->control_fd < 0)
     return;

   if(remove_from_epoll)
     remove_select_msg_com_con_ex(__FILE__, __LINE__, (char *)__FUNCTION__, svr_list->control_fd);

   svr_list->con_state = HEART_BEAT_CON_RUNTIME_INIT;
   svr_list->bytes_remaining = 0;
   svr_list->send_offset = 0;
   if(svr_list->partial_buf != NULL)
     FREE_AND_MAKE_NULL(svr_list->partial_buf, "svr_list->partial_buf", -1);

   if(close(svr_list->control_fd) < 0)
   {
     MLTL1(EL_F, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFO,
                    "Error in closing fd %d", svr_list->control_fd);
   }
   svr_list->control_fd = -1;

}

/*
 * Make control connetion to server (ip) at port (port) in a blocking mode
 * Called when test is started
 *
 * Returns:
 *   0 - Sucess
 *  -1 - Error and event is generated and log is in monitor.log
 *
 * Adds following in monitor.log
 *   Making control connection to CavMonAgent. ip = %s, port = %d, msg = %s, server_idx = %d",
 *
 */

/* Calling this function from ns_runtime_changes_monitor.c */
/* It will make control connection is not already there 
 * Returns:
 *  >= 0 - Success (old fd or new fd)
 *  -1 - Error in making control connection
 *  -2 - IP/Port is not existing in servers list
 *
 * TODO - Make this non blocking
 */

int check_and_open_nb_cntrl_connection(CM_info *cus_mon_ptr, char *ip, int port, char *msg)
{
  int ContrlConnVal = -2;
  int i,j;
  //ServerInfo *servers_list = NULL;
  //int total_no_of_servers = 0;

  //servers_list = (ServerInfo *) topolib_get_server_info();
  //Total no. of servers in ServerInfo structure
  //total_no_of_servers = topolib_get_total_no_of_servers();

  NSDL3_MON(NULL, NULL, "Method Called ip = %s, port = %d, msg = %s", ip, port, msg);

  for(i = 0; i < topo_info[topo_idx].total_tier_count; i++)
  {
    for(j=0;j<topo_info[topo_idx].topo_tier_info[i].total_server;j++)
    {
      if((topo_info[topo_idx].topo_tier_info[i].topo_server[j].used_row != -1) && !(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->server_flag & DUMMY))
      {
        NSDL3_MON(NULL, NULL, "server_index = %d,topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->server_ip = %s,cus_mon_ptr->cs_ip = %s," "cus_mon_ptr->cs_port = %d",j,topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->server_ip, cus_mon_ptr->cs_ip, cus_mon_ptr->cs_port);

        if(topolib_chk_server_ip_port(cus_mon_ptr->cs_ip, cus_mon_ptr->cs_port, 
               topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->server_ip, NULL, NULL, 0, 0) != 0) //match
        {
          if(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->control_fd < 0)
          {
          //KJ:do we need to check return value
          //Abhishek: We donot need to check the fd, as the control connection never breaks when we stop a monitor. The below code will not be executed as it is called from start monitor.
            ContrlConnVal = make_nb_cntrl_connection(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr, 0);
            break;
          }
          else
            return(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->control_fd);
        }
      }
    }
  } 
  return ContrlConnVal;
}

/*Calling this function from:
 *   send_end_msg_to_all_cavmonserver_used(), 
 *   send_testrun_running_msg_to_all_cavmonserver_used() and 
 *   send_testrun_starts_msg_to_all_cavmonserver_used()
 * for creating connection either blocking/non-blocking.
 * blocking -> if first time
 * non-blocking -> if more than one time
*/
static int make_control_conn_blocking_or_nb(char *ip, int port, char *msg, int msg_len, int server_idx ,int do_not_add_fd_in_epoll,int i)
{
  int ret = 0;
  char msg_buf[32] = {0};

  ServerInfo *servers_list =topo_info[topo_idx].topo_tier_info[i].topo_server[server_idx].server_ptr->topo_servers_list ;

  //servers_list = (ServerInfo *) topolib_get_server_info();

  NSDL3_MON(NULL, NULL, "Method Called, ip = %s, port = %d, msg = %s, msg_len = %d, servers_list[%d].control_fd = %d,"
                        "servers_list->con_state = %d,"
                        "servers_list->cntrl_conn_retry_attempts = %d,"
                        "servers_list->cntrl_conn_state = %c",
                         ip, port, msg, msg_len,servers_list->control_fd,server_idx, servers_list->con_state,
                         servers_list->cntrl_conn_retry_attempts,servers_list->cntrl_conn_state);

  // if ip:port is coming that it will connect to port given along with ip not the passed one

  if(servers_list->cntrl_conn_retry_attempts == 0)
  {
    if(servers_list->con_state == HEART_BEAT_CON_STOPPED)
      return -1;        //con_state eql to HEART_BEAT_CON_STOPPED means all retry count is completed and its logged once no need to log again.
    NSDL3_MON(NULL, NULL, "All control connection retry count has been completed for server %s.",
                           servers_list->server_ip);
    NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_MAJOR,
                               __FILE__, (char*)__FUNCTION__,
                               "All control connection retry count has been completed for server %s",
                                servers_list->server_ip);

    MLTL1(EL_F, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFO,
                      "All control connection retry count has been completed for server %s", servers_list->server_ip);

    servers_list->con_state = HEART_BEAT_CON_STOPPED;
    return -1;
}

  if(servers_list->cntrl_conn_state & CTRL_CONN_ERROR)
  {
  /*   NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_MAJOR,
                               __FILE__, (char*)__FUNCTION__,
                               "Got unknown server error from function gethostbyname for %s while making control connection. So, changing retry count to 0 and state to stopped.",
                                servers_list->server_ip);

     MLTL1(EL_F, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFO,
                      "Got unknown server error from function gethostbyname for %s while making control connection. So, changing retry attempts count to 0 and state to stopped.", servers_list->server_ip);
     servers_list->cntrl_conn_retry_attempts = 0;
     servers_list->con_state = HEART_BEAT_CON_STOPPED;*/
    return -1;
  }

  if(servers_list->cntrl_conn_retry_attempts == -1)
  {
    NSDL3_MON(NULL, NULL, "Retry flag is not set hence no control connection retry will be done for server %s.",
                           servers_list->server_ip);
    NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_MAJOR,
                               __FILE__, (char*)__FUNCTION__,
                               "Retry flag is not set hence no control connection retry will be done for server %s",
                                servers_list->server_ip);
    return -1;
  }

  if(!do_not_add_fd_in_epoll)
  {
    sprintf(msg_buf, "For retry attempt %d",
           ((max_cm_retry_count - servers_list->cntrl_conn_retry_attempts) == 0)?
           1:(max_cm_retry_count - servers_list->cntrl_conn_retry_attempts) + 1);
  }

NSDL3_MON(NULL, NULL, "Making control connection on server %s. %s", servers_list->server_ip, msg_buf);

  if(monitor_debug_level > 0)
  {
    MLTL1(EL_F, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFO,
                    "Making control connection on server %s. %s", servers_list->server_ip, msg_buf);
  }

  ret = make_nb_cntrl_connection(topo_info[topo_idx].topo_tier_info[i].topo_server[server_idx].server_ptr, do_not_add_fd_in_epoll);

  if(servers_list->con_state != HEART_BEAT_CON_CONNECTED)
  {
    if(servers_list->cntrl_conn_retry_attempts > 0)
      servers_list->cntrl_conn_retry_attempts--;
  }

  return ret;

  NSDL3_MON(NULL, NULL, "Method end. Now control connection fd is = %d, servers_list->cntrl_conn_state = %c, "
                        "servers_list->con_state = %d", servers_list->control_fd,
                         servers_list->cntrl_conn_state, servers_list->con_state);
  return 0;
}




/* Handled partial write here
 * This is to send msg on non-blocking connection.
 * Because we are calling this funtion from 2 places: (1) send_control_msg_to_server() ->here we do not need to remove from epoll in any case
 *                                                    (2) handle_connect_and_send_ctrl_msg() -> here we have to remove from epoll
 *                                                    hence passing flag 'flag_to_rem_frm_epoll'.
 * Removing from epoll in two cases: (1) send failure -> remove + close fd
 *                                   (2) send success -> only remove fd
 * In case of partial send -> will keep this fd in epoll_wait.
 *
 * */ 

static void send_ctrl_msg_on_nb_conn(ServerInfo *svr_list, char *send_msg, int send_msg_len, int flag_to_rem_frm_epoll)
{
  char *buf_ptr = send_msg;
  int bytes_sent = -1, remaining_bytes = -1;
  char msg_buf[1024];     //buf to hold msg control msg.
  NSDL2_MON(NULL, NULL, "Method called, svr_list = %p, send_msg = [%s], send_msg_len = %d, flag_to_rem_frm_epoll = %d", 
                         svr_list, send_msg?send_msg:NULL, send_msg_len, flag_to_rem_frm_epoll);

  //if(send_msg_len) //First time 
  if(svr_list->partial_buf == NULL && svr_list->con_state != HEART_BEAT_CON_SENDING) //First time 
  {
    buf_ptr = send_msg;
    remaining_bytes = send_msg_len;
  }
  else //If there is partial send
  {
    buf_ptr = svr_list->partial_buf;
    remaining_bytes = svr_list->bytes_remaining;
  }

  NSDL2_MON(NULL, NULL, "Sending control msg: remaining_bytes = %d", remaining_bytes); 
  // Send MSG to CMON
  while(remaining_bytes)
  {
    NSDL2_MON(NULL, NULL, "Send control MSG: svr_list->control_fd = %d, remaining_bytes = %d,  send_offset = %d, buf = [%s]", 
                           svr_list->control_fd, remaining_bytes, svr_list->send_offset, buf_ptr);

    if((bytes_sent = send(svr_list->control_fd, buf_ptr + svr_list->send_offset, remaining_bytes, 0)) < 0)
    {
      NSDL2_MON(NULL, NULL, "bytes_sent = %d, errno = %d, error = %s", bytes_sent, errno, nslb_strerror(errno));
      if(errno == EAGAIN) //If message send partially
      {
        if(svr_list->partial_buf == NULL)
        {
          MY_MALLOC(svr_list->partial_buf, (remaining_bytes + 1), "Malloc buffer for partial send", -1);
        }
        strcpy(svr_list->partial_buf, buf_ptr + svr_list->send_offset);
        svr_list->bytes_remaining = remaining_bytes;
        svr_list->con_state = HEART_BEAT_CON_SENDING;

        NSDL3_MON(NULL, NULL, "Partial Send control msg: svr_list->control_fd = %d, remaining_bytes = %d, send_offse = %d," 
                              "remaning buf = [%s]", svr_list->control_fd, svr_list->bytes_remaining, 
                               svr_list->send_offset, buf_ptr + svr_list->send_offset);
       
        break;
      }
      if(errno == EINTR) //If any intrept occurr
      {
        NSDL2_MON(NULL, NULL, "Interrupted. Continuing...");
        continue;
      }
      else 
      {
        //sending msg failed for this monitor, close fd & send for nxt monitor
        NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_MAJOR,
                                __FILE__, (char*)__FUNCTION__,
                       "Control message send failure with server %s", svr_list->server_ip);

        MLTL1(EL_F, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFO,
                           "Control message send failure with server %s", svr_list->server_ip);
        close_ctrl_conn(svr_list, flag_to_rem_frm_epoll);
        return;
      } 
    }

    NSDL2_MON(NULL, NULL, "bytes_sent = %d", bytes_sent);

    remaining_bytes -= bytes_sent;
    svr_list->send_offset += bytes_sent; 
    
    if(bytes_sent == 0)
    {
      NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_MAJOR,
                                __FILE__, (char*)__FUNCTION__,
                       "Control message send failure with server %s", svr_list->control_fd);
      close_ctrl_conn(svr_list, flag_to_rem_frm_epoll);
      return;
    }
  } //End while Loop 

  if(remaining_bytes == 0)
  {
      sprintf(msg_buf, "source address = %s, ", nslb_get_src_addr(svr_list->control_fd));
      sprintf(msg_buf+strlen(msg_buf), "destination address = %s", nslb_get_dest_addr(svr_list->control_fd));

      NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_INFO,
                                __FILE__, (char*)__FUNCTION__,
		                         "Control message sent successfully, %s", msg_buf); 

     if(flag_to_rem_frm_epoll)
     {
       //if bytes sent successfully then remove from epoll
       remove_select_msg_com_con_ex(__FILE__, __LINE__, (char *)__FUNCTION__, svr_list->control_fd);
     }
   
     svr_list->con_state = HEART_BEAT_CON_RUNTIME_INIT;
     //this is to decide at parent epoll, which msg we need to send 'test_run_start' or 'test_run_running'
     svr_list->start_msg_sent = 1;
     svr_list->bytes_remaining = 0;
     svr_list->send_offset = 0;
     if(svr_list->partial_buf != NULL)
       FREE_AND_MAKE_NULL(svr_list->partial_buf, "svr_list->partial_buf", -1);
  }
}

/*This is the common function to send message on both blocking and non-blocking connection. 
 * On blocking -> send directly
 * On non-blocking -> send only if state is HEART_BEAT_CON_CONNECTED
 *                    If state is CONNECTING -> will be sending this from epoll_wait.*/
static void send_control_msg_to_server(char *ip, int port, char *msg, int msg_len, int server_idx, int close_con, int tier_idx) 
{
  NSDL3_MON(NULL, NULL, "msg %s", msg);
 
  ServerInfo *topo_servers_list=topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->topo_servers_list;   
  //ServerInfo *servers_list = NULL;

  //servers_list = (ServerInfo *) topolib_get_server_info();

  //send control message on non-blocking connection
  NSDL3_MON(NULL, NULL, "Going to send message on non-blocking connection con_state is %d, topo_servers_list->control_fd = %d", 
  topo_servers_list->con_state, topo_servers_list->control_fd);

 if((topo_servers_list->con_state == HEART_BEAT_CON_CONNECTED) || (topo_servers_list->con_state == HEART_BEAT_CON_RUNTIME_INIT))
    send_ctrl_msg_on_nb_conn(topo_servers_list, msg, msg_len, HEART_BEAT_CON_NOT_REMOVE_FROM_EPOLL); 
  else
    NSDL3_MON(NULL, NULL, "topo_servers_list->con_state %d is not HEART_BEAT_CON_CONNECTED", topo_servers_list->con_state);
 
  if(close_con) 
  {
    NSDL3_MON(NULL, NULL, "Closing connection to CavMonAgent. ip = %s, port = %d, msg = %s, msg_len = %d", ip, port, msg, msg_len);
    if (close(topo_servers_list->control_fd) < 0)  
      //KJ event log
  topo_servers_list->control_fd = -1; // Make it -1
  topo_servers_list->cntrl_conn_retry_attempts = max_cm_retry_count;
  } 
  NSDL3_MON(NULL, NULL, "Function ends. Returning...");
  return;
}

static inline void make_test_run_start_ctrl_msg(ServerInfo *svr_list, char *msg_buf, int *msg_len)
{
  char *cmon_settings_buf = NULL;

    if(svr_list->cmon_settings != NULL)
      cmon_settings_buf = svr_list->cmon_settings;
    else if(cmon_settings != NULL)
      cmon_settings_buf = cmon_settings;

    if(svr_list->origin_cmon != NULL)
    {
      if(cmon_settings_buf != NULL)
        *msg_len = sprintf(msg_buf, "test_run_starts:MON_TEST_RUN=%d;ProgressMsec=%d;MON_NS_VER=%s;%s;ORIGIN_CMON=%s;MON_PARTITION_IDX=%lld\n", testidx,global_settings->progress_secs,ns_version,cmon_settings_buf, svr_list->origin_cmon, g_start_partition_idx);
      else
        *msg_len = sprintf(msg_buf, "test_run_starts:MON_TEST_RUN=%d;ProgressMsec=%d;MON_NS_VER=%s;ORIGIN_CMON=%s;MON_PARTITION_IDX=%lld\n", testidx,global_settings->progress_secs,ns_version, svr_list->origin_cmon, g_start_partition_idx);
    }
    else
    {
      if(cmon_settings_buf != NULL)
        *msg_len = sprintf(msg_buf, "test_run_starts:MON_TEST_RUN=%d;ProgressMsec=%d;MON_NS_VER=%s;%s;MON_PARTITION_IDX=%lld\n", testidx,global_settings->progress_secs,ns_version,cmon_settings_buf, g_start_partition_idx);
      else
        *msg_len = sprintf(msg_buf, "test_run_starts:MON_TEST_RUN=%d;ProgressMsec=%d;MON_NS_VER=%s;MON_PARTITION_IDX=%lld\n", testidx,global_settings->progress_secs,ns_version, g_start_partition_idx);
    }

      NSDL3_MON(NULL, NULL, "testrun_start_msg_len = [%d], testrun_start_msg_buf = [%s]", *msg_len, msg_buf);
}

//close control connection 
static void close_control_conn(TopoServer *topo_server) 
{
  NSDL3_MON(NULL, NULL, "Closing control connection fd '%d' for server '%s'.",topo_server->server_ptr->topo_servers_list->control_fd, topo_server->server_ptr->topo_servers_list->server_ip);
  if (close(topo_server->server_ptr->topo_servers_list->control_fd) < 0)  
    MLTL1(EL_F, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFO,
                       "Error in closing control connection fd %d for server %s.", (topo_server->server_ptr->topo_servers_list->control_fd), 
                        (topo_server->server_ptr->topo_servers_list->server_ip));
  
  topo_server->server_ptr->topo_servers_list->control_fd = -1; // Make it -1
  topo_server->server_ptr->topo_servers_list->cntrl_conn_retry_attempts = max_cm_retry_count;
}

void send_end_msg_to_NDC()
{
  char end_testrun_msg[1024];
  int testidx = start_testidx;

  sprintf(end_testrun_msg, "end_test_run:MON_TEST_RUN=%d;MON_NS_VER=%s;MON_PARTITION_IDX=%lld\n", testidx,ns_version, g_start_partition_idx);
  
  write_msg(&ndc_data_mccptr, (char *)&end_testrun_msg, strlen(end_testrun_msg), 0, DATA_MODE);
}

/* Send end test run message to all server which are used*/
/* Called from ns_parent after test is over */

void send_end_msg_to_all_cavmonserver_used() 
{

  //int server_index = 0;
  int make_conn_ret_val = 0;
  char end_tetstrun_msg[1024];
  int testidx = start_testidx;
  int end_msg_len = 0;
  int i,j;
  
  for(i=0;i<topo_info[topo_idx].total_tier_count;i++)
  {
    for(j=0;j<topo_info[topo_idx].topo_tier_info[i].total_server;j++) 
    {
      //'-1' means all monitor over for this server hence close control connection 
      if((topo_info[topo_idx].topo_tier_info[i].topo_server[j].used_row) != -1 && !(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->server_flag & DUMMY))
      {  
        if(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->cmon_monitor_count < 0)
      {
        if(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->control_fd >0)
          close_control_conn(&topo_info[topo_idx].topo_tier_info[i].topo_server[j]);
      } 


        if(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->cmon_monitor_count) 
        {
          if(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->origin_cmon != NULL)
            end_msg_len = sprintf(end_tetstrun_msg, "end_test_run:MON_TEST_RUN=%d;MON_NS_VER=%s;ORIGIN_CMON=%s;MON_PARTITION_IDX=%lld\n", testidx,ns_version, topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->origin_cmon, g_start_partition_idx);
          else
            end_msg_len = sprintf(end_tetstrun_msg, "end_test_run:MON_TEST_RUN=%d;MON_NS_VER=%s;MON_PARTITION_IDX=%lld\n", testidx,ns_version, g_start_partition_idx);

          if(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->control_fd < 0) 
          {
            /*if(cm_retry_flag)
            servers_list[server_index].cntrl_conn_retry_attempts = max_cm_retry_count;
        else
          servers_list[server_index].cntrl_conn_retry_attempts = -1;*/

        //7891 is default port it may possible port is comming along with IP
            make_conn_ret_val = make_control_conn_blocking_or_nb(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->server_ip, DEFAULT_CMON_PORT, end_tetstrun_msg, end_msg_len, j, 0,i);
          }
          if(make_conn_ret_val == 0)
          {
            send_control_msg_to_server(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->server_ip, DEFAULT_CMON_PORT, end_tetstrun_msg, end_msg_len, j, SEND_END_MSG_AND_CLOSE_CONN,i);     
          }
          topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->cmon_monitor_count = 0; // Unset it
        }
      }
    }
  }
}
/* Send test run running message to all server*/
/* Called from deliver_report.c after test is over */
void send_testrun_running_msg_to_all_cavmonserver_used(int seq_num) 
{ 
  int running_testrun_msg_len = 0;
  int make_conn_ret_val = 0;
  //int server_index = 0;
  char running_tetstrun_msg[4096];
  char *cmon_settings_buf = NULL;
  int testidx = start_testidx;


  //test_run_running:MON_TEST_RUN=<TR Number>;ProgressMsec=<ProgressInteravalInMS>;SeqNum=<seq_num>
  
  NSDL3_MON(NULL, NULL, "Method Called, running_tetstrun_msg = %s, running_testrun_msg_len = %d\n", running_tetstrun_msg, running_testrun_msg_len);

  //for(server_index = 0; server_index < total_no_of_servers; server_index++) 

    //'-1' means all monitor over for this server hence close control connection 
    //if(servers_list[server_index].cmon_monitor_count == -1 && servers_list[server_index].control_fd > 0) 
    //if(servers_list[server_index].cmon_monitor_count == -1) 
    
    
    for(int i=0;i<topo_info[topo_idx].total_tier_count;i++)
    {
      for(int j=0;j<topo_info[topo_idx].topo_tier_info[i].total_server;j++)
      {
        if((topo_info[topo_idx].topo_tier_info[i].topo_server[j].used_row != -1) && !(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->server_flag & DUMMY))
        {
          if(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->cmon_monitor_count < 0) 
          {
            if(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->control_fd > 0)
              close_control_conn(&topo_info[topo_idx].topo_tier_info[i].topo_server[j]);
 
            continue;
          }

          if(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->cmon_monitor_count)
          {
            if(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->cmon_settings != NULL)
              cmon_settings_buf = topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->cmon_settings;
            else if(cmon_settings != NULL)
              cmon_settings_buf = cmon_settings;

            if(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->origin_cmon != NULL)
            { 
              if(cmon_settings_buf != NULL)
              {
                running_testrun_msg_len = sprintf(running_tetstrun_msg, "test_run_running:MON_TEST_RUN=%d;ProgressMsec=%d;SeqNum=%d;MON_NS_VER=%s;%s;ORIGIN_CMON=%s;MON_PARTITION_IDX=%lld\n", testidx,global_settings->progress_secs,seq_num,ns_version,cmon_settings_buf, topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->origin_cmon, g_start_partition_idx);
              }
              else
              {
                running_testrun_msg_len = sprintf(running_tetstrun_msg, "test_run_running:MON_TEST_RUN=%d;ProgressMsec=%d;SeqNum=%d;MON_NS_VER=%s;ORIGIN_CMON=%s;MON_PARTITION_IDX=%lld\n", testidx,global_settings->progress_secs,seq_num,ns_version, topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->origin_cmon, g_start_partition_idx);
              }
            }
            else
            {
              if(cmon_settings_buf != NULL)
                running_testrun_msg_len = sprintf(running_tetstrun_msg, "test_run_running:MON_TEST_RUN=%d;ProgressMsec=%d;SeqNum=%d;MON_NS_VER=%s;%s;MON_PARTITION_IDX=%lld\n", testidx,global_settings->progress_secs,seq_num,ns_version,cmon_settings_buf, g_start_partition_idx);
              else
                running_testrun_msg_len = sprintf(running_tetstrun_msg, "test_run_running:MON_TEST_RUN=%d;ProgressMsec=%d;SeqNum=%d;MON_NS_VER=%s;MON_PARTITION_IDX=%lld\n", testidx,global_settings->progress_secs,seq_num,ns_version, g_start_partition_idx);
            }

            NSDL3_MON(NULL, NULL, "running_testrun_msg_len = [%d], running_tetstrun_msg = [%s]", running_testrun_msg_len, running_tetstrun_msg);

            if(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->control_fd < 0)
            {

              NSDL3_MON(NULL, NULL, "FD is %d less than 0. Going to make control connection.", (topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->control_fd));
               //7891 is default port it may possible port is comming along with IP
               make_conn_ret_val = make_control_conn_blocking_or_nb(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->server_ip, DEFAULT_CMON_PORT, running_tetstrun_msg, running_testrun_msg_len, j, 0,i);
             }
             if(make_conn_ret_val == 0)
             {
               NSDL3_MON(NULL, NULL, "make_conn_ret_val is %d. Going to send control message on fd %d.",
                    make_conn_ret_val,topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->control_fd);

               send_control_msg_to_server(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->server_ip, DEFAULT_CMON_PORT, running_tetstrun_msg, running_testrun_msg_len, j, SEND_CTRL_MSG_AND_DO_NOT_CLOSE_CONN,i);
             }
          }
    
          make_conn_ret_val = 0;
        }
  
      }
    }
}
/*Send test run starts message to all server*/
/* Called from ns_parent.c after test is over */
void send_testrun_starts_msg_to_all_cavmonserver_used() 
{
  //int server_index = 0; 
  int make_conn_ret_val = 0;
  int start_testrun_msg_len = 0;
  char testrun_starts_msg[4096];
  char *cmon_settings_buf = NULL;

  //ServerInfo *servers_list = NULL;
  //int total_no_of_servers = 0;
  int cmon_monitor_count;
  //servers_list = (ServerInfo *) topolib_get_server_info();
  //Total no. of servers in ServerInfo structure
  //total_no_of_servers = topolib_get_total_no_of_servers();

  //test_run_starts:MON_TEST_RUN=1234;ProgressMsec=10000

  NSDL2_MON(NULL, NULL, "Method Called, testrun_starts_msg = %s, "
                        "start_testrun_msg_len = %d, \n", 
                        testrun_starts_msg, start_testrun_msg_len);

  //for(server_index = 0; server_index < total_no_of_servers; server_index++) 
  
   for(int i=0;i<topo_info[topo_idx].total_tier_count;i++)
   {
     for(int j=0;j<topo_info[topo_idx].topo_tier_info[i].total_server;j++)
     {
       if((topo_info[topo_idx].topo_tier_info[i].topo_server[j].used_row != -1) && !(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->server_flag & DUMMY))
       { 
         if(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->cmon_monitor_count < 0)
         {
           if(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->control_fd >0)
           { 
             close_control_conn(&topo_info[topo_idx].topo_tier_info[i].topo_server[j]);
             continue;
           }
         }
         if(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->cmon_settings != NULL)
         {
           MY_MALLOC(cmon_settings_buf, strlen(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->cmon_settings) + 1, "Malloc cmon_settings_buf", -1); 
           strcpy(cmon_settings_buf, topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->cmon_settings);
         }
         cmon_monitor_count=topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->cmon_monitor_count;
    
         NSDL2_MON(NULL, NULL, "SEND MSG :topo_info[topo_idx].topo_tier_info[i].topo_server[%d].server_ptr->topo_servers_list->cmon_monitor_count = %d, topo_info[topo_idx].topo_tier_info[i].topo_server[%d].server_ptr->topo_servers_list->origin_cmon = %s,topo_info[topo_idx].topo_tier_info[i].topo_server[%d].server_ptr->topo_servers_list->server_ip = %s,topo_info[topo_idx].topo_tier_info[i].topo_server[%d].server_ptr->topo_servers_list->cmon_settings = %s, cmon_settings = %s,cmon_settings_buf = %s", j,cmon_monitor_count, j,topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->origin_cmon, j,topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->server_ip, topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->cmon_settings, cmon_settings, cmon_settings_buf);

       if(cmon_monitor_count)
       {
        ServerInfo *topo_servers_list = topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list;
        //  TODO CALL FUNCTION .....
    
        //make_test_run_start_ctrl_msg(&topo_info[topo_idx].topo_tier_info[i].topo_server[j], testrun_starts_msg, &start_testrun_msg_len); 
         make_test_run_start_ctrl_msg(topo_servers_list, testrun_starts_msg, &start_testrun_msg_len); 
         if(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->control_fd < 0)
         {
           if(cm_retry_flag)
             topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->cntrl_conn_retry_attempts = max_cm_retry_count;
          else
            topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->cntrl_conn_retry_attempts = -1;

          make_conn_ret_val = make_control_conn_blocking_or_nb(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->server_ip, DEFAULT_CMON_PORT, testrun_starts_msg, start_testrun_msg_len, j, 1,i);
         }
         if(make_conn_ret_val == 0)
         {
           send_control_msg_to_server(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->server_ip,DEFAULT_CMON_PORT, testrun_starts_msg, start_testrun_msg_len, j, SEND_CTRL_MSG_AND_DO_NOT_CLOSE_CONN,i);     
         }
        }
       }
     }
   }
}

/*---------------- | Functions for HEART BEAT Disaster Recovery | --------------------*/ 

/*This is to make and send control message on epollout event */
static inline void make_test_run_running_ctrl_msg(ServerInfo *svr_list, char *msg_buf, int *msg_len, int seq_num)
{
  char *cmon_settings_buf = NULL;

    if(svr_list->cmon_settings != NULL)
      cmon_settings_buf = svr_list->cmon_settings;
    else if(cmon_settings != NULL)
      cmon_settings_buf = cmon_settings;

    if(svr_list->origin_cmon != NULL)
    {
      if(cmon_settings_buf != NULL)
        *msg_len = sprintf(msg_buf, "test_run_running:MON_TEST_RUN=%d;ProgressMsec=%d;SeqNum=%d;MON_NS_VER=%s;%s;ORIGIN_CMON=%s;MON_PARTITION_IDX=%lld\n", testidx,global_settings->progress_secs,seq_num,ns_version,cmon_settings_buf, svr_list->origin_cmon, g_start_partition_idx);
      else
        *msg_len = sprintf(msg_buf, "test_run_running:MON_TEST_RUN=%d;ProgressMsec=%d;SeqNum=%d;MON_NS_VER=%s;ORIGIN_CMON=%s;MON_PARTITION_IDX=%lld\n", testidx,global_settings->progress_secs,seq_num,ns_version, svr_list->origin_cmon, g_start_partition_idx);
    }
    else
    {
      if(cmon_settings_buf != NULL)
        *msg_len = sprintf(msg_buf, "test_run_running:MON_TEST_RUN=%d;ProgressMsec=%d;SeqNum=%d;MON_NS_VER=%s;%s;MON_PARTITION_IDX=%lld\n", testidx,global_settings->progress_secs,seq_num,ns_version,cmon_settings_buf, g_start_partition_idx);
      else
        *msg_len = sprintf(msg_buf, "test_run_running:MON_TEST_RUN=%d;ProgressMsec=%d;SeqNum=%d;MON_NS_VER=%s;MON_PARTITION_IDX=%lld\n", testidx,global_settings->progress_secs,seq_num,ns_version, g_start_partition_idx);
    }

      NSDL3_MON(NULL, NULL, "testrun_start_msg_len = [%d], testrun_start_msg_buf = [%s]", *msg_len, msg_buf);
}


/*Wrapper function for close_ctrl_conn() */
static void rem_hb_fd_frm_epoll(void *ptr)
{
  ServerInfo *svr_list = (ServerInfo *) ptr;

  NSDL2_MON(NULL, NULL, "Method called, svr_list = %p, svr_list->control_fd = %d, svr_list->con_state = %d", 
                        svr_list, svr_list->control_fd, svr_list->con_state);

  //remove fd from epoll
  //close fd
  close_ctrl_conn(svr_list, HEART_BEAT_CON_REMOVE_FROM_EPOLL);
  svr_list->cntrl_conn_retry_attempts = max_cm_retry_count;
}

/*Called from wait_forever.c on epollout event */
static void handle_connect_and_send_ctrl_msg(void *ptr)
{
  int con_state;
  static int seq_num = 1;
  int send_msg_len = 0;
  char SendMsg[HEART_BEAT_CON_SEND_MSG_BUF_SIZE] = "\0";
  char err_msg[1024] = "\0";

  ServerInfo *svr_list = (ServerInfo *) ptr;

  NSDL2_MON(NULL, NULL, "Method called, svr_list = %p, svr_list->control_fd = %d, svr_list->con_state = %d", 
                        svr_list, svr_list->control_fd, svr_list->con_state);

  if(svr_list->control_fd < 0) 
  {
    return;
  } 

  if(svr_list->con_state == HEART_BEAT_CON_CONNECTING)
  {
    //Again send connect request
    NSDL3_MON(NULL, NULL, "Since control connection is in HEART_BEAT_CON_CONNECTING so try to Reconnect for fd %d and to server ip = %s, port = %d", 
                           svr_list->control_fd, svr_list->server_ip, DEFAULT_CMON_PORT);

    if(nslb_nb_connect(svr_list->control_fd, svr_list->server_ip, DEFAULT_CMON_PORT, &con_state, err_msg) != 0 &&
       con_state != NSLB_CON_CONNECTED)
    {
      NSDL3_MON(NULL, NULL, "err_msg = %s", err_msg);
      NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_CRITICAL,
                                __FILE__, (char*)__FUNCTION__,
                               "Retry control connection failed on server %s. %s",
                               svr_list->server_ip, err_msg);
 
      MLTL1(EL_F, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFO,
                                 "Retry control connection failed on server %s. %s",
                                 svr_list->server_ip, err_msg);

      close_ctrl_conn(svr_list, HEART_BEAT_CON_REMOVE_FROM_EPOLL);

      if((strstr(err_msg, "gethostbyname")) && (strstr(err_msg, "Unknown server error")))
        svr_list->cntrl_conn_state |= CTRL_CONN_ERROR;

      update_health_monitor_sample_data(&hm_data->num_control_conn_failure);

      return;
    }

    //Since control connection made succefully so reset retry attemps
    svr_list->cntrl_conn_retry_attempts = max_cm_retry_count;
    svr_list->con_state = HEART_BEAT_CON_CONNECTED;
    NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_MAJOR,
                                __FILE__, (char*)__FUNCTION__,
                                "Control connection established on server %s. %s",
                                svr_list->server_ip, err_msg);
      
      if(monitor_debug_level > 0)
      {
        MLTL1(EL_F, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFO,
                                   "Control connection made on server %s",
                                    svr_list->server_ip);
      }
  }
  else
  {
    MLTL1(EL_F, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFO,
                      "Error: control connection state is (%d) which is not HEART_BEAT_CON_CONNECTING. Then it should not come here.",
                       svr_list->con_state);
    update_health_monitor_sample_data(&hm_data->num_control_conn_failure);
  }
  
  NSDL3_MON(NULL, NULL, "Making control connection message.");

  if(svr_list->start_msg_sent == 1 )
  {
      make_test_run_running_ctrl_msg(svr_list, SendMsg, &send_msg_len, seq_num);
      seq_num++;
  }
  else
     make_test_run_start_ctrl_msg(svr_list, SendMsg, &send_msg_len);

  NSDL3_MON(NULL, NULL, "After making control connection message, SendMsg = [%s], send_msg_len = %d", 
                           SendMsg, send_msg_len);

  send_ctrl_msg_on_nb_conn(svr_list, SendMsg, send_msg_len, HEART_BEAT_CON_REMOVE_FROM_EPOLL);


  //sent start msg, now set start_msg_sent to 1
  if(svr_list->start_msg_sent == 0)
  {
    if(svr_list->con_state == HEART_BEAT_CON_RUNTIME_INIT)
      svr_list->start_msg_sent = 1;
  }

  NSDL3_MON(NULL, NULL, " After sending control connection message, svr_list->con_type = %d, svr_list->con_state = %d," 
                        " svr_list->cntrl_conn_state = %c", svr_list->con_type, svr_list->con_state, svr_list->cntrl_conn_state);
}

/*Called from wait_forever.c on epollout event */
void handle_server_ctrl_conn_event(struct epoll_event *pfds, int i, void *ptr)
{
  if(pfds[i].events & EPOLLOUT)
  {
    NSDL2_MON(NULL, NULL, "HEART_BEAT: got EPOLLOUT on control connection.");
    handle_connect_and_send_ctrl_msg(ptr);
  }
  else if(pfds[i].events & EPOLLERR)
  {
    NSDL2_MON(NULL, NULL, "HEART_BEAT: got EPOLLERR on control connection. err_num = %d, error = %s", errno, nslb_strerror(errno));
    rem_hb_fd_frm_epoll(ptr);
  }
  else if(pfds[i].events & EPOLLHUP)
  {
    NSDL2_MON(NULL, NULL, "HEART_BEAT: got EPOLLHUP on control connection.");
    rem_hb_fd_frm_epoll(ptr);
  }
  else
  {
    NSDL2_MON(NULL, NULL, "HEART_BEAT: Unknown case.");
    NSTL1_OUT(NULL, NULL, "HEART_BEAT: Unknown case.\n");
  }
}

