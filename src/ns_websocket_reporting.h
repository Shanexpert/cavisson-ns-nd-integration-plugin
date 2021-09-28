#ifndef NS_WS_REPORTING
#define NS_WS_REPORTING

// Group Data Structure
typedef struct WSStats_gp
{
  Long_data ws_stats_initiated_conn_per_sec;      //1.  Number of WS connections initiated per second.
  Long_data ws_stats_tot_initiated_conn;          //2.  Total number of WS connections initiated.
  Long_data ws_stats_established_conn_per_sec;    //3.  Number of WS connections opened per second in the sampling period
  Long_data ws_stats_tot_established_conn;        //4.  Total number of WS connections opened.
  Long_data ws_stats_closed_conn_per_sec;         //5.  Number of WS connections closed per second in the sampling period.
  Long_data ws_stats_tot_closed_conn;             //6.  Total number of WS connections closed.
  Long_data ws_stats_failed_conn_per_sec;         //7.  Number of WS connections failed per second in the sampling period.
  Long_data ws_stats_tot_failed_conn;             //8.  Number of WS connections failed.
  Long_data ws_stats_msg_sent_per_sec;            //9.  Number of WS message sent per second in the sampling period.
  Long_data ws_stats_tot_msg_sent;                //10. Total number of WS message sent.
  Long_data ws_stats_msg_read_per_sec;            //11. Number of WS message read per second in the sampling period.
  Long_data ws_stats_tot_msg_read;                //12. Total number of WS message read.
  Long_data ws_stats_msg_send_failed_per_sec;     //13. Number of WS message send failed per second in the sampling period.
  Long_data ws_stats_tot_msg_send_failed;         //14. Total number of WS message send failed.
  Long_data ws_stats_msg_read_failed_per_sec;     //15. Number of WS message read failed per second in the sampling period.
  Long_data ws_stats_tot_msg_read_failed;         //16. Total number of WS message failed.
  Long_data ws_stats_tot_bytes_send_per_sec;      //17. Total number of WS bytes send per second in the sampling period.
  Long_data ws_stats_tot_bytes_recv_per_sec;      //18. Total number of WS bytes received per second in the sampling period.
} WSStats_gp;

//Structure of Websocket Avg
typedef struct {
  //int ws_num_connections;

  u_ns_8B_t ws_tx_bytes;            //Send bytes
  u_ns_8B_t ws_rx_bytes;            //Receive bytes

  u_ns_8B_t ws_num_con_initiated;   //Number of initiated requests for making the connection (periodic)
  u_ns_8B_t ws_num_con_succ;        //Number of succ requests for making the connection out of initiated (periodic)
  u_ns_8B_t ws_num_con_fail;        //Number of failures requests for making the connection out of initiated (periodic)
  u_ns_8B_t ws_num_con_break;       //Number of initiated requests for closing the connection (periodic)

  u_ns_8B_t ws_num_hits;            
  u_ns_8B_t ws_num_tries;

  u_ns_8B_t ws_avg_time;
  u_ns_8B_t ws_min_time;
  u_ns_8B_t ws_max_time;
  u_ns_8B_t ws_tot_time;

  u_ns_8B_t ws_fetches_started;
  u_ns_8B_t ws_num_msg_send;        //Count of sending msg
  u_ns_8B_t ws_num_msg_read;        //Count of receiving msg
  u_ns_8B_t ws_num_msg_send_fail;   //Count of send msg
  u_ns_8B_t ws_num_msg_read_fail;   //Count of read msg


  u_ns_4B_t ws_response[MAX_GRANULES+1];
  u_ns_4B_t ws_error_codes[TOTAL_URL_ERR];   /* Total error codes for WSFailureStats is 29 */
  u_ns_4B_t ws_status_codes[41];             /* Presently they are 41 will have to increase it in case will have to add more. */

} WSAvgTime;  


