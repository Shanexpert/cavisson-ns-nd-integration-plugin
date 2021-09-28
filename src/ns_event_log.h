/******************************************************************
 * Name    : ns_event_log.h 
 * Author  : Arun Nishad
 * Purpose : 
            
 * Note:
 * Modification History:
 * 
*****************************************************************/

#ifndef __NS_EVENT_LOG_H
#define __NS_EVENT_LOG_H 

#include <sys/epoll.h>

#include "ns_data_types.h"
#include "ns_msg_def.h"
#include "ns_tls_utils.h"
#include "ns_trace_level.h"

#define UNSIGNED_INT_BUF_LEN 10

#define DO_NOT_LOG_EVENT	 2 
#define LOG_FILTER_BASED_EVENT	 1 
#define LOG_ALL_EVENT		 0 

#define EVENT_CORE    0
#define EVENT_SCRIPT  1
#define EVENT_MONITOR 2
#define EVENT_API     3
#define EVENT_SYNC_POINT 4
#define EVENT_NDCOLLECTOR 5

#define MAX_EVENT_LOG_BUF_SIZE 64000

#define EL_CONSOLE 0x00000001    // console
#define EL_DEBUG   0x00000002    // debug log 
#define EL_FILE    0x00000004    // event log

#define EL_C       1             // Only Console
#define EL_D       2             // Only Debug log
#define EL_F       4             // Only Event log file
#define EL_CD      3             // Console & debug log
#define EL_DF      6             // Debug log & event log
#define EL_CF      5             // Console & event log
#define EL_CDF     7             // Console, debug log & event log

extern int kw_set_event_log(char *text, int runtime_flag, char *err_msg);
extern void ns_log_event_core(int mask, int debug_mask, int module_mask,
                              char *host, char *file_name, int line_num, 
                              char *func, int severity, char *format, ...);

extern char *get_relative_time();
extern char *get_absolute_time();
extern char *get_relative_time_with_ms();
extern void close_event_log_fd();
extern void ns_log_event_write(int log_level, char *buf, int to_write);
extern int convert_string_to_int(char *sever);

//extern void *attributes[32];

// Event Deduplication
//extern shr_logging* el_shr_mem;

//extern shr_logging* initialize_el_memory(int num_proc, int testidx);
extern int nsa_log_mgr_pid;
extern void flush_el_buffers();
//extern void send_siguser_to_event_logger();
extern void kw_set_enable_event_deduplication(char *text, char *value, int flag);

extern void ns_el_1_attr(unsigned int uid, unsigned int sid, unsigned int eid,
                  unsigned char src, unsigned char severity, char* attr1, char *format, ...);

extern void ns_el_2_attr(unsigned int uid, unsigned int sid, unsigned int eid,
                  unsigned char src, unsigned char severity, char* attr1, char *attr2, char *format, ...);

extern void ns_el_3_attr(unsigned int uid, unsigned int sid, unsigned int eid,
                  unsigned char src, unsigned char severity, char* attr1, char *attr2, char *attr3, char *format, ...);

extern void ns_el_4_attr(unsigned int uid, unsigned int sid, unsigned int eid,
                  unsigned char src, unsigned char severity, char* attr1, char *attr2, char *attr3, char *attr4, char *format, ...);

extern void ns_el_5_attr(unsigned int uid, unsigned int sid, unsigned int eid,
                  unsigned char src, unsigned char severity,
                  char* attr1, char *attr2, char *attr3, char *attr4, char *attr5,
                  char *format, ...);
int testidx;
#define NS_EL_1_ATTR(eid, uid, sid, src, severity, attr_1, ...){\
    if((global_settings->filter_mode != DO_NOT_LOG_EVENT) && (severity >= global_settings->event_log) && (testidx > 0)) {\
      if(ISCALLER_PARENT || ISCALLER_PARENT_AFTER_DH ||ISCALLER_NVM || ISCALLER_DATA_HANDLER)\
        ns_el_1_attr(eid, uid, sid, src, severity, attr_1, __VA_ARGS__);\
      else \
        NSTL1(NULL, NULL,  __VA_ARGS__);\
    }\
  }

#define NS_EL_2_ATTR(eid, uid, sid, src, severity, attr_1, attr_2, ...)\
  {\
    if((global_settings->filter_mode != DO_NOT_LOG_EVENT) && (severity >= global_settings->event_log) && (testidx > 0))\
    {\
      if(ISCALLER_PARENT || ISCALLER_PARENT_AFTER_DH || ISCALLER_NVM || ISCALLER_DATA_HANDLER)\
        ns_el_2_attr(eid, uid, sid, src, severity, attr_1, attr_2, __VA_ARGS__);\
      else \
        NSTL1(NULL, NULL,  __VA_ARGS__);\
    }\
  }

#define NS_EL_3_ATTR(eid, uid, sid, src, severity, attr_1, attr_2, attr3, ...){\
    if((global_settings->filter_mode != DO_NOT_LOG_EVENT) && (severity >= global_settings->event_log) && (testidx > 0)) {\
      if(ISCALLER_PARENT || ISCALLER_PARENT_AFTER_DH || ISCALLER_NVM || ISCALLER_DATA_HANDLER)\
        ns_el_3_attr(eid, uid, sid, src, severity, attr_1, attr_2, attr3, __VA_ARGS__);\
      else \
        NSTL1(NULL, NULL,  __VA_ARGS__);\
    }\
  }

#define NS_EL_4_ATTR(eid, uid, sid, src, severity, attr_1, attr_2, attr3, attr4, ...){\
    if((global_settings->filter_mode != DO_NOT_LOG_EVENT) && (severity >= global_settings->event_log) && (testidx > 0)) {\
      if(ISCALLER_PARENT || ISCALLER_PARENT_AFTER_DH || ISCALLER_NVM || ISCALLER_DATA_HANDLER)\
        ns_el_4_attr(eid, uid, sid, src, severity, attr_1, attr_2, attr3, attr4, __VA_ARGS__);\
      else \
        NSTL1(NULL, NULL,  __VA_ARGS__);\
    }\
  }

#define NS_EL_5_ATTR(eid, uid, sid, src, severity, attr_1, attr_2, attr3, attr4, attr5, ...){ \
    if((global_settings->filter_mode != DO_NOT_LOG_EVENT) && (severity >= global_settings->event_log) && (testidx > 0)) {\
      if(ISCALLER_PARENT || ISCALLER_PARENT_AFTER_DH || ISCALLER_NVM || ISCALLER_DATA_HANDLER)\
        ns_el_5_attr(eid, uid, sid, src, severity, attr_1, attr_2, attr3, attr4, attr5, __VA_ARGS__);\
      else \
        NSTL1(NULL, NULL,  __VA_ARGS__);\
    }\
  }

extern void copy_event_def_to_shr_mem();
extern void init_event_logger(int recovery_flag);
extern void init_events();
extern int kw_set_enable_log_mgr(char*, int runtime_flag, char *err_msg);
extern void kw_set_event_definition_file(char*);
extern int chk_nsa_log_mgr();
extern void handle_nsa_log_mgr(struct epoll_event *pfds, void *cptr, int ii, u_ns_ts_t now);
extern void print_core_events(char *function, char *file, char *format, ...);

extern void process_log_mgr_port_change_msg_frm_parent(parent_child* el_msg);
extern int  nsa_log_mgr_recovery();
extern void send_nsa_log_mgr_port_change_msg(int port);
//extern void add_select_el_msg_com_con(int epfd, char* data_ptr, int fd, int event);
#endif

//End of file
