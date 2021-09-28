#ifndef ns_test_monitor_h__
#define ns_test_monitor_h__

#define SM_HTTP_API_CSV              "sm_http_api.csv"
#define SM_WEB_PAGE_AUDIT_CSV        "sm_web_page_audit.csv"
#define MAX_MON_NAME_LENGTH      128

enum monitor_type
{
  MONITOR_TYPE_NONE,
  HTTP_API,
  WEB_PAGE_AUDIT,
  MONITOR_TYPE_MAX
};

enum monitor_data_type
{
  RTG_DATA,
  HTTP_REQUESTS,
  HTTP_RESPONSE,
  HAR_FILE,
  CHECK_POINT
};

typedef struct{

  //Total Time
  u_ns_8B_t total_tot_time; //Total Time(Main + Emd) = RESOLVE + CONNECT + SSL + SEND + FIRST_BYTE + DOWNLOAD
  u_ns_8B_t total_min_time;
  u_ns_8B_t total_max_time;
  u_ns_8B_t total_time_count;

  //Send Time
  u_ns_8B_t send_tot_time;
  u_ns_8B_t send_min_time;
  u_ns_8B_t send_max_time;
  u_ns_8B_t send_time_count;

  //Redirect Time
  u_ns_8B_t redirect_tot_time; //Total Time of Redirected URLs only
  u_ns_8B_t redirect_min_time;
  u_ns_8B_t redirect_max_time;
  u_ns_8B_t redirect_time_count;

  //Request Body Size 
  u_ns_8B_t req_body_size;

  //Status Code
  int status_code;

  //Availability Status
  int avail_status;

  //Unavailability Reason 
  int unavail_reason;

}CavTestHTTPAvgTime;

typedef struct{

  //Send Time
  u_ns_8B_t send_tot_time;
  u_ns_8B_t send_min_time;
  u_ns_8B_t send_max_time;
  u_ns_8B_t send_time_count;

  //Redirect Time
  u_ns_8B_t redirect_tot_time;
  u_ns_8B_t redirect_min_time;
  u_ns_8B_t redirect_max_time;
  u_ns_8B_t redirect_time_count;

  //Request Header Size 
  u_ns_8B_t req_header_size;

  //Response Header Size 
  u_ns_8B_t res_header_size;

  //Status Code
  int status_code;

  //Unavailability Reason 
  int unavail_reason;

}CavTestWebAvgTime;


#define FILL_CAVTEST_HTTP_API_DATA_PERIODIC(a, b){                                         \
  SET_MIN ((a)->total_min_time, (b)->total_min_time);                                      \
  SET_MAX ((a)->total_max_time, (b)->total_max_time);                                      \
  SET_MIN ((a)->send_min_time, (b)->send_min_time);                                        \
  SET_MAX ((a)->send_max_time, (b)->send_max_time);                                        \
  SET_MIN ((a)->redirect_min_time, (b)->redirect_min_time);                                \
  SET_MAX ((a)->redirect_max_time, (b)->redirect_max_time);                                \
  (a)->total_tot_time += (b)->total_tot_time;                                              \
  (a)->total_time_count += (b)->total_time_count;                                          \
  (a)->send_tot_time += (b)->send_tot_time;                                                \
  (a)->send_time_count += (b)->send_time_count;                                            \
  (a)->redirect_tot_time += (b)->redirect_tot_time;                                        \
  (a)->redirect_time_count += (b)->redirect_time_count;                                    \
  (a)->req_body_size += (b)->req_body_size;                                                \
  (a)->status_code = (b)->status_code;                                                     \
  (a)->avail_status = (b)->avail_status;                                                   \
  (a)->unavail_reason = (b)->unavail_reason;                                               \
}

