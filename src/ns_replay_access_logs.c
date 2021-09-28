#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <regex.h>

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
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "ns_log.h"
#include "divide_users.h"
#include "divide_values.h"
#include "child_init.h"
#include "ns_debug_trace.h"
#include "ns_event_log.h"
#include "ns_event_id.h"
#include "ns_schedule_ramp_down_fcu.h"
#include "ns_schedule_phases_parse.h"
#include "ns_sock_com.h"
#include "ns_replay_access_logs_parse.h"
#include "ns_replay_access_logs.h"
#include "ns_string.h"
#include "ns_alloc.h"
#include "ns_url_req.h"

#include "ns_vuser_trace.h"
#include "ns_page_dump.h"
#include "ns_user_profile.h"
#include "ns_exit.h"
#include "ns_trace_level.h"
#include "nslb_cav_conf.h"

extern struct iovec *vector;
extern int *free_array;

extern int g_replay_iteration_count;
// To set next replay page in scenario using replay access logs.
int set_next_replay_page(VUser *vptr)
{
  int next_page;

  NSDL2_REPLAY(vptr, NULL, "Method Called");

  next_page = ns_get_replay_page_ext(vptr); 
  ns_set_int_val("cav_replay_page_id", next_page); 
  if(next_page == -1)
  {  
    ns_set_int_val("cav_replay_more_pages", 0); // Set to 0 to indicate all pages done
  }
  else
  {
    ns_set_int_val("cav_replay_more_pages", 1);
  }
  return 0; // Not used
}

// Get next page and set think time for currently executed page
// When next page is first page, think time will be 0.
// When next page is -1, think time will be 0
// Otherwise it will be think of page to be executed
// Note - cur_req is not increamted here. Only next_req is increamented here
// Next request was added in 3.9.1 to handle case where cur_req does not increasment e.g. when connection fails or SSL handshake fails as
// cur_req is increamened in make replay req which is not called in these cases
//

int ns_get_replay_page_ext(VUser *vptr)
{
  ReplayUserInfo *ruiptr;
  int page_id = -1;
  int page_response_time;
  u_ns_ts_t now = get_ms_stamp();

  NSDL1_REPLAY(vptr, NULL, "Method Called. total_replay_user_entries = %d, vptr->replay_user_idx = %d,  now = %u", total_replay_user_entries, vptr->replay_user_idx, now);

  // Check if data is valid or not - should never happen
  if((vptr->replay_user_idx < 0) || (vptr->replay_user_idx >= total_replay_user_entries))
  {
    //error_log("Invalid User index\n");
    NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_CRITICAL,
                                     __FILE__, (char*)__FUNCTION__,
                                    "Error: Invalid User Index");
    NSDL2_REPLAY(vptr, NULL, "Error: Invalid User Index");
    return -1;
  } 

  ruiptr = &g_replay_user_info[vptr->replay_user_idx];
 
  // Set it to 0
  vptr->pg_think_time = 0;

  // In most case, at this pont cur_req and next req will be same as cur req gets increamented afte request is made
  NSDL3_REPLAY(vptr, NULL, "cur_req = %d, next_req = %d, num_req = %d", ruiptr->cur_req, ruiptr->next_req, ruiptr->num_req);

  // Check if this is first page
  if(ruiptr->next_req == 0)
  {
    page_id = g_replay_req[ruiptr->start_index].page_id;
    ruiptr->cur_req = ruiptr->next_req; // Must set as make req use it for making request
    ruiptr->next_req++;  // Increament to set to next request
    NSDL1_REPLAY(vptr, NULL, "First Page: %d, vptr->replay_user_idx = %d", page_id, vptr->replay_user_idx);
    return(page_id);
  }
  else if(ruiptr->next_req < ruiptr->num_req)
  {
    page_id = g_replay_req[ruiptr->start_index + ruiptr->next_req].page_id;
    // Next page : Set Page think Time 
    /**
    if(ruiptr->cur_req == (ruiptr->num_req -1)) // Last page, so no think time
    {
      NSDL2_REPLAY(vptr, NULL, "Last Page: %d, No Page think time (ms) = %d", page_id, vptr->pg_think_time);
    }
    else
**/
    {
      page_response_time = now - vptr->pg_begin_at;
      NSDL2_REPLAY(vptr, NULL, "now = %u, vptr->pg_begin_at = %u, page_response_time = %d",
                              now, vptr->pg_begin_at, page_response_time);

      vptr->pg_think_time = g_replay_req[ruiptr->start_index + ruiptr->next_req].start_time - g_replay_req[ruiptr->start_index + ruiptr->next_req - 1].start_time - page_response_time;

      if(vptr->pg_think_time < 0)
      {
        NSDL2_REPLAY(vptr, NULL, "Next Page: %d, Page think time is -ve = %d. Changing to 0", page_id, vptr->pg_think_time);
        vptr->pg_think_time = 0;
      } else {
        NSDL2_REPLAY(vptr, NULL, "Applying inter page time factor by %d", global_settings->inter_page_time_factor);
        vptr->pg_think_time =   vptr->pg_think_time * global_settings->inter_page_time_factor;
      }

      NSDL2_REPLAY(vptr, NULL, "Next Page: %d, Page think time (ms) = %d", page_id, vptr->pg_think_time);
    }
    ruiptr->cur_req = ruiptr->next_req; // Must set as make req use it for making request
    ruiptr->next_req++; 

    // setting page think time to 0 on Ctrl + C
    //if(sigterm_received == 1 && gRunPhase == NS_RUN_PHASE_RAMP_DOWN && global_settings->rampdown_method.option > 0)
    if(sigterm_received == 1 && runprof_table_shr_mem[vptr->group_num].gset.rampdown_method.option > 0)
    {
      NSDL2_REPLAY(vptr, NULL, "Setting Page think time to 0 for page %d as rampdown_method.option = %d", page_id, runprof_table_shr_mem[vptr->group_num].gset.rampdown_method.option);
      vptr->pg_think_time = 0;
    }

    return page_id;
  }
  else // If not more pages left, return -1
  {
    NSDL2_REPLAY(vptr, NULL, "No more page left");
    return -1;
  }
}

