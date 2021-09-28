/*──────────────────────────────────────────────────────────────────────────────────────
##   @ This file is part of cavMain Integration with TSDB                                │
##   @ ns_tsdb.c                                                                         │
##                                                                                       │
##   • Description : Contains all the common function used in Comunincation with TSDB    │
##                      1) Register Gdf.                                                 │
##                      2) Metrics Register.                                             │
##                      3) Data Insert.                                                  │
##                      4) Metrics Delete.                                               │
##                      5) TSDB API Logging.                                             │
##                                                                                       │
##   • Date: June, 2020                                                                  │
##                                                                                       │
##   • Author: Paras Jain <paras.jain@cavisson.com>                                      │
##                                                                                       │
##   • Reviewer    : Prashant Singhal <prashant.singhal@cavisson.com>                    │
##                   Kushal Jerath    <kushal.jerath@cavisson.com>                       │
##                                                                                       │
##   @Copyright (C) 2020 by Cavisson System <www.cavisson.com>                           │
##                                                                                       │
###──────────────────────────────────────────────────────────────────────────────────────*/
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <regex.h>
#include <errno.h>
#include <curl/curl.h>
#include <time.h>
#include <sys/stat.h>
#include <locale.h>
#include "ns_msg_def.h"
#include "ns_gdf.h"
#include "ns_log.h"
#include "ns_event_id.h"
#include "ns_event_log.h"
#include "ns_string.h"
#include "ns_custom_monitor.h"
#include "ns_mon_log.h"
#include "cav_tsdb_interface.h"
#include "ns_tsdb.h"
#include "ns_appliance_health_monitor.h"
char g_send_backlog_data_to_TSDB = 0;
//void extract_derive_formula(char *buff, char *derived_formula); 
/**
 * Name		: ns_tsdb_init
 *
 * Description	: init of API logging structure 
 **/
inline void ns_tsdb_init()
{
  memset(api_timing, 0,TOTAL_API_CALL * sizeof(tsdb_api_timing));
  api_timing[TSDB_REGISTER_GROUP].api_name = "tsdb_register_gdf";
  api_timing[TSDB_REGISTER_SUBJECT].api_name = "tsdb_insert_bulk_metric_by_mg_gid";
  api_timing[TSDB_ADD_BULK_METRICS].api_name = "tsdb_insert_bulk_metric_by_id";
  api_timing[TSDB_DELETE_SUBJECT].api_name = "tsdb_delete_metrics_by_id";
}

/* Name		: ns_tsdb_timespec_diff
 *
 * Description	: Calculate the difference between start and stop time and store in result
 **/
static inline void ns_tsdb_timespec_diff(struct timespec *start, struct timespec *stop,  struct timespec *result)
{
  if((stop->tv_nsec - start->tv_nsec) < 0) 
  {
    result->tv_sec = stop->tv_sec - start->tv_sec - 1;
    result->tv_nsec = stop->tv_nsec - start->tv_nsec + 1000000000;
  } 
  else 
  {
    result->tv_sec = stop->tv_sec - start->tv_sec;
    result->tv_nsec = stop->tv_nsec - start->tv_nsec;
  }
  return;
}

/**
 * Name		: ns_tsdb_dumb_logs
 *
 * Description	: Logging of TSDB API min,max,avg time and count at every 15 minutes
 **/
inline void ns_tsdb_dumb_logs()
{
  for (int i = 0 ; i < TOTAL_API_CALL ; i++)
  {
    if (api_timing[i].count > 0)
    {
      setlocale(LC_NUMERIC, "");
      NSTL1(NULL, NULL, "TSDB_INGESTION:Time taken by TSDB api's Api name = '%s', min(ns) = '%'ld', max(ns) = '%'ld', avg(ns) = '%'ld',sum(ns) = '%'ld, Api count = '%'ld', Error count = '%'d', Ingestion_Rate = '%'5.2f'\n", api_timing[i].api_name, api_timing[i].min,api_timing[i].max, api_timing[i].avg/api_timing[i].count, api_timing[i].avg, api_timing[i].count, api_timing[i].error_count, (float)api_timing[i].count/TSDB_INGESTION_RATE);
      api_timing[i].min=api_timing[i].max;
      api_timing[i].max=0;
      api_timing[i].error_count=0;
      api_timing[i].count = 0;
      api_timing[i].avg = 0;
    }
  }
}

