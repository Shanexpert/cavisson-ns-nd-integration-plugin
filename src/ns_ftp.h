#ifndef __NS_FTP_H__
#define  __NS_FTP_H__

/* protocol states */
/* Any change in protocol states should also be reflected in g_ftp_st_str[][] */
/* Should always start with 0 since we are filling 0 in cptr init */

#define ST_FTP_INITIALIZATION  0
#define ST_FTP_CONNECTED       1
#define ST_FTP_USER            2
#define ST_FTP_PASS            3
#define ST_FTP_PASV            4
#define ST_FTP_RETR            5
#define ST_FTP_RETR_INTRM      6
#define ST_FTP_RETR_INTRM_RECEIVED      7
#define ST_FTP_QUIT            8
#define ST_FTP_PORT            9
#define ST_FTP_STOR            10
#define ST_FTP_STOR_INTRM      11
#define ST_FTP_STOR_INTRM_RECEIVED 12
#define ST_FTP_TYPE 13 // mode for ascii and binary
#define ST_FTP_CMD  14 // for command type 
 
/*FTP ACTION TYPES GET/STORE */
#define FTP_ACTION_RETR         1
#define FTP_ACTION_STOR         2

/* CMDS */
#define FTP_CMD_USER   "USER "
#define FTP_CMD_PASS   "PASS "
#define FTP_CMD_PASV   "PASV\r\n"
#define FTP_CMD_PORT   "PORT "
#define FTP_CMD_RETR   "RETR "
#define FTP_CMD_STOR   "STOR "
#define FTP_CMD_TYPE   "TYPE I"
#define FTP_CMD_QUIT   "QUIT\r\n"

#define FTP_CMD_CRLF   "\r\n"


/* response states */
#define FTP_HDST_RCODE_X        0
#define FTP_HDST_RCODE_Y        1
#define FTP_HDST_RCODE_Z        2
#define FTP_HDST_TEXT           3
#define FTP_HDST_TEXT_CR        4
#define FTP_HDST_END            5

/*Transfer type*/
#define FTP_TRANSFER_TYPE_ACTIVE       0
#define FTP_TRANSFER_TYPE_PASSIVE      1
#define FTP_TRANSFER_TYPE_BINARY       1 

// Set cumulative elements of FTP struct a with b into a
#define SET_MIN_MAX_FTP_CUMULATIVES(a, b)\
  NSDL2_MESSAGES(NULL, NULL, "Before ftp_c_min_time = %llu, ftp_c_max_time = %llu", (a)->ftp_c_min_time, (a)->ftp_c_max_time); \
  SET_MIN ((a)->ftp_c_min_time, (b)->ftp_overall_min_time);\
  SET_MAX ((a)->ftp_c_max_time, (b)->ftp_overall_max_time);\
  NSDL2_MESSAGES(NULL, NULL, "After ftp_c_min_time = %llu, ftp_c_max_time = %llu", (a)->ftp_c_min_time, (a)->ftp_c_max_time);

#define SET_MIN_MAX_FTP_CUMULATIVES_PARENT(a, b)\
  NSDL2_MESSAGES(NULL, NULL, "Before ftp_c_min_time = %llu, ftp_c_max_time = %llu", (a)->ftp_c_min_time, (a)->ftp_c_max_time); \
  SET_MIN ((a)->ftp_c_min_time, (b)->ftp_c_min_time);\
  SET_MAX ((a)->ftp_c_max_time, (b)->ftp_c_max_time);\
  NSDL2_MESSAGES(NULL, NULL, "After ftp_c_min_time = %llu, ftp_c_max_time = %llu", (a)->ftp_c_min_time, (a)->ftp_c_max_time);

//Set  periodic elements of FTP struct a with b into a
#define SET_MIN_MAX_FTP_PERIODICS(a, b)\
  SET_MIN ((a)->ftp_overall_min_time, (b)->ftp_overall_min_time);\
  SET_MAX ((a)->ftp_overall_max_time, (b)->ftp_overall_max_time);\
  SET_MIN ((a)->ftp_min_time, (b)->ftp_min_time);\
  SET_MAX ((a)->ftp_max_time, (b)->ftp_max_time);\
  SET_MIN ((a)->ftp_failure_min_time, (b)->ftp_failure_min_time);\
  SET_MAX ((a)->ftp_failure_max_time, (b)->ftp_failure_max_time);

