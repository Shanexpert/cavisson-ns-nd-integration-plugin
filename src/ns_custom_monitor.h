#ifndef _ns_custom_monitor_h
#define _ns_custom_monitor_h

#include <stdbool.h>
#include "ns_exit.h"
#include "ns_monitor_metric_priority.h"

/*Archana :: Following values also defined in monitors/linux/server.h, later code from "server.h" has to merge with following*/
// Value of option
#define RUN_EVERY_TIME_OPTION 1
#define RUN_ONLY_ONCE_OPTION  2

// Value of print mode 
#define PRINT_PERIODIC_MODE   0
#define PRINT_CUMULATIVE_MODE 1

// Value of access
#define RUN_LOCAL_ACCESS      1
#define RUN_REMOTE_ACCESS     2


#define PROCESS_REQUIRED  1
#define COMPLETED_PROCESSED 2


#define MON_STOP_NORMAL 0
#define MON_STOP_ON_RECEIVE_ERROR 1
#define MON_STOP_ON_REQUEST 2
#define MON_DELETE_ON_REQUEST 3
#define MON_DELETE_INSTANTLY 4
#define MON_DELETE_AFTER_SERVER_INACTIVE 5
#define SM_DELETE_ON_RECEIVE_ERROR 5             // this is for synthetic monitoring when receive status ERROR for monitor

//Custom monitor connection states
#define CM_INIT                    0
#define CM_CONNECTED               1
#define CM_CONNECTING              2
#define CM_SENDING                 3     
#define CM_RUNNING                 4
#define CM_STOPPED                 5
#define CM_DELETED                 6
#define CM_REUSED                  7
#define CM_VECTOR_RESET            8
#define CM_VECTOR_RECEIVED         9

// macro to set flag for instantly reused vector
#define MON_REUSED_NORMALLY 0
#define MON_REUSED_INSTANTLY 1

#define CUSTOM_TYPE		0X01
#define STANDARD_TYPE		0X02

#define CM_DR_ARRAY_SIZE 5*1024 //disaster recovery array
#define CM_SEND_MSG_BUF_SIZE 3 * 1024

#define CM_NOT_REMOVE_FROM_EPOLL    0
#define CM_REMOVE_FROM_EPOLL        1

#define ADD_VECTOR_MSG "ADD_VECTOR|"
#define ADD_VECTOR_MSG_LEN 11

#define DELETE_VECTOR_MSG "DELETE_VECTOR|"
#define DELETE_VECTOR_MSG_LEN 14

#define DVM_RESET_IDX_MSG "dvm_reset_index"
#define DVM_RESET_IDX_MSG_LEN 15
#define VECTOR_NAME_MAX_LEN 512

#define OLD_FORMAT_VECTOR 0
#define NEW_FORMAT_VECTOR 1

//Macro used for flag on monitor level 
#define OUTBOUND_ENABLED                0x0001
#define ND_MONITOR                      0x0002
#define RUNTIME_ADDED_MONITOR		0x0004
#define USE_LPS                         0x0008
#define ORACLE_STATS                    0x0010
//#define GROUP_VECTOR                    0x0200     //No more used
#define ALL_VECTOR_DELETED              0x0020	     
#define NEW_FORMAT                      0x0040
#define DYNAMIC_MONITOR                 0x0080
#define REMOVE_MONITOR			0x0100		//Bit used toremove monitor from structure at the time of monitor reused
#define OVERALL_CREATED                 0x0200
#define DATA_PENDING                    0x0400
#define NV_MONITOR                      0x0800
#define COHERENCE_CLUSTER               0x1000
#define COHERENCE_OTHERS                0x2000
#define NA_KUBER                        0x4000
#define ALERT_LOG_MONITOR               0x8000
#define IP_DATA_MONITOR                 0x10000
#define NA_WMI_PERIPHERAL_MONITOR       0x20000 
#define NA_GDF                          0x40000
#define NA_MSQL                         0x80000
#define ND_BT_GDF                       0x100000 
#define ND_BACKEND_CALL_STATS_GDF       0x200000 

//Macro used for flags on vector level
#define DATA_FILLED 			0x0100
#define MON_BREADCRUMB_SET 		0x0200
#define OVERALL_VECTOR 			0x0400
#define RUNTIME_DELETED_VECTOR 		0x0800
#define WRITTEN				0x0010
#define RUNTIME_RECENT_DELETED_VECTOR	0x0020
#define WAITING_DELETED_VECTOR		0x0040

//Node Pod States
#define KUBER_NODE_DISCOVERY   0x01
#define POD_DISCOVERY    0x02
//#define NODE_SHUTDOWN    0x04
//#define POD_SHUTDOWN     0x08
#define NODE_DELETE      0x10
#define POD_DELETE       0x20
#define NODE_EXISTING	 0x40
#define POD_EXISTING	 0x80
#define RESET	 	 0x100
#define RESET_NODE_POD	  0x03
#define NEW_NODE 0x01 
#define NEW_POD 0x02
#define POD_SHUTDOWN 0x20
#define NODE_SHUTDOWN 0x10

#define MAX_DATA_COL	128

#define MBEAN_MONITOR 	0X01 
#define BCI_MBEAN_MONITOR 	0X01 
#define CMON_MBEAN_MONITOR 	0X02 

#define CONNECT_TO_NDC	0X01  //THis is set in case MBEAN monitor need to be applied on NDC. Only Mbean type monitor are applied from json for NDC.
#define CONNECT_TO_CMON	0X02  //This will be set for all monitors to be applied on CMON. For MBean based monitor will be identified if config_file is provided
#define CONNECT_TO_BOTH_AGENT 0x03
 
