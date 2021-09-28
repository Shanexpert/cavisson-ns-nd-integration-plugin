#define _GNU_SOURCE
#include <unistd.h>
#include <stdlib.h>
#include <v1/topolib_structures.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <regex.h>
#include <errno.h>
#include <curl/curl.h>
#include <time.h>
#include <string.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "nslb_util.h"
#include "nslb_cav_conf.h"
#include "nslb_alloc.h"
#include "nslb_log.h"
#include "nslb_date.h"
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
#include "ns_msg_com_util.h"

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
#include "netstorm.h"
#include "wait_forever.h"
#include "ns_exit.h"
#include "ns_ndc_outbound.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"
#include "ns_monitor_init.h"


#define MAX_MON_ID_ENTRIES 1000
#define START_NDC	   0
#define STOP_NDC	   1

//this is taken to store message to be sent to NDC if NDC is not in CONNECTED state at that time.
char *pending_messages_for_ndc_data_conn = NULL;
int pending_message_buf_size = 0;
#define DEFAULT_SIZE_FOR_PENDING_BUFFER  2*1024

//Linked list head
RestartMonList *restart_mon_list_head_ptr;

static char *error_msg[]={"Received failure from monitor/batch job", "Error in making connection to server", "Connection closed by other side", "Read error", "Send error", "Epoll error", "System error", "Timeout"};

void make_send_msg_for_monitors(CM_info *cm_info_mon_conn_ptr,int server_index, JSON_info *json_element_ptr)
{
  //ServerInfo *server_info = NULL;
  //server_info = (ServerInfo *) topolib_get_server_info();
  int delete_found=0;
  int msg_len;
  int mon_idx;
  int mon_id;
  char tmp_msg[MAX_MONITOR_BUFFER_SIZE];

  //Search out for the index which is marked deleted so that we will reuse it again
  for(mon_idx = 0; mon_idx < topo_info[topo_idx].topo_tier_info[cm_info_mon_conn_ptr->tier_index].topo_server[cm_info_mon_conn_ptr->server_index].server_ptr->topo_servers_list->total_mon_id_entries; ++mon_idx)
  {
    mon_id = topo_info[topo_idx].topo_tier_info[cm_info_mon_conn_ptr->tier_index].topo_server[cm_info_mon_conn_ptr->server_index].server_ptr->topo_servers_list->monitor_config[mon_idx].mon_id;
    if((mon_id >= 0) && (mon_id_map_table[mon_id].state == DELETED_MONITOR))
    { 
      delete_found=1;
      break;
    }
  }
  if(delete_found ==0)
  {
    //Allocate entry in monitor_config structure of server structure to store monitor init message buffer.
    add_entry_in_monitor_config(cm_info_mon_conn_ptr->server_index,cm_info_mon_conn_ptr->tier_index);    
  }
  if(json_element_ptr && !(json_element_ptr->agent_type & CONNECT_TO_NDC))
    topo_info[topo_idx].topo_tier_info[cm_info_mon_conn_ptr->tier_index].topo_server[cm_info_mon_conn_ptr->server_index].server_ptr->topo_servers_list->monitor_config[mon_idx].mon_id = g_mon_id;
  else
    topo_info[topo_idx].topo_tier_info[cm_info_mon_conn_ptr->tier_index].topo_server[cm_info_mon_conn_ptr->server_index].server_ptr->topo_servers_list->monitor_config[mon_idx].mon_id = cm_info_mon_conn_ptr->mon_id;

  //We need to pass server_display_name to NDC to handle NODE_MODIFY situations.
  if(cm_info_mon_conn_ptr->tier_name)
    msg_len = sprintf(tmp_msg, "nd_data_req:action=mon_config;server=%s%c%s;mon_id=%d;msg=", cm_info_mon_conn_ptr->tier_name, global_settings->hierarchical_view_vector_separator, cm_info_mon_conn_ptr->server_display_name, cm_info_mon_conn_ptr->mon_id);
  else
    msg_len = sprintf(tmp_msg, "nd_data_req:action=mon_config;server=%s;mon_id=%d;msg=", cm_info_mon_conn_ptr->server_display_name, cm_info_mon_conn_ptr->mon_id);

  cm_make_send_msg(cm_info_mon_conn_ptr, tmp_msg, global_settings->progress_secs, &msg_len);

  //MY_MALLOC_AND_MEMSET(*msg_buff, msg_len + 1, "Init message buffer allocation", -1);
  if(topo_info[topo_idx].topo_tier_info[cm_info_mon_conn_ptr->tier_index].topo_server[cm_info_mon_conn_ptr->server_index].server_ptr->topo_servers_list->monitor_config[mon_idx].cusMon_buff_size < (msg_len +1))
  {
    MY_REALLOC_AND_MEMSET(topo_info[topo_idx].topo_tier_info[cm_info_mon_conn_ptr->tier_index].topo_server[cm_info_mon_conn_ptr->server_index].server_ptr->topo_servers_list->monitor_config[mon_idx].init_cusMon_buff, (msg_len + 1),topo_info[topo_idx].topo_tier_info[cm_info_mon_conn_ptr->tier_index].topo_server[cm_info_mon_conn_ptr->server_index].server_ptr->topo_servers_list->monitor_config[mon_idx].cusMon_buff_size, "Init message buffer allocation", -1);

    topo_info[topo_idx].topo_tier_info[cm_info_mon_conn_ptr->tier_index].topo_server[cm_info_mon_conn_ptr->server_index].server_ptr->topo_servers_list->monitor_config[mon_idx].cusMon_buff_size = msg_len +1;
  }

  strcpy(topo_info[topo_idx].topo_tier_info[cm_info_mon_conn_ptr->tier_index].topo_server[cm_info_mon_conn_ptr->server_index].server_ptr->topo_servers_list->monitor_config[mon_idx].init_cusMon_buff, monitor_scratch_buf);

  if(mon_idx == topo_info[topo_idx].topo_tier_info[cm_info_mon_conn_ptr->tier_index].topo_server[cm_info_mon_conn_ptr->server_index].server_ptr->topo_servers_list->total_mon_id_entries)
    topo_info[topo_idx].topo_tier_info[cm_info_mon_conn_ptr->tier_index].topo_server[cm_info_mon_conn_ptr->server_index].server_ptr->topo_servers_list->total_mon_id_entries += 1;
}


void add_entry_in_monitor_config(int server_index,int tier_index)
{

  if(topo_info[topo_idx].topo_tier_info[tier_index].topo_server[server_index].server_ptr->topo_servers_list->total_mon_id_entries >=topo_info[topo_idx].topo_tier_info[tier_index].topo_server[server_index].server_ptr->topo_servers_list->max_mon_id_entries)
  {     
    MY_REALLOC_AND_MEMSET(topo_info[topo_idx].topo_tier_info[tier_index].topo_server[server_index].server_ptr->topo_servers_list->monitor_config, ((topo_info[topo_idx].topo_tier_info[tier_index].topo_server[server_index].server_ptr->topo_servers_list->max_mon_id_entries + DELTA_MON_ID_ENTRIES) * sizeof(MonitorConfig)), ((topo_info[topo_idx].topo_tier_info[tier_index].topo_server[server_index].server_ptr->topo_servers_list->max_mon_id_entries) * sizeof(MonitorConfig)), "Row memory reallocation for init buffer", -1);
        
    topo_info[topo_idx].topo_tier_info[tier_index].topo_server[server_index].server_ptr->topo_servers_list->max_mon_id_entries += DELTA_MON_ID_ENTRIES;
  }
}    


//This method was created to allocate memory to mon_id_map_table. We were setting elements of this table to -1 before allocating memory to it. This case was generated when a custom monitor was deleted, reused and then deleted in a same progress interval. In this case when we reuse monitor we allot a new mon_id to newly added monitor and max_mon_id_entries was updated during merging. If more number of monitors were deleted and reused then at some time, g_mon_id will be larger than max_mon_id_entries and when we will delete that reused monitor, it will mark its state DELETED_MONITOR at that mon_id in Mon_id_map_table which has not been reallocated. Refer Bug 40993.
int add_entry_in_mon_id_map_table()
{
  if((g_mon_id >= max_mon_id_entries) || (max_mon_id_entries == 0))
  {
    /* It is possible that (g_mon_id - max_mon_id_entries) will be greater than DELTA_MON_ID_ENTRIES */
    MY_REALLOC_AND_MEMSET_WITH_MINUS_ONE(mon_id_map_table, ((g_mon_id + DELTA_MON_ID_ENTRIES) * sizeof(MonIdMapTable)),
      (max_mon_id_entries * sizeof(MonIdMapTable)), "Reallocation of mon_id_map_table", 0);

    max_mon_id_entries = (g_mon_id + DELTA_MON_ID_ENTRIES);
  } 
  return 0;
}

void handle_reused_entry_for_CM(CM_vector_info *cus_mon_vector_ptr, int i, int server_index)
{
  CM_info *cm_info_mon_conn_ptr = cus_mon_vector_ptr[i].cm_info_mon_conn_ptr;

  add_entry_in_mon_id_map_table();
  add_entry_in_monitor_config(server_index,cm_info_mon_conn_ptr->tier_index);
  cm_info_mon_conn_ptr->mon_id = g_mon_id;
  topo_info[topo_idx].topo_tier_info[cm_info_mon_conn_ptr->tier_index].topo_server[server_index].server_ptr->topo_servers_list->monitor_config[topo_info[topo_idx].topo_tier_info[cm_info_mon_conn_ptr->tier_index].topo_server[server_index].server_ptr->topo_servers_list->total_mon_id_entries].mon_id=g_mon_id;
  make_send_msg_for_monitors(cm_info_mon_conn_ptr,server_index,NULL);

}

void mark_deleted_in_server_structure(CM_info *cus_mon_ptr, int reason)
{
  int i;
  int mon_id=cus_mon_ptr->mon_id;
  if(topo_info[topo_idx].topo_tier_info[cus_mon_ptr->tier_index].topo_server[cus_mon_ptr->server_index].used_row != -1)
  {
    for(i = 0; i < topo_info[topo_idx].topo_tier_info[cus_mon_ptr->tier_index].topo_server[cus_mon_ptr->server_index].server_ptr->topo_servers_list->total_mon_id_entries; i++)
    {
      if(topo_info[topo_idx].topo_tier_info[cus_mon_ptr->tier_index].topo_server[cus_mon_ptr->server_index].server_ptr->topo_servers_list->monitor_config[i].mon_id == mon_id)
      {
        if(reason == MON_DELETE_AFTER_SERVER_INACTIVE)
        {
          //If any monitor is deleted intentionally, then we donot need to mark it inactive, as its already been deleted.
          if((mon_id_map_table[mon_id].state != DELETED_MONITOR) || (mon_id_map_table[mon_id].state != CLEANED_MONITOR))
            mon_id_map_table[mon_id].state = INACTIVE_MONITOR;
        }
        else
          mon_id_map_table[mon_id].state = DELETED_MONITOR;
      
        //MSR->cm_index will be -1 only if there is no entry for monitor corresponding to this in cm_info_ptr
        //mon_id_map_table[mon_id].cm_index = -1;
        NSDL2_MON(NULL, NULL, "Marked DELETED_MONITOR in mon_id table for mon_id(%d) at server index '%d' in server structure for message stored at index '%d' in init_cusMon_buff",cus_mon_ptr-> mon_id, cus_mon_ptr->server_index, i);
        return;
      }
    }
  }
}

void make_pending_messages_for_ndc_data_conn(char *buffer, int size)
{
  int old_size = 0, new_size;

  if(!pending_messages_for_ndc_data_conn)
  {
    MY_MALLOC_AND_MEMSET(pending_messages_for_ndc_data_conn, DEFAULT_SIZE_FOR_PENDING_BUFFER, "Allocation for Pending buffer", 0);
    pending_message_buf_size += DEFAULT_SIZE_FOR_PENDING_BUFFER;
  }

  if(pending_messages_for_ndc_data_conn[0] != '\0')
    old_size = strlen(pending_messages_for_ndc_data_conn);

  new_size = old_size + size;
  
  if(new_size > pending_message_buf_size)
  {
    MY_REALLOC_AND_MEMSET(pending_messages_for_ndc_data_conn, (new_size + 1), pending_message_buf_size, "Reallocation of pending_messages_for_ndc_data_conn", -1);
    pending_message_buf_size = new_size + 1;
  }

  strcat(pending_messages_for_ndc_data_conn, buffer);
}


int check_and_send_pending_messages()
{
  if(!pending_messages_for_ndc_data_conn)
    return 0;

  int size = strlen(pending_messages_for_ndc_data_conn);
  if(size > 0)
  {
    if(write_msg(&ndc_data_mccptr, pending_messages_for_ndc_data_conn, size, 0, DATA_MODE) < 0)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Error in sending pending message to NDC");
      return 0;
    }
  }

  pending_messages_for_ndc_data_conn[0] = '\0';
  return 0;
}


