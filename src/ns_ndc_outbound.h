#include "ns_custom_monitor.h"
#include "ns_check_monitor.h"

typedef struct RestartMonList
{
  int mon_id;
  struct RestartMonList *next;
}RestartMonList;

extern RestartMonList *restart_mon_list_head_ptr;

extern int pending_message_buf_size;
extern char *pending_messages_for_ndc_data_conn;
extern void check_and_send_mon_request_to_start_monitor();
extern void make_send_msg_for_monitors(CM_info *cus_mon_ptr, int server_index, JSON_info *json_element_ptr);
extern int make_mon_msg_and_send_to_NDC(int runtime_flag, int send_only_to_ndc, int parent_epoll_done);
extern int write_msg_for_chk_mon(char *buf, int size);
extern int receive_data_from_NDC(int chk_mon_phase);
extern inline void mark_check_monitor_fail_for_outbound(CheckMonitorInfo *check_monitor_ptr, int status, int *num_pre_test_check, int *num_post_test_check);
extern inline void mark_check_monitor_pass_for_outbound(CheckMonitorInfo *check_monitor_ptr, int status, int *num_pre_test_check, int *num_post_test_check);
extern void wait_for_post_check_results_for_outbound();
extern void wait_for_pre_test_chk_for_outbound();
extern int start_nd_ndc_data_conn();
extern int stop_nd_ndc_data_conn();
extern int wait_for_NDC_to_respond(char *buff, int operation);
extern int make_and_send_start_msg_to_ndc_on_data_conn();
extern inline void handle_ndc_data_conn(struct epoll_event *pfds, int i);
extern int handle_ndc_data_connection_recovery(int chk_mon_done);
extern int kw_set_outbound_connection(char *buff);
extern int make_and_send_msg_to_start_monitor(CM_info * cus_mon_ptr);
extern void create_ndc_msg_for_data(char *buff);
extern void handle_reused_entry_for_CM(CM_vector_info *cus_mon_vector_ptr, int i, int server_index);
extern void mark_deleted_in_server_structure(CM_info *cus_mon_ptr, int reason);
extern void add_entry_in_monitor_config(int server_index,int tier_index);
extern int add_entry_in_mon_id_map_table();
extern void make_pending_messages_for_ndc_data_conn(char *buffer, int size);
extern void reset_monitor_config_in_server_list(int tier_idx,int server_idx);
extern void set_index_for_NA_group(CM_info *local_cm_ptr, int grp_num_monitors, int monitor_idx);
extern int make_mon_msg_and_send_to_NDC(int runtime_flag, int is_ndc_restarted, int parent_epoll_done);
extern void send_monitor_request_to_NDC(int server_idx, int ret);
extern void set_index_for_NA_group(CM_info *local_cm_ptr, int grp_num_monitors, int monitor_idx);
extern int add_entry_in_mon_id_map_table();