// Return number of users and set iid with the timer for next user generation
//
// Input
// Return
//   num_users - 0 or > 0
//   iid - set to next timer for user generation in ms or -1 is no more user in the next round
//
int get_num_usrs_to_replay(int *iid, int *start_ramp_down)
{
  ReplayUserInfo *ruiptr;
  u_ns_ts_t first_time_stamp = 0;
  u_ns_ts_t next_time_stamp  = 0;
  int num_users = 0;

  NSDL1_REPLAY(NULL, NULL, "Method Called, g_cur_usr_index = %d, total_replay_user_entries = %d", g_cur_usr_index, total_replay_user_entries);

  // Check if at least one user is left or not
  if(g_cur_usr_index >= total_replay_user_entries)
  {
    NSDL1_REPLAY(NULL, NULL, "No more users left");
    *start_ramp_down = 1; 
    *iid = -1;
    return num_users;
  }

  // Find the start time of the first user to be started in this round
  ruiptr = &g_replay_user_info[g_cur_usr_index];
  NSDL3_REPLAY(NULL, NULL, "ruiptr->start_index = %d", ruiptr->start_index);
  first_time_stamp = (u_ns_ts_t)((double)g_replay_req[ruiptr->start_index].start_time * global_settings->arrival_time_factor);
  NSDL1_REPLAY(NULL, NULL, "first_time_stamp = %u", first_time_stamp);

  num_users = 1;

  // Since our timer granularity is <= 10 ms, we need to start all users
  // whose start time is within 10 msec of the first user to be started in this round
  for(;;)
  {
    NSDL4_REPLAY(NULL, NULL, "g_cur_usr_index = %d, total_replay_user_entries = %d, num_users = %d, global_settings->arrival_time_factor = %lf", g_cur_usr_index, total_replay_user_entries, num_users, global_settings->arrival_time_factor);
    // Check if more users are there
    if(g_cur_usr_index >= total_replay_user_entries)
    {
      *start_ramp_down = 1; 
      break;
    }
    else
    {
       if((g_cur_usr_index + num_users)< total_replay_user_entries)     //next user exists
       {
         ruiptr = &g_replay_user_info[g_cur_usr_index + num_users];
         NSDL3_REPLAY(NULL, NULL, "ruiptr->start_index = %d", ruiptr->start_index);
         next_time_stamp = (u_ns_ts_t)((double)g_replay_req[ruiptr->start_index].start_time * global_settings->arrival_time_factor);

         //Get all user which are with in range of 10 ms time stamp for cur user
         if((first_time_stamp + 10) >= next_time_stamp)
         {
           NSDL4_REPLAY(NULL, NULL, "first_time_stamp = %u, next_time_stamp = %u", first_time_stamp, next_time_stamp);
           num_users++;   //next user in with range to run (diff is 10 ms)
         }
         else
           break;
       }
       else  //no next user run only this one
           break;
    }
  }

  NSDL2_REPLAY(NULL, NULL, "Num users = %d", num_users);
  // Check is any user is there for next round
  if(g_cur_usr_index + num_users >= total_replay_user_entries)
    *iid = -1;
  else
  {
    ruiptr = &g_replay_user_info[g_cur_usr_index + num_users];
    // NSDL3_REPLAY(NULL, NULL, "ruiptr->start_index = %u", ruiptr->start_index);
    // next_time_stamp = g_replay_req[ruiptr->start_index].start_time; 

    *iid = next_time_stamp - first_time_stamp;
  }
  
  if(*iid < 0)
    *start_ramp_down = 1; 
    
  NSDL1_REPLAY(NULL, NULL, "Next replay user generation iid = %u", *iid);

  return (num_users);
}

void set_user_replay_user_idx(VUser *vptr)
{
  if(!(global_settings->replay_mode))
    return;

  NSDL1_REPLAY(vptr, NULL, "Method Called, g_cur_usr_index = %d", g_cur_usr_index);
  vptr->replay_user_idx = g_cur_usr_index++;
}

void write_to_last_file()
{
  FILE *last_file_fp;
  char last_file_name[MAX_LINE_LENGTH]="\0";

  NSDL2_REPLAY(NULL, NULL, "Method called.");

  sprintf(last_file_name, "%s/ns_index_%d.last", replay_file_dir, my_port_index);
  last_file_fp = fopen(last_file_name, "w");

  if(!last_file_fp)
  {
    fprintf(stderr, "Unable to create %s.\n", last_file_name);
    return;
  }

  fprintf(last_file_fp, "#Format: line number,page index\n#Test Run: %d\n", testidx);
  if(g_cur_usr_index != total_replay_user_entries)
  {
    fprintf(last_file_fp, "%u,%s,%llu,%d\n", 
                           g_replay_user_info[g_cur_usr_index].line_num,
                           g_replay_user_info[g_cur_usr_index].user_id,
                           g_replay_user_info[g_cur_usr_index].users_timestamp,
                           0); // Page index
  }
}

// Used to support iteration
static int g_replay_current_iteration = 0;
static int g_replay_ramp_down = 0;

static void replay_generate_users( ClientData client_data, u_ns_ts_t now)
{
  int num_users;
  int all_replay_user_started = 0;
  int time_val = 0;
  Schedule *schedule_ptr = (Schedule *)client_data.p;
  int i;

  NSDL1_REPLAY(NULL, NULL, "Method Called. g_replay_current_iteration = %d, g_replay_ramp_down = %d", g_replay_current_iteration, g_replay_ramp_down);
 
  // Do not do anything if sigterm_received 
  if(sigterm_received == 1)
  {
    NSDL1_REPLAY(NULL, NULL, "Returning as Sigterm received.");
    return;
  }

  if(g_cur_usr_index == 0 && g_replay_current_iteration == 0){
    //fprintf(stderr, "NVM %d -Starting %d's Iteration \n", my_port_index, g_replay_current_iteration);
    fprintf(stderr, "NVM %d -Starting Iteration Count:%d\n", my_port_index, g_replay_current_iteration + 1);
  }

  // For multiple iteration we need to check if all users have completed their current sessions. Wait until all the uesrs
  // have completed their sessions  
  if(g_replay_ramp_down == 1){
    if((gNumVuserActive + gNumVuserThinking + gNumVuserWaiting) != 0){
      NSDL4_REPLAY(NULL, NULL, "Current iteration %d's all users has not completed there sessions. Waiting them to complete their sessions");
    } else{ // If all the sessions off previous iteration are completed, reset g_cur_usr_index, cur_req and next_req for all the users 
      g_cur_usr_index = 0;
      for(i = 0; i < total_replay_user_entries; i++) {
        fprintf(stderr, "NVM %d - Ending Iteration Count:%d\n", my_port_index, g_replay_current_iteration);
        NSDL4_REPLAY(NULL, NULL, "NVM %d -Starting %d's Iteration", my_port_index, g_replay_current_iteration + 1);
        fprintf(stderr, "NVM %d -Starting Iteration Count:%d\n", my_port_index, g_replay_current_iteration + 1);
        g_replay_user_info[i].cur_req = 0;
        g_replay_user_info[i].next_req = 0;
        g_replay_ramp_down = 0; // Reset it so that it cab go to get_num_usrs_to_replay
      } 
    }    
  }
 
  // This condition is added for iteration. in case, no user is left in current iteraion, get_num_usrs_to_replay will not be called 
  //if(g_replay_ramp_down == 0 && (gNumVuserActive + gNumVuserThinking + gNumVuserWaiting) == 0){
  if(g_replay_ramp_down == 0 ){
    num_users = get_num_usrs_to_replay(&time_val, &all_replay_user_started);
    if(all_replay_user_started == 1){
      NSDL4_REPLAY(NULL, NULL, "All users of iteration %d started", g_replay_current_iteration);
      g_replay_current_iteration++; // Increment iteration
      g_replay_ramp_down = 1 ; // Set global rampdown
    } /*else{
      NSDL4_REPLAY(NULL, NULL, "Users left in iteration %d started", g_replay_current_iteration);
      g_replay_ramp_down = 0; // Reset global rampdown
    }*/
 
  
#if NS_DEBUG_ON
    if(num_users > 1)
      NSDL4_REPLAY(NULL, NULL, "#User=%d,From#line=%u,uid=%s,To#line=%u,uid=%s,time_val=%d",
                                                    num_users, 
                                                    g_replay_user_info[g_cur_usr_index].line_num,
                                                    g_replay_user_info[g_cur_usr_index].user_id,
                                                    g_replay_user_info[g_cur_usr_index + num_users - 1].line_num,
                                                    g_replay_user_info[g_cur_usr_index + num_users - 1].user_id, time_val);
#endif

    NSDL1_REPLAY(NULL, NULL, "num_users = %d, time_val = %d, all_replay_user_started = %d, sigterm_received = %d", num_users, time_val, all_replay_user_started, sigterm_received);

    // Only one group (SGRP) supported for Replay Access Logs scenario type. 
    // So we have passed hard coded group number(0) in new user()
    if(num_users > 0)
      new_user(num_users, now, 0, 1, NULL, NULL);
  }

  // If no user is left for the current iteration and there is more iteration to replay, then time_val to 5 seconds so that it can go to next 
  // iteration 
  if( (time_val <= 0) && (g_replay_current_iteration < g_replay_iteration_count)){
    NSDL1_REPLAY(NULL, NULL, "Setting time_val, so that it can go to next iteration");
    time_val = 5000;
  }
 
  // Finish will be called in case all the iteration is completed
  if(g_replay_current_iteration == g_replay_iteration_count) 
  {
    finish(now);
    NSDL1_REPLAY(NULL, NULL, "NVM %d - Last %d users started.", my_port_index, num_users);
    fprintf(stderr, "ReplayAccessLog: NVM %d - Last %d users started.\n", my_port_index, num_users);
  } 
   

  if(sigterm_received == 0)
  {
    if(time_val >= 0)
    {
      //ab_timers[AB_TIMEOUT_RAMP].timeout_val = time_val;
      schedule_ptr->phase_end_tmr->actual_timeout = time_val;
      dis_timer_think_add( AB_TIMEOUT_END, schedule_ptr->phase_end_tmr, now, replay_generate_users, client_data, 0);
    }
  }
}

