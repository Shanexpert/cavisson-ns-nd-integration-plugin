#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/times.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <regex.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "nslb_util.h"
#include "netomni/src/core/ni_scenario_distribution.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"
#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "user_tables.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "url.h"
#include "util.h"
#include "timing.h"
#include "tmr.h"
#include "amf.h"
#include "ns_trans_parse.h"
#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"
#include "netstorm.h"
#include "runlogic.h"
#include "ns_gdf.h"
#include "ns_vars.h"
#include "ns_log.h"
#include "ns_cookie.h"
#include "ns_user_monitor.h"
#include "ns_alloc.h"
#include "ns_percentile.h"
#include "ns_parse_scen_conf.h"
#include "ns_server_admin_utils.h"
#include "ns_error_codes.h"
#include "ns_page.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "ns_ftp_parse.h"
#include "ns_ftp.h"
#include "ns_smtp_parse.h"
#include "ns_script_parse.h"
#include "ns_vuser.h"
#include "ns_group_data.h"
#include "ns_trace_level.h"
#include "ns_connection_pool.h"
#include "netomni/src/core/ni_scenario_distribution.h"
#include "ns_server_ip_data.h"
#include "ns_http_cache_reporting.h"
#include "ns_network_cache_reporting.h"
#include "ns_dns_reporting.h"
#include "ns_ldap.h"
#include "ns_imap.h"
#include "ns_jrmi.h"
#include "ns_netstorm_diagnostics.h"
#include "ns_rbu_page_stat.h"
#include "ns_page_based_stats.h"
#include "ns_runtime_runlogic_progress.h"
#include "nslb_get_norm_obj_id.h"
#include "ns_ip_data.h"
#include "ns_trans.h"
#include "ns_dynamic_avg_time.h"
#include "dos_attack/ns_dos_attack_reporting.h"
#include "wait_forever.h" 
#include "ns_progress_report.h" 
#include "ns_rbu_domain_stat.h"
#include "ns_svr_ip_normalization.h"
#include "ns_jmeter.h"
#include "ns_xmpp.h"
#include "ns_websocket_reporting.h"
#include "ns_http_status_codes.h"
#include "ns_socket.h"

#include "ns_test_monitor.h"

#define RESET_TX_AVG(start_idx, a) \
{ \
  int j; \
  for(j = start_idx; j < max_tx_entries; j++) {\
    NSDL1_TRANS(NULL, NULL, "start_idx = %d, max_tx_entries = %d, a = %p", start_idx, max_tx_entries, a); \
    a[j].tx_min_time = 0xFFFFFFFF; \
    a[j].tx_succ_min_time = 0xFFFFFFFF;\
    a[j].tx_failure_min_time = 0xFFFFFFFF;\
    a[j].tx_netcache_hit_min_time = 0xFFFFFFFF;\
    a[j].tx_netcache_miss_min_time = 0xFFFFFFFF;\
    a[j].tx_min_think_time = 0xFFFFFFFF; \
  }\
}

#define PARENT_RESET_TX_CAVG(start_idx, a) \
{ \
  int j; \
  for(j = start_idx; j < max_tx_entries; j++) {\
    NSDL1_TRANS(NULL, NULL, "start_idx = %d, max_tx_entries = %d, a = %p", start_idx, max_tx_entries, a); \
    a[j].tx_c_min_time = 0xFFFFFFFF; \
  }\
}

extern avgtime **g_cur_avg, **g_next_avg, **g_end_avg;

void process_parent_object_discover_response(Msg_com_con *mccptr, Norm_Ids *msg)
{
  int opcode = msg->opcode;
  int type = msg->type;
  VUser *my_vptr = (VUser*)msg->vuser;

  NSTL1(my_vptr, NULL, "%s Recieved object discovery response msg from NS/Generator Parent data connection: opcode = %d, norm_id = %d",
                        dynamic_feature_name[type], opcode, msg->norm_id);
}

void send_object_discovery_response_to_child(Msg_com_con *mccptr, Norm_Ids *msg)
{
  int type = msg->type;
  int nvm_id;

  NSTL1(NULL, NULL, "%s Sending object discovery response msg to Generator Parent/child: opcode = %d, norm_id = %d, "
                    "data = %s, data_len = %d, type = %d for data connection", dynamic_feature_name[type], msg->opcode,
                    msg->norm_id, msg->data, msg->data_len, msg->type);
  
  //In case of RBU Domain Stat or Server Ip stat no need to send response to NVM    
  if(type == NEW_OBJECT_DISCOVERY_RBU_DOMAIN || type == NEW_OBJECT_DISCOVERY_SVR_IP || type == NEW_OBJECT_DISCOVERY_STATUS_CODE)
    return;

  if(loader_opcode == CLIENT_LOADER) {
    parent_msg *recv_msg = (parent_msg *)msg; 
    nvm_id = recv_msg->top.internal.gen_rtc_idx;
    write_msg(&g_dh_msg_com_con[nvm_id], (char *)msg, sizeof(Norm_Ids), 0, DATA_MODE); 
    return;
  }

  write_msg(mccptr, (char *)msg, sizeof(Norm_Ids), 0, DATA_MODE);
}

