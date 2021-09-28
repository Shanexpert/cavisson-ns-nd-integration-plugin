#include "ns_trans_normalization.h"

#ifndef OUTPUT_H
#define OUTPUT_H

extern char heading[];
extern void print_report(FILE *fp1, FILE *fp2, int obj_type, int is_periodic, avgtime *average_time, cavgtime *cavg, char *heading);
extern void print_vuserinfo(FILE *fp1, FILE *fp2, int is_periodic, avgtime *avg, cavgtime *cavg);
extern void print_throughput(FILE *fp1, FILE *fp2, int is_periodic, avgtime *avg, cavgtime *cavg);
extern void log_summary_data (avgtime *avg, cavgtime *cavg, double *url_data, double *smtp_data, double *pop3_data, double *ftp_data, double *ldap_data, double *imap_data, double *jrmi_data, double *dns_data, double *pg_data, double *tx_data, double *ss_data, TxDataCum *savedTxData, double *ws_data);
extern void print_all_tx_report(FILE *fp1, FILE *fp2, int is_periodic, avgtime *avg, cavgtime *cavg, char *heading);
extern inline void copy_data_into_tx_buf(cavgtime *cavg, TxDataCum *gsavedTxData);
extern char g_test_start_time[];
extern void create_trans_data_file(int gen_idx, TxDataCum *savedTxData);
extern void send_tx_data(Msg_com_con *mccptr, int opcode);
extern void update_summary_top_field(int field_idx, char *buf, int update_tr_dir_summary_top_field);
extern unsigned long get_test_start_time_from_summary_top();

#define SET_MIN(a , b)\
	if ( (b) < (a)) (a) = (b)

#define SET_MAX(a , b)\
	if ( (b) > (a)) (a) = (b)


//Set cumulative elements of strct a with b into a
#define SET_MIN_MAX_CUMULATIVES_PARENT(a, b) {\
  SET_MIN ((a)->c_min_time, (b)->c_min_time);\
  SET_MAX ((a)->c_max_time, (b)->c_max_time);\
  SET_MIN ((a)->smtp_c_min_time, (b)->smtp_c_min_time);\
  SET_MAX ((a)->smtp_c_max_time, (b)->smtp_c_max_time);\
  SET_MIN ((a)->pop3_c_min_time, (b)->pop3_c_min_time);\
  SET_MAX ((a)->pop3_c_max_time, (b)->pop3_c_max_time);\
  SET_MIN ((a)->dns_c_min_time, (b)->dns_c_min_time);\
  SET_MAX ((a)->dns_c_max_time, (b)->dns_c_max_time);\
  SET_MIN ((a)->pg_c_min_time, (b)->pg_c_min_time);\
  SET_MAX ((a)->pg_c_max_time, (b)->pg_c_max_time);\
  SET_MIN ((a)->sess_c_min_time, (b)->sess_c_min_time);\
  SET_MAX ((a)->sess_c_max_time, (b)->sess_c_max_time);\
}

#define SET_MIN_MAX_CUMULATIVES(a, b) {\
  SET_MIN ((a)->pop3_c_min_time, (b)->pop3_overall_min_time);\
  SET_MAX ((a)->pop3_c_max_time, (b)->pop3_overall_max_time);\
  SET_MIN ((a)->smtp_c_min_time, (b)->smtp_min_time);\
  SET_MAX ((a)->smtp_c_max_time, (b)->smtp_max_time);\
  SET_MIN ((a)->dns_c_min_time, (b)->dns_overall_min_time);\
  SET_MAX ((a)->dns_c_max_time, (b)->dns_overall_max_time);\
  SET_MIN ((a)->pg_c_min_time, (b)->pg_min_time);\
  SET_MAX ((a)->pg_c_max_time, (b)->pg_max_time);\
  SET_MIN ((a)->sess_c_min_time, (b)->sess_min_time);\
  SET_MAX ((a)->sess_c_max_time, (b)->sess_max_time);\
  SET_MIN ((a)->c_min_time, (b)->url_overall_min_time);\
  SET_MAX ((a)->c_max_time, (b)->url_overall_max_time);\
}

#define SET_SRVIP_STATS(count, a, b, child_id) {\
  NSDL4_TRANS(NULL, NULL,"SRVIP: Method Called, a = %p, b = %p, total_srv_ip_entries = %d, child_id = %d",\
                           a, b, count, child_id);\
  for (i = 0; i < count; i++) {\
    int parent_index = g_svr_ip_loc2norm_table[child_id].nvm_svr_ip_loc2norm_table[i];\
    if(parent_index != -1){\
      a[parent_index].cur_url_req += b[i].cur_url_req;\
      strcpy(a[parent_index].ip, b[i].ip);\
    }\
    NSDL4_TRANS(NULL, NULL,"i = %d, parent_index = %d, a[parent_index].cur_url_req = %d, a[parent_index].ip = %s, "\
                           "b[i].cur_url_req = %d, b[i].ip = %s", i, parent_index, a[parent_index].cur_url_req, a[parent_index].ip,\
                            b[i].cur_url_req, b[i].ip);\
  }\
}

