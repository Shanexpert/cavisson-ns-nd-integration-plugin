
#include <stdio.h>
#include <string.h>
#include <v1/topolib_structures.h>
#include <sys/epoll.h>
#include "url.h"
#include "util.h"
#include "ns_log.h"
#include "ns_trace_level.h"
#include "ns_global_settings.h"
#include "ns_custom_monitor.h"
#include "ns_gdf.h"
#include "ns_msg_com_util.h"
#include "ns_ndc.h"
#include "ns_alloc.h"
#include "ns_dynamic_vector_monitor.h"
#include "ns_custom_monitor.h"
#include "wait_forever.h"
#include "ns_ndc_outbound.h"
#include "ns_pre_test_check.h"
#include "ns_mon_log.h"
#include "ns_event_id.h"
#include "ns_string.h"
#include "ns_event_log.h"
#include "ns_user_monitor.h"
#include "ns_standard_monitors.h"
#include "ns_check_monitor.h"
#define SUCCESSFULLY_SWAP_PARENT_NS_MON 0
#define SUCCESSFULLY_SWAP_PARENT_ND_MON 1
#define ND_MON_ALL_VECTOR_DELETED 2
#define NS_MON_ALL_VECTOR_DELETED -1

int g_time_to_get_inactive_inst = 0;
char g_enable_new_gdf_on_partition_switch = 0;
extern void initialize_dyn_mon_vector_groups(CM_info *dyn_cm_start_ptr, int start_indx, int total_entries);
void kw_set_enable_auto_scale_cleanup_setting(char *buffer)
{
  char key[1024] = {0};
  int time = 0;
  int mode = 0;
  if(sscanf(buffer, "%s %d %d", key, &mode, &time) < 3)
  {
     MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Given argument for keyword AUTO_SCALE_CLEANUP_SETTING is less than expected, therefore going to set default value 0 0.");
     g_enable_new_gdf_on_partition_switch = 0;
     g_time_to_get_inactive_inst = 0;
  }
  else {
     g_time_to_get_inactive_inst = time * 1000;
     g_enable_new_gdf_on_partition_switch = mode;
  }
  MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "set g_time_to_get_inactive_inst = %d, g_enable_new_gdf_on_partition_switch = %d", g_time_to_get_inactive_inst, g_enable_new_gdf_on_partition_switch);
}

void set_dvm_idx_mapping()
{
  int i, k, relative_dyn_idx = 0;
  CM_info *cm_ptr = NULL;
  CM_vector_info *vector_list = NULL;

  for(i = 0; i < total_rtgRewrt_monitor_list_entries; i++)
  {
    cm_ptr = rtgRewrt_monitor_list_ptr[i].cm_info_mon_conn_ptr;
    vector_list = cm_ptr->vector_list;
    relative_dyn_idx = 0;

    /*if(cm_ptr->dvm_cm_mapping_tbl_row_idx != -1)
    {
      //memset each row of dvm idx mapping table
      memset(dvm_idx_mapping_ptr[cm_ptr->dvm_cm_mapping_tbl_row_idx], -1, (cm_ptr->max_mapping_tbl_vector_entries * sizeof(DvmVectorIdxMappingTbl)));
    }*/

    for(k = 0; k < cm_ptr->total_vectors; k++)
    {
      MLTL3(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "relative_dyn_idx = %d, k = %d, rtgRewrt_monitor_list_ptr[i].dvm_cm_mapping_tbl_row_idx = %d", relative_dyn_idx, k, cm_ptr->dvm_cm_mapping_tbl_row_idx);
      if((cm_ptr->dvm_cm_mapping_tbl_row_idx != -1))//dynamic vector monitor
      {
        if(vector_list[k].vector_state == CM_VECTOR_RESET)
        {
          relative_dyn_idx++;
          continue;
        }

        if(vector_list[k].vectorIdx >= 0)
        {
          //reset is_data_filled of each vector in mapping table on every merging
          //dvm_idx_mapping_ptr[cm_ptr->dvm_cm_mapping_tbl_row_idx][vector_list[k].vectorIdx].is_data_filled = -1;
 
          //update relative index in mapping index
          dvm_idx_mapping_ptr[cm_ptr->dvm_cm_mapping_tbl_row_idx][vector_list[k].vectorIdx].relative_dyn_idx = relative_dyn_idx;
 
          relative_dyn_idx++;
        }
      }
    }
    
    if(is_outbound_connection_enabled)
    {
      if(cm_ptr->mon_id >= 0)
        mon_id_map_table[cm_ptr->mon_id].mon_index = i;
    }

    if(!g_delete_vec_freq_cntr)
    {
      cm_ptr->total_deleted_vectors = 0;
    }

    cm_ptr->total_reused_vectors = 0;
  }
}

