/******************************************************************
 * Name                 : ns_event_log.c
 * Author               : Arun Nishad
 * Purpose              : Log event to event log file.
 * Initial Version      : Tuesday, March 03 2009
 * Modification History :
 * File will be in following format :

*****************************************************************/

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <regex.h>
#include <sys/types.h>
#include <pwd.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"

#include "ns_static_vars.h"
#include "nslb_time_stamp.h"
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
#include "ns_http_version.h"

#include "netstorm.h"
#include "ns_log.h"
#include "ns_auto_cookie.h"
#include "ns_cookie.h"
#include "amf.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "ns_url_req.h"
#include "ns_alloc.h"
#include "ns_sock_com.h"
#include "ns_debug_trace.h"
#include "ns_smtp.h"
#include "ns_smtp_send.h"
//#include "ns_handle_read.h"
#include "init_cav.h"
#include "ns_event_log.h"
#include "ns_string.h"
#include "wait_forever.h"
#include "nslb_hash_code.h"
#include "ns_event_filter.h"
#include "ns_event_id.h"
#include "nslb_util.h"
#include "nslb_sock.h"
#include "ns_monitoring.h"
#include "ns_trace_level.h"
#include "netomni/src/core/ni_scenario_distribution.h"
#include "nslb_comp_recovery.h"
#include "nslb_hash_code.h"
#include "ns_parent.h"
#include "ns_exit.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"
#include "nslb_cav_conf.h"

#define MAX_BUF_SIZE_FOR_EVENTS 1024
#define EVENT_DEF_FILE_FIELD 10
#define EVENTS_FIELD 11

#define MAX_RECOVERY_COUNT                5 
#define RESTART_TIME_THRESHOLD_IN_SEC   900

extern Msg_com_con ndc_mccptr;

int nsa_log_mgr_pid = -1;

static char *severity_str[]={"Clear", "Debug", "Information", "Warning", "Minor", "Major", "Critical"};
static char *src_type[] = {"Core", "Monitor", "Script", "API", "SyncPoint", "NDCollector"};
static int big_buf_shmid = -1;
static int num_actual_event_id = 0;
static int cur_event_definition_files = 0;
static Events **events_head_shr_mem = NULL;

static int shm_id = -1;
static  char cur_time[100];
//static  char cur_date_time[100];

ComponentRecoveryData ReaderRecoveryData;

/*Following buffers moved from the function to
 * global static.
 * event log can get called in user context(If user has given transactions
 * in scripts and transaction logging some event.), in this case event logger was taking arround
 * 25K memory and our default stack size is 8K therefore NS was coredumping.
 */

//static char ev_msg[MAX_EVENT_DESC_SIZE +  1];
//static char encoded_msg[MAX_EVENT_DESC_SIZE +  1];
//static char attr_list[MAX_EVENT_ATTR_SIZE + 1];

//static char el_buf[MAX_BUF_LEN_FOR_EL + 1]; 
//static char event_buf[MAX_BUF_LEN_FOR_EL + 1];

//====================================================================

char *get_relative_time() {
  unsigned int local_now = get_ms_stamp() - global_settings->test_start_time;
  sprintf(cur_time, "%02d:%02d:%02d", (local_now/1000) / 3600, ((local_now/1000) % 3600) / 60, ((local_now/1000) % 3600) % 60);
  return(cur_time);
}

char *get_relative_time_with_ms() {
  unsigned int local_now = get_ms_stamp() - global_settings->test_start_time;
  int ms = local_now%1000;

  sprintf(cur_time, "%02d:%02d:%02d.%03d", (local_now/1000) / 3600, ((local_now/1000) % 3600) / 60, ((local_now/1000) % 3600) % 60, ms);
  return(cur_time);
}

//Send new test run message to all NVMs
void send_nsa_log_mgr_port_change_msg(int port)
{
  int i;
  parent_child nsa_log_mgr_msg;
  NSDL1_PARENT(NULL, NULL, "Method called.");

  nsa_log_mgr_msg.opcode = NSA_LOG_MGR_PORT_CHANGE_MESSAGE;
  nsa_log_mgr_msg.event_logger_port = port;

  if(loader_opcode != MASTER_LOADER) 
  {
    //send to NVM
    for(i = 0; i < global_settings->num_process; i++)
    {
      //if NVM is over then no need to send msg to NVM
      if (g_msg_com_con[i].fd != -1)
      {
        NSTL1(NULL, NULL, "Sending msg to NVM id = %d %s", i,  
                msg_com_con_to_str(&g_msg_com_con[i]));

        NSDL3_MESSAGES(NULL, NULL, "Sending msg to NVM id = %d %s", i,  
                msg_com_con_to_str(&g_msg_com_con[i]));

        nsa_log_mgr_msg.msg_len = sizeof(parent_child) - sizeof(int);
	write_msg(&g_msg_com_con[i], (char *)&nsa_log_mgr_msg, sizeof(parent_child), 0, CONTROL_MODE);
      }
    }
  }
  else //MASTER LOADER 
  {
    for(i=0; i<sgrp_used_genrator_entries ;i++) {
      if (g_msg_com_con[i].fd != -1) {
        NSTL1(NULL, NULL, "Sending msg to Client id = %d %s", i,  
                msg_com_con_to_str(&g_msg_com_con[i]));
        NSDL3_MESSAGES(NULL, NULL, "Sending msg to Client id = %d %s", i,  
                msg_com_con_to_str(&g_msg_com_con[i]));
        //Send message
        nsa_log_mgr_msg.msg_len = sizeof(parent_child) - sizeof(int);
	write_msg(&g_msg_com_con[i], (char *)&nsa_log_mgr_msg, sizeof(nsa_log_mgr_msg), 0, CONTROL_MODE);
      }
    }
  }

  NSTL1(NULL, NULL, "Sent nsa logger port change msg.");
}

//Connect to event logger used by parent and child
static int nsa_log_mgr_recovery_connect()
{
  char *ip;

  //if(loader_opcode != CLIENT_LOADER) {
  if(!send_events_to_master) {
    ip = "127.0.0.1"; // used by Event Logger
  } else {
    ip = master_ip; // used by Event Logger
  }

  //Connect parent to event logger with non blocking connect
    NSTL1(NULL, NULL, "Connecting to event logger at ip address %s and port %d\n", ip, event_logger_port_number);

  if ((connect_to_event_logger_nb(ip, event_logger_port_number)) < 0)
  {
    NSTL1(NULL, NULL, "%s:  Error in creating the TCP socket to"
       "communicate with the nsa_event_logger (%s:%d)."
      " Aborting...\n", (loader_opcode == STAND_ALONE)?"NS parent":(loader_opcode == CLIENT_LOADER)?"Generator parent":"Controller parent", ip, event_logger_port_number);
    return 0;
  } 
  return 0;
}

//This function starts event logger if it is stopped. This function justs start the event logger and send message to NVM
//to connect or to generator in case of NetCloud
//1. Check if event logger enabled. If not return
//2. Check if send_events_to_master is true then return.
//3. Check if log manager pid is -1. If yes then start event logger
int nsa_log_mgr_recovery()
{
  NSDL2_MESSAGES(NULL, NULL, "Method called");

  /*Do not create process for log mgr as you have to send all events to masters LOG MGR*/
  if(loader_opcode == CLIENT_LOADER) {
    return -1;
  }

  //event logger is not running. start it. This should be done by NS Parent
  //-1 -> if abnormal termination
  //-2 -> if terminated through tool
  //we need to start only if pid is -1
  if (nsa_log_mgr_pid != -1)
    return -1;
  
  init_event_logger(1);

  /* Expected: execlp fails, child exit and parent is suppose to receive SIGCHILD and set 'nsa_log_mgr_pid -1' & 'event_logger_port_number 0' 
   *           and then we should came here and from below check we should return
   * Issue is: Below code gets executed before execution of handler. 
   * Solution: added one more check of 'event_logger_port_number' below.
   * TODO:     we can check if 'nsa_log_mgr_pid' is of event logger using 'ps', then only we move forward else we should return. 
   */
  if (nsa_log_mgr_pid < 0) {
   //not able to start log mgr no need to connect
    return -1;
  }

  //In hanlder, we are setting 'nsa_log_mgr_pid  -1 & event_logger_port_number to 0', but if for some reason SIGCHILD signal got delayed then there is posibility that we get error after some time in code.
  //If sigchild handler executes in between above check and below check, then we should be able to control below execution using some logic, hence adding check below also
  if(event_logger_port_number > 0)
  {
    //Send message to NVM's and generator in case of NetCloud
    //inform all NVM's about port change
    send_nsa_log_mgr_port_change_msg(event_logger_port_number);
    nsa_log_mgr_recovery_connect(); 
    set_test_run_info_event_logger_port(event_logger_port_number);
  }

  //TODO: HOW TO HANDLE RECOVERY WHEN CONNECTION FAILS
  return 0;
}