#define SET_MIN_MAX_PERIODICS_TX_STATS(count, a, b, child_id) {\
  NSDL4_TRANS(NULL, NULL,"Method Called, a = %p, b = %p, total_tx_entries = %d, child_id = %d",\
                           a, b, count, child_id);\
  for (i = 0; i < count; i++) {\
    int parent_index = g_tx_loc2norm_table[child_id].nvm_tx_loc2norm_table[i]; \
    if(parent_index != -1){\
      SET_MIN (a[parent_index].tx_min_time, b[i].tx_min_time);\
      SET_MIN (a[parent_index].tx_succ_min_time, b[i].tx_succ_min_time);\
      SET_MIN (a[parent_index].tx_failure_min_time, b[i].tx_failure_min_time);\
      SET_MIN (a[parent_index].tx_netcache_hit_min_time, b[i].tx_netcache_hit_min_time);\
      SET_MIN (a[parent_index].tx_netcache_miss_min_time, b[i].tx_netcache_miss_min_time);\
      SET_MIN (a[parent_index].tx_min_think_time, b[i].tx_min_think_time);\
      SET_MAX (a[parent_index].tx_max_time, b[i].tx_max_time);\
      SET_MAX (a[parent_index].tx_succ_max_time, b[i].tx_succ_max_time);\
      SET_MAX (a[parent_index].tx_failure_max_time, b[i].tx_failure_max_time);\
      SET_MAX (a[parent_index].tx_netcache_hit_max_time, b[i].tx_netcache_hit_max_time);\
      SET_MAX (a[parent_index].tx_netcache_miss_max_time, b[i].tx_netcache_miss_max_time);\
      SET_MAX (a[parent_index].tx_max_think_time, b[i].tx_max_think_time);\
      NSDL4_TRANS(NULL, NULL," i = %d, parent_index = %d, a[parent_index].tx_min_time = %u, b[i].tx_min_time = %u", i, parent_index, a[parent_index].tx_min_time, b[i].tx_min_time);\
   }\
 }\
}

