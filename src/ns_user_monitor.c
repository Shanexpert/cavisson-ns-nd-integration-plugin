/**
 * File: ns_user_monitor.c
 * Date: Mon, Nov 17, 2008
 * Purpose: This file contains all the code related to User Monitors.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <regex.h>

#include "url.h"
#include "ns_byte_vars.h"
#include "ns_nsl_vars.h"
#include "ns_search_vars.h"
#include "ns_cookie_vars.h"
#include "ns_check_point_vars.h"

#include "ns_static_vars.h"
#include "ns_msg_def.h"
#include "ns_check_replysize_vars.h"
#include "ns_error_codes.h"
#include "ns_server.h"
#include "ns_log.h"
#include "ns_user_monitor.h"
#include "util.h"
#include "ns_test_gdf.h"
#include "ns_gdf.h"
#include "ns_alloc.h"
#include "ns_percentile.h"
#include "ns_trace_level.h"
#include "ns_event_id.h"
#include "ns_event_log.h"
#include "ns_string.h"
#include "nslb_util.h"
#include "ns_custom_monitor.h"
#include "ns_check_monitor.h"
#include "ns_dynamic_vector_monitor.h"
#include "ns_mon_log.h"


//extern void end_test_run( void );

#define SET_DEF_MIN 1.7976931348623158e+308
#define SET_DEF_MAX  0

#define SET_MIN(a , b)                          \
  if ( (b) < (a)) (a) = (b)

#define SET_MAX(a , b)                          \
  if ( (b) > (a)) (a) = (b)

#define IS_VECTOR           0
#define IS_SCALAR           1

extern void fprint2f(FILE *fp1, FILE *fp2, char * format, ...);
//extern avgtime *average_time;
#ifndef CAV_MAIN
extern int g_avgtime_size;
int total_um_entries = 0; /* global */
UM_info *um_info = NULL;
#else
extern __thread int g_avgtime_size;
__thread int total_um_entries = 0; /* global */
__thread UM_info *um_info = NULL;
#endif
extern Group_Info *group_data_ptr;
extern Graph_Info *graph_data_ptr;
extern int group_count;
extern int graph_count_mem;
extern double convert_data_by_formula_print(char formula, double data);
extern int sgrp_used_genrator_entries;

int total_um_data_entries = 0;
UM_data *um_data = NULL;


static void usage()
{
  fprintf(stderr, "Error: Invalid arguments\n");
  fprintf(stderr, "Usage: USER_MONITOR <gdf name>\n");
  NS_EXIT(-1,"");
}

// Function to check if group type is scalar or vector
static int is_group_vector(char *gdf_name)
{
  FILE *read_gdf_fp;
  char line[MAX_LINE_LENGTH];
  char tmpbuff[MAX_LINE_LENGTH];
  char *buffer[GDF_MAX_FIELDS];
  int i = 0; 
    
  //reset

  if(strncmp(gdf_name,"NA", 2))
    read_gdf_fp = open_gdf(gdf_name);
  else
    return -1;
    
  while (fgets(line, MAX_LINE_LENGTH, read_gdf_fp) !=NULL)
  {
    line[strlen(line) - 1] = '\0';
    if((sscanf(line, "%s", tmpbuff) == -1) || line[0] == '#' || line[0] == '\0' || !(strncasecmp(line, "info|", strlen("info|"))) || !(strncasecmp(line, "graph|", strlen("graph|"))))
      continue;
    else if(!(strncasecmp(line, "group|", strlen("group|"))))
    {
      i = get_tokens(line, buffer, "|", GDF_MAX_FIELDS);

      if(i != 9)
      {
        ns_um_monitor_log(EL_F, 0, 0, _FLN_, NULL,
                              EID_DATAMON_ERROR, EVENT_CRITICAL,
                              "Error: For User Monitor no. of field in Group line is not 9. Line = %s", tmpbuff);
        continue;
      }
  
      if(!strcasecmp(buffer[3], "vector"))
        return IS_VECTOR;
      else if(!strcasecmp(buffer[3], "scalar"))
        return IS_SCALAR;
    }
  }
  return -1;
}
                                             
//called by parent
void update_um_data_avgtime_size()
{
  NSDL1_MON(NULL, NULL, "Method Called, g_avgtime_size = %d, g_avg_um_data_idx = %d", g_avgtime_size, g_avg_um_data_idx);
  g_avg_um_data_idx = g_avgtime_size;
  g_avgtime_size += (sizeof(UM_data) * total_um_data_entries);
  NSDL1_MON(NULL, NULL, "After g_avgtime_size = %d, g_avg_um_data_idx = %d",
                                          g_avgtime_size, g_avg_um_data_idx);
}

// called by parent
void update_um_data_cavgtime_size()
{

  NSDL1_MON(NULL, NULL, "Method Called, g_cavgtime_size = %d", g_cavgtime_size);
  g_cavg_um_data_idx = g_cavgtime_size;
  g_cavgtime_size += (sizeof(UM_data) * total_um_data_entries);
  NSDL1_MON(NULL, NULL, "After g_cavgtime_size = %d, g_cavg_um_data_idx = %d",
                                          g_cavgtime_size, g_cavg_um_data_idx);
}

