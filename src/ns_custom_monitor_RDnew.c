#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <regex.h>
#include <errno.h>
#include <curl/curl.h>
#include <time.h>
#include "../../base/topology/topolib_v1/topolib_structures.h"

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "nslb_util.h"
#include "nslb_cav_conf.h"
#include "nslb_alloc.h"
#include "nslb_log.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"
#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "util.h"
#include "tmr.h"
#include "timing.h"
#include "ns_gdf.h"
#include "ns_msg_com_util.h"
#include "ns_log.h"
#include "ns_user_monitor.h"
#include "ns_custom_monitor.h"
#include "ns_check_monitor.h"
#include "ns_pre_test_check.h"
#include "ns_alloc.h"
#include "ns_schedule_phases.h"
#include "ns_child_msg_com.h"
#include "ns_percentile.h"
#include "ns_dynamic_vector_monitor.h"
#include "ns_mon_log.h"
#include "ns_event_log.h"
#include "ns_server_admin_utils.h"
#include "ns_string.h"
#include "ns_event_id.h"
#include "nslb_sock.h"
#include "nslb_get_norm_obj_id.h"
#include "ns_get_log_file_monitor.h"
#include "ns_monitor_profiles.h"
#include "ns_monitoring.h"
#include "ns_coherence_nid_table.h"
#include "ns_trace_level.h"
#include "ns_compress_idx.h"
#include "ns_nv_tbl.h"
#include "netomni/src/core/ni_scenario_distribution.h"
#include "ns_parent.h"
#include "ns_ndc.h"
#include "ns_ndc_outbound.h"
#include "v1/topolib_runtime_changes.h"
#include "ns_standard_monitors.h"
#include "ns_runtime_changes_monitor.h"
#include "ns_exit.h"
#include "wait_forever.h"
#include "nslb_mon_registration_util.h"
#include "ns_appliance_health_monitor.h"
#include "nslb_mon_registration_con.h"
#include "ns_custom_monitor_RDnew.h"
#include "ns_error_msg.h"
#include "ns_ip_data.h"

#include "init_cav.h"
#include "nslb_msg_com.h"
#include "ns_monitor_2d_table.h"
#include "ns_monitor_init.h"
#include "ns_tsdb.h"
#define MON_REUSED_NORMALLY 0
#define MON_REUSED_INSTANTLY 1
NodeList *node_list;

#define DELTA_CUSTOM_MONTABLE_ENTRIES 5

char is_query_execution_norm_init_done = 0;
char is_query_execution_norm_init_done_for_planhandling = 0;
char is_temp_db_norm_init_done = 0;
NormObjKey *query_execution_sql_id_key;
NormObjKey *plan_handle_sql_id_key;
NormObjKey *temp_db_sql_id_key;
extern int check_allocation_for_reused(CM_info *cus_mon_ptr);
/*
static void usage(char *error, char *mon_name, char *mon_args, int runtime_flag, char *err_msg)
{
//TODO: msg is overwriting in err_msg.....check??

  NSDL2_MON(NULL, NULL, "Method called");
  sprintf(err_msg, "\n%s:\n%s %s\n", error, mon_name, mon_args);
  strcat(err_msg, "Usage:\n");

  strcat(err_msg, "CUSTOM_MONITOR <Create Server IP {NO | NS | Any IP}> <GDF FileName> <Vector Name> <Option {Run Every Time (1) | Run Once (2)}> <Program Path> [Program Arguments]\n");
  //printf("Where: \n"); // need to write help enteries
  strcat(err_msg, "Example:\n");
  strcat(err_msg, "CUSTOM_MONITOR 192.168.18.104 cm_vmstat.gdf VMStat 2 /opt/cavisson/monitors/samples/cm_vmstat\n\n");
}
*/
static void replace_strings(char *buf, char *new_buf, int max_len, char* from, char* to) {

  NSDL2_MON(NULL, NULL, "Method called, Replacing '%s' by '%s'", from, to);

  new_buf[0] = '\0';
  char *p;
  char *q = buf;
  int from_len = strlen(from);
  int to_len = strlen(to);
  int total_len = 0;

  while((p = strstr(q, from)) != NULL) {
    total_len += (p - q) + to_len;
    if(total_len > max_len) {
     NSDL2_MON(NULL, NULL, "Not Replacing '%s' by '%s' as too small buffer. Total = %d, Max = %d",
                            from, to, total_len, max_len);
     strncpy(new_buf, buf, max_len);
     return;
    }
    strncat(new_buf, q, p - q);
    strcat(new_buf, to);
    q = p + from_len;
  }
  strcat(new_buf, q);
}

/*static int cm_set_use_lps(char *mon_name, char *mon_args)
{
  if(((strstr(mon_name, "cm_access_log_stats") != NULL) || (strstr(mon_args, "cm_access_log_stats") != NULL)) ||
    ((strstr(mon_name, "cm_file") != NULL) || (strstr(mon_args, "cm_file") != NULL)) ||
    ((strstr(mon_name, "cm_access_log_status_stats") != NULL) || (strstr(mon_args, "cm_access_log_status_stats") != NULL)))
    return 1;
  else
    return 0;
}*/


//This function will add node at the head of DeletedList
void delete_node_pod(int norm_node_id, NodePodInfo *temp_node_pod_info_ptr)
{
  NodePodInfo *temp;
  temp=node_list[norm_node_id].DeletedList;
  node_list[norm_node_id].DeletedList=temp_node_pod_info_ptr;
  node_list[norm_node_id].DeletedList->next=temp;
  NSTL2(NULL, NULL, "At norm_node_id = %d  the Pod = %s is added to DeletedList\n", norm_node_id, temp_node_pod_info_ptr->PodName);
  NSDL1_MON(NULL, NULL, "setting flag bit for pod delete\n");
  node_list[norm_node_id].flag |= DELETE_POD;
}

//This function will add node at the head of AddedList
void add_node_pod(int norm_node_id, NodePodInfo *temp_node_pod_info_ptr)
{
  NodePodInfo *temp;
  temp=node_list[norm_node_id].AddedList;
  node_list[norm_node_id].AddedList=temp_node_pod_info_ptr;
  (node_list[norm_node_id].AddedList)->next=temp;
  NSTL2(NULL, NULL, "At norm_node_id = %d  the Pod = %s is added to AddedList\n", norm_node_id, temp_node_pod_info_ptr->PodName);
  NSDL1_MON(NULL, NULL, "setting flag bit for pod discovery\n");
  node_list[norm_node_id].flag |= ADD_POD;
} 

//this function will update the time and container count according to the operation and list 
//it will return true if it finds the pod and update it
//we will get the pointer for Addedlist, Head and DeletedList
bool check_pod_in_list(int norm_node_id, NodePodInfo **start, char *field[8], char operation)
{
  NodePodInfo *curr = NULL;
  NodePodInfo *prev = NULL;

  if(*start == NULL)
    return false;

  if(strcmp(field[2], (*start)->NodeName) != 0)
  {
    NSTL2(NULL, NULL, " Nodename at the norm_node_id = %d is not matching with node name = %s", norm_node_id, field[2]);
    return true;
  }

  curr = *start;

  while(curr!=NULL)
  {
    if(!strcmp(curr->PodName,field[5]))
    {
      NSTL2(NULL, NULL, "Container_count = %d for %s of node %s with norn_node_id = %d",curr->container_count, field[5] , field[2], norm_node_id);
      curr->node_start_time = calculate_timestamp_from_date_string(field[0]);
      curr->node_start_time *= 1000;

      curr->pod_start_time = calculate_timestamp_from_date_string(field[3]);
      curr->pod_start_time *= 1000;

      if((operation & REUSE_POD) || ((operation & DELETE_POD) && (curr->container_count == 1)))
      {
        // 1st node
        if(curr == *start)
        {
          *start = curr->next;
        }
        else //subsequent nodes
        {
          prev->next=curr->next;
        }

        if(operation & REUSE_POD)
          add_node_pod(norm_node_id, curr);
        else
          delete_node_pod(norm_node_id, curr);

        return true;

      }
      else
      {
        if(operation & ADD_POD)
        {
          curr->container_count++;
          NSTL2(NULL, NULL, "Container_count incremented to %d for %s of node %s with norn_node_id = %d",curr->container_count, field[5] , field[2], norm_node_id);
        }
        else
        {
          curr->container_count--;
          NSTL2(NULL, NULL, "Container_count decremented to %d for %s of node %s with norn_node_id = %d",curr->container_count, field[5] , field[2], norm_node_id);
        }

        return true;
      }

    }
    else
    {
      prev=curr;
      curr=curr->next;
    }
  }

  return false;
}


bool check_if_pod_exist(int norm_node_id, char *field[8], char operation)
{

  //In case of add pod and delete pod, we are checking in addlist and head 
  //we are not checking delete list because we are moving it to deletelist only when container count is one 
  //If pod is in deletelist and again we are getting delete for it then it is a invalid case
  if((operation & DELETE_POD) || (operation & ADD_POD))
  {
    NSTL2(NULL, NULL, "Checking the pod = %s in all list of node = %s with norm_node_id = %d\n",field[5], field[2], norm_node_id);

    if(check_pod_in_list(norm_node_id, &(node_list[norm_node_id].head), field, operation))
      return true;
    else if(check_pod_in_list(norm_node_id, &(node_list[norm_node_id].AddedList), field, operation))
      return true;

  }
  //In case of reuse pod, we are checking in delete list
  else
  {
    NSTL2(NULL, NULL, "REUSE_POD opreation received for pod = %s of node = %s ", field[5], field[2]);

    NSTL2(NULL, NULL, "Checking the pod = %s in all list of node = %s with norm_node_id = %d\n",field[5], field[2], norm_node_id);

    if(check_pod_in_list(norm_node_id, &(node_list[norm_node_id].DeletedList), field, operation))
      return true;
    else if(check_pod_in_list(norm_node_id, &(node_list[norm_node_id].AddedList), field, ADD_POD))//sending ADD_POD because REUSE_POD will remove node from the list
      return true;
    else if(check_pod_in_list(norm_node_id, &(node_list[norm_node_id].head), field, ADD_POD))
      return true;
  }
  
  NSTL2(NULL, NULL, "Pod = %s not found in any list of node = %s with norm_node_id = %d\n",field[5], field[2], norm_node_id);
  
  return false;
}

//Apply kubernetes monitor
void apply_kube_mon(NodePodInfo *pod_info_ptr,int tier_idx, int server_idx)
{

  if(total_mon_config_list_entries > 0) 
  {
    mj_apply_monitors_on_autoscaled_server(pod_info_ptr->vector_name, server_idx, tier_idx, pod_info_ptr->appname_pattern);
  }
  else
  {
    add_json_monitors(pod_info_ptr->vector_name, server_idx, tier_idx, pod_info_ptr->appname_pattern, 1, 0, 0);
  }
}