//Set periodic elements of struct a with b into a
#define SET_MIN_MAX_PERIODICS(a, b) {\
  SET_MIN ((a)->min_time, (b)->min_time);\
  SET_MAX ((a)->max_time, (b)->max_time);\
  SET_MIN ((a)->url_overall_min_time, (b)->url_overall_min_time);\
  SET_MAX ((a)->url_overall_max_time, (b)->url_overall_max_time);\
  SET_MIN ((a)->url_failure_min_time, (b)->url_failure_min_time);\
  SET_MAX ((a)->url_failure_max_time, (b)->url_failure_max_time);\
  SET_MIN ((a)->url_dns_min_time, (b)->url_dns_min_time);\
  SET_MAX ((a)->url_dns_max_time, (b)->url_dns_max_time);\
  SET_MIN ((a)->url_conn_min_time, (b)->url_conn_min_time);\
  SET_MAX ((a)->url_conn_max_time, (b)->url_conn_max_time);\
  SET_MIN ((a)->url_ssl_min_time, (b)->url_ssl_min_time);\
  SET_MAX ((a)->url_ssl_max_time, (b)->url_ssl_max_time);\
  SET_MIN ((a)->url_frst_byte_rcv_min_time, (b)->url_frst_byte_rcv_min_time);\
  SET_MAX ((a)->url_frst_byte_rcv_max_time, (b)->url_frst_byte_rcv_max_time);\
  SET_MIN ((a)->url_dwnld_min_time, (b)->url_dwnld_min_time);\
  SET_MAX ((a)->url_dwnld_max_time, (b)->url_dwnld_max_time);\
  SET_MIN ((a)->smtp_min_time, (b)->smtp_min_time);\
  SET_MAX ((a)->smtp_max_time, (b)->smtp_max_time);\
  SET_MIN ((a)->pop3_overall_min_time, (b)->pop3_overall_min_time);\
  SET_MAX ((a)->pop3_overall_max_time, (b)->pop3_overall_max_time);\
  SET_MIN ((a)->pop3_failure_min_time, (b)->pop3_failure_min_time);\
  SET_MAX ((a)->pop3_failure_max_time, (b)->pop3_failure_max_time);\
  SET_MIN ((a)->pop3_min_time, (b)->pop3_min_time);\
  SET_MAX ((a)->pop3_max_time, (b)->pop3_max_time);\
  SET_MIN ((a)->dns_overall_min_time, (b)->dns_overall_min_time);\
  SET_MAX ((a)->dns_overall_max_time, (b)->dns_overall_max_time);\
  SET_MIN ((a)->dns_failure_min_time, (b)->dns_failure_min_time);\
  SET_MAX ((a)->dns_failure_max_time, (b)->dns_failure_max_time);\
  SET_MIN ((a)->dns_min_time, (b)->dns_min_time);\
  SET_MAX ((a)->dns_max_time, (b)->dns_max_time);\
  SET_MIN ((a)->pg_min_time, (b)->pg_min_time);\
  SET_MAX ((a)->pg_max_time, (b)->pg_max_time);\
  SET_MIN ((a)->pg_succ_min_resp_time, (b)->pg_succ_min_resp_time);\
  SET_MAX ((a)->pg_succ_max_resp_time, (b)->pg_succ_max_resp_time);\
  SET_MIN ((a)->pg_fail_min_resp_time, (b)->pg_fail_min_resp_time);\
  SET_MAX ((a)->pg_fail_max_resp_time, (b)->pg_fail_max_resp_time);\
  SET_MIN ((a)->tx_min_time, (b)->tx_min_time);\
  SET_MAX ((a)->tx_max_time, (b)->tx_max_time);\
  SET_MIN ((a)->tx_succ_min_resp_time, (b)->tx_succ_min_resp_time);\
  SET_MAX ((a)->tx_succ_max_resp_time, (b)->tx_succ_max_resp_time);\
  SET_MIN ((a)->tx_fail_min_resp_time, (b)->tx_fail_min_resp_time);\
  SET_MAX ((a)->tx_fail_max_resp_time, (b)->tx_fail_max_resp_time);\
  SET_MIN ((a)->tx_min_think_time, (b)->tx_min_think_time);\
  SET_MAX ((a)->tx_max_think_time, (b)->tx_max_think_time);\
  SET_MIN ((a)->sess_min_time, (b)->sess_min_time);\
  SET_MAX ((a)->sess_max_time, (b)->sess_max_time);\
  SET_MIN ((a)->sess_succ_min_resp_time, (b)->sess_succ_min_resp_time);\
  SET_MAX ((a)->sess_succ_max_resp_time, (b)->sess_succ_max_resp_time);\
  SET_MIN ((a)->sess_fail_min_resp_time, (b)->sess_fail_min_resp_time);\
  SET_MAX ((a)->sess_fail_max_resp_time, (b)->sess_fail_max_resp_time);\
  SET_MIN ((a)->page_js_proc_time_min, (b)->page_js_proc_time_min);\
  SET_MAX ((a)->page_js_proc_time_max, (b)->page_js_proc_time_max);\
  SET_MIN ((a)->page_proc_time_min, (b)->page_proc_time_min);\
  SET_MAX ((a)->page_proc_time_max, (b)->page_proc_time_max);\
  SET_MIN ((a)->bind_sock_fail_min, (b)->bind_sock_fail_min);\
  SET_MAX ((a)->bind_sock_fail_max, (b)->bind_sock_fail_max);\
}

//accukumate cumulative elements of struct a with b into a

