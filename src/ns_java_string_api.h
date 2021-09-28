#ifndef NS_JAVA_STRING_API_H
#define NS_JAVA_STRING_API_H

/* This file will contain some internal methods declarations which will be use in ns_java_string_api  */
extern char *ns_eval_string_flag_internal(char* string, int encode_flag, long *size, VUser *api_vptr);
extern int ns_advance_param_internal(char *param, VUser *api_vptr);
extern int ns_save_string_flag_internal(const char* param_value, int value_length, const char* param_name, int encode_flag, VUser *my_vptr, int not_binary_flag);
extern int ns_get_int_val_internal(char *var, VUser *my_vptr);
extern inline int get_page_status_internal (VUser *my_vptr);
extern inline char *ns_paramarr_idx_or_random_internal(const char * paramArrayName, unsigned int index, int random_flag, VUser *my_vptr);
extern int ns_paramarr_len_internal(const char* paramArrayName, VUser *my_vptr);
extern int ns_add_cookies_internal (char *cookie_buf, VUser *my_vptr);
extern int inline get_sess_status_internal (VUser *my_vptr);
extern int inline set_sess_status_internal(int status, VUser *my_vptr);
extern char inline * ns_get_user_ip_internal(VUser *my_vptr);
extern int ns_is_rampdown_user_internal(VUser *my_vptr);
extern int trace_log_current_sess(VUser* vptr);
extern int ns_set_form_body_ex_internal(char *form_buf_param_name, char *form_body_param_name, char *in_str, VUser *api_vptr);
extern int ns_url_get_hdr_size_internal(VUser *vptr);
extern int ns_url_get_body_size_internal(VUser *vptr);
extern int ns_url_get_resp_size_internal(VUser *vptr);
extern void update_user_flow_count_ex(VUser* vptr, int id);
extern int ns_websocket_ext(VUser *vptr, int ws_api_id, int ws_api_flag);
extern void delete_all_auto_header(VUser *vptr);
extern void ns_web_add_auto_header_data(VUser *vptr, char *header, char *content, int flag);
extern void ns_web_add_hdr_data(VUser *vptr, char *header, char *content, int flag);
extern int get_schedule_phase_type_int(VUser *vptr);
int ns_get_replay_page_ext(VUser *vptr);
extern int ns_start_tx_resp_create_msg(Msg_com_con *mccptr, int ret_value, int *out_msg_len);
extern int ns_sync_point_resp_create_msg(Msg_com_con *mccptr, int ret_value, int *out_msg_len);
extern int ns_get_pg_think_time_internal(VUser *my_vptr);
extern int ns_xml_replace(char *in_param, char *xpath_query, char *xml_fragment, char *out_param);
#endif
