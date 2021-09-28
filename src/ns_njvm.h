#ifndef _ns_njvm_h
#define _ns_njvm_h

#include "ns_schedule_phases_parse.h" 
#include "ns_standard_monitors.h"
#include "ns_script_parse.h"

extern int njvm_total_busy_thread; // no of bsy thread
extern int njvm_total_free_thread; // No of free thread 

extern int  kw_set_njvm_system_class_path(char *buf, char *err_msg, int runtime_flag);
extern int  kw_set_njvm_class_path(char *buf, char *err_msg, int runtime_flag);
extern int  kw_set_njvm_java_home_path(char *buf, char *err_msg, int runtime_flag);
extern int kw_set_njvm_std_args(char *buf, char *err_msg, int runtime_flag);
extern int  kw_set_njvm_custom_args(char *buf, char *err_msg, int runtime_flag);
extern int  kw_set_njvm_conn_timeout(char *buf, unsigned long* global_set, char *err_msg, int runtime_flag);
extern int  kw_set_njvm_msg_timeout(char *buf, unsigned long* global_set, char *err_msg, int runtime_flag);
extern int  kw_set_njvm_simulator_mode(char *buf);
extern int kw_set_njvm_thread_pool(char *buf, char *err_msg, int runtime_flag);
extern int  kw_set_njvm_con_mode(char *buf, char* global_set);

extern int create_classes_for_java_type_script( int sess_idx, char* script_filepath, char* flow_file, int file_count, FlowFileList_st *script_filelist);
//extern int parse_script_type_java(int sess_idx, char *script_filepath);
extern void make_scripts_jar();
extern void get_java_home_path();
extern void njvm_add_auto_monitors();
extern void njvm_free_thread(Msg_com_con *node);
extern Msg_com_con *njvm_get_thread();
extern int is_java_type_script();
extern inline void check_and_init_njvm();
extern void njvm_accept_thrd_con(int event_fd, int epoll_add_flag);
inline void send_msg_from_nvm_to_parent(int opcode, int th_flag);
extern void divide_thrd_among_nvm();
#endif

