/******************************************************************
 * Name    : ns_debug_trace.h
 * Author  : Archana
 * Purpose : This file contains declaration of macros, methods related to
             parsing keyword and to create debug_trace.log file
 * Note:
 * Modification History:
 * 19/11/08 - Initial Version
*****************************************************************/

#ifndef __NS_DEBUG_TRACE_H__
#define __NS_DEBUG_TRACE_H__ 


#include "netstorm.h"

#define _FLN_  __FILE__, __LINE__, (char *)__FUNCTION__
#define MAX_HD_FT_SIZE 256  //header footer length

extern void ns_debug_trace_log(int indent_level, VUser *vptr, connection *cptr, int debug_mask, unsigned long long  module_mask, char *file, int line, char *fname, char *format, ...);

extern void ns_script_execution_log(int indent_level, VUser *vptr, connection *cptr, int debug_mask, unsigned long long  module_mask, char *file, int line, char *fname, char *format, ...);
extern int g_parent_idx;
#ifdef NS_DEBUG_ON
  #define NS_DT1(vptr, cptr, debug_mask, module_mask, ...) \
  {\
     ns_debug_trace_log (1, vptr, cptr, debug_mask, module_mask, _FLN_, __VA_ARGS__);\
     if(g_parent_idx == -1) debug_log(debug_mask, module_mask, _FLN_, vptr, cptr, __VA_ARGS__);\
  }
  #define NS_DT2(vptr, cptr, debug_mask, module_mask, ...) \
  {\
     ns_debug_trace_log (2, vptr, cptr, debug_mask, module_mask, _FLN_, __VA_ARGS__);\
     if(g_parent_idx == -1) debug_log(debug_mask, module_mask, _FLN_, vptr, cptr, __VA_ARGS__);\
  }
  #define NS_DT3(vptr, cptr, debug_mask, module_mask, ...) \
  {\
     ns_debug_trace_log (3, vptr, cptr, debug_mask, module_mask, _FLN_, __VA_ARGS__);\
     if(g_parent_idx == -1) debug_log(debug_mask, module_mask, _FLN_, vptr, cptr, __VA_ARGS__);\
  }
  #define NS_DT4(vptr, cptr, debug_mask, module_mask, ...) \
  {\
     ns_debug_trace_log (4, vptr, cptr, debug_mask, module_mask, _FLN_, __VA_ARGS__);\
     if(g_parent_idx == -1) debug_log(debug_mask, module_mask, _FLN_, vptr, cptr, __VA_ARGS__);\
  }
#else
  #define NS_DT1(vptr, cptr, ...)
  #define NS_DT2(vptr, cptr, ...)
  #define NS_DT3(vptr, cptr, ...)
  #define NS_DT4(vptr, cptr, ...)
#endif

#define NS_SEL(vptr, cptr, debug_mask, module_mask, ...) \
{\
   ns_script_execution_log(1, vptr, cptr, debug_mask, module_mask, _FLN_, __VA_ARGS__);\
   debug_log(debug_mask, module_mask, _FLN_, vptr, cptr, __VA_ARGS__);\
}

#define NS_SET_WRITE_PTR(write_ptr, write_idx, free_space) \
{ \
  NSDL4_API(NULL, NULL, "write_ptr = %p, write_idx = %d, free_space = %d", write_ptr, write_idx, free_space); \
  write_ptr += write_idx; \
  free_space -= write_idx; \
}

#ifndef CAV_MAIN
extern int debug_trace_log_value;
#else
extern __thread int debug_trace_log_value;
#endif
extern int kw_set_debug_trace(char *text, char *keyword, char *buf);
extern void trace_log_init();
extern void trace_log_end();
extern void script_execution_log_init();
extern void script_execution_log_end();

extern int script_execution_log_fd;
#endif

//End of file
