#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "netomni/src/core/ni_scenario_distribution.h"
#include "ns_msg_def.h"
#include "ns_gdf.h"
#include "ns_log.h"
#include "ns_imap.h"
#include "ns_group_data.h"
#include "ns_global_settings.h"
#include "ns_xmpp.h"
static double convert_long_ps(Long_data row_data)
{
  NSDL2_GDF(NULL, NULL, "Method called, row_data = %lu", row_data);
  return((double )((row_data))/((double )global_settings->progress_secs/1000.0));
}
int g_xmpp_cavgtime_idx = -1;
#ifndef CAV_MAIN
int g_xmpp_avgtime_idx = -1;
XMPPAvgTime *xmpp_avgtime = NULL;
#else
__thread int g_xmpp_avgtime_idx = -1;
__thread XMPPAvgTime *xmpp_avgtime = NULL;
#endif
XMPPCAvgTime *xmpp_cavgtime = NULL;

// Called by parent
inline void update_xmpp_avgtime_size() {
  NSDL2_XMPP(NULL, NULL, "Method Called, g_avgtime_size = %d, g_xmpp_avgtime_idx = %d",
                                          g_avgtime_size, g_xmpp_avgtime_idx);

  if((global_settings->protocol_enabled & XMPP_PROTOCOL_ENABLED)) {
    NSDL2_XMPP(NULL, NULL, "XMPP is enabled.");
    g_xmpp_avgtime_idx = g_avgtime_size;
    g_avgtime_size += sizeof(XMPPAvgTime);

  } else {
    NSDL2_XMPP(NULL, NULL, "XMPP is disabled.");
  }

  NSDL2_XMPP(NULL, NULL, "After g_avgtime_size = %d, g_xmpp_avgtime_idx = %d",
                                          g_avgtime_size, g_xmpp_avgtime_idx);
}

// called by parent
inline void update_xmpp_cavgtime_size(){

  NSDL2_XMPP(NULL, NULL, "Method Called, g_XMPPcavgtime_size = %d, g_xmpp_cavgtime_idx = %d",
                                          g_cavgtime_size, g_xmpp_cavgtime_idx);

  if(global_settings->protocol_enabled & XMPP_PROTOCOL_ENABLED) {
    NSDL2_XMPP(NULL, NULL, "XMPP is enabled.");
    g_xmpp_cavgtime_idx = g_cavgtime_size;
    g_cavgtime_size += sizeof(XMPPCAvgTime);
  } else {
    NSDL2_XMPP(NULL, NULL, "XMPP is disabled.");
  }

  NSDL2_XMPP(NULL, NULL, "After g_cavgtime_size = %d, g_xmpp_cavgtime_idx = %d",
                                          g_cavgtime_size, g_xmpp_cavgtime_idx);
}

// Called by child
inline void set_xmpp_avgtime_ptr() {

  NSDL2_XMPP(NULL, NULL, "Method Called");

  if((global_settings->protocol_enabled & XMPP_PROTOCOL_ENABLED)) {
    NSDL2_XMPP(NULL, NULL, "XMPP is enabled.");
   /* We have allocated average_time with the size of XMPPAvgTime
    * also now we can point that using g_xmpp_avgtime_idx*/
    xmpp_avgtime = (XMPPAvgTime*)((char *)average_time + g_xmpp_avgtime_idx);
  } else {
    NSDL2_XMPP(NULL, NULL, "XMPP is disabled.");
    xmpp_avgtime = NULL;
  }

  NSDL2_XMPP(NULL, NULL, "xmpp_avgtime = %p", xmpp_avgtime);
}