int write_msg_for_mon(char *buf, int size, int add_in_epoll)
{
  int bytes_writen;
  char *msg_ptr;
  int new_size = 0;

  NSDL2_MESSAGES(NULL, NULL, "ndc_data_mccptr.state = %d", ndc_data_mccptr.state);
  if((buf != NULL) && (ndc_data_mccptr.state & NS_STATE_WRITING))
  {
    new_size = ndc_data_mccptr.write_offset + ndc_data_mccptr.write_bytes_remaining + size;
    if(new_size < 0)
    {
      //Buffer size exceeding the maximum value of integer ( 0x7FFFFFFF in HEX , 2147483647 in Decimal)
      //Here if flag is not set then error will be logged and flag will be set so that it can be logged only once until flag reset
      //Flag will reset when all data has been written and write_offset is zero.
      if(!(ndc_data_mccptr.flags & NS_MSG_COM_CON_BUF_SIZE_EXCEEDED))
      {
        MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Buffer size exceeding the maximum value of integer");
        MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "write_offset = %d write_bytes_remaining = %d size = %d",ndc_data_mccptr.write_offset,ndc_data_mccptr.write_bytes_remaining,size);
        ndc_data_mccptr.flags |= NS_MSG_COM_CON_BUF_SIZE_EXCEEDED;
      }
      return 0;
    }

    if ((new_size) > ndc_data_mccptr.write_buf_size)
    {
      MY_REALLOC_EX(ndc_data_mccptr.write_buf, new_size, ndc_data_mccptr.write_buf_size, "ndc_data_mccptr.write_buf increasing.", -1);
      ndc_data_mccptr.write_buf_size = new_size;
    }

    memcpy(ndc_data_mccptr.write_buf + ndc_data_mccptr.write_offset + ndc_data_mccptr.write_bytes_remaining, buf, size); /* append in the end */
    ndc_data_mccptr.write_bytes_remaining += size;

    msg_ptr = ndc_data_mccptr.write_buf;
  }
  else if(buf != NULL)
  {
    msg_ptr = buf;
    ndc_data_mccptr.write_bytes_remaining = size;
    ndc_data_mccptr.write_offset = 0;
    ndc_data_mccptr.flags &= ~NS_MSG_COM_CON_BUF_SIZE_EXCEEDED;
  }
  else
    msg_ptr = ndc_data_mccptr.write_buf;


  while (ndc_data_mccptr.write_bytes_remaining)
  {
    if (!buf) {
      NSDL2_MESSAGES(NULL, NULL, "Sending rest of the message. offset = %d, bytes_remaining = %d, %s",
                ndc_data_mccptr.write_offset, ndc_data_mccptr.write_bytes_remaining, msg_com_con_to_str(&ndc_data_mccptr));
    } else {
      NSDL2_MESSAGES(NULL, NULL, "Sending the message. bytes_remaining = %d, %s",
                      ndc_data_mccptr.write_bytes_remaining, msg_com_con_to_str(&ndc_data_mccptr));
    }

    if ((bytes_writen = write (ndc_data_mccptr.fd, msg_ptr + ndc_data_mccptr.write_offset, ndc_data_mccptr.write_bytes_remaining)) < 0)
    {
      if(errno == EAGAIN)
      {
        if (!(ndc_data_mccptr.state & NS_STATE_WRITING))
        {
          if(add_in_epoll)
          {
            MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Adding in epoll for epoll_in_out");
            //mod_select_msg_com_con_ex(__FILE__, __LINE__, (char *)__FUNCTION__, (char *)&ndc_data_mccptr, ndc_data_mccptr.fd, EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP);
            MOD_SELECT_MSG_COM_CON((char *)&ndc_data_mccptr, ndc_data_mccptr.fd, EPOLLIN | EPOLLERR | EPOLLHUP, DATA_MODE);
          }

          bcopy(msg_ptr + ndc_data_mccptr.write_offset, ndc_data_mccptr.write_buf, ndc_data_mccptr.write_bytes_remaining);
          ndc_data_mccptr.write_offset = 0;
        }

        ndc_data_mccptr.state |= NS_STATE_WRITING; // Set state to writing message
        return 0;
      }
      else
      {
        NSDL2_MESSAGES(NULL, NULL, "Error in write (MsgCom = %s) due to error = %s", msg_com_con_to_str(&ndc_data_mccptr), nslb_strerror(errno));
        NSTL1_OUT(NULL, NULL, "Error in write (MsgCom = %s) due to error = %s\n", msg_com_con_to_str(&ndc_data_mccptr), nslb_strerror(errno));
        close_msg_com_con_and_exit(&ndc_data_mccptr, (char *)__FUNCTION__, __LINE__, __FILE__);
        return -1; /* This is to handle unable to write to tools */
      }
    }
    else if (bytes_writen == 0)
    {
      NSDL2_MESSAGES(NULL, NULL, "write returned = 0 for %s", msg_com_con_to_str(&ndc_data_mccptr));
      continue;
    }

    ndc_data_mccptr.write_offset += bytes_writen;
    ndc_data_mccptr.write_bytes_remaining -= bytes_writen;
  }

  if (ndc_data_mccptr.state & NS_STATE_WRITING)
  {
    ndc_data_mccptr.state &= ~NS_STATE_WRITING;
  }
  if(add_in_epoll)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "ADDING in parent epoll after write for ndc_data_mccptr.");
   // mod_select_msg_com_con_ex(__FILE__, __LINE__, (char *)__FUNCTION__, (char *)&ndc_data_mccptr, ndc_data_mccptr.fd, EPOLLIN | EPOLLERR | EPOLLHUP);
    MOD_SELECT_MSG_COM_CON((char *)&ndc_data_mccptr, ndc_data_mccptr.fd, EPOLLIN | EPOLLERR | EPOLLHUP, DATA_MODE);
  }
  NSDL2_MESSAGES(NULL, NULL, "Exiting method");
  return 0;
}


