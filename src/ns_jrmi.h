
#ifndef _NS_JRMI
#define _NS_JRMI

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
#include "ns_msg_com_util.h"

#define SINGLE       0
#define STREAM       1
#define MULTIPLEX    2

// Jrmi protom state use in maintaining jrmi connection states while handshake and diffrent call communications
#define ST_JRMI_HANDSHAKE    		0
#define ST_JRMI_HANDSHAKE_RES	 	1
#define ST_JRMI_IDENTIFIER	 	2
#define ST_JRMI_STATIC_CALL		3
#define ST_JRMI_STATIC_CALL_RES		4
#define ST_JRMI_NSTATIC_CALL		5
#define ST_JRMI_NSTATIC_CALL_RES	6
#define ST_JRMI_PING			7
#define ST_JRMI_PING_RES	        8
#define ST_JRMI_DGACK			9
#define ST_JRMI_END			10


extern char *jrmi_buff;
extern int jrmi_content_size;
// This is structure for jrmi avgtime
// Filled by child and send to parent in progress report
typedef struct {
  int jrmi_num_connections;

  u_ns_8B_t jrmi_total_bytes;
  u_ns_8B_t jrmi_tx_bytes;
  u_ns_8B_t jrmi_rx_bytes;

  u_ns_8B_t jrmi_num_con_initiated;   //Number of initiated requests for making the connection (periodic)
  u_ns_8B_t jrmi_num_con_succ;        //Number of succ requests for making the connection out of initiated (periodic)
  u_ns_8B_t jrmi_num_con_fail;        //Number of failures requests for making the connection out of initiated (periodic)
  u_ns_8B_t jrmi_num_con_break;       //Number of initiated requests for closing the connection (periodic)

  u_ns_8B_t jrmi_num_hits;
  u_ns_8B_t jrmi_num_tries;
  u_ns_8B_t jrmi_avg_time;
  u_ns_8B_t jrmi_min_time;
  u_ns_8B_t jrmi_max_time;
  u_ns_8B_t jrmi_tot_time;
  u_ns_8B_t jrmi_fetches_started;

  u_ns_4B_t jrmi_response[MAX_GRANULES+1];
  u_ns_4B_t jrmi_error_codes[TOTAL_URL_ERR];   /* for url error codes 1-8 */
} JRMIAvgTime;  


// Cumulative counters - Used by parent ONLY
typedef struct {
  u_ns_8B_t jrmi_c_tot_total_bytes;
  u_ns_8B_t jrmi_c_tot_tx_bytes;
  u_ns_8B_t jrmi_c_tot_rx_bytes;
  u_ns_8B_t jrmi_c_num_con_initiated;
  u_ns_8B_t jrmi_c_num_con_succ;
  u_ns_8B_t jrmi_c_num_con_fail;
  u_ns_8B_t jrmi_c_num_con_break;
  u_ns_8B_t jrmi_succ_fetches;
  u_ns_8B_t jrmi_fetches_completed;
  u_ns_8B_t jrmi_c_min_time;
  u_ns_8B_t jrmi_c_max_time;
  u_ns_8B_t jrmi_c_tot_time;
  u_ns_8B_t jrmi_c_avg_time;
  u_ns_8B_t cum_jrmi_fetches_started;
  u_ns_4B_t cum_jrmi_error_codes[TOTAL_URL_ERR];   /* for url error codes 1-8 */
} JRMICAvgTime;


#ifndef CAV_MAIN
extern int g_jrmi_avgtime_idx;
extern JRMIAvgTime *jrmi_avgtime;
#else
extern __thread int g_jrmi_avgtime_idx;
extern __thread JRMIAvgTime *jrmi_avgtime;
#endif
extern int g_jrmi_cavgtime_idx;
extern int g_JRMIcavgtime_size;

extern JRMICAvgTime *jrmi_cavgtime;

extern int jrmi_num_connections; 

