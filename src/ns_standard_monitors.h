/**
 * Name             : ns_standard_monitors.h
 * Author           : Shri Chandra
 * Purpose          : This file contains decleration of standard monitor structure and parsing method
 * Modification Date:
 * Initial Version  : 09/09/2009
***********************************************************************************************************/

#ifndef _ns_standard_monitor_h
#define _ns_standard_monitor_h

#include "ns_dynamic_vector_monitor.h"
#include "ns_server_admin_utils.h"
#include <nslb_mem_pool.h>

#define MAX_BUF_SIZE_FOR_STD_MON 1024
#define MAX_BIG_BUF_SIZE_FOR_STD_MON 128*1024
#define MAX_BUF_SIZE_FOR_CUS_MON 64*1024  /*size of java monitors arguments is very large*/
#define MAX_BUF_SIZE_FOR_DYN_MON 1024

#define STD_RUNTIME_RETURN_OR_EXIT(runtime_flag) \
{ \
  if(runtime_flag) \
    return -1; \
  else \
    NS_EXIT(-1,"Exit called from STD_RUNTIME_RETURN_OR_EXIT"); \
}   

typedef struct 
{
  char *tier_name;
  char flag;
} TierInfo;

typedef struct
{
  char *tier_group;
  char flag;
} TierGroupInfo;

typedef struct {
  NSLB_MP_COMMON;
  TierInfo *tier_info;
  int total_active_tier;
  int total_tier;
  TierGroupInfo *tier_group_info;
  char **exclude_tier;
  int total_exclude_tier;
  char **exclude_server;
  int total_exclude_server;
  char **specific_server;
  int total_specific_server;
}MBeanJsonInfo;

typedef struct {
  char flag;
  short record_id;
  char *mon_name;
  char *config_file;
  char *msg_buf;
  char *gdf_name;
  nslb_mp_handler mbean_json_info_pool;
}MBeanMonInfo;

extern MBeanMonInfo *mbean_mon_ptr;

typedef struct {
  char *monitor_name; //Name of Monitor
  char *gdf_name; // Name of GDF File
  int run_option; // Run Option
  char *pgrm_name; // Monitor Prorgam Name
  char *pgrm_type; // Monitor Program Type ex . bin, java, shell
  char *fixed_args; // Arguments option which required by standard monitor
  char *machine_types; //This contains list of server types
  char *monitor_type; // Type of monitor eg. CM(custom monitor) or DVM(dynamic vector monitor). Default is CM.
  char *comment; // Comments used by GUI
  char use_lps;
  char use_args;
  char skip_breadcrumb_creation;
  //These 3 entries will be used for Mbean monitors.
  char agent_type;		//BCI or CMON or ALL
  short record_id;                 
  char *config_json;
}StdMonitor;

extern void create_and_fill_hidden_ex(int gdf_flag, int flag, JSON_info *json_ptr);
extern void remove_hidden_file(int gdf_flag);
extern int metric_based_mon_arg_parsing(char *arguments, char *server, char *vector_name, StdMonitor *std_mon_ptr, char *server_name, char *dvm_vector_arg_buf, int runtime_flag, char *err_msg, int use_lps_flag, JSON_info *jsonElement,int tier_idx,int server_index);
extern int kw_set_standard_monitor(char *keyword, char *buf, int runtime_flag, char *pod_name, char *err_msg, JSON_info *json_info_ptr); 
extern int kw_set_standard_monitor(char *keyword, char *buf, int runtime_flag, char *pod_name, char *err_msg, JSON_info *json_info_ptr);
extern StdMonitor *get_standard_mon_entry(char *monitor_name, char *machine_type, int runtime_flag, char *err_msg); 
extern void set_dvm_args(char *dvm_vector_arg_buf, char *dvm_data_arg_buf, int *use_lps_flag, char *vector_name, StdMonitor *std_mon_ptr);
extern int set_agent_type(JSON_info *json_info_ptr, StdMonitor *std_mon_ptr, char *mon_name);
//extern int kw_set_standard_monitor(char *keyword, char *buf, int runtime_flag, char *err_msg);
#endif
