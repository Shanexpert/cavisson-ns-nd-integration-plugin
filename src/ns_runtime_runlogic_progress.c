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
#include "ns_runtime_runlogic_progress.h"
#include "ns_error_msg.h"
#include "ns_kw_usage.h"

VUserFlowBasedGP *vuser_flow_gp_ptr = NULL;
VUserFlowData *vuser_flow_data = NULL;
unsigned int show_vuser_flow_gp_idx = -1;
#ifndef CAV_MAIN
unsigned int show_vuser_flow_idx = -1;
VUserFlowAvgTime *vuser_flow_avgtime = NULL;
#else
__thread unsigned int show_vuser_flow_idx = -1;
__thread VUserFlowAvgTime *vuser_flow_avgtime = NULL;
#endif
extern char g_runtime_flag;
int total_flow_path_entries_created = 0;
int total_flow_path_entries = 0;
int max_flow_path_entries = 0;
#define DELTA_VUSER_FLOW_ENTRIES 128
#define INIT_VUSER_FLOW_ENTRIES 128
//int max_flow_path_entries = INIT_VUSER_FLOW_ENTRIES;

//keyword parsing
int kw_set_g_show_vuser_flow(char *buf, GroupSettings *gset, char *err_msg, int runtime_flag)
{   
  char keyword[MAX_DATA_LINE_LENGTH];
  char mode_str[32 + 1];
  char group_str[256];
  char tmp[MAX_DATA_LINE_LENGTH]; //This used to check if some extra field is given
  int num;
  int mode = 0;
        
  num = sscanf(buf, "%s %s %s %s", keyword, group_str, mode_str, tmp);
    
  NSDL2_PARSING(NULL, NULL, "Method called, buf = %s, num= %d , key=[%s], group_str=[%s], mode_str=[%s]", buf, num, keyword, group_str, mode_str);
    
  if(num != 3)
  { 
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_SHOW_RUNTIME_RUNLOGIC_PROGRESS_USAGE, CAV_ERR_1011020, CAV_ERR_MSG_1);
  }

  if(ns_is_numeric(mode_str) == 0)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_SHOW_RUNTIME_RUNLOGIC_PROGRESS_USAGE, CAV_ERR_1011020, CAV_ERR_MSG_2);
  }

  mode = atoi(mode_str);
  if(mode < 0 || mode > 1)
  {
    NS_KW_PARSING_ERR(buf, runtime_flag, err_msg, G_SHOW_RUNTIME_RUNLOGIC_PROGRESS_USAGE, CAV_ERR_1011020, CAV_ERR_MSG_3);
  }

  gset->show_vuser_flow_mode = mode;

  if(mode == 1) //If any group is setting mode 1 then enable this fetaure in global setting
    global_settings->show_vuser_flow_mode = mode;

  NSDL2_PARSING(NULL, NULL, "global_settings->show_vuser_flow_mode = %d", gset->show_vuser_flow_mode);

  return 0;
}

static int create_flow_path_table_entry(int *row_num) {
  NSDL2_MISC(NULL, NULL, "Method called");

  if (total_flow_path_entries_created == max_flow_path_entries) {
    MY_REALLOC_EX (vuser_flow_data, (max_flow_path_entries + DELTA_VUSER_FLOW_ENTRIES) * sizeof(VUserFlowData), max_flow_path_entries * sizeof(VUserFlowData),"vuserFlowData", -1);
    if (!vuser_flow_data) {
      fprintf(stderr,"create_flow_path_table_entry(): Error allocating more memory for vuser flow data\n");
      return(FAILURE);
    } else max_flow_path_entries += DELTA_VUSER_FLOW_ENTRIES;
  }
  *row_num = total_flow_path_entries_created++;
  //runProfTable[*row_num].pacing_idx = -1;
  return (SUCCESS);
}

