#ifndef NS_NVM_NJVM_MSG_COM_H
#define NS_NVM_NJVM_MSG_COM_H

//Define Message Opcodes
#define NS_NJVM_START_USER_REQ	1000
#define NS_NJVM_START_USER_REP	2000

#define NS_NJVM_BIND_NVM_REQ	1001
#define NS_NJVM_BIND_NVM_REP	2001

#define NJVM_VUTD_INFO_REQ	1002
#define NJVM_VUTD_INFO_REP	2002

#define NS_NJVM_API_WEB_URL_REQ	1003
#define NS_NJVM_API_WEB_URL_REP	2003

#define NS_NJVM_API_PAGE_THINK_TIME_REQ	1004
#define NS_NJVM_API_PAGE_THINK_TIME_REP	2004

#define NS_NJVM_API_START_TRANSACTION_REQ	1005
#define NS_NJVM_API_START_TRANSACTION_REP	2005

#define NS_NJVM_API_END_TRANSACTION_REQ	1006
#define NS_NJVM_API_END_TRANSACTION_REP	2006

#define NS_NJVM_API_END_TRANSACTION_AS_REQ	1007
#define NS_NJVM_API_END_TRANSACTION_AS_REP	2007

#define NS_NJVM_API_GET_TRANSACTION_TIME_REQ	1008
#define NS_NJVM_API_GET_TRANSACTION_TIME_REP	2008

#define NS_NJVM_API_GET_TX_STATUS_REQ	1009
#define NS_NJVM_API_GET_TX_STATUS_REP	2009

#define NS_NJVM_API_SET_TX_STATUS_REQ	1010
#define NS_NJVM_API_SET_TX_STATUS_REP	2010

#define NS_NJVM_API_SYNC_POINT_REQ	1011
#define NS_NJVM_API_SYNC_POINT_REP	2011

#define NS_NJVM_API_SAVE_STRING_FLAG_REQ	1012
#define NS_NJVM_API_SAVE_STRING_FLAG_REP	2012

#define NS_NJVM_API_EVAL_STRING_FLAG_REQ	1013
#define NS_NJVM_API_EVAL_STRING_FLAG_REP	2013

#define NS_NJVM_API_EVAL_STRING_REQ	1014
#define NS_NJVM_API_EVAL_STRING_REP	2014

#define NS_NJVM_API_SAVE_STRING_REQ	1015
#define NS_NJVM_API_SAVE_STRING_REP	2015

#define NS_NJVM_API_LOG_STRING_REQ	1016
#define NS_NJVM_API_LOG_STRING_REP	2016

#define NS_NJVM_API_LOG_MSG_REQ	1017
#define NS_NJVM_API_LOG_MSG_REP	2017

#define NS_NJVM_API_DEFINE_SYNCPOINT_REQ	1018
#define NS_NJVM_API_DEFINE_SYNCPOINT_REP	2018

#define NS_NJVM_API_GET_PAGE_STATUS_REQ	1019
#define NS_NJVM_API_GET_PAGE_STATUS_REP	2019

#define NS_NJVM_API_CLEANUP_COOKIES_REQ    1020
#define NS_NJVM_API_CLEANUP_COOKIES_REP    2020

#define NS_NJVM_API_SET_PAGE_STATUS_REQ	1021
#define NS_NJVM_API_SET_PAGE_STATUS_REP	2021

#define NS_NJVM_API_SOAP_WS_SECURITY_REQ   1022
#define NS_NJVM_API_SOAP_WS_SECURITY_REP   2022

#define NS_NJVM_API_GET_SESSION_STATUS_REQ	1023
#define NS_NJVM_API_GET_SESSION_STATUS_REP	2023

#define NS_NJVM_API_SET_SESSION_STATUS_REQ	1025
#define NS_NJVM_API_SET_SESSION_STATUS_REP	2025

#define NS_NJVM_API_DISABLE_SOAP_WS_SECURITY_REQ   1026
#define NS_NJVM_API_DISABLE_SOAP_WS_SECURITY_REP   2026

/* Api corresponding to NS_NJVM_API_GET_SESS_STATUS_REQ and 
   NS_NJVM_API_SET_SESS_STATUS_REQ are depreciated */

#define NS_NJVM_API_EVAL_COMPRESS_PARAM_REQ	1027
#define NS_NJVM_API_EVAL_COMPRESS_PARAM_REP	2027

#define NS_NJVM_API_EVAL_DECOMPRESS_PARAM_REQ	1028
#define NS_NJVM_API_EVAL_DECOMPRESS_PARAM_REP	2028