//accukumate cumulative elements of FTP struct a with b into a 
#define ACC_FTP_CUMULATIVES(a, b)\
  (a)->cum_ftp_fetches_started += (b)->ftp_fetches_started;\
  (a)->ftp_fetches_completed += (b)->ftp_num_tries;\
  (a)->ftp_succ_fetches += (b)->ftp_num_hits;\
  (a)->ftp_c_tot_time += (b)->ftp_overall_tot_time;\
  (a)->ftp_c_num_con_initiated += (b)->ftp_num_con_initiated;\
  (a)->ftp_c_num_con_succ += (b)->ftp_num_con_succ;\
  (a)->ftp_c_num_con_fail += (b)->ftp_num_con_fail;\
  (a)->ftp_c_num_con_break += (b)->ftp_num_con_break;\
  (a)->ftp_c_tot_total_bytes += (b)->ftp_total_bytes ;\
  (a)->ftp_c_tot_tx_bytes += (b)->ftp_tx_bytes;\
  (a)->ftp_c_tot_rx_bytes += (b)->ftp_rx_bytes;\
  for (i = 0; i < TOTAL_URL_ERR; i++) {\
    (a)->cum_ftp_error_codes[i] += (b)->ftp_error_codes[i]; \
  }

#define ACC_FTP_CUMULATIVES_PARENT(a, b)\
  (a)->cum_ftp_fetches_started += (b)->cum_ftp_fetches_started;\
  (a)->ftp_fetches_completed += (b)->ftp_fetches_completed;\
  (a)->ftp_succ_fetches += (b)->ftp_succ_fetches;\
  (a)->ftp_c_tot_time += (b)->ftp_c_tot_time;\
  (a)->ftp_c_num_con_initiated += (b)->ftp_c_num_con_initiated;\
  (a)->ftp_c_num_con_succ += (b)->ftp_c_num_con_succ;\
  (a)->ftp_c_num_con_fail += (b)->ftp_c_num_con_fail;\
  (a)->ftp_c_num_con_break += (b)->ftp_c_num_con_break;\
  (a)->ftp_c_tot_total_bytes += (b)->ftp_c_tot_total_bytes ;\
  (a)->ftp_c_tot_tx_bytes += (b)->ftp_c_tot_tx_bytes;\
  (a)->ftp_c_tot_rx_bytes += (b)->ftp_c_tot_rx_bytes;\
  for (i = 0; i < TOTAL_URL_ERR; i++) {\
    (a)->cum_ftp_error_codes[i] += (b)->cum_ftp_error_codes[i]; \
  }

#define ACC_FTP_PERIODICS(a, b)\
  (a)->ftp_total_bytes += (b)->ftp_total_bytes ;\
  (a)->ftp_tx_bytes += (b)->ftp_tx_bytes;\
  (a)->ftp_rx_bytes += (b)->ftp_rx_bytes;\
  (a)->ftp_num_con_initiated += (b)->ftp_num_con_initiated;\
  (a)->ftp_num_con_succ += (b)->ftp_num_con_succ;\
  (a)->ftp_num_con_fail += (b)->ftp_num_con_fail;\
  (a)->ftp_num_con_break += (b)->ftp_num_con_break;\
  (a)->ftp_num_hits += (b)->ftp_num_hits;\
  (a)->ftp_num_tries += (b)->ftp_num_tries;\
  (a)->ftp_overall_avg_time += (b)->ftp_overall_avg_time;\
  (a)->ftp_overall_tot_time += (b)->ftp_overall_tot_time;\
  (a)->ftp_avg_time += (b)->ftp_avg_time;\
  (a)->ftp_tot_time += (b)->ftp_tot_time;\
  (a)->ftp_failure_avg_time += (b)->ftp_failure_avg_time;\
  (a)->ftp_failure_tot_time += (b)->ftp_failure_tot_time;\
  (a)->ftp_fetches_started += (b)->ftp_fetches_started;\
 /*(a)->ftp_response[MAX_GRANULES+1] += (b)->ftp_response[MAX_GRANULES+1];\
   (a)->ftp_error_codes[TOTAL_URL_ERR] += (b)->ftp_error_codes[TOTAL_URL_ERR];\
   (a)->cum_ftp_error_codes[TOTAL_URL_ERR] += (b)->cum_ftp_error_codes[TOTAL_URL_ERR];*/\
  for (i = 0; i < TOTAL_URL_ERR; i++) {\
    (a)->ftp_error_codes[i] += (b)->ftp_error_codes[i];\
  }
                 
