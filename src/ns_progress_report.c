/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Name    : ns_progress_report.c                                  * 
 * Author  : Devendar/Manish/Gaurav                                *
 * Purpose : Handling progress/finished report                     *
 * History : 27 November 2017                                      *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ns_log.h"
#include "ns_trace_level.h"
#include "ns_alloc.h"
#include "ns_msg_def.h"
#include "ns_gdf.h"
#include "ns_group_data.h"
#include "util.h"
#include "wait_forever.h"
#include "nslb_time_stamp.h"
#include "netstorm.h"
#include "ns_ftp.h"
#include "ns_ldap.h"
#include "ns_imap.h"
#include "ns_jrmi.h"
#include "ns_group_data.h"
#include "ns_ip_data.h"
#include "ns_rbu_page_stat.h"
#include "ns_page_based_stats.h"
#include "ns_msg_com_util.h"
#include "output.h"
#include "ns_server_ip_data.h"
#include "ns_dynamic_avg_time.h"
#include "ns_trans_normalization.h"
#include "ns_trans_parse.h"
#include "ns_user_monitor.h"
#include "ns_runtime_runlogic_progress.h"
#include "ns_websocket_reporting.h"
#include "ns_http_cache_reporting.h"
#include "ns_proxy_server_reporting.h"
#include "ns_network_cache_reporting.h"
#include "ns_dns_reporting.h"
#include "dos_attack/ns_dos_attack_reporting.h"
#include "ns_progress_report.h"
#include "ns_percentile.h"
#include "netomni/src/core/ni_scenario_distribution.h"
#include "nslb_util.h"
#include "ns_netstorm_diagnostics.h"
#include "deliver_report.h"
#include "ns_event_log.h"
#include "ns_exit.h"
#include "ns_replay_db_query.h"
#include "ns_schedule_fcs.h"
#include "ns_svr_ip_normalization.h"
#include "ns_rbu_domain_stat.h"
#include "ns_jmeter.h"
#include "ns_handle_alert.h"
#include "ns_xmpp.h"
#include "ns_http_status_codes.h"
#include "ns_data_handler_thread.h"
#include "ns_error_msg.h"
#include "nslb_dashboard_alert.h"
#include "nslb_encode.h"
#include "nslb_bitflag.h"
#include "ns_socket.h"
#include "ns_test_monitor.h"
#include "comp_decomp/nslb_comp_decomp.h"

void **progress_data_pool;  // array of memory pool pointers for every child
SampleInfo *progress_info;  // array of Sample Info
extern struct timeval timeout;
extern struct timeval kill_timeout;
extern avgtime **g_cur_avg;
extern cavgtime **g_cur_finished;
extern avgtime **g_next_avg;
extern cavgtime **g_next_finished;
extern avgtime **g_dest_avg;
extern cavgtime **g_dest_cavg;
extern avgtime **g_end_avg;
extern int sgrp_used_genrator_entries;
extern unsigned int cur_sample;
extern int *g_last_pr_sample;

//extern int num_active; /* number of Finish expected */
//extern int num_pge; /* numprogress expected */

/*******************************************************
 * Usage PROGRESS_REPORT_QUEUE <queue length> Keyword
 ******************************************************/
void kw_set_progress_report_queue_usage(char *err)
{
  NSTL1_OUT(NULL, NULL, "Error: Invalid value of PROGRESS_REPORT_QUEUE keyword: %s\n", err);
  NSTL1_OUT(NULL, NULL, "  Usage: PROGRESS_REPORT_QUEUE <num_samples> <max_queue_to_flush> <data_comp_tpye>\n");
  NSTL1_OUT(NULL, NULL, "  This keyword enables settings to queue progress report\n");
  NSTL1_OUT(NULL, NULL, "  Num samples: min - 2 , default - 8 and max - child timeout samples\n");
  NSTL1_OUT(NULL, NULL, "  Max queue to flush: min - 1 , default - 4 and max -> equal to num sample \n");
  NSTL1_OUT(NULL, NULL, "  Data compression type: default - NO_COMPRESSION, It can be GZIP_COMPRESSION or BR_COMPRESSION\n");
  NS_EXIT(-1, "%s\nUsage: PROGRESS_REPORT_QUEUE <num_samples> <max_queue_to_flush> <data_comp_tpye>\n");
}

/*******************************************************
 * Parsing PROGRESS_REPORT_QUEUE <queue length> Keyword 
 * queue length cannot be greater than child timeout
   progress sample
 
 *******************************************************/
void kw_set_progress_report_queue(char *buf)
{
  char keyword[1024] = "";
  char tmp[1024] = "";
  int num_samples = 0;
  int num;
  int max_queue_to_flush = 4;
  int data_comp_tpye = NSLB_COMP_NONE;  //NO_COMPRESSION means data(progress or percentile) will not compressed while transfered on socket
  num = sscanf(buf, "%s %d %d %d %s", keyword, &num_samples, &max_queue_to_flush, &data_comp_tpye, tmp);

  NSDL1_PARSING(NULL, NULL, "Method called, buf = %s, num = %d , key = [%s], num_samples = [%d], data_comp_tpye = [%d]", 
                             buf, num, keyword, num_samples, data_comp_tpye);

  if(num < 2 || num > 4)
    kw_set_progress_report_queue_usage("Invalid idenfier");

  if(num_samples < 2)
    kw_set_progress_report_queue_usage("num samples cannot be less 2");

  if(!max_queue_to_flush) //atleast 1
    kw_set_progress_report_queue_usage("max_queue_to_flush cannot be 0");

  if(max_queue_to_flush > num_samples)
    max_queue_to_flush = num_samples; //No recovery
  
  //We are not supporting BR as it taking lots of time 
  if(data_comp_tpye < 0 || data_comp_tpye > 2)
    kw_set_progress_report_queue_usage("data_comp_tpye can only be 0/1/2");

  /*Bug 77908 - When progress sec is less than 4 seconds then create queue of 30 */
  if((num_samples * global_settings->progress_secs) < 30000)
  {
    num_samples = 30;
    max_queue_to_flush = 15;
    NS_DUMP_WARNING("Progress report queue is less than 30 due to progress interval %d msecs. Setting progress report queue length to %d",
                    global_settings->progress_secs, (loader_opcode == MASTER_LOADER)?32:30);
  }

  if(loader_opcode == MASTER_LOADER)
    num_samples += 2;

  global_settings->progress_report_queue_size = num_samples;
  global_settings->progress_report_max_queue_to_flush = max_queue_to_flush;
  global_settings->data_comp_type = data_comp_tpye;
  NSDL3_PARSING(NULL, NULL, "Method exit, progress_report_queue_size = %d, global_settings->data_comp_tpye = %d", 
                            global_settings->progress_report_queue_size, global_settings->data_comp_type);
}

/**********************************************************************
 * Purpose     : Initialization of pool and Sample Info
 * num_entries : Netcloud = total_generators + 1                                            
                 Netstorm = 0 + 1
 * Slots       : Queue length passed by PROGRESS_REPORT_QUEUE keyword
 * g_cur_avg   : Array of avg pointers, pointing to first slot of BL
 * g_next_avg  : Array of avg pointers, pointing to next slot of BL
 * node_ptr    : Instance of ProgressMsg
 * ProgressMsg : sample_id       - progress report#
                 sampel_data     - container for avgtime
                 sampel_cum_data - container for cavgtime
                 sample_time     - progress report received time

 * progress_info: Instance of SampleInfo
 * SampleInfo   : sample_count - counter of sample
                  sample_id    - container of progress report#
                  child_mask   - **

 **********************************************************************/
void init_progress_data_pool(int num_entries)
{
  int id, ret;
  ProgressMsg *node_ptr = NULL;
  nslb_mp_handler *mpool = NULL;

  NSDL1_PARENT(NULL, NULL, "Method called. initializing progress report data pool. num_entries = %d, progress_report_queue_size = %d", 
                            num_entries, global_settings->progress_report_queue_size);

  MY_MALLOC_AND_MEMSET(progress_data_pool, (num_entries * sizeof(void*)), "Creating progress_data_pool", -1);

  for(id = 0; id < num_entries; id++)
  {
    //creating mpool to store child's avgtime data
    MY_MALLOC(mpool, sizeof(nslb_mp_handler), "nslb_mp_handler", -1);
    nslb_mp_init(mpool, sizeof(ProgressMsg), global_settings->progress_report_queue_size, 0, NON_MT_ENV);
    if((ret = nslb_mp_create(mpool)) != 0)
    {
      NSTL1(NULL,NULL, "Error: Unable to create memory pool for entry = %d\n", id);
      NS_EXIT(1, "Error: Unable to create memory pool for entry = %d", id);//TODO do nslb_exit
    }

    progress_data_pool[id] = mpool;

    /* Get slot for current sample */
    node_ptr = (ProgressMsg *)nslb_mp_get_slot(mpool);
    node_ptr->sample_data = g_cur_avg[id];
    node_ptr->sample_cum_data = g_cur_finished[id];
    node_ptr->sample_id = 1; //cur_sample
    
    /* Get slot for next sample */
    node_ptr = (ProgressMsg *)nslb_mp_get_slot(mpool);
    node_ptr->sample_data = g_next_avg[id];
    node_ptr->sample_cum_data = g_next_finished[id];
    node_ptr->sample_id = 2;//cur_sample+1
  }

  MY_MALLOC_AND_MEMSET(progress_info, (global_settings->progress_report_queue_size * sizeof(SampleInfo)), "SampleInfo", -1);
  for(id = 0; id < global_settings->progress_report_queue_size; id++)
    progress_info[id].sample_id = -1;
}

void init_ctx_struct(TxDataCum *txcData)
{
  int i;
  /*Bug 79836 - Here, memset and min set was done with total_tx_entries.
    If max_tx_entries and total_tx_entries will vary then junk data
    will be there for (max_tx_entries - total_tx_entries) as these
    memory segment is not memset.*/
  memset ((char *)txcData, 0, max_tx_entries * sizeof(TxDataCum));
  for (i = 0; i < max_tx_entries; i++)
  {
    txcData[i].tx_c_min_time = 0xFFFFFFFF;
  }
}

void init_tx_struct(TxDataSample *txData)
{
  int i;
  /*Bug 79836 - Here, memset and min set was done with total_tx_entries.
    If max_tx_entries and total_tx_entries will vary then junk data
    will be there for (max_tx_entries - total_tx_entries) as these
    memory segment is not memset.*/
  memset ((char *)txData, 0, max_tx_entries * sizeof(TxDataSample));
  for (i = 0; i < max_tx_entries; i++)
  {
    txData[i].tx_min_time = 0xFFFFFFFF;
    txData[i].tx_succ_min_time = 0xFFFFFFFF;
    txData[i].tx_failure_min_time = 0xFFFFFFFF;
    txData[i].tx_netcache_hit_min_time = 0xFFFFFFFF;
    txData[i].tx_netcache_miss_min_time = 0xFFFFFFFF;
    txData[i].tx_min_think_time = 0xFFFFFFFF;
  }
}