#define DELTA_NODE_POD_ENTRIES 5

#define DUPLICATE_MON 999

/*
#define DISCOVERY 2
#define SHUTDOWN  -2
#define DELETE    0
#define RESET     -1
#define EXISTING  1
*/

//This macro will replace vector element received on parent connection with the entry vectorReplaceTo in JSON, if vector matches with vectorReplaceFrom entry in JSON. This was done beacuse there was duplicate issue with the weblogic monitors. There were multiple weblogic admins on a single server. If all the admin of that server giving same instance then this logic can change instance name form one admin to the desired name provided. There is a limitation to this. It will work if all the admin have only one instance, if there are multiple instance on admins and all are repeating themselves, we dont have multiple 'vectorReplaceTo' and 'vectorReplaceFrom' field in JSON. So we can only replace one instance.

#define IS_TIMES_GRAPH(graph_type) ((graph_type == DATA_TYPE_TIMES) || (graph_type == DATA_TYPE_TIMES_4B_10) || (graph_type == DATA_TYPE_TIMES_4B_1000))

#define IS_TIMES_STD_GRAPH(graph_type) ((graph_type == DATA_TYPE_TIMES_STD) || (graph_type == DATA_TYPE_TIMES_STD_4B_1000))

#define REPLACE_INSTANCE_CAME_WITH_INSTANCE_PROVIDED_IN_JSON(vector) \
{ \
  if(parent_ptr && (parent_ptr->vectorReplaceFrom)) \
  { \
    if(!strncmp(vector, parent_ptr->vectorReplaceFrom, strlen(parent_ptr->vectorReplaceFrom)) && (parent_ptr->vectorReplaceTo)) \
      sprintf(instance, "%s%s", parent_ptr->vectorReplaceTo, (vector + strlen(parent_ptr->vectorReplaceFrom))); \
    else \
      strcpy(instance, vector); \
  } \
  else \
    strcpy(instance, vector); \
}

//This function will set Agent (LPS or CMON) for monitors 
#define CM_SET_AGENT(cus_mon_ptr, svr_ip, svr_port) \
{ \
  char *ptr = NULL; \
  NSDL2_MON(NULL, NULL, "Setting monitor agent ip and port, cus_mon_ptr = %p, use_lps = %d", cus_mon_ptr, (cus_mon_ptr->flags & USE_LPS)?1:0); \
 \
  if(cus_mon_ptr->flags & USE_LPS) \
  { \
    strcpy(svr_ip, global_settings->lps_server); \
    svr_port = global_settings->lps_port; \
  } \
  else \
  { \
    if((ptr = strchr(cus_mon_ptr->cs_ip, ':')) != NULL) \
    { \
      *ptr = '\0'; \
      svr_port = atoi(ptr + 1); \
    } \
    else \
      svr_port = cus_mon_ptr->cs_port; \
 \
    strcpy(svr_ip, cus_mon_ptr->cs_ip); \
  } \
 \
  NSDL3_MON(NULL, NULL, "After setting monitors Agent ip = %s and port = %d", svr_ip, svr_port); \
}

#define CM_RUNTIME_RETURN_OR_EXIT(runtime_flag, err_msg, flag,tier_idx,server_index) \
{ \
  if(runtime_flag) \
  { \
    if(flag) \
    { \
      topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_index].server_ptr->topo_servers_list->cmon_monitor_count--; \
    } \
    return -1; \
  } \
  else \
  { \
    NS_EXIT(-1,"%s", err_msg); \
  } \
}

#define CM_RUNTIME_DUPLICATE_ERROR_RETURN_OR_EXIT(runtime_flag, err_msg, flag,tier_idx,server_index) \
{ \
  if(runtime_flag) \
  { \
    if((flag == 1) || (dyn_cm == 1)) \
    { \
      topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_index].server_ptr->topo_servers_list->cmon_monitor_count--; \
      return -2; \
    } \
    else \
    { \
       topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_index].server_ptr->topo_servers_list->cmon_monitor_count--; \
      return 0; \
    } \
  } \
  else \
  { \
    NS_EXIT(-1,"%s", err_msg); \
  } \
}

#define HANDLE_VECTOR_FORMAT(cm_ptr, j) \
{ \
  if(cm_ptr.conn_state == CM_DELETED || cm_ptr.conn_state == CM_STOPPED || cm_ptr.conn_state == CM_RUNNING || cm_ptr.conn_state == CM_INIT) \
  { \
    if((parent_ptr) && (cm_table_ptr[cus_mon_row].vectorIdx >= 0)) \
    { \
      if(parent_ptr->dvm_cm_mapping_tbl_row_idx < 0) \
      { \
        parent_ptr->dvm_cm_mapping_tbl_row_idx = total_mapping_tbl_row_entries; \
        total_mapping_tbl_row_entries++; \
        total_dummy_dvm_mapping_tbl_row_entries --; \
      } \
      if(cm_table_ptr[cus_mon_row].vectorIdx >= *max_mapping_tbl_vectors_entries) \
      { \
        *max_mapping_tbl_vectors_entries = create_mapping_table_col_entries(parent_ptr->dvm_cm_mapping_tbl_row_idx, cm_table_ptr[cus_mon_row].vectorIdx, *max_mapping_tbl_vectors_entries); \
      } \
      dvm_idx_mapping_ptr[parent_ptr->dvm_cm_mapping_tbl_row_idx][cm_table_ptr[cus_mon_row].vectorIdx].relative_dyn_idx = j; \
      if(cm_table_ptr[cus_mon_row].data != NULL) \
      { \
        dvm_idx_mapping_ptr[parent_ptr->dvm_cm_mapping_tbl_row_idx][cm_table_ptr[cus_mon_row].vectorIdx].is_data_filled = 1; \
      } \
    } \
  } \
}