static int add_user_monitor(char *gdf_name)
{
  int i;
  
  NSDL2_MON(NULL, NULL, "Method called. Adding %s, in um_info array at position %d", gdf_name, total_um_entries);

  /* check if already existing in the list */
  for (i = 0; i < total_um_entries; i++) {
    if (strcmp(um_info[i].gdf_name, gdf_name) == 0) { /* Found - exit */
      error_log("Can not use same gdf(%s) twice.", gdf_name);
      NS_EXIT(-1,"Can not use same gdf(%s) twice.\n", gdf_name);
    }
  }

  i = is_group_vector(gdf_name);
  if(i != IS_SCALAR)
  {
    ns_um_monitor_log(EL_F, 0, 0, _FLN_, um_info,
                              EID_DATAMON_ERROR, EVENT_CRITICAL,
                              "Error: Group type for user monitor must be scalar. Hence ignoring this gdf : %s", gdf_name);
    return -1;
  }

  total_um_entries++;
  MY_REALLOC_AND_MEMSET(um_info, (total_um_entries * sizeof(UM_info)), ((total_um_entries - 1) * sizeof(UM_info)), "um_info", -1);
  strncpy(um_info[total_um_entries - 1].gdf_name, gdf_name, 
          sizeof(um_info[total_um_entries].gdf_name) - 1);

  NSDL2_MON(NULL, NULL, "Added %s, in um_info array at position %d", gdf_name, total_um_entries - 1);

  um_info[total_um_entries - 1].log_once = 0;
  return 0;
}


void user_monitor_config(char *keyword, char *buf)
{
  char key[1024] = "";
  char gdf_name[1024] = "";
  char gdf_file[1024];
  int num;
  struct stat sbuf;

  
  NSDL2_MON(NULL, NULL, "Method called, keyword = %s, buf = %s", keyword, buf);
  if (strcasecmp(keyword, "USER_MONITOR") != 0) return;

  num = sscanf(buf, "%s %s", key, gdf_name);

  if (stat(gdf_name, &sbuf) == -1)
    sprintf(gdf_file, "%s/sys/%s", g_ns_wdir, gdf_name);
  else 
    sprintf(gdf_file, "%s", gdf_name);

  if(num < 2) // All fields except arguments are mandatory. 
    usage("Error: Too few arguments for Custom Monitor\n");

  /* check if gdf_name exists with or without .gdf extension XXXXX:TODO:BHAV */
  add_user_monitor(gdf_file);
}


void process_user_monitor_gdf() 
{
  int i;
  NSDL1_MON(NULL, NULL, "Method called. total_um_entries = %d", total_um_entries);
  for (i = 0; i < total_um_entries; i++) {
    um_info[i].group_idx = group_count;
    process_gdf(um_info[i].gdf_name, 1, NULL, 0);
  }
}

void fill_um_group_info(char *gdf_name, char *groupName, int rpGroupID, int numGraph) 
{
  int i;
  int tmp_id;

  NSDL2_MON(NULL, NULL, "Method called. gdf_name = %s, groupName = %s, rpGroupID = %d, numGraph = %d", 
             gdf_name, groupName, rpGroupID, numGraph);

  if ((tmp_id = check_if_user_monitor(rpGroupID)) != -1) { /* ID already exists exit */
    error_log("Can not use same group id. gdf [%s] and [%s] have same group id %d", 
              gdf_name, um_info[tmp_id].gdf_name, rpGroupID);
    NS_EXIT(-1,"Can not use same group id. gdf [%s] and [%s] have same group id %d\n",
            gdf_name, um_info[tmp_id].gdf_name, rpGroupID);
  }

  /* find and insert */
  for (i = 0; i < total_um_entries; i++) {
    if (strcmp(um_info[i].gdf_name, gdf_name) == 0) {
      um_info[i].rpt_group_id = rpGroupID;
      um_info[i].number_of_graphs = numGraph;
      um_info[i].dindex = total_um_data_entries;
      total_um_data_entries += numGraph;
      NSDL2_MON(NULL, NULL, "total_um_entries = %d, gdf_name = %s, um_info[i].gdf_name = %s, numGraph = %d, "
                 "total_um_data_entries = %d\n", 
                 total_um_entries, gdf_name, um_info[i].gdf_name, numGraph, total_um_data_entries);
      MY_REALLOC(um_data, total_um_data_entries * sizeof(UM_data), "um_data", -1);
      return;
    }
  }
  
  /* not found - this can not happen because we have already 
   * sorted them out in the beginning */
}

static int get_graph_idx(int um_group_idx)
{
  Group_Info *local_group_data_ptr = NULL;
  
  local_group_data_ptr = group_data_ptr + um_info[um_group_idx].group_idx;
  return graph_count_mem - local_group_data_ptr->graph_info_index;
}

