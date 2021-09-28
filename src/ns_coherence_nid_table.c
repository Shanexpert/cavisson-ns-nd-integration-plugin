/*
This file contain functions to create NID -> Instance Mapping




*/

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include "ns_alloc.h"
#include "ns_event_log.h"
#include "nslb_get_norm_obj_id.h"
#include "ns_coherence_nid_table.h"
#include "ns_check_monitor.h"
#include "ns_event_id.h"
#include "ns_string.h"
#include "v1/topolib_structures.h"
#include "nslb_util.h"
#include "ns_global_settings.h"
#include "ns_pre_test_check.h"
#include "ns_dynamic_vector_monitor.h"
#include "ns_trace_level.h"
#include "ns_custom_monitor.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"
#include "ns_monitor_profiles.h"
#include "url.h"
#define DELTA_NID_TABLE_ENTRIES       5
#ifndef MAX_FIELDS
#define MAX_FIELDS 10   
#endif
#define FIELDS_IN_CLUSTER_WITHOUT_NID 1
#define FIELDS_IN_CLUSTER_OLD_FORMAT_SERVICE_OR_STORAGE_MON 2
#define FIELDS_IN_NEW_FORMAT 3
#define FIELDS_IN_CLUSTER_BREADCRUMB_FORMAT 4
#define FIELDS_IN_CACHE_BREADCRUMB_FORMAT 5

#define BUF_LENGTH 1024


//NOdeID Instance table
NIDInstanceTableList *nid_inst_tbl_ptr_list;

int nid_table_size = 0; //node id table size

char cluster_vector_file_path[BUF_LENGTH] = {0};
char cache_vector_file_path[BUF_LENGTH] = {0};

int kw_set_coherence_nodeid_table_size(char *keyword, char *buf, int runtime_flag, char *err_msg)
{
  char key[BUF_LENGTH] = {0};

  if((sscanf(buf, "%s %d", key, &nid_table_size)) < 2)
  {
    NS_KW_PARSING_ERR(keyword, -1, err_msg, COHERENCE_NID_TABLE_SIZE_USAGE, CAV_ERR_1011359, keyword);
  }
  return 0;
}

/* set nid_table_size with keyword value passed*/
/* If coherence cluster monitor is applied by user, we need to init the table before recieving the data*/
int nid_inst_tbl_init(int nid, int row_no)
{
  if(nid >= nid_inst_tbl_ptr_list[row_no].max_nid_inst_tbl_entries)
  {
    NSTL2(NULL, NULL, "Realloc NID Table from size %d to size %d.", nid_inst_tbl_ptr_list[row_no].max_nid_inst_tbl_entries, nid + nid_table_size);

    NS_EL_2_ATTR(EID_NS_INIT,  -1, -1, EVENT_CORE, EVENT_INFO, __FILE__, (char*)__FUNCTION__,
                     "Realloc NID Table from size %d to size %d.", nid_inst_tbl_ptr_list[row_no].max_nid_inst_tbl_entries, nid + nid_table_size);

    MY_REALLOC_AND_MEMSET(nid_inst_tbl_ptr_list[row_no].nid_inst_tbl_ptr, (sizeof(NIDInstanceTable) * (nid + nid_table_size)), (sizeof(NIDInstanceTable) * (nid_inst_tbl_ptr_list[row_no].max_nid_inst_tbl_entries)), "allocate NIDInstanceTable", -1);

    nid_inst_tbl_ptr_list[row_no].max_nid_inst_tbl_entries = nid + nid_table_size;
  }
  
  return 0;
}

