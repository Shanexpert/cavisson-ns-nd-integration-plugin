/*********************************************************************************************
* Name                   : ns_server_ip_data.c  
* Purpose                : This C file holds the function(s) for handling server ip   
* Author                 : Anubhav/jagat  
*********************************************************************************************/
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
#include "nslb_get_norm_obj_id.h"
#include "ns_ip_data.h"
#include "ns_trans.h"
#include "ns_dynamic_avg_time.h"
#include "ns_rbu_domain_stat.h"
#include "ns_svr_ip_normalization.h"
#include "ns_server_ip_data.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"

SrvIPStatGP *srv_ip_stat_gp_ptr = NULL;
SrvIPAvgTime *srv_ip_avgtime = NULL;
int srv_ip_avgtime_size = 0;
unsigned int srv_ip_data_idx = -1;
unsigned int srv_ip_data_gp_idx = 0;
int max_srv_ip_entries;
int total_normalized_svr_ips = 0;
NormObjKey normServerIPTable;

char dynamic_feature_name[MAX_DYN_OBJS][32] = {{"Default:"}, {"DynamicTX:"}, {"DynamicTXCum:"}, {"ServerIp"}, {"DynamicRBUDomain:"}, {"HTTPResponseCode:"}, {"TCPClientFailures"}};

#ifndef CAV_MAIN
extern NormObjKey normRuntimeTXTable;
#else
extern __thread NormObjKey normRuntimeTXTable;
#endif

//keyword parsing
int kw_set_show_server_ip_data(char *buf, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char mode_str[32 + 1] = {0};
  int num;
  int mode = 0;

  num = sscanf(buf, "%s %s", keyword, mode_str);

  NSDL2_PARSING(NULL, NULL, "Method called, buf = %s, num= %d , key=[%s], mode_str=[%s]", buf, num, keyword, mode_str);

  if(num != 2)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, SHOW_SERVER_IP_DATA_USAGE, CAV_ERR_1011064, CAV_ERR_MSG_1);
  }

  if(ns_is_numeric(mode_str) == 0)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, SHOW_SERVER_IP_DATA_USAGE, CAV_ERR_1011064, CAV_ERR_MSG_2);
  }

  mode = atoi(mode_str);
  if(mode < 0 || mode > 1)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, SHOW_SERVER_IP_DATA_USAGE, CAV_ERR_1011064, CAV_ERR_MSG_3);
  }
  
  global_settings->show_server_ip_data = mode;

  NSDL2_PARSING(NULL, NULL, "global_settings->show_server_ip_data = %d", global_settings->show_server_ip_data);

  return 0;
}

void printSrvIpStatGraph(char **TwoD , int *Idx2d, char *prefix, int groupId, int genId)
{
  char buff[1024];
  char vector_name[1024];
  char *name;
  int i = 0;
  int count;

  NSDL2_SVRIP(NULL, NULL, " Method called. Idx2d = %d, prefix = %s, genId = %d, groupId = %d", *Idx2d, prefix, genId, groupId);

  count = dynObjForGdf[NEW_OBJECT_DISCOVERY_SVR_IP].total + dynObjForGdf[NEW_OBJECT_DISCOVERY_SVR_IP].startId;

  for(i = 0; i < count; i++)
  {
    name = nslb_get_norm_table_data(dynObjForGdf[NEW_OBJECT_DISCOVERY_SVR_IP].normTable, i);

    if(g_runtime_flag == 0)
    {
      dynObjForGdf[NEW_OBJECT_DISCOVERY_SVR_IP].rtg_index_tbl[genId][i] = msg_data_size + ((dynObjForGdf[NEW_OBJECT_DISCOVERY_SVR_IP].rtg_group_size) * (*Idx2d));
      NSDL2_SVRIP(NULL, NULL, "RTG index set for NS/NC Controller/GeneratorId = %d, and ServerIPName = %s is %d. Index of DynObjForGdf = %d", genId, name, dynObjForGdf[NEW_OBJECT_DISCOVERY_SVR_IP].rtg_index_tbl[genId][i], NEW_OBJECT_DISCOVERY_SVR_IP);
    }

    sprintf(vector_name, "%s%s", prefix, name);
    sprintf(buff, "%s %d", vector_name, dynObjForGdf[NEW_OBJECT_DISCOVERY_SVR_IP].rtg_index_tbl[genId][i]);
    fprintf(write_gdf_fp, "%s\n", buff);

    fill_2d(TwoD, *Idx2d, vector_name);
    *Idx2d = *Idx2d  + 1;
    NSDL2_SVRIP(NULL, NULL, "Idx2d = %d", *Idx2d);
  }
}