// Parent
void process_new_object_discovery_record(Msg_com_con *mccptr, Norm_Ids *msg)
{
  int flag_new = 0;
  int nvm_id = msg->child_id;
  int type = msg->type;
  int local_norm_id = msg->local_norm_id;
  int local_dyn_avg_idx = msg->dyn_avg_idx; //this is the local avg_idx of child/generators avg(Ex Dyntx updates server ip avg idx)
  int row_num = 0;
  int norm_id;
  int old_avg_size;
  int inc_offset = -1;   //intialised with -1 as 0 is valid

  NSDL2_MISC(NULL, NULL, "Method Called, nvm_id = %d, type = %d, local_norm_id = %d for data connection", nvm_id, type, local_norm_id);

  NSTL1(NULL, NULL, "%s Recieved new object discovery record msg from child = '%d', "
      "type = %d, local_norm_id = %d for data connection", 
       dynamic_feature_name[type], nvm_id, type, local_norm_id);
  
  if(type == NEW_OBJECT_DISCOVERY_TX)
  {
    norm_id = ns_trans_add_dynamic_tx(nvm_id, local_norm_id, msg->data, msg->data_len, &flag_new);
    NSDL2_MISC(NULL, NULL, "Parent Getting norm id = %d, flag_new = %d for data connection", norm_id, flag_new);
  }
  else if(type == NEW_OBJECT_DISCOVERY_RBU_DOMAIN)
  {
    norm_id = ns_rbu_add_dynamic_domain(nvm_id, local_norm_id, msg->data, msg->data_len, &flag_new);
    NSDL2_MISC(NULL, NULL, "NEW_OBJECT_DISCOVERY_RBU_DOMAIN: parent Getting norm id = %d, flag_new = %d for data connection", norm_id, flag_new);
  } 
  else if(type == NEW_OBJECT_DISCOVERY_SVR_IP) 
  {
    norm_id = ns_add_svr_ip(nvm_id, local_norm_id, msg->data, msg->data_len, &flag_new);
    NSDL2_MISC(NULL, NULL, "NEW_OBJECT_DISCOVERY_SVR_IP: parent Getting norm id = %d, flag_new = %d for data connection", norm_id, flag_new); 
  }
  else if(type == NEW_OBJECT_DISCOVERY_STATUS_CODE) 
  {
    norm_id = ns_add_dyn_status_code(nvm_id, local_norm_id, msg->data, msg->data_len, &flag_new);
    NSDL2_MISC(NULL, NULL, "NEW_OBJECT_DISCOVERY_STATUS_CODE: Parent Getting norm id = %d, flag_new = %d", norm_id, flag_new); 
  }
  else if(type == NEW_OBJECT_DISCOVERY_TCP_CLIENT_FAILURES) 
  {
    norm_id = ns_add_dyn_tcp_client_failures(nvm_id, local_norm_id, msg->data, msg->data_len, &flag_new);
    NSDL2_MISC(NULL, NULL, "NEW_OBJECT_DISCOVERY_TCP_CLIENT_FAILURES: Parent Getting norm id = %d, flag_new = %d", norm_id, flag_new); 
  }
  else if(type == NEW_OBJECT_DISCOVERY_UDP_CLIENT_FAILURES) 
  {
    norm_id = ns_add_dyn_udp_client_failures(nvm_id, local_norm_id, msg->data, msg->data_len, &flag_new);
    NSDL2_MISC(NULL, NULL, "NEW_OBJECT_DISCOVERY_UDP_CLIENT_FAILURES: Parent Getting norm id = %d, flag_new = %d", norm_id, flag_new); 
  }
  else
  {
    NSDL2_MISC(NULL, NULL, "Unknown dynamic object type for data connection");
  }
 
  if(flag_new) 
  {
    old_avg_size = g_avgtime_size;
    create_dynamic_data_avg(mccptr, &row_num, nvm_id, type);
  }

  //TODO:
  /*  Need to update next dynamic object's loc2norm avg idx for each child                     */
  /*  Incrementing all next dynamic avg's with offset of current index with old index */
  NSDL2_MISC(NULL, NULL, "for %s, local_dyn_avg_idx = %d", dynamic_feature_name[type], local_dyn_avg_idx); 
  if(local_dyn_avg_idx != -1 && nvm_id != -1)
  {
    switch(type)
    {
      case NEW_OBJECT_DISCOVERY_UDP_CLIENT_FAILURES:
      {
        if (IS_TCP_CLIENT_API_EXIST)
        {
          NSDL2_MISC(NULL, NULL, "At NEW_OBJECT_DISCOVERY_UDP_CLIENT_FAILURES, "
              "before increasing offset, avg_idx (On NVM) = %d", 
              g_tcp_clinet_errs_loc2normtbl[nvm_id].avg_idx); 
         
          if(inc_offset<0)
             inc_offset = local_dyn_avg_idx - g_tcp_clinet_errs_loc2normtbl[nvm_id].avg_idx; 
          g_tcp_clinet_errs_loc2normtbl[nvm_id].avg_idx += inc_offset;
         
          NSDL2_MISC(NULL, NULL, "At NEW_OBJECT_DISCOVERY_UDP_CLIENT_FAILURES, "
              "after increading offset by %d, new avg_idx is %d", 
               inc_offset, g_tcp_clinet_errs_loc2normtbl[nvm_id].avg_idx); 
        }
      }
      case NEW_OBJECT_DISCOVERY_TCP_CLIENT_FAILURES:
      {
        NSDL2_MISC(NULL, NULL, "At NEW_OBJECT_DISCOVERY_TCP_CLIENT_FAILURES, before increasing offset, loc_http_status_code_avg_idx %d", 
            g_http_status_code_loc2norm_table[nvm_id].loc_http_status_code_avg_idx); 

        if(inc_offset<0)
           inc_offset = local_dyn_avg_idx - g_http_status_code_loc2norm_table[nvm_id].loc_http_status_code_avg_idx; 
        g_http_status_code_loc2norm_table[nvm_id].loc_http_status_code_avg_idx += inc_offset;

        NSDL2_MISC(NULL, NULL, "At NEW_OBJECT_DISCOVERY_TCP_CLIENT_FAILURES, after increading offset by %d, new loc_http_status_code_avg_idx is %d", 
                                 inc_offset, g_http_status_code_loc2norm_table[nvm_id].loc_http_status_code_avg_idx); 
      }
      case NEW_OBJECT_DISCOVERY_STATUS_CODE:
      {
        //if(total_tx_entries)
        {
           NSDL2_MISC(NULL, NULL, "At NEW_OBJECT_DISCOVERY_STATUS_CODE, before increasing offset, loc_tx_avg_idx %d", 
                                   g_tx_loc2norm_table[nvm_id].loc_tx_avg_idx); 
           if(inc_offset<0)
              inc_offset = local_dyn_avg_idx - g_tx_loc2norm_table[nvm_id].loc_tx_avg_idx; 
           g_tx_loc2norm_table[nvm_id].loc_tx_avg_idx += inc_offset;
           NSDL2_MISC(NULL, NULL, "At NEW_OBJECT_DISCOVERY_STATUS_CODE, after increading offset by %d, new loc_tx_avg_idx is %d", 
                                   inc_offset, g_tx_loc2norm_table[nvm_id].loc_tx_avg_idx); 
        }
      }
      case NEW_OBJECT_DISCOVERY_TX:
      {
        //if(SHOW_SERVER_IP && (srv_ip_data_idx != -1))
        if(SHOW_SERVER_IP ){
           NSDL2_MISC(NULL, NULL, "At NEW_OBJECT_DISCOVERY_TX, before increasing offset, loc_srv_ip_avg_idx %d", 
                                   g_svr_ip_loc2norm_table[nvm_id].loc_srv_ip_avg_idx); 
           if(inc_offset<0)
              inc_offset = local_dyn_avg_idx - g_svr_ip_loc2norm_table[nvm_id].loc_srv_ip_avg_idx; 
           g_svr_ip_loc2norm_table[nvm_id].loc_srv_ip_avg_idx += inc_offset;
           NSDL2_MISC(NULL, NULL, "At NEW_OBJECT_DISCOVERY_TX,  after increading offset by %d, new loc_srv_ip_avg_idx is %d", 
                                inc_offset, g_svr_ip_loc2norm_table[nvm_id].loc_srv_ip_avg_idx); 
           }
      }
      case NEW_OBJECT_DISCOVERY_SVR_IP:
      {

        if((global_settings->show_ip_data == IP_BASED_DATA_ENABLED) && total_group_ip_entries)
        {
           NSDL2_MISC(NULL, NULL, "At NEW_OBJECT_DISCOVERY_SVR_IP, before increasing offset, loc_ipdata_avg_idx %d", 
                                    g_ipdata_loc_ipdata_avg_idx[nvm_id]); 
          if(inc_offset<0)
            inc_offset = local_dyn_avg_idx - g_ipdata_loc_ipdata_avg_idx[nvm_id]; 
          g_ipdata_loc_ipdata_avg_idx[nvm_id] += inc_offset;
          NSDL2_MISC(NULL, NULL, "At NEW_OBJECT_DISCOVERY_SVR_IP, after increading offset by %d, new loc_ipdata_avg_idx = %d", 
                                inc_offset, g_ipdata_loc_ipdata_avg_idx[nvm_id]); 
        }

        if(global_settings->rbu_domain_stats_mode && (rbu_domain_stat_avg_idx != -1))
        {
          NSDL2_MISC(NULL, NULL, "At NEW_OBJECT_DISCOVERY_SVR_IP, before increasing offset, loc_domain_avg_idx %d", 
                                    g_domain_loc2norm_table[nvm_id].loc_domain_avg_idx); 
          if(inc_offset<0)
            inc_offset = local_dyn_avg_idx - g_domain_loc2norm_table[nvm_id].loc_domain_avg_idx; 
          g_domain_loc2norm_table[nvm_id].loc_domain_avg_idx += inc_offset;
          NSDL2_MISC(NULL, NULL, "At NEW_OBJECT_DISCOVERY_SVR_IP, after increading offset by %d, new loc_domain_avg_idx = %d", 
                                inc_offset, g_domain_loc2norm_table[nvm_id].loc_domain_avg_idx); 
        }
        
      }
    }
  }

  if(loader_opcode == CLIENT_LOADER)
  {
    //Here Generator Parent forward message to master and return
    NSTL1(NULL, NULL, "%s Forwarding normalization object disecovery msg to master - opcode = %d, data = %s, data_len = %d "
                      "for data connection", dynamic_feature_name[type], msg->opcode, msg->data, msg->data_len, msg->type);
    msg->local_norm_id = norm_id;
    msg->dyn_avg_idx = -1;
    switch(type)
    {
      case NEW_OBJECT_DISCOVERY_UDP_CLIENT_FAILURES:
      if(msg->dyn_avg_idx == -1)
      {
        msg->dyn_avg_idx = g_tcp_client_failures_avg_idx;
      } 
      case NEW_OBJECT_DISCOVERY_TCP_CLIENT_FAILURES:
      if(msg->dyn_avg_idx == -1)
      {
        msg->dyn_avg_idx = http_resp_code_avgtime_idx;
      } 
      case NEW_OBJECT_DISCOVERY_STATUS_CODE:
      if(msg->dyn_avg_idx == -1)
      {
        msg->dyn_avg_idx = g_trans_avgtime_idx;
      } 
      case NEW_OBJECT_DISCOVERY_TX:
      if(msg->dyn_avg_idx == -1)
      {
        msg->dyn_avg_idx = srv_ip_data_idx;
      }
      case NEW_OBJECT_DISCOVERY_SVR_IP:
      if(msg->dyn_avg_idx == -1)
      {
        if((global_settings->show_ip_data == IP_BASED_DATA_ENABLED) && total_group_ip_entries)
          msg->dyn_avg_idx = ip_data_gp_idx;
        else 
          msg->dyn_avg_idx = rbu_domain_stat_avg_idx;
      } 
    }
/*  Previous Logic
    if(type == NEW_OBJECT_DISCOVERY_STATUS_CODE)
      msg->dyn_avg_idx = g_trans_avgtime_idx;
    else if(type == NEW_OBJECT_DISCOVERY_TX)
      msg->dyn_avg_idx = srv_ip_data_idx;
    else if(type == NEW_OBJECT_DISCOVERY_SVR_IP)
      msg->dyn_avg_idx = rbu_domain_stat_avg_idx; */
    parent_msg *recv_msg = (parent_msg *)msg;
    recv_msg->top.internal.gen_rtc_idx = nvm_id;
    forward_dh_msg_to_master(dh_master_fd, (parent_msg *)msg, sizeof(Norm_Ids));
  } else {
    msg->opcode = NS_NEW_OBJECT_DISCOVERY_RESPONSE;
    msg->norm_id = norm_id;
    send_object_discovery_response_to_child(mccptr, msg);
  }

  //Check if g_avgtime_size is greater than the size of mccptr_buf_size then realloc connection's read_buf buffer.
  check_if_need_to_realloc_connection_read_buf(mccptr, nvm_id, old_avg_size, type); 
}

