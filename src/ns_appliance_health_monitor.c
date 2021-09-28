#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <v1/topolib_structures.h>
#include "util.h"
#include "netstorm.h"
#include "ns_msg_com_util.h"
#include "ns_msg_def.h"
#include "ns_log.h"
#include "ns_trace_level.h"
#include "ns_alloc.h"
#include "ns_gdf.h"
#include "ns_test_gdf.h"
#include "ns_data_types.h"
#include "ns_common.h"
#include "ns_global_settings.h"
#include "ns_custom_monitor.h"
#include "ns_standard_monitors.h"
#include "wait_forever.h"
#include "ns_appliance_health_monitor.h"
#include "nslb_cav_conf.h"
#include "v1/topolib_runtime_changes.h"
#include "netomni/src/core/ni_scenario_distribution.h"
#include "ns_monitor_profiles.h"
#include "ns_custom_monitor_RDnew.h"
#include "ns_string.h"
#include "ns_event_id.h"
#include "ns_event_log.h"
#include "ns_mon_log.h"
#include "ns_tsdb.h"
#include "url.h"
//Global pointers to access from any where.
HM_info *hm_info_ptr = NULL;
HM_Data *hm_data = NULL;
HM_Times_data *hm_times_data = NULL;
char g_machine[128];
char g_store_machine[128];
char g_hierarchy_prefix[128];
int g_tier_server_count_index = -1;
int g_tier_index_send=-1;
//Keyword parsing for ENABLE_AUTO_MONITOR_REGISTRATION. Value can be 0/1. 
int kw_set_enable_auto_monitor_registration(char *keyword, char *buf)
{
  char key[1024] = "";
  int num, value;
  char text[1024] = "";

  NSDL3_MON(NULL, NULL, "Parsing Keyword %s. Buffer is '%s'", keyword, buf);

  num = sscanf(buf, "%s %d %s", key, &value, text);
  
  if(num < 2)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Wrong usage of Keyword %s. ENABLE_AUTO_MONITOR_REGISTRATION <1/0>. Disabling it by default.", keyword);
    value = 0;
  }

  if(value > 0)
    global_settings->enable_health_monitor = 1;
 
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "NS Appliance Health Monitor is %s. Value of the keyword is %d", (global_settings->enable_health_monitor ? "Enabled" : "Disabled"), global_settings->enable_health_monitor);
  return 0;
}

static void init_hm_info()
{
  char gdf_name[HM_MAX_NAME_LENGTH];

  MY_MALLOC_AND_MEMSET(hm_info_ptr, (sizeof(HM_info)), "Initialization of HM_info structure", -1);

  sprintf(gdf_name, "%s/sys/cm_hm_ns_health_stats.gdf", g_ns_wdir);
  MY_MALLOC(hm_info_ptr->gdf_name, strlen(gdf_name) + 1, "NS health Monitor gdf allocation", 0);
  strcpy(hm_info_ptr->gdf_name, gdf_name);

  //This is increamented to increase the count of total monitor in graph.
  // g_mon_id = get_next_mon_id();
}


void process_hm_gdf()
{
  int pdf_id, graphId;
  char *graphDesc = NULL;
  //char err_msg[HM_MAX_NAME_LENGTH];
  char vector_name[HM_MAX_NAME_LENGTH];
  int group_size;

  NSDL2_GDF(NULL, NULL, "Method called, enable_health_monitor = %d", global_settings->enable_health_monitor);
  if(!global_settings->enable_health_monitor)
    return;

  if(!hm_info_ptr)
    init_hm_info();

  if(enable_store_config)
    sprintf(vector_name, "%s!Cavisson%c%s",g_store_machine,global_settings->hierarchical_view_vector_separator, g_machine);
  else
    sprintf(vector_name,"Cavisson%c%s",global_settings->hierarchical_view_vector_separator,g_machine);

  MALLOC_AND_COPY(vector_name, hm_info_ptr->vector_name, strlen(vector_name) + 1, "Vector name for HM(s)", -1);
  hm_info_ptr->gp_info_idx = group_count;
  get_no_of_elements_of_gdf(hm_info_ptr->gdf_name, &hm_info_ptr->num_element, &group_size,  &pdf_id, &graphId, graphDesc);

  MY_MALLOC_AND_MEMSET(hm_info_ptr->data, (hm_info_ptr->num_element * sizeof(double)), "Data allocation for HM", -1);

  hm_info_ptr->rtg_index = msg_data_size;

  process_gdf(hm_info_ptr->gdf_name, 0, NULL, 0);
}