//send_only_to_ndc : This variable is set when we are making connection to NDC at recovery and we need to send monitor config message to NDC for all monitors leaving the one using LPS. We are returning if it is called from NDC recovery code.
//parent_epoll_done : This variable is used when we need to add ndc_data_mccptr.fd in parent's epoll. When sending all the monitor message in the begigning of the test we donot need to add parent in epoll.
int make_mon_msg_and_send_to_NDC(int runtime_flag, int is_ndc_restarted, int parent_epoll_done)
{
  int i, j, ret = 0; 
  int state;
  int mon_idx = -1;
  CM_info *cm_info_mon_conn_ptr = NULL;

  //Need to reset this to start cm_dr_table from 0 index. BUG 39853
  g_total_aborted_cm_conn = 0;

  for(i=0; i< total_cm_update_monitor_entries; i++)
  {
    if(ndc_data_mccptr.fd > 0 && cm_update_monitor_buf[i])
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                       "Sending message to NDC for MON_CONFIG for monitors added at runtime. Message: %s", cm_update_monitor_buf[i]);
      if(write_msg(&ndc_data_mccptr, cm_update_monitor_buf[i], strlen(cm_update_monitor_buf[i]), 0, DATA_MODE) < 0)
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
                                                             "Error in sending message to cm_update_monitor msg on NDC data connection.");
      }
      FREE_AND_MAKE_NULL(cm_update_monitor_buf[i], "cm_update_monitor_buf", -1);

      if((i+1) == total_cm_update_monitor_entries)
        total_cm_update_monitor_entries=0;
    }
    else
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
                  "Info: Could not send monitor request for cm_update to NDC at runtime as connection with NDC is not established. Hence skipping for rest of the monitors of this server.");
      break;
    }
  }
  
  for(int tier = 0; tier < topo_info[topo_idx].total_tier_count; tier++)
  {
    for(i=0;i<topo_info[topo_idx].topo_tier_info[tier].total_server;i++)
    {
      if(!runtime_flag || is_ndc_restarted)
      {
        if((topo_info[topo_idx].topo_tier_info[tier].topo_server[i].used_row != -1) && (!topo_info[topo_idx].topo_tier_info[tier].topo_server[i].server_ptr->server_flag & DUMMY))
        {
          for(j = 0; j < topo_info[topo_idx].topo_tier_info[tier].topo_server[i].server_ptr->topo_servers_list->total_mon_id_entries; j++)
          {
            if(ndc_data_mccptr.fd > 0)
            {
              //If server is marked inactive and NDC is restarted then also we donot need to send monitor request messages.
              state = mon_id_map_table[topo_info[topo_idx].topo_tier_info[tier].topo_server[i].server_ptr->topo_servers_list->monitor_config[j].mon_id].state;
              if((state != DELETED_MONITOR) && (state != INACTIVE_MONITOR))
              {
                 MLTL1(EL_F, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, "Sending message to NDC for MON_CONFIG. Message: %s"
                 ,topo_info[topo_idx].topo_tier_info[tier].topo_server[i].server_ptr->topo_servers_list->monitor_config[j].init_cusMon_buff);
                if(write_msg_for_mon(topo_info[topo_idx].topo_tier_info[tier].topo_server[i].server_ptr->topo_servers_list->monitor_config[j].init_cusMon_buff, strlen(topo_info[topo_idx].topo_tier_info[tier].topo_server[i].server_ptr->topo_servers_list->monitor_config[j].init_cusMon_buff), parent_epoll_done) < 0)
                {
                  MLTL1(EL_F, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, "Error in sending message to server %s on NDC data connection.", topo_info[topo_idx].topo_tier_info[tier].topo_server[i].server_disp_name);
                  break;
                }  
                mon_id_map_table[topo_info[topo_idx].topo_tier_info[tier].topo_server[i].server_ptr->topo_servers_list->monitor_config[j].mon_id].state = RUNNING_MONITOR;

                mon_idx = mon_id_map_table[topo_info[topo_idx].topo_tier_info[tier].topo_server[i].server_ptr->topo_servers_list->monitor_config[j].mon_id].mon_index;
                if(mon_idx >= 0)
                  monitor_list_ptr[mon_idx].cm_info_mon_conn_ptr->conn_state = CM_RUNNING;
              }
            }
            else
            {
              MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
               "Info: Could not send monitor request for mon_id (%d) to NDC during start of test or NDC restart as connection with NDC is not established. Hence skipping for rest of the monitors of this server.", topo_info[topo_idx].topo_tier_info[tier].topo_server[i].server_ptr->topo_servers_list->monitor_config[j].mon_id);

              j = -1;
              break;
            }
          }
        }
      }
      else
      {
        if(topo_info[topo_idx].topo_tier_info[tier].topo_server[i].used_row != -1)
        { 
          for(j = 0; j < topo_info[topo_idx].topo_tier_info[tier].topo_server[i].server_ptr->topo_servers_list->total_mon_id_entries; j++)
          {
            if((ndc_data_mccptr.fd > 0) && (ndc_data_mccptr.state & NS_CONNECTED))
            {
              //We will only send monitor request to NDC which is newly applied and added in structure.
              state = mon_id_map_table[topo_info[topo_idx].topo_tier_info[tier].topo_server[i].server_ptr->topo_servers_list->monitor_config[j].mon_id].state;
              if(state == INIT_MONITOR)
              {
                MLTL1(EL_DF, 0, 0, _FLN_, cm_info_mon_conn_ptr, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                "Sending message to NDC for MON_CONFIG for monitors added at runtime. Message: %s", 
                topo_info[topo_idx].topo_tier_info[tier].topo_server[i].server_ptr->topo_servers_list->monitor_config[j].init_cusMon_buff);   
                if(write_msg(&ndc_data_mccptr, 
                  topo_info[topo_idx].topo_tier_info[tier].topo_server[i].server_ptr->topo_servers_list->monitor_config[j].init_cusMon_buff,                    strlen(topo_info[topo_idx].topo_tier_info[tier].topo_server[i].server_ptr->topo_servers_list->monitor_config[j].init_cusMon_buff), 0, DATA_MODE) < 0)
                {
                  MLTL1(EL_DF, 0, 0, _FLN_, cm_info_mon_conn_ptr, EID_DATAMON_ERROR, EVENT_INFORMATION,
                  "Error in sending message to server %s on NDC data connection.", topo_info[topo_idx].topo_tier_info[tier].topo_server[i].server_disp_name);
                  break;
                }
                //servers_list[i].runtime_monitor_offset++;
                mon_id_map_table[topo_info[topo_idx].topo_tier_info[tier].topo_server[i].server_ptr->topo_servers_list->monitor_config[j].mon_id].state = RUNNING_MONITOR;
           
                mon_idx = mon_id_map_table[topo_info[topo_idx].topo_tier_info[tier].topo_server[i].server_ptr->topo_servers_list->monitor_config[j].mon_id].mon_index;
                if(mon_idx >= 0)
                  monitor_list_ptr[mon_idx].cm_info_mon_conn_ptr->conn_state = CM_RUNNING;
              }
            }
            else
            {
              MLTL1(EL_DF, 0, 0, _FLN_, cm_info_mon_conn_ptr, EID_DATAMON_ERROR, EVENT_INFORMATION,
                 "Info: Could not send monitor request for mon_id (%d) to NDC at runtime as connection with NDC is not established. Hence skipping for rest of the monitors of this server.", topo_info[topo_idx].topo_tier_info[tier].topo_server[i].server_ptr->topo_servers_list->monitor_config[j].mon_id);
              j = -1;
              break;
            }
          }
        } 
      }
      if(j==-1) break;   //Donot want to traverse through server loop if ndc connection is closed.
    }
  } 

  //Going to make connection for LPS based monitors.
  for(i = 0; i < total_monitor_list_entries; i++)
  {
    cm_info_mon_conn_ptr = monitor_list_ptr[i].cm_info_mon_conn_ptr;

    if(cm_info_mon_conn_ptr->flags & ND_MONITOR)
    {
      NSDL4_MON(NULL, NULL, "fd = %d, flag = %d, dynamic = %d, retry count = %d", cm_info_mon_conn_ptr->fd, cm_info_mon_conn_ptr->flags, monitor_list_ptr[i].is_dynamic, cm_info_mon_conn_ptr->cm_retry_attempts);
    }
    if((cm_info_mon_conn_ptr->fd < 0) && (!(cm_info_mon_conn_ptr->flags & OUTBOUND_ENABLED))) 
    {
      if(runtime_flag)
      {
        NSDL2_MON(NULL, NULL, "cm_info_mon_conn_ptr->cm_retry_attempts = [%d]", cm_info_mon_conn_ptr->cm_retry_attempts);
        //Check if old monitor recovery count exceeds or not, if not then add for recovery
        //Runtime added monitor & old monitor not added for dr => both will have cm_retry_attempts '-1'.
        //Only old monitors added for recovery will have cm_retry_attempts greater than 0.
     
        if((cm_info_mon_conn_ptr->cm_retry_attempts > 0)) //old monitors 
        {
          if(g_total_aborted_cm_conn >= max_size_for_dr_table)
          {
            MLTL1(EL_DF, 0, 0, _FLN_, cm_info_mon_conn_ptr, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Reached maximum limit of DR table entry. So cannot allocate any more memory. Hence returning. cm_info_mon_conn_ptr->gdf_name = %s, cm_info_mon_conn_ptr->monitor_name = %s.", cm_info_mon_conn_ptr->gdf_name, cm_info_mon_conn_ptr->monitor_name);
           continue;
          }
        
          if(g_total_aborted_cm_conn >= max_dr_table_entries)
          {
            MY_REALLOC_AND_MEMSET(cm_dr_table, ((max_dr_table_entries + delta_size_for_dr_table) * sizeof(CM_info *)), (max_dr_table_entries * sizeof(CM_info *)), "Reallocation of DR table", -1);
            max_dr_table_entries += delta_size_for_dr_table;
        
            MLTL1(EL_DF, 0, 0, _FLN_, cm_info_mon_conn_ptr, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "DR table has been reallocated by DELTA(%d) entries. Now new size will %d", delta_size_for_dr_table, max_dr_table_entries);
          }
          NSDL4_MON(NULL, NULL, "Going to add monitor %s applied on %s in dr table.", cm_info_mon_conn_ptr->monitor_name, cm_info_mon_conn_ptr->server_display_name);
          cm_dr_table[g_total_aborted_cm_conn] = cm_info_mon_conn_ptr;
          g_total_aborted_cm_conn++;
     
          //If any monitor's conn_state is CM_DELETED and (retry_attempt > 0), then it means it is reused monitor.
          if(cm_info_mon_conn_ptr->conn_state == CM_DELETED)
            cm_info_mon_conn_ptr->conn_state = CM_INIT;
        }
        else if (((cm_info_mon_conn_ptr->cm_retry_attempts == -1) && (cm_info_mon_conn_ptr->flags & RUNTIME_ADDED_MONITOR)) || (is_ndc_restarted))
        {
          //ret = cm_make_nb_conn(cm_dr_table[i], 1);
          ret = cm_make_nb_conn(cm_info_mon_conn_ptr, 1);
          if(ret == -1)
          {
            cm_handle_err_case(cm_info_mon_conn_ptr, CM_NOT_REMOVE_FROM_EPOLL);
            cm_update_dr_table(cm_info_mon_conn_ptr);
          }
        }
      }
      else
      {
       if(topo_info[topo_idx].topo_tier_info[cm_info_mon_conn_ptr->tier_index].topo_server[cm_info_mon_conn_ptr->server_index].used_row != -1)
       { 
          if(topo_info[topo_idx].topo_tier_info[cm_info_mon_conn_ptr->tier_index].topo_server[cm_info_mon_conn_ptr->server_index].server_ptr->topo_servers_list->cntrl_conn_state & CTRL_CONN_ERROR)
          {
            MLTL1(EL_F, 0, 0, _FLN_, cm_info_mon_conn_ptr, EID_DATAMON_GENERAL, EVENT_INFORMATION,"This monitor return Unknown server error from gethostbyname,therefore retry has been skipped, changing its retry count to 0 and state to stopped");
            cm_info_mon_conn_ptr->cm_retry_attempts = 0;
            cm_info_mon_conn_ptr->conn_state = CM_STOPPED;
            continue;
          }

          MLTL1(EL_DF, 0, 0, _FLN_, cm_info_mon_conn_ptr, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Going to call cm_make_nb_conn() for %s", cm_info_mon_conn_ptr->monitor_name);
          ret = cm_make_nb_conn(cm_info_mon_conn_ptr, 0);
   
          if(ret < 0)
          { 
            if (!(global_settings->continue_on_mon_failure))
            {
              MLTL1(EL_F, 0, 0, _FLN_, cm_info_mon_conn_ptr, EID_DATAMON_ERROR, EVENT_MAJOR,
                                                  " Custom/Standard monitor - %s(%s) failed. Test run Canceled",
                                                  get_gdf_group_name(cm_info_mon_conn_ptr->gp_info_index), cm_info_mon_conn_ptr->monitor_name);
              NSDL3_MON(NULL, NULL, "Connection making failed for monitor %s(%s). So exitting.", get_gdf_group_name(cm_info_mon_conn_ptr->gp_info_index), cm_info_mon_conn_ptr->monitor_name);
              NS_EXIT(-1, CAV_ERR_1060014, get_gdf_group_name(cm_info_mon_conn_ptr->gp_info_index), cm_info_mon_conn_ptr->monitor_name);
            }
            MLTL1(EL_F, 0, 0, _FLN_, cm_info_mon_conn_ptr, EID_DATAMON_ERROR, EVENT_MAJOR,
                                                  " Data will not be available for this monitor.");
            NSDL3_MON(NULL, NULL, "Connection making failed on Starting of test so Retry later to make connection");
            cm_update_dr_table(cm_info_mon_conn_ptr);
            if(cm_info_mon_conn_ptr->fd)
            {
              close(cm_info_mon_conn_ptr->fd);
              cm_info_mon_conn_ptr->fd = -1;
            }
            continue;
          }
       }
      }
    }
  }
    return 1;
}

//mon_id:FTPFile:<file_name>:<chunkid>:<this_chunk_size>\n"data"
int write_in_ftp_file(CheckMonitorInfo *check_monitor_ptr, char *buffer, int bytes_to_write)
{
  if(check_monitor_ptr->monitor_type == INTERNAL_CHECK_MON)
  {
    strncat(topo_info[topo_idx].topo_tier_info[check_monitor_ptr->tier_idx].topo_server[check_monitor_ptr->topo_server_info_idx].server_ptr->server_sig_output_buffer, buffer, bytes_to_write);
  }
  else
  {
    if(fwrite(buffer, sizeof(char), bytes_to_write, check_monitor_ptr->ftp_fp) <= 0)
    {
      ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
                                EID_CHKMON_ERROR, EVENT_MAJOR,
                                  "CheckMonitor = '%s', Error: Can not write to ftp file.",
                                 check_monitor_ptr->check_monitor_name);
      mark_check_monitor_fail_for_outbound(check_monitor_ptr, CHECK_MONITOR_SYSERR, &num_pre_test_check, &num_post_test_check);
      return FAILURE;
    }

    check_monitor_ptr->ftp_file_size -= bytes_to_write;
  }

  return SUCCESS;
}

int find_chk_mon_from_idx(int mon_id, int chk_mon_state)
{
  int i;
  if(chk_mon_state == 1)
  {
    for (i = 0; i < total_pre_test_check_entries; i++)
    {
      if(mon_id == pre_test_check_info_ptr[i].mon_id)
        return i;
    }
  }

  else
  {
    for (i = 0; i < total_check_monitors_entries; i++)
    {
      if(mon_id == check_monitor_info_ptr[i].mon_id)
        return i;
    }
  }
  return -1;
}


int complete_ftp_for_chk_mon(int mon_id, int bytes_to_write, int chk_mon_phase, char *buffer, int *chk_mon_ftp_state)
{
  
  CheckMonitorInfo *check_monitor_ptr;
 
  int chk_mon_idx = find_chk_mon_from_idx(mon_id, chk_mon_phase);

  if(chk_mon_phase == CHECK_MONITOR_EVENT_BEFORE_TEST_IS_STARTED)
    check_monitor_ptr = &pre_test_check_info_ptr[chk_mon_idx];
  else
    check_monitor_ptr = &check_monitor_info_ptr[chk_mon_idx];

  if(write_in_ftp_file(check_monitor_ptr, buffer, bytes_to_write) == SUCCESS)
  {
    if(check_monitor_ptr->ftp_file_size == 0)
    {
      if(check_monitor_ptr->monitor_type == INTERNAL_CHECK_MON)
        process_data(check_monitor_ptr);

      *chk_mon_ftp_state = 0;

      close_ftp_file(check_monitor_ptr);
      return CHK_MON_FTP_COMPLETE;
    } 
  }
  return 0;
}

void set_nan_for_one_custom_monitor(CM_info *cus_mon_ptr)
{
  int vec_idx, data_idx;

  CM_vector_info *vector_list = NULL;

  vector_list = cus_mon_ptr->vector_list;
  for(vec_idx = 0; vec_idx < cus_mon_ptr->total_vectors; vec_idx++)
  {
    vector_list[vec_idx].flags &= ~DATA_FILLED;
    vector_list[vec_idx].vector_state = CM_VECTOR_RESET;

    if(vector_list[vec_idx].data == NULL)
    {
      MLTL2(EL_F, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFORMATION, "Data element which are to be set NAN is NULL for this vector %s", vector_list[vec_idx].vector_name); 
      continue;
    } 
   
    for(data_idx = 0; data_idx < cus_mon_ptr->no_of_element; data_idx++)
      vector_list[vec_idx].data[data_idx] = 0.0/0.0;
  }
  
  if(cus_mon_ptr->dvm_cm_mapping_tbl_row_idx >= 0)
  {
    MLTL2(EL_F, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFORMATION, "Going to memset dvm_idx_mapping_ptr for %s",
                                                                                   cus_mon_ptr->gdf_name_only);
    memset(dvm_idx_mapping_ptr[cus_mon_ptr->dvm_cm_mapping_tbl_row_idx], -1,
           cus_mon_ptr->max_mapping_tbl_vector_entries * sizeof(DvmVectorIdxMappingTbl));
  }
}

void set_nan_for_all_custom_monitor()
{ 
  CM_info *temp_ptr = NULL;
  int mon_idx;
  if(is_outbound_connection_enabled == 1)
  { 
    for(mon_idx=0; mon_idx<total_monitor_list_entries; mon_idx++)
    {
      temp_ptr = monitor_list_ptr[mon_idx].cm_info_mon_conn_ptr;
      if(!((temp_ptr->flags & ND_MONITOR) || (temp_ptr->flags & USE_LPS)))
      {	
        set_nan_for_one_custom_monitor(temp_ptr);
      }	
    }
  } 
}

