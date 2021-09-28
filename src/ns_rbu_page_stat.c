/********************************************************************************
 * File Name            : ns_rbu_page_stat.c
 * Author(s)            : Manish Mishra, Shikha
 * Date                 : 19 September 2015
 * Copyright            : (c) Cavisson Systems
 * Purpose              : Contains function related to RBU page stat data graphs,
 *                        Do the gdf functionality.
 * Modification History : <Author(s)>, <Date>, <Change Description/Location>
 ********************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/times.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>

#include <regex.h>
#include <libgen.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "nslb_util.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"

#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "user_tables.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "url.h"
#include "util.h"
#include "timing.h"
#include "tmr.h"
#include "amf.h"
#include "ns_trans_parse.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"
#include "netomni/src/core/ni_scenario_distribution.h"

#include "netstorm.h"
#include "runlogic.h"
#include "ns_gdf.h"
#include "ns_vars.h"
#include "ns_log.h"
#include "ns_cookie.h"
#include "ns_user_monitor.h"
#include "ns_alloc.h"
#include "ns_percentile.h"
#include "ns_parse_scen_conf.h"
#include "ns_server_admin_utils.h"
#include "ns_error_codes.h"
#include "ns_page.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "ns_ftp_parse.h"
#include "ns_ftp.h"
#include "ns_smtp_parse.h"
#include "ns_script_parse.h"
#include "ns_vuser.h"
#include "ns_group_data.h"
#include "ns_rbu_page_stat.h"
#include "ns_test_gdf.h"
#include "ns_rbu_api.h"
#include "deliver_report.h"
#include "output.h"

RBU_Page_Stat_data_gp *rbu_page_stat_data_gp_ptr;

#ifndef CAV_MAIN
unsigned int rbu_page_stat_data_gp_idx = -1;
RBUPageStatAvgTime *rbu_page_stat_avg;
#else
__thread unsigned int rbu_page_stat_data_gp_idx = -1;
__thread RBUPageStatAvgTime *rbu_page_stat_avg;
#endif
unsigned int rbu_page_stat_data_idx = -1;
unsigned int rbu_resp_attr_idx = -1;

// Print only Scenario Group Name as vector lines in o/p file
char **printRBUPageStat()
{
  int i = 0, j = 0, k = 0;
  char **TwoD;
  char buff[1024], buffer[2048];
  TwoD = init_2d(g_rbu_num_pages * (sgrp_used_genrator_entries + 1));
  int write_idx = 0, idx = 0;
  int TwoD_id = 0;
  char *write_ptr = NULL;
  char prefix[1024];
      
  NSDL2_MISC(NULL, NULL, "Method Called, total_sess_entries = %d, total_runprof_entries = %d, g_actual_num_pages = %d, "
                         "total_page_entries = %d",
                          total_sess_entries, total_runprof_entries, g_actual_num_pages, total_page_entries);
      
  for(k=0; k < sgrp_used_genrator_entries + 1; k++)
  { 
    for(j = 0; j < total_runprof_entries; j++)
    { 
       NSDL2_MISC(NULL, NULL, "j = %d, enable_rbu = %d", j, runProfTable[j].gset.rbu_gset.enable_rbu);

       if(!runProfTable[j].gset.rbu_gset.enable_rbu)
         continue;
      
       write_idx = 0;
       write_idx += sprintf(buff, "%s>", RETRIEVE_BUFFER_DATA(runProfTable[j].scen_group_name));
       idx = write_idx;
      
       write_ptr = buff + write_idx;
    
       NSDL2_MISC(NULL, NULL, "write_idx = %d", write_idx);
      
       for(i = 0; i < runProfTable[j].num_pages; i++)
       {
          write_idx = idx;
          write_idx += sprintf(write_ptr, "%s", RETRIEVE_BUFFER_DATA(gPageTable[gSessionTable[runProfTable[j].sessprof_idx].first_page + i].page_name));
          buff[write_idx] = '\0';
          getNCPrefix(prefix, k-1, -1, ">", 0);
          sprintf(buffer, "%s%s", prefix, buff);

          NSDL2_MISC(NULL, NULL, "buffer = %s", buffer);
          fprintf(write_gdf_fp, "%s\n", buffer);
          fill_2d(TwoD, TwoD_id++, buff);
       }
    }
  }

  return TwoD;
}

// Called by ns_parent.c to update group data size into g_avgtime_size
inline void update_rbu_page_stat_data_avgtime_size() 
{
  NSDL2_RBU(NULL, NULL, "Method Called, g_avgtime_size = %d, rbu_page_stat_data_idx = %d",
                         g_avgtime_size, rbu_page_stat_data_idx);
  
  if(global_settings->browser_used != -1)
  {
    NSDL2_RBU(NULL, NULL, "RBU is enabled, g_avgtime_size = %d", g_avgtime_size);
    rbu_page_stat_data_gp_idx = g_avgtime_size;
    g_avgtime_size += RBU_PAGE_STAT_SIZE;
  } else {
    NSDL2_RBU(NULL, NULL, "RBU is disabled.");
  }
  
  NSDL2_RBU(NULL, NULL, "After g_avgtime_size = %d, rbu_page_stat_data_idx = %d",
                  g_avgtime_size, rbu_page_stat_data_idx);
}

// Called by child
inline void set_rbu_page_stat_data_avgtime_ptr() 
{
  NSDL2_RBU(NULL, NULL, "Method Called");

  if(global_settings->browser_used != -1)
  {
    rbu_page_stat_avg = (RBUPageStatAvgTime*)((char *)average_time + rbu_page_stat_data_gp_idx);
    NSDL2_RBU(NULL, NULL, "RBU is enabled, rbu_page_stat_avg = %p, rbu_page_stat_data_gp_idx = %d", rbu_page_stat_avg, rbu_page_stat_data_gp_idx);
  } else {
    NSDL2_RBU(NULL, NULL, "RBU is disabled.");
    rbu_page_stat_avg = NULL;
  }
  NSDL2_RBU(NULL, NULL, "rbu_page_stat_avg = %p", rbu_page_stat_avg);
}

//This Function will store data of structure RBUPageStatAvgTime from RBU_RespAttr
inline void set_rbu_page_stat_data_avgtime_data(VUser *vptr, RBU_RespAttr *rbu_resp_attr)
{ 
  int page_idx_rbu_avg = -1;
  unsigned long long sum_sqr = 0;
  int src_val = 0;
  // This change is done to call  set_rbu_page_stat_data_avgtime_data from api ns_send_rbu_stats, this api is used by PMS apllication
  // script to update RBU page stats
  if(rbu_resp_attr == NULL) 
  {
    NSDL2_RBU(NULL, NULL,"G_RBU Enabled");
    rbu_resp_attr = vptr->httpData->rbu_resp_attr;
  }

  //RBU_RespAttr *rbu_resp_attr = vptr->httpData->rbu_resp_attr;
  page_idx_rbu_avg = runprof_table_shr_mem[vptr->group_num].start_page_idx + vptr->cur_page->relative_page_idx;
 
  NSDL2_RBU(vptr, NULL, "Method Called, rbu_page_stat_avg = %p, rbu_resp_attr = %p, group_num = %d, start_page_idx = %u, relative_page_idx = %u, "
                        "vptr->cur_page = %p", 
                         rbu_page_stat_avg, rbu_resp_attr, vptr->group_num, runprof_table_shr_mem[vptr->group_num].start_page_idx, 
                         vptr->cur_page->relative_page_idx, vptr->cur_page);

  if(rbu_page_stat_avg == NULL || rbu_resp_attr == NULL || vptr->cur_page == NULL || page_idx_rbu_avg == -1)
    return;

  NSDL4_RBU(vptr, NULL, "RBU_resp_attr data - "
                        "on_content_load_time= %d, on_load_time = %d, page_load_time = %d, _tti_time = %d, _cav_startRender_time = %d, "
                        "visually_complete_time = %d, request_without_cache = %d, request_from_cache = %d, byte_received = %f, "
                        "byte_send = %f, js_size = %f, css_size = %f, img_wgt = %f, pg_wgt = %d, "
                        "pg_avail = %d, dns_time = %f, tcp_time = %f, "
                        "ssl_time = %f, connect_time = %f, wait_time = %f, rcv_time = %f, blckd_time = %f"
                        "tbt = %d, lcp = %d, cls = %f, ",
                         rbu_resp_attr->on_content_load_time, 
                         rbu_resp_attr->on_load_time, 
                         rbu_resp_attr->page_load_time, 
                         rbu_resp_attr->_tti_time, 
                         rbu_resp_attr->_cav_start_render_time, 
                         rbu_resp_attr->_cav_end_render_time,
                         rbu_resp_attr->request_without_cache,
                         rbu_resp_attr->request_from_cache,
                         rbu_resp_attr->byte_rcvd,
                         rbu_resp_attr->byte_send,
                         rbu_resp_attr->resp_js_size,
                         rbu_resp_attr->resp_css_size,
                         rbu_resp_attr->resp_img_size,
                         rbu_resp_attr->pg_wgt,
                         rbu_resp_attr->pg_avail,
                         rbu_resp_attr->dns_time, 
                         rbu_resp_attr->tcp_time,
                         rbu_resp_attr->ssl_time,
                         rbu_resp_attr->connect_time,
                         rbu_resp_attr->wait_time, 
                         rbu_resp_attr->rcv_time,
                         rbu_resp_attr->blckd_time,
                         rbu_resp_attr->total_blocking_time,
                         rbu_resp_attr->largest_contentful_paint,
                         rbu_resp_attr->cum_layout_shift);



  //Storing data from RBU_RespAttr to RBUPageStatAvgTime for GDF 
  //A. For Elements of Time like : DOMContent Loaded Time ,OnLoad Time, etc.
  
  //Fill DOMContent Loaded Time in avg structure
  FILL_RBU_PAGE_STAT_AVG(on_content_load_time, DOMContent_Loaded_time, DOMContent_Loaded_min_time, DOMContent_Loaded_max_time, 
                         DOMContent_Loaded_counts, sum_sqr, DOMContent_Loaded_sum_sqr);

  //Fill Onload Time in avg structure
  FILL_RBU_PAGE_STAT_AVG(on_load_time, OnLoad_time, OnLoad_min_time, OnLoad_max_time, OnLoad_counts, sum_sqr, OnLoad_sum_sqr);

  //Fill PageLoad Time in avg structure
  FILL_RBU_PAGE_STAT_AVG(page_load_time, PageLoad_time, PageLoad_min_time, PageLoad_max_time, PageLoad_counts, sum_sqr, PageLoad_sum_sqr);

  //Fill TTI in avg structure
  FILL_RBU_PAGE_STAT_AVG(_tti_time, TTI_time, TTI_min_time, TTI_max_time, TTI_counts, sum_sqr, TTI_sum_sqr);

  //Fill Start Render in avg structure
  FILL_RBU_PAGE_STAT_AVG(_cav_start_render_time, _cav_startRender_time, _cav_startRender_min_time, _cav_startRender_max_time, 
                         _cav_startRender_counts, sum_sqr, _cav_startRender_sum_sqr);

  //fill visually complete in avg structure
  FILL_RBU_PAGE_STAT_AVG(_cav_end_render_time, visually_complete_time, visually_complete_min_time, visually_complete_max_time, 
                         visually_complete_counts, sum_sqr, visually_complete_sum_sqr);

  //Fill cur_rbu_requests in avg structure
  FILL_RBU_PAGE_STAT_AVG(request_without_cache, cur_rbu_requests, cur_rbu_requests_min, cur_rbu_requests_max, cur_rbu_requests_counts, sum_sqr, cur_rbu_requests_sum_sqr);

  //Fill cur_rbu_requests in avg structure
  FILL_RBU_PAGE_STAT_AVG(request_from_cache, cur_rbu_browser_cache, cur_rbu_browser_cache_min, cur_rbu_browser_cache_max, 
                         cur_rbu_browser_cache_count, sum_sqr, cur_rbu_browser_cache_sum_sqr);

  //Fill cur_rbu_requests in avg structure
  FILL_RBU_PAGE_STAT_AVG(byte_rcvd, cur_rbu_bytes_recieved, cur_rbu_bytes_recieved_min, cur_rbu_bytes_recieved_max, 
                         cur_rbu_bytes_recieved_count, sum_sqr, cur_rbu_bytes_recieved_sum_sqr);

  //Fill cur_rbu_requests in avg structure
  FILL_RBU_PAGE_STAT_AVG(byte_send, cur_rbu_bytes_send, cur_rbu_bytes_send_min, cur_rbu_bytes_send_max, cur_rbu_bytes_send_count, sum_sqr, cur_rbu_bytes_send_sum_sqr);

  //Fill For New Graphs
  FILL_RBU_PAGE_STAT_AVG(pg_wgt, cur_rbu_page_wgt, cur_rbu_page_wgt_min, cur_rbu_page_wgt_max, cur_rbu_page_wgt_count, sum_sqr, cur_rbu_page_wgt_sum_sqr);

  FILL_RBU_PAGE_STAT_AVG(resp_js_size, cur_rbu_js_size, cur_rbu_js_size_min, cur_rbu_js_size_max, cur_rbu_js_count, sum_sqr, cur_rbu_js_sum_sqr);

  FILL_RBU_PAGE_STAT_AVG(resp_css_size, cur_rbu_css_size, cur_rbu_css_size_min, cur_rbu_css_size_max, cur_rbu_css_count, sum_sqr, cur_rbu_css_sum_sqr);

  FILL_RBU_PAGE_STAT_AVG(resp_img_size, cur_rbu_img_wgt, cur_rbu_img_wgt_min, cur_rbu_img_wgt_max, cur_rbu_img_wgt_count, sum_sqr, cur_rbu_img_wgt_sum_sqr);

  FILL_RBU_PAGE_STAT_AVG(dom_element, cur_rbu_dom_element, cur_rbu_dom_element_min, cur_rbu_dom_element_max, cur_rbu_dom_element_count, sum_sqr, cur_rbu_dom_element_sum_sqr);

  FILL_RBU_PAGE_STAT_AVG(pg_speed, cur_rbu_pg_speed, cur_rbu_pg_speed_min, cur_rbu_pg_speed_max, cur_rbu_pg_speed_count, sum_sqr, cur_rbu_pg_speed_sum_sqr);

  FILL_RBU_PAGE_STAT_AVG(akamai_cache, cur_rbu_akamai_cache, cur_rbu_akamai_cache_min, cur_rbu_akamai_cache_max, cur_rbu_akamai_cache_count, sum_sqr, cur_rbu_akamai_cache_sum_sqr);

  FILL_RBU_PAGE_STAT_AVG(main_url_resp_time, cur_rbu_main_url_resp_time, cur_rbu_main_url_resp_time_min, cur_rbu_main_url_resp_time_max, 
                         cur_rbu_main_url_resp_time_count, sum_sqr, cur_rbu_main_url_resp_time_sum_sqr);

  //Calculation for URL timing
  FILL_RBU_PAGE_STAT_AVG_TIMES_GRP(dns_time, dns_time, dns_min_time, dns_max_time, dns_counts);
  FILL_RBU_PAGE_STAT_AVG_TIMES_GRP(tcp_time, tcp_time, tcp_min_time, tcp_max_time, tcp_counts);
  FILL_RBU_PAGE_STAT_AVG_TIMES_GRP(ssl_time, ssl_time, ssl_min_time, ssl_max_time, ssl_counts);
  FILL_RBU_PAGE_STAT_AVG_TIMES_GRP(connect_time, connect_time, connect_min_time, connect_max_time, connect_counts);
  FILL_RBU_PAGE_STAT_AVG_TIMES_GRP(wait_time, wait_time, wait_min_time, wait_max_time, wait_counts);
  FILL_RBU_PAGE_STAT_AVG_TIMES_GRP(rcv_time, rcv_time, rcv_min_time, rcv_max_time, rcv_counts);
  FILL_RBU_PAGE_STAT_AVG_TIMES_GRP(blckd_time, blckd_time, blckd_min_time, blckd_max_time, blckd_counts);
  FILL_RBU_PAGE_STAT_AVG_TIMES_GRP(url_resp_time, url_resp_time, url_resp_min_time, url_resp_max_time, url_resp_counts);

  FILL_RBU_PAGE_STAT_AVG(total_blocking_time, tbt, tbt_min, tbt_max, tbt_counts, sum_sqr, tbt_sum_sqr);
  FILL_RBU_PAGE_STAT_AVG(largest_contentful_paint, lcp, lcp_min, lcp_max, lcp_count, sum_sqr, lcp_sum_sqr);
  FILL_RBU_PAGE_STAT_AVG_FLOAT(cum_layout_shift, cls, cls_min, cls_max, cls_count, sum_sqr, cls_sum_sqr);
  //FILL_RBU_PAGE_STAT_AVG_PG_AVAIL(pg_avail, pg_avail);

  //Macro for Debug
  RBUPageStatAvgTime_Data_DUMP;
}     


inline void set_rbu_page_status_data_avgtime_data(VUser *vptr, RBU_RespAttr *rbu_resp_attr)
{
  NSDL1_RBU(vptr, NULL,"Method Called");

  int page_idx_rbu_avg = -1;
  //int src_val = 0;

  if(rbu_page_stat_avg == NULL || vptr->cur_page == NULL)
    return;

  page_idx_rbu_avg = runprof_table_shr_mem[vptr->group_num].start_page_idx + vptr->cur_page->relative_page_idx;

  if(page_idx_rbu_avg == -1)
    return;

  NSDL2_RBU(vptr, NULL, "rbu_page_stat_avg = %p, rbu_resp_attr = %p, group_num = %d, start_page_idx = %u, relative_page_idx = %u, "
                      "vptr->cur_page = %p",
                       rbu_page_stat_avg, rbu_resp_attr, vptr->group_num, runprof_table_shr_mem[vptr->group_num].start_page_idx,
                       vptr->cur_page->relative_page_idx, vptr->cur_page);

  //Reset Page Status of AVG Structure
  RESET_RBU_PAGE_STATUS_AVG(rbu_page_stat_avg[page_idx_rbu_avg]); 

  if(rbu_resp_attr == NULL)
  {
    NSDL2_RBU(vptr, NULL,"G_RBU Enabled");
    if(vptr->httpData->rbu_resp_attr == NULL)
    {  
      NSDL4_RBU(vptr, NULL,"rbu_resp_attr Not accessible from vptr also");
      switch(vptr->page_status)
      {
        case NS_REQUEST_OK :      
        case NS_REQUEST_2xx:      
             rbu_page_stat_avg[page_idx_rbu_avg].pg_status_2xx = 1; 
             break;
        case NS_REQUEST_1xx: 
             rbu_page_stat_avg[page_idx_rbu_avg].pg_status_1xx = 1; 
             break;
        case NS_REQUEST_3xx: 
             rbu_page_stat_avg[page_idx_rbu_avg].pg_status_3xx = 1;
             break;
        case NS_REQUEST_4xx: 
             rbu_page_stat_avg[page_idx_rbu_avg].pg_status_4xx = 1; 
             break;
        case NS_REQUEST_5xx: 
             rbu_page_stat_avg[page_idx_rbu_avg].pg_status_5xx = 1; 
             break;
        default:
             rbu_page_stat_avg[page_idx_rbu_avg].pg_status_other = 1;
             break;
      }
      //Dump debug
      RBUPageStatAvgTime_Data_DUMP;
      return;
    }
    else
      rbu_resp_attr = vptr->httpData->rbu_resp_attr;
  }

  //Page Availibility graph
  //FILL_RBU_PAGE_STAT_AVG_PG_AVAIL(rbu_resp_attr->pg_status, pg_avail); //bug 83012
  switch(vptr->page_status)
  {
    case NS_REQUEST_OK:
    case NS_REQUEST_1xx:
    case NS_REQUEST_2xx:
    case NS_REQUEST_3xx:
      rbu_page_stat_avg[page_idx_rbu_avg].pg_avail = 1;
      break;
    default:
      rbu_page_stat_avg[page_idx_rbu_avg].pg_avail = 0;
      break;
  }

  //Fill Avg Structure for Page Status
  switch(rbu_resp_attr->pg_status)
  {
    case PG_STATUS_1xx:
      rbu_page_stat_avg[page_idx_rbu_avg].pg_status_1xx = 1;
      break;
    case PG_STATUS_2xx:
      rbu_page_stat_avg[page_idx_rbu_avg].pg_status_2xx = 1;
      break;
    case PG_STATUS_3xx:
      rbu_page_stat_avg[page_idx_rbu_avg].pg_status_3xx = 1;
      break;
    case PG_STATUS_4xx:
      rbu_page_stat_avg[page_idx_rbu_avg].pg_status_4xx = 1;
      break;
    case PG_STATUS_5xx:
      rbu_page_stat_avg[page_idx_rbu_avg].pg_status_5xx = 1;
      break;
    default:
      rbu_page_stat_avg[page_idx_rbu_avg].pg_status_other = 1;
      break;
  }

  //Macro for Debug
  RBUPageStatAvgTime_Data_DUMP;
}
      
inline void set_rbu_page_stat_data_avgtime_data_sess_only(VUser *vptr, RBU_RespAttr *rbu_resp_attr)
{ 
  int page_idx_rbu_avg = -1;
  int src_val = 0;

  // This change is done to call set_rbu_page_stat_data_avgtime_data_sess_only from api ns_send_rbu_stats, this api is used by PMS application
  // script to update RBU page stats
  if(rbu_resp_attr == NULL) 
  {
    NSDL2_RBU(NULL, NULL,"G_RBU Enabled");
    rbu_resp_attr = vptr->httpData->rbu_resp_attr;
  }


  //RBU_RespAttr *rbu_resp_attr = vptr->httpData->rbu_resp_attr;
  page_idx_rbu_avg = runprof_table_shr_mem[vptr->group_num].start_page_idx + vptr->cur_page->relative_page_idx;
 
  NSDL2_RBU(vptr, NULL, "Method Called, rbu_page_stat_avg = %p, rbu_resp_attr = %p, group_num = %d, start_page_idx = %u, relative_page_idx = %u, "
                        "vptr->cur_page = %p", 
                         rbu_page_stat_avg, rbu_resp_attr, vptr->group_num, runprof_table_shr_mem[vptr->group_num].start_page_idx, 
                         vptr->cur_page->relative_page_idx, vptr->cur_page);

  if(rbu_page_stat_avg == NULL || rbu_resp_attr == NULL || vptr->cur_page == NULL || page_idx_rbu_avg == -1)
    return;

  NSDL2_RBU(vptr, NULL, "Before: sess_completed = %d, sess_success = %d", rbu_resp_attr->sess_completed, rbu_resp_attr->sess_success);

  FILL_RBU_PAGE_STAT_AVG_CUM(sess_completed, sess_completed);
  FILL_RBU_PAGE_STAT_AVG_CUM(sess_success, sess_success);

  NSDL2_RBU(vptr, NULL, "After: sess_completed = %d, sess_success = %d", rbu_page_stat_avg[page_idx_rbu_avg].sess_completed, 
                                      rbu_page_stat_avg[page_idx_rbu_avg].sess_success);
}     

// Function for filling the data in the structure of RBUPageStatAvgTime.
inline void fill_rbu_page_stat_gp(avgtime **g_avg)
{
  int i = 0, j = 0, group_id = 0, k = 0, v_idx;
  RBUPageStatAvgTime *rbu_page_stat_avg = NULL;
  avgtime *avg = NULL;
  RBU_Page_Stat_data_gp *rbu_page_stat_data_local_gp_ptr = rbu_page_stat_data_gp_ptr;
 
  NSDL2_RBU(NULL, NULL, "Method Called, rbu_page_stat_avg = %p, rbu_page_stat_data_gp_ptr = %p, rbu_page_stat_data_idx = %d, "
                        "total_runprof_entries = %d, g_rbu_num_pages = %d, sgrp_used_genrator_entries = %d", 
                         rbu_page_stat_avg, rbu_page_stat_data_gp_ptr, rbu_page_stat_data_idx, total_runprof_entries, g_rbu_num_pages,
                         sgrp_used_genrator_entries);

  if(rbu_page_stat_data_gp_ptr == NULL) 
    return;

  for(v_idx = 0; v_idx < sgrp_used_genrator_entries + 1; v_idx++)
  {
    avg = (avgtime *)g_avg[v_idx];
    rbu_page_stat_avg = (RBUPageStatAvgTime *) ((char*) avg + rbu_page_stat_data_gp_idx);
    NSDL2_RBU(NULL, NULL, "v_idx = %d, rbu_page_stat_avg = %p", v_idx, rbu_page_stat_avg);

    //Fill only RBU page data into rtg message
    for(group_id = 0; group_id < total_runprof_entries; group_id++)
    {
      NSDL2_RBU(NULL, NULL, "group_id = %d, enable_rbu = %d", group_id, runprof_table_shr_mem[group_id].gset.rbu_gset.enable_rbu);
  
      if(!runprof_table_shr_mem[group_id].gset.rbu_gset.enable_rbu)
        continue;
  
      for (j = 0, i = runprof_table_shr_mem[group_id].start_page_idx; j < runprof_table_shr_mem[group_id].num_pages; j++, i++, k++) 
      {
        NSDL2_RBU(NULL, NULL, "j = %d, i = %d, DOMContent_Loaded_time = %d, DOMContent_Loaded_min_time = %d, DOMContent_Loaded_max_time = %d, "
                              "DOMContent_Loaded_counts = %d, OnLoad_time = %d, TTI_time = %d,  _cav_startRender_time = %d, "
                              "visually_complete_time = %d", 
                             j, i, rbu_page_stat_avg[i].DOMContent_Loaded_time, 
                             rbu_page_stat_avg[i].DOMContent_Loaded_min_time,
                             rbu_page_stat_avg[i].DOMContent_Loaded_max_time,
                             rbu_page_stat_avg[i].DOMContent_Loaded_counts,
                             rbu_page_stat_avg[i].OnLoad_time, 
                             rbu_page_stat_avg[i].TTI_time, 
                             rbu_page_stat_avg[i]._cav_startRender_time, 
                             rbu_page_stat_avg[i].visually_complete_time);
        NSDL2_RBU(NULL, NULL, "Square Sum  : DOMContent_Loaded_sum_sqr = [%lld], OnLoad_sum_sqr = [%lld], "
                              "PageLoad_sum_sqr = [%lld], TTI_sum_sqr = [%lld], _cav_startRender_sum_sqr  = [%lld], visually_complete_sum_sqr = [%lld],"
                              " cur_rbu_requests_sum_sqr = [%lld], cur_rbu_browser_cache_sum_sqr = [%lld], cur_rbu_bytes_recieved_sum_sqr = [%lld], "
                              " cur_rbu_bytes_send_sum_sqr = [%lld], cur_rbu_page_wgt_sum_sqr = [%lld], cur_rbu_js_sum_sqr = [%lld], cur_rbu_css_sum_sqr = [%lld] "
                              ", cur_rbu_img_wgt_sum_sqr = [%lld], cur_rbu_dom_element_sum_sqr = [%lld], cur_rbu_pg_speed_sum_sqr = [%lld], cur_rbu_akamai_cache_sum_sqr = [%lld]"
                              ", cur_rbu_main_url_resp_time_sum_sqr = [%lld]", 
                              rbu_page_stat_avg[i].DOMContent_Loaded_sum_sqr, 
                              rbu_page_stat_avg[i].OnLoad_sum_sqr,
                              rbu_page_stat_avg[i].PageLoad_sum_sqr,
                              rbu_page_stat_avg[i].TTI_sum_sqr,
                              rbu_page_stat_avg[i]._cav_startRender_sum_sqr,
                              rbu_page_stat_avg[i].visually_complete_sum_sqr,
                              rbu_page_stat_avg[i].cur_rbu_requests_sum_sqr,
                              rbu_page_stat_avg[i].cur_rbu_browser_cache_sum_sqr,
                              rbu_page_stat_avg[i].cur_rbu_bytes_recieved_sum_sqr,
                              rbu_page_stat_avg[i].cur_rbu_bytes_send_sum_sqr,
		              rbu_page_stat_avg[i].cur_rbu_page_wgt_sum_sqr,
			      rbu_page_stat_avg[i].cur_rbu_js_sum_sqr,
			      rbu_page_stat_avg[i].cur_rbu_css_sum_sqr,
			      rbu_page_stat_avg[i].cur_rbu_img_wgt_sum_sqr,
			      rbu_page_stat_avg[i].cur_rbu_dom_element_sum_sqr,
			      rbu_page_stat_avg[i].cur_rbu_pg_speed_sum_sqr,
			      rbu_page_stat_avg[i].cur_rbu_akamai_cache_sum_sqr,
			      rbu_page_stat_avg[i].cur_rbu_main_url_resp_time_sum_sqr);
  
        if(rbu_page_stat_avg[i].DOMContent_Loaded_counts > 0)
        {
          GDF_COPY_TIMES_STD_VECTOR_DATA(rbu_page_stat_data_idx, 0, k, 0, 
                      (double)(((double)rbu_page_stat_avg[i].DOMContent_Loaded_time) / 
                                 ((double)(1000.0 * (double)rbu_page_stat_avg[i].DOMContent_Loaded_counts))),
                      (double)rbu_page_stat_avg[i].DOMContent_Loaded_min_time/1000.0,
                      (double)rbu_page_stat_avg[i].DOMContent_Loaded_max_time/1000.0,
                      rbu_page_stat_avg[i].DOMContent_Loaded_counts,
                      (double)rbu_page_stat_avg[i].DOMContent_Loaded_sum_sqr/1000.0,  //Converting sum_sqr into millisecond
                      rbu_page_stat_data_local_gp_ptr[k].DOMContent_Loaded_time_gp.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].DOMContent_Loaded_time_gp.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].DOMContent_Loaded_time_gp.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].DOMContent_Loaded_time_gp.succ,
                      rbu_page_stat_data_local_gp_ptr[k].DOMContent_Loaded_time_gp.sum_of_sqr);
        }
        else
        {
          GDF_COPY_TIMES_STD_VECTOR_DATA(rbu_page_stat_data_idx, 0, k, 0,
                    0,
                    -1,
                    0,
                    0,
		    0,
                    rbu_page_stat_data_local_gp_ptr[k].DOMContent_Loaded_time_gp.avg_time,
                    rbu_page_stat_data_local_gp_ptr[k].DOMContent_Loaded_time_gp.min_time,
                    rbu_page_stat_data_local_gp_ptr[k].DOMContent_Loaded_time_gp.max_time,
                    rbu_page_stat_data_local_gp_ptr[k].DOMContent_Loaded_time_gp.succ,
		    rbu_page_stat_data_local_gp_ptr[k].DOMContent_Loaded_time_gp.sum_of_sqr);
        }
  
        NSDL2_RBU(NULL, NULL, "rbu_page_stat_data_idx = %d, i = %d, DOMContent_Loaded_time = %d, DOMContent_Loaded_counts = %d, DOMContent_Loaded_min_time = %d, DOMContent_Loaded_max_time = %d, avg_time = %f, min_time = %f, max_time = %f, succ = %f", rbu_page_stat_data_idx, i, rbu_page_stat_avg[i].DOMContent_Loaded_time, rbu_page_stat_avg[i].DOMContent_Loaded_counts, rbu_page_stat_avg[i].DOMContent_Loaded_min_time, rbu_page_stat_avg[i].DOMContent_Loaded_max_time, rbu_page_stat_data_local_gp_ptr[k].DOMContent_Loaded_time_gp.avg_time,  rbu_page_stat_data_local_gp_ptr[k].DOMContent_Loaded_time_gp.min_time, rbu_page_stat_data_local_gp_ptr[k].DOMContent_Loaded_time_gp.max_time, rbu_page_stat_data_local_gp_ptr[k].DOMContent_Loaded_time_gp.succ);
      
        if(rbu_page_stat_avg[i].OnLoad_counts > 0)
        {
          GDF_COPY_TIMES_STD_VECTOR_DATA(rbu_page_stat_data_idx, 1, k, 0,
                      (double)(((double)rbu_page_stat_avg[i].OnLoad_time) / 
                                 ((double)(1000.0*(double)rbu_page_stat_avg[i].OnLoad_counts))),
                      (double)rbu_page_stat_avg[i].OnLoad_min_time/1000.0,
                      (double)rbu_page_stat_avg[i].OnLoad_max_time/1000.0,
                      rbu_page_stat_avg[i].OnLoad_counts,
		      rbu_page_stat_avg[i].OnLoad_sum_sqr/1000.0,                     //Converting sum_sqr into millisecond 
                      rbu_page_stat_data_local_gp_ptr[k].OnLoad_time_gp.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].OnLoad_time_gp.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].OnLoad_time_gp.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].OnLoad_time_gp.succ,
		      rbu_page_stat_data_local_gp_ptr[k].OnLoad_time_gp.sum_of_sqr);
        }
        else
        {
          GDF_COPY_TIMES_STD_VECTOR_DATA(rbu_page_stat_data_idx, 1, k, 0,
                      0,
                      -1,
                      0,
                      0,
		      0,
                      rbu_page_stat_data_local_gp_ptr[k].OnLoad_time_gp.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].OnLoad_time_gp.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].OnLoad_time_gp.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].OnLoad_time_gp.succ,
		      rbu_page_stat_data_local_gp_ptr[k].OnLoad_time_gp.sum_of_sqr);
        }
  
        NSDL2_RBU(NULL, NULL, "Group idx = %d, rbu_page_stat_data_idx = %d, OnLoad_time = %d, OnLoad_counts = %d, OnLoad_min_time = %d, OnLoad_max_time = %d, avg_time = %f, min_time = %f, max_time = %f, succ = %f",i, rbu_page_stat_data_idx, rbu_page_stat_avg[i].OnLoad_time, rbu_page_stat_avg[i].OnLoad_counts, rbu_page_stat_avg[i].OnLoad_min_time, rbu_page_stat_avg[i].OnLoad_max_time, rbu_page_stat_data_local_gp_ptr[k].OnLoad_time_gp.avg_time,  rbu_page_stat_data_local_gp_ptr[k].OnLoad_time_gp.min_time, rbu_page_stat_data_local_gp_ptr[k].OnLoad_time_gp.max_time, rbu_page_stat_data_local_gp_ptr[k].OnLoad_time_gp.succ);
  
        if(rbu_page_stat_avg[i].PageLoad_counts > 0)
        {
          GDF_COPY_TIMES_STD_VECTOR_DATA(rbu_page_stat_data_idx, 2, k, 0,
                      (double)(((double)rbu_page_stat_avg[i].PageLoad_time) / 
                                 ((double)(1000.0*(double)rbu_page_stat_avg[i].PageLoad_counts))),
                      (double)rbu_page_stat_avg[i].PageLoad_min_time/1000.0,
                      (double)rbu_page_stat_avg[i].PageLoad_max_time/1000.0,
                      rbu_page_stat_avg[i].PageLoad_counts,
		      rbu_page_stat_avg[i].PageLoad_sum_sqr/1000.0,                   //Converting sum_sqr into millisecond
                      rbu_page_stat_data_local_gp_ptr[k].PageLoad_time_gp.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].PageLoad_time_gp.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].PageLoad_time_gp.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].PageLoad_time_gp.succ,
		      rbu_page_stat_data_local_gp_ptr[k].PageLoad_time_gp.sum_of_sqr);
        }
        else
        {
          GDF_COPY_TIMES_STD_VECTOR_DATA(rbu_page_stat_data_idx, 2, k, 0,
                      0,
                      -1,
                      0,
                      0,
		      0,
                      rbu_page_stat_data_local_gp_ptr[k].PageLoad_time_gp.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].PageLoad_time_gp.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].PageLoad_time_gp.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].PageLoad_time_gp.succ,
		      rbu_page_stat_data_local_gp_ptr[k].PageLoad_time_gp.sum_of_sqr);
        }
  
        NSDL2_RBU(NULL, NULL, "Group idx = %d, rbu_page_stat_data_idx = %d, PageLoad_time = %d, PageLoad_counts = %d, PageLoad_min_time = %d, PageLoad_max_time = %d, avg_time = %f, min_time = %f, max_time = %f, succ = %f",i, rbu_page_stat_data_idx, rbu_page_stat_avg[i].PageLoad_time, rbu_page_stat_avg[i].PageLoad_counts, rbu_page_stat_avg[i].PageLoad_min_time, rbu_page_stat_avg[i].PageLoad_max_time, rbu_page_stat_data_local_gp_ptr[k].PageLoad_time_gp.avg_time, rbu_page_stat_data_local_gp_ptr[k].PageLoad_time_gp.min_time, rbu_page_stat_data_local_gp_ptr[k].PageLoad_time_gp.max_time, rbu_page_stat_data_local_gp_ptr[k].PageLoad_time_gp.succ);
  
  
        if(rbu_page_stat_avg[i].TTI_counts > 0)
        {
          GDF_COPY_TIMES_STD_VECTOR_DATA(rbu_page_stat_data_idx,3, k, 0,
                      (double)(((double)rbu_page_stat_avg[i].TTI_time) / 
                                 ((double)(1000.0*(double)rbu_page_stat_avg[i].TTI_counts))), 
                      (double)rbu_page_stat_avg[i].TTI_min_time/1000.0,
                      (double)rbu_page_stat_avg[i].TTI_max_time/1000.0,
                      rbu_page_stat_avg[i].TTI_counts,
		      rbu_page_stat_avg[i].TTI_sum_sqr/1000.0,                        //Converting sum_sqr into millisecond
                      rbu_page_stat_data_local_gp_ptr[k].TTI_time_gp.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].TTI_time_gp.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].TTI_time_gp.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].TTI_time_gp.succ,
		      rbu_page_stat_data_local_gp_ptr[k].TTI_time_gp.sum_of_sqr);
        }
        else
        {
          GDF_COPY_TIMES_STD_VECTOR_DATA(rbu_page_stat_data_idx, 3, k, 0,
                      0,
                      -1,
                      0,
                      0,
		      0,
                      rbu_page_stat_data_local_gp_ptr[k].TTI_time_gp.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].TTI_time_gp.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].TTI_time_gp.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].TTI_time_gp.succ,
		      rbu_page_stat_data_local_gp_ptr[k].TTI_time_gp.sum_of_sqr);
        }
  
        NSDL2_RBU(NULL, NULL, "Group idx = %d, rbu_page_stat_data_idx = %d, TTI_time = %d, TTI_counts = %d, TTI_min_time = %d, TTI_max_time = %d, avg_time = %f, min_time = %f, max_time = %f, succ = %f",i, rbu_page_stat_data_idx, rbu_page_stat_avg[i].TTI_time, rbu_page_stat_avg[i].TTI_counts, rbu_page_stat_avg[i].TTI_min_time, rbu_page_stat_avg[i].TTI_max_time, rbu_page_stat_data_local_gp_ptr[k].TTI_time_gp.avg_time, rbu_page_stat_data_local_gp_ptr[k].TTI_time_gp.min_time, rbu_page_stat_data_local_gp_ptr[k].TTI_time_gp.max_time, rbu_page_stat_data_local_gp_ptr[k].TTI_time_gp.succ);
  
        if(rbu_page_stat_avg[i]._cav_startRender_counts > 0)
        {
          GDF_COPY_TIMES_STD_VECTOR_DATA(rbu_page_stat_data_idx,4, k, 0,
                      (double)(((double)rbu_page_stat_avg[i]._cav_startRender_time) / 
                                 ((double)(1000.0*(double)rbu_page_stat_avg[i]._cav_startRender_counts))), 
                      (double)rbu_page_stat_avg[i]._cav_startRender_min_time/1000.0,
                      (double)rbu_page_stat_avg[i]._cav_startRender_max_time/1000.0,
                      rbu_page_stat_avg[i]._cav_startRender_counts,
		      rbu_page_stat_avg[i]._cav_startRender_sum_sqr/1000.0,           //Converting sum_sqr into millisecond 
                      rbu_page_stat_data_local_gp_ptr[k]._cav_startRender_time_gp.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k]._cav_startRender_time_gp.min_time,
                      rbu_page_stat_data_local_gp_ptr[k]._cav_startRender_time_gp.max_time,
                      rbu_page_stat_data_local_gp_ptr[k]._cav_startRender_time_gp.succ,
		      rbu_page_stat_data_local_gp_ptr[k]._cav_startRender_time_gp.sum_of_sqr);
        }
        else
        {
          GDF_COPY_TIMES_STD_VECTOR_DATA(rbu_page_stat_data_idx, 4, k, 0,
                      0,
                      -1,
                      0,
                      0,
		      0,
                      rbu_page_stat_data_local_gp_ptr[k]._cav_startRender_time_gp.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k]._cav_startRender_time_gp.min_time,
                      rbu_page_stat_data_local_gp_ptr[k]._cav_startRender_time_gp.max_time,
                      rbu_page_stat_data_local_gp_ptr[k]._cav_startRender_time_gp.succ,
		      rbu_page_stat_data_local_gp_ptr[k]._cav_startRender_time_gp.sum_of_sqr);
        }
  
        NSDL2_RBU(NULL, NULL, "Group idx = %d, rbu_page_stat_data_idx = %d, _cav_startRender_time = %d, _cav_startRender_counts = %d, _cav_startRender_min_time = %d, _cav_startRender_max_time = %d, avg_time = %f, min_time = %f, max_time = %f, succ = %f",i, rbu_page_stat_data_idx, rbu_page_stat_avg[i]._cav_startRender_time, rbu_page_stat_avg[i]._cav_startRender_counts, rbu_page_stat_avg[i]._cav_startRender_min_time, rbu_page_stat_avg[i]._cav_startRender_max_time, rbu_page_stat_data_local_gp_ptr[k]._cav_startRender_time_gp.avg_time, rbu_page_stat_data_local_gp_ptr[k]._cav_startRender_time_gp.min_time, rbu_page_stat_data_local_gp_ptr[k]._cav_startRender_time_gp.max_time, rbu_page_stat_data_local_gp_ptr[k]._cav_startRender_time_gp.succ);
  
        if(rbu_page_stat_avg[i].visually_complete_counts > 0)
        {
          GDF_COPY_TIMES_STD_VECTOR_DATA(rbu_page_stat_data_idx, 5, k, 0,
                      (double)(((double)rbu_page_stat_avg[i].visually_complete_time) / 
                                 ((double)(1000.0*(double)rbu_page_stat_avg[i].visually_complete_counts))),
                      (double)rbu_page_stat_avg[i].visually_complete_min_time/1000.0,
                      (double)rbu_page_stat_avg[i].visually_complete_max_time/1000.0,
                      rbu_page_stat_avg[i].visually_complete_counts,
		      rbu_page_stat_avg[i].visually_complete_sum_sqr/1000.0,          //Converting sum_sqr into millisecond
                      rbu_page_stat_data_local_gp_ptr[k].visually_complete_time_gp.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].visually_complete_time_gp.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].visually_complete_time_gp.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].visually_complete_time_gp.succ,
		      rbu_page_stat_data_local_gp_ptr[k].visually_complete_time_gp.sum_of_sqr);
        }
        else
        {
          GDF_COPY_TIMES_STD_VECTOR_DATA(rbu_page_stat_data_idx, 5, k, 0,
                      0,
                      -1,
                      0,
                      0,
		      0,
                      rbu_page_stat_data_local_gp_ptr[k].visually_complete_time_gp.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].visually_complete_time_gp.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].visually_complete_time_gp.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].visually_complete_time_gp.succ,
		      rbu_page_stat_data_local_gp_ptr[k].visually_complete_time_gp.sum_of_sqr);
        }
  
        NSDL2_RBU(NULL, NULL, "Group idx = %d, rbu_page_stat_data_idx = %d, visually_complete_time = %d, visually_complete_counts = %d, visually_complete_min_time = %d, visually_complete_max_time = %d, avg_time = %f, min_time = %f, max_time = %f, succ = %f",i, rbu_page_stat_data_idx, rbu_page_stat_avg[i].visually_complete_time, rbu_page_stat_avg[i].visually_complete_counts, rbu_page_stat_avg[i].visually_complete_min_time, rbu_page_stat_avg[i].visually_complete_max_time, rbu_page_stat_data_local_gp_ptr[k].visually_complete_time_gp.avg_time, rbu_page_stat_data_local_gp_ptr[k].visually_complete_time_gp.min_time, rbu_page_stat_data_local_gp_ptr[k].visually_complete_time_gp.max_time, rbu_page_stat_data_local_gp_ptr[k].visually_complete_time_gp.succ);
  
        if(rbu_page_stat_avg[i].cur_rbu_requests_counts > 0)
        {
          GDF_COPY_TIMES_STD_VECTOR_DATA(rbu_page_stat_data_idx, 6, k, 0,
                      (double)(((double)rbu_page_stat_avg[i].cur_rbu_requests) /
                                 ((double)(1000.0*(double)rbu_page_stat_avg[i].cur_rbu_requests_counts))*1000),
                      (double)rbu_page_stat_avg[i].cur_rbu_requests_min,
                      (double)rbu_page_stat_avg[i].cur_rbu_requests_max,
                      rbu_page_stat_avg[i].cur_rbu_requests_counts,
		      rbu_page_stat_avg[i].cur_rbu_requests_sum_sqr*1000,             //It is not in millisecond, but when std_dev will be 
                                                                                      //calculated it will be treated as millisecond, 
                                                                                      //hence multiplied by 1000
                      rbu_page_stat_data_local_gp_ptr[k].rbu_requests.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].rbu_requests.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].rbu_requests.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].rbu_requests.succ,
		      rbu_page_stat_data_local_gp_ptr[k].rbu_requests.sum_of_sqr);
        }
        else
        {
          GDF_COPY_TIMES_STD_VECTOR_DATA(rbu_page_stat_data_idx, 6, k, 0,
                      0,
                      -1,
                      0,
                      0,
		      0,
                      rbu_page_stat_data_local_gp_ptr[k].rbu_requests.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].rbu_requests.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].rbu_requests.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].rbu_requests.succ,
		      rbu_page_stat_data_local_gp_ptr[k].rbu_requests.sum_of_sqr);
        }
  
        if(rbu_page_stat_avg[i].cur_rbu_browser_cache_count > 0)
        {
          GDF_COPY_TIMES_STD_VECTOR_DATA(rbu_page_stat_data_idx, 7, k, 0,
                      (double)(((double)rbu_page_stat_avg[i].cur_rbu_browser_cache) /
                                 ((double)(1000.0*(double)rbu_page_stat_avg[i].cur_rbu_browser_cache_count))*1000),
                      (double)rbu_page_stat_avg[i].cur_rbu_browser_cache_min,
                      (double)rbu_page_stat_avg[i].cur_rbu_browser_cache_max,
                      rbu_page_stat_avg[i].cur_rbu_browser_cache_count,
		      rbu_page_stat_avg[i].cur_rbu_browser_cache_sum_sqr*1000.0,      //It is not in millisecond, but when std_dev will be 
                                                                                      //calculated it will be treated as millisecond, 
                                                                                      //hence multiplied by 1000
                      rbu_page_stat_data_local_gp_ptr[k].rbu_browser_cache.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].rbu_browser_cache.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].rbu_browser_cache.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].rbu_browser_cache.succ,
		      rbu_page_stat_data_local_gp_ptr[k].rbu_browser_cache.sum_of_sqr);
        }
        else
        {               
          GDF_COPY_TIMES_STD_VECTOR_DATA(rbu_page_stat_data_idx, 7, k, 0,
                      0,
                      -1,
                      0,
                      0,
		      0,
                      rbu_page_stat_data_local_gp_ptr[k].rbu_browser_cache.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].rbu_browser_cache.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].rbu_browser_cache.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].rbu_browser_cache.succ,
		      rbu_page_stat_data_local_gp_ptr[k].rbu_browser_cache.sum_of_sqr);
        }
  
        if(rbu_page_stat_avg[i].cur_rbu_bytes_recieved_count > 0)
        {
          GDF_COPY_TIMES_STD_VECTOR_DATA(rbu_page_stat_data_idx, 8, k, 0,
                      (double)(((double)rbu_page_stat_avg[i].cur_rbu_bytes_recieved) /
                                 ((double)(1000.0*(double)rbu_page_stat_avg[i].cur_rbu_bytes_recieved_count))*1000),
                      (double)rbu_page_stat_avg[i].cur_rbu_bytes_recieved_min,
                      (double)rbu_page_stat_avg[i].cur_rbu_bytes_recieved_max,
                      rbu_page_stat_avg[i].cur_rbu_bytes_recieved_count,
		      rbu_page_stat_avg[i].cur_rbu_bytes_recieved_sum_sqr*1000.0,     //It is not in millisecond, but when std_dev will be 
                                                                                      //calculated it will be treated as millisecond, 
                                                                                      //hence multiplied by 1000
                      rbu_page_stat_data_local_gp_ptr[k].rbu_bytes_recieved.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].rbu_bytes_recieved.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].rbu_bytes_recieved.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].rbu_bytes_recieved.succ,
		      rbu_page_stat_data_local_gp_ptr[k].rbu_bytes_recieved.sum_of_sqr);
        }
        else
        {
          GDF_COPY_TIMES_STD_VECTOR_DATA(rbu_page_stat_data_idx, 8, k, 0,
                      0,
                      -1,
                      0,
                      0,
		      0,
                      rbu_page_stat_data_local_gp_ptr[k].rbu_bytes_recieved.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].rbu_bytes_recieved.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].rbu_bytes_recieved.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].rbu_bytes_recieved.succ,
		      rbu_page_stat_data_local_gp_ptr[k].rbu_bytes_recieved.sum_of_sqr);
        }
  
        if(rbu_page_stat_avg[i].cur_rbu_bytes_send_count > 0)
        {
          GDF_COPY_TIMES_STD_VECTOR_DATA(rbu_page_stat_data_idx, 9, k, 0,
                      (double)(((double)rbu_page_stat_avg[i].cur_rbu_bytes_send) /
                                 ((double)(1000.0*(double)rbu_page_stat_avg[i].cur_rbu_bytes_send_count))*1000),
                      (double)rbu_page_stat_avg[i].cur_rbu_bytes_send_min,
                      (double)rbu_page_stat_avg[i].cur_rbu_bytes_send_max,
                      rbu_page_stat_avg[i].cur_rbu_bytes_send_count,
		      rbu_page_stat_avg[i].cur_rbu_bytes_send_sum_sqr*1000.0,         //It is not in millisecond, but when std_dev will be 
                                                                                      //calculated it will be treated as millisecond, 
                                                                                      //hence multiplied by 1000
                      rbu_page_stat_data_local_gp_ptr[k].rbu_bytes_send.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].rbu_bytes_send.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].rbu_bytes_send.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].rbu_bytes_send.succ,
		      rbu_page_stat_data_local_gp_ptr[k].rbu_bytes_send.sum_of_sqr);
        }
        else
        {
          GDF_COPY_TIMES_STD_VECTOR_DATA(rbu_page_stat_data_idx, 9, k, 0,
                      0,
                      -1,
                      0,
                      0,
		      0,
                      rbu_page_stat_data_local_gp_ptr[k].rbu_bytes_send.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].rbu_bytes_send.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].rbu_bytes_send.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].rbu_bytes_send.succ,
		      rbu_page_stat_data_local_gp_ptr[k].rbu_bytes_send.sum_of_sqr);
        }
        //New Graphs Added for different parameters coming in HAR
        if(rbu_page_stat_avg[i].cur_rbu_page_wgt_count > 0)
        {
          GDF_COPY_TIMES_STD_VECTOR_DATA(rbu_page_stat_data_idx, 10, k, 0,
                      (double)(((double)rbu_page_stat_avg[i].cur_rbu_page_wgt) /
                                 ((double)(1000.0*(double)rbu_page_stat_avg[i].cur_rbu_page_wgt_count))*1000),
                      (double)rbu_page_stat_avg[i].cur_rbu_page_wgt_min,
                      (double)rbu_page_stat_avg[i].cur_rbu_page_wgt_max,
                      rbu_page_stat_avg[i].cur_rbu_page_wgt_count,
		      rbu_page_stat_avg[i].cur_rbu_page_wgt_sum_sqr*1000.0,           //It is not in millisecond, but when std_dev will be 
                                                                                      //calculated it will be treated as millisecond, 
                                                                                      //hence multiplied by 1000
                      rbu_page_stat_data_local_gp_ptr[k].page_weight_gp.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].page_weight_gp.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].page_weight_gp.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].page_weight_gp.succ,
		      rbu_page_stat_data_local_gp_ptr[k].page_weight_gp.sum_of_sqr);
        }
        else
        {
          GDF_COPY_TIMES_STD_VECTOR_DATA(rbu_page_stat_data_idx, 10, k, 0,
                      0,
                      -1,
                      0,
                      0,
		      0,
                      rbu_page_stat_data_local_gp_ptr[k].page_weight_gp.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].page_weight_gp.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].page_weight_gp.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].page_weight_gp.succ,
		      rbu_page_stat_data_local_gp_ptr[k].page_weight_gp.sum_of_sqr);
        }
        if(rbu_page_stat_avg[i].cur_rbu_js_count > 0)
        {
          GDF_COPY_TIMES_STD_VECTOR_DATA(rbu_page_stat_data_idx, 11, k, 0,
                      (double)(((double)rbu_page_stat_avg[i].cur_rbu_js_size) /
                                 ((double)(1000.0*(double)rbu_page_stat_avg[i].cur_rbu_js_count))*1000),
                      (double)rbu_page_stat_avg[i].cur_rbu_js_size_min,
                      (double)rbu_page_stat_avg[i].cur_rbu_js_size_max,
                      rbu_page_stat_avg[i].cur_rbu_js_count,
		      rbu_page_stat_avg[i].cur_rbu_js_sum_sqr*1000.0,                 //It is not in millisecond, but when std_dev will be 
                                                                                      //calculated it will be treated as millisecond, 
                                                                                      //hence multiplied by 1000
                      rbu_page_stat_data_local_gp_ptr[k].js_size_gp.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].js_size_gp.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].js_size_gp.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].js_size_gp.succ,
		      rbu_page_stat_data_local_gp_ptr[k].js_size_gp.sum_of_sqr);
        }
        else
        {
          GDF_COPY_TIMES_STD_VECTOR_DATA(rbu_page_stat_data_idx, 11, k, 0,
                      0,
                      -1,
                      0,
                      0,
		      0,
                      rbu_page_stat_data_local_gp_ptr[k].js_size_gp.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].js_size_gp.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].js_size_gp.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].js_size_gp.succ,
		      rbu_page_stat_data_local_gp_ptr[k].js_size_gp.sum_of_sqr);
        }
        if(rbu_page_stat_avg[i].cur_rbu_css_count > 0)
        {
          GDF_COPY_TIMES_STD_VECTOR_DATA(rbu_page_stat_data_idx, 12, k, 0,
                      (double)(((double)rbu_page_stat_avg[i].cur_rbu_css_size) /
                                 ((double)(1000.0*(double)rbu_page_stat_avg[i].cur_rbu_css_count))*1000),
                      (double)rbu_page_stat_avg[i].cur_rbu_css_size_min,
                      (double)rbu_page_stat_avg[i].cur_rbu_css_size_max,
                      rbu_page_stat_avg[i].cur_rbu_css_count,
		      rbu_page_stat_avg[i].cur_rbu_css_sum_sqr*1000.0,                //It is not in millisecond, but when std_dev will be 
                                                                                      //calculated it will be treated as millisecond, 
                                                                                      //hence multiplied by 1000
                      rbu_page_stat_data_local_gp_ptr[k].css_size_gp.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].css_size_gp.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].css_size_gp.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].css_size_gp.succ,
		      rbu_page_stat_data_local_gp_ptr[k].css_size_gp.sum_of_sqr);
        }
        else
        {
          GDF_COPY_TIMES_STD_VECTOR_DATA(rbu_page_stat_data_idx, 12, k, 0,
                      0,
                      -1,
                      0,
                      0,
		      0,
                      rbu_page_stat_data_local_gp_ptr[k].css_size_gp.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].css_size_gp.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].css_size_gp.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].css_size_gp.succ,
		      rbu_page_stat_data_local_gp_ptr[k].css_size_gp.sum_of_sqr);
        }
        if(rbu_page_stat_avg[i].cur_rbu_img_wgt_count > 0)
        {
          GDF_COPY_TIMES_STD_VECTOR_DATA(rbu_page_stat_data_idx, 13, k, 0,
                      (double)(((double)rbu_page_stat_avg[i].cur_rbu_img_wgt) /
                                 ((double)(1000.0*(double)rbu_page_stat_avg[i].cur_rbu_img_wgt_count))*1000),
                      (double)rbu_page_stat_avg[i].cur_rbu_img_wgt_min,
                      (double)rbu_page_stat_avg[i].cur_rbu_img_wgt_max,
                      rbu_page_stat_avg[i].cur_rbu_img_wgt_count,
		      rbu_page_stat_avg[i].cur_rbu_img_wgt_sum_sqr*1000.0,            //It is not in millisecond, but when std_dev will be 
                                                                                      //calculated it will be treated as millisecond, 
                                                                                      //hence multiplied by 1000
                      rbu_page_stat_data_local_gp_ptr[k].img_wgt_gp.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].img_wgt_gp.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].img_wgt_gp.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].img_wgt_gp.succ,
		      rbu_page_stat_data_local_gp_ptr[k].img_wgt_gp.sum_of_sqr);
        }
        else
        {
          GDF_COPY_TIMES_STD_VECTOR_DATA(rbu_page_stat_data_idx, 13, k, 0,
                      0,
                      -1,
                      0,
                      0,
		      0,
                      rbu_page_stat_data_local_gp_ptr[k].img_wgt_gp.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].img_wgt_gp.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].img_wgt_gp.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].img_wgt_gp.succ,
		      rbu_page_stat_data_local_gp_ptr[k].img_wgt_gp.sum_of_sqr);
        }
        if(rbu_page_stat_avg[i].cur_rbu_dom_element_count > 0)
        {
          GDF_COPY_TIMES_STD_VECTOR_DATA(rbu_page_stat_data_idx, 14, k, 0,
                      (double)(((double)rbu_page_stat_avg[i].cur_rbu_dom_element) /
                                 ((double)(1000.0*(double)rbu_page_stat_avg[i].cur_rbu_dom_element_count))*1000),
                      (double)rbu_page_stat_avg[i].cur_rbu_dom_element_min,
                      (double)rbu_page_stat_avg[i].cur_rbu_dom_element_max,
                      rbu_page_stat_avg[i].cur_rbu_dom_element_count,
		      rbu_page_stat_avg[i].cur_rbu_dom_element_sum_sqr*1000.0,        //It is not in millisecond, but when std_dev will be 
                                                                                      //calculated it will be treated as millisecond, 
                                                                                      //hence multiplied by 1000
                      rbu_page_stat_data_local_gp_ptr[k].dom_element_gp.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].dom_element_gp.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].dom_element_gp.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].dom_element_gp.succ,
		      rbu_page_stat_data_local_gp_ptr[k].dom_element_gp.sum_of_sqr);
        }
        else
        {
          GDF_COPY_TIMES_STD_VECTOR_DATA(rbu_page_stat_data_idx, 14, k, 0,
                      0,
                      -1,
                      0,
                      0,
		      0,
                      rbu_page_stat_data_local_gp_ptr[k].dom_element_gp.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].dom_element_gp.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].dom_element_gp.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].dom_element_gp.succ,
		      rbu_page_stat_data_local_gp_ptr[k].dom_element_gp.sum_of_sqr);
        }
        if(rbu_page_stat_avg[i].cur_rbu_pg_speed_count > 0)
        {
          GDF_COPY_TIMES_STD_VECTOR_DATA(rbu_page_stat_data_idx, 15, k, 0,
                      (double)(((double)rbu_page_stat_avg[i].cur_rbu_pg_speed) /
                                 ((double)(1000.0*(double)rbu_page_stat_avg[i].cur_rbu_pg_speed_count))*1000),
                      (double)rbu_page_stat_avg[i].cur_rbu_pg_speed_min,
                      (double)rbu_page_stat_avg[i].cur_rbu_pg_speed_max,
                      rbu_page_stat_avg[i].cur_rbu_pg_speed_count,
		      rbu_page_stat_avg[i].cur_rbu_pg_speed_sum_sqr*1000.0,           //It is not in millisecond, but when std_dev will be 
                                                                                      //calculated it will be treated as millisecond, 
                                                                                      //hence multiplied by 1000
                      rbu_page_stat_data_local_gp_ptr[k].pg_speed_gp.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].pg_speed_gp.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].pg_speed_gp.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].pg_speed_gp.succ,
		      rbu_page_stat_data_local_gp_ptr[k].pg_speed_gp.sum_of_sqr);
        }
        else
        {
          GDF_COPY_TIMES_STD_VECTOR_DATA(rbu_page_stat_data_idx, 15, k, 0,
                      0,
                      -1,
                      0,
                      0,
		      0,
                      rbu_page_stat_data_local_gp_ptr[k].pg_speed_gp.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].pg_speed_gp.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].pg_speed_gp.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].pg_speed_gp.succ,
		      rbu_page_stat_data_local_gp_ptr[k].pg_speed_gp.sum_of_sqr);
        }
        if(rbu_page_stat_avg[i].cur_rbu_akamai_cache_count > 0)
        {
          GDF_COPY_TIMES_STD_VECTOR_DATA(rbu_page_stat_data_idx, 16, k, 0,
                      (double)(((double)rbu_page_stat_avg[i].cur_rbu_akamai_cache) /
                                 ((double)(1000.0*(double)rbu_page_stat_avg[i].cur_rbu_akamai_cache_count))*1000),
                      (double)rbu_page_stat_avg[i].cur_rbu_akamai_cache_min,
                      (double)rbu_page_stat_avg[i].cur_rbu_akamai_cache_max,
                      rbu_page_stat_avg[i].cur_rbu_akamai_cache_count,
		      rbu_page_stat_avg[i].cur_rbu_akamai_cache_sum_sqr*1000.0,       //It is not in millisecond, but when std_dev will be 
                                                                                      //calculated it will be treated as millisecond, 
                                                                                      //hence multiplied by 1000
                      rbu_page_stat_data_local_gp_ptr[k].akamai_cache_gp.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].akamai_cache_gp.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].akamai_cache_gp.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].akamai_cache_gp.succ,
		      rbu_page_stat_data_local_gp_ptr[k].akamai_cache_gp.sum_of_sqr);
        }
        else
        {
          GDF_COPY_TIMES_STD_VECTOR_DATA(rbu_page_stat_data_idx, 16, k, 0,
                      0,
                      -1,
                      0,
                      0,
		      0,
                      rbu_page_stat_data_local_gp_ptr[k].akamai_cache_gp.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].akamai_cache_gp.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].akamai_cache_gp.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].akamai_cache_gp.succ,
		      rbu_page_stat_data_local_gp_ptr[k].akamai_cache_gp.sum_of_sqr);
        }

        if(rbu_page_stat_avg[i].cur_rbu_main_url_resp_time_count > 0)
        {
          GDF_COPY_TIMES_STD_VECTOR_DATA(rbu_page_stat_data_idx, 17, k, 0,
                      (double)(((double)rbu_page_stat_avg[i].cur_rbu_main_url_resp_time) /
                                 ((double)(1000.0*(double)rbu_page_stat_avg[i].cur_rbu_main_url_resp_time_count))),
                      (double)rbu_page_stat_avg[i].cur_rbu_main_url_resp_time_min/1000.0,
                      (double)rbu_page_stat_avg[i].cur_rbu_main_url_resp_time_max/1000.0,
                      rbu_page_stat_avg[i].cur_rbu_main_url_resp_time_count,
		      rbu_page_stat_avg[i].cur_rbu_main_url_resp_time_sum_sqr/1000.0, //Converting sum_sqr into millisecond
                      rbu_page_stat_data_local_gp_ptr[k].main_url_resp_time_gp.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].main_url_resp_time_gp.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].main_url_resp_time_gp.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].main_url_resp_time_gp.succ,
		      rbu_page_stat_data_local_gp_ptr[k].main_url_resp_time_gp.sum_of_sqr);
        }
        else
        {
          GDF_COPY_TIMES_STD_VECTOR_DATA(rbu_page_stat_data_idx, 17, k, 0,
                      0,
                      -1,
                      0,
                      0,
		      0,
                      rbu_page_stat_data_local_gp_ptr[k].main_url_resp_time_gp.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].main_url_resp_time_gp.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].main_url_resp_time_gp.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].main_url_resp_time_gp.succ,
		      rbu_page_stat_data_local_gp_ptr[k].main_url_resp_time_gp.sum_of_sqr);
        }

        NSDL2_RBU(NULL, NULL, "Fill Page avalibinlity in rtg - group idx = %d, rbu_page_stat_data_idx = %d, pg_avail = %d",
                               i, rbu_page_stat_data_idx, rbu_page_stat_avg[i].pg_avail); 
  
        GDF_COPY_VECTOR_DATA(rbu_page_stat_data_idx, 18, k, 0, rbu_page_stat_avg[i].pg_avail, rbu_page_stat_data_local_gp_ptr[k].pg_avail); 

        NSDL2_RBU(NULL, NULL, "Fill sess_completed - sess_completed = %d, sess_success = %d", 
                               rbu_page_stat_avg[i].sess_completed, rbu_page_stat_avg[i].sess_success);
        GDF_COPY_VECTOR_DATA(rbu_page_stat_data_idx, 19, k, 0, 
                             rbu_page_stat_avg[i].sess_completed, rbu_page_stat_data_local_gp_ptr[k].sess_completed); 
        GDF_COPY_VECTOR_DATA(rbu_page_stat_data_idx, 20, k, 0, rbu_page_stat_avg[i].sess_success, rbu_page_stat_data_local_gp_ptr[k].sess_success); 
        NSDL2_RBU(NULL, NULL, "AFTER: Fill in grp - sess_completed - sess_completed = %d, sess_success = %d", 
                               rbu_page_stat_data_local_gp_ptr[k].sess_completed, rbu_page_stat_data_local_gp_ptr[k].sess_success);
        
        NSDL2_RBU(NULL, NULL, "Fill Page Status for group idx = %d, rbu_page_stat_data_idx = %d, pg_status_1xx = %d"
                              ", pg_status_2xx = %d,  pg_status_3xx = %d, pg_status_4xx = %d, pg_status_5xx = %d, pg_status_other = %d",
                               i, rbu_page_stat_data_idx, rbu_page_stat_avg[i].pg_status_1xx, rbu_page_stat_avg[i].pg_status_2xx,
                               rbu_page_stat_avg[i].pg_status_3xx, rbu_page_stat_avg[i].pg_status_4xx, rbu_page_stat_avg[i].pg_status_5xx,
                               rbu_page_stat_avg[i].pg_status_other);

        GDF_COPY_VECTOR_DATA(rbu_page_stat_data_idx, 21, k, 0, rbu_page_stat_avg[i].pg_status_1xx, rbu_page_stat_data_local_gp_ptr[k].pg_status_1xx);

        GDF_COPY_VECTOR_DATA(rbu_page_stat_data_idx, 22, k, 0, rbu_page_stat_avg[i].pg_status_2xx, rbu_page_stat_data_local_gp_ptr[k].pg_status_2xx);

        GDF_COPY_VECTOR_DATA(rbu_page_stat_data_idx, 23, k, 0, rbu_page_stat_avg[i].pg_status_3xx, rbu_page_stat_data_local_gp_ptr[k].pg_status_3xx);

        GDF_COPY_VECTOR_DATA(rbu_page_stat_data_idx, 24, k, 0, rbu_page_stat_avg[i].pg_status_4xx, rbu_page_stat_data_local_gp_ptr[k].pg_status_4xx);

        GDF_COPY_VECTOR_DATA(rbu_page_stat_data_idx, 25, k, 0, rbu_page_stat_avg[i].pg_status_5xx, rbu_page_stat_data_local_gp_ptr[k].pg_status_5xx);

        GDF_COPY_VECTOR_DATA(rbu_page_stat_data_idx, 26, k, 0, rbu_page_stat_avg[i].pg_status_other, rbu_page_stat_data_local_gp_ptr[k].pg_status_other);

        NSDL2_RBU(NULL, NULL, "Fill Page Status for timings | dns_time = %f, tcp_time = %f,  ssl_time = %f, connect_time = %f, "
                              "wait_time = %f, rcv_time = %f, blckd_time =%f, url_resp_time = %f",
                               rbu_page_stat_avg[i].dns_time, rbu_page_stat_avg[i].tcp_time, rbu_page_stat_avg[i].ssl_time, 
                               rbu_page_stat_avg[i].connect_time, rbu_page_stat_avg[i].wait_time, rbu_page_stat_avg[i].rcv_time,
                               rbu_page_stat_avg[i].blckd_time, rbu_page_stat_avg[i].url_resp_time);

        // DNS time Graph
        if(rbu_page_stat_avg[i].dns_counts > 0)
        {
          GDF_COPY_TIMES_SCALAR_DATA(rbu_page_stat_data_idx, 27, 
                      (double)(((double)rbu_page_stat_avg[i].dns_time) /
                                 ((double)(1000.0*(double)rbu_page_stat_avg[i].dns_counts))),
                      (double)rbu_page_stat_avg[i].dns_min_time/1000.0,
                      (double)rbu_page_stat_avg[i].dns_max_time/1000.0,
                      rbu_page_stat_avg[i].dns_counts,
                      rbu_page_stat_data_local_gp_ptr[k].dns_time_gp.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].dns_time_gp.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].dns_time_gp.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].dns_time_gp.succ);
        }                        
        else          
        {             
          GDF_COPY_TIMES_SCALAR_DATA(rbu_page_stat_data_idx, 27,
                      0,
                      -1,
                      0,
                      0,
                      rbu_page_stat_data_local_gp_ptr[k].dns_time_gp.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].dns_time_gp.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].dns_time_gp.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].dns_time_gp.succ);
        }

        // tcp time Graph
        if(rbu_page_stat_avg[i].tcp_counts > 0)
        {             
          GDF_COPY_TIMES_SCALAR_DATA(rbu_page_stat_data_idx, 28,
                      (double)(((double)rbu_page_stat_avg[i].tcp_time) /
                                 ((double)(1000.0*(double)rbu_page_stat_avg[i].tcp_counts))),
                      (double)rbu_page_stat_avg[i].tcp_min_time/1000.0,
                      (double)rbu_page_stat_avg[i].tcp_max_time/1000.0,
                      rbu_page_stat_avg[i].tcp_counts,
                      rbu_page_stat_data_local_gp_ptr[k].tcp_time_gp.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].tcp_time_gp.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].tcp_time_gp.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].tcp_time_gp.succ);
        }
        else
        {              
          GDF_COPY_TIMES_SCALAR_DATA(rbu_page_stat_data_idx, 28,
                      0,
                      -1,           
                      0,
                      0,
                      rbu_page_stat_data_local_gp_ptr[k].tcp_time_gp.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].tcp_time_gp.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].tcp_time_gp.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].tcp_time_gp.succ);
        }

        // ssl time Graph
        if(rbu_page_stat_avg[i].ssl_counts > 0)
        {             
          GDF_COPY_TIMES_SCALAR_DATA(rbu_page_stat_data_idx, 29, 
                      (double)(((double)rbu_page_stat_avg[i].ssl_time) /
                                 ((double)(1000.0*(double)rbu_page_stat_avg[i].ssl_counts))),
                      (double)rbu_page_stat_avg[i].ssl_min_time/1000.0,
                      (double)rbu_page_stat_avg[i].ssl_max_time/1000.0,
                      rbu_page_stat_avg[i].ssl_counts,
                      rbu_page_stat_data_local_gp_ptr[k].ssl_time_gp.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].ssl_time_gp.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].ssl_time_gp.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].ssl_time_gp.succ);
        }
        else           
        {              
          GDF_COPY_TIMES_SCALAR_DATA(rbu_page_stat_data_idx, 29,
                      0,
                      -1,
                      0, 
                      0,
                      rbu_page_stat_data_local_gp_ptr[k].ssl_time_gp.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].ssl_time_gp.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].ssl_time_gp.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].ssl_time_gp.succ);
        }

        // connect time Graph
        if(rbu_page_stat_avg[i].connect_counts > 0)
        {             
          GDF_COPY_TIMES_SCALAR_DATA(rbu_page_stat_data_idx, 30, 
                      (double)(((double)rbu_page_stat_avg[i].connect_time) /
                                 ((double)(1000.0*(double)rbu_page_stat_avg[i].connect_counts))),
                      (double)rbu_page_stat_avg[i].connect_min_time/1000.0,
                      (double)rbu_page_stat_avg[i].connect_max_time/1000.0,
                      rbu_page_stat_avg[i].connect_counts,
                      rbu_page_stat_data_local_gp_ptr[k].connect_time_gp.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].connect_time_gp.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].connect_time_gp.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].connect_time_gp.succ);
        }
        else           
        {              
          GDF_COPY_TIMES_SCALAR_DATA(rbu_page_stat_data_idx, 30,
                      0,
                      -1,
                      0, 
                      0,
                      rbu_page_stat_data_local_gp_ptr[k].connect_time_gp.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].connect_time_gp.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].connect_time_gp.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].connect_time_gp.succ);
        }

        // wait time Graph
        if(rbu_page_stat_avg[i].wait_counts > 0) 
        {             
          GDF_COPY_TIMES_SCALAR_DATA(rbu_page_stat_data_idx, 31, 
                      (double)(((double)rbu_page_stat_avg[i].wait_time) /
                                 ((double)(1000.0*(double)rbu_page_stat_avg[i].wait_counts))),
                      (double)rbu_page_stat_avg[i].wait_min_time/1000.0,
                      (double)rbu_page_stat_avg[i].wait_max_time/1000.0,
                      rbu_page_stat_avg[i].wait_counts,
                      rbu_page_stat_data_local_gp_ptr[k].wait_time_gp.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].wait_time_gp.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].wait_time_gp.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].wait_time_gp.succ);
        }
        else           
        {              
          GDF_COPY_TIMES_SCALAR_DATA(rbu_page_stat_data_idx, 31,
                      0,
                      -1,
                      0, 
                      0,
                      rbu_page_stat_data_local_gp_ptr[k].wait_time_gp.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].wait_time_gp.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].wait_time_gp.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].wait_time_gp.succ);
        }

        // rcv time Graph
        if(rbu_page_stat_avg[i].rcv_counts > 0)
        {             
          GDF_COPY_TIMES_SCALAR_DATA(rbu_page_stat_data_idx, 32, 
                      (double)(((double)rbu_page_stat_avg[i].rcv_time) /
                                 ((double)(1000.0*(double)rbu_page_stat_avg[i].rcv_counts))),
                      (double)rbu_page_stat_avg[i].rcv_min_time/1000.0,
                      (double)rbu_page_stat_avg[i].rcv_max_time/1000.0,
                      rbu_page_stat_avg[i].rcv_counts,
                      rbu_page_stat_data_local_gp_ptr[k].rcv_time_gp.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].rcv_time_gp.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].rcv_time_gp.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].rcv_time_gp.succ);
        }
        else           
        {              
          GDF_COPY_TIMES_SCALAR_DATA(rbu_page_stat_data_idx, 32,
                      0,
                      -1,
                      0, 
                      0,
                      rbu_page_stat_data_local_gp_ptr[k].rcv_time_gp.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].rcv_time_gp.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].rcv_time_gp.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].rcv_time_gp.succ);
        }

        // blckd time Graph
        if(rbu_page_stat_avg[i].blckd_counts > 0)
        {             
          GDF_COPY_TIMES_SCALAR_DATA(rbu_page_stat_data_idx, 33, 
                      (double)(((double)rbu_page_stat_avg[i].blckd_time) /
                                 ((double)(1000.0*(double)rbu_page_stat_avg[i].blckd_counts))),
                      (double)rbu_page_stat_avg[i].blckd_min_time/1000.0,
                      (double)rbu_page_stat_avg[i].blckd_max_time/1000.0,
                      rbu_page_stat_avg[i].blckd_counts,
                      rbu_page_stat_data_local_gp_ptr[k].blckd_time_gp.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].blckd_time_gp.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].blckd_time_gp.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].blckd_time_gp.succ);
        }
        else           
        {              
          GDF_COPY_TIMES_SCALAR_DATA(rbu_page_stat_data_idx, 33,
                      0,
                      -1,
                      0, 
                      0,
                      rbu_page_stat_data_local_gp_ptr[k].blckd_time_gp.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].blckd_time_gp.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].blckd_time_gp.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].blckd_time_gp.succ);
        }

        // url_resp time Graph
        if(rbu_page_stat_avg[i].url_resp_counts > 0)
        {             
          GDF_COPY_TIMES_SCALAR_DATA(rbu_page_stat_data_idx, 34,
                      (double)(((double)rbu_page_stat_avg[i].url_resp_time) /
                                 ((double)(1000.0*(double)rbu_page_stat_avg[i].url_resp_counts))),
                      (double)rbu_page_stat_avg[i].url_resp_min_time/1000.0,
                      (double)rbu_page_stat_avg[i].url_resp_max_time/1000.0,
                      rbu_page_stat_avg[i].url_resp_counts,
                      rbu_page_stat_data_local_gp_ptr[k].url_resp_time_gp.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].url_resp_time_gp.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].url_resp_time_gp.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].url_resp_time_gp.succ);
        }
        else          
        {             
          GDF_COPY_TIMES_SCALAR_DATA(rbu_page_stat_data_idx, 34,
                      0,
                      -1,
                      0,
                      0,
                      rbu_page_stat_data_local_gp_ptr[k].url_resp_time_gp.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].url_resp_time_gp.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].url_resp_time_gp.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].url_resp_time_gp.succ);
        }
        

        NSDL2_RBU(NULL, NULL, "tbt_counts = %d, lcp_count = %d, cls_count - %d, tbt = %d, lcp = %d, cls = %f", rbu_page_stat_avg[i].tbt_counts, rbu_page_stat_avg[i].lcp_count, rbu_page_stat_avg[i].cls_count, rbu_page_stat_avg[i].tbt, rbu_page_stat_avg[i].lcp, rbu_page_stat_avg[i].cls);


        if(rbu_page_stat_avg[i].tbt_counts > 0)
        {             
          GDF_COPY_TIMES_STD_VECTOR_DATA(rbu_page_stat_data_idx, 35, k, 0,
                      (double)(((double)rbu_page_stat_avg[i].tbt) /
                                 ((double)(1000.0*(double)rbu_page_stat_avg[i].tbt_counts))),
                      (double)rbu_page_stat_avg[i].tbt_min/1000.0,
                      (double)rbu_page_stat_avg[i].tbt_max/1000.0,
                      rbu_page_stat_avg[i].tbt_counts,
                      rbu_page_stat_avg[i].tbt_sum_sqr/1000.0,                     //Converting sum_sqr into millisecond 
                      rbu_page_stat_data_local_gp_ptr[k].tbt_gp.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].tbt_gp.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].tbt_gp.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].tbt_gp.succ,
                      rbu_page_stat_data_local_gp_ptr[k].tbt_gp.sum_of_sqr);
        }
        else
        {
          GDF_COPY_TIMES_STD_VECTOR_DATA(rbu_page_stat_data_idx, 35, k, 0,
                      0,
                      -1,
                      0,
                      0,
                      0,
                      rbu_page_stat_data_local_gp_ptr[k].tbt_gp.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].tbt_gp.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].tbt_gp.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].tbt_gp.succ,
                      rbu_page_stat_data_local_gp_ptr[k].tbt_gp.sum_of_sqr);
        }

        if(rbu_page_stat_avg[i].lcp_count > 0)
        {
          GDF_COPY_TIMES_STD_VECTOR_DATA(rbu_page_stat_data_idx, 36, k, 0,
                      (double)(((double)rbu_page_stat_avg[i].lcp) /
                                 ((double)(1000.0*(double)rbu_page_stat_avg[i].lcp_count))),
                      (double)rbu_page_stat_avg[i].lcp_min/1000.0,
                      (double)rbu_page_stat_avg[i].lcp_max/1000.0,
                      rbu_page_stat_avg[i].lcp_count,
                      rbu_page_stat_avg[i].lcp_sum_sqr/1000.0,                     //Converting sum_sqr into millisecond 
                      rbu_page_stat_data_local_gp_ptr[k].lcp_gp.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].lcp_gp.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].lcp_gp.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].lcp_gp.succ,
                      rbu_page_stat_data_local_gp_ptr[k].lcp_gp.sum_of_sqr);
        }

          else
        {
          GDF_COPY_TIMES_STD_VECTOR_DATA(rbu_page_stat_data_idx, 36, k, 0,
                      0,
                      -1,
                      0,
                      0,
                      0,
                      rbu_page_stat_data_local_gp_ptr[k].lcp_gp.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].lcp_gp.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].lcp_gp.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].lcp_gp.succ,
                      rbu_page_stat_data_local_gp_ptr[k].lcp_gp.sum_of_sqr);
        }

        if(rbu_page_stat_avg[i].cls_count > 0)
        {
          GDF_COPY_TIMES_STD_VECTOR_DATA(rbu_page_stat_data_idx, 37, k, 0,
                      (double)(((double)rbu_page_stat_avg[i].cls) /
                                 ((double)((double)rbu_page_stat_avg[i].cls_count))),
                      (double)rbu_page_stat_avg[i].cls_min,
                      (double)rbu_page_stat_avg[i].cls_max,
                      rbu_page_stat_avg[i].cls_count,
                      rbu_page_stat_avg[i].cls_sum_sqr,                     //Converting sum_sqr into millisecond 
                      rbu_page_stat_data_local_gp_ptr[k].cls_gp.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].cls_gp.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].cls_gp.max_time,
                          rbu_page_stat_data_local_gp_ptr[k].cls_gp.succ,
                      rbu_page_stat_data_local_gp_ptr[k].cls_gp.sum_of_sqr);
        }
        else
        {
          GDF_COPY_TIMES_STD_VECTOR_DATA(rbu_page_stat_data_idx, 37, k, 0,
                      0,
                      -1,
                      0,
                      0,
                      0,
                      rbu_page_stat_data_local_gp_ptr[k].cls_gp.avg_time,
                      rbu_page_stat_data_local_gp_ptr[k].cls_gp.min_time,
                      rbu_page_stat_data_local_gp_ptr[k].cls_gp.max_time,
                      rbu_page_stat_data_local_gp_ptr[k].cls_gp.succ,
                      rbu_page_stat_data_local_gp_ptr[k].cls_gp.sum_of_sqr);
  
        }
      }
    }
  }
}
/*------------------------------------------------------------------------------------------------
* Function Name      : get_browser_mode()
*
* Purpose            : This function will convert browser-type (string) to browser-mode (numeric)
*
* Input              : browser - Browser-Type
*                      e.g.: Firefox, Chromium-browser, etc.
*
* Output             : Browser Mode
*                      e.g.: 0, 1, etc.
*    --------------------------------------------------
*    |         Input         |         Output         |
*    --------------------------------------------------
*          Firefox           |         0
*      Chromium-browser      |         1
*        Mobile Safari       |         2
*         Mozilla(IE)        |         3
* Android HttpURLConnection  |         4
*
* Called from        : parse_and_set_rbu_page_stat()
*
* Author             : Vikas Verma
* Date               : 05 October 2017
*-----------------------------------------------------------------------------------------------*/
inline int get_browser_mode(char *browser){
  NSDL4_RBU(NULL, NULL, "Method called for browser = %s", browser);
  if(!strcasecmp(browser, "Firefox") || !strcmp(browser, "0"))
    return 0;                                                     //Return 0 for Firefox
  else if(!strcasecmp(browser, "Chromium-browser") || !strcasecmp(browser, "Chrome Mobile") || !strcmp(browser, "1"))
    return 1;                                                     //Return 1 for Chromium-browser
  else if(!strcasecmp(browser, "Mobile Safari") || !strcasecmp(browser, "Safari") || !strcmp(browser, "2"))
    return 2;                                                     //Return 2 for Mobile Safari
  /*Internally 'Mozilla' is set as browserType in HarStat in case of IE,
    So we are comparing with 'Mozilla' as well in place of IE
  */
  else if(!strcasecmp(browser, "Internet Explorer") || !strcasecmp(browser, "Mozilla") || !strcmp(browser, "3")) 
    return 3;                                                     //Return 3 for Internet Explorer 
  else if(!strcasecmp(browser, "Android HttpURLConnection"))
    return 4;                                                     //Return 4 for Android HttpURLConnection
  else
    return 0;                                                     //By Default or for any other browser, we show firefox
}

