#ifndef NS_GROUP_DATA_H
#define NS_GROUP_DATA_H

#define SHOW_GROUP_DATA_ENABLED 1
#define SHOW_GROUP_DATA_DISABLED 0

#define GROUP_VUSER_SIZE (sizeof(GROUPVuser) * total_runprof_entries)
#define GROUP_AVGTIME_SIZE (sizeof(GROUPAvgTime) * total_runprof_entries)

#define GET_FTP_AVG(vptr, local_ftp_avg) \
  local_ftp_avg = (FTPAvgTime*)((char*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp)) + g_ftp_avgtime_idx);

#define GET_LDAP_AVG(vptr, local_ldap_avg) \
  local_ldap_avg = (LDAPAvgTime*)((char*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp)) + g_ldap_avgtime_idx);

#define GET_IMAP_AVG(vptr, local_imap_avg) \
  local_imap_avg = (IMAPAvgTime*)((char*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp)) + g_imap_avgtime_idx);

#define GET_JRMI_AVG(vptr, local_jrmi_avg) \
  local_jrmi_avg = (JRMIAvgTime*)((char*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp)) + g_jrmi_avgtime_idx);

#define GET_CACHE_AVG(vptr, local_cache_avg) \
  local_cache_avg = (CacheAvgTime*)((char*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp)) + g_cache_avgtime_idx);

#define GET_PROXY_AVG(vptr, local_proxy_avg) \
  local_proxy_avg = (ProxyAvgTime*)((char*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp)) + g_proxy_avgtime_idx);

#define GET_DOS_ATTACK_AVG(vptr, local_dos_attack_avg) \
  local_dos_attack_avg = (DosAttackAvgTime*)((char*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp)) + g_dos_attack_avgtime_idx);

#define DEC_HTTP_HTTPS_CONN_COUNTERS(vptr) \
  --num_connections; \
  if(SHOW_GRP_DATA) { \
    grp_vuser[vptr->group_num].cur_vusers_connection--;\
  }

#define INC_HTTP_HTTPS_NUM_CONN_BREAK_COUNTERS(vptr) \
  (average_time->num_con_break)++; \
  if(SHOW_GRP_DATA) { \
    avgtime *lol_average_time;\
    lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));\
    (lol_average_time->num_con_break)++; \
  }

#define DEC_SMTP_SMTPS_CONN_COUNTERS(vptr) \
  --smtp_num_connections; \
  (average_time->smtp_num_con_break)++; \
  if(SHOW_GRP_DATA) { \
    avgtime *lol_average_time;\
    lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));\
    (lol_average_time->smtp_num_connections--); \
    (lol_average_time->smtp_num_con_break)++; \
  }

#define DEC_POP3_SPOP3_CONN_COUNTERS(vptr) \
  --pop3_num_connections; \
  (average_time->pop3_num_con_break)++; \
  if(SHOW_GRP_DATA) { \
    avgtime *lol_average_time;\
    lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));\
    (lol_average_time->pop3_num_connections)--; \
    (lol_average_time->pop3_num_con_break)++; \
  }

#define INC_FTP_CONN_COUNTERS(vptr) \
  --ftp_num_connections; \
  (ftp_avgtime->ftp_num_con_break)++; \
  if(SHOW_GRP_DATA) { \
    FTPAvgTime *local_ftp_avg = NULL; \
    GET_FTP_AVG(vptr, local_ftp_avg); \
    (local_ftp_avg->ftp_num_con_break)++; \
  }

#define DEC_FTP_DATA_CONN_COUNTERS(vptr) \
  --ftp_num_connections; \
  (ftp_avgtime->ftp_num_con_break)++; \
  if(SHOW_GRP_DATA) { \
    FTPAvgTime *local_ftp_avg = NULL; \
    GET_FTP_AVG(vptr, local_ftp_avg); \
    (local_ftp_avg->ftp_num_con_break)++; \
  }

#define DEC_LDAP_CONN_COUNTERS(vptr) \
  --ldap_num_connections; \
  (ldap_avgtime->ldap_num_con_break)++; \
  if(SHOW_GRP_DATA) { \
    LDAPAvgTime *local_ldap_avg = NULL; \
    GET_LDAP_AVG(vptr, local_ldap_avg); \
    local_ldap_avg->ldap_num_con_break++; \
  }

#define DEC_DNS_CONN_COUNTERS(vptr) \
  --dns_num_connections; \
  (average_time->dns_num_con_break)++; \
  if(SHOW_GRP_DATA) { \
    avgtime *lol_average_time;\
    lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));\
    lol_average_time->dns_num_connections--; \
    lol_average_time->dns_num_con_break++; \
  } 

#define DEC_IMAP_IMAPS_CONN_COUNTERS(vptr) \
  --imap_num_connections; \
  (imap_avgtime->imap_num_con_break)++; \
  if(SHOW_GRP_DATA) { \
    IMAPAvgTime *local_imap_avg = NULL; \
    GET_IMAP_AVG(vptr, local_imap_avg); \
    (local_imap_avg->imap_num_con_break)++; \
  }

#define DEC_JRMI_CONN_COUNTERS(vptr) \
  --jrmi_num_connections; \
  (jrmi_avgtime->jrmi_num_con_break)++; \
  if(SHOW_GRP_DATA) { \
    JRMIAvgTime *local_jrmi_avg = NULL; \
    GET_JRMI_AVG(vptr, local_jrmi_avg); \
    (local_jrmi_avg->jrmi_num_con_break)++; \
  }

#define INC_HTTP_HTTPS_CONN_COUNTERS(vptr) \
  ++num_connections; \
  average_time->num_con_succ++; \
  if(SHOW_GRP_DATA) { \
    avgtime *lol_average_time;\
    lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));\
    lol_average_time->num_con_succ++; \
    grp_vuser[vptr->group_num].cur_vusers_connection++; \
  }

#define INC_SMTP_SMTPS_CONN_COUNTERS(vptr) \
  ++smtp_num_connections; \
  (average_time->smtp_num_con_succ)++; \
  if(SHOW_GRP_DATA) { \
    avgtime *lol_average_time;\
    lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));\
    (lol_average_time->smtp_num_connections++); \
    (lol_average_time->smtp_num_con_succ)++; \
  }

#define INC_POP3_SPOP3_CONN_COUNTERS(vptr) \
  ++pop3_num_connections; \
  (average_time->pop3_num_con_succ)++; \
  if(SHOW_GRP_DATA) { \
    avgtime *lol_average_time;\
    lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));\
    lol_average_time->pop3_num_connections++; \
    lol_average_time->pop3_num_con_succ++; \
  }

#define INC_FTP_DATA_CONN_COUNTERS(vptr) \
  ++ftp_num_connections; \
  (ftp_avgtime->ftp_num_con_succ)++; \
  if(SHOW_GRP_DATA) { \
    FTPAvgTime *local_ftp_avg = NULL; \
    GET_FTP_AVG(vptr, local_ftp_avg); \
    (local_ftp_avg->ftp_num_con_succ)++; \
  }

#define INC_LDAP_CONN_COUNTERS(vptr) \
  ++ldap_num_connections; \
  (ldap_avgtime->ldap_num_con_succ)++; \
  if(SHOW_GRP_DATA) { \
    LDAPAvgTime *local_ldap_avg = NULL; \
    GET_LDAP_AVG(vptr, local_ldap_avg); \
    local_ldap_avg->ldap_num_con_succ++; \
  }