#define CHILD_RESET_CAVTEST_HTTP_API_AVGTIME(a){                                           \
  CavTestHTTPAvgTime *loc_cavtest_http_api_avgtime;                                        \
  loc_cavtest_http_api_avgtime = (CavTestHTTPAvgTime*)((char*)a + g_cavtest_http_avg_idx); \
  loc_cavtest_http_api_avgtime->total_tot_time = 0;                                        \
  loc_cavtest_http_api_avgtime->total_min_time = 0xFFFFFFFF;                               \
  loc_cavtest_http_api_avgtime->total_max_time = 0;                                        \
  loc_cavtest_http_api_avgtime->total_time_count = 0;                                      \
  loc_cavtest_http_api_avgtime->send_tot_time = 0;                                         \
  loc_cavtest_http_api_avgtime->send_min_time = 0xFFFFFFFF;                                \
  loc_cavtest_http_api_avgtime->send_max_time = 0;                                         \
  loc_cavtest_http_api_avgtime->send_time_count = 0;                                       \
  loc_cavtest_http_api_avgtime->redirect_tot_time = 0;                                     \
  loc_cavtest_http_api_avgtime->redirect_min_time = 0xFFFFFFFF;                            \
  loc_cavtest_http_api_avgtime->redirect_max_time = 0;                                     \
  loc_cavtest_http_api_avgtime->redirect_time_count = 0;                                   \
  loc_cavtest_http_api_avgtime->req_body_size = 0;                                         \
  loc_cavtest_http_api_avgtime->status_code = 0;                                           \
  loc_cavtest_http_api_avgtime->avail_status = 0;                                          \
  loc_cavtest_http_api_avgtime->unavail_reason = 0;                                        \
}

#define CHILD_RESET_CAVTEST_WEB_PAGE_AUDIT_AVGTIME(a){                                     \
  CavTestWebAvgTime *loc_cavtest_web_avgtime;                                              \
  loc_cavtest_web_avgtime = (CavTestWebAvgTime*)((char*)a + g_cavtest_web_avg_idx);        \
  loc_cavtest_web_avgtime->send_tot_time = 0;                                              \
  loc_cavtest_web_avgtime->send_min_time = 0xFFFFFFFF;                                     \
  loc_cavtest_web_avgtime->send_max_time = 0;                                              \
  loc_cavtest_web_avgtime->send_time_count = 0;                                            \
  loc_cavtest_web_avgtime->redirect_tot_time = 0;                                          \
  loc_cavtest_web_avgtime->redirect_min_time = 0xFFFFFFFF;                                 \
  loc_cavtest_web_avgtime->redirect_max_time = 0;                                          \
  loc_cavtest_web_avgtime->redirect_time_count = 0;                                        \
  loc_cavtest_web_avgtime->req_header_size = 0;                                            \
  loc_cavtest_web_avgtime->res_header_size = 0;                                            \
  loc_cavtest_web_avgtime->status_code = 0;                                                \
  loc_cavtest_web_avgtime->unavail_reason = 0;                                             \
}

#define FILL_CAVTEST_WEB_PAGE_AUDIT_DATA_PERIODIC(a, b){                                   \
  SET_MIN ((a)->send_min_time, (b)->send_min_time);                                        \
  SET_MAX ((a)->send_max_time, (b)->send_max_time);                                        \
  SET_MIN ((a)->redirect_min_time, (b)->redirect_min_time);                                \
  SET_MAX ((a)->redirect_max_time, (b)->redirect_max_time);                                \
  (a)->send_tot_time += (b)->send_tot_time;                                                \
  (a)->send_time_count += (b)->send_time_count;                                            \
  (a)->redirect_tot_time += (b)->redirect_tot_time;                                        \
  (a)->redirect_time_count += (b)->redirect_time_count;                                    \
  (a)->req_header_size += (b)->req_header_size;                                            \
  (a)->res_header_size += (b)->res_header_size;                                            \
  (a)->status_code = (b)->status_code;                                                     \
  (a)->unavail_reason = (b)->unavail_reason;                                               \
}
#ifndef CAV_MAIN
extern int g_cavtest_http_avg_idx;
extern CavTestHTTPAvgTime *cavtest_http_avg;
extern int g_cavtest_web_avg_idx;
extern CavTestWebAvgTime *cavtest_web_avg;
#else
extern __thread int g_cavtest_http_avg_idx;
extern __thread CavTestHTTPAvgTime *cavtest_http_avg;
extern __thread int g_cavtest_web_avg_idx;
extern __thread CavTestWebAvgTime *cavtest_web_avg;
#endif
extern int kw_set_test_monitor_config(char*, char*, int);
extern void send_test_monitor_gdf_data(VUser *, int, avgtime **, cavgtime **);

extern inline void update_avgtime_size_for_cavtest();
extern inline void set_cavtest_data_avgtime_ptr();

void cavtest_log_rep(VUser *vptr, char *buf, int byte_log, int complete_data);
#endif //ns_test_monitor_h__
