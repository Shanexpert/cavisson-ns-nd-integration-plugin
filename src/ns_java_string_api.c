#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "url.h"
#include "ns_tag_vars.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "nslb_util.h"
#include "init_cav.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"
#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "user_tables.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "util.h"
#include "timing.h"
#include "tmr.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"
#include "netstorm.h"
#include "ns_alloc.h"
#include "ns_log.h"
#include "ns_string.h"
#include "ns_msg_com_util.h"
#include "ns_nvm_msg_def.pb-c.h"
#include "ns_nvm_njvm_msg_com.h"
#include "ns_vuser_tasks.h"
#include "ns_page_think_time.h"
#include "ns_sync_point.h"
#include "ns_trans.h"
#include "ns_session.h"
#include "ns_user_profile.h"
#include "ns_java_string_api.h"
#include "ns_auto_cookie.h"
#include "ns_cookie.h"
#include "ns_server_mapping.h"
#include "ns_string.h"
#include "decomp.h"
#include "ns_url_resp.h"
#include "ns_auto_fetch_embd.h"
#include "ns_jrmi.h"
#include "nslb_time_stamp.h"
#include "ns_page.h"
#include "ns_rbu_page_stat.h"
#include "ns_sockjs.h"
//#include "ns_child_msg_com.h"
#include "ns_websocket.h"
#include "ns_websocket_reporting.h"
#include "ns_group_data.h"

//declare handler here.
static int ns_web_url_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_page_think_time_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_start_transaction_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_end_transaction_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_end_transaction_as_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_end_transaction_as_ex_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_end_transaction_ex_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_get_transaction_time_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_get_tx_status_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_set_tx_status_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_save_string_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_exit_session_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int njvm_bind_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_sync_point_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_eval_string_msg_proc_fun(Msg_com_con* mccptr, int *out_msg_len);
static int ns_save_string_flag_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_eval_string_flag_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_log_string_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_define_syncpoint_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_get_page_status_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_get_session_status_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_set_session_status_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_get_int_val_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_set_int_val_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_get_cookie_val_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_get_cookie_val_ex_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_set_cookie_val_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_set_cookie_val_ex_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_get_auto_cookie_mode_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_log_event_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_set_referer_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
int ns_get_replay_page_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
//static int ns_url_get_body_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
//static int ns_url_get_hdr_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
//static int ns_url_get_resp_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_url_get_resp_size_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_url_get_hdr_size_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_url_get_body_size_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_validate_response_checksum_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
//static int ns_get_redirect_url_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_add_cookie_val_ex_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_add_cookies_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_advance_param_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_paramarr_idx_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_paramarr_len_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_paramarr_random_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_check_reply_size_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_set_ua_string_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_get_cookies_disallowed_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_set_cookies_disallowed_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_get_user_ip_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
//static int ns_add_user_data_point_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_is_rampdown_user_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
//static int ns_set_embd_obj_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_setup_save_url_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_force_server_mapping_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_set_ka_time_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_get_ka_time_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_get_all_cookies_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_click_api_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_set_form_body_ex_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_trace_log_current_sess_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
//static int ns_save_searched_string_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
//static int ns_set_form_body_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_get_double_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_set_double_val_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
//static int ns_process_njvm_resp_buffer_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_process_njvm_jrmi_step_end_func(Msg_com_con* mccptr, int *out_msg_lenu);
static int ns_process_njvm_jrmi_step_start_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_process_njvm_jrmi_resp_buffer_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_start_timer_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_end_timer_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_rbu_page_stats_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_get_cur_partition_msg_proc_function(Msg_com_con* mccptr, int *out_msg_len);
static int ns_get_host_id_msg_proc_function(Msg_com_con* mccptr, int *out_msg_len);
static int ns_db_replay_query_msg_proc_fuction(Msg_com_con* mccptr, int *out_msg_len);
static int ns_db_replay_query_end_msg_proc_function(Msg_com_con* mccptr, int *out_msg_len);
static int ns_rbu_setting_function(Msg_com_con* mccptr, int *out_msg_len);
static int ns_unset_ssl_settings_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_set_ssl_settings_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
//static int ns_remove_cookie_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_cleanup_cookies_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_web_add_header_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_web_add_auto_header_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_web_remove_auto_header_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_web_cleanup_auto_headers_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_get_machine_type_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_web_websocket_send_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_web_websocket_close_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_web_websocket_search_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_web_websocket_check_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_web_websocket_read_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_sockjs_close_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_save_binary_val_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_stop_inline_urls_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_update_user_flow_count_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_get_ua_string_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_get_schedule_phase_name_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_get_schedule_phase_type_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_get_referer_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_start_transaction_ex_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_disable_soap_ws_security_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_enable_soap_ws_security_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_soap_ws_security_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_save_value_from_file_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_eval_compress_param_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_eval_decompress_param_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_get_link_hdr_value_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_get_redirect_url_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_save_command_output_in_param_msg_proc_fun(Msg_com_con* mccptr, int *out_msg_len); 
static int ns_get_param_val_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_get_pg_think_time_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_save_searched_string_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_save_data_ex_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_replace_ex_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_xml_replace_ex_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_save_data_eval_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
static int ns_save_data_var_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len);
/**************************List of jthread message handler****************************************************
 *  hanldler function should be of type jThreadMessageHandler(int (*jThreadMessageHandler) (Msg_com_con *, int *))  *
 *  There are some vacant placecs named unknown so fill them first,                                          *
 *  currently there is only one hanlder for both req and response(? do we need to take separate for any one) *
 *                                                                                                           *
 *************************************************************************************************************/

jThreadMessageHandler jthraed_msg_handler[] = {
/*0  NS_NJVM_START_USER_REQ	1000 */  NULL,
/*1  NS_NJVM_BIND_NVM_REQ	1001 */ njvm_bind_msg_proc_func,
/*2  NJVM_VUTD_INFO_REQ	1002 */ NULL,
/*3  NS_NJVM_API_WEB_URL_REQ	1003 */ ns_web_url_msg_proc_func,
/*4  NS_NJVM_API_PAGE_THINK_TIME_REQ	1004 */ ns_page_think_time_msg_proc_func,
/*5  NS_NJVM_API_START_TRANSACTION_REQ	1005 */ ns_start_transaction_msg_proc_func,
/*6  NS_NJVM_API_END_TRANSACTION_REQ	1006 */ ns_end_transaction_msg_proc_func,
/*7  NS_NJVM_API_END_TRANSACTION_AS_REQ	1007 */ ns_end_transaction_as_msg_proc_func,
/*8  NS_NJVM_API_GET_TRANSACTION_TIME_REQ	1008 */ ns_get_transaction_time_msg_proc_func,
/*9  NS_NJVM_API_GET_TX_STATUS_REQ	1009 */ ns_get_tx_status_msg_proc_func,
/*10 NS_NJVM_API_SET_TX_STATUS_REQ	1010 */ ns_set_tx_status_msg_proc_func,
/*11 NS_NJVM_API_SYNC_POINT_REQ	1011 */ ns_sync_point_msg_proc_func,
/*12 NS_NJVM_API_SAVE_STRING_FLAG_REQ	1012 */ ns_save_string_flag_proc_func,
/*13 NS_NJVM_API_EVAL_STRING_FLAG_REQ	1013 */ ns_eval_string_flag_proc_func,
/*14 NS_NJVM_API_EVAL_STRING_REQ	1014 */ ns_eval_string_msg_proc_fun,
/*15 NS_NJVM_API_SAVE_STRING_REQ	1015 */ ns_save_string_msg_proc_func,
/*16 NS_NJVM_API_LOG_STRING_REQ	1016 */ ns_log_string_msg_proc_func,
/*17 NS_NJVM_API_LOG_MSG_REQ	1017 */ NULL,
/*18 NS_NJVM_API_DEFINE_SYNCPOINT_REQ	1018 */ ns_define_syncpoint_msg_proc_func,
/*19 NS_NJVM_API_GET_PAGE_STATUS_REQ	1019 */ ns_get_page_status_proc_func,
/*20 NS_NJVM_API_CLEANUP_COOKIES_REQ    1020 */ ns_cleanup_cookies_msg_proc_func,
/*21 NS_NJVM_API_SET_PAGE_STATUS_REQ	1021 */ /* opcode can be use */ NULL,
/*22 NS_NJVM_API_SOAP_WS_SECURITY_REQ	1022 */ ns_soap_ws_security_msg_proc_func,
/*23 NS_NJVM_API_GET_SESSION_STATUS_REQ	1023 */ ns_get_session_status_proc_func,
/*24 UNKNOWN */ NULL,
/*25 NS_NJVM_API_SET_SESSION_STATUS_REQ	1025 */ ns_set_session_status_proc_func,
/*26 NS_NJVM_API_DISABLE_SOAP_WS_SECURITY_REQ	1026 */ ns_disable_soap_ws_security_msg_proc_func,
/*27 NS_NJVM_API_EVAL_COMPRESS_PARAM_REQ 1027 */ ns_eval_compress_param_msg_proc_func,
/*28 NS_NJVM_API_EVAL_DECOMPRESS_PARAM_REQ 1028 */ ns_eval_decompress_param_msg_proc_func ,
/*29 NS_NJVM_API_GET_INT_VAL_REQ	1029 */ ns_get_int_val_msg_proc_func,
/*30 NS_NJVM_API_SET_INT_VAL_REQ	1030 */ ns_set_int_val_msg_proc_func,
/*31 NS_NJVM_API_GET_COOKIE_IDX_REQ	1031 */ /* opcode can be use */ NULL,
/*32 NS_NJVM_API_GET_COOKIE_VAL_REQ	1032 */ ns_get_cookie_val_msg_proc_func,
/*33 NS_NJVM_API_GET_COOKIE_VAL_EX_REQ	1033 */ ns_get_cookie_val_ex_msg_proc_func,
/*34 NS_NJVM_API_SET_COOKIE_VAL_REQ	1034 */ ns_set_cookie_val_msg_proc_func,
/*35 NS_NJVM_API_SET_COOKIE_VAL_EX_REQ	1035 */ ns_set_cookie_val_ex_msg_proc_func,
/*36 NS_NJVM_API_GET_AUTO_COOKIE_MODE_REQ	1036 */ ns_get_auto_cookie_mode_msg_proc_func,
/*37 NS_NJVM_API_REMOVE_COOKIE_REQ      1037 */ /*ns_remove_cookie_msg_proc_func*/ NULL,
/*38 NS_NJVM_API_URL_GET_BODY_MSG_REQ	1038 */ /*ns_url_get_body_msg_proc_func*/ NULL,
/*39 NS_NJVM_API_URL_GET_HDR_MSG_REQ	1039 */ /*ns_url_get_hdr_msg_proc_func*/ NULL,
/*40 NS_NJVM_API_URL_GET_RESP_MSG_REQ	1040 */ /*ns_url_get_resp_msg_proc_func*/ NULL,
/*41 NS_NJVM_API_URL_GET_HDR_SIZE_REQ	1041 */ ns_url_get_hdr_size_msg_proc_func,
/*42 NS_NJVM_API_URL_GET_BODY_SIZE_REQ	1042 */ ns_url_get_body_size_msg_proc_func,
/*43 NS_NJVM_API_VALIDATE_RESPONSE_CHECKSUM_REQ	1043 */ ns_validate_response_checksum_msg_proc_func,
/*44 NS_NJVM_API_GET_REDIRECT_URL_REQ	1044 */ /*ns_get_redirect_url_msg_proc_func*/ NULL,
/*45 NS_NJVM_API_WEB_WEBSOCKET_CHECK_REQ	1045 */ ns_web_websocket_check_msg_proc_func,
/*46 NS_NJVM_API_ENABLE_SOAP_WS_SECURITY_REQ	1046 */ ns_enable_soap_ws_security_msg_proc_func,
/*47 NS_NJVM_API_ADD_COOKIE_VAL_EX_REQ	1047 */ ns_add_cookie_val_ex_msg_proc_func,
/*48 NS_NJVM_API_ADD_COOKIES_REQ	1048 */ ns_add_cookies_msg_proc_func,
/*49 NS_NJVM_API_ADVANCE_PARAM_REQ	1049 */ ns_advance_param_msg_proc_func,
/*50 NS_NJVM_API_PARAM_SPRINTF_EX_REQ	1050 */ /* opcode can be use */  NULL,
/*51 NS_NJVM_API_PARAMARR_IDX_REQ	1051 */  ns_paramarr_idx_msg_proc_func,
/*52 NS_NJVM_API_PARAMARR_LEN_REQ	1052 */  ns_paramarr_len_msg_proc_func,
/*53 NS_NJVM_API_PARAMARR_RANDOM_REQ	1053 */ ns_paramarr_random_msg_proc_func,
/*54 NS_NJVM_API_CHECK_REPLY_SIZE_REQ 1054   */ ns_check_reply_size_msg_proc_func,
/*55 UNKNOWN */ NULL,
/*56 UNKNOWN */ NULL,
/*57 NS_NJVM_API_SET_UA_STRING_REQ 1057 */ ns_set_ua_string_msg_proc_func,
/*58 NS_NJVM_API_GET_COOKIES_DISALLOWED_REQ 1058 */ ns_get_cookies_disallowed_msg_proc_func,
/*59 NS_NJVM_API_SET_COOKIES_DISALLOWED_REQ 1059 */ ns_set_cookies_disallowed_msg_proc_func,
/*60 NS_NJVM_API_GET_USER_IP_REQ 1060 */ ns_get_user_ip_msg_proc_func,
/*61 NS_NJVM_API_ADD_USER_DATA_POINT_REQ 1061 */ /*ns_add_user_data_point_msg_proc_func*/ NULL,
/*62 NS_NJVM_API_IS_RAMPDOWN_USER_REQ 1062 */ ns_is_rampdown_user_msg_proc_func,
/*63 NS_NJVM_API_SET_EMBD_OBJECTS_REQ 1063 *//* ns_set_embd_obj_msg_proc_func*/ NULL,
/*64 NS_NJVM_API_SETUP_SAVE_URL_REQ 1064 */ ns_setup_save_url_msg_proc_func,
/*65 NS_NJVM_API_FORCE_SERVER_MAPPING_REQ 1065 */ ns_force_server_mapping_msg_proc_func ,
/*66 NS_NJVM_API_SET_KA_TIME_REQ 1066 */ ns_set_ka_time_msg_proc_func,
/*67 NS_NJVM_API_GET_KA_TIME_REQ 1067 */ ns_get_ka_time_msg_proc_func,
/*68 NS_NJVM_API_GET_ALL_COOKIES_REQ 1068 */ ns_get_all_cookies_msg_proc_func,
/*69 NS_NJVM_API_CLICK_API_REQ 1069 */ ns_click_api_msg_proc_func,
/*70 NS_NJVM_API_SET_FORM_BODY_EX_REQ 1070 */ ns_set_form_body_ex_msg_proc_func,
/*71 NS_NJVM_API_TRACE_LOG_CURRENT_SESS_REQ 1071 */ ns_trace_log_current_sess_msg_proc_func,
/*72 NS_NJVM_API_WEB_WEBSOCKET_READ_REQ 1072 */ ns_web_websocket_read_msg_proc_func,
/*73 NS_NJVM_API_SET_FORM_BODY_REQ 1073 */ /*ns_set_form_body_msg_proc_func */ NULL,
/*74 NS_NJVM_API_GET_DOUBLE_VAL_REQ 1074*/ ns_get_double_msg_proc_func,
/*75 NS_NJVM_API_SET_DOUBLE_VAL 1075*/ ns_set_double_val_msg_proc_func,
/*76 NS_NJVM_PROCESS_RESP_BUFFER 1076*/ ns_process_njvm_jrmi_resp_buffer_func,
/*77 NS_NJVM_PROCESS__JRMI_STEP_START 1077*/ ns_process_njvm_jrmi_step_start_func,
/*78 NS_NJVM_PROCESS_JRMI_STEP_END 1078*/ ns_process_njvm_jrmi_step_end_func,
/*79 NS_NJVM_API_START_TIMER_REQ 1079 */ ns_start_timer_msg_proc_func,
/*80 NS_NJVM_API_END_TIMER_REQ 1080 */ ns_end_timer_msg_proc_func,
/*81 NS_API_SEND_RBU_STATS 1081 */ ns_rbu_page_stats_msg_proc_func,
/*82 NS_API_GET_CURRENT_PARTITION_REQ 1082 */ ns_get_cur_partition_msg_proc_function,
/*83 NS_API_GET_HOST_ID_REQ = 1083 */ns_get_host_id_msg_proc_function ,
/*84 NS_API_DB_REPLAY_QUERY_REQ = 1084 */ ns_db_replay_query_msg_proc_fuction ,
/*85 NS_API_DB_REPLAY_QUERY_END_REQ = 1085 */ ns_db_replay_query_end_msg_proc_function,
/*86 UNKNOWN */ NULL,
/*87 NS_NJVM_API_RBU_SETTING_REQ = 1087*/ ns_rbu_setting_function,
/*88 NS_NJVM_API_WEB_ADD_HEADER_REQ = 1088 */ ns_web_add_header_msg_proc_func,
/*89 NS_NJVM_API_WEB_ADD_AUTO_HEADER_REQ = 1089 */ ns_web_add_auto_header_msg_proc_func,
/*90 NS_NJVM_API_WEB_REMOVE_AUTO_HEADER_REQ = 1090 */ ns_web_remove_auto_header_msg_proc_func,
/*91 NS_NJVM_API_WEB_CLEANUP_AUTO_HEADER_REQ = 1091 */ ns_web_cleanup_auto_headers_msg_proc_func,
/*92 NS_NJVM_API_LOG_EVENT_REQ  = 1092 */ ns_log_event_msg_proc_func,
/*93 NS_NJVM_API_GET_MACHINE_TYPE_REQ = 1093*/ ns_get_machine_type_msg_proc_func,
/*94 NS_NJVM_API_WEB_WEBSOCKET_SEND_REQ = 1094*/ ns_web_websocket_send_msg_proc_func,
/*95 NS_NJVM_API_WEB_WEBSOCKET_CLOSE_REQ = 1095*/ ns_web_websocket_close_msg_proc_func,
/*96 NS_NJVM_API_WEB_WEBSOCKET_SEARCH_REQ = 1096 */ ns_web_websocket_search_msg_proc_func,
/*97 UNKNOWN */ NULL,
/*98 UNKNOWN */ NULL,
/*99 NS_NJVM_API_EXIT_SESSION_REQ      = 1099 */ ns_exit_session_msg_proc_func,
/* NS_NJVM_API_SOCKJS_CLOSE_REQ        = 1100 */ ns_sockjs_close_msg_proc_func,
/* NS_NJVM_API_SAVE_VALUE_FROM_FILE_REQ = 1101 */ ns_save_value_from_file_msg_proc_func,
/* NS_NJVM_API_SAVE_BINARY_VAL_REQ     = 1102 */ ns_save_binary_val_msg_proc_func,
/* NS_NJVM_API_STOP_INLINE_URLS_REQ   = 1103 */ ns_stop_inline_urls_msg_proc_func,
/* NS_NJVM_API_URL_GET_RESP_SIZE_REQ  = 1104 */ ns_url_get_resp_size_msg_proc_func,
/* NS_NJVM_API_UPDATE_USER_FLOW_COUNT_REQ = 1105 */ ns_update_user_flow_count_msg_proc_func,
/* NS_NJVM_API_GET_REPLAY_PAGE_REQ = 1106 */ ns_get_replay_page_msg_proc_func,
/* NS_NJVM_API_GET_UA_STRING_REQ = 1107 */ ns_get_ua_string_msg_proc_func,
/* NS_NJVM_API_GET_SCHEDULE_PHASE_NAME_REQ = 1108 */ ns_get_schedule_phase_name_msg_proc_func,
/* NS_NJVM_API_GET_SCHEDULE_PHASE_TYPE_REQ = 1109 */ ns_get_schedule_phase_type_msg_proc_func,
/* NS_NJVM_API_GET_REFERER_REQ             = 1110 */ ns_get_referer_msg_proc_func,
/* NS_NJVM_API_SET_REFERER_REQ             = 1111 */ ns_set_referer_msg_proc_func,
/* NS_NJVM_API_START_TRANSACTION_EX_REQ    = 1112 */ ns_start_transaction_ex_msg_proc_func,
/* NS_NJVM_API_END_TRANSACTION_EX_REQ      = 1113 */ ns_end_transaction_ex_msg_proc_func,
/* NS_NJVM_API_END_TRANSACTION_AS_EX_REQ   = 1114 */ ns_end_transaction_as_ex_msg_proc_func,
/* NS_NJVM_API_SET_SSL_SETTINGS_REQ        = 1115 */ ns_set_ssl_settings_msg_proc_func,
/* NS_NJVM_API_UNSET_SSL_SETTINGS_REQ      = 1116 */ ns_unset_ssl_settings_msg_proc_func,
/* NS_NJVM_API_GET_LINK_HDR_VALUE_REQ      = 1117 */ ns_get_link_hdr_value_msg_proc_func,
/* NS_NJVM_API_GET_REDIRECT_URL_REQ        = 1118 */ ns_get_redirect_url_msg_proc_func,
/* NS_NJVM_API_SAVE_COMMAND_OUTPUT_IN_PARAM_REQ = 1119 */ ns_save_command_output_in_param_msg_proc_fun,
/* NS_NJVM_API_GET_PARAM_VAL_REQ = 1120 */                ns_get_param_val_msg_proc_func,
/* Previously this  API opcode was 1045 and it was marked as depreciated in 4.2.0 but as per bug 75345,
   this api has to be added again. So created new opcode as old one is used for different API */
/* NS_NJVM_API_GET_PG_THINK_TIME_REQ = 1121 */ ns_get_pg_think_time_msg_proc_func,
/* NS_NJVM_API_SAVE_SEARCHED_STRING_REQ = 1122 */ ns_save_searched_string_msg_proc_func,
/* NS_NJVM_API_SAVE_DATA_EX_REQ = 1123 */ ns_save_data_ex_msg_proc_func,
/* NS_NJVM_API_NS_REPLACE_REQ = 1124 */ ns_replace_ex_msg_proc_func,
/* NS_NJVM_API_XML_REPLACE_REQ = 1125 */ ns_xml_replace_ex_msg_proc_func,
/* NS_NJVM_API_SAVE_DATA_EVAL_REQ = 1126 */ ns_save_data_eval_msg_proc_func,
/* NS_NJVM_API_SAVE_DATA_VAR_REQ = 1127 */ ns_save_data_var_msg_proc_func
};