extern int jrmi_operation;
// Set cumulative elements of JRMI struct a with b into a
#define SET_MIN_MAX_JRMI_CUMULATIVES(a, b)\
  NSDL2_MESSAGES(NULL, NULL, "Before jrmi_c_min_time = %llu, jrmi_c_max_time = %llu", (a)->jrmi_c_min_time, (b)->jrmi_min_time);\
  SET_MIN ((a)->jrmi_c_min_time, (b)->jrmi_min_time);\
  SET_MAX ((a)->jrmi_c_max_time, (b)->jrmi_max_time);\
  NSDL2_MESSAGES(NULL, NULL, "After jrmi_c_min_time = %llu, jrmi_c_max_time = %llu", (a)->jrmi_c_min_time, (b)->jrmi_min_time);\

#define SET_MIN_MAX_JRMI_CUMULATIVES_PARENT(a, b)\
  NSDL2_MESSAGES(NULL, NULL, "Before jrmi_c_min_time = %llu, jrmi_c_max_time = %llu", (a)->jrmi_c_min_time, (b)->jrmi_c_min_time);\
  SET_MIN ((a)->jrmi_c_min_time, (b)->jrmi_c_min_time);\
  SET_MAX ((a)->jrmi_c_max_time, (b)->jrmi_c_max_time);\
  NSDL2_MESSAGES(NULL, NULL, "After jrmi_c_min_time = %llu, jrmi_c_max_time = %llu", (a)->jrmi_c_min_time, (b)->jrmi_c_min_time);\

//Set  periodic elements of JRMI struct a with b into a
#define SET_MIN_MAX_JRMI_PERIODICS(a, b)\
  SET_MIN ((a)->jrmi_min_time, (b)->jrmi_min_time);\
  SET_MAX ((a)->jrmi_max_time, (b)->jrmi_max_time);\

//accukumate cumulative elements of JRMI struct a with b into a 
#define ACC_JRMI_CUMULATIVES(a, b)\
  (a)->cum_jrmi_fetches_started += (b)->jrmi_fetches_started;\
  (a)->jrmi_fetches_completed += (b)->jrmi_num_tries;\
  NSDL2_MESSAGES(NULL, NULL, "Before jrmi_succ_fetches = %llu, jrmi_succ_fetches = %llu",(a)->jrmi_succ_fetches, (b)->jrmi_num_hits);\
  (a)->jrmi_succ_fetches += (b)->jrmi_num_hits;\
  NSDL2_MESSAGES(NULL, NULL, "after jrmi_succ_fetches = %llu, jrmi_succ_fetches = %llu",(a)->jrmi_succ_fetches, (b)->jrmi_num_hits);\
  (a)->jrmi_c_tot_time += (b)->jrmi_tot_time;\
  (a)->jrmi_c_num_con_initiated += (b)->jrmi_num_con_initiated;\
  (a)->jrmi_c_num_con_succ += (b)->jrmi_num_con_succ;\
  (a)->jrmi_c_num_con_fail += (b)->jrmi_num_con_fail;\
  (a)->jrmi_c_num_con_break += (b)->jrmi_num_con_break;\
  (a)->jrmi_c_tot_total_bytes += (b)->jrmi_total_bytes ;\
  (a)->jrmi_c_tot_tx_bytes += (b)->jrmi_tx_bytes;\
  (a)->jrmi_c_tot_rx_bytes += (b)->jrmi_rx_bytes;\
  for (i = 0; i < TOTAL_URL_ERR; i++) {\
    (a)->cum_jrmi_error_codes[i] += (b)->jrmi_error_codes[i];\
  }

