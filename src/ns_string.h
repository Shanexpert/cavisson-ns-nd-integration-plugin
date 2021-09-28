#ifndef __NS_STRING_H__
#define __NS_STRING_H__

#include <stddef.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "ns_log.h"
#include "ns_data_types.h"

/* Example of Script debug log
 *
 * NSDL1_SCRIPT("This page status is %d", ns_get_page_status());
 *
 * NSDL2_SCRIPT("This page status is %d", ns_get_page_status());
 *
 * NSDL3_SCRIPT("This page status is %d", ns_get_page_status());
 *
 * NSDL4_SCRIPT("This page status is %d", ns_get_page_status());
 *
 */

#define EVENT_INFO EVENT_DEBUG    // Backward compatibility
// For Event log
#define EVENT_CLEAR       0
#define EVENT_DEBUG       1
#define EVENT_INFORMATION 2
#define EVENT_WARNING     3
#define EVENT_MINOR       4
#define EVENT_MAJOR       5
#define EVENT_CRITICAL    6

#define NUM_SEVERITY      7 

#define NS_LOG_STD 1
#define NS_LOG_EXTENDED 2
#define NS_LOG_DEBUG 3

#define NS_USE_CONFIGURED_THINK_TIME -1

//added for ns_param_api.c
#define NETSTORM_CODE_NONE   0
#define NETSTORM_CODE_ENCODE 1
#define NETSTORM_CODE_DECODE 2

#define NS_AUTO_STATUS    -1  // For setting status based on page status
#define NS_SUCCESS_STATUS  0  // Set status to success
#define NS_TX_CV_FAIL      33 // Set status to success | Same valu as of NS_REQUEST_CV_FAILURE

//for ns_check_reply_size
#define SIZE_AS_PER_MODE 0 //OK 
#define SIZE_TO_SMALL 1   // response size is small
#define SIZE_TO_BIG 2     //response size is big
//mode values
#define NS_CHK_REP_SZ_MODE_NOT_BETWEEN_MIN_MAX 1

//#define MAX_PARAM_VALUE_SIZE 64000 
#define MAX_PARAM_VALUE_SIZE 127 

/*These are for save data API*/
#define NS_TRUNC_FILE  0
#define NS_APPEND_FILE 1
#define NS_ADD_DATA_IN_FILE 2

/*TR069 protocol related macros*/


#define NS_TR069_ERROR                          -1

#define NS_TR069_CPE_INVOKE_INFORM              1
#define NS_TR069_CPE_INVITE_RPC                 2
#define NS_TR069_GET_RPC_METHODS                3
#define NS_TR069_SET_PARAMETER_VALUES           4
#define NS_TR069_GET_PARAMETER_VALUES           5
#define NS_TR069_GET_PARAMETER_NAMES            6
#define NS_TR069_SET_PARAMETER_ATTRIBUTES       7
#define NS_TR069_GET_PARAMETER_ATTRIBUTES       8
#define NS_TR069_ADD_OBJECT                     9
#define NS_TR069_DELETE_OBJECT                  10
#define NS_TR069_REBOOT                         11
#define NS_TR069_DOWNLOAD                       12
#define NS_TR069_TRANSFER_COMPLETE              13

#define NS_TR069_END_OF_SESSION                 99

#define NS_TR069_NUM_API                        13

// Return value for get_rfc() api
#define NS_TR069_RFC_FROM_ACS                   0
#define NS_TR069_WAIT_IS_OVER                   1
#define NS_TR069_VALUE_CHANGE_ACTIVE            2

# define NS_DECODE_TRIPLETS                     1
// MD Type value for HMAC
#define MD_TYPE_SHA1                         "SHA1"
#define MD_TYPE_MD5                          "MD5" 
#define MD_TYPE_SHA256                       "SHA256"
#define ENCODE_MODE_ENABLE                    1 
#define ENCODE_MODE_DISABLE                   0

#define DIGEST_SHA1   1
#define DIGEST_SHA256 2
#define MAX_PARAM_SIZE			      1024

extern double ns_get_double_val(char * var);
extern int ns_set_double_val(const char* param_name, double value);
extern int ns_save_string_flag(const char* param_value, int value_length, const char* param_name, int encode_flag);
extern char* ns_eval_string_flag(char* string, int encode_flag, long *size);


extern u_ns_ts_t ns_get_ms_stamp();

extern char* ns_eval_string(char* string);
extern unsigned char *ns_hmac(unsigned char *message , int msg_len, unsigned char *key, size_t key_len, char *md_type, int encode_mode); 
extern int ns_get_hmac_signature(char *method, char *uri, char *headers, char *body, char *region, char *key, char *ksecret, char *signature, char *sku_value, char *zipcode, char *amzDate, char *dateStamp, char *vendor_code); 
extern int ns_save_string(const char* param_value, const char* param_name);
extern int ns_chk_strcpy(char *dest, char *src, int dest_len);
extern int ns_aws_signature(char *method, char *uri, char *canonicalHeaders, char *signedHeaders, char *body, char *region, char *service, char *key, char *ksecret, char *amzDate, char *dateStamp, char *signature);