void fill_um_graph_info(char *graph_name, int um_group_idx, int rpGraphID, char data_type)
{
  UM_data *local_um_data;
  int gidx;
  
  gidx = get_graph_idx(um_group_idx);

  int data_idx = um_info[um_group_idx].dindex + gidx;/* rpGraphID - 1 */;

  NSDL2_MON(NULL, NULL, "Method called. "
             "data_idx = %d, graph_name = %s, rpGraphID = %d, um_info[%d].dindex = %d, data_type = %d\n", 
             data_idx, graph_name, rpGraphID, um_group_idx, um_info[um_group_idx].dindex, data_type);

  local_um_data = &um_data[data_idx];

  local_um_data->data_type = data_type;
  local_um_data->rpt_id = rpGraphID;
  local_um_data->actual_idx = gidx;

  /* default values to other data */
  local_um_data->count = 0;
  local_um_data->min = SET_DEF_MIN;
  local_um_data->max = SET_DEF_MAX;
  local_um_data->sum_of_squares = DATA_NOT_VALID;
  local_um_data->cum_value = DATA_NOT_VALID;
}

/**
 * Function checks if the group id is already registerd if yes, it returns its idx in um_info
 * else returns -1
 */
int check_if_user_monitor(int rpGroupID) 
{
  int i;
  NSDL2_MON(NULL, NULL, "Method called. rpGroupID = %d", rpGroupID);

  for (i = 0; i < total_um_entries; i++) {
    if (um_info[i].rpt_group_id == rpGroupID) {
      NSDL2_MON(NULL, NULL, "found at %d\n", i);
      return i;
    }
  }

  /* not found */
  return -1;
}


/* Copy intelligently and reset the values */
/* Copy user monitor data from UM data buffer to avg time */
/* Called by NVM child */
void insert_um_data_in_avg_time(UM_data *dest, UM_data *src)
{
  UM_data *avg_um_data = dest;
  UM_data *local_um_data = src;
  
  int i;

  NSDL2_MON(NULL, NULL, "Method called.");

  for (i = 0; i < total_um_data_entries; i++) {
    avg_um_data = &dest[i];
    local_um_data = &src[i];

    /* copy */
    avg_um_data->data_type = local_um_data->data_type;
    avg_um_data->rpt_id = local_um_data->rpt_id;
    // Copy common fields */
    avg_um_data->cum_value = local_um_data->cum_value;
    /* We need to fill count for all as this is checked by parent of data is valid or not */
    avg_um_data->count = local_um_data->count;

    switch (local_um_data->data_type) {
    case DATA_TYPE_SAMPLE:
    case DATA_TYPE_SUM:
    case DATA_TYPE_RATE:
    case DATA_TYPE_CUMULATIVE:

      /* clear */
      local_um_data->cum_value = DATA_NOT_VALID;
      local_um_data->count = 0; // Cleare for the next sample
      break;

    case DATA_TYPE_TIMES:
      avg_um_data->min = local_um_data->min;
      avg_um_data->max = local_um_data->max;

      /* clear */
      local_um_data->count = 0; // Cleare for the next sample
      local_um_data->cum_value = DATA_NOT_VALID;
      local_um_data->min = SET_DEF_MIN;
      local_um_data->max = SET_DEF_MAX;
      break;

    case DATA_TYPE_TIMES_STD:
      avg_um_data->min = local_um_data->min;
      avg_um_data->max = local_um_data->max;
      avg_um_data->sum_of_squares = local_um_data->sum_of_squares;

      local_um_data->count = 0;
      local_um_data->cum_value = DATA_NOT_VALID;
      local_um_data->min = SET_DEF_MIN;
      local_um_data->max = SET_DEF_MAX;
      local_um_data->sum_of_squares = DATA_NOT_VALID;
      break;
    }
  }
}

/* This function accumulate(src to dest) only cummulative data for cummulative graph only.*/
void calculate_um_data_in_cavg(UM_data *dest, UM_data *src)
{
  UM_data *cavg_um_data;
  UM_data *local_um_data;

  int i;

  NSDL1_MON(NULL, NULL, "Method called.");
  for (i = 0; i < total_um_data_entries; i++) 
  {
    cavg_um_data = &dest[i];
    local_um_data = &src[i]; 
 
    NSDL2_MON(NULL, NULL, "User Monitor - Before filling, cumlative value is = %f, data_type = %d", 
                           cavg_um_data->cum_value, cavg_um_data->data_type);
    /* Count is always updated even if DATA_NOT_VALID as count will still be zero. so this is ok */
    /* Count is not used in case of rate as rate is calcualted using formula */
    if (local_um_data->data_type == DATA_TYPE_CUMULATIVE) 
    {
      cavg_um_data->count += local_um_data->count;
      cavg_um_data->data_type = local_um_data->data_type;
      cavg_um_data->rpt_id = local_um_data->rpt_id;

      if (cavg_um_data->cum_value == DATA_NOT_VALID) // parent data is not valid, so set it
        cavg_um_data->cum_value = local_um_data->value; // child data can also be nogt valid. But we can set it
      if (local_um_data->cum_value != DATA_NOT_VALID) // parent date is valid and child is data also valid
        cavg_um_data->cum_value += local_um_data->value;
    }
    NSDL2_MON(NULL, NULL, "User Monitor - After filling, cumlative value is = %f, data_type = %d", 
                           cavg_um_data->cum_value, cavg_um_data->data_type);
  }
}

