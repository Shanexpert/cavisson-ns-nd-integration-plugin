/******************************************************************
 * Name    :    ns_ndc.c
 * Purpose :    This file contains communication methods for NetStorm and ND
 * Author  :   
 * Initial version date:    17/04/14
 *****************************************************************/

#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <libgen.h>
#include <v1/topolib_structures.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "url.h"
#include "util.h"
#include "ns_log.h"
#include "wait_forever.h"
#include "ns_msg_com_util.h"
#include "ns_parent.h"
#include "ns_event_log.h"
#include "ns_event_id.h"
#include "ns_msg_com_util.h"
#include "ns_custom_monitor.h"
#include "ns_dynamic_vector_monitor.h"
#include "ns_user_monitor.h"
#include "ns_batch_jobs.h"
#include "ns_check_monitor.h"
#include "ns_mon_log.h"
#include "nslb_util.h"
#include "nslb_sock.h"
//#include "ns_lps.h"
#include "nslb_date.h"
#include "nslb_cav_conf.h"
#include "ns_trace_level.h"
#include "ns_string.h"
#include "ns_gdf.h"
#include "nslb_util.h"
#include "ns_alloc.h"
#include "ns_ndc.h"
#include "ns_standard_monitors.h"
#include "ns_ndc_outbound.h"
#include "ns_dynamic_vector_monitor.h"
#include "ns_auto_scale.h"
#include "ns_exit.h"
#include "ns_appliance_health_monitor.h"
#include "ns_custom_monitor_RDnew.h"
#include "ns_pre_test_check.h"
#include "ns_error_msg.h"
#include "ns_monitor_init.h"

#define NDC_ANY_SERVER_CHECK 1

#define NODE_DISCOVERY 1
#define NODE_MODIFY 2
#define END_MONITOR 3
#define NODE_INACTIVE 4
#define INSTANCE_INACTIVE 5
#define INSTANCE_DELETE 6
#define SERVER_DELETE 7
#define CMON_INSTANCE_DOWN 8
#define CMON_INSTANCE_UP 9
#define CMON_CONN_RETRY 10
#define SERVER_ADD 0X01
#define TIER_ADD 0X02
#define INSTANCE_ADD 0X04
#define MONITOR_ADD 0X08
#define CMON_ADD 0x10
#define NDC_READ_BUF_SIZE ((16 * 1024) - 1)
//InstancePidTbl *inst_pid = NULL;
//static char ndc_read_msg[1023 + 1];
static char ndc_read_msg[NDC_READ_BUF_SIZE + 1];
static int ndc_left_bytes_from_prev = 0;
static int ndc_state = 0;
static char partial_buf[NDC_READ_BUF_SIZE + 1];
double g_ndc_start_time;
char ndc_connection_state = 0;
char ndc_send_req = 0;

int total_ndc_node_msg = 0;
int g_check_nd_overall_to_delete = 0;
int max_ndc_node_msg = 0;
char **ndc_node_msg = NULL;

PercentileList *percentile_list_head_ptr = NULL;
TierList *tier_list_head_ptr = NULL;
NodeList *node_list;

extern int kw_set_runtime_monitors(char *buf, char *err_msg);
extern int get_tier_id_from_tier(int tier_id);
/*Function used toerarchical_view_vector_separatorhierarchical_view_topology_name create message for NDC*/
static void create_ndc_msg(char *msg_buf)
{
  pid_t ns_pid = getpid();
  NSDL1_MON(NULL, NULL, "Method called");
  
  sprintf(msg_buf, "nd_control_req:action=start_net_diagnostics;");
  sprintf(msg_buf + strlen(msg_buf), "TEST_RUN=%d;NS_WDIR=%s;READER_RUN_MODE=%d;"
                   "TIME_STAMP=%llu;ND_PROFILE=%s;NS_EVENT_LOGGER_PORT=%hu;NS_PID=%d;MON_FREQUENCY=%d;"
                   "CAV_EPOCH_DIFF=%ld;PARTITION_IDX=%lld;START_PARTITION_IDX=%lld;ND_VECTOR_SEPARATOR=%c;TOPOLOGY_NAME=%s;MACHINE_TYPE=%s;"
                   "MULTIDISK_RAWDATA_PATH=%s;MULTIDISK_NDLOGS_PATH=%s;MULTIDISK_PROCESSED_DATA_PATH=%s;MULTIDISK_PERCENTILE_DATA_PATH=%s;MACHINE_OP_TYPE=%d;\n", 
                   testidx, g_ns_wdir, global_settings->reader_run_mode, get_ns_start_time_in_secs(), global_settings->nd_profile_name, 
                   event_logger_port_number, ns_pid, global_settings->progress_secs,
               global_settings->unix_cav_epoch_diff, g_partition_idx, g_start_partition_idx,global_settings->hierarchical_view_vector_separator, 
                   global_settings->hierarchical_view_topology_name, g_cavinfo.config, 
                   global_settings->multidisk_rawdata_path?global_settings->multidisk_rawdata_path:"",
                   global_settings->multidisk_ndlogs_path?global_settings->multidisk_ndlogs_path:"",
                   global_settings->multidisk_processed_data_path?global_settings->multidisk_processed_data_path:"", 
                   global_settings->multidisk_percentile_data_path?global_settings->multidisk_percentile_data_path:"",loader_opcode);
}


//This is called when we need to delete nd vectors. It is called from 3 case:
//instance_reset -> on data connection for all ND monitors expect flow path stats.
//nd_control_req:action=monitor_instance_reset;instanceid=1;
//action=pid -> on control connection (for all instance_reset. we dont get reset message for fp_stats, we need to delete vector of fp on this message.
//nd_control_info:action=pid;NodeJS:CAV-QA-30-26:Instance1=1192;
void delete_vector_matched_inst_id(int inst_id, char mark_delete_flag)
{
  int mon_idx = 0, vec_idx = 0, z;

  CM_info *cus_mon_ptr = NULL;
  CM_vector_info *vector_list = NULL;
  
  for (mon_idx=0; mon_idx < total_monitor_list_entries; mon_idx++)
  { 
    cus_mon_ptr = monitor_list_ptr[mon_idx].cm_info_mon_conn_ptr;

    if(cus_mon_ptr->flags & ND_MONITOR)
    {
      vector_list = cus_mon_ptr->vector_list;
      MLTL2(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "cus_mon_ptr->monitor_name = [ %s] \n", cus_mon_ptr->monitor_name);

      for(vec_idx = 0; vec_idx < cus_mon_ptr->total_vectors; vec_idx++)
      {
        if((vector_list[vec_idx].inst_actual_id > -1) && (vector_list[vec_idx].inst_actual_id == inst_id) && !(vector_list[vec_idx].flags & OVERALL_VECTOR))
        {
           MLTL2(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "vector_list[vec_idx].vector_name = [ %s] , inst_id = [ %d] vector_list[vec_idx].vectorIdx = [%d] vector_list[vec_idx].instanceIdx = [%d]\n", vector_list[vec_idx].vector_name, inst_id, vector_list[vec_idx].vectorIdx, vector_list[vec_idx].instanceIdx);   
        
          //point data to dummy data "0". Now data will be shown 0 for this monitor. We are not changing overall as those might 
          //be getting data from other instances. Putting data to 0 might disturb the averages calculated by overall as the 
          //data for this vector will be considered in calculation.
          if((vector_list[vec_idx].vectorIdx >= 0) && (vector_list[vec_idx].instanceIdx >= 0))
          {
           
            (cus_mon_ptr->instanceVectorIdxTable)[vector_list[vec_idx].mappedVectorIdx][vector_list[vec_idx].instanceIdx]->reset_and_validate_flag |= ND_RESET_FLAG;
            (cus_mon_ptr->instanceVectorIdxTable)[vector_list[vec_idx].mappedVectorIdx][vector_list[vec_idx].instanceIdx]->reset_and_validate_flag &= ~GOT_ND_DATA;

            vector_list[vec_idx].data = nan_buff;

            vector_list[vec_idx].vectorIdx = -1;
            vector_list[vec_idx].instanceIdx = -1;
            vector_list[vec_idx].tierIdx = -1;
            vector_list[vec_idx].mappedVectorIdx = -1;
          }
          if(mark_delete_flag)
          {
              
            vector_list[vec_idx].flags |= RUNTIME_RECENT_DELETED_VECTOR;
            // set "nan" on vector deletion
            for(z = 0; z < cus_mon_ptr->no_of_element; z++)
            {
              vector_list[vec_idx].data[z] = 0.0/0.0;
            }
 
            if((vector_list[vec_idx].vector_state != CM_DELETED) && !(cus_mon_ptr->flags & ALL_VECTOR_DELETED) && !(vector_list[vec_idx].flags & WAITING_DELETED_VECTOR))
            {
              if(cus_mon_ptr->total_deleted_vectors >= cus_mon_ptr->max_deleted_vectors)
              {
                MLTL2(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                       "Monitor name: %s, total_deleted_vectors = %d and max_deleted_vectors = %d", cus_mon_ptr->monitor_name, cus_mon_ptr->total_deleted_vectors, cus_mon_ptr->max_deleted_vectors);  
             
                create_entry_in_reused_or_deleted_structure(&cus_mon_ptr->deleted_vector, &cus_mon_ptr->max_deleted_vectors);
                MLTL2(EL_DF, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                      "After create_entry_in_reused_or_deleted_structure Now total_entries = %d max_entries = %d", cus_mon_ptr->total_deleted_vectors, cus_mon_ptr->max_deleted_vectors); 
              } 
              cus_mon_ptr->deleted_vector[cus_mon_ptr->total_deleted_vectors] = vec_idx;

              /*if((cus_mon_ptr->is_group_vector) && (cus_mon_ptr->dyn_num_vectors > 1) && (g_enable_new_gdf_on_partition_switch))
                make_new_parent(cus_mon_ptr);*/
              if(!g_enable_delete_vec_freq)
              {
                vector_list[vec_idx].flags |= RUNTIME_DELETED_VECTOR;
                vector_list[vec_idx].vector_state = CM_DELETED;
	        total_deleted_vectors++;
                monitor_deleted_on_runtime = 1;
                monitor_runtime_changes_applied = 1;
              }
              else
              {
                vector_list[vec_idx].flags |= WAITING_DELETED_VECTOR;
                total_waiting_deleted_vectors++;
              }

              cus_mon_ptr->total_deleted_vectors++;
            }
          }
        }
      }
    }     
  }
}

// T1:S1:I1:877;T1:S1:I1:877;T1:S1:I1:877;
char *parse_and_extract(char *buf, int reset_ndc_state)
{
  char *fields[4];
  int num_field = 0, line_len = 0, inst_id = -1;
  char *tmp_ptr = NULL;
  char *line_ptr = NULL;
  char *line = NULL;
  int check_null_flag = 0; 

  line_ptr = buf;    
  
  while((tmp_ptr = strchr(line_ptr, ';')) || ((reset_ndc_state == 1) && (tmp_ptr = strchr(line_ptr, '\0'))))
  {
    if(!(strchr(line_ptr, ';')))
      check_null_flag = 1;
    
    *tmp_ptr = '\0';
    line = line_ptr;
    line_len = strlen(line) + 1;
    if(strstr(line, "Message=Warning:") != NULL)
    {
      line_ptr = tmp_ptr + 1;
      ndc_left_bytes_from_prev -= line_len;    

      if(check_null_flag == 1)
        break; 

      continue;
    }

    // skipping 'nd_control_rep:action=start_net_diagnostics;result=Warning;'    
    if((strstr(line, "action=") == NULL) && (strstr(line, "result=") == NULL))
    { 
      num_field = get_tokens_with_multi_delimiter(line, fields, "=:", 4);

      if(num_field < 4)
      {
        if(num_field == 3)
        {
          //Moving this log to NSTL2 because lots of log is coming in auto scaling for the server which is no more act
          MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Not in proper format hence skipping line. num_field = %d : fields[0] = %s, fields[1] = %s, fields[2] = %s", num_field, fields[0], fields[1] , fields[2]);
        }
      }
      else 
      {
        topolib_search_and_fill(fields[0], fields[1], fields[2], atoi(fields[3]), &inst_id, topo_idx); //fill in structure

        //if instance id reset then delete vectors having matching instance id
        if(inst_id != -1)
        {
          delete_vector_matched_inst_id(inst_id, 0);
          inst_id = -1;
        }
      }
    }
    //line_ptr = tmp_ptr + 1;
    if(check_null_flag == 1)
    {
      if(*line_ptr == '\0')
        line_ptr++; //pointing to null, hence do +1
      break; 
    }
    else
    {
      line_ptr = tmp_ptr + 1;
      ndc_left_bytes_from_prev -= line_len;   
    }
  }
  return(line_ptr);
}

int apply_check_monitor_on_server(CheckMonitorInfo *check_mon_ptr, int server_idx)
{
  char monitor_buf[32*2024];
  char err_msg[1024];

  int tier_idx = check_mon_ptr->tier_idx;

  JSON_info *json_element_ptr;
  MY_MALLOC_AND_MEMSET(json_element_ptr, sizeof(JSON_info), "JSON Info Ptr", -1);

  if(check_mon_ptr->instance_name)
  {
    MY_MALLOC(json_element_ptr->instance_name, (strlen(check_mon_ptr->instance_name) + 1), "Check monitor Name", -1);
    strcpy(json_element_ptr->instance_name, check_mon_ptr->instance_name);
  }

  if(check_mon_ptr->mon_name)
  {
     MY_MALLOC(json_element_ptr->mon_name, (strlen(check_mon_ptr->mon_name) + 1), "Check monitor Name", -1);
     strcpy(json_element_ptr->mon_name, check_mon_ptr->mon_name);
  }

  if(check_mon_ptr->json_args)
  {
     MY_MALLOC(json_element_ptr->args, (strlen(check_mon_ptr->json_args) + 1), "Check monitor Name", -1);
     strcpy(json_element_ptr->args, check_mon_ptr->json_args);
  }

  json_element_ptr->any_server = check_mon_ptr->any_server;
  
  //this check is or batch job monitor 
  if(check_mon_ptr->monitor_type == CHECK_MON_IS_BATCH_JOB)
  {
    //here we make monitor buf for batch job
    sprintf(monitor_buf,"BATCH_JOB %s%c%s %s %s",topo_info[topo_idx].topo_tier_info[tier_idx].tier_name, global_settings->hierarchical_view_vector_separator, topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->server_name ,check_mon_ptr->mon_name, check_mon_ptr->json_args);

    if(parse_job_batch(monitor_buf, 1,err_msg, " ",json_element_ptr) < 0)
    {
      NSDL1_MON(NULL, NULL, "Monitor with monitor buf '%s' not applied, ERROR : '%s' ", monitor_buf, err_msg);
      return -1;
    }
  }
  else
  {
    //here we make monitor buf for batch job
    sprintf(monitor_buf,"CHECK_MONITOR %s %s%c%s%c%s %s",topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->server_name ,topo_info[topo_idx].topo_tier_info[tier_idx].tier_name, global_settings->hierarchical_view_vector_separator,topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_disp_name ,global_settings->hierarchical_view_vector_separator ,check_mon_ptr->mon_name, check_mon_ptr->json_args);

    if(kw_set_check_monitor("CHECK_MONITOR", monitor_buf, 1, err_msg, json_element_ptr) < 0)
    {
      NSDL1_MON(NULL, NULL, "Monitor with monitor buf '%s' not applied, ERROR : '%s' ", monitor_buf, err_msg);
      return -1;
    }
  }
   
  if(json_element_ptr)
  {
    FREE_AND_MAKE_NULL(json_element_ptr->instance_name, "json_info_ptr->instance_name", 0);
    FREE_AND_MAKE_NULL(json_element_ptr->mon_name, "json_info_ptr->mon_name", 0);
    FREE_AND_MAKE_NULL(json_element_ptr->args, "json_info_ptr->args", 0);
    FREE_AND_MAKE_NULL(json_element_ptr, "json_info_ptr", 0);
  }

  return 1;
}

