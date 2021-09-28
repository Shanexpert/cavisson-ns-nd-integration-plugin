
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
//#include <sys/prctl.h>
#ifdef SLOW_CON
#include <linux/socket.h>
#include <netinet/tcp.h>
#define TCP_BWEMU_REV_DELAY 16
#define TCP_BWEMU_REV_RPD 17
#define TCP_BWEMU_REV_CONSPD 18
#endif
#ifdef NS_USE_MODEM
#include <linux/socket.h>
//#include <linux/cavmodem.h>
#include <netinet/tcp.h>
#include <regex.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "nslb_util.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"
#include "nslb_time_stamp.h"
#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "ns_error_codes.h"
#include "user_tables.h"
#include "ns_server.h"
#include "util.h"
#include "timing.h"
#include "tmr.h"

#include "logging.h"
#include "ns_ssl.h"
#include "ns_fdset.h"
#include "ns_goal_based_sla.h"
#include "ns_schedule_phases.h"

#include "netstorm.h"
#include "cavmodem.h"
#include "ns_wan_env.h"

#endif
#include <netdb.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#ifdef USE_EPOLL
//#include <asm/page.h>
// This code has been commented for FC8 PORTING
//#include <linux/linkage.h>
#include <linux/unistd.h>
#include <sys/epoll.h>
#include <asm/unistd.h>
#endif
#include <math.h>
#include "runlogic.h"
#include "uids.h"
#include "cookies.h"
//#include "logging.h"
#include <gsl/gsl_randist.h>
#include "weib_think.h"
#include "netstorm.h"
#include <pwd.h>
#include <stdarg.h>
#include <sys/file.h>

#include "decomp.h"
#include "ns_string.h"
#include "nslb_sock.h"
#include "poi.h"
#include "ns_sock_list.h"
#include "src_ip.h"
#include "unique_vals.h"
#include "divide_users.h"
#include "divide_values.h"
#include "child_init.h"
#include "util.h" 
#include "ns_msg_com_util.h" 
#include "output.h"
#include "smon.h"
#include "eth.h"
#include "timing.h"
#include "deliver_report.h"
#include "wait_forever.h"
#include "ns_master_agent.h"
#include "ns_gdf.h"
#include "ns_custom_monitor.h"
#include "server_stats.h"
#include "ns_trans.h"
#include "ns_sock_com.h"
#include "ns_log.h"
#include "ns_cpu_affinity.h"
#include "ns_summary_rpt.h"
#include "ns_parent.h"
#include "ns_child_msg_com.h"
#include "ns_http_hdr_states.h"
#include "ns_url_resp.h"
#include "ns_vars.h"
#include "ns_ssl.h"
#include "ns_auto_fetch_embd.h"
#include "ns_parallel_fetch.h"
#include "ns_auto_cookie.h"
#include "ns_cookie.h"
#include "ns_debug_trace.h"
#include "ns_alloc.h"
#include "ns_percentile.h"
#include "ns_auto_redirect.h"
#include "ns_url_req.h"
#include "ns_replay_access_logs.h"
#include "ns_replay_access_logs_parse.h"
#include "ns_page.h"
#include "ns_vuser.h"
#include "ns_schedule_ramp_down_fcu.h"
#include "ns_schedule_ramp_up_fcu.h"
#include "ns_global_dat.h"
#include "ns_smtp_send.h"
#include "ns_smtp.h"
#include "ns_pop3_send.h"
#include "ns_pop3.h"
#include "ns_ftp_send.h"
#include "ns_dns.h"
#include "ns_http_pipelining.h"
#include "ns_http_status_codes.h"

#include "ns_server_mapping.h"
#include "ns_event_log.h"
#include "ns_event_id.h"


#include "ns_http_hdr_states.h"
#include "ns_http_cache_table.h"
#include "ns_alloc.h"
#include "ns_http_cache_store.h"
#include "ns_http_cache_hdr.h"
#include "ns_http_cache.h"
#include "nslb_date.h"
#include "ns_vuser_ctx.h"
#include "ns_data_types.h"
#include "ns_netstorm_diagnostics.h"
#include "netomni/src/core/ni_scenario_distribution.h"
#include "ns_group_data.h"

extern void fprint2f(FILE *fp1, FILE* fp2,char* format, ...);

