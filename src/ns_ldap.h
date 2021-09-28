
#ifndef LDAP_H
#define LDAP_H


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
#include "ns_page_dump.h"

#define LDAP_LOGIN   0
#define LDAP_LOGOUT  1
#define LDAP_ADD     2
#define LDAP_SEARCH  3
#define LDAP_UPDATE  4
#define LDAP_DELETE  5
#define LDAP_MODIFY  6
#define LDAP_RENAME  7

//operation
#define BINDREQUEST 		0
#define BINDRESPONSE 		1
#define UNBINDREQUEST 		2
#define SEARCHREQUEST		3
#define SEARCHRESULTENTRY 	4
#define SEARCHRESULTDONE 	5
#define SEARCHRESULTREFERENCE 	6
#define MODIFYREQUEST 		6
#define MODIFYRESPONSE 		8
#define ADDREQUEST 		8      //need to veryfy operation codes , have some confusion
#define ADDRESPONSE 		10
#define DELREQUEST		10
#define DELRESPONSE 		12
#define MODIFYDNREQUEST 	13
#define MODIFYDNRESPONSE 	14
#define COMPAREREQUEST 		15
#define COMPARERESPONSE 	16
#define ABANDONREQUEST 		17
#define EXTENDEDREQUEST 	18
#define EXTENDEDRESPONSE 	19
#define INTERMEDIATERESPONSE 	20

#define BER_STRING_TYPE 0x04
#define BER_BOOLEAN_TYPE 0x01
#define BER_INTEGER_TYPE 0x02
#define BER_NULL_TYPE 0x05
#define BER_ENUMERATED_TYPE 0x0a
#define BER_SEQUENCE_TYPE 0x30
#define BER_SET_TYPE 0x31

// TODO: replace by enum and take another array and keep opcode in that so we can avoid switch cases. 
#define LDAP_SEARCH_AND 0x0
#define LDAP_SEARCH_OR 0x1
#define LDAP_SEARCH_NOT 0x2
#define LDAP_SEARCH_EQUALITY 0x3
#define LDAP_SEARCH_SUBSTRING 0x4
#define LDAP_SEARCH_PRESENT 0x7
#define LDAP_SEARCH_GREATEREQUAL 0x5
#define LDAP_SEARCH_LESSEQUAL 0x6
#define LDAP_SEARCH_APPROX_MATCH 0x8
#define LDAP_SEARCH_EXTENSIBLE 0x9

typedef struct {  // for search structure
   char *base;
   char *scope;
   char *timelimit;
   char *deref;
   char *typesonly;
   char *sizelimit;

   char *filter;
   char *attribute;
}Search;

typedef struct { //for bind request
   char *version;
   char *dn_name;     //dn name
   char *type;           //to decide authentication type sasl/simple
   char *auth_name;
   char *credentials; //in case of sasl
}Authentication;

typedef struct {
   char *msg_buf;
   char *dn;
   char *mod_operation;  //for modify mode only
}Add;

typedef struct LdapOperand_t {
  u_int8_t operator;
  char tag[64];
  char value[128];
  union {
    struct LdapOperand_t *operands;
    struct LdapOperand_t *next;
  };
  // link list for child.
  struct LdapOperand_t *children;
} LdapOperand_t;


typedef struct {
   char *dn;
   char *new_dn;
   char *del_old; 
}Rename;

typedef struct { 
   char *dn;
}Delete;

extern char *ldap_buffer;
extern int ldap_buffer_len;

extern void make_search_request(connection *cptr, unsigned char **req_buf, int* msg_len, Search ss);
extern void make_bind_request(connection *cptr, unsigned char **req_buf, int* msg_len, Authentication auth);
extern void make_add_request(connection *cptr, unsigned char **req_buf, int* msg_len, Add add, int operation);
extern void make_delete_request(connection *cptr, unsigned char **req_buf, int* msg_len, Delete del);

// This is structure for ldap avgtime
// Filled by child and send to parent in progress report
typedef struct {
  int ldap_num_connections;

  u_ns_8B_t ldap_total_bytes;
  u_ns_8B_t ldap_tx_bytes;
  u_ns_8B_t ldap_rx_bytes;

  u_ns_8B_t ldap_num_con_initiated;   //Number of initiated requests for making the connection (periodic)
  u_ns_8B_t ldap_num_con_succ;        //Number of succ requests for making the connection out of initiated (periodic)
  u_ns_8B_t ldap_num_con_fail;        //Number of failures requests for making the connection out of initiated (periodic)
  u_ns_8B_t ldap_num_con_break;       //Number of initiated requests for closing the connection (periodic)

  u_ns_8B_t ldap_num_hits;
  u_ns_8B_t ldap_num_tries;
  u_ns_8B_t ldap_avg_time;
  u_ns_8B_t ldap_min_time;
  u_ns_8B_t ldap_max_time;
  u_ns_8B_t ldap_tot_time;
  u_ns_8B_t ldap_fetches_started;

  u_ns_4B_t ldap_response[MAX_GRANULES+1];
  u_ns_4B_t ldap_error_codes[TOTAL_URL_ERR];   /* for url error codes 1-8 */
} LDAPAvgTime;  