//update the source tier info with deletion and updation
int update_source_tier_info( CM_info *cm_info_ptr ,int tier_idx ,int delete_flag ,int index ,int next_server_idx)
{
  char mon_list_name[MAX_DATA_LINE_LENGTH]; //this is used for making the mon name for monitor

  int dest_tier_id;
  int source_tier_idx;
  
  Mon_List_Info *mon_list; //this pointer is used for monitor list info

  for(dest_tier_id = 0; dest_tier_id<total_dest_tier; dest_tier_id++)
  {
    for(source_tier_idx=0; source_tier_idx < dest_tier_info[dest_tier_id].total_source_tier ;source_tier_idx++ )
    {
      if(dest_tier_info[dest_tier_id].source_tier_info[source_tier_idx].source_tier_id == tier_idx)
      {
        //for checking mon_name present in mon list
        sprintf(mon_list_name, "%s%c%s%c%s" , topo_info[topo_idx].topo_tier_info[tier_idx].tier_name ,global_settings->hierarchical_view_vector_separator , topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[index].server_disp_name , global_settings->hierarchical_view_vector_separator , dest_tier_info[dest_tier_id].dest_server_ip);
        
        //search mon name from mon_list
        mon_list = dest_tier_info[dest_tier_id].source_tier_info[source_tier_idx].mon_list_info_pool.busy_head;

        while (mon_list != NULL)
        {
          if(strcmp(topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[index].server_disp_name, mon_list->source_server_ip) == 0) //if mon name matched
          {
            if(delete_flag == 1)
            {
              //free mon_list and return slot to mp pool
              NSDL1_MON(NULL, NULL, "Remove entry from dest_tier_info struct of '%s' monitor which is applied on %s server", mon_list->monitor_name,topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[index].server_disp_name);
              free_mon_list_info(mon_list);
              nslb_mp_free_slot(&(dest_tier_info[dest_tier_id].source_tier_info[source_tier_idx].mon_list_info_pool), mon_list);
            }
            else
            {
              //for checking mon_name present in mon list
              sprintf(mon_list_name, "%s%c%s%c%s" ,topo_info[topo_idx].topo_tier_info[tier_idx].tier_name ,global_settings->hierarchical_view_vector_separator , topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[index].server_disp_name , global_settings->hierarchical_view_vector_separator , dest_tier_info[dest_tier_id].dest_server_ip);

              NSDL1_MON(NULL, NULL, "Change source server ip of %s monitor from dest_tier_info struct with %s server", mon_list->monitor_name,topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[index].server_disp_name);
              FREE_AND_MAKE_NULL( mon_list->source_server_ip ,"Freeing source server ip of mon list", -1); 
              MALLOC_AND_COPY(topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[index].server_disp_name ,mon_list->source_server_ip , (strlen(topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[index].server_disp_name) + 1), "Copying source server ip",0 );//store new src ip
              FREE_AND_MAKE_NULL( mon_list->mon_name ,"Freeing source server ip of mon list", -1);
              MALLOC_AND_COPY( mon_list_name ,mon_list->mon_name , (strlen(mon_list_name) + 1), "Copying source server ip",0 );//store new src ip
            }
          }
          mon_list = nslb_next(mon_list);
        }
        break;
      }
    }
  }
  return 1;
}

//apply monitor on inactive destination tier server which are autoscaled
int add_monitors_for_inactive_dest_server( int Server_Id , int Tier_Id)
{
  char monitor_buf[32*MAX_DATA_LINE_LENGTH];
  char temp_monitor_buf[32*MAX_DATA_LINE_LENGTH];
  char mon_list_name[MAX_DATA_LINE_LENGTH]; //this is used for making the mon name for monitor
  char tmp_buf[MAX_DATA_LINE_LENGTH];
  char prgrm_args[2*MAX_DATA_LINE_LENGTH];
  char leftover_arguments[MAX_DATA_LINE_LENGTH];
  char err_msg[MAX_DATA_LINE_LENGTH];
  

  char *variable_ptr =NULL;
  int dest_tier_name_norm_id;
  int source_tier_id;  //loop variable for source tier names
  int mon_ret = -1;
   
  Mon_List_Info *mon_list;
  //wrong tied id
  dest_tier_name_norm_id =nslb_get_norm_id(dest_tier_name_id_key,topo_info[topo_idx].topo_tier_info[Tier_Id].tier_name ,strlen(topo_info[topo_idx].topo_tier_info[Tier_Id].tier_name));
  
  //check destination tier name is present in normalized table or not ..if not do nothing 
  if ( dest_tier_name_norm_id != -2 )
  {
    //check destination server ip active or not
    if(!dest_tier_info[dest_tier_name_norm_id].dest_server_ip_active)
    {
      //fill destination tier info structure here not copy destination tier name i.e already filled
      dest_tier_info[dest_tier_name_norm_id].dest_server_ip_active = true; // for checking destination server ip active or not
      REALLOC_AND_COPY(topo_info[topo_idx].topo_tier_info[Tier_Id].topo_server[Server_Id].server_disp_name ,dest_tier_info[dest_tier_name_norm_id].dest_server_ip, strlen(topo_info[topo_idx].topo_tier_info[Tier_Id].topo_server[Server_Id].server_disp_name) ," Copying destination server ip" , 0); //copying destination server ip

      //here we are looping total source tier
      for(source_tier_id =0; source_tier_id < dest_tier_info[dest_tier_name_norm_id].total_source_tier; source_tier_id++)
      {
        mon_list =dest_tier_info[dest_tier_name_norm_id].source_tier_info[source_tier_id].mon_list_info_pool.busy_head;

        //traversing monitor list to get monitor info
        while (mon_list != NULL)
        {
          sprintf(mon_list_name , "%s%c%s%c%s", dest_tier_info[dest_tier_name_norm_id].source_tier_info[source_tier_id].source_tier_name, global_settings->hierarchical_view_vector_separator ,mon_list->source_server_ip , global_settings->hierarchical_view_vector_separator , dest_tier_info[dest_tier_name_norm_id].dest_server_ip); //this buffer is used for mon_name in mon list

          REALLOC_AND_COPY( mon_list_name ,mon_list->mon_name, (strlen(mon_list_name) + 1), "Copying monitor name",0); //store mon name for deletion purpose
          MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Add monitor of destination tier whose vector name is [%s]. and monitor name is %s", mon_list->mon_name,mon_list->monitor_name);          
          //make program args of monitor
          variable_ptr = NULL;
          strcpy(prgrm_args, mon_list->prgrm_args);
          variable_ptr=strstr(prgrm_args, "%cav_tier_any_server%");
          strcpy(leftover_arguments, variable_ptr + 21);
          strcpy(variable_ptr,topo_info[topo_idx].topo_tier_info[Tier_Id].topo_server[Server_Id].server_ptr->server_name);
          strcpy(variable_ptr + strlen(topo_info[topo_idx].topo_tier_info[Tier_Id].topo_server[Server_Id].server_ptr->server_name),leftover_arguments);

          //make monitor buf for every monitor present on server
          if(enable_store_config && !strcmp( dest_tier_info[dest_tier_name_norm_id].source_tier_info[source_tier_id].source_tier_name , "Cavisson"))
            sprintf(tmp_buf, "%s!%s", g_store_machine, dest_tier_info[dest_tier_name_norm_id].source_tier_info[source_tier_id].source_tier_name);
          else
            sprintf(tmp_buf, "%s", dest_tier_info[dest_tier_name_norm_id].source_tier_info[source_tier_id].source_tier_name);

          //here if we found %cav_tier_any_server% in instance then we make hierachy Source_tier>Source_server_name>Destination_server_name
          sprintf(monitor_buf,"STANDARD_MONITOR %s%c%s %s%c%s%c%s %s %s", dest_tier_info[dest_tier_name_norm_id].source_tier_info[source_tier_id].source_tier_name, global_settings->hierarchical_view_vector_separator, mon_list->source_server_ip, tmp_buf, global_settings->hierarchical_view_vector_separator, mon_list->source_server_ip , global_settings->hierarchical_view_vector_separator,dest_tier_info[dest_tier_name_norm_id].dest_server_ip,mon_list->monitor_name, prgrm_args);

          //apply monitor
          strcpy(temp_monitor_buf,monitor_buf);
           
          REALLOC_AND_COPY( dest_tier_info[dest_tier_name_norm_id].dest_server_ip ,mon_list->json_info_ptr->instance_name, (strlen(dest_tier_info[dest_tier_name_norm_id].dest_server_ip) + 1), "Copying instance name",0);
          
          MLTL4(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Add monitor of destination tier whose monitor buf is %s",monitor_buf); 
          if((mon_ret = kw_set_standard_monitor("STANDARD_MONITOR", monitor_buf, 1, 0, err_msg, mon_list->json_info_ptr)) < 0 )
            MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                                 "Failed to apply json auto monitor with buffer: %s . Error: %s", temp_monitor_buf, err_msg);

          mon_list = nslb_next(mon_list);
        } //move to next monitor      
      } //source tier loop
      //topo_server_info[Server_Id].auto_monitor_applied = 1;
    } //destination tier source ip is active
    else
    {
      NSDL2_MON(NULL, NULL, "Not Applying monitor with destination any server tag on server '%s' with server_idx = %d. Server is already active on %s tier",topo_info[topo_idx].topo_tier_info[Tier_Id].topo_server[Server_Id].server_disp_name, Server_Id, topo_info[topo_idx].topo_tier_info[Tier_Id].tier_name);
    }
  }
  else
  {
    NSDL2_MON(NULL, NULL, "Not Applying monitor with destination any server tag on server '%s' with server_idx = %d. %s tier is not used", topo_info[topo_idx].topo_tier_info[Tier_Id].topo_server[Server_Id].server_disp_name, Server_Id, topo_info[topo_idx].topo_tier_info[Tier_Id].tier_name);
  }
  return 1;
}



//this fuction will change the element of cm_info_ptr according to server at server_idx and free vector all the vector list
int apply_monitor_on_server(CM_info *cm_info_ptr, int server_idx, int index)
{
  NSDL1_MON(NULL, NULL, "Applying monitor '%s' with any tag on server '%s' with server_idx = %d", cm_info_ptr->pgm_path, topo_info[topo_idx].topo_tier_info[cm_info_ptr->tier_index].topo_server[server_idx].server_disp_name, server_idx);

  char err_msg[1024];
  char mon_type[15];
  char monitor_buf[32*2024];
 
  int tier_idx = cm_info_ptr->tier_index;

  if(!(cm_info_ptr->flags & USE_LPS) || !strncmp(cm_info_ptr->gdf_name, "cm_access_", 10))
    sprintf(mon_type, "CUSTOM_MONITOR");
  else
    sprintf(mon_type, "LOG_MONITOR");
  
  //make dest mon function which makes the monitor buf arr and pass to function
  if (cm_info_ptr->dest_any_server)
  {
    update_source_tier_info( cm_info_ptr ,tier_idx , 0 , index ,server_idx);  //change source ip for destination tier info
  } 
  if(cm_info_ptr->instance_name)
   sprintf(monitor_buf, "%s %s%c%s %s %s%c%s%c%s 2 %s %s", mon_type, cm_info_ptr->tier_name, global_settings->hierarchical_view_vector_separator,topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_disp_name, cm_info_ptr->gdf_name_only, cm_info_ptr->tier_name, global_settings->hierarchical_view_vector_separator,topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_disp_name, global_settings->hierarchical_view_vector_separator, cm_info_ptr->instance_name, cm_info_ptr->pgm_path, cm_info_ptr->pgm_args);
  else
    sprintf(monitor_buf,"%s %s%c%s %s %s%c%s 2 %s %s", mon_type, cm_info_ptr->tier_name, global_settings->hierarchical_view_vector_separator, topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_disp_name, cm_info_ptr->gdf_name_only, cm_info_ptr->tier_name, global_settings->hierarchical_view_vector_separator, topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_disp_name, cm_info_ptr->pgm_path, cm_info_ptr->pgm_args);

  JSON_info *json_element_ptr;
  MY_MALLOC_AND_MEMSET(json_element_ptr, sizeof(JSON_info), "JSON Info Ptr", -1);
    
  if(cm_info_ptr && json_element_ptr)
  {
    if(cm_info_ptr->vectorReplaceFrom)
    {
      MALLOC_AND_COPY(cm_info_ptr->vectorReplaceFrom, json_element_ptr->vectorReplaceFrom, (strlen(cm_info_ptr->vectorReplaceFrom) + 1), "Copying elements from cm_info file: vectorReplaceFrom", 0);
    }

    if(cm_info_ptr->vectorReplaceTo)
    {
      MALLOC_AND_COPY(cm_info_ptr->vectorReplaceTo, json_element_ptr->vectorReplaceTo, (strlen(cm_info_ptr->vectorReplaceTo) + 1), "Copying elements from cm_info file vectorReplaceTo", 0);
    }

    if(cm_info_ptr->config_file)
    {
      MALLOC_AND_COPY(cm_info_ptr->config_file, json_element_ptr->config_json, (strlen(cm_info_ptr->config_file) + 1), "Copying elements from json cm_info config json", 0);
    }
   
    if(cm_info_ptr->instance_name)
    {
      MALLOC_AND_COPY(cm_info_ptr->instance_name, json_element_ptr->instance_name, (strlen(cm_info_ptr->instance_name) + 1), "Copying elements from cm_info instance name", 0);
    }

    //if(cm_info_ptr->init_vector_file_flag)
      //json_element_ptr->init_vector_file = 1;

    MLTL2(EL_DF, 0, 0, _FLN_, cm_info_ptr, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "VectorReplaceFrom field is preset in cm_info for gdf->%s, vectorReplaceFrom->%s, vectorReplaceTo->%s, tier_server_mapping_type->%d", cm_info_ptr->gdf_name, cm_info_ptr->vectorReplaceFrom, cm_info_ptr->vectorReplaceTo, cm_info_ptr->tier_server_mapping_type); 

    json_element_ptr->tier_server_mapping_type = cm_info_ptr->tier_server_mapping_type;
    json_element_ptr->any_server = cm_info_ptr->any_server;
    json_element_ptr->dest_any_server_flag = cm_info_ptr->dest_any_server;
    json_element_ptr->frequency = cm_info_ptr->frequency /1000;
    json_element_ptr->is_process = cm_info_ptr->is_process;
    // this is for monconfig struct
    if(cm_info_ptr->g_mon_id)
    {
      MALLOC_AND_COPY(cm_info_ptr->g_mon_id, json_element_ptr->g_mon_id, strlen(cm_info_ptr->g_mon_id) + 1, "cm_info_ptr g_mon_id", -1);
    }
    json_element_ptr->mon_info_index = cm_info_ptr->mon_info_index;
  }

  if(custom_monitor_config(mon_type, monitor_buf, topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_disp_name, monitor_list_ptr[cm_info_ptr->monitor_list_idx].is_dynamic, 1, err_msg, cm_info_ptr->pod_name, json_element_ptr, cm_info_ptr->skip_breadcrumb_creation) < 0)
  {
    NSDL1_MON(NULL, NULL, "Monitor with monitor buf '%s' not applied, ERROR : '%s' ", monitor_buf, err_msg);
    return -1; 
  }
  else
  {
    g_mon_id = get_next_mon_id();
    monitor_added_on_runtime = 1;
  }

  //here we add total appname and appname in new cm_ptr structure for NA_KUBER monitor
  //cm_info_ptr is the old CM info ptr pointer that will be added in new cm_ptr.
  if(cm_info_ptr->appname_pattern && (cm_info_ptr->appname_pattern[0]) && (cm_info_ptr->appname_pattern[0][0] != '\0'))
  {
     int aj;
     CM_info *cm_ptr = monitor_list_ptr[total_monitor_list_entries - 1].cm_info_mon_conn_ptr;
    
     MY_MALLOC(cm_ptr->appname_pattern,((cm_info_ptr->total_appname_pattern+1) * sizeof(char *)), "Allocating for appname pattern row", (total_monitor_list_entries - 1));

     for(aj=0; aj < cm_info_ptr->total_appname_pattern; aj++)
     {
       MY_MALLOC(cm_ptr->appname_pattern[aj],strlen(cm_info_ptr->appname_pattern[aj])+1, "Allocating for appname pattern", aj);
       strcpy(cm_ptr->appname_pattern[aj], cm_info_ptr->appname_pattern[aj]);
     }
     cm_ptr->total_appname_pattern = cm_info_ptr->total_appname_pattern;
   }

   if(json_element_ptr)
    {
      FREE_AND_MAKE_NULL(json_element_ptr->vectorReplaceFrom, "json_info_ptr->vectorReplaceFrom", 0);
      FREE_AND_MAKE_NULL(json_element_ptr->vectorReplaceTo, "json_info_ptr->vectorReplaceTo", 0);
      FREE_AND_MAKE_NULL(json_element_ptr->javaClassPath, "json_info_ptr->javaClassPath", 0);
      FREE_AND_MAKE_NULL(json_element_ptr->javaHome, "json_info_ptr->javaHome", 0);
      FREE_AND_MAKE_NULL(json_element_ptr->init_vector_file, "json_info_ptr->init_vector_file", 0);
      FREE_AND_MAKE_NULL(json_element_ptr->instance_name, "json_info_ptr->instance_name", 0);
      FREE_AND_MAKE_NULL(json_element_ptr->mon_name, "json_info_ptr->mon_name", 0);
      FREE_AND_MAKE_NULL(json_element_ptr, "json_info_ptr", 0);
    }

  return 1;
}