//Process event logger port change msg send by parent
void process_log_mgr_port_change_msg_frm_parent(parent_child* el_msg)
{
  NSTL1(NULL, NULL, "Method Called. opcode = %d, New port number is = %d", el_msg->opcode, el_msg->event_logger_port);
 
  event_logger_port_number = el_msg->event_logger_port;

  nsa_log_mgr_recovery_connect();
}

/* This function chks nsa_log_mgr if died */
int chk_nsa_log_mgr() {

  NSDL2_MESSAGES(NULL, NULL, "Method called, nsa_log_mgr_pid = %d", nsa_log_mgr_pid);
  int ret_pid, status;

  if (nsa_log_mgr_pid > 0) {
    // -1 meaning wait for any child process.
    ret_pid = waitpid(-1, &status, 0);
    if(ret_pid == nsa_log_mgr_pid) {
      NSTL1_OUT(NULL, NULL, "NetStorm Log Manager process got killed.\n");
      nsa_log_mgr_pid = -1;
      return 1;
    }
  }
  return 0;
}

//match to severity_str as through event string will come not MACRO
int convert_string_to_int(char *sever) {
  int i;

  for(i = 0; i< NUM_SEVERITY; i++) {
    //printf("convert_string_to_int: sever = %s  severity_str = %s i = %d\n", sever, severity_str[i], i);
    if(!strcasecmp(severity_str[i], sever))
        return i;
  }
  //If sever is Info, convert it to Debug
  if(!strcasecmp(sever, "Info")) 
    return 1;  //1 being the index of Debug in severity_str
  
  NSTL1(NULL, NULL, "Warning: Invalid severity (%s) given setting default to warning\n", sever);
  return 2;   //returning default warning
}

// EVENT_LOG <Value> <mode>
int kw_set_event_log(char *buf, int runtime_flag, char *err_msg) {
  int num;
  char keyword[MAX_LINE_LENGTH]="\0";
  char field1[16];
  int field2 = 0;

  NSDL1_PARENT(NULL, NULL, "Method Called, buf = %s", buf);

  num = sscanf(buf, "%s %s %d", keyword, field1, &field2);
  if(num < 3) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, EVENT_LOG_USAGE, CAV_ERR_1011061, CAV_ERR_MSG_1);
  }

  global_settings->event_log = convert_string_to_int(field1); 

  if(field2 < LOG_ALL_EVENT || field2 > DO_NOT_LOG_EVENT) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, EVENT_LOG_USAGE, CAV_ERR_1011061, CAV_ERR_MSG_3);
  }

  global_settings->filter_mode = field2; 

  return 0;
}

// ENABLE_LOG_MGR <Disable/Enable> <Port>
int kw_set_enable_log_mgr(char *buf, int runtime_flag, char *err_msg) {
  int num;
  char keyword[MAX_LINE_LENGTH]="\0";
  int field1 = 16;
  int field2 = 0;

  NSDL1_PARENT(NULL, NULL, "Method Called, buf = %s", buf);

  num = sscanf(buf, "%s %d %d", keyword, &field1, &field2);
  if(num < 2) {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, ENABLE_LOG_MGR_USAGE, CAV_ERR_1011060, CAV_ERR_MSG_1);
  }

  global_settings->enable_event_logger = field1; 
  global_settings->event_logger_port   = field2;

  return 0;
}

// EVENT_DEFINITION_FILE <File Name>
void kw_set_event_definition_file(char *buf) {
  int num;
  char keyword[MAX_LINE_LENGTH]="\0";
  char field1[1024];

  NSDL1_PARENT(NULL, NULL, "Method Called, buf = %s", buf);

  num = sscanf(buf, "%s %s", keyword, field1);
  if(num < 2) {
    NSTL1_OUT(NULL, NULL, "%s needs at least one arguments\n", keyword);
    exit(-1);
  }

  if(cur_event_definition_files > MAX_EVENT_DEFINITION_FILE) {
    NSTL1_OUT(NULL, NULL, "Max event definition files can not be more that %d.\n", MAX_EVENT_DEFINITION_FILE);
    exit(-1);
  }

  // We need to save netstorm_events.dat at 0th position as we can override these events 
  // Because in sorted scenario.conf EVENT_DEFINITION_FILE kw will be sort
  if(strcmp(field1, "netstorm_events.dat") == 0) {
    strcpy(global_settings->event_file_name[0], field1);
    if(cur_event_definition_files == 0) cur_event_definition_files = 1;
  } else {
     if(cur_event_definition_files == 0) cur_event_definition_files = 1;
     strcpy(global_settings->event_file_name[cur_event_definition_files], field1);
     cur_event_definition_files++;
  }
}

#if 0
/*This function is a copy of add_select_msg_com_con but more GENERIC can be replaced add_select_msg_com_con*/
inline void add_select_el_msg_com_con(int epfd, char* data_ptr, int fd, int event) {
  struct epoll_event pfd;

  NSDL1_MESSAGES(NULL, NULL, "Method called. Adding fd = %d for event = %x", fd, event);

  bzero(&pfd, sizeof(struct epoll_event)); //Added after valgrind reported bug

  pfd.events = event;
  pfd.data.ptr = (void *) data_ptr;

  if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &pfd) == -1) {
     NSTL1_OUT(NULL, NULL, "%s() - EPOLL_CTL_ADD: err = %s\n", (char*)__FUNCTION__, nslb_strerror(errno));
     exit (-1);
  }
}
#endif