void fill_vuser_flow_data_struct(int sess_id, char *script_path)
{
  FILE *script_tree_fp = NULL;
  char line[4096] = {0};
  char script_tree_path[4096];
  char *ptr =  NULL;
  int total_flow_path_per_script = 0;
  int row_num = 0;
  int grp_idx = get_group_idx(sess_id);
 
  NSDL2_GDF(NULL, NULL, " Method called, sess_id = %d, script_path = %s", sess_id, script_path);
  if (gSessionTable[sess_id].flags & ST_FLAGS_SCRIPT_NEW_FORMAT)
    sprintf(script_tree_path, "%s/runlogic/.%s.tree.path", script_path, runProfTable[grp_idx].runlogic);
  else
    sprintf(script_tree_path, "%s/.tree.path", script_path);
    
  if((script_tree_fp = fopen(script_tree_path, "r")) == NULL)
  {
    NSDL2_GDF(NULL, NULL, "Parsing script %s, .tree.path does not exists, skipping the script\n", script_tree_path);
    return;
  }

  while(nslb_fgets(line, 1024, script_tree_fp, 0))
  { 
    line[strlen(line) - 1] = '\0';
    if((ptr = strchr(line, ':')) != NULL) {
      *ptr = '\0';
      ptr++;
    }
    create_flow_path_table_entry(&row_num);
    vuser_flow_data[row_num].id = atoi(line);
    strcpy(vuser_flow_data[row_num].flow_path, ptr);
    if(total_flow_path_per_script == 0)
      gSessionTable[sess_id].flow_path_start_idx = row_num;
    total_flow_path_per_script++;
    NSDL2_GDF(NULL, NULL, "id = %d, flow_path = %s, total_flow_path_per_script = %d, total_flow_path_entries = %d", vuser_flow_data[row_num].id, vuser_flow_data[row_num].flow_path, total_flow_path_per_script, total_flow_path_entries);
  }
  gSessionTable[sess_id].num_of_flow_path = total_flow_path_per_script;
  NSDL2_GDF(NULL, NULL, "Parsing: num_of_flow_path = %d", gSessionTable[sess_id].num_of_flow_path);  
}

static void printGroupGraph_ShowVuserFlow(char **TwoD , int *Idx2d, char *prefix)
{
  int i, j;
  int flow_idx;
  char buff[1024];
  char buff2[1024];
  NSDL2_GDF(NULL, NULL, " Method called.");

  for(i = 0; i < total_runprof_entries; i++)
  {
    NSDL2_GDF(NULL, NULL, "num_of_flow_path = %d, group name = %s", gSessionTable[runProfTable[i].sessprof_idx].num_of_flow_path,
                           RETRIEVE_BUFFER_DATA(runProfTable[i].scen_group_name));
    if(g_runtime_flag == 0)
      sprintf(buff, "%s%s", prefix, RETRIEVE_BUFFER_DATA(runProfTable[i].scen_group_name));
    else
      sprintf(buff, "%s%s", prefix, runprof_table_shr_mem[i].scen_group_name);

    //If ith group is not enabled to show runlogic progress just continue
    if (runProfTable[i].gset.show_vuser_flow_mode != SHOW_RUNTIME_RUNLOGIC_PROGRESS_ENABLED)
      continue;

    flow_idx = gSessionTable[runProfTable[i].sessprof_idx].flow_path_start_idx;
    for(j = 0; j < gSessionTable[runProfTable[i].sessprof_idx].num_of_flow_path; j++) {
      NSDL2_GDF(NULL, NULL, "id = %d, flow_path = %s", vuser_flow_data[flow_idx].id, vuser_flow_data[flow_idx].flow_path);
      //G1>0_Block(NS), Controller>Generator>G1>0_Block(NC)
      sprintf(buff2, "%s>%d_%s", buff, vuser_flow_data[flow_idx].id, vuser_flow_data[flow_idx].flow_path);
      fprintf(write_gdf_fp, "%s\n", buff2);
      fill_2d(TwoD, *Idx2d, buff2);
      *Idx2d = *Idx2d  + 1;
      flow_idx++;
    }
  }
}

char **printVuserFlowDataStat()
{       
  int i = 0;
  char **TwoD;
  char prefix[1024];
  int Idx2d = 0;
  NSDL2_MISC(NULL, NULL, "Method Called");
  TwoD = init_2d(total_flow_path_entries * (sgrp_used_genrator_entries + 1));
      
  for(i = 0; i < sgrp_used_genrator_entries + 1; i++)
  {
    getNCPrefix(prefix, i-1, -1, ">", 0); //for controller or NS
    //NSDL2_GDF(NULL, NULL, "in printVuserFlowDataStat prefix is = %s", prefix);
    printGroupGraph_ShowVuserFlow(TwoD, &Idx2d, prefix); 
  }
  return TwoD;
}

// Called by ns_parent.c to update vuser flow data size into g_avgtime_size
inline void update_show_vuser_flow_avgtime_size()
{ 
  NSDL1_MISC(NULL, NULL, "Method Called, g_avgtime_size = %d, show_vuser_flow_idx = %d, total_runprof_entries = %d",
                        g_avgtime_size, show_vuser_flow_idx, total_runprof_entries);
    
  if(SHOW_RUNTIME_RUNLOGIC_PROGRESS)
  {
    NSDL2_MISC(NULL, NULL, "SHOW_RUNTIME_RUNLOGIC_PROGRESS is enabled.");
    show_vuser_flow_idx = g_avgtime_size;
    g_avgtime_size += RUNTIME_RUNLOGIC_PROGRESS_AVGTIME_SIZE;
  } else {
    NSDL2_MISC(NULL, NULL, "SHOW_RUNTIME_RUNLOGIC_PROGRESS is disabled.");
  } 
        
  NSDL2_MISC(NULL, NULL, "After g_avgtime_size = %d, show_vuser_flow_idx = %d",
                  g_avgtime_size, show_vuser_flow_idx);
}   

