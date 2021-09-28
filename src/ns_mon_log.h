/******************************************************************
 * Name    : ns_monitor_log.h 
 * Author  : Arun Nishad
 * Purpose : 
            
 * Note:
 * Modification History:
 * 
*****************************************************************/
#include "ns_user_monitor.h"
#include "ns_check_monitor.h"
#include "ns_custom_monitor.h"
#ifndef __NS_MONITOR_LOG_H
#define __NS_MONITOR_LOG_H 


extern int monitor_log_fd;

extern void ns_monitor_log(int mask, int debug_mask, int module_mask, char *file, int line_num,
				     char *func, char *host, char *mon_name, int fd,
				     unsigned int event_id, int severity, char *format, ...);
extern void ns_cm_monitor_log(int mask, int debug_mask, int module_mask, char *file, int line_num,
					char *func, CM_info *cm_ptr,
					unsigned int event_id, int severity, char *format, ...);
extern void ns_um_monitor_log(int mask, int debug_mask, int module_mask, char *file, int line_num,
					char *func, UM_info *um_ptr,
					unsigned int event_id, int severity, char *format, ...);
extern void ns_dynamic_vector_monitor_log(int mask, int debug_mask, int module_mask, char *file, int line,
						    char *func, DynamicVectorMonitorInfo *dynamic_vector_monitor_ptr,
						    unsigned int event_id, int severity, char *format, ...);
extern void ns_check_monitor_log(int mask, int debug_mask, int module_mask, char *file,
				           int line_num, char *func, CheckMonitorInfo *check_monitor_ptr,
					   unsigned int event_id, int severity, char *format, ...);
extern char *extract_header_from_event(char *event, int *severity);
extern int ns_cm_monitor_event_command(CM_info *cus_ptr, char *buffer, char *file,
						         int line_num, char *func);
extern void ns_check_monitor_event_command(CheckMonitorInfo *check_monitor_ptr, char *buffer,
					   char *file, int line_num, char *func);
extern void ns_dynamic_vector_monitor_event_command(DynamicVectorMonitorInfo *dynamic_vector_monitor_ptr,
					   char *buffer, char *file, int line_num, char *func);

#define MLTL1(mask,debug_mask,module_mask,_FLN_,cm_ptr,event_id,severity,...) if(mon_log_tracing_level & 0x000000FF) ns_cm_monitor_log(mask,debug_mask,module_mask,_FLN_,cm_ptr,event_id,severity,__VA_ARGS__) 

#define MLTL2(mask,debug_mask,module_mask,_FLN_,cm_ptr,event_id,severity,...) if(mon_log_tracing_level & 0x0000FF00) ns_cm_monitor_log(mask,debug_mask,module_mask,_FLN_,cm_ptr,event_id,severity,__VA_ARGS__) 

#define MLTL3(mask,debug_mask,module_mask,_FLN_,cm_ptr,event_id,severity,...) if(mon_log_tracing_level & 0x00FF0000) ns_cm_monitor_log(mask,debug_mask,module_mask,_FLN_,cm_ptr,event_id,severity,__VA_ARGS__) 

#define MLTL4(mask,debug_mask,module_mask,_FLN_,cm_ptr,event_id,severity,...) if(mon_log_tracing_level & 0xFF000000) ns_cm_monitor_log(mask,debug_mask,module_mask,_FLN_,cm_ptr,event_id,severity,__VA_ARGS__)
#endif
//End of file