/* Return values:
  -1 - Error: machine name not found in topology
   0 - Started server signature
   1 - Either Found instance name 
   2 - Fallback to NodeID
*/
int find_instance_name_from_topolib(char *machine_name, char *instance_name, pid_t pid, int *tierinfo_idx, int nid, int start_server_sig_or_not)
{
  int srv_index = -1;
  int inst_index = -1;
  int i = 0, j = 0, k = 0;

  NSTL2(NULL, NULL, "machine_name = %s, instance_name = %s, pid = %u, nid = %d, start_server_sig_or_not = %d", machine_name, instance_name, pid, nid, start_server_sig_or_not);

  //if have instance name then (a) search this instance in instance.conf & respective machine name in topo server.conf
  //                                 if found get tier, server & set breadcrumb
  //                                 else search in topology for server present in vector name and execute all the steps mentioned below
  //THis code will execute when member name is present in vector name
  if(instance_name[0] != '\0')
  {
    NSTL2(NULL, NULL, "Found instance name %s in vector.", instance_name);
    for(i = 0; i < topo_info[topo_idx].total_tier_count;i++)
    {
      for(j=0;j<topo_info[topo_idx].topo_tier_info[i].total_server;j++)
      {
        if(topo_info[topo_idx].topo_tier_info[i].topo_server[j].used_row != -1 && !topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->server_flag & DUMMY) 
        { 
          TopoServerInfo *server_ptr     = topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr;
          TopoInstance *tmp_instance_ptr = topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->instance_head;
          if((strcmp(instance_name,tmp_instance_ptr->display_name) == 0) && (strcmp(machine_name,topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_disp_name) == 0))
          {
            NSTL2(NULL, NULL, "Matched instance_name %s with topo_instance_info[%d].displayName %s and machine_name = %s with topo_server_info[%d].ServerDispName %s. Obtained tier index is %d.", instance_name, k, server_ptr->server_disp_name, machine_name, j, server_ptr->server_disp_name, i);

           tierinfo_idx =&i; //save tier index to get tier name   
           return 1;
          }
          else
            tmp_instance_ptr=tmp_instance_ptr->next_instance;
        } 
      }
    }
  }

  //search in topology for server present in vector name
  //if found,
  //         match pid
  //         if found,
  //            return instance name
  //         if not found
  //           start server signature   
  //if not found
  //         show error
  //   

  //search in topology for server present in vector name
  //NOTE: Comparing with server display name only, not with server name
  for (i = 0; i < topo_info[topo_idx].total_tier_count; i++)
  {
    for(int j=0;j<topo_info[topo_idx].topo_tier_info[i].total_server;j++)
    {
      if((topo_info[topo_idx].topo_tier_info[i].topo_server[j].used_row != -1) && (!topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->server_flag & DUMMY))
      {
        if(!strcmp(machine_name, topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_disp_name)) //found machine name
        {
          srv_index = j;
          tierinfo_idx = &i; //save tier index to get tier name
          NSTL2(NULL, NULL, "Matched server %s present in vector name with topology server display name %s.", machine_name, topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_disp_name);
          break;
        }
      }
    }  
  }
  //machine name not found then return -1 do logging
  if(srv_index == -1)
  {
    NSTL1_OUT(NULL, NULL, "Error: Obtained machine name %s not present in topology server.conf.\n", machine_name);
    return -1;
  }
  int tier_idx=*tierinfo_idx;
  //match pid
  for (int k = 0; k < topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[srv_index].server_ptr->tot_inst; j++)
  {
    //inst_index = topo_server_info[srv_index].inst_indx[j];
    TopoInstance *tmp_instance_ptr = topo_info[topo_idx].topo_tier_info[i].topo_server[j].server_ptr->instance_head;
    //matched pid, fill instance name & return
    if(tmp_instance_ptr->pid == pid)
    {
      NSTL2(NULL, NULL, "Matched pid %u present in vector name with pid %u saved in topology instance structure at index %d.", pid, tmp_instance_ptr->pid, inst_index);
      strcpy(instance_name, tmp_instance_ptr->display_name);
      return 1;
    }
   else
     tmp_instance_ptr=tmp_instance_ptr->next_instance;
  }
 
  if(java_process_server_sig_enabled == 1 && topo_info[topo_idx].topo_tier_info[i].topo_server[srv_index].server_ptr->server_sig_indx >= 0)
  {
    //start server signature only when this func called during runtime, not during DVM->CM because we are retrying for server sign from parent
    if((start_server_sig_or_not == 1) && (pre_test_check_info_ptr[topo_info[topo_idx].topo_tier_info[i].topo_server[srv_index].server_ptr->server_sig_indx].fd <= 0))
    {
      pre_test_check_info_ptr[topo_info[topo_idx].topo_tier_info[i].topo_server[srv_index].server_ptr->server_sig_indx].status = CHECK_MONITOR_RETRY;
      pre_test_check_info_ptr[topo_info[topo_idx].topo_tier_info[i].topo_server[srv_index].server_ptr->server_sig_indx].from_event = CHECK_MONITOR_EVENT_RETRY_INTERNAL_MON;
      pre_test_check_info_ptr[topo_info[topo_idx].topo_tier_info[i].topo_server[srv_index].server_ptr->server_sig_indx].state = CHECK_MON_COMMAND_STATE;

     reset_chk_mon_values(&pre_test_check_info_ptr[topo_info[topo_idx].topo_tier_info[i].topo_server[srv_index].server_ptr->server_sig_indx]);

      NSTL2(NULL, NULL, "Starting server signature on server %s.", pre_test_check_info_ptr[topo_info[topo_idx].topo_tier_info[i].topo_server[srv_index].server_ptr->server_sig_indx].cs_ip);
      //if not found pid, start server signature
      start_server_sig(&pre_test_check_info_ptr[topo_info[topo_idx].topo_tier_info[i].topo_server[srv_index].server_ptr->server_sig_indx]);
    }

    //If no. of retries reached the max limit, show data using node id
    if(pre_test_check_info_ptr[topo_info[topo_idx].topo_tier_info[i].topo_server[srv_index].server_ptr->server_sig_indx].retry_count == pre_test_check_info_ptr[topo_info[topo_idx].topo_tier_info[i].topo_server[srv_index].server_ptr->server_sig_indx].max_retry_count)
    {
      NSTL1(NULL, NULL, "No. of retries reached the max limit %d, showing data using node id %d.", pre_test_check_info_ptr[topo_info[topo_idx].topo_tier_info[i].topo_server[srv_index].server_ptr->server_sig_indx].max_retry_count, nid);
      sprintf(instance_name, "%d", nid);
      return 2;
    }
  }
  else
  {
    sprintf(instance_name, "%d", nid);
    return 2;
  }
  

  return 0;
}