/* Combine UM data from progress report of each child/client. Called by parent/controller */
void fill_um_data_in_avg_and_cavg(UM_data *dest, UM_data *src, UM_data *cum_dest, UM_data *cum_dest_gen)
{
  UM_data *avg_um_data;
  UM_data *src_um_data;
 
  UM_data *cum_um_data; 
  UM_data *cum_gen_um_data; 
  int i;

  NSDL1_MON(NULL, NULL, "Method called.");
  NSDL3_MON(NULL, NULL, "********************UM Data of parent/controller start**********************");
  for (i = 0; i < total_um_data_entries; i++) {
    avg_um_data = &dest[i];
    src_um_data = &src[i];
    cum_um_data = &cum_dest[i];
    if(cum_dest_gen != NULL)
      cum_gen_um_data = &cum_dest_gen[i];

    NSDL3_MON(NULL, NULL, "UM Data from child/clients: data = %d, data_type = %d, "
                          "rpt_id = %d, count = %6.3f, value = %6.3f, min = %6.3f, "
                          "max = %6.3f, sum_of_squares = %6.3f. Cum value = %6.3f", 
                          i, src_um_data->data_type, src_um_data->rpt_id, src_um_data->count, 
                          src_um_data->value, src_um_data->min, src_um_data->max, 
                          src_um_data->sum_of_squares, cum_um_data->cum_value);

    avg_um_data->data_type = src_um_data->data_type;
    avg_um_data->rpt_id = src_um_data->rpt_id;
   
    //For Cummulative graph data
    cum_um_data->data_type = src_um_data->data_type;
    cum_um_data->rpt_id = src_um_data->rpt_id;
    if(cum_dest_gen != NULL)
      cum_gen_um_data->rpt_id += src_um_data->rpt_id;

    /* Count is always updated even if DATA_NOT_VALID as count will still be zero. so this is ok */
    /* Count is not used in case of rate as rate is calcualted using formula */
    avg_um_data->count += src_um_data->count; 
    cum_um_data->count += src_um_data->count;
    if(cum_dest_gen != NULL)
      cum_gen_um_data->count += src_um_data->count;
    

    switch (src_um_data->data_type) {
      case DATA_TYPE_SAMPLE:
      case DATA_TYPE_SUM:
      case DATA_TYPE_RATE: 
        if (avg_um_data->value == DATA_NOT_VALID) { // parent data is not valid, so set it
          avg_um_data->value = src_um_data->value; // child data can also be nogt valid. But we can set it
        }
        else if (src_um_data->value != DATA_NOT_VALID) // parent date is valid and child is data also valid
          avg_um_data->value += src_um_data->value;

        break;

      case DATA_TYPE_CUMULATIVE:
        //here we can fill cummulive value from src to cum_dest 
        if (cum_um_data->cum_value == DATA_NOT_VALID) { // parent data is not valid, so set it
          cum_um_data->cum_value = src_um_data->value; // child data can also be nogt valid. But we can set it
          NSDL3_MON(NULL, NULL, "fill by parent cumlative value = %f, avg cum_vlaue = %f", cum_um_data->cum_value, src_um_data->value);
        }
        else if (src_um_data->value != DATA_NOT_VALID) // parent date is valid and child is data also valid
          cum_um_data->cum_value += src_um_data->value;

        //here we can fill cummulative value form src to dest  
        if (avg_um_data->cum_value == DATA_NOT_VALID) { // parent data is not valid, so set it
          avg_um_data->cum_value = src_um_data->value; // child data can also be nogt valid. But we can set it
          NSDL3_MON(NULL, NULL, "fill by parent cumlative value = %f, avg cum_vlaue = %f", avg_um_data->cum_value, src_um_data->value);
        }
        else if (src_um_data->value != DATA_NOT_VALID) // parent date is valid and child is data also valid
          avg_um_data->cum_value += src_um_data->value;

        //here we can fill cummlative value from src to cum_dest_gen.
        //This for because we can accumlate the data for particular generator also.
        if(cum_dest_gen != NULL)
        {
        if (cum_gen_um_data->cum_value == DATA_NOT_VALID) { // parent data is not valid, so set it
          cum_gen_um_data->cum_value = src_um_data->value; // child data can also be nogt valid. But we can set it
          NSDL3_MON(NULL, NULL, "fill by parent cumlative value = %f, avg cum_vlaue = %f", cum_gen_um_data->cum_value, src_um_data->value);
        }
        else if (src_um_data->value != DATA_NOT_VALID) // parent date is valid and child is data also valid
          cum_gen_um_data->cum_value += src_um_data->value;
        }
        break;
 
      case DATA_TYPE_TIMES:
        if (avg_um_data->value == DATA_NOT_VALID) {
          avg_um_data->value = src_um_data->value;
          avg_um_data->min = src_um_data->min;
          avg_um_data->max = src_um_data->max;
        } else if (src_um_data->value != DATA_NOT_VALID) {
          avg_um_data->value += src_um_data->value;
          SET_MIN(avg_um_data->min, src_um_data->min);
          SET_MAX(avg_um_data->max, src_um_data->max);
        }
     
        break;
     
      case DATA_TYPE_TIMES_STD:
        if (avg_um_data->value == DATA_NOT_VALID) {
          avg_um_data->value = src_um_data->value;
          avg_um_data->min = src_um_data->min;
          avg_um_data->max = src_um_data->max;
          avg_um_data->sum_of_squares = src_um_data->sum_of_squares;
        } else if (src_um_data->value != DATA_NOT_VALID) {
          avg_um_data->value += src_um_data->value;
          SET_MIN(avg_um_data->min, src_um_data->min);
          SET_MAX(avg_um_data->max, src_um_data->max);
          avg_um_data->sum_of_squares += src_um_data->sum_of_squares;
        }
     
        break;
      }
     
      NSDL3_MON(NULL, NULL, "UM Data of parent/controller: data = %d, avg data_type = %d, rpt_id = %d, count = %6.3f, value = %6.3f, "
                 "cum data_type = %d, cum_value = %f, min = %6.3f, max = %6.3f, sum_of_squares = %6.3f", 
                 i, avg_um_data->data_type, avg_um_data->rpt_id, avg_um_data->count, 
                 avg_um_data->value, cum_um_data->data_type, cum_um_data->cum_value, avg_um_data->min, avg_um_data->max, 
                 avg_um_data->sum_of_squares);
  }
  NSDL3_MON(NULL, NULL, "********************UM Data of parent/controller end**********************");
}

