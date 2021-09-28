


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <v1/topolib_structures.h>
#include "ns_custom_monitor.h"
#include "ns_common.h"
#include "nslb_util.h"
#include "ns_data_types.h"
#include "ns_global_settings.h"
#include "ns_log.h"
#include "ns_alloc.h"
#include "ns_trace_level.h"
#include "ns_gdf.h"
#include "ns_event_log.h"
#include "ns_event_id.h"
#include "ns_string.h"
#include "ns_monitor_2d_table.h"
#include "ns_check_monitor.h"
#include "ns_dynamic_vector_monitor.h"
#include "ns_user_monitor.h"
#include "ns_mon_log.h"
#include "ns_appliance_health_monitor.h"
#include "ns_ndc.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"
#include "ns_tsdb.h"
#include "ns_monitor_profiles.h"
#define VECTOR_NAME_MAX_LEN 512
#define MAX_LONG_LONG_VALUE 0xffffffffffffffffll

extern void fill_agg_data(CM_info *cus_mon_ptr, double *data, int tieridx, int normvecidx, Group_Info *local_group_ptr, Graph_Info *local_graph_ptr);
unsigned long get_ms_stamp();



/*int validate_and_compare_ids(int instanceidx, unsigned short tieridx, char *fields[])
{
  int i;

  for (i = 0; i < total_instance_table_entries; i++)
  {
    //if instance matches & its server also matches then consider this found else not
    //if((!strcmp(topo_instance_info[i].InstanceName, instance)) && (topolib_chk_server_ip_port(server, port, topo_server_info[topo_instance_info[i].topo_serverinfo_idx].ServerName, NULL, NULL, 0) == 1))
    if(!strcmp(topo_instance_info[i].displayName, fields[2]))
    {
      unsigned short ServerIdx = topo_instance_info[i].topo_serverinfo_idx;
      unsigned short TierIdx = topo_instance_info[i].tierinfo_idx;

      if(strcmp(topo_server_info[ServerIdx].ServerDispName, fields[1]) != 0)
        return -1;

      if(strcmp(topo_tier_info[TierIdx].TierName, fields[0]) != 0)
        return -1;

      if(topo_instance_info[i].tierid != tieridx || topo_instance_info[i].InstanceId != instanceidx)
        return -1;

      return 0;
    }
  }
  return -1;
}

*/

int validate_and_compare_ids(int instanceidx, unsigned short tieridx, char *fields[])
{
  int i,j;
  for(i=0;i<topo_info[topo_idx].total_tier_count;i++)
  {
    for(j=0;j<topo_info[topo_idx].topo_tier_info[i].total_server;j++)
    {  
      if(topo_info[topo_idx].topo_tier_info[i].topo_server[j].used_row != -1)
      {
        TopoServerInfo *server_ptr     = topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr;
        TopoInstance *tmp_instance_ptr = topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->instance_head;
   
        if(!strcmp(server_ptr->instance_head->display_name,fields[2]))
        {
          tmp_instance_ptr=tmp_instance_ptr->next_instance;
        }
        else
        {
          if(strcmp(server_ptr->server_disp_name,fields[1]) != 0)
            return -1;
          if(strcmp(topo_info[topo_idx].topo_tier_info[i].tier_name,fields[0]) != 0)
            return -1;
          if(topo_info[topo_idx].topo_tier_info[i].tier_id_tf != tieridx || tmp_instance_ptr->instance_id_tf != instanceidx)
            return -1;
       
           return 0;
        }
      } 
    }
  }
  return -1; 
}






/********************************************************************************************************
parse_vector_new_format ()

char *vector_line - Input
char *vector_name - Output
short *tieridx - Output
short *instanceidx - Output
short *vectoridx - Output
char *backend_name - Output

returns int (0 for success, -1 error)
********************************************************************************************************/

int enable_nd_data_validation, g_nd_max_tps, g_nd_max_resp, g_nd_max_count;

extern int validate_nd_data(CM_info *cus_mon_ptr, char *vector_name, char *buffer);