void replay_mode_user_generation(Schedule *schedule_ptr)
{
  u_ns_ts_t now = get_ms_stamp();
  ReplayUserInfo *ruiptr;
  ClientData client_data; 
  u_ns_ts_t iid, first_time_stamp;
  client_data.p = schedule_ptr;

  NSDL1_REPLAY(NULL, NULL, "Method Called, now = %u", now);

  // If index file has header only, no of user will be zero for the nvm
  // In this case NVM will not exit as other NVM's may have requets, it will send finish report and never retrun back
  if(total_replay_user_entries == 0)
  {
    NSDL1_REPLAY(NULL, NULL, "No User found in NVM %d. Sending finish report", now, my_port_index);
    finish(now);
    fprintf(stderr, "It Should never retun here\n"); // NVM will never return here as user is 0 
    return;
  }

  ruiptr = &g_replay_user_info[0];
  // Multiply by arrival_time_factor to apply arrival factor on firts user of nvm
  first_time_stamp = g_replay_req[ruiptr->start_index].start_time * global_settings->arrival_time_factor;
 
  //iid = first_time_stamp - now;
  iid = first_time_stamp;

  if(iid >  0)
  {
    NSDL3_REPLAY(NULL, NULL, "ReplayAccessLog: NVM %d - First user(%s) will start after %5.2f seconds, iid = %u", my_port_index, ruiptr->user_id, iid/1000.0, iid);
    fprintf(stderr, "ReplayAccessLog: NVM %d - First user(%s) will start after %5.2f seconds.\n", my_port_index, ruiptr->user_id, iid/1000.0);
    //ab_timers[AB_TIMEOUT_RAMP].timeout_val = iid;
    schedule_ptr->phase_end_tmr->actual_timeout = iid;
    dis_timer_think_add( AB_TIMEOUT_RAMP, schedule_ptr->phase_end_tmr, now, replay_generate_users, client_data, 0);
  }
  else
  {
    NSDL3_REPLAY(NULL, NULL, "ReplayAccessLog: NVM %d - Starting first(%s) user, iid = %u", my_port_index, ruiptr->user_id, iid);
    fprintf(stderr, "ReplayAccessLog: NVM %d - Starting first(%s) user.\n", my_port_index, ruiptr->user_id);
    replay_generate_users(client_data, now);
  }
}


// For sending request

#define MAX_READ_SIZE 1024*1024

// This macro is defined to set remaining byte with to send left byte 
// And then it checks if coming for first time after write
//
// TBD - check if we need to to modselect
//   Also we need to set CNST_SSL_WRITING for SSL
//

#define REPLAY_HANDLE_PARTIAL_SEND {\
  cptr->bytes_left_to_send = to_send; \
  NSDL3_REPLAY(NULL, NULL, "Partial write happened. Returning back. Bytes_remaining = %d, offset = %d and to_send = %d", cptr->bytes_left_to_send, cptr->body_offset, to_send); \
    if (cptr->url_num->request_type == HTTPS_REQUEST) \
      cptr->conn_state = CNST_SSL_WRITING; \
    else \
      cptr->conn_state = CNST_WRITING; \
  } \
  return;

// This macro will call if there was error in sending response
// And error in reading from file or sending on socket connection
//
//
// TBD - how to handle retry
/*#define REPLAY_HANDLE_ERROR \
  close_replay_req_file(cptr, now); \
  retry_connection(cptr, now, NS_REQUEST_WRITE_FAIL); \
  return;
*/

#define REPLAY_HANDLE_ERROR(req_status) \
  retry_connection(cptr, now, req_status); \
  return;

/*
static inline int open_replay_req_file(connection *cptr, u_ns_ts_t now)
{
  ReplayUserInfo *ruiptr;
  char req_file_name[2048];
  int req_file_fd = -1;
  VUser *vptr;
  struct stat buf;


  vptr = cptr->vptr;
  ruiptr = &g_replay_user_info[vptr->replay_user_idx];

  NSDL3_REPLAY(NULL, NULL, "Method called, vptr->replay_user_idx = %d, ruiptr->cur_req = %d", vptr->replay_user_idx, ruiptr->cur_req);

  // Check if this is first page
  // Remember ruiptr->num_req is already incremented in ns_get_replay_page_ext (which is called through API)
  if((ruiptr->cur_req < 0) || (ruiptr->cur_req >= ruiptr->num_req))
  {
    NSDL3_REPLAY(NULL, NULL, "Invalid current page request for replay: vptr->replay_user_idx = %d, ruiptr->cur_req = %d, ruiptr->num_req = %d, req_file_name = %s", vptr->replay_user_idx, ruiptr->cur_req, ruiptr->num_req, req_file_name);
    fprintf(stderr, "Error: Invalid current page request for replay\n");
    return -1;
  }

  //cur_req is incremented already so getting current index by decrementing cur_req"
  sprintf(req_file_name, "%s/%u", replay_file_dir, g_replay_req[ruiptr->start_index + ruiptr->cur_req].file_fd.req_file_name);

  NSDL3_REPLAY(NULL, NULL, "req_file_name = %s", req_file_name);
  if((req_file_fd = open(req_file_name, O_RDONLY)) < 0)
  {
    fprintf(stderr, "Error: Error in opening replay request file = %s\n", req_file_name);
    return -1;
  }

  if(fstat(req_file_fd , &buf) < 0)
  {
    fprintf(stderr, "fstat failed for %s\n", req_file_name);
    return -1;
  }

  cptr->bytes_left_to_send = buf.st_size; //if file size is 0 Handle later

  g_replay_req[ruiptr->start_index + ruiptr->cur_req].file_fd.req_fd = req_file_fd;

  NSDL2_REPLAY(NULL, NULL, "User id (%s), size of file '%s' is %d, fd = %d", ruiptr->user_id, req_file_name, cptr->bytes_left_to_send, req_file_fd);
 
  return 0;
}
*/
/*
static inline int get_replay_fd(connection *cptr)
{
  VUser *vptr;
  ReplayUserInfo *ruiptr;
  vptr = cptr->vptr;

  NSDL2_REPLAY(NULL, NULL, "Method called");

  ruiptr = &g_replay_user_info[vptr->replay_user_idx];

  return(g_replay_req[ruiptr->start_index + ruiptr->cur_req].file_fd.req_fd);

}
*/

/*
static inline void close_replay_req_file(connection *cptr, u_ns_ts_t now)
{
  VUser *vptr;
  ReplayUserInfo *ruiptr;
  vptr = cptr->vptr;

  ruiptr = &g_replay_user_info[vptr->replay_user_idx];
  NSDL2_REPLAY(NULL, NULL, "Method called, ruiptr->cur_req = %d", ruiptr->cur_req);
  close(get_replay_fd(cptr));

  //cur_req increment here earliar we are doing inside API calling
  ruiptr->cur_req++;
  NSDL2_REPLAY(NULL, NULL, "After Incrementing, ruiptr->cur_req = %d", ruiptr->cur_req);
}
*/

#define TIME_FMT 32
#define TIMESTAMP_SIZE 29
/* This fucntion calculates the current timestamp in format YYYY-MM-DDTHH:MM:SS.msTimezone i.e. 2015-12-21T07:55:03.137-08:00 
   Here timezone value is difference in time w.r.t to GMT */