#define ACC_JRMI_CUMULATIVES_PARENT(a, b)\
  (a)->cum_jrmi_fetches_started += (b)->cum_jrmi_fetches_started;\
  (a)->jrmi_fetches_completed += (b)->jrmi_fetches_completed;\
  NSDL2_MESSAGES(NULL, NULL, "Before jrmi_succ_fetches = %llu, jrmi_succ_fetches = %llu",(a)->jrmi_succ_fetches, (b)->jrmi_succ_fetches);\
  (a)->jrmi_succ_fetches += (b)->jrmi_succ_fetches;\
  NSDL2_MESSAGES(NULL, NULL, "after jrmi_succ_fetches = %llu, jrmi_succ_fetches = %llu",(a)->jrmi_succ_fetches, (b)->jrmi_succ_fetches);\
  (a)->jrmi_c_tot_time += (b)->jrmi_c_tot_time;\
  (a)->jrmi_c_num_con_initiated += (b)->jrmi_c_num_con_initiated;\
  (a)->jrmi_c_num_con_succ += (b)->jrmi_c_num_con_succ;\
  (a)->jrmi_c_num_con_fail += (b)->jrmi_c_num_con_fail;\
  (a)->jrmi_c_num_con_break += (b)->jrmi_c_num_con_break;\
  (a)->jrmi_c_tot_total_bytes += (b)->jrmi_c_tot_total_bytes ;\
  (a)->jrmi_c_tot_tx_bytes += (b)->jrmi_c_tot_tx_bytes;\
  (a)->jrmi_c_tot_rx_bytes += (b)->jrmi_c_tot_rx_bytes;\
  for (i = 0; i < TOTAL_URL_ERR; i++) {\
    (a)->cum_jrmi_error_codes[i] += (b)->cum_jrmi_error_codes[i];\
  }

#define ACC_JRMI_PERIODICS(a, b)\
  (a)->jrmi_total_bytes += (b)->jrmi_total_bytes ;\
  (a)->jrmi_tx_bytes += (b)->jrmi_tx_bytes;\
  (a)->jrmi_rx_bytes += (b)->jrmi_rx_bytes;\
  (a)->jrmi_num_con_initiated += (b)->jrmi_num_con_initiated;\
  (a)->jrmi_num_con_succ += (b)->jrmi_num_con_succ;\
  (a)->jrmi_num_con_fail += (b)->jrmi_num_con_fail;\
  (a)->jrmi_num_con_break += (b)->jrmi_num_con_break;\
  (a)->jrmi_num_hits += (b)->jrmi_num_hits;\
  (a)->jrmi_num_tries += (b)->jrmi_num_tries;\
  (a)->jrmi_avg_time += (b)->jrmi_avg_time;\
  (a)->jrmi_tot_time += (b)->jrmi_tot_time;\
  (a)->jrmi_fetches_started += (b)->jrmi_fetches_started;\
 /*(a)->jrmi_response[MAX_GRANULES+1] += (b)->jrmi_response[MAX_GRANULES+1];\
   (a)->jrmi_error_codes[TOTAL_URL_ERR] += (b)->jrmi_error_codes[TOTAL_URL_ERR];\
   (a)->cum_jrmi_error_codes[TOTAL_URL_ERR] += (b)->cum_jrmi_error_codes[TOTAL_URL_ERR];*/\
  for (i = 0; i < TOTAL_URL_ERR; i++) {\
    (a)->jrmi_error_codes[i] += (b)->jrmi_error_codes[i];\
  }
                 
#define CHILD_RESET_JRMI_AVGTIME(a) \
{\
  if(global_settings->protocol_enabled & JRMI_PROTOCOL_ENABLED)\
  { \
    JRMIAvgTime *loc_jrmi_avgtime = (JRMIAvgTime*)((char*)a + g_jrmi_avgtime_idx); \
    memset(loc_jrmi_avgtime->jrmi_error_codes, 0, TOTAL_URL_ERR * sizeof(int)); \
    loc_jrmi_avgtime->jrmi_num_hits = 0;\
    loc_jrmi_avgtime->jrmi_num_tries = 0;\
    loc_jrmi_avgtime->jrmi_avg_time = 0;\
    loc_jrmi_avgtime->jrmi_min_time = 0xFFFFFFFF;\
    loc_jrmi_avgtime->jrmi_max_time = 0;\
    loc_jrmi_avgtime->jrmi_tot_time = 0;\
    loc_jrmi_avgtime->jrmi_fetches_started = 0;\
    loc_jrmi_avgtime->jrmi_num_con_initiated = 0;\
    loc_jrmi_avgtime->jrmi_num_con_succ = 0;\
    loc_jrmi_avgtime->jrmi_num_con_fail = 0;\
    loc_jrmi_avgtime->jrmi_num_con_break = 0;\
    loc_jrmi_avgtime->jrmi_total_bytes = 0;\
    loc_jrmi_avgtime->jrmi_tx_bytes = 0;\
    loc_jrmi_avgtime->jrmi_rx_bytes = 0;\
  }\
}

