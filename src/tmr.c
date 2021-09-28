#include <stdlib.h>
#include <unistd.h>
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
#include "timing.h"
#include "tmr.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "ns_log.h"

#ifdef NS_DEBUG_ON
char *get_timer_type_by_name(int value)
{
  switch(value)
  {
   case AB_TIMEOUT_IDLE:       return("AB_TIMEOUT_IDLE"); 
   case AB_TIMEOUT_THINK:      return("AB_TIMEOUT_THINK");
   case AB_TIMEOUT_STHINK:     return("AB_TIMEOUT_STHINK");
   case AB_TIMEOUT_RAMP:       return("AB_TIMEOUT_RAMP"); 
   case AB_TIMEOUT_END:        return("AB_TIMEOUT_END"); 
   case AB_TIMEOUT_PROGRESS:   return("AB_TIMEOUT_PROGRESS");
   case AB_TIMEOUT_UCLEANUP:   return("AB_TIMEOUT_UCLEANUP");
   case AB_TIMEOUT_KA:         return("AB_TIMEOUT_KA");
#ifdef RMI_MODE 
   case AB_TIMEOUT_URL_IDLE:   return("AB_TIMEOUT_URL_IDLE");
   case AB_TIMEOUT_RETRY_CONN: return("AB_TIMEOUT_RETRY_CONN");
#endif
   default: return("NA");
  }
}
#endif

discrete_timer ab_timers[] =
  {{NULL,NULL,60},
   {NULL,NULL,60},
   {NULL,NULL,1},
   {NULL,NULL,2},
   {NULL,NULL,60},
   {NULL,NULL,10},
   {NULL, NULL,10},
   {NULL, NULL,10}
#ifdef RMI_MODE
   ,{NULL, NULL, 1}
   ,{NULL, NULL, 1}
#endif
  };

static int ab_timers_count = sizeof(ab_timers)/sizeof(ab_timers[0]);

void dis_timer_init(void)
{
  int i;

  NSDL2_TIMER(NULL, NULL, "Method called. ab_timers_count=%d", ab_timers_count);
  for (i = 0; i < ab_timers_count; i++)
    ab_timers[i].next = ab_timers[i].prev = NULL;

  //ab_timers[AB_TIMEOUT_IDLE].timeout_val = globals.idle_secs*1000;
  //ab_timers[AB_TIMEOUT_THINK].timeout_val = (globals.max_think_time/2);
  ab_timers[AB_TIMEOUT_RAMP].timeout_val = 2000;
  ab_timers[AB_TIMEOUT_END].timeout_val = global_settings->test_stab_time*1000;
  ab_timers[AB_TIMEOUT_PROGRESS].timeout_val = global_settings->progress_secs;
  //ab_timers[AB_TIMEOUT_UCLEANUP].timeout_val = global_settings->user_cleanup_time;
  //ab_timers[AB_TIMEOUT_PROGRESS].timeout_val = global_settings->progress_secs*1000;
#ifdef RMI_MODE
  //ab_timers[AB_TIMEOUT_URL_IDLE].timeout_val = global_settings->url_idle_secs * 1000;
  ab_timers[AB_TIMEOUT_RETRY_CONN].timeout_val = global_settings->conn_retry_time;
#endif
}

/*
  remove element from from our timer array
  idx: the index of the element you want to remove
 */
inline void dis_timer_del(timer_type* tmr)
{
  int type = tmr->timer_type;
  NSDL2_TIMER(NULL, NULL, "Method called. type=%s, tmr=%p, now=%u", get_timer_type_by_name(type), tmr, get_ms_stamp());

  assert(type >= 0);

  if (tmr->next)
    tmr->next->prev = tmr->prev;
  else
    ab_timers[type].prev = tmr->prev;
  if (tmr->prev)
    tmr->prev->next = tmr->next;
  else
    ab_timers[type].next = tmr->next;

  tmr->timer_type = -1;
  //NSDL3_TIMER(vptr, cptr, "STS:del tmr: type=%d, tmr=0x%x DONE\n", type, (unsigned int) tmr);
}

/*If call_optimzed = 1, then call optimized
 *  else non optimized timer add function*/
