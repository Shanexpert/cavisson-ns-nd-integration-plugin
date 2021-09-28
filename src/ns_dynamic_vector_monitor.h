/******************************************************************
 * Name    : ns_dynamic_vector_monitor.h 
 * Author  : Archana
 * Purpose : This file contains declaration of macros and methods. 
 * Note:
 * Modification History:
 * 03/11/09 - Initial Version
*****************************************************************/
#include "util.h"
#include <stdbool.h>
#include "nslb_get_norm_obj_id.h"
#include "../../base/topology/topolib_v1/topolib_structures.h"
#ifndef _NS_DYNAMIC_VECTOR_MONITOR_H
#define _NS_DYNAMIC_VECTOR_MONITOR_H 
#include "url.h"
//Starting events for Check Monitors
#define JSON_CHECK_MONITOR_EVENT_BEFORE_TEST_IS_STARTED             "1"   //"Before test is started"
#define JSON_CHECK_MONITOR_EVENT_START_OF_TEST                      "2"   //"Start of Test"
#define JSON_CHECK_MONITOR_EVENT_START_OF_PHASE                     "3"   //"At Start of the Phase"
#define JSON_CHECK_MONITOR_EVENT_AFTER_TEST_IS_OVER                 "90"  //"After test is Over"



#define NA_SM_GDF                     1
#define CM_GET_LOG_FILE               2
#define TIBCO_SVC_TIME                3
#define SERVICE_STATS                 4
#define NA_GENERIC_DB                 5
#define ORACLE_STATS_GDF              6
#define NA_SR                         7
#define NA_GENERICDB_MYSQL            8
#define NA_GENERICDB_ORACLE           9
#define NA_GENERICDB_POSTGRES         10
#define NA_GENERICDB_MSSQL            11
#define NA_GENERICDB_MONGO_DB         12


//End events for Check Monitors
#define JSON_TILL_TEST_COMPLETION          "1"
#define JSON_COMPLETE_SPECIFIED_EXECUTIONS "2"
#define JSON_TILL_COMPLETION_OF_PHASE      "3"

//Mon-types
#define CHECK_MON_TYPE                     0   //for check monitors
#define SERVER_SIGNATURE_MON_TYPE          1   //for server signature
#define STD_MON_TYPE                       2   //for standard monitors
#define DELETE_STD_MON_TYPE                3   //for delete monitors
#define LOG_MON_TYPE			   4   //for log monitor
#define CUSTOM_GDF_MON_TYPE                5
#define DELETE_CUSTOM_GDF                  6   //For deleting custom_gdf's like Log parse or get-log-file. Or custom-mon-gdf. Whose gdfs are not predefined.
#define BATCH_JOB_TYPE                     7   // for batch job monitors
#define DELETE_CHECK_MON_TYPE              8   // for deleting check monitor
#define DELETE_BATCH_JOB_TYPE              9   // for deleting batch job monitor 

#define NUM_KUBE_VECTOR_FIELDS 8

#define NEGATIVE_G_MON_IDX     -1


#define GET_MP(option, value) \
{ \
  if(!strcmp(option, "H")) \
    value = METRIC_PRIORITY_HIGH; \
  else if(!strcmp(option, "M")) \
    value = METRIC_PRIORITY_MEDIUM; \
  else if(!strcmp(option, "L")) \
    value = METRIC_PRIORITY_LOW; \
}

//This is the structure used to fill input array with field element.
//used in copy_vector_data function.
typedef struct kub_ip_mon_vector_fields
{
  char  input[256];
  char  pattern[256];
}kub_ip_mon_vector_fields;

typedef struct SERVER_MP{
   char mp;
   char *server;
}SERVER_MP;

typedef struct JSON_MP{
  char mp;
  char *tier;
  int total_server_entries;
  int max_server_entries;
  SERVER_MP *server_mp_ptr;
} JSON_MP;
  

//structure for getting the values of TierServerType, VectorReplaceFrom and VectorReplaceTo

