#include <stdio.h>
#include <v1/topolib_structures.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <url.h>
#include "ns_runtime_changes_monitor.h"
#include "ns_dynamic_vector_monitor.h"
#include "ns_custom_monitor.h"
#include "ns_log.h"
#include "ns_tier_group.h"
#include "v1/topolib_runtime_changes.h"
#include "nslb_util.h"
#include "nslb_log.h"
#include "ns_runtime.h"
void handle_tier_deletion(char *tierName,int tid)
{
  int ret = 0;
  char monitor_buf[MAX_DATA_LINE_LENGTH]={0};
  char err_msg[MAX_DATA_LINE_LENGTH]={0};

  sprintf(monitor_buf,"DELETE_MONITOR AllMon NA NA %s%c", topo_info[topo_idx].topo_tier_info[tid].tier_name,global_settings->hierarchical_view_vector_separator);

  ret = kw_set_runtime_monitors(monitor_buf, err_msg);

  if(ret == 0) //atleast one monitor parsing successfull
  {
    monitor_runtime_changes_applied = 1;  //set in both the cases 'ADD/DELETE'
    monitor_deleted_on_runtime = 2;  //set in both the cases 'ADD/DELETE'
  }

  if(err_msg != '\0')
    NSTL1(NULL, NULL, "%s",err_msg);
  
  return;
}

void handle_tier_addition(char *tierName,int tid)
{
  int j;
  

  handle_tier_deletion(tierName,tid);

  if((global_settings->json_mode == 1) && ((global_settings->auto_json_monitors_filepath) && (global_settings->auto_json_monitors_filepath[0] != '\0')))
  {
    for(j=0;j<topo_info[topo_idx].topo_tier_info[tid].total_server;j++)
    {
      if((topo_info[topo_idx].topo_tier_info[tid].topo_server[j].used_row != -1) && (topo_info[topo_idx].topo_tier_info[tid].topo_server[j].server_id_tf < 0))
        continue;
     
      add_json_monitors(NULL,j, tid, NULL, 1, 0, 0);
    }
  }
}

void handle_tier_list_update(int topo_tier_group_index, char *oldTierList[], int oldTierListCount)
{
  int i = 0, j = 0, ret = 0;

  while(i < topo_info[topo_idx].topo_tier_group[topo_tier_group_index].total_list_count && j < oldTierListCount)
  {
    ret = strcmp(topo_info[topo_idx].topo_tier_group[topo_tier_group_index].tierList[i], oldTierList[j]);

    if(ret == 0)
    {
      i++;
      j++;
    }
    else if(ret < 0)
    { 
     int tid=topolib_get_tier_id_from_tier_name(topo_info[topo_idx].topo_tier_group[topo_tier_group_index].tierList[i],topo_idx);
     handle_tier_addition(topo_info[topo_idx].topo_tier_group[topo_tier_group_index].tierList[i],tid);
      i++;
    }
    else
    { 
     int tid=topolib_get_tier_id_from_tier_name(oldTierList[j],topo_idx);
     handle_tier_deletion(oldTierList[j],tid);
      j++;
    } 
  }
  
  if(i <topo_info[topo_idx].topo_tier_group[topo_tier_group_index].total_list_count)
  { 
    int tid=topolib_get_tier_id_from_tier_name(topo_info[topo_idx].topo_tier_group[topo_tier_group_index].tierList[i],topo_idx);
    handle_tier_addition(topo_info[topo_idx].topo_tier_group[topo_tier_group_index].tierList[i],tid);
    i++;
  }

  if(j < oldTierListCount)
  { int tid=topolib_get_tier_id_from_tier_name(oldTierList[j],topo_idx);
    handle_tier_deletion(oldTierList[j],tid);
    j++;
  }

  return;
}

