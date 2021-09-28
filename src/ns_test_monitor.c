#include <stdio.h>
#include <stdlib.h>

#include "nslb_util.h"
#include "nslb_alloc.h"
#include "nslb_sock.h"
#include "util.h"
#include "ns_log.h"
#include "ns_msg_def.h"
#include "ns_common.h"
#include "ns_global_settings.h"
#include "ns_trace_level.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"
#include "ns_exit.h"
#include "ns_gdf.h"
#include "ns_rbu_page_stat.h"
#include "netstorm.h"

#include "ns_file_upload.h"
#include "ns_test_monitor.h"
#include "ns_cavmain_child_thread.h"

#define MONITOR_DATA_BUFFER_SIZE 64*1024

//HTTP API
#ifndef CAV_MAIN
int g_cavtest_http_avg_idx = -1;
CavTestHTTPAvgTime *cavtest_http_avg;
//Web Page Audit
int g_cavtest_web_avg_idx = -1;
CavTestWebAvgTime *cavtest_web_avg;
#else
__thread int g_cavtest_http_avg_idx = -1;
__thread CavTestHTTPAvgTime *cavtest_http_avg;
//Web Page Audit
__thread int g_cavtest_web_avg_idx = -1;
__thread CavTestWebAvgTime *cavtest_web_avg;
#endif

static char *monitor_data_buffer = NULL;
static int monitor_data_buffer_len = 0;

void create_http_api_monitor_data();
void create_web_page_audit_monitor_data();

typedef struct CavMonDataInfo
{
  char type[256];
  void (*create_data)(VUser *vptr, avgtime **g_avg, cavgtime **g_cavg);
}CavMonDataInfo;

CavMonDataInfo monitor_data[MONITOR_TYPE_MAX] = 
{
  {"NONE", NULL},
  {"HTTP_API", create_http_api_monitor_data},
  {"WEB_PAGE_AUDIT", create_web_page_audit_monitor_data} 
}; 
/*
char monitor_data_type_str[][32] = {
  "RTG_DATA",
  "HTTP_REQUEST",
  "HTTP_RESPONSE",
  "HAR_FILE",
  "CHECK_POINT"
}; */

enum http_api_data_index
{
  http_api_total_time_idx,
  http_api_resolve_time_idx,
  http_api_connect_time_idx,
  http_api_ssl_time_idx,
  http_api_send_time_idx,
  http_api_first_byte_time_idx,
  http_api_download_time_idx,
  http_api_response_time_idx,
  http_api_redirtect_time_idx,
  http_api_request_header_size_idx,
  http_api_request_body_size_idx,
  http_api_request_size_idx,
  http_api_response_header_size_idx,
  http_api_response_body_size_idx,
  http_api_response_size_idx,
  http_api_status_code_idx,
  http_api_availabilty_status_idx,
  http_api_unavailabilty_reason_idx,
  http_api_max_idx
};

enum web_page_audit_data_index
{
  web_page_audit_total_time_idx,
  web_page_audit_resolve_time_idx,
  web_page_audit_connect_time_idx,
  web_page_audit_ssl_time_idx,
  web_page_audit_send_time_idx,
  web_page_audit_first_byte_time_idx,
  web_page_audit_download_time_idx,
  web_page_audit_response_time_idx,
  web_page_audit_tbt_time_idx,
  web_page_audit_lcp_time_idx,
  web_page_audit_cls_time_idx,
  web_page_audit_redirtect_time_idx,
  web_page_audit_request_header_size_idx,
  web_page_audit_request_body_size_idx,
  web_page_audit_request_size_idx,
  web_page_audit_response_header_size_idx,
  web_page_audit_response_body_size_idx,
  web_page_audit_response_size_idx,
  web_page_audit_status_code_idx,
  web_page_audit_availabilty_status_idx,
  web_page_audit_unavailabilty_reason_idx,
  web_page_audit_domload_time_idx,
  web_page_audit_onload_time_idx,
  web_page_audit_overall_time_idx,
  web_page_audit_tti_time_idx,
  web_page_audit_start_render_time_idx,
  web_page_audit_visual_complete_time_idx,
  web_page_audit_page_weight_idx,
  web_page_audit_js_size_idx,
  web_page_audit_css_size_idx,
  web_page_audit_img_weight_idx,
  web_page_audit_dom_ele_idx,
  web_page_audit_page_score_idx,
  web_page_audit_max_idx
};

/*--------------------------------------------------------------------------------------------- 
 * Purpose  : This function will parse keyword TEST_MONITOR_CONFIG and store all required data 
 *
 * Input    : buf  : TEST_MONITOR_CONFIG <MON_TYPE> <MON_IDX> <MON_NAME> <TIER_NAME> <SERVER_NAME> <TR_NUMBER> <PARTITION_ID> <FILE_UPLOAD_URL>
 *
 *                   MON_TYPE           -  Test Monitor Type
 *                                           - 0 - None (default)
 *                                           - 1 - HTTP API 
 *                                           - 2 - Web Page Audit
 *                   MON_IDX            -  Monitor Index
 *                   MON_NAME           -  Monitor Name
 *                   TIER_NAME          -  Tier Name 
 *                   SERVER_NAME        -  Server Name 
 *                   TR_NUMBER          -  CavMain Controller Testrun Number
 *                   PARTITION_ID       -  PartitionId of the SM test
 *                   FILE_UPLOAD_URL    -  URL which will process the X-Cav-File-Upload header and store the payload in the provided file
 *
 *            Eg: TEST_MONITOR_CONFIG 0
 *            Eg: TEST_MONITOR_CONFIG 1 0 GoogleTest Cavisson 10.10.70.8 11111 20200815120100 https://cav-test.com/fileupload
 *
 *            NOTE: If Test Monitor Type is not 0, then all other arguments are mandatory.
 *
 * Output   : On error    -1
 *            On success   0 
 *--------------------------------------------------------------------------------------------*/
