#include <stdio.h>
#include <sys/wait.h>
#include <sys/epoll.h>
#include <regex.h>
#include <libgen.h>
#include <v1/topolib_structures.h>
#include "nslb_big_buf.h"

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "nslb_util.h"
#include "smon.h"
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
#include "timing.h"
#include "tmr.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "ns_ftp.h"
#include "ns_ldap.h"
#include "ns_imap.h"
#include "ns_jrmi.h"
#include "ns_group_data.h"
#include "ns_ip_data.h"
#include "ns_rbu_page_stat.h"
#include "ns_page_based_stats.h"
#include "ns_msg_com_util.h" 
#include "output.h"
#include "nslb_sock.h"
#include "wait_forever.h"
#include "ns_gdf.h"
#include "ns_custom_monitor.h"
#include "ns_log.h"
#include "ns_parent.h"
#include "ns_user_monitor.h"
#include "ns_alloc.h"
#include "ns_percentile.h"
#include "ns_check_monitor.h"
#include "ns_event_log.h"
#include "ns_schedule_phases_parse.h"
#include "ns_schedule_pause_and_resume.h"
#include "ns_string.h"
#include "ns_event_log.h"
#include "ns_child_msg_com.h"
#include "ns_event_id.h"
#include "ns_global_dat.h"
#include "ns_http_cache_reporting.h"
#include "dos_attack/ns_dos_attack_reporting.h"
#include "ns_vuser_trace.h"
#include "ns_netstorm_diagnostics.h"
#include "netomni/src/core/ni_user_distribution.h"
#include "netomni/src/core/ni_scenario_distribution.h"
#include "ns_sync_point.h"
#include "ns_proxy_server_reporting.h"
#include "ns_network_cache_reporting.h"
#include "ns_lps.h"
#include "ns_dns_reporting.h"
#include "ns_trace_level.h"
#include "ns_server_admin_utils.h"
#include "deliver_report.h"
#include "ns_njvm.h"
#include "ns_comp_ctrl.h"
#include "ns_ndc.h"
#include "nia_fa_function.h"
#include "ns_runtime_changes_monitor.h"
#include "ns_dynamic_vector_monitor.h"
#include "ns_pre_test_check.h"
#include "ns_runtime_changes_quantity.h"
#include "ns_static_vars_rtc.h"
#include "ns_trans_parse.h"
#include "ns_runtime_changes.h"
#include "ns_runtime_runlogic_progress.h"
#include "ns_ndc_outbound.h"
#include "ns_auto_scale.h"
#include "ns_trans.h"
#include "ns_server_ip_data.h"
#include "ns_dynamic_avg_time.h"
#include "ns_trans_normalization.h"
#include "ns_websocket_reporting.h"
#include "ns_progress_report.h"
#include "ns_exit.h"
#include "nslb_dashboard_alert.h"
#include "ns_replay_db_query.h"
#include "ns_rbu_domain_stat.h"
#include "nslb_mon_registration_util.h"
#include "nslb_mon_registration_con.h"
#include "ns_appliance_health_monitor.h"
#include "ns_tier_group.h"
#include "ns_handle_alert.h"
#include "ns_vuser_runtime_control.h"
#include "ns_test_init_stat.h"
#include "nslb_cav_conf.h"
#include "ns_monitor_profiles.h" 
#include "ns_runtime.h"
#include "nslb_encode.h"
#include "ns_error_msg.h"
#include "ns_tsdb.h"
#include "ns_monitor_init.h"

unsigned long total_rx_pr = 0; //total progress report received
unsigned long total_tx_pr = 0; //total progress report sent

//Msg_com_con *g_dh_msg_com_con = NULL;
Msg_com_con *g_dh_msg_com_con_nvm0 = NULL;
Msg_com_con *g_dh_master_msg_com_con = NULL; // For Master <-> thread message communication connection data(Parent maintains it)
//Msg_com_con *g_listen_dh_msg_com_con;
//static int num_data_handler_msg_com_con = 0;

//int num_pge; /* numprogress expected */
//int num_connected;
u_ns_ts_t start_msg_ts;
static struct timeval timeout, kill_timeout;
avgtime **g_end_avg=NULL;
cavgtime **g_next_finished;
cavgtime **g_cur_finished;
cavgtime **g_dest_cavg;
parent_msg *msg_dh = NULL;
u_ns_ts_t g_next_partition_switch_time_stamp = 0;
unsigned int gen_delayed_samples = 0;

unsigned int cur_sample =1;
static cavgtime* c_end_results = NULL;
avgtime **g_cur_avg = NULL, **g_next_avg = NULL, **g_dest_avg = NULL;
static avgtime *amsg = NULL;
static int rcv_amt;
int dh_master_fd=-1;
//Msg_com_con *dh_listen_msg_com_con;

int *g_last_pr_sample; 

extern void set_idx_for_partition_switch(ClientData cd, u_ns_ts_t now);
extern void process_change_tr_msg_frm_parent(HourlyMonitoringSess_msg* tr_msg);
extern Msg_com_con g_el_subproc_msg_com_con;
extern int event_logger_dh_fd;
int g_progress_delay_read;
static int read_and_proc_msg_from_nvm_dh(Msg_com_con *mccptr, struct epoll_event *epev, int i, int num_children, avgtime *local_end_avg);

void kill_children(void) {
  int i;
  int state;
  char process_descrp[64];

  NSDL2_PARENT(NULL, NULL, "Method called");
  state = ns_parent_state;
  ns_parent_state = NS_PARENT_ST_TEST_OVER; //Ignore any SIGCHLD signal
        NSDL3_MESSAGES(NULL, NULL, "Setting parent test run status = %d", ns_parent_state);
  if(loader_opcode == MASTER_LOADER)  // Called from master
  {
    send_msg_to_all_clients(END_TEST_RUN_MESSAGE, 0);
    return;
  }
  for (i = 0; i<global_settings->num_process;i++) {
    sprintf(process_descrp, "child process [%d]", i);
    nslb_kill_and_wait_for_pid_ex(v_port_table[i].pid, process_descrp, 1, 5);
  }
  ns_parent_state = state;
}

/* Update progress report variables and total number of generators, 
 * because a generator got killed and we wont be receiving any samples from this generator*/
static inline int update_vars_to_continue_test(int killed_nvm_id)
{
  NSDL1_MESSAGES(NULL, NULL, "Method called, num_active = %d, num_pge = %d", g_data_control_var.num_active,
                             g_data_control_var.num_pge);
   
  if(g_data_control_var.num_active)
  {
    decrease_sample_count(killed_nvm_id, 0);
    //Reset all_nvms_sent fparam rtc counters
    //incrementing number of killed generators
    UPDATE_GLOB_DATA_CONTROL_VAR(g_data_control_var.num_active--; g_data_control_var.num_pge--; g_data_control_var.total_killed_nvms++)
    if (global_settings->num_process == g_data_control_var.total_killed_nvms)
    {
      NSTL1(NULL, NULL, "All NVMs got failed. Hence stopping the test...\n");
      kill_all_children((char *)__FUNCTION__, __LINE__, __FILE__);
      parent_save_data_before_end();
      save_status_of_test_run(TEST_RUN_STOPPED_DUE_TO_SYSTEM_ERRORS);
      NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_WARNING,
                  __FILE__, (char*)__FUNCTION__,
                 "All NVMs got failed. Hence stopping the test...\n");
      NS_EXIT(-1, "All NVMs got failed. Hence stopping the test...");
    }

    NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_WARNING,
           __FILE__, (char*)__FUNCTION__,
           "Received test termination event from NVM = %d, Test will continue...", killed_nvm_id);
  }
  NSTL1(NULL, NULL, "Modified progress report variables: num_active = %d, num_pge = %d, killed nvms = %d",
                    g_data_control_var.num_active, g_data_control_var.num_pge, g_data_control_var.total_killed_nvms);
  HANDLE_QUEUE_COMPLETE();
  return 0;
}

