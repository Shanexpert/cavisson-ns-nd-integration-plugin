#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <math.h>
#include <sys/ioctl.h>
#include <assert.h>
#include "cavmodem.h"
#include <dlfcn.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <regex.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"
#include "nslb_time_stamp.h"
#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "user_tables.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "util.h"
#include "timing.h"
#include "tmr.h"
#include "ns_msg_com_util.h" 
#include "output.h"
#include "smon.h"
#include "init_cav.h"
#include "eth.h"
#include "ns_parse_src_ip.h"
#include "wait_forever.h"
#include "deliver_report.h"

#include "ns_gdf.h"
#include "ns_custom_monitor.h"

#include "server_stats.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "ns_ftp.h" 
#include "ns_gdf.h"
#include "ns_fill_gdf.c"
#include "ns_summary_rpt.h"
#include "ns_goal_based_sla.h"
#include "ns_user_monitor.h"
#include "ns_percentile.h"
#include "ns_global_dat.h"
#include "ns_event_log.h"
#include "dos_attack/ns_dos_attack_reporting.h"
#include "ns_alloc.h"
#include "ns_sync_point.h"
#include "ns_network_cache_reporting.h"
#include "ns_lps.h"
#include "ns_check_monitor.h"
#include "ns_monitoring.h"
#include "ns_dns_reporting.h"
#include "ns_license.h"
#include "ns_server_admin_utils.h"
#include "netomni/src/core/ni_user_distribution.h"
#include "netomni/src/core/ni_scenario_distribution.h"
#include "ns_parent.h"
#include "ns_ndc.h"
#include "nia_fa_function.h"
#include "ns_dynamic_vector_monitor.h"
#include "ns_ip_data.h"
#include "ns_group_data.h"
#include "ns_rbu_page_stat.h"
#include "ns_page_based_stats.h"
#include "ns_ldap.h"
#include "ns_imap.h"
#include "ns_jrmi.h"
#include "ns_child_msg_com.h"
#include "ns_trace_level.h"
#include "db_aggregator.h"
#include "ns_runtime_runlogic_progress.h"
#include "ns_ndc_outbound.h"
#include "ns_server_ip_data.h"
#include "ns_websocket_reporting.h"
#include "ns_runtime_changes_monitor.h"
#include "ns_exit.h"
#include "ns_rbu_domain_stat.h"
#include "ns_appliance_health_monitor.h"
#include "ns_jmeter.h"
#include "ns_xmpp.h"
#include "ns_http_status_codes.h"
#include "ns_websocket.h"
#include "ns_fc2.h"
#include "ns_runtime_changes.h"
#include "ns_data_handler_thread.h"
#include "ns_global_settings.h"
#include "nslb_sock.h"
#include "ns_error_msg.h"
#include "ns_monitor_profiles.h"
#include "ns_socket.h"
#include "ns_test_monitor.h"

extern void dvm_make_conn();

extern Long_data rtg_pkt_ts;
 
extern int create_report_table_entry(int *row_num);
extern void check_if_partition_switch_to_be_done(ClientData cd, u_ns_ts_t now);

extern int gui_data_seq; // Defined in wait_forever.c
extern Msg_com_con ndc_mccptr;
avgtime *tmp_reset_avg;
cavgtime *tmp_reset_cavg;

Long_data g_current_time_deliver_report;
// write ip data to controller
void send_ip_data_to_controller(int fd, avgtime *avg)
{
  static int first_time = 0;
  int i = 0, j = 0;
  User_trace my_msg;
  int length = 0;
  char grp_ip_buf[1024 + 1] = "";

  my_msg.opcode = IP_MONITOR_DATA;
  IPBasedAvgTime *ip_avg;
  ip_avg = (IPBasedAvgTime *) ((char*) avg + ip_data_gp_idx); 

  NSDL2_REPORTING(NULL, NULL, "Method called");
  NSDL2_REPORTING(NULL, NULL, "total_ip_entries = %d, ip_data_gp_idx = %d", total_ip_entries, ip_data_gp_idx);
  
  if (!first_time) {
    for(j = 0; j < total_runprof_entries; j++)
    {
       for(i = 0; i < runprof_table_shr_mem[j].gset.num_ip_entries; i++)
       {      
         //make buffer of grp>ip
         snprintf(grp_ip_buf, 1024, "%s>%s", runprof_table_shr_mem[j].scen_group_name, runprof_table_shr_mem[j].gset.ips[i].ip_str);
         NSDL2_REPORTING(NULL, NULL, "ip_avg[%d].cur_url_req = %d, global_settings->event_generating_host = %s grp_ip_buf = %s", 
                                      i, ip_avg[i].cur_url_req, global_settings->event_generating_host, grp_ip_buf);
         length += sprintf(my_msg.reply_msg + length, "%d:%d:%s>%s|%d\n", g_parent_idx, runprof_table_shr_mem[j].gset.ips[i].ip_id, 
                               global_settings->event_generating_host, grp_ip_buf, ip_avg[i].cur_url_req);
       }
    }
    first_time = 1;
  }
  else {
    for(j = 0; j < total_runprof_entries; j++)
    {
      for(i = 0; i < runprof_table_shr_mem[j].gset.num_ip_entries; i++)
      {      
        NSDL2_REPORTING(NULL, NULL, "ip_avg[%d].cur_url_req = %d", i, ip_avg[i].cur_url_req);
        length += sprintf(my_msg.reply_msg + length, "%d:%d|%d\n", 
                           g_parent_idx, runprof_table_shr_mem[j].gset.ips[i].ip_id, ip_avg[i].cur_url_req);
      }
    }

  }
  my_msg.reply_status = length;
  forward_dh_msg_to_master(fd, (parent_msg *)&my_msg, sizeof(User_trace));
}

// Write data to GUI for
void
write_gui_data()
{
  Msg_data_hdr *msg_data_hdr_local_ptr  = (Msg_data_hdr *) msg_data_ptr;
  double seq_no = (msg_data_hdr_local_ptr->seq_no);

  NSDL2_REPORTING(NULL, NULL, "Method called");

  if((gui_data_seq != 0) && (gui_data_seq != seq_no))
    return; 

  time_t ts = time(NULL);

  fprintf(gui_fp, "%s, opcode: %6.0f, seq_number: %6.0f testrun = %d\n", ctime(&ts), (msg_data_hdr_local_ptr->opcode), seq_no, testidx);

  log_vuser_gp();
  log_ssl_gp();
  log_url_hits_gp ();
  log_page_download_gp ();
  log_session_gp ();
  log_url_fail_gp ();
  log_page_fail_gp ();
  log_session_fail_gp ();
  log_all_trans_data();         // This function will print the logs of Trans_overall_gp, Trans_fail_gp, Trans_time_gp, Trans_completed_gp and Trans_success_gp

  log_custom_monitor_gp();
}

/* This function is called from deliver_report() and copy_progress_data in case of Netcloud
 * Here GDF data get fills in msg_data_ptr and in case of NC per generator rtgMessage.dat
 * is updated
 * gen_idx : -1 for standalone and controller
 *         :  greater than -1 for generator
 **/