static inline void replay_req_parameterization(connection *cptr, int cr_offset)
{
  char time[TIME_FMT];
  struct tm *tm;
  struct timespec ts;
  int offset, h_offset, m_offset;

  NSDL2_REPLAY(NULL, cptr, "Method called");

  clock_gettime(CLOCK_REALTIME, &ts); 
  if((tm = localtime(&ts.tv_sec)) != NULL)
  {
    offset = (int) tm->tm_gmtoff;
    h_offset = offset / 3600;
    m_offset = (abs(offset) % 3600)/60;
    sprintf(time, "%04d-%02d-%02dT%02d:%02d:%02d.%03ld%+03d:%02d",
           tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
           tm->tm_hour, tm->tm_min, tm->tm_sec,ts.tv_nsec/1000000,
           h_offset, m_offset);
    // Copy 29 bytes only as timestamp has a fixed size value. Using strncpy and not strcpy/snprintf as they copy null as the last character 
    strncpy(cptr->free_array + cr_offset, time, TIMESTAMP_SIZE); 
  }
}

/***************************************************************************************************************************************
  Send replay request in case of web service RAL. This method is called to handle write(complete and partial). This method writes data 
  using cptr->free_array in case of https and http with parameterization. In case of http without parameterization, sendfile is used
  This method uses following variable of cptr
     body_offset: To keep track of offset in file which is not yet send in case of http without parameterization. 
                  In other cases it is used to track how much data is written on socket from the cptr->free_array
     bytes_left_to_send: Bytes yet to be send
     req_file fd . add new var in connection struct
     free_array: contains the request to be send 
     free_array_size: contains the size of cptr->free_array
     fd: socket fd  
**************************************************************************************************************************************/
static void send_replay_web_service_log_req(connection *cptr, u_ns_ts_t now)
{
  VUser *vptr = cptr->vptr;
  ReplayUserInfo *ruiptr;
  int ssl_errno;
  int bytes = 0;
  short cr_type;
  int req_file_fd = -1;
  int to_send;
  unsigned long err_no; 
  char *err_str_ptr = NULL; 

  NSDL2_REPLAY(vptr, cptr, "Method called");

  ruiptr = &g_replay_user_info[vptr->replay_user_idx];
  req_file_fd = ruiptr->req_file_fd;
  to_send = cptr->bytes_left_to_send;
  cr_type = g_replay_req[ruiptr->start_index + ruiptr->cur_req].type;

  if(cptr->url_num->request_type == HTTPS_REQUEST) { 
    // HTTPS with and without parameterization
    NSDL2_REPLAY(vptr, cptr, "Replay HTTPS request, to_send = %d", to_send);
    // When an SSL_write() operation has to be repeated because of SSL_ERROR_WANT_READ or SSL_ERROR_WANT_WRITE, 
    // it must be repeated with the same arguments. Hence use same buffer in case of partial handling
    ERR_clear_error();
    bytes = SSL_write(cptr->ssl, cptr->free_array + cptr->body_offset, to_send);
    if (bytes < to_send) 
    {
      switch (ssl_errno = SSL_get_error(cptr->ssl, bytes)) 
      {
        case SSL_ERROR_NONE:     
          cptr->body_offset += bytes;
          to_send -= bytes;
          REPLAY_HANDLE_PARTIAL_SEND  // This will return from here
        case SSL_ERROR_WANT_WRITE:
          REPLAY_HANDLE_PARTIAL_SEND  // This will return from here
        case SSL_ERROR_WANT_READ:
          NSDL2_REPLAY(vptr, cptr, "SSL_write error: SSL_ERROR_WANT_READ");
          break;
        case SSL_ERROR_ZERO_RETURN:
          NSDL2_REPLAY(vptr, cptr,  "SSL_write error: aborted");
          break;
        case SSL_ERROR_SSL:
          err_no = ERR_get_error();
          err_str_ptr = ERR_error_string(err_no, NULL); 
          NSTL2(vptr, cptr, "Error in sending request. SSL_write: err = %s", err_str_ptr);
          break;
        default:  //some other problem
          NSDL2_REPLAY(vptr, cptr, "Error in sending request. SSL_write: err = %d", ssl_errno);
      }
      REPLAY_HANDLE_ERROR(NS_REQUEST_SSLWRITE_FAIL)
    }
  } else {  
    // HTTP with and without parameterization
    if(cr_type) {
      // Using write here as NS is sending parameterized buffer in this case
      bytes = write(cptr->conn_fd, cptr->free_array + cptr->body_offset, to_send); 
    } else {
      // Using sendfile here as NS is sending same request as in file. Because this copying is done within the kernel, sendfile is more
      // efficient than the combination of read and write, which would require transferring data to and from user space.
      bytes = sendfile(cptr->conn_fd, req_file_fd, (off_t*)&(cptr->body_offset), to_send);
    }
     
    if (bytes < 0) {
      if (errno == EAGAIN) {
        REPLAY_HANDLE_PARTIAL_SEND
      } else { //some other problem
        NSTL1(vptr, cptr, "Error in sending request : Error = %s", nslb_strerror(errno));
        REPLAY_HANDLE_ERROR(NS_REQUEST_WRITE_FAIL)
      }
    } else if (bytes == 0) { // This can come if data file is not correct. req_size is more than the file size
      if(cr_type) {
        //Connection Closed 
        NSDL2_REPLAY(vptr, cptr, "Connection closed from server side");
        REPLAY_HANDLE_ERROR(NS_REQUEST_WRITE_FAIL)
      }
      else {
        REPLAY_HANDLE_PARTIAL_SEND
      } 
    } else if (bytes < to_send) {
      NSDL2_REPLAY(vptr, cptr, "Partial Request written, bytes written = %d", bytes);

      //Increase the offset in case writing from buffer as sendfile will increase the offset itself.
      if(cr_type)
        cptr->body_offset += bytes;

      to_send -= bytes;
      REPLAY_HANDLE_PARTIAL_SEND  // This will return from here
    } else {
      NSDL2_REPLAY(vptr, cptr, "HTTP write complete, bytes written = %d", bytes);
    }
  }
  NSDL2_REPLAY(vptr, cptr, "Processing replay request after SSL_write/sendfile. offset = %d, to_send = %d, bytes send = %d", cptr->body_offset, to_send, bytes);

  if (cptr->conn_state == CNST_REUSE_CON)
    cptr->req_code_filled = -2;

  cptr->conn_state = CNST_HEADERS;

  cptr->tcp_bytes_sent = bytes;
  average_time->tx_bytes += bytes;
  // Calling in case complete write is done
  on_request_write_done (cptr);

  return;
}


static int read_request_file_to_log_req(char *buf, int max_read_size, ReplayUserInfo *ruiptr)
{
   int bytes = 0;
   int req_file_fd     = ruiptr->req_file_fd;
   unsigned int req_file_offset = g_replay_req[ruiptr->start_index + ruiptr->cur_req].offset;
   unsigned int req_file_size   = g_replay_req[ruiptr->start_index + ruiptr->cur_req].size;

   if (lseek(req_file_fd, req_file_offset, SEEK_SET) == -1)
   {
      fprintf(stderr, "Seek failed. offset = %u, err = %s\n", req_file_offset, nslb_strerror(errno));
      return -1;
   }
   else
   {
     while(1)
     {
       if ((bytes = read(req_file_fd, buf, req_file_size > max_read_size ? max_read_size: req_file_size)) <=0 )
       {
          if(bytes == 0)
          {
            NSDL2_REPLAY(NULL, NULL, "Reading of static file interrupted, continuing");
            continue;
          }
          fprintf(stderr, "Error in reading static file. bytes = %d, err=%s\n", bytes, nslb_strerror(errno));
          return -1;
       }
       break;
     }
   }

   return bytes;
}