char ** printHMVectors()
{
  char **TwoD;

  NSDL2_GDF(NULL, NULL, "Method Called");
  
  //This Health monitor will be a type of custom monitor so, need to allocate it with only one entry.
  TwoD = init_2d(1);

  fprintf(write_gdf_fp, "%s %d\n", hm_info_ptr->vector_name, hm_info_ptr->rtg_index);

  fill_2d(TwoD, 0, hm_info_ptr->vector_name);

  return TwoD;
}


int convert_and_save_data_into_double(char *buffer, double *data)
{
  char *ptr = NULL, *end_ptr = NULL;
  int i = 0; 

  ptr = strtok(buffer, " ");

  while(ptr)
  {
    ++i;
 
    if(i > hm_info_ptr->num_element)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Found Data element greater than maximum data expected for Group '%s'. Hence returning. Data parsed is %d, and data elements in gdf is %d", hm_info_ptr->gdf_name, i, hm_info_ptr->num_element);
      break;
    }

    data[i - 1] = strtod(ptr, &end_ptr);
    ptr = strtok(NULL, " ");
  }

  if(i < hm_info_ptr->num_element)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Found Data element less than expected data element for this group '%s'. Will not process data elements for this group for now. Received data elements = %d and expected data element is %d", hm_info_ptr->gdf_name, i, hm_info_ptr->num_element);
    return -1;
  }
  return 0;
}

void init_hm_data_structures()
{
  MY_MALLOC_AND_MEMSET(hm_data, sizeof(HM_Data), "NS HealthMonitorData", -1);
  MY_MALLOC_AND_MEMSET(hm_times_data, sizeof(HM_Times_data), "NS HealthMonitorData", -1);
}


void reset_hm_data()
{
  hm_data->num_auto_scaled_tiers = 0;
  hm_data->num_auto_scaled_servers = 0;
  hm_data->num_auto_scaled_instances = 0;
  hm_data->num_inactive_servers = 0;
  hm_data->num_inactive_instances = 0;
  hm_data->num_monitors_exited = 0;
  hm_data->mon_with_thirdparty_issues = 0;
  hm_data->retry_count_exceeding_threshold = 0;
  hm_data->num_control_conn_failure = 0;
  hm_data->num_server_error_get_host_by_name = 0;
  hm_data->num_duplicate_vectors = 0;
  hm_data->data_already_filled = 0;
  hm_data->data_more_than_expected = 0;
  hm_data->data_less_than_expected = 0;
  hm_data->vectors_discovered = 0;
  hm_data->monitors_deleted = 0;
  hm_data->deleted_vectors = 0;
  hm_data->reused_vectors = 0;
  hm_data->monitors_added = 0;
  hm_data->total_data_throughput = 0;
  hm_data->num_DataOverFlow = 0;
  //hm_data->nvm_queue_count = 0;
  nslb_mon_reset_time_values(&hm_times_data->rtg_pkt_ts_diff);
  nslb_mon_reset_time_values(&hm_times_data->progress_report_processing_delay);
}

static void calculate_per_min_data(double *value)
{
  double data;
  data = *value * 60 * 1000;
  *value = data / (global_settings->progress_secs);
}

static void convert_to_kilobits_per_sec(double *value)
{
  double data;
  data = *value * 8 / 1024;   // converting to kilo bits
  *value = data / ((global_settings->progress_secs) / 1000);  //as progress_secs is in ms, so need to divide it by 1000.
}

