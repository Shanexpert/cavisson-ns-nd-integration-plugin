/**********************************************************************
 * File Name            : ns_dns_reporting.c
 * Author(s)            : Naveen Raina
 * Date                 : 23 Aug 2013
 * Copyright            : (c) Cavisson Systems
 * Purpose              : Parsing & Reporting DNS Lookup Stats
 *
 * Modification History :
 *              <Author(s)>, <Date>, <Change Description/Location>
**********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include "ns_dns_reporting.h"
#include "util.h"
#include "nslb_util.h"
#include "netomni/src/core/ni_scenario_distribution.h"
#include "ns_group_data.h"
#include "ns_gdf.h"
#include "ns_exit.h"
#include "ns_trace_level.h"

#define MAX_DNS_KEY_LEN 512 

/*---------------- KEYWORD PARSING SECTION BEGINS------------------------------------------*/

static void ns_dns_usages(char *err)
{
  NSTL1_OUT(NULL, NULL, "Error: Invalid value of ENABLE_DNS_GRAPH keyword: %s\n", err);
  NSTL1_OUT(NULL, NULL, "  Usage: ENABLE_DNS_GRAPH <mode>\n");
  NSTL1_OUT(NULL, NULL, "  This keyword is used to enable or disable the DNS Lookup graphs.\n");
  NSTL1_OUT(NULL, NULL, "    Mode: Mode for enable/disable the DNS Lookup graphs. It can only be 0, 1 or 2\n");
  NSTL1_OUT(NULL, NULL, "      0 - Enable DNS Lookup graphs only if G_USE_DNS is enabled for any group.(default)\n");
  NSTL1_OUT(NULL, NULL, "      1 - Enable DNS Lookup graphs.\n");
  NSTL1_OUT(NULL, NULL, "      2 - Disable DNS Lookup graphs.\n");
  NS_EXIT(-1, "%s\nUsage: ENABLE_DNS_GRAPH <mode>", err);
}

int kw_set_enable_dns_lookup_graphs(char *buf) 
{
  char keyword[MAX_DNS_KEY_LEN + 1];
  char mode_str[32 + 1];
  char tmp[MAX_DNS_KEY_LEN + 1]; //This used to check if some extra field is given
  int num;
  int mode = 0;
  
  num = sscanf(buf, "%s %s %s", keyword, mode_str, tmp);

  NSDL2_REPORTING(NULL, NULL, "Method called, buf = %s, num= %d , key=[%s], mode_str=[%s]", buf, num, keyword, mode_str);

  if(num > 2)
  {
    ns_dns_usages("Number of argument exceeded one");
  }

  if(ns_is_numeric(mode_str) == 0)
  {
    ns_dns_usages("ENABLE_DNS_GRAPH mode is not numeric");
  }

  mode = atoi(mode_str);
  if(mode < 0 || mode > 2)
  {
    ns_dns_usages("ENABLE_DNS_GRAPH mode is not valid");
  }

  global_settings->enable_dns_graph_mode = mode;

  NSDL2_REPORTING(NULL, NULL, "global_settings->enable_dns_graph_mode = %hd", global_settings->enable_dns_graph_mode);

  return 0;
}


void dns_lookup_stats_init()
{
  int i;

  NSDL2_REPORTING(NULL, NULL,"Method called. total_runprof_entries = %d", total_runprof_entries);

  for (i=0; i < total_runprof_entries; i++)
  {
    if(runProfTable[i].gset.use_dns == 0 && global_settings->enable_dns_graph_mode == 0)
    {
      global_settings->enable_dns_graph_mode = 2;

      NSDL2_REPORTING(NULL, NULL,"Setting the mode as 2 as it will not run when ENABLE_DNS_GRAPH mode is zero and G_USE_DNS mode is zero."
							" global_settings->enable_dns_graph_mode = %hd", global_settings->enable_dns_graph_mode);

      break;
    }
  }
}