extern int ns_log_string(int level, char* buffer);

extern int ns_log_msg(int level, char* format, ...);

extern int ns_decode_3des(char *key, char *in, char *out, int out_len, int mode);

extern int url_resp_hdr_size;
// Transaction APIs

// For start, success is 0, < 0 is error
// For end/end as succes >= 0 as it return the status of tx, error is < 0
// For set and get status succes >= 0 as it return the status of tx, error is < 0
// For get time succes >= 0, error is < 0

#define NS_TX_SUCCESS          0 // Sucess for start. End >= 0 is sucess
#define NS_TX_ERROR           -1 // Generic error
#define NS_TX_BAD_TX_NAME     -2 // Start or end as TX name is not correct
#define NS_TX_PG_RELOADING    -3 // Page is reloading
#define NS_TX_RUNNING         -4 // Tx is running
#define NS_TX_NOT_RUNNING     -5 // Tx is not running
#define NS_TX_END_AS_RUNNING  -6 // End as tx is running
#define NS_NO_TX_RUNNING      -7 // No tx running

#define NS_ARG_IS_BUF		0
#define NS_ARG_IS_PARAM		1
#define NS_ARG_IS_FILE		2
#define NS_ORD_ANY 		-2
#define NS_ORD_ALL		-1
#define NS_FROM_START		0
#define NS_SAVE_COMPLETE	-1

#define NS_STRING_API_ERROR -3

#define GET_THREAD_VPTR(vptr, X)\
{ \
  Msg_com_con *tmp_nvm_info_tmp; \
  tmp_nvm_info_tmp = (Msg_com_con *)get_thread_specific_data;\
  if(tmp_nvm_info_tmp == NULL) \
  { \
    NSTL1(NULL, NULL, "tmp_nvm_info_tmp is getting NULL, hence returning");\
    return X; \
  } \
  vptr = tmp_nvm_info_tmp->vptr; \
} \

#define IS_NS_SCRIPT_MODE_USER_CONTEXT  ISCALLER_NVM

extern int ns_start_transaction(char* tx_name);
extern int ns_start_transaction_ex(char* tx_name);

extern int ns_sync_point(char* sp_name);
extern void ns_define_syncpoint(char *sp_name);

extern int ns_end_transaction(char* tx_name, int status);
extern int ns_end_transaction_ex(char* tx_name, int status);

extern int ns_define_transaction(char *tx_name);

extern int ns_end_transaction_as(char* tx_name, int status, char *end_name);
extern int ns_end_transaction_as_ex(char* tx_name, int status, char *end_name);

extern int ns_get_transaction_time (char *tx_name);

extern int ns_set_tx_status (char* tx_name, int status);

extern int ns_get_tx_status (char* tx_name);
extern long ns_start_timer(char *start_timer_name);
extern long ns_end_timer(char *end_timer_name);
extern long long ns_get_cur_partition();
extern int ns_get_host_name(); 
// Keyword APIs : Added by Anuj 28/03/08
extern int ns_get_num_nvm();
extern int ns_get_num_ka_pct();
extern int ns_get_min_ka();
extern int ns_get_max_ka();
// End : Keyword APIs

extern int ns_get_page_status(void);

extern int ns_get_session_status(void);
extern int ns_set_session_status(int status);

extern int ns_get_int_val (char * var_name);
extern int ns_set_int_val (char * var_name, int value);

extern char *ns_get_cookie_val (int cookie_idx);
extern char *ns_get_cookie_val_ex (char *cookie_name, char *domain, char *path);
extern int ns_set_cookie_val (int cookie_idx, char *cookie_val);
extern int ns_set_cookie_val_ex (char *cookie_name, char *domain, char *path, char *cookie_val);
extern int ns_get_auto_cookie_mode();
extern int ns_remove_cookie (char *name, char *path, char *domain, int free_for_next_req);
extern int ns_cleanup_cookies();
extern int ns_web_add_header(char *header, char *content, int flag);
extern int ns_web_add_auto_header(char *header, char *content, int flag);
extern int ns_web_remove_auto_header(char *header, int flag);
extern int ns_web_cleanup_auto_headers();

//extern char * ns_get_ua_string ();
extern int ns_get_ua_string (char *ua_string_buf, int ua_string_len);
//extern void ns_set_ua_string (char *ua_static_ptr);
extern int ns_set_ua_string (char *ua_static_ptr, int ua_static_ptr_len);

extern int ns_get_cookies_disallowed ();
extern int ns_set_cookies_disallowed (int value);

extern char *ns_get_user_ip();
extern int ns_get_nvmid();
extern unsigned int ns_get_userid();
extern unsigned int ns_get_sessid();
extern int ns_get_testid();

extern void *do_shmget(long int size, char *msg);
extern void *do_shmget_with_id(long int size, char *msg, int *shmid); 
extern void *do_shmget_with_id_ex(long int size, char *msg, int *shmid, int auto_del);
extern void check_shared_mem(long int size);