//This function will tokenize the data,take out node,pod norm id and fill it accordingly in the node pod list
int add_node_and_pod_in_list(char *vector_name, CM_vector_info *vector_row, CM_info *cm_info_mon_conn_ptr, int idx, char operation)
{
  char *field[10];
  char *temp_ptr = vector_name;
  char temp_vector_name[2048];
  temp_vector_name[0] = '\0';
  int norm_node_id = -1;
  int node_new_flag = 0;
  int node_len = 0;
  int token_no;
  int server_info_index = -1;
  //int server_index = -1;
  bool loop_whole_list = false;
  int tier_idx=cm_info_mon_conn_ptr->tier_index;
  vector_row->vectorIdx = idx;

  // init for Node
  if(!norm_tbl_init_done_for_node)
  {
    MY_MALLOC(node_id_key, (16*1024 * sizeof(NormObjKey)), "Memory allocation to Norm Node table", -1);
    nslb_init_norm_id_table(node_id_key, 16*1024);
    norm_tbl_init_done_for_node = 1;
  }

  if(operation & ADD_POD)
  {
    temp_ptr = strchr(vector_name, ':');
    temp_ptr++;
  }

  strcpy(temp_vector_name, temp_ptr);

  //This function takes vector name and parses the it with hierarchical view seperator.
  if(parse_kubernetes_vector_format(temp_vector_name , field) < 0)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, cm_info_mon_conn_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
              " Received invalid vector %s", vector_name);
    if(operation & ADD_POD)
    {
      free_vec_lst_row(vector_row);
      cm_info_mon_conn_ptr->total_vectors--;
    }

    return -1;
  }

  node_len = strlen(field[2]);

  //norm_node_id = nslb_get_or_gen_norm_id(node_id_key, field[2], strlen(field[2]), &node_new_flag);
  if(operation & DELETE_POD)
  {
    norm_node_id = nslb_get_norm_id(node_id_key, field[2], node_len);
    if(norm_node_id < 0 )
    {
      NSTL2(NULL, NULL, "Received delete vector for pod = %s for node = %s which does not exist", field[5], field[2]);
      return 0;
    }

    NSTL2(NULL, NULL, "DELETE_POD opreation received for pod = %s of node = %s ", field[5], field[2]);

    if(check_if_pod_exist(norm_node_id, field, operation))
    {
      return 0;
    }

    NSTL2(NULL, NULL, "DELETE_POD opreation received for pod = %s of node = %s not found in list", field[5], field[2]);
  }
  else
  {
    norm_node_id = nslb_get_set_or_gen_norm_id_ex(node_id_key, field[2], node_len, &node_new_flag);

    //this structure is used for storing server idx and appname (in app name we store pod name)
    MY_MALLOC_AND_MEMSET(vector_row->kube_info, sizeof(Kube_Info), "custom_monitor_vector", 0)
    MALLOC_AND_COPY(field[5], vector_row->kube_info->app_name, strlen(field[5]) + 1, "app_name", -1);
    NSTL2(NULL, NULL, "norm_node_id = %d for node = %s", norm_node_id, field[2]);

    // New node received so reallocate if required
    //Set List pointers to NULL
    //Set New Node flag 0x01
    if(node_new_flag)
    {
      //If index returned by nslb_get_or_gen_norm_id_ex is more than max then reallocate
      if(norm_node_id >= max_node_list_entries)
      {
        MY_REALLOC_AND_MEMSET(node_list, ((norm_node_id + DELTA_NODE_POD_ENTRIES) * sizeof(NodeList)), (max_node_list_entries * sizeof(NodeList)), "Creating row of Node Table",max_node_list_entries);

        max_node_list_entries = norm_node_id + DELTA_NODE_POD_ENTRIES;
        NSTL2(NULL, NULL, "Reallocation done for new node = %s with norm_node_id = %d , max_node_list_entries = %d ", field[2] , norm_node_id,max_node_list_entries);
      }
      sprintf(node_list[norm_node_id].NodeIp, "%s", field[1]);

      node_list[norm_node_id].server_info_index = -1;
      node_list[norm_node_id].CmonPodName = NULL;
      node_list[norm_node_id].head = NULL;
      node_list[norm_node_id].AddedList = NULL;
      node_list[norm_node_id].DeletedList = NULL;
      node_list[norm_node_id].flag |= ADD_NODE;
      node_list[norm_node_id].tier_idx= tier_idx;
      node_list_entries++;

      vector_row->kube_info->server_idx = -1;
       
      NSTL2(NULL, NULL, "tier_idx we got is = %d for node = %s", tier_idx, field[2]);
      NSTL2(NULL, NULL, "New node = %s added with norm node id = %d and max_node_list_entries = %d , node_list_entries = %d",field[2], norm_node_id, max_node_list_entries, node_list_entries);

    }
    else
    {
      //here we are storing existing server_info_index of node as we are using this in add kubernetes monitor on runtime
      vector_row->kube_info->server_idx = node_list[norm_node_id].server_info_index;
      if(check_if_pod_exist(norm_node_id, field, operation))
      {
        return 0;
      }
    }


    //New POD discovered, 1. create pod node 
    // Add this pod node in AddedList (When we send message to NDC we will move to the Head)
    NodePodInfo *node_pod_info_ptr;
    MY_MALLOC_AND_MEMSET(node_pod_info_ptr, sizeof(NodePodInfo), "node_pod_info_ptr,", 0)

    node_pod_info_ptr->node_start_time = calculate_timestamp_from_date_string(field[0]);
    node_pod_info_ptr->node_start_time *= 1000;
    
    MALLOC_AND_COPY(field[2], node_pod_info_ptr->NodeName, node_len + 1, "Node name", -1);

    MALLOC_AND_COPY(field[1], node_pod_info_ptr->NodeIp, strlen(field[1]) + 1, "Node ip", -1);

    node_pod_info_ptr->pod_start_time = calculate_timestamp_from_date_string(field[3]);
    node_pod_info_ptr->pod_start_time *= 1000;

    MALLOC_AND_COPY(field[5], node_pod_info_ptr->PodName, strlen(field[5]) + 1, "Pode name", -1);

    node_pod_info_ptr->container_count = 1;
    node_pod_info_ptr->next = NULL;

  if(!strncmp(node_pod_info_ptr->PodName, cm_info_mon_conn_ptr->cmon_pod_pattern, strlen(cm_info_mon_conn_ptr->cmon_pod_pattern)))
    {
      loop_whole_list = true;
      node_list[norm_node_id].server_info_index = -1;

      FREE_AND_MAKE_NULL(node_list[norm_node_id].CmonPodName, "CmonPodName", norm_node_id);

      sprintf(node_list[norm_node_id].CmonPodIp, "%s", field[4]);
      MALLOC_AND_COPY(node_pod_info_ptr->PodName, node_list[norm_node_id].CmonPodName, strlen(node_pod_info_ptr->PodName) + 1, "CmonPode name", -1);

     MLTL1(EL_DF, 0, 0, _FLN_, cm_info_mon_conn_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"CmonPodName = %s added to NodeName = %s with NodeIp = %s at norm_node_id = %d\n",node_list[norm_node_id].CmonPodName, node_pod_info_ptr->NodeName, node_list[norm_node_id].NodeIp, norm_node_id);
    }


    // Add pod in list
    if (node_list[norm_node_id].head != NULL)
    {
      if (strcmp(field[2], node_list[norm_node_id].head->NodeName) == 0)
      {
        MLTL4(EL_DF, 0, 0, _FLN_, cm_info_mon_conn_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"calling add_node_pod function");
        add_node_pod(norm_node_id, node_pod_info_ptr);
      }
      else
      {
        MLTL1(EL_DF, 0, 0, _FLN_, cm_info_mon_conn_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"Nodename at the norm_node_id = %d is not matching with node name = %s", norm_node_id, field[2]); 
      }
    }
    else
    {
      MLTL4(EL_DF, 0, 0, _FLN_, cm_info_mon_conn_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION,"calling add_node_pod function 2");
      add_node_pod(norm_node_id, node_pod_info_ptr);
    }


    NSTL2(NULL, NULL, "calling add_json_monitors to apply the monitors on pod = %s of node = %s", field[5], field[2]);
    //this structure is used for storing server idx and appname (in app name we store pod name)
    MY_MALLOC_AND_MEMSET(vector_row->kube_info, sizeof(Kube_Info), "custom_monitor_vector", 0)
    vector_row->kube_info->server_idx = -1;
    MALLOC_AND_COPY(field[5], vector_row->kube_info->app_name, strlen(field[5]) + 1, "app_name", -1);

    for(token_no=0;token_no<cm_info_mon_conn_ptr->total_appname_pattern;token_no++)
    {
      if(strstr(field[5], cm_info_mon_conn_ptr->appname_pattern[token_no]))
      {
        MALLOC_AND_COPY(cm_info_mon_conn_ptr->appname_pattern[token_no], node_pod_info_ptr->appname_pattern, strlen(cm_info_mon_conn_ptr->appname_pattern[token_no]) + 1, "appname_pattern", token_no);
        MALLOC_AND_COPY(vector_name, node_pod_info_ptr->vector_name, strlen(vector_name) + 1, "vector_name", -1);

        if(node_list[norm_node_id].CmonPodName != NULL || !is_outbound_connection_enabled)
        {

           MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "CmonPodName = %s for NodeName = %s", node_list[norm_node_id].CmonPodName,field[2]);
 

          if(node_list[norm_node_id].server_info_index == -1)
          {
            if(is_outbound_connection_enabled)
            {
             
              server_info_index =topolib_check_server_name_exists_or_not_in_topology(node_list[norm_node_id].CmonPodName, topo_info[topo_idx].topo_tier_info[tier_idx].tier_name,topo_idx);                                          
              node_list[norm_node_id].server_info_index = server_info_index;
              NSTL2(NULL, NULL, "server_index we got is = %d ", server_info_index);
            }
            else
            {

              server_info_index =topolib_check_server_name_exists_or_not_in_topology(field[2],topo_info[topo_idx].topo_tier_info[tier_idx].tier_name,topo_idx);
              NSTL2(NULL, NULL, "server_index we got is = %d ", server_info_index);
              if(server_info_index == -1)
              {
                MLTL3(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,"check_server_name_exists_or_not_in_topology got server_info_index = %d\n", server_info_index);
                topolib_add_new_server_without_id(topo_info[topo_idx].topo_tier_info[tier_idx].tier_id_tf,topo_info[topo_idx].topo_tier_info[tier_idx].tier_name, -1,field[1],"/opt/cavison/monitors","/usr/bin", "LinuxEx", field[2],topo_idx,node_list[norm_node_id].tier_idx);
                 /*if(topo_info[topo_idx].g_server_struct_realloc_flag == 1)
                 {
                   do_add_control_fd(tier_idx,server_info_index);
                   topo_info[topo_idx].g_server_struct_realloc_flag = 0;
                 }*/

              }
              server_info_index =topolib_check_server_name_exists_or_not_in_topology(field[2],topo_info[topo_idx].topo_tier_info[tier_idx].tier_name,topo_idx);                                          
              node_list[norm_node_id].server_info_index = server_info_index;

            }
          }
          else
          {
            server_info_index = node_list[norm_node_id].server_info_index;
          }

           MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "server_info_index =%d for NodeName = %s and PodName =%s",server_info_index,field[2],field[5]);

          //store servr_index and app_name
          vector_row->kube_info->server_idx = server_info_index;
           
          if(server_info_index > -1)
          {
            if(loop_whole_list && is_outbound_connection_enabled )
            {
              MLTL3(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                                        "Going to Loop for the whole LinkedList for NodeName = %s at norm_node_id = %d",field[2],norm_node_id);

              NodePodInfo *curr = node_list[norm_node_id].AddedList;
              while(curr != NULL)
              {
                if(curr->appname_pattern != NULL)
                  apply_kube_mon(curr, tier_idx , server_info_index); 
                curr=curr->next;
              }

              curr = node_list[norm_node_id].head;
              while(curr != NULL)
              {
                if(curr->appname_pattern != NULL)
               
                  apply_kube_mon(curr,tier_idx ,server_info_index);
                curr=curr->next;
              }

              curr = node_list[norm_node_id].DeletedList;
              while(curr != NULL)
              {
                if(curr->appname_pattern != NULL)
                  apply_kube_mon(curr, tier_idx,server_info_index);

                curr=curr->next;
              }
            }
            else
            {
              apply_kube_mon(node_pod_info_ptr, tier_idx ,server_info_index);

              MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                                              "Calling add_json_monitors_function for single PodName = %s of NodeName = %s",field[5], field[2]);
            }
          }
          else
          {
            MLTL1(EL_DF, 0, 0, _FLN_, cm_info_mon_conn_ptr , EID_DATAMON_INV_DATA, EVENT_MINOR,
           "Server_info_index not found for NodeName = %s , NodeIp = %s , PodName = %s , PodIp = %s\n", field[2], field[1], field[5], field[4]);
          }
        }
        break;
      }
    }
  }

  return 0;
}


//NEW CODE ENDS


//function to parse file provided in keyword with the vectors of BT and Backend Calls
/*static int parse_init_vectors_file(int monitor_list_index, char *init_vector_file, char *breadcrumb, int runtime_flag)
{
  FILE *vec_fp;
  char vector_line[MAX_LINE_LENGTH], tmpbuff[MAX_LINE_LENGTH], vector[MAX_LINE_LENGTH] = {0}, temp_init_vector_file[MAX_LINE_LENGTH];
  int bytes_read = 0;
  char overallVector[16];

  if (init_vector_file[0] == '/')
  {
    NSTL2(NULL, NULL, "Provide path without absolute path. Current path passed is %s", init_vector_file);
    return 0;
  }
  else
  {
   sprintf(temp_init_vector_file,"%s/%s", g_ns_wdir, init_vector_file);
  }

  vec_fp = fopen(temp_init_vector_file, "r");
  if(!vec_fp)
  {
    NSTL1_OUT(NULL, NULL, "Error: In opening File: %s\n", temp_init_vector_file);
  }
  else
  {
    sprintf(overallVector, "%s%c%s", "Overall", global_settings->hierarchical_view_vector_separator, "Overall");

    while (nslb_fgets(vector_line, MAX_LINE_LENGTH, vec_fp, 0) != NULL)
    {
      bytes_read = strlen(vector_line);

      vector_line[bytes_read - 1] = '\0';

      if(vector_line[0] == '#' || vector_line[0] == '\0')  // blank line
        continue;
      if(sscanf(vector_line, "%s", tmpbuff) == -1)  // for blank line with some spaces.
        continue;
      if(strstr(vector_line, overallVector) != NULL) //skip overall vectors
        continue;

      strcpy(vector, vector_line);

      add_cm_vector_info_node(monitor_list_index, vector, NULL, 0, breadcrumb, runtime_flag, 1); 
    }
  }

  CLOSE_FP(vec_fp);
  temp_init_vector_file[0]='\0';
  return(0);
}*/

  
// Information: dyn_cm is kept to differentitate custom and dynamic because we wanted to set vector name initially bcoz monitor is not sending vectorm name and only sending the data line.