int handle_monitor_close_from_NDC(char *buffer)
{
  char *mon_id_ptr = NULL, *colon_ptr = NULL;  
  char *field[MAX_MON_ID_ENTRIES];
  int total_ids, i, mon_id, cm_idx;
  short state;

  mon_id_ptr = strstr(buffer, "mon_id=");
  if(!mon_id_ptr)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Wrong format of buffer received from NDC. No 'mon_id=' fouund. Buffer = %s", buffer);
    return -1;
  } 
    
  colon_ptr = strstr(mon_id_ptr + 1, ";");
  if(!colon_ptr)
  { 
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
              "Wrong format of buffer received from NDC. No ';' fouund. Buffer = %s", buffer);
    return -1;
  }

  *colon_ptr = '\0';

  total_ids = get_tokens((mon_id_ptr + 7), field, ",", MAX_MON_ID_ENTRIES);

  for(i = 0; i < total_ids; i++)
  {
    if(ns_is_numeric(field[i]))
      mon_id = atoi(field[i]);
    else
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_INV_DATA, EVENT_INFORMATION,
              "Wrong mon_id received. Mon_id = %s", field[i]);
      continue;
    }

    cm_idx = mon_id_map_table[mon_id].mon_index;
    state = mon_id_map_table[mon_id].state;

    if((state == DELETED_MONITOR) || (state == INACTIVE_MONITOR) || (state == CLEANED_MONITOR) || (cm_idx < 0))
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Monitor for mon_id %d is -1. It may have been deleted earlier.", mon_id);
      continue;
    }
    set_nan_for_one_custom_monitor(monitor_list_ptr[cm_idx].cm_info_mon_conn_ptr);
  }
  return 0;
}

void delete_from_restart_monitor_list(RestartMonList **prev, RestartMonList **curr)
{
  RestartMonList *tmp_node;

  if(*prev == NULL)
    restart_mon_list_head_ptr = (*curr)->next;
  else
    (*prev)->next = (*curr)->next;

  tmp_node = *curr;
  (*curr) = (*curr)->next;

  NSTL3(NULL, NULL, "deleted mon_id %d from the linked list to restart monitor", tmp_node->mon_id);
  free(tmp_node);
}

void resend_monitor_request_to_NDC(CM_info * cus_mon_ptr)
{
  //ServerInfo *servers_list = NULL;
  //servers_list = (ServerInfo *) topolib_get_server_info();
  int i;
  char *msg_buff;

  NSDL1_MON(NULL, NULL, "Method Called");
  for(i = 0; i < topo_info[topo_idx].topo_tier_info[cus_mon_ptr->tier_index].topo_server[cus_mon_ptr->server_index].server_ptr->topo_servers_list->total_mon_id_entries; i++)
  {
    if(topo_info[topo_idx].topo_tier_info[cus_mon_ptr->tier_index].topo_server[cus_mon_ptr->server_index].server_ptr->topo_servers_list->monitor_config[i].mon_id == cus_mon_ptr->mon_id)
    {
      msg_buff = topo_info[topo_idx].topo_tier_info[cus_mon_ptr->tier_index].topo_server[cus_mon_ptr->server_index].server_ptr->topo_servers_list->monitor_config[i].init_cusMon_buff;
      if(write_msg(&ndc_data_mccptr, msg_buff, strlen(msg_buff), 0, DATA_MODE) < 0)
      {
        ns_cm_monitor_log(EL_F, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFORMATION, "Error in sending cm_init_monitor message to NDC for Mon id = %d. Msg = %s, Error = %s", cus_mon_ptr->mon_id, msg_buff, nslb_strerror(errno));
        make_pending_messages_for_ndc_data_conn(msg_buff, strlen(msg_buff));

        return;
      }
      ns_cm_monitor_log(EL_F, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFORMATION, "cm_init_monitor message sent to NDC for Mon id = %d. Msg = %s", cus_mon_ptr->mon_id, msg_buff);
      mon_id_map_table[cus_mon_ptr->mon_id].state = RUNNING_MONITOR;

      cus_mon_ptr->conn_state = CM_RUNNING;
      break;
    }
  }
  NSDL1_MON(NULL, NULL, "Method END");
}

//This function is used in OUTBOUND MODE to close the connection and restart the monitor.
void check_and_send_mon_request_to_start_monitor()
{
  NSDL2_MON(NULL, NULL, "Method  Called. head_ptr for retsart_monitor_list = %p", restart_mon_list_head_ptr);
  int mon_id, cm_index = -1;
  CM_info *cm_info_mon_conn_ptr = NULL;
  RestartMonList *curr = restart_mon_list_head_ptr;
  RestartMonList *prev = NULL;

  while(curr != NULL)
  {
    mon_id = curr->mon_id;
    cm_index = mon_id_map_table[mon_id].mon_index;
    if(cm_index < 0)
    {
      NSTL1(NULL, NULL, "Found negative cm_index while accessing from linked list for mon_id = %d. Hence returning.", mon_id);
      delete_from_restart_monitor_list(&prev, &curr);
      continue;
    }

    cm_info_mon_conn_ptr = monitor_list_ptr[cm_index].cm_info_mon_conn_ptr;

    //NOTE: RESTART_MON_ON_INVALID_VECTOR state is set when data is coming as id|data in the first sample and in that case we are closing the monitor connection in INBOUND but in OUTBOUND mode we need to send end_monitor NDC. We have already sent end_monitor  after returning from filldata(). 
    //We need to send the monitor request to NDC again, so that data start coming with vector name. We are skipping one sample to send the monitor request, and it because some data from the previous monitor might be stuck on the socket stream. If that data is processed again, it willagain close the connection and resend the monitor request.
    //Now we are not sending the monitor request in the next sample, we are skipping for one sample. For permanent fix we will be adding this monitor as a new one. But this can happen very frequently so mon_id will be exhausted if we will be adding a new monitor every time. So we need to fix the mon_id first. We need to comeup with a solution to reuse mon_id. 
    NSDL2_MON(NULL, NULL, "mon_id_map_table[mon_id].state = %d, mon_id_map_table[mon_id].retry_count = %d, cm_info_mon_conn_ptr->mon_name = %s", mon_id_map_table[mon_id].state, mon_id_map_table[mon_id].retry_count, cm_info_mon_conn_ptr->monitor_name);

    if(mon_id_map_table[mon_id].state == RESTART_MON_ON_INVALID_VECTOR)
    {
      if(mon_id_map_table[mon_id].retry_count == 1)
      {
        resend_monitor_request_to_NDC(cm_info_mon_conn_ptr);
        mon_id_map_table[mon_id].retry_count = 0;
        delete_from_restart_monitor_list(&prev, &curr);
        continue;
      }
      else
        mon_id_map_table[mon_id].retry_count ++;
    }
    prev = curr;
    curr = curr->next;
  }
}

//Function to insert nodein a linked list
void insert_into_restart_monitor_list(int mon_id)
{
  RestartMonList *node;

  MY_MALLOC_AND_MEMSET(node, sizeof(RestartMonList), "One node of RestartMonList", -1);
  node->mon_id = mon_id;

  NSTL3(NULL, NULL, "entry for mon_id %d \n",mon_id);

  node->next = restart_mon_list_head_ptr;
  restart_mon_list_head_ptr = node;
}

//id:vector_name|data
int check_vector_name_present(char *buf)
{
  char *ptr;

  //If : is not present in vector then return -1
  if((ptr=strchr(buf,':')) != NULL)
      return 0;
  return -1;
}


