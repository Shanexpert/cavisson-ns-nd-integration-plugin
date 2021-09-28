#ifndef WAIT_FOREVER_H
#define WAIT_FOREVER_H

#include "ns_msg_com_util.h"
extern avgtime* wait_forever(int num_children, cavgtime **c_end_results);
//extern void kill_children();
extern void init_avgtime (avgtime *msg, int);
extern cavgtime **g_cur_finished;
extern cavgtime **g_next_finished;
extern void init_cavgtime (cavgtime *msg, int dont_reset_all);
//extern void inline copy_progress_data (int first, avgtime *total_avg, avgtime *msg, cavgtime* save);
//extern void add_finished(cavgtime *msg, avgtime* total_avg);
//extern void inline copy_end_data (int first, avgtime *end_avg, avgtime *msg);
//extern void kill_all_children(void)
#define CPU_HEALTH_IDX 0

// Parent states
#define NS_PARENT_ST_INIT         0
#define NS_PARENT_ST_TEST_STARTED 1
#define NS_PARENT_ST_TEST_OVER    2

// RTC states
#define RTC_START_STATE           0
#define RESET_RTC_STATE           1
#define RTC_PAUSE_STATE           2
#define RTC_RESUME_STATE          3

#define CONTINUE_WITH_STARTED_GENERATOR(gen) {\
  if(generator_entry[gen].flags & IS_GEN_INACTIVE) \
   continue; \
}

#define CONTROL_MODE 0 //control connection to child
#define DATA_MODE 1   //data connection to child

extern int master_fd;
extern int ns_parent_state; //0: init, 1= started, 2 = ending
extern int udp_fd;  /* this fd is the fd that the parent listens to for messages from the child (e.g. progress reports, finish reports, etc..) */
extern int gui_fd;
extern int gui_fd2;
extern int run_mode;
extern FILE *gui_fp;
extern FILE *rtg_fp;
extern  FILE *rfp;
extern  FILE *srfp;
extern int loader_opcode; /* MASTER or CLIENT */
extern char send_events_to_master;
extern int g_collect_no_eth_data; //default is 0 - means collect
extern int total_udpport_entries;
extern pid_t cpu_mon_pid;
extern parent_msg *msg;
extern int got_start_phase;
extern int g_parent_idx;
extern int g_generator_idx;
extern int deliver_report_done;

extern void set_scheduler_start_flag();
typedef struct {
  int fd_num;
  int samples_awaited;
  int value_rcv;
  short cmp;
} udp_ports;

// extern udp_ports* udp_array;
extern void ns_handle_sigusr1( int sig );
extern void check_before_sending_nxt_phase(parent_msg *msg);
//extern int num_active;
extern inline void create_rtg_file_data_file_send_start_msg_to_server();
//extern int total_killed_gen;
//extern int total_killed_generator;
extern int total_killed_nvms;
extern int num_gen_expected;

//Added for RTC
extern void rtc_log_on_epoll_timeout(int epoll_timeout);
extern int open_connect_to_gui_server();

//to be used in ns_dynamic_avg.c
extern int parent_realloc_cavg(int size, int old_size, int type);
//for Avg time 
extern void malloc_avgtime();
extern inline void process_default_case_msg(Msg_com_con *mccptr, int th_flag);
extern avgtime **g_cur_avg, **g_next_avg, **g_dest_avg;
extern void reset_global_avg(int gen_id);
extern inline avgtime* handle_all_finish_report_msg(avgtime *end_avg);
extern inline void init_vars_for_each_test(int num_children); 
extern void free_msg_com_con(Msg_com_con *ptr, int num);
//extern void mark_gen_inactive_and_remove_from_list(Msg_com_con *mccptr, int gen_idx, int kill_by_stop_api, int len, char *buff);
extern inline int update_vars_to_continue_ctrl_test(int gen_id);
extern void mark_gen_inactive_and_remove_from_list(Msg_com_con *mccptr, int gen_idx, int kill_by_stop_api, int len, char *buff);
extern void check_before_sending_nxt_phase_only_from_controller(int phase_idx, int grp_idx, int child_idx);
extern void process_end_test_run_msg(Msg_com_con *mccptr, EndTestRunMsg *end_test_run_msg, int conn_mode);

#endif