typedef struct JSON_info{
  bool any_server;  //for string "Any" in specific server field

  bool dest_any_server_flag; //for dest-any-server in Json to know whether it is used or not
  char *dest_any_server_arr[64];  
  char *g_mon_id;  //this is the id of monitor which is passed in json_info_struct and it is unique for all monitors
  char tier_server_mapping_type;
  char connect_mode;
  char agent_type;      //agent type may cmon or BCI in case of config json default is connect to cmon
  char metric_priority;
  char mon_type;
  char *vectorReplaceFrom;
  char *vectorReplaceTo;
  char *javaClassPath;
  char *javaHome;
  char *namespace;
  char *cmon_pod_pattern;
  char *init_vector_file;
  char *config_json;
  char *mon_name;
  char *instance_name;
  char *args;           //holds te argument or options required to run a program
  char *old_options;
  char *options;
  char skip_breadcrumb_creation;           //used for metric hierachy prefix 
  char *os_type;
  char *use_agent;    //
  char *app_name; 
  char *pgm_type;     //stores the program type such as  java,linux,c etc. 
  int run_opt;        //run option which is 2 in most the cases
  JSON_MP *json_mp_ptr;
  int total_mp_enteries;
  char use_lps;
  int no_of_dest_any_server_elements;
  int frequency;
  char is_process;
  int sm_mode;       // this will be 1 or 2 if 1 means dont need to retry and if 2 we have to retry if fails.
  int mon_info_index;   // this is mon_config_list_ptr index
  void* std_mon_ptr;
  int gdf_flag;
  int generic_gdf_flag;
  int lps_enable;
  char *tier_name;
  int tier_idx;
  char *server_name;
  int server_index;
  
} JSON_info;

typedef struct DynamicVectorMonitorInfo
{
  char con_type; // Must be first field
  char conn_state;          // connection state: stop,abort,running
  char is_coh_new_format;
  char skip_breadcrumb_creation;
  int dvm_retry_attempts;
  int bytes_remaining;
  int send_offset;
  int timestamp; //to decide connection timeout in non-blocking

  //File descriptor for TCP/FTP socket made to make connection with CS.
  int fd;                   
  char *monitor_name;         // monitor name
  char *cs_ip;               // Create Server IP
  char *cavmon_ip;               // Cavmon IP
  int cs_port;             // Create Server Port
  short cavmon_port;             // Cavmon Port
  char *gdf_name;
  char *vector_list;         // vector name list with space seperated 
  int option;                // 1 is for run periodically and 2 is for run once
  char *pgm_path;            // Monitor program name with or without path
  char *pgm_args;            // program argument given by user eg; filename, etc.
  char *pgm_name_for_vectors; //Program name with or without path to get vector list from CS
  char *pgm_args_for_vectors; //Program args with or without path to get vector list from CS

  int access;                // 1 as local (default) and 2 as remote
  char *rem_ip;              // remote IP
  char *rem_username;        // remote User name
  char *rem_password;        // remote Password

  int status;                // Status of the dynamic vector monitor - Pass/Fail
  int use_lps;               // to indicate whether to make connection through LPS or not

  // For keeping read line if partial
  char *partial_buffer;      // partial message
  char *origin_cmon;  //Added for Heroku, this is the origin cmon server name or ip address as given by origin cmon to proxy cmon
  char *init_vector_list;  //for ND's BT and backend call monitors in which vectors are passed from file
  int server_index; 
  char *appname_pattern; 
  int is_outbound;
  char *tier_name;
  char *server_name;
  char *pod_name;
  int nid_table_row_idx;
  int is_nd_monitor;
  int frequency;
  JSON_info *json_element_ptr;     //This structure contains json elements 
} DynamicVectorMonitorInfo;

typedef struct
{
  char *gdf_name;
  short key_size;
} VectorGdfHash;

VectorGdfHash *vector_gdf_hash;
int total_vector_gdf_hash_entries;
int max_vector_gdf_hash_entries;