void send_new_object_discovery_record_to_parent(VUser *vptr, int data_len, char *data, int type, int norm_id)
{
  Norm_Ids msg;

  memset(&msg, 0, sizeof(Norm_Ids));
  msg.opcode = NS_NEW_OBJECT_DISCOVERY;
  msg.child_id = my_port_index;
  msg.type = type;
  msg.local_norm_id = norm_id; 
  msg.dyn_avg_idx = -1;
  switch (type)
  {
    case NEW_OBJECT_DISCOVERY_UDP_CLIENT_FAILURES:
      if(msg.dyn_avg_idx == -1)
        msg.dyn_avg_idx = g_tcp_client_failures_avg_idx;
    case NEW_OBJECT_DISCOVERY_TCP_CLIENT_FAILURES:
      if(msg.dyn_avg_idx == -1)
        msg.dyn_avg_idx = http_resp_code_avgtime_idx;
    case NEW_OBJECT_DISCOVERY_STATUS_CODE:
      if(msg.dyn_avg_idx == -1)
        msg.dyn_avg_idx = g_trans_avgtime_idx;
    case NEW_OBJECT_DISCOVERY_TX:
      if(msg.dyn_avg_idx == -1)
        msg.dyn_avg_idx = srv_ip_data_idx;
    case NEW_OBJECT_DISCOVERY_SVR_IP:
      if(msg.dyn_avg_idx == -1)
      {
        if((global_settings->show_ip_data == IP_BASED_DATA_ENABLED) && total_group_ip_entries)
          msg.dyn_avg_idx = ip_data_gp_idx;
        else 
          msg.dyn_avg_idx = rbu_domain_stat_avg_idx;
      }
  }
/* Previous Logic

  if(type == NEW_OBJECT_DISCOVERY_STATUS_CODE)
    msg.dyn_avg_idx = g_trans_avgtime_idx;
  else if(type == NEW_OBJECT_DISCOVERY_TX)
    msg.dyn_avg_idx = srv_ip_data_idx;
  else if(type == NEW_OBJECT_DISCOVERY_SVR_IP)
    msg.dyn_avg_idx = rbu_domain_stat_avg_idx;  */
  msg.data_len = data_len;
  strncpy(msg.data, data, data_len);
  msg.data[data_len] = '\0';
  msg.vuser = vptr; // vptr is returned by parent to find which user it is?
  msg.msg_len = sizeof(Norm_Ids) - sizeof(int);

  NSTL1(vptr, NULL, "%s Sending new object discovery record msg to NS/Generator Parent: opcode = %d, data = %s, data_len = %d, "
                    "msg.local_norm_id = %d, msg.child_id = %d", dynamic_feature_name[type], msg.opcode, msg.data, msg.data_len, 
                     msg.local_norm_id, msg.child_id);

  write_msg(&g_dh_child_msg_com_con, (char *)&msg, sizeof(Norm_Ids), 0, DATA_MODE);
}

inline void set_ftp_cavg_ptr(cavgtime *local_cavg, int type)
{
  NSDL1_MISC(NULL, NULL, "%s Method called local_cavg = %p", dynamic_feature_name[type], local_cavg);
  if(global_settings->protocol_enabled & FTP_PROTOCOL_ENABLED) {
    NSDL3_MISC(NULL, NULL, "%s FTP is enabled.", dynamic_feature_name[type]);
    ftp_cavgtime = (FTPCAvgTime *)((char *)local_cavg + g_ftp_cavgtime_idx);
  } else {
    NSDL3_MISC(NULL, NULL, "%s FTP is disabled.", dynamic_feature_name[type]);
  }
  NSDL1_MISC(NULL, NULL, "%s Method existing, ftp_cavgtime = %p", dynamic_feature_name[type], ftp_cavgtime);
}

inline void set_ldap_cavg_ptr(cavgtime *local_cavg, int type)
{
  NSDL1_MISC(NULL, NULL, "%s Method Called, g_ldap_cavgtime_idx = %d", dynamic_feature_name[type], g_ldap_cavgtime_idx);
  if(global_settings->protocol_enabled & LDAP_PROTOCOL_ENABLED) {
    NSDL3_MISC(NULL, NULL, "%s LDAP is enabled.", dynamic_feature_name[type]);
    ldap_cavgtime = (LDAPCAvgTime *)((char *)local_cavg + g_ldap_cavgtime_idx);
  } else {
    NSDL3_MISC(NULL, NULL, "%s LDAP is disabled.", dynamic_feature_name[type]);
  }
  NSDL1_MISC(NULL, NULL, "%s Method existing, ldap_cavgtime = %p", dynamic_feature_name[type], ldap_cavgtime);
}

inline void set_imap_cavg_ptr(cavgtime *local_cavg, int type)
{
  NSDL1_MISC(NULL, NULL, "%s Method Called, g_imap_cavgtime_idx = %d", dynamic_feature_name[type], g_imap_cavgtime_idx);
  if(global_settings->protocol_enabled & IMAP_PROTOCOL_ENABLED) {
    NSDL3_MISC(NULL, NULL, "%s IMAP is enabled.", dynamic_feature_name[type]);
    imap_cavgtime = (IMAPCAvgTime *)((char *)local_cavg + g_imap_cavgtime_idx);
  } else {
    NSDL3_MISC(NULL, NULL, "%s IMAP is disabled.", dynamic_feature_name[type]);
  }
  NSDL1_MISC(NULL, NULL, "%s Method existing, imap_cavgtime = %p", dynamic_feature_name[type], imap_cavgtime);
}

inline void set_jrmi_cavg_ptr(cavgtime *local_cavg, int type)
{
  NSDL1_MISC(NULL, NULL, "%s Method Called, g_jrmi_cavgtime_idx = %d", dynamic_feature_name[type], g_jrmi_cavgtime_idx);
  if(global_settings->protocol_enabled & JRMI_PROTOCOL_ENABLED) {
    NSDL3_MISC(NULL, NULL, "%s JRMI is enabled.", dynamic_feature_name[type]);
    jrmi_cavgtime = (JRMICAvgTime *)((char *)local_cavg + g_jrmi_cavgtime_idx);
  } else {
    NSDL3_MISC(NULL, NULL, "%s JRMI is disabled.", dynamic_feature_name[type]);
  }
  NSDL3_MISC(NULL, NULL, "%s Method existing, jrmi_cavgtime = %p", dynamic_feature_name[type], jrmi_cavgtime);
}

inline void set_xmpp_cavg_ptr(cavgtime *local_cavg, int type)
{
  NSDL1_MISC(NULL, NULL, "%s Method Called, g_xmpp_cavgtime_idx = %d", dynamic_feature_name[type], g_xmpp_cavgtime_idx);
  if(global_settings->protocol_enabled & XMPP_PROTOCOL_ENABLED) {
    NSDL3_MISC(NULL, NULL, "%s XMPP is enabled.", dynamic_feature_name[type]);
    xmpp_cavgtime = (XMPPCAvgTime *)((char *)local_cavg + g_xmpp_cavgtime_idx);
  } else {
    NSDL3_MISC(NULL, NULL, "%s XMPP is disabled.", dynamic_feature_name[type]);
  }
  NSDL1_MISC(NULL, NULL, "%s Method existing, xmpp_cavgtime = %p", dynamic_feature_name[type], xmpp_cavgtime);
}

//TODO: set for cavg when SHOW_GROUP_DATA enabled
inline void set_num_groups_cavg_ptr(cavgtime *local_cavg, int type)
{

}

inline void set_um_cavg_ptr(cavgtime *local_cavg, int type)
{
  NSDL1_MISC(NULL, NULL, "%s Method Called, g_cavg_um_data_idx = %d", dynamic_feature_name[type], g_cavg_um_data_idx);
  if(total_um_data_entries) {
    um_data = (UM_data *)((char *)local_cavg + g_cavg_um_data_idx);
  }
  NSDL3_MISC(NULL, NULL, "%s Method existing, um_data = %p", dynamic_feature_name[type], um_data);
}

inline void set_diag_cavg_ptr(cavgtime *local_cavg, int type)
{
  NSDL1_MISC(NULL, NULL, "%s Method Called, g_ns_diag_cavgtime_idx = %d", dynamic_feature_name[type], g_ns_diag_cavgtime_idx);
  if(global_settings->g_enable_ns_diag) {
    NSDL3_MISC(NULL, NULL, "%s Memory debuging is enabled.", dynamic_feature_name[type]);
    ns_diag_cavgtime = (NSDiagCAvgTime *)((char *)local_cavg + g_ns_diag_cavgtime_idx);
    if(loader_opcode != STAND_ALONE)
      nc_diag_cavgtime = (NCDiagCAvgTime *)((char *)local_cavg + g_ns_diag_cavgtime_idx);
  } else {
    NSDL3_MISC(NULL, NULL, "%s Memory debuging is disabled.", dynamic_feature_name[type]);
  }

  NSDL4_MISC(NULL, NULL, "%s Method existing, ns_diag_cavgtime = %p", dynamic_feature_name[type], ns_diag_cavgtime);
}

inline void set_trans_cavg_ptr(cavgtime *local_cavg, int type)
{
  NSDL1_MISC(NULL, NULL, "%s Method Called, g_trans_cavgtime_idx = %d", dynamic_feature_name[type], g_trans_cavgtime_idx);
  txCData = (TxDataCum *)((char *)local_cavg + g_trans_cavgtime_idx);
  NSDL4_MISC(NULL, NULL, "%s Method existing, txCData = %p", dynamic_feature_name[type], txCData);
}