static void convert_data_to_per_min()
{
  calculate_per_min_data(&hm_data->num_auto_scaled_tiers);
  calculate_per_min_data(&hm_data->num_auto_scaled_servers);
  calculate_per_min_data(&hm_data->num_auto_scaled_instances);
  calculate_per_min_data(&hm_data->num_inactive_servers);
  calculate_per_min_data(&hm_data->num_inactive_instances);
  calculate_per_min_data(&hm_data->num_monitors_exited);
  calculate_per_min_data(&hm_data->mon_with_thirdparty_issues);
  calculate_per_min_data(&hm_data->retry_count_exceeding_threshold);
  calculate_per_min_data(&hm_data->num_control_conn_failure);
  calculate_per_min_data(&hm_data->num_server_error_get_host_by_name);
  calculate_per_min_data(&hm_data->num_duplicate_vectors);
  calculate_per_min_data(&hm_data->data_already_filled);
  calculate_per_min_data(&hm_data->data_more_than_expected);
  calculate_per_min_data(&hm_data->data_less_than_expected);
  calculate_per_min_data(&hm_data->deleted_vectors);
  calculate_per_min_data(&hm_data->reused_vectors);
  calculate_per_min_data(&hm_data->vectors_discovered);
  calculate_per_min_data(&hm_data->monitors_added);
  calculate_per_min_data(&hm_data->monitors_deleted);
}

static void set_hm_data_for_times_rtg_pkt_ts_diff(TimesData *time_data)
{
  hm_data->rtg_pkt_ts_diff_avg = time_data->total_size / time_data->count;
  hm_data->rtg_pkt_ts_diff_min = time_data->min;
  hm_data->rtg_pkt_ts_diff_max = time_data->max;
  hm_data->rtg_pkt_ts_diff_count = time_data->count;
}

static void set_hm_data_for_times_progress_report_processing_delay(TimesData *time_data)
{
  hm_data->progress_report_processing_delay_avg = time_data->total_size / time_data->count;
  hm_data->progress_report_processing_delay_min = time_data->min;
  hm_data->progress_report_processing_delay_max = time_data->max;
  hm_data->progress_report_processing_delay_count = time_data->count;
}

void fill_hm_data_ptr()
{
  char *hm_block_ptr = NULL;
  int data_index = 0, i;

  Group_Info *local_group_data_ptr = NULL;
  Graph_Info *local_graph_data_ptr = NULL;

  local_group_data_ptr = group_data_ptr + hm_info_ptr->gp_info_idx;
  local_graph_data_ptr = graph_data_ptr + local_group_data_ptr->graph_info_index;

  hm_block_ptr = (char *) msg_data_ptr + hm_info_ptr->rtg_index;
  
  //converts necessary element to per minute.
  convert_data_to_per_min();
  convert_to_kilobits_per_sec(&hm_data->total_data_throughput);
  set_hm_data_for_times_rtg_pkt_ts_diff(&hm_times_data->rtg_pkt_ts_diff);
  set_hm_data_for_times_progress_report_processing_delay(&hm_times_data->progress_report_processing_delay);
  //type casting structure of double element to double data array.
  hm_info_ptr->data = (double *) (hm_data);

  for(i = 0; i < local_group_data_ptr->num_graphs; i++)   //Loop for num graphs
  {
    NSDL4_MON(NULL, NULL, "Going to fill data in rtg buffer for GroupName '%s'. Graph index = %d, Graph data = %d", local_group_data_ptr->group_name, i, hm_info_ptr->data[data_index]);

    hm_block_ptr = fill_data_by_graph_type(hm_info_ptr->gp_info_idx, i, 0, 0, local_graph_data_ptr->data_type, hm_block_ptr, hm_info_ptr->data, &data_index, &(hm_info_ptr->is_data_overflowed));

    local_graph_data_ptr++;
  
    if(!(g_tsdb_configuration_flag & RTG_MODE))
    { 
       CM_info *cm_ptr = NULL;
   //    time_t current_time;
     //  current_time = time(NULL);
       MY_MALLOC_AND_MEMSET(cm_ptr,  sizeof(CM_info), "Data allocation for Metric ID", -1);
       cm_ptr->gp_info_index = hm_info_ptr->gp_info_idx;
       cm_ptr->no_of_element = hm_info_ptr->num_element;
       cm_ptr->frequency = global_settings->progress_secs ; 
       MALLOC_AND_COPY(hm_info_ptr->vector_name,cm_ptr->monitor_name, strlen(hm_info_ptr->vector_name) + 1, "Vector name for HM(s)", -1);
       if(hm_info_ptr->metrics_idx == NULL)
       {
	 ns_tsdb_malloc_metric_id_buffer(&hm_info_ptr->metrics_idx, hm_info_ptr->num_element);
//	 MY_MALLOC_AND_MEMSET(hm_info_ptr->metrics_idx, (hm_info_ptr->num_element * sizeof(long)), "Data allocation for Metric ID", -1);

       }
       MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,"ns_tsdb_insert Method Called with  vectorsname %s", cm_ptr->monitor_name);

      ns_tsdb_insert(cm_ptr, hm_info_ptr->data, hm_info_ptr->metrics_idx, &hm_info_ptr->metric_id_found, 0, hm_info_ptr->vector_name); //Need to discuss
   }
 }
  reset_hm_data();
}


