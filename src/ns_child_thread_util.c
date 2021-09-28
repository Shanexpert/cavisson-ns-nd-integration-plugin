#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <regex.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"

#include "nslb_time_stamp.h"
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
#include "weib_think.h"
#include "ns_sock_list.h"
#include "src_ip.h"
#include "unique_vals.h"
#include "divide_users.h"
#include "divide_values.h"
#include "child_init.h"
#include "eth.h"
#include "wait_forever.h"
#include "ns_log.h"
#include "ns_child.h"
#include "ns_schedule_phases.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "ns_msg_com_util.h"
#include "ns_summary_rpt.h"
//#include "ns_handle_read.h"
#include "ns_gdf.h"
#include "ns_wan_env.h"
#include "ns_replay_access_logs.h"
#include "ns_schedule_ramp_up_fcu.h"
#include "ns_schedule_ramp_down_fcu.h"
#include "ns_schedule_pause_and_resume.h"
#include "ns_user_monitor.h"
#include "ns_custom_monitor.h"
#include "ns_sock_com.h"
#include "ns_vuser_tasks.h"
#include "ns_vuser.h"
#include "ns_child_thread_util.h"
#include "ns_vuser_thread.h"
#include "ns_page_think_time.h"
#include "nslb_sock.h"
#include "ns_alloc.h"
#include "ns_trans.h"
#include "ns_msg_com_util.h"
#include "ns_event_log.h"
#include "ns_event_id.h"
#include "ns_string.h"
#include "ns_sync_point.h"

#include "nslb_util.h"
#include "ns_exit.h"

#include "ns_websocket.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"
#include "ns_socket.h"

#define MAX_READ_LENGTH 16000
#define TRANS_VAR_SIZE 1027 //1024 + 2 for { } 1 for \0

/*bug66693 - In case of thread mode, vptr->thdd_ptr->tx_name and vptr->thdd_ptr->eval_buf is a union hence in case of using search parameter for dynamic transactions while starting transaction in tx_start_with_name ns_eval_string was using both in memcpy, hence corrupting the same. Changing this to a local buffer on stack in case of parameterisation*/

#define THREADMODE_SET_TX_NAME(loc_tx_name_var, loc_tx_name, thdd_tx_name) \
{\
  if(thdd_tx_name[0] == '{'){\
    strncpy(loc_tx_name_var, thdd_tx_name, TRANS_VAR_SIZE);\
    loc_tx_name = loc_tx_name_var;\
  }\
  else{\
    loc_tx_name = thdd_tx_name;\
  }\
}

extern int add_user_data_point(int rptGroupID, int rptGraphID, double value);
extern int nsi_sockjs_close(VUser *vptr);