//Error Status of dynamic vector monitor. These macros in +ve num and start with 0 since these status used in array to print error msg for failed status monitor.
#define DYNAMIC_VECTOR_MONITOR_FAIL         0
#define DYNAMIC_VECTOR_MONITOR_CONN_FAIL    1
#define DYNAMIC_VECTOR_MONITOR_CONN_CLOSED  2
#define DYNAMIC_VECTOR_MONITOR_READ_FAIL    3
#define DYNAMIC_VECTOR_MONITOR_SEND_FAIL    4
#define DYNAMIC_VECTOR_MONITOR_EPOLLERR     5
#define DYNAMIC_VECTOR_MONITOR_SYSERR       6
/* DYNAMIC_VECTOR_MONITOR_NOT_STARTED used : 
   - To set Status of the dynamic vector monitor at init time.
   - To set Error Status of dynamic vector monitor in case of time out if dynamic vector mon not run till test completed.
*/
#define DYNAMIC_VECTOR_MONITOR_NOT_STARTED  7 

//Status of the dynamic vector monitor should not be same as Error Status of dynamic vector monitor
#define DYNAMIC_VECTOR_MONITOR_PASS         8

//Return status of dynamic vector result for dynamic vector monitor
#define DYNAMIC_VECTOR_MONITOR_DONE_WITH_ALL_FAIL  0
#define DYNAMIC_VECTOR_MONITOR_DONE_WITH_SOME_FAIL 1
#define DYNAMIC_VECTOR_MONITOR_DONE_WITH_ALL_PASS  2

#define DYNAMIC_VECTOR_MONITOR_DEFAULT_TIMEOUT   15 //default value 15 seconds for timeout

//String line that send by create server 
#define DYNAMIC_VECTOR_MONITOR_FAILED_LINE "Error:"
#define DYNAMIC_VECTOR_MONITOR_EVENT       "Event:"
#define MAX_STRING_LENGTH 256

#define CHECK_MONITOR 1
#define STANDARD_MONITOR 2
#define SERVER_SIGNATURE 3
#define LOG_MONITOR 4
#define CUSTOM_GDF_MON 5
#define BATCH_JOB 6

extern void copy_instance_name_in_json_info_ptr(char *instance, JSON_info *json_info_ptr);
extern NormObjKey *specific_server_id_key;
extern int kw_set_dynamic_vector_monitor(char *keyword, char *buf, char *server_name, int use_lps_flag, int runtime_flag, char *pod_name, char *err_msg, char *file_path, JSON_info *json_info_ptr, char skip_breadcrumb_creation);
extern int run_dynamic_vector_monitor();   //call from ns_parent.c
extern void add_dynamic_custom_monitors(int runtime_flag, char *err_msg); //call from ns_gdf.c
extern int kw_set_dynamic_vector_timeout(char *keyword, char *buf, char *kw_buf, char *err_msg, int runtime_flag);
extern int kw_set_continue_on_monitor_error(char *buf, char *err_msg, int runtime_flag);
/*
extern void set_vector_and_data_args_for_cus_gdf(char *fields[], JSON_info *json_info_ptr, char *dvm_vector_arg_buf, char *dvm_data_arg_buf ,int *use_lps_flag);
extern void set_vector_arg_for_cus_gdf(char *fields[], char *dvm_vector_arg_buf, char *dvm_hv_vector_prefix, JSON_info *json_info_ptr);
extern void set_data_args_for_cus_gdf(char *fields[],char *dvm_data_arg_buf, char *dvm_hv_vector_prefix, JSON_info *json_info_ptr);
extern void set_lps_flag_for_cus_gdf(int *use_lps_flag, JSON_info *json_info_ptr);
*/




//For HPD port
extern void get_and_set_hpd_port();
//ND HOT SPOT MONITOR
extern void kw_set_nd_enable_hot_spot_monitor(char *keyword, char *buf, char *err_msg);
//ND Nodejs Server monitor
extern void kw_set_nd_enable_nodejs_server_monitor(char *keyword, char *buf, char *err_msg);
//ND Nodejs Async Event monitor 
extern void kw_set_nd_enable_nodejs_async_event_monitor(char *keyword, char *buf, char *err_msg);
//ND FLOWPATH MONITOR
extern void kw_set_nd_enable_fp_monitor(char *keyword, char *buf, char *err_msg);
//ND METHOD MONITOR
extern void kw_set_nd_enable_method_monitor(char *keyword, char *buf);
extern int kw_set_nd_enable_method_monitor_ex(char *keyword, char *buf, char *err_msg);
extern int kw_set_nd_enable_http_header_capture_monitor(char *keyword, char *buf, char *err_msg); 
extern int kw_set_nd_enable_exceptions_monitor(char *keyword, char *buf, char *err_msg);
extern int kw_set_dynamic_vector_monitor_retry_count(char *keyword, char *buf, char *kw_buf, char *err_msg, int runtime_flag);
extern void kw_set_server_health(char *keyword, char *buf, char *err_msg);