//This function will fill data in rtg buffer.
void fill_hm_data()
{
  if(!global_settings->enable_health_monitor)
    return;

  /*g_all_mon_id will be increased if any monitor is added in CM_info structure. Monitor count and vector count is different. total_cm_entries 
    stores all vector count. And g_all_mon_id is increased when a new monitor is added.*/

  //hm_monitor_data->num_total_monitors_applied = g_all_mon_id;

  NSDL4_MON(NULL, NULL, "Going to fill data for gdf %s", hm_info_ptr->gdf_name);
  fill_hm_data_ptr();
}


void print_hm_data_ptr(FILE* fp1, FILE* fp2)
{
  int i;
  char *hm_block_ptr = NULL;
  Graph_Data *gdata;

  Group_Info *local_group_data_ptr = NULL;
  Graph_Info *local_graph_data_ptr = NULL;

  local_group_data_ptr = group_data_ptr + hm_info_ptr->gp_info_idx;
  local_graph_data_ptr = graph_data_ptr +  local_group_data_ptr->graph_info_index;

  hm_block_ptr = (char *) msg_data_ptr + hm_info_ptr->rtg_index;

  fprint2f(fp1, fp2, "%s (%s):\n", local_group_data_ptr->group_name, hm_info_ptr->vector_name);
   
  //loop_for_graph
  for(i = 0; i < local_group_data_ptr->num_graphs; i++)
  {
    //gdata is pointing to 3-D table graph_data of graph_info structure.
    //Passing 1st and 2nd arguement as 0. This arguement means group vector idx and graph vector idx respectively. As it will be a custom monitor then num vector for group and graph will be 1. so index will be 0.
    gdata = get_graph_data_ptr(0, 0, local_graph_data_ptr);

    hm_block_ptr = print_data_by_graph_type(fp1, fp2, local_graph_data_ptr, hm_block_ptr, PRINT_PERIODIC_MODE, gdata, hm_info_ptr->vector_name);
    local_graph_data_ptr++;
  }  
}


void print_hm_data(FILE* fp1, FILE* fp2)
{
  if(!global_settings->enable_health_monitor)
    return;
 
  NSDL4_MON(NULL, NULL, "Going to print data on progress report for gdf %s", hm_info_ptr->gdf_name);
  print_hm_data_ptr(fp1, fp2);
}

void update_structure_with_global_values()
{
  if(!global_settings->enable_health_monitor)
    return;

  hm_data->num_tiers = topolib_get_total_tier_count(topo_idx);
  hm_data->num_instances = topolib_get_total_instance_entries(topo_idx);
  hm_data->num_servers = topolib_get_total_server_count(topo_idx);

  hm_data->monitors_added = total_monitor_list_entries - hm_data->total_monitors_applied;
  hm_data->total_monitors_applied = total_monitor_list_entries;
  hm_data->rtg_packet_size = msg_data_size;
  
  //tesrun.gdf should also be included in gdf version, so +1 is needed.
  hm_data->gdf_version = (test_run_gdf_count + 1);
}