extern char * ns_get_guid();
extern int use_geoip_db;

extern unsigned int get_an_IP_address_for_area(unsigned int area_id);
extern char *ns_get_area_ip_char(unsigned int area_id);
extern unsigned int ns_get_area_ip(unsigned int area_id);
extern char * ns_ip_to_char(unsigned int addr);

//Added 02/12/2007 for Url Pre Post helper functions.
extern char *ns_url_get_body_msg(int *size);
extern char *ns_url_get_hdr_msg(int *size);
extern char *ns_url_get_resp_msg(int *size);
extern int ns_url_get_hdr_size();
extern int ns_url_get_body_size();
extern int ns_url_get_resp_size();
extern int ns_url_get_http_status_code();

extern void ns_md5_checksum_to_ascii(unsigned char *source, unsigned char *dest);
extern int ns_gen_md5_checksum(const unsigned char *buf, int len, unsigned char *checksum_buf) ;
extern int ns_validate_response_checksum(char *cookie_name, char *var_name);

//Added API 15/09/2008 for encoding and decoding the value of variable
extern char *ns_encode_url(const char *string, int inlength);
extern char *ns_decode_url(const char *string, int length);
extern void ns_encode_decode_url_free(char *ptr);
extern char *ns_encode_eval_string(char *string);
extern int ns_encode_save_string(const char* param_value, const char* param_name);

extern int ns_add_user_data_point(int rptGroupID, int rptGraphID, double value);
extern int ns_is_rampdown_user(void);
//api to log event in event log
extern int ns_log_event(int severity, char *format, ...);

//----End

extern char *ns_get_redirect_url();
extern char *ns_get_link_hdr_value();


extern int ns_add_cookie_val_ex(char *cookie_name, char *domain, char *path, char *cookie_val);
extern int ns_add_cookies (char *cookie_buf);

extern int ns_get_replay_page();

//added for ns_param_api.c
extern int ns_advance_param(const char *param_name); 
extern char * ns_paramarr_idx(const char * paramArrayName, unsigned int index); 
extern int ns_paramarr_len(const char * paramArrayName); 
extern char * ns_paramarr_random(const char * paramArrayName); 
extern int ns_check_reply_size(int mode, int value1, int value2);

// Return session status as a string
extern  char *ns_get_session_status_name(int status);
// Return URL, Page and Tx  status as a string
extern char *ns_get_status_name(int status);
extern int ns_set_embd_objects(int num_eurls, char **eurls, int both_api_and_auto_fetch_url);
extern int ns_setup_save_url(int type, int depth, char *var);
extern int ns_force_server_mapping(char *rec, char* map);

/*API to get random number*/
extern int ns_get_random_number_int(int min , int max);
extern char* ns_get_random_number_str(int min , int max, char *format);
extern char* ns_get_random_str (int min, int max, char *format);

extern unsigned long long ns_get_unique_number();
/*This function is for setting 
* Keep Alive timeout at runtime.
* Given time is in milli seconds*/
int ns_set_ka_time(int ka_time);

extern int ns_get_pg_think_time();

/*This function is to get
* Keep Alive timeout at runtime.
* Given time is in milli seconds*/
extern int ns_get_ka_time();

/*These API are dependent upon nsa_log_mgr*/
extern int ns_save_data_eval(char *file_name, int mode, char *eval_string);
extern int ns_save_data_ex(char *file_name, int mode, char *format, ...);
extern int ns_save_data_var(char *file_name, int mode, char *var_name);
extern char *ns_get_all_cookies(char *cookie_buf, int cookie_max_buf_len);

/* WebSocket APIs */
extern int ns_web_url(int page_id);
extern int ns_web_websocket_send(int page_id);
extern int ns_web_websocket_close(int close_id);
extern char *ns_web_websocket_search(char *mesg, char *lb, char *rb);
extern char *ns_web_websocket_read(int con_id, int timeout, int *resp_sz);
extern int ns_web_websocket_check(char *mesg, char *check_value , int check_action);

extern int ns_page_think_time(double page_think_time);
extern int ns_exit_session(void); 

/* SockJs APIs */
extern int ns_sockjs_close(int close_id);

/*Click Script API's :BEGIN */
extern int ns_click_api(int page_id, int clickaction_id);
/*Click Script API's :END */

/*Form API's :BEGIN */
extern int ns_set_form_body(char *form_buf_param_name, char *form_body_param_name, int ordinal, int num_args, ...);
extern int ns_set_form_body_ex(char *form_buf_param_name, char *form_body_param_name, char *in_str);
/*Form API's :END */

/* TR069 ******/
extern int ns_tr069_cpe_invoke_inform(int page_id);
extern int ns_tr069_cpe_invite_rpc(int page_id);
extern int ns_tr069_cpe_execute_get_rpc_methods(int page_id);
extern int ns_tr069_cpe_execute_set_parameter_values(int page_id);
extern int ns_tr069_cpe_execute_get_parameter_values(int page_id);
extern int ns_tr069_cpe_execute_get_parameter_names(int page_id);
extern int ns_tr069_cpe_execute_set_parameter_attributes(int page_id);
extern int ns_tr069_cpe_execute_get_parameter_attributes(int page_id);
extern int ns_tr069_cpe_execute_reboot(int page_id);
extern int ns_tr069_cpe_execute_download(int page_id);
extern int ns_tr069_cpe_execute_add_object(int page_id);
extern int ns_tr069_cpe_execute_delete_object(int page_id);
extern int ns_tr069_cpe_transfer_complete(int page_id);