/* Each api should be of type <int (*nvm_njvm_msg_fun)(Msg_com_con *)>

   Every Api will take a message buffer as an input
   It will process that message on basis of opcode and will call internal ns api.
   If api will return some non void result then it will save that result in out_msg
   and set it's length.
   Apis may have following three return code
   0 - success
   -1 - Failure
   1 - Task have been added, and it's result will be send when that task will be completed
   NOTE: Each api will send status in output message.(0 if successfull)
*/


//we need to advance read_buf by NVM_NJVM_MSG_HDR_SIZE
//size coming in message will not include sizeof itself(-4bytes)
#define GET_MESSAGE_FROM_CONNECTION(con, msg, msg_len)	\
{				\
  if(con->read_buf) {	\
    msg = con->read_buf + NVM_NJVM_MSG_HDR_SIZE;	\
    msg_len = (*(int *)con->read_buf) - (NVM_NJVM_MSG_HDR_SIZE - sizeof(int));		\
		NSDL2_MESSAGES(NULL, NULL, "Recieved message length = %d ", msg_len);\
  } else {	\
		NSDL2_MESSAGES(NULL, NULL, "No message found on jnvm %s", msg_com_con_to_str(con));		\
		return -1;			\
  }									\
}


//this will check current size of write buf. if size is not sufficient then it will realloc it.
#define GET_OUT_MESSAGE_BUFFER(con, out_msg, out_msg_len)	\
{			\
  if(con->write_buf_size < (out_msg_len + NVM_NJVM_MSG_HDR_SIZE))	\
  {		\
    MY_REALLOC(con->write_buf, (out_msg_len+NVM_NJVM_MSG_HDR_SIZE), "con->write_buf", -1);		\
    con->write_buf_size = (out_msg_len +NVM_NJVM_MSG_HDR_SIZE);		\
  }		\
  out_msg = con->write_buf + NVM_NJVM_MSG_HDR_SIZE;		\
}

//ns_eval_string api
static int ns_eval_string_msg_proc_fun(Msg_com_con* mccptr, int *out_msg_len)
{
  char *msg;
  int len;
  char *out_msg;

  NsMsg1Str *in_msg_struct;
  NsMsg1Str out_msg_struct = NS_MSG1__STR__INIT;
  VUser *vptr = mccptr->vptr;

  NSDL2_API(vptr, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message.
  if((in_msg_struct = ns_msg1__str__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }

  //call api and save it's result
  long tmp_len;
  out_msg_struct.field1 = 0;
  out_msg_struct.field1 = ns_eval_string_flag_internal(in_msg_struct->field1, 0, &tmp_len, vptr);

  //free unpacked msg
  ns_msg1__str__free_unpacked(in_msg_struct, NULL);

  //pack this message
  *out_msg_len = ns_msg1__str__get_packed_size(&out_msg_struct);
  //TODO: This length should be compared with maximum available length for out buffer.

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__str__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}


//Api ns_save_string
static int ns_save_string_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *msg;
  int len;
  char *out_msg;

  NsMsg2SS *in_msg_struct;
  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
  VUser *vptr = mccptr->vptr;

  NSDL2_API(vptr, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message
  if((in_msg_struct = ns_msg2__ss__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }

  //call api and save it's result
  out_msg_struct.field1 = ns_save_string_flag_internal(in_msg_struct->field1 /*Param value*/, -1, in_msg_struct->field2/*param name*/, 0, vptr, 1);

  //free unpacked message
  ns_msg2__ss__free_unpacked(in_msg_struct, NULL);

  //pack this message
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}

static int ns_sync_point_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *msg;
  char len;
  //char *out_msg;
  int ret;

  NsMsg1Str *in_msg_struct;
//  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT; 
  VUser *vptr = mccptr->vptr;

  NSDL2_API(vptr, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message
  if((in_msg_struct = ns_msg1__str__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }
  
  char *sync_name = in_msg_struct->field1;

  if(!global_settings->sp_enable)
  {
    NSDL2_API(vptr, NULL, "SyncPoint is not enabled");
    //free unpacked data
    ns_msg1__str__free_unpacked(in_msg_struct, NULL);
   // No need to end the nvm njvm connection so return 0
    return 0;
  } 
 
  if((ret = ns_sync_point_ext(sync_name, vptr)) != 0)
  { 
     NSTL1(vptr, NULL, "Failed to add task for ns_sync_point_ext");
     //free unpacked data
     ns_msg1__str__free_unpacked(in_msg_struct, NULL);
     return 0;
  }

  //free unpacked data
  ns_msg1__str__free_unpacked(in_msg_struct, NULL);

  //Response will not be set here
  return 1;

  //free unpacked data
  //ns_msg1__str__free_unpacked(in_msg_struct, NULL);

  //pack the result
  //*out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);
  //TODO: This length should be compared with maximum available length for out buffer.

  //now get buffer to fill message.
  //GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);
 
//  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
  //  fprintf(stderr, "Failed to pack complete message\n");
   // return -1;
 // }
  //return 0;
}

static int ns_eval_string_flag_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *msg;
  char len;
  char *out_msg;

  NsMsg2SI *in_msg_struct;
  NsMsg1Str out_msg_struct = NS_MSG1__STR__INIT;
  VUser *vptr = mccptr->vptr;

  NSDL2_API(vptr, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message.
  if((in_msg_struct = ns_msg2__si__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }

  //call api and save it's result
  long tmp_len;
  out_msg_struct.field1 = 0;
  out_msg_struct.field1 = ns_eval_string_flag_internal(in_msg_struct->field1, in_msg_struct->field2, &tmp_len, vptr);

  //free unpacked msg
  ns_msg2__si__free_unpacked(in_msg_struct, NULL);

  //pack this message
  *out_msg_len = ns_msg1__str__get_packed_size(&out_msg_struct);
  //TODO: This length should be compared with maximum available length for out buffer.

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__str__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}


static int ns_save_string_flag_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *msg;
  char len;
  char *out_msg;

  NsMsg3SSI *in_msg_struct;
  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
  VUser *vptr = mccptr->vptr;

  NSDL2_API(vptr, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message
  if((in_msg_struct = ns_msg3__ssi__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }

  //call api and save it's result
  out_msg_struct.field1 = ns_save_string_flag_internal(in_msg_struct->field1 /*Param value*/, -1, in_msg_struct->field2/*param name*/, in_msg_struct->field3, vptr, 1);

  //free unpacked message
  ns_msg3__ssi__free_unpacked(in_msg_struct, NULL);

  //pack this message
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}

static int ns_log_string_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *msg;
  int len;
  char *out_msg;

  NsMsg2IS *in_msg_struct;
  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
//  VUser *vptr = mccptr->vptr;

  NSDL2_API(NULL, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message.
  if((in_msg_struct = ns_msg2__is__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }

  //call api and save it's result
  out_msg_struct.field1 = ns_log_string(in_msg_struct->field1, in_msg_struct->field2);

  //free unpacked msg
  ns_msg2__is__free_unpacked(in_msg_struct, NULL);

  //pack this message
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);
  //TODO: This length should be compared with maximum available length for out buffer.

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}

int ns_define_syncpoint_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  return 0;
}

int ns_get_page_status_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *out_msg;

  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
  VUser *vptr = mccptr->vptr;

  NSDL2_API(vptr, NULL, "Method called");

  //call api and save it's result
  out_msg_struct.field1 = get_page_status_internal(vptr);

  //pack this message
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);
  //TODO: This length should be compared with maximum available length for out buffer.

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}


int ns_get_session_status_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *out_msg;

  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
  VUser *vptr = mccptr->vptr;

  NSDL2_API(NULL, NULL, "Method called");

  //call api and save it's result
  out_msg_struct.field1 = get_sess_status_internal(vptr);


  NSDL2_API(NULL, NULL, "Output message = %d\n", out_msg_struct.field1);

  //pack this message
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);
  //TODO: This length should be compared with maximum available length for out buffer.

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;

}

int ns_set_session_status_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *msg;
  char len;
  char *out_msg;

  NsMsg1Int *in_msg_struct;
  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
  VUser *vptr = mccptr->vptr;

  NSDL2_API(NULL, NULL, "Method called");

  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message.
  if((in_msg_struct = ns_msg1__int__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }

  NSDL2_API(NULL, NULL, "Input message = %d\n", in_msg_struct->field1);
  //call api and save it's result
  out_msg_struct.field1 = set_sess_status_internal(in_msg_struct->field1, vptr);

  NSDL2_API(NULL, NULL, "Onput message = %d\n", out_msg_struct.field1);
  //pack this message
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);
  //TODO: This length should be compared with maximum available length for out buffer.

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;

}

int ns_get_int_val_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *msg;
  char len;
  char *out_msg;

  NsMsg1Str *in_msg_struct;
  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
  VUser *vptr = mccptr->vptr;

  NSDL2_API(vptr, NULL, "Method called");

  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message.
  if((in_msg_struct = ns_msg1__str__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }

  //call api and save it's result
  out_msg_struct.field1 = ns_get_int_val_internal(in_msg_struct->field1, vptr);

  //pack this message
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);
  //TODO: This length should be compared with maximum available length for out buffer.

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;

}

int ns_set_int_val_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *msg;
  char len;
  char *out_msg;

  NsMsg2SI *in_msg_struct;
  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
  VUser *vptr = mccptr->vptr;

  NSDL2_API(vptr, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message.
  if((in_msg_struct = ns_msg2__si__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }

  //call api and save it's result
  char buf[32];
  sprintf(buf, "%d", in_msg_struct->field2);

  out_msg_struct.field1 = 0;
  out_msg_struct.field1 = ns_save_string_flag_internal(buf, -1, in_msg_struct->field1, 0, vptr, 1);

  //free unpacked msg
  ns_msg2__si__free_unpacked(in_msg_struct, NULL);

  //pack this message
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);
  //TODO: This length should be compared with maximum available length for out buffer.

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}