void kill_all_children(char *function_name, int line_num, char *file_name) {
  int i;
  char process_descrp[512];
  
  NSDL2_PARENT(NULL, NULL, "Method called, my_port_index = %d", my_port_index);
  if (my_port_index != 255) // To safe guard calling for this function from children
    return;
 
  NSDL3_MESSAGES(NULL, NULL, "Method called.");
  printf("%s: %s() called kill_all_children() from line number %d\n", file_name, function_name, line_num);

  nethavoc_cleanup();

  if(loader_opcode == MASTER_LOADER)  // Called from master
  {
    send_msg_to_all_clients(END_TEST_RUN_MESSAGE, 1);
    if(nia_file_aggregator_pid > 0)
    {
      NSTL1(NULL, NULL,"Sending SIGTERM to NIA File Aggregator (pid = %d)", nia_file_aggregator_pid);
      sprintf(process_descrp, "nia_file_aggregator process");
      nslb_kill_and_wait_for_pid_ex(nia_file_aggregator_pid, process_descrp, 1, 10);
    }
    return;
  }
 
  else if (loader_opcode == CLIENT_LOADER)
  {
     if ((g_master_msg_com_con != NULL) && (g_master_msg_com_con->fd >= 0))
     {
       complete_leftover_write(g_master_msg_com_con, CONTROL_MODE);
       if ((g_dh_master_msg_com_con != NULL) && (g_dh_master_msg_com_con->fd >= 0))
       {
         complete_leftover_write(g_dh_master_msg_com_con, DATA_MODE);
         send_msg_to_master(g_dh_master_msg_com_con->fd, END_TEST_RUN_MESSAGE, DATA_MODE);
         if (g_dh_master_msg_com_con->fd)
           complete_leftover_write(g_dh_master_msg_com_con, DATA_MODE);
       }
     }
  }

  ns_parent_state = NS_PARENT_ST_TEST_OVER;
  NSDL3_MESSAGES(NULL, NULL, "Setting parent test run status = %d", ns_parent_state);
#ifndef NS_PROFILE
  for (i = 0; i<global_settings->num_process;i++) {
    NSDL3_MESSAGES(NULL, NULL, "child_id = %d, child pid = %d", i, v_port_table[i].pid);
    sprintf(process_descrp, "child process [%d]", i);
    if (v_port_table[i].pid > 0)
      nslb_kill_and_wait_for_pid_ex(v_port_table[i].pid, process_descrp, 1, 5);
  }
  if (writer_pid > 0) {
    NSDL3_MESSAGES(NULL, NULL, "writer pid = %d", writer_pid);
    sprintf(process_descrp, "writer process");
    nslb_kill_and_wait_for_pid_ex(writer_pid, process_descrp, 1, 10);
  }
  if (cpu_mon_pid) {
    NSDL3_MESSAGES(NULL, NULL, "mon pid = %d", cpu_mon_pid);
    sprintf(process_descrp, "monitor process");
    nslb_kill_and_wait_for_pid_ex(cpu_mon_pid, process_descrp, 1, 10);
  }

  if(nsa_log_mgr_pid > 0) {
    NSDL3_MESSAGES(NULL, NULL, "nsa_log_mgr_pid = %d", nsa_log_mgr_pid);
    sprintf(process_descrp, "nsa log manager");
    nslb_kill_and_wait_for_pid_ex(nsa_log_mgr_pid, process_descrp, 1, 5);
  }
 
  //bug 53966: req rep uploader should be killed when generator/Parent is getting exit
  if(nia_req_rep_uploader_pid > 0) {
    NSDL3_MESSAGES(NULL, NULL, "nia_req_rep_uploader_pid = %d", nia_req_rep_uploader_pid);
    sprintf(process_descrp, "nia_req_rep_uploader");
    nslb_kill_and_wait_for_pid_ex(nia_req_rep_uploader_pid, process_descrp, 1, 5);
  }
#endif
  NSDL3_MESSAGES(NULL, NULL, "Exiting method"); 
}
  //bug71656:req to solve retry problem after generator's test is stopped
int search_ip_in_svr_list(char *gen_ip,int *tier_idx)
{
  int i;
  int id=topolib_get_tier_id_from_tier_name("Cavisson",topo_idx);
  tier_idx=&id;
  for(i = 0; i < topo_info[topo_idx].topo_tier_info[*tier_idx].total_server; i++)
  {
      if((topo_info[topo_idx].topo_tier_info[*tier_idx].topo_server[i].used_row != -1) && (strcmp(topo_info[topo_idx].topo_tier_info[*tier_idx].topo_server[i].server_ptr->server_name, gen_ip) == 0))
        return -1;
  
      else
        return i;
   }
  return 0;
}

void handle_gen_alert_msg(char *alert_msg, int child_idx)
{
  User_trace msg;
  msg.opcode = GENERATOR_ALERT;
  msg.child_id = child_idx;
  msg.gen_rtc_idx = g_generator_idx;

  NSDL1_MESSAGES(NULL, NULL, "Method called, msg = %s", alert_msg);

  strcpy(msg.reply_msg, alert_msg);
  NSTL1(NULL, NULL, "Send alert msg '%s' to master.", alert_msg);
  msg.msg_len = sizeof(User_trace) - sizeof(int);
  write_msg(g_dh_master_msg_com_con, (char *)&msg, sizeof(User_trace), 1, DATA_MODE); // Write at event logger fd
}

static void handle_nvm_failure(Msg_com_con *mccptr, int fd)
{
  int ret;
  char alert_msg[ALERT_MSG_SIZE + 1];

  NSDL1_MESSAGES(NULL, NULL, "Method called, child = %d", mccptr->nvm_index);

  if ((ret = update_vars_to_continue_test (mccptr->nvm_index)) == -1) {
    return;
  }

  NSTL1(NULL, NULL, "NVM got killed. Test will continue and ignore failed NVM = %d, fd = %d,"
                    " Total failed NVM(s) = %d, Total NVM(s) = %d", mccptr->nvm_index, fd,
                    g_data_control_var.total_killed_nvms, global_settings->num_process);
  //TODO: NVM got killed here : reason missing
  if (loader_opcode == CLIENT_LOADER)
  {
     sprintf(alert_msg, "Cavisson Virtual Machine (CVM%d) exited unexpectedly of generator '%s' with IP '%s'. This will lead to lower "
                        "than expected load generation", (mccptr->nvm_index + 1), global_settings->event_generating_host, g_cavinfo.NSAdminIP);
  }
  else if(loader_opcode == STAND_ALONE)
  {
     sprintf(alert_msg, "Cavisson Virtual Machine (CVM%d) exited unexpectedly. This will lead to lower than expected load generation", 
                         (mccptr->nvm_index + 1));
  }
  ns_send_alert(ALERT_CRITICAL, alert_msg);
  NSDL1_MESSAGES(NULL, NULL, "Exiting method");
}

/* Function used to update progress report variable and count of killed generator
 * Here if number of generators are equal to total number of killed generator then 
 * we need to terminate test on controller
 * Otherwise test will continue and event would be log in event.log file*/
static inline avgtime* handle_generator_failure(Msg_com_con *mccptr, int fd)
{
  int ret;
  char alert_msg[ALERT_MSG_SIZE + 1];
  int gen_id = mccptr->nvm_index;
  int index;
  int tier_idx;
  NSDL1_MESSAGES(NULL, NULL, "Method called");

  //This is called when generators are marked down. 
  gen_conn_failure_monitor_handling(mccptr->fd);
  index = search_ip_in_svr_list(generator_entry[gen_id].IP,&tier_idx);
  if (index == -1)
  {
    NSTL1(NULL, NULL, "We didn't find the Gen ip %s in the serverlist", generator_entry[gen_id].IP);
  }
  else
    end_mon(index, END_MONITOR, -1, MON_STOP_ON_RECEIVE_ERROR,tier_idx );

  if ((ret = update_vars_to_continue_ctrl_test (gen_id)) == -1) {
    NSTL1(NULL, NULL, "Running in controller mode need to continue test and will ignore failed "
                      "generator ip = %s, total_killed_gen = %d", mccptr->ip, g_data_control_var.total_killed_gen);
    return NULL;
  }

  sprintf(alert_msg, "Lost connection with generator '%s' with IP '%s'. This will lead to lower than expected load generation", generator_entry[gen_id].gen_name, generator_entry[gen_id].IP);
  //snprintf(alert_msg, 1024, "Generator '%s', ip '%s' for %s got failed", generator_entry[gen_id].gen_name, generator_entry[gen_id].IP, g_test_or_session);
  ns_send_alert(ALERT_CRITICAL, alert_msg); 
  NSTL1(NULL, NULL, "Number of killed generator(s) = %d, total number of generator = %d",
                    g_data_control_var.total_killed_generator, global_settings->num_process); 

  return handle_all_finish_report_msg(g_end_avg[0]);
}

#define HANDLE_NVM_FAILURE()\
 NSDL2_PARENT(NULL, NULL, "loader_opcode = %d, nvm_fail_continue = %d, flags = %d, con_type = %d", loader_opcode, global_settings->nvm_fail_continue, mccptr->flags, mccptr->con_type);\
 if ((loader_opcode != MASTER_LOADER) && (global_settings->nvm_fail_continue == 1) && (mccptr->flags & NS_MSG_COM_CON_IS_CLOSED) && ((mccptr->con_type == NS_STRUCT_TYPE_NVM_PARENT_COM) || (mccptr->con_type == NS_STRUCT_TYPE_NVM_PARENT_DATA_COM))) \
 { \
   handle_nvm_failure(mccptr, local_fd); \
 } 
     
/* In case of controller mode, where CONTINUE_TEST_ON_GEN_FAILURE keyword is enable 
 * and mccptr->flags set by close_msg_com_con_and_exit then we need to 
 * call handle_generator_failure function which update progress report vars*/
#define HANDLE_GEN_FAILURE(val)\
 if ((loader_opcode == MASTER_LOADER) && (global_settings->con_test_gen_fail_setting.mode == 1) && (mccptr->flags & NS_MSG_COM_CON_IS_CLOSED) && (mccptr->con_type == NS_STRUCT_TYPE_CLIENT_TO_MASTER_COM)) \
 { \
   if(val == 1)  \
     local_end_avg = handle_generator_failure(mccptr, local_fd); \
   else \
     handle_generator_failure(mccptr, local_fd); \
 }

static inline avgtime* handle_all_finish_report_msg(avgtime *end_avg)
{
  //bug fixed: where one generator is killed and same time FINISH_REPORT msg come. so num_active never be zero due to killed generator.
  //Now we can handle it. All reports rcd
  if(!g_data_control_var.num_active)
  {
    NSDL3_MESSAGES(NULL, NULL, "Received FINISH_REPORT msg from all child/clients");
    NSTL1(NULL, NULL, "Received FINISH_REPORT msg_dh from all child/clients");
    HANDLE_QUEUE_COMPLETE(); //dump pending progress report if any
    /* Send if previous message was not sent completely to master */
    if (dh_master_fd >= 0)
      complete_leftover_write(g_dh_master_msg_com_con, DATA_MODE);
    end_avg->elapsed = cur_sample -1; // cur_sample has not been send yet
    end_avg->complete = 1;
    deliver_report(run_mode, dh_master_fd, g_end_avg, g_cur_finished, rfp, srfp);
    /* Send if previous message was not sent completely to master */
    if (dh_master_fd >= 0)
      complete_leftover_write(g_dh_master_msg_com_con, DATA_MODE);
    
    // clean up children
    //kill_children();
    //sleep(2);
    ns_process_cav_memory_map();   
    // close(udp_fd);
    if (rfp)
      fflush(rfp);
    return end_avg;
  }
  else
    return NULL;
}

