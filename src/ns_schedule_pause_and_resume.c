/************************************************************************************************************
 *  Name            : ns_schedule_pause_and_resume.c 
 *  Purpose         : To control Netstorm Pause/Resume  
 *  Initial Version : Monday, July 06 2009
 *  Modification    : Thursday, September 24 2009
 ***********************************************************************************************************/
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <regex.h>
#include<string.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "nslb_util.h"
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
#include "ns_schedule_phases.h"
#include "ns_schedule_phases_parse.h"
#include "ns_log.h"
#include "wait_forever.h"
#include "ns_gdf.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "ns_schedule_pause_and_resume.h"
#include "ns_schedule_ramp_down_fcu.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "netomni/src/core/ni_scenario_distribution.h"
#include "ns_trace_level.h"
#include "ns_njvm.h"
#include "ns_event_log.h"
#include "ns_runtime_changes_monitor.h"
#include "ns_runtime.h"

// Function used by Parent ------ Start

static u_ns_ts_t test_paused_at;
static char *cmd_own = NULL;
static u_ns_ts_t total_pause_time;
int send_pause_resume_msg = 0;        //counter for send PAUSE_SCHEDULE msg

static void create_pause_resume_phase_detail_buf(char *buf)
{
  Schedule *cur_schedule; 
  int i,j = 0;

  NSDL2_MESSAGES(NULL, NULL, "Method called");

  if(global_settings->schedule_by == SCHEDULE_BY_SCENARIO)
  {
    sprintf(buf, "%s", scenario_schedule->phase_array[scenario_schedule->phase_idx].phase_name);
  }
  else if(global_settings->schedule_by == SCHEDULE_BY_GROUP)
  {
    for(i = 0; i < total_runprof_entries; i++)
    {
      cur_schedule = &group_schedule[i]; 
     
      // Do not log the group if all phase of a group are completed
      if(cur_schedule->phase_array[cur_schedule->num_phases - 1].phase_status == PHASE_IS_COMPLETED) {
        continue;
      }
       
      if(j++ > 0)
        sprintf(buf, "%s,", buf); 

      sprintf(buf, "%s%s:%s", buf, runprof_table_shr_mem[i].scen_group_name, cur_schedule->phase_array[cur_schedule->phase_idx].phase_name);
    }
  }
}

// This function logs the Pause/Resume status in logs/TRxxxx dir named a file pause_resume.log
/* Scenario Based
 * Elapsed Time|User|Action|Duration|Total Paused Time|Schedule Details
 * 00:00:00|arunnishad_CDev|Paused|00:00:03|00:00:00|ALL:FirstPhase
 * 00:00:03|arunnishad_CDev|Resumed|-|00:00:03|ALL:FirstPhase
 *
 * Group Based
 *
 * Elapsed Time|User|Action|Duration|Total Paused Time|Schedule Details
 * 00:00:00|arunnishad_CDev|Paused|00:00:03|00:00:00|G1:RampUpPhase1,G2:DurationPhase2
 * 00:00:03|arunnishad_CDev|Resumed|-|00:00:03|G1:RampUpPhase1,G2:DurationPhase2
 */
static void log_pause_resume_history(char *action, char *elapsed_time, char *duration, char *cmd_owner)
{
  char file_name[1024] = "\0";
  FILE *pause_resume_fp;
  static int header_flag = 0;
  char total_pause_time_str[1024];
  char group_phase_detail_buf[10*1024]="\0";
  //char *token_ptr = "unknown";
  
  NSDL2_MESSAGES(NULL, NULL, "Method called");
  /*Bug 9131: In NC on generator pause-resume feature core dump while getting details 
    of user id coming in controller message. This user-id does not exists on generator machine.
    Hence adding controller name in file rather computing from id
    NC-OPT : Fix it to cavisson (28th Aug 2019) 
  if (loader_opcode == CLIENT_LOADER) {
    if (getenv("NS_CONTROLLER_USR_GRP_NAME")) {
      token_ptr = strtok((getenv("NS_CONTROLLER_USR_GRP_NAME")), ":");
    }
  }
  */
  sprintf(file_name, "%s/logs/TR%d/pause_resume.log", g_ns_wdir, testidx);

 
  pause_resume_fp = fopen(file_name, "a");
   
  if(pause_resume_fp)
  {
    if(!header_flag )
    {
      fprintf(pause_resume_fp, "Elapsed Time|User|Action|Duration|Total Paused Time|Current Schedule Phase(s)\n");
      header_flag = 1;
    }
    convert_to_hh_mm_ss(total_pause_time, total_pause_time_str);
    create_pause_resume_phase_detail_buf(group_phase_detail_buf);
    if (loader_opcode == CLIENT_LOADER)
      fprintf(pause_resume_fp, "%s|cavisson|%s|%s|%s|%s\n", elapsed_time, action, duration, total_pause_time_str, group_phase_detail_buf);
    else 
      fprintf(pause_resume_fp, "%s|%s|%s|%s|%s|%s\n", elapsed_time, cmd_owner, action, duration, total_pause_time_str, group_phase_detail_buf);
    fclose(pause_resume_fp);
  }
  else
   fprintf(stderr, "Unable to write %s\n", file_name);
}