void set_cavg_ptr(cavgtime *cavg_ptr, int type)
{
  NSDL1_MISC(NULL, NULL, "%s Method called cavg_ptr = %p", dynamic_feature_name[type], cavg_ptr);
  //Set ftp cavg pointers.
  set_ftp_cavg_ptr(cavg_ptr, type);

  //Set ldap cavg pointers.
  set_ldap_cavg_ptr(cavg_ptr, type);

  //Set imap cavg pointers.
  set_imap_cavg_ptr(cavg_ptr, type);

  //Set group data cavg pointers.
  set_jrmi_cavg_ptr(cavg_ptr, type);

  ////Set XMPP cavg pointers.
  set_xmpp_cavg_ptr(cavg_ptr, type);

  //TODO: set for cavg when SHOW_GROUP_DATA enabled
  set_num_groups_cavg_ptr(cavg_ptr, type);

  //Set user monitor cavg pointers.
  set_um_cavg_ptr(cavg_ptr, type);

  //Set diag cavg pointers.
  set_diag_cavg_ptr(cavg_ptr, type);

  // Set TCP Client Failure 

  //Set tx cavg pointers.
  set_trans_cavg_ptr(cavg_ptr, type);

  NSDL4_MISC(NULL, NULL, "%s Method existing", dynamic_feature_name[type]);
}

/*
Here we set all avg ptr in case of static or dynamic call
But in case of dynamic for TX, ServerIP --> then shift the allocated memory and realign avg ptrs.
*/
void set_avgtime_ptr(avgtime *avgtime_ptr, int update_avg_sz, int avgtime_inc_sz, int type, int update_idx, int nvm_id)
{
  NSDL1_MISC(NULL, NULL, "%s Method Called, avgtime_ptr = %p, avgtime_inc_sz = %d, type = %d",
                          dynamic_feature_name[type], avgtime_ptr, avgtime_inc_sz, type);

  //Here we point avgtime_ptr to average_time because this is common code for adjusting avg by child and parent with (static or dynamically). 
  average_time = avgtime_ptr;

  init_um_data((UM_data *)((char *)average_time + g_avg_um_data_idx));

  cache_set_cache_avgtime_ptr();

  set_proxy_avgtime_ptr();

  //For Network Cache
  set_nw_cache_stats_avgtime_ptr();

  //For DNS Lookup
  set_dns_lookup_stats_avgtime_ptr();
  //FTP
  ftp_set_ftp_avgtime_ptr();

  //LDAP
  ldap_set_ldap_avgtime_ptr();

  //IMAP
  imap_set_imap_avgtime_ptr();

  //JRMI
  jrmi_set_jrmi_avgtime_ptr();

  //XMPP
  set_xmpp_avgtime_ptr();

  //DOS Attack 
  set_dos_attack_avgtime_ptr();

  //WS
  ws_set_ws_avgtime_ptr();

  // Socket Stats
  SET_AVGTIME_PTR4SOCKET_TCP_UDP_CLIENT;

  set_ns_diag_avgtime_ptr();

  //Show group data graphs
  set_group_data_avgtime_ptr();

  //Shows RBU Page Stat graphs
  set_rbu_page_stat_data_avgtime_ptr();

  //Shows Cavisson Test Monitor graphs
  set_cavtest_data_avgtime_ptr();

  //Shows Page based Stat
  set_page_based_stat_avgtime_ptr();

  //Shows Vuser Flow graphs
  set_vuser_flow_stat_avgtime_ptr();

  // Not used after re design
  //ns_jmeter_set_avgtime_ptr();

  /*======================================================
    Manish Mishra, 13Aug2020
    -------------

    [HINT: NSDynObj]

          Update avgtime pointer for Dynamic Groups -
          Be careful in updating pointer, MUST handle
          memory shifting for other dynamic Groups
          according to your's Dynamic Group position in
          avgtime memory.
    
                           |               |       
                           +---------------+
            StaticGrps  => |               |
                           +---------------+
            FirstDynGrp => |  UDPClienFail |
                           +---------------+
                           |  TCPClientFail|
                           +---------------+
                           |  HTTP SC      |
                           +---------------+
                           |  Trans        |
                           +---------------+
                           |  SRVIP        |
                           +---------------+
                           |  SRCIP        | 
                           +---------------+
            LastDynGrp =>  |  RBUDomain    |
                           +---------------+

  ======================================================*/

  //UDP Client Failures
  set_udp_client_failures_avg_ptr();
  if(type == NEW_OBJECT_DISCOVERY_UDP_CLIENT_FAILURES)
    set_and_move_below_udp_client_failure_avg_ptr(avgtime_ptr, update_avg_sz, avgtime_inc_sz, update_idx, nvm_id);

  //TCP Client Failures
  set_tcp_client_failures_avg_ptr();
  if(type == NEW_OBJECT_DISCOVERY_TCP_CLIENT_FAILURES)
    set_and_move_below_tcp_client_failure_avg_ptr(avgtime_ptr, update_avg_sz, avgtime_inc_sz, update_idx, nvm_id);

  //Setting avg_ptr for http_response_code at start of dyn
  //For HTTP response code
  set_http_resp_code_avgtime_ptr();
  if(type == NEW_OBJECT_DISCOVERY_STATUS_CODE)
    set_and_move_below_status_code_avg_ptr(avgtime_ptr, update_avg_sz, avgtime_inc_sz, update_idx, nvm_id);

  //TX 
  set_trans_avgtime_ptr();
  if(type == NEW_OBJECT_DISCOVERY_TX)
    set_and_move_below_tx_avg_ptr(avgtime_ptr, update_avg_sz, avgtime_inc_sz, update_idx, nvm_id);

  //Server IP
  #ifndef CAV_MAIN
  set_srv_ip_based_stat_avgtime_ptr();
  if(type == NEW_OBJECT_DISCOVERY_SVR_IP)
    set_and_move_below_server_ip_avg_ptr(avgtime_ptr, update_avg_sz, avgtime_inc_sz, update_idx, nvm_id);

  //Client IP (or Source IP)
  set_ip_based_stat_avgtime_ptr();
  #endif

  //Shows RBU Domain Stat graphs
  set_rbu_domain_stat_data_avgtime_ptr();

  NSDL4_MISC(NULL, NULL, "%s Method exiting: average_time = %p", dynamic_feature_name[type], average_time);
}
/*
This function call from child_init.c 
In starting of test we allocate avgtime and set the pointers.
In case of dynamic features (TX, ServerIP) then realloc avgtime and set their pointers.
*/
int realloc_avgtime_and_set_ptrs(int new_size, int old_size, int type)
{
  int avgtime_inc_sz = new_size - old_size;
  int reset_start_idx = total_tx_entries;

  NSDL1_MISC(NULL, NULL, "%s Method called, old-average_time = %p, new_size = %d, old_size = %d, type = %d",
                           dynamic_feature_name[type], average_time, new_size, old_size, type);

  NSTL1(NULL, NULL, "%s Allocate avgtime and set ptrs: average_time = %p, with new_size = %d and old_size = %d, type = %d",
                     dynamic_feature_name[type], average_time, new_size, old_size, type);

  MY_REALLOC_AND_MEMSET(average_time, new_size, old_size, "average_time", -1);

  set_avgtime_ptr(average_time, new_size, avgtime_inc_sz, type, 1, -1);

  if(type == NEW_OBJECT_DISCOVERY_TX){
    RESET_TX_AVG(reset_start_idx,txData);
  }
  else if(type == NEW_OBJECT_DISCOVERY_RBU_DOMAIN)
    RESET_RBU_DOMAIN_STAT_AVG(rbu_domain_stat_avg, total_rbu_domain_entries, max_rbu_domain_entries);

  NSDL1_MISC(NULL, NULL, "%s, Method exit and set all avgtime ptrs\n", dynamic_feature_name[type]);
  
  return 1;
}