//mon_id:v_id:v_name|data
int receive_data_from_NDC(int chk_mon_phase)
{
  int mon_id;
  int ret=0;
  char *new_line_ptr = NULL, *mon_id_ptr = NULL;
  char *buffer;
  int cm_idx = -1, bytes_read = -1, ndc_node_msg_row = 0;
  int bytes_remaining;
  short state;
 
  while(1)
  {
    bytes_read = recv(ndc_data_mccptr.fd, (ndc_data_mccptr.read_buf + ndc_data_mccptr.read_offset), (ndc_data_mccptr.read_buf_size - ndc_data_mccptr.read_offset), 0);

    NSDL3_MON(NULL, NULL, "bytes_read = %d, message received: %s, ndc_data_mccptr.read_offset = %d", bytes_read, (ndc_data_mccptr.read_buf + ndc_data_mccptr.read_offset), ndc_data_mccptr.read_offset);
    if(bytes_read < 0)
    {
      if(errno == EINTR)
      {
        NSDL2_MON(NULL, NULL, "Interrupted. Continuing...");
        continue;
      }
      else if(errno == EAGAIN)
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Received EAGAIN while reading data form NDC");
        return SUCCESS;
      }
      else
      {
         MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Error in receiving data from NDC. Error: %s, ndc_data_mccptr.read_buf = %p, ndc_data_mccptr.read_offset = %d", nslb_strerror(errno), ndc_data_mccptr.read_buf, ndc_data_mccptr.read_offset);
         return FAILURE;
      }
    }
    else if(bytes_read == 0)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Connection closed from other side. Going to clean ndc_data_mccptr structure.");

      Msg_com_con *m = &ndc_data_mccptr;
      CLOSE_MSG_COM_CON(m, DATA_MODE);
      set_nan_for_all_custom_monitor();
      return FAILURE;
    }
    else
    {
      // terminate buffer
      bytes_read += ndc_data_mccptr.read_offset;
      ndc_data_mccptr.read_offset = 0;
      ndc_data_mccptr.read_buf[bytes_read] = '\0';
      buffer = ndc_data_mccptr.read_buf;
      bytes_remaining = bytes_read;

      while(1)
      { 
        new_line_ptr = strstr(buffer, "\n");

	if(buffer == new_line_ptr)
	{
          MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, "Blank Newline Received ... Ignoring");
          bytes_remaining = bytes_remaining - (new_line_ptr - buffer);
          buffer = new_line_ptr + 1;
	}
	else if(new_line_ptr)
        {
          mon_id = -1;
          *new_line_ptr = '\0';
          MLTL4(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Received data from NDC -> %s", buffer);

          if(*(char *) buffer == 'n')
          {
            if(!strncmp(buffer, START_REPLY_FROM_NDC, strlen(START_REPLY_FROM_NDC))) 
            {
              MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Received message from NDC: %s.", buffer);
              ndc_data_mccptr.state |= NS_DATA_CONN_MADE;
              buffer = new_line_ptr + 1;

              //This method will check if there is any content in pending messages buffer and will send messages to NDC accordingly.
              check_and_send_pending_messages();
              continue;
            }
            else if(!strncmp(buffer, STOP_REPLY_FROM_NDC, strlen(STOP_REPLY_FROM_NDC))) 
            {
              MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                   "Received message from NDC: %s. Going to close NS-NDC data connection.", buffer);

              Msg_com_con *m = &ndc_data_mccptr;
              CLOSE_MSG_COM_CON(m, DATA_MODE);
              return 0;
            }
            else if(strncmp(buffer, "nd_data_rep:action=monitor_connection_close;", 44) == 0)
            {
              MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
               "Received message from NDC: %s", buffer);

	      if(handle_monitor_close_from_NDC(buffer) < 0)
                return 0;
            }
            else if(!strncmp(buffer, "ndc_node_discovery_ctrl_msg:", 28) || !strncmp(buffer, "ndc_node_end_mon:", 17) || !strncmp(buffer, "ndc_node_modify:", 16) || !strncmp(buffer, "ndc_server_inactive:", 20) || !strncmp(buffer, "ndc_instance_inactive:", 22) || !strncmp(buffer, "ndc_instance_delete:", 20) || !strncmp(buffer, "ndc_server_delete:", 18) || !strncmp(buffer, "ndc_instance_down:", 18) || !strncmp(buffer, "ndc_instance_up:", 16) || !(strncmp(buffer,"ndc_cavmon_msg:",15)))
            {
              MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Received message from NDC: %s", buffer);
              create_table_entry_dvm(&ndc_node_msg_row, &total_ndc_node_msg, &max_ndc_node_msg, (char **)&ndc_node_msg, sizeof(char*), "Node Discovery Message");
              MY_MALLOC(ndc_node_msg[ndc_node_msg_row], strlen(buffer) + 1, "NDC Node Msg", ndc_node_msg_row);
              strcpy(ndc_node_msg[ndc_node_msg_row], buffer);
             }
             else if(strstr(buffer, "result=ERROR") != NULL)
             {
               MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                 "Received Error on NDC data connection. Message = %s", buffer);

               Msg_com_con *m = &ndc_data_mccptr;
               CLOSE_MSG_COM_CON(m, DATA_MODE);
               NS_EXIT(-1, CAV_ERR_1060015, buffer);
             }
            buffer = new_line_ptr + 1;
            continue;
          }
          
          mon_id_ptr = strstr(buffer, ":");
 
          if(mon_id_ptr)
          {  
            *mon_id_ptr = '\0';
            if(ns_is_numeric((buffer)))
              mon_id = atoi(buffer); 

            /* g_all_mon_id is global variable and it will be increased every time a new monitor is added***.
               And mon_id which will be coming from cmon side will always be less than g_all_mon_id. It will be increased by 1 only */
            if(mon_id > g_all_mon_id || mon_id < 0)
            {
              MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                    "Received invalid mon_id from NDC. mon_id = %d, Buffer=%s", mon_id, buffer);
              buffer = new_line_ptr + 1;
              continue;
            }           
           

            cm_idx = mon_id_map_table[mon_id].mon_index;
            state = mon_id_map_table[mon_id].state;

            if((state == INACTIVE_MONITOR) || (state == DELETED_MONITOR) || (state == CLEANED_MONITOR) || (cm_idx < 0))
            {
              MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                      "Received Mon_id = %d, state = %d, cm_index = %d. Either this monitor is deleted or cm_idx is not updated in mon_id_map_table or cm_idx is negative. So Ignoring this line", mon_id, state, cm_idx);
	            buffer = new_line_ptr + 1;
              continue;
            }
       
            buffer = mon_id_ptr + 1;
	    int length = strlen(buffer) + 1;
            NSTL2(NULL, NULL, "dbuf_len[%d]", strlen(buffer));
 
            strcpy(monitor_list_ptr[cm_idx].cm_info_mon_conn_ptr->data_buf, buffer);
 
            //id|data
            //In case of SM and PERIPHERAL MONITOR we dont need to dump data in tg the data will dump in csv So, no need to check that vector
            // whether vector name is present or not
            if(!(monitor_list_ptr[cm_idx].cm_info_mon_conn_ptr->flags & NA_WMI_PERIPHERAL_MONITOR || 
               monitor_list_ptr[cm_idx].cm_info_mon_conn_ptr->gdf_flag == NA_SM_GDF))
            {
              if((monitor_list_ptr[cm_idx].cm_info_mon_conn_ptr->conn_state != CM_VECTOR_RECEIVED) &&
                 (monitor_list_ptr[cm_idx].cm_info_mon_conn_ptr->conn_state == CM_RUNNING) &&
                 (monitor_list_ptr[cm_idx].cm_info_mon_conn_ptr->flags & DYNAMIC_MONITOR))
              {
                if((check_vector_name_present(buffer) == -1))
                {
                  MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                        "Received Mon_id = %d, state = %d, cm_index = %d. Received first time data without vector name. GDF=%s Buffer= %s",
                        mon_id, state, cm_idx, monitor_list_ptr[cm_idx].cm_info_mon_conn_ptr->gdf_name, buffer);
                  ret = INVALID_VECTOR;
                }
                else
                {
                  MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                          "Received Mon_id = %d, state = %d, cm_index = %d. Received first time data with vector name. GDF=%s Buffer= %s",
                           mon_id, state, cm_idx, monitor_list_ptr[cm_idx].cm_info_mon_conn_ptr->gdf_name, buffer);
                }
                monitor_list_ptr[cm_idx].cm_info_mon_conn_ptr->conn_state = CM_VECTOR_RECEIVED;
              }
            }
            
            //if we receive first time a vector without vector name of a monitor then we dont need to call filldata 
            if(ret != INVALID_VECTOR)
            {
              MLTL3(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                  "Sending data line %s to filldata()", buffer);
              ret = filldata(monitor_list_ptr[cm_idx].cm_info_mon_conn_ptr, length);
            }

            //Here we are adding INVALID_VECTOR_RECEIVED flags for those monitor in which we recieve INVALID vector.
            //Old Design we are sending end_monitor multiple times as much we have vector received for the same monitor
            //count of end_monitor send == no of Invalid vector received for same monitor.
            //New Design we are set flag INVALID_VECTOR_RECEIVED first time we receive INVALID vector.
            // and now for other invalid vector received for same monitor we dont call the stop_one_custom_monitor func.
            if(ret == INVALID_VECTOR)
            {
              if((monitor_list_ptr[cm_idx].cm_info_mon_conn_ptr->conn_state == CM_VECTOR_RECEIVED) &&
                 (monitor_list_ptr[cm_idx].cm_info_mon_conn_ptr->flags & DYNAMIC_MONITOR))
              {
                //Here we are sending reason MON_STOP_ON_RECEIVE_ERROR So that dvm_mapping_ptr will me memset and
                //vector state will be set VECTOR_RESET.
                stop_one_custom_monitor(monitor_list_ptr[cm_idx].cm_info_mon_conn_ptr, MON_STOP_ON_RECEIVE_ERROR);
                //Here we are setting conn_state CM_STOPPED So that we can not send multiple end_monitor for same monitor
                //For now we are setting Here, But in future we will do it in handle_monitor_stop So that inbound case will also handle
                monitor_list_ptr[cm_idx].cm_info_mon_conn_ptr->conn_state = CM_STOPPED;
                insert_into_restart_monitor_list(mon_id);
                mon_id_map_table[mon_id].state = RESTART_MON_ON_INVALID_VECTOR;
                NSDL2_MON(NULL, NULL, "Found an invalid vector entry for monitor %s, mon_id = %d. Added in linked list",
                                       monitor_list_ptr[cm_idx].cm_info_mon_conn_ptr->monitor_name, mon_id);
              }
            }
            ret=0;
          }
          else
          {
             MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
                   "Wrong format of data receveid. Data line : %s", buffer);
          }
     
          bytes_remaining = bytes_remaining - (new_line_ptr - buffer);
          buffer = new_line_ptr + 1;
        }
        else
        {
          break;       
        }
      }

      int bytes_processed = buffer - ndc_data_mccptr.read_buf;

      if(bytes_processed)
      {
        ndc_data_mccptr.read_offset = bytes_read - bytes_processed;

	if(ndc_data_mccptr.read_offset)
          memmove(ndc_data_mccptr.read_buf, buffer, ndc_data_mccptr.read_offset);
      }
      else
      {
        ndc_data_mccptr.read_offset = bytes_read;

	//Realloc ndc_data_mccptr.read_buf if (bytes_remaining + 1024 Bytes > ndc_data_mccptr.read_buf_size)
	if((bytes_remaining + 1024) > ndc_data_mccptr.read_buf_size)
	{
	  ndc_data_mccptr.read_buf_size = ndc_data_mccptr.read_buf_size +  MAX_BUFFER_SIZE_FOR_MONITOR;
	  MY_REALLOC(ndc_data_mccptr.read_buf, ndc_data_mccptr.read_buf_size + 1, "ndc_data_mccptr.read_buf", -1);

          NSTL1_OUT(NULL, NULL, "Going to realloc ndc_data_mccptr.read_buf with size = %d", ndc_data_mccptr.read_buf_size);
	}
      }
      return SUCCESS;
    }
  }
}


inline void mark_check_monitor_fail_for_outbound(CheckMonitorInfo *check_monitor_ptr, int status, int *num_pre_test_check, int *num_post_test_check)
{
  char ChkMon_BatchJob[50];

  NSDL2_MON(NULL, NULL, "Method called. CheckMonitor => %s, status = %d", CheckMonitor_to_string(check_monitor_ptr, s_check_monitor_buf), status);

  if(check_monitor_ptr->bj_success_criteria > 0) 
    strcpy(ChkMon_BatchJob, "Batch Job");
  else
    strcpy(ChkMon_BatchJob, "Check Monitor");
  
  NSDL2_MON(NULL, NULL, "ChkMon_BatchJob = %s", ChkMon_BatchJob);

  if(check_monitor_ptr->from_event == CHECK_MONITOR_EVENT_BEFORE_TEST_IS_STARTED) 
    *num_pre_test_check = (*num_pre_test_check) - 1;
  else if(check_monitor_ptr->from_event == CHECK_MONITOR_EVENT_AFTER_TEST_IS_OVER)
    *num_post_test_check = (*num_post_test_check) - 1;

  check_monitor_ptr->status = status;
  
  ns_check_monitor_log(EL_CDF, DM_EXECUTION, MM_MON, _FLN_, check_monitor_ptr,
                               EID_CHKMON_ERROR, EVENT_MAJOR,
                               "%s failed - %s: %s",
                               ChkMon_BatchJob, check_monitor_ptr->check_monitor_name, error_msg[status]);
}


inline void mark_check_monitor_pass_for_outbound(CheckMonitorInfo *check_monitor_ptr, int status, int *num_pre_test_check, int *num_post_test_check)
{
  char ChkMon_BatchJob[50];

  NSDL2_MON(NULL, NULL, "Method called. CheckMonitor => %s, status = %d", check_monitor_ptr->check_monitor_name, status);

  if(check_monitor_ptr->bj_success_criteria > 0)
    strcpy(ChkMon_BatchJob, "Batch Job");
  else
    strcpy(ChkMon_BatchJob, "Check Monitor");

  NSDL2_MON(NULL, NULL, "ChkMon_BatchJob = %s", ChkMon_BatchJob);

  check_monitor_ptr->status = status;

  ns_check_monitor_log(EL_DF, DM_EXECUTION, MM_MON, _FLN_, check_monitor_ptr,
                                             EID_CHKMON_GENERAL, EVENT_INFO,
                                             "%s passed - %s",
                                             ChkMon_BatchJob, check_monitor_ptr->check_monitor_name);

  #ifdef NS_DEBUG_ON
    printf("%s passed - %s\n", ChkMon_BatchJob, check_monitor_ptr->check_monitor_name);
  #endif

  if(check_monitor_ptr->option == RUN_EVERY_TIME_OPTION) //Run periodic
  {
    if(check_monitor_ptr->max_count == -1)
    {
      if(strcmp(check_monitor_ptr->end_phase_name, "")==0)
      {
        ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
				  EID_CHKMON_GENERAL, EVENT_INFO,
				  "Since %s '%s' is running periodically till test is over,"
				  " so connection not closed. Returning...",
				  ChkMon_BatchJob, check_monitor_ptr->check_monitor_name);
      }
      else
      {
        ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
                                  EID_CHKMON_GENERAL, EVENT_INFO,
                                  "Since %s '%s' is running periodically till %s phase is over,"
                                  " so connection not closed. Returning...",
                                  ChkMon_BatchJob, check_monitor_ptr->check_monitor_name,check_monitor_ptr->end_phase_name); 
      }
      return;
    }
    check_monitor_ptr->max_count--;
    if(check_monitor_ptr->max_count > 0)
    {
      ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
				  EID_CHKMON_GENERAL, EVENT_INFO,
				  "Since %s '%s' is running periodically and"
				  " repeat count left (%d) is not 0, so connection not closed. Returning...",
				  ChkMon_BatchJob, check_monitor_ptr->check_monitor_name, check_monitor_ptr->max_count);
      return;
    }
    ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
					  EID_CHKMON_GENERAL, EVENT_INFO,
					  "%s '%s' repeat count left 0, so connection is closed.",
					  ChkMon_BatchJob, check_monitor_ptr->check_monitor_name, check_monitor_ptr->max_count);
  }

  if(check_monitor_ptr->from_event == CHECK_MONITOR_EVENT_BEFORE_TEST_IS_STARTED)
    *num_pre_test_check = (*num_pre_test_check) - 1;
  else if(check_monitor_ptr->from_event == CHECK_MONITOR_EVENT_AFTER_TEST_IS_OVER)
    *num_post_test_check = (*num_post_test_check) - 1;
}


void wait_for_post_check_results_for_outbound()
{
  int cnt, i;
  int epoll_time_out;
  struct epoll_event *epev = NULL;
  CheckMonitorInfo *check_monitor_ptr = NULL;
  
  epoll_time_out = post_test_check_timeout;

  NSDL2_MON(NULL, NULL, "Method called. num_post_test_check = %d, epoll_timeout in sec = %d", num_post_test_check, epoll_time_out);

  MY_MALLOC(epev, sizeof(struct epoll_event), "CheckMonitor epev", -1);

  while(1)
  {
    if(num_post_test_check == 0)
    {
      NSDL2_MON(NULL, NULL, "Check monitor done.");
      break;
    }
    memset(epev, 0, sizeof(struct epoll_event));
    NSDL2_MON(NULL, NULL, "Wait for an I/O event on an epoll file descriptor epoll_fd = %d, num_post_test_check = %d, epoll_time_out = %d", epoll_fd, num_post_test_check, epoll_time_out);

    cnt = epoll_wait(epoll_fd, epev, num_post_test_check, epoll_time_out * 1000);
    NSDL4_MON(NULL, NULL, "epoll wait return value = %d", cnt);

    if (cnt > 0)
    {
      for (i = 0; i < cnt; i++)
      {
        if (epev[i].events & EPOLLERR) 
          mark_check_monitor_fail_for_outbound(check_monitor_ptr, CHECK_MONITOR_EPOLLERR, &num_pre_test_check, &num_post_test_check);
        else if (epev[i].events & EPOLLIN)
          receive_data_from_NDC(CHECK_MONITOR_EVENT_AFTER_TEST_IS_OVER);
        else if(epev[i].events & EPOLLOUT)
          write_msg_for_mon(NULL, 0, 1);
        else
        {
          ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
				      EID_CHKMON_ERROR, EVENT_MAJOR,
				      "Event failed for check monitor '%s'. epev[i].events = %d err = %s",
				      check_monitor_ptr->check_monitor_name, epev[i].events, nslb_strerror(errno));
          mark_check_monitor_fail_for_outbound(check_monitor_ptr, CHECK_MONITOR_SYSERR, &num_pre_test_check, &num_post_test_check);
        }
      }
    } 
    else if(flag_run_time_changes_called)
    {
      flag_run_time_changes_called = 0;  
      NSDL2_MON(NULL, NULL, "Got event on epoll, but it was for runtime changes.Continuing..");
      continue;
    }
    else //Timeout while waiting for check monitor results
    {
      abort_post_test_checks(cnt, nslb_strerror(errno));
      break;
    }
  }
  if (rfp) fflush(rfp);
  FREE_AND_MAKE_NULL(epev, "epev", -1);
}


