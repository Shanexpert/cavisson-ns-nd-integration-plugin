/******************************************************************
 * Name    :    ns_child.c
 * Purpose :    This file contains methods for NetStorm child
 * Note    :
 * Author  :    Archana
 * Intial version date:    07/04/08
 * Last modification date: 08/04/08
*****************************************************************/
#include <stdio.h>
#include <gsl/gsl_randist.h>
#include <v1/topolib_structures.h>
#ifdef USE_EPOLL
#include <sys/epoll.h>
#endif
#include <regex.h>

#include "nslb_big_buf.h"

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
#include "ns_smtp.h"
#include "ns_pop3.h"
#include "ns_ftp.h"
#include "ns_dns.h"
#include "ns_http_pipelining.h"
#include "ns_runtime_changes.h"
#include "ns_child_runtime_changes.h"
#include "ns_event_log.h"
#include "ns_string.h"
#include "ns_event_id.h"
#include "ns_sock_listen.h"
#include "ns_vuser_tasks.h"
#include "ns_vuser_trace.h"
#include "ns_vuser_ctx.h"
#include "ns_vuser.h"
#include "ns_child_thread_util.h"

#include "ns_vuser_ctx.h"
#include "ns_sync_point.h"

#include "tr069/src/ns_tr069_acs_con.h"
#include "tr069/src/ns_tr069_http_read_req.h"
#include "ns_schedule_phases_parse.h"
#include "ns_monitoring.h"
#include "ns_trace_level.h"
#include "ns_websocket.h"
#include "ns_websocket_reporting.h"
#include "ns_static_vars_rtc.h"

// For njvm
#include "ns_njvm.h"
#include "ns_nvm_njvm_msg_com.h"
#include "ns_ldap.h"
#include "ns_jrmi.h"
#include "ns_imap.h"
#include "ns_java_obj_mgr.h"
#include "ns_group_data.h"
#include "ns_runtime_changes_quantity.h"
#include "ns_server_ip_data.h"
#include "ns_dynamic_avg_time.h"
//for http2
#include "ns_h2_req.h"

#include "ns_connection_pool.h"
#include "ns_exit.h"
#include "ns_jmeter.h"
#include "ns_xmpp.h"
#include "ns_vuser_runtime_control.h"
#include "ns_handle_alert.h"
#include "nslb_util.h"
#include "ns_socket_io.h"
#include "ns_h2_header_files.h"
#include "ns_cavmain_child.h"
#define RUNTIME_CHANGE_NOT_INITIATED 0 // Not started
#define RUNTIME_CHANGE_STARTED       1 // Parent send signal to start
#define RUNTIME_CHANGE_END           2 // Parent completed update in shm
#define NS_STRUCT_TYPE_SM_THREAD_LISTEN            24  // Msg_com_con structure used for SM Thread to NVM Listen
#define NS_STRUCT_TYPE_SM_THREAD_COM            25  // Msg_com_con structure used for SM Threads to NVM communication
static int runtime_change_done_by_parent = 0;  // it is For Run time changes
extern Msg_com_con ndc_mccptr;
//extern void make_new_partition_dir_and_send_msg_to_proc(ClientData cd, u_ns_ts_t now);
extern void set_idx_for_partition_switch(ClientData cd, u_ns_ts_t now);
extern int g_rtc_msg_seq_num;
int g_quantity_flag;

/* Process RTC_PAUSE message from parent*/
static void process_pause_for_rtc_msg_frm_parent(parent_child *msg, u_ns_ts_t now)
{

  if (global_settings->rtc_pause_seq !=  msg->gen_rtc_idx)  //Old Message Ignore
  {
    NSTL1(NULL, NULL, "(Parent -> NVM:%d) RTC_PAUSE(138) ignoring pause. rtc_pause_seq =%d, gen_rtc_idx=%d", global_settings->rtc_pause_seq, msg->gen_rtc_idx);
    return;
  }
  g_quantity_flag = msg->ns_version;
  NSDL1_RUNTIME(NULL, NULL, "Method called, RTC: Child id %d Received RTC_PAUSE message from parent, hence stopping processing., "
                            "runtime_change_done_by_parent = %d, rtc_pause_seq = %d, gen_rtc_idx = %d", 
                             my_port_index, runtime_change_done_by_parent, global_settings->rtc_pause_seq, msg->gen_rtc_idx);
  NSTL1(NULL, NULL, "(Parent -> NVM:%d) RTC_PAUSE(138) initiating pause, quantity = %d", my_port_index, g_quantity_flag);
  g_rtc_msg_seq_num = msg->gen_rtc_idx; //Save sequence number
  rtc_quantity_pause_resume(msg->opcode, now);
  NSDL2_SCHEDULE(NULL, NULL, "g_rtc_msg_seq_num = %d for opcode =  %d", g_rtc_msg_seq_num, msg->opcode);
  if (runtime_change_done_by_parent == RUNTIME_CHANGE_NOT_INITIATED)
    runtime_change_done_by_parent = RUNTIME_CHANGE_STARTED;
}

//For children
static void
handle_child_sigrtmin( int sig )
{
  NSDL2_MESSAGES(NULL, NULL, "Method called, Received sig = %d, pid = %d, interrup Rcd", sig, getpid());
  NSDL2_MESSAGES(NULL, NULL, "Reset Child on Resumed, runtime_change_done_by_parent = %d", runtime_change_done_by_parent);
  rtc_reset_child_on_resumed();

  #ifdef NS_DEBUG_ON
    NSDL2_MESSAGES(NULL, NULL, "NVM(%d): Dump per_proc table after shm_addr updating --->>", my_port_index);
    dump_per_proc_vgroup_table_internal(&per_proc_vgroup_table[(my_port_index * total_group_entries)]);
  #endif

  // Sleep so that parent can update all the data structs of this child
  //usleep(1000 * 100);
}

//For children
static void
handle_child_sigint( int sig )
{
  NSDL2_MESSAGES(NULL, NULL, "Method called, Received sig = %d, pid = %d, interrup Rcd", sig, getpid());
  //printf("%d: interrup Rcd\n", getpid());
  sigterm_received = 1;
}

static void
handle_child_sigpipe( int sig )
{
  NSDL2_MESSAGES(NULL, NULL, "Method called, Received sig = %d", sig);
  /* Nothing special.  We only catch the signal so that syscalls return
  ** an error, instead of just exitting the process.
  */
  //  printf("Signal %d rcd\n", sig);
}

void
handle_child_sigterm( int sig )
{
  // printf("%d: sigterm rcd at %u\n", getpid(), get_ms_stamp());
  //sigterm_received = 1;
  //flush_logging_buffers();
  int i;
  NSDL2_MESSAGES(NULL, NULL, "Method called, Received sig = %d", sig);
  //NS_EXIT(0, "END_TEST_RUN received from parent, nvmid = %d. Exiting..", child_idx);
  NSTL1(NULL,NULL, "SIGTERM received (Parent -> NVMID:%d), GenID = %d, Exiting..", my_port_index, child_idx/256);

  NSDL1_MESSAGES(NULL, NULL, "Calling for CAV_MEMORY_DUMP");
  ns_process_cav_memory_map();

  /* Bug - 79355: When NVM is getting SIGTERM, all of its associated threads are not getting sufficient time to get 
     killed/cleaned properly which resulting in core. */
  /* Do this only if atleast one group is running in thread mode */
  for(i=0; i < total_runprof_entries; i++)
  {
     if(runprof_table_shr_mem[i].gset.script_mode == NS_SCRIPT_MODE_SEPARATE_THREAD){
        fprintf(stdout,"\nWaiting for 10 seconds so that all threads associated to NVM get cleaned properly\n");
        sleep(10);
        break;
     }
  }

  exit(0);
}

static void
handle_child_sigrtmin1( int sig )
{
  NSDL2_MESSAGES(NULL, NULL, "Method called, Received sig = %d, pid = %d, interrup Rcd", sig, getpid());
  sigterm_received = 1;
  end_test_run_mode = 1; //Setting flag for stopping test immediately
}

// Progress report related methods ***************************