/* Log UM data in progress report. Called by parent/controller */
void print_um_data(FILE *fp1, FILE *fp2, UM_data *local_um_data, UM_data *local_cavg_um_data)
{
  int i, j;
  
  UM_info *local_um_group;
  UM_data *local_um_graph;
  
  UM_data *local_cavg_um_graph;

  char formula = -1;
  //char cum_formula = -1;

  NSDL2_MON(NULL, NULL, "Method called.");

  for (i = 0; i < total_um_entries; i++) {
    local_um_group = &um_info[i];
 
    fprint2f(fp1, fp2, "    %s:\n", get_gdf_group_name(local_um_group->group_idx));
    for (j = 0; j < local_um_group->number_of_graphs; j++) 
    {
      local_um_graph = &local_um_data[local_um_group->dindex + j];
      formula = get_gdf_graph_formula(local_um_group->group_idx, j);

      //Print for cummulative graph
      local_cavg_um_graph = &local_cavg_um_data[local_um_group->dindex + j];
      //cum_formula = get_gdf_graph_formula(local_um_group->group_idx, j); 

      fprint2f(fp1, fp2, "        %s: ", get_gdf_graph_name(local_um_group->group_idx, j));
      
      switch (local_um_graph->data_type) {
      case DATA_TYPE_SAMPLE:
      case DATA_TYPE_SUM:

        if (local_um_graph->value == DATA_NOT_VALID)
          fprint2f(fp1, fp2, "NA");
        else
          fprint2f(fp1, fp2, "%6.3f", convert_data_by_formula_print(formula, 
                                                 local_um_graph->count ?  local_um_graph->value / local_um_graph->count : 0));
        break;

      case DATA_TYPE_RATE:
        //Default formula for rate datatype is PS.
        if(formula == -1)
        {
          formula = FORMULA_PS;
          if(local_um_group->log_once == 0)
          {
            ns_um_monitor_log(EL_F, 0, 0, _FLN_, local_um_group,
                          EID_DATAMON_GENERAL, EVENT_INFORMATION,
                          "For user monitor gdf (%s), formula for rate datatype was not mentioned. Assigning formula as per second.",
                                 local_um_group->gdf_name);
            local_um_group->log_once = 1;
          }
        }

        if (local_um_graph->cum_value == DATA_NOT_VALID)
          fprint2f(fp1, fp2, "NA");
        else
          fprint2f(fp1, fp2, "%6.3f",
                   convert_data_by_formula_print(formula,
                                                 local_um_graph->cum_value
/*                                             local_um_graph->count ?  */
/*                                             local_um_graph->cum_value / local_um_graph->count : 0 */));

        case DATA_TYPE_CUMULATIVE:
        if (local_cavg_um_graph->cum_value == DATA_NOT_VALID)
          fprint2f(fp1, fp2, "NA");
        else
          fprint2f(fp1, fp2, "%6.3f", convert_data_by_formula_print(formula, local_cavg_um_graph->cum_value));
        break;

      case DATA_TYPE_TIMES:
        if (local_um_graph->value == DATA_NOT_VALID)
          fprint2f(fp1, fp2, "NA  min=NA  max=NA  count=NA");
        else
          fprint2f(fp1, fp2, "%6.3f   min=%6.3f   max=%6.3f  count=%.0f",
                   convert_data_by_formula_print(formula, local_um_graph->count ? 
                                                 local_um_graph->value / local_um_graph->count : 0),
                   convert_data_by_formula_print(formula, local_um_graph->min),
                   convert_data_by_formula_print(formula, local_um_graph->max), 
                   local_um_graph->count);
        break;

      case DATA_TYPE_TIMES_STD:
        if (local_um_graph->value == DATA_NOT_VALID)
          fprint2f(fp1, fp2, "NA  min=NA  max=NA  count=NA");
        else
          fprint2f(fp1, fp2, "%6.3f   min=%6.3f   max=%6.3f  count=%.0f",
                   convert_data_by_formula_print(formula, local_um_graph->count ? 
                                                 local_um_graph->value / local_um_graph->count : 0),
                   convert_data_by_formula_print(formula, local_um_graph->min),
                   convert_data_by_formula_print(formula, local_um_graph->max), 
                   local_um_graph->count);

        break;
      }
      
      fprint2f(fp1, fp2, "\n"); 
    }
  }
}

