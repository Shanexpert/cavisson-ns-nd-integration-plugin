/******************************************************************
 * Name    : ns_monitor_init.c 
 * Author  : Himanshu Singhal
 * Purpose : This file contains methods related to parsing json files
 * Note:
 * Modification History:
 * 06/Aug/2020 - Initial Version
*****************************************************************/
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <string.h>
#include <ctype.h>
#include <curl/curl.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <regex.h>
#include <v1/topolib_structures.h>
#include <dirent.h>
#include <ns_monitor_init.h>
#include <v1/cav_bits_flags.h>

#include "ns_get_log_file_monitor.h"
#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "nslb_util.h"
#include "nslb_sock.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"
#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "util.h"
#include "ns_msg_def.h"
#include "ns_log.h"
#include "tmr.h"
#include "timing.h"
#include "ns_custom_monitor.h"
#include "ns_dynamic_vector_monitor.h"
#include "ns_schedule_phases.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "ns_alloc.h"
#include "wait_forever.h"
#include "nslb_cav_conf.h"
#include "ns_server_admin_utils.h"
#include "ns_user_monitor.h"
#include "ns_batch_jobs.h"
#include "ns_check_monitor.h"
#include "ns_event_log.h"
#include "ns_mon_log.h"
#include "ns_string.h"
#include "ns_event_id.h"
#include "init_cav.h"
#include "nslb_msg_com.h"
#include "ns_coherence_nid_table.h"
#include "ns_trace_level.h"
#include "ns_nv_tbl.h"
#include "ns_monitor_profiles.h"
#include "ns_monitor_2d_table.h"
#include "ns_standard_monitors.h"
#include "ns_ndc_outbound.h"
#include "ns_ndc.h"
#include "ns_gdf.h"
#include "ns_runtime_changes_monitor.h"
#include "ns_exit.h"
#include "ns_appliance_health_monitor.h"
#include "ns_monitor_metric_priority.h"
#include "ns_custom_monitor_RDnew.h"
#include "ns_lps.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"
#include "ns_parse_scen_conf.h"
#include "url.h"
#include "../../base/topology/topolib_v1/topolib_structures.h"

NormObjKey *g_monid_hash;   //used for normalized table of g_mon_id
int total_mon_config_list_entries;  //used to store the total no. of entries present in MonConfigList struct.
int max_mon_config_list_entries;

char *g_json_file_buf_ptr =NULL;    //this pointer is used to save the json file content
int g_size_of_json_file_buf;

// free ArrInfo struct
void free_arr_info_struct(int count, char **list)
{
  for(int i =0; i <count; ++i)
    FREE_AND_MAKE_NULL(list[i], "Monconfig Arrnfo list", i);
}

//free json_info struct elemnets
void free_json_info_struct(JSON_info *json_info_ptr)
{
  NSDL2_MON(NULL, NULL, "Method called free_json_info_struct");

  FREE_AND_MAKE_NULL(json_info_ptr->vectorReplaceFrom, "json_info_ptr->vectorReplaceFrom", 0);
  FREE_AND_MAKE_NULL(json_info_ptr->vectorReplaceTo, "json_info_ptr->vectorReplaceTo", 0);
  FREE_AND_MAKE_NULL(json_info_ptr->javaClassPath, "json_info_ptr->javaClassPath", 0);
  FREE_AND_MAKE_NULL(json_info_ptr->javaHome, "json_info_ptr->javaHome", 0);
  FREE_AND_MAKE_NULL(json_info_ptr->init_vector_file, "json_info_ptr->init_vector_file", 0);
  FREE_AND_MAKE_NULL(json_info_ptr->instance_name, "json_info_ptr->instance_name", 0);
  FREE_AND_MAKE_NULL(json_info_ptr->mon_name, "json_info_ptr->mon_name", 0);
  FREE_AND_MAKE_NULL(json_info_ptr->args, "json_info_ptr->args", 0);
  FREE_AND_MAKE_NULL(json_info_ptr->options, "json_info_ptr->args", 0); 
  FREE_AND_MAKE_NULL(json_info_ptr->old_options, "json_info_ptr->args", 0);
  FREE_AND_MAKE_NULL(json_info_ptr->config_json, "json_info_ptr->config_json", 0);
  FREE_AND_MAKE_NULL(json_info_ptr->os_type, "json_info_ptr->os_type", 0);
  FREE_AND_MAKE_NULL(json_info_ptr->app_name, "json_info_ptr->app_name", 0);
  FREE_AND_MAKE_NULL(json_info_ptr->pgm_type, "json_info_ptr->pgm_type", 0);
  FREE_AND_MAKE_NULL(json_info_ptr->use_agent, "json_info_ptr->use_agent", 0);
  FREE_AND_MAKE_NULL(json_info_ptr->g_mon_id, "json_info_ptr->g_mon_id", 0);
  FREE_AND_MAKE_NULL(json_info_ptr->std_mon_ptr, "json_info_ptr->std_mon_ptr", 0);

  for(int i = 0; i < json_info_ptr->no_of_dest_any_server_elements; i++)
    FREE_AND_MAKE_NULL(json_info_ptr->dest_any_server_arr[i], "json_info_ptr->dest_any_server_arr", 0);

  FREE_AND_MAKE_NULL(json_info_ptr, "json_info_ptr", 0);
}


// free the mon config structure
// also decrease the value of total_mon_config_list_entries by 1
// Here we also remove the entry from g_monid_hash table of mon config
void free_mon_config_structure(MonConfig *mon_config, int flag)
{
  //MonConfig *mon_config;
  int norm_id;
  //int mon_info_index=0;

  NSDL2_MON(NULL, NULL, "Method called free_mon_config_structure");
 
  NSDL1_MON(NULL, NULL, " Monitor Id '%s' deleting from g_monid_hash table of monitor config struct '%s'\n", mon_config->g_mon_id);
  //remove entry from hash table of MonConfig
  if(flag == 1)
  {
    nslb_delete_norm_id_ex(g_monid_hash, mon_config->g_mon_id, strlen(mon_config->g_mon_id), &norm_id);
    total_mon_config_list_entries --;
  }

  FREE_AND_MAKE_NULL(mon_config->vectorReplaceFrom, "Monconfig vectorReplaceFrom", mon_info_index);
  FREE_AND_MAKE_NULL(mon_config->vectorReplaceTo, "Monconfig vectorReplaceTo", mon_info_index);
  FREE_AND_MAKE_NULL(mon_config->javaClassPath, "Monconfig javaClassPath", mon_info_index);
  FREE_AND_MAKE_NULL(mon_config->javaHome, "Monconfig javaHome", mon_info_index);
  FREE_AND_MAKE_NULL(mon_config->init_vector_file, "Monconfig init_vector_file", mon_info_index);
  FREE_AND_MAKE_NULL(mon_config->instance_name, "Monconfig instance_name", mon_info_index);
  FREE_AND_MAKE_NULL(mon_config->mon_name, "Monconfig mon_name", mon_info_index);
  FREE_AND_MAKE_NULL(mon_config->options, "Monconfig args", mon_info_index);
  FREE_AND_MAKE_NULL(mon_config->old_options, "Monconfig args", mon_info_index);
  FREE_AND_MAKE_NULL(mon_config->config_json, "Monconfig config_json", mon_info_index);
  FREE_AND_MAKE_NULL(mon_config->os_type, "Monconfig os_type", mon_info_index);
  FREE_AND_MAKE_NULL(mon_config->app_name, "Monconfig app_name", mon_info_index);
  FREE_AND_MAKE_NULL(mon_config->pgm_type, "Monconfig pgm_type", mon_info_index);
  FREE_AND_MAKE_NULL(mon_config->use_agent, "Monconfig use_agent", mon_info_index);
  FREE_AND_MAKE_NULL(mon_config->g_mon_id, "Monconfig g_mon_id", mon_info_index);
  FREE_AND_MAKE_NULL(mon_config->gdf_name, "Monconfig gdf_name", mon_info_index);
  FREE_AND_MAKE_NULL(mon_config->cm_init_buffer, "Monconfig gdf_name", mon_info_index);

  if(mon_config->mon_info_flags & MINFO_FLAG_TIER_GROUP)
  {
    FREE_AND_MAKE_NULL(mon_config->tier_group, "Monconfig tier group", mon_info_index);
  }
  else
  {
    FREE_AND_MAKE_NULL(mon_config->tier_name, "Monconfig tier_name", mon_info_index);
  }

  if(mon_config->exclude_tier)
  {
    free_arr_info_struct(mon_config->exclude_tier[0].count, mon_config->exclude_tier[0].list);
    FREE_AND_MAKE_NULL(mon_config->exclude_tier, "Monconfig exclude tier", mon_info_index);
  }
  if(mon_config->exclude_server)
  {
    free_arr_info_struct(mon_config->exclude_server[0].count, mon_config->exclude_server[0].list);
    FREE_AND_MAKE_NULL(mon_config->exclude_server, "Monconfig exclude server", mon_info_index);
  }
  if(mon_config->specific_server)
  {
    free_arr_info_struct(mon_config->specific_server[0].count, mon_config->specific_server[0].list);
    FREE_AND_MAKE_NULL(mon_config->specific_server, "Monconfig specific server", mon_info_index);
  }
  if(mon_config->dest_tier_server)
  {
    free_arr_info_struct(mon_config->dest_tier_server[0].count, mon_config->dest_tier_server[0].list);
    FREE_AND_MAKE_NULL(mon_config->dest_tier_server, "Monconfig destination tier server", mon_info_index);
  }
  if(mon_config->kube_ip_appname)
  {
    free_arr_info_struct(mon_config->kube_ip_appname[0].count, mon_config->kube_ip_appname[0].list);
    FREE_AND_MAKE_NULL(mon_config->kube_ip_appname, "Monconfig kube ip names", mon_info_index);
  }

  if(mon_config->json_info_ptr)
    free_json_info_struct(mon_config->json_info_ptr);

  mon_config->mon_err_count = 0;
  mon_config->count = 0;
  mon_config->total_mon_id_index = 0;
  mon_config->max_mon_id_index = 0;  
  mon_config->sm_mode = 0;
  mon_config->run_opt = 0;
  mon_config->frequency = 0;
  mon_config->is_process = 0;
  mon_config->tier_server_mapping_type= 0;
  mon_config->agent_type = 0;              
  mon_config->metric_priority = 0;
  mon_config->skip_breadcrumb_creation = 0;  
  mon_config->mon_type = 0;

  FREE_AND_MAKE_NULL(mon_config->mon_id_struct, "Monconfig Arrnfo list", mon_info_index);
 
  nslb_mp_free_slot(&mon_config_pool, mon_config);
}

// This function will delete the entry from specific server hash tbale entry
void mj_delete_specific_server_hash_entry(CM_info *cus_mon_ptr)
{
  char tmp_buf[MAX_DATA_LINE_LENGTH];
  int norm_id;

  //This is the case when ENABLE_AUTO_JSON_MONITOR mode 2 is on.
  if(total_mon_config_list_entries > 0)
  {
    if(cus_mon_ptr->instance_name == NULL)
      sprintf(tmp_buf, "%s%c%s%c%s", cus_mon_ptr->gdf_name_only, global_settings->hierarchical_view_vector_separator, cus_mon_ptr->tier_name,
                                     global_settings->hierarchical_view_vector_separator, cus_mon_ptr->g_mon_id);
    else
      sprintf(tmp_buf, "%s%c%s%c%s%c%s",cus_mon_ptr->gdf_name_only, global_settings->hierarchical_view_vector_separator,
                                        cus_mon_ptr->instance_name, global_settings->hierarchical_view_vector_separator, cus_mon_ptr->tier_name,                                        global_settings->hierarchical_view_vector_separator, cus_mon_ptr->g_mon_id);
  }
  else
  {
    if(cus_mon_ptr->instance_name == NULL)
      sprintf(tmp_buf, "%s%c%s", cus_mon_ptr->gdf_name_only, global_settings->hierarchical_view_vector_separator, cus_mon_ptr->tier_name);
    else
      sprintf(tmp_buf, "%s%c%s%c%s", cus_mon_ptr->gdf_name_only, global_settings->hierarchical_view_vector_separator,
                                     cus_mon_ptr->instance_name, global_settings->hierarchical_view_vector_separator, cus_mon_ptr->tier_name);
  }

  nslb_delete_norm_id_ex(specific_server_id_key, tmp_buf, strlen(tmp_buf), &norm_id);

  NSDL1_MON(NULL, NULL, "Monitor tmp_buf '%s' deleted from hash table for server '%s'\n", tmp_buf, cus_mon_ptr->server_name);
}

// This function is call to delete the monitors
int mj_runtime_process_deleted_monitors(int mon_info_index, int server_index)
{
  CM_info *cus_mon_ptr = NULL;
  MonConfig *mon_config;

  int mon_id, id;
  int index;  //this index is of cm_ptr

  NSDL2_MON(NULL, NULL, "Method called mj_runtime_process_deleted_monitors");

  mon_config = mon_config_list_ptr[mon_info_index].mon_config;

  for(id=0; id<mon_config->total_mon_id_index; id++)
  {
    mon_id = mon_config->mon_id_struct[id].mon_id; 
    if(mon_id >= 0 && mon_id <= max_mon_id_entries)
    {
      index = mon_id_map_table[mon_id].mon_index;
      if(index >=0 && index <= total_monitor_list_entries)
      {
        if(mon_config->agent_type & CONNECT_TO_NDC)
        {
          make_and_send_del_msg_to_NDC(&mon_config->tier_name, 1, NULL, 0, mon_config->specific_server[0].list,
                                       mon_config->specific_server[0].count, mon_config->exclude_server[0].list, 
                                       mon_config->exclude_server[0].count, -1, mon_config->instance_name, mon_config->gdf_name);
        }
        else
        {
          cus_mon_ptr = monitor_list_ptr[index].cm_info_mon_conn_ptr;
          if(server_index >= 0)
          {
            if(cus_mon_ptr->server_index == server_index)
            {
              stop_one_custom_monitor(cus_mon_ptr, MON_DELETE_ON_REQUEST);
              break;
            }
          }
          else
            stop_one_custom_monitor(cus_mon_ptr, MON_DELETE_ON_REQUEST);
        }
      }
      else
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, "Invalid mon_index %d on mon_id %d.Hence continue",
                                                                                 index, mon_id);
      }
    }
    else
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                        "Either Invalid mon_id %d or mon_id greater than max_mon_id_entries %d ,Hence continue ", mon_id, max_mon_id_entries);
    }
  }

  if(server_index < 0) 
    free_mon_config_structure(mon_config_list_ptr[mon_info_index].mon_config, 1);

  return 1;
}