//move in topology
//this function will get the next active server leaving the monitor at "index"
static int get_next_active_server_idx(int tier_idx, int index)
{
  int server_idx = 0;

  NSDL1_MON(NULL, NULL, "get_next_active_server_idx method called for finding next active server in tier '%s' with tier_idx = %d", topo_info[topo_idx].topo_tier_info[tier_idx].tier_name, tier_idx);
  for(server_idx = 0; server_idx < topo_info[topo_idx].topo_tier_info[tier_idx].total_server; server_idx++)
  {
    if((topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].used_row != -1) && (topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->status == 1) && (server_idx !=  index && topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->cmon_control_conn_down == 0))
    return server_idx;
    
  }
  return -1;
}

//delete and add destination tier server monitors
int delete_and_add_dest_monitors( int dest_tier_name_norm_id, int tier_idx , int index)
{
  char monitor_buf[32*MAX_DATA_LINE_LENGTH];
  char fname[MAX_DATA_LINE_LENGTH];
  char tmp_buf[512];
  char err_msg[MAX_DATA_LINE_LENGTH];
  char group_id[8];

  int next_server_idx;
  int source_tier_id;
  
  Mon_List_Info *mon_list; 
  //if compare if dest server in dest tier = serder idx  
  if(dest_tier_info[dest_tier_name_norm_id].dest_server_ip_active)
  {
    if(strncmp(dest_tier_info[dest_tier_name_norm_id].dest_server_ip,topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[index].server_disp_name,strlen(topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[index].server_disp_name))==0)
    {
      dest_tier_info[dest_tier_name_norm_id].dest_server_ip_active = false; // make destination server ip inactive
      for(source_tier_id=0; source_tier_id < dest_tier_info[dest_tier_name_norm_id].total_source_tier ;source_tier_id++ )
      {
        mon_list = dest_tier_info[dest_tier_name_norm_id].source_tier_info[source_tier_id].mon_list_info_pool.busy_head;
      
        //traversing monitor list to get monitor info
        while (mon_list != NULL)
        {
          strcpy(tmp_buf ,mon_list->gdf_name);
          sprintf(fname, "%s/sys/%s", g_ns_wdir, tmp_buf);
          get_group_id_from_gdf_name(fname, group_id);

          MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Delete monitor of destination tier whose vector name is [%s]. and monitor name is %s", mon_list->mon_name,mon_list->monitor_name);

          sprintf(monitor_buf,"DELETE_MONITOR GroupMon %s NA %s%c%s%c%s %s", group_id, dest_tier_info[dest_tier_name_norm_id].source_tier_info[source_tier_id].source_tier_name, global_settings->hierarchical_view_vector_separator, mon_list->source_server_ip ,global_settings->hierarchical_view_vector_separator, dest_tier_info[dest_tier_name_norm_id].dest_server_ip ,tmp_buf);
       
          MLTL4(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Delete monitor of destination tier whose monitor buf is %s",monitor_buf);
          //delete monitor
          kw_set_runtime_monitors(monitor_buf, err_msg);

          if(err_msg != '\0')
            MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION, "Error %s",err_msg);
          
          mon_list = nslb_next(mon_list);
          err_msg[0] = '\0';
        }
      }
      //deleted monitor on down destination server
      next_server_idx = get_next_active_server_idx(tier_idx, index);
      if( next_server_idx != -1)
      {  
        //this function is for destination tier server 
        add_monitors_for_inactive_dest_server(next_server_idx , tier_idx);
      }
      else
      {
        NSDL1_MON(NULL, NULL, "Monitor with tier-for-any-server tag not applied bcoz of no next active server present");
      }
    }
  }
  return 1;
}

//remove node from node list
//this fucntion is call for NA_KUBER monitor
//in which we remove the deleted cmon pod node list from node list So that when we receive cmon pod on same node then we apply monitor again.
void remove_node_from_node_list(CM_info *cus_mon_ptr)
{
  int node_idx, norm_id;

  NodePodInfo *add_pod_list_ptr;
  NodePodInfo *delete_pod_list_ptr;
  NodePodInfo *head_pod_list_ptr;

  for(node_idx=0; node_idx<node_list_entries; node_idx++)
  {
    if(node_list[node_idx].CmonPodName && (strcmp(cus_mon_ptr->server_display_name, node_list[node_idx].CmonPodName) == 0))
    {
      node_list[node_idx].CmonPodIp[0] = '\0';
      node_list[node_idx].NodeIp[0] = '\0';
      FREE_AND_MAKE_NULL(node_list[node_idx].CmonPodName, "Node name", -1);
      add_pod_list_ptr = node_list[node_idx].AddedList;
      delete_pod_list_ptr = node_list[node_idx].DeletedList;
      head_pod_list_ptr = node_list[node_idx].head;
      if(node_list[node_idx].AddedList != NULL)
      {
        while(add_pod_list_ptr != NULL)
        {
          nslb_delete_norm_id_ex(node_id_key, add_pod_list_ptr->NodeName, strlen(add_pod_list_ptr->NodeName), &norm_id);
          free_node_pod(add_pod_list_ptr);
          add_pod_list_ptr = add_pod_list_ptr->next;
          NSTL2(NULL, NULL, "Node = %s with norm node id = %d is deleted and node_list_entries = %d",add_pod_list_ptr->NodeName, node_idx, node_list_entries);
        }
      }
      if(node_list[node_idx].DeletedList != NULL)
      {
        while(delete_pod_list_ptr != NULL)
        {
          nslb_delete_norm_id_ex(node_id_key, delete_pod_list_ptr->NodeName, strlen(delete_pod_list_ptr->NodeName), &norm_id);
          free_node_pod(delete_pod_list_ptr);
          delete_pod_list_ptr = delete_pod_list_ptr->next;
          NSTL2(NULL, NULL, "Node = %s with norm node id = %d is deleted and node_list_entries = %d",add_pod_list_ptr->NodeName, node_idx, node_list_entries);
        }
      }
      if(node_list[node_idx].head != NULL)
      {
        while(head_pod_list_ptr != NULL)
        {
          nslb_delete_norm_id_ex(node_id_key, head_pod_list_ptr->NodeName, strlen(head_pod_list_ptr->NodeName), &norm_id);
          free_node_pod(head_pod_list_ptr);
          head_pod_list_ptr = head_pod_list_ptr->next;
          NSTL2(NULL, NULL, "Node = %s with norm node id = %d is deleted and node_list_entries = %d",head_pod_list_ptr->NodeName, node_idx, node_list_entries);
        }
      }
      node_list_entries--;
      break; 
    } 
  } 
}


// Adding operation for use in case of any server
void end_mon(int index, int operation , int cm_idx, int reason,int tier_idx)
{
  int mon_id = 0, next_server_idx = -1; 
  CM_info *cm_ptr = NULL;
  CheckMonitorInfo *check_mon_ptr = NULL;
  int norm_id;
  char tmp_buf[MAX_DATA_LINE_LENGTH];
  int dest_tier_name_norm_id;
  int total_mon = total_monitor_list_entries;
  
  NSDL1_MON(NULL, NULL, "end_mon method called for index = %d , operation = %d, Tier_ID = %d", index , operation , tier_idx);
 
  if (operation == CMON_CONN_RETRY)
  {
    mon_id  = cm_idx;
    total_mon = cm_idx + 1;
  }

  //this handling is for destination tier server
  if( total_dest_tier > 0 )
  {
    dest_tier_name_norm_id = nslb_get_norm_id(dest_tier_name_id_key,topo_info[topo_idx].topo_tier_info[tier_idx].tier_name , strlen(topo_info[topo_idx].topo_tier_info[tier_idx].tier_name));
    if(dest_tier_name_norm_id != -2)
      delete_and_add_dest_monitors(dest_tier_name_norm_id, tier_idx , index);
  }

  for (; mon_id <total_mon; mon_id++)
  {
    cm_ptr = monitor_list_ptr[mon_id].cm_info_mon_conn_ptr;
   
    //this is the case when we get INSTANCE_DOWN operation and after that we get NODE_INACTIVE operation then we dont delete the monitor and applied on next active server because it is already applied when we get INSTANCE_DOWN operation.
    if(cm_ptr->any_server && cm_ptr->conn_state == CM_DELETED)
    {
      continue;
    }
     
    //if(cm_ptr->server_index == topo_server_info[index].serverinfo_idx) //Verify
    if(cm_ptr->server_index == index && cm_ptr->tier_index == tier_idx)
    {
     if(cm_ptr->any_server)
     {
       mj_delete_specific_server_hash_entry(cm_ptr);
      
     }
     else
     {
       //if we are getting INSTANCE_DOWN then we only stop and start again monitors whose any_server is true
       if(operation == CMON_INSTANCE_DOWN)
         continue;
     }

      if(cm_ptr->any_server && next_server_idx < 0) 
        next_server_idx = get_next_active_server_idx(tier_idx, index);
      
     
      if(stop_one_custom_monitor(cm_ptr, reason) == -1)
      { 
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Error in sending end_monitor to mon whose vector name is [%s]. Therefore going to close fd and mark deleted", cm_ptr->monitor_name); 
        handle_monitor_stop(cm_ptr, reason);
      }
      
      //Here we are remove node from node_list
      if(cm_ptr->flags & NA_KUBER)
      {
        remove_node_from_node_list(cm_ptr);
      }
      
      if( operation != NODE_MODIFY && cm_ptr->any_server == true && next_server_idx >= 0)
      {
        NSDL1_MON(NULL, NULL, "next_server_idx = %d having server name '%s' for applying monitors having any tag\n", next_server_idx, topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[index].server_disp_name);

        // applying monitor on next server for any tag in json
        //pass index of previous active server which is down now
        if(apply_monitor_on_server(cm_ptr, next_server_idx ,index))
        {
          NSDL1_MON(NULL, NULL, "Monitor '%s' with any tag has been applied on the next active server '%s'\n", cm_ptr->pgm_path, topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[index].server_disp_name);
        }
        else
          NSDL1_MON(NULL, NULL, "Monitor '%s' with any tag not applied on the next active server '%s'\n", cm_ptr->pgm_path, topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[index].server_disp_name);
        //This is the case when ENABLE_AUTO_JSON_MONITOR mode 2 is on.
        if(total_mon_config_list_entries > 0)
        {
          mj_increase_server_count(cm_ptr->mon_info_index);
        }
      }
      else
      {
        //this is for destination tier info struct
        if(cm_ptr->dest_any_server)
          update_source_tier_info(cm_ptr ,tier_idx , 1 ,index , next_server_idx); //delete monitor from destination monitor list
        
        //This is the case when ENABLE_AUTO_JSON_MONITOR mode 2 is on.
        if(total_mon_config_list_entries > 0)
        {
          mj_decrease_server_count(cm_ptr->mon_info_index, cm_ptr->mon_id,tier_idx);
        }
      }      
    }   
  }
  monitor_runtime_changes_applied = 1;

  for (mon_id = 0; mon_id < total_check_monitors_entries; mon_id++)
  {
    check_mon_ptr = &(check_monitor_info_ptr[mon_id]);
    if(check_mon_ptr->server_index == index && check_mon_ptr->tier_idx == tier_idx)
    {
      
      check_mon_ptr->status = CHECK_MONITOR_STOPPED;

      if(check_mon_ptr->any_server && next_server_idx < 0) 
        next_server_idx = get_next_active_server_idx(tier_idx, index);

      if( operation != NODE_MODIFY && check_mon_ptr->any_server)
      {

        if(check_mon_ptr->instance_name == NULL)
          sprintf(tmp_buf, "%s%c%s", check_mon_ptr->mon_name, global_settings->hierarchical_view_vector_separator, topo_info[topo_idx].topo_tier_info[tier_idx].tier_name);
        else
          sprintf(tmp_buf, "%s%c%s%c%s", check_mon_ptr->mon_name, global_settings->hierarchical_view_vector_separator, check_mon_ptr->instance_name, global_settings->hierarchical_view_vector_separator, topo_info[topo_idx].topo_tier_info[tier_idx].tier_name);


        //NSDL1_MON(NULL, NULL, "check monitor marked status has been been updated \n");
        nslb_delete_norm_id_ex(specific_server_id_key, tmp_buf, strlen(tmp_buf), &norm_id);
 
        NSDL1_MON(NULL, NULL, "check mon tmp_buf '%s' deleted from hash table for server '%s'\n", tmp_buf, topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[index].server_disp_name);

      }

      if( operation != NODE_MODIFY && check_mon_ptr->any_server == true && next_server_idx >= 0)
      {
        // applying check monitor on next server for any tag in json
        if(apply_check_monitor_on_server(check_mon_ptr, next_server_idx))
          NSDL1_MON(NULL, NULL, "Check monitor '%s' has been applied on the next active server '%s'\n", check_mon_ptr->mon_name, topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[index].server_disp_name);
        else
          NSDL1_MON(NULL, NULL, "Check monitor '%s' not applied on the next active server '%s'\n", check_mon_ptr->mon_name, topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[index].server_disp_name);
        //This is the case when ENABLE_AUTO_JSON_MONITOR mode 2 is on.
        if(total_mon_config_list_entries > 0)
        {
          mj_increase_server_count(check_mon_ptr->mon_info_index);
        } 
      }
      else
      {
        //This is the case when ENABLE_AUTO_JSON_MONITOR mode 2 is on.
        if(total_mon_config_list_entries > 0)
        {
          mj_decrease_server_count(check_mon_ptr->mon_info_index, check_mon_ptr->mon_id,tier_idx);
        }
      } 
    }
  }
}