//Check if dns cache is enabled for any group then set in global setting.
inline void dns_lookup_cache_init()
{
  int i;

  NSDL2_DNS(NULL, NULL,"Method called. total_runprof_entries = %d", total_runprof_entries);

  for (i=0; i < total_runprof_entries; i++)
  {
    if((runProfTable[i].gset.use_dns == 1 || runProfTable[i].gset.use_dns == 2 ) && (runProfTable[i].gset.dns_caching_mode == 0 || (runProfTable[i].gset.dns_caching_mode == 2)))
    {
      global_settings->protocol_enabled |= DNS_CACHE_ENABLED;

      NSDL2_DNS(NULL, NULL,"dns cache mode is enabled for group index %d, setting DNS_CACHE_ENABLED flag in global_settings->protocol_enabled ", i);
      break;
    }
  }
}

/*---------------- KEYWORD PARSING SECTION ENDS ------------------------------------------*/


/*---------------- GDF PARSING SECTION BEGINS ------------------------------------------*/

extern void fprint2f(FILE *fp1, FILE* fp2,char* format, ...);


/* Http DNS Lookup Graph info */

DnsLookupStats_gp *dns_lookup_stats_gp_ptr = NULL;

unsigned int dns_lookup_stats_gp_idx;
#ifndef CAV_MAIN
int dns_lookup_stats_avgtime_idx = -1;
DnsLookupStatsAvgTime *dns_lookup_stats_avgtime = NULL; //used to fill DNS Lookup pointers 
#else
__thread int dns_lookup_stats_avgtime_idx = -1;
__thread DnsLookupStatsAvgTime *dns_lookup_stats_avgtime = NULL; //used to fill DNS Lookup pointers 
#endif
inline void set_dns_lookup_stats_avgtime_ptr() 
{
  NSDL1_GDF(NULL, NULL, "Method Called");

  if(IS_DNS_LOOKUP_STATS_ENABLED) {

    NSDL2_GDF(NULL, NULL, "ENABLE_DNS_GRAPH is enabled. dns_lookup_stats_avgtime_idx = %d", dns_lookup_stats_avgtime_idx);
   /* We have allocated average_time with the size of DnsLookupStatsAvgTime
    * also now we can point that using dns_lookup_stats_avgtime_idx */ 
    dns_lookup_stats_avgtime = (DnsLookupStatsAvgTime*)((char *)average_time + dns_lookup_stats_avgtime_idx);
  } else {
    NSDL1_GDF(NULL, NULL, "ENABLE_DNS_GRAPH keyword is disabled.");
    dns_lookup_stats_avgtime = NULL;
  }

  NSDL1_GDF(NULL, NULL, "dns_lookup_stats_avgtime = %p", dns_lookup_stats_avgtime);
}


// Add size of DNS Lookup if it is enabled 
inline void update_dns_lookup_stats_avgtime_size() 
{
 
  NSDL1_GDF(NULL, NULL, "Method Called, g_avgtime_size = %d, dns_lookup_stats_avgtime_idx = %d", g_avgtime_size, dns_lookup_stats_avgtime_idx);
  
  if(IS_DNS_LOOKUP_STATS_ENABLED){
    NSDL4_GDF(NULL, NULL, "DNS LOOKUP graphs are enabled.");
    dns_lookup_stats_avgtime_idx = g_avgtime_size;
    g_avgtime_size += sizeof(DnsLookupStatsAvgTime);
  } else {
    NSDL4_GDF(NULL, NULL, "DNS LOOKUP graphs are disabled.");
  }

  NSDL4_GDF(NULL, NULL, "After g_avgtime_size = %d, dns_lookup_stats_avgtime_idx = %d",
		  g_avgtime_size, dns_lookup_stats_avgtime_idx);
}

/*----------------------- GDF PARSING SECTION ENDS ---------------------------------*/