/* Fill data in RTG buffer from Avg. and c_avg. In case of Netcloud, this is done for Overall and for each generator */
/* Called by parent/controller */
void fill_um_data(avgtime **g_avg, cavgtime **c_avg)
{
  int i, j, gv_idx;
  char *block_ptr = NULL;

  Group_Info *local_group_data_ptr = NULL;
  Graph_Info *local_graph_data_ptr = NULL;
  UM_data *avg_um_data_ptr;
  UM_data *cavg_um_data_ptr;

  int gp_info_index;
  int c_graph;
  int c_group_numvec = 0; // This is 0 in NS. In case of NetCloud, it will be 0 for contoller, 1,2,3 .. for generators
  int c_graph_numvec = 0; // This is always 0

  double um_avg_value;
  double um_min;
  double um_max;
  double um_count;
  double um_sum_of_sqr;
  avgtime *avg = NULL;
  cavgtime *cavg = NULL;
  int data_valid;

  NSDL2_MON(NULL, NULL, "Method called.");

  // Loop thru all user monitors used in the scenario
  for (i = 0; i < total_um_entries; i++)
  {
    //TODO: how gp_info_index variable worked for generator's?
    gp_info_index = um_info[i].group_idx;
    local_group_data_ptr = group_data_ptr + gp_info_index;
    local_graph_data_ptr = graph_data_ptr + local_group_data_ptr->graph_info_index;
    // Point to the address in msg data ptr (rtg buf) of first graph data of user monitor 
    block_ptr = msg_data_ptr + local_graph_data_ptr->graph_msg_index;

    NSDL2_MON(NULL, NULL, "Fill data for User Monitor %d. GDF Name = %s, group_idx = %d, dindex = %d, rpt_group_id = %d, number_of_graphs = %d, "
                           "local_group_data_ptr = %p, local_graph_data_ptr = %p. block_ptr = %p, group_data_ptr = %p, graph_data_ptr = %p", 
                           i, um_info[i].gdf_name, um_info[i].group_idx, um_info[i].dindex, um_info[i].rpt_group_id, um_info[i].number_of_graphs,
                           local_group_data_ptr, local_graph_data_ptr, block_ptr, group_data_ptr, graph_data_ptr);

    // Loop thru all avg time (first is for contoller/parent and others are for generators
    // In case of NS, sgrp_used_genrator_entries is 0
    c_group_numvec = 0; // Must be reset here
    for(gv_idx = 0; gv_idx < sgrp_used_genrator_entries + 1; gv_idx++, c_group_numvec++)
    {
      avg = (avgtime *)g_avg[gv_idx]; // Point to the correct avg (Parent/Controller or generator)
      cavg = (cavgtime *)c_avg[gv_idx]; // Point to the correct cavg (Parent/Controller or generator)

      avg_um_data_ptr = (UM_data *)((char *)avg + g_avg_um_data_idx); // Point to the start of UM data in avg
      avg_um_data_ptr = avg_um_data_ptr + um_info[i].dindex; // Point to the UM data in avg for the UM being processed in the loop

      cavg_um_data_ptr = (UM_data *)((char *)cavg + g_cavg_um_data_idx); // Point to the start of UM data in cavg
      cavg_um_data_ptr = cavg_um_data_ptr + um_info[i].dindex; // Point to the UM data in cavg for the UM being processed in the loop

      NSDL2_MON(NULL, NULL, "User Monitor %d, gv_idx = %d, avg = %p, cavg = %p, dindex = %d, avg_um_data_ptr = %p, cavg_um_data_ptr = %p, "
                            "g_avg_um_data_idx = %d, g_cavg_um_data_idx = %d", i, gv_idx, avg, cavg, um_info[i].dindex, 
                             avg_um_data_ptr, cavg_um_data_ptr, g_avg_um_data_idx, g_cavg_um_data_idx);

      // Loop thru all graphs the user monitor
      for (j = 0; j < (um_info[i].number_of_graphs); j++) 
      {
        c_graph = j;
      
        um_avg_value = (double)avg_um_data_ptr->value;
        um_min = (double)avg_um_data_ptr->min;
        um_max = (double)avg_um_data_ptr->max;
        um_count = (double)avg_um_data_ptr->count;
        um_sum_of_sqr = (double)avg_um_data_ptr->sum_of_squares;
     
        NSDL2_MON(NULL, NULL, "User Monitor %d, gv_idx = %d, graph index = %d, um_avg_value = %f, um_min = %f, "
                              "um_max = %f, um_count = %f, um_sum_of_sqr = %f, avg type = %d", 
                               i, gv_idx, j, um_avg_value, um_min, um_max, um_count, um_sum_of_sqr, avg_um_data_ptr->data_type);
        if(um_count == 0) 
        {
          NSDL2_MON(NULL, NULL, "User Monitor %d, graph index = %d, Count is 0", i, j);
          data_valid = 0;
          um_avg_value = um_min = um_max = 0;
        } 
        else
          data_valid = 1;
      
        switch(avg_um_data_ptr->data_type)
        {
          case DATA_TYPE_SAMPLE:
            if(um_count) um_avg_value = um_avg_value / um_count; // Calculate average value of all samples
            *((Long_data *)(block_ptr)) =  ((Long_data)um_avg_value);
            if (data_valid)
              update_gdf_data(gp_info_index, c_graph, c_group_numvec, c_graph_numvec, um_avg_value);
            block_ptr += sizeof(Long_data);
            break;
       
          case DATA_TYPE_RATE: // Formula in GDF will convert to rate which is done by dashboard GUI
          case DATA_TYPE_SUM:
            *(Long_data *)(block_ptr) = um_avg_value; //This can be 
            if (data_valid)
              update_gdf_data(gp_info_index, c_graph, c_group_numvec, c_graph_numvec, um_avg_value);
            block_ptr += sizeof(Long_long_data);
            break;

          //For cummlative graph - we can fill data in cavg_um_data_ptr
          case DATA_TYPE_CUMULATIVE:
            *(Long_data *)(block_ptr) = (double)cavg_um_data_ptr->cum_value;
            NSDL3_MON(NULL, NULL, "value = %f, data_valid = %d", (double)cavg_um_data_ptr->cum_value, data_valid);
            if (data_valid)
              update_gdf_data(gp_info_index, c_graph, c_group_numvec, c_graph_numvec, (double)cavg_um_data_ptr->cum_value);
            block_ptr += sizeof(Long_long_data);
            break;

          case DATA_TYPE_TIMES:
            if(um_count) um_avg_value = um_avg_value / um_count;
            *((Long_data *)(block_ptr)) =  (um_avg_value);
            block_ptr += sizeof(Long_data);
            *((Long_data *)(block_ptr)) =  (um_min);
            block_ptr += sizeof(Long_data);
            *((Long_data *)(block_ptr)) =  (um_max);
            block_ptr += sizeof(Long_data);
            *((Long_data *)(block_ptr)) =  (um_count);
            block_ptr += sizeof(Long_data);
       
            if (data_valid)
              update_gdf_data_times(gp_info_index, c_graph, c_group_numvec, c_graph_numvec,
                                    (Long_long_data)um_avg_value, (Long_long_data)um_min,
                                    (Long_long_data)um_max, (Long_long_data)um_count);
            break;
       
          case DATA_TYPE_TIMES_STD:
            if(um_count) um_avg_value = um_avg_value / um_count;
            *((Long_data *)(block_ptr)) =  (um_avg_value);
            block_ptr += sizeof(Long_data);
            *((Long_data *)(block_ptr)) =  (um_min);
            block_ptr += sizeof(Long_data);
            *((Long_data *)(block_ptr)) =  (um_max);
            block_ptr += sizeof(Long_data);
            *((Long_data *)(block_ptr)) =  (um_count);
            block_ptr += sizeof(Long_data);
            *((Long_data *)(block_ptr)) =  (um_sum_of_sqr);
            block_ptr += sizeof(Long_data);
       
            if (data_valid)
              update_gdf_data_times_std(gp_info_index, c_graph, c_group_numvec, c_graph_numvec,
                                        (Long_long_data)um_avg_value, (Long_long_data)um_min,
                                        (Long_long_data)um_max, (Long_long_data)um_count,
                                        um_sum_of_sqr);
            break;
          default:
            fprintf(stderr,"Error: User Monitor dataType (%d) in GDF is not correct. Ignored\n", avg_um_data_ptr->data_type);
            // NS_EXIT(-1,"Default Message");
        }//Switch
        avg_um_data_ptr++;
        cavg_um_data_ptr++;
      }
    }
  }
}