/* Name		: ns_tsdb_log_api_timing
 *
 * Description	: Fill the max,min,count and avg in api_timing structurfill the max,min,count and avg in api_timing structure
 **/
static inline void ns_tsdb_log_api_timing(struct timespec *start, struct timespec *stop, int api_type,  int error_count)
{
  struct timespec result;

  //calculate time diff
  ns_tsdb_timespec_diff(start, stop, &result);

  //based on the api index as defined in the above macro
  api_timing[api_type].avg = api_timing[api_type].avg + result.tv_nsec;

  if ((result.tv_nsec) >  api_timing[api_type].max)
    api_timing[api_type].max = result.tv_nsec; 

  if (result.tv_nsec <  api_timing[api_type].min)
    api_timing[api_type].min = result.tv_nsec;
   
  api_timing[api_type].count++;

  if(error_count)
    api_timing[api_type].error_count++;
}

/**
 * Name		: ns_tsdb_delete_metrics_by_id
 *
 * Description	: Delete Metric id of a vector on server inactive/end monitor
 **/
inline void ns_tsdb_delete_metrics_by_id(CM_info *cus_mon_ptr, void *metric_id)
{
  char err_msg[TSDB_ERROR_BUF_8192 + 1];
  //Group_Info *local_group_data_ptr;
  int    ret = 0; 
  struct timespec start;
  struct timespec stop;

  MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, "TSDB:LOGGING Going to delete Metric id = %ld",metric_id);
 //  local_group_data_ptr = group_data_ptr + cus_mon_ptr->gp_info_index;
  
  if (clock_gettime(CLOCK_MONOTONIC, &start) == -1)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, "TSDB:Error in clock_gettime()");
  }

//  ret = tsdb_delete_metric_by_id(metric_id, err_msg);

  if (clock_gettime(CLOCK_MONOTONIC, &stop) == -1)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, "TSDB:Error in clock_gettime()");
  }

  ns_tsdb_log_api_timing(&start, &stop, TSDB_DELETE_SUBJECT, (ret == -1) ? 1:0);

  if(ret < 0)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, "TSDB:ERROR Received error deleting metric id error message = %s Return val =%d\n", err_msg, ret);
  }
}

 
/*static inline char get_subject_name(CM_info *cus_mon_ptr, char *vector_name,  tag_val_ptr_t tv_arr_buf[], int num_levels)
{ 
  //CM_vector_info *vector_list = cus_mon_ptr->vector_list;
  if(vector_name == NULL)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, "TSDB:ERROR INVALID VECTOR_NAME\n");
    return -1;
  }

  strcpy(vector_name_bkp, vector_name);
  strcpy(Hierarchy_bkp, local_group_data_ptr->Hierarchy);
  tv_arr_buf[0].tag = "customer";
  tv_arr_buf[1].tag = "application";
  tv_arr_buf[0].val =  "Default";
  tv_arr_buf[1].val =  "Default";
   
  mon_breadcrumb_fields =   get_tokens(vector_name_bkp, vector_fields,  ">",  50); 

  vector_name_fields    =   get_tokens(Hierarchy_bkp, breadcrumb_fields, ">", 50);

  if(mon_breadcrumb_fields != vector_name_fields)
  {
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, "TSDB:ERROR MISMATCHED BREADCUM vector_name_fields  %d mon_breadcrumb_fields %d vector_name = %s local_group_data_ptr->Hierarchy %s MONITOR_NAME %s Group Name %s\n", vector_name_fields, mon_breadcrumb_fields, vector_name, local_group_data_ptr->Hierarchy, cus_mon_ptr->monitor_name, local_group_data_ptr->group_name);
  }
  
  for (int i = 0 ; i< vector_name_fields;i++)
  {
    tv_arr_buf[i + 2].tag =  breadcrumb_fields[i];
    if(mon_breadcrumb_fields > i)
      tv_arr_buf[i + 2 ].val =  vector_fields[i];
    else
      tv_arr_buf[i + 2].val = "Default";
    
  }
  //MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, "TSDB:LOGGING Subject send to TSDB  %s\n",subject);
  return 0;
}*/