static int validate_to_resume_test_run(int fd, int msg_from)
{
  char msg_buf[1024];
  int len;

  NSDL2_SCHEDULE(NULL, NULL, "Method called");

  if(sigterm_received) {
    len = sprintf(msg_buf, "Test cannot be resumed as test run is going to stop.\n");
    NSDL2_SCHEDULE(NULL, NULL, "%s", msg_buf);
    if(fd > 0 && loader_opcode != CLIENT_LOADER) {
      if (send(fd, msg_buf, len, 0) != len) {
       fprintf(stderr, "Unable to send data to 'nsu_pause_test' command\n");
      } 
    }
    return 1;
  } else if(loader_opcode == CLIENT_LOADER && msg_from == MSG_FROM_CMD) {
    len = sprintf(msg_buf, "Test cannot be resumed as test run is running in master mode and you are trying to resume one the clients of the master test.\n");
    NSDL2_SCHEDULE(NULL, NULL, "%s", msg_buf);
    if(fd > 0 && loader_opcode != CLIENT_LOADER) {
      if (send(fd, msg_buf, len, 0) != len) {
       fprintf(stderr, "Unable to send data to 'nsu_resume_test' command\n");
      } 
    }
    return 1;
  }
  else 
   return 0;
}

static int validate_to_pause_test_run(int fd, int msg_from)
{
  char msg_buf[1024];
  int len;
  int grp_mode = get_group_mode(-1);

  NSDL2_MESSAGES(NULL, NULL, "Method called, fd = %d", fd);

  if(sigterm_received) {
    len = sprintf(msg_buf, "Test cannot be paused as test run is going to stop.\n");
    NSDL2_SCHEDULE(NULL, NULL, "%s", msg_buf);
    if(fd > 0 && loader_opcode != CLIENT_LOADER) {
      if (send(fd, msg_buf, len, 0) != len) {
       fprintf(stderr, "Unable to send data to 'nsu_pause_test' command\n");
      } 
    }
    return 1;
  } else if(loader_opcode == CLIENT_LOADER && msg_from == MSG_FROM_CMD) {
    len = sprintf(msg_buf, "Test cannot be paused as test run is running in master mode and you are trying to pause one the clients of the master test.\n");
    NSDL2_SCHEDULE(NULL, NULL, "%s", msg_buf);
    if(fd > 0) {
      if (send(fd, msg_buf, len, 0) != len) {
       fprintf(stderr, "Unable to send data to 'nsu_pause_test' command\n");
      } 
    }
    return 1;
  } else if((grp_mode != TC_FIX_CONCURRENT_USERS &&
      grp_mode != TC_FIX_USER_RATE &&   
      grp_mode != TC_MIXED_MODE) &&  
     (run_mode == FIND_NUM_USERS || run_mode == STABILIZE_RUN)) {
    len = sprintf(msg_buf, "Test cannot be paused as test run is a goal based scenario.\n");
    NSDL2_SCHEDULE(NULL, NULL, "%s", msg_buf);
    if(fd > 0 && loader_opcode != CLIENT_LOADER) {
      if (send(fd, msg_buf, len, 0) != len) {
       fprintf(stderr, "Unable to send data to 'nsu_pause_test' command\n");
      } 
    }
    return 1;
  } else if( is_all_phase_over()) {
    len = sprintf(msg_buf, "Test cannot be paused as all phase are over.\n");
    NSDL2_SCHEDULE(NULL, NULL, "%s", msg_buf);
    if(fd > 0 && loader_opcode != CLIENT_LOADER) {
      if (send(fd, msg_buf, len, 0) != len) {
       fprintf(stderr, "Unable to send data to 'nsu_pause_test' command\n");
      } 
    }
    return 1;
  } 
  else
   return 0;
}