void init_avgtime (avgtime *msg, int dont_reset_all)
{
  int i;
  TxDataSample *tx_msg = NULL;
  NSDL2_PARENT(NULL, NULL, "Method called. g_avgtime_size = %d", g_avgtime_size);
   
  bzero (msg, g_avg_size_only_grp);
 
  msg->min_time = MAX_VALUE_4B_U;
  msg->url_overall_min_time = MAX_VALUE_4B_U;
  msg->url_failure_min_time = MAX_VALUE_4B_U;
  msg->url_dns_min_time = MAX_VALUE_4B_U;
  msg->url_conn_min_time = MAX_VALUE_4B_U;
  msg->url_ssl_min_time = MAX_VALUE_4B_U;
  msg->url_frst_byte_rcv_min_time = MAX_VALUE_4B_U;
  msg->url_dwnld_min_time = MAX_VALUE_4B_U;
  msg->smtp_min_time = MAX_VALUE_4B_U;
  msg->pop3_min_time = MAX_VALUE_4B_U;
 
  if(global_settings->protocol_enabled & FTP_PROTOCOL_ENABLED) {
     FTPAvgTime *ftp_msg = NULL;
     ftp_msg = ((FTPAvgTime*)((char*)msg + g_ftp_avgtime_idx));
     ftp_msg->ftp_min_time = 0xFFFFFFFF;
  }
 
  if(global_settings->protocol_enabled & LDAP_PROTOCOL_ENABLED) {
     LDAPAvgTime *ldap_msg = NULL;
     ldap_msg = ((LDAPAvgTime*)((char*)msg + g_ldap_avgtime_idx));
     ldap_msg->ldap_min_time = 0xFFFFFFFF;
  }
 
  if(global_settings->protocol_enabled & IMAP_PROTOCOL_ENABLED) {
     IMAPAvgTime *imap_msg = NULL;
     imap_msg = ((IMAPAvgTime*)((char*)msg + g_imap_avgtime_idx));
     imap_msg->imap_min_time = 0xFFFFFFFF;
  }
 
  if(global_settings->protocol_enabled & JRMI_PROTOCOL_ENABLED) {
     JRMIAvgTime *jrmi_msg = NULL;
     jrmi_msg = ((JRMIAvgTime*)((char*)msg + g_jrmi_avgtime_idx));
     jrmi_msg->jrmi_min_time = 0xFFFFFFFF;
  }
  
  if(global_settings->protocol_enabled & WS_PROTOCOL_ENABLED) {
     WSAvgTime *ws_msg = NULL;
     ws_msg = ((WSAvgTime*)((char*)msg + g_ws_avgtime_idx));
     NSDL2_WS(NULL, NULL, "ws_msg = [%p]", ws_msg);
     ws_msg->ws_min_time = 0xFFFFFFFF;
  }

#if 0
  if(global_settings->protocol_enabled & JMETER_PROTOCOL_ENABLED) {
     jmeter_avgtime *jm_msg = NULL;
     jm_msg = ((jmeter_avgtime*)((char*)msg + g_jmeter_avgtime_idx));
     NSDL2_WS(NULL, NULL, "jm_msg = [%p]", jm_msg);
     jm_msg->jm_min_time = 0xFFFFFFFF;  
  }
#endif

  if(global_settings->protocol_enabled & FC2_PROTOCOL_ENABLED) {
     FC2AvgTime *fc2_msg = NULL;
     fc2_msg = ((FC2AvgTime*)((char*)msg + g_fc2_avgtime_idx));
     NSDL2_FC2(NULL, NULL, "fc2_msg = [%p]", fc2_msg);
     fc2_msg->fc2_overall_min_time = MAX_VALUE_4B_U;
     fc2_msg->fc2_failure_min_time = MAX_VALUE_4B_U;
  }

  if (IS_TCP_CLIENT_API_EXIST)
    ns_socket_client_init_avgtime(msg, g_tcp_client_avg_idx);

  if (IS_UDP_CLIENT_API_EXIST)
    ns_socket_client_init_avgtime(msg, g_udp_client_avg_idx);

  msg->pg_min_time = 0xFFFFFFFF;
  msg->tx_min_time = 0xFFFFFFFF;
  msg->tx_succ_min_resp_time = 0xFFFFFFFF;
  msg->tx_fail_min_resp_time = 0xFFFFFFFF;
  msg->tx_min_think_time = 0xFFFFFFFF;
  msg->sess_min_time = 0xFFFFFFFF;
 
  msg->page_js_proc_time_min = MAX_VALUE_4B_U;
  msg->page_proc_time_min = MAX_VALUE_4B_U;
  msg->pg_succ_min_resp_time = MAX_VALUE_4B_U;
  msg->pg_fail_min_resp_time = MAX_VALUE_4B_U;


  msg->sess_succ_min_resp_time = MAX_VALUE_4B_U;
  msg->sess_fail_min_resp_time = MAX_VALUE_4B_U;

  msg->bind_sock_fail_min = MAX_VALUE_4B_U;
 
  if(global_settings->protocol_enabled & HTTP_CACHE_ENABLED)
  {
    ((CacheAvgTime*)((char*)msg + g_cache_avgtime_idx))->cache_search_url_time_min = MAX_VALUE_4B_U;
    ((CacheAvgTime*)((char*)msg + g_cache_avgtime_idx))->cache_add_url_time_min = MAX_VALUE_4B_U;
  }
 
  if(IS_PROXY_ENABLED)
  {
    ((ProxyAvgTime*)((char*)msg + g_proxy_avgtime_idx))->connect_success_response_time_min = MAX_VALUE_4B_U;
    ((ProxyAvgTime*)((char*)msg + g_proxy_avgtime_idx))->connect_failure_response_time_min = MAX_VALUE_4B_U;
  }
 
  if(IS_NETWORK_CACHE_STATS_ENABLED)
  {
    ((NetworkCacheStatsAvgTime*)((char*)msg + g_network_cache_stats_avgtime_idx))->network_cache_stats_hits_response_time_min = MAX_VALUE_4B_U;
    ((NetworkCacheStatsAvgTime*)((char*)msg + g_network_cache_stats_avgtime_idx))->network_cache_stats_miss_response_time_min = MAX_VALUE_4B_U;
  }
 
  if(IS_DNS_LOOKUP_STATS_ENABLED)
  {
    ((DnsLookupStatsAvgTime*)((char*)msg + dns_lookup_stats_avgtime_idx))->dns_lookup_time_min = MAX_VALUE_4B_U;
  }
 
  if(global_settings->monitor_type == HTTP_API)
  {
    CavTestHTTPAvgTime *cavtest_msg = NULL;
    cavtest_msg = ((CavTestHTTPAvgTime*)((char*)msg + g_cavtest_http_avg_idx));
    NSDL2_GDF(NULL, NULL, "cavtest_msg = [%p]", cavtest_msg);
    cavtest_msg->total_min_time = MAX_VALUE_4B_U;
    cavtest_msg->send_min_time = MAX_VALUE_4B_U;
    cavtest_msg->redirect_min_time = MAX_VALUE_4B_U;
  }
  else if(global_settings->monitor_type == WEB_PAGE_AUDIT)
  {
    CavTestWebAvgTime *cavtest_msg = NULL;
    cavtest_msg = ((CavTestWebAvgTime*)((char*)msg + g_cavtest_web_avg_idx));
    NSDL2_GDF(NULL, NULL, "cavtest_msg = [%p]", cavtest_msg);
    cavtest_msg->send_min_time = MAX_VALUE_4B_U;
    cavtest_msg->redirect_min_time = MAX_VALUE_4B_U;
  }

/*
  if(global_settings->protocol_enabled & XMPP_PROTOCOL_ENABLED) {
     XMPPAvgTime *xmpp_msg = NULL;
     xmpp_msg = ((XMPPAvgTime*)((char*)msg + g_xmpp_avgtime_idx));
  }   
*/
 
  if(!dont_reset_all)
  {

    HTTPRespCodeAvgTime *http_resp_code_msg = (HTTPRespCodeAvgTime*)(((char*)msg + http_resp_code_avgtime_idx)) ;
    memset ((char *)http_resp_code_msg, 0, max_http_resp_code_entries * sizeof(HTTPRespCodeAvgTime));

    tx_msg = (TxDataSample*)(((char*)msg + g_trans_avgtime_idx));
    init_tx_struct(tx_msg);
    init_um_data((UM_data *)((char *)msg + g_avg_um_data_idx));

    if(SHOW_GRP_DATA)
    {
      GROUPAvgTime *grp_data_msg = NULL;
      grp_data_msg = ((GROUPAvgTime*)((char*)msg + group_data_gp_idx));
      memset(grp_data_msg, 0, GROUP_AVGTIME_SIZE); 
      for (i = 0; i < total_runprof_entries; i++)
      {
        grp_data_msg[i].sess_min_time = MAX_VALUE_4B_U;
        grp_data_msg[i].ka_min_time = MAX_VALUE_4B_U;
        grp_data_msg[i].page_think_min_time = MAX_VALUE_4B_U;
      }
    }

    if(SHOW_RUNTIME_RUNLOGIC_PROGRESS) {
      VUserFlowAvgTime *runtime_runlogic_prog_msg = NULL;
      runtime_runlogic_prog_msg = ((VUserFlowAvgTime*)((char*)msg + show_vuser_flow_idx)); 
      memset(runtime_runlogic_prog_msg, 0, RUNTIME_RUNLOGIC_PROGRESS_AVGTIME_SIZE);
    }
 
    if(SHOW_SERVER_IP)
    {
      SrvIPAvgTime *srv_ip_msg = NULL;
      srv_ip_msg = (SrvIPAvgTime*)((char*)msg + srv_ip_data_idx);
      memset(srv_ip_msg, 0, ((max_srv_ip_entries) * sizeof(SrvIPAvgTime)));
    }

    //Bug 43692-HTTP Requests Send/Sec graph under Source IP Stats should be sample data, in current design we are receiving cumulative data.
    if((global_settings->show_ip_data == IP_BASED_DATA_ENABLED) && (total_ip_entries))
    {
      NSDL2_MISC(NULL, NULL, "SHOW IP DATA is enabled.");
      IPBasedAvgTime *ip_msg = (IPBasedAvgTime*)((char*)msg + ip_data_gp_idx);
      memset(ip_msg, 0, ((total_group_ip_entries) * sizeof(IPBasedAvgTime))); 
    }

    //For Page Stat : Inintialising min values of page stat
    initialise_page_based_stat_min(msg);
  
    // For RBU : set the value for DOMContent, PageLoad, OnLoad, Start Render, TTI, VisuallyComplete (min,max)
    //if(global_settings->browser_used != -1 )
    if(global_settings->protocol_enabled & RBU_API_USED)
    {
      RBUPageStatAvgTime *rbu_page_msg = NULL;
      rbu_page_msg = ((RBUPageStatAvgTime*)((char*)msg + rbu_page_stat_data_gp_idx));
      NSDL2_PARENT(NULL, NULL, "We are initialising min and max value for RBU stats total_page_entries = %d, "
                               "rbu_page_msg = %p, rbu_page_stat_data_gp_id = %d", 
                                            total_page_entries, rbu_page_msg, rbu_page_stat_data_gp_idx);
      memset(rbu_page_msg, 0, RBU_PAGE_STAT_SIZE);
      for (i = 0; i < g_actual_num_pages; i++)
      {
        rbu_page_msg[i].DOMContent_Loaded_min_time = MAX_VALUE_4B_U;
        rbu_page_msg[i].OnLoad_min_time = MAX_VALUE_4B_U;
        rbu_page_msg[i].PageLoad_min_time = MAX_VALUE_4B_U;
        rbu_page_msg[i].TTI_min_time = MAX_VALUE_4B_U;
        rbu_page_msg[i]._cav_startRender_min_time = MAX_VALUE_4B_U;
        rbu_page_msg[i].visually_complete_min_time = MAX_VALUE_4B_U;
        rbu_page_msg[i].cur_rbu_requests_min = MAX_VALUE_4B_U;
        rbu_page_msg[i].cur_rbu_browser_cache_min = MAX_VALUE_4B_U;
        rbu_page_msg[i].cur_rbu_bytes_recieved_min = MAX_VALUE_4B_U;
        rbu_page_msg[i].cur_rbu_bytes_send_min = MAX_VALUE_4B_U;
        rbu_page_msg[i].cur_rbu_page_wgt_min = MAX_VALUE_4B_U;
        rbu_page_msg[i].cur_rbu_js_size_min = MAX_VALUE_4B_U;
        rbu_page_msg[i].cur_rbu_css_size_min = MAX_VALUE_4B_U;
        rbu_page_msg[i].cur_rbu_img_wgt_min = MAX_VALUE_4B_U;
        rbu_page_msg[i].cur_rbu_dom_element_min = MAX_VALUE_4B_U;
        rbu_page_msg[i].cur_rbu_pg_speed_min = MAX_VALUE_4B_U;
        rbu_page_msg[i].cur_rbu_akamai_cache_min = MAX_VALUE_4B_U;
        rbu_page_msg[i].cur_rbu_main_url_resp_time_min = MAX_VALUE_4B_U;
        rbu_page_msg[i].pg_avail = 1; //Default page availability shoud be 1  
        rbu_page_msg[i].dns_min_time= MAX_VALUE_4B_U;
        rbu_page_msg[i].tcp_min_time = MAX_VALUE_4B_U;
        rbu_page_msg[i].ssl_min_time = MAX_VALUE_4B_U;
        rbu_page_msg[i].connect_min_time = MAX_VALUE_4B_U;
        rbu_page_msg[i].wait_min_time = MAX_VALUE_4B_U;
        rbu_page_msg[i].rcv_min_time = MAX_VALUE_4B_U;
        rbu_page_msg[i].blckd_min_time = MAX_VALUE_4B_U;
        rbu_page_msg[i].url_resp_min_time = MAX_VALUE_4B_U;
        rbu_page_msg[i].tbt_min = MAX_VALUE_4B_U;
        rbu_page_msg[i].lcp_min = MAX_VALUE_4B_U;
        rbu_page_msg[i].cls_min = MAX_VALUE_4B_U;

      }
    }

    //For Domain Stat : 
    if(global_settings->rbu_domain_stats_mode)
      initialise_rbu_domain_stat_min(msg);
  }
}

void init_cavgtime (cavgtime *msg, int dont_reset_all)
{
  TxDataCum *tx_cmsg = NULL;
  NSDL2_PARENT(NULL, NULL, "Method called");
  bzero (msg, g_cavg_size_only_grp);

  msg->c_min_time = 0xFFFFFFFF;
  msg->smtp_c_min_time = 0xFFFFFFFF;
  msg->pop3_c_min_time = 0xFFFFFFFF;

  if(global_settings->protocol_enabled & FTP_PROTOCOL_ENABLED) {
     FTPCAvgTime *ftp_msg = NULL;
     ftp_msg = ((FTPCAvgTime*)((char*)msg + g_ftp_cavgtime_idx));
     ftp_msg->ftp_c_min_time = 0xFFFFFFFF;
  } 
  if(global_settings->protocol_enabled & LDAP_PROTOCOL_ENABLED) {
     LDAPCAvgTime *ldap_msg = NULL;
     ldap_msg = ((LDAPCAvgTime*)((char*)msg + g_ldap_cavgtime_idx));
     ldap_msg->ldap_c_min_time = 0xFFFFFFFF;
  }

  if(global_settings->protocol_enabled & IMAP_PROTOCOL_ENABLED) {
     IMAPCAvgTime *imap_msg = NULL;
     imap_msg = ((IMAPCAvgTime*)((char*)msg + g_imap_cavgtime_idx));
     imap_msg->imap_c_min_time = 0xFFFFFFFF;
  } 

  if(global_settings->protocol_enabled & JRMI_PROTOCOL_ENABLED) {
     JRMICAvgTime *jrmi_msg = NULL;
     jrmi_msg = ((JRMICAvgTime*)((char*)msg + g_jrmi_cavgtime_idx));
     jrmi_msg->jrmi_c_min_time = 0xFFFFFFFF;
  } 

  if(global_settings->protocol_enabled & WS_PROTOCOL_ENABLED) {
     WSCAvgTime *ws_msg = NULL;
     ws_msg = ((WSCAvgTime*)((char*)msg + g_ws_cavgtime_idx));
     ws_msg->ws_c_min_time = 0xFFFFFFFF;
  }

  /*
  if(global_settings->protocol_enabled & XMPP_PROTOCOL_ENABLED) {
     XMPPCAvgTime *xmpp_msg = NULL;
     xmpp_msg = ((XMPPCAvgTime*)((char*)msg + g_xmpp_cavgtime_idx));
  } 
  */
  msg->pg_c_min_time = 0xFFFFFFFF;
  msg->tx_c_min_time = 0xFFFFFFFF;
  msg->sess_c_min_time = 0xFFFFFFFF;

  if(!dont_reset_all)
  {
    tx_cmsg = (TxDataCum*)(((char*)msg + g_trans_cavgtime_idx));
    init_ctx_struct(tx_cmsg);
    init_um_data((UM_data *)((char *)msg + g_cavg_um_data_idx));
  }
}


/***********************************************************************************************
 |  • NAME:   	
 | 	malloc_avgtime() - allocating memory for all avgtime type variables and initialize them 
 |
 |  • SYNOPSIS: 
 | 	inline void malloc_avgtime()	
 |
 |  • DESCRIPTION:   	
 |	This function will do following task -
 |	1. Allocate memory for all global variables of type avgtime -
 |		g_cur_avg -> Store current sample of all the 
 |
 |  • RETURN VALUE:
 |	nothing
 ************************************************************************************************/
void malloc_avgtime()
{
  int i, j;

  NSDL2_MESSAGES(NULL, NULL, "Method called");
  MY_MALLOC_AND_MEMSET(g_cur_avg, (sgrp_used_genrator_entries + 1) * sizeof(avgtime *), "creating table for cur_avg", -1);
  MY_MALLOC_AND_MEMSET(g_next_avg, (sgrp_used_genrator_entries + 1) * sizeof(avgtime *), "creating table for next_avg", -1);

  //For finish report
  MY_MALLOC_AND_MEMSET(g_end_avg, (sgrp_used_genrator_entries + 1) * sizeof(avgtime *), "creating table for end_avg", -1);
  
  MY_MALLOC_AND_MEMSET(g_dest_avg, (sgrp_used_genrator_entries + 1) * sizeof(avgtime *), "creating table for next_avg", -1);
  MY_MALLOC(g_dest_cavg, (sgrp_used_genrator_entries + 1) * sizeof(cavgtime *), "g_next_finished", -1);

  //For cum 
  MY_MALLOC(g_cur_finished, (sgrp_used_genrator_entries + 1) * sizeof(cavgtime *), "g_cur_finished", -1);
  MY_MALLOC(g_next_finished, (sgrp_used_genrator_entries + 1) * sizeof(cavgtime *), "g_next_finished", -1);

  for(i = 0; i < (sgrp_used_genrator_entries + 1); i++)
  {
    NSTL1(NULL, NULL, "Allocating avg's & cagv's in Parent for grp_idx = %d",i);
    MY_MALLOC(g_cur_avg[i], g_avgtime_size, "cur_avg Allocated", i);
    MY_MALLOC(g_end_avg[i], g_avgtime_size, "end_avg Allocated", i);
    MY_MALLOC(g_next_avg[i], g_avgtime_size, "next_avg Allocated", i);

    MY_MALLOC(g_cur_finished[i], g_cavgtime_size, "cur_finished Allocated", i);
    MY_MALLOC(g_next_finished[i], g_cavgtime_size, "next_finished Allocated", i);
  
    //initialize all avgtime 
    for(j = 0; j < TOTAL_GRP_ENTERIES_WITH_GRP_KW; j++)
    {
      GET_AVG_STRUCT(g_cur_avg[i], j);
      init_avgtime (tmp_reset_avg, j);
      tmp_reset_avg->opcode = PROGRESS_REPORT;
      tmp_reset_avg->elapsed = cur_sample;
      
      GET_AVG_STRUCT(g_next_avg[i], j);
      init_avgtime (tmp_reset_avg, j);
      tmp_reset_avg->opcode = PROGRESS_REPORT;
      tmp_reset_avg->elapsed = cur_sample + 1;
      
      GET_AVG_STRUCT(g_end_avg[i], j);
      init_avgtime (tmp_reset_avg, j);
      
      GET_CAVG_STRUCT(g_cur_finished[i], j);
      init_cavgtime (tmp_reset_cavg, j);
      
      GET_CAVG_STRUCT(g_next_finished[i], j);
      init_cavgtime (tmp_reset_cavg, j);
    }
  } 
  /* In release 3.9.3,
   * NetCloud: Here we malloc gsavedTxData double pointer, TxData pointer for each controller + number of generators.
   * Now in case of standalone, sgrp_used_genrator_entries will be 0 
   * Next we allocate memory for TxData structure pointer for total number of transaction  
   * */ 

  if(!gsavedTxData) {
    NSDL2_MESSAGES(NULL, NULL, "Malloc gsavedTxData, sgrp_used_genrator_entries = %d", sgrp_used_genrator_entries);
    MY_MALLOC_AND_MEMSET(gsavedTxData, (sgrp_used_genrator_entries + 1) * sizeof(TxDataCum *), "gsavedTxData", -1);
  } 

  if(total_tx_entries)
  {
    NSDL2_MESSAGES(NULL, NULL, "Malloc gsavedTxData, sgrp_used_genrator_entries = %d", sgrp_used_genrator_entries);
    //For generators we allocate TxData structure pointer from index 1 to n, here index 0 is for controller or standalone
    for(i = 0; i < (sgrp_used_genrator_entries + 1); i++)
      MY_MALLOC(gsavedTxData[i], total_tx_entries * sizeof(TxDataCum), "gsavedTxData", -1);
  }

  /*Allocate progress report pool*/
  init_progress_data_pool(sgrp_used_genrator_entries + 1);
}