int kw_set_test_monitor_config(char *buf, char *err_msg, int runtime_flag)
{
  char file_upload_url[FILE_NAME_LENGTH];
  #ifndef CAV_MAIN
  char partition_id_str[MAX_NAME_LENGTH];
  char tr_num_str[MAX_NAME_LENGTH];
  char tier[MAX_MON_NAME_LENGTH];
  char server[MAX_MON_NAME_LENGTH];
  char monitor_name[MAX_MON_NAME_LENGTH];
  char monitor_type_str[8];
  char monitor_idx_str[8];
  char tmp[SGRP_NAME_MAX_LENGTH];
  char keyword[SGRP_NAME_MAX_LENGTH];
  int tr_num = 0;
  #endif
  char hostname[MAX_TNAME_LENGTH];
  char request_line[MAX_TNAME_LENGTH];
  int request_type;
  int ret = 0;
  int monitor_type = 0;

  int server_port;
  char *hptr;
  char *tmp_ptr , *tmp_hostname;

  NSDL2_PARSING(NULL, NULL, "Method called, buf = %s", buf);
  #ifndef CAV_MAIN

  ret = sscanf(buf, "%s %s %s %s %s %s %s %s %s %s", keyword, monitor_type_str, monitor_idx_str, monitor_name, tier, server,
                     tr_num_str, partition_id_str, file_upload_url, tmp);
  NSDL2_PARSING(NULL, NULL, "keyword = %s, monitor_type_str = %s, monitor_idx_str = %s, monitor_name = %s, tier = %s, server = %s, "
                            "tr_num_str = %s, partition_id_str = %s, file_upload_url = %s",
                             keyword, monitor_type_str, monitor_idx_str, monitor_name, tier, server, tr_num_str, partition_id_str,
                             file_upload_url);

  if (ret < 2 || ret > 9)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, TEST_MONITOR_TYPE_USAGE, CAV_ERR_1011295, CAV_ERR_MSG_1);
  }

  if (nslb_atoi(monitor_type_str, &monitor_type) < 0)
  {
    //ERROR: Invalid Monitor type
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, TEST_MONITOR_TYPE_USAGE, CAV_ERR_1011295, CAV_ERR_MSG_2);
  }
  if (monitor_type < MONITOR_TYPE_NONE || monitor_type >= MONITOR_TYPE_MAX)
  {
    //ERROR: Invalid Monitor type
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, TEST_MONITOR_TYPE_USAGE, CAV_ERR_1011295, CAV_ERR_MSG_3);
  }

  if (monitor_type != MONITOR_TYPE_NONE)
  {
    if (ret != 9)
    {
      NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, TEST_MONITOR_TYPE_USAGE, CAV_ERR_1011295, CAV_ERR_MSG_1);
    }
    else
    {
      //Set Monitor Index
      if (nslb_atoi(monitor_idx_str, &(global_settings->monitor_idx)) < 0)
      {
        //ERROR: Invalid Monitor Idx
        NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, TEST_MONITOR_TYPE_USAGE, CAV_ERR_1011295, CAV_ERR_MSG_2);
      }

      //Set Monitor Name
      NSLB_MALLOC_AND_MEMSET(global_settings->monitor_name, MAX_MON_NAME_LENGTH + 1, "global_settings->monitor_name", -1, NULL);
      snprintf(global_settings->monitor_name, MAX_MON_NAME_LENGTH, "%s", monitor_name);

      //Set Tier Name
      NSLB_MALLOC_AND_MEMSET(global_settings->cavtest_tier, MAX_MON_NAME_LENGTH + 1, "global_settings->cavtest_tier", -1, NULL);
      snprintf(global_settings->cavtest_tier, MAX_MON_NAME_LENGTH, "%s", tier);

      //Set Server Name
      NSLB_MALLOC_AND_MEMSET(global_settings->cavtest_server, MAX_MON_NAME_LENGTH + 1, "global_settings->cavtest_server", -1, NULL);
      snprintf(global_settings->cavtest_server, MAX_MON_NAME_LENGTH, "%s", server);

      //Set CavMain Controller TR
      if (nslb_atoi(tr_num_str, &tr_num) < 0)
      {
        //ERROR: Invalid CavMain Controller TR
        NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, TEST_MONITOR_TYPE_USAGE, CAV_ERR_1011295, CAV_ERR_MSG_2);
      }
      snprintf(g_controller_testrun, 32, "%d", tr_num);

      //Set Partition Idx
      if((global_settings->cavtest_partition_idx = atoll(partition_id_str)) <= 0)
      {
        //ERROR: Invalid Partition Idx
        NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, TEST_MONITOR_TYPE_USAGE, CAV_ERR_1011295, CAV_ERR_MSG_2);
      }

      NSDL2_PARSING(NULL, NULL, "global_settings->monitor_idx = %d, global_settings->monitor_name = %s, global_settings->cavtest_tier = %s, "
                                "global_settings->cavtest_server = %s, g_controller_testrun = %s, "
                                "global_settings->cavtest_partition_idx = %lld",
                                 global_settings->monitor_idx, global_settings->monitor_name, global_settings->cavtest_tier,
                                 global_settings->cavtest_server, g_controller_testrun, global_settings->cavtest_partition_idx);

      //Set file upload URL
#else
      //strcpy(file_upload_url, buf);
     // sscanf(buf, "%s %s", keyword, file_upload_url);
       sscanf(buf, "%s", file_upload_url);
#endif
      if(parse_url_param(file_upload_url, "{/?#", &request_type, hostname, request_line) != RET_PARSE_OK)
        NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, TEST_MONITOR_TYPE_USAGE, CAV_ERR_1011295, CAV_ERR_1012069_MSG, file_upload_url);

      NSDL2_PARSING(NULL, NULL, "request_type = %d, hostname = %s, request_line = %s.", request_type, hostname, request_line);
      //Request type should be from http, https
      if(request_type == HTTP_REQUEST)
        request_type = 0; //HTTP
      else if(request_type == HTTPS_REQUEST)
        request_type = 1; //HTTPS
      else
      {
        NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, TEST_MONITOR_TYPE_USAGE, CAV_ERR_1011295,
                               "Provided URL is not valid. Expecting either 'http://' or 'https://'");
      }

      // This is done to support ipv6 hostname in script . This is done temporarily to validate ip . Original hostname is not modified 
      MY_MALLOC (tmp_hostname, strlen(hostname) + 1, "tmp_hostname", -1);
      // Copy hostname to tmp_hostname 
      strcpy(tmp_hostname, hostname);
      // Check if ipv6 starts with '[' 
      if (tmp_hostname[0] == '[')
      {
        hptr = tmp_hostname + 1;
        // We also need to remove ']'. This is required because we need to validate ipv6 address . Which is not possible with '[]'
        if ((tmp_ptr = rindex(tmp_hostname , ']'))) // This means ] is present
          *tmp_ptr = '\0';
      }
      else {
        hptr = tmp_hostname;
      }
      //Check for ip version types (ipv4 or ipv6)
      ret = is_valid_ip(hptr);
      free(tmp_hostname);

      //Seperate out host name and port hptr will now point to host and server_port will store the port given by user .  
      hptr = nslb_split_host_port(hostname, &server_port);
  
      NSDL4_HTTP(NULL, NULL, "hostname = [%s], server_port = [%d] ret = [%d]  ", hptr, server_port, ret );

      //If port is not specified we will set default port for ipv4 as well as ipv6 
      if (server_port == 0)
      {
        switch (request_type)
        {
          case 0:               //HTTP
               if (ret == IPv6)
                 server_port = 6880;
               else
                 server_port = 80;
               NSDL1_HTTP(NULL, NULL, "Setting HTTP Port = %d", server_port);
               break;
          case 1:               //HTTPS
              if (ret == IPv6)
                server_port = 6443;
              else
                server_port = 443;
              NSDL1_HTTP(NULL, NULL, "Setting HTTPS Port = %d", server_port);
        }
      }

      //Allocate memory for global_settings->file_upload_info
      MY_MALLOC(global_settings->file_upload_info, sizeof(FileUploadInfo), "FileUploadInfo", -1);
            
      global_settings->file_upload_info->tp_init_size = 1;
      global_settings->file_upload_info->tp_max_size = 1;
      global_settings->file_upload_info->mq_init_size = 5;
      global_settings->file_upload_info->mq_max_size = 60;
      strcpy(global_settings->file_upload_info->server_ip, hptr);
      global_settings->file_upload_info->server_port = server_port;
      global_settings->file_upload_info->protocol = request_type;
      strcpy(global_settings->file_upload_info->url, request_line);
      global_settings->file_upload_info->max_conn_retry = 5;
      global_settings->file_upload_info->retry_timer = 60;
  
      ns_init_file_upload(global_settings->file_upload_info->tp_init_size, global_settings->file_upload_info->tp_max_size,
                          global_settings->file_upload_info->mq_init_size, global_settings->file_upload_info->mq_max_size);

      ns_config_file_upload(global_settings->file_upload_info->server_ip, global_settings->file_upload_info->server_port,
                            global_settings->file_upload_info->protocol, global_settings->file_upload_info->url,
                            global_settings->file_upload_info->max_conn_retry, global_settings->file_upload_info->retry_timer, ns_event_fd);
  #ifndef CAV_MAIN
    }
  }
  #endif
  global_settings->monitor_type = monitor_type;
  NSLB_MALLOC(monitor_data_buffer, MONITOR_DATA_BUFFER_SIZE + 1, "monitor_data_buffer", -1, NULL);
 
  NSDL2_PARSING(NULL, NULL, "End: global_settings->monitor_type = %d", global_settings->monitor_type);
  return 0;
}

