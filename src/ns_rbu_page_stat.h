#ifndef RBU_PAGE_STAT_DATA_H
#define RBU_PAGE_STAT_DATA_H

//Macros used while filling RBU_RespAttr,when we have Java-Type Script and not running RBU
#define MAX_ENTRY_FOR_RBU_PAGE_STAT 28           //Number of graphs in RBU Page Stat+har file
#define ON_CONTENT_LOAD 0
#define ON_LOAD 1
#define PAGE_LOAD 2
#define TTI 3
#define START_RENDER 4
#define VISUALLY_COMPLETE 5
#define REQUEST_WO_CACHE 6
#define REQUEST_FROM_CACHE 7
#define BYTE_RCVD 8
#define BYTE_SEND 9
#define PG_WGT 10
#define JS_SIZE 11
#define CSS_SIZE 12
#define IMG_SIZE 13
#define DOM_ELEMNT 14
#define PG_SPEED 15
#define AKAMAI_CACHE 16
#define MAIN_URL_RESP 17
#define PG_AVAIL 18
#define SESS_COMPLETE 19
#define SESS_SUCC 20
#define HAR_NAME 21
#define BROWSER_MODE 22
#define MAIN_URL_START_DATE_TIME 23
#define PAGE_STATUS 24
#define DEVICE_INFO 25
#define PERFORMANCE_TRACE_MODE 26
#define SPEED_INDEX 27

#define RBU_PAGE_STAT_SIZE (sizeof(RBUPageStatAvgTime) * g_actual_num_pages)

//Set periodic elements of struct a with b into a
#define SET_MIN_MAX_RBU_PAGE_STAT_DATA_PERIODICS(a, b)\
  for (i = 0; i < g_actual_num_pages; i++) {\
    SET_MIN (a[i].DOMContent_Loaded_min_time, b[i].DOMContent_Loaded_min_time);\
    SET_MAX (a[i].DOMContent_Loaded_max_time, b[i].DOMContent_Loaded_max_time);\
    SET_MIN (a[i].OnLoad_min_time, b[i].OnLoad_min_time);\
    SET_MAX (a[i].OnLoad_max_time, b[i].OnLoad_max_time);\
    SET_MIN (a[i].PageLoad_min_time, b[i].PageLoad_min_time);\
    SET_MAX (a[i].PageLoad_max_time, b[i].PageLoad_max_time);\
    SET_MIN (a[i].TTI_min_time, b[i].TTI_min_time);\
    SET_MAX (a[i].TTI_max_time, b[i].TTI_max_time);\
    SET_MIN (a[i]._cav_startRender_min_time, b[i]._cav_startRender_min_time);\
    SET_MAX (a[i]._cav_startRender_max_time, b[i]._cav_startRender_max_time);\
    SET_MIN (a[i].visually_complete_min_time, b[i].visually_complete_min_time);\
    SET_MAX (a[i].visually_complete_max_time, b[i].visually_complete_max_time);\
    SET_MIN (a[i].cur_rbu_requests_min, b[i].cur_rbu_requests_min);\
    SET_MAX (a[i].cur_rbu_requests_max, b[i].cur_rbu_requests_max);\
    SET_MIN (a[i].cur_rbu_browser_cache_min, b[i].cur_rbu_browser_cache_min);\
    SET_MAX (a[i].cur_rbu_browser_cache_max, b[i].cur_rbu_browser_cache_max);\
    SET_MIN (a[i].cur_rbu_bytes_recieved_min, b[i].cur_rbu_bytes_recieved_min);\
    SET_MAX (a[i].cur_rbu_bytes_recieved_max, b[i].cur_rbu_bytes_recieved_max);\
    SET_MIN (a[i].cur_rbu_bytes_send_min, b[i].cur_rbu_bytes_send_min);\
    SET_MAX (a[i].cur_rbu_bytes_send_max, b[i].cur_rbu_bytes_send_max);\
    SET_MIN (a[i].cur_rbu_page_wgt_min, b[i].cur_rbu_page_wgt_min);\
    SET_MAX (a[i].cur_rbu_page_wgt_max, b[i].cur_rbu_page_wgt_max);\
    SET_MIN (a[i].cur_rbu_js_size_min, b[i].cur_rbu_js_size_min);\
    SET_MAX (a[i].cur_rbu_js_size_max, b[i].cur_rbu_js_size_max);\
    SET_MIN (a[i].cur_rbu_css_size_min, b[i].cur_rbu_css_size_min);\
    SET_MAX (a[i].cur_rbu_css_size_max, b[i].cur_rbu_css_size_max);\
    SET_MIN (a[i].cur_rbu_img_wgt_min, b[i].cur_rbu_img_wgt_min);\
    SET_MAX (a[i].cur_rbu_img_wgt_max, b[i].cur_rbu_img_wgt_max);\
    SET_MIN (a[i].cur_rbu_dom_element_min, b[i].cur_rbu_dom_element_min);\
    SET_MAX (a[i].cur_rbu_dom_element_max, b[i].cur_rbu_dom_element_max);\
    SET_MIN (a[i].cur_rbu_pg_speed_min, b[i].cur_rbu_pg_speed_min);\
    SET_MAX (a[i].cur_rbu_pg_speed_max, b[i].cur_rbu_pg_speed_max);\
    SET_MIN (a[i].cur_rbu_akamai_cache_min, b[i].cur_rbu_akamai_cache_min);\
    SET_MAX (a[i].cur_rbu_akamai_cache_max, b[i].cur_rbu_akamai_cache_max);\
    SET_MIN (a[i].cur_rbu_main_url_resp_time_min, b[i].cur_rbu_main_url_resp_time_min);\
    SET_MAX (a[i].cur_rbu_main_url_resp_time_max, b[i].cur_rbu_main_url_resp_time_max);\
    SET_MIN (a[i].dns_min_time, b[i].dns_min_time);\
    SET_MAX (a[i].dns_max_time, b[i].dns_max_time);\
    SET_MIN (a[i].tcp_min_time, b[i].tcp_min_time);\
    SET_MAX (a[i].tcp_max_time, b[i].tcp_max_time);\
    SET_MIN (a[i].ssl_min_time, b[i].ssl_min_time);\
    SET_MAX (a[i].ssl_max_time, b[i].ssl_max_time);\
    SET_MIN (a[i].connect_min_time, b[i].connect_min_time);\
    SET_MAX (a[i].connect_max_time, b[i].connect_max_time);\
    SET_MIN (a[i].wait_min_time, b[i].wait_min_time);\
    SET_MAX (a[i].wait_max_time, b[i].wait_max_time);\
    SET_MIN (a[i].rcv_min_time, b[i].rcv_min_time);\
    SET_MAX (a[i].rcv_max_time, b[i].rcv_max_time);\
    SET_MIN (a[i].blckd_min_time, b[i].blckd_min_time);\
    SET_MAX (a[i].blckd_max_time, b[i].blckd_max_time);\
    SET_MIN (a[i].url_resp_min_time, b[i].url_resp_min_time);\
    SET_MAX (a[i].url_resp_max_time, b[i].url_resp_max_time);\
    SET_MIN (a[i].tbt_min, b[i].tbt_min);\
    SET_MAX (a[i].tbt_max, b[i].tbt_max);\
    SET_MIN (a[i].lcp_min, b[i].lcp_min);\
    SET_MAX (a[i].lcp_max, b[i].lcp_max);\
    SET_MIN (a[i].cls_min, b[i].cls_min);\
    SET_MAX (a[i].cls_max, b[i].cls_max);\
  }