void fill_rtg_graph_data(avgtime **g_avg, cavgtime **g_cavg, int gen_idx)
{
  NSDL2_REPORTING(NULL, NULL, "Method called. gen_idx = %d, g_avg = %p, g_cavg = %p", gen_idx, g_avg, g_cavg);
  fill_vuser_gp (g_avg);
  fill_ssl_gp (g_avg,g_cavg);
  fill_url_hits_gp (g_avg,g_cavg);
  fill_page_download_gp (g_avg,g_cavg);
  fill_session_gp (g_avg,g_cavg);
  fill_url_fail_gp (g_avg);
  fill_page_fail_gp (g_avg);
  fill_session_fail_gp (g_avg);

  fill_http_status_codes_gp(g_avg);

  fill_smtp_hits_gp (g_avg,g_cavg);
  fill_smtp_net_throughput_gp (g_avg,g_cavg);
  fill_smtp_fail_gp (g_avg);

  fill_pop3_hits_gp (g_avg,g_cavg);
  fill_pop3_net_throughput_gp (g_avg,g_cavg);
  fill_pop3_fail_gp (g_avg);

  // For FTP
  fill_ftp_gp (g_avg, g_cavg); 

  // For LDAP
  fill_ldap_gp (g_avg, g_cavg);
 
  // for group 
  if(group_data_gp_ptr) {
    fill_group_gp(g_avg);
  }

  //For RBU page stat
  if(rbu_page_stat_data_gp_ptr && (global_settings->browser_used != -1)) {
    fill_rbu_page_stat_gp(g_avg);
  }

  //NSDL2_REPORTING(NULL, NULL, "page_based_stat_avgtime = %p", page_based_stat_avgtime);
  //For Page Based Stat
  if(page_based_stat_gp_ptr) {
    fill_page_based_stat_gp(g_avg);
  }   
  // For IMAP
  fill_imap_gp (g_avg, g_cavg);
 
  // For JRMI
  fill_jrmi_gp (g_avg, g_cavg); 

  fill_dns_hits_gp (g_avg,g_cavg);
  fill_dns_net_throughput_gp (g_avg,g_cavg);
  fill_dns_fail_gp (g_avg);
       
  if(global_settings->g_enable_ns_diag) 
  {
    fill_ns_diag_gp(g_avg);
    fill_ns_diag_cum_gp(g_cavg);
  }
  if(global_settings->protocol_enabled & HTTP_CACHE_ENABLED) {
    fill_cache_gp (g_avg);
  }
  /*bug 70480 fill http2 server push graph pointer */
  if(IS_SERVER_PUSH_ENABLED) {
    fill_http2_srv_push_gp(g_cavg);
  }
  if(IS_PROXY_ENABLED) {
    fill_proxy_gp (g_avg);
  }
  if(IS_NETWORK_CACHE_STATS_ENABLED) { 
    fill_nw_cache_stats_gp (g_avg); 
  }
  if(IS_DNS_LOOKUP_STATS_ENABLED) {
    fill_dns_lookup_stats_gp (g_avg); 
  }
    
  if(global_settings->protocol_enabled & DOS_ATTACK_ENABLED) {
    fill_dos_attack_gp(g_avg); 
  }

  if(IS_WS_ENABLED) {
    fill_ws_stats_gp (g_avg, g_cavg);    //WebSocket Graph
    fill_ws_status_codes(g_avg);         //Response Codes
    fill_ws_failure_stats_gp(g_avg);     //Failure Stats  
  }

  FILL_TCP_CLIENT_AVG;
  FILL_UDP_CLIENT_AVG;

  if(global_settings->protocol_enabled & FC2_PROTOCOL_ENABLED)
    fill_fc2_gp(g_avg,g_cavg);

  fill_all_trans_data(g_avg, g_cavg); // It will fill all transtion group data (ns_fill_gdf.c)

  if(vuser_flow_gp_ptr) {
    fill_vuser_flow_gp(g_avg);
  }
  
  if(SHOW_SERVER_IP) {
    fill_srv_ip_data_gp(g_avg);
  }

  if(ip_data_gp_ptr){
    fill_ip_gp(g_avg);
  }

  /* Fill RBUDomainStats graph data */
  if(rbu_domain_stat_gp_ptr)
    fill_rbu_domain_stat_gp(g_avg);

  if(global_settings->protocol_enabled & XMPP_PROTOCOL_ENABLED){
    fill_xmpp_gp(g_avg, g_cavg);
  }
  
  NSDL2_REPORTING(NULL, NULL, "For generator index = %d", gen_idx);
  if (gen_idx != -1)
  {
    if(!(g_tsdb_configuration_flag & TSDB_MODE))
    {
      if (generator_entry[gen_idx].rtg_fp) {
        if (fwrite(msg_data_ptr, nc_msg_data_size, 1, generator_entry[gen_idx].rtg_fp) < 1)
          perror ("deliver_report(): Generator rtg file write failed");
          fflush(generator_entry[gen_idx].rtg_fp); // To force all buffered data to rtg file, without fflush partial data will write
      }
    }
  }
}

inline void send_data_to_secondary_gui_server()
{
  NSDL1_MISC(NULL, NULL, "Method called, gui_fd2 = %d", gui_fd2);

  if(gui_fd2 == -1)
    open_connect_to_gui_server(&gui_fd2, global_settings->secondary_gui_server_addr, global_settings->secondary_gui_server_port);
  
  //gui_fd will be set in open_connect_to_gui_server function
  if(gui_fd2 != -1)
  {
    /* Netstorm sends Message Header only to gui since gui only requires header, rest of the data it reads from
     * rtgMessage.dat file */
    if (send(gui_fd2, msg_data_ptr, sizeof(Msg_data_hdr), 0) != sizeof(Msg_data_hdr)) 
    {
      print_core_events((char*)__FUNCTION__, __FILE__,
                         "NetStorm unable to send data (Message Header) to GUI (%s:%hd) server."
                         " error = %s", global_settings->secondary_gui_server_addr, global_settings->secondary_gui_server_port, nslb_strerror(errno) );
      if(errno != ECONNREFUSED)
      {
        CLOSE_FD(gui_fd2);
        open_connect_to_gui_server(&gui_fd2, global_settings->secondary_gui_server_addr, global_settings->secondary_gui_server_port);
      }
    }
  }
}