// Cumulative counters - Used by parent ONLY
typedef struct {
  u_ns_8B_t ws_c_num_tx_bytes;       //Total no. of sent bytes
  u_ns_8B_t ws_c_num_rx_bytes;       //Total no. of received bytes

  u_ns_8B_t ws_c_num_con_initiated;  //Total no. of initiated con, which can be open/close 
  u_ns_8B_t ws_c_num_con_succ;       //Total no. of open successful con.
  u_ns_8B_t ws_c_num_con_fail;       //Total no. of failed con.
  u_ns_8B_t ws_c_num_con_break;      //Total no. of close con.

  u_ns_8B_t ws_succ_fetches;         //Filled data from ws_num_hits
  u_ns_8B_t ws_fetches_completed;    //Filled data from ws_num_tries

  u_ns_8B_t ws_c_min_time;
  u_ns_8B_t ws_c_max_time;
  u_ns_8B_t ws_c_tot_time;
  u_ns_8B_t ws_c_avg_time;

  u_ns_8B_t cum_ws_fetches_started;  //Total no. of con is going to fetch (before handle_connect)
  u_ns_8B_t ws_c_num_msg_send;       //Total count of send msg
  u_ns_8B_t ws_c_num_msg_read;       //Total count of fail msg
  u_ns_8B_t ws_c_num_msg_send_fail;  //Total count of send msg
  u_ns_8B_t ws_c_num_msg_read_fail;  //Total count of read msg

  u_ns_4B_t cum_ws_error_codes[TOTAL_URL_ERR];   /* for url error codes 1-8 */
  u_ns_4B_t cum_ws_status_codes[41];

} WSCAvgTime;

//WS Resp Codes
typedef struct WSStatusCodes_gp{
  Long_data ws_status_codes[41];   // 41 http status codes in gdf 
} WSStatusCodes_gp;

//WS Failure Stats
typedef struct WSFailureStats_gp{
  Long_data ws_failures;   // 41 http status codes in gdf
} WSFailureStats_gp;



#define IS_WS_ENABLED (global_settings->protocol_enabled & WS_PROTOCOL_ENABLED)

#define GET_WS_AVG(vptr, local_ws_avg) \
  local_ws_avg = (WSAvgTime*)((char *)average_time + (vptr->group_num + 1) * g_avg_size_only_grp + g_ws_avgtime_idx);

#define SET_MIN_MAX_WS_CUMULATIVES(a, b)\
  SET_MIN ((a)->ws_c_min_time, (b)->ws_min_time);\
  SET_MAX ((a)->ws_c_max_time, (b)->ws_max_time);\
  NSDL2_MESSAGES(NULL, NULL, "SET_MIN_MAX_WS_CUMULATIVES, ws_c_min_time = %llu, ws_c_max_time = %llu", (a)->ws_c_min_time, (b)->ws_min_time);\
      
#define SET_MIN_MAX_WS_CUMULATIVES_PARENT(a, b)\
  SET_MIN ((a)->ws_c_min_time, (b)->ws_c_min_time);\
  SET_MAX ((a)->ws_c_max_time, (b)->ws_c_max_time);\
  NSDL2_MESSAGES(NULL, NULL, "SET_MIN_MAX_WS_CUMULATIVES_PARENT, ws_c_min_time = %llu, ws_c_max_time = %llu", (a)->ws_c_min_time, (b)->ws_c_min_time);\

//Set  periodic elements of ws struct a with b into a
#define SET_MIN_MAX_WS_PERIODICS(a, b)\
  SET_MIN ((a)->ws_min_time, (b)->ws_min_time);\
  SET_MAX ((a)->ws_max_time, (b)->ws_max_time);\