extern int ns_tr069_get_rfc(int wait_time);
extern int ns_tr069_register_rfc (char *ip, unsigned short port, char *url);
extern int ns_tr069_get_periodic_inform_time(void);
extern int ns_tr069_get_periodic_inform_time_ex();

// Done for TR069 mapping as we should have good naming for tr069

// NETSTORM DIAGNOSTICS
extern void *ns_malloc(int size);
extern void *ns_realloc(void *ptr, int size, int old_size);
extern void ns_free(void *ptr, int size);
/* DOS SYN ATTACK */
extern void ns_dos_syn_attack(char *source_ip_add, char *dest_ip_add, unsigned int dest_port, int num_attack);


/* YAHOO PROTOCAL */
extern int ns_ymsg_logout_ext();
extern int ns_ymsg_send_chat_ext(char *my_yahoo_id, char *dest_yahoo_id, char *chat_msg);
extern int ns_ymsg_login_ext(char *yahoo_id, char *password, int inital_status, int debug_level);
//extern char *ns_ymsg_init_ylad();
// extern void ns_ymsg_set_globals(char *ns_local_host, char *ns_ylad, int *ns_poll_loop);

extern void *ns_ymsg_get_local_host();
extern void *ns_ymsg_get_ylad();
extern void *ns_ymsg_get_buddies();
extern int ns_ymsg_get_connection_tags();

extern void ns_ymsg_set_local_host(void *ptr);
extern void ns_ymsg_set_ylad(void *ptr);
extern void ns_ymsg_set_buddies(void *ptr);
extern void ns_ymsg_set_connection_tags(int con_tag);

extern char *ns_red_client_ex(char *ip, int port, char *req_buf);
extern char *ns_red_client_partial_ex(char *ip, int port, char *req_buf, int num_partial);
extern void ns_set_red_rep_time_out_ex(int time_out); 
extern void ns_set_red_connect_time_out_ex(int time_out);

extern char *ns_send_data_with_delay(char *ip, int port, int arr_len, char **req_buf, int delay);

#define NS_YMSG_DECLARE_GLOBAL() \
  void *ns_local_host; \
  void *ns_ylad; \
  void *ns_buddies; \
  int ns_connection_tags;

#define NS_YMSG_GET_GLOBAL() \
{ \
  ns_local_host = ns_ymsg_get_local_host(); \
  ns_ylad = ns_ymsg_get_ylad(); \
  ns_buddies = ns_ymsg_get_buddies(); \
  ns_connection_tags = ns_ymsg_get_connection_tags(); \
}


#define NS_YMSG_SET_GLOBAL() \
{ \
  ns_ymsg_set_local_host(ns_local_host); \
  ns_ymsg_set_ylad(ns_ylad); \
  ns_ymsg_set_buddies(ns_buddies); \
  ns_ymsg_set_connection_tags(ns_connection_tags); \
}

/* End YAHOO PROTOCOL */


/*External browser */
#define NS_RBU_PAGE_LOAD_WAIT_TIME       60               // Time taken to load particular page (time in sec).
#define NS_RBU_ENABLE_HAR_RENAMEING      1                // This will indicate har file have to be renamed or not? on runtime 
				                          // 1 for rename the har file
#define NS_RBU_VNC_DISPLAY_ID            "1"              // This is vncserver display number 
#define NS_RBU_HAR_LOG_DIR               "logs"           // Directory where har files accumulated
#define NS_RBU_DEFAULT_USER_PROFILE      "default"        // Browser profile 


/* Checkpoint fail condition */
#define NS_CP_IGNORE             -1
#define NS_CP_FAIL_ON_FOUND       0 // Fail
#define NS_CP_FAIL_ON_NOT_FOUND   1

/* Checkpint Action */
#define NS_CP_ACTION_STOP      	0
#define NS_CP_ACTION_CONTINUE  	1

/*Checkpoint regex*/
#define NS_CP_NO_REGEX_IC 0 // No regex, no ignore case
#define NS_CP_REGEX       1
#define NS_CP_IC          2
#define NS_CP_REGEX_IC    3

#define NS_CP_SEARCH_STR 1
#define NS_CP_SEARCH_VAR_TXT 2
#define NS_CP_SEARCH_VAR_PFX 3
#define NS_CP_SEARCH_VAR_SFX 4
#define NS_CP_SEARCH_VAR_PFX_SFX 5


#define NS_SEARCH_VAR_LB 6
#define NS_SEARCH_VAR_RB 7
#define NS_SEARCH_VAR_LB_RB 8

char* ns_get_param_val(char* variable_name, char* buf, int* len);
/*External Browser*/