void apply_core_and_postgress_monitor(FILE *fauto_mprof, char *vector_name)
{
  char monitor_buffer[1024], copy_buffer[1024], err_msg[1024] = "";

  sprintf(monitor_buffer, "STANDARD_MONITOR %s %s%cHMCoreMonitorStats HMCoreMonitorStats",vector_name ,vector_name, global_settings->hierarchical_view_vector_separator);
  strcpy(copy_buffer,monitor_buffer);
  if(kw_set_standard_monitor("STANDARD_MONITOR", monitor_buffer, 0, NULL, err_msg, NULL) < 0)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error in applying HMCoreMonitorStats monitor for NSDBUpload. Error: %s", err_msg);
  }
  else
  {
    fprintf(fauto_mprof, "%s\n", copy_buffer);
  }
}

void apply_component_process_monitor()
{
  char monitor_buffer[8*1024], copy_buffer[1024], err_msg[1024] = "";
  char auto_mprof[4*MAX_NAME_LEN];
  FILE *fauto_mprof;
  char Tiername[512];
  char vector_name[2*1024] = {0};
  if(enable_store_config)
    sprintf(Tiername,"%s!Cavisson",g_store_machine);
  else
    sprintf(Tiername,"Cavisson");

  sprintf(auto_mprof, "%s/logs/%s/ns_files/ns_auto.mprof", g_ns_wdir, global_settings->tr_or_common_files);
  NSDL2_SCHEDULE(NULL, NULL, "Method Called, Opening file '%s' on write mode in function apply_component_process_monitor", auto_mprof);

  if ((fauto_mprof = fopen(auto_mprof, "a+")) == NULL) 
  {
    fprintf(stderr, "error in creating auto mprof file\n");
    perror("fopen");
    return;
  }

  sprintf(vector_name,"%s%c%s",Tiername, global_settings->hierarchical_view_vector_separator,g_machine);

  if((g_auto_ns_mon_flag == INACTIVATE) )   //If auto monitor is disabled.
  {
    char controller_name[128];
    if(get_controller_name(controller_name) == MON_FAILURE)
      return;
    if(enable_store_config)
    {
      sprintf(monitor_buffer, "STANDARD_MONITOR %s %s!%s%cAutoMon ProcessDataExV3 -T TR%d -C %s",
                  vector_name, g_store_machine,vector_name,global_settings->hierarchical_view_vector_separator,testidx, controller_name);
    }
    else
    {   
      sprintf(monitor_buffer, "STANDARD_MONITOR %s %s%cAutoMon ProcessDataExV3 -T TR%d -C %s",
            vector_name, vector_name, global_settings->hierarchical_view_vector_separator , testidx, controller_name);
    }
    strcpy(copy_buffer,monitor_buffer);
    if(kw_set_standard_monitor("STANDARD_MONITOR", monitor_buffer, 0, NULL, err_msg, NULL) < 0)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
                 "Error in applying ProcessDataExV3 monitor : %s", err_msg);
    }
    else
    {
      fprintf(fauto_mprof, "%s\n", copy_buffer);
    }
    apply_core_and_postgress_monitor(fauto_mprof,vector_name);
    fclose(fauto_mprof);
  }
  //If auto_mon is active and also new process monitoring is enabled, we have already applied 
  else 
  {
    apply_core_and_postgress_monitor(fauto_mprof,vector_name); 
    fflush(fauto_mprof);
    fclose(fauto_mprof);
    return;
  }
}
 