#define INC_DNS_CONN_COUNTERS(vptr) \
  ++dns_num_connections; \
  (average_time->dns_num_con_succ)++; \
  if(SHOW_GRP_DATA) { \
    avgtime *lol_average_time;\
    lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));\
    lol_average_time->dns_num_connections++; \
    lol_average_time->dns_num_con_succ++; \
  } 

#define INC_IMAP_IMAPS_CONN_COUNTERS(vptr) \
  ++imap_num_connections; \
  (imap_avgtime->imap_num_con_succ)++; \
  if(SHOW_GRP_DATA) { \
    IMAPAvgTime *local_imap_avg = NULL; \
    GET_IMAP_AVG(vptr, local_imap_avg); \
    (local_imap_avg->imap_num_con_succ)++; \
  }

#define INC_JRMI_CONN_COUNTERS(vptr) \
  ++jrmi_num_connections; \
  (jrmi_avgtime->jrmi_num_con_succ)++; \
  if(SHOW_GRP_DATA) { \
    JRMIAvgTime *local_jrmi_avg = NULL; \
    GET_JRMI_AVG(vptr, local_jrmi_avg); \
    (local_jrmi_avg->jrmi_num_con_succ)++; \
  }

#define INC_SMTP_TX_BYTES_COUNTER(vptr, bytes_sent) \
  average_time->smtp_tx_bytes += bytes_sent; \
  if(SHOW_GRP_DATA) { \
    avgtime *lol_average_time;\
    lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));\
    lol_average_time->smtp_tx_bytes += bytes_sent; \
  }  

#define INC_POP3_SPOP3_TX_BYTES_COUNTER(vptr, bytes_sent) \
  average_time->pop3_tx_bytes += bytes_sent; \
  if(SHOW_GRP_DATA) { \
    avgtime *lol_average_time;\
    lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));\
    lol_average_time->pop3_tx_bytes += bytes_sent; \
  }  

#define INC_FTP_DATA_TX_BYTES_COUNTER(vptr, bytes_sent) \
  ftp_avgtime->ftp_tx_bytes += bytes_sent; \
  NSDL2_FTP(NULL, cptr, "bytes_sent = %d, ftp_avgtime->ftp_tx_bytes = %ld", bytes_sent, ftp_avgtime->ftp_tx_bytes); \
  if(SHOW_GRP_DATA) { \
    FTPAvgTime *local_ftp_avg = NULL; \
    GET_FTP_AVG(vptr, local_ftp_avg); \
    local_ftp_avg->ftp_tx_bytes += bytes_sent; \
  }  

#define INC_DNS_TX_BYTES_COUNTER(vptr, bytes_sent) \
  average_time->dns_tx_bytes += bytes_sent; \
  if(SHOW_GRP_DATA) { \
    avgtime *lol_average_time;\
    lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));\
    lol_average_time->dns_tx_bytes += bytes_sent; \
  }  

#define INC_LDAP_TX_BYTES_COUNTER(vptr, bytes_sent) \
  ldap_avgtime->ldap_tx_bytes += bytes_sent; \
  if(SHOW_GRP_DATA) { \
    LDAPAvgTime *local_ldap_avg = NULL; \
    GET_LDAP_AVG(vptr, local_ldap_avg); \
    local_ldap_avg->ldap_tx_bytes += bytes_sent; \
  }

#define INC_IMAP_TX_BYTES_COUNTER(vptr, bytes_sent) \
  imap_avgtime->imap_tx_bytes += bytes_sent; \
  if(SHOW_GRP_DATA) { \
    IMAPAvgTime *local_imap_avg = NULL; \
    GET_IMAP_AVG(vptr, local_imap_avg); \
    local_imap_avg->imap_tx_bytes += bytes_sent; \
  }

#define INC_JRMI_TX_BYTES_COUNTER(vptr, bytes_sent) \
  jrmi_avgtime->jrmi_tx_bytes += bytes_sent; \
  if(SHOW_GRP_DATA) { \
    JRMIAvgTime *local_jrmi_avg = NULL; \
    GET_JRMI_AVG(vptr, local_jrmi_avg); \
    local_jrmi_avg->jrmi_tx_bytes += bytes_sent; \
  }

#define INC_OTHER_TYPE_TX_BYTES_COUNTER(vptr, bytes_sent) \
  average_time->tx_bytes += bytes_sent; \
  if(SHOW_GRP_DATA) { \
    avgtime *lol_average_time;\
    lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));\
    lol_average_time->tx_bytes += bytes_sent; \
  }

#define INC_HTTP_FETCHES_STARTED_COUNTER(vptr) \
  average_time->fetches_started++; \
  if(SHOW_GRP_DATA) { \
    avgtime *lol_average_time;\
    lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));\
    lol_average_time->fetches_started++; \
  }

#define INC_SMTP_SMTPS_FETCHES_STARTED_COUNTER(vptr) \
  average_time->smtp_fetches_started++; \
  if(SHOW_GRP_DATA) { \
    avgtime *lol_average_time;\
    lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));\
    lol_average_time->smtp_fetches_started++; \
  }

#define INC_POP3_SPOP3_FETCHES_STARTED_COUNTER(vptr) \
  average_time->pop3_fetches_started++; \
  if(SHOW_GRP_DATA) { \
    avgtime *lol_average_time;\
    lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));\
    lol_average_time->pop3_fetches_started++; \
  }

#define INC_FTP_FETCHES_STARTED_COUNTER(vptr) \
  ftp_avgtime->ftp_fetches_started++; \
  if(SHOW_GRP_DATA) { \
    FTPAvgTime *local_ftp_avg = NULL; \
    GET_FTP_AVG(vptr, local_ftp_avg); \
    local_ftp_avg->ftp_fetches_started++; \
  }

#define INC_DNS_FETCHES_STARTED_COUNTER(vptr) \
  average_time->dns_fetches_started++; \
  if(SHOW_GRP_DATA) { \
    avgtime *lol_average_time;\
    lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));\
    lol_average_time->dns_fetches_started++; \
  }

#define INC_LDAP_FETCHES_STARTED_COUNTER(vptr) \
  ldap_avgtime->ldap_fetches_started++; \
  if(SHOW_GRP_DATA) { \
    LDAPAvgTime *local_ldap_avg = NULL; \
    GET_LDAP_AVG(vptr, local_ldap_avg); \
    local_ldap_avg->ldap_fetches_started++; \
  }

#define INC_IMAP_IMAPS_FETCHES_STARTED_COUNTER(vptr) \
  imap_avgtime->imap_fetches_started++; \
  if(SHOW_GRP_DATA) { \
    IMAPAvgTime *local_imap_avg = NULL; \
    GET_IMAP_AVG(vptr, local_imap_avg); \
    local_imap_avg->imap_fetches_started++; \
  }

#define INC_JRMI_FETCHES_STARTED_COUNTER(vptr) \
  jrmi_avgtime->jrmi_fetches_started++; \
  if(SHOW_GRP_DATA) { \
    JRMIAvgTime *local_jrmi_avg = NULL; \
    GET_JRMI_AVG(vptr, local_jrmi_avg); \
    local_jrmi_avg->jrmi_fetches_started++; \
  }

#define INC_HTTP_HTTPS_NUM_TRIES_COUNTER(vptr) \
  average_time->num_tries++; \
  average_time->url_error_codes[status]++; \
  if(SHOW_GRP_DATA) { \
    avgtime *lol_average_time;\
    lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));\
    lol_average_time->num_tries++; \
    lol_average_time->url_error_codes[status]++; \
  }