/* BEGIN DB APIS */

extern int ns_db_odbc_init(void);
extern int ns_db_connect(char *conn_str);
extern int ns_db_alloc_stmt_handle(void **p_stmt);
extern int ns_db_execute_direct(void *stmt, char *qstr);
extern int ns_db_get_value(void *stmt, char *retbuf, int retbuf_len);
extern int ns_db_get_value_in_file(void *stmt, char *file);
extern int ns_db_free_stmt(void *stmt);
extern int ns_db_odbc_close(void);
extern int ns_db_prepare(void *in_stmt, char *qstr);
extern int ns_db_execute(void *in_stmt);
extern int ns_db_bindparameter(void             *in_stmt, 
                               unsigned short   p_no, 
                               signed short int io_type, 
                               signed short int v_type, 
                               signed short int p_type, 
                               unsigned long    col_size, 
                               signed short int d_digit, 
                               void             *p_value_ptr, 
                               long             buf_len, 
                               long             *strlen_indptr);
/* String conversion API */
extern char *ns_decode_html(char *in_str, int in_str_len, char *out_str);
extern char *ns_encode_html(char *in_str, int in_str_len, char *out_str);
//extern char *ns_encode_base64(char *in_str, int in_str_len, char *out_str);
//extern char *ns_decode_base64(char *in_str, int in_str_len, char *out_str);

#define ns_tr069_wait	 ns_page_think_time
/*PageDump API*/
extern int ns_trace_log_current_sess();

extern int ns_save_searched_string(int in_type, char *in, int out_type, char *out, char *lb, char *rb, int  ord, int start_offset, int save_len);
extern char * ns_get_machine_type();

extern char * ns_get_schedule_phase_name();
extern int ns_get_schedule_phase_type();
extern char* ns_get_referer();
extern int ns_set_referer(char *referer);
extern int ns_stop_inline_urls();
extern int ns_get_controller_testid();
extern int ns_soap_ws_security(char *keyFile, char *certFile, int algorithm, char *token, char *digest_id, char *cert_id, char *sign_id, char *key_info_id, char* sec_token_id );
extern int ns_enable_soap_ws_security();
extern int ns_disable_soap_ws_security();
extern int ns_evp_digest(char *buffer , int len , int algo , unsigned char *hash);
extern int ns_evp_sign(char *buffer , int len , int algo , char *privKey , unsigned char *signature, int sig_size);
extern int ns_encode_base64_binary(const void* data_buf, size_t dataLength, char* result, size_t resultSize);

//Replay DB
extern void ns_db_replay_query(int *idx, int *num_parameters, char *query_param_buf);
extern int ns_db_replay_query_end(int idx, int status, char *msg);

//compress api
extern int ns_save_value_from_file(const char *file_name, const char* param_name);
extern int ns_eval_compress_param(char *in_param_name, char *out_param);
extern int ns_eval_decompress_param(char *in_param_name, char *out_param);
extern int ns_save_binary_val(const char* param_value, const char* param_name, int value_length);

//SSL api
extern int ns_set_ssl_settings(char *cert_file, char *key_file);
extern int ns_unset_ssl_settings();


/* MONGODB START */ 
#define FIND                0
#define AGGREGATE           1
#define CMD                 2

//Authentication type
#define BASIC               0

//DELETE TYPE
#define DEL_DOCUMENT        0
#define DEL_COLLECTION      1
#define DEL_DATABASE        2

#define TIMESTAMP_2012 1325376000LL

extern int ns_mongodb_connect( char *ip, int port, short auth_type, char *user, char *pass, char *dbname);
extern int ns_mongodb_select_db_coll(char *dbname , char *collname);
extern int ns_mongodb_execute_direct(char *query);
extern int ns_mongodb_collection_find(char *query, int limit);
extern int ns_mongodb_collection_insert(char *query);
extern int ns_mongodb_delete(char *dbname, char *collname, char *query, int del_type);
extern int ns_mongodb_collection_update(char *query, char *updator); 
extern char *ns_mongodb_get_val();
extern int update_user_flow_count(int id);
extern char *ns_decrypt(char *encrypt_string);
extern char *ns_eval_string_copy(char *dest, char *str, int dest_len);
extern char *ns_eval_string_copy(char *dest, char *str, int dest_len);
extern int ns_xmpp_send(int);
/* MONGODB END */

/*Cassandra Start*/
extern int ns_cassdb_connect(char *host, int port, char *user, char *pass);
extern int ns_cassdb_execute_query(char *dbname);
extern int ns_cassdb_disconnect();
extern char *ns_cassdb_get_val();
/*Cassandra End*/

extern void ns_save_command_output_in_param(char *param, char *command);
/* Run Cmd store Output in param*/


/* JMeter Start */
extern int ns_jmeter_start();
/* JMeter End */

/* BODY_ENCRYPTION */
#define NONE	 		0
#define AES_128_CBC		1
#define AES_128_CTR		2
#define AES_192_CBC		3
#define AES_192_CTR		4
#define AES_192_ECB		5
#define AES_256_CBC		6
#define AES_256_CTR		7