inline void dis_timer_add_ex(int type, timer_type* tmr,
				 u_ns_ts_t now, TimerProc* timer_proc, ClientData client_data,
				 int periodic, int call_optimized)
{
  NSDL4_TIMER(NULL, NULL, "Method called");
  if(call_optimized)
  {
    NSDL4_TIMER(NULL, NULL, "Setting optimized timer");
    dis_timer_add(type, tmr, now, timer_proc, client_data, periodic);
  }
  else
  {
    NSDL4_TIMER(NULL, NULL, "Setting nonoptimized timer(sorted)");
    dis_timer_think_add(type, tmr, now, timer_proc, client_data, periodic);
  }
}

/* add a new timer 
   type: either AB_TIMEOUT_GQ AB_TIMEOUT_KEEPALIVE 
   idx: the index of the element in the array
 */

inline void dis_timer_add(int type, timer_type* tmr,
				 u_ns_ts_t now, TimerProc* timer_proc, ClientData client_data,
				 int periodic )
{
  NSDL2_TIMER(NULL, NULL, "Method called. type=%s, idx=%p now=%u val=%d", get_timer_type_by_name(type), tmr, now, tmr->actual_timeout);

  if(tmr->timer_type >= 0) {
	dis_timer_del(tmr);
  }

  tmr->timer_type = type;
  tmr->timer_proc = timer_proc;
  tmr->periodic = periodic;
  tmr->client_data = client_data;
  //tmr->timeout = now + ab_timers[type].timeout_val;
  tmr->timeout = now + tmr->actual_timeout; 

  if (ab_timers[type].prev)
    ab_timers[type].prev->next = tmr;
  else
    ab_timers[type].next = tmr;
  tmr->prev = ab_timers[type].prev;
  tmr->next = NULL;
  ab_timers[type].prev = tmr;
  //NSDL3_TIMER(vptr, cptr, "STS:add tmr: type=%d, idx=0x%x ADDED\n", type, (unsigned int)tmr);
}

/* Following timer put the timers in increasing order of timeout.
 * We use this method in cases like when we dont know that nxt timer timeout will be more than prev timer or not.
 * E.g: Group Based Scheduling, it may possible that there is running more than one timer at a time.
 *      Scenario Based Scheduling a single time will run at a time.
 * TODO -- Optimization ??
 */

void dis_timer_think_add(int type, timer_type* tmr,
    u_ns_ts_t now, TimerProc* timer_proc, ClientData client_data,
    int periodic )
{
  u_ns_ts_t absolute_timeout;
  timer_type* prev_timer;

  if (global_settings->non_random_timers)
	return (dis_timer_add(type, tmr, now, timer_proc, client_data, periodic ));

  NSDL2_TIMER(NULL, NULL, "Method Called. type=%s, tmr=%p now=%u val=%d", get_timer_type_by_name(type), tmr, now, tmr->actual_timeout);

#if 0
  if (type != AB_TIMEOUT_THINK) {
    fprintf(stderr, "dis_timer_think_add: timer to add must be of type AB_TIMEOUT_THINK\n");
    return;
  }
#endif

  //absolute_timeout = now + ab_timers[type].timeout_val;
  absolute_timeout = now + tmr->actual_timeout; 

  if (tmr->timer_type >= 0) {
    dis_timer_del(tmr);
  }

  tmr->timer_type = type;
  tmr->timer_proc = timer_proc;
  tmr->periodic = periodic;
  tmr->client_data = client_data;
  tmr->timeout = absolute_timeout;

  if (ab_timers[type].prev) {
    prev_timer = ab_timers[type].prev;
    while (prev_timer->timeout > absolute_timeout) {
      prev_timer = prev_timer->prev;
      if (prev_timer == NULL)
	break;
    }
    if (prev_timer) {
      if (prev_timer->next == NULL) { /*Means we got put it at end of the list */
	prev_timer->next = tmr;
	tmr->prev = prev_timer;
	tmr->next = NULL;
	ab_timers[type].prev = tmr;
      } else {  /* Means we put it within the list */
	prev_timer->next->prev = tmr;
	tmr->next = prev_timer->next;
	prev_timer->next = tmr;
	tmr->prev = prev_timer;
      }
    } else { /* Means we got to put it at the front of the list */
      tmr->next = ab_timers[type].next;
      tmr->next->prev = tmr;
      tmr->prev = NULL;
      ab_timers[type].next = tmr;
    }
  }
  else {   /* The list is empty */
    tmr->next = NULL;
    tmr->prev = NULL;
    ab_timers[type].next = tmr;
    ab_timers[type].prev = tmr;
  }

  //NSDL3_TIMER(vptr, cptr, "STS:add tmr: type=%d, tmr=%d ADDED\n", type, (unsigned int)tmr);
}