/*void make_new_parent(CM_info * cm_info, int i)
{
  int dbuf_len;
  NSTL1(NULL, NULL, "calling make_new_parent for cm_info->vector_name [%s] .New parent is [%s]", cm_info->vector_name, (cm_info + i)->vector_name); 
  (cm_info + i)->fd = cm_info->fd; 
  (cm_info + i)->is_dynamic = cm_info->is_dynamic;
  (cm_info + i)->num_dynamic_filled = 0;
  (cm_info + i)->dyn_num_vectors = cm_info->dyn_num_vectors - i;
  (cm_info + i)->is_group_vector = cm_info->is_group_vector;
  (cm_info + i)->group_vector_idx = cm_info->group_vector_idx;
  (cm_info + i)->group_element_size = cm_info->group_element_size;
  //malloc of data_buf are also done in set_cm_info_values but we are malloc data_buf and copy buffer here to care of partial buffer
  dbuf_len = MAX_MONITOR_BUFFER_SIZE + 1;
  MY_MALLOC((cm_info + i)->data_buf, dbuf_len, "Custom Monitor data buf", -1);
  memset((cm_info + i)->data_buf, 0, dbuf_len);
  memcpy((cm_info + i)->data_buf, cm_info->data_buf, cm_info->dindex);
  (cm_info + i)->dindex = cm_info->dindex;
  (cm_info + i)->instanceVectorIdxTable = cm_info->instanceVectorIdxTable;
  (cm_info + i)->tierNormVectorIdxTable = cm_info->tierNormVectorIdxTable;
  (cm_info + i)->cur_tierIdx_TierNormVectorIdxTable = cm_info->cur_tierIdx_TierNormVectorIdxTable;
  (cm_info + i)->cur_normVecIdx_TierNormVectorIdxTable = cm_info->cur_normVecIdx_TierNormVectorIdxTable;
  (cm_info + i)->cur_instIdx_InstanceVectorIdxTable = cm_info->cur_instIdx_InstanceVectorIdxTable;
  (cm_info + i)->cur_vecIdx_InstanceVectorIdxTable = cm_info->cur_vecIdx_InstanceVectorIdxTable;
  (cm_info + i)->instanceIdxMap = cm_info->instanceIdxMap;
  (cm_info + i)->tierIdxmap = cm_info->tierIdxmap;
  (cm_info + i)->ndVectorIdxmap = cm_info->ndVectorIdxmap;
  memcpy((cm_info + i)->save_times_graph_idx, cm_info->save_times_graph_idx, 128);
  memcpy(&((cm_info + i)->key), &(cm_info->key), sizeof(cm_info->key));
  (cm_info + i)->is_norm_init_done = cm_info->is_norm_init_done;
  (cm_info + i)->dummy_data = cm_info->dummy_data;
}
  This function will return 1 On successfully swap parent in case of nd
  return 2 when all vector of nd mon is deleted
  return 0 On successfully swap parent in case of NS mon
  return -1 when all vector is deleted in case of ns mon

int swap_parent(CM_info *src_cm_ptr)
{
  int i;
  char tmp_vector_name[2048] = {0} ,tmp_breadcrumb[2048] = {0};
  long tmp_rtg_index = 0;
  int tmp;
  char tmp_char, tmp_conn_state, nd_mon = 0;

  //check if this is nd mon
  if((src_cm_ptr->is_nd_vector==1) || (strstr(src_cm_ptr->gdf_name,"cm_nd_integration_point_status.gdf")) || (strstr(src_cm_ptr->gdf_name,"cm_nd_entry_point_stats.gdf")) || (strstr(src_cm_ptr->gdf_name, "cm_nd_db_query_stats.gdf")))
  {
     nd_mon = 1;
  }

  for(i = 1; i < src_cm_ptr->dyn_num_vectors; i++)
  {
    if((src_cm_ptr + i)->conn_state != CM_DELETED)
    {
      if(src_cm_ptr->is_nd_vector)
      {
        make_new_parent(src_cm_ptr, i);
        return SUCCESSFULLY_SWAP_PARENT_ND_MON;
      }
      else 
      {
        NSTL1(NULL, NULL, "Going to swap parent(%s) and child(%s)", src_cm_ptr->mon_breadcrumb, (src_cm_ptr + i)->mon_breadcrumb);
        strcpy(tmp_breadcrumb, (src_cm_ptr + i)->mon_breadcrumb);
        REALLOC_AND_COPY(src_cm_ptr->mon_breadcrumb, (src_cm_ptr + i)->mon_breadcrumb, strlen(src_cm_ptr->mon_breadcrumb) + 1, "Copying parent breadcrumb to reused vector", i);
        REALLOC_AND_COPY(tmp_breadcrumb, src_cm_ptr->mon_breadcrumb, strlen(tmp_breadcrumb) + 1, "Copying reused vector breadcrumb to parent", i);
      
        strcpy(tmp_vector_name, (src_cm_ptr + i)->vector_name);
        REALLOC_AND_COPY(src_cm_ptr->vector_name, (src_cm_ptr + i)->vector_name, strlen(src_cm_ptr->vector_name) + 1, "Copying parent vector name to reused vector", i);
        REALLOC_AND_COPY(tmp_vector_name, src_cm_ptr->vector_name, strlen(tmp_vector_name) + 1, "Copying reused vector name to parent vector", i);
      
        tmp_rtg_index = (src_cm_ptr + i)->rtg_index;
        (src_cm_ptr + i)->rtg_index = src_cm_ptr->rtg_index;
        src_cm_ptr->rtg_index = tmp_rtg_index;
      
        if((src_cm_ptr + i)->dvm_cm_mapping_tbl_row_idx != -1)
        {
          tmp = dvm_idx_mapping_ptr[(src_cm_ptr + i)->dvm_cm_mapping_tbl_row_idx][(src_cm_ptr + i)->vectorIdx].relative_dyn_idx;
          dvm_idx_mapping_ptr[(src_cm_ptr + i)->dvm_cm_mapping_tbl_row_idx][(src_cm_ptr + i)->vectorIdx].relative_dyn_idx = dvm_idx_mapping_ptr[src_cm_ptr->dvm_cm_mapping_tbl_row_idx][src_cm_ptr->vectorIdx].relative_dyn_idx;
          dvm_idx_mapping_ptr[src_cm_ptr->dvm_cm_mapping_tbl_row_idx][src_cm_ptr->vectorIdx].relative_dyn_idx = tmp;
        }
      
        tmp_char = (src_cm_ptr + i)->runtime_and_copy_flag;
        src_cm_ptr->runtime_and_copy_flag = tmp_char;
      
        tmp = (src_cm_ptr + i)->cm_retry_attempts;
        (src_cm_ptr + i)->cm_retry_attempts = src_cm_ptr->cm_retry_attempts;
        src_cm_ptr->cm_retry_attempts = tmp;
      
        tmp_conn_state = src_cm_ptr->conn_state;
        src_cm_ptr->conn_state = (src_cm_ptr + i)->conn_state;
        (src_cm_ptr + i)->conn_state = tmp_conn_state;
      
        tmp = (src_cm_ptr + i)->vectorIdx;
        (src_cm_ptr + i)->vectorIdx = src_cm_ptr->vectorIdx;
        src_cm_ptr->vectorIdx = tmp;
      
        return SUCCESSFULLY_SWAP_PARENT_NS_MON;
      }
    }
  }
  
  if(nd_mon == 1)
    return ND_MON_ALL_VECTOR_DELETED;
  else
    return NS_MON_ALL_VECTOR_DELETED;
}
void make_dummy(CM_info *cm_ptr)
{
   NSTL1(NULL, NULL,"Calling make_dummy funtion");
   char vector_name[128];
   cm_ptr->flags |= ALL_VECTOR_DELETED;
   sprintf(cm_ptr->monitor_name, "");
   cm_ptr->conn_state = CM_CONNECTED;
    
   if(strstr(cm_ptr->gdf_name, "cm_nd_backend_call_stats.gdf") != NULL) 
     sprintf(vector_name, "NDEnableBackendCallMon");
   else if(strstr(cm_ptr->gdf_name, "cm_nd_bt_ip.gdf") != NULL)
     sprintf(vector_name, "NDBTIPMON");
   else if(strstr(cm_ptr->gdf_name, "cm_nd_bt.gdf") != NULL)
     sprintf(vector_name, "NDBTMON");
   else if(strstr(cm_ptr->gdf_name, "cm_nd_exception_stats.gdf"))
     sprintf(vector_name, "NDExceptionsMon");
   else if(strstr(cm_ptr->gdf_name, "cm_nd_http_header_stats.gdf"))
     sprintf(vector_name, "NDHTTPHeaderCapture");
   else if(strstr(cm_ptr->gdf_name, "cm_nd_thread_hot_spot_data.gdf"))
     sprintf(vector_name, "NDHotSpotThread");
   else if(strstr(cm_ptr->gdf_name, "cm_nd_nodejs_server_monitor.gdf"))
     sprintf(vector_name, "NDNodejsServer");
   else if(strstr(cm_ptr->gdf_name, "cm_nd_nodejs_async_event.gdf"))
     sprintf(vector_name, "NDNodejsAsyncEvent");
   else if(strstr(cm_ptr->gdf_name, "cm_nd_method_stats.gdf"))
     sprintf(vector_name, "NDMethodMon");
   else if(strstr(cm_ptr->gdf_name, "cm_nd_jvm_thread_monitor.gdf"))
     sprintf(vector_name, "ND_ENABLE_CPU_BY_THREAD");
   else if(strstr(cm_ptr->gdf_name, "cm_nd_nodejs_event_loop.gdf"))
     sprintf(vector_name, "NDEnableEventLoopMon");
   else if(strstr(cm_ptr->gdf_name, "cm_nd_nodejs_gc.gdf"))
     sprintf(vector_name, "NDEnableNodeGCMon");
   else if(strstr(cm_ptr->gdf_name, "cm_nd_fp_stats.gdf"))
     sprintf(vector_name, "NDFlowPath");
   else if(strstr(cm_ptr->gdf_name, "cm_nd_entry_point_stats.gdf"))
     sprintf(vector_name, "NDEntryPointMon");
   else
     make_vector_name_from_gdf(cm_ptr->gdf_name, vector_name);

   sprintf(cm_ptr->mon_breadcrumb, "%s", vector_name);
   cm_ptr->dyn_num_vectors = 1;
   NSTL1(NULL, NULL, "mon_breadcrumb is %s", cm_ptr->mon_breadcrumb);

}*/

