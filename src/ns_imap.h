#ifndef IMAP_H
#define IMAP_H

#include "ns_server.h"
#include "util.h"
#include "ns_error_codes.h"
#include "tmr.h"
#include "timing.h"
#include "logging.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"
#include "netstorm.h"

#define IMAP_SELECT 1
#define IMAP_STORE  2
#define IMAP_FETCH  3
#define IMAP_LIST   4
#define IMAP_SEARCH 5
#define IMAP_DELETE 6


/* imap protocol states */
/* Any change in protocol states should also be reflected in g_imap_st_str[][] */
/* Should always start with 0 since we are filling 0 in cptr init */
#define ST_IMAP_INITIALIZATION  0
#define ST_IMAP_CONNECTED       1
#define ST_IMAP_HANDSHAKE       2
#define ST_IMAP_SELECT          3
#define ST_IMAP_FETCH           4
#define ST_IMAP_LIST            5
#define ST_IMAP_STORE           6
#define ST_IMAP_SEARCH          7
#define ST_IMAP_LOGOUT          8
#define ST_IMAP_LOGIN           9
#define ST_IMAP_DELETE          10
#define ST_IMAP_STARTTLS        11
#define ST_IMAP_TLS_LOGIN       12

/* imap cptr header states */
#define IMAP_H_NEW              0 
#define IMAP_H_STR              1
#define IMAP_H_TXT              2
#define IMAP_H_STR_SP           3
#define IMAP_H_STR_TXT          4
#define IMAP_H_STR_SP_B         5
#define IMAP_H_STR_SP_O         6
#define IMAP_H_STR_SP_N         7
#define IMAP_H_STR_SP_BA        8
#define IMAP_H_STR_DOT          9
#define IMAP_H_STR_TXT_CR       10
#define IMAP_H_STR_DOT_CR       11
#define IMAP_H_END              12
#define IMAP_H_TXT_SP           13
#define IMAP_H_TXT_DOT          14
#define IMAP_H_TXT_CR           15
#define IMAP_H_TXT_DOT_CR       16
#define IMAP_H_TXT_SP_B         17
#define IMAP_H_TXT_SP_O         18
#define IMAP_H_TXT_SP_N         19
#define IMAP_H_TXT_SP_BA        20
#define IMAP_H_STR_TXT_S        21
#define IMAP_H_STR_TXT_ST       22
#define IMAP_H_STR_TXT_STA      23
#define IMAP_H_STR_TXT_STAR     24
#define IMAP_H_STR_TXT_START    25
#define IMAP_H_STR_TXT_STARTT   26
#define IMAP_H_STR_TXT_STARTTL  27
#define IMAP_H_STR_TXT_STARTTLS 28

/* IMAP response code */
#define IMAP_OK   1
#define IMAP_ERR  0

#define IMAP_TAG  "a2 "

/* IMAP cmds TODO: change it according to commands which we are going to implement in IMAP*/
#define IMAP_CMD_LOGIN   "login "
#define IMAP_CMD_FETCH   "fetch "
#define IMAP_CMD_STARTTLS   "starttls\r\n"
#define IMAP_CMD_LOGOUT   "logout\r\n"
#define IMAP_CMD_LIST    "list \"\" *\r\n"
#define IMAP_CMD_SELECT  "select inbox\r\n"
#define IMAP_CMD_CRLF    "\r\n"