#define CHILD_RESET_FTP_AVGTIME(a) \
{\
  if(global_settings->protocol_enabled & FTP_PROTOCOL_ENABLED)\
  { \
    FTPAvgTime *loc_ftp_avgtime = (FTPAvgTime*)((char*)a + g_ftp_avgtime_idx); \
    memset(loc_ftp_avgtime->ftp_error_codes, 0, TOTAL_URL_ERR * sizeof(int)); \
    loc_ftp_avgtime->ftp_num_hits = 0;\
    loc_ftp_avgtime->ftp_num_tries = 0;\
    loc_ftp_avgtime->ftp_overall_avg_time = 0;\
    loc_ftp_avgtime->ftp_overall_min_time = 0xFFFFFFFF;\
    loc_ftp_avgtime->ftp_overall_max_time = 0;\
    loc_ftp_avgtime->ftp_overall_tot_time = 0;\
    loc_ftp_avgtime->ftp_avg_time = 0;\
    loc_ftp_avgtime->ftp_min_time = 0xFFFFFFFF;\
    loc_ftp_avgtime->ftp_max_time = 0;\
    loc_ftp_avgtime->ftp_tot_time = 0;\
    loc_ftp_avgtime->ftp_failure_avg_time = 0;\
    loc_ftp_avgtime->ftp_failure_min_time = 0xFFFFFFFF;\
    loc_ftp_avgtime->ftp_failure_max_time = 0;\
    loc_ftp_avgtime->ftp_failure_tot_time = 0;\
    loc_ftp_avgtime->ftp_fetches_started = 0;\
    loc_ftp_avgtime->ftp_num_con_initiated = 0;\
    loc_ftp_avgtime->ftp_num_con_succ = 0;\
    loc_ftp_avgtime->ftp_num_con_fail = 0;\
    loc_ftp_avgtime->ftp_num_con_break = 0;\
    loc_ftp_avgtime->ftp_total_bytes = 0;\
    loc_ftp_avgtime->ftp_tx_bytes = 0;\
    loc_ftp_avgtime->ftp_rx_bytes = 0;\
  }\
}

#define CHILD_FTP_SET_CUM_FIELD_OF_AVGTIME(a)\
  if(global_settings->protocol_enabled & FTP_PROTOCOL_ENABLED) { \
    SET_MIN(a->ftp_c_min_time, a->ftp_min_time); \
    SET_MAX(a->ftp_c_max_time, a->ftp_max_time); \
    (a)->ftp_num_connections = ftp_num_connections; \
    (a)->ftp_fetches_completed += (a)->ftp_num_tries;\
    (a)->ftp_succ_fetches += (a)->ftp_num_hits;\
    (a)->cum_ftp_fetches_started += (a)->ftp_fetches_started;\
    (a)->ftp_c_tot_time += (a)->ftp_tot_time;\
    (a)->ftp_c_tot_total_bytes +=(a)->ftp_total_bytes;\
    (a)->ftp_c_tot_tx_bytes += (a)->ftp_tx_bytes;\
    (a)->ftp_c_tot_rx_bytes += (a)->ftp_rx_bytes;\
    (a)->ftp_c_num_con_succ += (a)->ftp_num_con_succ;\
    (a)->ftp_c_num_con_initiated += (a)->ftp_num_con_initiated;\
    (a)->ftp_c_num_con_fail += (a)->ftp_num_con_fail;\
    (a)->ftp_c_num_con_break += (a)->ftp_num_con_break;\
    memcpy(a->cum_ftp_error_codes, a->ftp_error_codes, sizeof(int) * TOTAL_URL_ERR); \
  }