/*This function invokes nsa_log_mgr if event log & event filter is enabled*/
void init_event_logger(int recovery_flag) {

  int num_proc_to_handle = 0;
  char port_str[UNSIGNED_INT_BUF_LEN + 1], num_proc_str[UNSIGNED_INT_BUF_LEN + 1];
  char filter_str[UNSIGNED_INT_BUF_LEN + 1], shm_id_str[UNSIGNED_INT_BUF_LEN + 1];
  char fd_str[UNSIGNED_INT_BUF_LEN + 1], num_total_event_id_str[UNSIGNED_INT_BUF_LEN + 1];
  char big_buf_shmid_str[UNSIGNED_INT_BUF_LEN + 1];
  char test_idx_str[12];
  char debug_str[12];
  char pgm_name[1024];

  NSDL2_PARENT(NULL, NULL, "Method Called, event_log = %d, filter_mode = %d, enable_event_logger = %d",
					   global_settings->event_log,
					   global_settings->filter_mode,
					   global_settings->enable_event_logger);

  if(recovery_flag == 0)
  {
     NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_DEBUG, __FILE__, (char*)__FUNCTION__, "Test debug event at start of init_event_logger()");

   /*Event filter is disabled, we need to process/log event at parent & nvms level*/ 
     if(!global_settings->enable_event_logger) {
       NSDL2_PARENT(NULL, NULL, "Not creating nsa_log_mgr process as log mgr is disabled");
       return;
     }

    /*Do not create process for log mgr as you have to send all events to masters LOG MGR*/
     if(loader_opcode == CLIENT_LOADER) {
        return;
     }

    
    if(nslb_init_component_recovery_data(&ReaderRecoveryData, MAX_RECOVERY_COUNT, (global_settings->progress_secs/1000 + 5) , RESTART_TIME_THRESHOLD_IN_SEC) == 0)
    {
      NSTL1(NULL, NULL, "Main thread Recovery data initialized with"
                         "MAX_RECOVERY_COUNT = %d, RESTART_TIME_THRESHOLD_IN_SEC = %d",
                             global_settings->progress_secs, RESTART_TIME_THRESHOLD_IN_SEC);

    }
    else
    {
      NSTL1(NULL, NULL, "Method Called. Component recovery could not be initialized");
    }
  }
 
  //NS_PARENT_LOGGER_LISTEN_PORTS keyword is enable. 
  if(global_settings->ns_use_port_min && loader_opcode == MASTER_LOADER) {
    event_logger_port_number = get_ns_port_defined(global_settings->ns_use_port_max, global_settings->ns_use_port_min, 0, &el_lfd, 0);
  }
  else{
    /*Listen on any PORT and return the PORT as this process is forked by PARENT so we will close this in PARENT*/
    event_logger_port_number = init_parent_listner_socket_new_v2(&el_lfd, global_settings->event_logger_port, 1);
    if(event_logger_port_number == 0) {
      NS_EXIT(-1, "Error in creating Parent TCP listen socket.");
    }
  }

  NSTL1(NULL, NULL, "Event Logger is listening on %d port.", event_logger_port_number);
  sprintf(port_str, "%d", event_logger_port_number);
  num_proc_to_handle = total_client_entries >0 ?
			 (global_settings->num_process + 1) * total_client_entries:
			 (global_settings->num_process + 1); 

  sprintf(num_proc_str, "%d", num_proc_to_handle); 
  sprintf(filter_str, "%d", global_settings->filter_mode);
  sprintf(fd_str, "%d", el_lfd);
  sprintf(shm_id_str, "%d", shm_id);
  sprintf(num_total_event_id_str, "%d", num_total_event_id);
  sprintf(big_buf_shmid_str, "%d", big_buf_shmid);
  sprintf(test_idx_str, "%d", testidx);
  sprintf(debug_str, "%d", group_default_settings->debug);

  NSDL2_PARENT(NULL, NULL, "num_proc = %d, filter = %d "
                           "fd = %d, shm_id = %d "
                           "num_total_event_id = %d "
                           "big_buf_shmid = %d "
                           "test_idx = %d "
                           "debug = %d", 
                           "TestRunInfoShmKey = %d",
                            num_proc_to_handle, global_settings->filter_mode, el_lfd,
                            shm_id, num_total_event_id, big_buf_shmid,
                            testidx, group_default_settings->debug,
                            shr_mem_new_tr_key); 

  /*Create process for event*/


  sprintf(pgm_name, "%s/bin/nsa_log_mgr", g_ns_wdir);
  if(nslb_recover_component_or_not(&ReaderRecoveryData) == 0)
  {
     NSTL1(NULL, NULL, "Main thread, nsa log manager is not running. Recovering nsa_log_mgr.");

    if ((nsa_log_mgr_pid = fork()) ==  0 ) {
      int do_not_close_list[1];
      do_not_close_list[0] = el_lfd;

      nslb_close_all_open_files(-1, 1, do_not_close_list);
      //if (execlp("nsa_log_mgr", "nsa_log_mgr",
      if (execlp(pgm_name, pgm_name,port_str, num_proc_str, fd_str,
				 filter_str, shm_id_str, num_total_event_id_str,
			   big_buf_shmid_str, g_ns_tmpdir, test_idx_str, debug_str, shr_mem_new_tr_key, NULL) == -1) {
         //NSTL1_OUT(NULL, NULL, "initialize event logger: error in execl\n");
         perror("execl");
         NSTL1(NULL, NULL, "Error in starting nsa_log_mgr. pgm_name is %s. Error = %s", pgm_name, nslb_strerror(errno));
         NS_EXIT(-1, CAV_ERR_1000037, pgm_name, errno, nslb_strerror(errno));
      }
    } else {
    /*Close as it has been inherited  to nsa_log_mgr now close it*/
    if (nsa_log_mgr_pid < 0) {
      //NSTL1_OUT(NULL, NULL, "error in forking the event logging process\n");
      NSTL1(NULL, NULL, "Error in forking nsa_log_mgr. pgm_name is %s. Error = %s", pgm_name, nslb_strerror(errno));   
      if(recovery_flag == 0)
        NS_EXIT(-1, "Error in forking nsa_log_mgr. pgm_name is %s. Error = %s", pgm_name, nslb_strerror(errno));
    }
    close(el_lfd);
   }
  }
  else
  {
    NSTL1(NULL, NULL, "Main thread, nsa_log_mgr  max restart count is over. Cannot recover NLM"
          " Retry count = %d, Max Retry count = %d", ReaderRecoveryData.retry_count, ReaderRecoveryData.max_retry_count);
  }

  if(recovery_flag == 0)
    NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_DEBUG, __FILE__, (char*)__FUNCTION__, "Test debug event at end of init_event_logger()");
}

static void chk_connect_to_nsa_log_mgr(Msg_com_con *mccptr)
{
  int con_state;
  char err_msg[2 * 1024] = "\0";

  NSTL1(NULL, NULL, "State is NS_CONNECTING so try to reconnect for fd %d and to server ip = %s, port = %d", mccptr->fd, mccptr->ip, event_logger_port_number );

  nslb_nb_connect(mccptr->fd, mccptr->ip, event_logger_port_number, &con_state, err_msg);
  if(con_state != NSLB_CON_CONNECTED)
  {
    NSTL1(NULL, NULL, "Still not connected. err_msg = %s", err_msg);
    //close_msg_com_con(mccptr);
    CLOSE_MSG_COM_CON(mccptr, CONTROL_MODE);
    return;
  }
  //Else socket is connected, add socket to EPOLL IN
  NSTL1(NULL, NULL, "Connected successfully with fd %d and to server ip = %s", mccptr->fd, mccptr->ip );
  mccptr->state &= ~NS_CONNECTING;
  mccptr->state |= NS_CONNECTED;

  mod_select_msg_com_con((char *)mccptr, mccptr->fd, EPOLLIN | EPOLLERR | EPOLLHUP);
  return;
}

/* For Event Logging*/
void handle_nsa_log_mgr(struct epoll_event *pfds, void *cptr, int i, u_ns_ts_t now) {
  //char got_error = 1;

  Msg_com_con *mccptr =  (Msg_com_con *)pfds[i].data.ptr;

  NSDL2_MESSAGES(NULL, NULL, "Method Called, now = %u, i = %d", now, i);

  if (pfds[i].events & EPOLLOUT) {
    //got_error = 0;
    if(global_settings->enable_event_logger) {
      if (mccptr->state & NS_STATE_WRITING) {
         if(write_msg(mccptr, NULL, 0, 0, CONTROL_MODE) < 0)
           goto NLM_EXIT_TL;
         else 
           return;
      }
      else if (mccptr->state & NS_CONNECTING) {
        chk_connect_to_nsa_log_mgr(mccptr);         
        return;
      }
      else {
         NSDL3_MESSAGES(NULL, NULL, "Event logger Write state not `writing', still we go EPOLLOUT event (in child)");
         return;
      }
    }
  } else if (pfds[i].events & EPOLLIN) {  // For nsa_log_mgr IN event should not come in +ve case
      NSDL3_MESSAGES(NULL, NULL, "EPOLLIN occured on sock %s. error = %s",
                                  msg_com_con_to_str(mccptr), nslb_strerror(errno));
      //close_msg_com_con(mccptr);
      CLOSE_MSG_COM_CON(mccptr, CONTROL_MODE);
  } else if (pfds[i].events & EPOLLERR) { // Error
      NSDL3_MESSAGES(NULL, NULL, "EPOLLERR occured on sock %s. error = %s",
                                  msg_com_con_to_str(mccptr), nslb_strerror(errno));
      //close_msg_com_con(mccptr);
      CLOSE_MSG_COM_CON(mccptr, CONTROL_MODE);
  } else if (pfds[i].events & EPOLLHUP) { // Error
      NSDL3_MESSAGES(NULL, NULL, "EPOLLHUP occured on sock %s. error = %s",
                                  msg_com_con_to_str(mccptr), nslb_strerror(errno));
      //close_msg_com_con(mccptr);
      CLOSE_MSG_COM_CON(mccptr, CONTROL_MODE);
  }

  NLM_EXIT_TL:
  if(my_port_index == 255)
  {
    NSTL1(NULL, NULL, "Parent: Connection with nsa_log_mgr is closed or error on this connection.");   
  }
  else
  {
    NSTL1(NULL, NULL, "NVM%d: Connection with nsa_log_mgr is closed or error on this connection.", my_port_index);   
  }
  event_logger_port_number = -1;

}

/*
---------------------------------------------------------------------------------------------------------------------------
| Total Size | OPCODE | Time Stamp | NVM ID | User ID | Session ID | Event ID | Source | Severity | Attribute(s) | Message |
---------------------------------------------------------------------------------------------------------------------------
*/