int apply_monitors_for_jvm()
{
  int i=1;
  char err_msg[1024] = ""; 
  char auto_mprof[MAX_NAME_LEN];
  FILE *fauto_mprof;
  char auto_mon_cmd[2*MAX_NAME_LEN];
  char copy_buffer[2*MAX_NAME_LEN];
  char server_name[2*128];
  char jvm_pid_file[1024];
  auto_mon_cmd[0] = '\0', auto_mprof[0] = '\0';

  sprintf(auto_mprof, "%s/logs/TR%d/common_files/ns_files/ns_auto.mprof", g_ns_wdir, testidx);
  if ((fauto_mprof = fopen(auto_mprof, "a+")) == NULL)
  { 
    fprintf(stderr, "error in screating auto mprof file\n");
    perror("fopen");
    return MON_FAILURE;
  }
  fprintf(fauto_mprof, "############# Java Thread Stats  ################\n");

  sprintf(server_name,"Cavisson%c%s",global_settings->hierarchical_view_vector_separator, g_machine);

  for(i=1;i<=global_settings->num_process;i++)                                                       
  {
    sprintf(jvm_pid_file, "%s/logs/TR%d/.pidfiles/.CJVM%d.pid", g_ns_wdir, testidx,i);
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              ".CJVM%d.pid",i); 
    sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s%cJVM_%d JavaThreadingStats -f %s", server_name, server_name, global_settings->hierarchical_view_vector_separator, i, jvm_pid_file);
    strcpy(copy_buffer,auto_mon_cmd);
    if(kw_set_standard_monitor("STANDARD_MONITOR",copy_buffer, 1, NULL, err_msg, NULL) < 0)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error in applying JavaThreadingStats monitor . Error: %s", err_msg);
    }
    else
    {
      fprintf(fauto_mprof, "%s\n", auto_mon_cmd);
    }
 
    //code for applying JVM JavaGCJMXSun8
    sprintf(auto_mon_cmd, "STANDARD_MONITOR %s %s%cJVM_%d JavaGCJMXSun8 -f %s", server_name, server_name, global_settings->hierarchical_view_vector_separator, i, jvm_pid_file);
    strcpy(copy_buffer,auto_mon_cmd);
    if(kw_set_standard_monitor("STANDARD_MONITOR",copy_buffer, 1, NULL, err_msg, NULL) < 0)
    { 
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error in applying JavaGCJMXSun8 monitor . Error: %s", err_msg);
    }
    else
    { 
      fprintf(fauto_mprof, "%s\n", auto_mon_cmd);
    } 
  }
  NSDL2_MON(NULL, NULL, "Java stdmon entry %s",auto_mon_cmd);
  fflush(fauto_mprof);
  fclose(fauto_mprof);
  return 0;
}

void update_health_monitor_sample_data(double *value)
{
  if(!global_settings->enable_health_monitor)
    return;

  *value += 1;
}

static void set_elements_for_cm_info(int fd, CM_info *cm_ptr)
{
  int dbuf_len;
  
  cm_ptr->fd = fd;

  cm_ptr->no_log_OR_vector_format &= ~FORMAT_NOT_DEFINED;
  cm_ptr->no_log_OR_vector_format |= NEW_VECTOR_FORMAT;
  //cm_ptr->flags |= ALL_VECTOR_DELETED;

  if(cm_ptr->no_of_element == 0)
    get_no_of_elements(cm_ptr, (int *)&(cm_ptr->no_of_element));

  //We set dbuf_len to a big size because there may come a big error message from custom monitor
  if(cm_ptr->data_buf == NULL)
  {
    dbuf_len = MAX_MONITOR_BUFFER_SIZE + 1;
    MY_MALLOC(cm_ptr->data_buf, dbuf_len, "Custom Monitor data", -1);
    memset(cm_ptr->data_buf, 0, dbuf_len);
  }
}