#define ACC_RBU_PAGE_STAT_DATA_PERIODICS(a, b)\
  for (i = 0; i < g_actual_num_pages; i++) {\
    a[i].DOMContent_Loaded_time += b[i].DOMContent_Loaded_time;\
    a[i].DOMContent_Loaded_counts += b[i].DOMContent_Loaded_counts;\
    a[i].DOMContent_Loaded_sum_sqr += b[i].DOMContent_Loaded_sum_sqr;\
    a[i].OnLoad_time += b[i].OnLoad_time;\
    a[i].OnLoad_counts += b[i].OnLoad_counts;\
    a[i].OnLoad_sum_sqr += b[i].OnLoad_sum_sqr;\
    a[i].PageLoad_time += b[i].PageLoad_time;\
    a[i].PageLoad_counts += b[i].PageLoad_counts;\
    a[i].PageLoad_sum_sqr += b[i].PageLoad_sum_sqr;\
    a[i].TTI_time += b[i].TTI_time;\
    a[i].TTI_counts += b[i].TTI_counts;\
    a[i].TTI_sum_sqr += b[i].TTI_sum_sqr;\
    a[i]._cav_startRender_time += b[i]._cav_startRender_time;\
    a[i]._cav_startRender_counts += b[i]._cav_startRender_counts;\
    a[i]._cav_startRender_sum_sqr += b[i]._cav_startRender_sum_sqr;\
    a[i].visually_complete_time += b[i].visually_complete_time;\
    a[i].visually_complete_counts += b[i].visually_complete_counts;\
    a[i].visually_complete_sum_sqr += b[i].visually_complete_sum_sqr;\
    a[i].cur_rbu_requests += b[i].cur_rbu_requests;\
    a[i].cur_rbu_requests_counts += b[i].cur_rbu_requests_counts;\
    a[i].cur_rbu_requests_sum_sqr += b[i].cur_rbu_requests_sum_sqr;\
    a[i].cur_rbu_browser_cache += b[i].cur_rbu_browser_cache;\
    a[i].cur_rbu_browser_cache_count += b[i].cur_rbu_browser_cache_count;\
    a[i].cur_rbu_browser_cache_sum_sqr += b[i].cur_rbu_browser_cache_sum_sqr;\
    a[i].cur_rbu_bytes_recieved += b[i].cur_rbu_bytes_recieved;\
    a[i].cur_rbu_bytes_recieved_count += b[i].cur_rbu_bytes_recieved_count;\
    a[i].cur_rbu_bytes_recieved_sum_sqr += b[i].cur_rbu_bytes_recieved_sum_sqr;\
    a[i].cur_rbu_bytes_send += b[i].cur_rbu_bytes_send;\
    a[i].cur_rbu_bytes_send_count += b[i].cur_rbu_bytes_send_count;\
    a[i].cur_rbu_bytes_send_sum_sqr += b[i].cur_rbu_bytes_send_sum_sqr;\
    a[i].cur_rbu_page_wgt += b[i].cur_rbu_page_wgt;\
    a[i].cur_rbu_page_wgt_count += b[i].cur_rbu_page_wgt_count;\
    a[i].cur_rbu_page_wgt_sum_sqr += b[i].cur_rbu_page_wgt_sum_sqr;\
    a[i].cur_rbu_js_size += b[i].cur_rbu_js_size;\
    a[i].cur_rbu_js_count += b[i].cur_rbu_js_count;\
    a[i].cur_rbu_js_sum_sqr += b[i].cur_rbu_js_sum_sqr;\
    a[i].cur_rbu_css_size += b[i].cur_rbu_css_size;\
    a[i].cur_rbu_css_count += b[i].cur_rbu_css_count;\
    a[i].cur_rbu_css_sum_sqr += b[i].cur_rbu_css_sum_sqr;\
    a[i].cur_rbu_img_wgt += b[i].cur_rbu_img_wgt;\
    a[i].cur_rbu_img_wgt_count += b[i].cur_rbu_img_wgt_count;\
    a[i].cur_rbu_img_wgt_sum_sqr += b[i].cur_rbu_img_wgt_sum_sqr;\
    a[i].cur_rbu_dom_element += b[i].cur_rbu_dom_element;\
    a[i].cur_rbu_dom_element_count += b[i].cur_rbu_dom_element_count;\
    a[i].cur_rbu_dom_element_sum_sqr += b[i].cur_rbu_dom_element_sum_sqr;\
    a[i].cur_rbu_pg_speed += b[i].cur_rbu_pg_speed;\
    a[i].cur_rbu_pg_speed_count += b[i].cur_rbu_pg_speed_count;\
    a[i].cur_rbu_pg_speed_sum_sqr += b[i].cur_rbu_pg_speed_sum_sqr;\
    a[i].cur_rbu_akamai_cache += b[i].cur_rbu_akamai_cache;\
    a[i].cur_rbu_akamai_cache_count += b[i].cur_rbu_akamai_cache_count;\
    a[i].cur_rbu_akamai_cache_sum_sqr += b[i].cur_rbu_akamai_cache_sum_sqr;\
    a[i].cur_rbu_main_url_resp_time += b[i].cur_rbu_main_url_resp_time;\
    a[i].cur_rbu_main_url_resp_time_count += b[i].cur_rbu_main_url_resp_time_count;\
    a[i].cur_rbu_main_url_resp_time_sum_sqr += b[i].cur_rbu_main_url_resp_time_sum_sqr;\
    a[i].pg_avail = b[i].pg_avail;\
    a[i].sess_completed += b[i].sess_completed;\
    a[i].sess_success += b[i].sess_success;\
    a[i].pg_status_1xx += b[i].pg_status_1xx;\
    a[i].pg_status_2xx += b[i].pg_status_2xx;\
    a[i].pg_status_3xx += b[i].pg_status_3xx;\
    a[i].pg_status_4xx += b[i].pg_status_4xx;\
    a[i].pg_status_5xx += b[i].pg_status_5xx;\
    a[i].pg_status_other += b[i].pg_status_other;\
    a[i].dns_time += b[i].dns_time;\
    a[i].dns_counts += b[i].dns_counts;\
    a[i].tcp_time += b[i].tcp_time;\
    a[i].tcp_counts += b[i].tcp_counts;\
    a[i].ssl_time += b[i].ssl_time;\
    a[i].ssl_counts += b[i].ssl_counts;\
    a[i].connect_time += b[i].connect_time;\
    a[i].connect_counts += b[i].connect_counts;\
    a[i].wait_time += b[i].wait_time;\
    a[i].wait_counts += b[i].wait_counts;\
    a[i].rcv_time += b[i].rcv_time;\
    a[i].rcv_counts += b[i].rcv_counts;\
    a[i].blckd_time += b[i].blckd_time;\
    a[i].blckd_counts += b[i].blckd_counts;\
    a[i].url_resp_time += b[i].url_resp_time;\
    a[i].url_resp_counts += b[i].url_resp_counts;\
    a[i].tbt += b[i].tbt;\
    a[i].tbt_counts += b[i].tbt_counts;\
    a[i].tbt_sum_sqr += b[i].tbt_sum_sqr;\
    a[i].lcp += b[i].lcp;\
    a[i].lcp_count += b[i].lcp_count;\
    a[i].lcp_sum_sqr += b[i].lcp_sum_sqr;\
    a[i].cls += b[i].cls;\
    a[i].cls_count += b[i].cls_count;\
    a[i].cls_sum_sqr += b[i].cls_sum_sqr;\  
  }
 

