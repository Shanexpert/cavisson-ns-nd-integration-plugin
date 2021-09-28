#define _GNU_SOURCE

#include <sys/wait.h>
#include <sys/epoll.h>
#include <regex.h>
#include <libgen.h>
#include <signal.h>

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
#include "ns_parent_msg_com.h"
#include "ns_runtime.h"
#include "ns_monitor_profiles.h"

extern HM_Times_data *hm_data_ptr;
//extern int g_rtc_msg_seq_num;
//RTC: Counter used to set number of messages send to generators/children 
//u_ns_ts_t rtc_epoll_start_time = 0;
//u_ns_ts_t rtc_epoll_end_time = 0;
//int runtime_change_state = RESET_RTC_STATE;
/*---------------------RunTimeChanges-----------------------
                            |--> ~RUNTIME_PROGRESS_FLAG & ~RUNTIME_SUCCESS_FLAG                        runtime changes failed.
     rtcdata->flags         |--> ~RUNTIME_PROGRESS_FLAG & RUNTIME_SUCCESS_FLAG                         runtime changes success.
                            |-->  RUNTIME_PROGRESS_FLAG & (RUNTIME_SUCCESS_FLAG|~RUNTIME_SUCCESS_FLAG) runtime changes in-progress.
*/

int got_start_phase = 0;
//this variable is set when rtgMessage.dat is written after progress interval.
//Runtime changes take effect when deliver_report_done is 1.
//Hence Runtime changes take effect after progress interval.
int deliver_report_done = 0;	

//variable to set time so that we can find the time taken to process PROGESS_REPORT and to deliver it.

extern char *extract_header_from_event(char *event, int *severity);
int master_fd=-1;
int ns_parent_state; //0: init, 1= started, 2 = ending

int udp_fd;  /* this fd is the fd that the parent listens to for messages from the child (e.g. progress reports, finish reports, etc..) */
int gui_fd = -1;
int gui_fd2 = -1; //This is FD connected to another controller's ns_server
int run_mode;

// Following two variables are used to enabled logging of graph data in gui.data
// These are set  using export GUI_DATA=<seq_num>
FILE *gui_fp ;
int gui_data_seq = 0; // Sequence to be logged in gui data. 0 means all.

FILE *rtg_fp = NULL;
FILE *rfp;
FILE *srfp;
#ifndef CAV_MAIN
int loader_opcode = -1; /* MASTER or CLIENT */
#else
int loader_opcode = 0; // For CAVMAIN loader opcode would always be STANDALONE
#endif
char send_events_to_master = 0; /* MASTER or CLIENT */
int g_collect_no_eth_data; //default is 0 - means collect
int total_udpport_entries;
int cpu_udp_fd = 0;

pid_t cpu_mon_pid = 0;

int g_generator_idx = -1; /* Earlier default value was changed from -1 to 0 
                           * NetCloud: To support Drill down reports reverting default value 
                           * from 0 to -1
                           * In case of generators, g_generator_idx will be 0,1,2,....,n
                           * Whereas for standalone and controller its value would be -1
                           */
int g_parent_idx = -1;          /* This is to store parent idx in case of master mode.
                                 * It will help in maintaining bitmasks. */

extern void read_nc_monitor_data(int cmd_fd, User_trace *vuser_trace_msg);
extern void dvm_make_conn();
extern int total_dynamic_vector_mon_entries;
avgtime *tmp_reset_avg;
cavgtime *tmp_reset_cavg;

extern int process_alert_server_config_rtc(int fd, char *msg);

#define HANDLE_CHILD_FAILURE {\
  int is_con_closed = 0;\
  NSDL2_PARENT(NULL, NULL, "loader_opcode = %d, nvm_fail_continue = %d, flags = %d, con_type = %d", loader_opcode, global_settings->nvm_fail_continue, mccptr->flags, mccptr->con_type);\
  if((loader_opcode == MASTER_LOADER) && (global_settings->con_test_gen_fail_setting.mode) && (mccptr->con_type == NS_STRUCT_TYPE_CLIENT_TO_MASTER_COM))\
    is_con_closed = 1;\
  else if((loader_opcode != MASTER_LOADER) && (global_settings->nvm_fail_continue) && (mccptr->con_type == NS_STRUCT_TYPE_NVM_PARENT_COM)){\
    is_con_closed = 1;\
  }\
  if(is_con_closed && (mccptr->flags & NS_MSG_COM_CON_IS_CLOSED))\
    decrease_msg_counters(mccptr->nvm_index, 0);\
}

void set_scheduler_start_flag()
{
  NSDL1_PARENT(NULL, NULL, "Method called");
  got_start_phase = 1;
}

//NetCloud: Added struct pointer for avgtime and cavgtime in order to create rtgMessage.dat file for generators
//static avgtime *nc_avg = NULL;
//static cavgtime *nc_cavg =  NULL;

#ifndef NS_PROFILE
//NC: In release 3.9.3, pass generator index to create and update generator's transaction files


//This is to fill cumulative data from cum struct to avg struct
//Because while filling we are using only avg struct

// Anuj 6/12/07: flag is passed since it is been recursively called by kill_all_childern, when there is an error, so there was a infinite loop
// flag = 1 , when called from the kill_all_childern()
// else flag = 0
int send_msg_to_all_clients(int opcode, int called_from_kill_all_childern)
{
  int i;
  parent_child send_msg;
  EndTestRunMsg end_msg;
  if(opcode == RTC_RESUME) 
    send_msg.gen_rtc_idx = ++(rtcdata->msg_seq_num);
  NSDL2_MESSAGES(NULL, NULL, "Method called, opcode = %d, called_from_kill_all_childern = %d, send_msg.gen_rtc_idx = %d", 
                                                          opcode, called_from_kill_all_childern, send_msg.gen_rtc_idx);
  if (opcode == END_TEST_RUN_MESSAGE) {
    end_msg.opcode = opcode;
    NSTL1(NULL, NULL, "(Master -> Generator) opcode = %d, end_msg.opcode = %d, end_msg = %p", opcode, end_msg.opcode, (char *)&end_msg);
  } else {
    send_msg.opcode = opcode;
    NSTL1(NULL, NULL, "(Master -> Generator) opcode = %d, send_msg.opcode = %d", opcode, send_msg.opcode);
  } 
  /* Netomni Changes: 
   * Earlier in MASTER-MODE we were using client struct, for code cleanup
   * total_client_entries are now replaced by sgrp_used_genrator_entries
   *  */
  for(i=0; i<sgrp_used_genrator_entries ;i++) 
  { 
    if(generator_entry[i].flags == 0)
    { 
      NSDL3_MESSAGES(NULL, NULL, "Generator flag for %s is not set",generator_entry[i].gen_name);
      continue;
    }
    /* How to handle partial write as this method is the last called ?? */
    if(opcode == END_TEST_RUN_MESSAGE)
    {
      if (generator_entry[i].flags & IS_GEN_INACTIVE) {
        if (g_dh_msg_com_con[i].ip) {
          NSDL3_MESSAGES(NULL, NULL, "Data connection with the client is already closed so"
                                     " not sending the msg %s", msg_com_con_to_str(&g_dh_msg_com_con[i]));
        }
        continue;
      } else if(g_dh_msg_com_con == NULL || g_dh_msg_com_con[i].fd == -1)
        continue;
      NSDL3_MESSAGES(NULL, NULL, "Sending data connection msg to Client id = %d, opcode = %d, %s", i, opcode, 
                msg_com_con_to_str(&g_dh_msg_com_con[i]));
    }

    if (generator_entry[i].flags & IS_GEN_INACTIVE) {
      if (g_msg_com_con[i].ip) {
        NSDL3_MESSAGES(NULL, NULL, "Control connection with the client is already closed so"
                                   " not sending the msg %s", msg_com_con_to_str(&g_msg_com_con[i]));
      }
      continue;
    }
    NSDL3_MESSAGES(NULL, NULL, "Sending control connection msg to Client id = %d, opcode = %d, %s", i, opcode, 
              msg_com_con_to_str(&g_msg_com_con[i]));

    if (opcode == END_TEST_RUN_MESSAGE){
      end_msg.msg_len = sizeof(end_msg) - sizeof(int);	
      write_msg(&g_dh_msg_com_con[i], (char *)&end_msg, sizeof(end_msg), 
                          called_from_kill_all_childern, DATA_MODE);
      write_msg(&g_msg_com_con[i], (char *)&end_msg, sizeof(end_msg), 
                          called_from_kill_all_childern, CONTROL_MODE);
    }
    else {
      send_msg.msg_len = sizeof(send_msg) - sizeof(int);

      if(opcode == RTC_RESUME) 
      {
        if((CHECK_RTC_FLAG(RUNTIME_QUANTITY_FLAG)) && (!generator_entry[i].send_buff[0]))
        {
           NSDL3_MESSAGES(NULL, NULL, "Not sending RTC Resume msg to Client id = %d",i); 
           continue;
        } 
      }

      if ((write_msg(&g_msg_com_con[i], (char *)&send_msg, sizeof(send_msg), called_from_kill_all_childern, CONTROL_MODE)) == RUNTIME_SUCCESS)
      { 
        if(opcode == RTC_RESUME)
          INC_RTC_MSG_COUNT(g_msg_com_con[i].nvm_index);
      }
    }
  }
  return(0);
}

cavgtime **g_next_finished;
cavgtime **g_cur_finished;
cavgtime **g_dest_cavg;
struct timeval timeout, kill_timeout;
parent_msg *msg = NULL;

static int ramp_down_first_msg_rcv = 0;

static int num_ready = 0;
static int rcv_amt;

static struct sockaddr_in cliaddr;
static socklen_t addrlen;

u_ns_ts_t start_msg_ts;
size_t *g_child_status_mask;
size_t **g_child_group_status_mask;
extern int all_nvms_sent[];

/* Just forward msg to children */
static inline void process_start_phase_from_master(parent_child *pc_msg)
{
  Schedule *schedule;

  if (pc_msg->grp_idx == -1) {
    schedule = scenario_schedule;
  } else {
    schedule = &(group_schedule[pc_msg->grp_idx]);
  }
  
  /* This for the parent to know its ID so that it can propogate it to parent
   * when needed. */

  if (loader_opcode == CLIENT_LOADER) {
    if (g_parent_idx == -1){
      g_parent_idx = pc_msg->child_id;
    }
    NSTL1(NULL, NULL, "Received start phase message from controller, setting schedule start flag for generator id = %d", g_parent_idx);
    //setting the start_phase
    set_scheduler_start_flag();
  }

  send_schedule_phase_start(schedule, pc_msg->grp_idx, pc_msg->phase_idx);
}

//this function is copy of check_before_sending_nxt_phase()
//This function will be called only on controller if any generator is dead 
//and that genartor was last generator from which we were expecting the pahse complete message
void check_before_sending_nxt_phase_only_from_controller(int phase_idx, int grp_idx, int child_idx) {

  int i, num;
  Phases *ph;
  char time[0xff];
  int *dependent_grp_array = NULL;
  static int num_grp_complete = 0;
  Schedule *schedule;
  
  NSDL2_MESSAGES(NULL, NULL, "Method Called, sigterm_received = %d, grp_idx=%d", sigterm_received, grp_idx); 

  if (grp_idx == -1) {
    schedule = scenario_schedule;
    ph = &(schedule->phase_array[phase_idx]);
  } else {
    schedule = &(group_schedule[grp_idx]);
    ph = &(schedule->phase_array[phase_idx]);
  }
  
  /* It may possible that pause msg from tool & phase complete message from some/all children come at the same time. 
   * In that case we can not send next phase to child.
   * This nxt phase we will send Resume msg will recieved. 
   * So to remember last phase complete msg we set the status of that phase (paused) & when resume recievs we 
   * find puased phase & send the nxt phase.
   */
  if(global_settings->pause_done == 1)
  {
    ph->phase_status = PHASE_PAUSED;
    return;
  }

  /* Phase is completed */
  ph->phase_status = PHASE_IS_COMPLETED;

  /* Check monitor end */
  stop_check_monitor(TILL_COMPLETION_OF_PHASE, schedule->phase_array[phase_idx].phase_name);

  /* Phase Commentary */
  convert_to_hh_mm_ss(get_ms_stamp() - global_settings->test_start_time, time);
  log_phase_time(PHASE_IS_COMPLETED, ph->phase_type, ph->phase_name, time);
  if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO) {
    print2f(rfp, "Phase '%s' (phase %d) was complete at %s\n", 
            ph->phase_name, phase_idx, time);
    /* We have to send last phase -
       Sigterm + Simple Scenario + Not Last Phase
    */    
    if(sigterm_received && 
       phase_idx != (schedule->num_phases - 1) && 
       global_settings->schedule_type == SCHEDULE_TYPE_SIMPLE) {
      NSDL2_MESSAGES(NULL, NULL, "Graceful exit, sigterm_received = %d.Parent/Controller process sending START_PHASE msg to child/generator process", sigterm_received);  
      NSDL2_MESSAGES(NULL, NULL, "Schedule phase send, phase_idx = %d\n", schedule->num_phases - 1);
      send_schedule_phase_start(schedule, grp_idx, schedule->num_phases - 1);
      schedule->phase_idx = schedule->num_phases - 1;
      return;
    }
  } else if (global_settings->schedule_by == SCHEDULE_BY_GROUP) {
    print2f(rfp, "Group '%s' (group %d) phase '%s' (phase %d) was complete at %s\n", 
            runprof_table_shr_mem[grp_idx].scen_group_name, grp_idx, 
            ph->phase_name, phase_idx, time);
  }


#ifdef NS_DEBUG_ON
  NSDL2_MESSAGES(NULL, NULL, "%s", nslb_show_bitflag(schedule->bitmask));