/**
 * Name		: ns_tsdb_insert
 *
 * Description	: Send Data to TSDB
 **/
inline void ns_tsdb_insert(CM_info *cus_mon_ptr, double *data, void *metrics_idx, char *metric_id_found, long ts, char *vector_name)
{
  int frequency = cus_mon_ptr->frequency /1000;
  //long mg_gid;
 // int num_levels = 0;
  int i = 0;
  int  ret = 0;
  int mon_breadcrumb_fields;
  int vector_name_fields;
  char *breadcrumb_fields[50];
  char *vector_fields[50];
  char vector_name_bkp[1024];
  char Hierarchy_bkp[1024];
  Graph_Info *local_graph_data_ptr;
  Group_Info *local_group_data_ptr;
  char err_msg[TSDB_ERROR_BUF_8192 + 1];
  struct timespec start;
  struct timespec stop;
  tsdb_metric_group tsdb_group_data_ptr;
  tsdb_metric_info  tsdb_metric_ptr[256];

  if(cus_mon_ptr->gp_info_index < 0)//Group not parsed 
  {  
    MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, "TSDB:LOGGING Group not parsed for monitor %s\n", cus_mon_ptr->monitor_name);
    cus_mon_ptr->flags|= DATA_PENDING;   
  //  cus_mon_ptr->ts    = ts; 
    g_send_backlog_data_to_TSDB = 1;
    return; 
  }
  else//if mg_id not available 
  {
    local_group_data_ptr = group_data_ptr + cus_mon_ptr->gp_info_index;

    MLTL3(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, "TSDB:LOGGING local_group_data_ptr->mg_gid = %ld\n", local_group_data_ptr->mg_gid);

    if(local_group_data_ptr->mg_gid < 0) //Group not registered with TSDB
    {
      local_graph_data_ptr = graph_data_ptr + local_group_data_ptr->graph_info_index; 
      tsdb_group_data_ptr.num_graphs = local_group_data_ptr->num_graphs;
      tsdb_group_data_ptr.rpt_grp_id = local_group_data_ptr->rpt_grp_id;
      tsdb_group_data_ptr.group_type = local_group_data_ptr->group_type;
      strcpy(tsdb_group_data_ptr.group_metric , local_group_data_ptr->groupMetric);
      strcpy(tsdb_group_data_ptr.group_name , local_group_data_ptr->group_name);
      strcpy(tsdb_group_data_ptr.group_desc , local_group_data_ptr->group_description);
      snprintf(tsdb_group_data_ptr.heirarchy, 128, "%s%s", STORE_PREFIX, local_group_data_ptr->Hierarchy);
       MLTL4(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, "Group Num Graphs =%d , Group Metric= %s, Group Name = %s,Group Description = %s,Group Hierarchy =%s\n",tsdb_group_data_ptr.num_graphs, tsdb_group_data_ptr.group_metric,tsdb_group_data_ptr.group_name,tsdb_group_data_ptr.group_desc,tsdb_group_data_ptr.heirarchy);


      for(i = 0; i < local_group_data_ptr->num_graphs; i++)
      {
        strcpy(tsdb_metric_ptr[i].graph_name, local_graph_data_ptr->graph_name);
        strcpy(tsdb_metric_ptr[i].graph_desc, local_graph_data_ptr->graph_discription);
        strcpy(tsdb_metric_ptr[i].derived_formula, local_graph_data_ptr->derived_formula);
        tsdb_metric_ptr[i].graph_type = local_graph_data_ptr->graph_type;
        tsdb_metric_ptr[i].formula = local_graph_data_ptr->formula;
        tsdb_metric_ptr[i].graph_state = local_graph_data_ptr->graph_state;
        tsdb_metric_ptr[i].metric_priority = local_graph_data_ptr->metric_priority;
        tsdb_metric_ptr[i].data_type = local_graph_data_ptr->data_type;
        tsdb_metric_ptr[i].rpt_id = local_graph_data_ptr->rpt_id;
        MLTL4(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, "Graph name =%s, Graph Description =%s,Graph Type=%d,Graph Formula=%d,Graph State=%d, Graph Metric Priority=%d, Graph Data Type=%d Graph Derived Formulas %s \n", tsdb_metric_ptr[i].graph_name,tsdb_metric_ptr[i].graph_desc, tsdb_metric_ptr[i].graph_type,tsdb_metric_ptr[i].formula, tsdb_metric_ptr[i].graph_state, tsdb_metric_ptr[i].metric_priority,tsdb_metric_ptr[i].data_type, tsdb_metric_ptr[i].derived_formula);

        local_graph_data_ptr = local_graph_data_ptr + 1;
      }

      MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, "TSDB:LOGGING Called tsdb_register_gdf() Method with group_name %s Graphs count =%d\n", tsdb_group_data_ptr.group_name, tsdb_group_data_ptr.num_graphs);
         
      if (clock_gettime(CLOCK_MONOTONIC, &start) == -1)
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, "TSDB:Error in clock_gettime()");
      }
    
      //Register gdf
  //    mg_gid = tsdb_register_gdf(&tsdb_group_data_ptr, local_group_data_ptr->num_graphs, tsdb_metric_ptr, err_msg);

      if (clock_gettime(CLOCK_MONOTONIC, &stop) == -1)
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, "TSDB:Error in clock_gettime()");
      }

    //  ns_tsdb_log_api_timing(&start, &stop, TSDB_REGISTER_GROUP, mg_gid == -1 ? 1:0);
 
     //: MLTL3(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, "TSDB:LOGGING tsdb_register_gdf() Method Returned value =%ld\n", mg_gid);

     /* if(mg_gid < 0)
      {
        MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, "TSDB:ERROR tsdb_register_gdf() error message = %s Return val = %ld\n", err_msg, mg_gid);
        return;
      }
*/
     // local_group_data_ptr->mg_gid = mg_gid;
    }
  }

  if(*metric_id_found == 1) //Metric_id_found value will be 0 on init/vector delete
  {
    //global_settings->progress_secs value in ms , covert in seconds

    //Send data by metric id
    MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, "TSDB:LOGGING Called tsdb_insert_bulk_metric_by_id() Method with args Graphs =%d, frequency =%d time stamp =%ld\n" , local_group_data_ptr->num_graphs, frequency, ts); 

    if (clock_gettime(CLOCK_MONOTONIC, &start) == -1)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, "TSDB:Error in clock_gettime()");
    }

 //   ret = tsdb_insert_bulk_metric_by_id(NULL , metrics_idx,frequency, data, local_group_data_ptr->num_graphs, ts);

    if (clock_gettime(CLOCK_MONOTONIC, &stop) == -1)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, "TSDB:Error in clock_gettime()");
    }

    ns_tsdb_log_api_timing(&start, &stop, TSDB_ADD_BULK_METRICS, (ret == -1) ? 1:0);

    if(ret < 0)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, "TSDB:ERROR tsdb_insert_bulk_metric_by_id() error message = %s Return val = %d\n", err_msg, ret);
      return;
    }

  //  #ifdef TSDB_TEST 
 //   char temp_subject[]= "ddd";
     for (int i = 0 ; i < local_group_data_ptr->num_graphs ; i ++)
     {
        MLTL3(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, "TSDB tsdb_val_data data %f ",data[i]);
     }

 //   ret = tsdb_val_data(cus_mon_ptr->monitor_name, temp_subject, local_group_data_ptr->num_graphs,frequency, 0, metrics_idx, data, err_msg);

    if(ret < 0)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, "TSDB:ERROR tsdb_val_data() error message = %s Return val = %d\n", err_msg, ret);
    }
  //  #endif
  }
  else
  {
  
    if(vector_name == NULL)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, "TSDB:ERROR INVALID VECTOR_NAME\n");
      return;
    }

    //num_levels = cus_mon_ptr->breadcrumb_level + 2; //for customer and application
    strcpy(vector_name_bkp, vector_name);
    strcpy(Hierarchy_bkp, local_group_data_ptr->Hierarchy);
    mon_breadcrumb_fields =   get_tokens(vector_name_bkp, vector_fields,  ">",  50);
    vector_name_fields    =   get_tokens(Hierarchy_bkp, breadcrumb_fields, ">", 50);
    
    if(enable_store_config)
    {
      int len = strlen(g_hierarchy_prefix);
      char buff[1024];
      strcpy(buff, breadcrumb_fields[0]);
      memmove(buff + len,buff, strlen(buff)) ;
      strncpy(buff, g_hierarchy_prefix, len);
      breadcrumb_fields[0] = buff;
    }

    if(mon_breadcrumb_fields != vector_name_fields)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, "TSDB:ERROR MISMATCHED BREADCUM vector_name_fields  %d mon_breadcrumb_fields %d vector_name = %s local_group_data_ptr->Hierarchy %s MONITOR_NAME %s Group Name %s\n", vector_name_fields, mon_breadcrumb_fields, vector_name, local_group_data_ptr->Hierarchy, cus_mon_ptr->monitor_name, local_group_data_ptr->group_name);
    }
  /*  tag_val_ptr_t tv_arr_buf[num_levels + 2];
   
    tv_arr_buf[0].tag = "customer";
    tv_arr_buf[1].tag = "application";
    tv_arr_buf[0].val =  "Default";
    tv_arr_buf[1].val =  "Default";
 */
    if(mon_breadcrumb_fields != vector_name_fields)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, "TSDB:ERROR MISMATCHED BREADCUM vector_name_fields  %d mon_bre    adcrumb_fields %d vector_name = %s local_group_data_ptr->Hierarchy %s MONITOR_NAME %s Group Name %s\n", vector_name_fields, mon_breadcrumb_fields, vector_name, local_group_data_ptr->Hierarchy, cus_mon_ptr->monitor_name, local_group_data_ptr->group_name);
    }
 
  /*  for (int i = 0 ; i< vector_name_fields;i++)
    {
      tv_arr_buf[i + 2].tag =  breadcrumb_fields[i];
      if(mon_breadcrumb_fields > i)
       tv_arr_buf[i + 2 ].val =  vector_fields[i];
      else
       tv_arr_buf[i + 2].val = "Default";
     }
*/

   // ret = get_subject_name(cus_mon_ptr, vector_name, tv_arr, num_levels);
   
   
    MLTL3(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, "TSDB:LOGGING Time stamp sent to TSDB in milliseconds = %ld, num_graphs=%d\n", ts, local_group_data_ptr->num_graphs);

    //global_settings->progress_secs value in ms , covert in seconds

    for(int k = 0; k < cus_mon_ptr->no_of_element; k++)
    {
      MLTL4(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, "TSDB:ELEMENT %f address = %x", data[k], &data[k]); 
    }

      MLTL3(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, "TSDB:Arguments Send with tsdb_insert_bulk_metric_by_mg_gid Method(), Metric Group gid =%d ,Freuency =%d, Data=%ld, Num Graphs =%d,Timestamp=%ld",local_group_data_ptr->mg_gid,frequency, data, local_group_data_ptr->num_graphs,ts); 

    if (clock_gettime(CLOCK_MONOTONIC, &start) == -1)
    { 
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, "TSDB:Error in clock_gettime()");
    }

   //Send data with register subject and get metric id