static inline int process_finish_report_ack_msg(Msg_com_con *mccptr)
{
  NSTL1(NULL, NULL, "FINISH_REPORT_ACK: (Generator:%d <- Master)", mccptr->nvm_index);
  return 2;
}

// The finish_report msg for test run has been recived, processing finish_report msg
static inline avgtime* process_finish_report_msg( int num_children , Msg_com_con *mccptr)
{
  int gen_idx = -1;
  int child_id = mccptr->nvm_index;

  NSDL2_MESSAGES(NULL, NULL, "Method called");

  UPDATE_GLOB_DATA_CONTROL_VAR(g_data_control_var.num_active--)
  //RTC: In case of Standalone/NetCloud, pause done message need to be acknowledge by child/generator.
  //Therefore if either process sends FINISH_REPORT to its parent then decrement send message counter 
  if(loader_opcode == MASTER_LOADER)
  {
    NSDL2_MESSAGES(NULL, NULL, "Received Progress report for generator id %d, test run number = %d for data connection", 
                                child_id, msg_dh->top.internal.testidx);
    gen_idx = child_id;
    generator_entry[child_id].gen_flag |= RCVD_GEN_FINISH; 
  }

  NSTL1(NULL, NULL, "Received FINISH_REPORT msg_dh from the child_id = %d, num_active = %d, loader_opcode = %d, from ip = %s "
                    "for data connection", child_id, g_data_control_var.num_active, loader_opcode, mccptr->ip);

  amsg = (avgtime *)&(msg_dh->top.avg);
  
  /*Copy finished report: below function is a merged reporting
    of progress as well as finish report */
  COPY_FINISH_REPORT();
  handle_all_finish_report_msg(g_end_avg[0]);
  if(loader_opcode == MASTER_LOADER)
  {
    parent_child send_msg;
    send_msg.child_id = child_id;
    send_msg.opcode = FINISH_REPORT_ACK;
    send_msg.abs_ts = (time(NULL) * 1000);
    send_msg.msg_len = sizeof(parent_child) - sizeof(int);
    write_msg(mccptr, (char *)&send_msg, sizeof(send_msg), 1, DATA_MODE);
  }
  CLOSE_MSG_COM_CON(mccptr, DATA_MODE);

  return NULL;
}

/* This function will log a message on trace log*/
static inline void process_progress_report_ack_msg(Msg_com_con *mccptr)
{
  NSTL1(NULL, NULL, "PROGRESS_REPORT_ACK: (Generator:%d <- Master), SampleID = %d, ReceivedTime(ms) = %lu, "
                    "SampeBirthTime(ms) = %.0lf, CurSampleID = %d, MasterMaxSampleID = %d",
                     msg_dh->top.internal.child_id,
                     msg_dh->top.internal.ns_version, time(NULL)*1000, msg_dh->top.internal.abs_ts, cur_sample,
                     msg_dh->top.internal.gen_rtc_idx);
  /*If generator is behind controller by progress report queue size 
   then resync generator and move cur_sample to controller provided sample */
  if((msg_dh->top.internal.gen_rtc_idx > cur_sample) &&
     (msg_dh->top.internal.gen_rtc_idx - cur_sample) > global_settings->progress_report_max_queue_to_flush)
  {
    NSTL1(NULL, NULL, "PROGRESS_REPORT_DELAY: Resyncing generator CurSampleID from %d to %d at ReceivedTime(ms) = %.0lf"
                      " as it crossed the queue resynchronization threshold(%d)",
                      cur_sample, msg_dh->top.internal.gen_rtc_idx, msg_dh->top.internal.abs_ts,
                      global_settings->progress_report_max_queue_to_flush);

    /* Calculate how many sample need to be synchronised */
    gen_delayed_samples = msg_dh->top.internal.gen_rtc_idx - cur_sample;

    /* Forcefully delivered all pending Progress Samples till controller's sample id */ 
    HANDLE_FORCE_COMPLETE(msg_dh->top.internal.gen_rtc_idx, 0);
  }

  g_last_acked_sample = msg_dh->top.internal.ns_version;
}

/* calling for PROGRESS_REPORT opcode
   process progress report from child
 */