int add_cm_vector_info_node(int monitor_list_index, char *vector_name, char *buffer, int max, char *breadcrumb, int runtime_flag, int dyn_cm)
{
  CM_info *cm_info_mon_conn_ptr = monitor_list_ptr[monitor_list_index].cm_info_mon_conn_ptr;
  int vector_list_row  = -1;
  char *temp_ptr = NULL; 
  int tieridx = -1;
  int instanceidx = -1;
  int vectoridx = -1;
  char backend_name[512] = {0};
  char data[512] = {0};
  int i,vectorid, j=0, m;
  int error_check;
  int num_field;
  char *field[8];
  int do_vector_validation_flag = 0;
  char err_msg[1024];
  err_msg[0] = '\0';
  char unknown_breadcrumb[10]={0};
  char vector[512] = {0};
  int vector_idx = 0;
  int vector_count = 0;
  int ret;
  int idx;
  int gdf_norm_id = -1;
  MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
   "calling add_cm_vector_info_node func monitor_list_index=%d vector_name=%s buffer=%s max=%d breadcrumb=%s runtime_flag=%d dyn_cm=%d",monitor_list_index,vector_name,buffer,max,breadcrumb,runtime_flag,dyn_cm);


  if(dyn_cm)
  {
    if(cm_info_mon_conn_ptr->total_vectors == cm_info_mon_conn_ptr->max_vectors)
    {
      if(create_table_entry_ex(&vector_list_row, &(cm_info_mon_conn_ptr->total_vectors), &(cm_info_mon_conn_ptr->max_vectors), (char **)&(cm_info_mon_conn_ptr->vector_list), sizeof(CM_vector_info), "Vector List Table") == -1)
      {
        return -1;
      }
      vector_list_row = cm_info_mon_conn_ptr->total_vectors;
      cm_info_mon_conn_ptr->total_vectors++;
    }
    else
    {
      vector_list_row=(cm_info_mon_conn_ptr->total_vectors)++;
    }
  }
  else
  {
    MY_MALLOC_AND_MEMSET(cm_info_mon_conn_ptr->vector_list, sizeof(CM_vector_info), "custom_monitor_vector", 0)
    cm_info_mon_conn_ptr->max_vectors = 1;
    cm_info_mon_conn_ptr->total_vectors = 1;
    vector_list_row=0;
  }

  if(dyn_cm)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, cm_info_mon_conn_ptr , EID_DATAMON_INV_DATA, EVENT_MINOR,
                     "Received new vector name %s in data from custom monitor: %s. Adding this vector in group.", vector_name, vector_name);
  }

  CM_vector_info *vector_row = &(cm_info_mon_conn_ptr->vector_list[vector_list_row]);

  vector_row->cm_info_mon_conn_ptr = cm_info_mon_conn_ptr;

  vector_row->vector_state = CM_INIT;
  vector_row->instanceIdx = -1;
  vector_row->tierIdx = -1;
  vector_row->vectorIdx = -1;
  vector_row->mappedVectorIdx = -1;
  vector_row->rtg_index[METRIC_PRIORITY_HIGH] = -1;
  vector_row->rtg_index[METRIC_PRIORITY_MEDIUM] = -1;
  vector_row->rtg_index[METRIC_PRIORITY_LOW] = -1;
  vector_row->rtg_index[MAX_METRIC_PRIORITY_LEVELS] = -1;
  vector_row->inst_actual_id = -1;
  vector_row->group_vector_idx = -1;
  vector_row->generator_id = -1;


  //for nd backend monitor, we need to fill some data
  if(cm_info_mon_conn_ptr->flags & ND_MONITOR)
  {
    if (strchr(vector_name, '|'))       //Mani: Its just a safety check for ND vector format, will remove this after 2-3 months. [7/02/1018]
    {
      //Here we are sending expected field 4 because in vector name there is no data present here
      //0|0|1:Default>Localhost>Instance2
      if(parse_vector_new_format(vector_name, vector, &tieridx, &instanceidx, &vectoridx, backend_name, data, NULL, 4, cm_info_mon_conn_ptr->gdf_name) < 0)
      {
        free_vec_lst_row(vector_row);
        cm_info_mon_conn_ptr->total_vectors--;
        return -1;
      }

      vector_row->vector_length = strlen(vector);
      MALLOC_AND_COPY(vector, vector_row->vector_name, vector_row->vector_length + 1, "Custom Monitor Name", vector_list_row);

      //fill actual id
      vector_row->inst_actual_id = instanceidx;

      if(cm_info_mon_conn_ptr->instanceIdxMap)
	vector_row->instanceIdx =  ns_get_id_value(cm_info_mon_conn_ptr->instanceIdxMap, instanceidx);
      else
        vector_row->instanceIdx =  instanceidx;

      if(cm_info_mon_conn_ptr->tierIdxmap)
        vector_row->tierIdx = ns_get_id_value(cm_info_mon_conn_ptr->tierIdxmap, tieridx);
      else
        vector_row->tierIdx = tieridx;

      vector_row->mappedVectorIdx = ns_get_id_value(cm_info_mon_conn_ptr->ndVectorIdxmap, vectoridx);

      vector_row->vectorIdx = vectoridx;

      MLTL3(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "instanceidx = [%d] , vector_row->instanceIdx = [ %d] , tieridx = [%d] , vector_row->tierIdx = [%d] \n", instanceidx, vector_row->instanceIdx, tieridx, vector_row->tierIdx);
      vector_row->vectorIdx = vectoridx;

      vector_row->flags |= DATA_FILLED;

      NSDL2_MON(NULL, NULL, " vector_row->inst_actual_id = [ %d ] , vector_row->vector_name = [ %s ] \n", vector_row->inst_actual_id, vector_row->vector_name);
    }
    else
    {
      if(!runtime_flag)
      {
        //NOTE: ND monitors cannot be added at runtime.
        //adding this, because dummy entry has no vector id and it is needed to make connections with ndc.
        vector_row->vector_length = strlen(vector_name);
        MALLOC_AND_COPY(vector_name, vector_row->vector_name, vector_row->vector_length + 1, "Custom Monitor Name", vector_list_row);
      }
      else
      {
        if(cm_info_mon_conn_ptr->mbean_mon_idx >= 0)     //Means MBEAN monitor is added at runtime
        {
          vector_row->vector_length = strlen(vector_name);
          MALLOC_AND_COPY(vector_name, vector_row->vector_name, vector_row->vector_length + 1, "Custom Monitor Name", vector_list_row);
        }
        else
        {
          MLTL1(EL_DF, 0, 0, _FLN_, cm_info_mon_conn_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION," Received invalid vector '%s': old format for gdf %s is not supported.", vector_name, cm_info_mon_conn_ptr->gdf_name);
          free_vec_lst_row(vector_row);
          cm_info_mon_conn_ptr->total_vectors--;
          return -1;
        }
      }
    }
  }
  // for NV monitor
  else if(dyn_cm == 1 && (cm_info_mon_conn_ptr->flags & NV_MONITOR))
  {
    error_check = parse_nv_vector(vector_name, vector, &vectorid);

    if(error_check == 0)
    {
      vector_row->vectorIdx = nv_get_id_value(cm_info_mon_conn_ptr->vectorIdxmap, vectorid, NULL);
      vector_row->flags |= DATA_FILLED; // TODO NEED TO FIX THIS , SETTING THIS ALWAYS RIGHT NOW, ALSO IF VECTOR ADDED USING 'ADD_VECTOR|V1 ...' THEN ALSO THIS FLAG WILL GET SET 
      
      vector_row->vector_length = strlen(vector);
      MALLOC_AND_COPY(vector, vector_row->vector_name, vector_row->vector_length+1, "Custom Monitor Name", vector_list_row);
    }
    else
    {
      vector_row->vector_length = strlen(vector);
      MALLOC_AND_COPY(vector, vector_row->vector_name, vector_row->vector_length+1, "Custom Monitor Name", vector_list_row);
    }
  }
     //NetCloud Monitors
  else if(strstr(cm_info_mon_conn_ptr->gdf_name, "ns_ip_data.gdf"))   //Comparing with gdf name as cs_port is not yet set.
  {
    /*First sample : 
       [previous] : GeneratorID:VectorID:GeneratorName>IP\n
       [new]      : GeneratorID:VectorID:GeneratorName>GroupName>IP\n
    Rest sample : 
      GeneratorID:VectorID\n */

    num_field = get_tokens_with_multi_delimiter(buffer, field, ":", 3);

    vector_row->generator_id = atoi(field[0]);
    vector_row->vectorIdx = atoi(field[1]);

    if(num_field > 2)
    {
      MALLOC_AND_COPY(field[2], vector_row->vector_name, strlen(field[2]) + 1, "Custom Monitor Name", vector_list_row);
    }
  }
  else //vector format : id:vectorname|data
  {
    //We have added a check of parent ptr as we do not want dummy vector to enter in this code. This will be called from add_new_vector then wewill have parent ptr.
    if(cm_info_mon_conn_ptr->flags & NA_KUBER)
    {
      temp_ptr = strchr(vector_name, ':');
      if(temp_ptr)
      {
        *temp_ptr = '\0';
        idx = atoi(vector_name);
        *temp_ptr = ':';
      }
      //handling of free vector in case of error is done inside this
      if(add_node_and_pod_in_list(vector_name, vector_row, cm_info_mon_conn_ptr, idx, ADD_POD) < 0)
        return -1;
    }
    if(dyn_cm == 1) //dynamic
    {
      //check if vector name is in new format or not
      temp_ptr = strchr(vector_name, ':');

      //breadcrumb check is to identify coherence monitor, need to skip coherence monitor, breadcrumb will be set only for coherenec monitor
      if(((breadcrumb == NULL) || (breadcrumb[0] == '\0')) && temp_ptr)
      {
        temp_ptr++;
        vectorid = atoi(vector_name);

        //realloc dvm indexng mapping table columns
        if(vectorid >= cm_info_mon_conn_ptr->max_mapping_tbl_vector_entries)
        {
          cm_info_mon_conn_ptr->max_mapping_tbl_vector_entries = create_mapping_table_col_entries(cm_info_mon_conn_ptr->dvm_cm_mapping_tbl_row_idx, vectorid, cm_info_mon_conn_ptr->max_mapping_tbl_vector_entries);
        }

        //Setting here, if vector is discovered then data must have been received.
        dvm_idx_mapping_ptr[cm_info_mon_conn_ptr->dvm_cm_mapping_tbl_row_idx][vectorid].is_data_filled = 1;

        vector_row->vectorIdx = vectorid;
        do_vector_validation_flag = 1;        //We dont want to verify vectors for ND, NV, and coherence.
      }
    }

    if(temp_ptr)
    {
      vector_row->vector_length = strlen(temp_ptr);
      MY_MALLOC(vector_row->vector_name, vector_row->vector_length + 1, "Custom Monitor Name", vector_list_row);
      strcpy(vector_row->vector_name, temp_ptr);
    }
    else
    {
      vector_row->vector_length = strlen(vector_name);
      MY_MALLOC(vector_row->vector_name, vector_row->vector_length + 1, "Custom Monitor Name", vector_list_row);
      strcpy(vector_row->vector_name, vector_name);
    }
  }

  MY_MALLOC(vector_row->data, (cm_info_mon_conn_ptr->no_of_element * sizeof(double)), "Custom Monitor data", vector_list_row); 
  memset(vector_row->data, 0, (cm_info_mon_conn_ptr->no_of_element * sizeof(double)));

  if(!(g_tsdb_configuration_flag & RTG_MODE))
  {
 //   MY_MALLOC(vector_row->metrics_idx, (cm_info_mon_conn_ptr->no_of_element * sizeof(long)), "Custom Monitor metrics id", vector_list_row);
    ns_tsdb_malloc_metric_id_buffer(&vector_row->metrics_idx , cm_info_mon_conn_ptr->no_of_element);
  }


  // set "nan" on monitor data connection break
  for(m = 0; m < cm_info_mon_conn_ptr->no_of_element; m++)
  {
    vector_row->data[m] = 0.0/0.0;
  }

  if(buffer != NULL)
  {
    //Long_long_data *data = cm_table_ptr[cus_mon_row].data;
   char *tmp_pipe_ptr = NULL;

   tmp_pipe_ptr = strchr(buffer, '|');
   if(tmp_pipe_ptr)
     buffer = tmp_pipe_ptr + 1;
 
   if(validate_and_fill_data(cm_info_mon_conn_ptr, max, buffer, vector_row->data, vector_row->metrics_idx, &vector_row->metrics_idx_found,  
                                           (monitor_list_ptr[cm_info_mon_conn_ptr->monitor_list_idx].is_dynamic) ? vector_row->mon_breadcrumb: vector_row->vector_name) < 0)
   {
      NSDL2_MON(NULL, NULL, "validate_and_fill_data return -1");
   }
   else
     vector_row->flags |= DATA_FILLED;
  }	
   
  NSDL3_MON(NULL, NULL, "vector_row->is_mon_breadcrumb_set = %d,"
                        " vector_row->vector_name = %s, dyn_cm = %d, vector_row->is_data_filled = %d, cm_info_mon_conn_ptr->fd = %d",
                         (vector_row->flags & MON_BREADCRUMB_SET),
                         vector_row->vector_name, dyn_cm, (vector_row->flags & DATA_FILLED), cm_info_mon_conn_ptr->fd);

  //set do not merge flag & set breadcrumb, for coherence monitors will receive breadcrumb in arguments 
  if((strstr(cm_info_mon_conn_ptr->gdf_name, "cm_coherence_service") != NULL) || (strstr(cm_info_mon_conn_ptr->gdf_name, "cm_coherence_storage") != NULL) || ((strstr(cm_info_mon_conn_ptr->gdf_name, "cm_coherence") != NULL) && (cm_info_mon_conn_ptr->flags & NEW_FORMAT) && (strstr(vector_name, "AllInstances") == NULL)))
  {
    if(breadcrumb[0] != '\0')
    {
      //save received 'breadcrumb' into structure
      MY_MALLOC(vector_row->mon_breadcrumb, strlen(breadcrumb) + 1, "Custom Monitor BreadCrumb", -1);
      strcpy(vector_row->mon_breadcrumb, breadcrumb);

      //set flag
      vector_row->flags |= MON_BREADCRUMB_SET;
    }
  }
  else
  {
    //TODO: DO WE NEED TO SKIP GDF: HAVING SCALAR GROUP: .. tibco.gdf  , etc
    //set monitor breadcrumb for vector group only.
    if(!(vector_row->flags & MON_BREADCRUMB_SET) && (dyn_cm))
    {
      NSDL3_MON(NULL, NULL, "Calling create_breadcrumb_path");
      create_breadcrumb_path(vector_list_row, runtime_flag, err_msg, cm_info_mon_conn_ptr->vector_list, cm_info_mon_conn_ptr);
    }
  }  

  //skip vectors having unknown breadcrumb 
  if(skip_unknown_breadcrumb)
  {
    if(vector_row->flags & MON_BREADCRUMB_SET)
    {

      //If Unknown comes in tier name then the breadcrumb will not be skipped.Breadcrumb will be skipped only if Unknown comes in server name or instance name

      //if (group_data_ptr[cm_table_ptr[cus_mon_row].gp_info_index].breadcrumb_format == BREADCRUMB_FORMAT_T)
        //sprintf(unknown_breadcrumb, "%s%c", "Unknown",global_settings->hierarchical_view_vector_separator);
      //else    
        sprintf(unknown_breadcrumb, "%c%s%c", global_settings->hierarchical_view_vector_separator,"Unknown",global_settings->hierarchical_view_vector_separator);

      if((vector_row->flags & MON_BREADCRUMB_SET) && (strstr(vector_row->mon_breadcrumb, unknown_breadcrumb) != NULL))
      {
        MLTL1(EL_F, 0, 0, _FLN_, cm_info_mon_conn_ptr, EID_DATAMON_INV_DATA, EVENT_MINOR,
                          "Error: Unknown breadcrumb path (%s) in a monitor whose graph definition file is (%s)\n",
                                          vector_row->mon_breadcrumb, cm_info_mon_conn_ptr->gdf_name);
        free_vec_lst_row(vector_row);
        cm_info_mon_conn_ptr->total_vectors--;
        return -1;
      }
    }
  }

  //To temporarily solve the performance issue of NV ,By skipping the check for duplicate breadcrumb if machine is NV.Lateron some permanent      fixed will be done.
  //if((!strncmp(g_cavinfo.config, "NV", 2)) || (strstr(g_cavinfo.SUB_CONFIG, "NV") != NULL) || (!strncmp(g_cavinfo.SUB_CONFIG, "ALL", 3)))
    if((do_vector_validation_flag == 1) || strstr(cm_info_mon_conn_ptr->gdf_name, "cm_nd_integration_Point_status.gdf"))
    {
      if(verify_vector_format(vector_row, cm_info_mon_conn_ptr) == -1)
      {
        free_vec_lst_row(vector_row);
        cm_info_mon_conn_ptr->total_vectors--;
        return -1;
      }
    }
    // TSDBKJ don't call this for TSDB
  if(!(g_tsdb_configuration_flag & TSDB_MODE))
  {   
    if(vector_row->flags & MON_BREADCRUMB_SET)
    {
      CM_info *cm_ptr = NULL;
      CM_vector_info *vector_list = NULL;
      int gdf_match = 0;

      if(((cm_info_mon_conn_ptr->gdf_flag != CM_GET_LOG_FILE) && (cm_info_mon_conn_ptr->gdf_flag != ORACLE_STATS_GDF)) && (global_settings->dynamic_cm_rt_table_mode  != 2))
      {
        for (i = 0; i < total_monitor_list_entries; i++)
        {
          //It will be removed in at deliver report. Need to slip this monitor till then because it doesnot have vector_list array.
          if(monitor_list_ptr[i].cm_info_mon_conn_ptr->flags & REMOVE_MONITOR)
            continue;

          if (strcmp(monitor_list_ptr[i].gdf_name, cm_info_mon_conn_ptr->gdf_name) == 0)
          {
            if(cm_info_mon_conn_ptr->flags & ND_MONITOR)
            {
               int normid = (cm_info_mon_conn_ptr->total_vectors - 1);
    
               //Here we want to set norm_id for vector received for ND monitors. This function returns norm index of the added string. If string was already added and we are trying to add it with different norm id, then it will return norm id the string that was previously added. We will use that index to mark reuse.
               ret = nslb_set_norm_id(&cm_info_mon_conn_ptr->nd_norm_table, vector_row->vector_name, strlen(vector_row->vector_name), normid);
               if(ret != normid)  //Entry Exist
               {
                  if(strcmp(vector_row->vector_name, cm_info_mon_conn_ptr->vector_list[ret].vector_name) != 0)
                  {
                    MLTL1(EL_DF, 0, 0, _FLN_, cm_info_mon_conn_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION, "The norm id provided by method is '%d'. It provides this norm id means same vector (%s) is present at this index. But the vector present is :%s. This is not possible.", ret, vector_row->vector_name, cm_info_mon_conn_ptr->vector_list[ret].vector_name);
               
                    free_vec_lst_row(vector_row);
                    cm_info_mon_conn_ptr->total_vectors--;
                    return -1;
                  }

                  if((cm_info_mon_conn_ptr->vector_list[ret].vector_state == CM_DELETED) || (cm_info_mon_conn_ptr->vector_list[ret].flags & WAITING_DELETED_VECTOR))
                  {
                    MLTL1(EL_DF, 0, 0, _FLN_, cm_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION, "Deleted custom monitor whose vector is (%s) has been added successfully\n", cm_info_mon_conn_ptr->vector_list[ret].vector_name);

                    if(!(cm_info_mon_conn_ptr->vector_list[ret].flags & WAITING_DELETED_VECTOR))
                    {
                       MLTL3(EL_DF, 0, 0, _FLN_, cm_info_mon_conn_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION, "calling g_vector_runtime_chnage %s", cm_info_mon_conn_ptr->gdf_name);
                      g_vector_runtime_changes = 1;
                      MLTL1(EL_DF, 0, 0, _FLN_, cm_info_mon_conn_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                     " add_custom_dynamic %d", g_vector_runtime_changes);

                      check_allocation_for_reused(cm_info_mon_conn_ptr);
                      cm_info_mon_conn_ptr->reused_vector[cm_info_mon_conn_ptr->total_reused_vectors] = ret;
                      cm_info_mon_conn_ptr->total_reused_vectors++;
                    }
                    else
                      total_waiting_deleted_vectors--;

                    set_reused_vector_counters(cm_info_mon_conn_ptr->vector_list, ret, cm_info_mon_conn_ptr, NULL, 0, NULL, NULL, MON_REUSED_NORMALLY);                   
                    cm_info_mon_conn_ptr->vector_list[ret].vectorIdx = vector_row->vectorIdx;
                    //Copying instanceIdx, tierIdx and mappedVectorIdx because search_and_set_nd_vector_elements() is not calledin case of overall. Thus, there is a duplicate code which we will remove from search_and_set_nd_vector_elements() after detailed analysis.
                    cm_info_mon_conn_ptr->vector_list[ret].instanceIdx = vector_row->instanceIdx;
                    cm_info_mon_conn_ptr->vector_list[ret].tierIdx = vector_row->tierIdx;
                    cm_info_mon_conn_ptr->vector_list[ret].mappedVectorIdx = vector_row->mappedVectorIdx;

                    free_vec_lst_row(vector_row);
                    cm_info_mon_conn_ptr->total_vectors--;
                  }
                  //mark duplicate or if it was already added then mark reused.
                  else
                  {
                    update_health_monitor_sample_data(&hm_data->num_duplicate_vectors);
                    MLTL1(EL_DF, 0, 0, _FLN_, cm_info_mon_conn_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION, "Error: Duplicate vector name(%s) in a monitor whose graph definition file (GDF) is %s.\n", (vector_row->flags & MON_BREADCRUMB_SET)?vector_row->mon_breadcrumb:vector_row->vector_name, cm_info_mon_conn_ptr->gdf_name);
                    free_vec_lst_row(vector_row);
                    cm_info_mon_conn_ptr->total_vectors--;

                    cm_info_mon_conn_ptr->flags &= ~ALL_VECTOR_DELETED;
                    if(!strcmp(cm_info_mon_conn_ptr->cs_ip, "127.0.0.1"))
                      return -1;
                    else
                    {
                      if(runtime_flag)  return -1;
                      else  NS_EXIT(-1, CAV_ERR_1060029, (vector_row->flags & MON_BREADCRUMB_SET)?vector_row->mon_breadcrumb:vector_row->vector_name, cm_info_mon_conn_ptr->gdf_name);
                    }
                  }
                break;
              }
            }
            else 
            {
	      //We will only compare for duplicate when (monitor is itself sending breadcrumb || monitor is of ND type || server name of vector and monitor is same i.e vector belongs to that monitor only)
              if(cm_info_mon_conn_ptr->server_index != monitor_list_ptr[i].cm_info_mon_conn_ptr->server_index)
              //if((strcmp(cm_info_mon_conn_ptr->server_name, monitor_list_ptr[i].cm_info_mon_conn_ptr->server_name)))
              continue;
            }
            gdf_match = 1;
            cm_ptr = monitor_list_ptr[i].cm_info_mon_conn_ptr;
            vector_list = cm_ptr->vector_list;
            //vector_count = (i == monitor_list_index)?(cm_ptr->total_vectors - 1):(cm_ptr->total_vectors);
            //Here j is the normid return by nslb_set_norm_id
            if(total_vector_gdf_hash_entries > 0 && ( cm_ptr->nd_norm_table.size > 0))
            {
              j=0;
              //Need to access vector_idx only if total_vector > 0
              if (cm_ptr->total_vectors > 0)
              {
                vector_idx = cm_ptr->total_vectors - 1;
                if(i == monitor_list_index)
                {
                  j = nslb_set_norm_id(&cm_ptr->nd_norm_table, vector_row->vector_name, strlen(vector_row->vector_name), vector_idx);
                }
                else
                {
                  j = nslb_get_norm_id(&cm_ptr->nd_norm_table, vector_row->vector_name, strlen(vector_row->vector_name));
                }
                vector_count=j+1;
              }
              else
                vector_count = 0;
            }
            else
            {
              j=0;
              vector_count = (i == monitor_list_index)?(cm_ptr->total_vectors - 1):(cm_ptr->total_vectors);
              vector_idx= -1;
            }
            MLTL3(EL_DF, 0, 0, _FLN_, cm_info_mon_conn_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION, "add_cm_vector_info j=%d, vector_count=%d, gdf=%s",
                                      j, vector_count, cm_ptr->gdf_name);

            //old vector count = j
            for(;j < vector_count; j++)
            {
              int pointer_change_done = 0, k=0;
              /*if(i == monitor_list_index && j == vector_list_row)
                continue;*/
              if((i == monitor_list_index && j != vector_idx) || (i != monitor_list_index && j > -1))
              {
              if ((vector_row->flags & MON_BREADCRUMB_SET)?(strcmp(vector_list[j].mon_breadcrumb, vector_row->mon_breadcrumb) == 0):(strcmp(vector_list[j].vector_name, vector_row->vector_name) == 0))
              {
                //if(vector_list[j].flags & RUNTIME_DELETED_VECTOR)
                if((vector_list[j].vector_state == CM_DELETED) || (vector_list[j].flags & WAITING_DELETED_VECTOR))
                {
                  MLTL1(EL_DF, 0, 0, _FLN_, cm_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION, "Deleted custom monitor whose vector is (%s) has been added successfully\n", vector_list[j].vector_name);
                  //if(i != monitor_list_index && cm_ptr->conn_state == CM_DELETED)
                  if(i != monitor_list_index)
                  {
                    //There is still an issue, when new format vectors are sent by monitors, i.e., id|data, and if multiple monitors are applied with same configuration (duplicate), then ("Expected ':' for the vectors received for the first time.") log line is dumped in monitor.log. It is because for the duplicate monitor, vectors will never be added because of duplicate entries, but dvm_mapping_tbl_row_idx will be assigned to it. But mapping table was never updated, because of duplicate. 
		    //So, from the next sample when ID|DATA comes from monitor, it checks in mapping table. It will find -1 in there, so connection will be closed providing the above mentioned log line. This issue needs to be handled. 
                    if(cm_ptr->conn_state != CM_DELETED)
                    {
                      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                               "cm_ptr->conn_state is not equal to CM_DELETED for gdf = %s\n", cm_ptr->gdf_name);
                      free_vec_lst_row(vector_row);
        	      cm_info_mon_conn_ptr->total_vectors--;
                      return -1;
                    }
                    else
                    {
                      //cm_info_mon_conn_ptr new cm_info , cm_ptr existiing cm_info
                      //existing vector list is reallocated
                      //update new cm_info ptr in old vectors
                      //copy existing vectors in new cm_info to vector list of old_cm_info
                      // initially 3 vectors v1 v2 v3 -> this got deleted -> new mon v4 v1 , cm_ptr v1 v2 v3 v4

                      int new_total_vectors = (cm_ptr->total_vectors + cm_info_mon_conn_ptr->total_vectors);
                      if(new_total_vectors > cm_ptr->max_vectors)    //Need to realloc old vector list to fill in new entries
                      {
                        MY_REALLOC_AND_MEMSET(cm_ptr->vector_list, ((new_total_vectors + DELTA_CUSTOM_MONTABLE_ENTRIES) * sizeof(CM_vector_info)), (cm_ptr->max_vectors * sizeof(CM_vector_info)), "Vector List after Reused", -1);
                        cm_ptr->max_vectors = (new_total_vectors + DELTA_CUSTOM_MONTABLE_ENTRIES);
                      }

                      for(k = 0; k < cm_ptr->total_vectors; k++)
                        cm_ptr->vector_list[k].cm_info_mon_conn_ptr = cm_info_mon_conn_ptr;

                      cm_info_mon_conn_ptr->new_vector_first_index = cm_ptr->total_vectors;
      
                      char tmp_breadcrumb_path[1024] = {0};
                      char tmp_vector_name[1024] = {0};
                      for(k = 0; k < (cm_info_mon_conn_ptr->total_vectors - 1); k++)
                      {
                        //KJ:add hash of vector in hash index of cm_ptr
                        if (total_vector_gdf_hash_entries > 0 && (cm_ptr->nd_norm_table.size > 0))
                        {
                          j = nslb_set_norm_id(&cm_ptr->nd_norm_table, cm_info_mon_conn_ptr->vector_list[k].vector_name,
                                              strlen(cm_info_mon_conn_ptr->vector_list[k].vector_name), cm_ptr->total_vectors);

                          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                                                                              "Reused vector normid =%d, Old cm_ptr total_vectors=%d, for vector =%s",j, cm_ptr->total_vectors, cm_info_mon_conn_ptr->vector_list[k].vector_name);
                          if(j != cm_ptr->total_vectors)
                          {
                            free_vec_lst_row(&cm_info_mon_conn_ptr->vector_list[k]);
                            continue;
                          }
                        }

                        memcpy(&cm_ptr->vector_list[cm_ptr->total_vectors], &cm_info_mon_conn_ptr->vector_list[k], sizeof(CM_vector_info));
                        sprintf(tmp_vector_name, "%s", cm_info_mon_conn_ptr->vector_list[k].vector_name);
                        sprintf(tmp_breadcrumb_path, "%s", cm_info_mon_conn_ptr->vector_list[k].mon_breadcrumb);
                        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION, "Reused case Going to free vector name %s of mon id %d for gdf_nam %s", cm_info_mon_conn_ptr->vector_list[k].vector_name, cm_ptr->mon_id, cm_ptr->gdf_name);
                        free_vec_lst_row(&cm_info_mon_conn_ptr->vector_list[k]);
                        cm_ptr->vector_list[cm_ptr->total_vectors].metrics_idx_found = 0;
                        MALLOC_AND_COPY(tmp_vector_name, cm_ptr->vector_list[cm_ptr->total_vectors].vector_name, strlen(tmp_vector_name)+1, "Vector name after memcpy", k);
                        MALLOC_AND_COPY(tmp_breadcrumb_path, cm_ptr->vector_list[cm_ptr->total_vectors].mon_breadcrumb, strlen(tmp_breadcrumb_path)+1, "Breadccrump after memcpy", k);
                        MY_MALLOC(cm_ptr->vector_list[cm_ptr->total_vectors].data, (cm_ptr->no_of_element * sizeof(double)), "Custom Monitor data", k);
                       cm_ptr->total_vectors ++;
                      }
   
                      //Copying information regarding vector list
                      cm_info_mon_conn_ptr->vector_list = cm_ptr->vector_list;
                      if (total_vector_gdf_hash_entries > 0 && (cm_ptr->nd_norm_table.size >0) && (cm_info_mon_conn_ptr->nd_norm_table.size > 0))
                      {
                        nslb_obj_hash_destroy(&(cm_info_mon_conn_ptr->nd_norm_table));
                        cm_info_mon_conn_ptr->nd_norm_table = cm_ptr->nd_norm_table;
                      }

                      cm_info_mon_conn_ptr->dvm_cm_mapping_tbl_row_idx = cm_ptr->dvm_cm_mapping_tbl_row_idx;
      	              cm_info_mon_conn_ptr->max_mapping_tbl_vector_entries = cm_ptr->max_mapping_tbl_vector_entries;
      	              cm_info_mon_conn_ptr->gp_info_index = cm_ptr->gp_info_index;
      
                      //copying information regarding deleted vector list
                      cm_info_mon_conn_ptr->max_deleted_vectors = cm_ptr->max_deleted_vectors;
                      cm_info_mon_conn_ptr->total_deleted_vectors = cm_ptr->total_deleted_vectors;
                      cm_info_mon_conn_ptr->deleted_vector = cm_ptr->deleted_vector;

                      //setting relative idx and data filled of new cm_info for first vector
                      dvm_idx_mapping_ptr[cm_info_mon_conn_ptr->dvm_cm_mapping_tbl_row_idx][vectorid].is_data_filled = 1;
                      dvm_idx_mapping_ptr[cm_info_mon_conn_ptr->dvm_cm_mapping_tbl_row_idx][vectorid].relative_dyn_idx = j;
                    
                      //cm_info_mon_conn_ptr->monitor_list_idx = cm_ptr->monitor_list_idx;
    
                      (monitor_list_ptr[i].cm_info_mon_conn_ptr)->flags |= REMOVE_MONITOR;
                    
                      //monitor_list_ptr[monitor_list_index].no_of_monitors = monitor_list_ptr[i].no_of_monitors;
                      //monitor_list_ptr[i].no_of_monitors = 0;
      
                      pointer_change_done = 1;

                      //put_free_id(&mon_id_pool, cm_ptr->mon_id);
                      //mon_id_map_table[cm_ptr->mon_id].mon_index = -1;
                      //mon_id_map_table[cm_ptr->mon_id].state = -1;

                      NSDL3_MON(NULL, NULL, "Put id: %d of re added dvm monitor: %s, gdf: %s", cm_ptr->mon_id, cm_ptr->monitor_name,
                        cm_ptr->gdf_name);                      
                    }

                    /*if(!(vector_list[j].flags & WAITING_DELETED_VECTOR))
                    {
                      g_vector_runtime_changes = 1;
  
                      check_allocation_for_reused(cm_info_mon_conn_ptr);
                      cm_info_mon_conn_ptr->reused_vector[cm_info_mon_conn_ptr->total_reused_vectors] = j;
                      cm_info_mon_conn_ptr->total_reused_vectors++;
                    }
                    else
                      total_waiting_deleted_vectors--;
                    */
                  }

                  //No need to pass cs_ip, pgm_path, pgm_args and port to copy because it copying from one place to the same place.
                  set_reused_vector_counters(cm_info_mon_conn_ptr->vector_list, j, cm_info_mon_conn_ptr, NULL, 0, NULL, NULL, MON_REUSED_NORMALLY);
                  cm_info_mon_conn_ptr->vector_list[j].vectorIdx = vector_row->vectorIdx;


                  /*g_vector_runtime_changes = 1;

                  check_allocation_for_reused(cm_info_mon_conn_ptr);
                  cm_info_mon_conn_ptr->reused_vector[cm_info_mon_conn_ptr->total_reused_vectors] = j;
                  cm_info_mon_conn_ptr->total_reused_vectors++;*/
                  if(!(vector_list[j].flags & WAITING_DELETED_VECTOR))
                  {
                     MLTL3(EL_DF, 0, 0, _FLN_, cm_info_mon_conn_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION, "calling g_vector_runtime_chnage %s", cm_info_mon_conn_ptr->gdf_name);
                    g_vector_runtime_changes = 1;
                    MLTL1(EL_DF, 0, 0, _FLN_, cm_info_mon_conn_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION, 
                  " add_custom_dynamic %d", g_vector_runtime_changes);
     
                    check_allocation_for_reused(cm_info_mon_conn_ptr);
                    cm_info_mon_conn_ptr->reused_vector[cm_info_mon_conn_ptr->total_reused_vectors] = j;
                    cm_info_mon_conn_ptr->total_reused_vectors++;
                  }
                  else
                    total_waiting_deleted_vectors--;

                  //if(cm_info_mon_conn_ptr->flags & OUTBOUND_ENABLED)
                  //handle_reused_entry_for_CM(cm_info_mon_conn_ptr->vector_list, j, cm_info_mon_conn_ptr->server_index);  //TODO MSR->Replace old function defination with new one 

                  free_vec_lst_row(vector_row);
                  cm_info_mon_conn_ptr->total_vectors--;
 	          cm_info_mon_conn_ptr->total_vectors = cm_ptr->total_vectors;
	          cm_info_mon_conn_ptr->max_vectors = cm_ptr->max_vectors;
                  if(pointer_change_done)
                  { 
                    cm_ptr->vector_list = NULL;
                    cm_ptr->total_vectors = 0;
                    cm_ptr->max_vectors = 0;
                    cm_ptr->deleted_vector = NULL;
                    cm_ptr->max_deleted_vectors = 0;
                    cm_ptr->total_deleted_vectors = 0;

                    
                  }

                  cm_info_mon_conn_ptr->flags &= ~ALL_VECTOR_DELETED;
                  return -1;
                
                } 
                else
                {
                  update_health_monitor_sample_data(&hm_data->num_duplicate_vectors);
                  sprintf(err_msg, "Error: Duplicate vector name(%s) in a monitor whose graph definition file (GDF) is %s.\n",
                  (vector_row->flags & MON_BREADCRUMB_SET)?vector_row->mon_breadcrumb:vector_row->vector_name, cm_ptr->gdf_name); 
                  MLTL1(EL_DF, 0, 0, _FLN_, cm_info_mon_conn_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION, "Error: Duplicate vector name(%s) in a monitor whose graph definition file (GDF) is %s.\n", (vector_row->flags & MON_BREADCRUMB_SET)?vector_row->mon_breadcrumb:vector_row->vector_name, cm_ptr->gdf_name);
                  free_vec_lst_row(vector_row);
        	  cm_info_mon_conn_ptr->total_vectors--;

                  cm_info_mon_conn_ptr->flags &= ~ALL_VECTOR_DELETED;
                  if(!strcmp(cm_info_mon_conn_ptr->cs_ip, "127.0.0.1"))
                    return -1;
                  else
                  {
                    if(runtime_flag)  return -1;
                    else  NS_EXIT(-1,CAV_ERR_1060029,(vector_row->flags & MON_BREADCRUMB_SET)?vector_row->mon_breadcrumb:vector_row->vector_name, cm_ptr->gdf_name); 
                  }
                } 
              }
              } 
            } 
          } 
          else
          {
            if(gdf_match)
              break;
          }
          //TODO increase by no of monitors 
        }
      }

      if (global_settings->dynamic_cm_rt_table_mode)
      {
        //TODO MSR-> Implement Hashinsg if required.
      }
    }
  }

  cm_info_mon_conn_ptr->flags &= ~ALL_VECTOR_DELETED;
  if(runtime_flag)
  {
    set_rtg_index_in_cm_info(-1, (void *)cm_info_mon_conn_ptr, tmp_msg_data_size, FILL_RTG_INDEX_AT_RUNTIME);

    tmp_msg_data_size += cm_info_mon_conn_ptr->group_element_size[MAX_METRIC_PRIORITY_LEVELS];
    //vector_row->rtg_index = tmp_msg_data_size; // KEY-
    //tmp_msg_data_size += cm_info_mon_conn_ptr->group_element_size;
  }

  if(runtime_flag)
  {
    if(cm_info_mon_conn_ptr->new_vector_first_index < 0)
      cm_info_mon_conn_ptr->new_vector_first_index = vector_list_row;

    //if gdf_name is not NA_KUBER then we set the flag g_monitor_runtime_changes = 1
    if(strncmp(cm_info_mon_conn_ptr->gdf_name, "NA_",3))
    {
      g_monitor_runtime_changes = 1;
      g_vector_runtime_changes = 1;
      
    }
    else
      g_monitor_runtime_changes_NA_gdf = 1;
  }
  if ((g_tsdb_configuration_flag & TSDB_MODE ) && cm_info_mon_conn_ptr->gp_info_index <= 0 )
  {
    gdf_norm_id = nslb_get_norm_id(g_gdf_hash, cm_info_mon_conn_ptr->gdf_name, strlen(cm_info_mon_conn_ptr->gdf_name));
     //add_gdf_in_hash_table(gdf_name_only);
    if (gdf_norm_id >= 0 )
      cm_info_mon_conn_ptr->gp_info_index = gdf_norm_id;
    else
      g_gdf_processing_flag = PROCESS_REQUIRED;

      MLTL1(EL_DF, 0, 0, _FLN_, cm_info_mon_conn_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                                     "GDF_FILE inside add_vector Method %s gdf_norm_id %d g_gdf_processing_flag %d", cm_info_mon_conn_ptr->gdf_name, gdf_norm_id, g_gdf_processing_flag);
  }

  return 0;
}