void handle_monitor_registration(void *ptr, int fd)
{
  int i;
  char *fields[MAX_FIEDLS_FOR_MON_REGISTRATION];
  char save_buf[1024];
  int num_fields;
  char err_msg[1024], buff[1024];
  CM_info *cm_table_ptr = NULL;
  int ret;
  char server_name[2*128];
 
  if(!global_settings->enable_health_monitor)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "IMPORTANT: Control Connection AUTO_MONITOR_REGISTRATION Keyword is diabled and still we are receiving data from some component. Here is the message received on the socket: %s", ((Mon_Reg_Req*)ptr)->message);
    //remove_select_msg_com_con(fd);
    REMOVE_SELECT_MSG_COM_CON(fd, DATA_MODE);
    if(close(fd) < 0)
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error in closing control connection from component. Error: %s", nslb_strerror(errno));
    return;
  }

  // Buffer type = (int)+(int)+(char*)   -->  (msg_len) (opdode) (gdf_name;component_name)
  //                     Size in bytes   -->  <-- 4 --> <--4 --> <---- user defined ----->

  Mon_Reg_Req *mon_reg_req_ptr = (Mon_Reg_Req*) ptr;
  MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Info regarding product health monitor. Opcode = %d, Message = %s, fd = %d", mon_reg_req_ptr->opcode, mon_reg_req_ptr->message, fd);
  strcpy(save_buf, mon_reg_req_ptr->message);
  num_fields = get_tokens(save_buf, fields, ";", MAX_FIEDLS_FOR_MON_REGISTRATION);

  if(num_fields != MAX_FIEDLS_FOR_MON_REGISTRATION)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Wrong Format of data received from component, hence returning. Message received: %s", mon_reg_req_ptr->message);
    return;
  }

  for(i = 0; i < total_monitor_list_entries; i++)
  {
    //Here we are comparing with gdf name only, but need to check for other elements too if same monitor is applied from another component running on same machine.
    if(strstr(monitor_list_ptr[i].gdf_name, fields[0]))
      break;

    //i += monitor_list_ptr[i].no_of_monitors;
  }

  if(i == total_monitor_list_entries)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Could not find gdf_name = %s, for component = %s in CM_info table. So going to add this monitor as DVM.", fields[0], fields[1]);  
     
    create_mapping_table_row_entries();

    if(strncmp(fields[0], "cm_nd_", 6) == 0)
      sprintf(server_name, "%s:%d", global_settings->net_diagnostics_server, global_settings->net_diagnostics_port);
    else
      sprintf(server_name, "Cavisson%c%s", global_settings->hierarchical_view_vector_separator,g_machine);
     
    sprintf(buff, "CUSTOM_MONITOR %s %s %s 2 cm_monitor_registration;%s", server_name, fields[0], fields[1], fields[1]);
    //ret = custom_config("CUSTOM_MONITOR", buff, server_name, 1, RUNTIME_DVM_TABLE, err_msg, NULL, 0, -1, breadcrumb, 0, NULL, NULL, NULL, NULL, 0, NULL, NULL, 0);
 //   sprintf(buff, "CUSTOM_MONITOR %s %s %s 2 %s",server_name,fields[0],server_name,fields[1]);
    ret = custom_monitor_config("CUSTOM_MONITOR", buff, server_name, 1, 1, err_msg, NULL, NULL, 0);
    NSDL2_MON(NULL, NULL, "MonitorRegistration:-> Buffer: %s, ret = %d", buff, ret);

    if(ret < 0)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error in adding Buffer: %s in monitor table. Error: %s", buff, err_msg);
      return;
    }
    else
      g_mon_id = get_next_mon_id();

    cm_table_ptr = monitor_list_ptr[i].cm_info_mon_conn_ptr;

    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "MonitorRegistration: Total_monitors = %d, cm_table_ptr = %p", i, cm_table_ptr);
    set_elements_for_cm_info(fd, cm_table_ptr);
    //remove_select_msg_com_con(fd);
    REMOVE_SELECT_MSG_COM_CON(fd, DATA_MODE);
    ADD_SELECT_MSG_COM_CON((char *)(monitor_list_ptr[i].cm_info_mon_conn_ptr), fd, EPOLLIN | EPOLLERR, DATA_MODE);
  }
  else
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Found existing entry at index = %d, fd = %d", i, fd);
    //Removing entry from epoll if already addded.
    if(monitor_list_ptr[i].cm_info_mon_conn_ptr->fd > 0)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Health Monitor (%s) already added in epoll with FD '%d', "
                        "and found new request for same monitor with Fd '%d'. So going to remove old entry (%p) "
                        "from epoll and adding new one.",
                         monitor_list_ptr[i].cm_info_mon_conn_ptr->gdf_name, monitor_list_ptr[i].cm_info_mon_conn_ptr->fd,
                         fd, monitor_list_ptr[i].cm_info_mon_conn_ptr);
      close(monitor_list_ptr[i].cm_info_mon_conn_ptr->fd);

      struct epoll_event pfd;
      bzero(&pfd, sizeof(struct epoll_event));

      if(epoll_ctl(g_msg_com_epfd, EPOLL_CTL_DEL, monitor_list_ptr[i].cm_info_mon_conn_ptr->fd, &pfd) < 0)
      {
        NSTL1_OUT(NULL, NULL, "EPOLL ERROR occured in parent process, handle_monitor_registration() - "
                              "with fd %d EPOLL_CTL_DEL: err = %s\n", monitor_list_ptr[i].cm_info_mon_conn_ptr->fd, nslb_strerror(errno));
      }
    }

    monitor_list_ptr[i].cm_info_mon_conn_ptr->fd = fd;
    //updating pointer for fd. Previously it was added in epoll with tool type ptr and we will add it with cm_info ptr.
    //remove_select_msg_com_con(fd);
    REMOVE_SELECT_MSG_COM_CON(fd, DATA_MODE);
    ADD_SELECT_MSG_COM_CON((char *)(monitor_list_ptr[i].cm_info_mon_conn_ptr), fd, EPOLLIN | EPOLLERR, DATA_MODE);
  }

  monitor_runtime_changes_applied = 1;
}

