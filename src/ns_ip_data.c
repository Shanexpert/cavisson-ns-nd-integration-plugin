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
#include "ns_ip_data.h"
#include "ns_group_data.h"
#include "ns_connection_pool.h"
#include "netomni/src/core/ni_scenario_distribution.h"
#include "ns_monitor_profiles.h"
#include "ns_dynamic_vector_monitor.h"

IP_based_stat_gp *ip_data_gp_ptr = NULL;
IPBasedAvgTime *ip_avgtime = NULL;
int ip_avgtime_size = 0;
extern int sgrp_used_genrator_entries;
unsigned int ip_data_gp_idx = -1;
unsigned int ip_data_idx = -1;
int *g_ipdata_loc_ipdata_avg_idx;
//extern FILE *write_gdf_fp = NULL;

// keyword parsing usages 
static void ns_ip_usages(char *err)
{
  NSTL1_OUT(NULL, NULL, "Error: Invalid value of SHOW_IP_DATA keyword: %s\n", err);
  NSTL1_OUT(NULL, NULL, "  Usage: SHOW_IP_DATA <mode>\n");
  NSTL1_OUT(NULL, NULL, "  This keyword is used to show no of requests sent for perticular IP.\n");
  NSTL1_OUT(NULL, NULL, "    Mode: Mode for enable/disable the IP data. It can only be 0, 1\n");
  NSTL1_OUT(NULL, NULL, "      0 - Disable IP graph data for any IP.(default)\n");
  NSTL1_OUT(NULL, NULL, "      1 - Enable IP graph data.\n");
  NS_EXIT(-1, "%s\nUsage: SHOW_IP_DATA <mode>", err);
}

//keyword parsing
int kw_set_ip_based_data(char *buf, char *err_msg)
{   
  char keyword[MAX_DATA_LINE_LENGTH];
  char mode_str[32 + 1];
  char SendBuffer[DYNAMIC_VECTOR_MON_MAX_LEN];
  char tmp[MAX_DATA_LINE_LENGTH]; //This used to check if some extra field is given
  int num;
  int mode = 0;
        
  num = sscanf(buf, "%s %s %s", keyword, mode_str, tmp);
    
  NSDL2_PARSING(NULL, NULL, "Method called, buf = %s, num= %d , key=[%s], mode_str=[%s]", buf, num, keyword, mode_str);
    
  if(num != 2)
  { 
    ns_ip_usages("Invalid number of arguments");
  }

  if(ns_is_numeric(mode_str) == 0)
  {
    ns_ip_usages("SHOW_IP_DATA mode is not numeric");
  }

  mode = atoi(mode_str);
  if(mode < 0 || mode > 1)
  {
    ns_ip_usages("SHOW_IP_DATA mode is not valid");
  }

  global_settings->show_ip_data = mode;

  NSDL2_PARSING(NULL, NULL, "global_settings->show_ip_data = %d", global_settings->show_ip_data);

  if((loader_opcode == MASTER_LOADER) && global_settings->show_ip_data)
  { 
    //ip/port does not make any sense here, because this monitor will not make any connection. This monitor data will come on parent-generator connection.
    sprintf(SendBuffer, "DYNAMIC_VECTOR_MONITOR %s:-1 NetCloudIPData ns_ip_data.gdf 2 ns_ip_data EOC ns_ip_data", LOOPBACK_IP_PORT );
    
    NSDL2_PARSING(NULL, NULL, "running monitor for show_ip_data");
    NSDL2_MON(NULL, NULL, "Adding %s", SendBuffer);
    
    //sleep(60);
    kw_set_dynamic_vector_monitor("DYNAMIC_VECTOR_MONITOR", SendBuffer, NULL, 0, 0, NULL, err_msg, NULL, NULL, 0);
  }

  return 0;
}