static int ns_get_cookie_val_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *msg;
  int len;
  char *out_msg;

  NsMsg1Int *in_msg_struct;
  NsMsg1Str out_msg_struct = NS_MSG1__STR__INIT;
  VUser *vptr = mccptr->vptr;

  NSDL2_API(vptr, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message.
  if((in_msg_struct = ns_msg1__int__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }

  //call api and save it's result
  //call api only if cookie mode is disabled
  if(global_settings->g_auto_cookie_mode == AUTO_COOKIE_DISABLE)
    out_msg_struct.field1 = ns_get_cookie_val_non_auto_mode(in_msg_struct->field1, vptr);
  else {
    fprintf(stderr, "ns_get_cookie_val() can not be used with AUTO_COOKIE enabled\n");
    out_msg_struct.field1 = NULL;
  }

  //free unpacked msg
  ns_msg1__int__free_unpacked(in_msg_struct, NULL);

  //pack this message
  *out_msg_len = ns_msg1__str__get_packed_size(&out_msg_struct);
  //TODO: This length should be compared with maximum available length for out buffer.

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__str__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}

int ns_get_cookie_val_ex_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *msg;
  int len;
  char *out_msg;

  NsMsg3SSS *in_msg_struct;
  NsMsg1Str out_msg_struct = NS_MSG1__STR__INIT;
  VUser *vptr = mccptr->vptr;

  NSDL2_API(vptr, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message
  if((in_msg_struct = ns_msg3__sss__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }

  //call api and save it's result
  if(global_settings->g_auto_cookie_mode != AUTO_COOKIE_DISABLE)
    out_msg_struct.field1 = ns_get_cookie_val_auto_mode(in_msg_struct->field1, in_msg_struct->field2, in_msg_struct->field3, vptr);
  else {
    fprintf(stderr, "ns_get_cookie_val_ex() can not be used with AUTO_COOKIE disabled\n");
    out_msg_struct.field1 = NULL;
  }

  //free unpacked message
  ns_msg3__sss__free_unpacked(in_msg_struct, NULL);

  //pack this message
  *out_msg_len = ns_msg1__str__get_packed_size(&out_msg_struct);

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__str__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;

}

int ns_set_cookie_val_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *msg;
  char len;
  char *out_msg;

  NsMsg2IS *in_msg_struct;
  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
  VUser *vptr = mccptr->vptr;

  NSDL2_API(vptr, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message.
  if((in_msg_struct = ns_msg2__is__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }

  //call api and save it's result
  if(global_settings->g_auto_cookie_mode == AUTO_COOKIE_DISABLE)
    out_msg_struct.field1 = ns_set_cookie_val_non_auto_mode(in_msg_struct->field1, in_msg_struct->field2, vptr);
  else  {
    fprintf(stderr, "ns_set_cookie_val() can not be used with AUTO_COOKIE enabled\n");
    out_msg_struct.field1 = -1;
  }

  //free unpacked msg
  ns_msg2__is__free_unpacked(in_msg_struct, NULL);

  //pack this message
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);
  //TODO: This length should be compared with maximum available length for out buffer.

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;

}

int ns_set_cookie_val_ex_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *msg;
  char len;
  char *out_msg;

  NsMsg4SSSS *in_msg_struct;
  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
  VUser *vptr = mccptr->vptr;

  NSDL2_API(vptr, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message.
  if((in_msg_struct = ns_msg4__ssss__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }

  //call api and save it's result
  if(global_settings->g_auto_cookie_mode == AUTO_COOKIE_DISABLE)
  {
    fprintf(stderr, "Error - ns_set_cookie_val() cannot be used if Auto Cookie Mode is disabled\n");
    out_msg_struct.field1 = -1;
  }
  else {
    out_msg_struct.field1 = ns_set_cookie_val_auto_mode(in_msg_struct->field1, in_msg_struct->field2,in_msg_struct->field3,in_msg_struct->field4, vptr);
  }

  //free unpacked msg
  ns_msg4__ssss__free_unpacked(in_msg_struct, NULL);

  //pack this message
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);
  //TODO: This length should be compared with maximum available length for out buffer.

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;

}

int ns_get_auto_cookie_mode_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *out_msg;

  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
  //VUser *vptr = mccptr->vptr;

  NSDL2_API(NULL, NULL, "Method called");

  //call api and save it's result
  out_msg_struct.field1 = ns_get_cookie_mode_auto();

   NSDL2_API(NULL, NULL, "auto cookie mode output message value = %d", out_msg_struct.field1);
  //pack this message
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);
  //TODO: This length should be compared with maximum available length for out buffer.

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}

/*int ns_url_get_body_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *out_msg;

  NsMsg1Str out_msg_struct = NS_MSG1__STR__INIT;

  VUser *vptr = mccptr->vptr;
  NSDL2_API(NULL, NULL, "Method called");

  if(vptr->cptr == NULL) {
    out_msg_struct.field1 =  full_buffer;
  }
  else if (vptr->cptr->compression_type) {
     char err[1024];
     char *inp_buffer_body = url_resp_buff + vptr->cptr->body_offset;
     int body_size = vptr->cptr->bytes - vptr->cptr->body_offset;
     if (ns_decomp_do_new (inp_buffer_body, body_size, vptr->cptr->compression_type, err)) {
       error_log("Error Decompressing: %s", err);
       fprintf (stderr, "Error decompressing non-chunked body: %s\n", err);
     }
  //   *size = ns_url_get_body_size();
      out_msg_struct.field1 = uncomp_buf;
    } else {
    // *size = ns_url_get_body_size();
      out_msg_struct.field1 = &url_resp_buff[url_resp_hdr_size];
    }

  //call api and save it's result
  //out_msg_struct.field1 = 0;
 // out_msg_struct.field1 = ns_url_get_body_msg(&len);

  NSDL2_API(NULL, NULL, "url body output = %s", out_msg_struct.field1);
  //pack this message
  *out_msg_len = ns_msg1__str__get_packed_size(&out_msg_struct);
  //TODO: This length should be compared with maximum available length for out buffer.

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__str__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}

int ns_url_get_hdr_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  int len;
  char *out_msg;

  NsMsg1Str out_msg_struct = NS_MSG1__STR__INIT;

  VUser *vptr = mccptr->vptr;
  NSDL2_API(NULL, NULL, "Method called");

  //call api and save it's result
  out_msg_struct.field1 = 0;

    //Currently there is no wrapper for thread mode so we handling here.
  if(vptr->cptr != NULL){
    out_msg_struct.field1 = url_resp_hdr_buff;
  }
  else {
    out_msg_struct.field1 = NULL;
  }

  //out_msg_struct.field1 = ns_url_get_hdr_msg(&len);

  //pack this message
  *out_msg_len = ns_msg1__str__get_packed_size(&out_msg_struct);
  //TODO: This length should be compared with maximum available length for out buffer.

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__str__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}

int ns_url_get_resp_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *out_msg;

  NsMsg1Str out_msg_struct = NS_MSG1__STR__INIT;

  VUser *vptr = mccptr->vptr;
  NSDL2_API(NULL, NULL, "Method called");

  if(vptr->cptr != NULL){
    out_msg_struct.field1 = url_resp_buff;
  }
  else {
    out_msg_struct.field1 = NULL;
  }

  //call api and save it's result
//  out_msg_struct.field1 = ns_url_get_resp_msg(&len);

  //pack this message
  *out_msg_len = ns_msg1__str__get_packed_size(&out_msg_struct);
  //TODO: This length should be compared with maximum available length for out buffer.

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__str__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
} */

int ns_url_get_hdr_size_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *out_msg;

  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;

  VUser *vptr = mccptr->vptr;

  NSDL2_API(vptr, NULL, "Method called");

  //call api and save it's result
  out_msg_struct.field1 = ns_url_get_hdr_size_internal(vptr);

  //pack this message
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;

}

int ns_url_get_body_size_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{

  char *out_msg;

  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;

  VUser *vptr = mccptr->vptr;

  NSDL2_API(vptr, NULL, "Method called");

  //call api and save it's result
  out_msg_struct.field1 = ns_url_get_body_size_internal(vptr);
  
  //pack this message
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}