static unsigned int max_elapsed = 0; 
static inline void process_progress_report_msg(Msg_com_con *mccptr)
{
  char child_name[256+1];
  int gen_idx = -1;
  int num_rcd;
  int actual_delay;
  char alert_msg[ALERT_MSG_SIZE];
  char delay[0xff];
  char err_msg[1024] = "\0";

  amsg = (avgtime *)&(msg_dh->top.avg);
  u_ns_ts_t cur_time_stamp = time(NULL) * 1000; //in ms
  parent_child send_msg;
  
  //calculating time difference between nvm sending message & parent receiving it
  mon_update_times_data(&hm_times_data->progress_report_processing_delay, (cur_time_stamp - amsg->abs_ts));
  NSDL2_MESSAGES(NULL, NULL, "Method called, Received Progress report for generator id %d, test run number = %d, Report Number #%d "
                             "for data connection", amsg->child_id, amsg->testidx, amsg->elapsed);
  /* NetCloud: In case of controller we need to find generator test run number and find corresponding directory path to 
   * store generator data, rtgMessage.dat, TestRun.gdf*/

   actual_delay = (cur_time_stamp - amsg->abs_ts);
  
  if(loader_opcode == MASTER_LOADER)
  {
    gen_idx = amsg->child_id;
    g_last_pr_sample[amsg->child_id] = amsg->elapsed;
    snprintf(child_name, 256, "(Master <- Generator:%d), Name = %s, Ip = %s, TRNum = %d", 
                               gen_idx, generator_entry[gen_idx].gen_name, generator_entry[gen_idx].IP, amsg->testidx);
    //actual_delay = (cur_time_stamp - amsg->abs_ts - generator_entry[gen_idx].con_gen_com_diff);
  }
  else
  {
    //actual_delay = (cur_time_stamp - amsg->abs_ts);
    g_last_pr_sample[amsg->child_id] = amsg->elapsed;
    snprintf(child_name, 256, "(Parent <- NVMID:%d)", amsg->child_id);
  }

  num_rcd = get_sample_info(amsg->elapsed);
  num_rcd = (num_rcd == -1)?0:num_rcd;
 
  NSDL1_MESSAGES(NULL, NULL, "PROGRESS_REPORT: %s, SampleID = %d, IsLastSample = %d, ReceivedTime(ms) = %llu, "
                    "SampeBirthTime(ms) = %.0lf, CurSampleID = %d, NumSampleReceived = %d NumSampleExpected = %d, "
                    "(Network + Epoll) Delay(ms) = %d, gen_delayed_samples = %u, "
                    "EpollEventStats: [in,out,err,hup] = [%lu,%lu,%lu,%lu]",
                    child_name, amsg->elapsed, amsg->complete, cur_time_stamp, amsg->abs_ts, cur_sample,
                    num_rcd+1, g_data_control_var.num_pge, actual_delay, gen_delayed_samples, 
                    g_epollin_count[amsg->child_id+1], g_epollout_count[amsg->child_id+1],
                    g_epollerr_count[amsg->child_id+1], g_epollhup_count[amsg->child_id+1]);

  NSTL1(NULL, NULL, "PROGRESS_REPORT: %s, SampleID = %d, IsLastSample = %d, ReceivedTime(ms) = %llu, "
                    "SampeBirthTime(ms) = %.0lf, CurSampleID = %d, NumSampleReceived = %d NumSampleExpected = %d, "
                    "(Network + Epoll) Delay(ms) = %d, gen_delayed_samples = %u, "
                    "EpollEventStats: [in,out,err,hup] = [%lu,%lu,%lu,%lu], "
                    "TotalBytesSent = %ld, TotalBytesReceived = %ld, PRTotalReceivedBytes = %ld",
                    child_name, amsg->elapsed, amsg->complete, cur_time_stamp, amsg->abs_ts, cur_sample,
                    num_rcd+1, g_data_control_var.num_pge, actual_delay, gen_delayed_samples,
                    g_epollin_count[amsg->child_id+1], g_epollout_count[amsg->child_id+1],
                    g_epollerr_count[amsg->child_id+1], g_epollhup_count[amsg->child_id+1],
                    mccptr->total_bytes_sent, mccptr->total_bytes_recieved, total_rx_pr);
 

  /*Copy received progress report*/
  COPY_PROGRESS_REPORT();

  if(loader_opcode == MASTER_LOADER)
  {
    if(amsg->elapsed > max_elapsed) 
      max_elapsed = amsg->elapsed;

    generator_entry[gen_idx].last_prgrss_rpt_elapsed = amsg->elapsed;
    generator_entry[gen_idx].last_prgrss_rpt_time = cur_time_stamp;
    /*Saving elapsed in variable test_end_time_on_gen*/
    convert_to_hh_mm_ss((generator_entry[gen_idx].last_prgrss_rpt_elapsed * global_settings->progress_secs),
                        generator_entry[gen_idx].test_end_time_on_gen);
    

    /************************************************************ 
     * Send PROGRESS_REPORT_ACK to Generator:
     *   Message: 
          0    4    8    12   16       20  24 (Bytes)
          +----+----+----+----+--------+----+
          |MLen|Opt |ChID|SId |TStamp  |MSId|
          +----+----+----+----+--------+----+

          MLen = message length
          Opt  = message opcode
          ChID = Child Id
          SId  = Sample Id (of child)
          TStamp = Absolute time stamp
          MSId = Max Sample Id (on Controller) 
     ***********************************************************/
    /*since on write_msg failure we are freeing mccptr->read_buf on close connection
      then msg will be NULL, avgtime might be corrupted*/
    send_msg.child_id = amsg->child_id;
    send_msg.opcode = PROGRESS_REPORT_ACK;
    send_msg.ns_version = amsg->elapsed; //ns_version using to send the report number
    send_msg.abs_ts = (time(NULL) * 1000);
    send_msg.gen_rtc_idx = max_elapsed; /* gen_rtc_idx using to send cur sample to inform cur sample*/
    send_msg.msg_len = sizeof(parent_child) - sizeof(int);

    write_msg(&g_dh_msg_com_con[amsg->child_id], (char *)&send_msg, sizeof(send_msg), 1, DATA_MODE);

    /*********************************************************************
     * Handling Generator delay:
     *   - Save Max SampleID (among all the Generators) and Last SampleID
     *     of each Generators 
     *   - Compare if of any Generator is slow as 50 % Queue size then
     *          
     *********************************************************************/

    //Check Global flag if set then return, to avoid recursive call of read_and_proc_msg_from_nvm_dh()
    if(g_progress_delay_read)
      return;
 
    for(gen_idx = 0; gen_idx < global_settings->num_process; gen_idx++) 
    {
      //Bug:53998 print killed generator only once for each progress report
      if(generator_entry[gen_idx].flags & IS_GEN_INACTIVE)
      {
        if((amsg->elapsed == cur_sample) && !num_rcd)
        {
          if (generator_entry[gen_idx].gen_flag & RCVD_GEN_FINISH){
            NSTL1(NULL, NULL, "Testrun on Generator '%s' is already ended successfully for data connection. Hence Skipping",
                              generator_entry[gen_idx].gen_name);
          } else{
            NSTL1(NULL, NULL, "Generator '%s' is already removed for data connection. Hence ignoring", generator_entry[gen_idx].gen_name);  
          } 
        }
        continue;
      }
      

      NSDL3_MESSAGES(NULL, NULL, "Generator %d, cur_time_stamp = %llu, last_prgrss_rpt_time = %u, "
                                 "last_prgrss_rpt_elapsed = %u, max_elapsed = %u",
                                  gen_idx, cur_time_stamp, generator_entry[gen_idx].last_prgrss_rpt_time,
                                  generator_entry[gen_idx].last_prgrss_rpt_elapsed, max_elapsed);
/*
      Balram: Bug id:33266/33804
      Enhancement: In ns_trace.log it should be available that from which generators the delay is coming.
      mark_gen_delayed_rep_msg: Flag is set to 1 for which generator the delayed report message has been logged in trace log.
*/
      //This checks more than 50% of queue is full
      if((max_elapsed - generator_entry[gen_idx].last_prgrss_rpt_elapsed) > global_settings->progress_report_queue_size/2)
      {
        if(!generator_entry[gen_idx].mark_gen_delayed_rep_msg) //First time delay
        {
          char cmd[1024];

          generator_entry[gen_idx].mark_gen_delayed_rep_msg = 1; //Set to avoid taking output again and again

          /*1. Adding check for collecting log of generator delay because data collection should only come in picture when,
               ENABLE_NC_TCPDUMP keyword is enabled.
            2. AND execution should be as one generator after another so no multiple process start together */

          if(IS_LAST_TCPDUMP_DUR_ENDED && IS_ENABLE_NC_TCPDUMP(ALWAYS))
          {

            sprintf(cmd, "nohup %s/bin/nsi_collect_data_on_progress_delay -d %d -T %d -I %s -p %d -i %s -n %s -W %s -G %d &",
                         g_ns_wdir,
                         global_settings->nc_tcpdump_settings->tcpdump_duration, testidx, g_cavinfo.NSAdminIP, g_dh_listen_port,
                         generator_entry[gen_idx].IP, generator_entry[gen_idx].gen_name, generator_entry[gen_idx].work,
                         generator_entry[gen_idx].testidx);

            NSTL1(NULL, NULL, "GENERATOR_DELAY: Getting time delay from generator for data connection id = %d , name = %s, "
                              "ip = %s. Collecting data using cmd(In background) = %s,"
                              "TotalBytesSent = %ld, TotalBytesReceived = %ld, PRTotalReceivedBytes = %ld",
                              gen_idx, generator_entry[gen_idx].gen_name, generator_entry[gen_idx].IP, cmd,
                              mccptr->total_bytes_sent, mccptr->total_bytes_recieved, total_rx_pr);
          
            if(nslb_system(cmd,1,err_msg) != 0)
            {
              NSTL1(NULL, NULL, "Error in executing cmd = '%s'. %s", cmd, err_msg);
            }
          }
          else
          {
            NSTL1(NULL, NULL, "GENERATOR_DELAY: Getting time delay from generator for data connection id = %d , name = %s, "
                              "ip = %s. But ENABLE_NC_TCPDUMP Keyword is disabled so not collecting any data.",
                              gen_idx, generator_entry[gen_idx].gen_name, generator_entry[gen_idx].IP);
           }
          //Alert will be place here
          convert_to_hh_mm_ss(cur_time_stamp - generator_entry[gen_idx].last_prgrss_rpt_time, delay);
          //snprintf(alert_msg, ALERT_MSG_SIZE, "Not getting progress report from generator '%s', ip '%s', for '%s' seconds", 
          //                                    generator_entry[gen_idx].gen_name, generator_entry[gen_idx].IP, delay);
          snprintf(alert_msg, ALERT_MSG_SIZE, "No metric report  received from generator '%s' with IP '%s' for last <%u> samples", 
                                              generator_entry[gen_idx].gen_name, generator_entry[gen_idx].IP, (max_elapsed - generator_entry[gen_idx].last_prgrss_rpt_elapsed -1));
          ns_send_alert(ALERT_MAJOR, alert_msg);
        }

        //delay has been found by 50% of queue size samples from a generator
        //EPOLL_STATS
        NSTL1(NULL, NULL, "GENERATOR_DELAY: EpollEventStats Generator[%s]|epollin[%lu]|epollout[%lu]|epollerr[%lu]|"
                          "epollhup[%lu]|TotalBytesSent[%ld]|TotalBytesReceived[%ld]|PRTotalReceivedBytes[%ld]", 
                          generator_entry[gen_idx].gen_name, g_epollin_count[gen_idx+1], g_epollout_count[gen_idx+1], 
                          g_epollerr_count[gen_idx+1], g_epollhup_count[gen_idx+1],mccptr->total_bytes_sent,
                          mccptr->total_bytes_recieved, total_rx_pr);

        NSDL2_MESSAGES(NULL, NULL, "GENERATOR_DELAY: Dumping delayed progress report of generator %s for data connection", 
                                   generator_entry[gen_idx].gen_name);
        //Set Global flag 
        g_progress_delay_read = 1;  
        //Add to Epoll In g_dh_msg_com_con[gen_idx].fd
        ADD_SELECT_MSG_COM_CON((char *)&g_dh_msg_com_con[gen_idx], g_dh_msg_com_con[gen_idx].fd, EPOLLIN | EPOLLERR | EPOLLHUP, DATA_MODE);
        //Read Socket Fourcefully g_dh_msg_com_con[gen_idx].fd 
        read_and_proc_msg_from_nvm_dh(&g_dh_msg_com_con[gen_idx], NULL, -1, -1, NULL);
        //Reset Global flag
        g_progress_delay_read = 0;  
      }
      // mark_gen_delayed_rep_msg: Flag is set to 0 if we are getting progress report from delayed generator.
      else
      {
        generator_entry[gen_idx].mark_gen_delayed_rep_msg = 0;
      }
      // mark_killed_gen: Flag is used to avoid multiple calling of mark_gen_inactive_and_remove_from_list.
      //This code has been commented because of we are getting 10 times delay on generators
      if((max_elapsed - generator_entry[gen_idx].last_prgrss_rpt_elapsed) > global_settings->progress_report_queue_size)
      {
        if(!generator_entry[gen_idx].mark_killed_gen)
        {
          NSTL1(NULL, NULL, "GENERATOR_DELAY: Did not get progress report for %d samples as last report received at %llu,"
                            " from generator id = %d , name = %s, ip = %s, testidx = %d for data connection."
                            " Going to close connection for this generator.",
                            global_settings->progress_report_queue_size, generator_entry[gen_idx].last_prgrss_rpt_time,
                            gen_idx, generator_entry[gen_idx].gen_name, generator_entry[gen_idx].IP,
                            generator_entry[gen_idx].testidx);
       
          generator_entry[gen_idx].mark_killed_gen = 1;
          mark_gen_inactive_and_remove_from_list(mccptr, gen_idx, 0, 0, NULL);
          strcpy(generator_entry[gen_idx].test_end_time_on_gen, get_relative_time());
          sprintf(alert_msg, "Discarded generator '%s' with IP '%s'  due to not getting metric report for last <%u> samples. This will lead to lower than expected load generation", generator_entry[gen_idx].gen_name, generator_entry[gen_idx].IP, (max_elapsed - generator_entry[gen_idx].last_prgrss_rpt_elapsed));
          ns_send_alert(ALERT_CRITICAL, alert_msg);
        }
      }
    } //Gen id loop
  }
}