void do_vector_cleanup_at_partition_switch(CM_info *cm_ptr)
{
  int vec_idx, rtgRewrt_vec_idx=0;
  int norm_id = 0;
  CM_vector_info *rtgRewrt_vector_list_ptr = NULL;
  CM_vector_info *vector_list = cm_ptr->vector_list;
  int total_deleted_vectors = 0; 

  MY_MALLOC_AND_MEMSET(rtgRewrt_vector_list_ptr, (sizeof(CM_vector_info) * cm_ptr->total_vectors), "", -1);
  
  for(vec_idx = 0; vec_idx < cm_ptr->total_vectors; vec_idx++)
  {
    if(vector_list[vec_idx].flags & WAITING_DELETED_VECTOR)
    {
      cm_ptr->deleted_vector[total_deleted_vectors++] = rtgRewrt_vec_idx;
    }      
    
    if(vector_list[vec_idx].vector_state == CM_DELETED)
    {
      free_vec_lst_row(&(vector_list[vec_idx]));
      continue;
    }
    else
    {
      if(cm_ptr->flags & ND_MONITOR || (cm_ptr->nd_norm_table.size > 0))
      {
        //No need to compare for duplicate entries in norm table because no new vector is coming, we are only cleaning up the structures.
        int ret = nslb_set_norm_id(&cm_ptr->nd_norm_table, vector_list[vec_idx].vector_name, strlen(vector_list[vec_idx].vector_name), norm_id);
        if(ret != norm_id)  //It should always match.
        {
          MLTL1(EL_DF, 0, 0, _FLN_, cm_ptr, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error: This should not happen. Vector name: %s, GDF: %s, ret = %d, norm_id = %d", vector_list[vec_idx].vector_name, cm_ptr->gdf_name, ret, norm_id);
          continue;
        }
        MLTL3(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                                        "Going to create vector_norm_table at partition switch. mon_name = %s, gdf_name = %s, vector_name = %s, vector_idx = %d", cm_ptr->monitor_name, cm_ptr->gdf_name, vector_list[vec_idx].vector_name, vec_idx);
        norm_id ++;
      }
      else
      {
        MLTL3(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                                                    "Not Going to create vector_norm_table at partition switch. mon_name = %s, gdf_name = %s ",
                                                     cm_ptr->monitor_name, cm_ptr->gdf_name);
      }

      memcpy(&(rtgRewrt_vector_list_ptr[rtgRewrt_vec_idx]), &(vector_list[vec_idx]), sizeof(CM_vector_info));
    }
    rtgRewrt_vec_idx++;
  }
  FREE_AND_MAKE_NULL(cm_ptr->vector_list, "cm_ptr->vector_list", -1);
  cm_ptr->vector_list = rtgRewrt_vector_list_ptr;
  cm_ptr->max_vectors = cm_ptr->total_vectors;
  cm_ptr->total_vectors = rtgRewrt_vec_idx;
  cm_ptr->total_deleted_vectors = total_deleted_vectors;
  cm_ptr->new_vector_first_index = -1;

  return;
}

//this function will do the cleanup for check monitors if status is CHECK_MONITOR_STOPPED at every partition switch 
void check_mon_cleanup()
{
  int mon_id;
  CheckMonitorInfo *check_mon_ptr = NULL;
  int total_check_mon_entries = 0;

  NSDL1_MON(NULL, NULL, "Method called\n");

  NSDL1_MON(NULL, NULL, "Before cleanup of check mon total_check_monitors_entries = %d \n", total_check_monitors_entries);

  MY_MALLOC_AND_MEMSET(check_mon_ptr, (sizeof(CheckMonitorInfo) * total_check_monitors_entries), "", -1)
  
  for(int num_aborted = 0; num_aborted < g_total_aborted_chk_mon_conn; num_aborted ++)
        check_monitor_info_ptr[num_aborted].conn_state = COPY_CHK_TO_DR; 

  g_total_aborted_chk_mon_conn = 0;
 
  for (mon_id = 0; mon_id < total_check_monitors_entries; mon_id++)
  {
    if(check_monitor_info_ptr[mon_id].status != CHECK_MONITOR_STOPPED)
    {
      /*if(create_table_entry(&check_mon_row, &total_check_mon_entries, &max_check_mon_entries, (char **)&check_mon_ptr, sizeof(CheckMonitorInfo), "Check Monitor Table") == -1)
      {
        NSDL1_MON(NULL, NULL, "Could not create table entry for Check Monitor Table\n");
      }*/
        
      REMOVE_SELECT_MSG_COM_CON(check_monitor_info_ptr[mon_id].fd, DATA_MODE);
      memcpy(&check_mon_ptr[total_check_mon_entries], &check_monitor_info_ptr[mon_id], sizeof(CheckMonitorInfo));
      ADD_SELECT_MSG_COM_CON((char *)&check_mon_ptr[total_check_mon_entries], check_mon_ptr[total_check_mon_entries].fd, EPOLLOUT | EPOLLERR | EPOLLHUP, DATA_MODE);
      if (check_mon_ptr[mon_id].conn_state == COPY_CHK_TO_DR) 
        chk_mon_update_dr_table(&check_mon_ptr[mon_id]);
      total_check_mon_entries++;
    }
  } 

  FREE_AND_MAKE_NULL(check_monitor_info_ptr, "monitor_list_ptr", -1);

  check_monitor_info_ptr = check_mon_ptr;
  max_check_monitors_entries = total_check_monitors_entries;
  total_check_monitors_entries = total_check_mon_entries;

  NSDL1_MON(NULL, NULL, "After cleanup of check mon total_check_monitors_entries = %d \n", total_check_monitors_entries);
}

void merge_monitor_list_into_rtgRewrt_monitor_list(Monitor_list *source_cm_ptr, Monitor_list *destination_cm_ptr, int num_source_cm_to_copy)
{ 
  int start_source_idx = 0, start_destination_idx = 0;
  int destination_idx = start_destination_idx;
  int source_idx;
  int total_mon_id_put = 0;
  int index_arr[6] = {-1, -1 ,-1, -1, -1, -1};
  int gdf_flag_arr[6]={NA_SR, NA_GENERICDB_MYSQL, NA_GENERICDB_ORACLE, NA_GENERICDB_POSTGRES, NA_GENERICDB_MSSQL, NA_GENERICDB_MONGO_DB};
  CM_info *cm_ptr = NULL;
  MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Method Called, source_cm_ptr = %p, start_source_idx = %d, destination_cm_ptr = %p, start_destination_idx = %d, "
                        "num_source_cm_to_copy = %d, total_monitor_list_entries = %d",
                        source_cm_ptr, start_source_idx, destination_cm_ptr, start_destination_idx, num_source_cm_to_copy,
                        total_monitor_list_entries);
  
  //memset cm_dr_table so that we can update pointer during merging
  if(cm_dr_table)
    memset(cm_dr_table, 0, (sizeof(CM_info *) * max_dr_table_entries));
 
  
  g_total_aborted_cm_conn = 0;

  for(source_idx = 0; source_idx < num_source_cm_to_copy; source_idx++)
  {
    NSDL4_MON(NULL, NULL, "source_idx = %d, destination_idx = %d, source_idx = %d", source_idx, destination_idx, source_idx);

    cm_ptr = source_cm_ptr[source_idx].cm_info_mon_conn_ptr;

    //Cleaning norm table.
    if(cm_ptr->flags & ND_MONITOR || (cm_ptr->nd_norm_table.size > 0))
    {
       MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                    "Going to destroy vector_norm_table at partition switch. mon_name = %s, gdf_name = %s",
                     cm_ptr->monitor_name, cm_ptr->gdf_name);
       nslb_obj_hash_destroy(&(cm_ptr->nd_norm_table));

       if(cm_ptr->flags & ND_MONITOR)
         nslb_init_norm_id_table(&cm_ptr->nd_norm_table, 128*1024);  //initialize norm_id_table
       else
         nslb_init_norm_id_table(&cm_ptr->nd_norm_table, cm_ptr->hash_size);
    }
    else
    {
       MLTL3(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                           "Not Going to destroy vector_norm_table at partition switch Not DVM. mon_name = %s, gdf_name = %s",
                                                   cm_ptr->monitor_name, cm_ptr->gdf_name);
    }  
      
     //clean inactive server list for outbound.
    if((cm_ptr->tier_index >= 0) && (cm_ptr->server_index >= 0) && (!topo_info[topo_idx].topo_tier_info[cm_ptr->tier_index].topo_server[cm_ptr->server_index].server_ptr->server_flag & DUMMY) && (topo_info[topo_idx].topo_tier_info[cm_ptr->tier_index].topo_server[cm_ptr->server_index].used_row != -1) && topo_info[topo_idx].topo_tier_info[cm_ptr->tier_index].topo_server[cm_ptr->server_index].server_ptr->status == 0 )
       reset_monitor_config_in_server_list(cm_ptr->tier_index,cm_ptr->server_index); 

    if(cm_ptr->conn_state == CM_DELETED)
    {
       if((cm_ptr->dvm_cm_mapping_tbl_row_idx != -1) && (cm_ptr->max_mapping_tbl_vector_entries > 0))
       {
         memset(dvm_idx_mapping_ptr[cm_ptr->dvm_cm_mapping_tbl_row_idx], -1, (cm_ptr->max_mapping_tbl_vector_entries * sizeof(DvmVectorIdxMappingTbl)));
       }

       if(is_outbound_connection_enabled) {
         mon_id_map_table[cm_ptr->mon_id].state = CLEANED_MONITOR;
         put_free_id(&mon_id_pool, cm_ptr->mon_id);
         mon_id_map_table[cm_ptr->mon_id].mon_index = -1;
         total_mon_id_put++;
         MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
           "mon_id: %d returned to pool at partition switch. mon_name = %s, gdf_name = %s", cm_ptr->mon_id, cm_ptr->monitor_name,
           cm_ptr->gdf_name);
       }


       free_mon_lst_row(source_idx);
       continue;
    }
    else
    {
      memcpy(&destination_cm_ptr[destination_idx], &source_cm_ptr[source_idx], sizeof(Monitor_list));
      destination_cm_ptr[destination_idx].no_of_monitors = 0;
      do_vector_cleanup_at_partition_switch(destination_cm_ptr[destination_idx].cm_info_mon_conn_ptr);
    } 
   
    if(global_settings->generic_mon_flag)
    { 
      if((destination_cm_ptr[destination_idx].cm_info_mon_conn_ptr->genericDb_gdf_flag  == NA_SR))
        index_arr[0] = 1;
      else if((destination_cm_ptr[destination_idx].cm_info_mon_conn_ptr->genericDb_gdf_flag  == NA_GENERICDB_MYSQL))
        index_arr[1] = 1;
      else if(destination_cm_ptr[destination_idx].cm_info_mon_conn_ptr->genericDb_gdf_flag  == NA_GENERICDB_ORACLE)
        index_arr[2] = 1;
      else if(destination_cm_ptr[destination_idx].cm_info_mon_conn_ptr->genericDb_gdf_flag  == NA_GENERICDB_POSTGRES)
        index_arr[3] = 1;
      else if(destination_cm_ptr[destination_idx].cm_info_mon_conn_ptr->genericDb_gdf_flag  == NA_GENERICDB_MSSQL)
        index_arr[4] = 1;
      else if(destination_cm_ptr[destination_idx].cm_info_mon_conn_ptr->genericDb_gdf_flag  == NA_GENERICDB_MONGO_DB)
        index_arr[5] = 1;
    }

    total_rtgRewrt_monitor_list_entries++;
    destination_idx++;
  }
  
  for(int i = 0 ; i < 6;i++) 
  {
    if(index_arr[i] == 1)
      create_and_fill_hidden_ex(gdf_flag_arr[i], 1, NULL);
    else
    {
      if(global_settings->generic_mon_flag == 1)
        remove_hidden_file(gdf_flag_arr[i]);
    }
  }
  MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION, "Total mon id put to pool: %d", total_mon_id_put);
  
  //initialize_dyn_mon_vector_groups(&(destination_cm_ptr[0]), 0, total_rtgRewrt_monitor_list_entries);
  set_dvm_idx_mapping();
}