/*************************************************************************
 * Purpose     : Get a free slot from Sample Info table to update counter
   Description : Find nearest free sample slot from array progress_info
                 best case  : first 
                 worst case : last
   NVM 0 sends sample 2 and sample 1 is not completed and then sample 3 came
   then progress_info will not update sample 3 counter (default)
   
   Return      : return slot
   
 ************************************************************************/
static inline int get_free_sample_count()
{
  int count;

  NSDL1_MESSAGES(NULL, NULL, "Method Called");
  /*****************************************************
   Get free slot from sample_info table 
   to update sample information
  *****************************************************/
  for(count = 0; count < global_settings->progress_report_queue_size; count++)
    if(progress_info[count].sample_id == -1)
      return count;

  return -1; //should not come here when sample is not left in sample_info
}

//reset contents of that sample whose all progress report received
void reset_sample_info(int recv_sample)
{
  int count;
  NSDL1_MESSAGES(NULL, NULL, "Method Called, reset received sample = %d", recv_sample);

  //Find Slot
  for(count = 0; count < global_settings->progress_report_queue_size; count++)
  {
    if(progress_info[count].sample_id == recv_sample){
      progress_info[count].sample_id = -1;
      progress_info[count].sample_count = 0;
      memset(&progress_info[count].child_mask, 0, 4 * sizeof(unsigned long)); //clearing 256 bit masking
      break;
    }
  }
}

int get_sample_info(int recv_sample)
{
  int i;
  //Find Slot
  for(i = 0; i < global_settings->progress_report_queue_size; i++)
    if(progress_info[i].sample_id == recv_sample)
      return progress_info[i].sample_count;
  
  return -1; //num_pge != not found sample
}
/******************************************************************
 * Description : This method will update received samples count
 * Input       : received report number
 * Output      : count of the received report number
                 
 ******************************************************************/
static int update_sample_info(int recv_sample, int child_idx)
{
  int i, sample_slot_id = -1, free_slot_id = -1;
  int mask_idx;
  unsigned long bit_idx;

  NSDL1_MESSAGES(NULL, NULL, "Method Called. received sample = %d, child_idx = %d", recv_sample, child_idx);

  //Find Slot
  for(i = 0; i < global_settings->progress_report_queue_size; i++)
  {
    if(progress_info[i].sample_id == recv_sample) {
      sample_slot_id = i; 
      break;
    }
    if(progress_info[i].sample_id == -1 && free_slot_id == -1)
    {
      free_slot_id = i;
    }
  }
  //if sample_slot_id is -1 && free_slot_id is -1 then return with proper error message as it shuold not happen
  //else set sample_slot_id to free_slot_id and set progress_info[sample_slot_id].sample_id to recv_sample
  if(sample_slot_id == -1)
  {
    if(free_slot_id == -1)
    {
       NSTL1(NULL, NULL, "No free slot available in progress_info for received sample = %d", recv_sample);
       return -1; 
    }
    sample_slot_id = free_slot_id;
    progress_info[sample_slot_id].sample_id = recv_sample;
  }
    
  progress_info[sample_slot_id].sample_count++;

  //seting bitmask of child used in finding failed generator
  mask_idx = child_idx/(sizeof(unsigned long)*8);
  bit_idx = 1 << (child_idx%(sizeof(unsigned long)*8));

  progress_info[sample_slot_id].child_mask[mask_idx] |= bit_idx;

  NSDL3_MESSAGES(NULL, NULL, "recv_sample=%d, count=%d", recv_sample, progress_info[sample_slot_id].sample_count);

  return progress_info[sample_slot_id].sample_count;
}

void child_delay_alert_msg(int rcv_sample)
{
  char alert_msg[ALERT_MSG_SIZE];
  int i;
  int child_idx;

  NSDL1_MESSAGES(NULL, NULL, "Method called, recv sample = %d", rcv_sample);
  for(i = 0; i < global_settings->progress_report_queue_size; i++)
    if(progress_info[i].sample_id == cur_sample)
      break;

  if(i == global_settings->progress_report_queue_size)
    return;

  for(i = 0; i < global_settings->num_process; i++)
  {
    child_idx = g_dh_msg_com_con[i].nvm_index;
    
    NSDL1_MESSAGES(NULL, NULL, "Method called, last recv sample from child [%d] = %d, progress_report_queue_size = %d, pr child mask = [%s]", 
                               child_idx, g_last_pr_sample[child_idx], global_settings->progress_report_queue_size, 
                               nslb_show_bitflag(progress_info[child_idx].child_mask));

    /**************************************************************
     Check for sending progress delay alert message for nvm/gen 
     1) Checking NVM/GEN is active or kill, If killed then continue
     2) Checking queue is not 50% complete then continue else
     3) Checking child mask is set or not set 
    ***************************************************************/
    if((g_dh_msg_com_con[i].flags & NS_MSG_COM_CON_IS_CLOSED) || ((rcv_sample - g_last_pr_sample[child_idx]) <= global_settings->progress_report_queue_size /2) || nslb_check_bit_set(progress_info[child_idx].child_mask, child_idx))
      continue;
    snprintf(alert_msg, ALERT_MSG_SIZE, "No metric report  received from Cavisson Virtual Machine (CVM%d) for last <%d> samples",
                                        (child_idx + 1), (rcv_sample - g_last_pr_sample[child_idx]));
    ns_send_alert(ALERT_MAJOR, alert_msg);
  }
}
/************************************************************************
 * Purpose     : Find slot of received sample if exists otherwise create
 * Description : Processing every sample untill received from all children
                 if sample not found then create a slot for that sample

 * Input       : index of progress_pool and received report number
   output      : if received sample found then output pmsg
                 else get a slot from pool, update received report number
                 and reset avgtime and cavgtime and output pmsg
 * Return value: pmsg : valid instance of ProgressMsg
                 NULL : instance could not be found

 ************************************************************************/
ProgressMsg *get_avg_from_progress_data_pool(int pool_id, int rcv_sample)
{
   int i,reset_avg = 0;
   avgtime *dest_avg = NULL;
   cavgtime *dest_cavg = NULL;
   nslb_mp_handler *mpool = NULL;

   NSDL1_MESSAGES(NULL, NULL, "Method called, child = %d, sample = %d", pool_id, rcv_sample);

   /* Index pool array for respective child */
   mpool = (nslb_mp_handler*)progress_data_pool[pool_id];
   if(!mpool)
   {
     /* This cannot be possible as we have initialized pool for every child */
     NSTL1_OUT(NULL, NULL, "Memory Pool cannot be null for %d\n", pool_id);
     return NULL;
   }

   /* Searching from head of the pool */
   ProgressMsg *pmsg = (ProgressMsg *) nslb_mp_busy_head(mpool);
   while(pmsg != NULL)
   {
     /*Sample found*/
     if(pmsg->sample_id == rcv_sample)
       break;
     /* Move to next slot*/
     pmsg = (ProgressMsg *)nslb_next(pmsg);
   }

   /* When Sample not found in any slot*/
   if(!pmsg)
   {
     /* If pool is full */
     if(mpool && !mpool->free_head)
     {
       NSTL1(NULL, NULL, "Memory pool exhausted, No slot is available to allocate for sample = %d, of child = %d", rcv_sample, pool_id);
       return NULL;
     }
     NSDL4_MESSAGES(NULL, NULL, "Sample not found in pool, allocating slot for sample = %d, of child = %d", rcv_sample, pool_id);
     /* Get new slot to save sample when pool is not full */
     pmsg = (ProgressMsg *)nslb_mp_get_slot(mpool);
     pmsg->sample_id = rcv_sample;
     reset_avg = 1;

     //Check Only if not equal to client loader
     if (loader_opcode != CLIENT_LOADER && !pool_id) {
       child_delay_alert_msg(rcv_sample);
     }
   }
   //TODO purpose of sample_time 
   /* updating sample_time for every sample*/
   pmsg->sample_time = get_ms_stamp();
   /* When first time sample came from child and allocate avg */
   if(pmsg->sample_data == NULL)
   { 
     NSDL4_MESSAGES(NULL, NULL, "Allocating avg ptr for rcv_sample = %d in pool id= %d", rcv_sample, pool_id);
     NSTL1(NULL, NULL, "Allocating avg ptr for rcv_sample = %d in pool id= %d", rcv_sample, pool_id);
     MY_MALLOC(pmsg->sample_data, g_avgtime_size, "Allocating memory for progress sample", rcv_sample);
     MY_MALLOC(pmsg->sample_cum_data, g_cavgtime_size, "Allocating memory for progress sample", rcv_sample);
     reset_avg = 1;
   }
   /* Get avg from container and reset */
   dest_avg = (avgtime*)pmsg->sample_data; 
   dest_cavg = (cavgtime *)pmsg->sample_cum_data;
   if(reset_avg)
   {
     for(i = 0; i < TOTAL_GRP_ENTERIES_WITH_GRP_KW; i++)
     {
       GET_AVG_STRUCT(dest_avg, i); 
       init_avgtime (tmp_reset_avg, i);
       tmp_reset_avg->opcode = PROGRESS_REPORT;
       tmp_reset_avg->elapsed = rcv_sample;
 
       GET_CAVG_STRUCT(dest_cavg, i);
       init_cavgtime (tmp_reset_cavg, i);
     }
   }
   return pmsg;
}

static void save_finished(cavgtime **g_msg, cavgtime **g_total_avg)
{
  int i, j, k , gen_idx;
  cavgtime *c_msg = NULL, *total_avg = NULL;
  cavgtime *tmp_c_msg = NULL, *tmp_total_avg = NULL;
  FTPCAvgTime *ftp_msg = NULL;
  FTPCAvgTime *total_ftpavg = NULL;
  LDAPCAvgTime *ldap_msg = NULL;
  LDAPCAvgTime *total_ldapavg = NULL;
  IMAPCAvgTime *imap_msg = NULL;
  IMAPCAvgTime *total_imapavg = NULL;
  JRMICAvgTime *jrmi_msg = NULL;
  JRMICAvgTime *total_jrmiavg = NULL;
  UM_data *um_msg = NULL;
  UM_data *total_umavg = NULL;
  WSCAvgTime *ws_msg = NULL;
  WSCAvgTime *total_wsavg = NULL;
  TxDataCum* tx_msg = NULL;
  TxDataCum* tx_save = NULL;

  XMPPCAvgTime *xmpp_msg = NULL;
  XMPPCAvgTime *total_xmppavg = NULL;

  NSDL2_MESSAGES(NULL, NULL, "Method called, sgrp_used_genrator_entries = %d", sgrp_used_genrator_entries);
  
  for(gen_idx = 0; gen_idx < (sgrp_used_genrator_entries + 1); gen_idx++)
  {
    tmp_c_msg = g_msg[gen_idx];
    tmp_total_avg = g_total_avg[gen_idx];

    NSDL3_MESSAGES(NULL, NULL, "total_tx_entries= %d", total_tx_entries);
    if (total_tx_entries) { //Transaction data is only once so checking for first time only
      NSDL2_MESSAGES(NULL, NULL, "Copy transaction data");
      tx_msg = (TxDataCum*)((char*)tmp_c_msg + g_trans_cavgtime_idx);
      tx_save = (TxDataCum*)((char*)tmp_total_avg + g_trans_cavgtime_idx);
      for(k = 0; k < total_tx_entries; k++)
      {
        SET_MIN_MAX_CUM_TX_CUMULATIVE(&tx_save[k], &tx_msg[k]);//Copy PerTransaction Min Max Tx Cumulative
        ACC_CUM_TX_DATA (&tx_save[k], &tx_msg[k]); //Copy PerTransaction Cumulative
      }
    }

    if(loader_opcode == STAND_ALONE)
    {
      ACC_NETSTORM_DIAGNOSTICS_CUMULATIVES_FROM_NEXT_SAMPLE (tmp_total_avg, tmp_c_msg); 
    }
    else
    {
      ACC_NETCLOUD_DIAGNOSTICS_CUMULATIVES_FROM_NEXT_SAMPLE (tmp_total_avg, tmp_c_msg); 
    }

    for (j = 0; j < TOTAL_GRP_ENTERIES_WITH_GRP_KW; j++) 
    {
      // This will add next cumulative counters from next_finished to cur_finished
      // This is to handle case if next progress report comes before current is progress report
      // is recieved from all NVM.
      // Since next_finished will have sum of one sample of only, we need to add these from next to cur
      // In normal case, next will have all zero.

      total_avg = (cavgtime*)((char*)tmp_total_avg + (j * g_cavg_size_only_grp)); 
      c_msg = (cavgtime*)((char*)tmp_c_msg + (j * g_cavg_size_only_grp)); 
     
      SET_MIN_MAX_CUMULATIVES_PARENT (total_avg, c_msg);
  
      ACC_CUMULATIVES_PARENT (total_avg, c_msg);
      /*Bug 49270: Copy Transaction Data*/
      SET_MIN_MAX_CUM_TX_CUMULATIVE(total_avg,c_msg);//Copy OverAll Min Max Tx Cumulative
      ACC_CUM_TX_DATA(total_avg, c_msg); //Copy OverAll Cumulative

      //For User monitor
      NSDL3_MESSAGES(NULL, NULL, "total_um_entries= %d", total_um_entries);
      if(total_um_entries) {
        um_msg = (UM_data*)((char*)c_msg + g_cavg_um_data_idx);
        total_umavg = (UM_data*)((char*)total_avg + g_cavg_um_data_idx);
        NSDL3_MESSAGES(NULL, NULL, "save_finished: total_um_entries = %d, um_msg = %p, total_umavg = %p", total_um_entries, um_msg, total_umavg); 
        calculate_um_data_in_cavg(total_umavg, um_msg);
      }
      /*ftp */
      if(global_settings->protocol_enabled & FTP_PROTOCOL_ENABLED) {
        ftp_msg = ((FTPCAvgTime*)((char*)c_msg + g_ftp_cavgtime_idx));
        total_ftpavg = ((FTPCAvgTime*)((char*)total_avg + g_ftp_cavgtime_idx));
        SET_MIN_MAX_FTP_CUMULATIVES_PARENT (total_ftpavg, ftp_msg);
        ACC_FTP_CUMULATIVES_PARENT(total_ftpavg,ftp_msg);
      }
  
      //LDAP
      if(global_settings->protocol_enabled & LDAP_PROTOCOL_ENABLED) {
        ldap_msg = ((LDAPCAvgTime*)((char*)c_msg + g_ldap_cavgtime_idx));
        total_ldapavg = ((LDAPCAvgTime*)((char*)total_avg + g_ldap_cavgtime_idx));
        SET_MIN_MAX_LDAP_CUMULATIVES_PARENT (total_ldapavg, ldap_msg);
        ACC_LDAP_CUMULATIVES_PARENT(total_ldapavg, ldap_msg);
      }
      
      //IMAP
      if(global_settings->protocol_enabled & IMAP_PROTOCOL_ENABLED) {
        imap_msg = ((IMAPCAvgTime*)((char*)c_msg + g_imap_cavgtime_idx));
        total_imapavg = ((IMAPCAvgTime*)((char*)total_avg + g_imap_cavgtime_idx));
        SET_MIN_MAX_IMAP_CUMULATIVES_PARENT (imap_msg, total_imapavg);
        ACC_IMAP_CUMULATIVES_PARENT(total_imapavg, imap_msg);
      }

      //JRMI
      if(global_settings->protocol_enabled & JRMI_PROTOCOL_ENABLED) {
        jrmi_msg = ((JRMICAvgTime*)((char*)c_msg + g_jrmi_cavgtime_idx));
        total_jrmiavg = ((JRMICAvgTime*)((char*)total_avg + g_jrmi_cavgtime_idx));
        SET_MIN_MAX_JRMI_CUMULATIVES_PARENT (jrmi_msg, total_jrmiavg);
        ACC_JRMI_CUMULATIVES_PARENT(total_jrmiavg, jrmi_msg);
      }

      //Websocket
      if(global_settings->protocol_enabled & WS_PROTOCOL_ENABLED) {
        ws_msg = ((WSCAvgTime*)((char*)c_msg + g_ws_cavgtime_idx));
        total_wsavg = ((WSCAvgTime*)((char*)total_avg + g_ws_cavgtime_idx));
        SET_MIN_MAX_WS_CUMULATIVES_PARENT (ws_msg, total_wsavg);
        ACC_WS_CUMULATIVES_PARENT(total_wsavg, ws_msg);
      }

      //XMPP
      if(global_settings->protocol_enabled & XMPP_PROTOCOL_ENABLED) {
        xmpp_msg = ((XMPPCAvgTime*)((char*)c_msg + g_xmpp_cavgtime_idx));
        total_xmppavg = ((XMPPCAvgTime*)((char*)total_avg + g_xmpp_cavgtime_idx));
        ACC_XMPP_CUMULATIVES_PARENT(total_xmppavg, xmpp_msg);
        NSDL3_MESSAGES(NULL, NULL, "XMPP CAvg pointer");
      }
   
      PARENT_COPY_SOCKET_TCP_UDP_CLIENT_CUMULATIVE_DATA_NEXT2CUR(total_avg, c_msg);

      init_cavgtime(c_msg, j);
    }
  }
}

