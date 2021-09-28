#ifndef _NS_EXIT_H
#define _NS_EXIT_H
#include "nslb_util.h"
#include "ns_trace_level.h"
#include "ns_test_init_stat.h"
#include "ns_tls_utils.h"

#define SYS_ERROR 0
#define USE_ONCE_ERROR 1
#define MEMPOOL_EXHAUST 9 

#ifdef NS_EXIT_VAR  
int testidx = 0; 
long long g_partition_idx = 0;
int ns_event_fd = -1;
char g_test_init_stage_id = -1;
__thread NSTLS g_tls = {0};
__thread int g_monitor_status; 
void end_stage(short stage_idx, char stagestatus, char *format, ...)
{
}
void end_test_run_int(char *msg , int status)
{
}

void ns_process_cav_memory_map()
{
}
 
#else
extern int testidx;
extern long long g_partition_idx;
extern int ns_event_fd;
extern char g_test_init_stage_id;
extern __thread NSTLS g_tls;
extern void end_test_run_int(char *msg , int status);
extern void ns_process_cav_memory_map();
extern void log_script_parse_error(int do_exit, char *err_msg_code, char *file, int line, char *fname, char *line_buf, char *format, ...);
extern __thread int g_monitor_status; 
#endif


#ifndef CAV_MAIN
#define NS_EXIT(exit_status, ...) {\
  ns_process_cav_memory_map(); \
  \
  end_stage(g_test_init_stage_id, TIS_ERROR, __VA_ARGS__); \
  if(testidx && g_partition_idx)\
  {\
    NSLB_FEXIT_LOG_STACK_TRACE(exit_status, ns_event_fd, g_tls.buffer, g_tls.buffer_size, __VA_ARGS__);\
  }\
  else\
    NSLB_FEXIT_LOG_STACK_TRACE(exit_status, ns_event_fd, NULL, 0, __VA_ARGS__);\
}
#else
#define NS_EXIT(exit_status, ...) {\
    NSLB_FEXIT_LOG_STACK_TRACE(exit_status, ns_event_fd, g_tls.buffer, g_tls.buffer_size, __VA_ARGS__);\
    if(ISCALLER_NVM_THREAD)\
       g_monitor_status = -1;\
    else \
       exit(exit_status);\
}
#endif
#define NS_EXIT_EX(exit_status, stage_idx, ...) {\
  ns_process_cav_memory_map(); \
  \
  if(testidx && g_partition_idx)\
  {\
    end_stage(stage_idx, TIS_ERROR, __VA_ARGS__); \
    NSLB_FEXIT_LOG_STACK_TRACE(exit_status, ns_event_fd, g_tls.buffer, g_tls.buffer_size, __VA_ARGS__);\
  }\
  else\
    NSLB_FEXIT_LOG_STACK_TRACE(exit_status, ns_event_fd, NULL, 0, __VA_ARGS__);\
}

#define end_test_run_ex(msg, status) {\
  NSTL1(NULL,NULL,"end_test_run called");\
  end_test_run_int(msg,status); \
}

#define end_test_run() {\
  NSTL1(NULL,NULL,"end_test_run called");\
  end_test_run_int("System Error", SYS_ERROR);\
}
#define NS_RUNTIME_RETURN_OR_INIT_ERROR(is_runtime, err_buff, ...)\
{\
  if(is_runtime)\
  {\
    nslb_vsnprintf(err_buff, 1024, __VA_ARGS__);\
    NSTL1(NULL, NULL, "%s", err_buff);\
    return -1; \
  }\
  NS_EXIT(-1, __VA_ARGS__);\
}
/**************************************************************
SCRIPT_PARSE_ERROR_EXIT_EX - Macro to log script error and exit
SCRIPT_PARSE_NO_RETURN_EX  - Macro to log script error only
**************************************************************/
#define SCRIPT_PARSE_ERROR_EXIT_EX(line_buf, err_msg_code, ...)\
{\
  log_script_parse_error(1, err_msg_code, _FLN_, line_buf, __VA_ARGS__);   \
}

#define SCRIPT_PARSE_NO_RETURN_EX(line_buf, err_msg_code, ...)\
{\
  log_script_parse_error(0, err_msg_code, _FLN_, line_buf, __VA_ARGS__);   \
}

//From Release 4.3.0 B14, SCRIPT_PARSE_ERROR will always do test
#define SCRIPT_PARSE_ERROR(line_buf, ...)\
{\
  SCRIPT_PARSE_ERROR_EXIT(line_buf, __VA_ARGS__);\
}

#define SCRIPT_PARSE_ERROR_EXIT(line_buf, ...)  \
{ \
  log_script_parse_error(1, CAV_ERR_1012001, _FLN_, line_buf, __VA_ARGS__);   \
}

#define SCRIPT_PARSE_NO_RETURN(line_buf, ...)  \
{ \
  log_script_parse_error(0, CAV_ERR_1012001, _FLN_, line_buf, __VA_ARGS__);   \
}

#endif
