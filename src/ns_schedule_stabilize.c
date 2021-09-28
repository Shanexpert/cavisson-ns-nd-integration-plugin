/************************************************************************************************************
 *  Name            : ns_schedule_stabilize.c 
 *  Purpose         : To control Netstorm Stabilize Phase
 *  Initial Version : Monday, July 06 2009
 *  Modification    : -
 ***********************************************************************************************************/

#include <regex.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"
#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "util.h"
#include "timing.h"
#include "tmr.h"
#include "ns_log.h"
#include "ns_schedule_phases.h"
#include "ns_schedule_stabilize.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "ns_percentile.h"

// this method sends the the message to parent if group phase ended
static void start_stabilize_callback(ClientData cd, u_ns_ts_t now)
{
  Schedule *schedule_ptr = (Schedule *)cd.p;

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phase Index = %d, Phase Type = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type);

  send_phase_complete(schedule_ptr);   
}

static void start_stabilize_timer(Schedule *schedule_ptr, int time_val, u_ns_ts_t now)
{
  ClientData cd;
  cd.p = schedule_ptr;  

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phase Index = %d, Phase Type = %d", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type);
  
  //ab_timers[AB_TIMEOUT_END].timeout_val = time_val;
  schedule_ptr->phase_end_tmr->actual_timeout = time_val;
  //dis_timer_add( AB_TIMEOUT_END, schedule_ptr->phase_end_tmr, now, start_stabilize_callback, cd, 0);
  dis_timer_think_add( AB_TIMEOUT_END, schedule_ptr->phase_end_tmr, now, start_stabilize_callback, cd, 0);
}

//this method strt Group immediately or with a timer
void start_stabilize_phase(Schedule *schedule_ptr, u_ns_ts_t now) 
{
  int time_val = schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_cmd.stabilize_phase.time;  // ms 
  ClientData cd;
  cd.p = schedule_ptr;  

  NSDL2_SCHEDULE(NULL, NULL, "Method called. Group Index = %d, Phase Index = %d, Phase Type = %d, time_val = %d (ms)", schedule_ptr->group_idx, schedule_ptr->phase_idx, schedule_ptr->phase_array[schedule_ptr->phase_idx].phase_type, time_val);
  
   if(time_val <= 0)
      start_stabilize_callback(cd, now);     
   else
      start_stabilize_timer(schedule_ptr, time_val, now);
}

/****************************************************END OF FILE********************************************/