void memset_group_data_ptr_of_mon()
{
 int i;
 for(i = ns_gp_end_idx; i < total_groups_entries; i++)
   memset(&group_data_ptr[i], 0, sizeof(Group_Info));

 total_groups_entries = ns_gp_end_idx;
 group_count = ns_gp_count;
}

void send_msg_to_get_inactive_instance_to_ndc(int th_flag)
{
  char msg[1024];

  if (global_settings->net_diagnostics_mode && (th_flag == DATA_MODE))
    return;
  else if (!global_settings->net_diagnostics_mode && (th_flag == CONTROL_MODE))
    return;

  if(global_settings->net_diagnostics_mode)
   sprintf(msg, "%s","nd_control_req:action=get_inactive_instances;\n");
  else
   sprintf(msg, "%s","nd_data_req:action=get_inactive_instances;\n");

  MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "call send_msg_to_get_inactive_instance_to_ndc %s", msg);

  if(send_msg_to_ndc(msg) == 0)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "successfully send %s to ndc", msg);
  }
}

void reset_mon_rtgRewrt_tables()
{
  MLTL3(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Method Called");

  FREE_AND_MAKE_NULL(monitor_list_ptr, "monitor_list_ptr", -1);

  monitor_list_ptr = rtgRewrt_monitor_list_ptr;
  max_monitor_list_entries = total_monitor_list_entries;
  total_monitor_list_entries = total_rtgRewrt_monitor_list_entries;

  rtgRewrt_monitor_list_ptr = NULL;
  total_rtgRewrt_monitor_list_entries = 0;

  //free_gdf_tables(); //free gdf tables
  MLTL3(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Method End"); 
}