// TODO : will not get correct backend name in case we get some extra data after 'T>S>I>M'
int parse_vector_new_format (char *vector_line, char *vector_name, int *tieridx, int *instanceidx, int *vectoridx, char *backend_name, char *data, CM_info *cm_ptr, int expected_field, char *gdf_name)
{

  int num_field = 0;
  int temp_tier_id, ret;
  int temp_instance_id;
  char *fields[10];
  //char *ptr = NULL;
  char *pointer = NULL;
  char temp_vector_name[VECTOR_NAME_MAX_LEN + 1] = {0};
  char hv_separator[2] = {0};
  hv_separator[0] = global_settings->hierarchical_view_vector_separator;
  

  //global global_settings->hierarchical_view_vector_separator
  //TID|InstID|VectorID:T>S>I>BackendName
  if( !vector_line || !vector_name || !tieridx || !instanceidx || !vectoridx || !backend_name)
    return -1;
  //replacing colon with pipe just to tokenize easily 

  if((pointer = strchr(vector_line, ':')) == NULL)
         return -1;
 
  if(pointer)
    *pointer = '|';

  num_field = get_tokens_with_multi_delimiter(vector_line, fields, "|", 10);
  
  //To handle invalid vector format
  if(num_field > expected_field)
    return -1;

  /*if (!((num_field == expected_field)              //First sample data (tId|iId|vId:vName|data)
       || (num_field == (expected_field - 1))))    //2nd+ sample date (tId|iId|vId|data)*/
  if((num_field != expected_field) && (num_field != (expected_field - 1)))
    return -1;
  
  //we got 5 fields now set value
  temp_tier_id = atoi(fields[0]);
  temp_instance_id = atoi(fields[1]);
  *vectoridx = atoi(fields[2]);

  //if((temp_tier_id >= 64*1024)|| (temp_instance_id >= 64*1024) || (temp_tier_id < 0) || (temp_instance_id < 0))
  if((temp_tier_id >= 64*1024)|| (temp_tier_id < 0) || (temp_instance_id < 0) || (*vectoridx < 0))
    return -1; 
  else
  {
    *tieridx = (unsigned short) temp_tier_id;
    *instanceidx =  temp_instance_id;
  }  
  if(num_field == 5)
  {
    strncpy(vector_name, fields[3], VECTOR_NAME_MAX_LEN - 1);
    vector_name[VECTOR_NAME_MAX_LEN - 1] = '\0';
    if(data)
    {
      strncpy(data, fields[4], VECTOR_NAME_MAX_LEN - 1);  
      data[VECTOR_NAME_MAX_LEN - 1] = '\0';
    }
  }
  if(num_field == 4)
  {
    if(cm_ptr == NULL)
    {
     strncpy(vector_name, fields[3], VECTOR_NAME_MAX_LEN - 1);
     vector_name[VECTOR_NAME_MAX_LEN - 1] = '\0';
    }
    else
    {
      strncpy(data, fields[3], VECTOR_NAME_MAX_LEN - 1);
      data[VECTOR_NAME_MAX_LEN - 1] = '\0';
    }
  }
  
  if(vector_name[0] == '\0')
    return 0;

  strcpy(temp_vector_name, vector_name);
  num_field = get_tokens_with_multi_delimiter(temp_vector_name, fields, hv_separator, 5);

  if(num_field < 3 && (strstr(gdf_name, "cm_nd_win_") == NULL))
    return -1;

  if(g_validate_nd_vector)
  {
    if(cm_ptr)
    {
      //Check whether breadcrumb is coming in correct format and with correct no of fields with respect to their gdf's
      if(cm_ptr->breadcrumb_level <= 0 )
        get_breadcrumb_level(cm_ptr);
    
      if(num_field == cm_ptr->breadcrumb_level) 
      {
        //Check if instance name, server name and tier name is correct as per the topologyand instance id.
        ret=validate_and_compare_ids(*instanceidx, *tieridx, fields);
        if(ret < 0)
          return -1;
      }
      else
      {
        return -1;
      }
    }
  }
  

  /*ptr = strrchr(vector_name, global_settings->hierarchical_view_vector_separator);
  if(!ptr)
    return -1;

  if((cm_ptr) && strstr(cm_ptr->gdf_name, "cm_nd_bt_ip.gdf"))
  {
    ptr--;
    while(*ptr != global_settings->hierarchical_view_vector_separator)
      ptr--;
  }
  ptr++;

  strncpy(backend_name, ptr, VECTOR_NAME_MAX_LEN - 1);*/
 
    if((cm_ptr) && strstr(cm_ptr->gdf_name, "cm_nd_bt_ip.gdf"))
    {
      sprintf(backend_name,"%s%c%s",fields[num_field - 2],global_settings->hierarchical_view_vector_separator,fields[num_field - 1]);
    }
    else
    {
      strncpy(backend_name, fields[num_field - 1], VECTOR_NAME_MAX_LEN - 1);
    }
  backend_name[VECTOR_NAME_MAX_LEN - 1] = '\0';
  return 0;
  
 

/*
  char *ptr = vector_line;

  char *ptr2 = strchr(ptr, '|');
  if (!ptr2)
    goto error_return;

  *ptr2 = '\0';
  *tieridx = atoi(ptr);

  ptr = ptr2 + 1;
  if(!ptr)
    goto error_return;

  ptr2 = strchr(ptr, '|');

  if (!ptr2)
    goto error_return;

  *ptr2 = '\0';
  *instanceidx = atoi(ptr);

  ptr = ptr2 + 1;
  if(!ptr)
    goto error_return;

  ptr2 = strchr(ptr, ':');

  if (!ptr2)
    goto error_return;

  *ptr2 = '\0';
  *vectoridx = atoi(ptr);

  ptr = ptr2 + 1;
  if(!ptr)
    goto error_return;

  ptr2 = strchr(ptr, '|');
    
  if(!ptr2)
    goto error_return;

  if (ptr2 && (ptr2+1))
  {
    *ptr2 = '\0';
    strncpy(data, ptr2 + 1, VECTOR_NAME_MAX_LEN - 1);
    data[VECTOR_NAME_MAX_LEN - 1] = '\0';    
  }

  strncpy(vector_name, ptr, VECTOR_NAME_MAX_LEN - 1);
  vector_name[VECTOR_NAME_MAX_LEN - 1] = '\0';
  
  ptr = strrchr(vector_name, global_settings->hierarchical_view_vector_separator);
  if(!ptr)
    goto error_return;
 
  if((cm_ptr) && strstr(cm_ptr->gdf_name, "cm_nd_bt_ip.gdf"))
  {
    ptr--;
    while(*ptr != global_settings->hierarchical_view_vector_separator)
      ptr--;
  }
  ptr++;

  strncpy(backend_name, ptr, VECTOR_NAME_MAX_LEN - 1);

  backend_name[VECTOR_NAME_MAX_LEN - 1] = '\0';
  
  return 0;

  error_return:
     return -1;
*/
}