void start_progress_reports(u_ns_ts_t now)
{
  #ifndef CAV_MAIN
  u_ns_ts_t rampnow;
  ClientData rampcd;
  int i;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Starting Progress Report Timer. now=%u, progress_secs = %u", now, global_settings->progress_secs);
  for(i = 0; i < TOTAL_GRP_ENTERIES_WITH_GRP_KW; i++) {
    GET_AVG_STRUCT(average_time, i);
    init_avgtime(tmp_reset_avg, i);
  }
  total_badchecksums = 0;

  rampnow = now = get_ms_stamp();
  g_start_time = rampnow;
  cum_timestamp = rampnow;
  interval_start_time = rampnow;
  // Start Timer for progres report
  //ab_timers[AB_TIMEOUT_PROGRESS].timeout_val = global_settings->progress_secs;
  progress_tmr->actual_timeout = global_settings->progress_secs;
  if ( do_verbose ) // do_verbose is always 1
  {
    rampcd.i = 0; // This is to indicate that progress report is last (complete) or not
    (void) dis_timer_add( AB_TIMEOUT_PROGRESS, progress_tmr, rampnow, progress_report,  rampcd, 1 );
  }
  #endif
}
//Process msg send by parent
void process_change_tr_msg_frm_parent(HourlyMonitoringSess_msg* tr_msg)
{
  NSDL2_MESSAGES(NULL, NULL, "Method Called. testrun id = %d, opcode = %d", tr_msg->testidx, tr_msg->opcode);

  if(loader_opcode == CLIENT_LOADER)
  {
    ClientData cd;
    g_partition_idx_for_generator = tr_msg->partition_idx; 
    //in case of generator call this, this will handle everything
    //make_new_partition_dir_and_send_msg_to_proc(cd, get_ms_stamp());
    set_idx_for_partition_switch(cd,get_ms_stamp());
  }
  else
  {
    //Flush share memory before switching to new partition
    flush_shm_buffer();
    //Need to close certain files
    //Update testrun index.
    testidx = tr_msg->testidx;
    g_partition_idx = tr_msg->partition_idx;
    sprintf(global_settings->tr_or_partition, "TR%d/%lld", testidx, g_partition_idx);
    close_and_reset_fd();
    /*open dlog file*/
    if ((open_dlog_file_in_append_mode(1)) == -1)
      NS_EXIT(-1, "Failed to open dlog file in append mode.");
  } 
  NSDL2_MESSAGES(NULL, NULL, "New partition_idx = %lld, global_settings->tr_or_partition = %s.", testidx, global_settings->tr_or_partition);
}

//Process nsa logger pid change msg send by parent
static void process_nsa_logger_pid_change_msg_frm_parent(parent_child* el_msg)
{
  NSTL1(NULL, NULL, "Method Called. opcode = %d, current nsa logger pid is = %d", el_msg->opcode, el_msg->nsa_logger_pid);
  writer_pid = el_msg->nsa_logger_pid;
}

static void handle_msg_from_parent(struct epoll_event *pfds, void *mccptr, int ii, u_ns_ts_t now)
{
  parent_child *rcv_msg;
  int size;

  NSDL2_MESSAGES(NULL, NULL, "Method Called.");

  if (pfds[ii].events & EPOLLOUT)
  {
    if (g_child_msg_com_con.state & NS_STATE_WRITING){
      write_msg(&g_child_msg_com_con, NULL, 0, 0, CONTROL_MODE);
    }
    else 
    {
      NSDL3_MESSAGES(NULL, NULL, "Write state not `writing', still we go EPOLLOUT event (in child)");
    }

    /* For Event Logging*/
    if(global_settings->event_log && global_settings->enable_event_logger) {
       if (g_el_subproc_msg_com_con.state & NS_STATE_WRITING) {
          write_msg(&g_el_subproc_msg_com_con, NULL, 0, 0, CONTROL_MODE);
       } else {
          NSDL3_MESSAGES(NULL, NULL, "Event logger Write state not `writing', still we go EPOLLOUT event (in child)");
       }
    }
  }

  if ((pfds[ii].events & EPOLLIN) &&
     (rcv_msg = (parent_child *)read_msg(&g_child_msg_com_con, &size, CONTROL_MODE)) != NULL)
  {
       NSDL3_MESSAGES(NULL, NULL, "opcode = %d, g_child_msg_com_con = %p", rcv_msg->opcode, g_child_msg_com_con);
       // Bug 92660
       #ifdef CAV_MAIN
       if (NS_START_TEST == rcv_msg->opcode) 
       {
          NSDL3_MESSAGES(NULL, NULL, "Recieved START_SM_REQ from parent.");
          cm_process_sm_start_msg_frm_parent((CM_MON_REQ*)((char*)rcv_msg));
       }
       else if(rcv_msg->opcode == NS_STOP_TEST)
       {
          NSDL3_MESSAGES(NULL, NULL, "Recieved STOP_SM_REQ from parent.");
          cm_process_sm_stop_msg_frm_parent((CM_MON_REQ*)((char*)rcv_msg));
       }
       else if (rcv_msg->opcode == START_COLLECTING)
       #else
       if (rcv_msg->opcode == START_COLLECTING)
       #endif
       {
          NSDL3_MESSAGES(NULL, NULL, "Recieved START_COLLECTING from parent.");
          start_collecting_reports(now);
       }
       else if(rcv_msg->opcode == START_PHASE)
           process_schedule_msg_from_parent(rcv_msg);
       else if(rcv_msg->opcode == PAUSE_SCHEDULE || rcv_msg->opcode == RESUME_SCHEDULE)
          pause_resume_netstorm(rcv_msg->opcode, now);
       else if(rcv_msg->opcode == VUSER_TRACE_REQ)
          start_vuser_trace((User_trace *)rcv_msg, now);
       else if(rcv_msg->opcode == SP_VUSER_WAIT)
         process_sp_wait_msg_frm_parent((SP_msg*)rcv_msg);
       else if(rcv_msg->opcode == SP_VUSER_CONTINUE)
         process_sp_continue_msg_frm_parent((SP_msg*)rcv_msg);
       else if(rcv_msg->opcode == SP_RELEASE)
         process_sp_release_msg_frm_parent((SP_msg*)rcv_msg);
       else if(rcv_msg->opcode == RTC_PAUSE)
         process_pause_for_rtc_msg_frm_parent(rcv_msg, now);
       else if(rcv_msg->opcode == NSA_LOG_MGR_PORT_CHANGE_MESSAGE)
         process_log_mgr_port_change_msg_frm_parent(rcv_msg);
       else if(rcv_msg->opcode == NSA_LOGGER_PID_CHANGE_MESSAGE)
         process_nsa_logger_pid_change_msg_frm_parent(rcv_msg);
       else if(rcv_msg->opcode == FPARAM_RTC_ATTACH_SHM_MSG)
         process_fparam_rtc_attach_shm_msg(rcv_msg);
       else if(rcv_msg->opcode == GET_VUSER_SUMMARY)
         ns_process_vuser_summary();
       else if(rcv_msg->opcode == GET_VUSER_LIST)
         ns_process_vuser_list((char *)rcv_msg + sizeof(RTC_VUser), size - sizeof(RTC_VUser));
       else if(rcv_msg->opcode == PAUSE_VUSER)
         ns_process_pause_vuser((char *)rcv_msg + sizeof(RTC_VUser), size - sizeof(RTC_VUser));
       else if(rcv_msg->opcode == RESUME_VUSER)
         ns_process_resume_vuser((char *)rcv_msg + sizeof(RTC_VUser), size - sizeof(RTC_VUser));
       else if(rcv_msg->opcode == STOP_VUSER)
         ns_process_stop_vuser((char *)rcv_msg + sizeof(RTC_VUser), size - sizeof(RTC_VUser));
       else if(rcv_msg->opcode == APPLY_ALERT_RTC)
         ns_process_apply_alert_rtc();
       else 
       {
          NSDL3_MESSAGES(NULL, NULL, "rcv_msg opcode is %d, recv_read is %d", rcv_msg->opcode, size);
          NSDL3_MESSAGES(NULL, NULL, "recieved invalid message from parent control connection.");
       }	     
    }
}

static void handle_msg_from_parent_dh(struct epoll_event *pfds, void *mccptr, int ii, u_ns_ts_t now)
{
  parent_child *rcv_msg;
  int size;

  NSDL2_MESSAGES(NULL, NULL, "Method Called.");

  if (pfds[ii].events & EPOLLOUT)
  {
    if (g_dh_child_msg_com_con.state & NS_STATE_WRITING){
      write_msg(&g_dh_child_msg_com_con, NULL, 0, 0, DATA_MODE);
    }
    else 
    {
      NSDL3_MESSAGES(NULL, NULL, "Write state not `writing', still we go EPOLLOUT event (in child)");
    }

  }

  if ((pfds[ii].events & EPOLLIN) &&
     (rcv_msg = (parent_child *)read_msg(&g_dh_child_msg_com_con, &size, DATA_MODE)) != NULL) 
  {
    NSDL3_MESSAGES(NULL, NULL, "opcode = %d, g_dh_child_msg_com_con = %p", rcv_msg->opcode, g_dh_child_msg_com_con);
    if(rcv_msg->opcode == NS_NEW_OBJECT_DISCOVERY_RESPONSE)
      process_parent_object_discover_response(&g_dh_child_msg_com_con, (Norm_Ids *)rcv_msg);
    else if(rcv_msg->opcode == NEW_TEST_RUN)
      process_change_tr_msg_frm_parent((HourlyMonitoringSess_msg*)rcv_msg);
    else if(rcv_msg->opcode == CAV_MEMORY_MAP)
      ns_process_cav_memory_map();
    else 
    {
      NSDL3_MESSAGES(NULL, NULL, "rcv_msg opcode is %d, recv_read is %d", rcv_msg->opcode, size);
      NSDL3_MESSAGES(NULL, NULL, "recieved invalid message from parent data connection.");
    }	     
  }
}