// Cumulative counters - Used by parent ONLY
typedef struct {
  u_ns_8B_t ldap_c_tot_total_bytes;
  u_ns_8B_t ldap_c_tot_tx_bytes;
  u_ns_8B_t ldap_c_tot_rx_bytes;
  u_ns_8B_t ldap_c_num_con_initiated;
  u_ns_8B_t ldap_c_num_con_succ;
  u_ns_8B_t ldap_c_num_con_fail;
  u_ns_8B_t ldap_c_num_con_break;
  u_ns_8B_t ldap_succ_fetches;
  u_ns_8B_t ldap_fetches_completed;
  u_ns_8B_t ldap_c_min_time;
  u_ns_8B_t ldap_c_max_time;
  u_ns_8B_t ldap_c_tot_time;
  u_ns_8B_t ldap_c_avg_time;
  u_ns_8B_t cum_ldap_fetches_started;
  u_ns_4B_t cum_ldap_error_codes[TOTAL_URL_ERR];   /* for url error codes 1-8 */
} LDAPCAvgTime;

#ifndef CAV_MAIN
extern int g_ldap_avgtime_idx;
extern LDAPAvgTime *ldap_avgtime;
#else
extern __thread int g_ldap_avgtime_idx;
extern __thread LDAPAvgTime *ldap_avgtime;
#endif
extern int g_ldap_cavgtime_idx;
extern int g_LDAPcavgtime_size;

extern LDAPCAvgTime *ldap_cavgtime;

extern int ldap_num_connections; 

extern int ldap_operation;
// Set cumulative elements of LDAP struct a with b into a
#define SET_MIN_MAX_LDAP_CUMULATIVES(a, b)\
  NSDL2_MESSAGES(NULL, NULL, "Before ldap_c_min_time = %llu, ldap_c_max_time = %llu", (a)->ldap_c_min_time, (b)->ldap_min_time); \
  SET_MIN ((a)->ldap_c_min_time, (b)->ldap_min_time);\
  SET_MAX ((a)->ldap_c_max_time, (b)->ldap_max_time);\
  NSDL2_MESSAGES(NULL, NULL, "After ldap_c_min_time = %llu, ldap_c_max_time = %llu", (a)->ldap_c_min_time, (b)->ldap_min_time);

#define SET_MIN_MAX_LDAP_CUMULATIVES_PARENT(a, b)\
  NSDL2_MESSAGES(NULL, NULL, "Before ldap_c_min_time = %llu, ldap_c_max_time = %llu", (a)->ldap_c_min_time, (b)->ldap_c_min_time); \
  SET_MIN ((a)->ldap_c_min_time, (b)->ldap_c_min_time);\
  SET_MAX ((a)->ldap_c_max_time, (b)->ldap_c_max_time);\
  NSDL2_MESSAGES(NULL, NULL, "After ldap_c_min_time = %llu, ldap_c_max_time = %llu", (a)->ldap_c_min_time, (b)->ldap_c_min_time);

//Set  periodic elements of LDAP struct a with b into a
#define SET_MIN_MAX_LDAP_PERIODICS(a, b)\
  SET_MIN ((a)->ldap_min_time, (b)->ldap_min_time);\
  SET_MAX ((a)->ldap_max_time, (b)->ldap_max_time);

//accukumate cumulative elements of LDAP struct a with b into a 
#define ACC_LDAP_CUMULATIVES(a, b)\
  (a)->cum_ldap_fetches_started += (b)->ldap_fetches_started;\
  (a)->ldap_fetches_completed += (b)->ldap_num_tries;\
  NSDL2_MESSAGES(NULL, NULL, "Before ldap_succ_fetches = %llu, ldap_succ_fetches = %llu",(a)->ldap_succ_fetches, (b)->ldap_num_hits); \
  (a)->ldap_succ_fetches += (b)->ldap_num_hits;\
  NSDL2_MESSAGES(NULL, NULL, "after ldap_succ_fetches = %llu, ldap_succ_fetches = %llu",(a)->ldap_succ_fetches, (b)->ldap_num_hits); \
  (a)->ldap_c_tot_time += (b)->ldap_tot_time;\
  (a)->ldap_c_num_con_initiated += (b)->ldap_num_con_initiated;\
  (a)->ldap_c_num_con_succ += (b)->ldap_num_con_succ;\
  (a)->ldap_c_num_con_fail += (b)->ldap_num_con_fail;\
  (a)->ldap_c_num_con_break += (b)->ldap_num_con_break;\
  (a)->ldap_c_tot_total_bytes += (b)->ldap_total_bytes ;\
  (a)->ldap_c_tot_tx_bytes += (b)->ldap_tx_bytes;\
  (a)->ldap_c_tot_rx_bytes += (b)->ldap_rx_bytes;\
  for (i = 0; i < TOTAL_URL_ERR; i++) {\
    (a)->cum_ldap_error_codes[i] += (b)->ldap_error_codes[i]; \
  }