int get_overall_vector(CM_info *cus_mon_ptr,char *vector_name, char *method, char *overallVector, int tieridx, int norm_vector_id, int send_overall_overall, char *overallVectorWithoutID)
{
  char *ptr;
  //char *tier = vector_name;
  int no_of_field_in_breadcrumb = 0;
  char *field[10];
  char vector[VECTOR_NAME_MAX_LEN + 1] = "";
  char hv_separator[2];
  char create_overall = 0;
  hv_separator[0] = global_settings->hierarchical_view_vector_separator;
  hv_separator[1] = '\0';
  
  strcpy(vector, vector_name);
  MLTL2(EL_F, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_MAJOR, "Vector name received in get_overall_vector = %s", vector_name);
  //get tier name and no of fields in breadcrumb
  no_of_field_in_breadcrumb = get_tokens(vector, field, hv_separator, 10);
  MLTL2(EL_F, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_MAJOR, " no_of_field_in_breadcrumb = %d", no_of_field_in_breadcrumb);

  ptr = strchr(method, ' ');
  if(ptr)
    return -1;
  

  //sending normvectorid 0 because it is not used by overall vector 
  if((strstr(cus_mon_ptr->gdf_name, "cm_nd_nodejs_event_loop.gdf") == NULL) && (strstr(cus_mon_ptr->gdf_name, "cm_nd_nodejs_gc.gdf") == NULL) && (strstr(cus_mon_ptr->gdf_name, "cm_nd_fp_stats.gdf") == NULL) && (strstr(cus_mon_ptr->gdf_name, "cm_nd_jvm_thread_monitor.gdf") == NULL) && (strstr(cus_mon_ptr->gdf_name, "cm_nd_win_") == NULL) && (strstr(cus_mon_ptr->gdf_name, "cm_java_gc_jmx_sun8") == NULL) && (strstr(cus_mon_ptr->gdf_name, "cm_nd_bt_percentile.gdf") == NULL) && (strstr(cus_mon_ptr->gdf_name, "cm_nd_ip_percentile.gdf") == NULL) && !(cus_mon_ptr->monitor_type & BCI_MBEAN_MONITOR))
  {
    if(overallVector)
    {
      if(no_of_field_in_breadcrumb == 3)
        sprintf(overallVector, "%d|0|%d:%s%s%s%s%s", tieridx, norm_vector_id,
                       vector, hv_separator, "Overall", hv_separator, method);
      else if(no_of_field_in_breadcrumb == 4 || no_of_field_in_breadcrumb == 5)
        sprintf(overallVector, "%d|0|%d:%s%s%s%s%s%s%s", tieridx, norm_vector_id,
                       vector, hv_separator, "Overall", hv_separator, "Overall", hv_separator, method);
      else
      {
        MLTL1(EL_F, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_MAJOR,
                                 "Overall vector is not generated as fields in vector name is greater than 5 or less than 3 , %s", vector_name);
        return -2;
      }
    }
 
    if(overallVectorWithoutID != NULL)
    {
      if(no_of_field_in_breadcrumb == 3)
        sprintf(overallVectorWithoutID, "%s%s%s%s%s", vector, hv_separator, "Overall", hv_separator, method);
      else if(no_of_field_in_breadcrumb == 4 || no_of_field_in_breadcrumb == 5)
        sprintf(overallVectorWithoutID, "%s%s%s%s%s%s%s", vector, hv_separator, "Overall", hv_separator, "Overall", hv_separator, method);
      else
      {
        MLTL1(EL_F, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_MAJOR,
                                 "Overall vector is not generated as fields in vector name is greater than 5 or less than 3 , %s", vector_name);
        return -2;
      }
    }
    create_overall |= 0x01;
  }

  //TODO remove strlen
  if(send_overall_overall && (strstr(cus_mon_ptr->gdf_name, "cm_nd_bt.gdf") == NULL) && (strstr(cus_mon_ptr->gdf_name, "cm_nd_exception_stats.gdf") == NULL) && (strstr(cus_mon_ptr->gdf_name, "cm_nd_nodejs_event_loop.gdf") == NULL) && (strstr(cus_mon_ptr->gdf_name, "cm_nd_nodejs_gc.gdf") == NULL) && (strstr(cus_mon_ptr->gdf_name, "cm_nd_jvm_thread_monitor.gdf") == NULL) && (strstr(cus_mon_ptr->gdf_name, "cm_nd_win_") == NULL) && (strstr(cus_mon_ptr->gdf_name, "cm_java_gc_jmx_sun8") == NULL) && (strstr(cus_mon_ptr->gdf_name, "cm_nd_bt_percentile.gdf") == NULL) && (strstr(cus_mon_ptr->gdf_name, "cm_nd_ip_percentile.gdf") == NULL) && !(cus_mon_ptr->monitor_type & BCI_MBEAN_MONITOR))
  {
    if(overallVector)
    {
      if(no_of_field_in_breadcrumb == 3)
        sprintf(overallVector + strlen(overallVector), " %d|0|0:%s%s%s%s%s", tieridx,
                         vector, hv_separator, "Overall", hv_separator, "Overall");
      else if(no_of_field_in_breadcrumb == 4 || no_of_field_in_breadcrumb == 5)
        sprintf(overallVector + strlen(overallVector), " %d|0|0:%s%s%s%s%s%s%s", tieridx, 
                         vector, hv_separator, "Overall", hv_separator, "Overall", hv_separator, "Overall");
      else
      {
        MLTL1(EL_F, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_MAJOR,
                               "Overall vector is not generated as fields in vector name is greater than 5 or less than 3 , %s", vector_name);
        return -2;
      }
    }

    if(overallVectorWithoutID != NULL)  
    {
      if(no_of_field_in_breadcrumb == 3)
        sprintf(overallVectorWithoutID + strlen(overallVectorWithoutID), " %s%s%s%s%s", vector, hv_separator, "Overall", hv_separator, "Overall");
      else if(no_of_field_in_breadcrumb == 4 || no_of_field_in_breadcrumb == 5)
        sprintf(overallVectorWithoutID + strlen(overallVectorWithoutID), " %s%s%s%s%s%s%s", vector, hv_separator, "Overall", hv_separator, "Overall", hv_separator, "Overall"); 
      else
      {
        MLTL1(EL_F, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_MAJOR,
                               "Overall vector is not generated as fields in vector name is greater than 5 or less than 3 , %s", vector_name);
        return -2;
      }
    }
    //We are saving different value for future use. value 2 can be used when Tier>Overall>Overall is created and value is set 1, if Tier>Overall>Instance is created. 
    create_overall |= 0x02;
  }
  if(create_overall != 0)
    cus_mon_ptr->flags |= OVERALL_CREATED;

  MLTL4(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Flags of CM_INFO of gdf %s is %d", cus_mon_ptr->gdf_name, cus_mon_ptr->flags);
  return 0;
}

void search_and_set_nd_vector_elements(CM_info *cus_mon_ptr, char *vector, short tierIdx, int vectorIdx, int instanceIdx, int *ret, char *backend_name, int norm_vector_id)
{
  int k = 0, reused_vec_case_set_is_data_filled = 0, total_flds = 0, i = 0;
  CM_vector_info *vector_list = cus_mon_ptr->vector_list;
  char overallvector[4096] = {0};
  char *field[3];  
  memset(field, 0, sizeof(field));

  NSDL1_MON(NULL, NULL, "Method called. vector = %s, backend_name = %s, norm_vector_id = %d", vector, backend_name, norm_vector_id);
  NSTL2(NULL, NULL, "Method called search_and_set_nd_vcetor_elements. vector = %s, backend_name = %s, norm_vector_id = %d", vector, backend_name, norm_vector_id);

  int mappedVectorIdx = ns_get_id_value(cus_mon_ptr->ndVectorIdxmap, vectorIdx);

  // Get vector index
  k = nslb_get_norm_id(&(cus_mon_ptr->nd_norm_table), vector, strlen(vector));
  //for (k = 0; k < cus_mon_ptr->total_vectors; k++)
  //{

  // nslb_get_norm_id returns -2 on fail
  if (k == -2)
  {
    k = cus_mon_ptr->total_vectors;
  }
  else if(strcmp(vector_list[k].vector_name, vector) == 0) //matched
  {
    NSDL2_MON(NULL, NULL, "Old vector received with new id: %d, vector: %s", k, vector);

    //set data ptr
    if(vector_list[k].flags & OVERALL_VECTOR)
    {
      if((cus_mon_ptr->tierNormVectorIdxTable[norm_vector_id][tierIdx]) != NULL)
        vector_list[k].data = (cus_mon_ptr->tierNormVectorIdxTable[norm_vector_id][tierIdx])->aggrgate_data;

      vector_list[k].vectorIdx = norm_vector_id;
    }
    else
    {
      if((cus_mon_ptr->instanceVectorIdxTable[mappedVectorIdx][instanceIdx]) != NULL)
        vector_list[k].data = (cus_mon_ptr->instanceVectorIdxTable[mappedVectorIdx][instanceIdx])->data;

      vector_list[k].vectorIdx = vectorIdx;
    }

    //set idx
    vector_list[k].mappedVectorIdx = mappedVectorIdx;
    vector_list[k].instanceIdx = instanceIdx;
    vector_list[k].tierIdx = tierIdx;

    vector_list[k].flags &= ~RUNTIME_DELETED_VECTOR;
    //if conn_state is CM_DELETED we have to set ret = 1, so that we can call add_new_vector to add deleted vector
    if(vector_list[k].vector_state == CM_DELETED || (vector_list[k].flags &  WAITING_DELETED_VECTOR) )
      *ret = 1;

    if(!(vector_list[k].flags & DATA_FILLED))
    {
      reused_vec_case_set_is_data_filled = 1;
      vector_list[k].flags |= DATA_FILLED;
      monitor_runtime_changes_applied = 1;
      //break;
    }
    else
      return;
  } else {
    NSTL4(NULL, NULL, "Vector name don't match, this should not happen.");
  }

 // }
  if(k == cus_mon_ptr->total_vectors)
    *ret = 1;
  else if(vector_list[k].vector_state == CM_VECTOR_RESET)  //This states is set at connection break. So no need to set ret = 1. If set it will add it as a new vector and Duplicate error will come for that vector.
  {
    vector_list[k].vector_state = CM_INIT;
    MLTL2(EL_F, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_GENERAL, EVENT_MAJOR, "Vector(%s) added after connection reset", 
          vector_list[k].vector_name);
  }
  else
    *ret = 1;

  if(reused_vec_case_set_is_data_filled == 1) 
  {
    get_overall_vector(cus_mon_ptr, vector, backend_name, NULL, tierIdx, norm_vector_id, 1, overallvector);
    if(overallvector[0] != '\0')
    {
      total_flds = get_tokens(overallvector, field, " ", 3);
      for(i = 0; i < total_flds ; i++)
      {
        for (k = 0; k < cus_mon_ptr->total_vectors; k++)    
        {
          if(!strcmp(vector_list[k].vector_name, field[i])) //matched                 
          {
            vector_list[k].flags |= DATA_FILLED;
            break;
          } 
        }
      }
    } 
  }
}

int send_instance_stop_msg_to_NDC(CM_info *cus_mon_ptr, int tier_id, int instance_id, int vector_id)
{
  char tier_inst_vec_id_buff[512] = {0};

  sprintf(tier_inst_vec_id_buff,"mon_vector_recovery:tierid=%d;instanceid=%d vectorid=%d;\n", tier_id, instance_id, vector_id);
  MLTL1(EL_F, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_GENERAL, EVENT_MAJOR,
                               "Sending message to NDC for vector received for the first time is without vector name"); 
  if((cm_send_msg(cus_mon_ptr, tier_inst_vec_id_buff, 1)) < 0)
    return -1;
  return 0;
}


int fetch_hash_index_and_delete_nd_vector(char *vector_name, CM_info *cus_mon_ptr)
{
  int v_len = strlen(vector_name);
  int vector_index;
 
  vector_index = nslb_get_norm_id(&(cus_mon_ptr->nd_norm_table), vector_name, v_len);
  NSDL3_MON(NULL, NULL, "Vector Name (%s) created for Deleting vectors of monitor gdf (%s) is present at index %d", vector_name, cus_mon_ptr->gdf_name, vector_index);
  if(vector_index < 0)
  {
    MLTL1(EL_F, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_MAJOR, "Vector Name (%s) doesnot exist for percentile monitor gdf: %s", vector_name, cus_mon_ptr->gdf_name);
    return -1;
  }

  delete_entry_for_nd_monitors(cus_mon_ptr, vector_index, 1); //sending  1 for instance to set flag
  return 0;
}


void delete_entry_for_nd_monitors(CM_info *cus_mon_ptr, int vectorIdx, char flag)
{
  CM_vector_info *vector_list = cus_mon_ptr->vector_list;

  if((vector_list[vectorIdx].vector_state == CM_DELETED) || (vector_list[vectorIdx].flags & WAITING_DELETED_VECTOR))
    return;
 
  if (flag)
  {
    if((vector_list[vectorIdx].mappedVectorIdx >= 0) && (vector_list[vectorIdx].instanceIdx >= 0))
    {
      (cus_mon_ptr->instanceVectorIdxTable)[vector_list[vectorIdx].mappedVectorIdx][vector_list[vectorIdx].instanceIdx]->reset_and_validate_flag |= ND_RESET_FLAG;
      (cus_mon_ptr->instanceVectorIdxTable)[vector_list[vectorIdx].mappedVectorIdx][vector_list[vectorIdx].instanceIdx]->reset_and_validate_flag &= ~GOT_ND_DATA;
    }
  }
  vector_list[vectorIdx].data = nan_buff;
 
  vector_list[vectorIdx].vectorIdx = -1; 
  vector_list[vectorIdx].mappedVectorIdx = -1;
  vector_list[vectorIdx].instanceIdx = -1;
  vector_list[vectorIdx].tierIdx = -1;

  vector_list[vectorIdx].flags |= RUNTIME_RECENT_DELETED_VECTOR;
  // set "nan" on vector deletion
  int z;
  for(z = 0; z < cus_mon_ptr->no_of_element; z++)
  {
    vector_list[vectorIdx].data[z] = 0.0/0.0;
  }
 
  if((vector_list[vectorIdx].vector_state != CM_DELETED) && !(cus_mon_ptr->flags & ALL_VECTOR_DELETED) && !(vector_list[vectorIdx].flags & WAITING_DELETED_VECTOR))
  {
    if(cus_mon_ptr->total_deleted_vectors >= cus_mon_ptr->max_deleted_vectors)
    {
      MLTL2(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Monitor name = %s, total_deleted_vectors = %d and max_deleted_vectors = %d", cus_mon_ptr->monitor_name, cus_mon_ptr->total_deleted_vectors, cus_mon_ptr->max_deleted_vectors);

      create_entry_in_reused_or_deleted_structure(&cus_mon_ptr->deleted_vector, &cus_mon_ptr->max_deleted_vectors);
      
      MLTL2(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "After create_entry_in_reused_or_deleted_structure Now total_entries = %d max_entries = %d", cus_mon_ptr->total_deleted_vectors, cus_mon_ptr->max_deleted_vectors);
     }
    cus_mon_ptr->deleted_vector[cus_mon_ptr->total_deleted_vectors] = vectorIdx;

    if(!g_enable_delete_vec_freq)
    { 
      vector_list[vectorIdx].vector_state = CM_DELETED;
      vector_list[vectorIdx].flags |= RUNTIME_DELETED_VECTOR;
      total_deleted_vectors++;
      monitor_deleted_on_runtime = 1; 
      monitor_runtime_changes_applied = 1;
    }
    else
    {
      vector_list[vectorIdx].flags |= WAITING_DELETED_VECTOR;
      total_waiting_deleted_vectors++;
    }

    cus_mon_ptr->total_deleted_vectors++;
  }
}

//set global pointers forpercentile monitors
CM_info *set_or_get_nd_percentile_ptrs(CM_info *cus_mon_ptr) 
{
  CM_info *tmp_ptr = NULL;

  if(strstr(cus_mon_ptr->gdf_name, "cm_nd_bt_percentile.gdf"))
    tmp_ptr = cm_bt_percentile_ptr = cus_mon_ptr;
  else if(strstr(cus_mon_ptr->gdf_name, "cm_nd_ip_percentile.gdf"))
    tmp_ptr = cm_ip_percentile_ptr = cus_mon_ptr;

  return tmp_ptr;
}

//Free elements of node and node itself
void free_percentile_list_node(PercentileList *list_ptr)
{
  FREE_AND_MAKE_NULL(list_ptr->name,"Percentile Name" , -1);
  FREE_AND_MAKE_NULL(list_ptr ,"ND percentile node allocation" , -1);
}

//Free elements of node and node itself
void free_tier_list_node(TierList *list_ptr)
{
  FREE_AND_MAKE_NULL(list_ptr->tier_name,"Tier Name" , -1);
  FREE_AND_MAKE_NULL(list_ptr ,"ND percentile node allocation" , -1);
}


void check_and_delete_nd_percentile_vectors_using_tiers(int tier_type)
{
  TierList *list_ptr = tier_list_head_ptr;
  TierList *prev_node = NULL;
  TierList *tmp_node = NULL;
  CM_vector_info *vector_list;
  int i = 0;
  char flag = 0;

  while(list_ptr != NULL)
  {
    if(list_ptr->state == INACTIVE)   //Need to delete vector from percentile monitors
    {
      if(cm_bt_percentile_ptr && (tier_type == ND_BT_TIER_LIST))  //For ND BT percentile monitor
      {
        MLTL3(EL_F, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_MAJOR, "Deleting BT percentile vector of tier %s", list_ptr->tier_name);
        vector_list = cm_bt_percentile_ptr->vector_list;
        for(i = 0; i < cm_bt_percentile_ptr->total_vectors; i++)
        {
          if(vector_list[i].vector_state == CM_DELETED)
            continue;

          if(!strncmp(vector_list[i].vector_name, list_ptr->tier_name, list_ptr->tier_len))
          {
            flag = 1;
            delete_entry_for_nd_monitors(cm_bt_percentile_ptr, i, 1); //sending  1 for instance to set flag
          } 
        }
      }

      if(cm_ip_percentile_ptr && (tier_type == ND_IP_TIER_LIST))  //For ND IP percentile monitor
      {
        MLTL3(EL_F, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_MAJOR, "Deleting IP percentile vector of tier %s", list_ptr->tier_name);
        vector_list = cm_ip_percentile_ptr->vector_list;
        for(i = 0; i < cm_ip_percentile_ptr->total_vectors; i++)
        {
          if(vector_list[i].vector_state == CM_DELETED)
            continue;

          if(!strncmp(vector_list[i].vector_name, list_ptr->tier_name, list_ptr->tier_len))
          {
            flag = 1;
            delete_entry_for_nd_monitors(cm_ip_percentile_ptr, i, 1); //sending  1 for instance to set flag
          }
        }
      }

      //For deletion of node because the this percentile Tier is deleted by user.
      if(prev_node == NULL)
      {
        tier_list_head_ptr = list_ptr->next;
        tmp_node = list_ptr;
        list_ptr = list_ptr->next;
      }
      else
      {
        tmp_node = list_ptr;
        prev_node->next = list_ptr->next;
        list_ptr = list_ptr->next;
      }
      free_tier_list_node(tmp_node);
    }
    else
    {
      prev_node = list_ptr;
      list_ptr = list_ptr->next;
    }
  }
  if(flag == 0)
  {
    MLTL1(EL_F, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_MAJOR, "No percentile vector were deleted");
  }
  else
  {
    MLTL1(EL_F, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_MAJOR, "Some percentile vector got deleted");
  }

}


void check_and_delete_nd_percentile_vectors()
{
  PercentileList *list_ptr = percentile_list_head_ptr;
  PercentileList *prev_node = NULL;
  PercentileList *tmp_node = NULL;
  CM_vector_info *vector_list;
  int i = 0;
  
  while(list_ptr != NULL)
  {
    if(list_ptr->state == INACTIVE)   //Need to delete vector from percentile monitors
    {
      MLTL3(EL_F, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_MAJOR, "Deleting BT percentile vector for percentile %s", list_ptr->name);
      if(cm_bt_percentile_ptr)  //For ND BT percentile monitor
      {
        vector_list = cm_bt_percentile_ptr->vector_list;
        for(i = 0; i < cm_bt_percentile_ptr->total_vectors; i++)
        {
          if(vector_list[i].vector_state == CM_DELETED)
            continue;

          if(!strcmp(vector_list[i].vector_name + (vector_list[i].vector_length - list_ptr->len), list_ptr->name))
          {
            delete_entry_for_nd_monitors(cm_bt_percentile_ptr, i, 1); //sending  1 for instance to set flag
            NSDL2_MON(NULL, NULL, "Entry for percentile %s has been deleted from ND BT monitor. Deleted Vector: %s", (list_ptr->name + 1), vector_list[i].vector_name);
          }
        }
      }
  
      if(cm_ip_percentile_ptr)  //For ND IP percentile monitor
      {
        MLTL3(EL_F, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_MAJOR, "Deleting IP percentile vector for percentile %s", list_ptr->name);
        vector_list = cm_ip_percentile_ptr->vector_list;
        for(i = 0; i < cm_ip_percentile_ptr->total_vectors; i++)
        {
          if(vector_list[i].vector_state == CM_DELETED)
            continue;

          if(!strcmp(vector_list[i].vector_name + (vector_list[i].vector_length - list_ptr->len - 1), list_ptr->name))
          {
            delete_entry_for_nd_monitors(cm_ip_percentile_ptr, i, 1); //sending  1 for instance to set flag
            NSDL2_MON(NULL, NULL, "Entry for percentile %s has been deleted from ND IP monitor. Deleted Vector: %s", (list_ptr->name + 1), vector_list[i].vector_name);
          }
        }
      }
  
      //For deletion of node because the this percentile is deleted by user.
      if(prev_node == NULL)
      {
        percentile_list_head_ptr = list_ptr->next;
        tmp_node = list_ptr;
        list_ptr = list_ptr->next;
      } 
      else
      {
        tmp_node = list_ptr;
        prev_node->next = list_ptr->next;
        list_ptr = list_ptr->next;
      }
      free_percentile_list_node(tmp_node);
    }
    else
    {
      prev_node = list_ptr;
      list_ptr = list_ptr->next;
    }
  }
}


//This will create vector name for its respective percentile monitor.
void create_vector_and_delete_percentile(char *vector_name, CM_info *cm_ptr)
{
  char local_buffer[1024];   //Enough for vector name
  int len, vector_index;
  PercentileList *list_ptr = percentile_list_head_ptr;

  len = sprintf(local_buffer, "%s", vector_name);

  while(list_ptr != NULL)
  {
    if(list_ptr->state == INACTIVE)
    {
      list_ptr = list_ptr->next;
      continue;
    }
    
    len = sprintf(local_buffer + len, "%s", list_ptr->name);  //Vector name in percentile_list will be saved with '>'

    //fetch_hash_index_and_delete_nd_vector(local_buffer, cm_ptr);
    //Looking up in hash table
    vector_index = nslb_get_norm_id(&(cm_ptr->nd_norm_table), local_buffer, len);
    NSDL3_MON(NULL, NULL, "Vector Name (%s) created for Deleting vectors of monitor gdf (%s) is present at index %d", local_buffer, cm_ptr->gdf_name, vector_index);
    if(vector_index < 0)
    {
      MLTL1(EL_F, 0, 0, _FLN_, cm_ptr, EID_DATAMON_ERROR, EVENT_MAJOR, "Vector Name (%s) doesnot exist for percentile monitor gdf: %s", local_buffer, cm_ptr->gdf_name);
      list_ptr = list_ptr->next;
      continue;
    }
   
    delete_entry_for_nd_monitors(cm_ptr, vector_index, 1); //sending  1 for instance to set flag
    list_ptr = list_ptr->next;
  }
  return;
}


//This function will calculate overall for all ND monitors. Previously this was done as soon as data was received from socket. Now the overall will be calculated from deliver_report.
void process_nd_overall_data()
{
  int i, v_list_idx, gp_info_index;
  CM_info *cus_mon_ptr;
  Graph_Info *local_graph_data_ptr;
  Group_Info *local_group_data_ptr;
  time_t mseconds_before, mseconds_after, mseconds_elapsed;
  char mseconds_buff[128];
  char is_nd_monitors_applied = 0;
 
  mseconds_before = get_ms_stamp();
  for( i = 0; i < total_monitor_list_entries; i++ )
  {
    cus_mon_ptr = monitor_list_ptr[i].cm_info_mon_conn_ptr;
    gp_info_index = cus_mon_ptr->gp_info_index;

    //Monitor whose vectors has not been received.
    if((gp_info_index < 0) || (!(cus_mon_ptr->flags & OVERALL_CREATED)))
      continue;

    local_group_data_ptr = group_data_ptr + gp_info_index;
    local_graph_data_ptr = graph_data_ptr + local_group_data_ptr->graph_info_index;

    NSDL1_MON(NULL, NULL, "Monitor_name = %s, gp_info_index = %d, local_group_data_ptr = %p, local_graph_data_ptr = %p", cus_mon_ptr->monitor_name, gp_info_index, local_group_data_ptr, local_graph_data_ptr);

    if((cus_mon_ptr->flags & ND_MONITOR) && !(cus_mon_ptr->flags & ALL_VECTOR_DELETED))
    {
      for(v_list_idx = 0; v_list_idx < cus_mon_ptr->total_vectors; v_list_idx++)
      {
        if(!(cus_mon_ptr->vector_list[v_list_idx].vector_state == CM_DELETED) && !(cus_mon_ptr->vector_list[v_list_idx].flags & OVERALL_VECTOR) && (cus_mon_ptr->vector_list[v_list_idx].mappedVectorIdx >= 0) && (cus_mon_ptr->vector_list[v_list_idx].instanceIdx >= 0) && (cus_mon_ptr->instanceVectorIdxTable[cus_mon_ptr->vector_list[v_list_idx].mappedVectorIdx][cus_mon_ptr->vector_list[v_list_idx].instanceIdx]))
        {
          fill_agg_data(cus_mon_ptr, cus_mon_ptr->instanceVectorIdxTable[cus_mon_ptr->vector_list[v_list_idx].mappedVectorIdx][cus_mon_ptr->vector_list[v_list_idx].instanceIdx]->data, cus_mon_ptr->vector_list[v_list_idx].tierIdx, cus_mon_ptr->instanceVectorIdxTable[cus_mon_ptr->vector_list[v_list_idx].mappedVectorIdx][cus_mon_ptr->vector_list[v_list_idx].instanceIdx]->norm_vector_id, local_group_data_ptr, local_graph_data_ptr);
          
          fill_agg_data(cus_mon_ptr, cus_mon_ptr->instanceVectorIdxTable[cus_mon_ptr->vector_list[v_list_idx].mappedVectorIdx][cus_mon_ptr->vector_list[v_list_idx].instanceIdx]->data, cus_mon_ptr->vector_list[v_list_idx].tierIdx, 0, local_group_data_ptr, local_graph_data_ptr);
          is_nd_monitors_applied = 1;
        }
	if ((cus_mon_ptr->flags & ND_MONITOR) && (cus_mon_ptr->vector_list[v_list_idx].flags & OVERALL_VECTOR) && (cus_mon_ptr->flags & OVERALL_CREATED) && (!(g_tsdb_configuration_flag & RTG_MODE)))
	{
	  //time_t current_time;
	  //current_time=time(NULL);
	  ns_tsdb_insert(cus_mon_ptr, cus_mon_ptr->vector_list[v_list_idx].data, cus_mon_ptr->vector_list[v_list_idx].metrics_idx, &cus_mon_ptr->vector_list[v_list_idx].metrics_idx_found, 0,  cus_mon_ptr->vector_list[v_list_idx].vector_name);
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
	                      "Send information to TSDB for ND overall with vector_name %s and data %ld\n", cus_mon_ptr->vector_list[v_list_idx].vector_name , cus_mon_ptr->vector_list[v_list_idx].data );

	}
      }
    }
  }

  if(is_nd_monitors_applied == 1)
  {
    mseconds_after = get_ms_stamp();
    mseconds_elapsed = mseconds_after - mseconds_before;
    convert_to_hh_mm_ss(mseconds_elapsed, mseconds_buff);
 
    MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                "Total time taken in calculating overall data for ND monitors: %d milliseconds", mseconds_elapsed);
  }

  return;
}