int handle_msg_from_vuser_thread(Msg_com_con *mcctptr, char *read_buf)
{ 
  
  if(!mcctptr->vptr)
  {
    NSDL3_SCHEDULE(NULL, NULL, "Error mcctptr->vptr is null for fd= %d", (mcctptr->nvm_thread_fd));
    return -1;
  }
  
   NSDL3_SCHEDULE(mcctptr->vptr, NULL, "Method Called, mcctptr = %p, mcctptr->vptr = %p, user_index = %d ", mcctptr, mcctptr->vptr, mcctptr->vptr->user_index);

    NSTL3(mcctptr->vptr, NULL, "Method Called, mcctptr = %p, mcctptr->vptr = %p, user_index = %d", mcctptr, mcctptr->vptr, mcctptr->vptr->user_index);

  if(vutd_read_msg(mcctptr->nvm_thread_fd, mcctptr->read_buf) < 0)
  {
     NSDL3_SCHEDULE(mcctptr->vptr, NULL, "NVM: Error in getting msg from thread on fd = %d", (mcctptr->nvm_thread_fd));
     NSTL1(mcctptr->vptr,NULL, "NVM: Error in gettting msg from thread on fd = %d\n", (mcctptr->nvm_thread_fd));
     return -1;
  }
  
  VUser *vptr = mcctptr->vptr;
  TLS_SET_VPTR(vptr);

  int page_think_time;
  char *tx_name;
  char *end_tx_name;
  char tx_name_var[TRANS_VAR_SIZE];
  char end_tx_name_var[TRANS_VAR_SIZE];
  int status;
  char *sp_name;
  int  ret_val, rptGroupID, rptGraphID, conn_id, time_out;
  double value;
  
  NSDL3_SCHEDULE(vptr, NULL, "Opcode = %d", *((int *)(mcctptr->read_buf) + 1));
  NS_VPTR_SET_NVM_CONTEXT(mcctptr->vptr);
  switch(*((int *)(mcctptr->read_buf) + 1))
  {
    case NS_API_WEB_URL_REQ:
      mcctptr->vptr->next_pg_id = mcctptr->vptr->thdd_ptr->page_id;
      NSDL3_SCHEDULE(NULL, NULL, "mcctptr->vptr->next_pg_id = %d", mcctptr->vptr->next_pg_id);
      if(!runprof_table_shr_mem[mcctptr->vptr->group_num].gset.rbu_gset.enable_rbu)
        vut_add_task(mcctptr->vptr, VUT_WEB_URL);
      else
        vut_add_task(mcctptr->vptr, VUT_RBU_WEB_URL);
      break;

    case NS_API_RBU_WEB_URL_END_REQ:
      mcctptr->vptr->next_pg_id = ((Ns_web_url_req *) (mcctptr->read_buf))->page_id;
      NSDL3_SCHEDULE(NULL, NULL, "mcctptr->vptr->next_pg_id = %d", mcctptr->vptr->next_pg_id);
      vut_add_task(mcctptr->vptr, VUT_RBU_WEB_URL_END);
      break;

    case NS_API_PAGE_TT_REQ:
      page_think_time = mcctptr->vptr->thdd_ptr->page_think_time;
      ns_page_think_time_ext(mcctptr->vptr, page_think_time);
      //ret = ns_page_think_time_ext(mcctptr->vptr, page_think_time);
      /*if((ret < 0) || (mcctptr->vptr->pg_think_time <= 0))
      {
        fprintf(stdout,"Returning as page_think_time is 0 (page_think_time = %d)", page_think_time);
        return 0;
      }
      vut_add_task(mcctptr->vptr, VUT_PAGE_THINK_TIME);*/
      break;

    case NS_API_START_TX_REQ:
      //tx_name = mcctptr->vptr->thdd_ptr->tx_name;
      THREADMODE_SET_TX_NAME(tx_name_var, tx_name, mcctptr->vptr->thdd_ptr->tx_name);
      int sp_chk_flag = *((int *)(mcctptr->read_buf) + 2);
      int ret;
      if(sp_chk_flag && global_settings->sp_enable)
      {
        NSDL3_SCHEDULE(NULL, NULL, "SyncPoint is enabled");
        ret = ns_trans_chk_for_sp(tx_name, mcctptr->vptr);
        if(ret == 0) //If return 0 then this user was in sync point
          break;
      }

      send_msg_nvm_to_vutd(mcctptr->vptr, NS_API_START_TX_REP, tx_start_with_name (tx_name, mcctptr->vptr)); 
      break;
      
    case NS_API_END_TX_REQ:
      //tx_name = mcctptr->vptr->thdd_ptr->tx_name;
      status = mcctptr->vptr->thdd_ptr->status;
      THREADMODE_SET_TX_NAME(tx_name_var, tx_name, mcctptr->vptr->thdd_ptr->tx_name);
      send_msg_nvm_to_vutd(mcctptr->vptr, NS_API_END_TX_REP, tx_end (tx_name, status, mcctptr->vptr)); 
      break;

    case NS_API_END_TX_AS_REQ:
      //tx_name = mcctptr->vptr->thdd_ptr->tx_name;
      THREADMODE_SET_TX_NAME(tx_name_var, tx_name, mcctptr->vptr->thdd_ptr->tx_name);
      status = mcctptr->vptr->thdd_ptr->status;
      //end_tx_name = mcctptr->vptr->thdd_ptr->end_as_tx_name;
      THREADMODE_SET_TX_NAME(end_tx_name_var, end_tx_name, mcctptr->vptr->thdd_ptr->end_as_tx_name);
      send_msg_nvm_to_vutd(mcctptr->vptr, NS_API_END_TX_AS_REP, tx_end_as (tx_name, status, end_tx_name, mcctptr->vptr)); 
      break;

    case NS_API_GET_TX_TIME_REQ:
      //tx_name = mcctptr->vptr->thdd_ptr->tx_name;
      THREADMODE_SET_TX_NAME(tx_name_var, tx_name, mcctptr->vptr->thdd_ptr->tx_name);
      send_msg_nvm_to_vutd(mcctptr->vptr, NS_API_GET_TX_TIME_REP, tx_get_time (tx_name, mcctptr->vptr)); 
      break;

    case NS_API_SET_TX_STATUS_REQ:
      //tx_name = mcctptr->vptr->thdd_ptr->tx_name;
      status = mcctptr->vptr->thdd_ptr->status;
      THREADMODE_SET_TX_NAME(tx_name_var, tx_name, mcctptr->vptr->thdd_ptr->tx_name);
      send_msg_nvm_to_vutd(mcctptr->vptr, NS_API_SET_TX_STATUS_REP, tx_set_status_by_name (tx_name, status, mcctptr->vptr)); 
      break;

    case NS_API_GET_TX_STATUS_REQ:
      //tx_name = mcctptr->vptr->thdd_ptr->tx_name;
      THREADMODE_SET_TX_NAME(tx_name_var, tx_name, mcctptr->vptr->thdd_ptr->tx_name);
      send_msg_nvm_to_vutd(mcctptr->vptr, NS_API_GET_TX_STATUS_REP, tx_get_status (tx_name, mcctptr->vptr)); 
      break;

    case NS_API_END_SESSION_REQ:
      vut_add_task(mcctptr->vptr, VUT_END_SESSION);
      break;

    case NS_API_SYNC_POINT_REQ:
      sp_name = mcctptr->vptr->thdd_ptr->tx_name;
      ret_val = ns_sync_point_ext(sp_name, mcctptr->vptr);
      if(ret_val == 0) //If return 0 then this user was in sync point
        break;

      send_msg_nvm_to_vutd(mcctptr->vptr, NS_API_SYNC_POINT_REP, 0);
      break;

    case NS_API_CLICK_ACTION_REQ:
      mcctptr->vptr->next_pg_id = ((Ns_click_action_req *) (mcctptr->read_buf))->page_id;
      mcctptr->vptr->httpData->clickaction_id = ((Ns_click_action_req *) (mcctptr->read_buf))->click_action_id;
      NSDL3_SCHEDULE(NULL, NULL, "mcctptr->vptr->next_pg_id = %d, mcctptr->vptr->httpData->clickaction_id = %d", mcctptr->vptr->next_pg_id,
                                  mcctptr->vptr->httpData->clickaction_id);
      vut_add_task(mcctptr->vptr, VUT_CLICK_ACTION);
      break;
    
    case VUTD_STOP_THREAD_EXIT:
      vutd_stop_thread(NULL, mcctptr, VUTD_STOP_THREAD_EXIT);
      send_msg_nvm_to_vutd(mcctptr->vptr, VUTD_STOP_THREAD_EXIT, 0);
      break;
    
    case NS_API_ADVANCE_PARAM_REQ:
      sp_name = mcctptr->vptr->thdd_ptr->tx_name;
      ret_val = ns_advance_param_internal(sp_name, mcctptr->vptr);
      
      if (ret_val != -2)
        /* In case UseOnceAbort , no need to send advance reply
           This is done to avoid the situation when two messages are sent to thread 
           i.e. NS_API_ADVANCE_PARAM_REP and END session request from ns_advance_param_internal */
        send_msg_nvm_to_vutd(mcctptr->vptr, NS_API_ADVANCE_PARAM_REP, ret_val);
      break;   
    
    case NS_API_USER_DATA_POINT_REQ:
      rptGroupID = mcctptr->vptr->thdd_ptr->tx_name_size;
      rptGraphID = mcctptr->vptr->thdd_ptr->end_as_tx_name_size;
      value = mcctptr->vptr->thdd_ptr->page_think_time;
      send_msg_nvm_to_vutd(mcctptr->vptr, NS_API_USER_DATA_POINT_REP, add_user_data_point(rptGroupID, rptGraphID, value));
      break;

   case NS_API_WEBSOCKET_SEND_REQ:
      mcctptr->vptr->ws_send_id = mcctptr->vptr->thdd_ptr->page_id;
      NSDL3_SCHEDULE(NULL, NULL, "mcctptr->vptr->ws_send_id = %d", mcctptr->vptr->ws_send_id);
      vut_add_task(mcctptr->vptr, VUT_WS_SEND);
      break;

    case NS_API_WEBSOCKET_CLOSE_REQ:
      mcctptr->vptr->ws_close_id = mcctptr->vptr->thdd_ptr->page_id;
      NSDL3_SCHEDULE(NULL, NULL, "mcctptr->vptr->next_pg_id = %d", mcctptr->vptr->next_pg_id);
      vut_add_task(mcctptr->vptr, VUT_WS_CLOSE);
      break;

    case NS_API_WEBSOCKET_READ_REQ:
      conn_id = mcctptr->vptr->thdd_ptr->page_id;
      time_out = mcctptr->vptr->thdd_ptr->tx_name_size;
      nsi_web_websocket_read(mcctptr->vptr, conn_id, time_out);
      break;
    case NS_API_SOCKJS_CLOSE_REQ:
      ret_val = nsi_sockjs_close(mcctptr->vptr);
      if(ret_val == -1)
      {
        NSDL3_SCHEDULE(NULL, NULL, "sockJs Close request failed for close_id %d",mcctptr->vptr->sockjs_close_id);
        mcctptr->vptr->sockjs_status = NS_REQUEST_ERRMISC;
      }
      send_msg_nvm_to_vutd(mcctptr->vptr, NS_API_SOCKJS_CLOSE_REP, ret_val);
      break;
    case NS_API_SOCKET_SEND_REQ:
      mcctptr->vptr->next_pg_id = mcctptr->vptr->thdd_ptr->page_id;
      NSDL3_SCHEDULE(NULL, NULL, "mcctptr->vptr->ws_send_id = %d", mcctptr->vptr->ws_send_id);
      vut_add_task(mcctptr->vptr, VUT_SOCKET_SEND);
      break;
    case NS_API_SOCKET_READ_REQ:
      mcctptr->vptr->next_pg_id = mcctptr->vptr->thdd_ptr->page_id;
      vut_add_task(mcctptr->vptr, VUT_SOCKET_READ);
      //nsi_socket_recv(vptr);
      break;
    case NS_API_SOCKET_CLOSE_REQ:
      mcctptr->vptr->next_pg_id = mcctptr->vptr->thdd_ptr->page_id;
      NSDL3_SCHEDULE(NULL, NULL, "mcctptr->vptr->next_pg_id = %d", mcctptr->vptr->next_pg_id);
      vut_add_task(mcctptr->vptr, VUT_SOCKET_CLOSE);
      break;
    case NS_API_CONN_TIMEOUT_REQ:
      g_socket_vars.socket_settings.conn_to = mcctptr->vptr->thdd_ptr->page_id;
      NSDL3_SCHEDULE(NULL, NULL, "mcctptr->vptr->thdd_ptr->page_id = %d, conn_to = %d",
                     mcctptr->vptr->thdd_ptr->page_id, g_socket_vars.socket_settings.conn_to);
      send_msg_nvm_to_vutd(mcctptr->vptr, NS_API_CONN_TIMEOUT_REP, 0);
      break;
    case NS_API_SEND_TIMEOUT_REQ:
      g_socket_vars.socket_settings.send_to = mcctptr->vptr->thdd_ptr->page_id;
      NSDL3_SCHEDULE(NULL, NULL, "mcctptr->vptr->thdd_ptr->page_id = %d, send_to = %d",
                     mcctptr->vptr->thdd_ptr->page_id, g_socket_vars.socket_settings.send_to);
      send_msg_nvm_to_vutd(mcctptr->vptr, NS_API_SEND_TIMEOUT_REP, 0);
      break;
    case NS_API_SEND_IA_TIMEOUT_REQ:
      g_socket_vars.socket_settings.send_ia_to = mcctptr->vptr->thdd_ptr->page_id;
      NSDL3_SCHEDULE(NULL, NULL, "mcctptr->vptr->thdd_ptr->page_id = %d, send_ia_to = %d",
                     mcctptr->vptr->thdd_ptr->page_id, g_socket_vars.socket_settings.send_ia_to);
      send_msg_nvm_to_vutd(mcctptr->vptr, NS_API_SEND_IA_TIMEOUT_REP, 0);
      break;
    case NS_API_RECV_TIMEOUT_REQ:
      g_socket_vars.socket_settings.recv_to = mcctptr->vptr->thdd_ptr->page_id;
      NSDL3_SCHEDULE(NULL, NULL, "mcctptr->vptr->thdd_ptr->page_id = %d, recv_to = %d",
                     mcctptr->vptr->thdd_ptr->page_id, g_socket_vars.socket_settings.recv_to);
      send_msg_nvm_to_vutd(mcctptr->vptr, NS_API_RECV_TIMEOUT_REP, 0);
      break;
    case NS_API_RECV_IA_TIMEOUT_REQ:
      g_socket_vars.socket_settings.recv_ia_to = mcctptr->vptr->thdd_ptr->page_id;
      NSDL3_SCHEDULE(NULL, NULL, "mcctptr->vptr->thdd_ptr->page_id = %d, recv_ia_to = %d",
                     mcctptr->vptr->thdd_ptr->page_id, g_socket_vars.socket_settings.recv_ia_to);
      send_msg_nvm_to_vutd(mcctptr->vptr, NS_API_RECV_IA_TIMEOUT_REP, 0);
      break;
    case NS_API_RECV_FB_TIMEOUT_REQ:
      g_socket_vars.socket_settings.recv_fb_to = mcctptr->vptr->thdd_ptr->page_id;
      NSDL3_SCHEDULE(NULL, NULL, "mcctptr->vptr->thdd_ptr->page_id = %d, recv_ia_to = %d",
                     mcctptr->vptr->thdd_ptr->page_id, g_socket_vars.socket_settings.recv_fb_to);
      send_msg_nvm_to_vutd(mcctptr->vptr, NS_API_RECV_FB_TIMEOUT_REP, 0);
      break;
    case NS_API_XMPP_SEND_REQ:
      mcctptr->vptr->next_pg_id = mcctptr->vptr->thdd_ptr->page_id;
      NSDL3_SCHEDULE(NULL, NULL, "XMPP SEND REQ:: mcctptr->vptr->next_pg_id = %d", mcctptr->vptr->next_pg_id);
      vut_add_task(mcctptr->vptr, VUT_XMPP_SEND);
      break;
    case NS_API_XMPP_LOGOUT_REQ:
      NSDL3_SCHEDULE(NULL, NULL, "Received XMPP LOGOUT Req from Thread");
      vut_add_task(mcctptr->vptr, VUT_XMPP_LOGOUT);
      break;

  }
  return 0; 
}