#define CHILD_RESET_RBU_PAGE_STAT_DATA_AVGTIME(a) \
  if(global_settings->browser_used != -1) { \
    for (i = 0; i < g_actual_num_pages; i++) {\
      a[i].DOMContent_Loaded_min_time = MAX_VALUE_4B_U;\
      a[i].DOMContent_Loaded_max_time = 0;\
      a[i].DOMContent_Loaded_counts = 0;\
      a[i].DOMContent_Loaded_time = 0;\
      a[i].DOMContent_Loaded_sum_sqr = 0;\
      a[i].OnLoad_min_time = MAX_VALUE_4B_U;\
      a[i].OnLoad_max_time = 0;\
      a[i].OnLoad_counts = 0;\
      a[i].OnLoad_time = 0;\
      a[i].OnLoad_sum_sqr = 0;\
      a[i].PageLoad_min_time = MAX_VALUE_4B_U;\
      a[i].PageLoad_max_time = 0;\
      a[i].PageLoad_counts = 0;\
      a[i].PageLoad_time = 0;\
      a[i].PageLoad_sum_sqr = 0;\
      a[i].TTI_min_time= MAX_VALUE_4B_U;\
      a[i].TTI_max_time = 0;\
      a[i].TTI_counts = 0;\
      a[i].TTI_time = 0;\
      a[i].TTI_sum_sqr = 0;\
      a[i]._cav_startRender_min_time= MAX_VALUE_4B_U;\
      a[i]._cav_startRender_max_time = 0;\
      a[i]._cav_startRender_counts = 0;\
      a[i]._cav_startRender_time = 0;\
      a[i]._cav_startRender_sum_sqr = 0;\
      a[i].visually_complete_min_time = MAX_VALUE_4B_U;\
      a[i].visually_complete_max_time = 0;\
      a[i].visually_complete_counts = 0;\
      a[i].visually_complete_time = 0;\
      a[i].visually_complete_sum_sqr = 0;\
      a[i].cur_rbu_requests_min = MAX_VALUE_4B_U;\
      a[i].cur_rbu_requests_max = 0;\
      a[i].cur_rbu_requests_counts = 0;\
      a[i].cur_rbu_requests = 0;\
      a[i].cur_rbu_requests_sum_sqr = 0;\
      a[i].cur_rbu_browser_cache_min = MAX_VALUE_4B_U;\
      a[i].cur_rbu_browser_cache_max = 0;\
      a[i].cur_rbu_browser_cache_count = 0;\
      a[i].cur_rbu_browser_cache = 0;\
      a[i].cur_rbu_browser_cache_sum_sqr = 0;\
      a[i].cur_rbu_bytes_recieved_min = MAX_VALUE_4B_U;\
      a[i].cur_rbu_bytes_recieved_max = 0;\
      a[i].cur_rbu_bytes_recieved_count = 0;\
      a[i].cur_rbu_bytes_recieved = 0;\
      a[i].cur_rbu_bytes_recieved_sum_sqr = 0;\
      a[i].cur_rbu_bytes_send_min = MAX_VALUE_4B_U;\
      a[i].cur_rbu_bytes_send_max = 0;\
      a[i].cur_rbu_bytes_send_count = 0;\
      a[i].cur_rbu_bytes_send = 0;\
      a[i].cur_rbu_bytes_send_sum_sqr = 0;\
      a[i].cur_rbu_page_wgt_min = MAX_VALUE_4B_U;\
      a[i].cur_rbu_page_wgt_max = 0;\
      a[i].cur_rbu_page_wgt_count = 0;\
      a[i].cur_rbu_page_wgt = 0;\
      a[i].cur_rbu_page_wgt_sum_sqr = 0;\
      a[i].cur_rbu_js_size_min = MAX_VALUE_4B_U;\
      a[i].cur_rbu_js_size_max = 0;\
      a[i].cur_rbu_js_count = 0;\
      a[i].cur_rbu_js_size = 0;\
      a[i].cur_rbu_js_sum_sqr = 0;\
      a[i].cur_rbu_css_size_min = MAX_VALUE_4B_U;\
      a[i].cur_rbu_css_size_max = 0;\
      a[i].cur_rbu_css_count = 0;\
      a[i].cur_rbu_css_size = 0;\
      a[i].cur_rbu_css_sum_sqr = 0;\
      a[i].cur_rbu_img_wgt_min = MAX_VALUE_4B_U;\
      a[i].cur_rbu_img_wgt_max = 0;\
      a[i].cur_rbu_img_wgt_count = 0;\
      a[i].cur_rbu_img_wgt = 0;\
      a[i].cur_rbu_img_wgt_sum_sqr = 0;\
      a[i].cur_rbu_dom_element_min = MAX_VALUE_4B_U;\
      a[i].cur_rbu_dom_element_max = 0;\
      a[i].cur_rbu_dom_element_count = 0;\
      a[i].cur_rbu_dom_element = 0;\
      a[i].cur_rbu_dom_element_sum_sqr = 0;\
      a[i].cur_rbu_pg_speed_min = MAX_VALUE_4B_U;\
      a[i].cur_rbu_pg_speed_max = 0;\
      a[i].cur_rbu_pg_speed_count = 0;\
      a[i].cur_rbu_pg_speed = 0;\
      a[i].cur_rbu_pg_speed_sum_sqr = 0;\
      a[i].cur_rbu_akamai_cache_min = MAX_VALUE_4B_U;\
      a[i].cur_rbu_akamai_cache_max = 0;\
      a[i].cur_rbu_akamai_cache_count = 0;\
      a[i].cur_rbu_akamai_cache = 0;\
      a[i].cur_rbu_akamai_cache_sum_sqr = 0;\
      a[i].cur_rbu_main_url_resp_time_min = MAX_VALUE_4B_U;\
      a[i].cur_rbu_main_url_resp_time_max = 0;\
      a[i].cur_rbu_main_url_resp_time_count = 0;\
      a[i].cur_rbu_main_url_resp_time = 0.0;\
      a[i].cur_rbu_main_url_resp_time_sum_sqr = 0;\
      a[i].pg_status_1xx = 0;\
      a[i].pg_status_2xx = 0;\
      a[i].pg_status_3xx = 0;\
      a[i].pg_status_4xx = 0;\
      a[i].pg_status_5xx = 0;\
      a[i].pg_status_other = 0;\
      a[i].dns_min_time = MAX_VALUE_4B_U;\
      a[i].dns_max_time = 0;\
      a[i].dns_counts = 0;\
      a[i].dns_time = 0;\
      a[i].tcp_min_time = MAX_VALUE_4B_U;\
      a[i].tcp_max_time = 0;\
      a[i].tcp_counts = 0;\
      a[i].tcp_time = 0;\
      a[i].ssl_min_time = MAX_VALUE_4B_U;\
      a[i].ssl_max_time = 0;\
      a[i].ssl_counts = 0;\
      a[i].ssl_time = 0;\
      a[i].connect_min_time = MAX_VALUE_4B_U;\
      a[i].connect_max_time = 0;\
      a[i].connect_counts = 0;\
      a[i].connect_time = 0;\
      a[i].wait_min_time = MAX_VALUE_4B_U;\
      a[i].wait_max_time = 0;\
      a[i].wait_counts = 0;\
      a[i].wait_time = 0;\
      a[i].rcv_min_time = MAX_VALUE_4B_U;\
      a[i].rcv_max_time = 0;\
      a[i].rcv_counts = 0;\
      a[i].rcv_time = 0;\
      a[i].blckd_min_time = MAX_VALUE_4B_U;\
      a[i].blckd_max_time = 0;\
      a[i].blckd_counts = 0;\
      a[i].blckd_time = 0;\
      a[i].url_resp_min_time = MAX_VALUE_4B_U;\
      a[i].url_resp_max_time = 0;\
      a[i].url_resp_counts = 0;\
      a[i].url_resp_time = 0;\
      a[i].tbt_min = MAX_VALUE_4B_U;\
      a[i].tbt_max = 0;\
      a[i].tbt_counts = 0;\
      a[i].tbt = 0;\
      a[i].tbt_sum_sqr = 0;\
      a[i].lcp_min = MAX_VALUE_4B_U;\
      a[i].lcp_max = 0;\
      a[i].lcp_count = 0;\
      a[i].lcp = 0;\
      a[i].lcp_sum_sqr = 0;\
      a[i].cls_min = MAX_VALUE_4B_U;\
      a[i].cls_max = 0;\
      a[i].cls_count = 0;\
      a[i].cls = 0;\
      a[i].cls_sum_sqr = 0;\
    }\
  }