#define ACC_WS_PERIODICS(a, b)\
  (a)->ws_tx_bytes += (b)->ws_tx_bytes;\
  (a)->ws_rx_bytes += (b)->ws_rx_bytes;\
  (a)->ws_num_con_initiated += (b)->ws_num_con_initiated;\
  (a)->ws_num_con_succ += (b)->ws_num_con_succ;\
  (a)->ws_num_con_fail += (b)->ws_num_con_fail;\
  (a)->ws_num_con_break += (b)->ws_num_con_break;\
  (a)->ws_num_hits += (b)->ws_num_hits;\
  (a)->ws_num_tries += (b)->ws_num_tries;\
  (a)->ws_avg_time += (b)->ws_avg_time;\
  (a)->ws_tot_time += (b)->ws_tot_time;\
  (a)->ws_fetches_started += (b)->ws_fetches_started;\
  (a)->ws_num_msg_send += (b)->ws_num_msg_send; \
  (a)->ws_num_msg_read += (b)->ws_num_msg_read; \
  (a)->ws_num_msg_read_fail += (b)->ws_num_msg_read_fail; \
  (a)->ws_num_msg_send_fail += (b)->ws_num_msg_send_fail; \
   NSDL2_WS(NULL, NULL, "ACC_WS_PERIODICS: ws_num_msg_send = %llu, ws_num_msg_read = %llu, ws_num_msg_read_fail = %llu, ws_num_tries = %llu, ws_fetches_started = %llu", (a)->ws_num_msg_send, (a)->ws_num_msg_read, (a)->ws_num_msg_read_fail, (a)->ws_num_tries, (a)->ws_fetches_started); \
  for (i = 0; i < TOTAL_URL_ERR; i++) {         /* Error codes for WSFailureStats */ \
   (a)->ws_error_codes[i] += (b)->ws_error_codes[i];\
  } \
  for (i = 0; i < total_ws_status_codes; i++) { \
    (a)->ws_status_codes[i] += (b)->ws_status_codes[i]; \
  }

//accumulate cumulative elements of WS struct a with b into a 
#define ACC_WS_CUMULATIVES(a, b)\
  (a)->ws_c_num_tx_bytes += (b)->ws_tx_bytes;\
  (a)->ws_c_num_rx_bytes += (b)->ws_rx_bytes; \
  (a)->ws_c_num_con_initiated += (b)->ws_num_con_initiated;\
  (a)->ws_c_num_con_succ += (b)->ws_num_con_succ;\
  (a)->ws_c_num_con_fail += (b)->ws_num_con_fail;\
  (a)->ws_c_num_con_break += (b)->ws_num_con_break;\
  (a)->ws_succ_fetches += (b)->ws_num_hits;\
  (a)->ws_fetches_completed += (b)->ws_num_tries;\
  (a)->cum_ws_fetches_started += (b)->ws_fetches_started;\
  (a)->ws_c_num_msg_send += (b)->ws_num_msg_send;\
  (a)->ws_c_num_msg_read += (b)->ws_num_msg_read;\
  (a)->ws_c_num_msg_read_fail += (b)->ws_num_msg_read_fail;\
  (a)->ws_c_num_msg_send_fail += (b)->ws_num_msg_send_fail;\
  NSDL2_WS(NULL, NULL, "ACC_WS_CUMULATIVES: ws_c_num_msg_send = %llu, ws_c_num_msg_read = %llu, ws_c_num_msg_read_fail = %llu, ws_num_tries = %llu, ws_fetches_started = %llu", (a)->ws_c_num_msg_send, (a)->ws_c_num_msg_read, (a)->ws_c_num_msg_read_fail, (b)->ws_num_tries, (b)->ws_fetches_started);\
  for (i = 0; i < TOTAL_URL_ERR; i++) {\
    (a)->cum_ws_error_codes[i] += (b)->ws_error_codes[i];\
  } \
  for (i = 0; i < total_ws_status_codes; i++) {\
    (a)->cum_ws_status_codes[i] += (b)->ws_status_codes[i];\
  }