//    ret = tsdb_insert_bulk_metric_by_mg_gid_ex(tv_arr_buf, num_levels, local_group_data_ptr->mg_gid,frequency, data, local_group_data_ptr->num_graphs , metrics_idx, ts, err_msg);

    if (clock_gettime(CLOCK_MONOTONIC, &stop) == -1)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, "TSDB:Error in clock_gettime()");
    }
  
    ns_tsdb_log_api_timing(&start, &stop, TSDB_REGISTER_SUBJECT, (ret== -1) ? 1:0);
    
     // MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, "TSDB:ELEMENT metrics_idx = %s\n address %p", (char *)(metrics_idx),metrics_idx); 
    if(ret < 0)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, "TSDB:ERROR tsdb_insert_bulk_metric_by_mg_gid() error message = %s Return val = %d\n", err_msg, ret);
      return;
    }
      *metric_id_found=1;
    ///#ifdef TSDB_TEST
     MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, "TSDB:Arguments tsdb_val_data() Methods Called , Monitor name=%s,Number Graphs=%d,Frequency=%d,Data=%ld",cus_mon_ptr->monitor_name,local_group_data_ptr->num_graphs, frequency,data);
     for (int i = 0 ; i < local_group_data_ptr->num_graphs ; i ++)
     {
   MLTL3(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, "TSDB:tsdb_insert_bulk_metric_by_mg_gid  data %f ",data[i]);
     }
   //  ret = tsdb_val_data(cus_mon_ptr->monitor_name, subject, local_group_data_ptr->num_graphs, frequency, 0, metrics_idx, data, err_msg);

    if(ret < 0)
    {
      MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, "TSDB:ERROR tsdb_val_data() error message = %s Return val = %d\n", err_msg, ret);
    }
   // #endif
  }
}