NSDiagCAvgTime *ns_diag_cavgtime = NULL;
NCDiagAvgTime *nc_diag_avgtime = NULL;
NCDiagCAvgTime *nc_diag_cavgtime = NULL;
unsigned int ns_diag_gp_idx;

NSDiag_gp *ns_diag_gp_ptr = NULL;
#ifndef CAV_MAIN
int g_ns_diag_avgtime_idx = -1; 
NSDiagAvgTime *ns_diag_avgtime = NULL;
#else
__thread int g_ns_diag_avgtime_idx = -1; 
__thread NSDiagAvgTime *ns_diag_avgtime = NULL;
#endif
int g_ns_diag_cavgtime_idx = -1;

//Epoll events counter for each NVM/Generator at parent/controller.
//0 index will have overall counters of all children.
//1...N will have counters of 0 to N-1 index for N children.
unsigned long* g_epollin_count;
unsigned long* g_epollout_count;
unsigned long* g_epollerr_count;
unsigned long* g_epollhup_count;

inline void set_ns_diag_avgtime_ptr() {

  NSDL2_MEMORY(NULL, NULL, "Method Called");

  if(global_settings->g_enable_ns_diag) {
    NSDL2_MEMORY(NULL, NULL, "Memory debuging is enabled.");
   /* We have allocated average_time with the size of CacheAvgTime
    * also now we can point that using g_cache_avgtime_idx*/
    ns_diag_avgtime = (NSDiagAvgTime*)((char *)average_time + g_ns_diag_avgtime_idx);
    if(loader_opcode != STAND_ALONE)
      nc_diag_avgtime = (NCDiagAvgTime*)((char *)average_time + g_ns_diag_avgtime_idx);
  } else {
    NSDL2_MEMORY(NULL, NULL, "Memory debuging is disabled.");
    ns_diag_avgtime = NULL;
    nc_diag_avgtime = NULL;
  }

  NSDL2_MEMORY(NULL, NULL, "ns_diag_avgtime = %p", ns_diag_avgtime);
  NSDL2_MEMORY(NULL, NULL, "nc_diag_avgtime = %p", nc_diag_avgtime);
}


// Add size of Cache if it is enabled 
inline void update_ns_diag_avgtime_size() {
 
  NSDL2_MEMORY(NULL, NULL, "Method Called, g_avgtime_size = %d, g_ns_diag_avgtime_idx = %d",
					  g_avgtime_size, g_ns_diag_avgtime_idx);
  
  if(global_settings->g_enable_ns_diag) {
    NSDL2_MEMORY(NULL, NULL, "Memory debuging is enabled.");
    g_ns_diag_avgtime_idx = g_avgtime_size;
    if(loader_opcode == STAND_ALONE)
      g_avgtime_size +=  sizeof(NSDiagAvgTime);
    else
      g_avgtime_size +=  sizeof(NCDiagAvgTime);
  } else {
    NSDL2_MEMORY(NULL, NULL, "Memory debuging is disabled.");
  }

  NSDL2_MEMORY(NULL, NULL, "After g_avgtime_size = %d, g_ns_diag_avgtime_idx = %d",
					  g_avgtime_size, g_ns_diag_avgtime_idx);
}

inline void update_ns_diag_cavgtime_size() {

  NSDL2_MEMORY(NULL, NULL, "Method Called, g_cavgtime_size = %d, g_ns_diag_cavgtime_idx = %d",
                                          g_cavgtime_size, g_ns_diag_cavgtime_idx);

  if(global_settings->g_enable_ns_diag) {
    NSDL2_MEMORY(NULL, NULL, "Memory debuging is enabled.");
    g_ns_diag_cavgtime_idx = g_cavgtime_size;
    if(loader_opcode == STAND_ALONE)
      g_cavgtime_size +=  sizeof(NSDiagCAvgTime);
    else
      g_cavgtime_size +=  sizeof(NCDiagCAvgTime);
  } else {
    NSDL2_MEMORY(NULL, NULL, "Memory debuging is disabled.");
  }

  NSDL2_MEMORY(NULL, NULL, "After g_cavgtime_size = %d, g_ns_diag_cavgtime_idx = %d",
                                          g_cavgtime_size, g_ns_diag_cavgtime_idx);
}
  