char **printIpDataStat()
{
  int i = 0, j = 0;
  char **TwoD;
  char buff[1024];
  TwoD = init_2d(total_group_ip_entries);
  int write_idx = 0;
  int TwoD_id = 0;
  char *write_ptr = NULL;
     
  NSDL2_MISC(NULL, NULL, "Method Called, total_runprof_entries = %d, total_ip_entries = %d",
                          total_runprof_entries, total_ip_entries);

  for(j = 0; j < total_runprof_entries; j++)
  {
     write_idx = sprintf(buff, "%s>", RETRIEVE_BUFFER_DATA(runProfTable[j].scen_group_name));

     write_ptr = buff + write_idx;

     NSDL2_MISC(NULL, NULL, "write_idx = %d, num_ip_entries = %d, buff = %s", write_idx, runProfTable[j].gset.num_ip_entries, buff);

     GroupSettings *cur_gset;
     if(runProfTable[j].gset.num_ip_entries)
       cur_gset = &runProfTable[j].gset;
     else
       cur_gset = group_default_settings;

     for(i = 0; i < cur_gset->num_ip_entries; i++)
     {
        NSDL2_MISC(NULL, NULL, "write_idx = %d, buff = %s, i = %d, j = %d, cur_gset->ips[i].ip_str = [%s]", 
                                write_idx, buff, i, j, cur_gset->ips[i].ip_str);
        sprintf(write_ptr, "%s", cur_gset->ips[i].ip_str);
  
        fprintf(write_gdf_fp, "%s\n", buff);
        fill_2d(TwoD, TwoD_id++, buff);
     }
  }

  return TwoD;
}


// Called by ns_parent.c to update ip data size into g_avgtime_size
inline void update_ip_data_avgtime_size()
{
  NSDL1_MISC(NULL, NULL, "Method Called, g_avgtime_size = %d, ip_data_gp_idx = %d, total_group_ip_entries = %d",
                        g_avgtime_size, ip_data_gp_idx, total_group_ip_entries);
  int i;

  if((global_settings->show_ip_data == IP_BASED_DATA_ENABLED) && (total_group_ip_entries) && (loader_opcode != MASTER_LOADER))
  {
    NSDL2_MISC(NULL, NULL, "SHOW IP DATA is enabled.");

    ip_avgtime_size = (sizeof(IPBasedAvgTime) * total_group_ip_entries); 
    ip_data_gp_idx = g_avgtime_size;
    g_avgtime_size += ip_avgtime_size;
    MY_MALLOC(g_ipdata_loc_ipdata_avg_idx, global_settings->num_process * sizeof(int), "g_ipdata_loc_ipdata_avg_idx", -1);    
    for(i = 0; i < global_settings->num_process; i++)
      g_ipdata_loc_ipdata_avg_idx[i] = ip_data_gp_idx;

  } else {
    NSDL2_MISC(NULL, NULL, "SHOW IP DATA is disabled.");
  }

  NSDL2_MISC(NULL, NULL, "After g_avgtime_size = %d, ip_data_gp_idx = %d, ip_avgtime_size = %d",
                  g_avgtime_size, ip_data_gp_idx, ip_avgtime_size);
}

// Called by child
inline void set_ip_based_stat_avgtime_ptr()
{
  NSDL1_MISC(NULL, NULL, "Method Called, show_ip_data = %d, ip_data_gp_idx = %d, average_time = %p, total_group_ip_entries = %d",
                          global_settings->show_ip_data, ip_data_gp_idx, average_time, total_group_ip_entries);

  if((global_settings->show_ip_data == IP_BASED_DATA_ENABLED) && total_group_ip_entries)
  {
    ip_avgtime = (IPBasedAvgTime*)((char *)average_time + ip_data_gp_idx);
  } else {
    ip_avgtime = NULL;
  }

  NSDL2_MISC(NULL, NULL, "ip_stat_avgtime set at address = %p", ip_avgtime);
}

inline void fill_ip_gp(avgtime **g_avg)
{
  int i, j = 0;
  IPBasedAvgTime *ip_avg = NULL;
  avgtime *avg = NULL;
  IP_based_stat_gp *ip_based_stat_local_gp_ptr = ip_data_gp_ptr;

  NSDL1_GDF(NULL, NULL, "Method called, ip_based_stat_idx = %d, total_ip_entries = %d", ip_data_gp_idx, total_ip_entries);

  avg = (avgtime *)g_avg[0];
  ip_avg = (IPBasedAvgTime *) ((char*) avg + ip_data_gp_idx);
  NSDL1_GDF(NULL, NULL, "ip_data_gp_idx = %d, avg = %p, ip_avg = %p",  ip_data_gp_idx, avg, ip_avg);
  for (i = 0; i < total_group_ip_entries; i++, j++)
  {
    NSDL1_GDF(NULL, NULL, "i = %d, cur_url_req = %d", i, ip_avg[i].cur_url_req); 
    GDF_COPY_VECTOR_DATA(ip_data_idx, 0, j, 0, convert_ps(ip_avg[i].cur_url_req),
                         ip_based_stat_local_gp_ptr[j].url_req);
  }
}