int generate_aggregate_vector_list(CM_info *cus_mon_ptr, char *vector_line, char **overall_list)
{
  int mapped_inst_id;
  int mapped_tier_id;
  int mapped_vector_id;
  char vector_name[VECTOR_NAME_MAX_LEN + 1] = "";
  char save_vector_name[VECTOR_NAME_MAX_LEN + 1] = ""; //for search_and_set_nd_vector_elements
  char backend_name[VECTOR_NAME_MAX_LEN + 1] = "";
  int tieridx, instanceidx, vectoridx;
  char agg_data[512];
  char overallvector[4096];
  int norm_vector_id, ret = 0;
  int send_overall_overall = 0;
  int num_data = 0, i = 0, new_flag = 0;

  instanceVectorIdx *inst_vctr = NULL;
  tierNormVectorIdx *tier_vctr = NULL;
  tierNormVectorIdx *overall_vctr = NULL;
  
  agg_data[0] = '\0';
  overallvector[0] = '\0';
  //get no. of elements
  if(cus_mon_ptr->no_of_element == 0)
  {
    get_no_of_elements(cus_mon_ptr, &num_data); 
    cus_mon_ptr->no_of_element = num_data;
  }
  else
  {
    num_data = cus_mon_ptr->no_of_element;
  }

  NSTL3(NULL, NULL, "vector_line in generate_agg_vector_list = %s", vector_line);
  if(parse_vector_new_format(vector_line, vector_name, &tieridx, &instanceidx, &vectoridx, backend_name, agg_data, cus_mon_ptr, 5, cus_mon_ptr->gdf_name) < 0)
    return -1;
  NSTL3(NULL, NULL, "vector_name in generate_agg_vector_list = %s", vector_name);
  
  //get instance map index
  mapped_inst_id = ns_get_id_value(cus_mon_ptr->instanceIdxMap, instanceidx);

  //get tier map index
  mapped_tier_id = ns_get_id_value(cus_mon_ptr->tierIdxmap, tieridx);

  //get vector map index
  mapped_vector_id = ns_get_id_value(cus_mon_ptr->ndVectorIdxmap, vectoridx);

  //NSTL1_OUT(NULL, NULL, "\n instanceidx = [%d] , mapped_inst_id = [%d] : tieridx = [%d] , mapped_tier_id = [%d] \n", instanceidx, mapped_inst_id, tieridx, mapped_tier_id);

  //get normalized method name
  ns_normalize_monitor_vector(cus_mon_ptr, backend_name, &norm_vector_id);

  //if((mapped_tier_id < 0) || (mapped_inst_id < 0) || (norm_vector_id < 0) || (mapped_vector_id < 0))
  if((mapped_tier_id < 0) || (mapped_inst_id < 0) || ((norm_vector_id < 0) && (backend_name[0] != '\0')) || (mapped_vector_id < 0))
    return -1;

  //This function will validate data for ND monitors. If data crosses the limit set by ND_DATA_VALIDATION keyword, we will ignire that data. Validation will not be done for the first data sample sent by any vector.
  if(enable_nd_data_validation)
  {
    if(validate_nd_data(cus_mon_ptr, vector_name, agg_data) < 0)
      return -2;
  }

  if(strstr(cus_mon_ptr->gdf_name, "cm_nd_bt.gdf") != NULL)
  {
    MLTL2(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "mapped_inst_id [ %d ] , mapped_tier_id = [ %d ] , vectoridx = [ %d ], tieridx = [ %d ] , instanceidx = [ %d] , norm_vector_id = [ %d ] , agg_data = [ %s ] ", mapped_inst_id, mapped_tier_id, vectoridx, tieridx, instanceidx, norm_vector_id, agg_data );
  }

  //check and realloc vector-instance table
  if(cus_mon_ptr->cur_vecIdx_InstanceVectorIdxTable <= mapped_vector_id)
  {
    ns_allocate_2d_matrix((void ***) &(cus_mon_ptr->instanceVectorIdxTable), 8, (mapped_vector_id - cus_mon_ptr->cur_vecIdx_InstanceVectorIdxTable) + 1,  
                           &(cus_mon_ptr->cur_instIdx_InstanceVectorIdxTable), 
                              &(cus_mon_ptr->cur_vecIdx_InstanceVectorIdxTable));
  }
  if(cus_mon_ptr->cur_instIdx_InstanceVectorIdxTable <= mapped_inst_id)
  {
    ns_allocate_matrix_col((void ***)&(cus_mon_ptr->instanceVectorIdxTable), 8, &(cus_mon_ptr->cur_instIdx_InstanceVectorIdxTable), 
                               &(cus_mon_ptr->cur_vecIdx_InstanceVectorIdxTable));
  }

   MLTL4(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "MappedVectoRID = %d, Mapped InstanceId = %d, norm_vector_id = %d, mappedTierId = %d, VectorName = %s", mapped_vector_id, mapped_inst_id, norm_vector_id, mapped_tier_id, vector_name);
  //fill entry in vector-instance table
  if((cus_mon_ptr->instanceVectorIdxTable)[mapped_vector_id][mapped_inst_id] == NULL)
  {
    MY_MALLOC_AND_MEMSET(inst_vctr, sizeof(instanceVectorIdx), "instanceVectorInstTable", -1);
    (cus_mon_ptr->instanceVectorIdxTable)[mapped_vector_id][mapped_inst_id] = inst_vctr;
    MY_MALLOC_AND_MEMSET(inst_vctr->data, (num_data * sizeof(double)), "instanceVectorInstTable", -1);
    if (!(g_tsdb_configuration_flag & RTG_MODE))
    {
      //MY_MALLOC_AND_MEMSET(inst_vctr->metrics_idx, (cus_mon_ptr->no_of_element * sizeof(long)), "instanceVectorInstTable metrics id", -1);
       ns_tsdb_malloc_metric_id_buffer(&inst_vctr->metrics_idx, cus_mon_ptr->no_of_element);
    }
    //fill data
    inst_vctr->norm_vector_id = norm_vector_id;
    inst_vctr->tier_id = mapped_tier_id;
    new_flag = 1;
    strcpy(save_vector_name, vector_name);
    ret = 1; //new
    if(!(cus_mon_ptr->flags & ALL_VECTOR_DELETED))
      (cus_mon_ptr->instanceVectorIdxTable)[mapped_vector_id][mapped_inst_id]->reset_and_validate_flag |= GOT_ND_DATA;
  }
  else
  {
    if ((cus_mon_ptr->instanceVectorIdxTable)[mapped_vector_id][mapped_inst_id]->reset_and_validate_flag & ND_RESET_FLAG)
    {
      //generate breadcrumb
      //find index in custom monitor table
      //update data index
      search_and_set_nd_vector_elements(cus_mon_ptr, vector_name, mapped_tier_id, vectoridx, mapped_inst_id, &ret, backend_name, norm_vector_id);

      (cus_mon_ptr->instanceVectorIdxTable)[mapped_vector_id][mapped_inst_id]->reset_and_validate_flag &= ~ND_RESET_FLAG; //reset
      (cus_mon_ptr->instanceVectorIdxTable)[mapped_vector_id][mapped_inst_id]->norm_vector_id = norm_vector_id;
      (cus_mon_ptr->instanceVectorIdxTable)[mapped_vector_id][mapped_inst_id]->tier_id = mapped_tier_id;
    }
    else
    {
      //inst_vctr = (cus_mon_ptr->instanceVectorIdxTable)[vectoridx][mapped_inst_id];
      ret = 0;
      if(norm_vector_id < 0)
        norm_vector_id = (cus_mon_ptr->instanceVectorIdxTable)[mapped_vector_id][mapped_inst_id]->norm_vector_id;
    }

    inst_vctr = (cus_mon_ptr->instanceVectorIdxTable)[mapped_vector_id][mapped_inst_id];
    if((cus_mon_ptr->instanceVectorIdxTable)[mapped_vector_id][mapped_inst_id]->reset_and_validate_flag & GOT_ND_DATA)
    {
      if(monitor_debug_level > 0)
      {
        MLTL2(EL_F, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_MAJOR,
                                   "Data is already filled for this vector(%s) for current progress report but updating it with the current data received[%s|%s]. mapped_vector_id = %d, mapped_inst_id = %d", vector_name, vector_name, agg_data, mapped_vector_id, mapped_inst_id);
      }
      update_health_monitor_sample_data(&hm_data->data_already_filled);
      //return -2;
    }
    else
      (cus_mon_ptr->instanceVectorIdxTable)[mapped_vector_id][mapped_inst_id]->reset_and_validate_flag |= GOT_ND_DATA;
  }

  if((ret != 0) && (vector_name[0] == '\0'))
  {
    if((send_instance_stop_msg_to_NDC(cus_mon_ptr, tieridx, instanceidx, vectoridx)) == -1)
    {
      MLTL1(EL_F, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_MAJOR, "Unable to get the vector_name");
    } 
    return -1;  
  }
  
   validate_and_fill_data(cus_mon_ptr, num_data, agg_data, inst_vctr->data, inst_vctr->metrics_idx, &inst_vctr->metrics_idx_found, vector_name);    
  //validate_and_fill_data(cus_mon_ptr, cus_mon_ptr->no_of_element, agg_data, inst_vctr->data);

  //check and realloc vector-tier table
  if(cus_mon_ptr->cur_normVecIdx_TierNormVectorIdxTable <= norm_vector_id)
  {
    ns_allocate_2d_matrix((void ***)&(cus_mon_ptr->tierNormVectorIdxTable), 8, DELTA_MAP_TABLE_ENTRIES, 
                           &(cus_mon_ptr->cur_tierIdx_TierNormVectorIdxTable), 
                              &(cus_mon_ptr->cur_normVecIdx_TierNormVectorIdxTable));
  }
  if(cus_mon_ptr->cur_tierIdx_TierNormVectorIdxTable <= mapped_tier_id)
  {
    ns_allocate_matrix_col((void ***)&(cus_mon_ptr->tierNormVectorIdxTable), 8, &(cus_mon_ptr->cur_tierIdx_TierNormVectorIdxTable), 
                               &(cus_mon_ptr->cur_normVecIdx_TierNormVectorIdxTable));
  }

  //fill entry in vector-tier table
  if(((cus_mon_ptr->tierNormVectorIdxTable)[norm_vector_id][mapped_tier_id] == NULL) || (((cus_mon_ptr->tierNormVectorIdxTable)[norm_vector_id][mapped_tier_id]->got_data_and_delete_flag | (cus_mon_ptr->tierNormVectorIdxTable)[0][mapped_tier_id]->got_data_and_delete_flag) & OVERALL_DELETE))
  {
    if((cus_mon_ptr->tierNormVectorIdxTable)[norm_vector_id][mapped_tier_id] == NULL)
    {
      MY_MALLOC_AND_MEMSET(tier_vctr, sizeof(tierNormVectorIdx), "tierNormVectorIdx", -1);
      (cus_mon_ptr->tierNormVectorIdxTable)[norm_vector_id][mapped_tier_id] = tier_vctr;

      MY_MALLOC_AND_MEMSET(tier_vctr->aggrgate_data, sizeof(double) * num_data, "TierVectorTable", -1);
    }
    else {
      (cus_mon_ptr->tierNormVectorIdxTable)[norm_vector_id][mapped_tier_id]->got_data_and_delete_flag &= ~OVERALL_DELETE;
      tier_vctr = (cus_mon_ptr->tierNormVectorIdxTable)[norm_vector_id][mapped_tier_id];
    }

    for(i=0; i<128; i++)
    {
      if(cus_mon_ptr->save_times_graph_idx[i] == -1)
        break;
      else      
        tier_vctr->aggrgate_data[cus_mon_ptr->save_times_graph_idx[i]] = MAX_LONG_LONG_VALUE;
    }

    //Earlier this was set in below if case. But it needs to be set every time code traverse through this block. As this block only deals with overall creation. It was done because when overall vectors was deleted and reused, code was going in else block as its memory was already allocated.
    if (((cus_mon_ptr->tierNormVectorIdxTable)[0][mapped_tier_id] == NULL) || (((cus_mon_ptr->tierNormVectorIdxTable)[0][mapped_tier_id]->got_data_and_delete_flag) & OVERALL_DELETE ))
    send_overall_overall = 1;

    if((cus_mon_ptr->tierNormVectorIdxTable)[0][mapped_tier_id] == NULL)
    {
      MY_MALLOC_AND_MEMSET(overall_vctr, sizeof(tierNormVectorIdx), "tierNormVectorIdx", -1);
      (cus_mon_ptr->tierNormVectorIdxTable)[0][mapped_tier_id] = overall_vctr;

      MY_MALLOC_AND_MEMSET(overall_vctr->aggrgate_data, sizeof(double) * num_data, "TierVectorTable", -1);

      for(i=0; i<128; i++)
      {
        if(cus_mon_ptr->save_times_graph_idx[i] == -1)
          break;
        else      
          overall_vctr->aggrgate_data[cus_mon_ptr->save_times_graph_idx[i]] = MAX_LONG_LONG_VALUE;
      }
    }
    else
    {
      overall_vctr = (cus_mon_ptr->tierNormVectorIdxTable)[0][mapped_tier_id];
    }
    //TODO error handling
    //because while filling structure we are doing mapping, hence pass here actual id not mapped id
    if(get_overall_vector(cus_mon_ptr, vector_name, backend_name, overallvector, tieridx, norm_vector_id, send_overall_overall, NULL) == -1)
    {
       MLTL2(EL_F, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_MAJOR, "Overall vector = %s", overallvector);
      return -1;   //returning becuse we get space in vector name so not creating overal for that vector
    }
  
     MLTL2(EL_F, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_ERROR, EVENT_MAJOR, "Overall vector = %s", overallvector);
    if(overallvector[0] != '\0')
    {
      //TODO optimize this
      MY_MALLOC_AND_MEMSET(*overall_list, strlen(overallvector) + 1, "Overall Vector List", -1);
      strcpy(*overall_list, overallvector);
    }
  }
  else
  {
    tier_vctr = (cus_mon_ptr->tierNormVectorIdxTable)[norm_vector_id][mapped_tier_id];
    overall_vctr = (cus_mon_ptr->tierNormVectorIdxTable)[0][mapped_tier_id];
  }

  MLTL4(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "(cus_mon_ptr->instanceVectorIdxTable)[mapped_vector_id][mapped_inst_id]->norm_vector_id = %d, VectorName = %s", (cus_mon_ptr->instanceVectorIdxTable)[mapped_vector_id][mapped_inst_id]->norm_vector_id, vector_name);
  //in fill_agg_data() we are using graphs structure & if all_vector_deleted is 1 then it means gdf parsing not done
  /* if(!(cus_mon_ptr->flags & ALL_VECTOR_DELETED))
  {
    fill_agg_data(cus_mon_ptr, inst_vctr->data, mapped_tier_id, norm_vector_id);

    //tier 'overall overall'
    fill_agg_data(cus_mon_ptr, inst_vctr->data, mapped_tier_id, 0);
  }*/

  if(new_flag)
  {
    NSDL1_MON(NULL, NULL, "New vector found for vector_line: %s", vector_line);
    search_and_set_nd_vector_elements(cus_mon_ptr, save_vector_name, mapped_tier_id, vectoridx, mapped_inst_id, &ret, backend_name,
      norm_vector_id);
    //Eralier the above function was called, and in that function it was unnecessarily traversing the vectors loop and comparing vector name. If the vector received is new vector, it will not match any vector present in the vector array. Hence the loop is overhead. So simply returning 1 as done in the previous function after the loop.
    return 1;
  }

  return ret;
}


