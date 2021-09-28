#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>

#include "ns_msg_def.h"
#include "wait_forever.h"
#include "ns_global_settings.h"

#include "ns_msg_com_util.h"
#include "ns_log.h"
#include "ns_event_log.h"
#include "logging.h"
#include "ns_trace_level.h"

#define NS_COMP_CTRL_SUCCESS            mccptr, req_msg, 0
#define NS_COMP_CTRL_NOT_CONFIGURED     mccptr, req_msg, 1 
#define NS_COMP_CTRL_ALREDY_RUNNING     mccptr, req_msg, 2
#define NS_COMP_CTRL_START_ERROR        mccptr, req_msg, 3
#define NS_COMP_CTRL_STOP_ERROR         mccptr, req_msg, 4
#define NS_COMP_CTRL_ALREADY_STOPPED    mccptr, req_msg, 5
#define NS_COMP_CTRL_INVALID_OPR        mccptr, req_msg, 6
#define NS_COMP_CTRL_INVALID_COMP       mccptr, req_msg, 7

static void send_reply(Msg_com_con *mccptr, parent_msg *req_msg, int status, char *msg)
{
  ns_comp_ctrl_rep rep_msg;

  //Copy header from req_msg to rep_msg
  memcpy(&rep_msg, req_msg, sizeof(parent_child)); 
  rep_msg.status = status;
  strcpy(rep_msg.reply_msg, msg);
  rep_msg.msg_len = sizeof(ns_comp_ctrl_rep) - sizeof(int);
  write_msg(mccptr, (char*)&rep_msg, sizeof(ns_comp_ctrl_rep), 0, CONTROL_MODE);
}

/* NLW (logging_writter) */
static void start_NLW(Msg_com_con *mccptr, parent_msg *req_msg)
{
  NSDL2_PARENT(NULL, NULL, "Method called, logging writer pid = [%d]", writer_pid);
  NSTL1(NULL, NULL, "Starting NLW on user request. logging writer pid is %d.", writer_pid);

  if(writer_pid > 0 )
  {
    send_reply(NS_COMP_CTRL_ALREDY_RUNNING, "Error - NLW is already running");
    return;
  }
  // Now start
  writer_pid = -1;
  int ret = nsa_logger_recovery();
  
  if(ret != 0)
    send_reply(NS_COMP_CTRL_START_ERROR, "Error in starting NLW");
  else
    send_reply(NS_COMP_CTRL_SUCCESS, "Success");
    
}

static void stop_NLW(Msg_com_con *mccptr, parent_msg *req_msg)
{
  NSDL2_PARENT(NULL, NULL, "Method called, logging writer pid = [%d]", writer_pid);
  NSTL1(NULL, NULL, "Stopping NLW on user request. logging writer pid is %d.", writer_pid);

  if(writer_pid > 0)
  {
    if (kill(writer_pid, SIGTERM) == -1)
    {
      //send_reply(NS_COMP_CTRL_STOP_ERROR, "Error - Unable to stop NLW. Kill failed with error %s.", strerror(errno));
      send_reply(NS_COMP_CTRL_STOP_ERROR, "Error - Unable to stop NLW. Kill failed.");
    }
    else
    {
      writer_pid = -2;
      send_reply(NS_COMP_CTRL_SUCCESS, "Success");
    }
  }
  else
    send_reply(NS_COMP_CTRL_ALREADY_STOPPED, "Error - NLW is already stopped");
  return;
}

/* NLM */
static void start_NLM(Msg_com_con *mccptr, parent_msg *req_msg)
{
  NSDL2_PARENT(NULL, NULL, "Method called, nsa_log_mgr pid = [%d]", nsa_log_mgr_pid);
  NSTL1(NULL, NULL, "Starting NLM on user request. nsa_log_mgr_pid = %d", nsa_log_mgr_pid);
 
  if(loader_opcode == CLIENT_LOADER) {
    send_reply(NS_COMP_CTRL_NOT_CONFIGURED, "Error - NLM does not run on generators.");
    return;
  }

  if(!(global_settings->enable_event_logger)) {
    send_reply(NS_COMP_CTRL_NOT_CONFIGURED, "Error - NLM is not enabled in this test/session");
    return;
  }

  if(nsa_log_mgr_pid > 0 )
  {
    send_reply(NS_COMP_CTRL_ALREDY_RUNNING, "Error - NLM is already running");
    return;
  }

  // Now start NLM
  nsa_log_mgr_pid = -1;
  int ret = nsa_log_mgr_recovery();
  
  if(ret != 0)
    send_reply(NS_COMP_CTRL_START_ERROR, "Error in starting NLM");
  else
    send_reply(NS_COMP_CTRL_SUCCESS, "Success");
  
}

static void stop_NLM(Msg_com_con *mccptr, parent_msg *req_msg)
{
  NSDL2_PARENT(NULL, NULL, "Method called, nsa_log_mgr pid = [%d]", nsa_log_mgr_pid);
  NSTL1(NULL, NULL, "Stopping NLM on user request. nsa_log_mgr_pid is %d.", nsa_log_mgr_pid);
  
  if(loader_opcode == CLIENT_LOADER) {
    send_reply(NS_COMP_CTRL_NOT_CONFIGURED, "Error - NLM does not run on generators.");
    return;
  }

  if(nsa_log_mgr_pid > 0)
  {
    if (kill(nsa_log_mgr_pid, SIGTERM) == -1)
    {
      //send_reply(NS_COMP_CTRL_STOP_ERROR, "Error - Unable to stop NLM. Kill failed with error %s.", strerror(errno));
      send_reply(NS_COMP_CTRL_STOP_ERROR, "Error - Unable to stop NLM. Kill failed.");
    }
    else
    {
      // Setting nsa_log_mgr_pid to -2 as we should not restart nsa_log_mgr
      nsa_log_mgr_pid = -2;
      send_reply(NS_COMP_CTRL_SUCCESS, "Success");
    }
  }
  else
    send_reply(NS_COMP_CTRL_ALREADY_STOPPED, "Error - NLM is already stopped");
  return;
}

// This will perform given operation(START/STOP) on given component(log manager, logging writer) 
void process_comp_ctrl_msg(Msg_com_con *mccptr, parent_msg *req_msg){

  NSDL2_PARENT(NULL, NULL, "Method called");

  switch(req_msg->top.internal.component){

  case COMP_NLM:
    if(req_msg->top.internal.operation == COMP_START){
      start_NLM(mccptr, req_msg);
    }else if(req_msg->top.internal.operation == COMP_STOP){
      stop_NLM(mccptr, req_msg); 
    }
    else{
      send_reply(NS_COMP_CTRL_INVALID_OPR, "Error - Invalid operation found");   //Invalid operation
    }
    break; 

  case COMP_NLW:
    if(req_msg->top.internal.operation == COMP_START){
       start_NLW(mccptr, req_msg);
    }else if(req_msg->top.internal.operation == COMP_STOP){
       stop_NLW(mccptr, req_msg);
    }
    break; 

   default:
     send_reply(NS_COMP_CTRL_INVALID_COMP, "Error - Invalid component found");
     break;
  }
}