#define NONE			0	
#define KEY_IVEC		1
#define BODY			2
#define KEY_IVEC_BODY		3
extern unsigned char *ns_aes_encrypt(unsigned char *buffer, int buffer_len, int encryption_algo, char base64_encode_option, char *key, int key_len, char *ivec, int ivec_len, char **err_msg);
extern unsigned char *ns_aes_decrypt(unsigned char *encrypted_buffer, int encrypted_buffer_len, int encryption_algo, char base64_encode_option, char *key , int key_len, char *ivec, int ivec_len, char **err_msg);
/* BODY_ENCRYPTION */

extern unsigned char *ns_encode_base64(unsigned char *buffer, int buffer_len, char **err_msg);
extern unsigned char *ns_decode_base64(unsigned char *encoded_buffer, int encoded_buffer_len, char **err_msg);

#define RTE_SUCCESS     0
#define RTE_FAILED      1

#define NS_RTE_Init()   int _rte_status                 

#define NS_RTE_Config(X)		\
  _rte_status = RTE_SUCCESS;            \
  if(ns_rte_config(X) < 0)              \
  {                                     \
     _rte_status = RTE_FAILED;          \
  }


#define NS_RTE_Connect(host,user,pass)  \
  _rte_status = RTE_SUCCESS;            \
  if(ns_rte_connect(host,user,pass) < 0)\
  {                                     \
     _rte_status = RTE_FAILED;          \
  }

#define NS_RTE_Login()                  \
  _rte_status = RTE_SUCCESS;            \
  if(ns_rte_login() < 0)                \
  {                                     \
    _rte_status = RTE_FAILED;           \
  }                     

#define NS_RTE_Type(input)              \
  _rte_status = RTE_SUCCESS;            \
  if(ns_rte_type(input) < 0 )           \
  {                                     \
     _rte_status = RTE_FAILED;          \
  }                                   
   
#define NS_RTE_Wait_Text(text,timeout)  \
  _rte_status = RTE_SUCCESS;            \
  if(ns_rte_wait_text(text,timeout) < 0)\
  {                                     \
     _rte_status = RTE_FAILED;          \
  }       

#define NS_RTE_Wait_Sync()              \
  _rte_status = RTE_SUCCESS;            \
  if(ns_rte_wait_sync() < 0)            \
  {                                     \
     _rte_status = RTE_FAILED;          \
  }       

#define NS_RTE_Disconnect()             \
  _rte_status = RTE_SUCCESS;            \
  if(ns_rte_disconnect() < 0)           \
  {                                     \
     _rte_status = RTE_FAILED;          \
  }

#define NS_RTE_STATUS           _rte_status

// Added for script debugging feature in 4.1.13
// This flag "ENABLE_RUNLOGIC_PROGRESS" is enabled when we are compiling the script.
// It will convert printf to ns_printf and fprintf to ns_fprintf which are defining in the script only.
#ifdef ENABLE_RUNLOGIC_PROGRESS 
  #define printf ns_printf
  #define fprintf ns_fprintf
#endif

extern int ns_printf(char *format, ...);
extern int ns_fprintf(FILE *fp, char *format, ...);
extern char *ns_jwt(const char *header, const char *payload, char *key);
extern int ns_read_file(char *filename , char *filebuf);

//Protobuf API's
extern int ns_protobuf_encode(char *xml_data, int is_file_or_buffer, char *proto_fname, char *msg_type, char *enc_param);
extern void ns_protobuf_decode(char *encoded_data_param, long len, int is_param, char *proto_fname, char *msg_type, char *decoded_param);
//JMS API'S
extern int ns_ibmmq_init_producer(char *ibmmq_hostname, int ibmmq_port, char *queue_manager, char *channel,char *ibmmq_queue,
                                  char *ibmmq_userId,  char *ibmmq_password, int max_pool_size, char *error_msg);
extern int ns_ibmmq_init_consumer(char *ibmmq_hostname, int ibmmq_port,  char *queue_manager,  char *channel, char *ibmmq_queue,
                                  char *ibmmq_userId, char *ibmmq_password, int max_pool_size, char *error_msg);
extern int ns_ibmmq_set_put_msg_mode(int jpid, int put_mode, char *error_msg);
extern int ns_ibmmq_set_Connection_timeout(int jpid, double timeout, char *error_msg);
extern int ns_ibmmq_set_putMsg_timeout(int jpid, double timeout, char *error_msg);
extern int ns_ibmmq_set_getMsg_timeout(int jpid, double timeout, char *error_msg);
extern int ns_ibmmq_get_connection(int jpid, char *transaction_name, char *error_msg);
extern int ns_ibmmq_release_connection(int jpcid, char *error_msg);
extern int ns_ibmmq_put_msg(int jpcid, char *msg, int msg_len, char *transaction_name, char *error_msg);
extern int ns_ibmmq_get_msg( int jpcid, char *msg, int msg_len, char *transaction_name, char *error_msg);
extern int ns_ibmmq_close_connection(int jpcid, char *transaction_name, char *error_msg);