DynamicVectorMonitorInfo *dynamic_vector_monitor_info_ptr;
extern int g_dyn_cm_start_idx;
extern int hpd_port;

//This function will set Agent (LPS or CMON) for monitors 
#define DVM_SET_AGENT_FOR_VECTORS(dynamic_vector_monitor_ptr, svr_ip, svr_port) \
{ \
  NSDL2_MON(NULL, NULL, "Setting monitor agent ip and port, dynamic_vector_monitor_ptr = %p, use_lps = %d", dynamic_vector_monitor_ptr, dynamic_vector_monitor_ptr->use_lps); \
 \
  if(dynamic_vector_monitor_ptr->use_lps == 1) \
  { \
    strcpy(svr_ip, global_settings->lps_server); \
    svr_port = global_settings->lps_port; \
  } \
  else \
  { \
    strcpy(svr_ip, dynamic_vector_monitor_ptr->cs_ip); \
    svr_port = dynamic_vector_monitor_ptr->cs_port; \
  } \
 \
  NSDL3_MON(NULL, NULL, "After setting monitors Agent ip = %s and port = %d", svr_ip, svr_port); \
}

#define FREE_JSON_EX() \
{ \
  if(json)\
  { \
    nslb_json_free(json); \
    ns_cm_monitor_log(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,\
                       "Going to free JSON.");\
    json=NULL;\
  }\
}

#define FREE_JSON() \
{ \
  if(json)\
  { \
    nslb_json_free(json); \
    ns_cm_monitor_log(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,\
                       "Going to free JSON.");\
    json=NULL;\
  }\
\
  break;\
}

#define OPEN_ELEMENT_OF_MONITORS_JSON(json) \
{ \
  int ret = 0; \
  if(json->c_element_type == NSLB_JSON_OBJECT) \
    ret = nslb_json_open_obj(json); \
  else if(json->c_element_type == NSLB_JSON_ARRAY) \
    ret = nslb_json_open_array(json); \
  if(ret != 0) { \
    ns_cm_monitor_log(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,\
                       "Unable to open JSON element of json [%s] or error due to: %s.", \
                        json_monitors_filepath, nslb_json_strerror(json));\
    ret=-1;\
  } \
}

#define CLOSE_JSON_ELEMENT(json) \
{ \
  if(nslb_json_close_obj(json) != 0) \
  { \
    ret = -1; \
    ns_cm_monitor_log(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,\
             "JSON element not closed [%s] or error due to: %s.", json_monitors_filepath, nslb_json_strerror(json));\
    FREE_JSON_EX(); \
  } \
}

#define CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON_EX(json) \
{ \
  if(nslb_json_close_obj(json) != 0){ \
    ret = -1;\
    {\
    ns_cm_monitor_log(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,\
                       "JSON element not closed [%s] or error due to: %s.", json_monitors_filepath, nslb_json_strerror(json));\
    }\
    FREE_JSON_EX();\
  } \
}

#define CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json) \
{ \
  if(nslb_json_close_obj(json) != 0){ \
    ret = -1;\
    {\
    ns_cm_monitor_log(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,\
                       "JSON element not closed [%s] or error due to: %s.", json_monitors_filepath, nslb_json_strerror(json));\
    }\
    FREE_JSON();\
  } \
}


#define CLOSE_ELEMENT_ARR_OF_MONITORS_JSON_EX(json) \
{ \
  if(nslb_json_close_array(json) != 0){ \
    ret = -1;\
    {\
    ns_cm_monitor_log(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,\
                       "Array element not closed [%s] or error due to: %s.", json_monitors_filepath, nslb_json_strerror(json));\
    }\
    FREE_JSON_EX();\
  } \
}