static int send_pause_resume_msg_to_all_clients(Pause_resume *pause_resume_msg)
{
  int i;

  NSDL2_MESSAGES(NULL, NULL, "Method called");
  NSTL1(NULL, NULL, "PAUSE-RESUME: Sending opcode '%d' to all active generator's.", pause_resume_msg->opcode);

  for(i=0; i<sgrp_used_genrator_entries ;i++) {
    /* How to handle partial write as this method is the last called ?? */
    if (g_msg_com_con[i].fd == -1) {
      if (g_msg_com_con[i].ip)
        NSDL3_MESSAGES(NULL, NULL, "Connection with the client is already closed so not sending the msg %s", msg_com_con_to_str(&g_msg_com_con[i]));
    } else {
      NSDL3_MESSAGES(NULL, NULL, "Sending msg to Client id = %d, opcode = %d, %s", i, pause_resume_msg->opcode, 
                msg_com_con_to_str(&g_msg_com_con[i]));
      pause_resume_msg->msg_len = sizeof(Pause_resume) - sizeof(int);
      write_msg(&g_msg_com_con[i], (char *)pause_resume_msg, sizeof(Pause_resume), 0, CONTROL_MODE);
      send_pause_resume_msg++;
      NSTL1(NULL, NULL, "PAUSE-RESUME: Sending msg to Client id = %d, opcode = %d, '%s', send_pause_resume_msg = %d", 
                         i, pause_resume_msg->opcode, msg_com_con_to_str(&g_msg_com_con[i]), send_pause_resume_msg);
    }
  }
  return(0);
}

void process_resume_schedule(int fd, Pause_resume *pause_resume_msg)
{
  int i;
  char time_str_to_log[512];
  char msg_buf[2048];
  int len;
  u_ns_ts_t now;
  Schedule *schedule_ptr;
  TestControlMsg test_control_msg;
  int ctrl_msg_hdr_size = sizeof(TestControlMsg);

  NSDL2_SCHEDULE(NULL, NULL, "Method called");
  NSTL1(NULL, NULL, "PAUSE-RESUME: Recieved RESUME_SCHEDULE msg.");

  if(validate_to_resume_test_run(fd, pause_resume_msg->msg_from))
  {
    NSDL2_MESSAGES(NULL, NULL, "Resume is not validated, returning");
    return;
  }

  test_control_msg.msg_data_hdr.opcode        = RESUME_TEST;
  test_control_msg.msg_data_hdr.test_run_num  = testidx;

  if(global_settings->pause_done) {
    if(gui_fd > 0) {
      if (send(gui_fd, &test_control_msg, ctrl_msg_hdr_size, 0) != ctrl_msg_hdr_size) {
        print_core_events((char*)__FUNCTION__, __FILE__,
                             "Failed to send RESUME status to GUI.(%s:%hd), fd = %d"
                             " error = %s", global_settings->gui_server_addr, global_settings->gui_server_port, gui_fd, nslb_strerror(errno) );

        if(errno != ECONNREFUSED)
        {
          CLOSE_FD(gui_fd);
          open_connect_to_gui_server(&gui_fd, global_settings->gui_server_addr, global_settings->gui_server_port);
        }

        /* Fix bug: 9131 where the nsServer is not running on generator.
           then in this case test is killed. So we have added a check,
           where the test is not exit on generator.
        */
        // Commenting below code to resolve bug 37320
       /* if(loader_opcode != CLIENT_LOADER) {
          kill_all_children((char *)__FUNCTION__, __LINE__, __FILE__);
          exit(1);
        }*/
      }
    }
    now = get_ms_stamp();
    total_pause_time = (now - test_paused_at) + total_pause_time;
    convert_to_hh_mm_ss(now - global_settings->test_start_time, time_str_to_log);
    // future[0] is used for user id who has pause/resume test run
    log_pause_resume_history("Resumed", time_str_to_log, "-", pause_resume_msg->cmd_owner);
    len = sprintf(msg_buf, "Resuming test run at %s ...\n", time_str_to_log);
    NSDL2_SCHEDULE(NULL, NULL, "total_pause_time = %u", total_pause_time);
    print2f_always(rfp, "%s", msg_buf);
    if(fd > 0 && loader_opcode != CLIENT_LOADER) {
      if (send(fd, msg_buf, len, 0) != len) {
        fprintf(stderr, "Unable to send data to command\n");
      }
    }
    global_settings->pause_done = 0;
  } else {
    len = sprintf(msg_buf, "Test cannot be resumed as test run is already in running state.\n");
    print2f_always(rfp, "%s", msg_buf);
    if(fd > 0 && loader_opcode != CLIENT_LOADER) {
      if (send(fd, msg_buf, len, 0) != len) {
        fprintf(stderr, "Unable to send data to command\n");
      }
    }
    return;
  } 
  
  // to cancel all prev alarms
  if(loader_opcode != CLIENT_LOADER)
    alarm(0);

  if(loader_opcode == MASTER_LOADER) {
    pause_resume_msg->msg_from = 0;
    send_pause_resume_msg_to_all_clients(pause_resume_msg);
  }
  else {
    NSTL1(NULL, NULL, "PAUSE-RESUME: NS/Generator Parent recieved RESUME_SCHEDULE msg & send to their NVM's");
    for(i=0; i<global_settings->num_process ;i++) {
      pause_resume_msg->msg_len = sizeof(Pause_resume)- sizeof(int); 
      write_msg(&g_msg_com_con[i], (char *)pause_resume_msg, sizeof(Pause_resume), 0, CONTROL_MODE);
      send_pause_resume_msg++;
      NSTL1(NULL, NULL, "PAUSE-RESUME: NS/Generator Parent sending RESUME_SCHEDULE msg to NVM = %d, send_pause_resume_msg = %d.", 
                         g_msg_com_con[i].nvm_index, send_pause_resume_msg);
    }
  }

  // Search which phase was paused when phase complete came with pause msg
  int j;
  parent_msg temp_msg;
  if(global_settings->schedule_by == SCHEDULE_BY_SCENARIO) {
     schedule_ptr = scenario_schedule;
     for(j = 0; j < schedule_ptr->num_phases; j++ ) {
       if(schedule_ptr->phase_array[j].phase_status == PHASE_PAUSED) {
         NSDL2_SCHEDULE(NULL, NULL, "Phase name = %s", schedule_ptr->phase_array[j].phase_name);
         temp_msg.top.internal.opcode = PHASE_COMPLETE; 
         temp_msg.top.internal.grp_idx = -1;
         temp_msg.top.internal.phase_idx = j; 
         // send nxt phase
         check_before_sending_nxt_phase(&temp_msg);
         break;
       }
     }
  }
  else if(global_settings->schedule_by == SCHEDULE_BY_GROUP) {
    for(i = 0; i < total_runprof_entries; i++) {
      schedule_ptr = &group_schedule[i];
      for(j = 0; j < schedule_ptr->num_phases; j++ ) {
        if(schedule_ptr->phase_array[j].phase_status == PHASE_PAUSED) {
          NSDL2_SCHEDULE(NULL, NULL, "Group id = %d, Phase name = %s", schedule_ptr->group_idx, schedule_ptr->phase_array[j].phase_name);
          temp_msg.top.internal.opcode = PHASE_COMPLETE; 
          temp_msg.top.internal.grp_idx = i;
          temp_msg.top.internal.phase_idx = j; 
          // send nxt phase
          check_before_sending_nxt_phase(&temp_msg);
          break;
        }
      }
    }
  }
}

