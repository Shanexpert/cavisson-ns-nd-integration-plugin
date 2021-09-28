//23/06/2014 Netstorm Have default stack size 16k. So this buffer size should be less than 16k.
//It was creating core dump.
#define MAX_TRACE_LEVEL_BUF_SIZE  8000
#define MAX_GRP_NAME_LEN          1024
#define _FLN_  __FILE__, __LINE__, (char *)__FUNCTION__
//#define NSTL(void_vptr, void_cptr, DM, MD, ...)  ns_trace_level_ex(DM, _FLN_, void_vptr, void_cptr, __VA_ARGS__)
//#define NSTL1(void_vptr, void_cptr, DM, MD, ...)  ns_trace_level_ex(0x000000FF, _FLN_, void_vptr, void_cptr, __VA_ARGS__)
//#define NSTL2(void_vptr, void_cptr, DM, MD, ...)  ns_trace_level_ex(0x0000FF00, _FLN_, void_vptr, void_cptr, __VA_ARGS__)
#define NSTL1(void_vptr, void_cptr, ...)  if(global_settings->ns_trace_level & 0x000000FF) ns_trace_level_ex(0x000000FF, _FLN_, void_vptr, void_cptr, __VA_ARGS__)
#define NSTL2(void_vptr, void_cptr, ...)  if(global_settings->ns_trace_level & 0x0000FF00) ns_trace_level_ex(0x0000FF00, _FLN_, void_vptr, void_cptr, __VA_ARGS__)
#define NSTL3(void_vptr, void_cptr, ...)  if(global_settings->ns_trace_level & 0x00FF0000) ns_trace_level_ex(0x00FF0000, _FLN_, void_vptr, void_cptr, __VA_ARGS__)
#define NSTL4(void_vptr, void_cptr, ...)  if(global_settings->ns_trace_level & 0xFF000000) ns_trace_level_ex(0xFF000000, _FLN_, void_vptr, void_cptr, __VA_ARGS__)

#define NSTL1_OUT(void_vptr, void_cptr, ...)  if(global_settings->ns_trace_level & 0x000000FF) ns_trace_level_ex_out(0x000000FF, _FLN_, void_vptr, void_cptr, __VA_ARGS__)
extern unsigned int max_trace_level_file_size;
extern unsigned int mon_log_tracing_level;
extern int ns_event_fd;
extern int kw_set_ns_trace_level(char *buf, char *err_msg, int runtime_flag);
extern int kw_set_nlm_trace_level(char *buf, char *err_msg, int runtime_flag);
extern int kw_set_nlw_trace_level(char *buf, char *err_msg, int runtime_flag);
extern int kw_set_nsdbu_trace_level(char *buf, char *err_msg, int runtime_flag);
extern int kw_set_nlr_trace_level(char *buf, char *err_msg, int runtime_flag);

extern void ns_trace_level_ex(int trace_level, char *file, int line, char *fname, void *void_vptr, void *void_cptr, char *format, ...);
extern void ns_trace_level_ex_out(int trace_level, char *file, int line, char *fname, void *void_vptr, void *void_cptr, char *format, ...);