void make_hidden_file(HiddenFileInfo *ptr, char *file_path , int switch_flag)
{
  FILE *fp = NULL;
  char buff_path[128]={0};
  char buff_path1[128]={0};
  char buff_path2[128]={0};
  char cur_partition_name[128]={0};
  sprintf(cur_partition_name, "%lld", g_partition_idx);
  sprintf(buff_path1,"%s/logs/%s/%s", g_ns_wdir, global_settings->tr_or_common_files, ptr->hidden_file_name);

  if(!switch_flag)
  { 
    create_csv_dir_and_link(cur_partition_name);
    sprintf(buff_path2,"%s/logs/%s/reports/csv/%s",g_ns_wdir, file_path, ptr->hidden_file_name);
    fp = fopen(buff_path1, "wr");
    if(fp != NULL )
    {
      ptr->is_file_created = 1;
      if(link(buff_path1, buff_path2))
        NSTL1(NULL, NULL, "Error in link error no %s buff path1 %s buff path %s", strerror(errno), buff_path1, buff_path);
    } 
    else 
    {
      NSDL1_PARENT(NULL, NULL, "Could not create file %s error %s", buff_path, strerror(errno));
      return ;
    }
    CLOSE_FP(fp);
  }
  else
  {
    sprintf(buff_path,"%s/logs/%s/reports/csv/%s",g_ns_wdir, file_path, ptr->hidden_file_name);
    ptr->is_file_created = 1;
    if(link(buff_path1, buff_path))
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                                     "Error in link error no %s buff path1 %s buff path %s", strerror(errno), buff_path1, buff_path);
  }
}