#define CHK_AND_CREATE_EL_FILE() \
{ \
  if(elog_fd <=0 ) { \
    /* Temporarily we are commenting below written fprintf, but there is a need to find some permanent solution. \
       NSTL1_OUT(NULL, NULL, "Info: nsa log mgr is not available to send events for NVM '%d'. Writing event in the file\n", my_port_index); */ \
 \
    sprintf(event_log_file, "%s/logs/%s/event.log", g_ns_wdir, global_settings->tr_or_partition); \
open_event_log_file(event_log_file, O_CREAT | O_WRONLY | O_APPEND | O_LARGEFILE | O_CLOEXEC, 0); \
  } \
}

# define LOG_EVENT_IN_FILE() \
{ \
  if(filter_event == 0) \
  { \
    len = sprintf(event_buf, "%02u:%02u:%02u", (ts/1000) / 3600, ((ts/1000) % 3600) / 60, ((ts/1000) % 3600) % 60); \
    len = sprintf(event_buf, "%s|%u|%s|%s:%d:%d:%d|%s|%s|%s|%s|%s\n", \
          event_buf, eid, severity_str[severity], \
          "127.0.0.1", my_port_index, (int)uid, (int)sid, src_type[src], host, \
       	  (event_def_shr_mem != NULL)?BIG_BUF_MEMORY_CONVERSION(event_def_shr_mem[hash_code].attr_name):RETRIEVE_BUFFER_DATA(event_def[hash_code].attr_name), \
       			attr, msg); \
       if(write(elog_fd, event_buf, len) < 0) { \
         /*chk for nsa_log_mgr process*/ \
         NSTL1_OUT(NULL, NULL, "NVM: %d, unable to write event to event.log. Msg %s\n", my_port_index, msg); \
       } \
  } \
  else \
  { \
    /* Show event message on screen */ \
    NSTL1_OUT(NULL, NULL, "%s\n", msg); \
  } \
} \

/* In case of monitors attribute 1 would be used to fill host name
 * for other cases we will be using global variable*/
#define CHECK_HOST_NAME \
  if (!strcmp(src_type[src], "Monitor")) \
    strcpy(host_name, attr1);\
  else\
    strcpy(host_name, (char *)global_settings->event_generating_host);  


static void ns_el(unsigned int eid, unsigned int uid, unsigned int sid,
           unsigned char src, unsigned char severity, char* host, unsigned short host_name_len, char* attr, unsigned short attr_len,
           char *msg, unsigned short msg_len) {

  char filter_event = 0;
  int event_head_idx;
  //char buf[MAX_BUF_LEN_FOR_EL + 1];
  //char *el_ptr = el_buf;
  char *el_ptr = g_tls.buffer;
  unsigned int tot_el_size = 0;
  unsigned int size_to_send = 0;
  unsigned int ts;
  unsigned short  opcode = EVENT_MSG_LOG;
  //char event_buf[MAX_BUF_LEN_FOR_EL + 1];
  char *event_buf = g_tls.buffer;
  char eid_str[16];
  int len;
  int hash_code;
  char event_log_file[1024];
  Msg_com_con *mccptr;
  char conn_mode = CONTROL_MODE;

  NSDL2_LOGGING(NULL, NULL, "Method called, event_log = %d", global_settings->event_log); 

  /*Do not log events*/
  /*if(!global_settings->event_log) {
    return;
  }
  */
  if(global_settings->test_start_time > 0 ) {
    ts = get_ms_stamp() - global_settings->test_start_time; 
  } else {
    ts = 0;
  }

  NSDL2_LOGGING(NULL, NULL,  "eid = %u, uid = %u, sid = %u, src = %d,"
                             "attr_len = %hu, attribute = %s, msg_len = %hu, message = %s"
                             "host_name_len = %hu, host = %s",
                             eid, uid, sid, src, attr_len, attr, msg_len, msg, host_name_len, host);

  /*write to nsa_log_mgr*/
  if(global_settings->enable_event_logger) {
    /* Here is 2 cases:
      1) nsa log mgr yet not started so log events at own level without filtering.
         so nsa_log_mgr_pid will be -1
      2) nsa log mgr started but died due some reasons or connection is closed
	 in this case we have fd = -1 
      */
    //if((nsa_log_mgr_pid > 0 || send_events_to_master) && g_el_subproc_msg_com_con.fd > 0) {
    if(ISCALLER_DATA_HANDLER) {
      mccptr = &g_dh_el_subproc_msg_com_con;
      conn_mode = DATA_MODE;
    }
    else {
      mccptr = &g_el_subproc_msg_com_con;
      conn_mode = CONTROL_MODE;
    }

    if((mccptr->fd > 0) && (mccptr->state & NS_CONNECTED))   {
       
       // For Total Size
       tot_el_size += UNSIGNED_INT_SIZE;

       el_ptr += UNSIGNED_INT_SIZE; // For total size
       // Copy OPCODE 
       memcpy(el_ptr, &opcode, UNSIGNED_SHORT);
       el_ptr += UNSIGNED_SHORT;
       tot_el_size  += UNSIGNED_SHORT;

       // Copy Time stamp 
       memcpy(el_ptr, &ts, UNSIGNED_INT_SIZE);
       el_ptr += UNSIGNED_INT_SIZE;
       tot_el_size  += UNSIGNED_INT_SIZE;

       // Copy NVM ID 
       memcpy(el_ptr, &my_port_index, UNSIGNED_CHAR);
       el_ptr += UNSIGNED_CHAR;
       tot_el_size  += UNSIGNED_CHAR;

       // Copy USER ID 
       memcpy(el_ptr, &uid, UNSIGNED_INT_SIZE);
       el_ptr += UNSIGNED_INT_SIZE;
       tot_el_size  += UNSIGNED_INT_SIZE;

       // Copy SESSION ID 
       memcpy(el_ptr, &sid, UNSIGNED_INT_SIZE);
       el_ptr += UNSIGNED_INT_SIZE;
       tot_el_size  += UNSIGNED_INT_SIZE;

       // Copy Event ID 
       memcpy(el_ptr, &eid, UNSIGNED_INT_SIZE);
       el_ptr += UNSIGNED_INT_SIZE;
       tot_el_size  += UNSIGNED_INT_SIZE;

       // Copy SRC Core, Script, Monitors ...
       memcpy(el_ptr, &src, UNSIGNED_CHAR);
       el_ptr += UNSIGNED_CHAR;
       tot_el_size  += UNSIGNED_CHAR;

       // Copy Severity 
       memcpy(el_ptr, &severity, UNSIGNED_CHAR);
       el_ptr += UNSIGNED_CHAR;
       tot_el_size  += UNSIGNED_CHAR;

       // Copy Host length and name
       memcpy(el_ptr, &host_name_len, UNSIGNED_SHORT);
       el_ptr += UNSIGNED_SHORT;
       tot_el_size  += UNSIGNED_SHORT;

       memcpy(el_ptr, host, host_name_len);
       el_ptr += host_name_len;
       tot_el_size  += host_name_len;

       // Attributes are , seperated
       memcpy(el_ptr, &attr_len, UNSIGNED_SHORT);
       el_ptr += UNSIGNED_SHORT;
       tot_el_size  += UNSIGNED_SHORT;

       memcpy(el_ptr, attr, attr_len);
       el_ptr += attr_len;
       tot_el_size  += attr_len;

       // Copy Msg len
       memcpy(el_ptr, &msg_len, UNSIGNED_SHORT);
       el_ptr += UNSIGNED_SHORT;
       tot_el_size  += UNSIGNED_SHORT;
  
       // Copy Msg
       memcpy(el_ptr, msg, msg_len);
       el_ptr += msg_len;
       tot_el_size  += msg_len;

       size_to_send = tot_el_size - UNSIGNED_INT_SIZE;
       
       memcpy(g_tls.buffer, &size_to_send, UNSIGNED_INT_SIZE);

       write_msg(mccptr, g_tls.buffer, tot_el_size, 1, conn_mode);
    } else {  // Somehow log manager died. Now filtering will stop working. All events will get logged
      CHK_AND_CREATE_EL_FILE();

      sprintf(eid_str, "%u", eid);
      // -1 is passed as we need only hash_code
      filter_event = is_event_duplicate(ts, severity, "-", eid_str, attr, &hash_code, -1); 

      LOG_EVENT_IN_FILE();
    }

   } else {  // Filter & log at own level
     /* We can use is_event_duplicate only if we after allocation of event_def_shr_mem*/
     CHK_AND_CREATE_EL_FILE();

     sprintf(eid_str, "%u", eid);
 
     if(event_def_shr_mem) {
       if (global_settings->filter_mode == LOG_FILTER_BASED_EVENT) {
         if(my_port_index != 255)
           event_head_idx = my_port_index;
         else
           event_head_idx = global_settings->num_process;
         filter_event = is_event_duplicate(ts, severity, "-", eid_str, attr, &hash_code, event_head_idx);
       } else {  // Do not filter but we need hash code so pass -1
         filter_event = is_event_duplicate(ts, severity, "-", eid_str, attr, &hash_code, -1);
       }
     }else{
       // Do not filter but we need hash code so pass -1
       filter_event = is_event_duplicate(ts, severity, "-", eid_str, attr, &hash_code, -1);
     }
     
     LOG_EVENT_IN_FILE();
  }
}