int ns_validate_response_checksum_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{

  char *msg;
  int len;
  char *out_msg;

  NsMsg2SS *in_msg_struct;
  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
  //VUser *vptr = mccptr->vptr;

  NSDL2_API(NULL, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message
  if((in_msg_struct = ns_msg2__ss__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }

  //call api and save it's result
  out_msg_struct.field1 = ns_validate_response_checksum(in_msg_struct->field1, in_msg_struct->field2);

  //free unpacked message
  ns_msg2__ss__free_unpacked(in_msg_struct, NULL);

  //pack this message
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;

}

/*int ns_get_redirect_url_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{

  char *out_msg;

  NsMsg1Str out_msg_struct = NS_MSG1__STR__INIT;

  VUser *vptr = mccptr->vptr;
  NSDL2_API(NULL, NULL, "Method called");

  if(vptr->cptr == NULL) {
     fprintf(stderr, "ns_get_redirect_url() with NULL cur_cptr\n");
     out_msg_struct.field1 = NULL;
    }
  else
    out_msg_struct.field1 = vptr->cptr->location_url;

  //call api and save it's result
//  out_msg_struct.field1 = ns_get_redirect_url();

  //pack this message
  *out_msg_len = ns_msg1__str__get_packed_size(&out_msg_struct);
  //TODO: This length should be compared with maximum available length for out buffer.

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__str__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}
*/

int ns_add_cookie_val_ex_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{

  char *msg;
  char len;
  char *out_msg;

  NsMsg4SSSS *in_msg_struct;
  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
  VUser *vptr = mccptr->vptr;

  NSDL2_API(vptr, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message.
  if((in_msg_struct = ns_msg4__ssss__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }

  //call api and save it's result
  if(global_settings->g_auto_cookie_mode == AUTO_COOKIE_DISABLE)
  {
    fprintf(stderr, "Error - ns_add_cookie_val_ex() cannot be used if Auto Cookie Mode is disabled\n");
    out_msg_struct.field1 = -1;
  }
  else {

  out_msg_struct.field1 = ns_add_cookie_val_auto_mode(in_msg_struct->field1, in_msg_struct->field2,in_msg_struct->field3,in_msg_struct->field4, vptr);
  }
  //free unpacked msg
  ns_msg4__ssss__free_unpacked(in_msg_struct, NULL);

  //pack this message
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);
  //TODO: This length should be compared with maximum available length for out buffer.

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}

int ns_add_cookies_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{

  char *msg;
  int len;
  char *out_msg;

  NsMsg1Str *in_msg_struct;
  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
  VUser *vptr = mccptr->vptr;

  NSDL2_API(vptr, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message
  if((in_msg_struct = ns_msg1__str__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }

  out_msg_struct.field1 = ns_add_cookies_internal(in_msg_struct->field1, vptr);

  //free unpacked data
  ns_msg1__str__free_unpacked(in_msg_struct, NULL);

  //pack the result
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);
  //TODO: This length should be compared with maximum available length for out buffer.

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;

}

int ns_advance_param_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *msg;
  int len;
  char *out_msg;

  NsMsg1Str *in_msg_struct;
  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
  VUser *vptr = mccptr->vptr;

  NSDL2_API(vptr, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message
  if((in_msg_struct = ns_msg1__str__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }

  out_msg_struct.field1 = ns_advance_param_internal(in_msg_struct->field1, vptr);

  /* Case when USE_ONCE DATA EXHAUST is encountered, we are performing nsi_end_session and
     sending the reply from there only */
  if(out_msg_struct.field1 == -2)
  {
    return 1;
  }

  //free unpacked data
  ns_msg1__str__free_unpacked(in_msg_struct, NULL);

  //pack the result
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);
  //TODO: This length should be compared with maximum available length for out buffer.

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}

int ns_paramarr_idx_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *msg;
  char len;
  char *out_msg;

  NsMsg2SUI *in_msg_struct;
  NsMsg1Str out_msg_struct = NS_MSG1__STR__INIT;
  VUser *vptr = mccptr->vptr;

  NSDL2_API(vptr, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message.
  if((in_msg_struct = ns_msg2__sui__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }

  //call api and save it's result
  out_msg_struct.field1 = ns_paramarr_idx_or_random_internal(in_msg_struct->field1, in_msg_struct->field2, 0, vptr);

  //free unpacked msg
  ns_msg2__sui__free_unpacked(in_msg_struct, NULL);

  //pack this message
  *out_msg_len = ns_msg1__str__get_packed_size(&out_msg_struct);
  //TODO: This length should be compared with maximum available length for out buffer.

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__str__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;

}

int ns_paramarr_len_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{

  char *msg;
  char len;
  char *out_msg;

  NsMsg1Str *in_msg_struct;
  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
  VUser *vptr = mccptr->vptr;

  NSDL2_API(vptr, NULL, "Method called");

  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message.
  if((in_msg_struct = ns_msg1__str__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }

  //call api and save it's result
  out_msg_struct.field1 = ns_paramarr_len_internal(in_msg_struct->field1, vptr);

  //pack this message
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);
  //TODO: This length should be compared with maximum available length for out buffer.

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}

int ns_paramarr_random_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *msg;
  int len;
  char *out_msg;

  NsMsg1Str *in_msg_struct;
  NsMsg1Str out_msg_struct = NS_MSG1__STR__INIT;
  VUser *vptr = mccptr->vptr;

  NSDL2_API(vptr, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message.
  if((in_msg_struct = ns_msg1__str__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }

  //call api and save it's result
  out_msg_struct.field1 = ns_paramarr_idx_or_random_internal(in_msg_struct->field1, 0, 1, vptr);

  //free unpacked msg
  ns_msg1__str__free_unpacked(in_msg_struct, NULL);

  //pack this message
  *out_msg_len = ns_msg1__str__get_packed_size(&out_msg_struct);
  //TODO: This length should be compared with maximum available length for out buffer.

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__str__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;

}

int ns_check_reply_size_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *msg;
  char len;
  char *out_msg;

  NsMsg3III *in_msg_struct;
  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
  //VUser *vptr = mccptr->vptr;

  NSDL2_API(NULL, NULL, "Method called");

  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message.
  if((in_msg_struct = ns_msg3__iii__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }

  //call api and save it's result
  out_msg_struct.field1 = ns_check_reply_size(in_msg_struct->field1, in_msg_struct->field2, in_msg_struct->field3);

  //pack this message
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);
  //TODO: This length should be compared with maximum available length for out buffer.

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}

int ns_set_ua_string_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{

  char *msg;
  int len;

  NsMsg1Str *in_msg_struct;
  VUser *vptr = mccptr->vptr;

  NSDL2_API(vptr, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message
  if((in_msg_struct = ns_msg1__str__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }

  //ns_set_ua_string_ext(in_msg_struct->field1, in_msg_struct->field2, vptr);
  ns_set_ua_string_ext(in_msg_struct->field1, strlen(in_msg_struct->field1), vptr);

  //free unpacked message
  ns_msg1__str__free_unpacked(in_msg_struct, NULL);

  return 0;
}

int ns_get_cookies_disallowed_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{

  char *out_msg;

  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
  VUser *vptr = mccptr->vptr;

  NSDL2_API(vptr, NULL, "Method called");

  out_msg_struct.field1 = ((vptr->flags)& NS_COOKIES_DISALLOWED);

  NSDL2_API(NULL, NULL, "output_value for cookies disallowed = %d", out_msg_struct.field1);
  //pack the result
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}

int ns_set_cookies_disallowed_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *msg;
  int len;
  VUser *vptr = mccptr->vptr;

  NsMsg1Int *in_msg_struct;

  NSDL2_API(NULL, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message
  if((in_msg_struct = ns_msg1__int__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }

  if(in_msg_struct->field1)
      vptr->flags |= NS_COOKIES_DISALLOWED;
  else
      vptr->flags &= ~NS_COOKIES_DISALLOWED;

  //free unpacked data
  ns_msg1__int__free_unpacked(in_msg_struct, NULL);

  return 0;
}

int ns_get_user_ip_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{

  char *out_msg;

  NsMsg1Str out_msg_struct = NS_MSG1__STR__INIT;

  VUser *vptr = mccptr->vptr;
  NSDL2_API(NULL, NULL, "Method called");

  //call api and save it's result
  out_msg_struct.field1 = ns_get_user_ip_internal(vptr);

  NSDL2_API(NULL, NULL, "ns_get_user_ip output = %s", out_msg_struct.field1);

  //pack this message
  *out_msg_len = ns_msg1__str__get_packed_size(&out_msg_struct);
  //TODO: This length should be compared with maximum available length for out buffer.

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__str__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}
/*
int ns_add_user_data_point_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *msg;
  char len;
  char *out_msg;

  NsMsg3IID *in_msg_struct;
  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;

  NSDL2_API(NULL, NULL, "Method called");

  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message.
  if((in_msg_struct = ns_msg3__iid__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }

  //call api and save it's result
  out_msg_struct.field1 = add_user_data_point(in_msg_struct->field1, in_msg_struct->field2, in_msg_struct->field3);

  //pack this message
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
} */

int ns_is_rampdown_user_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *out_msg;

  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
  VUser *vptr = mccptr->vptr;

  NSDL2_API(vptr, NULL, "Method called");

  out_msg_struct.field1 = ns_is_rampdown_user_internal(vptr);

  NSDL2_API(NULL, NULL, "output_value for is_rampdown_user = %d", out_msg_struct.field1);
  //pack the result
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}

int ns_set_embd_obj_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{

  return 0;//TO DO implement it

}

int ns_setup_save_url_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *msg;
  char len;
  char *out_msg;

  NsMsg3IIS *in_msg_struct;
  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
  VUser *vptr = mccptr->vptr;

  NSDL2_API(NULL, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message.
  if((in_msg_struct = ns_msg3__iis__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }

  //call api and save it's result
  out_msg_struct.field1 = ns_setup_save_url_ext(in_msg_struct->field1, in_msg_struct->field2, in_msg_struct->field3, vptr);

  //free unpacked msg
  ns_msg3__iis__free_unpacked(in_msg_struct, NULL);

  //pack this message
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);
  //TODO: This length should be compared with maximum available length for out buffer.

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}

int ns_force_server_mapping_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *msg;
  int len;
  char *out_msg;

  NsMsg2SS *in_msg_struct;
  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
  VUser *vptr = mccptr->vptr;

  NSDL2_API(vptr, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message
  if((in_msg_struct = ns_msg2__ss__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }

  //call api and save it's result
  out_msg_struct.field1 = ns_force_server_mapping_ext(vptr, in_msg_struct->field1, in_msg_struct->field2);

  //free unpacked message
  ns_msg2__ss__free_unpacked(in_msg_struct, NULL);

  //pack this message
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}

int ns_set_ka_time_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *msg;
  int len;
  VUser *vptr = mccptr->vptr;

  NsMsg1Int *in_msg_struct;

  NSDL2_API(vptr, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message
  if((in_msg_struct = ns_msg1__int__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }

  vptr->ka_timeout = in_msg_struct->field1;
  NSDL2_API(vptr, NULL, "Method Called, setting ka_timeout = %d", vptr->ka_timeout);

  //free unpacked data
  ns_msg1__int__free_unpacked(in_msg_struct, NULL);

  return 0;
}

int ns_get_ka_time_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *out_msg;

  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
  VUser *vptr = mccptr->vptr;

  NSDL2_API(vptr, NULL, "Method called");

  out_msg_struct.field1 = vptr->ka_timeout;

  NSDL2_API(NULL, NULL, "output_value for is_rampdown_user = %d", out_msg_struct.field1);
  //pack the result
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}

int ns_get_all_cookies_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *msg;
  char len;
  char *out_msg;

  NsMsg2SI *in_msg_struct;
  NsMsg1Str out_msg_struct = NS_MSG1__STR__INIT;
  VUser *vptr = mccptr->vptr;

  NSDL2_API(vptr, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message.
  if((in_msg_struct = ns_msg2__si__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }

  //call api and save it's result
  out_msg_struct.field1 = get_all_cookies(vptr, in_msg_struct->field1, in_msg_struct->field2);

  //free unpacked msg
  ns_msg2__si__free_unpacked(in_msg_struct, NULL);

  //pack this message
  *out_msg_len = ns_msg1__str__get_packed_size(&out_msg_struct);
  //TODO: This length should be compared with maximum available length for out buffer.

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__str__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}

int ns_click_api_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *msg;
  char len;

  NsMsg2II *in_msg_struct;
  VUser *vptr = mccptr->vptr;

  NSDL2_API(vptr, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message.
  if((in_msg_struct = ns_msg2__ii__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }

  //call api and save it's result
  ns_click_action(vptr, in_msg_struct->field1, in_msg_struct->field2);
  
  //free unpacked msg
  ns_msg2__ii__free_unpacked(in_msg_struct, NULL);

  return 1; //Response will not be set here, RBU sends response later
}

int ns_set_form_body_ex_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *msg;
  char len;
  char *out_msg;

  NsMsg3SSS *in_msg_struct;
  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
  VUser *vptr = mccptr->vptr;

  NSDL2_API(vptr, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message
  if((in_msg_struct = ns_msg3__sss__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }

  //call api and save it's result
  out_msg_struct.field1 = ns_set_form_body_ex_internal(in_msg_struct->field1, in_msg_struct->field2, in_msg_struct->field3, vptr);

  //free unpacked message
  ns_msg3__sss__free_unpacked(in_msg_struct, NULL);

  //pack this message
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;

}

int ns_trace_log_current_sess_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *out_msg;

  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
  VUser *vptr = mccptr->vptr;

  NSDL2_API(vptr, NULL, "Method called");

  out_msg_struct.field1 = ns_trace_log_current_sess(vptr);

  NSDL2_API(NULL, NULL, "output_value for log_current_sess = %d", out_msg_struct.field1);
  //pack the result
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}


#if 0
#define ARGUMENT_LIST1(a)	a->field1
#define ARGUMENT_LIST2(a) a->field1, a->field2
#define ARGUMENT_LIST3(a) a->field1, a->field2, a->field3
#define ARGUMENT_LIST4(a) a->field1, a->field2, a->field3, a->field4

//This macro can be used to handle other apis

#define NS_NJVM_API_HANDLER (					\
	in_char,														\
	in_len,															\
	out_char,														\
	out_len,														\
	InMsgType,													\
	OutMsgType,													\
	unpack_func,												\
	free_unpack_func,										\
	get_pack_size_func,									\
	pack_func,													\
	api_func,														\
	ARGUMENT_LIST												\
	)																		\
	{																																	\
		InMsgType *in_msg_struct;																				\
		OutMsgType out_msg_struct = NS_MSG1__INT__INIT;									\
																																		\
		NSDL2_API(NULL, NULL, "Method called");													\
																																		\
		if((in_msg_struct = unpack_func(NULL, len, msg)) == NULL) {			\
			fprintf(stderr, "Invalid message recieved\n");								\
			return -1;																										\
		}																																\
																																		\
		out_msg_struct.field1 = api_func(ARGUMENT_LIST(in_msg_struct));	\
																																		\
		free_unpack_func(in_msg_struct, NULL);													\
																																		\
		*out_msg_len = get_pack_size_func(&out_msg_struct);							\
																																		\
		if(*out_msg_len != pack_pack(&out_msg_struct, out_msg)) {				\
			fprintf(stderr, "Failed to pack complete message\n");					\
			return -1;																										\
		}																																\
		return 0;																												\
	}

static int ns_get_int_val_msg_proc_func(char *msg, int len, char *out_msg, int *out_msg_len)
{
  NS_NJVM_API_HANDLER(msg, len, out_msg, out_msg_len,
                      NsMsg1Str,
											NsMsg1Int,
	                    ns_msg1__str__unpack,
	                    ns_msg1__str__free_unpacked,
	                    ns_msg1__int__get_packed_size,
	                    ns_msg1__int__pack,
	                    ns_get_int_val,
	                    ARGUMENT_LIST1);

}

#endif


//ns_web_url api
static int ns_web_url_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *msg;
  int len;

  NsMsg1Int *in_msg_struct;
  VUser *vptr = mccptr->vptr;

  NSDL2_API(vptr, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message.
  if((in_msg_struct = ns_msg1__int__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }

  //Add task to nvm and return from here with return code 1.(When web_url will complete then it will send the response)
  *out_msg_len = 0;
  int page_id = in_msg_struct->field1;

  //free unpacked message
  ns_msg1__int__free_unpacked(in_msg_struct, NULL);

  int ret;
  NSDL2_API(NULL, NULL, "njvm thread(%s), api - ns_web_url(%d)", msg_com_con_to_str(mccptr), page_id);
  if((ret = ns_web_url_ext(vptr, page_id)) != 0)
  {
    fprintf(stderr, "Failed to add task for ns_web_url\n");
    return -1;
  }
  //Response will not be set here
  return 1;
}

//Page think time
static int ns_page_think_time_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *msg;
  int len;

  NsMsg1Double *in_msg_struct;
  VUser *vptr = mccptr->vptr;

  NSDL2_API(vptr, NULL, "Method Called, vptr->vuser_state = %hd", vptr->vuser_state);

  /*
    Bug 66696 - Core | Test got killed with JAVA type script when think time API applied after last page and also session pacing in scenario
    This flag should be set in case of JRMI protocol only. ns_page_think_time() api for JAVA doesn't say anything about JRMI, so commenting
    below line.
    TODO : How to check this api is called for JRMI.
  */
  //vptr->flags |= NS_JNVM_JRMI_RESP;

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message
  if((in_msg_struct = ns_msg1__double__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }

  NSDL2_API(NULL, NULL, "njvm thread(%s), api - ns_page_think_time(%f)", msg_com_con_to_str(mccptr), in_msg_struct->field1);

  int page_think_time =  (int ) (in_msg_struct->field1 * 1000);

  //free unpacked message
  ns_msg1__double__free_unpacked(in_msg_struct, NULL);

  int ret;
  if((ret = ns_page_think_time_ext(vptr, page_think_time)) != 0) {
    fprintf(stderr, "Failed to add task for ns_page_think_time\n");
    return -1;
  }

  *out_msg_len = 0;
  return 1;
}


static int ns_start_transaction_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *msg;
  int len;
  char *out_msg;

  NsMsg1Str *in_msg_struct;
  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
  VUser *vptr = mccptr->vptr;

  NSDL2_API(vptr, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message
  if((in_msg_struct = ns_msg1__str__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }
  char *tx_name = in_msg_struct->field1;

  if(global_settings->sp_enable)
  {
    NSDL2_API(vptr, NULL, "SyncPoint is enabled");
    if(!ns_trans_chk_for_sp(tx_name, vptr)) {//If return 0 then this user was in sync point
      //free unpacked data
      ns_msg1__str__free_unpacked(in_msg_struct, NULL);
      return 1; //return 1 means task is added successfully and reply will be sent on execution
    }
  }


  out_msg_struct.field1 = tx_start_with_name (tx_name, vptr);
  //free unpacked data
  ns_msg1__str__free_unpacked(in_msg_struct, NULL);

  //pack this message
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);
  //TODO: This length should be compared with maximum available length for out buffer.

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}

//ns_end_transaction
static int ns_end_transaction_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *msg;
  int len;
  char *out_msg;

  NsMsg2SI *in_msg_struct;
  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
  VUser *vptr = mccptr->vptr;

  NSDL2_API(vptr, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message
  if((in_msg_struct = ns_msg2__si__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }
  char *tx_name = in_msg_struct->field1;
  int status = in_msg_struct->field2;

  out_msg_struct.field1 = tx_end(tx_name, status, vptr);

  //free unpacked data
  ns_msg2__si__free_unpacked(in_msg_struct, NULL);

  //pack the result
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);
  //TODO: This length should be compared with maximum available length for out buffer.

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}


//ns_end_transaction_as
static int ns_end_transaction_as_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *msg;
  int len;
  char *out_msg;

  NsMsg3SIS *in_msg_struct;
  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
  VUser *vptr = mccptr->vptr;

  NSDL2_API(vptr, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message
  if((in_msg_struct = ns_msg3__sis__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }
  //These declaration can be avoid
  char *tx_name = in_msg_struct->field1;
  int status = in_msg_struct->field2;
  char *end_tx_name = in_msg_struct->field3;

  out_msg_struct.field1 = tx_end_as(tx_name, status, end_tx_name, vptr);

  //free unpacked data
  ns_msg3__sis__free_unpacked(in_msg_struct, NULL);

  //pack the result
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);
  //TODO: This length should be compared with maximum available length for out buffer.

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}

//ns_save_data_Ex
int ns_save_data_ex_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *msg;
  int len;
  char *out_msg;

  NsMsg3SIS *in_msg_struct;
  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;

  IW_UNUSED(VUser *vptr = mccptr->vptr);

  NSDL2_API(vptr, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message
  if((in_msg_struct = ns_msg3__sis__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }
  //These declaration can be avoid
  char *file_name = in_msg_struct->field1;
  int mode = in_msg_struct->field2;
  char *format = in_msg_struct->field3;

  out_msg_struct.field1 = ns_save_data_ex(file_name, mode, format);

  //free unpacked data
  ns_msg3__sis__free_unpacked(in_msg_struct, NULL);

  //pack the result
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);
  //TODO: This length should be compared with maximum available length for out buffer.

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}

//ns_save_data_eval
int ns_save_data_eval_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *msg;
  int len;
  char *out_msg;

  NsMsg3SIS *in_msg_struct;
  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;

  IW_UNUSED(VUser *vptr = mccptr->vptr);

  NSDL2_API(vptr, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message
  if((in_msg_struct = ns_msg3__sis__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }

  //ns_save_data_eval(filename, mode, eval_string)
  NSDL2_API(vptr, NULL, "Recieved arguments from njvm: filename %s, mode %d, eval_string %s", 
                         in_msg_struct->field1, in_msg_struct->field2, in_msg_struct->field3);
  out_msg_struct.field1 = ns_save_data_eval(in_msg_struct->field1, in_msg_struct->field2, in_msg_struct->field3);

  //free unpacked data
  ns_msg3__sis__free_unpacked(in_msg_struct, NULL);

  //pack the result
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);
  //TODO: This length should be compared with maximum available length for out buffer.

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}

//ns_save_data_var
int ns_save_data_var_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *msg;
  int len;
  char *out_msg;

  NsMsg3SIS *in_msg_struct;
  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;

  IW_UNUSED(VUser *vptr = mccptr->vptr);

  NSDL2_API(vptr, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message
  if((in_msg_struct = ns_msg3__sis__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }

  //ns_save_data_var(filename, mode, variable_name)
  NSDL2_API(vptr, NULL, "Recieved arguments from njvm: filename %s, mode %d, variable %s", 
                         in_msg_struct->field1, in_msg_struct->field2, in_msg_struct->field3);
  out_msg_struct.field1 = ns_save_data_var(in_msg_struct->field1, in_msg_struct->field2, in_msg_struct->field3);

  //free unpacked data
  ns_msg3__sis__free_unpacked(in_msg_struct, NULL);

  //pack the result
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);
  //TODO: This length should be compared with maximum available length for out buffer.

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}

//ns_get_transaction_time
static int ns_get_transaction_time_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *msg;
  int len;
  char *out_msg;

  NsMsg1Str *in_msg_struct;
  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
  VUser *vptr = mccptr->vptr;

  NSDL2_API(vptr, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message
  if((in_msg_struct = ns_msg1__str__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }
  //These declaration can be avoid
  char *tx_name = in_msg_struct->field1;

  out_msg_struct.field1 = tx_get_time(tx_name, vptr);

  //free unpacked data
  ns_msg1__str__free_unpacked(in_msg_struct, NULL);

  //pack the result
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);
  //TODO: This length should be compared with maximum available length for out buffer.

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}


//ns_get_tx_status
static int ns_get_tx_status_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *msg;
  int len;
  char *out_msg;

  NsMsg1Str *in_msg_struct;
  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
  VUser *vptr = mccptr->vptr;

  NSDL2_API(vptr, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message
  if((in_msg_struct = ns_msg1__str__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }
  //These declaration can be avoid
  char *tx_name = in_msg_struct->field1;

  out_msg_struct.field1 = tx_get_status(tx_name, vptr);

  //free unpacked data
  ns_msg1__str__free_unpacked(in_msg_struct, NULL);

  //pack the result
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);
  //TODO: This length should be compared with maximum available length for out buffer.

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}

//ns_set_tx_status
static int ns_set_tx_status_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *msg;
  int len;
  char *out_msg;

  NsMsg2SI *in_msg_struct;
  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
  VUser *vptr = mccptr->vptr;

  NSDL2_API(vptr, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message
  if((in_msg_struct = ns_msg2__si__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }
  //These declaration can be avoid
  char *tx_name = in_msg_struct->field1;
  int status = in_msg_struct->field2;

  out_msg_struct.field1 = tx_set_status_by_name(tx_name, status, vptr);

  //free unpacked data
  ns_msg2__si__free_unpacked(in_msg_struct, NULL);

  //pack the result
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);
  //TODO: This length should be compared with maximum available length for out buffer.

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}


//ns_exit_session
static int ns_exit_session_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  //char *out_msg;

  //NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
  VUser *vptr = mccptr->vptr;

  NSDL2_API(vptr, NULL, "Method called");

  NSDL2_API(NULL, NULL, "njvm thread(%s), api - ns_exit_session()", msg_com_con_to_str(mccptr));

  //out_msg_struct.field1 = ns_exit_session_ext(vptr);
   
   ns_exit_session_ext(vptr);
  
  //pack the result
 /* *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }*/
  NSDL2_API(vptr, NULL, "NVM will send response later during its task execution.");
  return 1;
}

//This function will create request message for start user request.
int start_user_req_create_msg(Msg_com_con* mccptr, int *out_msg_len)
{
  char *out_msg;
  char sess_name_with_proj_subproj[2048];
  char package_name[512];

  NsMsgStartUser out_msg_struct = NS_MSG__START_USER__INIT;
  VUser *vptr = mccptr->vptr;
  snprintf(sess_name_with_proj_subproj, 2048, "%s/%s/%s", vptr->sess_ptr->proj_name, vptr->sess_ptr->sub_proj_name, vptr->sess_ptr->sess_name);
  NSDL2_API(vptr, NULL, "Method called");

  out_msg_struct.user_id = vptr->user_index;
  out_msg_struct.sess_idx = vptr->sess_ptr->sess_id;
  out_msg_struct.script_name = sess_name_with_proj_subproj;
  //out_msg_struct.script_name = vptr->sess_ptr->sess_name;
  out_msg_struct.num_ka_pct = runprof_table_shr_mem[vptr->group_num].gset.ka_pct;
  out_msg_struct.group_id = vptr->group_num;
  out_msg_struct.sess_inst = vptr->sess_inst;
  out_msg_struct.runlogic_name = runprof_table_shr_mem[vptr->group_num].gset.runlogic_func;

  if (session_table_shr_mem[vptr->sess_ptr->sess_id].flags & ST_FLAGS_SCRIPT_NEW_JAVA_PKG)
  {
    snprintf(package_name, 512, "%s.runlogic", vptr->sess_ptr->sess_name);
    out_msg_struct.package_name = package_name;
  }else
  {
    snprintf(package_name, 512, "com.cavisson.scripts.%s", vptr->sess_ptr->sess_name);
    out_msg_struct.package_name = package_name;
  }

  //set num_ka_min.
  if((runprof_table_shr_mem[vptr->group_num].gset.num_ka_min == 999999999)
			&&
    (runprof_table_shr_mem[vptr->group_num].gset.num_ka_range == 0))
    out_msg_struct.num_ka_min = 0;
  else
		out_msg_struct.num_ka_min = runprof_table_shr_mem[vptr->group_num].gset.num_ka_min;

  //set max_ka
  if((runprof_table_shr_mem[vptr->group_num].gset.num_ka_min == 999999999)
		&&
    (runprof_table_shr_mem[vptr->group_num].gset.num_ka_range == 0))
    out_msg_struct.max_ka = 0;
  else {
    out_msg_struct.max_ka =  ((runprof_table_shr_mem[vptr->group_num].gset.num_ka_range) +
                              (runprof_table_shr_mem[vptr->group_num].gset.num_ka_min));
  }

  //save ua string. (Maximum length of ua string will be 32000bytes)
  static char ua_input_buffer[32000+1];
  int ret;
  ret = ns_get_ua_string_ext(ua_input_buffer, 32000, vptr);
  if(ret < 0)
    out_msg_struct.ua_string = NULL;
  else {
    out_msg_struct.ua_string = ua_input_buffer;
    out_msg_struct.ua_string[ret] = 0; //no need for this.
  }

  NSDL2_API(vptr, NULL, "out_msg_struct.user_id = %d, out_msg_struct.sess_idx = %d, out_msg_struct.script_name = %s,"
                                " out_msg_struct.num_ka_pct = %d, out_msg_struct.max_ka = %d, out_msg_struct.ua_string = %s",
                                  out_msg_struct.user_id, out_msg_struct.sess_idx, out_msg_struct.script_name, out_msg_struct.num_ka_pct,
                                  out_msg_struct.max_ka, out_msg_struct.ua_string);

  //pack the result
  *out_msg_len = ns_msg__start_user__get_packed_size(&out_msg_struct);

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  //TODO: This length should be compared with maximum available length for out buffer.
  if(*out_msg_len != ns_msg__start_user__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}

int stop_user_create_msg(Msg_com_con* mccptr, int *out_msg_len)
{
  char *out_msg;

  NsMsg2II out_msg_struct = NS_MSG2__II__INIT;
  VUser *vptr = mccptr->vptr;

  NSDL2_API(vptr, NULL, "Method called");
  out_msg_struct.field1 = vptr->user_index;
  out_msg_struct.field2 = vptr->sess_inst;

  *out_msg_len = ns_msg2__ii__get_packed_size(&out_msg_struct);

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg2__ii__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}


//This method will handle bind request recieved from njvm. If recieved status is 1(fail) then send end_test_run message to NJVM
static int njvm_bind_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *msg;
  int len;
  char *out_msg;

  NsMsg1Int *in_msg_struct;
  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;

  NSDL2_API(NULL, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message
  if((in_msg_struct = ns_msg1__int__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }
  //These declaration can be avoid
  int status = in_msg_struct->field1;

  //free unpacked data
  ns_msg1__int__free_unpacked(in_msg_struct, NULL);

  //If recieved status is not success then call end_test_run.
  if(status){
    //This method will send end test run message to njvm.
    //njvm_end_test_run();  //do we need to send this message.
    //TODO: handle this case properly.
    end_test_run();
    return -1;
  }

  out_msg_struct.field1 = status;

  //pack this message
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}



/********These are not handler *******/


//Handle start user response
int handle_njvm_start_user_response(Msg_com_con* mccptr)
{
  char *msg;
  int len;
  //VUser *vptr = mccptr->vptr;

  NsMsg1Int *in_msg_struct;
  NSDL2_MESSAGES(NULL, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message
  if((in_msg_struct = ns_msg1__int__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }

  int status = in_msg_struct->field1;
  ns_msg1__int__free_unpacked(in_msg_struct, NULL);

  NSDL2_MESSAGES(NULL, NULL, "status = %d", status);

  if(status != 0) {
    NSDL2_MESSAGES(NULL, NULL, "njvm_thread(%s), Failed to start user, currently stoping test.", msg_com_con_to_str(mccptr));
    fprintf(stderr, "njvm thread(%s), Failed to start user, currently stoping test.", msg_com_con_to_str(mccptr));
    //TODO: Handle this case (close this connection and move this to ceased thread.)
    close_njvm_msg_com_con(mccptr);
    //end_test_run();
    return -1;
  }
  return 0;
}

int handle_njvm_increase_thread_rep(Msg_com_con *mccptr)
{
  char *msg;
  int len;
  //VUser *vptr = mccptr->vptr;

  NsMsg1Int *in_msg_struct;
  NSDL2_MESSAGES(NULL, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message
  if((in_msg_struct = ns_msg1__int__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }

  NSDL2_MESSAGES(NULL, NULL, "njvm_thread(%s), cummulative thread count = %d", in_msg_struct->field1);

  ns_msg1__int__free_unpacked(in_msg_struct, NULL);

  return 0;
}

int handle_njvm_stop_user_response(Msg_com_con *mccptr)
{
  char *msg;
  int len;
  //VUser *vptr = mccptr->vptr;

  NsMsg1Int *in_msg_struct;
  NSDL2_MESSAGES(NULL, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message
  if((in_msg_struct = ns_msg1__int__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }

  int status = in_msg_struct->field1;
  ns_msg1__int__free_unpacked(in_msg_struct, NULL);

  if(status != 0) {
    NSDL2_MESSAGES(NULL, NULL, "njvm_thread(%s), Failed to stop user, stoping test.");
    fprintf(stderr, "njvm thread(%s), Failed to stop user, stoping test.\n", msg_com_con_to_str(mccptr));
    //TODO: Handle this case (close this connection and move this to ceased thread.)
    end_test_run();
    return -1;
  }
  return 0;
}

int handle_njvm_error_message(Msg_com_con *mccptr)
{
  char *msg;
  int len;
  //VUser *vptr = mccptr->vptr;

  NsMsg2IS *in_msg_struct;
  NSDL2_MESSAGES(NULL, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message
  if((in_msg_struct = ns_msg2__is__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }

  int err_code = in_msg_struct->field1;
  char *err_msg = in_msg_struct->field2;

  //free unpacked message
  ns_msg2__is__free_unpacked(in_msg_struct, NULL);


  //TODO: Handle this case properly.
  NSDL2_MESSAGES(NULL, NULL, "njvm(%s), error occured, error code - %3d, error message = %s", msg_com_con_to_str(mccptr), err_code, err_msg);
  fprintf(stderr, "njvm(%s), error occured, error code - %3d, error message = %s\n", msg_com_con_to_str(mccptr), err_code, err_msg);
  return 0;
}

int increase_thread_pool_create_msg(Msg_com_con *mccptr, int cum_thread_count, int *out_msg_len)
{
  char *out_msg;

  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;

  NSDL2_API(NULL, NULL, "Method called");

  out_msg_struct.field1 = cum_thread_count;

  //pack this message
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}

//create message for ns_web_url response
int ns_web_url_resp_create_msg(Msg_com_con *mccptr, int ret_value, int *out_msg_len)
{
  char *out_msg;

  VUser* vptr = mccptr->vptr;
  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;

  NSDL2_API(NULL, NULL, "Method called return value is [%d] ", ret_value);

  if (ret_value == 0 ){
    out_msg_struct.field1 = vptr->page_instance;
    NSDL2_API(NULL, NULL, "vptr->page_instance is [%d] \n ", vptr->page_instance);
  }
  else {
    out_msg_struct.field1 = ret_value;
  }

  NSDL2_API(NULL, NULL, "out_msg_struct.field1 is  [%d] \n ", out_msg_struct.field1);
  //pack this message
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}

//create message for ns_page_think_time api
int ns_page_think_time_resp_create_msg(Msg_com_con *mccptr, int ret_value, int *out_msg_len)
{
  char *out_msg;

  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;

  NSDL2_API(NULL, NULL, "Method called");

  out_msg_struct.field1 = ret_value;

  //pack this message
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}


//create message for ns_web_websocket_send response api
int ns_web_websocket_send_resp_create_msg(Msg_com_con *mccptr, int ret_value, int *out_msg_len)
{
  char *out_msg;

  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;

  NSDL2_API(NULL, NULL, "Method called");

  out_msg_struct.field1 = ret_value;
      
  //pack this message
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);
    
  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);
    
  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}


//create message for ns_web_websocket_read response api
int ns_web_websocket_read_resp_create_msg(Msg_com_con *mccptr, int ret_value, int *out_msg_len)
{
  char *out_msg;
  VUser *vptr = mccptr->vptr; 

  NsMsg1Str out_msg_struct = NS_MSG1__STR__INIT;

  NSDL2_API(NULL, NULL, "Method called");

  ret_value = vptr->ws_status;
  out_msg_struct.field1 = NULL;

  if(!ret_value) //If timer is not expire
    out_msg_struct.field1 = vptr->url_resp_buff;
 
  if(ret_value)
  {
    vptr->ws_status = NS_REQUEST_NO_READ;
    ws_avgtime->ws_error_codes[NS_REQUEST_NO_READ]++;
    NSDL2_API(vptr, NULL, "ws_avgtime->ws_error_codes[%d] = %llu", vptr->ws_status, ws_avgtime->ws_error_codes[NS_REQUEST_NO_READ]);
    INC_WS_MSG_READ_FAIL_COUNTER(vptr);   //Updated avg counter for failed msg
  }

  //pack this message
  *out_msg_len = ns_msg1__str__get_packed_size(&out_msg_struct);

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__str__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}



//create message for ns_web_websocket_close response api
int ns_web_websocket_close_resp_create_msg(Msg_com_con *mccptr, int ret_value, int *out_msg_len)
{
  char *out_msg;

  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;

  NSDL2_API(NULL, NULL, "Method called");

  out_msg_struct.field1 = ret_value;

  //pack this message
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}

//create message for ns_click_api_resp response api
int ns_click_api_resp_create_msg(Msg_com_con *mccptr, int ret_value, int *out_msg_len)
{
  char *out_msg;

  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;

  NSDL2_API(NULL, NULL, "Method called");

  out_msg_struct.field1 = ret_value;
      
  //pack this message
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);
    
  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);
    
  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}

//create message for ns_end_session response api
int ns_end_session_resp_create_msg(Msg_com_con *mccptr, int ret_value, int *out_msg_len)
{
  char *out_msg;

  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;

  NSDL2_API(NULL, NULL, "Method called");

  out_msg_struct.field1 = ret_value;

  //pack this message
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}

//create message for ns_sync_point api 
int ns_sync_point_resp_create_msg(Msg_com_con *mccptr, int ret_value, int *out_msg_len)
{   
  char *out_msg;

  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
      
  NSDL2_API(NULL, NULL, "Method called");

  out_msg_struct.field1 = ret_value;
  
  //pack this message
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);
         
  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);
  
  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}


int ns_start_tx_resp_create_msg(Msg_com_con *mccptr, int ret_value, int *out_msg_len)
{
  char *out_msg;
      
  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT; 
    
  NSDL2_API(NULL, NULL, "Method called");
      
  out_msg_struct.field1 = ret_value;
    
  //pack this message
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);
    
  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);
    
  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}

int ns_get_double_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *msg;
  char len;
  char *out_msg;

  NsMsg1Str *in_msg_struct;
  NsMsg1Double out_msg_struct = NS_MSG1__DOUBLE__INIT;

  IW_UNUSED(VUser *vptr = mccptr->vptr);

  NSDL2_API(vptr, NULL, "Method called");

  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message.
  if((in_msg_struct = ns_msg1__str__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }

  //call api and save it's result
  out_msg_struct.field1 = ns_get_double_val(in_msg_struct->field1);

  //pack this message
  *out_msg_len = ns_msg1__double__get_packed_size(&out_msg_struct);
  //TODO: This length should be compared with maximum available length for out buffer.

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__double__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;

}

int ns_set_double_val_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *msg;
  char len;
  char *out_msg;

  NsMsg2SD *in_msg_struct;
  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
  VUser *vptr = mccptr->vptr;

  NSDL2_API(vptr, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message.
  if((in_msg_struct = ns_msg2__sd__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }

  //call api and save it's result
  char buf[32];
  sprintf(buf, "%f", in_msg_struct->field2);

  out_msg_struct.field1 = 0;
  out_msg_struct.field1 = ns_save_string_flag_internal(buf, -1, in_msg_struct->field1, 0, vptr, 1);

  //free unpacked msg
  ns_msg2__sd__free_unpacked(in_msg_struct, NULL);

  //pack this message
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);
  //TODO: This length should be compared with maximum available length for out buffer.

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}

int ns_process_njvm_jrmi_resp_buffer_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *msg;
  int len;
  char *out_msg;
  VUser* vptr = mccptr->vptr;

  NsMsg1Int *in_msg_struct;
  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;

  NSDL2_API(NULL, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message
  if((in_msg_struct = ns_msg1__int__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }

  //free unpacked data
  ns_msg1__int__free_unpacked(in_msg_struct, NULL);

  out_msg_struct.field1 = process_njvm_resp_buffer(mccptr, NULL, 0, vptr, in_msg_struct->field1);

  //pack this message
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}

static int ns_process_njvm_jrmi_step_start_func(Msg_com_con* mccptr, int *out_msg_len){

  char *msg;
  int len;
  char *out_msg;
  VUser* vptr = mccptr->vptr;

  u_ns_ts_t now = get_ms_stamp();

  NsMsg1Int *in_msg_struct;
  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
  NSDL2_API(NULL, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message
  if((in_msg_struct = ns_msg1__int__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }

  //free unpacked data
  ns_msg1__int__free_unpacked(in_msg_struct, NULL);

  vptr->next_pg_id = in_msg_struct->field1;
  vptr->cur_page = vptr->sess_ptr->first_page + vptr->next_pg_id;
  NSDL2_API(NULL, NULL, "vptr->next_page_id [%d] , vptr->cur_page->page_id [%u] ", vptr->next_pg_id, vptr->cur_page->page_id);
  vptr->flags |= NS_JNVM_JRMI_RESP;

  on_page_start(vptr, now);
  out_msg_struct.field1 = vptr->page_instance;  //sending page instance in output
  NSDL2_API(NULL, NULL, "out_msg_struct.field1 [%d] ", out_msg_struct.field1);
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);
  //TODO: This length should be compared with maximum available length for out buffer.

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}

int ns_process_njvm_jrmi_step_end_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *msg;
  char len;
  char *out_msg;

  NsMsg2SI *in_msg_struct;
  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
  VUser *vptr = mccptr->vptr;

  NSDL2_API(vptr, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message.
  if((in_msg_struct = ns_msg2__si__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }

  out_msg_struct.field1 = process_njvm_resp_buffer(mccptr, in_msg_struct->field1, in_msg_struct->field2, vptr, -1);

  //free unpacked msg
  ns_msg2__si__free_unpacked(in_msg_struct, NULL);

  //pack this message
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}


static int ns_start_timer_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *msg, *out_msg, *timer_name;
  NsMsg1Str *in_msg_struct;
  NsMsg1Long out_msg_struct = NS_MSG1__LONG__INIT;
  IW_UNUSED(VUser *vptr = mccptr->vptr);
  int len;

  NSDL2_API(vptr, NULL, "Method called");

  // Get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  // Unpack message
  if((in_msg_struct = ns_msg1__str__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }
  timer_name = in_msg_struct->field1;

  out_msg_struct.field1 = ns_start_timer(timer_name);

  // Free unpacked data
  ns_msg1__str__free_unpacked(in_msg_struct, NULL);

  // Pack out message
  *out_msg_len = ns_msg1__long__get_packed_size(&out_msg_struct);

  // Now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__long__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}


static int ns_end_timer_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *msg, *out_msg , *timer_name;
  NsMsg1Str *in_msg_struct;
  NsMsg1Long out_msg_struct = NS_MSG1__LONG__INIT;
  IW_UNUSED(VUser *vptr = mccptr->vptr);
  int len;

  NSDL2_API(vptr, NULL, "Method called");

  // Get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  // Unpack message
  if((in_msg_struct = ns_msg1__str__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }
  timer_name = in_msg_struct->field1;

  out_msg_struct.field1 = ns_end_timer(timer_name);
  NSDL2_API(vptr, NULL, "Timer elapsed at  [%ld]", out_msg_struct.field1);

  // Free unpacked data
  ns_msg1__str__free_unpacked(in_msg_struct, NULL);

  // Pack out message
  *out_msg_len = ns_msg1__long__get_packed_size(&out_msg_struct);

  // Now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if (*out_msg_len != ns_msg1__long__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}

/*---------------------------------------------------------------------------------------------
       * Function Name  : ns_rbu_page_stats_msg_proc_func()
       *
       * Purpose             :This function will unpack message coming form java-type script, then handle the message and
       *                      parse the message using parse_and_set_rbu_page_stat()
       *
       * Input               : mccptr pointer of structure Msg_com_con,
       *                       Msg_com_con is Structure for Message communication connection for child, parent and master
       *
*--------------------------------------------------------------------------------------------*/
static int ns_rbu_page_stats_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *msg, *input_msg, *out_msg ;
  NsMsg1Str *in_msg_struct;
  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
  VUser *vptr = mccptr->vptr;
  int len;

  NSDL2_API(vptr, NULL, "Method called");

  // Get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  // Unpack message
  if((in_msg_struct = ns_msg1__str__unpack(NULL, len, (void *)msg)) == NULL)
  {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }

  input_msg = in_msg_struct->field1;

  out_msg_struct.field1 = 0;//default value

  //This function will parse the data and fill the data in RBU_RespAttr structure
  parse_and_set_rbu_page_stat(vptr, input_msg);

  //free unpacked data
  ns_msg1__str__free_unpacked(in_msg_struct, NULL);

  //pack the result
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);
  //TODO: This length should be compared with maximum available length for out buffer.

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}

//API to return current partition  id

int ns_get_cur_partition_msg_proc_function(Msg_com_con* mccptr, int *out_msg_len)
{
  char *out_msg;
  //long ns_cur_partition;
  NSDL2_API(NULL, NULL, "Method called");

  NsMsg1Long out_msg_struct = NS_MSG1__LONG__INIT;
  //VUser *vptr = mccptr->vptr;

  // Getting current partation

  out_msg_struct.field1 = ns_get_cur_partition() ;
  NSDL2_API(NULL, NULL, "current partition is  [%ld]", out_msg_struct.field1);


  // Pack out message
  *out_msg_len = ns_msg1__long__get_packed_size(&out_msg_struct);

  // Now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if (*out_msg_len != ns_msg1__long__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}

// Api to get host id
static int ns_get_host_id_msg_proc_function(Msg_com_con* mccptr, int *out_msg_len)
{
  char *out_msg;
  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
  NSDL2_API(NULL, NULL, "Method called");

  out_msg_struct.field1 = ns_get_host_name();
  
  NSDL2_API(NULL, NULL, "host name is [%d] ", out_msg_struct.field1);
  
  //pack this message
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}

// Api to replay db queries
static int ns_db_replay_query_msg_proc_fuction(Msg_com_con* mccptr, int *out_msg_len)
{

  char *out_msg;
  char *query_param = NULL;
  NsMsg3IIS out_msg_struct = NS_MSG3__IIS__INIT;
  NSDL2_API(NULL, NULL, "Method called");

  // TODO: Rellaoc it, paramter size will be flexible
  MY_MALLOC(query_param, 5120, "query_param", -1);

  // Get query id from query table, with paramter values seprate by \n
  ns_db_replay_query(&out_msg_struct.field1, &out_msg_struct.field2, query_param);

  out_msg_struct.field3 = query_param;
  NSDL2_API(NULL, NULL, "query id is  [%d] ,  number_of_param = [%d] , string is [%s]", out_msg_struct.field1, out_msg_struct.field2, out_msg_struct.field3);

  // pack this message
  *out_msg_len = ns_msg3__iis__get_packed_size(&out_msg_struct);

  // now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg3__iis__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  FREE_AND_MAKE_NULL(query_param, "query_param", -1);
  return 0;
}

// Api to end transaction from database
static int ns_db_replay_query_end_msg_proc_function(Msg_com_con* mccptr, int *out_msg_len)
{
  char *out_msg, *msg;
  NsMsg3IIS *in_msg_struct;
  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
  int len;

  NSDL2_API(NULL, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message
  if((in_msg_struct = ns_msg3__iis__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }
  NSDL2_API(NULL, NULL, "query id is  [%d] and transction status is [%d] msg = %s ", in_msg_struct->field1, in_msg_struct->field2, in_msg_struct->field3);

  out_msg_struct.field1 = ns_db_replay_query_end(in_msg_struct->field1, in_msg_struct->field2, in_msg_struct->field3);
  NSDL2_API(NULL, NULL, "status is  [%d] ", out_msg_struct.field1);

  //free unpacked data
  ns_msg3__iis__free_unpacked(in_msg_struct, NULL);

  //pack this message
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}

/* --------------------------------------------------------------
  Name          : ns_rbu_setting_function()
  Purpose       : return rbu settings to NJVM
----------------------------------------------------------------*/
static int ns_rbu_setting_function(Msg_com_con* mccptr, int *out_msg_len)
{
  char *msg, *out_msg, buffer[1024] = "";
  int len;

  NsMsg1Int *in_msg_struct;
  NsMsg1Str out_msg_struct = NS_MSG1__STR__INIT;
  NSDL2_API(NULL, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message
  if((in_msg_struct = ns_msg1__int__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }

  //get rbu settings
  ns_get_rbu_settings(in_msg_struct->field1, buffer);
  out_msg_struct.field1 = buffer;

  //free unpacked data
  ns_msg1__int__free_unpacked(in_msg_struct, NULL);

  NSDL2_API(NULL, NULL, "out_msg_struct.field1 = %s ", out_msg_struct.field1);
  *out_msg_len = ns_msg1__str__get_packed_size(&out_msg_struct);
  //TODO: This length should be compared with maximum available length for out buffer.

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__str__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}

//api ns_start_transaction_ex
int ns_start_transaction_ex_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
    char *out_msg, *msg;
    int  len;

    /* Initialize output message */
    NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
    NsMsg1Str *in_msg_struct;

    /* Get the vptr from mccptr */
    VUser *vptr = mccptr->vptr;

    NSDL2_API(vptr, NULL, "Method called");

    // Get message from mccptr
    GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

    // unpack message
    if((in_msg_struct = ns_msg1__str__unpack(NULL, len, (void *)msg)) == NULL)
    {
        fprintf(stderr, "Invalid message recieved\n");
        return -1;
    }

    out_msg_struct.field1 = tx_start_with_name(ns_eval_string(in_msg_struct->field1), vptr);

    // Free unpacked message
    ns_msg1__str__free_unpacked(in_msg_struct, NULL);

    /* Get the size of the message to be packed */
    *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

    /* Get buffer to fill message. */
    GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

    /* Packed the message if failure return -1 else 0 */
    if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg))
    {
        fprintf(stderr, "Failed to pack complete message\n");
        return -1;
    }
    return 0;
}


//api ns_enable_soap_ws_Security
int ns_enable_soap_ws_security_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
    char *out_msg;

    NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;

    NSDL2_API(NULL, NULL, "Method called");

    mccptr->vptr->flags |= NS_SOAP_WSSE_ENABLE;

    // Pack the result
    *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

    // Now get buffer to fill message.
    GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

    if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg))
    {
        fprintf(stderr, "Failed to pack complete message\n");
        return -1;
    }
    return 0;
}

//api ns_disable_soap_ws_security
int ns_disable_soap_ws_security_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
    char *out_msg;

    NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;

    NSDL2_API(NULL, NULL, "Method called");
    mccptr->vptr->flags &= ~NS_SOAP_WSSE_ENABLE;

    // Pack the result
    *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

    // Now get buffer to fill message.
    GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

    if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg))
    {
        fprintf(stderr, "Failed to pack complete message\n");
        return -1;
    }
    return 0;
}

//api ns_soap_ws_security
int ns_soap_ws_security_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
    //char *msg;
   // int len;
    char *out_msg;

  //  NsMsg9SSISSSSSS *in_msg_struct;
    NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;

    NSDL2_API(NULL, NULL, "Method called");
    // Get message from mccptr
   // GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

    // Unpack message
//    if((in_msg_struct = ns_msg9__ssissssss__unpack(NULL, len, (void *)msg)) == NULL)
  //  {
  //      fprintf(stderr, "Invalid message recieved\n");
   //     return -1;
   // }
  //  out_msg_struct.field1 = ns_soap_ws_security(in_msg_struct->field1, in_msg_struct->field2, in_msg_struct->field3, in_msg_struct->field4, in_msg_struct->field5, in_msg_struct->field6, in_msg_struct->field7, in_msg_struct->field8, in_msg_struct->field9);

    // Free unpacked data
   // ns_msg2__ssissssss__free_unpacked(in_msg_struct, NULL);

    // Pack the result
    *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

    // Now get buffer to fill message.
    GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

    if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg))
    {
        fprintf(stderr, "Failed to pack complete message\n");
        return -1;
    }
    return 0;
}

//api ns_unset_ssl_settings_msg_proc_func
int ns_unset_ssl_settings_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
    char *out_msg;

    NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;

    VUser *vptr = mccptr->vptr;

    NSDL2_API(vptr, NULL, "Method called");

    // Call api
    ns_unset_ssl_setting_ex(vptr);

    out_msg_struct.field1 = 0;

    // Pack the result
    *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

    // Now get buffer to fill message.
    GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

    if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg))
    {
        fprintf(stderr, "Failed to pack complete message\n");
        return -1;
    }
    return 0;
}
//api ns_set_ssl_settings
int ns_set_ssl_settings_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
    char *msg;
    int len;
    char *out_msg;

    NsMsg2SS *in_msg_struct;
    NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;

    VUser *vptr = mccptr->vptr;

    NSDL2_API(vptr, NULL, "Method called");

    // Get message from mccptr
    GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

    // Unpack message
    if((in_msg_struct = ns_msg2__ss__unpack(NULL, len, (void *)msg)) == NULL)
    {
        fprintf(stderr, "Invalid message recieved\n");
        return -1;
    }

    // Call APi and save its result
    ns_set_ssl_setting_ex(vptr, in_msg_struct->field1, in_msg_struct->field2);
    out_msg_struct.field1 = 0;

    // Free unpacked data
    ns_msg2__ss__free_unpacked(in_msg_struct, NULL);

    // Pack the result
    *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

    // Now get buffer to fill message.
    GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

    if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg))
    {
        fprintf(stderr, "Failed to pack complete message\n");
        return -1;
    }
    return 0;
}

//api ns_set_referer
int ns_set_referer_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
    char *out_msg, *msg;
    int len;

    /* Initialize output message */
    NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
    NsMsg1Str *in_msg_struct;

    NSDL2_API(NULL, NULL, "Method called");

    // Get message from mccptr
    GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

    // unpack message
    if((in_msg_struct = ns_msg1__str__unpack(NULL, len, (void *)msg)) == NULL)
    {
        fprintf(stderr, "Invalid message recieved\n");
        return -1;
    }

    /* Call Api and save its result */
    out_msg_struct.field1 = ns_set_referer(in_msg_struct->field1);

    // Free unpacked message
    ns_msg1__str__free_unpacked(in_msg_struct, NULL);

    /* Get the size of the message to be packed */
    *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

    /* Get buffer to fill message. */
    GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

    /* Packed the message if failure return -1 else 0 */
    if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg))
    {
        fprintf(stderr, "Failed to pack complete message\n");
        return -1;
    }
    return 0;
}