/*
  find and return the index of the first element that's going to expire

 */
timer_type* dis_timer_next(u_ns_ts_t now)
{
  int i, type;
  u_ns_ts_t timeout, min;
  timer_type* idx, *tidx;
  
  NSDL2_TIMER(NULL, NULL, "Method called. now=%u", now);
  /*NS Monitoring: In Kohls a continue monitoring test was ran for 25 days, 
  here "int min = INT_MAX" its value overflow after 25 days. Due to which PTT, session pacing and all other timers 
  were not executed and hence everytime next timer send was NULL. Therefore child process stuck into infinite 
  epoll wait. 
  Solution: Making min variable unsigned long long int*/ 
  min = ULLONG_MAX;
  idx = NULL;
  for (i = 0; i < ab_timers_count; i++) {
    while (ab_timers[i].next &&
	   ab_timers[i].next->timer_type >= 0) {
      timeout = ab_timers[i].next->timeout;
      
      if (timeout <= now) {
	/* Timer has expired */	
	tidx = ab_timers[i].next;
	type = tidx->timer_type;
	(tidx->timer_proc)( tidx->client_data, now );
	/* It is possible that that timer has been stopped during execution of callback */
	if ( (tidx->timer_type >=0 ) && (timeout == tidx->timeout) && (type == tidx->timer_type)) {
	  dis_timer_del(tidx);
	  if ( tidx->periodic )
          {
            if(type == AB_TIMEOUT_RAMP || type ==AB_TIMEOUT_END)
              dis_timer_think_add(type, tidx ,timeout, tidx->timer_proc, tidx->client_data, 1);
            else
              dis_timer_add(type, tidx ,timeout, tidx->timer_proc, tidx->client_data, 1);
          }
	}
	continue;
      }
	
      if (timeout < min) {
	min = timeout;
	idx = ab_timers[i].next;
      }
      break;
    }
  }

  //NSDL3_TIMER(vptr, cptr, "STS:next tmr returns 0x%x: \n", (unsigned int)idx);
  return idx;
}

/* Execute the already expired timers */
void dis_timer_run_ex(u_ns_ts_t now, int tmr_type)
{
  register int i, /*min,*/ type, start_idx, end_idx;
  register u_ns_ts_t timeout;
  //register timer_type* idx;
  register timer_type* tidx;

  NSDL2_TIMER(NULL, NULL, "Method called. now=%u", now);

  //min = INT_MAX;
  //idx = NULL;
  if (tmr_type == -1)
  {
    start_idx = 0;
    end_idx = ab_timers_count;
  }
  else if (tmr_type < ab_timers_count)
  {
    start_idx = tmr_type;
    end_idx = tmr_type + 1;
  } 
  else
  {  
     //TODO
     return;
  } 

  for (i = start_idx; i < end_idx; i++) {
    while (ab_timers[i].next &&
	   ab_timers[i].next->timer_type >= 0) {
      timeout = ab_timers[i].next->timeout;
      if (timeout <= now) {
	/* Timer has expired */	
	tidx = ab_timers[i].next;
	type = tidx->timer_type;

	(tidx->timer_proc)( tidx->client_data, now );
	/* It is possible that that timer has been stopped during execution of callback */
	if ((tidx->timer_type >=0 ) && (timeout == tidx->timeout) && (type == tidx->timer_type)) {
	  dis_timer_del(tidx);
	  if ( tidx->periodic )
          {
            if(type == AB_TIMEOUT_RAMP || type ==AB_TIMEOUT_END)
              dis_timer_think_add(type, tidx ,timeout, tidx->timer_proc, tidx->client_data, 1);
            else
              dis_timer_add(type, tidx ,timeout, tidx->timer_proc, tidx->client_data, 1);
          }
	}
	continue;
      } else
        break;
    }
  }
  return;
}

void dis_timer_run(u_ns_ts_t now)
{
  dis_timer_run_ex(now, -1);
}

inline void dis_timer_reset( u_ns_ts_t now, timer_type* tmr)
{
  dis_timer_reset_ex(now, tmr, 1);
}

/* Change the start time of a timer */