// This is structure for ftp avgtime
// Filled by child and send to parent in progress report
typedef struct {
  int ftp_num_connections;

  u_ns_8B_t ftp_total_bytes;
  u_ns_8B_t ftp_tx_bytes;
  u_ns_8B_t ftp_rx_bytes;

  u_ns_8B_t ftp_num_con_initiated;   //Number of initiated requests for making the connection (periodic)
  u_ns_8B_t ftp_num_con_succ;        //Number of succ requests for making the connection out of initiated (periodic)
  u_ns_8B_t ftp_num_con_fail;        //Number of failures requests for making the connection out of initiated (periodic)
  u_ns_8B_t ftp_num_con_break;       //Number of initiated requests for closing the connection (periodic)

  u_ns_8B_t ftp_num_hits;
  u_ns_8B_t ftp_num_tries;
  //Success
  u_ns_8B_t ftp_avg_time;
  u_ns_8B_t ftp_min_time;
  u_ns_8B_t ftp_max_time;
  u_ns_8B_t ftp_tot_time;

  //Overall
  u_ns_8B_t ftp_overall_avg_time;
  u_ns_4B_t ftp_overall_min_time;
  u_ns_4B_t ftp_overall_max_time;
  u_ns_8B_t ftp_overall_tot_time;

  //Failed
  u_ns_8B_t ftp_failure_avg_time;
  u_ns_4B_t ftp_failure_min_time;
  u_ns_4B_t ftp_failure_max_time;
  u_ns_8B_t ftp_failure_tot_time;
  u_ns_8B_t ftp_fetches_started;

  u_ns_4B_t ftp_response[MAX_GRANULES+1];
  u_ns_4B_t ftp_error_codes[TOTAL_URL_ERR];   /* for url error codes 1-8 */
} FTPAvgTime;  


// Cumulative counters - Used by parent ONLY
typedef struct {
  u_ns_8B_t ftp_c_tot_total_bytes;
  u_ns_8B_t ftp_c_tot_tx_bytes;
  u_ns_8B_t ftp_c_tot_rx_bytes;
  u_ns_8B_t ftp_c_num_con_initiated;
  u_ns_8B_t ftp_c_num_con_succ;
  u_ns_8B_t ftp_c_num_con_fail;
  u_ns_8B_t ftp_c_num_con_break;
  u_ns_8B_t ftp_succ_fetches;
  u_ns_8B_t ftp_fetches_completed;
  u_ns_8B_t ftp_c_min_time;                   //Overall ftp time - ftp_overall_min_time
  u_ns_8B_t ftp_c_max_time;                   //Overall ftp time - ftp_overall_max_time
  u_ns_8B_t ftp_c_tot_time;                   //Overall ftp time - ftp_overall_tot_time
  u_ns_8B_t ftp_c_avg_time;                   //Overall ftp time - ftp_overall_avg_time
  u_ns_8B_t cum_ftp_fetches_started;
  u_ns_4B_t cum_ftp_error_codes[TOTAL_URL_ERR];   /* for url error codes 1-8 */
} FTPCAvgTime;

#ifndef CAV_MAIN
extern int g_ftp_avgtime_idx;
extern FTPAvgTime *ftp_avgtime;
#else
extern __thread int g_ftp_avgtime_idx;
extern __thread FTPAvgTime *ftp_avgtime;
#endif
extern int g_ftp_cavgtime_idx;
extern int g_FTPcavgtime_size;

extern FTPCAvgTime *ftp_cavgtime;

extern void delete_ftp_timeout_timer(connection *cptr);
extern int handle_ftp_read(connection *cptr, u_ns_ts_t now );
extern char *ftp_state_to_str(int state);
extern int handle_ftp_read( connection *cptr, u_ns_ts_t now );
extern int handle_ftp_data_read( connection *cptr, u_ns_ts_t now ) ;
extern void debug_log_ftp_req(connection *cptr, char *buf, int bytes_to_log, int complete_data, int first_trace_write_flag);
extern void ftp_set_ftp_avgtime_ptr();
extern void ftp_update_ftp_avgtime_size();
extern connection *get_new_data_connection(connection *cptr, u_ns_ts_t now, char *hostname, int port, action_request_Shr *new_url_num);
extern  void fill_ftp_gp (avgtime **ftp_avg, cavgtime **ftp_cavg);
extern void print_ftp_throughput(FILE *fp1, FILE *fp2, int is_periodic, avgtime *avg, cavgtime *cavg);
extern void ftp_log_summary_data (avgtime *avg, double *ftp_data, FILE *fp,cavgtime *cavg);
extern void ftp_update_ftp_cavgtime_size();
extern void ftp_send_cmd(connection *cptr, u_ns_ts_t now);
extern void ftp_send_type(connection *cptr, u_ns_ts_t now);
extern void ftp_send_stor(connection *cptr, u_ns_ts_t now);
extern int handle_ftp_data_write(connection *cptr, u_ns_ts_t now);
//extern void fill_rtg_graph_data(avgtime *avg, FTPAvgTime *ftp_avg, cavgtime *cavg, int gen_idx);
#endif  /*  __NS_FTP_H__ */