int vutd_create_listen_fd(Msg_com_con *mccptr, int con_type, unsigned short *listen_port)
{
  char *ip_addr = NULL;
  int port = 0;
  char err_msg[1024];
  char server_ip[30];
  int fd;

  NSDL3_SCHEDULE(NULL, NULL, "Method Called");

  fd = nslb_Tcp_listen_ex(port, 1000, ip_addr, err_msg);
  NSDL3_SCHEDULE(NULL, NULL, "NVM Listner fd = %d", fd);
  if(fd == -1)
  {
    fprintf(stderr, "Error:Enable to create Listener\n");
    return -1;
  }
  memset(mccptr, 0, sizeof(Msg_com_con));
  mccptr->con_type = con_type;
  strcpy(server_ip, nslb_get_src_addr(fd));
  NSDL3_SCHEDULE(NULL, NULL, "NVM Listner sever ip and port = %s", server_ip);
  
  MY_MALLOC(mccptr->ip, strlen(server_ip), "g_nvm_listen_msg_con", -1);
  strcpy(mccptr->ip, server_ip);

  char *tmp;
  tmp = strrchr(server_ip, '.');
  tmp++;
  *listen_port = atoi(tmp);
  NSDL3_SCHEDULE(NULL, NULL, "tmp = %s, g_nvm_listen_port = %d", tmp, g_nvm_listen_port);
  mccptr->fd = fd;
  NSDL3_SCHEDULE(NULL, NULL, "mccptr->fd = %d,  fd = %d", mccptr->fd , fd);
  NSDL3_SCHEDULE(NULL, NULL, "Method Exiting, mccptr->con_type = %d", mccptr->con_type);
  return fd;
}

