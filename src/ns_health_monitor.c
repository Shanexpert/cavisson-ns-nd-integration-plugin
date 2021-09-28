#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>
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
#include "ns_log.h"
#include "ns_health_monitor.h"
#include "ns_exit.h"
 
#define LINE_LENGTH 128 

#if 0
#define DM_EXECUTION 0
#define TESTCASE_OUTPUT 0
#define _FLN_ __FILE__,__LINE__,(char*)__FUNCTION__

static inline void debug_log(int log_level, int mask, char *file, int line, char *fname, char *format, ...)
{
  printf("debug_log() called\n");
}
#endif

static float old_uptime;
static float old_idletime;

/*
cat /proc/uptime                first value is uptime & second value is idle time
20777.35 19566.32               both value will be used.

cat /proc/loadavg               first value is rq_length         
0.02 0.06 0.04 1/164 18065      Only first value will be used
*/

static inline void
get_health(char file_name[], float *field1, float *field2)
{
  FILE *fp_health;
  char read_buf[LINE_LENGTH]; 

  NSDL2_TESTCASE(NULL, NULL, "Method called");  
  if ((fp_health = fopen(file_name, "r")) == NULL)
  {
    NS_EXIT(-1,"could not open file %s\n",file_name); // Should we exit here or not 
  }
   
  if (fgets(read_buf, LINE_LENGTH, fp_health) == NULL)
  {
    NS_EXIT(-1,"error reading from file %s\n",file_name);
  }
  sscanf(read_buf, "%f %f", field1, field2);
  fclose(fp_health);
}

static inline float 
get_cpu_busy()
{
  float uptime, idletime, temp;
  float window_uptime, window_idletime, window_busytime;

  NSDL2_TESTCASE(NULL, NULL, "Method called");  
  get_health("/proc/uptime", &uptime, &idletime);

  window_uptime = uptime - old_uptime;
  window_idletime = idletime - old_idletime;
  window_busytime = window_uptime - window_idletime;

  NSDL3_TESTCASE(NULL, NULL, "uptime : %f, idletime : %f, window_uptime : %f, window_idletime : %f, window_busytime : %f", uptime, idletime, window_uptime, window_idletime, window_busytime);
  temp = ( window_busytime * 100 ) / window_uptime;
  return(temp);
}

static inline float
get_rq_length()
{
  float rq_length_input, temp;
  get_health("/proc/loadavg", &rq_length_input, &temp);
  
  NSDL2_TESTCASE(NULL, NULL, "Method called, rq_length = %f", rq_length_input);
  temp = rq_length_input * 100;
  return(temp);
}

//In the below mthd we are passing the end_results for future use
int
is_sys_healthy(int init_flag, avgtime *end_results)
{
  int cpu_busy;
  int rq_length;

  NSDL2_TESTCASE(NULL, NULL, "Method called.");

  if(init_flag)   
  {
    get_health("/proc/uptime", &old_uptime, &old_idletime);
    NSDL3_TESTCASE(NULL, NULL, "old_uptime : %f, old_idletime : %f", old_uptime, old_idletime);
    return 1; // We are not concern here 
  }
  cpu_busy = get_cpu_busy();
  rq_length = get_rq_length();
   
  NSDL3_TESTCASE(NULL, NULL, "cpu_busy : %d, rq_length (*100) : %d", cpu_busy, rq_length);

  if (((rq_length > 75) && (cpu_busy > 95)) || ((rq_length > 100) && (cpu_busy > 75)))
  {
    NSDL3_TESTCASE(NULL, NULL, "System is not healthy");
    return (SYS_NOT_HEALTHY); // Not healthy
  }

#if 0
  // Todo - fix MAX_BANDWIDTH_REACHED and make it dynmic
  if (((end_results->c_tot_rx_bytes * 8)/global_settings->test_stab_time) >= MAX_BANDWIDTH_REACHED)
    return 0;
#endif

  NSDL3_TESTCASE(NULL, NULL, "System is healthy");
  return (SYS_HEALTHY); // Healthy
}


#if 0
int main()
{
  int i = 0;
  int return_value;
  while(i < 10)
  {
    printf("Running for i = %d\n", i);
    if(!i)
    {
      is_sys_healthy(1);
      printf("old_uptime = %f, old_idletime = %f\n", old_uptime, old_idletime);
      i++;
      sleep(10);
      continue;
    } 
    return_value = is_sys_healthy(0); 
    if(return_value == 0) 
      printf("System is Unhealthy................\n");
    else if(return_value == 1) 
      printf("System is Healthy..................\n");
    sleep(10);
    i++;
  } 
  return 0;
}
#endif