//This will be called at partition switch
char **printSrvIpStat(int groupId)
{
  int i = 0;
  char **TwoD;
  char prefix[1024];
  int Idx2d = 0;
  NSDL2_SVRIP(NULL, NULL, "Method Called total_normalized_svr_ips = %d", total_normalized_svr_ips);
  int total_svr_ips = total_normalized_svr_ips * (sgrp_used_genrator_entries + 1);
  TwoD = init_2d(total_svr_ips);

  for(i=0; i < sgrp_used_genrator_entries + 1; i++)
  {
    getNCPrefix(prefix, i-1, -1, ">", 0); //for controller or NS as grp_data_flag is disabled and grp index fixed
    NSDL2_GDF(NULL, NULL, "in trans prefix is = %s", prefix);
    printSrvIpStatGraph(TwoD, &Idx2d, prefix, groupId, i);
  }

  msg_data_size = msg_data_size + ((dynObjForGdf[NEW_OBJECT_DISCOVERY_SVR_IP].rtg_group_size) * (sgrp_used_genrator_entries));

  return TwoD;
}

//called by parent
inline void update_srv_ip_data_avgtime_size()
{
  NSDL1_SVRIP(NULL, NULL, "Method Called, g_avgtime_size = %d, srv_ip_data_idx = %d, total_normalized_svr_ips = %d",
                        g_avgtime_size, srv_ip_data_idx, total_normalized_svr_ips);
    
  if(SHOW_SERVER_IP)
  {
    int NormalizedTableSize = 512;
    nslb_init_norm_id_table_ex(&normServerIPTable, NormalizedTableSize);
    //ns_svr_ip_init_loc2norm_table();

    NSDL2_SVRIP(NULL, NULL, "server ip data keywrord is enabled, INIT_SRV_IP_ENTRIES = %d", INIT_SRV_IP_ENTRIES);
    //Here we intially create memory for server ip with INIT_SRV_IP_ENTRIES in avg.
    max_srv_ip_entries = INIT_SRV_IP_ENTRIES;
    srv_ip_avgtime_size = sizeof(SrvIPAvgTime) * (max_srv_ip_entries);
    srv_ip_data_idx = g_avgtime_size;  
    g_avgtime_size += srv_ip_avgtime_size;
    //filling srv_ip_data_idx for each child in g_svr_ip_loc2norm_table 
    ns_svr_ip_init_loc2norm_table();
  } else {
    NSDL2_SVRIP(NULL, NULL, "server ip data keyword is disabled.");
  }

  NSDL2_SVRIP(NULL, NULL, "After g_avgtime_size = %d, srv_ip_data_idx = %d, srv_ip_avgtime_size = %d",
                  g_avgtime_size, srv_ip_data_idx, srv_ip_avgtime_size);
}

// Called by child
inline void set_srv_ip_based_stat_avgtime_ptr()
{
  NSDL1_SVRIP(NULL, NULL, "Method Called, show_server_ip_data = %d, srv_ip_data_idx = %d, average_time = %p, total_normalized_svr_ips = %d",
                          global_settings->show_server_ip_data, srv_ip_data_idx, average_time, total_normalized_svr_ips);

  if(SHOW_SERVER_IP)
  {
    srv_ip_avgtime = (SrvIPAvgTime*)((char *)average_time + srv_ip_data_idx);
  } else {
    srv_ip_avgtime = NULL;
  }
  NSDL2_SVRIP(NULL, NULL, "srv_ip_avgtime set at address = %p", srv_ip_avgtime);
}

