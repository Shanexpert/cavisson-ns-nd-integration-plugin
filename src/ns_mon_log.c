/******************************************************************
 * Name                 : ns_mon_log.c
 * Author               : Arun Nishad
 * Purpose              : Log to monitor.log, event.log or Console based on Mask value passed file.
 * Initial Version      : Tuesday, March 03 2009
 * Modification History :
*****************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>
#include <regex.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "nslb_sock.h"
#include "nslb_cav_conf.h"
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
#include "ns_dynamic_vector_monitor.h"
#include "ns_custom_monitor.h"
#include "ns_check_monitor.h"
#include "ns_user_monitor.h"
#include "ns_event_log.h"
#include "ns_mon_log.h"
#include "ns_log.h"
#include "ns_event_id.h"
#include "ns_trace_level.h"
#include "ns_exit.h"
#include "ns_string.h"

int monitor_log_fd = -1; //monitor.log fd

/* This function will set host in event log as NSAppliance or NDAppliance if moniotr is running on own machine */
inline void set_host(char *in_host, char *out_host)
{
  if(strcmp(in_host, "127.0.0.1"))
    return;

  if(!strcmp(g_cavinfo.config, "NDE"))
    strcpy(out_host, "NDAppliance");
  else
    strcpy(out_host, "NSAppliance");
}

//this method can be called at max 3 times, for MONITOR, CUSTOM_MONITOR, CHECK_MONITOR but opens the files only the once, 
static void open_monitor_log_file()
{
  if (monitor_log_fd <= 0 ) //if fd is not open then open it
  {
    char log_file[1024];
    sprintf(log_file, "%s/logs/%s/monitor.log", g_ns_wdir, global_settings->tr_or_partition);
    monitor_log_fd = open(log_file, O_CREAT|O_WRONLY|O_APPEND|O_CLOEXEC, 00666);
    if (!monitor_log_fd)
    {
      NS_EXIT(-1, "Error: Error in opening file '%s', Error = '%s'",log_file, nslb_strerror(errno));
    }
    else
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "monitor.log fd = %d'\n", monitor_log_fd);
    }
  }
}

// char *event will be in following format
// Event:0.1:Info|This is an event
// & what to log
char *extract_header_from_event(char *event, int *severity)
{
  char *ptr1, *ptr2, *ptr3;
  int version_length = 0,sever_length = 0;
  
  char version[10]="\0";
  char sever[32]="\0";
 
  if((event = strstr(event, "Event:")) != NULL)
  {
    ptr1 = index(event, ':');
    if(!ptr1)
      return NULL;
    ptr1++; //ignore :

    ptr2 = index(ptr1 + 1, ':');
    if(!ptr2)
      return NULL;
    ptr2++;  //ignore :

    ptr3 = index(ptr2 + 1, '|');

    if(!ptr3)
      return NULL;
    ptr3++;  //ignore |
    
    version_length = strlen(ptr1) - strlen(ptr2) - 1;
    sever_length = strlen(ptr2) - strlen(ptr3) - 1;
    //we got a coredump here bcz of garbage value the length of severity string was more than 32
    //due to which we were copying more than 32 char in string copy so checking before string copy
    if( version_length < 9 )
      strncpy(version, ptr1, version_length);
    else
      return NULL;

    if( sever_length < 31 )
      strncpy(sever, ptr2, sever_length);
    else
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Invalid Event = '%s' found\n", event);
      *severity = 2; 
      return NULL;
    }
    *severity = convert_string_to_int(sever);
    return(ptr3); //it has '|'
  }
  return NULL;
}

/*  Mask description :
 *  Mask-Value   CONSOLE  MONITOR_LOG EVENT_LOG
 *    1            Yes       No         No
 *    2            No        Yes        No
 *    3            Yes       Yes        No
 *    4            No        No         Yes
 *    5            Yes       No         Yes
 *    6            No        Yes        Yes
 *    7            Yes       Yes        Yes
 */    