/*---------------------------------------------------------------------------------------------
       * Function Name  : parse_and_set_rbu_page_stat()
       *`
       * Purpose             : i) This function will parse the message coming from java-type script for PMS_application, 
       *                      ii) fill the parsed value to the RBU_RespAttr structure,
       *                     iii) from RBU_RespAttr structure, RBUPageStatAvgTime structure is filled,
       *                          using set_rbu_page_stat_data_avgtime_data() and set_rbu_page_stat_data_avgtime_data_sess_only().
       *
       * Input                : vptr: to get RBU_RespAttr structure access
       *                        input_msg : message coming from java_type_script, which is tokenized here
       * Called from   : ns_rbu_page_stats_msg_proc_func()
*--------------------------------------------------------------------------------------------*/ 
inline void parse_and_set_rbu_page_stat(VUser *vptr, char *input_msg)
{
  char *attr[MAX_ENTRY_FOR_RBU_PAGE_STAT];                    //Total Number of Graphs in RBU Page Stat
  int num_attr = 0;
  int graph_no = 0;
  char har_name[1024] = "";
  u_ns_ts_t har_date_and_time = -1;
  char prof_name[128] = {0};
  int speed_index = -1;
  char pagename[MAX_LINE_LENGTH] = "";
  char *start_ptr, *end_ptr = NULL;

  NSDL2_RBU(vptr, NULL, "Method called input_msg = %s", input_msg);  

  RBU_RespAttr rbu_resp_attr_var;
  RBU_RespAttr *rbu_resp_attr;
  rbu_resp_attr = &rbu_resp_attr_var;

  memset(rbu_resp_attr, 0, sizeof(RBU_RespAttr));

  RESET_RBU_RESP_ATTR_TIME(rbu_resp_attr);

  num_attr = get_tokens(input_msg, attr, "|", MAX_ENTRY_FOR_RBU_PAGE_STAT);

  if(num_attr != MAX_ENTRY_FOR_RBU_PAGE_STAT){
    NSDL4_RBU(vptr, NULL, "number of attribute [%d] coming from java message is not as much of number of graphs [%d]",
                           num_attr, MAX_ENTRY_FOR_RBU_PAGE_STAT);
  }

  //Allocating for device info 
  NSDL2_API(vptr, NULL, "Allocating dvc_info, rbu_resp_attr->dvc_info = %p", \
                         rbu_resp_attr->dvc_info); \
  MY_MALLOC(rbu_resp_attr->dvc_info, RBU_HAR_FILE_NAME_SIZE + 1,\
                                    "rbu_resp_attr->dvc_info", -1); \
  memset(rbu_resp_attr->dvc_info, 0, RBU_HAR_FILE_NAME_SIZE + 1); \
  NSDL2_API(vptr, NULL, "After allocation of dvc_info, rbu_resp_attr->dvc_info = %p", \
                                             rbu_resp_attr->dvc_info); \
  //Allocating for access_log_msg 
  NSDL4_API(vptr, NULL, "Allocating access_log_msg, rbu_resp_attr->access_log_msg = %p", rbu_resp_attr->access_log_msg);
  MY_MALLOC(rbu_resp_attr->access_log_msg, RBU_MAX_ACC_LOG_LENGTH + 1, "rbu_resp_attr->access_log_msg", -1);
  memset(rbu_resp_attr->access_log_msg, 0, RBU_MAX_ACC_LOG_LENGTH + 1);
  NSDL3_API(vptr, NULL, "After allocation of access_log_msg, rbu_resp_attr->access_log_msg = %p", rbu_resp_attr->access_log_msg);

  for(graph_no = 0 ; graph_no < num_attr; graph_no++)
  {
    switch(graph_no)
    {
      case ON_CONTENT_LOAD:                                             //1. Dom Load Time
        rbu_resp_attr->on_content_load_time = atol(attr[graph_no]);
        NSDL4_RBU(vptr, NULL, "RBU RESP ATTR on_content_load_time = %d and index is [%d]", rbu_resp_attr->on_content_load_time, graph_no);
        break;
      case ON_LOAD:                                              //2. On Load Time
        rbu_resp_attr->on_load_time = atol(attr[graph_no]);
        NSDL4_RBU(vptr, NULL, "RBU RESP ATTR on_load_time = %d and index is [%d]", rbu_resp_attr->on_load_time, graph_no);
        break;
      case PAGE_LOAD:                                            //3. Page Load Time
        rbu_resp_attr->page_load_time = atol(attr[graph_no]);
        NSDL4_RBU(vptr, NULL, "RBU RESP ATTR page_load_time = %d and index is [%d]", rbu_resp_attr->page_load_time, graph_no);
        break;
      case TTI:                                                  //4. TTI
        rbu_resp_attr->_tti_time = atol(attr[graph_no]);
        NSDL4_RBU(vptr, NULL, "RBU RESP ATTR _tti_time = %d and index is [%d]", rbu_resp_attr->_tti_time, graph_no);
        break;
      case START_RENDER:                                         //5. Start Render Time
        (rbu_resp_attr->_cav_start_render_time) = atol(attr[graph_no]);
        NSDL4_RBU(vptr, NULL, "RBU RESP ATTR _cav_start_render_time = %d and index is [%d]", rbu_resp_attr->_cav_start_render_time, graph_no);
        break;
      case VISUALLY_COMPLETE:                                    //6. Visually Complete Time
        rbu_resp_attr->_cav_end_render_time = atol(attr[graph_no]);
        NSDL4_RBU(vptr, NULL, "RBU RESP ATTR _cav_end_render_time = %d and index is [%d]", rbu_resp_attr->_cav_end_render_time, graph_no);
        break;
      case REQUEST_WO_CACHE:                                     //7. Request Without Cache
        rbu_resp_attr->request_without_cache = atol(attr[graph_no]);
        NSDL4_RBU(vptr, NULL, "RBU RESP ATTR request_without_cache = %d and index is [%d]", rbu_resp_attr->request_without_cache, graph_no);
        break;
      case REQUEST_FROM_CACHE:                                   //8. Request From Cache
        rbu_resp_attr->request_from_cache = atol(attr[graph_no]);
        NSDL4_RBU(vptr, NULL, "RBU RESP ATTR request_from_cache = %d and index is [%d]", rbu_resp_attr->request_from_cache, graph_no);
        break;
      case BYTE_RCVD:                                            //9. Byte Recieved
        rbu_resp_attr->byte_rcvd = (float)(atol(attr[graph_no]))/1024;
        NSDL4_RBU(vptr, NULL, "RBU RESP ATTR byte_rcvd = %f and index is [%d]", rbu_resp_attr->byte_rcvd, graph_no);
        break;
      case BYTE_SEND:                                            //10. Byte Send
        rbu_resp_attr->byte_send = (float)(atol(attr[graph_no]))/1024;
        NSDL4_RBU(vptr, NULL, "RBU RESP ATTR byte_send = %f and index is [%d]", rbu_resp_attr->byte_send, graph_no);
        break;
      case PG_WGT:                                               //11. Total Page Wght
        rbu_resp_attr->pg_wgt = (float)(atol(attr[graph_no]))/1024;
        NSDL4_RBU(vptr, NULL, "RBU RESP ATTR pg_wgt = %f and index is [%d]", rbu_resp_attr->pg_wgt, graph_no);
        break;
      case JS_SIZE:                                              //12. JS Size
        rbu_resp_attr->resp_js_size = (float)(atol(attr[graph_no]))/1024;
        NSDL4_RBU(vptr, NULL, "RBU RESP ATTR resp_js_size = %f and index is [%d]", rbu_resp_attr->resp_js_size, graph_no);
        break;
      case CSS_SIZE:                                              //13. CSS Size
        rbu_resp_attr->resp_css_size = (float)(atol(attr[graph_no]))/1024;
        NSDL4_RBU(vptr, NULL, "RBU RESP ATTR resp_css_size = %f and index is [%d]", rbu_resp_attr->resp_css_size, graph_no);
        break;
      case IMG_SIZE:                                             //14. Image Size
        rbu_resp_attr->resp_img_size = (float)(atol(attr[graph_no]))/1024;
        NSDL4_RBU(vptr, NULL, "RBU RESP ATTR resp_img_size = %f and index is [%d]", rbu_resp_attr->resp_img_size, graph_no);
        break;
      case DOM_ELEMNT:                                           //15. Dom Element
        rbu_resp_attr->dom_element = atol(attr[graph_no]);
        NSDL4_RBU(vptr, NULL, "RBU RESP ATTR dom element = %d and index is [%d]", rbu_resp_attr->dom_element, graph_no);
        break;
      case PG_SPEED:                                             //16. Page Speed/Score
        rbu_resp_attr->pg_speed = atol(attr[graph_no]);
        NSDL4_RBU(vptr, NULL, "RBU RESP ATTR pg_speed = %d and index is [%d]", rbu_resp_attr->pg_speed, graph_no);
        break;
      case AKAMAI_CACHE:                                         //17. Akamai Cache Offload
        rbu_resp_attr->akamai_cache = atol(attr[graph_no]);
        NSDL4_RBU(vptr, NULL, "RBU RESP ATTR akamai_cache = %d and index is [%d]", rbu_resp_attr->akamai_cache, graph_no);
        break;
      case MAIN_URL_RESP:                                        //18. Main URL Resp Time
        rbu_resp_attr->main_url_resp_time = atol(attr[graph_no]);
        NSDL4_RBU(vptr, NULL, "RBU RESP ATTR main_url_resp_time = %d and index is [%d]", rbu_resp_attr->main_url_resp_time, graph_no);
        break;
      case PG_AVAIL:                                             //19. Page Availability
        rbu_resp_attr->pg_avail = atol(attr[graph_no]);
        NSDL4_RBU(vptr, NULL, "RBU RESP ATTR pg_avail = %d and index is [%d]", rbu_resp_attr->pg_avail, graph_no);
        break;
      case SESS_COMPLETE:                                        //20. Session Complete
        rbu_resp_attr->sess_completed = atol(attr[graph_no]);
        NSDL4_RBU(vptr, NULL, "RBU RESP ATTR sess_completed = %d and index is [%d]", rbu_resp_attr->sess_completed, graph_no);
        break;
      case SESS_SUCC:                                            //21. Session Success
        rbu_resp_attr->sess_success = atol(attr[graph_no]);
        NSDL4_RBU(vptr, NULL, "RBU RESP ATTR request_without_cache = %d and index is [%d]", rbu_resp_attr->sess_success, graph_no);
        break;
      case HAR_NAME:                                             //22. Har File Name
        get_har_name_and_date(vptr, attr[graph_no], har_name, &har_date_and_time, &speed_index, prof_name);
        NSDL4_RBU(vptr, NULL, "RBU RESP ATTR Har_name calculation");
        //get pagename and findout page_norm_id
        start_ptr = har_name;
        end_ptr = strstr(har_name, "+");
        if(start_ptr && end_ptr)
        {
          start_ptr += 2;  //Move 2 bytes forward for start of pagename
          snprintf(pagename, (end_ptr - start_ptr) + 1, "%s", start_ptr);  //+1 for '\0'

          rbu_resp_attr->page_norm_id = get_norm_id_for_page(pagename,
                                            get_sess_name_with_proj_subproj_int(vptr->sess_ptr->sess_name, vptr->sess_ptr->sess_id, "/"),
                                            vptr->sess_ptr->sess_norm_id); 
          NSDL4_RBU(vptr, NULL, "page_norm_id = %d for page '%s and Har = %s'.", rbu_resp_attr->page_norm_id, pagename, har_name);
        }
        break;
      case BROWSER_MODE:                                         //23. Browser Mode
        //For Bug : 17415
        //runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.browser_mode = atol(attr[graph_no]); 
        runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.browser_mode = get_browser_mode(attr[graph_no]);
        NSDL4_RBU(vptr, NULL, "RBU RESP ATTR browser mode = %d and index is [%d]", 
                               runprof_table_shr_mem[vptr->group_num].gset.rbu_gset.browser_mode, graph_no);
        break;
      case MAIN_URL_START_DATE_TIME:                            //24. Main Url start date time
        NSDL4_RBU(vptr, NULL, "RBU RESP ATTR : msg value of main_url_start_date_time - %lld and unix_cav_epoch_diff - %ld",
                               atol(attr[graph_no]), global_settings->unix_cav_epoch_diff);
        rbu_resp_attr->main_url_start_date_time = ((atol(attr[graph_no])/1000) - global_settings->unix_cav_epoch_diff);
        NSDL4_RBU(vptr, NULL, "RBU RESP ATTR main_url_start_date_time in epoch = %lld and index is [%d]",
                               rbu_resp_attr->main_url_start_date_time, graph_no);
        break;
      case PAGE_STATUS:                                         //25. Page Status
        rbu_resp_attr->cv_status = atoi(attr[graph_no]);
        NSDL4_RBU(vptr, NULL, "RBU RESP ATTR pg_status = %d and index is [%d]", rbu_resp_attr->cv_status, graph_no);
        break;
      case DEVICE_INFO:                                         //26. Device Info in format - Mobile:Android:6.0:Samsung Galaxy
        strcpy(rbu_resp_attr->dvc_info, attr[graph_no]);
        NSDL4_RBU(vptr, NULL, "RBU RESP ATTR Device Info = %s and index is [%d]", rbu_resp_attr->dvc_info, graph_no);
        break;
      case PERFORMANCE_TRACE_MODE:                              //27. Performance trace(JSProfiler) mode
        rbu_resp_attr->performance_trace_flag = atoi(attr[graph_no]);
        NSDL4_RBU(vptr, NULL, "RBU RESP ATTR performance_trace_flag = %d and index is [%d]", rbu_resp_attr->performance_trace_flag, graph_no);
        break;
      case SPEED_INDEX:                                        //28. Speed Index
        speed_index = atoi(attr[graph_no]);
        NSDL4_RBU(vptr, NULL, "speed_index = %d and index is [%d]", speed_index, graph_no);
        break;
      default:
        NSDL4_RBU(vptr, NULL, "Captured message index is more than %d", num_attr);
        break;
    }
  }

  if(har_name[0])
    log_page_rbu_detail_record(vptr, har_name , har_date_and_time, speed_index, rbu_resp_attr->cav_nv_val, rbu_resp_attr, prof_name);

  create_csv_data(vptr, rbu_resp_attr);
  //These two functions will fill RBUPageStatAvgTime structure from RBU_RESP_ATTR structure
  if(global_settings->protocol_enabled & RBU_API_USED){
    set_rbu_page_stat_data_avgtime_data(vptr, rbu_resp_attr);	
    set_rbu_page_stat_data_avgtime_data_sess_only(vptr, rbu_resp_attr);
    set_rbu_page_status_data_avgtime_data(vptr, NULL);
  }
}

/*---------------------------------------------------------------------------------------------
       * Function Name  : ns_get_rbu_settings()
       *
       * Purpose        : Fill buffer with RBU settings
       *
       * Input          : grp_idx: group number
       *                  out_msg : pointer to a buffer where to write rbu settings
       * Called from   : ns_rbu_setting_function()[ns_java_string_api.c]
*--------------------------------------------------------------------------------------------*/
inline void ns_get_rbu_settings(int grp_idx, char *out_msg)
{
  NSDL2_RBU(NULL, NULL, "Method called grp_idx = %d", grp_idx);

  sprintf(out_msg,"#debug_trace|%d|#clip_settings|%d|%d", debug_trace_log_value,
                   runprof_table_shr_mem[grp_idx].gset.rbu_gset.enable_capture_clip,
                   runprof_table_shr_mem[grp_idx].gset.rbu_gset.clip_frequency);

  NSDL1_RBU(NULL, NULL, "out_msg = %s", out_msg);
}