void update_nid_table(int nid, char *machine_name, char *instance_name, pid_t pid, int tierinfo_idx, int row_no, int server_index)
{
  NSTL2(NULL, NULL, "Updating NodeID table with values: machine_name %s, instance_name %s, pid %u, tiername %s, NodeID %d", machine_name, instance_name, pid, topo_info[topo_idx].topo_tier_info[tierinfo_idx].tier_name, nid);

  REALLOC_AND_COPY(instance_name, nid_inst_tbl_ptr_list[row_no].nid_inst_tbl_ptr[nid].InstanceName, strlen(instance_name) + 1, "Instance Name", -1);
  REALLOC_AND_COPY(machine_name, nid_inst_tbl_ptr_list[row_no].nid_inst_tbl_ptr[nid].MachineName, strlen(machine_name) + 1, "Machine Name", -1);
  REALLOC_AND_COPY(topo_info[topo_idx].topo_tier_info[tierinfo_idx].tier_name, nid_inst_tbl_ptr_list[row_no].nid_inst_tbl_ptr[nid].TierName, strlen(topo_info[topo_idx].topo_tier_info[tierinfo_idx].tier_name) + 1, "Tier Name", -1);
  nid_inst_tbl_ptr_list[row_no].nid_inst_tbl_ptr[nid].pid = pid;
  nid_inst_tbl_ptr_list[row_no].server_index = server_index;
}

//Get Instance Name at the index specified by nid
//return 0 on success
//return 1 if we didn't got the instance name, and we executed the server signature monitor
//to get instance name [OR WE MIGHT RETURN FAILURE AND LET MONITOR CODE START SERV_SIG]