//api ns_get_referer
int ns_get_referer_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
    char *out_msg;

    VUser *vptr = mccptr->vptr;

    /* Initialize output message */
    NsMsg1Str out_msg_struct = NS_MSG1__STR__INIT;

    NSDL2_API(vptr, NULL, "Method called");

    /* Call Api and save its result */
    out_msg_struct.field1 = vptr->referer;

    /* Get the size of the message to be packed */
    *out_msg_len = ns_msg1__str__get_packed_size(&out_msg_struct);

    /* Get buffer to fill message. */
    GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

    /* Packed the message if failure return -1 else 0 */
    if(*out_msg_len != ns_msg1__str__pack(&out_msg_struct, (void *)out_msg))
    {
        fprintf(stderr, "Failed to pack complete message\n");
        return -1;
    }
    return 0;
}

//api ns_get_schedule_phase_name
int ns_get_schedule_phase_name_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
    char *out_msg;

    /* Get the vptr from mccptr */
    VUser *vptr = mccptr->vptr;

    /* Initialize output message */
    NsMsg1Str out_msg_struct = NS_MSG1__STR__INIT;

    NSDL2_API(vptr, NULL, "Method called");

    /* Call Api and save its result */
    out_msg_struct.field1 = ns_get_schedule_phase_name(vptr);

    /* Get the size of the message to be packed */
    *out_msg_len = ns_msg1__str__get_packed_size(&out_msg_struct);

    /* Get buffer to fill message. */
    GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

    /* Packed the message if failure return -1 else 0 */
    if(*out_msg_len != ns_msg1__str__pack(&out_msg_struct, (void *)out_msg))
    {
        fprintf(stderr, "Failed to pack complete message\n");
        return -1;
    }
    return 0;
}