//called by child
inline void set_vuser_flow_stat_avgtime_ptr()
{
  NSDL1_MISC(NULL, NULL, "Method Called, show_vuser_flow_idx = %d, average_time = %p", show_vuser_flow_idx, average_time);

  if(SHOW_RUNTIME_RUNLOGIC_PROGRESS)
  {
    vuser_flow_avgtime = (VUserFlowAvgTime*)((char *)average_time + show_vuser_flow_idx);
  } else {
    vuser_flow_avgtime = NULL;
  }

  NSDL2_MISC(NULL, NULL, "vuser_flow_avgtime set at address = %p", vuser_flow_avgtime);
}

inline void fill_vuser_flow_gp(avgtime **g_avg)
{
  int i, gen_idx;
  VUserFlowAvgTime *vuser_flow_avg = NULL;
  VUserFlowBasedGP *vuser_flow_local_gp_ptr = vuser_flow_gp_ptr; 
  avgtime *avg;
 
  NSDL1_GDF(NULL, NULL, "Method called, show_vuser_flow_idx = %d, total_runprof_entries = %d", show_vuser_flow_idx, total_runprof_entries);

  for(gen_idx = 0; gen_idx < sgrp_used_genrator_entries + 1; gen_idx++) 
  {
    avg = (avgtime *)g_avg[gen_idx];
    vuser_flow_avg = (VUserFlowAvgTime*)((char *)avg + show_vuser_flow_idx);
    NSDL1_GDF(NULL, NULL, "avg = %p, vuser_flow_avg = %p, total_flow_path_entries = %d", avg, vuser_flow_avg, total_flow_path_entries);   
    for(i = 0; i < total_flow_path_entries; i++)
    {
      NSDL1_GDF(NULL, NULL, "i = %d, cur_vuser_running_flow = %d", i, vuser_flow_avg[i].cur_vuser_running_flow);
      GDF_COPY_VECTOR_DATA(show_vuser_flow_gp_idx, 0, 0, 0, vuser_flow_avg[i].cur_vuser_running_flow,
                           vuser_flow_local_gp_ptr->vuser_running_flow);
      vuser_flow_local_gp_ptr++;
    }
  } 
}

void update_user_flow_count_ex(VUser* vptr, int id)
{
  int grp_num = vptr->group_num;
  int i;
  int size = 0;

//  if (!(SHOW_RUNTIME_RUNLOGIC_PROGRESS_GRP(vptr->group_num))) return;
  
  if(runprof_table_shr_mem[grp_num].gset.show_vuser_flow_mode != 1)
    return; 


  VUserFlowAvgTime *local_vuser_flow_avg;

  NSDL1_PARSING(NULL, NULL, "Method Called, average_time = %p, show_vuser_flow_gp_idx = %d, grp_num = %d, id = %d", average_time, show_vuser_flow_gp_idx, grp_num, id);

  for(i = 0; i < grp_num; i++)
  {
    if (!(SHOW_RUNTIME_RUNLOGIC_PROGRESS_GRP(vptr->group_num)))
      continue;

    size += (runprof_table_shr_mem[i].sess_ptr->num_of_flow_path * sizeof(VUserFlowAvgTime)); 
    NSDL2_PARSING(NULL, NULL, "num_of_flow_path = %d, size = %d", runprof_table_shr_mem[i].sess_ptr->num_of_flow_path, size);
  }
  //NSDL2_PARSING(NULL, NULL, "num_of_flow_path = %d", runprof_table_shr_mem[i].sess_ptr->num_of_flow_path);

  if(id > runprof_table_shr_mem[i].sess_ptr->num_of_flow_path){
    NSTL2(NULL, NULL, "User Flow Count id = %d runprof_table_shr_mem[i].sess_ptr->num_of_flow_path = %d", id, runprof_table_shr_mem[i].sess_ptr->num_of_flow_path);
    NSDL1_PARSING(NULL, NULL, "User Flow Count id = %d runprof_table_shr_mem[i].sess_ptr->num_of_flow_path = %d", id, runprof_table_shr_mem[i].sess_ptr->num_of_flow_path);
    return;
  }
  
  local_vuser_flow_avg = (VUserFlowAvgTime *)(((char*)((char *)average_time + show_vuser_flow_idx)) + size);
  
  local_vuser_flow_avg[id].cur_vuser_running_flow++;
  NSDL2_PARSING(NULL, NULL, "id = %d, local_vuser_flow_avg = %p, cur_vuser_running_flow = %d",
                             id, local_vuser_flow_avg, local_vuser_flow_avg[id].cur_vuser_running_flow);
}