#define CLOSE_ELEMENT_ARR_OF_MONITORS_JSON(json) \
{ \
  if(nslb_json_close_array(json) != 0){ \
    ret = -1;\
    {\
    ns_cm_monitor_log(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,\
                       "Array element not closed [%s] or error due to: %s.", json_monitors_filepath, nslb_json_strerror(json));\
    }\
    FREE_JSON();\
  } \
}

//mandatory_flag is set 1 if ele_name is mandatory, and if ele_name is not mandatory, mandatory_flag is set as 0
#define GOTO_ELEMENT_OF_MONITORS_JSON(json, ele_name, mandatory_flag) \
{\
  if(nslb_json_goto_element(json, ele_name) != 0) { \
    if(mandatory_flag)\
    {\
    ns_cm_monitor_log(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,\
                       "Element name : %s not found [%s] error due to: %s.",ele_name, json_monitors_filepath, nslb_json_strerror(json));\
    }\
    ret = -1;\
  } \
}

#define GOTO_NEXT_ARRAY_ITEM_OF_MONITORS_JSON(json) \
{ \
    if(nslb_json_next_array_item(json) == -1) { \
    ret = -1;\
    {\
    if(((int)json -> error) != 6 && ((int)json -> error) != 0 )\
    ns_cm_monitor_log(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,\
                       "Array item not found [%s] or error due to: %s.", json_monitors_filepath, nslb_json_strerror(json));\
    }\
    break;\
  } \
}

//This macro will not return if error.
#define GOTO_ELEMENT2_OF_MONITORS_JSON(json_ele_name) nslb_json_goto_element(json, ele_name)

//This will free val if not null
#define GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, val, len_ptr, malloc_flag, decode_flag) \
{ \
  val = nslb_json_element_value(json, len_ptr, malloc_flag, decode_flag); \
        if(!val) { \
    ret = -1;\
    FREE_JSON();\
  } \
}

#define GET_ELEMENT_VALUE_OF_MONITORS_JSON_EX(json, val, len_ptr, malloc_flag, decode_flag) \
{ \
  val = nslb_json_element_value(json, len_ptr, malloc_flag, decode_flag); \
        if(!val) { \
    ret = -1;\
    FREE_JSON_EX();\
  } \
}


#define CHECK_OPTIONAL_FEATURES_IN_JSON(json,ret,instance_name,tier_id)\
{\
  GOTO_ELEMENT_OF_MONITORS_JSON(json, "exclude-tier",0);\
  if(ret != -1)\
  {\
    GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);\
    no_of_token_elements = get_tokens(dummy_ptr, token_arr, ",", MAX_NO_OF_APP);\
    for(et=0;et<no_of_token_elements;et++)\
    {\
      if(!strncmp(token_arr[et], "$p:", 3))\
      {\
        if(!nslb_regex_match(topo_info[topo_idx].topo_tier_info[tier_id].tier_name, token_arr[et]+3))\
        {\
          CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);\
          ret = 1;\
          break;\
        }\
      }\
      else if(strchr(token_arr[et],'*') || strchr(token_arr[et],'.') || strchr(token_arr[et],'?'))\
      {\
        if(!nslb_regex_match(topo_info[topo_idx].topo_tier_info[tier_id].tier_name, token_arr[et]))\
        {\
          CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);\
          ret = 1;\
          break;\
        }\
      }\
      else if(!strcmp(topo_info[topo_idx].topo_tier_info[tier_id].tier_name, token_arr[et]) || !strcasecmp(token_arr[et], "AllTier"))\
      {\
        CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);\
        ret=1;\
        break;\
      }\
    }\
\
    if(ret == 1)\
      continue;\
  }\
  else\
    ret=0;\
\
  GOTO_ELEMENT_OF_MONITORS_JSON(json, "exclude-server",0);\
  if(ret != -1)\
  {\
    GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);\
    no_of_token_elements = get_tokens(dummy_ptr, token_arr, ",", MAX_NO_OF_APP);\
    for(et=0;et<no_of_token_elements;et++)\
    {\
      if(!nslb_regex_match(topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_idx].server_disp_name, token_arr[et]) || !nslb_regex_match(topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_idx].server_ptr->server_name, token_arr[et]))\
      {\
        CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);\
        ret=1;\
        break;\
      }\
    }\