void send_test_monitor_data(VUser *vptr, char *buffer, int length)
{

  #ifndef CAV_MAIN
  fprintf(stdout, "RTG_DATA_START|%s\n", buffer);
  #else
  cm_send_message_to_parent(NS_TEST_RESULT, vptr->sm_mon_info->mon_id, buffer);
  #endif
}

void send_test_monitor_gdf_data(VUser *vptr, int type, avgtime **g_avg, cavgtime **g_cavg)
{
  NSDL2_MISC(NULL, NULL, "Method called with type = %d, g_avg = %p, g_cavg = %p", type, g_avg, g_cavg);

  if((global_settings->monitor_type == MONITOR_TYPE_NONE) || (type != global_settings->monitor_type))
  {
    //ERROR
    return;
  }
  monitor_data[type].create_data(vptr, g_avg, g_cavg);
  send_test_monitor_data(vptr, monitor_data_buffer, monitor_data_buffer_len);
}

void start_test_monitor_data()
{
  monitor_data_buffer_len = 0;
}

void end_test_monitor_data()
{
  monitor_data_buffer_len--;
  monitor_data_buffer[monitor_data_buffer_len] = '\0';
}

void fill_test_monitor_data_Long(Long_data data)
{
  monitor_data_buffer_len += 
              snprintf(monitor_data_buffer + monitor_data_buffer_len, MONITOR_DATA_BUFFER_SIZE - monitor_data_buffer_len, "%lf ", data);
}