// This function will call search_replace_chars function which is used to manipulate the programs args
// also this func will fill pod_name
// make the special instance name for specific monitor.
void mj_replace_char_and_update_pod_name(char *vector_name, char *prgrm_args, char *pod_name, char *app_name, char *monitor_name,
                                          char *tmp_instance_name)
{
  char *field[10];

  char temp_buff[MAX_DATA_LINE_LENGTH];
  char mon_name[MAX_DATA_LINE_LENGTH];
  char namespace[MAX_DATA_LINE_LENGTH];
  char out_buf[16*MAX_DATA_LINE_LENGTH];
  char seq[]="%25";

  int num_field;
  int ret_value;

  kub_ip_mon_vector_fields *vector_fields_ptr = NULL;

  strcpy(temp_buff, vector_name);
  strcpy(mon_name, monitor_name);

  num_field = parse_kubernetes_vector_format(temp_buff, field);
  if(num_field < 0)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                       "Received Invalid vector name. vector name = %s", vector_name);
    return;
  }
  /*
  function name: copy_vector_data_in_string
  Description: This function is used to copy field array entrires to input array.
  Which consist node_ip, node_name, pod_ip, pod_name, container_name.
  */
  vector_fields_ptr = copy_vector_data_in_string(field);
  strcpy(pod_name, field[5]);
  if(num_field == 8)
  {
    strcpy(namespace, field[7]);
  }

  ret_value = search_replace_chars(&prgrm_args, seq, out_buf, vector_fields_ptr);

  MLTL3(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "dummy_ptr after function call is = %s",prgrm_args);

  MLTL3(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              " ret value after function \"search_replace_chars\" is  = %d", ret_value);

  if(ret_value == 0)
  {
    MLTL3(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                "dummy_ptr, if no changes are their in dummy_ptr= %s",prgrm_args);
    return;
  }

  if((!strcmp(mon_name, "RedisSlaveStatsEx")) || (!strcmp(mon_name, "RedisActivityStatsV2Ex")) || (!strcmp(mon_name, "RedisCacheStatsV2Ex")) ||
     (!strcmp(mon_name, "RedisperformanceStatsV2Ex")) || (!strcmp(mon_name, "RedisSystemStatsEx")))
  {
    if(namespace[0] != '\0')
      sprintf(tmp_instance_name, "%s%c%s", namespace, global_settings->hierarchical_view_vector_separator, pod_name);
    else
      sprintf(tmp_instance_name, "default%c%s", global_settings->hierarchical_view_vector_separator, pod_name);
  }
  else
    sprintf(tmp_instance_name, "%s", pod_name);

  return;
}

// This function is used for rellocation of bit vector
int mj_allocate_memory_to_bit_magic_api(bit_vector_t *mon_bv, int *max_bit_vectors)
{
  NSDL2_MON(NULL, NULL, "Method called mj_allocate_memory_to_bit_magic_api");

  if(*max_bit_vectors == 0)
  {
    *mon_bv = nslb_get_bv(DELTA_BIT_VECTOR_ENTRIES);    //give memory to monitor bits
    *max_bit_vectors += DELTA_BIT_VECTOR_ENTRIES;
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                                                        "max_bit_vectors %d",*max_bit_vectors);
  }
  else if(total_mon_config_list_entries >= *max_bit_vectors)
  {
     *max_bit_vectors += DELTA_BIT_VECTOR_ENTRIES;     //DELTA_BIT_VECTOR_ENTRIES 100 
     nslb_bv_expand(mon_bv, *max_bit_vectors);         // if all the bits are used then expand the bit with more bits
     MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                                                        "max_bit_vectors %d",*max_bit_vectors);
  }

  return 0;
}


// This function will call when ENABLE_AUTO_JSON_MONITOR mode 2 is on.
void mj_apply_monitors_on_autoscaled_server(char *vector_name, int server_id, int tier_id, char *app_name)
{
  MonConfig *mon_config;
  int total_no_bit_sets = 0;
  int bit_pos;
  int mon_info_index = -1;
  int apply_monitor_flag =0;

  //Loop through monitor id’s to be applied on tiers (use bit magic function)
  // nslb_rank func returns the total no of bit set in bit vector
  if(!topo_info[topo_idx].topo_tier_info[tier_id].mon_bv)    //This is the case when tier is not autoscaled at start of test 
  {
    for(int mon_id=0; mon_id<total_mon_config_list_entries; mon_id++)
    {
      mon_config = mon_config_list_ptr[mon_id].mon_config;
      if(mon_config && mon_config->tier_name && 
        (strcmp(mon_config->tier_name, topo_info[topo_idx].topo_tier_info[tier_id].tier_name) == 0))
      {
        mj_allocate_memory_to_bit_magic_api(&(topo_info[topo_idx].topo_tier_info[tier_id].mon_bv), 
                                                   &(topo_info[topo_idx].topo_tier_info[tier_id].max_bit_vectors));
        
        nslb_bit_set(&(topo_info[topo_idx].topo_tier_info[tier_id].mon_bv), mon_id);                 //set the bit of mon_info_index index
        mon_config->tier_idx = tier_id;
        apply_monitor_flag =1;
      }
    }
    if(apply_monitor_flag)
    {
      total_no_bit_sets = nslb_rank(topo_info[topo_idx].topo_tier_info[tier_id].mon_bv,
                                                          topo_info[topo_idx].topo_tier_info[tier_id].max_bit_vectors -1);

      for(bit_pos=1; bit_pos<=total_no_bit_sets; bit_pos++)
      {
        mon_info_index = nslb_select(topo_info[topo_idx].topo_tier_info[tier_id].mon_bv, bit_pos);  //nslb_select return the pos of set bits
        apply_monitor(vector_name, server_id, tier_id, mon_info_index, app_name, 1);
      }
    }
  }
  else
  {
    total_no_bit_sets = nslb_rank(topo_info[topo_idx].topo_tier_info[tier_id].mon_bv, 
                                                               topo_info[topo_idx].topo_tier_info[tier_id].max_bit_vectors);
    for(bit_pos=1; bit_pos<=total_no_bit_sets; bit_pos++)
    {  
      mon_info_index = nslb_select(topo_info[topo_idx].topo_tier_info[tier_id].mon_bv, bit_pos);  //nslb_select return the pos of set bits
      apply_monitor(vector_name, server_id, tier_id, mon_info_index, app_name, 1);
    }
  }
  return;
}


//this function will call when ENABLE_JSON_MONITOR mode 2 in this we increase the server count by 1
void mj_increase_server_count(int mon_info_index)
{
  int id;
  MonConfig *mon_config;
  mon_config = mon_config_list_ptr[mon_info_index].mon_config;

  if(mon_config->total_mon_id_index >= mon_config->max_mon_id_index)
  {
    MY_REALLOC_AND_MEMSET(mon_config->mon_id_struct,((mon_config->max_mon_id_index + DELTA_BIT_VECTOR_ENTRIES) * sizeof(MonId)),
                          (mon_config->max_mon_id_index * sizeof(MonId)), "Reallocation of mon_id_struct array", -1);
    mon_config->max_mon_id_index += DELTA_BIT_VECTOR_ENTRIES;
  }
  //also store the g_mon_id in mon_id_struct array
  id =mon_config->total_mon_id_index;
  
  mon_config->mon_id_struct[id].mon_id = g_mon_id;
  mon_config->mon_id_struct[id].status = MJ_QUEUED;
  mon_config->total_mon_id_index +=1;
  mon_config->count += 1;

}


/*void update_mon_config_status_to_request_sent(int mon_info_index, int mon_id)
{
  MonConfig *mon_config;

  int id;

  mon_config = mon_config_list_ptr[mon_info_index].mon_config;
  for(id =0; id < mon_config->max_mon_id_index; id++)
  {
    //and find out the mon_id
    if(mon_id == mon_config->mon_id_struct[id].mon_id)
    {
      mon_config->mon_id_struct[id].status == MJ_REQUEST_SENT;
      break;
    }
  }
  return;
}*/

//this function will call when ENABLE_JSON_MONITOR mode 2 in this we decrease the server count by 1
//TODO: also decrease the 
void mj_decrease_server_count(int mon_info_index, int mon_id,int tid)
{
  MonConfig *mon_config;

  int id;
  int tier_group_idx;
   
  mon_config = mon_config_list_ptr[mon_info_index].mon_config;
  for(id =0; id < mon_config->total_mon_id_index; id++)
  {
    //and find out the mon_id
    if(mon_id == mon_config->mon_id_struct[id].mon_id)
    {
      mon_config->count -= 1;
      if(mon_config->mon_id_struct[id].status == MJ_FAILURE && mon_config->mon_err_count >0)
      {
        mon_config->mon_err_count -= 1;
      }
      if(mon_config->mon_info_flags & MINFO_FLAG_TIER_GROUP)  //This means tier group is present
      {
        tier_group_idx =mon_config->tier_group_idx;
        for(int i=0; i<topo_info[topo_idx].topo_tier_group[tier_group_idx].total_list_count; i++)
        {
          //here we are saving tier_id of tier
          tid =topolib_get_tier_id_from_tier_name(topo_info[topo_idx].topo_tier_group[tier_group_idx].tierList[i],topo_idx);
          if( tid == -1 )
          {
            MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
                               "Tier [%s] does not exist in topology so skipping monitor application for this tier.",
                                topo_info[topo_idx].topo_tier_group[tier_group_idx].tierList[i]);
            continue;
          }
          else
            nslb_bit_clr(&(topo_info[topo_idx].topo_tier_info[tid].mon_bv), mon_info_index);   //unset the bit of mon_info_index index
        }  
      }
      else
        nslb_bit_clr(&(topo_info[topo_idx].topo_tier_info[mon_config->tier_idx].mon_bv), mon_info_index);   //unset the bit of mon_info_index index

      mon_config->mon_id_struct[id].mon_id = -1;
      mon_config->mon_id_struct[id].status = 0;
      mon_config->mon_id_struct[id].message[0] = '\0';
      //mon_config->total_mon_id_index -= 1;        
      return;
    }
  }
  return;
}

//copy source json info ptr to destination src_json_info_ptr
void mj_fill_json_info_ptr_struct(JSON_info *dest_json_info_ptr, MonConfig *mon_config, int mon_info_index)
{
  NSDL2_MON(NULL, NULL, "Method called mj_fill_json_info_ptr_struct");
  if(mon_config->vectorReplaceTo)
  {
    MALLOC_AND_COPY(mon_config->vectorReplaceTo ,dest_json_info_ptr->vectorReplaceTo,
                                       (strlen(mon_config->vectorReplaceTo) + 1), "JSON Info Ptr VectorReplaceTo", -1);
  }

  if(mon_config->vectorReplaceFrom)
  {
    MALLOC_AND_COPY(mon_config->vectorReplaceFrom ,dest_json_info_ptr->vectorReplaceFrom,
                                       (strlen(mon_config->vectorReplaceFrom)+1), "JSON Info Ptr VectorReplaceFrom", -1);
  }

  if(mon_config->javaHome)
  {
    MALLOC_AND_COPY(mon_config->javaHome ,dest_json_info_ptr->javaHome, (strlen(mon_config->javaHome) + 1),
                                       "JSON Info Ptr JavaHome", -1);
  }

  if(mon_config->javaClassPath)
  {
    MALLOC_AND_COPY(mon_config->javaClassPath ,dest_json_info_ptr->javaClassPath, (strlen(mon_config->javaClassPath) + 1),
                                       "JSON Info Ptr javaClassPath", -1);
  }

  if(mon_config->init_vector_file)
  {
    MALLOC_AND_COPY(mon_config->init_vector_file, dest_json_info_ptr->init_vector_file,
                                       (strlen(mon_config->init_vector_file) +1), "JSON Info Ptr init vector file", -1);
  }

  if(mon_config->config_json)
  {
    MALLOC_AND_COPY(mon_config->config_json, dest_json_info_ptr->config_json, strlen(mon_config->config_json) + 1,
                                       "JSON Info config json", -1);
  }

  if(mon_config->mon_name)
  {
    MALLOC_AND_COPY(mon_config->mon_name, dest_json_info_ptr->mon_name, strlen(mon_config->mon_name) + 1,
                                       "JSON Info config json", -1);
  }

  if(mon_config->os_type)
  {
    MALLOC_AND_COPY(mon_config->os_type, dest_json_info_ptr->os_type, strlen(mon_config->os_type) + 1,
                                        "JSON INFO ptr os_type", -1);
  }

  if(mon_config->pgm_type)
  {
    MALLOC_AND_COPY(mon_config->pgm_type, dest_json_info_ptr->pgm_type, strlen(mon_config->pgm_type) + 1,
                                        "JSON INFO ptr pgm_type", -1);
  }

  if(mon_config->options)
  {
    MALLOC_AND_COPY(mon_config->options, dest_json_info_ptr->options, strlen(mon_config->options) + 1,
                                        "JSON INFO ptr args", -1);
  }

  if(mon_config->old_options)
  {
    MALLOC_AND_COPY(mon_config->old_options, dest_json_info_ptr->old_options, strlen(mon_config->old_options) + 1,
                                        "JSON INFO ptr args", -1);
  }

  if(mon_config->app_name)
  {
    MALLOC_AND_COPY(mon_config->app_name, dest_json_info_ptr->app_name, strlen(mon_config->app_name) + 1,
                                        "JSON INFO ptr app_name", -1);
  }

  if(mon_config->use_agent)
  {
    MALLOC_AND_COPY(mon_config->use_agent, dest_json_info_ptr->use_agent, strlen(mon_config->use_agent) + 1,
                                        "JSON INFO ptr use_agent", -1);
  }

  if(mon_config->g_mon_id)
  {
    MALLOC_AND_COPY(mon_config->g_mon_id, dest_json_info_ptr->g_mon_id, strlen(mon_config->g_mon_id) + 1,
                                        "JSON INFO ptr g_mon_id", -1);
  }

  dest_json_info_ptr->run_opt = mon_config->run_opt;
  dest_json_info_ptr->tier_server_mapping_type = mon_config->tier_server_mapping_type;
  dest_json_info_ptr->metric_priority = mon_config->metric_priority;
  dest_json_info_ptr->agent_type = mon_config->agent_type;
  dest_json_info_ptr->mon_info_index = mon_info_index;
  dest_json_info_ptr->sm_mode = mon_config->sm_mode;
  dest_json_info_ptr->mon_type = mon_config->mon_type;
  dest_json_info_ptr->is_process = mon_config->is_process;

  if(mon_config->mon_info_flags & MINFO_FLAG_ANY_SERVER)
    dest_json_info_ptr->any_server = true;

  if(mon_config->mon_info_flags & MINFO_FLAG_DEST_TIER_ANY_SERVER)
  {
    dest_json_info_ptr->dest_any_server_flag = true; //for destination tier
    dest_json_info_ptr->no_of_dest_any_server_elements = mon_config->dest_tier_server[0].count;

    for(int i = 0; i < mon_config->dest_tier_server[0].count; i++)
      dest_json_info_ptr->dest_any_server_arr[i] = mon_config->dest_tier_server[0].list[i];
  }
}