void wait_for_pre_test_chk_for_outbound()
{
  int cnt, i;
  int epoll_time_out;
  struct epoll_event *epev = NULL;
  CheckMonitorInfo *check_monitor_ptr = pre_test_check_info_ptr;

  epoll_time_out = pre_test_check_timeout;

  NSDL2_MON(NULL, NULL, "Method called. num_pre_test_check = %d, epoll_timeout in sec = %d", num_pre_test_check, epoll_time_out);

  MY_MALLOC(epev, sizeof(struct epoll_event), "CheckMonitor epev", -1);

  while(1)
  {
    if(num_pre_test_check == 0)
    {
      NSDL2_MON(NULL, NULL, "Check Monitor/Batch Job done.");
      break;
    }
    memset(epev, 0, sizeof(struct epoll_event));
    cnt = epoll_wait(epoll_fd, epev, num_pre_test_check, epoll_time_out * 1000);
    NSDL4_MON(NULL, NULL, "epoll wait return value = %d", cnt);

    if (cnt > 0)
    {
      for (i = 0; i < cnt; i++)
      {
        check_monitor_ptr = (CheckMonitorInfo *)epev[i].data.ptr;
        if (epev[i].events & EPOLLERR) 
          mark_check_monitor_fail_for_outbound(check_monitor_ptr, CHECK_MONITOR_EPOLLERR, &num_pre_test_check, &num_post_test_check);
        else if (epev[i].events & EPOLLIN)
          receive_data_from_NDC(CHECK_MONITOR_EVENT_BEFORE_TEST_IS_STARTED);
        else if (epev[i].events & EPOLLOUT)    //for NoN-Blocking Code
          write_msg_for_mon(NULL, 0, 1);
        else
        {
          ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
				      EID_CHKMON_ERROR, EVENT_MAJOR,
				      "Event failed for Check Monitor/Batch Job '%s'. epev[i].events = %d err = %s",
				       check_monitor_ptr->check_monitor_name, epev[i].events, nslb_strerror(errno));
          mark_check_monitor_fail_for_outbound(check_monitor_ptr, CHECK_MONITOR_SYSERR, &num_pre_test_check, &num_post_test_check);
        }
      }
    } 
    else if(flag_run_time_changes_called)
    {
      flag_run_time_changes_called = 0;  
      NSDL2_MON(NULL, NULL, "Got event on epoll, but it was for runtime changes.Continuing..");
      continue;
    }
    else //Timeout while waiting for check monitor results
    {
      abort_pre_test_checks(cnt, nslb_strerror(errno));
      break;
    }
  }
  if (rfp) fflush(rfp);
  FREE_AND_MAKE_NULL(epev, "epev", -1);
}


int process_data_from_NDC_for_chk_mon(char *cmd_line, CheckMonitorInfo *check_monitor_ptr, int *chunk_size, int *chk_mon_ftp_state, int *chk_mon_id_for_ftp)
{
  int total_flds;
  char *field[10];
  int chunk_id;
  char tmp_cmd_line[1024];

  //If a FTPFile: type check monitor comes, its data will come in following format.
  //monid:FTPFile:<file_name>:<file_size>\n
  //We will process this line and get ftp_file_size and we will set state to FTP_STATE. And data will be coming from next line in chunks and each chunk will have its seperate id and size.
  //FTPFile:<file_name>:<chunk_id>:<chunk_size>:data
  //We will read data of chunk size, and will ignore any '\n' or anything that appeears in data.
  if(!strncmp(cmd_line, CHECK_MONITOR_FTP_FILE, strlen(CHECK_MONITOR_FTP_FILE)))
  {
    if(check_monitor_ptr->state == CHECK_MON_FTP_FILE_STATE)
    {
      //If a new line with FTPFile: is coming . that means, it must be coming second time and it will contain chunkid and chunk size.
      NSDL3_MON(NULL, NULL, "Going to process line for ftp -> %s", cmd_line);

      strcpy(tmp_cmd_line, cmd_line);

      total_flds = get_tokens(tmp_cmd_line, field, ":", 10);
      if(total_flds != 4)
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Wrong format for FTPFILE received. Format = %s", cmd_line);
        return -1; 
      }
    
      if(field[2])
        chunk_id = atoi(field[2]);

      if(field[3])
        *chunk_size = atoi(field[3]);

      *chk_mon_ftp_state = 1;
      *chk_mon_id_for_ftp = check_monitor_ptr->mon_id;
   
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "chunk_size = %d, chunk_id = %d", chunk_size, chunk_id);
   }
    else
    {
      if(extract_ftpfilename_size_createfile_with_size(cmd_line, check_monitor_ptr) == FAILURE)
      {
        close_ftp_file(check_monitor_ptr);
        return FAILURE;
      }
      if(check_monitor_ptr->ftp_file_size > 0)
        check_monitor_ptr->state = CHECK_MON_FTP_FILE_STATE;

      NSDL1_MON(NULL, NULL, "Ftp file size = %d", check_monitor_ptr->ftp_file_size);
    } 
  }

  else if(!(strncmp(cmd_line, CHECK_MONITOR_EVENT, strlen(CHECK_MONITOR_EVENT))))
  {
    NSDL3_MON(NULL, NULL, "%s found for CheckMonitor '%s'", CHECK_MONITOR_EVENT, check_monitor_ptr->check_monitor_name);
    ns_check_monitor_event_command(check_monitor_ptr, cmd_line, _FLN_);
  }
  else if(!(strncmp(cmd_line, CHECK_MONITOR_PASSED_LINE, strlen(CHECK_MONITOR_PASSED_LINE))))
  {
    NSDL3_MON(NULL, NULL, "%s found for CheckMonitor '%s'", CHECK_MONITOR_PASSED_LINE, check_monitor_ptr->check_monitor_name);
    mark_check_monitor_pass_for_outbound(check_monitor_ptr, CHECK_MONITOR_PASS, &num_pre_test_check, &num_post_test_check);
    return SUCCESS;
  }
  else if(!(strncmp(cmd_line, CHECK_MONITOR_FAILED_LINE, strlen(CHECK_MONITOR_FAILED_LINE))))
  {
    NSDL3_MON(NULL, NULL, "%s found for CheckMonitor '%s'", CHECK_MONITOR_FAILED_LINE, check_monitor_ptr->check_monitor_name);
    ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
    			    EID_CHKMON_ERROR, EVENT_INFO,
    			    "Check Monitor Failed", "%s found for CheckMonitor '%s'",
    			    CHECK_MONITOR_FAILED_LINE, check_monitor_ptr->check_monitor_name);
    mark_check_monitor_fail_for_outbound(check_monitor_ptr, CHECK_MONITOR_FAIL, &num_pre_test_check, &num_post_test_check);
    return FAILURE;
  }
  else if(!(strncmp(cmd_line, CHECK_MONITOR_UNREACHABLE, strlen(CHECK_MONITOR_UNREACHABLE))))
  {
    NSDL3_MON(NULL, NULL, "%s found for CheckMonitor '%s'", CHECK_MONITOR_UNREACHABLE, check_monitor_ptr->check_monitor_name);
    ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
                            EID_CHKMON_ERROR, EVENT_INFO,
                            "Check Monitor Failed", "%s found for CheckMonitor '%s'",
                            CHECK_MONITOR_UNREACHABLE, check_monitor_ptr->check_monitor_name);
    mark_check_monitor_fail_for_outbound(check_monitor_ptr, CHECK_MONITOR_FAIL, &num_pre_test_check, &num_post_test_check);
    return FAILURE;
  }
  else if(!(strncmp(cmd_line, CHECK_MONITOR_SEND_FAILED, strlen(CHECK_MONITOR_SEND_FAILED))))
  {
    NSDL3_MON(NULL, NULL, "%s found for CheckMonitor '%s'", CHECK_MONITOR_SEND_FAILED, check_monitor_ptr->check_monitor_name);
    ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr,
                            EID_CHKMON_ERROR, EVENT_INFO,
                            "Check Monitor Failed", "%s found for CheckMonitor '%s'",
                            CHECK_MONITOR_SEND_FAILED, check_monitor_ptr->check_monitor_name);
    mark_check_monitor_fail_for_outbound(check_monitor_ptr, CHECK_MONITOR_FAIL, &num_pre_test_check, &num_post_test_check);
    return FAILURE;
  }
  else // Invalid command
  {
    ns_check_monitor_log(EL_DF, DM_WARN1, MM_MON, _FLN_, check_monitor_ptr, EID_CHKMON_GENERAL, EVENT_INFO, "%s", cmd_line); 
    return FAILURE;
  }

  return SUCCESS;
}
//Blocking function. Will be called at initial connection creation.
int send_start_msg_to_NDC()
{
  char SendMsg[4096] = "\0";

  NSDL1_MON(NULL, NULL, "Method called");

  create_ndc_msg_for_data(SendMsg);

  MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Going to send Message to NDC for starting data connection: %s", SendMsg);
  if(send(ndc_data_mccptr.fd, SendMsg, strlen(SendMsg), 0) < 0)
    return -1;

  return 0;
}

int start_nd_ndc_data_conn()
{
  //char err_msg[1024];
  //err_msg[0] = '\0';

  // can we do it in outbound keyword parsing
  if((global_settings->net_diagnostics_mode == 0) && (global_settings->net_diagnostics_port == 0))
  { 
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Auto scale mode is off"); 
    ndc_data_mccptr.fd = -1;
    return 0;
  } 

  init_msg_con_struct(&ndc_data_mccptr, ndc_data_mccptr.fd, CONNECTION_TYPE_OTHER, global_settings->net_diagnostics_server, NS_NDC_DATA_CONN);
  /*ndc_data_mccptr.fd = nslb_tcp_client_ex(global_settings->net_diagnostics_server, global_settings->net_diagnostics_port, 10, err_msg);

  // We will not exit the testrun on data connection failure. It will get retried in deliver report
  if(ndc_data_mccptr.fd == -1)
  {
    // NS mode with auto scaling enabled
    if(global_settings->net_diagnostics_mode == 0)
    {
      NSTL1_OUT(NULL, NULL, "Error: Unable to connect to Net Diagnostics Server IP %s, Port = %d. Error = %s\n",
                            global_settings->net_diagnostics_server, global_settings->net_diagnostics_port, err_msg);

      //exit(-1);
    }
    else // ND mode
    {
      NSTL1_OUT(NULL, NULL, "Error: Unable to connect to Net Diagnostics Server IP %s, Port = %d. Error = %s\n",
                            global_settings->net_diagnostics_server, global_settings->net_diagnostics_port, err_msg);
      NSTL1_OUT(NULL, NULL, "Please check NDC is running or not.\n");

      //On starting, connection must made if not made then exit
      //nde_skip_bad_partition();
      //exit(-1);   /// ?????
    //  continue;
    }
    return -1;
  }



  //make send and receive blocking as we don't have any epoll now to put non blocking fd [ABHISHEK]

  if(send_start_msg_to_NDC() < 0)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Error in sending data to NDC on for starting data connection");   
    return -1;
  }

  if(wait_for_NDC_to_respond(START_REPLY_FROM_NDC, START_NDC) < 0)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Error in getting response from NDC");
    return 0;
  }

  MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Data connection between NDC and NS is created. NS=%s, NDC=%d, fd=%d", nslb_get_src_addr(ndc_data_mccptr.fd), global_settings->net_diagnostics_port, ndc_data_mccptr.fd);
 
  MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Address of ndc_data_mccptr.read_buf is %p", ndc_data_mccptr.read_buf);
 
  if(fcntl(ndc_data_mccptr.fd, F_SETFL, O_NONBLOCK) < 0)
  {
    sprintf(err_msg, "Error: fcntl() in making connection non blocking for socket %d, errno %d (%s).\n", ndc_data_mccptr.fd, errno, nslb_strerror(errno));
   
    close(ndc_data_mccptr.fd);
    ndc_data_mccptr.fd = -1;
    return -1;
  }
  init_msg_con_struct(&ndc_data_mccptr, ndc_data_mccptr.fd, CONNECTION_TYPE_OTHER, global_settings->net_diagnostics_server, NS_NDC_DATA_CONN);
  

  ndc_data_mccptr.state |= NS_CONNECTED;*/

  NSDL2_MON(NULL, NULL, "Method end.");
  
  return 0;
}