static void handle_sig_alarm(int sig)
{
  Pause_resume pause_resume_msg;
  NSDL2_MESSAGES(NULL, NULL, "Method called");

  memset(&pause_resume_msg, 0, sizeof(Pause_resume));
  pause_resume_msg.opcode = RESUME_SCHEDULE; 
  strncpy(pause_resume_msg.cmd_owner, cmd_own, 128);
  pause_resume_msg.msg_from = MSG_FROM_CMD; 
  process_resume_schedule(-1, &pause_resume_msg);
}

void process_pause_schedule(int fd, Pause_resume *pause_resume_msg)
{
  int i, len;
  char msg_buf[2048];
  char time_str_to_log[512];
  char time_str_to_print[512];
  int pause_time = pause_resume_msg->time; 
  Schedule *schedule_ptr;
  int is_rampup_or_rampdown = 0;

  TestControlMsg test_control_msg;
  int ctrl_msg_hdr_size = sizeof(TestControlMsg);
  cmd_own = pause_resume_msg->cmd_owner; //We setting command owner for handle_sig_alarm

  NSDL2_MESSAGES(NULL, NULL, "Method called, pause time = %d", pause_time);
  NSTL1(NULL, NULL, "PAUSE-RESUME: Recieved PAUSE_SCHEDULE msg.");
 
  if(validate_to_pause_test_run(fd, pause_resume_msg->msg_from))
  {
    NSDL2_MESSAGES(NULL, NULL, "Pause is not validated, returning");
    return;
  }

  test_control_msg.msg_data_hdr.opcode        = PAUSE_TEST;
  test_control_msg.msg_data_hdr.test_run_num  = testidx;

 // test_paused_at = get_ms_stamp();
  if(!global_settings->pause_done) {
    if(gui_fd > 0) {
      if (send(gui_fd, &test_control_msg, ctrl_msg_hdr_size, 0) != ctrl_msg_hdr_size) {
        print_core_events((char*)__FUNCTION__, __FILE__,
                             "Failed to send PAUSE status to GUI.(%s:%hd), fd = %d"
                             " error = %s", global_settings->gui_server_addr, global_settings->gui_server_port, gui_fd, nslb_strerror(errno));

        if(errno != ECONNREFUSED)
        {
          CLOSE_FD(gui_fd);
          open_connect_to_gui_server(&gui_fd, global_settings->gui_server_addr, global_settings->gui_server_port);
        }

        /* Fix bug: 9131 where the nsServer is not running on generator.
           then in this case test is killed. So we have added a check,
           where the test is not exit.
        */
        // Commenting below code to resolve bug 37320
        /*if(loader_opcode != CLIENT_LOADER) {
          kill_all_children((char *)__FUNCTION__, __LINE__, __FILE__);
          exit(1);
        }*/
      }
    }

  test_paused_at = get_ms_stamp();
    global_settings->pause_done = 1;
    convert_to_hh_mm_ss(test_paused_at - global_settings->test_start_time, time_str_to_log);
    if (global_settings->schedule_by == SCHEDULE_BY_SCENARIO)
    {

      schedule_ptr = scenario_schedule;
      if(schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type == SCHEDULE_PHASE_RAMP_UP ||
                                             schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type == SCHEDULE_PHASE_RAMP_DOWN)
          is_rampup_or_rampdown = 1;

    }
    else if(global_settings->schedule_by == SCHEDULE_BY_GROUP)
    {
      for(i = 0; i < total_runprof_entries; i++)
      {

        if ((runprof_table_shr_mem[i].quantity == 0))
          continue;

        schedule_ptr = &group_schedule[i];

        //Also check if any group is in rampup or rampdown phase then we need to add additional message  
        if(schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type == SCHEDULE_PHASE_RAMP_UP ||
                                             schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type == SCHEDULE_PHASE_RAMP_DOWN){
           is_rampup_or_rampdown = 1;
          break;
        }
      }
    }
    NSDL2_SCHEDULE(NULL, NULL, "schedule_ptr = %p, is_rampup_or_rampdown = %d", schedule_ptr, is_rampup_or_rampdown);


    if(pause_time > 0) {
      convert_to_hh_mm_ss(pause_time*1000, time_str_to_print);
      if(is_rampup_or_rampdown){
        len = sprintf(msg_buf, "Pausing test run for '%d sec (%s)' at %s ..."
                               "Please wait for sometime as some users are in initializing stage and it will take time to pause\n", 
                                pause_time, time_str_to_print, time_str_to_log);
      } else {
        len = sprintf(msg_buf, "Pausing test run for '%d sec (%s)' at %s ...\n", pause_time, time_str_to_print, time_str_to_log);
      }
      print2f_always(rfp, "%s", msg_buf);
      if(loader_opcode != CLIENT_LOADER) {
        alarm(pause_time);
        (void) signal( SIGALRM, handle_sig_alarm);
      }
    }
    else {
      strcpy(time_str_to_print, "Till resumed");
      if(is_rampup_or_rampdown){
        len = sprintf(msg_buf,  "Pausing test run for 'Indefinite time' at %s ..."
                                "Please wait for sometime as some users are in initializing stage and it will take time to pause\n", 
                                 time_str_to_log);
      } else {
        len = sprintf(msg_buf,  "Pausing test run for 'Indefinite time' at %s ...\n" , time_str_to_log);
      }
      print2f_always(rfp, "%s", msg_buf);
    }
    // future[0] is used for user id who has pause/resume test run
    log_pause_resume_history("Paused", time_str_to_log, time_str_to_print, pause_resume_msg->cmd_owner);
    if(fd > 0 && loader_opcode != CLIENT_LOADER) {
      if (send(fd, msg_buf, len, 0) != len) {
        fprintf(stderr, "Unable to send data to command for control connection\n");
      } 
    }
  } else {
      len = sprintf(msg_buf,  "Test cannot be paused as test run is already in paused state.\n");
      print2f_always(rfp, "%s", msg_buf);
      if(fd > 0 && loader_opcode != CLIENT_LOADER) {
        if (send(fd, msg_buf, len, 0) != len) {
         fprintf(stderr, "Unable to send data to command\n");
        } 
      }
      return;
  }

  if(loader_opcode == MASTER_LOADER) {
    pause_resume_msg->msg_from = 0;    // this is to know from where msg has came either from command or master
    send_pause_resume_msg_to_all_clients(pause_resume_msg);
  }
  else {
    NSTL1(NULL, NULL, "PAUSE-RESUME: NS/Generator Parent recieved PAUSE_SCHEDULE msg & send to their NVM's");
    for(i=0; i<global_settings->num_process ;i++) {
     pause_resume_msg->msg_len = sizeof(Pause_resume) - sizeof(int); 
     write_msg(&g_msg_com_con[i], (char *)pause_resume_msg, sizeof(Pause_resume), 0, CONTROL_MODE);
     send_pause_resume_msg++;
     NSTL1(NULL, NULL, "PAUSE-RESUME: NS/Generator Parent sending PAUSE_SCHEDULE msg to NVM = %d and send_pause_msg = %d", g_msg_com_con[i].nvm_index, send_pause_resume_msg);
    } 
  }
}