/**************************************************************
copy unused transaction data from current avg to next avg as these
transactions index are not allocated in gdf yet 
***************************************************************/ 
void copy_from_cur_to_next(avgtime *cur_avg, avgtime *next_avg)
{
  TxDataSample *txPtr = NULL, *txPtr2 = NULL;
  Rbu_domain_stat_avgtime *rbu_domain_cur_avg_ptr = NULL, *rbu_domain_next_avg_ptr = NULL;
  int size, i, dyn_obj_idx;
  int total_dyn_entries_in_gdf = 0;

  NSDL1_MESSAGES(NULL, NULL, "Method Called");

  for(dyn_obj_idx = 1; dyn_obj_idx < MAX_DYN_OBJS; dyn_obj_idx++)
  {
    total_dyn_entries_in_gdf = dynObjForGdf[dyn_obj_idx].startId;
    NSDL2_MESSAGES(NULL, NULL, "dynObjIdx = %d, total_dyn_entries_in_gdf = %d", i, total_dyn_entries_in_gdf); 
    switch(dyn_obj_idx)
    {
      case NEW_OBJECT_DISCOVERY_TX:
        if(total_tx_entries != total_dyn_entries_in_gdf)
        {
          //Size of data that will be moved form current to next avg
          size = (total_tx_entries - total_dyn_entries_in_gdf) * sizeof(TxDataSample);
      
          /*Currnet Avg Data*/
          txPtr = (TxDataSample*)((((char*)cur_avg) + g_trans_avgtime_idx));
      
          /*Next Avg Data*/
          txPtr2 = (TxDataSample*)((((char*)next_avg) + g_trans_avgtime_idx));
      
          /*Copy transaction data from current Avg to next Avg*/
          COPY_PERIODICS_TX_STATS(total_dyn_entries_in_gdf,total_tx_entries, txPtr2, txPtr);
      
          /*BUG: 32514: Reset the memory that that has been moved from current to next avg, so that progress report gets synced with RTG.*/
          memset(&txPtr[total_dyn_entries_in_gdf],0,size);
          txPtr = (TxDataSample*)((((char*)cur_avg) + g_trans_avgtime_idx));
          for (i = total_dyn_entries_in_gdf; i < total_tx_entries; i++)
          {
            txPtr[i].tx_min_time = 0xFFFFFFFF;
            txPtr[i].tx_succ_min_time = 0xFFFFFFFF;
            txPtr[i].tx_failure_min_time = 0xFFFFFFFF;
            txPtr[i].tx_netcache_hit_min_time = 0xFFFFFFFF;
            txPtr[i].tx_netcache_miss_min_time = 0xFFFFFFFF;
            txPtr[i].tx_min_think_time = 0xFFFFFFFF;
          }
        }
        break; 

      case NEW_OBJECT_DISCOVERY_RBU_DOMAIN:
        NSDL2_MESSAGES(NULL, NULL, "total_rbu_domain_entries = %d, total_dyn_entries_in_gdf = %d", 
                         total_rbu_domain_entries, total_dyn_entries_in_gdf);
        if(total_rbu_domain_entries != total_dyn_entries_in_gdf)
        {
          //Size of data that will be moved form current to next avg
          size = (total_rbu_domain_entries - total_dyn_entries_in_gdf) * sizeof(Rbu_domain_stat_avgtime);
      
          /*Currnet Avg Data*/
          rbu_domain_cur_avg_ptr = (Rbu_domain_stat_avgtime*)((((char*)cur_avg) + rbu_domain_stat_avg_idx));
      
          /*Next Avg Data*/
          rbu_domain_next_avg_ptr = (Rbu_domain_stat_avgtime*)((((char*)next_avg) + rbu_domain_stat_avg_idx));
      
          /*Copy transaction data from current Avg to next Avg*/
          COPY_PERIODICS_RBU_DOMAIN_STATS_FROM_CUR_TO_NEXT_AVG(total_dyn_entries_in_gdf, 
                total_rbu_domain_entries, rbu_domain_next_avg_ptr, rbu_domain_cur_avg_ptr);

          memset(&rbu_domain_cur_avg_ptr[total_dyn_entries_in_gdf], 0, size);
          RESET_RBU_DOMAIN_STAT_AVG(rbu_domain_cur_avg_ptr, total_dyn_entries_in_gdf, total_rbu_domain_entries);
        }
        break;

      case NEW_OBJECT_DISCOVERY_SVR_IP:
        if(SHOW_SERVER_IP){

          SrvIPAvgTime *srvPtr1 = NULL, *srvPtr2 = NULL;
          int total_normalized_svr_ips_in_gdf = dynObjForGdf[NEW_OBJECT_DISCOVERY_SVR_IP].startId;
          if(total_normalized_svr_ips != total_normalized_svr_ips_in_gdf)
          {
            size = (total_normalized_svr_ips - total_normalized_svr_ips_in_gdf) * sizeof(SrvIPAvgTime);

            srvPtr1 = (SrvIPAvgTime*)((char*)cur_avg + srv_ip_data_idx);

            srvPtr2 = (SrvIPAvgTime*)((char*)next_avg + srv_ip_data_idx);

            COPY_PERIODICS_SERVER_IP_STATS(total_normalized_svr_ips_in_gdf, total_normalized_svr_ips, srvPtr2, srvPtr1);

            memset(&srvPtr1[total_normalized_svr_ips_in_gdf], 0, size);
          }
        }
        break;

      case NEW_OBJECT_DISCOVERY_STATUS_CODE:
        NSDL2_MESSAGES(NULL, NULL, "total_http_resp_code_entries = %d, total_dyn_entries_in_gdf = %d", 
                                   total_http_resp_code_entries, total_dyn_entries_in_gdf); 
        HTTPRespCodeAvgTime *status_code_ptr1 = NULL, *status_code_ptr2 = NULL;
        if(total_http_resp_code_entries != total_dyn_entries_in_gdf)
        {
          size = (total_http_resp_code_entries - total_dyn_entries_in_gdf) * sizeof(HTTPRespCodeAvgTime);

          status_code_ptr1 = (HTTPRespCodeAvgTime*)((char*)cur_avg + http_resp_code_avgtime_idx);

          status_code_ptr2 = (HTTPRespCodeAvgTime*)((char*)next_avg + http_resp_code_avgtime_idx);

          COPY_PERIODICS_STATUS_CODE(total_dyn_entries_in_gdf, total_http_resp_code_entries, status_code_ptr2, status_code_ptr1);

          memset(&status_code_ptr1[total_dyn_entries_in_gdf], 0, size);

        }
        break;
      case NEW_OBJECT_DISCOVERY_TCP_CLIENT_FAILURES:
      {
        NSDL2_MESSAGES(NULL, NULL, "g_total_tcp_client_errs = %d, total_dyn_entries_in_gdf = %d", 
            g_total_tcp_client_errs, total_dyn_entries_in_gdf); 

        TCPClientFailureAvgTime *cur_ptr = NULL, *next_ptr = NULL;

        if(g_total_tcp_client_errs != total_dyn_entries_in_gdf)
        {
          size = (g_total_tcp_client_errs - total_dyn_entries_in_gdf) * sizeof(TCPClientFailureAvgTime);

          cur_ptr = (TCPClientFailureAvgTime *)((char*)cur_avg + g_tcp_client_failures_avg_idx);
          next_ptr = (TCPClientFailureAvgTime *)((char*)next_avg + g_tcp_client_failures_avg_idx);

          PARENT_COPY_TCP_CLIENT_FAILURES_PERIODIC_DATA_CUR2NEXT(next_ptr, cur_ptr, 
              total_dyn_entries_in_gdf, g_total_tcp_client_errs);

          memset(&cur_ptr[total_dyn_entries_in_gdf], 0, size);

        }
        break;
      }
      case NEW_OBJECT_DISCOVERY_UDP_CLIENT_FAILURES:
      {
        NSDL2_MESSAGES(NULL, NULL, "g_total_udp_client_errs = %d, total_dyn_entries_in_gdf = %d", 
            g_total_udp_client_errs, total_dyn_entries_in_gdf); 

        UDPClientFailureAvgTime *cur_ptr = NULL, *next_ptr = NULL;

        if(g_total_udp_client_errs != total_dyn_entries_in_gdf)
        {
          size = (g_total_udp_client_errs - total_dyn_entries_in_gdf) * sizeof(UDPClientFailureAvgTime);

          cur_ptr = (UDPClientFailureAvgTime *)((char*)cur_avg + g_udp_client_failures_avg_idx);
          next_ptr = (UDPClientFailureAvgTime *)((char*)next_avg + g_udp_client_failures_avg_idx);

          PARENT_COPY_UDP_CLIENT_FAILURES_PERIODIC_DATA_CUR2NEXT(next_ptr, cur_ptr, 
              total_dyn_entries_in_gdf, g_total_udp_client_errs);

          memset(&cur_ptr[total_dyn_entries_in_gdf], 0, size);

        }
        break;
      }
      default:
        NSDL2_MESSAGES(NULL, NULL, "Default case"); 
    }
  }
}

int decrease_sample_count(int child_id, int sample_id)
{
  int slot_id;
  int mask_idx;
  unsigned long bit_idx;

  NSDL3_MESSAGES(NULL, NULL, "Method called, sample_id = %d, child = %d", sample_id, child_id);

  for(slot_id = 0; slot_id < global_settings->progress_report_queue_size; slot_id++)
  {
    if((progress_info[slot_id].sample_id != -1) && (!sample_id || progress_info[slot_id].sample_id == sample_id))
    {
      mask_idx = child_id / (sizeof(unsigned long)*8);
      bit_idx = 1 << (child_id%(sizeof(unsigned long)*8));
  
      if(progress_info[slot_id].child_mask[mask_idx] & bit_idx)
      {
        NSDL1_MESSAGES(NULL, NULL, "Bit is already set for child = %d", child_id);
        progress_info[slot_id].sample_count--;
        progress_info[slot_id].child_mask[mask_idx] &= ~bit_idx;
        if(sample_id)
          break;
      }
    }
  }
  return 0;
}

void print_child_mask(int recv_sample)
{
  int i;
  
  for(i = 0; i < global_settings->progress_report_queue_size; i++)
    if(progress_info[i].sample_id == recv_sample)
      break;

  if(i == global_settings->progress_report_queue_size)
    return;
  
  NSTL1(NULL, NULL, "Report# %d, Sample Bitmask = [%X-%X-%X-%X]", recv_sample,
		     progress_info[i].child_mask[0], progress_info[i].child_mask[1], 
		     progress_info[i].child_mask[2], progress_info[i].child_mask[3]);
}

/***********************************************************************************************
 |  • NAME:   	
 | 	check_progress_report_complete() - check and deliver progress report
 |
 |  • SYNOPSIS: 
 | 	void check_progress_report_complete(int completion_mode)	
 |
 |  • DESCRIPTION:   	
 |	Purpose 1: handle when all children progress report is received
 |	Purpose 2: handle when delayed for more than queued progress report  
 |		then dump the cur_sample forcefully.
 |
 |  • RETURN VALUE:
 |	nothing
 |
 |  TODO: write complete logic for the below func 
 |   while loop ==> to dump the progress report from current to last received
 |   this default approach all the time we are going to dump all the previous 
 |   progress report coming
 ************************************************************************************************/