#ifdef NS_DEBUG_ON
static void debug_log_replay_req(connection *cptr, ReplayUserInfo *ruiptr)
{
  VUser* vptr = cptr->vptr;
  char log_file[1024]="\0";

  if ((runprof_table_shr_mem[vptr->group_num].gset.debug & DM_LOGIC4) && 
      (runprof_table_shr_mem[vptr->group_num].gset.module_mask & MM_REPLAY))
  {
    int log_fd;
    char line_break[] = "\n------------------------------------------------------------\n";

    // Log file name format is url_req_<user_id>_<page_id>.dat
    // url_id is not yet implemented (always 0)
    sprintf(log_file, "%s/logs/TR%d/%lld/ns_logs/req_rep/url_req_%s_%d.dat", g_ns_wdir, testidx, vptr->partition_idx, ruiptr->user_id, g_replay_req[ruiptr->start_index + ruiptr->cur_req].page_id);

    // Do not change the debug trace message as it is parsed by GUI
    NS_DT4(vptr, cptr, DM_L1, MM_REPLAY, "Request is in file '%s'", log_file);

    if((log_fd = open(log_file, O_CREAT|O_WRONLY|O_APPEND|O_CLOEXEC, 00666)) < 0)
      fprintf(stderr, "Error: Error in opening file for logging URL request\n");
    else
    {
      char buf[MAX_READ_SIZE];
      int bytes_read = 0;
      if((bytes_read = read_request_file_to_log_req(buf, MAX_READ_SIZE, ruiptr)) > 0)
      {
        write(log_fd, buf, bytes_read);
        write(log_fd, line_break, strlen(line_break));
        close(log_fd);
      }
    }
  }
}
#endif /* NS_DEBUG_ON */

#ifdef NS_DEBUG_ON
static void debug_log_replay_req_buffer(connection *cptr, ReplayUserInfo *ruiptr, char *req_buffer, int req_size)
{
  VUser* vptr = cptr->vptr;
  char log_file[1024]="\0";

  if ((runprof_table_shr_mem[vptr->group_num].gset.debug & DM_LOGIC4) && 
      (runprof_table_shr_mem[vptr->group_num].gset.module_mask & MM_REPLAY))
  {
    int log_fd;
    char line_break[] = "\n------------------------------------------------------------\n";

    // Log file name format is url_req_<user_id>_<page_id>.dat
    // url_id is not yet implemented (always 0)
    sprintf(log_file, "%s/logs/TR%d/%lld/ns_logs/req_rep/url_req_%s_%d.dat", g_ns_wdir, testidx, vptr->partition_idx, ruiptr->user_id, g_replay_req[ruiptr->start_index + ruiptr->cur_req].page_id);

    // Do not change the debug trace message as it is parsed by GUI
    NS_DT4(vptr, cptr, DM_L1, MM_REPLAY, "Request is in file '%s'", log_file);

    if((log_fd = open(log_file, O_CREAT|O_WRONLY|O_APPEND|O_CLOEXEC, 00666)) < 0)
      fprintf(stderr, "Error: Error in opening file for logging URL request\n");
    else
    {
      write(log_fd, req_buffer, req_size);
      write(log_fd, line_break, strlen(line_break));
      close(log_fd);
    }
  }
}
#endif /* NS_DEBUG_ON */

static inline void replay_save_ua_string(char *http_req_hdr_buf, ReplayUserInfo *ruiptr, VUser *vptr, int *ua_filled)
{

  NSDL3_REPLAY(vptr, NULL, "Method called. http_req_hdr_buf = %s", http_req_hdr_buf);

  *ua_filled = 0;
  // Extract user Agent String from the http req hdr bur from the first URL of user session 
  // and save. This is needed so that request made using NetStorm code for redirect/InLine will have this UA String
  // Currenly only MAIN URL comes here as InLine as per log is  handled by netstorm code
  if(ruiptr->cur_req == 0)
  {
    char *ua_hdr = strcasestr(http_req_hdr_buf, "user-agent"); // we need give all lower case for strcasestr
    if(ua_hdr != NULL)
    {
      ua_hdr = strstr(ua_hdr, ":");
      if(ua_hdr != NULL)
      {
        char *ua_hdr_end = strstr(ua_hdr, "\r");
        if(ua_hdr_end != NULL)
        {
          int  ua_len = ua_hdr_end - ua_hdr;
          NSDL1_REPLAY(vptr, NULL, "Found UA String = %*.*s", ua_len, ua_len, ua_hdr);
          ns_set_ua_string_ext (ua_hdr, ua_len, vptr);
          *ua_filled = 1;
        }
      }
    }
  }
}

inline int fill_replay_req_body(VUser *vptr, connection* cptr, action_request_Shr *request, char *http_req_body_buf, int http_req_body_size)
{
  int http_content_length_idx = 0;
  int ret, content_length = http_req_body_size;

  NSDL1_HTTP(cptr, vptr, "Method Called");
 
  if (request->proto.http.http_method == HTTP_METHOD_POST ||
      request->proto.http.http_method == HTTP_METHOD_PUT ||
      request->proto.http.http_method == HTTP_METHOD_PATCH ||
      (request->proto.http.http_method == HTTP_METHOD_GET &&
       request->proto.http.post_ptr) || (http_req_body_size)) { // Content-Length is always send even if post_ptr is NULL
     http_content_length_idx = g_req_rep_io_vector.cur_idx++;
  }  

  //Fill end of the header line 
  NS_FILL_IOVEC(g_req_rep_io_vector, CRLFString, CRLFString_Length); 

  if(request->proto.http.post_ptr)
  {
    NSDL3_HTTP(NULL, cptr, "Post Body Present");
    if((ret = insert_segments(vptr, cptr, request->proto.http.post_ptr, &g_req_rep_io_vector, &content_length, 
                              request->proto.http.body_encoding_flag, 1, REQ_PART_BODY, request, SEG_IS_NOT_REPEAT_BLOCK)) < 0)
      return ret;
  }
  else if(http_req_body_size){ // If body is present then fill it at last vector
    NS_FILL_IOVEC_AND_MARK_FREE(g_req_rep_io_vector, http_req_body_buf, http_req_body_size);
  }  

  if(request->proto.http.http_method == HTTP_METHOD_POST ||
      request->proto.http.http_method == HTTP_METHOD_PUT ||
      request->proto.http.http_method == HTTP_METHOD_PATCH ||
      (request->proto.http.http_method == HTTP_METHOD_GET &&
       request->proto.http.post_ptr) || (http_req_body_size))
  {
    int written_amt;
    if((written_amt = snprintf(post_content_ptr, post_content_val_size, "%d\r\n", content_length)) > post_content_val_size)
    {
      fprintf(stderr, "make_request(): Error, writing too much into content_buf array\n");
      return -1;
    }
    
    cptr->http_payload_sent = content_length;
    int cur_idx = g_req_rep_io_vector.cur_idx;
    g_req_rep_io_vector.cur_idx = http_content_length_idx;
    NS_FILL_IOVEC(g_req_rep_io_vector, content_length_buf, (written_amt + POST_CONTENT_VAR_SIZE));
    g_req_rep_io_vector.cur_idx = cur_idx;
  }
  else
    cptr->http_payload_sent = 0;   
  
  return 0;
}

// Send replay request for access log based replay
// This method is called for first time as well as to send left bytes
// This method uses following variable of cptr
//
//   body_offset: To keep track of offset in file which is not yet send
//   bytes_left_to_send: Bytes yet to be send
//   req_file fd . add new var in connection struct
//   fd: socket fd
//
// Called from ns_fd_check() and netstorm_child() like handle_write() and handle_ssl_write()