#define SERIAL_NO_INCREMENT(serial_no)\
{\
  serial_no++;\
  if(serial_no == 10000000-1)\
  {\
    serial_no = 0;\
  }\
}\

#define FILL_DETAILS_FOR_DELETE_MON_REQUEST()	\
{	\
int k; \
if(no_of_exclude_tier > 0)	\
  {	\
    strcat(msg_buffer, "EXCLUDE_TIER=");	\
    for(k = 0; k < no_of_exclude_tier; k++)	\
    {	\
      if(search_tier_in_mbean_info_tier_list(exclude_tier[k], json_info) < 0)	\
        continue;	\
	\
      if(k == (no_of_exclude_tier - 1))	\
        sprintf(msg_buffer, "%s%s:", msg_buffer, exclude_tier[k]);	\
      else	\
        sprintf(msg_buffer, "%s%s,", msg_buffer, exclude_tier[k]);	\
    }	\
  }	\
	\
  if(no_of_specific_server > 0)	\
  {	\
    strcat(msg_buffer, "SEPCIFIC_SERVER=");	\
    for(k = 0; k < no_of_specific_server; k++)	\
    {	\
      if(k == (no_of_specific_server - 1))	\
        sprintf(msg_buffer, "%s%s:", msg_buffer, specific_server[k]);	\
      else	\
        sprintf(msg_buffer, "%s%s,", msg_buffer, specific_server[k]);	\
    }	\
  }	\
	\
}

#define NORMAL_CM_TABLE 0
#define RUNTIME_CM_TABLE 1
#define RUNTIME_DVM_TABLE 2

#define OLD_VECTOR_FORMAT   		0x01
#define NEW_VECTOR_FORMAT		0x02
#define FORMAT_NOT_DEFINED		0x04
#define NO_LOGGING			0x08
#define LOG_ONCE			0x10
#define DEFAULT_LOG			0x20

#define ND_RESET_FLAG               0x01
#define GOT_ND_DATA                 0x02

#define HB_DATA_CON_INIT 1
#define HB_DATA_CON_SENDING 2

#define NV_INITIALIZE 0x01 //We use this bit to intialize the nv_monitors. It is used only for NV.
#define NV_START_HEADER 0x02 //We use this to set the 2nd bit if we get CAV_DATA_START header.It is used only for NV.

#define DELTA_DVM_CM_IDX_MAPPING_TABLE_ENTRIES 5 //TODO 1 value is only for testing change it to 5 // this will be used during allocation of both row and columns
#define DELTA_MON_ID_ENTRIES 	5 //This value is to assign default allocation to mon_id_map_table.
#define CHK_MON_START_MON_ID 50000   //Starting monitor id for check monitor.
#define ALL_MON_START_MON_ID 0       //Starting monitor id for all monitor other than check monitor.

#define GOT_DATA 0x01
#define OVERALL_DELETE 0x02

#define MAX_FIEDLS_FOR_MON_REGISTRATION	5

#define CONTROLLER_VECTOR_NAME 	"Controller"
#define NS_VECTOR_NAME 		"NSAppliance"
#define NO_VECTOR_NAME 		"NOAppliance"
#define ND_VECTOR_NAME 		"NDAppliance"
#define NV_VECTOR_NAME 		"NVAppliance"

#include "ns_compress_idx.h"
#include "ns_data_types.h"
#include "nslb_get_norm_obj_id.h"
#include "ns_dynamic_vector_monitor.h"

extern char g_enable_tcp_keepalive_opt;
extern int g_tcp_conn_idle;
extern int g_tcp_conn_keepintvl;
extern int g_tcp_conn_keepcnt;


extern bool g_enable_delete_vec_freq;
extern int g_delete_vec_freq_cntr;
extern char g_delete_vec_freq;


//MSSQL meta-data table related elements
extern char is_query_execution_norm_init_done;
extern char is_temp_db_norm_init_done;
extern NormObjKey *query_execution_sql_id_key;
extern NormObjKey *temp_db_sql_id_key;
extern NormObjKey *plan_handle_sql_id_key;

extern int enable_store_config;
#define STORE_PREFIX (enable_store_config)?g_hierarchy_prefix:""

extern int total_cm_entries;
extern int max_cm_entries;

extern int total_cm_runtime_entries;
extern int max_cm_runtime_entries;

extern int total_dyn_cm_runtime_entries;
extern int max_dyn_cm_runtime_entries;

extern int total_mapping_tbl_row_entries; // total number of monitors in 2-d matrix
extern int max_mapping_tbl_row_entries; // max number of monitors in 2-d matrix
extern int total_dummy_dvm_mapping_tbl_row_entries;

extern int dump_monitor_tables;
extern int auto_json_mon_first_time_flag;

extern char g_validate_nd_vector;
extern char g_log_mode;
extern char gdf_log_pattern[1024];
extern FILE *g_nd_monLog_fd;

extern int g_vector_runtime_changes;
extern int g_monitor_runtime_changes;
extern int g_monitor_runtime_changes_NA_gdf;
extern int new_monitor_first_idx;

extern Long_long_data nan_buff[128];

extern int g_remove_trailing_char;
extern char g_trailing_char[256];

extern char *mssql_metadata_csv;