/* Manipulates epoll timeout */
static int calculate_child_epoll_timeout(int *call_timers, int *epoll_timeout_zero_cnt, u_ns_ts_t now) {

  int temp_ms = 0;
  timer_type* temp_tmr = NULL;

  NSDL2_POLL(NULL, NULL, "Method called, call_timers, = %d now = %u", *call_timers, now);

  // Call timers if set 
  if(*call_timers) {
    temp_tmr = dis_timer_next( now );
    if (temp_tmr == NULL) {
        //NSTL1(NULL, NULL, "temp_ms = %d, temp_tmr->timeout = %d", temp_ms, temp_tmr->timeout);
       NSTL1(NULL, NULL, "No timeout pending for NVM (%d). Using infinite time in epoll_wait().",
				  my_child_index);
      temp_ms = -1;
    } else {  /*temp_tmr is not NULL*/
        temp_ms = temp_tmr->timeout - get_ms_stamp();
	/*if temp_ms is got 0 more than nvm_timeout_max_cnt than we need to enforce timeout*/
        if (temp_ms > 0) {
          /*Reset zero count as we got temp_ms > 0*/
          *epoll_timeout_zero_cnt = 0;
       } else {
         (*epoll_timeout_zero_cnt)++;
	 /* we need to set temp_ms with nvm_epoll_timeout if 0 count exceeds nvm_timeout_max_cnt*/
         if(*epoll_timeout_zero_cnt < global_settings->nvm_timeout_max_cnt) {
           temp_ms = 0;
	   /* Setting call timers on the basis of skip_nvm_timer_call*/
           *call_timers = !global_settings->skip_nvm_timer_call;
         } else {
       	   NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_WARNING,
                                	__FILE__, (char*)__FUNCTION__,
                                 	"Setting NVM (%d) epoll_wait timeout to %d.",
				  	my_child_index, global_settings->nvm_epoll_timeout);
           temp_ms = global_settings->nvm_epoll_timeout;
           *epoll_timeout_zero_cnt = 0; // Reset
         }
       }
    }
  } else {  // Call timer is not set
      temp_ms = global_settings->nvm_epoll_timeout;
      *call_timers = 1;  // Call timers
  }

  NSDL3_POLL(NULL, NULL, "About to sleep for %d msec at %u, call_timers = %d", 
					temp_ms, get_ms_stamp(), *call_timers);
  return temp_ms;
}

void check_and_create_nvm_listner()
{
  int nvm_listen_fd = -1;
  int i = 0; 
  int num_thread;

  //NSTL1_OUT(NULL, NULL, "v_port_table[my_port_index].num_vusers = %d, my_port_index = %d\n", v_port_table[my_port_index].num_vusers, my_port_index);
  for (i = 0; i < total_runprof_entries; i++) { 
    //NSTL1_OUT(NULL, NULL, "runprof_table_shr_mem[%d].gset.script_mode = %d\n", i, runprof_table_shr_mem[i].gset.script_mode);
    if(runprof_table_shr_mem[i].gset.script_mode == NS_SCRIPT_MODE_SEPARATE_THREAD){
      NSDL2_MESSAGES(NULL, NULL, "runprof_table_shr_mem[i].gset.script_mode = %d, nvm_listen_fd = %d", runprof_table_shr_mem[i].gset.script_mode, nvm_listen_fd);
      if((nvm_listen_fd = vutd_create_listen_fd(&g_nvm_listen_msg_con, NS_STRUCT_TYPE_VUSER_THREAD_LISTEN, &g_nvm_listen_port)) == -1)
      {
        // TODO - send end parent
        NSTL1(NULL, NULL, "Error: Unable to create Listener for threads\n");
        NS_EXIT (0, "Error: Unable to create Listener for threads");
      }
      add_select_msg_com_con((char*)&g_nvm_listen_msg_con, nvm_listen_fd, EPOLLIN);
      //num_thread = global_settings->init_thread / global_settings->num_process;
      if (global_settings->init_thread < global_settings->num_process)
      {
        global_settings->init_thread = global_settings->num_process;
        NSTL1(NULL, NULL, "Initial number of threads are less than number of NVM's so setting it equal to NVMs. Now, Init_thread = %d and num_process = %d", global_settings->init_thread, global_settings->num_process);
      }
      num_thread = global_settings->init_thread / global_settings->num_process;
      if(global_settings->max_thread < (global_settings->init_thread + global_settings->incremental_thread)) {
        global_settings->max_thread = global_settings->init_thread + global_settings->incremental_thread;
        NSTL1(NULL, NULL, "Max thread is less then sum of init and incremantal thread so setting it to init+incremental."
                          "global_settings->max_thread = %d", global_settings->max_thread);
      }
           
      NSTL1(NULL, NULL, "init_thread = %d, num_process = %d, num_thread = %d", global_settings->init_thread, global_settings->num_process, num_thread);
     

      if(vutd_create_thread(num_thread) == 0)
      {
        NSTL1(NULL, NULL, "Error: Creating Threads\n");
        NS_EXIT (0, "Error: Creating Threads");
      }  

      NSDL2_MESSAGES(NULL, NULL, "NVM listener is start with fd %d, g_nvm_listen_port = %d, g_nvm_listen_msg_con->type = %d, g_nvm_listen_msg_con = %p, g_nvm_listen_msg_con.fd = %d", nvm_listen_fd, g_nvm_listen_port, g_nvm_listen_msg_con.con_type, &g_nvm_listen_msg_con, g_nvm_listen_msg_con.fd);
      break;
    }
  }
}

/*static void handle_sig_alarm_for_system_pause(int sig)
{ 
  NSDL2_MESSAGES(NULL, NULL, "Method called");
  NSTL1(NULL, NULL, "Parent missed RTC_RESUME(139) hence breaking pause() for NVM = %d through alarm timeout", my_port_index);
} */

/* clear all copied data inherited during fork */
static void clear_parent_inherited_data()
{
 // ServerInfo *servers_list = NULL;
 // int total_no_of_servers = 0;
  
  if(ndc_mccptr.fd > 0){
    close(ndc_mccptr.fd);
    ndc_mccptr.fd = -1;
  }

  if(ndc_data_mccptr.fd > 0){
    close(ndc_data_mccptr.fd);
    ndc_data_mccptr.fd = -1;
  }

  close_parent_listen_fd();/* (udp_fd); */ /* udp_fd is for the parent only */
/*bug 92660: added if() before closing any fd*/
  if(g_msg_com_epfd > 0)
   close(g_msg_com_epfd);
  if(g_dh_msg_com_epfd > 0)
    close(g_dh_msg_com_epfd);
  if(rfp > 0)
    fclose(rfp);

 // servers_list = (ServerInfo *) topolib_get_server_info();
 // total_no_of_servers = topolib_get_total_no_of_servers();

  for(int i = 0; i < topo_info[topo_idx].total_tier_count; i++) 
  {
    for(int j=0;j<topo_info[topo_idx].topo_tier_info[i].total_server;j++)
    {
      if((topo_info[topo_idx].topo_tier_info[i].topo_server[j].used_row != -1) && (topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->control_fd != -1))
      {
        if(close(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->control_fd) == -1) 
        {
        // closing error
        }
      }
    }
  }
  for(int i = 0; i < total_monitor_list_entries; ++i) {
    if(monitor_list_ptr[i].cm_info_mon_conn_ptr->fd != -1) {
      if(close(monitor_list_ptr[i].cm_info_mon_conn_ptr->fd) == -1) {
        // error closing
      }
    }
  }

  if (!total_um_entries)
    free_gdf();
  else
    free_gdf_data();
  
  free_cm_info();
}