//called from smon.c  for "MONITOR"
//we will log all event to monitor.log also
void ns_monitor_log(int mask, int debug_mask, int module_mask, char *file, int line,
			      char *func, char *host, char *mon_name, int fd,
			      unsigned int event_id, int severity, char *format, ...)
{
  char buffer[MAX_EVENT_LOG_BUF_SIZE + 1 ]="\0";
  char mon_name_without_path[255 + 1 ]="\0";
  char tmp_host[1024 + 1 ] = "\0";
  int amt_written, amt_written1;
  va_list ap;
  char time_buf[100];
  amt_written = amt_written1 = 0;
 
  //may possible mon_name dont have path ('/'), 
  if(rindex(mon_name, '/'))
    strcpy(mon_name_without_path, rindex(mon_name, '/') + 1);

  if(!strcmp(host, "NS")) 
    strcpy(tmp_host, g_cavinfo.NSAdminIP); 
  else if(!strcmp(host, "NO"))
    strcpy(tmp_host, g_cavinfo.SRAdminIP);
  else
    strcpy(tmp_host, host);

  /*In case of NS monitoring itself, host name should be NSAppliance*/
  set_host(host, tmp_host); 
 
  amt_written1 = sprintf(buffer, "\n%s|%s|Monitor|%s|%s|%d|", nslb_get_cur_date_time(time_buf, 1), tmp_host, mon_name_without_path, "-",fd);
  va_start (ap, format);
  amt_written = vsnprintf(buffer + amt_written1, MAX_EVENT_LOG_BUF_SIZE - amt_written1, format, ap);
  va_end(ap);
  buffer[MAX_EVENT_LOG_BUF_SIZE] = 0;

  if(mask & EL_CONSOLE)
    fprintf(stderr, "%s\n", buffer + amt_written1);       //write to console

  if(mask & EL_DEBUG)
     debug_log(debug_mask, module_mask, file, line, func, NULL, NULL, buffer + amt_written1);   //write to debug log

  //if(mask & EL_FILE)
    //ns_log_event_write(severity, buffer, tot_write);                         //write to event log
  /* Here first attribute should be host name, while logging monitor event we assume that first attribute of
   * event log should be host name*/
  NSDL3_MON(NULL, NULL, "host = %s", tmp_host);
  NS_EL_3_ATTR(event_id, -1, -1, 1, severity, tmp_host, "Monitor", mon_name_without_path, buffer + amt_written1);
  
  open_monitor_log_file();
  if(write(monitor_log_fd, buffer, amt_written1 + amt_written) <0)  //write to monitor.log
  {
    NS_EXIT (-1, "Can not write to monitor.log with monitor fd = %d Error is %s", monitor_log_fd, nslb_strerror(errno));
  }
}