int
deliver_report (int run_mode, int fd, avgtime **g_avg, cavgtime **g_cavg, FILE *rfp, FILE* srfp) 
{
  int rnum, i;
  int display_period = 0, display_cumul = 0;
  double url_data[5] = {0,0,0,0,0};
  double pg_data[5] = {0,0,0,0,0};
  double tx_data[5] = {0,0,0,0,0};
  double ss_data[5] = {0,0,0,0,0};
  double smtp_data[5] = {0,0,0,0,0};
  double pop3_data[5] = {0,0,0,0,0};
  double ftp_data[5] = {0,0,0,0,0};
  double dns_data[5] = {0,0,0,0,0};
  double ldap_data[5] = {0,0,0,0,0};
  double imap_data[5] = {0,0,0,0,0};
  double jrmi_data[5] = {0,0,0,0,0};
  double ws_data[5] = {0,0,0,0,0};
  char tmp_buf[1024];
  int ret;

  FTPAvgTime *ftp_avg = NULL;
  FTPCAvgTime *ftp_cavg = NULL;
  LDAPAvgTime *ldap_avg = NULL;
  LDAPCAvgTime *ldap_cavg = NULL;
  #ifdef NS_DEBUG_ON
  GROUPAvgTime *grp_avg = NULL;
  PageStatAvgTime *page_based_stat_avgtime = NULL;  
  RBUPageStatAvgTime *rbu_page_stat_avg = NULL;  
  XMPPAvgTime *xmpp_avg = NULL;
  XMPPCAvgTime *xmpp_cavg = NULL;
  #endif
  IMAPAvgTime *imap_avg = NULL;
  IMAPCAvgTime *imap_cavg = NULL;
  JRMIAvgTime *jrmi_avg = NULL;
  JRMICAvgTime *jrmi_cavg = NULL;
  WSAvgTime *ws_avg = NULL;
  WSCAvgTime *ws_cavg = NULL;


  avgtime *avg = NULL;
  cavgtime *c_avg = NULL;
  avgtime *tmp_avg = NULL;
  cavgtime *tmp_c_avg = NULL;
 
  if(g_enable_delete_vec_freq)// Increment the counter if vector delete frequency counter feature is enabled
  {
    g_delete_vec_freq_cntr++; 

    if(g_delete_vec_freq_cntr >= g_delete_vec_freq)
    {
      if(total_waiting_deleted_vectors) 
        g_vector_runtime_changes = 1;    
 
      g_delete_vec_freq_cntr = 0;
    }
  }

  NSTL2(NULL, NULL, "g_delete_vec_feq_cntr = %d", g_delete_vec_freq_cntr);

  Msg_data_hdr *msg_data_hdr_local_ptr  = (Msg_data_hdr *) msg_data_ptr;
  
  NSDL2_REPORTING(NULL, NULL, "Method called");

  //NSDL2_REPORTING(NULL, NULL, "Netstorm virtual users=%f (avg_users)", avg->avg_users);

  // Check if users exceeded licenses user limit or not
  /*Bug 11523: If license file is not found on generators still they should continue test
    In NC setup clients are using our machines as generators here license verification isnt require
    By pass flow*/
  if (loader_opcode != CLIENT_LOADER)
  {
    if((ret = ns_lic_chk_users_limit((unsigned int)g_avg[0]->running_users))){
      if(ret == -2){
        kill_all_children((char *)__FUNCTION__, __LINE__, __FILE__);
        NS_EXIT(1, ""); 
      }
    }
  }
  
  create_server_count_data_buf();

  if(monitor_runtime_changes_applied || g_monitor_runtime_changes || g_monitor_runtime_changes_NA_gdf || g_vector_runtime_changes || is_rtc_applied_for_dyn_objs())
  {
    NSTL1(NULL, NULL, "Starting monitors added during runtime for data connection.");
    runtime_change_mon_dr();
    monitor_runtime_changes_applied = 0;
    g_vector_runtime_changes = 0;
    g_monitor_runtime_changes = 0;
    g_monitor_runtime_changes_NA_gdf = 0;
    reset_dynamic_obj_structure();
    monitor_runtime_changes_applied = 0;
  }

  if(!is_outbound_connection_enabled)
  {
    //This function is called to send testrun running message to all server at specified intervals. 
    send_testrun_running_msg_to_all_cavmonserver_used(g_avg[0]->elapsed);
  
    //send heart beat on monitors data connection
    if(g_enable_mon_data_conn_hb) 
    {
      if(g_last_hb_send >= g_count_to_send_hb)
      {
        send_hb_on_data_conn();
        g_last_hb_send = 1;
      }
      else
        g_last_hb_send++;
    }
  }

  avg = (avgtime*)g_avg[0];
  c_avg = (cavgtime*)g_cavg[0];

  #ifdef NS_DEBUG_ON
  // for RBU Page Stat if G_RBU is enabled : we are using "browser_used" because whenever we use RBU we will have browser_used(0,1,2)
  if(global_settings->browser_used != -1) {
    rbu_page_stat_avg = (RBUPageStatAvgTime *) ((char*) avg + rbu_page_stat_data_gp_idx);
    NSDL2_REPORTING(NULL, NULL, "rbu_page_stat_avg = %p and rbu_page_stat_data_gp_idx = %d for data connection", rbu_page_stat_avg, rbu_page_stat_data_gp_idx);
  }  
 
  // for Page based Stat if ENABLE_PAGE_BASED_STAT is enabled 
  NSDL2_REPORTING(NULL, NULL, "Delivery Reoprt : page_based_stat = %d, avg = %p for data connection", global_settings->page_based_stat, avg);
  if(global_settings->page_based_stat == PAGE_BASED_STAT_ENABLED) {
    page_based_stat_avgtime = (PageStatAvgTime *) ((char*) avg + page_based_stat_gp_idx);
    NSDL2_REPORTING(NULL, NULL, "page_based_stat_avgtime = %p and page_based_stat_gp_idx = %d for data connection", page_based_stat_avgtime, page_based_stat_gp_idx);
  }

  // for Group keyword is enabled 
  if(SHOW_GRP_DATA) {
    grp_avg = (GROUPAvgTime *) ((char*) avg + group_data_gp_idx);
    NSDL2_REPORTING(NULL, NULL, "grp_avg = %p for data connection", grp_avg);
  }  
  #endif

  tmp_avg = avg;
  tmp_c_avg = c_avg;
  for(i = 0; i < TOTAL_GRP_ENTERIES_WITH_GRP_KW; i++)
  {
    avg = (avgtime*)((char*)tmp_avg + (i * g_avg_size_only_grp));
    c_avg = (cavgtime*)((char*)tmp_c_avg + (i * g_cavg_size_only_grp));

    // For FTP Protocol if enabled
    if(global_settings->protocol_enabled & FTP_PROTOCOL_ENABLED) {
      ftp_avg = (FTPAvgTime *) ((char*) avg + g_ftp_avgtime_idx);
      ftp_cavg = (FTPCAvgTime *) ((char*) c_avg + g_ftp_cavgtime_idx);
    }
    // LDAP
    if(global_settings->protocol_enabled & LDAP_PROTOCOL_ENABLED) {
      ldap_avg = (LDAPAvgTime *) ((char*) avg + g_ldap_avgtime_idx);
      ldap_cavg = (LDAPCAvgTime *) ((char*) c_avg + g_ldap_cavgtime_idx);
      NSDL2_REPORTING(NULL, NULL, "pointing ldap_avg index = [%d] ldap_avg = [%p] for data connection", g_ldap_avgtime_idx, ldap_avg);
    }
    // IMAP
    if(global_settings->protocol_enabled & IMAP_PROTOCOL_ENABLED) {
      imap_avg = (IMAPAvgTime *) ((char*) avg + g_imap_avgtime_idx);
      imap_cavg = (IMAPCAvgTime *) ((char*) c_avg + g_imap_cavgtime_idx);
      NSDL2_REPORTING(NULL, NULL, "pointing imap_avg index = [%d] imap_avg = [%p] for data connection", g_imap_avgtime_idx, imap_avg);
    }
    // JRMI
    if(global_settings->protocol_enabled & JRMI_PROTOCOL_ENABLED) {
      jrmi_avg = (JRMIAvgTime *) ((char*) avg + g_jrmi_avgtime_idx);
      jrmi_cavg = (JRMICAvgTime *) ((char*) c_avg + g_jrmi_cavgtime_idx);
      NSDL2_REPORTING(NULL, NULL, "pointing jrmi_avg index = [%d] jrmi_avg = [%p] for data connection", g_jrmi_avgtime_idx, jrmi_avg);
    }
    
    //WS
    if(IS_WS_ENABLED) {
      NSDL2_WS(NULL, NULL, "g_ws_avgtime_idx = %d for data connection", g_ws_avgtime_idx);
      ws_avg = (WSAvgTime *) ((char*) avg + g_ws_avgtime_idx);
      ws_cavg = (WSCAvgTime *) ((char*) c_avg + g_ws_cavgtime_idx);
      NSDL2_REPORTING(NULL, NULL, "pointing ws_avg index = [%d] ws_avg = [%p], ws_cavg = [%p] for data connection", g_ws_avgtime_idx, ws_avg, ws_cavg);
    }    

    #ifdef NS_DEBUG_ON
    // XMPP
    if(global_settings->protocol_enabled & XMPP_PROTOCOL_ENABLED) {
      xmpp_avg = (XMPPAvgTime *) ((char*) avg + g_xmpp_avgtime_idx);
      xmpp_cavg = (XMPPCAvgTime *) ((char*) c_avg + g_xmpp_cavgtime_idx);
      NSDL2_REPORTING(NULL, NULL, "pointing xmpp_avg index = [%d] xmpp_avg = [%p], xmpp_cavg = [%p] for data connection", g_xmpp_avgtime_idx, xmpp_avg, xmpp_cavg);
    }
    #endif

 
    //update_eth_data();
 
    NSDL2_REPORTING(NULL, NULL, "avg->opcode = %d for data connection", avg->opcode);
    if  (avg->opcode == PROGRESS_REPORT) {

    #if 0
      if ((loader_opcode != MASTER_LOADER) && (!g_collect_no_eth_data)) {
              avg->eth_rx_bps = get_eth_rx_bps(0, global_settings->progress_secs);
              avg->c_eth_rx_bytes = get_eth_rx_bytes();
 
              avg->eth_tx_bps = get_eth_tx_bps(0, global_settings->progress_secs);
              avg->c_eth_tx_bytes = get_eth_tx_bytes();
 
              avg->eth_rx_pps = get_eth_rx_pps(0, global_settings->progress_secs);
              avg->c_eth_rx_packets = get_eth_rx_packets();
 
              avg->eth_tx_pps = get_eth_tx_pps(0, global_settings->progress_secs);
              avg->c_eth_tx_packets = get_eth_tx_packets();
      }
    #endif
      avg->url_overall_avg_time = avg->url_overall_tot_time/(avg->num_tries?avg->num_tries:1);
      //avg->avg_time = avg->tot_time/(avg->num_hits?avg->num_hits:1);
      c_avg->c_avg_time = c_avg->c_tot_time/(c_avg->url_succ_fetches?c_avg->url_succ_fetches:1);

      NSDL2_REPORTING(NULL, NULL, "avg->url_overall_avg_time = %d, avg->url_overall_tot_time = %d, avg->num_tries = %d for data connection", 
                                   avg->url_overall_avg_time, avg->url_overall_tot_time, avg->num_tries);
 
      /* for SMTP */
      avg->smtp_avg_time = avg->smtp_tot_time/(avg->smtp_num_hits?avg->smtp_num_hits:1);
      c_avg->smtp_c_avg_time = c_avg->smtp_c_tot_time/(c_avg->smtp_succ_fetches?c_avg->smtp_succ_fetches:1);
 
      /* for pop3 */
      avg->pop3_overall_avg_time = avg->pop3_overall_tot_time/(avg->pop3_num_tries?avg->pop3_num_tries:1);
      c_avg->pop3_c_avg_time = c_avg->pop3_c_tot_time/(c_avg->pop3_succ_fetches?c_avg->pop3_succ_fetches:1);
      //For Success
      avg->pop3_avg_time = avg->pop3_tot_time/(avg->pop3_num_hits?avg->pop3_num_hits:1);
      //For Failure
      avg->pop3_failure_avg_time = avg->pop3_failure_tot_time/((avg->pop3_num_tries - avg->pop3_num_hits)?
                                                             (avg->pop3_num_tries - avg->pop3_num_hits):1);
 
      /* for FTP */
      if(ftp_avg) {
        ftp_avg->ftp_overall_avg_time = ftp_avg->ftp_overall_tot_time/(ftp_avg->ftp_num_tries?ftp_avg->ftp_num_tries:1);
        ftp_cavg->ftp_c_avg_time = ftp_cavg->ftp_c_tot_time/(ftp_cavg->ftp_succ_fetches?ftp_cavg->ftp_succ_fetches:1);
        //For Success
        ftp_avg->ftp_avg_time = ftp_avg->ftp_tot_time/(ftp_avg->ftp_num_hits?ftp_avg->ftp_num_hits:1);
        //For Failure 
        ftp_avg->ftp_failure_avg_time = ftp_avg->ftp_failure_tot_time/((ftp_avg->ftp_num_tries - ftp_avg->ftp_num_hits) ? 
                                                                       (ftp_avg->ftp_num_tries - ftp_avg->ftp_num_hits):1);
      }
 
      // LDAP
      if(ldap_avg) {
        ldap_avg->ldap_avg_time = ldap_avg->ldap_tot_time/(ldap_avg->ldap_num_hits?ldap_avg->ldap_num_hits:1);
        ldap_cavg->ldap_c_avg_time = ldap_cavg->ldap_c_tot_time/(ldap_cavg->ldap_succ_fetches?ldap_cavg->ldap_succ_fetches:1);
      }
 
      // IMAP
      if(imap_avg) {
        imap_avg->imap_overall_avg_time = imap_avg->imap_overall_tot_time/(imap_avg->imap_num_tries?imap_avg->imap_num_tries:1);
        imap_cavg->imap_c_avg_time = imap_cavg->imap_c_tot_time/(imap_cavg->imap_succ_fetches?imap_cavg->imap_succ_fetches:1);

        //For Success
        imap_avg->imap_avg_time = imap_avg->imap_tot_time/(imap_avg->imap_num_hits?imap_avg->imap_num_hits:1);
        //For Failure 
        imap_avg->imap_failure_avg_time = imap_avg->imap_failure_tot_time/((imap_avg->imap_num_tries - imap_avg->imap_num_hits) ? 
                                                                       (imap_avg->imap_num_tries - imap_avg->imap_num_hits):1);
      }
      // JRMI
      if(jrmi_avg) {
        jrmi_avg->jrmi_avg_time = jrmi_avg->jrmi_tot_time/(jrmi_avg->jrmi_num_hits?jrmi_avg->jrmi_num_hits:1);
        jrmi_cavg->jrmi_c_avg_time = jrmi_cavg->jrmi_c_tot_time/(jrmi_cavg->jrmi_succ_fetches?jrmi_cavg->jrmi_succ_fetches:1);
      }
 
      /* for DNS */
      //For Overall
      avg->dns_overall_avg_time = avg->dns_overall_tot_time/(avg->dns_num_tries?avg->dns_num_tries:1);
      c_avg->dns_c_avg_time = c_avg->dns_c_tot_time/(c_avg->dns_succ_fetches?c_avg->dns_succ_fetches:1);
      //For Success
      avg->dns_avg_time = avg->dns_tot_time/(avg->dns_num_hits?avg->dns_num_hits:1);
      //For Failure
      avg->dns_failure_avg_time = avg->dns_failure_tot_time/((avg->dns_num_tries - avg->dns_num_hits)?
                                                             (avg->dns_num_tries - avg->dns_num_hits):1);
      //WS
      NSDL2_WS(NULL, NULL, "ws_avg = %p for data connection", ws_avg);
      if(ws_avg) {
        ws_avg->ws_avg_time = ws_avg->ws_tot_time/(ws_avg->ws_num_hits?ws_avg->ws_num_hits:1);
        ws_cavg->ws_c_avg_time = ws_cavg->ws_c_tot_time/(ws_cavg->ws_succ_fetches?ws_cavg->ws_succ_fetches:1);
        NSDL2_WS(NULL, NULL, "ws_avg_time = [%d], ws_c_avg_time = [%d] for data connection", ws_avg->ws_avg_time, ws_cavg->ws_c_avg_time);
      }

#ifdef NS_TIME
      //For Overall Response time, we will need to calculate for completed
      avg->pg_avg_time = avg->pg_tot_time/(avg->pg_tries?avg->pg_tries:1);
      avg->tx_avg_time = avg->tx_tot_time/(avg->tx_fetches_completed?avg->tx_fetches_completed:1);
      avg->sess_avg_time = avg->sess_tot_time/(avg->sess_tries?avg->sess_tries:1);
      c_avg->pg_c_avg_time = c_avg->pg_c_tot_time/(c_avg->pg_fetches_completed?c_avg->pg_fetches_completed:1);
      c_avg->tx_c_avg_time = c_avg->tx_c_tot_time/(c_avg->tx_c_fetches_completed?c_avg->tx_c_fetches_completed:1);
      c_avg->sess_c_avg_time = c_avg->sess_c_tot_time/(c_avg->sess_fetches_completed?c_avg->sess_fetches_completed:1);
     
      //For Successful Response time, we will need to calculate for success tx Not doing for cumulative stats
      avg->avg_time = avg->tot_time/(avg->num_hits?avg->num_hits:1);
      avg->pg_succ_avg_resp_time = avg->pg_succ_tot_resp_time/(avg->pg_hits?avg->pg_hits:1);
      avg->tx_succ_avg_resp_time = avg->tx_succ_tot_resp_time/(avg->tx_succ_fetches?avg->tx_succ_fetches:1);
      avg->sess_succ_avg_resp_time = avg->sess_succ_tot_resp_time/(avg->sess_hits?avg->sess_hits:1);
     
      //For failure Response time, we will need to calculate for failure tx
      avg->url_failure_avg_time = avg->url_failure_tot_time/((avg->num_tries - avg->num_hits)?(avg->num_tries - avg->num_hits):1);
      avg->pg_fail_avg_resp_time = avg->pg_fail_tot_resp_time/((avg->pg_tries - avg->pg_hits)?(avg->pg_tries - avg->pg_hits):1);
      avg->tx_fail_avg_resp_time = avg->tx_fail_tot_resp_time/((avg->tx_fetches_completed - avg->tx_succ_fetches)?(avg->tx_fetches_completed - avg->tx_succ_fetches):1);
      avg->sess_fail_avg_resp_time = avg->sess_fail_tot_resp_time/((avg->sess_tries - avg->sess_hits)?(avg->sess_tries - avg->sess_hits):1);
#endif
    }
  }

  avg = (avgtime*)g_avg[0];
  c_avg = g_cavg[0];
  if(avg->opcode == PROGRESS_REPORT) 
  {
    check_system_capacity(avg); //ns_goal_based_sla.c
 
    /* this process is a CLIENT */
    if (fd >= 0) {
      /* send ip data to controller */ 
      NSDL2_REPORTING(NULL, NULL, "global_settings->show_ip_data = %d, total_ip_entries = %d for data connection", 
                                   global_settings->show_ip_data,total_ip_entries);
      if ((global_settings->show_ip_data == IP_BASED_DATA_ENABLED) && (loader_opcode == CLIENT_LOADER) && total_ip_entries) {
        send_ip_data_to_controller(fd, avg);
      }
 
      /*Fill Parent Stats in case of Generator*/
      NSDL2_REPORTING(NULL, NULL, "NetStormDiagnostics Stats - Filling parent data to avg.");
      FILL_PARENT_NESTORM_DIAGNOSTICS_STATS(avg);
      /* send total_avg to MASTER */
      //printf("sending progress report to coordinator...\n");
      NSDL2_REPORTING(NULL, NULL, "g_avgtime_size = %d, ip_avgtime_size = %d for data connection", g_avgtime_size, ip_avgtime_size);
      
      forward_dh_msg_to_master_ex(fd, (parent_msg *)avg, g_avgtime_size - ip_avgtime_size, sizeof(MsgHdr), global_settings->data_comp_type);
      NSTL1(NULL, NULL, "Before TCPDUMP on Generator: sample count = %d, last acknowledged sample = %d, sample delay = %d,"
                        " g_tcpdump_started_time = %lld, get_ms_stamp = %lld", 
                         avg->elapsed, g_last_acked_sample, global_settings->progress_report_queue_size, 
                         g_tcpdump_started_time, get_ms_stamp());
      if((avg->elapsed - g_last_acked_sample) > global_settings->progress_report_queue_size/2)
      {
        if(IS_LAST_TCPDUMP_DUR_ENDED && (IS_ENABLE_NC_TCPDUMP(ALWAYS)||IS_ENABLE_NC_TCPDUMP(CONFAIL)))
        {
          char path_buf[MAX_VAR_SIZE + 1];
          snprintf(path_buf, MAX_VAR_SIZE, "%s/logs/TR%d/ns_logs/tcpdump", g_ns_wdir, testidx);
          NSTL1(NULL, NULL, "Taking TCPDUMP on Controller at path %s for control connection", path_buf);
          g_tcpdump_started_time = get_ms_stamp();
          nslb_start_tcp_dump(master_ip, dh_master_port, global_settings->nc_tcpdump_settings->tcpdump_duration, path_buf);
        }
      }
    }
    //else
    {
      /* this process is not a CLIENT so just print out stats */
      /* print all elemnts of total_avg */
        /*      if (run_mode != NORMAL_RUN)
       return 0;*/
      // make it global - static data_point send_data_point_ptr->
/* 	static int first_dp =1; */
/* /\************************************************************************\/ */
/*       if (first_dp) */
/*       { */
/*         fill_msg_data_opcode (avg); */
/*         first_dp = 0; */
/*       } */
    process_nd_overall_data();
    if(!(g_tsdb_configuration_flag & TSDB_MODE))
    {
      check_rtg_size();
      fill_msg_data_seq (avg, testidx);
      fill_rtg_graph_data(g_avg, g_cavg, -1);
      send_test_monitor_gdf_data(NULL,global_settings->monitor_type, g_avg, g_cavg);
      fill_cm_data(); //GDF is processing only first time in case of TSDB
    }
    else if(global_settings->write_rtg_data_in_db_or_csv)
    {
      NSTL1(NULL, NULL, "write rtg data in msg queue in TSDB mode");
      fill_all_trans_data(g_avg, g_cavg); // It will fill all transtion group data (ns_fill_gdf.c)
    }

    fill_hm_data();
    fill_um_data(g_avg, g_cavg);

   
       //will be called only in case of outbound mode 
      if(is_outbound_connection_enabled && restart_mon_list_head_ptr)
        check_and_send_mon_request_to_start_monitor();

      //Updating structure of HealthMonitorData with global values.
      update_structure_with_global_values();


      // This will get and fill SS for Unix (Using rstat)
      if(loader_opcode != CLIENT_LOADER) //rstat will run only for Standalone and Master
        get_rstat_data_for_all_servers(num_server_unix_rstat, server_stats_gp_ptr);


      if (gui_fp) // write in file for debuging
            write_gui_data();

    // Change on 26/11/09, now GUI read data from raw data file(rtgMessage.dat)

    /* if header size is changed, one will also need to change DATA_PKT_SIZE_TX_BY_NS file data.java in GUI since data packet of all types of data (START_PKT, DATA_PKT, END_PKT & All control data packet(PAUSED_MSG, RESUME_MSG)) is set to DATA_PKT_SIZE_TX_BY_NS which is presently 112 bytes(msg header size) */
 
    /*data must be write in to file before sendto GUI*/
    //printf("sent data msg\n");
   if(!(g_tsdb_configuration_flag & TSDB_MODE)) {
    if (rtg_fp) {
      if (fwrite(msg_data_ptr, msg_data_size, 1, rtg_fp) < 1)
      {
        perror ("deliver_report(): rtg write ");
        char cmmd[124];
        sprintf(cmmd, "kill -11 %d", getpid());
        NSTL1(0, NULL, "Going to generate core using cmd (%s) for data connection", cmmd);
        system(cmmd);
      }
      fflush(rtg_fp); // To force all buffered data to rtg file, without fflush partial data will write
    }
  }

    INIT_PERIODIC_ALLOC_STATS;

    //This will initialize epoll stats in Cavisson_Diagnostics graph
    INIT_PERIODIC_EPOLL_STATS;
        // Abhishek 9/10/2006 - moved here to check delivery report
    if (loader_opcode != CLIENT_LOADER)
    {
      if (global_settings->gui_server_addr[0])
      {
        if(gui_fd == -1)
          open_connect_to_gui_server(&gui_fd, global_settings->gui_server_addr, global_settings->gui_server_port);
      }
      //gui_fd will be set in open_connect_to_gui_server function
      if(gui_fd != -1)
      {
        /* Netstorm sends Message Header only to gui since gui only requires header, rest of the data it reads from
         * rtgMessage.dat file */
        if (send(gui_fd, msg_data_ptr, sizeof(Msg_data_hdr), 0) != sizeof(Msg_data_hdr)) 
        {
          print_core_events((char*)__FUNCTION__, __FILE__,
                             "NetStorm unable to send data (Message Header) to GUI (%s:%hd) server."
                             " error = %s", global_settings->gui_server_addr, global_settings->gui_server_port, nslb_strerror(errno) );
          if(errno != ECONNREFUSED)
          {
            CLOSE_FD(gui_fd);
            open_connect_to_gui_server(&gui_fd, global_settings->gui_server_addr, global_settings->gui_server_port);
          }
        }
      }
    
      if (global_settings->secondary_gui_server_addr[0])
      {
        send_data_to_secondary_gui_server();
      }
    }

    //If progress report enable
    if (global_settings->progress_report_mode > DISABLE_PROGRESS_REPORT ) { 
    if ((global_settings->display_report == 0) || (global_settings->display_report == 1))
      display_period = 1;

    if ((global_settings->display_report == 0) || (global_settings->display_report == 2))
      display_cumul = 1;

#if 0
	if(avg->complete) 
        else 
          sprintf(tmp_buf, "Expected = %d, Received = %d",
			    global_settings->num_process, global_settings->num_process - num_active);
#endif

        tmp_buf[0] = '\0';
        print2f(rfp, "netstorm: -%s-- %d sec (%2.2d:%2.2d:%2.2d HH:MM:SS) (Actual time: %s) %s\n",
	     avg->complete?"--":"**",
	     avg->elapsed*global_settings->progress_secs/1000,
	     (avg->elapsed*global_settings->progress_secs/1000)/3600,
	     ((avg->elapsed*global_settings->progress_secs/1000)%3600)/60,
	     ((avg->elapsed*global_settings->progress_secs/1000)%3600)%60,
	     get_relative_time(), tmp_buf);

      if (display_period && (global_settings->report_mask & URL_REPORT)) {
	print_report(console_fp, rfp, URL_REPORT, 1, avg, c_avg, "     Url/Period");
      }

      if (display_cumul && (global_settings->report_mask & URL_REPORT)) {
	print_report(console_fp, rfp, URL_REPORT, 0, avg, c_avg, "     Url Cumul.");
      }

      if (display_period && (global_settings->report_mask & SMTP_REPORT)) {
	print_report(console_fp, rfp, SMTP_REPORT, 1, avg, c_avg, "    SMTP/Period");
      }

      if (display_cumul && (global_settings->report_mask & SMTP_REPORT)) {
	print_report(console_fp, rfp, SMTP_REPORT, 0, avg, c_avg, "    SMTP Cumul.");
      }

      if (display_period && (global_settings->report_mask & POP3_REPORT)) {
	print_report(console_fp, rfp, POP3_REPORT, 1, avg, c_avg, "    POP3/Period");
      }

      if (display_cumul && (global_settings->report_mask & POP3_REPORT)) {
	print_report(console_fp, rfp, POP3_REPORT, 0, avg, c_avg, "    POP3 Cumul.");
      }

      if (display_period && (global_settings->report_mask & FTP_REPORT)) {
	print_report(console_fp, rfp, FTP_REPORT, 1, avg, c_avg, "     FTP/Period");
        NSDL2_MESSAGES(NULL, NULL, "Value of display_period is = %d for data connection and value of report mask is = %d",display_cumul, global_settings->report_mask); 
      }

      if (display_cumul && (global_settings->report_mask & FTP_REPORT)) {
	print_report(console_fp, rfp, FTP_REPORT, 0, avg, c_avg, "     FTP Cumul.");
        NSDL2_MESSAGES(NULL, NULL, "Value of display_cumul is = %d for data connection and value of report mask is = %d",display_cumul, global_settings->report_mask); 
      }

      if (display_period && (global_settings->report_mask & LDAP_REPORT)) {
	print_report(console_fp, rfp, LDAP_REPORT, 1, avg, c_avg, "     LDAP/Period");
        NSDL2_MESSAGES(NULL, NULL, "Value of display_period is = %d for data connection and value of report mask is = %d",display_cumul, global_settings->report_mask); 
      }

      if (display_cumul && (global_settings->report_mask & LDAP_REPORT)) {
	print_report(console_fp, rfp, LDAP_REPORT, 0, avg, c_avg, "     LDAP Cumul.");
        NSDL2_MESSAGES(NULL, NULL, "Value of display_cumul is = %d for data connection and value of report mask is = %d",display_cumul, global_settings->report_mask); 
      }

      if (display_period && (global_settings->report_mask & IMAP_REPORT)) {
	print_report(console_fp, rfp, IMAP_REPORT, 1, avg, c_avg, "     IMAP/Period");
        NSDL2_MESSAGES(NULL, NULL, "Value of display_period is = %d for data connection, and value of report mask is = %d",display_cumul, global_settings->report_mask); 
      }

      if (display_cumul && (global_settings->report_mask & IMAP_REPORT)) {
	print_report(console_fp, rfp, IMAP_REPORT, 0, avg, c_avg, "     IMAP Cumul.");
        NSDL2_MESSAGES(NULL, NULL, "Value of display_cumul is = %d for data connection and value of report mask is = %d",display_cumul, global_settings->report_mask); 
      }

      if (display_period && (global_settings->report_mask & JRMI_REPORT)) {
	print_report(console_fp, rfp, JRMI_REPORT, 1, avg, c_avg, "     JRMI/Period");
        NSDL2_MESSAGES(NULL, NULL, "Value of display_period is = %d for data connection and value of report mask is = %d",display_cumul, global_settings->report_mask); 
      }

      if (display_cumul && (global_settings->report_mask & JRMI_REPORT)) {
	print_report(console_fp, rfp, JRMI_REPORT, 0, avg, c_avg, "     JRMI Cumul.");
        NSDL2_MESSAGES(NULL, NULL, "Value of display_cumul is = %d for data connection and value of report mask is = %d",display_cumul, global_settings->report_mask); 
      }

      if (display_period && (global_settings->report_mask & DNS_REPORT)) {
	print_report(console_fp, rfp, DNS_REPORT, 1, avg, c_avg, "    DNS/Period");
      }

      if (display_cumul && (global_settings->report_mask & DNS_REPORT)) {
	print_report(console_fp, rfp, DNS_REPORT, 0, avg, c_avg, "    DNS Cumul.");
      }

      if (display_period && (global_settings->report_mask & SVR_IP_REPORT)) {
        dump_svr_ip_progress_report(console_fp, rfp, 1, avg, c_avg, "    Server Req/Period");
      }
/*
      if (display_period && (global_settings->report_mask & WS_REPORT)) {
        print_report(console_fp, rfp, WS_REPORT, 1, avg, c_avg, "    WS/Period");
      }

      if (display_cumul && (global_settings->report_mask & WS_REPORT)) {
        print_report(console_fp, rfp, WS_REPORT, 0, avg, c_avg, "    WS Cumul.");
      }
*/
#ifdef NS_TIME
      if (display_period && (global_settings->report_mask & PAGE_REPORT))
	print_report(console_fp, rfp, PAGE_REPORT, 1, avg, c_avg, "    Page/Period");

      if (display_cumul && (global_settings->report_mask & PAGE_REPORT))
	print_report(console_fp, rfp, PAGE_REPORT, 0, avg, c_avg, "    Page Cumul.");

      if (display_period && (global_settings->report_mask & TX_REPORT))
	print_report(console_fp, rfp, TX_REPORT, 1, avg, c_avg, "    Tx/Period");

      if (display_cumul && (global_settings->report_mask & TX_REPORT))
	print_report(console_fp, rfp, TX_REPORT, 0, avg, c_avg, "    Tx Cumul.");

      if (display_period && (global_settings->report_mask & SESS_REPORT))
	print_report(console_fp, rfp, SESS_REPORT, 1, avg, c_avg, "    Sess/Period");

      if (display_cumul && (global_settings->report_mask & SESS_REPORT))
	print_report(console_fp, rfp, SESS_REPORT, 0, avg, c_avg, "    Sess Cumul.");
#endif
      if (display_cumul && (global_settings->report_mask & VUSER_REPORT))
          print_vuserinfo(console_fp, rfp, 1, avg, c_avg);

      print_throughput(console_fp, rfp, 1, avg, c_avg);
      //If complete progress report enable
      if ((global_settings->progress_report_mode == COMPLETE_PROGRESS_REPORT)) {  
        if(!(g_tsdb_configuration_flag & TSDB_MODE))
	print_cm_data(console_fp, rfp);
        print_hm_data(console_fp, rfp);
        print_um_data(console_fp, rfp, (UM_data *)((char *)avg + g_avg_um_data_idx), (UM_data *)((char *)c_avg + g_cavg_um_data_idx));
      }
      // Abhishek 9/10/2006 - this will print server stat data

      }
      if(total_tx_entries)
        print_all_tx_report(console_fp, rfp, 0, avg, c_avg, "    Detail Transaction Report (cumulative)");

      if (global_settings->progress_report_mode > DISABLE_PROGRESS_REPORT) {
        fprint2f(console_fp, rfp, "\n");
      }
    }
  }
  else
  { /* It is FINISH report */
    //assert (srfp);
   
    long test_start_time = 0;

    if((test_start_time = get_test_start_time_from_summary_top()) == 0)    
      global_settings->test_duration = get_ms_stamp() - global_settings->test_start_time;
    else
    {
      time_t now;
      time(&now);
      global_settings->test_duration = ((long) difftime(now, test_start_time))*1000;
    }
     
    update_test_runphase_duration();

    avg = (avgtime*)g_avg[0];
    c_avg = (cavgtime*)g_cavg[0];
    tmp_avg = avg;
    tmp_c_avg = c_avg;

    for(i = 0; i < TOTAL_GRP_ENTERIES_WITH_GRP_KW; i++)
    {
      avg = (avgtime*)((char*)tmp_avg + (i * g_avg_size_only_grp));
      c_avg = (cavgtime*)((char*)tmp_c_avg + (i * g_cavg_size_only_grp));

      avg->url_overall_avg_time = c_avg->c_tot_time/(c_avg->url_succ_fetches?c_avg->url_succ_fetches:1);
      //avg->avg_time = c_avg->c_tot_time/(c_avg->url_succ_fetches?c_avg->url_succ_fetches:1);
      avg->smtp_avg_time = c_avg->smtp_c_tot_time/(c_avg->smtp_succ_fetches?c_avg->smtp_succ_fetches:1);
      avg->pop3_overall_avg_time = c_avg->pop3_c_tot_time/(c_avg->pop3_succ_fetches?c_avg->pop3_succ_fetches:1);
      if ((loader_opcode != MASTER_LOADER) && (!g_collect_no_eth_data)) {
              avg->eth_rx_bps = get_eth_rx_bps(1, global_settings->test_duration);
              avg->eth_tx_bps = get_eth_tx_bps(1, global_settings->test_duration);
      }
#ifdef NS_TIME
/*      if (global_settings->exclude_failed_agg) {
        avg->pg_avg_time = c_avg->pg_c_tot_time/(c_avg->pg_succ_fetches?c_avg->pg_succ_fetches:1);
        avg->tx_avg_time = c_avg->tx_c_tot_time/(c_avg->tx_c_succ_fetches?c_avg->tx_c_succ_fetches:1);
        avg->sess_avg_time = c_avg->sess_c_tot_time/(c_avg->sess_succ_fetches?c_avg->sess_succ_fetches:1);
      } else { */
        avg->pg_avg_time = c_avg->pg_c_tot_time/(c_avg->pg_fetches_completed?c_avg->pg_fetches_completed:1);
        avg->tx_avg_time = c_avg->tx_c_tot_time/(c_avg->tx_c_fetches_completed?c_avg->tx_c_fetches_completed:1);
        avg->sess_avg_time = c_avg->sess_c_tot_time/(c_avg->sess_fetches_completed?c_avg->sess_fetches_completed:1);
     // }
#endif
    }

    avg = (avgtime*)g_avg[0];
    c_avg = g_cavg[0];

    /* this process is a CLIENT */
    if (fd  >= 0) {
      /* send end_avg to MASTER */
      printf("sending End report to Coordinator...\n");
      NSDL2_REPORTING(NULL, NULL, "Here sending finish report to master: g_avgtime_size = %d, ip_avgtime_size = %d for data connection", g_avgtime_size, ip_avgtime_size);
      forward_dh_msg_to_master(fd, (parent_msg *)avg, g_avgtime_size - ip_avgtime_size);

      /* Anil: do we need to do following code under if (fd >= 0) */
// Added if 0 on date jun 9, 2008, as part of debug logging
#if 0
      if (global_settings->debug) {
	/* print finish msg and kill all children by sending SIGTERM to all children */
	sprintf(heading, "netstorm: END %s (cur=%lu)      Url  Report        ", avg->complete?"--":"**",  get_ms_stamp());
	print_report(console_fp, NULL, URL_REPORT, 0, avg, heading);
#ifdef NS_TIME
	print_report(console_fp, NULL, PAGE_REPORT, 0, avg, "END      Page Report       ");

	print_report(console_fp, NULL, TX_REPORT, 0, avg, "END      Transaction Report       ");

	print_report(console_fp, NULL, SESS_REPORT, 0, avg, "END      Session Report       ");
#endif

        print_vuserinfo(console_fp, NULL, 0, avg);
        print_throughput(console_fp, NULL, 0, avg);
      }
#endif
    }
    //else
    { /* this process is not a CLIENT so just print out stats */
      if (((testcase_shr_mem->mode == TC_FIX_MEAN_USERS) || (testcase_shr_mem->mode == TC_MEET_SLA)) && (avg->opcode == FINISH_REPORT)) {

	if (!c_avg->sess_succ_fetches)
	  avg->sess_avg_time = -1;
	else
	  avg->sess_avg_time = avg->sess_tot_time/c_avg->sess_succ_fetches;

        // fill_sla_stats(avg);
      }

      /*if (run_mode != NORMAL_RUN)
	return 0;*/

      /* save the report in mem */ 
      if (create_report_table_entry(&rnum) != SUCCESS) {
        print_core_events((char*)__FUNCTION__, __FILE__,
                          "Could not allocate memory for the report table for data connection.");
	NS_EXIT(-1, CAV_ERR_1060020);
      }
      memcpy(&reportTable[rnum], avg, sizeof(avgtime));
      //If progress report mode enable
      if (global_settings->progress_report_mode > DISABLE_PROGRESS_REPORT) {
      print2f(rfp, "netstorm: --%s- END (Actual time: %s)\n", avg->complete?"--":"**", get_relative_time());

      if (global_settings->report_mask & URL_REPORT)
          print_report(console_fp, rfp, URL_REPORT, 0, avg, c_avg, "    Url  Report");

      if (global_settings->report_mask & SMTP_REPORT)
          print_report(console_fp, rfp, SMTP_REPORT, 0, avg, c_avg, "   Smtp  Report");

      if (global_settings->report_mask & POP3_REPORT)
          print_report(console_fp, rfp, POP3_REPORT, 0, avg, c_avg, "   POP3  Report");

      if (global_settings->report_mask & WS_REPORT)
          print_report(console_fp, rfp, WS_REPORT, 0, avg, c_avg, "   WS  Report");

//      if (global_settings->report_mask & JMETER_REPORT)
//          print_report(console_fp, rfp, JMETER_REPORT, 1, avg, c_avg, "   JMeter  Report");

      if (global_settings->report_mask & TCP_CLIENT_REPORT)
          print_report(console_fp, rfp, TCP_CLIENT_REPORT, 0, avg, c_avg, "   TCP Clinet Report");

#ifdef NS_TIME
      if (global_settings->report_mask & PAGE_REPORT)
          print_report(console_fp, rfp, PAGE_REPORT, 0, avg, c_avg, "   Page  Report");

      if (global_settings->report_mask & TX_REPORT)
          print_report(console_fp, rfp, TX_REPORT, 0, avg, c_avg, "    Transaction Report");

      if (global_settings->report_mask & SESS_REPORT)
          print_report(console_fp, rfp, SESS_REPORT, 0, avg, c_avg, " Session Report");

#endif
      if (global_settings->report_mask & VUSER_REPORT)
          print_vuserinfo(console_fp, rfp, 0, avg, c_avg);

      print_throughput(console_fp, rfp, 0, avg, c_avg);
      fprint2f(console_fp, rfp, "\n");
      }

      //NC: Here we are passing TxData for controller and standalone
      create_gdf_summary_data();
      /* send the finish msg to the gui */
      {
        fill_msg_end_hdr (avg);
         if(!(g_tsdb_configuration_flag & TSDB_MODE))
        {
          if (rtg_fp)
          {
            if (fwrite(msg_data_ptr, msg_data_size, 1, rtg_fp) < 1)
              perror ("end_msg(): rtg write ");
            fflush(rtg_fp);
            CLOSE_FP(rtg_fp); // Must close now as no more data is to be written.
          }
        }
      //if (global_settings->report_level > 2)
          if (gui_fd != -1)
	  {
	    time_t ts = time(NULL);
            if(gui_fp)
              fprintf(gui_fp, "END: %s\topcode: %6.0f\n\tseq_number: %6.0f for data connection\n",
              ctime(&ts), (msg_data_hdr_local_ptr->opcode), (msg_data_hdr_local_ptr->seq_no));

          /* To increase the size of data send to GUI, netstorm send only Message Header only*/
          if (send(gui_fd, msg_data_ptr, sizeof(Msg_data_hdr), 0) != sizeof(Msg_data_hdr)) {
            print_core_events((char*)__FUNCTION__, __FILE__,
                             "NetStorm unable to send data (Message Header) to GUI server for data connection."
                             " error = %s.", nslb_strerror(errno));
            //Bug 2179 Fixed - Test should not stop if nsServer is restarted 
	    //kill_all_children();
	    //exit(1);
	  }
        }

        if (gui_fd2 != -1)
	{
          if (send(gui_fd2, msg_data_ptr, sizeof(Msg_data_hdr), 0) != sizeof(Msg_data_hdr)) 
          {
            print_core_events((char*)__FUNCTION__, __FILE__,
                             "NetStorm unable to send data (Message Header) to secondary GUI server for data connection."
                             " error = %s.", nslb_strerror(errno));
	  }
        }
      }
    }

    // [PERCENTILE] check and flush if percentile data not dumped 
    flush_pctdata();

    /*In case of percentile enable fill pct array*/
    /*NC: Bug fix done for 8968, in case of controller, percentile memory will be empty*/
    if (loader_opcode != MASTER_LOADER)
    {
      NSDL3_GDF(NULL, NULL, "In case of percentile enable fill pct array for data connection");
      if (g_percentile_report != 0) {
        fill_percentiles(parent_pdf_addr, url_data, smtp_data, pop3_data, ftp_data, dns_data, pg_data, tx_data, ss_data);
      }
    }   
    log_summary_data(avg, c_avg, url_data, smtp_data, pop3_data, ftp_data, ldap_data, imap_data, jrmi_data, dns_data, pg_data, tx_data, ss_data, gsavedTxData?gsavedTxData[0]:NULL, ws_data);
    log_global_dat(c_avg, pg_data);
  }

  //Recovery is now done after sending progress report. Earlier it was done before sending progress report.
  //Doing recovery earlier can delay send report as recovery might take time, so moving the code here

  //moved this code to wait_forever.c as per bug 8853
  //Handling runtime dynamic vector monitor
  //if(total_dynamic_vector_mon_entries)
  //{
  //  dvm_make_conn(); //handles first time & recovery
  //}

  send_node_pod_status_message_to_ndc();

  //Handling custom monitor disaster recovery
  handle_cm_disaster_recovery();

  //Handling check monitor disaster recovery
  handle_chk_mon_disaster_recovery();

  if((ndc_mccptr.fd < 0) && (global_settings->net_diagnostics_mode))
    ndc_recovery_connect();

  if((ndc_data_mccptr.fd < 0) && (global_settings->net_diagnostics_port != 0))
    handle_ndc_data_connection_recovery(1);

  if(ndc_data_mccptr.state & NS_DATA_CONN_MADE)
  {
    if(is_outbound_connection_enabled)
      make_mon_msg_and_send_to_NDC(0, 1, 1);

    ndc_data_mccptr.state &= ~NS_DATA_CONN_MADE;
    ndc_data_mccptr.state |= NS_CONNECTED;
  }

  if((lps_mccptr.fd < 0) && (global_settings->lps_mode)) 
    lps_recovery_connect();

  if(global_settings->enable_event_logger)
    nsa_log_mgr_recovery();

  //In case of netcloud logging_writer will not run on controller.
  if((run_mode == NORMAL_RUN) && loader_opcode != MASTER_LOADER)
    nsa_logger_recovery();

  if(loader_opcode == CLIENT_LOADER && global_settings->reader_run_mode)
    req_rep_uploader_recovery();

  if(loader_opcode == MASTER_LOADER && global_settings->reader_run_mode)
    nia_file_aggregator_recovery(loader_opcode);

  if((global_settings->net_diagnostics_mode >= 1) && (global_settings->db_aggregator_mode == 1))
    db_aggregator_recovery();

  if(nc_ip_data_mon_idx >= 0) 
    reset_ip_data_monitor();

  //Runtime changes take effect when deliver_report_done is 1.
  //Hence Runtime changes take effect after progress interval.
  NSDL2_REPORTING(NULL, NULL, "Setting deliver_report_done as 1 for data connection");
  deliver_report_done = 1;
  /* Earliar this function was called to create trans_detail.dat/trans_not_run.dat
   * at the end of test run but it may possible that someone has stopped test run
   * forcefully so on this case file was not created.
   * So now we are creating these files in every progress report.
   * Limitation: While writing trans_detail.dat/trans_not_run.dat file got the forced stop
   *             in this case file may not be correct.
   */
   /* NetCloud: Here TxData is send for controller and standalone 
    * -1 refers to generator index*/
   if(total_tx_entries)
    create_trans_data_file(-1, gsavedTxData[0]); 
  //If SyncPoint is enable then only create file. We are calling summary file here one time
  // as after parsing this function will be called at runtime only.
  if(global_settings->sp_enable) 
  {
    NSDL4_SP(NULL, NULL, "sp_enable = %d for data connection", global_settings->sp_enable);
    // At last when all NVM went down at end of test we need to show all syncpoint active as de-active
    if(!g_data_control_var.num_active)
    {
      int i;
      for(i = 0; i < total_syncpoint_entries; i++)
        g_sync_point_table[i].sp_actv_inactv = 0;
    }  
    create_sync_point_summary_file();
  }
  if ( g_set_check_var_to_switch_partition == 1)
  {
    if (( rtg_pkt_ts >= (g_loc_next_partition_time - global_settings->progress_secs)) || (!(g_tsdb_configuration_flag & RTG_MODE)))
    {
      check_if_partition_switch_to_be_done(g_client_data, get_ms_stamp()); 
    }
  }

  NSDL4_SP(NULL, NULL, "after create_sync_point_summary_file() for data connection");

  return 0;
}