#define ACC_CUMULATIVES(a, b) {\
  (a)->c_num_con_succ += (b)->num_con_succ;\
  (a)->c_num_con_initiated += (b)->num_con_initiated;\
  (a)->c_num_con_fail += (b)->num_con_fail;\
  (a)->c_num_con_break += (b)->num_con_break;\
  (a)->c_ssl_new += (b)->ssl_new;\
  (a)->c_ssl_reused += (b)->ssl_reused;\
  (a)->c_ssl_reuse_attempted += (b)->ssl_reuse_attempted;\
  (a)->cum_smtp_fetches_started += (b)->smtp_fetches_started;\
  (a)->smtp_fetches_completed += (b)->smtp_num_tries;\
  (a)->smtp_succ_fetches += (b)->smtp_num_hits;\
  (a)->smtp_c_tot_time += (b)->smtp_tot_time;\
  (a)->smtp_c_num_con_initiated += (b)->smtp_num_con_initiated;\
  (a)->smtp_c_num_con_succ += (b)->smtp_num_con_succ;\
  (a)->smtp_c_num_con_fail += (b)->smtp_num_con_fail;\
  (a)->smtp_c_num_con_break += (b)->smtp_num_con_break;\
  (a)->cum_pop3_fetches_started += (b)->pop3_fetches_started;\
  (a)->pop3_fetches_completed += (b)->pop3_num_tries;\
  (a)->pop3_succ_fetches += (b)->pop3_num_hits;\
  (a)->pop3_c_tot_time += (b)->pop3_overall_tot_time;\
  (a)->pop3_c_num_con_initiated += (b)->pop3_num_con_initiated;\
  (a)->pop3_c_num_con_succ += (b)->pop3_num_con_succ;\
  (a)->pop3_c_num_con_fail += (b)->pop3_num_con_fail;\
  (a)->pop3_c_num_con_break += (b)->pop3_num_con_break;\
  (a)->cum_dns_fetches_started += (b)->dns_fetches_started;\
  (a)->dns_fetches_completed += (b)->dns_num_tries;\
  (a)->dns_succ_fetches += (b)->dns_num_hits;\
  (a)->dns_c_tot_time += (b)->dns_overall_tot_time;\
  (a)->dns_c_num_con_initiated += (b)->dns_num_con_initiated;\
  (a)->dns_c_num_con_succ += (b)->dns_num_con_succ;\
  (a)->dns_c_num_con_fail += (b)->dns_num_con_fail;\
  (a)->dns_c_num_con_break += (b)->dns_num_con_break;\
  (a)->cum_pg_fetches_started += (b)->pg_fetches_started;\
  (a)->pg_fetches_completed += (b)->pg_tries;\
  (a)->pg_succ_fetches += (b)->pg_hits;\
  (a)->pg_c_tot_time += (b)->pg_tot_time;\
  (a)->cum_ss_fetches_started += (b)->ss_fetches_started;\
  (a)->sess_fetches_completed += (b)->sess_tries;\
  (a)->sess_succ_fetches += (b)->sess_hits;\
  (a)->sess_c_tot_time += (b)->sess_tot_time;\
  (a)->cum_fetches_started += (b)->fetches_started;\
  (a)->url_fetches_completed += (b)->num_tries;\
  (a)->url_succ_fetches += (b)->num_hits;\
  (a)->c_tot_time += (b)->url_overall_tot_time;\
  for (i = 0; i < TOTAL_PAGE_ERR; i++) {\
    (a)->cum_pg_error_codes[i] += (b)->pg_error_codes[i];\
  }\
  for (i = 0; i < TOTAL_SESS_ERR; i++) {\
    (a)->cum_sess_error_codes[i] += (b)->sess_error_codes[i];\
  }\
  for (i = 0; i < TOTAL_URL_ERR; i++) {\
    (a)->cum_url_error_codes[i] += (b)->url_error_codes[i]; \
  }\
  for (i = 0; i < TOTAL_TX_ERR; i++) {\
    (a)->cum_tx_error_codes[i] += (b)->tx_error_codes[i];\
  }\
  for (i = 0; i < TOTAL_URL_ERR; i++) {\
    (a)->cum_smtp_error_codes[i] += (b)->smtp_error_codes[i]; \
    (a)->cum_pop3_error_codes[i] += (b)->pop3_error_codes[i]; \
    (a)->cum_dns_error_codes[i] += (b)->dns_error_codes[i]; \
  }\
  /*bug 70480  assign num_srv_push to cum_srv_pushed_resources*/\
  (a)->cum_srv_pushed_resources += (b)->num_srv_push;\
}