\
    if(ret == 1)\
      continue;\
  }\
  else\
    ret=0;\
\
  GOTO_ELEMENT_OF_MONITORS_JSON(json, "specific-server",0);\
  if(ret != -1)\
  {\
    GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);\
    no_of_token_elements = get_tokens(dummy_ptr, token_arr, ",", MAX_NO_OF_APP);\
\
    if(no_of_token_elements && !(strcasecmp(token_arr[0],"Any")))\
    {\
      if(json_info_ptr)\
        json_info_ptr->any_server=true;\
\
      ret = 0;\
    }\
    else\
    {\
      if(json_info_ptr)\
        json_info_ptr->any_server=false;\
      for(et=0;et<no_of_token_elements;et++)\
      {\
        if(nslb_regex_match(topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_idx].server_disp_name, token_arr[et]))\
          ret=1;\
        else\
        {\
          ret = 0;\
          break;\
        }\
      }\
    }\
\
    if(ret == 1)\
    {\
      CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);\
      continue;\
    }\
  }\
  else\
    ret=0;\
\
  sprintf(instance_name, " ");\
\
  GOTO_ELEMENT_OF_MONITORS_JSON(json, "instance",0);\
  if(ret != -1)\
  {\
    GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);\
    sprintf(instance_name, "%c%s", global_settings->hierarchical_view_vector_separator, dummy_ptr);\
  }\
  else\
  {\
    ret=0;\
  }\
  MALLOC_AND_COPY( topo_info[topo_idx].topo_tier_info[tier_id].tier_name,json_info_ptr->tier_name ,(strlen(topo_info[topo_idx].topo_tier_info[tier_id].tier_name) + 1), "Copy tier name", -1);\
  json_info_ptr->tier_idx = tier_id; \
  MALLOC_AND_COPY( topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_idx].server_disp_name,json_info_ptr->server_name ,(strlen(topo_info[topo_idx].topo_tier_info[tier_id].topo_server[server_idx].server_disp_name) + 1), "Copy server name", -1);\
  json_info_ptr->server_index=server_idx;\
}



//DVM connection states
#define DVM_INIT                    1
#define DVM_CONNECTED               2
#define DVM_CONNECTING              3
#define DVM_SENDING                 4     
#define DVM_WAITING_FOR_VECTORS     5
#define DVM_RECEIVED_VECTOR_LIST    6 //to check if vector list received or not, to decide whether we need to add this as CM or not
#define DVM_PROCESSED               7 //Done: state after converting received vectors->CM
#define DVM_DELETED                 8
#define DVM_FAILED                  9

#define DYNAMIC_VECTOR_MON_MAX_LEN      4096
#define MAX_DYNAMIC_VECTORS             100*1024  //max number of vectors
#define ND_MON_VECTOR_BUFF_LEN		10*1024

extern int read_output_from_create_server_to_get_vector_list(DynamicVectorMonitorInfo *dynamic_vector_monitor_ptr, int runtime_flag);
extern char* failed_dynamic_vector_monitor;
extern int total_dynamic_vector_mon_entries;
extern void free_dynamic_vector_monitor();
extern char *dvm_to_str(DynamicVectorMonitorInfo *dynamic_vector_monitor_ptr);
extern int dynamic_vector_monitor_retry_count;
extern int dynamic_vector_monitor_timeout;
extern char *error_msgs[];
extern void set_coherence_cache_format_type();
extern char nd_bt_and_backend_vector_file[1024];
//short cs_port; TODO CHECK
extern int g_dyn_cm_start_idx_runtime;

extern char is_dyn_mon_added_on_runtime;
extern int total_nid_table_row_entries;
extern int max_nid_table_row_entries;
extern int total_no_of_coherence_cluster_mon;