// This function will fill and allocate monitor of ArrInfo structure 
void fill_arr_info_struct(char *data, ArrInfo *arr_info)
{
  char *element_token_arr[MAX_NO_OF_APP];
  int total_no_of_element;
  int var;

  NSDL2_MON(NULL, NULL, "Method called fill_arr_info_struct");
  total_no_of_element = get_tokens(data, element_token_arr, ",", MAX_NO_OF_APP);
  MY_MALLOC(arr_info[0].list,((total_no_of_element+1) * sizeof(char *)), "Allocating memory for ArrInfo struct list of MonConfig", -1);

  for(var=0; var<total_no_of_element; var++)
  { 
    MALLOC_AND_COPY(element_token_arr[var], arr_info[0].list[var], (strlen(element_token_arr[var]) + 1),
                                          "Copying Element token array in MonConfig ArrInfo Struct", -1);
  }
  arr_info[0].count = total_no_of_element;
}


// This function used to fill the json_info structure and other varibles which is used in create_mon_buf func
int apply_monitor(char *vector_name, int server_idx, int tier_id, int mon_info_index, char *app_name, int runtime_flag)
{
  MonConfig *mon_config =NULL;

  NSDL2_MON(NULL, NULL, "Method called mj_create_mon_buf");

  mon_config = mon_config_list_ptr[mon_info_index].mon_config;
  
  //This is case of MBEAN monitors
  if(mon_config->json_info_ptr && (mon_config->json_info_ptr->agent_type & CONNECT_TO_NDC))
  { 
    char *tier_name_arr[1];
    tier_name_arr[0] = mon_config->tier_name; 
    add_entry_for_mbean_mon(mon_config->mon_name, tier_name_arr, 1, NULL, 0, mon_config->specific_server[0].list,
                            mon_config->specific_server[0].count, mon_config->exclude_server[0].list, mon_config->exclude_server[0].count,
                            -1, mon_config->gdf_name, runtime_flag);
    mbean_mon_rtc_applied = 1;
    return 1;
  }
  
  create_mon_buf(mon_config->exclude_server[0].list, mon_config->exclude_server[0].count, mon_config->specific_server[0].list,
                 mon_config->specific_server[0].count, mon_config->mon_name, mon_config->options, mon_config->instance_name,
                 mon_config->json_info_ptr, runtime_flag, mon_config->mon_type, tier_id, mon_config->gdf_name, NULL, server_idx, vector_name,
                 app_name);
  return 1;
}


// This function will used to call apply monitor for each tier_id
void mj_add_monitors_from_json(int mon_info_index, int runtime_flag)
{
  MonConfig *mon_config;

  int tier_id, ret=0;
  int index;

  mon_config = mon_config_list_ptr[mon_info_index].mon_config;

  if(mon_config->mon_info_flags & MINFO_FLAG_TIER_GROUP)  //This means tier group is present
  {
    tier_id = mon_config->tier_group_idx;
    for(int i=0; i<topo_info[topo_idx].topo_tier_group[tier_id].total_list_count; i++)
    {
      //here we are saving tier_id of tier
      index = topolib_get_tier_id_from_tier_name(topo_info[topo_idx].topo_tier_group[tier_id].tierList[i],topo_idx);
      if(tier_id == -1)
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
                               "Tier [%s] does not exist in topology so skipping monitor application for this tier.",
                                                  topo_info[topo_idx].topo_tier_group[tier_id].tierList[i]);
        continue;
      }
      if(mon_config->exclude_tier[0].count)
      {
        ret =check_if_tier_excluded(mon_config->exclude_tier[0].count, mon_config->exclude_tier[0].list, topo_info[topo_idx].topo_tier_info[index].tier_name);

        if(ret == 1)
        {
          ret = 0;
          continue;
        }
      }
      // Here if appname is present different from default and monitor is applied at the time of runtime then we apply kubernetes monitors
      // according to the vectors of NA_KUBER i.e add_delete_kubernetes_monitor_on_process_diff
      if(runtime_flag == 1 && mon_config->mon_info_flags & MINFO_FLAG_APP_NAME)
        add_delete_kubernetes_monitor_on_process_diff(mon_config->app_name, topo_info[topo_idx].topo_tier_info[index].tier_name, mon_config->mon_name, 0); 
      else
        apply_monitor(NULL, -1, index, mon_info_index, NULL, runtime_flag);
    }
  }
  else
  {
    // also we have to handle the AllTier
    if(!strcasecmp(mon_config->tier_name, "AllTier"))
    {
      for(tier_id=0; tier_id < topo_info[topo_idx].total_tier_count; tier_id++)
      {
        if(mon_config->exclude_tier[0].count)
        {
          ret = check_if_tier_excluded(mon_config->exclude_tier[0].count, mon_config->exclude_tier[0].list, 
                                                                      topo_info[topo_idx].topo_tier_info[tier_id].tier_name);
          if(ret == 1)
          {
            ret = 0;
            continue;
          }
        }
        // Here if appname is present different from default and monitor is applied at the time of runtime then we apply kubernetes monitors
        // according to the vectors of NA_KUBER i.e add_delete_kubernetes_monitor_on_process_diff
        if(runtime_flag == 1 && mon_config->mon_info_flags & MINFO_FLAG_APP_NAME)
          add_delete_kubernetes_monitor_on_process_diff(mon_config->app_name, 
                                      topo_info[topo_idx].topo_tier_info[tier_id].tier_name, mon_config->mon_name, 0);
        else
          apply_monitor(NULL, -1, tier_id, mon_info_index, NULL, runtime_flag);
      }
    }
    else
    {
      tier_id = mon_config->tier_idx;
      if(tier_id == -1)
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"Failed to apply json auto monitor on %s tier.This tier is not present in topology", mon_config->tier_name);
      }
      else
      {
        // Here if appname is present different from default and monitor is applied at the time of runtime then we apply kubernetes monitors
        // according to the vectors of NA_KUBER i.e add_delete_kubernetes_monitor_on_process_diff
        if(runtime_flag == 1 && mon_config->mon_info_flags & MINFO_FLAG_APP_NAME)
          add_delete_kubernetes_monitor_on_process_diff(mon_config->app_name, mon_config->tier_name, mon_config->mon_name, 0);
        else
          apply_monitor(NULL, -1, tier_id, mon_info_index, NULL, runtime_flag);
      }
    }
  }
  return;
}

inline void make_mon_config_cm_init_buffer(MonConfig *mon_config)
{
  char buffer[MAX_DATA_LINE_LENGTH];
  int buf_len = 0;

  buf_len =sprintf(buffer, "cm_init_monitor:G_MON_ID=%s;MON_FREQUENCY=%d;IS_PROCESS=%d;MON_OPTION=%d;MON_TEST_RUN=%d;MON_VECTOR_SEPARATOR=%c;"
                           "MON_NS_WDIR=%s;MON_NS_VER=%s;MON_PARTITION_IDX=%lld;NUM_TX=%d;",
                           mon_config->g_mon_id, mon_config->frequency, mon_config->is_process, mon_config->run_opt, testidx,
                           global_settings->hierarchical_view_vector_separator, g_ns_wdir, ns_version, g_start_partition_idx, total_tx_entries);

  MALLOC_AND_COPY(buffer, mon_config->cm_init_buffer, (buf_len + 1), "MonConfig cm_init buffer", -1);
}


// This function will copy arr_ptr list in list name and also set flag is type
void copy_elements(char *buffer, StringWithType **list, int *total_entries, char type, NormObjKey *g_hash_table, int server_index,
                   int *max_entries)
{ 
  int norm_id = -1;
  int new_flag;
  int index, row;
 
  norm_id = nslb_get_norm_id(g_hash_table, buffer, strlen(buffer));
  if(norm_id == -2)
  {
    index = nslb_get_set_or_gen_norm_id_ex(g_hash_table, buffer, strlen(buffer), &new_flag);
    //reallocation of monitor config structure
    if(create_table_entry_ex(&row, total_entries, max_entries,
                               (char **)&(*list), sizeof(StringWithType), "Allocating MonConfig Structure Table") == -1)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION, "Could not create table entry for StringWithType");
      return;
    }
    strcpy((*list)[index].name, buffer);
    (*list)[index].type = type;
    (*list)[index].idx = server_index;
    *total_entries = *total_entries + 1;
  }
  else
  {
    if(type != (*list)[norm_id].type)
      (*list)[norm_id].type = NONE;
  }
  return;
}

// This function will check the entry in hash tables
void check_entry_in_hash_table(ArrInfo *arr_ptr, StringWithType **list, int *total_entries, char type, NormObjKey *g_hash_table, int tier_idx,
                               int *max_entries)
{
  int i, j;

  if(arr_ptr != NULL)
  {
    for(i=0; i< arr_ptr->count; i++)
    {
      if(tier_idx >= 0)
      {
        for(j=0; j <topo_info[topo_idx].topo_tier_info[tier_idx].total_server; j++)
        {
          if(topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[j].used_row != -1 && strcmp(topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[j].server_disp_name,arr_ptr->list[i]) == 0)
          {
            copy_elements(arr_ptr->list[i], list, total_entries, type, g_hash_table, j, max_entries);
            break;
          }

        }
      }
      else
        copy_elements(arr_ptr->list[i], list, total_entries, type, g_hash_table, j , max_entries);
    }
  }
  else
  {
    for(i=0; i< topo_info[topo_idx].topo_tier_info[tier_idx].total_server; i++)
    {
      if(type == INCLUDE_ANY || type == EXCLUDE_ANY)
      {  
        if((topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[i].used_row != -1) && (topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[i].server_ptr->status == 1))
        {
          copy_elements(topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[i].server_disp_name, list, total_entries, type, g_hash_table, i, max_entries);
          break;
        }
      }
      else
        copy_elements(topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[i].server_disp_name, list, total_entries, type, g_hash_table, i, max_entries);
    }
    
  }
  return;
}