inline void dis_timer_reset_ex( u_ns_ts_t now, timer_type* tmr, int call_optimized)
{
  int type, periodic;
  NSDL2_TIMER(NULL, NULL, "Method called. tmr=%p: now=%u", tmr, now);
  
  type = tmr->timer_type;
  periodic = tmr->periodic;
  dis_timer_del(tmr);
  dis_timer_add_ex(type, tmr ,now, tmr->timer_proc, 
		tmr->client_data, periodic, call_optimized);
  //NSDL3_TIMER(vptr, cptr, "STS:reset tmr=0x%x Done: \n", (unsigned int) tmr);
}

/*REset timer for DLE connection. This function
 * will get call only for IDLE timer*/
inline void dis_idle_timer_reset (u_ns_ts_t now, timer_type* tmr)
{
  int type, periodic;
  NSDL2_TIMER(NULL, NULL, "Method called. tmr=%p: now=%u", tmr, now);
  
  type = tmr->timer_type;
  periodic = tmr->periodic;

  NSDL2_TIMER(NULL, NULL, "RESET TIMER: type = %d, timeout = %llu, actual_timeout = %d, periodic = %d, timer_status = %d", 
                           type, tmr->timeout, tmr->actual_timeout, tmr->periodic, tmr->timer_status);

  dis_timer_del(tmr);
  dis_timer_add_ex(type, tmr ,now, tmr->timer_proc, 
		tmr->client_data, periodic, global_settings->idle_timeout_all_flag);
  //NSDL3_TIMER(vptr, cptr, "STS:reset tmr=0x%x Done: \n", (unsigned int) tmr);
}


// Not Used is netstorm as we are applying Ramp Down Method on each user
#if 0

int dis_timer_remove_sess_think( void ) {
  timer_type* iter_timer;
  timer_type* del_timer;
  int return_val = 0;

#if 0
  for (iter_timer = ab_timers[AB_TIMEOUT_STHINK].next; iter_timer; ) {
      num++; 
      iter_timer = iter_timer->next;
  }
#endif
  NSDL2_TIMER(NULL, NULL, "Method called");
  for (iter_timer = ab_timers[AB_TIMEOUT_STHINK].next; iter_timer; ) {
    //if (iter_timer->think_type == NS_VUSER_SESSION_THINK) 
    {
      del_timer = iter_timer;
      iter_timer = del_timer->next;
      dis_timer_del(del_timer);
      return_val++;
      if (!iter_timer)
	break;
    }
    //else
     // iter_timer = iter_timer->next;
  }

#if 0
  if (num != return_val) {
	printf("PID=%d:******** NOT GOOD num is %d and actule del is %d\n", getpid(), num, return_val);
  } else {
	printf("PID=%d:******** GOOD num is %d and actule del is %d\n", getpid(), num, return_val);
  }
#endif

  return return_val;
}

void dis_mark_all_think_expire( u_ns_ts_t now ) {
  timer_type* iter_timer;
  timer_type* del_timer;

  NSDL2_TIMER(NULL, NULL, "Method called. now = %u", now);
  for (iter_timer = ab_timers[AB_TIMEOUT_THINK].next; iter_timer; ) {
    if (iter_timer->timer_type >= 0) {
      del_timer = iter_timer;
      iter_timer = del_timer->next;
      del_timer->timeout = now;
      if (!iter_timer)
	break;
    }
    else
      iter_timer = iter_timer->next;
  }

  return ;
}

// This mthd will be used for setting the ramp_down_ideal_msecs in the ab_timers[AB_TIMEOUT_IDLE], Added by Anuj: 03/04/08
void dis_mark_all_idle_ramp_down( u_ns_ts_t now ) 
{
  NSDL2_TIMER(NULL, NULL, "Method called. now = %u", now);
  timer_type* iter_timer;
  timer_type* del_timer;
  unsigned int idle_msec = (now + global_settings->ramp_down_ideal_msecs);
  //printf("dis_mark_all_idle_ramp_down(): now = %u\n", now);

  for (iter_timer = ab_timers[AB_TIMEOUT_IDLE].next; iter_timer; ) {
    if (iter_timer->timer_type >= 0) {
      del_timer = iter_timer;
      iter_timer = del_timer->next;
      //      printf("dis_mark_all_idle_ramp_down(): current del_timer->timeout = %u\n", del_timer->timeout);
      if ( del_timer->timeout > idle_msec )
        del_timer->timeout = idle_msec;
        NSDL2_TIMER(NULL, NULL, "Now the del_timer->timeout has been set to (%u)", idle_msec);
      if (!iter_timer)
	break;
    }
    else
      iter_timer = iter_timer->next;
  }

  return ;
}

#endif