//Global buffer for cm_update_monitor request if we will not send the msg to ndc then we will store this in buffer and retry again
extern int total_cm_update_monitor_entries;
extern int max_cm_update_monitor_entries;
extern char **cm_update_monitor_buf;


extern int total_mbean_mon;
extern int max_mbean_mon;
extern int mbean_monitor_offset;
extern char mbean_mon_rtc_applied;

extern int total_dest_tier; //this global variable is used to store the destination tier names count

NormObjKey *dest_tier_name_id_key;   //used for normalized table of destination tier name 

#define DELETED_TIER		 0X01 //Set if tier deleted due to some runtime change. 
#define ADDED_TIER 0X02       //Set if new tier added to existing monitor.

#define UPDATED_NDC_MON		0X01  // Set in mbean_mon_ptr if any request to be send to NDC(currently only 
                                //request for added tier is sent.)

#define DELTA_MAP_TABLE_ENTRIES 64

#define ADD_NODE 0x01
#define ADD_POD 0x02
#define REUSE_POD 0x04
#define DELETE_POD 0x20


/*typedef struct id_map_table_t
{
  short *idxMap;
  int mapSize;
  int maxNormValue;
} id_map_table_t; moved toseparate header file*/

typedef struct HiddenFileInfo 
{
  char *gdf_name;
  char *hidden_file_name;
  char is_file_created;
  int gdf_flag;
}HiddenFileInfo;



typedef struct instanceVectorIdx
{
  char reset_and_validate_flag;
  double *data;
  void *metrics_idx;
  char metrics_idx_found;
  int norm_vector_id;
  unsigned short tier_id;  //tier ID returned from vector/data. This can be different from what NS is storing and should be taken care.
} instanceVectorIdx;

//Structure used for GenIpData Monitor
typedef struct GenVectorIdx
{
  int max_vectors;
  int gen_fd;
  int num_vector;     //No of vector of each generator
  double *data;
  void *metrics_idx;
  char metrics_idx_found;
} GenVectorIdx;

extern GenVectorIdx  **genVectorPtr;    //For NetCloud IP monitor

//create 1 struct for monitor list of source_tier_info
typedef struct Mon_List_Info
{
  NSLB_MP_COMMON;
  char *prgrm_args;  // store original program args with %dest_any_server%
  char *mon_name;   //store mon name with T>S>I , this is for monitor uniqueness
  char *gdf_name;  // for deletion purpose not used
  char *monitor_name;  //store monitor name
  char *source_server_ip;
  JSON_info *json_info_ptr;
}Mon_List_Info;

//this structure is used for dest-any-server keyword in json.
typedef struct Source_Tier_Info
{
  char *source_tier_name;
  int source_tier_id;  //this is for checking of source tier is present or not on deletion
  nslb_mp_handler mon_list_info_pool;
}Source_Tier_Info;


typedef struct Dest_Tier_Info
{
  bool dest_server_ip_active; 
  char *dest_tier_name;
  char *dest_server_ip;
  int max_source_tier_entries;
  int total_source_tier;
  Source_Tier_Info * source_tier_info;
}Dest_Tier_Info;

Dest_Tier_Info *dest_tier_info;

typedef struct tierNormVectorIdx
{
 double *aggrgate_data;  //do we need to have a double ptr storing all the data received from different BCI
 char got_data_and_delete_flag;
} tierNormVectorIdx;

typedef struct NodePodInfo
{
  char *NodeIp;
  char *NodeName;
  char *PodName;
  char *appname_pattern;
  char *vector_name;
  char container_count;
  long long node_start_time;
  long long pod_start_time;
  struct NodePodInfo *next;
} NodePodInfo;

typedef struct NodeList
{
  char NodeIp[20];
  char CmonPodIp[20];
  char flag;  //0x01 New Node 0x2 New Pod
  int server_info_index;
  char *CmonPodName;
  NodePodInfo *DeletedList;
  NodePodInfo *AddedList;
  NodePodInfo *head;
  int tier_idx;
}NodeList;


extern NodePodInfo *node_info_ptr;
extern NodePodInfo *pod_info_ptr;
/*
typedef struct MSSQL_table
{
  char *gdf;
  char *csv_file;
  char *meta_data_csv;
  int norm_id_col;
  char *norm_table_col_list;
  char *main_table_col_list;
  char *date_time_col_list;
  int norm_tbl_idx;
}MSSQL_table;
*/

typedef struct
{
  char *csv_file;
  char *metadata_csv_file; 
  char *planhandle_csv_file; 
  FILE *csv_file_fp;
  FILE *metadata_csv_file_fp;
  FILE *planhandle_csv_file_fp;
  int date_time_idx[MAX_DATA_COL];   //This is an integer array whose purpose is to store column index when data is coming from cmon in csv format for mssql monitor. we store at which index "date-time" format is coming. And its index will be stored in integer array. Index stored in array should be in increasing order. 
  //eg: data from cmon is like:   1,2,3,4,5,2018-02-18 10:01:39,20,40,20,102,2018-08-07 08:02:49,30,102
  //data_time_idx array will have entry like: {6,11}, which shows at 6th and 11th column date-time is present.
  int date_time_col_count;
  int is_norm_init_done;
  NormObjKey *sql_id_key;
  int date_format;     //eg. 20180525 -> (YYYYMMDD)
  int norm_id_col;
  int main_table_col_arr[MAX_DATA_COL];
  char no_modification_flag;
  int norm_table_array[MAX_DATA_COL];
  int norm_tbl_idx;
} CSVStats;


typedef struct CM_info CM_info;