int vutd_accept_connetion(int fd)
{
//  Accept
  int accept_fd  = -1; 
  Msg_com_con *mcctptr;
  NSDL3_SCHEDULE(NULL, NULL, "Method Called, NVM listen fd = %d", fd);
  if((accept_fd = accept(fd, NULL, 0)) == -1)
  {
    fprintf(stderr,"Error in accept");
    //exit -1;
  }
  //int bytes_read;
  Msg_com_con tmp_mcctptr;
  memset(&tmp_mcctptr, 0, sizeof(Msg_com_con));
  MY_MALLOC(tmp_mcctptr.read_buf, 512, "Read First msg from thread", -1);
  tmp_mcctptr.fd = accept_fd; 
  NSDL3_SCHEDULE(NULL, NULL, "accept_fd = %d, tmp_mcctptr.read_buf = %p ", accept_fd, tmp_mcctptr.read_buf);
  if(vutd_read_msg(tmp_mcctptr.fd, tmp_mcctptr.read_buf) < 0)
  {
     NSDL3_SCHEDULE(NULL, NULL, "NVM: Error in reading INFO_REQ msg from thread on fd = %d", tmp_mcctptr.fd);
     NSTL1(NULL, NULL, "NVM: Error in reading INFO_REQ msg from thread on fd = %d\n", tmp_mcctptr.fd);
     free(tmp_mcctptr.read_buf);
     return -1;
  }
  tmp_mcctptr.fd = -1;
  NSDL3_SCHEDULE(NULL, NULL, "Opcode = %d", *((int *) (tmp_mcctptr.read_buf) + 1));
  
   NSDL3_SCHEDULE(NULL, NULL, "NVM: Opcode received for INFO_REQ = %d", *((int *) (tmp_mcctptr.read_buf) + 1));
   NSTL2(NULL, NULL, "NVM: Opcode received for INFO_REQ = %d", *((int *) (tmp_mcctptr.read_buf) + 1));
    if(*((int *) (tmp_mcctptr.read_buf) + 1) == VUTD_INFO_REQ)
    {
      mcctptr = (Msg_com_con *) (((Vutd_info*) (tmp_mcctptr.read_buf))->thread_info);
 
      mcctptr->con_type = NS_STRUCT_TYPE_VUSER_THREAD_COM;
      mcctptr->nvm_thread_fd = accept_fd;
      //mcctptr->vptr->mcctptr = mcctptr;
      NSDL3_SCHEDULE(NULL, NULL, "mcctptr = %p, mcctptr->con_type = %d", mcctptr, mcctptr->con_type);
      add_select_msg_com_con((char*)mcctptr, accept_fd, EPOLLIN);

      Vutd_info vutd_info_rep;
      vutd_info_rep.opcode = VUTD_INFO_REP;
      vutd_info_rep.thread_info = NULL;
      //vutd_info_rep.msg_len = sizeof(Vutd_info) - sizeof(int);
      if(vutd_write_msg(mcctptr->nvm_thread_fd, (char*)(&vutd_info_rep), sizeof(Vutd_info)) < 0)
      {
        NSDL3_SCHEDULE(NULL, NULL, "Error in replying: %s to thread from fd = %d\n", (char*)(&vutd_info_rep), mcctptr->nvm_thread_fd);
        NSTL1(NULL, NULL, "Error in replying: %s to thread from fd = %d\n", (char*)(&vutd_info_rep), mcctptr->nvm_thread_fd);  
        free(tmp_mcctptr.read_buf);
        vutd_stop_thread(mcctptr->vptr, mcctptr, VUTD_STOP_THREAD_EXIT);   //call from nvm
      }
      free(tmp_mcctptr.read_buf);
      return accept_fd;
    }
  
  free(tmp_mcctptr.read_buf);
  close(accept_fd);
  return -1;
}

int vutd_create_thread_reuse_connection()
{
  NSDL3_SCHEDULE(NULL, NULL, "Mehod Calles, ");
  int i;
  Msg_com_con *g_nvm_thread_msg_com = NULL;
 
  int thread_err;
  pthread_attr_t attr;
  size_t stacksize;
  int ret;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
  pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
  pthread_attr_getstacksize (&attr, &stacksize);
 // fprintf(stderr, "Current Pthread stack size = %ld\n", stacksize);
  if(stacksize != (global_settings->stack_size * 1024))
  {
    ret = pthread_attr_setstacksize(&attr, global_settings->stack_size * 1024);
    if(ret !=0 )
    {
      fprintf(stderr, "Error in change stack size error no = %d\n", ret);
      //perror("Error in change stack size: ");
    }
    pthread_attr_getstacksize (&attr, &stacksize);
    //fprintf(stderr, "After change Pthread stack size = %ld\n", stacksize);
  }

  for(i = 0; i < num_ceased_thread; i++)
  {
    g_nvm_thread_msg_com = ceased_thread_info; 
    ceased_thread_info = ceased_thread_info->next;
    if(sem_init(&(g_nvm_thread_msg_com->run_thread), 0, 0) == -1)
    {
      perror("sem_init");
      return 0;
    }
    if((thread_err = pthread_create(&(g_nvm_thread_msg_com->thread_id), &attr, vutd_worker_thread, (g_nvm_thread_msg_com))) != 0)
    {
      char err[2048];
      sprintf(err, "pthread_create:%s. ", nslb_strerror(errno));
      if(thread_err == EAGAIN)
        strcat(err, "Insufficient resources to create another thread, or a system-imposed limit on the number of threads was encountered. The latter case may occur in two ways: the RLIMIT_NPROC soft resource limit (set via setrlimit(2)), which limits the  number  of  process  for  a  real  user ID, was reached; or the kernel's system-wide limit on the number of threads, /proc/sys/kernel/threads-max, was reached.");
      else if(thread_err == EINVAL)
        strcat(err, "Invalid settings in attr.");
      else if(thread_err == EPERM)
        strcat(err, "No permission to set the scheduling policy and parameters specified in attr.");
      else
        strcat(err, "Error in creating new thread, thread_err = %d"); 
      NS_EL_2_ATTR(EID_VUSER_THREAD, -1,
                                -1,
                                EVENT_CORE, EVENT_CRITICAL,
                                (char *)__FILE__, (char *)__FUNCTION__,
                                "%s", err);
      return i;
    }
    
    NSDL3_SCHEDULE(NULL, NULL, "thread created with id = %lu, g_nvm_thread_msg_com[%d] = %p", 
                          g_nvm_thread_msg_com[i].thread_id, i, &g_nvm_thread_msg_com[i]);
    total_free_thread++;
    if(!free_thread_pool){
      free_thread_pool = g_nvm_thread_msg_com;
      free_thread_pool_tail = g_nvm_thread_msg_com;
      g_nvm_thread_msg_com->next = NULL;
    }
    else {
      free_thread_pool_tail->next = g_nvm_thread_msg_com;
      free_thread_pool_tail = g_nvm_thread_msg_com;
      free_thread_pool_tail->next = NULL;
    }
  }
  NSTL1(NULL, NULL, "reuse free pool, num_ceased_thread = %d, free_thread_pool = %p, thread = [free = %d, busy = %d]", num_ceased_thread, free_thread_pool, total_free_thread, total_busy_thread);
  ceased_thread_info = NULL;
  num_ceased_thread = 0;
  return i;
}