#define ACC_WS_CUMULATIVES_PARENT(a, b)\
  (a)->ws_c_num_tx_bytes += (b)->ws_c_num_tx_bytes;\
  (a)->ws_c_num_rx_bytes += (b)->ws_c_num_rx_bytes;\
  (a)->ws_c_tot_time += (b)->ws_c_tot_time;\
  (a)->ws_c_num_con_initiated += (b)->ws_c_num_con_initiated;\
  (a)->ws_c_num_con_succ += (b)->ws_c_num_con_succ;\
  (a)->ws_c_num_con_fail += (b)->ws_c_num_con_fail;\
  (a)->ws_c_num_con_break += (b)->ws_c_num_con_break;\
  (a)->ws_succ_fetches += (b)->ws_succ_fetches;\
  (a)->ws_fetches_completed += (b)->ws_fetches_completed;\
  (a)->ws_c_num_msg_send += (b)->ws_c_num_msg_send;\
  (a)->ws_c_num_msg_read += (b)->ws_c_num_msg_read;\
  (a)->ws_c_num_msg_read_fail += (b)->ws_c_num_msg_read_fail;\
  (a)->ws_c_num_msg_send_fail += (b)->ws_c_num_msg_send_fail;\
  (a)->cum_ws_fetches_started += (b)->cum_ws_fetches_started;\
  NSDL2_WS(NULL, NULL, "ACC_WS_CUMULATIVES_PARENT: ws_c_num_msg_send = %llu, ws_c_num_msg_read = %llu, ws_c_num_msg_read_fail = %llu, cum_ws_fetches_started = %llu", (a)->ws_c_num_msg_send, (a)->ws_c_num_msg_read, (a)->ws_c_num_msg_read_fail, (a)->cum_ws_fetches_started);\
  for (i = 0; i < TOTAL_URL_ERR; i++) {\
  } \
  for (i = 0; i < total_ws_status_codes; i++) {\
    (a)->cum_ws_status_codes[i] += (b)->cum_ws_status_codes[i];\
  } 

#define CHILD_RESET_WS_AVGTIME(a) \
  if(global_settings->protocol_enabled & WS_PROTOCOL_ENABLED) { \
    WSAvgTime *loc_ws_avgtime; \
    loc_ws_avgtime = (WSAvgTime*)((char*)a + g_ws_avgtime_idx); \
    memset(loc_ws_avgtime->ws_error_codes, 0, TOTAL_URL_ERR * sizeof(int)); \
    memset(loc_ws_avgtime->ws_status_codes, 0, total_ws_status_codes * sizeof(int)); \
    NSDL2_WS(NULL, NULL, "Reset child loc_ws_avgtime, loc_ws_avgtime = %p", loc_ws_avgtime);\
    loc_ws_avgtime->ws_tx_bytes = 0;\
    loc_ws_avgtime->ws_rx_bytes = 0;\
    loc_ws_avgtime->ws_num_con_initiated = 0;\
    loc_ws_avgtime->ws_num_con_succ = 0;\
    loc_ws_avgtime->ws_num_con_fail = 0;\
    loc_ws_avgtime->ws_num_con_break = 0;\
    loc_ws_avgtime->ws_num_hits = 0;\
    loc_ws_avgtime->ws_num_tries = 0;\
    loc_ws_avgtime->ws_avg_time = 0;\
    loc_ws_avgtime->ws_min_time = 0xFFFFFFFF;\
    loc_ws_avgtime->ws_max_time = 0;\
    loc_ws_avgtime->ws_tot_time = 0;\
    loc_ws_avgtime->ws_fetches_started = 0;\
    loc_ws_avgtime->ws_fetches_started = 0; \
    loc_ws_avgtime->ws_num_msg_send = 0; \
    loc_ws_avgtime->ws_num_msg_read = 0; \
    loc_ws_avgtime->ws_num_msg_send_fail = 0; \
    loc_ws_avgtime->ws_num_msg_read_fail = 0; \
  }

#define INC_WS_WSS_INITIATED_CONN_COUNTERS(vptr) \
{ \
  (ws_avgtime->ws_num_con_initiated)++; \
  NSDL2_WS(NULL, NULL, "ws_avgtime->ws_num_con_initiated = %llu", ws_avgtime->ws_num_con_initiated); \
  if(SHOW_GRP_DATA) { \
    WSAvgTime *local_ws_avg = NULL; \
    GET_WS_AVG(vptr, local_ws_avg); \
    (local_ws_avg->ws_num_con_initiated)++; \
  } \
}

#define INC_WS_WSS_CONN_FAIL_COUNTERS(vptr) \
{ \
  (ws_avgtime->ws_num_con_fail)++; \
  NSDL2_WS(NULL, NULL, "ws_avgtime->ws_num_con_fail = %llu", ws_avgtime->ws_num_con_fail); \
  if(SHOW_GRP_DATA) { \
    WSAvgTime *local_ws_avg = NULL; \
    GET_WS_AVG(vptr, local_ws_avg); \
    (local_ws_avg->ws_num_con_fail)++; \
  } \
}


