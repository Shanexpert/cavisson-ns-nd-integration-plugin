
#ifndef CHILD_INIT_H
#define CHILD_INIT_H

extern int * my_runprof_table_cumulative;

extern void child_data_init();
extern void child_init();
extern void test_data_init();
extern int parent_fd;
extern int parent_dh_fd;
extern unsigned short parent_port_number;
extern unsigned short parent_data_handler_listen_port;
extern unsigned short event_logger_port_number;
extern int ultimate_max_vusers;

#ifndef CAV_MAIN
extern int user_group_table_size;
extern int user_cookie_table_size;
extern int user_dynamic_vars_table_size;
extern int user_var_table_size;
extern int user_order_table_size;
extern int user_svr_table_size;
extern int usr_table_size; //usr_entry (user server resolve entry) table size
extern int * my_runprof_table;
extern PerProcVgroupTable * my_vgroup_table;
extern VUser* gFreeVuserHead;
extern int gFreeVuserCnt;
extern int gFreeVuserMinCnt;
extern int *scen_group_adjust;
extern int *seq_group_next;
#else
extern __thread int user_group_table_size;
extern __thread int user_cookie_table_size;
extern __thread int user_dynamic_vars_table_size;
extern __thread int user_var_table_size;
extern __thread int user_order_table_size;
extern __thread int user_svr_table_size;
extern __thread int usr_table_size; //usr_entry (user server resolve entry) table size
extern __thread int * my_runprof_table;
extern __thread PerProcVgroupTable * my_vgroup_table;
extern __thread VUser* gFreeVuserHead;
extern __thread int gFreeVuserCnt;
extern __thread int gFreeVuserMinCnt;
extern __thread int *scen_group_adjust;
extern __thread int *seq_group_next;
#endif
#ifdef RMI_MODE
extern int user_byte_vars_table_size;
#endif
extern timer_type* ramp_tmr;
extern timer_type* end_tmr;
extern timer_type* progress_tmr;
extern int num_connections;
extern int smtp_num_connections;
extern int pop3_num_connections;
extern int ftp_num_connections;
extern int dns_num_connections;
//extern int max_parallel;

extern int total_badchecksums;
extern int ramping_done;
extern unsigned int rp_handle, sp_handle, up_handle, gen_handle;
#ifndef CAV_MAIN
extern avgtime *average_time; //used bt children to send progress report to parent
#else
extern __thread avgtime *average_time; //used bt children to send progress report to parent
#endif
extern int event_logger_fd;
extern int event_logger_dh_fd;

#endif