//Storing data from RBU_RespAttr to RBUPageStatAvgTime for TimeStd graph
#define FILL_RBU_PAGE_STAT_AVG(src, dest, dest_min, dest_max, dest_count, src_sum_sqr, dest_sum_sqr) \
{ \
  src_val = rbu_resp_attr->src; \
  if(src_val < 0) \
    src_val = 0; \
  SET_MIN(rbu_page_stat_avg[page_idx_rbu_avg].dest_min, src_val); \
  SET_MAX(rbu_page_stat_avg[page_idx_rbu_avg].dest_max, src_val); \
  rbu_page_stat_avg[page_idx_rbu_avg].dest += src_val; \
  rbu_page_stat_avg[page_idx_rbu_avg].dest_count++; \
  src_sum_sqr = src_val*src_val; \
  rbu_page_stat_avg[page_idx_rbu_avg].dest_sum_sqr += src_sum_sqr; \
}

#define FILL_RBU_PAGE_STAT_AVG_FLOAT(src, dest, dest_min, dest_max, dest_count, src_sum_sqr, dest_sum_sqr) \
{ \
  float v = 0.0;\
  v = rbu_resp_attr->src; \
  if(v < 0) \
    v = 0; \
  SET_MIN(rbu_page_stat_avg[page_idx_rbu_avg].dest_min, v); \
  SET_MAX(rbu_page_stat_avg[page_idx_rbu_avg].dest_max, v); \
  rbu_page_stat_avg[page_idx_rbu_avg].dest += v; \
  rbu_page_stat_avg[page_idx_rbu_avg].dest_count++; \
  rbu_page_stat_avg[page_idx_rbu_avg].dest_sum_sqr += (v * v); \
}