#define INC_SMTP_SMTPS_NUM_TRIES_COUNTER(vptr) \
  average_time->smtp_num_tries++; \
  average_time->smtp_error_codes[status]++; \
  if(SHOW_GRP_DATA) { \
    avgtime *lol_average_time;\
    lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));\
    lol_average_time->smtp_num_tries++; \
    lol_average_time->smtp_error_codes[status]++; \
  }

#define INC_POP3_SPOP3_NUM_TRIES_COUNTER(vptr) \
  average_time->pop3_num_tries++; \
  average_time->pop3_error_codes[status]++; \
  if(SHOW_GRP_DATA) { \
    avgtime *lol_average_time;\
    lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));\
    lol_average_time->pop3_num_tries++; \
    lol_average_time->pop3_error_codes[status]++; \
  }

#define INC_DNS_NUM_TRIES_COUNTER(vptr) \
  average_time->dns_num_tries++; \
  average_time->dns_error_codes[status]++; \
  if(SHOW_GRP_DATA) { \
    avgtime *lol_average_time;\
    lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));\
    lol_average_time->dns_num_tries++; \
    lol_average_time->dns_error_codes[status]++; \
  }

#define INC_LDAP_NUM_TRIES_COUNTER(vptr) \
  ldap_avgtime->ldap_num_tries++; \
  ldap_avgtime->ldap_error_codes[status]++; \
  if(SHOW_GRP_DATA) { \
    LDAPAvgTime *local_ldap_avg = NULL; \
    GET_LDAP_AVG(vptr, local_ldap_avg); \
    local_ldap_avg->ldap_num_tries++; \
    local_ldap_avg->ldap_error_codes[status]++; \
  }

#define INC_JRMI_NUM_TRIES_COUNTER(vptr) \
  jrmi_avgtime->jrmi_num_tries++; \
  jrmi_avgtime->jrmi_error_codes[status]++; \
  if(SHOW_GRP_DATA) { \
    JRMIAvgTime *local_jrmi_avg = NULL; \
    GET_JRMI_AVG(vptr, local_jrmi_avg); \
    local_jrmi_avg->jrmi_num_tries++; \
    local_jrmi_avg->jrmi_error_codes[status]++; \
  }

#define INC_FTP_NUM_TRIES_COUNTER(vptr) \
  ftp_avgtime->ftp_num_tries++; \
  ftp_avgtime->ftp_error_codes[status]++; \
  if(SHOW_GRP_DATA) { \
    FTPAvgTime *local_ftp_avg = NULL; \
    GET_FTP_AVG(vptr, local_ftp_avg); \
    local_ftp_avg->ftp_num_tries++; \
    local_ftp_avg->ftp_error_codes[status]++; \
  }

#define INC_IMAP_IMAPS_NUM_TRIES_COUNTER(vptr) \
  imap_avgtime->imap_num_tries++; \
  imap_avgtime->imap_error_codes[status]++; \
  if(SHOW_GRP_DATA) { \
    IMAPAvgTime *local_imap_avg = NULL; \
    GET_IMAP_AVG(vptr, local_imap_avg); \
    local_imap_avg->imap_num_tries++; \
    local_imap_avg->imap_error_codes[status]++; \
  }

#define CHILD_RESET_TX_AVGTIME(start_idx, a) \
  for (i = start_idx; i < total_tx_entries; i++) { \
    txData[i].tx_succ_fetches = 0; \
    txData[i].tx_fetches_completed = 0; \
    txData[i].tx_min_time = 0xFFFFFFFF; \
    NSDL1_GDF(NULL, NULL, "CHILD_RESET_TX_AVGTIME(): i = %d, txData[i].tx_min_time = %d, txData[i].tx_min_time = %p", i, txData[i].tx_min_time, &txData[i].tx_min_time); \
    txData[i].tx_max_time = 0; \
    txData[i].tx_avg_time = 0; \
    txData[i].tx_tot_time = 0; \
    txData[i].tx_tot_sqr_time = 0; \
    txData[i].tx_fetches_started = 0; \
    txData[i].tx_netcache_fetches = 0; \
    txData[i].tx_succ_min_time = 0xFFFFFFFF; \
    txData[i].tx_succ_max_time = 0; \
    txData[i].tx_succ_tot_time = 0; \
    txData[i].tx_succ_tot_sqr_time = 0; \
    txData[i].tx_failure_min_time = 0xFFFFFFFF; \
    txData[i].tx_failure_max_time = 0; \
    txData[i].tx_failure_tot_time = 0; \
    txData[i].tx_failure_tot_sqr_time = 0; \
    txData[i].tx_netcache_hit_min_time = 0xFFFFFFFF;\
    txData[i].tx_netcache_hit_max_time = 0;\
    txData[i].tx_netcache_hit_tot_time = 0;\
    txData[i].tx_netcache_hit_tot_sqr_time = 0;\
    txData[i].tx_netcache_miss_min_time = 0xFFFFFFFF;\
    txData[i].tx_netcache_miss_max_time = 0;\
    txData[i].tx_netcache_miss_tot_time = 0;\
    txData[i].tx_netcache_miss_tot_sqr_time = 0;\
    txData[i].tx_min_think_time = 0xFFFFFFFF; \
    txData[i].tx_max_think_time = 0; \
    txData[i].tx_tot_think_time = 0; \
    txData[i].tx_rx_bytes = 0; \
    txData[i].tx_tx_bytes = 0; \
  }
 
#define FILL_GRP_BASED_DATA \
  int z; \
  if(SHOW_GRP_DATA){\
    avgtime *lol_average_time;\
    for(z = 1; z <= total_runprof_entries; z++){\
      lol_average_time = (avgtime*)((char *)average_time + (z * g_avg_size_only_grp));\
      if ((global_settings->protocol_enabled & JMETER_PROTOCOL_ENABLED)) \
        lol_average_time->running_users = jmeter_get_running_vusers_by_sgrp(z-1); \
      else \
        lol_average_time->running_users = ((grp_vuser[z - 1].cur_vusers_active + grp_vuser[z - 1].cur_vusers_thinking + grp_vuser[z - 1].cur_vusers_waiting + grp_vuser[z - 1].cur_sp_users + grp_vuser[z - 1].cur_vusers_blocked + grp_vuser[z - 1].cur_vusers_paused)); \
    } \
  }

#define FILL_HTTP_TOT_TIME_FOR_GROUP_BASED(vptr, min, max, tot, count) \
  if(SHOW_GRP_DATA) \
   { \
    avgtime *lol_average_time;\
    lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));\
    SET_MIN (lol_average_time->min, download_time); \
    SET_MAX (lol_average_time->max, download_time); \
    lol_average_time->tot += download_time; \
    if(count) lol_average_time->num_hits++; \
  }

#define FILL_SMTP_TOT_TIME_FOR_GROUP_BASED(vptr) \
  if(SHOW_GRP_DATA) \
  { \
    avgtime *lol_average_time;\
    lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));\
    if (download_time < lol_average_time->smtp_min_time) { \
      lol_average_time->smtp_min_time = download_time; \
    } \
    if (download_time > lol_average_time->smtp_max_time) { \
      lol_average_time->smtp_max_time = download_time; \
    } \
    lol_average_time->smtp_tot_time += download_time; \
    lol_average_time->smtp_num_hits++; \
  }