#endif  /* NS_DEBUG_ON */

  if (phase_idx == (schedule->num_phases - 1))
  {          /* Last phase of that grp */
    num = get_dependent_group(grp_idx, dependent_grp_array);
    if (num != -1)
    {
      if (dependent_grp_array == NULL)
        MY_MALLOC(dependent_grp_array, sizeof(int) * total_runprof_entries, "dependent_grp_array", -1);
      NSDL3_SCHEDULE(NULL, NULL, "Dependent %d groups found\n", num);
      for (i = 0; i < num; i++) {
        int dependent_grp_idx = dependent_grp_array[i];
        //Shilpa - schedule - should point to dependent group id now
        schedule = &(group_schedule[dependent_grp_idx]);

        send_schedule_phase_start(schedule, dependent_grp_idx, 0); /* grp_idx, phase_idx */
        NSDL3_SCHEDULE(NULL, NULL, "Sent grp start msg to grp %d\n", dependent_grp_idx);
      }
    }
    num_grp_complete++;
  }
  else
  {
    send_schedule_phase_start(schedule, grp_idx, phase_idx + 1);
    schedule->phase_idx = phase_idx + 1;
  }
  
  if (num_grp_complete == total_runprof_entries) {
    /* All groups are completed */
    /* Nothing */
  }
}
// Following method is called in 2 cases 
// - from process phase complete msg() if phase complete message is recieved from all children.
// - from process resume schedule() if phase is paused by this method when called from process phase complete msg() 
void check_before_sending_nxt_phase(parent_msg *msg) {

  int i, num;
  Phases *ph;
  parent_child *pc_msg = &(msg->top.internal);
  int phase_idx = pc_msg->phase_idx;
  int grp_idx = pc_msg->grp_idx;
  char time[0xff];
  int *dependent_grp_array = NULL;
  static int num_grp_complete = 0;
  Schedule *schedule;
  
  NSDL2_MESSAGES(NULL, NULL, "Method Called, sigterm_received = %d, grp_idx=%d", sigterm_received, grp_idx); 

  if (grp_idx == -1) {
    schedule = scenario_schedule;
    ph = &(schedule->phase_array[phase_idx]);
  } else {
    schedule = &(group_schedule[grp_idx]);
    ph = &(schedule->phase_array[phase_idx]);
  }
  
  /* It may possible that pause msg from tool & phase complete message from some/all children come at the same time. 
   * In that case we can not send next phase to child.
   * This nxt phase we will send Resume msg will recieved. 
   * So to remember last phase complete msg we set the status of that phase (paused) & when resume recievs we 
   * find puased phase & send the nxt phase.
   */
  if(global_settings->pause_done == 1)
  {
    ph->phase_status = PHASE_PAUSED;
    NSTL1(NULL, NULL, "Here pause msg came from tool. So, setting phase status to PAUSE");
    return;
  }

  /* Phase is completed */
  ph->phase_status = PHASE_IS_COMPLETED;

  if (loader_opcode == CLIENT_LOADER) {
     if (phase_idx != (schedule->num_phases - 1)) 
      schedule->phase_idx = phase_idx + 1;
    NSTL1(NULL, NULL, "'%s' phase completed, sending message to controller", ph->phase_name);
    forward_msg_to_master(master_fd, msg, sizeof(parent_child));
    /* Added Phase complete message for generators in global.dat file 
     * as there was issue in gui while viewing phase duration */
    convert_to_hh_mm_ss(get_ms_stamp() - global_settings->test_start_time, time);
    log_phase_time(PHASE_IS_COMPLETED, ph->phase_type, ph->phase_name, time);
    if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO) {
      print2f(rfp, "Phase '%s' (phase %d) was complete at %s\n", 
               ph->phase_name, phase_idx, time);
    } else if (global_settings->schedule_by == SCHEDULE_BY_GROUP) {
      print2f(rfp, "Group '%s' (group %d) phase '%s' (phase %d) was complete at %s\n", 
              runprof_table_shr_mem[grp_idx].scen_group_name, grp_idx, 
              ph->phase_name, phase_idx, time);
    }
    return;
  } /* else */


  /* Check monitor end */
  stop_check_monitor(TILL_COMPLETION_OF_PHASE, schedule->phase_array[phase_idx].phase_name);

  /* Phase Commentary */
  convert_to_hh_mm_ss(get_ms_stamp() - global_settings->test_start_time, time);
  log_phase_time(PHASE_IS_COMPLETED, ph->phase_type, ph->phase_name, time);
  if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO) {
    print2f(rfp, "Phase '%s' (phase %d) was complete at %s\n", 
            ph->phase_name, phase_idx, time);
    /* We have to send last phase -
       Sigterm + Simple Scenario + Not Last Phase
    */    
    if(sigterm_received && 
       phase_idx != (schedule->num_phases - 1) && 
       global_settings->schedule_type == SCHEDULE_TYPE_SIMPLE) {
      NSDL2_MESSAGES(NULL, NULL, "Graceful exit, sigterm_received = %d.Parent/Controller process sending START_PHASE msg to child/generator process", sigterm_received);  
      NSDL2_MESSAGES(NULL, NULL, "Schedule phase send, phase_idx = %d\n", schedule->num_phases - 1);
      send_schedule_phase_start(schedule, grp_idx, schedule->num_phases - 1);
      schedule->phase_idx = schedule->num_phases - 1;
      return;
    }
  } else if (global_settings->schedule_by == SCHEDULE_BY_GROUP) {
    print2f(rfp, "Group '%s' (group %d) phase '%s' (phase %d) was complete at %s\n", 
            runprof_table_shr_mem[grp_idx].scen_group_name, grp_idx, 
            ph->phase_name, phase_idx, time);
  }


#ifdef NS_DEBUG_ON
  NSDL2_MESSAGES(NULL, NULL, "%s", nslb_show_bitflag(schedule->bitmask));
#endif  /* NS_DEBUG_ON */

  NSDL3_SCHEDULE(NULL, NULL, "Check phase '%s' completed for all alive NVMs/Gens, "
                             "phase_idx = %d, schedule->num_phases = %d, num_grp_complete = %d", 
                              get_phase_name(phase_idx), phase_idx, schedule->num_phases, num_grp_complete);

  if (phase_idx == (schedule->num_phases - 1)) {          /* Last phase of that grp */
    if (dependent_grp_array == NULL) {
      MY_MALLOC(dependent_grp_array, sizeof(int) * total_runprof_entries, "dependent_grp_array", -1);
    }
    num = get_dependent_group(grp_idx, dependent_grp_array);
    if (num != -1) {
      NSDL3_SCHEDULE(NULL, NULL, "Dependent %d groups found\n", num);
      for (i = 0; i < num; i++) {
        int dependent_grp_idx = dependent_grp_array[i];
        //Shilpa - schedule - should point to dependent group id now
        schedule = &(group_schedule[dependent_grp_idx]);

        send_schedule_phase_start(schedule, dependent_grp_idx, 0); /* grp_idx, phase_idx */
        NSDL3_SCHEDULE(NULL, NULL, "Sent grp start msg to grp %d\n", dependent_grp_idx);
      }
    }
    num_grp_complete++;
  } else {
    send_schedule_phase_start(schedule, grp_idx, phase_idx + 1);
    schedule->phase_idx = phase_idx + 1;
  }
  
  if (num_grp_complete == total_runprof_entries) {
    NSDL3_SCHEDULE(NULL, NULL, "All groups have comepleted their all scheduling");
    /* All groups are completed */
    /* Nothing */
  }
}

static inline int process_phase_complete_msg(parent_msg *msg)
{
  Schedule *schedule;
  parent_child *pc_msg = &(msg->top.internal);

  NSDL3_MESSAGES(NULL, NULL, "Method Called, grp idx = %d, phase idx = %d, child id = %d", 
                 pc_msg->grp_idx, pc_msg->phase_idx, pc_msg->child_id);

  NSTL1(NULL, NULL, "Received phase complete message for grp idx = %d" 
                   " phase completed phase idx = %d, child/generator id = %d," 
                   " Active children/generator = %d, num children/generator = %d",
                   pc_msg->grp_idx, pc_msg->phase_idx, pc_msg->child_id,
                   g_data_control_var.num_active, global_settings->num_process);

  if (sigterm_received && 
      !(global_settings->schedule_by == SCHEDULE_BY_SCENARIO &&  
      global_settings->schedule_type == SCHEDULE_TYPE_SIMPLE)) {         /* This is done to cease all scheduling activities once we receive a ctrl-c */
    NSDL3_MESSAGES(NULL, NULL, "Scheduling msg from child %d ignored.", pc_msg->child_id);
    return 0;
  }

  if (pc_msg->grp_idx == -1) {
    schedule = scenario_schedule;
  } else {
    schedule = &(group_schedule[pc_msg->grp_idx]);
  }
  
#ifdef NS_DEBUG_ON
  NSDL3_MESSAGES(NULL, NULL, "Received phase complete message: NVM ID = %d, Group Index = %d,"
                             " phase idx = %d, phase = %s, bitflag = %s, phase name = %s", 
                             pc_msg->child_id, pc_msg->grp_idx, pc_msg->phase_idx, 
                             get_phase_name(schedule->phase_array[pc_msg->phase_idx].phase_type),
                             nslb_show_bitflag(schedule->bitmask), schedule->phase_array[msg->top.internal.phase_idx].phase_name);
#endif

  /* Phase timings */
  if (schedule->phase_array[msg->top.internal.phase_idx].phase_type == 
      SCHEDULE_PHASE_RAMP_UP) {
    global_settings->test_rampup_done_time = get_ms_stamp();
  }

  if (schedule->phase_array[msg->top.internal.phase_idx].phase_type == 
      SCHEDULE_PHASE_STABILIZE) {  /* or ramp up phase complete ?? */
    global_settings->test_runphase_start_time = get_ms_stamp(); //Child Runphase start
    gRunPhase = NS_RUN_PHASE_EXECUTE;
  }


  DEC_SCHEDULE_MSG_COUNT(pc_msg->child_id, 0, pc_msg->grp_idx);
  if(CHECK_ALL_SCHEDULE_MSG_DONE)
  {
    /*Stop Nethavoc Scenario at phase end*/
    nethavoc_send_api(schedule->phase_array[msg->top.internal.phase_idx].phase_name, NS_PHASE_END);
    check_before_sending_nxt_phase(msg);
  }
  return 0;
}

void dump_generator_entry()
{
  int i;
  NSDL2_MESSAGES(NULL, NULL, "Method called, total_generator_entries = %d", total_generator_entries);  

  for(i = 0; i < total_generator_entries; i++)
  {
    NSDL2_PARSING(NULL, NULL, "Gen_Id = %d, mark_gen = %d, gen_name = [%s], IP = [%s], resolved_IP = [%s], agentport = [%s], "
                              "location = [%s], work = [%s], gen_type = [%s], comments = [%s], gen_path = [%s], gen_keyword = [%s], "
                              "fd = %d, used_gen_flag = %d, ramp_up_vuser_or_sess_per_gen = %d, ramp_down_vuser_or_sess_per_gen = %d, "
                              "testidx = %d, pct_fd = %d, flags = %d, num_groups = %d, resolve_flag = %d, test_start_time_on_gen = [%s],"
                              "test_end_time_on_gen = [%s]", 
                               i, generator_entry[i].mark_gen, generator_entry[i].gen_name, generator_entry[i].IP, 
                               generator_entry[i].resolved_IP, generator_entry[i].agentport, generator_entry[i].location,
                               generator_entry[i].work, generator_entry[i].gen_type, generator_entry[i].comments,
                               generator_entry[i].gen_path, generator_entry[i].gen_keyword, generator_entry[i].fd,
                               generator_entry[i].used_gen_flag, generator_entry[i].ramp_up_vuser_or_sess_per_gen, 
                               generator_entry[i].ramp_down_vuser_or_sess_per_gen, generator_entry[i].testidx,
                               generator_entry[i].pct_fd, generator_entry[i].flags, generator_entry[i].num_groups, 
                               generator_entry[i].resolve_flag, generator_entry[i].test_start_time_on_gen,
                               generator_entry[i].test_end_time_on_gen);
  }
}