void create_http_api_monitor_data(VUser *vptr, avgtime **g_avg, cavgtime **g_cavg)
{
  int i;
  Long_data data;
  int gv_idx = 0; //grp_idx = vptr->group_num; //SHOW_GROUP_DATA will be disabled
  avgtime *avg = NULL;
  //avgtime *tmp_avg = NULL;
  CavTestHTTPAvgTime *ct_avg = NULL;
  
  NSDL2_MISC(NULL, NULL, "Method called, g_avg = %p, g_cavg = %p", g_avg, g_cavg);

  avg = (avgtime*)g_avg[gv_idx];
  //avg = (avgtime*)((char*)tmp_avg + (grp_idx * g_avg_size_only_grp));
  
  ct_avg = (CavTestHTTPAvgTime*)((char*)avg + g_cavtest_http_avg_idx);

  start_test_monitor_data();
  /* Format:  <mon_id>:<sequence id>:<vectorname>|<data>\n
     Example: 1:0:GoogleTest>index| 2 3 4 6 7 8 9\n
  */
  #ifdef CAV_MAIN
  monitor_data_buffer_len += 
              snprintf(monitor_data_buffer + monitor_data_buffer_len, MONITOR_DATA_BUFFER_SIZE - monitor_data_buffer_len, "%d:%d:%s>%s|",
                       global_settings->monitor_idx, vptr->cur_page->page_id, global_settings->monitor_name, 
                       vptr->cur_page->page_name);
  #else
  monitor_data_buffer_len +=
             snprintf(monitor_data_buffer + monitor_data_buffer_len, MONITOR_DATA_BUFFER_SIZE - monitor_data_buffer_len, "%d:%d:%s>%s|",
                       global_settings->monitor_idx, runprof_table_shr_mem[0].start_page_idx, global_settings->monitor_name,
                       runprof_table_shr_mem[0].sess_ptr->first_page->page_name);
  #endif

  for(i = 0; i < http_api_max_idx; i++)
  {
    switch(i)
    {
      case http_api_total_time_idx:             //Total time
        if(ct_avg->total_time_count)
        {
          data = ct_avg->total_tot_time/ct_avg->total_time_count;
          fill_test_monitor_data_Long(data);    //average
                                                
          data = ct_avg->total_min_time;        
          fill_test_monitor_data_Long(data);    //min
                                                
          data = ct_avg->total_max_time;        
          fill_test_monitor_data_Long(data);    //max
                                                
          data = ct_avg->total_time_count;      
          fill_test_monitor_data_Long(data);    //count
        }
        else                                    
        {                                       
          data = 0;                             
          fill_test_monitor_data_Long(data);    //average
                                                
          data = -1;                            
          fill_test_monitor_data_Long(data);    //min
                                                
          data = 0;                             
          fill_test_monitor_data_Long(data);    //max
                                                
          data = 0;                             
          fill_test_monitor_data_Long(data);    //count
        }
        break;                                  
      case http_api_resolve_time_idx:           //Resolve time
        if(avg->url_dns_count)
        {
          data = avg->url_dns_tot_time/avg->url_dns_count;
          fill_test_monitor_data_Long(data);    //average
                                                
          data = avg->url_dns_min_time;         
          fill_test_monitor_data_Long(data);    //min
                                                
          data = avg->url_dns_max_time;         
          fill_test_monitor_data_Long(data);    //max
                                                
          data = avg->url_dns_count;            
          fill_test_monitor_data_Long(data);    //count
        }                                       
        else                                    
        {                                       
          data = 0;                             
          fill_test_monitor_data_Long(data);    //average
                                                
          data = -1;                            
          fill_test_monitor_data_Long(data);    //min
                                                
          data = 0;                             
          fill_test_monitor_data_Long(data);    //max
                                                
          data = 0;                             
          fill_test_monitor_data_Long(data);    //count
        }                                       
        break;                                  
      case http_api_connect_time_idx:           //Connect Time
        if(avg->url_conn_count)
        {        
          data = avg->url_conn_tot_time/avg->url_conn_count;
          fill_test_monitor_data_Long(data);    //average
                                                
          data = avg->url_conn_min_time;        
          fill_test_monitor_data_Long(data);    //min
                                                
          data = avg->url_conn_max_time;        
          fill_test_monitor_data_Long(data);    //max
                                                
          data = avg->url_conn_count;           
          fill_test_monitor_data_Long(data);    //count
        }                                       
        else                                    
        {                                       
          data = 0;                             
          fill_test_monitor_data_Long(data);    //average
                                                
          data = -1;                            
          fill_test_monitor_data_Long(data);    //min
                                                
          data = 0;                             
          fill_test_monitor_data_Long(data);    //max
                                                
          data = 0;                             
          fill_test_monitor_data_Long(data);    //count
        }                                       
        break;                                  
      case http_api_ssl_time_idx:               //SSL Handshak Time
        if(avg->url_ssl_count)
        {
          data = avg->url_ssl_tot_time/avg->url_ssl_count;
          fill_test_monitor_data_Long(data);    //average
                                                
          data = avg->url_ssl_min_time;         
          fill_test_monitor_data_Long(data);    //min
                                                
          data = avg->url_ssl_max_time;         
          fill_test_monitor_data_Long(data);    //max
                                                
          data = avg->url_ssl_count;            
          fill_test_monitor_data_Long(data);    //count
        }                                       
        else                                    
        {                                       
          data = 0;                             
          fill_test_monitor_data_Long(data);    //average
                                                
          data = -1;                            
          fill_test_monitor_data_Long(data);    //min
                                                
          data = 0;                             
          fill_test_monitor_data_Long(data);    //max
                                                
          data = 0;                             
          fill_test_monitor_data_Long(data);    //count
        }                                       
        break;                                  
      case http_api_send_time_idx:              //Send Time
        if(ct_avg->send_time_count)
        {
          data = ct_avg->send_tot_time/ct_avg->send_time_count;                                 
          fill_test_monitor_data_Long(data);    //average

          data = ct_avg->send_min_time;
          fill_test_monitor_data_Long(data);    //min

          data = ct_avg->send_max_time;
          fill_test_monitor_data_Long(data);    //max

          data = ct_avg->send_time_count; 
          fill_test_monitor_data_Long(data);    //count
        }
        else                                    
        {                                       
          data = 0;                             
          fill_test_monitor_data_Long(data);    //average
                                                
          data = -1;                            
          fill_test_monitor_data_Long(data);    //min
                                                
          data = 0;                             
          fill_test_monitor_data_Long(data);    //max
                                                
          data = 0;                             
          fill_test_monitor_data_Long(data);    //count
        }
        break;                                  
      case http_api_first_byte_time_idx:        //First Byte Time
        if(avg->url_frst_byte_rcv_count)
        {
          data = avg->url_frst_byte_rcv_tot_time/avg->url_frst_byte_rcv_count;
          fill_test_monitor_data_Long(data);    //average
         
          data = avg->url_frst_byte_rcv_min_time;
          fill_test_monitor_data_Long(data);    //min
         
          data = avg->url_frst_byte_rcv_max_time;
          fill_test_monitor_data_Long(data);    //max
         
          data = avg->url_frst_byte_rcv_count;
          fill_test_monitor_data_Long(data);    //count
        }                                       
        else                                    
        {                                       
          data = 0;                             
          fill_test_monitor_data_Long(data);    //average
                                                
          data = -1;                            
          fill_test_monitor_data_Long(data);    //min
                                                
          data = 0;                             
          fill_test_monitor_data_Long(data);    //max
                                                
          data = 0;                             
          fill_test_monitor_data_Long(data);    //count
        }                                       
        break;                                  
      case http_api_download_time_idx:          //Download Time
        if(avg->url_dwnld_count)
        {
          data = avg->url_dwnld_tot_time/avg->url_dwnld_count;
          fill_test_monitor_data_Long(data);    //average
                                                
          data = avg->url_dwnld_min_time;       
          fill_test_monitor_data_Long(data);    //min
                                                
          data = avg->url_dwnld_max_time;       
          fill_test_monitor_data_Long(data);    //max
                                                
          data = avg->url_dwnld_count;          
          fill_test_monitor_data_Long(data);    //count
        }                                       
        else                                    
        {                                       
          data = 0;                             
          fill_test_monitor_data_Long(data);    //average
                                                
          data = -1;                            
          fill_test_monitor_data_Long(data);    //min
                                                
          data = 0;                             
          fill_test_monitor_data_Long(data);    //max
                                                
          data = 0;                             
          fill_test_monitor_data_Long(data);    //count
        }
        break;                                  
      break;                                    
      case http_api_response_time_idx:          //Response Time
        if(avg->num_tries)
        {
          data = avg->url_overall_avg_time;       
          fill_test_monitor_data_Long(data);    //average
                                                
          data = avg->url_overall_min_time;     
          fill_test_monitor_data_Long(data);    //min
                                                
          data = avg->url_overall_max_time;     
          fill_test_monitor_data_Long(data);    //max
                                                
          data = avg->num_tries;                
          fill_test_monitor_data_Long(data);    //count
        }
        else                                    
        {                                       
          data = 0;                             
          fill_test_monitor_data_Long(data);    //average
                                                
          data = -1;                            
          fill_test_monitor_data_Long(data);    //min
                                                
          data = 0;                             
          fill_test_monitor_data_Long(data);    //max
                                                
          data = 0;                             
          fill_test_monitor_data_Long(data);    //count
        }
        break;                                    
      case http_api_redirtect_time_idx:         //Redirect Time
        if(ct_avg->redirect_time_count)
        {
          data = ct_avg->redirect_tot_time/ct_avg->redirect_time_count;
          fill_test_monitor_data_Long(data);    //average

          data = ct_avg->redirect_min_time;
          fill_test_monitor_data_Long(data);    //min

          data = ct_avg->redirect_max_time;
          fill_test_monitor_data_Long(data);    //max

          data = ct_avg->redirect_time_count;
          fill_test_monitor_data_Long(data);    //count
        }
        else                                    
        {                                       
          data = 0;                             
          fill_test_monitor_data_Long(data);    //average
                                                
          data = -1;                            
          fill_test_monitor_data_Long(data);    //min
                                                
          data = 0;                             
          fill_test_monitor_data_Long(data);    //max
                                                
          data = 0;                             
          fill_test_monitor_data_Long(data);    //count
        }
        break;                                    
      case http_api_request_header_size_idx:    //Request Header Size
        data = avg->tx_bytes - ct_avg->req_body_size;                                 
        fill_test_monitor_data_Long(data); 
        break;                                    
      case http_api_request_body_size_idx:      //Request Body Size
        data = ct_avg->req_body_size;                                 
        fill_test_monitor_data_Long(data); 
        break;                                    
      case http_api_request_size_idx:           //Request Size
        data = avg->tx_bytes;        
        fill_test_monitor_data_Long(data); 
        break;                                   
      case http_api_response_header_size_idx:   //Response Header Size
        data = avg->rx_bytes - avg->total_bytes;
        fill_test_monitor_data_Long(data); 
        break;                                   
      case http_api_response_body_size_idx:     //Response Body Size
        data = avg->total_bytes;
        fill_test_monitor_data_Long(data); 
        break;                                   
      case http_api_response_size_idx:          //Response Size
        data = avg->rx_bytes;        
        fill_test_monitor_data_Long(data); 
        break;                                   
      case http_api_status_code_idx:            //Status Code
        fill_test_monitor_data_Long(ct_avg->status_code); 
        break;                                   
      case http_api_availabilty_status_idx:     //Availability Status
        fill_test_monitor_data_Long(ct_avg->avail_status); 
        break;  
      case http_api_unavailabilty_reason_idx:   //Unavailability Reason
        data = 0;
        fill_test_monitor_data_Long(data); 
        break;  
    };
  }
  end_test_monitor_data();
}