void ns_cm_monitor_log(int mask, int debug_mask, int module_mask, char *file, int line,
                       char *func, CM_info *cm_info_mon_conn_ptr, unsigned int event_id, int severity, char *format, ...) {

  char buffer[MAX_EVENT_LOG_BUF_SIZE + 1 ]="\0";
  char host[1024]="\0";
  char gdf[1024]="\0";
  char g_mon_id[1024]="\0";
  char vector_name[2*1024] = "\0";
  int amt_written, amt_written1, fd = -1;
  va_list ap;
  char time_buf[100];

  amt_written = amt_written1 = 0;

  //may possible mon_name dont have path ('/'), 

  if(cm_info_mon_conn_ptr != NULL)
  {
    if(rindex(cm_info_mon_conn_ptr->gdf_name, '/'))
      strcpy(gdf, rindex(cm_info_mon_conn_ptr->gdf_name, '/') + 1);
    else
      strcpy(gdf, cm_info_mon_conn_ptr->gdf_name);

    if(cm_info_mon_conn_ptr->access == RUN_LOCAL_ACCESS)
    {
      if(cm_info_mon_conn_ptr->server_display_name)
        strcpy(host, cm_info_mon_conn_ptr->server_display_name);
    }
    else
    {
      if(cm_info_mon_conn_ptr->rem_ip != NULL)
        strcpy(host, cm_info_mon_conn_ptr->rem_ip);
    }

    if(cm_info_mon_conn_ptr->monitor_name != NULL)
      strcpy(vector_name, cm_info_mon_conn_ptr->monitor_name);
    fd = cm_info_mon_conn_ptr->fd;

    if(cm_info_mon_conn_ptr->g_mon_id != NULL)
      strcpy(g_mon_id, cm_info_mon_conn_ptr->g_mon_id);
  }
  else
  {
    strcpy(vector_name, "NA");
    strcpy(gdf, "NA");
    strcpy(host, "NA");
    strcpy(g_mon_id, "-1");
  }

  if(!strcmp(host, "NS"))
    strcpy(host, g_cavinfo.NSAdminIP);
  else if(!strcmp(host, "NO"))
    strcpy(host, g_cavinfo.SRAdminIP);

  /*In case of NS monitoring itself, host name should be NSAppliance*/
  set_host(host, host);

  amt_written1 = sprintf(buffer, "\n%s|%s|Custom Monitor|%s|%d|%s:%s|%s|%s|%d|", nslb_get_cur_date_time(time_buf, 1),  host,file,line, gdf, vector_name, g_mon_id, "-", ((is_outbound_connection_enabled) && (cm_info_mon_conn_ptr != NULL)) ? cm_info_mon_conn_ptr->mon_id : fd);
  va_start (ap, format);
  amt_written = vsnprintf(buffer + amt_written1, MAX_EVENT_LOG_BUF_SIZE - amt_written1, format, ap);
  va_end(ap);
  buffer[MAX_EVENT_LOG_BUF_SIZE] = 0;

  if(mask & EL_CONSOLE)
    fprintf(stderr, "%s\n", buffer + amt_written1);       //write to console

  if(mask & EL_DEBUG)
     debug_log(debug_mask, module_mask, file, line, func, NULL, NULL,
                           "%s", buffer + amt_written1);  //write to debug log

  //if(mask & EL_FILE)
   // ns_log_event_write(severity, buffer, tot_write);                         //write to event log
  /* Here first attribute should be host name, while logging monitor event we assume that first attribute of
   * event log should be host name*/
  NSDL3_MON(NULL, NULL, "host = %s", host);
  NS_EL_3_ATTR(event_id, -1, -1, 1, severity, host, gdf, vector_name,
                                    "%s", buffer + amt_written1);
  open_monitor_log_file();
  if((amt_written1 + amt_written) >= MAX_EVENT_LOG_BUF_SIZE)
  {
    if(write(monitor_log_fd, buffer, MAX_EVENT_LOG_BUF_SIZE) <0)  //write to monitor.log
     {
       NS_EXIT (-1, "Can not write to monitor.log with monitor fd = %d Error is %s", monitor_log_fd, strerror(errno));
     }
  }
  else
  {
    if(write(monitor_log_fd, buffer, amt_written1 + amt_written) <0)  //write to monitor.log
    {
      NS_EXIT (-1, "Can not write to monitor.log with monitor fd = %d Error is %s", monitor_log_fd, strerror(errno));
    }
  }
}



//called from ns_dynamic_vector_monitor.c for DYNAMIC_VECTOR_MONITOR
void ns_dynamic_vector_monitor_log(int mask, int debug_mask, int module_mask,
				   char *file, int line, char *func, 
				   DynamicVectorMonitorInfo *dynamic_vector_monitor_ptr,
				   unsigned int event_id, int severity, char *format, ...) {
  char buffer[MAX_EVENT_LOG_BUF_SIZE + 1 ]="\0";
  char host[1024]="\0";
  char gdf[1024]="\0";
  int amt_written, amt_written1;
  va_list ap;
  char time_buf[100];

  amt_written = amt_written1 = 0;
 
  //may possible mon_name dont have path ('/'), 

  if(rindex(dynamic_vector_monitor_ptr->gdf_name, '/'))
    strcpy(gdf, rindex(dynamic_vector_monitor_ptr->gdf_name, '/') + 1);
  else
    strcpy(gdf, dynamic_vector_monitor_ptr->gdf_name);

  if(dynamic_vector_monitor_ptr->access == RUN_LOCAL_ACCESS)
    strcpy(host, dynamic_vector_monitor_ptr->cs_ip);
  else
    strcpy(host, dynamic_vector_monitor_ptr->rem_ip); 

  if(!strcmp(host, "NS")) 
    strcpy(host, g_cavinfo.NSAdminIP); 
  else if(!strcmp(host, "NO"))
    strcpy(host, g_cavinfo.SRAdminIP);

  /*In case of NS monitoring itself, host name should be NSAppliance*/
  set_host(host, host);
  
  amt_written1 = sprintf(buffer, "\n%s|%s|Dynamic Vector Monitor|%s:%s|%s|%d|",
			         nslb_get_cur_date_time(time_buf, 1), host, gdf,
				 dynamic_vector_monitor_ptr->monitor_name, "-", dynamic_vector_monitor_ptr->fd);
  va_start (ap, format);
  amt_written = vsnprintf(buffer + amt_written1, MAX_EVENT_LOG_BUF_SIZE - amt_written1, format, ap);
  va_end(ap);

  if(amt_written >= MAX_EVENT_LOG_BUF_SIZE - amt_written1)
    amt_written = MAX_EVENT_LOG_BUF_SIZE - amt_written1 - 1;

  buffer[MAX_EVENT_LOG_BUF_SIZE] = 0;

  if(mask & EL_CONSOLE)
    fprintf(stderr, "%s\n", buffer + amt_written1);       //write to console

  if(mask & EL_DEBUG)
     debug_log(debug_mask, module_mask, file, line, func, NULL, NULL,
                           "%s", buffer + amt_written1);  //write to debug log

  //if(mask & EL_FILE)
    //ns_log_event_write(severity, buffer, tot_write);                         //write to event log
  /* Here first attribute should be host name, while logging monitor event we assume that first attribute of
   * event log should be host name*/
  NSDL3_MON(NULL, NULL, "host = %s", host);
  NS_EL_3_ATTR(event_id, -1, -1, 1, severity, host, gdf, dynamic_vector_monitor_ptr->monitor_name,
                                    "%s", buffer + amt_written1);
  
  open_monitor_log_file();
  if(write(monitor_log_fd, buffer, amt_written1 + amt_written) <0)  //write to monitor.log
  {
    NS_EXIT (-1, "Can not write to monitor.log with monitor fd = %d Error is %s", monitor_log_fd, nslb_strerror(errno));
  }
}