#define NS_NJVM_API_GET_INT_VAL_REQ	1029
#define NS_NJVM_API_GET_INT_VAL_REP	2029

#define NS_NJVM_API_SET_INT_VAL_REQ	1030
#define NS_NJVM_API_SET_INT_VAL_REP	2030

#define NS_NJVM_API_GET_COOKIE_IDX_REQ	1031
#define NS_NJVM_API_GET_COOKIE_IDX_REP	2031

#define NS_NJVM_API_GET_COOKIE_VAL_REQ	1032
#define NS_NJVM_API_GET_COOKIE_VAL_REP	2032

#define NS_NJVM_API_GET_COOKIE_VAL_EX_REQ	1033
#define NS_NJVM_API_GET_COOKIE_VAL_EX_REP	2033

#define NS_NJVM_API_SET_COOKIE_VAL_REQ	1034
#define NS_NJVM_API_SET_COOKIE_VAL_REP	2034

#define NS_NJVM_API_SET_COOKIE_VAL_EX_REQ	1035
#define NS_NJVM_API_SET_COOKIE_VAL_EX_REP	2035

#define NS_NJVM_API_GET_AUTO_COOKIE_MODE_REQ	1036
#define NS_NJVM_API_GET_AUTO_COOKIE_MODE_REP	2036

#define NS_NJVM_API_REMOVE_COOKIE_REQ      1037
#define NS_NJVM_API_REMOVE_COOKIE_REP      2037

#define NS_NJVM_API_URL_GET_BODY_MSG_REQ	1038
#define NS_NJVM_API_URL_GET_BODY_MSG_REP	2038

#define NS_NJVM_API_URL_GET_HDR_MSG_REQ	1039
#define NS_NJVM_API_URL_GET_HDR_MSG_REP	2039

#define NS_NJVM_API_URL_GET_RESP_MSG_REQ	1040
#define NS_NJVM_API_URL_GET_RESP_MSG_REP	2040

#define NS_NJVM_API_URL_GET_HDR_SIZE_REQ	1041
#define NS_NJVM_API_URL_GET_HDR_SIZE_REP	2041

#define NS_NJVM_API_URL_GET_BODY_SIZE_REQ	1042
#define NS_NJVM_API_URL_GET_BODY_SIZE_REP	2042

#define NS_NJVM_API_VALIDATE_RESPONSE_CHECKSUM_REQ	1043
#define NS_NJVM_API_VALIDATE_RESPONSE_CHECKSUM_REP	2043

#define NS_NJVM_API_GET_REDIRECT_URL_REQ	1044
#define NS_NJVM_API_GET_REDIRECT_URL_REP	2044

/* Api corresponding to NS_NJVM_API_GET_PG_THINK_TIME_REQ and 
   NS_NJVM_API_SET_PG_THINK_TIME_REQ are depreciated */

#define NS_NJVM_API_WEB_WEBSOCKET_CHECK_REQ	1045
#define NS_NJVM_API_WEB_WEBSOCKET_CHECK_REP	2045

#define NS_NJVM_API_ENABLE_SOAP_WS_SECURITY_REQ	1046
#define NS_NJVM_API_ENABLE_SOAP_WS_SECURITY_REP	2046

#define NS_NJVM_API_ADD_COOKIE_VAL_EX_REQ	1047
#define NS_NJVM_API_ADD_COOKIE_VAL_EX_REP	2047

#define NS_NJVM_API_ADD_COOKIES_REQ	1048
#define NS_NJVM_API_ADD_COOKIES_REP	2048

#define NS_NJVM_API_ADVANCE_PARAM_REQ	1049
#define NS_NJVM_API_ADVANCE_PARAM_REP	2049

#define NS_NJVM_API_PARAM_SPRINTF_EX_REQ	1050
#define NS_NJVM_API_PARAM_SPRINTF_EX_REP	2050

#define NS_NJVM_API_PARAMARR_IDX_REQ	1051
#define NS_NJVM_API_PARAMARR_IDX_REP	2051

#define NS_NJVM_API_PARAMARR_LEN_REQ	1052
#define NS_NJVM_API_PARAMARR_LEN_REP	2052

#define NS_NJVM_API_PARAMARR_RANDOM_REQ	1053
#define NS_NJVM_API_PARAMARR_RANDOM_REP	2053

#define NS_NJVM_API_CHECK_REPLY_SIZE_REQ  1054
#define NS_NJVM_API_CHECK_REPLY_SIZE_REP  2054