void create_web_page_audit_monitor_data(VUser *vptr, avgtime **g_avg, cavgtime **g_cavg)
{
  int j;
  Long_data data;
  int gv_idx = 0, i = 0;
  avgtime *avg = NULL;
  RBUPageStatAvgTime *rbu_page_stat_avg = NULL;
  CavTestWebAvgTime *ct_avg = NULL;

  NSDL2_MISC(NULL, NULL, "Method called, g_avg = %p, g_cavg = %p", g_avg, g_cavg);

  avg = (avgtime *)g_avg[gv_idx];
  rbu_page_stat_avg = (RBUPageStatAvgTime *) ((char*) avg + rbu_page_stat_data_gp_idx);

  ct_avg = (CavTestWebAvgTime*)((char*)avg + g_cavtest_web_avg_idx);

  start_test_monitor_data();
  /* Format:  <mon_id>:<sequence id>:<vectorname>|<data>\n
     Example: 1:0:GoogleTest>index| 2 3 4 6 7 8 9\n
  */
  #ifdef CAV_MAIN
  monitor_data_buffer_len += 
              snprintf(monitor_data_buffer + monitor_data_buffer_len, MONITOR_DATA_BUFFER_SIZE - monitor_data_buffer_len, "%d:%d:%s>%s|",
                       global_settings->monitor_idx, vptr->cur_page->page_id, global_settings->monitor_name, 
                       vptr->cur_page->page_name);
  #else
  monitor_data_buffer_len +=
             snprintf(monitor_data_buffer + monitor_data_buffer_len, MONITOR_DATA_BUFFER_SIZE - monitor_data_buffer_len, "%d:%d:%s>%s|",
                       global_settings->monitor_idx, runprof_table_shr_mem[0].start_page_idx, global_settings->monitor_name,
                       runprof_table_shr_mem[0].sess_ptr->first_page->page_name);
  #endif

  for(j = 0; j < web_page_audit_max_idx; j++)
  {
    switch(j)
    {
      case web_page_audit_total_time_idx:       //Total Time
        if(rbu_page_stat_avg[i].PageLoad_counts > 0)
        {
          data = (double)(((double)rbu_page_stat_avg[i].PageLoad_time) /
                            ((double)(1000.0*(double)rbu_page_stat_avg[i].PageLoad_counts)));
          fill_test_monitor_data_Long(data);    //average
                                                
          data = (double)rbu_page_stat_avg[i].PageLoad_min_time/1000.0;
          fill_test_monitor_data_Long(data);    //min
                                                
          data = (double)rbu_page_stat_avg[i].PageLoad_max_time/1000.0;
          fill_test_monitor_data_Long(data);    //max
                                                
          data = rbu_page_stat_avg[i].PageLoad_counts;
          fill_test_monitor_data_Long(data);    //count
        }
        else                                    
        {                                       
          data = 0;                             
          fill_test_monitor_data_Long(data);    //average
                                                
          data = -1;                            
          fill_test_monitor_data_Long(data);    //min
                                                
          data = 0;                             
          fill_test_monitor_data_Long(data);    //max
                                                
          data = 0;                             
          fill_test_monitor_data_Long(data);    //count
        }
        break;
      case web_page_audit_resolve_time_idx:     //DNS Time
        if(rbu_page_stat_avg[i].dns_counts > 0)
        {
          data = (double)(((double)rbu_page_stat_avg[i].dns_time) /
                            ((double)(1000.0*(double)rbu_page_stat_avg[i].dns_counts)));
          fill_test_monitor_data_Long(data);    //average
                                                
          data = (double)rbu_page_stat_avg[i].dns_min_time/1000.0;
          fill_test_monitor_data_Long(data);    //min
                                                
          data = (double)rbu_page_stat_avg[i].dns_max_time/1000.0;
          fill_test_monitor_data_Long(data);    //max
                                                
          data = rbu_page_stat_avg[i].dns_counts;
          fill_test_monitor_data_Long(data);    //count
        }
        else                                    
        {                                       
          data = 0;                             
          fill_test_monitor_data_Long(data);    //average
                                                
          data = -1;                            
          fill_test_monitor_data_Long(data);    //min
                                                
          data = 0;                             
          fill_test_monitor_data_Long(data);    //max
                                                
          data = 0;                             
          fill_test_monitor_data_Long(data);    //count
        }      
        break;
      case web_page_audit_connect_time_idx:     //Connect Time
        if(rbu_page_stat_avg[i].connect_counts > 0)
        {
          data = (double)(((double)rbu_page_stat_avg[i].connect_time) /
                            ((double)(1000.0*(double)rbu_page_stat_avg[i].connect_counts)));
          fill_test_monitor_data_Long(data);    //average

          data = (double)rbu_page_stat_avg[i].connect_min_time/1000.0;
          fill_test_monitor_data_Long(data);    //min

          data = (double)rbu_page_stat_avg[i].connect_max_time/1000.0;
          fill_test_monitor_data_Long(data);    //max

          data = rbu_page_stat_avg[i].connect_counts;
          fill_test_monitor_data_Long(data);    //count
        }
        else 
        {     
          data = 0;                             
          fill_test_monitor_data_Long(data);    //average

          data = -1;
          fill_test_monitor_data_Long(data);    //min

          data = 0;
          fill_test_monitor_data_Long(data);    //max

          data = 0;
          fill_test_monitor_data_Long(data);    //count
        }
        break;
      case web_page_audit_ssl_time_idx:         //SSL Time
        if(rbu_page_stat_avg[i].ssl_counts > 0)
        {
          data = (double)(((double)rbu_page_stat_avg[i].ssl_time) /
                            ((double)(1000.0*(double)rbu_page_stat_avg[i].ssl_counts)));
          fill_test_monitor_data_Long(data);    //average

          data = (double)rbu_page_stat_avg[i].ssl_min_time/1000.0;
          fill_test_monitor_data_Long(data);    //min

          data = (double)rbu_page_stat_avg[i].ssl_max_time/1000.0;
          fill_test_monitor_data_Long(data);    //max

          data = rbu_page_stat_avg[i].ssl_counts;
          fill_test_monitor_data_Long(data);    //count
        }
        else 
        {     
          data = 0;                             
          fill_test_monitor_data_Long(data);    //average

          data = -1;
          fill_test_monitor_data_Long(data);    //min

          data = 0;
          fill_test_monitor_data_Long(data);    //max

          data = 0;
          fill_test_monitor_data_Long(data);    //count
        }
        break;
      case web_page_audit_send_time_idx:        //TODO:Send Time
        data = 0;                              
        fill_test_monitor_data_Long(data);      //average
                                               
        data = -1;                             
        fill_test_monitor_data_Long(data);      //min
                                               
        data = 0;                              
        fill_test_monitor_data_Long(data);      //max
                                               
        data = 0;                              
        fill_test_monitor_data_Long(data);      //count
        break;                                  
      case web_page_audit_first_byte_time_idx:  //TODO: First Byte Time
        data = 0;                              
        fill_test_monitor_data_Long(data);      //average
                                                
        data = -1;                              
        fill_test_monitor_data_Long(data);      //min
                                                
        data = 0;                               
        fill_test_monitor_data_Long(data);      //max
                                                
        data = 0;                               
        fill_test_monitor_data_Long(data);      //count
        break;
      case web_page_audit_download_time_idx:    //Download Time
        if(rbu_page_stat_avg[i].rcv_counts > 0)
        {
          data = (double)(((double)rbu_page_stat_avg[i].rcv_time) /
                            ((double)(1000.0*(double)rbu_page_stat_avg[i].rcv_counts)));
          fill_test_monitor_data_Long(data);    //average

          data = (double)rbu_page_stat_avg[i].rcv_min_time/1000.0;
          fill_test_monitor_data_Long(data);    //min

          data = (double)rbu_page_stat_avg[i].rcv_max_time/1000.0;
          fill_test_monitor_data_Long(data);    //max

          data = rbu_page_stat_avg[i].rcv_counts;
          fill_test_monitor_data_Long(data);    //count
        }
        else 
        {     
          data = 0;                             
          fill_test_monitor_data_Long(data);    //average

          data = -1;
          fill_test_monitor_data_Long(data);    //min

          data = 0;
          fill_test_monitor_data_Long(data);    //max

          data = 0;
          fill_test_monitor_data_Long(data);    //count
        }
        break;
      case web_page_audit_response_time_idx:    //Response Time
        if(rbu_page_stat_avg[i].url_resp_counts > 0)
        {
          data = (double)(((double)rbu_page_stat_avg[i].url_resp_time) /
                            ((double)(1000.0*(double)rbu_page_stat_avg[i].url_resp_counts)));
          fill_test_monitor_data_Long(data);    //average

          data = (double)rbu_page_stat_avg[i].url_resp_min_time/1000.0;
          fill_test_monitor_data_Long(data);    //min

          data = (double)rbu_page_stat_avg[i].url_resp_max_time/1000.0;
          fill_test_monitor_data_Long(data);    //max

          data = rbu_page_stat_avg[i].url_resp_counts;
             fill_test_monitor_data_Long(data);    //count
        }
        else
        {
          data = 0;
          fill_test_monitor_data_Long(data);    //average

          data = -1;
          fill_test_monitor_data_Long(data);    //min

          data = 0;
          fill_test_monitor_data_Long(data);    //max

          data = 0;
          fill_test_monitor_data_Long(data);    //count
        }
        break;
     case web_page_audit_tbt_time_idx:
        if(rbu_page_stat_avg[i].tbt_counts > 0)
        {
          data = (double)(((double)rbu_page_stat_avg[i].tbt) /                  
                           ((double)(1000.0*(double)rbu_page_stat_avg[i].tbt_counts)));
          fill_test_monitor_data_Long(data);    //average

          data = (double)rbu_page_stat_avg[i].tbt_min/1000.0;
          fill_test_monitor_data_Long(data);    //min

          data = (double)rbu_page_stat_avg[i].tbt_max/1000.0;
          fill_test_monitor_data_Long(data);    //max

          data = rbu_page_stat_avg[i].tbt_counts;
          fill_test_monitor_data_Long(data);    //count
        }
        else
        {
          data = 0;
          fill_test_monitor_data_Long(data);    //average

          data = -1;
          fill_test_monitor_data_Long(data);    //min

          data = 0;
          fill_test_monitor_data_Long(data);    //max

          data = 0;
          fill_test_monitor_data_Long(data);    //count
            }
        break;
     case web_page_audit_lcp_time_idx:
        if(rbu_page_stat_avg[i].lcp_count > 0)
        {
          data = (double)(((double)rbu_page_stat_avg[i].lcp) /
                            ((double)(1000.0*(double)rbu_page_stat_avg[i].lcp_count)));
          fill_test_monitor_data_Long(data);    //average

          data = (double)rbu_page_stat_avg[i].lcp_min/1000.0;
          fill_test_monitor_data_Long(data);    //min

          data = (double)rbu_page_stat_avg[i].lcp_max/1000.0;
          fill_test_monitor_data_Long(data);    //max

          data = rbu_page_stat_avg[i].lcp_count;
          fill_test_monitor_data_Long(data);    //count
        }
        else
        {
          data = 0;
          fill_test_monitor_data_Long(data);    //average

          data = -1;
          fill_test_monitor_data_Long(data);    //min

          data = 0;
            fill_test_monitor_data_Long(data);    //max

          data = 0;
          fill_test_monitor_data_Long(data);    //count
        }
        break;
     case web_page_audit_cls_time_idx:
        if(rbu_page_stat_avg[i].cls_count > 0)
        {
          data = (double)(((double)rbu_page_stat_avg[i].cls) /
                            ((double)(1000.0*(double)rbu_page_stat_avg[i].cls_count)));
          fill_test_monitor_data_Long(data);    //average

          data = (double)rbu_page_stat_avg[i].cls_min/1000.0;
          fill_test_monitor_data_Long(data);    //min

          data = (double)rbu_page_stat_avg[i].cls_max/1000.0;
          fill_test_monitor_data_Long(data);    //max

          data = rbu_page_stat_avg[i].cls_count;
          fill_test_monitor_data_Long(data);    //count
        }
        else 
        {     
          data = 0;                             
          fill_test_monitor_data_Long(data);    //average

          data = -1;
          fill_test_monitor_data_Long(data);    //min

          data = 0;
          fill_test_monitor_data_Long(data);    //max

          data = 0;
          fill_test_monitor_data_Long(data);    //count
        }
        break;
      case web_page_audit_redirtect_time_idx:  //TODO:Redirect Time
        data = 0;                              
        fill_test_monitor_data_Long(data);     //average
                                               
        data = -1;                             
        fill_test_monitor_data_Long(data);     //min
                                               
        data = 0;                              
        fill_test_monitor_data_Long(data);     //max
                                               
        data = 0;                              
        fill_test_monitor_data_Long(data);     //count
        break;
      case web_page_audit_request_header_size_idx:  //Request Header Size
        data = ct_avg->req_header_size;
        fill_test_monitor_data_Long(data);
        break;
      case web_page_audit_request_body_size_idx:    //Request Body Size
        data = rbu_page_stat_avg[i].cur_rbu_bytes_send - ct_avg->req_header_size;
        fill_test_monitor_data_Long(data);
        break;
      case web_page_audit_request_size_idx:         //Request Size
        data = rbu_page_stat_avg[i].cur_rbu_bytes_send;
        fill_test_monitor_data_Long(data);
        break;
      case web_page_audit_response_header_size_idx: //Response Header Size
        data = ct_avg->res_header_size;
        fill_test_monitor_data_Long(data);
        break;
      case web_page_audit_response_body_size_idx:   //Response Body Size
        data = rbu_page_stat_avg[i].cur_rbu_bytes_recieved;
        fill_test_monitor_data_Long(data);
        break;
      case web_page_audit_response_size_idx:        //Response Size
        data = ct_avg->res_header_size + rbu_page_stat_avg[i].cur_rbu_bytes_recieved; //Header size + Body size
        fill_test_monitor_data_Long(data);
        break;
      case web_page_audit_status_code_idx:          //Status Code
        data = ct_avg->status_code;
        fill_test_monitor_data_Long(data);
        break;
      case web_page_audit_availabilty_status_idx:   //Availibity Status
        data = rbu_page_stat_avg[i].pg_avail;
        fill_test_monitor_data_Long(data);
        break;
      case web_page_audit_unavailabilty_reason_idx: //Unavailibity Reason
        data = 0;
        fill_test_monitor_data_Long(data); 
        break;
      case web_page_audit_domload_time_idx:         //DOMLoad Time
        if(rbu_page_stat_avg[i].DOMContent_Loaded_counts > 0)
        {
          data = (double)(((double)rbu_page_stat_avg[i].DOMContent_Loaded_time) /
                            ((double)(1000.0 * (double)rbu_page_stat_avg[i].DOMContent_Loaded_counts)));
          fill_test_monitor_data_Long(data);    //average

          data = (double)rbu_page_stat_avg[i].DOMContent_Loaded_min_time/1000.0;
          fill_test_monitor_data_Long(data);    //min

          data = (double)rbu_page_stat_avg[i].DOMContent_Loaded_max_time/1000.0;
          fill_test_monitor_data_Long(data);    //max

          data = rbu_page_stat_avg[i].DOMContent_Loaded_counts;
          fill_test_monitor_data_Long(data);    //count
        }
        else                                    
        {                                       
          data = 0;                             
          fill_test_monitor_data_Long(data);    //average
                                                
          data = -1;                            
          fill_test_monitor_data_Long(data);    //min
                                                
          data = 0;                             
          fill_test_monitor_data_Long(data);    //max
                                                
          data = 0;                             
          fill_test_monitor_data_Long(data);    //count
        }
        break;
      case web_page_audit_onload_time_idx:          //ONLoad Time
        if(rbu_page_stat_avg[i].OnLoad_counts > 0)
        {
          data = (double)(((double)rbu_page_stat_avg[i].OnLoad_time) /
                            ((double)(1000.0*(double)rbu_page_stat_avg[i].OnLoad_counts))); 
          fill_test_monitor_data_Long(data);    //average
                                                
          data = (double)rbu_page_stat_avg[i].OnLoad_min_time/1000.0;
          fill_test_monitor_data_Long(data);    //min
                                                
          data = (double)rbu_page_stat_avg[i].OnLoad_max_time/1000.0;
          fill_test_monitor_data_Long(data);    //max
                                                
          data = rbu_page_stat_avg[i].OnLoad_counts;
          fill_test_monitor_data_Long(data);    //count
        }
        else                                    
        {                                       
          data = 0;                             
          fill_test_monitor_data_Long(data);    //average
                                                
          data = -1;                            
          fill_test_monitor_data_Long(data);    //min
                                                
          data = 0;                             
          fill_test_monitor_data_Long(data);    //max
                                                
          data = 0;                             
          fill_test_monitor_data_Long(data);    //count
        }
        break;
      case web_page_audit_overall_time_idx:     //Overall Time
        if(rbu_page_stat_avg[i].PageLoad_counts > 0)
        {
          data = (double)(((double)rbu_page_stat_avg[i].PageLoad_time) /
                            ((double)(1000.0*(double)rbu_page_stat_avg[i].PageLoad_counts)));
          fill_test_monitor_data_Long(data);    //average
                                                
          data = (double)rbu_page_stat_avg[i].PageLoad_min_time/1000.0;
          fill_test_monitor_data_Long(data);    //min
                                                
          data = (double)rbu_page_stat_avg[i].PageLoad_max_time/1000.0;
          fill_test_monitor_data_Long(data);    //max
                                                
          data = rbu_page_stat_avg[i].PageLoad_counts;
          fill_test_monitor_data_Long(data);    //count
        }
        else                                    
        {                                       
          data = 0;                             
          fill_test_monitor_data_Long(data);    //average
                                                
          data = -1;                            
          fill_test_monitor_data_Long(data);    //min
                                                
          data = 0;                             
          fill_test_monitor_data_Long(data);    //max
                                                
          data = 0;                             
          fill_test_monitor_data_Long(data);    //count
        }
        break;
      case web_page_audit_tti_time_idx:         //TTI Time
        if(rbu_page_stat_avg[i].TTI_counts > 0)
        {
          data = (double)(((double)rbu_page_stat_avg[i].TTI_time) /
                            ((double)(1000.0*(double)rbu_page_stat_avg[i].TTI_counts))); 
          fill_test_monitor_data_Long(data);    //average
                                                
          data = (double)rbu_page_stat_avg[i].TTI_min_time/1000.0;
          fill_test_monitor_data_Long(data);    //min
                                                
          data = (double)rbu_page_stat_avg[i].TTI_max_time/1000.0;
          fill_test_monitor_data_Long(data);    //max
                                                
          data = rbu_page_stat_avg[i].TTI_counts;
          fill_test_monitor_data_Long(data);    //count
        }
        else                                    
        {                                       
          data = 0;                             
          fill_test_monitor_data_Long(data);    //average
                                                
          data = -1;                            
          fill_test_monitor_data_Long(data);    //min
                                                
          data = 0;                             
          fill_test_monitor_data_Long(data);    //max
                                                
          data = 0;                             
          fill_test_monitor_data_Long(data);    //count
        }
        break;
      case web_page_audit_start_render_time_idx://Start Render Time
        if(rbu_page_stat_avg[i]._cav_startRender_counts > 0)
        {
          data = (double)(((double)rbu_page_stat_avg[i]._cav_startRender_time) /
                            ((double)(1000.0*(double)rbu_page_stat_avg[i]._cav_startRender_counts)));
          fill_test_monitor_data_Long(data);    //average
                                                
          data = (double)rbu_page_stat_avg[i]._cav_startRender_min_time/1000.0;
          fill_test_monitor_data_Long(data);    //min
                                                
          data = (double)rbu_page_stat_avg[i]._cav_startRender_max_time/1000.0;
          fill_test_monitor_data_Long(data);    //max
                                                
          data = rbu_page_stat_avg[i]._cav_startRender_counts;
          fill_test_monitor_data_Long(data);    //count
        }
        else                                    
        {                                       
          data = 0;                             
          fill_test_monitor_data_Long(data);    //average
                                                
          data = -1;                            
          fill_test_monitor_data_Long(data);    //min
                                                
          data = 0;                             
          fill_test_monitor_data_Long(data);    //max
                                                
          data = 0;                             
          fill_test_monitor_data_Long(data);    //count
        }
        break;
      case web_page_audit_visual_complete_time_idx:  //Visual Complete Time
        if(rbu_page_stat_avg[i].visually_complete_counts > 0)
        {
          data = (double)(((double)rbu_page_stat_avg[i].visually_complete_time) /
                            ((double)(1000.0*(double)rbu_page_stat_avg[i].visually_complete_counts))); 
          fill_test_monitor_data_Long(data);    //average
                                                
          data = (double)rbu_page_stat_avg[i].visually_complete_min_time/1000.0;
          fill_test_monitor_data_Long(data);    //min
                                                
          data = (double)rbu_page_stat_avg[i].visually_complete_max_time/1000.0;
          fill_test_monitor_data_Long(data);    //max
                                                
          data = rbu_page_stat_avg[i].visually_complete_counts;
          fill_test_monitor_data_Long(data);    //count
        }
        else                                    
        {                                       
          data = 0;                             
          fill_test_monitor_data_Long(data);    //average
                                                
          data = -1;                            
          fill_test_monitor_data_Long(data);    //min
                                                
          data = 0;                             
          fill_test_monitor_data_Long(data);    //max
                                                
          data = 0;                             
          fill_test_monitor_data_Long(data);    //count
        }
        break;
      case web_page_audit_page_weight_idx:          //Page Weight
        data = rbu_page_stat_avg[i].cur_rbu_page_wgt;
        fill_test_monitor_data_Long(data);
        break;
      case web_page_audit_js_size_idx:              //JS Size
        data = rbu_page_stat_avg[i].cur_rbu_js_size;
        fill_test_monitor_data_Long(data);
        break;
      case web_page_audit_css_size_idx:            //CSS Size
        data = rbu_page_stat_avg[i].cur_rbu_css_size;
        fill_test_monitor_data_Long(data);
        break;
      case web_page_audit_img_weight_idx:          //Img Weight
        data = rbu_page_stat_avg[i].cur_rbu_img_wgt;
        fill_test_monitor_data_Long(data);
        break;
      case web_page_audit_dom_ele_idx:            //DOM Element
        data = rbu_page_stat_avg[i].cur_rbu_dom_element;
        fill_test_monitor_data_Long(data);
        break;
      case web_page_audit_page_score_idx:         //Page Score
        data = rbu_page_stat_avg[i].cur_rbu_pg_speed;
        fill_test_monitor_data_Long(data);
        break;
    }
  }
  end_test_monitor_data();
}

