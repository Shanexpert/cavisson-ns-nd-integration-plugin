#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <regex.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string.h>
#include <time.h>
#include <ctype.h>

#include "cdr_log.h"
#include "cdr_main.h"
#include "cdr_config.h"

int cdr_tmp_trace_fd          = -1;
int cdr_trace_fd          = -1;
int cdr_audit_fd          = -1;
int g_debug_level = 0X000000FF;


static char *get_cur_date_time()
{
  long    tloc;
  struct  tm *lt;
  static  char cur_date_time[CDT_BUFF];

  (void)time(&tloc);
  if((lt = localtime(&tloc)) == (struct tm *)NULL)
    strcpy(cur_date_time, "Error|Error");
  else
    sprintf(cur_date_time, "%02d/%02d/%02d %02d:%02d:%02d",  lt->tm_mon + 1, lt->tm_mday, (1900 + lt->tm_year)%2000, lt->tm_hour, lt->tm_min, lt->tm_sec);
  return(cur_date_time);
}

// This mthd will create the trace.log file in the append_mode
static void open_log(char *name, int *fd, unsigned long max_size, char *header, char include_default_path)
{
  char log_file[BUFF_SIZE_1024], log_file_prev[BUFF_SIZE_1024 * 2];
  struct stat stat_buf;
  int status;
  if(include_default_path)
    sprintf(log_file, "%s/logs/data_retention/logs/%s", ns_wdir, name);
  else 
    sprintf(log_file, "%s", name);

  // Check size using stat and size > max size of debug or error log fie
  if((stat(log_file, &stat_buf) == 0) && (stat_buf.st_size > max_size)) 
  {
    //Check if fd is open, close it
    if(*fd > 0)
    {
      close(*fd);
      *fd = -1;
    }
    sprintf(log_file_prev, "%s.prev", log_file);

   status = rename(log_file, log_file_prev);

   if(status < 0)
     fprintf(stderr, "Error in moving '%s' file, err = %s\n", log_file, strerror(errno));
  }

  if (*fd <= 0 ) //if fd is not open then open it
  {
    *fd = open (log_file, O_CREAT|O_WRONLY|O_APPEND|O_CLOEXEC, 00666);

    if(!*fd)
      fprintf(stderr, "Error: Error in opening file '%s', Error = '%s'\n", log_file, strerror(errno));
    else if ( (stat(log_file, &stat_buf) == 0) && (stat_buf.st_size == 0))
      dprintf(*fd, "%s", header);
  }
}