void check_progress_report_complete(int sample_completed, int completion_mode, int num_rcd)
{
  int i, j, sample_count;
  ProgressMsg *pmsg;
  nslb_mp_handler *pool;
  
  NSDL1_MESSAGES(NULL, NULL, "Method called. completion_mode = %d, num_rcd = %d, num_pge= %d", completion_mode, num_rcd, g_data_control_var.num_pge);

  /**********************************************************
   -  When progress sample is received from all the children
   -  When queue is full then force fully complete current sample
   -  When a child got killed num_pge is decreased while num_rcd may be
      greater for that particular sample (This may not be possible as 
      num_pge will be updated later after processing that sample, but
      it may be saved in pool)
      (  completion_mode  ) --> Dumping current sample when queue is full
      (num_rcd >= num_pge) --> Dump all the previous sample of present sample 
  ***********************************************************/
 
  if ((num_rcd >= g_data_control_var.num_pge) || (completion_mode == FORCE_COMPLETE)) 
  {
    if(completion_mode != NORMAL_COMPLETE)
    {
      print_child_mask(sample_completed);
    }
    //Iterate for dumping all previous samples from present sample came
    while(cur_sample <= sample_completed)
    {
      /* In the case of FORCE_COMPLETE, check whether there is any sample exist for sampleID 'cur_sample' 
         if yes then deliver it otherwise break the loop */
      if(completion_mode == FORCE_COMPLETE)
      {
        sample_count = get_sample_info(cur_sample);
        if(sample_count <= 0)
          break;
      }
      
      NSDL3_MESSAGES(NULL, NULL, "Sample completed %d", cur_sample);

      g_cur_avg[0]->complete = 1;
      copy_from_cur_to_next(g_cur_avg[0], g_next_avg[0]);
   
      //TODO review with neeraj 
      /* According to previous design we are saving next to cur cumulative before delivery, 
          save finished add cumulatives for all features except tx which is done in copy_progress_data. */

      /* Now report will be delivered first then all the reportes will be saved to the next slot as current slot will be 
         freed after delivery and save_finished will save the cumulatives stats for all features including trancations. */

      //TODO How to control time stamp of two delivered report as rtg will have same time stamp
      deliver_report(run_mode, dh_master_fd, g_cur_avg, g_cur_finished, rfp, NULL);
      //adding data from g_cur_finished and g_next_finished
      save_finished(g_cur_finished, g_next_finished);
      //clearing slot of cur_sample from sample table as cur_sample is completed
      reset_sample_info(cur_sample);

      /* Loop thorugh all generator queue and get the progress report states, zero index is for overall stats.*/
      for(i=0; i<(sgrp_used_genrator_entries + 1); i++)
      {
        #ifdef CHK_AVG_FOR_JUNK_DATA 
        check_avgtime_for_junk_data("ns_progress_report.c[1153]", 2);
        #endif

        pool = (nslb_mp_handler *)progress_data_pool[i];
        pmsg = (ProgressMsg *) nslb_mp_busy_head(pool);//Current Slot
        nslb_mp_free_slot_ex(pool, pmsg);
        /* Move next sample to current sample as current sample is processed and freed.
           Get next avaiable sample from queue and set the next as previous next is moved to current. */
        pmsg = (ProgressMsg *) nslb_mp_busy_head(pool);//Current + 1 Slot  
	/*Set current avg & cavg*/
        g_cur_avg[i] = (avgtime*)(pmsg->sample_data);
        g_cur_finished[i] = (cavgtime*) pmsg->sample_cum_data;
        /*Get next progress report sample from queue*/
        pmsg = nslb_next(pmsg);
        if(!pmsg)
        {
          //when pool is full
          //TODO check
          if(pool && !pool->free_head)
          {
            NSTL1_OUT(NULL, NULL, "Memory pool exhausted, Next slot is not available to be allocated for sample = %d",
                               cur_sample + 2);
            timeout = kill_timeout; /* set_timer;*/
            return;
          }
          
          if(!i) NSTL1(NULL, NULL, "Getting new slot for sample id = %d", cur_sample + 1);
          pmsg = (ProgressMsg *)nslb_mp_get_slot(pool); // Current + 2 Slot
          pmsg->sample_id = cur_sample + 2;
          /*Set next avg & cavg*/
          g_next_avg[i] = (avgtime*) pmsg->sample_data;
          g_next_finished[i] = (cavgtime*) pmsg->sample_cum_data;
          /*Reset both avg & cavg as thay may contain old data so better to reset here*/
          for(j = 0; j < (TOTAL_GRP_ENTERIES_WITH_GRP_KW); j++)
          {
            GET_AVG_STRUCT(g_next_avg[i], j);
            init_avgtime (tmp_reset_avg, j);
            tmp_reset_avg->opcode = PROGRESS_REPORT;
            tmp_reset_avg->elapsed = cur_sample+2;
            
            GET_CAVG_STRUCT(g_next_finished[i], j);
            init_cavgtime (tmp_reset_cavg, j);
          }
        }
        else
        {
          /*Set next avg & cavg*/
          g_next_avg[i] = (avgtime*) pmsg->sample_data;
          g_next_finished[i] = (cavgtime*) pmsg->sample_cum_data;
        } 
        #ifdef CHK_AVG_FOR_JUNK_DATA 
        check_avgtime_for_junk_data("ns_progress_report.c[1153]", 2);
        #endif
      }
      cur_sample++; //Moving current to next sample as current is processed
    }
    timeout = kill_timeout; /* set_timer;*/
  }
}

/* Copy progress report data
   Accumulate child data in parent avgs and cavgs */