/*bug 79062 */
/*handle ssl write for partial data  */
int h2_handle_ssl_write_ex(connection *cptr, u_ns_ts_t now){

  int ret;
  NSDL2_MESSAGES(NULL, cptr, "Method called cptr->http2->front=%p cptr->http2_rear=%p cptr->http2->http2_cur_stream=%p",
                       cptr->http2->front, cptr->http2->rear,cptr->http2->http2_cur_stream);
  //add http2_cur_stream to queue if it is not equal to front
  if(cptr->http2->http2_cur_stream != cptr->http2->front)
     add_node_to_queue(cptr, cptr->http2->http2_cur_stream);
  NSDL2_MESSAGES(NULL, cptr, "cptr->bytes_left_to_send=%d",cptr->bytes_left_to_send);
  /*if(cptr->bytes_left_to_send)
    return HTTP2_ERROR;*/
  while(cptr->http2->front){
    NSDL2_MESSAGES(NULL, cptr, "cptr->http2->front=%p",cptr->http2->front);
    // Copy front data into cptr  
    copy_stream_to_cptr(cptr, cptr->http2->front);
    if(!cptr->bytes_left_to_send){
      cptr->http2->front = cptr->http2->front->q_next;
      continue;
    }
      
    cptr->http2->http2_cur_stream = cptr->http2->front;
    ret = handle_ssl_write (cptr, now);
    if(ret == 0){
      NSDL2_MESSAGES(NULL, cptr, "Going to write next request[%p]",cptr->http2->front->q_next);
      cptr->http2->front = cptr->http2->front->q_next;
    } else if(ret == -1){
      NSDL2_MESSAGES(NULL, cptr, "Eagain copy all the counters to scptr");
      copy_cptr_to_stream(cptr, cptr->http2->front);
      break;
    } else {
      NSDL2_MESSAGES(NULL, cptr, "Error in writing request");
      break;
    }
  }
 return ret;
}

#if 0
//Funtion used to add parent fds in new epoll created by child
inline int child_add_select(char* data_ptr, int fd, int event)
{
  struct epoll_event pfd;
  
  NSDL1_MESSAGES(NULL, NULL, "Method called. Adding fd = %d for event = %x", fd, event);
  
  bzero(&pfd, sizeof(struct epoll_event)); //Added after valgrind reported bug
  
  pfd.events = event;
  pfd.data.ptr = (void *) data_ptr;
  
  if (epoll_ctl(v_sleep_epoll_fd, EPOLL_CTL_ADD, fd, &pfd) == -1)
  {
    NSTL1(NULL, NULL, "EPOLL ERROR occured in EPOLL_CTL_ADD child process[%d] for fd %d Err[%d]: %s", my_port_index, fd, 
                       errno, nslb_strerror(errno));
    return 1;
  } 
  return 0;
}

//Funtion used to remove parent fds in new epoll created by child
static inline void child_remove_select()
{
  struct epoll_event pfd;

  NSDL1_MESSAGES(NULL, NULL, "Method called. Removing fd = %d from ns_pause() epoll", parent_fd);
  bzero(&pfd, sizeof(struct epoll_event)); //Added after valgrind reported bug

  if (epoll_ctl(v_sleep_epoll_fd, EPOLL_CTL_DEL, parent_fd, &pfd) == -1)
  {
    NSTL1(NULL, NULL, "EPOLL ERROR occured in EPOLL_CTL_DEL child[%d] for fd %d Err[%d]: %s", my_port_index, parent_fd, errno, nslb_strerror(errno));
    return;
  }
}

int child_wait_for_resume()
{
  int cnt, i;
  int epoll_timeout;
  struct epoll_event epev;
  u_ns_ts_t local_rtc_epoll_start_time;
  u_ns_ts_t epoll_wait_time = 0;
  int rcv_amt;
  void *event_ptr = NULL;
  parent_child *msg = NULL;
  
  NSDL1_MESSAGES(NULL, NULL, "Method called");
  
  local_rtc_epoll_start_time = get_ms_stamp();
  epoll_timeout = global_settings->rtc_system_pause_timeout_val*1000;
  while(1)
  {

    NSDL2_MESSAGES(NULL, NULL, "child epoll timeout: epoll_timeout = %d", epoll_timeout);

    memset(&epev, 0, sizeof(struct epoll_event));

    epoll_timeout = epoll_timeout - epoll_wait_time;
    cnt = epoll_wait(v_sleep_epoll_fd, &epev, 1, epoll_timeout);

    epoll_wait_time = get_ms_stamp() - local_rtc_epoll_start_time;
    NSDL2_MESSAGES(NULL, NULL, "cnt = %d", cnt);
   
    if ((cnt <= 0) && (epoll_wait_time < epoll_timeout))
    {
      if (errno != EINTR)
        NSTL1(NULL, NULL, "netstorm_child: epoll_wait. Err[%d]: %s", errno, nslb_strerror(errno));
      continue;
    }

    for (i = 0; i < cnt; i++)
    {
      event_ptr = epev.data.ptr;
      if(event_ptr == NULL)
        continue;
      
      NSDL1_POLL(NULL, NULL, "epoll return type = %d, event_ptr = %p, events = %s", 
                              *(char *)event_ptr, event_ptr, (epev.events & EPOLLOUT)?"EPOLLOUT":"EPOLLIN");
      if (epev.events & EPOLLERR) {
        continue;
      }

      msg = NULL;
      if (epev.events & EPOLLIN)
        msg = (parent_child *)read_msg(&g_child_msg_com_con, &rcv_amt, CONTROL_MODE);

      if (msg == NULL) continue;

      NSDL3_MESSAGES(NULL, NULL, "msg->top.internal.opcode = %d", msg->opcode);

      switch (msg->opcode)
      {
        case RTC_RESUME:
          g_rtc_msg_seq_num = msg->gen_rtc_idx;
          runtime_change_done_by_parent = RUNTIME_CHANGE_NOT_INITIATED;
          return 0;  
          break;

        case APPLY_ALERT_RTC:
          ns_process_apply_alert_rtc();
          break;
       
        case SP_RELEASE:
          process_sp_release_msg_frm_parent((SP_msg*)msg);
          break;
 
        default:
          NSTL1(NULL, NULL, "Recieved invalid message from parent opcode = %d", msg->opcode);
          break;
      }
    }
   
    //In case of epoll timeout we need to 
    if(epoll_wait_time >= epoll_timeout)
    {
      runtime_change_done_by_parent = RUNTIME_CHANGE_NOT_INITIATED;
      NSTL1(NULL, NULL, "RTC Epoll Timeout: start time %d, epoll_wait_time %d, epoll_timeout = %d",
                   local_rtc_epoll_start_time, epoll_wait_time, epoll_timeout);
      return -1;
    }
  }
  return 0;
}
#endif

void ns_pause(u_ns_ts_t now)
{
  int i;
  int total_timeout;
  u_ns_ts_t local_rtc_start_time;
  int wait_time = 0;
  int timeout;

  NSDL2_MESSAGES(NULL, NULL, "Method Called");
  local_rtc_start_time = get_ms_stamp();
  total_timeout = global_settings->rtc_system_pause_timeout_val*1000;

  NSDL2_MESSAGES(NULL, NULL, " Processing stopping = %d, rtc_seq = %d", global_settings->rtc_pause_seq, g_rtc_msg_seq_num);  
  for(i = 0; i < 5; i++) //Retry to resend message if partially send.
  {
    send_msg_from_nvm_to_parent(RTC_PAUSE_DONE, CONTROL_MODE);
    //RTC_PAUSE_DONE message send to parent now NVM can pause
    if (g_child_msg_com_con.write_bytes_remaining == 0) {
      NSTL1(NULL, NULL, "(NVM:%d -> Parent) RTC_PAUSE_DONE(145) processing stopped", my_port_index);
      break; 
    }      
    //In case of EAGAIN we need to sleep for 2 sec and retry to write message to parent
    NSTL1(NULL, NULL, "(NVM:%d -> Parent) [partial] RTC_PAUSE_DONE(145), Sending in 200ms interval for %d times,"
                      " write_bytes_remaining = %d", i + 1, g_child_msg_com_con.write_bytes_remaining);
    usleep(200 * 1000);
    NSTL1(NULL, NULL, "Sleep Completed/Interrupted for %d times", i + 1);
  }
  
  //Unable to send message to parent then go to pause state now parent will send resume message on epoll timeout
  if (i == 5) {
    NSTL1(NULL, NULL, "Failed to send RTC_PAUSE_DONE to parent still going to pause state."
                      " Expected parent will resume NVM on epoll timeout");
    return; 
  } 

  while(1)
  {
    timeout = total_timeout - wait_time;
    if(timeout > 0)
    {
      NSDL1_MESSAGES(NULL, NULL, " Processing stopped = %d, rtc_seq = %d", global_settings->rtc_pause_seq, g_rtc_msg_seq_num);  
      usleep(100 * 1000); //100 ms
      
      //Check if progress report timer expire then send progress report
      dis_timer_run_ex(get_ms_stamp(), AB_TIMEOUT_PROGRESS);

      if (global_settings->rtc_pause_seq != g_rtc_msg_seq_num)  //RESUME
      {
        rtc_reset_child_on_resumed();

        #ifdef NS_DEBUG_ON
        NSDL2_MESSAGES(NULL, NULL, "NVM(%d): Dump per_proc table after shm_addr updating --->>", my_port_index);
        dump_per_proc_vgroup_table_internal(&per_proc_vgroup_table[(my_port_index * total_group_entries)]);
        #endif
        send_msg_from_nvm_to_parent(RTC_RESUME_DONE, CONTROL_MODE);
        runtime_data_updation(now);
        NSDL2_MESSAGES(NULL, NULL, "Run time update by NVM is complete");
        //Resume done is sent, resume phase timers
        rtc_quantity_pause_resume(RTC_RESUME, get_ms_stamp());
        runtime_change_done_by_parent = RUNTIME_CHANGE_NOT_INITIATED;

        return; 
      }
      
      wait_time = get_ms_stamp() - local_rtc_start_time;
    } 
    else
    {
      runtime_change_done_by_parent = RUNTIME_CHANGE_NOT_INITIATED;
      NSTL1(NULL, NULL, "RTC Epoll Timeout: start time %d, wait_time %d, timeout = %d",
                 local_rtc_start_time, wait_time, timeout);
      return;  
    }
  }
}