/**
 * Name		: ns_tsdb_send_pending_data
 *
 * Description	: Send Pending data to TSDB, for a cm_ptr it will be buffered till group index not parsed
 **/

inline void ns_tsdb_send_pending_data()
{
  int i = 0, j = 0;
  CM_info *cm_ptr = NULL;

  MLTL1(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, "TSDB:LOGGING ns_tsdb_send_pending_data() called\n");

  for(i=0; i< total_monitor_list_entries; i++)
  {
    cm_ptr = monitor_list_ptr[i].cm_info_mon_conn_ptr;

   // cm_ptr->ts = time(NULL);

    if(cm_ptr->flags & DATA_PENDING) //DATA_PENDING will be marked only vector recived for first time for Group
    {
      CM_vector_info *vector_list = cm_ptr->vector_list;

      for(j = 0 ;j < cm_ptr->total_vectors; j++)
        ns_tsdb_insert(cm_ptr, vector_list[j].data, vector_list[j].metrics_idx, &vector_list[j].metrics_idx_found, 0, (monitor_list_ptr[cm_ptr->monitor_list_idx].is_dynamic) ? vector_list[j].mon_breadcrumb: vector_list[j].vector_name);
         
      cm_ptr->flags &= DATA_PENDING;
    }
  }
}

/* Name		: ns_tsdb_group_information
 *
 * Description	: Fill group information for TSDB
 **/