void nid_get_and_save_instance_name(int nid, char *machine_name, char *instance_name, pid_t pid, int *tierinfo_idx, int start_server_sig_or_not, int row_no, int server_index)
{
  //if obtaine node ID is greater than max NID table entries then realloc 

  nid_inst_tbl_init(nid, row_no);

  //Check if data at index (nid) is NULL
  if(nid_inst_tbl_ptr_list[row_no].nid_inst_tbl_ptr[nid].MachineName == NULL)
  {
     NSTL2(NULL, NULL, "Entry does not exists in NodeID table at index %d.", nid);
    //New nid, we don't have any entry in table. Go for instance mapping
    if(find_instance_name_from_topolib(machine_name, instance_name, pid, tierinfo_idx, nid, start_server_sig_or_not) == 1)
    {
      //update Node ID table
      update_nid_table(nid, machine_name, instance_name, pid, *tierinfo_idx, row_no, server_index);
    }
  }
  else
  {
    NSTL2(NULL, NULL, "Entry exists in NodeID table at index %d.", nid);

    // Go to nid index and compare pid & machine name got in vector and strored in the table.
    if ((nid_inst_tbl_ptr_list[row_no].nid_inst_tbl_ptr[nid].pid == pid) && (strcmp(machine_name, nid_inst_tbl_ptr_list[row_no].nid_inst_tbl_ptr[nid].MachineName) == 0)) 
    {
      NSTL2(NULL, NULL, "Matched pid %u & machine_name %s present in vector name with pid %d & machine_name %s present in NodeID table at index %d.", pid, machine_name, nid_inst_tbl_ptr_list[row_no].nid_inst_tbl_ptr[nid].pid, nid_inst_tbl_ptr_list[row_no].nid_inst_tbl_ptr[nid].MachineName, nid);

      //PID & machine name matched, 
      strcpy(instance_name, nid_inst_tbl_ptr_list[row_no].nid_inst_tbl_ptr[nid].InstanceName);
      return;
    }
    else 
   {
      NSTL2(NULL, NULL, "Not Matched pid %u & machine_name %s present in vector name with pid %d & machine_name %s present in NodeID table at index %d.", pid, machine_name, nid_inst_tbl_ptr_list[row_no].nid_inst_tbl_ptr[nid].pid, nid_inst_tbl_ptr_list[row_no].nid_inst_tbl_ptr[nid].MachineName, nid);

      //PID changed or NID changed, earlier instance name was at different index
      //get instance name from the topology
      //find_instance_name_from_topolib(char *machine_name, char *instance_name, pid_t pid);
      if(find_instance_name_from_topolib(machine_name, instance_name, pid, tierinfo_idx, nid, start_server_sig_or_not) == 1)
      {
        //update Node ID table
        update_nid_table(nid, machine_name, instance_name, pid, *tierinfo_idx, row_no, server_index);
      }
      else
      {
        memset(&(nid_inst_tbl_ptr_list[row_no].nid_inst_tbl_ptr[nid]), 0, sizeof(NIDInstanceTable));
      }
    }
  }
}


static void remove_trailing_char(char *machine_name)
{
  char *ptr;
  int num_fields=0;
  char *fields[5] = {0};
  char trailing_char[128] = {0};
  char trailing_for_tokenzing[128] = {0};

  sprintf(trailing_for_tokenzing,"%s",g_trailing_char);
  num_fields = get_tokens_with_multi_delimiter(trailing_for_tokenzing, fields, ",", 5);
  int j=0;
  while(num_fields)
  {
    sprintf(trailing_char,"%s",fields[j]);
    ptr = strstr(machine_name,trailing_char);
    if(ptr)
    {
      NSTL2(NULL, NULL, "Found replacable character(%s) in machine name %s. Going to replace it with NULL", trailing_char, machine_name);
      *ptr = '\0';
    }
      NSTL3(NULL, NULL, "Machine name after replacing =%s",machine_name);
    j++;
    num_fields--;
  }
}