//this structure is used to store the appname and server_idx which is used when we get process_diff or some RTC changes
typedef struct Kube_Info{
  char *app_name;  //here basically we save pod name
  int server_idx;
}Kube_Info;

typedef struct CM_vector_info {
  CM_info  *cm_info_mon_conn_ptr ;          //Pointer to parent to use the the common members
  char *vector_name;  // vector name 
  int vector_state;
  int inst_actual_id;
  double *data;    // holds the data for printing.
  char metrics_idx_found;
  void *metrics_idx; // for fetch the metrics id .
  char *mon_breadcrumb;         //new vector name
  short flags; // flags will consists of old members which are runtime_and_copy_flag, is_data_filled, is_mon_breadcrumb_set and is_overall.
  int instanceIdx;
  int tierIdx;
  int vectorIdx; //NOTE : FOR OVERALL THIS WILL HAVE NORMALIZED ID, FOR VECTORS THIS WILL HAVE ACTUAL VECTOR IDX. AND THIS VALUE CAN BE GREATER THAN MAX VECTOR IDX RECEIVED TILL. ALSO FOR "OVERALL" THIS ID SHOULD NEVER USE.
  int mappedVectorIdx; //NOTE: Saved mapped Vector id
  union
  {
    short generator_id;                 //Store generator id for netcloud monitors
    short vector_length;
  };
  long rtg_index[MAX_METRIC_PRIORITY_LEVELS + 1];
  int group_vector_idx;
  Kube_Info *kube_info;
} CM_vector_info;

typedef struct CM_info {
  char con_type; // Must be first field
  // file descriptor for tcp socket made to make connection with CS.
  char hb_conn_state;          // connection state: stop,abort,running
  char nv_data_header; // this flag is used only for NV monitors to check CAV_DATA_START and CAV_DATA_END header in the recieved message.Its 1st bit is set for NV monitors and for rest monitors its 0. Its 2nd bit is set when we get CAV_DATA_START header and we reset its 1st bit if we get CAV_DATA_END.
  char tier_server_mapping_type;    //
  int conn_state;
  char *g_mon_id;  //this is the id of monitor which is passed in json_info_struct and it is unique for all monitors
  int mon_info_index;   // this is mon_config_list_ptr index
  char *monitor_name;  //monitor name is vector name passed in custom monitor buffer it will be same as vector name in case of custom monitor
  char *mbean_monitor_name;  //mbean monitor name standard monitor name in case of mbean monitor for cmon
  int fd; // fd must be the 1st field as we are using this struct as epoll_event.data.ptr
  int reused_vec_parent_fd;
  char *pgm_path;  // Custom Monitor program name with or without path
  char *gdf_name;  //this gdf name is with absolute path of gdf
  char *gdf_name_only;  //this will store only gdf_name without absolute path
  int option;        // 1 as every (default)  and 2 as once
  int print_mode;
  char *cs_ip;                 // Create Server IP
  char *server_display_name;
  char *cavmon_ip;                 // Cavmon IP
  int cs_port;                 // Create Server Port
  int cavmon_port;                 // Cavmon Port
  int access ;       // 1 as local (default) and 2 as remote
  int gp_info_index;//this will use as index in group_data_ptr.
  int group_num_vectors;
  int num_group;               // to store the num of group in a particular custom gdf.

  //For deleted vectors information
  int total_reused_vectors;
  int max_reused_vectors;
  int *reused_vector;

  int max_deleted_vectors;
  int total_deleted_vectors;
  int *deleted_vector;

  char *rem_ip;                // remote IP
  char *rem_username;          // remote Username
  char *rem_password;          // remote Password
  char *pgm_args;       //programm argument given by user eg; filename, etc
  
  unsigned int  no_of_element; // no of element in a custom file
  unsigned short group_element_size[MAX_METRIC_PRIORITY_LEVELS + 1];// Sum of size of elements of this vector, Max Size = 65535 => max number of graphs with types Times = 2047
  char init_vector_file_flag;           //flag for init_vector_file
  char skip_breadcrumb_creation;
  int cm_idx;

  char *data_buf;              // to store the data comming from CS.
  int dindex;                  // index to trace the data came from Create server.
  int num_dynamic_filled;      // Number of vector data lines filled

  int cm_retry_attempts;
  int bytes_remaining;
  int send_offset;
  int monitor_list_idx;
  char *partial_buf;        //Here we store data which is partial write
  int partial_buf_len;

  /* For monitor hierarchical view */
  int hb_partial_buf_len;
  int hb_send_offset;
  int hb_bytes_remaining;
  char *hb_partial_buf;
  char *dest_file_name;     //Here we store destination file name of cm_get_log_file
  char *origin_cmon;     //Added for Heroku, this is the origin cmon server name or ip address as given by origin cmon to proxy cmon

  instanceVectorIdx *** instanceVectorIdxTable;
  tierNormVectorIdx *** tierNormVectorIdxTable;
  struct id_map_table_t *instanceIdxMap;
  struct id_map_table_t *tierIdxmap;
  struct id_map_table_t *ndVectorIdxmap;
  int cur_tierIdx_TierNormVectorIdxTable;
  int cur_normVecIdx_TierNormVectorIdxTable;
  int cur_instIdx_InstanceVectorIdxTable;
  int cur_vecIdx_InstanceVectorIdxTable;

  NormObjKey key;
  NormObjKey nd_norm_table;
  union
  {
    int is_norm_init_done;
    int is_monitor_registered;
  };
  int save_times_graph_idx[128];

  struct NVMapDataTbl *nv_map_data_tbl; // NV data 
  struct nv_id_map_table_t *vectorIdxmap;  //mapped vector id in NV
  double *dummy_data;

  char metric_priority;
  bool any_server;      //for specific_server in json having "Any" in it

  int prev_group_num_vectors; //to know how many vectors added on runtime on each progress interval , 'no. of new vectors = group_num_vectors - prev_group_num_vectors'

  bool dest_any_server;   //for dest-any-server in json
  int parent_idx;
  int dvm_cm_mapping_tbl_row_idx; // save respective monitor DVM_CM_Idx_Mapping_Tbl index , used when data received on parent fd to access dvm_cm_idx_mapping table
  unsigned int max_mapping_tbl_vector_entries; // total no. of vectors present in mapping table, including extra entries
  int server_index;
  int breadcrumb_level;
  int mon_id;
  int total_appname_pattern;
  char **appname_pattern;
  char *namespace;
  char *cmon_pod_pattern;
  char *tier_name;
  char *server_name;
  char *pod_name;
  char *component_name;             //Component name is taken for Monitor Registration
  char *instance_name;
  char *vectorReplaceFrom;          //
  char *vectorReplaceTo;	    //has been added from json
  CSVStats *csv_stats_ptr;	    //This structure is used only for mssql monitors which does not have any metadata table. for monitor requiring metadata table will have a seperate global table.
  CM_vector_info *vector_list;
  int nid_table_row_idx;
  int total_vectors;
  int max_vectors;
  int new_vector_first_index;
  int no_log_OR_vector_format;
  int flags;
  char is_data_overflowed;      //Use to dump overflow in trace log only once
  char monitor_type;			//DVM,CM,MBean
  int mbean_mon_idx;
  char *config_file; //Added for Generic Enhancment.
  int frequency;
  char is_process;
  int sm_mode;       // this will be 1 or 2 if 1 means dont need to retry and if 2 we have to retry if fails.
  int gdf_flag;
  int hash_size;
  int genericDb_gdf_flag;
  int tier_index;
} CM_info;