static inline void process_start_msg_by_client(Msg_com_con *mccptr)
{
  static int num_started = 0;
  int chk_num_nvm_per_gen_vary = 0;
  static int test_got_started = 0;
  char cmd[1024 + 1];
  sighandler_t prev_handler;
  char err_msg[1024]= "\0";

  NSDL2_MESSAGES(NULL, NULL, "Method called, sgrp_used_genrator_entries = %d, num_started = %d, num_process = %d for control connection.", 
                                sgrp_used_genrator_entries, num_started, global_settings->num_process);

  /*BUG 67668: This is dirty fix to avoid gen connection after start of test*/
  if(loader_opcode == MASTER_LOADER && test_got_started) {
    end_test_run_msg_to_client(mccptr);
    close_msg_com_con(mccptr);
    return;
  }
  if (msg->top.internal.avg_time_size != g_avgtime_size) {
    NS_EXIT(-1, CAV_ERR_1060048, msg->top.internal.avg_time_size, mccptr->ip, g_avgtime_size);
  }
  num_started++;
  
  NSDL3_MESSAGES(NULL, NULL, "[Control Connection]: Received START_MSG_BY_CLIENT from client id = %d, testrun number = %d, %s", 
            msg->top.internal.child_id, msg->top.internal.testidx, msg_com_con_to_str(mccptr));
  NSTL1(NULL, NULL, "[Control Connection]: Received START_MSG_BY_CLIENT from client <id:%d name:%s>, testrun number = %d, %s", 
            msg->top.internal.child_id, (loader_opcode == MASTER_LOADER)?(char *)generator_entry[msg->top.internal.child_id].gen_name:"NVM",
            msg->top.internal.testidx, msg_com_con_to_str(mccptr));
  //TODO: This may not be needed as we should not start test on any error
  if(loader_opcode == MASTER_LOADER) {
    generator_entry[msg->top.internal.child_id].testidx = msg->top.internal.testidx; //bug 55694: saving testidx
    fprint2f(console_fp, rfp, "Client (%s) Test Run Number = %d\n", mccptr->ip, msg->top.internal.testidx);

    if (global_settings->max_num_nvm_per_generator != 0 && 
        (global_settings->max_num_nvm_per_generator != msg->top.internal.num_nvm_per_generator)) 
      chk_num_nvm_per_gen_vary++ ;

    find_max_num_nvm_per_generator(mccptr, msg, num_started);
    if (chk_num_nvm_per_gen_vary == 1) 
      NSTL1_OUT(NULL, NULL, "[Control Connection]: Number of NVM %d received from generator %s, whereas maximum number of NVMs %d running in other generators.\n"
                "The generators are running with different number of NVMs, in this case scheduling may be affected.\n", 
                  msg->top.internal.num_nvm_per_generator, generator_entry[msg->top.internal.child_id].gen_name, 
                     global_settings->max_num_nvm_per_generator);
    prev_handler = signal( SIGCHLD, SIG_IGN);
    snprintf(cmd, 1024, "nohup %s/bin/nsu_get_gen_info 0 %d %s %s >/dev/null 2>&1 &", g_ns_wdir, testidx,
                 generator_entry[msg->top.internal.child_id].gen_name, global_settings->ctrl_server_ip);
    NSTL1(NULL, NULL, "[Control Connection]: nsu_get_gen_info cmd = %s", cmd);
    if(nslb_system(cmd,1,err_msg))
      NSTL1(NULL, NULL, "[Control Connection]: unable to get info from generator %s, ip = %s",
                         generator_entry[msg->top.internal.child_id].gen_name,
                         generator_entry[msg->top.internal.child_id].IP);
    (void) signal( SIGCHLD, prev_handler);
  }

  mccptr->nvm_index = msg->top.internal.child_id;
  INC_CHILD_STATUS_COUNT(mccptr->nvm_index);
  NSDL3_MESSAGES(NULL, NULL, "Check whether parent got START message from all childs, num_started_gen  = %d,"
                      " tot_num_gen = %d, num_connected = %d, status mask = %s", num_started, global_settings->num_process, 
                      g_data_control_var.num_connected, nslb_show_bitflag(g_child_status_mask));
  if (num_started == g_data_control_var.num_connected){
    //Reset num_started here as in goal based scenarios it will never reset
    num_started = 0;
    test_got_started = 1;
    NSDL3_MESSAGES(NULL, NULL, "All Generators/NVms have been started successfully."); 
    // In case of netcloud when we will get START_MSG_BY_CLIENT by all the nvms, then we will send START_MSG_BY_CLIENT to controller
    if(loader_opcode == CLIENT_LOADER){
      NSTL1(NULL, NULL, "Sending START_MSG_BY_CLIENT message to controller"); 
      send_msg_to_master(master_fd, START_MSG_BY_CLIENT, CONTROL_MODE);
      return;
    }
    //setting the start_phase
    set_scheduler_start_flag();

    if(!g_debug_script)
      fprint2f(console_fp, rfp, "Got all start messages from generators/children in %u msec\n", get_ms_stamp() - start_msg_ts);
   //Initalizing last_prgrss_rpt_time here becasue in RBU generator were taking much time to start the test, because of this 
   //Controller was ignoring generators which are taking time. Now we are initializing here so that controller dont ignore the generator
    if(loader_opcode == MASTER_LOADER){
      int gen_idx;
      for(gen_idx = 0; gen_idx < global_settings->num_process; gen_idx++)
      {
        generator_entry[gen_idx].last_prgrss_rpt_time = time(NULL) * 1000;
        strcpy(generator_entry[gen_idx].test_start_time_on_gen, get_relative_time());
      }
    }

    if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO) {
      if (scenario_schedule->phase_array[0].phase_cmd.start_phase.dependent_grp == -1) {
        send_schedule_phase_start(scenario_schedule, -1, 0);
        /*Fix done for bug#7763, when applied pause resume feature in START phase, phase name was blank.
          Here scenario_schedule->phase_idx was never initialized, hence added code to initialising 
          phase index field with 0 as it will always be first phase*/   
        scenario_schedule->phase_idx = 0;  
      }
    } else if (global_settings->schedule_by == SCHEDULE_BY_GROUP) {
      int i;
      for (i = 0; i < total_runprof_entries; i++) {
        if (group_schedule[i].phase_array[0].phase_cmd.start_phase.dependent_grp == -1) {
          send_schedule_phase_start(&(group_schedule[i]), i, 0);
          //Initialising with 0 as it will always be first phase of this group 
          group_schedule[i].phase_idx = 0;
        }
      }
    }
    //start nia_file_aggregator only if netcloud test and reader_run_mode is > 0
    if(loader_opcode == MASTER_LOADER && global_settings->reader_run_mode)
      init_component_rec_and_start_nia_fa(loader_opcode);
    
    #ifdef NS_DEBUG_ON
    if(loader_opcode == MASTER_LOADER)
      dump_generator_entry();  
    #endif
  }
}

void reset_global_avg(int gen_id)
{ 
   //Resetting all avg of killed generator
   init_avgtime (g_cur_avg[gen_id + 1], 0);
   init_avgtime (g_next_avg[gen_id + 1], 0);
   init_avgtime (g_end_avg[gen_id + 1], 0);

   init_cavgtime (g_cur_finished[gen_id + 1], 0);
   init_cavgtime (g_next_finished[gen_id + 1], 0);
}

static inline void process_finish_tesrun_msg(Msg_com_con *mccptr)
{
  NSDL2_MESSAGES(NULL, NULL, "Method called, Received FINISH_TESRUN_MSG from master. %s", msg_com_con_to_str(mccptr));
  handle_parent_sigusr1(-1);
}
//In case of stopping test immediately, generators need to call handle_parent_sigrtmin1 function
static inline void process_finish_tesrun_immediate_msg(Msg_com_con *mccptr)
{
  NSDL2_MESSAGES(NULL, NULL, "Method called, Received FINISH_TEST_RUN_IMMEDIATE_MESSAGE from master. %s", msg_com_con_to_str(mccptr));
  handle_parent_sigrtmin1(-1);
}

inline int  decrease_msg_counters(int child_id, int is_fin_report)
{
  int idx, group_idx;
  Schedule *schedule;

  NSDL2_MESSAGES(NULL, NULL, "Method called, Child %d, finish report flag %d, status mask=%s",
                             child_id, is_fin_report, nslb_show_bitflag(g_child_status_mask));
  NSDL2_MESSAGES(NULL, NULL, "vuser mask=%s", nslb_show_bitflag(vuser_client_mask));
  NSDL2_MESSAGES(NULL, NULL, "rtc mask=%s", nslb_show_bitflag(rtcdata->child_bitmask));
  DEC_CHILD_STATUS_COUNT(child_id);
  DEC_VUSER_MSG_COUNT(child_id, 2);
  DEC_CHECK_RTC_RETURN(child_id, 1);
  all_nvms_sent[child_id] = 0;
  if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO)
  {
    schedule = scenario_schedule;
    NSDL2_MESSAGES(NULL, NULL, "Before: Child %d, group status=%s", child_id, nslb_show_bitflag(g_child_group_status_mask[0]));
    NSDL2_MESSAGES(NULL, NULL, "ramp=%s", nslb_show_bitflag(schedule->ramp_bitmask));
    NSDL2_MESSAGES(NULL, NULL, "rampdone=%s", nslb_show_bitflag(schedule->ramp_done_bitmask));
    NSDL2_MESSAGES(NULL, NULL, "phase complete=%s", nslb_show_bitflag(schedule->bitmask));
    if(!is_fin_report)
    {
      DEC_STATUS_MSG_COUNT(child_id, 0);
      DEC_RAMPDONE_MSG_COUNT(child_id, 1, -1);
      if(nslb_check_bit_set(schedule->ramp_bitmask, child_id))
      {
        DEC_RAMP_MSG_COUNT(child_id, 1, -1);
        schedule->ramp_msg_to_expect--;
      }
    }
    DEC_SCHEDULE_MSG_COUNT(child_id, 1, -1);
    NSDL2_MESSAGES(NULL, NULL, "After: Child %d, group status=%s", child_id, nslb_show_bitflag(g_child_group_status_mask[0]));
    NSDL2_MESSAGES(NULL, NULL, "ramp=%s", nslb_show_bitflag(schedule->ramp_bitmask));
    NSDL2_MESSAGES(NULL, NULL, "rampdone=%s", nslb_show_bitflag(schedule->ramp_done_bitmask));
    NSDL2_MESSAGES(NULL, NULL, "phase complete=%s", nslb_show_bitflag(schedule->bitmask));
  }
  else
  {
    for (idx = 0; idx <total_runprof_entries; idx++)
    {
      if(loader_opcode == MASTER_LOADER)
        group_idx = find_group_idx_using_gen_idx(child_id, idx);
      else
        group_idx = idx;

      if(group_idx != -1)
      {
        schedule = &(group_schedule[group_idx]);
        NSDL2_MESSAGES(NULL, NULL, "Before: Child %d, group = %d, group status=%s", child_id, group_idx,
                                   nslb_show_bitflag(g_child_group_status_mask[0]));
        NSDL2_MESSAGES(NULL, NULL, "ramp=%s", nslb_show_bitflag(schedule->ramp_bitmask));
        NSDL2_MESSAGES(NULL, NULL, "rampdone=%s", nslb_show_bitflag(schedule->ramp_done_bitmask));
        NSDL2_MESSAGES(NULL, NULL, "phase complete=%s", nslb_show_bitflag(schedule->bitmask));

        if(!is_fin_report)
        {
          DEC_STATUS_MSG_COUNT(child_id, group_idx);
          DEC_RAMPDONE_MSG_COUNT(child_id, 1, group_idx);
          if(nslb_check_bit_set(schedule->ramp_bitmask, child_id))
          {
            DEC_RAMP_MSG_COUNT(child_id, 1, group_idx);
            schedule->ramp_msg_to_expect--;
          }
        }
        DEC_SCHEDULE_MSG_COUNT(child_id, 1, group_idx);
        NSDL2_MESSAGES(NULL, NULL, "Before: Child %d, group = %d, group status=%s", child_id, group_idx,
                                   nslb_show_bitflag(g_child_group_status_mask[0]));
        NSDL2_MESSAGES(NULL, NULL, "ramp=%s", nslb_show_bitflag(schedule->ramp_bitmask));
        NSDL2_MESSAGES(NULL, NULL, "rampdone=%s", nslb_show_bitflag(schedule->ramp_done_bitmask));
        NSDL2_MESSAGES(NULL, NULL, "phase complete=%s", nslb_show_bitflag(schedule->bitmask));
      }
    }
  }
  NSDL2_MESSAGES(NULL, NULL, "Method exit, Child %d, status mask=%s", child_id, nslb_show_bitflag(g_child_status_mask));
  NSDL2_MESSAGES(NULL, NULL, "vuser mask=%s", nslb_show_bitflag(vuser_client_mask));
  NSDL2_MESSAGES(NULL, NULL, "child mask=%s", nslb_show_bitflag(rtcdata->child_bitmask));
  return 0;
}

/* Update progress report variables and total number of generators, 
 * because a generator got killed and we wont be receiving any samples from this generator*/
//inline int update_vars_to_continue_ctrl_test(char *gen_ip)
inline int update_vars_to_continue_ctrl_test(int gen_id)
{
  int num_gen_running_expected;

  NSDL1_MESSAGES(NULL, NULL, "Method called, num_active = %d, num_pge = %d, gen_id = %d",
                             g_data_control_var.num_active, g_data_control_var.num_pge, gen_id);
  //Bug: 39448, if generator is already marked then return
  if ((gen_id < 0) || (generator_entry[gen_id].flags & IS_GEN_INACTIVE))
  {
    NSTL1(NULL, NULL, "Generator id %d already marked as killed", gen_id);
    return -1;
  }
  generator_entry[gen_id].flags |= IS_GEN_INACTIVE;
  generator_entry[gen_id].flags &= ~IS_GEN_ACTIVE;
  decrease_sample_count(gen_id, 0);
  //count total number of generator is ignored
  UPDATE_GLOB_DATA_CONTROL_VAR(g_data_control_var.num_active--; g_data_control_var.num_pge--; g_data_control_var.total_killed_generator++; g_data_control_var.total_killed_gen++) 
  //Last generator got killed
  if ((global_settings->num_process == g_data_control_var.total_killed_gen) || !g_data_control_var.num_active)
  {
    NSTL1_OUT(NULL, NULL, "All generators got terminated. Hence stopping the test...\n");
    kill_all_children((char *)__FUNCTION__, __LINE__, __FILE__);
    parent_save_data_before_end();
    save_status_of_test_run(TEST_RUN_STOPPED_DUE_TO_SYSTEM_ERRORS);
    NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_WARNING,
                __FILE__, (char*)__FUNCTION__,
               "All generators got terminated. Hence stopping the test...");
    NS_EXIT(-1, CAV_ERR_1060045);
  }

  NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_WARNING,
         __FILE__, (char*)__FUNCTION__,
         "Received test termination event from generator name = %s, ip = %s, fd = %d. Test will continue...",
         generator_entry[gen_id].gen_name, generator_entry[gen_id].IP, g_msg_com_con[gen_id].fd);
 
  NSTL1_OUT(NULL, NULL, "Discarding unhealthy generator:%s", generator_entry[gen_id].gen_name);
  NSDL2_MESSAGES(NULL, NULL, "Generator id = %d", gen_id);

  if(global_settings->con_test_gen_fail_setting.percent_running)
  {
    NSTL1(NULL, NULL, "Limit Running generator percentage is %d", global_settings->con_test_gen_fail_setting.percent_running);
    num_gen_running_expected = (global_settings->con_test_gen_fail_setting.percent_running * global_settings->num_process)/100;
    NSTL1(NULL, NULL, "num_active %d, num_gen_running_expected %d", g_data_control_var.num_active, num_gen_running_expected);
    if(g_data_control_var.num_active < num_gen_running_expected)
    {
      kill_all_children((char *)__FUNCTION__, __LINE__, __FILE__);
      NS_EXIT(-1, CAV_ERR_1060046, global_settings->con_test_gen_fail_setting.percent_running, global_settings->num_process);
    }
  }

  reset_global_avg(gen_id);
  //decrease_msg_counters(gen_id, 0);
  NSTL1(NULL, NULL, "Modified progress report variables: num_active = %d, num_pge = %d",
                    g_data_control_var.num_active, g_data_control_var.num_pge);
  HANDLE_QUEUE_COMPLETE();
  return 0;
}

// reset all variables which are shared by ramp up & ramp down, when Ist ramp down msg recieved 
inline void init_vars_of_ramp_up_down(Schedule *schedule)
{
   NSDL2_MESSAGES(NULL, NULL, "Method called");

   schedule->cum_total_ramp = schedule->total_ramp = 0;
   schedule->cum_total_ramp_done = schedule->total_ramp_done = 0;
   ramp_down_first_msg_rcv = 0; /* TODO:BHAV:XXXXX */

   schedule->ramp_start_time = get_ms_stamp(); 
   schedule->ramp_done_per_cycle_count = 0;

}