// This is structure for imap avgtime
// Filled by child and send to parent in progress report
typedef struct {
  int imap_num_connections;

  u_ns_8B_t imap_total_bytes;
  u_ns_8B_t imap_tx_bytes;
  u_ns_8B_t imap_rx_bytes;

  u_ns_8B_t imap_num_con_initiated;   //Number of initiated requests for making the connection (periodic)
  u_ns_8B_t imap_num_con_succ;        //Number of succ requests for making the connection out of initiated (periodic)
  u_ns_8B_t imap_num_con_fail;        //Number of failures requests for making the connection out of initiated (periodic)
  u_ns_8B_t imap_num_con_break;       //Number of initiated requests for closing the connection (periodic)


  u_ns_8B_t imap_num_hits;
  u_ns_8B_t imap_num_tries;
  //Success
  u_ns_8B_t imap_avg_time;
  u_ns_8B_t imap_min_time;
  u_ns_8B_t imap_max_time;
  u_ns_8B_t imap_tot_time;

  //Overall
  u_ns_8B_t imap_overall_avg_time;
  u_ns_4B_t imap_overall_min_time;
  u_ns_4B_t imap_overall_max_time;
  u_ns_8B_t imap_overall_tot_time;

  //Failed
  u_ns_8B_t imap_failure_avg_time;
  u_ns_4B_t imap_failure_min_time;
  u_ns_4B_t imap_failure_max_time;
  u_ns_8B_t imap_failure_tot_time;

  u_ns_8B_t imap_fetches_started;

  u_ns_4B_t imap_response[MAX_GRANULES+1];
  u_ns_4B_t imap_error_codes[TOTAL_URL_ERR];   /* for url error codes 1-8 */
} IMAPAvgTime;  


// Cumulative counters - Used by parent ONLY
typedef struct {
  u_ns_8B_t imap_c_tot_total_bytes;
  u_ns_8B_t imap_c_tot_tx_bytes;
  u_ns_8B_t imap_c_tot_rx_bytes;
  u_ns_8B_t imap_c_num_con_initiated;
  u_ns_8B_t imap_c_num_con_succ;
  u_ns_8B_t imap_c_num_con_fail;
  u_ns_8B_t imap_c_num_con_break;
  u_ns_8B_t imap_succ_fetches;
  u_ns_8B_t imap_fetches_completed;
  u_ns_8B_t imap_c_min_time;                   //Overall imap time - imap_overall_min_time
  u_ns_8B_t imap_c_max_time;                   //Overall imap time - imap_overall_max_time
  u_ns_8B_t imap_c_tot_time;                   //Overall imap time - imap_overall_tot_time
  u_ns_8B_t imap_c_avg_time;                   //Overall imap time - imap_overall_avg_time
  u_ns_8B_t cum_imap_fetches_started;
  u_ns_4B_t cum_imap_error_codes[TOTAL_URL_ERR];   /* for url error codes 1-8 */
} IMAPCAvgTime;

#ifndef CAV_MAIN
extern int g_imap_avgtime_idx;
extern IMAPAvgTime *imap_avgtime;
#else
extern __thread int g_imap_avgtime_idx;
extern __thread IMAPAvgTime *imap_avgtime;
#endif
extern int g_imap_cavgtime_idx;
extern int g_IMAPcavgtime_size;

extern IMAPCAvgTime *imap_cavgtime;

extern int imap_num_connections; 

// Set cumulative elements of IMAP struct a with b into a
#define SET_MIN_MAX_IMAP_CUMULATIVES(a, b)\
  NSDL2_MESSAGES(NULL, NULL, "Before imap_c_min_time = %llu, imap_c_max_time = %llu", (a)->imap_c_min_time, (a)->imap_c_max_time); \
  SET_MIN ((a)->imap_c_min_time, (b)->imap_overall_min_time);\
  SET_MAX ((a)->imap_c_max_time, (b)->imap_overall_max_time);\
  NSDL2_MESSAGES(NULL, NULL, "After imap_c_min_time = %llu, imap_c_max_time = %llu", (a)->imap_c_min_time, (a)->imap_c_max_time);

#define SET_MIN_MAX_IMAP_CUMULATIVES_PARENT(a, b)\
  NSDL2_MESSAGES(NULL, NULL, "Before imap_c_min_time = %llu, imap_c_max_time = %llu", (a)->imap_c_min_time, (a)->imap_c_max_time); \
  SET_MIN ((a)->imap_c_min_time, (b)->imap_c_min_time);\
  SET_MAX ((a)->imap_c_max_time, (b)->imap_c_max_time);\
  NSDL2_MESSAGES(NULL, NULL, "After imap_c_min_time = %llu, imap_c_max_time = %llu", (a)->imap_c_min_time, (a)->imap_c_max_time);

