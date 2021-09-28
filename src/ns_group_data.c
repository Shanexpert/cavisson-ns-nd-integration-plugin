/********************************************************************************
 * File Name            : ns_group_data.c
 * Author(s)            : Manpreet Kaur, Jagat Singh
 * Date                 : 4 August 2014
 * Copyright            : (c) Cavisson Systems
 * Purpose              : Contains function related to show group based data graphs,
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
#include "ns_group_data.h"
#include "ns_connection_pool.h"
#include "netomni/src/core/ni_scenario_distribution.h"
#include "ns_trace_level.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"

GROUPVuser *grp_vuser;
Group_data_gp *group_data_gp_ptr = NULL;
extern int sgrp_used_genrator_entries;
#ifndef CAV_MAIN
GROUPAvgTime *grp_avgtime = NULL;
unsigned int group_data_gp_idx = -1;
#else
__thread unsigned int group_data_gp_idx = -1;
__thread GROUPAvgTime *grp_avgtime = NULL;
#endif
unsigned int group_data_idx = -1;
extern FILE *write_gdf_fp;
extern char g_runtime_flag;
void printGroupGraph(char **TwoD , int *Idx2d, char *prefix)
{
  int i = 0;
  char buff[1024];
  NSDL2_GDF(NULL, NULL, " Method called.");

  for(i=0; i < total_runprof_entries; i++)
  {
    if(g_runtime_flag == 0)
      sprintf(buff, "%s%s", prefix, RETRIEVE_BUFFER_DATA(runProfTable[i].scen_group_name));
    else
      sprintf(buff, "%s%s", prefix, runprof_table_shr_mem[i].scen_group_name);
    fprintf(write_gdf_fp, "%s\n", buff);
    fill_2d(TwoD, *Idx2d, buff);
    *Idx2d = *Idx2d  + 1;
  }
}

// Print only Scenario Group Name as vector lines in o/p file
char **printGroup()
{
  int i = 0;
  char **TwoD;
  char prefix[1024];
  int Idx2d = 0;
  //FILE *write_gdf_fp = NULL;
  TwoD = init_2d(total_runprof_entries * (sgrp_used_genrator_entries + 1));
  NSDL2_MISC(NULL, NULL, "Method Called");

  for(i=0; i < sgrp_used_genrator_entries + 1; i++)
  {
    getNCPrefix(prefix, i-1, -1, ">", 0); //for controller or NS
    NSDL2_GDF(NULL, NULL, "in printGroup prefix is = %s", prefix);
    printGroupGraph(TwoD, &Idx2d, prefix);
  }
  return TwoD;
}

// Called by ns_parent.c to update group data size into g_avgtime_size
inline void update_group_data_avgtime_size() 
{
  NSDL1_MISC(NULL, NULL, "Method Called, g_avgtime_size = %d, group_data_gp_idx = %d, total_runprof_entries = %d",
                        g_avgtime_size, group_data_gp_idx, total_runprof_entries);
  
  if(SHOW_GRP_DATA) 
  {
    NSDL2_MISC(NULL, NULL, "SHOW GROUP DATA is enabled.");
    group_data_gp_idx = g_avgtime_size;
    g_avgtime_size += GROUP_AVGTIME_SIZE;
  } else {
    NSDL2_MISC(NULL, NULL, "SHOW GROUP DATA is disabled.");
  }
  
  NSDL2_MISC(NULL, NULL, "After g_avgtime_size = %d, group_data_gp_idx = %d",
                  g_avgtime_size, group_data_gp_idx);
}

// Called by child
inline void set_group_data_avgtime_ptr() 
{
  NSDL1_MISC(NULL, NULL, "Method Called");

  if(SHOW_GRP_DATA) 
  {
    NSDL2_MISC(NULL, NULL, "SHOW GROUP DATA is enabled.");
    grp_avgtime = (GROUPAvgTime*)((char *)average_time + group_data_gp_idx);
  } else {
    NSDL2_MISC(NULL, NULL, "SHOW GROUP DATA is disabled.");
    grp_avgtime = NULL;
  }

  NSDL2_MISC(NULL, NULL, "grp_avgtime = %p", grp_avgtime);
}



//keyword parsing
int kw_set_group_based_data(char *buf, char *err_msg, int runtime_flag)
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
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, SHOW_GROUP_DATA_USAGE, CAV_ERR_1011062, CAV_ERR_MSG_1);
  }

  if(ns_is_numeric(mode_str) == 0)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, SHOW_GROUP_DATA_USAGE, CAV_ERR_1011062, CAV_ERR_MSG_2);
  }

  mode = atoi(mode_str);
  if(mode < 0 || mode > 1)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, SHOW_GROUP_DATA_USAGE, CAV_ERR_1011062, CAV_ERR_MSG_3);
  }

  global_settings->show_group_data = mode;

  NSDL2_PARSING(NULL, NULL, "global_settings->show_group_data = %d", global_settings->show_group_data);

  return 0;
}

// Function for filling the data in the structure of GROUPAvgTime.
inline void fill_group_gp(avgtime **g_avg)
{
  int i, j = 0, v_idx;
  GROUPAvgTime *grp_avg = NULL;
  avgtime *avg = NULL;
  Group_data_gp *group_data_local_gp_ptr = group_data_gp_ptr;

  NSDL1_GDF(NULL, NULL, "Method called, group_data_idx = %d, group_data_gp_idx = %u", group_data_idx, group_data_gp_idx);

  for(v_idx = 0; v_idx < sgrp_used_genrator_entries + 1; v_idx++)
  { 
    avg = (avgtime *)g_avg[v_idx];
    grp_avg = (GROUPAvgTime *) ((char*) avg + group_data_gp_idx); 
    for (i = 0; i < total_runprof_entries ; i++, j++) 
    {
      if(grp_avg[i].session_pacing_counts > 0)
      {
        GDF_COPY_TIMES_VECTOR_DATA(group_data_idx, 0, j, 0,
                    (double)(((double)grp_avg[i].time_to_wait)/((double)(1000.0*(double)grp_avg[i].session_pacing_counts))),
                    (double)grp_avg[i].sess_min_time/1000.0,
                    (double)grp_avg[i].sess_max_time/1000.0,
                    grp_avg[i].session_pacing_counts,
                    group_data_local_gp_ptr[j].sess_time.avg_time,
                    group_data_local_gp_ptr[j].sess_time.min_time,
                    group_data_local_gp_ptr[j].sess_time.max_time,
                    group_data_local_gp_ptr[j].sess_time.succ);
      }
      else
      {
        GDF_COPY_TIMES_VECTOR_DATA(group_data_idx, 0, j, 0,
                    0,
                    -1,
                    -1,
                    0,
                    group_data_local_gp_ptr[j].sess_time.avg_time,
                    group_data_local_gp_ptr[j].sess_time.min_time,
                    group_data_local_gp_ptr[j].sess_time.max_time,
                    group_data_local_gp_ptr[j].sess_time.succ);
      }

      NSDL1_GDF(NULL, NULL, "group_data_idx = %d, i = %d time_to_wait = %d, session_pacing_counts = %d, sess_min_time = %d, sess_max_time = %d, avg_time = %f, min_time = %f, max_time = %f, succ = %f",group_data_idx, i, grp_avg[i].time_to_wait, grp_avg[i].session_pacing_counts, grp_avg[i].sess_min_time, grp_avg[i].sess_max_time, group_data_local_gp_ptr[j].sess_time.avg_time,  group_data_local_gp_ptr[j].sess_time.min_time, group_data_local_gp_ptr[j].sess_time.max_time, group_data_local_gp_ptr[j].sess_time.succ);
    
      if(grp_avg[i].ka_counts > 0)
      {
        GDF_COPY_TIMES_VECTOR_DATA(group_data_idx, 1, j, 0,
                      (double)(((double)grp_avg[i].ka_time)/((double)(1000.0*(double)grp_avg[i].ka_counts))),
                      (double)grp_avg[i].ka_min_time/1000.0,
                      (double)grp_avg[i].ka_max_time/1000.0,
                      grp_avg[i].ka_counts,
                      group_data_local_gp_ptr[j].ka_time.avg_time,
                      group_data_local_gp_ptr[j].ka_time.min_time,
                      group_data_local_gp_ptr[j].ka_time.max_time,
                      group_data_local_gp_ptr[j].ka_time.succ);
      }
      else
      {
        GDF_COPY_TIMES_VECTOR_DATA(group_data_idx, 1, j, 0,
                      0,
                      -1,
                      -1,
                      0,
                      group_data_local_gp_ptr[j].ka_time.avg_time,
                      group_data_local_gp_ptr[j].ka_time.min_time,
                      group_data_local_gp_ptr[j].ka_time.max_time,
                      group_data_local_gp_ptr[j].ka_time.succ);
      }

      //NSDL1_GDF(NULL, NULL, "Group idx = %d, group_data_idx = %d, ka_time = %d, ka_counts = %d, ka_min_time = %d, ka_max_time = %d, avg_time = %f, min_time = %f, max_time = %f, succ = %f",i, group_data_idx, grp_avg[i].ka_time, grp_avg[i].ka_counts, grp_avg[i].ka_min_time, grp_avg[i].ka_max_time, group_data_local_gp_ptr[i].ka_time.avg_time,  group_data_local_gp_ptr[i].ka_time.min_time, group_data_local_gp_ptr[i].ka_time.max_time, group_data_local_gp_ptr[i].ka_time.succ);

      if(grp_avg[i].page_think_counts > 0)
      {
        GDF_COPY_TIMES_VECTOR_DATA(group_data_idx, 2, j, 0,
                      (double)(((double)grp_avg[i].page_think_time)/((double)(1000.0*(double)grp_avg[i].page_think_counts))),
                      (double)grp_avg[i].page_think_min_time/1000.0,
                      (double)grp_avg[i].page_think_max_time/1000.0,
                      grp_avg[i].page_think_counts,
                      group_data_local_gp_ptr[j].page_think_time.avg_time,
                      group_data_local_gp_ptr[j].page_think_time.min_time,
                      group_data_local_gp_ptr[j].page_think_time.max_time,
                      group_data_local_gp_ptr[j].page_think_time.succ);
      }
      else
      {
        GDF_COPY_TIMES_VECTOR_DATA(group_data_idx, 2, j, 0,
                      0,
                      -1,
                      -1,
                      0,
                      group_data_local_gp_ptr[j].page_think_time.avg_time,
                      group_data_local_gp_ptr[j].page_think_time.min_time,
                      group_data_local_gp_ptr[j].page_think_time.max_time,
                      group_data_local_gp_ptr[j].page_think_time.succ);
      }

      NSDL1_GDF(NULL, NULL, "Group idx = %d, group_data_idx = %d, page_think_time = %d, page_think_counts = %d, page_think_min_time = %d, page_think_max_time = %d, avg_time = %f, min_time = %f, max_time = %f, succ = %f",i, group_data_idx, grp_avg[i].page_think_time, grp_avg[i].page_think_counts, grp_avg[i].page_think_min_time, grp_avg[i].page_think_max_time, group_data_local_gp_ptr[j].page_think_time.avg_time, group_data_local_gp_ptr[j].page_think_time.min_time, group_data_local_gp_ptr[j].page_think_time.max_time, group_data_local_gp_ptr[j].page_think_time.succ);
    }
    //group_data_local_gp_ptr++;
  }
}

// This Function will be set the counters for session pacing graph data
void set_grp_based_counter_for_session_pacing(void *lvptr, int time_to_think)
{
  VUser *vptr = (VUser *) lvptr;

  NSDL1_GDF(vptr, NULL, "Method Called, Counters = %d, time_to_think = %d time_to_wait = %d, sess_min_time = %d, sess_max_time = %d", grp_avgtime[vptr->group_num].session_pacing_counts, time_to_think, grp_avgtime[vptr->group_num].time_to_wait, grp_avgtime[vptr->group_num].sess_min_time, grp_avgtime[vptr->group_num].sess_max_time);

  if(vptr->flags & DO_NOT_INCLU_SPT_IN_TIME_GRAPH)
  {
    NSTL2(vptr, NULL, "SHOW_GRAPH_DATA: First session is enabled on this vuser so returning.");
    vptr->flags &= ~DO_NOT_INCLU_SPT_IN_TIME_GRAPH;
    return;
  }

  if(time_to_think < grp_avgtime[vptr->group_num].sess_min_time)
    grp_avgtime[vptr->group_num].sess_min_time = time_to_think; 
  if(time_to_think > grp_avgtime[vptr->group_num].sess_max_time) 
    grp_avgtime[vptr->group_num].sess_max_time = time_to_think;

  grp_avgtime[vptr->group_num].session_pacing_counts++;
  grp_avgtime[vptr->group_num].time_to_wait += time_to_think;

  NSDL1_GDF(vptr, NULL, "After updating, Counters = %d, time_to_think = %d time_to_wait = %d, sess_min_time = %d, sess_max_time = %d", grp_avgtime[vptr->group_num].session_pacing_counts, time_to_think, grp_avgtime[vptr->group_num].time_to_wait, grp_avgtime[vptr->group_num].sess_min_time, grp_avgtime[vptr->group_num].sess_max_time);
}

void set_grp_based_counter_for_keep_alive(void *lvptr, int ka_time_out)
{
  VUser *vptr = (VUser *) lvptr;
  NSDL1_GDF(vptr, NULL, "Method Called ka_timeout = %d, ka_min_time = %d, ka_max_time = %d", ka_time_out, grp_avgtime[vptr->group_num].ka_min_time, grp_avgtime[vptr->group_num].ka_max_time);

  if(ka_time_out < grp_avgtime[vptr->group_num].ka_min_time)
    grp_avgtime[vptr->group_num].ka_min_time = ka_time_out;

  if(ka_time_out > grp_avgtime[vptr->group_num].ka_max_time)
    grp_avgtime[vptr->group_num].ka_max_time = ka_time_out;
  
  grp_avgtime[vptr->group_num].ka_time += ka_time_out;
  grp_avgtime[vptr->group_num].ka_counts++;

  NSDL1_GDF(vptr, NULL, "After updating, Counters = %d, ka_min_time = %d, ka_max_time = %d", grp_avgtime[vptr->group_num].ka_counts, grp_avgtime[vptr->group_num].ka_min_time, grp_avgtime[vptr->group_num].ka_max_time);

}

void set_grp_based_counter_for_page_think_time(void *lvptr, int pg_think_time)
{
  VUser *vptr = (VUser *) lvptr;

  NSDL1_GDF(vptr, NULL, "Method Called pg_think_time = %d, page_think_min_time = %d, page_think_max_time = %d", pg_think_time, grp_avgtime[vptr->group_num].page_think_min_time, grp_avgtime[vptr->group_num].page_think_max_time);

  if(pg_think_time < grp_avgtime[vptr->group_num].page_think_min_time)
    grp_avgtime[vptr->group_num].page_think_min_time = pg_think_time;

  if(pg_think_time > grp_avgtime[vptr->group_num].page_think_max_time)
    grp_avgtime[vptr->group_num].page_think_max_time = pg_think_time;
  
  grp_avgtime[vptr->group_num].page_think_time += pg_think_time;
  grp_avgtime[vptr->group_num].page_think_counts++;

  NSDL1_GDF(vptr, NULL, "After updating, Counters = %d, page_think_min_time = %d, page_think_max_time = %d", grp_avgtime[vptr->group_num].page_think_counts, grp_avgtime[vptr->group_num].page_think_min_time, grp_avgtime[vptr->group_num].page_think_max_time);
}