/*
  RETURN VALUES :
  0 - new format & not found instance name in vector name 
  1 - old format
  2 - found instance name in vector name
*/
static int parse_cluster_vector(char *vector, char *inst_name, char *machine_name, int *pid, char *role_name, int *node_id, char *breadcrumb)
{
  char *fields[MAX_FIELDS];
  char buff[BUF_LENGTH] = {0};
  int num_fields = 0;
  char server[BUF_LENGTH + 1] = {0};
  char display_server_name[BUF_LENGTH + 1] = {0};
  char tiername[BUF_LENGTH + 1] = {0};
  char err_msg[BUF_LENGTH + 1] = {0};
  char hv_separator[2] = {0};
  hv_separator[0] = global_settings->hierarchical_view_vector_separator;

  NSTL2(NULL, NULL, "Parsing coherence cluster vector %s", vector);

  num_fields = get_tokens(vector, fields, hv_separator, MAX_FIELDS);

  NSTL2(NULL, NULL, "Obtained num_fields %d.", num_fields);

  //old format
  if(num_fields == FIELDS_IN_CLUSTER_OLD_FORMAT_SERVICE_OR_STORAGE_MON)
  {
    NSTL1(NULL, NULL, "Vector %s format is old. Hence returning...", vector);
    return 1;
  }

  //new format
  if(num_fields == FIELDS_IN_NEW_FORMAT)
  {
    NSTL2(NULL, NULL, "Vector %s format is new. Going to process vector name.", vector);

    strcpy(role_name, fields[0]);
    *node_id = atoi(fields[2]);

    //parse colon separated data
    strcpy(buff, fields[1]);
    num_fields = get_tokens(buff, fields, ":", MAX_FIELDS);

    NSTL2(NULL, NULL, "After tokenizing num_fields %d : role_name %s, buff %s, node_id %d, machine_name %s, pid %d, inst_name %s", num_fields, role_name, buff, *node_id, fields[0], atoi(fields[1]), fields[2]);

    if(num_fields == 3)  //found instance name in vector
    {
      strcpy(inst_name, fields[2]);
      strcpy(machine_name, fields[0]);
      *pid = atoi(fields[1]);
      return 2;
    }
    strcpy(machine_name, fields[0]);
    *pid = atoi(fields[1]);

    if(g_remove_trailing_char)
      remove_trailing_char(machine_name);
  }

  if(num_fields == FIELDS_IN_CLUSTER_BREADCRUMB_FORMAT)
  {
   //vector is in format : Apptier > machine name > Role > Instance Name
   sprintf(breadcrumb, "%s%c%s%c%s%c%s", fields[0], global_settings->hierarchical_view_vector_separator, fields[1], global_settings->hierarchical_view_vector_separator, fields[2], global_settings->hierarchical_view_vector_separator, fields[3]);
   return 3;
  }

  //for cluster monitor without NID.
  // Vector name format : <InstanceName>
  //Breadcrumb format : Tier>Server>InstanceName
  if(num_fields == FIELDS_IN_CLUSTER_WITHOUT_NID)
  {
    topolib_get_tier_and_server_disp_name_from_instance(fields[0], tiername, display_server_name, err_msg, server,topo_idx);
    sprintf(breadcrumb, "%s%c%s%c%s", tiername, global_settings->hierarchical_view_vector_separator,
                                                 display_server_name, global_settings->hierarchical_view_vector_separator, fields[0]);

    return 1;
  }

  return 0;
}

// Apptier > machine name > Role > Instance Name
//void generate_breadcrumb_for_coh_cluster(int tierinfo_idx, char *machine_name, char *inst_name, char *breadcrumb, char *role_name)
void generate_breadcrumb_for_coh_cluster(int nid, char *role_name, char *breadcrumb, char *inst_name, int tierinfo_idx, int row_no, int server_index)
{
  //sprintf(breadcrumb, "%s%c%s%c%s%c%s", nid_inst_tbl_ptr[nid].TierName, global_settings->hierarchical_view_vector_separator, nid_inst_tbl_ptr[nid].MachineName, global_settings->hierarchical_view_vector_separator, role_name, global_settings->hierarchical_view_vector_separator, nid_inst_tbl_ptr[nid].InstanceName);

  //Not taking instance name from NID Table because in case when we fallback back on NID as insatnce name, we do nto update NID table, hence passing instance to this function
  if((nid_inst_tbl_ptr_list[row_no].nid_inst_tbl_ptr[nid].TierName != NULL) || (nid_inst_tbl_ptr_list[row_no].nid_inst_tbl_ptr[nid].MachineName != NULL))
  {
    sprintf(breadcrumb, "%s%c%s%c%s%c%s", nid_inst_tbl_ptr_list[row_no].nid_inst_tbl_ptr[nid].TierName, global_settings->hierarchical_view_vector_separator, nid_inst_tbl_ptr_list[row_no].nid_inst_tbl_ptr[nid].MachineName, global_settings->hierarchical_view_vector_separator, role_name, global_settings->hierarchical_view_vector_separator, inst_name);
  }
  else
  {
    sprintf(breadcrumb, "Unknown%cUnknown%c%s%c%s", global_settings->hierarchical_view_vector_separator, global_settings->hierarchical_view_vector_separator, role_name, global_settings->hierarchical_view_vector_separator, inst_name);
  }

  NSTL2(NULL, NULL, "Generated coherence cluster monitor breadcrumb is %s", breadcrumb);
}