#define FILL_RBU_PAGE_STAT_AVG_PG_AVAIL(src, dest) \
{ \
  src_val = rbu_resp_attr->src; \
  if(src_val < 0) \
    src_val = 0; \
  rbu_page_stat_avg[page_idx_rbu_avg].dest = src_val; \
}

#define FILL_RBU_PAGE_STAT_AVG_CUM(src, dest) \
{ \
  src_val = rbu_resp_attr->src; \
  if(src_val < 0) \
    src_val = 0; \
  rbu_page_stat_avg[page_idx_rbu_avg].dest += src_val; \
}

//Storing data from RBU_RespAttr to RBUPageStatAvgTime for Times graph
#define FILL_RBU_PAGE_STAT_AVG_TIMES_GRP(src, dest, dest_min, dest_max, dest_count) \
{ \
  src_val = rbu_resp_attr->src; \
  if(src_val < 0) \
    src_val = 0; \
  SET_MIN(rbu_page_stat_avg[page_idx_rbu_avg].dest_min, src_val); \
  SET_MAX(rbu_page_stat_avg[page_idx_rbu_avg].dest_max, src_val); \
  rbu_page_stat_avg[page_idx_rbu_avg].dest += src_val; \
  rbu_page_stat_avg[page_idx_rbu_avg].dest_count++; \
}

//Reset Page Status of RBUPageStatAvgTime structure
#define RESET_RBU_PAGE_STATUS_AVG(rbu_page) \
{ \
  rbu_page.pg_status_1xx = 0; \
  rbu_page.pg_status_2xx = 0; \
  rbu_page.pg_status_3xx = 0; \
  rbu_page.pg_status_4xx = 0; \
  rbu_page.pg_status_5xx = 0; \
  rbu_page.pg_status_other = 0; \
}