static inline void update_vars_for_ramp_msg(Schedule *schedule, parent_msg *msg, Msg_com_con *mccptr, char *ramp_str)
{
  int master_fd = -1;
  int grp_type = get_group_mode(msg->top.internal.grp_idx);

  NSDL3_MESSAGES(NULL, NULL, "Method called");

  // Neeraj - Is v_port_table initialized for Master?
  //for (process_id = 0; process_id < global_settings->num_process; process_id++)
  //total_ramp += v_port_table[process_id].ramping_done;

  /**
   * This is to handle rampup done msg received from some children before rampup is done for other children 
   * In case some child has stopped sending rampup message (i.e. already rampup done), remaining children
   * are still sending ramup message. But we still add up equal to num_proc.
   */

  schedule->total_ramp += schedule->prev_total_ramp_done;
  schedule->cum_total_ramp += schedule->prev_cum_total_ramp_done;

  /* Here we decrement msg_expected because this marks the cycle completion.
   * And reset the rampup done per-cycle count 
  if (schedule->ramp_done_per_cycle_count != 0) {
    schedule->ramp_msg_to_expect -= schedule->ramp_done_per_cycle_count;
    schedule->ramp_done_per_cycle_count = 0;
  } */

  if(loader_opcode == CLIENT_LOADER)  // Came from child
  {
    msg->top.internal.num_users = schedule->total_ramp;
    msg->top.internal.cum_users = schedule->cum_total_ramp;
    NSDL3_MESSAGES(NULL, NULL, "Received RAMP%s_MESSAGE from all children. Sending RAMPUP_MESSAGE to master. total_ramp = %d, from ip = %s", ramp_str, msg->top.internal.num_users, mccptr->ip);
    forward_msg_to_master(master_fd, msg, sizeof(parent_child));
    // DL_ISSUE
  }

  /* RAMPING UP: 100 (total 500) users */     
  if (msg->top.internal.grp_idx == -1) {

    if (schedule->phase_array[msg->top.internal.phase_idx].phase_type == SCHEDULE_PHASE_RAMP_UP) {
      if(grp_type != TC_FIX_CONCURRENT_USERS) {
        NSTL1(NULL, NULL, "RAMPING %s: %0.2f (total %0.2f) sessions per minute ...",
                       ramp_str, 
                       (double)(schedule->total_ramp / ((double)SESSION_RATE_MULTIPLE)),
                       (double)(schedule->cum_total_ramp / ((double)SESSION_RATE_MULTIPLE)));
      } else
        NSTL1(NULL, NULL, "RAMPING %s: %d (total %d) users ...", 
                         ramp_str, 
                         schedule->total_ramp,
                         schedule->cum_total_ramp);

    } else if (schedule->phase_array[msg->top.internal.phase_idx].phase_type ==  SCHEDULE_PHASE_RAMP_DOWN) {
      if(grp_type != TC_FIX_CONCURRENT_USERS) {
        NSTL1(NULL, NULL, "RAMPING %s: %0.2f (total %0.2f) sessions per minute ...",
                       ramp_str, 
                       (double)(schedule->total_ramp / ((double)SESSION_RATE_MULTIPLE)),
                       (double)(schedule->cum_total_ramp / ((double)SESSION_RATE_MULTIPLE)));
      } else
        NSTL1(NULL, NULL, "RAMPING %s: %d (total %d) users ...", 
                       ramp_str, 
                       schedule->total_ramp,
                       schedule->cum_total_ramp);
    } else {
      /* Can not be. */
    }
  } else {                  /* Group based */
    /* RAMPING UP (Group G1 Phase RampUp1): 100 (total 500) users */
    /* RAMPING UP (Group G1 Phase RampUp1): 100.12 (total 500) sessions/min */
    
    if (schedule->phase_array[msg->top.internal.phase_idx].phase_type == SCHEDULE_PHASE_RAMP_UP) {
      if(grp_type != TC_FIX_CONCURRENT_USERS) {
        NSTL1(NULL, NULL, "RAMPING %s (Group '%s', Phase '%s'): %0.2f (total %0.2f) sessions per minute ...",ramp_str, 
                       runprof_table_shr_mem[msg->top.internal.grp_idx].scen_group_name,
                       schedule->phase_array[msg->top.internal.phase_idx].phase_name,
                       (double)(schedule->total_ramp / ((double)SESSION_RATE_MULTIPLE)),
                       (double)(schedule->cum_total_ramp / ((double)SESSION_RATE_MULTIPLE)));
      } else
        NSTL1(NULL, NULL, "RAMPING %s (Group '%s', Phase '%s'): %d (total %d) users ...", 
                       ramp_str, 
                       runprof_table_shr_mem[msg->top.internal.grp_idx].scen_group_name,
                       schedule->phase_array[msg->top.internal.phase_idx].phase_name,
                       schedule->total_ramp,
                       schedule->cum_total_ramp);
    } else if (schedule->phase_array[msg->top.internal.phase_idx].phase_type ==  SCHEDULE_PHASE_RAMP_DOWN) {
      if (grp_type != TC_FIX_CONCURRENT_USERS) {
        NSTL1(NULL, NULL, "RAMPING %s (Group '%s', Phase '%s'): %0.2f (total %0.2f) sessions per minute ...",ramp_str, 
                       runprof_table_shr_mem[msg->top.internal.grp_idx].scen_group_name,
                       schedule->phase_array[msg->top.internal.phase_idx].phase_name,
                       (double)(schedule->total_ramp / ((double)SESSION_RATE_MULTIPLE)),
                       (double)(schedule->cum_total_ramp / ((double)SESSION_RATE_MULTIPLE)));
      } else
        NSTL1(NULL, NULL, "RAMPING %s (Group '%s', Phase '%s'): %d (total %d) users ...", 
                       ramp_str, 
                       runprof_table_shr_mem[msg->top.internal.grp_idx].scen_group_name,
                       schedule->phase_array[msg->top.internal.phase_idx].phase_name,
                       schedule->total_ramp,
                       schedule->cum_total_ramp);
    } else {
      /* Can not be. */
    }
  }

  schedule->total_ramp = 0;
  schedule->cum_total_ramp = 0;
}

// process_ramp_msg used for ramp up & ramp down both only ramp_str differs which have substring "UP" or "DOWN"
// When the rampup msg for test run has been recived, processing rampup msg
static inline void process_ramp_msg(Msg_com_con *mccptr)
{
  char ramp_str[5];
  Schedule *schedule;
  int is_bit_already_set;
  int cnt_bmask;

  if(msg->top.internal.opcode == RAMPUP_MESSAGE)
  {
    strcpy(ramp_str, "UP");
  }
  else if(msg->top.internal.opcode == RAMPDOWN_MESSAGE) {
    strcpy(ramp_str, "DOWN");
  }

  NSDL2_MESSAGES(NULL, NULL, "Method called, ramp_str = %s", ramp_str);

  if (msg->top.internal.grp_idx == -1) {
    schedule = scenario_schedule;
  } else {
    schedule = &(group_schedule[msg->top.internal.grp_idx]);
  }

  /* Check if we are in appropriate phase */
  if ((msg->top.internal.opcode == RAMPUP_MESSAGE) &&
      schedule->phase_array[msg->top.internal.phase_idx].phase_type != SCHEDULE_PHASE_RAMP_UP) {
      NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_WARNING,
                                __FILE__, (char*)__FUNCTION__,
       		               "Ignoring ramp up msg from child %d since it came in wrong phase (%s).",
                      	       msg->top.internal.child_id, 
                      	       schedule->phase_array[schedule->phase_idx].phase_name);
    return;
  } 
  
  if ((msg->top.internal.opcode == RAMPDOWN_MESSAGE) &&
      schedule->phase_array[msg->top.internal.phase_idx].phase_type != SCHEDULE_PHASE_RAMP_DOWN) {
      NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_WARNING,
                                __FILE__, (char*)__FUNCTION__,
                      		"Ignoring ramp down msg from child %d since it came in wrong phase (%s).", 
 	                        msg->top.internal.child_id, 
                      		schedule->phase_array[msg->top.internal.grp_idx].phase_name);
    return;
  }


  cnt_bmask = nslb_count_bitflag(schedule->ramp_bitmask);
  is_bit_already_set = nslb_check_bit_set(schedule->ramp_bitmask, msg->top.internal.child_id);

  if (is_bit_already_set) {
    schedule->total_ramp += msg->top.internal.num_users; 
    schedule->cum_total_ramp += msg->top.internal.cum_users; 
  } else {
    NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_WARNING,
                                __FILE__, (char*)__FUNCTION__,
				"Ignoring ramp msg from child %d since"
				" bitmask already reset.",
				msg->top.internal.child_id);

    NSDL3_MESSAGES(NULL, NULL, "grp = %d, count = %d, expected = %d, num_proc = %d, cycle_done = %d, "
                               "total_ramp_done = %d", msg->top.internal.grp_idx, cnt_bmask,
                               schedule->ramp_msg_to_expect, global_settings->num_process, 
                               schedule->ramp_done_per_cycle_count, schedule->total_ramp_done);

  }

  NSTL2(NULL, NULL, "Received RAMP%s_MESSAGE from client '%d'. total_ramp = %d,"
                    " from ip = %s", ramp_str, msg->top.internal.child_id, schedule->total_ramp, mccptr->ip);

  if ( get_max_log_level() >= 1 )
  {

    
    if ((cnt_bmask - 1) == 0) {  /* First msg of the cycle */
      schedule->prev_total_ramp_done = schedule->total_ramp_done;
      schedule->prev_cum_total_ramp_done = schedule->cum_total_ramp_done;
    }
    
    DEC_RAMP_MSG_COUNT(msg->top.internal.child_id, 0, msg->top.internal.grp_idx);
    cnt_bmask = nslb_count_bitflag(schedule->ramp_bitmask);
    NSDL3_SCHEDULE(NULL, NULL, "Received RAMP%s_MESSAGE Group=%d, Count=%d, Expected=%d, Total=%d,"
                  " bitflag=%s, total_ramp_done = %d", ramp_str, msg->top.internal.grp_idx, cnt_bmask,
                  schedule->ramp_msg_to_expect, global_settings->num_process,
                  nslb_show_bitflag(schedule->ramp_bitmask), schedule->total_ramp_done);
    if (CHECK_ALL_RAMP_MSG_DONE)
    {
      /* Reset bitmask */
      INC_RAMP_MSG_COUNT(msg->top.internal.grp_idx);
      update_vars_for_ramp_msg(schedule, msg, mccptr, ramp_str);
    
    } else if (cnt_bmask > schedule->ramp_msg_to_expect) {
      NSTL1(NULL, NULL, "Number of RAMP%s_MESSAGE received (%d) is more than expected number of"
                            " ramp%s messages (%d)\n", ramp_str, cnt_bmask, ramp_str, schedule->ramp_msg_to_expect);
    }
  }
}

static inline void update_vars_for_ramp_done_msg(Schedule *schedule, parent_msg *msg, Msg_com_con *mccptr, char *ramp_str)
{
  int grp_type = get_group_mode(msg->top.internal.grp_idx);
  int master_fd = -1;

  if(loader_opcode == CLIENT_LOADER)  // Came from child
  {
    msg->top.internal.num_users = schedule->total_ramp_done;
    msg->top.internal.cum_users = schedule->cum_total_ramp_done;
    NSDL3_MESSAGES(NULL, NULL, "Received RAMP%s_DONE_MESSAGE from all children. Sending"
                               " RAMP%s_DONE_MESSAGE to master", ramp_str, ramp_str);
    NSTL2(NULL, NULL, "Received RAMP%s_DONE_MESSAGE from all children. Sending RAMP%s_DONE_MESSAGE"
                      " to master", ramp_str, ramp_str);
    forward_msg_to_master(master_fd, msg, sizeof(parent_child));
    // DL_ISSUE
    if (grp_type != TC_FIX_CONCURRENT_USERS) {
      NSTL1(NULL, NULL, "RAMP%s DONE: %0.2f sessions per minute", ramp_str,
                        (double)(schedule->total_ramp_done / ((double)SESSION_RATE_MULTIPLE)));
    } else
      NSTL1(NULL, NULL, "RAMP%s DONE: %d users", ramp_str, schedule->total_ramp_done);
  }
  else {
    NSTL2(NULL, NULL, "Received RAMP%s_DONE_MESSAGE from all children.", ramp_str);
    u_ns_ts_t local_now = get_ms_stamp() - schedule->ramp_start_time;
    if (msg->top.internal.grp_idx == -1) {
      if (grp_type != TC_FIX_CONCURRENT_USERS) {
        NSTL1(NULL, NULL, "RAMP%s DONE: %0.2f sessions per minute in (%2.2d:%2.2d:%2.2d HH:MM:SS)", ramp_str, 
                       (double)(schedule->total_ramp_done / 
                                ((double)SESSION_RATE_MULTIPLE)),
                       (local_now/1000) / 3600,
                       ((local_now/1000) % 3600) / 60,
                       ((local_now/1000) % 3600) % 60);
      } else
        NSTL1(NULL, NULL, "RAMP%s DONE: %d users in (%2.2d:%2.2d:%2.2d HH:MM:SS)", ramp_str, schedule->total_ramp_done,
                       (local_now/1000) / 3600,
                       ((local_now/1000) % 3600) / 60,
                       ((local_now/1000) % 3600) % 60);
    } else {
      if (grp_type != TC_FIX_CONCURRENT_USERS) {
        NSTL1(NULL, NULL, "RAMP%s DONE (Group '%s', Phase '%s'): %0.2f sessions per minute in"
                       " (%2.2d:%2.2d:%2.2d HH:MM:SS)", ramp_str, 
                       runprof_table_shr_mem[msg->top.internal.grp_idx].scen_group_name,
                       schedule->phase_array[msg->top.internal.phase_idx].phase_name,
                       (double)(schedule->total_ramp_done / 
                                ((double)SESSION_RATE_MULTIPLE)),
                       (local_now/1000) / 3600,
                       ((local_now/1000) % 3600) / 60,
                       ((local_now/1000) % 3600) % 60);
      } else
        NSTL1(NULL, NULL, "RAMP%s DONE (Group '%s', Phase '%s'): %d users in (%2.2d:%2.2d:%2.2d HH:MM:SS)", ramp_str, 
                       runprof_table_shr_mem[msg->top.internal.grp_idx].scen_group_name,
                       schedule->phase_array[msg->top.internal.phase_idx].phase_name,
                       schedule->total_ramp_done,
                       (local_now/1000) / 3600,
                       ((local_now/1000) % 3600) / 60,
                       ((local_now/1000) % 3600) % 60);
    }
  }
}
//process_ramp_done_msg used for ramp up & ramp down both only ramp_str differs which have substring "UP" or "DOWN"
// When the rampup_done msg for test run has been recived, processing rampup_done msg
static inline void process_ramp_done_msg(Msg_com_con *mccptr)
{
  char ramp_str[5] = "\0";
  Schedule *schedule;

  if (msg->top.internal.grp_idx == -1)
    schedule = scenario_schedule;
  else {
    schedule = &(group_schedule[msg->top.internal.grp_idx]);
  }

  /* Check if we are in appropriate phase */
  if (msg->top.internal.opcode == RAMPUP_DONE_MESSAGE) {
    if(schedule->phase_array[msg->top.internal.phase_idx].phase_type != SCHEDULE_PHASE_RAMP_UP) {
      NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_WARNING,
                                __FILE__, (char*)__FUNCTION__,
                     		"Ignoring ramp up done msg from child %d"
				" since it came in wrong phase (%s).",
                      		msg->top.internal.child_id, 
                      		schedule->phase_array[schedule->phase_idx].phase_name);
      return;
    }
    strcpy(ramp_str, "UP");
    NSDL2_MESSAGES(NULL, NULL, "ramp_start_time = %lu", schedule->ramp_start_time);
  } 
  
  if (msg->top.internal.opcode == RAMPDOWN_DONE_MESSAGE) {
    if(schedule->phase_array[msg->top.internal.phase_idx].phase_type != SCHEDULE_PHASE_RAMP_DOWN) {
      NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_WARNING,
                                __FILE__, (char*)__FUNCTION__,
       		                "Ignoring ramp down done msg from child %d"
				" since it came in wrong phase (%s).", 
                      		msg->top.internal.child_id, 
                      		schedule->phase_array[schedule->phase_idx].phase_name);
      return;
    }
    strcpy(ramp_str, "DOWN");
    if(!ramp_down_first_msg_rcv)  //if no ramp down msg rcv than reset process_ramp_msg will not be called to reset vars
    {
      ramp_down_first_msg_rcv = 1;
    }
  }
  
  NSDL2_MESSAGES(NULL, NULL, "Method called, ramp_str = %s", ramp_str);

  //if (cnt_bmask == 0)           /* Cycle not yet started */
  /* Here since we are taking indivitual reading for children, we can not reply on
   * Cycle but on individual child's status. */
  if (nslb_check_bit_set(schedule->ramp_bitmask, msg->top.internal.child_id))
  {
    /*Since, we received RAMPUP/DOWN_DONE_MESSAGE prior to ramp up/down message, reset bit*/
    DEC_RAMP_MSG_COUNT(msg->top.internal.child_id, 0, msg->top.internal.grp_idx);
    //schedule->ramp_msg_to_expect--;
  }
  else
    schedule->ramp_done_per_cycle_count++;

  schedule->total_ramp_done += msg->top.internal.num_users;
  schedule->cum_total_ramp_done += msg->top.internal.cum_users;

  if (get_max_log_level() >= 1)
  {
    NSDL2_MESSAGES(NULL, NULL, "grp = %d, child id = %d, ramp mask = %s", msg->top.internal.grp_idx,
                                msg->top.internal.child_id, nslb_show_bitflag(schedule->ramp_bitmask));
    //In case of NC: On Controller total_killed_nvms always will be 0
    //In case of Generator/Standalone: total_killed_gen always will be 0
    DEC_STATUS_MSG_COUNT(msg->top.internal.child_id, msg->top.internal.grp_idx);
    DEC_RAMPDONE_MSG_COUNT(msg->top.internal.child_id, 0, msg->top.internal.grp_idx);
    if (CHECK_ALL_RAMPDONE_MSG_DONE)
    {
      /* set status bitmask, it will be set when start phase sent*/
      //INC_RAMPDONE_MSG_COUNT(msg->top.internal.grp_idx);
      update_vars_for_ramp_done_msg(schedule, msg, mccptr, ramp_str);
    }
  }
}