/* Old Format:
 *    Vector name : 
 *      RoleName>NID  
 *
 * New Format:
 *    Vector name : 
 *      RoleName>MachineName:PID:MemberName>NodeID
 *    BreadCrumb :
 *      Apptier > machine name > Role > Instance Name
 *
 * Process coherence vector list
 *  
*/
void process_coh_cluster_mon_vector(char *vector, char *breadcrumb, int start_server_sig_or_not, int row_no, int server_index)
{
  char machine_name[BUF_LENGTH] = {0};
  char role_name[BUF_LENGTH] = {0};
  char inst_name[BUF_LENGTH] = {0};
  int node_id = 0;
  int tierinfo_idx = -1;
  pid_t pid = 0;
  int ret = -1;

  //Parse coherence cluster vector
  ret = parse_cluster_vector(vector, inst_name, machine_name, &pid, role_name, &node_id ,breadcrumb);
  if((ret == 1) || (ret == 3))
  {
    return ; //old format or found complete breadcrumb
  }

  //Save instance name
  nid_get_and_save_instance_name(node_id, machine_name, inst_name, pid, &tierinfo_idx, start_server_sig_or_not, row_no, server_index);

  //if found instance name then generate breadcrumb
  if(inst_name[0] != '\0')
  {
    //generate_breadcrumb_for_coh_cluster(tierinfo_idx, machine_name, inst_name, breadcrumb, role_name);
    generate_breadcrumb_for_coh_cluster(node_id, role_name, breadcrumb, inst_name, tierinfo_idx, row_no, server_index);
  }
  return ;
}


/*************************************** Coherence cache code begins ********************************/

// Format : F/B>NID>CacheName
//Format : <member-name>:nid:<vector_seprator><vector name>
int parse_cache_service_or_storage_vector(char *vector, char *cache_type, char *cache_service_or_storage_name, int *nid, char *breadcrumb, char *storage_instance_name)
{
  char *fields[MAX_FIELDS];
  char *tmp_ptr;
  int num_fields = 0;
  char hv_separator[2] = {0};
  hv_separator[0] = global_settings->hierarchical_view_vector_separator;

  NSTL2(NULL, NULL, "Parsing coherence cache monitor vector %s.", vector);

  num_fields = get_tokens(vector, fields, hv_separator, MAX_FIELDS);
  
  if(num_fields == FIELDS_IN_CACHE_BREADCRUMB_FORMAT)
  {
   //vector is in format : Tier>Server>Type>Instance>CacheName
   sprintf(breadcrumb, "%s%c%s%c%s%c%s%c%s", fields[0], global_settings->hierarchical_view_vector_separator, fields[1], global_settings->hierarchical_view_vector_separator, fields[2], global_settings->hierarchical_view_vector_separator, fields[3], global_settings->hierarchical_view_vector_separator, fields[4]);
   return 1;
  }

  if(num_fields == FIELDS_IN_NEW_FORMAT)
  {
    strcpy(cache_type, fields[0]);
    *nid = atoi(fields[1]);
    strcpy(cache_service_or_storage_name, fields[2]);
  }
 
  if(num_fields == FIELDS_IN_CLUSTER_OLD_FORMAT_SERVICE_OR_STORAGE_MON)
  {
    tmp_ptr=strchr(fields[0],':');
    if(tmp_ptr)    //storage monitor
    {
      *tmp_ptr='\0';
      tmp_ptr++;
      strcpy(storage_instance_name, fields[0]);
      *nid = atoi(tmp_ptr);
      strcpy(cache_service_or_storage_name, fields[1]);
    }
    else  //service monitor
    {
      *nid = atoi(fields[0]);
      strcpy(cache_service_or_storage_name, fields[1]);
    }
  }
 
  NSTL2(NULL, NULL, "After tokenizing num_fields %d : cache_type %s, nid %d, cache_service_or_storage_name %s, member name %s.", num_fields, cache_type, *nid, cache_service_or_storage_name, storage_instance_name);
  return num_fields;
}