// Called by parent
inline void update_avgtime_size_for_cavtest()
{
  NSDL2_GDF(NULL, NULL, "Method Called, g_avgtime_size = %d, g_cavtest_http_avg_idx = %d",
                                          g_avgtime_size, g_cavtest_http_avg_idx);
  
  if(global_settings->monitor_type == HTTP_API)
  {
    NSDL2_GDF(NULL, NULL, "Cavisson Test Monitor is enabled for HTTP API.");
    g_cavtest_http_avg_idx = g_avgtime_size;
    g_avgtime_size += sizeof(CavTestHTTPAvgTime);

    NSDL2_GDF(NULL, NULL, "After g_avgtime_size = %d, g_cavtest_http_avg_idx = %d",
                                          g_avgtime_size, g_cavtest_http_avg_idx);
  }
  else if(global_settings->monitor_type == WEB_PAGE_AUDIT)
  {
    NSDL2_GDF(NULL, NULL, "Cavisson Test Monitor is enabled for Web Page Audit.");
    g_cavtest_web_avg_idx = g_avgtime_size;
    g_avgtime_size += sizeof(CavTestWebAvgTime);

    NSDL2_GDF(NULL, NULL, "After g_avgtime_size = %d, g_cavtest_http_avg_idx = %d",
                                          g_avgtime_size, g_cavtest_web_avg_idx);
  }
  else
  {
    NSDL2_GDF(NULL, NULL, "Cavisson Test Monitor is disabled.");
  }
}

inline void set_cavtest_data_avgtime_ptr()
{
  NSDL2_GDF(NULL, NULL, "Method Called");

  if(global_settings->monitor_type == HTTP_API)
  {
    cavtest_http_avg = (CavTestHTTPAvgTime*)((char *)average_time + g_cavtest_http_avg_idx);
    NSDL2_GDF(NULL, NULL, "Cavisson Test Monitor is enabled for HTTP API, cavtest_http_avg = %p, g_cavtest_http_avg_idx = %d",
                           cavtest_http_avg, g_cavtest_http_avg_idx);
  }
  else if(global_settings->monitor_type == WEB_PAGE_AUDIT)
  {
    cavtest_web_avg = (CavTestWebAvgTime*)((char *)average_time + g_cavtest_web_avg_idx);
    NSDL2_GDF(NULL, NULL, "Cavisson Test Monitor is enabled for Web Page Audit, cavtest_web_avg = %p, g_cavtest_web_avg_idx = %d",
                           cavtest_web_avg, g_cavtest_web_avg_idx);
  }
  else
  {
    NSDL2_GDF(NULL, NULL, "Cavisson Test Monitor is disabled.");
    cavtest_http_avg = NULL;
    cavtest_web_avg = NULL;
  }
}