#define CHILD_JRMI_SET_CUM_FIELD_OF_AVGTIME(a)\
  if(global_settings->protocol_enabled & JRMI_PROTOCOL_ENABLED) { \
    SET_MIN(a->jrmi_c_min_time, a->jrmi_min_time); \
    SET_MAX(a->jrmi_c_max_time, a->jrmi_max_time); \
    (a)->jrmi_num_connections = jrmi_num_connections; \
    (a)->jrmi_fetches_completed += (a)->jrmi_num_tries;\
    (a)->jrmi_succ_fetches += (a)->jrmi_num_hits;\
    (a)->cum_jrmi_fetches_started += (a)->jrmi_fetches_started;\
    (a)->jrmi_c_tot_time += (a)->jrmi_tot_time;\
    (a)->jrmi_c_tot_total_bytes +=(a)->jrmi_total_bytes;\
    (a)->jrmi_c_tot_tx_bytes += (a)->jrmi_tx_bytes;\
    (a)->jrmi_c_tot_rx_bytes += (a)->jrmi_rx_bytes;\
    (a)->jrmi_c_num_con_succ += (a)->jrmi_num_con_succ;\
    (a)->jrmi_c_num_con_initiated += (a)->jrmi_num_con_initiated;\
    (a)->jrmi_c_num_con_fail += (a)->jrmi_num_con_fail;\
    (a)->jrmi_c_num_con_break += (a)->jrmi_num_con_break;\
    memcpy(a->cum_jrmi_error_codes, a->jrmi_error_codes, sizeof(int) * TOTAL_URL_ERR); \
  }
extern char *jrmiRepPointer;
extern int jrmiRepLength;
extern void jrmi_set_jrmi_avgtime_ptr();
extern void jrmi_update_jrmi_avgtime_size();
extern void fill_jrmi_gp (avgtime **jrmi_avg, cavgtime **jrmi_cavg);
extern void print_jrmi_throughput(FILE *fp1, FILE *fp2, int is_periodic, avgtime *avg, cavgtime *cavg);
extern void jrmi_log_summary_data (avgtime *avg, double *jrmi_data, FILE *fp, cavgtime *cavg);
extern void jrmi_update_jrmi_cavgtime_size();
extern int kw_set_jrmi_timeout(char *buf, int *to_change, char *err_msg);
extern void jrmi_timeout_handle( ClientData client_data, u_ns_ts_t now );
extern void delete_jrmi_timeout_timer(connection *cptr);
extern void make_jrmi_msg( connection *cptr, u_ns_ts_t now);
extern int  kw_set_jrmicall_timeout(char *buf, int* global_set , int* global_port);
extern int process_njvm_resp_buffer(Msg_com_con *mccptr, char *page_name, int status, VUser* vptr, int var);
extern int ns_parse_jrmi(char *api_name, char *api_to_run, FILE *flow_fp, FILE *outfp, char *flow_filename, char *flowout_filename, int sess_idx);
extern void add_jrmi_timeout_timer(connection *cptr, u_ns_ts_t now);
extern void jrmi_con_setup(VUser *vptr, action_request_Shr *url_num, u_ns_ts_t now);
extern void handle_jrmi_write(connection *cptr, u_ns_ts_t now);
extern int handle_jrmi_read( connection *cptr, u_ns_ts_t now );
#endif
