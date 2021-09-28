/********************************************************************************
 * File Name            : ns_page_based_stats.c 
 * Author(s)            : Shikha
 * Date                 : 26 December 2015
 * Copyright            : (c) Cavisson Systems
 * Purpose              : Contains function related to show page based stat in graphs,
 *                        Parsing of Keyword and gdf functionality.
 * Modification History : <Author(s)>, <Date>, <Change Description/Location>
 ********************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/times.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>

#include "nslb_util.h"
#include "ns_error_codes.h"
#include "util.h"
#include "timing.h"
#include "tmr.h"
#include "logging.h"

#include "ns_schedule_phases.h"
#include "netomni/src/core/ni_scenario_distribution.h"
#include "netstorm.h"
#include "runlogic.h"
#include "ns_gdf.h"
#include "ns_log.h"
#include "ns_user_monitor.h"
#include "ns_alloc.h"
#include "ns_percentile.h"
#include "ns_parse_scen_conf.h"
#include "ns_msg_com_util.h"
#include "ns_child_msg_com.h"
#include "ns_script_parse.h"
#include "ns_vuser.h"
#include "ns_page_based_stats.h"
#include "ns_group_data.h"
#include "ns_exit.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"

Page_based_stat_gp *page_based_stat_gp_ptr;
#ifndef CAV_MAIN
unsigned int page_based_stat_gp_idx = -1;
PageStatAvgTime *page_stat_avgtime;
#else
__thread unsigned int page_based_stat_gp_idx = -1;
__thread PageStatAvgTime *page_stat_avgtime;
#endif
unsigned int page_based_stat_idx = -1;

//FILE *write_gdf_fp = NULL;

// Write vectors in the GDF and also save in two D array in following format
//  ScenarioGroup>PageName (e.g. G1>Login)
void printPageBasedStatGraph(char **TwoD , int *Idx2d, char *prefix)
{
  int i = 0, j = 0;
  char buff[1024];
  int write_idx = 0;
  NSDL2_GDF(NULL, NULL, " Method called.");

  for(j = 0; j < total_runprof_entries; j++)
  {
    write_idx = snprintf(buff, 1024, "%s%s>", prefix, RETRIEVE_BUFFER_DATA(runProfTable[j].scen_group_name));
    for(i = 0; i < runProfTable[j].num_pages; i++)
    {
      snprintf(buff + write_idx, 1024 - write_idx, "%s",
                                 RETRIEVE_BUFFER_DATA(gPageTable[gSessionTable[runProfTable[j].sessprof_idx].first_page + i].page_name));
      NSDL2_MISC(NULL, NULL, "buff = %s", buff);
      fprintf(write_gdf_fp, "%s\n", buff);
      fill_2d(TwoD, *Idx2d, buff);
      *Idx2d = *Idx2d + 1;
    }
  }
}

char **printPageBasedStat()
{
  int i = 0;
  char **TwoD;
  char prefix[1024];
  TwoD = init_2d(g_actual_num_pages * (sgrp_used_genrator_entries + 1));
  int Idx2d = 0;
    
  NSDL2_MISC(NULL, NULL, "Method Called, total_sess_entries = %d, total_runprof_entries = %d, g_actual_num_pages = %d, "
                         "total_page_entries = %d",
                          total_sess_entries, total_runprof_entries, g_actual_num_pages, total_page_entries);
    
  for(i=0; i < sgrp_used_genrator_entries + 1; i++)
  {   
    getNCPrefix(prefix, i-1, -1, ">", 0);
    printPageBasedStatGraph(TwoD, &Idx2d, prefix);
  }   
  return TwoD;
}

// Called by ns_parent.c to update page stat size into g_avgtime_size
inline void update_page_based_stat_avgtime_size() 
{
  NSDL1_MISC(NULL, NULL, "Method Called, page_based_stat = %d, g_avgtime_size = %d, page_based_stat_gp_idx = %d, g_actual_num_pages = %d",
                          global_settings->page_based_stat, g_avgtime_size, page_based_stat_gp_idx, g_actual_num_pages);
  
  if(global_settings->page_based_stat == PAGE_BASED_STAT_ENABLED) 
  {
    NSDL2_MISC(NULL, NULL, "PAGE BASED STAT is enabled.");
    page_based_stat_gp_idx = g_avgtime_size;
    g_avgtime_size += PAGE_STAT_AVGTIME_SIZE;
  } else {
    NSDL2_MISC(NULL, NULL, "PAGE BASED STAT is disabled.");
  }
  
  NSDL2_MISC(NULL, NULL, "After g_avgtime_size = %d, page_based_stat_gp_idx, = %d",
                  g_avgtime_size, page_based_stat_gp_idx); 
}

// Called by child
inline void set_page_based_stat_avgtime_ptr() 
{
  NSDL1_MISC(NULL, NULL, "Method Called, page_based_stat = %d, page_based_stat_gp_idx = %d", 
                          global_settings->page_based_stat, page_based_stat_gp_idx);

  if(global_settings->page_based_stat == PAGE_BASED_STAT_ENABLED) 
  {
    page_stat_avgtime = (PageStatAvgTime*)((char *)average_time + page_based_stat_gp_idx);
  } else {
    page_stat_avgtime = NULL;
  }

  NSDL2_MISC(NULL, NULL, "page_stat_avgtime set at address = %p", page_stat_avgtime);
}


//keyword parsing
int kw_set_page_based_stat(char *buf, char *err_msg, int runtime_flag)
{
  char keyword[MAX_DATA_LINE_LENGTH];
  char mode_str[32 + 1];
  char tmp[MAX_DATA_LINE_LENGTH]; //This used to check if some extra field is given
  int num;
  int mode = 0;

  num = sscanf(buf, "%s %s %s", keyword, mode_str, tmp);

  NSDL2_PARSING(NULL, NULL, "Method called, buf = %s, num= %d , key=[%s], mode_str=[%s]", buf, num, keyword, mode_str);

  if(num != 2)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, ENABLE_PAGE_BASED_STATS_USAGE, CAV_ERR_1011063, CAV_ERR_MSG_1);
  }

  if(ns_is_numeric(mode_str) == 0)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, ENABLE_PAGE_BASED_STATS_USAGE, CAV_ERR_1011063, CAV_ERR_MSG_2);
  }

  mode = atoi(mode_str);
  if(mode < 0 || mode > 1)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, ENABLE_PAGE_BASED_STATS_USAGE, CAV_ERR_1011063, CAV_ERR_MSG_3);
  }

  global_settings->page_based_stat = mode;

  NSDL2_PARSING(NULL, NULL, "global_settings->page_based_stat = %d", global_settings->page_based_stat);

  return 0;
}

//Set page_start and relative_page_id
inline void init_page_based_stat()
{
  NSDL2_PARSING(NULL, NULL, "Method Called, total_runprof_entries = %d, total_sess_entries = %d", total_runprof_entries, total_sess_entries);

  //Set page_start in RunProfTable
  set_runproftable_start_page_idx();

  //Set Relative Page Id in PageTable
  set_gpagetable_relative_page_idx();
}

inline void initialise_page_based_stat_min(avgtime *msg)
{
  int i = 0;
  if(global_settings->page_based_stat == PAGE_BASED_STAT_ENABLED)
  {
    PageStatAvgTime *page_stat_msg = NULL;
    page_stat_msg = ((PageStatAvgTime*)((char*)msg + page_based_stat_gp_idx));
    NSDL2_PARENT(NULL, NULL, "We are initialising min and max value for Page stats g_actual_num_pages = %d, "
                              "page_stat_msg = %p, page_based_stat_gp_idx = %d",
                               g_actual_num_pages, page_stat_msg, page_based_stat_gp_idx);
    memset(page_stat_msg, 0, PAGE_STAT_AVGTIME_SIZE);
    for (i = 0; i < g_actual_num_pages; i++)
    {
      page_stat_msg[i].page_think_min_time = MAX_VALUE_4B_U;
      page_stat_msg[i].inline_delay_min_time = MAX_VALUE_4B_U;
      page_stat_msg[i].block_min_time = MAX_VALUE_4B_U;
      page_stat_msg[i].conn_reuse_delay_min_time = MAX_VALUE_4B_U;
    }
  }
}

// Function for filling the data in the structure of PageStatAvgTime.
inline void fill_page_based_stat_gp(avgtime **g_avg)
{
  int i, v_idx, k = 0;
  if(page_based_stat_gp_ptr== NULL) return;
  PageStatAvgTime *page_based_stat_avgtime = NULL;
  avgtime *avg = NULL;
  Page_based_stat_gp *page_based_stat_local_gp_ptr = page_based_stat_gp_ptr;
 
  NSDL1_GDF(NULL, NULL, "Method called, page_based_stat_idx = %d, g_actual_num_pages = %d", page_based_stat_idx, g_actual_num_pages);
 
  for(v_idx = 0; v_idx < sgrp_used_genrator_entries + 1; v_idx++)
  {
    avg = (avgtime *)g_avg[v_idx]; 
    page_based_stat_avgtime = (PageStatAvgTime *) ((char*) avg + page_based_stat_gp_idx);
    for (i = 0; i < g_actual_num_pages ; i++, k++) 
    {
      NSDL1_GDF(NULL, NULL, "Before : Page idx = %d, page_based_stat_idx = %d, page_think_time = %d, page_think_counts = %d," 
                "page_think_min_time = %d, page_think_max_time = %d, page_think_time_gp.avg_time = %f,"
                "page_think_time_gp.min_time = %f, page_think_time_gp.max_time = %f, page_think_time_gp.succ = %f,"
                "inline_delay_time = %d, inline_delay_counts = %d, inline_delay_min_time = %d, inline_delay_max_time = %d" 
                "inline_delay_time_gp.avg_time = %d, inline_delay_time_gp.min_time = %f, inline_delay_time_gp.max_time = %f,"
          	"inline_delay_time_gp.succ = %f, block_time = %d, block_counts = %d, block_min_time = %d, block_max_time = %d",
                 i, page_based_stat_idx, page_based_stat_avgtime[i].page_think_time, page_based_stat_avgtime[i].page_think_counts, 
                 page_based_stat_avgtime[i].page_think_min_time, page_based_stat_avgtime[i].page_think_max_time, 
                 page_based_stat_local_gp_ptr[k].page_think_time_gp.avg_time, page_based_stat_local_gp_ptr[k].page_think_time_gp.min_time, 
                 page_based_stat_local_gp_ptr[k].page_think_time_gp.max_time, page_based_stat_local_gp_ptr[k].page_think_time_gp.succ,
                 page_based_stat_avgtime[i].inline_delay_time, page_based_stat_avgtime[i].inline_delay_counts, 
                 page_based_stat_avgtime[i].inline_delay_min_time, 
                 page_based_stat_avgtime[i].inline_delay_max_time, page_based_stat_local_gp_ptr[k].inline_delay_time_gp.avg_time, 
                 page_based_stat_local_gp_ptr[k].inline_delay_time_gp.min_time, page_based_stat_local_gp_ptr[k].inline_delay_time_gp.max_time,
                 page_based_stat_local_gp_ptr[k].inline_delay_time_gp.succ,
                 page_based_stat_avgtime[i].block_time, page_based_stat_avgtime[i].block_counts, 
                 page_based_stat_avgtime[i].block_min_time,               
                 page_based_stat_avgtime[i].block_max_time);
 
 
      if(page_based_stat_avgtime[i].page_think_counts > 0)
      {
        GDF_COPY_TIMES_VECTOR_DATA(page_based_stat_idx, 0, k, 0,
                      (double)(((double)page_based_stat_avgtime[i].page_think_time)/
                               ((double)(1000.0*(double)page_based_stat_avgtime[i].page_think_counts))),
                      (double)page_based_stat_avgtime[i].page_think_min_time/1000.0,
                      (double)page_based_stat_avgtime[i].page_think_max_time/1000.0,
                      page_based_stat_avgtime[i].page_think_counts,
                      page_based_stat_local_gp_ptr[k].page_think_time_gp.avg_time,
                      page_based_stat_local_gp_ptr[k].page_think_time_gp.min_time,
                      page_based_stat_local_gp_ptr[k].page_think_time_gp.max_time,
                      page_based_stat_local_gp_ptr[k].page_think_time_gp.succ);
      }
      else
      {
        GDF_COPY_TIMES_VECTOR_DATA(page_based_stat_idx, 0, k, 0,
                      0,
                      -1,
                      0,
                      0,
                      page_based_stat_local_gp_ptr[k].page_think_time_gp.avg_time,
                      page_based_stat_local_gp_ptr[k].page_think_time_gp.min_time,
                      page_based_stat_local_gp_ptr[k].page_think_time_gp.max_time,
                      page_based_stat_local_gp_ptr[k].page_think_time_gp.succ);
      }
 
      if(page_based_stat_avgtime[i].inline_delay_counts > 0)
      {
        GDF_COPY_TIMES_VECTOR_DATA(page_based_stat_idx, 1, k, 0,
                      (double)(((double)page_based_stat_avgtime[i].inline_delay_time)/
                               ((double)(1000.0*(double)page_based_stat_avgtime[i].inline_delay_counts))),
                      (double)page_based_stat_avgtime[i].inline_delay_min_time/1000.0,
                      (double)page_based_stat_avgtime[i].inline_delay_max_time/1000.0,
                      page_based_stat_avgtime[i].inline_delay_counts,
                      page_based_stat_local_gp_ptr[k].inline_delay_time_gp.avg_time,
                      page_based_stat_local_gp_ptr[k].inline_delay_time_gp.min_time,
                      page_based_stat_local_gp_ptr[k].inline_delay_time_gp.max_time,
                      page_based_stat_local_gp_ptr[k].inline_delay_time_gp.succ);
      }
      else
      {
        GDF_COPY_TIMES_VECTOR_DATA(page_based_stat_idx, 1, k, 0,
                      0,
                      -1,
                      0,
                      0,
                      page_based_stat_local_gp_ptr[k].inline_delay_time_gp.avg_time,
                      page_based_stat_local_gp_ptr[k].inline_delay_time_gp.min_time,
                      page_based_stat_local_gp_ptr[k].inline_delay_time_gp.max_time,
                      page_based_stat_local_gp_ptr[k].inline_delay_time_gp.succ);
      }
 
 
      if(page_based_stat_avgtime[i].block_counts > 0)
      {
        GDF_COPY_TIMES_VECTOR_DATA(page_based_stat_idx, 2, k, 0,
                      (double)(((double)page_based_stat_avgtime[i].block_time)/
                               ((double)(1000.0*(double)page_based_stat_avgtime[i].block_counts))),
                      (double)page_based_stat_avgtime[i].block_min_time/1000.0,
                      (double)page_based_stat_avgtime[i].block_max_time/1000.0,
                      page_based_stat_avgtime[i].block_counts,
                      page_based_stat_local_gp_ptr[k].block_time_gp.avg_time,
                      page_based_stat_local_gp_ptr[k].block_time_gp.min_time,
                      page_based_stat_local_gp_ptr[k].block_time_gp.max_time,
                      page_based_stat_local_gp_ptr[k].block_time_gp.succ);
      }
      else
      {
        GDF_COPY_TIMES_VECTOR_DATA(page_based_stat_idx, 2, k, 0,
                      0,
                      -1,
                      0,
                      0,
                      page_based_stat_local_gp_ptr[k].block_time_gp.avg_time,
                      page_based_stat_local_gp_ptr[k].block_time_gp.min_time,
                      page_based_stat_local_gp_ptr[k].block_time_gp.max_time,
                      page_based_stat_local_gp_ptr[k].block_time_gp.succ);
      }
 
      if(page_based_stat_avgtime[i].conn_reuse_delay_counts > 0)
      {
        GDF_COPY_TIMES_VECTOR_DATA(page_based_stat_idx, 3, k, 0,
                      (double)(((double)page_based_stat_avgtime[i].conn_reuse_delay_time)/
                               ((double)(1000.0*(double)page_based_stat_avgtime[i].conn_reuse_delay_counts))),
                      (double)page_based_stat_avgtime[i].conn_reuse_delay_min_time/1000.0,
                      (double)page_based_stat_avgtime[i].conn_reuse_delay_max_time/1000.0,
                      page_based_stat_avgtime[i].conn_reuse_delay_counts,
                      page_based_stat_local_gp_ptr[k].conn_reuse_delay_time_gp.avg_time,
                      page_based_stat_local_gp_ptr[k].conn_reuse_delay_time_gp.min_time,
                      page_based_stat_local_gp_ptr[k].conn_reuse_delay_time_gp.max_time,
                      page_based_stat_local_gp_ptr[k].conn_reuse_delay_time_gp.succ);
      }
      else
      {
        GDF_COPY_TIMES_VECTOR_DATA(page_based_stat_idx, 3, k, 0,
                      0,
                      -1,
                      0,
                      0,
                      page_based_stat_local_gp_ptr[k].conn_reuse_delay_time_gp.avg_time,
                      page_based_stat_local_gp_ptr[k].conn_reuse_delay_time_gp.min_time,
                      page_based_stat_local_gp_ptr[k].conn_reuse_delay_time_gp.max_time,
                      page_based_stat_local_gp_ptr[k].conn_reuse_delay_time_gp.succ);
      }
 
      NSDL1_GDF(NULL, NULL, "After : Page idx = %d, page_based_stat_idx = %d, page_think_time = %d, page_think_counts = %d," 
                "page_think_min_time = %d, page_think_max_time = %d, page_think_time_gp.avg_time = %f,"
                "page_think_time_gp.min_time = %f, page_think_time_gp.max_time = %f, page_think_time_gp.succ = %f,"
                "inline_delay_time = %d, inline_delay_counts = %d, inline_delay_min_time = %d, "
                "inline_delay_max_time = %d, inline_delay_time_gp.min_time = %f, inline_delay_time_gp.max_time = %f,"
                "inline_delay_time_gp.succ = %f, block_time = %d, block_counts = %d, block_min_time = %d, block_max_time = %d, "
                "conn_reuse_delay_time = %d, conn_reuse_delay_min_time = %d, conn_reuse_delay_max_time = %d," 
                " conn_reuse_delay_counts = %d",
                i, page_based_stat_idx, page_based_stat_avgtime[i].page_think_time, page_based_stat_avgtime[i].page_think_counts, 
                page_based_stat_avgtime[i].page_think_min_time, page_based_stat_avgtime[i].page_think_max_time, 
                page_based_stat_local_gp_ptr[k].page_think_time_gp.avg_time, page_based_stat_local_gp_ptr[k].page_think_time_gp.min_time, 
                page_based_stat_local_gp_ptr[k].page_think_time_gp.max_time, page_based_stat_local_gp_ptr[k].page_think_time_gp.succ,
                page_based_stat_avgtime[i].inline_delay_time, page_based_stat_avgtime[i].inline_delay_counts, 
                page_based_stat_avgtime[i].inline_delay_min_time, 
                page_based_stat_avgtime[i].inline_delay_max_time, 
                page_based_stat_local_gp_ptr[k].inline_delay_time_gp.min_time, page_based_stat_local_gp_ptr[k].inline_delay_time_gp.max_time,
                page_based_stat_local_gp_ptr[k].inline_delay_time_gp.succ,
                page_based_stat_avgtime[i].block_time, page_based_stat_avgtime[i].block_counts, 
                page_based_stat_avgtime[i].block_min_time,               
                page_based_stat_avgtime[i].block_max_time,  
                page_based_stat_local_gp_ptr[k].conn_reuse_delay_time_gp.avg_time,
                page_based_stat_local_gp_ptr[k].conn_reuse_delay_time_gp.min_time , 
                page_based_stat_local_gp_ptr[k].conn_reuse_delay_time_gp.max_time,
                page_based_stat_local_gp_ptr[k].conn_reuse_delay_time_gp.succ);
    }
  }
}

//Setting Counter for Page think time
void set_page_based_counter_for_page_think_time(void *lvptr, int pg_think_time)
{
  VUser *vptr = (VUser *) lvptr;
  int page_stat_page_idx = -1;

  NSDL1_GDF(NULL, NULL, "Method Called");

  if(page_stat_avgtime == NULL)
  {
    NSDL2_GDF(NULL, NULL, "page_stat_avgtime is null");
    return ;
  }

  if( vptr->cur_page == NULL)
  {
    NSDL1_GDF(vptr, NULL, "We are returning as no page in script, vptr->cur_page = %d ",vptr->cur_page);
    return ;
  }

  NSDL1_GDF(NULL, NULL, "Setting page_stat_page_idx: group_num = %d, start_page_idx = %u, relative_page_idx = %u",
                         vptr->group_num, runprof_table_shr_mem[vptr->group_num].start_page_idx, vptr->cur_page->relative_page_idx);

  page_stat_page_idx = runprof_table_shr_mem[vptr->group_num].start_page_idx + vptr->cur_page->relative_page_idx;

  NSDL1_GDF(NULL, NULL, "Before Setting page think time: page_stat_page_idx = %d, pg_think_time = %d, page_think_min_time = %d,"
                                       " page_think_max_time = %d", 
                                        page_stat_page_idx, pg_think_time, page_stat_avgtime[page_stat_page_idx].page_think_min_time, 
                                        page_stat_avgtime[page_stat_page_idx].page_think_max_time);

  if(pg_think_time < page_stat_avgtime[page_stat_page_idx].page_think_min_time)
    page_stat_avgtime[page_stat_page_idx].page_think_min_time = pg_think_time;
 
  if(pg_think_time > page_stat_avgtime[page_stat_page_idx].page_think_max_time)
    page_stat_avgtime[page_stat_page_idx].page_think_max_time = pg_think_time;
  
  page_stat_avgtime[page_stat_page_idx].page_think_time += pg_think_time;
  page_stat_avgtime[page_stat_page_idx].page_think_counts++;

  NSDL1_GDF(vptr, NULL, "After updating, Counters = %d, page_think_min_time = %d, page_think_max_time = %d", page_stat_avgtime[page_stat_page_idx].page_think_counts, page_stat_avgtime[page_stat_page_idx].page_think_min_time, page_stat_avgtime[page_stat_page_idx].page_think_max_time);
}


//Setting counter for page inline delay time
void set_page_based_counter_for_inline_delay_time(void *lvptr, int inline_req_delay)
{                   
  VUser *vptr = (VUser *) lvptr;
  int page_stat_page_idx = -1;

  NSDL1_GDF(NULL, NULL, "Method Called, vptr = %p, page_stat_avgtime = %p",
                                 vptr, page_stat_avgtime);
  if(page_stat_avgtime == NULL)
  {
    NSDL2_GDF(NULL, NULL, "page_stat_avgtime is null");
    return ;
  }

  NSDL1_GDF(NULL, NULL, "Setting page_stat_page_idx: start_page_idx = %u, relative_page_idx = %u",
                         runprof_table_shr_mem[vptr->group_num].start_page_idx, vptr->cur_page->relative_page_idx);

  page_stat_page_idx = runprof_table_shr_mem[vptr->group_num].start_page_idx + vptr->cur_page->relative_page_idx;

  NSDL1_GDF(NULL, NULL, "Setting Page ID for page_based_stat page_stat_page_idx = %d", page_stat_page_idx);

  if(inline_req_delay < page_stat_avgtime[page_stat_page_idx].inline_delay_min_time)
    page_stat_avgtime[page_stat_page_idx].inline_delay_min_time = inline_req_delay;
  
  if(inline_req_delay > page_stat_avgtime[page_stat_page_idx].inline_delay_max_time)
    page_stat_avgtime[page_stat_page_idx].inline_delay_max_time= inline_req_delay;
  
  page_stat_avgtime[page_stat_page_idx].inline_delay_time += inline_req_delay;
  page_stat_avgtime[page_stat_page_idx].inline_delay_counts++;

    NSDL1_GDF(vptr, NULL, "After updating, Counters = %d, inline_delay_min_time = %d, inline_delay_max_time = %d", page_stat_avgtime[page_stat_page_idx].inline_delay_counts, page_stat_avgtime[page_stat_page_idx].inline_delay_min_time, page_stat_avgtime[page_stat_page_idx].inline_delay_max_time);

}

//Setting counter for block time 
void set_page_based_counter_for_block_time(void *lvptr, int inline_block_time)
{                   
  VUser *vptr = (VUser *) lvptr;
  int page_stat_page_idx = -1;

  NSDL1_GDF(NULL, NULL, "Method Called, vptr = %p, page_stat_avgtime = %p",
                                 vptr, page_stat_avgtime);
  if(page_stat_avgtime == NULL)
  {
    NSDL2_GDF(NULL, NULL, "page_stat_avgtime is null");
    return ;
  }

  NSDL1_GDF(NULL, NULL, "Setting page_stat_page_idx: start_page_idx = %u, relative_page_idx = %u",
                         runprof_table_shr_mem[vptr->group_num].start_page_idx, vptr->cur_page->relative_page_idx);

  page_stat_page_idx = runprof_table_shr_mem[vptr->group_num].start_page_idx + vptr->cur_page->relative_page_idx;

  NSDL1_GDF(NULL, NULL, "Setting Page ID for page_based_stat page_stat_page_idx = %d", page_stat_page_idx);

  if(inline_block_time < page_stat_avgtime[page_stat_page_idx].block_min_time)
    page_stat_avgtime[page_stat_page_idx].block_min_time = inline_block_time;
  
  if(inline_block_time > page_stat_avgtime[page_stat_page_idx].block_max_time)
    page_stat_avgtime[page_stat_page_idx].block_max_time = inline_block_time;
  
  page_stat_avgtime[page_stat_page_idx].block_time += inline_block_time;
  page_stat_avgtime[page_stat_page_idx].block_counts++;
  
  NSDL1_GDF(vptr, NULL, "After updating, Counters = %d, block_min_time = %d, block_max_time = %d", page_stat_avgtime[page_stat_page_idx].block_counts, page_stat_avgtime[page_stat_page_idx].block_min_time, page_stat_avgtime[page_stat_page_idx].block_max_time);

}

//Setting counter for connection reuse delay 
void set_page_based_counter_for_conn_reuse_delay(void *lvptr, int conn_reuse_delay)
{
  VUser *vptr = (VUser *) lvptr;
  int page_stat_page_idx = -1;

  NSDL1_GDF(NULL, NULL, "Method Called, vptr = %p, page_stat_avgtime = %p",
                                 vptr, page_stat_avgtime);
  if(page_stat_avgtime == NULL)
  {
    NSDL2_GDF(NULL, NULL, "page_stat_avgtime is null");
    return ;
  }

  NSDL1_GDF(NULL, NULL, "Setting page_stat_page_idx: start_page_idx = %u, relative_page_idx = %u",
                         runprof_table_shr_mem[vptr->group_num].start_page_idx, vptr->cur_page->relative_page_idx);

  page_stat_page_idx = runprof_table_shr_mem[vptr->group_num].start_page_idx + vptr->cur_page->relative_page_idx;

  NSDL1_GDF(NULL, NULL, "Setting Page ID for page_based_stat page_stat_page_idx = %d", page_stat_page_idx);

  if(conn_reuse_delay < page_stat_avgtime[page_stat_page_idx].conn_reuse_delay_min_time)
    page_stat_avgtime[page_stat_page_idx].conn_reuse_delay_min_time = conn_reuse_delay;

  if(conn_reuse_delay > page_stat_avgtime[page_stat_page_idx].conn_reuse_delay_max_time)
    page_stat_avgtime[page_stat_page_idx].conn_reuse_delay_max_time = conn_reuse_delay;

  page_stat_avgtime[page_stat_page_idx].conn_reuse_delay_time += conn_reuse_delay;
  page_stat_avgtime[page_stat_page_idx].conn_reuse_delay_counts++;

  NSDL1_GDF(vptr, NULL, "After updating, Counters = %d, conn_reuse_delay_min_time = %d, conn_reuse_delay_max_time = %d", page_stat_avgtime[page_stat_page_idx].conn_reuse_delay_counts, page_stat_avgtime[page_stat_page_idx].conn_reuse_delay_min_time, page_stat_avgtime[page_stat_page_idx].conn_reuse_delay_max_time);

}