static Msg_com_con *get_msg_com_ptr(int child_idx)
{
  int i;
  for(i = 0; i < global_settings->num_process; i++)
  {
    if(g_msg_com_con[i].nvm_index == child_idx)
     return &g_msg_com_con[i];
  }
  //Should not reach here
  return NULL;
}

void handle_child_failure(Schedule *schedule, int grp_idx, int child_idx, int msg_type)
{
  char ramp_str[5];
  parent_msg msg;
  Msg_com_con *mccptr = get_msg_com_ptr(child_idx);

  NSTL1(NULL, NULL, "Handling phase completion on child failure, Id = %d, Group = %d, msg_type = %d", child_idx, grp_idx, msg_type);
  switch(msg_type)
  {
    case SCHEDULE_MSG:
      if(CHECK_ALL_SCHEDULE_MSG_DONE)
        check_before_sending_nxt_phase_only_from_controller(schedule->phase_idx, grp_idx, child_idx);
      break;

    case RAMP_MSG:
      if(CHECK_ALL_RAMP_MSG_DONE)
      {
        //TODO:
        msg.top.internal.grp_idx = grp_idx;
        msg.top.internal.child_id = child_idx;
        msg.top.internal.phase_idx = schedule->phase_idx;
        if(schedule->phase_array[schedule->phase_idx].phase_type == SCHEDULE_PHASE_RAMP_UP)
        {
          strcpy(ramp_str, "UP");
          msg.top.internal.opcode = RAMPUP_MESSAGE;
        }
        else if(schedule->phase_array[schedule->phase_idx].phase_type == SCHEDULE_PHASE_RAMP_DOWN)
        {
          strcpy(ramp_str, "DOWN");
          msg.top.internal.opcode = RAMPDOWN_MESSAGE;
        }
        update_vars_for_ramp_msg(schedule, &msg, mccptr, ramp_str);
      }
      break;

    case RAMPDONE_MSG:
      if(CHECK_ALL_RAMPDONE_MSG_DONE)
      {
        //TODO:
        msg.top.internal.grp_idx = grp_idx;
        msg.top.internal.child_id = child_idx;
        msg.top.internal.phase_idx = schedule->phase_idx;
        if(schedule->phase_array[schedule->phase_idx].phase_type == SCHEDULE_PHASE_RAMP_UP)
        {
          strcpy(ramp_str, "UP");
          msg.top.internal.opcode = RAMPUP_DONE_MESSAGE;
        }
        else if(schedule->phase_array[schedule->phase_idx].phase_type == SCHEDULE_PHASE_RAMP_DOWN)
        {
          strcpy(ramp_str, "DOWN");
          msg.top.internal.opcode = RAMPDOWN_DONE_MESSAGE;
        }
        update_vars_for_ramp_done_msg(schedule, &msg, mccptr, ramp_str);
      }
      break;

    default:
      NSTL1(NULL, NULL, "handle_child_failure(): Invalid message type %d", msg_type);
      break;
  }
}

// The ready_to_collect_data msg for test run has been recived, processing ready_to_collect_data msg
static inline void process_ready_to_collect_data_msg(Msg_com_con *mccptr)
{
  int k;

  NSDL2_MESSAGES(NULL, NULL, "Method called");
  num_ready++;

  if(loader_opcode == MASTER_LOADER) {
    NSDL3_MESSAGES(NULL, NULL, "Received READY_TO_COLLECT_DATA msg from client_id = %d, num_ready = %d, from ip = %s",
	      msg->top.internal.child_id, num_ready, mccptr->ip);
    NSTL1(NULL, NULL, "Received READY_TO_COLLECT_DATA msg from client_id = %d, num_ready = %d, from ip = %s",
	      msg->top.internal.child_id, num_ready, mccptr->ip);
  } else {
    NSDL3_MESSAGES(NULL, NULL, "Received READY_TO_COLLECT_DATA from child_id = %d, num_ready = %d, fro ip = %s", msg->top.internal.child_id, num_ready, mccptr->ip);
  }

  if(loader_opcode != MASTER_LOADER)
  {
    memcpy(&v_port_table[msg->top.internal.child_id].child_addr, &cliaddr, sizeof(cliaddr));
    v_port_table[msg->top.internal.child_id].child_addr_len = addrlen;
  }

  if (num_ready == global_settings->num_process)
  {

    if(loader_opcode == MASTER_LOADER)
    {
      NSDL3_MESSAGES(NULL, NULL, "Netstorm is starting Run Time Phase");
      print2f_always(rfp, "Netstorm is starting Run Time Phase\n");
      NSDL3_MESSAGES(NULL, NULL, "Sending START_COLLECTING to all clients");
      NSTL1(NULL, NULL, "Sending START_COLLECTING to all clients");
      send_msg_to_all_clients(START_COLLECTING, 0);
      global_settings->test_runphase_start_time = get_ms_stamp();
    }
    else if (loader_opcode == CLIENT_LOADER)
    {  
      // DL_ISSUE
        print2f_always(rfp, "Netstorm is waiting for Run Time Phase Signal from master\n");

      NSDL3_MESSAGES(NULL, NULL, "Received READY_TO_COLLECT_DATA from all children. Sending READY_TO_COLLECT_DATA to master");
      NSTL1(NULL, NULL, "Received READY_TO_COLLECT_DATA from all children. Sending READY_TO_COLLECT_DATA to master");
      send_msg_to_master(master_fd, READY_TO_COLLECT_DATA, CONTROL_MODE);
    }
    else  // Stand-alone
    {
      int sent_msg = 0;
      parent_child ready_msg;

      NSDL3_MESSAGES(NULL, NULL, "Received READY_TO_COLLECT_DATA msg from all children. Sending START_COLLECTING to all children");
      ready_msg.opcode = START_COLLECTING;
      for (k = 0; k < global_settings->num_process; k++)
      {
        sent_msg++;

        // Note - Child_id here is not correct.
        NSDL3_MESSAGES(NULL, NULL, "Sending START_COLLECTING to child_id = %d", k);
        ready_msg.msg_len = sizeof(ready_msg) - sizeof(int);
	write_msg(&g_msg_com_con[k], (char *)&ready_msg, sizeof(ready_msg), 0, CONTROL_MODE);
      }
      global_settings->test_runphase_start_time = get_ms_stamp();
    }
  }
}

// The start_collecting msg for test run has been recived, processing start_collecting msg
/* This msg only comes from master */
static inline void process_start_collecting_msg(Msg_com_con *mccptr)
{
  int k;
  int sent_msg = 0;
  parent_child ready_msg;

  NSDL2_MESSAGES(NULL, NULL, "Method called");
  ready_msg.opcode = START_COLLECTING;

  NSDL3_MESSAGES(NULL, NULL, "Received START_COLLECTING from %s", mccptr->ip);
  /* How do we check if the msg came from master not the child */
  if (loader_opcode == MASTER_LOADER)
    print2f_always(rfp, "Netstorm is starting Run Time Phase\n");

  for (k = 0; k < global_settings->num_process; k++) {

    /*       Note - Child_id here is not correct. */
    sent_msg++;
    NSDL3_MESSAGES(NULL, NULL, "Sending START_COLLECTING to child_id = %d", k);
    ready_msg.msg_len = sizeof(ready_msg) - sizeof(int);
    write_msg(&g_msg_com_con[k], (char *)&ready_msg, sizeof(ready_msg), 0, CONTROL_MODE);
    NSDL3_MESSAGES(NULL, NULL, "Succesfully sent mesg to children (%d) from parent", msg->top.internal.child_id);
  }
  global_settings->test_runphase_start_time = get_ms_stamp();
}

int is_rampdown_in_progress()
{
  NSDL2_MESSAGES(NULL, NULL, "Method called");
  return ramp_down_first_msg_rcv;
}

inline void process_default_case_msg(Msg_com_con *mccptr, int th_flag)
{
  NSDL1_MESSAGES(NULL, NULL, "Method called");
  
  if(th_flag == DATA_MODE)
  {
    print_core_events((char*)__FUNCTION__, __FILE__,  "Received invalid message from child/client for data connection." 
           "child_id = %d, opcode = %d, from ip = %s",
            msg_dh->top.internal.child_id, msg_dh->top.internal.opcode, mccptr->ip);
  }else{
    print_core_events((char*)__FUNCTION__, __FILE__,  "Received invalid message from child/client for control connection." 
           "child_id = %d, opcode = %d, from ip = %s",
            msg->top.internal.child_id, msg->top.internal.opcode, mccptr->ip);
  }
}

inline void init_vars_for_each_test(int num_children)
{
  NSDL2_MESSAGES(NULL, NULL, "Method called");
  
  UPDATE_GLOB_DATA_CONTROL_VAR(g_data_control_var.num_active = g_data_control_var.num_pge = num_children)
  num_ready = 0; 
  cur_sample = 1; 
}

int open_connect_to_gui_server(int *server_fd, char *server_addr, int server_port)
{
  if ((*server_fd = nslb_udp_client(server_addr, server_port, 0)) < 0)
  {
    print_core_events((char*)__FUNCTION__, __FILE__,
                                   "NetStorm unable to create UDP socket to GUI (%s:%d) server."
                                   " error = %s", server_addr, server_port, nslb_strerror(errno) );
    *server_fd = -1;
  }
  return (*server_fd);
}

void send_pre_post_test_msg_to_tomcat()
{
  char *ptr;
  char buf[1024];
  if ((ptr = getenv("GUI_DATA")))
  {
    gui_data_seq = atoi(ptr);
    sprintf(buf, "logs/%s/gui.data", global_settings->tr_or_partition);
    gui_fp = fopen (buf, "w");
    if (!gui_fp) printf("Unable to open gui.data file (%s)\n", buf);
  }
  time_t ts = time(NULL);
  if (gui_fp)
  {
    Msg_data_hdr *msg_data_local_hdr_ptr = (Msg_data_hdr *) msg_data_ptr;
   
    if (msg_data_local_hdr_ptr->opcode == MSG_PRE_TEST_PKT) 
    {
      fprintf(gui_fp, "PRE START: %s\topcode: %6.0f\n\tseq_number: %6.0f\n",
      ctime(&ts), (msg_data_local_hdr_ptr->opcode), (msg_data_local_hdr_ptr->seq_no));
    }
    else if (msg_data_local_hdr_ptr->opcode == MSG_POST_TEST_PKT)
    {
      fprintf(gui_fp, "POST TEST: %s\topcode: %6.0f\n\tseq_number: %6.0f\n",
      ctime(&ts), (msg_data_local_hdr_ptr->opcode), (msg_data_local_hdr_ptr->seq_no));
    }
  }

  if (global_settings->gui_server_addr[0])
  {
    if(gui_fd == -1)
    {
      int ret;
      ret = open_connect_to_gui_server(&gui_fd, global_settings->gui_server_addr, global_settings->gui_server_port);
      if(ret == -1)
        return;
    }

    /* Netstorm sends Message Header only to gui since gui only requires header, rest of the data it reads from
    * rtgMessage.dat file */

    if (send(gui_fd, msg_data_ptr, sizeof(Msg_data_hdr), 0) != sizeof(Msg_data_hdr))
    {
      print_core_events((char*)__FUNCTION__, __FILE__,
                                       "NetStorm unable to send messgae data header to GUI (%s:%hd) server."
                                       " error = %s", global_settings->gui_server_addr, global_settings->gui_server_port, nslb_strerror(errno) );
      return;
    }
    else
      NSTL1(NULL, NULL, "successfully send the PRE_TEST message to GUI");
  }

}