//accukumate cumulative elements of struct a with b into a
#define ACC_CUMULATIVES_PARENT(a, b) {\
  (a)->cum_fetches_started += (b)->cum_fetches_started;\
  (a)->url_fetches_completed += (b)->url_fetches_completed;\
  (a)->url_succ_fetches += (b)->url_succ_fetches;\
  (a)->c_tot_time += (b)->c_tot_time;\
  (a)->cum_smtp_fetches_started += (b)->cum_smtp_fetches_started;\
  (a)->smtp_fetches_completed += (b)->smtp_fetches_completed;\
  (a)->smtp_succ_fetches += (b)->smtp_succ_fetches;\
  (a)->smtp_c_tot_time += (b)->smtp_c_tot_time;\
  (a)->cum_pop3_fetches_started += (b)->cum_pop3_fetches_started;\
  (a)->pop3_fetches_completed += (b)->pop3_fetches_completed;\
  (a)->pop3_succ_fetches += (b)->pop3_succ_fetches;\
  (a)->pop3_c_tot_time += (b)->pop3_c_tot_time;\
  (a)->cum_dns_fetches_started += (b)->cum_dns_fetches_started;\
  (a)->dns_fetches_completed += (b)->dns_fetches_completed;\
  (a)->dns_succ_fetches += (b)->dns_succ_fetches;\
  (a)->dns_c_tot_time += (b)->dns_c_tot_time;\
  (a)->cum_pg_fetches_started += (b)->cum_pg_fetches_started;\
  (a)->pg_fetches_completed += (b)->pg_fetches_completed;\
  (a)->pg_succ_fetches += (b)->pg_succ_fetches;\
  (a)->pg_c_tot_time += (b)->pg_c_tot_time;\
  (a)->cum_ss_fetches_started += (b)->cum_ss_fetches_started;\
  (a)->sess_fetches_completed += (b)->sess_fetches_completed;\
  (a)->sess_succ_fetches += (b)->sess_succ_fetches;\
  (a)->sess_c_tot_time += (b)->sess_c_tot_time;\
  (a)->c_num_con_initiated += (b)->c_num_con_initiated;\
  (a)->c_num_con_succ += (b)->c_num_con_succ;\
  (a)->c_num_con_fail += (b)->c_num_con_fail;\
  (a)->c_num_con_break += (b)->c_num_con_break;\
  (a)->c_ssl_new += (b)->c_ssl_new;\
  (a)->c_ssl_reused += (b)->c_ssl_reused;\
  (a)->c_ssl_reuse_attempted += (b)->c_ssl_reuse_attempted;\
  (a)->smtp_c_num_con_initiated += (b)->smtp_c_num_con_initiated;\
  (a)->smtp_c_num_con_succ += (b)->smtp_c_num_con_succ;\
  (a)->smtp_c_num_con_fail += (b)->smtp_c_num_con_fail;\
  (a)->smtp_c_num_con_break += (b)->smtp_c_num_con_break;\
  (a)->pop3_c_num_con_initiated += (b)->pop3_c_num_con_initiated;\
  (a)->pop3_c_num_con_fail += (b)->pop3_c_num_con_fail;\
  (a)->pop3_c_num_con_break += (b)->pop3_c_num_con_break;\
  (a)->dns_c_num_con_initiated += (b)->dns_c_num_con_initiated;\
  (a)->dns_c_num_con_succ += (b)->dns_c_num_con_succ;\
  (a)->dns_c_num_con_fail += (b)->dns_c_num_con_fail;\
  (a)->dns_c_num_con_break += (b)->dns_c_num_con_break;\
  for (i = 0; i < TOTAL_PAGE_ERR; i++) {\
    (a)->cum_pg_error_codes[i] += (b)->cum_pg_error_codes[i];\
  }\
  for (i = 0; i < TOTAL_TX_ERR; i++) {\
    (a)->cum_tx_error_codes[i] += (b)->cum_tx_error_codes[i];\
  }\
  for (i = 0; i < TOTAL_SESS_ERR; i++) {\
    (a)->cum_sess_error_codes[i] += (b)->cum_sess_error_codes[i];\
  }\
  for (i = 0; i < TOTAL_URL_ERR; i++) {\
    (a)->cum_url_error_codes[i] += (b)->cum_url_error_codes[i]; \
    (a)->cum_smtp_error_codes[i] += (b)->cum_smtp_error_codes[i]; \
    (a)->cum_pop3_error_codes[i] += (b)->cum_pop3_error_codes[i]; \
    (a)->cum_dns_error_codes[i] += (b)->cum_dns_error_codes[i]; \
  }                                                           \
  /*bug 70480  cum_srv_pushed_resources*/\
  (a)->cum_srv_pushed_resources += (b)->cum_srv_pushed_resources;\
}

#define ACC_PERIODICS_TX_STATS(count, a, b, child_id) {\
  for (i = 0; i < count; i++) {\
    int parent_index = g_tx_loc2norm_table[child_id].nvm_tx_loc2norm_table[i]; \
    if(parent_index != -1){\
      a[parent_index].tx_fetches_started += b[i].tx_fetches_started;\
      a[parent_index].tx_succ_fetches += b[i].tx_succ_fetches;\
      a[parent_index].tx_fetches_completed += b[i].tx_fetches_completed;\
      a[parent_index].tx_netcache_fetches += b[i].tx_netcache_fetches;\
      a[parent_index].tx_tot_time += b[i].tx_tot_time;\
      a[parent_index].tx_succ_tot_time += b[i].tx_succ_tot_time;\
      a[parent_index].tx_failure_tot_time += b[i].tx_failure_tot_time;\
      a[parent_index].tx_tot_sqr_time += b[i].tx_tot_sqr_time;\
      a[parent_index].tx_succ_tot_sqr_time += b[i].tx_succ_tot_sqr_time;\
      a[parent_index].tx_failure_tot_sqr_time += b[i].tx_failure_tot_sqr_time;\
      a[parent_index].tx_netcache_hit_tot_time += b[i].tx_netcache_hit_tot_time;\
      a[parent_index].tx_netcache_hit_tot_sqr_time += b[i].tx_netcache_hit_tot_sqr_time;\
      a[parent_index].tx_netcache_miss_tot_time += b[i].tx_netcache_miss_tot_time;\
      a[parent_index].tx_netcache_miss_tot_sqr_time += b[i].tx_netcache_miss_tot_sqr_time;\
      a[parent_index].tx_tot_think_time += b[i].tx_tot_think_time;\
      a[parent_index].tx_tx_bytes += b[i].tx_tx_bytes;\
      a[parent_index].tx_rx_bytes += b[i].tx_rx_bytes;\
    }\
  }\
}