#define RBUPageStatAvgTime_Data_DUMP \
  NSDL2_RBU(NULL, NULL, "RBUPageStatAvgTime Data: page_idx_rbu_avg = %d, DOMContent_Loaded_time = %d, DOMContent_Loaded_ min_time = %d," \
                        "DOMContent_Loaded_max_time = %d, DOMContent_Loaded_counts = %d, OnLoad_time = %d, OnLoad_min_time = %d," \
                        "OnLoad_max_time = %d, OnLoad_counts = %d, PageLoad_time = %d, PageLoad_min_time = %d, PageLoad_max_time = %d," \
                        "PageLoad_counts = %d, TTI_time = %d, TTI_min_time = %u, TTI_max_time = %u, TTI_counts = %d," \
                        "_cav_startRender_time = %d, _cav_startRender_min_time = %u, _cav_startRender_max_time = %u," \
                        "_cav_startRender_counts = %d, visually_complete_time = %d, visually_complete_min_time = %u," \
                        "visually_complete_max_time = %u, visually_complete_counts = %d, request = %d, request_min = %d," \
                        "request_max = %d, request_count = %d, Browser Cache = %d, Bytes Received = %f, Byte Send = %f , "\
                        "main_url_resp_time = %d, pg_avail = %d, sess_completed = %d, sess_success = %d," \
                        "pg_status_1xx = %d, pg_status_2xx = %d, pg_status_3xx = %d, pg_status_4xx = %d, pg_status_5xx = %d, " \
                        "pg_status_other = %d, dns_time = %f, tcp_time = %f, ssl_time = %f, connect_time = %f," \
                        " wait_time = %f, rcv_time = %f, blckd_time = %f, url_resp_time = %f",\
                        " tbt = %f, lcp = %f, cls = %f",\
                        page_idx_rbu_avg, rbu_page_stat_avg[page_idx_rbu_avg].DOMContent_Loaded_time, \
                        rbu_page_stat_avg[page_idx_rbu_avg].DOMContent_Loaded_min_time, \
                        rbu_page_stat_avg[page_idx_rbu_avg].DOMContent_Loaded_max_time, \
                        rbu_page_stat_avg[page_idx_rbu_avg].DOMContent_Loaded_counts, \
                        rbu_page_stat_avg[page_idx_rbu_avg].OnLoad_time, rbu_page_stat_avg[page_idx_rbu_avg].OnLoad_min_time, \
                        rbu_page_stat_avg[page_idx_rbu_avg].OnLoad_max_time, \
                        rbu_page_stat_avg[page_idx_rbu_avg].OnLoad_counts, rbu_page_stat_avg[page_idx_rbu_avg].PageLoad_time, \
                        rbu_page_stat_avg[page_idx_rbu_avg].PageLoad_min_time, \
                        rbu_page_stat_avg[page_idx_rbu_avg].PageLoad_max_time, rbu_page_stat_avg[page_idx_rbu_avg].PageLoad_counts, \
                        rbu_page_stat_avg[page_idx_rbu_avg].TTI_time, \
                        rbu_page_stat_avg[page_idx_rbu_avg].TTI_min_time, rbu_page_stat_avg[page_idx_rbu_avg].TTI_max_time, \
                        rbu_page_stat_avg[page_idx_rbu_avg].TTI_counts, \
                        rbu_page_stat_avg[page_idx_rbu_avg]._cav_startRender_time, \
                        rbu_page_stat_avg[page_idx_rbu_avg]._cav_startRender_min_time, \
                        rbu_page_stat_avg[page_idx_rbu_avg]._cav_startRender_max_time, \
                        rbu_page_stat_avg[page_idx_rbu_avg]._cav_startRender_counts, \
                        rbu_page_stat_avg[page_idx_rbu_avg].visually_complete_time, \
                        rbu_page_stat_avg[page_idx_rbu_avg].visually_complete_min_time, \
                        rbu_page_stat_avg[page_idx_rbu_avg].visually_complete_max_time, \
                        rbu_page_stat_avg[page_idx_rbu_avg].visually_complete_counts, \
                        rbu_page_stat_avg[page_idx_rbu_avg].cur_rbu_requests, rbu_page_stat_avg[page_idx_rbu_avg].cur_rbu_requests_min, \
                        rbu_page_stat_avg[page_idx_rbu_avg].cur_rbu_requests_max, \
                        rbu_page_stat_avg[page_idx_rbu_avg].cur_rbu_requests_counts, \
                        rbu_page_stat_avg[page_idx_rbu_avg].cur_rbu_browser_cache, \
                        rbu_page_stat_avg[page_idx_rbu_avg].cur_rbu_bytes_recieved, \
                        rbu_page_stat_avg[page_idx_rbu_avg].cur_rbu_bytes_send, \
                        rbu_page_stat_avg[page_idx_rbu_avg].cur_rbu_main_url_resp_time, \
                        rbu_page_stat_avg[page_idx_rbu_avg].pg_avail, \
                        rbu_page_stat_avg[page_idx_rbu_avg].sess_completed, \
                        rbu_page_stat_avg[page_idx_rbu_avg].sess_success,\
                        rbu_page_stat_avg[page_idx_rbu_avg].pg_status_1xx, \
                        rbu_page_stat_avg[page_idx_rbu_avg].pg_status_2xx, \
                        rbu_page_stat_avg[page_idx_rbu_avg].pg_status_3xx, \
                        rbu_page_stat_avg[page_idx_rbu_avg].pg_status_4xx, \
                        rbu_page_stat_avg[page_idx_rbu_avg].pg_status_5xx, \
                        rbu_page_stat_avg[page_idx_rbu_avg].pg_status_other, \
                        rbu_page_stat_avg[page_idx_rbu_avg].dns_time, \
                        rbu_page_stat_avg[page_idx_rbu_avg].tcp_time, \
                        rbu_page_stat_avg[page_idx_rbu_avg].ssl_time, \
                        rbu_page_stat_avg[page_idx_rbu_avg].connect_time, \
                        rbu_page_stat_avg[page_idx_rbu_avg].wait_time, \
                        rbu_page_stat_avg[page_idx_rbu_avg].rcv_time, \
                        rbu_page_stat_avg[page_idx_rbu_avg].blckd_time, \
                        rbu_page_stat_avg[page_idx_rbu_avg].url_resp_time,\
                        rbu_page_stat_avg[page_idx_rbu_avg].tbt,\
                        rbu_page_stat_avg[page_idx_rbu_avg].lcp,\
                        rbu_page_stat_avg[page_idx_rbu_avg].cls);