static inline void process_start_msg_by_client_data_msg(Msg_com_con *mccptr)
{
  static int num_started = 0;
  
  NSDL2_MESSAGES(NULL, NULL, "Method called, sgrp_used_genrator_entries = %d, num_started = %d, num_process = %d for control connection.", 
                                sgrp_used_genrator_entries, num_started, global_settings->num_process);

  num_started++;
  
  if(loader_opcode == MASTER_LOADER)
  {
    generator_entry[msg_dh->top.internal.child_id].con_gen_com_diff = ((time(NULL) * 1000) - msg_dh->top.internal.abs_ts);
  }
  NSTL1(NULL, NULL, "Received START_MSG_BY_CLIENT_DATA from client id = %d , testrun number = %d, delay = %d, %s", 
                     msg_dh->top.internal.child_id, msg_dh->top.internal.testidx,
                     (loader_opcode == MASTER_LOADER)?generator_entry[msg_dh->top.internal.child_id].con_gen_com_diff:0,
                     msg_com_con_to_str(mccptr));

  mccptr->nvm_index = msg_dh->top.internal.child_id;

  NSDL3_MESSAGES(NULL, NULL, "[Data Connection]: Check whether parent got START message from all children, "
                             " num_started_gen = %d, tot_num_gen = %d, num_connected = %d", num_started,
                             global_settings->num_process, g_data_control_var.num_connected);
  
  if (num_started == g_data_control_var.num_connected)
  {
    //Reset num_started here as in goal based scenarios it will never reset
    num_started = 0;
    NSDL3_MESSAGES(NULL, NULL, "[Data Connection]: All Generators/NVMs have been started successfully."); 
    // In case of netcloud when we will get START_MSG_BY_CLIENT by all the nvms, then we will send START_MSG_BY_CLIENT to controller
    if(loader_opcode == CLIENT_LOADER){
      NSTL1(NULL, NULL, "All generators/NVMs started successfully. Sending START_MSG_BY_CLIENT_DATA message to controller");
      send_msg_to_master(dh_master_fd, START_MSG_BY_CLIENT_DATA, DATA_MODE);
    }
  }
}

// When the end msg for test run has been recived
void process_end_test_run_msg(Msg_com_con *mccptr, EndTestRunMsg *end_test_run_msg, int conn_mode)
{
  NSDL2_MESSAGES(NULL, NULL, "Method called");
  int i, ret;
  int status = end_test_run_msg->status; 
  Msg_com_con *ch_mccptr;
 
  if(status == 3) 
   NSTL1(NULL, NULL, "NSI_STOP_GENERATOR_TOOL:Going to remove generator as requested by user");

  if (loader_opcode == CLIENT_LOADER)
  {
    // From master
    if ((mccptr->fd == dh_master_fd) || (mccptr->fd == master_fd))
    {
      NSTL1(NULL, NULL, "Received END_TEST_RUN_MESSAGE from master. Sending END_TEST_RUN_ACK_MESSAGE");
      send_end_test_ack_msg(conn_mode);
      ch_mccptr = ((conn_mode == DATA_MODE)?g_dh_msg_com_con:g_msg_com_con);
      //Close both data connection with CVMs
      for(i = 0; i < global_settings->num_process; i ++)
      {
        if(ch_mccptr[i].fd > 0)
          CLOSE_MSG_COM_CON(&ch_mccptr[i], conn_mode);
      }

      if(conn_mode == DATA_MODE)
      {
        CLOSE_MSG_COM_CON(g_dh_master_msg_com_con, conn_mode);
      }
      else
      {
        CLOSE_MSG_COM_CON(g_master_msg_com_con, conn_mode);
        kill_all_children((char *)__FUNCTION__, __LINE__, __FILE__);
        //Check if generator is not healthy ( status code 2) so generate core using abort command
        if(status == 2)
        {
          NSTL1(NULL, NULL, "create core using abort command");
          fprintf(stderr, "Test aborted signal from master");
          abort();
        }
        NS_EXIT(-1, "Test stopped, end test run request received from master");
      }
    }
    else
    {
      if(global_settings->nvm_fail_continue)
      {
        if((ret = update_vars_to_continue_test(mccptr->nvm_index)) == -1)
        {
          NSTL1(NULL, NULL, "Received END_TEST_RUN_MESSAGE from child '%d' ip = %s. Error in update vars", msg_dh->top.internal.child_id, mccptr->ip);
          return;
        }
        if(global_settings->num_process == g_data_control_var.total_killed_nvms)
        {
          NSTL1(NULL, NULL, "Received END_TEST_RUN_MESSAGE from child '%d' ip = %s. Sending it to master", msg_dh->top.internal.child_id, mccptr->ip);
          send_end_test_msg(end_test_run_msg->error_msg, end_test_run_msg->status);
        }
        else
        {
          NSTL1(NULL, NULL, "Received END_TEST_RUN_MESSAGE from child '%d' ip = %s. Ignore it", msg_dh->top.internal.child_id, mccptr->ip);
        }
      }
      else
      {
        NSTL1(NULL, NULL, "Received END_TEST_RUN_MESSAGE from child '%d' ip = %s for data handler. Sending it to master", msg_dh->top.internal.child_id, mccptr->ip);
        send_end_test_msg(end_test_run_msg->error_msg, end_test_run_msg->status);
      }
    }
  }
  else  // Master or Standalone mode. kill_all_children() will take care of the logic, msg will be sent by child
  {
     NSTL1(NULL, NULL, "Received END_TEST_RUN_MESSAGE from the generator/child '%d' ip = %s for data handler. Message: %s.", msg_dh->top.internal.child_id, mccptr->ip, end_test_run_msg->error_msg);
    /* Need to call kill_all_children and terminate test in case of standalone test 
     * or Controller mode where if generator fails then test need to stop*/
    if ((loader_opcode == STAND_ALONE && (!global_settings->nvm_fail_continue)) || 
       (loader_opcode == MASTER_LOADER && (!global_settings->con_test_gen_fail_setting.mode))) 
    {
      NSTL1(NULL, NULL, "Test Run will be stopped for data connection");
      kill_all_children((char *)__FUNCTION__, __LINE__, __FILE__);
      parent_save_data_before_end();
      save_status_of_test_run(TEST_RUN_STOPPED_BY_USER);
      NS_EXIT(-1, "FATAL ERROR: TEST RUN CANCELLED for data connection");

    }
    else if(loader_opcode == STAND_ALONE)  //Parent
    {
      if((ret = update_vars_to_continue_test(mccptr->nvm_index)) == -1)
      {
        NSTL1(NULL, NULL, "Error in update vars");
        return;
      }
      if(global_settings->num_process == g_data_control_var.total_killed_nvms)
      {
         NSTL1(NULL, NULL, "Test Run will be stopped\n");
         kill_all_children((char *)__FUNCTION__, __LINE__, __FILE__);
         parent_save_data_before_end();
         save_status_of_test_run(TEST_RUN_STOPPED_BY_USER);
         NS_EXIT(-1, "FATAL ERROR: TEST RUN CANCELLED\n");
      }
    } 
    else { /* Controller mode where we need to continue if generator fails*/
      NSTL1(NULL, NULL,"Controller test's will continue. Failed generator will be ignored and marked as inactive for data connection.");
      NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_WARNING,
                __FILE__, (char*)__FUNCTION__,"Received end test run message" 
               "from generator, ip = %s. Controller test's will continue."
               "Failed generator will be ignored.\n",
               mccptr->ip);
      
      mark_gen_inactive_and_remove_from_list(mccptr, msg_dh->top.internal.child_id, 0, 0, NULL);
    }
  }
}

/* Return: 
        0  - success
        1  - Error 
        2  - End Report got, complete 
 */