//called ns_pre_test_check.c for CHECK_MONITOR
void ns_check_monitor_log(int mask, int debug_mask, int module_mask, char *file,
			  int line, char *func, CheckMonitorInfo *check_monitor_ptr,
			  unsigned int event_id, int severity, char *format, ...) {

  char buffer[MAX_EVENT_LOG_BUF_SIZE + 1 ]="\0";
  char host[1024]="\0";
  char monitor_type[1024]="\0";
  char mon_name[1024]="\0";
  int amt_written, amt_written1;
  va_list ap;
  char time_buf[100];

  amt_written = amt_written1 = 0;
 
  if ( check_monitor_ptr->monitor_type == CHECK_MON_IS_CHECK_MONITOR )
   strcpy(monitor_type, "Check Monitor");
  else if ( check_monitor_ptr->monitor_type == CHECK_MON_IS_SERVER_SIGNATURE )
   strcpy(monitor_type, "Server Signature");
  else if ( check_monitor_ptr->monitor_type == CHECK_MON_IS_BATCH_JOB )
   strcpy(monitor_type, "Batch Job");
  else
   strcpy(monitor_type, "NA");

  //may possible pgm_path dont have path ('/'), 
  if(rindex(check_monitor_ptr->check_monitor_name, '/'))
    strcpy(mon_name, rindex(check_monitor_ptr->check_monitor_name, '/') + 1);
  else
    strcpy(mon_name, check_monitor_ptr->check_monitor_name);

  if(check_monitor_ptr->access == RUN_LOCAL_ACCESS)
    strcpy(host, check_monitor_ptr->cs_ip);
  else
    strcpy(host, check_monitor_ptr->rem_ip); 

  if(!strcmp(host, "NS")) 
    strcpy(host, g_cavinfo.NSAdminIP); 
  else if(!strcmp(host, "NO"))
    strcpy(host, g_cavinfo.SRAdminIP);

  /*In case of NS monitoring itself, host name should be NSAppliance*/
  set_host(host, host);

  amt_written1 = sprintf(buffer, "\n%s|%s|%s|%s|%s|%d|", nslb_get_cur_date_time(time_buf, 1), host,
                 monitor_type, mon_name, "-", check_monitor_ptr->fd);
  va_start (ap, format);
  amt_written = vsnprintf(buffer + amt_written1, MAX_EVENT_LOG_BUF_SIZE - amt_written1, format, ap);
  va_end(ap);
  buffer[MAX_EVENT_LOG_BUF_SIZE] = 0;

  if(mask & EL_CONSOLE)
    fprintf(stderr, "%s\n", buffer + amt_written1);       //write to console

  if(mask & EL_DEBUG)
     debug_log(debug_mask, module_mask, file, line, func, NULL, NULL,
                           "%s", buffer + amt_written1);  //write to debug log

  //if(mask & EL_FILE)
    //ns_log_event_write(severity, buffer, tot_write);                         //write to event log
  /* Here first attribute should be host name, while logging monitor event we assume that first attribute of
   * event log should be host name*/ 
  NSDL3_MON(NULL, NULL, "host = %s", host);
  NS_EL_3_ATTR(event_id, -1, -1, 1, severity, host, monitor_type, mon_name,
                                    "%s", buffer + amt_written1);
  
  open_monitor_log_file();
  if(write(monitor_log_fd, buffer, amt_written1 + amt_written) <0)  //write to monitor.log
  {
    NS_EXIT (-1, "Can not write to monitor.log with monitor fd = %d Error is %s", monitor_log_fd, nslb_strerror(errno));
  }
}