/* function to log events and also print to console*/
void print_core_events(char *function, char *file, char *format, ...) {

  char err_msg[2048 + 1];
  int amt_written = 0;
  va_list ap;

  va_start (ap, format);
  amt_written = vsnprintf(err_msg, 2048, format, ap);
  va_end(ap);

  if(amt_written > 0) {
    amt_written = strlen(err_msg);
    if(amt_written > 2048)
      amt_written = 2048;
  } else {
    amt_written = 2048;
  }

  err_msg[amt_written] = '\0';
  //NSTL1_OUT(NULL, NULL, "%s\n", err_msg);
  NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_CRITICAL,
                                     file, function, err_msg);
}

#define APPEND_COMMA(attr_list, attr_len)\
{\
  attr_list[attr_len++] = ','; \
  attr_list[attr_len++] = ' '; \
}\

/*For ONE attributes*/
void ns_el_1_attr(unsigned int eid, unsigned int uid, unsigned int sid,
                  unsigned char src, unsigned char severity, char* attr1, char *format, ...) {

  va_list ap;
  int amt_written = 0;
  unsigned short msg_len, attr_len;
  unsigned short host_name_len;
  unsigned short max_len = MAX_EVENT_ATTR_SIZE;
  char host_name[MAX_HOST_NAME_SIZE];
  char ev_msg[MAX_EVENT_DESC_SIZE +  1];
  char encoded_msg[MAX_EVENT_DESC_SIZE +  1];
  char attr_list[MAX_EVENT_ATTR_SIZE + 1];

  if(severity > EVENT_CRITICAL) {
    NSEL_MAJ(NULL, NULL, ERROR_ID, ERROR_ATTR, "Invalid severity (%d) for event (%u) must be between (%d - %d)", 
			 severity, eid, EVENT_CLEAR, EVENT_CRITICAL);
    return;
  }
  
  NSDL2_LOGGING(NULL, NULL, "Method called");
  attr_len = nslb_event_copy_and_encode_special_chars(attr1, strlen(attr1), max_len, attr_list); 

  //Update host name and length with respect to source
  CHECK_HOST_NAME 
  host_name_len = strlen(host_name);

  va_start (ap, format);
  amt_written = vsnprintf(ev_msg, MAX_EVENT_DESC_SIZE, format, ap);
  va_end(ap);

  if(amt_written > 0) {
    amt_written = strlen(ev_msg);
    if(amt_written > MAX_EVENT_DESC_SIZE)
      amt_written = MAX_EVENT_DESC_SIZE;
  } else {
    amt_written = MAX_EVENT_DESC_SIZE;
  }

  ev_msg[amt_written] = '\0';
  msg_len = nslb_event_copy_and_encode_special_chars(ev_msg, amt_written, MAX_EVENT_DESC_SIZE, encoded_msg);
  
  ns_el(eid, uid, sid, src, severity, host_name, host_name_len, attr_list, attr_len, encoded_msg, msg_len);
}

/*For TWO attributes*/
void ns_el_2_attr(unsigned int eid, unsigned int uid, unsigned int sid,
                  unsigned char src, unsigned char severity, char* attr1, char* attr2, char *format, ...) {

  va_list ap;
  char ev_msg[MAX_EVENT_DESC_SIZE +  1];
  char encoded_msg[MAX_EVENT_DESC_SIZE +  1];
  char attr_list[MAX_EVENT_ATTR_SIZE + 1];
  int amt_written = 0;
  unsigned short msg_len, attr_len;
  unsigned short max_len = MAX_EVENT_ATTR_SIZE;
  char host_name[MAX_HOST_NAME_SIZE];
  unsigned short host_name_len;
  
  if(severity > EVENT_CRITICAL) {
    NSEL_MAJ(NULL, NULL, ERROR_ID, ERROR_ATTR, "Invalid severity (%d) for event (%u) must be between (%d - %d)", 
			 severity, eid, EVENT_CLEAR, EVENT_CRITICAL);
    return;
  }

  NSDL2_LOGGING(NULL, NULL, "Method called");

  attr_len = nslb_event_copy_and_encode_special_chars(attr1, strlen(attr1), max_len, attr_list); 

  APPEND_COMMA(attr_list, attr_len);

  max_len = MAX_EVENT_ATTR_SIZE > attr_len?(MAX_EVENT_ATTR_SIZE - attr_len):0;
  if(max_len)
    attr_len += nslb_event_copy_and_encode_special_chars(attr2, strlen(attr2), max_len, attr_list + attr_len); 
  //Update host name and length with respect to source
  CHECK_HOST_NAME 
  host_name_len = strlen(host_name);

  va_start (ap, format);
  amt_written = vsnprintf(ev_msg, MAX_EVENT_DESC_SIZE, format, ap);
  va_end(ap);

  if(amt_written > 0) {
    amt_written = strlen(ev_msg);
    if(amt_written > MAX_EVENT_DESC_SIZE)
      amt_written = MAX_EVENT_DESC_SIZE;
  } else {
    amt_written = MAX_EVENT_DESC_SIZE;
  }

  ev_msg[amt_written] = '\0';
  msg_len = nslb_event_copy_and_encode_special_chars(ev_msg, amt_written, MAX_EVENT_DESC_SIZE, encoded_msg);
 
  ns_el(eid, uid, sid, src, severity, host_name, host_name_len, attr_list, attr_len, encoded_msg, msg_len);
}

/*For THREE attributes*/
void ns_el_3_attr(unsigned int eid, unsigned int uid, unsigned int sid,
                  unsigned char src, unsigned char severity, char* attr1, char* attr2, char* attr3, char *format, ...) {

  va_list ap;
  char ev_msg[MAX_EVENT_DESC_SIZE +  1]; 
  char encoded_msg[MAX_EVENT_DESC_SIZE +  1];
  char attr_list[MAX_EVENT_ATTR_SIZE + 1];
  int amt_written = 0;
  unsigned short msg_len, attr_len;
  unsigned short max_len = MAX_EVENT_ATTR_SIZE;
  char host_name[MAX_HOST_NAME_SIZE]; //Host name
  unsigned short host_name_len;

  if(severity > EVENT_CRITICAL) {
    NSEL_MAJ(NULL, NULL, ERROR_ID, ERROR_ATTR, "Invalid severity (%d) for event (%u) must be between (%d - %d)", 
			 severity, eid, EVENT_CLEAR, EVENT_CRITICAL);
    return;
  }
 
  NSDL2_LOGGING(NULL, NULL, "Method called, attr1 = %s", attr1);
  attr_len = nslb_event_copy_and_encode_special_chars(attr1, strlen(attr1), max_len, attr_list);
  APPEND_COMMA(attr_list, attr_len);
    
  max_len = MAX_EVENT_ATTR_SIZE > attr_len?(MAX_EVENT_ATTR_SIZE - attr_len):0;
  if(max_len)
  {
    attr_len += nslb_event_copy_and_encode_special_chars(attr2, strlen(attr2), max_len, attr_list + attr_len);
    APPEND_COMMA(attr_list, attr_len);
  }
  
  max_len = MAX_EVENT_ATTR_SIZE > attr_len?(MAX_EVENT_ATTR_SIZE - attr_len):0;
  if(max_len)
    attr_len += nslb_event_copy_and_encode_special_chars(attr3, strlen(attr3), max_len, attr_list + attr_len);

  //Update host name and length with respect to source
  CHECK_HOST_NAME 
  host_name_len = strlen(host_name);

  va_start (ap, format);
  amt_written = vsnprintf(ev_msg, MAX_EVENT_DESC_SIZE, format, ap);
  va_end(ap);

  if(amt_written > 0) {
    amt_written = strlen(ev_msg);
    if(amt_written > MAX_EVENT_DESC_SIZE)
      amt_written = MAX_EVENT_DESC_SIZE;
  } else {
    amt_written = MAX_EVENT_DESC_SIZE;
  }

  ev_msg[amt_written] = '\0';
  msg_len = nslb_event_copy_and_encode_special_chars(ev_msg, amt_written, MAX_EVENT_DESC_SIZE, encoded_msg);
  ns_el(eid, uid, sid, src, severity, host_name, host_name_len, attr_list, attr_len, encoded_msg, msg_len);
}