void cdr_tmp_trace_log(int log_level, char *file, int line, char *fname,  char *format, ...)
{
  va_list ap;
  char buffer[MAX_DEBUG_ERR_LOG_BUF_SIZE + 1];
  int amt_written = 0, amt_written1=0;

  
  if((log_level & g_debug_level) == 0)
    return;

  //"Absolute Time Stamp|File|Line|Function|Logs"
  amt_written1 = sprintf(buffer, "\n%s|%s|%d|%s|", get_cur_date_time(), file, line, fname);

  va_start (ap, format);
  amt_written = vsnprintf(buffer + amt_written1 , MAX_DEBUG_ERR_LOG_BUF_SIZE - amt_written1, format, ap);
  va_end(ap);

  /**
   *  Upon  successful return, vsnprintf return the number of characters printed (not including the trailing ’\0’ used
   *  to end output to strings).  The functions vsnprintf do not write more  than  size  bytes  (including  the
   *  trailing  ’\0’).  If the output was truncated due to this limit then the return value is the number of characters (not
   *  including the trailing ’\0’) which would have been written to the final string if enough  space  had  been  available.
   *  Thus,  a return value of size or more means that the output was truncated. (See also below under NOTES.)  If an output
   *  error is encountered, a negative value is returned.
   */

  buffer[MAX_DEBUG_ERR_LOG_BUF_SIZE] = 0;

  // In some cases, vsnprintf return -1 but data is copied in buffer
  // This is a quick fix to handle this. need to find the root cause
  if(amt_written < 0)
    amt_written = strlen(buffer) - amt_written1;

  if(amt_written > (MAX_DEBUG_ERR_LOG_BUF_SIZE - amt_written1))
    amt_written = (MAX_DEBUG_ERR_LOG_BUF_SIZE - amt_written1);

  open_log("/home/cavisson/drm_trace.log", &cdr_tmp_trace_fd, 104857600, DEBUG_HEADER, 0);
  write(cdr_tmp_trace_fd, buffer, amt_written + amt_written1);
}
void cdr_trace_log(int log_level, char *file, int line, char *fname,  char *format, ...)
{
  va_list ap;
  char buffer[MAX_DEBUG_ERR_LOG_BUF_SIZE + 1];
  int amt_written = 0, amt_written1=0;

  
  if((log_level & g_debug_level) == 0)
    return;

  //"Absolute Time Stamp|File|Line|Function|Logs"
  amt_written1 = sprintf(buffer, "\n%s|%s|%d|%s|", get_cur_date_time(), file, line, fname);

  va_start (ap, format);
  amt_written = vsnprintf(buffer + amt_written1 , MAX_DEBUG_ERR_LOG_BUF_SIZE - amt_written1, format, ap);
  va_end(ap);

  /**
   *  Upon  successful return, vsnprintf return the number of characters printed (not including the trailing ’\0’ used
   *  to end output to strings).  The functions vsnprintf do not write more  than  size  bytes  (including  the
   *  trailing  ’\0’).  If the output was truncated due to this limit then the return value is the number of characters (not
   *  including the trailing ’\0’) which would have been written to the final string if enough  space  had  been  available.
   *  Thus,  a return value of size or more means that the output was truncated. (See also below under NOTES.)  If an output
   *  error is encountered, a negative value is returned.
   */

  buffer[MAX_DEBUG_ERR_LOG_BUF_SIZE] = 0;

  // In some cases, vsnprintf return -1 but data is copied in buffer
  // This is a quick fix to handle this. need to find the root cause
  if(amt_written < 0)
    amt_written = strlen(buffer) - amt_written1;

  if(amt_written > (MAX_DEBUG_ERR_LOG_BUF_SIZE - amt_written1))
    amt_written = (MAX_DEBUG_ERR_LOG_BUF_SIZE - amt_written1);

  open_log("drm_trace.log", &cdr_trace_fd, cdr_config.log_file_size, DEBUG_HEADER, 1);
  write(cdr_trace_fd, buffer, amt_written + amt_written1);
}


extern void cdr_audit_log(int entry_type, int tr_num, long long int partition_num, char *cmp, long long int size, char *operation, char *discription, long long int tot_time, char *user, char *nv_client_id)
{
  char buffer[MAX_DEBUG_ERR_LOG_BUF_SIZE + 1];
  int amt_written = 0;
  
  //"Delete date and time | TR # / Client ID | Partition |Component |size | Opretion | Description| Time taken by operation| UserName"
  if(nv_client_id)
    amt_written = sprintf(buffer, "\n%s|%s|%lld|%s|%lld|%s|%s|%lld|%s", get_cur_date_time(), nv_client_id, partition_num, cmp, size, operation, discription, tot_time, user);
  else
  {
    if(entry_type == 1)
      amt_written = sprintf(buffer, "\n%s|%d|%lld|%s|%lld|%s|%s|%lld|%s", get_cur_date_time(), tr_num, partition_num, cmp, size, operation, discription, tot_time, user);
    else 
      amt_written = sprintf(buffer, "\n%s|%d|%lld|%s|%lld|%s|%s|%lld|%s", get_cur_date_time(), tr_num, partition_num, cmp, size, operation, discription, tot_time, user);
  }


  buffer[MAX_DEBUG_ERR_LOG_BUF_SIZE] = 0;

  if(amt_written > (MAX_DEBUG_ERR_LOG_BUF_SIZE - amt_written))
    amt_written = (MAX_DEBUG_ERR_LOG_BUF_SIZE - amt_written);

  open_log("audit.log", &cdr_audit_fd, cdr_config.audit_log_file_size, AUDIT_HEADER, 1);
  write(cdr_audit_fd, buffer, amt_written);
}