void netstorm_child()
{
  struct epoll_event pfds[NS_EPOLL_MAXFD];
  struct epoll_event epev[1];
  int ii, fd;
  u_ns_ts_t now =0;
  int r;
  connection *cptr;
  VUser *vptr = NULL;
  int temp_ms;
  int events;
  int epoll_timeout_zero_cnt = 0;
  int call_timers = 1;  // flag to call or not to call  dis_timer_run & dis_timer_next
  void *event_ptr = NULL;
  int accept_fd;
  int ws_ssl_ret = 0;
  u_ns_ts_t event_proc_st; 
  u_ns_ts_t event_proc_et; 

  SET_CALLER_TYPE(IS_NVM);

#ifndef NS_PROFILE
	my_port_index = atoi(getenv("CHILD_INDEX"));
	my_child_index = my_port_index;
#else
	my_port_index = 0;
#endif

#ifdef CAV_MAIN
  my_port_index = 0;
#endif


  /* CHILD PROCESS or non-parallel process */
  NSDL2_MESSAGES(NULL, NULL, "Method called, nvm_epoll_timeout = %d, nvm_timeout_max_cnt = %d, skip_nvm_timer_call = %d",
                                           global_settings->nvm_epoll_timeout,
                                           global_settings->nvm_timeout_max_cnt,
                                           global_settings->skip_nvm_timer_call);  

    //moved to function clear_parent_inherited_data ()
    clear_parent_inherited_data();

    (void) signal( SIGCHLD, SIG_IGN );
    //	sleep(10);
    LIBXML_TEST_VERSION
    if (getenv("NS_DEBUG_CHILD"))
    sleep(10);

    free_unique_range_var();
    child_init();

/*  #ifdef ENABLE_SSL
    // Initialize the SSL stuff 
    ssl_main_init();
  #endif */

  #ifdef NS_USE_MODEM
    if (ns_cavmodem_init() == -1)
      end_test_run();
  #endif

  if (ns_epoll_init(&v_epoll_fd) == -1)
    end_test_run();

   #ifdef CAV_MAIN
   // Creating a listerner port and add fd to epoll for communication with SM threads
   sm_create_nvm_listener();
   #endif

  /*Before Tue Nov 16 15:36:02 IST 2010 we are filling NULL in pfd.data.ptr*/ 
  //add_select_msg_com_con((char*)&g_child_msg_com_con, parent_fd, EPOLLIN);
  ADD_SELECT_MSG_COM_CON((char*)&g_child_msg_com_con, parent_fd, EPOLLIN, CONTROL_MODE);
  //add_select_msg_com_con((char*)&g_dh_child_msg_com_con, parent_dh_fd, EPOLLIN);
#ifndef CAV_MAIN
  ADD_SELECT_MSG_COM_CON((char*)&g_dh_child_msg_com_con, parent_dh_fd, EPOLLIN, DATA_MODE);
#endif
  /*For Event Logger*/
  if(global_settings->enable_event_logger)
     add_select_msg_com_con((char*)&g_el_subproc_msg_com_con, event_logger_fd , EPOLLIN | EPOLLERR | EPOLLHUP);

  check_and_create_nvm_listner();

  // Check for java type sript and init njvm
  check_and_init_njvm();


  //
  if(global_settings->use_java_obj_mgr){
    init_java_obj_mgr_con(vptr, global_settings->java_object_mgr_port);
  } 

  ns_fd_init(&ns_fdset, NS_EPOLL_MAXFD);
#ifndef CAV_MAIN
  weib_rangen = alloc_weib_gen(v_port_entry->pid);
  exp_rangen = alloc_exp_gen(v_port_entry->pid);
#endif
  child_init_src_ip();

  if(global_settings->high_perf_mode)
    init_sock_list(v_epoll_fd, pfds, NS_EPOLL_MAXFD);

  (void) signal( SIGPIPE, handle_child_sigpipe );
  (void) signal( SIGTERM, handle_child_sigterm );
  (void) signal( SIGINT, handle_child_sigint );
  (void) signal( SIGRTMIN, handle_child_sigrtmin);
  (void) signal( SIGRTMIN+1, handle_child_sigrtmin1);

  send_msg_from_nvm_to_parent(START_MSG_BY_CLIENT, CONTROL_MODE);
#ifndef CAV_MAIN
  send_msg_from_nvm_to_parent(START_MSG_BY_CLIENT_DATA, DATA_MODE);
 
  NSDL2_MESSAGES(NULL, NULL, "num_users = %d", v_port_entry->num_vusers);
#endif

  /** NetStorm run JMeter ASIS, there is an assumption that ONE SGRP can have only
      one JMeter instance. 
      Allocating memory of structure Msg_com_con to store information about connect between NVM and JMeter (CavJMAgent)
      for each SGRP */
  if (g_script_or_url == JMETER_TYPE_SCRIPT)
  {
    jmeter_init();
  }

  //Set Down VUser Stats
  for(ii = 0; ii< total_runprof_entries; ii++)
  {
    gVUserSummaryTable[ii].num_down_vuser = per_proc_runprof_table[my_port_index * total_runprof_entries + ii];
    NSDL2_MESSAGES(NULL, NULL, "totol user = %d, num_vuser[%d] = %d", 
                                v_port_entry->num_vusers, ii, gVUserSummaryTable[ii].num_down_vuser);

    if(g_script_or_url == JMETER_TYPE_SCRIPT)
      jmeter_per_grp_init(ii);
  }

  int epvmin, epvmax, epvavg, epvcount;
  u_ns_ts_t epvst = get_ms_stamp(); 
  u_ns_ts_t epvet = epvst;  
  u_ns_ts_t epvpt = 0; 
  
  epvmax = epvavg = epvcount = 0;
  epvmin = 0x7FFFFFFF;

  //--ANIL
  /* Main loop. */

  //start_sbrk = (unsigned int)sbrk(0);
  //printf ("sbrk at start = 0x%x\n", start_sbrk);
  //i = 0;
  for (;;) 
  {
    now = get_ms_stamp();
    if(runtime_change_done_by_parent == RUNTIME_CHANGE_STARTED) {
      NSDL2_MESSAGES(NULL, NULL, "Run time changes started by parent. Pausing for parent to complete");
      /*Issue - RTG hang because nvm's remains in pause state.
        Reason - Sometimes on runtime change parent works so fast & nvm's works so slow that parent sends the resume signal to nvm's before nvm's goes in pause state. After this when nvm goes in pause state, it never resumes because parent has already sent resume signal*/   
      NSTL1(NULL, NULL, "Applying alarm for %d secs to break pause() through alarm timeout",
                         global_settings->rtc_system_pause_timeout_val);

      ns_pause(now);
    }
    NSDL2_MESSAGES(NULL, NULL, "sigterm_received = %d, end_test_run_mode = %d", sigterm_received, end_test_run_mode);
    #ifndef CAV_MAIN
    if (sigterm_received) 
      finish(now);
    else
     now = ramp_up_users(now);
    #endif

    // Must be called before calculate_child_epoll_timeout as this may be setting timer
    vut_execute(now);
    if(vut_task_count == 0)
      temp_ms = calculate_child_epoll_timeout(&call_timers, &epoll_timeout_zero_cnt, now);
    else 
    {
      NSDL2_POLL(NULL, NULL, "Vuser tasks are in queue, so setting epoll timeout to 0");
      call_timers = 1;
      temp_ms = 0;
    }

    r = epoll_wait(v_epoll_fd, pfds, NS_EPOLL_MAXFD, temp_ms);

    NSDL4_POLL(NULL, NULL, "epoll r = %d, gRunPhase = %d", r, gRunPhase);
    if ( r < 0 ) 
    {
      if (errno != EINTR)
        perror( "netstorm_child: select/epoll_wait" );
      continue;
    }

    ns_fd_zero(&ns_fdset);

    //3.9.2 Build#25 updating now in case if no event caught (to apply other timers callback).
    /*if(r == 0)
      now = get_ms_stamp();*/

    event_proc_st = get_ms_stamp();
 
    for (ii = 0; ii < r; ii++) {
      //  3.9.2 Build#25 – Added code to reset now as req can take time specifically if DNS lookup is done …
      //  so next request start time need to change accordingly
      now = get_ms_stamp();
      cptr = NULL;  // Make it NULL so that checking for cptr is easy 
      fd = -1;
      event_ptr = pfds[ii].data.ptr;
      events = pfds[ii].events;
  
      if(event_ptr == NULL) { // event_ptr can not be NULL
        NSEL_CRI(NULL, NULL, ERROR_ID, ERROR_ATTR,
		 "Ignoring, as got event data ptr NULL while it should not."
		 " ii = %d, events = %d",
		 ii, events);
        continue;
      }

      NSDL1_POLL(NULL, NULL, "[EPOLL] epoll return type = %d, event_ptr = %p, events = %s", 
                              *(char *)event_ptr, event_ptr, 
                               (events & EPOLLOUT)?"EPOLLOUT":((events & EPOLLIN)?"EPOLLIN":"EPOLL Others Event"));

      switch(*(char *)event_ptr)
      {
        case NS_STRUCT_TYPE_NVM_PARENT_COM:
          NSDL1_POLL(NULL, NULL, "cptr = %p, Handling parent/child control communication.", cptr);
          handle_msg_from_parent(pfds, event_ptr, ii, now);
          continue;

        case NS_STRUCT_TYPE_NVM_PARENT_DATA_COM:
          NSDL1_POLL(NULL, NULL, "cptr = %p, Handling parent/child data communication.", cptr);
          handle_msg_from_parent_dh(pfds, event_ptr, ii, now);
          continue;

        case NS_STRUCT_TYPE_LOG_MGR_COM:
          NSDL1_POLL(NULL, NULL, "cptr = %p, Handling nsa_log_mgr.", cptr);
          handle_nsa_log_mgr(pfds, event_ptr, ii, now);
          continue;

        case NS_STRUCT_TYPE_CPTR:
          NSDL1_POLL(NULL, NULL, "cptr = %p, Handling connection cptr.", cptr);
          if(global_settings->high_perf_mode) {
            { 
              cptr = (connection *) ((SOCK_data *)event_ptr)->cptr;
              if (!cptr) { // cptr is NULL. This means fd is not associated with the connection.
                num_set_select++;
                continue;
              }
            }
          } else { // Non high perf mode
            cptr = (connection *) event_ptr;
            fd = cptr->conn_fd;
          }
          break;
        case NS_STRUCT_TYPE_VUSER_THREAD_LISTEN:
          NSDL2_MESSAGES(NULL, NULL, "Event find on listen fd %d", ((Msg_com_con *) event_ptr)->fd);
          
          vutd_accept_connetion( ((Msg_com_con *) event_ptr)->fd);
          continue;
        case NS_STRUCT_TYPE_SM_THREAD_LISTEN:
           accept_connection_from_sm_thread(((Msg_com_con *) event_ptr)->fd); 
           continue;
        case NS_STRUCT_TYPE_VUSER_THREAD_COM:
          /* validate if the mode is multi thread else log error and continue*/
          handle_msg_from_vuser_thread((Msg_com_con *)event_ptr, NULL);
          continue;
        case NS_STRUCT_TYPE_SM_THREAD_COM:
          handle_msg_from_sm_thread((Msg_com_con *)event_ptr);
          continue;
        case NS_STRUCT_TYPE_NJVM_LISTEN:
          // if got connection on njvm listen fd, it will be thread connection 
          njvm_accept_thrd_con(((Msg_com_con *) event_ptr)->fd, 1);
          continue;
        case NS_STRUCT_TYPE_NJVM_THREAD:
          // handle msg on thread connections
          handle_msg_from_njvm((Msg_com_con *)event_ptr, events);
          continue;
        case NS_STRUCT_TYPE_NJVM_CONTROL:
          // Got a message on njvm control connection
          handle_msg_from_njvm((Msg_com_con *)event_ptr, events);
          continue;
        case NS_JMETER_DATA_CONN:
          NSDL1_POLL(NULL, NULL, "Inside JMeter case, ((Msg_com_con *) event_ptr)->fd = %d, write_offset = %d", 
                                                      ((Msg_com_con *) event_ptr)->fd, ((Msg_com_con *) event_ptr)->write_offset);
          if(((Msg_com_con *) event_ptr)->fd == ((Msg_com_con *) event_ptr)->write_offset)
          {
            NSDL1_POLL(NULL, NULL, "((Msg_com_con *) event_ptr)->fd = %d", ((Msg_com_con *) event_ptr)->fd);
            accept_fd = accept(((Msg_com_con *) event_ptr)->fd, NULL, 0);
            if(accept_fd < 0)
            {
	      if(errno == EAGAIN)
      	      {
                NSDL2_MISC(NULL, NULL, "accept EAGAIN, err = %s, accept_fd = %d", nslb_strerror(errno), accept_fd);
                continue;
              }
              else
              {
                NSTL1_OUT(NULL, NULL, "Error in accepting connection. Error = %s\n", nslb_strerror(errno));
                end_test_run();
              }
            }
            fcntl(accept_fd, F_SETFL, O_NONBLOCK);
            //adding accept fd to epoll for getting data
            ((Msg_com_con *) event_ptr)->fd = accept_fd;
            add_socket_fd(v_epoll_fd, (char*)event_ptr, accept_fd, EPOLLIN|EPOLLERR|EPOLLHUP);
            NSTL1(NULL, NULL, "Connection accepted from JMeter for nvm %d. fd = %d", my_port_index, accept_fd);
         }   
          collect_data_from_jmeter_listener(((Msg_com_con *) event_ptr)->fd, pfds, (Msg_com_con *)event_ptr);
          continue;
        default:
          continue;          
      } 
     
      VUser *vptr = cptr->vptr;
      TLS_SET_VPTR(vptr);

      NSDL1_POLL(NULL, NULL, "[EPOLL] ab_select active: cptr=%p, fd=%d, state=%d",
		 cptr, fd, cptr->conn_state);
      
      switch ( cptr->conn_state ) 
      {
        case CNST_CONNECTING:
          num_reset_select++; /*TST */
          if (cptr->request_type == USER_SOCKET_REQUEST) {
            if (events & EPOLLOUT) {
              cptr->header_state = events;    //using unused field to pass events to user
              switch_to_vuser_ctx(vptr, "connect event on user socket-switching to user context");
            } else {
              NSDL4_DNS(NULL, cptr, "got events 0x%x in CNST_CONNECTING for user socket");
              continue; 
            }

          } else 
            handle_connect( cptr, now, 1 );
          break;

        case CNST_WRITING:
        if (runprof_table_shr_mem[vptr->group_num].gset.enable_pipelining &&
              (cptr->request_type == HTTP_REQUEST ||
               cptr->request_type == HTTPS_REQUEST)) {
            if ((events & EPOLLOUT) == EPOLLOUT) {
              if(IS_REPLAY_TO_USE_REPLAY_CODE())
                send_replay_req_after_partial_write(cptr, now);
              else{
                if(IS_HTTP2_INLINE_REQUEST){
                  h2_handle_incomplete_write(cptr, now); //bug 79062 method renamed
                } else  
                  handle_write(cptr, now);
                }
              }
            if ((events & EPOLLIN) == EPOLLIN) {
              handle_read( cptr, now );
            }
          } else if (cptr->request_type == USER_SOCKET_REQUEST) {
            if (events & EPOLLOUT) {
              cptr->header_state = events;    //using unused field to pass events to user
              switch_to_vuser_ctx(vptr, "user socket: ready for write -switching to user context");
            } else {
              NSDL4_DNS(NULL, cptr, "got events 0x%x in CNST_WRITING for user socket");
              continue; 
            } 

          } else {
            NSDL2_WS(NULL, cptr, "Handle incomplete write  for protocol - request type = %d", cptr->request_type);
            if(IS_REPLAY_TO_USE_REPLAY_CODE()) 
              send_replay_req_after_partial_write(cptr, now);
            else {
              if(IS_HTTP2_INLINE_REQUEST){
                h2_handle_incomplete_write(cptr, now); /* bug 79062 method renamed*/
              } else  
                handle_write(cptr, now);
              }
          }

        break;

        case CNST_WS_FRAME_WRITING:
          {
            NSDL4_DNS(NULL, cptr, "Handle incomplete write  for protocol - request type = %d", cptr->request_type);
            handle_send_ws_frame(cptr, now); /* this is fine for SMTP also */
          }
        break;
        // This state will be set in case of HTTP2 only for following: connection setup, response reading   
        case CNST_HTTP2_WRITING:
         // Check for write event in case of connection setup only, request will be written by using HTTP code
          NSDL4_HTTP2(NULL, cptr, "cptr->proto_state = %d, events = %x [event 5 is READ|WRITE & 4 is WRITE ONLY]", cptr->http2_state, events);
          if ((events & EPOLLOUT) == EPOLLOUT)
	    h2_handle_non_ssl_write(cptr, now);
        break;
        /*bug 51330: for reading WINDOW_UPDATE event*/
        case CNST_HTTP2_READING:
         NSDL4_HTTP2(NULL, cptr, "cptr->proto_state = %d, events = %x [event 5 is READ|WRITE & 1 is READ ONLY]", cptr->http2_state, events);
         if ((events & EPOLLIN) == EPOLLIN)
           http2_handle_read(cptr, now);
         break;

        case CNST_FC2_WRITING:
        // Check for write event 
         if ((events & EPOLLOUT) == EPOLLOUT) {
           if (cptr->fc2_state == FC2_SEND_HANDSHAKE || cptr->fc2_state == FC2_SEND_MESSAGE)
             fc2_handle_write(cptr, now);
         }
         NSDL4_HTTP2(NULL, cptr, "cptr->proto_state = %d, events = %x", cptr->fc2_state, events);
         // Check for read event  
         if ((events & EPOLLIN) == EPOLLIN) {
           if (cptr->fc2_state == FC2_RECEIVE_HANDSHAKE || cptr->fc2_state == FC2_RECEIVE_MESSAGE)
             fc2_handle_read(cptr, now);
         }
         break;
        
      #ifdef ENABLE_SSL
        case CNST_SSL_WRITING:
                NSDL4_HTTP(NULL, cptr, "WSS: ws_ssl_ret = %d cptr->http_protocol=%d cptr->http2=%p", ws_ssl_ret,
													cptr->http_protocol, cptr->http2);
          //if (runprof_table_shr_mem[((VUser*)cptr->vptr)->group_num].gset.enable_pipelining &&
          if (runprof_table_shr_mem[vptr->group_num].gset.enable_pipelining &&
              (cptr->request_type == HTTP_REQUEST ||
               cptr->request_type == HTTPS_REQUEST)) {
          
            if ((events & EPOLLOUT) == EPOLLOUT) {
              if(IS_REPLAY_TO_USE_REPLAY_CODE()) 
                send_replay_req_after_partial_write(cptr, now);
              else{
                /*bug 79062 : call  h2_handle_ssl_write_ex() inxase of HTTP2*/
                if((cptr->http_protocol == HTTP_MODE_HTTP2) && cptr->http2) {
                  h2_handle_ssl_write_ex(cptr, now);
                  if( (cptr->url_num) && (cptr->url_num->proto.http.type == EMBEDDED_URL)) {
                    HTTP2_SET_INLINE_QUEUE
                  }
                }
                else
                  handle_ssl_write( cptr, now );
                }

              }

           /* Handle Reading */
           if ((events & EPOLLIN) == EPOLLIN) {
             handle_read( cptr, now );
           }
          } else {
            if(IS_REPLAY_TO_USE_REPLAY_CODE())
              send_replay_req_after_partial_write(cptr, now);
            else{
              /*bug 79062: call  h2_handle_ssl_write_ex() inxase of HTTP2*/
              if((cptr->http_protocol == HTTP_MODE_HTTP2) && cptr->http2) {
                ws_ssl_ret = h2_handle_ssl_write_ex(cptr, now);
                if( (cptr->url_num) && (cptr->url_num->proto.http.type == EMBEDDED_URL)) {
                  HTTP2_SET_INLINE_QUEUE
                }
              }
              else
                ws_ssl_ret =  handle_ssl_write( cptr, now );
              //Handle Partial SSL write, if chunk is greater than 16KB than need to handle partial read.
              if (cptr->request_type == WSS_REQUEST)
              {
                if(ws_ssl_ret == WS_SSL_ERROR)
                {
                  NSDL4_HTTP(NULL, cptr, "WSS: send partial data for cptr = %p, conn_fd = %d", cptr, cptr->conn_fd);
                  ws_ssl_ret = -2; 
                }

                if(ws_ssl_ret && (ws_ssl_ret != WS_SSL_PARTIAL_WRITE)) //In case of ssl partial send we will not increase send fail counter
                {
                  INC_WS_MSG_SEND_FAIL_COUNTER(vptr);   //Updated avg counter for failed msg
                }

                if(ws_ssl_ret != WS_SSL_PARTIAL_WRITE)
                {
                  cptr->conn_state = CNST_WS_IDLE;
                  // switch to vuser ctx if partial ssl write is done 
                  switch_to_vuser_ctx(vptr, "SwitcToVUser: nsi_websocket_send(): Websocket send partial frame done.");
                }
              }
            }
          }
        break;
        case CNST_LDAP_WRITING:
          handle_ldap_write(cptr, now);
	  break;
        case CNST_JRMI_WRITING:
          handle_jrmi_write(cptr, now);
	  break;
        case CNST_SSLCONNECTING:
          num_reset_select++; /*TST */
          handle_connect( cptr, now, 1 );
        break;
      #endif
        case CNST_CHUNKED_READING:
        case CNST_HEADERS:
        case CNST_READING:
        case CNST_WS_READING:
        case CNST_WS_FRAME_READING:
        case CNST_WS_IDLE:
          //if (runprof_table_shr_mem[((VUser*)cptr->vptr)->group_num].gset.enable_pipelining &&
          if (runprof_table_shr_mem[vptr->group_num].gset.enable_pipelining &&
              (cptr->request_type == HTTP_REQUEST ||
               cptr->request_type == HTTPS_REQUEST)) {
            if ((events & EPOLLIN) == EPOLLIN) {

              NSDL4_DNS(NULL, cptr, "Got state = CNST_READING");
              handle_read( cptr, now );
            }

            if ((events & EPOLLOUT) == EPOLLOUT) {
              if(IS_REPLAY_TO_USE_REPLAY_CODE())
                send_replay_req_after_partial_write(cptr, now);
              else{
                if(IS_HTTP2_INLINE_REQUEST){
                  h2_handle_incomplete_write(cptr, now); /* bug 79062 method renamed */
                } else  
                  handle_write(cptr, now);
              }
            }
          } else {
              NSDL4_DNS(NULL, cptr, "Got state = %d, request_type = %d", cptr->conn_state, cptr->url_num->request_type);
              switch(cptr->url_num->request_type) {
                case HTTP_REQUEST:
                case HTTPS_REQUEST:
                case WS_REQUEST:
                case WSS_REQUEST:
                  handle_read(cptr, now);
                  break;
                case SMTP_REQUEST:
                case SMTPS_REQUEST:
                  handle_smtp_read(cptr, now);
                  break;
                case POP3_REQUEST:
                case SPOP3_REQUEST:
                  handle_pop3_read(cptr, now);
                  break;
                case FTP_REQUEST:
                  handle_ftp_read(cptr, now);
                  break;
                case FTP_DATA_REQUEST:
                  handle_ftp_data_read(cptr, now);
                  break;
                case DNS_REQUEST:
                  handle_dns_read(cptr, now);
                  // Below code is added to handle dns query done internaly through ns dns protocol in http
                  // So in case of http and other protocols dns query, after reading dns response we have to start http normal flow by 
                  // calling start_socket with linked cptr for further http requests.
                  if(cptr->conn_link && (cptr->flags & NS_CPTR_FLAGS_DNS_DONE)){
                    connection *http_cptr =  (connection*)(cptr->conn_link);  
                    http_cptr->conn_link = NULL;
                    free_connection_slot(cptr, now);  
                    start_socket(http_cptr, now);
                  }
                  break;
                case LDAP_REQUEST:
                case LDAPS_REQUEST:
                  handle_ldap_read(cptr, now);
                  break;
                case JRMI_REQUEST:
                  handle_jrmi_read(cptr, now);
                  break;
                case IMAP_REQUEST:
                case IMAPS_REQUEST:
                  handle_imap_read(cptr, now);
                  break;
                case XMPP_REQUEST:
                case XMPPS_REQUEST:
                  xmpp_read(cptr, now);
                  break;
                case SOCKET_REQUEST:
                case SSL_SOCKET_REQUEST:
                  if((events & EPOLLIN) == EPOLLIN)
                  {
                    NSDL4_HTTP(vptr, cptr, "[EPOLL] SocketAPI event EPOLLIN get, read data from socket");
                    handle_recv(cptr, now);
                  }
                  else
                  {
                    NSDL4_HTTP(vptr, cptr, "[EPOLL] SocketAPI event %s get, just ignore as cptr state is reading.", 
                               ((events & EPOLLIN) == EPOLLOUT)?"EPOLLOUT":"Epoll Other Event");
                  }
                  break;
                case  USER_SOCKET_REQUEST:
                  cptr->header_state = events;    //using unused field to pass events to user
                  if (events & EPOLLIN) {
                    switch_to_vuser_ctx(vptr, "user socket: ready for read -switching to user context");
                  } else {
                    NSDL4_DNS(NULL, cptr, "got events 0x%x in CNST_READING for user socket");
                    continue; 
                  }
                  break;
                default:
                  NSTL1_OUT(NULL, NULL, "%s: Invalid Request type (%d)\n", 
                                   (char *)__FUNCTION__,
                                   cptr->url_num->request_type);
              }
          }
        break;
        /*case WS_FRAME_READING:
          ws_handle_frame_read(cptr, now);
        break;*/
        case CNST_IDLE:
        {
          NSDL4_HTTP(vptr, cptr, "[EPOLL] cptr state is CNST_IDLE");
        }
        break;
        case CNST_REQ_READING:
          http_read_request(cptr, now);
          break;
        case CNST_LISTENING:
        {
          //This cptr is Data connection
          if(cptr->request_type == CPE_RFC_REQUEST)
            tr069_accept_connection(cptr, now); 
          else if (cptr->request_type == USER_SOCKET_REQUEST) {
            if (events & EPOLLIN) {
              cptr->header_state = events;    //using unused field to pass events to user
              switch_to_vuser_ctx(vptr, "accept ready event on user socket -switching to user context");
            } else {
              NSDL4_DNS(NULL, cptr, "got events 0x%x in CNST_LISTENING for user socket");
              continue; 
            }

          } else
            accept_connection(cptr, now);
        }
        break;
        case CNST_REUSE_CON:
        {
          NSDL4_CHILD(NULL, cptr, "cptr->request_type = %d, cptr = %p", cptr->request_type, cptr);
 
          /*
            NOTE: Don't use cptr->url_num as it is already freed by handle_page_complete, so it is pointing to the dangling ptr
                  that may cause memory corruption in case of accessing the request type using url_num.
                  Access the request_type using cptr only.
          */
          
          vptr = (VUser*)cptr->vptr;
          
          if(cptr->request_type == WS_REQUEST || cptr->request_type == WSS_REQUEST)
          {
            NSTL2(vptr, cptr, "WebSocket cptr %p, vptr %p, is in CNST_REUSE_CON, add cptr in inuse list and read data.", cptr, vptr);
            remove_head_glb_reuse_list(vptr);
            cptr->conn_state = CNST_WS_FRAME_READING;
            handle_read(cptr, now);
            break; 
          }

          /* In case of http2 when request was served from cache conn_state was set to CNST_REUSE_CON and hence we were not going to read
           for the next request*/        
          if ((cptr->request_type == HTTP_REQUEST || cptr->request_type == HTTPS_REQUEST) && (cptr->http_protocol== HTTP_MODE_HTTP2))
          {
            NSDL4_HTTP2(vptr, cptr, "HTTP2 cptr %p, vptr %p, is in CNST_REUSE_CON", cptr, vptr);

            if(cptr->http2 ) {  // && cptr->http2->total_open_streams){ bug 68086 - call read even if open streams = 0, in order 
                                    /* to read response from server even if there is any time out on stream */
              http2_handle_read(cptr, now);
              break;
            }            
            if(cptr->http2 && cptr->http2->donot_release_cptr) //do we need to reset it with each new stream
              break;
          }
          /* 
             No data is expected as cptr is in CNST_REUSE_CON state so if any epoll event is occured 
             and read failed as bytes_read is < 0 and error is EAGAIN then only recevied event will be ignored 
             else connection will be closed and cptr will be released. 
             As this data is not useful in all case so not reading the full data , just reading the one byte to check 
             the connection status.
             1. bytes_read is zero means connection is closed from remote , so close fd and release cptr
             2. bytes_read is greater than zero means remote has sent some data that should not be happened , so close fd and release cptr
             3. bytes_read is less then zero means some event received but read failed so ignore the event.
          */
          char buf[1];
          int bytes_read = 1;

          bytes_read = read( cptr->conn_fd, buf, bytes_read );
          if (( bytes_read < 0 ) && (errno == EAGAIN))
            break;

          NSDL4_DNS(NULL, cptr, "CNST_REUSE_CON");
          close_fd_and_release_cptr(cptr, NS_FD_CLOSE_REMOVE_RESP, now);
          break;
	}

	default: /* case CNST_FREE */
	  if (fd != -1) { /* Added check as we are making fd -1 in close_fd - Oct 16 2008 */          
            printf("unknown socket active: cptr=%p fd=%d state=%d\n", cptr, fd, cptr->conn_state);
            if (vptr->vuser_state != NS_VUSER_CLEANUP)   
              {
                NSDL4_DNS(NULL, cptr, "CNST_FREE");
                close_fd_and_release_cptr(cptr, NS_FD_CLOSE_REMOVE_RESP, now);
              }
          }
	}
       
        /* Devendar: 29/08/2019
           Sometime events processing take more time that cause problem in RTC and Progress Report.
           -> RTC message stay in socket queue for longer time more than configured timeout and RTC 
              failed with error epoll timeout. The same thing may happen with Progress Report ACK message also.
           -> Progress Report or RTC reply has to wait for longer timer for write event as many other
              events are coming on epoll that may cause delay in progress report delivery or RTC reply.
           -> It may cause the delayed expiry of progress report timer
           So All RTC and Progress Report processeing should be prioritize to handle or deliver the 
           RTC and Progress Report on time.
           ->All RTC communication done through the Control Connection (g_child_msg_com_con) and all 
           Progress Report communication done through the Data Connection (g_dh_child_msg_com_con).
           ->To prioritize the above mention communication, it is required to read and write the data on 
           both connection for every second after epoll_wait() so in case multiple event received 
           then after processing of every event, check event processing time to tigger the force "read", "write" 
           and "progress report interval".
        */
        /* Manish: Check event processing takes more than 1 sec or not if yes then read/write 
                   RTC/ProgresReport message forcefully if pending */
        event_proc_et = get_ms_stamp();
        epvet = event_proc_et;
        if((event_proc_et - event_proc_st) >= 1000)
        {
          //Forcefully read message form Control Connection (i.e g_child_msg_com_con) if any.  
          epev[0].events = EPOLLIN;           
          handle_msg_from_parent(epev, &g_child_msg_com_con, 0, event_proc_et);

          //Forcefully write message on Control Connection (i.e g_child_msg_com_con) if any.  
          epev[0].events = EPOLLOUT;           
          handle_msg_from_parent(epev, &g_child_msg_com_con, 0, event_proc_et);
         

      #ifndef CAV_MAIN
          //Forcefully read message form Data Connection (i.e g_dh_child_msg_com_con) if any.  
          epev[0].events = EPOLLIN;           
          handle_msg_from_parent_dh(epev, &g_dh_child_msg_com_con, 0, event_proc_et);

          //Forcefully write message on Data Connection (i.e g_dh_child_msg_com_con) if any.  
          epev[0].events = EPOLLOUT;           
          handle_msg_from_parent_dh(epev, &g_dh_child_msg_com_con, 0, event_proc_et);

          //Check if progress report timer expire then send progress report
          dis_timer_run_ex(event_proc_et, AB_TIMEOUT_PROGRESS);
      #endif
          event_proc_st = event_proc_et;
        }

        epvcount++; 
        epvpt = epvet - now;  //Single event processing time
        if(epvmin > epvpt)  //Get min time taken by Single event processing
          epvmin = epvpt;
 
        if(epvmax < epvpt) //Get max time taken by singke event processing
          epvmax = epvpt;
        
        epvavg += epvpt;
 
        //NSTL1(NULL, NULL, "NNNNN:: min = %d, max = %d, epvcount = %d, epvst = %llu, epvet = %llu, epvpt = %llu", 
          //                epvmin, epvmax, epvcount, epvst, epvet, epvpt);
        if((epvet - epvst) >= 900000) //Check 15min
        {
          epvavg = epvavg/epvcount; 
          NSTL1(NULL, NULL, "NVM: EPOLL Stats(15min) -> min = %d, max = %d, avg = %d, count = %d, Total Event = %d, NumProcessedEvent = %d", 
                             epvmin, epvmax, epvavg, epvcount, r, ii);    
          epvmax = epvavg = epvcount = 0;
          epvmin = 0x7FFFFFFF;
          epvst = epvet;
        }
      }
      if(call_timers) {
        /* Manish: previously we were getting "now" form start of last event processing but in case 
                   if event processing takes more time then timers which expried during that 
                   interval will not be processed as "now" is old. Hence getting timestap again so that 
                   all timer expired by this time can be processed. */
        now = get_ms_stamp();
        /* And run the timers. */
        dis_timer_run(now);
      }
    } /* for(;;) */
}