void kw_set_netstorm_diagnostics (char *buf){
  char keyword[MAX_DATA_LINE_LENGTH];
  int  enable = 0;
  int num;

  NSDL2_MEMORY(NULL, NULL, "Method called");

  if ((num = sscanf(buf, "%s %d", keyword, &enable)) != 2)
  {
    fprintf(stderr, "read_keywords(): Need ONE field after keyword %s\n", keyword);
    exit(-1);
  }
   
  if(enable < 0 || enable > 1) 
  {
    fprintf(stderr, "NETSTORM_DIAGNOSTICS keyword can have only 0 or 1 values\n");
    exit(-1);
  }
 
  if(enable) 
    global_settings->g_enable_ns_diag = 1;
  else
    global_settings->g_enable_ns_diag = 0;
}

inline void fill_ns_diag_cum_gp (cavgtime **cavg)
{
  int g_idx = 0, v_idx = 0;
  cavgtime *c_avg = NULL;

  if(ns_diag_gp_ptr == NULL)
    return;
 
  NSDiag_gp *ns_diag_local_gp_ptr = ns_diag_gp_ptr;

  NSDL2_GDF(NULL, NULL, "Method called, g_ns_diag_cavgtime_idx = %d", g_ns_diag_cavgtime_idx);

  for(v_idx = 0; v_idx < sgrp_used_genrator_entries + 1; v_idx++)
  {
    c_avg = (cavgtime *)cavg[v_idx];
    ns_diag_cavgtime = (NSDiagCAvgTime*)((char*) c_avg + g_ns_diag_cavgtime_idx);
    nc_diag_cavgtime = (NCDiagCAvgTime*)((char*) c_avg + g_ns_diag_cavgtime_idx);
 
    if(v_idx == 0)
    {
      GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                           BYTES_TO_MB(g_c_mem_allocated), ns_diag_local_gp_ptr->p_mem_cum_allocated); g_idx++;
      GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                           BYTES_TO_MB(g_c_mem_freed), ns_diag_local_gp_ptr->p_mem_cum_freed); g_idx++;

      GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                           g_c_alloc_count, ns_diag_local_gp_ptr->p_mem_num_malloced); g_idx++;
      GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                           g_c_free_count, ns_diag_local_gp_ptr->p_mem_num_freed); g_idx++;
      GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                           g_c_shared_mem_alloc_count, ns_diag_local_gp_ptr->p_mem_num_shared_allocated); g_idx++;

      GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                           BYTES_TO_MB(g_c_shared_mem_allocated), ns_diag_local_gp_ptr->p_mem_cum_shared_allocated); g_idx++;
     
      // for child
      if(loader_opcode == STAND_ALONE)
      {
        GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                             BYTES_TO_MB(ns_diag_cavgtime->c_mem_cum_allocated), ns_diag_local_gp_ptr->c_mem_cum_allocated); g_idx++;
        GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                             BYTES_TO_MB(ns_diag_cavgtime->c_mem_cum_freed), ns_diag_local_gp_ptr->c_mem_cum_freed); g_idx++;
        GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                             ns_diag_cavgtime->c_mem_cum_num_malloced , ns_diag_local_gp_ptr->c_mem_num_malloced); g_idx++;
        GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                             ns_diag_cavgtime->c_mem_cum_num_freed, ns_diag_local_gp_ptr->c_mem_num_freed); g_idx++;
        GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                             ns_diag_cavgtime->c_mem_cum_num_shared_allocated ,ns_diag_local_gp_ptr->c_mem_num_shared_allocated); g_idx++;
        GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                             BYTES_TO_MB(ns_diag_cavgtime->c_mem_cum_shared_allocated), ns_diag_local_gp_ptr->c_mem_shared_allocated); g_idx++;
      }
      else
      {
        NSDL2_GDF(NULL, NULL, "v_idx = %d, nc_diag_cavgtime->c_mem_cum_allocated = %d", v_idx, nc_diag_cavgtime->c_mem_cum_allocated);
        GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                             BYTES_TO_MB(nc_diag_cavgtime->c_mem_cum_allocated), ns_diag_local_gp_ptr->c_mem_cum_allocated); g_idx++;
        GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                             BYTES_TO_MB(nc_diag_cavgtime->c_mem_cum_freed), ns_diag_local_gp_ptr->c_mem_cum_freed); g_idx++;
        GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                             nc_diag_cavgtime->c_mem_cum_num_malloced , ns_diag_local_gp_ptr->c_mem_num_malloced); g_idx++;
        GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                             nc_diag_cavgtime->c_mem_cum_num_freed, ns_diag_local_gp_ptr->c_mem_num_freed); g_idx++;
        GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                             nc_diag_cavgtime->c_mem_cum_num_shared_allocated ,ns_diag_local_gp_ptr->c_mem_num_shared_allocated); g_idx++;
        GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                             BYTES_TO_MB(nc_diag_cavgtime->c_mem_cum_shared_allocated), ns_diag_local_gp_ptr->c_mem_shared_allocated); g_idx++;
      }
    }
    else
    {
      NSDL2_GDF(NULL, NULL, "v_idx = %d nc_diag_cavgtime->p_mem_cum_allocated = %d", v_idx, nc_diag_cavgtime->p_mem_cum_allocated);
      GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                           BYTES_TO_MB(nc_diag_cavgtime->p_mem_cum_allocated), ns_diag_local_gp_ptr->p_mem_cum_allocated); g_idx++;
      GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                           BYTES_TO_MB(nc_diag_cavgtime->p_mem_cum_freed), ns_diag_local_gp_ptr->p_mem_cum_freed); g_idx++;

      GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                           nc_diag_cavgtime->p_mem_cum_num_malloced, ns_diag_local_gp_ptr->p_mem_num_malloced); g_idx++;
      GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                           nc_diag_cavgtime->p_mem_cum_num_freed, ns_diag_local_gp_ptr->p_mem_num_freed); g_idx++;
      GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                           nc_diag_cavgtime->p_mem_cum_num_shared_allocated, ns_diag_local_gp_ptr->p_mem_num_shared_allocated); g_idx++;
      GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                           BYTES_TO_MB(nc_diag_cavgtime->p_mem_cum_shared_allocated), ns_diag_local_gp_ptr->p_mem_cum_shared_allocated); g_idx++;
      // for child
      GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                           BYTES_TO_MB(nc_diag_cavgtime->c_mem_cum_allocated), ns_diag_local_gp_ptr->c_mem_cum_allocated); g_idx++;
      GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                           BYTES_TO_MB(nc_diag_cavgtime->c_mem_cum_freed), ns_diag_local_gp_ptr->c_mem_cum_freed); g_idx++;
      GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                           nc_diag_cavgtime->c_mem_cum_num_malloced , ns_diag_local_gp_ptr->c_mem_num_malloced); g_idx++;
      GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                           nc_diag_cavgtime->c_mem_cum_num_freed, ns_diag_local_gp_ptr->c_mem_num_freed); g_idx++;
      GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                           nc_diag_cavgtime->c_mem_cum_num_shared_allocated ,ns_diag_local_gp_ptr->c_mem_num_shared_allocated); g_idx++;
      GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                           BYTES_TO_MB(nc_diag_cavgtime->c_mem_cum_shared_allocated), ns_diag_local_gp_ptr->c_mem_shared_allocated); g_idx++;

    }
    g_idx = 0;
    ns_diag_local_gp_ptr++;
  }
}