void copy_progress_data (avgtime **gtotal_avg, avgtime *msg, cavgtime **gsave, int gen_idx)
{
  int i, j;
  avgtime *total_avg = NULL; 
  cavgtime *save = NULL;
  FTPAvgTime *ftp_msg = NULL;
  FTPCAvgTime *ftp_save = NULL;
  FTPAvgTime *total_ftpavg = NULL;
  LDAPAvgTime *ldap_msg = NULL;
  LDAPCAvgTime *ldap_save = NULL;
  LDAPAvgTime *total_ldapavg = NULL;
  GROUPAvgTime *group_msg = NULL;
  GROUPAvgTime *total_grpavg = NULL; 
  IPBasedAvgTime *ip_msg = NULL;
  IPBasedAvgTime *total_ipavg = NULL;
  RBUPageStatAvgTime *rbu_page_stat_msg = NULL;
  RBUPageStatAvgTime *total_rbu_page_stat_msg = NULL;
  PageStatAvgTime *page_based_stat_msg = NULL;
  PageStatAvgTime *total_page_based_stat_msg = NULL;
  IMAPAvgTime *imap_msg = NULL;
  IMAPCAvgTime *imap_save = NULL;
  IMAPAvgTime *total_imapavg = NULL;
  JRMIAvgTime *jrmi_msg = NULL;
  JRMICAvgTime *jrmi_save = NULL;
  JRMIAvgTime *total_jrmiavg = NULL;
  WSAvgTime *ws_msg = NULL;
  WSCAvgTime *ws_save = NULL;
  WSAvgTime *total_wsavg = NULL;

  cavgtime *nc_cavg =  NULL;
  cavgtime *gen_save =  NULL;
  cavgtime *tmp_gen_save =  NULL;
  avgtime *gen_total_avg = NULL; 
  VUserFlowAvgTime *vuser_flow_msg;
  VUserFlowAvgTime *total_vuser_flow_avg;
  SrvIPAvgTime *srv_ip_msg = NULL;
  SrvIPAvgTime *total_srv_ip_avg = NULL;
  SrvIPAvgTime *gen_total_srv_ip_avg = NULL;

  XMPPAvgTime *xmpp_msg = NULL;
  XMPPCAvgTime *xmpp_save = NULL;
  XMPPAvgTime *total_xmppavg = NULL;
 
  FC2AvgTime *fc2_msg = NULL;
  FC2CAvgTime *fc2_save = NULL;
  FC2AvgTime *total_fc2avg = NULL;
  
  HTTPRespCodeAvgTime *http_resp_code_msg = NULL;
  HTTPRespCodeAvgTime *total_http_resp_code_avg = NULL;
  HTTPRespCodeAvgTime *gen_total_http_resp_code_avg = NULL;

  CavTestHTTPAvgTime *cavtest_msg = NULL;
  CavTestHTTPAvgTime *total_cavtest_msg = NULL;
  CavTestWebAvgTime *cavtest_web_msg = NULL;
  CavTestWebAvgTime *total_cavtest_web_msg = NULL;

  NSDL2_MESSAGES(NULL, NULL, "Method called, gen_idx = %d", gen_idx);
  
  total_avg = (avgtime *)gtotal_avg[0];
  save = (cavgtime *)gsave[0];
  
  if(gen_idx != -1)
  {
    tmp_gen_save = gsave[gen_idx + 1];
    gen_total_avg = gtotal_avg[gen_idx + 1];
  }

  //HTTP Resp Code 
  http_resp_code_msg = ((HTTPRespCodeAvgTime*)((char*)msg + g_http_status_code_loc2norm_table[msg->child_id].loc_http_status_code_avg_idx));
  total_http_resp_code_avg = ((HTTPRespCodeAvgTime*)((char*)total_avg + http_resp_code_avgtime_idx));
  total_avg->total_http_resp_code_entries = total_http_resp_code_entries;

  ACC_HTTP_RESP_CODE_PERIODICS (msg->total_http_resp_code_entries, total_http_resp_code_avg, http_resp_code_msg, msg->child_id);

  if(gen_idx != -1)
  {
    gen_total_http_resp_code_avg = (HTTPRespCodeAvgTime*)((char *)gen_total_avg + http_resp_code_avgtime_idx) ;
    ACC_HTTP_RESP_CODE_PERIODICS (msg->total_http_resp_code_entries, gen_total_http_resp_code_avg, http_resp_code_msg, msg->child_id);
  }

  NSDL2_MESSAGES(NULL, NULL, "Response Code : Copy response code data for parent/master gen_idx = %d msg = %p, "
                             "http_resp_code_msg = %p, total_avg = %p total_http_resp_code_avg = %p, "
                             "msg_total_http_status_code_entries = %d, http_resp_code_avgtime_idx = %d "
                             "g_http_status_code_loc2norm_table[msg->child_id].loc_http_status_code_avg_idx = %d ", 
                              (gen_idx + 1), msg, http_resp_code_msg, total_avg, total_http_resp_code_avg, 
                              msg->total_http_resp_code_entries, http_resp_code_avgtime_idx,
                              g_http_status_code_loc2norm_table[msg->child_id].loc_http_status_code_avg_idx);


  // TCP Client Failure 
  if (g_total_tcp_client_errs)
  {
    total_avg->total_tcp_client_failures_entries = g_total_tcp_client_errs;
    PARENT_COPY_TCP_CLIENT_FAILURES_PR(total_avg, save, msg, msg->child_id, msg->total_tcp_client_failures_entries);
    if (gen_idx != -1)
      PARENT_COPY_TCP_CLIENT_FAILURES_PR(gen_total_avg, tmp_gen_save, msg, msg->child_id, msg->total_tcp_client_failures_entries);
  }

  if (g_total_udp_client_errs)
  {
    total_avg->total_udp_client_failures_entries = g_total_udp_client_errs;
    PARENT_COPY_UDP_CLIENT_FAILURES_PR(total_avg, save, msg, msg->child_id, msg->total_udp_client_failures_entries);
    if (gen_idx != -1)
      PARENT_COPY_UDP_CLIENT_FAILURES_PR(gen_total_avg, tmp_gen_save, msg, msg->child_id, msg->total_udp_client_failures_entries);
  }

  //Here we can set or accumulate Transaction data because TX Stats is never used as a group based.
  if(total_tx_entries)
  {
    TxDataSample *tx_msg = NULL;

    TxDataSample *total_txavg = NULL;
    TxDataSample *gen_total_txavg = NULL;
    TxDataCum *tx_save = NULL;
    TxDataCum *gen_tx_save = NULL;

    //tx_msg = (TxDataSample*)((char*)msg + g_trans_avgtime_idx);
    tx_msg = (TxDataSample*)((char*)msg + g_tx_loc2norm_table[msg->child_id].loc_tx_avg_idx);
    total_txavg = (TxDataSample*)((char*)gtotal_avg[0] + g_trans_avgtime_idx);
    tx_save = (TxDataCum*)((char*)gsave[0] + g_trans_cavgtime_idx);

    int child_total_tx_entries = msg->total_tx_entries;

    NSDL2_MESSAGES(NULL, NULL, "g_trans_avgtime_idx = %d, g_trans_cavgtime_idx = %d, tx_msg = %p, gtotal_avg = %p, total_tx_entries = %d, child_total_tx_entries = %d", g_trans_avgtime_idx, g_trans_cavgtime_idx, tx_msg, gtotal_avg[0], total_tx_entries, child_total_tx_entries);

    gtotal_avg[0]->total_tx_entries = total_tx_entries;

    NSDL2_MESSAGES(NULL, NULL, "gtotal_avg[0]->total_tx_entries = %d", gtotal_avg[0]->total_tx_entries);
    //Here we set TX periodic data
    SET_MIN_MAX_PERIODICS_TX_STATS(child_total_tx_entries, total_txavg, tx_msg, msg->child_id);
    ACC_PERIODICS_TX_STATS(child_total_tx_entries, total_txavg, tx_msg, msg->child_id);

    //Here we set TX cummlative data
    SET_MIN_MAX_CUM_TX_STATS(child_total_tx_entries, tx_save, tx_msg, msg->child_id);
    ACC_CUM_TX_STATS(child_total_tx_entries, tx_save, tx_msg, msg->child_id);

    if (gen_idx != -1) {
      NSDL2_MESSAGES(NULL, NULL, "Copy transaction data for generators");

      gen_tx_save = (TxDataCum*)((char*)tmp_gen_save + g_trans_cavgtime_idx);
      gen_total_txavg = (TxDataSample*)((char*)gen_total_avg + g_trans_avgtime_idx);

      //Here we set TX periodic data
      SET_MIN_MAX_PERIODICS_TX_STATS(child_total_tx_entries, gen_total_txavg, tx_msg, msg->child_id);
      ACC_PERIODICS_TX_STATS(child_total_tx_entries, gen_total_txavg, tx_msg, msg->child_id);

      //Here we set TX cummlative data
      SET_MIN_MAX_CUM_TX_STATS(child_total_tx_entries, gen_tx_save, tx_msg, msg->child_id);
      ACC_CUM_TX_STATS(child_total_tx_entries, gen_tx_save, tx_msg, msg->child_id);

      copy_data_into_tx_buf(tmp_gen_save, gsavedTxData[gen_idx + 1]);
      create_trans_data_file(gen_idx, gsavedTxData[gen_idx + 1]);
    }
  }

  /*RBU Page Stat*/
  if( global_settings->browser_used != -1) {
    rbu_page_stat_msg = (RBUPageStatAvgTime*)((char*)msg +rbu_page_stat_data_gp_idx);
    total_rbu_page_stat_msg = (RBUPageStatAvgTime*)((char*)total_avg + rbu_page_stat_data_gp_idx);
    SET_MIN_MAX_RBU_PAGE_STAT_DATA_PERIODICS(total_rbu_page_stat_msg, rbu_page_stat_msg);
    ACC_RBU_PAGE_STAT_DATA_PERIODICS(total_rbu_page_stat_msg, rbu_page_stat_msg);
    NSDL2_MESSAGES(NULL, NULL, "total_rbu_page_stat_msg = %p, rbu_page_stat_msg = %p", total_rbu_page_stat_msg, rbu_page_stat_msg);
  }
 
  /*Page Based Stat*/
  if(global_settings->page_based_stat == PAGE_BASED_STAT_ENABLED) {
    NSDL2_MESSAGES(NULL, NULL, "total_page_based_stat_msg = %p, page_based_stat_msg = %p", total_page_based_stat_msg, page_based_stat_msg);
    page_based_stat_msg = (PageStatAvgTime*)((char*)msg + page_based_stat_gp_idx);
    total_page_based_stat_msg = (PageStatAvgTime*)((char*)total_avg + page_based_stat_gp_idx);
    SET_MIN_MAX_PAGE_BASED_STAT_PERIODICS(total_page_based_stat_msg, page_based_stat_msg);
    ACC_PAGE_BASED_STAT_PERIODICS(total_page_based_stat_msg, page_based_stat_msg);
    NSDL2_MESSAGES(NULL, NULL, "total_page_based_stat_msg = %p, page_based_stat_msg = %p", total_page_based_stat_msg, page_based_stat_msg);
  }

  /*Cavisson Test Monitor*/
  if(global_settings->monitor_type == HTTP_API) {
    cavtest_msg = (CavTestHTTPAvgTime*)((char*)msg + g_cavtest_http_avg_idx);
    total_cavtest_msg = (CavTestHTTPAvgTime*)((char*)total_avg + g_cavtest_http_avg_idx);
    FILL_CAVTEST_HTTP_API_DATA_PERIODIC(total_cavtest_msg, cavtest_msg);
    NSDL2_MESSAGES(NULL, NULL, "total_cavtest_msg = %p, cavtest_msg = %p", total_cavtest_msg, cavtest_msg);
  }
  else if(global_settings->monitor_type == WEB_PAGE_AUDIT){
    cavtest_web_msg = (CavTestWebAvgTime*)((char*)msg + g_cavtest_web_avg_idx);
    total_cavtest_web_msg = (CavTestWebAvgTime*)((char*)total_avg + g_cavtest_web_avg_idx);
    FILL_CAVTEST_WEB_PAGE_AUDIT_DATA_PERIODIC(total_cavtest_web_msg, cavtest_web_msg);
    NSDL2_MESSAGES(NULL, NULL, "total_cavtest_web_msg = %p, cavtest_web_msg = %p", total_cavtest_web_msg, cavtest_web_msg);
  }

  /*Here we can fill page think time, keep-alive and session pacing related data for Group based feature*/
  if(SHOW_GRP_DATA) {
    group_msg = (GROUPAvgTime*)((char*)msg + group_data_gp_idx);
    total_grpavg = (GROUPAvgTime*)((char*)total_avg + group_data_gp_idx);
    SET_MIN_MAX_GROUP_DATA_PERIODICS(total_grpavg, group_msg);
    ACC_GROUP_DATA_PERIODICS(total_grpavg, group_msg);
    NSDL2_MESSAGES(NULL, NULL, "total_grpavg = %p, group_msg = %p", total_grpavg, group_msg);
  }

  if(SHOW_RUNTIME_RUNLOGIC_PROGRESS) {
    vuser_flow_msg = (VUserFlowAvgTime*)((char*)msg + show_vuser_flow_idx);
    total_vuser_flow_avg = (VUserFlowAvgTime*)((char*)total_avg + show_vuser_flow_idx);
    NSDL2_MESSAGES(NULL, NULL, "vuser_flow_msg = %p, total_vuser_flow_avg = %p, total_flow_path_entries = %d", vuser_flow_msg, total_vuser_flow_avg, total_flow_path_entries);
    for(i= 0; i < total_flow_path_entries; i++) {
      total_vuser_flow_avg[i].cur_vuser_running_flow += vuser_flow_msg[i].cur_vuser_running_flow;
      NSDL2_MESSAGES(NULL, NULL, "Accumulate runtime_runlogic_progress data where idx = %d, cur_vuser_running_flow = %d, total_cur_vuser_running_flow = %d", i, vuser_flow_msg[i].cur_vuser_running_flow, total_vuser_flow_avg[i].cur_vuser_running_flow);
    } 
  }

  if(SHOW_SERVER_IP)
  {
    //srv_ip_msg = (SrvIPAvgTime*)((char*)msg + srv_ip_data_idx);
    srv_ip_msg = (SrvIPAvgTime*)((char*)msg + g_svr_ip_loc2norm_table[msg->child_id].loc_srv_ip_avg_idx);
    total_srv_ip_avg = (SrvIPAvgTime*)((char*)total_avg + srv_ip_data_idx);
    NSDL2_MESSAGES(NULL, NULL, "SRVIP: Copy server_ip data for parent/master gen_idx = %d msg = %p, srv_ip_msg = %p, total_avg = %p "
                               "total_srv_ip_avg = %p msg_total_norm_srv_ips = %d, srv_ip_data_idx = %d "
                               "g_svr_ip_loc2norm_table[msg->child_id].loc_srv_ip_avg_idx = %d ", (gen_idx + 1), msg, srv_ip_msg,
                                total_avg, total_srv_ip_avg, msg->total_server_ip_entries, srv_ip_data_idx,
                                g_svr_ip_loc2norm_table[msg->child_id].loc_srv_ip_avg_idx);

    total_avg->total_server_ip_entries = total_normalized_svr_ips;
    SET_SRVIP_STATS(msg->total_server_ip_entries, total_srv_ip_avg, srv_ip_msg, msg->child_id);

    if (gen_idx != -1) {
      gen_total_srv_ip_avg = (SrvIPAvgTime*)((char*)gen_total_avg + srv_ip_data_idx);
      NSDL2_MESSAGES(NULL, NULL, "Copy server_ip data for generators gen_idx = %d, gen_total_avg = %p gen_total_srv_ip_avg = %p",(gen_idx +1),
                                  gen_total_avg, gen_total_srv_ip_avg);
      SET_SRVIP_STATS(msg->total_server_ip_entries, gen_total_srv_ip_avg, srv_ip_msg, msg->child_id);
    }
  }

  /*RBU Domain Stat*/
  NSDL2_MESSAGES(NULL, NULL, "Copy progress data: rbu_domain_stats_mode = %d, total_rbu_domain_entries = %d", 
                              global_settings->rbu_domain_stats_mode, total_rbu_domain_entries);

  if(global_settings->rbu_domain_stats_mode && total_rbu_domain_entries) {
    Rbu_domain_stat_avgtime *rbu_domain_stat_msg = NULL;
    Rbu_domain_stat_avgtime *total_rbu_domain_stat_msg = NULL;
    Rbu_domain_stat_avgtime *gen_total_rbu_domain_stat_msg = NULL;
 
    rbu_domain_stat_msg = (Rbu_domain_stat_avgtime*)((char*)msg + g_domain_loc2norm_table[msg->child_id].loc_domain_avg_idx);
    total_rbu_domain_stat_msg = (Rbu_domain_stat_avgtime*)((char*)total_avg + rbu_domain_stat_avg_idx);
    
    int child_total_rbu_domain_entries = msg->total_rbu_domain_entries;
    gtotal_avg[0]->total_rbu_domain_entries = total_rbu_domain_entries;

    NSDL2_MESSAGES(NULL, NULL, "RBUDomainStat: copy progress data for Overall- rbu_domain_stat_avg_idx = %d, "
                               "rbu_domain_stat_msg = %p, total_rbu_domain_stat_msg = %p, total_rbu_domain_entries = %d, "
                               "child_total_rbu_domain_entries = %d", 
                                rbu_domain_stat_avg_idx, rbu_domain_stat_msg, total_rbu_domain_stat_msg, 
                                total_rbu_domain_entries, child_total_rbu_domain_entries);

    SET_MIN_MAX_PERIODICS_RBU_DOMAIN_STATS(child_total_rbu_domain_entries, total_rbu_domain_stat_msg, rbu_domain_stat_msg, msg->child_id);
    ACC_PERIODICS_RBU_DOMAIN_STATS(child_total_rbu_domain_entries, total_rbu_domain_stat_msg, rbu_domain_stat_msg, msg->child_id);

    if (gen_idx != -1) {
      NSDL2_MESSAGES(NULL, NULL, "Copy transaction data for generators");

      gen_total_rbu_domain_stat_msg = (Rbu_domain_stat_avgtime*)((char*)gen_total_avg + rbu_domain_stat_avg_idx);

      NSDL2_MESSAGES(NULL, NULL, "RBUDomainStat: copy progress data for generator id = %d, gen_total_rbu_domain_stat_msg = %d", 
                                  (gen_idx + 1), gen_total_rbu_domain_stat_msg);

      SET_MIN_MAX_PERIODICS_RBU_DOMAIN_STATS(child_total_rbu_domain_entries, gen_total_rbu_domain_stat_msg, rbu_domain_stat_msg, msg->child_id);
      ACC_PERIODICS_RBU_DOMAIN_STATS(child_total_rbu_domain_entries, gen_total_rbu_domain_stat_msg, rbu_domain_stat_msg, msg->child_id);

    }
  }

  // Accumulate periodic in avgtime and cummulative counters in cavgtime
  if(loader_opcode == STAND_ALONE)
  {
    ACC_NETSTORM_DIAGNOSTICS(total_avg, msg, save); //Overall
  }
  else
  {
    ACC_NETCLOUD_DIAGNOSTICS(total_avg, msg, save); //Overall
  }
  //Copy avgtime struct and msg_data_ptr data for particular generator 
  if (loader_opcode == MASTER_LOADER) {
    avgtime *nc_avg = NULL;
    nc_avg = (avgtime *)gtotal_avg[gen_idx + 1];
    nc_cavg = (cavgtime *)gsave[gen_idx + 1];
    /*g_static_avgtime_size excludes transaction, server_ip and ip_data data
     transaction, server_ip are handled using normalization above.
     ip_data is copied separately after this*/
    memcpy(nc_avg, msg, g_static_avgtime_size); //Generator
    ACC_NETSTORM_DIAGNOSTICS_CUMULATIVES(nc_cavg, msg); //Generator Cumulativs
  }

  if((global_settings->show_ip_data == IP_BASED_DATA_ENABLED) && (total_ip_entries)) {
    ip_msg = (IPBasedAvgTime*)((char*)msg + g_ipdata_loc_ipdata_avg_idx[msg->child_id]);
    total_ipavg = (IPBasedAvgTime*)((char*)total_avg + g_ipdata_loc_ipdata_avg_idx[msg->child_id]);

    if(loader_opcode == MASTER_LOADER){
      avgtime *loc_nc_avg = NULL;
      loc_nc_avg = (avgtime *)gtotal_avg[gen_idx + 1];
      memcpy(loc_nc_avg + ip_data_gp_idx , ip_msg, ip_avgtime_size);
    }
    else {
      for(i= 0; i < total_group_ip_entries; i++) {
        total_ipavg[i].cur_url_req += ip_msg[i].cur_url_req;
      }
    }
  }

  if (loader_opcode == MASTER_LOADER) 
  {
    total_avg->eth_rx_bps += msg->eth_rx_bps;
    total_avg->eth_tx_bps += msg->eth_tx_bps;
    total_avg->eth_rx_pps += msg->eth_rx_pps;
    total_avg->eth_tx_pps += msg->eth_tx_pps;
  }
  
  if(total_um_entries)
  {
    //Here we can fill user monitor data from msg avg to total_avg, save and generator nc_cavg  
    fill_um_data_in_avg_and_cavg((UM_data *)((char *)total_avg + g_avg_um_data_idx), 
                                  (UM_data *)((char *)msg + g_avg_um_data_idx), 
                                  (UM_data*)((char*)save + g_cavg_um_data_idx),  
                                  nc_cavg?(UM_data*)((char*)nc_cavg + g_cavg_um_data_idx):NULL);
  }


  avgtime *tmp_total_avg = total_avg;
  avgtime *tmp_msg = msg;
  cavgtime *tmp_save = save;
  for (j = 0; j < TOTAL_GRP_ENTERIES_WITH_GRP_KW; j++) 
  {
    total_avg = (avgtime*)((char*)tmp_total_avg + (j * g_avg_size_only_grp));
    msg = (avgtime*)((char*)tmp_msg + (j * g_avg_size_only_grp));
    save = (cavgtime*)((char*)tmp_save + (j * g_cavg_size_only_grp));

    SET_MIN_MAX_PERIODICS (total_avg, msg);
    ACC_PERIODICS (total_avg, msg);
 
    SET_MIN_MAX_CUMULATIVES (save, msg);
    ACC_CUMULATIVES (save, msg);

    //Accumlate avgtime data in cavgtime data for transactions
    SET_MIN_MAX_TX_CUMULATIVE (save, msg);
    ACC_TX_DATA (save, msg);

    if(global_settings->protocol_enabled & FTP_PROTOCOL_ENABLED) {
        ftp_msg = (FTPAvgTime*)((char*)msg + g_ftp_avgtime_idx);
        ftp_save = (FTPCAvgTime*)((char*)save + g_ftp_cavgtime_idx);
        SET_MIN_MAX_FTP_CUMULATIVES(ftp_save, ftp_msg);
        ACC_FTP_CUMULATIVES(ftp_save, ftp_msg);
      }
 
    //for LDAP
      if(global_settings->protocol_enabled & LDAP_PROTOCOL_ENABLED) {
        ldap_msg = (LDAPAvgTime*)((char*)msg + g_ldap_avgtime_idx);
        ldap_save = (LDAPCAvgTime*)((char*)save + g_ldap_cavgtime_idx);
        SET_MIN_MAX_LDAP_CUMULATIVES(ldap_save, ldap_msg);
        ACC_LDAP_CUMULATIVES(ldap_save, ldap_msg);
      }
    
     if(global_settings->protocol_enabled & IMAP_PROTOCOL_ENABLED) {
        imap_msg = (IMAPAvgTime*)((char*)msg + g_imap_avgtime_idx);
        imap_save = (IMAPCAvgTime*)((char*)save + g_imap_cavgtime_idx);
        SET_MIN_MAX_IMAP_CUMULATIVES(imap_save, imap_msg);
        ACC_IMAP_CUMULATIVES(imap_save, imap_msg);
      }
 
      if(global_settings->protocol_enabled & JRMI_PROTOCOL_ENABLED) {
        jrmi_msg = (JRMIAvgTime*)((char*)msg + g_jrmi_avgtime_idx);
        jrmi_save = (JRMICAvgTime*)((char*)save + g_jrmi_cavgtime_idx);
        SET_MIN_MAX_JRMI_CUMULATIVES(jrmi_save, jrmi_msg);
        ACC_JRMI_CUMULATIVES(jrmi_save, jrmi_msg);
      }

      //Websocket
      if(global_settings->protocol_enabled & WS_PROTOCOL_ENABLED) {
        ws_msg = (WSAvgTime*)((char*)msg + g_ws_avgtime_idx);
        ws_save = (WSCAvgTime*)((char*)save + g_ws_cavgtime_idx);
        SET_MIN_MAX_WS_CUMULATIVES(ws_save, ws_msg);
        ACC_WS_CUMULATIVES(ws_save, ws_msg);

        //ws_msg = (WSAvgTime*)((char*)msg +g_ws_avgtime_idx);
        total_wsavg = (WSAvgTime*)((char*)total_avg + g_ws_avgtime_idx);
        SET_MIN_MAX_WS_PERIODICS(total_wsavg, ws_msg);
        ACC_WS_PERIODICS(total_wsavg, ws_msg);
 
        NSDL2_WS(NULL, NULL, "WSAvgTime data:  g_ws_avgtime_idx = %d, total_wsavg = %p, ws_msg = %p, ws_save = %p, msg->ws_num_con_succ = %d, "
                             "total_wsavg->ws_num_con_succ = %d", 
                              g_ws_avgtime_idx, total_wsavg, ws_msg, ws_save, ws_msg->ws_num_con_succ, total_wsavg->ws_num_con_succ);

     }

     // Socket API, Copy Child/Gen data into Parent's memory for Overall 
     PARENT_COPY_SOCKET_TCP_UDP_CLIENT_PR(total_avg, save, msg);

     //FC2
      if(global_settings->protocol_enabled & FC2_PROTOCOL_ENABLED) {
        fc2_msg = (FC2AvgTime*)((char*)msg + g_fc2_avgtime_idx);
        fc2_save = (FC2CAvgTime*)((char*)save + g_fc2_cavgtime_idx);
        
        SET_MIN_MAX_FC2_CUMULATIVES(fc2_save, fc2_msg);
        ACC_FC2_CUMULATIVES(fc2_save, fc2_msg);

        total_fc2avg = (FC2AvgTime*)((char*)total_avg + g_fc2_avgtime_idx);
        SET_MIN_MAX_FC2_PERIODICS(total_fc2avg, fc2_msg);
        ACC_FC2_PERIODICS(total_fc2avg, fc2_msg);
           /*FC2*/
        total_fc2avg->fc2_total_bytes += fc2_msg->fc2_total_bytes;
        fc2_save->fc2_c_tot_total_bytes += fc2_msg->fc2_total_bytes;

        NSDL2_FC2(NULL, NULL, "FC2AvgTime data:  g_fc2_avgtime_idx = %d, total_fc2avg = %p, fc2_msg = %p, fc2_save = %p",
                                                 g_fc2_avgtime_idx, total_fc2avg, fc2_msg, fc2_save);
     }

     //JMeter
     #if 0 
     if(global_settings->protocol_enabled & JMETER_PROTOCOL_ENABLED) {
       jmeter_avgtime *jm_msg = (jmeter_avgtime*)((char*)msg + g_jmeter_avgtime_idx);

       jmeter_avgtime *total_jm_avg = (jmeter_avgtime*)((char*)total_avg + g_jmeter_avgtime_idx);
       //SET_MIN_MAX_WS_PERIODICS(total_jm_avg, jm_msg);
       ACC_JMETER_PERIODICS(total_jm_avg, jm_msg);
    }
    #endif

    //XMPP
    if(global_settings->protocol_enabled & XMPP_PROTOCOL_ENABLED) {
       xmpp_msg = (XMPPAvgTime*)((char*)msg + g_xmpp_avgtime_idx);
       xmpp_save = (XMPPCAvgTime*)((char*)save + g_xmpp_cavgtime_idx);

       total_xmppavg = (XMPPAvgTime*)((char*)total_avg + g_xmpp_avgtime_idx);

       ACC_XMPP_PERIODICS(total_xmppavg, xmpp_msg);
       ACC_XMPP_CUMULATIVES(xmpp_save, xmpp_msg);

      NSDL2_MESSAGES(NULL, NULL, "group = %d, xmpp_msg->xmpp_msg_sent = %d, xmpp_save->xmpp_msg_sent = %d", 
                                  j, xmpp_msg->xmpp_msg_sent, xmpp_save->xmpp_c_msg_sent);

    }
    //ftp
    if(global_settings->protocol_enabled & FTP_PROTOCOL_ENABLED) {
      ftp_msg = (FTPAvgTime*)((char*)msg +g_ftp_avgtime_idx);
      total_ftpavg = (FTPAvgTime*)((char*)total_avg + g_ftp_avgtime_idx);
      SET_MIN_MAX_FTP_PERIODICS(total_ftpavg, ftp_msg);
      ACC_FTP_PERIODICS(total_ftpavg, ftp_msg);
    }
 
    //LDAP
    if(global_settings->protocol_enabled & LDAP_PROTOCOL_ENABLED) {
      ldap_msg = (LDAPAvgTime*)((char*)msg +g_ldap_avgtime_idx);
      total_ldapavg = (LDAPAvgTime*)((char*)total_avg + g_ldap_avgtime_idx);
      SET_MIN_MAX_LDAP_PERIODICS(total_ldapavg, ldap_msg);
      ACC_LDAP_PERIODICS(total_ldapavg, ldap_msg);
    }
 
    //IMAP
    if(global_settings->protocol_enabled & IMAP_PROTOCOL_ENABLED) {
      imap_msg = (IMAPAvgTime*)((char*)msg +g_imap_avgtime_idx);
      total_imapavg = (IMAPAvgTime*)((char*)total_avg + g_imap_avgtime_idx);
      SET_MIN_MAX_IMAP_PERIODICS(total_imapavg, imap_msg);
      ACC_IMAP_PERIODICS(total_imapavg, imap_msg);
    }
    //JRMI
    if(global_settings->protocol_enabled & JRMI_PROTOCOL_ENABLED) {
      jrmi_msg = (JRMIAvgTime*)((char*)msg +g_jrmi_avgtime_idx);
      total_jrmiavg = (JRMIAvgTime*)((char*)total_avg + g_jrmi_avgtime_idx);
      SET_MIN_MAX_JRMI_PERIODICS(total_jrmiavg, jrmi_msg);
      ACC_JRMI_PERIODICS(total_jrmiavg, jrmi_msg);
    }

    ACC_CACHE_PERIODICS ((CacheAvgTime*)((char*)total_avg + g_cache_avgtime_idx), 
          		(CacheAvgTime*)((char*)msg + g_cache_avgtime_idx));
 
    ACC_PROXY_PERIODICS ((ProxyAvgTime*)((char*)total_avg + g_proxy_avgtime_idx), 
          		(ProxyAvgTime*)((char*)msg + g_proxy_avgtime_idx));
 
    ACC_NETWORK_CACHE_STATS_PERIODICS ((NetworkCacheStatsAvgTime*)((char*)total_avg + g_network_cache_stats_avgtime_idx),
                                  ((NetworkCacheStatsAvgTime*)((char*)msg + g_network_cache_stats_avgtime_idx)));  
 
    ACC_DNS_LOOKUP_STATS_PERIODICS ((DnsLookupStatsAvgTime*)((char*)total_avg + dns_lookup_stats_avgtime_idx),
                                  ((DnsLookupStatsAvgTime*)((char*)msg + dns_lookup_stats_avgtime_idx)));  
 
    //DOS Attack 
    ACC_DOS_ATTACK_PERIODICS ((DosAttackAvgTime*)((char*)total_avg + g_dos_attack_avgtime_idx), 
          		(DosAttackAvgTime*)((char*)msg + g_dos_attack_avgtime_idx));
    
    
    total_avg->total_bytes += msg->total_bytes;
    total_avg->tx_bytes += msg->tx_bytes;
    total_avg->rx_bytes += msg->rx_bytes;
    //to accumulate cummulative in cavg
    save->c_tot_rx_bytes += msg->rx_bytes;
    save->c_tot_tx_bytes += msg->tx_bytes;
    save->c_tot_total_bytes += msg->total_bytes;

    /* SMTP */
    total_avg->smtp_total_bytes += msg->smtp_total_bytes;
    total_avg->smtp_tx_bytes += msg->smtp_tx_bytes;
    total_avg->smtp_rx_bytes += msg->smtp_rx_bytes;
    save->smtp_c_tot_rx_bytes += msg->smtp_rx_bytes;
    save->smtp_c_tot_tx_bytes += msg->smtp_tx_bytes;
    save->smtp_c_tot_total_bytes += msg->smtp_total_bytes;
 
    /* POP3 */
    total_avg->pop3_total_bytes += msg->pop3_total_bytes;
    total_avg->pop3_tx_bytes += msg->pop3_tx_bytes;
    total_avg->pop3_rx_bytes += msg->pop3_rx_bytes;
    save->pop3_c_tot_rx_bytes += msg->pop3_rx_bytes;
    save->pop3_c_tot_tx_bytes += msg->pop3_tx_bytes;
    save->pop3_c_tot_total_bytes += msg->pop3_total_bytes;
 
 
    /* DNS */
    total_avg->dns_total_bytes += msg->dns_total_bytes;
    total_avg->dns_tx_bytes += msg->dns_tx_bytes;
    total_avg->dns_rx_bytes += msg->dns_rx_bytes;
    save->dns_c_tot_rx_bytes += msg->dns_rx_bytes;
    save->dns_c_tot_tx_bytes += msg->dns_tx_bytes;
    save->dns_c_tot_total_bytes += msg->dns_total_bytes;

    total_avg->num_connections += msg->num_connections;
    total_avg->smtp_num_connections += msg->smtp_num_connections;
    total_avg->pop3_num_connections += msg->pop3_num_connections;
 
    /*FTP*/
    if(global_settings->protocol_enabled & FTP_PROTOCOL_ENABLED) {
      FTPAvgTime *total_ftpavg = NULL;
      FTPAvgTime *ftp_msg = NULL;
      ftp_msg = (FTPAvgTime*)((char*)msg +g_ftp_avgtime_idx);
      total_ftpavg = (FTPAvgTime*)((char*)total_avg + g_ftp_avgtime_idx);
      total_ftpavg->ftp_num_connections += ftp_msg->ftp_num_connections;
    }
 
    //LDAP
    if(global_settings->protocol_enabled & LDAP_PROTOCOL_ENABLED) {
      LDAPAvgTime *total_ldapavg = NULL;
      LDAPAvgTime *ldap_msg = NULL;
      ldap_msg = (LDAPAvgTime*)((char*)msg +g_ldap_avgtime_idx);
      total_ldapavg = (LDAPAvgTime*)((char*)total_avg + g_ldap_avgtime_idx);
      total_ldapavg->ldap_num_connections += ldap_msg->ldap_num_connections;
    }
 
    //IMAP
    if(global_settings->protocol_enabled & IMAP_PROTOCOL_ENABLED) {
      IMAPAvgTime *total_imapavg = NULL;
      IMAPAvgTime *imap_msg = NULL;
      imap_msg = (IMAPAvgTime*)((char*)msg +g_imap_avgtime_idx);
      total_imapavg = (IMAPAvgTime*)((char*)total_avg + g_imap_avgtime_idx);
      total_imapavg->imap_num_connections += imap_msg->imap_num_connections;
    }
    //JRMI
    if(global_settings->protocol_enabled & JRMI_PROTOCOL_ENABLED) {
      JRMIAvgTime *total_jrmiavg = NULL;
      JRMIAvgTime *jrmi_msg = NULL;
      jrmi_msg = (JRMIAvgTime*)((char*)msg +g_jrmi_avgtime_idx);
      total_jrmiavg = (JRMIAvgTime*)((char*)total_avg + g_jrmi_avgtime_idx);
      total_jrmiavg->jrmi_num_connections += jrmi_msg->jrmi_num_connections;
    }

    //XMPP
    /*
    if(global_settings->protocol_enabled & XMPP_PROTOCOL_ENABLED) {
      XMPPAvgTime *total_xmppavg = NULL;
      XMPPAvgTime *xmpp_msg = NULL;
      xmpp_msg = (XMPPAvgTime*)((char*)msg +g_xmpp_avgtime_idx);
      total_xmppavg = (XMPPAvgTime*)((char*)total_avg + g_xmpp_avgtime_idx);
    }
    */
 
    total_avg->cur_vusers_active += msg->cur_vusers_active;
    total_avg->cur_vusers_thinking += msg->cur_vusers_thinking;
    total_avg->cur_vusers_waiting += msg->cur_vusers_waiting;
    total_avg->cur_vusers_cleanup += msg->cur_vusers_cleanup;
    total_avg->cur_vusers_in_sp += msg->cur_vusers_in_sp;
    total_avg->cur_vusers_blocked += msg->cur_vusers_blocked;
    total_avg->cur_vusers_paused += msg->cur_vusers_paused;
 
    total_avg->running_users += msg->running_users;

    NSDL4_MESSAGES(NULL, NULL, "j = %d, total_avg->cur_vusers_active = %d, msg->cur_vusers_active = %d, total_avg->cur_vusers_thinking = %d, msg->cur_vusers_thinking = %d, total_avg->cur_vusers_waiting = %d, msg->cur_vusers_waiting = %d, total_avg->cur_vusers_cleanup = %d, msg->cur_vusers_cleanup = %d, total_avg->cur_vusers_in_sp = %d, msg->cur_vusers_in_sp = %d, total_avg->cur_vusers_blocked = %d, msg->cur_vusers_blocked = %d, total_avg->cur_vusers_paused = %d, msg->cur_vusers_paused = %d, total_avg->running_users = %d, msg->running_users = %d", j, total_avg->cur_vusers_active, msg->cur_vusers_active, total_avg->cur_vusers_thinking, msg->cur_vusers_thinking, total_avg->cur_vusers_waiting, msg->cur_vusers_waiting, total_avg->cur_vusers_cleanup, msg->cur_vusers_cleanup, total_avg->cur_vusers_in_sp, msg->cur_vusers_in_sp, total_avg->cur_vusers_blocked, msg->cur_vusers_blocked, total_avg->cur_vusers_paused, msg->cur_vusers_paused, total_avg->running_users, msg->running_users);

    //Accumlate Generator Data for Transaction
    //For generators, we need to copy data from message to TxData struct and create and update transaction files
    if (gen_idx != -1) 
    {
      NSDL2_MESSAGES(NULL, NULL, "Copy transaction data for generators");
      gen_save = (cavgtime*)((char*)tmp_gen_save + (j * g_cavg_size_only_grp));
      avgtime *gen_total = (avgtime *)((char*)gen_total_avg + (j * g_cavg_size_only_grp));

      //Accumlate Generator avgtime data in avgtime
      SET_MIN_MAX_TX_CUMULATIVE (gen_save, msg);
      ACC_TX_DATA (gen_save, msg);
      SET_MIN_MAX_CUMULATIVES (gen_save, msg);
      ACC_CUMULATIVES (gen_save, msg);
     
      gen_save->c_tot_rx_bytes += msg->rx_bytes;
      gen_save->c_tot_tx_bytes += msg->tx_bytes;
      gen_save->c_tot_total_bytes += msg->total_bytes;
      
 
      gen_save->smtp_c_tot_rx_bytes += msg->smtp_rx_bytes;
      gen_save->smtp_c_tot_tx_bytes += msg->smtp_tx_bytes;
      gen_save->smtp_c_tot_total_bytes += msg->smtp_total_bytes;
 
      gen_save->pop3_c_tot_rx_bytes += msg->pop3_rx_bytes;
      gen_save->pop3_c_tot_tx_bytes += msg->pop3_tx_bytes;
      gen_save->pop3_c_tot_total_bytes += msg->pop3_total_bytes;
 
      gen_save->dns_c_tot_rx_bytes += msg->dns_rx_bytes;
      gen_save->dns_c_tot_tx_bytes += msg->dns_tx_bytes;
      gen_save->dns_c_tot_total_bytes += msg->dns_total_bytes;

      if(global_settings->protocol_enabled & FTP_PROTOCOL_ENABLED) { 
        SET_MIN_MAX_FTP_CUMULATIVES((FTPCAvgTime*)((char *)gen_save + g_ftp_cavgtime_idx), ftp_msg);
        ACC_FTP_CUMULATIVES((FTPCAvgTime*)((char *)gen_save + g_ftp_cavgtime_idx), ftp_msg);
      }
      
      if(global_settings->protocol_enabled & LDAP_PROTOCOL_ENABLED){
        SET_MIN_MAX_LDAP_CUMULATIVES((LDAPCAvgTime*)((char*)gen_save + g_ldap_cavgtime_idx), ldap_msg);
        ACC_LDAP_CUMULATIVES((LDAPCAvgTime*)((char*)gen_save + g_ldap_cavgtime_idx) , ldap_msg);
      }
      
      if(global_settings->protocol_enabled & IMAP_PROTOCOL_ENABLED){
        SET_MIN_MAX_IMAP_CUMULATIVES((IMAPCAvgTime*)((char*)gen_save + g_imap_cavgtime_idx), imap_msg);
        ACC_IMAP_CUMULATIVES((IMAPCAvgTime*)((char*)gen_save + g_imap_cavgtime_idx), imap_msg);
      }
 
      if(global_settings->protocol_enabled & JRMI_PROTOCOL_ENABLED){
        SET_MIN_MAX_JRMI_CUMULATIVES((JRMICAvgTime*)((char*)gen_save + g_jrmi_cavgtime_idx), jrmi_msg);
        ACC_JRMI_CUMULATIVES((JRMICAvgTime*)((char*)gen_save + g_jrmi_cavgtime_idx), jrmi_msg);
      }

      if(global_settings->protocol_enabled & WS_PROTOCOL_ENABLED){
        SET_MIN_MAX_WS_CUMULATIVES((WSCAvgTime*)((char*)gen_save + g_ws_cavgtime_idx), ws_msg);
        ACC_WS_CUMULATIVES((WSCAvgTime*)((char*)gen_save + g_ws_cavgtime_idx), ws_msg);
      }

      if(global_settings->protocol_enabled & FC2_PROTOCOL_ENABLED){
        SET_MIN_MAX_FC2_CUMULATIVES((FC2CAvgTime*)((char*)gen_save + g_fc2_cavgtime_idx), fc2_msg);
        ACC_FC2_CUMULATIVES((FC2CAvgTime*)((char*)gen_save + g_fc2_cavgtime_idx), fc2_msg);
      }

      if(global_settings->protocol_enabled & XMPP_PROTOCOL_ENABLED){
        ACC_XMPP_CUMULATIVES((XMPPCAvgTime*)((char*)gen_save + g_xmpp_cavgtime_idx), xmpp_msg);
      }
     
      // Socket API, Copy Child/Gen data into Parent's generator specific memory
      PARENT_COPY_SOCKET_TCP_UDP_CLIENT_PR(gen_total, gen_save, msg);
    }
  }
}