void increment_srv_ip_data_counter(char *server_name, int server_name_len, int norm_id)
{
  NSDL2_SVRIP(NULL, NULL, "Method Called total_normalized_svr_ips = %d, norm_id = %d", 
                           total_normalized_svr_ips, norm_id);

  //srv_ip_avgtime[norm_id].ip_norm_id = norm_id; //This is not required will remove
  srv_ip_avgtime[norm_id].cur_url_req++;
  strncpy(srv_ip_avgtime[norm_id].ip, server_name, server_name_len);
  NSDL2_SVRIP(NULL, NULL, "cur_url_req = %d, norm_id = %d, ip = %s", 
                   srv_ip_avgtime[norm_id].cur_url_req, norm_id, srv_ip_avgtime[norm_id].ip);
}
static int allocate_srv_ip_id_and_send_discovery_msg_to_parent(VUser *vptr, char *data, int data_len)
{
  int flag_new;
  int norm_id;

  NSDL1_SVRIP(vptr, NULL, "Method called, server_name = %s server_name_len = %d", data, data_len);

  norm_id = nslb_get_or_set_norm_id(&normServerIPTable, data, data_len, &flag_new);

  NSDL1_SVRIP(vptr, NULL, "Here ServerIp came: flag_new = %d, norm_id = %d", flag_new, norm_id);
  if(flag_new)
  {
    int row_num;
    int old_avg_size = g_avgtime_size;
    create_dynamic_data_avg(&g_dh_child_msg_com_con, &row_num, my_port_index, NEW_OBJECT_DISCOVERY_SVR_IP);
    //send local_norm_id to parent
    send_new_object_discovery_record_to_parent(vptr, data_len, data, NEW_OBJECT_DISCOVERY_SVR_IP, norm_id);
    //Check if g_avgtime_size is greater than the size of mccptr_buf_size then realloc connection's read_buf buffer.
    check_if_need_to_realloc_connection_read_buf(&g_dh_child_msg_com_con, my_port_index, old_avg_size, NEW_OBJECT_DISCOVERY_SVR_IP);
  }
  else
  {
    NSTL1(vptr, NULL, "Error - It should be always new at this point .. server_name = %s, server_name_len = %d ", data, data_len);
  }
  return norm_id;
}
/*This function called on on_request_write_done(). 
  In case of NS: We send request for get a norm id for server_name if we get then increment the counter otherwise send msg to parent
  In NC: If norm id not found for the server_name on generator child then it send to Generator parent and generator parent send to master.
*/
void update_counters_for_this_server(VUser *vptr, struct sockaddr_in6 *addr_in, char *host_name, int host_name_len)
{
  int norm_id;
  char *server_ip_name = NULL;
  char server_vec_name[512]; //server_vec_name hostname>server_ip_name ex. www.google.com>102.304.503.1
  int server_vec_name_len;

  NSDL1_SVRIP(NULL, NULL, "ServerIP: Method Called");
  server_ip_name = nslb_sockaddr_to_ip((struct sockaddr *)addr_in, 0);

  sprintf(server_vec_name, "%s>%s", host_name, server_ip_name);
  server_vec_name_len = strlen(server_vec_name);

  norm_id = nslb_get_norm_id(&normServerIPTable, server_vec_name, server_vec_name_len);

  NSDL1_SVRIP(NULL, NULL, "ServerIP: norm_id = %d host_name = %s server_ip_name = %s server_vec_name = %s",
                                     norm_id, host_name, server_ip_name, server_vec_name);

  //New server name found hence sending discovery to parent
  if(norm_id == -2) {
    norm_id = allocate_srv_ip_id_and_send_discovery_msg_to_parent(vptr, server_vec_name, server_vec_name_len);
  }
  increment_srv_ip_data_counter(server_vec_name, server_vec_name_len, norm_id);
}