void apply_monitors_at_node_discovery(int server_idx, int ret)
{
  if((global_settings->json_mode == 1) && ((global_settings->auto_json_monitors_filepath) &&
     (global_settings->auto_json_monitors_filepath[0] != '\0')))
  {
    //this function is for destination tier server
    if(total_dest_tier > 0)
      add_monitors_for_inactive_dest_server(server_idx , ret);

    if(topo_info[topo_idx].topo_tier_info[ret].topo_server[server_idx].server_ptr->topo_servers_list->total_mon_id_entries > 0)
    {
      send_monitor_request_to_NDC(server_idx, ret);
      add_json_monitors(NULL, server_idx, ret, NULL, 1, 0, 1);
    }
    else
      add_json_monitors(NULL, server_idx, ret, NULL, 1, 0, 0);
  }
  
  //This is the case when ENABLE_AUTO_JSON_MONITOR mode 2 is on.
  else if(total_mon_config_list_entries >0)
  {
    //this function is for destination tier server
    if(total_dest_tier > 0)
      add_monitors_for_inactive_dest_server(server_idx , ret);

    //if(servers_list[topo_server_info[server_idx].serverinfo_idx].total_mon_id_entries > 0)
      if(topo_info[topo_idx].topo_tier_info[ret].topo_server[server_idx].server_ptr->topo_servers_list->total_mon_id_entries > 0)
        send_monitor_request_to_NDC(server_idx, ret);
    else
      mj_apply_monitors_on_autoscaled_server(NULL, server_idx, ret, NULL);
  }
  return;
}


//Function to parse control message for adding new server/tier/instance receive from ndc
int parse_and_extract_server_info(char **data, int max_field, int operation)
{
  char *TierName = NULL,*cmonVer = NULL,*ServerIp = NULL,*cmonPid = NULL,*InstName = NULL, *OldServerIp = NULL;
  char ServerHost[512] = "\0", cmonHome[512] = "\0", javaHome[512] = "\0", MachineType[128] = "\0";
  int i, Tier_Id = -1, Server_Id = -1, Inst_Id = -1, ret = -1, server_idx = -1, agentType = 0;
  int mon_id = -1, Retry_value = -1, index = -1;
  char SendMsg[512];
  int tier_norm_idx;
  char err_buf[MAX_LINE_SIZE];
  int tier_idx =-1; 

  err_buf[0]='\0';
  //ServerInfo *servers_list = NULL;
  //servers_list = (ServerInfo *) topolib_get_server_info();

  MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Method is called for parsing control message"); 
  for(i = 0;i < max_field;i++)
  {
     if(data[i])
     { 
       if(!strncmp(data[i],"tierId=",7))
       { if(strlen(data[i] + 7) > 0)
           Tier_Id = atoi(data[i] + 7);
       }
       else if(!strncmp(data[i],"tierName=",9))
       {
         if(strlen(data[i] + 9) > 0)
           TierName = data[i] + 9;
       }
       else if(!strncmp(data[i],"serverId=",9))
       {
         if(strlen(data[i] + 9) > 0)
           Server_Id = atoi(data[i] + 9);
       }
       else if(!strncmp(data[i],"cmonVersion=",12))
       {
         if(strlen(data[i] + 12) > 0)
           cmonVer = data[i] + 12;
       }
       else if(!strncmp(data[i],"serverIp=",9))
       {
         if(strlen(data[i] + 9) > 0)
           ServerIp = data[i] + 9;
       }
       else if(!strncmp(data[i],"cmonHome=",9))
       {
         if(strlen(data[i] + 9) > 0)
           strcpy(cmonHome,(data[i] + 9));
       }
       else if(!strncmp(data[i],"cmonPid=",8))
       {
         if(strlen(data[i] + 8) > 0)
           cmonPid = data[i] + 8;
       }
       else if(!strncmp(data[i],"cmonJavaHome=",13))
       {
         if(strlen(data[i] + 13) > 0)
           strcpy(javaHome,(data[i] + 13));
       }
       //else if(!strncmp(data[i],"cmonStartTime=",14))
       
       else if(!strncmp(data[i],"instanceId=",11))
       {
         if(strlen(data[i] + 11) > 0)
           Inst_Id = atoi(data[i] + 11);
       }
       else if(!strncmp(data[i],"instance=",9))
       {
         if(strlen(data[i] + 9) > 0)
           InstName = data[i] + 9;
       }
       else if(!strncmp(data[i],"MachineType=",12))
       {
         if(strlen(data[i] + 12) > 0)
           strcpy(MachineType,(data[i] + 12));
       }
       else if(!strncmp(data[i],"serverHost=",11))
       {
         if(strlen(data[i] + 11) > 0)
           strcpy(ServerHost,(data[i] + 11));
       }
       else if(!strncmp(data[i],"oldserverIp=",12))
       {
         if(strlen(data[i] + 12) > 0)
           OldServerIp = data[i] + 12;
       }
       else if(!strncmp(data[i],"agentType=",10))
       {
         if(strlen(data[i] + 10) > 0)
           agentType = atoi(data[i] + 10);
       }
       else if (!strncmp(data[i],"retry=",6))
       { 
            if(strlen(data[i] + 6) > 0)
            Retry_value = atoi(data[i] + 6);
       }
       else if (!strncmp(data[i],"mon_id=",7))
       { 
            if(strlen(data[i] + 7) > 0)
            mon_id = atoi(data[i] + 7);
       }
           
     }  
  }
  //return tieridx & serveridx as args in topolib_add_new_server
  // if new server add server server_list structure in epoll
  if(operation == NODE_DISCOVERY)
  {
    //calling of function to add server/tier/instance
    //this function will return tier index if new server is added ortherwise -1 is return
    ret = topolib_add_new_server(Tier_Id, TierName, Server_Id, cmonVer, ServerIp, cmonHome, cmonPid, javaHome, InstName, Inst_Id, MachineType,ServerHost, max_cm_retry_count, topo_idx,err_buf,&tier_idx,&server_idx);

    NSDL2_MON(NULL, NULL,"Return value of add_new_server is %d.",ret); 
    if(cmonVer && (err_buf[0] != '\0'))
    {
      NSDL2_MON(NULL, NULL,"Server.conf does not have proper format of cmon version %s, %s", cmonVer ,err_buf);
    }

    if(topo_info[topo_idx].add_server_flag & TIER_ADD)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "New tier has been added with: tier id = %d, tier name = %s",Tier_Id,TierName);
      update_health_monitor_sample_data(&hm_data->num_auto_scaled_tiers);
    }
    if(topo_info[topo_idx].add_server_flag & INSTANCE_ADD)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "New instance has been added with: InstName = %s, Inst id = %d", InstName, Inst_Id); 
      update_health_monitor_sample_data(&hm_data->num_auto_scaled_instances);
    }
    if(topo_info[topo_idx].add_server_flag & SERVER_ADD)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "New server has been added with: ServerName = %s, Server id = %d", ServerIp, Server_Id);
      update_health_monitor_sample_data(&hm_data->num_auto_scaled_servers);
    }
    if(topo_info[topo_idx].add_server_flag & CMON_ADD)
    { 
      tier_norm_idx=topolib_get_tier_idx_from_tier_id_tf(Tier_Id,topo_idx);
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "New cmon version has been added with: ServerName = %s, Server id = %d", ServerIp, Server_Id);
      server_idx = topolib_get_topo_server_idx_from_server_id( ServerHost,topo_idx,tier_norm_idx);

      if(server_idx > -1)
      {
        end_mon(server_idx, END_MONITOR , -1, MON_DELETE_AFTER_SERVER_INACTIVE,tier_norm_idx); 
        if(ret > -1)
          apply_monitors_at_node_discovery(server_idx, ret);
      }
      return 1;
    }
  }
  else if (operation == CMON_CONN_RETRY)
  { 
    //tier_norm_idx=topolib_get_tier_id_from_tier_name(TierName, topo_idx);
    if(mon_id >= 0 && mon_id <= max_mon_id_entries)
    {
      index=mon_id_map_table[mon_id].mon_index;
      if( index >=0 && index <= total_monitor_list_entries)
      {
        tier_norm_idx= topolib_get_tier_id_from_tier_name(monitor_list_ptr[index].cm_info_mon_conn_ptr->tier_name, topo_idx);
        server_idx= topolib_get_topo_server_idx_from_server_id(monitor_list_ptr[index].cm_info_mon_conn_ptr->server_name,topo_idx,tier_norm_idx);
        if(Retry_value == 0 )
        {
           MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                          "Going to send Message to NDC for stop monitor with retry value = %d , mon_id = %d", Retry_value , mon_id);
         //server_info[cm_info_mon_conn_ptr->server_index]
           end_mon(server_idx, CMON_CONN_RETRY, index, MON_DELETE_AFTER_SERVER_INACTIVE,tier_norm_idx ); //TODO 
          
        }
        else if( Retry_value == 1)
        {
          if(monitor_list_ptr[index].cm_info_mon_conn_ptr->tier_name)
          {
            snprintf(SendMsg, 512, "nd_data_req:action=retry;mon_id=%d;server=%s%c%s;\n", mon_id, monitor_list_ptr[index].cm_info_mon_conn_ptr->tier_name, global_settings->hierarchical_view_vector_separator, monitor_list_ptr[index].cm_info_mon_conn_ptr->server_display_name);
          }
          else
          {
            snprintf(SendMsg, 512, "nd_data_req:action=retry;mon_id=%d;server=%s;\n", mon_id, monitor_list_ptr[index].cm_info_mon_conn_ptr->server_display_name);
          }

          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                              "Going to send Message to NDC for  data connection: %s", SendMsg);

          if(write_msg(&ndc_data_mccptr, SendMsg, strlen(SendMsg), 0, DATA_MODE) < 0)
          {
            MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                             "Error in sending msg to NDC\n");
             return FAILURE;
          }
        }
        else
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                         "Invalid mon_id received from NDC retry value = %d,mon_id= %d\n", Retry_value,mon_id);
      }
      else
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                                        "Invalid mon_index %d on mon_id %d for retry %d ,Hence continue ", index,mon_id,Retry_value);
        return FAILURE;
      }
    }
    else
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                                              "Either Invalid mon_id %d received from ndc with retry value %d or mon_id greater than max_mon_id_entries %d ,Hence continue ", mon_id, Retry_value, max_mon_id_entries);
      return FAILURE;
    }
  }
  // NODE_MODIFY comes when we change the server ip but server name remains the same
  else if(operation == NODE_MODIFY)
  {
    tier_norm_idx=topolib_get_tier_idx_from_tier_id_tf(Tier_Id,topo_idx);
    if(tier_norm_idx > -1)
    {
      server_idx = topolib_check_server_exist_in_topology_or_not(ServerHost, topo_idx,tier_norm_idx);
      if(server_idx > -1)
      {
        if(topo_info[topo_idx].topo_tier_info[tier_norm_idx].topo_server[server_idx].server_ptr->auto_monitor_applied == 1)
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                "Going to end mon for server idx [%d]", server_idx);
          end_mon(server_idx, NODE_MODIFY, -1, MON_DELETE_AFTER_SERVER_INACTIVE,tier_norm_idx);
          topo_info[topo_idx].topo_tier_info[tier_norm_idx].topo_server[server_idx].server_ptr->auto_monitor_applied = 0;
        }
        topolib_modify_topo_server_info(topo_info[topo_idx].topo_tier_info[tier_norm_idx].tier_name, server_idx, ServerIp,topo_idx);
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Successfuly modify server details on index [%d], ServerId [%d] TierId [%d]  OldServerIp [%s] ServerIp [%s]", server_idx, Server_Id, Tier_Id, OldServerIp, ServerIp);
        if(agentType > 1)
          //ret = topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->tierinfo_idx; 
          ret = tier_norm_idx;
    }  
    else
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Failed to modify server details on index [%d], ServerId [%d] TierId [%d]  OldServerIp [%s] ServerIp [%s]", server_idx, Server_Id, Tier_Id, OldServerIp, ServerIp);
    }
  }

  //INSTANCE_DOWN comes if CMON instance is down since threshold(Default 60 Seconds) it can be configurable also
  //this operation is performed only for monitors whose configured by any_server in specific server tag
  else if(operation == CMON_INSTANCE_DOWN)
  {
    tier_norm_idx=topolib_get_tier_idx_from_tier_id_tf(Tier_Id,topo_idx);
    if(tier_norm_idx > -1)
    {
      server_idx = topolib_get_topo_server_idx_from_server_id(ServerHost,topo_idx,tier_norm_idx);
      if(server_idx > -1)
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                "Auto monitor on server [%s] which is inactive now as we are receiving ndc_instance_down msg, Therefore going to stop mon on the server and apply monitor on other active server those are configured any server keyword.", topo_info[topo_idx].topo_tier_info[tier_norm_idx].topo_server[server_idx].server_disp_name);
        end_mon(server_idx, CMON_INSTANCE_DOWN , -1 , MON_DELETE_AFTER_SERVER_INACTIVE,tier_norm_idx);
        //here we are setting cmon_control_state this will to identify the active server for any-server case which is used
        //next_active_server func i.e to find out the server idx of next available server
        topo_info[topo_idx].topo_tier_info[tier_norm_idx].topo_server[server_idx].server_ptr->cmon_control_conn_down = 1;  //1 means connection is break
      }
    }
    else
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,"Tier_norm_idx not found is %d for %s",tier_norm_idx,ServerHost);
 
    }
  }
    
  //INSTANCE_UP comes only when we get INSTANCE_DOWN for the same server then INSTANCE_UP is comes in n intervals of time.
  //this operation is performed only for monitors whose configured by any_server in specific server tag
  else if(operation == CMON_INSTANCE_UP)
  {
    tier_norm_idx=topolib_get_tier_idx_from_tier_id_tf(Tier_Id,topo_idx);
    if(tier_norm_idx > -1)
    {
      server_idx = topolib_get_topo_server_idx_from_server_id(ServerHost,topo_idx,tier_norm_idx);
      //apply monitor
      if(server_idx > -1)
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Auto monitor has been added on : server ip = %s, server name = %s, tier id = %d, server id = %d whose instance is up again",ServerIp, ServerHost, Tier_Id, Server_Id);
        //here we are setting cmon_control_state this will to identify the active server for any-server case which is used
        //next_active_server func i.e to find out the server idx of next available server
        topo_info[topo_idx].topo_tier_info[tier_norm_idx].topo_server[server_idx].server_ptr->cmon_control_conn_down = 0;
        if((global_settings->json_mode == 1) && ((global_settings->auto_json_monitors_filepath) && (global_settings->auto_json_monitors_filepath[0] != '\0')))
        {
          add_json_monitors(NULL, server_idx, tier_norm_idx, NULL, 1, NDC_ANY_SERVER_CHECK, 0);
        }
      }
      else
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Server_idx not found for : server ip = %s, server name = %s, tier id = %d, server id = %d whose instance is up again",ServerIp, ServerHost, Tier_Id, Server_Id);
      }
    }
    else
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Tier_idx not found for : server ip = %s, server name = %s, tier id = %d, server id = %d whose instance is up again",ServerIp, ServerHost, Tier_Id, Server_Id);
    }
  }
  // END_MONITOR comes when we change the server name but server remains the same 
  else if(operation == END_MONITOR)
  { 
    tier_norm_idx=topolib_get_tier_idx_from_tier_id_tf(Tier_Id,topo_idx);
    if(tier_norm_idx > -1)
    {
      server_idx = topolib_get_topo_server_idx_from_server_id(ServerHost,topo_idx,tier_norm_idx);
      if(server_idx > -1)
      { 
         //Removing below conditional block so that we can have both active & inactive servers index in server_indx arr member of tier structure.
         //if(set_server_indx(Server_Id, Tier_Id) == 0)
        end_mon(server_idx, END_MONITOR , -1, MON_DELETE_AFTER_SERVER_INACTIVE,tier_norm_idx);
        topo_info[topo_idx].topo_tier_info[tier_norm_idx].topo_server[server_idx].server_ptr->auto_monitor_applied = 0;
        topo_info[topo_idx].topo_tier_info[tier_norm_idx].topo_server[server_idx].server_ptr->auto_scale = 0;
      }
    }
   
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Successfuly delete all mon on serverId [%d] and tierId [%d]", Server_Id, Tier_Id);
  }
  else if(operation == NODE_INACTIVE)
  {
    tier_norm_idx=topolib_get_tier_idx_from_tier_id_tf(Tier_Id,topo_idx);
    if(tier_norm_idx>=0)
    { 
      server_idx = topolib_get_topo_server_idx_from_server_id(ServerHost,topo_idx,tier_norm_idx);
      if(server_idx < 0)
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error in making node inactive, server entry not found"); 
      }
      else
      {
        //MS->Removing below conditional block so that we can have both active & inactive servers index in server_indx arr member of tier structure. 
        //if(set_server_indx(Server_Id, Tier_Id) == 0)
   
        if(topo_info[topo_idx].topo_tier_info[tier_norm_idx].topo_server[server_idx].server_ptr->auto_monitor_applied == 1)
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Auto monitor on server [%s] which is inactive is still runing, therefore going to stop mon on the server", topo_info[topo_idx].topo_tier_info[tier_norm_idx].topo_server[server_idx].server_disp_name);
          end_mon(server_idx, NODE_INACTIVE , -1, MON_DELETE_AFTER_SERVER_INACTIVE,tier_norm_idx);
          topo_info[topo_idx].topo_tier_info[tier_norm_idx].topo_server[server_idx].server_ptr->auto_monitor_applied = 0;
        }
        if( topo_info[topo_idx].topo_tier_info[tier_norm_idx].topo_server[server_idx].server_ptr->status == 1 ) 
        {
          topo_info[topo_idx].topo_tier_info[tier_norm_idx].topo_server[server_idx].server_ptr->status = 0;
          topo_info[topo_idx].topo_tier_info[tier_norm_idx].del_server++; 
        }
        update_health_monitor_sample_data(&hm_data->num_inactive_servers);
        //reset_instance_of_nd_mon(Inst_Id);
      }
    
    }
  }
  else if(operation == INSTANCE_INACTIVE)
  {
    //todo malloc with size of InstName + 2
    char tmp_buff[256];
    sprintf(tmp_buff, ">%s", InstName);
    //delete vector
    //delete_vector_of_instance(Inst_Id, tmp_buff);
    delete_vector_matched_inst_id(Inst_Id, 1);
    update_health_monitor_sample_data(&hm_data->num_inactive_instances);
  }
  else if(operation == INSTANCE_DELETE) 
  { 
    tier_norm_idx=topolib_get_tier_idx_from_tier_id_tf(Tier_Id,topo_idx);
    if(tier_norm_idx >=0)
    {
      if(topolib_delete_server_instance(topo_info[topo_idx].topo_tier_info[tier_norm_idx].tier_name,ServerHost,Inst_Id,0,topo_idx)==-1)   // 0 for ndc_instance_delete , 1 for ndc_server_delete 
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Match Not found for Tier id = %d, Instance id = %d",Tier_Id,Inst_Id);
      }
      else
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                "Successfully deleted Instance with Tier id = %d, Instance id = %d",Tier_Id,Inst_Id);
        total_deleted_instances += 1;
        update_health_monitor_sample_data(&hm_data->num_inactive_instances);
      }
    
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Tier_norm_id not found for Tier id = %d, Instance id = %d",Tier_Id,Inst_Id);
   }
  }
  else if(operation == SERVER_DELETE)
  {
    tier_norm_idx=topolib_get_tier_idx_from_tier_id_tf(Tier_Id,topo_idx);
    if(tier_norm_idx>=0)
    {
      int server_index=topolib_check_server_exist_in_topology_or_not(ServerHost,topo_idx,tier_norm_idx);
      if(server_index >=0) 
      {
        if(!is_outbound_connection_enabled && 
           topo_info[topo_idx].topo_tier_info[tier_norm_idx].topo_server[server_index].server_ptr->topo_servers_list->control_fd>0)
        {
          do_remove_control_fd(tier_norm_idx,server_index);
        } 
        if(topolib_delete_server_instance(topo_info[topo_idx].topo_tier_info[tier_norm_idx].tier_name,ServerHost,Server_Id,1,topo_idx)==-1) // 0 for ndc_instance_delete , 1 for ndc_server_delete 
        {
            MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Match Not found for Tier id = %d, Server id = %d",Tier_Id,Server_Id);
        }
        else
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Successfully deleted Server with Tier id = %d, Server id = %d",Tier_Id,Server_Id);
          total_deleted_servers += 1;
          update_health_monitor_sample_data(&hm_data->num_inactive_servers);
        }
      }
    }
    else
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Tier_norm_idx not found with Tier id = %d, Server id = %d",Tier_Id,Server_Id);
    }
  }

     
   
  if(ret > -1)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                                        "Auto monitor has been added on : server ip = %s, server name = %s, tier id = %d, server id = %d",
                                         ServerIp, ServerHost, Tier_Id, Server_Id);
    
    server_idx = topolib_get_topo_server_idx_from_server_id(ServerHost,topo_idx,ret);
    if(server_idx >= 0)
    {
      if(operation == NODE_MODIFY)
         reset_monitor_config_in_server_list(ret, server_idx); 

      apply_monitors_at_node_discovery(server_idx, ret);
    } 
    return 1;
  }
}
//Function to remove and add control fd
void do_remove_add_control_fd()
{
      
  for(int i=0;i<topo_info[topo_idx].total_tier_count;i++)
  {
    for(int j=0;j<topo_info[topo_idx].topo_tier_info[i].total_server;j++)
    {
      if(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->control_fd>0)
      {
      NSDL2_MON(NULL, NULL, "con state of fd [%d] is %d",topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->control_fd ,topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->con_state);
      MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
            "con state of fd [%d] is %d",topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->control_fd,
               topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->con_state);
      
      if((topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->con_state == HEART_BEAT_CON_CONNECTING) || 
                   topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->control_fd == HEART_BEAT_CON_SENDING)
      {
        NSDL2_MON(NULL, NULL, "Remove select fd  = [%d]", topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->control_fd);
        MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
      "Remove select fd  = [%d]", topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->control_fd);
        //remove select to remove fd with old address of servers_list
        REMOVE_SELECT_MSG_COM_CON(topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->control_fd, CONTROL_MODE);

        NSDL2_MON(NULL, NULL, "Add select fd  = [%d]", topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->control_fd);
        //add select with new address of servers_list
        ADD_SELECT_MSG_COM_CON((char *)(&topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list), topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->control_fd, EPOLLOUT | EPOLLERR | EPOLLHUP, CONTROL_MODE);

        NSDL2_MON(NULL, NULL, "Done remove select & add select of fd  = [%d]", topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->topo_servers_list->control_fd);
      }
      }
    }
  }
}