// This mthd does the following sequentially
// 1 - Cretaes the rtg file
// 2 - Creates the data file
// 3 - Sends start msg to Server
//static inline void create_rtg_file_data_file_send_start_msg_to_server()
inline void create_rtg_file_data_file_send_start_msg_to_server(int rtgMessage_file_name_counter)
{
  NSDL2_MESSAGES(NULL, NULL, "Method called");
  // Bug - 41679
  //if ((get_max_log_level() >= 1) && (run_mode == NORMAL_RUN))
  if(run_mode == NORMAL_RUN)
  {
    // start_msg_hdr, for getting the value of opcode and all.
    char buf[1024];
    char old_buf[1024];
    char new_buf[1024];
    
    if (!(g_tsdb_configuration_flag & TSDB_MODE))
    {

      fill_msg_start_hdr ();         // in ns_fill_gdf.cr

      if (get_max_report_level() > 0)
      { 
        if(rtgMessage_file_name_counter == 0)
          sprintf(buf, "logs/%s/rtgMessage.dat", global_settings->tr_or_partition);
        else
          sprintf(buf, "logs/%s/rtgMessage.dat.%d", global_settings->tr_or_partition, rtgMessage_file_name_counter);

        rtg_fp = fopen (buf, "w");
        if (!rtg_fp)
        {
          printf("Unable to open rtgMessage.dat file (%s)\n", buf);
        }

        if (fwrite(msg_data_ptr, msg_data_size, 1, rtg_fp) < 1)
        {
          perror ("Error: Error in writing start message in rtgMessage.dat file\n");
          NS_EXIT(-1, CAV_ERR_1060047);
        }
        fflush(rtg_fp);
      }

      if(rtgMessage_file_name_counter != 0)
      {
        sprintf(old_buf, "%s/%s", g_ns_wdir, buf);
        sprintf(new_buf,"%s/logs/TR%d/%lld/rtgMessage.dat.%d.%llu", g_ns_wdir, testidx, g_partition_idx, rtgMessage_file_name_counter, g_testrun_rtg_timestamp);
      //link(old_buf, new_buf);
        if((link(old_buf, new_buf)) == -1)
        {
          if((symlink(old_buf, new_buf)) == -1)
            NSTL1(NULL, NULL, "Error: Unable to create link %s, err = %s", new_buf, nslb_strerror(errno));
        }
        NSDL1_MESSAGES(NULL, NULL, "Created link of %s in %s", old_buf, new_buf);
      }
    }
    // Dec 19,07 - Neeraj - Moved this code above so that we can log gui data even if running from command.
    // This is done so that can store one sample to get results from automation toools
    char *ptr;
    if ((ptr = getenv("GUI_DATA")))
    {
      gui_data_seq = atoi(ptr);
      sprintf(buf, "logs/%s/gui.data", global_settings->tr_or_partition);
      gui_fp = fopen (buf, "w");
      if (!gui_fp) printf("Unable to open gui.data file (%s)\n", buf);
    }
    time_t ts = time(NULL);
    if (gui_fp)
    {
      Msg_data_hdr *msg_data_local_hdr_ptr = (Msg_data_hdr *) msg_data_ptr;

      fprintf(gui_fp, "START: %s\topcode: %6.0f\n\tseq_number: %6.0f\n",
        ctime(&ts), (msg_data_local_hdr_ptr->opcode), (msg_data_local_hdr_ptr->seq_no));
    }

    if (global_settings->gui_server_addr[0])
    {
      if(gui_fd == -1)
      {
        int ret;
        ret = open_connect_to_gui_server(&gui_fd, global_settings->gui_server_addr, global_settings->gui_server_port);
        if(ret == -1)
        return;
      }

      /* Netstorm sends Message Header only to gui since gui only requires header, rest of the data it reads from
       * rtgMessage.dat file */

      if (send(gui_fd, msg_data_ptr, sizeof(Msg_data_hdr), 0) != sizeof(Msg_data_hdr))
      {
        print_core_events((char*)__FUNCTION__, __FILE__,
                                         "NetStorm unable to send messgae data header to GUI (%s:%hd) server."
                                         " error = %s", global_settings->gui_server_addr, global_settings->gui_server_port, nslb_strerror(errno) );
        return;
      }
    }
    //If secondary gui server is given then send data to secondary gui server also
    if (global_settings->secondary_gui_server_addr[0])
    { 
      send_data_to_secondary_gui_server();
    }
  }
}

static inline void fill_buff_to_remove_gen_by_stop_api(int gen_idx, char *reply_buff)
{
  NSDL2_PARENT(NULL, NULL, "Method Called, where Specified Generator has been removed, gen_idx = %d, gen_name = %s", gen_idx, generator_entry[gen_idx].gen_name);
  generator_entry[gen_idx].flags |= IS_GEN_REMOVE;
  strcpy(generator_entry[gen_idx].test_end_time_on_gen, get_relative_time());
  NSTL1(NULL, NULL, "Going to remove generator, where gen_idx = %d, gen_name = %s, flags = %d, test_end_time_on_gen = %s", gen_idx, generator_entry[gen_idx].gen_name, generator_entry[gen_idx].flags, generator_entry[gen_idx].test_end_time_on_gen);
}

static inline void send_data_to_stop_tool(Msg_com_con *mccptr, char *reply_buff, int len)
{
  NSDL1_MESSAGES(NULL, NULL, "Method Called, where reply_buff = %s, len = %d", reply_buff, len);
  memcpy(reply_buff, &len, sizeof(int));
  write_msg(mccptr, reply_buff, len + sizeof(int), 0, CONTROL_MODE);
  len = 0;
  memcpy(reply_buff, &len, sizeof(int));
  write_msg(mccptr, reply_buff, sizeof(int), 0, CONTROL_MODE);
}

void mark_gen_inactive_and_remove_from_list(Msg_com_con *mccptr, int gen_idx, int kill_by_stop_api, int len, char *buff)
{
  char cmd[1024];
  char err_msg[1024]= "\0";

  NSDL2_MESSAGES(NULL, NULL, "Method Called, where gen_idx = %d, gen_name = %s, kill_by_stop_api = %d",
                             gen_idx, generator_entry[gen_idx].gen_name, kill_by_stop_api);
  //Gaurav: dump netstat -natp in generator TR before generator got dead. bg this process may takes time
  sprintf(cmd, "nohup nsu_server_admin -g -i -s %s -c 'sudo netstat -napt'"
               " >%s/logs/TR%d/NetCloud/%s/TR%d/netstat_output.txt 2>&1 &", generator_entry[gen_idx].IP,
               g_ns_wdir, testidx, generator_entry[gen_idx].gen_name, generator_entry[gen_idx].testidx);
  NSTL1(NULL, NULL, "cmd = %s", cmd);
  if(nslb_system(cmd,1,err_msg) != 0)
  {
    NSTL1(NULL, NULL, "\nError in executing cmd = %s\n", cmd);
  }

  //Did not get progress report from this genartor. Marking this generator dead.
  //Send end test run message to generator
  EndTestRunMsg end_msg;
  end_msg.opcode = END_TEST_RUN_MESSAGE;
  //2-Generator Not Healthy 3-Generator is stopped intentionally
  end_msg.status = ((kill_by_stop_api == 0)?2:3);
  end_msg.msg_len = sizeof(end_msg) - sizeof(int);
  write_msg(&g_dh_msg_com_con[gen_idx], (char *)&end_msg, sizeof(end_msg), 0, DATA_MODE);
  write_msg(&g_msg_com_con[gen_idx], (char *)&end_msg, sizeof(end_msg), 0, CONTROL_MODE);
  
  if (kill_by_stop_api) {
    fill_buff_to_remove_gen_by_stop_api(gen_idx, buff);
    send_data_to_stop_tool(mccptr, buff, len);
  }
  decrease_msg_counters(gen_idx, 0);
  update_vars_to_continue_ctrl_test(gen_idx);
}

static void process_nvm_data_from_generator(Msg_com_con *mccptr)
{
  NSTL1(NULL, NULL, "Method Called"); 
  if(loader_opcode == CLIENT_LOADER)
  {
    if ((g_master_msg_com_con != NULL) && (g_master_msg_com_con->fd >= 0))
    {
      parent_child send_msg;
      send_msg.child_id = g_parent_idx;
      send_msg.opcode = SHOW_ACTIVE_WITH_NVM_GEN;
      send_msg.testidx = testidx;
      send_msg.num_killed_nvm = g_data_control_var.total_killed_nvms;
      send_msg.avg_time_size = g_avgtime_size - ip_avgtime_size;
      send_msg.msg_len = sizeof(send_msg) - sizeof(int);
      NSDL3_MESSAGES(NULL, NULL, "send_msg.avg_time_size = %d, ip_avgtime_size = %d", send_msg.avg_time_size, ip_avgtime_size);
      write_msg(g_master_msg_com_con, (char *)(&send_msg), sizeof(send_msg), 0, CONTROL_MODE);
    }
  }
}

static void process_show_generator(Msg_com_con *mccptr)
{
  int genIdx;
  char reply_buff[16000];
  int len = 0;
  char *reply_buff_ptr = reply_buff;
  
  reply_buff_ptr += 4; //Making space for size of buffer
 
  if(loader_opcode == MASTER_LOADER)
  {
    send_msg_to_all_clients(GET_NVM_DATA_FROM_GEN, 0); 
  }

  for (genIdx = 0 ; genIdx < global_settings->num_process; genIdx++)
  {
    if(generator_entry[genIdx].flags & IS_GEN_ACTIVE) {
      NSTL1(NULL, NULL, "gen_idx = %d, num_nvm_per_gen = %d, num_killed_nvm = %d", 
                         genIdx, generator_entry[genIdx].total_nvms, generator_entry[genIdx].total_killed_nvm); 
      len += sprintf(reply_buff_ptr + len, "%s|%s|%s|%s|%s|Active|%d|%s|%d|%d\n", generator_entry[genIdx].gen_name,
                     generator_entry[genIdx].IP, generator_entry[genIdx].agentport, generator_entry[genIdx].location,
                     basename(generator_entry[genIdx].work), generator_entry[genIdx].testidx,
                     generator_entry[genIdx].test_end_time_on_gen, generator_entry[genIdx].total_nvms,
                     generator_entry[genIdx].total_killed_nvm);
    } else if(generator_entry[genIdx].flags & IS_GEN_INACTIVE) {
      len += sprintf(reply_buff_ptr + len, "%s|%s|%s|%s|%s|Stopped|%d|%s|-|-\n", generator_entry[genIdx].gen_name,
                     generator_entry[genIdx].IP, generator_entry[genIdx].agentport, generator_entry[genIdx].location,
                     basename(generator_entry[genIdx].work), generator_entry[genIdx].testidx, 
                     generator_entry[genIdx].test_end_time_on_gen);
    } else if(generator_entry[genIdx].flags & IS_GEN_REMOVE) {
      len += sprintf(reply_buff_ptr + len, "%s|%s|%s|%s|%s|Removed|%d|%s|-|-\n", generator_entry[genIdx].gen_name,
                     generator_entry[genIdx].IP, generator_entry[genIdx].agentport, generator_entry[genIdx].location,
                     basename(generator_entry[genIdx].work), generator_entry[genIdx].testidx,
                     generator_entry[genIdx].test_end_time_on_gen);
    } else {
       len += sprintf(reply_buff_ptr + len, "%s|%s|%s|%s|%s|Self-Removed|%d|%s|-|-\n", generator_entry[genIdx].gen_name,
                     generator_entry[genIdx].IP, generator_entry[genIdx].agentport, generator_entry[genIdx].location,
                     basename(generator_entry[genIdx].work), generator_entry[genIdx].testidx,
                     generator_entry[genIdx].test_end_time_on_gen);
    } 
  }

  memcpy(reply_buff, &len, sizeof(int));
  write_msg(mccptr, reply_buff, len + sizeof(int), 0, CONTROL_MODE);

  len = 0;
  memcpy(reply_buff, &len, sizeof(int));
  write_msg(mccptr, reply_buff, sizeof(int), 0, CONTROL_MODE);
}

static void process_active_gen_with_nvm(Msg_com_con *mccptr)
{
  parent_child *tmpmsg = &msg->top.internal;
  generator_entry[tmpmsg->child_id].total_killed_nvm = tmpmsg->num_killed_nvm;
  NSTL1(NULL, NULL, "Values in Generator struct: child_id = %d, total_killed_nvm = %d, total_nvms = %d",
                    tmpmsg->child_id, generator_entry[tmpmsg->child_id].total_killed_nvm,
                    generator_entry[tmpmsg->child_id].total_nvms);
}

static inline void process_stop_generator(Msg_com_con *mccptr, ns_comp_ctrl_rep *msg)
{
  int genIdx, genFlg = 0;
  char reply_buff[1024];
  int len;

  NSTL1(NULL, NULL, "CONTROL_CONNECTION NSI_STOP_GENERATOR_TOOL: Going to remove generator as requested by user.");
  for (genIdx = 0; genIdx < global_settings->num_process; genIdx++)
  {
    if(!strcmp(msg->reply_msg, (char *)generator_entry[genIdx].gen_name))
    {
      genFlg = 1;
      if(generator_entry[genIdx].flags & IS_GEN_ACTIVE)
      {
        len = sprintf(reply_buff + sizeof(int), "Generator %s has been removed.\n", msg->reply_msg);
        mark_gen_inactive_and_remove_from_list(mccptr, genIdx, 1, len, reply_buff);
        break;
      }
      else
      {
        len = sprintf(reply_buff + sizeof(int), "Generator %s is not running.\n", msg->reply_msg);
        send_data_to_stop_tool(mccptr, reply_buff, len);
        break;
      }
    }
  }

  if(genFlg == 0)
  {
    NSDL2_PARENT(NULL, NULL, "Specified Generator not found.");
    len = sprintf(reply_buff + sizeof(int), "Generator %s not found.\n", msg->reply_msg);
    send_data_to_stop_tool(mccptr, reply_buff, len);
  }
}