// This function will check the further tags in mon config
// This function will process modify json tag and apply monitor same monitor again with different configuration
int mj_apply_monitor_of_modified_json(MonConfig *mon_config_new_ptr, int mon_info_index, int runtime_flag)
{
  MonConfig *mon_config_old_ptr = NULL;

  NormObjKey *g_server_hash_table;
  StringWithType *server_list = NULL;
  int total_servers = 0;
  
  NormObjKey *g_tier_hash_table;
  StringWithType *tier_list = NULL;
  int total_tiers = 0;
 
  int max_entries;
 
  mon_config_old_ptr = mon_config_list_ptr[mon_info_index].mon_config;

  if(mon_config_new_ptr->mon_info_flags & MINFO_FLAG_EXCLUDE_TIER || mon_config_old_ptr->mon_info_flags & MINFO_FLAG_EXCLUDE_TIER)
  {
    //here we make tier name normalized table  
    MY_MALLOC(g_tier_hash_table, (1024 * sizeof(NormObjKey)), "Memory allocation to Norm g_tier_hash_table", -1);
    nslb_init_norm_id_table(g_tier_hash_table, 1024);

    //old -> T1 T2   new - T3 T1
    //for exclude-tier tag
    if(mon_config_new_ptr->mon_info_flags & MINFO_FLAG_EXCLUDE_TIER)
      //Copy new_tiers in tier_list with EXCLUDE flag
      check_entry_in_hash_table(mon_config_new_ptr->exclude_tier, &tier_list, &total_tiers, EXCLUDE, g_tier_hash_table, -1, &max_entries);

    if(mon_config_old_ptr->mon_info_flags & MINFO_FLAG_EXCLUDE_TIER)
      //Copy old_tier in tier_list with INCLUDED flag
      check_entry_in_hash_table(mon_config_old_ptr->exclude_tier, &tier_list, &total_tiers, INCLUDE, g_tier_hash_table, -1, &max_entries);
    
    //make unique element in both array 
    for(int i=0; i<total_tiers; i++)
    {
      if(tier_list[i].type == INCLUDE)
        mj_add_monitors_from_json(mon_info_index, 1);
      else if(tier_list[i].type == EXCLUDE)
        mj_runtime_process_deleted_monitors(mon_info_index, -1);
    }
  
    free_mon_config_structure(mon_config_old_ptr, 0);
    mon_config_list_ptr[mon_info_index].mon_config = mon_config_new_ptr;

    nslb_obj_hash_destroy(g_tier_hash_table);
    return SUCCESS;
  }

  //here we make server name normalized table  
  MY_MALLOC(g_server_hash_table, (1024 * sizeof(NormObjKey)), "Memory allocation to Norm g_server_hash_table", -1);
  nslb_init_norm_id_table(g_server_hash_table, 1024);

  //for exclude-server tag
  if(mon_config_new_ptr->mon_info_flags & MINFO_FLAG_EXCLUDE_SERVER)
    //Copy new_servers in server_list with EXCLUDE flag
    check_entry_in_hash_table(mon_config_new_ptr->exclude_server, &server_list, &total_servers, EXCLUDE, g_server_hash_table,
                              mon_config_new_ptr->tier_idx, &max_entries);

  if(mon_config_old_ptr->mon_info_flags & MINFO_FLAG_EXCLUDE_SERVER)
    //Copy old_servers in server_list with added flag
    check_entry_in_hash_table(mon_config_old_ptr->exclude_server, &server_list, &total_servers, INCLUDE, g_server_hash_table,
                              mon_config_new_ptr->tier_idx, &max_entries);
  
  //for specific-server tag
  if(mon_config_new_ptr->mon_info_flags & MINFO_FLAG_SPECIFIC_SERVER && mon_config_old_ptr->mon_info_flags & MINFO_FLAG_SPECIFIC_SERVER)
  {
    check_entry_in_hash_table(mon_config_new_ptr->specific_server, &server_list, &total_servers, INCLUDE, g_server_hash_table,
                              mon_config_new_ptr->tier_idx, &max_entries);
    check_entry_in_hash_table(mon_config_old_ptr->specific_server, &server_list, &total_servers, EXCLUDE, g_server_hash_table,
                              mon_config_new_ptr->tier_idx, &max_entries);
  }
  else if(mon_config_new_ptr->mon_info_flags & MINFO_FLAG_SPECIFIC_SERVER)
  {
    //Copy new_servers in server_list with unused flag and copy previous servers with deleted flag expect new one.
    check_entry_in_hash_table(mon_config_new_ptr->specific_server, &server_list, &total_servers, INCLUDE, g_server_hash_table,
                              mon_config_new_ptr->tier_idx, &max_entries);
    //also handle any server also.
    if(mon_config_old_ptr->mon_info_flags & MINFO_FLAG_ANY_SERVER)
      //copy any server with EXCLUDE flag
      check_entry_in_hash_table(NULL, &server_list, &total_servers, EXCLUDE_ANY, g_server_hash_table, mon_config_old_ptr->tier_idx,
                                &max_entries);
    else
      //copy all server of tier with EXCLUDE flag
      check_entry_in_hash_table(NULL, &server_list, &total_servers, EXCLUDE, g_server_hash_table, mon_config_old_ptr->tier_idx, &max_entries);
  }
  else if(mon_config_old_ptr->mon_info_flags & MINFO_FLAG_SPECIFIC_SERVER)
  {
    //Add old servers in server_list with unused flag and add all server of new servers in server_list with added flag except old one.
    check_entry_in_hash_table(mon_config_old_ptr->specific_server, &server_list, &total_servers, EXCLUDE, g_server_hash_table,
                              mon_config_old_ptr->tier_idx, &max_entries);
    //also handle any server also.
    if(mon_config_new_ptr->mon_info_flags & MINFO_FLAG_ANY_SERVER)
      //copy any server with INCLUDE flag
      check_entry_in_hash_table(NULL, &server_list, &total_servers, INCLUDE_ANY, g_server_hash_table, mon_config_new_ptr->tier_idx,
                                &max_entries);
    else
      //copy all server of tier with INCLUDE flag
      check_entry_in_hash_table(NULL, &server_list, &total_servers, INCLUDE, g_server_hash_table, mon_config_new_ptr->tier_idx, &max_entries);
  }
  else if(mon_config_new_ptr->mon_info_flags & MINFO_FLAG_ANY_SERVER && !((mon_config_old_ptr->mon_info_flags & MINFO_FLAG_ANY_SERVER)))
  {
    //mark EXCLUDE server except first active server.
    check_entry_in_hash_table(NULL, &server_list, &total_servers, EXCLUDE, g_server_hash_table, mon_config_old_ptr->tier_idx, &max_entries);
    check_entry_in_hash_table(NULL, &server_list, &total_servers, INCLUDE_ANY, g_server_hash_table, mon_config_old_ptr->tier_idx, &max_entries);
  }
  else if(mon_config_old_ptr->mon_info_flags & MINFO_FLAG_ANY_SERVER && !((mon_config_new_ptr->mon_info_flags & MINFO_FLAG_ANY_SERVER)))
  {
    //mark INCLUDE server except first active server.
    check_entry_in_hash_table(NULL, &server_list, &total_servers, INCLUDE, g_server_hash_table, mon_config_new_ptr->tier_idx, &max_entries);
    check_entry_in_hash_table(NULL, &server_list, &total_servers, EXCLUDE_ANY, g_server_hash_table, mon_config_new_ptr->tier_idx, &max_entries);
  }

  for(int i=0; i<total_servers; i++)
  {
    if(server_list[i].type == INCLUDE || server_list[i].type == INCLUDE_ANY)
    {
      //TODO: Also handle kubernetes case
      create_mon_buf(NULL, 0, NULL, 0, mon_config_new_ptr->mon_name, mon_config_new_ptr->options, mon_config_new_ptr->instance_name,
                     mon_config_new_ptr->json_info_ptr, runtime_flag, mon_config_new_ptr->mon_type, mon_config_new_ptr->tier_idx,
                     mon_config_new_ptr->gdf_name, NULL, server_list[i].idx, NULL, NULL);
    }
    else if(server_list[i].type == EXCLUDE || server_list[i].type == EXCLUDE_ANY)
      //call specific server delete monitor
      mj_runtime_process_deleted_monitors(mon_info_index, server_list[i].idx);
  }
 
  free_mon_config_structure(mon_config_old_ptr, 0);
  mon_config_list_ptr[mon_info_index].mon_config = mon_config_new_ptr;
 
  nslb_obj_hash_destroy(g_server_hash_table);

  if(total_servers > 0)
    return SUCCESS;
  else
    return FAILURE;
}

//This function will process modiefy tag in mon_config node
void mj_process_modified_tag(MonConfig *mon_config_new_ptr, int mon_info_index, int runtime_flag)
{
  MonConfig *mon_config_old_ptr = NULL;
  
  mon_config_old_ptr = mon_config_list_ptr[mon_info_index].mon_config;

  //If options , instance_name or app_name are changed then we need to restart the monitor.
  if(strcmp(mon_config_new_ptr->options,mon_config_old_ptr->options) ||
     (mon_config_old_ptr->instance_name && mon_config_new_ptr->instance_name &&
        (strcmp(mon_config_new_ptr->instance_name, mon_config_old_ptr->instance_name))) ||
     (mon_config_old_ptr->app_name && mon_config_new_ptr->app_name && strcmp(mon_config_new_ptr->app_name, mon_config_old_ptr->app_name)))
  {
    mj_runtime_process_deleted_monitors(mon_info_index, -1);  //delete monitor with previous configuration
    mon_config_list_ptr[mon_info_index].mon_config = mon_config_new_ptr; // assign in old mon_config to new one
    mj_add_monitors_from_json(mon_info_index, runtime_flag); //apply_monitor with new configuration
  }
  else if(mon_config_new_ptr->agent_type != mon_config_old_ptr->agent_type)
  {
    if(!(mon_config_new_ptr->agent_type & mon_config_old_ptr->agent_type)) //CMON to BCI or BCI to CMON
    {
      mj_runtime_process_deleted_monitors(mon_info_index, -1);  //delete monitor with previous configuration
      mon_config_list_ptr[mon_info_index].mon_config = mon_config_new_ptr; // assign old mon_config to new one
      mj_add_monitors_from_json(mon_info_index, runtime_flag); //apply_monitor with new configuration
    }
    else if(mon_config_old_ptr->agent_type & CONNECT_TO_BOTH_AGENT)
    {
      //delete the monitor on one agent type which is not present in current configuration
      //if new conf is CMON then delete on BCI otherwise CMON.
      mon_config_old_ptr->agent_type &= ~mon_config_new_ptr->agent_type;

      mj_runtime_process_deleted_monitors(mon_info_index, -1);  //delete monitor with previous configuration for one agent type 
      
      if((mj_apply_monitor_of_modified_json(mon_config_new_ptr, mon_info_index, runtime_flag)) == FAILURE)
        mj_add_monitors_from_json(mon_info_index, runtime_flag); //apply_monitor with new configuration
    }
    else //mon_config_new_ptr->agent_type & CONNECT_TO_BOTH_AGENT
      mj_apply_monitor_of_modified_json(mon_config_new_ptr, mon_info_index, runtime_flag); //working ??
  }
  else
    mj_apply_monitor_of_modified_json(mon_config_new_ptr, mon_info_index, runtime_flag);

  return;
}


/*  here we tier_type is 0 means tier_name
--> So, tier_id is of topo_tier_info structure
--> Here we tier_type is 0 means tier_group
  So, tier_id is of topo_tier_group_info */
//this function will intiallize and malloc the mon config list struct and also call the mj_create_mon_buf if json is modified
void mj_extract_mon_id_and_initialize_mon_config_struct(nslb_json_t *json, int tier_id, char tier_type, char *temp_tiername,
                                                       MonConfig *mon_config_node, int action_flag, char *json_monitors_filepath,
                                                       int runtime_flag)
{
  StdMonitor *std_mon_ptr = NULL;

  int mon_config_row;
  int mon_config_norm_id; //MonConfig normalized table
  int mon_info_index= -1;
  int g_monid_new_flag;
  int tid;

  char err_msg[MAX_DATA_LINE_LENGTH];

  NSDL2_MON(NULL, NULL, "Method called mj_extract_mon_id_and_initialize_mon_config_struct");

  if((mon_config_node->old_options == NULL) && (mon_config_node->options))
  {
    MALLOC_AND_COPY(mon_config_node->options, mon_config_node->old_options, (strlen(mon_config_node->options) + 1),
                                              "MonConfig Program args", -1);
  }

  mon_config_norm_id = nslb_get_norm_id(g_monid_hash, mon_config_node->g_mon_id, strlen(mon_config_node->g_mon_id));
  mon_info_index = mon_config_norm_id;
 
  if(mon_config_norm_id == -2 && action_flag == MJ_ADDED)  //New G_MON_ID and coming to added first time 
  {
    mon_info_index = nslb_get_set_or_gen_norm_id_ex(g_monid_hash, mon_config_node->g_mon_id, strlen(mon_config_node->g_mon_id),
                                                    &g_monid_new_flag);
    MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                                       "mon_info_index %d is linked with g_monid %s", mon_info_index, mon_config_node->g_mon_id);

    //reallocation of monitor config structure
    if(create_table_entry_ex(&mon_config_row, &total_mon_config_list_entries, &max_mon_config_list_entries,
                               (char **)&mon_config_list_ptr, sizeof(MonConfigList), "Allocating MonConfig Structure Table") == -1)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION, "Could not create table entry for Mon Config");
      return;
    }
    total_mon_config_list_entries += 1;
  }
  else if(mon_config_norm_id != -2 && action_flag == MJ_ADDED) //Old G_MON_ID come with new monitor addition
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                                       "This Monitor g_monid %s is already added in hash map with different json",mon_config_node->g_mon_id);
    free_mon_config_structure(mon_config_node, 0);
    return;
  }
  else if(mon_config_norm_id == -2 && action_flag == MJ_MODIFIED) //New G_MON_ID come with modified tag here will not process this G_MON_ID
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
          "This Monitor g_monid %s is newly added and come with modiefied tag and Here we will not process ", mon_config_node->g_mon_id);
    free_mon_config_structure(mon_config_node, 0);
    return;
  }
    
  if(tier_type == 1)  //if tier_type is 1 that means we get tier_name
  {
    MALLOC_AND_COPY(temp_tiername, mon_config_node->tier_name, (strlen(temp_tiername) + 1), "MonConfig Tier Name", -1);
    // also we have to handle the AllTier
    if(!strcasecmp(mon_config_node->tier_name, "AllTier"))
    {
      for(tier_id=0; tier_id < topo_info[topo_idx].total_tier_count; tier_id++)
      {
        mj_allocate_memory_to_bit_magic_api(&(topo_info[topo_idx].topo_tier_info[tier_id].mon_bv), &(topo_info[topo_idx].topo_tier_info[tier_id].max_bit_vectors));
        nslb_bit_set(&(topo_info[topo_idx].topo_tier_info[tier_id].mon_bv), mon_info_index);                 //set the bit of mon_info_index index
      }
    }
    else
    {
      //This is the case when tier is not present at start of the test
      if(tier_id != -1)
      {
        mj_allocate_memory_to_bit_magic_api(&(topo_info[topo_idx].topo_tier_info[tier_id].mon_bv), &(topo_info[topo_idx].topo_tier_info[tier_id].max_bit_vectors));
        nslb_bit_set(&(topo_info[topo_idx].topo_tier_info[tier_id].mon_bv), mon_info_index);                 //set the bit of mon_info_index index
        mon_config_node->tier_idx = tier_id;
      }
      else
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                           "Failed to apply json auto monitor on %s tier.This tier is not present in topology",mon_config_node->tier_name);
        mon_config_node->tier_idx =-1;
      }
    }
  }
  else        //this is for tier_group
  {
    mon_config_node->mon_info_flags |= MINFO_FLAG_TIER_GROUP;
    MALLOC_AND_COPY(temp_tiername, mon_config_node->tier_group, (strlen(temp_tiername) + 1), "MonConfig Tier Group Name", -1);
    mon_config_node->tier_group_idx = tier_id;

    for(int i=0; i<topo_info[topo_idx].topo_tier_group[tier_id].total_list_count; i++)
    {
      //here we are saving tier_id of tier
      tid = topolib_get_tier_id_from_tier_name(topo_info[topo_idx].topo_tier_group[tier_id].tierList[i],topo_idx);
      if( tid == -1 )
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
                               "Tier [%s] does not exist in topology so skipping monitor application for this tier.",
                                topo_info[topo_idx].topo_tier_group[tier_id].tierList[i]);
        continue;
      }
      else
      {
        mj_allocate_memory_to_bit_magic_api(&(topo_info[topo_idx].topo_tier_info[tid].mon_bv), &(topo_info[topo_idx].topo_tier_info[tid].max_bit_vectors));
        nslb_bit_set(&(topo_info[topo_idx].topo_tier_info[tid].mon_bv), mon_info_index);
      }
    }
  }

  MY_MALLOC_AND_MEMSET(mon_config_node->json_info_ptr, sizeof(JSON_info), "JSON Info Ptr", -1);
  // Here copying monConfig struct in json_info_struct
  mj_fill_json_info_ptr_struct(mon_config_node->json_info_ptr, mon_config_node, mon_info_index);

  if(mon_config_node->mon_type == MTYPE_STD)
  {
    if((std_mon_ptr = get_standard_mon_entry(mon_config_node->mon_name, "ALL", runtime_flag, err_msg)) != NULL)
    {
      if((std_mon_ptr->agent_type & CONNECT_TO_BOTH_AGENT))
      {
        if((set_agent_type(mon_config_node->json_info_ptr, std_mon_ptr, mon_config_node->mon_name)) == -1)
        {
          free_mon_config_structure(mon_config_node, 1);
          return;
        }
      }
      mon_config_node->json_info_ptr->std_mon_ptr = std_mon_ptr;
      MALLOC_AND_COPY(std_mon_ptr->gdf_name, mon_config_node->gdf_name, (strlen(std_mon_ptr->gdf_name) + 1), "MonConfig GDF name", -1);
    }
  }

  if(mon_config_node->json_info_ptr && (mon_config_node->json_info_ptr->agent_type & CONNECT_TO_CMON))
  {
    if(std_mon_ptr && std_mon_ptr->config_json) // for MBean monitor
    {
      MALLOC_AND_COPY(std_mon_ptr->config_json, mon_config_node->json_info_ptr->config_json, strlen(std_mon_ptr->config_json)+1,
                                                   "config_json", -1);
      MALLOC_AND_COPY(std_mon_ptr->monitor_name, mon_config_node->json_info_ptr->mon_name, strlen(std_mon_ptr->monitor_name)+1,
                                                   "monitor_name", -1);
    }
  }

  //Make cm_init buffer for mon config
  make_mon_config_cm_init_buffer(mon_config_node);
  
  if(mon_config_norm_id != -2 && action_flag == MJ_MODIFIED) //Old G_MON_ID come with modified tag
    mj_process_modified_tag(mon_config_node, mon_config_norm_id, 1);
  else
  {
    mon_config_list_ptr[mon_info_index].mon_config = mon_config_node;
    if(runtime_flag)
      mj_add_monitors_from_json(mon_info_index, runtime_flag);//apply_monitor
  }
  return;
}