void apply_tier_server_count_monitor()
{
  char server_name[256], buff[1024];
  char err_msg[1024]="";

  sprintf(server_name, "Cavisson%c%s", global_settings->hierarchical_view_vector_separator, g_machine);
  
  sprintf(buff,"DYNAMIC_VECTOR_MONITOR %s Cavisson>NDAppliance>TierServerCount cm_hm_ns_tier_server_count.gdf 2 cm_monitor_registration EOC cm_monitor_registration", server_name);

  NSDL2_MON(NULL, NULL, "Monitor buffer: %s", buff);
  kw_set_dynamic_vector_monitor("DYNAMIC_VECTOR_MONITOR", buff, server_name, 0, 0, NULL, err_msg, NULL, NULL, 1);
}

void create_server_count_data_buf()
{
  if(g_tier_server_count_index == -1)
    return;
  int i;
  char buffer[1024];
  int length = 0;
  for(i=0; i<topo_info[topo_idx].total_tier_count; i++)
  {
    if(g_tier_index_send < i)
    {
      g_tier_index_send=i;
      sprintf(buffer,"%d:%s|%d", i, topo_info[topo_idx].topo_tier_info[i].tier_name, topo_info[topo_idx].topo_tier_info[i].active_servers); 
    }
    else
      sprintf(buffer,"%d|%d", i,topo_info[topo_idx].topo_tier_info[i].active_servers);
      
      NSDL2_MON(NULL, NULL, "Data buffer: %s", buffer);
   
    length = strlen(buffer) + 1;
    strcpy(monitor_list_ptr[g_tier_server_count_index].cm_info_mon_conn_ptr->data_buf, buffer);   //buffer is created above
    filldata(monitor_list_ptr[g_tier_server_count_index].cm_info_mon_conn_ptr, length);
  }
}

void mon_update_times_data(TimesData *time_data, Long_data value)
{
  if(time_data->min > value)
    time_data->min = value;

  if(time_data->max < value)
   time_data->max = value;

  time_data->total_size += value;
  time_data->count++;

  /*
  if(!time_data->total_size)
    time_data->avg = 0;
  else
    time_data->avg = time_data->total_size / time_data->count;
  */
}