//functions used for defining keywords, defined in ns_parse_scen_conf.c
extern int kw_set_nd_enable_entry_point_monitor(char *keyword, char *buf, char *err_msg);
extern int kw_set_nd_enable_db_call_monitor(char *keyword, char *buf, char *err_msg);
extern int kw_set_nd_enable_bt_ip_monitor(char *keyword, char *buf, char *err_msg);
extern int kw_set_nd_enable_bt_monitor(char *keyword, char *buf, char *err_msg);
extern int kw_set_nd_enable_backend_call_monitor(char *keyword, char *buf, char *err_msg);
extern int kw_set_nd_enable_fp_stats_monitor_ex(char *keyword, char *buf, char *err_msg);
extern int kw_set_nd_enable_business_trans_stats_monitor(char *keyword, char *buf, char *err_msg);
extern int kw_set_nd_enable_cpu_by_thread_monitor(char *keyword, char *buf, char *err_msg);
extern int kw_set_nv_enable_monitor(char *keyword, char *buf, char *nv_mon_gdf_name, char *err_msg);
extern int kw_set_enable_auto_server_sig(char *keyword, char *buf);
extern int kw_set_log_vuser_data_interval(char *keyword, char *buf, char *err_msg);
extern int kw_set_g_inline_min_con_reuse_delay(char *buf, char *change_msg, int *min_con_delay, int *max_con_delay);
extern int kw_set_g_enable_dt(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag);
extern int kw_set_ip_based_data(char *buf, char *err_msg);
extern int kw_set_stop_test_if_dnsmasq_not_run(char *buf);
extern int kw_check_nc_build_compatibility(char *buf);
extern int get_norm_id_for_session(char *sess_name); 
extern void create_and_fill_multi_disk_path(char *file_type, char *multidisk_path);
extern int kw_set_nd_enable_node_gc_monitor(char *keyword, char *buf);
extern int kw_set_nd_enable_event_loop_monitor(char *keyword, char *buf);
extern int kw_set_enable_alert_rule_monitor(char *keyword, char *buf, char *err_msg);
extern void kw_set_dynamic_cm_rt_table_size(char *buf);
extern int kw_set_server_signature(char *keyword, char *monitor_buf, int runtime_flag, char *err_msg, JSON_info *json_info_ptr);
extern int add_json_monitors(char *vector_name, int server_idx, int tier_idx, char *app_name, int runtime_flag, int ndc_any_server_check, int lps_based_monitors);
extern int add_json_monitors_ex(int runtime_flag, nslb_json_t *json, int delete_flag);
extern int read_json_and_apply_monitor(int runtime_flag, char process_global_vars);
extern int g_dyn_cm_start_idx;
extern void kw_set_dvm_malloc_delta_size(char *buff);
extern int create_table_entry_dvm(int *row_num, int *total, int *max, char **ptr, int size, char *name);
extern int g_dvm_malloc_delta_entries;
extern void kw_set_dynamic_cm_rt_table_settings(char *buf);
extern void kw_set_nd_enable_monitor(char *keyword, char *buf, char *err_msg);
extern void kw_set_enable_vector_hash(char *keyword, char *buf, char *err_msg);
extern void make_vector_name_from_gdf(char *gdf_name,char *vector_name);
int init_and_check_if_mon_applied(char *tmp_buf);
extern int check_if_tier_excluded(int no_of_exclude_tier_elements, char *exclude_tier_arr[], char *tier_name);
extern kub_ip_mon_vector_fields *copy_vector_data_in_string(char *field[10]);
extern int search_replace_chars(char **dummy_ptr , char *delim , char *out_buf, kub_ip_mon_vector_fields *vector_fields_ptr);
extern void add_delete_kubernetes_monitor_on_process_diff(char *appname, char *tier_name, char *mon_name, int delete_flag);
extern int create_mon_buf(char *exclude_server_arr[], int no_of_exclude_server_elements, char *specific_server_arr[],
                   int no_of_specific_server_elements, char *mon_name, char *tmp_prgrm_args, char *instance_name, JSON_info *json_info_ptr,
                   int process_diff_json, char mon_type, int tier_id, char *gdf_name, JSON_MP *json_mp_ptr, int server_idx,
                   char *kub_vector_name, char *app_name);

#endif