// The finish_report msg for test run has been recived, processing finish_report msg
static int process_finish_report_msg(Msg_com_con *mccptr)
{
  NSDL2_MESSAGES(NULL, NULL, "Method called");

  NSTL1(NULL, NULL, "[Control]: Received FINISH_REPORT msg from the child_id = %d"
                    " from ip = %s", mccptr->nvm_index, mccptr->ip);

  decrease_msg_counters(mccptr->nvm_index, 1);
  if(loader_opcode == MASTER_LOADER)
  {
    parent_child send_msg;
    send_msg.child_id = mccptr->nvm_index;
    send_msg.opcode = FINISH_REPORT_ACK;
    send_msg.abs_ts = (time(NULL) * 1000);
    send_msg.msg_len = sizeof(parent_child) - sizeof(int);
    write_msg(mccptr, (char *)&send_msg, sizeof(send_msg), 1, CONTROL_MODE);
  }
  CLOSE_MSG_COM_CON(mccptr, CONTROL_MODE);
  return CHECK_ALL_CHILD_DONE; 
}

static int read_and_proc_msg_from_nvm(Msg_com_con *mccptr, struct epoll_event *epev, int i, int num_children, avgtime *local_end_avg)
{

  int local_fd = mccptr->fd; //For debugging purpose
  local_end_avg = NULL;
  int retn = 0;
 
  NSDL2_PARENT(NULL, NULL, "mccptr = %p, i = %d, num_children = %d for control connection", mccptr, i, num_children);
  if (local_fd < 0)
    return 1;

  if (epev[i].events & EPOLLERR) {
    NSDL3_MESSAGES(NULL, NULL, "EPOLLERR occured on sock %s. error = %s for control connection", 
              msg_com_con_to_str(mccptr), nslb_strerror(errno));
    CLOSE_MSG_COM_CON(mccptr, CONTROL_MODE);
    HANDLE_CHILD_FAILURE;
    return 1;
  }
  if (epev[i].events & EPOLLHUP) {
    NSDL3_MESSAGES(NULL, NULL, "EPOLLHUP occured on sock %s. error = %s for control connection", 
              msg_com_con_to_str(mccptr), nslb_strerror(errno));
    CLOSE_MSG_COM_CON(mccptr, CONTROL_MODE);
    HANDLE_CHILD_FAILURE;
    return 1;
  }

  /* partial write */
  if (epev[i].events & EPOLLOUT){
    if (mccptr->state & NS_STATE_WRITING)
      write_msg(mccptr, NULL, 0, 0, CONTROL_MODE);
    else {
      NSDL3_MESSAGES(NULL, NULL, "Write state not `writing', still we got EPOLLOUT event on fd = %d for control connection", mccptr->fd);
    }
  }

  /* Data we are reading is not complete, wait for anoter poll
     In case of nsa_log_mgr we will never get EPOLLIN beacuse we dont
     have to read anything in that case msg will be NULL & we will return
     back from here
  */
  msg = NULL;
  if (epev[i].events & EPOLLIN) 
    msg = (parent_msg *)read_msg(mccptr, &rcv_amt, CONTROL_MODE);
 
  HANDLE_CHILD_FAILURE;
  if(msg == NULL)
    return 1;

  NSDL3_MESSAGES(NULL, NULL, "RTC flags = %x for control connection", CHECK_RTC_FLAG(RUNTIME_SET_ALL_FLAG));
  if ((loader_opcode != CLIENT_LOADER) && (msg->top.internal.opcode >= RTC_PAUSE && msg->top.internal.opcode <= TIER_GROUP_RTC ) && CHECK_RTC_FLAG(RUNTIME_PROGRESS_FLAG)) {
    CHECK_RTC_APPLIED ()
  }

  NSDL3_MESSAGES(NULL, NULL, "NS parent get message of len = %d, opcode = %d, mccptr = %p for control connection", 
                              msg->top.internal.msg_len, msg->top.internal.opcode, mccptr);

  switch (msg->top.internal.opcode)
  {
    case RAMPUP_MESSAGE:
      process_ramp_msg(mccptr);
      break;

    case RAMPDOWN_MESSAGE:
      process_ramp_msg(mccptr);
      break;
    
    case VUSER_TRACE_REQ: // From command
      process_vuser_tracing_req(mccptr->fd, (User_trace *)&(msg->top.internal));
      break;

    case VUSER_TRACE_REP: // from NVM
      process_vuser_tracing_rep(mccptr->fd, (User_trace *)&(msg->top.internal));
      break;

    case RAMPUP_DONE_MESSAGE:
      process_ramp_done_msg(mccptr);
      break;

    case RAMPDOWN_DONE_MESSAGE:
      process_ramp_done_msg(mccptr);
      break;

    case READY_TO_COLLECT_DATA:
      process_ready_to_collect_data_msg(mccptr);
      break;
 
    case START_COLLECTING:
      process_start_collecting_msg(mccptr);
      break;
 
    case FINISH_TEST_RUN_MESSAGE: //Can be received only by CLIENT from master
      process_finish_tesrun_msg(mccptr);
      break;
 
    case FINISH_TEST_RUN_IMMEDIATE_MESSAGE: //Can be received only by generator 
      process_finish_tesrun_immediate_msg(mccptr);
      break;
 
    case START_MSG_BY_CLIENT:
      process_start_msg_by_client(mccptr);
      break;
 
    case PHASE_COMPLETE:
      process_phase_complete_msg(msg);
      break;
 
    case APPLY_PHASE_RTC:
      set_rtc_info(mccptr, APPLY_PHASE_RTC);
      process_schedule_rtc(mccptr->fd, (char *)msg);
      if(CHECK_RTC_FLAG(RUNTIME_FAIL))
        RUNTIME_UPDATION_RESPONSE
      break;
    
    case APPLY_QUANTITY_RTC:
      set_rtc_info(mccptr, APPLY_QUANTITY_RTC);
      process_quantity_rtc(mccptr->fd, (char *)msg);
      if(CHECK_RTC_FLAG(RUNTIME_FAIL))
        RUNTIME_UPDATION_RESPONSE
      break;
/*
    case QUANTITY_RESUME_RTC:
      process_rtc_quantity_resume_schedule((User_trace *)&(msg->top.internal));
      break;
*/ 
    case PAUSE_SCHEDULE:
      process_pause_schedule(mccptr->fd, (Pause_resume *)&(msg->top.internal));
      break;
 
    case RESUME_SCHEDULE:
      process_resume_schedule(mccptr->fd, (Pause_resume *)&(msg->top.internal));
      break;
    
    case START_PHASE:
      process_start_phase_from_master(&(msg->top.internal));
      break;
 
    //Sync Point releated opcodes
    case SP_VUSER_TEST: 
      //From NVM. Parent need to test if this user need to go in Sync Point
      process_test_msg_from_nvm(mccptr->fd, (SP_msg*)&(msg->top.internal));
      break;
 
    case SP_VUSER_WAIT: 
      process_sp_wait_msg_frm_parent((SP_msg*)&(msg->top.internal));
      break;

    case SP_VUSER_CONTINUE: 
      process_sp_continue_msg_frm_parent((SP_msg*)&(msg->top.internal));
      break;

    case SP_RELEASE: 
      process_sp_release_msg_frm_parent((SP_msg*)&(msg->top.internal));
      break;
 
    //NetCloud related opcodes
    case RTC_PAUSE: 
      //From Controller. Generators need to pause their NVMs 
      process_pause_for_rtc(msg->top.internal.opcode, msg->top.internal.gen_rtc_idx, (User_trace *)&(msg->top.internal));
      break;
 
    case RTC_RESUME: 
      //From Controller. Generators need to resume their NVMs as RTC changes failed 
      process_resume_from_rtc(msg->top.internal.gen_rtc_idx);
      break;
 
    case NC_APPLY_RTC_MESSAGE: 
      //From Controller. Generators need to apply RTC changes 
      process_nc_apply_rtc_message(mccptr->fd, (User_trace *)&(msg->top.internal));
      break;
 
    case RTC_PAUSE_DONE: 
      //From Generators/child pause done has been applied 
      if(!CHECK_RTC_FLAG(RUNTIME_FPARAM_FLAG))
        process_pause_done_message(mccptr->fd, (User_trace *)&(msg->top.internal));
      else
        update_fparam_rtc_struct();
      break;
 
    case RTC_RESUME_DONE: 
      //From Generators/child resume done has been applied, log message 
      if(!CHECK_RTC_FLAG(RUNTIME_FPARAM_FLAG))
        process_resume_done_message(mccptr->fd, msg->top.internal.opcode, msg->top.internal.child_id, msg->top.internal.gen_rtc_idx);
      else
      {
        char *reply_msg = ((User_trace *)&(msg->top.internal))->reply_msg;
        NSDL1_MESSAGES(NULL, NULL, "reply msg = %s", reply_msg);
        if(loader_opcode == MASTER_LOADER)
        {
          if(strncmp(reply_msg, "SUCCESS", 7))
          {
            NSTL1(NULL, NULL, "Error message from Generator[%d]. Messaage: %s",  msg->top.internal.child_id, reply_msg);
            SET_RTC_FLAG(RUNTIME_FAIL);
            RUNTIME_UPDATE_LOG(reply_msg)
            strcpy(rtcdata->err_msg, reply_msg);
          }
        }
        handle_fparam_rtc_done(msg->top.internal.child_id); 
      }
      break;
 
    //case NC_RTC_FAILED_MESSAGE: 
      //From Generators in case rtc fails, sends error message to controller 
      //process_nc_rtc_failed_message(mccptr->fd, (User_trace *)&(msg->top.internal), 0);
     // break;
 
    case NS_COMP_CNTRL_MSG:
      //
      process_comp_ctrl_msg(mccptr, msg);
      break;
 
    case NSA_LOG_MGR_PORT_CHANGE_MESSAGE:
      send_nsa_log_mgr_port_change_msg(((parent_child*)&(msg->top.internal))->event_logger_port);
      break;
 
    case RTC_PHASE_COMPLETED:
      process_rtc_ph_complete(msg); 
      break;
 
    case SHOW_ACTIVE_WITH_NVM_GEN:
      process_active_gen_with_nvm(mccptr);
      break; 
 
    case GET_NVM_DATA_FROM_GEN:
      process_nvm_data_from_generator(mccptr);
      break;
 
    case STOP_PARTICULAR_GEN:
      process_stop_generator(mccptr, (ns_comp_ctrl_rep*)&(msg->top.internal));
      break;
 
    case SHOW_ACTIVE_GENERATOR:
      process_show_generator(mccptr);
      break;
 
    case FPARAM_RTC_ATTACH_SHM_DONE_MSG:
      process_fparam_rtc_attach_shm_done_msg(msg);
      break;
 
    case APPLY_FPARAM_RTC:
      set_rtc_info(mccptr, APPLY_FPARAM_RTC);
      if((retn = handle_fparam_rtc(mccptr, (char *)msg)) != 0)
        handle_fparam_rtc_done(retn);
      break;
 
    case PAUSE_SCHEDULE_DONE:
    case RESUME_SCHEDULE_DONE:
      process_pause_resume_feature_done_msg(msg->top.internal.opcode);
      break; 
 
    case TEST_TRAFFIC_STATS:
      process_http_test_traffic_stats(msg, mccptr);
      break;
 
    /*Changes for VUser Run Time Control*/
    case GET_VUSER_SUMMARY:
      g_vuser_msg_seq_num = msg->top.internal.gen_rtc_idx;
      process_get_vuser_summary(mccptr);
      break;
    
    case GET_VUSER_SUMMARY_ACK:
      g_vuser_msg_seq_num = msg->top.internal.gen_rtc_idx;
      process_get_vuser_summary_ack(msg->top.internal.child_id, (GroupVUserSummaryTable *)((char *)&msg->top.internal + sizeof(RTC_VUser)));
      break;
 
    case GET_VUSER_LIST:
      g_vuser_msg_seq_num = msg->top.internal.gen_rtc_idx;
      process_get_vuser_list(mccptr, (char*)msg, (msg->top.internal.msg_len - (sizeof(RTC_VUser) - sizeof(int)))); 
      break;
 
    case GET_VUSER_LIST_ACK:
      g_vuser_msg_seq_num = msg->top.internal.gen_rtc_idx;
      process_get_vuser_list_ack(msg->top.internal.child_id, 
                                 (VUserInfoTable *)((char *)&msg->top.internal + sizeof(RTC_VUser)),
                                 (msg->top.internal.msg_len - (sizeof(RTC_VUser) - sizeof(int))));
      break;
 
    case PAUSE_VUSER:
      g_vuser_msg_seq_num = msg->top.internal.gen_rtc_idx;
      process_pause_vuser(mccptr, (char*)msg, (msg->top.internal.msg_len - (sizeof(RTC_VUser) - sizeof(int))));
      break;
 
    case PAUSE_VUSER_ACK:
      g_vuser_msg_seq_num = msg->top.internal.gen_rtc_idx;
      process_pause_vuser_ack(msg->top.internal.child_id, ((char *)&msg->top.internal + sizeof(RTC_VUser)));
      break; 
 
    case RESUME_VUSER:
      g_vuser_msg_seq_num = msg->top.internal.gen_rtc_idx;
      process_resume_vuser(mccptr, (char*)msg, (msg->top.internal.msg_len - (sizeof(RTC_VUser) - sizeof(int))));
      break;
 
    case RESUME_VUSER_ACK:
      g_vuser_msg_seq_num = msg->top.internal.gen_rtc_idx;
      process_resume_vuser_ack(msg->top.internal.child_id, ((char *)&msg->top.internal + sizeof(RTC_VUser)));
      break;
 
    case STOP_VUSER:
      g_vuser_msg_seq_num = msg->top.internal.gen_rtc_idx;
      process_stop_vuser(mccptr, (char*)msg, (msg->top.internal.msg_len - (sizeof(RTC_VUser) - sizeof(int))));
      break;
 
    case STOP_VUSER_ACK:
      g_vuser_msg_seq_num = msg->top.internal.gen_rtc_idx;
      process_stop_vuser_ack(msg->top.internal.child_id, ((char *)&msg->top.internal + sizeof(RTC_VUser)));
      break;
 
    case APPLY_ALERT_RTC:
      set_rtc_info(mccptr, APPLY_ALERT_RTC);
      process_alert_rtc(mccptr, (char *)msg);
      if(CHECK_RTC_FLAG(RUNTIME_FAIL))
      {  
        send_rtc_msg_to_invoker(mccptr, APPLY_ALERT_RTC, NULL, 0);
        RUNTIME_UPDATION_RESET_FLAGS
      }
      break;

    case NC_RTC_APPLIED_MESSAGE:
      //From Generators. Controller appends success message in RTC log files 
      process_nc_rtc_applied_message(mccptr->fd, (User_trace *)&(msg->top.internal));
      break;
 
    case END_TEST_RUN_MESSAGE:
      process_end_test_run_msg(mccptr, (EndTestRunMsg *)&(msg->top.internal), CONTROL_MODE);
      // Note - Above function exits in all cases, do break is not hit
      break;

    //Add to Received END_TEST_RUN_ACK_MESSAGE
    case END_TEST_RUN_ACK_MESSAGE:
      NSTL1(NULL, NULL, "Received END_TEST_RUN_ACK_MESSAGE from child '%d' for control connection.", mccptr->nvm_index);
      CLOSE_MSG_COM_CON(mccptr, CONTROL_MODE);
      HANDLE_CHILD_FAILURE;
      break;

    case FINISH_REPORT:
      if (process_finish_report_msg(mccptr)) {
        if (loader_opcode == CLIENT_LOADER)
        {
          NSTL1(NULL, NULL, "Received FINISH_REPORT msg from all child/clients");
          parent_msg cmsg;
          cmsg.top.internal.opcode = FINISH_REPORT;
          forward_msg_to_master(g_master_msg_com_con->fd, &cmsg, sizeof(parent_msg));
          break;
        }
        return 2;
      } else
        break;

    case FINISH_REPORT_ACK:
        NSTL1(NULL, NULL, "FINISH_REPORT_ACK: (Generator:%d <- Master)", mccptr->nvm_index);
        return 2;
      break;
 
    default:
      process_default_case_msg(mccptr, CONTROL_MODE);
      break;

  } // End of switch
  return 0;
} 

