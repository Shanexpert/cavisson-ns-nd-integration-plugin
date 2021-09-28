/********************************************************************************************************************
 * File Name      : ns_monitor_mertic_priority.c                                                                    |
 |                                                                                                                  | 
 * Synopsis       : This file will contain all the function involved in Project "RTG Packet Optimisation and Metric |  
 |                  Priority"                                                                                      |
 |                                                                                                                  |
 * Author(s)      : Manish Kumar Mishra                                                                             |
 |                                                                                                                  |
 * Date           : Wed Jul 11 18:46:52 IST 2018 
 |                                                                                                                  |
 * Copyright      : (c) Cavisson Systems                                                                            |
 |                                                                                                                  |
 * Mod. History   :                                                                                                 |
 *******************************************************************************************************************/

#include <stdio.h>
#include <string.h>

#include "ns_log.h"
#include "ns_msg_def.h"
#include "ns_gdf.h"
#include "ns_exit.h"
#include "ns_global_settings.h"
#include "ns_monitor_metric_priority.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"


char g_metric_priority = METRIC_PRIORITY_LOW;
HMLMetric *hml_metrics;


/*---------------------------------------------------------------------------------------
  Fun. Name     : set_metric_priority()
  Purpose       : This function will set global variable g_metric_priority 
  Input         : priority   - provide string containg priority like H, M and L 
                  flag       - provide set priority as default
----------------------------------------------------------------------------------------*/
char get_metric_priority_id(char *priority, int flag)
{
  char dest; 
  NSDL2_GDF(NULL, NULL, "Method called, priority = %s, flag = %d", priority, flag);

  if(flag)
    dest = MAX_METRIC_PRIORITY_LEVELS;
  else
  {
    if(!strcasecmp(priority, "H"))
      dest = METRIC_PRIORITY_HIGH;
    else if(!strcasecmp(priority, "M"))
      dest = METRIC_PRIORITY_MEDIUM;
    else
      dest = METRIC_PRIORITY_LOW;
  }
  
  NSDL2_GDF(NULL, NULL, "dest = %d", dest);
  return dest;
}

char get_metric_priority_by_id(int id)
{
  char dest;

  if(id == METRIC_PRIORITY_HIGH)
    dest = 'H';
  else if(id == METRIC_PRIORITY_MEDIUM)
    dest = 'M';
  else
    dest = 'L';

  return dest;
}

/*---------------------------------------------------------------------------------------
  Fun. Name     : is_graph_excluded()
  Purpose       : This function will tell whether provide graph need to consider or not
                  in the test.
                  Graph consideration criteria -
                  1. If provided graph is marked as Excluded i.e. its 8th field has value 
                     'E', then graph will not be consider
                  2. If in JSON monitor  
----------------------------------------------------------------------------------------*/
int is_graph_excluded(char *graph_priority, char *graph_state, int mon_priority)
{
  int ret = GRAPH_NOT_EXCLUDED;
  //char gp_priority = METRIC_PRIORITY_HIGH;
  char gp_priority = METRIC_PRIORITY_LOW;

  NSDL2_GDF(NULL, NULL, "Method called, graph_priority = %s, graph_state = %s, mon_priority = %d", 
                         graph_priority, graph_state, mon_priority);

  if(!strcmp(graph_state, "E")) 
    ret = GRAPH_EXCLUDED;
  else if(mon_priority != -1)
  {
    // Set graph priority
    if(!strcmp(graph_priority, "H"))
      gp_priority = METRIC_PRIORITY_HIGH;
    else if(!strcmp(graph_priority, "M"))
      gp_priority = METRIC_PRIORITY_MEDIUM;
    else
      gp_priority = METRIC_PRIORITY_LOW;

    //Check this graph priority is >=< than Overall priority
    //if(gp_priority > g_metric_priority)
    if(gp_priority > mon_priority)
      ret = GRAPH_EXCLUDED;
    else
    {
      if(mon_priority == MAX_METRIC_PRIORITY_LEVELS)
        ret = MAX_METRIC_PRIORITY_LEVELS;
      else
        ret = gp_priority;
    }
  }

  return ret;
} 


int kw_enable_hml_group_in_testrun_gdf(char *keyword, char *buf, char *err_msg, int runtime_flag)
{
  char key[1024];
  int value;
        
  NSDL2_MON(NULL, NULL, "Method called, keyword = %s, buf = %s", keyword, buf);
 
  if(sscanf(buf, "%s %d", key, &value) != 2)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, ENABLE_HML_GROUPS_USAGE, CAV_ERR_1011160, CAV_ERR_MSG_1);
  }

  global_settings->enable_hml_group_in_testrun_gdf = value;
  NSDL2_MON(NULL, NULL, "enable_clubing_hml_in_rtg = %d", global_settings->enable_hml_group_in_testrun_gdf);
  return 0;
}

//Pathak : add comments
inline void init_hml_msg_data_size()
{
  int i;

  NSDL3_GDF(NULL, NULL, "Method called");

  for(i = 0; i <= MAX_METRIC_PRIORITY_LEVELS; i++)
  {
    hml_msg_data_size[i][HML_START_RTG_IDX] = -1;
    hml_msg_data_size[i][HML_MSG_DATA_SIZE] = 0;
  }

  hml_start_rtg_idx = -1;
}