#define COPY_PERIODICS_TX_STATS(start_idx, total_idx, a, b) {\
  for (i = start_idx ; i < total_idx; i++) {\
    a[i].tx_fetches_started += b[i].tx_fetches_started;\
    a[i].tx_succ_fetches += b[i].tx_succ_fetches;\
    a[i].tx_fetches_completed += b[i].tx_fetches_completed;\
    a[i].tx_netcache_fetches += b[i].tx_netcache_fetches;\
    a[i].tx_tot_time += b[i].tx_tot_time;\
    a[i].tx_tot_think_time += b[i].tx_tot_think_time;\
    a[i].tx_tx_bytes += b[i].tx_tx_bytes;\
    a[i].tx_rx_bytes += b[i].tx_rx_bytes;\
    a[i].tx_succ_tot_time += b[i].tx_succ_tot_time;\
    a[i].tx_failure_tot_time += b[i].tx_failure_tot_time;\
    a[i].tx_tot_sqr_time += b[i].tx_tot_sqr_time;\
    a[i].tx_succ_tot_sqr_time += b[i].tx_succ_tot_sqr_time;\
    a[i].tx_failure_tot_sqr_time += b[i].tx_failure_tot_sqr_time;\
    a[i].tx_netcache_hit_tot_time += b[i].tx_netcache_hit_tot_time;\
    a[i].tx_netcache_hit_tot_sqr_time += b[i].tx_netcache_hit_tot_sqr_time;\
    a[i].tx_netcache_miss_tot_time += b[i].tx_netcache_miss_tot_time;\
    a[i].tx_netcache_miss_tot_sqr_time += b[i].tx_netcache_miss_tot_sqr_time;\
    SET_MIN (a[i].tx_min_time, b[i].tx_min_time);\
    SET_MAX (a[i].tx_max_time, b[i].tx_max_time);\
    SET_MIN (a[i].tx_succ_min_time, b[i].tx_succ_min_time);\
    SET_MAX (a[i].tx_succ_max_time, b[i].tx_succ_max_time);\
    SET_MIN (a[i].tx_failure_min_time, b[i].tx_failure_min_time);\
    SET_MAX (a[i].tx_failure_max_time, b[i].tx_failure_max_time);\
    SET_MIN (a[i].tx_min_think_time, b[i].tx_min_think_time);\
    SET_MAX (a[i].tx_max_think_time, b[i].tx_max_think_time);\
  }\
}

#define COPY_PERIODICS_SERVER_IP_STATS(start_idx, total_idx, a, b) {\
  for (i = start_idx ; i < total_idx; i++) {\
    a[i].cur_url_req += b[i].cur_url_req;\
  }\
}