// This function will call at start of the test
void start_monitors_on_all_servers()
{
  int mon_id;

  NSDL2_MON(NULL, NULL, "Method called start_monitors_on_all_servers");

  for(mon_id=0; mon_id<total_mon_config_list_entries; mon_id++)
  {
    mj_add_monitors_from_json(mon_id, 0);
  }
  return;
}


/**********************************************************JSON Parsing Code Function**********************************************************/

//This function will read global json variables and store the MetricPriority in g_metric_priority
void mj_read_global_json(char *json_file)
{
  nslb_json_t *json = NULL;
  nslb_json_error err;
  
  char json_monitors_filepath[MAX_DATA_LINE_LENGTH];
  char *dummy_ptr=NULL;
  int len=0;
  int ret=0;
 
  NSDL2_MON(NULL, NULL, "Method called mj_read_global_json");

  if(check_json_format(json_file) != 0)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                       "Error: Either filename [%s] does not exist or there are one or more error in json [%s]. Check it in json editor.",
                        json_file, json_file);
    return;
  }
  //store json file in json pointer
  json = nslb_json_init_buffer(json_file, 0, 0, &err);
  MLTL4(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "JSON =%p",json);
  if(json == NULL)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                       "Unable to convert json content of file [%s] to json structure, due to: %s.",
                        json_file, nslb_json_strerror(json));
    return;
  }
  
  strncpy(json_monitors_filepath, json_file, MAX_DATA_LINE_LENGTH);

  OPEN_ELEMENT_OF_MONITORS_JSON(json);

  GOTO_ELEMENT_OF_MONITORS_JSON(json, "MetricPriority", 0);
  if(ret != -1)
  {
    GET_ELEMENT_VALUE_OF_MONITORS_JSON_EX(json, dummy_ptr, &len, 0, 1);
    if(len > 0)
    g_metric_priority = get_metric_priority_id(dummy_ptr, 0);
  }
  else
    ret = 0;

  NSDL2_MON(NULL, NULL, "g_metric_priority = %d", g_metric_priority); 
  CLOSE_JSON_ELEMENT(json);
  return;
}

// This function will parse the type of monitor i.e added deleted or modified
// return action_flag
// 0 means monitor is added
// 1 means monitor is modified
// 2 means monitor is deleted
int mj_parse_type_in_json(nslb_json_t *json, char *json_monitors_filepath, char *g_monid_buf, int g_monid_buf_len)
{
  char *dummy_ptr=NULL;
  int len=0, ret=0;
  int mon_info_index;
  
  NSDL2_MON(NULL, NULL, "Method called mj_parse_type_in_json");

  GOTO_ELEMENT_OF_MONITORS_JSON(json, "type",0);
  {
    if(ret != -1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON_EX(json, dummy_ptr, &len, 0, 1);
      // We traverse the json file for added and modified type
      // Not require the json traversing for deleted type only we check the id and with the help of id we deleted the monitor 
      // And clean the structure.
      // For modified type we have also traverse the json file because we dont know which tag is modified;
      if(!(strcasecmp(dummy_ptr, "added")))
      {
        return MJ_ADDED;
      }
      else if(!(strcasecmp(dummy_ptr, "deleted")))
      {
        mon_info_index = nslb_get_norm_id(g_monid_hash, g_monid_buf, g_monid_buf_len);
        //here we extract the id tag from json which is unique for all the monitors
        if(mon_info_index != -1)
        {
          mj_runtime_process_deleted_monitors(mon_info_index, -1);
        }
        CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON_EX(json);
        return MJ_DELETED;       
      }
      else if(!(strcasecmp(dummy_ptr, "modified")))
      {
        return MJ_MODIFIED;     
      }
      /*else
      {
        CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON_EX(json);
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "no provided valid argument: type %s", dummy_ptr);
        return 0;
      }*/
    }
    else
      ret=0;
  }
  return 0;
}

// this function will parse exclude_server, specific server, exclude tier, frequency and instance tags
inline static void mj_process_server_related_info(nslb_json_t *json, MonConfig *mon_config_node, char *json_monitors_filepath)
{ 
  char *dummy_ptr=NULL;
  int len=0, ret=0; 

  NSDL2_MON(NULL, NULL, "Method called mj_process_server_related_info");

  //Give memory to each buffer
  NSLB_REALLOC_AND_MEMSET(mon_config_node->exclude_tier, 1 * sizeof(ArrInfo), 0,
                                                                    "Reallocating Exclude tier struct of MonConfig", -1, 0);
  NSLB_REALLOC_AND_MEMSET(mon_config_node->exclude_server, 1 * sizeof(ArrInfo), 0,
                                                                    "Reallocating exclude Server struct of MonConfig", -1, 0);
  NSLB_REALLOC_AND_MEMSET(mon_config_node->specific_server, 1 * sizeof(ArrInfo), 0,
                                                                    "Reallocating Specific Server struct of MonConfig", -1, 0);
  GOTO_ELEMENT_OF_MONITORS_JSON(json, "exclude-tier",0);
  {
    if(ret != -1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON_EX(json, dummy_ptr, &len, 0, 1);
      fill_arr_info_struct(dummy_ptr, mon_config_node->exclude_tier);
      mon_config_node->mon_info_flags |= MINFO_FLAG_EXCLUDE_TIER;
    }
    else
     ret=0;
  }

  GOTO_ELEMENT_OF_MONITORS_JSON(json, "exclude-server",0);
  {
    if(ret != -1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON_EX(json, dummy_ptr, &len, 0, 1);
      fill_arr_info_struct(dummy_ptr, mon_config_node->exclude_server);
      mon_config_node->mon_info_flags |= MINFO_FLAG_EXCLUDE_SERVER;
    }
    else
      ret=0;
  }

  GOTO_ELEMENT_OF_MONITORS_JSON(json, "specific-server",0);
  {
    if(ret != -1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON_EX(json, dummy_ptr, &len, 0, 1);
      fill_arr_info_struct(dummy_ptr, mon_config_node->specific_server);
      mon_config_node->mon_info_flags |= MINFO_FLAG_SPECIFIC_SERVER;
      //if any is passed in specific server
      if(strcasecmp(mon_config_node->specific_server[0].list[0],"Any") == 0)
      {
        mon_config_node->mon_info_flags |= MINFO_FLAG_ANY_SERVER;  
      }
    }
    else
      ret=0;
  }
    
  //here frequency is monitor interval
  GOTO_ELEMENT_OF_MONITORS_JSON(json, "frequency",0);
  {
    if(ret != -1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON_EX(json, dummy_ptr, &len, 0, 1);
      mon_config_node->frequency = atoi(dummy_ptr) *1000;
    }
    else
    {
      mon_config_node->frequency = global_settings->progress_secs;
      ret=0;
    }
  }
 
  //This will be 1 or 2 
  //If 1 means dont need to retry and if 2 we have to retry if fails. 
  GOTO_ELEMENT_OF_MONITORS_JSON(json, "sm-mode",0);
  {
    if(ret != -1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON_EX(json, dummy_ptr, &len, 0, 1);
      mon_config_node->sm_mode = atoi(dummy_ptr);
    }
    else
      ret=0;
  }

  GOTO_ELEMENT_OF_MONITORS_JSON(json, "instance",0);
  {
    if(ret != -1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON_EX(json, dummy_ptr, &len, 0, 1);
      MALLOC_AND_COPY(dummy_ptr, mon_config_node->instance_name, (len + 1), "MonConfig Instance Name of Standard Monitor", -1);
    }
    else
      ret=0;
  }

  GOTO_ELEMENT_OF_MONITORS_JSON(json, "run-as-process",0);
  {
    if(ret !=-1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON_EX(json, dummy_ptr, &len, 0, 1);
      if( !strcasecmp(dummy_ptr,"true"))
        mon_config_node->is_process = 1;
    }
    else
      ret=0;
  }

  return; 
}


// This function will process the standard monitor json
void mj_process_standard_monitors(nslb_json_t *json, int process_std_mon, char *temp_tiername, int tier_id, char tier_type,
                                  char *json_monitors_filepath, int runtime_flag)
{
  char *dummy_ptr=NULL;
  
  char mon_name[MAX_DATA_LINE_LENGTH];
  int mon_name_len;
  char g_monid_buf[MAX_DATA_LINE_LENGTH];
  int g_monid_buf_len;

  int len, ret =0;
  int action_flag=0; 
  MonConfig *mon_config_node=NULL;

  NSDL2_MON(NULL, NULL, "Method called mj_process_standard_monitors");
  
  while (process_std_mon)
  {
    ret=0;
    
    GOTO_NEXT_ARRAY_ITEM_OF_MONITORS_JSON(json);
    OPEN_ELEMENT_OF_MONITORS_JSON(json);

    //id tag is for all the monitor and it is mandatory tag which is used for uniqueness of monitor
    GOTO_ELEMENT_OF_MONITORS_JSON(json, "id",1);
    if(ret!=-1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON_EX(json, dummy_ptr, &len, 0, 1);
      strcpy(g_monid_buf, dummy_ptr);
      g_monid_buf_len =len;
    }
    else
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                                       "not provided argument: id and which is mandatory: for tier %s , JSON file %s",
                                       temp_tiername, json_monitors_filepath);
      CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON_EX(json);
      continue;
    }
    //Action Flag
    // 0 means monitor is added
    // 1 means monitor is modified
    // 2 means monitor is deleted
    if((action_flag = mj_parse_type_in_json(json, json_monitors_filepath, g_monid_buf, g_monid_buf_len)) == 2)  //That means type is deleted
      continue;

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "enabled",0);
    {
      if(ret!=-1)
      {
        GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
        if( !strcasecmp(dummy_ptr,"false"))
        {
          CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
          continue;
        }
      }
      else
        ret=0;
    }

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "std-mon-name",1);
    if(ret!=-1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      strcpy(mon_name, dummy_ptr);
      mon_name_len = len;
    }
    else
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                                         "not provided argument: std-mon-name for tier: %s ,JSON file %s",
                                         temp_tiername, json_monitors_filepath);
      CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
      continue;
    }
    
    //MY_MALLOC_AND_MEMSET(mon_config_node, (sizeof(MonConfig)), "mon_config_node", -1);

    mon_config_node = (MonConfig*)nslb_mp_get_slot(&(mon_config_pool));
    MALLOC_AND_COPY(mon_name, mon_config_node->mon_name, (mon_name_len + 1), "MonConfig Montior Name of Std Monitor", -1);
    MALLOC_AND_COPY(g_monid_buf, mon_config_node->g_mon_id, (g_monid_buf_len + 1), "MonConfig g_mon_id", -1);

    //here this function will parse server related tags
    mj_process_server_related_info(json, mon_config_node, json_monitors_filepath);

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "tier-for-any-server",0);
    {
      if(ret != -1)
      {
        GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
        NSLB_REALLOC_AND_MEMSET(mon_config_node->dest_tier_server, 1 * sizeof(ArrInfo), 0,
                                                                    "Reallocating Destination Tier Server struct of MonConfig", -1, 0);
        fill_arr_info_struct(dummy_ptr, mon_config_node->dest_tier_server);
        mon_config_node->mon_info_flags |= MINFO_FLAG_DEST_TIER_ANY_SERVER;
      }
      else
        ret=0;
    }

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "init-vector-file",0);
    {
      if(ret != -1)
      {
        GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
        MALLOC_AND_COPY(dummy_ptr, mon_config_node->init_vector_file, (len + 1), "MonConfig Initvectorfile of Standard Monitor", -1);
      }
      else
        ret=0;
    }

    mon_config_node->agent_type |= CONNECT_TO_CMON;

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "app",1);
    OPEN_ELEMENT_OF_MONITORS_JSON(json);

    while(1)
    {
      ret=0;

      GOTO_NEXT_ARRAY_ITEM_OF_MONITORS_JSON(json);
      OPEN_ELEMENT_OF_MONITORS_JSON(json);
      GOTO_ELEMENT_OF_MONITORS_JSON(json, "app-name",0);
      if(ret != -1)
      {
        GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
        //this check is for app_name if appname is not default then we set the MINFO_FLAG_APP_NAME bit in MonConfig
        if(strcasecmp(dummy_ptr,"default"))
        {
          MALLOC_AND_COPY(dummy_ptr, mon_config_node->app_name, (len + 1), "MonConfig App Name of Standard Monitor", -1);
          mon_config_node->mon_info_flags |= MINFO_FLAG_APP_NAME;
        }
      }
      else
        ret=0;

      GOTO_ELEMENT_OF_MONITORS_JSON(json, "cmon-pod-pattern",0);
      if(ret !=-1)
      {
        GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
        MALLOC_AND_COPY(dummy_ptr, mon_config_node->cmon_pod_pattern, (len + 1), "MonConfig Cmon pod pattern of Standard Monitor", -1);
      }
      else
        ret=0;

      GOTO_ELEMENT_OF_MONITORS_JSON(json, "java-home",0);
      if(ret !=-1)
      {
        GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
        MALLOC_AND_COPY(dummy_ptr, mon_config_node->javaHome, (len + 1), "MonConfig JAVA Home of Standard Monitor", -1);
      }
      else
        ret=0;

      GOTO_ELEMENT_OF_MONITORS_JSON(json, "java-classpath",0);
      if(ret !=-1)
      {
        GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
        MALLOC_AND_COPY(dummy_ptr, mon_config_node->javaClassPath, (len + 1), "MonConfig JavaClasspath of Standard Monitor", -1);
      }
      else
        ret=0;

      GOTO_ELEMENT_OF_MONITORS_JSON(json, "TierServerMappingType",0);
      if(ret !=-1)
      {
        GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
        if(!strncasecmp(dummy_ptr, "standard", 8))
          mon_config_node->tier_server_mapping_type |= STANDARD_TYPE;
        else if(!strncasecmp(dummy_ptr, "custom", 6))
          mon_config_node->tier_server_mapping_type |= CUSTOM_TYPE;
      }
      else
        ret=0;

      GOTO_ELEMENT_OF_MONITORS_JSON(json, "VectorReplaceFrom",0);
      if(ret != -1)
      {
        GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
        MALLOC_AND_COPY(dummy_ptr, mon_config_node->vectorReplaceFrom, (len + 1), "MonConfig VectorReplaceFrom of Standard Monitor", -1);
      }
      else
        ret = 0;

      GOTO_ELEMENT_OF_MONITORS_JSON(json, "VectorReplaceTo",0);
      if(ret != -1)
      {
        GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
        MALLOC_AND_COPY(dummy_ptr, mon_config_node->vectorReplaceTo, (len + 1), "MonConfig VectorReplaceTo of Standard Monitor", -1);
      }
      else
        ret=0;

      GOTO_ELEMENT_OF_MONITORS_JSON(json, "old-options",0);
      if(ret != -1)
      {
        GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
        MALLOC_AND_COPY(dummy_ptr, mon_config_node->old_options, (len + 1), "MonConfig Program Args of Standard Monitor", -1);
      }
      else
        ret=0;
  
      GOTO_ELEMENT_OF_MONITORS_JSON(json, "options",1);
      if(ret != -1)
      {
        GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
        MALLOC_AND_COPY(dummy_ptr, mon_config_node->options, (len + 1), "MonConfig Program Args of Standard Monitor", -1);
      }
      else
        ret=0;

      mon_config_node->agent_type |= CONNECT_TO_CMON;
      GOTO_ELEMENT_OF_MONITORS_JSON(json, "agent-type",0);
      {
        if(ret != -1)
        {
          GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
          if(!strcasecmp(dummy_ptr,"BCI"))
          {
            //here unset cmon 
            mon_config_node->agent_type |= CONNECT_TO_NDC;
            mon_config_node->agent_type &= ~CONNECT_TO_CMON;
          }
          //We do not want it to start NDC Mbean monitor to be applied at runtime. 
          else if(!strcasecmp(dummy_ptr,"ALL"))
            mon_config_node->agent_type |= CONNECT_TO_BOTH_AGENT;
        }
        else
          ret=0;
      }
   
      mon_config_node->run_opt = 2;    
      mon_config_node->mon_type = MTYPE_STD; 
      //here we extract the id tag from json which is unique for all the monitors
      mj_extract_mon_id_and_initialize_mon_config_struct(json, tier_id, tier_type, temp_tiername, mon_config_node, action_flag,
                                                         json_monitors_filepath, runtime_flag);
 
      CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
    }
 
    if(ret == -1)
    {
      if(((int)json->error)!=6)
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                                  "Can't able to apply monitors on Tier [%s] from json [%s] due to error: %s.",
                                  temp_tiername, global_settings->auto_json_monitors_filepath, nslb_json_strerror(json));
        return;
      }
      else
      {
        CLOSE_ELEMENT_ARR_OF_MONITORS_JSON(json);
        CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
        continue;
      }
    }
    CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json)
  }
  
  if(ret == -1)
  {
    if(((int)json->error)!=6)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                         "Can't able to apply monitors on Tier [%s] from json [%s] due to error: %s.",
                         temp_tiername, json_monitors_filepath, nslb_json_strerror(json));
    }
    else
    {
      CLOSE_ELEMENT_ARR_OF_MONITORS_JSON_EX(json);
      ret =0;
    }
  }
  return;
}