//Set  periodic elements of IMAP struct a with b into a
#define SET_MIN_MAX_IMAP_PERIODICS(a, b)\
  SET_MIN ((a)->imap_overall_min_time, (b)->imap_overall_min_time);\
  SET_MAX ((a)->imap_overall_max_time, (b)->imap_overall_max_time);\
  SET_MIN ((a)->imap_min_time, (b)->imap_min_time);\
  SET_MAX ((a)->imap_max_time, (b)->imap_max_time);\
  SET_MIN ((a)->imap_failure_min_time, (b)->imap_failure_min_time);\
  SET_MAX ((a)->imap_failure_max_time, (b)->imap_failure_max_time); 

//accukumate cumulative elements of IMAP struct a with b into a 
#define ACC_IMAP_CUMULATIVES(a, b)\
  (a)->cum_imap_fetches_started += (b)->imap_fetches_started;\
  (a)->imap_fetches_completed += (b)->imap_num_hits;\
  NSDL2_MESSAGES(NULL, NULL, "Before imap_succ_fetches = %llu, imap_succ_fetches = %llu",(a)->imap_succ_fetches, (b)->imap_num_tries); \
  (a)->imap_succ_fetches += (b)->imap_num_tries;\
  NSDL2_MESSAGES(NULL, NULL, "after imap_succ_fetches = %llu, imap_succ_fetches = %llu",(a)->imap_succ_fetches, (b)->imap_num_tries); \
  (a)->imap_c_tot_time += (b)->imap_overall_tot_time;\
  (a)->imap_c_num_con_initiated += (b)->imap_num_con_initiated;\
  (a)->imap_c_num_con_succ += (b)->imap_num_con_succ;\
  (a)->imap_c_num_con_fail += (b)->imap_num_con_fail;\
  (a)->imap_c_num_con_break += (b)->imap_num_con_break;\
  (a)->imap_c_tot_total_bytes += (b)->imap_total_bytes ;\
  (a)->imap_c_tot_tx_bytes += (b)->imap_tx_bytes;\
  (a)->imap_c_tot_rx_bytes += (b)->imap_rx_bytes;\
  for (i = 0; i < TOTAL_URL_ERR; i++) {\
    (a)->cum_imap_error_codes[i] += (b)->imap_error_codes[i]; \
  }

#define ACC_IMAP_CUMULATIVES_PARENT(a, b)\
  (a)->cum_imap_fetches_started += (b)->cum_imap_fetches_started;\
  (a)->imap_fetches_completed += (b)->imap_fetches_completed;\
  NSDL2_MESSAGES(NULL, NULL, "Before imap_succ_fetches = %llu, imap_succ_fetches = %llu",(a)->imap_succ_fetches, (b)->imap_succ_fetches); \
  (a)->imap_succ_fetches += (b)->imap_succ_fetches;\
  NSDL2_MESSAGES(NULL, NULL, "after imap_succ_fetches = %llu, imap_succ_fetches = %llu",(a)->imap_succ_fetches, (b)->imap_succ_fetches); \
  (a)->imap_c_tot_time += (b)->imap_c_tot_time;\
  (a)->imap_c_num_con_initiated += (b)->imap_c_num_con_initiated;\
  (a)->imap_c_num_con_succ += (b)->imap_c_num_con_succ;\
  (a)->imap_c_num_con_fail += (b)->imap_c_num_con_fail;\
  (a)->imap_c_num_con_break += (b)->imap_c_num_con_break;\
  (a)->imap_c_tot_total_bytes += (b)->imap_c_tot_total_bytes ;\
  (a)->imap_c_tot_tx_bytes += (b)->imap_c_tot_tx_bytes;\
  (a)->imap_c_tot_rx_bytes += (b)->imap_c_tot_rx_bytes;\
  for (i = 0; i < TOTAL_URL_ERR; i++) {\
    (a)->cum_imap_error_codes[i] += (b)->cum_imap_error_codes[i]; \
  }