#define FILL_POP3_TOT_TIME_FOR_GROUP_BASED(vptr, min, max, tot, count) \
  if(SHOW_GRP_DATA) \
  { \
    avgtime *lol_average_time;\
    lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));\
    SET_MIN (lol_average_time->min, download_time); \
    SET_MAX (lol_average_time->max, download_time); \
    lol_average_time->tot += download_time; \
    if(count) lol_average_time->pop3_num_hits++; \
  }

#define FILL_FTP_TOT_TIME_FOR_GROUP_BASED(vptr, min, max, tot, count) \
  if(SHOW_GRP_DATA) \
  { \
    FTPAvgTime *local_ftp_avg = NULL; \
    GET_FTP_AVG(vptr, local_ftp_avg); \
    SET_MIN (local_ftp_avg->min, download_time); \
    SET_MAX (local_ftp_avg->max, download_time); \
    local_ftp_avg->tot += download_time; \
    if(count) local_ftp_avg->ftp_num_hits++; \
  }

#define FILL_DNS_TOT_TIME_FOR_GROUP_BASED(vptr, min, max, tot, count) \
  if(SHOW_GRP_DATA) \
  { \
    avgtime *lol_average_time;\
    lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));\
    SET_MIN (lol_average_time->min, download_time); \
    SET_MAX (lol_average_time->max, download_time); \
    lol_average_time->tot += download_time; \
    if(count) lol_average_time->dns_num_hits++; \
  }
 
#define FILL_LDAP_TOT_TIME_FOR_GROUP_BASED(vptr) \
  if(SHOW_GRP_DATA) \
  { \
    LDAPAvgTime *local_ldap_avg = NULL; \
    GET_LDAP_AVG(vptr, local_ldap_avg); \
    if (download_time < local_ldap_avg->ldap_min_time) { \
      local_ldap_avg->ldap_min_time = download_time; \
    } \
    if (download_time > local_ldap_avg->ldap_max_time) { \
      local_ldap_avg->ldap_max_time = download_time; \
    } \
    local_ldap_avg->ldap_tot_time += download_time; \
    local_ldap_avg->ldap_num_hits++; \
  }

#define FILL_IMAP_TOT_TIME_FOR_GROUP_BASED(vptr, min, max, tot, count) \
  if(SHOW_GRP_DATA) \
  { \
    IMAPAvgTime *local_imap_avg = NULL; \
    GET_IMAP_AVG(vptr, local_imap_avg); \
    SET_MIN (local_imap_avg->min, download_time); \
    SET_MAX (local_imap_avg->max, download_time); \
    local_imap_avg->tot += download_time; \
    if(count) local_imap_avg->imap_num_hits++; \
  }

#define FILL_JRMI_TOT_TIME_FOR_GROUP_BASED(vptr) \
  if(SHOW_GRP_DATA) \
  { \
    JRMIAvgTime *local_jrmi_avg = NULL; \
    GET_JRMI_AVG(vptr, local_jrmi_avg); \
    if (download_time < local_jrmi_avg->jrmi_min_time) { \
      local_jrmi_avg->jrmi_min_time = download_time; \
    } \
    if (download_time > local_jrmi_avg->jrmi_max_time) { \
      local_jrmi_avg->jrmi_max_time = download_time; \
    } \
    local_jrmi_avg->jrmi_tot_time += download_time; \
    local_jrmi_avg->jrmi_num_hits++; \
  }
 
#define FILL_GRP_BASE_VUSER \
  int m; \
  if(SHOW_GRP_DATA) \
  { \
    avgtime *lol_average_time;\
    for(m = 1; m <= total_runprof_entries; m++) \
    { \
      if ((global_settings->protocol_enabled & JMETER_PROTOCOL_ENABLED)) \
        lol_average_time->cur_vusers_active = jmeter_get_active_vusers_by_sgrp(m-1); \
      else \
      { \
        lol_average_time = (avgtime*)((char *)average_time + (m * g_avg_size_only_grp));\
        lol_average_time->num_connections = grp_vuser[m - 1].cur_vusers_connection; \
        lol_average_time->cur_vusers_active = grp_vuser[m - 1].cur_vusers_active; \
        lol_average_time->cur_vusers_thinking = grp_vuser[m - 1].cur_vusers_thinking; \
        lol_average_time->cur_vusers_waiting = grp_vuser[m - 1].cur_vusers_waiting; \
        lol_average_time->cur_vusers_cleanup = grp_vuser[m - 1].cur_vusers_idling; \
        lol_average_time->cur_vusers_in_sp = grp_vuser[m - 1].cur_sp_users; \
        lol_average_time->cur_vusers_blocked = grp_vuser[m - 1].cur_vusers_blocked; \
        lol_average_time->cur_vusers_paused = grp_vuser[m - 1].cur_vusers_paused; \
      } \
    } \
  }

#define INC_GRP_BASED_NW_CACHE_STATS_PROBE_REQ(vptr) \
  if(SHOW_GRP_DATA) { \
    local_nw_cache_avg = (NetworkCacheStatsAvgTime*)((char*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp)) + g_network_cache_stats_avgtime_idx); \
    NSDL1_GDF(vptr, NULL, "average_time = %p, sgrp: group_num = %d, g_avg_size_only_grp = %d, g_network_cache_stats_avgtime_idx = %d", average_time, vptr->group_num, g_avg_size_only_grp, g_network_cache_stats_avgtime_idx); \
    local_nw_cache_avg->network_cache_stats_probe_req++; \
  }

#define INC_GRP_BASED_NON_NW_CACHE_USED_REQ(vptr) \
  if(SHOW_GRP_DATA) { \
    local_nw_cache_avg = (NetworkCacheStatsAvgTime*)((char*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp)) + g_network_cache_stats_avgtime_idx); \
    local_nw_cache_avg->non_network_cache_used_req++; \
  }
    
#define INC_GRP_BASED_NW_CACHE_STATS_NUM_FAIL(vptr) \
  if(SHOW_GRP_DATA) { \
    local_nw_cache_avg = (NetworkCacheStatsAvgTime*)((char*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp)) + g_network_cache_stats_avgtime_idx); \
    local_nw_cache_avg->network_cache_stats_num_fail++; \
  }

#define INC_GRP_BASED_NUM_NON_CACHEABLE_REQUESTS(vptr) \
  if(SHOW_GRP_DATA) { \
    local_nw_cache_avg = (NetworkCacheStatsAvgTime*)((char*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp)) + g_network_cache_stats_avgtime_idx); \
    local_nw_cache_avg->num_non_cacheable_requests++; \
  }
  
#define INC_GRP_BASED_NUM_CACHEABLE_REQUESTS(vptr) \
  if(SHOW_GRP_DATA) { \
    local_nw_cache_avg = (NetworkCacheStatsAvgTime*)((char*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp)) + g_network_cache_stats_avgtime_idx); \
    local_nw_cache_avg->num_cacheable_requests++; \
  }

#define INC_GRP_BASED_NW_CACHE_STATS_NUM_HITS(vptr) \
  if(SHOW_GRP_DATA) { \
    local_nw_cache_avg = (NetworkCacheStatsAvgTime*)((char*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp)) + g_network_cache_stats_avgtime_idx); \
    local_nw_cache_avg->network_cache_stats_num_hits++; \
  }
  
