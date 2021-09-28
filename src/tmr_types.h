#include <assert.h>
#include <limits.h>
#include <sys/time.h>

#define AB_TIMEOUT_IDLE 0
#define AB_TIMEOUT_THINK 1
#define AB_TIMEOUT_RAMP 2
#define AB_TIMEOUT_END 3
#define AB_TIMEOUT_PROGRESS 4
#define AB_TIMEOUT_UCLEANUP 5
#define AB_MAX_TABLE_SIZE 6

/* ClientData is a random value that tags along with a timer.  The client
** can use it for whatever, and it gets passed to the callback when the
** timer triggers.
*/
typedef union {
  void* p;
  int i;
  long l;
} ClientData;

/* The TimerProc gets called when the timer expires.  It gets passed
** the ClientData associated with the timer, and a timeval in case
** it wants to schedule another timer.
*/
typedef void TimerProc( ClientData client_data, u_ns_ts_t now );

typedef struct timer_type {
  int timer_type;
  unsigned long timeout;
  struct timer_type* next;
  struct timer_type* prev;
  TimerProc* timer_proc;
  ClientData client_data;
  int periodic;        
} timer_type;

typedef struct discrete_timer {
    timer_type* prev; /* to the latest entry */
    timer_type* next; /* to the oldest entry */
    int timeout_val; /* value for this discrete timer, in seconds */
} discrete_timer;