//need to be reviewed by kushal sir
void do_add_control_fd(int tier_idx,int server_idx)
{

   NSDL2_MON(NULL, NULL, "Add select fd  = [%d]", topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->topo_servers_list->control_fd);

  //add select with new address of servers_list
  ADD_SELECT_MSG_COM_CON((char *)(&topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->topo_servers_list), topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->topo_servers_list->control_fd, EPOLLOUT | EPOLLERR | EPOLLHUP, CONTROL_MODE);

  NSDL2_MON(NULL, NULL, "Done remove select & add select of fd  = [%d]", topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->topo_servers_list->control_fd);
}
void do_add_control_fd_wrapper(TopoServerInfo *server_ptr)
{

   NSDL2_MON(NULL, NULL, "Add select fd  = [%d]", server_ptr->topo_servers_list->control_fd);

  //add select with new address of servers_list
  ADD_SELECT_MSG_COM_CON((char *)(&server_ptr->topo_servers_list), server_ptr->topo_servers_list->control_fd, EPOLLOUT | EPOLLERR | EPOLLHUP, CONTROL_MODE);

  NSDL2_MON(NULL, NULL, "Done remove select & add select of fd  = [%d]", server_ptr->topo_servers_list->control_fd);
}







void do_remove_control_fd(int tier_idx,int server_idx)
{
   NSDL2_MON(NULL, NULL, "Remove select fd  = [%d]", topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->topo_servers_list->control_fd);
   MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
    "Remove select fd  = [%d]", topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->topo_servers_list->control_fd);
    //remove select to remove fd with old address of servers_list
  REMOVE_SELECT_MSG_COM_CON(topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->topo_servers_list->control_fd, CONTROL_MODE);
 

}



//vector separator is also added with vector name so that it can be compared when deleting. Comparison is done from reverse order using length with vector name of percentile monitors.
//Ex. 9thPercentile will be matched with 9thPercentile and 99thPercentile as well.
//But >9thPercentile will always match with >9thPercentile.
PercentileList *allocate_node_and_members(char *percentile_num, char *percentile_name)
{
  PercentileList *list_ptr = NULL;

  MY_MALLOC_AND_MEMSET(list_ptr, sizeof(PercentileList), "ND percentile node allocation", -1);
  list_ptr->len = strlen(percentile_name) + 1;          // +1 is for vector separator
  MY_MALLOC_AND_MEMSET(list_ptr->name, (list_ptr->len) + 1, "Percentile Name", -1);
  sprintf(list_ptr->name, "%c%s", global_settings->hierarchical_view_vector_separator, percentile_name);
  list_ptr->state = ACTIVE;
  list_ptr->value = atoi(percentile_num);

  return list_ptr;
}

//Need to set INACTIVE so that fresh list will be updated with ACTIVE state.
void mark_percentile_linked_list_inactive()
{
  PercentileList *list_ptr = percentile_list_head_ptr; 

  while(list_ptr != NULL)
  {
    list_ptr->state = INACTIVE;
    list_ptr = list_ptr->next;
  }
  
  NSDL2_MON(NULL, NULL, "Percentile list has been marked INACTIVE");
  return;
}

//It is same as allocate_node_and_members().
TierList *tier_allocate_node_and_members(char *tier_id, char *tier_name, char tier_type)
{
  TierList *list_ptr = NULL;

  MY_MALLOC_AND_MEMSET(list_ptr, sizeof(TierList), "ND tier node allocation", -1);
  list_ptr->tier_len = strlen(tier_name) + 1;          // +1 is for vector separator
  MY_MALLOC_AND_MEMSET(list_ptr->tier_name, (list_ptr->tier_len) + 1, "Tier Name", -1);
  sprintf(list_ptr->tier_name, "%s%c", tier_name, global_settings->hierarchical_view_vector_separator);
  list_ptr->tier_id = atoi(tier_id);
  list_ptr->tier_type = tier_type;
  list_ptr->state = ACTIVE;

  return list_ptr;
}


void mark_tier_linked_list_inactive(int type)
{
  TierList *list_ptr = tier_list_head_ptr;

  while(list_ptr != NULL)
  {
    if (type == list_ptr->tier_type)
    list_ptr->state = INACTIVE;
    list_ptr = list_ptr->next;
  }

  return;
}


void fill_tier_node_members(char *line, int type)
{
  char *fields[512];
  char tmp_buff[MAX_DATA_LINE_LENGTH]={0};
  int num_fields;
  int i=0, t_id;
  char *ptr = NULL;
  TierList *list_ptr = tier_list_head_ptr;
  TierList *new_node;

  NSDL2_MON(NULL, NULL, "Method Called with line: %s", line);
  //Need to mark every node as INACTIVE, because we have received new percentile list.
  mark_tier_linked_list_inactive(type);

  //Tokenising newly received percentile buffer
  //strcpy(tmp_buff, line);
  snprintf(tmp_buff, MAX_DATA_LINE_LENGTH, "%s", line);
  num_fields = get_tokens_with_multi_delimiter(tmp_buff, fields, ",", 512);


  for(i = 0; i < num_fields; i++)
  {
    ptr = strchr(fields[i], ':');
    if(!ptr)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Received invalid tier entry. %s", fields[i]);
      continue;
    }

    *ptr = '\0';
    t_id = atoi(fields[i]);

    list_ptr = tier_list_head_ptr;
    //If tier list is empty, we only need to add.
    //else we need to compare and mark ACTIVE accordingly.
    while(list_ptr != NULL)     //Will be NULL for first time entry
    {
      //We will only compare with the same type tier.
      if((list_ptr->tier_type == type) && (list_ptr->tier_id == t_id))
      {
        list_ptr->state = ACTIVE;
        break;
      }
      list_ptr = list_ptr->next;
    }

    if(list_ptr == NULL)
    {
      NSDL2_MON(NULL, NULL, "Going to add entry (%s) in linked list with ACTIVE state", (ptr+1));
      new_node = tier_allocate_node_and_members(fields[i], ptr+1, type);

      //insertion in linked list at head
      new_node->next = tier_list_head_ptr;
      tier_list_head_ptr = new_node;
    }
  }
}


void fill_percentile_node_members(char *line)
{
  char *fields[128];
  char tmp_buff[MAX_DATA_LINE_LENGTH]={0};
  int num_fields;
  int i=0, p_value;
  char *ptr = NULL;
  PercentileList *list_ptr = percentile_list_head_ptr;
  PercentileList *new_node;
   
  NSDL2_MON(NULL, NULL, "Method Called with line: %s", line);
  //Need to mark every node as INACTIVE, because we have received new percentile list.
  mark_percentile_linked_list_inactive();

  //Tokenising newly received percentile buffer
  strcpy(tmp_buff, line);
  num_fields = get_tokens_with_multi_delimiter(tmp_buff, fields, ",", 128);

 
  for(i = 0; i < num_fields; i++)
  {
    ptr = strchr(fields[i], ':');
    if(!ptr)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Received invalid percentile entry. %s", fields[i]);
      continue;
    }
    
    *ptr = '\0';
    p_value = atoi(fields[i]);

    list_ptr = percentile_list_head_ptr;

    //If percentile list is empty, we only need to add.
    //else we need to compare and mark ACTIVE accordingly.
    while(list_ptr != NULL)     //Will be NULL for first time entry
    {
      if(list_ptr->value == p_value)
      {
        list_ptr->state = ACTIVE;
        break;
      }

      list_ptr = list_ptr->next;
    }

    if(list_ptr == NULL)
    {
      NSDL2_MON(NULL, NULL, "Going to add entry (%s) in linked list with ACTIVE state", (ptr+1));
      new_node = allocate_node_and_members(fields[i], ptr+1);
 
      //insertion in linked list
      new_node->next = percentile_list_head_ptr;
      percentile_list_head_ptr = new_node;     
    }
  }
}


