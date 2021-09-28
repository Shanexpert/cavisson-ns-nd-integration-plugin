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
#include "ns_vuser_thread.h"
#include "ns_child_thread_util.h"
#include "ns_page_think_time.h"
#include "nslb_sock.h"
#include "ns_alloc.h"
#include <setjmp.h>

#define MAX_READ_LENGTH 16000

Msg_com_con *free_thread_pool = NULL;
Msg_com_con *ceased_thread_info = NULL;
Msg_com_con *free_thread_pool_tail = NULL;
int total_free_thread = 0;
int total_busy_thread = 0;
int num_ceased_thread = 0;
//Removal of thread local data
//__thread jmp_buf jmp_buffer;//used in set_jmp and long_jmp

static void vutd_do_free_or_exit(Msg_com_con *thread_info, int status)
{
  if(status == VUTD_STOP_THREAD_EXIT) {
    NSDL3_SCHEDULE(NULL, NULL, "Thread getting exit due to Read/Write Fail with NVM or Request of THREAD EXIT is received");
    NSTL1(NULL, NULL, "Thread getting exit due to Read/Write Fail with NVM OR Request of THREAD EXIT is received");
    close(thread_info->thread_nvm_fd);
    close(thread_info->nvm_thread_fd);
    thread_info->thread_nvm_fd = -1;
    thread_info->nvm_thread_fd = -1;
    pthread_exit(NULL);
  }
  else if(status == VUTD_STOP_THREAD_FREE) {
    NSDL3_SCHEDULE(NULL, NULL, "Thread jumps to sem_wait..");
    longjmp(g_tls.jmp_buffer, 1001);
  }
}

void *vutd_worker_thread(void *tmp_ptr)
{
  Msg_com_con *thread_info;
  char *ip = "127.0.0.1";
  int flags;
  Vutd_info thd_info;
  thread_info = (Msg_com_con *) tmp_ptr;
   
  // Initializing thread local storage
  ns_tls_init(VUSER_THREAD_LOCAL_BUFFER_SIZE);
  SET_CALLER_TYPE(IS_NVM_THREAD);
  set_thread_specific_data(tmp_ptr);

  NSDL3_SCHEDULE(NULL, NULL, "Method called, thread_info = %p", thread_info);
  void (*runlogic_func_ptr)(void) = NULL;
  if(thread_info->thread_nvm_fd == -1)  
  {
    int nvm_fd = -1;
    thd_info.thread_info = thread_info;

    NSDL3_SCHEDULE(NULL, NULL, "ip = %s, port = %d", ip, g_nvm_listen_port);
    if((nvm_fd = nslb_tcp_client_r(ip, g_nvm_listen_port)) == -1)
    {
      NSDL3_SCHEDULE(NULL, NULL, "Error in connecting with NVM, ip = %s, port = %d\n", ip, g_nvm_listen_port);
      fprintf(stderr, "Error in connecting with NVM, ip = %s, port = %d\n", ip, g_nvm_listen_port);
      ns_tls_free();
      vutd_do_free_or_exit(thread_info, VUTD_STOP_THREAD_EXIT);
    }
    thread_info->thread_nvm_fd = nvm_fd;
    flags = fcntl(nvm_fd, F_GETFL , 0);
    if(flags == -1)
    {
      NSDL3_SCHEDULE(NULL, NULL, "Error in fd BLOCKING so exiting thread");
      thd_info.opcode = VUTD_STOP_THREAD_EXIT;
      vutd_send_msg_to_nvm(0, (char *) (&thd_info), sizeof(Vutd_info));
    } 
 
    flags &= ~O_NONBLOCK; 
    if( fcntl(nvm_fd, F_SETFL, flags) == -1)
    {
      NSDL3_SCHEDULE(NULL, NULL, "Error in fd BLOCKING so exiting thread");
      thd_info.opcode = VUTD_STOP_THREAD_EXIT;
      vutd_send_msg_to_nvm(0, (char *) (&thd_info), sizeof(Vutd_info));
    }
    thd_info.opcode = VUTD_INFO_REQ;
    vutd_send_msg_to_nvm(0, (char *) (&thd_info), sizeof(Vutd_info));
    NSDL3_SCHEDULE(NULL, NULL, "Msg send to NVM successfully by thread, thd_info = %p, thread_info = %p", (char *) (&thd_info), thread_info);
  } 
  while(1)
  {
    NSDL3_SCHEDULE(NULL, NULL, "Before jmp: thread_info = %p", thread_info);
    setjmp(g_tls.jmp_buffer);
    NSDL3_SCHEDULE(NULL, NULL, "After jmp: thread_info = %p", thread_info);
    if(sem_wait(&(thread_info->run_thread)) == -1)
    {
      perror("sem_wait");
      thd_info.opcode = VUTD_STOP_THREAD_EXIT;
      vutd_send_msg_to_nvm(0, (char *) (&thd_info), sizeof(Vutd_info));
    }
    NSDL3_SCHEDULE(NULL, NULL, "Thread wakes up for thread_info = %p", thread_info);
    VUser *vptr = thread_info->vptr;
    TLS_SET_VPTR(vptr);
    runlogic_func_ptr = runprof_table_shr_mem[vptr->group_num].gset.runlogic_func_ptr;
    runlogic_func_ptr();
  }
  TLS_FREE_AND_RETURN(0);
}