/*For Four attributes*/
void ns_el_4_attr(unsigned int eid, unsigned int uid, unsigned int sid,
                  unsigned char src, unsigned char severity, char* attr1, char* attr2, char* attr3, char *attr4, char *format, ...) {

  va_list ap;
  char ev_msg[MAX_EVENT_DESC_SIZE +  1]; 
  char encoded_msg[MAX_EVENT_DESC_SIZE +  1];
  char attr_list[MAX_EVENT_ATTR_SIZE + 1];
  int amt_written = 0;
  unsigned short msg_len, attr_len;
  unsigned short max_len = MAX_EVENT_ATTR_SIZE;
  char host_name[MAX_HOST_NAME_SIZE]; //Host name
  unsigned short host_name_len;
  
  if(severity > EVENT_CRITICAL) {
    NSEL_MAJ(NULL, NULL, ERROR_ID, ERROR_ATTR, "Invalid severity (%d) for event (%u) must be between (%d - %d)", 
			 severity, eid, EVENT_CLEAR, EVENT_CRITICAL);
    return;
  }
 
  NSDL2_LOGGING(NULL, NULL, "Method called");

  attr_len = nslb_event_copy_and_encode_special_chars(attr1, strlen(attr1), max_len, attr_list); 
  APPEND_COMMA(attr_list, attr_len);
  
  max_len = MAX_EVENT_ATTR_SIZE > attr_len?(MAX_EVENT_ATTR_SIZE - attr_len -1):0;
  if(max_len)
  {
    attr_len += nslb_event_copy_and_encode_special_chars(attr2, strlen(attr2), max_len, attr_list + attr_len); 
    APPEND_COMMA(attr_list, attr_len);
  }

  max_len = MAX_EVENT_ATTR_SIZE > attr_len?(MAX_EVENT_ATTR_SIZE - attr_len -1):0;
  if(max_len)
  {
    attr_len += nslb_event_copy_and_encode_special_chars(attr3, strlen(attr3), max_len, attr_list + attr_len); 
    APPEND_COMMA(attr_list, attr_len);
  }
  
  max_len = MAX_EVENT_ATTR_SIZE > attr_len?(MAX_EVENT_ATTR_SIZE - attr_len -1):0;
  if(max_len)
    attr_len += nslb_event_copy_and_encode_special_chars(attr4, strlen(attr4), max_len, attr_list + attr_len); 
  //Update host name and length with respect to source
  CHECK_HOST_NAME 
  host_name_len = strlen(host_name);

  va_start (ap, format);
  amt_written = vsnprintf(ev_msg, MAX_EVENT_DESC_SIZE, format, ap);
  va_end(ap);
  
  if(amt_written > 0) {
    amt_written = strlen(ev_msg);
    if(amt_written > MAX_EVENT_DESC_SIZE)
      amt_written = MAX_EVENT_DESC_SIZE;
  } else {
    amt_written = MAX_EVENT_DESC_SIZE;
  }

  ev_msg[amt_written] = '\0';
  msg_len = nslb_event_copy_and_encode_special_chars(ev_msg, amt_written, MAX_EVENT_DESC_SIZE, encoded_msg);
 
  ns_el(eid, uid, sid, src, severity, host_name, host_name_len, attr_list, attr_len, encoded_msg, msg_len);
}

void ns_el_5_attr(unsigned int eid, unsigned int uid, unsigned int sid,
                  unsigned char src, unsigned char severity, 
                  char* attr1, char* attr2, char* attr3, char *attr4, char *attr5,
                  char *format, ...) {

  va_list ap;
  char ev_msg[MAX_EVENT_DESC_SIZE +  1]; 
  char encoded_msg[MAX_EVENT_DESC_SIZE +  1];
  char attr_list[MAX_EVENT_ATTR_SIZE + 1];
  int amt_written = 0;
  unsigned short msg_len, attr_len;
  unsigned short max_len = MAX_EVENT_ATTR_SIZE;
  char host_name[MAX_HOST_NAME_SIZE]; //Host name
  unsigned short host_name_len; //Host name length
  
  if(severity > EVENT_CRITICAL) {
    NSEL_MAJ(NULL, NULL, ERROR_ID, ERROR_ATTR, "Invalid severity (%d) for event (%u) must be between (%d - %d)", 
			 severity, eid, EVENT_CLEAR, EVENT_CRITICAL);
    return;
  }
 
  NSDL2_LOGGING(NULL, NULL, "Method called");

  attr_len = nslb_event_copy_and_encode_special_chars(attr1, strlen(attr1), max_len, attr_list); 
  APPEND_COMMA(attr_list, attr_len);

  max_len = MAX_EVENT_ATTR_SIZE > attr_len?(MAX_EVENT_ATTR_SIZE - attr_len -1):0;
  if(max_len)
  {
    attr_len += nslb_event_copy_and_encode_special_chars(attr2, strlen(attr2), max_len, attr_list + attr_len); 
    APPEND_COMMA(attr_list, attr_len);
  }

  max_len = MAX_EVENT_ATTR_SIZE > attr_len?(MAX_EVENT_ATTR_SIZE - attr_len -1):0;
  if(max_len)
  {
    attr_len += nslb_event_copy_and_encode_special_chars(attr3, strlen(attr3), max_len, attr_list + attr_len); 
    APPEND_COMMA(attr_list, attr_len);
  }
  
  max_len = MAX_EVENT_ATTR_SIZE > attr_len?(MAX_EVENT_ATTR_SIZE - attr_len -1):0;
  if(max_len)
  {
    attr_len += nslb_event_copy_and_encode_special_chars(attr4, strlen(attr4), max_len, attr_list + attr_len); 
    APPEND_COMMA(attr_list, attr_len);
  }

  max_len = MAX_EVENT_ATTR_SIZE > attr_len?(MAX_EVENT_ATTR_SIZE - attr_len -1):0;
  if(max_len)
    attr_len += nslb_event_copy_and_encode_special_chars(attr5, strlen(attr5), max_len, attr_list + attr_len); 
  //Update host name and length with respect to source
  CHECK_HOST_NAME 
  host_name_len = strlen(host_name);

  va_start (ap, format);
  amt_written = vsnprintf(ev_msg, MAX_EVENT_DESC_SIZE, format, ap);
  va_end(ap);
  
  if(amt_written > 0) {
    amt_written = strlen(ev_msg);
    if(amt_written > MAX_EVENT_DESC_SIZE)
      amt_written = MAX_EVENT_DESC_SIZE;
  } else {
    amt_written = MAX_EVENT_DESC_SIZE;
  }

  ev_msg[amt_written] = '\0';
  msg_len = nslb_event_copy_and_encode_special_chars(ev_msg, amt_written, MAX_EVENT_DESC_SIZE, encoded_msg);
 
  ns_el(eid, uid, sid, src, severity, host_name, host_name_len, attr_list, attr_len, encoded_msg, msg_len);
}
#if 0
/*Return index to severity in severity_str*/
static int convert_sever_string_to_int(char *sever) {
  int severity_index;
    
  for(severity_index = 0; severity_index <= 5; severity_index++) { 
    if(!strcasecmp(severity_str[severity_index], sever))
      return severity_index;
  } 
  //returning default warning
  return EVENT_WARNING;
}   
#endif
    