//Here we cannot move memory because TX data is last one at that time.
//If someone added new graph after updated transaction data and used cavg data structure then need to move a memory.
//If new added graph data is not dynamic then in this case used above update_trans_cavgtime_size() 
int parent_realloc_cavg(int size, int old_size, int type)
{
  int i;
  int reset_start_idx = total_tx_entries;
  ProgressMsg* pmsg = NULL;
  nslb_mp_handler *mpool = NULL;
  
  NSDL1_MISC(NULL, NULL, "%s Method called, size = %d, old_size = %d, type = %d", dynamic_feature_name[type], size, old_size, type);

  for(i = 0; i < (sgrp_used_genrator_entries + 1); i++)
  {
    NSTL1(NULL, NULL, "%s Realloc cavg's in progress_data_pool for pool_id = %d with update avg size = %d", 
                       dynamic_feature_name[type], i, size);
    mpool = progress_data_pool[i];
    //Reallocate all busy avgs
    pmsg = (ProgressMsg *) nslb_mp_busy_head(mpool);
    while(pmsg && pmsg->sample_cum_data)
    {
      MY_REALLOC_AND_MEMSET(pmsg->sample_cum_data, size, old_size, "reallocate progress data pool busy cavgs", -1);
      set_cavg_ptr((cavgtime *)pmsg->sample_cum_data,type);
      PARENT_RESET_TX_CAVG(reset_start_idx, txCData);
      pmsg = (ProgressMsg *)nslb_next(pmsg);
    }
    //Reallocate all free avgs
    pmsg = (ProgressMsg *) nslb_mp_free_head(mpool);
    while(pmsg && pmsg->sample_cum_data)
    {
      MY_REALLOC_AND_MEMSET(pmsg->sample_cum_data, size, old_size, "reallocate progress data pool free cavgs", -1);
      set_cavg_ptr((cavgtime *)pmsg->sample_cum_data,type);
      PARENT_RESET_TX_CAVG(reset_start_idx, txCData);
      pmsg = (ProgressMsg *)nslb_next(pmsg);
    }
    //Set g_cur_finished to reallocated memory 
    pmsg = (ProgressMsg *) nslb_mp_busy_head(mpool);
    g_cur_finished[i] = (cavgtime*)pmsg->sample_cum_data;
    //Set g_next_finished to reallocated memory 
    pmsg = (ProgressMsg*) nslb_next(pmsg);
    g_next_finished[i] = (cavgtime*)pmsg->sample_cum_data;

    MY_REALLOC_AND_MEMSET(gsavedTxData[i], size, old_size, "gsavedTxData reallocated", i);
    PARENT_RESET_TX_CAVG(reset_start_idx, gsavedTxData[i]);
  }
  return 0;
}

/*
Here we realloc g_cur_avg, g_end_avg, g_next_avg in case of dynamic features and set their pointers
It call only in dyanmic TX and ServerIP
*/
void parent_realloc_avgtime_and_set_ptrs(int size, int old_size, int type, int nvm_id)
{
  int i;
  int reset_start_idx = total_tx_entries;
  int avgtime_inc_sz = size - old_size;
  ProgressMsg* pmsg = NULL;
  nslb_mp_handler *mpool = NULL;
  int update_idx = 1;
  NSDL1_MISC(NULL, NULL, "%s Method called, size = %d, old_size = %d, type = %d, avgtime_inc_sz = %d",
                          dynamic_feature_name[type], size, old_size, type, avgtime_inc_sz);

  for(i = 0; i < (sgrp_used_genrator_entries + 1); i++)
  {
    NSTL1(NULL, NULL, "%s Realloc avg's in progress_data_pool for pool_id = %d with update avg size = %d", 
                       dynamic_feature_name[type], i, size);
    
    mpool = progress_data_pool[i];

    //Reallocate all busy avgs
    pmsg = (ProgressMsg *) nslb_mp_busy_head(mpool);
    while(pmsg && pmsg->sample_data)
    {
      MY_REALLOC_AND_MEMSET(pmsg->sample_data, size, old_size, "realloc_progress_data_pool_avgs for busy avgs", -1);
      set_avgtime_ptr((avgtime *)pmsg->sample_data, size, avgtime_inc_sz, type, update_idx, nvm_id);
      update_idx = 0;
      if(type == NEW_OBJECT_DISCOVERY_TX){
        RESET_TX_AVG(reset_start_idx,txData);}
      else if(type == NEW_OBJECT_DISCOVERY_RBU_DOMAIN)
        RESET_RBU_DOMAIN_STAT_AVG(rbu_domain_stat_avg, total_rbu_domain_entries, max_rbu_domain_entries);
      pmsg = (ProgressMsg *)nslb_next(pmsg);
    }

    //Reallocate all free avgs
    pmsg = (ProgressMsg *) nslb_mp_free_head(mpool);
    while(pmsg && pmsg->sample_data)
    {
      MY_REALLOC_AND_MEMSET(pmsg->sample_data, size, old_size, "realloc_progress_data_pool_avgs for free_avgs", -1);
      set_avgtime_ptr((avgtime *)pmsg->sample_data, size, avgtime_inc_sz, type, update_idx, nvm_id);
      update_idx = 0;
      if(type == NEW_OBJECT_DISCOVERY_TX){
        RESET_TX_AVG(reset_start_idx,txData);}
      else if(type == NEW_OBJECT_DISCOVERY_RBU_DOMAIN)
        RESET_RBU_DOMAIN_STAT_AVG(rbu_domain_stat_avg, total_rbu_domain_entries, max_rbu_domain_entries);
      pmsg = (ProgressMsg *)nslb_next(pmsg);
    }

    #ifdef CHK_AVG_FOR_JUNK_DATA
    //Print free and busy pool avg
    check_avgtime_for_junk_data("ns_dynamic_avg_time.c[712]", 2);
    #endif  

    //Set g_cur_avg to reallocated memory 
    pmsg = (ProgressMsg *) nslb_mp_busy_head(mpool);
    g_cur_avg[i] = (avgtime*)pmsg->sample_data;

    //Set g_next_avg to reallocated memory 
    pmsg = (ProgressMsg *) nslb_next(pmsg);
    g_next_avg[i] = (avgtime*)pmsg->sample_data;

    //realloc g_end_avg 
    if(g_end_avg[i])
    {
      MY_REALLOC_AND_MEMSET(g_end_avg[i], size, old_size, "end_avg Allocated Once", i);
      set_avgtime_ptr(g_end_avg[i], size, avgtime_inc_sz, type, update_idx, nvm_id);
      if(type == NEW_OBJECT_DISCOVERY_TX){
        RESET_TX_AVG(reset_start_idx,txData);}
      else if(type == NEW_OBJECT_DISCOVERY_RBU_DOMAIN)
        RESET_RBU_DOMAIN_STAT_AVG(rbu_domain_stat_avg, total_rbu_domain_entries, max_rbu_domain_entries);
    }
  }
}