// Function used by Parent ------ Ends

static void pause_group_stabilize_timer(Schedule *schedule_ptr, u_ns_ts_t now)
{
  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d, timer_type = %s", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, get_timer_type_by_name(schedule_ptr->phase_end_tmr->timer_type));

  if(schedule_ptr->phase_end_tmr->timer_type > 0)
  {
    if(schedule_ptr->phase_end_tmr->timeout > now)
      schedule_ptr->phase_end_tmr->timeout = schedule_ptr->phase_end_tmr->timeout - now;
    else 
      schedule_ptr->phase_end_tmr->timeout = 0;

    schedule_ptr->phase_end_tmr->timer_status = 1;  // Paused
    dis_timer_del(schedule_ptr->phase_end_tmr);

    NSDL2_SCHEDULE(NULL, NULL, "Group Index = %d, Phases Index = %d, Phases Type = %d, now = %u, time_out = %u", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, now, schedule_ptr->phase_end_tmr->timeout);
  }
}

static void pause_duration_timer(Schedule * schedule_ptr, u_ns_ts_t now)
{
  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d, timer_type = %s", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, get_timer_type_by_name(schedule_ptr->phase_end_tmr->timer_type));

  if(schedule_ptr->phase_end_tmr->timer_type > 0)
  {
    if(schedule_ptr->phase_end_tmr->timeout > now)
      schedule_ptr->phase_end_tmr->timeout = schedule_ptr->phase_end_tmr->timeout - now;
    else 
      schedule_ptr->phase_end_tmr->timeout = 0;

    schedule_ptr->phase_end_tmr->timer_status = 1; // Paused
    dis_timer_del(schedule_ptr->phase_end_tmr);

    NSDL2_SCHEDULE(NULL, NULL, "Group Index = %d, Phases Index = %d, Phases Type = %d, now = %u, current time_out = %u, ", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, now, schedule_ptr->phase_end_tmr->timeout);
  }
}