// RBU Page Stat Data Structure
typedef struct RBU_Page_Stat_data_gp
{
  Times_std_data DOMContent_Loaded_time_gp;  //structure for the session pacing time
  Times_std_data OnLoad_time_gp;    // structure for the keep alive time out time 
  Times_std_data PageLoad_time_gp;    // structure for the keep alive time out time 
  Times_std_data TTI_time_gp;    // structure for the keep alive time out time 
  Times_std_data _cav_startRender_time_gp;    // structure for the keep alive time out time 
  Times_std_data visually_complete_time_gp;    // structure for the keep alive time out time 
  Times_std_data rbu_requests;
  Times_std_data rbu_browser_cache;
  Times_std_data rbu_bytes_recieved;
  Times_std_data rbu_bytes_send;
  //Extra attributes for extra parameter. 
  Times_std_data page_weight_gp;
  Times_std_data js_size_gp;
  Times_std_data css_size_gp;
  Times_std_data img_wgt_gp;
  Times_std_data dom_element_gp;
  Times_std_data pg_speed_gp;
  Times_std_data akamai_cache_gp;
  Times_std_data main_url_resp_time_gp;   //Back-end Time
  Long_data pg_avail;
  Long_data sess_completed;
  Long_data sess_success;
  Long_data pg_status_1xx;                    //Page Status Code
  Long_data pg_status_2xx;                    //Page Status Code
  Long_data pg_status_3xx;                    //Page Status Code
  Long_data pg_status_4xx;                    //Page Status Code
  Long_data pg_status_5xx;                    //Page Status Code
  Long_data pg_status_other;                  //Page Status Code
  Times_data dns_time_gp;                     //Overall DNS time
  Times_data tcp_time_gp;                     //Overall TCP time
  Times_data ssl_time_gp;                     //Overall SSL time 
  Times_data connect_time_gp;                 //Overall Connect time
  Times_data wait_time_gp;                    //Overall Wait time
  Times_data rcv_time_gp;                     //Overall Receive time
  Times_data blckd_time_gp;                   //Overall Blocked time
  Times_data url_resp_time_gp;                //Overall URL response time
  Times_std_data tbt_gp; // total blocking time
  Times_std_data lcp_gp; // largest contentful shift. 
  Times_std_data cls_gp; // cumulative layout shift

} RBU_Page_Stat_data_gp;

