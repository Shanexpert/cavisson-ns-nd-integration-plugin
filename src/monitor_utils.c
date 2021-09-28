/******************************************************************
 * Name    : monitor_utils.c
 * Author  : Neeraj Jain
 * Purpose : This contains util functions for nsu_monitor
 * Modification History:
 * Note:
 *
 *
 * 02/14/08 - Initial Version
*****************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/stat.h>

#include <string.h>
#include <time.h>

#include "monitor_utils.h"
#include "nslb_util.h"

static unsigned int max_debug_log_file_size = 100000000;  // Approx 100MB
static unsigned int max_error_log_file_size = 10000000;   // Approx 10 MB

static int error_fd = -1;
static int trace_fd = -1;
static int debug_fd = -1;
extern int verbose;
extern int my_child_id, my_sub_child_id;

//#define MAX_LOG_BUF_SIZE  64000
#define MAX_LOG_BUF_SIZE  2*1024*1024   // changed to 2 MB because response body is logged in debug at some places 

// This method will create the error.log file in the append_mode
static inline void open_log(char *name, int open_flags, int *fd, int max_size)
{
char log_file[1024], log_file_prev[1024];
char wdir[1024];
char command[1024];
struct stat stat_buf;

  if (getenv("NS_WDIR") != NULL)
  {
    strcpy(wdir, getenv("NS_WDIR"));
    sprintf(log_file, "%s/webapps/netstorm/logs/%s", wdir, name);
  }
  else
    sprintf(log_file, "/tmp/%s", name);

  // Check size using stat and size > max size of debug or error log fie
  if((stat(log_file, &stat_buf) == 0) && (stat_buf.st_size > max_size))  //
  {
   // check if fd is open, then close it
    if(*fd > 0)
    {
      close(*fd);
      *fd = -1;
    }
    sprintf(log_file_prev, "%s.prev", log_file);
   // Move current file as prev file using mv -f
    sprintf(command, "mv -f %s %s", log_file, log_file_prev);
    // Never use debug_log from here
    if (verbose) printf("Moving file %s with size %lu to %s, Max size = %d", log_file, stat_buf.st_size, log_file_prev, max_size);
   int status;
   status = system(command);
   if(status < 0)
    // Never use debug_log from here
     fprintf(stderr, "Error: Error in moving file, err = %s\n", nslb_strerror(errno));
  }
  
  if(*fd <= 0) //if fd is not open then open it
  {
    *fd = open (log_file, O_CREAT|O_WRONLY|O_CLOEXEC|open_flags, 00666);
    if (!fd)
    {
      fprintf(stderr, "Error: Error in opening file '%s', err = %s\n", log_file, nslb_strerror(errno));
      exit (-1);
    }
  }
}

// nsu_monitor need to call it in start to make error log empty 
// so that in case nsu_monitor core dumps without loggin any error
// error file is not from previous recording
void open_trace_log()
{
  char file_name[1024];
  sprintf(file_name, "monitor_trace.log");

  open_log(file_name, O_TRUNC, &trace_fd, max_error_log_file_size);
}

void open_error_log()
{
  char file_name[1024];
  sprintf(file_name, "monitor_error.log");

  open_log(file_name, O_TRUNC, &error_fd, max_error_log_file_size);
}

void error_log(int log_level, char *file, int line, char *fname, char *format, ...)
{
va_list ap;
char buffer[MAX_LOG_BUF_SIZE + 1];
char hdr[1024];
int amt_written = 0;
char cur_time_buf[100];

  va_start (ap, format);
  amt_written = vsnprintf(buffer, MAX_LOG_BUF_SIZE, format, ap);
  va_end(ap);
  buffer[amt_written] = 0;

  // Create error log in truncate mode so that we have errors for the
  // current recording only. This error log will be used by GUI to show errors
  open_error_log(); // It will not open if aleady open and is not moved to prev

  // Write errors without hdr fields as this is used in GUI
  write(error_fd, "\n", 1);
  write(error_fd, buffer, amt_written);

  // We log error in debug log also as it is easy to correlate error with debug logs
  open_log("monitor_debug.log", O_APPEND, &debug_fd, max_debug_log_file_size);

  // Add new line so that called need not put \n
  sprintf(hdr, "\n%s|%s|%d|%s|Error|child_id=%d|sub_child_id=%d|pid=%d|", nslb_get_cur_date_time(cur_time_buf, 0), file, line, fname, my_child_id, my_sub_child_id, getpid());
  write(debug_fd, hdr, strlen(hdr)); // write without new line
  write(debug_fd, buffer, amt_written);
}

void trace_log(char* buffer)
{
  // Create trace log in truncate mode so that we have errors for the
  // current recording only. This trace log will be used by GUI to show trace
  open_trace_log(); // It will not open if aleady open and is not moved to prev

  // Write errors without hdr fields as this is used in GUI
  write(trace_fd, buffer, strlen(buffer));
}

void debug_log(int log_level, char *file, int line, char *fname, int child_id, int sub_child_id, int id_req_resp, char *format, ...)
{
va_list ap;
char buffer[MAX_LOG_BUF_SIZE + 1];
char file_name[1024 + 1];
char hdr[1024];
int amt_written = 0;
char curr_time_buffer[100];

  if((log_level > 0) && (verbose == 0))
    return;
  va_start (ap, format);
  amt_written = vsnprintf(buffer, MAX_LOG_BUF_SIZE, format, ap);
  va_end(ap);

  buffer[MAX_LOG_BUF_SIZE] = 0;

  //if(debug_fd == -1) debug_fd = open_log("monitor_debug.log", O_APPEND);
  sprintf(file_name, "monitor_debug.log");
  open_log(file_name, O_APPEND, &debug_fd, max_debug_log_file_size);

  // Add new line so that called need not put \n
  sprintf(hdr, "\n%s|%s|%d|%s|Debug|child_id=%d|sub_child_id=%d|pid=%d|%d_%d_%d|", nslb_get_cur_date_time(curr_time_buffer, 0), file, line, fname, my_child_id, my_sub_child_id, getpid(), child_id, sub_child_id, id_req_resp);
  write(debug_fd, hdr, strlen(hdr)); // write without new line
  write(debug_fd, buffer, amt_written);
}

/**
int verbose = 1;
main()
{
  error_log(0, _FL_, "test", "This is error. Code = %d, name = %s\n", 10, "Neeraj");
  debug_log(0, _FL_, "test", "This is debug log. Code = %d\n", 10);
}
**/