void init_um_data(UM_data *ud)
{
  //  memcpy(ud, um_data, sizeof(UM_data) * total_um_data_entries);
  UM_info *local_um_info;
  UM_data *local_um_data;
  int i, j;
    
  NSDL2_MON(NULL, NULL, "Method called.");
  //  memset(ud, -1, (sizeof(UM_data) * total_um_data_entries));
  for (i = 0; i < total_um_entries; i++) {
    local_um_info = &um_info[i];
    for (j = 0; j < local_um_info->number_of_graphs; j++) {
      local_um_data = &ud[local_um_info->dindex + j];
      
      local_um_data->cum_value = DATA_NOT_VALID;
      local_um_data->min = SET_DEF_MAX;
      local_um_data->max = SET_DEF_MIN;
      local_um_data->count = 0;
      local_um_data->sum_of_squares = DATA_NOT_VALID;
    }
  }
}

/* Called from C API  from flow files */
int add_user_data_point(int rptGroupID, int rptGraphID, double value)
{
  int um_group_idx = 0;
  UM_data *local_um_data = NULL;
  UM_info *local_um_info = NULL;
  int actual_idx;
  
  NSDL1_MON(NULL, NULL, "Method called. rptGroupID = %d, rptGraphID = %d, value = %f", rptGroupID, rptGraphID, value);
  if ((um_group_idx = check_if_user_monitor(rptGroupID)) == -1) {
    error_log("User monitor - Group ID %d passed in ns_add_user_data_point() is not valid", rptGroupID);
    NS_EXIT(-1,"User monitor - Group ID %d passed in ns_add_user_data_point() is not valid\n", rptGroupID);
  }
  
  local_um_info = &um_info[um_group_idx];
  

  /* This is done in order to avoid gaps between graph id 
   * sequence or invalid graph id in GDF.*/
  for (actual_idx = 0; actual_idx < local_um_info->number_of_graphs; actual_idx++) {
    if (um_data[local_um_info->dindex + actual_idx].rpt_id == rptGraphID) {
      local_um_data = &um_data[local_um_info->dindex + actual_idx];
      break;
    }
  }

  if (actual_idx == local_um_info->number_of_graphs) {
    error_log("User monitor - Graph ID %d passed in ns_add_user_data_point() is not valid", rptGraphID);
    NS_EXIT(-1,"User monitor - Graph ID %d passed in ns_add_user_data_point() is not valid\n", rptGraphID);
  }

  /*We are increasing count every time. 
    But no need in RATE and CUMULATIVE but we need to check if sample is valid or not using count */
  local_um_data->count++;

  switch(local_um_data->data_type) {
  case DATA_TYPE_SAMPLE:
  case DATA_TYPE_SUM:
  case DATA_TYPE_RATE:
  case DATA_TYPE_CUMULATIVE:
    if (local_um_data->cum_value == DATA_NOT_VALID)
      local_um_data->cum_value = (double)value;
    else
      local_um_data->cum_value += (double)value;
    break;

  case DATA_TYPE_TIMES:
    if (local_um_data->cum_value == DATA_NOT_VALID) {
      local_um_data->cum_value = (double)value;
      local_um_data->min = value; /* Assuming value will always be >= 0 */
      local_um_data->max = value;
    } else {
      local_um_data->cum_value += (double)value;
      SET_MIN(local_um_data->min, value);
      SET_MAX(local_um_data->max, value);
    }
    break;

  case DATA_TYPE_TIMES_STD:
    if (local_um_data->cum_value == DATA_NOT_VALID) {
      local_um_data->cum_value = (double)value;
      local_um_data->min = value; /* Assuming value will always be >= 0 */
      local_um_data->max = value;
      local_um_data->sum_of_squares = (value * value);
    } else {
      local_um_data->cum_value += (double)value;
      SET_MIN(local_um_data->min, value);
      SET_MAX(local_um_data->max, value);
      local_um_data->sum_of_squares += (value * value);
    }
    break;
  }
  
  /* Fill in percentile data */
  if (g_percentile_report == 1) {
    Group_Info *local_group_data_ptr = NULL;
    Graph_Info *local_graph_data_ptr = NULL;

    local_group_data_ptr = group_data_ptr + local_um_info->group_idx;
    local_graph_data_ptr = graph_data_ptr + local_group_data_ptr->graph_info_index + actual_idx;
    if (local_graph_data_ptr->pdf_info_idx != -1)
      update_pdf_data(value, local_graph_data_ptr->pdf_info_idx, 0, 0, 0);
  }
  
  return 0;
}

void dump_um_data (int gen_idx, UM_data *src)
{
   UM_data *local_um_data;
   int i;
   for (i = 0; i < total_um_data_entries; i++) {
    local_um_data = &src[i];
    NSDL3_MON(NULL, NULL, "UM Data from child/clients: data_type = %d, rpt_id = %d, count = %6.3f, cum_value = %6.3f, min = %6.3f, "
               "max = %6.3f, sum_of_squares = %6.3f",
               local_um_data->data_type, local_um_data->rpt_id, local_um_data->count,
               local_um_data->cum_value, local_um_data->min, local_um_data->max,
               local_um_data->sum_of_squares);

    fprintf(stderr, "GenId = %d, UM Data from child/clients: data_type = %d, rpt_id = %d, count = %6.3f, cum_value = %6.3f, min = %6.3f, "               "max = %6.3f, sum_of_squares = %6.3f\n", gen_idx,
               local_um_data->data_type, local_um_data->rpt_id, local_um_data->count,
               local_um_data->cum_value, local_um_data->min, local_um_data->max,
               local_um_data->sum_of_squares);
   }
}