//api ns_end_transaction_ex
int ns_end_transaction_ex_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
    char *msg;
    int len;
    char *out_msg;

    NsMsg2SI *in_msg_struct;
    /* Initialize output message */
    NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;

    VUser *vptr = mccptr->vptr;

    NSDL2_API(vptr, NULL, "Method called");

    // Get message from mccptr
    GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

    // Unpack message
    if((in_msg_struct = ns_msg2__si__unpack(NULL, len, (void *)msg)) == NULL)
    {
        fprintf(stderr, "Invalid message recieved\n");
        return -1;
    }

    out_msg_struct.field1 = tx_end(ns_eval_string(in_msg_struct->field1), in_msg_struct->field2, vptr);

    //free unpacked data
    ns_msg2__si__free_unpacked(in_msg_struct, NULL);

    //pack the result
    *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

    //now get buffer to fill message.
    GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

    if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg))
    {
        fprintf(stderr, "Failed to pack complete message\n");
        return -1;
    }
    return 0;
}

//api ns_get_schedule_phase_type
int ns_get_schedule_phase_type_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
    char *out_msg;

    /* Get the vptr from mccptr */
    VUser *vptr = mccptr->vptr;

    /* Initialize output message */
    NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;

    NSDL2_API(vptr, NULL, "Method called");

    /* Call Api and save its result */
    out_msg_struct.field1 = get_schedule_phase_type_int(vptr);

    /* Get the size of the message to be packed */
    *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

    /* Get buffer to fill message. */
    GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

    /* Packed the message if failure return -1 else 0 */
    if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg))
    {
        fprintf(stderr, "Failed to pack complete message\n");
        return -1;
    }
    return 0;
}

