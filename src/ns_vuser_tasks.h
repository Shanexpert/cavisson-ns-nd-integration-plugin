#ifndef NS_VUSER_TASKS_H
#define NS_VUSER_TASKS_H


#define NS_SCRIPT_MODE_0      0
#define NS_SCRIPT_MODE_1      1
#define NS_SCRIPT_MODE_2      2
#define NS_SCRIPT_MODE_3      3

// VUT_NO_TASK: This is used to indicate there is no task on vptr.
// Once taks is executed we set operation to this value
// so that if we are setting new task on same vptr, we do not add 
// same vptr again in the task list. we just change the task to be done

#define VUT_NO_TASK           0 // No task
#define VUT_WEB_URL           1 // Web URL
#define VUT_PAGE_THINK_TIME   2 // Page think time
#define VUT_END_SESSION       3 // End of session
#define VUT_TR069_WAIT_TIME   4 // Wait time for TR069 after inform sesion is complete
#define VUT_CLICK_ACTION      5 // For Click and Script
#define VUT_SYNC_POINT        6 // For sync point
#define VUT_RBU_WEB_URL_END   7 // For RBU (RealBrowserUrl) url 
#define VUT_SWITCH_TO_NVM     8 
#define VUT_WS_SEND           9  //For websocket send api
#define VUT_WS_CLOSE          10 //For websocket close api
#define VUT_JMETER_GET_STATS  11 // To fill jmeter data
#define VUT_SOCKJS_CLOSE      12 //For sockjs close api
#define VUT_XMPP_SEND         13 //For xmpp_send api
#define VUT_XMPP_LOGOUT       14 //For xmpp_close api
//These operations are not actually added as task
#define VUT_START_SESSION     15
#define VUT_USER_CONTEXT      16
#define VUT_SESSION_PACING    17
#define VUT_EPOLL_WAIT        18
#define VUT_SOCKET_SEND       19
#define VUT_SOCKET_CLOSE      20
#define VUT_SOCKET_OPT        21
#define VUT_SOCKET_READ       22
#define VUT_RDP	              23 /*bug 79149*/
#define VUT_RBU_WEB_URL       24

extern int ns_web_url_ext(VUser *vptr, int page_id);
extern int ns_click_action(VUser *vptr, int page_id, int clickaction_id);
extern void vut_execute(u_ns_ts_t now);
extern void vut_add_task(VUser *vptr, int operation);
extern void nsi_web_url(VUser *vptr, u_ns_ts_t now);
extern void nsi_click_action(VUser *vptr, u_ns_ts_t now);
extern unsigned int vut_task_count;

extern inline void nsi_web_url_int(VUser *vptr, u_ns_ts_t now);
unsigned int vut_task_overwrite_count;
//RBU
extern void ns_rbu_setup_for_execute_page(VUser *vptr, int page_id, u_ns_ts_t now);
extern void get_wan_args_for_browser(VUser *vptr, char *wan_args, char *wan_access);
extern int ns_rbu_execute_page(VUser *vptr, int page_id);
extern void ns_rbu_handle_web_url_end(VUser *vptr, u_ns_ts_t now);
extern long ns_rbu_read_ss_file(VUser *vptr);
extern int ns_rbu_click_execute_page(VUser *vptr, int page_id, int ca_id);
extern int ns_rbu_execute_page_via_chrome(VUser *vptr, int page_id);
extern int ns_rbu_execute_page_via_node(VUser *vptr, int page_id);
extern inline void ns_rbu_make_full_qal_url(VUser *vptr);
extern inline void ns_rbu_make_req_cookies(VUser *vptr, char **req_cookies_buf, int *req_cookies_size);
extern int ns_rbu_page_think_time_as_sleep(VUser *vptr, int page_think_time);
extern inline int ns_rbu_make_req_headers(VUser *vptr, char **req_headers_buf, int *req_headers_size);
extern int ns_rbu_copy_profiles_to_tr(VUser *vptr, char *b_path, char *profile_name);
extern int ns_rbu_remove_cookies(VUser *vptr, char *name, char *path, char *domain, int free_for_next_req);
extern void make_msg_and_send_alert(VUser *vptr, char *cav_resp);
extern int nsi_xmpp_logout(VUser *vptr, u_ns_ts_t now);
extern void pause_vuser(VUser *vptr);
extern int ns_rbu_web_url_end(VUser *vptr, char *url, char *har_file, int har_file_size, char *prof_name, char *prev_har_file_name, int req_ok,
                              time_t start_time);
extern int ns_rbu_wait_for_har_file(char *page_name, char *url, char *prof_name, int wait_time, char *firefox_log_path, char *har_log_dir, 
                                    char *cmd_buf, int har_file_flag, VUser *vptr, time_t start_time);
extern int ns_rbu_wait_for_lighthouse_report(VUser *vptr, int wait_time, time_t start_time);
extern void move_html_snapshot_to_tr(VUser *vptr, int wait_time, time_t start_time, int ret);
extern int perform_task_after_rbu_failure(int ret, VUser *vptr);
extern int move_snapshot_to_tr_v2(VUser *vptr, int wait_time, time_t start_time, char *renamed_har_file, char *prev_har_file, int page_status,                                   time_t performance_trace_start_time);
extern int ns_rbu_run_firefox(VUser *vptr, int dummy_flag);
extern int ns_rbu_make_click_con_to_browser(VUser *vptr, char *prof_name, time_t start_time);
extern int make_rbu_connection(VUser *vptr);
extern int ns_rbu_chrome_send_message(VUser *vptr, int fd);
extern int ns_rbu_chrome_read_message(VUser *vptr, int con_fd);
extern int ns_rbu_browser_after_start(VUser *vptr);
extern int move_snapshot_to_tr(VUser *vptr, int wait_time, time_t start_time, int ret);


//Socket API
int nsi_socket_send(VUser *vptr);
int nsi_socket_close(VUser *vptr);

#define NS_VPTR_SET_USER_CONTEXT(X)	if (X) (X)->flags |= NS_VPTR_FLAGS_USER_CTX;
#define NS_VPTR_SET_NVM_CONTEXT(X)      if (X) (X)->flags &= ~NS_VPTR_FLAGS_USER_CTX;
#endif  //NS_VUSER_TASKS_H