#define INC_GRP_BASED_NW_CACHE_STATS_NUM_MISSES(vptr) \
  if(SHOW_GRP_DATA) { \
    local_nw_cache_avg = (NetworkCacheStatsAvgTime*)((char*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp)) + g_network_cache_stats_avgtime_idx); \
    local_nw_cache_avg->network_cache_stats_num_misses++; \
  }
   
#define INC_GRP_BASED_NW_CACHE_STATS_STATE_OTHERS(vptr) \
  if(SHOW_GRP_DATA) { \
    local_nw_cache_avg = (NetworkCacheStatsAvgTime*)((char*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp)) + g_network_cache_stats_avgtime_idx); \
    local_nw_cache_avg->network_cache_stats_state_others++; \
  }
  
#define INC_GRP_BASED_NW_CACHE_REFRESH_HITS(vptr) \
  if(SHOW_GRP_DATA) { \
    local_nw_cache_avg = (NetworkCacheStatsAvgTime*)((char*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp)) + g_network_cache_stats_avgtime_idx); \
    local_nw_cache_avg->network_cache_refresh_hits++; \
  }

#define GRP_BASED_UPDATE_HIT_RESP_TIME_AND_THROUGHPUT_COUNTERS(d_time, vptr) \
  if(SHOW_GRP_DATA) { \
    local_nw_cache_avg = (NetworkCacheStatsAvgTime*)((char*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp)) + g_network_cache_stats_avgtime_idx); \
    if(d_time < local_nw_cache_avg->network_cache_stats_hits_response_time_min) \
    {                                                                           \
      local_nw_cache_avg->network_cache_stats_hits_response_time_min = d_time;  \
    }                                                                           \
    if(d_time > local_nw_cache_avg->network_cache_stats_hits_response_time_max) \
    {                                                                           \
      local_nw_cache_avg->network_cache_stats_hits_response_time_max = d_time;  \
    }                                                                           \
    local_nw_cache_avg->network_cache_stats_hits_response_time_total += d_time; \
    local_nw_cache_avg->content_size_recv_from_cache += cptr->tcp_bytes_recv;   \
  }

#define GRP_BASED_UPDATE_MISS_RESP_TIME_AND_THROUGHPUT_COUNTERS(d_time, vptr) \
  if(SHOW_GRP_DATA) { \
    local_nw_cache_avg = (NetworkCacheStatsAvgTime*)((char*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp)) + g_network_cache_stats_avgtime_idx); \
    if(d_time < local_nw_cache_avg->network_cache_stats_miss_response_time_min)\
    {                                                                          \
      local_nw_cache_avg->network_cache_stats_miss_response_time_min = d_time; \
    }                                                                          \
    if(d_time > local_nw_cache_avg->network_cache_stats_miss_response_time_max)\
    {                                                                          \
      local_nw_cache_avg->network_cache_stats_miss_response_time_max = d_time; \
    }                                                                          \
    local_nw_cache_avg->network_cache_stats_miss_response_time_total += d_time;\
    local_nw_cache_avg->content_size_not_recv_from_cache += cptr->tcp_bytes_recv;\
  }

#define GET_DNS_LOOKUP_AVG(vptr, local_dns_lookup_avg) \
  local_dns_lookup_avg = (DnsLookupStatsAvgTime*)((char*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp)) + dns_lookup_stats_avgtime_idx); 

#define INCREMENT_GRP_BASED_DNS_LOOKUP_CUM_SAMPLE_COUNTER(vptr) \
  if(SHOW_GRP_DATA) { \
    DnsLookupStatsAvgTime *local_dns_lookup_avg = NULL; \
    GET_DNS_LOOKUP_AVG(vptr, local_dns_lookup_avg) \
    local_dns_lookup_avg->dns_lookup_per_sec++; \
    local_dns_lookup_avg->total_dns_lookup_req++; \
  }

#define INCREMENT_GRP_BASED_DNS_LOOKUP_FAILURE_COUNTER(vptr) \
  if(SHOW_GRP_DATA) { \
    DnsLookupStatsAvgTime *local_dns_lookup_avg = NULL; \
    GET_DNS_LOOKUP_AVG(vptr, local_dns_lookup_avg) \
    local_dns_lookup_avg->dns_failure_per_sec++; \
  }

#define INCREMENT_GRP_BASED_DNS_LOOKUP_FROM_CACHE_COUNTER(vptr) \
  if(SHOW_GRP_DATA) { \
    DnsLookupStatsAvgTime *local_dns_lookup_avg = NULL; \
    GET_DNS_LOOKUP_AVG(vptr, local_dns_lookup_avg) \
    local_dns_lookup_avg->dns_from_cache_per_sec++; \
  }

#define UPDATE_GRP_BASED_DNS_LOOKUP_TIME_COUNTERS(vptr, diff_time) \
  if(SHOW_GRP_DATA) { \
    DnsLookupStatsAvgTime *local_dns_lookup_avg = NULL; \
    GET_DNS_LOOKUP_AVG(vptr, local_dns_lookup_avg) \
    if(diff_time < local_dns_lookup_avg->dns_lookup_time_min)\
    {                                                        \
      local_dns_lookup_avg->dns_lookup_time_min = diff_time; \
    }                                                        \
    if(diff_time > local_dns_lookup_avg->dns_lookup_time_max)\
    {                                                        \
      local_dns_lookup_avg->dns_lookup_time_max = diff_time; \
    }                                                        \
    local_dns_lookup_avg->dns_lookup_time_total += diff_time;\
  }

#define INC_POP3_RX_BYTES(vptr, bytes_handled) \
  average_time->pop3_rx_bytes += bytes_handled; \
  if(SHOW_GRP_DATA) { \
    avgtime *lol_average_time;\
    lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));\
    lol_average_time->pop3_rx_bytes += bytes_handled; \
  }  

#define INC_FTP_RX_BYTES(vptr, bytes_handled) \
  ftp_avgtime->ftp_rx_bytes += bytes_handled; \
  ftp_avgtime->ftp_total_bytes += bytes_handled; \
  if(SHOW_GRP_DATA) { \
    FTPAvgTime *local_ftp_avg = NULL; \
    GET_FTP_AVG(vptr, local_ftp_avg); \
    local_ftp_avg->ftp_rx_bytes += bytes_handled; \
    local_ftp_avg->ftp_total_bytes += bytes_handled; \
  }  

#define INC_IMAP_RX_BYTES(vptr, bytes_handled) \
  if(SHOW_GRP_DATA) { \
    IMAPAvgTime *local_imap_avg = NULL; \
    GET_IMAP_AVG(vptr, local_imap_avg); \
    local_imap_avg->imap_rx_bytes += bytes_handled; \
    local_imap_avg->imap_total_bytes += bytes_handled; \
  }

/*bug 70480 : MACRO added INC_HTTP2_SERVER_PUSH_COUNTER*/
#define INC_HTTP2_SERVER_PUSH_COUNTER(vptr) \
  average_time->num_srv_push++;			\
  if(SHOW_GRP_DATA) { \
   avgtime *lol_average_time;\
    lol_average_time = (avgtime*)((char *)average_time + ((vptr->group_num + 1) * g_avg_size_only_grp));\
    lol_average_time->num_srv_push++; \
  }

#define INC_CACHE_COUNTERS(vptr) \
  cache_avgtime->cache_num_tries++; \
  if(SHOW_GRP_DATA) { \
    CacheAvgTime *local_cache_avg = NULL; \
    GET_CACHE_AVG(vptr, local_cache_avg); \
    local_cache_avg->cache_num_tries++; \
  }