static int vutd_wait_for_nvm_rep(int type, char *send_msg, int size)
{
  // send message
  NSDL3_SCHEDULE(NULL, NULL, "Method Called, type = %d, send_msg = %p,sendmsg val = %d, size = %d", type, send_msg,*((int *)(send_msg)), size);
  Msg_com_con *tmp_nvm_info_tmp;
  tmp_nvm_info_tmp = (Msg_com_con *)get_thread_specific_data;
  NSDL3_SCHEDULE(NULL, NULL, "tmp_nvm_info_tmp = %p, tmp_nvm_info_tmp->con_type = %d", tmp_nvm_info_tmp, tmp_nvm_info_tmp->con_type);

  Vutd_info thd_info;
  thd_info.thread_info = tmp_nvm_info_tmp;
  tmp_nvm_info_tmp->fd = tmp_nvm_info_tmp->thread_nvm_fd;

  NSDL3_SCHEDULE(NULL, NULL, "Message opcode = %d, Message size = %d, thread_nvm_fd = %d, nvm_thread_fd = %d", *(((int *)(send_msg))+1), size,   tmp_nvm_info_tmp->thread_nvm_fd, tmp_nvm_info_tmp->nvm_thread_fd);
  NSTL2(NULL, NULL, "Message opcode = %d, Message size = %d, thread=>nvm fd = %d, nvm=>thread fd = %d", *(((int *)(send_msg))+1), size, tmp_nvm_info_tmp->thread_nvm_fd, tmp_nvm_info_tmp->nvm_thread_fd);
  
  // Send message to NVM
  if(vutd_write_msg(tmp_nvm_info_tmp->thread_nvm_fd, send_msg, size) < 0)
  {
    // Not able to send message to NVM so exit thread    
     NSTL1(NULL, NULL, "Error in sending message to NVM, Msg pointer: %p, fd = %d", send_msg, tmp_nvm_info_tmp->thread_nvm_fd);
     NSDL3_SCHEDULE(NULL, NULL, "Exiting Vuser thread due to error in sending msg to NVM, fd = %d", tmp_nvm_info_tmp->thread_nvm_fd);
     vutd_do_free_or_exit(tmp_nvm_info_tmp, VUTD_STOP_THREAD_EXIT);
     return -1;
  }
  
  // Wait for reply
  int ret;
  
  NSDL3_SCHEDULE(NULL, NULL, "Waiting for response from NVM on fd = %d", tmp_nvm_info_tmp->thread_nvm_fd);
  NSTL2(NULL, NULL, "Waiting for response from NVM on fd = %d", tmp_nvm_info_tmp->thread_nvm_fd);
  if(vutd_read_msg(tmp_nvm_info_tmp->thread_nvm_fd, tmp_nvm_info_tmp->read_buf) < 0)
  {
    NSDL3_SCHEDULE(NULL, NULL, "Error in Reading Msg from NVM on fd = %d", tmp_nvm_info_tmp->thread_nvm_fd);
    NSTL1(NULL, NULL, "Error in Reading Msg from NVM on fd = %d\n", tmp_nvm_info_tmp->thread_nvm_fd);
    vutd_do_free_or_exit(tmp_nvm_info_tmp, VUTD_STOP_THREAD_EXIT);
    return -1;
  }

  // Bug 111105 - Same handling as done for user context. We dont set vptr when message type is 0
  VUser *vptr = (VUser *) tmp_nvm_info_tmp->vptr;
  if(vptr && (vptr->flags & NS_VPTR_FLAGS_SESSION_COMPLETE))
  {
    NSDL3_SCHEDULE(vptr, NULL, "Session is marked as completed, Execute End sesssion. fd=%d", tmp_nvm_info_tmp->thread_nvm_fd);
    vptr->flags &= ~NS_VPTR_FLAGS_SESSION_COMPLETE;
    ns_exit_session();
    return 0;
  }

  NSDL3_SCHEDULE(NULL, NULL, "Received opcode from NVM = %d", *((int *)(tmp_nvm_info_tmp->read_buf) + 1));
  NSTL2(NULL, NULL, "Received opcode from NVM = %d", *((int *)(tmp_nvm_info_tmp->read_buf) + 1));

  switch(*((int *)(tmp_nvm_info_tmp->read_buf) + 1))
  {
    case VUTD_INFO_REP:
    case NS_API_PAGE_TT_REP:
    case NS_API_END_SESSION_REP:
   /* No need to do anything when sockjs response received */
    case NS_API_SOCKJS_CLOSE_REP:
    case NS_API_CONN_TIMEOUT_REP:
    case NS_API_SEND_TIMEOUT_REP:
    case NS_API_SEND_IA_TIMEOUT_REP:
    case NS_API_RECV_TIMEOUT_REP:
    case NS_API_RECV_IA_TIMEOUT_REP:
    case NS_API_RECV_FB_TIMEOUT_REP:
      break;
    case NS_API_WEB_URL_REP:
    case NS_API_RBU_WEB_URL_END_REP:


    case NS_API_SYNC_POINT_REP:
    case NS_API_START_TX_REP:
    case NS_API_END_TX_REP:
    case NS_API_END_TX_AS_REP:
    case NS_API_GET_TX_TIME_REP:
    case NS_API_SET_TX_STATUS_REP:
    case NS_API_GET_TX_STATUS_REP:
    case NS_API_ADVANCE_PARAM_REP:
    case NS_API_USER_DATA_POINT_REP:
    case NS_API_WEBSOCKET_SEND_REP:
    case NS_API_WEBSOCKET_CLOSE_REP:
    case NS_API_WEBSOCKET_READ_REP:
    case NS_API_SOCKET_SEND_REP:
    case NS_API_SOCKET_READ_REP:
    case NS_API_SOCKET_CLOSE_REP:
    case NS_API_XMPP_SEND_REP:
      ret = ((Ns_api_rep*) (tmp_nvm_info_tmp->read_buf))->ret_val;
      return ret;
      break;    
    case NS_API_END_SESSION_REQ:
      NSDL3_SCHEDULE(NULL, NULL, "Thread receives end session request, STOPPING thread..");
      NSTL2(NULL, NULL, "Thread receives end session request, STOPPING thread..");
      vutd_do_free_or_exit(tmp_nvm_info_tmp, VUTD_STOP_THREAD_FREE);
      break;
    case VUTD_STOP_THREAD_EXIT:
      vutd_do_free_or_exit(tmp_nvm_info_tmp, VUTD_STOP_THREAD_EXIT);
      break;
    default:
     NSTL1(NULL, NULL, "message %s is not valid msg, Opcode received in this case = %d", tmp_nvm_info_tmp->read_buf, *((int *)(tmp_nvm_info_tmp->read_buf)));
     //sending msg to nvm to free thread rather than doing it from thread itself
     thd_info.opcode = VUTD_STOP_THREAD_EXIT;
     vutd_send_msg_to_nvm(VUTD_STOP_THREAD_EXIT, (char *) (&thd_info), sizeof(Vutd_info));
  }
  NSDL3_SCHEDULE(NULL, NULL, "Mehod Exiting");
   return 0;
}

// Called from ns_web_url_ex 

int vutd_send_msg_to_nvm(int type, char *send_msg, int size)
{
  NSDL3_SCHEDULE(NULL, NULL, "Method Called, type = %d, sendmsg = %p, size = %d", type, send_msg, size);

  return(vutd_wait_for_nvm_rep(type, send_msg, size));
}
