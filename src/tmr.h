#ifndef TMR_H
#define TMR_H
#include <assert.h>
#include <limits.h>
#include <sys/time.h>

#include "ns_data_types.h"

#define AB_TIMEOUT_PROGRESS 0
#define AB_TIMEOUT_IDLE 1
#define AB_PARENT_SP_OVERALL_TIMEOUT AB_TIMEOUT_IDLE  //for sync point
// This timeout type is added for Syncpoint where RELEASE_TYPE is TIME/PERIOD/PERIODIC
#define AB_PARENT_SP_RELEASE_TYPE_TIMEOUT AB_TIMEOUT_IDLE
#define AB_TIMEOUT_THINK 2
#define AB_PARENT_NEW_TEST_RUN_TIMEOUT AB_TIMEOUT_THINK //Added for hourly monitoring NS
#define AB_TIMEOUT_STHINK 3
#define AB_PARENT_SP_VUSER_ARRIVAL_TIMEOUT AB_TIMEOUT_STHINK
#define AB_TIMEOUT_RAMP 4
#define AB_TIMEOUT_END 5
#define AB_TIMEOUT_UCLEANUP 6
#define AB_TIMEOUT_KA       7
#define AB_RTG_REWRITE_TIMEOUT AB_TIMEOUT_RAMP //

#ifdef RMI_MODE
#define AB_TIMEOUT_URL_IDLE 8
#define AB_TIMEOUT_RETRY_CONN 9
#define AB_MAX_TABLE_SIZE 10
#else
#define AB_MAX_TABLE_SIZE 8 
#endif

/* ClientData is a random value that tags aLong with a timer.  The client
** can use it for whatever, and it gets passed to the callback when the
** timer triggers.
*/
typedef struct {
  union {
    void* p;
    int i;
    int l;
  };
  void* x;
  u_ns_ts_t timer_started_at; 
} ClientData;

/* The TimerProc gets called when the timer expires.  It gets passed
** the ClientData associated with the timer, and a timeval in case
** it wants to schedule another timer.
*/
typedef void TimerProc( ClientData client_data, u_ns_ts_t now );

typedef struct timer_type {
  u_ns_ts_t timeout;
  struct timer_type* next;
  struct timer_type* prev;
  TimerProc* timer_proc;
  ClientData client_data;
  int actual_timeout;
  char periodic;        
  char timer_type;
  char timer_status;
  char dummy;
} timer_type;

typedef struct discrete_timer {
    timer_type* prev; /* to the latest entry */
    timer_type* next; /* to the oldest entry */
    /* Following fields is used only in HPD, here we use int actual_timeout; of timer_type struct,
     * as we were facing timer issue in group based scenario
     */
    int timeout_val; /* value for this discrete timer, in seconds */
} discrete_timer;

extern discrete_timer ab_timers[];

extern void dis_timer_init(void);
extern void dis_timer_run_ex(u_ns_ts_t now, int tmr_type);
extern inline void dis_timer_del(timer_type* tmr);
extern inline void dis_timer_add_ex(int type, timer_type* tmr,
			  u_ns_ts_t now, TimerProc* timer_proc, ClientData client_data,
			  int periodic, int call_optimized);
extern inline void dis_timer_add(int type, timer_type* tmr,
			  u_ns_ts_t now, TimerProc* timer_proc, ClientData client_data,
			  int periodic );
extern void dis_timer_think_add(int type, timer_type* tmr,
				u_ns_ts_t now, TimerProc* timer_proc, ClientData client_data,
				int periodic );
extern timer_type* dis_timer_next(u_ns_ts_t now);
extern void dis_timer_run(u_ns_ts_t now);
extern inline void dis_timer_reset( u_ns_ts_t now, timer_type* tmr);
extern inline void dis_timer_reset_ex( u_ns_ts_t now, timer_type* tmr, int call_optimized);
extern inline void dis_idle_timer_reset( u_ns_ts_t now, timer_type* tmr);
extern int dis_timer_remove_sess_think(void);
extern void dis_mark_all_think_expire( u_ns_ts_t now );
extern void dis_mark_all_idle_ramp_down (u_ns_ts_t now);
extern char *get_timer_type_by_name(int value);
#endif