// This function will process the custom gdf monitor json
void mj_process_custom_gdf_monitors(nslb_json_t *json, int process_custom_gdf_mon, char *temp_tiername, int tier_id, char tier_type,
                                    char *json_monitors_filepath, int runtime_flag)
{
  char *dummy_ptr=NULL;
  
  char prgrm_type[MAX_DATA_LINE_LENGTH];
  int prgrm_type_len;
  char mon_name[MAX_DATA_LINE_LENGTH];
  int mon_name_len;
  char g_monid_buf[MAX_DATA_LINE_LENGTH];
  int g_monid_buf_len;
  char gdf_name[MAX_DATA_LINE_LENGTH];
  int gdf_name_len;
  char config_file[MAX_DATA_LINE_LENGTH];
  char app_name[MAX_DATA_LINE_LENGTH];
  int app_name_len;

  int len, ret =0;
  int action_flag=0;

  MonConfig *mon_config_node=NULL;

  NSDL2_MON(NULL, NULL, "Method called mj_process_custom_gdf_monitors");

  while(process_custom_gdf_mon)
  {
    ret=0;

    GOTO_NEXT_ARRAY_ITEM_OF_MONITORS_JSON(json);
    OPEN_ELEMENT_OF_MONITORS_JSON(json);
    
    //id tag is for all the monitor and it is mandatory tag which is used for uniqueness of monitor
    GOTO_ELEMENT_OF_MONITORS_JSON(json, "id",1);
    if(ret!=-1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON_EX(json, dummy_ptr, &len, 0, 1);
      strcpy(g_monid_buf,dummy_ptr);
      g_monid_buf_len =len;
    }
    else
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                                       "not provided argument: id and which is mandatory: for tier %s, JSON file %s",
                                       temp_tiername, json_monitors_filepath);
      CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON_EX(json);
      continue;
    }
    //Action Flag
    // 0 means monitor is added
    // 1 means monitor is modified
    // 2 means monitor is deleted
    if((action_flag = mj_parse_type_in_json(json, json_monitors_filepath, g_monid_buf, g_monid_buf_len)) == 2)  //That means type is deleted
      continue;

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "enabled",0);
    {
      if(ret!=-1)
      {
        GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
        if( !strcasecmp(dummy_ptr,"false"))
        {
          CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
          continue;
        }
      }
      else
        ret=0;
    }

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "program-name", 1);
    if(ret != 1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      strcpy(mon_name, dummy_ptr);
      mon_name_len = len;
    }
    else
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                                           "not provided argument: program-name of custom-gdf-mon for tier: %s, JSON file %s",
                                           temp_tiername, json_monitors_filepath);
      CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
      continue;
    }

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "gdf-name", 1);
    if(ret != 1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      strcpy(gdf_name, dummy_ptr);
      gdf_name_len =len;
    }
    else
    {
      MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                                            "not provided argument: program-name of custom-gdf-mon %s for tier: %s, JSON file %s",
                                             mon_name, temp_tiername, json_monitors_filepath);
      CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
      continue;
    }

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "pgm-type", 1);
    if(ret!=-1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      strcpy(prgrm_type, dummy_ptr);
      prgrm_type_len =len;
    }
    else
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                                               "not provided argument: pgm-type of custom-gdf-mon %s for tier: %s, JSON file %s",
                                               mon_name, temp_tiername, json_monitors_filepath);
      CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
      continue;
    }
  
    GOTO_ELEMENT_OF_MONITORS_JSON(json, "app-name", 1);
    if(ret!=-1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      strcpy(app_name, dummy_ptr);
      app_name_len =len;
    }
    else
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                                                  "not provided argument: app-name of custom-gdf-mon %s for tier: %s, JSON file %s",
                                                   mon_name, temp_tiername, json_monitors_filepath);
      CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
      continue;
    }

    //MY_MALLOC_AND_MEMSET(mon_config_node, (sizeof(MonConfig)), "mon_config_node", -1);

    mon_config_node = (MonConfig*)nslb_mp_get_slot(&(mon_config_pool));
    MALLOC_AND_COPY(app_name, mon_config_node->app_name, (app_name_len + 1), "MonConfig App Name of Custom GDF Monitor", -1);
    MALLOC_AND_COPY(prgrm_type, mon_config_node->pgm_type, (prgrm_type_len + 1), "MonConfig Program type of Custom GDF Monitor", -1);
    MALLOC_AND_COPY(mon_name, mon_config_node->mon_name, (mon_name_len + 1), "MonConfig Montior Name of Custom GDF Monitor", -1);
    MALLOC_AND_COPY(gdf_name, mon_config_node->gdf_name, (gdf_name_len + 1), "MonConfig GDF name of Custom GDF Monitor", -1);
    MALLOC_AND_COPY(g_monid_buf, mon_config_node->g_mon_id, (g_monid_buf_len + 1), "MonConfig g_mon_id", -1);

    //here this function will parse server related tags
    mj_process_server_related_info(json, mon_config_node, json_monitors_filepath);

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "run-option", 0);
    if(ret != -1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      mon_config_node->run_opt = atoi(dummy_ptr);

      if((mon_config_node->run_opt != 1) && (mon_config_node->run_opt != 2))
      {
        MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                                 "run_option provided is not correct.Hence setting it to 2:program-name of custom-gdf-mon %s for tier: %s",
                                                  mon_config_node->mon_name, temp_tiername);
        mon_config_node->run_opt = 2;
      }
    }
    else
    {
      mon_config_node->run_opt =2;
      ret=0;
    }

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "cfg", 0);
    if(ret!=-1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      len = sprintf(config_file,"%s/mprof/.custom/%s", g_ns_wdir,dummy_ptr);
      MALLOC_AND_COPY(config_file, mon_config_node->config_json, (len + 1), "MonConfig Config Json File of Custom GDF Monitor", -1);
    }
    else
    {
      ret=0;
      MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                                                "not provided argument: cfg of custom-gdf-mon %s for tier: %s",
                                                 mon_config_node->mon_name, temp_tiername);
    } 

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "os-type", 0);
    if(ret!=-1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      MALLOC_AND_COPY(dummy_ptr, mon_config_node->os_type, (len + 1), "MonConfig OS Type of Custom GDF Monitor", -1);
    }
    else
    {
      MALLOC_AND_COPY("LinuxEx", mon_config_node->os_type, 8, "MonConfig OS type of Custom GDF Monitor", -1);
      MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                            "not provided argument: OS-type of custom-gdf-mon %s for tier: %s. Hence setting default os-type to LinuxEx",
                             mon_config_node->mon_name, temp_tiername);
      ret =0;
    }

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "old-options", 0);
    if(ret != -1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      MALLOC_AND_COPY(dummy_ptr, mon_config_node->old_options, (len + 1), "MonConfig Program Args of Standard Monitor", -1);
    }
    else
      ret=0;
 
    GOTO_ELEMENT_OF_MONITORS_JSON(json, "options",1);
    if(ret != -1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      MALLOC_AND_COPY(dummy_ptr, mon_config_node->options, (len + 1), "MonConfig Program Args of Standard Monitor", -1);
    }
    else
      ret=0;

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "mhp", 0);
    if(ret != -1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      mon_config_node->skip_breadcrumb_creation = atoi(dummy_ptr);
      if((mon_config_node->skip_breadcrumb_creation != 1) && (mon_config_node->skip_breadcrumb_creation != 0))
      {
        MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                    "Metric Hierarchy prefix provided is not correct.Hence setting it to 1: program-name of custom-gdf-mon %s for tier: %s",
                     mon_config_node->mon_name, temp_tiername);
        mon_config_node->skip_breadcrumb_creation = 1;
      }
    }
    else
    {
      mon_config_node->skip_breadcrumb_creation = 1;
      ret=0;
    }

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "java-home", 0);
    if(ret!=-1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      MALLOC_AND_COPY(dummy_ptr, mon_config_node->javaHome, (len + 1), "MonConfig JAVA Home of Custom GDF Monitor", -1);
    }
    else
    {
      ret =0;
    }

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "java-classpath", 0);
    if(ret!=-1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      MALLOC_AND_COPY(dummy_ptr, mon_config_node->javaClassPath, (len + 1), "MonConfig JAVA class path of Custom GDF Monitor", -1);
      NSDL2_MON(NULL, NULL, "javaClassPath  %s",mon_config_node->javaClassPath);
    }
    else
    {
      ret =0;
    }

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "use-agent", 0);
    if(ret!=-1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      MALLOC_AND_COPY(dummy_ptr, mon_config_node->use_agent, (len + 1), "MonConfig UseAgent of Custom GDF Monitor", -1);
    }
    else
    {
      ret =0;
    }

    mon_config_node->agent_type |= CONNECT_TO_CMON;
    GOTO_ELEMENT_OF_MONITORS_JSON(json, "agent-type",0);
    {
      if(ret != -1)
      {
        GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
        if(!strcasecmp(dummy_ptr,"BCI"))
        {
          //here unset cmon 
          mon_config_node->agent_type |= CONNECT_TO_NDC;
          mon_config_node->agent_type &= ~CONNECT_TO_CMON;
        }
        //We do not want it to start NDC Mbean monitor to be applied at runtime. 
        else if(!strcasecmp(dummy_ptr,"ALL"))
        {
          mon_config_node->agent_type |= CONNECT_TO_BOTH_AGENT;
        }
      }
      else
        ret=0;
    }

    mon_config_node->mon_type = MTYPE_CUSTOM_GDF;
    mon_config_node->metric_priority = g_metric_priority;
   
    //here we extract the id tag from json which is unique for all the monitors
    mj_extract_mon_id_and_initialize_mon_config_struct(json, tier_id, tier_type, temp_tiername, mon_config_node, action_flag,
                                                       json_monitors_filepath, runtime_flag);

    CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
  }
  
  if(ret == -1)
  {
    if(((int)json->error)!=6)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                         "Can't able to apply monitors on Tier [%s] from json [%s] due to error: %s.",
                         temp_tiername, json_monitors_filepath, nslb_json_strerror(json));
    }
    else
    {
      CLOSE_ELEMENT_ARR_OF_MONITORS_JSON_EX(json);
      ret =0;
    }
  }
  return;
}