int initialize_and_malloc_cm_info_node_members(char * vector_name, int monitor_list_index, CM_info *cm_info_mon_conn_ptr, JSON_info *json_element_ptr, int runtime_flag, char *gdf_file, char *pod_name, char *pgm_path, int option, int print_mode, char *cs_ip, int access, char *rem_ip, char *rem_username, char *rem_password, char *pgm_args, char use_lps, char *server_name, char *origin_cmon, int dyn_cm, char *dest_file_name, char *err_msg, int max, int fd, ServerCptr server_mon_ptr, int server_index, char skip_breadcrumb_creation, char *gdf_name_only,int tier_idx)
{

  char *temp_ptr = NULL, *ptr[2] = {0};
  //int server_info_index = -1;
  char *hv_ptr = NULL;
  int num_data = 0, cs_port = 0, i = 0;
  char server_display_name[1024] = {0};
  char csv_filepath[1024] = {0};
  char file[128] = {0};
  time_t now = time(NULL);
  int mbean_mon_idx = -1;
  int norm_vector_id = 0;
  char backend_name[512] = {0};
  int length;
  int dbuf_len = 0;
  struct tm tm_struct;
  //ServerInfo *server_info = NULL;
  //server_info = (ServerInfo *) topolib_get_server_info();

  cm_info_mon_conn_ptr->num_group = -1;
  cm_info_mon_conn_ptr->con_type = NS_STRUCT_CUSTOM_MON;
  cm_info_mon_conn_ptr->conn_state = CM_INIT;
  cm_info_mon_conn_ptr->monitor_list_idx = monitor_list_index;
  cm_info_mon_conn_ptr->cm_retry_attempts = -1;
  cm_info_mon_conn_ptr->hb_conn_state = HB_DATA_CON_INIT;
  //for dvm cm mapping table
  cm_info_mon_conn_ptr->server_index = server_index;
  cm_info_mon_conn_ptr->no_log_OR_vector_format |= FORMAT_NOT_DEFINED;
  if(dyn_cm)
    cm_info_mon_conn_ptr->flags |= DYNAMIC_MONITOR;
  cm_info_mon_conn_ptr->tier_index=tier_idx;
  cm_info_mon_conn_ptr->reused_vec_parent_fd = -1;
  cm_info_mon_conn_ptr->fd = -1;
  cm_info_mon_conn_ptr->breadcrumb_level = -1;
  cm_info_mon_conn_ptr->mon_id = -1;
  cm_info_mon_conn_ptr->gp_info_index = -1;
  cm_info_mon_conn_ptr->dvm_cm_mapping_tbl_row_idx = -1;
  //Setting different value for backend monitor because we have to log only once.
  cm_info_mon_conn_ptr->flags |= LOG_ONCE;
  cm_info_mon_conn_ptr->new_vector_first_index = -1;
  cm_info_mon_conn_ptr->any_server = false;
  cm_info_mon_conn_ptr->dest_any_server = false;
   

  if(json_element_ptr)
  {
    if(json_element_ptr->vectorReplaceFrom)
    {
      MALLOC_AND_COPY(json_element_ptr->vectorReplaceFrom, cm_info_mon_conn_ptr->vectorReplaceFrom, (strlen(json_element_ptr->vectorReplaceFrom) + 1), "Copying elements from json file: vectorReplaceFrom", 0);
    }

    if(json_element_ptr->vectorReplaceTo)
    {
      MALLOC_AND_COPY(json_element_ptr->vectorReplaceTo, cm_info_mon_conn_ptr->vectorReplaceTo, (strlen(json_element_ptr->vectorReplaceTo) + 1), "Copying elements from json file vectorReplaceTo", 0);
    }

    if(json_element_ptr->config_json)
    {
      MALLOC_AND_COPY(json_element_ptr->config_json, cm_info_mon_conn_ptr->config_file, (strlen(json_element_ptr->config_json) + 1), "Copying elements from json file config json", 0);
    }
    if(json_element_ptr->namespace)
    {
      MALLOC_AND_COPY(json_element_ptr->namespace, cm_info_mon_conn_ptr->namespace, (strlen(json_element_ptr->namespace) + 1), "Copying element namespace from json namespace", 0);
    }
    if(json_element_ptr->instance_name)
    {
      MALLOC_AND_COPY(json_element_ptr->instance_name, cm_info_mon_conn_ptr->instance_name, (strlen(json_element_ptr->instance_name) + 1), "Copying elements from json instance name", 0);
    }
    if(json_element_ptr->cmon_pod_pattern)
    {
      MALLOC_AND_COPY(json_element_ptr->cmon_pod_pattern, cm_info_mon_conn_ptr->cmon_pod_pattern, (strlen(json_element_ptr->cmon_pod_pattern) + 1), "Copying cmon_pod_pattern in cm_info", 0);
    }
    else if(!(strcmp(gdf_file,"NA_KUBER")))
    {
       MALLOC_AND_COPY("cmon-", cm_info_mon_conn_ptr->cmon_pod_pattern, 6, "Copying cmon_pod_pattern in cm_info", 0);
    }
    else
      cm_info_mon_conn_ptr->cmon_pod_pattern = NULL;

    if(json_element_ptr->init_vector_file)
      cm_info_mon_conn_ptr->init_vector_file_flag = 1;

    if(json_element_ptr->vectorReplaceTo && json_element_ptr->vectorReplaceFrom)
    {
      MLTL2(EL_DF, 0, 0, _FLN_, cm_info_mon_conn_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
              " VectorReplaceFrom field is preset in JSON for gdf->%s, vectorReplaceFrom->%s, vectorReplaceTo->%s, tier_server_mapping_type->%d", gdf_file, json_element_ptr->vectorReplaceFrom, json_element_ptr->vectorReplaceTo, json_element_ptr->tier_server_mapping_type);     
    }
   
    
    cm_info_mon_conn_ptr->is_process = json_element_ptr->is_process;
    cm_info_mon_conn_ptr->tier_server_mapping_type = json_element_ptr->tier_server_mapping_type;
    cm_info_mon_conn_ptr->any_server = json_element_ptr->any_server;
    cm_info_mon_conn_ptr->dest_any_server = json_element_ptr->dest_any_server_flag;
    cm_info_mon_conn_ptr->gdf_flag = json_element_ptr->gdf_flag;
   
    if(global_settings->generic_mon_flag == 1) 
      cm_info_mon_conn_ptr->genericDb_gdf_flag = json_element_ptr->generic_gdf_flag;
    
 
    if(json_element_ptr->frequency != 0)
      cm_info_mon_conn_ptr->frequency = json_element_ptr->frequency; 
    else
      cm_info_mon_conn_ptr->frequency = global_settings->progress_secs;
     
    
    if(json_element_ptr->g_mon_id)
    {
      MALLOC_AND_COPY(json_element_ptr->g_mon_id, cm_info_mon_conn_ptr->g_mon_id, (strlen(json_element_ptr->g_mon_id) + 1), "g_mon_id in cm_info", 0);
    }
    else
    {
      MALLOC_AND_COPY("-1", cm_info_mon_conn_ptr->g_mon_id, 2, "g_mon_id in cm_info", 0); 
    }
    cm_info_mon_conn_ptr->mon_info_index = json_element_ptr->mon_info_index;
    cm_info_mon_conn_ptr->sm_mode = json_element_ptr->sm_mode;
  }
  //This is safety check if frequency comes 0 then set as global_settings->progress_secs 
  //In auto mon case it comes 0
  else
  {
    cm_info_mon_conn_ptr->frequency = global_settings->progress_secs;
    MALLOC_AND_COPY("-1", cm_info_mon_conn_ptr->g_mon_id, 2, "g_mon_id in cm_info", 0);
  }

  if(!global_settings->enable_hml_group_in_testrun_gdf)
    cm_info_mon_conn_ptr->metric_priority = MAX_METRIC_PRIORITY_LEVELS;
  else
    cm_info_mon_conn_ptr->metric_priority = g_metric_priority;

  if(cm_info_mon_conn_ptr->tier_server_mapping_type == 0)
    cm_info_mon_conn_ptr->tier_server_mapping_type |= STANDARD_TYPE;

  if(runtime_flag)
  {
    cm_info_mon_conn_ptr->flags |= RUNTIME_ADDED_MONITOR;
  }

  memset(ptr, 0, sizeof(ptr));  // To initize the ptr with NULL.
  char local_cs_ip[1024] = {0};
   
   strcpy(local_cs_ip, cs_ip); 
   i = get_tokens(local_cs_ip, ptr, ":", 2);
   if(ptr[0])
     strcpy(cs_ip, ptr[0]);
   if(ptr[1])
     cs_port = atoi(ptr[1]);

   if(cs_port == 0)
     cs_port = g_cmon_port;
  
  MALLOC_AND_COPY(gdf_file, cm_info_mon_conn_ptr->gdf_name, strlen(gdf_file) + 1, "Monitor gdf name", -1)

  //in gdf_name_only only gdf_name is present not with absolute path
  MALLOC_AND_COPY(gdf_name_only, cm_info_mon_conn_ptr->gdf_name_only, strlen(gdf_name_only) + 1, "Monitor gdf name only", -1)

  MALLOC_AND_COPY(vector_name, cm_info_mon_conn_ptr->monitor_name, strlen(vector_name) + 1, "Monitor name", -1)
  
  get_no_of_elements(cm_info_mon_conn_ptr, &num_data);
  cm_info_mon_conn_ptr->no_of_element = num_data;

  if(strncasecmp(cm_info_mon_conn_ptr->gdf_name, "NA", 2) != 0)
    get_breadcrumb_level(cm_info_mon_conn_ptr);

  if(skip_breadcrumb_creation == 1)
    cm_info_mon_conn_ptr->skip_breadcrumb_creation = 1;

  if(cm_info_mon_conn_ptr->flags & NV_MONITOR)
    cm_info_mon_conn_ptr->nv_data_header = 1;

  if(pod_name)
  {
    MY_MALLOC(cm_info_mon_conn_ptr->pod_name,strlen(pod_name)+1, "Allocating for pod_name", 0);
    strcpy(cm_info_mon_conn_ptr->pod_name, pod_name);
  }

  // For MBean CMON
  if(json_element_ptr && json_element_ptr->config_json && (json_element_ptr->agent_type & CONNECT_TO_CMON))
  {
    //json_element_ptr will be set if mbean monitor applied for CMON 
    if(json_element_ptr->mon_name)
      MALLOC_AND_COPY(json_element_ptr->mon_name, cm_info_mon_conn_ptr->mbean_monitor_name, strlen(json_element_ptr->mon_name) + 1,
    "Monitor name", -1)
    cm_info_mon_conn_ptr->monitor_type |= CMON_MBEAN_MONITOR;  
  }

  // For MBean BCI
  if(json_element_ptr && (json_element_ptr->agent_type & CONNECT_TO_NDC))
    mbean_mon_idx = search_mon_entry_in_list(cm_info_mon_conn_ptr->monitor_name);

  if(cm_info_mon_conn_ptr->server_index >= 0)
  {
  MY_MALLOC(cm_info_mon_conn_ptr->cs_ip, strlen(topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[cm_info_mon_conn_ptr->server_index].server_ptr->topo_servers_list->server_ip) + 1, "ServerIp in place of display name", -1);
    strcpy(cm_info_mon_conn_ptr->cs_ip, topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[cm_info_mon_conn_ptr->server_index].server_ptr->topo_servers_list->server_ip);
    MY_MALLOC(cm_info_mon_conn_ptr->server_name, strlen(topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[cm_info_mon_conn_ptr->server_index].server_disp_name) + 1, "Custom Monitor cs_ip", 0);
    strcpy(cm_info_mon_conn_ptr->server_name, topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[cm_info_mon_conn_ptr->server_index].server_disp_name);

  }
  else
  {
    if((hv_ptr = strchr(cs_ip, global_settings->hierarchical_view_vector_separator)) != NULL) //hierarchical view
    {
      *hv_ptr = '\0';
      cs_ip = hv_ptr + 1;
    }

    MY_MALLOC(cm_info_mon_conn_ptr->cs_ip, strlen(cs_ip) + 1, "Custom Monitor cs_ip", -1);
    strcpy(cm_info_mon_conn_ptr->cs_ip, cs_ip);
  }

  if((hv_ptr = strchr(cm_info_mon_conn_ptr->cs_ip, ':')))  //Resuing pointer
  {
    *hv_ptr = '\0';
    cs_port = atoi(hv_ptr + 1);
  }

  cm_info_mon_conn_ptr->cs_port = cs_port;

  if(strstr(cm_info_mon_conn_ptr->gdf_name, "cm_nv_") != NULL)
  {
    cm_info_mon_conn_ptr->flags |= NV_MONITOR;
    cm_info_mon_conn_ptr->cs_port = hpd_port;
     cs_port = hpd_port;
  }
  else if(!strcmp(cm_info_mon_conn_ptr->gdf_name, "cm_coherence_cluster"))
    cm_info_mon_conn_ptr->flags |= COHERENCE_CLUSTER;
  else if(!strcmp(cm_info_mon_conn_ptr->gdf_name, "NA_KUBER"))
       cm_info_mon_conn_ptr->flags |= NA_KUBER;    
  else if(!strcmp(cm_info_mon_conn_ptr->gdf_name, "cm_alert_log_stats.gdf"))
           cm_info_mon_conn_ptr->flags |= ALERT_LOG_MONITOR;
  else if((strstr(cm_info_mon_conn_ptr->gdf_name, "cm_coherence_service") != NULL) || (strstr(cm_info_mon_conn_ptr->gdf_name, "cm_coherence_storage") != NULL) || ((cm_info_mon_conn_ptr->flags & NEW_FORMAT) && (strstr(cm_info_mon_conn_ptr->gdf_name, "cm_coherence_cache") != NULL)))
     cm_info_mon_conn_ptr->flags |= COHERENCE_OTHERS;
  else if(strncmp(cm_info_mon_conn_ptr->gdf_name, "NA_wmi_peripheral_device_status.gdf", 35) == 0)
       cm_info_mon_conn_ptr->flags |=  NA_WMI_PERIPHERAL_MONITOR;
  else if(!strncmp(cm_info_mon_conn_ptr->gdf_name, "NA_", 3))  
        cm_info_mon_conn_ptr->flags |= NA_GDF;
  else if((!strncmp(cm_info_mon_conn_ptr->gdf_name,"NA_mssql_", 9)))
        cm_info_mon_conn_ptr->flags |= NA_MSQL;

  //for nd backend monitor, we need to fill some data
  if((global_settings->net_diagnostics_mode) &&(!strcmp(cm_info_mon_conn_ptr->cs_ip, global_settings->net_diagnostics_server)) && (cm_info_mon_conn_ptr->cs_port== global_settings->net_diagnostics_port) && (!strstr(cm_info_mon_conn_ptr->gdf_name, "cm_nd_db_query_stats.gdf")) && (!strstr(cm_info_mon_conn_ptr->gdf_name,"cm_nd_entry_point_stats.gdf")) && (!strstr(cm_info_mon_conn_ptr->gdf_name,"cm_nd_integration_point_status.gdf")))
  {
    cm_info_mon_conn_ptr->flags |= ND_MONITOR;
    if (json_element_ptr && (json_element_ptr->agent_type & CONNECT_TO_NDC))
    {
      //Option provided to Connect with CMON Or BCI
      //cm_info_mon_conn_ptr->monitor_type |= MBEAN_MONITOR;       
      cm_info_mon_conn_ptr->monitor_type &= ~CMON_MBEAN_MONITOR;  
      cm_info_mon_conn_ptr->monitor_type |= BCI_MBEAN_MONITOR;    
      cm_info_mon_conn_ptr->mbean_mon_idx = mbean_mon_idx;}
   
    //set ND percentile ptrs 
    set_or_get_nd_percentile_ptrs(cm_info_mon_conn_ptr);

    //allocate tables
    cm_info_mon_conn_ptr->instanceIdxMap = ns_init_map_table(NS_MAP_TBL);
    cm_info_mon_conn_ptr->tierIdxmap = ns_init_map_table(NS_MAP_TBL);
    cm_info_mon_conn_ptr->ndVectorIdxmap = ns_init_map_table(NS_MAP_TBL);

    //PASS CURRENT MALLOC SIZE TO ns_init_2d_matrix() TODO . take size in cm info struct
    ns_allocate_2d_matrix((void ***)&(cm_info_mon_conn_ptr->instanceVectorIdxTable), DELTA_MAP_TABLE_ENTRIES, DELTA_MAP_TABLE_ENTRIES, &(cm_info_mon_conn_ptr->cur_instIdx_InstanceVectorIdxTable), &(cm_info_mon_conn_ptr->cur_vecIdx_InstanceVectorIdxTable));
    ns_allocate_2d_matrix((void ***)&(cm_info_mon_conn_ptr->tierNormVectorIdxTable), DELTA_MAP_TABLE_ENTRIES, DELTA_MAP_TABLE_ENTRIES, &(cm_info_mon_conn_ptr->cur_normVecIdx_TierNormVectorIdxTable), &(cm_info_mon_conn_ptr->cur_tierIdx_TierNormVectorIdxTable));


    //initialize norm_id_table for ND monitors
    //nslb_init_norm_id_table(&cm_info_mon_conn_ptr->nd_norm_table, 128*1024);

    strcpy(backend_name, "cm_nd_agg_monitor");
    ns_normalize_monitor_vector(cm_info_mon_conn_ptr, backend_name, &norm_vector_id);
    //alocate dummy buffer in parent, need to take care of copying this buffer if this parent vector is deleted at 
    //runtime.
    MY_MALLOC_AND_MEMSET(cm_info_mon_conn_ptr->dummy_data, (num_data * sizeof(double)), "dummy data allocation", -1);

    if (strstr(cm_info_mon_conn_ptr->gdf_name, "cm_nd_bt.gdf"))
    {
      cm_info_mon_conn_ptr->flags |= ND_BT_GDF;
    }
    if (strstr(cm_info_mon_conn_ptr->gdf_name, "cm_nd_backend_call_stats.gdf"))
    {
      cm_info_mon_conn_ptr->flags |= ND_BACKEND_CALL_STATS_GDF;
    }

  }
  else if(cm_info_mon_conn_ptr->flags & NA_WMI_PERIPHERAL_MONITOR)
  {
    MY_MALLOC_AND_MEMSET(cm_info_mon_conn_ptr->csv_stats_ptr, (sizeof(CSVStats)), "CSV Structure", monitor_list_index);

    strftime(file, 100, "%Y%m%d", nslb_localtime(&now, &tm_struct, 1));

    cm_info_mon_conn_ptr->csv_stats_ptr->date_format = atoi(file);
    length = sprintf(csv_filepath, "%s/logs/TR%d/device_status_report/status_report_%s", g_ns_wdir, testidx, file);
    mkdir_ex(csv_filepath);
    MALLOC_AND_COPY(csv_filepath, cm_info_mon_conn_ptr->csv_stats_ptr->csv_file, (length+1), "Wmi Periphal Device status csv", monitor_list_index);

  }
  else if(strstr(cm_info_mon_conn_ptr->gdf_name, "ns_ip_data.gdf"))
  {
    if(nc_ip_data_mon_idx == -1)
    {
      nc_ip_data_mon_idx = cm_info_mon_conn_ptr->monitor_list_idx;

      //allocate 2-d table row
      MY_MALLOC_AND_MEMSET(genVectorPtr, (sgrp_used_genrator_entries * sizeof(void *)), "GeneratorVectorIndex row allocation", -1);
      for(i = 0; i < sgrp_used_genrator_entries; i++)
      {
        MY_MALLOC_AND_MEMSET(genVectorPtr[i], (DELTA_CUSTOM_MONTABLE_ENTRIES * sizeof(GenVectorIdx)), "GeneratorVectorIndex Column allocation. Default allocation for 5 IP's", -1);
        genVectorPtr[i][0].max_vectors += DELTA_CUSTOM_MONTABLE_ENTRIES;
      }
    }
    else
    {
      MLTL1(EL_DF, 0, 0, _FLN_, cm_info_mon_conn_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
              " Adding NC monitor for the first time and nc_ip_data_mon_idx(%d) should be negative.", nc_ip_data_mon_idx);
      return -1;
    }
  }
  else if(cm_info_mon_conn_ptr->flags & NV_MONITOR)
  {
    //We donot want to create dvm_mapping table for NV monitors. Hence this check.
  }
  else if(strncmp(cm_info_mon_conn_ptr->gdf_name, "NA_genericDB_", 13) == 0)
  {
    cm_info_mon_conn_ptr->gdf_flag = NA_GENERIC_DB;
    //send msg to lps in case of generic DB monitors
    if(runtime_flag && !is_mssql_monitor_applied)
    {
       is_mssql_monitor_applied = 1; 
       //create_mssql_stats_hidden_file();
    }
    is_mssql_monitor_applied = 1;    
  }
  else
  {
    //dvm_cm_mapping row allocation and set in cm_info. Earlier this code was while adding vectors.
    if(max_mapping_tbl_row_entries <= total_mapping_tbl_row_entries)
    {
      create_mapping_table_row_entries();
 
      MLTL3(EL_DF, 0, 0, _FLN_, cm_info_mon_conn_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
              "max_mapping_tbl_row_entries = %d, total_mapping_tbl_row_entries = %d, total_dummy_dvm_mapping_tbl_row_entries = %d", max_mapping_tbl_row_entries, total_mapping_tbl_row_entries, total_dummy_dvm_mapping_tbl_row_entries);
    }
    cm_info_mon_conn_ptr->dvm_cm_mapping_tbl_row_idx = total_mapping_tbl_row_entries;
    total_mapping_tbl_row_entries++;
  }

  if(dyn_cm)
  {
    if(cm_info_mon_conn_ptr->flags & ND_MONITOR)
      nslb_init_norm_id_table(&cm_info_mon_conn_ptr->nd_norm_table, 128*1024);  //initialize norm_id_table
    else
    {
      for(int idx=0; idx < total_vector_gdf_hash_entries; idx++)
      {
        if(!(strcmp(vector_gdf_hash[idx].gdf_name, cm_info_mon_conn_ptr->gdf_name_only)))
        {
            cm_info_mon_conn_ptr->hash_size = vector_gdf_hash[idx].key_size;
            nslb_init_norm_id_table(&cm_info_mon_conn_ptr->nd_norm_table, cm_info_mon_conn_ptr->hash_size);

            MLTL3(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                                         "Going to create vector_norm_table at start test. mon_name = %s, hash size = %d, gdf_name=%s",                                                                            cm_info_mon_conn_ptr->monitor_name, cm_info_mon_conn_ptr->hash_size, cm_info_mon_conn_ptr->gdf_name_only);
          break;
        }
      }
    }
  }


  MY_MALLOC(cm_info_mon_conn_ptr->pgm_path, strlen(pgm_path) + 1, "Custom Monitor Path", 0);
  strcpy(cm_info_mon_conn_ptr->pgm_path, pgm_path);

  cm_info_mon_conn_ptr->option = option;
  cm_info_mon_conn_ptr->print_mode = print_mode;

  //This check has been added because server_index is set -ve in case of NC.
  if(server_index != -1)
  {
    if((temp_ptr = strchr(topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_index].server_ptr->server_name, ':')))
    {
      *temp_ptr = '\0';
      cs_port = atoi(temp_ptr + 1);
      *temp_ptr = ':';
    }

    /*MY_MALLOC(cm_info_mon_conn_ptr->cs_ip, strlen(server_info[cm_info_mon_conn_ptr->server_index].server_ip) + 1, "ServerIp in place of display name", -1);
    strcpy(cm_info_mon_conn_ptr->cs_ip, server_info[cm_info_mon_conn_ptr->server_index].server_ip);
    MY_MALLOC(cm_info_mon_conn_ptr->server_name, strlen(server_info[cm_info_mon_conn_ptr->server_index].server_disp_name) + 1, "Custom Monitor cs_ip", 0);
    strcpy(cm_info_mon_conn_ptr->server_name, server_info[cm_info_mon_conn_ptr->server_index].server_disp_name);*/

    //In two cases topo_server_idx is set -ve. We make an entry for 127.0.0.1:7892 when we parse NET_DIAGNOSTICS_SERVER keyword. Its topo_server_idx is -2. And another case is when we add 127.0.0.1:7891. It is added when topology is not defined and we make a default entry in topology with topo_server_idx -1.
    if(cm_info_mon_conn_ptr->server_index >= 0)
    {
      MY_MALLOC(cm_info_mon_conn_ptr->tier_name, strlen(topo_info[topo_idx].topo_tier_info[tier_idx].tier_name) + 1, "Tier Name", 0);
      strcpy(cm_info_mon_conn_ptr->tier_name, topo_info[topo_idx].topo_tier_info[tier_idx].tier_name);
    }
  }
  else
  {

    if((hv_ptr = strchr(cs_ip, global_settings->hierarchical_view_vector_separator)) != NULL) //hierarchical view
    {
      *hv_ptr = '\0';
      cs_ip = hv_ptr + 1;
    }
    
    MY_MALLOC(cm_info_mon_conn_ptr->cs_ip, strlen(cs_ip) + 1, "Custom Monitor cs_ip", 0);
    strcpy(cm_info_mon_conn_ptr->cs_ip, cs_ip);  
  }
  cm_info_mon_conn_ptr->cs_port = cs_port;
  cm_info_mon_conn_ptr->access = access;

  //get and fill server display name 
  if(cm_info_mon_conn_ptr->server_index < 0)
  {
    //get and fill server display name 
   MALLOC_AND_COPY(topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_index].server_disp_name, cm_info_mon_conn_ptr->server_name, strlen(server_display_name) + 1, "server namefor NC case", -1);
  }
  else
  {
   if(cm_info_mon_conn_ptr->server_index >= 0)
    strcpy(server_display_name,topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[cm_info_mon_conn_ptr->server_index].server_disp_name);
    else 
      strcpy(server_display_name,cm_info_mon_conn_ptr->server_name);

  }

  MY_MALLOC(cm_info_mon_conn_ptr->server_display_name, strlen(server_display_name) + 1, "Custom Monitor server_display_name", 0);
  strcpy(cm_info_mon_conn_ptr->server_display_name, server_display_name);

  MY_MALLOC(cm_info_mon_conn_ptr->cavmon_ip, strlen(cm_info_mon_conn_ptr->cs_ip) + 1, "Custom Monitor cavmon_ip", 0);
  strcpy(cm_info_mon_conn_ptr->cavmon_ip, cm_info_mon_conn_ptr->cs_ip);
  cm_info_mon_conn_ptr->cavmon_port = cm_info_mon_conn_ptr->cs_port;