//(1) READ AND PARSE OUTPUT , (2) handle partial data , (3) search each obtained tier/server/instance in structure (4) extract and save pid in 'topo_instance_info' structure
int parse_and_fill_inst_pid_in_topo(char *read_msg, int do_remove)
{
  int bytes_read = 0, reset_ndc_state = 0, len = 0, inst_id = -1, break_loop = 0, ndc_node_msg_row = 0;
  char *lft_bytes_ptr = NULL, *new_line_ptr = NULL, *line = NULL, *ptr = NULL, *err_ptr = NULL, *inst_ptr = NULL, *colon_ptr = NULL;
  char *nlp = NULL;
 
  while(1)
  {
    bytes_read = 0;
    break_loop = 0;

    //if((bytes_read = read(ndc_mccptr.fd, ndc_read_msg + ndc_left_bytes_from_prev, 1023 - ndc_left_bytes_from_prev)) < 0) 
    if((bytes_read = read(ndc_mccptr.fd, ndc_read_msg + ndc_left_bytes_from_prev, NDC_READ_BUF_SIZE - ndc_left_bytes_from_prev)) < 0) 
    {
      if (errno == EAGAIN)
      {
        if(do_remove == 0)
          continue;
        else
          return 0;
      }
      else if (errno == EINTR)
      {   /* this means we were interrupted */
        continue;
      }
      else
      {
        if(read_msg) 
          sprintf(read_msg, "NETDIAGNOSTICS: Error in receiving response from ND Collector. Connection got disconnected from ND Collector.\n");

        return -1;
      }
    }

    //if current bytes is 0 and buffer is full then do processing
    if((bytes_read == 0) && (ndc_left_bytes_from_prev != NDC_READ_BUF_SIZE))
    {
        NSDL1_MON(NULL, NULL, "ND Connection is closed by ND Collector, fd = %d", ndc_mccptr.fd);
        NS_EL_1_ATTR(EID_NDCOLLECTOR, -1, -1, EVENT_NDCOLLECTOR, 6, "NDCollector",
                     "ND Connection is closed by NDCollector, fd =%d", ndc_mccptr.fd);
        if(do_remove){
          REMOVE_SELECT_MSG_COM_CON(ndc_mccptr.fd, DATA_MODE);
        }
        close(ndc_mccptr.fd);
        ndc_mccptr.fd = -1;
        ndc_left_bytes_from_prev = 0;
      if(read_msg) 
      {
        sprintf(read_msg, "NETDIAGNOSTICS: Error in receiving response from ND Collector. "
                         "Connection got disconnected from ND Collector.\n");
      }
      return -1;
    }
   
    ndc_left_bytes_from_prev += bytes_read;
    ndc_read_msg[ndc_left_bytes_from_prev] = '\0';

    MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Recieved message from NDC. Message = %s, ndc_left_bytes_from_prev = %d , bytes_read = %d ", ndc_read_msg, ndc_left_bytes_from_prev, bytes_read); 
   
    NSDL2_MON(NULL, NULL, "Recieved message from NDC. Message = %s", ndc_read_msg);
    
    ptr = ndc_read_msg;

    if(do_remove == 0)
    {
      nlp = strstr(ptr, "\n");      
      if(nlp)
        break_loop = 1; 
    }

//nd_control_rep:action=start_net_diagnostics;result=Warning;40.9_tier:40.9_server:40.9_instance=52558;40.10_tier:40.10_server:40.10_instance=16688;40.6_tier:40.6_server:40.6_instance;Message=Warning: Fail in making control connection;\nEvent:1.0:Critical|Control connection missing from AppInstance (tierName = 40.6_tier, serverName = 40.6_server, appName = 40.6_instance). As per configured preference, continuing the test, anticipating that control connection with this instance will be recovered later\n


    //while(((new_line_ptr = strstr(ptr, "\n")) || (ndc_left_bytes_from_prev == 4095)) && ((!strncmp(ptr, "nd_control_info:action=pid", 26)) || (!strncmp(ptr, "nd_control_rep:action=start_net_diagnostics", 43)) || ((err_ptr = strstr(ptr, "Error"))) || (!strncmp(ptr, "nd_control_rep:action=stop_net_diagnostics", 42)) || (ndc_state == 1) || (!strncmp(ptr, "nd_control_req:action=instance_shutdown", 39)) || (!strncmp(ptr, "ndc_node_discovery_ctrl_msg:", 28)) || (!strncmp(ptr, "nd_control_rep:action=start_ndc_connection", 42)) ))
    while((new_line_ptr = strstr(ptr, "\n")) || (ndc_left_bytes_from_prev == NDC_READ_BUF_SIZE))
    {
      //reset
      reset_ndc_state = 0;
      lft_bytes_ptr = NULL;  
      len = 0;
      
      if(new_line_ptr)
      {
        reset_ndc_state = 1;
        *new_line_ptr = '\0'; 
      }

 
      //if(stop)
      if (!strncmp(ptr, "nd_control_rep:action=stop_net_diagnostics", 42))
      {
        if(read_msg) 
          sprintf(read_msg, "%s", ptr);

        ndc_left_bytes_from_prev = 0;
        return -1;
      }

      err_ptr = strstr(ptr, "result=Error");

      if(err_ptr) //found Error
      {
        NSTL1_OUT(NULL, NULL, "NETDIAGNOSTICS ERROR: %s\n", err_ptr);
        NS_EXIT(-1,CAV_ERR_1060080,err_ptr);
        close(ndc_mccptr.fd);
        ndc_mccptr.fd = -1;
        ndc_left_bytes_from_prev = 0;
        ndc_read_msg[0] = '\0';
        
        //if error is received on making the connection for the first time then exit and stop the session
        if (ndc_connection_state != NDC_START_CONNECTION_INIT)
        {
          nde_skip_bad_partition();
          NS_EXIT(-1, "Failed to send message to ndcollector, see logs for errors");   
        }

        return (-1); //error
      }

      line = ptr;
      len = strlen(line);

      if(!strncmp(line, "nd_control_info:action=pid", 26) || !strncmp(line, "nd_control_rep:action=start_net_diagnostics;result=", 51) || (ndc_state == 1))
      {
        if(!strncmp(line, "nd_control_rep:action=start_net_diagnostics;result=", 51))
        {
          ndc_mccptr.state |= NS_CONNECTED;
          //setting state as connected once the response is received from NDC. 
          ndc_connection_state = NDC_START_CONNECTION_INIT;
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Response message from NDC is receied. Gonig to set ndc_mccptr.state = %d", ndc_mccptr.state);
        }

        ndc_state = 1;
        lft_bytes_ptr = parse_and_extract(line, reset_ndc_state);

        //once processed complete line then reset ndc_state  
        if(reset_ndc_state == 1)
          ndc_state = 0;
      }
      else if(!strncmp(line, "nd_control_req:action=instance_shutdown", 39))
      { 
        // nd_control_req:action=instance_shutdown;tierid=<id>;tier=<displayname>;serverid=<id>;server=<displayname>;instanceid=<id>;instance=<displayname>;

        //parse and extrace instance id
        inst_ptr = strstr(line, "instanceid=");
        if(inst_ptr)
          inst_ptr += 11;
  
        if(inst_ptr)
        {
          colon_ptr = strchr(inst_ptr, ';');
          if(colon_ptr)
            *colon_ptr = '\0';

          inst_id = atoi(inst_ptr); 
          
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "\n inst_id = [ %d] \n", inst_id); 
          //search this id in CM_info table, if found mark vector for deletion
          delete_vector_matched_inst_id(inst_id, 1);     
        }
        ndc_left_bytes_from_prev -= len;
      }
      else if(!strncmp(ptr, "ndc_node_discovery_ctrl_msg:", 28) || !strncmp(ptr, "ndc_node_end_mon:", 17) || !strncmp(ptr, "ndc_node_modify:", 16) || !strncmp(ptr, "ndc_server_inactive:", 20) || !strncmp(ptr, "ndc_instance_inactive:", 22) || !strncmp(ptr, "ndc_instance_delete:", 20) || !strncmp(ptr, "ndc_server_delete:", 18) || !strncmp(ptr, "ndc_instance_down:", 18) || !strncmp(ptr, "ndc_instance_up:", 16))
      {
        create_table_entry_dvm(&ndc_node_msg_row, &total_ndc_node_msg, &max_ndc_node_msg, (char **)&ndc_node_msg, sizeof(char*), "Node Discovery Message");
        MY_MALLOC(ndc_node_msg[ndc_node_msg_row], strlen(ptr) + 1, "NDC Node Msg", ndc_node_msg_row);
        strcpy(ndc_node_msg[ndc_node_msg_row], ptr);
        // decrease length
        ndc_left_bytes_from_prev -= len;
      }
      /*else if(!strncmp(ptr, "ndc_node_discovery_ctrl_msg:", 28))
      {
        ptr = ptr + 28;
        char *fields[30]; // assuming values will not exceed 30. current max value is ..
        int ret;
        NSTL2(NULL, NULL, "ndc_node_discovery_ctrl_msg is receive with buff = %s",ptr); 
        //tokenize ptr
        ret = get_tokens_with_multi_delimiter(ptr, fields, ";", 30);
        
        parse_and_extract_server_info(fields, ret, NODE_DISCOVERY);
        // decrease length
        ndc_left_bytes_from_prev -= len;
      }
      else if(!strncmp(ptr, "ndc_node_end_mon:", 17))
      {
        ptr = ptr + 17;
        char *fields[30];
        int ret;
        NSTL1(NULL, NULL, "ndc_monitor_end_msg is receive with buff = %s",ptr);
        //tokenize ptr
        ret = get_tokens_with_multi_delimiter(ptr, fields, ";", 30);

        parse_and_extract_server_info(fields, ret, END_MONITOR);
        // decrease length
        ndc_left_bytes_from_prev -= len;
      }
      else if(!strncmp(ptr, "ndc_node_modify:", 16))
      {
        ptr = ptr + 16;
        char *fields[30];
        int ret;
        NSTL1(NULL, NULL, "ndc_node_modify_msg is receive with buff = %s",ptr);
        //tokenize ptr
        ret = get_tokens_with_multi_delimiter(ptr, fields, ";", 30);

        parse_and_extract_server_info(fields, ret, NODE_MODIFY);
        // decrease length
        ndc_left_bytes_from_prev -= len;
      }*/
      else if(!strncmp(ptr, "nd_control_rep:action=start_ndc_connection", 42))
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "ndc_control_rep is receive with buff = %s",ptr);
        // decrease length
        ndc_left_bytes_from_prev -= len;
      }
      //Ex: nd_control_info:percentile_list=80:80thPercentile,85:85thPercentile,90:90thPercentile;\nnd_control_info:bt_tier_list=<tierid>:<tiername>,<tierid>:<tiername>;\nnd_control_info:ip_tier_list=<tierid>:<tiername>,<tierid>:<tiername>;\n
      else if(!strncmp(ptr, "nd_control_info:percentile_list=", 32))
      { 
        MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "nd_control_info:percentile_list is receive with buff = %s", ptr);
        char *tmp_ptr = strchr(ptr, ';');
        if(tmp_ptr)
        {
          *tmp_ptr = '\0';
          
          fill_percentile_node_members(ptr+32);
          check_and_delete_nd_percentile_vectors();
        }
        ndc_left_bytes_from_prev -= len;
      }
      else if(!strncmp(ptr, "nd_control_info:bt_tier_list=", 29))
      {
        char *tmp_ptr = strchr(ptr, ';');
        if(tmp_ptr)
        { 
          NSTL1(NULL, NULL, "nd_control_info:bt_tier_list= is receive with buff = %s", ptr);
          *tmp_ptr = '\0';
          fill_tier_node_members(ptr + 29, ND_BT_TIER_LIST);
          check_and_delete_nd_percentile_vectors_using_tiers(ND_BT_TIER_LIST);
        }
        ndc_left_bytes_from_prev -= len;
      }
      else if(!strncmp(ptr, "nd_control_info:ip_tier_list=", 29))
      {
        NSTL1(NULL, NULL, "nd_control_info:ip_tier_list= is receive with buff = %s", ptr);
        char *tmp_ptr = strchr(ptr, ';');
        if(tmp_ptr)
        {
          *tmp_ptr = '\0';
          fill_tier_node_members(ptr + 29, ND_IP_TIER_LIST);
          check_and_delete_nd_percentile_vectors_using_tiers(ND_IP_TIER_LIST);
        }
        ndc_left_bytes_from_prev -= len;
      }
      /*else if(!strncmp(ptr, "ndc_server_inactive:", 20))
      {
        ptr = ptr + 20;
        char *fields[30];
        int ret;
        NSTL1(NULL, NULL, "ndc_server_inactive is receive with buff = %s",ptr);
        //tokenize ptr
        ret = get_tokens_with_multi_delimiter(ptr, fields, ";", 30);

        parse_and_extract_server_info(fields, ret, NODE_INACTIVE);
        // decrease length
        ndc_left_bytes_from_prev -= len;
      }
      else if(!strncmp(ptr, "ndc_instance_inactive:", 22))
      {
        ptr = ptr + 22;
        char *fields[30];
        int ret;
        NSTL1(NULL, NULL, "ndc_instance_inactive is receive with buff = %s",ptr);
        //tokenize ptr
        ret = get_tokens_with_multi_delimiter(ptr, fields, ";", 30);

        parse_and_extract_server_info(fields, ret, INSTANCE_INACTIVE);
        // decrease length
        ndc_left_bytes_from_prev -= len;
      }*/
      else
      {
        ndc_state = 0;

        int severity;
        char *event = extract_header_from_event(line, &severity);
   
        if(event)
          NS_EL_1_ATTR(EID_NDCOLLECTOR, -1, -1, EVENT_NDCOLLECTOR, severity, "NDCollector","%s", event);

        ndc_left_bytes_from_prev -= len;
      }

      if(new_line_ptr)
        ndc_left_bytes_from_prev--; //for newline
      
      if(!new_line_ptr)
        break;
      ptr = new_line_ptr + 1;
      if(!ptr)
        break;
    }
    
    //If new server has been added.Do remoe and add all control fd
    /*if(g_server_struct_realloc_flag == 1)
    {
      do_remove_add_control_fd();
      g_server_struct_realloc_flag = 0;
    }*/

    //copy left bytes of 'nd_control_info'message only not others
    if(ndc_left_bytes_from_prev)
    {
      //we expect atleast one new line in 8k buffer.If it is not so we will return error.
      if(ndc_left_bytes_from_prev == NDC_READ_BUF_SIZE)
      {
        NSDL1_MON(NULL, NULL, "Receive partial line with length more than 8k");
        //NSEL_MAJ 
        NS_EL_1_ATTR(EID_NDCOLLECTOR, -1, -1, EVENT_NDCOLLECTOR, 6, "NDCollector",
                     "Receive partial line with length more than 8k from NDCOLLECTOR");
        if(do_remove){
          REMOVE_SELECT_MSG_COM_CON(ndc_mccptr.fd, DATA_MODE);
        }   
        close(ndc_mccptr.fd);
        ndc_mccptr.fd = -1;
        ndc_left_bytes_from_prev = 0;
        if(read_msg)
        { 
          sprintf(read_msg, "NETDIAGNOSTICS: Receiving error response from ND Collector."
                         "Receive partial line with length more than 8k from NDCOLLECTOR\n");
        }
        return -1;
      }
      if(lft_bytes_ptr != NULL)
      {
        bcopy(lft_bytes_ptr, ndc_read_msg, ndc_left_bytes_from_prev + 1); 
        lft_bytes_ptr = NULL;
      }
      else
      {
        //copy in partial
        strncpy(partial_buf, ptr, ndc_left_bytes_from_prev + 1);
        //copy back from partial buff
        strcpy(ndc_read_msg, partial_buf);
        partial_buf[0] = '\0';
      }
    }
    else
    {
      ndc_read_msg[0] = '\0';
      //ndc_left_bytes_from_prev = 0;
    }

    if(break_loop == 1)
      break;
  }


  //copy left bytes of 'nd_control_info'message only not others
  //if((ndc_left_bytes_from_prev) && (lft_bytes_ptr != NULL))
    //bcopy(lft_bytes_ptr, ndc_read_msg, ndc_left_bytes_from_prev + 1);

  return 0;
}