//api ns_get_ua_string_msg_proc_func
int ns_get_ua_string_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
    char *out_msg, *msg;
    int len;

    /* Initialize output message */
    NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
    NsMsg2SI *in_msg_struct;

    /* Get the vptr from mccptr */
    VUser *vptr = mccptr->vptr;

    NSDL2_API(vptr, NULL, "Method called");

    // Get message from mccptr
    GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

    // unpack message
    if((in_msg_struct = ns_msg2__si__unpack(NULL, len, (void *)msg)) == NULL)
    {
        fprintf(stderr, "Invalid message recieved\n");
        return -1;
    }

    /* Call Api and save its result */
    out_msg_struct.field1 = ns_get_ua_string_ext(in_msg_struct->field1, in_msg_struct->field2, vptr);

    // Free unpacked message
    ns_msg2__si__free_unpacked(in_msg_struct, NULL);

    /* Get the size of the message to be packed */
    *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

    /* Get buffer to fill message. */
    GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

    /* Packed the message if failure return -1 else 0 */
    if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg))
    {
        fprintf(stderr, "Failed to pack complete message\n");
        return -1;
    }
    return 0;
}

//api ns_get_replay_page_msg_proc_func
int ns_get_replay_page_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
    char *out_msg;

    // Initialize output message
    NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;

    NSDL2_API(NULL, NULL, "Method called");

    // Get the vptr from mccptr
    VUser *api_vptr = mccptr->vptr;

    // Call Api and save its result
    out_msg_struct.field1  = ns_get_replay_page_ext(api_vptr);

    if(api_vptr->sess_ptr->num_pages <= out_msg_struct.field1)
    {
        NS_EXIT(-1, "Error: Script has less page (%d <= %d).\n", out_msg_struct.field1, api_vptr->sess_ptr->num_pages);
    }

    // Get the size of the message to be packed
    *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

    // Get buffer to fill message.
    GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

    // Packed the message if failure return -1 else 0
    if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg))
    {
        fprintf(stderr, "Failed to pack complete message\n");
        return -1;
    }
    return 0;
}

// api ns_end_transaction_as_Ex
int ns_end_transaction_as_ex_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
    char *msg;
    int len;
    char *out_msg;

    NsMsg3SIS *in_msg_struct;
    NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;

    NSDL2_API(NULL, NULL, "Method called");

    // Get message from mccptr
    GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

    // Unpack message
    if((in_msg_struct = ns_msg3__sis__unpack(NULL, len, (void *)msg)) == NULL)
    {
        fprintf(stderr, "Invalid message recieved\n");
        return -1;
    }

    char *tx_name = in_msg_struct->field1;
    int status = in_msg_struct->field2;
    char *end_tx_name = in_msg_struct->field3;

    //out_msg_struct.field1 = tx_end_as(ns_eval_string(tx_name), status, ns_eval_string(end_tx_name), vptr);
      out_msg_struct.field1 = ns_end_transaction_as_ex(tx_name,status,end_tx_name);
   
    // Free unpacked data
    ns_msg3__sis__free_unpacked(in_msg_struct, NULL);

    // Pack the result
    *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

    // Now get buffer to fill message.
    GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

    if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg))
    {
        fprintf(stderr, "Failed to pack complete message\n");
        return -1;
    }
    return 0;
}

//api ns_update_user_flow
int ns_update_user_flow_count_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
    char *out_msg, *msg;
    int len;

    /* Initialize output message */
    NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
    NsMsg1Int *in_msg_struct;

    /* Get the vptr from mccptr */
    VUser *vptr = mccptr->vptr;

    NSDL2_API(vptr, NULL, "Method called");

    // Get message from mccptr
    GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

    // unpack message
    if((in_msg_struct = ns_msg1__int__unpack(NULL, len, (void *)msg)) == NULL)
    {
        fprintf(stderr, "Invalid message recieved\n");
        return -1;
    }

    /* Call Api and save its result */
    update_user_flow_count_ex(vptr,in_msg_struct->field1);
    out_msg_struct.field1 = 0;

    // Free unpacked message
    ns_msg1__int__free_unpacked(in_msg_struct, NULL);

    /* Get the size of the message to be packed */
    *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

    /* Get buffer to fill message. */
    GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

    /* Packed the message if failure return -1 else 0 */
    if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg))
    {
        fprintf(stderr, "Failed to pack complete message\n");
        return -1;
    }
 return 0;
}

//api ns_sockjs_close

int ns_sockjs_close_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
    char *out_msg, *msg;
    int len;

    /* Initialize output message */
    NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
    NsMsg1Int *in_msg_struct;
    VUser *vptr = mccptr->vptr;

    NSDL2_API(vptr, NULL, "Method called");

    // Get message from mccptr
    GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

    // Unpack message
    if((in_msg_struct = ns_msg1__int__unpack(NULL, len, (void *)msg)) == NULL)
    {
        fprintf(stderr, "Invalid message recieved\n");
        return -1;
    }
     
    vptr->sockjs_close_id = in_msg_struct->field1;
   
    /* Call Api and save its result */
    out_msg_struct.field1 = nsi_sockjs_close(vptr);

    // Free unpacked message
    ns_msg1__int__free_unpacked(in_msg_struct, NULL);

    /* Get the size of the message to be packed */
    *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

    /* Get buffer to fill message. */
    GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

    /* Packed the message if failure return -1 else 0 */
    if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg))
    {
        fprintf(stderr, "Failed to pack complete message\n");
        return -1;
    }
    return 0;
}

//api ns_save_binary_val
int ns_save_binary_val_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *msg;
  char len;
  char *out_msg;

  NsMsg3SIS *in_msg_struct;
  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;

  VUser *vptr = mccptr->vptr;

  NSDL2_API(vptr, NULL, "Method called");

  // Get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

   //unpack message
   if((in_msg_struct = ns_msg3__sis__unpack(NULL, len, (void *)msg)) == NULL) {
       fprintf(stderr, "Invalid message recieved\n");
       return -1;
    }

    // call api and save it's result
   out_msg_struct.field1 = ns_save_string_flag_internal(in_msg_struct->field1, in_msg_struct->field2, in_msg_struct->field3, 0, vptr, 0);

  //free unpacked message
  ns_msg3__sis__free_unpacked(in_msg_struct, NULL);

 //pack this message
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}

//api ns_web_websocket_check
int ns_web_websocket_check_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
    char *out_msg, *msg;
    int len;

    /* Initialize output message */
    NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
    NsMsg3SSI *in_msg_struct;

    NSDL2_API(NULL, NULL, "Method called");

    // Get message from mccptr
    GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

    // unpack message
    if((in_msg_struct = ns_msg3__ssi__unpack(NULL, len, (void *)msg)) == NULL)
    {
        fprintf(stderr, "Invalid message recieved\n");
        return -1;
    }

    /* Call Api and save its result */
    out_msg_struct.field1 = ns_web_websocket_check(in_msg_struct->field1, in_msg_struct->field2, in_msg_struct->field3);

    // Free unpacked message
    ns_msg3__ssi__free_unpacked(in_msg_struct, NULL);

    /* Get the size of the message to be packed */
    *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

    /* Get buffer to fill message. */
    GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

    /* Packed the message if failure return -1 else 0 */
    if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg))
    {
        fprintf(stderr, "Failed to pack complete message\n");
        return -1;
    }
    return 0;
}

//api ns_web_websocket_read
/* 
NOTE: JAVA doesn't support the concept of output parameters like C, 
      so in case of JTS size of the websocket message can't be retrieved. 
      Therefore, in case of JTS nsi_web_websocket_readshould be called as:
  
      message = nsApi.ns_web_websocket_read(con_id,timeout,resp_sz);

 		NOT LIKE "C"

      message = ns_web_websocket_read(con_id,timeout,&resp_sz); 

     whereas incase of JTS,
	resp_sz = can be integer value (0,1,24,100,etc)
*/                                               
int ns_web_websocket_read_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
    char *msg;
    int len;
    int ret;

    NsMsg3III *in_msg_struct;

    /* Get the vptr from mccptr */
    VUser *vptr = mccptr->vptr;

    NSDL2_API(NULL, NULL, "Method called");

    // Get message from mccptr
    GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

    // unpack message
    if((in_msg_struct = ns_msg3__iii__unpack(NULL, len, (void *)msg)) == NULL)
    {
        fprintf(stderr, "Invalid message recieved\n");
        return -1;
    }
     
    int conn_id = in_msg_struct->field1;
    int timeout = in_msg_struct->field2;
    // int length  = in_msg_struct->field3;

    // Free unpacked message
    ns_msg3__iii__free_unpacked(in_msg_struct, NULL);
    
     /* Initialize ws_status for read timer */
    vptr->ws_status = 0; 

    /* Call Api and save its result */
    ret = nsi_web_websocket_read(vptr,conn_id, timeout);
    if(ret != 0)
    {
       fprintf(stderr, "Failed in reading nsi_websocket_read\n");
       return -1;
    }

    /* Get the size of the message to be packed */
    //*out_msg_len = ns_msg1__str__get_packed_size(&out_msg_struct);

    /* Get buffer to fill message. */
   // GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

    /* Packed the message if failure return -1 else 0 */
   // if(*out_msg_len != ns_msg1__str__pack(&out_msg_struct, (void *)out_msg))
   // {
   //     fprintf(stderr, "Failed to pack complete message\n");
   //     return -1;
   // }
    return 1; // Wait for response from NVM
}

//api ns_web_websocket_search_msg_proc_func
int ns_web_websocket_search_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
    char *out_msg, *msg;
    int len;

    /* Initialize output message */
    NsMsg1Str out_msg_struct = NS_MSG1__STR__INIT;
    NsMsg3SSS *in_msg_struct;

    NSDL2_API(NULL, NULL, "Method called");

    // Get message from mccptr
    GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

    // unpack message
    if((in_msg_struct = ns_msg3__sss__unpack(NULL, len, (void *)msg)) == NULL)
    {
        fprintf(stderr, "Invalid message recieved\n");
        return -1;
    }

    /* Call Api and save its result */
    out_msg_struct.field1 = ns_web_websocket_search(in_msg_struct->field1, in_msg_struct->field2, in_msg_struct->field3);

    // Free unpacked message
    ns_msg3__sss__free_unpacked(in_msg_struct, NULL);

    /* Get the size of the message to be packed */
    *out_msg_len = ns_msg1__str__get_packed_size(&out_msg_struct);

    /* Get buffer to fill message. */
    GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

    /* Packed the message if failure return -1 else 0 */
    if(*out_msg_len != ns_msg1__str__pack(&out_msg_struct, (void *)out_msg))
    {
        fprintf(stderr, "Failed to pack complete message\n");
        return -1;
    }
    return 0;
}

//api ns_stop_inline
int ns_stop_inline_urls_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
    char *out_msg;

    /* Initialize output message */
    NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;

    NSDL2_API(NULL, NULL, "Method called");

    /* Call Api and save its result */
    out_msg_struct.field1 = ns_stop_inline_urls();

    /* Get the size of the message to be packed */
    *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

    /* Get buffer to fill message. */
    GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

    /* Packed the message if failure return -1 else 0 */
    if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg))
    {
        fprintf(stderr, "Failed to pack complete message\n");
        return -1;
    }
    return 0;
}

//api ns_web_websocket_close
int ns_web_websocket_close_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
   // char *out_msg;
    char *msg;
    int len,close_id,ret;

    /* Initialize output message */
    //NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
    NsMsg1Int *in_msg_struct;

    /* Get the vptr from mccptr */
    VUser *vptr = mccptr->vptr;

    NSDL2_API(vptr, NULL, "Method called");

    // Get message from mccptr
    GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

    // unpack message
    if((in_msg_struct = ns_msg1__int__unpack(NULL, len, (void *)msg)) == NULL)
    {
        fprintf(stderr, "Invalid message recieved\n");
        return -1;
    }
   
    /* Call and save input values */
    close_id = vptr->ws_close_id =  in_msg_struct->field1;
  
    // Free unpacked message
    ns_msg1__int__free_unpacked(in_msg_struct, NULL);

    /* Add this task to NVM */
    if((ret = ns_websocket_ext(vptr,close_id,1)) != 0)
    {
       fprintf(stderr, "Failed to add task for ns_websocket_close\n");
       return -1;
    }

    //Response will not be set here
    return 1;

    /* Get the size of the message to be packed */
   // *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

    /* Get buffer to fill message. */
   // GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

    /* Packed the message if failure return -1 else 0 */
    //if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg))
    //{
     //   fprintf(stderr, "Failed to pack complete message\n");
     //   return -1;
   // }

}

//api ns_web_websocket_send
int ns_web_websocket_send_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
    char *msg;
    int len,send_id,ret;

    /* Get the vptr from mccptr */
    VUser *vptr = mccptr->vptr;

    /* Initialize output message */
    //  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
    NsMsg1Int *in_msg_struct;

    NSDL2_API(vptr, NULL, "Method called");

    // Get message from mccptr
    GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

    // Unpack message
    if((in_msg_struct = ns_msg1__int__unpack(NULL, len, (void *)msg)) == NULL)
    {
        fprintf(stderr, "Invalid message recieved\n");
        return -1;
    }

    /* Get Send id */
    send_id = vptr->ws_send_id =  in_msg_struct->field1;
    //free unpacked message
    ns_msg1__int__free_unpacked(in_msg_struct, NULL);
    
    /* Add this task to NVM */
     if((ret = ns_websocket_ext(vptr,send_id,0)) != 0)
     {
        fprintf(stderr, "Failed to add task for ns_websocket_send\n");
        return -1;
     }
   
    //Response will not be set here
    return 1;



    /* Get the size of the message to be packed */
    // *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

    /* Get buffer to fill message. */
   //  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

    /* Packed the message if failure return -1 else 0 */
    // if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg))
   // {
   //     fprintf(stderr, "Failed to pack complete message\n");
   //     return -1;
  //  }
    return 0;
}

//function returning machine type(NDE, NDAppliance, NSAppliance)
int ns_get_machine_type_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
    char *out_msg;

    /* Initialize output message */
    NsMsg1Str out_msg_struct = NS_MSG1__STR__INIT;

    NSDL2_API(NULL, NULL, "Method called");

    /* Call Api and save its result */
    out_msg_struct.field1 = (char *)global_settings->event_generating_host;

    /* Get the size of the message to be packed */
    *out_msg_len = ns_msg1__str__get_packed_size(&out_msg_struct);

    /* Get buffer to fill message. */
    GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

    /* Packed the message if failure return -1 else 0 */
    if(*out_msg_len != ns_msg1__str__pack(&out_msg_struct, (void *)out_msg))
    {
        fprintf(stderr, "Failed to pack complete message\n");
        return -1;
    }
    return 0;
}

// api ns_log_event
int ns_log_event_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
    char *msg;
    char *out_msg;
    int len;

    /* Initialize output message */
    NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
    NsMsg2IS *in_msg_struct;

    NSDL2_API(NULL, NULL, "Method called");

    /* Get the input message from mccptr send by njvm */
    GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

    // Unpack message
    if((in_msg_struct = ns_msg2__is__unpack(NULL, len, (void *)msg)) == NULL)
    {
        fprintf(stderr, "Invalid message recieved\n");
        return -1;
    }

    /* Call Api and save its result */
    out_msg_struct.field1 = ns_log_event(in_msg_struct->field1, in_msg_struct->field2);

    // Free unpacked data
    ns_msg2__is__free_unpacked(in_msg_struct, NULL);

    /* Get the size of the message to be packed */
    *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

    /* Get buffer to fill message. */
    GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

    /* Packed the message if failure return -1 else 0 */
    if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg))
    {
        fprintf(stderr, "Failed to pack complete message\n");
        return -1;
    }
    return 0;
}