/*
This function create table for NEW_OBJECT_DISCOVERY_SVR_IP, DYANMIC_TX and NEW_OBJECT_DISCOVERY_RBU_DOMAIN. 
In Parent we can realloc g_cur_avg, g_next_avg and g_end_avg and memset their size.
On NVM's we can just realloc average_time and memset with new avg size.
*/
void create_dynamic_data_avg(Msg_com_con *mccptr, int *row_num, int nvm_id, int type)
{
  NSDL2_MISC(NULL, NULL, "%s Method Called, total_normalized_svr_ips = %d, max_srv_ip_entries = %d, " 
      "total_tx_entries = %d, max_tx_entries = %d, type = %d", 
       dynamic_feature_name[type], total_normalized_svr_ips, max_srv_ip_entries, 
       total_tx_entries, max_tx_entries, type);

  int updated_avg_time_size, updated_cavg_time_size = 0;

  if(type == NEW_OBJECT_DISCOVERY_SVR_IP)
  {
    if (total_normalized_svr_ips == max_srv_ip_entries)
    {
      updated_avg_time_size = ((DELTA_SRV_IP_ENTRIES) * sizeof(SrvIPAvgTime)) + g_avgtime_size;
      NSDL2_MISC(NULL, NULL, "%s g_avgtime_size = %d, updated_avg_time_size = %d", dynamic_feature_name[type], g_avgtime_size,
                               updated_avg_time_size);
      max_srv_ip_entries += DELTA_SRV_IP_ENTRIES;
      if(my_port_index != 255)
      {
        //Here we can realloc avgtime and set all pointers for NVM'S.  
        realloc_avgtime_and_set_ptrs(updated_avg_time_size, g_avgtime_size, type);
      }
      else
      {
        //In Parent we can allocate g_cur_avg, g_next_avg and g_end_avg and memset 
        parent_realloc_avgtime_and_set_ptrs(updated_avg_time_size, g_avgtime_size, type, nvm_id);
        NSDL2_MISC(NULL, NULL, "%s Update Client idx on parent: srv_ip_data_idx = %d",
                                  dynamic_feature_name[type], srv_ip_data_idx);
      }
      g_avgtime_size = updated_avg_time_size;
    }
    *row_num = total_normalized_svr_ips++;
  }
  else if(type == NEW_OBJECT_DISCOVERY_TX)
  {
    if(total_tx_entries == max_tx_entries)
    {
      updated_avg_time_size = ((DELTA_TX_ENTRIES) * sizeof(TxDataSample)) +  g_avgtime_size;

      NSDL2_MISC(NULL, NULL, "%s updated_avg_time_size = %d, old avg size = %d", 
                              dynamic_feature_name[type], updated_avg_time_size, g_avgtime_size);

      max_tx_entries += DELTA_TX_ENTRIES;
      if(my_port_index != 255)
      {
        //Here we can realloc avgtime and set all pointers for NVM'S.  
        realloc_avgtime_and_set_ptrs(updated_avg_time_size, g_avgtime_size, type);
      }
      else
      {
        //In Parent we can allocate g_cur_avg, g_next_avg and g_end_avg and memset 
        updated_cavg_time_size = ((DELTA_TX_ENTRIES) * sizeof(TxDataCum)) + g_cavgtime_size;
        parent_realloc_avgtime_and_set_ptrs(updated_avg_time_size, g_avgtime_size, type, nvm_id);
        parent_realloc_cavg(updated_cavg_time_size, g_cavgtime_size, type);
        NSDL2_MISC(NULL, NULL, "%s Update on parent: , old g_cavgtime_size = %d, updated_cavg_time_size = %d", 
                               dynamic_feature_name[type], g_cavgtime_size, updated_cavg_time_size);
        g_cavgtime_size = updated_cavg_time_size;
      }
      g_avgtime_size = updated_avg_time_size;
    }
    total_tx_entries++;
    NSDL3_SVRIP(NULL, NULL, "%s total_tx_entries = %d, max_tx_entries = %d", 
                             dynamic_feature_name[type], total_tx_entries, max_tx_entries);
    /***************************************
      Handle shared memory for pdf data
     **************************************/
    if (g_percentile_report == 1)
    {
      if(my_port_index == 255) //Parent
        check_if_need_to_resize_parent_pdf_memory();
      else
        check_if_need_to_resize_child_pdf_memory();
    }
  }
  else if(type == NEW_OBJECT_DISCOVERY_RBU_DOMAIN)
  {
    NSDL2_MISC(NULL, NULL, "NEW_OBJECT_DISCOVERY_RBU_DOMAIN: my_port_index = %d, total_rbu_domain_entries = %d, "
                           "max_rbu_domain_entries = %d, g_avgtime_size = %d,"
                           "rbu_domain_stat_avgtime = %p",
                            my_port_index, total_rbu_domain_entries, max_rbu_domain_entries, g_avgtime_size, rbu_domain_stat_avg);
    
    if(total_rbu_domain_entries >= max_rbu_domain_entries)
    {
      updated_avg_time_size = g_avgtime_size + ((DELTA_RBU_DOMAIN_ENTRIES) * sizeof(Rbu_domain_stat_avgtime));

      NSDL2_MISC(NULL, NULL, "%s updated_avg_time_size = %d, old avg size = %d",
                              dynamic_feature_name[type], updated_avg_time_size, g_avgtime_size);

      max_rbu_domain_entries += DELTA_RBU_DOMAIN_ENTRIES;

      if(my_port_index != 255)
      {
        //Here we can realloc avgtime and set all pointers for NVM'S.  
        realloc_avgtime_and_set_ptrs(updated_avg_time_size, g_avgtime_size, type);
      }
      else
      {
        //In Parent we can allocate g_cur_avg, g_next_avg and g_end_avg and memset 
        parent_realloc_avgtime_and_set_ptrs(updated_avg_time_size, g_avgtime_size, type, nvm_id);

        //updated_cavg_time_size = ((DELTA_RBU_DOMAIN_ENTRIES) * sizeof(Rbu_domain_stat_avgtime)) + g_cavgtime_size;
        //parent_realloc_cavg(updated_cavg_time_size, g_cavgtime_size, type);
        //g_cavgtime_size = updated_cavg_time_size;
      }

      NSDL2_MISC(NULL, NULL, "%s updated_avg_time_size = %d, g_avgtime_size = %d", 
                                dynamic_feature_name[type], updated_avg_time_size, g_avgtime_size);

      g_avgtime_size = updated_avg_time_size;
    }

    total_rbu_domain_entries++;

    NSDL3_MISC(NULL, NULL, "NEW_OBJECT_DISCOVERY_RBU_DOMAIN: my_port_index = %d, %s total_rbu_domain_entries = %d, "
                           "max_rbu_domain_entries = %d",
                             my_port_index, dynamic_feature_name[type], total_rbu_domain_entries, max_rbu_domain_entries);
  }
  else if(type == NEW_OBJECT_DISCOVERY_STATUS_CODE)
  {
    NSDL2_MISC(NULL, NULL, "NEW_OBJECT_DISCOVERY_STATUS_CODE : my_port_index = %d, total_http_resp_code_entries = %d, "
                           "max_http_resp_code_entries = %d, g_avgtime_size = %d,",
                            my_port_index, total_http_resp_code_entries, max_http_resp_code_entries, g_avgtime_size);
    
    if(total_http_resp_code_entries >= max_http_resp_code_entries)
    {
      updated_avg_time_size = g_avgtime_size + ((DELTA_STATUS_CODE_ENTRIES) * sizeof(HTTPRespCodeAvgTime));

      NSDL2_MISC(NULL, NULL, "%s updated_avg_time_size = %d, old avg size = %d",
                              dynamic_feature_name[type], updated_avg_time_size, g_avgtime_size);

      max_http_resp_code_entries += DELTA_STATUS_CODE_ENTRIES;

      if(my_port_index != 255)
      {
        //Here we can realloc avgtime and set all pointers for NVM'S.  
        realloc_avgtime_and_set_ptrs(updated_avg_time_size, g_avgtime_size, type);
      }
      else
      {
        //In Parent we can allocate g_cur_avg, g_next_avg and g_end_avg and memset 
        parent_realloc_avgtime_and_set_ptrs(updated_avg_time_size, g_avgtime_size, type, nvm_id);
      }

      NSDL2_MISC(NULL, NULL, "%s updated_avg_time_size = %d, g_avgtime_size = %d", 
                                dynamic_feature_name[type], updated_avg_time_size, g_avgtime_size);

      g_avgtime_size = updated_avg_time_size;
    }
    //increment total_http_resp_code_entries
    total_http_resp_code_entries++;

    NSDL3_MISC(NULL, NULL, "NEW_OBJECT_DISCOVERY_STATUS_CODE : my_port_index = %d, %s total_http_resp_code_entries = %d, "
                           "max_http_resp_code_entries = %d",
                           my_port_index, dynamic_feature_name[type], total_http_resp_code_entries, max_http_resp_code_entries);
  }
  else if(type == NEW_OBJECT_DISCOVERY_TCP_CLIENT_FAILURES)
  {
    NSDL2_MISC(NULL, NULL, "NEW_OBJECT_DISCOVERY_TCP_CLIENT_FAILURES (Before): my_port_index = %d, " 
        "g_total_tcp_client_errs = %d, g_max_total_tcp_client_errs = %d",
         my_port_index, g_total_tcp_client_errs, g_max_total_tcp_client_errs);
    
    if(g_total_tcp_client_errs >= g_max_total_tcp_client_errs)
    {
      updated_avg_time_size = g_avgtime_size + ((DELTA_TCP_CLIENT_ERR_ENTRIES) * sizeof(TCPClientFailureAvgTime));

      NSDL2_MISC(NULL, NULL, "%s updated_avg_time_size = %d, old avg size = %d",
                              dynamic_feature_name[type], updated_avg_time_size, g_avgtime_size);

      g_max_total_tcp_client_errs += DELTA_TCP_CLIENT_ERR_ENTRIES;

      if(my_port_index != 255)
      {
        //Here we can realloc avgtime and set all pointers for NVM'S.  
        realloc_avgtime_and_set_ptrs(updated_avg_time_size, g_avgtime_size, type);
      }
      else
      {
        //In Parent we can allocate g_cur_avg, g_next_avg and g_end_avg and memset 
        parent_realloc_avgtime_and_set_ptrs(updated_avg_time_size, g_avgtime_size, type, nvm_id);

        // Update memory for cummulative
        updated_cavg_time_size = ((DELTA_TX_ENTRIES) * sizeof(TCPClientFailureCAvgTime)) + g_cavgtime_size;
        parent_realloc_cavg(updated_cavg_time_size, g_cavgtime_size, type);
        g_cavgtime_size = updated_cavg_time_size;
      }

      g_avgtime_size = updated_avg_time_size;

      NSDL2_MISC(NULL, NULL, "%s updated_avg_time_size = %d, g_avgtime_size = %d", 
                                dynamic_feature_name[type], updated_avg_time_size, g_avgtime_size);

    }

    g_total_tcp_client_errs++;

    NSDL2_MISC(NULL, NULL, "NEW_OBJECT_DISCOVERY_TCP_CLIENT_FAILURES (After): my_port_index = %d, " 
        "g_total_tcp_client_errs = %d, g_max_total_tcp_client_errs = %d",
         my_port_index, g_total_tcp_client_errs, g_max_total_tcp_client_errs);
  }
  else if(type == NEW_OBJECT_DISCOVERY_UDP_CLIENT_FAILURES)
  {
    NSDL2_MISC(NULL, NULL, "NEW_OBJECT_DISCOVERY_UDP_CLIENT_FAILURES (Before): my_port_index = %d, " 
        "g_total_udp_client_errs = %d, g_max_total_udp_client_errs = %d",
         my_port_index, g_total_udp_client_errs, g_max_total_udp_client_errs);
    
    if(g_total_udp_client_errs >= g_max_total_udp_client_errs)
    {
      updated_avg_time_size = g_avgtime_size + ((DELTA_TCP_CLIENT_ERR_ENTRIES) * sizeof(TCPClientFailureAvgTime));

      NSDL2_MISC(NULL, NULL, "%s updated_avg_time_size = %d, old avg size = %d",
                              dynamic_feature_name[type], updated_avg_time_size, g_avgtime_size);

      g_max_total_udp_client_errs += DELTA_TCP_CLIENT_ERR_ENTRIES;

      if(my_port_index != 255)
      {
        //Here we can realloc avgtime and set all pointers for NVM'S.  
        realloc_avgtime_and_set_ptrs(updated_avg_time_size, g_avgtime_size, type);
      }
      else
      {
        //In Parent we can allocate g_cur_avg, g_next_avg and g_end_avg and memset 
        parent_realloc_avgtime_and_set_ptrs(updated_avg_time_size, g_avgtime_size, type, nvm_id);

        // Update memory for cummulative
        updated_cavg_time_size = ((DELTA_TX_ENTRIES) * sizeof(TCPClientFailureCAvgTime)) + g_cavgtime_size;
        parent_realloc_cavg(updated_cavg_time_size, g_cavgtime_size, type);
        g_cavgtime_size = updated_cavg_time_size;
      }

      g_avgtime_size = updated_avg_time_size;

      NSDL2_MISC(NULL, NULL, "%s updated_avg_time_size = %d, g_avgtime_size = %d", 
                                dynamic_feature_name[type], updated_avg_time_size, g_avgtime_size);

    }

    g_total_udp_client_errs++;

    NSDL2_MISC(NULL, NULL, "NEW_OBJECT_DISCOVERY_UDP_CLIENT_FAILURES (After): my_port_index = %d, " 
        "g_total_udp_client_errs = %d, g_max_total_udp_client_errs = %d",
         my_port_index, g_total_udp_client_errs, g_max_total_udp_client_errs);
  }

  NSDL2_MISC(NULL, NULL, "Method exist, row_num = %d", *row_num);
}