/* For FCU:
 *         -> Two timer is used phase_ramp_tmr(Ramping), phase_end_tmr (Messaging & staggering of childs)
 *         -> On Pause it should stop both timers.
 *
 * For FCU: 
 *         -> Two timer is used phase_ramp_tmr(Generate users), phase_end_tmr (Ramping Callbacks & staggering of childs)
 *         -> On Pause it should not stop phase_ramp_tmr, stop only phase_end_tmr.
 */

static void pause_ramp_up_ramp_down(Schedule * schedule_ptr, u_ns_ts_t now)
{
  int scenario_type;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type);

  scenario_type = get_group_mode(schedule_ptr->group_idx);

  if(scenario_type == TC_FIX_CONCURRENT_USERS)
  {
    if(schedule_ptr->phase_ramp_tmr->timer_type > 0)
    {
      //Find remaining time to run after RTC
      if(schedule_ptr->phase_ramp_tmr->timeout > now)
        schedule_ptr->phase_ramp_tmr->timeout = schedule_ptr->phase_ramp_tmr->timeout - now;
      else
        schedule_ptr->phase_ramp_tmr->timeout = 0; 

      schedule_ptr->phase_ramp_tmr->timer_status = 1; // Paused
      dis_timer_del(schedule_ptr->phase_ramp_tmr);

      NSDL2_SCHEDULE(NULL, NULL, "Ramp timer, Group Index = %d, Phases Index = %d, Phases Type = %d, now = %u, current time_out = %u", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, now, schedule_ptr->phase_ramp_tmr->timeout);
    }
  }

  if(schedule_ptr->phase_end_tmr->timer_type > 0)
  {
    if(schedule_ptr->phase_end_tmr->timeout > now)
      schedule_ptr->phase_end_tmr->timeout = schedule_ptr->phase_end_tmr->timeout - now;
    else
      schedule_ptr->phase_end_tmr->timeout = 0; 

    schedule_ptr->phase_end_tmr->timer_status = 1; // Paused
    dis_timer_del(schedule_ptr->phase_end_tmr);

    NSDL2_SCHEDULE(NULL, NULL, "End timer, Group Index = %d, Phases Index = %d, Phases Type = %d, now = %u, current time_out = %u", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, now, schedule_ptr->phase_end_tmr->timeout);
  }
}