static inline void ns_tsdb_group_information(Group_Info *group_data_ptr, tsdb_metric_group *tsdb_group_data_ptr)
{
  tsdb_group_data_ptr->num_graphs = group_data_ptr->num_graphs;
  strcpy(tsdb_group_data_ptr->group_metric , group_data_ptr->groupMetric);
  strcpy(tsdb_group_data_ptr->group_name , group_data_ptr->group_name);
  strcpy(tsdb_group_data_ptr->group_desc , group_data_ptr->group_description);
  strcpy(tsdb_group_data_ptr->heirarchy,group_data_ptr->Hierarchy);
  MLTL4(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, "Group Num Graphs =%d , Group Metric= %s, Group Name = %s,Group Name  = %s,Group Hierarchy =%s\n",tsdb_group_data_ptr->group_metric,tsdb_group_data_ptr->group_name,tsdb_group_data_ptr->group_desc,tsdb_group_data_ptr->heirarchy);
  /*if (tsdb_group_data_ptr.group_type == GROUP_GRAPH_TYPE_VECTOR)
  {
    if (!tsdb_group_data_ptr.excluded_graph)
    { 
       memset(tsdb_group_data_ptr.excluded_graph, '0', tsdb_group_data_ptr.num_graphs * sizeof(char));
    }
  }*/
  
}
/*void extract_derive_formula(char *buff, char *derived_formula)
{
  int count = 0;
  char *ptr = buff;
  char temp[1024];
  while (*ptr != '\0' )
  {
    if (*ptr == '|')
      count++;
    if(count > 11)
    {
      count = 0;
      ptr=ptr + 1;
      break;
    }
    ptr++;
  }
 
  strcpy(temp,ptr);
  ptr=temp;
  while(*ptr != '\0')
  {
    if(*ptr == '|')
    {
      *ptr = '\0';
      strcpy(derived_formula,temp);
      break;
    }
    ptr++;
  }
}
*/
/* Name		: ns_tsdb_graph_info
 *
 * Description	: Fill Graph info for TSDB
 **/