int vutd_create_thread(int num_thread)
{
   NSDL3_SCHEDULE(NULL, NULL, "Mehod Calles, ");
   int i;
   Msg_com_con *g_nvm_thread_msg_com = NULL;
 
  if(!g_nvm_thread_msg_com)
  {
     MY_MALLOC(g_nvm_thread_msg_com, sizeof(Msg_com_con) * num_thread, "NS_STRUCT_TYPE_VUSER_THREAD_COM", -1);
     if(!g_nvm_thread_msg_com){
       NSDL3_SCHEDULE(NULL, NULL, "Error: in create_table_entry of vuser" );
       return 0;
     }
  }

  memset(g_nvm_thread_msg_com , 0, sizeof(Msg_com_con) * num_thread);

  int thread_err;
  pthread_attr_t attr;
  size_t stacksize;
  int ret;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
  pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
  pthread_attr_getstacksize (&attr, &stacksize);
  NSDL3_SCHEDULE(NULL, NULL, "Current Pthread stack size = %ld\n", stacksize);
  if(stacksize != (global_settings->stack_size * 1024))
  {
    ret = pthread_attr_setstacksize(&attr, global_settings->stack_size * 1024);
    if(ret !=0 )
    {
      fprintf(stderr, "Error in change stack size error no = %d\n", ret);
      //perror("Error in change stack size: ");
    }
    pthread_attr_getstacksize (&attr, &stacksize);
    NSDL3_SCHEDULE(NULL, NULL, "After change Pthread stack size = %ld\n", stacksize);
  }

  for(i = 0; i < num_thread; i++)
  {
    /* Malloc the memory for NVM and thread communication 
     * We are not free this memory becouse when thread is assing to another user 
     * user cn use this memory.
     * We are alloc the mamory size 512 becouse 
     * Any struct have not more then this */
    MY_MALLOC(g_nvm_thread_msg_com[i].read_buf, 512, "Fos msg com NVM and thread", -1);
     g_nvm_thread_msg_com[i].thread_nvm_fd = -1; 
     g_nvm_thread_msg_com[i].nvm_thread_fd = -1;
    if(sem_init(&(g_nvm_thread_msg_com[i].run_thread), 0, 0) == -1)
    {
      perror("sem_init");
      NS_EXIT(-1, "Failed to create semaphore");
    }
    if((thread_err = pthread_create(&(g_nvm_thread_msg_com[i].thread_id), &attr, vutd_worker_thread, &(g_nvm_thread_msg_com[i]))) != 0)
    {
      char err[2048];
      sprintf(err, "pthread_create:%s. ", nslb_strerror(errno));
      if(thread_err == EAGAIN)
        strcat(err, "Insufficient resources to create another thread, or a system-imposed limit on the number of threads was encountered. The latter case may occur in two ways: the RLIMIT_NPROC soft resource limit (set via setrlimit(2)), which limits the  number  of  process  for  a  real  user ID, was reached; or the kernel's system-wide limit on the number of threads, /proc/sys/kernel/threads-max, was reached.");
      else if(thread_err == EINVAL)
        strcat(err, "Invalid settings in attr.");
      else if(thread_err == EPERM)
        strcat(err, "No permission to set the scheduling policy and parameters specified in attr.");
      else
        strcat(err, "Error in creating new thread, thread_err = %d"); 
      NS_EL_2_ATTR(EID_VUSER_THREAD, -1,
                                -1,
                                EVENT_CORE, EVENT_CRITICAL,
                                (char *)__FILE__, (char *)__FUNCTION__,
                                "%s", err);
      return i;
    }
    
    NSDL3_SCHEDULE(NULL, NULL, "thread created with id = %lu, g_nvm_thread_msg_com[%d] = %p", 
                          g_nvm_thread_msg_com[i].thread_id, i, &g_nvm_thread_msg_com[i]);

    total_free_thread++;
    if(!free_thread_pool){
      free_thread_pool = &(g_nvm_thread_msg_com[i]);
      free_thread_pool_tail = &(g_nvm_thread_msg_com[i]);
      free_thread_pool_tail->next = NULL; 
    }
    else {
      free_thread_pool_tail->next = &(g_nvm_thread_msg_com[i]);
      free_thread_pool_tail = &(g_nvm_thread_msg_com[i]);
      free_thread_pool_tail->next = NULL;
    }
    NSTL1(NULL, NULL, "Thread created, free_tread_pool = %p, thread = [free = %d, busy = %d]", 
                       free_thread_pool, total_free_thread, total_busy_thread);
  }
  return i;
}


int vutd_stop_thread(VUser *tmp_vptr, Msg_com_con *mcctptr, int status)
{
  NSDL3_SCHEDULE(NULL, NULL, "Method Called, tmp_vptr = %p, status = %d", tmp_vptr, status);
  Msg_com_con *tmp_nvm_info_tmp = NULL;

  if(status == VUTD_STOP_THREAD_EXIT)
  {
    tmp_nvm_info_tmp = mcctptr; 
    // We should decrease total and busy and increase ceased count using LOCKING
    Msg_com_con *tmp_ceased_thread = ceased_thread_info;
    ceased_thread_info = tmp_nvm_info_tmp;
    sem_destroy(&(ceased_thread_info->run_thread));
    ceased_thread_info->next = tmp_ceased_thread;
    num_ceased_thread++;
    NSDL3_SCHEDULE(tmp_vptr, NULL, "Stopping thread and moving it to ceased thread list.", mcctptr->nvm_thread_fd);
    NSTL2(tmp_vptr, NULL, "Stopping thread and moving it to ceased thread list.", mcctptr->nvm_thread_fd);
  }
  else
  {
    NSDL3_SCHEDULE(NULL, NULL, "Add ens session in vuser task");
    free_thread(mcctptr);
  }
  return 0;
}


Msg_com_con *get_thread()
{

  NSDL2_SCHEDULE(NULL, NULL, "Method Called,free_tread_pool = %p, thread = [free = %d, busy = %d], ceased_thread_info = %p", free_thread_pool, 
                              total_free_thread, total_busy_thread, ceased_thread_info);

  NSTL3(NULL, NULL, "Method Called,free_tread_pool = %p, thread = [free = %d, busy = %d], ceased_thread_info = %p", free_thread_pool, total_free_thread, total_busy_thread, ceased_thread_info);
  //fprintf(stderr, "\n%s|%d|get_thread total_busy_thread = %d\n", __FILE__, __LINE__, total_busy_thread);
  if(free_thread_pool)
  {
    Msg_com_con *tmp_pool_ptr;
    tmp_pool_ptr = free_thread_pool;
    free_thread_pool = free_thread_pool->next;
    tmp_pool_ptr->next = NULL;
 
    total_free_thread--;
    total_busy_thread++;
    NSDL2_SCHEDULE(NULL, NULL, "Getting thread from free thread pool. Thread_Info = [free = %d, busy = %d], free_thread_pool = %p", 
                                total_free_thread, total_busy_thread, free_thread_pool);
    NSTL3(NULL, NULL, "free thread pool. free_thread_pool = %p, Thread_Info = [free = %d, busy = %d], ceased_thread_pool = %p", free_thread_pool, total_free_thread, total_busy_thread, ceased_thread_info);
    return tmp_pool_ptr;
  }
  //fprintf(stderr, "\n\n\n%s|%d|ceased_thread_info = %p, num_ceased_thread = %d\n\n\n", __FILE__, __LINE__, ceased_thread_info, num_ceased_thread);
  if(ceased_thread_info)
  {
    /* If any error in restarting the thread vutd_create_thread_reuse_connection return 0
     * So return NULL otherwise get thread from thread pool and return */
    NSTL1(NULL, NULL, "Free is NULL and some threads exit due to some error case. ceased_thread_info = %p", ceased_thread_info);
    if(vutd_create_thread_reuse_connection() == 0)
      return NULL;
    NSTL1(NULL, NULL, "Ceased thread pool ceased_thread_info = %p, free_tread_pool = %p, thread = [free = %d, busy = %d]", ceased_thread_info, free_thread_pool, total_free_thread, total_busy_thread);
    return(get_thread());
  }
  int num_thread = 0;
    
  if (global_settings->incremental_thread < global_settings->num_process)
  {
    global_settings->incremental_thread = global_settings->num_process;
    NSTL1(NULL, NULL, "Incremantal threads are less than number of NVM's. So, incremental_thread is set equal to NVMs. incremental_thread= %d, num_process = %d, num_thread = %d", global_settings->incremental_thread, global_settings->num_process, num_thread);
  }
  num_thread = global_settings->incremental_thread / global_settings->num_process;
  if(global_settings->max_thread < (global_settings->init_thread + global_settings->incremental_thread)) {
    global_settings->max_thread = global_settings->init_thread + global_settings->incremental_thread;
    NSTL1(NULL, NULL, "Max thread is less then sum of init and incremantal thread so setting it to init+incremental."
                       "global_settings->max_thread = %d", global_settings->max_thread);
  }
  if((num_thread + total_busy_thread) > (global_settings->max_thread / global_settings->num_process))
    num_thread = (global_settings->max_thread / global_settings->num_process) - total_busy_thread;
   
  NSTL1(NULL, NULL, "Incrementing thread num_thread = %d, free_tread_pool = %p, thread = [free = %d, busy = %d]", num_thread, free_thread_pool, total_free_thread, total_busy_thread);
  if(num_thread != 0)
  {
    /* If any error in starting the thread vutd_create_thread return 0
     * So return NULL otherwise get thread from thread pool and return */
    if(vutd_create_thread(num_thread) == 0)
      return NULL;
    return(get_thread());
  }

  NSDL3_SCHEDULE(NULL, NULL, "Error : No thread in Free Pool");
  return NULL;
}