inline void fill_ns_diag_gp (avgtime **g_avg) 
{
  int g_idx = 0, v_idx = 0;
  avgtime *avg = NULL;

  if(ns_diag_gp_ptr == NULL)
    return;

  NSDiag_gp *ns_diag_local_gp_ptr = ns_diag_gp_ptr;

  NSDL2_GDF(NULL, NULL, "Method called");

  for(v_idx = 0; v_idx < sgrp_used_genrator_entries + 1; v_idx++)
  { 
    avg = (avgtime *)g_avg[v_idx]; 
    //Filling for Overall in graph
    if(v_idx == 0)
    {
      ns_diag_avgtime = (NSDiagAvgTime*)((char*) avg + g_ns_diag_avgtime_idx);
      GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                         BYTES_TO_MB(g_mem_allocated), ns_diag_local_gp_ptr->p_mem_allocated); g_idx++;
      GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                         BYTES_TO_MB(g_mem_freed), ns_diag_local_gp_ptr->p_mem_freed); g_idx++;
      //child data
      if(loader_opcode == STAND_ALONE)
      {
        GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                             BYTES_TO_MB(ns_diag_avgtime->c_mem_allocated), ns_diag_local_gp_ptr->c_mem_allocated); g_idx++;
        GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                             BYTES_TO_MB (ns_diag_avgtime->c_mem_freed), ns_diag_local_gp_ptr->c_mem_freed); g_idx++;
        GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                             ns_diag_avgtime->t_total_threads ,ns_diag_local_gp_ptr->t_total_threads); g_idx++;
        GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                             ns_diag_avgtime->t_busy_threads ,ns_diag_local_gp_ptr->t_total_busy_threads); g_idx++;
        GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                             ns_diag_avgtime->t_dead_threads ,ns_diag_local_gp_ptr->t_total_dead_threads); g_idx++;
        GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                             BYTES_TO_MB(ns_diag_avgtime->t_total_threads * global_settings->stack_size * 1024), 
                             ns_diag_local_gp_ptr->t_stack_mem_used); g_idx++;
      }
      else
      {
        nc_diag_avgtime = (NCDiagAvgTime*)((char*) avg + g_ns_diag_avgtime_idx);
        NSDL4_GDF(NULL, NULL, "v_idx = %d, nc_diag_avgtime->c_mem_allocated = %d", v_idx, nc_diag_avgtime->c_mem_allocated); 
        GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                             BYTES_TO_MB(nc_diag_avgtime->c_mem_allocated), ns_diag_local_gp_ptr->c_mem_allocated); g_idx++;
        GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                             BYTES_TO_MB (nc_diag_avgtime->c_mem_freed), ns_diag_local_gp_ptr->c_mem_freed); g_idx++;
        GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                             nc_diag_avgtime->t_total_threads ,ns_diag_local_gp_ptr->t_total_threads); g_idx++;
        GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                             nc_diag_avgtime->t_busy_threads ,ns_diag_local_gp_ptr->t_total_busy_threads); g_idx++;
        GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                             nc_diag_avgtime->t_dead_threads ,ns_diag_local_gp_ptr->t_total_dead_threads); g_idx++;
        GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                             BYTES_TO_MB(nc_diag_avgtime->t_total_threads * global_settings->stack_size * 1024), 
                             ns_diag_local_gp_ptr->t_stack_mem_used); g_idx++;
      }     
    }
    else
    {
      nc_diag_avgtime = (NCDiagAvgTime*)((char*) avg + g_ns_diag_avgtime_idx);
      NSDL2_GDF(NULL, NULL, "v_idx = %d nc_diag_avgtime->p_mem_allocated = %d", v_idx, nc_diag_avgtime->p_mem_allocated);

      GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                         BYTES_TO_MB(nc_diag_avgtime->p_mem_allocated), ns_diag_local_gp_ptr->p_mem_allocated); g_idx++;
      GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                         BYTES_TO_MB(nc_diag_avgtime->p_mem_freed), ns_diag_local_gp_ptr->p_mem_freed); g_idx++;

      //child data
      GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                           BYTES_TO_MB(nc_diag_avgtime->c_mem_allocated), ns_diag_local_gp_ptr->c_mem_allocated); g_idx++;
      GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                           BYTES_TO_MB (nc_diag_avgtime->c_mem_freed), ns_diag_local_gp_ptr->c_mem_freed); g_idx++;
      GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                           nc_diag_avgtime->t_total_threads ,ns_diag_local_gp_ptr->t_total_threads); g_idx++;
      GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                           nc_diag_avgtime->t_busy_threads ,ns_diag_local_gp_ptr->t_total_busy_threads); g_idx++;
      GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                           nc_diag_avgtime->t_dead_threads ,ns_diag_local_gp_ptr->t_total_dead_threads); g_idx++;
      GDF_COPY_VECTOR_DATA(ns_diag_gp_idx, g_idx, v_idx, 0,
                           BYTES_TO_MB(nc_diag_avgtime->t_total_threads * global_settings->stack_size * 1024), 
                                     ns_diag_local_gp_ptr->t_stack_mem_used); g_idx++;
    }
    g_idx = 0;
    ns_diag_local_gp_ptr++;
  }
}