/*Copy finish report data
  accumulate end report|summary of progress from child to parent
*/
void copy_end_data (avgtime **gend_avg, avgtime *msg, cavgtime **gsave, int gen_idx)
{
  int i, j;
  FTPAvgTime *end_ftpavg = NULL;
  FTPCAvgTime *end_ftpcavg = NULL;
  FTPAvgTime *ftp_msg = NULL;
  LDAPAvgTime *end_ldapavg = NULL;
  LDAPCAvgTime *end_ldapcavg = NULL;
  LDAPAvgTime *ldap_msg = NULL;
  IMAPAvgTime *end_imapavg = NULL;
  IMAPCAvgTime *end_imapcavg = NULL;
  JRMIAvgTime *end_jrmiavg = NULL;
  JRMICAvgTime *end_jrmicavg = NULL;
  JRMIAvgTime *jrmi_msg = NULL;
  IMAPAvgTime *imap_msg = NULL;
  cavgtime *gen_save =  NULL;
  TxDataCum *gen_tx_save = NULL;

  XMPPCAvgTime *end_xmppcavg = NULL;
  XMPPAvgTime *xmpp_msg = NULL;

  FC2CAvgTime *end_fc2cavg = NULL;
  FC2AvgTime *fc2_msg = NULL;

  avgtime *end_avg = (avgtime *)(gend_avg[0]);
  cavgtime *save = (cavgtime *)(gsave[0]);

  NSDL2_MESSAGES(NULL, NULL, "Method called");

  //NC: For generators, we need to copy data from message to TxData struct and create and update transaction files
  if (gen_idx != -1 && total_tx_entries) { //Transaction data is only once so checking for first time only
    NSDL2_MESSAGES(NULL, NULL, "Copy transaction data for generators");
    gen_save = gsave[gen_idx + 1];
    gen_tx_save = (TxDataCum*)((char*)gen_save + g_trans_cavgtime_idx);
    SET_MIN_MAX_TX_CUMULATIVE (gen_tx_save, msg);
    ACC_TX_DATA (gen_tx_save, msg);
    copy_data_into_tx_buf(gen_save, gsavedTxData[gen_idx + 1]);
    create_trans_data_file(gen_idx, gsavedTxData[gen_idx + 1]);
  }

  avgtime *tmp_end_avg = end_avg;
  avgtime *tmp_msg = msg;
  cavgtime *tmp_save = save;

  for (j = 0; j < TOTAL_GRP_ENTERIES_WITH_GRP_KW; j++)
  {
    end_avg = (avgtime*)((char*)tmp_end_avg + (j * g_avg_size_only_grp));
    save = (cavgtime*)((char*)tmp_save + (j * g_cavg_size_only_grp));
    msg = (avgtime*)((char*)tmp_msg + (j * g_avg_size_only_grp));

    end_avg->opcode = FINISH_REPORT;
 
    SET_MIN_MAX_CUMULATIVES (save, msg);
 
    ACC_CUMULATIVES (save, msg);
 
    //FTP
    if(global_settings->protocol_enabled & FTP_PROTOCOL_ENABLED) {
      end_ftpavg = (FTPAvgTime*)((char*)end_avg + g_ftp_avgtime_idx);
      end_ftpcavg = (FTPCAvgTime*)((char*)save + g_ftp_cavgtime_idx);
      ftp_msg = (FTPAvgTime*)((char*)msg +g_ftp_avgtime_idx);
      SET_MIN_MAX_FTP_CUMULATIVES (end_ftpcavg, ftp_msg);
      ACC_FTP_CUMULATIVES (end_ftpcavg, ftp_msg);
      end_ftpavg->ftp_num_connections += ftp_msg->ftp_num_connections;
    }

    //LDAP
    if(global_settings->protocol_enabled & LDAP_PROTOCOL_ENABLED) {
      end_ldapavg = (LDAPAvgTime*)((char*)end_avg + g_ldap_avgtime_idx);
      end_ldapcavg = (LDAPCAvgTime*)((char*)save + g_ldap_cavgtime_idx);
      ldap_msg = (LDAPAvgTime*)((char*)msg +g_ldap_avgtime_idx);
      SET_MIN_MAX_LDAP_CUMULATIVES (end_ldapcavg, ldap_msg);
      ACC_LDAP_CUMULATIVES (end_ldapcavg, ldap_msg);
      end_ldapavg->ldap_num_connections += ldap_msg->ldap_num_connections;
    }
  
    //IMAP
    if(global_settings->protocol_enabled & IMAP_PROTOCOL_ENABLED) {
      end_imapavg = (IMAPAvgTime*)((char*)end_avg + g_imap_avgtime_idx);
      end_imapcavg = (IMAPCAvgTime*)((char*)save + g_imap_cavgtime_idx);
      imap_msg = (IMAPAvgTime*)((char*)msg +g_imap_avgtime_idx);
      SET_MIN_MAX_IMAP_CUMULATIVES (end_imapcavg, imap_msg);
      ACC_IMAP_CUMULATIVES (end_imapcavg, imap_msg);
      end_imapavg->imap_num_connections += imap_msg->imap_num_connections;
    }
 
    //JRMI
    if(global_settings->protocol_enabled & JRMI_PROTOCOL_ENABLED) {
      end_jrmiavg = (JRMIAvgTime*)((char*)end_avg + g_jrmi_avgtime_idx);
      end_jrmicavg = (JRMICAvgTime*)((char*)save + g_jrmi_cavgtime_idx);
      jrmi_msg = (JRMIAvgTime*)((char*)msg +g_jrmi_avgtime_idx);
      SET_MIN_MAX_JRMI_CUMULATIVES (end_jrmicavg, jrmi_msg);
      ACC_JRMI_CUMULATIVES (end_jrmicavg, jrmi_msg);
      end_jrmiavg->jrmi_num_connections += jrmi_msg->jrmi_num_connections;
    }

    //XMPP
    if(global_settings->protocol_enabled & XMPP_PROTOCOL_ENABLED) {
      end_xmppcavg = (XMPPCAvgTime*)((char*)save + g_xmpp_cavgtime_idx);
      xmpp_msg = (XMPPAvgTime*)((char*)msg +g_xmpp_avgtime_idx);
      
      ACC_XMPP_CUMULATIVES (end_xmppcavg, xmpp_msg);

      NSDL2_MESSAGES(NULL, NULL, "group = %d, xmpp_msg->xmpp_msg_sent = %d, end_xmppcavg->xmpp_msg_sent = %d", 
                                  j, xmpp_msg->xmpp_msg_sent, end_xmppcavg->xmpp_c_msg_sent);

    }

   //FC2
    if(global_settings->protocol_enabled & FC2_PROTOCOL_ENABLED) {
      end_fc2cavg = (FC2CAvgTime*)((char*)save + g_fc2_cavgtime_idx);
      fc2_msg = (FC2AvgTime*)((char*)msg +g_fc2_avgtime_idx);
      ACC_FC2_CUMULATIVES (end_fc2cavg, fc2_msg);
    }

    end_avg->num_connections += msg->num_connections;
    end_avg->smtp_num_connections += msg->smtp_num_connections;
    end_avg->pop3_num_connections += msg->pop3_num_connections;

    end_avg->cur_vusers_active += msg->cur_vusers_active;
    end_avg->cur_vusers_thinking += msg->cur_vusers_thinking;
    end_avg->cur_vusers_waiting += msg->cur_vusers_waiting;
    end_avg->cur_vusers_cleanup += msg->cur_vusers_cleanup;
    end_avg->cur_vusers_in_sp += msg->cur_vusers_in_sp;
    end_avg->cur_vusers_blocked += msg->cur_vusers_blocked;
    end_avg->cur_vusers_paused += msg->cur_vusers_paused;
    end_avg->total_cum_user_ms += msg->total_cum_user_ms;
    end_avg->running_users += msg->running_users;

    save->c_tot_total_bytes += msg->total_bytes;
    save->c_tot_tx_bytes += msg->tx_bytes;
    save->c_tot_rx_bytes += msg->rx_bytes;
    
 
  }
}