//Purpose: Parse rtc message for tier group
void parse_tier_group_rtc_data(char *tool_msg, char *send_msg, int testidx)
{
  int len, opcode, tier_group_rtc_applied = 0, i, j;
  char tier_group_msg_buff[MAXIMUM_LINE_LENGTH] = {0};
  char action[32 + 1] = {0};
  char error_msg[MAX_LINE_SIZE] = {0};
  //char rtc_log_file[MAX_LINE_SIZE];
  char error_str[MAX_LINE_SIZE] = {0};
  char oldname[MAX_LINE_SIZE] = {0};
  char newname[MAX_LINE_SIZE] = {0};
  char *fields[MAX_NUM_OF_FIELDS] = {0};
  //FILE *rtclog_fp = NULL;
  char *cur_ptr = NULL;
  char *temp_ptr = NULL;
  char *old_name_ptr = NULL;
  char *oldTierList[1024]={0};
  int oldTierListCount = 0;
  char component;
  char old_component_type;
  char new_component_type;

  memcpy(&len, tool_msg, 4);
  memcpy(&opcode, tool_msg + 4, 4);
  NSDL2_MON(NULL, NULL, "Method called, msg size = %d", len);

  strncpy(tier_group_msg_buff, tool_msg + 8, len - 4);  // skip -> 4 byte for msg len and 4 byte for opcode
  NSDL2_MON(NULL, NULL, "tier_group_msg = %s", tier_group_msg_buff);

  /* Open file logs/TRxx/runtime_changes/runtime_changes.log to dump RTC logs */
  //sprintf(rtc_log_file, "%s/logs/TR%d/runtime_changes/runtime_changes.log", g_ns_wdir, testidx);

  //if((rtclog_fp = fopen(rtc_log_file, "a")) == NULL)
  //{
  //  len = snprintf(error_msg, MAX_LINE_SIZE, "parse_tier_group_rtc_data() - Error in opening file %s.", rtc_log_file);
  //}

  cur_ptr = tier_group_msg_buff;
  if((temp_ptr = strchr(tier_group_msg_buff, ';')) == NULL)
  {
    NSDL2_MON(NULL, NULL, "Invalid tier_group_msg = %s", tier_group_msg_buff);
    len = snprintf(error_msg, MAX_LINE_SIZE, "Invalid tier_group_msg = %s", tier_group_msg_buff);
  }
  else
  {
    strncpy(action, tier_group_msg_buff, temp_ptr - cur_ptr);
    cur_ptr = temp_ptr + 1;
    temp_ptr = NULL;

    if(!strcasecmp(action, "addGroup"))
    {
       if(topolib_manage_runtime_add_group(cur_ptr, error_str, &component,topo_idx) == 0)
       {
         tier_group_rtc_applied = 1;
         NSDL2_MON(NULL, NULL, "tier group added successfully at runtime. %d", component);
       }
       else
       {
         len = snprintf(error_msg, MAX_LINE_SIZE, "Failed to add tier group at runtime, Error: %s", error_str);
         NSDL2_MON(NULL, NULL, "Failed to add tier group at runtime, Error: %s", error_str);
       }
    }
    else if(!strcasecmp(action, "deleteGroup"))
    {
       
       int num_fields;
    
       if((num_fields = get_tokens_with_multi_delimiter(cur_ptr, fields, "|", MAX_NUM_OF_FIELDS)) < 2)
       {
         len = snprintf(error_msg, MAX_LINE_SIZE, "Invalid number of fileds in tier_group_msg, num_fields = %d", num_fields);
         NSDL2_MON(NULL, NULL, "Invalid number of fileds in tier_group_msg, num_fields = %d", num_fields);
       }
       else
       { 
         if(topolib_manage_runtime_delete_group(fields[0], atoi(fields[1]), error_str,topo_idx) == 0)
         {
           tier_group_rtc_applied = 1;
           NSDL2_MON(NULL, NULL, "tier group deleted successfully at runtime.");
         }
         else
         {
           len = snprintf(error_msg, MAX_LINE_SIZE, "Failed to delete tier group '%s'at runtime, Error: %s", fields[0], error_str);
           NSDL2_MON(NULL, NULL, "Failed to delete tier group '%s'at runtime, Error: %s", fields[0], error_str);
         }
       }
    }
    else if(!strcasecmp(action, "updateGroup"))
    {
      /*
       *  Format: updateGroup;oldName=CavGroup|newName=newGrpName|type=List|definition=tier1,tier2|component=2
       */

       temp_ptr = strchr(cur_ptr, '|');
       old_name_ptr = strchr(cur_ptr, '=');

      /*
       *	   	   cur_ptr	   temp_ptr	
       *	      	      v	      	      v
       *  Format: updateGroup;oldName=CavGroup|newName=newGrpName|type=List|definition=tier1,tier2|component=2
       *          		     ^	      
       *                	old_name_ptr
       *
       *  This is done to extract oldname from the message i.e CavGroup in this case it will be copied in oldname variable. 
       *    
       */
       if(temp_ptr == NULL || old_name_ptr == NULL)
       {
         len = snprintf(error_msg, MAX_LINE_SIZE, "Invalid format of tier group update message. Unable to extract old tier group name. Hence skipping processing of this message[%s]",cur_ptr);
         NSDL2_MON(NULL, NULL, "Invalid format of tier group update message. Unable to extract old tier group name. Hence skipping processing of this message[%s]",cur_ptr);
       }
       else
       {
         strncpy(oldname, old_name_ptr+1, temp_ptr-old_name_ptr-1);
       
         for(i=0;i<topo_info[topo_idx].total_tier_count;i++)
         {
           if(!strcasecmp(topo_info[topo_idx].topo_tier_group[i].GrpName, oldname))
           {
             for(j = 0; j < topo_info[topo_idx].topo_tier_group[i].total_list_count; j++)
             {
               MALLOC_AND_COPY(topo_info[topo_idx].topo_tier_group[i].tierList[j], oldTierList[j], strlen(topo_info[topo_idx].topo_tier_group[i].tierList[j]), "topo_tier_group old tier list", j);
             }
             oldTierListCount =topo_info[topo_idx].topo_tier_group[i].total_list_count;
           }
         }
       
         if(topolib_manage_runtime_update_group(cur_ptr, error_str, oldname, newname, &old_component_type, &new_component_type,topo_idx) == 0)
         {
           tier_group_rtc_applied = 1;
           handle_tier_list_update(i, oldTierList, oldTierListCount);
           NSDL2_MON(NULL, NULL, "tier group updated successfully at runtime. old component %d new component %d", old_component_type, new_component_type);
         }
         else
         {
           len = snprintf(error_msg, MAX_LINE_SIZE, "Failed to update tier group at runtime, Error: %s", error_str);
           NSDL2_MON(NULL, NULL, "Failed to update tier group at runtime, Error: %s", error_str);
         }
       
         for(j = 0; j < oldTierListCount; j++)
         {
           FREE_AND_MAKE_NULL(oldTierList[j], "topo_info[topo_idx].topo_tier_group tier list", j);
         }
         oldTierListCount = 0;
      }
    }
    else
    {
      len = snprintf(error_msg, MAX_LINE_SIZE, "Failed to perform tier group operation, Error: Invalid action/operation %s", action);
      NSDL2_MON(NULL, NULL, "Invalid action/operation %s", action);
    }
  }
/*
  len = snprintf(send_msg + 8, MAX_LINE_SIZE, "%s", tier_group_rtc_applied?"SUCCESS":error_msg);
  memcpy(send_msg + 4, &opcode, 4);
  len += 4;
  memcpy(send_msg, &len, 4);
*/
 
  snprintf(send_msg, MAX_LINE_SIZE, "%s", tier_group_rtc_applied?"SUCCESS":error_msg);
}