//Monitor.log entry for user monitor
void ns_um_monitor_log(int mask, int debug_mask, int module_mask, char *file, int line,
		       char *func, UM_info *um_ptr, unsigned int event_id, int severity, char *format, ...) {

  char buffer[MAX_EVENT_LOG_BUF_SIZE + 1 ]="\0";
  char host[1024]="\0";
  char gdf[1024]="\0";
  int amt_written, amt_written1, fd = -1;
  va_list ap;
  char time_buf[100];

  amt_written = amt_written1 = 0;
 
  //may possible mon_name dont have path ('/'), 

  if(um_ptr != NULL)
  {
    if(rindex(um_ptr->gdf_name, '/'))
      strcpy(gdf, rindex(um_ptr->gdf_name, '/') + 1);
    else
      strcpy(gdf, um_ptr->gdf_name);

  }
  else
  {
    strcpy(gdf, "NA");
    strcpy(host, "NA");
  }

  if(!strcmp(host, "NS")) 
    strcpy(host, g_cavinfo.NSAdminIP); 
  else if(!strcmp(host, "NO"))
    strcpy(host, g_cavinfo.SRAdminIP);

  /*In case of NS monitoring itself, host name should be NSAppliance*/
  set_host(host, host);
  
  amt_written1 = sprintf(buffer, "\n%s|%s|User Monitor|%s|%s|%d|", nslb_get_cur_date_time(time_buf, 1), host, gdf, "-", fd);
  va_start (ap, format);
  amt_written = vsnprintf(buffer + amt_written1, MAX_EVENT_LOG_BUF_SIZE - amt_written1, format, ap);
  va_end(ap);
  buffer[MAX_EVENT_LOG_BUF_SIZE] = 0;

  if(mask & EL_CONSOLE)
    fprintf(stderr, "%s\n", buffer + amt_written1);       //write to console

  if(mask & EL_DEBUG)
     debug_log(debug_mask, module_mask, file, line, func, NULL, NULL,
                           "%s", buffer + amt_written1);  //write to debug log

  //if(mask & EL_FILE)
   // ns_log_event_write(severity, buffer, tot_write);                         //write to event log
  /* Here first attribute should be host name, while logging monitor event we assume that first attribute of
   * event log should be host name*/
  NSDL3_MON(NULL, NULL, "host = %s", host);
  
  open_monitor_log_file();
  if(write(monitor_log_fd, buffer, amt_written1 + amt_written) <0)  //write to monitor.log
  { 
    NS_EXIT (-1, "Can not write to monitor.log with monitor fd = %d Error is %s", monitor_log_fd, nslb_strerror(errno));
  }
}


//this method need to called when data is recieved as we are expecting an event may come
int ns_cm_monitor_event_command(CM_info *cus_ptr, char *buffer, char *file, int line_num, char *func) {
   int severity=2;
   char *event = extract_header_from_event(buffer, &severity);

   if(event) {
     MLTL1(EL_F, 0, 0, _FLN_, cus_ptr, EID_DATAMON_API, 
                                   severity, "%s", event);
     return 0;
   } else {
     return 1;
   }
}

void ns_check_monitor_event_command(CheckMonitorInfo *check_monitor_ptr, char *buffer, char *file, int line_num, char *func) {

   int severity;
   char *event = extract_header_from_event(buffer, &severity);

   if(event) {
     ns_check_monitor_log(EL_F, 0, 0, _FLN_, check_monitor_ptr, EID_CHKMON_API,
                                      severity, "%s", event);
   }
}

void ns_dynamic_vector_monitor_event_command(DynamicVectorMonitorInfo *dynamic_vector_monitor_ptr,
					     char *buffer, char *file, int line_num, char *func) {
   int severity;
   NSDL3_MON(NULL, NULL, "Method Called");

   char *event = extract_header_from_event(buffer, &severity);
   if(event) {
     ns_dynamic_vector_monitor_log(EL_F, 0, 0, _FLN_, dynamic_vector_monitor_ptr, EID_DATAMON_API,
                                        severity, "%s", event);
   }
}