//Tier>Server>Type>Instance>CacheName
void generate_breadcrumb_for_coh_cache(char *breadcrumb, int nid, char *cache_name, char *cache_type, int row_no)
{
  sprintf(breadcrumb, "%s%c%s%c%s%c%s%c%s", nid_inst_tbl_ptr_list[row_no].nid_inst_tbl_ptr[nid].TierName, global_settings->hierarchical_view_vector_separator, nid_inst_tbl_ptr_list[row_no].nid_inst_tbl_ptr[nid].MachineName, global_settings->hierarchical_view_vector_separator, cache_type, global_settings->hierarchical_view_vector_separator, nid_inst_tbl_ptr_list[row_no].nid_inst_tbl_ptr[nid].InstanceName, global_settings->hierarchical_view_vector_separator, cache_name);
  NSTL2(NULL, NULL, "Generated coherence cache monitor breadcrumb %s", breadcrumb);
}

//Tier>Server>Instance>ServiceName
void generate_breadcrumb_for_coh_service(char *breadcrumb, int nid, char *service_name, int row_no)
{
  sprintf(breadcrumb, "%s%c%s%c%s%c%s", nid_inst_tbl_ptr_list[row_no].nid_inst_tbl_ptr[nid].TierName, global_settings->hierarchical_view_vector_separator, nid_inst_tbl_ptr_list[row_no].nid_inst_tbl_ptr[nid].MachineName, global_settings->hierarchical_view_vector_separator, nid_inst_tbl_ptr_list[row_no].nid_inst_tbl_ptr[nid].InstanceName, global_settings->hierarchical_view_vector_separator, service_name);
  NSTL2(NULL, NULL, "Generated coherence service monitor breadcrumb %s", breadcrumb);
}
 
//Tier>Server>Instance>StorageName
void generate_breadcrumb_for_coh_storage(char *breadcrumb, int nid , char *storage_name, char *storage_instance_name, int row_no)
{
  char server[BUF_LENGTH + 1] = {0};
  char display_server_name[BUF_LENGTH + 1] = {0};
  char tiername[BUF_LENGTH + 1] = {0};
  char err_msg[BUF_LENGTH + 1] = {0};
 
  if(storage_instance_name[0] != '\0')
  {
    topolib_get_tier_and_server_disp_name_from_instance(storage_instance_name, tiername, display_server_name, err_msg, server, topo_idx);
    if(tiername[0] != '\0' && display_server_name[0] != '\0')
    {
      sprintf(breadcrumb, "%s%c%s%c%s%c%s", tiername, global_settings->hierarchical_view_vector_separator, display_server_name, global_settings->hierarchical_view_vector_separator, storage_instance_name, global_settings->hierarchical_view_vector_separator, storage_name);
      NSTL2(NULL, NULL, "Generated coherence storage monitor breadcrumb %s", breadcrumb);
      return;
    }
  }
   sprintf(breadcrumb, "%s%c%s%c%s%c%s", nid_inst_tbl_ptr_list[row_no].nid_inst_tbl_ptr[nid].TierName, global_settings->hierarchical_view_vector_separator, nid_inst_tbl_ptr_list[row_no].nid_inst_tbl_ptr[nid].MachineName, global_settings->hierarchical_view_vector_separator, nid_inst_tbl_ptr_list[row_no].nid_inst_tbl_ptr[nid].InstanceName, global_settings->hierarchical_view_vector_separator, storage_name);
  NSTL2(NULL, NULL, "Generated coherence storage monitor breadcrumb %s", breadcrumb);
}

/* Format: CacheType>NodeID>CacheName
*/
void process_coh_cache_service_storage_mon_vector(char *vector, char *breadcrumb, char *gdf_name, int row_no)
{
  char cache_type[BUF_LENGTH] = {0};
  char cache_service_or_storage_name[BUF_LENGTH] = {0};
  char storage_instance_name[BUF_LENGTH] = {0};
  int nid = -1;
  int ret = 0;
  
    //parse cache vector
  ret = parse_cache_service_or_storage_vector(vector, cache_type, cache_service_or_storage_name, &nid, breadcrumb, storage_instance_name);
  if (ret == 1)
    return; //found complete breadcrumb
  if (storage_instance_name[0] != '\0')
  {
    generate_breadcrumb_for_coh_storage(breadcrumb, nid, cache_service_or_storage_name, storage_instance_name, row_no);
    return;  //Done creation of breadcrumb of coherence storage cache monitor
  }

  //if found instance name then generate breadcrumb
  if((nid <= nid_inst_tbl_ptr_list[row_no].max_nid_inst_tbl_entries) && (nid_inst_tbl_ptr_list[row_no].nid_inst_tbl_ptr[nid].InstanceName != NULL))
  {
    if(ret == FIELDS_IN_CLUSTER_OLD_FORMAT_SERVICE_OR_STORAGE_MON) //service monitor
      if(strstr(gdf_name, "cm_coherence_storage") != NULL)
        generate_breadcrumb_for_coh_storage(breadcrumb, nid, cache_service_or_storage_name, storage_instance_name, row_no);
      else
        generate_breadcrumb_for_coh_service(breadcrumb, nid, cache_service_or_storage_name, row_no);
    else // cache
      generate_breadcrumb_for_coh_cache(breadcrumb, nid, cache_service_or_storage_name, cache_type, row_no);
  }
  return;
}