// We are allocate below values even for local access option so that we dont have to validate these in init_server().
  MY_MALLOC(cm_info_mon_conn_ptr->rem_ip, strlen(rem_ip) + 1, "Custom Monitor rem_ip", 0);
  strcpy(cm_info_mon_conn_ptr->rem_ip, rem_ip);

  MY_MALLOC(cm_info_mon_conn_ptr->rem_username, strlen(rem_username) + 1, "Custom Monitor rem_username", 0);
  strcpy(cm_info_mon_conn_ptr->rem_username, rem_username);

  MY_MALLOC(cm_info_mon_conn_ptr->rem_password, strlen(rem_password) + 1, "Custom Monitor rem_password", 0);
  strcpy(cm_info_mon_conn_ptr->rem_password, rem_password);

  if(use_lps)
    cm_info_mon_conn_ptr->flags |= USE_LPS;

  MY_MALLOC(cm_info_mon_conn_ptr->pgm_args, strlen(pgm_args) + 1, "Custom Monitor args", 0);
  strcpy(cm_info_mon_conn_ptr->pgm_args, pgm_args);

  if(cm_info_mon_conn_ptr->flags & COHERENCE_CLUSTER)
  {
    total_no_of_coherence_cluster_mon++;

    if(strstr(cm_info_mon_conn_ptr->pgm_args, "-useMemberName 0") == NULL)
      cm_info_mon_conn_ptr->flags |= NEW_FORMAT;
  }

  if(origin_cmon[0] != '\0')
  {
    NSDL3_MON(NULL, NULL, "Added custom monitor  .. origin_cmon NOT NOLL ... malloc.");
    //For Heruku
    MY_MALLOC(cm_info_mon_conn_ptr->origin_cmon, strlen(origin_cmon) + 1, "Custom Monitor Origin Cmon Name", 0);
    strcpy(cm_info_mon_conn_ptr->origin_cmon, origin_cmon);
  }

  //Destination File Name is used in case of Get Log File Monitor.
  if(cm_info_mon_conn_ptr->gdf_flag == CM_GET_LOG_FILE)
  {
    MY_MALLOC(cm_info_mon_conn_ptr->dest_file_name, strlen(dest_file_name) + 1, "Destination File Name", 0);
    strcpy(cm_info_mon_conn_ptr->dest_file_name, dest_file_name);
  }

  if(cm_info_mon_conn_ptr ->gdf_flag ==ORACLE_STATS_GDF)
  {
    char buf[1024] = "";

    //In case of SQL_REPORT metric, gdf name will be 'NA' & we need to set this flag only for SQL_REPORT
    if(!strncmp(cm_info_mon_conn_ptr->gdf_name, "NA", 2))
    {
      cm_info_mon_conn_ptr->flags |= ORACLE_STATS;
    }

    //in this dir, oracle html files are dumped.
    //creating this dir here because LPS creates these with root owner, and nsu_rm_trun fails to remove these
    sprintf(buf, "%s/logs/%s/server_logs/oracle_reports/%s/", g_ns_wdir, global_settings->tr_or_partition, cm_info_mon_conn_ptr->cs_ip);
    NSDL1_MON(NULL, NULL, "buf is [%s]", buf);
    if(mkdir_ex(buf) == 0) //error case
    {
      NSDL1_MON(NULL, NULL, "Error: Could not create dir %s, error is %s", buf, nslb_strerror(errno));
      sprintf(err_msg, "Error: Could not create dir %s, error is %s\n", buf, nslb_strerror(errno));
      if(!runtime_flag)
        NSTL1_OUT(NULL, NULL, "%s", err_msg);
    }
  } 

  //for explicit new vectors found on runtime
  if(fd != -1)
    cm_info_mon_conn_ptr->fd = fd;
  else
    cm_info_mon_conn_ptr->fd = -1;

  cm_info_mon_conn_ptr->num_dynamic_filled = 0;

  if(runtime_flag)
  {
    NSDL2_MON(NULL, NULL, "Set rtg_index for monitor '%s', tmp_msg_data_size = %d, "
                           "dyn_cm = %d, cus_mon_row = %d, group_element_size = %d",
                            cm_info_mon_conn_ptr->gdf_name, tmp_msg_data_size, dyn_cm, monitor_list_index,
                            cm_info_mon_conn_ptr->group_element_size);

    if(cm_info_mon_conn_ptr->no_of_element <= 0)
    {
      get_no_of_elements(cm_info_mon_conn_ptr, &num_data);
      cm_info_mon_conn_ptr->no_of_element = num_data;
    }

    //tmp_msg_data_size += cm_info_mon_conn_ptr->group_element_size;
  }  
 
  //is_norm_init_done is reused  
  if(!strncmp(cm_info_mon_conn_ptr->pgm_path, "cm_monitor_registration", 18))
  {
    cm_info_mon_conn_ptr->is_monitor_registered = MONITOR_REGISTRATION;
    if((temp_ptr = strchr(cm_info_mon_conn_ptr->pgm_path, ';')) != NULL)
    {
      *temp_ptr = '\0';
      MALLOC_AND_COPY(ptr + 1, cm_info_mon_conn_ptr->component_name, (strlen(temp_ptr + 1) + 1), "Component name", 0);
    }
  }

  if(is_outbound_connection_enabled)
  {
    if(!(cm_info_mon_conn_ptr->flags & ND_MONITOR) && !(cm_info_mon_conn_ptr->flags & USE_LPS)
        && (cm_info_mon_conn_ptr->is_monitor_registered != MONITOR_REGISTRATION))
            cm_info_mon_conn_ptr->flags |= OUTBOUND_ENABLED;
            
     cm_info_mon_conn_ptr->mon_id = g_mon_id;

    //Allocate entry in monitor_config structure of server structure to store monitor init message buffer.
    add_entry_in_monitor_config(cm_info_mon_conn_ptr->server_index,cm_info_mon_conn_ptr->tier_index);
    add_entry_in_mon_id_map_table();
    mon_id_map_table[g_mon_id].retry_count = 0;    
  }

  // if ((cm_info_mon_conn_ptr->flags & OUTBOUND_ENABLED) && (json_element_ptr && !(json_element_ptr->agent_type & CONNECT_TO_NDC)))
  // cm_info_mon_conn_ptr->mon_id = g_mon_id;
  
  //We set dbuf_len to a big size because there may come a big error message from custom monitor
  if(cm_info_mon_conn_ptr->data_buf == NULL)
  {
    if(cm_info_mon_conn_ptr->gdf_flag == NA_GENERIC_DB)
      dbuf_len = g_mssql_data_buf_size + 1;
    else
      dbuf_len = MAX_MONITOR_BUFFER_SIZE + 1;
    MY_MALLOC(cm_info_mon_conn_ptr->data_buf, dbuf_len, "Custom Monitor data", monitor_list_index);
    memset(cm_info_mon_conn_ptr->data_buf, 0, dbuf_len);
    MLTL1(EL_DF, DM_METHOD, MM_MON, _FLN_, cm_info_mon_conn_ptr, EID_DATAMON_GENERAL, EVENT_INFO,
					 "maxlen %d cus_mon_ptr->gdf_name %s", dbuf_len, cm_info_mon_conn_ptr->gdf_name); 
  }

  if (is_outbound_connection_enabled)
  {
    server_index = cm_info_mon_conn_ptr->server_index;
     
    if(cm_info_mon_conn_ptr->flags & OUTBOUND_ENABLED)
    { 
      make_send_msg_for_monitors(cm_info_mon_conn_ptr, server_index, json_element_ptr); 
    } 

    //if (status = 0), monitor is being applied on an inactive server. Its message will be sent at node discovery.
    if (server_index >= 0)
    {
      if(topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_index].server_ptr->status == 1)
        mon_id_map_table[g_mon_id].state = INIT_MONITOR;
      else
        mon_id_map_table[g_mon_id].state = INACTIVE_MONITOR;
    }

    //We will only increase runtime_monitor_offset for newly added monitors.
    if(!runtime_flag)
      topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[cm_info_mon_conn_ptr->server_index].server_ptr->topo_servers_list->runtime_monitor_offset ++;
  }

  return 0;
}