/*************************************************************************************
   Purpose: Handle received progress and finish report
   Input:   amsg          -| child avgtime
            gen_idx       -| -1 for NS
                             >-1 for NC
            finish_report -| flag to handle finish report
                           | 0 -> progress_report
                           | 1 -> finish_report
   
   Output:  dest_avg      -| array of avgtime pointers
            g_cur_finised -| array of cavgtime pointers

   Description: This function updates parent avgtime from child avgtime
                accumulates child data when a child sends progress/
                finished report. 

                In finish report, g_end_avg is updated which is
                summarized avgtime of the test.

   Progress_pool             mpool                      array_of_ptrs
    _______                 ______                       _____
   |Overall|  ------------>|      |s1------------------>| cur |_____________________
    _______                 ______                       _avg_                      |
   |  G1   |  ______       |      |s2________________________     ______            |     
    _______         |       ______    ______                 |-->| Next |_____      |
   |  G2   |  ____  |--------------->|      |s1                   __avg__     |     |
    _______       |    ______         ______                                  |     |
                  |-->|      |s1     |      |s2                               |     |
                       ______         ______                                  |     |
                      |      |s2                              g_next_avg      |     |
                       ______                                   ____ <________|     |
                                                              0|    |               |
                                                                ____                |
                                                              1|    |               |
                                                                ____                |
                                                              2|    |               |
                                                                ____                |
                                                                                    |
                                                     g_cur_avg                      |
                                                      _____  <______________________|
                                                    0|     |
                                                      _____
                                                    1|     |
                                                      _____
                                                    2|     |
                                                      _____

******************************************************************************************/
void handle_rcvd_report(avgtime *amsg, int gen_idx, int finish_report)
{
  int num_rcd;
  int max_queue_size = global_settings->progress_report_queue_size;
  ProgressMsg *dest_msg = NULL;
  NSDL1_MESSAGES(NULL, NULL, "Method called. gen_idx = %d, finish_report = %d, rcvd_sample = %d, cur_sample = %d",
                            gen_idx, finish_report, amsg->elapsed, cur_sample);

  if(loader_opcode == CLIENT_LOADER)
    max_queue_size = global_settings->progress_report_queue_size/2;
  /**********************************************************************************************
    Handle received sample -
    1. If received sample id is older than current sample id
       then Ignore that sample.
       Note: If its condition comes, there is somthing seriously 
        going wrong either on Network or Machine, please rectify
        the issue. 
    2. If received sample id lies on interval 
       (current sapme id, current sample id + progress_report_queue_size)
       then 
       (i) update sample info table
       (ii) copy received sample into progress report queue   
    3. If received sample id is greater than current sample id + progress_report_queue_size
       then 
       (i) forcefully complete either current sample or all old samples
       (ii) update sample info table 
       (iii) copy received sample into progress report queue   
    4. For case 2 and 3 , check which sample ids has been comes from all the generators/child
       and delivery all those.
   ***********************************************************************************************/
  if (!finish_report && amsg->elapsed < cur_sample)
  {
    NSTL1(NULL, NULL, "Ignoring the progress report# %d received from child/generator id:%d name:%s as current report# is %d", 
                       amsg->elapsed, msg_dh->top.internal.child_id,
                       (loader_opcode == MASTER_LOADER)?(char *)generator_entry[msg_dh->top.internal.child_id].gen_name:"NVM", cur_sample);
    return;
  }
  /*********************************************************************************
    If received sample is newer than last queued samples then forcefully
    deliver first queued sample (i.e. current sample) and queued this received
    sample 
    Eg:
    Suppose we have 3 NVMs (say NMV0, NVM1, NVM2) and one NVM (NVM0) is 
    running delayed w.r.t others (i.e. NVM1 and NVM2). 
    NVM0 not send any sample (Why??)
    NVM1 and NVM2 send their 10 samples. 

    Case 1: NVM0 may be busy and NVM1 or NMV2 their 10 + 1 sample
      In this case we need to forcefully deliver FIRST SAMPLE of all the NVMs 
      so that we can store this newly received sample

    Case 2: NVM0 may be busy but died when NVM1 or NVM2 send their 10 + 1 sample
       In this case if we need to deliver all the queued samples   
  ********************************************************************************/
  if(amsg->elapsed >= (cur_sample + max_queue_size))
  { 
    NSTL1(NULL, NULL, "Moving on to next sample as current report# is %d and received report# is %d from child/generator id:%d name:%s", 
                       cur_sample, amsg->elapsed, msg_dh->top.internal.child_id,
                       (loader_opcode == MASTER_LOADER)?(char *)generator_entry[msg_dh->top.internal.child_id].gen_name:"NVM"); 
    num_rcd = get_sample_info(cur_sample);
    HANDLE_FORCE_COMPLETE(cur_sample, num_rcd);
  }
  /********************************************************************************
   Get container from the slot and process data
   If received sample is newer than queued sample and memory pool is full 
   then ProgressMsg instance is NULL then we have forcefully dump the current
   sample and get a slot for updating newer sample
   This case is only possible when 
   amsg->elapsed >= (cur_sample + global_settings->progress_report_queue_size))
    
  *********************************************************************************/
  if(!finish_report)
  {
    /*Get Avg for Overall*/
    dest_msg = (ProgressMsg*)get_avg_from_progress_data_pool(0, amsg->elapsed);
    if(!dest_msg)
    {
        NSTL1(NULL, NULL, "Error: overall avg can't be null");
        return;
    }
    g_dest_avg[0] = (avgtime *)dest_msg->sample_data;
    g_dest_cavg[0] = (cavgtime *)dest_msg->sample_cum_data;
  
    /*Get Avg for Generator*/
    if(gen_idx != -1)
    {
      dest_msg = (ProgressMsg*)get_avg_from_progress_data_pool(gen_idx + 1, amsg->elapsed);
      if(!dest_msg)
      {
        NSTL1(NULL, NULL, "Error: generator avg can't be null", gen_idx);
        return;
      }
      g_dest_avg[gen_idx + 1] = (avgtime *)dest_msg->sample_data;
      g_dest_cavg[gen_idx + 1] = (cavgtime *)dest_msg->sample_cum_data;
    }
 
    /******************************************************************************
     Update sample info table
     1. If same sample comes from different NVMs/Generators then update sample_count in 
        progress_info 
     2. Maintains sample count for a list of samples (list == progress_report_queue_size)
        For e.g NVM 0 send sample 100, then before its completion sends 101
     *****************************************************************************/
    num_rcd = update_sample_info(amsg->elapsed, amsg->child_id);
    if(num_rcd < 0)
    {
      NSTL1(NULL, NULL, "Error: no space left in sample info table", gen_idx);
      return;
    }
    copy_progress_data(g_dest_avg, amsg, g_dest_cavg, gen_idx);
    /* Check if sample received from all child then deliver otherwise collect*/
    HANDLE_IF_COMPLETE(amsg->elapsed, num_rcd);
  }
  else
  { 
    copy_end_data (g_end_avg, amsg, g_cur_finished, gen_idx);
    int tmp_cur_sample = cur_sample;
    while(tmp_cur_sample <= amsg->elapsed)
    {
      decrease_sample_count(amsg->child_id, tmp_cur_sample);
      tmp_cur_sample++;
    }
  }
}

void reset_avgtime()
{
   int i,j;

   // initialize cur_sample each time 
   cur_sample = 1;

   for(i = 0; i < (sgrp_used_genrator_entries + 1); i++)
   {
      //initialize all avgtime 
      for(j = 0; j < TOTAL_GRP_ENTERIES_WITH_GRP_KW; j++)
      {
        GET_AVG_STRUCT(g_cur_avg[i], j);
        init_avgtime (tmp_reset_avg, j);
        tmp_reset_avg->opcode = PROGRESS_REPORT;
        tmp_reset_avg->elapsed = cur_sample; //add cur_sample to print

        GET_AVG_STRUCT(g_next_avg[i], j);
        init_avgtime (tmp_reset_avg, j);
        tmp_reset_avg->opcode = PROGRESS_REPORT;
        tmp_reset_avg->elapsed = cur_sample + 1;

        GET_AVG_STRUCT(g_end_avg[i], j);
        init_avgtime (tmp_reset_avg, j);

        GET_CAVG_STRUCT(g_cur_finished[i], j);
        init_cavgtime (tmp_reset_cavg, j);

        GET_CAVG_STRUCT(g_next_finished[i], j);
        init_cavgtime (tmp_reset_cavg, j);
    }
  }
}