// Called on start and it works in blocking mode. It exits in case of error 
static int nd_send_msg_to_ndc_on_start()
{
  char SendMsg[4096]="\0";
  //static char read_msg_buffer[NDC_READ_BUF_SIZE + 1] = {0};
  //int ret = 0;

  NSDL1_MON(NULL, NULL, "Method called");

  //Create messaage to send to NDC
  create_ndc_msg(SendMsg);

  MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Sending message to ND. Message = %s", SendMsg);
  if (send(ndc_mccptr.fd, SendMsg, strlen(SendMsg), 0) != strlen(SendMsg))
  {
    NSTL1_OUT(NULL, NULL, "nd_send_msg_to_ndc_on_start: Error in sending message to NDC." 
                    "Net Diagnostics Server IP %s, Port = %d. Error = %s\n",
                    global_settings->net_diagnostics_server, global_settings->net_diagnostics_port, nslb_strerror(errno));

    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "nd_send_msg_to_ndc_on_start: Error in sending message to NDC."
                    "Net Diagnostics Server IP %s, Port = %d. Error = %s\n",
                    global_settings->net_diagnostics_server, global_settings->net_diagnostics_port, nslb_strerror(errno));
    close(ndc_mccptr.fd);
    ndc_mccptr.fd = -1;
    return -1; //error
  }

  //data connection will only be made if control connection response is received by netstorm.
  //We will wait if test is ND or NS with outbound connection enabled.
  //Indefinite wait for response  from NDC ??
  /*if(global_settings->net_diagnostics_mode || is_outbound_connection_enabled)
  {
    while(!(ndc_mccptr.state & NS_CONNECTED))
    {
      ret = parse_and_fill_inst_pid_in_topo(read_msg_buffer, 0);
      if(ret < 0)
      {
        NSTL1_OUT(NULL, NULL, "%s\n", read_msg_buffer);
        return(ret);
      }
      else
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "ndc_mccptr.state = %d", ndc_mccptr.state);
      }
    }
  }*/
  
  //Check to store the current time 
  g_ndc_start_time = ns_get_ms_stamp();
  ndc_connection_state = NDC_WAIT_FOR_RESP;
  NSTL1(NULL, NULL, "g_ndc_start_time %f", g_ndc_start_time);


  /*{ 
    char read_msg[1024], *tmp;
    int byte_read = 0;
   
    //wait until msg is received or connection close
    while(1)
    {
      NSTL1(NULL, NULL, "Reading msg from NDC");
      if((byte_read = read(ndc_mccptr.fd, read_msg, 1024)) == 0) 
      {
        NSTL1(NULL, NULL, "NETDIAGNOSTICS: Error in receiving response from ND Collector. "
                        "Connection got disconnected from ND Collector.\n");
        NSTL1_OUT(NULL, NULL, "NETDIAGNOSTICS: Error in receiving response from ND Collector. "
                        "Connection got disconnected from ND Collector.\n");
        close(ndc_mccptr.fd);
        ndc_mccptr.fd = -1;
        return (-1); //error
      }
      if(byte_read >= 0)
      {
        break;
      }
    }
    read_msg[byte_read] = '\0';
    NSTL1(NULL, NULL, "Recieved message from NDC. Message = %s", read_msg);
    NSDL2_MON(NULL, NULL, "Recieved message from NDC. Message = %s", read_msg);
    if((tmp = strstr(read_msg, "Error")))
    {
      tmp += strlen("Error;");
      NSTL1_OUT(NULL, NULL, "NETDIAGNOSTICS ERROR: %s\n", tmp);
      NSTL1(NULL, NULL, "Got error from NDC. Closing connection. Error = %s", tmp);
      close(ndc_mccptr.fd);
      ndc_mccptr.fd = -1;
      return (-1); //error
    }
  }*/
  MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Auto scale mode is ON");
  NSDL2_MON(NULL, NULL, "Method End. Start ND OK");
  return 0;
}


//we call this function if in continuous monitoring our test get terminated in any case then we skip that bad partition.So, here we update .curPartition file when we get bad partition and update the 'nextPartition' member of .partition_info_txt file of the prev partition. We are doing all this to skip the bad partition.

void nde_skip_bad_partition()
{
  char file_buf[1024] = {0};
  char partition[15] = {0};

  static CurPartInfo temp_cur_part_info;
  static PartitionInfo  temp_part_info;

  sprintf(file_buf,"%s/logs/TR%d", getenv("NS_WDIR"), testidx);

  sprintf(partition,"%lld", g_prev_partition_idx);

  nslb_get_partition_info(file_buf, &temp_part_info, partition);
  temp_part_info.next_partition = 0;
  nslb_save_partition_info(testidx, g_prev_partition_idx, &temp_part_info, NULL);

  temp_cur_part_info.first_partition_idx = g_first_partition_idx;
  temp_cur_part_info.cur_partition_idx = g_prev_partition_idx;

  nslb_save_cur_partition_file(testidx, &temp_cur_part_info);
}


// Called on start 
void start_nd_ndc()
{
  char err_msg[1024];
  err_msg[0] = '\0';

  NSDL1_MON(NULL, NULL, "Method called global_settings->net_diagnostics_mode = %d", global_settings->net_diagnostics_mode);
  
  /* Connection with ND Collector is made only when NET_DIAGNOSTICS_SERVER keyword is on. 
   * If ND is enabled (G_ENABLED_NETDIAGNOSTICS) for any group, FP header will be included
   * in the http request, however, control connection (for sending start_net_diagnostics msg
   * to ND Collector) is not dependant on G_ENABLED_NETDIAGNOSTICS KW. 
   */

  //This is the case when Auto scale mode is off
  if((global_settings->net_diagnostics_mode == 0) && (global_settings->net_diagnostics_port == 0))
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Auto scale mode is Off");
    ndc_mccptr.fd = -1;
    return;
  }

  if(global_settings->net_diagnostics_server[0] == '\0')
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "NET_DIAGNOSTICS_SERVER is not defined but NetDiagnostic is enabled.");
    nde_skip_bad_partition();
    NS_EXIT(-1, CAV_ERR_1031048);
  }

  //if topology name is NA , then print error message and exit.
  if(!(strcmp(global_settings->hierarchical_view_topology_name,"NA")))
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Toplogy Name is not defined in HIERARCHICAL_VIEW but NetDiagnostic is enabled.\n");
    nde_skip_bad_partition();
    NS_EXIT(-1, CAV_ERR_1031049);
  }

  //malloc InstancePidTbl to save data related to instance pid received from ndc
  //if(global_settings->hierarchical_view)
  //{
    //NSLB_MALLOC_AND_MEMSET(inst_pid, sizeof(InstancePidTbl), "Malloc InstancePidTbl", -1);  
  //}

  // Make blocking connectin with timeout
  ndc_mccptr.fd = nslb_tcp_client_ex(global_settings->net_diagnostics_server, global_settings->net_diagnostics_port, 10, err_msg);
  if(ndc_mccptr.fd == -1)
  {
    if(global_settings->net_diagnostics_mode == 0)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error: Unable to connect to Net Diagnostics Server IP %s, Port = %d. Error = %s\n",
                            global_settings->net_diagnostics_server, global_settings->net_diagnostics_port, err_msg);
      return;
    }
    else{
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error: Unable to connect to Net Diagnostics Server IP %s, Port = %d. Error = %s\n", 
                            global_settings->net_diagnostics_server, global_settings->net_diagnostics_port, err_msg);

      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Please check NDC is running or not.\n");
       //On starting, connection must made if not made then exit
      nde_skip_bad_partition();
      NS_EXIT(-1, CAV_ERR_1031050, global_settings->net_diagnostics_server, global_settings->net_diagnostics_port, err_msg);
    }
  }
  
  int ret = nd_send_msg_to_ndc_on_start();

  if((ret < 0)  && (global_settings->net_diagnostics_mode)) 
  {
    nde_skip_bad_partition();  
    NS_EXIT(-1, CAV_ERR_1031051);
  }

  //(1) READ AND PARSE OUTPUT , (2) handle partial data , (3) search each obtained tier/server/instance in structure (4) extract and save pid in 'topo_instance_info' structure
  //parse_and_fill_inst_pid_in_topo(ndc_mccptr.fd);    
    
  /* Making above open socket non - blocking*/
  if(fcntl(ndc_mccptr.fd, F_SETFL, O_NONBLOCK) < 0)
  {
    sprintf(err_msg, "fcntl() socket: %d errno %d (%s)", ndc_mccptr.fd, errno, nslb_strerror(errno));
    if(global_settings->net_diagnostics_mode)
    {
      nde_skip_bad_partition();
      NS_EXIT(-1, CAV_ERR_1031052, err_msg);
    }
  }
   //we set ndc_mccptr.state to NS_CONNECTED on receiving response from ndc,So we have to save it previous state before calling init_msg_con_struct function as in this function we do memset.
  unsigned char state = ndc_mccptr.state;
  init_msg_con_struct(&ndc_mccptr, ndc_mccptr.fd, CONNECTION_TYPE_OTHER, global_settings->net_diagnostics_server, NS_NDC_TYPE);
/* Initialize ndc_mccptr */
  ndc_mccptr.state = state;

  NSDL1_MON(NULL, NULL, "Method End.");
}

//Conection is established now add select, create message and send message
static inline void ndc_con_succ()
{
  char SendMsg[4096] = "\0";

  NSDL1_MON(NULL, NULL, "Method called.");
  create_ndc_msg(SendMsg);
  MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
             "Sending Message to NDC after recovery. SendMessage : %s", SendMsg);
  //Send message to NDC 
  write_msg(&ndc_mccptr, (char *)&SendMsg, strlen(SendMsg), 0, DATA_MODE);
  NSDL1_MON(NULL, NULL, "Method End.");
}

/*****************************************************RECOVERY**********************************************/



// Do a non blocking connect to ndc
static int connect_to_ndc_nb(char *ip, unsigned short port) {
  
  int fd;
  char err_msg[2 * 1024] = "\0";
  int con_state;

  //Opening a non-blocking socket.
  if((fd = nslb_nb_open_socket((AF_INET), err_msg)) < 0)
  //If We need local connection, then we need to open a socket fo AF_UNIX family. Data transfer takes fast as it is fully dedicated to connect to local machines rather than a TCP connection which is able to make connection with all types of machines. Here NS is a client and trying to connect NDC. If we are opening a UNIX socket it means NDC must also have an open UNIX socket.
  {
    NS_EL_2_ATTR(EID_NDCOLLECTOR, -1, -1,
                                EVENT_CORE, EVENT_CRITICAL,
                                (char *)__FILE__,(char *) __FUNCTION__,
                                "Error: Error in opening socket");

    NSDL1_MON(NULL, NULL, "Error: Error in opening socket");
    return -1;
  }
  
  {
    NSDL3_MON(NULL, NULL, "Socket opened successfully so making connection for fd %d to server ip =%s, port = %d",
                           fd, ip, port);
    //Calling non-blocking connect
    int con_ret = nslb_nb_connect(fd, ip, port, &con_state, err_msg);

    NSDL3_MON(NULL, NULL, "con_ret = %d, con_state = %d", con_ret, con_state);
    /* Initialize ndc_mccptr */
    //init_msg_con_struct(&ndc_mccptr, ndc_mccptr.fd, CONNECTION_TYPE_OTHER, global_settings->net_diagnostics_server, NS_NDC_TYPE);
    init_msg_con_struct(&ndc_mccptr, fd, CONNECTION_TYPE_OTHER, global_settings->net_diagnostics_server, NS_NDC_TYPE);
    if(con_ret == 0)
    {
      //Connected Ok. 
      ADD_SELECT_MSG_COM_CON((char *)&ndc_mccptr, ndc_mccptr.fd, EPOLLIN | EPOLLERR | EPOLLHUP, DATA_MODE); 
      ndc_con_succ();
    }
    else if(con_ret > 0)
    {
      if(con_state == NSLB_CON_CONNECTED)
      {
        //Connect Ok.
        ADD_SELECT_MSG_COM_CON((char *)&ndc_mccptr, ndc_mccptr.fd, EPOLLIN | EPOLLERR | EPOLLHUP, DATA_MODE);
        ndc_con_succ();
      }
      else if(con_state == NSLB_CON_CONNECTING)
      {
        //Connecting state, need to add fd on EPOLLOUT
         ndc_mccptr.state |= NS_CONNECTING;
         MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
             "Connecting to NDC at ip address %s and port %d\n", ip, port);
         // Note - Connection event comes as EPOLLOUT
         ADD_SELECT_MSG_COM_CON((char *)&ndc_mccptr, ndc_mccptr.fd, EPOLLOUT | EPOLLIN | EPOLLERR | EPOLLHUP, DATA_MODE);
      }
      else
      {
        ADD_SELECT_MSG_COM_CON((char *)&ndc_mccptr, ndc_mccptr.fd, EPOLLOUT | EPOLLIN | EPOLLERR | EPOLLHUP, DATA_MODE);
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
             "Unknown status of connections while connecting with NDC at ip address %s and port %d\n", ip, port);
      }
    }
    else //Error case. We need to restart again
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
             "Unknown status of connections while connecting with NDC at ip address %s and port %d\n", ip, port); 
      close(fd);
      fd = -1;
      return -1;
    }
  }
  //Update nc_mccptr.fd and type
  //ndc_mccptr.fd = fd;
  //ndc_mccptr.con_type = NS_NDC_TYPE;
  return 0;
}

/* Function is used to connect non blocking connection, returns -1 on error and 0 on success
 * */
int chk_connect_to_ndc()
{
  int con_state;
  char err_msg[2 * 1024] = "\0";

  MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
             "NDC:State is  NS_CONNECTING so try to reconnect for fd %d and to server ip = %s, port = %d", ndc_mccptr.fd, global_settings->net_diagnostics_server, global_settings->net_diagnostics_port);
  nslb_nb_connect(ndc_mccptr.fd, global_settings->net_diagnostics_server, global_settings->net_diagnostics_port, &con_state, err_msg);

  if(con_state != NSLB_CON_CONNECTED)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
             "NDC:Still not connected. err_msg = %s", err_msg);
    CLOSE_MSG_COM_CON(&ndc_mccptr, DATA_MODE);
    return -1;
  }
  //Else socket is connected, add socket to EPOLL IN
  MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
             "NDC:Connected successfully with fd %d and to server ip = %s", ndc_mccptr.fd, global_settings->net_diagnostics_server);
  ndc_mccptr.state &= ~NS_CONNECTING;
  MOD_SELECT_MSG_COM_CON((char *)&ndc_mccptr, ndc_mccptr.fd, EPOLLIN | EPOLLERR | EPOLLHUP, DATA_MODE);
  return 0; //Success case
}

/***********************************************************************************
 * read_ndc_reply_msg()
 *
 * This function logs NDC specifice events. Data for multiple events can be present
 * in the buffer being read. 
 *
 * This function handlespartial reads byu saving the leftover data from previous call.
 *
 * It is assumed that:
 *
 * 1. If there are multipleevents, they will surely be separated by 
 *    newline character.
 * 2. Newline character isnot allowedin the event text.
 *
 * Any event bigger than 4094 bytes will be truncated and only first 4094 characters
 * are logged. 
 *
 ************************************************************************************/
inline char *read_ndc_reply_msg()
{
  static char read_msg[NDC_READ_BUF_SIZE + 1] = {0};
  parse_and_fill_inst_pid_in_topo(read_msg, 1);

  if(read_msg[0] != '\0')
    return read_msg;
  else
    return NULL;

}

/***********************************************************************************
 * handle_ndc()
 *
 * This function logs NDC specifice events. Data for multiple events can be present
 * in the buffer being read. 
 *
 * This function handles partial reads byu saving the leftover data from previous call.
 *
 * It is assumed that:
 *
 * 1. If there are multipleevents, they will surely be separated by 
 *    newline character.
 * 2. Newline character isnot allowedin the event text.
 *
 * Any event bigger than 4094 bytes will be truncated and only first 4094 characters
 * are logged. 
 *
 ************************************************************************************/
inline void handle_ndc(struct epoll_event *pfds, int i)
{
  //char SendMsg[2048] = "\0";
  //Msg_com_con *mccptr =  (Msg_com_con *)pfds[i].data.ptr;  

  NSDL1_MON(NULL, NULL, "Method Called");
  
  if (pfds[i].events & EPOLLOUT) 
  {
    NSDL1_MON(NULL, NULL, "Connection Received EPOLLOUT event");
    /*In case of recovery we are creating a non blocking connection therefore need to verify connection state*/
    if (ndc_mccptr.state & NS_CONNECTING)
    {
      int ret = chk_connect_to_ndc();
      NSDL1_MON(NULL, NULL, "Connection Return value of chk_connect_to_ndc, ret = %d", ret);
      if (ret == -1)
        return;
      //This point connection is established. Currently we are sending same message for recovery and partiton switching
      //so no need to check for state
      //Here we do not need to do add_select bec we have already done in case of recovery 
      ndc_con_succ();
      return;
    }

    if (ndc_mccptr.state & NS_STATE_WRITING)
      write_msg(&ndc_mccptr, NULL, 0, 0, DATA_MODE);

  } else if (pfds[i].events & EPOLLIN) {
     read_ndc_reply_msg();
  } else if (pfds[i].events & EPOLLHUP){
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
             "Connection EPOLLHUP occured on sock %s. error = %s",
                    msg_com_con_to_str(&ndc_mccptr), nslb_strerror(errno));
    NSDL1_MON(NULL, NULL, "Connection EPOLLHUP occured on sock %s. error = %s",
                    msg_com_con_to_str(&ndc_mccptr), nslb_strerror(errno));
    //close_msg_com_con_and_exit(&ndc_mccptr, (char *)__FUNCTION__, __LINE__, __FILE__);
    CLOSE_MSG_COM_CON_EXIT((&ndc_mccptr), DATA_MODE);
  } else if (pfds[i].events & EPOLLERR){
     MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
             "Connection EPOLLERR occured on sock %s. error = %s",
                msg_com_con_to_str(&ndc_mccptr), nslb_strerror(errno)); 
    NSDL3_MON(NULL, NULL, "Connection EPOLLERR occured on sock %s. error = %s",
                msg_com_con_to_str(&ndc_mccptr), nslb_strerror(errno));
     CLOSE_MSG_COM_CON_EXIT((&ndc_mccptr), DATA_MODE);
  } else {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
               "This should not happen.");
    NSDL3_MON(NULL, NULL, "This should not happen.");
     
  }
}