inline int
make_replay_access_log_req(connection* cptr, int *num_vectors, NSIOVector *ns_iovec, u_ns_ts_t now)
{
  int req_file_fd = -1;
  int bytes_read = 0;
  off_t offset;
  VUser *vptr;
  ReplayUserInfo *ruiptr;
  vptr = cptr->vptr;
  char *http_req_hdr_buf = NULL;
  char *http_req_body_buf = NULL;
  PerHostSvrTableEntry_Shr* svr_entry;
  char *tmp;
  char *first_space_ptr;
  char *sec_space_ptr;
  int url_len, ret;
  char *newline_ptr;

  NSDL2_REPLAY(NULL, cptr, "Method called");
  
  action_request_Shr* request = cptr->url_num;
  http_request_Shr *http_request = &(cptr->url_num->proto.http);
  int disable_headers = runprof_table_shr_mem[vptr->group_num].gset.disable_headers;
  int use_rec_host = runprof_table_shr_mem[vptr->group_num].gset.use_rec_host;

  ruiptr = &g_replay_user_info[vptr->replay_user_idx];
  req_file_fd = ruiptr->req_file_fd;

  offset = (off_t)g_replay_req[ruiptr->start_index + ruiptr->cur_req].offset; 
  int http_req_hdr_size = g_replay_req[ruiptr->start_index + ruiptr->cur_req].size; 
  int http_req_body_size = g_replay_req[ruiptr->start_index + ruiptr->cur_req].body_size;

  NSDL2_REPLAY(NULL, cptr, "offset = %ld, http_req_hdr_size = %d, req_file_fd = %d, disable_headers = %d", offset, http_req_hdr_size, req_file_fd, disable_headers);

  // Malloc buffer for http request line and headers which are in req file
  // Read in this buffer

  MY_MALLOC(http_req_hdr_buf, http_req_hdr_size, "AccessLogReplayReq", ruiptr->cur_req);
   
  // If script has body it will get priority 
  if(request->proto.http.post_ptr == NULL){
    if(http_req_body_size)
      MY_MALLOC(http_req_body_buf, http_req_body_size, "AccessLogReplayReq", ruiptr->cur_req);
  }
  
  // We can safely increament here as this is used only when request is made. Not in partial write method
 
  // Seek file up to start offset
  if (lseek(req_file_fd, offset, SEEK_SET) == -1) 
  {
    fprintf(stderr, "Seek failed. offset = %d, http_req_hdr_size = %d, err = %s", cptr->body_offset, http_req_hdr_size, nslb_strerror(errno));
    NSDL2_REPLAY(NULL, NULL, "Seek failed. offset = %u, http_req_hdr_size = %d, err = %s", cptr->body_offset, http_req_hdr_size, nslb_strerror(errno));
    return -1; // TODO
  }
  // Read req line with headers
  while(1)
  {
    if ((bytes_read = read(req_file_fd, http_req_hdr_buf, http_req_hdr_size)) != http_req_hdr_size)
    {
      if(bytes_read == 0)
      {
        NSDL2_REPLAY(NULL, NULL, "Reading of static file interrupted, continuing");
        continue;
      }
      // Error in reading file
      fprintf(stderr, "Error in reading static file. bytes = %d, err=%s", bytes_read, nslb_strerror(errno));
      NSDL2_REPLAY(NULL, NULL, "Error in reading static file. bytes = %d, err=%s", bytes_read, nslb_strerror(errno));
      return -1; // TODO
    }
    break;
  }
 
  // Extract user Agent String from the http req hdr bur from the first URL of user session 
  int ua_filled;
  replay_save_ua_string(http_req_hdr_buf, ruiptr, vptr, &ua_filled);
  
  // Read request body and post body is not given in script
  if((request->proto.http.post_ptr == NULL) && (http_req_body_size > 0)){
    while(1)
    {
      if ((bytes_read = read(req_file_fd, http_req_body_buf, http_req_body_size)) != http_req_body_size)
      {
        if(bytes_read == 0)
        {
          NSDL2_REPLAY(NULL, NULL, "Reading of static file interrupted, continuing");
          continue;
        }
        // Error in reading file
        fprintf(stderr, "Error in reading static file. bytes = %d, err=%s", bytes_read, nslb_strerror(errno));
        NSDL2_REPLAY(NULL, NULL, "Error in reading static file. bytes = %d, err=%s", bytes_read, nslb_strerror(errno));
        return -1; // TODO
      }
      break;
    }
  }
  NSDL2_REPLAY(NULL, NULL, "http_req_hdr_buf = %s, http_req_hdr_size = %d", http_req_hdr_buf, http_req_hdr_size);
  if(http_req_body_size)
    NSDL2_REPLAY(NULL, NULL, "http_req_body_buf = %s, http_req_body_size = %d", http_req_body_buf, http_req_body_size);

/**********************  correlated url section start *********************/
  // Use cpt->url, in case URL is parameterize
  if (!(http_request->url.num_entries == 1 && http_request->url.seg_start->type == STR)){

    // Step1a: Fill http method taken from request file
    char *temp;
    int len;

    first_space_ptr = strstr(http_req_hdr_buf, " ");
    len = first_space_ptr - http_req_hdr_buf;
    MY_MALLOC(temp, len + 1, "Allocate mthod for replay in case url is correlated", -1);
    strncpy(temp, http_req_hdr_buf, len);
    temp[len] = ' '; // Add space after method
    
    NS_FILL_IOVEC_AND_MARK_FREE(*ns_iovec, temp, (len + 1));

    if(cptr->url) {
      // Step1b: Fill http method
      NSDL3_REPLAY(vptr, cptr, "Filling url for parameterization case [%s] of len = %d at %d vector idx",
                                                                                cptr->url, cptr->url_len, ns_iovec->cur_idx);
      NS_FILL_IOVEC_AND_MARK_FREE(*ns_iovec, cptr->url, cptr->url_len);

      // Step1c: Fill HTTP Version
      // extract http version from http_req_hdr_buf
      first_space_ptr++; 
      sec_space_ptr = strstr(first_space_ptr, " ");
      sec_space_ptr++;
      newline_ptr = strstr(sec_space_ptr, "\n");
      len = newline_ptr - sec_space_ptr + 1;
      MY_MALLOC(temp, len + 1, "Allocate HTTP version string for replay in case url is correlated", -1);
      temp[0] = ' '; // Add space before http version
      strncpy(temp + 1, sec_space_ptr, len);
  
      NS_FILL_IOVEC_AND_MARK_FREE(*ns_iovec, temp, (len + 1));
 
      newline_ptr++; 
      
      // Step1d - add header string read from request file 
      len = newline_ptr - http_req_hdr_buf;
      len = http_req_hdr_size -len; 
      if(len){
        MY_MALLOC(temp, len + 1, "allocate header for replay", -1);
        strncpy(temp, newline_ptr, len);
    
        NS_FILL_IOVEC_AND_MARK_FREE(*ns_iovec, temp, len);
      }
/**********************  correlated url section end *********************/
    } else { // Should not come here
      NSEL_CRI(NULL, cptr, ERROR_ID, ERROR_ATTR, "Url is NULL, This should not happen");
      NSDL2_REPLAY(NULL, cptr, "Url is NULL. This should not happen");
      return -1;
    }
  } else{
    // Step1: Fill HTTP Request from replay request file
    NS_FILL_IOVEC_AND_MARK_FREE(*ns_iovec, http_req_hdr_buf, http_req_hdr_size);
  }
 
  // Set cptr->url form replay request, as it is used for authentication  
  first_space_ptr = strstr(http_req_hdr_buf, " ");
  if(first_space_ptr){
    first_space_ptr++;
    sec_space_ptr = strstr(first_space_ptr, " ");
    if(sec_space_ptr){
      NSDL2_REPLAY(NULL, NULL, "Going to reset cptr->url for replay");
      url_len = sec_space_ptr - first_space_ptr;  
      MY_MALLOC(tmp, url_len + 1, "allocate cptr->url for replay", -1);
      strncpy(tmp, first_space_ptr, url_len);
      tmp[url_len] = '\0';
      // In case url is correlated, it will be freed in free vector
      if(http_request->url.num_entries == 1 && http_request->url.seg_start->type == STR){ 
        if(cptr->flags & NS_CPTR_FLAGS_FREE_URL){
          if(cptr->url)
            FREE_AND_MAKE_NOT_NULL(cptr->url, "Free cptr->url to reset", -1);
        }
      }
      cptr->url = tmp;
      cptr->url_len = url_len; 
      cptr->flags |= NS_CPTR_FLAGS_FREE_URL; 
      NSDL2_REPLAY(NULL, NULL, "New url = %s", cptr->url);
    } else {
      NSDL2_REPLAY(NULL, NULL, "Space after Url not found in request, It should not happen");
    } 
  } else {
    NSDL2_REPLAY(NULL, NULL, "Space after method not found in request, It should not happen");
  }

  // Free it here in case of url is correlated 
  if(!(http_request->url.num_entries == 1 && http_request->url.seg_start->type == STR))
    FREE_AND_MAKE_NOT_NULL(http_req_hdr_buf, "Free http_req_hdr_buf buffer", -1);

  //We need actual host to make URL and HOST header
  svr_entry = get_svr_entry(vptr, cptr->url_num->index.svr_ptr);
 
  // Step2: Fill Cookie headers if any cookie to be send
  /* The cookie has to be filled before body since we dont know how many cookies will be filled in case of auto cookie. */
  // We are filling cookie after request line. So set cookie_start_idx to point after url segments
  // Now insert cookies as we do not know how many cookies are there for Auto Mode.
  // So we are filling cookies after request line
  if((ret = make_cookie_segments(cptr, &(request->proto.http), ns_iovec)) < 0)
    return ret;
  
  // Step6: Fill standard headers as required
    
  /* insert the Referer and copy to new one if needed */
  if (vptr->referer_size) {
    NS_FILL_IOVEC(*ns_iovec, Referer_buf, REFERER_STRING_LENGTH);

    NS_FILL_IOVEC(*ns_iovec, vptr->referer, vptr->referer_size);
  }

  //save_referer is now called from validate_req_code, this is done to implement new referer design Bug 17161 
 
  /* insert the Hostr-Agent header */
  if (!(disable_headers & NS_HOST_HEADER)) {
      NS_FILL_IOVEC(*ns_iovec, Host_header_buf, HOST_HEADER_STRING_LENGTH);

      // Added by Anuj 08/03/08
      if (use_rec_host == 0) //Send actual host (mapped)
      {
        //Manish: here we directly get server_entry because it is already set in start_socket
        NS_FILL_IOVEC(*ns_iovec, svr_entry->server_name, svr_entry->server_name_len);
      }
      else //Send recorded host
      {
        NS_FILL_IOVEC(*ns_iovec, cptr->url_num->index.svr_ptr->server_hostname, cptr->url_num->index.svr_ptr->server_hostname_len);
      }
   
      NSDL2_HTTP(NULL, cptr, "The USE_REC_HOST=%d, and server_name=%s", use_rec_host, (char *)ns_iovec->vector[ns_iovec->cur_idx].iov_base);

      NS_FILL_IOVEC(*ns_iovec, CRLFString, CRLFString_Length);
  }

  /* insert the standard headers */
  if (!(disable_headers & NS_ACCEPT_HEADER)) {
      NS_FILL_IOVEC(*ns_iovec, Accept_buf, ACCEPT_BUF_STRING_LENGTH);
  }

  if (!(disable_headers & NS_ACCEPT_ENC_HEADER)) {//globals.no_compression == 0)
      NS_FILL_IOVEC(*ns_iovec, Accept_enc_buf, ACCEPT_ENC_BUF_STRING_LENGTH);
  }

  if (!(disable_headers & NS_KA_HEADER)) {
      NS_FILL_IOVEC(*ns_iovec, keep_alive_buf, KEEP_ALIVE_BUF_STRING_LENGTH);
  }

  if (!(disable_headers & NS_CONNECTION_HEADER)) {
      NS_FILL_IOVEC(*ns_iovec, connection_buf, CONNECTION_BUF_STRING_LENGTH);
  }

  /* insert the rest of the headers */
  // request->proto.http.hdrs has end of headers line also
  // TODO - Issue -> if there are not script headers, then end of headers may not come
  if(request->proto.http.hdrs.num_entries)
  {
    NSDL2_REPLAY(NULL, cptr, "Going to add scrript headers next_idx %d", ns_iovec->cur_idx);
    if ((ret = insert_segments(vptr, cptr, &request->proto.http.hdrs, ns_iovec,
                                    NULL, 0, 1, REQ_PART_HEADERS, request, SEG_IS_NOT_REPEAT_BLOCK)) < 0) {
      return ret;
    }
  }

  /**********************************************************************/
  /*Body Should fill at last. Don't fill any header after fill_replay_req_body */
  /**********************************************************************/
  if((ret = fill_replay_req_body(vptr, cptr, request, http_req_body_buf, http_req_body_size)) < 0)
    return ret;

  NS_DT3(vptr, cptr, DM_L1, MM_SOCKETS, "Starting replay of page %s URL(%s)on fd = %d. Request line is %s",
         cptr->url_num->proto.http.type == MAIN_URL ? "main" : "inline",
         get_req_type_by_name(cptr->url_num->request_type),cptr->conn_fd,
         cptr->url);

  NSDL3_HTTP(NULL, cptr, "next_idx = %d", ns_iovec->cur_idx);

  *num_vectors = ns_iovec->cur_idx;
  NSDL3_HTTP(NULL, cptr, "num_vectors = %d", ns_iovec->cur_idx);

  return(0);
}


