#ifndef NS_CAVMAIN_H 
#define NS_CAVMAIN_H

#include "util.h"
#include "ns_msg_def.h"
#include "ns_msg_com_util.h"
#include "nslb_map.h"

#define MAX_OUTPUT_BUF_SIZE     16384
#define MAX_CM_MON_ID_SIZE      256
#define MAX_CMD_LEN             4096
#define READ_BUF_SIZE		4*1024
#define PATH_SIZE		1024
/*******************************************/
#define NS_ZERO                         0
#define NS_WAIT_FOREVER                 1
#define NS_EPOLL_MAXFD 			(1024*32)
#define SM_PARITION_ID_SZ		16

#define NS_ERR_BUF_SIZE		8*1024
#define TOT_REQ_FIELDS			7
#define SM_MON_ID_MAX_LEN	256
#define CM_MON_STOPPED          "Monitor Stopped Successfully"
#define CM_MON_STOPPED_ERR      "Monitor Stopped with Error"
#define CM_MON_STARTED          "Monitor Started successfully"
#define CM_MON_INVALID_REQ      "Invalid SM Request, as there is No MON Req running for Monitor ID"

#define CAVGEN_VERSION_1        1
#define CAVGEN_VERSION_2        2

#define NSLB_MAP_INIT_SIZE      1024
#define NSLB_MAP_DELATE_SIZE    128
#define NORM_ID_NOT_FOUND       -2

extern NSLBMap *sm_map;
extern unsigned short cm_port_number;
#ifndef CAV_MAIN
extern char *ns_nvm_scratch_buf;
#else
extern __thread char *ns_nvm_scratch_buf;
#endif

#define CM_GET_MAP() sm_map
#define CM_GET_EPOLL_FD() g_msg_com_epfd
#define CM_GET_LISTENER_FD() listen_fd

#define CM_GET_LISTENER_FOR_CHILD_FD() parent_listen_fd
#define NS_PUT_ERR_ON_STDERR(msg)\
{ \
  fprintf(stderr, "%s\n", msg);\
}

#define SM_SET_CAVGEN_VERSION(version) global_settings->cavgen_version = version;
#define SM_GET_CAVGEN_VERSION() global_settings->cavgen_version

#define NS_GET_ERR_CODE() g_ns_err.code
#define NS_GET_ERR_MSG()  g_ns_err.msg
#define NS_RESET_ERR() \
{ \
  g_ns_err.code = 0; \
  memset(g_ns_err.msg, 0, NS_ERR_BUF_SIZE);\
}

#define NS_GET_ERR_BUF() g_ns_err.msg

#define NS_INIT_GROUP_N_GLOBAL_SETTINGS()\
{\
  NSLB_MALLOC_AND_MEMSET(group_default_settings, sizeof(GroupSettings), "GroupSettings", -1, NULL);\
  NSLB_MALLOC_AND_MEMSET(global_settings, sizeof(Global_data), "Global_data", -1, NULL);\
  global_settings->test_start_time = get_ms_stamp();\
}

#define NS_FREE_GROUP_N_GLOBAL_SETTINGS()\
{\
  NSLB_FREE_AND_MAKE_NULL(group_default_settings, "GroupSettings", -1, NULL);\
  NSLB_FREE_AND_MAKE_NULL(global_settings, "Global_data", -1, NULL);\
}

/*******************************************/
typedef enum {
  CM_ERROR = -1,
  CM_SUCCESS
} CmStatus;
   
typedef enum {
  SM_OPCODE,
  SM_MONITOR_INDEX,
  SM_MONITOR_ID,
  SM_MONITOR_NAME,
  SM_MONITOR_TYPE,
  SM_TIER_NAME,
  SM_SRV_NAME,
  SM_PARTITION_ID,
  SM_TR_NUM,
  SM_OPERATION,
  SM_KEYWORD,
  /*SM_REQ_TOT_FIELDS, *//*it is for total number/count of request fields*/
  SM_REQ_MAX_FIELDS  /*in includes req name as well*/
} SmReqFields;
/*NsString: for holding a string and corresponding length*/
typedef struct NsString
{
  int len;
  char* value;
}NsString;
       
