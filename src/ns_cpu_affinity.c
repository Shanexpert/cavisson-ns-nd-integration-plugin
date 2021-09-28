/********************************************************************
* Name: ns_cpu_affinity.c 
* Purpose: This files contains the functions related to CPU_AFFINITY, which is used to bind a particular child process of netstorm to the particular CPU.
* Author: Anuj Dhiman
* Intial version date:     28/03/08
* Last modification date:  28/03/08
********************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>


#define __USE_GNU
#include <sched.h>
#undef __USE_GNU
#include <regex.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "nslb_util.h"
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
#include "ns_cpu_affinity.h"
#include "ns_exit.h"
#include "ns_error_msg.h"

// This is defined in ns_gdf.h
// Making NS_MAX_CPUS to 255. We cant make it 
// more than 255 because my_port_index is unsigned char
// In HPD we are supporting more than 255 children 
#define NS_MAX_CPUS    255          // Maximum number of CPUs can be given with keyword CPU_AFFINITY
#define CPU_MASK       0X0000000000000001 // CPU_MASK for the CPU ID = 0

static int num_affinity = 0;                       // Number of CPUs given with keyword CPU_AFFINITY
static unsigned  long long cpu_mask_array[NS_MAX_CPUS];  // CPU_MASK array for the CPUs 
static int g_num_cpu = -1;                         // Number of logical CPUs present in the System

/******************************************** Start : Functions used within this File only *********************************/

// left shifting the CPU_MASK by bit_to_shift, and assigning to cpu_mask_array
static void set_cpu_mask (int index, int cpu_num)
{
  NSDL2_SCHEDULE(NULL, NULL, "Method called, index = %d, cpu_num = %d\n", index, cpu_num);
  cpu_mask_array[index] = CPU_MASK << cpu_num;
}

// validate the CPU ID given with the Logical CPU ID present in the system
static void val_cpu_num(int cpu_num)
{
  NSDL2_SCHEDULE(NULL, NULL, "Method called, cpu_num = %d", cpu_num);
  if ((cpu_num < 0) || (cpu_num >= g_num_cpu))
  {
    NS_EXIT(-1, "Error: The CPU Number given (%d) with keyword 'CPU_AFFINITY' can not be greater than number of CPUs (%d) in the machine\n", cpu_num, g_num_cpu);
  }
}

/********************** End : Functions used within this File only ************************/


/********************** Start : Functions used from outside of this File *************************/


// This mthd will be called from util.c (read_keyword())
int parse_cpu_affinity(char *line_buf)
{
  int i = 0;

  NSDL2_SCHEDULE(NULL, NULL, "Method called");

  g_num_cpu = nslb_get_num_cpu(); // this is done to initialize g_num_cpu;

  NSDL3_SCHEDULE(NULL, NULL, "line_buf = %s, num_affinity = %d\n", line_buf, num_affinity);

  char *cpu_num[100];

  // This line_buf is include the keyword name also, hence decrementing by -1
  // If there is extra space in the end of the keyword line.In this case num_affinity will be one more than supplied, but it will not effect the execution, since atoi("") = 0, in the set_cpu_mask(), need to be fixed later
  num_affinity = (get_tokens(line_buf, cpu_num, " ", 100) - 1);
  //printf("num_affinity = %d\n", num_affinity);
  if(num_affinity < 1 || num_affinity > NS_MAX_CPUS)
  {
    NS_EXIT(-1, "Error: Number of fields after the CPU_AFFINITY keyword must be between 1 and 32\n");
  }

  for (i = 0; i < num_affinity; i++)
  {
    val_cpu_num(atoi(cpu_num[i + 1])); // Since first token is keyword, need to add 1
    set_cpu_mask(i, atoi(cpu_num[i + 1]));
  }
  return (0);
}

// This mthd will validate the CPU_AFFINITY, will be called from the netstorm.c just before the for loop which is creating the child process
int validate_cpu_affinity()
{
  if (num_affinity <= 0)
    return (0);

  NSDL2_SCHEDULE(NULL, NULL, "Method called");
  NSDL3_SCHEDULE(NULL, NULL, "num_affinity = %d, global_settings->num_process = %d\n", num_affinity, global_settings->num_process);

  // If user wants to run the two or more processes on the same CPU, 
  // then he has to repeat the CPU ID desired number of times with 'CPU_AFFINITY'
  // we are allowing num_affinity  to be > global_settings->num_process as netstorm 
  // decides global_settings->num_process based on several factors e.g. number of users
  if (num_affinity < global_settings->num_process)
  {
    NS_EXIT(-1, CAV_ERR_1031056, num_affinity, global_settings->num_process);
  }
  return (0);
}

// This mthd is for binding the process to the particular CPU
int set_cpu_affinity(int index, pid_t child_pid)
{
  NSDL2_SCHEDULE(NULL, NULL, "Method called, num_affinity = %d\n", num_affinity);
  if (num_affinity <= 0)
    return (0);
  //printf("child_pid = %d, cpu_mask_array[%d] = %d, ", child_pid, index,(int )cpu_mask_array[index]);
  
  if (sched_setaffinity (child_pid, sizeof (unsigned long long), (cpu_set_t *)&cpu_mask_array[index]) < 0) 
  {
    //perror("sched_setaffinity");
    fprintf(stderr," Error in setting CPU affinity for child index = %d, child_pid = %d, CPU mask is %lld. Error = %s", index, child_pid, cpu_mask_array[index], nslb_strerror(errno));
    NS_EXIT(-1, " Error in setting CPU affinity for child index = %d, child_pid = %d, CPU mask is %lld.", index, child_pid, cpu_mask_array[index]);  // We should do kill all children?
  }
  return (0);
}

/************************ End : Functions used from outside of this File *************************/

/*

// This mthd is for testing only
static void show_cpu_mask_array()
{
  int i;
  for (i = 0; i < num_affinity; i++)
  {
    printf("The %d element of cpu_mask_array is %ld\n", i, cpu_mask_array[i]);
  }
}

// For testing only, comment below code once testing is done 
int main ()
{
  char line_buf[] = "CPU_AFFINITY 0 1 2 3 4 5 67 8 9 9";
  parse_cpu_affinity(line_buf);

  printf("num_affinity = %d\n", num_affinity);
  show_cpu_mask_array();
  validate_cpu_affinity();
  return (0);
}
*/

// End of file