#ifdef NS_DEBUG_ON
static char *cache_stats_to_str(DnsLookupStatsAvgTime *dns_lookup_stats_avgtime_ptr, char *debug_buf)
{
  sprintf(debug_buf,"DNS Lookup (per Sec) = %u, DNS Failure (per Sec) = %u, DNS Lookup From Cache (per sec) = %u, Total = %lld, "
                    "DNS Resp Time( Min = %d, Max = %d, Tot = %lld )",
                     dns_lookup_stats_avgtime_ptr->dns_lookup_per_sec, dns_lookup_stats_avgtime_ptr->dns_failure_per_sec,
                     dns_lookup_stats_avgtime_ptr->dns_from_cache_per_sec,
                     dns_lookup_stats_avgtime_ptr->total_dns_lookup_req, dns_lookup_stats_avgtime_ptr->dns_lookup_time_min, 
                     dns_lookup_stats_avgtime_ptr->dns_lookup_time_max,  dns_lookup_stats_avgtime_ptr->dns_lookup_time_total);

  return (debug_buf);
}
#endif

void print_dns_lookup_stats_progress_report(FILE *fp1, FILE *fp2, int is_periodic, avgtime *avg) 
{

  double dns_lookup_per_sec = 0;
  double dns_failure_per_sec = 0;
  double dns_from_cache_per_sec = 0;
  u_ns_8B_t total_dns_lookup_req = 0;

  double dns_lookup_time_max = 0;
  double dns_lookup_time_min = 0;
  double dns_lookup_stats_response_time_avg = 0;

  DnsLookupStatsAvgTime* dns_lookup_stats_avgtime_local = NULL;

  NSDL2_GDF(NULL, NULL, "Method Called. is_periodic = %d", is_periodic);
 
  dns_lookup_stats_avgtime_local = (DnsLookupStatsAvgTime*)((char*)avg + dns_lookup_stats_avgtime_idx);

  // For debug logging only
#ifdef NS_DEBUG_ON
  char debug_buf[4096];
  NSDL2_GDF(NULL, NULL, "Parent: DNS Lookup Stats = %s", cache_stats_to_str(dns_lookup_stats_avgtime_local, debug_buf)); 
#endif

  dns_lookup_per_sec = convert_long_data_to_ps_long_long(dns_lookup_stats_avgtime_local->dns_lookup_per_sec);
  dns_failure_per_sec = convert_long_data_to_ps_long_long(dns_lookup_stats_avgtime_local->dns_failure_per_sec);
  dns_from_cache_per_sec = convert_long_data_to_ps_long_long(dns_lookup_stats_avgtime_local->dns_from_cache_per_sec);

  total_dns_lookup_req = (dns_lookup_stats_avgtime_local->total_dns_lookup_req);

  // time_total has total time in milli-seconds.
  // Min and max are in milli-seconds. 
  dns_lookup_time_min = (dns_lookup_stats_avgtime_local->dns_lookup_time_min == MAX_VALUE_4B_U)?0:dns_lookup_stats_avgtime_local->dns_lookup_time_min;

  dns_lookup_time_max = dns_lookup_stats_avgtime_local->dns_lookup_time_max;

  if(dns_lookup_per_sec)
  {
    dns_lookup_stats_response_time_avg = dns_lookup_stats_avgtime_local->dns_lookup_time_total/dns_lookup_stats_avgtime_local->dns_lookup_per_sec;
  }

  NSDL4_GDF(NULL, NULL, "dns_lookup_stats_response_time_avg = %f", dns_lookup_stats_response_time_avg);

  fprint2f(fp1, fp2, "    Total DNS Lookup (Cum): %lld  DNS Lookup (per sec): %'.3f\n"
                     "    DNS Lookup Failure (per sec): %'.3f\n"
                     "    DNS Lookup From Cache (per sec): %'.3f\n"
                     "    DNS Lookup Time (mSec): min %'.3f  avg %'.3f max %'.3f\n",
	                 total_dns_lookup_req, dns_lookup_per_sec, dns_failure_per_sec, dns_from_cache_per_sec, 
                         dns_lookup_time_min, dns_lookup_stats_response_time_avg, dns_lookup_time_max);

}