#define SM_REQ_TOT_FIELDS 	9
/*SmRequest: Synthetic Monitoring request structure*/
typedef struct SmRequest 
{
  int opcode;
  int child_id;  /*child which can be  process request/TR/orThreadID*/
  Msg_com_con *mccptr;
  /*SM REQ Fields are: monitor_id, monitor_name, monitor_type, tier_name, server_name, partition_id, tr_number*/
  NsString req_field_arr[SM_REQ_TOT_FIELDS]; 
  pthread_mutex_t lock;
  
  struct SmRequest *child_prev;
  struct SmRequest *child_next;
}SmRequest;

typedef struct cm_thread_args
{
  SmRequest *smReq;
  char cmd_buf[MAX_OUTPUT_BUF_SIZE + 1];
}CMThreadArgs;

typedef struct NSError
{
  int code;
  char msg[NS_ERR_BUF_SIZE];
}NSError;

extern NSError g_ns_err;

typedef struct {
 char scenario_path[PATH_SIZE];
 char script_path[PATH_SIZE];
 char hpd_path[PATH_SIZE];
}FilePath;

enum {CMON_ENV_PATH, SCENARIO_PATH, SCRIPT_PATH};
extern FilePath g_file_path;
extern int parent_listen_fd;
//extern int g_parent_child_port_number;
extern unsigned short parent_port_number;
#define NS_GET_SCENARIO_PATH()  g_file_path.scenario_path 
#define NS_GET_SCRIPT_PATH()    g_file_path.script_path 
#define NS_GET_FILE_UPLOAD_URL()  g_file_path.hpd_path 
/*bug id: 101320: using g_ns_ta_dir instead of g_ns_wdir*/
#define NS_SET_SCENARIO_PATH(mon_id) \
{\
  NSDL2_MESSAGES(NULL, NULL, "ta_dir=%s mon_id=%s", GET_NS_TA_DIR(), mon_id); \
  sprintf(g_file_path.scenario_path, "%s%s%s.conf", GET_NS_TA_DIR(), NS_DEFAULT_SCENARIO_PATH, mon_id);\
  NSDL2_MESSAGES(NULL, NULL, "scenario path=%s", g_file_path.scenario_path); \
}

/*bug id: 101320: using g_ns_ta_dir instead of g_ns_wdir*/
#define NS_SET_SCRIPT_PATH(monid) \
{\
  NSDL2_MESSAGES(NULL, NULL, "ta_dir= %s monid=%s", GET_NS_TA_DIR(), monid); \
  sprintf(g_file_path.script_path, "%s%s%s", GET_NS_TA_DIR(), NS_DEFAULT_SCRIPT_PATH, monid);\
  NSDL2_MESSAGES(NULL, NULL, "script path=%s", g_file_path.script_path); \
}  

#define CM_GET_PARENT_PORT_NUM() parent_port_number
//g_parent_child_port_number 
#define ASSIGN_ADDRESS_AND_VALUE_LEN(value, value_len, addr, len)\
{ \
  NSDL2_MESSAGES(NULL, NULL,"recieved addr=%p len=%d", addr, len);\
  *value_len = len;\
  *value = addr; \
  NSDL2_MESSAGES(NULL, NULL,"after assignment, value=%p value_len= %d", *value, *value_len);\
}

#define FILL_SM_REQ_FIELDS(des, src, len) \
{ \
  NSDL2_MESSAGES(NULL, NULL,"src =%s", src); \
  strncpy(des, src, len);\
  des[len] = '\0'; \
  NSDL2_MESSAGES(NULL, NULL,"value =%s len =%d", des, len); \
}



NormObjKey req_field_normtbl;
/* MSG_HDR 
int msg_len; ==>packet length
int opcode; \   ==>opcode
int child_id; ==> nvm idx
int ns_version; ==> mon_type or operation[update, pause, resume]
int gen_rtc_idx; ==> mon_index 
int testidx; ==> tr number
long partition_idx; ==> partition id
double abs_ts; ==> time when we sent req ro child*/

typedef struct
{
  /* Following HDR should be same in parent_child and avgtime msg */
  MSG_HDR
  char *data;
} CM_MON_REQ;

typedef struct
{
 NsString tier;
 NsString server;
 NsString hpd_url;
}CM_CHILD_CONFIG;

