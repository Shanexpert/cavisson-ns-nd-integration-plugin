#ifndef _NS_TR069_LIB_H_
#define _NS_TR069_LIB_H_ 


#define TR069_SUCCESS  0
#define TR069_ERROR    1

//#define TR069_MAX_BLOCK_LEN	1024 * 10
#define TR069_MAX_BLOCK_LEN	(1024 * 10)

#define TR069_NAME_LEN		256
#define TR069_TYPE_LEN		16
#define TR069_VALUE_LEN		64

#define CPE_MAX_RFC_REQUESTS    3

#define GET_RPC_METHODS_LEN               13
#define SET_PARAMETER_VALUES_LEN          18
#define GET_PARAMETER_VALUES_LEN          18
#define GET_PARAMETER_NAMES_LEN           17
#define SET_PARAMETER_ATTRIBUTES_LEN      22
#define GET_PARAMETER_ATTRIBUTES_LEN      22
#define ADD_OBJECT_LEN                   9
#define DELETE_OBJECT_LEN                12
#define REBOOT_LEN                      6
#define DOWNLOAD_LEN                    8 

#define CLEAR_WHITE_SPACE(ptr) {while ((*ptr == ' ') || (*ptr == '\t')) ptr++;}

// Bitmask for TR069 event types used internally by netstorm
// Refer to Table 7 - Event Types of CPE Wan Management Protocol v1.1 for meaning of these events
//
#define NS_TR069_REMAP_SERVER 		      0x00000001  //value 1      

#define NS_TR069_EVENT_BOOTSTRAP 		    0x00000002  //value 1      
#define NS_TR069_EVENT_BOOT			    0x00000004//value 2
#define NS_TR069_EVENT_M_REBOOT                     0x00000008//value 2048
#define NS_TR069_EVENT_VALUE_CHANGE_PASSIVE         0x00000010
#define NS_TR069_EVENT_VALUE_CHANGE_ACTIVE          0x00000020
#define NS_TR069_EVENT_PERIODIC			    0x00000040 
#define NS_TR069_EVENT_GOT_RFC                      0x00000080 
#define NS_TR069_EVENT_WAITING_FOR_RFC_FROM_ACS     0x00000100  // TR069 - Set if waiting for RFC with timer
#define NS_TR069_EVENT_M_DOWNLOAD                   0x00000200  // Got download req from ACS
#define NS_TR069_EVENT_TRANSFER_COMPLETE            0x00000400  // Transfer is complete


//Mapped with tr069EventName Array
//

#define NS_TR069_BOOTSTRAP_ID                  0
#define NS_TR069_BOOT_ID                       1
#define NS_TR069_PERIODIC_ID                   2
#define NS_TR069_SCHEDULED_ID                  3 // Not supported
#define NS_TR069_VALUE_CHANGE_ID               4
#define NS_TR069_KICKED_ID                     5 // Not supported
#define NS_TR069_GOT_RFC_ID                    6
#define NS_TR069_TRANSFER_COMPLETE_ID          7
#define NS_TR069_DIAGNOSTIC_COMPLETE_ID        8 // Not supported
#define NS_TR069_REQUEST_DOWNLOAD_ID           9 // Not supported
#define NS_TR069_AUTONOMOUS_TRANSFER_COMPLETE_ID 10 // Not supported
#define NS_TR069_M_REBOOT_ID                  11
#define NS_TR069_M_SCHEDULE_INFORM_ID         12 // Not supported
#define NS_TR069_M_DOWNLOAD_ID                13
#define NS_TR069_M_UPLOAD_ID                  14 // Not supported

/*
#define TR069_EVENT_SCHEDULED                       0x00000008  //value 8
#define TR069_EVENT_KICKED                          0x00000020  //value 32
#define TR069_EVENT_CONNECTION_REQUEST              0x00000040  //value 64
#define TR069_EVENT_TRANSFER_COMPLETE               0x00000080  //value 128
#define TR069_EVENT_DIAGNOSTICS_COMPLETE            0x00000100  //value 256
#define TR069_EVENT_REQUEST_DOWNLOAD                0x00000200  //value 512
#define TR069_EVENT_AUTONOMOUS_TRANSFER_COMPLETE    0x00000400  //value 1024
#define TR069_EVENT_M_SCHEDULE_INFORM               0x00001000  //value 4096
#define TR069_EVENT_M_DOWNLOAD                      0x00002000  //value 8192
#define TR069_EVENT_M_UPLOAD                        0x00004000  //value 16384
*/
#define TR069_MAX_EVENTS			    15  // Increase if below two are uncommented
// Following are not supported by netstorm now
// #define TR069_EVENT_M_VENDOR_SPECIFIC_METHOD      0x00008000  //value 32768
// #define TR069_EVENT_X_OUI_EVENT                   0x00010000  //value 65536
	
#define NS_TR069_DATA_INSERT_INVALID_PARAM    -1
#define NS_TR069_DATA_INSERT_RONLY            -2
#define NS_TR069_DATA_INSERT_SUCCESS           0
#define NS_TR069_DATA_INSERT_FAILURE           1

