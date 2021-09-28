#ifndef NS_RUNTIME_CHANGES_USERS_H
#define NS_RUNTIME_CHANGES_USERS_H

#define RUNTIME_ERROR   -1
#define RUNTIME_SUCCESS  0
#define RUNTIME_FLAG     1
#define MAX_NVMS         254
#define INCREASE_USERS_OR_SESSIONS 1
#define DECREASE_USERS_OR_SESSIONS 2

#define RUNTIME_DECREASE_QUANTITY_MODE_0   0
#define RUNTIME_DECREASE_QUANTITY_MODE_1   1
#define RUNTIME_DECREASE_QUANTITY_MODE_2   2


#define REMOVE_RAMPED_UP_VUSERS         0
#define REMOVE_NOT_RAMPED_UP_VUSERS     1

#define RTC_QTY_BUFFER_SIZE 64000

#define USER_RTC         0
#define SESSION_RTC      1
#define USER_SESSION_RTC 2


extern int phase_end_msg_flag;
extern int kw_set_runtime_users_or_sessions(char *buf, char *err_msg, int runtime_id, int first_time);
extern int kw_set_runtime_change_quantity_settings(char *buf, char *err);
extern int parse_keyword_runtime_quantity(char *buf, char *err_msg, int *grp_idx, int *num_users_or_sess, int *runtime_operation, int runtime_id, int *runtime_index, int *runtime_per_user_fetches);
extern void runtime_get_nvm_quantity(int grp_idx, int ramped_up_users_or_sess, int available_qty_to_remove[]);
extern int kw_set_runtime_phases(char *buf, char *err_msg, int runtime_flag);
extern void runtime_schedule_malloc_and_memset();
extern void process_rtc_ph_complete(parent_msg *msg);
extern int find_available_rtc_id();
extern void process_quantity_rtc(int fd, char *msg);
extern void process_rtc_quantity_resume_schedule(User_trace *msg);
//extern void sending_pause_for_rtc_msg();
extern int make_data_for_gen(char *err_msg);
extern int find_grp_idx_for_rtc(char *grp, char *err_msg);
extern int parse_qty_buff_for_generator(char *line, int runtime_id, int is_rtc_for_qty);
extern int send_qty_pause_msg_to_all_clients();
extern int send_qty_resume_msg_to_all_clients(User_trace *pause_msg);
extern void process_schedule_rtc(int fd, char *msg);
extern void rtc_quantity_pause_resume(int opcode, u_ns_ts_t now);
extern void rtc_schedule_pause_resume(int opcode, parent_child *recv_msg, u_ns_ts_t now);
extern void dump_rtc_log_in_runtime_all_conf_file(FILE *runtime_all_fp);
extern int kw_set_runtime_qty_timeout(char *buf, char *err_msg);
extern int rtc_resume_from_pause();
extern int process_nc_rtc_applied_message(int gen_fd, User_trace *msg);
extern void process_nc_schedule_detail_response_msg(int gen_fd, User_trace *msg);
extern void send_nc_apply_rtc_msg_to_all_gen(int opcode, char *msg, int flag, int runtime_id);
extern void handle_rtc_child_failed(int child_id);
extern void check_and_send_next_phase_msg();
extern int is_process_still_active(int grp_idx, int proc_index);
extern int runtime_distribute_fetches(char nvm_ids[], int num_fetches, char *err_msg);
extern void runtime_get_process_cnt_serving_grp(char nvm_ids[], int *nvm_count, int grp_idx);
extern int runtime_distribute_fetchs_over_nvms_and_grps(char nvm_ids[], int fetches, int nvm_count, int grp_idx, char *err_msg);

#endif //NS_RUNTIME_CHANGES_USERS_H