#define ACC_IMAP_PERIODICS(a, b)\
  (a)->imap_total_bytes += (b)->imap_total_bytes ;\
  (a)->imap_tx_bytes += (b)->imap_tx_bytes;\
  (a)->imap_rx_bytes += (b)->imap_rx_bytes;\
  (a)->imap_num_con_initiated += (b)->imap_num_con_initiated;\
  (a)->imap_num_con_succ += (b)->imap_num_con_succ;\
  (a)->imap_num_con_fail += (b)->imap_num_con_fail;\
  (a)->imap_num_con_break += (b)->imap_num_con_break;\
  (a)->imap_num_hits += (b)->imap_num_hits;\
  (a)->imap_num_tries += (b)->imap_num_tries;\
  (a)->imap_overall_tot_time += (b)->imap_overall_tot_time;\
  (a)->imap_overall_avg_time += (b)->imap_overall_avg_time;\
  (a)->imap_tot_time += (b)->imap_tot_time;\
  (a)->imap_avg_time += (b)->imap_avg_time;\
  (a)->imap_failure_avg_time += (b)->imap_failure_avg_time;\
  (a)->imap_failure_tot_time += (b)->imap_failure_tot_time;\
  (a)->imap_fetches_started += (b)->imap_fetches_started;\
 /*(a)->imap_response[MAX_GRANULES+1] += (b)->imap_response[MAX_GRANULES+1];\
   (a)->imap_error_codes[TOTAL_URL_ERR] += (b)->imap_error_codes[TOTAL_URL_ERR];\
   (a)->cum_imap_error_codes[TOTAL_URL_ERR] += (b)->cum_imap_error_codes[TOTAL_URL_ERR];*/\
  for (i = 0; i < TOTAL_URL_ERR; i++) {\
    (a)->imap_error_codes[i] += (b)->imap_error_codes[i];\
  }
                 
#define CHILD_RESET_IMAP_AVGTIME(a) \
{\
  if(global_settings->protocol_enabled & IMAP_PROTOCOL_ENABLED) \
  { \
    IMAPAvgTime *loc_imap_avgtime = (IMAPAvgTime*)((char*)a + g_imap_avgtime_idx); \
    memset(loc_imap_avgtime->imap_error_codes, 0, TOTAL_URL_ERR * sizeof(int)); \
    loc_imap_avgtime->imap_num_hits = 0;\
    loc_imap_avgtime->imap_num_tries = 0;\
    loc_imap_avgtime->imap_overall_avg_time = 0;\
    loc_imap_avgtime->imap_overall_min_time = 0xFFFFFFFF;\
    loc_imap_avgtime->imap_overall_max_time = 0;\
    loc_imap_avgtime->imap_overall_tot_time = 0;\
    loc_imap_avgtime->imap_avg_time = 0;\
    loc_imap_avgtime->imap_min_time = 0xFFFFFFFF;\
    loc_imap_avgtime->imap_max_time = 0;\
    loc_imap_avgtime->imap_tot_time = 0;\
    loc_imap_avgtime->imap_failure_avg_time = 0;\
    loc_imap_avgtime->imap_failure_min_time = 0xFFFFFFFF;\
    loc_imap_avgtime->imap_failure_max_time = 0;\
    loc_imap_avgtime->imap_failure_tot_time = 0;\
    loc_imap_avgtime->imap_fetches_started = 0;\
    loc_imap_avgtime->imap_num_con_initiated = 0;\
    loc_imap_avgtime->imap_num_con_succ = 0;\
    loc_imap_avgtime->imap_num_con_fail = 0;\
    loc_imap_avgtime->imap_num_con_break = 0;\
    loc_imap_avgtime->imap_total_bytes = 0;\
    loc_imap_avgtime->imap_tx_bytes = 0;\
    loc_imap_avgtime->imap_rx_bytes = 0;\
  }\
}