/********************* Parsing of ND_DATA_VALIDATION keyword *************************/

int kw_set_nd_enable_data_validation(char *keyword, char *buf, char *err_msg, int runtime_flag)
{
  char key[1024]; 
  int ValidateMode = 0, num = 0;
  char text[1024];
        
  NSDL2_MON(NULL, NULL, "Method called, keyword = %s, buf = %s, global_settings->net_diagnostics_mode=%d", 
                                                     keyword, buf, global_settings->net_diagnostics_mode);

  num = sscanf(buf, "%s %d %d %d %d %d %s", key, &enable_nd_data_validation, &ValidateMode, &g_nd_max_tps, &g_nd_max_resp,
                                           &g_nd_max_count, text);
   
  NSDL2_MON(NULL, NULL, "Method called, keyword = %s, num = %d", keyword, num);         
  if(num != 6) 
  {
    if (runtime_flag){
      snprintf(err_msg, 2*1024, "Error: Too few/more arguments for %s keywords", key);
      return -1;
    }
    NS_KW_PARSING_ERR(keyword, runtime_flag, err_msg, ND_DATA_VALIDATION_USAGE, CAV_ERR_1011359,key);
  }
  
  if(g_nd_max_tps < 0 || g_nd_max_resp < 0 || g_nd_max_count < 0)
  {
    if(g_nd_max_tps < 0)
      NSTL1_OUT(NULL, NULL, "value of tps max is less than 0 which is not allowed.\n");

    if(g_nd_max_resp < 0)
      NSTL1_OUT(NULL, NULL, "value of resp max is less than 0 which is not allowed.\n");

    if(g_nd_max_count < 0)
      NSTL1_OUT(NULL, NULL, "value of count max is less than 0 which is not allowed.\n");
    
    if(runtime_flag){
      strcpy(err_msg, "USAGE: ND_DATA_VALIDATION <0/1> <0/1> <tps max> <resp max> <count max>");
      return -1;
    }
    NS_KW_PARSING_ERR(keyword, runtime_flag, err_msg, ND_DATA_VALIDATION_USAGE, CAV_ERR_1011359, key);
  }
  
  if(ValidateMode == 1)
    g_validate_nd_vector = 1;
  else
    g_validate_nd_vector = 0;
  return 0;
}