// Function for filling the data in the structure of XMPP_gp 
inline void fill_xmpp_gp (avgtime **g_avg, cavgtime **g_cavg)
{
  if(xmpp_stat_gp_ptr == NULL) return;

  int gv_idx, grp_idx, g_idx = 0;

  XMPPAvgTime *xmpp_avg = NULL;
  XMPPCAvgTime *xmpp_cavg = NULL;
  avgtime *avg = NULL;
  cavgtime *cavg = NULL;

  XMPP_gp *loc_xmpp_stat_gp_ptr = xmpp_stat_gp_ptr;
  int throughput = 0;

  NSDL2_GDF(NULL, NULL, "Method called, loc_xmpp_stat_gp_ptr = %p, xmpp_stat_gp_ptr = %p", loc_xmpp_stat_gp_ptr, xmpp_stat_gp_ptr);

  for(gv_idx = 0; gv_idx < sgrp_used_genrator_entries + 1; gv_idx++)
  {
    avg = (avgtime *)g_avg[gv_idx];
    cavg = (cavgtime *)g_cavg[gv_idx];
    for(grp_idx = 0; grp_idx < TOTAL_GRP_ENTERIES_WITH_GRP_KW; grp_idx++)
    {
      /* bug id 101742* using method convert_ps */
      xmpp_avg = (XMPPAvgTime*)((char*)((char *)avg + (grp_idx * g_avg_size_only_grp)) + g_xmpp_avgtime_idx);
      xmpp_cavg = (XMPPCAvgTime *)((char*)((char *)cavg + (grp_idx * g_cavg_size_only_grp)) + g_xmpp_cavgtime_idx);

      NSDL2_GDF(NULL, NULL, "Before filling : loc_xmpp_stat_gp_ptr = %p, avg = %p, xmpp_avg = %p, xmpp_cavg = %p, grp_idx = %d", 
                               loc_xmpp_stat_gp_ptr, avg, xmpp_avg, xmpp_cavg, grp_idx);

      NSDL2_GDF(NULL, NULL, "g_idx = %d, xmpp_login_attempted = %d", g_idx, xmpp_avg->xmpp_login_attempted); 
 
      //Login Attempted per second
      GDF_COPY_VECTOR_DATA(xmpp_stat_gp_idx, g_idx, gv_idx, 0, 
                            convert_long_ps(xmpp_avg->xmpp_login_attempted), loc_xmpp_stat_gp_ptr->login_attempted); 
      NSDL2_GDF(NULL, NULL, "XMPP Login Attempted/Sec xmpp_avg->xmpp_login_attempted %lu,", xmpp_avg->xmpp_login_attempted);
      NSDL2_GDF(NULL, NULL, "XMPP Login Attempted/Sec after ps xmpp_avg->xmpp_login_attempted  %lu,", convert_long_ps(xmpp_avg->xmpp_login_attempted));
      g_idx++;

      //Total Login Attempted 
      NSDL2_GDF(NULL, NULL, "Cum: g_idx = %d, xmpp_c_login_attempted = %d", g_idx, xmpp_cavg->xmpp_c_login_attempted); 
      GDF_COPY_VECTOR_DATA(xmpp_stat_gp_idx, g_idx, gv_idx, 0,
                            xmpp_cavg->xmpp_c_login_attempted, loc_xmpp_stat_gp_ptr->login_attempted_count);
      g_idx++;

      NSDL2_GDF(NULL, NULL, "g_idx = %d, xmpp_login_succ = %d", g_idx, xmpp_avg->xmpp_login_succ); 
      //Login success per second
      GDF_COPY_VECTOR_DATA(xmpp_stat_gp_idx, g_idx, gv_idx, 0, 
                            convert_long_ps(xmpp_avg->xmpp_login_succ), loc_xmpp_stat_gp_ptr->login_succ); 
      g_idx++;
      NSDL2_GDF(NULL, NULL, "XMPP Login Succeed/Sec xmpp_avg->xmpp_login_succ %lu,", xmpp_avg->xmpp_login_succ);
      NSDL2_GDF(NULL, NULL, "XMPP Login Succeed/Sec after ps xmpp_avg->xmpp_login_succ  %lu,", convert_long_ps(xmpp_avg->xmpp_login_succ));

      //Total Login Success 
      NSDL2_GDF(NULL, NULL, "Cum: g_idx = %d, xmpp_c_login_succ = %d", g_idx, xmpp_cavg->xmpp_c_login_succ); 
      GDF_COPY_VECTOR_DATA(xmpp_stat_gp_idx, g_idx, gv_idx, 0,
                            xmpp_cavg->xmpp_c_login_succ, loc_xmpp_stat_gp_ptr->login_succ_count);
      g_idx++;

      //Login failed per second
      GDF_COPY_VECTOR_DATA(xmpp_stat_gp_idx, g_idx, gv_idx, 0, 
                            convert_long_ps(xmpp_avg->xmpp_login_failed), loc_xmpp_stat_gp_ptr->login_failed);
      g_idx++;
      NSDL2_GDF(NULL, NULL, "XMPP Login Failed/Sec xmpp_avg->xmpp_login_failed %lu,", xmpp_avg->xmpp_login_failed);
      NSDL2_GDF(NULL, NULL, "XMPP Login Failed/Sec after ps xmpp_avg->xmpp_login_failed  %lu,", convert_long_ps(xmpp_avg->xmpp_login_failed));
    
      //Total Login Failed 
      GDF_COPY_VECTOR_DATA(xmpp_stat_gp_idx, g_idx, gv_idx, 0,
                            xmpp_cavg->xmpp_c_login_failed, loc_xmpp_stat_gp_ptr->login_failed_count);
      g_idx++;

      //Message sent per second
      NSDL2_GDF(NULL, NULL, ": g_idx = %d, xmpp_msg_sent = %d", g_idx, xmpp_avg->xmpp_msg_sent); 
      GDF_COPY_VECTOR_DATA(xmpp_stat_gp_idx, g_idx, gv_idx, 0, 
                            convert_long_ps(xmpp_avg->xmpp_msg_sent), loc_xmpp_stat_gp_ptr->msg_sent);
      g_idx++;
      NSDL2_GDF(NULL, NULL, "XMPP Message Send /Sec xmpp_avg->xmpp_msg_sent %lu,", xmpp_avg->xmpp_msg_sent);
      NSDL2_GDF(NULL, NULL, "XMPP Message Send /Sec after ps xmpp_avg->xmpp_msg_sent  %lu,", convert_long_ps(xmpp_avg->xmpp_msg_sent));
      //Total Message Sent
      NSDL2_GDF(NULL, NULL, "Cum: g_idx = %d, xmpp_cavg->xmpp_c_msg_sent = %d", g_idx, xmpp_cavg->xmpp_c_msg_sent); 
      GDF_COPY_VECTOR_DATA(xmpp_stat_gp_idx, g_idx, gv_idx, 0,
                            xmpp_cavg->xmpp_c_msg_sent, loc_xmpp_stat_gp_ptr->msg_sent_count);
      g_idx++;

      NSDL2_GDF(NULL, NULL, "g_idx = %d, xmpp_msg_send_failed = %d", g_idx, xmpp_avg->xmpp_msg_send_failed); 
      //Message send failed per second
      GDF_COPY_VECTOR_DATA(xmpp_stat_gp_idx, g_idx, gv_idx, 0, 
                            convert_long_ps(xmpp_avg->xmpp_msg_send_failed), loc_xmpp_stat_gp_ptr->msg_send_failed);
      g_idx++;
      NSDL2_GDF(NULL, NULL, "XMPP Message Send Failed/Sec xmpp_avg->xmpp_msg_send_failed %lu,", xmpp_avg->xmpp_msg_send_failed);
      NSDL2_GDF(NULL, NULL, "XMPP Message Send Failed/Sec after ps xmpp_avg->xmpp_msg_send_failed  %lu,", convert_long_ps(xmpp_avg->xmpp_msg_send_failed));

      //Total Message Send Failed
      GDF_COPY_VECTOR_DATA(xmpp_stat_gp_idx, g_idx, gv_idx, 0,
                            xmpp_cavg->xmpp_c_msg_send_failed, loc_xmpp_stat_gp_ptr->msg_send_failed_count);
      g_idx++;

      NSDL2_GDF(NULL, NULL, "g_idx = %d, xmpp_msg_rcvd = %d", g_idx, xmpp_avg->xmpp_msg_rcvd); 
      //Message received per second
      GDF_COPY_VECTOR_DATA(xmpp_stat_gp_idx, g_idx, gv_idx, 0, 
                            convert_long_ps(xmpp_avg->xmpp_msg_rcvd), loc_xmpp_stat_gp_ptr->msg_rcvd);
      g_idx++;
      NSDL2_GDF(NULL, NULL, "XMPP Message Received/Sec xmpp_avg->xmpp_msg_rcvd %lu,", xmpp_avg->xmpp_msg_rcvd);
      NSDL2_GDF(NULL, NULL, "XMPP Message Received/Sec after ps xmpp_avg->xmpp_msg_rcvd  %lu,", convert_long_ps(xmpp_avg->xmpp_msg_rcvd));

      //Total Message Received 
      GDF_COPY_VECTOR_DATA(xmpp_stat_gp_idx, g_idx, gv_idx, 0,
                            xmpp_cavg->xmpp_c_msg_rcvd, loc_xmpp_stat_gp_ptr->msg_rcvd_count);
      g_idx++;

      NSDL2_GDF(NULL, NULL, "g_idx = %d, xmpp_msg_dlvrd = %d", g_idx, xmpp_avg->xmpp_msg_dlvrd); 
      //Message delivered per second
      GDF_COPY_VECTOR_DATA(xmpp_stat_gp_idx, g_idx, gv_idx, 0, 
                            convert_long_ps(xmpp_avg->xmpp_msg_dlvrd), loc_xmpp_stat_gp_ptr->msg_dlvrd);
      g_idx++;
      NSDL2_GDF(NULL, NULL, "XMPP Message Delivered/Sec xmpp_avg->xmpp_msg_dlvrd %lu,", xmpp_avg->xmpp_msg_dlvrd);
      NSDL2_GDF(NULL, NULL, "XMPP Message Delivered/Sec after ps xmpp_avg->xmpp_msg_dlvrd  %lu,", convert_long_ps(xmpp_avg->xmpp_msg_dlvrd));

      //Total Message Delivered 
      GDF_COPY_VECTOR_DATA(xmpp_stat_gp_idx, g_idx, gv_idx, 0,
                            xmpp_cavg->xmpp_c_msg_dlvrd, loc_xmpp_stat_gp_ptr->msg_dlvrd_count);
      g_idx++;

      //Send Throughput 
      /*bug id 101742: using method convert_kbps() */
      throughput = (xmpp_avg->xmpp_send_bytes)*8/((double)global_settings->progress_secs/1000);  
      NSDL2_GDF(NULL, NULL, "Send : g_idx = %d, throughput = %d", g_idx, throughput);       
      GDF_COPY_VECTOR_DATA(xmpp_stat_gp_idx, g_idx, gv_idx, 0, 
                            convert_kbps(throughput), loc_xmpp_stat_gp_ptr->send_throughput);
      g_idx++;
      NSDL2_GDF(NULL, NULL, " XMPP Send Throughput = %f ", throughput);
      NSDL2_GDF(NULL, NULL, " XMPP Send Throughput after kbps result = %f ", convert_kbps(throughput));

      //Recieved Throughput
      throughput = (xmpp_avg->xmpp_rcvd_bytes)*8/((double)global_settings->progress_secs/1000);  
      NSDL2_GDF(NULL, NULL, "Received : g_idx = %d, throughput = %d", g_idx, throughput); 
      GDF_COPY_VECTOR_DATA(xmpp_stat_gp_idx, g_idx, gv_idx, 0, 
                            convert_kbps(throughput), loc_xmpp_stat_gp_ptr->rcvd_throughput);
      g_idx = 0;
      NSDL2_GDF(NULL, NULL, " XMPP Receive Throughput = %f ", throughput);
      NSDL2_GDF(NULL, NULL, " XMPP Receive Throughput after kbps result = %f ", convert_kbps(throughput));

      loc_xmpp_stat_gp_ptr++;
    }
  }
}