static int read_and_proc_msg_from_nvm_dh(Msg_com_con *mccptr, struct epoll_event *epev, int i, int num_children, avgtime *local_end_avg)
{

  int local_fd = mccptr->fd; //For debugging purpose
  local_end_avg = NULL;

  char ret_msg[2200];
  int retn = 0; 
 
  NSDL2_PARENT(NULL, NULL, "mccptr = %p, i = %d, num_children = %d for data connection", mccptr, i, num_children);
  if (local_fd < 0)
  {
    NSTL1(NULL, NULL, "Socket fd '%d' for child '%d' is negative", mccptr->fd, mccptr->nvm_index);
    return 1;
  }

  if(!g_progress_delay_read)
  {  
    if (epev[i].events & EPOLLERR) {
      NSDL3_MESSAGES(NULL, NULL, "EPOLLERR occured on sock %s for data connection. error = %s", 
                msg_com_con_to_str(mccptr), nslb_strerror(errno));
      INC_EPOLL_EVENT_STATS(mccptr->nvm_index, g_epollerr_count);
      CLOSE_MSG_COM_CON_EXIT(mccptr, DATA_MODE);
      HANDLE_NVM_FAILURE ();
      HANDLE_GEN_FAILURE (0);
      return 1;
    }
    if (epev[i].events & EPOLLHUP) {
      NSDL3_MESSAGES(NULL, NULL, "EPOLLHUP occured on sock %s for data connection. error = %s", 
                msg_com_con_to_str(mccptr), nslb_strerror(errno));
      
      INC_EPOLL_EVENT_STATS(mccptr->nvm_index, g_epollhup_count);
      CLOSE_MSG_COM_CON_EXIT(mccptr, DATA_MODE);
      HANDLE_NVM_FAILURE ();
      HANDLE_GEN_FAILURE (0);
      return 1;
    }
 
    /* partial write */
    if (epev[i].events & EPOLLOUT){
      INC_EPOLL_EVENT_STATS(mccptr->nvm_index, g_epollout_count);
      if (mccptr->state & NS_STATE_WRITING)
        write_msg(mccptr, NULL, 0, 0, DATA_MODE);
      else {
        NSDL3_MESSAGES(NULL, NULL, "Write state not `writing', still we got EPOLLOUT event on fd = %d for data connection", mccptr->fd);
      }
    }
 
    msg_dh = NULL;
    if (epev[i].events & EPOLLIN)
    {
      INC_EPOLL_EVENT_STATS(mccptr->nvm_index, g_epollin_count);
      msg_dh = (parent_msg *)read_msg(mccptr, &rcv_amt, DATA_MODE);
    }
  }
  else
  {
     msg_dh = (parent_msg *)read_msg(mccptr, &rcv_amt, DATA_MODE);
  }   
    /* data we are reading is not complete, wait for anoter poll */
  /* In case of nsa_log_mgr we will never get EPOLLIN beacuse we dont
     have to read anything in that case msg will be NULL & we will return
     back from here
  */
 
  HANDLE_NVM_FAILURE ();
  //Here if this is last generator where we were expecting the FINISH_REPORT so we need to just return from here
  //1 means we will return if this generator was last gen from which we were expecting FINISH_REPORT
  HANDLE_GEN_FAILURE (1);  //Note:- In case of last gen finish report we are setting local_end_avg. so need to check here and return.
  if(local_end_avg != NULL) 
    return 1;

  if (msg_dh == NULL)
    return 1; 

  NSDL3_MESSAGES(NULL, NULL, "NS Parent get message of len = %d, opcode = %d, mccptr = %p for data connection", 
                              msg_dh->top.internal.msg_len, msg_dh->top.internal.opcode, mccptr);

  switch (msg_dh->top.internal.opcode)
  {
    case START_MSG_BY_CLIENT_DATA:
      process_start_msg_by_client_data_msg(mccptr);
      break;

    case PROGRESS_REPORT:
      total_rx_pr += rcv_amt; 
      process_progress_report_msg(mccptr);
      break;
 
    case PERCENTILE_REPORT:
      process_percentile_report_msg(mccptr);
      break;
 
    case PROGRESS_REPORT_ACK:
      process_progress_report_ack_msg(mccptr);
      break;
 
    case FINISH_REPORT:
      process_finish_report_msg((num_children - g_data_control_var.total_killed_nvms), mccptr);
      if((loader_opcode != CLIENT_LOADER) && !g_data_control_var.num_active)
        return 2;
      break;

    case FINISH_REPORT_ACK:
      if(process_finish_report_ack_msg(mccptr))
        return 2;
      break;
 
    case GET_ALL_TX_DATA:
    case GET_MISSING_TX_DATA:
      send_tx_data(mccptr, msg_dh->top.internal.opcode);
      break;
 
    case MONITOR_REGISTRATION:
      handle_monitor_registration(msg_dh, mccptr->fd);
      break;
 
    case IP_MONITOR_DATA:
      read_nc_monitor_data(mccptr->fd, (User_trace *)&(msg_dh->top.internal));
      break;
 
    case NS_NEW_OBJECT_DISCOVERY:
      process_new_object_discovery_record(mccptr, (Norm_Ids*)&(msg_dh->top.internal));
      break; 
    
    case NS_NEW_OBJECT_DISCOVERY_RESPONSE:
      send_object_discovery_response_to_child(mccptr, (Norm_Ids*)&(msg_dh->top.internal));
      break;          
 
    case ATTACH_PDF_SHM_MSG:
      process_attach_pdf_shm_msg(msg_dh);
      break;
 
    case END_TEST_RUN_MESSAGE:
      process_end_test_run_msg(mccptr, (EndTestRunMsg *)&(msg_dh->top.internal), DATA_MODE);
      // Note - Above function exits in all cases, do break is not hit
      break;
 
    //Add to Received END_TEST_RUN_ACK_MESSAGE
    case END_TEST_RUN_ACK_MESSAGE:
      NSTL1(NULL, NULL, "Received END_TEST_RUN_ACK_MESSAGE from child '%d' for data connection.", mccptr->nvm_index);
      //Close the connection from the generator
      CLOSE_MSG_COM_CON(mccptr, DATA_MODE);
      break;
      
    case CAV_MEMORY_MAP:
      process_cav_memory_map(mccptr);
      break;
  
    case APPLY_MONITOR_RTC:
     set_rtc_info(mccptr, APPLY_MONITOR_RTC);
     parse_monitor_rtc_data((char *)msg_dh);
     break;
  
    case APPLY_CAVMAIN_RTC:
     set_rtc_info(mccptr, APPLY_CAVMAIN_RTC);
     parse_sm_monitor_rtc_data((char *)msg_dh , ret_msg);
     retn = write_msg(rtcdata->invoker_mccptr, ret_msg, strlen(ret_msg), 0, DATA_MODE);
     if (retn)
     { 
       fprintf(stderr, "\nparse_sm_monitor_rtc_data() - write message failed for data connection\n");
     }
     RUNTIME_UPDATION_RESET_FLAGS
     break;
    
    case TIER_GROUP_RTC:
      NSDL3_MESSAGES(NULL, NULL, "opcode TIER_GROUP_RTC recevied with msg = %s for data connection.", (char *)msg_dh + 8);
  
      set_rtc_info(mccptr, TIER_GROUP_RTC);
      parse_tier_group_rtc_data((char *)msg_dh, ret_msg, testidx);
      RUNTIME_UPDATION_CLOSE_FILES
      RUNTIME_UPDATION_RESET_FLAGS
  
      retn = write_msg(mccptr, ret_msg, strlen(ret_msg), 0, DATA_MODE);
      if (retn)
      {
        fprintf(stderr, "\nparse_tier_group_rtc_data() - write message failed for data connection\n");
      } 
      break;

  case NEW_TEST_RUN:
    process_change_tr_msg_frm_parent((HourlyMonitoringSess_msg*)&(msg_dh->top.internal)); 
    break;
 
    default:
      process_default_case_msg(mccptr, DATA_MODE);
      break;

  } // End of switch
  return 0;
}