typedef struct RBUPageStatAvgTime
{
  int DOMContent_Loaded_counts;
  int DOMContent_Loaded_time;
  unsigned int DOMContent_Loaded_min_time;
  unsigned int DOMContent_Loaded_max_time;
  unsigned long long DOMContent_Loaded_sum_sqr;            //For Calculation of Std_Dev

  int OnLoad_counts;
  int OnLoad_time;
  unsigned int OnLoad_min_time;
  unsigned int OnLoad_max_time;
  unsigned long long OnLoad_sum_sqr;                       //For Calculation of Std_Dev

  int PageLoad_counts;
  int PageLoad_time;
  unsigned int PageLoad_min_time;
  unsigned int PageLoad_max_time;
  unsigned long long PageLoad_sum_sqr;                     //For Calculation of Std_Dev
  
  int TTI_counts;
  int TTI_time;
  unsigned int TTI_min_time;
  unsigned int TTI_max_time;
  unsigned long long TTI_sum_sqr;                          //For Calculation of Std_Dev

  int _cav_startRender_counts;
  int _cav_startRender_time;
  unsigned int _cav_startRender_min_time;
  unsigned int _cav_startRender_max_time;
  unsigned long long _cav_startRender_sum_sqr;             //For Calculation of Std_Dev

  int visually_complete_counts;
  int visually_complete_time;
  unsigned int visually_complete_min_time;
  unsigned int visually_complete_max_time;
  unsigned long long visually_complete_sum_sqr;            //For Calculation of Std_Dev

  int cur_rbu_requests_counts;
  int cur_rbu_requests;
  unsigned int cur_rbu_requests_min;
  unsigned int cur_rbu_requests_max;
  unsigned long long cur_rbu_requests_sum_sqr;             //For Calculation of Std_Dev

  int cur_rbu_browser_cache_count;
  int cur_rbu_browser_cache;
  unsigned int cur_rbu_browser_cache_min;
  unsigned int cur_rbu_browser_cache_max;
  unsigned long long cur_rbu_browser_cache_sum_sqr;        //For Calculation of Std_Dev

  int cur_rbu_bytes_recieved_count;
  float cur_rbu_bytes_recieved;
  float cur_rbu_bytes_recieved_min;
  float cur_rbu_bytes_recieved_max;
  unsigned long long cur_rbu_bytes_recieved_sum_sqr;       //For Calculation of Std_Dev

  int cur_rbu_bytes_send_count;
  float cur_rbu_bytes_send;
  float cur_rbu_bytes_send_min;
  float cur_rbu_bytes_send_max;
  unsigned long long cur_rbu_bytes_send_sum_sqr;           //For Calculation of Std_Dev
  
  int cur_rbu_page_wgt_count;
  int cur_rbu_page_wgt;
  unsigned int cur_rbu_page_wgt_min;
  unsigned int cur_rbu_page_wgt_max;
  unsigned long long cur_rbu_page_wgt_sum_sqr;             //For Calculation of Std_Dev

  int cur_rbu_js_count;
  float cur_rbu_js_size;
  float cur_rbu_js_size_min;
  float cur_rbu_js_size_max;
  unsigned long long cur_rbu_js_sum_sqr;                   //For Calculation of Std_Dev

  int cur_rbu_css_count;
  float cur_rbu_css_size;
  float cur_rbu_css_size_min;
  float cur_rbu_css_size_max;
  unsigned long long cur_rbu_css_sum_sqr;                  //For Calculation of Std_Dev

  int cur_rbu_img_wgt_count;
  float cur_rbu_img_wgt;
  float cur_rbu_img_wgt_min;
  float cur_rbu_img_wgt_max;
  unsigned long long cur_rbu_img_wgt_sum_sqr;              //For Calculation of Std_Dev

  int cur_rbu_dom_element_count;
  int cur_rbu_dom_element;
  unsigned int cur_rbu_dom_element_min;
  unsigned int cur_rbu_dom_element_max;
  unsigned long long cur_rbu_dom_element_sum_sqr;          //For Calculation of Std_Dev

  int cur_rbu_pg_speed_count;
  int cur_rbu_pg_speed;
  unsigned int cur_rbu_pg_speed_min;
  unsigned int cur_rbu_pg_speed_max;
  unsigned long long cur_rbu_pg_speed_sum_sqr;             //For Calculation of Std_Dev

  int cur_rbu_akamai_cache_count;
  float cur_rbu_akamai_cache;
  float cur_rbu_akamai_cache_min;
  float cur_rbu_akamai_cache_max;
  unsigned long long cur_rbu_akamai_cache_sum_sqr;         //For Calculation of Std_Dev
  
  int cur_rbu_main_url_resp_time_count;
  float cur_rbu_main_url_resp_time;
  float cur_rbu_main_url_resp_time_min;
  float cur_rbu_main_url_resp_time_max;
  unsigned long long cur_rbu_main_url_resp_time_sum_sqr;   //For Calculation of Std_Dev

  int pg_avail; 
  int sess_completed;
  int sess_success;
  int pg_status_1xx;                                          // For Status Code of Page 
  int pg_status_2xx;                                          // For Status Code of Page 
  int pg_status_3xx;                                          // For Status Code of Page 
  int pg_status_4xx;                                          // For Status Code of Page 
  int pg_status_5xx;                                          // For Status Code of Page 
  int pg_status_other;                                          // For Status Code of Page 

  int dns_counts;                                             //DNS timing attributes
  float dns_time;
  float dns_min_time;
  float dns_max_time;

  int tcp_counts;                                             //TCP timing attributes
  float tcp_time;
  float tcp_min_time;
  float tcp_max_time;

  int ssl_counts;                                              //SSL timing attributes
  float ssl_time;
  float ssl_min_time;
  float ssl_max_time;

  int connect_counts;                                          //Connect timing attributes
  float connect_time;
  float connect_min_time;
  float connect_max_time;

  int wait_counts;                                            //Wait timing attributes
  float wait_time;
  float wait_min_time;
  float wait_max_time;

  int rcv_counts;                                             //Receive timing attributes
  float rcv_time;
  float rcv_min_time;
  float rcv_max_time;

  int blckd_counts;                                           //Blocked timing attributes
  float blckd_time;
  float blckd_min_time;
  float blckd_max_time;

  int url_resp_counts;                                        //URL timing attributes
  float url_resp_time;
  float url_resp_min_time;
  float url_resp_max_time;

  int tbt_counts; // total blocking time;
  int tbt;
  unsigned int tbt_min;
  unsigned int tbt_max;
  unsigned long long tbt_sum_sqr;

  int lcp_count; // largest contentful time. 
  int lcp;
  unsigned int lcp_min;
  unsigned int lcp_max;
  unsigned long long lcp_sum_sqr;

  int cls_count; // cumulative layout shift. 
  float cls;
  float cls_min;
  float cls_max;
  unsigned long long cls_sum_sqr;

} RBUPageStatAvgTime;

extern RBU_Page_Stat_data_gp *rbu_page_stat_data_gp_ptr;
#ifndef CAV_MAIN
extern RBUPageStatAvgTime *rbu_page_stat_avg;
extern unsigned int rbu_page_stat_data_gp_idx;
#else
extern __thread RBUPageStatAvgTime *rbu_page_stat_avg;
extern __thread unsigned int rbu_page_stat_data_gp_idx;
#endif
extern unsigned int rbu_page_stat_data_idx;
extern unsigned int rbu_resp_attr_idx;
extern char **printRBUPageStat();
extern char **init_2d(int no_of_host);
extern void fill_2d(char **TwoD, int i, char *fill_data);
extern inline void update_rbu_page_stat_data_avgtime_size();
extern inline void set_rbu_page_stat_data_avgtime_ptr();
extern inline void set_rbu_page_stat_data_avgtime_data();
extern inline void set_rbu_page_stat_data_avgtime_data_sess_only();
extern inline void parse_and_set_rbu_page_stat();
extern void fill_rbu_page_stat_gp(avgtime **rbu_page_stat_avg);
extern inline void set_rbu_page_status_data_avgtime_data();

extern inline void ns_get_rbu_settings(int grp_idx, char *out_msg);
extern int create_csv_data();
#endif