int add_cm_info_node(char *vector_name, char* pgm_path, char* gdf_name, int run_options, int print_mode, char *cs_ip, int access, char *rem_ip, char* rem_username, char* rem_password, char* pgm_args, int use_lps, char* server_name, int runtime_flag, char* err_msg, int is_dynamic, char* origin_cmon, int fd, ServerCptr server_mon_ptr, char* breadcrumb, int server_index, char* pod_name, JSON_info *json_element_ptr, int max, char *dest_file_name, char skip_breadcrumb_creation, char* gdf_name_only,int tier_idx)
{
  int monitor_list_row = -1;
  CM_info *cm_info_node = NULL;

  //we are initializing the cm_info_node firstly then creating entry in monitor list because thread implementation was creating chaos 
  MY_MALLOC_AND_MEMSET(cm_info_node, (sizeof(CM_info )), "cm_info_node", -1);
  if(initialize_and_malloc_cm_info_node_members(vector_name, -1, cm_info_node, json_element_ptr, runtime_flag, gdf_name, pod_name, pgm_path, run_options, print_mode, cs_ip, access, rem_ip, rem_username, rem_password, pgm_args, use_lps, server_name, origin_cmon, is_dynamic, dest_file_name, err_msg, max, fd, server_mon_ptr, server_index, skip_breadcrumb_creation, gdf_name_only,tier_idx) < 0)
  {
   //TODO MSR->Error Handling: Make function of free for both cm_info node and members of it
    if(cm_info_node != NULL)
      free_cm_info_node(cm_info_node, monitor_list_row + 1);

    topo_info[topo_idx].topo_tier_info[cm_info_node->tier_index].topo_server[cm_info_node->server_index].server_ptr->topo_servers_list->cmon_monitor_count--;
    return -1;
  }
  
  if(create_table_entry_ex(&monitor_list_row, &total_monitor_list_entries, &max_monitor_list_entries, (char **)&monitor_list_ptr, sizeof(Monitor_list), "Monitor List Table") == -1)
  {
    CM_RUNTIME_RETURN_OR_EXIT(runtime_flag, CAV_ERR_1060066, 1,tier_idx,server_index);
  }

  //This is the case when when ENABLE_AUTO_JSON_MONITOR mode 2 is on i.e directory name is given of json files.
  //here we have to handle to cases also
  //if server is autoscaled then we have to increase the server count by 1.
  if(json_element_ptr && total_mon_config_list_entries > 0)
  {
    mj_increase_server_count(json_element_ptr->mon_info_index);
  }

  monitor_list_row = total_monitor_list_entries;

  cm_info_node->monitor_list_idx = monitor_list_row;

  monitor_list_ptr[monitor_list_row].is_dynamic = is_dynamic;  //will be set on the basis of flag passed
  
  monitor_list_ptr[monitor_list_row].cm_info_mon_conn_ptr = cm_info_node;    
 
  MALLOC_AND_COPY(gdf_name, monitor_list_ptr[monitor_list_row].gdf_name,(strlen(gdf_name) + 1), "GDF name", monitor_list_row);

  total_monitor_list_entries++;

  return monitor_list_row;
}  



int add_custom_dynamic_monitor(char *vector_name, char *pgm_path, char *gdf_name, int option, int print_mode, char *cs_ip, int access, char *rem_ip, char *rem_username, char *rem_password, char *pgm_args, char use_lps, char *server_name, int runtime_flag, char *err_msg, int dyn_cm, char *origin_cmon, char *buffer, int max, int fd, ServerCptr server_mon_ptr, char *breadcrumb, int is_new_format, id_map_table_t *ins_map_ptr,  id_map_table_t *tier_map_ptr, id_map_table_t *vector_map_ptr, CM_info *cm_info_mon_conn_ptr, unsigned int *max_mapping_tbl_vectors_entries, int server_index, char *pod_name, JSON_info *json_element_ptr, char skip_breadcrumb_creation,int tier_idx)
{
  char gdf_file[1024] = "";
  int i = 0;
  char dest_file_name[1024]="";
  char csIp[1024]; //to take cs_ip
  char *field[2] = {0};
  int csPort = 7891;
  char local_cs_ip[1024] = {0};
  int monitor_list_index = -1;
  int ret = 0;
  //int specific_server_new_flag=0; 
  char tmp_buf[MAX_DATA_LINE_LENGTH];
  CM_vector_info *vector_list = NULL;
  //if(tier_idx>=0)
    //cm_info_mon_conn_ptr->tier_index=tier_idx; 

  //ServerInfo *server_info = NULL;
  //server_info = (ServerInfo *) topolib_get_server_info();
  //To extract cs_ip
  strcpy(csIp,cs_ip);

  //Bug :78308
  //Fixed core:parsing ND monitors keywords when server is not received from topology
  if(server_index == -1)
  {
      tier_idx = 0;
  }
  if(server_index != -1)
    strcpy(csIp,topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_index].server_ptr->topo_servers_list->server_ip);


  strcpy(local_cs_ip, csIp);
  get_tokens(local_cs_ip, field, ":", 2);
  if(field[0])
    strcpy(csIp,field[0]);
  if(field[1])
    csPort = atoi(field[1]);

  NSDL2_MON(NULL, NULL, "Method called");

  
  if(json_element_ptr && json_element_ptr->any_server)
  {
    //This is the case when ENABLE_AUTO_JSON_MONITOR mode 2 is on.
    if(total_mon_config_list_entries > 0)
    {
      if(json_element_ptr->instance_name == NULL)
      sprintf(tmp_buf, "%s%c%s%c%s", gdf_name, global_settings->hierarchical_view_vector_separator,topo_info[topo_idx].topo_tier_info[tier_idx].tier_name,global_settings->hierarchical_view_vector_separator,json_element_ptr->g_mon_id);
      
      else
        sprintf(tmp_buf, "%s%c%s%c%s%c%s", gdf_name, global_settings->hierarchical_view_vector_separator,json_element_ptr->instance_name, global_settings->hierarchical_view_vector_separator,topo_info[topo_idx].topo_tier_info[tier_idx].tier_name,global_settings->hierarchical_view_vector_separator, json_element_ptr->g_mon_id);
    }
    else
    {
      if(json_element_ptr->instance_name == NULL)
         sprintf(tmp_buf, "%s%c%s", gdf_name, global_settings->hierarchical_view_vector_separator, topo_info[topo_idx].topo_tier_info[tier_idx].tier_name);
      else
         sprintf(tmp_buf, "%s%c%s%c%s", gdf_name, global_settings->hierarchical_view_vector_separator, json_element_ptr->instance_name, global_settings->hierarchical_view_vector_separator, topo_info[topo_idx].topo_tier_info[tier_idx].tier_name);
    }

    NSDL1_MON(NULL, NULL, "Monitor tmp_buf '%s' for making entry in hash table for server '%s'\n", tmp_buf, topo_info[topo_idx].topo_tier_info[tier_idx].tier_name);

    if(init_and_check_if_mon_applied(tmp_buf))
    {
      sprintf(err_msg, "Monitor tmp_buf '%s' can not be applied on server %s as it is already applied on any other server of tier '%s'", tmp_buf, topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_index].server_disp_name,topo_info[topo_idx].topo_tier_info[tier_idx].tier_name );
    
      NSDL1_MON(NULL, NULL, "%s\n", err_msg);

      return -1;
    }
  }
  //TODO PJTSDB:Set bit
  //Here we are creating destination file for Get Log File Monitor.
   if((json_element_ptr != NULL) && (json_element_ptr->gdf_flag == CM_GET_LOG_FILE))
  {
    create_getFileMonitor_dest_file(pgm_path, pgm_args, vector_name, csIp, dest_file_name, err_msg);
  }
 
  //TODO PJTSDB:Set Bit
  /* Because in mprof the gdf name is like NA_YYYYMMDD_HHMMSS
   * So we compair first 3 bit NA_ and put GDF NAME NA */
   if (!strncmp(gdf_name, "NA_", 3))
   {

    if(!strncmp(gdf_name, "NA_SR", 5)) // for oracle awr stats monitor with metrics SQL_REPORT
      sprintf(gdf_file, "%s", "NA_SR");
     else if ((strchr(gdf_name, '/') != NULL) || (!strncmp(gdf_name, "NA_genericDB_", 13)) || (!strncmp(gdf_name, "NA_KUBER", 8)) || (!strncmp(gdf_name, "NA_wmi_", 7)))   //special handling for MSSQL, as it only has to write csv.

       sprintf(gdf_file, "%s", gdf_name);

     else if(!strncmp(gdf_name, "NA", 2))// for cm_get_log file monitor
      sprintf(gdf_file, "%s",gdf_name);
   }
     else
       sprintf(gdf_file, "%s/sys/%s", g_ns_wdir, gdf_name);
   

  /* Check if CM names for similar GDFs is not same excluding Get Log File Monitor;
   *  because we use dummy gdf in case of Log File Monitor */

  /* In hierarchical_view, for DVM we have to ignore this 'msg & exit' because here for same gdf vector name can be same beacuse of 
          //else if(flags -s waiting)
         // {
                 
         // }
   * NoPrefix option. 
   * For example: mpstat vector list on 66 -> CPU0, CPU1, CPU2 
   *              and mpstat vector list on 65 -> CPU0, CPU1, CPU2 */
 if(!(g_tsdb_configuration_flag & TSDB_MODE))
 {
  if(!strstr(pgm_path, "cm_get_log_file") && (!strstr(pgm_path, "cm_oracle_stats")))
  {
    for (i = 0; i < total_monitor_list_entries ; i++)
    {
      if (!(dyn_cm) && (strcmp(monitor_list_ptr[i].gdf_name, gdf_file) == 0))
      {
        monitor_list_index = i;
        if(!strcmp(vector_name, (monitor_list_ptr[monitor_list_index].cm_info_mon_conn_ptr)->monitor_name))
        {
          cm_info_mon_conn_ptr = monitor_list_ptr[monitor_list_index].cm_info_mon_conn_ptr;
          vector_list = (monitor_list_ptr[monitor_list_index].cm_info_mon_conn_ptr)->vector_list;
          if(cm_info_mon_conn_ptr->conn_state == CM_DELETED)
          {
            MLTL1(EL_DF, 0, 0, _FLN_, cm_info_mon_conn_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION, "Deleted custom monitor whose monitor name is (%s) has been added successfully\n", cm_info_mon_conn_ptr->monitor_name);
            if(server_index >= 0)
              set_reused_vector_counters(vector_list, 0, cm_info_mon_conn_ptr, topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_index].server_ptr->topo_servers_list->server_ip, csPort, pgm_path, pgm_args, MON_REUSED_NORMALLY);
            else
              set_reused_vector_counters(vector_list, 0, cm_info_mon_conn_ptr, csIp, csPort, pgm_path, pgm_args, MON_REUSED_NORMALLY);

            
            if(json_element_ptr)
            {
              cm_info_mon_conn_ptr->any_server = json_element_ptr->any_server;
              cm_info_mon_conn_ptr->dest_any_server = json_element_ptr->dest_any_server_flag;
            }

            cm_info_mon_conn_ptr->server_index = server_index;
           // if(cm_info_mon_conn_ptr->flags & OUTBOUND_ENABLED)
              // handle_reused_entry_for_CM(vector_list, 0, server_index);  //TODO MSR->Replace old function defination with new one 
             MLTL3(EL_DF, 0, 0, _FLN_, cm_info_mon_conn_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION, "calling g_vector_runtime_chnage %s", cm_info_mon_conn_ptr->gdf_name);
            g_vector_runtime_changes = 1;
            MLTL1(EL_DF, 0, 0, _FLN_, cm_info_mon_conn_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
                 " add_custom_dynamic %d", g_vector_runtime_changes);
           
            check_allocation_for_reused(cm_info_mon_conn_ptr); 
            cm_info_mon_conn_ptr->reused_vector[cm_info_mon_conn_ptr->total_reused_vectors] = 0;
            cm_info_mon_conn_ptr->total_reused_vectors++;
            
            if( (is_outbound_connection_enabled) && cm_info_mon_conn_ptr->mon_id >= 0 )
            {
              make_send_msg_for_monitors(cm_info_mon_conn_ptr, server_index, NULL);
              mon_id_map_table[cm_info_mon_conn_ptr->mon_id].state = INIT_MONITOR;  //this is done for any-server switching
            }

            //return DUPLICATE_MON;
              return 0;
          }
          else
          {
            sprintf(err_msg, CAV_ERR_1060065, vector_name, gdf_file);
            MLTL1(EL_DF, 0, 0, _FLN_, monitor_list_ptr[monitor_list_index].cm_info_mon_conn_ptr, EID_DATAMON_INV_DATA, EVENT_INFORMATION, "Error: Duplicate monitor name(%s) whose graph definition file (GDF) is %s.\n", vector_name, gdf_file);
            if(!strcmp(cm_info_mon_conn_ptr->cs_ip, "127.0.0.1"))
            {
              CM_RUNTIME_RETURN_OR_EXIT(1, err_msg, 0,-1,-1);
            }
            else
            {
              CM_RUNTIME_RETURN_OR_EXIT(runtime_flag, err_msg, 0,-1,-1);
            }
          }
        }
      }
    }
   }
 }


  monitor_list_index = add_cm_info_node(vector_name, pgm_path, gdf_file, option, print_mode, cs_ip, access, rem_ip, rem_username, rem_password, pgm_args, use_lps, server_name, runtime_flag, err_msg, dyn_cm, origin_cmon, fd, server_mon_ptr, breadcrumb, server_index, pod_name, json_element_ptr, max, dest_file_name, skip_breadcrumb_creation, gdf_name,tier_idx);

  if(monitor_list_index < 0)
    return -1;
  else
  {
   if(strncmp(monitor_list_ptr[monitor_list_index].gdf_name, "NA_",3))
    g_monitor_runtime_changes = 1;
   else
    g_monitor_runtime_changes_NA_gdf = 1;

   new_monitor_first_idx = monitor_list_index;
  }

  if(!dyn_cm)
  {
    ret = add_cm_vector_info_node(monitor_list_index, vector_name, buffer, max, breadcrumb, runtime_flag, dyn_cm);
    if(ret < 0)
    {
      free_mon_lst_row(monitor_list_index);
    }
  }
  else
  {
    monitor_list_ptr[monitor_list_index].cm_info_mon_conn_ptr->flags |= ALL_VECTOR_DELETED;
    
    //if(json_element_ptr && json_element_ptr->init_vector_file)
      //parse_init_vectors_file( monitor_list_index, json_element_ptr->init_vector_file, breadcrumb, runtime_flag);
     
  }

  return ret;
}
void extract_prgms_args(char *custom_buf, char *buf , int num)
{
  char *ptr = custom_buf;
  int count = 0;
  while(*ptr != '\0')
  {
    if(*ptr == ' ')
      count++;

    if(count > num)
    {
      break;
    }
    ptr++;
  }
  while (*ptr == ' ')
    ptr++;
  if(ptr != NULL)
    strcpy(buf, ptr);
}