int stop_nd_ndc_data_conn()
{
  char buffer[1024];
  
  if(ndc_data_mccptr.fd > 0)
  {
    sprintf(buffer, "nd_data_req:action=stop_monitor_data_conn;TEST_RUN=%d;PARTITION_IDX=%lld\n", testidx, g_partition_idx);
    
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Going to send MSG to NDC. ndc_data_mccptr.fd = %d, state = %d, MESSAGE = %s", ndc_data_mccptr.fd, ndc_data_mccptr.state, buffer); 

   NSDL1_MON(NULL, NULL, "Sending Message to NDC. ndc_data_mccptr.fd = %d, state = %d, message = %s", ndc_data_mccptr.fd, ndc_data_mccptr.state, buffer);

    if(ndc_data_mccptr.state & NS_CONNECTED)
    {
      if(write_msg(&ndc_data_mccptr, buffer, strlen(buffer), 0, DATA_MODE) < 0)
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Error in sending %s to NDC\n", buffer);
        NSTL1_OUT(NULL, NULL, "Error in sending %s message to NDC\n", buffer);
        return -1;
      }

      //When this function will be executed, we will be out of the loop from ns_parent.c, it means we will be out of wait forever loop. Hence we need to call this function to recive the response from NDC. Whenwe are out of wait forever loop, we wont be getting any events on epoll. So no more response will be received from NDC EPOLL.
      //Refer BUG 28503

      if(wait_for_NDC_to_respond(STOP_REPLY_FROM_NDC, STOP_NDC) > 0)
        close_msg_com_con(&ndc_data_mccptr);
    }
    else
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Not going to send closing message to NDC as connection is not in  connected state");
    }
  }
  else
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Connection from NDC is already closed. Hence returning.");
  }
  

  return 0;
}


//This is a blocking function and can block if NDC doesnot reply.
int wait_for_NDC_to_respond(char *buff, int operation)
{
  int bytes_read, total_read = 0;
  char RecvBuff[MAX_BUFFER_SIZE_FOR_MONITOR] = "\0";
  char *tmp_ptr = NULL;
  
  while(1)
  {
    if((bytes_read = read (ndc_data_mccptr.fd, (RecvBuff + total_read), MAX_BUFFER_SIZE_FOR_MONITOR)) < 0)
      continue;

    else if(bytes_read == 0)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Connection closed from other side. Error in reading reply on NDC data connection.");
      close(ndc_data_mccptr.fd);
      ndc_data_mccptr.fd = -1;
      return -1;
    }

    total_read += bytes_read;
    RecvBuff[total_read] = '\0';

    while(1)
    {
      tmp_ptr = strstr(RecvBuff, "\n");
      if(tmp_ptr)
      {
        *tmp_ptr = '\0';
        if(!strcmp(RecvBuff, buff))
        {
          NSDL1_MON(NULL, NULL, "Received Message from NDC: %s", RecvBuff);
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Received Message from NDC on data connection: %s", RecvBuff);  
          return 1;
        }
        else if(strcasestr(RecvBuff, "result=ERROR")) 
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Received Error from NDC on data connection. Message->%s.", RecvBuff);
          NSDL1_MON(NULL, NULL, "Received Error from NDC on data connection. Message->%s.", RecvBuff); 
          close(ndc_data_mccptr.fd);
          NS_EXIT(-1,  CAV_ERR_1060015, RecvBuff);
        }
        else
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Received Invalid message on data connection. Going to wait again. Message->%s.", RecvBuff); 
          NSDL1_MON(NULL, NULL, "Received Invalid message on data connection. Going to wait again. Message->%s.", RecvBuff);
        }

        strcpy(RecvBuff, tmp_ptr + 1);
        total_read = strlen(RecvBuff);
      }
      else
      {
        if(operation == STOP_NDC)
          total_read = 0;

        break;
      }
 
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Received partial data: %s", RecvBuff);
    }
  }
  return 0;
}


void create_ndc_msg_for_data(char *msg_buf)
{
  NSDL1_MON(NULL, NULL, "Method_called");

  sprintf(msg_buf, "nd_data_req:action=start_monitor_data_conn;");

  sprintf(msg_buf + strlen(msg_buf), "TEST_RUN=%d;NS_WDIR=%s;READER_RUN_MODE=%d;"
                   "TIME_STAMP=%llu;ND_PROFILE=%s;NS_EVENT_LOGGER_PORT=%hu;NS_PID=%d;MON_FREQUENCY=%d;"
                   "CAV_EPOCH_DIFF=%ld;PARTITION_IDX=%lld;START_PARTITION_IDX=%lld;ND_VECTOR_SEPARATOR=%c;TOPOLOGY_NAME=%s;MACHINE_TYPE=%s;"
                   "MULTIDISK_RAWDATA_PATH=%s;MULTIDISK_NDLOGS_PATH=%s;MULTIDISK_PROCESSED_DATA_PATH=%s;OUTBOUND=%d;MACHINE_OP_TYPE=%d;\n", 
                   testidx, g_ns_wdir, global_settings->reader_run_mode, get_ns_start_time_in_secs(), global_settings->nd_profile_name, 
                   event_logger_port_number, getpid(), global_settings->progress_secs,
              global_settings->unix_cav_epoch_diff, g_partition_idx, g_start_partition_idx, global_settings->hierarchical_view_vector_separator, 
                   global_settings->hierarchical_view_topology_name, g_cavinfo.config, 
                   global_settings->multidisk_rawdata_path?global_settings->multidisk_rawdata_path:"",
                   global_settings->multidisk_ndlogs_path?global_settings->multidisk_ndlogs_path:"",
                   global_settings->multidisk_processed_data_path?global_settings->multidisk_processed_data_path:"",
                   is_outbound_connection_enabled, loader_opcode);
}

//non blocking function. Need to call at runtime
int make_and_send_start_msg_to_ndc_on_data_conn()
{
  char SendMsg[4096] = "\0";

  NSDL1_MON(NULL, NULL, "Method called");

  create_ndc_msg_for_data(SendMsg);

  MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Going to send Message to NDC for starting data connection: %s", SendMsg);
  if(write_msg(&ndc_data_mccptr, SendMsg, strlen(SendMsg), 0, DATA_MODE) < 0)
    return -1;

  return 0;
}


inline void handle_ndc_data_conn(struct epoll_event *pfds, int i)
{
  int con_state;
  char err_msg[1024] = "\0";

  NSDL1_MON(NULL, NULL, "Method Called");
  
  if (pfds[i].events & EPOLLOUT) 
  {
    NSDL1_MON(NULL, NULL, "For data connection, Received EPOLLOUT event");
    /*In case of recovery we are creating a non blocking connection therefore need to verify connection state*/
    if (ndc_data_mccptr.state & NS_CONNECTING)
    {
      nslb_nb_connect(ndc_data_mccptr.fd, global_settings->net_diagnostics_server, global_settings->net_diagnostics_port, &con_state, err_msg);
  
      if(con_state != NSLB_CON_CONNECTED)
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "NDC:Still not connected for data connection. err_msg = %s", err_msg);
        CLOSE_MSG_COM_CON(&ndc_data_mccptr, DATA_MODE);
        return; 
      }
      //Else socket is connected, add socket to EPOLL IN
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "NDC:Connected successfully with fd %d and to server ip = %s for data connection", ndc_data_mccptr.fd, global_settings->net_diagnostics_server);
      ndc_data_mccptr.state &= ~NS_CONNECTING;
      //mod_select_msg_com_con((char *)&ndc_data_mccptr, ndc_data_mccptr.fd, EPOLLIN | EPOLLERR | EPOLLHUP);
      MOD_SELECT_MSG_COM_CON((char *)&ndc_data_mccptr, ndc_data_mccptr.fd, EPOLLIN | EPOLLERR | EPOLLHUP, DATA_MODE);

      if(global_settings->net_diagnostics_mode)
      {
        if(ndc_mccptr.state & NS_CONNECTED)
          make_and_send_start_msg_to_ndc_on_data_conn();
        else
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Data connection with NDC is not created yet. So will not send message for data connection.");
          CLOSE_MSG_COM_CON(&ndc_data_mccptr, DATA_MODE);
          return;
        }
      }
      else
        make_and_send_start_msg_to_ndc_on_data_conn();
    }
    else if((ndc_data_mccptr.state & NS_CONNECTED) || ndc_data_mccptr.state & NS_DATA_CONN_MADE)
    {
      //ndc_data_mccptr.state &= ~NS_CONNECTED;
      if (ndc_data_mccptr.state & NS_STATE_WRITING)
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Writting state. Going to write remaining message for data connection.");   
        write_msg(&ndc_data_mccptr, NULL, 0, 0, DATA_MODE);
      }
    }//If Initial message for data connection is not sent successfully.
    else if (ndc_data_mccptr.state & NS_STATE_WRITING)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Writting state. Going to write remaining message for data connection.");
      write_msg(&ndc_data_mccptr, NULL, 0, 0, DATA_MODE);
    }
  } 
  else if (pfds[i].events & EPOLLIN) 
    receive_data_from_NDC(-1);

  else if (pfds[i].events & EPOLLHUP)
  {
    NSTL1_OUT(NULL, NULL, "EPOLLHUP occured on sock %s for data connection. error = %s",
                    msg_com_con_to_str(&ndc_data_mccptr), nslb_strerror(errno));
    NSDL1_MON(NULL, NULL, "EPOLLHUP occured on sock %s for data connection. error = %s",
                    msg_com_con_to_str(&ndc_data_mccptr), nslb_strerror(errno));
    //close_msg_com_con_and_exit(&ndc_data_mccptr, (char *)__FUNCTION__, __LINE__, __FILE__);
    Msg_com_con *m = &ndc_data_mccptr;
    CLOSE_MSG_COM_CON_EXIT(m, DATA_MODE);
  } 
  else if (pfds[i].events & EPOLLERR)
  {
    NSTL1_OUT(NULL, NULL, "EPOLLERR occured on sock %s for data connection. error = %s",
                msg_com_con_to_str(&ndc_data_mccptr), nslb_strerror(errno));
     NSDL3_MON(NULL, NULL, "EPOLLERR occured on sock %s for data connection. error = %s",
                msg_com_con_to_str(&ndc_data_mccptr), nslb_strerror(errno));
     //close_msg_com_con_and_exit(&ndc_data_mccptr, (char *)__FUNCTION__, __LINE__, __FILE__);
     Msg_com_con *m = &ndc_data_mccptr;
     CLOSE_MSG_COM_CON_EXIT(m, DATA_MODE);
  } 
  else 
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "This should not happen.");
    NSDL3_MON(NULL, NULL, "This should not happen.");
     
  }
}