typedef struct Monitor_list{
  char *gdf_name;
  short is_dynamic;
  int no_of_monitors; //will be set for first in the group
  CM_info *cm_info_mon_conn_ptr;
} Monitor_list;


typedef struct Mon_from_json
{
  int mon_type;
  char* mon_buff;
  char* pod_name;
  JSON_info *json_info_ptr;
} Mon_from_json;



typedef struct DvmVectorIdxMappingTbl
{
  int relative_dyn_idx;  // save relative to parent CMInfo table index
  char is_data_filled; // initially -1, once data filled at 'relative_dyn_idx' mark this to 1. 
}DvmVectorIdxMappingTbl;

//Mapping table for all monitor id and cm_index
typedef struct MonIdMapTable
{
  int mon_index;
  char retry_count; 
  short state;       //This is taken as short because we memset this structure with -1, and if it was char then all of its bit was set due to -1. So we needed to reset this element to 0.
}MonIdMapTable ;
extern MonIdMapTable *mon_id_map_table;

typedef struct Mon_Reg_Req
{
  int msg_len;
  int opcode;
  char message[1024];
}Mon_Reg_Req;


struct id_pool
{
  int *id_pool_list;
  int id_pool_max_entries;
  int next_available_idx; /*id will be put at next_available_idx and next_available_idx will be increase by 1
                           *next_available_idx will be decrease by 1 and get the id at next_available_idx */
};
extern struct id_pool mon_id_pool;

//For percentile monitors
extern CM_info *cm_bt_percentile_ptr;
extern CM_info *cm_ip_percentile_ptr;

extern Monitor_list *monitor_list_ptr;
extern Monitor_list *rtgRewrt_monitor_list_ptr;
extern int total_monitor_list_entries;     //total entries of monitors
extern int max_monitor_list_entries;

extern CM_info *save_cm_info_runtime_ptr;
extern int total_save_cm_runtime_entries;

extern DvmVectorIdxMappingTbl **dvm_idx_mapping_ptr;

extern int total_rtgRewrt_monitor_list_entries;
extern int total_dyn_cm_runtime_entries;
extern int nc_ip_data_mon_idx;     // NetCloud IP Data global variable which will store the index of its location in CM table

extern int add_node_and_pod_in_list(char *vector_name, CM_vector_info *vector_row, CM_info *cm_info_mon_conn_ptr, int idx, char operation);
extern int create_table_entry(int *row_num, int *total, int *max, char **ptr, int size, char *name);
extern void check_cavinfo_value(char *value);
extern char* get_next_cm_gdf_name(int cm_index, int data_size);
extern void set_cm_gp_info_index(int cm_index, int group_index, int group_num_vectors);
extern int set_cm_info_values(int cm_index, int num_group, int num_element, int group_num_vectors, int group_index);
extern void custom_monitor_setup(int frequency);
extern int set_custom_monitor_fd (fd_set * rfd, int max_fd);
extern int handle_if_custom_monitor_fd(void *ptr);
extern void fill_cm_data();
extern void print_cm_data(FILE * fp1, FILE* fp2);
extern void log_custom_monitor_gp();
extern char *get_cm_server(unsigned int index);
extern int cm_group_is_vector(char *gdf_name);
extern int cm_get_num_vectors(char *gdf_name, int mon_idx);
extern void initialize_cm_vector_groups();
//extern void print_cm_data_ptr(CM_info *cus_mon_ptr, FILE* fp1, FILE* fp2);
 extern void print_cm_data_ptr(int custom_mon_id, FILE* fp1, FILE* fp2);