#define CHILD_IMAP_SET_CUM_FIELD_OF_AVGTIME(a)\
  if(global_settings->protocol_enabled & IMAP_PROTOCOL_ENABLED) { \
    SET_MIN(a->imap_c_min_time, a->imap_min_time); \
    SET_MAX(a->imap_c_max_time, a->imap_max_time); \
    (a)->imap_num_connections = imap_num_connections; \
    (a)->imap_fetches_completed += (a)->imap_num_tries;\
    (a)->imap_succ_fetches += (a)->imap_num_hits;\
    (a)->cum_imap_fetches_started += (a)->imap_fetches_started;\
    (a)->imap_c_tot_time += (a)->imap_tot_time;\
    (a)->imap_c_tot_total_bytes +=(a)->imap_total_bytes;\
    (a)->imap_c_tot_tx_bytes += (a)->imap_tx_bytes;\
    (a)->imap_c_tot_rx_bytes += (a)->imap_rx_bytes;\
    (a)->imap_c_num_con_succ += (a)->imap_num_con_succ;\
    (a)->imap_c_num_con_initiated += (a)->imap_num_con_initiated;\
    (a)->imap_c_num_con_fail += (a)->imap_num_con_fail;\
    (a)->imap_c_num_con_break += (a)->imap_num_con_break;\
    memcpy(a->cum_imap_error_codes, a->imap_error_codes, sizeof(int) * TOTAL_URL_ERR); \
  }
#ifndef CAV_MAIN
extern IMAPAvgTime *imap_avgtime;
#else
extern __thread IMAPAvgTime *imap_avgtime;
#endif
extern void imap_set_imap_avgtime_ptr();
extern void fill_imap_gp (avgtime **imap_avg, cavgtime **imap_cavg);
extern void print_imap_throughput(FILE *fp1, FILE *fp2, int is_periodic, avgtime *avg, cavgtime *cavg);
extern void imap_log_summary_data (avgtime *avg, double *imap_data, FILE *fp, cavgtime *cavg);
extern void imap_update_imap_cavgtime_size();

extern void imap_timeout_handle( ClientData client_data, u_ns_ts_t now );
extern void delete_imap_timeout_timer(connection *cptr);

extern void debug_log_imap_req(connection *cptr, char *buf, int bytes_to_log, int complete_data, int first_trace_write_flag);
extern void imap_process_select(connection *cptr, u_ns_ts_t now, char *buf, int bytes_read);
extern void imap_process_fetch(connection *cptr, u_ns_ts_t now, char *buf, int bytes_read);
extern void imap_process_list(connection *cptr, u_ns_ts_t now, char *buf, int bytes_read);
//extern void imap_process_search(connection *cptr, u_ns_ts_t now, char *buf, int bytes_read);
extern void imap_process_handshake(connection *cptr, u_ns_ts_t now, char *buf, int bytes_read, int start_tls);
extern void debug_log_imap_res(connection *cptr, char *buf, int size);
extern void imap_process_logout(connection *cptr, u_ns_ts_t now, char *buf, int bytes_read);
extern int  ns_parse_imap(char *api_name, char *api_to_run, FILE *flow_fp, FILE *outfp, char *flow_filename, char *flowout_filename, int sess_idx,int imap_action_type);
extern void imap_process_login(connection *cptr, u_ns_ts_t now, char *buf, int bytes_read);
extern void imap_update_imap_avgtime_size();
extern int kw_set_imap_timeout(char *buf, int *to_change, char *err_msg);
extern void imap_send_login(connection *cptr, u_ns_ts_t now);
extern int handle_imap_read( connection *cptr, u_ns_ts_t now );
extern char *imap_state_to_str(int state);
extern void imap_process_starttls(connection *cptr, u_ns_ts_t now, char *buf, int bytes_read);
#endif