void make_and_send_replay_access_log_req(connection *cptr, u_ns_ts_t now)
{
  int http_size = 0;
  int num_vectors, i;
  VUser *vptr = cptr->vptr; 

  NSDL2_REPLAY(NULL, cptr, "Method called");

  NS_RESET_IOVEC(g_req_rep_io_vector);

  if(make_replay_access_log_req(cptr, &num_vectors, &g_req_rep_io_vector, now) < 0)
    return;
 
  for(i = 0; i < num_vectors; i++) {
    http_size += g_req_rep_io_vector.vector[i].iov_len;  // Here we are calculating Header size
  }

  NSDL2_REPLAY(NULL, cptr, "num_vectors = %d", num_vectors);

  if((NS_IF_TRACING_ENABLE_FOR_USER && cptr->url_num->proto.http.type != EMBEDDED_URL) ||
    /*For page dump, if inline is enabled then only dump the inline urls in page dump 
    *Otherwise dump only main url*/
    ((NS_IF_PAGE_DUMP_ENABLE_WITH_TRACE_ON_FAIL) && 
    (((cptr->url_num->proto.http.type == EMBEDDED_URL) && (runprof_table_shr_mem[vptr->group_num].gset.trace_inline_url == 1)) || (cptr->url_num->proto.http.type != EMBEDDED_URL))))
     
  {
    ut_update_req_file(vptr, http_size, num_vectors, g_req_rep_io_vector.vector);
  }

  switch (cptr->url_num->request_type)
  {
    case HTTPS_REQUEST:
      copy_request_into_buffer(cptr, http_size, &g_req_rep_io_vector);
      handle_ssl_write (cptr, now);
      return;
    case HTTP_REQUEST:
      send_http_req(cptr, http_size, &g_req_rep_io_vector, now);
      return;
  }
  return;
}

static inline void replay_vuser_trace(VUser *vptr, ReplayUserInfo *ruiptr)
{
  char vuser_trace_buf[MAX_READ_SIZE + 1];
  int bytes_read = 0;

  NSDL2_REPLAY(vptr, NULL, "Method called");

  if((bytes_read = read_request_file_to_log_req(vuser_trace_buf, MAX_READ_SIZE, ruiptr)) > 0)
  {
    struct iovec vector[1];
    vuser_trace_buf[bytes_read] = '\0';
    vector[0].iov_base = vuser_trace_buf;
    vector[0].iov_len = bytes_read;
    ut_update_req_file(vptr, bytes_read, 1, vector);
  }
}