//Connect to NDC by NS
int ndc_recovery_connect()
{
  //Connect NS to NDC with non blocking connect
  MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Connecting to NDC at ip address %s and port %d\n", global_settings->net_diagnostics_server, global_settings->net_diagnostics_port);
  if ((connect_to_ndc_nb(global_settings->net_diagnostics_server, global_settings->net_diagnostics_port)) < 0) 
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Connecting to NDC at ip address %s and port %d\n", global_settings->net_diagnostics_server, global_settings->net_diagnostics_port);
  } 
  return 0;
}

/********************************************************STOP_NDC**********************************************************/


int stop_nd_ndc()
{
  char err_msg[1024];
  err_msg[0] = '\0';

  char buffer[4096];
  NSDL1_MON(NULL, NULL, "Method called");

  if((!global_settings->net_diagnostics_mode) && (global_settings->net_diagnostics_port == 0))
  {
    return 0;
  }
  if(global_settings->net_diagnostics_mode == 0)
    sprintf(buffer,"nd_control_req:action=stop_ndc_connection\n");
  else
    sprintf(buffer,"nd_control_req:action=stop_net_diagnostics\n");

  if(ndc_mccptr.fd){ //Create blocking connection
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Trying to send leftover message if any");
    complete_leftover_write(&ndc_mccptr, DATA_MODE); // This may close connection
  }

  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Going to send stop message. Message = [%s]", buffer);
    NSDL2_MON(NULL, NULL, "ndc_mccptr.fd = %d, message = %s", ndc_mccptr.fd, buffer);
    if(ndc_mccptr.fd == -1) //Create blocking connection
    {

      // Make new connection as blocking to avoid handling of non blocking ( We may change in future)
      ndc_mccptr.fd = nslb_tcp_client_ex(global_settings->net_diagnostics_server, global_settings->net_diagnostics_port, 10, err_msg);
      if(ndc_mccptr.fd == -1)
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Error: Unable to connect to Net Diagnostics Server IP %s, Port = %d for %s\n",
                              global_settings->net_diagnostics_server, global_settings->net_diagnostics_port, buffer);
        if(global_settings->net_diagnostics_mode)
        {
          NSTL1_OUT(NULL, NULL, "Error: Unable to connect to Net Diagnostics Server IP %s, Port = %d for %s\n",
                              global_settings->net_diagnostics_server, global_settings->net_diagnostics_port, buffer);
          NSTL1_OUT(NULL, NULL, "       Please check NDC is running or not.\n");
        }
          return -1;
      }
    }
    else //In case of non blocking socket we need to set socket to blocking and send message  
    {
      /* Making above open socket to blocking*/
      /*if(fcntl(ndc_mccptr.fd, F_SETFL, ~O_NONBLOCK) < 0)
      {
        NSTL1(NULL, NULL, "Error: fcntl() in making connection blocking for socket %d, errno %d (%s).\n", ndc_mccptr.fd, errno, nslb_strerror(errno));
        sprintf(err_msg, "Error: fcntl() in making connection blocking for socket %d, errno %d (%s).\n", ndc_mccptr.fd, errno, nslb_strerror(errno));
        return 0;
      }*/
    }
    NSDL2_MON(NULL, NULL, "ndc_mccptr.fd = %d, message = %s", ndc_mccptr.fd, buffer);
    if (send(ndc_mccptr.fd, buffer, strlen(buffer), 0) != strlen(buffer))
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Error in sending stop_net_diagnostics message to NDC\n");
      NSTL1_OUT(NULL, NULL, "Error in sending stop_net_diagnostics message to NDC\n");
      return -1;
    }
  }

  MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Message sent to stop the NDC. Message = %s", buffer);
  NSDL1_MON(NULL, NULL, "Method End.");
  return 0;
}

/* Function used to read NDC message, this will be called after post processing*/
int read_ndc_stop_msg()
{
  char *read_msg = NULL, *tmp;  
  int count = 0;

  NSDL1_MON(NULL, NULL, "Method called, ndc_mccptr.fd = %d", ndc_mccptr.fd);

  //On write failure we can have fd -1
  if (ndc_mccptr.fd != -1) 
  {
    //wait until msg is received or connection close
    while(1)
    {
      count++;
      NSTL1_OUT(NULL, NULL, "NETDIAGNOSTICS: Waiting for ND Data Processing Complete Message from ND Collector ...\n");
      if((read_msg = read_ndc_reply_msg()) != NULL) 
      {
        break;
      }
      sleep(10);
      if(count == 10)
      {
        NSTL1_OUT(NULL, NULL, "NETDIAGNOSTICS: Did not received ND Data Processing Complete Message from ND Collector.\n");
        return 0;
      }
    }

    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Got message from NDC. Message = %s", read_msg); 
    if((tmp = strstr(read_msg, "Error")))
    {
      tmp += 6; /*strlen("Error;")*/
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Got error from NDC while reading ack for stop message. Error = %s", tmp);
      NSTL1_OUT(NULL, NULL, "NETDIAGNOSTICS ERROR:%s\n", tmp);
   
      close(ndc_mccptr.fd);
      ndc_mccptr.fd = -1;
      return -1;
    }
    else
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "NETDIAGNOSTICS: Successfully received ND Data Processing Complete Message from ND Collector.\n");
      NSTL1_OUT(NULL, NULL, "NETDIAGNOSTICS: Successfully received ND Data Processing Complete Message from ND Collector.\n");
    }
 
    if(ndc_mccptr.fd != -1) {
      close(ndc_mccptr.fd);
      ndc_mccptr.fd = -1;
    }
  }
  NSDL2_MON(NULL, NULL, "Method End.");
  return 0;
}

/****************************************************CONTINUES MONITORING*********************************************/
/* send_partition_switching_msg_to_ndc
 *
 * This function will create message and send to NDC 
 * using write_msg()
 * If NDC connection is closed then message will be send during recovery
 * */
//If while sending partition switch message to NDC connection is not created or fd is negative. No need to send partial switch message again. Because when connection will re-establish NS will send start-up message to NDC and it will send new parition id with start-up message. And NDC after receiveing new partition considers it as partition switch.
void send_partition_switching_msg_to_ndc()
{
  char SendMsg[4096] = "\0";

  NSDL1_MON(NULL, NULL, "Method called");

  //Create messaage to send to NDC
  if(global_settings->net_diagnostics_mode)
  {
    create_ndc_msg(SendMsg);
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Sending switch message to NDC. Message = %s", SendMsg);

    if(ndc_mccptr.fd == -1)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "NDC connection is not established. Message will be send on recovery.\n");
      return;
    }
    //Send message to NDC 
    write_msg(&ndc_mccptr, (char *)&SendMsg, strlen(SendMsg), 0, DATA_MODE);
  }
  
  create_ndc_msg_for_data(SendMsg);

  MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Sending switch message to NDC. Message = %s", SendMsg);
    
  if(ndc_data_mccptr.fd == -1)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "NDC data connection is not established. Message will be send on recovery.\n");
    return;
  }

  //Send message to NDC 
  write_msg(&ndc_data_mccptr, (char *)&SendMsg, strlen(SendMsg), 0, DATA_MODE);
}


void make_ndlogs_dir_and_link()
{
  char buf1[1024] = "";
  char buf2[1024] = "";

  if(global_settings->multidisk_ndlogs_path && global_settings->multidisk_ndlogs_path[0])
  {
    snprintf(buf1, 1024, "%s/%s/nd/logs/", global_settings->multidisk_ndlogs_path, global_settings->tr_or_partition);
    //It will create path till nd directory only. logs will not be created
    snprintf(buf2, 1024, "%s/logs/%s/nd/logs", g_ns_wdir, global_settings->tr_or_partition);

    mkdir_ex(buf1);
    mkdir_ex(buf2);
    symlink(buf1, buf2);

    snprintf(buf1, 1024, "%s/TR%d/nd/logs/", global_settings->multidisk_ndlogs_path, testidx);
    snprintf(buf2, 1024, "%s/logs/TR%d/nd/logs", g_ns_wdir, testidx);
    mkdir_ex(buf1);
    mkdir_ex(buf2);
    symlink(buf1, buf2);
  }
  else
  {
    snprintf(buf1, 1024, "%s/logs/%s/nd/logs/", g_ns_wdir, global_settings->tr_or_partition);
    mkdir_ex(buf1);

    snprintf(buf1, 1024, "%s/logs/TR%d/nd/logs/", g_ns_wdir, testidx);
    mkdir_ex(buf1);
  }
}

int send_msg_to_ndc(char *msg_buff)
{
  //int fd;
  char state;
  int flag=-1;
  Msg_com_con *tmp_ndc_ptr;
  int fd;

  if(global_settings->net_diagnostics_mode) 
  {
    //fd = ndc_mccptr.fd;
    state = ndc_mccptr.state;
    flag = CONTROL_MODE;
    tmp_ndc_ptr = &ndc_mccptr;
    fd = ndc_mccptr.fd;
    
  }
  else
  {
    //fd = ndc_data_mccptr.fd;
    state = ndc_data_mccptr.state;
    flag = DATA_MODE;
    tmp_ndc_ptr = &ndc_data_mccptr;
    fd = ndc_data_mccptr.fd;
  }
  
  if(state & NS_CONNECTED)
  {
    if(write_msg(tmp_ndc_ptr, msg_buff, strlen(msg_buff), 0, flag) < 0)
    {
      NSTL1_OUT(NULL, NULL, "nd_send_msg_to_ndc_on_start: Error in sending message to NDC."
                      "Net Diagnostics Server IP %s, Port = %d. Error = %s\n",
                      global_settings->net_diagnostics_server, global_settings->net_diagnostics_port, nslb_strerror(errno));
 
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "nd_send_msg_to_ndc_on_start: Error in sending message to NDC."
                      "Net Diagnostics Server IP %s, Port = %d. Error = %s\n",
                      global_settings->net_diagnostics_server, global_settings->net_diagnostics_port, nslb_strerror(errno));  

      close(fd);
      fd = -1;
      return -1; //error
    }
    return 0;
  }
  return 1;
}

void parse_ndc_node_msgs()
{
  char *fields[30]; // assuming values will not exceed 30. current max value is ..
  int ret,i;
  char read_msg[NDC_READ_BUF_SIZE + 1] = {0};
  for(i=0;i<total_ndc_node_msg;i++)
  {
    strcpy(read_msg,ndc_node_msg[i]);
    char *ptr=read_msg;
    if(!strncmp(ptr, "ndc_node_discovery_ctrl_msg:", 28))
    {
      ptr = ptr + 28;
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "ndc_node_discovery_ctrl_msg is receive with buff = %s",ptr);
      //tokenize ptr
      ret = get_tokens_with_multi_delimiter(ptr, fields, ";", 30);

      parse_and_extract_server_info(fields, ret, NODE_DISCOVERY); 
    }
    else if(!strncmp(ptr, "ndc_node_end_mon:", 17))
    {
      ptr = ptr + 17;
      MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "ndc_monitor_end_msg is receive with buff = %s",ptr);
      //tokenize ptr
      ret = get_tokens_with_multi_delimiter(ptr, fields, ";", 30);

      parse_and_extract_server_info(fields, ret, END_MONITOR);
    }
    else if(!strncmp(ptr, "ndc_node_modify:", 16))
    {
      ptr = ptr + 16;
      MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "ndc_node_modify_msg is receive with buff = %s",ptr);
       //tokenize ptr
      ret = get_tokens_with_multi_delimiter(ptr, fields, ";", 30);

      parse_and_extract_server_info(fields, ret, NODE_MODIFY);
    }
    else if(!strncmp(ptr, "ndc_server_inactive:", 20))
    {
      ptr = ptr + 20;
      MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "ndc_server_inactive is receive with buff = %s",ptr);
      //tokenize ptr
      ret = get_tokens_with_multi_delimiter(ptr, fields, ";", 30);

      parse_and_extract_server_info(fields, ret, NODE_INACTIVE);
    }
    else if(!strncmp(ptr, "ndc_instance_inactive:", 22))
    {
      ptr = ptr + 22;
      MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "ndc_instance_inactive is receive with buff = %s",ptr);
      //tokenize ptr
      ret = get_tokens_with_multi_delimiter(ptr, fields, ";", 30);

      parse_and_extract_server_info(fields, ret, INSTANCE_INACTIVE);
    }
    else if(!strncmp(ptr, "ndc_instance_delete:", 20))
    {
     ptr = ptr + 20;
     MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "ndc_instance_delete is receive with buff = %s",ptr);
     ret = get_tokens_with_multi_delimiter(ptr, fields, ";", 30);
     parse_and_extract_server_info(fields, ret, INSTANCE_DELETE); 
    }
    else if(!strncmp(ptr, "ndc_instance_down:", 18))
    {
     ptr = ptr + 18;
     MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "ndc_instance_down is receive with buff = %s",ptr);
     ret = get_tokens_with_multi_delimiter(ptr, fields, ";", 30);
     parse_and_extract_server_info(fields, ret, CMON_INSTANCE_DOWN);
    }
    else if(!strncmp(ptr, "ndc_instance_up:", 16))
    {
     ptr = ptr + 16;
     MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "ndc_instance_up is receive with buff = %s",ptr);
     ret = get_tokens_with_multi_delimiter(ptr, fields, ";", 30);
     parse_and_extract_server_info(fields, ret, CMON_INSTANCE_UP);
    }
    else if(!strncmp(ptr, "ndc_server_delete:", 18))
    {
     ptr = ptr + 18;
     MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "ndc_server_delete is receive with buff = %s",ptr);
     ret = get_tokens_with_multi_delimiter(ptr, fields, ";", 30);
     parse_and_extract_server_info(fields, ret, SERVER_DELETE);
    }
    else if(!strncmp(ptr, "ndc_cavmon_msg:", 15))
    {
     ptr = ptr + 15;
     MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "ndc_cavmon_msg is receive with buff = %s",ptr);
     ret = get_tokens_with_multi_delimiter(ptr, fields, ";", 30);
     parse_and_extract_server_info(fields, ret, CMON_CONN_RETRY);
    }
    FREE_AND_MAKE_NULL(ndc_node_msg[i], "ndc_node_msg[i]", i);
   
    /*//If new server has been added.Do remoe and add all control fd
    if(topo_info[topo_idx].g_server_struct_realloc_flag == 1)
    {
      //old code was to remove all servers from epoll, expecting that the serverlist ptr might have changed
      //new structure server_list ptr will never chnage, so just add new server in epoll
      do_remove_add_control_fd();
      topo_info[topo_idx].g_server_struct_realloc_flag = 0;
    }*/
  }
  FREE_AND_MAKE_NULL(ndc_node_msg, "ndc_node_msg", -1);
  total_ndc_node_msg=0;
  max_ndc_node_msg=0;
}
