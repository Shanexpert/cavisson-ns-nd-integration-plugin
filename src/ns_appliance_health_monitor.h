#ifndef _ns_appliance_health_monitor_h

#define _ns_appliance_health_monitor_h

#include "nslb_mon_registration_util.h"

#define MAX_NO_OF_ELEMENTS  	100
#define HM_MAX_BUFFER_LENGTH	4096
#define HM_MAX_NAME_LENGTH	1024
#define THRESHOLD_RETRY_COUNT	30
#define NS_HEALTH_MONITOR_GRP_ID 10656 
#define NVM_MONITORS		1
#define NON_NVM_MONITORS 	2
extern char g_machine[128];
extern char g_store_machine[128];
extern char g_hierarchy_prefix[128];
typedef struct HM_info
{
  char *gdf_name;
  char *vector_name;
  int gp_info_idx;
  void *metrics_idx;
  int num_element;
  int rtg_index;
  double *data;
  char  metric_id_found;
  char is_data_overflowed; 
} HM_info;

extern HM_info *hm_info_ptr;
extern int g_tier_server_count_index;
extern int g_tier_index_send;

typedef struct HM_Times_data
{
  TimesData rtg_pkt_ts_diff;
  TimesData progress_report_processing_delay;
} HM_Times_data;
 
extern HM_Times_data *hm_times_data;


typedef struct HM_Data
{
  double num_tiers;
  double num_servers;
  double num_instances;
  double num_auto_scaled_tiers;
  double num_auto_scaled_servers;
  double num_auto_scaled_instances;
  double num_inactive_servers;
  double num_inactive_instances;
  double total_monitors_applied;
  double num_monitors_exited;
  double mon_with_thirdparty_issues;
  double retry_count_exceeding_threshold;
  double num_control_conn_failure;
  double num_server_error_get_host_by_name; 
  double num_duplicate_vectors;
  double data_already_filled;
  double data_more_than_expected;
  double data_less_than_expected;
  double total_data_throughput;
  double rtg_packet_size;
  double gdf_version;
  double deleted_vectors;
  double reused_vectors;
  double vectors_discovered;
  double monitors_added;
  double monitors_deleted;
  double rtg_pkt_ts_diff_avg;
  double rtg_pkt_ts_diff_min;
  double rtg_pkt_ts_diff_max;
  double rtg_pkt_ts_diff_count;
  double progress_report_processing_delay_avg;
  double progress_report_processing_delay_min;
  double progress_report_processing_delay_max;
  double progress_report_processing_delay_count; 
  double num_DataOverFlow;
} HM_Data;

extern HM_Data *hm_data;

extern int kw_set_enable_appliance_health_monitor(char *keyword, char *buf);
extern void handle_monitor_registration(void *ptr, int fd);
extern void apply_component_process_monitor();
extern void update_health_monitor_sample_data(double *value);
extern char **init_2d(int no_of_host);
extern char ** printHMVectors();
extern int kw_set_enable_auto_monitor_registration(char *keyword, char *buf);
extern void init_hm_data_structures();
extern void process_hm_gdf();
extern void update_structure_with_global_values();
extern void fill_hm_data();
extern void print_hm_data(FILE* fp1, FILE* fp2);
extern void create_server_count_data_buf();
extern void apply_tier_server_count_monitor();
extern void update_times_data(Times_data *time_data , Long_data value);
extern void mon_update_times_data(TimesData *time_data, Long_data value);
extern int apply_monitors_for_jvm();
#endif