static inline void init_status_bitflag()
{
  int grp_idx;
  int num_entries;

  NSDL2_MESSAGES(NULL, NULL, "Method called");
  g_child_status_mask = nslb_alloc_bitflag(); 
  if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO)
    num_entries = 1;
  else
    num_entries = total_runprof_entries;
  MY_MALLOC(g_child_group_status_mask, num_entries * sizeof(size_t *), "group wise child mask array", -1);
  for(grp_idx = 0; grp_idx < num_entries; grp_idx++)
    g_child_group_status_mask[grp_idx] = nslb_alloc_bitflag();
}

avgtime* wait_forever(int num_children, cavgtime **c_end_avg)
{
  int cnt = 0, i, ret; //cnt must be set to zero 
  struct epoll_event *epev = NULL;
  int epoll_timeout;
  int rtc_epoll_timeout;
  //char epoll_timeout_cnt = 0;
  char cmd[0xffff];
  int timer_running = 0;
  timer_type* temp_tmr = NULL;
  int temp_ms = 0;
  avgtime *end_avg = NULL;
  int nvm_msg_processed = 0;
  avgtime *local_end_avg = NULL;
  int process_epoll_time_threshold = global_settings->progress_secs / 2;
  SET_CALLER_TYPE(IS_PARENT_AFTER_DH);

  NSDL2_PARENT(NULL, NULL, "Method called, parent_epoll_timeout = %d, parent_timeout_max_cnt = %d, dump_parent_on_timeout = %d "
                           "for control connection", global_settings->parent_epoll_timeout, global_settings->parent_timeout_max_cnt,
			    global_settings->dump_parent_on_timeout);	

  //Runtime changes init
  init_runtime_data_struct();
  sprintf(rtcdata->log_file, "%s/logs/TR%d/runtime_changes/runtime_changes.log", g_ns_wdir, testidx);

  sprintf(cmd, "%s/bin/get_user_stats.sh %s/logs/TR%d/system_stats.log",
				 g_ns_wdir, g_ns_wdir, testidx);
  start_msg_ts = get_ms_stamp();
  MY_MALLOC_AND_MEMSET(epev, sizeof(struct epoll_event) * g_parent_epoll_event_size, "epoll event", -1);

  if (epev == NULL)
  {
    NSTL1(NULL, NULL, "%s:%d Malloc failed for control connection. So, exiting.", __FUNCTION__, __LINE__);
    NS_EXIT(-1, CAV_ERR_1060025, __FUNCTION__, __LINE__);
  }

  if (loader_opcode == MASTER_LOADER)
    (void) signal( SIGINT, handle_master_sigint );

  kill_timeout.tv_sec  = global_settings->parent_epoll_timeout/1000; 
  kill_timeout.tv_usec = global_settings->parent_epoll_timeout%1000;
 
  init_vars_for_each_test(num_children);
  init_status_bitflag();
  if(global_settings->con_test_gen_fail_setting.percent_started != 100)
  {
    UPDATE_GLOB_DATA_CONTROL_VAR(g_data_control_var.num_active = g_data_control_var.num_pge = g_data_control_var.num_connected)
  }
  // Moving init_all_avgtime from wait_forever.c to ns_parent.c because it was required for dynamic transaction project before processsing gdf
  // init_all_avgtime();
  timeout = kill_timeout;

  //rtc_epoll_timeout = kill_timeout.tv_sec * 1000 + kill_timeout.tv_usec / 1000;
  rtc_epoll_timeout = global_settings->rtc_schedule_timeout_val * 1000;

  end_avg = (avgtime *)g_end_avg[0];
  *c_end_avg = (cavgtime *)g_cur_finished[0];

  /* this process is a client and need to open a TCP socket to connect to the master */
  if (loader_opcode == CLIENT_LOADER)
  {
    NSTL1_OUT(NULL, NULL, "Connecting to controller at IP address %s and port %d for control connection\n", master_ip, master_port);
    if ((master_fd = connect_to_master()) < 0) {
      fprintf( stderr, "Generator parent:  Error in creating the TCP socket to communicate with the controller (%s:%d) "
                       "for control connection. Aborting...\n", master_ip, master_port);
    }
    NSTL1_OUT(NULL, NULL, "Started test...\n");
    //    sleep(3); // This is added so that master gets start message from all clients before  it sends next message
    //Bug 60868: client loader not showing above as they are buffered
    fflush(NULL);
  }

  if((loader_opcode != CLIENT_LOADER) && (!global_settings->gui_server_addr[0] && !g_gui_bg_test)) //to print progress report on console when it is not Client.
    console_fp = stdout;

  /*Incase of running GUI test ib background gui server address is set 0.0.0.0 to avoid writing of progress report to console to testrun.out so reset the gui_server_addr to zero for further use*/
  /* If LPS to NS connection for ND is exist then add ndc_mccptr.fd to epoll*/
  NSDL1_PARENT(NULL, NULL, "ND Connection fd = %d for control connection", ndc_mccptr.fd);
  
  /*end of init_instance stage started in parent_init_before_starting_test*/
  if(run_mode == NORMAL_RUN)
     end_stage(NS_START_INST, TIS_FINISHED, NULL);

  int time_left_for_sending_inactive_inst_req_to_ndc;
  u_ns_ts_t last_time_stamp_of_sending_inactive_inst_req_to_ndc = get_ms_stamp();
  while(1)
  {

    epoll_timeout = kill_timeout.tv_sec * 1000 + kill_timeout.tv_usec / 1000;
    timer_running = 0;

   if(g_time_to_get_inactive_inst > 0)
   {
     time_left_for_sending_inactive_inst_req_to_ndc = get_ms_stamp() - last_time_stamp_of_sending_inactive_inst_req_to_ndc;
     if(time_left_for_sending_inactive_inst_req_to_ndc >= g_time_to_get_inactive_inst)
     {
       send_msg_to_get_inactive_instance_to_ndc(CONTROL_MODE);
       last_time_stamp_of_sending_inactive_inst_req_to_ndc = get_ms_stamp();
     }
   }
   /* If Sync Point is enalbed then calculate then set timer 
    * If monitoring mode enable then we need to set timer
    * */
   //TODO:
   if((global_settings->sp_enable == SP_ENABLE) || (global_settings->partition_creation_mode > 0)){
     temp_tmr = dis_timer_next(get_ms_stamp());
     if (temp_tmr != NULL) 
     { 
       timer_running = 1;
       /*temp_tmr is not NULL*/
       temp_ms = temp_tmr->timeout - get_ms_stamp();

       if(temp_ms < epoll_timeout )
         epoll_timeout = temp_ms; // It can be 0 also
       NSDL2_PARENT(NULL, NULL, "nde_epoll_timeout = %d, ms_stamp = %d for control connection", epoll_timeout, get_ms_stamp());
     }
   }
    
   NSDL2_PARENT(NULL, NULL, "After calculate_parent_epoll_timeout: epoll_timeout = %d for control connection", epoll_timeout);

    NSDL1_PARENT(NULL, NULL, "Timeout is sec=%lu usec=%lu at %lu for control connection", timeout.tv_sec, timeout.tv_usec, get_ms_stamp());

    /***************************************************************
      Forcefully read data form socekt if NS is running in ND mode
      This must be done before applying monitor run time change as it may set deliver_report_done to 1
     **************************************************************/    
    if((g_msg_com_con_nvm0 != NULL) && (nvm_msg_processed == 0))
    {
      epev[0].events = EPOLLIN;
      NSDL1_PARENT(NULL, NULL, "Forcefully reading message from nvm0 for control connection");
      ret = read_and_proc_msg_from_nvm(g_msg_com_con_nvm0, epev, 0, num_children, local_end_avg);
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
    NSDL4_PARENT(NULL, NULL, "monitor_runtime_changes_applied = %d for control connection", 
                              monitor_runtime_changes_applied);

    //NSDL4_PARENT(NULL, NULL, "Setting deliver_report_done as 0 for control connection");
    //deliver_report_done = 0;
    
    //To dump the monitor structure
    if(dump_monitor_tables == 1 && monitor_runtime_changes_applied == 0)
    {
      NSTL1(NULL, NULL, "Dumping structure of monitors for control connection.");
      dump_monitor_table();
      dump_monitor_tables = 0;
    }
 
    if(cnt > 0)
    {
      memset(epev, 0, sizeof(struct epoll_event) * cnt);
    }
    cnt = epoll_wait(g_msg_com_epfd, epev, g_parent_epoll_event_size, epoll_timeout);
   
    u_ns_ts_t process_epoll_st = get_ms_stamp();
    //printf("wokeup from select with cnt=%d\n", cnt);
    /* RTC: In case of runtime changes in NS and NC, if epoll timeout and 
     * pause and resume acknowledgements are not received.
     * In case of pause ack not received then we need to send resume messages to all NVMs or Generators.
     * Whereas in case of resume ack we need to log the message.*/
    if (rtcdata->cur_state != RESET_RTC_STATE) {
      rtc_log_on_epoll_timeout(rtc_epoll_timeout);
    } 

    if (cnt > 0)
    {
      /* Reset epoll timeout count to 0 as we has to track only continuesly timeout*/
      //epoll_timeout_cnt = 0;
      for (i = 0; i < cnt; i++)
      {
        Msg_com_con *mccptr = NULL;

        void *event_ptr = NULL;

        event_ptr = epev[i].data.ptr;
        /* What if fd_mon is NULL it can be NULL if any events come from nsa_log_mgr in Normal case it should not come*/
        NSDL3_MESSAGES(NULL, NULL, "NS Parent get an event of type = %d for control connection", *(char *) event_ptr);
        if(event_ptr) {
          switch(*(char *) event_ptr)
          {
            /* Any event is encountered on ndc_mccptr.fd,
            * Means connection close by LPS or event from LPS */
            case NS_LPS_TYPE: 
              handle_lps(epev, i);
              continue;

            case NS_STRUCT_HEART_BEAT:
              handle_server_ctrl_conn_event(epev, i, event_ptr);
              continue;

            case NS_STRUCT_TYPE_LOG_MGR_COM:
              /* Here we got an event from nsa_log_mgr*/
              handle_nsa_log_mgr(epev, event_ptr, i, get_ms_stamp());
              continue;

            case NS_STRUCT_TYPE_LISTEN:
              /* Connection came from one of the tools */
              accept_connection_from_tools();
              continue;
          }

          mccptr = (Msg_com_con *)epev[i].data.ptr;
        } else {  // fd_mon is NULL it means that this event for nsa_log_mgr
           NSEL_MAJ(NULL, NULL, ERROR_ID, ERROR_ATTR, "wait_forever: fd_mon got NULL while it should not for control connection.");
           close_msg_com_con_and_exit(mccptr, (char *)__FUNCTION__, __LINE__, __FILE__);
           continue;
        }

        /**********************************
         Handle Parent-NVM communications 
         *********************************/
        ret = read_and_proc_msg_from_nvm(mccptr, epev, i, num_children, local_end_avg);
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
      }  // End of for()
    } // End of if
    else if (cnt == 0 && timer_running)
    {
      NSDL2_POLL(NULL, NULL, "Going to run dis_timer_run");
      dis_timer_run(get_ms_stamp());
    }
    else if(!got_start_phase && cnt == 0 ) {
      NSTL1(NULL, NULL, "Parent epoll timeout but start phase not yet received for control connection, hence continue epoll wait");
      continue;
    }
    else if (cnt == 0)
    {
      NSDL3_MESSAGES(NULL, NULL, "epoll_wait() timeout for control connection. No event found");
      continue; 
    } 
    else if (errno == EBADF)
      perror("Bad g_msg_com_epfd");
    else if (errno == EFAULT)
      perror("The memory area");
    else if (errno == EINVAL)
      perror("g_msg_com_epfd is not valid");
    else
    {
      if (errno != EINTR)
        perror("epoll_wait() failed");
      else
        NSDL3_MESSAGES(NULL, NULL, "epoll_wait() interrupted for control connection");
    }
    
    /* Check how much time epoll takes, > 50 % of progress interval then log in trace */
    u_ns_ts_t process_epoll_end = get_ms_stamp();
    u_ns_ts_t process_epoll_time = process_epoll_end - process_epoll_st;
    if(process_epoll_time > process_epoll_time_threshold || cnt > g_parent_epoll_event_size)
    {
      NSTL1(NULL, NULL, "Control handler took more than 50%% of progress interval to process epoll events or got more events than max, "
                        "process_epoll_time = %lld, global_settings->progress_secs = %d, Number of events processed = %d, "
                        "Max number of events = %d", 
                         process_epoll_time, global_settings->progress_secs, cnt, g_parent_epoll_event_size);
    }
  }

  // Should we close listen_fd - may be not
  FREE_AND_MAKE_NOT_NULL(epev, "epev", -1);
  return end_avg;
}
#endif