#define DEC_WS_WSS_CONN_COUNTERS(vptr) \
{ \
  (ws_avgtime->ws_num_con_break)++; \
  NSDL2_PARSING(NULL, NULL, "ws_avgtime->ws_num_con_break = %llu", ws_avgtime->ws_num_con_break); \
  if(SHOW_GRP_DATA) { \
    WSAvgTime *local_ws_avg = NULL;\
    GET_WS_AVG(vptr, local_ws_avg); \
    (local_ws_avg->ws_num_con_break)++; \
  } \
}

#define INC_WS_WSS_CONN_COUNTERS(vptr) \
{ \
  (ws_avgtime->ws_num_con_succ)++; \
  NSDL2_PARSING(NULL, NULL, "ws_avgtime->ws_num_con_succ = %llu", ws_avgtime->ws_num_con_succ); \
  if(SHOW_GRP_DATA) { \
    WSAvgTime *local_ws_avg = NULL; \
    GET_WS_AVG(vptr, local_ws_avg); \
    (local_ws_avg->ws_num_con_succ)++; \
  } \
}

#define INC_WS_TX_BYTES_COUNTER(vptr, bytes_sent) \
{ \
  ws_avgtime->ws_tx_bytes += bytes_sent; \
  ws_avgtime->ws_num_msg_send++; \
  NSDL2_WS(NULL, NULL, "ws_tx_bytes = %llu", ws_avgtime->ws_tx_bytes); \
  if(SHOW_GRP_DATA) { \
    WSAvgTime *local_ws_avg = NULL; \
    GET_WS_AVG(vptr, local_ws_avg); \
    local_ws_avg->ws_tx_bytes += bytes_sent; \
  } \
}

#define INC_WS_WSS_FETCHES_STARTED_COUNTER(vptr) \
{ \
  ws_avgtime->ws_fetches_started++; \
  if(SHOW_GRP_DATA) { \
    WSAvgTime *local_ws_avg = NULL; \
    GET_WS_AVG(vptr, local_ws_avg); \
    local_ws_avg->ws_fetches_started++; \
  } \
}

#define INC_WS_WSS_NUM_TRIES_COUNTER(vptr) \
{ \
  ws_avgtime->ws_num_tries++; \
  /*ws_avgtime->ws_error_codes[status]++;*/ \
  if(status == NS_REQUEST_CONFAIL) \
  { \
  ws_avgtime->ws_error_codes[status]++; \
  } \
  NSDL2_PARSING(NULL, NULL, "average_time->ws_num_tries = %llu, ws_avgtime->ws_error_codes[%d] = %llu, vptr->group_num = %d", ws_avgtime->ws_num_tries, status, ws_avgtime->ws_error_codes[status], vptr->group_num); \
  if(SHOW_GRP_DATA) { \
    WSAvgTime *local_ws_avg = NULL;\
    GET_WS_AVG(vptr, local_ws_avg); \
    local_ws_avg->ws_num_tries++; \
    local_ws_avg->ws_error_codes[status]++; \
    NSDL2_PARSING(NULL, NULL, "local_ws_avg = %p, ws_num_tries = %llu, ws_error_codes[%d] = %llu, vptr->group_num = %d", local_ws_avg, local_ws_avg->ws_num_tries, status, local_ws_avg->ws_error_codes[status], vptr->group_num); \
  } \
}

#define FILL_WS_TOT_TIME_FOR_GROUP_BASED(vptr) \
{ \
  if(SHOW_GRP_DATA) \
  { \
    WSAvgTime *local_ws_avg = NULL; \
    GET_WS_AVG(vptr, local_ws_avg); \
    if (download_time < local_ws_avg->ws_min_time) { \
      local_ws_avg->ws_min_time = download_time; \
    } \
    if (download_time > local_ws_avg->ws_max_time) { \
      local_ws_avg->ws_max_time = download_time; \
    } \
    local_ws_avg->ws_tot_time += download_time; \
    local_ws_avg->ws_num_hits++; \
  } \
}