#define INC_CACHE_NUM_HITS_COUNTERS(vptr) \
  cache_avgtime->cache_num_hits++; \
  if(SHOW_GRP_DATA) { \
    CacheAvgTime *local_cache_avg = NULL; \
    GET_CACHE_AVG(vptr, local_cache_avg); \
    local_cache_avg->cache_num_hits++; \
  }

#define INC_CACHE_NUM_MISSED_COUNTERS(vptr) \
  cache_avgtime->cache_num_missed++; \
  if(SHOW_GRP_DATA) { \
    CacheAvgTime *local_cache_avg = NULL; \
    GET_CACHE_AVG(vptr, local_cache_avg); \
    local_cache_avg->cache_num_missed++; \
  }

#define INC_CACHE_BYTES_USED_COUNTERS(vptr, resp_len) \
  if(SHOW_GRP_DATA) { \
    CacheAvgTime *local_cache_avg = NULL; \
    GET_CACHE_AVG(vptr, local_cache_avg); \
    local_cache_avg->cache_bytes_used += resp_len; \
  }

#define DEC_CACHE_BYTES_USED_COUNTERS(vptr, resp_len) \
  if(SHOW_GRP_DATA) { \
    CacheAvgTime *local_cache_avg = NULL; \
    GET_CACHE_AVG(vptr, local_cache_avg); \
    local_cache_avg->cache_bytes_used -= resp_len; \
  }

#define SET_CACHE_BYTES_HIT_COUNTERS(vptr, resp_len) \
  if(SHOW_GRP_DATA) { \
    CacheAvgTime *local_cache_avg = NULL; \
    GET_CACHE_AVG(vptr, local_cache_avg); \
    local_cache_avg->cache_bytes_hit += resp_len; \
  }

#define DEC_CACHE_NUM_ENTERIES_COUNTERS(vptr) \
  cache_avgtime->cache_num_entries--; \
  if(SHOW_GRP_DATA) { \
    CacheAvgTime *local_cache_avg = NULL; \
    GET_CACHE_AVG(vptr, local_cache_avg); \
    local_cache_avg->cache_num_entries--; \
  }

#define INC_CACHE_NUM_ENTERIES_COUNTERS(vptr) \
  cache_avgtime->cache_num_entries++; \
  if(SHOW_GRP_DATA) { \
    CacheAvgTime *local_cache_avg = NULL; \
    GET_CACHE_AVG(vptr, local_cache_avg); \
    local_cache_avg->cache_num_entries++; \
  }

#define INC_CACHE_NUM_ENTERIES_REPLACED_COUNTERS(vptr) \
  cache_avgtime->cache_num_entries_replaced++; \
  if(SHOW_GRP_DATA) { \
    CacheAvgTime *local_cache_avg = NULL; \
    GET_CACHE_AVG(vptr, local_cache_avg); \
    local_cache_avg->cache_num_entries_replaced++; \
  }

#define INC_CACHE_NUM_ENTERIES_REVALIDATION_COUNTERS(vptr) \
  cache_avgtime->cache_num_entries_revalidation++; \
  if(SHOW_GRP_DATA) { \
    CacheAvgTime *local_cache_avg = NULL; \
    GET_CACHE_AVG(vptr, local_cache_avg); \
    local_cache_avg->cache_num_entries_revalidation++; \
  }

#define INC_CACHE_NUM_ENTERIES_REVALIDATION_IMS_COUNTERS(vptr) \
  cache_avgtime->cache_num_entries_revalidation_ims++; \
  if(SHOW_GRP_DATA) { \
    CacheAvgTime *local_cache_avg = NULL; \
    GET_CACHE_AVG(vptr, local_cache_avg); \
    local_cache_avg->cache_num_entries_revalidation_ims++; \
  }

#define INC_CACHE_NUM_ENTERIES_REVALIDATION_ETAG_COUNTERS(vptr) \
  cache_avgtime->cache_num_entries_revalidation_etag++; \
  if(SHOW_GRP_DATA) { \
    CacheAvgTime *local_cache_avg = NULL; \
    GET_CACHE_AVG(vptr, local_cache_avg); \
    local_cache_avg->cache_num_entries_revalidation_etag++; \
  }

#define INC_CACHE_REVALIDATION_NOT_MODIFIED_COUNTERS(vptr) \
  cache_avgtime->cache_revalidation_not_modified++; \
  if(SHOW_GRP_DATA) { \
    CacheAvgTime *local_cache_avg = NULL; \
    GET_CACHE_AVG(vptr, local_cache_avg); \
    local_cache_avg->cache_revalidation_not_modified++; \
  }

#define INC_CACHE_REVALIDATION_SUCC_COUNTERS(vptr) \
  cache_avgtime->cache_revalidation_success++; \
  if(SHOW_GRP_DATA) { \
    CacheAvgTime *local_cache_avg = NULL; \
    GET_CACHE_AVG(vptr, local_cache_avg); \
    local_cache_avg->cache_revalidation_success++; \
  }

#define INC_CACHE_NUM_ENTERIES_CACHEABLE_COUNTERS(vptr) \
  cache_avgtime->cache_num_entries_cacheable++; \
  if(SHOW_GRP_DATA) { \
    CacheAvgTime *local_cache_avg = NULL; \
    GET_CACHE_AVG(vptr, local_cache_avg); \
    local_cache_avg->cache_num_entries_cacheable++; \
  }

#define INC_CACHE_NUM_ENTERIES_NON_CACHEABLE_COUNTERS(vptr) \
  cache_avgtime->cache_num_entries_non_cacheable++; \
  if(SHOW_GRP_DATA) { \
    CacheAvgTime *local_cache_avg = NULL; \
    GET_CACHE_AVG(vptr, local_cache_avg); \
    local_cache_avg->cache_num_entries_non_cacheable++; \
  }

#define INC_CACHE_NUM_ENTERIES_COLLISION_COUNTERS(vptr) \
  cache_avgtime->cache_num_entries_collisions++; \
  if(SHOW_GRP_DATA) { \
    CacheAvgTime *local_cache_avg = NULL; \
    GET_CACHE_AVG(vptr, local_cache_avg); \
    local_cache_avg->cache_num_entries_collisions++; \
  }

#define INC_CACHE_NUM_ENTERIES_ERR_CREATION_COUNTERS(vptr) \
  cache_avgtime->cache_num_error_entries_creations++; \
  if(SHOW_GRP_DATA) { \
    CacheAvgTime *local_cache_avg = NULL; \
    GET_CACHE_AVG(vptr, local_cache_avg); \
    local_cache_avg->cache_num_error_entries_creations++; \
  }

#define INC_CACHE_SEARCH_URL_TIME_TOT_COUNTERS(vptr, lol_search_url_time) \
  cache_avgtime->cache_search_url_time_total += lol_search_url_time; \
  if(SHOW_GRP_DATA) { \
    CacheAvgTime *local_cache_avg = NULL; \
    GET_CACHE_AVG(vptr, local_cache_avg); \
    local_cache_avg->cache_search_url_time_total += lol_search_url_time; \
  }