int kw_set_coh_cluster_cache_vectors(char *keyword, char *buf)
{
  char key[BUF_LENGTH] = "";
  char temp[BUF_LENGTH];
  int value , args;
  char err_msg[MAX_AUTO_MON_BUF_SIZE]; 

  NSDL2_MON(NULL, NULL, "Method called, keyword = %s, buf = %s, global_settings->net_diagnostics_mode=%d", keyword, buf, global_settings->net_diagnostics_mode);

  args = sscanf(buf, "%s %d %s %s %s", key, &value, cluster_vector_file_path, cache_vector_file_path, temp);
  
  if(args != 4)
  {
    NS_KW_PARSING_ERR(keyword, -1, err_msg, COH_CLUSTER_AND_CACHE_VECTORS_USAGE, CAV_ERR_1011359, keyword);
  }

  if(value == 1)
  {
    if(!strncmp(cluster_vector_file_path,"NA",2))
    {
      cluster_vector_file_path[0]='\0';
    }
    if(!strncmp(cache_vector_file_path,"NA",2))
    { 
      cache_vector_file_path[0]='\0';                        
    }
  }
  return 0;
}

void nid_inst_tbl_dump(FILE *tmp_fp)
{
  int i = 0, j = 0;

  NSDL3_MON(NULL, NULL, " Method Called.");

  fprintf(tmp_fp, "\n----------------- NIDInstanceTable *nid_inst_tbl_ptr_list Dump STARTS:----------------\n");
  for(i = 0; i < total_nid_table_row_entries; i++)
  {
    for(j = 0; j < nid_inst_tbl_ptr_list[i].max_nid_inst_tbl_entries; j++)
    {
      if( nid_inst_tbl_ptr_list[i].nid_inst_tbl_ptr[j].TierName != NULL )
      {
      fprintf(tmp_fp, "row_no = [ %d ], node_id = [ %d], InstanceName = [%s], MachineName = [%s], TierName = [%s], pid = [%u]\n",i, j, nid_inst_tbl_ptr_list[i].nid_inst_tbl_ptr[j].InstanceName,nid_inst_tbl_ptr_list[i].nid_inst_tbl_ptr[j].MachineName,nid_inst_tbl_ptr_list[i].nid_inst_tbl_ptr[j].TierName,nid_inst_tbl_ptr_list[i].nid_inst_tbl_ptr[j].pid);
      }
    }
  }
  fprintf(tmp_fp, "\n---------------- NIDInstanceTable *nid_inst_tbl_ptr Dump ENDS:----------------\n\n\n");
}


int find_row_idx_from_nid_table(int server_index)
{
  int i;

  for(i = 0; i < total_nid_table_row_entries; i++)
  {
    if(nid_inst_tbl_ptr_list[i].server_index == server_index)
      return i;
  }
  
  return -1;
}


int create_nid_table_row_entries()  //create rows of NID Mapping table
{ 
  NSDL4_MON(NULL, NULL, "Method called. total = %d, max = %d, ptr = %p, size = %d, name = Coherence NID Table", total_nid_table_row_entries, max_nid_table_row_entries, nid_inst_tbl_ptr_list, sizeof(NIDInstanceTableList));
  
  MY_REALLOC_AND_MEMSET(nid_inst_tbl_ptr_list, ((max_nid_table_row_entries + DELTA_NID_TABLE_ENTRIES) * sizeof(NIDInstanceTableList)), (max_nid_table_row_entries *  sizeof(NIDInstanceTableList)), "DVM Vector Idx Mapping Table Row", -1);
  
  max_nid_table_row_entries += (DELTA_NID_TABLE_ENTRIES);
  
  return 0;
}