// This is used by child to connect to parent
avgtime* wait_forever_data(int num_children, cavgtime **c_end_avg)
{
  int cnt = 0, i, ret; //cnt must be set to zero 
  struct epoll_event *epev = NULL;
  int epoll_timeout;
  char epoll_timeout_cnt = 0;
  char cmd[0xffff];
  int temp_ms = 0;
  avgtime *end_avg = NULL;
  int nvm_msg_processed = 0;
  avgtime *local_end_avg = NULL;
  int process_epoll_time_threshold = global_settings->progress_secs / 2;
  double time_diff;
  double current_time;
  char err_msg[1024] = "\0"; 

 //This variables are used for TSDB purpose
  time_t last_ts_tsdb_api_logging;
  time_t tsdb_current_time;
  time_t tsdb_time_diff;


  NSDL2_PARENT(NULL, NULL, "Method called, parent_epoll_timeout = %d, parent_timeout_max_cnt = %d, dump_parent_on_timeout = %d for data connection.", global_settings->parent_epoll_timeout, global_settings->parent_timeout_max_cnt, global_settings->dump_parent_on_timeout);	

  //Runtime changes init 
  init_runtime_data_struct();
  sprintf(cmd, "%s/bin/get_user_stats.sh %s/logs/TR%d/system_stats.log",
				 getenv("NS_WDIR"), getenv("NS_WDIR"), testidx);
  start_msg_ts = get_ms_stamp();
  MY_MALLOC_AND_MEMSET(g_last_pr_sample, MAX_NVM_NUM * sizeof(int), "last progress sample array", -1); 
  MY_MALLOC_AND_MEMSET(epev, sizeof(struct epoll_event) * g_parent_epoll_event_size, "epoll event", -1);

  if (epev == NULL)
  {
    kill_all_children((char *)__FUNCTION__, __LINE__, __FILE__);
    NS_EXIT(-1, "%s:%d Malloc failed for dynamic memory in data connection. So, exiting.", __FUNCTION__, __LINE__);
  }

  if (loader_opcode == MASTER_LOADER)
    (void) signal( SIGINT, handle_master_sigint );

  kill_timeout.tv_sec  = global_settings->parent_epoll_timeout/1000; 
  kill_timeout.tv_usec = global_settings->parent_epoll_timeout%1000;
 
  init_vars_for_each_test(num_children);
  if(global_settings->con_test_gen_fail_setting.percent_started != 100)
  {
    UPDATE_GLOB_DATA_CONTROL_VAR(g_data_control_var.num_active = g_data_control_var.num_pge = g_data_control_var.num_connected)
  }
  // Moving init_all_avgtime from wait_forever.c to ns_parent.c because it was required for dynamic transaction project before processsing gdf
  // init_all_avgtime();
  timeout = kill_timeout;

  end_avg = (avgtime *)g_end_avg[0];
  *c_end_avg = (cavgtime *)g_cur_finished[0];

  /* this process is a client and need to open a TCP socket to connect to the master */
  if (loader_opcode == CLIENT_LOADER)
  {
    NSTL1_OUT(NULL, NULL, "Connecting to controller at IP address %s and port %d for data connection.\n", master_ip, dh_master_port);
    if ((dh_master_fd = connect_to_master()) < 0) {
      fprintf( stderr, "Generator parent:  Error in creating the TCP socket to communicate with the controller (%s:%d) "
                       "for data connection. Aborting...\n", master_ip, dh_master_port);
      kill_all_children((char *)__FUNCTION__, __LINE__, __FILE__);
    }
    NSTL1_OUT(NULL, NULL, "Data connection connected...\n");


    //send_msg_to_master(master_fd, START_MSG_BY_CLIENT);
    //NSTL1_OUT(NULL, NULL, "Started test...\n");
    //    sleep(3); // This is added so that master gets start message from all clients before  it sends next message
    //Bug 60868: client loader not showing above as they are buffered
    //fflush(NULL);
  }

  if(loader_opcode == MASTER_LOADER)
  {
    int i;
    for (i = 0; i < sgrp_used_genrator_entries; i++) {
      CONTINUE_WITH_STARTED_GENERATOR(i);
      NSDL1_PARENT(NULL, NULL, "Create pctMessage.dat file for data connection generator i = %d", i);
      create_gen_pct_file(i);
    }
  } 
  create_rtg_file_data_file_send_start_msg_to_server(0);


  if(ndc_mccptr.fd != -1)
  { // We add here as epoll fd is created after start_nd_ndc()
    ADD_SELECT_MSG_COM_CON((char *)&ndc_mccptr, ndc_mccptr.fd, EPOLLIN | EPOLLERR | EPOLLHUP, DATA_MODE);
  }

  //We can remove check for outbound [TODO]
  if((ndc_data_mccptr.fd != -1))
  { //We will add data connecton fd here.
    ADD_SELECT_MSG_COM_CON((char *)&ndc_data_mccptr, ndc_data_mccptr.fd, EPOLLIN | EPOLLERR| EPOLLHUP, DATA_MODE);
  }

  //TODO: diccuss with kushal and neeraj
  //Some monitors need to be applied on JVM.
  if(is_java_type_script())
    apply_monitors_for_jvm();

  //apply_process_data_on_generator
  if((g_generator_process_monitoring_flag) && (loader_opcode == MASTER_LOADER))
    apply_java_process_monitor_for_generators();

  if(g_auto_ns_mon_flag)
    apply_jmeter_monitors();

  
  /*end of init_instance stage started in parent_init_before_starting_test*/
  //end_stage(NS_START_INST, TIS_FINISHED, NULL);

  int time_left_for_sending_inactive_inst_req_to_ndc;
  if(!(g_tsdb_configuration_flag & RTG_MODE))
    last_ts_tsdb_api_logging = 0 ;
  u_ns_ts_t last_time_stamp_of_sending_inactive_inst_req_to_ndc = get_ms_stamp();
  
  if(!(g_tsdb_configuration_flag & RTG_MODE))
    ns_tsdb_init();

  while(1)
  {
    epoll_timeout = kill_timeout.tv_sec * 1000 + kill_timeout.tv_usec / 1000;

   if(g_time_to_get_inactive_inst > 0)
   {
     time_left_for_sending_inactive_inst_req_to_ndc = get_ms_stamp() - last_time_stamp_of_sending_inactive_inst_req_to_ndc;
     if(time_left_for_sending_inactive_inst_req_to_ndc >= g_time_to_get_inactive_inst)
     {
       send_msg_to_get_inactive_instance_to_ndc(DATA_MODE);
       last_time_stamp_of_sending_inactive_inst_req_to_ndc = get_ms_stamp();
     }
   }
    if(!(g_tsdb_configuration_flag & RTG_MODE))
     {
       tsdb_current_time = ns_get_ms_stamp();
       NSTL3(NULL, NULL,"current and last_ts_tsdb_api_loggingtime for TSDB  = %ld, last_ts_tsdb_api_logging = %ld ",tsdb_current_time, last_ts_tsdb_api_logging);
       tsdb_time_diff  = tsdb_current_time - last_ts_tsdb_api_logging;
       if(tsdb_time_diff  >= TSDB_LOG_TIMEOUT)
       {
         NSTL1(NULL,NULL,"ns_tsdb_dumb_logs() Method called for logging with difference = %ld",tsdb_time_diff);
         ns_tsdb_dumb_logs();
         last_ts_tsdb_api_logging = ns_get_ms_stamp() ;
       }
     }

   //Checking time difference , if response not received from NDC to stop session
   if(ndc_connection_state == NDC_WAIT_FOR_RESP)
   {
     current_time = ns_get_ms_stamp();
     NSTL1(NULL, NULL,"current time = %f", current_time);
     time_diff = current_time - g_ndc_start_time;
     if(time_diff > NDC_TIMEOUT)
     { 
       NS_EXIT(-1, "Waited for 15 mins but still failed to receive message from ndcollector, thus stopping the session");
     }
   }

   /* If Sync Point is enalbed then calculate then set timer 
    * If monitoring mode enable then we need to set timer
    * */
   //TODO:
   if(global_settings->partition_creation_mode > 0){
     if(g_next_partition_switch_time_stamp)
     {
       u_ns_ts_t cur_time_ms = get_ms_stamp();
       if(g_next_partition_switch_time_stamp <= cur_time_ms)
       {
          //switch_partition
          ClientData cd; 
          set_idx_for_partition_switch(cd, cur_time_ms);
          g_next_partition_switch_time_stamp = cur_time_ms + global_settings->time_ptr->actual_timeout;        
       }
       temp_ms = g_next_partition_switch_time_stamp - cur_time_ms;
       if(temp_ms < epoll_timeout )
         epoll_timeout = temp_ms; // It can be 0 also
       NSDL2_PARENT(NULL, NULL, "Data connection nde_epoll_timeout = %d, ms_stamp = %d", epoll_timeout, get_ms_stamp());
     }
   }
    
   NSDL2_PARENT(NULL, NULL, "After calculate_parent_epoll_timeout in data connection: epoll_timeout = %d", epoll_timeout);

    NSDL1_PARENT(NULL, NULL, "Timeout is sec=%lu usec=%lu at %lu", timeout.tv_sec, timeout.tv_usec, get_ms_stamp());

    if(total_json_monitors)
      apply_monitors_from_json();

    if(total_ndc_node_msg)
      parse_ndc_node_msgs();

    if(mbean_mon_rtc_applied == 1)
    { 
      add_in_cm_table_and_create_msg(1);
      mbean_mon_rtc_applied = 0;
      monitor_runtime_changes_applied = 1;
    }


    /***************************************************************
      Forcefully read data form socekt if NS is running in ND mode
      This must be done before applying monitor run time change as it may set deliver_report_done to 1
     **************************************************************/    
    if((g_dh_msg_com_con_nvm0 != NULL) && (nvm_msg_processed == 0))
    {
      epev[0].events = EPOLLIN;
      NSDL1_PARENT(NULL, NULL, "Forcefully reading message from nvm0 for data connection.");
      ret = read_and_proc_msg_from_nvm_dh(g_dh_msg_com_con_nvm0, epev, 0, num_children, local_end_avg);
      if(local_end_avg != NULL)
        return local_end_avg;

      if(ret == 2)
      {
        FREE_AND_MAKE_NOT_NULL(epev, "epev", -1);
        return end_avg;
      }
      if(cnt == 0)
        cnt = 1; //Needed for memset
    }

    nvm_msg_processed = 0;
    //deliver_report_done is set when rtgMessage.dat is written after progress interval.
    //Runtime changes take effect when deliver_report_done is 1.
    //Hence Runtime changes take effect after progress interval.
    NSDL4_PARENT(NULL, NULL, "Data connection monitor_runtime_changes_applied = %d, deliver_report_done = %d", 
                              monitor_runtime_changes_applied, deliver_report_done);

    NSDL4_PARENT(NULL, NULL, "Setting deliver_report_done as 0 for data connection");
    deliver_report_done = 0;
    
    //To dump the monitor structure
    if(dump_monitor_tables == 1 && monitor_runtime_changes_applied == 0)
    {
      NSTL1(NULL, NULL, "Dumping structure of monitors for data connection.");
      dump_monitor_table();
      dump_monitor_tables = 0;
    }
 
    if(cnt > 0)
    {
      memset(epev, 0, sizeof(struct epoll_event) * cnt);
    }
    cnt = epoll_wait(g_dh_msg_com_epfd, epev, g_parent_epoll_event_size, epoll_timeout);

    u_ns_ts_t process_epoll_st = get_ms_stamp();
    //printf("wokeup from select with cnt=%d\n", cnt);
    /* RTC: In case of runtime changes in NS and NC, if epoll timeout and 
     * pause and resume acknowledgements are not received.
     * In case of pause ack not received then we need to send resume messages to all NVMs or Generators.
     * Whereas in case of resume ack we need to log the message.*/

    if (cnt > 0)
    {
      /* Reset epoll timeout count to 0 as we has to track only continuesly timeout*/
      epoll_timeout_cnt = 0;
      for (i = 0; i < cnt; i++)
      {
        #ifdef CHK_AVG_FOR_JUNK_DATA 
        check_avgtime_for_junk_data("ns_data_handler_thread.c[1317]", 2);
        #endif

        Msg_com_con *mccptr = NULL;
        void *event_ptr = NULL;

        event_ptr = epev[i].data.ptr;
        /* What if fd_mon is NULL it can be NULL if any events come from nsa_log_mgr in Normal case it should not come*/
        NSDL3_MESSAGES(NULL, NULL, "NS Parent get an event of type = %d for data connection", *(char *) event_ptr);
        if(event_ptr) {
          switch(*(char *) event_ptr)
          {
            /* Any event is encountered on ndc_mccptr.fd,
            * Means connection close by LPS or event from LPS */
            case NS_NDC_TYPE:
              handle_ndc(epev, i);
              continue;

            case NS_NDC_DATA_CONN:
              handle_ndc_data_conn(epev, i);
              continue;
            case NS_STRUCT_CUSTOM_MON:
              if(epev[i].events & EPOLLOUT)
              {
                NSDL4_PARENT(NULL, NULL, "CUSTOM_MON: inside epoll out for data connection");
                cm_send_msg_to_cmon(event_ptr, 1);
                continue;
              }
              else
              {
                NSDL4_PARENT(NULL, NULL, "CUSTOM_MON: inside epoll in for data connection");
                handle_if_custom_monitor_fd (event_ptr); continue;
              }
            //TODO: HANDLE EPOLLERR...
            case NS_STRUCT_CHECK_MON:
              if(epev[i].events & EPOLLOUT)
              {
                NSDL4_PARENT(NULL, NULL, "CHECK_MON: inside epoll out for data connection");
                chk_mon_send_msg_to_cmon(event_ptr);
                continue;
              }
              else
              {
                NSDL4_PARENT(NULL, NULL, "CHECK_MON: inside epoll in for data connection");
                handle_if_check_monitor_fd (event_ptr); continue;
              }
        
            case NS_STRUCT_TYPE_LISTEN:
              /* Connection came from one of the tools */
              accept_connection_from_tools_dh();
              continue;
          }

          mccptr = (Msg_com_con *)epev[i].data.ptr;
        } else {  // fd_mon is NULL it means that this event for nsa_log_mgr
           NSEL_MAJ(NULL, NULL, ERROR_ID, ERROR_ATTR, "wait_forever: fd_mon got NULL for data connection while it should not.");
           CLOSE_MSG_COM_CON_EXIT(mccptr, DATA_MODE);
           continue;
        }

        /**********************************
         Handle Parent-NVM communications 
         *********************************/
        ret = read_and_proc_msg_from_nvm_dh(mccptr, epev, i, num_children, local_end_avg);
        if(local_end_avg != NULL)
          return local_end_avg;

        if(ret == 1)
          continue;
        else if(ret == 2)
        {
          FREE_AND_MAKE_NOT_NULL(epev, "epev", -1);
          return end_avg;
        }
        nvm_msg_processed = 1;
        #ifdef CHK_AVG_FOR_JUNK_DATA
        check_avgtime_for_junk_data("ns_data_handler_thread.c[1389]", 2);
        #endif
      }  // End of for()
    } // End of if
    else if (cnt == 0)
    {
      epoll_timeout_cnt++;
      NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_WARNING,
                                __FILE__, (char*)__FUNCTION__,
				"Parent epoll_wait timeout for data connection.  cur timeout count  = %d max timeout count = %d",
				 epoll_timeout_cnt, global_settings->parent_timeout_max_cnt);

      /* Continue if timeout count is not continueusly reached to max_cnt*/
      if(epoll_timeout_cnt <= global_settings->parent_timeout_max_cnt) {
	 continue;
      } else {
      	  NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_WARNING,
                                     __FILE__, (char*)__FUNCTION__,
                                     "Parent epoll_wait timeout  for data connection cur count reached to its max limit = %d ",
				     global_settings->parent_timeout_max_cnt);	
         
	 /* If has to dump all its NVMs DO NOT CONTINUE*/
         if(global_settings->dump_parent_on_timeout == 0) {
      	    NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_MAJOR,
                                     __FILE__, (char*)__FUNCTION__,
                                     "Gathering debug information from children for data connection. It might take a while...");
            nslb_system(cmd,1,err_msg);
            continue;
         }
      }
      /* kill all children by sending SIGTERM to all children */
      if (rfp) {
        char err_msg[1024];
        sprintf(err_msg, "Netstorm was expecting '%d' samples but got only '%d' at time stamp '%llu' for data connection",
                          num_children, num_children - g_data_control_var.num_active, get_ms_stamp());
        fprintf(rfp, "%s", err_msg);
      	NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_MAJOR,
                                 __FILE__, (char*)__FUNCTION__, err_msg);
      }

      end_avg->elapsed = cur_sample -1; /* cur_sample has not been send yet */
      if(loader_opcode == MASTER_LOADER)
      {
        NSDL2_MESSAGES(NULL, NULL, "Received Progress report for generator id %d, test run number = %d for data connection", msg_dh->top.internal.child_id, msg_dh->top.internal.testidx);
      }
      deliver_report(run_mode, dh_master_fd, g_end_avg, g_cur_finished, rfp, srfp);
      sleep(2);

      ns_parent_state = NS_PARENT_ST_TEST_OVER;
      NSDL3_MESSAGES(NULL, NULL, "Setting parent test run status = %d for data connection", ns_parent_state);
        
      nslb_system(cmd,1,err_msg);
      
      if (rfp)
        fflush(rfp);

      FREE_AND_MAKE_NOT_NULL(epev, "epev", -1);
      return end_avg;
    } 
    else if (errno == EBADF)
      perror("Bad g_dh_msg_com_epfd");
    else if (errno == EFAULT)
      perror("The memory area");
    else if (errno == EINVAL)
      perror("g_dh_msg_com_epfd is not valid");
    else
    {
      if (errno != EINTR)
        perror("epoll_wait() failed");
      else
        NSDL3_MESSAGES(NULL, NULL, "epoll_wait() interrupted");
    }
    
    /* Check how much time epoll takes, > 50 % of progress interval then log in trace */
    u_ns_ts_t process_epoll_end = get_ms_stamp();
    u_ns_ts_t process_epoll_time = process_epoll_end - process_epoll_st;
    if(process_epoll_time > process_epoll_time_threshold || cnt > g_parent_epoll_event_size)
    {
      NSTL1(NULL, NULL, "Data handler took more than 50%% of progress interval to process epoll events or got more events than max, "
                        "process_epoll_time = %lld, global_settings->progress_secs = %d, Number of events processed = %d, "
                        "Max number of events = %d", 
                         process_epoll_time, global_settings->progress_secs, cnt, g_parent_epoll_event_size);
    }
  }

  // Should we close listen_fd - may be not

  if (rfp) fflush(rfp);
  FREE_AND_MAKE_NOT_NULL(epev, "epev", -1);
  return end_avg;
}