#define INC_CACHE_ADD_URL_TIME_TOT_COUNTERS(vptr, lol_add_url_time) \
  cache_avgtime->cache_add_url_time_total += lol_add_url_time; \
  if(SHOW_GRP_DATA) { \
    CacheAvgTime *local_cache_avg = NULL; \
    GET_CACHE_AVG(vptr, local_cache_avg); \
    local_cache_avg->cache_add_url_time_total += lol_add_url_time; \
  }

#define INC_HTTP_PROXY_INSPECTED_REQ(vptr) \
  proxy_avgtime->http_proxy_inspected_requests++; \
  if(SHOW_GRP_DATA) { \
    ProxyAvgTime *local_proxy_avg = NULL; \
    GET_PROXY_AVG(vptr, local_proxy_avg); \
    local_proxy_avg->http_proxy_inspected_requests++; \
  }

#define INC_HTTP_PROXY_EXCP_REQ(vptr) \
  proxy_avgtime->http_proxy_excp_requests++; \
  if(SHOW_GRP_DATA) { \
    ProxyAvgTime *local_proxy_avg = NULL; \
    GET_PROXY_AVG(vptr, local_proxy_avg); \
    local_proxy_avg->http_proxy_excp_requests++; \
  }

#define INC_HTTP_PROXY_REQ(vptr) \
  proxy_avgtime->http_proxy_requests++; \
  if(SHOW_GRP_DATA) { \
    ProxyAvgTime *local_proxy_avg = NULL; \
    GET_PROXY_AVG(vptr, local_proxy_avg); \
    local_proxy_avg->http_proxy_requests++; \
  }

#define INC_TOT_HTTP_PROXY_REQ(vptr) \
  proxy_avgtime->tot_http_proxy_requests++; \
  if(SHOW_GRP_DATA) { \
    ProxyAvgTime *local_proxy_avg = NULL; \
    GET_PROXY_AVG(vptr, local_proxy_avg); \
    local_proxy_avg->tot_http_proxy_requests++; \
  }

#define INC_HTTPS_PROXY_INSPECTED_REQ(vptr) \
  proxy_avgtime->https_proxy_inspected_requests++; \
  if(SHOW_GRP_DATA) { \
    ProxyAvgTime *local_proxy_avg = NULL; \
    GET_PROXY_AVG(vptr, local_proxy_avg); \
    local_proxy_avg->https_proxy_inspected_requests++; \
  }

#define INC_HTTPS_PROXY_EXCP_REQ(vptr) \
  proxy_avgtime->https_proxy_excp_requests++; \
  if(SHOW_GRP_DATA) { \
    ProxyAvgTime *local_proxy_avg = NULL; \
    GET_PROXY_AVG(vptr, local_proxy_avg); \
    local_proxy_avg->https_proxy_excp_requests++; \
  }

#define INC_HTTPS_PROXY_REQ(vptr) \
  proxy_avgtime->https_proxy_requests++; \
  if(SHOW_GRP_DATA) { \
    ProxyAvgTime *local_proxy_avg = NULL; \
    GET_PROXY_AVG(vptr, local_proxy_avg); \
    local_proxy_avg->https_proxy_requests++; \
  }

#define INC_TOT_HTTPS_PROXY_REQ(vptr) \
  proxy_avgtime->tot_https_proxy_requests++; \
  if(SHOW_GRP_DATA) { \
    ProxyAvgTime *local_proxy_avg = NULL; \
    GET_PROXY_AVG(vptr, local_proxy_avg); \
    local_proxy_avg->tot_https_proxy_requests++; \
  }

#define INC_CONNECT_SUCC_COUNTERS(vptr) \
  proxy_avgtime->connect_successful++; \
  if(SHOW_GRP_DATA) { \
    ProxyAvgTime *local_proxy_avg = NULL; \
    GET_PROXY_AVG(vptr, local_proxy_avg); \
    local_proxy_avg->connect_successful++; \
  }

#define DEC_CONNECT_FAILURE_COUNTERS(vptr) \
  proxy_avgtime->connect_failure++; \
  if(SHOW_GRP_DATA) { \
    ProxyAvgTime *local_proxy_avg = NULL; \
    GET_PROXY_AVG(vptr, local_proxy_avg); \
    local_proxy_avg->connect_failure++; \
  }

#define INC_CONNECT_FAIL_RESP_TIME_TOT_FOR_GRP(vptr) \
  if(SHOW_GRP_DATA) { \
    ProxyAvgTime *local_proxy_avg = NULL; \
    GET_PROXY_AVG(vptr, local_proxy_avg); \
    local_proxy_avg->connect_failure_response_time_total += vptr->httpData->proxy_con_resp_time->http_connect_failure; \
    SETTING_MIN_VALUE(vptr->httpData->proxy_con_resp_time->http_connect_failure, local_proxy_avg->connect_failure_response_time_min) \
    SETTING_MAX_VALUE(vptr->httpData->proxy_con_resp_time->http_connect_failure, local_proxy_avg->connect_failure_response_time_max) \
  }

#define INC_CONNECT_SUCC_RESP_TIME_TOT_FOR_GRP(vptr) \
  if(SHOW_GRP_DATA) { \
    ProxyAvgTime *local_proxy_avg = NULL; \
    GET_PROXY_AVG(vptr, local_proxy_avg); \
    local_proxy_avg->connect_success_response_time_total += vptr->httpData->proxy_con_resp_time->http_connect_success;   \
    SETTING_MIN_VALUE (vptr->httpData->proxy_con_resp_time->http_connect_success , local_proxy_avg->connect_success_response_time_min); \
    SETTING_MAX_VALUE (vptr->httpData->proxy_con_resp_time->http_connect_success, local_proxy_avg->connect_success_response_time_max);  \
  }

#define INC_CONNECT_1XX_COUNTERS(vptr) \
  proxy_avgtime->connect_1xx++; \
  if(SHOW_GRP_DATA) { \
    ProxyAvgTime *local_proxy_avg = NULL; \
    GET_PROXY_AVG(vptr, local_proxy_avg); \
    local_proxy_avg->connect_1xx++; \
  }

#define INC_CONNECT_2XX_COUNTERS(vptr) \
  proxy_avgtime->connect_2xx++; \
  if(SHOW_GRP_DATA) { \
    ProxyAvgTime *local_proxy_avg = NULL; \
    GET_PROXY_AVG(vptr, local_proxy_avg); \
    local_proxy_avg->connect_2xx++; \
  }

#define INC_CONNECT_3XX_COUNTERS(vptr) \
  proxy_avgtime->connect_3xx++; \
  if(SHOW_GRP_DATA) { \
    ProxyAvgTime *local_proxy_avg = NULL; \
    GET_PROXY_AVG(vptr, local_proxy_avg); \
    local_proxy_avg->connect_3xx++; \
  }

#define INC_CONNECT_4XX_COUNTERS(vptr) \
  proxy_avgtime->connect_4xx++; \
  if(SHOW_GRP_DATA) { \
    ProxyAvgTime *local_proxy_avg = NULL; \
    GET_PROXY_AVG(vptr, local_proxy_avg); \
    local_proxy_avg->connect_4xx++; \
  }

#define INC_CONNECT_5XX_COUNTERS(vptr) \
  proxy_avgtime->connect_5xx++; \
  if(SHOW_GRP_DATA) { \
    ProxyAvgTime *local_proxy_avg = NULL; \
    GET_PROXY_AVG(vptr, local_proxy_avg); \
    local_proxy_avg->connect_5xx++; \
  }

