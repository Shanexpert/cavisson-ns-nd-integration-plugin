/******************************************************************
 * Name    :    ns_child_msg_com.c
 * Purpose :    This file contains methods related to message
                communication between child to parent
 * Note    :
 * Author  :    Archana
 * Intial version date:    08/04/08
 * Last modification date: 08/04/08
*****************************************************************/

#ifndef __NS_CHILD_MSG_COM__
#define __NS_CHILD_MSG_COM__

#define SYS_ERROR 0
#define USE_ONCE_ERROR 1
#define MEMPOOL_EXHAUST 9 

#define RUNNING_VUSERS gNumVuserActive+gNumVuserThinking+gNumVuserWaiting+gNumVuserSPWaiting+gNumVuserBlocked+gNumVuserPaused

extern Msg_com_con g_child_msg_com_con;
extern Msg_com_con g_dh_child_msg_com_con;
extern Msg_com_con g_el_subproc_msg_com_con;
extern Msg_com_con g_dh_el_subproc_msg_com_con;

extern Msg_com_con g_nvm_listen_msg_con;
extern Msg_com_con *g_jmeter_listen_msg_con;
extern unsigned short g_nvm_listen_port;

extern void fill_and_send_child_to_parent_msg(char *str_opcode, parent_child *msg, int opcode);
extern void send_ramp_up_done_msg_for_sessions();
extern void send_ramp_up_msg_for_sessions();
extern void send_ramp_up_done_msg(Schedule *schedule_ptr);
extern void send_ramp_up_msg(ClientData cd, u_ns_ts_t now);
//extern void end_test_run( void );
//extern void mark_finish(u_ns_ts_t now);
extern void progress_report( ClientData client_data, u_ns_ts_t now );
extern void start_collecting_reports(u_ns_ts_t now);
extern void warmup_done();
extern void do_warmup(u_ns_ts_t now);
extern void send_finish_report();
extern void send_ramp_down_msg();
extern void send_ramp_down_done_msg();
extern void send_ramp_down_sess_msg();
extern void send_ramp_down_done_sess_msg();
extern void send_phase_complete(Schedule *schedule_ptr);
extern void process_schedule_msg_from_parent(parent_child *msg);
extern inline void send_child_to_parent_msg(char *str_opcode, char *msg, int size, int thread_flag);
//extern void end_test_run_ex (char *msg, int status);
extern void end_test_run_int (char *msg, int status);
extern inline void fill_and_send_child_to_parent_msg(char *str_opcode, parent_child *msg, int opcode);

extern void check_if_need_to_resize_child_pdf_memory();
extern void process_schedule(int phase_idx,int phase_type, int grp_idx);
#endif