static void resume_group_stabilize_timer(Schedule * schedule_ptr, u_ns_ts_t now)
{
  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d, now = %u", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, now);

  timer_type *tmr = schedule_ptr->phase_end_tmr;

  if(tmr->timer_status == 1) {
    NSDL2_SCHEDULE(NULL, NULL, "Resuming Stabilize timer with timeout = %u%. Group Index = %d, Phases Index = %d, Phases Type = %d, now = %u", tmr->timeout, schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, now);
    tmr->actual_timeout = tmr->timeout;
    dis_timer_think_add(AB_TIMEOUT_END, tmr, now, tmr->timer_proc, tmr->client_data, tmr->periodic); 
  }
}

static void resume_duration_timer(Schedule * schedule_ptr, u_ns_ts_t now)
{
  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d, now = %u", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, now);

  if(schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_cmd.duration_phase.duration_mode == DURATION_MODE_INDEFINITE && global_settings->replay_mode == 0)
    return;

  timer_type *tmr = schedule_ptr->phase_end_tmr;
  if(tmr->timer_status == 1) {
    NSDL2_SCHEDULE(NULL, NULL, "Resuming Duration timer with timeout = %u%. Group Index = %d, Phases Index = %d, Phases Type = %d, now = %u", tmr->timeout, schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, now);
    tmr->actual_timeout = tmr->timeout;
    dis_timer_think_add(AB_TIMEOUT_END, tmr, now, tmr->timer_proc, tmr->client_data, tmr->periodic); 
  }
}


static void reset_ramp_up_ramp_down_timers(timer_type *tmr, int tmr_type, u_ns_ts_t now)
{
  unsigned int temp_timeout;

  if(tmr->timer_status == 1)
  {
    /* Storing actual timeout as if we have the periodic timer than it has to first start for ramining timiout,
    *  then with its actual periodic timeout */
    temp_timeout = tmr->actual_timeout;
    tmr->actual_timeout = tmr->timeout;
    // actual_timeout will be used when we call dis timer think add
    dis_timer_think_add(tmr_type, tmr, now, tmr->timer_proc, tmr->client_data, tmr->periodic); 
    tmr->actual_timeout = temp_timeout; 
    /* if its a periodic timer than dis timer think add will be called from dis timer nxt or dis tiemr
    *  run with a updated actual_timeout */
    tmr->timer_status = 0;
  }
}

/* For FCU:
 *         -> Two timer is used phase_ramp_tmr(Ramping), phase_end_tmr (Messaging & staggering of childs)
 *         -> On Resume it should restart both timers.
 *
 * For FCU: 
 *         -> Two timer is used phase_ramp_tmr(Generate users), phase_end_tmr (Ramping Callbacks & staggering of childs)
 *         -> On Resume it should not start phase_ramp_tmr, start only phase_end_tmr.
 */

static void resume_ramp_up_ramp_down(Schedule *schedule_ptr, u_ns_ts_t now)
{
  int scenario_type;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d, now = %u", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, now);

  scenario_type = get_group_mode(schedule_ptr->group_idx);

  // We have to reset ramp timer only in case of FCU
  if(scenario_type == TC_FIX_CONCURRENT_USERS)
    reset_ramp_up_ramp_down_timers(schedule_ptr->phase_ramp_tmr, AB_TIMEOUT_RAMP, now);

  reset_ramp_up_ramp_down_timers(schedule_ptr->phase_end_tmr, AB_TIMEOUT_END, now);
}