/* This function will parse the log data monitor , get log file monitor and log pattern monitor json
-----------------------------------------------
  process_log__mon               -   value
-----------------------------------------------
  PROCESS_LOG_PATTERN_MON         -    1
  PROCESS_GET_LOG_FILE_MON        -    2
  PROCESS_LOG_DATA_MON            -    3
-------------------------------------------------*/
void mj_process_log_monitors(nslb_json_t *json, int process_log_mon, char *temp_tiername, int tier_id, char tier_type, char *mon_name_buf,
                             char *json_monitors_filepath, int runtime_flag)
{
  char *dummy_ptr=NULL;

  char prgrm_args[MAX_DATA_LINE_LENGTH];
  char new_options[MAX_DATA_LINE_LENGTH];
  char old_options[MAX_DATA_LINE_LENGTH];
  char mon_name[MAX_DATA_LINE_LENGTH];
  int mon_name_len;
  char g_monid_buf[MAX_DATA_LINE_LENGTH];
  int g_monid_buf_len;
  char gdf_name[MAX_DATA_LINE_LENGTH];
  int gdf_name_len;
  int run_options;

  int len, ret=0;
  int action_flag=0;
 
  MonConfig *mon_config_node=NULL;
  
  NSDL2_MON(NULL, NULL, "Method called mj_process_log_monitors");

  while (process_log_mon)
  {
    ret=0;

    GOTO_NEXT_ARRAY_ITEM_OF_MONITORS_JSON(json);
    OPEN_ELEMENT_OF_MONITORS_JSON(json);

    //id tag is for all the monitor and it is mandatory tag which is used for uniqueness of monitor
    GOTO_ELEMENT_OF_MONITORS_JSON(json, "id",1);
    if(ret!=-1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON_EX(json, dummy_ptr, &len, 0, 1);
      strcpy(g_monid_buf, dummy_ptr);
      g_monid_buf_len =len;
    }
    else
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                                       "not provided argument: id and which is mandatory: for tier %s, JSON file:%s",
                                       temp_tiername, json_monitors_filepath);
      CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON_EX(json);
      continue;
    }
    //Action Flag
    // 0 means monitor is added
    // 1 means monitor is modified
    // 2 means monitor is deleted
    if((action_flag = mj_parse_type_in_json(json, json_monitors_filepath, g_monid_buf, g_monid_buf_len)) == 2)  //That means type is deleted
      continue;

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "enabled",0);
    {
      if(ret!=-1)
      {
        GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
        if( !strcasecmp(dummy_ptr,"false"))
        {
          CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
          continue;
        }
      }
      else
        ret=0;
    }

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "log-mon-name",1);
    if(ret!=-1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      strcpy(mon_name, dummy_ptr);
      mon_name_len =len;
    }
    else
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                                         "not provided argument: log-mon-name for tier: %s, JSON file %s ",
                                         temp_tiername, json_monitors_filepath);
      CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
      continue;
    }

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "gdf-name",1);
    if(ret!=-1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      strcpy(gdf_name, dummy_ptr);
      gdf_name_len =len;
    }
    else
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                                   "not provided argument: gdf-name of log-mon-name %s for tier: %s, JSON file %s",
                                    mon_name, temp_tiername, json_monitors_filepath);
      CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
      continue;
    }
    GOTO_ELEMENT_OF_MONITORS_JSON(json, "run-option",0);
    if(ret!=-1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      run_options = atoi(dummy_ptr);
      strcat(prgrm_args,dummy_ptr);
      strcat(prgrm_args," ");
    }
    else
    {
      strcat(prgrm_args,"2 ");
      run_options = 2;
      ret=0;
    }

    if(process_log_mon == PROCESS_LOG_PATTERN_MON)
      strcat(prgrm_args,"java cm_log_parser ");
    else if(process_log_mon == PROCESS_GET_LOG_FILE_MON)
      strcat(prgrm_args,"cm_get_log_file ");
    else if(process_log_mon == PROCESS_LOG_DATA_MON)
      strcat(prgrm_args,"java cm_file ");

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "options",1);
    if(ret!=-1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      strcpy(new_options, prgrm_args);
      strcat(new_options, dummy_ptr); 
    }
    else
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                                             "not provided argument: options of log-mon-name %s for tier: %s, JSON file %s",
                                              mon_config_node->mon_name,temp_tiername, json_monitors_filepath);
      CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
      continue;
    }

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "old-options",0);
    if(ret!=-1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      strcpy(old_options, prgrm_args);
      strcat(old_options, dummy_ptr);
    }

    mon_config_node = (MonConfig*)nslb_mp_get_slot(&(mon_config_pool)); 
    MALLOC_AND_COPY(new_options, mon_config_node->options, (strlen(new_options) +1), "MonConfig Program args of Log Monitor", -1);
    if(old_options[0] != '\0')
      MALLOC_AND_COPY(old_options, mon_config_node->old_options, (strlen(old_options) +1), "MonConfig Program args of Log Monitor", -1);
    MALLOC_AND_COPY(mon_name, mon_config_node->mon_name, (mon_name_len + 1), "MonConfig Montior Name of Log Monitor", -1);
    MALLOC_AND_COPY(gdf_name, mon_config_node->gdf_name, (gdf_name_len + 1), "MonConfig GDF name of Log Monitor", -1);
    MALLOC_AND_COPY(g_monid_buf, mon_config_node->g_mon_id, (g_monid_buf_len + 1), "MonConfig g_mon_id", -1);
    
    mon_config_node->run_opt = run_options;
    mon_config_node->mon_type = MTYPE_LOG;
    mon_config_node->agent_type |= CONNECT_TO_CMON;
    mon_config_node->metric_priority = g_metric_priority;
   
    //here this function will parse server related tags
    mj_process_server_related_info(json, mon_config_node, json_monitors_filepath);

    if(mon_config_node->instance_name == NULL)
      MALLOC_AND_COPY(mon_config_node->mon_name, mon_config_node->instance_name, (strlen(mon_config_node->mon_name) + 1),
                                                "MonConfig Instance Name of Log monitor", -1);
 
    //here we extract the id tag from json which is unique for all the monitors
    mj_extract_mon_id_and_initialize_mon_config_struct(json, tier_id, tier_type, temp_tiername, mon_config_node, action_flag,
                                                       json_monitors_filepath, runtime_flag);

    CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
  }
  
  if(ret == -1)
  {
    if(((int)json->error)!=6)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                         "Can't able to apply monitors on Tier [%s] from json [%s] due to error: %s.",
                         temp_tiername, json_monitors_filepath, nslb_json_strerror(json));
    }
    else
    {
      CLOSE_ELEMENT_ARR_OF_MONITORS_JSON_EX(json);
      ret =0;
    }
  }
  return;
}


/* This function will parse the check monitor , batch job monitor and server signature monitor json
-----------------------------------------------
  process_check_mon               -   value
-----------------------------------------------
  PROCESS_CHECK_MON               -    1
  PROCESS_BATCH_JOB_MON           -    2
  PROCESS_SERVER_SIGNATURE_MON    -    3
-------------------------------------------------*/
void mj_process_check_monitors(nslb_json_t *json, char *temp_tiername, int tier_id, char tier_type, int process_check_mon,
                               char *mon_name_buf, char *json_monitors_filepath, int runtime_flag)
{
  char *dummy_ptr=NULL;

  // We are defining these varible so that 
  char prgrm_args[MAX_DATA_LINE_LENGTH];
  char old_prgrm_args[MAX_DATA_LINE_LENGTH];
  int prgrm_args_len;
  int old_prgrm_args_len;
  char mon_name[MAX_DATA_LINE_LENGTH];
  int mon_name_len;
  char g_monid_buf[MAX_DATA_LINE_LENGTH];
  int g_monid_buf_len;
  int mon_type;

  int len=0, ret=0;
  int action_flag=0; 

  MonConfig *mon_config_node=NULL;

  NSDL2_MON(NULL, NULL, "Method called mj_process_check_monitors");

  while(process_check_mon)
  {
    ret=0;

    GOTO_NEXT_ARRAY_ITEM_OF_MONITORS_JSON(json);
    OPEN_ELEMENT_OF_MONITORS_JSON(json);

    //id tag is for all the monitor and it is mandatory tag which is used for uniqueness of monitor
    GOTO_ELEMENT_OF_MONITORS_JSON(json, "id",1);
    if(ret!=-1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON_EX(json, dummy_ptr, &len, 0, 1);
      strcpy(g_monid_buf, dummy_ptr);
      g_monid_buf_len =len;
    }
    else
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                                       "not provided argument: id and which is mandatory: for tier %s JSON file %s",
                                       temp_tiername, json_monitors_filepath);
      CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON_EX(json);
      continue;
    }
    //Action Flag
    // 0 means monitor is added
    // 1 means monitor is modified
    // 2 means monitor is deleted
    if((action_flag = mj_parse_type_in_json(json, json_monitors_filepath, g_monid_buf, g_monid_buf_len)) == 2)  //That means type is deleted
      continue;

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "enabled",0);
    {
      if(ret!=-1)
      {
        GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
        if( !strcasecmp(dummy_ptr,"false"))
        {
          CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
          continue;
        }
      }
      else
        ret=0;
    }

    if(process_check_mon == PROCESS_CHECK_MON)
    {
      GOTO_ELEMENT_OF_MONITORS_JSON(json, "check-mon-name",1);
      mon_type = MTYPE_CHECK;
    }
    else if(process_check_mon == PROCESS_BATCH_JOB_MON)
    {
      GOTO_ELEMENT_OF_MONITORS_JSON(json, "batch-job-mon",1);
      mon_type = MTYPE_BATCH_JOB;
    }
    else if(process_check_mon == PROCESS_SERVER_SIGNATURE_MON)
    {
      GOTO_ELEMENT_OF_MONITORS_JSON(json, "signature-name",1);
      mon_type = MTYPE_SERVER_SIGNATURE;
    }
    else
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                                   "not provided any tag for the monitor name in %s, Tier:%s, JSON file:",
                                   mon_name_buf, temp_tiername, json_monitors_filepath);
      CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
      continue;
    }
 
    if(ret ==-1)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                                   "not provided any tag for the monitor name in %s, Tier:%s, JSON file:",
                                   mon_name_buf, temp_tiername, json_monitors_filepath);
      CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
      continue;
    }
 
    GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
    strcpy(mon_name, dummy_ptr);
    mon_name_len =len;
 
    ret=0;

    GOTO_ELEMENT_OF_MONITORS_JSON(json, "old-options",0);
    if(ret != -1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      strcpy(old_prgrm_args, dummy_ptr);
      old_prgrm_args_len= len;
    }

    ret=0;
    GOTO_ELEMENT_OF_MONITORS_JSON(json, "options",1);
    if(ret != -1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      strcpy(prgrm_args, dummy_ptr);
      prgrm_args_len= len;
    }
    else
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                                    "not provided argument: options for %s %s for tier :%s, JSON file:",
                                     mon_name_buf, mon_name, temp_tiername, json_monitors_filepath);
      CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
      continue;
    }

    mon_config_node = (MonConfig*)nslb_mp_get_slot(&(mon_config_pool));
    //here this function will parse server related tags
    mj_process_server_related_info(json, mon_config_node, json_monitors_filepath);

    MALLOC_AND_COPY(g_monid_buf, mon_config_node->g_mon_id, (g_monid_buf_len + 1), "MonConfig g_mon_id", -1);
    MALLOC_AND_COPY(prgrm_args, mon_config_node->options, (prgrm_args_len + 1), "MonConfig Program args of Check monitor", -1);
    if(old_prgrm_args[0] != '\0')
      MALLOC_AND_COPY(old_prgrm_args, mon_config_node->old_options, (old_prgrm_args_len + 1), "MonConfig Program args of Check monitor", -1);
    MALLOC_AND_COPY(mon_name, mon_config_node->mon_name, (mon_name_len + 1), "MonConfig Mon name of Check monitor", -1);
    mon_config_node->mon_type = mon_type;
    mon_config_node->agent_type |= CONNECT_TO_CMON;
    mon_config_node->metric_priority = g_metric_priority;

    //here we extract the id tag from json which is unique for all the monitors
    mj_extract_mon_id_and_initialize_mon_config_struct(json, tier_id, tier_type, temp_tiername, mon_config_node, action_flag, 
                                                       json_monitors_filepath, runtime_flag);
    CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
  }
  
  if(ret == -1)
  {
    if(((int)json->error)!=6)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                       "Can't able to apply monitors on Tier [%s] from json [%s] due to error: %s.",
                         temp_tiername, json_monitors_filepath, nslb_json_strerror(json));
    }
    else
    {
      CLOSE_ELEMENT_ARR_OF_MONITORS_JSON_EX(json);
      ret=0;
    }
  }
  return;
}


// This function will copy the file in buffer so that we can parse the json content
int copy_json_file_in_buffer(char *json_monitors_filepath, int runtime_flag)
{
  FILE *json_fp;
  long lSize;

  NSDL2_MON(NULL, NULL, "json_file_path = %s", json_monitors_filepath);
  json_fp = fopen (json_monitors_filepath , "r" );
  if(!json_fp )
  {
    NSTL1_OUT(NULL, NULL, "Not able to open the file\n");
    return -1;
  }
  
  fseek(json_fp , 0L , SEEK_END);
  lSize = ftell( json_fp);
  rewind(json_fp);

  if((g_size_of_json_file_buf < lSize+1) || (runtime_flag == 1))
  {
    g_size_of_json_file_buf = lSize+1;
    MY_REALLOC(g_json_file_buf_ptr, (lSize + 1), "Allocating auto_json_monitors_diff_ptr", -1);
  }

  if( !g_json_file_buf_ptr )
  {
    CLOSE_FP(json_fp);
    NSTL1_OUT(NULL, NULL, "memory alloc fails for json_file_buf_ptr\n");
    return -1;
  }

  // Copy the json file into the json file buffer
  if(fread(g_json_file_buf_ptr, lSize, 1 , json_fp) != 1)
  {
    CLOSE_FP(json_fp);
    NSTL1_OUT(NULL, NULL, "Read to fail the json file %s\n", json_monitors_filepath);
    return -1;
  }
  
  CLOSE_FP(json_fp);
  return 0;
}