#define ACC_LDAP_CUMULATIVES_PARENT(a, b)\
  (a)->cum_ldap_fetches_started += (b)->cum_ldap_fetches_started;\
  (a)->ldap_fetches_completed += (b)->ldap_fetches_completed;\
  NSDL2_MESSAGES(NULL, NULL, "Before ldap_succ_fetches = %llu, ldap_succ_fetches = %llu",(a)->ldap_succ_fetches, (b)->ldap_succ_fetches); \
  (a)->ldap_succ_fetches += (b)->ldap_succ_fetches;\
  NSDL2_MESSAGES(NULL, NULL, "after ldap_succ_fetches = %llu, ldap_succ_fetches = %llu",(a)->ldap_succ_fetches, (b)->ldap_succ_fetches); \
  (a)->ldap_c_tot_time += (b)->ldap_c_tot_time;\
  (a)->ldap_c_num_con_initiated += (b)->ldap_c_num_con_initiated;\
  (a)->ldap_c_num_con_succ += (b)->ldap_c_num_con_succ;\
  (a)->ldap_c_num_con_fail += (b)->ldap_c_num_con_fail;\
  (a)->ldap_c_num_con_break += (b)->ldap_c_num_con_break;\
  (a)->ldap_c_tot_total_bytes += (b)->ldap_c_tot_total_bytes ;\
  (a)->ldap_c_tot_tx_bytes += (b)->ldap_c_tot_tx_bytes;\
  (a)->ldap_c_tot_rx_bytes += (b)->ldap_c_tot_rx_bytes;\
  for (i = 0; i < TOTAL_URL_ERR; i++) {\
    (a)->cum_ldap_error_codes[i] += (b)->cum_ldap_error_codes[i]; \
  }

#define ACC_LDAP_PERIODICS(a, b)\
  (a)->ldap_total_bytes += (b)->ldap_total_bytes ;\
  (a)->ldap_tx_bytes += (b)->ldap_tx_bytes;\
  (a)->ldap_rx_bytes += (b)->ldap_rx_bytes;\
  (a)->ldap_num_con_initiated += (b)->ldap_num_con_initiated;\
  (a)->ldap_num_con_succ += (b)->ldap_num_con_succ;\
  (a)->ldap_num_con_fail += (b)->ldap_num_con_fail;\
  (a)->ldap_num_con_break += (b)->ldap_num_con_break;\
  (a)->ldap_num_hits += (b)->ldap_num_hits;\
  (a)->ldap_num_tries += (b)->ldap_num_tries;\
  (a)->ldap_avg_time += (b)->ldap_avg_time;\
  (a)->ldap_tot_time += (b)->ldap_tot_time;\
  (a)->ldap_fetches_started += (b)->ldap_fetches_started;\
 /*(a)->ldap_response[MAX_GRANULES+1] += (b)->ldap_response[MAX_GRANULES+1];\
   (a)->ldap_error_codes[TOTAL_URL_ERR] += (b)->ldap_error_codes[TOTAL_URL_ERR];\
   (a)->cum_ldap_error_codes[TOTAL_URL_ERR] += (b)->cum_ldap_error_codes[TOTAL_URL_ERR];*/\
  for (i = 0; i < TOTAL_URL_ERR; i++) {\
    (a)->ldap_error_codes[i] += (b)->ldap_error_codes[i];\
  }
                 
#define CHILD_RESET_LDAP_AVGTIME(a) \
{\
  if(global_settings->protocol_enabled & LDAP_PROTOCOL_ENABLED) \
  { \
    LDAPAvgTime *loc_ldap_avgtime = (LDAPAvgTime*)((char*)a + g_ldap_avgtime_idx); \
    memset(loc_ldap_avgtime->ldap_error_codes, 0, TOTAL_URL_ERR * sizeof(int)); \
    loc_ldap_avgtime->ldap_num_hits = 0;\
    loc_ldap_avgtime->ldap_num_tries = 0;\
    loc_ldap_avgtime->ldap_avg_time = 0;\
    loc_ldap_avgtime->ldap_min_time = 0xFFFFFFFF;\
    loc_ldap_avgtime->ldap_max_time = 0;\
    loc_ldap_avgtime->ldap_tot_time = 0;\
    loc_ldap_avgtime->ldap_fetches_started = 0;\
    loc_ldap_avgtime->ldap_num_con_initiated = 0;\
    loc_ldap_avgtime->ldap_num_con_succ = 0;\
    loc_ldap_avgtime->ldap_num_con_fail = 0;\
    loc_ldap_avgtime->ldap_num_con_break = 0;\
    loc_ldap_avgtime->ldap_total_bytes = 0;\
    loc_ldap_avgtime->ldap_tx_bytes = 0;\
    loc_ldap_avgtime->ldap_rx_bytes = 0;\
  }\
}