static inline void ns_tsdb_graph_info(Graph_Info *graph_data_ptr, Group_Info *group_data_ptr, tsdb_metric_info  *tsdb_metric_ptr[])
{
  int i;

  for(i = 0; i < group_data_ptr->num_graphs; i++)
  {
    strcpy(tsdb_metric_ptr[i]->graph_name, graph_data_ptr->graph_name);
    strcpy(tsdb_metric_ptr[i]->graph_desc, graph_data_ptr->graph_discription);
//extract_derive_formula(graph_data_ptr->gline, tsdb_metric_ptr[i]->derived_formula);
    tsdb_metric_ptr[i]->graph_type = graph_data_ptr->graph_type;
    tsdb_metric_ptr[i]->formula = graph_data_ptr->formula + 1;
    tsdb_metric_ptr[i]->graph_state = graph_data_ptr->graph_state;
    tsdb_metric_ptr[i]->metric_priority = graph_data_ptr->metric_priority;
    tsdb_metric_ptr[i]->data_type = graph_data_ptr->data_type;
    MLTL4(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, "Graph name =%s, Graph Description =%s,Graph Type=%d,Graph Formula=%d,Graph State=%d, Graph Metric Priority=%d, Graph Data Type=%d\n", tsdb_metric_ptr[i]->graph_name,tsdb_metric_ptr[i]->graph_desc, tsdb_metric_ptr[i]->graph_type,tsdb_metric_ptr[i]->formula, tsdb_metric_ptr[i]->graph_state, tsdb_metric_ptr[i]->metric_priority,tsdb_metric_ptr[i]->data_type);
    graph_data_ptr = graph_data_ptr + 1;
  }
}

/* Name             : ns_tsdb_malloc_metric_id_buffer
 *
 * Description      : It allocate the metrics_id on the bases of num_element
 **/
inline void ns_tsdb_malloc_metric_id_buffer(void **ptr , int num)
{
  MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, "TSDB:LOGGING tsdb_malloc_metric_id_buffer Method Called!, num_graph %d",num);
//  *ptr = tsdb_malloc_metric_id_buffer(num);
}


inline void ns_tsdb_free_metric_id_buffer(void **ptr)
{
  MLTL2(EL_DF, 0, 0, _FLN_, NULL, EID_DATAMON_GENERAL, EVENT_INFORMATION, "TSDB:LOGGING tsdb_free_metric_id_buffer Method Called!");
  //tsdb_free_metric_id_buffer(ptr);
}