//extern void fill_cm_data_ptr(CM_info *cus_mon_ptr);
extern void fill_cm_data_ptr(int custom_mon_id);
extern inline void stop_all_custom_monitors(); //use in ns_parent.c to stop all custom mon
int create_table_entry_ex(int *row_num, int *total, int *max, char **ptr, int size, char *name);
extern void add_select_custom_monitor();
extern void free_cm_info();
extern void free_cm_info_node(CM_info *cm_info, int mon_idx);
extern int check_allocation_for_reused(CM_info *cus_mon_ptr);

extern int add_select_custom_monitor_one_mon(CM_info *cus_mon_ptr);
extern inline void make_connection_for_one_mon(CM_info *cus_mon_ptr, char *err_msg);
extern int send_msg_to_create_server_for_one_mon(CM_info *cus_mon_ptr, int frequency, int custom_mon_id);
extern int stop_one_custom_monitor(CM_info *cus_mon_ptr, int reason); 

extern void get_ns_version_with_build_number();

extern void set_nv_mon_data_ptrs(int cm_index, CM_info *cm_ptr);

extern char ns_version[];

//custom monitor disaster recovery 
extern int kw_set_enable_monitor_dr(char *keyword, char *buf);
extern int cm_send_msg_to_cmon(void *ptr, int init_flag);
extern inline void handle_cm_disaster_recovery();

extern void cm_info_dump();

extern int cm_retry_flag;                    // custom monitor and control connection retry flag, will retry connection once closed or not
extern int max_cm_retry_count;               // custom monitor and control connection retry count


extern int check_and_open_nb_cntrl_connection(CM_info *cus_mon_ptr, char *ip, int port, char *msg);

extern int total_cm_runtime_entries;
extern int g_total_aborted_cm_conn;
extern int total_cm_runtime_entries;
extern CM_info **cm_dr_table;
extern int cm_make_nb_conn(CM_info *cus_mon_ptr, int init_flag);
extern inline void cm_handle_err_case(CM_info *cus_mon_ptr, int remove_from_epoll);
extern inline void cm_update_dr_table(CM_info *cus_mon_ptr);

extern inline void cm_make_send_msg(CM_info *cus_mon_ptr, char *msg_buf, int frequency, int *msg_len);
extern int cm_send_msg(CM_info *cus_mon_ptr, char *buf_ptr, int init_flag);

extern int monitor_runtime_changes_applied;
extern int monitor_deleted_on_runtime;
extern int monitor_added_on_runtime;

extern void free_cm_tbl_row(CM_info *cm_info, int table_type);

extern void set_nd_backend_mon_data_ptrs(int cm_index, int group_num_vectors);

extern int parse_vector_new_format (char *vector_line, char *vector_name, int *tieridx, int *instanceidx, int *vectoridx, char *backend_name, char *data, CM_info *cm_ptr, int expected_field, char *gdf_name);

extern void *ns_init_map_table(int tbl_type);
extern int ns_allocate_2d_matrix(void ***ptr, short delta_row_size, short delta_col_size, int *cur_row_size, int *cur_col_size);

extern int validate_and_fill_data(CM_info *cus_mon_ptr, int max, char *buffer, Long_long_data *data, void *metrics_idx, char *metrics_idx_found, char *vector_name);

extern void get_no_of_elements(CM_info *cus_mon_ptr, int *num_data);

//extern int nv_monitor_start_idx;
extern int nv_vector_format;

extern void kw_enable_non_blocking_conn(char *buf);
extern int enable_nb_conn;
extern int conn_timeout;

extern int is_mssql_monitor_applied;

extern char skip_unknown_breadcrumb;
extern int kw_set_unknown_breadcrumb(char *keyword, char *buf);

#define ND_BCI_DYN_MON_TIER_AGG_BACKEND_NORM_TABLE_SIZE 2*1024

extern int total_waiting_deleted_vectors;
extern int total_deleted_vectors;
extern int total_deleted_servers;
extern int total_deleted_instances;

extern int warning_no_vectors;

extern int init_size_for_dr_table;
extern int delta_size_for_dr_table;
extern int max_size_for_dr_table;
extern int max_dr_table_entries;

//max_fds on parent epoll
extern int g_parent_epoll_event_size;

//Functions for NetCloud
//Called when Generator fails
extern void gen_conn_failure_monitor_handling(int fd);
//Set data pointers for netcloud 2-D table in CM table
extern void set_netcloud_data_ptrs(int i);
extern void reset_ip_data_monitor();
extern int handle_monitor_stop(CM_info *cus_mon_ptr, int reason);

extern int  g_nd_max_tps, g_nd_max_resp, g_nd_max_count;
//End 
extern int kw_enable_heroku_monitor(char *keyword, char *buf, char *err_msg);
extern void get_breadcrumb_level(CM_info *cus_mon_ptr);
extern void apply_auto_json_monitor();
//mssql
extern int is_norm_init_done;
extern int check_for_csv_file(CM_info *cus_mon_ptr);

//ptr of json file of monitors
extern Mon_from_json *json_monitors_ptr;
extern int total_json_monitors;
extern int max_json_monitors;
extern char *auto_json_monitors_ptr;
extern char *auto_json_monitors_diff_ptr;