void fill_srv_ip_data_gp(avgtime **g_avg)
{
  int i, gv_idx;
  //long num_samples = 0; // Taking long as it is periodic
  avgtime *avg = NULL;
  SrvIPAvgTime *srv_ip_avg = NULL;
  int j = 0;

  NSDL2_GDF(NULL, NULL, "Method called");

  SrvIPStatGP *srv_ip_based_stat_local_gp_ptr;
  for(gv_idx = 0; gv_idx < sgrp_used_genrator_entries + 1; gv_idx++)
  {
    avg = (avgtime *)g_avg[gv_idx];
    srv_ip_avg = (SrvIPAvgTime *) ((char*) avg + srv_ip_data_idx); 
    NSDL1_SVRIP(NULL, NULL, "Fill SRVIP: avg = %p gen_idx = %d srv_ip_avg = %p",avg, gv_idx, srv_ip_avg);
    // No need to do memset as we are filling always
    for(i = 0; i < total_normalized_svr_ips; i++, j++)
    {
      NSDL1_SVRIP(NULL, NULL, "DynObjForGdf info ServerIp: ServerIpIdx = %d, dynObjForGdf[NEW_OBJECT_DISCOVERY_SVR_IP].startId=%d," 
                              "dynObjForGdf[NEW_OBJECT_DISCOVERY_SVR_IP].total=%d,"
                              "dynObjForGdf[NEW_OBJECT_DISCOVERY_SVR_IP].rtg_index_tbl[gv_idx][i] = %d", i, 
                               dynObjForGdf[NEW_OBJECT_DISCOVERY_SVR_IP].startId, dynObjForGdf[NEW_OBJECT_DISCOVERY_SVR_IP].total, 
                               dynObjForGdf[NEW_OBJECT_DISCOVERY_SVR_IP].rtg_index_tbl[gv_idx][i]);
      
      if(dynObjForGdf[NEW_OBJECT_DISCOVERY_SVR_IP].rtg_index_tbl[gv_idx][i] < 0)
        continue;

      srv_ip_based_stat_local_gp_ptr = (SrvIPStatGP *)(msg_data_ptr + dynObjForGdf[NEW_OBJECT_DISCOVERY_SVR_IP].rtg_index_tbl[gv_idx][i]);
      NSDL1_SVRIP(NULL, NULL, "i = %d, cur_url_req = %d", i, srv_ip_avg[i].cur_url_req);
      GDF_COPY_VECTOR_DATA(srv_ip_data_gp_idx, 0, j, 0, srv_ip_avg[i].cur_url_req,
                           srv_ip_based_stat_local_gp_ptr->url_req);
    }
  }
}

void dump_svr_ip_progress_report(FILE *fp1, FILE *fp2, int is_periodic, avgtime *avg, cavgtime *c_avg, char *heading)
{
  int i;
  SrvIPAvgTime *srv_ip_avg = NULL;

  srv_ip_avg = (SrvIPAvgTime*)((char*)avg + srv_ip_data_idx);
  NSDL1_SVRIP(NULL, NULL, "Method called, avg = %p, srv_ip_data_idx = %d, srv_ip_avg = %p, total_server_ip_entries = %d", avg, srv_ip_data_idx, srv_ip_avg, avg->total_server_ip_entries);

  if(dynObjForGdf[NEW_OBJECT_DISCOVERY_SVR_IP].is_gp_info_filled){
    for(i = 0; i < avg->total_server_ip_entries; i++)
    {
      //int parent_index = g_svr_ip_loc2norm_table[child_id].nvm_svr_ip_loc2norm_table[i];
      NSDL1_SVRIP(NULL, NULL, "i = %d, Ip = %s", i, srv_ip_avg[i].ip);
      fprint2f(fp1, fp2, "%s:  Server IP %s, count %d\n", heading, srv_ip_avg[i].ip,
                          srv_ip_avg[i].cur_url_req);
    }
  }
}