void pause_resume_timers(Schedule * schedule_ptr, int type, u_ns_ts_t now)
{
 // Return as this group phase is not yet come
 if ( schedule_ptr->phase_idx == -1 ||
      schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_status != PHASE_RUNNING )         // Not Runnning 
  {
    return;
  }

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phases Index = %d, Phases Type = %d, now = %u", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, now);

  Phases *phase_ptr = &schedule_ptr->phase_array[schedule_ptr->phase_idx];


  if(phase_ptr->phase_status == PHASE_RUNNING)  // Means Phase is started for this group 
  {
    switch(phase_ptr->phase_type)
    {
     // Phase start & stabilize has the same code because we have to just delete & add the timer
     case SCHEDULE_PHASE_START:
     case SCHEDULE_PHASE_STABILIZE:
       if((type == PAUSE_SCHEDULE) || (type == RTC_PAUSE))
          pause_group_stabilize_timer(schedule_ptr, now);
       else
          resume_group_stabilize_timer(schedule_ptr, now);
       break;
     // In this we will not add timer on RESUME if its an Indefinite mode
     case SCHEDULE_PHASE_DURATION:
       if((type == PAUSE_SCHEDULE) || (type == RTC_PAUSE))
          pause_duration_timer(schedule_ptr, now);
       else
          resume_duration_timer(schedule_ptr, now);
       break;
     case SCHEDULE_PHASE_RAMP_UP:
     case SCHEDULE_PHASE_RAMP_DOWN:
       if((type == PAUSE_SCHEDULE) || (type == RTC_PAUSE))
         pause_ramp_up_ramp_down(schedule_ptr, now);
       else
         resume_ramp_up_ramp_down(schedule_ptr, now);
       break;
     default:
       fprintf(stderr, "Invalid phase type (%d) recieved.", phase_ptr->phase_type);
       end_test_run();
    }
 }
}

void pause_resume_netstorm(int type, u_ns_ts_t now)
{
  int i;
  Schedule *cur_schedule; 

  NSDL2_SCHEDULE(NULL, NULL, "Method called.now = %u", now);

  if(type == PAUSE_SCHEDULE) {
    NSTL1(NULL, NULL, "PAUSE-RESUME: Getting opcode '%d' from NVM = %d to pause test", type, my_port_index);
    global_settings->pause_done = 1;
  }
  else {
    NSTL1(NULL, NULL, "PAUSE-RESUME: Getting opcode '%d' from NVM = %d to resume test", type, my_port_index);
    global_settings->pause_done = 0;
  }
 
  if(global_settings->schedule_by == SCHEDULE_BY_SCENARIO)
  {
    cur_schedule = v_port_entry->scenario_schedule; 
    pause_resume_timers(cur_schedule, type, now);
  }
  else if(global_settings->schedule_by == SCHEDULE_BY_GROUP)
  {
    for(i = 0; i < total_runprof_entries; i++)
    {
      cur_schedule = &(v_port_entry->group_schedule[i]);
      pause_resume_timers(cur_schedule, type, now);
    }
  }
  if(type == PAUSE_SCHEDULE)
    send_msg_from_nvm_to_parent(PAUSE_SCHEDULE_DONE, CONTROL_MODE);
  else
    send_msg_from_nvm_to_parent(RESUME_SCHEDULE_DONE, CONTROL_MODE);
}

void process_pause_resume_feature_done_msg(int opcode)
{
  static int recv_pause_msg = 0; 
  recv_pause_msg++;

  NSDL2_SCHEDULE(NULL, NULL, "Method Called, opcode = %d, send_pause_resume_msg = %d, recv_pause_msg = %d", 
                              opcode, send_pause_resume_msg, recv_pause_msg); 
 
  NSTL1(NULL, NULL, "PAUSE-RESUME: Recieved %d from child '%d'", opcode, msg->top.internal.child_id);

  if (recv_pause_msg == send_pause_resume_msg)
  {
    if (loader_opcode == CLIENT_LOADER)
    {
      send_msg_to_master(master_fd, opcode, CONTROL_MODE);
    } 
    else 
    {
      if(opcode == PAUSE_SCHEDULE_DONE) {
        NSTL1(NULL, NULL, "PAUSE-RESUME: Got all PAUSE_SCHEDULE message from generators/child, recv_pause_msg = %d", recv_pause_msg);
      }
      else {
        NSTL1(NULL, NULL, "PAUSE-RESUME: Got all RESUME_SCHEDULE message from generators/child, recv_pause_msg = %d", recv_pause_msg);
      }
    }
    recv_pause_msg = send_pause_resume_msg = 0;  //reset the variables which is just used for sending and recieving
  }
}