extern int is_outbound_connection_enabled;
extern int g_all_mon_id;
extern int g_mon_id;
extern int g_chk_mon_id;
extern int max_mon_id_entries;
extern int monitor_debug_level;
extern int max_node_list_entries;
extern char norm_tbl_init_done_for_node;
extern int node_list_entries;
extern NormObjKey *node_id_key;
extern NodePodInfo *node_pod_info_ptr;
extern NodePodInfo *pod_info_ptr;
extern NormObjKey *dyn_cm_rt_hash_key;
//extern MSSQL_table *mssql_table_ptr;
extern int total_mssql_row;
extern int max_mssql_row;
extern char * encode_message(char *msg_buf, int msg_len);
extern int filldata(CM_info *cus_mon_ptr,int buff_length);
extern void check_reused_vector(CM_vector_info local_cm_ptr);
extern CM_info *rtgRewrt_cm_info_ptr;
extern int total_rtgRewrt_cm_entries;
extern void apply_monitors_from_json();
extern char g_rtg_rewrite;
extern int delete_pod_node(char *node_name,char *pod_name, int norm_node_id, int norm_pod_id);
extern int discover_pod_node(char *node_name, char *pod_name, int norm_node_id, int norm_pod_id, char node_new_flag, char pod_new_flag);
extern int send_node_pod_status_to_ndc(NodePodInfo *node_pod_info_ptr, int node_pod_type, int discovery_shutdown);
extern int parse_kubernetes_vector_format(char *vector_name, char *field[8]);
extern void send_node_pod_status_message_to_ndc();

extern int g_disable_dvm_vector_list;
extern void kw_set_disable_dvm_vector_list(char *keyword, char *buf);
extern int check_if_alredy_deleted(CM_info *cus_mon_ptr, int idx);
extern int create_mapping_table_col_entries(int row_no, int vec_count, int max_mapping_tbl_vectors_entries);
extern void create_entry_in_reused_or_deleted_structure(int **input, int *max_entries);
extern void initialize_reused_deleted_vector();
extern void close_custom_monitor_connection(CM_info *cus_mon_ptr);
extern int g_enable_recreate_parent_epoll;
extern void kw_set_allocate_size_for_dr_table(char *keyword, char *buf);
extern void kw_coherenece_remove_trailing_space(char *keyword, char *buf);
extern int kw_set_parent_epoll_event_size(char *keyword, char *buf);
extern int kw_set_monitor_debug_level(char *keyword, char *buf);
extern int handle_epoll_error_for_cm(void *ptr);
extern void get_group_id_from_gdf_name(char *fname, char group_id[]);
extern int create_mapping_table_row_entries();

extern void initialize_dyn_cm_rt_hash_key();
extern NormObjKey *dyn_cm_rt_hash_key;
extern int g_mssql_data_buf_size;
extern void create_accesslog_id(char *origin_cmon_and_ip_port, char *pgm_args, char *vector_name, char *mon_name);
extern int serial_no;
extern void set_reused_vector_counters(CM_vector_info *vector_list, int i, CM_info *cm_info_mon_conn_ptr, char *cs_ip, int cs_port, char *pgm_path, char *pgm_args, int instant_flag);

//MBean Monitor code
extern int search_mon_entry_in_list(char *monitor_name);
extern void add_in_cm_table_and_create_msg(int runtime_flag);
extern int make_and_send_del_msg_to_NDC(char **tiername, int no_of_tiers, char **exclude_tier, int no_of_exclude_tier, char **specific_server, int no_of_specific_server, char **exclude_server, int no_of_exclude_server, int tier_group_index, char *instance_name, char *gdf_name);
extern void add_entry_for_mbean_mon(char *monitor_name, char **tiername, int no_of_tiers, char **exclude_tier, int no_of_exclude_tier, char **specific_server, int no_of_specific_server, char **exclude_server, int no_of_exclude_server, int tier_group_index, char* gdf_name, int runtime_flag);
extern void create_breadcrumb_path(int vector_list_row, int runtime_flag, char *err_msg, CM_vector_info *vector_table_ptr, CM_info *parent_ptr);
extern void reload_normid_from_metadata_csv(NormObjKey *sql_id_key, char *csv_file_path, char *csv_file_name, int name_col, int norm_id_col);
extern void free_mon_lst_row(int mon_idx);
extern int convert_buf_to_array(char *buffer, int *arr_val);
extern int read_mssql_file(char *err_msg);
extern void generate_csv_file_name(char *gdf_name, char *csv_name);
extern int verify_vector_format(CM_vector_info *cm_ptr, CM_info *parent_ptr);
extern long long calculate_timestamp_from_date_string(char * buffer);
extern void free_vec_lst_row(CM_vector_info * vector_info);
extern void set_no_of_monitors();
extern void set_no_of_monitors_for_tsdb(int *flag);
extern void read_nc_monitor_data(int cmd_fd, User_trace *vuser_trace_msg);
void put_free_id(struct id_pool *pool, int id);
int get_free_id(struct id_pool *pool);
int get_next_mon_id();  /* will return a mon_id from free_mon_id_pool if available otherwise from g_all_mon_id */

extern HiddenFileInfo *hidden_file_ptr;
extern int total_hidden_file_entries;
extern int max_hiddlen_file_entries;
//This function is to send cm_update_msg to ndc
extern void make_cm_update_msg_and_send_msg_to_ndc(char *mon_args, int mon_info_index);

//This is for to free node pod info
extern void free_node_pod(struct NodePodInfo *node_pod_info_ptr);

//Destination Tier Any Server
extern void free_mon_list_info( Mon_List_Info *mon_list );
//extern Mon_List_Info *search_mon_name_in_mon_list_info(char *mon_name, int dest_tier_name_norm_id, int source_tier_idx);
extern void copy_to_scratch_buffer_for_monitor_request(char *buffer, int length, char reset_flag, int reset_length);
#endif