//accukumate periodic elements of struct a with b into a
#define ACC_PERIODICS(a, b) {\
  NSDL4_TRANS(NULL, NULL,"Method called: ACC_PERIODICS()"); \
  (a)->fetches_started += (b)->fetches_started;\
  (a)->fetches_sent += (b)->fetches_sent;\
  (a)->smtp_fetches_started += (b)->smtp_fetches_started;\
  (a)->pop3_fetches_started += (b)->pop3_fetches_started;\
  (a)->dns_fetches_started += (b)->dns_fetches_started;\
  (a)->num_hits += (b)->num_hits;\
  (a)->smtp_num_hits += (b)->smtp_num_hits;\
  (a)->pop3_num_hits += (b)->pop3_num_hits;\
  (a)->dns_num_hits += (b)->dns_num_hits;\
  (a)->num_tries += (b)->num_tries;\
  (a)->smtp_num_tries += (b)->smtp_num_tries;\
  (a)->pop3_num_tries += (b)->pop3_num_tries;\
  (a)->dns_num_tries += (b)->dns_num_tries;\
  (a)->url_overall_tot_time += (b)->url_overall_tot_time;\
  (a)->tot_time += (b)->tot_time;\
  (a)->url_failure_tot_time += (b)->url_failure_tot_time;\
  (a)->smtp_tot_time += (b)->smtp_tot_time;\
  (a)->pop3_overall_tot_time += (b)->pop3_overall_tot_time;\
  (a)->pop3_tot_time += (b)->pop3_tot_time;\
  (a)->pop3_failure_tot_time += (b)->pop3_failure_tot_time;\
  (a)->dns_overall_tot_time += (b)->dns_overall_tot_time;\
  (a)->dns_tot_time += (b)->dns_tot_time;\
  (a)->dns_failure_tot_time += (b)->dns_failure_tot_time;\
  (a)->url_dns_tot_time += (b)->url_dns_tot_time;\
  (a)->url_dns_count += (b)->url_dns_count;\
  (a)->url_conn_tot_time += (b)->url_conn_tot_time;\
  (a)->url_conn_count += (b)->url_conn_count;\
  (a)->url_ssl_tot_time += (b)->url_ssl_tot_time;\
  (a)->url_ssl_count += (b)->url_ssl_count;\
  (a)->url_frst_byte_rcv_tot_time += (b)->url_frst_byte_rcv_tot_time; \
  (a)->url_frst_byte_rcv_count += (b)->url_frst_byte_rcv_count;\
  (a)->url_dwnld_tot_time += (b)->url_dwnld_tot_time;\
  (a)->url_dwnld_count += (b)->url_dwnld_count;\
  (a)->pg_fetches_started += (b)->pg_fetches_started;\
  (a)->pg_hits += (b)->pg_hits;\
  (a)->pg_tries += (b)->pg_tries;\
  (a)->pg_tot_time += (b)->pg_tot_time;\
  (a)->pg_succ_tot_resp_time += (b)->pg_succ_tot_resp_time;\
  (a)->pg_fail_tot_resp_time += (b)->pg_fail_tot_resp_time;\
  (a)->tx_tot_time += (b)->tx_tot_time;\
  (a)->tx_succ_tot_resp_time += (b)->tx_succ_tot_resp_time;\
  (a)->tx_fail_tot_resp_time += (b)->tx_fail_tot_resp_time;\
  (a)->tx_tot_think_time += (b)->tx_tot_think_time;\
  (a)->tx_tx_bytes += (b)->tx_tx_bytes;\
  (a)->tx_rx_bytes += (b)->tx_rx_bytes;\
  (a)->page_proc_time_tot += (b)->page_proc_time_tot;\
  (a)->page_js_proc_time_tot += (b)->page_js_proc_time_tot;\
  (a)->tx_fetches_started += (b)->tx_fetches_started;\
  (a)->tx_succ_fetches += (b)->tx_succ_fetches;\
  (a)->tx_fetches_completed += (b)->tx_fetches_completed;\
  (a)->tx_tot_sqr_time += (b)->tx_tot_sqr_time;\
  (a)->ss_fetches_started += (b)->ss_fetches_started;\
  (a)->sess_hits += (b)->sess_hits;\
  (a)->sess_tries += (b)->sess_tries;\
  (a)->sess_tot_time += (b)->sess_tot_time;\
  (a)->sess_succ_tot_resp_time += (b)->sess_succ_tot_resp_time;\
  (a)->sess_fail_tot_resp_time += (b)->sess_fail_tot_resp_time;\
  (a)->num_con_succ += (b)->num_con_succ;\
  (a)->num_con_initiated += (b)->num_con_initiated;\
  (a)->num_con_fail += (b)->num_con_fail;\
  (a)->num_con_break += (b)->num_con_break;\
  (a)->ssl_new += (b)->ssl_new;\
  (a)->ssl_reused += (b)->ssl_reused;\
  (a)->ssl_reuse_attempted += (b)->ssl_reuse_attempted;\
  (a)->smtp_num_con_succ += (b)->smtp_num_con_succ;\
  (a)->smtp_num_con_initiated += (b)->smtp_num_con_initiated;\
  (a)->smtp_num_con_fail += (b)->smtp_num_con_fail;\
  (a)->smtp_num_con_break += (b)->smtp_num_con_break;\
  (a)->pop3_num_con_succ += (b)->pop3_num_con_succ;\
  (a)->pop3_num_con_initiated += (b)->pop3_num_con_initiated;\
  (a)->pop3_num_con_fail += (b)->pop3_num_con_fail;\
  (a)->pop3_num_con_break += (b)->pop3_num_con_break;\
  (a)->dns_num_con_succ += (b)->dns_num_con_succ;\
  (a)->dns_num_con_initiated += (b)->dns_num_con_initiated;\
  (a)->dns_num_con_fail += (b)->dns_num_con_fail;\
  (a)->dns_num_con_break += (b)->dns_num_con_break;\
  (a)->bind_sock_fail_tot += (b)->bind_sock_fail_tot;\
  NSDL4_TRANS(NULL, NULL, "TOTAL_PAGE_ERR = %d", TOTAL_PAGE_ERR);\
  for (i = 0; i < TOTAL_PAGE_ERR; i++) {\
    (a)->pg_error_codes[i] += (b)->pg_error_codes[i];\
  }\
  NSDL4_TRANS(NULL, NULL, "TOTAL_TX_ERR = %d", TOTAL_TX_ERR);\
  for (i = 0; i < TOTAL_TX_ERR; i++) {\
    (a)->tx_error_codes[i] += (b)->tx_error_codes[i];\
  }\
  NSDL4_TRANS(NULL, NULL, "TOTAL_SESS_ERR = %d", TOTAL_SESS_ERR);\
  for (i = 0; i < TOTAL_SESS_ERR; i++) {\
    (a)->sess_error_codes[i] += (b)->sess_error_codes[i];\
  }\
  NSDL4_TRANS(NULL, NULL, "TOTAL_URL_ERR = %d", TOTAL_URL_ERR);\
  for (i = 0; i < TOTAL_URL_ERR; i++) {\
    (a)->url_error_codes[i] += (b)->url_error_codes[i];       \
    (a)->smtp_error_codes[i] += (b)->smtp_error_codes[i];     \
    (a)->pop3_error_codes[i] += (b)->pop3_error_codes[i];     \
    (a)->dns_error_codes[i] += (b)->dns_error_codes[i];     \
  }\
  /*bug 70480  copy num_srv_push*/\
  (a)->num_srv_push += (b)->num_srv_push;\
}
           