#define NS_TR069_DATA_GET_INVALID_PARAM       -1
#define NS_TR069_DATA_GET_SUCCESS              0
#define NS_TR069_DATA_GET_FAILURE              1

#define NS_TR069_OPTIONS_AUTH                  0x00000001
#define NS_TR069_OPTIONS_TREE_DATA             0x00000002

#define RETREIVE_TR069_BUF_DATA(offset) (g_tr069_big_buf + offset)

extern short         total_inform_parameters;
extern short *tr069_inform_paramters_indexes;

typedef int   (*StrToIndex)(const char*, unsigned int);
typedef char *(*HashCodeToStr)(unsigned int);
typedef int   (*HashCodeToIndex)(unsigned int);
typedef int   (*IndexToHashCode)(unsigned int);
typedef char *(*IndexToStr)(unsigned int);

extern StrToIndex      tr069_path_name_to_index;
extern HashCodeToStr   tr069_hash_code_to_path_name;
extern HashCodeToIndex tr069_hash_code_to_index;
extern IndexToHashCode tr069_index_to_hash_code;
extern IndexToStr      tr069_index_to_path_name;

extern char *tr069_get_name_from_index(int param_path_idx, int *param_len);

extern int tr069_acs_url_idx;
extern char tr069_block[];

extern void tr069_parent_init();
extern void tr069_nvm_init(int num_users);

typedef void (*TR069GetInformParamCB)(char *name, char *type, char *value, char **cur_ptr, int *num_params);
typedef void (*TR069GPNblockCB)(char *name, char *writable,  char **cur_ptr, int *num_params);
typedef void (*TR069GPAblockCB)(char *name, int notification, char *accessList, char **cur_ptr, int *num_params);
typedef void (*TR069GPVblockCB)(char *name, char *type, char *value, char **cur_ptr, int *num_params);
typedef void (*TR069GetRPCMethodsblockCB)(int len, char *param_start, char **cur_ptr, int *num_params);

extern inline char *tr069_get_event_name(int event_id);
extern inline int tr069_parse_req_and_get_rpc_method_name(VUser *vptr, int ret);
extern inline void tr069_fill_response(char *forWhich, char *ParamName, char *ParamValue);
extern void kw_set_tr069_acs_url(char* , char* , int);
extern void kw_set_tr069_options(char* conf_line, int *options, int flag);
extern void kw_set_tr069_cpe_data_dir(char *buf, char *tr069_file_dir, int flag);
extern int kw_set_tr069_cpe_download_time(char *buf, int *min_time, int *max_time, int flag);
extern int kw_set_tr069_cpe_reboot_time(char *buf, int *min_time, int *max_time, int flag);
extern int kw_set_tr069_cpe_periodic_inform_time(char *buf, int *min_time, int *max_time, int flag);
extern void tr069_vuser_data_init(VUser *vptr);

extern char*
tr069_get_full_param_values_str(VUser *vptr, char *param_name, int param_name_len);

extern int 
tr069_get_full_param_values_num(VUser *vptr, char *param_name, int param_name_len);

extern int tr069_get_param_values_with_cb(VUser *vptr, char *param_name, int param_name_len,
                                   TR069GPVblockCB tr069_CB, char **cur_ptr, 
                                   int *num_params);
extern int tr069_get_param_attributes_with_cb(VUser *vptr, char *param_name, int param_name_len,
                                       TR069GPAblockCB tr069_CB, char **cur_ptr, int *num_params);
extern int tr069_get_param_names_with_cb(VUser *vptr, char *param_name,
                                         int param_name_len, TR069GPNblockCB tr069_CB,
                                         char **cur_ptr, int *num_params);

extern int tr069_set_param_values(VUser *vptr, char *param_name, int param_name_len, 
                                        char *param_key, int param_key_len);
extern int tr069_set_param_attributes_with_cb(VUser *vptr, char *param_name, int param_name_len, 
                                        char *attribute, int attribute_len);

extern char* tr069_is_cpe_new(VUser *vptr);
extern void get_values_for_get_parameter_values(VUser *vptr, char *rep_ptr);
extern void tr069_set_reboot_cmd_key (VUser *vptr, char *cmd_key, int cmd_key_len);
extern void tr069_clear_vars();
extern int tr069_extract_num_param_frm_req (char *data_buf);
extern int get_tr069_data_value(char *data_buf, char **value, int *len, char *str_to_search_start, char *str_to_search_end);
extern void tr069_dump_cpe_data_for_each_users(int num_users, char *tr069_data_writeback_location);
extern char* tr069_get_abs_param_values(VUser *vptr, char *param_name, int param_name_len);
extern void  tr069_switch_acs_to_acs_main_url(VUser *vptr, char *new_url);
extern int tr069_get_data_value(char *data_buf, char **value, int *len,
                                char *str_to_search_start,
                                char *str_to_search_end);
extern inline int extract_and_set_param_key(VUser *vptr, char *cur_ptr);
extern inline void tr069_init_addr_for_admin_ip();
#endif