//api ns_url_get_resp_size_msg_proc_func
int ns_url_get_resp_size_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
    char *out_msg;

    /* Initialize output message */
    NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;

    /* Get the vptr from mccptr */
    VUser *vptr = mccptr->vptr;

    NSDL2_API(vptr, NULL, "Method called");

    /* Call Api and save its result */
    out_msg_struct.field1 = ns_url_get_resp_size_internal(vptr);

    /* Get the size of the message to be packed */
    *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

    /* Get buffer to fill message. */
    GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

    /* Packed the message if failure return -1 else 0 */
    if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg))
    {
        fprintf(stderr, "Failed to pack complete message\n");
        return -1;
    }
    return 0;
}

//api ns_Web_cleanup_auto_headers_msg_proc_func
int ns_web_cleanup_auto_headers_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
    char *out_msg;

    /* Initialize output message */
    NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;

    /* Get vptr from mccptr */
    VUser *vptr = mccptr->vptr;

    NSDL2_API(vptr, NULL, "Method called");

    /* Call Api and save its result */
    delete_all_auto_header(vptr);
    out_msg_struct.field1 = 0;

    /* Get the size of the message to be packed */
    *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

    /* Get buffer to fill message. */
    GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

    /* Packed the message if failure return -1 else 0 */
    if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg))
    {
        fprintf(stderr, "Failed to pack complete message\n");
        return -1;
    }
    return 0;
}

//api ns_web_remove_auto_hrader_msg_proc_func
int ns_web_remove_auto_header_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
    char *out_msg, *msg;
    int len;

    NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
    NsMsg2SI *in_msg_struct;

    IW_UNUSED(VUser *vptr = mccptr->vptr);

    NSDL2_API(vptr, NULL, "Method called");

    // Get message from mccptr
    GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

    // Unpack message
    if((in_msg_struct = ns_msg2__si__unpack(NULL, len, (void *)msg)) == NULL)
    {
        fprintf(stderr, "Invalid message recieved\n");
        return -1;
    }

    /* Call Api and save its result */
    out_msg_struct.field1 =  ns_web_remove_auto_header( in_msg_struct->field1, in_msg_struct->field2);
    
    // Free unpacked message
    ns_msg2__si__free_unpacked(in_msg_struct, NULL);

    /* Get the size of the message to be packed */
    *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

    /* Get buffer to fill message. */
    GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

    /* Packed the message if failure return -1 else 0 */
    if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg))
    {
        fprintf(stderr, "Failed to pack complete message\n");
        return -1;
    }
    return 0;
}

//api ns_web_add_auto_header_msg_proc_func
int ns_web_add_auto_header_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
    char *out_msg, *msg;
    int len;

    /* Initialize output message */
    NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
    NsMsg3SSI *in_msg_struct;

    NSDL2_API(NULL, NULL, "Method called");

    // Get message from mccptr
    GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

    // Unpack message
    if((in_msg_struct = ns_msg3__ssi__unpack(NULL, len, (void *)msg)) == NULL)
    {
        fprintf(stderr, "Invalid message recieved\n");
        return -1;
    }
    
    /* Call Api and save its result */
    out_msg_struct.field1 = ns_web_add_auto_header(in_msg_struct->field1,in_msg_struct->field2, in_msg_struct->field3);

    // Free unpacked message
    ns_msg3__ssi__free_unpacked(in_msg_struct, NULL);

    /* Get the size of the message to be packed */
    *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

    /* Get buffer to fill message. */
    GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

    /* Packed the message if failure return -1 else 0 */
    if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg))
    {
        fprintf(stderr, "Failed to pack complete message\n");
        return -1;
    }
    return 0;
}

//api ns_web_add_header_msg_proc_func
int ns_web_add_header_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
    char *out_msg, *msg;
    int len;

    NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
    NsMsg3SSI *in_msg_struct;

    NSDL2_API(NULL, NULL, "Method called");

    // Get message from mccptr
    GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

    // Unpack message
    if((in_msg_struct = ns_msg3__ssi__unpack(NULL, len, (void *)msg)) == NULL)
    {
        fprintf(stderr, "Invalid message recieved\n");
        return -1;
    }

    /* Call Api and save its result */
    out_msg_struct.field1=ns_web_add_header(in_msg_struct->field1, in_msg_struct->field2, in_msg_struct->field3);

    // Free unpacked message
    ns_msg3__ssi__free_unpacked(in_msg_struct, NULL);

    /* Get the size of the message to be packed */
    *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

    /* Get buffer to fill message. */
    GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

    /* Packed the message if failure return -1 else 0 */
    if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg))
    {
        fprintf(stderr, "Failed to pack complete message\n");
        return -1;
    }
    return 0;
}



// api ns_remove_cookie_msg_proc_func
int ns_remove_cookie_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  //char *msg;
  //char len;
  char *out_msg;

  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;

 // NsMsg4SSSI *in_msg_struct;

  NSDL2_API(NULL, NULL, "Method called");

  // Get message from mccptr
 // GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

   // Unpack message
 //  if((in_msg_struct = ns_msg4__sssi__unpack(NULL, len, (void *)msg)) == NULL)
  // {
   //   fprintf(stderr, "Invalid message recieved\n");
   //   return -1;
  // }

   // Call Api and save the result
 //  out_msg_struct.field1 = ns_remove_cookie(in_msg_struct->field1, in_msg_struct->field2, in_msg_struct->field3, in_msg_struct->field4);

   // Free unpacked data
  // ns_msg1__sssi__free_unpacked(in_msg_struct, NULL);

   // Pack the result
   *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);


   // Now get buffer to fill message.
   GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

    if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
  	      fprintf(stderr, "Failed to pack complete message\n");
             return -1;
    }

 return 0;
}

// api ns_cleanup_cookies
int ns_cleanup_cookies_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
   char *out_msg;

  /* Initialize output message */
  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;

  NSDL2_API(NULL, NULL, "Method called");

  /* Call Api and save its result */
  out_msg_struct.field1 = ns_cleanup_cookies();

  /* Get the size of the message to be packed */
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

  /* Get buffer to fill message. */
   GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  /* Packed the message if failure return -1 else 0 */
  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
      fprintf(stderr, "Failed to pack complete message\n");
      return -1;
  }

  return 0;
}


// api ns_save_value_from_file_msg_proc_func
int ns_save_value_from_file_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
    char *msg;
    int len;
    char *out_msg;

    NsMsg2SS *in_msg_struct;

    /* Initialize output message */
    NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;

    NSDL2_API(NULL, NULL, "Method called");

    // Get message from mccptr
    GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

    // Unpack message
    if((in_msg_struct = ns_msg2__ss__unpack(NULL, len, (void *)msg)) == NULL) {
        fprintf(stderr, "Invalid message recieved\n");
        return -1;
    }

    // Call Api and save its result
    out_msg_struct.field1 = ns_save_value_from_file(in_msg_struct->field1,in_msg_struct->field2);

    // Free unpacked data
    ns_msg2__ss__free_unpacked(in_msg_struct, NULL);

   // Pack the result
   *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

   // Now get buffer to fill message.
   GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

   if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
       fprintf(stderr, "Failed to pack complete message\n");
       return -1;
   }
   return 0;

}

//api ns_eval_compress_param_msg_proc_func
static int ns_eval_compress_param_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
    char *msg;
    char len;
    char *out_msg;

    NsMsg2SS *in_msg_struct;

    /* Initialize output message */
    NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;

    NSDL2_API(NULL, NULL, "Method called");

    //get message from mccptr
    GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

    //unpack message.
    if((in_msg_struct = ns_msg2__ss__unpack(NULL, len, (void *)msg)) == NULL)
    {
        fprintf(stderr, "Invalid message recieved\n");
        return -1;
    }

    //call api and save it's result
    out_msg_struct.field1 = ns_eval_compress_param(in_msg_struct->field1, in_msg_struct->field2);

    //free unpacked msg
    ns_msg2__ss__free_unpacked(in_msg_struct, NULL);

    //pack this message
    *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);
    //TODO: This length should be compared with maximum available length for out buffer.

    //now get buffer to fill message.
    GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

    if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg))
    {
        fprintf(stderr, "Failed to pack complete message\n");
        return -1;
    }
    return 0;
}

//api ns_eval_decompress_param_msg_proc_func
static int ns_eval_decompress_param_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
    char *msg;
    char len;
    char *out_msg;

    NsMsg2SS *in_msg_struct;

    /* Initialize output message */
    NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;

    NSDL2_API(NULL, NULL, "Method called");

    //get message from mccptr
    GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

    //unpack message.
    if((in_msg_struct = ns_msg2__ss__unpack(NULL, len, (void *)msg)) == NULL)
    {
        fprintf(stderr, "Invalid message recieved\n");
        return -1;
    }

    //call api and save it's result
    out_msg_struct.field1 = ns_eval_decompress_param(in_msg_struct->field1, in_msg_struct->field2);

    //free unpacked msg
    ns_msg2__ss__free_unpacked(in_msg_struct, NULL);

    //pack this message
    *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);
    //TODO: This length should be compared with maximum available length for out buffer.

    //now get buffer to fill message.
    GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

    if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg))
    {
        fprintf(stderr, "Failed to pack complete message\n");
        return -1;
    }
    return 0;
}

//function returing link hdr value 
int ns_get_link_hdr_value_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
    char *out_msg;

    /* Initialize output message */
    NsMsg1Str out_msg_struct = NS_MSG1__STR__INIT;

    NSDL2_API(NULL, NULL, "Method called");

    /* Call Api and save its result */
    out_msg_struct.field1 = ns_get_link_hdr_value();

    /* Get the size of the message to be packed */
    *out_msg_len = ns_msg1__str__get_packed_size(&out_msg_struct);

    /* Get buffer to fill message. */
    GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

    /* Packed the message if failure return -1 else 0 */
    if(*out_msg_len != ns_msg1__str__pack(&out_msg_struct, (void *)out_msg))
    {
        fprintf(stderr, "Failed to pack complete message\n");
        return -1;
    }
    return 0;
}

//function returing redirected url 
int ns_get_redirect_url_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
    char *out_msg;

    /* Initialize output message */
    NsMsg1Str out_msg_struct = NS_MSG1__STR__INIT;

    NSDL2_API(NULL, NULL, "Method called");

    /* Call Api and save its result */
    out_msg_struct.field1 = ns_get_redirect_url();

    /* Get the size of the message to be packed */
    *out_msg_len = ns_msg1__str__get_packed_size(&out_msg_struct);

    /* Get buffer to fill message. */
    GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

    /* Packed the message if failure return -1 else 0 */
    if(*out_msg_len != ns_msg1__str__pack(&out_msg_struct, (void *)out_msg))
    {
        fprintf(stderr, "Failed to pack complete message\n");
        return -1;
    }
    return 0;
}


int ns_save_command_output_in_param_msg_proc_fun(Msg_com_con* mccptr, int *out_msg_len)
{

  char *msg;
  int len;
  char *out_msg;

  NsMsg2SS *in_msg_struct;
  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;

  NSDL2_API(NULL, NULL, "Method called");

  // Get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message
  if((in_msg_struct = ns_msg2__ss__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }

  //call api and save it's result
  ns_save_command_output_in_param(in_msg_struct->field1, in_msg_struct->field2);
  out_msg_struct.field1 = 0;
  
  //free unpacked message
  ns_msg2__ss__free_unpacked(in_msg_struct, NULL);

  //pack this message
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;

}

/*
Note: JAVA doesn't support the concept of output parameters like C,
so in case of JTS parameter buffer and size can’t be retrieved through output parameters. 
Parameter value for below Api is retrieved through the return parameter.
Therefore, in case of JTS ns_get_param_val should be called as:

For Ex -   
   message = nsApi.ns_param_get_val(name, buffer, size);
                
               NOT LIKE "C"

   message = ns_param_get_val(name, buffer, &size); 
   
   whereas in case of JTS,
        buffer - string/empty string
        size - can be any integer value(0,1,10,etc)    
    
*/

static int ns_get_param_val_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *msg;
  int len;
  char *out_msg;

  NsMsg3SSI *in_msg_struct;
  NsMsg1Str out_msg_struct = NS_MSG1__STR__INIT;

  NSDL2_API(NULL, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message
  if((in_msg_struct = ns_msg3__ssi__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }

  //call api and save it's result
  out_msg_struct.field1 = ns_get_param_val(in_msg_struct->field1, NULL, &len);

  printf("\n Value of get_param_val is %s",out_msg_struct.field1);

  //free unpacked message
  ns_msg3__ssi__free_unpacked(in_msg_struct, NULL);

  //pack this message
  *out_msg_len = ns_msg1__str__get_packed_size(&out_msg_struct);

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__str__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}

int ns_get_pg_think_time_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{

  char *out_msg;

  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
  VUser *vptr = mccptr->vptr;

  NSDL2_API(vptr, NULL, "Method called");

  NSDL2_API(NULL, NULL, "njvm thread(%s), api - ns_get_pg_think_time()", msg_com_con_to_str(mccptr));

  out_msg_struct.field1 = ns_get_pg_think_time_internal(vptr);

  //pack the result
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}

//Api ns_save_string_searched_string
static int ns_save_searched_string_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{
  char *msg;
  int len;
  char *out_msg;

  NsMsg9ISISSSIII *in_msg_struct; 
  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;

  IW_UNUSED(VUser *vptr = mccptr->vptr);

  NSDL2_API(vptr, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message
  if((in_msg_struct = ns_msg9__isisssiii__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }

  //call api and save it's result
  out_msg_struct.field1 = ns_save_searched_string(in_msg_struct->field1, in_msg_struct->field2, in_msg_struct->field3, in_msg_struct->field4,
                          in_msg_struct->field5, in_msg_struct->field6, in_msg_struct->field7, in_msg_struct->field8, in_msg_struct->field9);

  //free unpacked message
  ns_msg9__isisssiii__free_unpacked(in_msg_struct, NULL);

  //pack this message
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}

int ns_replace_ex_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{

  char *msg;
  int len;
  char *out_msg;
  
  NsMsg3SSS *in_msg_struct;
  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;
  
  NSDL2_API(NULL, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);
  
  //unpack message
  if((in_msg_struct = ns_msg3__sss__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  } 

  //call api and save it's result
  out_msg_struct.field1 = ns_replace(in_msg_struct->field1, in_msg_struct->field2, in_msg_struct->field3);
  
  //free unpacked message
  ns_msg3__sss__free_unpacked(in_msg_struct, NULL);
  
  //pack this message
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);
  
  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);
  
  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  } 
  return 0;
}

int ns_xml_replace_ex_msg_proc_func(Msg_com_con* mccptr, int *out_msg_len)
{

  char *msg;
  int len;
  char *out_msg;

  NsMsg4SSSS *in_msg_struct;
  NsMsg1Int out_msg_struct = NS_MSG1__INT__INIT;

  NSDL2_API(NULL, NULL, "Method called");

  //get message from mccptr
  GET_MESSAGE_FROM_CONNECTION(mccptr, msg, len);

  //unpack message
  if((in_msg_struct = ns_msg4__ssss__unpack(NULL, len, (void *)msg)) == NULL) {
    fprintf(stderr, "Invalid message recieved\n");
    return -1;
  }

  //call api and save it's result
  out_msg_struct.field1 = ns_xml_replace(in_msg_struct->field1, in_msg_struct->field2, in_msg_struct->field3, in_msg_struct->field4);

  //free unpacked message
  ns_msg4__ssss__free_unpacked(in_msg_struct, NULL);

  //pack this message
  *out_msg_len = ns_msg1__int__get_packed_size(&out_msg_struct);

  //now get buffer to fill message.
  GET_OUT_MESSAGE_BUFFER(mccptr, out_msg, *out_msg_len);

  if(*out_msg_len != ns_msg1__int__pack(&out_msg_struct, (void *)out_msg)) {
    fprintf(stderr, "Failed to pack complete message\n");
    return -1;
  }
  return 0;
}