#define NS_NJVM_INCREASE_THREAD_POOL_REQ 1055
#define NS_NJVM_INCREASE_THREAD_POOL_REP 2055

#define NS_NJVM_ERROR_MSG_FROM_NJVM 2056

#define NS_NJVM_STOP_USER_REQ 1097
#define NS_NJVM_STOP_USER_REP 2097

#define NS_NJVM_EXIT_TEST_REQ 1098 
#define NS_NJVM_EXIT_TEST_REP 2098

#define NS_NJVM_API_EXIT_SESSION_REQ	1999
#define NS_NJVM_API_EXIT_SESSION_REP	2999

#define NS_NJVM_API_START_TIMER_REQ 1079
#define NS_NJVM_API_START_TIMER_REP 2079

#define NS_NJVM_API_END_TIMER_REQ 1080 
#define NS_NJVM_API_END_TIMER_REP 2080

#define NS_API_SEND_RBU_STATS 1081                       //For RBU Page Stat support in java-type script
#define NS_API_SEND_RBU_STATS_REP 2081

#define NS_API_GET_CURRENT_PARTITION_REQ 1082
#define NS_API_GET_CURRENT_PARTITION_REP 2082

#define NS_API_GET_HOST_ID_REQ 1083
#define  NS_API_GET_HOST_ID_REP 2083

#define NS_API_DB_REPLAY_QUERY_REQ 1084 
#define NS_API_DB_REPLAY_QUERY_REP 2084 

#define NS_API_DB_REPLAY_QUERY_END_REQ 1085
#define NS_API_DB_REPLAY_QUERY_END_REP 2085

#define NS_NJVM_API_WEB_WEBSOCKET_SEND_REQ 1094
#define NS_NJVM_API_WEB_WEBSOCKET_SEND_REP 2094

#define NS_NJVM_API_WEB_WEBSOCKET_CLOSE_REQ 1095
#define NS_NJVM_API_WEB_WEBSOCKET_CLOSE_REP 2095

#define NS_NJVM_API_WEB_WEBSOCKET_READ_REQ 1072
#define NS_NJVM_API_WEB_WEBSOCKET_READ_REP 2072

#define NS_NJVM_API_CLICK_API_REQ          1069
#define NS_NJVM_API_CLICK_API_REP          2069  

/* This opcode is used to send the response to JAVA after ending session
   Currently we are sending this response from nsi_end_session function */
#define NS_NJVM_API_END_SESSION_REP 2099

//Array for message handler
typedef int (*jThreadMessageHandler) (Msg_com_con *, int *);

extern jThreadMessageHandler jthraed_msg_handler[];
 
#define NVM_NJVM_MSG_HDR_SIZE 24 /* sizeof(int<size>) + sizeof(int<opcode>) + 4*sizeof(int<futurefields>) */

extern int send_msg_to_njvm(Msg_com_con *mccptr, int opcode, int output);
extern int handle_msg_from_njvm(Msg_com_con *mccptr, int epoll_events); 

extern int handle_njvm_error_message(Msg_com_con *mccptr);
extern int handle_njvm_stop_user_response(Msg_com_con *mccptr);
extern int handle_njvm_start_user_response(Msg_com_con* mccptr);
extern int handle_njvm_increase_thread_rep(Msg_com_con *mccptr);
extern int start_user_req_create_msg(Msg_com_con* mccptr, int *out_msg_len);
extern int increase_thread_pool_create_msg(Msg_com_con *mccptr, int cum_thread_count, int *out_msg_len);
extern int stop_user_create_msg(Msg_com_con* mccptr, int *out_msg_len);
extern int ns_web_url_resp_create_msg(Msg_com_con *mccptr, int ret_value, int *out_msg_len);
extern int ns_page_think_time_resp_create_msg(Msg_com_con *mccptr, int ret_value, int *out_msg_len);
extern void send_msg_nvm_to_vutd(VUser *vptr, int type, int ret_val);
extern void close_njvm_msg_com_con(Msg_com_con *mccptr);
extern int ns_web_websocket_send_resp_create_msg(Msg_com_con *mccptr, int ret_value, int *out_msg_len);
extern int ns_web_websocket_close_resp_create_msg(Msg_com_con *mccptr, int ret_value, int *out_msg_len);
extern int ns_web_websocket_read_resp_create_msg(Msg_com_con *mccptr, int ret_value, int *out_msg_len);
extern int ns_end_session_resp_create_msg(Msg_com_con *mccptr, int ret_value, int *out_msg_len);
#endif