void free_thread(Msg_com_con *to_fre_pool_ptr)
{
  NSDL3_SCHEDULE(NULL, NULL, "Method Called, to_fre_pool_ptr = %p", to_fre_pool_ptr);

  if(!free_thread_pool){
    free_thread_pool = to_fre_pool_ptr;
    free_thread_pool_tail = to_fre_pool_ptr;
    free_thread_pool_tail->next = NULL;
  }
  else {
    free_thread_pool_tail->next = to_fre_pool_ptr;
    free_thread_pool_tail = to_fre_pool_ptr;
    free_thread_pool_tail->next = NULL;
  }

  total_free_thread++;
  total_busy_thread--;
  NSTL3(NULL, NULL, "Method Called,free_tread_pool = %p, thread = [free = %d, busy = %d], to_fre_pool_ptr = %p", free_thread_pool, total_free_thread, total_busy_thread, to_fre_pool_ptr);
}

void send_msg_nvm_to_vutd(VUser *vptr, int type, int ret_val)
{
  Ns_api_rep tmp_ns_api_rep;

  NSDL3_SCHEDULE(NULL, NULL, "Method called, type = %d", type);

  switch(type)
  {
    case NS_API_WEB_URL_REP: // Same is used for RBU URL END
      tmp_ns_api_rep.opcode = NS_API_WEB_URL_REP;
      tmp_ns_api_rep.ret_val = vptr->page_status;
      break;

    case NS_API_PAGE_TT_REP:
      tmp_ns_api_rep.opcode = NS_API_PAGE_TT_REP;
      tmp_ns_api_rep.ret_val = ret_val; 
      break;

    case NS_API_END_SESSION_REP:
      tmp_ns_api_rep.opcode = NS_API_END_SESSION_REP;
      tmp_ns_api_rep.ret_val = ret_val; 
      break;

    case NS_API_START_TX_REP:
      tmp_ns_api_rep.opcode = NS_API_START_TX_REP;
      tmp_ns_api_rep.ret_val = ret_val; 
      break;

    case NS_API_END_TX_REP:
      tmp_ns_api_rep.opcode = NS_API_END_TX_REP;
      tmp_ns_api_rep.ret_val = ret_val; 
      break;

    case NS_API_END_TX_AS_REP:
      tmp_ns_api_rep.opcode = NS_API_END_TX_AS_REP;
      tmp_ns_api_rep.ret_val = ret_val; 
      break;

    case NS_API_GET_TX_TIME_REP:
      tmp_ns_api_rep.opcode = NS_API_GET_TX_TIME_REP;
      tmp_ns_api_rep.ret_val = ret_val; 
      break;

    case NS_API_SET_TX_STATUS_REP:
      tmp_ns_api_rep.opcode = NS_API_SET_TX_STATUS_REP;
      tmp_ns_api_rep.ret_val = ret_val; 
      break;

    case NS_API_GET_TX_STATUS_REP:
      tmp_ns_api_rep.opcode = NS_API_GET_TX_STATUS_REP;
      tmp_ns_api_rep.ret_val = ret_val; 
      break;

    case NS_API_END_SESSION_REQ:
      tmp_ns_api_rep.opcode = NS_API_END_SESSION_REQ;
      break;

    case NS_API_SYNC_POINT_REP:
      tmp_ns_api_rep.opcode = NS_API_SYNC_POINT_REP;
      tmp_ns_api_rep.ret_val = ret_val; 
      break;

    case VUTD_STOP_THREAD_EXIT:
      tmp_ns_api_rep.opcode = VUTD_STOP_THREAD_EXIT;
      break;

    case NS_API_ADVANCE_PARAM_REP:
      tmp_ns_api_rep.opcode = NS_API_ADVANCE_PARAM_REP;
      tmp_ns_api_rep.ret_val = ret_val;
      break;
    
    case NS_API_USER_DATA_POINT_REP:
      tmp_ns_api_rep.opcode = NS_API_USER_DATA_POINT_REP;
      tmp_ns_api_rep.ret_val = ret_val;
      break;
 
    case NS_API_WEBSOCKET_SEND_REP: 
      tmp_ns_api_rep.opcode = NS_API_WEBSOCKET_SEND_REP;
      tmp_ns_api_rep.ret_val = vptr->page_status;
      break;

    case NS_API_WEBSOCKET_CLOSE_REP: 
      tmp_ns_api_rep.opcode = NS_API_WEBSOCKET_CLOSE_REP;
      tmp_ns_api_rep.ret_val = vptr->page_status;
      break;

    case NS_API_WEBSOCKET_READ_REP: 
      tmp_ns_api_rep.opcode = NS_API_WEBSOCKET_READ_REP;
      tmp_ns_api_rep.ret_val = ret_val;
      break;
    case NS_API_SOCKJS_CLOSE_REP:
      tmp_ns_api_rep.opcode = NS_API_SOCKJS_CLOSE_REP;
      break;
    case NS_API_SOCKET_SEND_REP: 
      tmp_ns_api_rep.opcode = NS_API_SOCKET_SEND_REP;
      tmp_ns_api_rep.ret_val = vptr->page_status;
      break;
    case NS_API_SOCKET_READ_REP:
      tmp_ns_api_rep.opcode = NS_API_SOCKET_READ_REP;
      tmp_ns_api_rep.ret_val = ret_val;
      break;
    case NS_API_SOCKET_CLOSE_REP:
      tmp_ns_api_rep.opcode = NS_API_SOCKET_CLOSE_REP;
      tmp_ns_api_rep.ret_val = vptr->page_status;
      break;
    case NS_API_CONN_TIMEOUT_REP:
      tmp_ns_api_rep.opcode = NS_API_CONN_TIMEOUT_REP;
      break;
    case NS_API_SEND_TIMEOUT_REP:
      tmp_ns_api_rep.opcode = NS_API_SEND_TIMEOUT_REP;
      break;
    case NS_API_SEND_IA_TIMEOUT_REP:
      tmp_ns_api_rep.opcode = NS_API_SEND_IA_TIMEOUT_REP;
      break;
    case NS_API_RECV_TIMEOUT_REP:
      tmp_ns_api_rep.opcode = NS_API_RECV_TIMEOUT_REP;
      break;
    case NS_API_RECV_IA_TIMEOUT_REP:
      tmp_ns_api_rep.opcode = NS_API_RECV_IA_TIMEOUT_REP;
      break;
    case NS_API_RECV_FB_TIMEOUT_REP:
      tmp_ns_api_rep.opcode = NS_API_RECV_FB_TIMEOUT_REP;
      break;
    case NS_API_XMPP_SEND_REP:
      tmp_ns_api_rep.opcode = NS_API_XMPP_SEND_REP;
      tmp_ns_api_rep.ret_val = ret_val; 
      break;
  }
  NS_VPTR_SET_USER_CONTEXT(vptr);
  vptr->mcctptr->fd = vptr->mcctptr->nvm_thread_fd;   //not getting vptr : 
  if(type == NS_API_END_SESSION_REQ)
  {
     NSDL3_SCHEDULE(vptr, NULL, " Received NS_API_END_SESSION_REQ, nvm_thread_fd = %d, thread_nvm_fd = %d", vptr->mcctptr->nvm_thread_fd, vptr->mcctptr->thread_nvm_fd);
     NSTL3(vptr, NULL, "Received NS_API_END_SESSION_REQ, nvm_thread_fd = %d, thread_nvm_fd = %d", vptr->mcctptr->nvm_thread_fd, vptr->mcctptr->thread_nvm_fd); 
    if(vutd_write_msg(vptr->mcctptr->nvm_thread_fd, (char*)(&tmp_ns_api_rep), sizeof(Ns_api_rep)) < 0)
    {
      //as the value of sendmsg is not proper- i clsoe the socket and set the file handle to -1/invalid.
      fprintf(stderr, "Error in sending: %s, type = %d\n", (char*)(&tmp_ns_api_rep), type);
      vutd_stop_thread(vptr, vptr->mcctptr, VUTD_STOP_THREAD_EXIT);  // call from nvm
      NS_VPTR_SET_NVM_CONTEXT(vptr);
    }
  }
  else
  {
    //tmp_ns_api_rep.msg_len = sizeof(Ns_api_rep) - sizeof(int);
    if(vutd_write_msg(vptr->mcctptr->nvm_thread_fd, (char*)(&tmp_ns_api_rep), sizeof(Ns_api_rep)) < 0)
    {
      //as the value of sendmsg is not proper- i clsoe the socket and set the file handle to -1/invalid.
      fprintf(stderr, "Error in sending: %s, type = %d\n", (char*)(&tmp_ns_api_rep), type);
      vutd_stop_thread(vptr, vptr->mcctptr, VUTD_STOP_THREAD_EXIT);  //call from nvm
      NS_VPTR_SET_NVM_CONTEXT(vptr);
    }
  }
  NSDL3_SCHEDULE(NULL, NULL, "Method Exiting");
}

