
#ifndef __MONITOR_UTILS_H__
#define __MONITOR_UTILS_H__

#define _FL_  __FILE__, __LINE__

#define _FLN_  __FILE__, __LINE__, (char *)__FUNCTION__

#define NS_CLASSIC_MON_LEVEL0	0
#define NS_CLASSIC_MON_LEVEL1	1
#define NS_CLASSIC_MON_LEVEL2	2
#define NS_CLASSIC_MON_LEVEL3	3

#define NSDL1_CLASSIC_MON(...)  debug_log(NS_CLASSIC_MON_LEVEL0, _FLN_, my_child_id, my_sub_child_id, req_resp_id, __VA_ARGS__)
#define NSDL2_CLASSIC_MON(...)  debug_log(NS_CLASSIC_MON_LEVEL1, _FLN_, my_child_id, my_sub_child_id, req_resp_id, __VA_ARGS__)
#define NSDL3_CLASSIC_MON(...)  debug_log(NS_CLASSIC_MON_LEVEL2, _FLN_, my_child_id, my_sub_child_id, req_resp_id, __VA_ARGS__)
#define NSDL4_CLASSIC_MON(...)  debug_log(NS_CLASSIC_MON_LEVEL3, _FLN_, my_child_id, my_sub_child_id, req_resp_id, __VA_ARGS__)

extern void open_error_log();
extern void open_trace_log();
extern void error_log(int log_level, char *file, int line, char *fname, char *format, ...);
extern void debug_log(int log_level, char *file, int line, char *fname, int my_child_id, int my_sub_child_id, int req_resp_id, char *format, ...);
extern void trace_log(char* buffer);

#endif