//Here we move the data from old avg to new avg ptr and memset old avg data only
void move_and_memset_new_allocated_avg(void *old_avg_ptr, void *new_avg_ptr, int memset_sz, int memmove_size)
{
  memmove(new_avg_ptr, old_avg_ptr, memmove_size);
  memset(old_avg_ptr, 0, memset_sz);
  NSDL2_MISC(NULL, NULL, "Method exit and move the memory in new avg");
}

//TCP Client Failure 
void set_and_move_below_tcp_client_failure_avg_ptr(avgtime *avgtime_ptr, int updated_avg_sz, int avgtime_inc_sz, int update_idx, int nvm_id)
{
  void *old_end, *new_end;
  int memset_sz, memmove_sz;
  int size;

  NSDL2_HTTP(NULL, NULL, "Method called, Shift avgtime updated_avg_sz = %d, "
      "avgtime_inc_sz = %d, max_http_resp_code_entries = %d", 
       updated_avg_sz, avgtime_inc_sz, g_max_total_tcp_client_errs);

  //New TCP Client Failure Avgtime size
  size = g_max_total_tcp_client_errs * sizeof(TCPClientFailureAvgTime);

  new_end = (char *)g_tcp_client_failures_avg + size;
  old_end = new_end - avgtime_inc_sz;
  
  memset_sz = avgtime_inc_sz;
  memmove_sz = updated_avg_sz - (g_tcp_client_failures_avg_idx + size);

  NSTL1(NULL, NULL, "Going to adjust overlapping memory, memmove_sz = %d, "
      "updated_avg_sz = %d, avgtime_inc_sz = %d, "
      "TCP Client Failure New size = %d, g_tcp_client_failures_avg_idx = %d", 
       memmove_sz, updated_avg_sz, avgtime_inc_sz, size, g_tcp_client_failures_avg_idx);

  if(update_idx)
  {
    //Update HTTP Status Code
    if(http_resp_code_avgtime_idx != -1)
      http_resp_code_avgtime_idx += avgtime_inc_sz;
 
    //For Tx
    if(g_trans_avgtime_idx != -1)
      g_trans_avgtime_idx += avgtime_inc_sz;

    //For Server IP 
    if(srv_ip_data_idx != -1)
      srv_ip_data_idx += avgtime_inc_sz; 

    //For Source IP 
    if(ip_data_gp_idx != -1)
      ip_data_gp_idx += avgtime_inc_sz;

    //For RBU Domain Stat 
    if(rbu_domain_stat_avg_idx != -1)
      rbu_domain_stat_avg_idx += avgtime_inc_sz;
  }  

  NSDL2_HTTP(NULL, NULL, "Set avg ptrs done, old_end = %p, new_end = %p, "
      "g_tcp_client_failures_avg = %p, size = %d, memset_sz = %d, memmove_sz = %d, "
      "http_resp_code_avgtime_idx = %d, g_trans_avgtime_idx = %d, "
      "srv_ip_data_idx = %d, ip_data_gp_idx = %u ", 
       old_end, new_end, g_tcp_client_failures_avg, size, memset_sz, memmove_sz, 
       http_resp_code_avgtime_idx, g_trans_avgtime_idx, srv_ip_data_idx, ip_data_gp_idx);

  if(memmove_sz > 0)
    move_and_memset_new_allocated_avg(old_end, new_end, memset_sz, memmove_sz);
}

void set_and_move_below_udp_client_failure_avg_ptr(avgtime *avgtime_ptr, int updated_avg_sz, int avgtime_inc_sz, int update_idx, int nvm_id)
{
  void *old_end, *new_end;
  int memset_sz, memmove_sz;
  int size;

  NSDL2_HTTP(NULL, NULL, "Method called, Shift avgtime updated_avg_sz = %d, "
      "avgtime_inc_sz = %d, max_http_resp_code_entries = %d", 
       updated_avg_sz, avgtime_inc_sz, g_max_total_udp_client_errs);

  //New TCP Client Failure Avgtime size
  size = g_max_total_udp_client_errs * sizeof(TCPClientFailureAvgTime);

  new_end = (char *)g_udp_client_failures_avg + size;
  old_end = new_end - avgtime_inc_sz;
  
  memset_sz = avgtime_inc_sz;
  memmove_sz = updated_avg_sz - (g_udp_client_failures_avg_idx + size);

  NSTL1(NULL, NULL, "Going to adjust overlapping memory, memmove_sz = %d, "
      "updated_avg_sz = %d, avgtime_inc_sz = %d, "
      "TCP Client Failure New size = %d, g_udp_client_failures_avg_idx = %d", 
       memmove_sz, updated_avg_sz, avgtime_inc_sz, size, g_udp_client_failures_avg_idx);

  if(update_idx)
  {
    //Update TCP Client Failures
    if(g_tcp_client_failures_avg_idx != -1)
      g_tcp_client_failures_avg_idx += avgtime_inc_sz;

    //Update HTTP Status Code
    if(http_resp_code_avgtime_idx != -1)
      http_resp_code_avgtime_idx += avgtime_inc_sz;
 
    //For Tx
    if(g_trans_avgtime_idx != -1)
      g_trans_avgtime_idx += avgtime_inc_sz;

    //For Server IP 
    if(srv_ip_data_idx != -1)
      srv_ip_data_idx += avgtime_inc_sz; 

    //For Source IP 
    if(ip_data_gp_idx != -1)
      ip_data_gp_idx += avgtime_inc_sz;

    //For RBU Domain Stat 
    if(rbu_domain_stat_avg_idx != -1)
      rbu_domain_stat_avg_idx += avgtime_inc_sz;
  }  

  NSDL2_HTTP(NULL, NULL, "Set avg ptrs done, old_end = %p, new_end = %p, "
      "g_udp_client_failures_avg = %p, size = %d, memset_sz = %d, memmove_sz = %d, "
      "http_resp_code_avgtime_idx = %d, g_trans_avgtime_idx = %d, "
      "srv_ip_data_idx = %d, ip_data_gp_idx = %u ", 
       old_end, new_end, g_udp_client_failures_avg, size, memset_sz, memmove_sz, 
       http_resp_code_avgtime_idx, g_trans_avgtime_idx, srv_ip_data_idx, ip_data_gp_idx);

  if(memmove_sz > 0)
    move_and_memset_new_allocated_avg(old_end, new_end, memset_sz, memmove_sz);
}