int vutd_read_msg(int fd, char *read_buf)
{
  int bytes_read;  // Bytes read in one read call
  //int size_tmp;

  if (fd == -1) {
    NSDL1_MESSAGES(NULL, NULL, "fd is -1 for .. returning.");
    return -1;  // Issue - this is misleading as it means read is not complete
  }

  NSDL1_MESSAGES(NULL, NULL, "Method called, fd = %d", fd);
  int read_offset = 0;
  int read_bytes_remaining = -1;


  while (1) /* Reading first four byte which is the size of the message */
  {
    NSDL2_MESSAGES(NULL, NULL, "Reading size of the message. Size received so far = %d from fd %d", read_offset, fd);

    if ((bytes_read = read (fd, read_buf + read_offset, sizeof(int) - read_offset)) < 0)
    {
      if (errno == EAGAIN)
      {
        NSDL2_MESSAGES(NULL, NULL, "Error in reading size of the message due to EAGAIN, offset = %d, fd = %d, continuing to read again", read_offset, fd);
        NSTL2(NULL, NULL, "Error in reading size of the message due to EAGAIN, fd = %d, continuing to read again", fd);
        //return -1;//NS_EAGAIN_RECEIVED;
        continue;
      } else if (errno == EINTR) {   /* this means we were interrupted */
        NSDL2_MESSAGES(NULL, NULL, "Interrupted while reading size of the message. continuing");
        NSTL2(NULL, NULL, "Error in reading size of the message due to EINTR, fd = %d, continuing to read again", fd);
        continue;
      }
      else
      {
        NSTL1(NULL, NULL, "Error in reading size of the message, fd = %d, error = %s", fd, nslb_strerror(errno));
        //close(fd); This is commented as both NVM and thread has to handle their FD closure
        return -1; /* This is to handle closed connection from tools */
      }
    }
    NSDL2_MESSAGES(NULL, NULL, "Bytes read to get size of the message = %d, fd = %d", bytes_read, fd);
    if (bytes_read == 0) {
      NSDL2_MESSAGES(NULL, NULL, "Connection closed from other side. error = %s, fd = %d", nslb_strerror(errno), fd);
      NSTL2(NULL, NULL, "Error in reading size of the message due to connection closed, bytes_read = 0, fd = %d", fd);
      //close(fd); This is commented as both NVM and thread has to handle their FD closure
      return -1;
    }
    read_offset += bytes_read;
    if (read_offset == sizeof(int))
    {
      NSDL2_MESSAGES(NULL, NULL, "Message length = %d", ((parent_child *)(read_buf))->msg_len);
      read_bytes_remaining = ((parent_child *)(read_buf))->msg_len;
      break;
    }
  }

  NSDL2_MESSAGES(NULL, NULL, "Remaining bytes(opcode + message) to read = %d", read_bytes_remaining);
  while (read_bytes_remaining > 0) /* Reading rest of the message */
  {
    NSDL2_MESSAGES(NULL, NULL, "Reading rest of the message. offset = %d, bytes_remaining = %d",
              read_offset, read_bytes_remaining);

    if((bytes_read = read (fd, read_buf + read_offset, read_bytes_remaining)) < 0)
    {
      if(errno == EAGAIN)
      {
        NSDL2_MESSAGES(NULL, NULL, "Complete message is not available for read(EAGAIN). offset = %d, bytes_remaining = %d",
                  read_offset, read_bytes_remaining );
        NSTL2(NULL, NULL, "Error in reading remaining message, encountered EAGAIN on fd = %d", fd);
        return -1;// NS_EAGAIN_RECEIVED | NS_READING;
      }
      else
      {
        NSDL2_MESSAGES(NULL, NULL, "Error in reading msg due to error = %s, bytes read = %d\n", nslb_strerror(errno), bytes_read);
        NSTL2(NULL, NULL, "Error in reading msg due to error = %s, bytes read = %d", nslb_strerror(errno), bytes_read);
        return -1; /* This is to handle error other than EAGAIN */
      }
    }
    NSDL2_MESSAGES(NULL, NULL, "Bytes_read for getting the remaining message = %d, fd = %d", bytes_read, fd);
    if (bytes_read == 0) {
      NSDL2_MESSAGES(NULL, NULL, "Connection closed from other side while reading remaining message. Error = %s, fd = %d", nslb_strerror(errno), fd);
      NSTL2(NULL, NULL, "Error in reading remaining message due to connection closed, bytes_read = 0, fd = %d", fd);
      return -1;
    }
    read_offset += bytes_read;
    read_bytes_remaining -= bytes_read;
  }

  NSDL2_MESSAGES(NULL, NULL, "Complete message read. Total message size read = %d", read_offset);
  return (read_offset);
}