extern int ns_kafka_init_producer(char *kafka_hostname, int kafka_port, char *kafka_topic, char *kafka_userId,
                                  char *kafka_password, int max_pool_size, char *error_msg);
extern int ns_kafka_init_consumer(char *kafka_hostname, int kafka_port, char *kafka_topic, char *consumer_group, char *kafka_userId,
                                  char *kafka_password, int max_pool_size, char *error_msg);
extern int ns_kafka_set_security_protocol(int jpid, char *security_protocol, char *error_msg);
extern int ns_kafka_set_ssl_ciphers(int jpid, char *ciphers, char *error_msg);
extern int ns_kafka_set_ssl_key_file(int jpid, char *keyFilePath, char *keyPassword, char *error_msg);
extern int ns_kafka_set_ssl_cert_file(int jpid, char *certificateFilePath, char *error_msg);
extern int ns_kafka_set_ssl_ca_file(int jpid, char *caCertifcateFilePath, char *error_msg);
extern int ns_kafka_set_ssl_crl_file(int jpid, char *crlFilePath, char *error_msg);
extern int ns_kafka_set_put_msg_mode(int jpid, int put_mode, char *error_msg);
extern int ns_kafka_set_Connection_timeout(int jpid, double timeout, char *error_msg);
extern int ns_kafka_set_putMsg_timeout(int jpid, double timeout, char *error_msg);
extern int ns_kafka_set_getMsg_timeout(int jpid, double timeout, char *error_msg);
extern int ns_kafka_set_sasl_properties(int jpid, char *sasl_mechanism, char *sasl_username,char *sasl_password, char *error_msg);
extern int ns_kafka_get_connection(int jpid, char *transaction_name, char *error_msg);
extern int ns_kafka_set_message_header(int jpcid, char *error_msg, char *header_name, char *header_value);
extern int ns_kafka_put_msg(int jpcid, char *msg, int msg_len, char *transaction_name, char *error_msg);
extern int ns_kafka_put_msg_v2(int jpcid, char *msg, int msg_len, char *key, int key_len, char *transaction_name, char *error_msg);
extern int ns_kafka_get_msg(int jpcid, char *msg, int msg_len, char *header, int hdr_len,  char *transaction_name, char *error_msg);
extern int ns_kakfa_release_connection(int jpcid, char *error_msg);
extern int ns_kakfa_close_connection(int jpcid, char *transaction_name, char *error_msg);

extern int ns_tibco_init_producer(char *tibco_hostname, int tibco_port, int t_or_q, char *tibco_topic_or_queue, char *tibco_userId,                                     char *tibco_password, int max_pool_size, char *error_msg);
extern int ns_tibco_init_consumer( char *tibco_hostname, int tibco_port, int t_or_q, char *tibco_topic_or_queue, char *tibco_userId,                                     char *tibco_password, int max_pool_size, char *error_msg);
extern int ns_tibco_set_ssl_ciphers(int jpid, char *ciphers, char *error_msg);
extern int ns_tibco_set_ssl_pvt_key_file(int jpid, char *pvtKeyFilePath, char *error_msg);
extern int ns_tibco_set_ssl_trusted_ca(int jpid, char *trustedCACertFilePath, char *error_msg);
extern int ns_tibco_set_ssl_issuer(int jpid, char *issuerCertFilePath, char *error_msg);
extern int ns_tibco_set_ssl_identity(int jpid, char *identityFilePath, char *ssl_pwd, char *error_msg);
extern int ns_tibco_set_put_msg_mode(int jpid, int put_mode, char *error_msg);
extern int ns_tibco_set_Connection_timeout(int jpid, double timeout, char *error_msg);
extern int ns_tibco_set_putMsg_timeout(int jpid, double timeout, char *error_msg);
extern int ns_tibco_set_getMsg_timeout(int jpid, double timeout, char *error_msg);
extern int ns_tibco_get_connection(int jpid,  char *transaction_name, char *error_msg);
extern int ns_tibco_set_message_header(int jpcid, char *error_msg, char *header_name, int value_type, ...);
extern int ns_tibco_put_msg(int jpcid, char *msg, int msg_len, char *transaction_name, char *error_msg );
extern int ns_tibco_get_msg(int jpcid, char *msg, int msg_len, char *header, int header_len, char *transaction_name, char *error_msg);
extern int ns_tibco_release_connection(int jpcid, char *error_msg);
extern int ns_tibco_close_connection(int jpcid,  char *transaction_name, char *error_msg);

/*Property type */
#define NS_TIBCO_BOOLEAN 11
#define NS_TIBCO_BYTE 12
#define NS_TIBCO_DOUBLE 13
#define NS_TIBCO_FLOAT 14
#define NS_TIBCO_INTEGER 15
#define NS_TIBCO_LONG 16
#define NS_TIBCO_SORT 17
#define NS_TIBCO_STRING 18

#define    TIBCO_NON_PERSISTENT                       1
#define    TIBCO_PERSISTENT                           2
#define    TIBCO_RELIABLE                             22