/*Here we can get old transaction, serverIp, clientIp and RBU Domain Stats avg ptr. 
  create a new avg ptr for transaction, serverIp, clientIP and RBU Domain Stats.
  Updated all Graph Indexes except status code: i.e. g_trans_avgtime_idx, srv_ip_data_idx, ip_data_gp_idx and rbu_domain_stat_avg_idx
  Getting total data size(for Transaction, ServerIp, Client IP and RBU Domain).
  Move the data on new avg ptr.
  Memset data from previous avg_ptr with realloc size.
*/
void set_and_move_below_status_code_avg_ptr(avgtime *avgtime_ptr, int updated_avg_sz, int avgtime_inc_sz, int update_idx, int nvm_id)
{
  void *old_status_code_end_avg_ptr, *new_status_code_end_avg_ptr;
  int memset_sz, memmove_sz;
  int status_code_table_size;

  NSDL2_HTTP(NULL, NULL, "DynamicStatusCode: Method Called, updated_avg_sz = %d, avgtime_inc_sz = %d, max_http_resp_code_entries = %d", 
                           updated_avg_sz, avgtime_inc_sz, max_http_resp_code_entries);

  // Note - max_http_resp_code_entries is inceased after this is done
  status_code_table_size = max_http_resp_code_entries * sizeof(HTTPRespCodeAvgTime);
  new_status_code_end_avg_ptr = (char *)http_resp_code_avgtime + status_code_table_size;
  old_status_code_end_avg_ptr = new_status_code_end_avg_ptr - avgtime_inc_sz;
  
  memset_sz = avgtime_inc_sz;
  memmove_sz = updated_avg_sz - (http_resp_code_avgtime_idx + status_code_table_size);
  NSTL1(NULL, NULL, "Going to adjust overlapping memory, memmove_sz = %d, updated_avg_sz = %d, avgtime_inc_sz = %d," 
                     " status_code_table_size = %d, http_resp_code_avgtime_idx = %d", 
                     memmove_sz, updated_avg_sz, avgtime_inc_sz, status_code_table_size, http_resp_code_avgtime_idx);

  if(update_idx){
    //For Tx
    if(g_trans_avgtime_idx != -1){
      g_trans_avgtime_idx +=avgtime_inc_sz;
    }
    //For Server IP 
    if(srv_ip_data_idx != -1){  
      srv_ip_data_idx += avgtime_inc_sz; 
    }
    //For Source IP 
    if(ip_data_gp_idx != -1)
      ip_data_gp_idx += avgtime_inc_sz;
    //For RBU Domain Stat 
    if(rbu_domain_stat_avg_idx != -1){
      rbu_domain_stat_avg_idx += avgtime_inc_sz;
    }
  }  
  NSDL2_HTTP(NULL, NULL, " old_status_code_end_avg_ptr = %p, new_status_code_end_avg_ptr = %p, http_resp_code_avgtime = %p,"
                          " status_code_table_size = %d, memset_sz = %d, memmove_sz = %d, g_trans_avgtime_idx = %d,"
                          " srv_ip_data_idx = %d, ip_data_gp_idx = %u ", 
                           old_status_code_end_avg_ptr, new_status_code_end_avg_ptr, http_resp_code_avgtime, 
                           status_code_table_size, memset_sz, memmove_sz, g_trans_avgtime_idx, srv_ip_data_idx, ip_data_gp_idx);

  if(memmove_sz > 0)
    move_and_memset_new_allocated_avg(old_status_code_end_avg_ptr, new_status_code_end_avg_ptr, memset_sz, memmove_sz);
}

/*Here we can get old serverIp and clientIp avg ptr. 
  create a new avg ptr for server and client IP.
  Updated both Graph Indexes: i.e. updated_srv_ip_data_idx, updated_ip_data_gp_idx.
  Getting total data size(for Server and Client IP).
  Move the data on new avg ptr.
  Memset data from srv_ip_old_avg_ptr with realloc size.
*/
void set_and_move_below_tx_avg_ptr(avgtime *avgtime_ptr, int updated_avg_sz, int avgtime_inc_sz, int update_idx, int nvm_id)
{
  void *old_tx_end_avg_ptr, *new_tx_end_avg_ptr;
  int memset_sz, memmove_sz;
  int tx_table_size;

  NSDL2_CHILD(NULL, NULL, "DynamicTX: Method Called, updated_avg_sz = %d, avgtime_inc_sz = %d, max_tx_entries = %d", 
                           updated_avg_sz, avgtime_inc_sz, max_tx_entries);

  // Note - max_tx_entries is inceased after this is done
  tx_table_size = max_tx_entries * sizeof(TxDataSample);
  new_tx_end_avg_ptr = (char *)txData + tx_table_size;
  old_tx_end_avg_ptr = new_tx_end_avg_ptr - avgtime_inc_sz;
  
  memset_sz = avgtime_inc_sz;
  memmove_sz = updated_avg_sz - (g_trans_avgtime_idx + tx_table_size);
  NSTL1(NULL, NULL, "Going to adjust overlapping memory, memmove_sz = %d, updated_avg_sz = %d, avgtime_inc_sz = %d, tx_table_size = %d, "
                    "g_trans_avgtime_idx = %d", memmove_sz, updated_avg_sz, avgtime_inc_sz, tx_table_size, g_trans_avgtime_idx);

  if(update_idx){
    if(srv_ip_data_idx != -1){  
      srv_ip_data_idx += avgtime_inc_sz; 
      //if(nvm_id !=-1)
        //g_svr_ip_loc2norm_table[nvm_id].loc_srv_ip_avg_idx = srv_ip_data_idx;
    }
    if(ip_data_gp_idx != -1)
      ip_data_gp_idx += avgtime_inc_sz;
    if(rbu_domain_stat_avg_idx != -1){
      rbu_domain_stat_avg_idx += avgtime_inc_sz;
      //if(nvm_id !=-1)
        //g_domain_loc2norm_table[nvm_id].loc_domain_avg_idx = rbu_domain_stat_avg_idx;
    }
  }  
  NSDL2_TRANS(NULL, NULL, "old_tx_end_avg_ptr = %p, new_tx_end_avg_ptr = %p, txData = %p, tx_table_size = %d, memset_sz = %d, "
                          "memmove_sz = %d, srv_ip_data_idx = %d, ip_data_gp_idx = %u", old_tx_end_avg_ptr, new_tx_end_avg_ptr, txData, 
                           tx_table_size, memset_sz, memmove_sz, srv_ip_data_idx, ip_data_gp_idx);

  if(memmove_sz > 0)
    move_and_memset_new_allocated_avg(old_tx_end_avg_ptr, new_tx_end_avg_ptr, memset_sz, memmove_sz);
}

/*Here we can get old clientIp avg ptr. 
  create a new avg ptr for client IP.
  Updated Graph Index: i.e updated_ip_data_gp_idx.
  Getting total data size for Client IP.
  Move the data on new avg ptr.
  Memset data from ip_old_avg_ptr with realloc size.
*/
void set_and_move_below_server_ip_avg_ptr(avgtime *avgtime_ptr, int updated_avg_sz, int avgtime_inc_sz, int update_idx, int nvm_id)
{
  void *old_srv_ip_end_avg_ptr, *new_srv_ip_end_avg_ptr;
  int memset_sz, memmove_sz;
  int srv_ip_table_size;

  NSDL2_CHILD(NULL, NULL, "ServerIp: Method Called, updated_avg_sz = %d, avgtime_inc_sz = %d, max_srv_ip_entries = %d", updated_avg_sz, avgtime_inc_sz, max_srv_ip_entries);

  // Note - max_srv_ip_entries is inceased after this is done
  srv_ip_table_size = max_srv_ip_entries * sizeof(SrvIPAvgTime);
  new_srv_ip_end_avg_ptr = (char *)srv_ip_avgtime + srv_ip_table_size;
  old_srv_ip_end_avg_ptr = new_srv_ip_end_avg_ptr - avgtime_inc_sz;

  memset_sz = avgtime_inc_sz;
  memmove_sz = updated_avg_sz - (srv_ip_data_idx + srv_ip_table_size);

  if(update_idx){
    
    if(ip_data_gp_idx != -1)
      ip_data_gp_idx += avgtime_inc_sz;
    
    if(rbu_domain_stat_avg_idx != -1){
      rbu_domain_stat_avg_idx += avgtime_inc_sz;
      //if(nvm_id !=-1)
        //g_domain_loc2norm_table[nvm_id].loc_domain_avg_idx = rbu_domain_stat_avg_idx;
    }
  }
 
  NSDL2_TRANS(NULL, NULL, "old_srv_ip_end_avg_ptr = %p, new_srv_ip_end_avg_ptr = %p, srv_ip_avgtime = %p, srv_ip_table_size = %d," 
                          "memset_sz = %d, memmove_sz = %d, ip_data_gp_idx = %u", old_srv_ip_end_avg_ptr, new_srv_ip_end_avg_ptr, 
                          srv_ip_avgtime, srv_ip_table_size, memset_sz, memmove_sz, ip_data_gp_idx);

  if(memmove_sz > 0)
    move_and_memset_new_allocated_avg(old_srv_ip_end_avg_ptr, new_srv_ip_end_avg_ptr, memset_sz, memmove_sz);
}

#ifdef CHK_AVG_FOR_JUNK_DATA
/*Function to check junk data in avg_time
   0 - busy_pool
   1 - free_pool
   2 - both busy and free pool*/
void check_avgtime_for_junk_data(char *from, int which_pool)
{
  ProgressMsg* loc_pmsg = NULL;
  nslb_mp_handler *loc_mpool = NULL;
  int i, slot_id = 0;
  char msg[1024];

  for(i = 0; i < (sgrp_used_genrator_entries + 1); i++)
  {
    loc_mpool = progress_data_pool[i];

    /**Printing all busy avgs**/
    if(which_pool != 1)
    {
      loc_pmsg = (ProgressMsg *) nslb_mp_busy_head(loc_mpool);
      while(loc_pmsg && loc_pmsg->sample_data)
      {
        slot_id++;
        sprintf(msg, "Traversing Busy Pool (%s)", from);
        validate_tx_entries(msg, (avgtime *)loc_pmsg->sample_data, slot_id);
        loc_pmsg = (ProgressMsg *)nslb_next(loc_pmsg);
      }
    }
    slot_id = 0;

    /**Printing all free avgs**/
    if(which_pool != 0)
    {
      loc_pmsg = (ProgressMsg *) nslb_mp_free_head(loc_mpool);
      while(loc_pmsg && loc_pmsg->sample_data)
      {
        slot_id++;
        sprintf(msg, "Traversing Free Pool (%s)", from);
        validate_tx_entries(msg, (avgtime *)loc_pmsg->sample_data, slot_id);
        loc_pmsg = (ProgressMsg *)nslb_next(loc_pmsg);
      }
    }
  }
}
#endif