int vutd_write_msg(int fd, char *buf, int size) 
{
  int bytes_writen;
  char *msg_ptr;
  //int copy_was_done = 0;

  NSDL1_MESSAGES(NULL, NULL, "Method called. size to write = %d, fd = %d, buf = %p", size, fd, buf);
  if (fd == -1) {
    NSDL1_MESSAGES(NULL, NULL, "fd is -1 for .. returning.");
    return -1;  
  }
 
  //Shibani: fill msg_len in MSG_HRD
  ((parent_child *)buf)->msg_len = size - sizeof(int);  

  msg_ptr = buf;
  int write_bytes_remaining = size;
  int write_offset = 0;
  
  while (write_bytes_remaining)
  {

     NSDL2_MESSAGES(NULL, NULL, "Sending the message. Byte to send = %d", write_bytes_remaining);
     NSTL2(NULL, NULL, "Sending the message. Byte to send = %d", write_bytes_remaining);
    if ((bytes_writen = write (fd, msg_ptr + write_offset, write_bytes_remaining)) < 0)
    {
      perror("vutd_write_msg(): Write Error:");
      return -1;
    }
  
    if (bytes_writen == 0) {
      NSDL2_MESSAGES(NULL, NULL, "Write returned = 0 on fd = %d",fd);
      continue;
    }
    write_offset += bytes_writen;
    //msg_ptr += bytes_writen;
    write_bytes_remaining -= bytes_writen;
  }

  NSDL2_MESSAGES(NULL, NULL, "Method Exiting, write_offset = %d, fd = %d", write_offset, fd);
  return 0;
}

//VUSER_THREAD_POOL <init thread pool size> <incremental size> <max thread pool size> <size of stack in KB>
int kw_set_vuser_thread_pool(char *buf, int flag, char *err_msg)
{
  char keyword[MAX_DATA_LINE_LENGTH] = "\0";
  char init_thread[MAX_DATA_LINE_LENGTH] = "\0";
  char incremental_thread[MAX_DATA_LINE_LENGTH] = "\0";
  char max_thread[MAX_DATA_LINE_LENGTH] = "\0";
  char stack_size[MAX_DATA_LINE_LENGTH] = "\0";
  char *val;
  int num;
  NSDL3_PARSING(NULL, NULL, "Method Called, buf = %s", buf);
  if ((num = sscanf(buf, "%s %s %s %s %s", keyword, init_thread, incremental_thread, max_thread, stack_size)) != 5) {
    NS_KW_PARSING_ERR(buf, flag, err_msg, VUSER_THREAD_POOL_USAGE, CAV_ERR_1011072, CAV_ERR_MSG_1);
  }

  val = init_thread;
  CLEAR_WHITE_SPACE(val);
  if(val == NULL)
    NS_KW_PARSING_ERR(buf, flag, err_msg, VUSER_THREAD_POOL_USAGE, CAV_ERR_1011072, CAV_ERR_MSG_1);//TODO

  num = atoi(val);
  if(num == 0)
    NS_KW_PARSING_ERR(buf, flag, err_msg, VUSER_THREAD_POOL_USAGE, CAV_ERR_1011072, CAV_ERR_MSG_4);

  if(num < 0)
    NS_KW_PARSING_ERR(buf, flag, err_msg, VUSER_THREAD_POOL_USAGE, CAV_ERR_1011072, CAV_ERR_MSG_8);

  if(ns_is_numeric(val) == 0)
    NS_KW_PARSING_ERR(buf, flag, err_msg, VUSER_THREAD_POOL_USAGE, CAV_ERR_1011072, CAV_ERR_MSG_2);

  global_settings->init_thread = num;
  NSDL3_PARSING(NULL, NULL, "Setting global_settings->init_thread = %d", global_settings->init_thread);

  val = incremental_thread;
  CLEAR_WHITE_SPACE(val);

  if(val == NULL)
    NS_KW_PARSING_ERR(buf, flag, err_msg, VUSER_THREAD_POOL_USAGE, CAV_ERR_1011072, CAV_ERR_MSG_1);

  num = atoi(val);
  if(num == 0)
    NS_KW_PARSING_ERR(buf, flag, err_msg, VUSER_THREAD_POOL_USAGE, CAV_ERR_1011072, CAV_ERR_MSG_4);

  if(num < 0)
    NS_KW_PARSING_ERR(buf, flag, err_msg, VUSER_THREAD_POOL_USAGE, CAV_ERR_1011072, CAV_ERR_MSG_8);

  if(ns_is_numeric(val) == 0)
    NS_KW_PARSING_ERR(buf, flag, err_msg, VUSER_THREAD_POOL_USAGE, CAV_ERR_1011072, CAV_ERR_MSG_2);

  global_settings->incremental_thread = num;

  val = max_thread;
  CLEAR_WHITE_SPACE(val);
  if(val == NULL)
    NS_KW_PARSING_ERR(buf, flag, err_msg, VUSER_THREAD_POOL_USAGE, CAV_ERR_1011072, CAV_ERR_MSG_1);

  num = atoi(val);
  if(num == 0)
    NS_KW_PARSING_ERR(buf, flag, err_msg, VUSER_THREAD_POOL_USAGE, CAV_ERR_1011072, CAV_ERR_MSG_4);  

  if(num < 0)
    NS_KW_PARSING_ERR(buf, flag, err_msg, VUSER_THREAD_POOL_USAGE, CAV_ERR_1011072, CAV_ERR_MSG_8);  

  if(ns_is_numeric(val) == 0)
    NS_KW_PARSING_ERR(buf, flag, err_msg, VUSER_THREAD_POOL_USAGE, CAV_ERR_1011072, CAV_ERR_MSG_2);

  global_settings->max_thread = num;

  val = stack_size;
  CLEAR_WHITE_SPACE(val);
  if(val == NULL)
    NS_KW_PARSING_ERR(buf, flag, err_msg, VUSER_THREAD_POOL_USAGE, CAV_ERR_1011072, CAV_ERR_MSG_1);

  if(ns_is_numeric(val) == 0)
    NS_KW_PARSING_ERR(buf, flag, err_msg, VUSER_THREAD_POOL_USAGE, CAV_ERR_1011072, CAV_ERR_MSG_2);

  num = atoi(val);
  if(num < 32 || num > 8192)
    NS_KW_PARSING_ERR(buf, flag, err_msg, VUSER_THREAD_POOL_USAGE, CAV_ERR_1011073, "");

  global_settings->stack_size = num;

  if(global_settings->max_thread < (global_settings->init_thread + global_settings->incremental_thread)) {
    global_settings->max_thread = global_settings->init_thread + global_settings->incremental_thread;
    NSTL1(NULL, NULL, "Max thread is less then sum of init and incremantal thread so setting it to init+incremental. max_thread = %d", global_settings->max_thread);
    NS_DUMP_WARNING("Max thread is less then sum of init and incremantal thread so setting it to init+incremental. max_thread = %d", global_settings->max_thread);
  }
  NSDL3_PARSING(NULL, NULL, "Method Exiting, init_thread = %d, incremental_thread = %d, max_thread = %d, stack_size = %d"
                            , global_settings->init_thread, global_settings->incremental_thread
                            , global_settings->max_thread, global_settings->stack_size);

  return 0;
}