void *ns_data_handler_thread()
{
  char *ip;
  if(global_settings->enable_memory_map)
  {
    //Initializes the memory map.
    nslb_mem_map_init();
  }

  MY_MALLOC_AND_MEMSET(g_epollin_count, (global_settings->num_process + 1)*sizeof(unsigned long), "g_epollin_count", -1);
  MY_MALLOC_AND_MEMSET(g_epollout_count, (global_settings->num_process + 1)*sizeof(unsigned long), "g_epollout_count", -1);
  MY_MALLOC_AND_MEMSET(g_epollerr_count, (global_settings->num_process + 1)*sizeof(unsigned long), "g_epollerr_count", -1);
  MY_MALLOC_AND_MEMSET(g_epollhup_count, (global_settings->num_process + 1)*sizeof(unsigned long), "g_epollhup_count", -1);

  // Initializing thread local storage
  ns_tls_init(VUSER_THREAD_BUFFER_SIZE);
  SET_CALLER_TYPE(IS_DATA_HANDLER);
  NSDL2_MESSAGES(NULL, NULL, "Method called");

  /* This code accept child thread connect*/
 //   ns_data_handler_thread_accept(global_settings->num_process);
  wait_for_child_registration_control_and_data_connection(global_settings->num_process , DATA_MODE);
  
  if(loader_opcode != CLIENT_LOADER) {
    ip = "127.0.0.1"; // Master/NS ip for event logger
  } else {
    ip = master_ip; // Generator/NVM Event Logger
  }

  /*Connect Data Handler to Event Logger*/
  if(global_settings->enable_event_logger) {
     if ((event_logger_dh_fd = connect_to_event_logger_ex(ip, event_logger_port_number, DATA_MODE)) < 0)  {
        fprintf( stderr, "%s:  Error in creating the TCP socket to"
                                    "communicate with the nsa_event_logger (%s:%d)."
                                    " Aborting...\n", (loader_opcode == STAND_ALONE)?"NS parent":(loader_opcode == CLIENT_LOADER)?"Generator parent":"Controller parent", ip, event_logger_port_number);
        END_TEST_RUN
     }
     ADD_SELECT_MSG_COM_CON((char *)&g_dh_el_subproc_msg_com_con, event_logger_dh_fd, EPOLLIN | EPOLLERR | EPOLLHUP, DATA_MODE);
     sprintf(g_dh_el_subproc_msg_com_con.conn_owner, "DATA_HANDLER_TO_EVENT_LOGGER");
  }

  /*This function processes only data opcode*/
  wait_forever_data(global_settings->num_process, &c_end_results);
  ns_process_cav_memory_map();
  NSDL2_MESSAGES(NULL, NULL, "returning from wait_forever_data");
  stop_all_custom_monitors();  //stop all custom monitors/standard monitors connection
  TLS_FREE_AND_RETURN(NULL);
}

void ns_data_handler_thread_create(pthread_t *data_handler_thid)
{
  //static pthread_attr_t data_handler_attr;
  //set stack size to the stack size set by ulimit
  //pthread_attr_init(&data_handler_attr);
  //pthread_attr_setdetachstate(&data_handler_attr, PTHREAD_CREATE_DETACHED);
  if (pthread_create(data_handler_thid, NULL, ns_data_handler_thread, NULL) != 0) {
    //Error
    NSTL1(NULL, NULL, "Error in creating thread for data connection, err:%s", nslb_strerror(errno));
    NS_EXIT(-1, "Error in creating thread for data connection, err:%s", nslb_strerror(errno));
  }
  //pthread_attr_destroy(&data_handler_attr);
}