#define ACC_CUM_TX_STATS(count, a, b, child_id) {\
  for (i = 0; i < count; i++) {\
    int parent_index = g_tx_loc2norm_table[child_id].nvm_tx_loc2norm_table[i]; \
    if(parent_index != -1){\
      a[parent_index].tx_c_fetches_started += b[i].tx_fetches_started;\
      a[parent_index].tx_c_fetches_completed += b[i].tx_fetches_completed;\
      a[parent_index].tx_c_succ_fetches += b[i].tx_succ_fetches;\
      a[parent_index].tx_c_netcache_fetches += b[i].tx_netcache_fetches;\
      a[parent_index].tx_c_tot_time += b[i].tx_tot_time;\
      a[parent_index].tx_c_tot_sqr_time += b[i].tx_tot_sqr_time;\
      NSDL4_TRANS(NULL, NULL, "TxIdx = %d, parent_index = %d, tx_c_fetches_started (a) = %llu, tx_c_fetches_started (b) = %llu, tx_c_fetches_completed (a) = %llu, tx_c_fetches_completed (b) = %llu, tx_c_succ_fetches (a) = %llu, tx_c_succ_fetches (b) = %llu, tx_c_tot_time (a) = %llu, tx_c_tot_time (b) = %llu, tx_c_tot_sqr_time (a) = %llu, tx_c_tot_sqr_time (b) = %llu", i, parent_index, a[parent_index].tx_c_fetches_started, b[i].tx_fetches_started, a[parent_index].tx_c_fetches_completed, b[i].tx_fetches_completed, a[parent_index].tx_c_succ_fetches, b[i].tx_succ_fetches, a[parent_index].tx_c_tot_time, b[i].tx_tot_time, a[parent_index].tx_c_tot_sqr_time, b[i].tx_tot_sqr_time); \
   }\
 }\
}

#define ACC_TX_DATA(a, b)  {\
  (a)->tx_c_fetches_started += (b)->tx_fetches_started;\
  (a)->tx_c_fetches_completed += (b)->tx_fetches_completed;\
  (a)->tx_c_succ_fetches += (b)->tx_succ_fetches;\
  (a)->tx_c_tot_time += (b)->tx_tot_time;\
  (a)->tx_c_tot_sqr_time += (b)->tx_tot_sqr_time;\
}

#define ACC_CUM_TX_DATA(a, b) {\
  (a)->tx_c_fetches_started += (b)->tx_c_fetches_started;\
  (a)->tx_c_fetches_completed += (b)->tx_c_fetches_completed;\
  (a)->tx_c_succ_fetches += (b)->tx_c_succ_fetches;\
  (a)->tx_c_tot_time += (b)->tx_c_tot_time;\
  (a)->tx_c_tot_sqr_time += (b)->tx_c_tot_sqr_time;\
}
	    
#define SET_MIN_MAX_CUM_TX_STATS(count, a, b, child_id) {\
  NSDL4_TRANS(NULL, NULL, "Method Called, total_tx_entries = %d", count); \
  for (i = 0; i < count; i++) {\
    int parent_index = g_tx_loc2norm_table[child_id].nvm_tx_loc2norm_table[i]; \
    if(parent_index != -1){\
      SET_MIN (a[parent_index].tx_c_min_time, b[i].tx_min_time);\
      SET_MAX (a[parent_index].tx_c_max_time, b[i].tx_max_time);\
    NSDL4_TRANS(NULL, NULL, "i = %d, parent_index = %d, a[i].tx_c_min_time = %d, b[i].tx_min_time = %d, a[i].tx_c_max_time = %d, b[i].tx_max_time = %d", i, parent_index, a[parent_index].tx_c_min_time, b[i].tx_min_time, a[parent_index].tx_c_max_time, b[i].tx_max_time);\
    }\
  }\
}

#define SET_MIN_MAX_TX_CUMULATIVE(a, b) {\
  SET_MIN ((a)->tx_c_min_time, (b)->tx_min_time);\
  SET_MAX ((a)->tx_c_max_time, (b)->tx_max_time);\
}

#define SET_MIN_MAX_CUM_TX_CUMULATIVE(a, b) {\
  SET_MIN ((a)->tx_c_min_time, (b)->tx_c_min_time);\
  SET_MAX ((a)->tx_c_max_time, (b)->tx_c_max_time);\
}
#endif