inline void fill_dns_lookup_stats_gp (avgtime **g_avg) 
{
  int g_idx = 0, v_idx, grp_idx;
  DnsLookupStatsAvgTime *dns_lookup_stats_avg = NULL;
  Long_data dns_lookup_stats_response_time_avg = 0;
  avgtime *avg = NULL;

  if(dns_lookup_stats_gp_ptr == NULL) 
    return;
  
  DnsLookupStats_gp *dns_lookup_stats_local_gp_ptr = dns_lookup_stats_gp_ptr;

  NSDL2_GDF(NULL, NULL, "Method called");
  
  for(v_idx = 0; v_idx < sgrp_used_genrator_entries + 1; v_idx++)
  {
    avg = (avgtime *)g_avg[v_idx];
    for(grp_idx = 0; grp_idx < TOTAL_GRP_ENTERIES_WITH_GRP_KW; grp_idx++)
    {
      dns_lookup_stats_avg = (DnsLookupStatsAvgTime*)((char*)((char *)avg + (grp_idx * g_avg_size_only_grp)) + dns_lookup_stats_avgtime_idx);
      GDF_COPY_VECTOR_DATA(dns_lookup_stats_gp_idx, g_idx, v_idx, 0, 
                           convert_long_data_to_ps_long_long((dns_lookup_stats_avg->dns_lookup_per_sec)),
                           dns_lookup_stats_local_gp_ptr->dns_lookup_ps); g_idx++;
 
      GDF_COPY_VECTOR_DATA(dns_lookup_stats_gp_idx, g_idx, v_idx, 0,
                           convert_long_data_to_ps_long_long((dns_lookup_stats_avg->dns_failure_per_sec)),
                           dns_lookup_stats_local_gp_ptr->dns_failure_ps); g_idx++;
 
      GDF_COPY_VECTOR_DATA(dns_lookup_stats_gp_idx, g_idx, v_idx, 0,
                           convert_long_data_to_ps_long_long((dns_lookup_stats_avg->dns_from_cache_per_sec)),
                           dns_lookup_stats_local_gp_ptr->dns_from_cache_ps); g_idx++;

      GDF_COPY_VECTOR_DATA(dns_lookup_stats_gp_idx, g_idx, v_idx, 0,
                           dns_lookup_stats_avg->total_dns_lookup_req,
                           dns_lookup_stats_local_gp_ptr->total_dns_lookup_req); g_idx++;

      if(dns_lookup_stats_avg->dns_lookup_per_sec)
      {
        dns_lookup_stats_response_time_avg = (Long_data )dns_lookup_stats_avg->dns_lookup_time_total/((Long_data )dns_lookup_stats_avg->dns_lookup_per_sec);
      }

      dns_lookup_stats_avg->dns_lookup_time_min = (dns_lookup_stats_avg->dns_lookup_time_min);

      GDF_COPY_TIMES_VECTOR_DATA(dns_lookup_stats_gp_idx, g_idx, v_idx, 0, 
                                 dns_lookup_stats_response_time_avg, dns_lookup_stats_avg->dns_lookup_time_min, 
			         dns_lookup_stats_avg->dns_lookup_time_max, dns_lookup_stats_avg->dns_lookup_per_sec,
                                 dns_lookup_stats_local_gp_ptr->dns_lookup_time.avg_time,
                                 dns_lookup_stats_local_gp_ptr->dns_lookup_time.min_time,
                                 dns_lookup_stats_local_gp_ptr->dns_lookup_time.max_time,
                                 dns_lookup_stats_local_gp_ptr->dns_lookup_time.succ); g_idx++;
      g_idx = 0;
      dns_lookup_stats_local_gp_ptr++;
    }
  }
}