#define INLINE_URLS "INLINE_URLS"
#define END_INLINE "END_INLINE"
#define MULTIPART_BODY_BEGIN "MULTIPART_BODY_BEGIN"
#define MULTIPART_BODY_END "MULTIPART_BODY_END"
#define BODY_BEGIN "BODY_BEGIN"
#define MULTIPART_BOUNDARY "MULTIPART_BOUNDARY"
#define ITEMDATA "ITEMDATA"
#define ITEMDATA_END "ITEMDATA_END"
#define ATTR_LIST_BEGIN "ATTR_LIST_BEGIN"
#define ATTR_LIST_END "ATTR_LIST_END"

#define HTTP_METHOD_GET    1
#define HTTP_METHOD_POST   2
#define NS_CONTENT_TYPE_TEXT   "text/plain"
#define NS_CONTENT_TYPE_HTML   "text/html"
#define NS_CONTENT_TYPE_JSON   "application/json"
#define NS_CONTENT_TYPE_IMAGE  "image/jpeg"

#define NORMAL   ALERT_NORMAL
#define MINOR    ALERT_MINOR
#define MAJOR    ALERT_MAJOR
#define CRITICAL ALERT_CRITICAL
#define INFO     ALERT_INFO

#define ALERT_NORMAL    0
#define ALERT_MINOR     1
#define ALERT_MAJOR     2
#define ALERT_CRITICAL  3
#define ALERT_INFO      6

#define ALERT_SUCCESS             0
#define ALERT_FAIL               -1
#define ALERT_QUEUE_FULL         -2
#define ALERT_RATE_LIMIT_EXCEED  -3

//Alert API
extern int ns_send_alert(int alert_type, char *alert_msg);
extern int ns_send_alert2(int alert_type, char *alert_policy, char *alert_msg);
extern int ns_send_alert_ex(int alert_type, int alert_method, char *content_type, char *alert_msg, int length);

// ================== || Socket APIs || =============================
//SocketAPIs - NS APIs
extern int ns_socket_open(int open_id);
extern int ns_socket_send(int send_id);
extern int ns_socket_recv(int recv_id);
extern int ns_socket_close(int close_id);

//bug 93672: gRPC API
extern int ns_grpc_client(int page_id);

//SocketAPIs - C APIs
extern int ns_socket_set_options(int action_idx, char *level, char *optname, void *optval, socklen_t optlen);
extern int ns_socket_get_options(int action_idx, char *level, char *optname, void *optval, socklen_t *optlen);
extern char *ns_socket_get_attribute(int action_idx, char *attribute_name);
extern int ns_socket_enable(int action_idx, char *operation);
extern int ns_socket_disable(int action_idx, char *operation);
extern int ns_socket_get_num_msg();
extern int ns_set_connect_timeout(float timeout);
extern int ns_set_send_timeout(float timeout);
extern int ns_set_send_inactivity_timeout(float timeout);
extern int ns_set_recv_timeout(float timeout);
extern int ns_set_recv_inactivity_timeout(float timeout);
extern int ns_set_recv_first_byte_timeout(float timeout);
extern int ns_socket_get_fd(int action_idx);
extern char *ns_socket_error(int *lerrno);
extern int ns_socket_start_ssl(int action_idx, char *version, char *ciphers);
extern char *ns_ascii_to_ebcdic(char *ascii_input, int ascii_len, int *con_len);
extern char *ns_ebcdic_to_ascii(char *ebcdic_input, int ebcdic_len, int *con_len);
// ==================================================================

//execute js api

extern int ns_exec_js(int inp_type, char *inp, int out_type, char *out, int size);
extern char* ns_js_error();

/*bug 79149: RDP Protocol */
//ToDo: ns_rdp_sync_on_mouse_double_click(
#define RDP_SUCCESS	0
#define RDP_ERROR	-1
#define RDP_CONN_FAIL	9
#define MOUSEMOVE_ABSOLUTE	0
#define MOUSEMOVE_RELATIVE	1

#define LEFT_BUTTON		0
#define LEFT_BUTTON_DOWN	1
#define LEFT_BUTTON_UP		2
#define RIGHT_BUTTON		3
#define RIGHT_BUTTON_DOWN	4
#define RIGHT_BUTTON_UP		5

#define KEY_PRESS_AND_RELEASE	0
#define KEY_DOWN		1
#define KEY_UP			2

int nsi_rdp_connect(char* host, char* user, char* pwd, char* domain);
int nsi_rdp_disconnect();
int nsi_rdp_key_type(int type, char *input);
int nsi_rdp_mouse_double_click(int mouseX, int mouseY, int button_type, int origin);
int nsi_rdp_mouse_click(int mouseX, int mouseY, int button_type, int origin);
int nsi_rdp_sync(int msec);
int ns_rdp(int open_id);
int ns_replace_ex(char *inp_param, char *out_param, char* find_str, char* replace_str, int ord, int ignore_case);
int ns_replace(char *inp_param, char* find_str, char* replace_str);

#endif // __NS_STRING_H__