static inline void replay_debug_trace(VUser *vptr, ReplayUserInfo *ruiptr)
{
  int num_bytes_for_dt;
  char buf[1024 + 1] = "";
  int bytes_read = 0;

  NSDL2_REPLAY(vptr, NULL, "Method called");

  num_bytes_for_dt = g_replay_req[ruiptr->start_index + ruiptr->cur_req].size < 1024 ?
      		g_replay_req[ruiptr->start_index + ruiptr->cur_req].size:1024;

  if((bytes_read = read_request_file_to_log_req(buf, num_bytes_for_dt, ruiptr)) > 0)
  {
     buf[bytes_read] = '\0';
     NS_DT4(vptr, NULL, DM_L1, MM_REPLAY, "Sending replay request at %s. User = %s, page_id = %d," 
                            " offset = %d, req_size = %d. Request (upto %d bytes) = %s",
                             get_relative_time_with_ms(), ruiptr->user_id,
                             g_replay_req[ruiptr->start_index + ruiptr->cur_req].page_id,
                             g_replay_req[ruiptr->start_index + ruiptr->cur_req].offset,
                             g_replay_req[ruiptr->start_index + ruiptr->cur_req].size,
                             bytes_read, buf);
  }
}

void process_replay_req(connection *cptr, u_ns_ts_t now)
{
  NSDL2_REPLAY(NULL, NULL, "Method Called");

  VUser *vptr;
  ReplayUserInfo *ruiptr;

  int bytes = 0;
  int to_send, ret;
  int req_file_fd = -1;
  IW_UNUSED(short cr_size = 0;)
  short cr_type = 0;
  int cr_offset = 0;

  vptr = cptr->vptr;
  ruiptr = &g_replay_user_info[vptr->replay_user_idx];

  // Check if this is first page
  // Remember ruiptr->num_req is already incremented in ns_get_replay_page_ext (which is called through API)
  // if((ruiptr->cur_req < 0) || (ruiptr->cur_req >= ruiptr->num_req)) // Removed < 0 as we changed it to unsigned short
  if(ruiptr->cur_req >= ruiptr->num_req)
  {
     NSTL1(NULL, NULL, "Invalid current page request for replay: vptr->replay_user_idx = %d, ruiptr->cur_req = %d, ruiptr->num_req = %d" , vptr->replay_user_idx, ruiptr->cur_req, ruiptr->num_req);
     Close_connection(cptr, 0, now, NS_REQUEST_ERRMISC, NS_COMPLETION_NOT_DONE);
     return;
  }

  /* If debug trace is enable enter in this block */ 
  if(debug_trace_log_value)
    replay_debug_trace(vptr, ruiptr);

  if (global_settings->replay_mode == REPLAY_USING_WEB_SERVICE_LOGS)
  {
    /* If req is generated from replay access log,
        then req log is generated for Vuser_trace, with the max size of 1MB */
    // Check if virtual user trace is enabled
    if((NS_IF_TRACING_ENABLE_FOR_USER && cptr->url_num->proto.http.type != EMBEDDED_URL) || 
       ((NS_IF_PAGE_DUMP_ENABLE_WITH_TRACE_ON_FAIL) && 
       (((cptr->url_num->proto.http.type == EMBEDDED_URL) && (runprof_table_shr_mem[vptr->group_num].gset.trace_inline_url == 1)) || (cptr->url_num->proto.http.type != EMBEDDED_URL))))
    {
      replay_vuser_trace(vptr, ruiptr); 
    }

    req_file_fd = ruiptr->req_file_fd;
    cptr->body_offset = g_replay_req[ruiptr->start_index + ruiptr->cur_req].offset;
    cr_type = g_replay_req[ruiptr->start_index + ruiptr->cur_req].type;
    to_send = cptr->bytes_left_to_send = g_replay_req[ruiptr->start_index + ruiptr->cur_req].size;
    //Only Timestamp is supported in parameterization
    if(cr_type) {
      cr_offset = g_replay_req[ruiptr->start_index + ruiptr->cur_req].creation_ts_offset;
      IW_UNUSED(cr_size = g_replay_req[ruiptr->start_index + ruiptr->cur_req].creation_ts_size);
    }
    NSDL2_REPLAY(NULL, cptr, "Method called. to_send = %d, req_file_fd = %d, cr_type = %d cr_offset = %d cr_size = %d", to_send, req_file_fd, cr_type, cr_offset, cr_size);

    // If request is HTTPS or it is parameterize, then only do lseek and reallocate buffer
    if(cptr->url_num->request_type == HTTPS_REQUEST || cr_type) {
      if (lseek(req_file_fd, cptr->body_offset, SEEK_SET) == -1)
      {
        NSTL1(NULL, NULL, "Seek failed. req_file_fd = %d, offset = %u, to_sent = %d, err = %s", req_file_fd, cptr->body_offset, to_send, nslb_strerror(errno));
        REPLAY_HANDLE_ERROR(NS_REQUEST_ERRMISC)
      }
      //Reset cptr->body_offset as buffer will be used
      cptr->body_offset = 0;

      if(to_send > cptr->free_array_size) {
        MY_REALLOC(cptr->free_array, to_send, "Allocating cptr->free_array for Replay Web Service", -1);
        cptr->free_array_size = to_send;
      }

      // Read data in nvm scratch buffer if protocol is SSL or request need to be parameterize
      while(bytes < to_send)
      {
        NSDL2_REPLAY(NULL, NULL, "Processing replay request. cptr->free_array = %p offset = %u, to_sent = %d, request_type = %d", cptr->free_array, cptr->body_offset, to_send, cptr->url_num->request_type);
        if ((ret = read(req_file_fd, cptr->free_array + bytes, to_send - bytes)) <= 0 )
        {
          if(ret == 0)
          {
            NSTL1(NULL, NULL, "Error in reading replay file EOF . bytes = %d, err=%s", bytes, nslb_strerror(errno));
            REPLAY_HANDLE_ERROR(NS_REQUEST_ERRMISC)
          }
          else if(errno == EINTR) //The call was interrupted by a signal before any data was read
          {
            NSDL2_REPLAY(NULL, NULL, "Reading of replay file interrupted, continuing");
            continue;
          }
          NSTL1(NULL, NULL, "Error in reading replay file. bytes = %d, err=%s", bytes, nslb_strerror(errno));
          REPLAY_HANDLE_ERROR(NS_REQUEST_ERRMISC)
        }
        bytes += ret;
      }
      if(cr_type)
        replay_req_parameterization(cptr, cr_offset);

      // Logging request in case of HTTPS Request and HTTP request with parameterization
      #ifdef NS_DEBUG_ON
        debug_log_replay_req_buffer(cptr, ruiptr, cptr->free_array, to_send);
      #endif
    } else {
      // Logging request in case of HTTP request without parameterization
      #ifdef NS_DEBUG_ON
        debug_log_replay_req(cptr, ruiptr);
      #endif
    } 
    send_replay_web_service_log_req(cptr, now); 
  }
  else
  {
    make_and_send_replay_access_log_req(cptr, now);
  }
}

void send_replay_req_after_partial_write(connection *cptr, u_ns_ts_t now)
{
  if(global_settings->replay_mode == REPLAY_USING_WEB_SERVICE_LOGS)
    send_replay_web_service_log_req(cptr, now);  
  else
    handle_write(cptr, now);
}


void validate_replay_access_log_settings()
{
  if (global_settings->replay_mode == 0)
    return;

  if (global_settings->schedule_by != SCHEDULE_BY_SCENARIO ||
      global_settings->schedule_type != SCHEDULE_TYPE_SIMPLE) {
    NS_EXIT(-1, "Replay Access Logs are supported only for Simple Scenario based schedules");
  }

  if (global_settings->use_prof_pct != PCT_MODE_PCT) {
    NS_EXIT(-1, "Only PCT mode supported with Replay Access Logs");
  }
}