#if 0
/**
Purpose         : This method is to print event definition file structure fields during initialization --  only for debuging 
Arguments       : 
		 - hashcode_index
Return Value    : None
**/
void print_event_def(int hashcode_index)
{

  printf("event_def[%d].event_id = %d\n", hashcode_index, event_def[hashcode_index].event_id);
  printf("event_def[%d].event_name = %s\n", hashcode_index, event_def[hashcode_index].event_name);
  printf("event_def[%d].attr_name = %s\n", hashcode_index, event_def[hashcode_index].attr_name);
  printf("event_def[%d].filter_mode = %d\n", hashcode_index, event_def[hashcode_index].filter_mode);
  //printf("event_def[%d].mode_based_param = %s\n", hashcode_index, event_def[hashcode_index].mode_based_param);
  printf("event_def[%d].event_detail_desc = %s\n", hashcode_index, event_def[hashcode_index].event_detail_desc);
  printf("event_def[%d].event_list = %s\n", hashcode_index, (char*) event_def[hashcode_index].event_head);
}

/**
Purpose         : This method is to debug log to print fields of events ""linked list 
Arguments       : 
		 - head pointer to events linked list
Return Value    : None
**/
void print_events_list(Events *head)
{
  int num_of_events = 0;
  if(head != NULL)
  {
    while(head !=NULL)
    {
      num_of_events++;
      printf("head->event_value= %s head->attr_value = %s state = %d \t", head->event_value, head->attr_value, head->state);
      head = head->next;
    }
    printf("\n");
  }
  else
    printf("The list is empty\n");
}
#endif

/**
Purpose         : This method is to extract event id from netstorm_events.dat and customer_events.dat and
		  write in event_id.txt
Arguments       : None 
Return Value    : None
**/
static void create_event_id_file(char *file_name) {
  char event_id_cmd[10 * 1024];
  //char custom_file_name[MAX_BUF_SIZE_FOR_EVENTS]="\0";
  FILE *cmd_fp;
 
  NSDL2_PARENT(NULL, NULL, "Method called, file_name = %s", file_name);

  sprintf(event_id_cmd, "awk -F'|' '!/^#/ {if(NF == 10) print $1}' %s"
			"| sort -u > %s/%s/event_id.txt",
			file_name, g_ns_wdir, g_ns_tmpdir);

  if ((cmd_fp = popen(event_id_cmd, "r")) == NULL) {
    NSTL1(NULL, NULL, "%s: error in calling popen for %s\n", (char*)__FUNCTION__, event_id_cmd);
    NS_EXIT (-1, "%s: error in calling popen for %s", (char*)__FUNCTION__, event_id_cmd); 
  }

  pclose(cmd_fp);
}

/*Copy local memory structure to shared one*/
/* Here we keep index of big buf in shared memory not the pointers,
 * address of pointer will same but after attaching big_buf_shr_mem start address may change*/
void copy_event_def_to_shr_mem(int id) {

  int i, event_id_count = 0;
  int event_head;
  int num_event_head_per_event_id = 0;

  NSDL2_PARENT(NULL, NULL, "Method called, num_total_event_id = %d", num_total_event_id);
  /*Do not create shared memory as we will not send any event log to nsa log mgr*/
  if(global_settings->filter_mode == DO_NOT_LOG_EVENT) {
    NSDL4_PARENT(NULL, NULL, "Event filter mode is set to DO NOT LOG EVENT returning.");
    return;
  }  

  big_buf_shmid = id;
  
  if(num_total_event_id) {

    /* nsa_log_mgr is enabled:  In this case event head is locally allocated
     * 				by nsa_log_mgr as only this process will filter 
     * nsa_log_mgr is disabled:
     * 				In this case event head must be allocated here in
     * 				parent as all children will do filter at own level
     */

    if(!global_settings->enable_event_logger && global_settings->filter_mode == LOG_FILTER_BASED_EVENT) {
        num_event_head_per_event_id = num_actual_event_id * (global_settings->num_process + 1);
        events_head_shr_mem = (Events**)do_shmget_with_id(sizeof(Events*) * num_event_head_per_event_id,
									   "Events head", &event_head);
        memset(events_head_shr_mem, 0, sizeof(Events*) * num_event_head_per_event_id);
    }

    /* we need follwoing in 2 cases
     * Case 1: We need no filtering, as we have to get attribute name
     * Case 2: We need filtering, as we need all these fields to filter 
     */ 
    event_def_shr_mem = (EventDefinitionShr*)do_shmget_with_id(sizeof(EventDefinitionShr) * num_total_event_id, "Event Definition", &shm_id);
    for(i = 0; i < num_total_event_id; i++) {
       event_def_shr_mem[i].event_id          = event_def[i].event_id ; 
       event_def_shr_mem[i].event_name        = event_def[i].event_name ; 
       event_def_shr_mem[i].attr_name         = event_def[i].attr_name ; 
       event_def_shr_mem[i].filter_mode       = event_def[i].filter_mode ; 
       event_def_shr_mem[i].mode_based_param  = event_def[i].mode_based_param ; 
       event_def_shr_mem[i].future4           = event_def[i].future4 ; 
       event_def_shr_mem[i].future3           = event_def[i].future3 ; 
       event_def_shr_mem[i].future2           = event_def[i].future2 ; 
       event_def_shr_mem[i].future1           = event_def[i].future1 ; 
       event_def_shr_mem[i].event_detail_desc = event_def[i].event_detail_desc ; 

       if(events_head_shr_mem == NULL) {
          event_def_shr_mem[i].event_head = NULL; 
       } else {
          /* event_id_count can not be greater than num_actual_event_id,
           * because num_actual_event_id is calculated as non zero event ids.*/
          if(event_def_shr_mem[i].event_id && event_id_count < num_actual_event_id) {
 	    event_def_shr_mem[i].event_head = &events_head_shr_mem[event_id_count * (global_settings->num_process + 1)]; 
	    event_id_count++;
          } else {
            event_def_shr_mem[i].event_head = NULL; 
          }
       }
    }    
  }
}


/**
Purpose         : This method create hash code table from unique event id's
Arguments       : None
Return Value    : None
**/
static void create_events_hash_code_table() {

  char event_id_file[MAX_BUF_SIZE_FOR_EVENTS];
  char path[1024];
  Hash_code_to_str_ex var_get_key;

  sprintf(path, "%s/%s", g_ns_wdir, g_ns_tmpdir);
  sprintf(event_id_file, "event_id.txt");
  num_total_event_id = generate_hash_table_ex(event_id_file, "event_id_hash_fun", &var_event_hash_func, &var_get_key, NULL, NULL, NULL, 0, path);
  
  MY_MALLOC(event_def, sizeof(EventDefinition) * num_total_event_id, "Event Definition tabke", num_total_event_id);
  memset(event_def, 0, sizeof(EventDefinition) * num_total_event_id);
}