//check_mon_done is passed to determin what epoll fd need to be added (check mon epoll / parent epoll)
static int connect_to_ndc_for_data_connection(char *ip, int port, int not_add_in_chk_mon_epoll)
{
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
                                "Error: Error in opening socket for ndc data connection");

    NSDL1_MON(NULL, NULL, "Error: Error in opening socket for ndc data connection");
    return -1;
  }
  
 
  NSDL3_MON(NULL, NULL, "Socket opened successfully so making connection for fd %d to server ip =%s, port = %d",
                         fd, ip, port);
  //Calling non-blocking connect
  int con_ret = nslb_nb_connect(fd, ip, port, &con_state, err_msg);

  NSDL3_MON(NULL, NULL, "con_ret = %d, con_state = %d", con_ret, con_state);
  /* Initialize ndc_data_mccptr */
  init_msg_con_struct(&ndc_data_mccptr, fd, CONNECTION_TYPE_OTHER, global_settings->net_diagnostics_server, NS_NDC_DATA_CONN);

  if(con_ret == 0)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Data connection from NDC is created."); 
    //add_select_msg_com_con((char *)&ndc_data_mccptr, ndc_data_mccptr.fd, EPOLLIN | EPOLLERR | EPOLLHUP);
    ADD_SELECT_MSG_COM_CON((char *)&ndc_data_mccptr, ndc_data_mccptr.fd, EPOLLIN | EPOLLERR | EPOLLHUP, DATA_MODE);

    if(global_settings->net_diagnostics_mode)
    {
      if(ndc_mccptr.state & NS_CONNECTED)
        make_and_send_start_msg_to_ndc_on_data_conn();
      else
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Data connection with NDC is not created yet. So will not send message for data connection.");
        //close_msg_com_con(&ndc_data_mccptr);
        Msg_com_con *m = &ndc_data_mccptr;
        CLOSE_MSG_COM_CON(m, DATA_MODE);
        return -1;
      }
    }
    else
      make_and_send_start_msg_to_ndc_on_data_conn();
  }
  else if(con_ret > 0)
  {
    if(con_state == NSLB_CON_CONNECTED)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Data connection from NDC is created."); 
      //add_select_msg_com_con((char *)&ndc_data_mccptr, ndc_data_mccptr.fd, EPOLLIN | EPOLLERR | EPOLLHUP);
      ADD_SELECT_MSG_COM_CON((char *)&ndc_data_mccptr, ndc_data_mccptr.fd, EPOLLIN | EPOLLERR | EPOLLHUP, DATA_MODE);

      if(global_settings->net_diagnostics_mode)
      {
        if(ndc_mccptr.state & NS_CONNECTED)
          make_and_send_start_msg_to_ndc_on_data_conn();
        else
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Data connection with NDC is not created yet. So will not send message for data connection.");
          //close_msg_com_con(&ndc_data_mccptr);
          Msg_com_con *m = &ndc_data_mccptr;
          CLOSE_MSG_COM_CON(m, DATA_MODE);
          return -1;
        }
      }
      else
        make_and_send_start_msg_to_ndc_on_data_conn();
    }
    else if(con_state == NSLB_CON_CONNECTING)
    {
      //Connecting state, need to add fd on EPOLLOUT
       ndc_data_mccptr.state |= NS_CONNECTING;
       MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Connecting to NDC at ip address %s and port %d\n", ip, port);
       //add_select_msg_com_con((char *)&ndc_data_mccptr, ndc_data_mccptr.fd, EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP);
       ADD_SELECT_MSG_COM_CON((char *)&ndc_data_mccptr, ndc_data_mccptr.fd, EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP, DATA_MODE);
       // Note - Connection event comes as EPOLLOUT
    }
  }
  else //Error case. We need to restart again
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error: Unable to connect NDC hence returning. IP = %s, port = %d", ip, port);
    close(fd);
    fd = -1;
    //close_msg_com_con(&ndc_data_mccptr);
    Msg_com_con *m = &ndc_data_mccptr;
    CLOSE_MSG_COM_CON(m, DATA_MODE);
    return -1;
  }


  MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "ndc_data_mccptr.state & NS_CONNECTING = %x, ndc_data_mccptr.fd=%d", ndc_data_mccptr.state, ndc_data_mccptr.fd);
 return 0;
}



int handle_ndc_data_connection_recovery(int not_add_in_chk_mon_epoll)
{
  MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Connecting to NDC for data connection at ip address %s and port %d\n", global_settings->net_diagnostics_server, global_settings->net_diagnostics_port);
  if ((connect_to_ndc_for_data_connection(global_settings->net_diagnostics_server, global_settings->net_diagnostics_port, not_add_in_chk_mon_epoll)) < 0)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Error in creating the TCP socket to communicate with the NDC at data connnection (%s:%d).",
           global_settings->net_diagnostics_server, global_settings->net_diagnostics_port);
  } 
  
 /* if(ndc_data_mccptr.state & NS_CONNECTED)
  {
    if(make_and_send_start_msg_to_ndc_on_data_conn() < 0)
    {
      NSTL1(NULL, NULL, "Error in sending start msg to NDC on data connection.");
      return -1;
    }

    //We will not wait for response here, as this connection is made non blocking and this will not be a blocking. 
    //wait_for_NDC_to_respond(START_REPLY_FROM_NDC);

    if(not_add_in_chk_mon_epoll)
    {
      if(make_mon_msg_and_send_to_NDC(0) < 0)
      {
        NSTL1(NULL, NULL, "Error in sending monitor config message to NDC on data connecion.");
        return -1;
      }
    }
  }*/
  return 0;
}


int kw_set_outbound_connection(char *buff)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char temp_buff[MAX_DATA_LINE_LENGTH];
  char err_msg[MAX_AUTO_MON_BUF_SIZE];
  int option;
  int num;
  

  //ABHI: TODO: Add extra arguement for junk.
  num = sscanf(buff,"%s %d %s", keyword, &option, temp_buff);
  
  if(num != 2)
  { 
    NS_KW_PARSING_ERR(keyword, 0, err_msg, ENABLE_OUTBOUND_CONNECTION_USAGE, CAV_ERR_1011359, keyword);
  }
  
  if(option == 1)
  {
   // ABHI: MAcro for values

    is_outbound_connection_enabled = 1;
    g_mon_id = ALL_MON_START_MON_ID;
    g_chk_mon_id = CHK_MON_START_MON_ID;  

    //Id for check monitor should start from 50000, because we need to distinguish data, wheather coming data is to be saved in cus_mon_ptr or is a check monitor data. It will be processed accordingly.
  }

  return 0;
}

int make_and_send_msg_to_start_monitor(CM_info * cus_mon_ptr)
{
  int msg_len = -1;
  char msg_buff[MAX_MONITOR_BUFFER_SIZE];
 
 // create_table_to_store_monitor_msg();
  NSDL1_MON(NULL, NULL, "Method Called");
  if(cus_mon_ptr->tier_name)
    msg_len = sprintf(msg_buff, "nd_data_req:action=mon_config;server=%s%c%s;mon_id=%d;msg=", cus_mon_ptr->tier_name, global_settings->hierarchical_view_vector_separator, cus_mon_ptr->server_display_name, cus_mon_ptr->mon_id);
  else
    msg_len = sprintf(msg_buff, "nd_data_req:action=mon_config;server=%s;mon_id=%d;msg=", cus_mon_ptr->server_display_name, cus_mon_ptr->mon_id);
 
  cm_make_send_msg(cus_mon_ptr, msg_buff, global_settings->progress_secs, &msg_len);

  if(write_msg(&ndc_data_mccptr, monitor_scratch_buf, msg_len, 0, DATA_MODE) < 0)
  {
    MLTL1(EL_F, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFORMATION, "cm_init message is not sent to NDC to restart monitor for mon_id = %d. Message = %s",cus_mon_ptr->mon_id, monitor_scratch_buf);
    return -1;
  }
  
  MLTL1(EL_F, 0, 0, _FLN_, cus_mon_ptr, EID_DATAMON_GENERAL, EVENT_INFORMATION, "Message sent to NDC to restart monitor for mon_id = %d. Message = %s",cus_mon_ptr->mon_id, monitor_scratch_buf);

  mon_id_map_table[cus_mon_ptr->mon_id].state = INIT_MONITOR;
  return 0;
}  

//This is to set cm_index in mon_id_map_table if gdf is NA. In outbound data wont come if cm_index is not set.
void set_index_for_NA_group(CM_info *local_cm_ptr, int grp_num_monitors, int monitor_idx)
{
  int i = 0;

  for(i = monitor_idx; i < grp_num_monitors; i++)
  {
    if(local_cm_ptr->mon_id < 0)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Mon Id should not be negative, for this monitor, Gdf = %s, monitor_name = %s.", local_cm_ptr->gdf_name, local_cm_ptr->monitor_name);
    }
    else
      mon_id_map_table[local_cm_ptr->mon_id].mon_index = monitor_idx;
  }
}


//This method will send monitor request to NDC. Because 
void send_monitor_request_to_NDC(int server_idx, int ret)
{
  int i, state, mon_idx;
  //ServerInfo *servers_list = NULL;
  CM_info *cm_ptr = NULL;
  //servers_list = (ServerInfo *) topolib_get_server_info();

  for(i = 0; i < topo_info[topo_idx].topo_tier_info[ret].topo_server[server_idx].server_ptr->topo_servers_list->total_mon_id_entries; i++)
  {
    state = mon_id_map_table[topo_info[topo_idx].topo_tier_info[ret].topo_server[server_idx].server_ptr->topo_servers_list->monitor_config[i].mon_id].state;

    //if state is cleaned, then we need to add monitor but no need to mark reused.
    if(state == CLEANED_MONITOR)
    {
      for(i = 0; i < topo_info[topo_idx].topo_tier_info[ret].topo_server[server_idx].server_ptr->topo_servers_list->total_mon_id_entries; i++)
        mon_id_map_table[topo_info[topo_idx].topo_tier_info[ret].topo_server[server_idx].server_ptr->topo_servers_list->monitor_config[i].mon_id].state = DELETED_MONITOR;

      reset_monitor_config_in_server_list(ret,server_idx);
      //This is the case when ENABLE_AUTO_JSON_MONITOR mode 2 is on.
      if(total_mon_config_list_entries > 0)
        mj_apply_monitors_on_autoscaled_server(NULL, server_idx, ret, NULL);
      else      
        add_json_monitors(NULL, server_idx, ret, NULL, 1, 0, 0);
      return;
    }
  }
 
  for(i = 0; i < topo_info[topo_idx].topo_tier_info[ret].topo_server[server_idx].server_ptr->topo_servers_list->total_mon_id_entries; i++)
  {
    if(ndc_data_mccptr.fd > 0)
    {
      state = mon_id_map_table[topo_info[topo_idx].topo_tier_info[ret].topo_server[server_idx].server_ptr->topo_servers_list->monitor_config[i].mon_id].state;
      
      if(state != DELETED_MONITOR)
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Sending message to NDC for MON_CONFIG for monitors added at runtime. Message: %s", 
                        topo_info[topo_idx].topo_tier_info[ret].topo_server[server_idx].server_ptr->topo_servers_list->monitor_config[i].init_cusMon_buff);
        if(write_msg(&ndc_data_mccptr, topo_info[topo_idx].topo_tier_info[ret].topo_server[server_idx].server_ptr->topo_servers_list->monitor_config[i].init_cusMon_buff, strlen(topo_info[topo_idx].topo_tier_info[ret].topo_server[server_idx].server_ptr->topo_servers_list->monitor_config[i].init_cusMon_buff), 0, DATA_MODE) < 0)
        {
          MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_ERROR, EVENT_INFORMATION,
              "Error in sending message to server %s on NDC data connection.",
                       topo_info[topo_idx].topo_tier_info[ret].topo_server[server_idx].server_disp_name);
          break;
        }
        mon_id_map_table[topo_info[topo_idx].topo_tier_info[ret].topo_server[server_idx].server_ptr->topo_servers_list->monitor_config[i].mon_id].state = RUNNING_MONITOR;

        mon_idx = mon_id_map_table[topo_info[topo_idx].topo_tier_info[ret].topo_server[server_idx].server_ptr->topo_servers_list->monitor_config[i].mon_id].mon_index;
        if(mon_idx >= 0)
        {
          cm_ptr = monitor_list_ptr[mon_idx].cm_info_mon_conn_ptr;
          //We are setting reused for custom monitor as vectors ifor DVM will be marked reused when data comes.
          if(monitor_list_ptr[mon_idx].is_dynamic == 0)
          {
            set_reused_vector_counters(cm_ptr->vector_list, 0, cm_ptr, cm_ptr->cs_ip, cm_ptr->cs_port, cm_ptr->pgm_path, cm_ptr->pgm_args, MON_REUSED_INSTANTLY);
            
           check_deleted_vector(cm_ptr, 0);
          }
        }
      }
    }
    else
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION,
              "Info: Could not send monitor request to NDC after node discovery, as connection with NDC is not established. Hence returning.");
      return;
    }
  }
}

void reset_monitor_config_in_server_list(int tier_idx,int server_idx)
{
  int j;
   
  if(topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->topo_servers_list->total_mon_id_entries == 0) 
    return;

  //This server is marked inactive, need to clean its monitor_list bcoz monitor were also marked deleted when server_inactive was received. And it will be cleaned up from structure at partition switch.
  for(j = 0; j < topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->topo_servers_list->total_mon_id_entries; j++)
  {
    FREE_AND_MAKE_NULL(topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->topo_servers_list->monitor_config[j].init_cusMon_buff, "cm_init_message", j);
    mon_id_map_table[topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->topo_servers_list->monitor_config[j].mon_id].state = DELETED_MONITOR;
    mon_id_map_table[topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->topo_servers_list->monitor_config[j].mon_id].mon_index = -1;
  }

  topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->topo_servers_list->total_mon_id_entries = 0;
  topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->topo_servers_list->max_mon_id_entries = 0;
  FREE_AND_MAKE_NULL(topo_info[topo_idx].topo_tier_info[tier_idx].topo_server[server_idx].server_ptr->topo_servers_list->monitor_config, "monitor_config 2-D table", j);
}