//This function will read individual json and add in MonitorInfo structure
int mj_read_json_and_save_struct(char *json_file, int runtime_flag)
{
  nslb_json_t *json = NULL;
  nslb_json_error err;

  char json_monitors_filepath[MAX_DATA_LINE_LENGTH];      //This variable will store the json file path
  char temp_tiername[MAX_DATA_LINE_LENGTH];               //This variable will store tier name or tier group name
  char *dummy_ptr=NULL;                                   //This pointer will extract the value from json tags
  char tier_type;                                    
  int tier_id= -1;
  int len=0;
  int ret=0;
 
  NSDL2_MON(NULL, NULL, "Method called mj_read_json_and_save_struct");
 
  //checking json file
  if(check_json_format(json_file) != 0)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                       "Error: Either filename [%s] does not exist or there are one or more error in json [%s]. Check it in json editor.",
                        json_file, json_file);
    return -1;
  }

  strncpy(json_monitors_filepath, json_file, MAX_DATA_LINE_LENGTH);
 
  // copy the file in buffer so that we can parse the json content 
  if(copy_json_file_in_buffer(json_monitors_filepath, runtime_flag) == -1)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                       "Error in problem to copy json file in buffer Json File: %s", json_file);
    return -1;
  }

  if(g_json_file_buf_ptr == NULL)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                       "Unable to process json [%s]", json_monitors_filepath);
    return -1;
  }

  //store json file in json pointer
  json = nslb_json_init_buffer(g_json_file_buf_ptr, 0, 0, &err);
  MLTL4(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "JSON =%p",json);
  if(json == NULL)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                       "Unable to convert json content of file [%s] to json structure, due to: %s.",
                        json_monitors_filepath, nslb_json_strerror(json));
    return -1;
  }
  
  OPEN_ELEMENT_OF_MONITORS_JSON(json);
  //here we read the json at tier level
  GOTO_ELEMENT_OF_MONITORS_JSON(json, "Tier", 1);
  OPEN_ELEMENT_OF_MONITORS_JSON(json);

  if(ret == -1)
  {
    if(((int)json->error)!=6)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                               "Can't able to open Tier element in json [%s] while applying json auto monitors due to error: %s.",
                                json_file, nslb_json_strerror(json));
      return -1;
    }
  }
  
  while(1)
  {
    temp_tiername[0] ='\0';
    ret = 0;
    tier_type = 1;  //if it is 1 then we get tier name not tier_group
    tier_id = -1;
    
    GOTO_NEXT_ARRAY_ITEM_OF_MONITORS_JSON(json);
    OPEN_ELEMENT_OF_MONITORS_JSON(json);
    //if tier_group is present in json
    GOTO_ELEMENT_OF_MONITORS_JSON(json, "tier_group",0);
    if(ret != -1)
    {
      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);

      strncpy(temp_tiername, dummy_ptr, MAX_DATA_LINE_LENGTH);
      if(temp_tiername[0] != '\0')
      {
        tier_type = 0;
        //here we are saving tier_group_id of tier group name
        tier_id =topolib_get_tier_group_id_from_tier_group_name(temp_tiername,topo_idx);
      }
      else
      {
        CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "ERROR:Tier Group entered is null.");
        continue;
      }
    }
    else if(ret == -1)
    {
      ret=0;

      GOTO_ELEMENT_OF_MONITORS_JSON(json, "name",1);

      if(ret==-1)
      {
        if(((int)json->error)!=6)
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                         "Can't able to open name element in json [%s] due to error: %s.",
                          json_file, nslb_json_strerror(json));
          return -1;
        }
        ret=0;
      }

      GET_ELEMENT_VALUE_OF_MONITORS_JSON(json, dummy_ptr, &len, 0, 1);
      if(ret==-1)
      {
        if(((int)json->error)!=6)
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                         "Can't able to open name element in json [%s] due to error: %s.",
                         json_monitors_filepath, nslb_json_strerror(json));
          return -1;
        }
        ret=0;
      }
      strncpy(temp_tiername, dummy_ptr, MAX_DATA_LINE_LENGTH);
      //here we are saving tier_id of tier
      //TODO:change func name
      tier_id = topolib_get_tier_id_from_tier_name(temp_tiername, topo_idx);
    }
    
    //this block is for check monitors
    ret=0;
    GOTO_ELEMENT_OF_MONITORS_JSON(json,"check-monitor",0);
    if(ret != -1)
    {
      OPEN_ELEMENT_OF_MONITORS_JSON(json);
      if(ret == -1)
      {
        if(((int)json->error)!=6)
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                         "Can't able to open check-monitor element in json [%s] while applying monitors due to error: %s.",
                         json_monitors_filepath, nslb_json_strerror(json));
          return -1;
        }
        ret=0;
      }
      else
      {
        mj_process_check_monitors(json, temp_tiername, tier_id, tier_type, PROCESS_CHECK_MON, "Check Monitor", json_monitors_filepath,
                                  runtime_flag);
      }
    }

    //this block is for batch job monitors
    ret=0;
    GOTO_ELEMENT_OF_MONITORS_JSON(json,"batch-job",0);
    if(ret != -1)
    {
      OPEN_ELEMENT_OF_MONITORS_JSON(json);
      if(ret == -1)
      {
        if(((int)json->error)!=6)
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                       "Can't able to open batch-job element in json [%s] while applying monitors due to error: %s.",
                        json_monitors_filepath, nslb_json_strerror(json));
          return -1;
        }
        ret=0;
      }
      else
      {
        mj_process_check_monitors(json, temp_tiername, tier_id, tier_type, PROCESS_BATCH_JOB_MON, "Batch Job", json_monitors_filepath,
                                  runtime_flag);
      }
    }

    //this block is for server signature monitors
    ret=0;
    GOTO_ELEMENT_OF_MONITORS_JSON(json,"server-signature",0);
    if(ret != -1)
    {
      OPEN_ELEMENT_OF_MONITORS_JSON(json);
      if(ret == -1)
      {
        if(((int)json->error)!=6)
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                       "Can't able to open server-signature element in json [%s] while applying monitors due to error: %s.",
                        json_monitors_filepath, nslb_json_strerror(json));
          return -1;
        }
        ret=0;
      }
      else
      {
        mj_process_check_monitors(json, temp_tiername, tier_id, tier_type, PROCESS_SERVER_SIGNATURE_MON, "Server Signature",
                                  json_monitors_filepath, runtime_flag);
      }
    }

    //this block is for log pattern monitors
    ret=0;
    GOTO_ELEMENT_OF_MONITORS_JSON(json,"log-pattern-mon",0);
    if(ret != -1)
    {
      OPEN_ELEMENT_OF_MONITORS_JSON(json);
      if(ret == -1)
      {
        if(((int)json->error)!=6)
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                       "Can't able to open log-pattern-mon element in json [%s] while applying monitors due to error: %s.",
                       json_monitors_filepath, nslb_json_strerror(json));
          return -1;
        }
        ret=0;
      }
      else
      {
        mj_process_log_monitors(json, PROCESS_LOG_PATTERN_MON, temp_tiername, tier_id, tier_type, "Log Pattern", json_monitors_filepath,
                                runtime_flag);
      }
    }

    //this block is for get log file monitors
    ret=0;
    GOTO_ELEMENT_OF_MONITORS_JSON(json,"get-log-file",0);
    if(ret != -1)
    {
      OPEN_ELEMENT_OF_MONITORS_JSON(json);
      if(ret == -1)
      {
        if(((int)json->error)!=6)
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                         "Can't able to open get-log-file element in json [%s] while applying monitors due to error: %s.",
                         json_monitors_filepath, nslb_json_strerror(json));
          return -1;
        }
        ret=0;
      }
      else
      {
        mj_process_log_monitors(json, PROCESS_GET_LOG_FILE_MON, temp_tiername, tier_id, tier_type, "Get Log File", json_monitors_filepath,
                                runtime_flag);
      }
    }

    //this block is for log data monitors
    ret=0;
    GOTO_ELEMENT_OF_MONITORS_JSON(json,"log-data-mon",0);
    if(ret != -1)
    {
      OPEN_ELEMENT_OF_MONITORS_JSON(json);
      if(ret == -1)
      {
        if(((int)json->error)!=6)
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                         "Can't able to open log-data-mon element in json [%s] while applying monitors due to error: %s.",
                         json_monitors_filepath, nslb_json_strerror(json));
          return -1;
        }
        ret=0;
      }
      else
      {
        mj_process_log_monitors(json, PROCESS_LOG_DATA_MON, temp_tiername, tier_id, tier_type, "Log Data", json_monitors_filepath, runtime_flag);
      }
    }

    //this block is for custom gdf monitors
    ret=0;
    GOTO_ELEMENT_OF_MONITORS_JSON(json,"custom-gdf-mon",0);
    if(ret != -1)
    {
      OPEN_ELEMENT_OF_MONITORS_JSON(json);
      if(ret == -1)
      {
        if(((int)json->error)!=6)
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                         "Can't able to open custom-gdf-mon element in json [%s] while applying monitors due to error: %s.",
                         json_monitors_filepath, nslb_json_strerror(json));
          return -1;
        }
        ret=0;
      }
      else
      {
        mj_process_custom_gdf_monitors(json, PROCESS_CUSTOM_GDF_MON, temp_tiername, tier_id, tier_type, json_monitors_filepath, runtime_flag);
      }
    }

    //this block is for standard monitors 
    ret=0;
    GOTO_ELEMENT_OF_MONITORS_JSON(json,"gdf",0);
    if(ret != -1)
    {
      OPEN_ELEMENT_OF_MONITORS_JSON(json);
      if(ret == -1)
      {
        if(((int)json->error)!=6)
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                         "Can't able to open gdf element in json [%s] while applying monitors due to error: %s.",
                         json_monitors_filepath, nslb_json_strerror(json));
          return -1;
        }
        ret=0;
      }
      else
      {
        mj_process_standard_monitors(json, PROCESS_STD_MON, temp_tiername, tier_id, tier_type, json_monitors_filepath, runtime_flag);
      }
    }
    CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);
    CLOSE_ELEMENT_ARR_OF_MONITORS_JSON(json);
    CLOSE_ELEMENT_OBJ_OF_MONITORS_JSON(json);    
  }
  if(runtime_flag)
    FREE_AND_MAKE_NULL(g_json_file_buf_ptr, "JSON file buf ptr", -1); 
  return 0;
}

// Scandir compare function that should return only directories and dont return those directories in . is present
int filter_directory(const struct dirent* file_name)
{
  if(file_name->d_type == DT_DIR && !(strstr(file_name->d_name,".")))
    return 1;
  return 0;
}

// Scandir compare function that should compare “*.json”
int filter_json_files(const struct dirent* file_name)
{
  if(file_name->d_type == DT_REG && strstr(file_name->d_name,".json"))
    return 1;
  return 0;
}


/* Name         : mj_timespec_diff
 * Description  : Calculate the difference between start and stop time and store in result
 **/
static void mj_timespec_diff(struct timespec *start, struct timespec *stop,  struct timespec *result)
{
  if((stop->tv_nsec - start->tv_nsec) < 0)
  {
    result->tv_sec = stop->tv_sec - start->tv_sec - 1;
    result->tv_nsec = stop->tv_nsec - start->tv_nsec + 1000000000;
  }
  else
  {
    result->tv_sec = stop->tv_sec - start->tv_sec;
    result->tv_nsec = stop->tv_nsec - start->tv_nsec;
  }
  return;
}

inline static void mj_read_json_dir_files(char *json_directory_path)
{
  struct dirent **FileList;

  char json_file_path_buf[512];
  int no_of_files;
  int file_idx;
  int ret=0;

  no_of_files = scandir(json_directory_path, &FileList, filter_json_files, NULL);

  if(no_of_files == -1 || no_of_files == 0)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"No json file present at given json path %s", json_directory_path);
    return;
  }

  for(file_idx=0; file_idx<no_of_files; file_idx++)
  {
    //we have to also append the full path of json_file
    sprintf(json_file_path_buf, "%s/%s", json_directory_path, FileList[file_idx]->d_name);
    MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"Going to process %s json file",json_file_path_buf);
    ret =mj_read_json_and_save_struct(json_file_path_buf, 0);
    if(ret == -1)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"%s json file is not processed",json_file_path_buf);
    }
  }
  return;
}

//This function will be called from url.c and will read all json and fill MonitorInfo structure
int mj_read_json_dir_create_config_struct(char *directory_path)
{
  struct dirent **DirList;
 
  char json_directory_path[512];
  int no_of_dirs;
  int dir_idx;

  struct timespec mj_start_time;
  struct timespec mj_end_time;
  struct timespec mj_exceution_time;
 
  NSDL2_MON(NULL, NULL, "Method called mj_read_json_dir_create_config_struct");
  //here we make mon_info_index normalized table  
  if(g_monid_hash == NULL)
  {
    MY_MALLOC(g_monid_hash, (16*1024 * sizeof(NormObjKey)), "Memory allocation to Norm g_mon_id table", -1);
    nslb_init_norm_id_table(g_monid_hash, 16*1024);
  }
  //intialize the bit library
  nslb_bitlib_init();

  //init memory pool for mon_config
  nslb_mp_init(&(mon_config_pool), sizeof(MonConfig), 50, 10, NON_MT_ENV);
  nslb_mp_create(&mon_config_pool);

  //store start time in nano second
  if (clock_gettime(CLOCK_MONOTONIC, &mj_start_time) == -1)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, "MJ:Error in clock_gettime()");
  }
  // In case of SM we get json_files in multiple directories.
  //if machine type is SM
  if(!strncmp(g_cavinfo.config, "SM", 2))
  {
    no_of_dirs = scandir(directory_path, &DirList, filter_directory, NULL);

    if(no_of_dirs == -1 || no_of_dirs == 0)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"No directory is present at given json path %s", directory_path);
      return 1;
    }
    // First we are looping the directory list
    for(dir_idx =0; dir_idx <no_of_dirs; dir_idx++)
    {
      sprintf(json_directory_path, "%s/%s", directory_path, DirList[dir_idx]->d_name);
      mj_read_json_dir_files(json_directory_path);
    }
  }
  else
  {
    mj_read_json_dir_files(directory_path);
  }
  FREE_AND_MAKE_NULL(g_json_file_buf_ptr, "JSON file buf ptr", -1);
  //end time in milli second
  if (clock_gettime(CLOCK_MONOTONIC, &mj_end_time) == -1)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, "MJ:Error in clock_gettime()");
  }
  //calculate time diff
  mj_timespec_diff(&mj_start_time, &mj_end_time, &mj_exceution_time);
 
  MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, 
                      "Time taken to parse json & populate struct in %ld nano second", mj_exceution_time.tv_nsec ); 
  return 1;
}