// process the Monitor keyword line.
int custom_monitor_config(char *keyword, char *buf, char *server_name, int dyn_cm, int runtime_flag, char *err_msg, char *pod_name, JSON_info *json_element_ptr, char skip_breadcrumb_creation)
{

  #define MAX_TEMP_BUF 2048
  char *buffer = NULL;
  int max = 0;
  int fd = -1;
  char *breadcrumb = NULL;
  int is_new_format = 0;
  id_map_table_t *ins_map_ptr = NULL;
  id_map_table_t *tier_map_ptr = NULL;
  id_map_table_t *vector_map_ptr = NULL;
  unsigned int *max_mapping_tbl_vectors_entries = NULL;
  char key[1024] = "";
  char pgm_args[MAX_MONITOR_BUFFER_SIZE] = "";   // Program arguments if any
  char *token_arr[2000];  // will contain the array of all arguments
  char vector_name[1024] = ""; // Custom Monitor name (for info only)
  char pgm_path[1024] = ""; // program name with or without path
  char gdf_name[1024] = "";
  char origin_cmon_and_ip_port[1024] = "NO";
  char cs_ip[1024] = "NO";
  char rem_ip[1024] = "NA";
  char rem_username[256] = "NA";
  char rem_password[256] = "NA";
  char origin_cmon[1024] = {0}; //For Heruku
  int print_mode = PRINT_PERIODIC_MODE;
  int option = RUN_EVERY_TIME_OPTION;
  int access = RUN_LOCAL_ACCESS;
//storing satrt_testidx in testidx for test monitoring purpose
  int testidx = start_testidx;
  int num = 0;
  int i=0, j = 0;               //just for storing the number of tokens
  char new_buf[MAX_TEMP_BUF + 1]="";
  char to_change[MAX_TEMP_BUF + 1]="";
  ServerCptr server_mon_ptr;   //arun 
  char use_lps = 0;
  int server_index = -1;
  int vector_name_len =0;
  int tier_idx;
  NSDL2_MON(NULL, NULL, "Method called, keyword = %s, buf = %s, server_name = %s", keyword, buf, server_name);
  //if (strcasecmp(keyword, "CUSTOM_MONITOR") != 0) return;

  // Last field to_change is used to detect is there are any args
  num = sscanf(buf, "%s %s %s %s %d %s %s", key, origin_cmon_and_ip_port, gdf_name, vector_name, &option, pgm_path, to_change);

  // This is the case in which we append the g_mon_id in vector_name So that monitor name will be created like T>S>I>G_MON_ID
  //NA_SM.gdf
  if((json_element_ptr) && (!strncmp(gdf_name, "NA_SM.gdf", 9)))
  {
    vector_name_len = strlen(vector_name);	  
    sprintf(vector_name + vector_name_len, "%c%s", global_settings->hierarchical_view_vector_separator, json_element_ptr->g_mon_id);
    MLTL1(EL_DF, 0, 0, _FLN_, NULL,EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Vector name created for %s gdf is %s", gdf_name, vector_name);
    json_element_ptr->gdf_flag = NA_SM_GDF;
  }

  NSDL2_MON(NULL, NULL, "num = %d", num);
  // Validation
  /*if(num < 6)  // All fields except arguments are mandatory.
  {
    sprintf(err_msg, CAV_ERR_1060064, buf);
    CM_RUNTIME_RETURN_OR_EXIT(runtime_flag, err_msg, 0);
  }
*/
  //TODO PJTSDB:Set BIts
  if(!strstr(gdf_name, "ns_ip_data.gdf")) //Set when ip_data_monitor.gdf parsE
  {
    if(find_tier_idx_and_server_idx(origin_cmon_and_ip_port, cs_ip, origin_cmon, global_settings->hierarchical_view_vector_separator, hpd_port, &server_index, &tier_idx) == -1)
    {
       print_core_events((char*)__FUNCTION__, __FILE__,
                 "Server (%s) not present in topolgy, for the Monitor (%s).\n", origin_cmon_and_ip_port, pgm_path);
      
       sprintf(err_msg,CAV_ERR_1060059,origin_cmon_and_ip_port, pgm_path); 
       CM_RUNTIME_RETURN_OR_EXIT(runtime_flag, err_msg, 0,-1,-1);
    }
    else
    {
      if(topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_index].server_ptr->is_agentless[0] == 'N')
      {
        access = RUN_LOCAL_ACCESS;
        strcpy(rem_ip, "NA");
        strcpy(rem_username, "NA");
        strcpy(rem_password, "NA");
      }
      else
      {
        access = RUN_REMOTE_ACCESS;
        strcpy(rem_ip, cs_ip);
        strcpy(cs_ip, g_cavinfo.NSAdminIP);
        strcpy(rem_username, topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_index].server_ptr->username);
        strcpy(rem_password, topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_index].server_ptr->password);
     }
    }
  }
  else
    strcpy(cs_ip, origin_cmon_and_ip_port);

  if(num < 6)  // All fields except arguments are mandatory.
  {
    sprintf(err_msg, CAV_ERR_1060064, buf); 
    CM_RUNTIME_RETURN_OR_EXIT(runtime_flag, err_msg, 0,-1,-1);
  }

    
  if(json_element_ptr)
  {
    if(!strncmp(pgm_path, "cm_oracle_stats", 15))
      json_element_ptr->gdf_flag = ORACLE_STATS_GDF;
    else if(strstr(pgm_path, "cm_get_log_file"))
      json_element_ptr->gdf_flag = CM_GET_LOG_FILE;
    else if(strstr(pgm_path, "cm_tibco_svc_time"))
      json_element_ptr->gdf_flag = TIBCO_SVC_TIME;
    else if(strstr(pgm_path, "cm_service_stats"))
      json_element_ptr->gdf_flag = SERVICE_STATS;
  }
   

  //TODO PJTSDB
  //For OracleStats monitor, no need to send heartbeat to cmon hence decrementing 'cmon_monitor_count'   
  if((json_element_ptr) && (json_element_ptr->gdf_flag == ORACLE_STATS_GDF))
  {
    //server_mon_ptr.server_index_ptr->cmon_monitor_count--;
    topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_index].server_ptr->topo_servers_list->cmon_monitor_count--;
    NSDL1_MON(NULL, NULL, "This is OracleStats monitor hence decrementedcmon_monitor_count by 1. cmon_monitor_count = [%d]",
        topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_index].server_ptr->topo_servers_list->cmon_monitor_count);
  }

  //here we need to get pgm_args only if user has given any pgm args
  if(!(g_tsdb_configuration_flag & TSDB_MODE) && (num > 6))
  {
    num--; // decrement num as it has first argument also (to_change)
    i = get_tokens(buf, token_arr, " ", 500); //get the total number of tokens 
    for(j = num; j < i; j++)
    {
      if(strstr(token_arr[j], "$OPTION"))
      {
        sprintf(to_change, "%d", option);
        replace_strings(token_arr[j], new_buf, MAX_TEMP_BUF, "$OPTION", to_change);
        strcat(pgm_args, new_buf);
      }
      else if(strstr(token_arr[j], "$INTERVAL_SECS"))
      {
        sprintf(to_change, "%d", global_settings->progress_secs / 10000);
        replace_strings(token_arr[j], new_buf, MAX_TEMP_BUF, "$INTERVAL_SECS", to_change);
        strcat(pgm_args, new_buf);
      }
      else if(strstr(token_arr[j], "$INTERVAL"))
      {
        sprintf(to_change, "%d", global_settings->progress_secs);
        replace_strings(token_arr[j], new_buf, MAX_TEMP_BUF, "$INTERVAL", to_change);
        strcat(pgm_args, new_buf);
      }
      else if(strstr(token_arr[j], "$MON_TEST_RUN"))
      {
        sprintf(to_change, "%d", testidx);
        replace_strings(token_arr[j], new_buf, MAX_TEMP_BUF, "$MON_TEST_RUN", to_change);
        strcat(pgm_args, new_buf);
      }
      else if(strstr(token_arr[j], "$VECTOR_NAME"))
      {
        sprintf(to_change, "%s", vector_name);
        NSDL2_MON(NULL, NULL, "Replacing '$VECTOR_NAME' by %s", to_change);
        replace_strings(token_arr[j], new_buf, MAX_TEMP_BUF, "$OPTION", to_change);
        strcat(pgm_args, new_buf);
      }
      else if(strstr(token_arr[j], "$CAV_MON_HOME"))
      {
        sprintf(to_change, "%s", topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_index].server_ptr->cav_mon_home);
        replace_strings(token_arr[j], new_buf, MAX_TEMP_BUF, "$CAV_MON_HOME", to_change);
        strcat(pgm_args, new_buf);
      }
      else if(strstr(token_arr[j], "$MON_PARTITION_IDX"))
      {
        sprintf(to_change, "%lld", g_start_partition_idx);
        replace_strings(token_arr[j], new_buf, MAX_TEMP_BUF, "$MON_PARTITION_IDX", to_change);
        strcat(pgm_args, new_buf);
      }
      else
        strcat(pgm_args, token_arr[j]);

      strcat(pgm_args, " ");
    }
  }
  else if((g_tsdb_configuration_flag & TSDB_MODE) && (num > 6))
  {
    extract_prgms_args(buf,pgm_args, 5);
  }

  if(option != RUN_EVERY_TIME_OPTION && option != RUN_ONLY_ONCE_OPTION)
  {
    sprintf(err_msg, CAV_ERR_1060060, pgm_path, pgm_args);
    CM_RUNTIME_RETURN_OR_EXIT(runtime_flag, err_msg, 1,tier_idx,server_index);
  }
  if(print_mode != PRINT_PERIODIC_MODE && print_mode != PRINT_CUMULATIVE_MODE)
  {
    sprintf(err_msg, CAV_ERR_1060061, pgm_path, pgm_args);
    CM_RUNTIME_RETURN_OR_EXIT(runtime_flag, err_msg, 1,tier_idx,server_index);
  }
  if(access != RUN_LOCAL_ACCESS && access != RUN_REMOTE_ACCESS)
  {
    sprintf(err_msg, CAV_ERR_1060062, pgm_path, pgm_args);
    CM_RUNTIME_RETURN_OR_EXIT(runtime_flag, err_msg, 1,tier_idx,server_index);
  }

  /* If moniter type is SPECIAL_MONITOR or LOG_MONITOR then the use LPS(LogParsingSystem) for start the moniter,
     Set the use_lps flage                                                                      */
  NSDL2_MON(NULL, NULL, "Check the monoiter type is SPECIAL_MONITOR or LOG_MONITOR and scenario conf file have LPS_SERVER Keyword , key = %s, global_settings->lps_server = %s", key, global_settings->lps_server);

//  NSDL2_MON(NULL, NULL, "pgm_path = %s, pgm_args = %s, cm_set_use_lps(pgm_path, pgm_args) = %d",
  //                       pgm_path, pgm_args, cm_set_use_lps(pgm_path, pgm_args));

  /* We are going to use LPS (Log Processing System) for all special monitors if LPS is enabled
   * This may change later
   * Service_stats is the only monitor which always make connection with LPS whether LPS_SERVER mode is 0/1/2
   * */
  if (json_element_ptr != NULL)
      use_lps = json_element_ptr->use_lps;

  if((json_element_ptr != NULL) &&((json_element_ptr->gdf_flag == SERVICE_STATS) ||  (json_element_ptr->gdf_flag == ORACLE_STATS_GDF)))
    use_lps = 1;
  /*else if((!(strcasecmp(key, "SPECIAL_MONITOR")) || !(strcasecmp(key, "LOG_MONITOR")) || (cm_set_use_lps(pgm_path, pgm_args) == 1)) && (global_settings->lps_mode == 1))
  {
   //Fixed bug 5442
   //ADDED CONTIONAL CHECK FOR GET LOG FILE MONITOR
    //Nither TIBCO NOR GET_LOG_FILE
    if(((strstr(pgm_path, "cm_tibco_svc_time") == NULL) && (strstr(pgm_args, "cm_tibco_svc_time") == NULL)) || (strstr(pgm_path, "cm_get_log_file") ))
      use_lps = 1;
  }*/
  //removing check for SPECIAL_MONITOR, as its not used now
  //when log monitor, auto gdf is generated
  //TODO check json_info once
  else if((json_element_ptr != NULL) && (json_element_ptr->lps_enable == 1) && (global_settings->lps_mode == 1))
    use_lps = 1;
  //CM_GET_LOG_FILE will always be configured as LOG_MONITOR
  else if((json_element_ptr != NULL) && (json_element_ptr->gdf_flag == CM_GET_LOG_FILE) && (global_settings->lps_mode != 1))
  {
    CM_RUNTIME_RETURN_OR_EXIT(runtime_flag, CAV_ERR_1060063, 1,tier_idx,server_index);
  }else if ((json_element_ptr == NULL) && !strcmp(gdf_name, "NA_get_log_file.gdf"))
    use_lps = 1;

  NSDL2_MON(NULL, NULL, "vector_name = %s, pgm_path = %s,  gdf_name = %s, option = %d, cs_ip = %s, access = %d, print_mode = %d, rem_ip = %s, rem_username = %s, rem_password = %s, pgm_args = %s, use_lps = %d, origin_cmon = %s", vector_name, pgm_path, gdf_name, option, cs_ip, access, print_mode, rem_ip, rem_username, rem_password, pgm_args, use_lps, origin_cmon);

  return(add_custom_dynamic_monitor(vector_name, pgm_path, gdf_name, option, print_mode, origin_cmon_and_ip_port, access, rem_ip, rem_username, rem_password, pgm_args, use_lps, server_name, runtime_flag, err_msg, dyn_cm, origin_cmon, buffer, max, fd, server_mon_ptr, breadcrumb, is_new_format, ins_map_ptr, tier_map_ptr, vector_map_ptr, NULL, max_mapping_tbl_vectors_entries, server_index, pod_name, json_element_ptr, skip_breadcrumb_creation,tier_idx));
#undef MAX_TEMP_BUF
}