#define CHILD_LDAP_SET_CUM_FIELD_OF_AVGTIME(a)\
  if(global_settings->protocol_enabled & LDAP_PROTOCOL_ENABLED) { \
    SET_MIN(a->ldap_c_min_time, a->ldap_min_time); \
    SET_MAX(a->ldap_c_max_time, a->ldap_max_time); \
    (a)->ldap_num_connections = ldap_num_connections; \
    (a)->ldap_fetches_completed += (a)->ldap_num_tries;\
    (a)->ldap_succ_fetches += (a)->ldap_num_hits;\
    (a)->cum_ldap_fetches_started += (a)->ldap_fetches_started;\
    (a)->ldap_c_tot_time += (a)->ldap_tot_time;\
    (a)->ldap_c_tot_total_bytes +=(a)->ldap_total_bytes;\
    (a)->ldap_c_tot_tx_bytes += (a)->ldap_tx_bytes;\
    (a)->ldap_c_tot_rx_bytes += (a)->ldap_rx_bytes;\
    (a)->ldap_c_num_con_succ += (a)->ldap_num_con_succ;\
    (a)->ldap_c_num_con_initiated += (a)->ldap_num_con_initiated;\
    (a)->ldap_c_num_con_fail += (a)->ldap_num_con_fail;\
    (a)->ldap_c_num_con_break += (a)->ldap_num_con_break;\
    memcpy(a->cum_ldap_error_codes, a->ldap_error_codes, sizeof(int) * TOTAL_URL_ERR); \
  }

#define LOG_LDAP_REQ(my_cptr, bytes_to_log)\
  vptr = my_cptr->vptr; \
  if(NS_IF_PAGE_DUMP_ENABLE && (runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_ALL_SESS \
                                || runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_PAGE_ON_HEADER_BASED) \
                            && (runprof_table_shr_mem[vptr->group_num].gset.trace_level > TRACE_URL_DETAIL)) \
  { \
    log_ldap_req(my_cptr, vptr, bytes_to_log); \
  }   

#define LOG_LDAP_RES(my_cptr, local_resp_buf)\
  VUser *vptr; \
  vptr = my_cptr->vptr; \
  if(NS_IF_PAGE_DUMP_ENABLE && (runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_ALL_SESS \
                                || runprof_table_shr_mem[vptr->group_num].gset.trace_on_fail == TRACE_PAGE_ON_HEADER_BASED) \
                            && (runprof_table_shr_mem[vptr->group_num].gset.trace_level > TRACE_URL_DETAIL)) \
  { \
    log_ldap_res(my_cptr, vptr, local_resp_buf); \
  }   

extern int ldap_decode(connection *cptr, char *buf, char *out, int in_len, int fd);
extern int ldap_read_int(const char *buf, int len);
extern int ns_parse_ldap(FILE *flow_fp, FILE *outfp,  char *flow_file, char *flow_outfile, int sess_idx, int operation);
extern int kw_set_ldap_timeout(char *buf, int *to_change, char *err_msg);
extern void ldap_timeout_handle( ClientData client_data, u_ns_ts_t now );
extern void ldap_set_ldap_avgtime_ptr();
extern void ldap_update_ldap_avgtime_size();
extern void fill_ldap_gp (avgtime **ldap_avg, cavgtime **ldap_cavg);
extern void print_ldap_throughput(FILE *fp1, FILE *fp2, int is_periodic, avgtime *avg, cavgtime *cavg);
extern void ldap_log_summary_data (avgtime *avg, double *ldap_data, FILE *fp, cavgtime *cavg);
extern void ldap_update_ldap_cavgtime_size();
extern void delete_ldap_timeout_timer(connection *cptr);
extern void make_ldap_request(connection *cptr, unsigned char *buffer, int *length);
extern void handle_ldap_write(connection *cptr, u_ns_ts_t now);
extern int handle_ldap_read(connection *cptr, u_ns_ts_t now);
extern void make_unbind_request(connection *cptr, unsigned char **req_buf, int* msg_len);
extern void make_rename_request(connection *cptr, unsigned char **req_buf,int* msg_len, Rename ren);
extern int copy_from_cptr_to_buf(connection *cptr, char *buf);
extern inline void get_ldap_msg_opcode(char *buffer, int *opcode);
extern void log_ldap_res(connection *cptr, VUser *vptr, char *buf);
#endif