#define INC_CONNECT_CONFAIL_COUNTERS(vptr) \
  proxy_avgtime->connect_confail++; \
  if(SHOW_GRP_DATA) { \
    ProxyAvgTime *local_proxy_avg = NULL; \
    GET_PROXY_AVG(vptr, local_proxy_avg); \
    local_proxy_avg->connect_confail++; \
  }

#define INC_CONNECT_TO_COUNTERS(vptr) \
  proxy_avgtime->connect_TO++; \
  if(SHOW_GRP_DATA) { \
    ProxyAvgTime *local_proxy_avg = NULL; \
    GET_PROXY_AVG(vptr, local_proxy_avg); \
    local_proxy_avg->connect_TO++; \
  }

#define INC_CONNECT_OTHERS_COUNTERS(vptr) \
  proxy_avgtime->connect_others++; \
  if(SHOW_GRP_DATA) { \
    ProxyAvgTime *local_proxy_avg = NULL; \
    GET_PROXY_AVG(vptr, local_proxy_avg); \
    local_proxy_avg->connect_others++; \
  }

#define INC_PROXY_AUTH_SUCC_COUNTERS(vptr) \
  proxy_avgtime->proxy_auth_success++; \
  if(SHOW_GRP_DATA) { \
    ProxyAvgTime *local_proxy_avg = NULL; \
    GET_PROXY_AVG(vptr, local_proxy_avg); \
    local_proxy_avg->proxy_auth_success++; \
  }

#define INC_PROXY_AUTH_FAILURE_COUNTERS(vptr) \
  proxy_avgtime->proxy_auth_failure++; \
  if(SHOW_GRP_DATA) { \
    ProxyAvgTime *local_proxy_avg = NULL; \
    GET_PROXY_AVG(vptr, local_proxy_avg); \
    local_proxy_avg->proxy_auth_failure++; \
  }

#define INC_DOS_SYNC_ATTACK_NUM_SUCC_COUNTERS(vptr) \
  dos_attack_avgtime->dos_syn_attacks_num_succ++; \
  if(SHOW_GRP_DATA) { \
    DosAttackAvgTime *local_dos_attack_avg = NULL; \
    GET_DOS_ATTACK_AVG(vptr, local_dos_attack_avg); \
    local_dos_attack_avg->dos_syn_attacks_num_succ++; \
  }

#define INC_DOS_SYNC_ATTACK_NUM_ERR_COUNTERS(vptr) \
  dos_attack_avgtime->dos_syn_attacks_num_err++; \
  if(SHOW_GRP_DATA) { \
    DosAttackAvgTime *local_dos_attack_avg = NULL; \
    GET_DOS_ATTACK_AVG(vptr, local_dos_attack_avg); \
    local_dos_attack_avg->dos_syn_attacks_num_err++; \
  }

//Set periodic elements of struct a with b into a
#define SET_MIN_MAX_GROUP_DATA_PERIODICS(a, b)\
  for (i = 0; i <total_runprof_entries ; i++) {\
    NSDL1_GDF(NULL, NULL, "a[%d].sess_min_time = %ld, b[%d].sess_min_time = %ld", i, a[i].sess_min_time, i, b[i].sess_min_time);\
    SET_MIN (a[i].sess_min_time, b[i].sess_min_time);\
    SET_MAX (a[i].sess_max_time, b[i].sess_max_time);\
    NSDL1_GDF(NULL, NULL, "a[%d].ka_min_time = %ld, b[%d].ka_min_time = %ld", i, a[i].ka_min_time, i, b[i].ka_min_time);\
    SET_MIN (a[i].ka_min_time, b[i].ka_min_time);\
    SET_MAX (a[i].ka_max_time, b[i].ka_max_time);\
    SET_MIN (a[i].page_think_min_time, b[i].page_think_min_time);\
    SET_MAX (a[i].page_think_max_time, b[i].page_think_max_time);\
  }

#define ACC_GROUP_DATA_PERIODICS(a, b)\
  for (i = 0; i <total_runprof_entries ; i++) {\
    a[i].time_to_wait += b[i].time_to_wait;\
    a[i].session_pacing_counts += b[i].session_pacing_counts;\
    a[i].ka_time += b[i].ka_time;\
    a[i].ka_counts += b[i].ka_counts;\
    a[i].page_think_time += b[i].page_think_time;\
    a[i].page_think_counts += b[i].page_think_counts;\
  }
    
#define CHILD_RESET_GROUP_DATA_AVGTIME(a) \
  if(SHOW_GRP_DATA) { \
    for (i = 0; i <total_runprof_entries; i++) {\
      a[i].sess_min_time = MAX_VALUE_4B_U;\
      a[i].sess_max_time = 0;\
      a[i].session_pacing_counts = 0;\
      a[i].time_to_wait = 0;\
      a[i].ka_min_time = MAX_VALUE_4B_U;\
      a[i].ka_max_time = 0;\
      a[i].ka_counts = 0;\
      a[i].ka_time = 0;\
      a[i].page_think_min_time = MAX_VALUE_4B_U;\
      a[i].page_think_max_time = 0;\
      a[i].page_think_counts = 0;\
      a[i].page_think_time = 0;\
    }\
  }

typedef struct
{
  //Vuser related counters
  int cur_vusers_running;
  int cur_vusers_active;
  int cur_vusers_thinking;
  int cur_vusers_waiting;
  int cur_vusers_idling;
  int cur_vusers_blocked;
  int cur_vusers_paused;
  int cur_sp_users; //Syncpoint users
  int cur_vusers_connection;
} GROUPVuser;

typedef struct
{
  //Session pacing related counters
  int session_pacing_counts;
  int time_to_wait;
  unsigned int sess_min_time;
  unsigned int sess_max_time;

  //Keep alive related counters
  int ka_counts;
  int ka_time;
  unsigned int ka_min_time;
  unsigned int ka_max_time;

  //Page think time related counters
  int page_think_counts;
  int page_think_time;
  unsigned int page_think_min_time;
  unsigned int page_think_max_time;
} GROUPAvgTime;

// Group Data Structure
typedef struct
{
  Times_data sess_time;  //structure for the session pacing time
  Times_data ka_time;    // structure for the keep alive time out time 
  Times_data page_think_time;    // structure for the keep alive time out time 
} Group_data_gp;

extern GROUPAvgTime *local_grp_avgtime;
extern GROUPVuser *grp_vuser;
extern Group_data_gp *group_data_gp_ptr;
extern int kw_set_group_based_data(char *buf, char *err_msg, int runtime_flag);
#ifndef CAV_MAIN
extern unsigned int group_data_gp_idx;
extern GROUPAvgTime *grp_avgtime;
#else
extern __thread unsigned int group_data_gp_idx;
extern __thread GROUPAvgTime *grp_avgtime;
#endif
extern unsigned int group_data_idx;
extern char **printGroup();
extern char **init_2d(int no_of_host);
extern void fill_2d(char **TwoD, int i, char *fill_data);
extern inline void update_group_data_avgtime_size();
extern inline void set_group_data_avgtime_ptr();
extern void fill_vuser_group_gp(GROUPAvgTime *grp_avg);
extern void set_grp_based_counter_for_session_pacing(void *vptr, int time_to_think);
extern void set_grp_based_counter_for_keep_alive(void *vptr, int ka_time_out);
extern void set_grp_based_counter_for_page_think_time(void *vptr, int pg_think_time);
extern void fill_group_gp(avgtime **grp_avg);
#endif
