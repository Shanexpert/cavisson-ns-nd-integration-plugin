#ifndef _ns_custom_monitor_redesign_h
#define _ns_custom_monitor_redesign_h

#include "ns_exit.h"
#include "ns_custom_monitor.h"
#include "ns_server_admin_utils.h"
#include "ns_dynamic_vector_monitor.h"
#include "nslb_alloc.h"

extern int kw_set_standard_monitor_RDnew(char *keyword, char *buf, int runtime_flag, char *pod_name, char *err_msg, JSON_info *jsonElement);
extern int kw_set_dynamic_vector_monitor_RDnew(char *keyword, char *buf, char* server_name, int use_lps_flag, int runtime_flag, char *pod_name, char *err_msg, char *init_vectors_file, JSON_info *json_info_ptr, int skip_breadcrumb_creation);
extern void make_send_msg_for_monitors_RDnew(CM_info *cm_info_mon_conn_ptr, char **msg_buff, int *total);
extern void ns_cm_monitor_log(int mask, int debug_mask, int module_mask, char *file, int line,
                       char *func, CM_info *cm_info_mon_conn_ptr, unsigned int event_id, int severity, char *format, ...);
extern int custom_monitor_config(char *keyword, char *buf, char *server_name, int dyn_cm, int runtime_flag, char *err_msg, char *pod_name, JSON_info *json_element_ptr, char skip_breadcrumb_creation);
extern int add_cm_vector_info_node(int monitor_list_index, char *vector_name, char *buffer, int max, char *breadcrumb, int runtime_flag, int dyn_cm);
extern void extract_prgms_args(char *custom_buf, char *buf , int num);
extern void make_hidden_file(HiddenFileInfo *ptr, char *file_path , int switch_flag);
#endif