typedef struct
{
 // NsString mon_index; ToDo: check with GUI if it is interger or would be string
  NsString mon_id;
  NsString mon_name;
  NsString tier_name;
  NsString server_name;
} CM_MON_INIT;

typedef struct
{
  NsString mon_id;
} CM_MON_STOP;

typedef struct
{
  NsString mon_id;
  NsString info;
} CM_MON_UPDATE;

typedef struct
{
  NsString mon_id;
  NsString msg;
} CM_CHILD_STATUS;

typedef struct
{
  NsString mon_id;
  NsString data;
} CM_CHILD_DATA;



//extern Msg_com_con *g_msg_com_con;
extern Msg_com_con **g_msg_com_con_arr;
int cm_main();

/*********************************************************************************/
inline void ns_register_sm_req();
inline void ns_register_sm_req_v2();
int cm_init_thread_manager(int trace_fd);
int cm_init_thread_manager_v2(int trace_fd);
int cm_process_sm_start_req(Msg_com_con*);
int cm_process_sm_stop_req(Msg_com_con*);
int cm_process_sm_update_req(Msg_com_con*);
int cm_process_sm_pause_req(Msg_com_con*) ;
int cm_process_sm_resume_req(Msg_com_con*);
int cm_process_sm_config_req(Msg_com_con*);
int cm_process_sm_start_req_v2(Msg_com_con*);
int cm_process_sm_stop_req_v2(Msg_com_con*);
int cm_process_sm_close_req(Msg_com_con*);
int cm_process_sm_close_req_v2(Msg_com_con*);
int cm_process_sm_update_req_v2(Msg_com_con*);
int cm_process_sm_pause_req_v2(Msg_com_con*) ;
int cm_process_sm_resume_req_v2(Msg_com_con*);
int cm_process_sm_config_req_v2(Msg_com_con*);
//int cm_handle_event(Msg_com_con *mccptr, int events);
int cm_handle_event_from_tool(Msg_com_con *mccptr, int events);
int cm_handle_event_from_child(Msg_com_con *mccptr, int events);
int cm_handle_read(Msg_com_con *mccptr);
int cm_handle_client_close(Msg_com_con *mccptr);
int cm_parse_and_process_sm_request(Msg_com_con *mccptr);
int cm_parse_request(Msg_com_con *mccptr);
int cm_get_req_opcode(char* str);
int cm_validate_sm_req(Msg_com_con *mccptr, int opcode, char req_arr[][256], int arr_sz);
int cm_process_sm_request(Msg_com_con*);
void create_nsport_file(char *port_file, unsigned short port_num);
void cm_wait_forever();
SmRequest* init_and_get_sm_req(Msg_com_con *mccptr, int opcode, char req_arr[][256], int arr_size);
int cm_check_for_space_char(char arr[][256], int arr_szie);
int sm_get_req_val(char *in[], int in_size, char out[][256] );
int ns_get_keyword(char* filePath, char* keyword, char *out);
int cm_process_child_request(Msg_com_con* mccptr);
int cm_send_msg_to_child(Msg_com_con* mccptr, SmRequest* sm_req);
int cm_run_stop_cmd(SmRequest *sm_req);
int cm_init_new_listner_socket(int *listner_fd, int con_type);
int cm_process_child_msg(Msg_com_con*, CM_MON_REQ*);
int cm_get_available_child_idx(void);
int cm_send_reply_and_release_req(char *mon_id, char *status, char *msg);
void cm_send_reply_v2(Msg_com_con *mccptr, char* reply);
void cm_close_conn_and_release_req(SmRequest *sm_req);
void cm_unpack_status_msg(CM_MON_REQ *rcv_data, char *out_mon_id, char *out_msg);
void cm_unpack_stop_msg(CM_MON_STOP *rcv_data, char *out_mon_id);
void cm_prep_and_send_msg_to_tool(SmRequest *sm_req, char* status_txt, char *status_msg);
void cm_unpack_init_msg(CM_MON_INIT *rcv_data, char *out_mon_id, char* out_mon_name, char *out_tier_name, char *out_srv_name);
int kw_set_cavgen_version(char *buf, int *to_change, char *err_msg, int runtime_flag);
/*********************************************************************************/

#endif