/**
 *  0         1           2                   3              4               5      6       7       8           9
 * Event ID|Event Name|Attribute Name List|Filtering Mode|Mode Based Param|Sever|Future2|future3|Future4|Event Description
Purpose         : This method is to initialize Event definition structure from netstorm_events.dat and custom_events.dat
Arguments       : 
		 - Event Definition File Name 
Return Value    : None 
**/
static void init_event_definition(char *file_name) {
  char buf[MAX_BUF_SIZE_FOR_EVENTS * 10];
  char *fields[EVENT_DEF_FILE_FIELD];
  int hashcode_index = 0;
  int line = 0;
  FILE *fp;
  
  if((fp = fopen(file_name, "r")) == NULL) {
    NSTL1(NULL, NULL, "Error in opening event definition file %s\n", file_name);
    NS_EXIT(-1, CAV_ERR_1000006, file_name, errno, nslb_strerror(errno));
  }

  //This read file and parse the fields
  while(fgets(buf, MAX_BUF_SIZE_FOR_EVENTS * 10, fp) != NULL) {
    line++;
    buf[strlen(buf)-1] = '\0'; // Remove new line

    if((buf[0] == '#') || (buf[0] == '\0'))   //Check for Comment Line and Blank Line
	continue;

    if((get_tokens(buf, fields, "|", EVENT_DEF_FILE_FIELD)) != EVENT_DEF_FILE_FIELD) {
      NSTL1(NULL, NULL, "Error: Syntax error in event definition file. At Line %d on '%s'\n", line, buf);
      NS_EXIT(-1, "Error: Syntax error in event definition file. At Line %d on '%s'\n", line, buf);
    }

    //getting hash code using event Id
    hashcode_index = var_event_hash_func(fields[0], strlen(fields[0])); 
    if((hashcode_index < 0) || (hashcode_index >= num_total_event_id)) {
      NSTL1(NULL, NULL, "Hashcode for %s is out of range (%d)", fields[0], hashcode_index);
      NS_EXIT (-1, CAV_ERR_1031019, fields[0], hashcode_index, num_total_event_id);
    }
    
    /* Actual number of event ids*/
    num_actual_event_id++;
    /*Event ID*/
    if(event_def[hashcode_index].event_id) {
      NSDL2_PARENT(NULL, NULL, "Event id (%lu) data is overwritten by file %s.",
				event_def[hashcode_index].event_id, file_name);  
      NSTL1_OUT(NULL, NULL, "Event id (%lu) data is overwritten by file %s.\n",
				event_def[hashcode_index].event_id, file_name);  
    }

    event_def[hashcode_index].event_id = atoi(fields[0]);

    if(event_def[hashcode_index].event_id == 0) {
      NSTL1(NULL, NULL, "'%s' event id is not allowed at line %d.\n", fields[0], line);
      NS_EXIT(-1, "'%s' event id is not allowed at line %d.", fields[0], line);
    }
 		
    /*Event Name*/
    if ((event_def[hashcode_index].event_name = copy_into_big_buf(fields[1], 0)) == -1) {
      NSTL1(NULL, NULL, "Failed in copying %s\n", fields[1]);
      NS_EXIT(-1, CAV_ERR_1000018, fields[1]);
    }

    /*Attribute Name*/
    if ((event_def[hashcode_index].attr_name = copy_into_big_buf(fields[2], 0)) == -1) {
      NSTL1(NULL, NULL, "Failed in copying %s\n", fields[2]);
      NS_EXIT(-1, CAV_ERR_1000018, fields[2]);
    }

    /*Filter mode*/
    event_def[hashcode_index].filter_mode = atoi(fields[3]);

    /*Filter mode bases param count/time*/
    if(event_def[hashcode_index].filter_mode == 2)
      event_def[hashcode_index].mode_based_param = atoi(fields[4]);
    else if(event_def[hashcode_index].filter_mode == 3)
      event_def[hashcode_index].mode_based_param = atol(fields[4]);
    else
      event_def[hashcode_index].mode_based_param = 0;

    /*Future4*/
    if ((event_def[hashcode_index].future4 = copy_into_big_buf(fields[5], 0)) == -1) {
      NSTL1(NULL, NULL, "Failed in copying %s\n", fields[6]);
      NS_EXIT(-1, CAV_ERR_1000018, fields[6]);
    }

    /*Future3*/
    if ((event_def[hashcode_index].future3 = copy_into_big_buf(fields[6], 0)) == -1) {
      NSTL1(NULL, NULL, "Failed in copying %s\n", fields[6]);
      NS_EXIT(-1, CAV_ERR_1000018, fields[6]);
    }
    /*Future2*/
    if ((event_def[hashcode_index].future2 = copy_into_big_buf(fields[7], 0)) == -1) {
      NSTL1(NULL, NULL, "Failed in copying %s\n", fields[7]);
      NS_EXIT(-1, CAV_ERR_1000018, fields[7]);
    }
    /*Future1*/
    if ((event_def[hashcode_index].future1 = copy_into_big_buf(fields[8], 0)) == -1) {
      NSTL1(NULL, NULL, "Failed in copying %s\n", fields[8]);
      NS_EXIT(-1, CAV_ERR_1000018, fields[8]);
    }
    /*Event Description*/
    if ((event_def[hashcode_index].event_detail_desc = copy_into_big_buf(fields[9], 0)) == -1) {
      NSTL1(NULL, NULL, "Failed in copying %s\n", fields[9]);
      NS_EXIT(-1, CAV_ERR_1000018, fields[9]);
    }

    //print_event_def(hashcode_index);
  } // End of while
  fclose(fp);
}

/**
Purpose         : This method is to read netstorm_events.dat and customer_events.dat
Arguments       : None
Return Value    : None
**/
void init_events() {
  //NSDL2_LOGGING(NULL, NULL, "Method called");
  int i;
  char file_name[4 * MAX_FILE_NAME];
  char all_file_names[MAX_EVENT_DEFINITION_FILE * 4 * MAX_FILE_NAME];
  char new_link[1024];
  int len = 0;
 
  // This event is added for so that we can if event log is workign as expected before init_events() is done
  NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_DEBUG, __FILE__, (char*) __FUNCTION__, "Test debug event before init_events()");

  all_file_names[0] = '\0';
  for(i = 0; i < cur_event_definition_files; i++) {
    // Create event_id.txt 
    if(global_settings->event_file_name[i][0] == '/') {
       sprintf(file_name, "%s", global_settings->event_file_name[i]); 
       sprintf(new_link, "%s/logs/%s/%s", g_ns_wdir, global_settings->tr_or_common_files, rindex(file_name, '/') + 1);
    } else {
       sprintf(file_name, "%s/events/%s", g_ns_wdir, global_settings->event_file_name[i]); 
       sprintf(new_link, "%s/logs/%s/%s", g_ns_wdir, global_settings->tr_or_common_files, global_settings->event_file_name[i]);
    }
/*
    if(link(file_name, new_link)) 
    {
      if(strstr(nslb_strerror(errno), "File exists") == NULL) 
      { 
        NSTL1(NULL, NULL, "Error in copying %s to %s. Error = %s\n", file_name, new_link, nslb_strerror(errno));
        NS_EXIT(-1, "Error in copying %s to %s. Error = %s", file_name, new_link, nslb_strerror(errno)); 
      } 
    }
*/
    if((link(file_name, new_link)) == -1)
    {
      if((symlink(file_name, new_link)) == -1)
      {
        NSTL1(NULL, NULL, "Error: Unable to create link %s, err = %s", new_link, nslb_strerror(errno));
        if(strstr(nslb_strerror(errno), "File exists") == NULL)
        {
           NSTL1(NULL, NULL, "Error in copying %s to %s. Error = %s\n", file_name, new_link, nslb_strerror(errno));
           NS_EXIT(-1, CAV_ERR_1000033, file_name, new_link, errno, nslb_strerror(errno)); 
        }
      }
    }
    NSDL2_MESSAGES(NULL, NULL, "Created link of %s in %s", file_name, new_link);

    len = sprintf(all_file_names, "%s%s ", all_file_names, file_name); 
  } 

  all_file_names[len - 1] = '\0'; // remove space 

  create_event_id_file(all_file_names);

  // This event is added for so that we can if event log is workign as expected before init_events() is done
  NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_DEBUG, __FILE__, (char*)__FUNCTION__, "Test debug event before create_events_hash_code_table()");

  // Create hash table from event id's 
  create_events_hash_code_table(); 

  NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_DEBUG, __FILE__, (char*)__FUNCTION__, "Test debug event before init_event_definition()");
  for(i = 0; i < cur_event_definition_files; i++) {
    if(global_settings->event_file_name[i][0] == '/') {
       sprintf(file_name, "%s", global_settings->event_file_name[i]); 
    } else {
       sprintf(file_name, "%s/events/%s", g_ns_wdir, global_settings->event_file_name[i]); 
    }
    init_event_definition(file_name);
  }

  // Note - event shared memory need to be initialized after Big buf shared memory is intialized

  NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_DEBUG, __FILE__, (char*)__FUNCTION__, "Test debug event at end of init_event()");
}

#if 0

int main(int argc, char *argv[])
{
  //int index;
  char event_input_file[4000];
  FILE *read_file;
  char event_buf[1024];
  int is_dup;

  init_events();
  sprintf(event_input_file, "%s", argv[1]);
  if ((read_file = fopen(event_input_file, "r")) == NULL)
  {
    printf("Error in opening file");
    exit(-1);
  }

  while(fgets(event_buf, 1024, read_file) != NULL)
  {
    event_buf[strlen(event_buf) - 1] = '\0';  // Replace new line by Null
    if(strchr(event_buf, '#') || event_buf[0] == '\0')
      continue;
    is_dup = is_event_duplicate(event_buf);
    if(is_dup == 0)
    {
      printf("Event Logged\n");
      ns_log_event(event_buf);
    }
    else if(is_dup == 1)
      printf("Same Event Occured\n");
    else
      printf("Invalid Error\n");
    //print_events_list(event_def[hashcode_index1].event_head);
  }

  if(close(event_log_fd) < 0)
  {
    NSTL1_OUT(NULL, NULL, "Close failed for event log.\n");
    exit (-1);
  }

  return 0;
}

#endif