#define INC_WS_RX_BYTES_COUNTER(vptr, bytes_handled) \
{ \
  ws_avgtime->ws_rx_bytes += bytes_handled; \
  ws_avgtime->ws_num_msg_read++; \
  NSDL2_WS(NULL, NULL, "ws_rx_bytes = %llu, ws_num_msg_read = %llu", ws_avgtime->ws_rx_bytes, ws_avgtime->ws_num_msg_read); \
  if(SHOW_GRP_DATA) { \
    WSAvgTime *local_ws_avg = NULL; \
    GET_WS_AVG(vptr, local_ws_avg); \
    local_ws_avg->ws_rx_bytes += bytes_handled; \
  } \
} 

#define INC_WS_MSG_SEND_FAIL_COUNTER(vptr) \
{ \
  ws_avgtime->ws_num_msg_send_fail++; \
  NSDL2_WS(NULL, NULL, "ws_num_msg_send_fail = %llu", ws_avgtime->ws_num_msg_send_fail); \
  if(SHOW_GRP_DATA) { \
    WSAvgTime *local_ws_avg = NULL; \
    GET_WS_AVG(vptr, local_ws_avg); \
    local_ws_avg->ws_num_msg_send_fail++; \
  } \
} 

#define INC_WS_MSG_READ_FAIL_COUNTER(vptr) \
{ \
  (ws_avgtime->ws_num_msg_read_fail++); \
  NSDL2_WS(NULL, NULL, "ws_num_msg_read_fail = %llu", ws_avgtime->ws_num_msg_read_fail); \
  if(SHOW_GRP_DATA) { \
    WSAvgTime *local_ws_avg = NULL; \
    GET_WS_AVG(vptr, local_ws_avg); \
    local_ws_avg->ws_num_msg_read_fail++; \
  } \
}

#define INC_WS_WSS_NUM_CONN_BREAK_COUNTERS(vptr) \
{ \
  (ws_avgtime->ws_num_con_break)++; \
  NSDL2_WS(NULL, NULL, "ws_num_con_break = %llu", ws_avgtime->ws_num_con_break); \
  if(SHOW_GRP_DATA) { \
    WSAvgTime *local_ws_avg = NULL; \
    GET_WS_AVG(vptr, local_ws_avg); \
    local_ws_avg->ws_num_con_break++; \
  } \
}

#define UPDATE_WS_RESP_STATUS_AND_ERR_GRPH \
{ \
  NSDL2_WS(NULL, NULL, "UPDATE_WS_RESP_STATUS_AND_ERR_GRPH: cptr = %p, ws_avgtime = %p", cptr, ws_avgtime); \
  update_ws_status_codes(cptr, ws_avgtime); \
  if(SHOW_GRP_DATA) { \
    WSAvgTime *local_ws_avg = NULL; \
    GET_WS_AVG(vptr, local_ws_avg); \
    update_ws_status_codes(cptr, local_ws_avg); \
  } \
}


#ifndef CAV_MAIN
extern int g_ws_avgtime_idx;
extern WSAvgTime *ws_avgtime;
#else
extern __thread int g_ws_avgtime_idx;
extern __thread WSAvgTime *ws_avgtime;
#endif
extern int g_ws_cavgtime_idx;
extern int sgrp_used_genrator_entries;   
extern int total_ws_status_codes;
extern unsigned int ws_status_codes_gp_idx;    //Response Codes
extern unsigned int ws_stats_gp_idx;           //WS Connection Graphs 
extern unsigned int ws_failure_stats_gp_idx;   //Failure Stats


extern WSCAvgTime *ws_cavgtime;
extern WSStats_gp *ws_stats_gp_ptr;
extern WSStatusCodes_gp *ws_status_codes_gp_ptr;
extern WSFailureStats_gp *ws_failure_stats_gp_ptr;

extern void ws_log_summary_data (avgtime *avg, double *ws_data, FILE *fp, cavgtime *cavg);
extern void update_websocket_data_avgtime_size();
extern void update_websocket_data_cavgtime_size();
extern void fill_ws_stats_gp (avgtime **g_avg, cavgtime **g_cavg);
extern void print_ws_throughput(FILE *fp1, FILE *fp2, int is_periodic, avgtime *avg, cavgtime *cavg);
extern void ws_set_ws_avgtime_ptr();
extern inline void update_ws_status_codes(connection *cptr, WSAvgTime *ws_avgtime);
extern inline void fill_ws_failure_stats_gp(avgtime **g_avg);
#endif
